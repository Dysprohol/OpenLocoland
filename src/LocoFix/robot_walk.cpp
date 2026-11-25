// A civil robot is told when the house it walks to is deleted (BUGS.md F35).
//
// A civil robot (B_CIV_ROBOT: VID 72, the miner, and 77, the mechanic) strolls about its town.
// In its stroll state 0xe, CivRobot_Action picks a house near it (CivRobot_FindNearestObject:
// one of the 16 VIDs of the table at 0x47ecd0 - the station 170 and the town houses 803-895),
// sets off towards the point in front of its door and puts ACT_MOVE_TO with the house as var1
// at the bottom of its command stack (0x438357: CommandList_PushBottom(0x22, house)), so when
// the walk ends it turns to the house (CivRobot_Action case 0x22 reads the house's x, y, z,
// direction and its VID's height, Vid+0x60).
//
// That entry holds the house as a bare pointer: it takes no reference. The engine drops a deleted
// object from the command stacks in its round of World_ObjectDeleted - every world object's
// OnObjectDeleted (vtable slot 4; Object_OnObjectDeleted turns an ACT_MOVE_TO to it into ACT_NONE) -,
// but Object_Destruct runs that round only while the object's reference count says somebody holds
// it (refCount > 1 after the object map's own reference is gone), and the round stops as soon as
// the count falls to 1. A house that nothing else refers to - every house, as a rule - is deleted
// without the round, and the robot's entry points at freed memory. When the robot gets there, case
// 0x22 reads the house from that memory: while the memory is not used again it reads the old
// values and the robot turns to where the house stood; once it is, the game crashes at 0x4378d4
// (`fadd [edx+0x60]` with the VID read as 0). A house dies in a fight (its hit points run out,
// Object_Update deletes it at the end of its death animation) or by a script (Destroy, Genocide,
// ReplaceUnit).
//
// When an object of one of the 16 VIDs is deleted, every world object whose command stack holds an
// ACT_MOVE_TO to it is told through its own OnObjectDeleted, as the engine's round would tell it:
// CivRobot_OnObjectDeleted -> Creature_OnObjectDeleted -> Object_OnObjectDeleted marks the entry
// ACT_NONE (act 0xff, var1 0). The robot finds the entry empty at the end of its walk and strolls on.
// Not while Level_Release deletes every object (g_deletingAllObjects): the robots go too. Called by
// the Object_Destruct hook of script_refs.cpp, before the destructor, while the object is whole.
//
// LocoFix.log names the first 20 houses of a run whose walks were dropped:
//
//   robot walk: VID 803 at 1424,2018 deleted while 1 object(s) walked to it - told
#include "patch.h"

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;           // World* g_world
constexpr uintptr_t G_DELETING_ALL = 0x489934;    // g_deletingAllObjects: 1 inside Level_Release
constexpr uintptr_t WALK_VIDS = 0x47ecd0;         // the 16 VIDs a civil robot walks to (int[16])
constexpr int WALK_VID_COUNT = 16;

constexpr int W_LAYERS = 0x4c;   // World: ObjectList[16] (count +4, items +0xc)
constexpr int LAYERS = 16;
constexpr int O_VID = 0x1c, O_X = 0x30, O_Y = 0x34;
constexpr int O_COMMANDS = 0x58;   // CommandList: count +4, items +0xc (Command: act, var1, var2, var3)
constexpr int V_NVID = 0x04;
constexpr int OBJECT_DELETED_SLOT = 4;   // GameObject vtable: OnObjectDeleted(GameObject* deleted)

constexpr int ACT_MOVE_TO = 0x22;
constexpr int LOG_LINES = 20;

using ObjectDeleted_t = void(__fastcall*)(uint8_t* object, void* edx, uint8_t* deleted);

int g_logged = 0;

template <typename T> T at(const uint8_t* p, int offset) { return *reinterpret_cast<const T*>(p + offset); }

bool walk_target(int nvid) {
    const int* vids = reinterpret_cast<const int*>(WALK_VIDS);
    for (int i = 0; i < WALK_VID_COUNT; ++i)
        if (vids[i] == nvid) return true;
    return false;
}

bool walks_to(const uint8_t* object, const uint8_t* target) {
    const int count = at<int>(object, O_COMMANDS + 0x04);
    const int* entry = at<const int*>(object, O_COMMANDS + 0x0c);
    if (!entry) return false;
    for (int k = 0; k < count; ++k, entry += 4)
        if (entry[0] == ACT_MOVE_TO && entry[1] == static_cast<int>(reinterpret_cast<uintptr_t>(target))) return true;
    return false;
}

}  // namespace

void robot_walk_deleting(uint8_t* object) {
    if (!object || *reinterpret_cast<const int*>(G_DELETING_ALL)) return;
    const uint8_t* vid = at<uint8_t*>(object, O_VID);
    if (!vid || !walk_target(at<int>(vid, V_NVID))) return;
    uint8_t* world = *reinterpret_cast<uint8_t**>(G_WORLD);
    if (!world) return;
    int told = 0;
    for (int layer = 0; layer < LAYERS; ++layer) {
        const uint8_t* list = world + W_LAYERS + layer * 0x10;
        uint8_t* const* items = at<uint8_t* const*>(list, 0x0c);
        if (!items) continue;
        for (int i = 0; i < at<int>(list, 0x04); ++i) {
            uint8_t* other = items[i];   // deleted objects leave null slots
            if (!other || other == object || !walks_to(other, object)) continue;
            auto deleted = reinterpret_cast<ObjectDeleted_t>(at<void**>(other, 0)[OBJECT_DELETED_SLOT]);
            deleted(other, nullptr, object);
            ++told;
        }
    }
    if (!told) return;
    if (g_logged < LOG_LINES)
        patch::log("robot walk: VID %d at %.0f,%.0f deleted while %d object(s) walked to it - told",
                   at<int>(vid, V_NVID), at<float>(object, O_X), at<float>(object, O_Y), told);
    else if (g_logged == LOG_LINES)
        patch::log("robot walk: more houses deleted while robots walked to them - not logged");
    ++g_logged;
}
