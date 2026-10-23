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
#include <commdlg.h>
#include <cctype>
#include <cstdio>

bool level_save_succeeded();   // level_save.cpp
bool restart_game(bool editor, const std::string& map, bool back_to_editor);   // restart.cpp
// editor_edit.cpp: editing the objects of the held level
bool edit_message(HWND window, UINT m, WPARAM w, LPARAM l);
void edit_command(int id);
void edit_reset();
bool edit_dirty();
void edit_saved();
void install_editor_edit();
void edit_level_held();
void edit_touch();
void new_map_dialog(HWND owner);                // editor_newmap.cpp
bool new_map_for_test(const std::string& text);  // editor_newmap.cpp
bool map_size_dialog(HWND owner, int& w, int& h);   // editor_newmap.cpp
bool resize_map(const std::string& map, int w, int h, std::string& error);   // editor_newmap.cpp
void edit_map_size(int& w, int& h);              // editor_edit.cpp
int edit_outside(int w, int h, bool remove);     // editor_edit.cpp
void edit_log_rail_points(float x, float y);

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
constexpr uintptr_t SET_POSITION = 0x41c680;   // SetPosition: the centre of the view, clamped

using GdiMode_t = void(__fastcall*)(void* graphics, void* edx);
using SetPosition_t = void(__fastcall*)(void* world, void* edx, float x, float y, int mode);
using PauseResume_t = void(__fastcall*)(uint8_t* world, void* edx);
using AssignCStr_t = char**(__fastcall*)(char** string, void* edx, const char* text);
using SaveLevel_t = void(__fastcall*)(uint8_t* world, void* edx, char* name);
using New_t = char*(__cdecl*)(size_t size);
using RegisterClassA_t = ATOM(WINAPI*)(const WNDCLASSA*);
using FileDialog_t = BOOL(WINAPI*)(OPENFILENAMEW*);

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
        edit_level_held();
    } else {
        world_flags(w) &= ~SCRIPTS_OFF;
        reinterpret_cast<PauseResume_t>(WORLD_RESUME)(w, nullptr);
    }
}

void set_running(HWND window, bool running) {
    g_running = running;
    uint8_t* w = world();
    if (w && running && g_held) hold_level(w, false);
    else if (w && !running && !g_held && current_format(level_file())) hold_level(w, true);
    patch::log("editor: the level %s", running ? "runs" : "is held");
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

// ---- Load and Save ----

void load_map(const std::string& path) {
    uint8_t* w = world();
    if (!w) return;
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
    edit_saved();
    // a painted ground: its own image next to the map, the map pointing at it
    ground_save(game_path(path));
    // its mission: the script (a new map gets one), the texts, the custom maps page
    mission_save(game_path(path));
    return true;
}

FileDialog_t dialog_function(const char* name) {
    static HMODULE comdlg = LoadLibraryA("comdlg32.dll");
    return comdlg ? reinterpret_cast<FileDialog_t>(GetProcAddress(comdlg, name)) : nullptr;
}

std::wstring wide_acp(const std::string& s) {
    std::wstring w(static_cast<size_t>(MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, nullptr, 0)), L'\0');
    MultiByteToWideChar(CP_ACP, 0, s.c_str(), -1, &w[0], static_cast<int>(w.size()));
    if (!w.empty()) w.pop_back();
    return w;
}

// A Windows open or save dialog (the W one: its title and file types are the pack's texts, shown
// right in any system code page); the file chosen as a path in the system's code page, empty when
// cancelled. `kind` names the files of `pattern` ("*.map"), `ext` the default extension.
std::string file_dialog(HWND owner, bool save, const std::string& folder, const std::string& file, const char* title,
                        const char* kind, const wchar_t* pattern, const wchar_t* ext) {
    const FileDialog_t show = dialog_function(save ? "GetSaveFileNameW" : "GetOpenFileNameW");
    if (!show) {
        patch::log("editor: comdlg32.dll is not available");
        return "";
    }
    std::wstring filter = ui_wide(kind);
    filter += L'\0';
    filter += pattern;
    filter += L'\0';
    filter += ui_wide(ui_text("FileAll", "All files (*.*)"));
    filter += L'\0';
    filter += L"*.*";
    filter += L'\0';
    filter += L'\0';
    const std::wstring initial = wide_acp(folder), caption = ui_wide(title);
    wchar_t name[MAX_PATH] = L"";
    lstrcpynW(name, wide_acp(file).c_str(), MAX_PATH);
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof ofn;
    ofn.hwndOwner = owner;
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = name;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = initial.c_str();
    ofn.lpstrTitle = caption.c_str();
    ofn.lpstrDefExt = ext;
    ofn.Flags = OFN_NOCHANGEDIR | OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    DialogScope scope;
    if (!show(&ofn)) return "";
    char path[MAX_PATH * 2] = "";
    WideCharToMultiByte(CP_ACP, 0, name, -1, path, sizeof path, nullptr, nullptr);
    return path;
}

// A file chosen in a Windows dialog, as a game path; empty when cancelled.
std::string choose_map(HWND owner, bool save) {
    const std::string current = full_path(g_current.empty() ? level_file() : g_current);
    const size_t slash = current.find_last_of('\\');
    std::string folder = slash == std::string::npos ? "" : current.substr(0, slash);
    if (folder.empty() || GetFileAttributesA(folder.c_str()) == INVALID_FILE_ATTRIBUTES)
        folder = patch::game_folder() + "maps";
    const std::string file = slash == std::string::npos ? "" : current.substr(slash + 1);
    const std::string chosen = file_dialog(owner, save, folder, file,
                                           save ? ui_text("FileSaveTitle", "Save map") : ui_text("FileLoadTitle", "Load map"),
                                           ui_text("FileMaps", "Maps (*.map)"), L"*.map", L"map");
    return chosen.empty() ? "" : game_path(chosen);
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

// Before the level goes away: with edits not saved, asks whether to save them. False when the
// user cancels (or the save fails). Tests are never asked.
bool ask_save(HWND window) {
    if (!edit_dirty() || (g_test && !g_test_ask)) return true;
    int answer;
    {
        DialogScope scope;
        const std::string text = ui_printf(ui_format("AskSave", "Save the changes to %s?"), current_map().c_str());
        answer = ui_message_box(window, text.c_str(), ui_text("AskSaveTitle", "Editor"), MB_YESNOCANCEL | MB_ICONQUESTION);
    }
    if (answer == IDNO) {
        edit_saved();   // the changes are dropped: not asked again on the way out
        return true;
    }
    if (answer != IDYES) return false;
    const std::string map = choose_map(window, true);
    return !map.empty() && save_map(map);
}

// File > Map size: the objects outside the new size go (asked first), the map is saved and its file
// resized (editor_newmap.cpp), then loaded again. A campaign map or the template is not resized in
// place: it is saved under a name of its own first.
bool resize_current(HWND window, int w, int h) {
    const std::string map = game_path(current_map());
    const std::string lower = [&] {
        std::string s = map;
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }();
    const bool campaign = lower.size() > 9 && lower.compare(0, 6, "maps\\l") == 0 && std::isdigit(static_cast<unsigned char>(lower[6])) &&
                          std::isdigit(static_cast<unsigned char>(lower[7])) && lower[8] == '\\';
    if (campaign || lower.compare(0, 14, "maps\\template\\") == 0) {
        DialogScope scope;
        if (window && !g_test)
            ui_message_box(window,
                        ui_text("ResizeCampaign", "Save the map under a name of its own first (File > Save): the "
                                                  "campaign's maps and the template are not resized."),
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

// A map of the game's own folders (the campaign's maps\lNN\, the template): not exported, not
// overwritten by an import.
bool game_folder_map(const std::string& map) {
    std::string lower = map;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    const bool campaign = lower.size() > 9 && lower.compare(0, 6, "maps\\l") == 0 && std::isdigit(static_cast<unsigned char>(lower[6])) &&
                          std::isdigit(static_cast<unsigned char>(lower[7])) && lower[8] == '\\';
    return campaign || lower.compare(0, 14, "maps\\template\\") == 0 || lower.compare(0, 12, "maps\\script\\") == 0;
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
    const std::string map = game_path(current_map());
    if (game_folder_map(map)) {
        patch::log("exchange: %s is a map of the game - not exported", map.c_str());
        if (asks())
            ask(window, ui_text("ExportGameMap", "Save the map under a name of its own first (File > Save): the campaign's maps and "
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
        DialogScope scope;
        if (mission_dialog(window)) edit_touch();
    } else if (id == ID_EXPORT) {
        export_current(window, "");
    } else if (id == ID_IMPORT) {
        import_package(window, "", Import::Ask);
    } else if (id == ID_ABOUT) {
        about(window);
    } else if (id == ID_RUN) {
        set_running(window, !g_running);
    } else if (id == ID_PLAY) {
        if (ask_save(window)) restart_game(false, current_map(), true);   // back here when it ends
    } else if (id == ID_BACK) {
        if (ask_save(window)) restart_game(false, "", false);
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
    if (g_held && !g_in_dialog && !g_in_menu && edit_message(window, m, w, l)) return 0;
    switch (m) {
    case WM_CLOSE:
        if (!g_in_dialog && !ask_save(window)) return 0;
        break;
    case WM_COMMAND:
        if (HIWORD(w) <= 1 && (LOWORD(w) == ID_LOAD || LOWORD(w) == ID_SAVE || LOWORD(w) == ID_ABOUT ||
                               (LOWORD(w) >= ID_RUN && LOWORD(w) <= ID_IMPORT) ||
                               (LOWORD(w) >= ID_UNDO && LOWORD(w) <= ID_SEMAPHORE))) {
            handle_command(window, LOWORD(w));
            return 0;
        }
        if (HIWORD(w) <= 1 && LOWORD(w) == ID_EXIT && (g_in_dialog || !ask_save(window))) return 0;
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
            if (data->dwData == TEST_MISSION) return mission_settings_from_file(path) ? (edit_touch(), TRUE) : FALSE;
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

// frame_limit.cpp's Level_Load wrapper: a held level is let go for the load (the flags would
// otherwise carry the old level's pause time over) and the new one is held once it is loaded.
void editor_level_loading(void* w) {
    if (g_enabled) edit_reset();   // the objects of the old level go away
    if (!g_held) return;
    world_flags(static_cast<uint8_t*>(w)) &= ~(PAUSED | SCRIPTS_OFF);
    g_held = false;
}

void editor_level_loaded(void* w) {
    auto* world = static_cast<uint8_t*>(w);
    const char* map = *reinterpret_cast<const char**>(world + WORLD_LEVEL_FILE);
    if (g_enabled && !g_running && current_format(map)) {
        mission_load(game_path(map));
        hold_level(world, true);
    }
}

// editor_edit.cpp: the bar's Mission settings entry.
void editor_mission_settings() {
    uint8_t* w = world();
    if (HWND window = w ? *reinterpret_cast<HWND*>(w + WORLD_HWND) : nullptr) handle_command(window, ID_MISSION);
}

// mouse_pan.cpp pans a held level although it is paused.
bool editor_level_held() { return g_held; }

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
