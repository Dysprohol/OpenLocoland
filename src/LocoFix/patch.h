// Helpers for patching the game code in memory.
//
// Every patch checks the expected bytes first. If they don't match (a different exe
// version), the patch is skipped and logged instead of breaking the code.
#pragma once
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <string>

// The OpenLocoland version, given by tools/release.py (git describe); "dev" for a build of build.py.
#ifndef LOCOFIX_VERSION
#define LOCOFIX_VERSION "dev"
#endif

namespace patch {

void log(const char* format, ...);

// Folder of the exe with a trailing backslash.
std::string game_folder();

// One command line token, quoted or not, and the position moved past it.
inline std::string read_token(const char*& at) {
    while (*at == ' ' || *at == '\t') ++at;
    std::string token;
    if (*at == '"') {
        for (++at; *at && *at != '"'; ++at) token += *at;
        if (*at == '"') ++at;
    } else {
        while (*at && *at != ' ' && *at != '\t') token += *at++;
    }
    return token;
}

// Takes a switch (`name`, or `alias`) out of the process command line, with the token after it
// when `value` is given (stored there). WinMain's lpCmdLine points into that buffer and
// World_Construct opens a command line without ".cfg" in it as the map to start with, so a
// switch of ours left there would be opened as a map. Returns whether the switch was there.
inline bool take_switch(const char* name, const char* alias = nullptr, std::string* value = nullptr) {
    char* line = GetCommandLineA();
    bool found = false;
    const char* at = line;
    while (*at) {
        while (*at == ' ' || *at == '\t') ++at;
        char* start = line + (at - line);
        const std::string token = read_token(at);
        if (token != name && !(alias && token == alias)) continue;
        found = true;
        if (value) *value = read_token(at);
        while (*at == ' ' || *at == '\t') ++at;
        std::memmove(start, at, std::strlen(at) + 1);
        at = start;
    }
    return found;
}

inline bool check(uintptr_t address, const void* expected, size_t size) {
    return std::memcmp(reinterpret_cast<const void*>(address), expected, size) == 0;
}

inline void write(uintptr_t address, const void* bytes, size_t size) {
    DWORD old;
    VirtualProtect(reinterpret_cast<void*>(address), size, PAGE_EXECUTE_READWRITE, &old);
    std::memcpy(reinterpret_cast<void*>(address), bytes, size);
    VirtualProtect(reinterpret_cast<void*>(address), size, old, &old);
    FlushInstructionCache(GetCurrentProcess(), reinterpret_cast<void*>(address), size);
}

// Replaces `call [iat]` (FF 15 xx xx xx xx) with `call target` + nop.
inline bool redirect_iat_call(uintptr_t address, uintptr_t iat, void* target, const char* name) {
    uint8_t expected[6] = {0xFF, 0x15};
    std::memcpy(expected + 2, &iat, 4);
    if (!check(address, expected, sizeof expected)) {
        log("skipped %s @ %08X: unexpected bytes", name, static_cast<unsigned>(address));
        return false;
    }
    uint8_t code[6] = {0xE8};
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(target) - (address + 5));
    std::memcpy(code + 1, &rel, 4);
    code[5] = 0x90;
    write(address, code, sizeof code);
    return true;
}

// Hooks a game function: its first bytes (`expected`, whole instructions with no relative
// jumps) go into a trampoline that ends with a jump to the rest of the original, and
// a jump to `hook` replaces them. Returns the trampoline (a way to call the original)
// or nullptr when the bytes don't match.
inline void* hook(uintptr_t address, const uint8_t* expected, size_t size, void* hook_fn, const char* name) {
    if (size < 5 || !check(address, expected, size)) {
        log("skipped hook %s @ %08X: unexpected bytes", name, static_cast<unsigned>(address));
        return nullptr;
    }
    auto* tramp = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, size + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp) return nullptr;
    std::memcpy(tramp, expected, size);
    tramp[size] = 0xE9;
    int32_t back = static_cast<int32_t>((address + size) - (reinterpret_cast<uintptr_t>(tramp) + size + 5));
    std::memcpy(tramp + size + 1, &back, 4);

    uint8_t jump[16];
    std::memset(jump, 0x90, sizeof jump);
    jump[0] = 0xE9;
    int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(hook_fn) - (address + 5));
    std::memcpy(jump + 1, &rel, 4);
    write(address, jump, size);
    return tramp;
}

// Replaces a whole function with `replacement`: `expected` are the first whole
// instructions of the game function and `size` is their size (at least 5). The rest of
// the overwritten bytes become nops. The replacement must use the same calling
// convention and return the way the original did (for example `ret 8` for __thiscall
// with two stack arguments).
inline bool redirect(uintptr_t address, const uint8_t* expected, size_t size, void* replacement,
                     const char* name) {
    if (size < 5 || size > 16 || !check(address, expected, size)) {
        log("skipped rewrite %s @ %08X: unexpected bytes", name, static_cast<unsigned>(address));
        return false;
    }
    uint8_t code[16];
    std::memset(code, 0x90, sizeof code);
    code[0] = 0xE9;
    const int32_t rel =
        static_cast<int32_t>(reinterpret_cast<uintptr_t>(replacement) - (address + 5));
    std::memcpy(code + 1, &rel, 4);
    write(address, code, size);
    return true;
}

// Writes `prefix` followed by `call target` (or `jmp target`) at `address`, padding
// the rest of `size` bytes with nop.
inline void write_call(uintptr_t address, const uint8_t* prefix, size_t prefix_size, void* target,
                       size_t size, bool jump = false) {
    uint8_t code[16];
    std::memset(code, 0x90, sizeof code);
    if (prefix_size) std::memcpy(code, prefix, prefix_size);
    const uintptr_t instr = address + prefix_size;
    code[prefix_size] = jump ? 0xE9 : 0xE8;
    const int32_t rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(target) - (instr + 5));
    std::memcpy(code + prefix_size + 1, &rel, 4);
    write(address, code, size);
}

}  // namespace patch
