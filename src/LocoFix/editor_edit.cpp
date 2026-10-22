// Editing the objects of the held level (editor mode, editor.cpp): select, move, turn, change
// the army, delete, undo and redo, and place new objects from a bar at the bottom of the view.
//
// Scripts and object updates are off while the level is held, so all of it is done here, with
// the engine's own pieces:
//  - the input comes through the editor's window procedure (editor.cpp, in front of the game's),
//    the mouse position in logical pixels (screen_scaling.cpp) plus the camera,
//  - after Graphics_DrawScene (0x40f0c0) the overlay is drawn with the engine's drawing calls:
//    the selection circle VID 460 ("Select Unit Circle", outline frame; the filled one marks
//    what a click would delete) under the selected objects, a preview of the object to place
//    (Graphics_DrawVid of the VID itself, the frame of its direction), the bar in the state
//    bar's look - its base (VID 450) and side tiles (VID 938) across the view, the icons in the
//    base's ten slots and in the depot panel's strip (VID 497) above them, the chosen one framed
//    (VID 456); the icons from the state bar's atlas (VID 455: the cross 3, the depot 10, the
//    wrench 2, the cannon 36 - the gun's own iconFrame 29 is an empty cell -, the locomotive 11)
//    and the editor's own (VID 939, made from the game's sprites by patch_data.py), their keys in
//    the depot panel's small font (VID 8) - and a status line in the mouse tips' font (VID 6),
//  - the cursor (Cursor_SetCursor): select 7, move 2 while dragging, delete 14 (export.lgc's
//    CURSOR_DELETE says 15, which is "capture" in the cursor table - BUGS.md),
//  - every change goes through what the engine has for it: Object_MoveTo (ACT_CHANGE_COOR, z
//    from the ground: World_GetGroundZForVid, keeping the object's height above it),
//    Object_SetDirection, ACT_SET_ARMY, ACT_UNDO_REMOVE / ACT_UNDO_INSERT (132/133, made for the
//    authors' MapEdit: the object leaves the layer lists - not drawn, not updated, not saved -
//    and comes back as it was; an engine leaves and takes its rail points and couplings),
//    World CreateObject (slot 8) for a new object and AskPlace (161) to refuse a place that an
//    object with a common moveMask already takes.
// The undo history keeps the state of every object a change touched before and after it
// (position, direction, army, present, semaphore red), so undo applies the "before" states and
// redo the "after" ones. Objects taken away stay allocated (undo needs them); they go with the
// level.
//
// Rules of MAP_RULES.md applied: a new unit gets the behave and the build list (items) of an
// object of the same VID already on the map, a new depot without one the game's 14 depot units
// (5 10 20 25 80 85 45 30 35 82 97 90 75 62); a fort comes with its two guns (VID 115) on its
// wall at the places the missions use for its direction, and they move, turn, change army and
// go with it; a red semaphore is one with the command 10,0,0,0 (ANI_STAND_OPEN, shown here
// with that animation); a depot, workshop or fort far from a track gets a warning. Rails are not
// moved or turned (the Track tool lays them); train cars stand on the track and move, turn round,
// couple and uncouple with their train ("trains", below).
//
// Keys: click selects (the bar picks a tool), drag moves, R / E or the wheel turn, A changes
// the army (0-3), T switches a semaphore red / green, Del deletes, Ctrl+Z / Ctrl+Y undo / redo,
// 1-0 pick the entries of the bar, [ and ] the decal to place, Esc or the right button go back
// to selecting; the Delete section entry of the strip deletes a rail's section with a click.
// The Push line tool of the strip (below, "push lines") picks two rail points for a line and
// checks it as SetPushLine does; T switches its flag, R turns it round, Del deletes it and S
// puts its semaphores. The gear of the strip opens File > Mission settings (editor.cpp).
// A factory, repair point, post or money crate snaps to its place on the track ("on the track")
// and a factory or repair point brings its push line and posts; the armies' colours are set as the
// mission script would ("the armies' colours"); outside the map a grid, at its edge a frame.
// Every text shown is looked up in [Editor] of the language pack (editor_text.h); the messages go
// to LocoFix.log in English.
#include "patch.h"
#include "editor_ground.h"
#include "editor_mission.h"
#include "editor_text.h"
#include "ground.h"
#include "rail_rules.h"
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>
#include <vector>

void window_to_logical(POINT& p);   // screen_scaling.cpp
const char* object_name(int nvid);  // editor_panel.cpp: the palette's names of the objects
bool palette_state(int nvid, int& behave, std::vector<short>& items);   // editor_panel.cpp
int palette_icon(int nvid);                                             // editor_panel.cpp
void panel_open_palette(bool vehicles);   // editor_panel.cpp: All objects, or the vehicles
bool editor_level_held();           // editor.cpp
void editor_mission_settings();     // editor.cpp: File > Mission settings...
// editor_panel.cpp: the Help and Menu buttons of the bar and their panels
void panel_draw(POINT mouse);
const char* panel_tip(POINT mouse);
bool panel_message(HWND window, UINT m, WPARAM w, POINT mouse);
bool panel_open();
void panel_reset();

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t G_CURSOR = 0x4a9d44;
constexpr uintptr_t G_OBJECT_MAP = 0x4a9c10;
constexpr uintptr_t DRAW_SCENE = 0x40f0c0;
constexpr uintptr_t DRAW_VID = 0x411a00;
constexpr uintptr_t FILL_RECT = 0x412cd0;
constexpr uintptr_t PRINTF = 0x412e70;
constexpr uintptr_t OBJECT_MOVE_TO = 0x446640;
constexpr uintptr_t OBJECT_SET_DIRECTION = 0x448660;
constexpr uintptr_t OBJECT_SET_ANIMATION = 0x4488c0;
constexpr uintptr_t COMMAND_PUSH = 0x4022b0;
constexpr uintptr_t GROUND_Z_FOR_VID = 0x420f70;
constexpr uintptr_t ASK_PLACE = 0x4174d0;
constexpr uintptr_t CURSOR_SET = 0x4256b0;

// World
constexpr size_t W_CAMERA_X = 0x44, W_CAMERA_Y = 0x48, W_LAYERS = 0x4c, W_VID_COUNT = 0x290, W_VIDS = 0x294;
constexpr size_t W_VTABLE_CREATE = 0x20, W_RESOURCE_PATH = 0x1c, W_LEVEL_FILE = 0x14;
constexpr int LAYERS = 16;
constexpr size_t LIST_SIZE = 0x10, LIST_COUNT = 0x04, LIST_ITEMS = 0x0c;
// Graphics: the view rectangle (logical pixels)
constexpr size_t G_VIEW_LEFT = 0x220, G_VIEW_RIGHT = 0x224, G_VIEW_TOP = 0x228, G_VIEW_BOTTOM = 0x22c;
// GameObject
constexpr size_t O_VID = 0x1c, O_FLAGS = 0x28, O_X = 0x30, O_Y = 0x34, O_Z = 0x38, O_UPLINK = 0x44;
constexpr size_t O_DIRECTION = 0x4c, O_COMMANDS = 0x58, O_BEHAVE = 0x8c, O_ITEMS_COUNT = 0x94, O_ITEMS = 0x9c;
constexpr size_t LIST_COMMANDS_COUNT = 0x04, LIST_COMMANDS_ITEMS = 0x0c;   // CommandList, Command = 4 ints
constexpr uint32_t REMOVED = 0x100;
// Rail, Engine, RailDot (re/types.txt)
constexpr size_t R_NODE0 = 0x78, R_NODE1 = 0x7c, E_FRONT_DOT = 0xc8, E_REAR_DOT = 0xd8;
constexpr size_t E_FLAGS = 0xa0, E_PREV = 0xa4, E_NEXT = 0xa8;   // Engine: engineFlags, prevCar, nextCar
constexpr size_t POS_DOT = 0x0, POS_DISTANCE = 0x4, POS_LINK = 0xc;   // RailPos
constexpr size_t LINK_DOT = 0x0, LINK_LENGTH = 0x4, LINK_DIRECTION = 0x10;   // RailLink
constexpr uintptr_t ENGINE_UPDATE_POSITION = 0x408c00, ENGINE_LEAVE_DOTS = 0x40a1e0, ENGINE_OCCUPY_DOTS = 0x40a2d0,
                    ENGINE_ATTACH = 0x409270, TRAIN_REVERSE = 0x409930, ENGINE_PUT_ON_RAILS = 0x4095b0;
constexpr size_t DOT_LINK_COUNT = 0x18, DOT_LINKS = 0x1c, DOT_LINK_SIZE = 0x14, DOT_X = 0xe0, DOT_Y = 0xe4, DOT_Z = 0xe8;
constexpr size_t DOT_REF_COUNT = 0x0, DOT_BROKEN = 0x4;
constexpr uintptr_t G_RAIL_MAP = 0x5ced30;   // RailMap: dots at +0x10 (PtrArray: count +4, items +0xc)
constexpr size_t RAIL_MAP_COUNT = 0x14, RAIL_MAP_ITEMS = 0x1c;
constexpr size_t W_GRID = 0x204, W_GRID_WIDTH = 0x208, W_GRID_HEIGHT = 0x20c;
constexpr uintptr_t DOT_RELEASE = 0x432b00, DOT_REMOVE_LINK = 0x433880, SET_GROUND_Z = 0x421170;
constexpr int ACT_CHANGE_DIRECTION = 60;
constexpr int VID_RAIL = 610;
constexpr size_t V_DIR_HALF_STEP = 0x340;
// Vid
constexpr size_t V_NVID = 0x04, V_CLASS = 0x10, V_FLAGS = 0x14, V_SIZE_X = 0x1c, V_SIZE_Y = 0x20, V_SIZE_Z = 0x24;
constexpr size_t V_ND_DIR = 0x64, V_ANIM_FRAMES = 0x68, V_MOVE_MASK = 0x18, V_FRAME_COUNT = 0x2a4;
constexpr uint32_t VID_SCREEN = 0x800, VID_HIDDEN = 0x400;
// classes (B_... of export.lgc)
constexpr int B_UNIT = 2, B_BUILDING = 3, B_AVIA = 4, B_FRAME = 10, B_LINKER = 12, B_TEXT = 19, B_CIV_ROBOT = 20,
              B_ENGINE = 21, B_RAIL = 22, B_DEPO = 24, B_CREATURE = 25, B_BALLOON = 26;
// actions (ACT_... of export.lgc)
constexpr int ACT_ADD_ITEM = 54, ACT_DELETE_ALL_ITEM = 57, ACT_CLEAR_COMMANDS = 0x48, ACT_SET_BEHAVE = 95,
              ACT_SET_ARMY = 97, ACT_UNDO_REMOVE = 132, ACT_UNDO_INSERT = 133;
constexpr int ANI_STAND = 0, ANI_STAND_OPEN = 10;
// cursor types (the cursor table of Cursor_Show)
constexpr int CURSOR_ARROW = 0, CURSOR_MOVE = 2, CURSOR_SELECT = 7, CURSOR_DELETE = 14;
// VIDs
constexpr int VID_GROUND = 1024, VID_DEPOT = 102, VID_WORKSHOP = 104, VID_GUN = 115, VID_SEMAPHORE = 152,
              VID_FORT = 162, VID_LOCOMOTIVE = 5, VID_CIRCLE = 460, VID_ICONS = 455, VID_CRATE = 169;
// the bar: the state bar's base, its side tiles (game/objects.override), the depot panel's strip,
// the frame of the chosen slot, the editor's icons (patch_data.make_editor_icons) and two fonts:
// the small one of the depot panel's keys and the mouse tips' one; the palette's icons (the chosen decal's)
constexpr int VID_BAR = 450, VID_BAR_SIDE = 938, VID_STRIP = 497, VID_SLOT_FRAME = 456, VID_EDITOR_ICONS = 939,
              VID_PALETTE_ICONS = 944, FONT_KEYS = 8, FONT_TIPS = 6;
constexpr short DEPOT_UNITS[] = {5, 10, 20, 25, 80, 85, 45, 30, 35, 82, 97, 90, 75, 62};
// menu ids of editor.cpp's Edit menu
constexpr int ID_UNDO = 40030, ID_REDO = 40031, ID_DELETE = 40032, ID_ROTATE_RIGHT = 40033, ID_ROTATE_LEFT = 40034,
              ID_ARMY = 40035, ID_SEMAPHORE = 40036;

using DrawScene_t = void(__fastcall*)(void* g, void* edx, int draw);
using DrawVid_t = void(__fastcall*)(void* g, void* edx, void* vid, int frame, float x, float y, float z);
using FillRect_t = void(__fastcall*)(void* g, void* edx, float l, float t, float r, float b, uint32_t argb);
using Printf_t = void(__cdecl*)(void* g, float x, float y, const char* format, ...);
using MoveTo_t = void(__fastcall*)(void* o, void* edx, float x, float y, float z);
using SetInt_t = void(__fastcall*)(void* o, void* edx, int value);
using Push_t = void(__fastcall*)(void* list, void* edx, int act, int v1, int v2, int v3);
using GroundZ_t = float(__fastcall*)(void* world, void* edx, void* vid, float x, float y);
using AskPlace_t = uint8_t*(__fastcall*)(void* map, void* edx, void* vid, float x, float y, float z);
using Create_t = uint8_t*(__fastcall*)(void* world, void* edx, void* vid, float x, float y, float z, unsigned dir,
                                       void* parent);
using Action_t = int(__fastcall*)(void* o, void* edx, int act, int v1, int v2, int v3);
using DotRelease_t = void(__fastcall*)(void* dot, void* edx);
using DotLink_t = void(__fastcall*)(void* dot, void* edx, void* other);
using SetGroundZ_t = void(__fastcall*)(void* world, void* edx, float x, float y, float z);

DrawScene_t g_draw_scene = nullptr;
bool g_test = false;

template <typename T> T& at(void* base, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}

uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }
void* graphics() { return *reinterpret_cast<void**>(G_GRAPHICS); }
uint8_t* vid_of(uint8_t* o) { return at<uint8_t*>(o, O_VID); }
int nvid_of(uint8_t* o) { return at<int>(vid_of(o), V_NVID); }
int class_of(uint8_t* o) { return at<int>(vid_of(o), V_CLASS); }
int army_of(uint8_t* o) { return (at<uint32_t>(o, O_FLAGS) >> 10) & 3; }
bool removed(uint8_t* o) { return (at<uint32_t>(o, O_FLAGS) & REMOVED) != 0; }

uint8_t* vid_by_number(int nvid) {
    uint8_t* w = world();
    if (!w || nvid < 0 || nvid >= at<int>(w, W_VID_COUNT)) return nullptr;
    return reinterpret_cast<uint8_t**>(w + W_VIDS)[nvid];
}

int act(uint8_t* o, int action, int v1 = 0) {
    return reinterpret_cast<Action_t>(at<void**>(o, 0)[1])(o, nullptr, action, v1, 0, 0);
}

float ground_z(uint8_t* vid, float x, float y) {
    return reinterpret_cast<GroundZ_t>(GROUND_Z_FOR_VID)(world(), nullptr, vid, x, y);
}

bool is_unit_class(int c) {
    return c == B_UNIT || c == B_BUILDING || c == B_AVIA || c == B_CIV_ROBOT || c == B_ENGINE || c == B_DEPO ||
           c == B_CREATURE || c == B_BALLOON;
}

// Every object of the level, the ones in the layer lists (not the removed ones).
template <typename F> void for_each_object(F f) {
    uint8_t* w = world();
    for (int layer = 0; layer < LAYERS; ++layer) {
        uint8_t* list = w + W_LAYERS + layer * LIST_SIZE;
        const int count = at<int>(list, LIST_COUNT);
        uint8_t** items = at<uint8_t**>(list, LIST_ITEMS);
        for (int i = 0; i < count; ++i)
            if (items && items[i]) f(items[i]);
    }
}

// ---- semaphores: red is the command 10,0,0,0 ----

bool red(uint8_t* o) {
    if (nvid_of(o) != VID_SEMAPHORE) return false;
    uint8_t* list = o + O_COMMANDS;
    const int count = at<int>(list, LIST_COMMANDS_COUNT);
    const int* items = at<int*>(list, LIST_COMMANDS_ITEMS);
    for (int i = 0; i < count && items; ++i)
        if (items[4 * i] == ANI_STAND_OPEN) return true;
    return false;
}

void set_red(uint8_t* o, bool on) {
    act(o, ACT_CLEAR_COMMANDS);
    if (on) reinterpret_cast<Push_t>(COMMAND_PUSH)(o + O_COMMANDS, nullptr, ANI_STAND_OPEN, 0, 0, 0);
    reinterpret_cast<SetInt_t>(OBJECT_SET_ANIMATION)(o, nullptr, on ? ANI_STAND_OPEN : ANI_STAND);
}

// ---- the armies' colours ----
//
// The colours of the armies are not in objects.res: the mission script sets them when the level
// starts - SetArmyGamma() of maps\script\setarmygamma.lgc, called by InitDefault - with
// SetVid(nvid, VID_GAMMA + army, value) for the units, the buildings' flags (VID 175, 770-774) and
// the state bar's icons (not set here: the editor's bar draws them untinted). A held level runs no
// script, so every army looked the same; the editor sets the same gammas (SetVidData 18-21: Gamma_FromScriptInt, then the VID's SetGamma, vtable
// slot 7). The buildings' flags are their linked objects (Object_CreateLinked: the fort and the
// mine link VID 175, the factory and the repair point a light that links it), coloured by army.
const char* const ARMY_NAMES[] = {"ours", "enemy", "neutral", "pirates"};   // the script's order
const char* const ARMY_KEYS[] = {"ArmyOurs", "ArmyEnemy", "ArmyNeutral", "ArmyPirates"};
const char* army_name(int army) { return ui_text(ARMY_KEYS[army & 3], ARMY_NAMES[army & 3]); }
// the same as the context panel's choices
const char* const ARMY_CHOICES[] = {"OURS", "ENEMY", "NEUTRAL", "PIRATES"};
const char* const ARMY_CHOICE_KEYS[] = {"CtxOurs", "CtxEnemy", "CtxNeutral", "CtxPirates"};
const uint32_t ARMY_GAMMA[4] = {0, 0xe83917, 0xf40059, 0x2e451f};
const uint32_t FLAG_GAMMA[4] = {0x7e007e, 0x007e7e, 0x00007e, 0x686868};

const int ARMY_GAMMA_VIDS[] = {5,  10, 15, 20, 21, 25, 26, 27, 28, 30, 31, 35, 40, 41, 42,  43,  45,  50, 55,
                               56, 60, 62, 63, 70, 71, 75, 76, 80, 81, 82, 83, 84, 85, 90, 91, 97, 110, 112, 115, 116, 117};
const int FLAG_VIDS[] = {175, 770, 771, 772, 773, 774};

struct Gamma {
    uint32_t darken, brighten;
};

// Gamma_FromScriptInt (0x442300): a byte with bit 7 clear darkens by twice its 7 bits, one with
// bit 7 set brightens by twice the 7 bits of its complement.
Gamma gamma_from_script(uint32_t v) {
    Gamma g = {0, 0};
    for (int k = 0; k < 4; ++k) {
        const uint32_t sign = 0x80u << (8 * k), mask = 0x7fu << (8 * k);
        if (v & sign) g.brighten |= (~v & mask) << 1;
        else g.darken |= (v & mask) << 1;
    }
    return g;
}

using VidSetGamma_t = void(__fastcall*)(void* vid, void* edx, Gamma* gamma, unsigned army);

void set_vid_gamma(int nvid, int army, uint32_t value) {
    uint8_t* v = vid_by_number(nvid);
    if (!v) return;
    Gamma g = gamma_from_script(value);
    reinterpret_cast<VidSetGamma_t>(at<void**>(v, 0)[7])(v, nullptr, &g, static_cast<unsigned>(army));
}

void set_army_gammas() {
    for (int army = 0; army < 4; ++army) {

        for (int nvid : ARMY_GAMMA_VIDS) set_vid_gamma(nvid, army, ARMY_GAMMA[army]);
        set_vid_gamma(95, army, ARMY_GAMMA[army == 3 ? 2 : army]);   // as the script has it
        for (int nvid : FLAG_VIDS) set_vid_gamma(nvid, army, FLAG_GAMMA[army]);
    }
}

// ---- object state and the undo history ----

struct State {
    float x, y, z;
    int direction, army;
    bool present, red;
    // a train car: its couplings, its two ends on the rails (RailPos), turned round or not, and the
    // patrol the editor keeps with it (written to the mission's script, editor_mission.h)
    bool car = false;
    uint8_t* prev = nullptr;
    uint8_t* next = nullptr;
    uint32_t car_flags = 0;
    int front[4] = {}, rear[4] = {};
    bool patrol = false;
    int patrol_x = 0, patrol_y = 0;
    // a region: its size, flags, fog and what it turns explosions into (regions, below)
    bool region = false;
    float region_w = 0, region_h = 0;
    uint32_t region_flags = 0, fog_colour = 0;
    int fog_low = 0, fog_high = 0, splash = 0;
};

// ---- regions ----
//
// A region (B_REGION, VIDs 750-752; Region_Action reads and saves 80 bytes of it - DEVELOPERS.md "Data
// formats") is a rectangle of the map: width and height around its place, the rectangle drawn at
// y - z. What it does, as Region_Draw, Region_InitFog and Vid_GetTerrainVariant show:
//  - fog: between two heights (low, high) of the ground's picture, in its colour (ARGB), over the
//    rectangle or - flag 8 - over the whole view, pulsing with flag 1 (Graphics_DrawFog);
//  - an image tiled over it (its VID's own: 750 is the light water's picture);
//  - six pairs of VIDs: an object of the first VID created in it, less than 25 above it, is made of
//    the second instead - the missions turn the big and the small explosion (308, 309) into the
//    explosion on water (338) or on lava (528) there (World_CreateObject);
//  - 750 and 752 grow rain circles (VID 339) while it rains (daynight.lgc: SetVidData(VID_CHILD)).
// The missions' 41 kinds: plain markers (751 with nothing set), splash regions over lakes and lava
// (751 with the pairs, often flag 8), fog regions (l01, l05, l07, l13, l18), rain circles (752).
constexpr size_t RG_FOG_TABLE = 0x78, RG_FLAGS = 0x7c, RG_HIGH = 0x80, RG_LOW = 0x84, RG_COLOUR = 0x88,
                 RG_WIDTH = 0x90, RG_HEIGHT = 0x94, RG_FROM = 0xa0, RG_TO = 0xb8;
constexpr uintptr_t REGION_INIT_FOG = 0x435c50;
constexpr int B_REGION = 23, VID_REGION = 751, VID_REGION_RAIN = 752, VID_REGION_WATER = 750;
constexpr int SPLASH_FROM[] = {308, 309}, SPLASH_TO[] = {0, 338, 528};   // none, water, lava
constexpr uint32_t VID_FLAG_VARIANT = 0x2000000;   // Vid.flags: Vid_GetTerrainVariant looks at it
using InitFog_t = void(__fastcall*)(void* region, void* edx, int low, int high, int colour);

// The water the missions lay (MAP_RULES.md 2): animated tiles of 128 x 128 (GroundVid with flag
// 0x10000: layer 6, drawn after the ground with the depth test) on a grid of 128 from (64, 64), all
// at one height per map - the level of the water -, over a ground whose picture is lower than it
// where the water shows. Light water 204, dark water 199, swamp water 205 and 229, lava 293.
struct WaterKind {
    int nvid;
    const char* key;
    const char* english;
    int bed_texture;            // index in editor\textures.tsv's order: 11 lake bed, 10 swamp, 9 lava
    int effect, effect_spacing; // the generator over it and how far apart (the missions')
    int second, second_spacing; // a second one (0: none)
    int splash;                 // the region's pairs: 1 water, 2 lava
    int below;                  // a new water's level: this much below the ground
};
const WaterKind WATER_KINDS[] = {
    {204, "WaterLight", "light water", 11, 504, 60, 510, 600, 1, 20},
    {199, "WaterDark", "dark water", 11, 504, 60, 510, 600, 1, 20},
    {205, "WaterSwamp", "swamp water", 10, 206, 200, 0, 0, 1, 16},
    {229, "WaterSwamp2", "swamp water 2", 10, 206, 200, 337, 220, 1, 16},
    {293, "WaterLava", "lava", 9, 441, 300, 206, 400, 2, 12},
};
constexpr int WATER_KIND_COUNT = sizeof WATER_KINDS / sizeof WATER_KINDS[0];

bool is_water_tile(int nvid) {
    for (const WaterKind& k : WATER_KINDS)
        if (k.nvid == nvid) return true;
    return false;
}

bool is_region(uint8_t* o);
int region_splash(uint8_t* o) {
    uint8_t* to = at<uint8_t*>(o, RG_TO);
    const int n = to ? at<int>(to, V_NVID) : 0;
    return n == SPLASH_TO[1] ? 1 : n == SPLASH_TO[2] ? 2 : 0;
}

void set_region_splash(uint8_t* o, int splash) {
    for (int i = 0; i < 6; ++i) at<uint8_t*>(o, RG_FROM + 4 * i) = at<uint8_t*>(o, RG_TO + 4 * i) = nullptr;
    if (!splash) return;
    for (int i = 0; i < 2; ++i) {
        uint8_t* from = vid_by_number(SPLASH_FROM[i]);
        uint8_t* to = vid_by_number(SPLASH_TO[splash]);
        if (!from || !to) continue;
        at<uint8_t*>(o, RG_FROM + 4 * i) = from;
        at<uint8_t*>(o, RG_TO + 4 * i) = to;
        at<uint32_t>(from, V_FLAGS) |= VID_FLAG_VARIANT;   // as Region_Action does when it loads them
    }
}

void set_region_fog(uint8_t* o, int low, int high, uint32_t colour) {
    reinterpret_cast<InitFog_t>(REGION_INIT_FOG)(o, nullptr, low, high, static_cast<int>(colour));
}

// A train's patrol, kept with one of its cars: the rail point at the far end (its screen position,
// as Engine_CommandToPoint takes it). Engine_Action has no case for ACT_PATROL, so a command kept
// in the map would do nothing: the patrol is the script's PatrolEngine (editor_mission.h).
std::map<uint8_t*, std::pair<int, int>> g_patrols;

bool patrol_of(uint8_t* o, int& x, int& y) {
    const auto it = g_patrols.find(o);
    if (it == g_patrols.end()) return false;
    x = it->second.first;
    y = it->second.second;
    return true;
}

void set_patrol(uint8_t* o, bool on, int x, int y) {
    if (on) g_patrols[o] = {x, y};
    else g_patrols.erase(o);
}

State state_of(uint8_t* o) {
    State s{at<float>(o, O_X), at<float>(o, O_Y), at<float>(o, O_Z), at<int>(o, O_DIRECTION), army_of(o),
            !removed(o), red(o)};
    if (class_of(o) == B_ENGINE) {
        s.car = true;
        s.prev = at<uint8_t*>(o, E_PREV);
        s.next = at<uint8_t*>(o, E_NEXT);
        s.car_flags = at<uint32_t>(o, E_FLAGS);
        std::memcpy(s.front, o + E_FRONT_DOT, sizeof s.front);
        std::memcpy(s.rear, o + E_REAR_DOT, sizeof s.rear);
        s.patrol = patrol_of(o, s.patrol_x, s.patrol_y);
    }
    if (is_region(o)) {
        s.region = true;
        s.region_w = at<float>(o, RG_WIDTH);
        s.region_h = at<float>(o, RG_HEIGHT);
        s.region_flags = at<uint32_t>(o, RG_FLAGS);
        s.fog_low = at<int>(o, RG_LOW);
        s.fog_high = at<int>(o, RG_HIGH);
        s.fog_colour = at<uint32_t>(o, RG_COLOUR);
        s.splash = region_splash(o);
    }
    return s;
}

bool is_region(uint8_t* o) { return o && class_of(o) == B_REGION; }

void restore_region(uint8_t* o, const State& s) {
    at<float>(o, RG_WIDTH) = s.region_w;
    at<float>(o, RG_HEIGHT) = s.region_h;
    at<uint32_t>(o, RG_FLAGS) = s.region_flags;
    if (s.fog_low != at<int>(o, RG_LOW) || s.fog_high != at<int>(o, RG_HIGH) || s.fog_colour != at<uint32_t>(o, RG_COLOUR))
        set_region_fog(o, s.fog_low, s.fog_high, s.fog_colour);
    if (s.splash != region_splash(o)) set_region_splash(o, s.splash);
}

// ---- rails and their points ----
//
// Rail_Action has no case for ACT_UNDO_REMOVE / ACT_UNDO_INSERT: a rail taken away that way
// would leave its two points and their links in g_railMap, and trains would still drive there.
// So the rail's own link (between its points) is removed and its points are released
// (RailDot_Release: a point no other rail uses goes with its links), and putting it back runs
// ACT_CHANGE_DIRECTION again, which adds the points and links them the way loading does
// (Rail_Construct does the same for a new rail). A point a train car stands on, or one whose
// link table would change under a car (RailDot_RemoveLink moves the last link into the hole),
// is not touched: such a rail is not deleted.

uint8_t*& rail_node(uint8_t* r, int i) { return at<uint8_t*>(r, i ? R_NODE1 : R_NODE0); }

bool rail_held_by_train(uint8_t* r) {
    std::vector<uint8_t*> dots;
    for (int i = 0; i < 2; ++i) {
        uint8_t* d = rail_node(r, i);
        if (!d) continue;
        dots.push_back(d);
        const int links = at<int>(d, DOT_LINK_COUNT);
        for (int k = 0; k < links && k < 6; ++k) dots.push_back(at<uint8_t*>(d, DOT_LINKS + k * DOT_LINK_SIZE));
    }
    bool held = false;
    for_each_object([&](uint8_t* o) {
        if (held || class_of(o) != B_ENGINE) return;
        for (uint8_t* d : dots)
            if (at<uint8_t*>(o, E_FRONT_DOT) == d || at<uint8_t*>(o, E_REAR_DOT) == d) held = true;
    });
    return held;
}

void rail_take_out(uint8_t* r) {
    uint8_t* n0 = rail_node(r, 0);
    uint8_t* n1 = rail_node(r, 1);
    const auto remove_link = reinterpret_cast<DotLink_t>(DOT_REMOVE_LINK);
    if (n0 && n1) {
        remove_link(n0, nullptr, n1);
        remove_link(n1, nullptr, n0);
    }
    // the points other rails keep: broken only while a destroyed rail (611, 601) still uses one
    std::vector<uint8_t*> kept;
    for (uint8_t* n : {n0, n1})
        if (n && at<int>(n, DOT_REF_COUNT) > 1) kept.push_back(n);
    const auto release = reinterpret_cast<DotRelease_t>(DOT_RELEASE);
    if (n0) release(n0, nullptr);
    if (n1) release(n1, nullptr);
    rail_node(r, 0) = rail_node(r, 1) = nullptr;
    for (uint8_t* n : kept) {
        int broken = 0;
        for_each_object([&](uint8_t* o) {
            if (o != r && class_of(o) == B_RAIL && (nvid_of(o) == 611 || nvid_of(o) == 601) &&
                (rail_node(o, 0) == n || rail_node(o, 1) == n))
                broken = 1;
        });
        at<int>(n, DOT_BROKEN) = broken;
    }
}

void rail_put_back(uint8_t* r) { act(r, ACT_CHANGE_DIRECTION, at<int>(r, O_DIRECTION)); }

using EngineCall_t = void(__fastcall*)(void* car, void* edx);
using EngineWith_t = int(__fastcall*)(void* car, void* edx, void* other);

void car_call(uintptr_t function, uint8_t* car) { reinterpret_cast<EngineCall_t>(function)(car, nullptr); }

// A train car as it was: its couplings, its ends on the rails and its place between them
// (Engine_UpdatePosition puts it halfway and turns it, its turret with it), the points it stands
// on (Engine_LeaveDots / Engine_OccupyDots), its patrol.
void restore_car(uint8_t* o, const State& s) {
    car_call(ENGINE_LEAVE_DOTS, o);
    at<uint8_t*>(o, E_PREV) = s.prev;
    at<uint8_t*>(o, E_NEXT) = s.next;
    at<uint32_t>(o, E_FLAGS) = s.car_flags;
    std::memcpy(o + E_FRONT_DOT, s.front, sizeof s.front);
    std::memcpy(o + E_REAR_DOT, s.rear, sizeof s.rear);
    int x = 0, y = 0;
    if (patrol_of(o, x, y) != s.patrol || x != s.patrol_x || y != s.patrol_y) set_patrol(o, s.patrol, s.patrol_x, s.patrol_y);
    if (at<uint8_t*>(o, E_FRONT_DOT) && at<uint8_t*>(o, E_REAR_DOT)) car_call(ENGINE_UPDATE_POSITION, o);
    car_call(ENGINE_OCCUPY_DOTS, o);
}

void apply(uint8_t* o, const State& s) {
    const bool rail = class_of(o) == B_RAIL, car = class_of(o) == B_ENGINE;
    const bool inserted = s.present && removed(o);
    if (inserted) act(o, ACT_UNDO_INSERT);
    if (!car && (s.x != at<float>(o, O_X) || s.y != at<float>(o, O_Y) || s.z != at<float>(o, O_Z)))
        reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(o, nullptr, s.x, s.y, s.z);
    if (!car && s.direction != at<int>(o, O_DIRECTION))
        reinterpret_cast<SetInt_t>(OBJECT_SET_DIRECTION)(o, nullptr, s.direction);
    if (rail && inserted) rail_put_back(o);
    if (s.army != army_of(o)) act(o, ACT_SET_ARMY, s.army);
    if (s.red != red(o)) set_red(o, s.red);
    if (car && s.present && s.car) restore_car(o, s);
    if (s.present && s.region && is_region(o)) restore_region(o, s);
    if (!s.present && !removed(o)) {
        if (rail) rail_take_out(o);
        act(o, ACT_UNDO_REMOVE);
    }
}

// ---- the ground grid ----

short* grid() { return at<short*>(world(), W_GRID); }
int grid_width() { return at<int>(world(), W_GRID_WIDTH); }
int grid_height() { return at<int>(world(), W_GRID_HEIGHT); }

short ground_cell(int cx, int cy) { return grid() ? grid()[cy * grid_width() + cx] : 0; }

// Through the engine's World_SetGroundZ (the cell of the point), at the middle of the cell.
void set_ground_cell(int cx, int cy, short z) {
    reinterpret_cast<SetGroundZ_t>(SET_GROUND_Z)(world(), nullptr, cx * 8 + 4.0f, cy * 8 + 4.0f, z);
}

struct GridChange {
    int cx, cy;
    short before, after;
};

struct Change {
    uint8_t* object;
    State before, after;
};

struct Operation {
    UiText what;
    std::vector<Change> changes;
    std::vector<GridChange> grid;   // ground heights a track changed
    bool lines = false;             // the push lines changed (editor_mission.cpp)
    std::vector<PushLine> lines_before, lines_after;
    std::shared_ptr<PaintPatch> paint;   // the ground's picture a stroke changed (editor_ground.cpp)
};

std::vector<Operation> g_undo, g_redo;
bool g_dirty = false;
std::vector<uint8_t*> g_selection;   // the selected object and its group (a fort's guns)
std::string g_message;               // shown in the status line for a while
DWORD g_message_time = 0;

// A message of the status line for a while, in the pack's language; LocoFix.log gets it in English
// (tests read it): `build` makes the text twice, the second time with the texts in English.
template <typename F> void message_by(F build) {
    g_message = build();
    g_message_time = GetTickCount();
    const bool was = ui_english(true);
    const std::string english = build();
    ui_english(was);
    patch::log("edit: %s", english.c_str());
}
#define MESSAGE(key, english, ...) message_by([&] { return ui_printf(ui_format(key, english), __VA_ARGS__); })

// Records what `objects` were (`before`) and are now as one step of the history.
void commit(const UiText& what, const std::vector<uint8_t*>& objects, const std::vector<State>& before) {
    Operation op{what, {}, {}, false, {}, {}};
    for (size_t i = 0; i < objects.size(); ++i) op.changes.push_back({objects[i], before[i], state_of(objects[i])});
    g_undo.push_back(op);
    g_redo.clear();
    g_dirty = true;
}

// The same for the push lines: they were `before` and are now the mission's.
void commit_lines(const UiText& what, const std::vector<PushLine>& before) {
    Operation op{what, {}, {}, true, before, mission_push_lines()};
    g_undo.push_back(op);
    g_redo.clear();
    g_dirty = true;
}

std::vector<State> states(const std::vector<uint8_t*>& objects) {
    std::vector<State> s;
    for (uint8_t* o : objects) s.push_back(state_of(o));
    return s;
}

void select(uint8_t* o);
void end_line();
// on the track (below)
bool on_track_vid(int nvid);
bool snap(int nvid, float& x, float& y, int& direction, UiText& reason);
void give_building_line(uint8_t* o);
bool take_building_lines(uint8_t* o, std::vector<uint8_t*>& posts);

void undo(bool redo) {
    auto& from = redo ? g_redo : g_undo;
    auto& to = redo ? g_undo : g_redo;
    if (from.empty()) {
        if (redo) MESSAGE("MsgNothingToRedo", "nothing to redo");
        else MESSAGE("MsgNothingToUndo", "nothing to undo");
        return;
    }
    end_line();
    Operation op = from.back();
    from.pop_back();
    for (const Change& c : op.changes) apply(c.object, redo ? c.after : c.before);
    // a car taken away or put back (ACT_UNDO_REMOVE / INSERT) unlinks or relinks its neighbours:
    // the couplings of the cars of the step once more, as they are to be
    for (const Change& c : op.changes) {
        const State& s = redo ? c.after : c.before;
        if (s.car && s.present) {
            at<uint8_t*>(c.object, E_PREV) = s.prev;
            at<uint8_t*>(c.object, E_NEXT) = s.next;
        }
    }
    for (const GridChange& g : op.grid) set_ground_cell(g.cx, g.cy, redo ? g.after : g.before);
    if (op.paint) ground_apply(*op.paint, redo);
    if (op.lines) mission_push_lines() = redo ? op.lines_after : op.lines_before;
    to.push_back(op);
    g_dirty = true;
    select(nullptr);
    MESSAGE("MsgUndoRedo", "%s: %s", redo ? ui_text("WordRedo", "redo") : ui_text("WordUndo", "undo"), op.what.get());
}

// ---- hosts and their guns (sockets) ----
//
// Guns stand in sockets of their host, measured over the 20 missions (the gun's offset from the
// host, its height above it): the fort (162) has two on its wall - x +-47 for the directions 0 and
// 128, y +-33 for 64 and 192, (-32, -24) / (+33, +23) for 32 and 160, (-33, +23) / (+33, -24) for 96
// and 224 - 93 up; the mine (165) one of those, the one on the side its direction picks, 94 up; the
// citadel (149) one on its top, 112 up. Every one of the missions' 73 guns stands in one of these
// (40 on forts, 30 on mines, 3 on citadels); the pirate walls (141) near some forts carry none - the
// guns beside them are the forts'. A socket holds a tower with a cannon (110) or a missile tower
// (115) - the citadel the pirates' missile tower (117) - or nothing; its gun keeps the host's army
// and turns and moves with it (a citadel is army 0 in the missions while its tower is the enemy's:
// on one of army 0 the gun is the enemy's, gun_army). The missions' fits: a fort two missile towers
// (8, as many as a cannon and a missile tower; 4 with two cannons), a mine one cannon (17, 13 a
// missile tower), a citadel its tower.

constexpr int VID_MINE_BUILDING = 165, VID_CITADEL = 149;
constexpr int GUN_CANNON = 110, GUN_MISSILE = 115, GUN_PIRATE = 117;

struct Socket {
    float dx, dy, dz;
};

const float FORT_SOCKETS[4][2][2] = {{{-47, 0}, {47, 0}},
                                     {{-32, -24}, {33, 23}},
                                     {{0, -33}, {0, 34}},
                                     {{-33, 23}, {33, -24}}};
// the mine's one socket by its direction / 32 (measured: 0, 64, 96, 128, 160, 224; 32 and 192 the
// mirrored ones)
const float MINE_SOCKETS[8][2] = {{-46, 1}, {-33, -23}, {1, -31}, {34, -22}, {47, 2}, {34, 23}, {-1, 31}, {-33, 24}};

std::vector<Socket> sockets_of(int nvid, int direction) {
    const int row = ((direction + 16) & 255) / 32 % 4, eighth = ((direction + 16) & 255) / 32;
    std::vector<Socket> out;
    if (nvid == VID_FORT)
        for (int i = 0; i < 2; ++i) out.push_back({FORT_SOCKETS[row][i][0], FORT_SOCKETS[row][i][1], 93});
    else if (nvid == VID_MINE_BUILDING)
        out.push_back({MINE_SOCKETS[eighth][0], MINE_SOCKETS[eighth][1], 94});
    else if (nvid == VID_CITADEL)
        out.push_back({0, 0, 112});
    return out;
}

bool is_gun(int nvid) { return nvid == GUN_CANNON || nvid == GUN_MISSILE || nvid == GUN_PIRATE; }

int gun_army(int host, int host_army) {
    return host == VID_CITADEL && host_army == 0 ? 1 : host_army;
}

// What a socket of a host can hold (0: nothing) and what a new host comes with.
std::vector<int> gun_choices(int host) {
    return host == VID_CITADEL ? std::vector<int>{0, GUN_PIRATE} : std::vector<int>{0, GUN_CANNON, GUN_MISSILE};
}

std::vector<int> default_fit(int host) {
    if (host == VID_FORT) return {GUN_MISSILE, GUN_MISSILE};
    if (host == VID_MINE_BUILDING) return {GUN_CANNON};
    if (host == VID_CITADEL) return {GUN_PIRATE};
    return {};
}

// The gun in each socket of a host (nullptr where it is empty).
std::vector<uint8_t*> guns_on(uint8_t* host) {
    std::vector<uint8_t*> out;
    for (const Socket& s : sockets_of(nvid_of(host), at<int>(host, O_DIRECTION))) {
        const float gx = at<float>(host, O_X) + s.dx, gy = at<float>(host, O_Y) + s.dy;
        uint8_t* found = nullptr;
        for_each_object([&](uint8_t* g) {
            if (!found && !removed(g) && is_gun(nvid_of(g)) && std::fabs(at<float>(g, O_X) - gx) < 10 &&
                std::fabs(at<float>(g, O_Y) - gy) < 10)
                found = g;
        });
        out.push_back(found);
    }
    return out;
}

// ---- train cars ----

bool is_car(uint8_t* o) { return o && class_of(o) == B_ENGINE; }
uint8_t*& car_prev(uint8_t* c) { return at<uint8_t*>(c, E_PREV); }
uint8_t*& car_next(uint8_t* c) { return at<uint8_t*>(c, E_NEXT); }

// The cars of a car's train, from the first (no prevCar) to the last.
std::vector<uint8_t*> train_of(uint8_t* c) {
    uint8_t* first = c;
    for (int guard = 0; car_prev(first) && guard < 200; ++guard) first = car_prev(first);
    std::vector<uint8_t*> out;
    for (uint8_t* k = first; k && out.size() < 200; k = car_next(k)) out.push_back(k);
    return out;
}

// The object and what goes with it: a host's guns in their sockets, a car's train (the car first).
std::vector<uint8_t*> group_of(uint8_t* o) {
    if (is_car(o)) {
        std::vector<uint8_t*> train{o};
        for (uint8_t* k : train_of(o))
            if (k != o) train.push_back(k);
        return train;
    }
    std::vector<uint8_t*> group{o};
    for (uint8_t* g : guns_on(o))
        if (g) group.push_back(g);
    return group;
}

// A gun put into a socket of a host: its place, height and direction, the host's army.
uint8_t* fit_gun(uint8_t* host, const Socket& s, int nvid);
// ---- the gravel bed of the track ----
//
// The missions that have one put a decal of the bed under every rail (l02, l09, l15: VID 612,
// l18: 615 ice, l12: 616 volcanic, l11: 617 desert): the rail's place and direction, 18 below the
// rail. In play a decal is painted into the ground; in editor mode it is an object, which the
// Track tool lays and deletes with its rail.
constexpr int BEDS[] = {612, 615, 616, 617};
constexpr float BED_DEPTH = 18;
int g_bed = 612;   // the bed of new track, 0 none (B in the Track tool)

// a bed, or the embankment under a swamp rail (VID 602, below), which goes with its rail the same way
bool is_bed(int nvid) { return std::find(std::begin(BEDS), std::end(BEDS), nvid) != std::end(BEDS) || nvid == 602; }

// The bed under a rail (the same place and direction).
uint8_t* bed_of(uint8_t* r) {
    uint8_t* found = nullptr;
    for_each_object([&](uint8_t* o) {
        if (!found && !removed(o) && is_bed(nvid_of(o)) && std::fabs(at<float>(o, O_X) - at<float>(r, O_X)) < 1 &&
            std::fabs(at<float>(o, O_Y) - at<float>(r, O_Y)) < 1 && at<int>(o, O_DIRECTION) == at<int>(r, O_DIRECTION))
            found = o;
    });
    return found;
}

// ---- picking ----

struct Box {
    float l, t, r, b;
    bool contains(float x, float y) const { return x >= l && x <= r && y >= t && y <= b; }
};

// Where an object is drawn, roughly: its footprint around (x, y - z) and its height above.
bool is_region(uint8_t* o);
constexpr float REGION_BORDER = 8;   // a region is picked at its frame, not inside

Box box_of(uint8_t* o) {
    uint8_t* v = vid_of(o);
    const float sx = at<float>(o, O_X), sy = at<float>(o, O_Y) - at<float>(o, O_Z);
    if (is_region(o)) {   // its rectangle
        const float hw = at<float>(o, RG_WIDTH) / 2, hh = at<float>(o, RG_HEIGHT) / 2;
        return {sx - hw - REGION_BORDER, sy - hh - REGION_BORDER, sx + hw + REGION_BORDER, sy + hh + REGION_BORDER};
    }
    const float hw = std::fmax(at<float>(v, V_SIZE_X) / 2, 10.0f), hd = std::fmax(at<float>(v, V_SIZE_Y) / 2, 6.0f);
    const float h = std::fmax(at<float>(v, V_SIZE_Z), 6.0f);
    return {sx - hw, sy - hd - h, sx + hw, sy + hd};
}

bool pickable(uint8_t* o) {
    uint8_t* v = vid_of(o);
    if (!v || removed(o) || at<uint8_t*>(o, O_UPLINK) || o == *reinterpret_cast<uint8_t**>(G_CURSOR)) return false;
    const int c = at<int>(v, V_CLASS);
    if (is_bed(at<int>(v, V_NVID))) return false;   // a rail's gravel bed goes with the rail
    if (is_water_tile(at<int>(v, V_NVID))) return false;   // the Water tool's
    return at<int>(v, V_NVID) != VID_GROUND && !(at<uint32_t>(v, V_FLAGS) & VID_SCREEN) && c != B_FRAME &&
           c != B_TEXT && c != B_LINKER;
}

// A point at a region's frame (inside it the objects under the region are picked).
bool on_region_frame(uint8_t* o, float x, float y) {
    const Box b = box_of(o);
    return b.contains(x, y) && !(x > b.l + 2 * REGION_BORDER && x < b.r - 2 * REGION_BORDER && y > b.t + 2 * REGION_BORDER &&
                                 y < b.b - 2 * REGION_BORDER);
}

// The object at a point of the map (screen coordinates plus the camera): the smallest box that
// holds the point, the rails only when nothing else is there.
uint8_t* pick(float x, float y) {
    uint8_t* best = nullptr;
    float best_area = 0;
    bool best_rail = true;
    for_each_object([&](uint8_t* o) {
        if (!pickable(o)) return;
        const Box b = box_of(o);
        if (!b.contains(x, y) || (is_region(o) && !on_region_frame(o, x, y))) return;
        const bool rail = class_of(o) == B_RAIL;
        const float area = (b.r - b.l) * (b.b - b.t);
        if (!best || (best_rail && !rail) || (rail == best_rail && area < best_area)) {
            best = o;
            best_area = area;
            best_rail = rail;
        }
    });
    return best;
}

bool movable(uint8_t* o) { return class_of(o) != B_RAIL; }   // a car moves with its train along the track
bool deletable(uint8_t* o) { return class_of(o) != B_RAIL || !rail_held_by_train(o); }

// ---- the bar ----

enum class Tool { Select, Delete, Section, Place, Track, PushLine, Settings, Height, Decals, Paint, Water, Region };
constexpr int PALETTE_NVID = -2;   // the entry that places what the palette panel picked (editor_panel.cpp)
constexpr int VEHICLES_NVID = -3;  // the same for the vehicles' palette (the cars, below "trains")

struct Entry {
    const char* text;    // its [Editor] key
    const char* label;   // in English
    const char* key;   // shown on the icon (the small font has digits, F, [ and ])
    Tool tool;
    int nvid;     // 0: the decal chosen with [ and ]
    int atlas;    // VID_ICONS or VID_EDITOR_ICONS
    int icon;     // its frame
    int army;
    bool red;
};

// The first ten sit in the ten slots of the state bar's base (keys 1-0), the others in the
// strip above them, as the depot panel's units.
const Entry ENTRIES[] = {
    {"ToolSelect", "Select", "1", Tool::Select, -1, VID_EDITOR_ICONS, 0, 0, false},
    {"ToolDelete", "Delete", "2", Tool::Delete, -1, VID_ICONS, 3, 0, false},
    {"ToolTrack", "Track", "3", Tool::Track, VID_RAIL, VID_EDITOR_ICONS, 1, 0, false},
    {"ToolFactory", "Factory", "4", Tool::Place, VID_DEPOT, VID_ICONS, 10, 0, false},
    {"ToolRepairPoint", "Repair point", "5", Tool::Place, VID_WORKSHOP, VID_ICONS, 2, 0, false},
    {"ToolFort", "Fort", "6", Tool::Place, VID_FORT, VID_EDITOR_ICONS, 3, 1, false},
    {"ToolPalette", "All objects", "7", Tool::Place, PALETTE_NVID, VID_EDITOR_ICONS, 10, 0, false},
    {"ToolSemaphoreGreen", "Semaphore green", "8", Tool::Place, VID_SEMAPHORE, VID_EDITOR_ICONS, 4, 0, false},
    {"ToolSemaphoreRed", "Semaphore red", "9", Tool::Place, VID_SEMAPHORE, VID_EDITOR_ICONS, 5, 0, true},
    {"ToolReinforcement", "Reinforcement point", "0", Tool::Place, VID_LOCOMOTIVE, VID_ICONS, 11, 3, false},
    {"ToolSection", "Delete a section of track", "", Tool::Section, -1, VID_EDITOR_ICONS, 2, 0, false},
    {"ToolDecals", "Decals", "[ ]", Tool::Decals, 0, VID_EDITOR_ICONS, 6, 0, false},
    {"ToolPushLine", "Push line", "", Tool::PushLine, -1, VID_EDITOR_ICONS, 7, 0, false},
    {"ToolMission", "Mission settings", "", Tool::Settings, -1, VID_ICONS, 0, 0, false},
    {"ToolHeight", "Ground height", "", Tool::Height, -1, VID_EDITOR_ICONS, 8, 0, false},
    {"ToolCrate", "Money crate", "", Tool::Place, VID_CRATE, VID_EDITOR_ICONS, 9, 3, false},
    {"ToolVehicles", "Vehicles", "", Tool::Place, VEHICLES_NVID, VID_ICONS, 12, 0, false},
    {"ToolPaint", "Paint the ground", "", Tool::Paint, -1, VID_EDITOR_ICONS, 11, 0, false},
    {"ToolWater", "Water and lava", "", Tool::Water, -1, VID_EDITOR_ICONS, 12, 0, false},
    {"ToolRegion", "Region", "", Tool::Region, -1, VID_EDITOR_ICONS, 13, 0, false},
};
constexpr int ENTRY_COUNT = sizeof ENTRIES / sizeof ENTRIES[0];
constexpr int SLOTS = 10;           // the slots of the base, then the strip
constexpr int DECAL_ENTRY = 11;
// The state bar's base (VID 450, 1024 x 50) with its ten slots (41 px apart, the first one's
// middle 327.5 px from the left edge and 22.5 from the top, as the unit bar uses them) and
// the depot panel's strip (VID 497, 409 x 41, ten 41 px cells) a slot higher above them.
constexpr float BASE_WIDTH = 1024, BASE_HEIGHT = 50, SIDE_WIDTH = 106;
constexpr float SLOT = 41, FIRST_SLOT_X = 327.5f, SLOT_Y = 22.5f, STRIP_WIDTH = 409;

int g_entry = 0;
std::vector<int> g_decals;   // the class 8 VIDs, for the decal entry
int g_decal = 0;
int g_place_direction = 0;
// the Push line tool: the begin is picked and the end is next; the selected line (mission_push_lines())
bool g_push_started = false;
POINT g_push_begin = {};
int g_push_selected = -1;

const Entry& entry() { return ENTRIES[g_entry]; }
const char* label_of(const Entry& e) { return ui_text(e.text, e.label); }

int decal_nvid() { return g_decals.empty() ? -1 : g_decals[g_decal % g_decals.size()]; }
// what the palette picked for its two entries: All objects (0) and Vehicles (1)
int g_picked_nvid[2] = {0, 0}, g_picked_army[2] = {0, 0};
bool is_palette(const Entry& e) { return e.nvid == PALETTE_NVID || e.nvid == VEHICLES_NVID; }
int palette_of(const Entry& e) { return e.nvid == VEHICLES_NVID ? 1 : 0; }

int place_nvid() {
    return is_palette(entry()) ? g_picked_nvid[palette_of(entry())] : entry().nvid != 0 ? entry().nvid : decal_nvid();
}
int place_army() { return is_palette(entry()) ? g_picked_army[palette_of(entry())] : entry().army; }
// The name of what is placed: the palette's object by its name, the bar's entries by theirs.
const char* place_label() {
    if (!is_palette(entry())) return label_of(entry());
    const char* name = object_name(place_nvid());
    return name && *name ? name : label_of(entry());
}

// The decals (spriteClass 8) of objects.res. The editor mode loads them as class 0 (Vid_Read),
// so they are read from the file: the OBJ records, the class after the name.
void find_decals() {
    if (!g_decals.empty()) return;
    const char* res = at<const char*>(world(), W_RESOURCE_PATH);
    HANDLE f = CreateFileA(res, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return;
    std::string d(GetFileSize(f, nullptr), '\0');
    DWORD got = 0;
    const bool read = !d.empty() && ReadFile(f, &d[0], static_cast<DWORD>(d.size()), &got, nullptr) && got == d.size();
    CloseHandle(f);
    if (!read) return;
    auto u32 = [&](size_t pos) { uint32_t v = 0; if (pos + 4 <= d.size()) std::memcpy(&v, &d[pos], 4); return v; };
    for (size_t pos = 12; pos + 8 <= d.size();) {
        const uint32_t size = u32(pos + 4);
        if (d.compare(pos, 4, "OBJ ") == 0) {
            const uint32_t flags = u32(pos + 8);
            const uint32_t count = (flags & 0x80000000) ? u32(pos + 16) : flags;
            size_t record = pos + ((flags & 0x80000000) ? 20 : 12);
            for (uint32_t i = 0; i < count && record + 8 <= d.size(); ++i) {
                const size_t data = record + 4;
                const size_t name_end = d.find('\0', data + 4);
                if (name_end == std::string::npos) break;
                const int nvid = static_cast<int>(u32(data));
                if (u32(name_end + 1 + 4) == 8 && vid_by_number(nvid)) g_decals.push_back(nvid);
                record = data + u32(record);
            }
        }
        pos += 8 + ((size + 1) & ~1u);
    }
}
Box view() {
    void* g = graphics();
    return {at<float>(g, G_VIEW_LEFT), at<float>(g, G_VIEW_TOP), at<float>(g, G_VIEW_RIGHT), at<float>(g, G_VIEW_BOTTOM)};
}

// The base of the bar: in the middle of the bottom edge of the view, as Player_ShowStateBar puts it.
Box bar() {
    const Box v = view();
    const float left = std::floor((v.l + v.r - BASE_WIDTH) / 2);
    return {left, v.b - BASE_HEIGHT, left + BASE_WIDTH, v.b};
}

// The middle of an entry's cell.
void entry_cell(int i, float& x, float& y) {
    const Box b = bar();
    x = b.l + FIRST_SLOT_X + SLOT * (i % SLOTS);
    y = b.t + SLOT_Y - SLOT * (i / SLOTS);
}

int bar_entry_at(float x, float y) {
    for (int i = 0; i < ENTRY_COUNT; ++i) {
        float cx, cy;
        entry_cell(i, cx, cy);
        if (std::fabs(x - cx) < SLOT / 2 && std::fabs(y - cy) < SLOT / 2) return i;
    }
    return -1;
}

// Over the bar: its base across the view, or the strip.
bool on_bar(float x, float y) {
    const Box b = bar();
    if (y >= b.t) return true;
    float cx, cy;
    entry_cell(SLOTS, cx, cy);
    return y >= cy - SLOT / 2 && x >= cx - SLOT / 2 && x < cx - SLOT / 2 + STRIP_WIDTH;
}

void close_context();   // the context panel, below

void choose_entry(int i) {
    if (i < 0 || i >= ENTRY_COUNT) return;
    end_line();
    g_push_started = false;
    const bool again = g_entry == i;
    g_entry = i;
    close_context();   // an object's panel goes with the tool it was opened in
    if (is_palette(entry()) || (again && entry().tool == Tool::Decals))
        panel_open_palette(entry().nvid == VEHICLES_NVID);   // pick an object (a decal for the Decals entry) there
    if (entry().tool == Tool::Place) {
        g_selection.clear();
        if (place_nvid() > 0) MESSAGE("MsgPlace", "place: %s (VID %d)", place_label(), place_nvid());
    } else {
        MESSAGE("MsgTool", "tool: %s", label_of(entry()));
    }
}

// ---- the mouse ----

POINT g_mouse = {-1, -1};   // logical pixels
bool g_ctrl = false, g_shift = false;

bool shift_down() { return g_shift || GetKeyState(VK_SHIFT) < 0; }

struct Drag {
    bool active = false;
    POINT start = {};
    std::vector<uint8_t*> objects;
    std::vector<State> before;
    std::vector<float> lift;   // height above the ground at the start
    bool moved = false;
} g_drag;

POINT logical(LPARAM l) {
    POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
    window_to_logical(p);
    return p;
}

// The map point under a logical point: x and y on the map, z the ground there.
void map_point(POINT p, uint8_t* vid, float& x, float& y, float& z) {
    uint8_t* w = world();
    x = at<float>(w, W_CAMERA_X) + p.x;
    const float screen_y = at<float>(w, W_CAMERA_Y) + p.y;
    z = vid ? ground_z(vid, x, screen_y) : 0;
    y = screen_y + z;
    if (vid) {
        z = ground_z(vid, x, y);
        y = screen_y + z;
    }
}

void select(uint8_t* o) {
    g_selection = o ? group_of(o) : std::vector<uint8_t*>();
    if (o)
        MESSAGE("MsgSelected", "selected VID %d at %.0f,%.0f direction %d army %d (%s)%s", nvid_of(o), at<float>(o, O_X),
                at<float>(o, O_Y), at<int>(o, O_DIRECTION), army_of(o), army_name(army_of(o)),
                red(o) ? ui_text("WordRedSuffix", " red") : "");
}

// ---- trains ----
//
// A car (B_ENGINE: locomotives, wagons, the repair car, the arsenal, the stone on the rails ...)
// stands on the rails with its two ends (RailPos front and rear, Weapon.length apart) and is
// coupled to the car in front (prevCar) and behind (nextCar); the first car drives the train
// (DEVELOPERS.md "Trains and rails"). The editor uses the engine's own pieces for all of it: a new car
// is put on the track by Engine_Construct (Engine_PutOnRails: the rail point nearest to it, the
// link nearest to its direction), Engine_AttachToTrain couples a car behind the last car of a
// train - turning the train round first when its other end is nearer - and puts it right behind
// that car, Train_Reverse turns a train round. So:
//  - a car is placed on the track (a link within 40 px of the cursor, facing along it, R turns it
//    the other way); placed at a train's free end (within 70 px) it is coupled behind it and gets
//    its army; on a car in the middle of a train it is refused,
//  - U uncouples the selected car from the car in front, C couples the selected end car to the
//    nearest free end of another train (the other train is laid behind it and takes its army),
//    R / E turn the train round, a drag moves the whole train along the track (the dragged car to
//    where it is dropped, the others behind and in front of it as they were), Del deletes the car,
//    Shift+Del the train, A sets the army of the train,
//  - a train patrols between where it stands and a rail point chosen on the map: the script's
//    PatrolEngine(train, x, y) with the point's screen position (Engine_CommandToPoint takes the
//    rail point nearest to it, COM_ENGINE_PATROL turns round at each end), written in main()'s
//    block of the mission's script with the train found by its first car (editor_mission.h) - a
//    command kept in the map would not do (Engine_Action has no case for ACT_PATROL, 36). A train
//    needs a locomotive to move: a wagonette alone stays (the missions' civil trains have one).
// Every change keeps the state of all the cars of the map (couplings, ends, patrol) before and
// after it, so undo and redo put every train back as it was.

constexpr float CAR_REACH = 40, COUPLE_REACH = 70, CAR_OVERLAP = 30;

std::vector<uint8_t*> all_cars() {
    std::vector<uint8_t*> out;
    for_each_object([&](uint8_t* o) {
        if (is_car(o) && !removed(o)) out.push_back(o);
    });
    return out;
}

// Where a RailPos is: its point, moved along its link by the distance (the link's length counts y
// 1.5 times - the same measure as the distance).
bool pos_point(uint8_t* pos, float& x, float& y, float& z) {
    uint8_t* dot = at<uint8_t*>(pos, POS_DOT);
    if (!dot) return false;
    x = static_cast<float>(at<int>(dot, DOT_X));
    y = static_cast<float>(at<int>(dot, DOT_Y));
    z = static_cast<float>(at<int>(dot, DOT_Z));
    const int link = at<int>(pos, POS_LINK);
    if (link < 0 || link >= at<int>(dot, DOT_LINK_COUNT)) return true;
    uint8_t* l = dot + DOT_LINKS + link * DOT_LINK_SIZE;
    uint8_t* other = at<uint8_t*>(l, LINK_DOT);
    const int length = at<int>(l, LINK_LENGTH);
    if (!other || length <= 0) return true;
    const float t = (std::min)(1.0f, static_cast<float>(at<int>(pos, POS_DISTANCE)) / length);
    x += (at<int>(other, DOT_X) - x) * t;
    y += (at<int>(other, DOT_Y) - y) * t;
    z += (at<int>(other, DOT_Z) - z) * t;
    return true;
}

// A place on the track: the point of a rail link nearest to a screen point (the map's x, y - z), the
// link's direction there.
struct TrackSpot {
    bool found = false;
    float x = 0, y = 0, z = 0;
    int direction = 0;
    uint8_t* dot = nullptr;   // the nearer end of the link
};

TrackSpot track_spot(float sx, float sy, float reach) {
    TrackSpot best;
    float best_d = reach;
    uint8_t* map = reinterpret_cast<uint8_t*>(G_RAIL_MAP);
    const int count = at<int>(map, RAIL_MAP_COUNT);
    uint8_t** dots = at<uint8_t**>(map, RAIL_MAP_ITEMS);
    for (int i = 0; i < count && dots; ++i) {
        uint8_t* d = dots[i];
        if (!d) continue;
        const float ax = static_cast<float>(at<int>(d, DOT_X)), ay = static_cast<float>(at<int>(d, DOT_Y)),
                    az = static_cast<float>(at<int>(d, DOT_Z));
        const int links = (std::min)(6, at<int>(d, DOT_LINK_COUNT));
        for (int k = 0; k < links; ++k) {
            uint8_t* l = d + DOT_LINKS + k * DOT_LINK_SIZE;
            uint8_t* o = at<uint8_t*>(l, LINK_DOT);
            if (!o || o < d) continue;   // each link once
            const float bx = static_cast<float>(at<int>(o, DOT_X)), by = static_cast<float>(at<int>(o, DOT_Y)),
                        bz = static_cast<float>(at<int>(o, DOT_Z));
            const float ux = bx - ax, uy = (by - bz) - (ay - az);
            const float len2 = ux * ux + uy * uy;
            float t = len2 > 0 ? ((sx - ax) * ux + (sy - (ay - az)) * uy) / len2 : 0;
            t = (std::max)(0.0f, (std::min)(1.0f, t));
            const float px = ax + ux * t, py = (ay - az) + uy * t;
            const float dist = std::hypot(sx - px, sy - py);
            if (dist < best_d) {
                best_d = dist;
                best.found = true;
                best.x = ax + (bx - ax) * t;
                best.y = ay + (by - ay) * t;
                best.z = az + (bz - az) * t;
                best.direction = at<uint8_t>(l, LINK_DIRECTION);
                best.dot = t < 0.5f ? d : o;
            }
        }
    }
    return best;
}

int turn_toward(int direction, int toward) {   // direction or its opposite, whichever is nearer to `toward`
    const int d = (direction - toward + 256) & 255;
    return d > 64 && d < 192 ? (direction + 128) & 255 : direction;
}

bool is_end(uint8_t* c) { return !car_prev(c) || !car_next(c); }

// Where a new car would go: on the track under the cursor, coupled to a train's free end nearby.
struct CarPlan {
    bool ok = false;
    UiText why;
    TrackSpot spot;
    int direction = 0;
    uint8_t* couple_to = nullptr;
};

CarPlan plan_car(POINT mouse) {
    CarPlan p;
    uint8_t* w = world();
    p.spot = track_spot(at<float>(w, W_CAMERA_X) + mouse.x, at<float>(w, W_CAMERA_Y) + mouse.y, CAR_REACH);
    if (!p.spot.found) {
        p.why = {"WhyCarTrack", "a car stands on the rails - no track under the cursor"};
        return p;
    }
    p.direction = (p.spot.direction + (g_place_direction >= 128 ? 128 : 0)) & 255;
    float best = COUPLE_REACH;
    for (uint8_t* c : all_cars()) {
        const float d = std::hypot(at<float>(c, O_X) - p.spot.x, at<float>(c, O_Y) - p.spot.y);
        if (d < CAR_OVERLAP && !is_end(c)) {
            p.why = {"WhyCarTaken", "a car of a train stands here"};
            return p;
        }
        if (d < best && is_end(c)) {
            best = d;
            p.couple_to = c;
        }
    }
    p.ok = true;
    return p;
}

// Takes a car out of its train (its neighbours' couplings too).
void unlink_car(uint8_t* c) {
    if (uint8_t* p = car_prev(c))
        if (car_next(p) == c) car_next(p) = nullptr;
    if (uint8_t* n = car_next(c))
        if (car_prev(n) == c) car_prev(n) = nullptr;
    car_prev(c) = car_next(c) = nullptr;
}

// Couples a single car behind the end of `train_car`'s train nearer to it (Engine_AttachToTrain),
// after moving it to that end - so the train is turned round only when its front is that end.
void attach_car(uint8_t* c, uint8_t* train_car, bool at_front) {
    const std::vector<uint8_t*> train = train_of(train_car);
    float x, y, z;
    if (at_front ? pos_point(train.front() + E_FRONT_DOT, x, y, z) : pos_point(train.back() + E_REAR_DOT, x, y, z))
        reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(c, nullptr, x, y, z);
    reinterpret_cast<EngineWith_t>(ENGINE_ATTACH)(c, nullptr, train_car);
}

void occupy_all() {
    for (uint8_t* c : all_cars()) car_call(ENGINE_OCCUPY_DOTS, c);
}

void commit_cars(const UiText& what, const std::vector<uint8_t*>& cars, const std::vector<State>& before,
                 uint8_t* made = nullptr) {
    std::vector<uint8_t*> objects = cars;
    std::vector<State> was = before;
    if (made) {
        objects.push_back(made);
        State none = state_of(made);
        none.present = false;
        was.push_back(none);
    }
    commit(what, objects, was);
}

uint8_t* create(int nvid, float x, float y, float z, int direction, int army);

void place_car(POINT mouse) {
    const int nvid = place_nvid();
    const CarPlan p = plan_car(mouse);
    if (!p.ok) {
        MESSAGE("MsgLabelReason", "%s: %s", place_label(), p.why.get());
        return;
    }
    const std::vector<uint8_t*> cars = all_cars();
    const std::vector<State> before = states(cars);
    for (uint8_t* c : cars) car_call(ENGINE_LEAVE_DOTS, c);   // Engine_PutOnRails couples only where it is told
    uint8_t* o = create(nvid, p.spot.x, p.spot.y, p.spot.z, p.direction, place_army());
    if (!o) {
        occupy_all();
        MESSAGE("MsgNotCreated", "VID %d could not be created", nvid);
        return;
    }
    unlink_car(o);
    if (p.couple_to) {
        const std::vector<uint8_t*> train = train_of(p.couple_to);
        const bool at_front = p.couple_to == train.front() &&
                              (train.size() > 1 || [&] {   // a single car: the end of it nearer to the new one
                                  float fx, fy, fz, rx, ry, rz;
                                  return pos_point(p.couple_to + E_FRONT_DOT, fx, fy, fz) &&
                                         pos_point(p.couple_to + E_REAR_DOT, rx, ry, rz) &&
                                         std::hypot(fx - p.spot.x, fy - p.spot.y) < std::hypot(rx - p.spot.x, ry - p.spot.y);
                              }());
        attach_car(o, p.couple_to, at_front);
        if (army_of(o) != army_of(p.couple_to)) act(o, ACT_SET_ARMY, army_of(p.couple_to));
    }
    occupy_all();
    commit_cars({"OpCar", "car"}, cars, before, o);
    g_selection = group_of(o);
    const size_t length = train_of(o).size();
    MESSAGE("MsgCarPlaced", "placed VID %d at %.0f,%.0f direction %d army %d - a train of %u", nvid, at<float>(o, O_X),
            at<float>(o, O_Y), at<int>(o, O_DIRECTION), army_of(o), static_cast<unsigned>(length));
}

void uncouple(uint8_t* c) {
    if (!is_car(c) || !car_prev(c)) {
        MESSAGE("MsgUncoupleFirst", "U uncouples a car from the car in front - this one is the first of its train");
        return;
    }
    const std::vector<uint8_t*> cars = all_cars();
    const std::vector<State> before = states(cars);
    uint8_t* p = car_prev(c);
    car_next(p) = nullptr;
    car_prev(c) = nullptr;
    commit_cars({"OpUncouple", "uncouple"}, cars, before);
    g_selection = group_of(c);
    MESSAGE("MsgUncoupled", "uncoupled: trains of %u and %u cars", static_cast<unsigned>(train_of(p).size()),
            static_cast<unsigned>(train_of(c).size()));
}

// The free end of another train nearest to a car at an end of its own train (within reach).
uint8_t* coupling_partner(uint8_t* c) {
    if (!is_car(c) || !is_end(c)) return nullptr;
    const std::vector<uint8_t*> own = train_of(c);
    uint8_t* best = nullptr;
    float best_d = COUPLE_REACH;
    for (uint8_t* k : all_cars()) {
        if (!is_end(k) || std::find(own.begin(), own.end(), k) != own.end()) continue;
        const float d = std::hypot(at<float>(k, O_X) - at<float>(c, O_X), at<float>(k, O_Y) - at<float>(c, O_Y));
        if (d < best_d) {
            best_d = d;
            best = k;
        }
    }
    return best;
}

void couple(uint8_t* c) {
    uint8_t* e = coupling_partner(c);
    if (!e) {
        MESSAGE("MsgNoPartner", "C couples an end car to the end of another train within %d px - none here",
                static_cast<int>(COUPLE_REACH));
        return;
    }
    const std::vector<uint8_t*> cars = all_cars();
    const std::vector<State> before = states(cars);
    std::vector<uint8_t*> other = train_of(e);
    if (other.front() != e) std::reverse(other.begin(), other.end());   // from the end that couples
    const std::vector<uint8_t*> own = train_of(c);
    const bool at_front = c == own.front() && (own.size() > 1 || [&] {
                              float fx, fy, fz, rx, ry, rz;
                              return pos_point(c + E_FRONT_DOT, fx, fy, fz) && pos_point(c + E_REAR_DOT, rx, ry, rz) &&
                                     std::hypot(fx - at<float>(e, O_X), fy - at<float>(e, O_Y)) <
                                         std::hypot(rx - at<float>(e, O_X), ry - at<float>(e, O_Y));
                          }());
    for (uint8_t* k : other) {
        car_call(ENGINE_LEAVE_DOTS, k);
        car_prev(k) = car_next(k) = nullptr;
    }
    for (size_t i = 0; i < other.size(); ++i) {
        attach_car(other[i], c, i == 0 && at_front);
        if (army_of(other[i]) != army_of(c)) act(other[i], ACT_SET_ARMY, army_of(c));
    }
    occupy_all();
    commit_cars({"OpCouple", "couple"}, cars, before);
    g_selection = group_of(c);
    MESSAGE("MsgCoupled", "coupled: a train of %u cars", static_cast<unsigned>(train_of(c).size()));
}

void reverse_train(uint8_t* c) {
    const std::vector<uint8_t*> cars = all_cars();
    const std::vector<State> before = states(cars);
    car_call(ENGINE_LEAVE_DOTS, c);
    car_call(TRAIN_REVERSE, c);
    for (uint8_t* k : train_of(c)) car_call(ENGINE_UPDATE_POSITION, k);
    occupy_all();
    commit_cars({"OpReverse", "turn round"}, cars, before);
    g_selection = group_of(c);
    MESSAGE("MsgReversed", "the train is turned round: VID %d first", nvid_of(train_of(c).front()));
}

// The train of `c` laid again along the track with `c` at a spot: the cars behind it after it,
// the ones in front of it before it, in their order. False (nothing changed) off the track.
bool move_train(uint8_t* c, float sx, float sy) {
    const TrackSpot spot = track_spot(sx, sy, CAR_REACH);
    if (!spot.found) return false;
    const std::vector<uint8_t*> train = train_of(c);
    const size_t k = std::find(train.begin(), train.end(), c) - train.begin();
    for (uint8_t* t : train) car_call(ENGINE_LEAVE_DOTS, t);
    for (uint8_t* t : train) car_prev(t) = car_next(t) = nullptr;
    reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(c, nullptr, spot.x, spot.y, spot.z);
    reinterpret_cast<SetInt_t>(OBJECT_SET_DIRECTION)(c, nullptr, turn_toward(spot.direction, at<int>(c, O_DIRECTION)));
    car_call(ENGINE_PUT_ON_RAILS, c);
    unlink_car(c);   // not coupled to whatever stands there
    for (size_t i = k + 1; i < train.size(); ++i) attach_car(train[i], c, false);
    for (size_t i = k; i-- > 0;) attach_car(train[i], c, i + 1 == k);   // the first one at the front, then behind
    if (train_of(c).front() != train.front()) car_call(TRAIN_REVERSE, c);   // the order it had
    for (uint8_t* t : train_of(c)) car_call(ENGINE_UPDATE_POSITION, t);
    occupy_all();
    return true;
}

// The train's patrol: kept in its first car, to the rail point under the cursor.
uint8_t* g_patrol_train = nullptr;   // a car whose train gets the next click's point

void set_train_patrol(uint8_t* c, bool on, float sx, float sy) {
    int x = 0, y = 0;
    if (on) {
        const TrackSpot spot = track_spot(sx, sy, CAR_REACH);
        if (!spot.found || !spot.dot) {
            MESSAGE("MsgPatrolTrack", "patrol: click a point of the track");
            return;
        }
        x = at<int>(spot.dot, DOT_X);
        y = at<int>(spot.dot, DOT_Y) - at<int>(spot.dot, DOT_Z);
    }
    const std::vector<uint8_t*> cars = all_cars();
    const std::vector<State> before = states(cars);
    for (uint8_t* k : train_of(c)) set_patrol(k, false, 0, 0);
    if (on) set_patrol(train_of(c).front(), true, x, y);
    commit_cars({"OpPatrol", "patrol"}, cars, before);
    g_selection = group_of(c);
    if (on) MESSAGE("MsgPatrol", "patrol: the train goes to and fro between where it stands and %d,%d", x, y);
    else MESSAGE("MsgPatrolNone", "patrol: none");
}

// The train's patrol point, if it has one.
bool train_patrol(uint8_t* c, int& x, int& y) {
    for (uint8_t* k : train_of(c))
        if (patrol_of(k, x, y)) return true;
    return false;
}

void start_drag(POINT p) {
    g_drag = Drag();
    if (g_selection.empty() || !movable(g_selection[0])) return;
    g_drag.active = true;
    g_drag.start = p;
    g_drag.objects = g_selection;
    g_drag.before = states(g_selection);
    for (uint8_t* o : g_selection)
        g_drag.lift.push_back(at<float>(o, O_Z) - ground_z(vid_of(o), at<float>(o, O_X), at<float>(o, O_Y)));
}

void continue_drag(POINT p) {
    const float dx = static_cast<float>(p.x - g_drag.start.x), dy = static_cast<float>(p.y - g_drag.start.y);
    if (!g_drag.moved && std::fabs(dx) + std::fabs(dy) < 3) return;   // a click, not a drag
    g_drag.moved = true;
    for (size_t i = 0; i < g_drag.objects.size(); ++i) {
        uint8_t* o = g_drag.objects[i];
        const State& s = g_drag.before[i];
        const float x = s.x + dx, y = s.y + dy;
        reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(o, nullptr, x, y, ground_z(vid_of(o), x, y) + g_drag.lift[i]);
    }
}

void end_drag() {
    if (g_drag.active && g_drag.moved && is_car(g_drag.objects[0])) {   // a train, along the track
        uint8_t* c = g_drag.objects[0];
        const float sx = at<float>(c, O_X), sy = at<float>(c, O_Y) - at<float>(c, O_Z);   // where it was dropped
        for (size_t i = 0; i < g_drag.objects.size(); ++i) apply(g_drag.objects[i], g_drag.before[i]);
        const std::vector<uint8_t*> cars = all_cars();
        const std::vector<State> before = states(cars);
        if (move_train(c, sx, sy)) {
            commit_cars({"OpMoveTrain", "move train"}, cars, before);
            MESSAGE("MsgTrainMoved", "the train of %u cars moved: VID %d at %.0f,%.0f", static_cast<unsigned>(train_of(c).size()),
                    nvid_of(c), at<float>(c, O_X), at<float>(c, O_Y));
        } else {
            MESSAGE("MsgTrainStays", "no track where the car was dropped - the train stays");
        }
        g_selection = group_of(c);
        g_drag = Drag();
        if (!g_test && GetCapture()) ReleaseCapture();
        return;
    }
    if (g_drag.active && g_drag.moved && g_drag.objects.size() == 1 && on_track_vid(nvid_of(g_drag.objects[0]))) {
        // on the track: to the place there nearest to where it was dropped, with its line and posts
        uint8_t* o = g_drag.objects[0];
        float x = at<float>(o, O_X), y = at<float>(o, O_Y);
        int direction = at<int>(o, O_DIRECTION);
        UiText why;
        apply(o, g_drag.before[0]);   // its old place first: the old line is found there
        if (!snap(nvid_of(o), x, y, direction, why)) {
            MESSAGE("MsgStays", "%s - it stays where it was", why.get());
        } else {
            const std::vector<PushLine> lines_before = mission_push_lines();
            std::vector<uint8_t*> objects{o};
            const bool lines_changed = take_building_lines(o, objects);
            std::vector<State> before = states(objects);
            for (size_t i = 1; i < objects.size(); ++i) {
                State gone = before[i];
                gone.present = false;
                apply(objects[i], gone);
            }
            reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(o, nullptr, x, y, ground_z(vid_of(o), x, y));
            reinterpret_cast<SetInt_t>(OBJECT_SET_DIRECTION)(o, nullptr, direction);
            commit({"OpMove", "move"}, objects, before);
            if (lines_changed) {
                Operation& op = g_undo.back();
                op.lines = true;
                op.lines_before = lines_before;
                op.lines_after = mission_push_lines();
            }
            if (nvid_of(o) == VID_DEPOT || nvid_of(o) == VID_WORKSHOP) give_building_line(o);
            else MESSAGE("MsgMovedTrack", "moved VID %d to %.0f,%.0f direction %d", nvid_of(o), x, y, direction);
        }
        g_drag = Drag();
        if (!g_test && GetCapture()) ReleaseCapture();
        return;
    }
    if (g_drag.active && g_drag.moved) {
        uint8_t* o = g_drag.objects[0];
        commit({"OpMove", "move"}, g_drag.objects, g_drag.before);
        MESSAGE("MsgMoved", "moved VID %d to %.0f,%.0f,%.0f", nvid_of(o), at<float>(o, O_X), at<float>(o, O_Y), at<float>(o, O_Z));
    }
    g_drag = Drag();
    if (!g_test && GetCapture()) ReleaseCapture();
}

// ---- changes of the selection ----

void turn(int steps) {
    if (entry().tool == Tool::Place) {
        uint8_t* v = vid_by_number(place_nvid());
        const int dirs = v ? (std::max)(1, at<int>(v, V_ND_DIR)) : 8;
        g_place_direction = (g_place_direction + steps * (256 / dirs) + 256) & 255;
        MESSAGE("MsgDirection", "direction %d", g_place_direction);
        return;
    }
    if (g_selection.empty() || !movable(g_selection[0])) return;
    uint8_t* o = g_selection[0];
    if (is_car(o)) {   // a train turns round
        reverse_train(o);
        return;
    }
    if (nvid_of(o) == VID_DEPOT || nvid_of(o) == VID_WORKSHOP || nvid_of(o) == VID_CRATE) {
        MESSAGE("MsgTurnedByTrack", "VID %d is turned by its track (drag it to another place on the track)", nvid_of(o));
        return;
    }
    if (nvid_of(o) == VID_SEMAPHORE) steps *= 4;   // a post faces one way or the other along the track
    const int dirs = (std::max)(1, at<int>(vid_of(o), V_ND_DIR));
    if (dirs <= 1) return;
    const std::vector<State> before = states(g_selection);
    const std::vector<uint8_t*> guns = guns_on(o);   // in their sockets before the turn
    const int direction = (at<int>(o, O_DIRECTION) + steps * (256 / dirs) + 256) & 255;
    reinterpret_cast<SetInt_t>(OBJECT_SET_DIRECTION)(o, nullptr, direction);
    const std::vector<Socket> sockets = sockets_of(nvid_of(o), direction);
    for (size_t i = 0; i < guns.size() && i < sockets.size(); ++i) {   // each gun to its socket's new place
        if (!guns[i]) continue;
        const float x = at<float>(o, O_X) + sockets[i].dx, y = at<float>(o, O_Y) + sockets[i].dy;
        reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(guns[i], nullptr, x, y, at<float>(o, O_Z) + sockets[i].dz);
        reinterpret_cast<SetInt_t>(OBJECT_SET_DIRECTION)(guns[i], nullptr, direction);
    }
    commit({"OpTurn", "turn"}, g_selection, before);
    MESSAGE("MsgTurned", "turned VID %d to direction %d", nvid_of(o), direction);
}

// The army of the selection: the object's, and its guns' (gun_army).
void set_army(int army) {
    if (g_selection.empty() || class_of(g_selection[0]) == B_RAIL) return;
    const std::vector<State> before = states(g_selection);
    uint8_t* host = g_selection[0];
    for (uint8_t* o : g_selection)
        act(o, ACT_SET_ARMY, o != host && is_gun(nvid_of(o)) ? gun_army(nvid_of(host), army) : army);
    commit({"OpArmy", "army"}, g_selection, before);
    MESSAGE("MsgArmy", "VID %d: army %d (%s)", nvid_of(host), army, army_name(army));
}

void next_army() {
    if (!g_selection.empty()) set_army((army_of(g_selection[0]) + 1) & 3);
}

void toggle_semaphore() {
    if (g_selection.empty() || nvid_of(g_selection[0]) != VID_SEMAPHORE) {
        MESSAGE("MsgSemaphoreOnly", "T switches a semaphore (VID 152)");
        return;
    }
    const std::vector<State> before = states(g_selection);
    set_red(g_selection[0], !red(g_selection[0]));
    commit({"OpSemaphore", "semaphore"}, g_selection, before);
    MESSAGE("MsgSemaphore", "semaphore %s", red(g_selection[0]) ? ui_text("WordRed", "red") : ui_text("WordGreen", "green"));
}

void delete_objects(const std::vector<uint8_t*>& chosen) {
    if (chosen.empty()) return;
    std::vector<uint8_t*> objects = chosen;   // with the beds of the rails
    for (uint8_t* o : chosen)
        if (class_of(o) == B_RAIL)
            if (uint8_t* b = bed_of(o)) objects.push_back(b);
    for (uint8_t* o : objects)
        if (!deletable(o)) {
            MESSAGE("MsgTrainOnTrack", "a train car stands on this track or next to it: not deleted");
            return;
        }
    // a factory's or repair point's push line and its posts go with it
    const std::vector<PushLine> lines_before = mission_push_lines();
    bool lines_changed = false;
    std::vector<uint8_t*> posts;
    for (uint8_t* o : chosen) lines_changed |= take_building_lines(o, posts);
    for (uint8_t* p : posts)
        if (std::find(objects.begin(), objects.end(), p) == objects.end()) objects.push_back(p);
    const std::vector<State> before = states(objects);
    for (size_t i = 0; i < objects.size(); ++i) {   // through apply: a rail leaves the rail network
        State gone = before[i];
        gone.present = false;
        apply(objects[i], gone);
    }
    commit({"OpDelete", "delete"}, objects, before);
    if (lines_changed) {
        Operation& op = g_undo.back();
        op.lines = true;
        op.lines_before = lines_before;
        op.lines_after = mission_push_lines();
    }
    MESSAGE("MsgDeleted", "deleted VID %d at %.0f,%.0f%s", nvid_of(objects[0]), before[0].x, before[0].y,
            objects.size() > 1 ? ui_printf(ui_format("MsgAndMore", " and %u more"), static_cast<unsigned>(objects.size() - 1)).c_str()
                               : "");
    if (!g_selection.empty() && g_selection[0] == objects[0]) g_selection.clear();
}

// ---- placing ----

// The behave and the build list of an object of the same VID on the map (map_tool.py place
// copies them the same way); a depot without one gets the game's 14 depot units.
void copy_unit_state(uint8_t* o) {
    if (!is_unit_class(class_of(o))) return;
    // of the same VID: best of the same army, then one with a build list (the enemy's depot of
    // a mission has none - the AI builds by its script)
    uint8_t* source = nullptr;
    int best = -1;
    for_each_object([&](uint8_t* s) {
        if (s == o || vid_of(s) != vid_of(o)) return;
        const int score = (army_of(s) == army_of(o) ? 2 : 0) + (at<int>(s, O_ITEMS_COUNT) > 0 ? 1 : 0);
        if (score > best) {
            best = score;
            source = s;
        }
    });
    if (source) {
        act(o, ACT_SET_BEHAVE, at<int>(source, O_BEHAVE));
        act(o, ACT_DELETE_ALL_ITEM);
        const short* items = at<short*>(source, O_ITEMS);
        for (int i = 0; items && i < at<int>(source, O_ITEMS_COUNT); ++i) act(o, ACT_ADD_ITEM, items[i]);
    } else {   // none on this map: the state of one of the missions (the palette's table, editor_panel.cpp)
        int behave = 0;
        std::vector<short> items;
        if (palette_state(nvid_of(o), behave, items)) {
            act(o, ACT_SET_BEHAVE, behave);
            act(o, ACT_DELETE_ALL_ITEM);
            for (short n : items) act(o, ACT_ADD_ITEM, n);
        }
    }
    if (nvid_of(o) == VID_DEPOT && at<int>(o, O_ITEMS_COUNT) == 0 && army_of(o) == 0) {   // the player's
        act(o, ACT_DELETE_ALL_ITEM);
        for (short n : DEPOT_UNITS) act(o, ACT_ADD_ITEM, n);
    }
}

float track_distance(float x, float y) {
    float best = 1e9f;
    for_each_object([&](uint8_t* r) {
        if (class_of(r) == B_RAIL)
            best = std::fmin(best, std::hypot(at<float>(r, O_X) - x, at<float>(r, O_Y) - y));
    });
    return best;
}

uint8_t* create(int nvid, float x, float y, float z, int direction, int army) {
    uint8_t* v = vid_by_number(nvid);
    if (!v) return nullptr;
    auto create_fn = reinterpret_cast<Create_t>(at<void**>(world(), 0)[W_VTABLE_CREATE / 4]);
    uint8_t* o = create_fn(world(), nullptr, v, x, y, z, static_cast<unsigned>(direction), nullptr);
    if (!o) return nullptr;
    if (army != army_of(o)) act(o, ACT_SET_ARMY, army);   // a gun of army 0 is the enemy's as created
    copy_unit_state(o);
    return o;
}

uint8_t* fit_gun(uint8_t* host, const Socket& s, int nvid) {
    return create(nvid, at<float>(host, O_X) + s.dx, at<float>(host, O_Y) + s.dy, at<float>(host, O_Z) + s.dz,
                  at<int>(host, O_DIRECTION), gun_army(nvid_of(host), army_of(host)));
}

std::vector<GridChange> flatten_under(uint8_t* o);   // the ground, below
std::string g_place_status;   // why the object to place cannot go where the cursor is

void place(POINT p) {
    const int nvid = place_nvid();
    if (nvid <= 0 && is_palette(entry())) {   // nothing picked yet
        panel_open_palette(entry().nvid == VEHICLES_NVID);
        return;
    }
    uint8_t* v = vid_by_number(nvid);
    if (!v) {
        MESSAGE("MsgNotLoaded", "VID %d is not loaded", nvid);
        return;
    }
    if (at<int>(v, V_CLASS) == B_ENGINE) {   // a car: on the track (trains, above)
        place_car(p);
        return;
    }
    float x, y, z;
    map_point(p, v, x, y, z);
    const int dirs = (std::max)(1, at<int>(v, V_ND_DIR));
    int direction = dirs > 1 ? g_place_direction : 0;
    if (on_track_vid(nvid)) {   // the authors' places on the track
        UiText why;
        if (!snap(nvid, x, y, direction, why)) {
            MESSAGE("MsgLabelReason", "%s: %s", place_label(), why.get());
            return;
        }
        z = ground_z(v, x, y);
    }
    if (uint8_t* map = *reinterpret_cast<uint8_t**>(G_OBJECT_MAP)) {
        uint8_t* blocker = reinterpret_cast<AskPlace_t>(ASK_PLACE)(map, nullptr, v, x, y, z);
        if (blocker && blocker != *reinterpret_cast<uint8_t**>(G_CURSOR)) {
            MESSAGE("MsgTaken", "%s: the place is taken by VID %d", place_label(), nvid_of(blocker));
            return;
        }
    }
    std::vector<uint8_t*> objects;
    uint8_t* o = create(nvid, x, y, z, direction, place_army());
    if (!o) {
        MESSAGE("MsgNotCreated", "VID %d could not be created", nvid);
        return;
    }
    objects.push_back(o);
    if (entry().red) set_red(o, true);
    if (is_region(o)) {   // a region from the palette: a rectangle to start with, sized in its panel
        at<float>(o, RG_WIDTH) = 192;
        at<float>(o, RG_HEIGHT) = 128;
    }
    const std::vector<Socket> sockets = sockets_of(nvid, direction);   // a host with the missions' fit
    const std::vector<int> fit = default_fit(nvid);
    for (size_t i = 0; i < sockets.size() && i < fit.size(); ++i)
        if (fit[i])
            if (uint8_t* g = fit_gun(o, sockets[i], fit[i])) objects.push_back(g);
    std::vector<State> before = states(objects);
    for (State& s : before) s.present = false;
    commit({"OpPlace", "place"}, objects, before);
    g_selection = group_of(o);   // the new one is selected (L on a post, A, Del)
    // a building: the ground under it at its height, the ring around half way (map_tool.py place)
    if (class_of(o) == B_BUILDING || class_of(o) == B_DEPO) g_undo.back().grid = flatten_under(o);
    if (nvid == VID_DEPOT || nvid == VID_WORKSHOP) {   // its push line and posts
        give_building_line(o);
        return;
    }
    const float track = track_distance(at<float>(o, O_X), at<float>(o, O_Y));
    const float limit = nvid == VID_DEPOT ? 100.0f : nvid == VID_WORKSHOP || nvid == VID_FORT ? 40.0f : 0.0f;
    MESSAGE("MsgPlaced", "placed VID %d at %.0f,%.0f,%.0f direction %d army %d%s%s", nvid, at<float>(o, O_X),
            at<float>(o, O_Y), at<float>(o, O_Z), direction, army_of(o), entry().red ? ui_text("WordRedSuffix", " red") : "",
            limit > 0 && track > limit ? ui_text("MsgNoTrackNear", " - no track near it (MAP_RULES.md 6)") : "");
}

// ---- the track ----
//
// The Track tool lays rails (VID 610) step by step along points of the rail lattice: the
// first click starts a line (at the end of a rail of the map, or a point of the lattice the
// map's rails are on), every next click lays the rail to the point of the lattice nearest to
// the cursor - one of the steps of the 12 shapes - when the rules of rail_rules.h (the same
// as map_tool.py's) let it; the preview shows that rail and its ends green, or red with the
// reason. The right button or Esc ends the line. A rail is created with World CreateObject:
// Rail_Construct runs ACT_CHANGE_DIRECTION, which puts its points and links them to the rails
// at its ends. The ground under it is flattened as map_tool.py track does (the cells it
// crosses to the track's level, the cells around half way), through World_SetGroundZ, and the
// step can be undone (the rail and the cells).
//
// The kinds of rail (V): the plain one (610), the swamp's (600: on an embankment, VID 602, 28 below
// it, over the swamp's ground - l06-l08 - which is left as it is; its points stand 7 above it, not
// 10), and the destroyed ones of both (611, 601: the death VIDs of the others; Rail_Action marks
// their points broken - trains slow down there and the repair car mends them as it passes). A
// ramp (PgUp: the next rail climbs, PgDn: it descends) is the same shape in a frame of the groups 1
// or 2, which raise one of its points by sizeZ 11 (rail_rules.h): the line goes on 11 higher or
// lower and the ground under the ramp slopes with it; its bed is the bed VID in the same direction
// (the beds have the rail's 37 frames - the missions' 98 ramps of l11, l12 and l15 have them so).

std::vector<rail::Point> g_line;   // the points of the line being laid
int g_line_level = 0;              // its ground level (plain rails stand 4 below, their points 6 above)
bool g_line_level_known = false;   // false until the first rail of a new line is laid
// the line's ground so far: the cells its rails cross and every cell's height before the line,
// so a line laid in one go flattens exactly as map_tool.py track does for the whole track
std::set<rail::Cell> g_line_cells;
std::map<rail::Cell, short> g_line_original;
int g_line_nvid = VID_RAIL;

constexpr int RAIL_KINDS[] = {610, 600, 611, 601};
const char* const RAIL_KIND_KEYS[] = {"RailPlain", "RailSwamp", "RailDestroyed", "RailSwampDestroyed"};
const char* const RAIL_KIND_NAMES[] = {"plain", "swamp", "destroyed", "destroyed swamp"};
constexpr int VID_EMBANKMENT = 602;
constexpr float EMBANKMENT_DEPTH = 28;
constexpr int SWAMP_LIFT = 29;   // a new swamp line: its level above the ground (the rails 28 above it)
int g_rail_kind = 0;
int g_ramp = 0;                  // the next rail climbs (1) or descends (-1)

bool swamp_rail(int nvid) { return nvid == 600 || nvid == 601; }
bool destroyed_rail(int nvid) { return nvid == 611 || nvid == 601; }
int rail_altitude(int nvid) { return swamp_rail(nvid) ? 7 : rail::ALTITUDE; }
const char* rail_kind_name(int nvid) {
    for (int k = 0; k < 4; ++k)
        if (RAIL_KINDS[k] == nvid) return ui_text(RAIL_KIND_KEYS[k], RAIL_KIND_NAMES[k]);
    return "?";
}
// The z of a rail of the line (its points stand at the line's level + 6, whatever the VID's altitude).
float line_rail_z(int nvid) { return g_line_level + 6.0f - rail_altitude(nvid); }

// The direction of a step's rail: its flat shape, or as a ramp - climbing raises the far end,
// descending the near one (the rail itself then stands RAMP lower).
int step_direction(const rail::Segment& s, rail::Point from, int ramp) {
    if (!ramp) return rail::DIRECTION[s.shape];
    const bool from_end0 = rail::close(s.end0.x, from.x) && rail::close(s.end0.y, from.y);
    const bool raise_end0 = ramp > 0 ? !from_end0 : from_end0;
    return (s.shape + (raise_end0 ? 12 : 24)) * 256 / rail::FRAMES;
}

struct RailObject {
    uint8_t* object;
    rail::Rail rail;
};

std::vector<RailObject> map_rails() {
    std::vector<RailObject> out;
    for_each_object([&](uint8_t* o) {
        if (class_of(o) != B_RAIL) return;
        // rail_of counts the plain rail's altitude: a swamp rail's points stand 3 lower
        const float z = at<float>(o, O_Z) + rail_altitude(nvid_of(o)) - rail::ALTITUDE;
        out.push_back({o, rail::rail_of(at<float>(o, O_X), at<float>(o, O_Y), z, at<int>(o, O_DIRECTION))});
    });
    return out;
}

std::vector<rail::Rail> rails_of(const std::vector<RailObject>& objects) {
    std::vector<rail::Rail> out;
    for (const RailObject& r : objects) out.push_back(r.rail);
    return out;
}

// The steps of the lattice: one rail of each shape, both ways.
const std::vector<rail::Point>& lattice_steps() {
    static std::vector<rail::Point> steps;
    if (steps.empty())
        for (const auto& e : rail::ENDS)
            for (int s = 0; s < 2; ++s)
                steps.push_back({(e[1 - s].x - e[s].x) * rail::HALF_X, (e[1 - s].y - e[s].y) * rail::HALF_Y});
    return steps;
}

double distance(rail::Point a, rail::Point b) { return std::hypot(a.x - b.x, a.y - b.y); }

// The rail of the map ending at a point (to continue at its height and VID), or null; `level` gets
// the ground level of a line that continues it - from the height of its rail point there, which
// a ramp raises.
uint8_t* rail_ending_at(const std::vector<RailObject>& rails, rail::Point p, int* level = nullptr) {
    for (const RailObject& r : rails)
        for (int i = 0; i < 2; ++i) {
            const rail::Point e = rail::end_of(r.rail, i);
            if (rail::close(e.x, p.x) && rail::close(e.y, p.y)) {
                if (level) *level = rail::height_of(r.rail, i) - rail::ALTITUDE + 4;
                return r.object;
            }
        }
    return nullptr;
}

// The screen point of the map under the mouse: the camera plus the mouse. The camera of a held
// level goes past the edges of the map (camera.cpp), so the Track tool can lay the main line on
// out of sight (MAP_RULES.md 4) and the Push line tool can pick its points there.
void mouse_on_map(float& x, float& y) {
    uint8_t* w = world();
    x = at<float>(w, W_CAMERA_X) + g_mouse.x;
    y = at<float>(w, W_CAMERA_Y) + g_mouse.y;
}

// The map point of the mouse at a ground level (the rails' drawing is y - z).
rail::Point mouse_on_level(int level) {
    float x, y;
    mouse_on_map(x, y);
    return {x, y + (level - 4.0)};
}

// Where a line would start: the end of a rail of the map near the cursor, or the point of the
// lattice of the nearest rail end (0, 0 without rails).
rail::Point line_start(const std::vector<RailObject>& rails) {
    uint8_t* w = world();
    const float x = at<float>(w, W_CAMERA_X) + g_mouse.x, screen_y = at<float>(w, W_CAMERA_Y) + g_mouse.y;
    rail::Point best = {0, 0};
    double best_distance = 1e9;
    for (const RailObject& r : rails)
        for (int i = 0; i < 2; ++i) {
            const rail::Point e = rail::end_of(r.rail, i);
            const double d = std::hypot(e.x - x, e.y - at<float>(r.object, O_Z) - screen_y);
            if (d < best_distance) {
                best_distance = d;
                best = {rail::round3(e.x), rail::round3(e.y)};
            }
        }
    if (best_distance < 20) return best;
    float mx, my, mz;   // the map point under the cursor, on the ground (as placing finds it)
    map_point(g_mouse, vid_by_number(VID_RAIL), mx, my, mz);
    const double y = my - 4;   // a rail stands 4 below the ground
    return {best.x + std::round((mx - best.x) / rail::HALF_X) * rail::HALF_X,
            best.y + std::round((y - best.y) / rail::HALF_Y) * rail::HALF_Y};
}

struct TrackStep {
    bool valid = false;
    UiText reason;
    rail::Point to = {};
    rail::Segment segment = {};
};

UiText reason_of(rail::Error e) {
    switch (e) {
    case rail::Error::Step: return {"RailStep", "not a step of the rail lattice (71 x 50 or halves)"};
    case rail::Error::Join: return {"RailJoin", "does not join the rail before it - a turn needs a curve"};
    case rail::Error::Sharp: return {"RailSharp", "turns more than 45 degrees"};
    case rail::Error::Last: return {"RailLast", "does not join the rails at its end"};
    case rail::Error::Links: return {"RailLinks", "more than 6 rails at a point"};
    case rail::Error::Duplicate: return {"RailDuplicate", "this rail is already there"};
    default: return {};
    }
}

// The next step of the line toward the cursor.
TrackStep next_step(const std::vector<RailObject>& rails) {
    TrackStep s;
    const rail::Point from = g_line.back(), mouse = mouse_on_level(g_line_level);
    double best = 1e9;
    for (const rail::Point& d : lattice_steps()) {
        const rail::Point to = {rail::round3(from.x + d.x), rail::round3(from.y + d.y)};
        if (distance(to, mouse) < best) {
            best = distance(to, mouse);
            s.to = to;
        }
    }
    const rail::Plan plan = rail::plan(rails_of(rails), {from, s.to}, {}, g_line_level - 4 + rail::ALTITUDE);
    if (plan.error != rail::Error::None) {
        s.reason = reason_of(plan.error);
        return s;
    }
    if (g_line.size() >= 2) {   // the turn from the line's previous step (plan_track's rule)
        const rail::Point before = g_line[g_line.size() - 2];
        const double h0 = std::atan2(from.y - before.y, from.x - before.x) * 180.0 / 3.14159265358979323846;
        const double h1 = std::atan2(s.to.y - from.y, s.to.x - from.x) * 180.0 / 3.14159265358979323846;
        const double turn = std::fmod(std::fmod(h1 - h0 + 180.0, 360.0) + 360.0, 360.0) - 180.0;
        if (std::fabs(turn) > 45.0) {
            s.reason = reason_of(rail::Error::Sharp);
            return s;
        }
    }
    s.valid = true;
    s.segment = plan.segments[0];
    return s;
}

void end_line() {
    if (!g_line.empty()) MESSAGE("MsgTrackEnds", "track: the line ends (%u rails)", static_cast<unsigned>(g_line.size() - 1));
    g_line.clear();
    g_line_cells.clear();
    g_line_original.clear();
}

void start_line() {
    const std::vector<RailObject> rails = map_rails();
    const rail::Point p = line_start(rails);
    end_line();
    g_line = {p};
    g_ramp = 0;
    if (uint8_t* r = rail_ending_at(rails, p, &g_line_level)) {
        g_line_level_known = true;
        for (int k = 0; k < 4; ++k)   // the line goes on in the rail's kind
            if (RAIL_KINDS[k] == nvid_of(r)) g_rail_kind = k;
    } else {
        // the ground under the line's first rail decides its level (map_tool.py track); until
        // it is laid, the ground at the point stands in for the preview
        g_line_level_known = false;
        g_line_level = ground_cell((std::max)(0, (std::min)(grid_width() - 1, static_cast<int>(p.x) / 8)),
                                   (std::max)(0, (std::min)(grid_height() - 1, static_cast<int>(p.y) / 8))) +
                       (swamp_rail(RAIL_KINDS[g_rail_kind]) ? SWAMP_LIFT : 0);
    }
    g_line_nvid = RAIL_KINDS[g_rail_kind];
    MESSAGE("MsgTrackFrom", "track: a line from %.1f,%.1f", p.x, p.y);
}

void lay_step() {
    const std::vector<RailObject> rails = map_rails();
    const TrackStep s = next_step(rails);
    if (!s.valid) {
        MESSAGE("MsgTrackStep", "track: %.1f,%.1f -> %.1f,%.1f %s", g_line.back().x, g_line.back().y, s.to.x, s.to.y,
                s.reason.get());
        return;
    }
    g_line_nvid = RAIL_KINDS[g_rail_kind];
    const bool swamp = swamp_rail(g_line_nvid);
    if (!g_line_level_known) {   // the first rail of a new line: the ground under its centre
        g_line_level = ground_cell((std::max)(0, (std::min)(grid_width() - 1, static_cast<int>(s.segment.x) / 8)),
                                   (std::max)(0, (std::min)(grid_height() - 1, static_cast<int>(s.segment.y) / 8))) +
                       (swamp ? SWAMP_LIFT : 0);
        g_line_level_known = true;
    }
    const rail::Point from = g_line.back();
    const int ramp = g_ramp;
    // the ground (not under a swamp rail: its embankment stands on the swamp): the cells the rail
    // crosses to the level - along a ramp sloping to the level 11 higher or lower -, those around
    // half way to it
    auto level_at = [&](const rail::Cell& c) {
        const double ux = s.to.x - from.x, uy = s.to.y - from.y, len2 = ux * ux + uy * uy;
        double t = len2 > 0 ? ((c.first * 8 + 4 - from.x) * ux + (c.second * 8 + 4 - from.y) * uy) / len2 : 0;
        t = (std::max)(0.0, (std::min)(1.0, t));
        return static_cast<short>(g_line_level + std::lround(ramp * rail::RAMP * t));
    };
    std::set<rail::Cell> cells, buffer;
    rail::cells({s.segment}, grid_width(), grid_height(), cells, buffer);
    std::vector<GridChange> changes;
    auto original = [&](const rail::Cell& c) {
        auto it = g_line_original.find(c);
        if (it == g_line_original.end()) it = g_line_original.emplace(c, ground_cell(c.first, c.second)).first;
        return it->second;
    };
    if (!swamp) {
        for (const rail::Cell& c : cells) {
            original(c);
            g_line_cells.insert(c);
            const short before = ground_cell(c.first, c.second);
            if (before != level_at(c)) changes.push_back({c.first, c.second, before, level_at(c)});
        }
        for (const rail::Cell& c : buffer) {
            if (g_line_cells.count(c)) continue;   // a cell of the track stays at its level
            const short before = ground_cell(c.first, c.second);
            const short after = static_cast<short>(rail::ramp(original(c), level_at(c)));
            if (before != after) changes.push_back({c.first, c.second, before, after});
        }
    }
    for (const GridChange& g : changes) set_ground_cell(g.cx, g.cy, g.after);
    const float z = line_rail_z(g_line_nvid) - (ramp < 0 ? rail::RAMP : 0);
    uint8_t* r = create(g_line_nvid, static_cast<float>(s.segment.x), static_cast<float>(s.segment.y), z,
                        step_direction(s.segment, from, ramp), 0);
    if (!r) {
        for (const GridChange& g : changes) set_ground_cell(g.cx, g.cy, g.before);
        MESSAGE("MsgTrackNotCreated", "track: VID %d could not be created", g_line_nvid);
        return;
    }
    std::vector<uint8_t*> made{r};
    const int bed = swamp ? VID_EMBANKMENT : g_bed;   // its gravel bed, or the swamp rail's embankment
    if (bed)
        if (uint8_t* b = create(bed, at<float>(r, O_X), at<float>(r, O_Y), at<float>(r, O_Z) - (swamp ? EMBANKMENT_DEPTH : BED_DEPTH),
                                at<int>(r, O_DIRECTION), 0))
            made.push_back(b);
    std::vector<State> before = states(made);
    for (State& b : before) b.present = false;
    commit({"OpTrack", "track"}, made, before);
    g_undo.back().grid = changes;
    g_line.push_back(s.to);
    g_line_level += ramp * rail::RAMP;
    g_ramp = 0;
    MESSAGE("MsgTrackRail", "track: rail of shape %d at %.1f,%.1f z %d (%u cells of ground)", s.segment.shape, s.segment.x,
            s.segment.y, static_cast<int>(z), static_cast<unsigned>(changes.size()));
}

// The rails of a section: from a rail along the track as long as one other rail continues it,
// up to a junction or an end.
std::vector<uint8_t*> section_of(uint8_t* start) {
    const std::vector<RailObject> rails = map_rails();
    std::vector<uint8_t*> section{start};
    for (size_t i = 0; i < section.size() && section.size() < 200; ++i) {
        const RailObject* current = nullptr;
        for (const RailObject& r : rails)
            if (r.object == section[i]) current = &r;
        if (!current) continue;
        for (int e = 0; e < 2; ++e) {
            const rail::Point p = rail::end_of(current->rail, e);
            std::vector<uint8_t*> others;
            for (const RailObject& r : rails)
                if (r.object != current->object)
                    for (int k = 0; k < 2; ++k) {
                        const rail::Point q = rail::end_of(r.rail, k);
                        if (rail::close(q.x, p.x) && rail::close(q.y, p.y)) others.push_back(r.object);
                    }
            if (others.size() == 1 && std::find(section.begin(), section.end(), others[0]) == section.end())
                section.push_back(others[0]);
        }
    }
    return section;
}
// ---- the ground ----
//
// The heights of the ground are a short per 8 x 8 px cell (World.grid, saved as GRID). The ground's
// picture is drawn as it is - the heights are what objects stand on and what the trains climb - so
// they are edited to match the picture: the Ground height tool raises (2 a step), lowers,
// flattens (to the height where the stroke began) or smooths (the mean of the 3 x 3 cells) the
// cells in a circle around the cursor, while the button is held. The cells of the track and the
// ring around them are left as the Track tool made them (MAP_RULES.md 3: flat at the track's
// height, a half ramp around). When the button is let go the objects stand on the new ground
// again (their height above it is kept: a fort's guns on its wall) and the stroke is one step
// of the history. Every height goes through World_SetGroundZ.

enum class Brush { Raise, Lower, Flatten, Smooth };
const char* const BRUSH_NAMES[] = {"raise", "lower", "flatten", "smooth"};
const char* const BRUSH_KEYS[] = {"BrushRaise", "BrushLower", "BrushFlatten", "BrushSmooth"};
Brush g_brush = Brush::Raise;
const char* brush_name() { return ui_text(BRUSH_KEYS[static_cast<int>(g_brush)], BRUSH_NAMES[static_cast<int>(g_brush)]); }
int g_radius = 2;   // cells of the brush (the Height tool) / 8 px of the scatter (Decals)
constexpr int MAX_RADIUS = 12, MIN_Z = 0, MAX_Z = 400, BRUSH_STEP = 2;
int g_spacing = 16;   // Decals: the least distance between two decals, px (< and >)
constexpr int MIN_SPACING = 4, MAX_SPACING = 64, SPACING_STEP = 4;

struct Stroke {
    bool active = false;
    int level = 0;                         // Flatten: the height where it began
    rail::Cell last = {-1, -1};            // the cell of the last step (one step a cell)
    std::map<rail::Cell, short> before;    // the cells changed, as they were
    std::set<rail::Cell> track;            // the track's cells and their ring: not touched
    std::vector<uint8_t*> objects;         // the objects that may stand on changed ground
    std::vector<float> lift;               // their height above the ground at the start
    std::vector<State> object_before;
    std::vector<uint8_t*> made;            // Decals: the ones placed
    std::vector<uint8_t*> erased;          // Decals: the ones taken away
    std::vector<State> erased_before;
    float last_x = -1e9f, last_y = -1e9f;  // Decals: where the last ones went
} g_stroke;

// The cell under the cursor (on the ground) and its world point.
rail::Cell cursor_cell(float& x, float& y) {
    float z;
    map_point(g_mouse, vid_by_number(VID_SEMAPHORE), x, y, z);
    return {static_cast<int>(std::floor(x / 8)), static_cast<int>(std::floor(y / 8))};
}

std::set<rail::Cell> track_cells() {
    std::vector<rail::Segment> segments;
    for (const RailObject& r : map_rails()) {
        const rail::Point a = rail::end_of(r.rail, 0), b = rail::end_of(r.rail, 1);
        segments.push_back({a, b, r.rail.x, r.rail.y, r.rail.shape, a, b});
    }
    std::set<rail::Cell> cells, ring;
    rail::cells(segments, grid_width(), grid_height(), cells, ring);
    cells.insert(ring.begin(), ring.end());
    return cells;
}

bool follows_ground(uint8_t* o) {   // objects that stand on the ground (not rails, cars, screen objects)
    return pickable(o) && class_of(o) != B_RAIL && class_of(o) != B_ENGINE;
}

void start_height_stroke() {
    g_stroke = Stroke();
    g_stroke.active = true;
    g_stroke.track = track_cells();
    float x, y;
    const rail::Cell c = cursor_cell(x, y);
    if (c.first >= 0 && c.first < grid_width() && c.second >= 0 && c.second < grid_height())
        g_stroke.level = ground_cell(c.first, c.second);
    for_each_object([&](uint8_t* o) {
        if (!follows_ground(o)) return;
        g_stroke.objects.push_back(o);
        g_stroke.lift.push_back(at<float>(o, O_Z) - ground_z(vid_of(o), at<float>(o, O_X), at<float>(o, O_Y)));
        g_stroke.object_before.push_back(state_of(o));
    });
}

void height_step() {
    float x, y;
    const rail::Cell centre = cursor_cell(x, y);
    if (centre == g_stroke.last) return;
    g_stroke.last = centre;
    const int w = grid_width(), h = grid_height();
    std::vector<std::pair<rail::Cell, short>> heights;
    for (int dy = -g_radius; dy <= g_radius; ++dy)
        for (int dx = -g_radius; dx <= g_radius; ++dx) {
            if (dx * dx + dy * dy > g_radius * g_radius) continue;
            const rail::Cell c = {centre.first + dx, centre.second + dy};
            if (c.first < 0 || c.first >= w || c.second < 0 || c.second >= h || g_stroke.track.count(c)) continue;
            const int z = ground_cell(c.first, c.second);
            int to = z;
            if (g_brush == Brush::Raise) to = z + BRUSH_STEP;
            else if (g_brush == Brush::Lower) to = z - BRUSH_STEP;
            else if (g_brush == Brush::Flatten) to = g_stroke.level;
            else {
                int sum = 0, n = 0;
                for (int ny = -1; ny <= 1; ++ny)
                    for (int nx = -1; nx <= 1; ++nx) {
                        const int px = c.first + nx, py = c.second + ny;
                        if (px < 0 || px >= w || py < 0 || py >= h) continue;
                        sum += ground_cell(px, py);
                        ++n;
                    }
                to = static_cast<int>(std::lround(static_cast<double>(sum) / n));
            }
            to = (std::max)(MIN_Z, (std::min)(MAX_Z, to));
            if (to != z) heights.push_back({c, static_cast<short>(to)});
        }
    for (const auto& ch : heights) {   // all from the heights before this step (the smoothing)
        g_stroke.before.emplace(ch.first, ground_cell(ch.first.first, ch.first.second));
        set_ground_cell(ch.first.first, ch.first.second, ch.second);
    }
}

void end_height_stroke() {
    if (!g_stroke.active) return;
    g_stroke.active = false;
    std::vector<GridChange> changes;
    for (const auto& c : g_stroke.before) {
        const short now = ground_cell(c.first.first, c.first.second);
        if (now != c.second) changes.push_back({c.first.first, c.first.second, c.second, now});
    }
    if (changes.empty()) return;
    std::vector<uint8_t*> moved;
    std::vector<State> moved_before;
    for (size_t i = 0; i < g_stroke.objects.size(); ++i) {
        uint8_t* o = g_stroke.objects[i];
        const float z = ground_z(vid_of(o), at<float>(o, O_X), at<float>(o, O_Y)) + g_stroke.lift[i];
        if (std::fabs(z - at<float>(o, O_Z)) < 0.5f) continue;
        reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(o, nullptr, at<float>(o, O_X), at<float>(o, O_Y), z);
        moved.push_back(o);
        moved_before.push_back(g_stroke.object_before[i]);
    }
    commit({"OpHeight", "ground height"}, moved, moved_before);
    g_undo.back().grid = changes;
    MESSAGE("MsgGround", "ground: %s - %u cells, %u objects on them", brush_name(),
            static_cast<unsigned>(changes.size()), static_cast<unsigned>(moved.size()));
}

// A building's footprint flattened at the height under its middle (ground.h), as grid changes.
std::vector<GridChange> flatten_under(uint8_t* o) {
    std::vector<GridChange> changes;
    const int w = grid_width(), h = grid_height();
    const int cx = static_cast<int>(at<float>(o, O_X)) / 8, cy = static_cast<int>(at<float>(o, O_Y)) / 8;
    if (!grid() || cx < 0 || cx >= w || cy < 0 || cy >= h) return changes;
    for (const ground::Height& c : ground::footprint(grid(), w, h, at<float>(o, O_X), at<float>(o, O_Y),
                                                     at<float>(vid_of(o), V_SIZE_X), at<float>(vid_of(o), V_SIZE_Y),
                                                     ground_cell(cx, cy))) {
        const short before = ground_cell(c.cx, c.cy);
        if (before == c.z) continue;
        changes.push_back({c.cx, c.cy, before, c.z});
        set_ground_cell(c.cx, c.cy, c.z);
    }
    return changes;
}

// ---- decals in quantity ----
//
// The Decals tool places the decal chosen with [ and ] (or picked in the palette: a click on the
// chosen Decals entry opens it). A click puts one at the cursor; dragging scatters them around it -
// 1 + the radius of them a step, in a circle of 8 px x the radius (+ and -), radius 0 one a step.
// Each is turned at random (a decal's directions are its variants) and none goes nearer than the
// spacing (< and >) to a decal already there, so they do not stack. With Shift it takes away every
// decal in the circle (the 61 VIDs of class 8, the beds as well). A stroke is one step of the history.
//
// A map's first decal is the plant it has most of (its terrain's), else the round bush (936).

constexpr int DEFAULT_DECAL = 936;
// decals that are not ground cover: beds (BEDS), wrecks, buildings, track and industry pieces
constexpr int NOT_COVER[] = {93, 96, 98, 177, 266, 530, 627, 804, 842, 849, 869, 871, 888, 929, 937};

bool is_decal(int nvid) { return std::find(g_decals.begin(), g_decals.end(), nvid) != g_decals.end(); }

void choose_default_decal() {
    std::map<int, int> count;
    for_each_object([&](uint8_t* o) {
        const int n = nvid_of(o);
        if (!removed(o) && is_decal(n) && !is_bed(n) && std::find(std::begin(NOT_COVER), std::end(NOT_COVER), n) == std::end(NOT_COVER))
            ++count[n];
    });
    int best = DEFAULT_DECAL;
    for (const auto& c : count)
        if (c.second > (count.count(best) ? count[best] : 0)) best = c.first;
    const auto it = std::find(g_decals.begin(), g_decals.end(), best);
    if (it != g_decals.end()) g_decal = static_cast<int>(it - g_decals.begin());
}

uint32_t g_random = 12345;
int random_below(int n) {
    g_random = g_random * 1103515245u + 12345u;
    return n > 0 ? static_cast<int>((g_random >> 8) % static_cast<uint32_t>(n)) : 0;
}

// A decal (not a bed) nearer than d to (x, y).
bool decal_near(float x, float y, float d) {
    bool found = false;
    for_each_object([&](uint8_t* o) {
        if (found || removed(o) || is_bed(nvid_of(o)) || !is_decal(nvid_of(o))) return;
        if (std::hypot(at<float>(o, O_X) - x, at<float>(o, O_Y) - y) < d) found = true;
    });
    return found;
}

void decal_step(bool first) {
    float x, y;
    cursor_cell(x, y);
    const float r = 8.0f * g_radius, gap = static_cast<float>(g_spacing);
    if (!first && std::hypot(x - g_stroke.last_x, y - g_stroke.last_y) < (std::max)(gap, r))
        return;
    g_stroke.last_x = x;
    g_stroke.last_y = y;
    if (shift_down()) {
        for_each_object([&](uint8_t* o) {
            if (removed(o) || std::find(g_decals.begin(), g_decals.end(), nvid_of(o)) == g_decals.end()) return;
            if (std::hypot(at<float>(o, O_X) - x, at<float>(o, O_Y) - y) > (std::max)(r, 12.0f)) return;
            g_stroke.erased.push_back(o);
            g_stroke.erased_before.push_back(state_of(o));
        });
        for (size_t i = 0; i < g_stroke.erased.size(); ++i)
            if (!removed(g_stroke.erased[i])) {
                State gone = g_stroke.erased_before[i];
                gone.present = false;
                apply(g_stroke.erased[i], gone);
            }
        return;
    }
    uint8_t* v = vid_by_number(decal_nvid());
    if (!v) return;
    const int dirs = (std::max)(1, at<int>(v, V_ND_DIR));
    const bool scatter = !first && g_radius > 0;   // a click: one at the cursor
    const int count = scatter ? 1 + g_radius : 1;
    int placed = 0;
    for (int tries = 0; placed < count && tries < count * 4; ++tries) {
        float px = x, py = y;
        if (scatter) {   // a point of the circle
            do {
                px = x + (random_below(2001) - 1000) / 1000.0f * r;
                py = y + (random_below(2001) - 1000) / 1000.0f * r;
            } while (std::hypot(px - x, py - y) > r);
        }
        if (decal_near(px, py, gap)) {
            if (!scatter) break;
            continue;
        }
        const int direction = dirs > 1 ? random_below(dirs) * 256 / dirs : 0;
        if (uint8_t* o = create(decal_nvid(), px, py, ground_z(v, px, py), direction, 0)) {
            g_stroke.made.push_back(o);
            ++placed;
        }
    }
    if (first && placed == 0)
        MESSAGE("MsgDecalTaken", "a decal is nearer than %d px here - the spacing (< >) keeps them apart", g_spacing);
}

void end_decal_stroke() {
    if (!g_stroke.active) return;
    g_stroke.active = false;
    std::vector<uint8_t*> objects = g_stroke.made;
    std::vector<State> before = states(objects);
    for (State& s : before) s.present = false;
    objects.insert(objects.end(), g_stroke.erased.begin(), g_stroke.erased.end());
    before.insert(before.end(), g_stroke.erased_before.begin(), g_stroke.erased_before.end());
    if (objects.empty()) return;
    commit(g_stroke.erased.empty() ? UiText{"OpDecals", "decals"} : UiText{"OpDecalsAway", "decals away"}, objects, before);
    MESSAGE("MsgDecals", "decals: %u placed (VID %d), %u taken away", static_cast<unsigned>(g_stroke.made.size()), decal_nvid(),
            static_cast<unsigned>(g_stroke.erased.size()));
}

// ---- the ground's picture: Paint, Water, Region ----
//
// The Paint tool paints the ground's picture with a texture sampled from the campaign grounds
// ([ and ] pick it, + and - the brush, Shift puts the original picture back); the Water tool lays
// water or lava the way the missions have it (MAP_RULES.md 2; W picks the kind): the picture is
// lowered under the brush to the bed - the water's level - 25, with a sloping shore, and painted
// with the bed's texture; the GRID cells there go down to the bed (half way in a ring, the track's
// cells left alone); the water tiles of the kind come where they are missing, at the map's level
// of that water (a new one: 20 below the ground where the first stroke begins, swamp water 16,
// lava 12, so their beds stay above a lower kind's level and its tiles do not show); when the stroke
// ends its generators are scattered over the water as far apart as the missions have them, and
// one region of the kind covers the painted water, turning explosions there into splashes. Shift
// takes the water away again (the original picture and GRID, the tiles and generators no longer
// over water). The Region tool draws a region's rectangle with the mouse; its panel sets the rest.
// A stroke is one step of the history (the picture's rectangle, the cells, the objects).

const char* ground_texture_name(int i);
void open_context(uint8_t* o);
int g_texture = 0;              // Paint: the texture ([ and ], editor\textures.tsv's order)
int g_water_kind = 0;           // Water: WATER_KINDS (W)
int g_paint_radius = 40;        // px, + and -
constexpr int MIN_PAINT_RADIUS = 8, MAX_PAINT_RADIUS = 200, PAINT_STEP = 8, WATER_BED = 25;
std::map<rail::Cell, short> g_grid_original;   // the cells the Water tool changed, as the level had them

struct GroundStroke {
    bool active = false, water = false, erase = false;
    int level = 0, bed = 0;                  // Water: the water's level, its bed (GRID levels)
    std::map<rail::Cell, short> grid_before;
    std::set<std::pair<int, int>> tiles;     // Water: the 128 squares with a tile of the kind
    std::set<std::pair<int, int>> others;    // ... and with a tile of another kind (left to it)
    std::vector<uint8_t*> made;              // Water: the tiles made
    std::vector<std::pair<float, float>> dabs;   // the brush's places (in the water: the generators' first places)
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;   // the screen rectangle it went over
    float last_x = -1e9f, last_y = -1e9f;
} g_gstroke;
std::set<rail::Cell> track_cells();   // the ground, above

bool ground_paintable() {
    if (ground_ready()) return true;
    uint8_t* ground = nullptr;
    for_each_object([&](uint8_t* o) {
        if (!ground && nvid_of(o) == VID_GROUND) ground = o;
    });
    const char* map = at<const char*>(world(), W_LEVEL_FILE);
    std::string why;
    if (!ground || !map || !ground_prepare(map, at<float>(ground, O_X), at<float>(ground, O_Y), vid_of(ground), why)) {
        MESSAGE("MsgGroundNot", "the ground's picture cannot be read (%s)", why.empty() ? "no ground object" : why.c_str());
        return false;
    }
    return true;
}

// The level of the map's water of a VID (its tiles'), or -1.
int water_level_of(int nvid) {
    int level = -1;
    for_each_object([&](uint8_t* o) {
        if (level < 0 && !removed(o) && nvid_of(o) == nvid) level = static_cast<int>(std::lround(at<float>(o, O_Z)));
    });
    return level;
}

std::pair<int, int> tile_square(float sx, float sy, int level) {   // the tile over a screen point
    return {static_cast<int>(std::floor(sx / 128)), static_cast<int>(std::floor((sy + level) / 128))};
}

void start_ground_stroke(bool water) {
    if (!ground_paintable()) return;
    g_gstroke = GroundStroke();
    g_gstroke.active = true;
    g_gstroke.water = water;
    g_gstroke.erase = shift_down();
    if (water) {
        const WaterKind& k = WATER_KINDS[g_water_kind];
        float x, y, z;
        map_point(g_mouse, vid_by_number(VID_SEMAPHORE), x, y, z);
        const int cx = (std::max)(0, (std::min)(grid_width() - 1, static_cast<int>(x) / 8));
        const int cy = (std::max)(0, (std::min)(grid_height() - 1, static_cast<int>(y) / 8));
        g_gstroke.level = water_level_of(k.nvid);
        if (g_gstroke.level < 0) g_gstroke.level = (std::max)(3, ground_cell(cx, cy) - k.below);
        g_gstroke.bed = (std::max)(0, g_gstroke.level - WATER_BED);
        // a water of another kind lower than this one would show through where its tiles reach:
        // the bed stays above its level (a tile is drawn wherever the picture is below its level)
        for (const WaterKind& other : WATER_KINDS) {
            const int l = other.nvid == k.nvid ? -1 : water_level_of(other.nvid);
            if (l >= 0 && l < g_gstroke.level) g_gstroke.bed = (std::max)(g_gstroke.bed, (std::min)(l + 2, g_gstroke.level - 2));
        }
        for_each_object([&](uint8_t* o) {
            if (removed(o) || !is_water_tile(nvid_of(o))) return;
            const std::pair<int, int> s{static_cast<int>(std::floor(at<float>(o, O_X) / 128)), static_cast<int>(std::floor(at<float>(o, O_Y) / 128))};
            (nvid_of(o) == k.nvid ? g_gstroke.tiles : g_gstroke.others).insert(s);
        });
    }
    ground_stroke_begin();
}

void ground_dab(float sx, float sy, bool first);

// A step of the stroke: dabs along the way from the last one, a third of the brush apart, so a fast
// drag paints a line and not a row of circles.
void ground_step(bool first) {
    if (!g_gstroke.active) return;
    float sx, sy;
    mouse_on_map(sx, sy);
    const float spacing = g_paint_radius * 0.3f;
    if (first) {
        ground_dab(sx, sy, true);
        return;
    }
    const float d = std::hypot(sx - g_gstroke.last_x, sy - g_gstroke.last_y);
    if (d < spacing) return;
    const float x0 = g_gstroke.last_x, y0 = g_gstroke.last_y;
    const int n = static_cast<int>(d / spacing);
    for (int i = 1; i <= n; ++i) ground_dab(x0 + (sx - x0) * i / n, y0 + (sy - y0) * i / n, false);
}

void ground_dab(float sx, float sy, bool first) {
    const float r = static_cast<float>(g_paint_radius);
    g_gstroke.last_x = sx;
    g_gstroke.last_y = sy;
    g_gstroke.dabs.push_back({sx, sy});
    g_gstroke.x0 = (std::min)(g_gstroke.x0, sx - r);
    g_gstroke.y0 = (std::min)(g_gstroke.y0, sy - r);
    g_gstroke.x1 = (std::max)(g_gstroke.x1, sx + r);
    g_gstroke.y1 = (std::max)(g_gstroke.y1, sy + r);
    if (!g_gstroke.water) {
        ground_paint(sx, sy, r, g_texture, g_gstroke.erase);
        return;
    }
    const WaterKind& k = WATER_KINDS[g_water_kind];
    ground_lower(sx, sy, r, g_gstroke.bed, g_gstroke.erase ? -1 : k.bed_texture, g_gstroke.erase);
    // the GRID cells whose ground shows in the brush (their screen place: y - z)
    static std::set<rail::Cell> track;
    if (first) track = track_cells();
    const int cx0 = (std::max)(0, static_cast<int>((sx - r) / 8)), cx1 = (std::min)(grid_width() - 1, static_cast<int>((sx + r) / 8));
    const int cy0 = (std::max)(0, static_cast<int>((sy - r) / 8)), cy1 = (std::min)(grid_height() - 1, static_cast<int>((sy + r + 240) / 8));
    for (int cy = cy0; cy <= cy1; ++cy)
        for (int cx = cx0; cx <= cx1; ++cx) {
            const rail::Cell c{cx, cy};
            if (track.count(c)) continue;
            const short now = ground_cell(cx, cy);
            const short original = g_grid_original.count(c) ? g_grid_original[c] : now;
            const float d = std::hypot(cx * 8 + 4 - sx, cy * 8 + 4 - original - sy) / r;
            if (d >= 1) continue;
            short want = now;
            if (g_gstroke.erase) want = original;
            else want = static_cast<short>((std::min)(static_cast<int>(now), d < 0.55f ? g_gstroke.bed : rail::ramp(original, g_gstroke.bed)));
            if (want == now) continue;
            g_grid_original.emplace(c, now);
            g_gstroke.grid_before.emplace(c, now);
            set_ground_cell(cx, cy, want);
        }
    if (g_gstroke.erase) return;
    // the tiles of the kind over the brush, where missing
    const auto a = tile_square(sx - r, sy - r, g_gstroke.level), b = tile_square(sx + r, sy + r, g_gstroke.level);
    for (int j = a.second; j <= b.second; ++j)
        for (int i = a.first; i <= b.first; ++i) {
            // a square with another kind's tile keeps it: a tile shows wherever the picture is below
            // its level, so two kinds in one square would show each other
            if (i < 0 || j < 0 || i * 128 >= grid_width() * 8 || j * 128 >= grid_height() * 8 || g_gstroke.tiles.count({i, j}) ||
                g_gstroke.others.count({i, j}))
                continue;
            if (uint8_t* t = create(k.nvid, 64.0f + 128 * i, 64.0f + 128 * j, static_cast<float>(g_gstroke.level), 0, 0)) {
                g_gstroke.made.push_back(t);
                g_gstroke.tiles.insert({i, j});
            }
        }
}

// Water at a screen point: the picture lower than the level there.
bool under_water(float sx, float sy, int level, int margin = 0) { return ground_level_at(sx, sy) < level - margin; }

// A tile's square still has water somewhere.
bool square_wet(int i, int j, int level) {
    for (int y = 0; y < 128; y += 8)
        for (int x = 0; x < 128; x += 8)
            if (under_water(i * 128.0f + x, j * 128.0f - level + y, level)) return true;
    return false;
}

void end_ground_stroke() {
    if (!g_gstroke.active) return;
    g_gstroke.active = false;
    const std::shared_ptr<PaintPatch> paint = ground_stroke_end();
    std::vector<uint8_t*> objects;
    std::vector<State> before;
    for (uint8_t* o : g_gstroke.made) {   // the tiles made: there was none
        objects.push_back(o);
        State none = state_of(o);
        none.present = false;
        before.push_back(none);
    }
    unsigned effects = 0, gone = 0;
    if (g_gstroke.water) {
        const WaterKind& k = WATER_KINDS[g_water_kind];
        const int level = g_gstroke.level;
        auto add = [&](uint8_t* o) {
            if (!o) return;
            objects.push_back(o);
            State none = state_of(o);
            none.present = false;
            before.push_back(none);
        };
        auto take = [&](uint8_t* o) {
            objects.push_back(o);
            before.push_back(state_of(o));
            State s = before.back();
            s.present = false;
            apply(o, s);
            ++gone;
        };
        if (!g_gstroke.erase) {
            // its generators over the water, as far apart as the missions have them
            for (const auto& e : {std::make_pair(k.effect, k.effect_spacing), std::make_pair(k.second, k.second_spacing)}) {
                if (!e.first) continue;
                const float s = static_cast<float>(e.second);
                // the places to try: the brush's own (in the water whatever its width), then a grid
                std::vector<std::pair<float, float>> places = g_gstroke.dabs;
                for (float y = g_gstroke.y0 + s / 2; y < g_gstroke.y1; y += s)
                    for (float x = g_gstroke.x0 + s / 2; x < g_gstroke.x1; x += s)
                        places.push_back({x + (random_below(1001) - 500) / 1000.0f * s * 0.6f, y + (random_below(1001) - 500) / 1000.0f * s * 0.6f});
                for (const auto& place : places) {
                    const float px = place.first, py = place.second;
                    if (!under_water(px, py, level, 3)) continue;
                    bool close_by = false;
                    for_each_object([&](uint8_t* o) {
                        if (!close_by && !removed(o) && nvid_of(o) == e.first &&
                            std::hypot(at<float>(o, O_X) - px, at<float>(o, O_Y) - at<float>(o, O_Z) - py) < s * 0.8f)
                            close_by = true;
                    });
                    if (close_by) continue;
                    if (uint8_t* o = create(e.first, px, py + level + 1, static_cast<float>(level + 1), 0, 0)) {
                        add(o);
                        ++effects;
                    }
                }
            }
            // one region of the kind over the painted water: explosions there splash
            uint8_t* region = nullptr;
            for_each_object([&](uint8_t* o) {
                if (!region && !removed(o) && is_region(o) && nvid_of(o) == VID_REGION && region_splash(o) == k.splash &&
                    std::fabs(at<float>(o, O_Z) - level) < 2)
                    region = o;
            });
            float l = g_gstroke.x0, t = g_gstroke.y0, r = g_gstroke.x1, b = g_gstroke.y1;
            if (region) {
                objects.push_back(region);
                before.push_back(state_of(region));
                const float rx = at<float>(region, O_X), ry = at<float>(region, O_Y) - at<float>(region, O_Z);
                const float hw = at<float>(region, RG_WIDTH) / 2, hh = at<float>(region, RG_HEIGHT) / 2;
                l = (std::min)(l, rx - hw);
                t = (std::min)(t, ry - hh);
                r = (std::max)(r, rx + hw);
                b = (std::max)(b, ry + hh);
                reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(region, nullptr, (l + r) / 2, (t + b) / 2 + level, static_cast<float>(level));
            } else if ((region = create(VID_REGION, (l + r) / 2, (t + b) / 2 + level, static_cast<float>(level), 0, 0)) != nullptr) {
                add(region);
                set_region_splash(region, k.splash);
            }
            if (region) {
                at<float>(region, RG_WIDTH) = r - l;
                at<float>(region, RG_HEIGHT) = b - t;
            }
        } else {
            // the tiles and generators no longer over water go
            std::vector<uint8_t*> dry;
            for_each_object([&](uint8_t* o) {
                if (removed(o)) return;
                const int n = nvid_of(o);
                const float sx = at<float>(o, O_X), sy = at<float>(o, O_Y) - at<float>(o, O_Z);
                if (sx < g_gstroke.x0 - 128 || sx > g_gstroke.x1 + 128 || sy < g_gstroke.y0 - 128 || sy > g_gstroke.y1 + 128) return;
                if (n == k.nvid) {
                    if (!square_wet(static_cast<int>(std::floor(at<float>(o, O_X) / 128)), static_cast<int>(std::floor(at<float>(o, O_Y) / 128)),
                                    level))
                        dry.push_back(o);
                } else if ((n == k.effect || (k.second && n == k.second)) && std::fabs(at<float>(o, O_Z) - level - 1) < 3 &&
                           !under_water(sx, sy, level)) {
                    dry.push_back(o);
                }
            });
            for (uint8_t* o : dry) take(o);
        }
    }
    if (!paint && objects.empty() && g_gstroke.grid_before.empty()) return;
    commit(g_gstroke.water ? (g_gstroke.erase ? UiText{"OpWaterAway", "water away"} : UiText{"OpWater", "water"})
                           : UiText{"OpPaint", "paint"},
           objects, before);
    Operation& op = g_undo.back();
    op.paint = paint;
    for (const auto& c : g_gstroke.grid_before)
        op.grid.push_back({c.first.first, c.first.second, c.second, ground_cell(c.first.first, c.first.second)});
    if (g_gstroke.water)
        MESSAGE("MsgWater", "%s: level %d, %u tiles, %u effects, %u cells of ground, %u taken away", ui_text(WATER_KINDS[g_water_kind].key, WATER_KINDS[g_water_kind].english),
                g_gstroke.level, static_cast<unsigned>(g_gstroke.made.size()), effects, static_cast<unsigned>(g_gstroke.grid_before.size()), gone);
    else
        MESSAGE("MsgPainted", "ground painted: %s, %dx%d px", g_gstroke.erase ? ui_text("WordOriginal", "the original") : ground_texture_name(g_texture),
                paint ? paint->w : 0, paint ? paint->h : 0);
}

const char* ground_texture_name(int i) {
    const std::vector<GroundTexture>& t = ground_textures();
    if (i < 0 || i >= static_cast<int>(t.size())) return "?";
    return ui_text(t[i].key.c_str(), t[i].english.c_str());
}

// ---- regions: the tool and the panel ----

POINT g_region_start = {-1, -1};   // the Region tool's drag (screen coordinates of the map)
bool g_region_drag = false;

void finish_region(float sx, float sy) {
    g_region_drag = false;
    const float l = (std::min)(sx, static_cast<float>(g_region_start.x)), r = (std::max)(sx, static_cast<float>(g_region_start.x));
    const float t = (std::min)(sy, static_cast<float>(g_region_start.y)), b = (std::max)(sy, static_cast<float>(g_region_start.y));
    if (r - l < 16 || b - t < 16) {
        MESSAGE("MsgRegionSmall", "a region: drag its rectangle (16 px at least)");
        return;
    }
    // its height: the ground's level under its middle
    const int cx = (std::max)(0, (std::min)(grid_width() - 1, static_cast<int>((l + r) / 16)));
    const int level = ground_cell(cx, (std::max)(0, (std::min)(grid_height() - 1, static_cast<int>((t + b) / 16))));
    uint8_t* o = create(VID_REGION, (l + r) / 2, (t + b) / 2 + level, static_cast<float>(level), 0, 0);
    if (!o) return;
    at<float>(o, RG_WIDTH) = r - l;
    at<float>(o, RG_HEIGHT) = b - t;
    State none = state_of(o);
    none.present = false;
    commit({"OpRegion", "region"}, {o}, {none});
    g_selection = {o};
    open_context(o);
    MESSAGE("MsgRegion", "region %.0f x %.0f at %.0f,%.0f, level %d", r - l, b - t, at<float>(o, O_X), at<float>(o, O_Y), level);
}

// A change of a region as one step of the history.
template <typename F> void region_change(uint8_t* o, F f) {
    const std::vector<State> before = states({o});
    f();
    commit({"OpRegion", "region"}, {o}, before);
}

// A fog covers the ground of the region whose level is below low fully and fades out up to
// high (Graphics_DrawFog compares the depth buffer - 1024, 8 a level, with them), so the missions'
// values are levels of their maps' ground. The presets keep the missions' colours and depths and are
// set around the region's own level: from elow under it to bove over it.
struct FogPreset {
    UiText name;
    int below, above;
    uint32_t colour;
};
// the missions' fogs: l05's blue mist (5-35), l01's dust (20-60), l07's red haze (30-45), l13's
// lava glow (7-20)
const FogPreset FOGS[] = {{{"CtxFogNone", "NONE"}, 0, 0, 0xff000000},
                          {{"CtxFogMist", "MIST"}, 20, 10, 0xff006478},
                          {{"CtxFogDust", "DUST"}, 25, 15, 0xff966400},
                          {{"CtxFogHaze", "HAZE"}, 5, 10, 0xffff3296},
                          {{"CtxFogGlow", "GLOW"}, 5, 8, 0xffff6400}};

// The rectangles of the regions, dashed, the selected one brighter: blue a splash region over water,
// orange over lava, grey a fog, white a marker.
void draw_regions() {
    uint8_t* w = world();
    const float cam_x = at<float>(w, W_CAMERA_X), cam_y = at<float>(w, W_CAMERA_Y);
    const auto fill = reinterpret_cast<FillRect_t>(FILL_RECT);
    for_each_object([&](uint8_t* o) {
        if (removed(o) || !is_region(o)) return;
        const float x = at<float>(o, O_X) - cam_x, y = at<float>(o, O_Y) - at<float>(o, O_Z) - cam_y;
        const float hw = at<float>(o, RG_WIDTH) / 2, hh = at<float>(o, RG_HEIGHT) / 2;
        const bool selected = !g_selection.empty() && g_selection[0] == o;
        const int splash = region_splash(o);
        uint32_t c = splash == 1 ? 0x4080c0 : splash == 2 ? 0xf08020 : at<int>(o, RG_HIGH) > at<int>(o, RG_LOW) ? 0xa0a0a0 : 0xffffff;
        c |= selected ? 0xf0000000 : 0x90000000;
        for (float u = -hw; u < hw; u += 8) {
            fill(graphics(), nullptr, x + u, y - hh, x + (std::min)(u + 4, hw), y - hh + 1, c);
            fill(graphics(), nullptr, x + u, y + hh - 1, x + (std::min)(u + 4, hw), y + hh, c);
        }
        for (float v = -hh; v < hh; v += 8) {
            fill(graphics(), nullptr, x - hw, y + v, x - hw + 1, y + (std::min)(v + 4, hh), c);
            fill(graphics(), nullptr, x + hw - 1, y + v, x + hw, y + (std::min)(v + 4, hh), c);
        }
    });
    if (g_region_drag) {   // the rectangle being drawn
        float sx, sy;
        mouse_on_map(sx, sy);
        const float l = (std::min)(sx, static_cast<float>(g_region_start.x)) - cam_x, r = (std::max)(sx, static_cast<float>(g_region_start.x)) - cam_x;
        const float t = (std::min)(sy, static_cast<float>(g_region_start.y)) - cam_y, b = (std::max)(sy, static_cast<float>(g_region_start.y)) - cam_y;
        fill(graphics(), nullptr, l, t, r, t + 1, 0xf0ffffff);
        fill(graphics(), nullptr, l, b - 1, r, b, 0xf0ffffff);
        fill(graphics(), nullptr, l, t, l + 1, b, 0xf0ffffff);
        fill(graphics(), nullptr, r - 1, t, r, b, 0xf0ffffff);
    }
}

// ---- push lines ----
//
// A push line (script SetPushLine, MAP_RULES.md 4-5) runs from the rail point nearest to its
// begin to the one nearest to its end, both in screen coordinates (the point's y - z). The tool
// picks two points of the rail network, checks the line the way SetPushLine (0x4346b0) builds
// it - RailMap_FindNearestDot of both ends (points with links), the begin with a link ("Can't
// SetPushLine" otherwise), the end moved to the begin's neighbour nearest to it when both ends
// are the same point, and a way between them (RailDot_Flood, "Can't PushLineFindDot" otherwise),
// whose shortest form is found here breadth first - and keeps it in the mission's list
// (editor_mission.cpp writes them into the script). The semaphores of a line can be put at its
// ends the way the missions have them (MAP_RULES.md 7).

std::vector<uint8_t*> rail_dots() {
    const int count = at<int>(reinterpret_cast<void*>(G_RAIL_MAP), RAIL_MAP_COUNT);
    uint8_t** items = at<uint8_t**>(reinterpret_cast<void*>(G_RAIL_MAP), RAIL_MAP_ITEMS);
    std::vector<uint8_t*> out;
    for (int i = 0; i < count && items; ++i)
        if (items[i]) out.push_back(items[i]);
    return out;
}

int dot_links(uint8_t* d) { return (std::min)(6, at<int>(d, DOT_LINK_COUNT)); }
uint8_t* dot_link(uint8_t* d, int k) { return at<uint8_t*>(d, DOT_LINKS + k * DOT_LINK_SIZE); }
int screen_y(uint8_t* d) { return at<int>(d, DOT_Y) - at<int>(d, DOT_Z); }

// RailMap_FindNearestDot: the point with links whose screen position is nearest.
uint8_t* nearest_dot(int x, int y) {
    uint8_t* best = nullptr;
    int best_d = 0xfffffff;
    for (uint8_t* d : rail_dots()) {
        if (at<int>(d, DOT_LINK_COUNT) < 1) continue;
        const int dx = at<int>(d, DOT_X) - x, dy = screen_y(d) - y;
        if (dx * dx + dy * dy < best_d) {
            best_d = dx * dx + dy * dy;
            best = d;
        }
    }
    return best;
}

struct Trace {
    const char* error = nullptr;     // the log line of SetPushLine that would come
    std::vector<uint8_t*> path;      // from the begin to the end
};

Trace trace(const PushLine& l) {
    Trace t;
    uint8_t* begin = nearest_dot(l.bx, l.by);
    uint8_t* end = nearest_dot(l.ex, l.ey);
    if (!begin || !end) {
        t.error = "Can't found dot";
        return t;
    }
    if (end == begin) {   // the begin's neighbour nearest to the end
        int best_d = 999999;
        for (int k = 0; k < dot_links(begin); ++k) {
            uint8_t* o = dot_link(begin, k);
            const int dx = at<int>(o, DOT_X) - l.ex, dy = screen_y(o) - l.ey;
            if (dx * dx + dy * dy < best_d) {
                best_d = dx * dx + dy * dy;
                end = o;
            }
        }
    }
    std::map<uint8_t*, uint8_t*> from{{begin, nullptr}};
    std::vector<uint8_t*> queue{begin};
    for (size_t i = 0; i < queue.size() && !from.count(end); ++i)
        for (int k = 0; k < dot_links(queue[i]); ++k) {
            uint8_t* o = dot_link(queue[i], k);
            if (o && !from.count(o)) {
                from[o] = queue[i];
                queue.push_back(o);
            }
        }
    if (!from.count(end)) {
        t.error = "Can't PushLineFindDot";
        return t;
    }
    for (uint8_t* d = end; d; d = from[d]) t.path.insert(t.path.begin(), d);
    return t;
}

POINT dot_screen(uint8_t* d) { return {at<int>(d, DOT_X), screen_y(d)}; }

// A trace's error (the log line of SetPushLine) as the status line shows it.
const char* push_error(const char* error) {
    if (!error) return "";
    if (std::strcmp(error, "Can't found dot") == 0) return ui_text("PushNoDot", error);
    if (std::strcmp(error, "Can't PushLineFindDot") == 0) return ui_text("PushNoPath", error);
    return error;
}


void log_push_lines() {
    const std::vector<PushLine>* lists[] = {&mission_push_lines(), &mission_script_push_lines()};
    for (const auto* list : lists)
        for (const PushLine& l : *list) {
            const Trace t = trace(l);
            patch::log("push line %d,%d -> %d,%d flag %d (%s): %s, %u points", l.bx, l.by, l.ex, l.ey, l.flag,
                       list == &mission_push_lines() ? "editor" : "script", t.error ? t.error : "ok",
                       static_cast<unsigned>(t.path.size()));
        }
}

bool push_lines_editable() {
    if (mission_managed()) return true;
    MESSAGE("MsgNoEditorBlocks", "this map's script has no editor blocks: save the map under a new name to edit its push lines");
    return false;
}

// A click of the Push line tool at a point of the map (screen coordinates plus the camera).
void push_click(int x, int y) {
    if (!push_lines_editable()) return;
    uint8_t* d = nearest_dot(x, y);
    if (!d) {
        MESSAGE("MsgPushNoTrack", "push line: the map has no track");
        return;
    }
    const POINT p = dot_screen(d);
    if (!g_push_started) {
        // a click on an end of a line selects it
        const std::vector<PushLine>& lines = mission_push_lines();
        for (int i = 0; i < static_cast<int>(lines.size()); ++i)
            if ((std::abs(lines[i].bx - x) < 10 && std::abs(lines[i].by - y) < 10) ||
                (std::abs(lines[i].ex - x) < 10 && std::abs(lines[i].ey - y) < 10)) {
                g_push_selected = i;
                MESSAGE("MsgPushSelected", "push line %d,%d -> %d,%d, flag %d: T switches the flag, R turns it round, S puts its "
                        "semaphores, Del deletes it", lines[i].bx, lines[i].by, lines[i].ex, lines[i].ey, lines[i].flag);
                return;
            }
        g_push_started = true;
        g_push_begin = p;
        g_push_selected = -1;
        MESSAGE("MsgPushFrom", "push line from %d,%d: click its end", p.x, p.y);
        return;
    }
    const PushLine line{g_push_begin.x, g_push_begin.y, p.x, p.y, 1};
    const Trace t = trace(line);
    if (t.error || t.path.size() < 2) {
        MESSAGE("MsgPushBad", "push line %d,%d -> %d,%d: %s", line.bx, line.by, line.ex, line.ey,
                t.error ? push_error(t.error) : ui_text("WordSamePoint", "the same point"));
        return;
    }
    const std::vector<PushLine> before = mission_push_lines();
    mission_push_lines().push_back(line);
    commit_lines({"OpPushLine", "push line"}, before);
    g_push_started = false;
    g_push_selected = static_cast<int>(mission_push_lines().size()) - 1;
    MESSAGE("MsgPushMade", "push line %d,%d -> %d,%d over %u rail points, flag 1 (T switches it, S puts its semaphores)", line.bx,
            line.by, line.ex, line.ey, static_cast<unsigned>(t.path.size()));
}

// Del, T and R of the tool on the selected line.
void push_change(int key) {
    std::vector<PushLine>& lines = mission_push_lines();
    if (g_push_selected < 0 || g_push_selected >= static_cast<int>(lines.size())) {
        MESSAGE("MsgPushClickEnd", "push line: click an end of a line first");
        return;
    }
    if (!push_lines_editable()) return;
    const std::vector<PushLine> before = lines;
    PushLine& l = lines[g_push_selected];
    if (key == VK_DELETE) {
        lines.erase(lines.begin() + g_push_selected);
        g_push_selected = -1;
        commit_lines({"OpPushDelete", "delete push line"}, before);
        MESSAGE("MsgPushDeleted", "push line deleted");
        return;
    }
    if (key == 'T') l.flag = l.flag ? 0 : 1;
    if (key == 'R') l = {l.ex, l.ey, l.bx, l.by, l.flag};
    commit_lines(key == 'T' ? UiText{"OpPushFlag", "push flag"} : UiText{"OpPushTurn", "turn push line"}, before);
    MESSAGE("MsgPushLine", "push line %d,%d -> %d,%d, flag %d", l.bx, l.by, l.ex, l.ey, l.flag);
}

// The direction (0 north, 64 east, 128 south, 192 west) of a vector, in the 8 of a semaphore.
int direction_of(float dx, float dy) {
    const float a = std::atan2(dx, -dy) * 128.0f / 3.14159265f;
    return ((static_cast<int>(std::lround(a / 32.0f)) * 32) % 256 + 256) % 256;
}

// S: the semaphores of the selected line (MAP_RULES.md 7): a post faces the trains it is for and
// stands 24 px to their right - a red one at the end for the trains that would enter the line
// there, a green one at the begin for those it lets in (none at a dead end, a depot's); a line
// that begins past the edge of the map (the level's entry or exit) has both at its end, which
// moves 33 px into the map when it is nearer the edge than 40 px. A post already standing
// within 30 px of such a place is moved, turned and coloured instead of a new one.
// The posts of a line by the rule above (the places where they belong).
struct Post {
    float x, y;
    int direction;
    bool red;
};

std::vector<Post> line_posts(const Trace& t) {
    std::vector<Post> posts;
    if (t.error || t.path.size() < 2) return posts;
    auto pos = [](uint8_t* d, float& x, float& y) {
        x = static_cast<float>(at<int>(d, DOT_X));
        y = static_cast<float>(at<int>(d, DOT_Y));
    };
    const size_t n = t.path.size() - 1;
    float bx, by, b1x, b1y, ex, ey, e1x, e1y;
    pos(t.path[0], bx, by);
    pos(t.path[1], b1x, b1y);
    pos(t.path[n], ex, ey);
    pos(t.path[n - 1], e1x, e1y);
    const float width = static_cast<float>(grid_width() * 8), height = static_cast<float>(grid_height() * 8);
    auto outside = [&](float x, float y) { return x < 0 || y < 0 || x >= width || y >= height; };
    auto near_edge = [&](float x, float y) { return x < 40 || y < 40 || x > width - 40 || y > height - 40; };
    float ax = ex, ay = ey;   // the anchor of the end's posts
    if (near_edge(ex, ey)) {
        const float len = std::hypot(ex - e1x, ey - e1y);
        if (len > 0) {
            ax += (ex - e1x) / len * 33;
            ay += (ey - e1y) / len * 33;
        }
    }
    auto post = [&](float x, float y, float mx, float my, bool is_red) {   // m: the trains' way
        const float len = std::hypot(mx, my);
        if (len <= 0) return;
        posts.push_back({x - my / len * 24, y + mx / len * 24, direction_of(-mx, -my), is_red});
    };
    post(ax, ay, e1x - ex, e1y - ey, true);   // trains entering at the end, against the line
    if (outside(bx, by)) post(ax, ay, ex - e1x, ey - e1y, false);
    else if (dot_links(t.path[0]) > 1) post(bx, by, b1x - bx, b1y - by, false);
    return posts;
}

// The post standing within 30 px of a place (not one of `taken`).
uint8_t* post_near(float x, float y, const std::vector<uint8_t*>& taken) {
    uint8_t* found = nullptr;
    for_each_object([&](uint8_t* o) {
        if (!found && !removed(o) && nvid_of(o) == VID_SEMAPHORE && std::find(taken.begin(), taken.end(), o) == taken.end() &&
            std::hypot(at<float>(o, O_X) - x, at<float>(o, O_Y) - y) < 30)
            found = o;
    });
    return found;
}

// Puts the posts: one standing near a place is moved, turned and coloured, the others are made;
// what they were goes into `objects` / `before` (a made one was not there).
void put_posts(const std::vector<Post>& posts, std::vector<uint8_t*>& objects, std::vector<State>& before) {
    uint8_t* v = vid_by_number(VID_SEMAPHORE);
    if (!v) return;
    for (const Post& p : posts) {
        uint8_t* found = post_near(p.x, p.y, objects);
        const float z = ground_z(v, p.x, p.y);
        if (found) {
            objects.push_back(found);
            before.push_back(state_of(found));
            reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(found, nullptr, p.x, p.y, z);
            reinterpret_cast<SetInt_t>(OBJECT_SET_DIRECTION)(found, nullptr, p.direction);
        } else {
            found = create(VID_SEMAPHORE, p.x, p.y, z, p.direction, 0);
            if (!found) continue;
            objects.push_back(found);
            State gone = state_of(found);
            gone.present = false;
            before.push_back(gone);
        }
        if (red(found) != p.red) set_red(found, p.red);
    }
}

void push_semaphores() {
    const std::vector<PushLine>& lines = mission_push_lines();
    if (g_push_selected < 0 || g_push_selected >= static_cast<int>(lines.size())) {
        MESSAGE("MsgSemClickEnd", "semaphores: click an end of a push line first");
        return;
    }
    const Trace t = trace(lines[g_push_selected]);
    if (t.error || t.path.size() < 2) {
        MESSAGE("MsgSemNotOnTrack", "semaphores: the line is not on the track (%s)",
                t.error ? push_error(t.error) : ui_text("WordOnePoint", "one point"));
        return;
    }
    const std::vector<Post> posts = line_posts(t);
    std::vector<uint8_t*> objects;
    std::vector<State> before;
    put_posts(posts, objects, before);
    if (objects.empty()) return;
    commit({"OpSemaphores", "semaphores"}, objects, before);
    MESSAGE("MsgSemaphores", "%u semaphores at the push line: %s", static_cast<unsigned>(objects.size()),
            posts.size() > 1 ? ui_text("SemBoth", "red at its end, green at its begin") : ui_text("SemEnd", "red at its end"));
}

// ---- on the track: the factory, the repair point, posts, money crates ----
//
// The authors put these on the track (measured over the 20 missions): every factory (VID 102) at the
// dead end of a straight spur, turned the way out along it (the nearest rail 1-38 px away), every
// repair point (104) on a straight leg, turned along it like a gate; a post (152) 15-58 px beside the
// track (median 31), facing the trains it is for; a money crate (169, army 3) on the track (1-46 px
// from a rail, median 17). Placing them snaps there - elsewhere they are refused with the reason. A
// factory gets its exit push line as the missions have them - from inside the building along the
// spur, 140 px (they are 100-175), push_flag 1: built cars appear in the factory and are pushed out -
// and a repair point the line through its leg, 240 px (139-304), flag 1, one way in the direction it
// faces; both with their posts (MAP_RULES.md 7). Deleting or moving the building takes its line and
// posts with it; all of it is one step of the history.

constexpr float FACTORY_INSIDE = 22;     // the factory's middle from the dead end (sandbox, l05)
constexpr float FACTORY_LINE = 140, REPAIR_LINE_HALF = 120, POST_SIDE = 31;

struct Snap {
    bool ok = false;
    UiText reason;
    float x = 0, y = 0;
    int direction = 0;
};

float dot_wx(uint8_t* d) { return static_cast<float>(at<int>(d, DOT_X)); }
float dot_wy(uint8_t* d) { return static_cast<float>(at<int>(d, DOT_Y)); }

// The unit vector from a point along a link.
void link_vector(uint8_t* d, int k, float& vx, float& vy) {
    uint8_t* o = dot_link(d, k);
    vx = dot_wx(o) - dot_wx(d);
    vy = dot_wy(o) - dot_wy(d);
    const float len = std::hypot(vx, vy);
    if (len > 0) {
        vx /= len;
        vy /= len;
    }
}

int round32(int direction) { return ((direction + 16) & 255) / 32 * 32; }

// The rail point nearest to a map point (world), with at least `links` links, within `reach`.
uint8_t* point_near(float x, float y, int links, float reach) {
    uint8_t* best = nullptr;
    float best_d = reach;
    for (uint8_t* d : rail_dots()) {
        if (dot_links(d) < links) continue;
        const float dist = std::hypot(dot_wx(d) - x, dot_wy(d) - y);
        if (dist < best_d) {
            best_d = dist;
            best = d;
        }
    }
    return best;
}

// From a point along the track the way (vx, vy) goes, as far as `length` px (screen: x and y - z,
// as the missions' lines are measured) or the end of the track; the point reached.
uint8_t* walk(uint8_t* d, float vx, float vy, float length) {
    float done = 0;
    while (done < length) {
        int best = -1;
        float best_dot = 0.5f;
        for (int k = 0; k < dot_links(d); ++k) {
            float lx, ly;
            link_vector(d, k, lx, ly);
            if (lx * vx + ly * vy > best_dot) {
                best_dot = lx * vx + ly * vy;
                best = k;
            }
        }
        if (best < 0) break;
        uint8_t* next = dot_link(d, best);
        link_vector(d, best, vx, vy);
        done += std::hypot(dot_wx(next) - dot_wx(d), static_cast<float>(screen_y(next) - screen_y(d)));
        d = next;
    }
    return d;
}

Snap snap_factory(float x, float y) {
    Snap s;
    uint8_t* end = nullptr;
    float best = 150;
    for (uint8_t* d : rail_dots()) {
        if (dot_links(d) != 1) continue;
        const float dist = std::hypot(dot_wx(d) - x, dot_wy(d) - y);
        if (dist < best) {
            best = dist;
            end = d;
        }
    }
    if (!end) {
        s.reason = {"SnapFactory", "a factory stands at the dead end of a spur - no end of track near"};
        return s;
    }
    float vx, vy;
    link_vector(end, 0, vx, vy);
    s.ok = true;
    s.x = dot_wx(end) + vx * FACTORY_INSIDE;
    s.y = dot_wy(end) + vy * FACTORY_INSIDE;
    s.direction = round32(direction_of(vx, vy));
    return s;
}

// A straight piece: a point with two links going opposite ways.
bool straight(uint8_t* d) {
    if (dot_links(d) != 2) return false;
    float ax, ay, bx, by;
    link_vector(d, 0, ax, ay);
    link_vector(d, 1, bx, by);
    return ax * bx + ay * by < -0.95f;
}

Snap snap_repair(float x, float y) {
    Snap s;
    uint8_t* best = nullptr;
    float best_d = 100;
    for (uint8_t* d : rail_dots()) {
        if (!straight(d)) continue;
        const float dist = std::hypot(dot_wx(d) - x, dot_wy(d) - y);
        if (dist < best_d) {
            best_d = dist;
            best = d;
        }
    }
    if (!best) {
        s.reason = {"SnapRepair", "a repair point stands on a straight piece of track - none near"};
        return s;
    }
    // it faces the way of the link with the smaller direction (north on a vertical leg, as in l05)
    float ax, ay, bx, by;
    link_vector(best, 0, ax, ay);
    link_vector(best, 1, bx, by);
    const int da = direction_of(ax, ay), db = direction_of(bx, by);
    s.ok = true;
    s.x = dot_wx(best);
    s.y = dot_wy(best);
    s.direction = round32(da <= db ? da : db);
    return s;
}

// A post beside the nearest track, on the side of the cursor, facing along the track the way
// nearest to `wanted`.
Snap snap_post(float x, float y, int wanted) {
    Snap s;
    uint8_t* d = point_near(x, y, 1, 100);
    if (!d) {
        s.reason = {"SnapSemaphore", "a semaphore stands beside the track - no track near"};
        return s;
    }
    float tx, ty;
    link_vector(d, 0, tx, ty);
    const float side = tx * (y - dot_wy(d)) - ty * (x - dot_wx(d)) >= 0 ? 1.0f : -1.0f;
    s.ok = true;
    s.x = dot_wx(d) - ty * side * POST_SIDE;
    s.y = dot_wy(d) + tx * side * POST_SIDE;
    const int along = round32(direction_of(tx, ty)), back = (along + 128) & 255;
    auto gap = [](int a, int b) { return (std::min)((a - b) & 255, (b - a) & 255); };
    s.direction = gap(along, wanted) <= gap(back, wanted) ? along : back;
    return s;
}

Snap snap_crate(float x, float y) {
    Snap s;
    uint8_t* d = point_near(x, y, 1, 80);
    if (!d) {
        s.reason = {"SnapCrate", "a money crate lies on the track - no track near"};
        return s;
    }
    s.ok = true;
    s.x = dot_wx(d);
    s.y = dot_wy(d);
    return s;
}

bool on_track_vid(int nvid) {
    return nvid == VID_DEPOT || nvid == VID_WORKSHOP || nvid == VID_SEMAPHORE || nvid == VID_CRATE;
}

// Where a VID of these goes for the cursor's map point; the others stay where they are.
bool snap(int nvid, float& x, float& y, int& direction, UiText& reason) {
    Snap s;
    if (nvid == VID_DEPOT) s = snap_factory(x, y);
    else if (nvid == VID_WORKSHOP) s = snap_repair(x, y);
    else if (nvid == VID_SEMAPHORE) s = snap_post(x, y, direction);
    else if (nvid == VID_CRATE) s = snap_crate(x, y);
    else return true;
    if (!s.ok) {
        reason = s.reason;
        return false;
    }
    x = s.x;
    y = s.y;
    if (nvid != VID_CRATE) direction = s.direction;
    return true;
}

// The push line a factory or a repair point standing at its place gets.
bool building_line(uint8_t* o, PushLine& line) {
    const float x = at<float>(o, O_X), y = at<float>(o, O_Y);
    const float a = at<int>(o, O_DIRECTION) * 6.2831853f / 256;
    const float vx = std::sin(a), vy = -std::cos(a);   // the way it faces
    if (nvid_of(o) == VID_DEPOT) {
        uint8_t* end = point_near(x - vx * FACTORY_INSIDE, y - vy * FACTORY_INSIDE, 1, 40);
        if (!end || dot_links(end) != 1) return false;
        uint8_t* out = walk(end, vx, vy, FACTORY_LINE);
        if (out == end) return false;
        const POINT b = dot_screen(end), e = dot_screen(out);
        line = {b.x, b.y, e.x, e.y, 1};
        return true;
    }
    if (nvid_of(o) == VID_WORKSHOP) {
        uint8_t* mid = point_near(x, y, 2, 40);
        if (!mid) return false;
        uint8_t* from = walk(mid, -vx, -vy, REPAIR_LINE_HALF);
        uint8_t* to = walk(mid, vx, vy, REPAIR_LINE_HALF);
        if (from == to) return false;
        const POINT b = dot_screen(from), e = dot_screen(to);
        line = {b.x, b.y, e.x, e.y, 1};
        return true;
    }
    return false;
}

// The editable lines of a factory or repair point: those whose way passes its middle (a factory's
// begins at the dead end behind it).
std::vector<int> lines_of(uint8_t* o) {
    std::vector<int> out;
    if (nvid_of(o) != VID_DEPOT && nvid_of(o) != VID_WORKSHOP) return out;
    const std::vector<PushLine>& lines = mission_push_lines();
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const Trace t = trace(lines[i]);
        for (uint8_t* d : t.path)
            if (std::hypot(dot_wx(d) - at<float>(o, O_X), dot_wy(d) - at<float>(o, O_Y)) < 30) {
                out.push_back(i);
                break;
            }
    }
    return out;
}

// Takes a building's lines away and their posts into `posts` (to delete); true when lines went.
bool take_building_lines(uint8_t* o, std::vector<uint8_t*>& posts) {
    std::vector<int> own = lines_of(o);
    if (own.empty()) return false;
    std::vector<PushLine>& lines = mission_push_lines();
    for (int i : own)
        for (const Post& p : line_posts(trace(lines[i])))
            if (uint8_t* post = post_near(p.x, p.y, posts)) posts.push_back(post);
    for (auto it = own.rbegin(); it != own.rend(); ++it) lines.erase(lines.begin() + *it);
    g_push_selected = -1;
    return true;
}

// L / Shift+L on a selected post: a push line at it - L lets the trains it faces in (a line from
// 140 px out toward the post, push_flag 1: the post green), Shift+L keeps them out (a line from the
// post away from them, push_flag 0, one way: the post red).
void post_line(bool against) {
    if (g_selection.empty() || nvid_of(g_selection[0]) != VID_SEMAPHORE) {
        MESSAGE("MsgLSelect", "L: select a semaphore first");
        return;
    }
    if (!push_lines_editable()) return;
    uint8_t* o = g_selection[0];
    uint8_t* d = point_near(at<float>(o, O_X), at<float>(o, O_Y), 1, 70);
    if (!d) {
        MESSAGE("MsgLNoTrack", "L: the semaphore has no track beside it");
        return;
    }
    const float a = at<int>(o, O_DIRECTION) * 6.2831853f / 256;
    const float fx = std::sin(a), fy = -std::cos(a);   // where its trains come from
    uint8_t* out = walk(d, fx, fy, FACTORY_LINE);
    if (out == d) {
        MESSAGE("MsgLNoWay", "L: no track toward the trains the semaphore faces");
        return;
    }
    const POINT p = dot_screen(d), q = dot_screen(out);
    const PushLine line = against ? PushLine{p.x, p.y, q.x, q.y, 0} : PushLine{q.x, q.y, p.x, p.y, 1};
    const std::vector<PushLine> lines_before = mission_push_lines();
    const std::vector<State> before = states({o});
    mission_push_lines().push_back(line);
    if (red(o) != against) set_red(o, against);
    commit({"OpSemaphoreLine", "semaphore line"}, {o}, before);
    Operation& op = g_undo.back();
    op.lines = true;
    op.lines_before = lines_before;
    op.lines_after = mission_push_lines();
    MESSAGE("MsgLLine", "push line %d,%d -> %d,%d flag %d at the semaphore, which is %s", line.bx, line.by, line.ex, line.ey,
            line.flag, against ? ui_text("WordRed", "red") : ui_text("WordGreen", "green"));
}

// After a factory or repair point is placed or moved (the last step of the history is its change):
// its line and posts, into that step.
void give_building_line(uint8_t* o) {
    PushLine line;
    if (!building_line(o, line)) return;
    const Trace t = trace(line);
    if (t.error) return;
    Operation& op = g_undo.back();
    if (!op.lines) {
        op.lines = true;
        op.lines_before = mission_push_lines();
    }
    mission_push_lines().push_back(line);
    op.lines_after = mission_push_lines();
    std::vector<uint8_t*> objects;
    std::vector<State> before;
    put_posts(line_posts(t), objects, before);
    for (size_t i = 0; i < objects.size(); ++i) op.changes.push_back({objects[i], before[i], state_of(objects[i])});
    MESSAGE("MsgBuildingLine", "%s at %.0f,%.0f direction %d with its push line %d,%d -> %d,%d and %u semaphores",
            nvid_of(o) == VID_DEPOT ? ui_text("WordFactory", "factory") : ui_text("WordRepairPoint", "repair point"),
            at<float>(o, O_X), at<float>(o, O_Y),
            at<int>(o, O_DIRECTION), line.bx, line.by, line.ex, line.ey, static_cast<unsigned>(objects.size()));
}
// ---- drawing ----

// Graphics_DrawVid draws at (x, y - z) of the view with the depth z.
void draw_vid(uint8_t* v, int frame, float x, float y, float z) {
    if (v) reinterpret_cast<DrawVid_t>(DRAW_VID)(graphics(), nullptr, v, frame, x, y, z);
}

// In front of the scene, as Unit_Draw draws the health bars (depth about 1900).
constexpr float FRONT = 1900;
void draw_front(uint8_t* v, int frame, float x, float y) { draw_vid(v, frame, x, y + FRONT, FRONT); }

// An object's look at a direction: animation 0 (ANI_STAND), the frame of that direction.
// The frame index as the engine computes it (Vid.dirHalfStep, 128 / nDir for most VIDs, 170
// for the rails: Rail_Action).
int frame_for(uint8_t* v, int direction) {
    const int dirs = (std::max)(1, at<int>(v, V_ND_DIR));
    const int index = ((direction + at<int>(v, V_DIR_HALF_STEP)) & 255) * dirs / 256;
    return index * (std::max)(1, at<int>(v, V_ANIM_FRAMES));
}

// A small square at a map point at a height (the rail ends of the track preview).
void mark(float x, float y, float z, uint32_t argb) {
    uint8_t* w = world();
    const float sx = x - at<float>(w, W_CAMERA_X), sy = y - z - at<float>(w, W_CAMERA_Y);
    reinterpret_cast<FillRect_t>(FILL_RECT)(graphics(), nullptr, sx - 4, sy - 4, sx + 4, sy + 4, argb);
}

std::string g_track_status;   // the status line's part for the Track tool

void draw_track() {
    uint8_t* w = world();
    const float cam_x = at<float>(w, W_CAMERA_X), cam_y = at<float>(w, W_CAMERA_Y);
    const std::vector<RailObject> rails = map_rails();
    char text[200];
    if (g_line.empty()) {
        const rail::Point p = line_start(rails);
        int level = 0;
        uint8_t* r = rail_ending_at(rails, p, &level);
        int cx = (std::max)(0, (std::min)(grid_width() - 1, static_cast<int>(p.x) / 8));
        int cy = (std::max)(0, (std::min)(grid_height() - 1, static_cast<int>(p.y) / 8));
        const float z = (r ? level : ground_cell(cx, cy)) - 4.0f;
        mark(static_cast<float>(p.x), static_cast<float>(p.y), z, 0xc0ffff00);
        std::snprintf(text, sizeof text, ui_format("StatusTrackStart", "Track: click a start point (%.1f,%.1f%s)"), p.x, p.y,
                      r ? ui_text("StatusRailEnd", ", the end of a rail") : "");
        g_track_status = text;
        return;
    }
    const TrackStep s = next_step(rails);
    const int nvid = RAIL_KINDS[g_rail_kind];
    const float z = line_rail_z(nvid) - (g_ramp < 0 ? rail::RAMP : 0);
    if (s.valid) {
        uint8_t* v = vid_by_number(nvid);
        if (v)
            draw_vid(v, frame_for(v, step_direction(s.segment, g_line.back(), g_ramp)), static_cast<float>(s.segment.x) - cam_x,
                     static_cast<float>(s.segment.y) - cam_y, z);
    }
    const uint32_t colour = s.valid ? 0xc000ff00 : 0xc0ff2020;
    mark(static_cast<float>(g_line.back().x), static_cast<float>(g_line.back().y), z, colour);
    mark(static_cast<float>(s.to.x), static_cast<float>(s.to.y), z, colour);
    std::snprintf(text, sizeof text, ui_format("StatusTrack", "Track: %u rails - %.1f,%.1f -> %.1f,%.1f %s"),
                  static_cast<unsigned>(g_line.size() - 1), g_line.back().x, g_line.back().y, s.to.x, s.to.y,
                  s.valid ? ui_text("WordOk", "ok") : s.reason.get());
    g_track_status = text;
}

// The push lines over the track: small squares along each line's way (green for push_flag 1,
// yellow for 0, grey for the script's own lines the editor does not write), a bigger one at its
// begin and a ringed one at its end; the selected line in white, a line without a way red at its
// two ends. With the tool, the rail point under the cursor in cyan with its screen coordinates.
std::string g_push_status;

void draw_push_lines(bool tool) {
    auto dot_mark = [](uint8_t* d, float size, uint32_t argb) {
        uint8_t* w = world();
        const float sx = at<int>(d, DOT_X) - at<float>(w, W_CAMERA_X), sy = screen_y(d) - at<float>(w, W_CAMERA_Y);
        reinterpret_cast<FillRect_t>(FILL_RECT)(graphics(), nullptr, sx - size, sy - size, sx + size, sy + size, argb);
    };
    auto draw = [&](const PushLine& l, uint32_t colour) {
        const Trace t = trace(l);
        if (t.error || t.path.empty()) {
            uint8_t* w = world();
            for (const POINT p : {POINT{l.bx, l.by}, POINT{l.ex, l.ey}}) {
                const float sx = p.x - at<float>(w, W_CAMERA_X), sy = p.y - at<float>(w, W_CAMERA_Y);
                reinterpret_cast<FillRect_t>(FILL_RECT)(graphics(), nullptr, sx - 6, sy - 6, sx + 6, sy + 6, 0xe0ff2020);
            }
            return;
        }
        for (uint8_t* d : t.path) dot_mark(d, 3, colour);
        dot_mark(t.path.front(), 6, colour);
        dot_mark(t.path.back(), 8, colour);
        dot_mark(t.path.back(), 4, 0xe0000000);
    };
    for (const PushLine& l : mission_script_push_lines()) draw(l, 0xd0b0b0ff);
    const std::vector<PushLine>& lines = mission_push_lines();
    for (int i = 0; i < static_cast<int>(lines.size()); ++i)
        draw(lines[i], i == g_push_selected && tool ? 0xf0ffffff : lines[i].flag ? 0xd000ff00 : 0xd0ffff00);
    if (!tool) return;
    uint8_t* w = world();
    float fx, fy;
    mouse_on_map(fx, fy);
    char text[160];
    if (uint8_t* d = nearest_dot(static_cast<int>(fx), static_cast<int>(fy))) {
        dot_mark(d, 5, 0xe000ffff);
        const POINT p = dot_screen(d);
        std::snprintf(text, sizeof text, ui_format("StatusPush", "Push line: rail point %d,%d%s"), p.x, p.y,
                      g_push_started ? ui_text("StatusPushEnd", " - click the end of the line")
                                     : ui_text("StatusPushBegin", " - click its begin, or an end of a line to select it"));
    } else {
        std::snprintf(text, sizeof text, "%s", ui_text("StatusPushNoTrack", "Push line: no track on the map"));
    }
    if (g_push_started) {
        const float sx = g_push_begin.x - at<float>(w, W_CAMERA_X), sy = g_push_begin.y - at<float>(w, W_CAMERA_Y);
        reinterpret_cast<FillRect_t>(FILL_RECT)(graphics(), nullptr, sx - 6, sy - 6, sx + 6, sy + 6, 0xe000ffff);
    }
    g_push_status = text;
}

// The Ground height tool: the cells around the cursor as squares at their height, coloured by it
// (blue low, green 60-80, red high; the track's cells grey), those of the brush brighter. The
// Decals tool: the circle it scatters in, and the decal to place at the cursor.
std::string g_ground_status;

void draw_ground_tool(bool heights) {
    uint8_t* w = world();
    const float cam_x = at<float>(w, W_CAMERA_X), cam_y = at<float>(w, W_CAMERA_Y);
    const auto fill = reinterpret_cast<FillRect_t>(FILL_RECT);
    float x, y;
    const rail::Cell c = cursor_cell(x, y);
    char text[160];
    if (heights) {
        const std::set<rail::Cell> track = g_stroke.active ? g_stroke.track : track_cells();
        const int reach = g_radius + 3;
        for (int dy = -reach; dy <= reach; ++dy)
            for (int dx = -reach; dx <= reach; ++dx) {
                const int cx = c.first + dx, cy = c.second + dy;
                if (cx < 0 || cx >= grid_width() || cy < 0 || cy >= grid_height()) continue;
                const int z = ground_cell(cx, cy);
                const bool in = dx * dx + dy * dy <= g_radius * g_radius;
                uint32_t rgb;
                if (track.count({cx, cy})) {
                    rgb = 0x808080;
                } else {
                    const int r = (std::max)(0, (std::min)(255, (z - 75) * 6)), b = (std::max)(0, (std::min)(255, (65 - z) * 6));
                    const int g = (std::max)(60, 255 - (std::max)(r, b));
                    rgb = (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
                }
                const float sx = cx * 8 + 4 - cam_x, sy = cy * 8 + 4 - z - cam_y, half = in ? 3.0f : 2.0f;
                fill(graphics(), nullptr, sx - half, sy - half, sx + half, sy + half, (in ? 0xe0000000u : 0x70000000u) | rgb);
            }
        const bool inside = c.first >= 0 && c.first < grid_width() && c.second >= 0 && c.second < grid_height();
        std::snprintf(text, sizeof text, ui_format("StatusHeight", "Ground height: %s (M), radius %d cells (+ -), the ground here %d"),
                      brush_name(), g_radius, inside ? ground_cell(c.first, c.second) : 0);
    } else {
        const float r = (std::max)(8.0f * g_radius, 12.0f);
        for (int k = 0; k < 24; ++k) {
            const float a = k * 6.2831853f / 24, px = x + r * std::cos(a), py = y + r * std::sin(a);
            const float sz = ground_z(vid_by_number(VID_SEMAPHORE), px, py);
            fill(graphics(), nullptr, px - cam_x - 1.5f, py - sz - cam_y - 1.5f, px - cam_x + 1.5f, py - sz - cam_y + 1.5f,
                 shift_down() ? 0xe0ff4040 : 0xe0ffffff);
        }
        if (uint8_t* v = vid_by_number(decal_nvid()))
            if (!shift_down()) draw_vid(v, frame_for(v, 0), x - cam_x, y - cam_y, ground_z(v, x, y));
        const char* name = object_name(decal_nvid());
        std::snprintf(text, sizeof text,
                      ui_format("StatusDecals", "Decals: %s, VID %d ([ ]), radius %d (+ -), spacing %d px (< >) - Shift takes away"),
                      name ? name : "?", decal_nvid(), g_radius, g_spacing);
    }
    g_ground_status = text;
}

// The Paint and Water tools: the brush's circle on the screen (the picture is the screen), the
// texture's colour in a square beside it; the status line.
void draw_paint_tool() {
    const auto fill = reinterpret_cast<FillRect_t>(FILL_RECT);
    const float mx = static_cast<float>(g_mouse.x), my = static_cast<float>(g_mouse.y), r = static_cast<float>(g_paint_radius);
    const bool erase = shift_down();
    for (int k = 0; k < 32; ++k) {
        const float a = k * 6.2831853f / 32;
        fill(graphics(), nullptr, mx + r * std::cos(a) - 1.5f, my + r * std::sin(a) - 1.5f, mx + r * std::cos(a) + 1.5f,
             my + r * std::sin(a) + 1.5f, erase ? 0xe0ff4040 : 0xe0ffffff);
    }
    char text[200];
    if (entry().tool == Tool::Paint) {
        const std::vector<GroundTexture>& t = ground_textures();
        if (g_texture < static_cast<int>(t.size())) {
            const uint16_t c = t[g_texture].mean;
            const uint32_t rgb = ((c >> 11) << 19) | (((c >> 5) & 63) << 10) | ((c & 31) << 3);
            fill(graphics(), nullptr, mx + r + 4, my - 8, mx + r + 20, my + 8, 0xff000000);
            fill(graphics(), nullptr, mx + r + 5, my - 7, mx + r + 19, my + 7, 0xff000000 | rgb);
        }
        std::snprintf(text, sizeof text,
                      ui_format("StatusPaint", "Paint: %s ([ ]), brush %d px (+ -) - Shift puts the original picture back"),
                      ground_texture_name(g_texture), g_paint_radius);
    } else {
        const WaterKind& k = WATER_KINDS[g_water_kind];
        const int level = water_level_of(k.nvid);
        std::snprintf(text, sizeof text, ui_format("StatusWater", "Water: %s (W), level %s, brush %d px (+ -) - Shift takes it away"),
                      ui_text(k.key, k.english), level >= 0 ? std::to_string(level).c_str() : ui_text("WordNew", "new"), g_paint_radius);
    }
    g_ground_status = text;
}

void circle(uint8_t* o, int frame) {
    uint8_t* w = world();
    draw_vid(vid_by_number(VID_CIRCLE), frame, at<float>(o, O_X) - at<float>(w, W_CAMERA_X),
             at<float>(o, O_Y) - at<float>(w, W_CAMERA_Y), at<float>(o, O_Z));
}

void set_cursor(int type) {
    if (void* c = *reinterpret_cast<void**>(G_CURSOR)) reinterpret_cast<SetInt_t>(CURSOR_SET)(c, nullptr, type);
}

// ---- the bar, in the state bar's look ----

// A picture of the bar at a screen point, in front of the scene; each part a step further in
// front than the one under it (Graphics_DrawVid: the depth is z).
// The VID may be hidden (VID flag 0x400: Player_HideStateBar hides the side tiles with the bar,
// status_bar.cpp), which the drawing skips, so it is shown for the call.
void draw_screen(int nvid, int frame, float x, float y, int step) {
    uint8_t* v = vid_by_number(nvid);
    if (!v) return;
    const float z = FRONT + static_cast<float>(step);
    const uint32_t flags = at<uint32_t>(v, V_FLAGS);
    at<uint32_t>(v, V_FLAGS) = flags & ~VID_HIDDEN;
    draw_vid(v, frame, x, y + z, z);
    at<uint32_t>(v, V_FLAGS) = flags;
}

// A text in a font VID, the way Text_Draw writes one: a frame per character (its code), sizeX
// apart, the first character's middle half a character from x; right-aligned with `right`.
float draw_text(int font, float x, float y, const char* text, bool right = false, int step = 5) {
    uint8_t* v = vid_by_number(font);
    if (!v) return x;
    const float w = at<float>(v, V_SIZE_X), h = at<float>(v, V_SIZE_Y);
    const float frames = static_cast<float>(at<short>(v, V_FRAME_COUNT));
    if (right) x -= w * static_cast<float>(std::strlen(text));
    for (const unsigned char* c = reinterpret_cast<const unsigned char*>(text); *c; ++c, x += w)
        if (*c >= 0x20 && *c < frames) draw_screen(font, *c, x + w / 2, y + h / 2, step);
    return x;
}

constexpr float LINE_HEIGHT = 14;

// A line of the status in the mouse tips' font, which has its own box behind every
// character - the look of the game's tips.
void status_line(float x, float y, const char* text) {
    if (vid_by_number(FONT_TIPS)) draw_text(FONT_TIPS, x + 2, y + 1, (std::string(" ") + text + " ").c_str());
    else reinterpret_cast<Printf_t>(PRINTF)(graphics(), x + 6, y + 2, "%s", text);
}

void draw_bar() {
    const Box v = view(), b = bar();
    const float y = (b.t + b.b) / 2;
    // the base, and its side tiles out to the edges of the view (status_bar.cpp lines them up
    // the same way): the ones next to the base (frames 1 and 2), then the repeated ones (0, 3)
    draw_screen(VID_BAR, 0, (b.l + b.r) / 2, y, 0);
    if (vid_by_number(VID_BAR_SIDE)) {
        for (float x = b.l - SIDE_WIDTH / 2, k = 0; x + SIDE_WIDTH / 2 > v.l; x -= SIDE_WIDTH, ++k)
            draw_screen(VID_BAR_SIDE, k == 0 ? 1 : 0, x, y, 0);
        for (float x = b.r + SIDE_WIDTH / 2, k = 0; x - SIDE_WIDTH / 2 < v.r; x += SIDE_WIDTH, ++k)
            draw_screen(VID_BAR_SIDE, k == 0 ? 2 : 3, x, y, 0);
    }
    // the strip of the entries past the slots, starting over the first slot
    float sx, sy;
    entry_cell(SLOTS, sx, sy);
    draw_screen(VID_STRIP, 0, sx - SLOT / 2 + STRIP_WIDTH / 2, sy, 1);
    // the icons, their keys, the chosen one framed
    for (int i = 0; i < ENTRY_COUNT; ++i) {
        const Entry& e = ENTRIES[i];
        float x, ey;
        entry_cell(i, x, ey);
        const int decal_icon = i == DECAL_ENTRY ? palette_icon(decal_nvid()) : -1;   // the chosen decal's picture
        if (decal_icon >= 0 && vid_by_number(VID_PALETTE_ICONS)) {
            draw_screen(VID_PALETTE_ICONS, decal_icon, x, ey, 2);
        } else if (vid_by_number(e.atlas)) {
            draw_screen(e.atlas, e.icon, x, ey, 2);
        } else {   // objects.res without the editor's icons: the name
            char label[5];
            std::snprintf(label, sizeof label, "%.4s", label_of(e));
            draw_text(FONT_TIPS, x - 18, ey - 6, label, false, 3);
        }
        draw_text(FONT_KEYS, x + 18, ey + 9, e.key, true, 4);
    }
    float cx, cy;
    entry_cell(g_entry, cx, cy);
    draw_screen(VID_SLOT_FRAME, 0, cx, cy, 3);
}

// ---- the context panel ----
//
// A right click on an object with the Select tool selects it and opens its panel, drawn in the
// game's style: rows of the depot panel's strip (VID 497) beside the object, the menu font (VID 2,
// the lit one VID 7 for what is set and what is under the mouse), with the object's name (the
// game's [Units] name, else the editor's) and per row a label and its choices - the army (the
// script's order: ours, enemy, neutral, pirates), turning, a semaphore's colour and line, a host's
// sockets (empty / tower with a cannon / missile tower; the citadel's pirate tower) and delete. A
// click on a choice does it (one step of the history, as the keys A, R, E, T, L, Del do), a click
// elsewhere, the right button or Esc close the panel.



struct Choice {
    UiText text;
    bool current = false;
    std::function<void()> act;
};

struct ContextRow {
    UiText label;
    std::vector<Choice> choices;
};

uint8_t* g_context = nullptr;   // the object of the open panel
struct Hit {
    float l, t, r, b;
    int row, choice;
};
std::vector<Hit> g_context_hits;   // the choices where the last frame drew them
constexpr int CONTEXT_FONT = 2, CONTEXT_LIT = 7, CONTEXT_STEP = 6;
constexpr float CONTEXT_ROW = 41, CONTEXT_WIDTH = 409, CONTEXT_LABEL = 15;   // the label's characters

// The gun `nvid` (0: none) into socket `i` of the host: the old one goes, the new one comes.
void set_socket(uint8_t* host, size_t i, int nvid) {
    const std::vector<Socket> sockets = sockets_of(nvid_of(host), at<int>(host, O_DIRECTION));
    const std::vector<uint8_t*> guns = guns_on(host);
    if (i >= sockets.size()) return;
    std::vector<uint8_t*> objects;
    std::vector<State> before;
    if (guns[i]) {
        if (nvid_of(guns[i]) == nvid) return;
        objects.push_back(guns[i]);
        before.push_back(state_of(guns[i]));
        State gone = before.back();
        gone.present = false;
        apply(guns[i], gone);
    }
    if (nvid)
        if (uint8_t* g = fit_gun(host, sockets[i], nvid)) {
            objects.push_back(g);
            State none = state_of(g);
            none.present = false;
            before.push_back(none);
        }
    if (objects.empty()) return;
    commit({"OpSocket", "socket"}, objects, before);
    g_selection = group_of(host);
    MESSAGE("MsgSocket", "socket %d of VID %d: %s", static_cast<int>(i + 1), nvid_of(host),
            nvid ? (ui_unit_name(nvid) ? ui_unit_name(nvid) : std::to_string(nvid).c_str()) : ui_text("WordEmpty", "empty"));
}

std::vector<ContextRow> context_rows(uint8_t* o) {
    std::vector<ContextRow> rows;
    const int nvid = nvid_of(o);
    if (class_of(o) != B_RAIL) {
        ContextRow army{{"CtxArmy", "ARMY:"}, {}};
        for (int a = 0; a < 4; ++a)
            army.choices.push_back({{ARMY_CHOICE_KEYS[a], ARMY_CHOICES[a]}, army_of(o) == a, [a] { set_army(a); }});
        rows.push_back(army);
    }
    if (is_region(o)) {   // its size, splashes and fog
        rows.clear();
        rows.push_back({{"CtxSize", "SIZE:"},
                        {{{"CtxWider", "WIDER"}, false, [o] { region_change(o, [o] { at<float>(o, RG_WIDTH) += 32; }); }},
                         {{"CtxNarrower", "NARROWER"}, false, [o] {
                              region_change(o, [o] { at<float>(o, RG_WIDTH) = (std::max)(16.0f, at<float>(o, RG_WIDTH) - 32); });
                          }},
                         {{"CtxTaller", "TALLER"}, false, [o] { region_change(o, [o] { at<float>(o, RG_HEIGHT) += 32; }); }},
                         {{"CtxLower", "LOWER"}, false, [o] {
                              region_change(o, [o] { at<float>(o, RG_HEIGHT) = (std::max)(16.0f, at<float>(o, RG_HEIGHT) - 32); });
                          }}}});
        ContextRow splash{{"CtxSplash", "EXPLOSIONS:"}, {}};
        const UiText SPLASH_NAMES[] = {{"CtxSplashNone", "AS THEY ARE"}, {"CtxSplashWater", "ON WATER"}, {"CtxSplashLava", "ON LAVA"}};
        for (int s = 0; s < 3; ++s)
            splash.choices.push_back({SPLASH_NAMES[s], region_splash(o) == s, [o, s] { region_change(o, [o, s] { set_region_splash(o, s); }); }});
        rows.push_back(splash);
        ContextRow fog{{"CtxFog", "FOG:"}, {}};
        const int level = static_cast<int>(std::lround(at<float>(o, O_Z)));
        for (const FogPreset& f : FOGS) {
            const int low = f.above ? level - f.below : 0, high = f.above ? level + f.above : 0;
            const bool current = !f.above ? at<int>(o, RG_HIGH) <= at<int>(o, RG_LOW)
                                          : at<int>(o, RG_HIGH) == high && at<int>(o, RG_LOW) == low && at<uint32_t>(o, RG_COLOUR) == f.colour;
            const uint32_t colour = f.colour;
            fog.choices.push_back({f.name, current, [o, low, high, colour] {
                                       region_change(o, [o, low, high, colour] { set_region_fog(o, low, high, colour); });
                                   }});
        }
        rows.push_back(fog);
        const uint32_t flags = at<uint32_t>(o, RG_FLAGS);
        rows.push_back({{"CtxFogOver", "FOG OVER:"},
                        {{{"CtxFogRegion", "THE REGION"}, !(flags & 8), [o] { region_change(o, [o] { at<uint32_t>(o, RG_FLAGS) &= ~8u; }); }},
                         {{"CtxFogView", "THE VIEW"}, (flags & 8) != 0, [o] { region_change(o, [o] { at<uint32_t>(o, RG_FLAGS) |= 8u; }); }},
                         {{"CtxPulse", "PULSING"}, (flags & 1) != 0, [o] { region_change(o, [o] { at<uint32_t>(o, RG_FLAGS) ^= 1u; }); }}}});
        rows.push_back({{"CtxDelete", "DELETE (DEL):"},
                        {{{"CtxDeleteIt", "DELETE"}, false, [o] {
                              delete_objects({o});
                              g_context = nullptr;
                          }}}});
        return rows;
    }
    if (is_car(o)) {   // its train
        ContextRow train{{"CtxTrain", "TRAIN:"}, {{{"CtxReverse", "TURN ROUND (R)"}, false, [o] { reverse_train(o); }}}};
        if (car_prev(o)) train.choices.push_back({{"CtxUncouple", "UNCOUPLE (U)"}, false, [o] { uncouple(o); }});
        if (coupling_partner(o)) train.choices.push_back({{"CtxCouple", "COUPLE (C)"}, false, [o] { couple(o); }});
        rows.push_back(train);
        int px = 0, py = 0;
        const bool patrol = train_patrol(o, px, py);
        rows.push_back({{"CtxPatrol", "PATROL:"},
                        {{{"CtxPatrolNone", "NONE"}, !patrol && g_patrol_train != o, [o] {
                              g_patrol_train = nullptr;
                              set_train_patrol(o, false, 0, 0);
                          }},
                         {{"CtxPatrolTo", "TO A POINT..."}, patrol || g_patrol_train == o, [o] {
                              g_patrol_train = o;
                              g_context = nullptr;
                              MESSAGE("MsgPatrolPick", "patrol: click the point of the track the train goes to and fro to");
                          }}}});
        rows.push_back({{"CtxDelete", "DELETE (DEL):"},
                        {{{"CtxDeleteCar", "CAR"}, false, [o] {
                              delete_objects({o});
                              g_context = nullptr;
                          }},
                         {{"CtxDeleteTrain", "TRAIN (SHIFT+DEL)"}, false, [o] {
                              delete_objects(group_of(o));
                              g_context = nullptr;
                          }}}});
        return rows;
    }
    if (movable(o) && at<int>(vid_of(o), V_ND_DIR) > 1)
        rows.push_back({{"CtxTurn", "TURN:"},
                        {{{"CtxTurnLeft", "LEFT (E)"}, false, [] { turn(-1); }},
                         {{"CtxTurnRight", "RIGHT (R)"}, false, [] { turn(1); }}}});
    if (nvid == VID_SEMAPHORE) {
        const bool is_red = red(o);
        rows.push_back({{"CtxColour", "COLOUR (T):"},
                        {{{"CtxGreen", "GREEN"}, !is_red, [o] { if (red(o)) toggle_semaphore(); }},
                         {{"CtxRed", "RED"}, is_red, [o] { if (!red(o)) toggle_semaphore(); }}}});
        rows.push_back({{"CtxLine", "LINE (L):"},
                        {{{"CtxLetIn", "LET IN"}, false, [] { post_line(false); }},
                         {{"CtxKeepOut", "KEEP OUT"}, false, [] { post_line(true); }}}});
    }
    const std::vector<Socket> sockets = sockets_of(nvid, at<int>(o, O_DIRECTION));
    const std::vector<uint8_t*> guns = guns_on(o);
    for (size_t i = 0; i < sockets.size(); ++i) {
        ContextRow row{{sockets.size() > 1 ? (i ? "CtxSocket2" : "CtxSocket1") : "CtxSocket",
                        sockets.size() > 1 ? (i ? "SOCKET 2:" : "SOCKET 1:") : "SOCKET:"},
                       {}};
        for (int gun : gun_choices(nvid)) {
            const UiText text = gun == 0 ? UiText{"CtxEmpty", "EMPTY"}
                                : gun == GUN_CANNON ? UiText{"CtxCannon", "CANNON"}
                                : gun == GUN_MISSILE ? UiText{"CtxMissile", "MISSILES"}
                                                     : UiText{"CtxPirateTower", "PIRATE TOWER"};
            const bool current = guns[i] ? nvid_of(guns[i]) == gun : gun == 0;
            row.choices.push_back({text, current, [o, i, gun] { set_socket(o, i, gun); }});
        }
        rows.push_back(row);
    }
    rows.push_back({{"CtxDelete", "DELETE (DEL):"},
                    {{{"CtxDeleteIt", "DELETE"}, false, [o] {
                          delete_objects(group_of(o));
                          g_context = nullptr;
                      }}}});
    return rows;
}

// Where the panel goes: beside the object, inside the view.
void context_box(uint8_t* o, int rows, float& x, float& y) {
    uint8_t* w = world();
    const Box v = view();
    const float sx = at<float>(o, O_X) - at<float>(w, W_CAMERA_X), sy = at<float>(o, O_Y) - at<float>(o, O_Z) - at<float>(w, W_CAMERA_Y);
    const float height = (rows + 1) * CONTEXT_ROW;
    x = sx + 50;
    if (x + CONTEXT_WIDTH > v.r) x = sx - 50 - CONTEXT_WIDTH;
    x = (std::max)(v.l, (std::min)(x, v.r - CONTEXT_WIDTH));
    y = (std::max)(v.t + 2 * LINE_HEIGHT, (std::min)(sy - height / 2, v.b - BASE_HEIGHT - SLOT - height));
}

void draw_context() {
    g_context_hits.clear();
    if (!g_context || removed(g_context)) {
        g_context = nullptr;
        return;
    }
    const std::vector<ContextRow> rows = context_rows(g_context);
    float x, y;
    context_box(g_context, static_cast<int>(rows.size()), x, y);
    float cw = 8, ch = 13;
    if (uint8_t* font = vid_by_number(CONTEXT_FONT)) {
        cw = at<float>(font, V_SIZE_X);
        ch = at<float>(font, V_SIZE_Y);
    }
    const int nvid = nvid_of(g_context);
    char title[128];
    const char* name = ui_unit_name(nvid) ? ui_unit_name(nvid) : object_name(nvid);
    std::snprintf(title, sizeof title, "%s (VID %d)", name ? name : "", nvid);
    for (size_t r = 0; r <= rows.size(); ++r) {   // the title's row, then the rows
        const float ry = y + r * CONTEXT_ROW;
        draw_screen(VID_STRIP, 0, x + CONTEXT_WIDTH / 2, ry + CONTEXT_ROW / 2, CONTEXT_STEP);
        const float ty = ry + (CONTEXT_ROW - ch) / 2;
        if (r == 0) {
            draw_text(CONTEXT_LIT, x + 10, ty, title, false, CONTEXT_STEP + 1);
            continue;
        }
        const ContextRow& row = rows[r - 1];
        draw_text(CONTEXT_FONT, x + 10, ty, row.label.get(), false, CONTEXT_STEP + 1);
        float cx = x + 10 + CONTEXT_LABEL * cw;
        for (size_t c = 0; c < row.choices.size(); ++c) {
            const char* text = row.choices[c].text.get();
            const float width = std::strlen(text) * cw;
            const bool hover = g_mouse.x >= cx - 3 && g_mouse.x < cx + width + 3 && g_mouse.y >= ry && g_mouse.y < ry + CONTEXT_ROW;
            draw_text(row.choices[c].current || hover ? CONTEXT_LIT : CONTEXT_FONT, cx, ty, text, false, CONTEXT_STEP + 1);
            if (row.choices[c].current) {   // what is set: underlined
                reinterpret_cast<FillRect_t>(FILL_RECT)(graphics(), nullptr, cx, ty + ch + 1, cx + width, ty + ch + 3, 0xe0f0e080);
            }
            g_context_hits.push_back({cx - 3, ry, cx + width + 3, ry + CONTEXT_ROW, static_cast<int>(r - 1), static_cast<int>(c)});
            cx += width + 2 * cw;
        }
    }
}

void close_context() { g_context = nullptr; }

void open_context(uint8_t* o) {
    g_context = o;
    if (o) {
        g_selection = group_of(o);
        patch::log("edit: context panel of VID %d", nvid_of(o));
    }
}

// A click while the panel is open: true when it was the panel's.
bool context_click(POINT m) {
    if (!g_context) return false;
    for (const Hit& h : g_context_hits)
        if (m.x >= h.l && m.x < h.r && m.y >= h.t && m.y < h.b) {
            const std::vector<ContextRow> rows = context_rows(g_context);
            if (h.row < static_cast<int>(rows.size()) && h.choice < static_cast<int>(rows[h.row].choices.size())) {
                patch::log("edit: context %s %s", rows[h.row].label.english, rows[h.row].choices[h.choice].text.english);
                g_selection = group_of(g_context);
                rows[h.row].choices[h.choice].act();
                if (g_context && !removed(g_context)) g_selection = group_of(g_context);
            }
            return true;
        }
    float x, y;
    context_box(g_context, static_cast<int>(context_rows(g_context).size()), x, y);
    const bool inside = m.x >= x && m.x < x + CONTEXT_WIDTH && m.y >= y && m.y < y + (context_rows(g_context).size() + 1) * CONTEXT_ROW;
    if (!inside) g_context = nullptr;   // a click elsewhere closes it
    return true;
}

void draw_overlay() {
    uint8_t* w = world();
    void* g = graphics();
    if (!w || !g) return;
    const float cam_x = at<float>(w, W_CAMERA_X), cam_y = at<float>(w, W_CAMERA_Y);
    const bool over_bar = on_bar(static_cast<float>(g_mouse.x), static_cast<float>(g_mouse.y));
    const float mx = cam_x + g_mouse.x, my = cam_y + g_mouse.y;

    for (uint8_t* o : g_selection) circle(o, 1);
    // a selected train's patrol: its far point, marked like the route markers' squares
    int px = 0, py = 0;
    if (!g_selection.empty() && is_car(g_selection[0]) && train_patrol(g_selection[0], px, py)) {
        const float sx = px - cam_x, sy = py - cam_y;
        reinterpret_cast<FillRect_t>(FILL_RECT)(g, nullptr, sx - 5, sy - 5, sx + 5, sy + 5, 0xe0f0d020);
        draw_text(FONT_TIPS, sx + 8, sy - 6, ui_text("WordPatrol", "patrol"), false, 3);
    }
    if (g_patrol_train && !removed(g_patrol_train)) {   // picking it: the rail point under the cursor
        const TrackSpot spot = track_spot(mx, my, CAR_REACH);
        if (spot.found && spot.dot) {
            const float sx = at<int>(spot.dot, DOT_X) - cam_x, sy = at<int>(spot.dot, DOT_Y) - at<int>(spot.dot, DOT_Z) - cam_y;
            reinterpret_cast<FillRect_t>(FILL_RECT)(g, nullptr, sx - 5, sy - 5, sx + 5, sy + 5, 0xe020f020);
        }
    }
    int cursor = CURSOR_ARROW;
    if (!over_bar && g_mouse.x >= 0 && !panel_open()) {
        if (entry().tool == Tool::Delete || entry().tool == Tool::Section) {
            if (uint8_t* o = pick(mx, my)) {
                const bool section = (shift_down() || entry().tool == Tool::Section) && class_of(o) == B_RAIL;
                for (uint8_t* d : section ? section_of(o) : is_car(o) && !shift_down() ? std::vector<uint8_t*>{o} : group_of(o))
                    circle(d, 0);
            }
            cursor = CURSOR_DELETE;
        } else if (entry().tool == Tool::Place && vid_by_number(place_nvid()) &&
                   at<int>(vid_by_number(place_nvid()), V_CLASS) == B_ENGINE) {   // a car: on the track
            uint8_t* v = vid_by_number(place_nvid());
            const CarPlan p = plan_car(g_mouse);
            g_place_status.clear();
            if (p.ok) {
                draw_vid(v, frame_for(v, p.direction), p.spot.x - cam_x, p.spot.y - cam_y, p.spot.z);
                if (p.couple_to) {
                    circle(p.couple_to, 1);
                    g_place_status = ui_printf(ui_format("StatusCarCouples", "%s: coupled to the train of VID %d"), place_label(),
                                               nvid_of(p.couple_to));
                }
            } else {
                g_place_status = std::string(place_label()) + ": " + p.why.get();
            }
        } else if (entry().tool == Tool::Place) {
            uint8_t* v = vid_by_number(place_nvid());
            g_place_status.clear();
            if (v) {
                float x, y, z;
                map_point(g_mouse, v, x, y, z);
                int direction = at<int>(v, V_ND_DIR) > 1 ? g_place_direction : 0;
                bool shown = true;
                if (on_track_vid(place_nvid())) {
                    UiText why;
                    shown = snap(place_nvid(), x, y, direction, why);
                    if (shown) z = ground_z(v, x, y);
                    else g_place_status = std::string(place_label()) + ": " + why.get();
                }
                if (shown) draw_vid(v, frame_for(v, direction), x - cam_x, y - cam_y, z);
                const std::vector<Socket> sockets = sockets_of(place_nvid(), direction);
                const std::vector<int> fit = default_fit(place_nvid());
                for (size_t i = 0; shown && i < sockets.size() && i < fit.size(); ++i)
                    if (uint8_t* gun = vid_by_number(fit[i]))
                        draw_vid(gun, frame_for(gun, direction), x + sockets[i].dx - cam_x, y + sockets[i].dy - cam_y,
                                 z + sockets[i].dz);
            }
        } else if (entry().tool == Tool::Track) {
            draw_track();
        } else if (entry().tool == Tool::PushLine) {
            // drawn below with the lines
        } else if (entry().tool == Tool::Height || entry().tool == Tool::Decals) {
            draw_ground_tool(entry().tool == Tool::Height);
        } else if (entry().tool == Tool::Paint || entry().tool == Tool::Water) {
            draw_paint_tool();
        } else if (g_drag.active && g_drag.moved) {
            cursor = CURSOR_MOVE;
        } else if (pick(mx, my)) {
            cursor = CURSOR_SELECT;
        }
    }
    set_cursor(cursor);
    draw_regions();
    draw_push_lines(entry().tool == Tool::PushLine);

    draw_bar();

    // the status line, in the mouse tips' font on a dark band: the tool (or the entry under
    // the cursor), the undo and redo counts, "not saved" and the last message
    const Box v = view();
    char status[400];
    char tool[96];
    const int hover = bar_entry_at(static_cast<float>(g_mouse.x), static_cast<float>(g_mouse.y));
    const Entry& shown = hover >= 0 ? ENTRIES[hover] : entry();
    const bool palette = is_palette(shown);
    const int shown_nvid = palette ? g_picked_nvid[palette_of(shown)] : shown.nvid != 0 ? shown.nvid : decal_nvid();
    if (shown.tool == Tool::Place && shown_nvid > 0)
        std::snprintf(tool, sizeof tool, ui_format("StatusToolVid", "%s: VID %d, direction %d"),
                      palette && object_name(shown_nvid) ? object_name(shown_nvid) : label_of(shown), shown_nvid, g_place_direction);
    else
        std::snprintf(tool, sizeof tool, "%s", label_of(shown));
    if (hover >= 0 && *shown.key) std::snprintf(tool + std::strlen(tool), sizeof tool - std::strlen(tool), " (%s)", shown.key);
    if (const char* tip = panel_tip(g_mouse)) std::snprintf(tool, sizeof tool, "%s", tip);   // the Help or Menu button
    std::snprintf(status, sizeof status, ui_format("StatusLine", "EDITOR - %s - undo %u, redo %u%s   %s"), tool,
                  static_cast<unsigned>(g_undo.size()), static_cast<unsigned>(g_redo.size()),
                  g_dirty ? ui_text("StatusNotSaved", " - not saved") : "",
                  GetTickCount() - g_message_time < 6000 ? g_message.c_str() : "");
    status_line(v.l, v.t, status);
    if (entry().tool == Tool::Track) status_line(v.l, v.t + LINE_HEIGHT, g_track_status.c_str());
    if (entry().tool == Tool::PushLine) status_line(v.l, v.t + LINE_HEIGHT, g_push_status.c_str());
    if (entry().tool == Tool::Height || entry().tool == Tool::Decals || entry().tool == Tool::Paint || entry().tool == Tool::Water)
        status_line(v.l, v.t + LINE_HEIGHT, g_ground_status.c_str());
    if (entry().tool == Tool::Region)
        status_line(v.l, v.t + LINE_HEIGHT,
                    ui_text("StatusRegion", "Region: drag its rectangle on the map; the right button on its frame opens its panel"));
    if (entry().tool == Tool::Place && !g_place_status.empty())
        status_line(v.l, v.t + LINE_HEIGHT, g_place_status.c_str());
    if (entry().tool == Tool::Track) {
        char bed[200];
        std::snprintf(bed, sizeof bed, ui_format("StatusRail", "rail (V): %s, next %s (PgUp / PgDn), gravel bed (B): %s"),
                      rail_kind_name(RAIL_KINDS[g_rail_kind]),
                      g_ramp > 0 ? ui_text("WordClimbs", "climbs") : g_ramp < 0 ? ui_text("WordDescends", "descends")
                                                                     : ui_text("WordFlat", "flat"),
                      swamp_rail(RAIL_KINDS[g_rail_kind]) ? "602" : g_bed ? std::to_string(g_bed).c_str() : ui_text("WordNone", "none"));
        status_line(v.l, v.t + 2 * LINE_HEIGHT, bed);
    }
    draw_context();
    panel_draw(g_mouse);   // the Help and Menu buttons, an open panel in front of everything
}

// The map against the void: the camera of a held level goes 300 px past the map (camera.cpp), where
// nothing is drawn. The outside gets a dim grid (32 px) and the map's edge a frame, so an empty map
// stands out; the track out there stays visible between the lines.
void draw_map_frame() {
    uint8_t* w = world();
    const Box v = view();
    const float cam_x = at<float>(w, W_CAMERA_X), cam_y = at<float>(w, W_CAMERA_Y);
    const float l = -cam_x, t = -cam_y, r = grid_width() * 8.0f - cam_x, b = grid_height() * 8.0f - cam_y;
    const auto fill = reinterpret_cast<FillRect_t>(FILL_RECT);
    auto outside = [&](float x0, float y0, float x1, float y1) {   // a part of the view off the map
        x0 = (std::max)(x0, v.l);
        y0 = (std::max)(y0, v.t);
        x1 = (std::min)(x1, v.r);
        y1 = (std::min)(y1, v.b);
        if (x0 >= x1 || y0 >= y1) return;
        for (float x = std::ceil((x0 + cam_x) / 32) * 32 - cam_x; x < x1; x += 32) fill(graphics(), nullptr, x, y0, x + 1, y1, 0x50607080);
        for (float y = std::ceil((y0 + cam_y) / 32) * 32 - cam_y; y < y1; y += 32) fill(graphics(), nullptr, x0, y, x1, y + 1, 0x50607080);
    };
    outside(v.l, v.t, v.r, t);   // above the map
    outside(v.l, b, v.r, v.b);   // below
    outside(v.l, t, l, b);       // left
    outside(r, t, v.r, b);       // right
    const uint32_t edge = 0xd0e0c060;
    fill(graphics(), nullptr, l - 2, t - 2, r + 2, t, edge);
    fill(graphics(), nullptr, l - 2, b, r + 2, b + 2, edge);
    fill(graphics(), nullptr, l - 2, t, l, b, edge);
    fill(graphics(), nullptr, r, t, r + 2, b, edge);
}

void __fastcall DrawScene_editor(void* g, void* edx, int draw) {
    const bool held = editor_level_held() && draw;
    g_draw_scene(g, edx, draw);
    if (held) {
        draw_map_frame();
        draw_overlay();
    }
}

}  // namespace

// ---- editor.cpp's side ----

// A message of the editor window while the level is held; true when it was handled here.
bool edit_message(HWND window, UINT m, WPARAM w, LPARAM l) {
    if (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST && m != WM_MOUSEWHEEL) g_mouse = logical(l);
    if (panel_message(window, m, w, g_mouse)) return true;   // the Help and Menu buttons, an open panel
    switch (m) {
    case WM_MOUSEMOVE:
        g_mouse = logical(l);
        if (g_drag.active) continue_drag(g_mouse);
        if (g_stroke.active && entry().tool == Tool::Height) height_step();
        if (g_stroke.active && entry().tool == Tool::Decals) decal_step(false);
        if (g_gstroke.active) ground_step(false);
        return false;   // the game scrolls by it
    case WM_LBUTTONDOWN: {
        g_mouse = logical(l);
        if (context_click(g_mouse)) return true;   // the open context panel
        const int i = bar_entry_at(static_cast<float>(g_mouse.x), static_cast<float>(g_mouse.y));
        if (i >= 0) {
            if (ENTRIES[i].tool == Tool::Settings) editor_mission_settings();
            else choose_entry(i);
            return true;
        }
        if (on_bar(static_cast<float>(g_mouse.x), static_cast<float>(g_mouse.y)))
            return true;   // the plates of the bar: nothing under them is picked or placed
        uint8_t* w0 = world();
        const float x = at<float>(w0, W_CAMERA_X) + g_mouse.x, y = at<float>(w0, W_CAMERA_Y) + g_mouse.y;
        if (entry().tool == Tool::Place) {
            place(g_mouse);
        } else if (entry().tool == Tool::Track) {
            if (g_line.empty()) start_line();
            else lay_step();
        } else if (entry().tool == Tool::Height) {
            start_height_stroke();
            height_step();
            if (!g_test) SetCapture(window);
        } else if (entry().tool == Tool::Decals) {
            g_stroke = Stroke();
            g_stroke.active = true;
            decal_step(true);
            if (!g_test) SetCapture(window);
        } else if (entry().tool == Tool::Paint || entry().tool == Tool::Water) {
            start_ground_stroke(entry().tool == Tool::Water);
            ground_step(true);
            if (!g_test) SetCapture(window);
        } else if (entry().tool == Tool::Region) {
            float sx, sy;
            mouse_on_map(sx, sy);
            g_region_start = {static_cast<LONG>(sx), static_cast<LONG>(sy)};
            g_region_drag = true;
            if (!g_test) SetCapture(window);
        } else if (entry().tool == Tool::PushLine) {
            float px, py;
            mouse_on_map(px, py);
            push_click(static_cast<int>(px), static_cast<int>(py));
        } else if (entry().tool == Tool::Delete || entry().tool == Tool::Section) {
            if (uint8_t* o = pick(x, y))
                delete_objects((shift_down() || entry().tool == Tool::Section) && class_of(o) == B_RAIL ? section_of(o)
                               : is_car(o) && !shift_down()                                         ? std::vector<uint8_t*>{o}
                                                                                                    : group_of(o));
        } else if (g_patrol_train && !removed(g_patrol_train)) {   // the patrol's point
            uint8_t* c = g_patrol_train;
            g_patrol_train = nullptr;
            set_train_patrol(c, true, x, y);
        } else {
            uint8_t* o = pick(x, y);
            select(o);
            if (o) {
                start_drag(g_mouse);
                if (g_drag.active && !g_test) SetCapture(window);
            }
        }
        return true;
    }
    case WM_LBUTTONUP:
        end_drag();
        if (entry().tool == Tool::Height) end_height_stroke();
        if (entry().tool == Tool::Decals) end_decal_stroke();
        end_ground_stroke();
        if (g_region_drag) {
            float sx, sy;
            mouse_on_map(sx, sy);
            finish_region(sx, sy);
        }
        if (!g_test && GetCapture()) ReleaseCapture();
        return true;
    case WM_RBUTTONDOWN:
        if (g_patrol_train) {   // no patrol point after all
            g_patrol_train = nullptr;
            return true;
        }
        if (entry().tool == Tool::Track && !g_line.empty()) {
            end_line();   // the right button ends the line first, then the tool
            return true;
        }
        if (entry().tool == Tool::PushLine && g_push_started) {
            g_push_started = false;   // the begin is dropped first
            return true;
        }
        if (entry().tool == Tool::Select && !on_bar(static_cast<float>(g_mouse.x), static_cast<float>(g_mouse.y))) {
            uint8_t* w0 = world();
            uint8_t* o = pick(at<float>(w0, W_CAMERA_X) + g_mouse.x, at<float>(w0, W_CAMERA_Y) + g_mouse.y);
            if (o && o != g_context) {   // its context panel (another right click closes it)
                open_context(o);
                return true;
            }
        }
        g_context = nullptr;
        choose_entry(0);
        select(nullptr);
        return true;
    case WM_RBUTTONUP:
        return true;
    case WM_MOUSEWHEEL:
        turn(GET_WHEEL_DELTA_WPARAM(w) > 0 ? 1 : -1);
        return true;
    case WM_KEYDOWN:
    case WM_KEYUP: {
        if (w == VK_CONTROL) g_ctrl = m == WM_KEYDOWN;
        if (w == VK_SHIFT) g_shift = m == WM_KEYDOWN;
        if (m == WM_KEYUP) return w != VK_CONTROL && w != VK_SHIFT && (w < VK_LEFT || w > VK_DOWN);
        const bool ctrl = g_ctrl || GetKeyState(VK_CONTROL) < 0;
        if (ctrl && w == 'Z') undo(false);
        else if (ctrl && w == 'Y') undo(true);
        else if (entry().tool == Tool::PushLine && (w == VK_DELETE || w == 'T' || w == 'R')) push_change(static_cast<int>(w));
        else if (entry().tool == Tool::PushLine && w == 'S') push_semaphores();
        else if (entry().tool == Tool::Height && w == 'M') {
            g_brush = static_cast<Brush>((static_cast<int>(g_brush) + 1) % 4);
            MESSAGE("MsgBrush", "ground: %s", brush_name());
        } else if ((entry().tool == Tool::Height || entry().tool == Tool::Decals) &&
                   (w == VK_OEM_PLUS || w == VK_ADD || w == VK_OEM_MINUS || w == VK_SUBTRACT)) {
            g_radius = (std::max)(0, (std::min)(MAX_RADIUS, g_radius + (w == VK_OEM_PLUS || w == VK_ADD ? 1 : -1)));
            MESSAGE("MsgRadius", "radius %d", g_radius);
        } else if ((entry().tool == Tool::Paint || entry().tool == Tool::Water) &&
                   (w == VK_OEM_PLUS || w == VK_ADD || w == VK_OEM_MINUS || w == VK_SUBTRACT)) {
            g_paint_radius = (std::max)(MIN_PAINT_RADIUS, (std::min)(MAX_PAINT_RADIUS, g_paint_radius + (w == VK_OEM_PLUS || w == VK_ADD ? PAINT_STEP : -PAINT_STEP)));
            MESSAGE("MsgBrushSize", "brush %d px", g_paint_radius);
        } else if (entry().tool == Tool::Paint && (w == VK_OEM_4 || w == VK_OEM_6)) {
            const int n = static_cast<int>(ground_textures().size());
            if (n) g_texture = (g_texture + (w == VK_OEM_6 ? 1 : n - 1)) % n;
            MESSAGE("MsgTexture", "texture: %s", ground_texture_name(g_texture));
        } else if (entry().tool == Tool::Water && w == 'W') {
            g_water_kind = (g_water_kind + 1) % WATER_KIND_COUNT;
            MESSAGE("MsgWaterKind", "water: %s (VID %d)", ui_text(WATER_KINDS[g_water_kind].key, WATER_KINDS[g_water_kind].english),
                    WATER_KINDS[g_water_kind].nvid);
        } else if (entry().tool == Tool::Decals && (w == VK_OEM_COMMA || w == VK_OEM_PERIOD)) {
            g_spacing = (std::max)(MIN_SPACING, (std::min)(MAX_SPACING, g_spacing + (w == VK_OEM_PERIOD ? SPACING_STEP : -SPACING_STEP)));
            MESSAGE("MsgSpacing", "spacing %d px", g_spacing);
        } else if (entry().tool == Tool::Track && w == 'V') {
            g_rail_kind = (g_rail_kind + 1) % 4;
            MESSAGE("MsgRailKind", "track: %s rails (VID %d)", rail_kind_name(RAIL_KINDS[g_rail_kind]), RAIL_KINDS[g_rail_kind]);
        } else if (entry().tool == Tool::Track && (w == VK_PRIOR || w == VK_NEXT)) {
            const int want = w == VK_PRIOR ? 1 : -1;
            g_ramp = g_ramp == want ? 0 : want;
            MESSAGE("MsgRamp", "track: the next rail %s", g_ramp > 0 ? ui_text("WordClimbs", "climbs")
                                                          : g_ramp < 0 ? ui_text("WordDescends", "descends")
                                                                       : ui_text("WordFlat", "flat"));
        } else if (entry().tool == Tool::Track && w == 'B') {
            const int* at_bed = std::find(std::begin(BEDS), std::end(BEDS), g_bed);
            g_bed = g_bed == 0 ? BEDS[0] : (at_bed + 1 == std::end(BEDS) ? 0 : *(at_bed + 1));
            MESSAGE("MsgBed", "gravel bed of new track: %s", g_bed ? std::to_string(g_bed).c_str() : ui_text("WordNone", "none"));
        }
        else if (w == VK_DELETE && !g_selection.empty() && is_car(g_selection[0]))   // the car; with Shift its train
            delete_objects(shift_down() ? g_selection : std::vector<uint8_t*>{g_selection[0]});
        else if (w == VK_DELETE)
            delete_objects(shift_down() && !g_selection.empty() && class_of(g_selection[0]) == B_RAIL
                               ? section_of(g_selection[0]) : g_selection);
        else if (w == 'U' && !g_selection.empty() && is_car(g_selection[0])) uncouple(g_selection[0]);
        else if (w == 'C' && !g_selection.empty() && is_car(g_selection[0])) couple(g_selection[0]);
        else if (w == 'L' && entry().tool != Tool::PushLine && entry().tool != Tool::Track) post_line(shift_down());
        else if (w == 'R') turn(1);
        else if (w == 'E') turn(-1);
        else if (w == 'A') next_army();
        else if (w == 'T') toggle_semaphore();
        else if (w == VK_ESCAPE) {
            if (g_patrol_train) {
                g_patrol_train = nullptr;
            } else if (entry().tool == Tool::Track && !g_line.empty()) {
                end_line();
            } else if (entry().tool == Tool::PushLine && g_push_started) {
                g_push_started = false;
            } else {
                choose_entry(0);
                select(nullptr);
            }
        } else if (w == VK_OEM_4 || w == VK_OEM_6) {
            if (!g_decals.empty()) {
                g_decal = (g_decal + (w == VK_OEM_6 ? 1 : static_cast<int>(g_decals.size()) - 1)) %
                          static_cast<int>(g_decals.size());
                g_entry = DECAL_ENTRY;
                const char* name = object_name(decal_nvid());
                MESSAGE("MsgDecal", "decal: %s (VID %d)", name ? name : "?", decal_nvid());
            }
        } else if (w >= '0' && w <= '9') {
            choose_entry((static_cast<int>(w - '0') + 9) % 10);
        } else {
            return false;   // the arrows scroll, the rest is the game's
        }
        return true;
    }
    case WM_CHAR:   // the debug keys of the game are characters (DebugMode): ours are not for it
        return std::strchr("rReEaAtTlLuUcC[]0123456789\x1b\x1a\x19", static_cast<int>(w)) != nullptr ||
               (entry().tool == Tool::PushLine && (w == 's' || w == 'S')) ||
               ((entry().tool == Tool::Height || entry().tool == Tool::Decals) && std::strchr("+-=mM", static_cast<int>(w))) ||
               (entry().tool == Tool::Decals && std::strchr(",.<>", static_cast<int>(w))) ||
               ((entry().tool == Tool::Paint || entry().tool == Tool::Water) && std::strchr("+-=", static_cast<int>(w))) ||
               (entry().tool == Tool::Water && (w == 'w' || w == 'W')) ||
               (entry().tool == Tool::Track && std::strchr("bBvV", static_cast<int>(w)));
    }
    return false;
}

void edit_command(int id) {
    if (id == ID_UNDO) undo(false);
    else if (id == ID_REDO) undo(true);
    else if (id == ID_DELETE) delete_objects(g_selection);
    else if (id == ID_ROTATE_RIGHT) turn(1);
    else if (id == ID_ROTATE_LEFT) turn(-1);
    else if (id == ID_ARMY) next_army();
    else if (id == ID_SEMAPHORE) toggle_semaphore();
}

// A level goes away (or comes): its objects with it.
void edit_reset() {
    panel_reset();
    g_undo.clear();
    g_redo.clear();
    g_selection.clear();
    g_drag = Drag();
    g_dirty = false;
    g_line.clear();
    g_line_cells.clear();
    g_line_original.clear();
    g_push_started = false;
    g_push_selected = -1;
    g_patrols.clear();
    g_patrol_train = nullptr;
    g_context = nullptr;
    ground_reset();
    g_grid_original.clear();
    g_gstroke = GroundStroke();
    g_region_drag = false;
}

// editor_mission.cpp, writing main()'s block: every train's patrol with its first car as it stands.
std::vector<TrainPatrol> edit_train_patrols() {
    std::vector<TrainPatrol> out;
    std::set<uint8_t*> done;
    for (const auto& p : g_patrols) {
        if (removed(p.first)) continue;
        uint8_t* first = train_of(p.first).front();
        if (!done.insert(first).second) continue;
        out.push_back({nvid_of(first), static_cast<int>(std::lround(at<float>(first, O_X))),
                       static_cast<int>(std::lround(at<float>(first, O_Y))), p.second.first, p.second.second});
    }
    return out;
}

// Test mode (editor.cpp's WM_COPYDATA "D"): the rail points of g_railMap near a map point, with
// their links, into LocoFix.log - to see that a deleted rail's points leave the network.
void edit_log_rail_points(float x, float y) {
    const int count = at<int>(reinterpret_cast<void*>(G_RAIL_MAP), RAIL_MAP_COUNT);
    uint8_t** dots = at<uint8_t**>(reinterpret_cast<void*>(G_RAIL_MAP), RAIL_MAP_ITEMS);
    std::string text;
    int near_dots = 0;
    for (int i = 0; i < count && dots; ++i) {
        uint8_t* d = dots[i];
        const int dx = at<int>(d, DOT_X), dy = at<int>(d, DOT_Y);
        if (std::fabs(dx - x) > 60 || std::fabs(dy - y) > 60) continue;
        ++near_dots;
        char item[96];
        std::snprintf(item, sizeof item, " (%d,%d,%d refs %d links %d:", dx, dy, at<int>(d, DOT_Z), at<int>(d, 0),
                      at<int>(d, DOT_LINK_COUNT));
        text += item;
        for (int k = 0; k < at<int>(d, DOT_LINK_COUNT) && k < 6; ++k) {
            uint8_t* o = at<uint8_t*>(d, DOT_LINKS + k * DOT_LINK_SIZE);
            std::snprintf(item, sizeof item, " %d,%d", at<int>(o, DOT_X), at<int>(o, DOT_Y));
            text += item;
        }
        text += ")";
    }
    patch::log("edit test: %d rail points of %d near %.0f,%.0f:%s", near_dots, count, x, y, text.c_str());
}

bool edit_dirty() { return g_dirty; }

// editor_panel.cpp's drawing: pictures and texts in front of the scene as the bar's, the view and the bar.
void edit_draw_picture(int nvid, int frame, float x, float y, int step) { draw_screen(nvid, frame, x, y, step); }
float edit_draw_text(int font, float x, float y, const char* text, int step) { return draw_text(font, x, y, text, false, step); }
bool edit_font_size(int font, float& w, float& h) {
    uint8_t* v = vid_by_number(font);
    w = v ? at<float>(v, V_SIZE_X) : 8;
    h = v ? at<float>(v, V_SIZE_Y) : 13;
    return v != nullptr;
}
void edit_view_box(float& l, float& t, float& r, float& b) {
    const Box v = view();
    l = v.l, t = v.t, r = v.r, b = v.b;
}
void edit_bar_box(float& l, float& t, float& r, float& b) {
    const Box v = bar();
    l = v.l, t = v.t, r = v.r, b = v.b;
}
void edit_saved() { g_dirty = false; }

void install_editor_edit() {
    char value[8] = {};
    g_test = GetEnvironmentVariableA("LOCOFIX_TEST", value, sizeof value) && value[0] == '1';
    // sub esp,0x10 / push ebx / push ebp / push esi / mov esi,ecx
    const uint8_t draw_scene[] = {0x83, 0xEC, 0x10, 0x53, 0x55, 0x56, 0x8B, 0xF1};
    g_draw_scene = reinterpret_cast<DrawScene_t>(patch::hook(DRAW_SCENE, draw_scene, sizeof draw_scene,
                                                             reinterpret_cast<void*>(&DrawScene_editor),
                                                             "Graphics_DrawScene"));
}

// editor.cpp after a level is held: the decals it has, for the bar.
void edit_level_held() {
    set_army_gammas();
    find_decals();
    choose_default_decal();
    log_push_lines();
    // the trains' patrols of the mission's script: to the car of that VID standing there
    g_patrols.clear();
    for (const TrainPatrol& p : mission_patrols()) {
        uint8_t* best = nullptr;
        float best_d = 20;
        for (uint8_t* c : all_cars()) {
            const float d = std::hypot(at<float>(c, O_X) - p.x, at<float>(c, O_Y) - p.y);
            if (nvid_of(c) == p.nvid && d <= best_d) {
                best_d = d;
                best = c;
            }
        }
        if (best) g_patrols[best] = {p.to_x, p.to_y};
        else patch::log("edit: no train of VID %d at %d,%d for its patrol", p.nvid, p.x, p.y);
    }
    // the gravel bed of new track: the one the map has most of, else the usual one
    std::map<int, int> beds;
    for_each_object([&](uint8_t* o) {
        if (is_bed(nvid_of(o))) ++beds[nvid_of(o)];
    });
    g_bed = BEDS[0];
    for (const auto& b : beds)
        if (b.second > beds[g_bed]) g_bed = b.first;
    // A red post's colour is its first command (10,0,0,0: ANI_STAND_OPEN), which runs only once the
    // objects are updated - never on a held level - so it is shown here; the command stays as it is.
    int shown = 0;
    for_each_object([&](uint8_t* o) {
        if (nvid_of(o) == VID_SEMAPHORE && red(o)) {
            reinterpret_cast<SetInt_t>(OBJECT_SET_ANIMATION)(o, nullptr, ANI_STAND_OPEN);
            ++shown;
        }
    });
    if (shown) patch::log("edit: %d red semaphores shown red", shown);
}

// editor.cpp after the mission settings changed.
void edit_touch() { g_dirty = true; }

// editor.cpp's File > Map size: the size of the held map, and the objects that a size w x h leaves
// outside - counted, or deleted (one step of the history). The track goes on out of sight at the
// ends of the main line, so rails, their beds and the cars on them up to 300 px past the new edge stay.
void edit_map_size(int& w, int& h) {
    w = grid_width() * 8;
    h = grid_height() * 8;
}

int edit_outside(int w, int h, bool remove) {
    std::vector<uint8_t*> out;
    for_each_object([&](uint8_t* o) {
        if (!pickable(o) && !is_bed(nvid_of(o))) return;
        const float x = at<float>(o, O_X), y = at<float>(o, O_Y);
        if (x >= 0 && y >= 0 && x < w && y < h) return;
        const bool track = class_of(o) == B_RAIL || class_of(o) == B_ENGINE || is_bed(nvid_of(o));
        const float beyond = (std::max)((std::max)(-x, x - w), (std::max)(-y, y - h));
        if (track && beyond <= 300) return;
        out.push_back(o);
    });
    if (remove && !out.empty()) delete_objects(out);
    return static_cast<int>(out.size());
}

// editor_panel.cpp's palette: an object picked - the bar's "All objects" entry (a car: the
// "Vehicles" entry) places it, in the army the missions give it.
void edit_pick_object(int nvid, int army) {
    const auto decal = std::find(g_decals.begin(), g_decals.end(), nvid);
    if (g_entry == DECAL_ENTRY && decal != g_decals.end()) {   // opened from the Decals entry: the brush's decal
        g_decal = static_cast<int>(decal - g_decals.begin());
        MESSAGE("MsgDecal", "decal: %s (VID %d)", object_name(nvid) ? object_name(nvid) : "?", nvid);
        return;
    }
    uint8_t* v = vid_by_number(nvid);
    const int which = v && at<int>(v, V_CLASS) == B_ENGINE ? 1 : 0;
    g_picked_nvid[which] = nvid;
    g_picked_army[which] = army;
    for (int i = 0; i < ENTRY_COUNT; ++i)
        if (ENTRIES[i].nvid == (which ? VEHICLES_NVID : PALETTE_NVID)) g_entry = i;
    end_line();
    g_selection.clear();
    g_context = nullptr;
    MESSAGE("MsgPlace", "place: %s (VID %d)", place_label(), nvid);
}