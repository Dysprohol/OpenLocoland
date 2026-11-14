// Zoom: the map drawn smaller, so more of it is on the screen.
//
// The game is drawn on a logical screen bigger than the window - the view at w / z x h / z for a zoom z
// below 1 - and screen_scaling.cpp stretches the picture down to the window, as it stretches a small
// map's screen up; the mouse is mapped back the same way. Changing it while a mission runs: right after a
// frame is presented (frame_limit.cpp's wrapper of Graphics_EndScene, never inside BeginScene/EndScene)
// the game gets the new screen (surfaces, screen size, view, clips: screen_set_logical), the fog texture
// big enough for it (fog_buffer_cover), and the camera so that the map point under the cursor stays under
// it (SetPosition, which also moves every menu object by the camera's change).
//
// THE PROBE (stage Z0, test mode only: LOCOFIX_TEST=1 and the DWORD ZoomProbe = 1 in the test key): the
// wheel steps through the zooms 1, 2/3, 1/2 and 1/3 - towards the user out, away back in -, the logical
// screen capped by the map on each axis; the interface is drawn at the bigger screen and shrinks with the
// world. Each change logs the time it took and the process' memory, and the frame rate of each step is
// logged on its own (frame_rate_section).
#include "patch.h"
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

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t SET_POSITION = 0x41c680;
constexpr char REG_KEY[] = "SOFTWARE\\Gromada\\Locoland";
constexpr int WORLD_FLAGS = 0x04, WORLD_SCROLL_TYPE = 0x30, WORLD_CAMERA_X = 0x44, WORLD_CAMERA_Y = 0x48;
constexpr int WORLD_MAP_W = 0x28, WORLD_MAP_H = 0x2c, WORLD_HWND = 0x214;
constexpr int MOUSE_WHEEL = 0x234, MOUSE_SCREEN_X = 0x240, MOUSE_SCREEN_Y = 0x244;
constexpr int MENU_HOVER = 0x24c + 0x14;
constexpr unsigned WORLD_EDITOR = 0x1, WORLD_ACTIVE = 0x8, WORLD_PAUSED = 0x10, WORLD_LOADING = 0x20;
constexpr unsigned SCROLL_BY_PLAYER = 0x1 | 0x20;   // SCROLL_FROM_BORDER, SCROLL_WITH_ARROWS

// The zooms, as fractions: the logical screen is the window times den / num.
struct Step {
    int num, den;
};
constexpr Step STEPS[] = {{1, 1}, {2, 3}, {1, 2}, {1, 3}};
constexpr int STEP_COUNT = sizeof STEPS / sizeof STEPS[0];

using SetPosition_t = void(__fastcall*)(void* world, void* edx, float x, float y, int mode);

template <typename T>
T& at(void* base, int offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}
uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }
uint8_t* graphics() { return *reinterpret_cast<uint8_t**>(G_GRAPHICS); }

int g_probe = -1;   // -1 not read yet
int g_step = 0;     // index in STEPS of the zoom in effect

bool probe() {
    if (g_probe >= 0) return g_probe != 0;
    g_probe = 0;
    char value[8] = {};
    if (!GetEnvironmentVariableA("LOCOFIX_TEST", value, sizeof value) || value[0] != '1') return false;
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    DWORD v = 0, size = sizeof v, type = 0;
    if (RegQueryValueExA(key, "ZoomProbe", nullptr, &type, reinterpret_cast<BYTE*>(&v), &size) == ERROR_SUCCESS &&
        type == REG_DWORD)
        g_probe = v ? 1 : 0;
    RegCloseKey(key);
    if (g_probe) patch::log("zoom: the probe is on (test mode, ZoomProbe)");
    return g_probe != 0;
}

// The logical screen of a step: the window times den / num, at most the map on each axis, at least the
// level's own screen.
void step_size(int step, int& w, int& h) {
    int window_w = 0, window_h = 0, base_w = 0, base_h = 0;
    window_size(window_w, window_h);
    screen_base_size(base_w, base_h);
    if (step == 0) {
        w = base_w;
        h = base_h;
        return;
    }
    const int map_w = static_cast<int>(at<float>(world(), WORLD_MAP_W));
    const int map_h = static_cast<int>(at<float>(world(), WORLD_MAP_H));
    w = MulDiv(window_w, STEPS[step].den, STEPS[step].num);
    h = MulDiv(window_h, STEPS[step].den, STEPS[step].num);
    if (w > map_w) w = map_w;
    if (h > map_h) h = map_h;
    if (w < base_w) w = base_w;
    if (h < base_h) h = base_h;
}

// The next step from `from` in direction `dir` (+1 out, -1 in) that changes the screen; `from` if none.
int next_step(int from, int dir) {
    int w0, h0;
    step_size(from, w0, h0);
    for (int i = from + dir; i >= 0 && i < STEP_COUNT; i += dir) {
        int w, h;
        step_size(i, w, h);
        if (w != w0 || h != h0) return i;
    }
    return from;
}

void log_memory(const char* what, double ms, int w, int h) {
    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    pmc.cb = sizeof pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof pmc);
    MEMORYSTATUSEX ms_ = {};
    ms_.dwLength = sizeof ms_;
    GlobalMemoryStatusEx(&ms_);
    const double mb = 1024.0 * 1024.0;
    patch::log("zoom: %s %dx%d in %.1f ms; memory: working set %.0f MB, private %.0f MB, address space used %.0f of %.0f MB",
               what, w, h, ms, pmc.WorkingSetSize / mb, pmc.PrivateUsage / mb,
               (ms_.ullTotalVirtual - ms_.ullAvailVirtual) / mb, ms_.ullTotalVirtual / mb);
}

bool zoom_allowed(uint8_t* w) {
    const unsigned flags = at<unsigned>(w, WORLD_FLAGS);
    return (flags & WORLD_ACTIVE) && !(flags & (WORLD_LOADING | WORLD_PAUSED | WORLD_EDITOR)) &&
           (at<unsigned>(w, WORLD_SCROLL_TYPE) & SCROLL_BY_PLAYER) && !menu_background_shown() &&
           !at<void*>(w, MENU_HOVER);
}

void set_step(int step) {
    uint8_t* w = world();
    uint8_t* g = graphics();
    LARGE_INTEGER f, t0, t1;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t0);

    int new_w, new_h;
    step_size(step, new_w, new_h);
    const float old_w = at<float>(g, 0x20c), old_h = at<float>(g, 0x210);
    // the cursor: its place in the window, and the map point under it
    POINT cursor = {static_cast<LONG>(at<float>(w, MOUSE_SCREEN_X)), static_cast<LONG>(at<float>(w, MOUSE_SCREEN_Y))};
    const float map_x = at<float>(w, WORLD_CAMERA_X) + cursor.x, map_y = at<float>(w, WORLD_CAMERA_Y) + cursor.y;
    logical_to_window(cursor);

    if (!screen_set_logical(new_w, new_h)) return;
    fog_buffer_cover(new_w, new_h);
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
    char what[64];
    wsprintfA(what, "%d/%d", STEPS[step].num, STEPS[step].den);
    char part[96];
    wsprintfA(part, "zoom %s %dx%d", what, new_w, new_h);
    frame_rate_section(step ? part : "");
    patch::log("zoom: %.0fx%.0f -> step %s", old_w, old_h, what);
    log_memory("screen", (t1.QuadPart - t0.QuadPart) * 1000.0 / f.QuadPart, new_w, new_h);
}

}  // namespace

// screen_scaling.cpp: a level is about to load; it gets its own screen again (the zoom is 1).
void zoom_level_loading() { g_step = 0; }

// frame_limit.cpp: after every frame is presented.
void zoom_after_frame() {
    uint8_t* w = world();
    if (!w || !graphics()) return;
    const int notches = at<int>(w, MOUSE_WHEEL);
    if (!notches || !probe() || !zoom_allowed(w)) return;
    int step = g_step;
    for (int n = notches < 0 ? -notches : notches; n > 0; --n) step = next_step(step, notches < 0 ? 1 : -1);
    at<int>(w, MOUSE_WHEEL) = 0;   // taken: the next frames do not see it again
    if (step != g_step) set_step(step);
}
