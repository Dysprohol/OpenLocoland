// A second row of buildable units in the depot panel.
//
// The depot's build list (Unit.items, set in the map) may name more units than the panel
// shows: Player_SetPanel (0x42f210) gives the list ten slots in one row - the icons are
// bar objects 80-89 (VID 455), the build times the bar's texts 10-19, the hotkey labels
// objects 90-99 (VID 8) and the backing strip object 100 (VID 497, 409 px: ten icons of
// 41 px) - and a unit past the tenth has no icon. The game knows 14 depot units (the
// hotkeys F1-F12, [ and ] of Player_HandleInput), so a depot with all of them shows only
// ten.
//
// After Player_SetPanel has built the depot list (panel 2) we add a row 41 px above it
// for units 11-20: a strip, ten icons, their build times and hotkey labels, made the way
// the engine makes its own - the icon frame is the weapon's iconFrame (of the linked VID
// first), army 3 when Player_CanBuild allows the unit, 2 when it does not, 1 for an empty
// slot. They go at the end of the bar list, which Player_SetPanel trims back to 80 objects
// every time, so they live and die with the panel.
//
// Player_HandleInput turns a click on an icon into an entry of the panel: (x - first
// icon x) / 41 + 1 on the row of object 80, + 100 (a queue slot) on any other row. Our
// row would read as the queue, so Player_HandlePanelAction (0x430ff0) is hooked: a click
// on one of our icons becomes entry 11 + column. Entries over ten (our icons and the
// hotkeys F11, F12, [ and ], which pass the unit's place in the list) are refused when the
// icon says the depot cannot build the unit - the engine's own test reads bar object
// 79 + entry, which for them is a label.
#include "patch.h"

#include <cstdio>

namespace {

constexpr uintptr_t SET_PANEL = 0x42f210;
constexpr uintptr_t HANDLE_PANEL_ACTION = 0x430ff0;
constexpr uintptr_t OBJECTLIST_ADD = 0x44b970;
constexpr uintptr_t OBJECT_SET_DIRECTION = 0x448660;
constexpr uintptr_t OBJECT_SET_ARMY = 0x448550;
constexpr uintptr_t PLAYER_CAN_BUILD = 0x432390;
constexpr uintptr_t VID_GET_BUILD_TIME_SEC = 0x44e440;
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr int WORLD_CAMERA_X = 0x44;   // World: top-left of the view on the map (cameraY at +0x48)

constexpr int ACT_SET_BEHAVE = 95;
constexpr int ACT_SET_TEXT = 120;
constexpr int B_DEPO = 0x18;
constexpr int PANEL_DEPOT_LIST = 2;
constexpr int ROW = 10;              // slots in a row
constexpr float ROW_HEIGHT = 41.0f;  // the icon pitch of the panel

// bar objects of the depot list (Player_SetPanel)
constexpr int FIRST_ICON = 80;
constexpr int FIRST_PRICE = 10;
constexpr int FIRST_LABEL = 90;
constexpr int STRIP = 100;
constexpr int PANEL_OBJECTS = 101;            // the bar with the depot list
constexpr int EXTRA_FIRST_ICON = PANEL_OBJECTS + 1;  // our strip, then ten icons, prices, labels
constexpr int EXTRA_OBJECTS = 1 + 3 * ROW;

using SetPanel_t = void(__fastcall*)(void* player, void* edx, int panel);
using HandlePanelAction_t = void(__fastcall*)(void* player, void* edx, int object, int* index);
using CreateObject_t = uint8_t*(__fastcall*)(void* world, void* edx, void* vid, float x, float y,
                                             float z, unsigned direction, void* parent);
using ObjectListAdd_t = void(__fastcall*)(void* list, void* edx, void* object);
using SetInt_t = void(__fastcall*)(void* object, void* edx, int value);
using CanBuild_t = int(__fastcall*)(void* player, void* edx, void* vid);
using BuildTime_t = int(__fastcall*)(void* vid, void* edx);
using Action_t = int(__fastcall*)(void* object, void* edx, int act, int var1, int var2, int var3);

SetPanel_t g_set_panel = nullptr;
HandlePanelAction_t g_handle_panel_action = nullptr;

template <typename T>
T& at(void* base, int offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }
int bar_count(void* player) { return at<int>(player, 0x18); }
uint8_t** bar_items(void* player) { return at<uint8_t**>(player, 0x20); }
int panel_of(void* player) { return at<int>(player, 0x2c) & ~0x10; }

uint8_t* vid_by_number(int nvid) {
    uint8_t* w = world();
    if (nvid < 0 || nvid >= at<int>(w, 0x290)) return nullptr;
    return reinterpret_cast<uint8_t**>(w + 0x294)[nvid];
}

void act(uint8_t* object, int action, int var1) {
    auto fn = reinterpret_cast<Action_t>(at<void**>(object, 0)[1]);
    fn(object, nullptr, action, var1, 0, 0);
}

void set_text(uint8_t* text, const char* value) {
    char* string = const_cast<char*>(value);   // ACT_SET_TEXT copies the String it is given
    act(text, ACT_SET_TEXT, static_cast<int>(reinterpret_cast<uintptr_t>(&string)));
}

// a copy of a bar object `rows_up` rows higher, added to the bar. The bar's objects are
// FRAME and TEXT objects: Frame_Construct takes a screen position and adds the camera (and
// SetPosition moves them with it), so the copy is given the source's position minus the camera.
uint8_t* copy_up(void* player, uint8_t* source, int rows_up) {
    auto create = reinterpret_cast<CreateObject_t>(at<void**>(world(), 0)[0x20 / 4]);
    const float x = at<float>(source, 0x30) - at<float>(world(), WORLD_CAMERA_X);
    const float y = at<float>(source, 0x34) - at<float>(world(), WORLD_CAMERA_X + 4);
    uint8_t* object = create(world(), nullptr, at<void*>(source, 0x1c), x, y - ROW_HEIGHT * rows_up,
                             at<float>(source, 0x38), 0, nullptr);
    if (object)
        reinterpret_cast<ObjectListAdd_t>(OBJECTLIST_ADD)(static_cast<uint8_t*>(player) + 0x14, nullptr,
                                                          object);
    return object;
}

const char* hotkey_label(int nvid) {
    // Player_HandleInput: F1-F12, [ and ] build these units
    switch (nvid) {
        case 5: return "F1";
        case 10: return "F2";
        case 20: return "F3";
        case 25: return "F4";
        case 80: return "F5";
        case 85: return "F6";
        case 45: return "F7";
        case 30: return "F8";
        case 35: return "F9";
        case 82: return "F10";
        case 97: return "F11";
        case 90: return "F12";
        case 75: return "[";
        case 62: return "]";
        default: return "";
    }
}

void add_second_row(void* player) {
    if (panel_of(player) != PANEL_DEPOT_LIST || bar_count(player) != PANEL_OBJECTS) return;
    uint8_t* depot = at<uint8_t*>(player, 0x10);
    if (!depot || at<int>(at<uint8_t*>(depot, 0x1c), 0x10) != B_DEPO) return;
    const int count = at<int>(depot, 0x94);             // Unit.items (ShortList)
    const short* list = at<short*>(depot, 0x9c);
    if (count <= ROW || !list) return;
    uint8_t** items = bar_items(player);
    if (at<int>(at<uint8_t*>(items[FIRST_ICON], 0x1c), 4) != 455 ||
        at<int>(at<uint8_t*>(items[STRIP], 0x1c), 4) != 497)
        return;   // not the bar this code knows

    copy_up(player, items[STRIP], 1);
    for (int c = 0; c < ROW; ++c) {
        uint8_t* icon = copy_up(player, bar_items(player)[FIRST_ICON + c], 1);
        if (!icon) return;
        const int entry = ROW + c;
        uint8_t* vid = entry < count ? vid_by_number(list[entry]) : nullptr;
        int army = 1;
        if (vid && at<int>(vid, 4) != 0) {
            uint8_t* link = at<uint8_t*>(vid, 0x5c);
            int frame = link ? at<int>(at<uint8_t*>(link, 0x404), 0x38) : 0;
            if (frame == 0) frame = at<int>(at<uint8_t*>(vid, 0x404), 0x38);
            const int dirs = at<int>(at<uint8_t*>(icon, 0x1c), 0x64);
            reinterpret_cast<SetInt_t>(OBJECT_SET_DIRECTION)(icon, nullptr,
                                                            static_cast<int>((unsigned(frame) << 8) / unsigned(dirs)));
            army = reinterpret_cast<CanBuild_t>(PLAYER_CAN_BUILD)(player, nullptr, vid) ? 3 : 2;
        }
        reinterpret_cast<SetInt_t>(OBJECT_SET_ARMY)(icon, nullptr, army);
    }
    for (int c = 0; c < ROW; ++c) {
        uint8_t* source = bar_items(player)[FIRST_PRICE + c];
        uint8_t* text = copy_up(player, source, 1);
        if (!text) return;
        act(text, ACT_SET_BEHAVE, at<int>(source, 0x7c));
        uint8_t* vid = ROW + c < count ? vid_by_number(list[ROW + c]) : nullptr;
        char price[16] = "";
        if (vid && at<int>(vid, 4) != 0)
            std::snprintf(price, sizeof price, "%d",
                          reinterpret_cast<BuildTime_t>(VID_GET_BUILD_TIME_SEC)(vid, nullptr));
        set_text(text, price);
    }
    for (int c = 0; c < ROW; ++c) {
        uint8_t* source = bar_items(player)[FIRST_LABEL + c];
        uint8_t* text = copy_up(player, source, 1);
        if (!text) return;
        act(text, ACT_SET_BEHAVE, at<int>(source, 0x7c));
        set_text(text, ROW + c < count ? hotkey_label(list[ROW + c]) : "");
    }
}

void __fastcall set_panel(void* player, void* edx, int panel) {
    g_set_panel(player, edx, panel);
    add_second_row(player);
}

void __fastcall handle_panel_action(void* player, void* edx, int object, int* index) {
    if (index && panel_of(player) == PANEL_DEPOT_LIST &&
        bar_count(player) >= PANEL_OBJECTS + EXTRA_OBJECTS) {
        uint8_t** items = bar_items(player);
        uint8_t* hover = at<uint8_t*>(world(), 0x24c + 0x14);   // World.menu.hover
        const uint32_t click_flags = at<uint32_t>(world(), 0x24c + 0x10);
        if (hover && (click_flags & 1)) {
            for (int c = 0; c < ROW; ++c) {
                if (items[EXTRA_FIRST_ICON + c] == hover) {
                    *index = ROW + c + 1;   // the engine read it as queue slot 100 + c
                    break;
                }
            }
        }
        if (*index > ROW && *index <= 2 * ROW) {
            uint8_t* icon = items[EXTRA_FIRST_ICON + *index - ROW - 1];
            const uint32_t army = (at<uint32_t>(icon, 0x28) >> 10) & 3;
            if (army != 3) return;   // empty slot or the depot cannot build it
        }
    }
    g_handle_panel_action(player, edx, object, index);
}

}  // namespace

// A unit's price in a factory - what the factory's panel shows under its icon: its build time in
// seconds, which the factory takes from the army's money (editor_mission.cpp: the runner's PlanPrice).
int unit_price(int nvid) {
    uint8_t* vid = vid_by_number(nvid);
    return vid ? reinterpret_cast<BuildTime_t>(VID_GET_BUILD_TIME_SEC)(vid, nullptr) : 0;
}

void install_depot_rows() {
    // push -1 / push 0x473d38 / mov eax,fs:[0]
    const uint8_t set_panel_start[] = {0x6A, 0xFF, 0x68, 0x38, 0x3D, 0x47, 0x00,
                                       0x64, 0xA1, 0x00, 0x00, 0x00, 0x00};
    // push esi / mov esi,ecx / push edi / mov eax,[esi+0x2c]
    const uint8_t action_start[] = {0x56, 0x8B, 0xF1, 0x57, 0x8B, 0x46, 0x2C};
    // the clicks first: a second row whose clicks went to the queue would be worse than none
    g_handle_panel_action = reinterpret_cast<HandlePanelAction_t>(patch::hook(
        HANDLE_PANEL_ACTION, action_start, sizeof action_start,
        reinterpret_cast<void*>(&handle_panel_action), "Player_HandlePanelAction"));
    if (!g_handle_panel_action) return;
    g_set_panel = reinterpret_cast<SetPanel_t>(patch::hook(
        SET_PANEL, set_panel_start, sizeof set_panel_start, reinterpret_cast<void*>(&set_panel),
        "Player_SetPanel"));
}
