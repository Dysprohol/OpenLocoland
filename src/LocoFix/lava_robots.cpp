// An infantry robot that walks into lava dies there (changes.md, editor-ground.md "Sheets of water and lava").
//
// The lava of a map is a sheet of the lava's 128 px tiles (VID 293, GroundVid with flag 0x10000: a
// SPRITE on its VID's layer, 6), all at one level, drawn with the depth test: the lava shows wherever
// the ground is lower than the sheet (MAP_RULES.md 2 - the missions' l12 and l13, the editor's Water
// and lava tool and the Region tool's LAVA sheets lay the same tiles). The engine gives the lava one
// rule of its own: a region over it turns the explosions made there into the explosion on lava (VID
// 528, Vid_GetTerrainVariant). A unit walks over it as over any ground - the tiles' moveMask (0xf) has
// no bit of the robot's (0x10) - and nothing happens to it. The missions' water has a rule in the
// scripts (gametact.lgc with WATER_Z, l11, l16, l17: a unit below the water's level takes damage every
// second), which stays as it is; the lava has none.
//
// Here the infantry robot (VID 70, B_UNIT; the paratrooper 84 becomes one when it lands) dies when it
// stands below the level of a lava tile that covers its place - where the lava shows over the ground
// it walks on -, with the game's own death and effect: ACT_DAMAGE of all its hp, as the water's
// script does it with ACT_DAMAGE (Object_SetHp: the kill counted, ANI_DEATH and its explosion, VID
// 332), and the explosion on lava (VID 528) at the lava's level on its place. The check runs after
// Unit_Move (0x44c640, Unit vtable slot 3), the robot's step in every update of the world (none while
// the game or the editor's held level is paused). A robot on the shore, beside the lava or over a
// sheet laid below the ground (hidden there) is not touched, nor is any other unit.
//
// LocoFix.log names the first 20 robots of a run:
//
//   lava: infantry robot (army 1) at 420,1310, z 52 - in the lava at 58 (tile at 448,1344): it dies
#include "patch.h"

#include <vector>

namespace {

constexpr uintptr_t UNIT_MOVE = 0x44c640;   // void __thiscall Unit_Move(Unit*)
constexpr uintptr_t G_WORLD = 0x4a9d38;     // World* g_world
constexpr uintptr_t G_GAME_TIME = 0x4a9d64;

constexpr int VID_INFANTRY = 70;
constexpr int VID_LAVA = 293;
constexpr int VID_LAVA_EXPLOSION = 528;
constexpr int ACT_DAMAGE = 85;
constexpr int ANI_DEATH = 15;
constexpr int LOG_LINES = 20;

// World, ObjectList, GameObject, Vid (re/types.txt)
constexpr int W_FRAME_COUNT = 0x20;
constexpr int W_LAYERS = 0x4c;   // ObjectList[16]: count +4, items +0xc
constexpr int W_CREATE_OBJECT = 0x20;   // World vtable slot 8 (GameWorld_CreateObject)
constexpr int W_VID_COUNT = 0x290;
constexpr int W_VIDS = 0x294;
constexpr int O_VID = 0x1c, O_FLAGS = 0x28, O_X = 0x30, O_Y = 0x34, O_Z = 0x38, O_ANIMATION = 0x48, O_HP = 0x54;
constexpr int V_NVID = 0x04, V_HALF_X = 0x334, V_HALF_Y = 0x338, V_LAYER = 0x33c, V_COUNT = 0x358;
constexpr uint32_t OBJ_REMOVED = 0x100;

using Move_t = void(__fastcall*)(uint8_t* unit, void* edx);
using Action_t = int(__fastcall*)(uint8_t* object, void* edx, int act, int var1, int var2, int var3);
using CreateObject_t = uint8_t*(__fastcall*)(uint8_t* world, void* edx, uint8_t* vid, float x, float y, float z,
                                             unsigned direction, void* parent);

Move_t g_unit_move = nullptr;
int g_logged = 0;

template <typename T> T at(const uint8_t* p, int offset) { return *reinterpret_cast<const T*>(p + offset); }

uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }

uint8_t* vid_by_number(uint8_t* w, int nvid) {
    return nvid < at<int>(w, W_VID_COUNT) ? reinterpret_cast<uint8_t**>(w + W_VIDS)[nvid] : nullptr;
}

// The lava tiles of the world as they are in this update: their boxes and levels, taken once a step
// (the world's game time and frame), so a sheet that rises or a tile laid or taken away counts at once.
struct Tile {
    float left, right, top, bottom, z;
};
std::vector<Tile> g_tiles;
float g_highest = 0;   // the highest tile's level: a robot at or above it is in no lava
const uint8_t* g_tiles_world = nullptr;
uint32_t g_tiles_time = 0;
int g_tiles_frame = -1;

void gather_tiles(uint8_t* w) {
    const uint32_t time = *reinterpret_cast<const uint32_t*>(G_GAME_TIME);
    const int frame = at<int>(w, W_FRAME_COUNT);
    if (w == g_tiles_world && time == g_tiles_time && frame == g_tiles_frame) return;
    g_tiles_world = w;
    g_tiles_time = time;
    g_tiles_frame = frame;
    g_tiles.clear();
    uint8_t* lava = vid_by_number(w, VID_LAVA);
    if (!lava) return;
    const int* count = reinterpret_cast<const int*>(lava + V_COUNT);
    if (count[0] + count[1] + count[2] + count[3] <= 0) return;   // no tile on the map
    const int layer = at<int>(lava, V_LAYER);
    if (layer < 0 || layer >= 16) return;
    const uint8_t* list = w + W_LAYERS + layer * 0x10;
    const int n = at<int>(list, 0x04);
    uint8_t* const* items = at<uint8_t* const*>(list, 0x0c);
    if (!items) return;
    const float half_x = at<float>(lava, V_HALF_X), half_y = at<float>(lava, V_HALF_Y);
    for (int i = 0; i < n; ++i) {
        const uint8_t* o = items[i];   // deleted objects leave null slots
        if (!o || at<uint8_t*>(o, O_VID) != lava || (at<uint32_t>(o, O_FLAGS) & OBJ_REMOVED)) continue;
        const float x = at<float>(o, O_X), y = at<float>(o, O_Y), z = at<float>(o, O_Z);
        if (g_tiles.empty() || z > g_highest) g_highest = z;
        g_tiles.push_back({x - half_x, x + half_x, y - half_y, y + half_y, z});
    }
}

void __fastcall unit_move(uint8_t* unit, void* edx) {
    g_unit_move(unit, edx);

    const uint8_t* vid = at<uint8_t*>(unit, O_VID);
    if (!vid || at<int>(vid, V_NVID) != VID_INFANTRY) return;
    if (at<int>(unit, O_ANIMATION) >= ANI_DEATH || at<int>(unit, O_HP) <= 0) return;   // dying already
    if (at<uint32_t>(unit, O_FLAGS) & OBJ_REMOVED) return;
    uint8_t* w = world();
    if (!w) return;
    gather_tiles(w);
    if (g_tiles.empty()) return;
    const float x = at<float>(unit, O_X), y = at<float>(unit, O_Y), z = at<float>(unit, O_Z);
    if (z >= g_highest) return;
    const Tile* in = nullptr;
    for (const Tile& t : g_tiles)
        if (z < t.z && x >= t.left && x <= t.right && y >= t.top && y <= t.bottom && (!in || t.z > in->z)) in = &t;
    if (!in) return;
    const Tile tile = *in;   // the damage below may lay or take tiles (scripts), not this copy

    const int army = static_cast<int>(at<uint32_t>(unit, O_FLAGS) >> 10 & 3);
    if (g_logged < LOG_LINES)
        patch::log("lava: infantry robot (army %d) at %.0f,%.0f, z %.0f - in the lava at %.0f (tile at %.0f,%.0f): it dies",
                   army, x, y, z, tile.z, (tile.left + tile.right) / 2, (tile.top + tile.bottom) / 2);
    else if (g_logged == LOG_LINES)
        patch::log("lava: more infantry robots died in the lava - not logged");
    ++g_logged;

    if (uint8_t* splash = vid_by_number(w, VID_LAVA_EXPLOSION)) {
        auto create = reinterpret_cast<CreateObject_t>(at<void**>(w, 0)[W_CREATE_OBJECT / 4]);
        create(w, nullptr, splash, x, y, tile.z, 0, nullptr);
    }
    auto action = reinterpret_cast<Action_t>(at<void**>(unit, 0)[1]);
    action(unit, nullptr, ACT_DAMAGE, at<int>(unit, O_HP), 0, 0);
}

}  // namespace

void install_lava_robots() {
    // sub esp, 0x10 / push esi / mov esi, ecx
    const uint8_t unit_move_start[] = {0x83, 0xEC, 0x10, 0x56, 0x8B, 0xF1};
    g_unit_move = reinterpret_cast<Move_t>(patch::hook(UNIT_MOVE, unit_move_start, sizeof unit_move_start,
                                                       reinterpret_cast<void*>(&unit_move), "Unit_Move"));
    if (g_unit_move)
        patch::log("lava: infantry robots die in lava @ %08X", static_cast<unsigned>(UNIT_MOVE));
}
