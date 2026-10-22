// Language packs: the game reads its texts from lang\<code>\ of the chosen language.
//
// The language versions of the game differ only in data files (texts, fonts, logo; the
// exe and objects.res are the same), so a pack is a folder that mirrors the game folder
// with just those files plus language.ini (name and launcher labels, UTF-8). When the
// game opens an existing file (CreateFileA - the C runtime goes through it too) or
// reads an .ini (GetPrivateProfileStringA/IntA - strings.ini), and the chosen pack has
// that file, the pack's copy is used.
//
// The launcher gets a "Language" list next to the full screen box; changing it switches
// the launcher labels at once and OK saves the choice in the registry next to the game's
// own settings. Without a saved choice the system language is used when there is a pack
// for it, else English; a saved choice whose pack is gone (not assembled) falls back to English
// too. Other modules' launcher controls are labelled the same way (add_launcher_label).
// language.ini also names the pack's code page ([language] codepage=, 1252 when missing): the
// one of its strings.ini, texts and fonts - the editor converts its texts with it.
#include "patch.h"
#include <cstdlib>
#include <map>
#include <vector>

namespace {

constexpr uintptr_t IAT_CreateFileA = 0x4750f8;
constexpr uintptr_t IAT_GetPrivateProfileStringA = 0x475118;
constexpr uintptr_t IAT_GetPrivateProfileIntA = 0x47511c;
constexpr uintptr_t IAT_DialogBoxParamA = 0x4751ac;
constexpr char REG_KEY[] = "SOFTWARE\\Gromada\\Locoland";   // RegPath in Locoland.cfg
constexpr char REG_VALUE[] = "Language";
constexpr int ID_FULL_SCREEN = 1005;
constexpr int ID_LANGUAGE = 1100;
constexpr int ID_LANGUAGE_LABEL = 1101;

struct Pack {
    std::string code;
    std::wstring name;
    std::map<std::string, std::wstring> launcher;   // labels from [launcher]
    unsigned codepage = 1252;                       // [language] codepage
};

std::vector<Pack> g_packs;
int g_current = -1;
std::string g_folder;   // game folder with a trailing backslash

std::wstring from_utf8(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return {};
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

bool read_pack(const std::string& code, Pack& pack) {
    FILE* f = std::fopen((g_folder + "lang\\" + code + "\\language.ini").c_str(), "rb");
    if (!f) return false;
    pack.code = code;
    std::string section;
    char buffer[512];
    while (std::fgets(buffer, sizeof buffer, f)) {
        std::string line = buffer;
        if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        line = trim(line);
        if (line.empty() || line[0] == ';') continue;
        if (line[0] == '[') {
            section = line;
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = trim(line.substr(0, eq));
        const std::wstring value = from_utf8(trim(line.substr(eq + 1)));
        if (section == "[language]" && key == "name") pack.name = value;
        if (section == "[language]" && key == "codepage") pack.codepage = std::strtoul(trim(line.substr(eq + 1)).c_str(), nullptr, 10);
        if (section == "[launcher]") pack.launcher[key] = value;
    }
    std::fclose(f);
    if (pack.name.empty()) pack.name = from_utf8(code);
    return true;
}

void find_packs() {
    WIN32_FIND_DATAA found;
    HANDLE h = FindFirstFileA((g_folder + "lang\\*").c_str(), &found);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || found.cFileName[0] == '.') continue;
        Pack pack;
        if (read_pack(found.cFileName, pack)) g_packs.push_back(pack);
    } while (FindNextFileA(h, &found));
    FindClose(h);
}

int pack_index(const std::string& code) {
    for (size_t i = 0; i < g_packs.size(); ++i)
        if (_stricmp(g_packs[i].code.c_str(), code.c_str()) == 0) return static_cast<int>(i);
    return -1;
}

std::string saved_language() {
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return {};
    char value[32] = {};
    DWORD size = sizeof value - 1, type = 0;
    const LONG r = RegQueryValueExA(key, REG_VALUE, nullptr, &type, reinterpret_cast<BYTE*>(value), &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS && type == REG_SZ ? value : "";
}

void save_language(const std::string& code) {
    HKEY key;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS)
        return;
    RegSetValueExA(key, REG_VALUE, 0, REG_SZ, reinterpret_cast<const BYTE*>(code.c_str()),
                   static_cast<DWORD>(code.size() + 1));
    RegCloseKey(key);
}

// ---- files ----

// The pack's copy of `path`, or an empty string when the pack has none.
std::string redirect(const char* path) {
    if (g_current < 0 || !path || !*path) return {};
    std::string p = path;
    for (char& c : p)
        if (c == '/') c = '\\';
    std::string rel;
    if (p.size() > 1 && (p[1] == ':' || (p[0] == '\\' && p[1] == '\\'))) {
        if (_strnicmp(p.c_str(), g_folder.c_str(), g_folder.size()) != 0) return {};
        rel = p.substr(g_folder.size());
    } else {
        rel = p;   // relative to the current folder, which is the game folder
    }
    while (rel.compare(0, 2, ".\\") == 0) rel.erase(0, 2);
    while (!rel.empty() && rel[0] == '\\') rel.erase(0, 1);
    const std::string candidate = g_folder + "lang\\" + g_packs[g_current].code + "\\" + rel;
    const DWORD attributes = GetFileAttributesA(candidate.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) return {};
    return candidate;
}

// In test mode (LOCOFIX_TEST=1) every file the game opens for reading goes to LocoFix.log,
// so tests can tell what the game is doing, e.g. that a mission briefing was opened.
bool g_log_opens = false;

HANDLE WINAPI CreateFileA_language(LPCSTR name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security,
                                   DWORD disposition, DWORD flags, HANDLE template_file) {
    // Every open of an existing file, also for writing: script FOpen (the mission texts:
    // objectives, win, loss) opens with "r+t", which asks for GENERIC_WRITE too - left out, those
    // texts came from the game folder's language whatever pack was chosen. Files the game
    // creates (CREATE_ALWAYS, OPEN_ALWAYS: high scores, logs, saved maps) stay where they are.
    std::string pack_copy;
    if (disposition == OPEN_EXISTING) {
        pack_copy = redirect(name);
        if (g_log_opens && name) patch::log("open %s", name);
    }
    return CreateFileA(pack_copy.empty() ? name : pack_copy.c_str(), access, share, security, disposition, flags,
                       template_file);
}

DWORD WINAPI GetPrivateProfileStringA_language(LPCSTR app, LPCSTR key, LPCSTR def, LPSTR out, DWORD size,
                                               LPCSTR file) {
    const std::string pack_copy = redirect(file);
    return GetPrivateProfileStringA(app, key, def, out, size, pack_copy.empty() ? file : pack_copy.c_str());
}

UINT WINAPI GetPrivateProfileIntA_language(LPCSTR app, LPCSTR key, INT def, LPCSTR file) {
    const std::string pack_copy = redirect(file);
    return GetPrivateProfileIntA(app, key, def, pack_copy.empty() ? file : pack_copy.c_str());
}

// ---- launcher ----

DLGPROC g_launcher_proc = nullptr;
std::vector<std::pair<int, std::string>> g_other_labels;   // add_launcher_label

void set_label(HWND dialog, int id, const char* key) {
    const auto& labels = g_packs[g_current].launcher;
    const auto it = labels.find(key);
    if (it != labels.end()) SetWindowTextW(GetDlgItem(dialog, id), it->second.c_str());
}

void apply_labels(HWND dialog) {
    if (g_current < 0) return;
    set_label(dialog, ID_FULL_SCREEN, "full_screen");
    set_label(dialog, IDCANCEL, "cancel");
    set_label(dialog, ID_LANGUAGE_LABEL, "language");
    for (const auto& label : g_other_labels) set_label(dialog, label.first, label.second.c_str());
}

// Places a control at a rectangle given in dialog units, like the dialog template does.
RECT dialog_rect(HWND dialog, int x, int y, int w, int h) {
    RECT r = {x, y, x + w, y + h};
    MapDialogRect(dialog, &r);
    return r;
}

void add_language_list(HWND dialog) {
    if (g_packs.empty()) return;
    const HFONT font = reinterpret_cast<HFONT>(SendMessageW(dialog, WM_GETFONT, 0, 0));
    const HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(dialog, GWLP_HINSTANCE));

    // The full screen box (7,166,120,10 after patch_exe.py) gets narrower to make room.
    RECT r = dialog_rect(dialog, 7, 166, 72, 10);
    MoveWindow(GetDlgItem(dialog, ID_FULL_SCREEN), r.left, r.top, r.right - r.left, r.bottom - r.top, TRUE);

    r = dialog_rect(dialog, 80, 167, 40, 8);
    HWND label = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_RIGHT, r.left, r.top,
                                 r.right - r.left, r.bottom - r.top, dialog,
                                 reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LANGUAGE_LABEL)), instance, nullptr);
    r = dialog_rect(dialog, 122, 164, 58, 80);
    HWND list = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST,
                                r.left, r.top, r.right - r.left, r.bottom - r.top, dialog,
                                reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LANGUAGE)), instance, nullptr);
    SendMessageW(label, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    SendMessageW(list, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    for (const Pack& p : g_packs) SendMessageW(list, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(p.name.c_str()));
    SendMessageW(list, CB_SETCURSEL, g_current, 0);
    apply_labels(dialog);
}

INT_PTR CALLBACK launcher_proc(HWND dialog, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_COMMAND && LOWORD(w) == ID_LANGUAGE) {
        if (HIWORD(w) == CBN_SELCHANGE) {
            const int selected = static_cast<int>(SendDlgItemMessageW(dialog, ID_LANGUAGE, CB_GETCURSEL, 0, 0));
            if (selected >= 0) g_current = selected;
            apply_labels(dialog);
        }
        return TRUE;
    }
    if (m == WM_COMMAND && LOWORD(w) == IDOK && g_current >= 0) save_language(g_packs[g_current].code);
    const INT_PTR result = g_launcher_proc(dialog, m, w, l);
    if (m == WM_INITDIALOG) add_language_list(dialog);
    return result;
}

INT_PTR WINAPI DialogBoxParamA_language(HINSTANCE instance, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param) {
    if (!IS_INTRESOURCE(name) && lstrcmpiA(name, "START_DIALOG") == 0) {
        g_launcher_proc = proc;
        proc = &launcher_proc;
    }
    return DialogBoxParamA(instance, name, parent, proc, param);
}

// Replaces an import of the game if it points where expected.
bool replace_iat(uintptr_t entry, const void* expected, void* function, const char* name) {
    if (*reinterpret_cast<const void* const*>(entry) != expected) {
        patch::log("skipped %s @ %08X: unexpected import", name, static_cast<unsigned>(entry));
        return false;
    }
    patch::write(entry, &function, sizeof function);
    return true;
}

}  // namespace

// Launcher controls added by other modules (created in their WM_INITDIALOG, which runs
// before ours) get their labels from [launcher] too.
void add_launcher_label(int id, const char* key) {
    g_other_labels.emplace_back(id, key);
}

// The chosen pack's code page (1252 without packs).
unsigned language_codepage() {
    return g_current >= 0 ? g_packs[g_current].codepage : 1252;
}

// The chosen pack's copy of a game file (a path relative to the game folder or in it), or the
// path itself - for files LocoFix reads on its own, not through the game's imports.
std::string language_file(const std::string& path) {
    const std::string copy = redirect(path.c_str());
    return copy.empty() ? path : copy;
}

void install_language() {
    char test[8] = {};
    g_log_opens = GetEnvironmentVariableA("LOCOFIX_TEST", test, sizeof test) && test[0] == '1';
    g_folder = patch::game_folder();
    find_packs();
    if (g_packs.empty()) return;

    const std::string saved = saved_language();
    g_current = pack_index(saved);
    if (g_current < 0 && saved.empty()) {   // the system's language when there is a pack for it
        const LANGID system = PRIMARYLANGID(GetUserDefaultUILanguage());
        const char* code = system == LANG_POLISH ? "pl" : system == LANG_GERMAN ? "de" : system == LANG_HUNGARIAN ? "hu"
                         : system == LANG_RUSSIAN ? "ru" : system == LANG_CZECH ? "cs" : "en";
        g_current = pack_index(code);
    }
    if (g_current < 0) g_current = pack_index("en");
    if (g_current < 0) g_current = 0;
    patch::log("language: %s (code page %u)%s", g_packs[g_current].code.c_str(), g_packs[g_current].codepage,
               !saved.empty() && _stricmp(saved.c_str(), g_packs[g_current].code.c_str()) != 0 ? " - the saved pack is not there"
                                                                                               : "");

    replace_iat(IAT_CreateFileA, reinterpret_cast<const void*>(&CreateFileA),
                reinterpret_cast<void*>(&CreateFileA_language), "CreateFileA");
    replace_iat(IAT_GetPrivateProfileStringA, reinterpret_cast<const void*>(&GetPrivateProfileStringA),
                reinterpret_cast<void*>(&GetPrivateProfileStringA_language), "GetPrivateProfileStringA");
    replace_iat(IAT_GetPrivateProfileIntA, reinterpret_cast<const void*>(&GetPrivateProfileIntA),
                reinterpret_cast<void*>(&GetPrivateProfileIntA_language), "GetPrivateProfileIntA");
    replace_iat(IAT_DialogBoxParamA, reinterpret_cast<const void*>(&DialogBoxParamA),
                reinterpret_cast<void*>(&DialogBoxParamA_language), "DialogBoxParamA");
}
