// The per-frame profiler (profile.h): where a frame's time goes, in the editor and in play.
//
// Off by default and then free: every measuring point tests one bool. LOCOFIX_PROFILE=1 in the
// environment or the registry DWORD EditorProfile = 1 (HKCU\SOFTWARE\Gromada\Locoland) turns it
// on for the run. Then:
// - World_DrawLayer (0x41fa70) is hooked: the time of each layer (0-1 ground, 2-4 software
//   sprites, 5-10 Direct3D) and how many objects its list holds;
// - the call of Graphics_DrawScene in GameWorld_Run (0x423a0f) is wrapped: the scene's time
//   less the layers and the editor's overlay is SCENE_REST (locks, the depth copy, shadows,
//   weather, effects);
// - Surface_Lock (0x4123c0) is hooked: the locks a frame takes and the time inside them (with
//   dgVoodoo a lock of the back buffer after Direct3D drawing reads the picture back), also by the
//   code that took them (the return address);
// - every Lock and Unlock of the back and the depth buffer through the surface itself
//   (lost_surfaces.cpp's guard calls surface_lock / surface_unlock): also the engine's locks that do not
//   go through Surface_Lock (the minimap's points lock the back buffer themselves) - "bb" the back
//   buffer's: how many, the time in its locks (the wait for the GPU and the copy to the CPU) and in its
//   unlocks (the copy back), by the code that took them; "z" the depth buffer's;
// - Graphics_DrawLight (0x411dd0) is hooked: the lights a frame and their time (part of d3d);
// - the call of Graphics_CopyDepthToZBuffer in Graphics_DrawScene (0x40f34f) is wrapped: the depth buffer's
//   copy into the hardware z-buffer between the scene's two halves ("in zcopy", part of scene-rest);
// - view_culling.cpp counts the objects that pass the view test (their Draw is called) and
//   those that pass only by the engine's "below the view" rule;
// - the editor's overlay (editor_edit.cpp) adds its parts, frame_limit.cpp the present and the
//   wait, and whatever is left of the frame is "other" (the game step, the input, the updates).
// Every ~2 s one line goes to LocoFix.log: fps, then per part the average and the worst frame
// in ms, then the counts per frame. EditorProfile = 2 (LOCOFIX_PROFILE=2) also starts the sampler
// (sampler.cpp) at the first frame.
#include "profile.h"
#include "patch.h"
#include <cstdio>
#include <intrin.h>

namespace prof {

bool g_on = false;

namespace {

constexpr uintptr_t DRAW_LAYER = 0x41fa70;
constexpr uintptr_t DRAW_SCENE = 0x40f0c0;
constexpr uintptr_t RUN_CALL_DRAW_SCENE = 0x423a0f;   // GameWorld_Run: call Graphics_DrawScene
constexpr uintptr_t SURFACE_LOCK = 0x4123c0;
constexpr uintptr_t DRAW_LIGHT = 0x411dd0;
constexpr size_t W_LAYERS = 0x4c, LIST_SIZE = 0x10, LIST_COUNT = 0x04, LIST_ITEMS = 0x0c;
constexpr char REG_KEY[] = "SOFTWARE\\Gromada\\Locoland";
constexpr double REPORT_SECONDS = 2.0;

const char* const NAMES[PARTS] = {"ground", "sprites", "d3d", "scene-rest", "ed-flush", "ed-frame", "ed-heights",
                                  "ed-marks", "ed-regions", "ed-lines", "ed-trains", "ed-arrivals", "ed-tool", "ed-bar", "ed-rows", "ed-status", "ed-panels", "present", "wait"};
const char* const COUNTER_NAMES[COUNTERS] = {"listed2-4", "drawn2-4", "below2-4", "listed", "drawn", "ed-iterated", "locks", "lights"};

int64_t g_frequency = 1;
int64_t g_frame[PARTS] = {};       // this frame
int64_t g_sum[PARTS] = {};         // since the last report
int64_t g_max[PARTS] = {};
int64_t g_frame_count[COUNTERS] = {};
int64_t g_count_sum[COUNTERS] = {};
int64_t g_other_sum = 0, g_other_max = 0, g_total_max = 0;
int64_t g_lock_frame = 0, g_lock_sum = 0;
int64_t g_part_locks[PARTS] = {};   // since the last report   // the time inside Surface_Lock (part of the parts above)
int64_t g_last_frame = 0, g_report_start = 0;
int g_frames = 0;
int g_layer = -1;
int g_level = 0;   // 1 the lines, 2 also the sampler (sampler.cpp)

struct LockCaller {   // the locks by the code that took them (its return address)
    uintptr_t at;
    int64_t count, ticks;
};
LockCaller g_lock_callers[16];
int64_t g_light_frame = 0, g_light_sum = 0;   // the time in Graphics_DrawLight (part of d3d)
int64_t g_zcopy_frame = 0, g_zcopy_sum = 0;   // the time in Graphics_CopyDepthToZBuffer (part of scene-rest)

// Every lock of the back and the depth buffer (surface_lock / surface_unlock), since the last report.
constexpr size_t G_DEPTH_BUFFER = 0xbf8, G_BACK_BUFFER = 0xc04;
uintptr_t g_surface_lock_caller = 0;   // Surface_Lock's caller while it runs: the lock's real caller
struct SurfaceLocks {
    int64_t count, ticks, unlock_ticks;
};
SurfaceLocks g_bb = {}, g_z = {};
LockCaller g_bb_callers[16];

using DrawLayer_t = void(__fastcall*)(void* world, void* edx, int layer);
DrawLayer_t g_draw_layer = nullptr;
using DrawScene_t = void(__fastcall*)(void* g, void* edx, int draw);
using SurfaceLock_t = void*(__stdcall*)(void* surface, int* pitch, unsigned flags);
SurfaceLock_t g_surface_lock = nullptr;

void* __stdcall SurfaceLock_profiled(void* surface, int* pitch, unsigned flags) {
    const int64_t began = now();
    g_surface_lock_caller = reinterpret_cast<uintptr_t>(_ReturnAddress());
    void* bits = g_surface_lock(surface, pitch, flags);
    g_surface_lock_caller = 0;
    const int64_t spent = now() - began;
    g_lock_frame += spent;
    count(LOCKS, 1);
    const auto at = reinterpret_cast<uintptr_t>(_ReturnAddress());
    for (auto& c : g_lock_callers)
        if (c.at == at || !c.at) {
            c.at = at;
            ++c.count;
            c.ticks += spent;
            break;
        }
    return bits;
}

using DrawLight_t = void(__fastcall*)(void* g, void* edx, float x, float y, float z, float w, float h, unsigned color);
DrawLight_t g_draw_light = nullptr;

void __fastcall DrawLight_profiled(void* g, void* edx, float x, float y, float z, float w, float h, unsigned color) {
    const int64_t began = now();
    g_draw_light(g, edx, x, y, z, w, h, color);
    g_light_frame += now() - began;
    count(LIGHTS, 1);
}

constexpr uintptr_t COPY_DEPTH = 0x415350;              // Graphics_CopyDepthToZBuffer (thiscall)
constexpr uintptr_t SCENE_CALL_COPY_DEPTH = 0x40f34f;   // Graphics_DrawScene: call Graphics_CopyDepthToZBuffer

int __fastcall CopyDepth_profiled(void* g, void*) {
    const int64_t began = now();
    const int r = reinterpret_cast<int(__fastcall*)(void*, void*)>(COPY_DEPTH)(g, nullptr);
    g_zcopy_frame += now() - began;
    return r;
}

Part part_of_layer(int layer) { return layer <= 1 ? GROUND : layer <= 4 ? SPRITES : D3D_LAYERS; }

void __fastcall DrawLayer_profiled(void* world, void* edx, int layer) {
    if (layer >= 0 && layer < 16) {
        const uint8_t* list = static_cast<uint8_t*>(world) + W_LAYERS + layer * LIST_SIZE;
        const int n = *reinterpret_cast<const int*>(list + LIST_COUNT);
        void* const* items = *reinterpret_cast<void* const* const*>(list + LIST_ITEMS);
        int listed = 0;
        for (int i = 0; items && i < n; ++i) listed += items[i] != nullptr;
        count(LISTED_ALL, listed);
        if (layer >= 2 && layer <= 4) count(LISTED_2_4, listed);
        if (layer == 0 || layer == 7) count(DRAWN_ALL, listed);   // drawn whole, no view test
    }
    const int64_t began = now(), locked = locks();
    g_layer = layer;
    g_draw_layer(world, edx, layer);
    g_layer = -1;
    add(part_of_layer(layer), now() - began);
    add_locks(part_of_layer(layer), locks() - locked);
}

void __fastcall DrawScene_profiled(void* g, void* edx, int draw) {
    int64_t inside = 0;   // what the parts measured inside the scene had before
    for (int p = GROUND; p <= ED_PANELS; ++p) inside += g_frame[p];
    const int64_t began = now();
    reinterpret_cast<DrawScene_t>(DRAW_SCENE)(g, edx, draw);   // the editor's hook when it is on
    const int64_t spent = now() - began;
    for (int p = GROUND; p <= ED_PANELS; ++p) inside -= g_frame[p];
    add(SCENE_REST, spent + inside);   // inside is minus what they added
}

double ms(int64_t ticks) { return ticks * 1000.0 / g_frequency; }

void report(int64_t t) {
    const double seconds = static_cast<double>(t - g_report_start) / g_frequency;
    char line[4096];   // a line is about 1000 characters
    int at = std::snprintf(line, sizeof line, "profile: %.1f fps, %d frames; ms avg/max:", g_frames / seconds, g_frames);
    const double per = 1.0 / g_frames;
    for (int p = 0; p < PARTS; ++p)
        at += std::snprintf(line + at, sizeof line - at, " %s %.2f/%.1f", NAMES[p], ms(g_sum[p]) * per, ms(g_max[p]));
    at += std::snprintf(line + at, sizeof line - at,  " other %.2f/%.1f, frame max %.1f; in locks %.2f; in lights %.2f; in zcopy %.2f; per frame:", ms(g_other_sum) * per,
                        ms(g_other_max), ms(g_total_max), ms(g_lock_sum) * per, ms(g_light_sum) * per, ms(g_zcopy_sum) * per);
    for (int c = 0; c < COUNTERS; ++c)
        at += std::snprintf(line + at, sizeof line - at, " %s %.0f", COUNTER_NAMES[c], g_count_sum[c] * per);
    at += std::snprintf(line + at, sizeof line - at, "; locks by part:");
    for (int p = 0; p < PARTS; ++p)
        if (g_part_locks[p]) at += std::snprintf(line + at, sizeof line - at, " %s %.1f", NAMES[p], g_part_locks[p] * per);
    at += std::snprintf(line + at, sizeof line - at, "; locks by caller:");
    for (auto& c : g_lock_callers)
        if (c.count) {
            at += std::snprintf(line + at, sizeof line - at, " %X %.1f/%.2fms", static_cast<unsigned>(c.at), c.count * per, ms(c.ticks) * per);
            c.count = c.ticks = 0;
        }
    at += std::snprintf(line + at, sizeof line - at, "; bb %.1f/%.2fms unlock %.2fms; z %.1f/%.2fms unlock %.2fms; bb by caller:",
                        g_bb.count * per, ms(g_bb.ticks) * per, ms(g_bb.unlock_ticks) * per, g_z.count * per,
                        ms(g_z.ticks) * per, ms(g_z.unlock_ticks) * per);
    for (auto& c : g_bb_callers)
        if (c.count) {
            at += std::snprintf(line + at, sizeof line - at, " %X %.1f/%.2fms", static_cast<unsigned>(c.at), c.count * per, ms(c.ticks) * per);
            c.at = 0;
            c.count = c.ticks = 0;
        }
    g_bb = g_z = {};
    patch::log("%s", line);
    for (int p = 0; p < PARTS; ++p) g_part_locks[p] = 0;
    for (int p = 0; p < PARTS; ++p) g_sum[p] = g_max[p] = 0;
    for (int c = 0; c < COUNTERS; ++c) g_count_sum[c] = 0;
    g_other_sum = g_other_max = g_total_max = 0;
    g_lock_sum = 0;
    g_light_sum = 0;
    g_zcopy_sum = 0;
    g_frames = 0;
    g_report_start = t;
}

int switched_on() {
    char value[8] = {};
    if (GetEnvironmentVariableA("LOCOFIX_PROFILE", value, sizeof value) && (value[0] == '1' || value[0] == '2')) return value[0] - '0';
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return 0;
    DWORD v = 0, size = sizeof v, type = 0;
    const LONG r = RegQueryValueExA(key, "EditorProfile", nullptr, &type, reinterpret_cast<BYTE*>(&v), &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS && type == REG_DWORD && (v == 1 || v == 2) ? static_cast<int>(v) : 0;
}

}  // namespace

int64_t now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

void add(Part part, int64_t ticks) { g_frame[part] += ticks; }

void surface_lock(void* surface, uintptr_t caller, int64_t ticks) {
    const auto* g = *reinterpret_cast<uint8_t* const*>(0x4a9a14);   // g_graphics
    if (!g_on || !g) return;
    const bool back = surface == *reinterpret_cast<void* const*>(g + G_BACK_BUFFER);
    if (!back && surface != *reinterpret_cast<void* const*>(g + G_DEPTH_BUFFER)) return;
    SurfaceLocks& l = back ? g_bb : g_z;
    ++l.count;
    l.ticks += ticks;
    if (!back) return;
    if (g_surface_lock_caller) caller = g_surface_lock_caller;
    for (auto& c : g_bb_callers)
        if (c.at == caller || !c.at) {
            c.at = caller;
            ++c.count;
            c.ticks += ticks;
            break;
        }
}

void surface_unlock(void* surface, int64_t ticks) {
    const auto* g = *reinterpret_cast<uint8_t* const*>(0x4a9a14);
    if (!g_on || !g) return;
    if (surface == *reinterpret_cast<void* const*>(g + G_BACK_BUFFER)) g_bb.unlock_ticks += ticks;
    else if (surface == *reinterpret_cast<void* const*>(g + G_DEPTH_BUFFER)) g_z.unlock_ticks += ticks;
}
int64_t locks() { return g_frame_count[LOCKS]; }
void add_locks(Part part, int64_t n) { g_part_locks[part] += n; }
void count(Counter counter, int n) {
    if (g_on) g_frame_count[counter] += n;
}
int current_layer() { return g_layer; }

void frame_end() {
    if (!g_on) return;
    if (g_level == 2) start_sampler();
    const int64_t t = now();
    if (g_last_frame && t - g_last_frame < g_frequency / 2) {   // a pause (a load, a dialog) is left out
        const int64_t total = t - g_last_frame;
        int64_t measured = 0;
        for (int p = 0; p < PARTS; ++p) {
            measured += g_frame[p];
            g_sum[p] += g_frame[p];
            if (g_frame[p] > g_max[p]) g_max[p] = g_frame[p];
        }
        const int64_t other = total - measured;
        g_other_sum += other;
        if (other > g_other_max) g_other_max = other;
        if (total > g_total_max) g_total_max = total;
        for (int c = 0; c < COUNTERS; ++c) g_count_sum[c] += g_frame_count[c];
        g_lock_sum += g_lock_frame;
        g_light_sum += g_light_frame;
        g_zcopy_sum += g_zcopy_frame;
        ++g_frames;
    }
    for (int p = 0; p < PARTS; ++p) g_frame[p] = 0;
    for (int c = 0; c < COUNTERS; ++c) g_frame_count[c] = 0;
    g_lock_frame = 0;
    g_light_frame = 0;
    g_zcopy_frame = 0;
    g_last_frame = t;
    if (!g_report_start) g_report_start = t;
    if (g_frames && t - g_report_start >= static_cast<int64_t>(REPORT_SECONDS * g_frequency)) report(t);
    else if (!g_frames) g_report_start = t;
}

}  // namespace prof

void install_profile() {
    using namespace prof;
    g_level = switched_on();
    if (!g_level) return;
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_frequency = f.QuadPart;
    // sub esp,8 / push ebx / push ebp / push esi / push edi
    const uint8_t draw_layer[] = {0x83, 0xEC, 0x08, 0x53, 0x55, 0x56, 0x57};
    const uint8_t call_scene[] = {0xE8, 0xAC, 0xB6, 0xFE, 0xFF};   // call Graphics_DrawScene
    if (!patch::check(RUN_CALL_DRAW_SCENE, call_scene, sizeof call_scene)) {
        patch::log("skipped the profiler: unexpected bytes");
        return;
    }
    g_draw_layer = reinterpret_cast<DrawLayer_t>(
        patch::hook(DRAW_LAYER, draw_layer, sizeof draw_layer, reinterpret_cast<void*>(&DrawLayer_profiled), "World_DrawLayer"));
    if (!g_draw_layer) return;
    patch::write_call(RUN_CALL_DRAW_SCENE, nullptr, 0, reinterpret_cast<void*>(&DrawScene_profiled), 5);
    // sub esp,0x7c / mov eax,[esp+0x80]
    const uint8_t surface_lock[] = {0x83, 0xEC, 0x7C, 0x8B, 0x84, 0x24, 0x80, 0x00, 0x00, 0x00};
    g_surface_lock = reinterpret_cast<SurfaceLock_t>(
        patch::hook(SURFACE_LOCK, surface_lock, sizeof surface_lock, reinterpret_cast<void*>(&SurfaceLock_profiled), "Surface_Lock"));
    // mov eax,[esp+0x18] / sub esp,0xdc
    const uint8_t draw_light[] = {0x8B, 0x44, 0x24, 0x18, 0x81, 0xEC, 0xDC, 0x00, 0x00, 0x00};
    g_draw_light = reinterpret_cast<DrawLight_t>(
        patch::hook(DRAW_LIGHT, draw_light, sizeof draw_light, reinterpret_cast<void*>(&DrawLight_profiled), "Graphics_DrawLight"));
    const uint8_t call_copy[] = {0xE8, 0xFC, 0x5F, 0x00, 0x00};   // call Graphics_CopyDepthToZBuffer
    if (patch::check(SCENE_CALL_COPY_DEPTH, call_copy, sizeof call_copy))
        patch::write_call(SCENE_CALL_COPY_DEPTH, nullptr, 0, reinterpret_cast<void*>(&CopyDepth_profiled), 5);
    g_on = true;
    patch::log("profiler: on (a line every %.0f s)", REPORT_SECONDS);
}
