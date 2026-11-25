// The map's script compiled before the editor runs or plays the map.
//
// The engine compiles a level's script (<map>.lgc and its #includes) as the level loads:
// Level_Load -> Script_Load -> Script_Compile, a one-pass compiler over the whole file. A compile
// error goes to error.log (Script_CompileError, 0x438f70: "LOGIC '<file>' line <n>: <the error>"
// and the source around it), and almost every one then ends the game where it stands - the compiler
// calls the CRT's exit(1) from deep in its recursive descent (an undeclared name, a missing ';', a
// bad #include ...); a missing file is only logged, and the level runs with no script. So a map of
// the editor with a slip in its script closed the game at Run the level (Ctrl+R) or Play the map,
// the editor with it, and Run the level's edits that were not saved were lost.
//
// The editor now compiles the script first, with the engine's own compiler (Script_Load, as
// Level_Load calls it) into a script machine of its own - not the World's, so the level held in
// the editor is not touched - on a thread of its own: the hook of the CRT's exit (crt_exit,
// 0x456f85) ends only that thread when the compiler gives up, and the files the compiler had open
// on it are closed (the hooks of crt_fopen and crt_fclose keep the list). The engine's code runs on
// the game's main thread only (its CRT is the single-threaded one: no locks), and the main thread
// waits for the check, so nothing else runs the engine meanwhile. The first error the compiler
// reports is kept, as error.log gets it, with the 60 characters of source the log shows; the
// error.log of the game has the lines as before. editor.cpp says it and stays on the map.
//
// The editor's own load of a map compiles its script too (Level_Load, although a held level runs
// none of it), so a map with such a script closed the editor as it was opened - File > Load, the
// editor's start on it, the way back from Play the map. In editor mode a level loaded to be held is
// checked first (the hook of Script_Load): when it does not compile, the World's script is released
// and the level is held without it, as with a missing file; editor.cpp says why in the status line.
// A level that runs (Run the level) gets the engine's compile alone - it was checked before it ran.
#include "patch.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "script_check.h"

bool editor_mode_on();         // editor.cpp
bool editor_level_running();   // editor.cpp: Run the level is on

namespace {

constexpr uintptr_t CRT_EXIT = 0x456f85;              // void exit(int)
constexpr uintptr_t CRT_FOPEN = 0x457a86;             // FILE* fopen(const char*, const char*)
constexpr uintptr_t CRT_FCLOSE = 0x457984;            // int fclose(FILE*)
constexpr uintptr_t SCRIPT_COMPILE_ERROR = 0x438f70;  // void __thiscall Script_CompileError(Script*, int code, char* text, int value)
constexpr uintptr_t SCRIPT_LOAD = 0x438ca0;           // int __thiscall Script_Load(Script*, char** file)
constexpr uintptr_t SCRIPT_DESTRUCT = 0x422ec0;       // void __thiscall Script_Destruct(Script*)
constexpr uintptr_t SCRIPT_RELEASE = 0x438810;        // void __thiscall Script_Release(Script*)
// what World_Construct puts in the World's script machine (re/types.txt struct Script)
constexpr uintptr_t VTABLE_VALUES = 0x475520;
constexpr uintptr_t VTABLE_SYMBOLS = 0x4756ec;
constexpr uintptr_t VTABLE_DEFINES = 0x4756f0;
constexpr uintptr_t EMPTY_STRING = 0x4a9d6c;
constexpr size_t SCRIPT_SIZE = 0x58;
constexpr size_t SCRIPT_SYMBOLS = 0x10, SCRIPT_DEFINES = 0x20, SCRIPT_NAME = 0x30, SCRIPT_SOURCE = 0x3c,
                 SCRIPT_LINE = 0x48, SCRIPT_MAIN = 0x50;
constexpr char LOGIC_LINE[] = "LOGIC '%s' line %i";   // 0x47ed78, the start of the line
// Log_Error's texts by code (0x426140): the error's value and text go into them
constexpr uintptr_t LOG_CATEGORY[] = {0x47c050, 0x47c068, 0x47c028, 0x47bffc, 0x47bfec, 0x47bf6c, 0x47bf5c, 0x47bf7c,
                                      0x47bfb0, 0x47bf94, 0x47bf54, 0x47bfcc, 0x47bf38, 0x47c018, 0x47bf28};
constexpr DWORD STOPPED = 0x5c;              // the check thread's exit code when the compiler gave up
constexpr SIZE_T CHECK_STACK = 4 << 20;      // the main thread has 1 MB (the exe's header)

using Exit_t = void(__cdecl*)(int);
using Fopen_t = void*(__cdecl*)(const char*, const char*);
using Fclose_t = int(__cdecl*)(void*);
using CompileError_t = void(__fastcall*)(uint8_t* script, void* edx, int code, const char* text, int value);
using Load_t = int(__fastcall*)(uint8_t* script, void* edx, char** file);
using Destruct_t = void(__fastcall*)(uint8_t* script, void* edx);

Exit_t g_exit = nullptr;
Fopen_t g_fopen = nullptr;
Fclose_t g_fclose = nullptr;
CompileError_t g_compile_error = nullptr;
Load_t g_load = nullptr;
ScriptCheck g_held;   // the editor's load held its level without the script: why

// the check: its thread, its script machine, the files open on it, the first error
DWORD g_thread = 0;
alignas(8) uint8_t g_script[SCRIPT_SIZE];
std::vector<void*> g_open;
bool g_failed = false;
std::string g_line, g_near;

bool on_check() { return g_thread && GetCurrentThreadId() == g_thread; }

void __cdecl exit_check(int code) {
    if (on_check()) ExitThread(STOPPED);   // the compiler gave up: only the check ends
    g_exit(code);
}

void* __cdecl fopen_check(const char* name, const char* mode) {
    void* file = g_fopen(name, mode);
    if (file && on_check()) g_open.push_back(file);
    return file;
}

int __cdecl fclose_check(void* file) {
    if (on_check())
        for (size_t i = 0; i < g_open.size(); ++i)
            if (g_open[i] == file) {
                g_open.erase(g_open.begin() + static_cast<std::ptrdiff_t>(i));
                break;
            }
    return g_fclose(file);
}

// The first error of the check, as Script_CompileError writes it to error.log; then the original
// writes it there.
void __fastcall compile_error_check(uint8_t* script, void* edx, int code, const char* text, int value) {
    if (script == g_script && on_check() && !g_failed) {
        g_failed = true;
        char buffer[1100];
        const char* name = *reinterpret_cast<const char**>(script + SCRIPT_NAME);
        std::snprintf(buffer, sizeof buffer, LOGIC_LINE, name ? name : "", *reinterpret_cast<int*>(script + SCRIPT_LINE) + 1);
        g_line = buffer;
        if (code >= 0 && code < static_cast<int>(sizeof LOG_CATEGORY / sizeof LOG_CATEGORY[0])) {
            std::snprintf(buffer, sizeof buffer, reinterpret_cast<const char*>(LOG_CATEGORY[code]), value, text ? text : "");
            g_line += std::string(": ") + buffer;
        }
        // the 60 characters around the error the log shows (the compiler's read position in the
        // middle), new lines and tabs as '?' there - here as spaces
        g_near.clear();
        if (const char* source = *reinterpret_cast<const char**>(script + SCRIPT_SOURCE)) {
            std::string around(source - 30, 60);
            for (char& c : around)
                if (c == '\n' || c == '\r' || c == '\t' || c == '\0') c = ' ';
            g_near = around;
        }
    }
    g_compile_error(script, edx, code, text, value);
}

DWORD WINAPI compile_thread(void* file) {
    char* name = static_cast<char*>(file);
    return g_load(g_script, nullptr, &name) == 0 ? 0 : 1;
}

ScriptCheck check_file(const std::string& lgc);

// Level_Load's Script_Load: in editor mode a level to be held gets its script only when it compiles.
int __fastcall load_check(uint8_t* script, void* edx, char** file) {
    if (script != g_script && !on_check() && editor_mode_on() && !editor_level_running() && file && *file && **file) {
        const ScriptCheck check = check_file(*file);
        if (!check.ok && !check.missing) {
            reinterpret_cast<Destruct_t>(SCRIPT_RELEASE)(script, nullptr);   // the same __thiscall shape
            g_held = check;
            patch::log("script check: the level is held without its script %s", *file);
            return 1;   // as for a missing file: no script
        }
    }
    return g_load(script, edx, file);
}

ScriptCheck check_file(const std::string& lgc) {
    ScriptCheck result;
    result.file = lgc;
    if (GetFileAttributesA(lgc.c_str()) == INVALID_FILE_ATTRIBUTES) {
        result.ok = false;
        result.missing = true;
        patch::log("script check: %s is missing", lgc.c_str());
        return result;
    }
    if (!g_exit || !g_fopen || !g_fclose || !g_compile_error || !g_load) return result;   // the hooks are not there: not checked
    std::memset(g_script, 0, sizeof g_script);
    *reinterpret_cast<uintptr_t*>(g_script) = VTABLE_VALUES;
    *reinterpret_cast<uintptr_t*>(g_script + SCRIPT_SYMBOLS) = VTABLE_SYMBOLS;
    *reinterpret_cast<uintptr_t*>(g_script + SCRIPT_DEFINES) = VTABLE_DEFINES;
    *reinterpret_cast<uintptr_t*>(g_script + SCRIPT_NAME) = EMPTY_STRING;
    *reinterpret_cast<int*>(g_script + SCRIPT_LINE) = -1;
    *reinterpret_cast<int*>(g_script + SCRIPT_MAIN) = -1;
    g_open.clear();
    g_failed = false;
    g_line.clear();
    g_near.clear();
    std::string name = lgc;
    DWORD id = 0;
    HANDLE thread = CreateThread(nullptr, CHECK_STACK, compile_thread, &name[0], CREATE_SUSPENDED | STACK_SIZE_PARAM_IS_A_RESERVATION, &id);
    if (!thread) {
        patch::log("script check: no thread for it (error %lu), %s not checked", GetLastError(), lgc.c_str());
        return result;
    }
    g_thread = id;
    ResumeThread(thread);
    WaitForSingleObject(thread, INFINITE);
    DWORD code = 0;
    GetExitCodeThread(thread, &code);
    CloseHandle(thread);
    g_thread = 0;
    // a compiler that gave up left its files open (Script_Load's, Script_Compile's, an #include's):
    // closed, so the script can be written or deleted and its folder removed
    for (void* file : g_open) g_fclose(file);
    const size_t left_open = g_open.size();
    g_open.clear();
    reinterpret_cast<Destruct_t>(SCRIPT_DESTRUCT)(g_script, nullptr);
    result.ok = code == 0 && !g_failed;
    result.line = g_line;
    result.near_text = g_near;
    if (result.ok)
        patch::log("script check: %s compiles", lgc.c_str());
    else
        patch::log("script check: %s does not compile (%s, %zu files closed after it) - %s | %s", lgc.c_str(),
                   code == STOPPED ? "the compiler gave up" : "the compiler reported it", left_open, g_line.c_str(), g_near.c_str());
    return result;
}

}  // namespace

ScriptCheck script_check(const std::string& lgc) { return check_file(lgc); }

bool script_check_held(ScriptCheck& check) {
    if (g_held.ok) return false;
    check = g_held;
    g_held = ScriptCheck();
    return true;
}

void install_script_check() {
    // push 0 / push 0 / push dword [esp+0xc]
    const uint8_t exit_bytes[] = {0x6A, 0x00, 0x6A, 0x00, 0xFF, 0x74, 0x24, 0x0C};
    // push 0x40 / push dword [esp+0xc]
    const uint8_t fopen_bytes[] = {0x6A, 0x40, 0xFF, 0x74, 0x24, 0x0C};
    // push esi / mov esi,[esp+8]
    const uint8_t fclose_bytes[] = {0x56, 0x8B, 0x74, 0x24, 0x08};
    // sub esp,0x40 / mov edx,[esp+0x4c]
    const uint8_t error_bytes[] = {0x83, 0xEC, 0x40, 0x8B, 0x54, 0x24, 0x4C};
    // push -1 / push 0x473e83 (its exception handler)
    const uint8_t load_bytes[] = {0x6A, 0xFF, 0x68, 0x83, 0x3E, 0x47, 0x00};
    g_exit = reinterpret_cast<Exit_t>(
        patch::hook(CRT_EXIT, exit_bytes, sizeof exit_bytes, reinterpret_cast<void*>(&exit_check), "crt_exit"));
    g_fopen = reinterpret_cast<Fopen_t>(
        patch::hook(CRT_FOPEN, fopen_bytes, sizeof fopen_bytes, reinterpret_cast<void*>(&fopen_check), "crt_fopen"));
    g_fclose = reinterpret_cast<Fclose_t>(
        patch::hook(CRT_FCLOSE, fclose_bytes, sizeof fclose_bytes, reinterpret_cast<void*>(&fclose_check), "crt_fclose"));
    g_compile_error = reinterpret_cast<CompileError_t>(patch::hook(SCRIPT_COMPILE_ERROR, error_bytes, sizeof error_bytes,
                                                                   reinterpret_cast<void*>(&compile_error_check),
                                                                   "Script_CompileError"));
    g_load = reinterpret_cast<Load_t>(
        patch::hook(SCRIPT_LOAD, load_bytes, sizeof load_bytes, reinterpret_cast<void*>(&load_check), "Script_Load"));
}
