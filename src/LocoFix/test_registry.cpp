// Test mode keeps its settings apart: HKCU\SOFTWARE\Gromada\Locoland-test instead of the player's key.
//
// The game's settings (the start window's ScreenX, ScreenY, BPP, Device, FullScreen, the window's
// position; RegPath of Locoland.cfg), the scripts' player data (Players, moved there by
// player_registry.cpp) and LocoFix's own settings (Language, FrameLimit, FpsCounter, Cheats,
// UnlockMissions, UpdateCheckAtStart, UpdatePrereleases, MinimapSize, InterfaceScale, the editor's EditorHints, EditorScatter,
// EditorMusic, EditorProfile...) all live under HKCU\SOFTWARE\Gromada\Locoland. A scripted test run
// (LOCOFIX_TEST=1, tools/test_game.ps1) used the same key as the player's own game on the same
// computer, and the test scripts' "put the old values back" could undo what the player had just
// chosen in his start window.
//
// So in test mode every key the process opens or creates under HKCU\SOFTWARE\Gromada\Locoland
// (the key itself and every subkey) is opened under HKCU\SOFTWARE\Gromada\Locoland-test instead.
// The calls that take a key's path are RegOpenKeyExA, RegCreateKeyExA (the exe imports these two;
// its other registry calls take an open key) and RegGetValueA (restart.cpp); they are replaced in
// the exe's import table and in LocoFix.dll's own, so the engine and every file of LocoFix follow
// without a change of their own. The tables are found through the modules' import directories, not
// fixed addresses, so this runs before the exe is recognised: a test of another edition's exe, where
// LocoFix applies no fixes, keeps its settings apart too (an exe wrapped by a copy protection that
// resolves its imports itself may have none to replace - LocoFix.log says so). test_game.ps1 writes the test key's settings before each run.
// LOCOFIX_TEST_KEY=1 does the same without the rest of test mode (the real mouse, the game paused when
// it is not in front), for a scripted check that needs those. Otherwise nothing is replaced.
#include "patch.h"

namespace {

constexpr char REAL_KEY[] = "SOFTWARE\\Gromada\\Locoland";
constexpr char TEST_KEY[] = "SOFTWARE\\Gromada\\Locoland-test";

using OpenKey_t = LONG(WINAPI*)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY);
using CreateKey_t = LONG(WINAPI*)(HKEY, LPCSTR, DWORD, LPSTR, DWORD, REGSAM, const LPSECURITY_ATTRIBUTES, PHKEY, LPDWORD);
using GetValue_t = LONG(WINAPI*)(HKEY, LPCSTR, LPCSTR, DWORD, LPDWORD, PVOID, LPDWORD);

OpenKey_t g_open = nullptr;
CreateKey_t g_create = nullptr;
GetValue_t g_get = nullptr;
bool g_on = false;

// True, with the test key's path in `out`, for a path under the player's key.
bool moved(HKEY root, LPCSTR path, std::string& out) {
    if (root != HKEY_CURRENT_USER || !path) return false;
    const char* p = path;
    while (*p == '\\') ++p;
    const size_t n = sizeof REAL_KEY - 1;
    if (_strnicmp(p, REAL_KEY, n) != 0 || (p[n] != '\0' && p[n] != '\\')) return false;
    out = TEST_KEY + std::string(p + n);
    return true;
}

LONG WINAPI open_key(HKEY root, LPCSTR path, DWORD options, REGSAM access, PHKEY result) {
    std::string test;
    return g_open(root, moved(root, path, test) ? test.c_str() : path, options, access, result);
}

LONG WINAPI create_key(HKEY root, LPCSTR path, DWORD reserved, LPSTR cls, DWORD options, REGSAM access,
                       const LPSECURITY_ATTRIBUTES security, PHKEY result, LPDWORD disposition) {
    std::string test;
    return g_create(root, moved(root, path, test) ? test.c_str() : path, reserved, cls, options, access, security,
                    result, disposition);
}

LONG WINAPI get_value(HKEY root, LPCSTR path, LPCSTR value, DWORD flags, LPDWORD type, PVOID data, LPDWORD size) {
    std::string test;
    return g_get(root, moved(root, path, test) ? test.c_str() : path, value, flags, type, data, size);
}

// Replaces the named ADVAPI32 imports of a module; returns how many entries it replaced.
int replace_imports(HMODULE module, const char* const* names, void* const* targets, int count) {
    auto* base = reinterpret_cast<uint8_t*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return 0;
    int replaced = 0;
    for (auto* d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        if (_stricmp(reinterpret_cast<const char*>(base + d->Name), "ADVAPI32.dll") != 0 || !d->OriginalFirstThunk)
            continue;
        auto* lookup = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->OriginalFirstThunk);
        auto* iat = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; lookup->u1.AddressOfData; ++lookup, ++iat) {
            if (IMAGE_SNAP_BY_ORDINAL(lookup->u1.Ordinal)) continue;
            const char* name = reinterpret_cast<const char*>(
                reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + lookup->u1.AddressOfData)->Name);
            for (int i = 0; i < count; ++i)
                if (std::strcmp(name, names[i]) == 0) {
                    void* target = targets[i];
                    patch::write(reinterpret_cast<uintptr_t>(&iat->u1.Function), &target, sizeof target);
                    ++replaced;
                }
        }
    }
    return replaced;
}

}  // namespace

bool test_registry_on() { return g_on; }

// First thing in test mode, before anything reads a setting.
void install_test_registry() {
    // LOCOFIX_TEST_KEY=1: the test key alone, without the rest of test mode - for a run with the real
    // mouse and focus (a full-screen check by a script) that must not touch the player's settings either
    char test[8] = {}, key_only[8] = {};
    const bool on = (GetEnvironmentVariableA("LOCOFIX_TEST", test, sizeof test) && test[0] == '1') ||
                    (GetEnvironmentVariableA("LOCOFIX_TEST_KEY", key_only, sizeof key_only) && key_only[0] == '1');
    if (!on) return;
    HMODULE advapi = GetModuleHandleA("advapi32.dll");
    if (!advapi) advapi = LoadLibraryA("advapi32.dll");
    g_open = reinterpret_cast<OpenKey_t>(GetProcAddress(advapi, "RegOpenKeyExA"));
    g_create = reinterpret_cast<CreateKey_t>(GetProcAddress(advapi, "RegCreateKeyExA"));
    g_get = reinterpret_cast<GetValue_t>(GetProcAddress(advapi, "RegGetValueA"));
    if (!g_open || !g_create || !g_get) {
        patch::log("test mode: registry functions not found - settings NOT kept apart");
        return;
    }
    const char* const names[] = {"RegOpenKeyExA", "RegCreateKeyExA", "RegGetValueA"};
    void* const targets[] = {reinterpret_cast<void*>(&open_key), reinterpret_cast<void*>(&create_key),
                             reinterpret_cast<void*>(&get_value)};
    HMODULE self = nullptr;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&install_test_registry), &self);
    const int exe = replace_imports(GetModuleHandleA(nullptr), names, targets, 3);
    const int own = self ? replace_imports(self, names, targets, 3) : 0;
    g_on = exe > 0;
    if (g_on)
        patch::log("test mode: settings in HKCU\\%s, not HKCU\\%s (%d imports of the exe, %d of LocoFix)", TEST_KEY,
                   REAL_KEY, exe, own);
    else
        patch::log("test mode: the exe's registry imports not found - its settings NOT kept apart");
}
