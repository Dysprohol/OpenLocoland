// The maps of the custom maps page (DEVELOPERS.md "The custom maps page", "Map files").
//
// A custom map is a folder of maps\ that holds <folder>.map - with its script <folder>.lgc, its
// texts, brief.lst and, when it has a picture of its own, <folder>.vid. Nothing else makes it one:
// the page lists every such folder, wherever it came from (saved in the editor, imported, copied
// by hand, renamed in Explorer), and drops a folder that is gone. The menu's script cannot list a
// folder, so LocoFix writes maps\custom.lst (two lines a map: its folder, its title) before the
// page reads it - the script function ListCustomMaps() (extern 204) when the page is made, and
// CustomMapsChanged() (205) tells the page when something in maps\ was added, removed or renamed
// while it is open (a change notification), so it is made again. CustomMapProblem(folder) (206)
// says why a map cannot be opened (gone, no ground image, no script ...) before the page starts
// it or opens it in the editor; the page shows it on its last line.
//
// The list is a cache: POLIGON first, then the maps in the order the list had them, then the new
// ones by name; a folder is written as it is spelled on disk and compared without case, so a
// folder is never listed twice. The title is read from the map's script - its "// title:" line,
// else BRIEF_HEADER, else the folder's name in capitals - in ASCII capitals (the menu's font, the
// same list in every language); two maps of the same title get " 2", " 3" ... on the page.
//
// A folder whose files or paths carry another name - renamed or copied by hand: maps\<new>\<old>.map,
// or its script's CUSTOM_MAP naming maps\<old>\ - is brought in line as an import under a new name
// does it (editor_exchange.cpp: the files <old>.* renamed, the paths in the script, the lists, the
// texts and the ground record), once its files have not changed for a few seconds (a copy may be
// still under way). Renaming in the editor is the same: the folder copied under the new name with
// its files renamed, the old one going to the Recycle Bin only when the new one is complete.
//
// The names: only what is needed - Windows' rules for a file name, ASCII (the script and the list
// keep the name as bytes, which another code page would read as other letters), short enough for
// the paths, and not a folder of the game (map_name_problem).
#include "patch.h"
#include "custom_maps.h"
#include "editor_exchange.h"
#include "editor_text.h"
#include "script_value.h"

#include <shellapi.h>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

std::string map_ground_image_of(const std::string& map_bytes);                // editor_newmap.cpp
bool set_map_ground_image(const std::string& map, const std::string& image);  // editor_newmap.cpp

namespace {

constexpr const char* LIST = "maps\\custom.lst";
constexpr const char* SCRATCH_MARK = "editor_new.txt";   // editor.cpp's scratch folders
constexpr size_t NAME_LENGTH = 64;    // maps\<name>\<name>.map and its texts stay far below MAX_PATH
constexpr size_t TITLE_LENGTH = 24;   // the page's row (editor_mission.cpp writes titles as long)
constexpr DWORD QUIET_MS = 3000;      // a folder renamed or copied by hand is brought in line after this
constexpr int EXTERN_LIST = 204, EXTERN_CHANGED = 205, EXTERN_PROBLEM = 206;
constexpr uintptr_t SCRIPT_PUSH_INT = 0x442700;      // void __thiscall Script_PushInt(Script*, int)
constexpr uintptr_t SCRIPT_PUSH_STRING = 0x442550;   // void __thiscall Script_PushString(Script*, char** text): a copy
constexpr uintptr_t CRT_NEW = 0x456b11;
constexpr uintptr_t CRT_DELETE = 0x456a00;
constexpr uintptr_t EMPTY_STRING = 0x4a9d6c;
constexpr size_t WORLD_SCRIPT = 0x14c;
constexpr size_t WORLD_SCRIPT_COUNT = 0x150;
constexpr size_t WORLD_SCRIPT_VALUES = 0x158;

using PushInt_t = void(__fastcall*)(uint8_t* script, void* edx, int value);
using PushString_t = void(__fastcall*)(uint8_t* script, void* edx, char** text);
using New_t = char*(__cdecl*)(size_t size);
using Delete_t = void(__cdecl*)(const void* memory);

HANDLE g_watch = nullptr;   // the change notification of maps\ (made with the first list)
DWORD g_retry_at = 0;       // a folder to bring in line was still changing: look again then

// ---- files ----

bool exists(const std::string& path) { return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

std::string read_file(const std::string& path) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return "";
    std::string data(GetFileSize(f, nullptr), '\0');
    DWORD got = 0;
    if (!data.empty() && (!ReadFile(f, &data[0], static_cast<DWORD>(data.size()), &got, nullptr) || got != data.size()))
        data.clear();
    CloseHandle(f);
    return data;
}

bool write_file(const std::string& path, const std::string& data) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = data.empty() || (WriteFile(f, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
                                     written == data.size());
    CloseHandle(f);
    return ok;
}

bool same(const std::string& a, const std::string& b) { return lstrcmpiA(a.c_str(), b.c_str()) == 0; }

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool starts_with_ci(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && same(s.substr(0, prefix.size()), prefix);
}

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> out;
    size_t a = 0;
    while (a < text.size()) {
        size_t b = text.find('\n', a);
        if (b == std::string::npos) b = text.size();
        std::string line = text.substr(a, b - a);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        out.push_back(line);
        a = b + 1;
    }
    return out;
}

// The entries of a folder (a game path): its subfolders or its files, as spelled on disk.
std::vector<std::string> entries(const std::string& folder, bool folders) {
    std::vector<std::string> out;
    WIN32_FIND_DATAA f;
    HANDLE h = FindFirstFileA((folder + "\\*").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) return out;
    do {
        const bool dir = (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (dir == folders && std::strcmp(f.cFileName, ".") != 0 && std::strcmp(f.cFileName, "..") != 0) out.push_back(f.cFileName);
    } while (FindNextFileA(h, &f));
    FindClose(h);
    return out;
}

// No file of the folder was written in the last QUIET_MS (a copy into it has ended).
bool quiet(const std::string& folder) {
    FILETIME now_ft;
    GetSystemTimeAsFileTime(&now_ft);
    const ULONGLONG now = (static_cast<ULONGLONG>(now_ft.dwHighDateTime) << 32) | now_ft.dwLowDateTime;
    WIN32_FIND_DATAA f;
    HANDLE h = FindFirstFileA((folder + "\\*").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) return true;
    bool still = true;
    do {
        const ULONGLONG t = (static_cast<ULONGLONG>(f.ftLastWriteTime.dwHighDateTime) << 32) | f.ftLastWriteTime.dwLowDateTime;
        if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && now < t + QUIET_MS * 10000ULL) still = false;
    } while (still && FindNextFileA(h, &f));
    FindClose(h);
    return still;
}

void remove_files(const std::string& folder) {
    for (const std::string& file : entries(folder, false)) DeleteFileA((folder + "\\" + file).c_str());
    RemoveDirectoryA(folder.c_str());
}

// ---- names and titles ----

bool campaign_folder(const std::string& f) {
    return f.size() == 3 && (f[0] == 'l' || f[0] == 'L') && std::isdigit(static_cast<unsigned char>(f[1])) &&
           std::isdigit(static_cast<unsigned char>(f[2]));
}

bool scratch_folder(const std::string& f) {
    return starts_with_ci(f, "_new") || same(f, "_run") || exists("maps\\" + f + "\\" + SCRATCH_MARK);
}

// The characters of a name: Windows' rules and ASCII. Empty when they are fine.
const char* name_chars_problem(const std::string& name) {
    if (name.empty() || name.size() > NAME_LENGTH)
        return ui_text("NameLength", "The name: 1 to 64 characters.");
    for (char c : name) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u < 32 || u > 126)
            return ui_text("NameAscii", "The name: English letters, digits, spaces and signs - no letters with marks (another "
                                        "system's code page would read them as other letters).");
        if (std::strchr("\\/:*?\"<>|", c)) return ui_text("NameSign", "The name cannot hold \\ / : * ? \" < > | (Windows).");
    }
    if (name.front() == ' ' || name.back() == ' ' || name.back() == '.')
        return ui_text("NameEnds", "The name cannot begin or end with a space, nor end with a dot (Windows).");
    static const char* const DEVICES[] = {"con", "prn", "aux", "nul", "com1", "com2", "com3", "com4", "com5", "com6",
                                          "com7", "com8", "com9", "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7",
                                          "lpt8", "lpt9"};
    const std::string base = lower(trim(name.substr(0, name.find('.'))));
    for (const char* d : DEVICES)
        if (base == d) return ui_text("NameDevice", "CON, PRN, AUX, NUL, COM1-9 and LPT1-9 are names of Windows' devices.");
    return "";
}

// A title for the page and the script: ASCII capitals without quotes, at most TITLE_LENGTH.
std::string clean_title(const std::string& text) {
    std::string out;
    for (char c : text) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (u >= 32 && u < 127 && c != '"' && c != '\\') out += static_cast<char>(std::toupper(u));
    }
    out = trim(out);
    if (out.size() > TITLE_LENGTH) out = trim(out.substr(0, TITLE_LENGTH));
    return out;
}

std::string script_of(const std::string& folder) { return "maps\\" + folder + "\\" + folder + ".lgc"; }

// The value of `#define <name> "..."` in a script, the backslashes undoubled; empty when none.
std::string define_text(const std::string& script, const char* name) {
    for (const std::string& raw : lines_of(script)) {
        const std::string line = trim(raw);
        if (line.compare(0, 7, "#define") != 0) continue;
        const std::string rest = trim(line.substr(7));
        if (rest.compare(0, std::strlen(name), name) != 0 || rest.size() == std::strlen(name) ||
            (rest[std::strlen(name)] != ' ' && rest[std::strlen(name)] != '\t'))
            continue;
        const size_t a = rest.find('"'), b = rest.rfind('"');
        if (a == std::string::npos || b <= a) return "";
        std::string out;
        for (size_t i = a + 1; i < b; ++i) {
            if (rest[i] == '\\' && i + 1 < b && rest[i + 1] == '\\') ++i;
            out += rest[i];
        }
        return out;
    }
    return "";
}

// The map's title: the script's "// title:" line, else its BRIEF_HEADER, else the folder's name.
std::string title_of(const std::string& folder) {
    const std::string script = read_file(script_of(folder));
    for (const std::string& raw : lines_of(script)) {
        const std::string line = trim(raw);
        if (line.compare(0, 9, "// title:") == 0) {
            const std::string t = clean_title(line.substr(9));
            if (!t.empty()) return t;
            break;
        }
    }
    std::string t = clean_title(define_text(script, "BRIEF_HEADER"));
    if (t.empty()) t = clean_title(folder);
    return t.empty() ? "MAP" : t;
}

// ---- bringing a folder in line ----

// The name the map's files and paths carry when it is not the folder's: the one .map of a folder
// without <folder>.map, or the folder its script's CUSTOM_MAP names. Empty when they agree.
std::string carried_name(const std::string& folder) {
    const std::string dir = "maps\\" + folder;
    if (!exists(dir + "\\" + folder + ".map")) {
        std::vector<std::string> maps;
        for (const std::string& file : entries(dir, false))
            if (file.size() > 4 && same(file.substr(file.size() - 4), ".map")) maps.push_back(file.substr(0, file.size() - 4));
        return maps.size() == 1 ? maps[0] : "";
    }
    const std::string own = define_text(read_file(script_of(folder)), "CUSTOM_MAP");   // maps\<name>\<name>.map
    if (!starts_with_ci(own, "maps\\")) return "";
    const size_t slash = own.find('\\', 5);
    const std::string name = slash == std::string::npos ? "" : own.substr(5, slash - 5);
    return same(name, folder) ? "" : name;
}

}  // namespace

// The script's title and header that were the old name in capitals become the new one's (each
// line keeps its own line end).
std::string custom_map_retitled(const std::string& script, const std::string& from, const std::string& to) {
    const std::string old_title = clean_title(from), new_title = clean_title(to);
    if (old_title == new_title) return script;
    std::string out;
    for (size_t a = 0; a < script.size();) {
        size_t b = script.find('\n', a);
        b = b == std::string::npos ? script.size() : b + 1;
        const std::string raw = script.substr(a, b - a), t = trim(raw);
        const std::string eol = raw.substr(raw.find_last_not_of("\r\n") + 1);
        if (t.compare(0, 9, "// title:") == 0 && clean_title(t.substr(9)) == old_title) out += "// title: " + new_title + eol;
        else if (t.compare(0, 7, "#define") == 0 && trim(t.substr(7)).compare(0, 13, "BRIEF_HEADER ") == 0 &&
                 clean_title(define_text(t, "BRIEF_HEADER")) == old_title)
            out += "#define BRIEF_HEADER \"" + new_title + "\"" + eol;
        else out += raw;
        a = b;
    }
    return out;
}

namespace {

// A folder whose files carry another name gets them renamed and its paths fixed, in place: the new
// files first, then the old names go.
void bring_in_line(const std::string& folder) {
    const std::string name = carried_name(folder);
    if (name.empty()) return;
    const std::string dir = "maps\\" + folder;
    if (!quiet(dir)) {
        g_retry_at = GetTickCount() + QUIET_MS;
        return;
    }
    Package p;
    std::string error;
    if (!exchange_read_folder(dir, name, p, error)) {
        patch::log("custom maps: %s carries the name %s, not read: %s", dir.c_str(), name.c_str(), error.c_str());
        return;
    }
    const std::vector<PackageFile> files = exchange_renamed(p, folder);
    for (const PackageFile& f : files)
        if (!write_file(dir + "\\" + f.name, f.data)) {
            patch::log("custom maps: %s could not be written", (dir + "\\" + f.name).c_str());
            return;
        }
    for (const PackageFile& f : p.files) {
        bool kept = false;
        for (const PackageFile& n : files) kept |= same(n.name, f.name);
        if (!kept) DeleteFileA((dir + "\\" + f.name).c_str());
    }
    // a ground of its own still named after the old folder (the files kept the folder's name): here
    const std::string map = dir + "\\" + folder + ".map", image = map_ground_image_of(read_file(map));
    if (starts_with_ci(image, "maps\\" + name + "\\")) {
        std::string base = image.substr(name.size() + 6);
        if (starts_with_ci(base, name + ".")) base = folder + base.substr(name.size());
        if (exists(dir + "\\" + base)) set_map_ground_image(map, dir + "\\" + base);
    }
    patch::log("custom maps: %s carried the name %s - its files and paths renamed after the folder", dir.c_str(), name.c_str());
}

// ---- the list ----

struct Entry {
    std::string folder, title, own;   // `own`: the script's, before " 2" ...
};

std::vector<Entry> scan() {
    std::vector<std::string> found;
    for (const std::string& folder : entries("maps", true)) {
        if (campaign_folder(folder) || same(folder, "script") || same(folder, "outro") || same(folder, "template") ||
            scratch_folder(folder))
            continue;
        bring_in_line(folder);
        if (custom_map_exists(folder)) found.push_back(folder);
    }
    // POLIGON, the order of the list as it was, the new ones by name
    std::vector<std::string> order{"sandbox"};
    const std::vector<std::string> old = lines_of(read_file(LIST));
    for (size_t i = 0; i < old.size(); i += 2)
        if (!trim(old[i]).empty()) order.push_back(trim(old[i]));
    std::vector<std::string> rest = found;
    std::sort(rest.begin(), rest.end(), [](const std::string& a, const std::string& b) { return lstrcmpiA(a.c_str(), b.c_str()) < 0; });
    order.insert(order.end(), rest.begin(), rest.end());
    std::vector<Entry> list;
    for (const std::string& want : order)
        for (const std::string& folder : found) {
            if (!same(want, folder)) continue;
            bool listed = false;
            for (const Entry& e : list) listed |= same(e.folder, folder);
            if (!listed) {
                const std::string t = title_of(folder);
                list.push_back({folder, t, t});
            }
        }
    for (size_t i = 0; i < list.size(); ++i) {   // the same title twice: " 2", " 3" ...
        int n = 1;
        for (size_t j = 0; j < i; ++j) n += same(list[j].own, list[i].own) ? 1 : 0;
        if (n > 1) list[i].title = trim(list[i].title.substr(0, TITLE_LENGTH - 3)) + " " + std::to_string(n);
    }
    return list;
}

void watch() {
    if (g_watch) return;
    g_watch = FindFirstChangeNotificationA((patch::game_folder() + "maps").c_str(), TRUE,
                                           FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME);
    if (g_watch == INVALID_HANDLE_VALUE) patch::log("custom maps: maps\\ cannot be watched (error %lu)", GetLastError());
}

bool changed() {
    watch();
    bool any = false;
    if (g_watch != INVALID_HANDLE_VALUE)
        while (WaitForSingleObject(g_watch, 0) == WAIT_OBJECT_0) {
            any = true;
            if (!FindNextChangeNotification(g_watch)) break;
        }
    if (g_retry_at && static_cast<int>(GetTickCount() - g_retry_at) >= 0) {
        g_retry_at = 0;
        any = true;
    }
    if (any) patch::log("custom maps: maps\\ changed - the page is made again");
    return any;
}

std::string pop_text(uint8_t* world) {
    int32_t& count = *reinterpret_cast<int32_t*>(world + WORLD_SCRIPT_COUNT);
    auto* values = *reinterpret_cast<sv::Value**>(world + WORLD_SCRIPT_VALUES);
    if (count <= 0) return "";
    const sv::Value& top = values[--count];
    return (top.flags & 1) && top.text ? top.text : "";
}

std::string menu_text(const char* text) {   // capitals in the pack's code page: the menu's fonts have no small letters
    std::wstring w = ui_wide(text);
    if (!w.empty()) CharUpperBuffW(&w[0], static_cast<DWORD>(w.size()));
    return ui_narrow(w);
}

}  // namespace

std::string map_name_problem(const std::string& name) {
    const char* chars = name_chars_problem(name);
    if (*chars) return chars;
    if (campaign_folder(name) || same(name, "script") || same(name, "outro") || same(name, "sandbox") || same(name, "template") ||
        starts_with_ci(name, "_new") || same(name, "_run"))
        return ui_text("NameGame", "This is the name of a folder of the game (the campaign's l01-l20, script, outro, sandbox, "
                                   "template) or of the editor's scratch (_new..., _run).");
    return "";
}

const char* map_name_rule() {
    return ui_text("NameRule", "The name is the map's folder and files: up to 64 English letters, digits, spaces and signs but "
                               "\\ / : * ? \" < > |, not a folder of the game.");
}

bool custom_map_exists(const std::string& folder) {
    return !folder.empty() && exists("maps\\" + folder + "\\" + folder + ".map");
}

std::string custom_map_title(const std::string& folder) { return title_of(folder); }

bool game_map_folder(const std::string& folder) {
    return campaign_folder(folder) || same(folder, "script") || same(folder, "outro") || same(folder, "sandbox") ||
           same(folder, "template") || scratch_folder(folder);
}

void custom_maps_list() {
    watch();
    const std::vector<Entry> list = scan();
    std::string data;
    for (const Entry& e : list) data += e.folder + "\r\n" + e.title + "\r\n";
    const bool same_list = read_file(LIST) == data && exists(LIST);
    if (!same_list && !write_file(LIST, data)) patch::log("custom maps: %s cannot be written", LIST);
    else patch::log("custom maps: %u maps on the page%s", static_cast<unsigned>(list.size()), same_list ? "" : " (the list written anew)");
}

std::string custom_map_problem(const std::string& folder, bool menu) {
    const char* problem = "";
    const std::string map = "maps\\" + folder + "\\" + folder + ".map";
    if (!custom_map_exists(folder)) {
        problem = ui_text("MapGone", "This map is no longer in the maps folder.");
    } else if (*name_chars_problem(folder)) {
        problem = ui_text("MapBadName", "Its folder's name needs English letters.");
    } else {
        const std::string bytes = read_file(map);
        const std::string ground = bytes.size() >= 12 ? map_ground_image_of(bytes) : "";
        if (bytes.size() < 12 || bytes.compare(0, 4, "RES ") != 0 || bytes.compare(8, 4, "MAP ") != 0)
            problem = ui_text("MapBroken", "Its .map file cannot be read.");
        else if (!ground.empty() && !exists(ground))
            problem = ui_text("MapNoGround", "Its ground image (.vid) is missing.");
        else if (!exists(script_of(folder)))
            problem = ui_text("MapNoScript", "Its script (.lgc) is missing.");
    }
    if (*problem) patch::log("custom maps: %s cannot be opened: %s", map.c_str(), problem);
    return menu && *problem ? menu_text(problem) : problem;
}

void custom_maps_keep_grounds(const std::string& folder) {
    const std::string prefix = "maps\\" + folder + "\\";
    for (const std::string& other : entries("maps", true)) {
        if (same(other, folder) || !custom_map_exists(other)) continue;
        const std::string map = "maps\\" + other + "\\" + other + ".map";
        const std::string image = map_ground_image_of(read_file(map));
        if (!starts_with_ci(image, prefix) || !exists(image)) continue;
        const std::string own = "maps\\" + other + "\\" + other + ".vid";
        if (CopyFileA(image.c_str(), own.c_str(), FALSE) && set_map_ground_image(map, own))
            patch::log("custom maps: %s keeps its ground as %s (it was %s)", map.c_str(), own.c_str(), image.c_str());
    }
}

bool custom_map_rename(const std::string& from, const std::string& to, bool remove_old, std::string& error) {
    error = map_name_problem(to);
    if (error.empty() && same(from, to)) error = ui_text("RenameSame", "That is its name already.");
    if (error.empty() && exists("maps\\" + to))
        error = ui_printf(ui_format("RenameTaken", "There is a folder maps\\%s already."), to.c_str());
    if (error.empty() && !custom_map_exists(from)) error = ui_text("MapGone", "This map is no longer in the maps folder.");
    if (!error.empty()) return false;
    const std::string old_dir = "maps\\" + from, new_dir = "maps\\" + to;
    Package p;
    std::string why;
    if (!exchange_read_folder(old_dir, from, p, why)) {
        error = ui_printf(ui_format("RenameFailed", "The map could not be renamed: %s"), why.c_str());
        return false;
    }
    const std::vector<PackageFile> files = exchange_renamed(p, to);
    if (!CreateDirectoryA(new_dir.c_str(), nullptr)) {
        error = ui_printf(ui_format("RenameFailed", "The map could not be renamed: %s"), (new_dir + " cannot be made").c_str());
        return false;
    }
    for (const PackageFile& f : files)
        if (!write_file(new_dir + "\\" + f.name, f.data) || read_file(new_dir + "\\" + f.name) != f.data) {
            remove_files(new_dir);   // the old folder stays as it was
            error = ui_printf(ui_format("RenameFailed", "The map could not be renamed: %s"), (new_dir + "\\" + f.name + " cannot be written").c_str());
            return false;
        }
    patch::log("custom maps: %s copied as %s (%u files)", old_dir.c_str(), new_dir.c_str(), static_cast<unsigned>(files.size()));
    {   // the renamed map keeps its place on the page
        std::vector<std::string> lines = lines_of(read_file(LIST));
        std::string data;
        for (size_t i = 0; i < lines.size(); ++i) data += (i % 2 == 0 && same(trim(lines[i]), from) ? to : lines[i]) + "\r\n";
        write_file(LIST, data);
    }
    custom_maps_keep_grounds(from);
    if (remove_old) {
        std::string ignored;
        custom_map_remove(from, ignored);
    }
    custom_maps_list();
    return true;
}

bool custom_map_remove(const std::string& folder, std::string& error) {
    if (folder.empty() || game_map_folder(folder) || !exists("maps\\" + folder)) {
        error = ui_text("MapGone", "This map is no longer in the maps folder.");
        return false;
    }
    custom_maps_keep_grounds(folder);
    std::wstring path = ui_wide(patch::game_folder() + "maps\\" + folder);
    path.push_back(L'\0');   // the list ends with two
    SHFILEOPSTRUCTW op = {};
    op.wFunc = FO_DELETE;
    op.pFrom = path.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
    const int result = SHFileOperationW(&op);
    if (result != 0 || op.fAnyOperationsAborted || exists("maps\\" + folder)) {
        patch::log("custom maps: maps\\%s could not be removed (%d)", folder.c_str(), result);
        error = ui_printf(ui_format("RemoveFailed", "The folder maps\\%s could not be removed (is a file of it open?)."), folder.c_str());
        return false;
    }
    patch::log("custom maps: maps\\%s moved to the Recycle Bin", folder.c_str());
    custom_maps_list();
    return true;
}

const char* custom_maps_level_guard(const char* map) {
    if (!map || reinterpret_cast<uintptr_t>(map) == EMPTY_STRING || !*map) return map;
    const size_t n = std::strlen(map);
    if (n < 4 || lstrcmpiA(map + n - 4, ".map") != 0) return map;
    const bool absolute = n > 1 && (map[1] == ':' || map[0] == '\\');
    if (exists(absolute ? std::string(map) : patch::game_folder() + map)) return map;
    static const char MENU[] = "maps\\menu.map";
    char* menu = reinterpret_cast<New_t>(CRT_NEW)(sizeof MENU);
    if (!menu) return map;
    std::memcpy(menu, MENU, sizeof MENU);
    patch::log("custom maps: %s is not there - the main menu instead", map);
    reinterpret_cast<Delete_t>(CRT_DELETE)(map);
    return menu;
}

bool custom_maps_extern(uint8_t* world, int function) {
    uint8_t* script = world + WORLD_SCRIPT;
    if (function == EXTERN_LIST) {   // ListCustomMaps(): maps\custom.lst written anew
        custom_maps_list();
        reinterpret_cast<PushInt_t>(SCRIPT_PUSH_INT)(script, nullptr, 0);
        return true;
    }
    if (function == EXTERN_CHANGED) {   // CustomMapsChanged(): 1 when maps\ changed since
        reinterpret_cast<PushInt_t>(SCRIPT_PUSH_INT)(script, nullptr, changed() ? 1 : 0);
        return true;
    }
    if (function == EXTERN_PROBLEM) {   // CustomMapProblem(string folder): "" or why it cannot be opened
        std::string text = custom_map_problem(pop_text(world), true);
        char* p = &text[0];
        if (text.empty()) p = const_cast<char*>("");
        reinterpret_cast<PushString_t>(SCRIPT_PUSH_STRING)(script, nullptr, &p);
        return true;
    }
    return false;
}
