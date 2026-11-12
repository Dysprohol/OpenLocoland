// Windowed mode: launcher, window size and position, picture and mouse relative to the
// game area.
#include "patch.h"

namespace {

// Addresses in Steam.exe (see re/symbols.csv).
constexpr uintptr_t IAT_CreateWindowExA = 0x475154;
constexpr uintptr_t IAT_GetWindowRect = 0x475170;

// At 0x40d950 Launcher_FillOrRead checks whether the selected device can render
// in a window. dgVoodoo does not report it, so the game ticked and disabled the
// "Full-Screen" box. We skip only this test (je -> nop nop); the "colour depth =
// desktop" and "resolution <= screen" conditions stay.
void unlock_windowed_mode_in_launcher() {
    const uintptr_t address = 0x40d95e;
    const uint8_t je[] = {0x74, 0x5D};
    const uint8_t nop[] = {0x90, 0x90};
    if (patch::check(address, je, sizeof je))
        patch::write(address, nop, sizeof nop);
    else
        patch::log("skipped windowed mode unlock @ %08X: unexpected bytes", static_cast<unsigned>(address));
}

// The game passes the resolution as the size of the whole window, so the game area
// came out smaller than the picture (e.g. 1018x739) and got scaled. Here we grow the
// window by its frame and title bar, and without a saved position (0,0) we put it in
// the middle of the monitor. So we do with a saved position on no monitor: the game saves
// the window rectangle on exit (0x41c423), which is (-32000, -32000) for a minimised
// window, and the next start would put the window there, out of sight. A menu bar (the class
// has one in editor mode, editor.cpp) is part of the window too.
//
// The game is per-monitor DPI-aware (dpi_aware.cpp), so a window's frame and title bar have the
// size of the monitor it is on: twice as tall on a monitor at 200 % as at 100 %. The frame is
// computed for the monitor the window opens on (AdjustWindowRectExForDpi, Windows 10 1607; before,
// AdjustWindowRectEx used the primary's scale and a 1920x1080 window on a 200 % TV had a game area
// of 1920x1069, seen 2026-11-12), and when the window moves to a monitor of another scale
// (WM_DPICHANGED) it is resized so that its game area keeps the chosen mode's size: Windows had
// shrunk it (1920x1080 -> 1439x800 dragged from the 200 % TV to a 100 % monitor), and the picture
// and the mouse no longer matched the engine's screen.
using AdjustForDpi_t = BOOL(WINAPI*)(LPRECT, DWORD, BOOL, DWORD, UINT);
using GetDpiForMonitor_t = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);

AdjustForDpi_t g_adjust_for_dpi = nullptr;
GetDpiForMonitor_t g_dpi_for_monitor = nullptr;

struct Kept { HWND window; int width, height; DWORD style, ex_style; BOOL menu; WNDPROC previous; } g_kept = {};

UINT monitor_dpi(HMONITOR monitor) {
    UINT x = 0, y = 0;
    if (monitor && g_dpi_for_monitor && SUCCEEDED(g_dpi_for_monitor(monitor, 0, &x, &y))) return x;   // MDT_EFFECTIVE_DPI
    return 0;
}

// The window's size for a game area of width x height at a DPI (0: as AdjustWindowRectEx has it).
SIZE window_size(int width, int height, DWORD style, BOOL menu, DWORD ex_style, UINT dpi) {
    RECT r = {0, 0, width, height};
    if (dpi && g_adjust_for_dpi) g_adjust_for_dpi(&r, style, menu, ex_style, dpi);
    else AdjustWindowRectEx(&r, style, menu, ex_style);
    return {r.right - r.left, r.bottom - r.top};
}

LRESULT CALLBACK kept_wndproc(HWND window, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_DPICHANGED && window == g_kept.window && (GetWindowLongA(window, GWL_STYLE) & WS_CAPTION)) {
        const RECT* suggested = reinterpret_cast<const RECT*>(l);
        const SIZE size = window_size(g_kept.width, g_kept.height, g_kept.style, g_kept.menu, g_kept.ex_style, HIWORD(w));
        SetWindowPos(window, nullptr, suggested->left, suggested->top, size.cx, size.cy, SWP_NOZORDER | SWP_NOACTIVATE);
        RECT client;
        GetClientRect(window, &client);
        patch::log("window: DPI %u - game area kept at %ldx%ld", HIWORD(w), client.right, client.bottom);
        return 0;
    }
    return CallWindowProcA(g_kept.previous, window, m, w, l);
}

HWND WINAPI create_window(DWORD ex_style, LPCSTR cls, LPCSTR title, DWORD style, int x, int y,
                          int width, int height, HWND parent, HMENU menu, HINSTANCE inst, LPVOID param) {
    const int area_width = width, area_height = height;
    BOOL has_menu = FALSE;
    if (style & WS_CAPTION) {
        WNDCLASSA wc;
        has_menu = menu || (GetClassInfoA(inst, cls, &wc) && wc.lpszMenuName);
        SIZE size = window_size(width, height, style, has_menu, ex_style, 0);
        const RECT saved = {x, y, x + size.cx, y + size.cy};
        HMONITOR monitor = MonitorFromRect(&saved, MONITOR_DEFAULTTONULL);
        if (x == 0 && y == 0 || !monitor) monitor = MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY);
        size = window_size(width, height, style, has_menu, ex_style, monitor_dpi(monitor));   // that monitor's frame
        width = size.cx;
        height = size.cy;
        if (x == 0 && y == 0 || !MonitorFromRect(&saved, MONITOR_DEFAULTTONULL)) {
            MONITORINFO mi = {sizeof mi};
            GetMonitorInfoA(monitor, &mi);
            const RECT& work = mi.rcWork;
            x = work.left + ((work.right - work.left) - width) / 2;
            y = work.top + ((work.bottom - work.top) - height) / 2;
            if (x < work.left) x = work.left;
            if (y < work.top) y = work.top;
        }
    }
    HWND window = CreateWindowExA(ex_style, cls, title, style, x, y, width, height, parent, menu, inst, param);
    if (window && (style & WS_CAPTION) && !g_kept.window) {
        g_kept = {window, area_width, area_height, style, ex_style, has_menu, nullptr};
        g_kept.previous = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&kept_wndproc)));
    }
    return window;
}

// GetWindowRect replacement: the game area rectangle in screen coordinates.
// In full screen it equals the window rectangle.
BOOL WINAPI game_area_rect(HWND window, RECT* r) {
    if (!GetClientRect(window, r)) return FALSE;
    ClientToScreen(window, reinterpret_cast<POINT*>(&r->left));
    ClientToScreen(window, reinterpret_cast<POINT*>(&r->right));
    return TRUE;
}

}  // namespace

void install_window() {
    g_adjust_for_dpi = reinterpret_cast<AdjustForDpi_t>(GetProcAddress(GetModuleHandleA("user32.dll"), "AdjustWindowRectExForDpi"));
    if (HMODULE shcore = LoadLibraryA("shcore.dll"))
        g_dpi_for_monitor = reinterpret_cast<GetDpiForMonitor_t>(GetProcAddress(shcore, "GetDpiForMonitor"));
    unlock_windowed_mode_in_launcher();
    patch::redirect_iat_call(0x41a832, IAT_CreateWindowExA, reinterpret_cast<void*>(&create_window),
                             "CreateWindowExA");

    // Drawing area, copying the picture to the window and the mouse position. Saving the
    // window position on exit (0x41c423) keeps the plain GetWindowRect.
    const uintptr_t sites[] = {0x40e296, 0x414ec2, 0x415925, 0x415b0b, 0x417a61, 0x417bf4};
    for (uintptr_t s : sites)
        patch::redirect_iat_call(s, IAT_GetWindowRect, reinterpret_cast<void*>(&game_area_rect), "GetWindowRect");
}
