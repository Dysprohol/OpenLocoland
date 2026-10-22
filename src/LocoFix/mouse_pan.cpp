// Camera panning with the middle mouse button: hold it and drag the map.
//
// The engine scrolls in World_UpdateScroll (0x41c830, once a frame) by the scroll type
// the scripts set: SCROLL_FROM_BORDER (1) and SCROLL_WITH_ARROWS (0x20) accelerate the
// camera, the SCROLL_TO_FLAGMAN modes follow a unit. SCROLL_WITH_MOUSE (2) is declared in
// export.lgc but the engine never implemented it, and the middle button is free (Mouse
// only sets a bit for it that nothing reads).
//
// Mouse_HandleMessage (0x417a10) gets every window message while the game window is
// active. On WM_MBUTTONDOWN we remember the cursor and the camera; while the button is
// held, World_UpdateScroll puts the camera at the start position minus the cursor
// movement (the map point under the cursor follows it) through SetPosition, so the map
// bounds and the centring of small maps (camera.cpp) still apply. Border and arrow
// scrolling are skipped meanwhile. The cursor movement is converted to logical pixels
// (screen_scaling.cpp), so the map keeps up with the cursor at any resolution.
//
// Panning works when the script allows border or arrow scrolling, and not while the game
// is paused (except a level editor mode holds, editor.cpp), a menu is shown or the window
// is inactive. Outside test mode the window
// captures the mouse during a drag, so the drag goes on when the cursor leaves it.
#include "patch.h"
#include <windowsx.h>

void window_to_logical(POINT& p);
bool menu_background_shown();
bool editor_level_held();

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t MOUSE_HANDLE_MESSAGE = 0x417a10;
constexpr uintptr_t UPDATE_SCROLL = 0x41c830;
constexpr uintptr_t SET_POSITION = 0x41c680;
constexpr uintptr_t G_SCROLL_SPEED_X = 0x4a9c20;   // border/arrow scrolling speed (accelerates)
constexpr uintptr_t G_SCROLL_SPEED_Y = 0x4a9c24;
constexpr unsigned WORLD_ACTIVE = 0x8, WORLD_PAUSED = 0x10, WORLD_LOADING = 0x20;
constexpr unsigned SCROLL_BY_PLAYER = 0x1 | 0x20;  // SCROLL_FROM_BORDER, SCROLL_WITH_ARROWS

// thiscall: ecx = world, x, y (centre of the view), mode on the stack
using SetPosition_t = void(__fastcall*)(void* world, void* edx, float x, float y, int mode);
const auto g_set_position = reinterpret_cast<SetPosition_t>(SET_POSITION);

bool g_test_mode = false;   // LOCOFIX_TEST=1: no mouse capture, the cursor is virtual

struct Drag {
    bool active = false;
    HWND window = nullptr;
    POINT start = {}, current = {};   // cursor in the game area, window pixels
    float camera_x = 0, camera_y = 0; // camera at the start
    int level_start = 0;              // World levelStartTime - a new level ends the drag
} g_drag;

template <typename T> T& field(uint8_t* base, int offset) { return *reinterpret_cast<T*>(base + offset); }

bool pan_allowed(uint8_t* world) {
    const unsigned flags = field<unsigned>(world, 0x04);
    return (flags & WORLD_ACTIVE) && !(flags & WORLD_LOADING) && (!(flags & WORLD_PAUSED) || editor_level_held()) &&
           (field<unsigned>(world, 0x30) & SCROLL_BY_PLAYER) && !menu_background_shown();
}

void end_drag() {
    if (!g_drag.active) return;
    g_drag.active = false;
    if (!g_test_mode && GetCapture() == g_drag.window) ReleaseCapture();
}

void begin_drag(HWND window, LPARAM l) {
    auto* world = *reinterpret_cast<uint8_t**>(G_WORLD);
    if (!world || !pan_allowed(world)) return;
    g_drag.active = true;
    g_drag.window = window;
    g_drag.start = g_drag.current = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    g_drag.camera_x = field<float>(world, 0x44);
    g_drag.camera_y = field<float>(world, 0x48);
    g_drag.level_start = field<int>(world, 0x24);
    if (!g_test_mode) SetCapture(window);
}

using MouseHandleMessage_t = int(__fastcall*)(void* mouse, void* edx, HWND window, UINT m, WPARAM w, LPARAM l);
MouseHandleMessage_t g_mouse_handle_message = nullptr;

int __fastcall MouseHandleMessage_hook(void* mouse, void* edx, HWND window, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_MBUTTONDOWN: begin_drag(window, l); break;
    case WM_MBUTTONUP: end_drag(); break;
    case WM_MOUSEMOVE:
        if (g_drag.active) g_drag.current = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        break;
    case WM_CAPTURECHANGED:
        if (g_drag.active && !g_test_mode && reinterpret_cast<HWND>(l) != g_drag.window) end_drag();
        break;
    }
    return g_mouse_handle_message(mouse, edx, window, m, w, l);
}

using UpdateScroll_t = void(__fastcall*)(void* world, void* edx);
UpdateScroll_t g_update_scroll = nullptr;

void __fastcall UpdateScroll_hook(void* world_ptr, void* edx) {
    auto* world = static_cast<uint8_t*>(world_ptr);
    if (g_drag.active && (!pan_allowed(world) || field<int>(world, 0x24) != g_drag.level_start ||
                          (!g_test_mode && GetCapture() != g_drag.window)))
        end_drag();
    if (!g_drag.active) {
        g_update_scroll(world_ptr, edx);
        return;
    }
    *reinterpret_cast<float*>(G_SCROLL_SPEED_X) = 0.0f;
    *reinterpret_cast<float*>(G_SCROLL_SPEED_Y) = 0.0f;

    POINT start = g_drag.start, current = g_drag.current;
    window_to_logical(start);
    window_to_logical(current);
    auto* screen = reinterpret_cast<float*>(*reinterpret_cast<uint8_t**>(G_GRAPHICS) + 0x20c);   // width, height
    const float x = g_drag.camera_x - static_cast<float>(current.x - start.x) + screen[0] * 0.5f;
    const float y = g_drag.camera_y - static_cast<float>(current.y - start.y) + screen[1] * 0.5f;
    g_set_position(world_ptr, nullptr, x, y, 0);
}

}  // namespace

void install_mouse_pan() {
    char value[8] = {};
    g_test_mode = GetEnvironmentVariableA("LOCOFIX_TEST", value, sizeof value) && value[0] == '1';

    // mov eax,[esp+8] / sub esp,0x10
    const uint8_t mouse[] = {0x8B, 0x44, 0x24, 0x08, 0x83, 0xEC, 0x10};
    // sub esp,8 / push esi / mov esi,ecx / push edi
    const uint8_t scroll[] = {0x83, 0xEC, 0x08, 0x56, 0x8B, 0xF1, 0x57};
    if (!patch::check(MOUSE_HANDLE_MESSAGE, mouse, sizeof mouse) ||
        !patch::check(UPDATE_SCROLL, scroll, sizeof scroll)) {
        patch::log("skipped mouse panning: unexpected bytes");
        return;
    }
    g_update_scroll = reinterpret_cast<UpdateScroll_t>(patch::hook(
        UPDATE_SCROLL, scroll, sizeof scroll, reinterpret_cast<void*>(&UpdateScroll_hook), "World_UpdateScroll"));
    if (!g_update_scroll) return;
    g_mouse_handle_message = reinterpret_cast<MouseHandleMessage_t>(patch::hook(
        MOUSE_HANDLE_MESSAGE, mouse, sizeof mouse, reinterpret_cast<void*>(&MouseHandleMessage_hook),
        "Mouse_HandleMessage"));
}
