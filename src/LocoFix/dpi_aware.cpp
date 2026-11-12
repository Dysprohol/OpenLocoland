// The game works in real screen pixels at any Windows display scale.
//
// The exe has no manifest and never declares itself DPI-aware, and dgVoodoo's DDraw.dll does not
// either (a running game reported "unaware", 2026-11-11). For such a process Windows scales
// everything: at 150 % a 1920x1080 screen is 1280x720 to the game (GetSystemMetrics, window and
// cursor coordinates) and the game's window is enlarged as a picture. So the start window's test
// "the mode is bigger than the screen" (Launcher_FillOrRead 0x40d660), which ticks and greys the
// full screen box, already failed for the default 1024x768 at 150 % (768 > 720), and the modes the
// list offers (resolutions.cpp, dgVoodoo's real ones) were measured against a smaller screen than
// the one the game then filled - a full screen whose lower part the cursor could not reach
// (BUGS.md "Running on current Windows").
//
// Here, first thing when LocoFix loads (before WinMain, so before any window of the game and
// before DirectDraw starts), the process becomes per-monitor DPI-aware: GetSystemMetrics, window
// sizes and the cursor are in real pixels everywhere, for the engine, dgVoodoo and LocoFix alike.
// Windows 10 1703 and later; older systems get the older calls (per monitor on 8.1, system-wide
// on Vista/7).
//
// The start window stays as it looked: it is a dialog of 2003 with a picture of fixed pixels
// (START_BITMAP, 256x256), which an aware dialog would leave at its size while the controls around
// it grow with the scale. So it is opened with the thread's DPI awareness set to "unaware" for
// the call (Windows 10 1607 and later; without it the dialog is simply aware) and Windows scales
// it as a picture, as before. Inside it Windows gives unaware numbers again, so the start window's
// own test of the screen size gets the real pixels from screen_pixels() (its two GetSystemMetrics
// calls at 0x40d985 and 0x40d9a2).
#include "patch.h"
#include <cstdio>

namespace {

constexpr uintptr_t IAT_DialogBoxParamA = 0x4751ac;
constexpr uintptr_t IAT_GetSystemMetrics = 0x475160;
constexpr uintptr_t LAUNCHER_SCREEN_WIDTH = 0x40d985;    // call [GetSystemMetrics] (SM_CXSCREEN)
constexpr uintptr_t LAUNCHER_SCREEN_HEIGHT = 0x40d9a2;   // call [GetSystemMetrics] (SM_CYSCREEN)

// DPI_AWARENESS_CONTEXT values (windef.h, Windows 10)
const HANDLE CONTEXT_UNAWARE = reinterpret_cast<HANDLE>(-1);
const HANDLE CONTEXT_PER_MONITOR_V2 = reinterpret_cast<HANDLE>(-4);

using SetProcessDpiAwarenessContext_t = BOOL(WINAPI*)(HANDLE);
using SetThreadDpiAwarenessContext_t = HANDLE(WINAPI*)(HANDLE);
using SetProcessDpiAwareness_t = HRESULT(WINAPI*)(int);   // shcore.dll, Windows 8.1
using SetProcessDPIAware_t = BOOL(WINAPI*)();
using DialogBoxParamA_t = INT_PTR(WINAPI*)(HINSTANCE, LPCSTR, HWND, DLGPROC, LPARAM);

SetThreadDpiAwarenessContext_t g_set_thread_context = nullptr;
DialogBoxParamA_t g_dialog_box = nullptr;   // update_check.cpp's hook
bool g_aware = false;

FARPROC user32(const char* name) { return GetProcAddress(GetModuleHandleA("user32.dll"), name); }

INT_PTR WINAPI DialogBoxParamA_unaware(HINSTANCE instance, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param) {
    const bool launcher = !IS_INTRESOURCE(name) && lstrcmpiA(name, "START_DIALOG") == 0;
    HANDLE old = launcher && g_set_thread_context ? g_set_thread_context(CONTEXT_UNAWARE) : nullptr;
    const INT_PTR result = g_dialog_box(instance, name, parent, proc, param);
    if (old) g_set_thread_context(old);
    return result;
}

}  // namespace

// The screen's size in real pixels whatever the calling thread's DPI awareness (the start
// window's thread is unaware while it is open); SM_CXSCREEN / SM_CYSCREEN, the primary monitor.
int screen_pixels(int index) {
    // test mode only: LOCOFIX_TEST_SCREEN=<w>x<h> stands for a smaller screen (test_game.ps1 -TestScreen)
    static int fake[2] = {-1, -1};
    if (fake[0] < 0) {
        fake[0] = fake[1] = 0;
        char test[8] = {}, size[32] = {};
        if (GetEnvironmentVariableA("LOCOFIX_TEST", test, sizeof test) && test[0] == '1' &&
            GetEnvironmentVariableA("LOCOFIX_TEST_SCREEN", size, sizeof size) &&
            std::sscanf(size, "%dx%d", &fake[0], &fake[1]) != 2)
            fake[0] = fake[1] = 0;
        if (fake[0] > 0) patch::log("test mode: the screen taken as %dx%d (LOCOFIX_TEST_SCREEN)", fake[0], fake[1]);
    }
    if (fake[0] > 0 && (index == SM_CXSCREEN || index == SM_CYSCREEN)) return index == SM_CXSCREEN ? fake[0] : fake[1];
    if (!g_aware || !g_set_thread_context) return GetSystemMetrics(index);
    HANDLE old = g_set_thread_context(CONTEXT_PER_MONITOR_V2);
    const int size = GetSystemMetrics(index);
    if (old) g_set_thread_context(old);
    return size;
}

namespace {

int WINAPI launcher_screen_metric(int index) { return screen_pixels(index); }

}  // namespace

// First thing after the exe is recognised, before anything reads a size of the screen.
void install_dpi_awareness() {
    g_set_thread_context = reinterpret_cast<SetThreadDpiAwarenessContext_t>(user32("SetThreadDpiAwarenessContext"));
    const char* how = nullptr;
    if (auto set = reinterpret_cast<SetProcessDpiAwarenessContext_t>(user32("SetProcessDpiAwarenessContext"))) {
        if (set(CONTEXT_PER_MONITOR_V2)) how = "per monitor v2";
        else patch::log("DPI awareness: SetProcessDpiAwarenessContext failed (error %lu)", GetLastError());
    }
    if (!how) {
        if (HMODULE shcore = LoadLibraryA("shcore.dll")) {
            auto set = reinterpret_cast<SetProcessDpiAwareness_t>(GetProcAddress(shcore, "SetProcessDpiAwareness"));
            if (set && SUCCEEDED(set(2))) how = "per monitor";   // PROCESS_PER_MONITOR_DPI_AWARE
        }
    }
    if (!how) {
        auto set = reinterpret_cast<SetProcessDPIAware_t>(user32("SetProcessDPIAware"));
        if (set && set()) how = "system";
    }
    g_aware = how != nullptr;
    if (g_aware)
        patch::log("DPI awareness: %s, screen %dx%d pixels", how, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
    else
        patch::log("DPI awareness: not set - Windows display scale applies");
}

// Last of the DialogBoxParamA hooks (after update_check.cpp), so the start window and all the
// hooks' additions to it are created with the thread unaware.
void install_dpi_launcher() {
    if (!g_aware) return;
    patch::redirect_iat_call(LAUNCHER_SCREEN_WIDTH, IAT_GetSystemMetrics, reinterpret_cast<void*>(&launcher_screen_metric),
                             "GetSystemMetrics (start window, width)");
    patch::redirect_iat_call(LAUNCHER_SCREEN_HEIGHT, IAT_GetSystemMetrics, reinterpret_cast<void*>(&launcher_screen_metric),
                             "GetSystemMetrics (start window, height)");
    g_dialog_box = *reinterpret_cast<DialogBoxParamA_t*>(IAT_DialogBoxParamA);
    void* dialog_box = reinterpret_cast<void*>(&DialogBoxParamA_unaware);
    patch::write(IAT_DialogBoxParamA, &dialog_box, sizeof dialog_box);
}
