// Player options of the launcher: "Cheats" (the developer keys) and "All missions" (every
// mission of the campaign's strategic map playable). Both are off by default; the launcher gets
// a row of two boxes under the frame limit, OK saves them in the registry next to the game's
// settings (Cheats, UnlockMissions; DWORD 0/1) and the installer writes their first values.
//
// The scripts read the options at compile time, which is every level load: two files of ours
// in maps\script hold the "off" state - cheats.lgc defines nothing (the original game, whose
// dokeys.lgc tests #ifdef KEYS_ENABLE) and unlock.lgc sets option_unlock_all to 0 - and next to them
// cheats_on.lgc (#define KEYS_ENABLE 1) and unlock_on.lgc (option_unlock_all 1). constants.lgc includes
// cheats.lgc and menu.lgc unlock.lgc (game/ and lang/<code>/ patches); when an option is on, the
// game's CreateFileA of the "off" file opens the "on" one instead. Nothing the player saved is
// touched: the menu shows the later missions playable while the stored progress (l<player> under
// Players) stays what it was, and menuend.lgc advances it only for the next mission in line.
//
// Under them the Minimap row: the minimap's size in play (minimap.cpp) - the original, twice or
// three times as big - in the registry value MinimapSize (DWORD 2 or 3; the original size takes
// the value away, so absent = the original, and Setup writes nothing for it). Only the sizes that
// fit the resolution chosen above are offered (minimap_size_fits: the frame must stay clear of the
// state bar's slots), the list follows a change of the resolution, and a size that no longer fits
// becomes the biggest that does. A note under it says what a bigger one does to the view.
//
// The launcher's caption also gets the OpenLocoland version (LOCOFIX_VERSION, set by
// tools/release.py; "dev" for a build of build.py).
#include "patch.h"
#include <cstdlib>

void add_launcher_label(int id, const char* key);   // language.cpp
void add_launcher_relabel(void (*relabel)(HWND dialog));
std::wstring launcher_text(const char* key, const wchar_t* fallback);
void minimap_set_size(int size);                    // minimap.cpp
bool minimap_size_fits(int size, int screen_w);

namespace {

constexpr uintptr_t IAT_CreateFileA = 0x4750f8;
constexpr uintptr_t IAT_DialogBoxParamA = 0x4751ac;
constexpr char REG_KEY[] = "SOFTWARE\\Gromada\\Locoland";   // RegPath in Locoland.cfg
constexpr int ID_CHEATS = 1120;
constexpr int ID_UNLOCK = 1121;
constexpr int ID_RESOLUTION = 1001;
constexpr int ID_MINIMAP_LABEL = 1122;
constexpr int ID_MINIMAP = 1123;
constexpr int ID_MINIMAP_NOTE = 1124;
constexpr int ROW_Y = 208;        // dialog units: under the frame limit's row (frame_limit.cpp, 194)
constexpr int MINIMAP_Y = 222;    // the Minimap row; its note takes three lines under it
constexpr int ROW_HEIGHT = 56;    // dialog units added to the launcher (update_check.cpp's row follows at 264)
constexpr int MINIMAP_SIZES = 3;

struct Option {
    const char* value;   // registry value
    const char* file;    // the script file of the "off" state, relative to the game folder
    bool on;
};

Option g_options[] = {
    {"Cheats", "maps\\script\\cheats.lgc", false},
    {"UnlockMissions", "maps\\script\\unlock.lgc", false},
};
Option& g_cheats = g_options[0];
Option& g_unlock = g_options[1];
int g_minimap = 1;   // MinimapSize, 1 = the original size

bool read_option(const char* name) {
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return false;
    DWORD value = 0, size = sizeof value, type = 0;
    const LONG r = RegQueryValueExA(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS && type == REG_DWORD && value != 0;
}

int read_minimap() {
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return 1;
    DWORD value = 0, size = sizeof value, type = 0;
    const LONG r = RegQueryValueExA(key, "MinimapSize", nullptr, &type, reinterpret_cast<BYTE*>(&value), &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS && type == REG_DWORD && value >= 1 && value <= MINIMAP_SIZES ? static_cast<int>(value) : 1;
}

void save_minimap() {
    HKEY key;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS)
        return;
    if (g_minimap > 1) {
        const DWORD value = g_minimap;
        RegSetValueExA(key, "MinimapSize", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof value);
    } else {
        RegDeleteValueA(key, "MinimapSize");   // absent = the original size
    }
    RegCloseKey(key);
}

void save_options() {
    HKEY key;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS)
        return;
    for (const Option& o : g_options) {
        const DWORD value = o.on;
        RegSetValueExA(key, o.value, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof value);
    }
    RegCloseKey(key);
}

void log_options() {
    patch::log("options: cheats %s, all missions %s", g_cheats.on ? "on" : "off", g_unlock.on ? "on" : "off");
}

// ---- files ----

using CreateFileA_t = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
CreateFileA_t g_create_file = nullptr;   // language.cpp's hook, or the system function

// `path` with "_on" before the extension when it names the file of an option that is on: the
// path the script compiler opens, relative to the game folder or absolute in it.
std::string option_file(const char* path) {
    if (!path) return {};
    std::string p = path;
    for (char& c : p)
        if (c == '/') c = '\\';
    for (const Option& o : g_options) {
        const size_t n = std::strlen(o.file);
        if (!o.on || p.size() < n || _stricmp(p.c_str() + p.size() - n, o.file) != 0) continue;
        if (p.size() > n && p[p.size() - n - 1] != '\\') continue;
        return p.substr(0, p.size() - 4) + "_on" + p.substr(p.size() - 4);
    }
    return {};
}

HANDLE WINAPI CreateFileA_options(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                                  DWORD disposition, DWORD flags, HANDLE template_file) {
    std::string variant;
    if (disposition == OPEN_EXISTING) variant = option_file(name);
    return g_create_file(variant.empty() ? name : variant.c_str(), access, share, security, disposition, flags,
                         template_file);
}

// ---- launcher ----

using DialogBoxParamA_t = INT_PTR(WINAPI*)(HINSTANCE, LPCSTR, HWND, DLGPROC, LPARAM);
DialogBoxParamA_t g_dialog_box = nullptr;   // frame_limit.cpp's hook
DLGPROC g_launcher_proc = nullptr;

RECT dialog_rect(HWND dialog, int x, int y, int w, int h) {
    RECT r = {x, y, x + w, y + h};
    MapDialogRect(dialog, &r);
    return r;
}

HWND add_check_box(HWND dialog, const wchar_t* text, RECT r, int id, bool checked) {
    HWND box = CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, r.left,
                               r.top, r.right - r.left, r.bottom - r.top, dialog,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                               reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(dialog, GWLP_HINSTANCE)), nullptr);
    SendMessageW(box, WM_SETFONT, SendMessageW(dialog, WM_GETFONT, 0, 0), FALSE);
    SendMessageW(box, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
    return box;
}

// Moves a control down by `dy` pixels.
void move_down(HWND dialog, int id, int dy) {
    HWND control = GetDlgItem(dialog, id);
    RECT r;
    GetWindowRect(control, &r);
    MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&r), 2);
    SetWindowPos(control, nullptr, r.left, r.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

HWND add_control(HWND dialog, const wchar_t* cls, const wchar_t* text, DWORD style, RECT r, int id) {
    HWND control = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, r.left, r.top, r.right - r.left,
                                   r.bottom - r.top, dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(dialog, GWLP_HINSTANCE)), nullptr);
    SendMessageW(control, WM_SETFONT, SendMessageW(dialog, WM_GETFONT, 0, 0), FALSE);
    return control;
}

// The width of the resolution chosen in the launcher's list ("1920 x 1080"), 0 when unknown.
int chosen_width(HWND dialog) {
    HWND list = GetDlgItem(dialog, ID_RESOLUTION);
    const auto i = list ? SendMessageA(list, CB_GETCURSEL, 0, 0) : CB_ERR;
    if (i == CB_ERR || SendMessageA(list, CB_GETLBTEXTLEN, i, 0) >= 64) return 0;
    char text[64] = {};
    SendMessageA(list, CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(text));
    return std::atoi(text);
}

// The Minimap list: the sizes that fit the chosen resolution, the wanted one (or the biggest
// that fits below it) chosen; called again for a new resolution and a new language.
void fill_minimap(HWND dialog) {
    HWND list = GetDlgItem(dialog, ID_MINIMAP);
    if (!list) return;
    int wanted = g_minimap;
    const auto current = SendMessageW(list, CB_GETCURSEL, 0, 0);
    if (current != CB_ERR) wanted = static_cast<int>(SendMessageW(list, CB_GETITEMDATA, current, 0));
    const int width = chosen_width(dialog);
    static const char* const KEYS[] = {"minimap_1", "minimap_2", "minimap_3"};
    static const wchar_t* const ENGLISH[] = {L"normal size", L"twice as big", L"three times as big"};
    SendMessageW(list, CB_RESETCONTENT, 0, 0);
    int select = 0;
    for (int size = 1; size <= MINIMAP_SIZES; ++size) {
        if (size > 1 && width && !minimap_size_fits(size, width)) break;
        const std::wstring text = launcher_text(KEYS[size - 1], ENGLISH[size - 1]);
        const auto i = SendMessageW(list, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        SendMessageW(list, CB_SETITEMDATA, i, size);
        if (size <= wanted) select = static_cast<int>(i);
    }
    SendMessageW(list, CB_SETCURSEL, select, 0);
}

// A row under the frame limit's; OK, Cancel and the bottom of the window move down to make room.
void add_launcher_row(HWND dialog) {
    if (!GetDlgItem(dialog, ID_RESOLUTION)) return;
    const RECT row = dialog_rect(dialog, 0, 0, 0, ROW_HEIGHT);
    move_down(dialog, IDOK, row.bottom);
    move_down(dialog, IDCANCEL, row.bottom);
    RECT w;
    GetWindowRect(dialog, &w);
    SetWindowPos(dialog, nullptr, 0, 0, w.right - w.left, w.bottom - w.top + row.bottom, SWP_NOMOVE | SWP_NOZORDER);
    add_check_box(dialog, L"Cheat keys", dialog_rect(dialog, 7, ROW_Y, 82, 10), ID_CHEATS, g_cheats.on);
    add_check_box(dialog, L"All missions", dialog_rect(dialog, 98, ROW_Y, 82, 10), ID_UNLOCK, g_unlock.on);
    add_control(dialog, L"STATIC", L"Minimap:", SS_LEFT, dialog_rect(dialog, 7, MINIMAP_Y + 2, 48, 8), ID_MINIMAP_LABEL);
    add_control(dialog, L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                dialog_rect(dialog, 58, MINIMAP_Y, 122, 60), ID_MINIMAP);
    add_control(dialog, L"STATIC", L"A bigger minimap leaves a strip under the bottom edge of the map, so it covers nothing.",
                SS_LEFT | SS_NOPREFIX, dialog_rect(dialog, 7, MINIMAP_Y + 15, 173, 26), ID_MINIMAP_NOTE);
    fill_minimap(dialog);

    wchar_t caption[256] = {};
    GetWindowTextW(dialog, caption, 200);
    wchar_t text[300];
    wsprintfW(text, L"%s - OpenLocoland %hs", caption, LOCOFIX_VERSION);
    SetWindowTextW(dialog, text);
}

void read_launcher_row(HWND dialog) {
    if (!GetDlgItem(dialog, ID_CHEATS)) return;
    g_cheats.on = IsDlgButtonChecked(dialog, ID_CHEATS) == BST_CHECKED;
    g_unlock.on = IsDlgButtonChecked(dialog, ID_UNLOCK) == BST_CHECKED;
    save_options();
    log_options();
    HWND list = GetDlgItem(dialog, ID_MINIMAP);
    const auto i = list ? SendMessageW(list, CB_GETCURSEL, 0, 0) : CB_ERR;
    if (i != CB_ERR) {
        g_minimap = static_cast<int>(SendMessageW(list, CB_GETITEMDATA, i, 0));
        save_minimap();
        minimap_set_size(g_minimap);
    }
}

INT_PTR CALLBACK launcher_proc(HWND dialog, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND && LOWORD(w) == IDOK) read_launcher_row(dialog);
    const INT_PTR result = g_launcher_proc(dialog, m, w, l);
    if (m == WM_INITDIALOG) add_launcher_row(dialog);
    if (m == WM_COMMAND && LOWORD(w) == ID_RESOLUTION && (HIWORD(w) == CBN_SELCHANGE || HIWORD(w) == CBN_SELENDOK))
        fill_minimap(dialog);
    return result;
}

INT_PTR WINAPI DialogBoxParamA_options(HINSTANCE instance, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param) {
    if (!IS_INTRESOURCE(name) && lstrcmpiA(name, "START_DIALOG") == 0) {
        g_launcher_proc = proc;
        proc = &launcher_proc;
    }
    return g_dialog_box(instance, name, parent, proc, param);
}

}  // namespace

// After language.cpp and frame_limit.cpp: we take over their CreateFileA and DialogBoxParamA
// entries and call them in turn, so the pack lookup sees the option's file and their launcher
// procs wrap ours (the language one labels our boxes).
void install_options() {
    for (Option& o : g_options) o.on = read_option(o.value);
    g_minimap = read_minimap();
    log_options();

    g_create_file = *reinterpret_cast<CreateFileA_t*>(IAT_CreateFileA);
    void* create_file = reinterpret_cast<void*>(&CreateFileA_options);
    patch::write(IAT_CreateFileA, &create_file, sizeof create_file);

    g_dialog_box = *reinterpret_cast<DialogBoxParamA_t*>(IAT_DialogBoxParamA);
    void* dialog_box = reinterpret_cast<void*>(&DialogBoxParamA_options);
    patch::write(IAT_DialogBoxParamA, &dialog_box, sizeof dialog_box);
    add_launcher_label(ID_CHEATS, "cheats");
    add_launcher_label(ID_UNLOCK, "all_missions");
    add_launcher_label(ID_MINIMAP_LABEL, "minimap");
    add_launcher_label(ID_MINIMAP_NOTE, "minimap_note");
    add_launcher_relabel(&fill_minimap);
}
