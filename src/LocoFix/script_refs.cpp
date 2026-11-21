// A script forgets every object that is deleted, not only the ones something else holds (BUGS.md F32).
//
// A script value with flag 0x10 holds a pointer to a GameObject - what the scripts call a unit
// (ScriptValue_InitObject 0x442460: flags 0x12; engine.md "Values and variables"). It is not a
// reference: the object's refCount (+0x2c) does not count it. The engine keeps such values right in
// another way: when an object is deleted, Script_ObjectDeleted (0x43d980) sets every value of the
// script stack that holds it to 0 and drops the flag, so `GetX(u)`, `Destroy(u)` and the rest see 0
// afterwards. Only World_ObjectDeleted (0x41bf00, World vtable slot 2) calls it, and the GameObject
// destructor Object_Destruct (0x445cc0) calls that only when the object still has more than one
// reference after it left the object map - a goal, a command or a list of someone else. Units
// (Unit_Destruct), regions and menu frames call it in their own destructors, so their values were
// always cleared; a plain object (a building or decoration of class OBJECT, TERRAIN, a cannon, a
// sprite) that only the world and a script hold was freed with the script's variable still pointing
// at it. Delete(1) frees the memory at once, so the next `GetX` reads freed memory and the next
// `Destroy` calls a vtable out of it: a modder's map script that destroyed an object when the engine
// stood on it and kept the variable crashed at the second `Destroy` (exception C0000096 at an
// address in no module, the return into Script_CallExtern's case 84 on the stack).
//
// The hook runs the destructor as it is and then Script_ObjectDeleted for the object - the engine's
// own clearing, for every object. The memory is still there (the deleting destructor frees it after
// Object_Destruct returns) and Script_ObjectDeleted only compares pointers. A pointer check at the
// time of use could not be trusted (the freed memory is soon another object), so the value is
// cleared when the object goes, as the engine does for units. Clearing a second time finds nothing:
// for the objects the engine already cleared nothing changes. The pass also covers the destroy
// script's parameter: Object_Destruct runs F<nvid>_DESTROY with the object after a unit's own
// clearing, which left the parameter's slot holding it.
//
// A value cleared only here is told in LocoFix.log, one line per object (the first 20 of a run):
//
//   script: object 0x0A1B2C30 (VID 171 at 400,1200) was deleted while 1 script value held it - it reads 0 now
#include "patch.h"

namespace {

constexpr uintptr_t OBJECT_DESTRUCT = 0x445cc0;       // void __thiscall Object_Destruct(GameObject*)
constexpr uintptr_t SCRIPT_OBJECT_DELETED = 0x43d980; // int __thiscall Script_ObjectDeleted(Script*, GameObject*)
constexpr uintptr_t G_WORLD = 0x4a9d38;               // World *g_world
constexpr int WORLD_SCRIPT = 0x14c;                   // World.script (struct Script)
constexpr int OBJ_VID = 0x1c;
constexpr int OBJ_X = 0x30;
constexpr int OBJ_Y = 0x34;
constexpr int VID_NVID = 0x04;
constexpr int LOG_LINES = 20;

using Destruct_t = void(__fastcall*)(uint8_t* object, void* edx);
using ObjectDeleted_t = int(__fastcall*)(uint8_t* script, void* edx, uint8_t* object);

Destruct_t g_destruct = nullptr;
int g_logged = 0;

template <typename T> T at(const uint8_t* p, int offset) { return *reinterpret_cast<const T*>(p + offset); }

void __fastcall object_destruct(uint8_t* object, void* edx) {
    // what the line says, read while the object is whole
    const uint8_t* vid = at<uint8_t*>(object, OBJ_VID);
    const int nvid = vid ? at<int>(vid, VID_NVID) : -1;
    const int x = static_cast<int>(at<float>(object, OBJ_X));
    const int y = static_cast<int>(at<float>(object, OBJ_Y));

    g_destruct(object, edx);

    uint8_t* world = *reinterpret_cast<uint8_t**>(G_WORLD);
    if (!world) return;
    const int cleared = reinterpret_cast<ObjectDeleted_t>(SCRIPT_OBJECT_DELETED)(world + WORLD_SCRIPT, nullptr, object);
    if (cleared <= 0 || g_logged > LOG_LINES) return;
    if (++g_logged > LOG_LINES) {
        patch::log("script: more objects deleted while a script held them - not logged");
        return;
    }
    patch::log("script: object 0x%08X (VID %d at %d,%d) was deleted while %d script value%s held it - %s 0 now",
               static_cast<unsigned>(reinterpret_cast<uintptr_t>(object)), nvid, x, y, cleared,
               cleared == 1 ? "" : "s", cleared == 1 ? "it reads" : "they read");
}

}  // namespace

void install_script_refs() {
    // push -1 / push 0x474516 (the SEH frame of the destructor)
    const uint8_t destruct_start[] = {0x6A, 0xFF, 0x68, 0x16, 0x45, 0x47, 0x00};
    g_destruct = reinterpret_cast<Destruct_t>(patch::hook(OBJECT_DESTRUCT, destruct_start, sizeof destruct_start,
                                                          reinterpret_cast<void*>(&object_destruct), "Object_Destruct"));
    if (g_destruct)
        patch::log("script values: cleared for every deleted object @ %08X", static_cast<unsigned>(OBJECT_DESTRUCT));
}
