// Camera on a map smaller than the screen.
//
// SetPosition (0x41c680) clamps the camera to [left map edge, right map edge - view
// width] (and the same vertically). When the map is narrower than the view - mission 1
// (1024x768) in widescreen or 2048x1536 maps at 2560x1440 - the range is inverted, the
// second clamp wins and the map ends up at the right and bottom edge of the screen. For
// the duration of this call we widen the map bounds symmetrically to the view size, so
// the camera lands exactly in the middle. Then the bounds go back, so the rest of the
// game (unit movement, clicks) sees the real map.
//
// A level held in editor mode (editor.cpp) gets EDITOR_MARGIN more on every side: the main line
// of a mission goes on past the edges of the map (MAP_RULES.md 4) and the editor's bar covers the
// bottom of the view, so the editor's camera goes that far out.
#include "patch.h"

bool editor_level_held();   // editor.cpp

namespace {

constexpr uintptr_t SET_POSITION = 0x41c680;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr float EDITOR_MARGIN = 300;

// thiscall: ecx = world object, x, y, mode on the stack; callee cleans up (like fastcall)
using SetPosition_t = void(__fastcall*)(void* world, void* edx, float x, float y, int mode);
SetPosition_t g_original = nullptr;

void __fastcall SetPosition_centered(void* world, void* edx, float x, float y, int mode) {
    auto* bounds = reinterpret_cast<float*>(static_cast<uint8_t*>(world) + 0x34);   // minX, maxX, minY, maxY
    auto* graphics = *reinterpret_cast<uint8_t**>(G_GRAPHICS);
    auto* view = reinterpret_cast<float*>(graphics + 0x220);                        // left, right, top, bottom

    const float saved[4] = {bounds[0], bounds[1], bounds[2], bounds[3]};
    for (int axis = 0; axis < 2; ++axis) {
        float& lo = bounds[axis * 2];
        float& hi = bounds[axis * 2 + 1];
        const float missing = (view[axis * 2 + 1] - view[axis * 2]) - (hi - lo);
        if (missing > 0) {
            lo -= missing / 2;
            hi += missing / 2;
        }
        if (editor_level_held()) {
            lo -= EDITOR_MARGIN;
            hi += EDITOR_MARGIN;
        }
    }
    g_original(world, edx, x, y, mode);
    for (int i = 0; i < 4; ++i) bounds[i] = saved[i];
}

}  // namespace

void install_camera() {
    // sub esp,8 / push esi / push edi / mov edi,[g_graphics]
    const uint8_t prologue[] = {0x83, 0xEC, 0x08, 0x56, 0x57, 0x8B, 0x3D, 0x14, 0x9A, 0x4A, 0x00};
    g_original = reinterpret_cast<SetPosition_t>(
        patch::hook(SET_POSITION, prologue, sizeof prologue, reinterpret_cast<void*>(&SetPosition_centered),
                    "SetPosition"));
}
