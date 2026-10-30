// Editor mode (--editor on the command line, with or without --test-map <map>): the mode the
// engine kept for the authors' own map editor (MapEdit.exe, which was not shipped).
//
// The mode is World.flags bit 0x1, which World_Construct copies from bit 0 of g_settings+0x220
// (0x47b410) - 0 in the exe, and nothing sets it. Set before WinMain, it gives:
//  - the window menu "AppMenu" (World_Construct registers the window class with it): File with
//    Load, Save, About and Exit, and an Options item with no popup (id 0xFFFF) - LocoFix leaves
//    it out of the class (RegisterClassA_editor below); the Menu panel of the bar has its commands,
//  - decals (spriteClass 8) loaded as ordinary sprites (Vid_Read), so they stay objects that
//    can be saved and moved instead of being baked into the ground and removed,
//  - objects with VID flag 8 (buildings) keep the authored ground height (Object_Construct
//    skips World_SetGroundZ), so a saved GRID is the one of the map file,
//  - no ANI_BIRTH handlers while a level loads (Level_Load) and no BIRTH scripts of new objects
//    (World_CreateObject),
//  - terrain objects on the minimap (Minimap_Draw),
//  - in full screen, presenting with Blt like a window (Graphics flag 0x400, the same settings
//    bit), so the menu bar is not flipped away.
// The game itself would run as usual - the mission script (its main creates reinforcements,
// changes the weather), the AI, trains driving, red semaphores taking their first command - and
// a save would write that running state. So a map in the current format (with GRPH - what
// Level_Save writes) is held once it is loaded: paused as script Pause(1) does (World_Pause: no
// object updates) and with script events off (World flags 0x20000: not even main runs). The
// camera still scrolls: the screen edge, the arrows (the scroll type the script would set is set
// here) and LocoFix's middle-button panning. The old-format scenes - logo.map with the intro,
// menu.map with the main menu and the strategic map - run as usual, so "--editor" alone starts
// at the main menu and a mission picked there opens held. Options > Run the level (the menu's
// Options item, which had no popup and no handler) lets the level run and holds it again
// (World_Resume puts the object timers back).
//
// The menu commands had no handlers left in the exe (GameWindow_WndProc handles only Exit,
// 40004, and the screenshot key, 40023; comdlg32 is not imported), so they are handled here, in
// a window procedure in front of the game's:
//  - Load (40001, Alt+F3): a Windows open dialog in the folder of the current map, then the
//    map is loaded the way script Load() does it (World.nextLevelFile and flags 0x40; Game_Step
//    loads it after the message),
//  - Save (40002, Alt+F2): a save dialog, then Level_Save with the name as a String of the
//    game's heap (level_save.cpp makes saving safe), then the map's mission (editor_mission.cpp:
//    the script's editor blocks - a new map gets a script -, the texts, the custom maps page),
//  - About (40003, Alt+F10): a small box,
//  - ours: File > New... (editor_newmap.cpp: a map on a campaign terrain or a generated ground),
//    File > Map size... (the same file: the saved map grown or cropped, then loaded again),
//    File > Export map... and Import map... (editor_exchange.cpp packs a map of the custom maps
//    page into a .zip and unpacks one into maps\<name>\ - below, "Export and Import"),
//    File > Mission settings... (editor_mission.cpp's dialog), File > Play the map and Back
//    to the menu (restart.cpp starts the game again in normal mode, on the current map - and back
//    in the editor when it is left for the menu - or at the main menu), the Edit menu (editor_edit.cpp: undo, redo,
//    delete, turn, army, semaphore) and Options > Run the level; edits not saved are offered to
//    be saved before the level goes (Load, Play, Back, Exit, closing the window).
// While the level is held, the mouse and the keys go to editor_edit.cpp first (object editing).
// The accelerators (AppAccel, always loaded) are Alt+F3, Alt+F2 and Alt+F10, while the menu
// says F3, F2 and F1 (those keys belong to the depot build list), so the labels are corrected.
// Dialogs and the menu run with the game in GDI mode (Graphics_EnterGdiMode: drawing stops,
// FlipToGDISurface in full screen), the Windows cursor shown (the game hides it for its own),
// and the game's current directory kept. Outside editor mode the accelerators do nothing, as
// before.
//
// The switch is removed from the process command line: World_Construct takes a command line
// without ".cfg" in it as the map to start with, and WinMain's lpCmdLine points into that
// buffer, so "--editor" alone would be opened as a map.
//
// Test mode (LOCOFIX_TEST=1) also takes WM_COPYDATA with dwData 40001 or 40002 and a file name:
// Load or Save without a dialog, so tools/test_game.ps1 can save and load maps.
#include "patch.h"
#include "editor_exchange.h"
#include "editor_ground.h"
#include "editor_mission.h"
#include "editor_text.h"
#include <shobjidl.h>
#include <cctype>
#include <cstdio>

bool level_save_succeeded();   // level_save.cpp
bool restart_game(bool editor, const std::string& map, bool back_to_editor);   // restart.cpp
// editor_edit.cpp: editing the objects of the held level
bool edit_message(HWND window, UINT m, WPARAM w, LPARAM l);
void edit_command(int id);
void edit_reset();
bool edit_dirty();
void edit_remember_selection();   // editor_edit.cpp: Run the level's way back
void edit_restore_selection();
void edit_keep_history();
void edit_restore_history();
void edit_saved();
void edit_message_saved(const std::string& map);
void panel_saved(const std::string& file);   // editor_panel.cpp: the SAVED notice
void edit_message_line(const std::string& text);
void install_editor_edit();
void edit_level_held();
void edit_touch();
void edit_assign_orders();   // editor_edit.cpp
void edit_commit_mission(const ArrivalsState& arrivals_before, const FactoryState& factories_before);
void panel_open_arrivals();   // editor_panel.cpp
void edit_open_enemy_factory();   // editor_edit.cpp: the panel of the map's first enemy factory
bool edit_open_factory_at(float x, float y);   // editor_edit.cpp: the panel of the factory at a map point
void new_map_dialog(HWND owner);                // editor_newmap.cpp
bool new_map_for_test(const std::string& text);  // editor_newmap.cpp
std::string make_start_map();                    // editor_newmap.cpp: maps\_new\, empty grass
std::string map_ground_image(const std::string& map);                          // editor_newmap.cpp
bool set_map_ground_image(const std::string& map, const std::string& image);  // editor_newmap.cpp
bool map_size_dialog(HWND owner, int& w, int& h);   // editor_newmap.cpp
bool resize_map(const std::string& map, int w, int h, std::string& error);   // editor_newmap.cpp
void edit_map_size(int& w, int& h);              // editor_edit.cpp
int edit_outside(int w, int h, bool remove);     // editor_edit.cpp
void edit_log_rail_points(float x, float y);
void test_cursor_to_middle(HWND window);   // test.cpp
std::string editor_new_map_title();   // below: the new map's title until it is saved

namespace {

constexpr uintptr_t G_SETTINGS_FLAGS = 0x47b410;   // g_settings + 0x220
constexpr uintptr_t READ_SETTINGS_FLAGS = 0x419a0a;   // World_Construct: mov edx,[ecx+0x220]
constexpr uintptr_t MENU_NAME = 0x41a658;   // World_Construct: and eax,"AppMenu" (lpszMenuName)
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t WORLD_PAUSE = 0x41fc20;
constexpr uintptr_t WORLD_RESUME = 0x41fc40;
constexpr uintptr_t ENTER_GDI_MODE = 0x411d40;
constexpr uintptr_t LEAVE_GDI_MODE = 0x411db0;
constexpr uintptr_t STRING_ASSIGN_CSTR = 0x426540;
constexpr uintptr_t CRT_NEW = 0x456b11;
constexpr uintptr_t CRT_DELETE = 0x456a00;
constexpr uintptr_t EMPTY_STRING = 0x4a9d6c;   // the shared empty String, never freed
constexpr uintptr_t IAT_RegisterClassA = 0x4751b8;
constexpr size_t WORLD_FLAGS = 0x004;
constexpr size_t WORLD_LEVEL_FILE = 0x014;
constexpr size_t WORLD_NEXT_LEVEL_FILE = 0x018;
constexpr size_t WORLD_SCROLL_TYPE = 0x030;
constexpr size_t WORLD_HWND = 0x214;
constexpr uint32_t SCROLL_FROM_BORDER = 0x1, SCROLL_WITH_ARROWS = 0x20;
constexpr size_t WORLD_VTABLE_SAVE_LEVEL = 0x1c;   // Level_Save
constexpr uint32_t PAUSED = 0x10, LEVEL_CHANGE = 0x40, SCRIPTS_OFF = 0x20000;
constexpr int ID_LOAD = 40001, ID_SAVE = 40002, ID_ABOUT = 40003, ID_EXIT = 40004;
constexpr int ID_RUN = 40010;         // ours: Options > Run the level
constexpr int ID_BACK = 40011;        // ours: File > Back to the menu
constexpr int ID_PLAY = 40012;        // ours: File > Play the map
constexpr int ID_MISSION = 40013;     // ours: File > Mission settings...
constexpr int ID_NEW = 40014;         // ours: File > New...
constexpr int ID_RESIZE = 40015;      // ours: File > Map size...
constexpr int ID_EXPORT = 40016;      // ours: File > Export map...
constexpr int ID_IMPORT = 40017;      // ours: File > Import map...
constexpr int ID_UNDO = 40030, ID_REDO = 40031, ID_DELETE = 40032, ID_ROTATE_RIGHT = 40033,
              ID_ROTATE_LEFT = 40034, ID_ARMY = 40035, ID_SEMAPHORE = 40036;   // ours: the Edit menu
constexpr int OPTIONS_POSITION = 1;   // the "Options" item (id 0xFFFF) on the menu bar
constexpr int TEST_VIEW = 40040;      // test mode: WM_COPYDATA "x y" puts the view there
constexpr int TEST_RAIL_POINTS = 40041;   // test mode: WM_COPYDATA "x y" logs the rail points there
constexpr int TEST_MISSION = 40042;   // test mode: WM_COPYDATA <file> - the mission settings of a file
constexpr int TEST_NEW_MAP = 40043;   // test mode: WM_COPYDATA "<name> <ground>" - File > New without the dialog
constexpr int TEST_RESIZE = 40044;    // test mode: WM_COPYDATA "<width> <height>" - File > Map size without the dialogs
constexpr int TEST_EXPORT = 40045;    // test mode: WM_COPYDATA <zip> - File > Export map without the dialogs
constexpr int TEST_IMPORT = 40046;    // test mode: WM_COPYDATA "<zip>|overwrite|rename|ask" - File > Import map
constexpr int TEST_FACTORY = 40047;   // test mode: WM_COPYDATA "x y" - the panel of the factory there
constexpr UINT WM_SAVE_AFTER_RUN = WM_APP + 0x52;   // Run the level's copy held again: save it (wParam 1: then end the game)
constexpr uintptr_t SET_POSITION = 0x41c680;   // SetPosition: the centre of the view, clamped
constexpr uintptr_t G_SOUND = 0x5d3a54;        // g_sound: the sound object
constexpr uintptr_t STOP_MUSIC = 0x444e50;     // StopMusic(g_sound)
constexpr uintptr_t SOUND_MUSIC_FILE = 0x3d8;  // g_sound: the music file (a char* of the game's heap, or the empty String)

using GdiMode_t = void(__fastcall*)(void* graphics, void* edx);
using SetPosition_t = void(__fastcall*)(void* world, void* edx, float x, float y, int mode);
using PauseResume_t = void(__fastcall*)(uint8_t* world, void* edx);
using StopMusic_t = bool(__fastcall*)(void* sound, void* edx);
using AssignCStr_t = char**(__fastcall*)(char** string, void* edx, const char* text);
using SaveLevel_t = void(__fastcall*)(uint8_t* world, void* edx, char* name);
using New_t = char*(__cdecl*)(size_t size);
using Delete_t = void(__cdecl*)(const void* memory);
using RegisterClassA_t = ATOM(WINAPI*)(const WNDCLASSA*);

bool g_enabled = false;
bool g_test = false;
bool g_test_ask = false;   // LOCOFIX_TEST_ASK=1: the questions are asked in test mode too (their screenshots)
bool g_running = false;   // Options > Run the level
bool g_held = false;      // the current level is held
bool g_ctrl = false;      // Ctrl is down (its key messages)
WNDPROC g_game_wndproc = nullptr;
RegisterClassA_t g_register_class = nullptr;
std::string g_current;   // the map last loaded or saved here (the dialogs start with it)

uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }
void* graphics() { return *reinterpret_cast<void**>(G_GRAPHICS); }

const char* level_file() {
    uint8_t* w = world();
    return w ? *reinterpret_cast<const char**>(w + WORLD_LEVEL_FILE) : "";
}

std::string full_path(const std::string& path) {
    char full[MAX_PATH];
    const DWORD n = GetFullPathNameA(path.c_str(), MAX_PATH, full, nullptr);
    return n && n < MAX_PATH ? full : path;
}

bool same_file(const std::string& a, const std::string& b) {
    return lstrcmpiA(full_path(a).c_str(), full_path(b).c_str()) == 0;
}

// A path inside the game's current directory as the game writes its own ("maps\...").
std::string game_path(const std::string& path) {
    char dir[MAX_PATH];
    const DWORD n = GetCurrentDirectoryA(MAX_PATH, dir);
    std::string full = full_path(path);
    if (n && n < MAX_PATH && full.size() > n + 1 && full[n] == '\\' && lstrcmpiA(full.substr(0, n).c_str(), dir) == 0)
        return full.substr(n + 1);
    return full;
}

std::string script_of(const std::string& map) {
    const size_t dot = map.find_last_of('.');
    return (dot == std::string::npos ? map : map.substr(0, dot)) + ".lgc";
}

uint32_t& world_flags(uint8_t* w) { return *reinterpret_cast<uint32_t*>(w + WORLD_FLAGS); }

// ---- holding the level ----

// A map in the current format - the one Level_Save writes, with a GRPH chunk - as opposed to
// the old-format scenes (logo.map, menu.map: the intro, the main menu and the strategic map).
bool current_format(const char* map) {
    HANDLE file = CreateFileA(map, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    bool found = false;
    uint32_t chunk[2];   // id, size
    DWORD read = 0;
    LONG position = 12;   // after "RES ", the size and "MAP "
    while (SetFilePointer(file, position, nullptr, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
           ReadFile(file, chunk, sizeof chunk, &read, nullptr) && read == sizeof chunk) {
        if (std::memcmp(&chunk[0], "GRPH", 4) == 0) found = true;
        if (found || std::memcmp(&chunk[0], "HEAD", 4) == 0) break;   // GRPH comes before HEAD
        position += 8 + ((chunk[1] + 1) & ~1u);
    }
    CloseHandle(file);
    return found;
}

void hold_level(uint8_t* w, bool hold) {
    g_held = hold;
    if (hold) {
        reinterpret_cast<PauseResume_t>(WORLD_PAUSE)(w, nullptr);
        world_flags(w) |= SCRIPTS_OFF;
        // the scripts would set it (SetScrollType); a level starts with the screen edge only
        *reinterpret_cast<uint32_t*>(w + WORLD_SCROLL_TYPE) |= SCROLL_FROM_BORDER | SCROLL_WITH_ARROWS;
        // Hide the game's state bar (icons/units UI) when returning to editor mode
        // Use Player_ReleaseLevel instead of Player_HideStateBar - it also clears bar slots/messages
        if (void* player = *reinterpret_cast<void**>(w + 0x220))  // World.players[0] (human player)
            reinterpret_cast<void(__fastcall*)(void*, void*)>(0x42e3b0)(player, nullptr);  // Player_ReleaseLevel
        edit_level_held();
    } else {
        world_flags(w) &= ~SCRIPTS_OFF;
        reinterpret_cast<PauseResume_t>(WORLD_RESUME)(w, nullptr);
    }
}

// The game's music (the running level's script played it): StopMusic(g_sound), as script function 134.
// StopMusic leaves the music's file name (g_sound +0x3d8, PlayMusicFile's), and the sound's update
// (every frame) now and then finds the stream stopped and starts that file again: the victory's
// music of a won run came back a few seconds after the level was held. The name goes with it - the
// update then stops the music for good (its own StopMusic for an empty name).
void stop_music() {
    auto* sound = *reinterpret_cast<uint8_t**>(G_SOUND);
    if (!sound) return;
    reinterpret_cast<StopMusic_t>(STOP_MUSIC)(sound, nullptr);
    char*& file = *reinterpret_cast<char**>(sound + SOUND_MUSIC_FILE);
    if (file && reinterpret_cast<uintptr_t>(file) != EMPTY_STRING) {
        reinterpret_cast<Delete_t>(CRT_DELETE)(file);
        file = reinterpret_cast<char*>(EMPTY_STRING);
    }
}

// Run the level: the level runs from its saved file - the mission's script with what the panels set
// (arrivals, orders, the factories' options) is compiled when the level loads, so the map is written
// first and loaded again, running. Back (Ctrl+R, BACK TO THE EDITOR in the Esc menu, or the level
// ending: won, lost, ABORT GAME) loads it again, held - the state of the run does not stay. A map
// that is not written in place - a campaign map, POLIGON or the template, and the start's new map,
// which has no name yet - runs from a copy in a hidden scratch folder, maps\_run\ (marked as the new
// map's, so it is never on the custom maps page and goes with the map); back, the copy is held again
// with the editor on the original map: its name, its edits not saved, the history.
constexpr const char* RUN_FOLDER = "maps\\_run";
constexpr const char* RUN_MAP = "maps\\_run\\_run.map";
std::string g_run_map;   // the map that runs (game path)
std::string g_run_origin;   // the map the copy in maps\_run\ stands for (empty: the map runs from its own file)
bool g_run_dirty = false;   // the copy holds edits not saved
bool g_run_back = false;   // the next load is the way back: the view and the chosen object as they were
bool g_save_on_back = false, g_close_on_back = false;   // the edits of the copy to be saved once it is held again
float g_run_view_x = 0, g_run_view_y = 0;   // the view's top-left before the run

void load_map(const std::string& path);

void stop_running() {
    g_running = false;
    stop_music();
    patch::log("editor: the level is held again - %s loaded", g_run_map.c_str());
    load_map(g_run_map);
}

// ---- the Windows cursor ----
//
// The game hides it (ShowCursor(FALSE) in World_Construct) and draws its own in the scene, so
// it is shown while a dialog or the menu is open and while the mouse is over the frame and the
// menu bar.

bool g_in_dialog = false, g_in_menu = false, g_over_frame = false, g_cursor_shown = false;

void update_cursor() {
    const bool show = g_in_dialog || g_in_menu || g_over_frame;
    if (show != g_cursor_shown) {
        ShowCursor(show);
        g_cursor_shown = show;
    }
}

// A modal dialog over the game: GDI mode as for the menu loop, the cursor shown, the current
// directory kept (the file dialogs may change it).
class DialogScope {
public:
    DialogScope() {
        GetCurrentDirectoryA(MAX_PATH, dir_);
        if (graphics()) reinterpret_cast<GdiMode_t>(ENTER_GDI_MODE)(graphics(), nullptr);
        g_in_dialog = true;
        update_cursor();
    }
    ~DialogScope() {
        g_in_dialog = false;
        update_cursor();
        if (graphics()) reinterpret_cast<GdiMode_t>(LEAVE_GDI_MODE)(graphics(), nullptr);
        SetCurrentDirectoryA(dir_);
    }

private:
    char dir_[MAX_PATH] = "";
};

// ---- the map the editor starts on ----
//
// The editor started from the menu (EDYTOR MAP: StartEditorMode("maps\\template\\template.map")) or
// by the desktop shortcut (--editor --test-map maps\template\template.map) does not open the template but
// a new empty map that has no name yet: 2048 x 1536 of grass, as File > New makes it
// (editor_newmap.cpp), titled NEW MAP (the pack's NewMapTitle). The engine loads a level only from
// its files, so the map is made in a hidden scratch folder, maps\_new\ (_new2 ... while an older one
// is still in use), marked with AUTO_MARK; it is never on the custom maps page, and the first save -
// File > Save, Ctrl+S, the "save the changes?" of Load, Play, Exit ... - asks for the name in maps\
// (choose_map), as File > Save does. A marked folder goes when the editor leaves its map - for
// another map, back to the menu, the end of the game -, so an untouched start leaves nothing in
// maps\. Play the map and Export ask for its name first, as a save does, so the played game and a
// package never take the scratch folder. A marked folder left behind (the game ended) is removed at
// the next start of the editor. The template
// itself stays: it is loaded like any map, and new maps take their script and texts from it.
constexpr const char* AUTO_MARK = "editor_new.txt";
bool g_playing = false;   // Play the map: the current map is kept for the game that plays it

std::string current_map();

std::string lower_path(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string folder_of(const std::string& map) {
    const size_t slash = map.find_last_of('\\');
    return slash == std::string::npos ? "" : map.substr(0, slash);
}

bool auto_folder(const std::string& folder) {
    return !folder.empty() && GetFileAttributesA((folder + "\\" + AUTO_MARK).c_str()) != INVALID_FILE_ATTRIBUTES;
}

// A scratch folder of maps\ made hidden and marked (`text` says what it is to whoever finds it).
bool mark_folder(const std::string& folder, const char* text) {
    CreateDirectoryA(folder.c_str(), nullptr);
    SetFileAttributesA(folder.c_str(), FILE_ATTRIBUTE_HIDDEN);
    HANDLE mark = CreateFileA((folder + "\\" + AUTO_MARK).c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (mark == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    WriteFile(mark, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
    CloseHandle(mark);
    return true;
}

// A marked folder of maps\ with its files (the editor made them all; it has no subfolders).
void remove_auto_folder(const std::string& folder) {
    if (!auto_folder(folder)) return;
    WIN32_FIND_DATAA f;
    HANDLE find = FindFirstFileA((folder + "\\*").c_str(), &f);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) DeleteFileA((folder + "\\" + f.cFileName).c_str());
        } while (FindNextFileA(find, &f));
        FindClose(find);
    }
    const bool removed = RemoveDirectoryA(folder.c_str()) != 0;
    patch::log(lower_path(folder) == lower_path(RUN_FOLDER) ? "editor: the run's copy %s left - %s" : "editor: the new map %s left unsaved - %s",
               folder.c_str(), removed ? "removed" : "could not be removed");
}

// The marked folders of maps\ left behind (but `keep`, the folder of the map being opened).
void sweep_auto_folders(const std::string& keep) {
    WIN32_FIND_DATAA f;
    HANDLE find = FindFirstFileA("maps\\*", &f);
    if (find == INVALID_HANDLE_VALUE) return;
    std::vector<std::string> folders;
    do {
        if ((f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && f.cFileName[0] != '.') folders.push_back(std::string("maps\\") + f.cFileName);
    } while (FindNextFileA(find, &f));
    FindClose(find);
    for (const std::string& folder : folders)
        if (lower_path(folder) != lower_path(keep)) remove_auto_folder(folder);
}

// The map to open when the editor is started on `requested`: for the template, a new empty map.
std::string start_map(const std::string& requested) {
    if (!g_enabled) return requested;
    const bool fresh = lower_path(game_path(requested)) == "maps\\template\\template.map";
    sweep_auto_folders(fresh ? "" : folder_of(game_path(requested)));
    if (!fresh) return requested;
    const std::string map = make_start_map();
    if (map.empty()) return requested;
    mark_folder(folder_of(map), "The editor's new map, not saved yet; removed with this folder when the editor leaves it.\r\n");
    patch::log("editor: starts on a new empty map %s", map.c_str());
    return map;
}

// The current map is left for `next` (empty: the game ends or goes back to the menu): a new map of
// the start that was never saved goes with its folder, and so does Run the level's copy.
// (the loaded level's folder, or the last saved map's: saved under another name, the level is still
// the start's map - or the copy of a run, which stands for the map of g_run_origin)
void leave_map(const std::string& next) {
    if (g_playing) return;
    const std::string to = next.empty() ? "" : lower_path(folder_of(game_path(next)));
    for (const std::string& map : {game_path(level_file()), game_path(current_map()), g_run_origin}) {
        if (!auto_folder(folder_of(map))) continue;
        if (!next.empty() && to == lower_path(folder_of(map))) continue;
        // the map stays while its copy runs or is held again
        if (to == lower_path(RUN_FOLDER) && lower_path(map) == lower_path(game_path(current_map()))) continue;
        remove_auto_folder(folder_of(map));
    }
}

// ---- Load and Save ----

void load_map(const std::string& path) {
    uint8_t* w = world();
    if (!w) return;
    leave_map(path);
    reinterpret_cast<AssignCStr_t>(STRING_ASSIGN_CSTR)(reinterpret_cast<char**>(w + WORLD_NEXT_LEVEL_FILE), nullptr,
                                                       path.c_str());
    *reinterpret_cast<uint32_t*>(w + WORLD_FLAGS) |= LEVEL_CHANGE;
    g_current = path;
    patch::log("editor: load %s", path.c_str());
}

bool save_map(const std::string& path) {
    uint8_t* w = world();
    if (!w) return false;
    // a new map's folder (maps\<name>\ for the custom maps page)
    const size_t slash = path.find_last_of('\\');
    if (slash != std::string::npos && slash > 0) CreateDirectoryA(path.substr(0, slash).c_str(), nullptr);
    char* name = reinterpret_cast<New_t>(CRT_NEW)(path.size() + 1);
    if (!name) return false;
    std::memcpy(name, path.c_str(), path.size() + 1);
    const auto save = *reinterpret_cast<SaveLevel_t*>(*reinterpret_cast<uint8_t**>(w) + WORLD_VTABLE_SAVE_LEVEL);
    save(w, nullptr, name);   // Level_Save frees the name
    if (!level_save_succeeded()) return false;
    g_current = path;
    // Run the level's copy in its scratch folder: nothing is saved for the user, and it stays off the
    // custom maps page
    const bool scratch = auto_folder(folder_of(game_path(path)));
    if (!scratch) edit_saved();
    // a painted ground: its own image next to the map, the map pointing at it
    ground_save(game_path(path));
    // the new map saved under its name takes its ground image along (the scratch folder goes when
    // the editor leaves it)
    const std::string image = map_ground_image(game_path(path));
    if (!scratch && !image.empty() && auto_folder(folder_of(image))) {
        const std::string base = game_path(path), own = base.substr(0, base.find_last_of('.')) + ".vid";
        if (CopyFileA(image.c_str(), own.c_str(), FALSE) && set_map_ground_image(base, own))
            patch::log("editor: %s takes its ground along as %s", base.c_str(), own.c_str());
    }
    // its mission: the script (a new map gets one), the texts, the custom maps page
    mission_save(game_path(path));
    if (!scratch) edit_message_saved(game_path(path));
    if (!scratch) panel_saved(game_path(path));   // SAVED in the middle of the view
    return true;
}

std::wstring wide_acp(const std::string& s) {
    std::wstring w(static_cast<size_t>(MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0)), L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &w[0], static_cast<int>(w.size()));
    if (!w.empty()) w.pop_back();
    return w;
}

// A Windows open or save dialog (IFileOpenDialog / IFileSaveDialog: its title and file types are the
// pack's texts, shown right in any system code page), opened in `folder` with `file` proposed (none
// when empty); the file chosen as a path in the system's code page, empty when cancelled. `kind`
// names the files of `pattern` ("*.map"), `ext` the default extension. SetFolder, not the folder the
// dialog remembers for the program (every Locoland.exe, another game folder's as well).
std::string file_dialog(HWND owner, bool save, const std::string& folder, const std::string& file, const char* title,
                        const char* kind, const wchar_t* pattern, const wchar_t* ext) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    std::string path;
    IFileDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(save ? CLSID_FileSaveDialog : CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&dialog)))) {
        patch::log("editor: the file dialog is not available");
    } else {
        const std::wstring kind_w = ui_wide(kind), all = ui_wide(ui_text("FileAll", "All files (*.*)"));
        const COMDLG_FILTERSPEC types[] = {{kind_w.c_str(), pattern}, {all.c_str(), L"*.*"}};
        dialog->SetFileTypes(2, types);
        dialog->SetTitle(ui_wide(title).c_str());
        dialog->SetDefaultExtension(ext);
        dialog->SetFileName(wide_acp(file).c_str());
        FILEOPENDIALOGOPTIONS options = 0;
        dialog->GetOptions(&options);
        dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_PATHMUSTEXIST |
                           (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST));
        IShellItem* start = nullptr;
        if (!folder.empty() && SUCCEEDED(SHCreateItemFromParsingName(wide_acp(folder).c_str(), nullptr, IID_PPV_ARGS(&start)))) {
            dialog->SetFolder(start);
            start->Release();
        }
        DialogScope scope;
        IShellItem* result = nullptr;
        if (SUCCEEDED(dialog->Show(owner)) && SUCCEEDED(dialog->GetResult(&result))) {
            wchar_t* name = nullptr;
            if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &name))) {
                char chosen[MAX_PATH * 2] = "";
                WideCharToMultiByte(CP_ACP, 0, name, -1, chosen, sizeof chosen, nullptr, nullptr);
                path = chosen;
                CoTaskMemFree(name);
            }
            result->Release();
        }
        dialog->Release();
    }
    if (SUCCEEDED(com)) CoUninitialize();
    return path;
}

// A map of the release that Setup writes again with every update - POLIGON (maps\sandbox\) and the
// template: the editor saves it only under a name of its own, as it does a campaign map.
bool release_map(const std::string& map) {
    const std::string lower = lower_path(map);
    return lower.compare(0, 13, "maps\\sandbox\\") == 0 || lower.compare(0, 14, "maps\\template\\") == 0;
}

// Why a map of the release is saved under another name (the status line, the save dialog's title).
std::string release_reason(const std::string& map) {
    const bool sandbox = lower_path(map).compare(0, 13, "maps\\sandbox\\") == 0;
    return ui_printf(ui_format("SaveRelease", "%s is a map of the release - save it under a name of its own, an update would overwrite it"),
                     sandbox ? ui_text("ReleaseSandbox", "POLIGON") : ui_text("ReleaseTemplate", "The template"));
}

// A scratch folder of the editor (a game path): the start's new map, maps\_new\ (_new2 ...), or the
// copy of Run the level, maps\_run\ - they go when the editor leaves them, so no dialog proposes
// them and a map chosen in one is refused.
bool scratch_folder(const std::string& folder) {
    const std::string lower = lower_path(folder);
    return lower.compare(0, 9, "maps\\_new") == 0 || lower == lower_path(RUN_FOLDER) || auto_folder(folder);
}

// A file chosen in a Windows dialog, as a game path; empty when cancelled. Load opens in maps\ with
// no name proposed; Save in the map's folder with its name. The editor's new map has no name yet:
// the dialog opens in maps\ with the first free name mapN; a map of the release opens it
// with the first free poligonN (POLIGON) or mapN and says why in its title and the status line, and
// is not saved over. A map saved straight into maps\ (where the game keeps only its menu and intro
// scenes) goes into a folder of its name, maps\<name>\<name>.map, so it is on the custom maps page; a
// map there already is overwritten only when the user says so.
std::string choose_map(HWND owner, bool save) {
    const std::string current = full_path(g_current.empty() ? level_file() : g_current);
    const size_t slash = current.find_last_of('\\');
    std::string folder = slash == std::string::npos ? "" : current.substr(0, slash);
    std::string file = slash == std::string::npos ? "" : current.substr(slash + 1);
    if (!save) folder.clear(), file.clear();
    const bool release = save && release_map(game_path(current));
    std::string title = save ? ui_text("FileSaveTitle", "Save map") : ui_text("FileLoadTitle", "Load map");
    if (save && (release || scratch_folder(folder_of(game_path(current))))) {
        // a name of its own proposed: the first free maps\poligon<n>\ for POLIGON, maps\map<n>\ else
        const std::string base = release && lower_path(game_path(current)).compare(0, 13, "maps\\sandbox\\") == 0 ? "poligon" : "map";
        folder.clear();
        int n = 1;
        while (n < 1000 && GetFileAttributesA(("maps\\" + base + std::to_string(n)).c_str()) != INVALID_FILE_ATTRIBUTES) ++n;
        file = base + std::to_string(n) + ".map";
    }
    if (release) {
        title = release_reason(game_path(current));
        edit_message_line(title);
    }
    if (folder.empty() || GetFileAttributesA(folder.c_str()) == INVALID_FILE_ATTRIBUTES)
        folder = patch::game_folder() + "maps";
    const std::string chosen = file_dialog(owner, save, folder, file, title.c_str(), ui_text("FileMaps", "Maps (*.map)"), L"*.map", L"map");
    if (chosen.empty()) return "";
    std::string map = game_path(chosen);
    if (scratch_folder(folder_of(map))) {
        DialogScope scope;
        ui_message_box(owner, ui_text("ScratchFolder", "This is the editor's scratch folder - choose a map in another folder."),
                       title.c_str(), MB_OK | MB_ICONINFORMATION);
        patch::log("editor: a map of a scratch folder refused: %s", map.c_str());
        return "";
    }
    if (save && map.find(':') != std::string::npos) {   // a map's script and texts are found by game paths
        DialogScope scope;
        ui_message_box(owner, ui_text("SaveOutside", "A map is saved in the maps folder of this game."),
                       ui_text("FileSaveTitle", "Save map"), MB_OK | MB_ICONINFORMATION);
        patch::log("editor: not saved outside the game folder: %s", map.c_str());
        return "";
    }
    if (save && lower_path(folder_of(map)) == "maps") {
        const std::string name = map.substr(5, map.find_last_of('.') - 5);
        map = "maps\\" + name + "\\" + name + ".map";
        if (GetFileAttributesA(map.c_str()) != INVALID_FILE_ATTRIBUTES && (!g_test || g_test_ask)) {
            DialogScope scope;
            const std::string text = ui_printf(ui_format("AskOverwrite", "There is a map %s already. Overwrite it?"), map.c_str());
            if (ui_message_box(owner, text.c_str(), ui_text("FileSaveTitle", "Save map"), MB_YESNO | MB_ICONQUESTION) != IDYES)
                return "";
        }
        patch::log("editor: saved into a folder of its own: %s", map.c_str());
    }
    if (save && release_map(map)) {   // chosen again: not saved where the next update writes
        const std::string why = release_reason(map);
        edit_message_line(why);
        if (!g_test || g_test_ask) {
            DialogScope scope;
            ui_message_box(owner, why.c_str(), ui_text("FileSaveTitle", "Save map"), MB_OK | MB_ICONINFORMATION);
        }
        patch::log("editor: not saved over a map of the release: %s", map.c_str());
        return "";
    }
    return map;
}

void about(HWND owner) {
    DialogScope scope;
    ui_message_box(owner,
                ui_text("AboutText",
                        "OpenLocoland\n\n"
                        "Locoland (Gromada, 2003) with the LocoFix fixes.\n\n"
                        "Editor mode - the engine's mode for the authors' map editor:\n"
                        "File > Load (Alt+F3) loads a map, File > Save (Alt+F2) saves the\n"
                        "current state of the map as a .map file.\n\n"
                        "Click an object to select it, drag it to move it; R / E or the\n"
                        "wheel turn it, A changes its army, T switches a semaphore, Del\n"
                        "removes it, Ctrl+Z / Ctrl+Y undo and redo. The bar at the bottom\n"
                        "places new objects ([ and ] pick the decal); Esc or the right\n"
                        "button go back to selecting. Track (3) lays rails, the Push line\n"
                        "tool of the strip marks one-way lines (T flag, S semaphores),\n"
                        "File > Mission settings edits the mission and its texts.\n\n"
                        "The ? and the menu button at the left end of the bar: the help\n"
                        "and every command of these menus; Ctrl+R runs or holds the level."),
                ui_text("AboutTitle", "About"), MB_OK | MB_ICONINFORMATION);
}

std::string current_map() { return g_current.empty() ? std::string(level_file()) : g_current; }

bool game_folder_map(const std::string& map);

// The file Ctrl+S saves the map into without asking: the one it was loaded from or last saved as -
// not a map of the game's own folders (the campaign, POLIGON, the template) nor the start's new map,
// which has no name yet; empty when the name is to be asked.
std::string own_file() {
    const std::string map = game_path(current_map());
    if (map.empty() || game_folder_map(map) || auto_folder(folder_of(map))) return "";
    return map;
}

// Ctrl+S (and a Yes to "Save the changes?"): into the map's own file, or the name asked as File > Save
// asks it. False when cancelled or the save failed.
bool save_current(HWND window) {
    std::string map = own_file();
    if (map.empty()) map = choose_map(window, true);
    return !map.empty() && save_map(map);
}

bool game_folder_map(const std::string& map);
bool asks();

bool run_level(HWND window) {
    if (!g_held) return false;
    const std::string map = game_path(current_map());
    // a map not written in place runs from a copy: the start's new map, a game's own map with edits
    // or held again from its copy after a run (unchanged, the game's own map runs from its file)
    const bool copy = auto_folder(folder_of(map)) ||
                      (game_folder_map(map) && (edit_dirty() || lower_path(game_path(level_file())) != lower_path(map)));
    const std::string run = copy ? RUN_MAP : map;
    // written as a save writes it, without asking - also when nothing changed, so the script has the
    // runner and the panels' settings of this version (an older map's script is brought up to date)
    if (copy || !game_folder_map(map)) {
        bool written = true;
        if (copy) {
            // the copy's script is the map's own, its editor blocks written anew (a script without them
            // runs as it is); the high scores page stays in its briefing
            written = mark_folder(RUN_FOLDER, "Run the level's copy of the map the editor runs; written again for every run and "
                                              "removed with this folder when the editor leaves the map.\r\n");
            DeleteFileA(script_of(run).c_str());
            CopyFileA(script_of(map).c_str(), script_of(run).c_str(), FALSE);
            const std::string scores = folder_of(map) + "\\hiscores.txt";
            if (GetFileAttributesA(scores.c_str()) != INVALID_FILE_ATTRIBUTES)
                CopyFileA(scores.c_str(), (std::string(RUN_FOLDER) + "\\hiscores.txt").c_str(), FALSE);
        }
        written = written && save_map(run);
        g_current = map;   // the editor stays on its map (save_map took the copy's name)
        if (!written) {
            if (asks()) {
                DialogScope scope;
                ui_message_box(window, ui_printf(ui_format("RunFailed", "The map could not be written (%s): it is not run."), run.c_str()).c_str(),
                               ui_text("RunTitle", "Quick test"), MB_OK | MB_ICONWARNING);
            }
            return false;
        }
    }
    if (uint8_t* w = world()) {
        g_run_view_x = *reinterpret_cast<float*>(w + 0x44);
        g_run_view_y = *reinterpret_cast<float*>(w + 0x48);
    }
    edit_remember_selection();
    edit_keep_history();
    g_run_back = true;
    g_run_map = run;
    g_run_dirty = edit_dirty();
    g_running = true;
    patch::log("editor: run %s%s", run.c_str(), copy ? (" - a copy of " + map).c_str() : "");
    load_map(run);
    g_run_origin = copy ? map : "";
    g_current = map;
    return true;
}

// Before the level goes away: with edits not saved, asks whether to save them. False when the
// user cancels (or the save fails). Tests are never asked. While Run the level's copy runs, its
// edits are saved from the copy held again: Yes goes back to the editor and saves there, and the
// game ends after that when it was ending (`ending`).
bool ask_save(HWND window, bool ending = false) {
    if (g_test && !g_test_ask) return true;
    if (g_running ? g_run_origin.empty() || !g_run_dirty : !edit_dirty()) return true;
    int answer;
    {
        DialogScope scope;
        // the new map has no name yet: its title instead of the scratch folder's file
        const std::string name = auto_folder(folder_of(game_path(current_map()))) ? editor_new_map_title() : current_map();
        const std::string text = ui_printf(ui_format("AskSave", "Save the changes to %s?"), name.c_str());
        answer = ui_message_box(window, text.c_str(), ui_text("AskSaveTitle", "Editor"), MB_YESNOCANCEL | MB_ICONQUESTION);
    }
    if (answer == IDNO) {
        edit_saved();   // the changes are dropped: not asked again on the way out
        g_run_dirty = false;
        return true;
    }
    if (answer != IDYES) return false;
    if (g_running) {
        g_save_on_back = true;
        g_close_on_back = ending;
        stop_running();
        return false;
    }
    return save_current(window);
}

// File > Map size: the objects outside the new size go (asked first), the map is saved and its file
// resized (editor_newmap.cpp), then loaded again. A campaign map or the template is not resized in
// place: it is saved under a name of its own first. The editor's new map gets its name first.
bool resize_current(HWND window, int w, int h) {
    std::string map = game_path(current_map());
    if (auto_folder(folder_of(map))) {   // the new map: its name first
        map = choose_map(window, true);
        if (map.empty()) return false;
    }
    const std::string lower = [&] {
        std::string s = map;
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }();
    const bool campaign = lower.size() > 9 && lower.compare(0, 6, "maps\\l") == 0 && std::isdigit(static_cast<unsigned char>(lower[6])) &&
                          std::isdigit(static_cast<unsigned char>(lower[7])) && lower[8] == '\\';
    if (campaign || release_map(map)) {
        DialogScope scope;
        if (window && !g_test)
            ui_message_box(window,
                        ui_text("ResizeCampaign", "Save the map under a name of its own first (File > Save): the "
                                                  "campaign's maps, POLIGON and the template are not resized."),
                        ui_text("ResizeTitle", "Map size"), MB_OK | MB_ICONINFORMATION);
        patch::log("editor: map size: %s is not resized", map.c_str());
        return false;
    }
    const int outside = edit_outside(w, h, false);
    if (!g_test) {
        DialogScope scope;
        const std::string text = ui_printf(ui_format("ResizeAsk", "The map is saved and resized to %d x %d; %d objects "
                                                                  "outside it are deleted. This is not undone. Go on?"),
                                           w, h, outside);
        if (ui_message_box(window, text.c_str(), ui_text("ResizeTitle", "Map size"), MB_YESNO | MB_ICONQUESTION) != IDYES)
            return false;
    }
    edit_outside(w, h, true);
    if (!save_map(map)) return false;
    std::string error;
    if (!resize_map(map, w, h, error)) {
        patch::log("editor: map size: %s", error.c_str());
        if (!g_test) {
            DialogScope scope;
            ui_message_box(window, error.c_str(), ui_text("ResizeTitle", "Map size"), MB_OK | MB_ICONWARNING);
        }
        return false;
    }
    load_map(map);
    return true;
}

// ---- Export and Import (editor_exchange.cpp packs and unpacks) ----
//
// File > Export map packs the saved map of the custom maps page into a .zip (the Desktop by
// default); edits not saved are offered to be saved first, and a campaign map or the template is
// not exported (saved under a name of its own first). File > Import map unpacks a .zip into
// maps\<name>\: a folder of that name that is on the custom maps page is overwritten when the user
// says so, otherwise - or when the folder is the game's own (a campaign map, the template) - the
// map comes as <name>_2, _3 ...; it is added to the custom maps page and offered to be opened.

// A map of the game's own folders (the campaign's maps\lNN\, POLIGON and the template, which an update
// writes again): not saved over by Ctrl+S or Run the level, not resized, not exported, not
// overwritten by an import.
bool game_folder_map(const std::string& map) {
    std::string lower = map;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const bool campaign = lower.size() > 9 && lower.compare(0, 6, "maps\\l") == 0 && std::isdigit(static_cast<unsigned char>(lower[6])) &&
                          std::isdigit(static_cast<unsigned char>(lower[7])) && lower[8] == '\\';
    return campaign || release_map(map) || lower.compare(0, 12, "maps\\script\\") == 0;
}

bool asks() { return !g_test || g_test_ask; }

int ask(HWND window, const std::string& text, UINT buttons) {
    DialogScope scope;
    return ui_message_box(window, text.c_str(), ui_text("ExchangeTitle", "Map exchange"), buttons);
}

std::string desktop_folder() {
    char profile[MAX_PATH] = "";
    if (!GetEnvironmentVariableA("USERPROFILE", profile, MAX_PATH)) return patch::game_folder();
    const std::string desktop = std::string(profile) + "\\Desktop";
    return GetFileAttributesA(desktop.c_str()) == INVALID_FILE_ATTRIBUTES ? std::string(profile) : desktop;
}

// A .zip chosen in a Windows dialog (a full path); empty when cancelled.
std::string choose_package(HWND owner, bool save, const std::string& name) {
    return file_dialog(owner, save, desktop_folder(), save ? name + ".zip" : "",
                       save ? ui_text("ExportTitle", "Export map") : ui_text("ImportTitle", "Import map"),
                       ui_text("FilePackages", "Map packages (*.zip)"), L"*.zip", L"zip");
}

// File > Export map; `zip` given (test mode): no dialogs.
bool export_current(HWND window, const std::string& zip_given) {
    if (!g_held) return false;
    if (!ask_save(window)) return false;
    if (auto_folder(folder_of(game_path(current_map()))) && !save_current(window)) return false;   // the new map: named
    const std::string map = game_path(current_map());
    if (game_folder_map(map)) {
        patch::log("exchange: %s is a map of the game - not exported", map.c_str());
        if (asks())
            ask(window, ui_text("ExportGameMap", "Save the map under a name of its own first (File > Save): the campaign's maps, POLIGON and "
                                                "the template are not exported."),
                MB_OK | MB_ICONINFORMATION);
        return false;
    }
    const size_t slash = map.find_last_of('\\'), dot = map.find_last_of('.');
    const std::string name = map.substr(slash + 1, dot - slash - 1);
    const std::string zip = !zip_given.empty() ? zip_given : choose_package(window, true, name);
    if (zip.empty()) return false;
    std::string error;
    int files = 0;
    const bool done = exchange_export(map, zip, error, files);
    if (!done) patch::log("exchange: export failed: %s", error.c_str());
    if (asks())
        ask(window,
            done ? ui_printf(ui_format("ExportDone", "The map %s is packed into %s (%d files). Send the file; the other "
                                                     "player imports it with File > Import map."),
                             name.c_str(), zip.c_str(), files)
                 : ui_printf(ui_format("ExportFailed", "The map could not be exported: %s"), error.c_str()),
            MB_OK | (done ? MB_ICONINFORMATION : MB_ICONWARNING));
    return done;
}

enum class Import { Ask, Overwrite, Rename };

bool folder_exists(const std::string& name) { return GetFileAttributesA(("maps\\" + name).c_str()) != INVALID_FILE_ATTRIBUTES; }

// File > Import map; `zip` given (test mode): no file dialog, `mode` decides an existing folder.
bool import_package(HWND window, const std::string& zip_given, Import mode) {
    const std::string zip = !zip_given.empty() ? zip_given : choose_package(window, false, "");
    if (zip.empty()) return false;
    Package p;
    std::string error;
    if (!exchange_read(zip, p, error)) {
        patch::log("exchange: %s is not a map package: %s", zip.c_str(), error.c_str());
        if (asks()) ask(window, ui_printf(ui_format("ImportBad", "%s is not a map package: %s"), zip.c_str(), error.c_str()), MB_OK | MB_ICONWARNING);
        return false;
    }
    // the folder's name: small letters, digits and _, at most 24
    std::string name;
    for (char c : p.name)
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') name += c;
    if (name.empty()) name = "imported";
    name = name.substr(0, 21);
    std::string title = p.title;
    bool overwrite = false;
    const bool listed = !mission_listed_title(name).empty();
    if (folder_exists(name)) {
        Import choice = mode;
        if (!listed || game_folder_map("maps\\" + name + "\\")) {
            choice = Import::Rename;   // the game's own folder, or one not of the custom maps page: never overwritten
        } else if (mode == Import::Ask) {
            const int answer = ask(window,
                                   ui_printf(ui_format("ImportExists", "There is a map %s (%s) already. Overwrite it?\n\n"
                                                                       "Yes: overwrite it.  No: import this one as a new map.  Cancel: "
                                                                       "do not import."),
                                             name.c_str(), mission_listed_title(name).c_str()),
                                   MB_YESNOCANCEL | MB_ICONQUESTION);
            if (answer == IDCANCEL) return false;
            choice = answer == IDYES ? Import::Overwrite : Import::Rename;
        }
        if (choice == Import::Overwrite) {
            overwrite = true;
        } else {
            int k = 2;
            while (folder_exists(name + "_" + std::to_string(k))) ++k;
            name += "_" + std::to_string(k);
            title = title.substr(0, 21) + " " + std::to_string(k);
        }
    }
    const std::string map = exchange_install(p, name, overwrite, error);
    if (map.empty()) {
        patch::log("exchange: import failed: %s", error.c_str());
        if (asks()) ask(window, ui_printf(ui_format("ImportFailed", "The map could not be imported: %s"), error.c_str()), MB_OK | MB_ICONWARNING);
        return false;
    }
    mission_list_map(name, title);
    patch::log("exchange: imported %s as %s (%s)%s", zip.c_str(), map.c_str(), title.c_str(),
               exchange_campaign_ground(p) ? ", on a campaign terrain" : ", with its own ground");
    bool open = !asks();
    if (asks())
        open = ask(window,
                   ui_printf(ui_format("ImportDone", "The map is imported as %s (%s) and is on the custom maps page. Open it in the "
                                                     "editor now?"),
                             map.c_str(), title.c_str()),
                   MB_YESNO | MB_ICONQUESTION) == IDYES;
    if (open && ask_save(window)) load_map(map);
    return true;
}

void handle_command(HWND window, int id) {
    if (g_in_dialog) return;   // a dialog's message loop dispatches to the game window too
    // while the level runs only Ctrl+R (the accelerators still post the rest): a load or a save would
    // take the running state, and a load would leave Run the level's copy that the way back holds
    if (g_running && id != ID_RUN && id != ID_ABOUT) return;
    if (id == ID_LOAD) {
        if (!ask_save(window)) return;
        const std::string map = choose_map(window, false);
        if (!map.empty()) load_map(map);
    } else if (id == ID_SAVE) {
        const std::string map = choose_map(window, true);
        if (!map.empty()) save_map(map);
    } else if (id == ID_NEW) {
        if (!ask_save(window)) return;
        DialogScope scope;
        new_map_dialog(window);
    } else if (id == ID_RESIZE) {
        if (!g_held) return;
        int w, h;
        edit_map_size(w, h);
        {
            DialogScope scope;
            if (!map_size_dialog(window, w, h)) return;
        }
        resize_current(window, w, h);
    } else if (id == ID_MISSION) {
        if (!g_held) return;
        bool arrivals = false, plan = false;
        const ArrivalsState arrivals_before = mission_arrivals();
        const FactoryState factories_before = mission_factories();
        {
            DialogScope scope;
            if (mission_dialog(window, &arrivals, &plan)) {
                edit_touch();
                edit_commit_mission(arrivals_before, factories_before);   // the queue is in the history too
            }
        }
        if (arrivals) panel_open_arrivals();   // its Arrivals... button
        if (plan) edit_open_enemy_factory();   // its Plan... button
    } else if (id == ID_EXPORT) {
        export_current(window, "");
    } else if (id == ID_IMPORT) {
        import_package(window, "", Import::Ask);
    } else if (id == ID_ABOUT) {
        about(window);
    } else if (id == ID_RUN) {
        if (g_running) stop_running();
        else run_level(window);
    } else if (id == ID_PLAY) {
        // the new map gets its name first: the played game must not run from the scratch folder
        if (auto_folder(folder_of(game_path(current_map())))) {
            if (!save_current(window)) return;
        } else if (!ask_save(window)) {
            return;
        }
        g_playing = true;   // the map stays for the game that plays it
        g_playing = restart_game(false, current_map(), true);   // back here when it ends
    } else if (id == ID_BACK) {
        if (!ask_save(window)) return;
        leave_map("");
        restart_game(false, "", false);
    } else if (id >= ID_UNDO && id <= ID_SEMAPHORE) {
        if (g_held) edit_command(id);
    }
}

LRESULT CALLBACK editor_wndproc(HWND window, UINT m, WPARAM w, LPARAM l) {
    // Ctrl+R runs the level or holds it again: the editor's bar and its menu button are not there
    // while it runs (Options > Run the level does the same)
    // (a test posts the Ctrl key's own messages, which GetKeyState does not see)
    if ((m == WM_KEYDOWN || m == WM_KEYUP) && w == VK_CONTROL) g_ctrl = m == WM_KEYDOWN;
    if (m == WM_KEYDOWN && w == 'R' && !(l & 0x40000000) && (g_ctrl || GetKeyState(VK_CONTROL) < 0) && !g_in_dialog &&
        !g_in_menu) {
        handle_command(window, ID_RUN);
        return 0;
    }
    // Ctrl+S saves the map (save_current); File > Save (Alt+F2) always asks for the name
    if (m == WM_KEYDOWN && w == 'S' && !(l & 0x40000000) && (g_ctrl || GetKeyState(VK_CONTROL) < 0) && !g_in_dialog &&
        !g_in_menu) {
        if (g_held) save_current(window);
        return 0;
    }
    if (m == WM_SAVE_AFTER_RUN) {
        if (g_held && !g_in_dialog && save_current(window) && w) PostMessageA(window, WM_CLOSE, 0, 0);
        return 0;
    }
    if (g_held && !g_in_dialog && !g_in_menu && edit_message(window, m, w, l)) return 0;
    switch (m) {
    case WM_CLOSE:
        if (g_in_dialog) break;
        if (!ask_save(window, true)) return 0;
        leave_map("");
        break;
    case WM_COMMAND:
        if (HIWORD(w) <= 1 && (LOWORD(w) == ID_LOAD || LOWORD(w) == ID_SAVE || LOWORD(w) == ID_ABOUT ||
                               (LOWORD(w) >= ID_RUN && LOWORD(w) <= ID_IMPORT) ||
                               (LOWORD(w) >= ID_UNDO && LOWORD(w) <= ID_SEMAPHORE))) {
            handle_command(window, LOWORD(w));
            return 0;
        }
        if (HIWORD(w) <= 1 && LOWORD(w) == ID_EXIT) {
            if (g_in_dialog || !ask_save(window, true)) return 0;
            leave_map("");
        }
        break;
    case WM_ENTERMENULOOP:
    case WM_EXITMENULOOP:
        g_in_menu = m == WM_ENTERMENULOOP;
        update_cursor();
        break;
    case WM_SETCURSOR:
        g_over_frame = LOWORD(l) != HTCLIENT;
        update_cursor();
        break;
    case WM_COPYDATA:
        if (g_test && !g_in_dialog) {
            const auto* data = reinterpret_cast<const COPYDATASTRUCT*>(l);
            const char* text = static_cast<const char*>(data->lpData);
            const std::string path(text, text ? strnlen(text, data->cbData) : 0);
            if (data->dwData == ID_SAVE) {
                const bool saved = save_map(path);
                patch::log("editor test: save %s %s", path.c_str(), saved ? "ok" : "failed");
                return saved;
            }
            if (data->dwData == ID_LOAD) {
                load_map(path);
                return TRUE;
            }
            if (data->dwData == TEST_NEW_MAP) return new_map_for_test(path) ? TRUE : FALSE;
            if (data->dwData == TEST_RESIZE) {   // "w h": File > Map size without the dialogs
                int w = 0, h = 0;
                if (std::sscanf(path.c_str(), "%d %d", &w, &h) != 2 || !g_held) return FALSE;
                const bool english = ui_english(true);   // LocoFix.log in English
                const bool resized = resize_current(window, w, h);
                ui_english(english);
                return resized ? TRUE : FALSE;
            }
            if (data->dwData == TEST_EXPORT) {
                const bool english = ui_english(true);
                const bool done = export_current(window, path);
                ui_english(english);
                return done ? TRUE : FALSE;
            }
            if (data->dwData == TEST_IMPORT) {   // "<zip>|overwrite|rename|ask"
                const size_t bar = path.find('|');
                const std::string mode = bar == std::string::npos ? "rename" : path.substr(bar + 1);
                const bool english = ui_english(mode != "ask");
                const bool done = import_package(window, path.substr(0, bar),
                                                 mode == "overwrite" ? Import::Overwrite : mode == "ask" ? Import::Ask : Import::Rename);
                ui_english(english);
                return done ? TRUE : FALSE;
            }
            if (data->dwData == TEST_MISSION) {
                const ArrivalsState arrivals_before = mission_arrivals();
                const FactoryState factories_before = mission_factories();
                if (!mission_settings_from_file(path)) return FALSE;
                edit_touch();
                edit_commit_mission(arrivals_before, factories_before);
                edit_assign_orders();   // its order: lines
                return TRUE;
            }
            if (data->dwData == TEST_FACTORY) {   // "x y": the panel of the factory there
                float x = 0, y = 0;
                return std::sscanf(path.c_str(), "%f %f", &x, &y) == 2 && edit_open_factory_at(x, y) ? TRUE : FALSE;
            }
            if (data->dwData == TEST_RAIL_POINTS) {   // "x y": the rail points there into the log
                float x = 0, y = 0;
                if (std::sscanf(path.c_str(), "%f %f", &x, &y) != 2 || !world()) return FALSE;
                edit_log_rail_points(x, y);
                return TRUE;
            }
            if (data->dwData == TEST_VIEW) {   // "x y": the top-left of the view
                float x = 0, y = 0;
                if (std::sscanf(path.c_str(), "%f %f", &x, &y) != 2 || !world() || !graphics()) return FALSE;
                const float* screen = reinterpret_cast<const float*>(static_cast<uint8_t*>(graphics()) + 0x20c);
                reinterpret_cast<SetPosition_t>(SET_POSITION)(world(), nullptr, x + screen[0] / 2, y + screen[1] / 2, 0);
                test_cursor_to_middle(window);   // or the edge scrolling moves the view on
                return TRUE;
            }
        }
        break;
    }
    return CallWindowProcA(g_game_wndproc, window, m, w, l);
}

// The window class without the exe's menu (AppMenu): the editor has no window menu. Its commands
// are in the Menu panel of the bar (editor_panel.cpp), which posts the same WM_COMMAND ids, and the
// game's accelerators (AppAccel: Alt+F2, Alt+F3, Alt+F10) post them too. The menu bar did not
// open in a window bigger than the map, and in full screen it pushed the game area below the
// picture, so every click landed under the cursor. Without it in the class the window is made
// at its size (window.cpp counts a class menu in), so the game area stays the resolution.
ATOM WINAPI RegisterClassA_editor(const WNDCLASSA* cls) {
    WNDCLASSA copy = *cls;
    g_game_wndproc = copy.lpfnWndProc;
    copy.lpfnWndProc = &editor_wndproc;
    copy.lpszMenuName = nullptr;
    return g_register_class(&copy);
}

}  // namespace

// restart.cpp: script StartEditorMode in editor mode loads the map.
bool editor_mode_on() { return g_enabled; }
void editor_load_map(const std::string& map) { load_map(map); }
// restart.cpp's StartEditorMode and test_map.cpp's --test-map in editor mode: the template opens as a
// new empty map ("the map the editor starts on").
std::string editor_start_map(const std::string& map) { return start_map(map); }

// frame_limit.cpp's Level_Load wrapper: a held level is let go for the load (the flags would
// otherwise carry the old level's pause time over) and the new one is held once it is loaded.
void editor_level_loading(void* w) {
    if (g_enabled) edit_reset();   // the objects of the old level go away
    if (!g_held) return;
    world_flags(static_cast<uint8_t*>(w)) &= ~(PAUSED | SCRIPTS_OFF);
    g_held = false;
}

// frame_limit.cpp's Level_Load wrapper, before the load: while the level runs (Run the level), the
// same map again (RESTART) runs; any other - the menu after ABORT GAME, a won or lost level's next
// map - is the editor's map again, held.
const char* editor_level_redirect(const char* map) {
    if (!g_enabled || !g_running || !map) return map;
    if (lower_path(game_path(map)) == lower_path(g_run_map)) return map;
    // the name is the game's string, which the load frees: the map's name in one of its own
    char* name = reinterpret_cast<New_t>(CRT_NEW)(g_run_map.size() + 1);
    if (!name) return map;
    std::memcpy(name, g_run_map.c_str(), g_run_map.size() + 1);
    g_running = false;
    stop_music();
    patch::log("editor: the run left for %s - %s is held again", map, g_run_map.c_str());
    if (reinterpret_cast<uintptr_t>(map) != EMPTY_STRING) reinterpret_cast<Delete_t>(CRT_DELETE)(map);
    return name;
}

void editor_level_loaded(void* w) {
    auto* world = static_cast<uint8_t*>(w);
    const char* map = *reinterpret_cast<const char**>(world + WORLD_LEVEL_FILE);
    if (g_enabled && !g_running && current_format(map)) {
        // the map Ctrl+S saves into is the one loaded, whichever way it came (File > Load, a mission
        // picked in the game's menus, StartEditorMode); the new map of the start keeps its scratch path,
        // Run the level's copy held again the map it stands for (below)
        g_current = game_path(map);
        mission_load(game_path(map));
        hold_level(world, true);
        if (g_run_back) {   // back from Run the level: the view and the chosen object as they were
            g_run_back = false;
            if (graphics()) {
                const float* screen = reinterpret_cast<const float*>(static_cast<uint8_t*>(graphics()) + 0x20c);
                reinterpret_cast<SetPosition_t>(SET_POSITION)(world, nullptr, g_run_view_x + screen[0] / 2, g_run_view_y + screen[1] / 2, 0);
            }
            edit_restore_history();
            edit_restore_selection();
            if (!g_run_origin.empty()) {   // the copy held again: the editor on the original map, its edits not saved
                g_current = g_run_origin;
                if (g_run_dirty) edit_touch();
                patch::log("editor: %s held again for %s%s", g_run_map.c_str(), g_run_origin.c_str(), g_run_dirty ? " (not saved)" : "");
            }
            // the edits of the copy were to be saved while it ran (the game ending): saved now
            HWND window = *reinterpret_cast<HWND*>(world + WORLD_HWND);
            if (g_save_on_back && window) PostMessageA(window, WM_SAVE_AFTER_RUN, g_close_on_back, 0);
            g_save_on_back = false;
        } else {
            g_run_origin.clear();
            // the start's new map fills the view from its top-left corner (a new map's first view
            // would show it 300 px into the void around it)
            if (auto_folder(folder_of(game_path(map))) && graphics()) {
                const float* screen = reinterpret_cast<const float*>(static_cast<uint8_t*>(graphics()) + 0x20c);
                reinterpret_cast<SetPosition_t>(SET_POSITION)(world, nullptr, screen[0] / 2, screen[1] / 2, 0);
                patch::log("editor: the view at %.0f,%.0f", *reinterpret_cast<float*>(world + 0x44), *reinterpret_cast<float*>(world + 0x48));
            }
        }
    }
}

// editor_edit.cpp: the bar's Mission settings entry.
void editor_mission_settings() {
    uint8_t* w = world();
    if (HWND window = w ? *reinterpret_cast<HWND*>(w + WORLD_HWND) : nullptr) handle_command(window, ID_MISSION);
}

// editor_mission.cpp: the editor's new map, not saved yet (its folder is the scratch one), and the
// title it has until then (ASCII capitals: the custom maps page's and the script's).
bool editor_unsaved_map(const std::string& map) { return auto_folder(folder_of(game_path(map))); }
std::string editor_new_map_title() { return ui_text("NewMapTitle", "NEW MAP"); }

// mouse_pan.cpp pans a held level although it is paused.
bool editor_level_held() { return g_held; }

// restart.cpp: the Esc menu's BACK TO THE EDITOR while the level runs inside the editor's own game
// (Run the level) holds the level again, as Ctrl+R does.
bool editor_level_running() { return g_enabled && g_running; }
void editor_hold_level() {
    uint8_t* w = world();
    if (!editor_level_running() || !w) return;
    stop_running();
}

// Install after screen scaling: the RegisterClassA chain is editor -> screen scaling -> test
// mode -> the game, so the menu commands are seen first.
void install_editor() {
    if (!patch::take_switch("--editor", "-editor")) return;
    char value[8] = {};
    g_test = GetEnvironmentVariableA("LOCOFIX_TEST", value, sizeof value) && value[0] == '1';
    g_test_ask = GetEnvironmentVariableA("LOCOFIX_TEST_ASK", value, sizeof value) && value[0] == '1';

    const uint8_t read_flags[] = {0x8B, 0x91, 0x20, 0x02, 0x00, 0x00};   // mov edx,[ecx+0x220]
    const uint8_t menu_name[] = {0x25, 0xE8, 0xB7, 0x47, 0x00};           // and eax,0x47b7e8
    if (!patch::check(READ_SETTINGS_FLAGS, read_flags, sizeof read_flags) ||
        !patch::check(MENU_NAME, menu_name, sizeof menu_name)) {
        patch::log("skipped editor mode: unexpected bytes");
        return;
    }
    const uint8_t pause[] = {0x8A, 0x51, 0x04, 0xB8, 0x10, 0x00, 0x00, 0x00};   // World_Pause: mov dl,[ecx+4] / mov eax,0x10
    const uint8_t resume[] = {0xF6, 0x41, 0x04, 0x10};                         // World_Resume: test byte [ecx+4],0x10
    if (!patch::check(WORLD_PAUSE, pause, sizeof pause) || !patch::check(WORLD_RESUME, resume, sizeof resume)) {
        patch::log("skipped editor mode: unexpected bytes");
        return;
    }
    g_enabled = true;
    const uint8_t flags = *reinterpret_cast<uint8_t*>(G_SETTINGS_FLAGS) | 1;
    patch::write(G_SETTINGS_FLAGS, &flags, 1);
    g_register_class = *reinterpret_cast<RegisterClassA_t*>(IAT_RegisterClassA);
    void* hook_fn = reinterpret_cast<void*>(&RegisterClassA_editor);
    patch::write(IAT_RegisterClassA, &hook_fn, sizeof hook_fn);
    install_editor_edit();
    patch::log("editor mode: on%s", g_test ? " (test commands by WM_COPYDATA)" : "");
}
