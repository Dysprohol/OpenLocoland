// Zoom out with the mouse wheel in play: the map drawn smaller, so more of it is on the screen, while the
// interface keeps its size.
//
// The file has two parts, so that another way of making the view can replace the second without touching
// the first:
//   1. THE ZOOM (state and input): the zoom in effect and the zooms a level has (make_steps: the steps, the
//      caps), the wheel (zoom_after_frame), the point under the cursor kept (set_step), the resets (a level
//      change: zoom_level_loading) and the log (each step's view size, switch time, memory and the average
//      frame time over its first seconds).
//   2. THE VIEW (view_apply): turns a zoom into what the game draws - here the composition below.
//
// The view now (the composition): the game is drawn on a logical screen bigger than the window - w / z x
// h / z for a zoom z below 1 - and screen_scaling.cpp stretches the picture down to the window, as it
// stretches a small map's screen up; the mouse is mapped back the same way, so the engine, its clicks and
// its hit tests work in logical pixels as ever. The interface is built in that bigger screen s = S / z times
// bigger (S the interface's size without the zoom, interface_scale.cpp; s from the bigger pictures the build
// makes, twice and three times): after the stretch it comes out at its own size, to the pixel. So the zooms
// are only those an s exists for:
//   - interface normal (S = 1): 1/2 (s = 2) and 1/3 (s = 3);
//   - twice (S = 2): 2/3 (s = 3);
//   - three times: none.
// The view is capped by the map on each axis (min(w / z, map), as screen_scaling.cpp caps a level's screen; a
// capped view of another shape than the window gets bars at the sides, as mission 1 has) - but only on one
// axis: a view the map caps on both would be stretched by less than the zoom, and the bar would come out at
// an in-between size, so such a step is not offered (finer steps will come from drawing the interface apart
// from the world) - and by MAX_PIXELS,
// the whole view of the biggest original map (3072x2304) - the frame's cost follows the logical pixels;
// the virtual adapter's numbers and the host's estimate are in docs/dev/working.md, and the test key's
// DWORD ZoomMaxPixels moves the cap for a measurement. A zoom whose view is no bigger than the step before
// it is left out: a map that already fits the window (mission 1), or a view that already holds the whole
// map, has no more steps - the wheel then does nothing.
//
// The wheel (Mouse+0x04 = World+0x234, delta / 120: the engine stores it and nothing in a mission reads it;
// script function 201 GetMouseWheel reads it in the main menu's custom maps page, menu_input.cpp):
// towards the player out, away back in, one step a notch. Not while the cursor is over the interface (the
// menu's hover - the bar, the minimap, the money plate, a message's strip), a menu is up, the game is
// paused or loading, the scripts hold the camera (no border or arrow scrolling: an intro), or in the editor
// (its wheel is its tools'). A menu (the Esc menu, the end screens) is laid out in the middle of the screen
// of the moment and stretched to the window by its 1024x768 middle (screen_scaling.cpp), so it is the same
// at any zoom: the zoom stays while it is up and the bar comes back at the zoom's scale after it. A level
// change gives the level its own screen again (zoom 1).
//
// A change is made right after a frame is presented (frame_limit.cpp's wrapper of Graphics_EndScene, never
// inside BeginScene/EndScene). The view: the new screen (surfaces, screen size, view, clips:
// screen_set_logical), the fog texture big enough for it (fog_buffer_cover), the state bar built again at
// the new s (status_bar_rebuild, the Esc menu's way back: the factory's list open again if it was), the
// message lines made again in the new font (interface_messages_rescale). Then the camera so that the map
// point under the cursor stays under it (SetPosition, which also moves every menu object by the camera's
// change; at the map's edges the camera's limits win).
//
// THE PROBE (stage Z0, test mode only: LOCOFIX_TEST=1 and the DWORD ZoomProbe = 1 in the test key): the
// wheel steps through the zooms 1, 2/3, 1/2 and 1/3, capped by the map only, and the interface is not built
// again - it shrinks with the world. For measuring the frame rate and memory of big views.
#include "patch.h"
#include <algorithm>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

bool screen_set_logical(int w, int h);       // screen_scaling.cpp
void screen_base_size(int& w, int& h);
void window_size(int& w, int& h);
void window_to_logical(POINT& p);
void logical_to_window(POINT& p);
bool menu_background_shown();
void fog_buffer_cover(int w, int h);         // fog_buffer.cpp
void frame_rate_section(const char* part);   // frame_limit.cpp
bool status_bar_rebuild(uint8_t* player);    // status_bar.cpp
void interface_messages_rescale();           // interface_texts.cpp
int interface_scale_for_window();            // interface_scale.cpp
bool interface_scale_available(int s);

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t SET_POSITION = 0x41c680;
constexpr char REG_KEY[] = "SOFTWARE\\Gromada\\Locoland";
constexpr int WORLD_FLAGS = 0x04, WORLD_SCROLL_TYPE = 0x30, WORLD_CAMERA_X = 0x44, WORLD_CAMERA_Y = 0x48;
constexpr int WORLD_MAP_W = 0x28, WORLD_MAP_H = 0x2c, WORLD_HWND = 0x214, WORLD_PLAYERS = 0x220;
constexpr int MOUSE_WHEEL = 0x234, MOUSE_SCREEN_X = 0x240, MOUSE_SCREEN_Y = 0x244;
constexpr int MENU_HOVER = 0x24c + 0x14;
constexpr unsigned WORLD_EDITOR = 0x1, WORLD_ACTIVE = 0x8, WORLD_PAUSED = 0x10, WORLD_LOADING = 0x20;
constexpr unsigned SCROLL_BY_PLAYER = 0x1 | 0x20;   // SCROLL_FROM_BORDER, SCROLL_WITH_ARROWS
constexpr int MAX_SCALE = 3;
// The most logical pixels a zoom's view may have: the whole of the biggest original map (see the top).
constexpr int MAX_PIXELS = 3072 * 2304;
constexpr double MEASURE_SECONDS = 3.0;   // the frame time logged for a step: over its first seconds

template <typename T>
T& at(void* base, int offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}
uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }
uint8_t* graphics() { return *reinterpret_cast<uint8_t**>(G_GRAPHICS); }

bool test_mode() {
    char value[8] = {};
    return GetEnvironmentVariableA("LOCOFIX_TEST", value, sizeof value) && value[0] == '1';
}

// A DWORD of the game's key, read only in test mode (the test key then); `fallback` otherwise.
int test_value(const char* name, int fallback) {
    if (!test_mode()) return fallback;
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return fallback;
    DWORD v = 0, size = sizeof v, type = 0;
    const LONG r = RegQueryValueExA(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&v), &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS && type == REG_DWORD ? static_cast<int>(v) : fallback;
}

// ======== 2. THE VIEW ========

// The game draws a w x h view of the map, shown in the window; `probe`: the interface is
// left as it is. False when it cannot (nothing changed).
bool view_apply(int w, int h, bool probe) {
    if (!screen_set_logical(w, h)) return false;
    fog_buffer_cover(w, h);
    if (!probe) {
        status_bar_rebuild(at<uint8_t*>(world(), WORLD_PLAYERS));   // the human's bar (World.players[0])
        interface_messages_rescale();
    }
    return true;
}

// ======== 1. THE ZOOM ========

// A zoom, as a fraction: the view is the window times den / num.
struct Step {
    int num, den;
};
constexpr int MAX_STEPS = 4;
constexpr Step PROBE_STEPS[MAX_STEPS] = {{1, 1}, {2, 3}, {1, 2}, {1, 3}};

using SetPosition_t = void(__fastcall*)(void* world, void* edx, float x, float y, int mode);

int g_probe = -1;   // -1 not read yet
Step g_steps[MAX_STEPS] = {{1, 1}};   // the zooms of the level in effect (made by make_steps at zoom 1)
int g_step_count = 1;
int g_step = 0;     // index in g_steps of the zoom in effect

// The frame time over a step's first seconds
int64_t g_measure_from = 0;   // QueryPerformanceCounter at the step's second frame, 0 = not measuring
int64_t g_measure_last = 0;
int g_measure_frames = -1;    // -1: the step's first frame (its switch) is left out
char g_measure_what[64] = "";

bool probe() {
    if (g_probe < 0) {
        g_probe = test_value("ZoomProbe", 0) ? 1 : 0;
        if (g_probe) patch::log("zoom: the probe is on (test mode, ZoomProbe)");
    }
    return g_probe != 0;
}

int max_pixels() {
    static const int value = test_value("ZoomMaxPixels", MAX_PIXELS);
    return value;
}

// The view of a zoom: the window times den / num, at most the map on each axis, at least the level's own.
void size_of(const Step& z, int& w, int& h) {
    int window_w = 0, window_h = 0, base_w = 0, base_h = 0;
    window_size(window_w, window_h);
    screen_base_size(base_w, base_h);
    const int map_w = static_cast<int>(at<float>(world(), WORLD_MAP_W));
    const int map_h = static_cast<int>(at<float>(world(), WORLD_MAP_H));
    w = MulDiv(window_w, z.den, z.num);
    h = MulDiv(window_h, z.den, z.num);
    if (w > map_w) w = map_w;
    if (h > map_h) h = map_h;
    if (w < base_w) w = base_w;
    if (h < base_h) h = base_h;
}

// The zooms of this level and window (at zoom 1, when the wheel is turned): see the top.
void make_steps() {
    Step wanted[MAX_STEPS] = {{1, 1}};
    int n = 1;
    if (probe()) {
        for (int i = 1; i < MAX_STEPS; ++i) wanted[n++] = PROBE_STEPS[i];
    } else {
        const int ui = interface_scale_for_window();   // S: the interface without the zoom
        for (int s = ui + 1; s <= MAX_SCALE && n < MAX_STEPS; ++s)
            if (interface_scale_available(s)) wanted[n++] = {ui, s};
    }
    g_step_count = 1;
    g_steps[0] = {1, 1};
    int last_w, last_h;
    size_of(g_steps[0], last_w, last_h);
    for (int i = 1; i < n; ++i) {
        int w, h;
        size_of(wanted[i], w, h);
        if (w <= last_w && h <= last_h) continue;   // nothing more on the screen than the step before
        if (!probe()) {
            if (static_cast<double>(w) * h > max_pixels()) break;
            // the map caps the view on both axes: the stretch would not be the zoom's, the bar not its size
            int window_w = 0, window_h = 0;
            window_size(window_w, window_h);
            const double stretch = (std::min)(static_cast<double>(window_w) / w, static_cast<double>(window_h) / h);
            if (stretch * wanted[i].den > wanted[i].num * 1.001) break;
        }
        g_steps[g_step_count++] = wanted[i];
        last_w = w;
        last_h = h;
    }
}

void log_memory(double ms, int w, int h) {
    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    pmc.cb = sizeof pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof pmc);
    MEMORYSTATUSEX status = {};
    status.dwLength = sizeof status;
    GlobalMemoryStatusEx(&status);
    const double mb = 1024.0 * 1024.0;
    patch::log("zoom: view %dx%d in %.1f ms; memory: working set %.0f MB, private %.0f MB, address space used %.0f "
               "of %.0f MB",
               w, h, ms, pmc.WorkingSetSize / mb, pmc.PrivateUsage / mb,
               (status.ullTotalVirtual - status.ullAvailVirtual) / mb, status.ullTotalVirtual / mb);
}

// Why the wheel does not zoom now, or nullptr.
const char* refused(uint8_t* w) {
    const unsigned flags = at<unsigned>(w, WORLD_FLAGS);
    if (!(flags & WORLD_ACTIVE)) return "the window is not active";
    if (flags & WORLD_EDITOR) return "editor mode";
    if (flags & (WORLD_LOADING | WORLD_PAUSED)) return "loading or paused";
    if (!(at<unsigned>(w, WORLD_SCROLL_TYPE) & SCROLL_BY_PLAYER)) return "the scripts hold the camera";
    if (menu_background_shown()) return "a menu is up";
    if (uint8_t* hover = at<uint8_t*>(w, MENU_HOVER)) {
        static char text[48];
        wsprintfA(text, "over the interface (VID %d)", at<uint8_t*>(hover, 0x1c) ? at<int>(at<uint8_t*>(hover, 0x1c), 4) : -1);
        return text;
    }
    return nullptr;
}

void set_step(int step) {
    uint8_t* w = world();
    uint8_t* g = graphics();
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);

    int new_w, new_h;
    size_of(g_steps[step], new_w, new_h);
    const int old_w = static_cast<int>(at<float>(g, 0x20c)), old_h = static_cast<int>(at<float>(g, 0x210));
    // the cursor: its place in the window, and the map point under it
    POINT cursor = {static_cast<LONG>(at<float>(w, MOUSE_SCREEN_X)), static_cast<LONG>(at<float>(w, MOUSE_SCREEN_Y))};
    const float map_x = at<float>(w, WORLD_CAMERA_X) + cursor.x, map_y = at<float>(w, WORLD_CAMERA_Y) + cursor.y;
    logical_to_window(cursor);

    if (!view_apply(new_w, new_h, probe())) return;
    g_step = step;

    POINT now = cursor;
    window_to_logical(now);
    reinterpret_cast<SetPosition_t>(SET_POSITION)(w, nullptr, map_x - now.x + new_w * 0.5f, map_y - now.y + new_h * 0.5f, 0);
    // the engine's mouse at the new mapping at once (it takes its place from WM_NCHITTEST)
    if (HWND window = at<HWND>(w, WORLD_HWND)) {
        POINT screen = cursor;
        ClientToScreen(window, &screen);
        SendMessageA(window, WM_NCHITTEST, 0, MAKELPARAM(screen.x, screen.y));
    }

    QueryPerformanceCounter(&t1);
    wsprintfA(g_measure_what, "zoom %d/%d %dx%d", g_steps[step].num, g_steps[step].den, new_w, new_h);
    frame_rate_section(step ? g_measure_what : "");
    patch::log("zoom: %dx%d -> %d/%d at window point %d,%d (map point %d,%d)", old_w, old_h, g_steps[step].num,
               g_steps[step].den, static_cast<int>(cursor.x), static_cast<int>(cursor.y), static_cast<int>(map_x),
               static_cast<int>(map_y));
    log_memory((t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart, new_w, new_h);
    g_measure_frames = -1;
    g_measure_from = 1;
}

// Every frame: the average frame time of a step's first MEASURE_SECONDS (its first frame left out).
void measure_frame() {
    if (!g_measure_from) return;
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    if (g_measure_frames < 0) {
        g_measure_from = g_measure_last = t.QuadPart;
        g_measure_frames = 0;
        return;
    }
    ++g_measure_frames;
    const double seconds = static_cast<double>(t.QuadPart - g_measure_from) / f.QuadPart;
    if (t.QuadPart - g_measure_last > f.QuadPart / 2) {   // a pause (the window left, a load): start afresh
        g_measure_from = t.QuadPart;
        g_measure_frames = 0;
    } else if (seconds >= MEASURE_SECONDS) {
        patch::log("zoom: %s: average frame %.1f ms (%.1f fps) over the first %.1f s", g_measure_what,
                   seconds * 1000.0 / g_measure_frames, g_measure_frames / seconds, seconds);
        g_measure_from = 0;
    }
    g_measure_last = t.QuadPart;
}

}  // namespace

// screen_scaling.cpp: a level is about to load; it gets its own screen again (the zoom is 1).
void zoom_level_loading() {
    g_step = 0;
    g_measure_from = 0;
}

// frame_limit.cpp: after every frame is presented.
void zoom_after_frame() {
    uint8_t* w = world();
    if (!w || !graphics()) return;
    measure_frame();
    const int notches = at<int>(w, MOUSE_WHEEL);
    if (!notches) return;
    if (const char* why = refused(w)) {
        static const bool test = test_mode();   // the tests see why a turn did nothing; a player's log stays quiet
        if (test) patch::log("zoom: %d notch(es) not taken - %s", notches, why);
        return;
    }
    at<int>(w, MOUSE_WHEEL) = 0;   // taken: nothing after the frame reads it again
    if (g_step == 0) make_steps();
    int step = g_step - notches;   // one step a notch; a turn towards the player (negative) goes out
    if (step < 0) step = 0;
    if (step >= g_step_count) step = g_step_count - 1;
    if (step != g_step) set_step(step);
    else if (test_mode()) patch::log("zoom: %d notch(es) at step %d of %d - no other step", notches, g_step, g_step_count);
}
