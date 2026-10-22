// Native rewrites of the World script stack helpers.
//
// World_PopInt (0x4429c0), World_PopString (0x442a20), World_PopObject (0x442a40),
// World_PopVid (0x442ae0) and Script_PopObject (0x442670) are the helpers every script
// extern uses to read its arguments: they take the top value off the script stack (the
// count at +0x150 of the World, the values at +0x158) and convert it. All five are
// hooked at their entry with a jump; the C++ keeps the calling conventions (__thiscall,
// `ret 4` for the VID) and the results.
//
// World_PopInt and World_PopObject return the World pointer when "%i" does not convert
// a text, Script_PopObject the script machine and World_PopVid the text of the caller,
// so the native versions return 0 there (BUGS.md F9).
#include "world_script.h"
#include "patch.h"

namespace {

constexpr uintptr_t POP_INT    = 0x4429c0;  // int __thiscall World_PopInt(World*)
constexpr uintptr_t POP_STRING = 0x442a20;  // char** __thiscall World_PopString(World*)
constexpr uintptr_t POP_OBJECT = 0x442a40;  // int __thiscall World_PopObject(World*)
constexpr uintptr_t POP_VID    = 0x442ae0;  // Vid* __thiscall World_PopVid(World*, char*)
constexpr uintptr_t SCRIPT_POP_OBJECT = 0x442670;  // GameObject* __thiscall Script_PopObject(Script*)
constexpr uintptr_t LOG_ERROR  = 0x426140;  // Log_Error, "this variable is not unit"
constexpr uintptr_t LOG_WRITE  = 0x426080;  // Log_Write, "Invalid nvid %s %i"
constexpr uintptr_t G_LOG      = 0x4a9d5c;  // Log *g_log
constexpr uintptr_t G_EMPTY_VID = 0x5d3a60; // Vid *g_emptyVid
constexpr uintptr_t CATEGORY_LOGIC = 0x47ed70;         // "LOGIC"
constexpr uintptr_t TEXT_NOT_UNIT = 0x47f428;         // "this variable is not unit"
constexpr uintptr_t FORMAT_INVALID_NVID = 0x47f3ec;   // "Invalid nvid %s %i"

// The game's log; g_log is read every time, it is set when the game opens the log.
world::WorldLog game_log() {
    return {*reinterpret_cast<void**>(G_LOG),
            reinterpret_cast<world::LogErrorFn>(LOG_ERROR),
            reinterpret_cast<world::LogWriteFn>(LOG_WRITE),
            reinterpret_cast<const char*>(CATEGORY_LOGIC),
            reinterpret_cast<const char*>(TEXT_NOT_UNIT),
            reinterpret_cast<const char*>(FORMAT_INVALID_NVID)};
}

int __fastcall PopInt(void* ecx_world, void* /*edx*/) {
    return world::PopInt(static_cast<uint8_t*>(ecx_world));
}

char** __fastcall PopString(void* ecx_world, void* /*edx*/) {
    return world::PopString(static_cast<uint8_t*>(ecx_world), &sv::AssignText);
}

int __fastcall PopObject(void* ecx_world, void* /*edx*/) {
    return world::PopObject(static_cast<uint8_t*>(ecx_world), game_log());
}

void* __fastcall PopVid(void* ecx_world, void* /*edx*/, const char* for_text) {
    return world::PopVid(static_cast<uint8_t*>(ecx_world), game_log(), for_text,
                          *reinterpret_cast<void**>(G_EMPTY_VID));
}

int __fastcall PopObjectOfScript(void* ecx_script, void* /*edx*/) {
    return world::ScriptPopObject(static_cast<uint8_t*>(ecx_script), game_log());
}

}  // namespace

void install_world_script() {
    // push ecx / mov edx,[ecx+0x150] / dec edx
    const uint8_t pop_int_bytes[] = {0x51, 0x8B, 0x91, 0x50, 0x01, 0x00, 0x00};
    if (patch::redirect(POP_INT, pop_int_bytes, sizeof pop_int_bytes,
                        reinterpret_cast<void*>(&PopInt), "World_PopInt"))
        patch::log("World_PopInt rewritten @ %08X (\"0x\" returns 0)", static_cast<unsigned>(POP_INT));

    // mov eax,[ecx+0x150] / dec eax / mov [ecx+0x150],eax
    const uint8_t pop_string_bytes[] = {0x8B, 0x81, 0x50, 0x01, 0x00, 0x00, 0x48};
    if (patch::redirect(POP_STRING, pop_string_bytes, sizeof pop_string_bytes,
                        reinterpret_cast<void*>(&PopString), "World_PopString"))
        patch::log("World_PopString rewritten @ %08X", static_cast<unsigned>(POP_STRING));

    // push ecx / push esi / mov esi,ecx / mov eax,[esi+0x150]
    const uint8_t pop_object_bytes[] = {0x51, 0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x50, 0x01,
                                        0x00, 0x00};
    if (patch::redirect(POP_OBJECT, pop_object_bytes, sizeof pop_object_bytes,
                        reinterpret_cast<void*>(&PopObject), "World_PopObject"))
        patch::log("World_PopObject rewritten @ %08X", static_cast<unsigned>(POP_OBJECT));

    // push ecx / push esi / mov esi,ecx / mov edx,[esi+0x150]
    const uint8_t pop_vid_bytes[] = {0x51, 0x56, 0x8B, 0xF1, 0x8B, 0x96, 0x50, 0x01,
                                     0x00, 0x00};
    if (patch::redirect(POP_VID, pop_vid_bytes, sizeof pop_vid_bytes,
                        reinterpret_cast<void*>(&PopVid), "World_PopVid"))
        patch::log("World_PopVid rewritten @ %08X", static_cast<unsigned>(POP_VID));

    // push ecx / push esi / mov esi,ecx / mov eax,[esi+4]
    const uint8_t script_pop_object_bytes[] = {0x51, 0x56, 0x8B, 0xF1, 0x8B, 0x46, 0x04};
    if (patch::redirect(SCRIPT_POP_OBJECT, script_pop_object_bytes,
                        sizeof script_pop_object_bytes,
                        reinterpret_cast<void*>(&PopObjectOfScript), "Script_PopObject"))
        patch::log("Script_PopObject rewritten @ %08X",
                   static_cast<unsigned>(SCRIPT_POP_OBJECT));
}
