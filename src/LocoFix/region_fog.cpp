// A region's fog table freed twice (BUGS.md F25).
//
// Region_InitFog 0x435c50 (low, high, colour) sets the fog of a region: it stores the three values,
// frees the fog table (+0x78, the mask's values for the levels between low and high) when there is
// one, and makes a new one only when low < high. A fog taken away (low >= high) therefore leaves
// the pointer to the freed table where it was, and the next Region_InitFog - a fog chosen again -
// frees it a second time, as does the region's deletion: the heap is corrupted and the game dies a
// little later somewhere else (in the editor at 0x41FB38, World_DrawLayer reading a layer list that
// was overwritten: FOG: MIST, FOG: NONE - or an undo of the fog -, FOG: MIST). The engine itself
// calls the function once a region, when a map is loaded (Region_Action), so the campaign never
// meets it; the editor's region panel and its undo call it again and again.
//
// The fix: after the original ran with no fog to make, the pointer is cleared.
#include "patch.h"

namespace {

constexpr uintptr_t REGION_INIT_FOG = 0x435c50;   // Region method (low, high, colour)
constexpr size_t RG_FOG_TABLE = 0x78;

using InitFog_t = void(__fastcall*)(uint8_t* region, void* edx, int low, int high, int colour);
InitFog_t g_original = nullptr;

void __fastcall init_fog(uint8_t* region, void* edx, int low, int high, int colour) {
    g_original(region, edx, low, high, colour);
    if (low >= high) *reinterpret_cast<void**>(region + RG_FOG_TABLE) = nullptr;   // freed, none made
}

}  // namespace

void install_region_fog() {
    const uint8_t start[] = {0x8B, 0x44, 0x24, 0x08, 0x8B, 0x54, 0x24, 0x0C};   // mov eax,[esp+8] / mov edx,[esp+0xc]
    g_original = reinterpret_cast<InitFog_t>(
        patch::hook(REGION_INIT_FOG, start, sizeof start, reinterpret_cast<void*>(&init_fog), "Region_InitFog"));
}
