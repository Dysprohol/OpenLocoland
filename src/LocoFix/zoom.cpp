// Zoom with the mouse wheel in play: the map drawn bigger or smaller, so less or more of it is on the screen,
// while the interface keeps its size to the pixel.
//
// The file has two parts, so that the way the view is made (zoom_view.cpp) stays apart from the zoom itself:
//   1. THE ZOOM (state and input): the zooms a level has (make_steps: the steps, the caps), the wheel
//      (zoom_after_frame, refused), the point under the cursor kept (go_to), an eased glide between steps, the
//      resets (a level change: zoom_level_loading), edge and arrow scrolling at the same speed on the screen at
//      every zoom (scroll_move), and the log (each step's view, switch time, memory and frame time).
//   2. THE VIEW (zoom_view.cpp): zoom_view_size says the view (map rectangle) a zoom shows, zoom_view_apply
//      makes the game draw it from the next frame on - the world at the zoom, the interface after it at the
//      window's size (two passes) - and zoom_view_between_frames gives a menu its screen while it is up.
//
// A zoom z is window pixels per map pixel; the level's own zoom is 1 (or a small map's own stretch: mission 1 at
// 1920x1080 is 1.40625). The steps are a ladder of 12 % a notch with the level's own zoom on it: inward own x
// 1.12^k up to 3 (the last step 3 itself), outward own / 1.12^k while the view is within the map on at least one
// axis, then the whole map (the zoom at which its longer side, against the window, fills the window) when that
// shows at least 3 % more than the last of them (a step within half a notch of the whole map is left out, the
// whole map taking its place). A view the map
// caps on one axis keeps the zoom (the picture is narrower or lower than the window, black round it); views
// bigger than MAX_PIXELS map pixels are not offered (the frame's cost follows the logical pixels: the whole view
// of the biggest original map, 3072x2304, is the most; the test key's DWORD ZoomMaxPixels moves it in test mode).
//
// The wheel (Mouse+0x04 = World+0x234, delta / 120: the engine stores it and nothing in a mission reads it;
// script function 201 GetMouseWheel reads it in the main menu's custom maps page, menu_input.cpp): towards the
// player out, away in, one step a notch. Not while the cursor is over the interface (the menu's hover - the bar,
// the minimap, the money plate, a message's strip), a menu is up, the game is paused or loading, the scripts hold
// the camera (no border or arrow scrolling: an intro). The zoom stays while a menu is up (zoom_view.cpp gives the
// menu its screen). A level change gives the level its own zoom.
//
// The map editor (a level held, editor.cpp): its plain wheel is its tools', so its zoom is Ctrl+wheel, passed in
// by editor_edit.cpp (zoom_editor_wheel) - the same steps, caps and anchor; not while one of its panels is open.
// Ctrl+0 puts the level's own zoom back (zoom_editor_reset). Run the level loads a level (its own zoom); coming
// back, editor.cpp asks for the editor's zoom again (zoom_editor_restore). The editor's camera margin past the
// map is in map pixels, so camera.cpp takes it times zoom_ratio().
//
// A change is made between two frames, right after one is presented (frame_limit.cpp's wrapper of
// Graphics_EndScene). A step is reached in a glide of GLIDE_MS (220 ms; the test key's ZoomGlide in test mode, 0 =
// none), eased out (quick at first, settling softly) and even in the logarithm: the zooms in between are drawn in
// surfaces made with room for two steps more out (zoom_view.cpp keeps them while they are big enough and not much
// too big, so a run of notches makes them once), the map point under the cursor kept under it at each of them; a
// notch during a glide goes on from where the glide is towards the step after its target. Changing the zoom makes
// no sound: the state bar built again for a zoom does not play the panel's click (status_bar.cpp).
#include "patch.h"
#include <algorithm>
#include <cmath>
#include <psapi.h>

#pragma comment(lib, "psapi.lib")

void window_size(int& w, int& h);              // screen_scaling.cpp
void window_to_logical(POINT& p);
void logical_to_window(POINT& p);
bool menu_background_shown();
bool editor_level_held();                      // editor.cpp
bool panel_open();                             // editor_panel.cpp: a panel of the editor is open
bool edit_busy();                              // editor_edit.cpp: a drag or a stroke under way
void edit_zoom_changed();                      // editor_edit.cpp: its mouse at the new view
void frame_rate_section(const char* part);     // frame_limit.cpp
bool zoom_view_installed();                    // zoom_view.cpp
double zoom_view_base();
double zoom_view_whole();
void zoom_view_size(double z, int& w, int& h);
bool zoom_view_apply(double z, double surface_z);
void zoom_view_between_frames();
void zoom_view_level_loading();
bool mouse_pan_dragging();                     // mouse_pan.cpp
void install_zoom_view();

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t SET_POSITION = 0x41c680;
constexpr uintptr_t CALL_SCROLL_MOVE = 0x41cb3f;   // World_UpdateScroll: the camera's move, SetPosition(centre)
constexpr char REG_KEY[] = "SOFTWARE\\Gromada\\Locoland";
constexpr int WORLD_FLAGS = 0x04, WORLD_SCROLL_TYPE = 0x30, WORLD_CAMERA_X = 0x44, WORLD_CAMERA_Y = 0x48;
constexpr int WORLD_HWND = 0x214;
constexpr int MOUSE_WHEEL = 0x234, MOUSE_SCREEN_X = 0x240, MOUSE_SCREEN_Y = 0x244;
constexpr int MENU_HOVER = 0x24c + 0x14;
constexpr unsigned WORLD_EDITOR = 0x1, WORLD_ACTIVE = 0x8, WORLD_PAUSED = 0x10, WORLD_LOADING = 0x20;
constexpr unsigned SCROLL_BY_PLAYER = 0x1 | 0x20;   // SCROLL_FROM_BORDER, SCROLL_WITH_ARROWS
// The most map pixels a zoom's view may have: the whole of the biggest original map (see the top).
constexpr int MAX_PIXELS = 3072 * 2304;
// THE LADDER: one notch is a step of LADDER (12 %) from the level's own zoom, which is on it exactly, inward up to
// MAX_IN (the last step lands on it exactly), outward while the view is within the map and the cap (then the whole
// map, if that shows more). The glide to it: GLIDE_MS, eased out; a notch during a glide goes on from where the
// glide is towards the step after its target, so fast turns add up into one move. The editor's zoom takes the
// same (zoom_ladder_step, zoom_glide_ms).
constexpr double LADDER = 1.12;
constexpr double MAX_IN = 3.0;
constexpr int GLIDE_MS = 220;
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

int max_pixels() {
    static const int value = test_value("ZoomMaxPixels", MAX_PIXELS);
    return value;
}
int glide_ms() {
    static const int value = test_value("ZoomGlide", GLIDE_MS);
    return value;
}

// ======== 1. THE ZOOM (state and input) ========

constexpr int MAX_STEPS = 64;
double g_steps[MAX_STEPS] = {1.0};   // the zooms of the level, from in to out (made by make_steps)
int g_step_count = 1;
int g_own = 0;        // the index of the level's own zoom
int g_step = 0;       // the index of the step in effect or being glided to
bool g_made = false;  // the steps made for this level
double g_zoom = 1.0;  // the zoom drawn now (a step, or between two in a glide)

int g_editor_notches = 0;   // the editor's Ctrl+wheel, taken after the frame as the wheel is
bool g_editor_reset = false;   // Ctrl+0
double g_restore_zoom = 0;     // the editor back from Run the level: its zoom (0 = none), the view's middle
float g_restore_x = 0, g_restore_y = 0;

// a glide
bool g_gliding = false;
bool g_after_menu = false;   // a glide a menu stopped: its step reached when the menu goes
double g_from = 1.0, g_to = 1.0;
int64_t g_glide_start = 0;

// The frame time over a step's first seconds
int64_t g_measure_from = 0;   // QueryPerformanceCounter at the step's second frame, 0 = not measuring
int64_t g_measure_last = 0;
int g_measure_frames = -1;    // -1: the step's first frame (its switch) is left out
char g_measure_what[64] = "";

int64_t now_counter() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}
double counter_ms(int64_t from, int64_t to) {
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    return (to - from) * 1000.0 / f.QuadPart;
}

double pixels(double z) {
    int w, h;
    zoom_view_size(z, w, h);
    return static_cast<double>(w) * h;
}

// The zooms of this level and window (at its own zoom, when the wheel is first turned).
void make_steps() {
    const double own = zoom_view_base(), whole = zoom_view_whole();
    // inward: own x 1.12^k up to MAX_IN, the last one MAX_IN itself (written from the outermost in)
    double in[MAX_STEPS];
    int n_in = 0;
    if (own < MAX_IN * 0.9999) {
        for (double z = own * LADDER; z < MAX_IN * 0.9999 && n_in < MAX_STEPS / 2 - 1; z *= LADDER) in[n_in++] = z;
        in[n_in++] = MAX_IN;
    }
    g_step_count = 0;
    for (int i = n_in - 1; i >= 0; --i) g_steps[g_step_count++] = in[i];
    g_own = g_step_count;
    g_steps[g_step_count++] = own;
    // outward: own / 1.12^k while the view is within the map on an axis and within the cap, then the whole map
    double last = own;
    for (double z = own / LADDER; g_step_count < MAX_STEPS - 1; z /= LADDER) {
        if (z <= whole * 1.0001 || pixels(z) > max_pixels()) break;
        // a step within half a notch of the whole map is left to the whole map (no notch that hardly moves)
        if (z <= whole * 1.06 && pixels(whole) <= max_pixels()) break;
        g_steps[g_step_count++] = z;
        last = z;
    }
    if (whole < last * 0.97 && pixels(whole) <= max_pixels() && g_step_count < MAX_STEPS) g_steps[g_step_count++] = whole;
    g_step = g_own;
    g_zoom = own;   // the level's own (mission 1: its stretch), where a first glide starts
    g_made = true;
    if (test_mode()) {
        char text[MAX_STEPS * 12];
        int n = 0;
        for (int i = 0; i < g_step_count; ++i)
            n += wsprintfA(text + n, "%s%d%s", i ? " " : "", static_cast<int>(g_steps[i] * 1000 + 0.5),
                           i == g_own ? "*" : "");
        patch::log("zoom: steps x1000 (in to out, * the level's own) %s", text);
    }
}

void log_memory(double ms, double z) {
    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    pmc.cb = sizeof pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof pmc);
    MEMORYSTATUSEX status = {};
    status.dwLength = sizeof status;
    GlobalMemoryStatusEx(&status);
    const double mb = 1024.0 * 1024.0;
    int w, h;
    zoom_view_size(z, w, h);
    patch::log("zoom: view %dx%d in %.1f ms; memory: working set %.0f MB, private %.0f MB, address space used %.0f "
               "of %.0f MB",
               w, h, ms, pmc.WorkingSetSize / mb, pmc.PrivateUsage / mb,
               (status.ullTotalVirtual - status.ullAvailVirtual) / mb, status.ullTotalVirtual / mb);
}

// Why the wheel does not zoom now, or nullptr.
const char* refused(uint8_t* w) {
    const unsigned flags = at<unsigned>(w, WORLD_FLAGS);
    if (!(flags & WORLD_ACTIVE)) return "the window is not active";
    if (flags & WORLD_EDITOR) {   // the map editor (its level held: paused, its own scroll type)
        if (!editor_level_held()) return "editor mode, the level not held";
        if (panel_open()) return "a panel of the editor is open";
        if (edit_busy()) return "a drag or a stroke is under way";
        if (flags & WORLD_LOADING) return "loading";
        return nullptr;
    }
    if (flags & (WORLD_LOADING | WORLD_PAUSED)) return "loading or paused";
    if (!(at<unsigned>(w, WORLD_SCROLL_TYPE) & SCROLL_BY_PLAYER)) return "the scripts hold the camera";
    if (menu_background_shown()) return "a menu is up";
    if (mouse_pan_dragging()) return "the map is being dragged";   // the drag holds its own map point
    if (uint8_t* hover = at<uint8_t*>(w, MENU_HOVER)) {
        static char text[48];
        wsprintfA(text, "over the interface (VID %d)", at<uint8_t*>(hover, 0x1c) ? at<int>(at<uint8_t*>(hover, 0x1c), 4) : -1);
        return text;
    }
    return nullptr;
}

using SetPosition_t = void(__fastcall*)(void* world, void* edx, float x, float y, int mode);

// The game draws zoom z (surfaces for `surface_z` too), the map point under the cursor kept under it.
bool show_zoom(double z, double surface_z) {
    uint8_t* w = world();
    uint8_t* g = graphics();
    // the cursor: its place in the window, and the map point under it
    POINT cursor = {static_cast<LONG>(at<float>(w, MOUSE_SCREEN_X)), static_cast<LONG>(at<float>(w, MOUSE_SCREEN_Y))};
    const float map_x = at<float>(w, WORLD_CAMERA_X) + cursor.x, map_y = at<float>(w, WORLD_CAMERA_Y) + cursor.y;
    logical_to_window(cursor);
    if (!zoom_view_apply(z, surface_z)) return false;
    g_zoom = z;
    POINT now = cursor;
    window_to_logical(now);
    const float view_w = at<float>(g, 0x20c), view_h = at<float>(g, 0x210);
    reinterpret_cast<SetPosition_t>(SET_POSITION)(w, nullptr, map_x - now.x + view_w * 0.5f, map_y - now.y + view_h * 0.5f, 0);
    // the engine's mouse at the new mapping at once (it takes its place from WM_NCHITTEST)
    if (HWND window = at<HWND>(w, WORLD_HWND)) {
        POINT screen = cursor;
        ClientToScreen(window, &screen);
        SendMessageA(window, WM_NCHITTEST, 0, MAKELPARAM(screen.x, screen.y));
    }
    if (at<unsigned>(w, WORLD_FLAGS) & WORLD_EDITOR) edit_zoom_changed();
    return true;
}

// The step reached (the end of a glide, or at once): logged and measured.
void reached(int step, int64_t t0) {
    const double z = g_steps[step];
    if (!show_zoom(z, z)) return;
    int w, h;
    zoom_view_size(z, w, h);
    wsprintfA(g_measure_what, "zoom %d/1000 %dx%d", static_cast<int>(z * 1000 + 0.5), w, h);
    frame_rate_section(step != g_own ? g_measure_what : "");
    patch::log("zoom: step %d/1000 (%d of %d, the level's own %d)", static_cast<int>(z * 1000 + 0.5), step,
               g_step_count, g_own);
    log_memory(counter_ms(t0, now_counter()), z);
    g_measure_frames = -1;
    g_measure_from = 1;
}

void go_to(int step) {
    const int64_t t0 = now_counter();
    const double target = g_steps[step];
    g_step = step;
    if (glide_ms() <= 0 || std::fabs(target - g_zoom) < 1e-6) {
        g_gliding = false;
        reached(step, t0);
        return;
    }
    g_from = g_zoom;
    g_to = target;
    g_glide_start = t0;
    g_gliding = true;
}

// Every frame of a glide: the zoom between its ends, even in the logarithm (steady to the eye), eased.
void glide() {
    if (!g_gliding) return;
    double t = counter_ms(g_glide_start, now_counter()) / glide_ms();
    if (t >= 1.0) {
        g_gliding = false;
        reached(g_step, now_counter());
        return;
    }
    t = 1.0 - (1.0 - t) * (1.0 - t) * (1.0 - t);   // eased out: quick at first, settling softly
    const double z = g_from * std::pow(g_to / g_from, t);
    if (!show_zoom(z, (std::min)(g_from, g_to))) {   // the surfaces of the bigger view: no new ones per frame
        g_gliding = false;
        reached(g_step, now_counter());
    }
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

// World_UpdateScroll's move of the camera (SetPosition by the view's centre, 0x41cb3f): border and arrow
// scrolling - and the slide towards the flagman (scroll types 4 and 8, from his distance in map pixels) - go in
// map pixels a frame, so on the screen they would be z times faster zoomed in and slower zoomed out: the move
// is taken 1 / z times as far (the level's own zoom: as it was). Following the flagman (type 0x10) is another
// call, untouched.
void __fastcall scroll_move(void* w, void* edx, float x, float y, int mode) {
    uint8_t* g = graphics();
    if (g_made && g_steps[g_own] > 0 && std::fabs(g_zoom - g_steps[g_own]) > 1e-6) {
        const float cx = at<float>(g, 0x20c) * 0.5f + at<float>(w, WORLD_CAMERA_X);
        const float cy = at<float>(g, 0x210) * 0.5f + at<float>(w, WORLD_CAMERA_Y);
        const float k = static_cast<float>(g_steps[g_own] / g_zoom);
        x = cx + (x - cx) * k;
        y = cy + (y - cy) * k;
    }
    reinterpret_cast<SetPosition_t>(SET_POSITION)(w, edx, x, y, mode);
}

// The editor's Ctrl+0 and its zoom back after Run the level, between two frames.
void editor_after_frame(uint8_t* w) {
    if (g_editor_reset) {
        g_editor_reset = false;
        if (g_made && g_step != g_own && !refused(w)) go_to(g_own);
    }
    if (g_restore_zoom > 0 && editor_level_held() && !(at<unsigned>(w, WORLD_FLAGS) & WORLD_LOADING)) {
        const double z = g_restore_zoom;
        g_restore_zoom = 0;
        if (!g_made) make_steps();
        int best = g_own;
        for (int i = 0; i < g_step_count; ++i)
            if (std::fabs(std::log(g_steps[i] / z)) < std::fabs(std::log(g_steps[best] / z))) best = i;
        if (best == g_own) return;
        g_step = best;
        g_gliding = false;
        if (!zoom_view_apply(g_steps[best], g_steps[best])) {
            g_step = g_own;
            return;
        }
        g_zoom = g_steps[best];
        uint8_t* g = graphics();
        reinterpret_cast<SetPosition_t>(SET_POSITION)(w, nullptr, g_restore_x, g_restore_y, 0);
        edit_zoom_changed();
        patch::log("zoom: the editor's zoom %d/1000 back (view %.0fx%.0f)", static_cast<int>(g_zoom * 1000 + 0.5),
                   at<float>(g, 0x20c), at<float>(g, 0x210));
    }
}

}  // namespace

// editor_edit.cpp: Ctrl+wheel in the editor (notches > 0 away from the player: in), taken after the frame.
void zoom_editor_wheel(int notches) { g_editor_notches += notches; }

// editor_edit.cpp: Ctrl+0 - the level's own zoom again (after the frame).
void zoom_editor_reset() { g_editor_reset = true; }

// The zoom against the level's own: 1 at its own zoom, 2 at twice as big, 0.5 at half (the editor's status line).
double zoom_relative() { return g_made && g_steps[g_own] > 0 ? g_zoom / g_steps[g_own] : 1.0; }

// Map pixels per pixel of the level's own screen: 1 at its own zoom, 2 zoomed out to 1/2 (camera.cpp: the
// editor's margin past the map is that many times its own).
double zoom_ratio() { return 1.0 / zoom_relative(); }

// editor.cpp, Run the level: the zoom the editor had and the view's middle (map point), asked for again by
// zoom_editor_restore when the editor is back.
bool zoom_editor_kept(double& z, float& x, float& y) {
    uint8_t* w = world();
    uint8_t* g = graphics();
    if (!w || !g || !g_made || g_step == g_own) return false;
    z = g_steps[g_step];
    x = at<float>(w, WORLD_CAMERA_X) + at<float>(g, 0x20c) * 0.5f;
    y = at<float>(w, WORLD_CAMERA_Y) + at<float>(g, 0x210) * 0.5f;
    return true;
}
void zoom_editor_restore(double z, float x, float y) {
    g_restore_zoom = z;
    g_restore_x = x;
    g_restore_y = y;
}

// screen_scaling.cpp: a level is about to load; it gets its own screen again (its own zoom).
void zoom_level_loading() {
    g_made = false;
    g_gliding = false;
    g_after_menu = false;
    g_step = g_own = 0;
    g_steps[0] = 1.0;
    g_step_count = 1;
    g_zoom = 1.0;
    g_measure_from = 0;
    g_editor_notches = 0;
    g_editor_reset = false;
    zoom_view_level_loading();
}

// frame_limit.cpp: after every frame is presented.
void zoom_after_frame() {
    uint8_t* w = world();
    if (!w || !graphics() || !zoom_view_installed()) return;
    if (g_gliding && menu_background_shown()) {   // a menu came up during a glide: its step after the menu
        g_gliding = false;
        g_after_menu = true;
    }
    if (g_after_menu && !menu_background_shown()) {
        g_after_menu = false;
        zoom_view_between_frames();   // the zoom's view back from the menu's screen first
        reached(g_step, now_counter());
    }
    zoom_view_between_frames();
    glide();
    measure_frame();
    const bool editor = (at<unsigned>(w, WORLD_FLAGS) & WORLD_EDITOR) != 0;
    if (editor) editor_after_frame(w);
    const int notches = editor ? g_editor_notches : at<int>(w, MOUSE_WHEEL);
    g_editor_notches = 0;
    if (!notches) return;
    if (const char* why = refused(w)) {
        static const bool test = test_mode();   // the tests see why a turn did nothing; a player's log stays quiet
        if (test) patch::log("zoom: %d notch(es) not taken - %s", notches, why);
        return;
    }
    if (!editor) at<int>(w, MOUSE_WHEEL) = 0;   // taken: nothing after the frame reads it again
    if (!g_made) make_steps();
    int step = g_step - notches;   // one step a notch; away from the player (positive) goes in
    if (step < 0) step = 0;
    if (step >= g_step_count) step = g_step_count - 1;
    if (step != g_step) go_to(step);
    else if (test_mode()) patch::log("zoom: %d notch(es) at step %d of %d - no other step", notches, g_step, g_step_count);
}

void install_zoom() {
    install_zoom_view();
    if (!zoom_view_installed()) return;
    const uint8_t* b = reinterpret_cast<const uint8_t*>(CALL_SCROLL_MOVE);
    int32_t rel;
    std::memcpy(&rel, b + 1, 4);
    if (b[0] != 0xE8 || CALL_SCROLL_MOVE + 5 + rel != SET_POSITION) {
        patch::log("zoom: scrolling's speed not followed (unexpected bytes)");
        return;
    }
    patch::write_call(CALL_SCROLL_MOVE, nullptr, 0, reinterpret_cast<void*>(&scroll_move), 5);
}

// The editor's zoom (another file) takes the same ladder and glide.
double zoom_ladder_step() { return LADDER; }
int zoom_glide_ms() { return glide_ms(); }
