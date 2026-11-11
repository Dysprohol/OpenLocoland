// Higher resolutions and widescreen.
//
// The launcher only shows modes from the built-in list in the settings struct that
// WinMain passes to World_Construct (0x47b1f0): +0x100 widths, +0x180 heights (32 entries each,
// zero-terminated), +0x200 colour depths. Originally: 640x480, 800x600, 1024x768,
// 16-bit only. The graphics constructor copies this struct to 0x47a8f8 and filters the
// card's modes against it (EnumDisplayModes callback at 0x413e70).
//
// 16 modes at most: a device's list has room for 16 (BUGS.md F29).
//
// Any size works: on a map smaller than the screen the game runs at a logical screen of
// the map size and the picture is stretched (screen_scaling.cpp).
#include "patch.h"

namespace {

constexpr uintptr_t SETTINGS = 0x47b1f0;
constexpr int CAPACITY = 32;   // entries in the list (the last one must stay zero)

struct Mode { int width, height; };

constexpr int MODES_MAX = 16;   // a device's list holds 16 (DisplayDevice, mode_list.cpp)

// Ours, the most used first: when more fit than the list holds (a 4K screen), the last ones are
// left out. The start window shows them in dgVoodoo's order (by size) whatever the order here.
constexpr Mode EXTRA[] = {
    {1920, 1080}, {2560, 1440}, {1280, 720}, {1600, 900}, {1366, 768}, {1280, 1024},
    {1920, 1200}, {1680, 1050}, {1440, 900}, {1280, 800}, {1600, 1200}, {2560, 1600},
    {3840, 2160}, {3440, 1440}, {2560, 1080}, {1920, 1440}, {1280, 960},
};

void add(Mode m) {
    auto* widths = reinterpret_cast<int*>(SETTINGS + 0x100);
    auto* heights = reinterpret_cast<int*>(SETTINGS + 0x180);
    int n = 0;
    while (n < CAPACITY && widths[n] != 0) {
        if (widths[n] == m.width && heights[n] == m.height) return;
        ++n;
    }
    if (n >= MODES_MAX) return;   // a device's list holds 16 (BUGS.md F29, mode_list.cpp)
    patch::write(reinterpret_cast<uintptr_t>(&widths[n]), &m.width, sizeof m.width);
    patch::write(reinterpret_cast<uintptr_t>(&heights[n]), &m.height, sizeof m.height);
}

}  // namespace

void install_resolutions() {
    const uint8_t expected[] = {0x80, 0x02, 0x00, 0x00, 0x20, 0x03, 0x00, 0x00};   // 640, 800
    if (!patch::check(SETTINGS + 0x100, expected, sizeof expected)) {
        patch::log("skipped resolution list @ %08X: unexpected bytes", static_cast<unsigned>(SETTINGS));
        return;
    }
    add({GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)});   // the screen's own size first
    for (const Mode& m : EXTRA) add(m);
}
