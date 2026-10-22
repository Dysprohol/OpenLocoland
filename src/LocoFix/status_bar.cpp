// The state bar across the whole width of the screen.
//
// Player_ShowStateBar (0x42e430) puts the base of the state bar (VID 450, 1024 x 60) in the
// middle of the bottom edge of the view. On a screen wider than 1024 - a big map at
// 1920x1080 - the map shows on both sides of it. VID 938 (game/objects.override) has four
// tiles cut from the bar's own plates by build.py (patch_data.make_status_bar_sides,
// vid\450side.vid), their joins cross-faded: the left end with its seam (direction 0,
// repeated; 64 next to the bar) and the right one (128 next to the bar; 192 repeated). When
// the engine creates the bar, we line them up outward from both ends of the base to the
// edges of the view, on its layer and height (z 2000, under the minimap and the other parts
// of the bar).
//
// The parts of the bar are FRAME objects: Frame_Construct (0x40c4d0) adds the camera to the
// position it is given and puts them on World.menu, whose objects SetPosition moves with the
// camera - so they are created at screen positions (the object's position minus the camera)
// and stay on the screen by themselves. They go with the level like the bar's own objects;
// hiding and showing the bar is done per VID (Vid_SetHidden), so the hooks of
// Player_HideStateBar and Player_ShowStateBar hide and show VID 938 with it.
#include "patch.h"

namespace {

constexpr uintptr_t SHOW_STATE_BAR = 0x42e430;
constexpr uintptr_t HIDE_STATE_BAR = 0x42ef10;
constexpr uintptr_t VID_SET_HIDDEN = 0x44e490;
constexpr uintptr_t OBJECT_SET_DIRECTION = 0x448660;
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr int WORLD_CAMERA_X = 0x44;   // World: top-left of the view on the map (cameraY at +0x48)

constexpr int BAR_VID = 450;
constexpr int SIDE_VID = 938;
constexpr float BAR_WIDTH = 1024.0f;
constexpr float TILE_WIDTH = 106.0f;
// the directions of the four frames of VID 938 (patch_data.make_status_bar_sides)
constexpr int LEFT = 0;           // repeated toward the left edge
constexpr int LEFT_INNER = 64;    // next to the bar, fading into its first columns
constexpr int RIGHT_INNER = 128;  // next to the bar, fading into its last columns
constexpr int RIGHT = 192;        // repeated toward the right edge

using Player_t = void(__fastcall*)(void* player, void* edx);
using CreateObject_t = uint8_t*(__fastcall*)(void* world, void* edx, void* vid, float x, float y,
                                             float z, unsigned direction, void* parent);
using SetInt_t = void(__fastcall*)(void* object, void* edx, int value);

Player_t g_show = nullptr;
Player_t g_hide = nullptr;

template <typename T>
T& at(void* base, int offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }

uint8_t* vid_by_number(int nvid) {
    uint8_t* w = world();
    if (!w || nvid >= at<int>(w, 0x290)) return nullptr;
    return reinterpret_cast<uint8_t**>(w + 0x294)[nvid];
}

void set_hidden(uint8_t* vid, int hidden) {
    reinterpret_cast<SetInt_t>(VID_SET_HIDDEN)(vid, nullptr, hidden);
}

uint8_t* bar_base(void* player) {
    const int count = at<int>(player, 0x18);   // PlayerBase.bar
    uint8_t** items = at<uint8_t**>(player, 0x20);
    for (int i = 0; i < count; ++i)
        if (items[i] && at<int>(at<uint8_t*>(items[i], 0x1c), 4) == BAR_VID) return items[i];
    return nullptr;
}

void add_sides(void* player, uint8_t* side) {
    uint8_t* base = bar_base(player);
    if (!base) return;
    auto* view = reinterpret_cast<float*>(*reinterpret_cast<uint8_t**>(G_GRAPHICS) + 0x220);   // left, right
    // the base's screen position: Frame_Construct adds the camera again
    const float x = at<float>(base, 0x30) - at<float>(world(), WORLD_CAMERA_X);
    const float y = at<float>(base, 0x34) - at<float>(world(), WORLD_CAMERA_X + 4);
    const float z = at<float>(base, 0x38);
    auto create = reinterpret_cast<CreateObject_t>(at<void**>(world(), 0)[0x20 / 4]);
    int tiles = 0;
    auto place = [&](float centre, int direction) {
        uint8_t* tile = create(world(), nullptr, side, centre, y, z, 0, nullptr);
        if (!tile) return;
        if (direction) reinterpret_cast<SetInt_t>(OBJECT_SET_DIRECTION)(tile, nullptr, direction);
        ++tiles;
    };
    // next to the bar the tile that fades into it, further out the repeated one
    for (float left = x - BAR_WIDTH / 2; left > view[0]; left -= TILE_WIDTH)
        place(left - TILE_WIDTH / 2, left == x - BAR_WIDTH / 2 ? LEFT_INNER : LEFT);
    for (float right = x + BAR_WIDTH / 2; right < view[1]; right += TILE_WIDTH)
        place(right + TILE_WIDTH / 2, right == x + BAR_WIDTH / 2 ? RIGHT_INNER : RIGHT);
    patch::log("state bar: %d side tiles for a view of %d-%d", tiles, static_cast<int>(view[0]),
               static_cast<int>(view[1]));
}

void __fastcall show(void* player, void* edx) {
    const bool created = at<int>(player, 0x18) == 0;   // the engine creates the bar now
    g_show(player, edx);
    uint8_t* side = vid_by_number(SIDE_VID);
    if (!side) return;   // objects.res without VID 938
    if (created && at<int>(player, 0x18) > 0) add_sides(player, side);
    set_hidden(side, 0);
}

void __fastcall hide(void* player, void* edx) {
    g_hide(player, edx);
    if (uint8_t* side = vid_by_number(SIDE_VID)) set_hidden(side, 1);
}

}  // namespace

void install_status_bar() {
    // sub esp,0x14 / mov eax,[g_world]
    const uint8_t show_start[] = {0x83, 0xEC, 0x14, 0xA1, 0x38, 0x9D, 0x4A, 0x00};
    // mov eax,[g_world] / push edi / mov edi,ecx
    const uint8_t hide_start[] = {0xA1, 0x38, 0x9D, 0x4A, 0x00, 0x57, 0x8B, 0xF9};
    g_hide = reinterpret_cast<Player_t>(patch::hook(HIDE_STATE_BAR, hide_start, sizeof hide_start,
                                                    reinterpret_cast<void*>(&hide), "Player_HideStateBar"));
    if (!g_hide) return;   // tiles that could not be hidden with the bar: rather none
    g_show = reinterpret_cast<Player_t>(patch::hook(SHOW_STATE_BAR, show_start, sizeof show_start,
                                                    reinterpret_cast<void*>(&show), "Player_ShowStateBar"));
}
