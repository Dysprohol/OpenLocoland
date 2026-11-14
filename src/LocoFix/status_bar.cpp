// The state bar: Player_ShowStateBar in C++, at the interface's scale, and the bar across the whole
// width of the screen.
//
// Player_ShowStateBar (0x42e430, Player vtable slot 7) shows the message fonts and the message click
// strip, and when the bar has no objects yet (the start of a mission, the way back from the Esc menu,
// which gives the bar up) creates its 80 objects in the order the rest of the engine indexes them
// (Player_SetPanel, Player_UpdateStateBar, Player_HandleInput, minimap.cpp, depot_rows.cpp):
//    0-9   the slot numbers (VID 4, frames 0x31-0x3a)       10-19 the slots' texts (font VID 8)
//    20    the marker under the flagman's slot (VID 456)      21    the round button left (VID 498)
//    22    the button "up" (VID 499)                          23-32 the slots' type icons (VID 455)
//    33-42 build lines (463)  43-52 hp lines (457)  53-62 ammo lines (458)  63-72 behaviour (459)
//    73    the minimap's frame (VID 464)                       74    the bar's base (VID 450)
//    75    the money plate (VID 469)                           76-77 the speed and money texts (font VID 2)
//    78-79 the plates' mouse tip boxes (VIDs 502, 503)
// then marks them invisible when the human does not play the bar's army, sets the panel (the train's
// cars when the flagman is an engine) and limits the camera: the map, and the bar's height less 2 px
// below it (the bar's base hides none of the map).
//
// The positions (screen coordinates; the objects are FRAMEs, which add the camera): the slots around
// the middle of the screen, x0 = width / 2 - 225 and y0 = the view's bottom - the base's height / 2
// - 2, slot i (1-10) at x0 + 41 i; the base in the middle of the bottom edge, the minimap's frame in
// the bottom right corner and the money plate in the top right one by their pictures and boxes, the
// texts from the view's right edge. Here every fixed offset is times the interface's scale s
// (interface_scale.cpp) and the objects are of the scale's bigger VIDs: at s = 1 the same VIDs at the
// same places as the engine's (the same float sums, the same order, the same depths z).
//
// The sides: on a screen wider than the base the map would show on both sides of it. VID 938
// (game/objects.override) has four tiles cut from the bar's own plates by build.py
// (patch_data.make_status_bar_sides, vid\450side.vid), their joins cross-faded: the left end with its
// seam (direction 0, repeated; 64 next to the bar) and the right one (128 next to the bar; 192
// repeated). They are lined up outward from both ends of the base to the edges of the view, on its
// layer and height (z 2000, under the minimap and the other parts of the bar). They go with the level
// like the bar's own objects; hiding and showing the bar is done per VID (Vid_SetHidden), so the hook
// of Player_HideStateBar and this one hide and show VID 938 (and its bigger copies) with it.
#include "patch.h"

void* minimap_frame_for_screen();          // minimap.cpp: the minimap's frame for the screen's size
void minimap_frame_back(void* original);
void minimap_bounds(void* player);         // the camera may go below the map by a bigger frame's height
int interface_choose_scale();              // interface_scale.cpp
int interface_scale();
void* zoom_view_layout_begin();            // zoom_view.cpp: the bar laid out for the window under a zoom
void zoom_view_layout_end(void* token);
double zoom_view_room();                   // map pixels per window pixel of the bar's height
uint8_t* interface_vid(int nvid, int scale);
void interface_copy_colours(int scale);
void interface_set_hidden(int nvid, int hidden);
void install_interface_scale();
void interface_cursor_scale(int scale);    // interface_texts.cpp: the cursors at the bar's scale while it is up

namespace {

constexpr uintptr_t SHOW_STATE_BAR = 0x42e430;
constexpr uintptr_t HIDE_STATE_BAR = 0x42ef10;
constexpr uintptr_t SET_PANEL = 0x42f210;            // Player_SetPanel (depot_rows.cpp hooks it)
constexpr uintptr_t VID_SET_HIDDEN = 0x44e490;
constexpr uintptr_t OBJECT_SET_DIRECTION = 0x448660;
constexpr uintptr_t OBJECT_SET_ARMY = 0x448550;
constexpr uintptr_t OBJECT_HIDE_CHAIN = 0x432530;
constexpr uintptr_t OBJECTLIST_ADD = 0x44b970;
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t G_EMPTY_VID = 0x5d3a60;
constexpr int WORLD_CAMERA_X = 0x44;   // World: top-left of the view on the map (cameraY at +0x48)
constexpr int WORLD_ARMY = 0x21c;      // the army of the local player
constexpr int OBJECT_INVISIBLE = 0x8000;
constexpr int B_ENGINE = 0x15;         // spriteClass of an engine: the panel of its cars
constexpr int B_DEPO = 0x18;           // spriteClass of a factory: its build list (panel 2) and queue (5)
constexpr int WORLD_MENU = 0x24c;      // World.menu: every FRAME object (count +4, items +0xc)

constexpr int BAR_VID = 450;
constexpr int SIDE_VID = 938;
constexpr float BAR_WIDTH = 1024.0f;
constexpr float TILE_WIDTH = 106.0f;
// the directions of the four frames of VID 938 (patch_data.make_status_bar_sides)
constexpr int LEFT = 0;           // repeated toward the left edge
constexpr int LEFT_INNER = 64;    // next to the bar, fading into its first columns
constexpr int RIGHT_INNER = 128;  // next to the bar, fading into its last columns
constexpr int RIGHT = 192;        // repeated toward the right edge
constexpr int SLOTS = 10;
constexpr int SLOT_PITCH = 41;

using Player_t = void(__fastcall*)(void* player, void* edx);
using CreateObject_t = uint8_t*(__fastcall*)(void* world, void* edx, void* vid, float x, float y,
                                             float z, unsigned direction, void* parent);
using SetInt_t = void(__fastcall*)(void* object, void* edx, int value);
using Object_t = void(__fastcall*)(void* object, void* edx);
using ObjectListAdd_t = void(__fastcall*)(void* list, void* edx, void* object);

Player_t g_hide = nullptr;

template <typename T>
T& at(void* base, int offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }
uint8_t* graphics() { return *reinterpret_cast<uint8_t**>(G_GRAPHICS); }

// World.vids[nvid], or the engine's empty VID when there is none (as the engine picks them)
uint8_t* vid_or_empty(int nvid) {
    uint8_t* w = world();
    uint8_t* v = nvid < at<int>(w, 0x290) ? reinterpret_cast<uint8_t**>(w + 0x294)[nvid] : nullptr;
    return v ? v : *reinterpret_cast<uint8_t**>(G_EMPTY_VID);
}

// the VID of `nvid` at the scale s (its bigger copy), or the empty VID
uint8_t* scaled_or_empty(int nvid, int s) {
    uint8_t* v = interface_vid(nvid, s);
    return v ? v : *reinterpret_cast<uint8_t**>(G_EMPTY_VID);
}

void set_hidden(uint8_t* vid, int hidden) {
    reinterpret_cast<SetInt_t>(VID_SET_HIDDEN)(vid, nullptr, hidden);
}

short height(uint8_t* vid) { return at<short>(vid, 0x2a8); }
short width(uint8_t* vid) { return at<short>(vid, 0x2a6); }
float size_x(uint8_t* vid) { return at<float>(vid, 0x1c); }
float size_y(uint8_t* vid) { return at<float>(vid, 0x20); }

uint8_t* bar_base(void* player) {
    const int count = at<int>(player, 0x18);   // PlayerBase.bar
    uint8_t** items = at<uint8_t**>(player, 0x20);
    for (int i = 0; i < count; ++i)
        if (items[i] && at<uint8_t*>(items[i], 0x1c) == interface_vid(BAR_VID, interface_scale())) return items[i];
    return nullptr;
}

// The side tiles of a bar built earlier are deleted: they are in no list but the world's menu (the bar's own
// objects go with the bar's list when it is given up), so a bar built again - TAB, a zoom (zoom.cpp) -
// would put a new row of them over the old one, at another place and size after a zoom. The engine's own
// way of letting a menu object go (ObjectList_RemoveAt): its Delete, whose Frame_Destruct takes it off
// the menu.
void remove_sides() {
    uint8_t* menu = world() + WORLD_MENU;
    for (int i = at<int>(menu, 4) - 1; i >= 0; --i) {
        if (i >= at<int>(menu, 4)) continue;
        uint8_t* o = at<uint8_t**>(menu, 0x0c)[i];
        if (!o || !at<uint8_t*>(o, 0x1c)) continue;
        const uint8_t* v = at<uint8_t*>(o, 0x1c);
        bool side = false;
        for (int s = 1; s <= 3; ++s)
            if (v == interface_vid(SIDE_VID, s)) side = true;
        if (side) reinterpret_cast<void(__fastcall*)(void*, void*, int)>(at<void**>(o, 0)[0])(o, nullptr, 1);
    }
}

void add_sides(void* player, uint8_t* side, int s) {
    uint8_t* base = bar_base(player);
    if (!base) return;
    auto* view = reinterpret_cast<float*>(graphics() + 0x220);   // left, right
    // the base's screen position: Frame_Construct adds the camera again
    const float x = at<float>(base, 0x30) - at<float>(world(), WORLD_CAMERA_X);
    const float y = at<float>(base, 0x34) - at<float>(world(), WORLD_CAMERA_X + 4);
    const float z = at<float>(base, 0x38);
    const float bar_width = BAR_WIDTH * s, tile_width = TILE_WIDTH * s;
    auto create = reinterpret_cast<CreateObject_t>(at<void**>(world(), 0)[0x20 / 4]);
    int tiles = 0;
    auto place = [&](float centre, int direction) {
        uint8_t* tile = create(world(), nullptr, side, centre, y, z, 0, nullptr);
        if (!tile) return;
        if (direction) reinterpret_cast<SetInt_t>(OBJECT_SET_DIRECTION)(tile, nullptr, direction);
        ++tiles;
    };
    // next to the bar the tile that fades into it, further out the repeated one
    for (float left = x - bar_width / 2; left > view[0]; left -= tile_width)
        place(left - tile_width / 2, left == x - bar_width / 2 ? LEFT_INNER : LEFT);
    for (float right = x + bar_width / 2; right < view[1]; right += tile_width)
        place(right + tile_width / 2, right == x + bar_width / 2 ? RIGHT_INNER : RIGHT);
    patch::log("state bar: %d side tiles for a view of %d-%d", tiles, static_cast<int>(view[0]),
               static_cast<int>(view[1]));
}

// The bar's objects (see the top). s = the interface's scale; at 1 the engine's own positions.
void create_bar(uint8_t* player, int s) {
    uint8_t* g = graphics();
    const float screen_w = at<float>(g, 0x20c);
    const float view_right = at<float>(g, 0x224), view_top = at<float>(g, 0x228), view_bottom = at<float>(g, 0x22c);
    auto create = reinterpret_cast<CreateObject_t>(at<void**>(world(), 0)[0x20 / 4]);
    void* list = player + 0x14;
    // an object at screen (x, y) and depth z: the engine's objects take y + z
    auto add = [&](uint8_t* vid, float x, float y, float z) {
        reinterpret_cast<ObjectListAdd_t>(OBJECTLIST_ADD)(list, nullptr, create(world(), nullptr, vid, x, y + z, z, 0, nullptr));
    };
    const float k = static_cast<float>(s);
    uint8_t* base = scaled_or_empty(BAR_VID, s);
    const float x0 = (screen_w * 0.5f + 95.0f * k) - 320.0f * k;                         // the engine's fVar2
    const float y0 = (view_bottom - static_cast<float>(height(base) / 2)) - 2.0f * k;    // fVar3
    auto slot_x = [&](int i, float dx) { return static_cast<float>(SLOT_PITCH * i) * k + x0 + dx * k; };
    for (int i = 1; i <= SLOTS; ++i) add(scaled_or_empty(4, s), slot_x(i, -14), y0 - 13 * k, 2005);
    for (int i = 1; i <= SLOTS; ++i) add(scaled_or_empty(8, s), slot_x(i, -17), (y0 - 17 * k) - 41 * k, 2005);
    add(scaled_or_empty(456, s), screen_w * 0.5f, y0, 2004);
    add(scaled_or_empty(498, s), x0 - 49 * k, y0 + 8 * k, 2003);
    add(scaled_or_empty(499, s), x0 - 11 * k, y0 + 3 * k, 2003);
    for (int i = 1; i <= SLOTS; ++i) add(scaled_or_empty(455, s), slot_x(i, 0), y0, 2003);
    for (int i = 1; i <= SLOTS; ++i) add(scaled_or_empty(463, s), slot_x(i, 3), y0 + 22 * k, 2002);
    for (int i = 1; i <= SLOTS; ++i) add(scaled_or_empty(457, s), slot_x(i, 3), y0 + 21 * k, 2001);
    for (int i = 1; i <= SLOTS; ++i) add(scaled_or_empty(458, s), slot_x(i, 3), y0 + 24 * k, 2001);
    for (int i = 1; i <= SLOTS; ++i) add(scaled_or_empty(459, s), slot_x(i, -17), y0 + 22 * k, 2001);
    // the minimap's frame: minimap.cpp picks it (it follows the interface's scale)
    void* original_frame = minimap_frame_for_screen();
    uint8_t* frame = vid_or_empty(464);
    add(frame, view_right - static_cast<float>(width(frame) / 2), view_bottom - static_cast<float>(height(frame) / 2), 2001);
    minimap_frame_back(original_frame);
    add(base, screen_w * 0.5f, view_bottom - static_cast<float>(height(base) / 2), 2000);
    uint8_t* plate = scaled_or_empty(469, s);
    add(plate, (view_right + 1.0f * k) - size_x(plate) * 0.5f, size_y(plate) * 0.5f + view_top, 2000);
    add(scaled_or_empty(2, s), (view_right - 75.0f * k) - 20.0f * k, view_top + 4 * k, 2001);
    add(scaled_or_empty(2, s), view_right - 43.0f * k, view_top + 4 * k, 2001);
    add(scaled_or_empty(502, s), ((view_right + 1.0f * k) - 30.0f * k) - size_x(plate) * 0.5f,
        size_y(plate) * 0.5f + view_top, 2000);
    add(scaled_or_empty(503, s), (view_right + 31.0f * k) - size_x(plate) * 0.5f, size_y(plate) * 0.5f + view_top, 2000);

    uint8_t** items = at<uint8_t**>(player, 0x20);
    uint8_t* up = items[22];
    reinterpret_cast<SetInt_t>(OBJECT_SET_DIRECTION)(up, nullptr, static_cast<int>(0x100u / at<unsigned>(at<uint8_t*>(up, 0x1c), 0x64)));
    reinterpret_cast<SetInt_t>(OBJECT_SET_ARMY)(up, nullptr, 2);
    if (at<int>(world(), WORLD_ARMY) != at<int>(player, 0x0c)) {
        for (int i = 0; i < at<int>(player, 0x18); ++i) {
            uint8_t* item = at<uint8_t**>(player, 0x20)[i];
            at<uint32_t>(item, 0x28) |= OBJECT_INVISIBLE;
            if (uint8_t* link = at<uint8_t*>(item, 0x40)) reinterpret_cast<Object_t>(OBJECT_HIDE_CHAIN)(link, nullptr);
        }
    }
}

// The camera's limits: the map, and below it the bar's height in map pixels less 2 - the engine's own room, so the
// bar hides none of the map (under a zoom the bar keeps its size in the window: its height / z in map pixels).
void set_bounds(uint8_t* player) {
    uint8_t* w = world();
    at<float>(w, 0x34) = 0.0f;                    // boundsLeft
    at<float>(w, 0x3c) = 0.0f;                    // boundsTop
    at<float>(w, 0x38) = at<float>(w, 0x28);      // boundsRight = the map's width
    const float bar = static_cast<float>(height(scaled_or_empty(BAR_VID, interface_scale())));
    at<float>(w, 0x40) = static_cast<float>(bar * zoom_view_room()) + at<float>(w, 0x2c) - 2.0f;
    minimap_bounds(player);
}

void show_bar(uint8_t* player);

// Player_ShowStateBar; under a zoom the bar has the window's layout (zoom_view.cpp)
void __fastcall show(uint8_t* player, void*) {
    void* token = zoom_view_layout_begin();
    show_bar(player);
    zoom_view_layout_end(token);
}

void show_bar(uint8_t* player) {
    // the message fonts and the click strip, and their bigger copies (the messages, interface_texts.cpp)
    for (int nvid : {14, 16, 17, 18, 449}) {
        set_hidden(vid_or_empty(nvid), 0);
        interface_set_hidden(nvid, 0);
    }
    const bool created = at<int>(player, 0x18) == 0;   // the bar is built now
    if (created) {
        const int s = interface_choose_scale();
        interface_copy_colours(s);
        create_bar(player, s);
    }
    interface_cursor_scale(interface_scale());
    uint8_t* flagman = at<uint8_t*>(player, 0x10);
    const int panel = flagman && at<int>(at<uint8_t*>(flagman, 0x1c), 0x10) == B_ENGINE ? 1 : 0;
    reinterpret_cast<SetInt_t>(SET_PANEL)(player, nullptr, panel);
    set_bounds(player);
    uint8_t* side = interface_vid(SIDE_VID, interface_scale());
    if (!side) return;   // objects.res without VID 938
    if (created && at<int>(player, 0x18) > 0) {
        remove_sides();
        add_sides(player, side, interface_scale());
    }
    set_hidden(side, 0);
}

void __fastcall hide(void* player, void* edx) {
    g_hide(player, edx);
    if (uint8_t* side = interface_vid(SIDE_VID, 1)) set_hidden(side, 1);
    for (int nvid : {SIDE_VID, 14, 16, 17, 18, 449}) interface_set_hidden(nvid, 1);
    interface_cursor_scale(1);
}

}  // namespace

// zoom.cpp, between two frames after the game's screen changed: the bar given up and built again for the new
// screen (as on the way back from the Esc menu), at the scale it takes now (interface_choose_scale), the
// factory's panel open again if it was. False when the bar is not built (TAB hid it, a menu is up): it is
// built for the screen when it is shown.
// zoom_view.cpp, after a zoom's step: the camera's room below the map for the bar's height at that zoom.
void status_bar_room(uint8_t* player) {
    if (player && at<int>(player, 0x18) > 0) set_bounds(player);
}

bool status_bar_rebuild(uint8_t* player) {
    if (!player || !g_hide || at<int>(player, 0x18) == 0) return false;
    const int panel = at<int>(player, 0x2c) & ~0x10;
    g_hide(player, nullptr);   // the engine's: the bar's list released (its objects deleted), the panel 6
    show(player, nullptr);
    uint8_t* flagman = at<uint8_t*>(player, 0x10);
    if ((panel == 2 || panel == 5) && flagman && at<int>(at<uint8_t*>(flagman, 0x1c), 0x10) == B_DEPO)
        reinterpret_cast<SetInt_t>(SET_PANEL)(player, nullptr, 2);
    return true;
}

void install_status_bar() {
    install_interface_scale();
    // sub esp,0x14 / mov eax,[g_world]
    const uint8_t show_start[] = {0x83, 0xEC, 0x14, 0xA1, 0x38, 0x9D, 0x4A, 0x00};
    // mov eax,[g_world] / push edi / mov edi,ecx
    const uint8_t hide_start[] = {0xA1, 0x38, 0x9D, 0x4A, 0x00, 0x57, 0x8B, 0xF9};
    g_hide = reinterpret_cast<Player_t>(patch::hook(HIDE_STATE_BAR, hide_start, sizeof hide_start,
                                                    reinterpret_cast<void*>(&hide), "Player_HideStateBar"));
    if (!g_hide) return;   // tiles that could not be hidden with the bar: rather none
    patch::redirect(SHOW_STATE_BAR, show_start, sizeof show_start, reinterpret_cast<void*>(&show), "Player_ShowStateBar");
}
