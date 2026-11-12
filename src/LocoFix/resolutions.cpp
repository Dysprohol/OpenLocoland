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
// The list keeps only the modes no wider and no taller than the screen in real pixels (the
// primary monitor; real because the game is DPI-aware, dpi_aware.cpp), plus the screen's own
// size, and 16 of them at most - a device's list has room for 16 (BUGS.md F29, mode_list.cpp). A bigger one was offered before and the start window then ticked and greyed the full
// screen box (Launcher_FillOrRead 0x40d660: "the mode is bigger than the screen"); dgVoodoo's
// full screen is a window of the screen's size, and the cursor could not reach the part of the
// game's picture past the screen's height (BUGS.md "Running on current Windows").
//
// The mode saved in the registry (ScreenX, ScreenY; World_Construct reads them into the
// settings at +0x228/+0x22c and Graphics_Construct into the graphics at +0x20c/+0x210) may no
// longer fit - another monitor, another display scale, an older version's list. The start
// window would select it if dgVoodoo lists it, or Graphics_Init would take it as it is when the
// start window is turned off (Locoland.cfg StartDialog=0), and in full screen nothing else
// limits it. So right after Graphics_Construct (its call in World_Construct at 0x41a518) a saved
// mode that does not fit becomes the screen's own size; a window of it is limited further by the
// engine as before (Graphics_Init clamps a window to the screen).
#include "patch.h"

int screen_pixels(int index);   // dpi_aware.cpp

namespace {

constexpr uintptr_t SETTINGS = 0x47b1f0;
constexpr int CAPACITY = 32;   // entries in the list (the last one must stay zero)
constexpr uintptr_t CONSTRUCT_CALL = 0x41a518;   // World_Construct: call Graphics_Construct (ecx = graphics)
constexpr uintptr_t GRAPHICS_CONSTRUCT = 0x4140b0;

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

void add(Mode* list, int& n, Mode m) {
    for (int i = 0; i < n; ++i)
        if (list[i].width == m.width && list[i].height == m.height) return;
    if (n < MODES_MAX) list[n++] = m;
}

using Construct_t = void*(__fastcall*)(void* graphics, void* edx, void* settings);

void* __fastcall construct_graphics(void* graphics, void* edx, void* settings) {
    void* result = reinterpret_cast<Construct_t>(GRAPHICS_CONSTRUCT)(graphics, edx, settings);
    auto* size = reinterpret_cast<float*>(static_cast<uint8_t*>(graphics) + 0x20c);   // width, height
    const Mode s = screen();
    const Mode saved = {static_cast<int>(size[0]), static_cast<int>(size[1])};
    patch::log("saved mode %dx%d, %s", saved.width, saved.height,
               *static_cast<uint32_t*>(graphics) & 0x80 ? "full screen" : "window");
    if (s.width > 0 && s.height > 0 && !fits(saved, s)) {
        patch::log("saved mode %dx%d is bigger than the screen (%dx%d) - the screen's size instead",
                   saved.width, saved.height, s.width, s.height);
        size[0] = static_cast<float>(s.width);
        size[1] = static_cast<float>(s.height);
    }
    return result;
}

}  // namespace

void install_resolutions() {
    const uint8_t expected[] = {0x80, 0x02, 0x00, 0x00, 0x20, 0x03, 0x00, 0x00};   // 640, 800
    if (!patch::check(SETTINGS + 0x100, expected, sizeof expected)) {
        patch::log("skipped resolution list @ %08X: unexpected bytes", static_cast<unsigned>(SETTINGS));
        return;
    }
    auto* widths = reinterpret_cast<int*>(SETTINGS + 0x100);
    auto* heights = reinterpret_cast<int*>(SETTINGS + 0x180);
    const Mode s = screen();
    Mode list[CAPACITY] = {};
    int n = 0;
    // the game's own modes, the screen's own size, then ours - each only when it fits, 16 at
    // most (more would overflow the device's list, BUGS.md F29); 640x480 always, so the list
    // is never empty
    for (int i = 0; i < CAPACITY && widths[i] != 0; ++i)
        if (i == 0 || fits({widths[i], heights[i]}, s)) add(list, n, {widths[i], heights[i]});
    if (s.width > 0 && s.height > 0) add(list, n, s);
    for (const Mode& m : EXTRA)
        if (fits(m, s)) add(list, n, m);
    for (int i = 0; i < CAPACITY; ++i) {
        patch::write(reinterpret_cast<uintptr_t>(&widths[i]), &list[i].width, sizeof list[i].width);
        patch::write(reinterpret_cast<uintptr_t>(&heights[i]), &list[i].height, sizeof list[i].height);
    }
    patch::log("resolutions: %d modes up to the screen's %dx%d", n, s.width, s.height);

    // e8 <rel32 to Graphics_Construct>
    uint8_t call[5] = {0xE8};
    const int32_t rel = static_cast<int32_t>(GRAPHICS_CONSTRUCT - (CONSTRUCT_CALL + 5));
    std::memcpy(call + 1, &rel, 4);
    if (patch::check(CONSTRUCT_CALL, call, sizeof call))
        patch::write_call(CONSTRUCT_CALL, nullptr, 0, reinterpret_cast<void*>(&construct_graphics), sizeof call);
    else
        patch::log("skipped saved mode check @ %08X: unexpected bytes", static_cast<unsigned>(CONSTRUCT_CALL));
}
