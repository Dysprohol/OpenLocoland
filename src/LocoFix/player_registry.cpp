// The player data of the scripts in HKEY_CURRENT_USER (BUGS.md S1).
//
// The scripts keep the players, their progress, scores, rank, sound settings and the
// menu's `return` flag with GetReg/SetReg (182/183) under "SOFTWARE\Gromada\steam\Players".
// With no hive prefix Registry_ParsePath (0x427340) picks HKEY_LOCAL_MACHINE, which a
// normal user may only read; the game relied on UAC registry virtualization to move the
// writes to the user's VirtualStore. On current Windows 11 that no longer happens: the
// RegCreateKeyExA of SetReg (0x436400) fails with ERROR_ACCESS_DENIED for every
// HKLM\SOFTWARE key and every access mask, although the token has virtualization allowed
// and enabled, and reads no longer see the VirtualStore - so nothing the scripts save
// survives.
//
// Registry_ParsePath is hooked: when it resolves to HKLM and the Players key (or a key
// under it), the hive becomes HKEY_CURRENT_USER and the path the Players key next to the
// game's own settings (RegPath in Locoland.cfg). At the first start with the fix that key
// does not exist yet and is filled from the old data: the values of the HKLM key, then
// those of the VirtualStore copy over them - the view the game had while virtualization
// worked - so the player keeps their progress.
#include "patch.h"

namespace {

constexpr uintptr_t REGISTRY_PARSE_PATH = 0x427340;
constexpr uintptr_t CRT_NEW = 0x456b11;
constexpr uintptr_t CRT_DELETE = 0x456a00;
constexpr uintptr_t EMPTY_STRING = 0x4a9d6c;   // the text of every empty String

constexpr char OLD_PATH[] = "SOFTWARE\\Gromada\\steam\\Players";
constexpr char NEW_PATH[] = "SOFTWARE\\Gromada\\Locoland\\Players";
// The VirtualStore copy of the HKLM key: a 32-bit process on 64-bit Windows, then 32-bit Windows.
const char* const VIRTUAL_STORE[] = {
    "Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\WOW6432Node\\Gromada\\steam\\Players",
    "Software\\Classes\\VirtualStore\\MACHINE\\SOFTWARE\\Gromada\\steam\\Players",
};

// A String of the game is a pointer to its text (crt_new, or EMPTY_STRING when empty).
using ParsePath_t = char**(__fastcall*)(char** path, void* edx, char** rest, HKEY* hive);
using CrtNew_t = void*(__cdecl*)(size_t size);
using CrtDelete_t = void(__cdecl*)(void* memory);
ParsePath_t g_original = nullptr;

char** __fastcall ParsePath_players(char** path, void* edx, char** rest, HKEY* hive) {
    g_original(path, edx, rest, hive);
    const size_t n = sizeof OLD_PATH - 1;
    const char* text = *rest;
    if (*hive != HKEY_LOCAL_MACHINE || _strnicmp(text, OLD_PATH, n) != 0 ||
        (text[n] != '\0' && text[n] != '\\'))
        return rest;
    const std::string moved = NEW_PATH + std::string(text + n);
    auto* copy = static_cast<char*>(reinterpret_cast<CrtNew_t>(CRT_NEW)(moved.size() + 1));
    std::memcpy(copy, moved.c_str(), moved.size() + 1);
    if (text != reinterpret_cast<const char*>(EMPTY_STRING))
        reinterpret_cast<CrtDelete_t>(CRT_DELETE)(*rest);
    *rest = copy;
    *hive = HKEY_CURRENT_USER;
    return rest;
}

// Copies every value of `from` into `to`; returns how many.
int copy_values(HKEY root, const char* from, HKEY to) {
    HKEY key;
    if (RegOpenKeyExA(root, from, 0, KEY_READ, &key) != ERROR_SUCCESS) return 0;
    int copied = 0;
    char name[256];
    BYTE data[1024];
    for (DWORD i = 0;; ++i) {
        DWORD name_size = sizeof name, data_size = sizeof data, type = 0;
        const LONG r = RegEnumValueA(key, i, name, &name_size, nullptr, &type, data, &data_size);
        if (r == ERROR_NO_MORE_ITEMS) break;
        if (r == ERROR_SUCCESS && RegSetValueExA(to, name, 0, type, data, data_size) == ERROR_SUCCESS)
            ++copied;
    }
    RegCloseKey(key);
    return copied;
}

void move_old_data() {
    HKEY key;
    DWORD disposition = 0;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, NEW_PATH, 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &key,
                        &disposition) != ERROR_SUCCESS) {
        patch::log("player data: can not create HKCU\\%s", NEW_PATH);
        return;
    }
    if (disposition == REG_CREATED_NEW_KEY) {
        const int machine = copy_values(HKEY_LOCAL_MACHINE, OLD_PATH, key);
        int store = 0;
        for (const char* path : VIRTUAL_STORE) store += copy_values(HKEY_CURRENT_USER, path, key);
        patch::log("player data: HKCU\\%s created, %d values from HKLM, %d from the VirtualStore",
                   NEW_PATH, machine, store);
    }
    RegCloseKey(key);
}

}  // namespace

void install_player_registry() {
    // push ecx / push esi / mov esi,ecx / push edi / mov edi,"HKEY_USERS\"
    const uint8_t prologue[] = {0x51, 0x56, 0x8B, 0xF1, 0x57, 0xBF, 0x2C, 0xE5, 0x47, 0x00};
    g_original = reinterpret_cast<ParsePath_t>(patch::hook(
        REGISTRY_PARSE_PATH, prologue, sizeof prologue, reinterpret_cast<void*>(&ParsePath_players),
        "Registry_ParsePath"));
    if (g_original) move_old_data();
}
