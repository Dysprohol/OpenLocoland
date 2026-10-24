// LocoFix.dll - game fixes loaded by Locoland.exe at the start of WinMain.
//
// The exe calls LoadLibraryA("LocoFix.dll") before WinMain (patch in tools/patch_exe.py),
// so everything here happens before the game reads its configuration and creates the
// window. Messages go to LocoFix.log next to the exe.
#include "patch.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <vector>

void install_language();
void install_editor_text();
void install_window();
void install_test_mode();
void install_resolutions();
void install_camera();
void install_view_culling();
void install_birth_animation();
void install_adopt_command();
void install_shadow_leak();
void install_unit_limit();
void install_fast_ftol();
void install_layer10_lock();
void install_mouse_coords();
void install_depot_rows();
void install_status_bar();
void install_mouse_pan();
void install_screen_scaling();
void install_frame_limit();
void install_test_map();
void frame_limit_exit();
void install_script_random();
void install_script_log();
void install_script_value();
void install_world_script();
void install_player_registry();
void install_level_save();
void install_editor();
void restart_wait();
void install_options();
void install_restart();

namespace {

// The game writes its log to "error<date> <time>.log" and renames it to error.log only
// on a normal exit (removing the previous one). After a crash or a killed process the
// dated file stays behind, so they pile up. We keep the newest few - they may be needed
// to look into a crash - and delete the rest. LocoFix.log starts afresh every run.
constexpr size_t KEPT_ORPHAN_LOGS = 3;

void clean_up_logs() {
    const std::string folder = patch::game_folder();
    std::vector<std::string> orphans;
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((folder + "error????-??-?? ??h??m??.log").c_str(), &found);
    if (h != INVALID_HANDLE_VALUE) {
        do orphans.push_back(found.cFileName);
        while (FindNextFileA(h, &found));
        FindClose(h);
    }
    std::sort(orphans.begin(), orphans.end());   // the date in the name sorts oldest first
    for (size_t i = 0; i + KEPT_ORPHAN_LOGS < orphans.size(); ++i)
        DeleteFileA((folder + orphans[i]).c_str());
    DeleteFileA((folder + "LocoFix.log").c_str());
}

}  // namespace

namespace patch {

std::string game_folder() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char* slash = std::strrchr(path, '\\');
    if (slash) slash[1] = '\0';
    return slash ? path : "";
}

void log(const char* format, ...) {
    FILE* f = std::fopen((game_folder() + "LocoFix.log").c_str(), "a");
    if (!f) return;
    va_list a;
    va_start(a, format);
    std::vfprintf(f, format, a);
    va_end(a);
    std::fputc('\n', f);
    std::fclose(f);
}

}  // namespace patch

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        restart_wait();   // a restarted game waits for the old one first (restart.cpp)
        clean_up_logs();
        patch::log("LocoFix %s", LOCOFIX_VERSION);
        // The patches fit one exe version only - we recognise it by the build date
        // in the PE header (2003-10-07, the Steam version; the same exe for all languages).
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(GetModuleHandleA(nullptr));
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(dos) + dos->e_lfanew);
        if (nt->FileHeader.TimeDateStamp != 0x3F8275D4) {
            patch::log("unknown exe - no fixes applied");
            return TRUE;
        }
        install_language();
        install_editor_text();   // after install_language: its lookups go to the chosen pack
        install_window();
        install_resolutions();
        install_camera();
        install_view_culling();
        install_birth_animation();
        install_adopt_command();
        install_shadow_leak();
        install_unit_limit();
        install_fast_ftol();
        install_layer10_lock();
        install_mouse_coords();
        install_depot_rows();
        install_status_bar();
        install_script_random();
        install_script_log();
        install_script_value();
        install_world_script();
        install_player_registry();
        install_mouse_pan();
        // Screen scaling hooks RegisterClassA after test mode and calls test mode's
        // hook in turn, so test mode supplies the virtual cursor and screen scaling maps it.
        install_test_mode();
        install_screen_scaling();
        install_editor();   // after screen scaling: its window procedure goes in front
        install_level_save();
        install_test_map();
        install_frame_limit();
        install_options();   // after language and frame_limit: takes their CreateFileA and DialogBoxParamA
        install_restart();   // after options: takes its DialogBoxParamA entry
    } else if (reason == DLL_PROCESS_DETACH) {
        frame_limit_exit();
    }
    return TRUE;
}
