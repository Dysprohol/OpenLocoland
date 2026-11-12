// Higher resolutions and widescreen - only those that fit the screen.
//
// The launcher only shows modes from the built-in list in the settings struct that
// WinMain passes to World_Construct (0x47b1f0): +0x100 widths, +0x180 heights (32 entries each,
// zero-terminated), +0x200 colour depths. Originally: 640x480, 800x600, 1024x768,
// 16-bit only. The graphics constructor copies this struct to 0x47a8f8 and filters the
// card's modes against it (EnumDisplayModes callback at 0x413e70).
//
// Any size works: on a map smaller than the screen the game runs at a logical screen of
// the map size and the picture is stretched (screen_scaling.cpp).
//
// The list keeps only the modes no wider and no taller than a monitor in real pixels (real because
// the game is DPI-aware, dpi_aware.cpp), plus each monitor's own size. A device's list has room
// for 16 (BUGS.md F30, mode_list.cpp), and with monitors of different sizes one list of 16 for all
// did not serve them: with a 4K TV next to a 2560x1440 screen, two of the
// 16 were modes only the TV has and the primary got 14. So the settings' list (room for 31) keeps the
// modes in the order of their worth - the game's own, each monitor's size, ours by use - and each
// device takes the first 16 of them that fit its monitor. DirectDrawEnumerateExA lists every
// monitor as a device (Graphics_EnumDevice 0x4142e0 gets its HMONITOR; device 0 is the primary
// display driver, without one) and dgVoodoo lists each device's modes only up to its monitor's size
// (seen 2026-11-12: 2560x1440 for DISPLAY2, 1920x1080 for DISPLAY1), so the list is measured against
// the biggest of them, and each device against its own monitor: its modes (mode_list.cpp, a safety
// net under dgVoodoo's own limit), the start window's test of the chosen mode (the two GetSystemMetrics
// calls of Launcher_FillOrRead at 0x40d985 / 0x40d9a2 get the device's monitor) and the saved mode's
// check below. The start window shows no device list (patch_exe.py moved it out), so the device is
// the registry's Device, 0 unless set by hand. A bigger one was offered before and the start window then ticked and greyed the full
// screen box (Launcher_FillOrRead 0x40d660: "the mode is bigger than the screen"); dgVoodoo's
// full screen is a window of the screen's size, and the cursor could not reach the part of the
// game's picture past the screen's height (BUGS.md "Running on current Windows").
//
// The mode saved in the registry (ScreenX, ScreenY; World_Construct reads them into the
// settings at +0x228/+0x22c and Graphics_Construct into the graphics at +0x20c/+0x210) may no
// longer fit - another monitor, another display scale, an older version's list, a remote desktop
// session. The start window would select it if dgVoodoo lists it, or Graphics_Init would take it as
// it is when the start window is skipped (Locoland.cfg StartDialog=0, a restart by restart.cpp), and
// in full screen nothing else limits it. So right after Graphics_Construct (its call in
// World_Construct at 0x41a518) a saved mode that does not fit is replaced, for this run, by the
// largest mode of the device's list that fits - one dgVoodoo can set (the screen's raw size may not
// be one of its modes, e.g. a remote desktop's 1916x1036). Graphics_Init writes the mode it starts
// with to the registry (ScreenX, ScreenY); when that is still the replacement - the player did not
// choose another in the start window - the saved one is written back right after it (the call at
// 0x41a887), so one start on a smaller screen does not lower the player's choice for good.
#include "patch.h"
#include <utility>

int screen_pixels(int index);   // dpi_aware.cpp

namespace {

constexpr uintptr_t SETTINGS = 0x47b1f0;
constexpr int CAPACITY = 32;   // entries in the list (the last one must stay zero)
constexpr uintptr_t CONSTRUCT_CALL = 0x41a518;   // World_Construct: call Graphics_Construct (ecx = graphics)
constexpr uintptr_t GRAPHICS_CONSTRUCT = 0x4140b0;
constexpr uintptr_t INIT_CALL = 0x41a887;   // World_Construct: call Graphics_Init (ecx = graphics, push hwnd)
constexpr uintptr_t GRAPHICS_INIT = 0x40dc10;
constexpr char REG_KEY[] = "SOFTWARE\\Gromada\\Locoland";   // RegPath in Locoland.cfg
constexpr int DEVICES = 0x234, DEVICE_SIZE = 0x10c, DEVICE = 0xa94;   // Graphics.devices[8], Graphics.device
constexpr int MODE_COUNT = 0x40, MODE_WIDTH = 0x44, MODE_HEIGHT = 0x84;   // DisplayDevice
constexpr uintptr_t ENUM_DEVICE = 0x4142e0;     // Graphics_EnumDevice, the DirectDrawEnumerateExA callback
constexpr uintptr_t G_DEVICE_COUNT = 0x4a9c00;  // g_ddDeviceCount: the slot the callback fills
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t IAT_GetSystemMetrics = 0x475160;
constexpr uintptr_t LAUNCHER_SCREEN_WIDTH = 0x40d985;    // call [GetSystemMetrics] (SM_CXSCREEN)
constexpr uintptr_t LAUNCHER_SCREEN_HEIGHT = 0x40d9a2;   // call [GetSystemMetrics] (SM_CYSCREEN)

struct Mode { int width, height; };

constexpr int MODES_MAX = 16;   // a device's list holds 16 (DisplayDevice, mode_list.cpp)

// Ours, the most used first: when more fit than the list holds (a 4K screen), the last ones are
// left out. The start window shows them in dgVoodoo's order (by size) whatever the order here.
constexpr Mode EXTRA[] = {
    {1920, 1080}, {2560, 1440}, {1280, 720}, {1600, 900}, {1366, 768}, {1280, 1024},
    {1920, 1200}, {1680, 1050}, {1440, 900}, {1280, 800}, {1600, 1200}, {2560, 1600},
    {3840, 2160}, {3440, 1440}, {2560, 1080}, {1920, 1440}, {1280, 960},
};

Mode screen() { return {screen_pixels(SM_CXSCREEN), screen_pixels(SM_CYSCREEN)}; }

bool fits(Mode m, Mode s) { return m.width <= s.width && m.height <= s.height; }

// A monitor's size as the game sees it; the primary through screen() (a test screen stands for it).
Mode monitor_size(HMONITOR monitor) {
    MONITORINFO mi = {sizeof mi};
    if (!monitor || !GetMonitorInfoA(monitor, &mi) || (mi.dwFlags & MONITORINFOF_PRIMARY)) return screen();
    return {mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top};
}

Mode g_device_screen[8] = {};   // each device's monitor, by its slot in Graphics.devices
Mode g_enumerating = {};        // the monitor of the device being enumerated, {0, 0} outside

using EnumDevice_t = BOOL(WINAPI*)(GUID* guid, char* description, char* name, void* devices, HMONITOR monitor);
EnumDevice_t g_enum_device = nullptr;

BOOL WINAPI enum_device(GUID* guid, char* description, char* name, void* devices, HMONITOR monitor) {
    const int slot = *reinterpret_cast<const int*>(G_DEVICE_COUNT);
    const Mode s = monitor_size(monitor);
    if (slot >= 0 && slot < 8) g_device_screen[slot] = s;
    g_enumerating = s;
    const BOOL result = g_enum_device(guid, description, name, devices, monitor);
    g_enumerating = {};
    patch::log("display device %d (%s): monitor %dx%d", slot, description ? description : "?", s.width, s.height);
    return result;
}

// The monitor of a device of the graphics; the primary when not known.
Mode device_screen(int device) {
    if (device >= 0 && device < 8 && g_device_screen[device].width > 0) return g_device_screen[device];
    return screen();
}

// The start window's test "is the mode bigger than the screen": the chosen device's monitor.
int WINAPI launcher_screen_metric(int index) {
    const auto* g = *reinterpret_cast<const uint8_t* const*>(G_GRAPHICS);
    const Mode s = device_screen(g ? *reinterpret_cast<const int*>(g + DEVICE) : 0);
    return index == SM_CXSCREEN ? s.width : index == SM_CYSCREEN ? s.height : screen_pixels(index);
}

void add(Mode* list, int& n, Mode m) {
    for (int i = 0; i < n; ++i)
        if (list[i].width == m.width && list[i].height == m.height) return;
    if (n < CAPACITY - 1) list[n++] = m;
}

using Construct_t = void*(__fastcall*)(void* graphics, void* edx, void* settings);
using Init_t = int(__fastcall*)(void* graphics, void* edx, HWND window);

struct Replaced { bool on; Mode saved, used; } g_replaced = {};

// The largest mode of the graphics' current device that fits `s`; {0, 0} when none does.
Mode largest_listed(const uint8_t* graphics, Mode s) {
    int device = *reinterpret_cast<const int*>(graphics + DEVICE);
    if (device < 0 || device > 7) device = 0;
    const uint8_t* d = graphics + DEVICES + device * DEVICE_SIZE;
    const int count = *reinterpret_cast<const int*>(d + MODE_COUNT);
    Mode best = {0, 0};
    for (int i = 0; i < count && i < MODES_MAX; ++i) {
        const Mode m = {reinterpret_cast<const int*>(d + MODE_WIDTH)[i], reinterpret_cast<const int*>(d + MODE_HEIGHT)[i]};
        if (fits(m, s) && m.width * m.height > best.width * best.height) best = m;
    }
    return best;
}

void* __fastcall construct_graphics(void* graphics, void* edx, void* settings) {
    void* result = reinterpret_cast<Construct_t>(GRAPHICS_CONSTRUCT)(graphics, edx, settings);
    auto* g = static_cast<uint8_t*>(graphics);
    auto* size = reinterpret_cast<float*>(g + 0x20c);   // width, height
    const Mode s = device_screen(*reinterpret_cast<const int*>(g + DEVICE));
    const Mode saved = {static_cast<int>(size[0]), static_cast<int>(size[1])};
    patch::log("saved mode %dx%d, %s", saved.width, saved.height,
               *static_cast<uint32_t*>(graphics) & 0x80 ? "full screen" : "window");
    if (s.width > 0 && s.height > 0 && !fits(saved, s)) {
        Mode used = largest_listed(g, s);
        if (used.width == 0) used = s;
        patch::log("saved mode %dx%d is bigger than the screen (%dx%d) - %dx%d for this run, the saved one stays",
                   saved.width, saved.height, s.width, s.height, used.width, used.height);
        size[0] = static_cast<float>(used.width);
        size[1] = static_cast<float>(used.height);
        g_replaced = {true, saved, used};
    }
    return result;
}

// After Graphics_Init has saved the mode it starts with: the player's own choice back, unless the
// start window chose another one.
int __fastcall init_graphics(void* graphics, void* edx, HWND window) {
    const int result = reinterpret_cast<Init_t>(GRAPHICS_INIT)(graphics, edx, window);
    const auto* size = reinterpret_cast<const float*>(static_cast<uint8_t*>(graphics) + 0x20c);
    if (g_replaced.on && static_cast<int>(size[0]) == g_replaced.used.width &&
        static_cast<int>(size[1]) == g_replaced.used.height) {
        HKEY key;
        if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) == ERROR_SUCCESS) {
            const DWORD w = g_replaced.saved.width, h = g_replaced.saved.height;
            RegSetValueExA(key, "ScreenX", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&w), sizeof w);
            RegSetValueExA(key, "ScreenY", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&h), sizeof h);
            RegCloseKey(key);
            patch::log("saved mode %dx%d kept in the registry", g_replaced.saved.width, g_replaced.saved.height);
        }
    } else if (g_replaced.on) {
        patch::log("the start window chose %dx%d - saved", static_cast<int>(size[0]), static_cast<int>(size[1]));
    }
    return result;
}

bool redirect_call(uintptr_t at, uintptr_t to, void* hook, const char* name) {
    uint8_t call[5] = {0xE8};
    const int32_t rel = static_cast<int32_t>(to - (at + 5));
    std::memcpy(call + 1, &rel, 4);
    if (!patch::check(at, call, sizeof call)) {
        patch::log("skipped %s @ %08X: unexpected bytes", name, static_cast<unsigned>(at));
        return false;
    }
    patch::write_call(at, nullptr, 0, hook, sizeof call);
    return true;
}

}  // namespace

// For mode_list.cpp: why the device being enumerated does not take a mode of the settings' list -
// nullptr when it does: bigger than the device's monitor, or not one of the first 16 of the list
// (in its order) that fit that monitor.
const char* mode_refused_for_device(int width, int height) {
    if (g_enumerating.width == 0) return nullptr;
    if (!fits({width, height}, g_enumerating)) return "bigger than the device's monitor";
    const auto* widths = reinterpret_cast<const int*>(SETTINGS + 0x100);
    const auto* heights = reinterpret_cast<const int*>(SETTINGS + 0x180);
    int rank = 0;
    for (int i = 0; i < CAPACITY && widths[i] != 0; ++i) {
        if (!fits({widths[i], heights[i]}, g_enumerating)) continue;
        if (widths[i] == width && heights[i] == height) return rank < MODES_MAX ? nullptr : "not one of the device's 16";
        ++rank;
    }
    return nullptr;   // not on the list: the engine leaves it out itself
}

void install_resolutions() {
    const uint8_t expected[] = {0x80, 0x02, 0x00, 0x00, 0x20, 0x03, 0x00, 0x00};   // 640, 800
    if (!patch::check(SETTINGS + 0x100, expected, sizeof expected)) {
        patch::log("skipped resolution list @ %08X: unexpected bytes", static_cast<unsigned>(SETTINGS));
        return;
    }
    auto* widths = reinterpret_cast<int*>(SETTINGS + 0x100);
    auto* heights = reinterpret_cast<int*>(SETTINGS + 0x180);
    // every monitor, the primary first
    Mode monitors[8] = {screen()};
    int count = 1;
    std::pair<Mode*, int*> all(monitors, &count);
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR m, HDC, RECT*, LPARAM data) -> BOOL {
        auto* all = reinterpret_cast<std::pair<Mode*, int*>*>(data);
        MONITORINFO mi = {sizeof mi};
        if (GetMonitorInfoA(m, &mi) && !(mi.dwFlags & MONITORINFOF_PRIMARY) && *all->second < 8)
            all->first[(*all->second)++] = monitor_size(m);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&all));
    auto fits_any = [&](Mode m) {
        for (int i = 0; i < count; ++i)
            if (fits(m, monitors[i])) return true;
        return false;
    };
    const Mode s = monitors[0];
    Mode list[CAPACITY] = {};
    int n = 0;
    // the game's own modes, each monitor's own size, then ours - each only when it fits a
    // monitor, in this order of worth (each device takes the first 16 that fit it, BUGS.md F30);
    // 640x480 always, so the list is never empty
    for (int i = 0; i < CAPACITY && widths[i] != 0; ++i)
        if (i == 0 || fits_any({widths[i], heights[i]})) add(list, n, {widths[i], heights[i]});
    for (int i = 0; i < count; ++i)
        if (monitors[i].width > 0 && monitors[i].height > 0) add(list, n, monitors[i]);
    for (const Mode& m : EXTRA)
        if (fits_any(m)) add(list, n, m);
    for (int i = 0; i < CAPACITY; ++i) {
        patch::write(reinterpret_cast<uintptr_t>(&widths[i]), &list[i].width, sizeof list[i].width);
        patch::write(reinterpret_cast<uintptr_t>(&heights[i]), &list[i].height, sizeof list[i].height);
    }
    patch::log("resolutions: %d modes up to the screen's %dx%d (%d monitor%s)", n, s.width, s.height, count,
               count == 1 ? "" : "s");

    const uint8_t enum_start[] = {0x81, 0xEC, 0xFC, 0x01, 0x00, 0x00};   // sub esp,0x1fc
    g_enum_device = reinterpret_cast<EnumDevice_t>(
        patch::hook(ENUM_DEVICE, enum_start, sizeof enum_start, reinterpret_cast<void*>(&enum_device), "Graphics_EnumDevice"));
    if (g_enum_device) {
        patch::redirect_iat_call(LAUNCHER_SCREEN_WIDTH, IAT_GetSystemMetrics, reinterpret_cast<void*>(&launcher_screen_metric),
                                 "GetSystemMetrics (start window, width)");
        patch::redirect_iat_call(LAUNCHER_SCREEN_HEIGHT, IAT_GetSystemMetrics, reinterpret_cast<void*>(&launcher_screen_metric),
                                 "GetSystemMetrics (start window, height)");
    }

    if (redirect_call(CONSTRUCT_CALL, GRAPHICS_CONSTRUCT, reinterpret_cast<void*>(&construct_graphics), "saved mode check"))
        redirect_call(INIT_CALL, GRAPHICS_INIT, reinterpret_cast<void*>(&init_graphics), "saved mode kept");
}
