// Test mode (environment variable LOCOFIX_TEST=1): mouse input only from the test.
//
// The game takes the mouse position from WM_NCHITTEST (Mouse_HandleMessage, 0x417a10),
// which Windows sends straight to the window procedure on every move of the real
// cursor, and the buttons from WM_LBUTTONDOWN/UP etc. Automated tests
// (tools/test_game.ps1) do not move the real cursor; they send WM_APP+1 to the window
// (wParam = mouse message - its high word the turn of a WM_MOUSEWHEEL -, lParam = x | y<<16
// in the game area). Here:
//  - on WM_APP+1 we set a virtual cursor, send the game WM_NCHITTEST with its position
//    and turn the message into an ordinary mouse message,
//  - messages from the real mouse in the queue are ignored,
//  - the window procedure gets the virtual cursor position in every WM_NCHITTEST,
//  - GetCursorPos/SetCursorPos work on the virtual cursor,
//  - the game stays active: WM_ACTIVATEAPP with FALSE is not passed on (World_HandleMessage
//    clears World flags 0x8 on it, after which GameWorld_Run sleeps and Mouse_HandleMessage
//    ignores the mouse), and WM_ACTIVATEAPP TRUE is posted once when the window is shown, in
//    case it came up behind another one - so a test runs with the game window behind the
//    user's and never has to bring it forward (test_game.ps1 does not).
// A quiet run (LOCOFIX_QUIET=1 next to LOCOFIX_TEST=1, test_game.ps1 without -Visible, which runs
// the game on a Win32 desktop of its own) also has no sound: the sound object's effects and music
// volumes (g_sound +0x3c4 / +0x3c8, 0-100) are held at 0 - SetSoundVolume and SetMusicVolume get 0
// whatever the script asks, and both fields are set to 0 every frame for a run whose scripts never
// set them. At 0 the engine plays no effect (it skips one under -3000 hundredths of a dB, and 0 is
// (0 - 100) * 32 = -3200) and starts no music (PlayMusicFile makes no stream at volume 0), so the
// sound code runs as ever, nothing is taken away, and nothing is written to the registry (the
// scripts keep their volumes with SetReg only on the settings page).
// Outside test mode nothing changes.
#include "patch.h"
#include <ddraw.h>

namespace {

constexpr uintptr_t IAT_GetCursorPos = 0x4751e8;
constexpr uintptr_t IAT_SetCursorPos = 0x47517c;
constexpr uintptr_t IAT_PeekMessageA = 0x475198;
constexpr uintptr_t IAT_RegisterClassA = 0x4751b8;
constexpr UINT WM_TEST_MOUSE = WM_APP + 1;
constexpr uintptr_t G_SOUND = 0x5d3a54;               // g_sound: the sound object
constexpr uintptr_t SET_SOUND_VOLUME = 0x444f90, SET_MUSIC_VOLUME = 0x444fc0;
constexpr int SOUND_VOLUME = 0x3c4, MUSIC_VOLUME = 0x3c8;
bool g_quiet = false;

using SetVolume_t = void(__fastcall*)(void* sound, void* edx, int volume);
SetVolume_t g_set_sound = nullptr, g_set_music = nullptr;
void __fastcall quiet_set_sound(void* sound, void* edx, int) { g_set_sound(sound, edx, 0); }
void __fastcall quiet_set_music(void* sound, void* edx, int) { g_set_music(sound, edx, 0); }

void hold_silence() {
    auto* sound = *reinterpret_cast<uint8_t**>(G_SOUND);
    if (!sound) return;
    *reinterpret_cast<int*>(sound + SOUND_VOLUME) = 0;
    *reinterpret_cast<int*>(sound + MUSIC_VOLUME) = 0;
}

POINT g_cursor = {0, 0};   // screen coordinates
WNDPROC g_game_wndproc = nullptr;

bool g_activated = false;

// test_game.ps1's c40051: the display taken away and given back, as when another program takes the display
// (every surface lost): DirectDraw's mode set to 1280x720 and, 1.5 s later, back to the game area's size.
constexpr UINT TEST_DISPLAY_FLIP = 40051;
constexpr UINT_PTR FLIP_TIMER = 0x7A11;
int g_flip_w = 0, g_flip_h = 0;

IDirectDraw7* game_ddraw() {
    uint8_t* g = *reinterpret_cast<uint8_t**>(0x4a9a14);   // g_graphics
    return g ? *reinterpret_cast<IDirectDraw7**>(g + 0xbec) : nullptr;
}

LRESULT CALLBACK test_wndproc(HWND window, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND && LOWORD(w) == TEST_DISPLAY_FLIP) {
        RECT client;
        GetClientRect(window, &client);
        g_flip_w = client.right;
        g_flip_h = client.bottom;
        IDirectDraw7* dd = game_ddraw();
        const HRESULT hr = dd ? dd->SetDisplayMode(1280, 720, 16, 0, 0) : E_FAIL;
        patch::log("test: display mode 1280x720 (%08lX), back to %dx%d in 1.5 s", static_cast<unsigned long>(hr), g_flip_w, g_flip_h);
        SetTimer(window, FLIP_TIMER, 1500, nullptr);
        return 0;
    }
    if (m == WM_TIMER && w == FLIP_TIMER) {
        KillTimer(window, FLIP_TIMER);
        IDirectDraw7* dd = game_ddraw();
        const HRESULT hr = dd ? dd->SetDisplayMode(g_flip_w, g_flip_h, 16, 0, 0) : E_FAIL;
        patch::log("test: display mode back to %dx%d (%08lX)", g_flip_w, g_flip_h, static_cast<unsigned long>(hr));
        return 0;
    }
    if (m == WM_NCHITTEST) l = MAKELPARAM(g_cursor.x, g_cursor.y);
    if (m == WM_ACTIVATEAPP && !w) return 0;   // the game keeps running behind other windows
    if (m == WM_SHOWWINDOW && w && !g_activated) {
        g_activated = true;
        PostMessageA(window, WM_ACTIVATEAPP, TRUE, 0);
    }
    return CallWindowProcA(g_game_wndproc, window, m, w, l);
}

ATOM WINAPI RegisterClassA_test(const WNDCLASSA* cls) {
    WNDCLASSA copy = *cls;
    g_game_wndproc = copy.lpfnWndProc;
    copy.lpfnWndProc = &test_wndproc;
    return RegisterClassA(&copy);
}

BOOL WINAPI virtual_GetCursorPos(POINT* p) {
    *p = g_cursor;
    return TRUE;
}

BOOL WINAPI virtual_SetCursorPos(int x, int y) {
    g_cursor = {x, y};
    return TRUE;
}

bool is_mouse(UINT message) {
    return message >= WM_MOUSEFIRST && message <= WM_MOUSELAST;
}

BOOL WINAPI PeekMessageA_test(MSG* m, HWND window, UINT first, UINT last, UINT remove) {
    if (g_quiet) hold_silence();
    BOOL got = PeekMessageA(m, window, first, last, remove);
    if (!got) return got;
    if (m->message == WM_TEST_MOUSE && is_mouse(LOWORD(m->wParam))) {
        // the high word carries the turn of a WM_MOUSEWHEEL (test_game.ps1's s step)
        const WPARAM turn = m->wParam & 0xFFFF0000;
        m->message = LOWORD(m->wParam);
        m->wParam = m->message == WM_LBUTTONDOWN ? MK_LBUTTON : m->message == WM_RBUTTONDOWN ? MK_RBUTTON
                  : m->message == WM_MOUSEWHEEL ? turn : 0;
        POINT p = {static_cast<short>(LOWORD(m->lParam)), static_cast<short>(HIWORD(m->lParam))};
        ClientToScreen(m->hwnd, &p);
        g_cursor = p;
        SendMessageA(m->hwnd, WM_NCHITTEST, 0, MAKELPARAM(p.x, p.y));
    } else if (is_mouse(m->message)) {
        m->message = WM_NULL;      // the real mouse has no effect on the test
    }
    return got;
}

void replace_iat(uintptr_t entry, void* function) {
    patch::write(entry, &function, sizeof function);
}

}  // namespace

// editor.cpp's view step (test_game.ps1 V<x>:<y>): the test cursor to the middle of the game area, so
// the engine's scrolling at the screen's edge does not take the view away again towards a cursor
// left at an edge (where the real cursor was at the start, or the last m step's point).
void test_cursor_to_middle(HWND window) {
    if (!g_game_wndproc) return;   // not test mode
    RECT r;
    GetClientRect(window, &r);
    POINT p = {(r.left + r.right) / 2, (r.top + r.bottom) / 2};
    ClientToScreen(window, &p);
    g_cursor = p;
    SendMessageA(window, WM_NCHITTEST, 0, MAKELPARAM(p.x, p.y));
}

void install_test_mode() {
    char value[8] = {};
    if (!GetEnvironmentVariableA("LOCOFIX_TEST", value, sizeof value) || value[0] != '1') return;

    GetCursorPos(&g_cursor);
    replace_iat(IAT_GetCursorPos, reinterpret_cast<void*>(&virtual_GetCursorPos));
    replace_iat(IAT_SetCursorPos, reinterpret_cast<void*>(&virtual_SetCursorPos));
    replace_iat(IAT_PeekMessageA, reinterpret_cast<void*>(&PeekMessageA_test));
    replace_iat(IAT_RegisterClassA, reinterpret_cast<void*>(&RegisterClassA_test));
    patch::log("test mode: mouse input from the test only, the game stays active behind other windows");

    if (!GetEnvironmentVariableA("LOCOFIX_QUIET", value, sizeof value) || value[0] != '1') return;
    // mov eax,[esp+4] / test eax,eax ; push esi / push edi / mov edi,[esp+0xc]
    const uint8_t sound_start[] = {0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0};
    const uint8_t music_start[] = {0x56, 0x57, 0x8B, 0x7C, 0x24, 0x0C};
    g_set_sound = reinterpret_cast<SetVolume_t>(patch::hook(SET_SOUND_VOLUME, sound_start, sizeof sound_start,
                                                            reinterpret_cast<void*>(&quiet_set_sound), "SetSoundVolume"));
    g_set_music = reinterpret_cast<SetVolume_t>(patch::hook(SET_MUSIC_VOLUME, music_start, sizeof music_start,
                                                            reinterpret_cast<void*>(&quiet_set_music), "SetMusicVolume"));
    g_quiet = g_set_sound && g_set_music;
    patch::log(g_quiet ? "test mode: quiet - sound and music at 0" : "test mode: quiet, but the sound could not be held at 0");
}
