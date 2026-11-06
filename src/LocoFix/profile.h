// A cheap per-frame profiler (profile.cpp): off unless LOCOFIX_PROFILE=1 is in the environment
// or the registry DWORD EditorProfile (next to FrameLimit) is 1. Each part of a frame adds its
// QueryPerformanceCounter ticks; every ~2 s LocoFix.log gets one line with the average and the
// worst frame of every part, the fps and the counts of objects drawn and iterated.
#pragma once
#include <cstdint>

namespace prof {

enum Part {
    GROUND,       // the engine's layers 0-1 (Direct3D ground tiles)
    SPRITES,      // layers 2-4 (software sprites into the locked back buffer)
    D3D_LAYERS,   // layers 5-10 (Direct3D, and layer 10 in software)
    SCENE_REST,   // the rest of Graphics_DrawScene: locks, depth copy, shadows, weather, effects
    ED_FLUSH,     // the editor's relief and ground flush before the scene
    ED_FRAME,     // the map's frame against the void
    ED_HEIGHTS,   // the heights view (contours)
    ED_MARKS,     // selection circles, a train's patrol marks, the mines
    ED_REGIONS,   // the regions' frames
    ED_LINES,     // the push lines
    ED_TRAINS,    // the enemy trains' orders
    ED_ARRIVALS,  // the arrivals' markers
    ED_TOOL,      // the tool under the cursor: pick / hover, the place preview, the brushes, the cursor
    ED_BAR,       // the bar
    ED_ROWS,      // the rows above the bar
    ED_STATUS,    // the status lines
    ED_PANELS,    // the music player, sockets, context panel, row tip, hint, the Help / Menu panels
    PRESENT,      // Graphics_EndScene (the Blt / Flip)
    WAIT,         // the frame limiter's wait
    PARTS
};

enum Counter {
    LISTED_2_4,     // objects in the lists of layers 2-4
    DRAWN_2_4,      // of them passed the view test (the sprite's Draw called)
    BELOW_2_4,      // of those, passed only by the "below the view" rule
    LISTED_ALL,     // objects in every layer list
    DRAWN_ALL,      // Draw called, every layer
    ED_ITERATED,    // objects the editor's for_each_object walked this frame
    LOCKS,          // Surface_Lock calls (the back and the depth buffer for software drawing)
    COUNTERS
};

extern bool g_on;
inline bool on() { return g_on; }
int64_t now();
void add(Part part, int64_t ticks);
int64_t locks();                      // the locks of this frame so far
void add_locks(Part part, int64_t n);  // ... of them taken in a part
void count(Counter counter, int n);
int current_layer();   // the layer World_DrawLayer draws now, -1 outside it

// Ticks from the last mark to now go to a part.
struct Lap {
    int64_t t, l = 0;
    Lap() : t(g_on ? now() : 0) {
        if (t) l = locks();
    }
    void to(Part part) {
        if (!t) return;
        const int64_t n = now(), k = locks();
        add(part, n - t);
        add_locks(part, k - l);
        t = n;
        l = k;
    }
};

void frame_end();   // frame_limit.cpp, after the present and the wait

}  // namespace prof
