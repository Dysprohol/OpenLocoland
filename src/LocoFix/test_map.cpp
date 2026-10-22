// Test map from the command line (--test-map <path>, or -map <path>): the game loads that
// map straight away, so a test does not have to pick it in the menu.
//
// Picking a mission in the menu only calls Load(map) in its script, which sets
// World->nextLevelFile and flags 0x40 for Game_Step; tools/test_missions.ps1 would have to
// open the strategic map and click the mission's dot to do the same. Here the map simply
// takes the place of the first level the game loads (in the Level_Load hook that
// frame_limit.cpp installs for its per-level frame rate), so the game loads it exactly as
// it would after the click, and the intro and the menu are skipped.
//
// The switch itself cannot be handed to the game: World_Construct takes a non-empty
// command line as the map to start with (levelFile), so the game would try to open
// "--test-map maps\l01\l01.map" as a map and show "LOAD: Invalid map file" in a message
// box. Replacing the name before Level_Load is what keeps that from happening; the map the
// game is in afterwards (World->levelFile) shows whether it really loaded.
//
// Level_Load takes the name as a String by value and frees it at the end (crt_delete), so the
// replacement is a String of the game's own heap and the requested one is freed in its place.
// (Handing it the static g_map made the game free memory of this DLL: harmless while the
// bytes in front of g_map happened to look like no heap block, a crash in the heap once
// other variables moved there.)
#include "patch.h"

namespace {

constexpr size_t WORLD_LEVEL_FILE = 0x014;   // World: the current map (String)
constexpr uintptr_t CRT_NEW = 0x456b11;
constexpr uintptr_t CRT_DELETE = 0x456a00;
constexpr uintptr_t EMPTY_STRING = 0x4a9d6c;   // the shared empty String, never freed

using New_t = char*(__cdecl*)(size_t size);
using Delete_t = void(__cdecl*)(const void* memory);

char g_map[MAX_PATH] = "";   // the map to load, empty without the switch
bool g_pending = false;      // a load was replaced and its result is not checked yet
bool g_checked = false;      // the test map was loaded (or failed) already

}  // namespace

void install_test_map() {
    const char* at = GetCommandLineA();
    bool found = false;
    std::string map;
    while (*at && !found) {
        const std::string token = patch::read_token(at);
        if (token == "--test-map" || token == "-map") {
            found = true;
            map = patch::read_token(at);
        }
    }
    if (!found) return;
    if (map.empty()) {
        patch::log("test map: the switch needs a map path, e.g. --test-map maps\\l01\\l01.map");
        return;
    }
    lstrcpynA(g_map, map.c_str(), sizeof g_map);
    const bool absolute = map.size() > 1 && (map[1] == ':' || map[0] == '\\');
    const std::string path = absolute ? map : patch::game_folder() + map;
    const bool there = GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES;
    patch::log("test map: %s%s", g_map, there ? "" : " (not found)");
}

const char* test_map_replace(const char* requested) {
    if (!g_map[0] || g_pending || g_checked) return requested;
    const size_t size = lstrlenA(g_map) + 1;
    char* map = reinterpret_cast<New_t>(CRT_NEW)(size);
    if (!map) return requested;
    lstrcpynA(map, g_map, static_cast<int>(size));
    if (requested && reinterpret_cast<uintptr_t>(requested) != EMPTY_STRING)
        reinterpret_cast<Delete_t>(CRT_DELETE)(requested);
    g_pending = true;
    return map;
}

void test_map_check(void* world) {
    if (!g_pending) return;
    g_pending = false;
    g_checked = true;
    const auto* level = *reinterpret_cast<const char* const*>(
        static_cast<uint8_t*>(world) + WORLD_LEVEL_FILE);
    if (level && lstrcmpiA(level, g_map) == 0)
        patch::log("test map: loaded %s", level);
    else
        patch::log("test map: %s did not load (the game is in '%s')", g_map, level ? level : "");
}
