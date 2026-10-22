// Objects and shadows across the whole screen at any resolution.
//
// Before drawing, the engine skips objects far from the middle of the screen, using a box
// of fixed size around the screen centre (camera + screen size / 2) - made for the
// original resolutions, at most 1024x768:
// - World_DrawLayer (layers other than 0 and 7): x within +-1024, y - z within +-512,
// - the shadow pass of Graphics_DrawScene (layers 2 and 3, the VID's DrawShadow): x
//   within +-512, y - z within +-512 - at 1920x1080 shadows were drawn only in the
//   middle 1024 pixels of the screen.
// Either way an object whose y is at least the vertical half of the box below the centre
// is drawn too. The test is `(value - centre + half) & ~(2 * half - 1)`, so the halves are
// powers of two built into the code.
//
// We replace both tests with one that takes the halves from the current screen size:
// max(original half, screen half + margin), where the margin is what the original box
// left at 1024x768 (horizontally 512 for objects and 0 for shadows, vertically 128).
// Up to 1024x768 nothing changes. Drawing more is safe - the drawing code clips to the
// view itself; the box only saves time.
#include "patch.h"

namespace {

constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t LAYER_TEST = 0x41faf2;    // in World_DrawLayer, esi = object
constexpr uintptr_t LAYER_DRAW = 0x41fb34;    // mov eax,[esi] / call [eax+0x14]
constexpr uintptr_t LAYER_SKIP = 0x41fb3f;    // dec edi (next object)
constexpr uintptr_t SHADOW_TEST = 0x40f488;   // in Graphics_DrawScene, edi = object
constexpr uintptr_t SHADOW_DRAW = 0x40f4df;   // mov ecx,[edi+0x1c] / call [vid+0x10]
constexpr uintptr_t SHADOW_SKIP = 0x40f4ee;   // dec ebp (next object); ebx = g_world

struct Box {
    int half_x, half_y;       // original halves
    int margin_x, margin_y;   // what they left beyond the screen at 1024x768
    bool truncate_y_z_apart;       // the shadow pass truncates y and z separately, World_DrawLayer y - z
};
constexpr Box OBJECTS = {1024, 512, 512, 128, false};
constexpr Box SHADOWS = {512, 512, 0, 128, true};

int max_int(int a, int b) { return a > b ? a : b; }

bool in_box(const uint8_t* object, const Box& box) {
    const auto* g = *reinterpret_cast<uint8_t* const*>(G_GRAPHICS);
    const auto* world = *reinterpret_cast<uint8_t* const*>(G_WORLD);
    const float screen_w = *reinterpret_cast<const float*>(g + 0x20c);
    const float screen_h = *reinterpret_cast<const float*>(g + 0x210);
    const int centre_x = static_cast<int>(screen_w * 0.5f + *reinterpret_cast<const float*>(world + 0x44));
    const int centre_y = static_cast<int>(screen_h * 0.5f + *reinterpret_cast<const float*>(world + 0x48));
    const int half_x = max_int(box.half_x, static_cast<int>(screen_w) / 2 + box.margin_x);
    const int half_y = max_int(box.half_y, static_cast<int>(screen_h) / 2 + box.margin_y);

    const float* position = reinterpret_cast<const float*>(object + 0x30);   // x, y, z
    const int dx = static_cast<int>(position[0]) - centre_x;
    const int dy = static_cast<int>(position[1]) - centre_y;
    const int dyz = (box.truncate_y_z_apart ? static_cast<int>(position[1]) - static_cast<int>(position[2])
                                       : static_cast<int>(position[1] - position[2])) - centre_y;
    if (dx >= -half_x && dx < half_x && dyz >= -half_y && dyz < half_y) return true;
    return dy >= half_y;
}

int __cdecl object_visible(const uint8_t* object) { return in_box(object, OBJECTS); }
int __cdecl shadow_visible(const uint8_t* object) { return in_box(object, SHADOWS); }

uintptr_t g_layer_draw = LAYER_DRAW, g_layer_skip = LAYER_SKIP;
uintptr_t g_shadow_draw = SHADOW_DRAW, g_shadow_skip = SHADOW_SKIP;

__declspec(naked) void layer_test() {
    __asm {
        pushad
        push esi
        call object_visible
        add esp, 4
        test eax, eax
        popad
        jz skip
        jmp dword ptr [g_layer_draw]
    skip:
        jmp dword ptr [g_layer_skip]
    }
}

__declspec(naked) void shadow_test() {
    __asm {
        pushad
        push edi
        call shadow_visible
        add esp, 4
        test eax, eax
        popad
        mov ebx, dword ptr ds:[0x4a9d38]   // g_world, expected after the test
        jz skip
        jmp dword ptr [g_shadow_draw]
    skip:
        jmp dword ptr [g_shadow_skip]
    }
}

}  // namespace

void install_view_culling() {
    // fld [esi+0x30] / call crt_ftol / sub eax,[esp+0x14] / add eax,0x400 / test eax,0xfffff800
    const uint8_t layer[] = {0xD9, 0x46, 0x30, 0xE8, 0x12, 0x6F, 0x03, 0x00, 0x2B, 0x44, 0x24, 0x14,
                             0x05, 0x00, 0x04, 0x00, 0x00, 0xA9, 0x00, 0xF8, 0xFF, 0xFF};
    // fld [edi+0x30] / call crt_ftol / sub eax,[esp+0x1c] / add eax,0x200 / test eax,0xfffffc00
    const uint8_t shadow[] = {0xD9, 0x47, 0x30, 0xE8, 0x7C, 0x75, 0x04, 0x00, 0x2B, 0x44, 0x24, 0x1C,
                              0x05, 0x00, 0x02, 0x00, 0x00, 0xA9, 0x00, 0xFC, 0xFF, 0xFF};
    if (!patch::check(LAYER_TEST, layer, sizeof layer) || !patch::check(SHADOW_TEST, shadow, sizeof shadow)) {
        patch::log("skipped view culling: unexpected bytes");
        return;
    }
    patch::write_call(LAYER_TEST, nullptr, 0, reinterpret_cast<void*>(&layer_test), 5, true);
    patch::write_call(SHADOW_TEST, nullptr, 0, reinterpret_cast<void*>(&shadow_test), 5, true);
}
