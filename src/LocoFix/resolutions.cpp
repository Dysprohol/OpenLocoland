// Higher resolutions and widescreen.
//
// The launcher only shows modes from the built-in list in the settings struct that
// WinMain passes to World_Construct (0x47b1f0): +0x100 widths, +0x180 heights (32 entries each,
// zero-terminated), +0x200 colour depths. Originally: 640x480, 800x600, 1024x768,
// 16-bit only. The graphics constructor copies this struct to 0x47a8f8 and filters the
// card's modes against it (EnumDisplayModes callback at 0x413e70).
//
// Any size works: on a map smaller than the screen the game runs at a logical screen of
// the map size and the picture is stretched (screen_scaling.cpp).
#include "patch.h"

namespace {

constexpr uintptr_t SETTINGS = 0x47b1f0;
constexpr int CAPACITY = 32;   // entries in the list (the last one must stay zero)

struct Mode { int width, height; };

constexpr Mode EXTRA[] = {
    {1280, 720}, {1280, 800}, {1280, 960}, {1280, 1024}, {1366, 768}, {1440, 900},
    {1600, 900}, {1600, 1200}, {1680, 1050}, {1920, 1080}, {1920, 1200}, {1920, 1440},
    {2560, 1080}, {2560, 1440}, {2560, 1600}, {3440, 1440}, {3840, 2160},
};

void add(Mode m) {
    auto* widths = reinterpret_cast<int*>(SETTINGS + 0x100);
    auto* heights = reinterpret_cast<int*>(SETTINGS + 0x180);
    int n = 0;
    while (n < CAPACITY && widths[n] != 0) {
        if (widths[n] == m.width && heights[n] == m.height) return;
        ++n;
    }
    if (n >= CAPACITY - 1) return;
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
    for (const Mode& m : EXTRA) add(m);
    add({GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)});
}
