// Native implementation of the World helpers that read and write the script stack.
//
// The script stack lives in the World: the count at +0x150 and the array of 12-byte
// script values at +0x158 (sv::Value, see script_value.h). The pop helpers take the
// top value off the stack and convert it:
//
//   World_PopInt (0x4429c0)     the value as an int (sv::ToInt)
//   World_PopString (0x442a20)  the value as a String (sv::ToString), returns it
//   World_PopObject (0x442a40)  the value as an int; a value that is not an object is
//                                reported in the log
//   World_PopVid (0x442ae0)     the VID of the number; a number that is not loaded is
//                                reported in the log and gives the empty VID
//
// Script_PopObject (0x442670) does the same as World_PopObject, but on the stack of the
// script machine (the World keeps its script at +0x14c).
//
// The sscanf path of the original returns whatever the output slot held when "%i" does
// not convert: the World pointer (World_PopInt, World_PopObject), the script machine
// (Script_PopObject) or the text of the caller (World_PopVid). The native versions
// return 0 instead, like the String_ToInt rewrite (BUGS.md F9).
#pragma once
#include <cstddef>
#include <cstdint>

#include "script_value.h"

namespace world {

constexpr std::size_t SCRIPT_COUNT = 0x150;   // int, values on the script stack
constexpr std::size_t SCRIPT_VALUES = 0x158;  // sv::Value*, the stack itself
constexpr std::size_t VID_COUNT = 0x290;      // int, number of loaded VIDs
constexpr std::size_t VIDS = 0x294;           // void**, the VIDs by number

// The script machine of the World is at World+0x14c: its stack is the count at +0x04
// and the values at +0x0c - the same fields as the World's own at +0x150 and +0x158.
constexpr std::size_t SCRIPT_MACHINE = 0x14c;
constexpr std::size_t MACHINE_COUNT = 0x04;
constexpr std::size_t MACHINE_VALUES = 0x0c;

// The game's log: Log_Error (0x426140) writes an error with a code and a category to
// error.log, Log_Write (0x426080) writes a line with a format.
using LogErrorFn = void (__cdecl*)(void* log, const char* category, int code,
                                   const char* text, int value);
using LogWriteFn = void (__cdecl*)(void* log, const char* format, ...);

// What the World helpers need from the game to report errors.
struct WorldLog {
    void* log;                   // Log *g_log
    LogErrorFn error;            // Log_Error
    LogWriteFn write;            // Log_Write
    const char* category;        // "LOGIC" - the category of a script error
    const char* not_unit;        // "this variable is not unit"
    const char* invalid_nvid;    // "Invalid nvid %s %i"
};

inline int32_t* script_count(uint8_t* world) {
    return reinterpret_cast<int32_t*>(world + SCRIPT_COUNT);
}

inline sv::Value* script_values(uint8_t* world) {
    return reinterpret_cast<sv::Value*>(*reinterpret_cast<uint8_t**>(world + SCRIPT_VALUES));
}

// Takes the top value off the script stack (the caller reads it before the next pop).
inline sv::Value* PopValue(uint8_t* world) {
    int32_t count = --*script_count(world);
    return script_values(world) + count;
}

inline int32_t PopInt(uint8_t* world) {
    return sv::ToInt(*PopValue(world));
}

inline char** PopString(uint8_t* world, sv::AssignFn assign) {
    return sv::ToString(PopValue(world), assign);
}

// The VID of a number; null when the number is not loaded.
inline void* vid_at(uint8_t* world, int32_t nvid) {
    const int32_t count = *reinterpret_cast<int32_t*>(world + VID_COUNT);
    if (nvid < 0 || nvid >= count) return nullptr;
    return reinterpret_cast<void**>(world + VIDS)[nvid];
}

// World_PopObject: pops a value and returns it as an int. A value that is not an object
// (and not 0) is reported in the log.
inline int32_t PopObject(uint8_t* world, const WorldLog& log) {
    const sv::Value& top = script_values(world)[*script_count(world) - 1];
    if (top.value != 0 && !(top.flags & 0x10)) {
        log.error(log.log, log.category, 10, log.not_unit, 0);
    }
    return sv::ToInt(*PopValue(world));
}

// World_PopVid: pops an nvid and returns its VID. A number that is not loaded is
// reported in the log and gives the empty VID.
inline void* PopVid(uint8_t* world, const WorldLog& log, const char* for_text,
                    void* empty_vid) {
    int32_t nvid = 0;
    const bool converted = sv::ToNumber(*PopValue(world), &nvid);
    void* vid = converted ? vid_at(world, nvid) : nullptr;
    if (!vid) vid = empty_vid;
    if (vid == empty_vid) {
        log.write(log.log, log.invalid_nvid, for_text, nvid);
    }
    return vid;
}

// The stack of a script machine (the World keeps its script at +0x14c).
inline int32_t* machine_count(uint8_t* machine) {
    return reinterpret_cast<int32_t*>(machine + MACHINE_COUNT);
}

inline sv::Value* machine_values(uint8_t* machine) {
    return reinterpret_cast<sv::Value*>(*reinterpret_cast<uint8_t**>(machine + MACHINE_VALUES));
}

// Script_PopObject: pops a value of the script stack and returns it as an int. A value
// that is not an object (and not 0) is reported in the log.
inline int32_t ScriptPopObject(uint8_t* machine, const WorldLog& log) {
    const sv::Value& top = machine_values(machine)[*machine_count(machine) - 1];
    if (top.value != 0 && !(top.flags & 0x10)) {
        log.error(log.log, log.category, 10, log.not_unit, 0);
    }
    const int32_t count = --*machine_count(machine);
    return sv::ToInt(machine_values(machine)[count]);
}

}  // namespace world
