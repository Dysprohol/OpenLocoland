// The display mode list of a device holds 16 modes, and the engine did not stop there (BUGS.md F29).
//
// Graphics_FilterDisplayMode (0x413e70, the EnumDisplayModes callback of Graphics_EnumDevice)
// puts every mode of the card that is in the settings' list into the device's DisplayDevice:
// modeWidth[16] at +0x44, modeHeight[16] at +0x84, modeFormat[16] at +0xc4, then desktopFormat
// (+0x104) and flags (+0x108) - with no check of modeCount. The 17th mode's width lands in the
// first mode's height, its height in the first mode's format, and further modes overwrite the
// desktop's format, the flags ("can render in a window") and the next device. The original list
// had three modes; with LocoFix's widescreen list dgVoodoo matched 17 on a 2560x1440 screen
// (2026-11-11): the start window's first line read "640 x 2560 x 0bpp", and a saved mode the list
// did not have fell back to that first mode - full screen forced (2560 is taller than the screen)
// and "Couldn't set the display mode", the game closed.
//
// Here a mode that finds the list full is not added: the original runs on a scratch device, so it
// still logs the mode as before, and a mode it would have taken is logged as left out. So is a mode
// bigger than the monitor of the device being enumerated (resolutions.cpp; dgVoodoo lists each
// device's modes only up to its monitor anyway - this is a safety net, not the limit).
#include "patch.h"

bool mode_fits_device(int width, int height);   // resolutions.cpp

namespace {

constexpr uintptr_t FILTER = 0x413e70;
constexpr int MODE_COUNT = 0x40;   // DisplayDevice.modeCount
constexpr int MODES_MAX = 16;
constexpr int DEVICE_SIZE = 0x10c;

using Filter_t = int(__stdcall*)(void* surface_desc, uint8_t* device);
Filter_t g_filter = nullptr;

int __stdcall filter_hook(void* surface_desc, uint8_t* device) {
    // DDSURFACEDESC2: dwHeight at +8, dwWidth at +0xc
    const int width = *reinterpret_cast<const int*>(static_cast<uint8_t*>(surface_desc) + 0xc);
    const int height = *reinterpret_cast<const int*>(static_cast<uint8_t*>(surface_desc) + 8);
    const bool full = *reinterpret_cast<int*>(device + MODE_COUNT) >= MODES_MAX;
    const bool fits = mode_fits_device(width, height);   // bigger than the device's monitor: left out too
    if (!full && fits) return g_filter(surface_desc, device);
    alignas(4) uint8_t scratch[DEVICE_SIZE] = {};
    const int result = g_filter(surface_desc, scratch);
    if (*reinterpret_cast<int*>(scratch + MODE_COUNT) != 0) {
        if (!fits) patch::log("display mode %dx%d left out: bigger than the device's monitor", width, height);
        else patch::log("display mode %dx%d left out: the list holds %d modes", width, height, MODES_MAX);
    }
    return result;
}

}  // namespace

void install_mode_list() {
    const uint8_t start[] = {0x53, 0x56, 0x8B, 0x74, 0x24, 0x0C};   // push ebx / push esi / mov esi,[esp+0xc]
    g_filter = reinterpret_cast<Filter_t>(
        patch::hook(FILTER, start, sizeof start, reinterpret_cast<void*>(&filter_hook), "Graphics_FilterDisplayMode"));
}
