// A region's fog that pulses (docs/dev/changes.md, BUGS.md "Quirks": a region's flag 1).
//
// Region_Draw 0x435770 keeps the top of a region's fog at +0x70, in eighths of a GRID level, and
// hands it to Graphics_DrawFog, which draws full fog below top + (low - high) * 8 fading out up to
// the top. Its flags (+0x7c) as the exe reads them:
//   1  the top is not set to high * 8 every frame: each frame t = g_gameTime & 7, and when t is
//      smaller than the frame before (+0x74: the 3-bit count wrapped) a top below high * 8 grows by
//      2 and one above it becomes 0. So the fog rises once from 0 to its level after the map is
//      loaded and stays - how fast depends on the frames' lengths in ms, not on the time alone -,
//      and nothing ever takes it down again;
//   2  Graphics_DrawFog's source blend ZERO in place of ONE: the fog darkens (l01, l07);
//   8  over the whole view, not the rectangle (also read by Vid_GetTerrainVariant,
//      Object_SpawnChildren and Object_Update for the region's pairs and children).
// No other bit is read anywhere (every access to +0x7c in the exe's listing, the beta's too), no
// map of the game has another one set, and Region_Action loads and saves the whole dword.
//
// The pulse is ours: flag 0x10 together with flag 1 swings the top between low * 8 and high * 8
// and back for as long as the map runs, one swing in 6 s of game time (flag 0x20: 12 s, flag
// 0x40: 3 s). With flag 1 set the original leaves a top alone unless the count wraps, so the hook
// writes the top and a last count of 0 - no count is below 0 - before the original runs and
// changes nothing else: the fog is drawn by the engine as ever. The same map in a game without
// this hook has flag 1 alone: the one rise. The time is the game's clock, so the pulse follows the
// game speed and stands still while the game is paused (World_Pause keeps the time it began at;
// the clock itself runs on and is put back when the pause ends); the editor holds a level by that
// same pause and shows the pulse moving, by the running clock.
//
// LOCOFIX_REGION_LOG=1 writes the top of every region with flag 1 to LocoFix.log as it changes
// (a pulsing one ten times a second) with the game's and the real time - the measurements of the
// rise and of the pulse.
#include "patch.h"
#include <cmath>
#include <map>
#include <mmsystem.h>

bool editor_level_held();   // editor.cpp

namespace {

constexpr uintptr_t REGION_VTBL_DRAW = 0x4759a0;   // Region vtable (0x47598c) slot 5
constexpr uintptr_t REGION_DRAW = 0x435770;
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GAME_TIME = 0x4a9d64;    // ms, advanced by Game_Step
constexpr uintptr_t G_PAUSE_TIME = 0x4a9d28;   // the game time World_Pause began at
constexpr size_t W_FLAGS = 0x04;
constexpr uint32_t WORLD_PAUSED = 0x10;
constexpr size_t O_X = 0x30, O_Y = 0x34;
constexpr size_t RG_TOP = 0x70, RG_COUNT = 0x74, RG_FLAGS = 0x7c, RG_HIGH = 0x80, RG_LOW = 0x84;
constexpr uint32_t RG_RISES = 1, RG_PULSES = 0x10, RG_SLOW = 0x20, RG_FAST = 0x40;
constexpr uint32_t PERIOD = 6000, PERIOD_SLOW = 12000, PERIOD_FAST = 3000;   // ms of game time

using Draw_t = void(__fastcall*)(uint8_t* region, void* edx);
const auto g_original = reinterpret_cast<Draw_t>(REGION_DRAW);
bool g_log = false;

template <class T> T& at(uint8_t* p, size_t offset) { return *reinterpret_cast<T*>(p + offset); }

uint32_t pulse_time() {
    uint8_t* world = *reinterpret_cast<uint8_t**>(G_WORLD);
    const bool paused = world && (at<uint32_t>(world, W_FLAGS) & WORLD_PAUSED) && !editor_level_held();
    return *reinterpret_cast<uint32_t*>(paused ? G_PAUSE_TIME : G_GAME_TIME);
}

// The top of a pulsing fog at a time: low * 8 at the start of a swing, high * 8 in its middle.
int pulse_top(uint32_t flags, int low, int high, uint32_t time) {
    const uint32_t period = flags & RG_SLOW ? PERIOD_SLOW : flags & RG_FAST ? PERIOD_FAST : PERIOD;
    const double phase = static_cast<double>(time % period) / period;
    const double up = (1.0 - std::cos(phase * 6.283185307179586)) / 2;   // 0 .. 1 .. 0
    return low * 8 + static_cast<int>(std::lround((high - low) * 8 * up));
}

struct Logged {
    int top = -1;
    DWORD real = 0;
};
std::map<uint8_t*, Logged> g_logged;

void log_top(uint8_t* region, bool pulses) {
    Logged& l = g_logged[region];
    const int top = at<int>(region, RG_TOP);
    const DWORD real = timeGetTime();
    if (top == l.top || (pulses && l.top >= 0 && real - l.real < 100)) return;
    l.top = top;
    l.real = real;
    patch::log("region %.0f,%.0f flags %u levels %d-%d: top %d at game time %u, real %lu%s", at<float>(region, O_X),
               at<float>(region, O_Y), at<uint32_t>(region, RG_FLAGS), at<int>(region, RG_LOW), at<int>(region, RG_HIGH),
               top, *reinterpret_cast<uint32_t*>(G_GAME_TIME), real, pulses ? " (pulse)" : "");
}

void __fastcall region_draw(uint8_t* region, void* edx) {
    const uint32_t flags = at<uint32_t>(region, RG_FLAGS);
    const int low = at<int>(region, RG_LOW), high = at<int>(region, RG_HIGH);
    const bool pulses = (flags & (RG_RISES | RG_PULSES)) == (RG_RISES | RG_PULSES) && high > low;
    if (pulses) {
        at<int>(region, RG_TOP) = pulse_top(flags, low, high, pulse_time());
        at<uint32_t>(region, RG_COUNT) = 0;
    }
    g_original(region, edx);
    if (g_log && (flags & RG_RISES) && high > low) log_top(region, pulses);
}

}  // namespace

void install_region_pulse() {
    const uintptr_t expected = REGION_DRAW;
    if (!patch::check(REGION_VTBL_DRAW, &expected, sizeof expected)) {
        patch::log("skipped region pulse: unexpected bytes");
        return;
    }
    const uintptr_t hook = reinterpret_cast<uintptr_t>(&region_draw);
    patch::write(REGION_VTBL_DRAW, &hook, sizeof hook);
    char value[8];
    g_log = GetEnvironmentVariableA("LOCOFIX_REGION_LOG", value, sizeof value) && value[0] == '1';
}
