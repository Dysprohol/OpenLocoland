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
//
// At the interface's scale s (interface_scale.cpp) the list is the engine's, made with the bigger
// VIDs and spaced s times wider: while Player_SetPanel runs, World.vids[455], [497] and [8] name the
// scale's bigger copies (the engine makes the icons, the strip and the labels from them, so they are
// counted, drawn and hit-tested as theirs), and afterwards every object the panel added is moved
// from its place relative to the first slot's type icon (bar object 23, already at the scale) to s
// times that - the strip, which the engine centres on the screen, only up. Our second row is a row
// of the scale's height above.
//
// The limits and the units that come later (an editor map's factory build options, editor-mission.md
// "A factory's build options"). The engine's list shows a unit as buildable (army 3) or not (2) and
// nothing more: a limit of AddUnitLimit (Vid.maxUnitArmy[army], or the total Vid.maxUnit) greys the
// icon once reached, and a unit the runner adds later (ACT_ADD_ITEM at its time, FactoryTact) is not in
// the list until then. So, where the panel is made:
//  - a unit with a limit gets a counter "n/max" in the icon's free bottom-left corner (the price is
//    top left, the key bottom right), n counted as Player_CanBuild counts it: the army's units of that
//    VID on the map and the VID in the queues of the factories (the army's limit, else the total one;
//    with both the one closer to its end); a unit without a limit gets an empty text, which draws
//    nothing, so a list with no limit looks as before;
//  - a unit the runner will add to this factory (the script's unlock_x/y/vid/time[] of a factory
//    within 30 px, the runner's own FindNearestSprite radius, a time still to come) is shown after the
//    list's units, where ACT_ADD_ITEM appends it, in the order of the times: its icon and price as the
//    engine draws a unit, greyed (army 2 - a click and its key do nothing, as for a unit that cannot
//    be built), no key, and in the counter's corner the time left, m:ss, by the runner's own clock
//    ((GetTime() - time_game_total) / 1000 against unlock_time). A map without the runner (every
//    campaign mission) has no such variables and shows nothing of it;
//  - over such an icon the mouse tip, the unit's name, says it in words too ([Editor] PlayFactoryLimit /
//    PlayFactoryUnlock of the language pack).
//
// The panel kept up to date (issue #20 point 2). Player_SetPanel decides each icon's army once, and
// Player_UpdateStateBar, every frame, draws the queue but never looks at the list again - so a unit
// that became affordable (money came in), or whose limit freed up (a unit lost) stayed greyed while
// the panel was open, and its click and key were refused, since Player_HandlePanelAction reads the
// icon's army; one no longer affordable stayed lit. A unit added to the list (an unlock) did not show
// until the factory was selected again. Player_UpdateStateBar is hooked: after it, every icon of the
// list gets the army Player_SetPanel would give it now (Player_CanBuild, the same call), and the
// counters and times their texts now (set only when they change); before it, a list that is no longer
// the one the panel was made from (a unit unlocked, a script's ACT_ADD_ITEM or ACT_DELETE_ALL_ITEM)
// makes the panel again (Player_SetPanel(2), as a click does; LocoFix.log "factory menu: ...").
#include "patch.h"

int interface_scale();                          // interface_scale.cpp
int interface_choose_scale();
void interface_copy_colours(int scale);
uint8_t* interface_vid(int nvid, int scale);
void* zoom_view_layout_begin();                 // zoom_view.cpp: the window's layout under a zoom
void zoom_view_layout_end(void* token);
void factory_log_panel(int step);               // factory_log.cpp

#include "editor_text.h"
#include "script_value.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uintptr_t SET_PANEL = 0x42f210;
constexpr uintptr_t HANDLE_PANEL_ACTION = 0x430ff0;
constexpr uintptr_t UPDATE_STATE_BAR = 0x4301e0;
constexpr uintptr_t MOUSE_TIP_TEXT = 0x41f5c0;       // World_GetMouseTipText
constexpr uintptr_t DRAW_UNIT_ICON = 0x42f120;       // StateBar_DrawUnitIcon
constexpr uintptr_t STRING_ASSIGN = 0x4264c0;
constexpr uintptr_t G_OBJECT_MAP = 0x4a9c10;         // the map hash: +0x34 the unit list (count +0x38, items +0x40)
constexpr uintptr_t G_GAME_TIME = 0x4a9d64;          // ms, the scripts' GetTime()
constexpr uintptr_t OBJECTLIST_ADD = 0x44b970;
constexpr uintptr_t OBJECT_SET_DIRECTION = 0x448660;
constexpr uintptr_t OBJECT_SET_ARMY = 0x448550;
constexpr uintptr_t OBJECT_MOVE_TO = 0x446640;
constexpr uintptr_t PLAYER_CAN_BUILD = 0x432390;
constexpr uintptr_t VID_GET_BUILD_TIME_SEC = 0x44e440;
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr int WORLD_CAMERA_X = 0x44;   // World: top-left of the view on the map (cameraY at +0x48)

constexpr int ACT_SET_BEHAVE = 95;
constexpr int ACT_SET_TEXT = 120;
constexpr int B_DEPO = 0x18;
constexpr int PANEL_DEPOT_LIST = 2;
constexpr int PANEL_DEPOT_QUEUE = 5;
constexpr int MARKER = 20;           // the marker under the flagman's slot (VID 456)
constexpr int FIRST_TYPE_ICON = 23;  // the bar's first slot's type icon: where the panel's rows start
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
constexpr int SHADOW_FONT = 1200;   // the price font's dark copies at x1, x2, x3 (tools/patch_data.py SHADOW_FONTS)

using SetPanel_t = void(__fastcall*)(void* player, void* edx, int panel);
using HandlePanelAction_t = void(__fastcall*)(void* player, void* edx, int object, int* index);
using CreateObject_t = uint8_t*(__fastcall*)(void* world, void* edx, void* vid, float x, float y,
                                             float z, unsigned direction, void* parent);
using ObjectListAdd_t = void(__fastcall*)(void* list, void* edx, void* object);
using SetInt_t = void(__fastcall*)(void* object, void* edx, int value);
using MoveTo_t = void(__fastcall*)(void* object, void* edx, float x, float y, float z);
using CanBuild_t = int(__fastcall*)(void* player, void* edx, void* vid);
using BuildTime_t = int(__fastcall*)(void* vid, void* edx);
using Action_t = int(__fastcall*)(void* object, void* edx, int act, int var1, int var2, int var3);
using UpdateStateBar_t = void(__fastcall*)(void* player, void* edx, void* mouse);
using MouseTipText_t = char**(__fastcall*)(void* world, void* edx, char** out);
using DrawUnitIcon_t = int(__fastcall*)(void* player, void* edx, int slot, void* vid);
using StringAssign_t = char**(__fastcall*)(char** self, void* edx, char** source);

SetPanel_t g_set_panel = nullptr;
HandlePanelAction_t g_handle_panel_action = nullptr;
UpdateStateBar_t g_update_state_bar = nullptr;
MouseTipText_t g_mouse_tip_text = nullptr;

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
    uint8_t* object = create(world(), nullptr, at<void*>(source, 0x1c), x, y - ROW_HEIGHT * interface_scale() * rows_up,
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

// The units the list shows: the depot's list, then the units that come later (the first `listed` are
// the list's).
struct Shown {
    std::vector<int> units;
    int listed = 0;
};

// the icon frame of a unit in the list: the weapon's iconFrame of its link first (StateBar_DrawUnitIcon)
void set_unit_icon(uint8_t* icon, uint8_t* vid) {
    uint8_t* link = at<uint8_t*>(vid, 0x5c);
    int frame = link ? at<int>(at<uint8_t*>(link, 0x404), 0x38) : 0;
    if (frame == 0) frame = at<int>(at<uint8_t*>(vid, 0x404), 0x38);
    const int dirs = at<int>(at<uint8_t*>(icon, 0x1c), 0x64);
    reinterpret_cast<SetInt_t>(OBJECT_SET_DIRECTION)(icon, nullptr,
                                                    static_cast<int>((unsigned(frame) << 8) / unsigned(dirs)));
}

// Units 11-20: a row above the engine's. Returns the bar index of its first icon, -1 when there is none.
int add_second_row(void* player, const Shown& shown) {
    const int count = static_cast<int>(shown.units.size());
    if (count <= ROW) return -1;
    const int first = bar_count(player) + 1;   // after the strip
    copy_up(player, bar_items(player)[STRIP], 1);
    for (int c = 0; c < ROW; ++c) {
        uint8_t* icon = copy_up(player, bar_items(player)[FIRST_ICON + c], 1);
        if (!icon) return -1;
        const int entry = ROW + c;
        uint8_t* vid = entry < count ? vid_by_number(shown.units[entry]) : nullptr;
        int army = 1;
        if (vid && at<int>(vid, 4) != 0) {
            set_unit_icon(icon, vid);
            army = entry < shown.listed && reinterpret_cast<CanBuild_t>(PLAYER_CAN_BUILD)(player, nullptr, vid) ? 3 : 2;
        }
        reinterpret_cast<SetInt_t>(OBJECT_SET_ARMY)(icon, nullptr, army);
    }
    for (int c = 0; c < ROW; ++c) {
        uint8_t* source = bar_items(player)[FIRST_PRICE + c];
        uint8_t* text = copy_up(player, source, 1);
        if (!text) return -1;
        act(text, ACT_SET_BEHAVE, at<int>(source, 0x7c));
        uint8_t* vid = ROW + c < count ? vid_by_number(shown.units[ROW + c]) : nullptr;
        char price[16] = "";
        if (vid && at<int>(vid, 4) != 0)
            std::snprintf(price, sizeof price, "%d",
                          reinterpret_cast<BuildTime_t>(VID_GET_BUILD_TIME_SEC)(vid, nullptr));
        set_text(text, price);
    }
    for (int c = 0; c < ROW; ++c) {
        uint8_t* source = bar_items(player)[FIRST_LABEL + c];
        uint8_t* text = copy_up(player, source, 1);
        if (!text) return -1;
        act(text, ACT_SET_BEHAVE, at<int>(source, 0x7c));
        set_text(text, ROW + c < shown.listed ? hotkey_label(shown.units[ROW + c]) : "");
    }
    return first;
}

// ---- the limits, the units that come later, the panel kept up to date ----

constexpr int UNLOCK_RADIUS = 30;   // the runner's FindNearestSprite(GETSPRITE_VID+102, x, y, 30)

uint32_t game_ms() { return *reinterpret_cast<uint32_t*>(G_GAME_TIME); }
int army_of(uint8_t* object) { return static_cast<int>((at<uint32_t>(object, 0x28) >> 10) & 3); }

// The script's own variable `name` (a symbol of kind 1, as script_refs.cpp reads them): its first value on
// the script's stack and how many there are; nullptr when the script has none. Its slots are looked up
// again only when the script's symbol table is another (a level loaded).
struct Variable {
    const char* name;
    const void* symbols = nullptr;
    int symbol_count = -1;
    int first = -1, count = 0;
};

const sv::Value* script_variable(Variable& v, int& count) {
    count = 0;
    uint8_t* script = world() + 0x14c;                  // World.script
    const uint8_t* symbols = at<uint8_t*>(script, 0x1c);
    const int symbol_count = at<int>(script, 0x14);
    if (symbols != v.symbols || symbol_count != v.symbol_count) {
        v.symbols = symbols;
        v.symbol_count = symbol_count;
        v.first = -1;
        v.count = 0;
        for (int i = 0; symbols && i < symbol_count; ++i) {
            const uint8_t* s = symbols + i * 0x18;
            if (s[4] != 1) continue;                      // a variable
            const char* name = *reinterpret_cast<const char* const*>(s);
            if (!name || std::strcmp(name, v.name) != 0) continue;
            v.first = *reinterpret_cast<const int*>(s + 0x0c);
            const int n = *reinterpret_cast<const int*>(s + 0x14);
            v.count = n > 0 ? n : 1;
            break;
        }
    }
    const sv::Value* stack = at<sv::Value*>(script, 0x0c);
    if (!stack || v.first < 0 || v.first + v.count > at<int>(script, 0x04)) return nullptr;
    count = v.count;
    return stack + v.first;
}

struct Later {
    int nvid;
    uint32_t at;   // the game time (GetTime()) the runner puts it into the list
};

// The units the editor's runner puts into this factory's list later (FactoryTact: unlock_vid[i] into the
// factory within 30 px of unlock_x/y[i] once (GetTime() - time_game_total) / 1000 >= unlock_time[i], which
// it then sets to -1), in the order they come; none on a map without the runner or before its start.
std::vector<Later> later_units(uint8_t* depot) {
    static Variable start{"time_game_total"}, xs{"unlock_x"}, ys{"unlock_y"}, vids{"unlock_vid"}, times{"unlock_time"};
    std::vector<Later> out;
    int n0 = 0, nx = 0, ny = 0, nv = 0, nt = 0;
    const sv::Value* t0 = script_variable(start, n0);
    const sv::Value* ux = script_variable(xs, nx);
    const sv::Value* uy = script_variable(ys, ny);
    const sv::Value* uv = script_variable(vids, nv);
    const sv::Value* ut = script_variable(times, nt);
    if (!t0 || !ux || !uy || !uv || !ut) return out;
    const int begin = sv::ToInt(*t0);
    if (begin == -1) return out;                        // InitDefault has not run yet
    const int count = at<int>(depot, 0x94);
    const short* list = at<short*>(depot, 0x9c);
    const float fx = at<float>(depot, 0x30), fy = at<float>(depot, 0x34);
    const int n = (std::min)({nx, ny, nv, nt});
    for (int i = 0; i < n; ++i) {
        const int nvid = sv::ToInt(uv[i]), time = sv::ToInt(ut[i]);
        if (nvid <= 0 || time < 0) continue;
        const float dx = fx - static_cast<float>(sv::ToInt(ux[i])), dy = fy - static_cast<float>(sv::ToInt(uy[i]));
        if (dx * dx + dy * dy > static_cast<float>(UNLOCK_RADIUS * UNLOCK_RADIUS)) continue;
        uint8_t* vid = vid_by_number(nvid);
        if (!vid || at<int>(vid, 4) == 0) continue;
        bool known = false;
        for (int k = 0; list && k < count && !known; ++k) known = list[k] == nvid;
        const uint32_t when = static_cast<uint32_t>(begin) + static_cast<uint32_t>(time) * 1000u;
        for (Later& l : out)
            if (l.nvid == nvid) {
                known = true;
                if (when < l.at) l.at = when;
            }
        if (!known) out.push_back({nvid, when});
    }
    std::stable_sort(out.begin(), out.end(), [](const Later& a, const Later& b) { return a.at < b.at; });
    return out;
}

// The unit's limit for the player's army as Player_CanBuild counts it: `used` the army's units of the VID
// on the map and the VID in the queues of the factories (of army 0, as the engine counts them), against the
// army's limit (Vid.maxUnitArmy) or the total one (Vid.maxUnit, every army's units); with both, the one
// with fewer left. False when the VID has neither.
bool unit_limit(void* player, uint8_t* vid, int& used, int& max) {
    const int army = at<int>(player, 0x0c) & 3;
    const int per_army = at<int>(vid, 0x348 + 4 * army), total = at<int>(vid, 0x344);
    if (per_army < 0 && total < 0) return false;
    const int nvid = at<int>(vid, 4);
    int queued = 0;
    if (uint8_t* map = *reinterpret_cast<uint8_t**>(G_OBJECT_MAP)) {
        uint8_t** units = at<uint8_t**>(map, 0x40);
        for (int i = at<int>(map, 0x38) - 1; units && i >= 0; --i) {
            uint8_t* o = units[i];
            if (!o || at<int>(at<uint8_t*>(o, 0x1c), 0x10) != B_DEPO || (at<uint32_t>(o, 0x28) & 0xc00)) continue;
            const int in_queue = at<int>(o, 0x494);
            const uint16_t* queue = reinterpret_cast<const uint16_t*>(o + 0xa4);
            for (int k = 0; k < 10 && k < in_queue; ++k) queued += queue[k] == nvid;
        }
    }
    const int mine = at<int>(vid, 0x358 + 4 * army) + queued;
    const int all = at<int>(vid, 0x358) + at<int>(vid, 0x35c) + at<int>(vid, 0x360) + at<int>(vid, 0x364) + queued;
    if (per_army >= 0 && (total < 0 || per_army - mine <= total - all)) {
        used = mine;
        max = per_army;
    } else {
        used = all;
        max = total;
    }
    return true;
}

// One unit of the panel: its icon and our text in the icon's bottom-left corner (the counter or the time).
struct Slot {
    int nvid = 0;
    bool later = false;     // not in the list yet
    uint32_t at = 0;        // later: when it comes
    uint8_t* icon = nullptr;
    uint8_t* note = nullptr;
    std::vector<uint8_t*> outline;   // the note's dark copies round it (none without the dark font)
    std::string text;       // the note's text now
    bool limited = false;   // a limit: `used` of `max`
    int used = 0, max = 0;
};

struct Menu {
    void* player = nullptr;
    uint8_t* depot = nullptr;
    std::vector<short> list;   // the depot's list the panel was made from
    std::vector<int> later;    // the later units it shows
    std::vector<Slot> slots;
    int bar = 0;               // the bar's objects with the panel
    uint8_t* first_icon = nullptr;
};

Menu g_menu;

// A text of the bar at x, y (the view's) in `font`, with the depth and behaviour of the price.
uint8_t* add_text(void* player, uint8_t* price, void* font, float x, float y) {
    auto create = reinterpret_cast<CreateObject_t>(at<void**>(world(), 0)[0x20 / 4]);
    uint8_t* text = create(world(), nullptr, font, x, y, at<float>(price, 0x38), 0, nullptr);
    if (!text) return nullptr;
    reinterpret_cast<ObjectListAdd_t>(OBJECTLIST_ADD)(static_cast<uint8_t*>(player) + 0x14, nullptr, text);
    act(text, ACT_SET_BEHAVE, at<int>(price, 0x7c));
    set_text(text, "");
    return text;
}

// The dark copy of the price's font (VID 8, or its bigger copy of the interface's scale - Player_SetPanel makes
// the list with it) and the scale it was made for; nullptr without one.
uint8_t* shadow_font(void* font, int& scale) {
    for (int s = 1; s <= 3; ++s)
        if (font == interface_vid(8, s)) {
            scale = s;
            return vid_by_number(SHADOW_FONT + s - 1);
        }
    return nullptr;
}

// Our text in an icon's bottom-left corner: the font, depth and alignment of its price (top left, at the
// icon's left edge) on the line of its key (bottom right), so it follows the interface's scale as they do.
// Round it a dark outline - eight dark copies one font pixel away in each direction, made after it, which the
// bar draws under it (a bar object added later is drawn earlier): the corner stands on the unit's picture,
// where the white text alone was hard to read on its light parts (the price and the key stand on the icon's
// dark ground and stay as the game draws them).
uint8_t* add_note(void* player, uint8_t* price, uint8_t* label, std::vector<uint8_t*>& outline) {
    const float x = at<float>(price, 0x30) - at<float>(world(), WORLD_CAMERA_X);
    const float y = at<float>(label, 0x34) - at<float>(label, 0x38) - at<float>(world(), WORLD_CAMERA_X + 4) +
                    at<float>(price, 0x38);
    void* font = at<void*>(price, 0x1c);
    uint8_t* note = add_text(player, price, font, x, y);
    int scale = 1;
    outline.clear();
    if (uint8_t* dark = note ? shadow_font(font, scale) : nullptr)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (dx || dy)
                    if (uint8_t* o = add_text(player, price, dark, x + static_cast<float>(dx * scale), y + static_cast<float>(dy * scale)))
                        outline.push_back(o);
    return note;
}

bool menu_valid(void* player) {
    return player && g_menu.player == player && panel_of(player) == PANEL_DEPOT_LIST && bar_count(player) == g_menu.bar &&
           at<uint8_t*>(player, 0x10) == g_menu.depot && bar_items(player)[FIRST_ICON] == g_menu.first_icon;
}

// Every unit's army, counter or time as the panel would show them now (the army by Player_SetPanel's own
// test, Player_CanBuild).
void refresh_menu(void* player) {
    const uint32_t now = game_ms();
    for (Slot& s : g_menu.slots) {
        uint8_t* vid = vid_by_number(s.nvid);
        if (!vid || at<int>(vid, 4) == 0) continue;
        char text[32] = "";
        int army = 2;
        s.limited = false;
        if (s.later) {
            const int left = static_cast<int>(s.at - now);
            const int seconds = left > 0 ? (left + 999) / 1000 : 0;
            std::snprintf(text, sizeof text, "%d:%02d", seconds / 60, seconds % 60);
        } else {
            army = reinterpret_cast<CanBuild_t>(PLAYER_CAN_BUILD)(player, nullptr, vid) ? 3 : 2;
            s.limited = unit_limit(player, vid, s.used, s.max);
            if (s.limited) std::snprintf(text, sizeof text, "%d/%d", s.used, s.max);
        }
        if (s.icon && army_of(s.icon) != army) reinterpret_cast<SetInt_t>(OBJECT_SET_ARMY)(s.icon, nullptr, army);
        if (s.note && s.text != text) {
            set_text(s.note, text);
            for (uint8_t* o : s.outline) set_text(o, text);
            s.text = text;
        }
    }
}

// After Player_SetPanel made the factory's list: the later units in the engine's empty slots, the second
// row, the corners' texts.
void make_menu(void* player) {
    if (panel_of(player) != PANEL_DEPOT_LIST || bar_count(player) != PANEL_OBJECTS) return;
    uint8_t* depot = at<uint8_t*>(player, 0x10);
    if (!depot || at<int>(at<uint8_t*>(depot, 0x1c), 0x10) != B_DEPO) return;
    if (at<uint8_t*>(bar_items(player)[FIRST_ICON], 0x1c) != interface_vid(455, interface_scale()) ||
        at<uint8_t*>(bar_items(player)[STRIP], 0x1c) != interface_vid(497, interface_scale()))
        return;   // not the bar this code knows
    const int count = at<int>(depot, 0x94);             // Unit.items (ShortList)
    const short* list = at<short*>(depot, 0x9c);
    Shown shown;
    for (int i = 0; list && i < count && i < 2 * ROW; ++i) shown.units.push_back(list[i]);
    shown.listed = static_cast<int>(shown.units.size());
    const std::vector<Later> later = later_units(depot);
    for (const Later& l : later)
        if (shown.units.size() < 2 * ROW) shown.units.push_back(l.nvid);

    // the later units of the first row in the engine's empty slots, drawn by the engine's own code (the
    // icon's frame, the price), greyed; no key
    for (int e = shown.listed; e < ROW && e < static_cast<int>(shown.units.size()); ++e) {
        uint8_t* vid = vid_by_number(shown.units[e]);
        if (reinterpret_cast<DrawUnitIcon_t>(DRAW_UNIT_ICON)(player, nullptr, e, vid) == 0)
            reinterpret_cast<SetInt_t>(OBJECT_SET_ARMY)(bar_items(player)[FIRST_ICON + e], nullptr, 2);
    }
    const int second = add_second_row(player, shown);
    if (static_cast<int>(shown.units.size()) > ROW && second < 0) return;

    Menu m;
    m.player = player;
    m.depot = depot;
    if (list) m.list.assign(list, list + count);
    for (const Later& l : later) m.later.push_back(l.nvid);
    for (int e = 0; e < static_cast<int>(shown.units.size()); ++e) {
        uint8_t** items = bar_items(player);
        const bool first_row = e < ROW;
        Slot s;
        s.nvid = shown.units[e];
        s.later = e >= shown.listed;
        s.at = s.later ? later[e - shown.listed].at : 0;
        s.icon = first_row ? items[FIRST_ICON + e] : items[second + e - ROW];
        uint8_t* price = first_row ? items[FIRST_PRICE + e] : items[second + ROW + e - ROW];
        uint8_t* label = first_row ? items[FIRST_LABEL + e] : items[second + 2 * ROW + e - ROW];
        s.note = add_note(player, price, label, s.outline);
        m.slots.push_back(s);
    }
    m.bar = bar_count(player);
    m.first_icon = bar_items(player)[FIRST_ICON];
    g_menu = m;
    refresh_menu(player);
}

// The panel no longer shows the factory's list: a unit put into it (an unlock) or taken out, or a later
// unit no longer to come.
bool menu_stale() {
    uint8_t* depot = g_menu.depot;
    const int count = at<int>(depot, 0x94);
    const short* list = at<short*>(depot, 0x9c);
    if (count != static_cast<int>(g_menu.list.size())) return true;
    for (int i = 0; list && i < count; ++i)
        if (list[i] != g_menu.list[i]) return true;
    const std::vector<Later> later = later_units(depot);
    if (later.size() != g_menu.later.size()) return true;
    for (size_t i = 0; i < later.size(); ++i)
        if (later[i].nvid != g_menu.later[i]) return true;
    return false;
}

// The panel's objects (bar objects from 80) s times as far from the first slot's type icon as the engine
// put them; the strip (VID 497, centred on the screen by the engine) only in height.
void scale_panel(void* player, int s) {
    const int count = bar_count(player);
    if (count <= FIRST_ICON) return;
    uint8_t** items = bar_items(player);
    uint8_t* anchor = items[FIRST_TYPE_ICON];
    const float ax = at<float>(anchor, 0x30), ay = at<float>(anchor, 0x34) - at<float>(anchor, 0x38);
    uint8_t* strip = interface_vid(497, s);
    for (int i = FIRST_ICON; i < count; ++i) {
        uint8_t* o = items[i];
        if (!o) continue;
        const float x = at<float>(o, 0x30), z = at<float>(o, 0x38), y = at<float>(o, 0x34) - z;
        const float nx = at<uint8_t*>(o, 0x1c) == strip ? x : ax + (x - ax) * s;
        const float ny = ay + (y - ay) * s;
        reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(o, nullptr, nx, ny + z, z);
    }
}

// The marker under the flagman's slot (bar object 20, VID 456): Player_SetPanel hides it by moving it 1000 px
// left of where it stands, and Player_UpdateStateBar puts it under the flagman's slot (an absolute place) when
// there is one. A bar built afresh has it in the middle of the screen, so after one SetPanel it stands at
// width / 2 - 1000 - on a screen wider than about 1960 px that is on the screen: a white frame on the bar's left
// part (2560: x 280), shown or not by how many SetPanel calls came since the bar was built (BUGS.md F31). After
// every SetPanel a marker that is under no slot's type icon goes far off the screen.
void hide_marker(void* player) {
    if (bar_count(player) <= FIRST_TYPE_ICON + ROW - 1) return;
    uint8_t** items = bar_items(player);
    uint8_t* marker = items[MARKER];
    if (!marker) return;
    const float x = at<float>(marker, 0x30);
    for (int i = 0; i < ROW; ++i)
        if (items[FIRST_TYPE_ICON + i] && at<float>(items[FIRST_TYPE_ICON + i], 0x30) == x) return;
    const float off = at<float>(world(), WORLD_CAMERA_X) - 100000.0f;   // far left of the view, whatever the screen
    if (x > off) reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(marker, nullptr, off, at<float>(marker, 0x34), at<float>(marker, 0x38));
}

void set_panel_laid_out(void* player, void* edx, int panel) {
    if (g_menu.player == player) g_menu = Menu();   // the engine takes the bar back to its 80 objects
    const int wanted = panel & ~0x10;
    const bool list = wanted == PANEL_DEPOT_LIST || wanted == PANEL_DEPOT_QUEUE;
    // With no bar (hidden with TAB, a factory clicked) Player_SetPanel(2) builds the bar itself (Player vtable
    // slot 7, status_bar.cpp) and then the list in the same call: the scale is the one that bar will get.
    const int s = list && bar_count(player) == 0 ? interface_choose_scale() : interface_scale();
    if (s <= 1 || !list) {
        g_set_panel(player, edx, panel);
        if (s > 1) scale_panel(player, s);   // a panel that falls back to the unit bar adds nothing
        make_menu(player);
        hide_marker(player);
        return;
    }
    // the army colours onto the bigger copies first: when this call builds the bar (no bar yet, status_bar.cpp
    // show()), World.vids below names the copies and show() would read them as the originals
    interface_copy_colours(s);
    // the engine makes the list's objects from World.vids: the scale's bigger copies meanwhile
    uint8_t** vids = reinterpret_cast<uint8_t**>(world() + 0x294);
    constexpr int SWAPPED[] = {455, 497, 8};
    uint8_t* kept[3] = {};
    for (int i = 0; i < 3; ++i) {
        kept[i] = vids[SWAPPED[i]];
        if (uint8_t* big = interface_vid(SWAPPED[i], s)) vids[SWAPPED[i]] = big;
    }
    g_set_panel(player, edx, panel);
    for (int i = 0; i < 3; ++i) vids[SWAPPED[i]] = kept[i];
    scale_panel(player, s);
    make_menu(player);
    hide_marker(player);
}

// Player_SetPanel. Under a zoom the panel is laid out in the window's pixels, as the bar is (the engine centres
// the factory list's strip, VID 497, on the screen's width): zoom_view.cpp sets the screen to the window's for it.
void __fastcall set_panel(void* player, void* edx, int panel) {
    void* token = zoom_view_layout_begin();
    set_panel_laid_out(player, edx, panel);
    zoom_view_layout_end(token);
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
    factory_log_panel(1);   // a build asked for meanwhile is the panel's (factory_log.cpp)
    g_handle_panel_action(player, edx, object, index);
    factory_log_panel(-1);
}

// Player_UpdateStateBar, every frame: the panel made again when the factory's list is another, and after
// the engine's update every unit's army and texts as they are now.
void __fastcall update_state_bar(void* player, void* edx, void* mouse) {
    if (menu_valid(player) && menu_stale()) {
        // Player_SetPanel(2) as a click calls it; without the bit "the bar shown for this panel" (0x10)
        // meanwhile, which only decides whether it plays the panel's click and hides such a bar
        uint32_t& panel = at<uint32_t>(player, 0x2c);
        const uint32_t shown_for_panel = panel & 0x10;
        panel &= ~0x10u;
        reinterpret_cast<SetPanel_t>(SET_PANEL)(player, nullptr, PANEL_DEPOT_LIST);
        panel |= shown_for_panel;
        patch::log("factory menu: the list of the factory at %.0f,%.0f changed while its panel was open - made again",
                   at<float>(at<uint8_t*>(player, 0x10), 0x30), at<float>(at<uint8_t*>(player, 0x10), 0x34));
    }
    g_update_state_bar(player, edx, mouse);
    if (menu_valid(player)) refresh_menu(player);
}

// World_GetMouseTipText: over a unit of the list with a limit or a time the tip, the unit's name
// ([MouseTips] MenuVid455Dir<frame>), says it too.
char** __fastcall mouse_tip_text(void* w, void* edx, char** out) {
    char** text = g_mouse_tip_text(w, edx, out);
    uint8_t* hover = at<uint8_t*>(world(), 0x24c + 0x14);   // World.menu.hover
    if (!text || !*text || !hover || !menu_valid(g_menu.player)) return text;
    for (const Slot& s : g_menu.slots) {
        if (s.icon != hover || (!s.later && !s.limited)) continue;
        const char* name = **text ? *text : ui_unit_name(s.nvid);
        const std::string tip =
            s.later ? ui_printf(ui_format("PlayFactoryUnlock", "%s - can be built in %s"), name ? name : "", s.text.c_str())
                    : ui_printf(ui_format("PlayFactoryLimit", "%s - at most %d on this map, %d built or queued"),
                                name ? name : "", s.max, s.used);
        char* value = const_cast<char*>(tip.c_str());
        reinterpret_cast<StringAssign_t>(STRING_ASSIGN)(text, nullptr, &value);
        break;
    }
    return text;
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
    if (!g_set_panel) return;
    // mov eax,fs:[0] / push -1 / push 0x473d60
    const uint8_t update_start[] = {0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xFF, 0x68, 0x60, 0x3D, 0x47, 0x00};
    g_update_state_bar = reinterpret_cast<UpdateStateBar_t>(patch::hook(
        UPDATE_STATE_BAR, update_start, sizeof update_start, reinterpret_cast<void*>(&update_state_bar),
        "Player_UpdateStateBar"));
    // push -1 / push 0x473237 / mov eax,fs:[0]
    const uint8_t tip_start[] = {0x6A, 0xFF, 0x68, 0x37, 0x32, 0x47, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00};
    g_mouse_tip_text = reinterpret_cast<MouseTipText_t>(patch::hook(
        MOUSE_TIP_TEXT, tip_start, sizeof tip_start, reinterpret_cast<void*>(&mouse_tip_text),
        "World_GetMouseTipText"));
}
