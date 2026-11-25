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
// always cleared; a plain object (a building or decoration of class OBJECT, a shell, a sprite) that
// only the world and a script held was freed with the script's variable still pointing at it.
// Delete(1) frees the memory at once, so the next `GetX` reads freed memory (soon another object)
// and the next `Destroy` calls a vtable out of it: a modder's map script that destroyed an object
// when the engine stood on it and kept the variable crashed at the second `Destroy` (exception
// C0000096 at an address in no module, the return into Script_CallExtern's case 84 on the stack).
//
// The hook runs the destructor as it is and then Script_ObjectDeleted for the object - the engine's
// own clearing, for every object. The memory is still there (the deleting destructor frees it after
// Object_Destruct returns) and the values are only compared with the pointer. A pointer check at the
// time of use could not be trusted (the freed memory is soon another object), so the value is
// cleared when the object goes, as the engine does for units. For the objects the engine cleared
// already nothing is found. The pass also covers the destroy script's parameter: Object_Destruct
// runs F<nvid>_DESTROY with the object after a unit's own clearing.
//
// LocoFix.log tells a value cleared only here when it is one of the script's own variables, by its
// name (the first 20 objects of a run):
//
//   script: object 0x0A1B2C30 (VID 171 at 400,1200) was deleted while variable fruit held it - it reads 0 now
//
// The parameters and the variables of script functions are not told: they keep the last object of
// their last call, which the next call writes anyway - the engine's events leave the shell that hit
// in TrainAttacked(train, enemy) and the handlers it calls, F070_0 the robot's link in its `link`,
// and in the campaign they were cleared every few seconds.
//
// The same hook tells the objects that walk to a deleted house first (robot_walk.cpp, BUGS.md F34).
#include "patch.h"
#include "script_value.h"

#include <string>
#include <vector>

void robot_walk_deleting(uint8_t* object);   // robot_walk.cpp

namespace {

constexpr uintptr_t OBJECT_DESTRUCT = 0x445cc0;       // void __thiscall Object_Destruct(GameObject*)
constexpr uintptr_t SCRIPT_OBJECT_DELETED = 0x43d980; // int __thiscall Script_ObjectDeleted(Script*, GameObject*)
constexpr uintptr_t G_WORLD = 0x4a9d38;               // World *g_world
constexpr int WORLD_SCRIPT = 0x14c;                   // World.script (struct Script)
constexpr int SCRIPT_COUNT = 0x04;                    // Script.stackCount
constexpr int SCRIPT_STACK = 0x0c;                    // Script.stack (ScriptValue*)
constexpr int SCRIPT_SYMBOL_COUNT = 0x14;             // Script.symbols.count
constexpr int SCRIPT_SYMBOLS = 0x1c;                  // Script.symbols.items (ScriptSymbol*, 0x18 each)
constexpr int SYMBOL_SIZE = 0x18;
constexpr int SYMBOL_NAME = 0x00;                     // String (its text pointer)
constexpr int SYMBOL_KIND = 0x04;                     // 1 variable
constexpr int SYMBOL_VALUE = 0x0c;                    // variable: first slot
constexpr int SYMBOL_COUNT = 0x14;                    // variable: elements
constexpr int OBJ_VID = 0x1c;
constexpr int OBJ_X = 0x30;
constexpr int OBJ_Y = 0x34;
constexpr int VID_NVID = 0x04;
constexpr int LOG_LINES = 20;
constexpr int MAX_SLOTS = 8;   // slots looked up by name per object

using Destruct_t = void(__fastcall*)(uint8_t* object, void* edx);
using ObjectDeleted_t = int(__fastcall*)(uint8_t* script, void* edx, uint8_t* object);

Destruct_t g_destruct = nullptr;
int g_logged = 0;

template <typename T> T at(const uint8_t* p, int offset) { return *reinterpret_cast<const T*>(p + offset); }

// The name of the script variable the slot belongs to ("name" or "name[i]"), or "" for any other
// slot: the parameters of the script functions and the variables declared in them (the compiler
// drops their names at the closing brace, so after compiling only the script's own variables have
// a symbol of kind 1) and the values being computed.
std::string slot_name(const uint8_t* script, int slot) {
    const int count = at<int>(script, SCRIPT_SYMBOL_COUNT);
    const uint8_t* symbols = at<uint8_t*>(script, SCRIPT_SYMBOLS);
    for (int i = 0; symbols && i < count; ++i) {
        const uint8_t* s = symbols + i * SYMBOL_SIZE;
        if (at<uint8_t>(s, SYMBOL_KIND) != 1) continue;
        const int first = at<int>(s, SYMBOL_VALUE);
        const int n = at<int>(s, SYMBOL_COUNT);
        if (slot < first || slot >= first + (n > 0 ? n : 1)) continue;
        const char* name = at<const char*>(s, SYMBOL_NAME);
        std::string text = std::string("variable ") + (name ? name : "?");
        if (n > 1) text += "[" + std::to_string(slot - first) + "]";
        return text;
    }
    return "";
}

void __fastcall object_destruct(uint8_t* object, void* edx) {
    robot_walk_deleting(object);

    // what the line says, read while the object is whole
    const uint8_t* vid = at<uint8_t*>(object, OBJ_VID);
    const int nvid = vid ? at<int>(vid, VID_NVID) : -1;
    const int x = static_cast<int>(at<float>(object, OBJ_X));
    const int y = static_cast<int>(at<float>(object, OBJ_Y));

    g_destruct(object, edx);

    uint8_t* world = *reinterpret_cast<uint8_t**>(G_WORLD);
    if (!world) return;
    uint8_t* script = world + WORLD_SCRIPT;
    const int count = at<int>(script, SCRIPT_COUNT);
    const sv::Value* stack = at<sv::Value*>(script, SCRIPT_STACK);
    if (!stack || count <= 0) return;

    // the values Script_ObjectDeleted is going to clear (the same test)
    int slots[MAX_SLOTS];
    int found = 0;
    const int32_t pointer = static_cast<int32_t>(reinterpret_cast<uintptr_t>(object));
    for (int i = 0; i < count; ++i)
        if ((stack[i].flags & 0x10) && stack[i].value == pointer) {
            if (found < MAX_SLOTS) slots[found] = i;
            ++found;
        }
    if (!found) return;
    reinterpret_cast<ObjectDeleted_t>(SCRIPT_OBJECT_DELETED)(script, nullptr, object);

    if (g_logged > LOG_LINES) return;
    std::vector<std::string> names;
    for (int k = 0; k < found && k < MAX_SLOTS; ++k) {
        const std::string name = slot_name(script, slots[k]);
        bool known = name.empty();
        for (const std::string& n : names) known = known || n == name;
        if (!known) names.push_back(name);
    }
    if (names.empty()) return;
    std::string held;
    for (const std::string& n : names) held += (held.empty() ? "" : ", ") + n;
    const int told = static_cast<int>(names.size());
    if (++g_logged > LOG_LINES) {
        patch::log("script: more objects deleted while a script variable held them - not logged");
        return;
    }
    patch::log("script: object 0x%08X (VID %d at %d,%d) was deleted while %s held it - %s 0 now",
               static_cast<unsigned>(pointer), nvid, x, y, held.c_str(), told == 1 ? "it reads" : "they read");
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
