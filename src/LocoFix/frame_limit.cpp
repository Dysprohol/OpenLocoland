// Frame rate limit and counter: the game renders as fast as it can (about 600 fps on the
// logo, 300 in the menu, 180 in a mission), which keeps a CPU core and the GPU busy for
// nothing.
//
// The main loop (GameWorld_Run) does one game step, draws the scene and presents it in
// Graphics_EndScene. The game step takes the real time since the previous step
// (timeGetTime, at most 71 ms) times the game speed, so fewer frames do not slow the game
// logic or the script clock (GetTime) - the steps just get longer. We wait after the
// present, so the next step reads the input and the time right after the wait and its
// frame is shown as soon as it is drawn. The wait is Sleep with a 1 ms timer resolution
// (timeBeginPeriod, which also makes the game's timeGetTime steps exact) for the bulk and
// a short spin for the rest.
//
// The launcher gets a row under the resolution: "FPS counter" and "Frame limit" (30 ... 240
// or none). OK saves both in the registry next to the game's settings (FpsCounter,
// FrameLimit; default off and 60). The counter is drawn by screen_scaling.cpp on the
// final picture just before it is presented (draw_fps_counter), at the top edge left of
// the game speed and money boxes, so it covers no part of the interface.
//
// LocoFix.log gets the measured frame rate of every level with the game clock against
// the real time (the game's own "Average fps" in error.log comes out as 0 for missions).
#include "patch.h"
#include <ddraw.h>
#include <mmsystem.h>
#include <cwchar>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "winmm.lib")

void add_launcher_label(int id, const char* key);   // language.cpp

// test_map.cpp: --test-map - the map to load in place of the requested one (once, the
// first level) and whether it really loaded.
const char* test_map_replace(const char* requested);
void test_map_check(void* world);
// editor.cpp: editor mode holds every level it loads
void editor_level_loading(void* world);
const char* editor_level_redirect(const char* map);
void editor_level_loaded(void* world);
void restart_level_loading(const char* map);   // restart.cpp
void factions_level_loading(const char* map);  // factions.cpp

namespace {

constexpr uintptr_t G_GAME_TIME = 0x4a9d64;   // ms, advanced by Game_Step
constexpr uintptr_t END_SCENE = 0x414d50;
constexpr uintptr_t RUN_CALL_END_SCENE = 0x423cac;   // GameWorld_Run: call Graphics_EndScene
constexpr uintptr_t LEVEL_LOAD = 0x41cff0;
constexpr uintptr_t CONSTRUCT_CALL_LEVEL_LOAD = 0x423482;   // GameWorld_Construct: the first level
constexpr uintptr_t VTBL_LEVEL_LOAD = 0x4756f4 + 6 * 4;   // GameWorld vtable slot 6: the next ones
constexpr uintptr_t IAT_DialogBoxParamA = 0x4751ac;
constexpr char REG_KEY[] = "SOFTWARE\\Gromada\\Locoland";   // RegPath in Locoland.cfg
constexpr int DEFAULT_LIMIT = 60;
constexpr int MAX_LIMIT = 1000;
constexpr int LIMITS[] = {30, 60, 75, 120, 144, 165, 240, 0};   // launcher list, 0 = none
constexpr int ID_COUNTER = 1110;
constexpr int ID_LIMIT_LABEL = 1111;
constexpr int ID_LIMIT = 1112;
constexpr int ID_RESOLUTION = 1001;
constexpr int ROW_HEIGHT = 14;   // dialog units added to the launcher
// The game speed and money boxes take the top right corner: about 122 x 20 pixels of
// the logical screen. The counter goes left of them, vertically centred on them.
constexpr int BOXES_WIDTH = 128;
constexpr int BOXES_MIDDLE = 10;

using EndScene_t = void(__fastcall*)(uint8_t* g, void* edx, int present);
const auto g_end_scene = reinterpret_cast<EndScene_t>(END_SCENE);

int64_t g_frequency = 0;   // QueryPerformanceCounter ticks per second
int64_t g_period = 0;      // ticks per frame, 0 = no limit
int64_t g_next = 0;        // earliest start of the next frame
int g_limit = DEFAULT_LIMIT;
bool g_counter = false;

int64_t now() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

void set_limit(int limit) {
    g_limit = limit < 0 ? 0 : limit > MAX_LIMIT ? MAX_LIMIT : limit;
    static bool timer_resolution = false;
    if (g_limit && !timer_resolution) {
        timeBeginPeriod(1);
        timer_resolution = true;
    }
    g_period = g_limit ? g_frequency / g_limit : 0;
    g_next = 0;
}

void wait_for_next_frame() {
    int64_t t = now();
    if (g_next && t < g_next) {
        const int64_t spin = g_frequency * 3 / 2000;   // 1.5 ms: Sleep(1) may take up to 2
        while (g_next - t > spin) {
            Sleep(1);
            t = now();
        }
        while (t < g_next) {
            YieldProcessor();
            t = now();
        }
    }
    // A frame late by less than a period is made up by the next one, so the average stays
    // exact; after a longer delay (a slow frame, a load) we start afresh instead of rushing.
    g_next = (!g_next || t - g_next > g_period) ? t + g_period : g_next + g_period;
}

// ---- measurement ----

// Frames, real time and game time since the last level load; pauses over half a second
// (the window losing focus, the game waiting for messages) are left out. The game time
// against the real time shows that the limit does not change the game speed.
char g_level[MAX_PATH] = "";
int64_t g_last_frame = 0, g_level_time = 0;
uint32_t g_last_game_time = 0;
int64_t g_level_game_time = 0;   // ms
unsigned g_frames = 0;           // frames counted in g_level_time

void report_level() {
    const double seconds = static_cast<double>(g_level_time) / g_frequency;
    if (seconds >= 1.0)
        patch::log("frame rate in %s: %.1f fps (%u frames, %.1f s; game clock x%.2f)", g_level,
                   g_frames / seconds, g_frames, seconds, g_level_game_time / (seconds * 1000.0));
    g_frames = 0;
    g_level_time = 0;
    g_level_game_time = 0;
    g_last_frame = 0;
}

// The counter shows the frames of the last half second.
int64_t g_counter_start = 0;
unsigned g_counter_frames = 0;
int g_counter_value = -1;   // -1 = nothing measured yet

void count_frame() {
    const int64_t t = now();
    const uint32_t game_time = *reinterpret_cast<uint32_t*>(G_GAME_TIME);
    if (g_last_frame && t - g_last_frame <= g_frequency / 2) {
        g_level_time += t - g_last_frame;
        g_level_game_time += static_cast<int32_t>(game_time - g_last_game_time);
        ++g_frames;
    }
    g_last_frame = t;
    g_last_game_time = game_time;

    ++g_counter_frames;
    if (t - g_counter_start >= g_frequency / 2) {
        if (g_counter_start)
            g_counter_value = static_cast<int>(g_counter_frames * g_frequency / (t - g_counter_start) + 0.5);
        g_counter_start = t;
        g_counter_frames = 0;
    }
}

using LevelLoad_t = int(__fastcall*)(void* world, void* edx, const char* name);
const auto g_level_load = reinterpret_cast<LevelLoad_t>(LEVEL_LOAD);

int __fastcall LevelLoad_measured(void* world, void* edx, const char* name) {
    report_level();
    const char* map = editor_level_redirect(test_map_replace(name));
    lstrcpynA(g_level, map ? map : "?", sizeof g_level);
    restart_level_loading(map);
    factions_level_loading(map);
    editor_level_loading(world);
    const int result = g_level_load(world, edx, map);
    test_map_check(world);
    editor_level_loaded(world);
    return result;
}

void __fastcall EndScene_limited(uint8_t* g, void* edx, int present) {
    g_end_scene(g, edx, present);
    if (g_period) wait_for_next_frame();
    count_frame();
}

// ---- settings ----

int read_setting(const char* name, int fallback) {
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return fallback;
    DWORD value = 0, size = sizeof value, type = 0;
    const LONG r = RegQueryValueExA(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS && type == REG_DWORD ? static_cast<int>(value) : fallback;
}

void save_settings() {
    HKEY key;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS)
        return;
    const DWORD limit = g_limit, counter = g_counter;
    RegSetValueExA(key, "FrameLimit", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&limit), sizeof limit);
    RegSetValueExA(key, "FpsCounter", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&counter), sizeof counter);
    RegCloseKey(key);
}

void log_settings() {
    if (g_limit) patch::log("frame limit: %d fps, counter %s", g_limit, g_counter ? "on" : "off");
    else patch::log("frame limit: off, counter %s", g_counter ? "on" : "off");
}

// ---- launcher ----

using DialogBoxParamA_t = INT_PTR(WINAPI*)(HINSTANCE, LPCSTR, HWND, DLGPROC, LPARAM);
DialogBoxParamA_t g_dialog_box = nullptr;   // language.cpp's hook, or the system function
DLGPROC g_launcher_proc = nullptr;

RECT dialog_rect(HWND dialog, int x, int y, int w, int h) {
    RECT r = {x, y, x + w, y + h};
    MapDialogRect(dialog, &r);
    return r;
}

HWND add_control(HWND dialog, const wchar_t* cls, const wchar_t* text, DWORD style, RECT r, int id) {
    HWND control = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, r.left, r.top, r.right - r.left,
                                   r.bottom - r.top, dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(dialog, GWLP_HINSTANCE)), nullptr);
    SendMessageW(control, WM_SETFONT, SendMessageW(dialog, WM_GETFONT, 0, 0), FALSE);
    return control;
}

// Moves a control down by `dy` pixels.
void move_down(HWND dialog, int id, int dy) {
    HWND control = GetDlgItem(dialog, id);
    RECT r;
    GetWindowRect(control, &r);
    MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&r), 2);
    SetWindowPos(control, nullptr, r.left, r.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

// A row under the resolution list (7,178 after patch_exe.py); OK, Cancel and the bottom
// of the window move down to make room.
void add_launcher_row(HWND dialog) {
    if (!GetDlgItem(dialog, ID_RESOLUTION)) return;
    const RECT row = dialog_rect(dialog, 0, 0, 0, ROW_HEIGHT);
    move_down(dialog, IDOK, row.bottom);
    move_down(dialog, IDCANCEL, row.bottom);
    RECT w;
    GetWindowRect(dialog, &w);
    SetWindowPos(dialog, nullptr, 0, 0, w.right - w.left, w.bottom - w.top + row.bottom, SWP_NOMOVE | SWP_NOZORDER);

    HWND counter = add_control(dialog, L"BUTTON", L"Licznik FPS", WS_TABSTOP | BS_AUTOCHECKBOX,
                               dialog_rect(dialog, 7, 194, 72, 10), ID_COUNTER);
    SendMessageW(counter, BM_SETCHECK, g_counter ? BST_CHECKED : BST_UNCHECKED, 0);
    add_control(dialog, L"STATIC", L"Limit FPS:", SS_RIGHT, dialog_rect(dialog, 80, 195, 40, 8), ID_LIMIT_LABEL);
    HWND list = add_control(dialog, L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                            dialog_rect(dialog, 122, 192, 58, 110), ID_LIMIT);
    bool listed = false;
    for (int limit : LIMITS) listed |= limit == g_limit;
    for (int limit : LIMITS) {
        if (!listed && (limit == 0 || limit > g_limit)) {   // a value set by hand in the registry
            wchar_t text[16];
            std::swprintf(text, 16, L"%d", g_limit);
            const auto i = SendMessageW(list, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
            SendMessageW(list, CB_SETITEMDATA, i, g_limit);
            listed = true;
        }
        wchar_t text[16];
        if (limit) std::swprintf(text, 16, L"%d", limit);
        else std::wcscpy(text, L"\x221E");   // none
        const auto i = SendMessageW(list, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text));
        SendMessageW(list, CB_SETITEMDATA, i, limit);
    }
    const int count = static_cast<int>(SendMessageW(list, CB_GETCOUNT, 0, 0));
    for (int i = 0; i < count; ++i)
        if (static_cast<int>(SendMessageW(list, CB_GETITEMDATA, i, 0)) == g_limit) SendMessageW(list, CB_SETCURSEL, i, 0);
}

void read_launcher_row(HWND dialog) {
    HWND list = GetDlgItem(dialog, ID_LIMIT);
    if (!list) return;
    const auto selected = SendMessageW(list, CB_GETCURSEL, 0, 0);
    if (selected >= 0) set_limit(static_cast<int>(SendMessageW(list, CB_GETITEMDATA, selected, 0)));
    g_counter = IsDlgButtonChecked(dialog, ID_COUNTER) == BST_CHECKED;
    save_settings();
    log_settings();
}

INT_PTR CALLBACK launcher_proc(HWND dialog, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND && LOWORD(w) == IDOK) read_launcher_row(dialog);
    const INT_PTR result = g_launcher_proc(dialog, m, w, l);
    if (m == WM_INITDIALOG) add_launcher_row(dialog);
    return result;
}

INT_PTR WINAPI DialogBoxParamA_frame_limit(HINSTANCE instance, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param) {
    if (!IS_INTRESOURCE(name) && lstrcmpiA(name, "START_DIALOG") == 0) {
        g_launcher_proc = proc;
        proc = &launcher_proc;
    }
    return g_dialog_box(instance, name, parent, proc, param);
}

// ---- counter ----

HFONT g_font = nullptr;
int g_font_height = 0;

bool is_call(uintptr_t address, uintptr_t target) {
    uint8_t call[5] = {0xE8};
    const int32_t rel = static_cast<int32_t>(target - (address + 5));
    std::memcpy(call + 1, &rel, 4);
    return patch::check(address, call, sizeof call);
}

}  // namespace

// Called by screen_scaling.cpp on the back buffer with the final picture, just before the
// present. `picture` is where the logical screen landed in the window, `scale` its zoom.
void draw_fps_counter(IDirectDrawSurface7* back_buffer, const RECT& picture, double scale) {
    if (!g_counter || g_counter_value < 0 || !back_buffer) return;
    const int height = static_cast<int>(13 * scale + 0.5);
    if (!g_font || g_font_height != height) {
        if (g_font) DeleteObject(g_font);
        g_font = CreateFontW(-height, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Tahoma");
        g_font_height = height;
    }
    HDC dc;
    if (FAILED(back_buffer->GetDC(&dc))) return;
    wchar_t text[16];
    const int length = std::swprintf(text, 16, L"%d FPS", g_counter_value);
    const HGDIOBJ old_font = SelectObject(dc, g_font);
    SIZE size;
    GetTextExtentPoint32W(dc, text, length, &size);
    const int x = picture.right - static_cast<int>(BOXES_WIDTH * scale + 0.5) - size.cx;
    const int y = picture.top + static_cast<int>(BOXES_MIDDLE * scale + 0.5) - size.cy / 2;
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, RGB(0, 0, 0));   // shadow, readable on any ground
    TextOutW(dc, x + 1, y + 1, text, length);
    SetTextColor(dc, RGB(96, 255, 64));   // the green of the game's counters
    TextOutW(dc, x, y, text, length);
    SelectObject(dc, old_font);
    back_buffer->ReleaseDC(dc);
}

void install_frame_limit() {
    if (!is_call(RUN_CALL_END_SCENE, END_SCENE)) {
        patch::log("skipped frame limit: unexpected bytes");
        return;
    }
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    g_frequency = f.QuadPart;
    set_limit(read_setting("FrameLimit", DEFAULT_LIMIT));
    g_counter = read_setting("FpsCounter", 0) != 0;
    log_settings();
    patch::write_call(RUN_CALL_END_SCENE, nullptr, 0, reinterpret_cast<void*>(&EndScene_limited), 5);

    // Installed after language.cpp: we take over its DialogBoxParamA entry and call it in
    // turn, so its proc wraps ours and labels our controls.
    g_dialog_box = *reinterpret_cast<DialogBoxParamA_t*>(IAT_DialogBoxParamA);
    void* dialog_box = reinterpret_cast<void*>(&DialogBoxParamA_frame_limit);
    patch::write(IAT_DialogBoxParamA, &dialog_box, sizeof dialog_box);
    add_launcher_label(ID_COUNTER, "fps_counter");
    add_launcher_label(ID_LIMIT_LABEL, "frame_limit");

    // the measurement starts afresh with every level
    void* measured = reinterpret_cast<void*>(&LevelLoad_measured);
    if (is_call(CONSTRUCT_CALL_LEVEL_LOAD, LEVEL_LOAD) &&
        *reinterpret_cast<uintptr_t*>(VTBL_LEVEL_LOAD) == LEVEL_LOAD) {
        patch::write_call(CONSTRUCT_CALL_LEVEL_LOAD, nullptr, 0, measured, 5);
        patch::write(VTBL_LEVEL_LOAD, &measured, sizeof measured);
    } else {
        patch::log("frame rate per level skipped: unexpected bytes");
    }
}

void frame_limit_exit() {
    if (g_frequency) report_level();
}
