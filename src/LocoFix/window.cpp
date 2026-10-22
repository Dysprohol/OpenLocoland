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
HWND WINAPI create_window(DWORD ex_style, LPCSTR cls, LPCSTR title, DWORD style, int x, int y,
                          int width, int height, HWND parent, HMENU menu, HINSTANCE inst, LPVOID param) {
    if (style & WS_CAPTION) {
        WNDCLASSA wc;
        const BOOL has_menu = menu || (GetClassInfoA(inst, cls, &wc) && wc.lpszMenuName);
        RECT r = {0, 0, width, height};
        AdjustWindowRectEx(&r, style, has_menu, ex_style);
        width = r.right - r.left;
        height = r.bottom - r.top;
        const RECT saved = {x, y, x + width, y + height};
        if (x == 0 && y == 0 || !MonitorFromRect(&saved, MONITOR_DEFAULTTONULL)) {
            MONITORINFO mi = {sizeof mi};
            GetMonitorInfoA(MonitorFromPoint({0, 0}, MONITOR_DEFAULTTOPRIMARY), &mi);
            const RECT& work = mi.rcWork;
            x = work.left + ((work.right - work.left) - width) / 2;
            y = work.top + ((work.bottom - work.top) - height) / 2;
            if (x < work.left) x = work.left;
            if (y < work.top) y = work.top;
        }
    }
    return CreateWindowExA(ex_style, cls, title, style, x, y, width, height, parent, menu, inst, param);
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
    unlock_windowed_mode_in_launcher();
    patch::redirect_iat_call(0x41a832, IAT_CreateWindowExA, reinterpret_cast<void*>(&create_window),
                             "CreateWindowExA");

    // Drawing area, copying the picture to the window and the mouse position. Saving the
    // window position on exit (0x41c423) keeps the plain GetWindowRect.
    const uintptr_t sites[] = {0x40e296, 0x414ec2, 0x415925, 0x415b0b, 0x417a61, 0x417bf4};
    for (uintptr_t s : sites)
        patch::redirect_iat_call(s, IAT_GetWindowRect, reinterpret_cast<void*>(&game_area_rect), "GetWindowRect");
}
