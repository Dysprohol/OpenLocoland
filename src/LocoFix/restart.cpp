// Restarting the game: into the editor mode from the menu and back from the editor.
//
// The editor mode is fixed for a run (editor.cpp: the window class gets its menu and the decals
// are converted while objects.res loads), so the menu's "EDYTOR MAP" cannot switch it on in a
// running game. A script function of ours restarts the game instead:
//
//   extern StartEditorMode(string map) 200;
//
// (200 is free in maps\script\export.lgc; the menu declares it in menu.lgc, game/maps/
// menu.lgc.patch). Script_CallExtern is hooked for that number: the game starts again as
// `Locoland.exe --editor --test-map <map> --no-launcher --after <pid>` and this one closes
// (WM_CLOSE, so it exits normally and saves its settings). In editor mode already, the map is
// just loaded. The editor's File > Play the map and Back to the menu (editor.cpp) restart the
// other way, without --editor.
//
// --after <pid>: the new game waits (at most 30 s) for the old one to end before anything else,
// so it reads the window position the old one saved and two games never share the folder's logs.
// --no-launcher: START_DIALOG is not shown - the game has read every setting from the registry
// before it would show it (World_Construct; the launcher only shows them and saves the changes),
// and the cfg's StartDialog=0 would do the same.
// --return-editor (File > Play the map): when the played map is left for the main menu (the end
// of the mission, "exit to the main menu" of the Esc menu), the game starts again in editor mode
// on that map instead (restart_level_loading, from frame_limit.cpp's Level_Load wrapper); the Esc
// menu's exit page also has BACK TO THE EDITOR (menutact.lgc.patch), which does it at once:
//
//   extern PlayedFromEditor() 202;   // 1 in a map played from the editor, 2 while the level runs
//                                    // inside the editor's own game (Run the level)
//   extern BackToEditor() 203;       // the game starts again in editor mode on it (2: the editor
//                                    // holds the level again)
//
// Either way the menu's `return` flag (Players key: 2 after a custom map, so the main menu opens on
// the custom maps page; the menu sets it back to 1 when it starts) gets the value it had before the
// map was played - the menu does not start on the way back, and the next start of the game would
// open the custom maps page.
// The switches are taken out of the command line (patch::take_switch).
#include "patch.h"
#include "script_value.h"

#include <cstring>

void editor_load_map(const std::string& map);   // editor.cpp
std::string editor_start_map(const std::string& map);   // editor.cpp: the template -> a new empty map
bool editor_mode_on();                            // editor.cpp
bool editor_level_running();                      // editor.cpp: Run the level is on in the editor's game
void editor_hold_level();                         // editor.cpp: ... and the level is held again
int recalc_grid_z_extern(uint8_t* world);         // ground.cpp
int mouse_wheel_extern(uint8_t* world);           // menu_input.cpp
bool restart_game(bool editor, const std::string& map, bool back_to_editor = false);

namespace {

constexpr uintptr_t SCRIPT_CALL_EXTERN = 0x43e5c0;
constexpr uintptr_t IAT_DialogBoxParamA = 0x4751ac;
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr size_t WORLD_HWND = 0x214;
constexpr size_t WORLD_SCRIPT_COUNT = 0x150;
constexpr size_t WORLD_SCRIPT_VALUES = 0x158;
constexpr int EXTERN_START_EDITOR_MODE = 200;
constexpr int EXTERN_RECALC_GRID_Z = 81;      // ReCalcGridZ(int unit): declared in export.lgc, missing in the exe
constexpr int EXTERN_MOUSE_WHEEL = 201;       // GetMouseWheel(): the menu's custom maps page
constexpr int EXTERN_PLAYED_FROM_EDITOR = 202;   // PlayedFromEditor(): the Esc menu (menutact.lgc.patch)
constexpr int EXTERN_BACK_TO_EDITOR = 203;       // BackToEditor()
constexpr uintptr_t SCRIPT_PUSH_INT = 0x442700;  // void __thiscall Script_PushInt(Script*, int)
constexpr size_t WORLD_SCRIPT = 0x14c;
// the scripts' Players key (player_registry.cpp moves it to HKEY_CURRENT_USER) and the menu's flag
constexpr char PLAYERS_KEY[] = "SOFTWARE\\Gromada\\Locoland\\Players";
constexpr char RETURN_VALUE[] = "return";
constexpr DWORD WAIT_FOR_OLD_GAME_MS = 30000;

using CallExtern_t = int(__fastcall*)(uint8_t* world, void* edx, int function);
using PushInt_t = void(__fastcall*)(uint8_t* script, void* edx, int value);
using DialogBoxParamA_t = INT_PTR(WINAPI*)(HINSTANCE, LPCSTR, HWND, DLGPROC, LPARAM);
CallExtern_t g_call_extern = nullptr;
DialogBoxParamA_t g_dialog_box = nullptr;
bool g_no_launcher = false;
bool g_return_editor = false;   // --return-editor
std::string g_played;           // the map played, the first one loaded
std::string g_return_flag;      // the menu's `return` before the map was played ("" when there was none)
bool g_return_flag_known = false;

// The menu's `return` flag of the Players key: read before the played map sets it, put back on the
// way to the editor.
bool read_return_flag(std::string& value) {
    char data[32] = "";
    DWORD size = sizeof data - 1, type = 0;
    const bool there = RegGetValueA(HKEY_CURRENT_USER, PLAYERS_KEY, RETURN_VALUE, RRF_RT_REG_SZ, &type, data, &size) == ERROR_SUCCESS;
    value = there ? data : "";
    return there;
}

void restore_return_flag() {
    if (!g_return_flag_known) return;
    HKEY key = nullptr;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, PLAYERS_KEY, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return;
    if (g_return_flag.empty()) RegDeleteValueA(key, RETURN_VALUE);
    else RegSetValueExA(key, RETURN_VALUE, 0, REG_SZ, reinterpret_cast<const BYTE*>(g_return_flag.c_str()),
                        static_cast<DWORD>(g_return_flag.size() + 1));
    RegCloseKey(key);
    patch::log("restart: the menu's return flag back to '%s'", g_return_flag.c_str());
}

// The played map is left for the editor: the game starts again in editor mode on it.
void back_to_editor() {
    if (!g_return_editor || g_played.empty()) return;
    patch::log("restart: back to the editor on %s", g_played.c_str());
    g_return_editor = false;
    restore_return_flag();
    restart_game(true, g_played);
}
std::string g_waited;   // "<pid> (n ms)" when --after waited, logged once the log is fresh

INT_PTR WINAPI DialogBoxParamA_restart(HINSTANCE instance, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param) {
    if (g_no_launcher && !IS_INTRESOURCE(name) && lstrcmpiA(name, "START_DIALOG") == 0) {
        patch::log("restart: the launcher is skipped, the settings of the registry are used");
        return 1;   // OK
    }
    return g_dialog_box(instance, name, parent, proc, param);
}

// StartEditorMode(string map): pops the map, pushes nothing.
int __fastcall CallExtern_restart(uint8_t* world, void* edx, int function) {
    if (function == EXTERN_RECALC_GRID_Z) return recalc_grid_z_extern(world);
    if (function == EXTERN_MOUSE_WHEEL) return mouse_wheel_extern(world);
    if (function == EXTERN_PLAYED_FROM_EDITOR) {
        const int how = g_return_editor && !g_played.empty() ? 1 : editor_level_running() ? 2 : 0;
        reinterpret_cast<PushInt_t>(SCRIPT_PUSH_INT)(world + WORLD_SCRIPT, nullptr, how);
        return 0;
    }
    if (function == EXTERN_BACK_TO_EDITOR) {
        if (editor_level_running()) editor_hold_level();   // the menu has gone (MenuUnloading)
        else back_to_editor();
        return 0;
    }
    if (function != EXTERN_START_EDITOR_MODE) return g_call_extern(world, edx, function);
    int32_t& count = *reinterpret_cast<int32_t*>(world + WORLD_SCRIPT_COUNT);
    auto* values = *reinterpret_cast<sv::Value**>(world + WORLD_SCRIPT_VALUES);
    std::string map;
    if (count > 0) {
        const sv::Value& top = values[--count];
        if ((top.flags & 1) && top.text) map = top.text;
    }
    if (editor_mode_on()) editor_load_map(editor_start_map(map));
    else restart_game(true, map);
    return 0;
}

}  // namespace

// Starts the game again - in editor mode or not, on a map or (empty) at the main menu - and
// closes this one. Returns false when the new game could not be started.
bool restart_game(bool editor, const std::string& map, bool back_to_editor) {
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string command = std::string("\"") + exe + "\"";
    if (editor) command += " --editor";
    if (!map.empty()) command += " --test-map \"" + map + "\"";
    if (back_to_editor) command += " --return-editor";
    command += " --no-launcher --after " + std::to_string(GetCurrentProcessId());
    STARTUPINFOA startup = {sizeof startup};
    PROCESS_INFORMATION process = {};
    std::string line = command;
    if (!CreateProcessA(nullptr, &line[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process)) {
        patch::log("restart: %s could not be started (error %lu)", command.c_str(), GetLastError());
        return false;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    patch::log("restart: %s", command.c_str());
    auto* world = *reinterpret_cast<uint8_t**>(G_WORLD);
    if (HWND window = world ? *reinterpret_cast<HWND*>(world + WORLD_HWND) : nullptr)
        PostMessageA(window, WM_CLOSE, 0, 0);
    return true;
}

// The first thing DllMain does: with --after <pid>, wait for that game to end.
void restart_wait() {
    std::string pid;
    if (!patch::take_switch("--after", nullptr, &pid)) return;
    const DWORD id = std::strtoul(pid.c_str(), nullptr, 10);
    HANDLE old = id ? OpenProcess(SYNCHRONIZE, FALSE, id) : nullptr;
    const DWORD start = GetTickCount();
    if (old) {
        WaitForSingleObject(old, WAIT_FOR_OLD_GAME_MS);
        CloseHandle(old);
    }
    g_waited = pid + " (" + std::to_string(GetTickCount() - start) + " ms)";
}

// After options.cpp, which took the DialogBoxParamA entry before us.
void install_restart() {
    if (!g_waited.empty()) patch::log("restart: waited for the old game %s", g_waited.c_str());
    g_no_launcher = patch::take_switch("--no-launcher");
    g_return_editor = patch::take_switch("--return-editor");
    if (g_return_editor) g_return_flag_known = true, read_return_flag(g_return_flag);   // before the map's script sets it
    g_dialog_box = *reinterpret_cast<DialogBoxParamA_t*>(IAT_DialogBoxParamA);
    void* dialog_box = reinterpret_cast<void*>(&DialogBoxParamA_restart);
    patch::write(IAT_DialogBoxParamA, &dialog_box, sizeof dialog_box);

    // mov eax,fs:[0] / push -1
    const uint8_t call_extern[] = {0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xFF};
    g_call_extern = reinterpret_cast<CallExtern_t>(patch::hook(
        SCRIPT_CALL_EXTERN, call_extern, sizeof call_extern, reinterpret_cast<void*>(&CallExtern_restart),
        "Script_CallExtern"));
}

// frame_limit.cpp's Level_Load wrapper, before a level loads: with --return-editor, the main
// menu after the played map restarts the game in editor mode on that map.
void restart_level_loading(const char* map) {
    if (!g_return_editor || !map) return;
    if (g_played.empty()) {
        g_played = map;
        return;
    }
    const size_t n = std::strlen(map);
    if (n >= 8 && lstrcmpiA(map + n - 8, "menu.map") == 0) back_to_editor();
}