// The mission of the map in the editor: its settings, push lines and texts (DEVELOPERS.md "Editor
// mode", MAP_RULES.md 8 and 11).
//
// The script of a mission (<map>.lgc) holds two blocks the editor writes:
//  - "// <editor>" ... "// </editor>" before #include "maps\script\header.lgc": the settings the
//    scripts in maps\script\ read - LEVEL, CUSTOM_MAP, MONEY, the time of day, rain, lightning
//    (NUKE), the text files (OBJECTIVES_FILE ... HISCORES_FILE, BRIEF_HEADER), the player's
//    reinforcement (PLAYER_REINFORCEMENT, player_reinforcement_train[]), the enemy (the patrol
//    points ATA_PATROLn_*, start_trains*[], level_queue_*[], LIMIT_ATACKERS) and the other
//    settings of the template (music, weather, gammas, AI timings) as they were,
//  - "// <editor main>" ... "// </editor main>" in main()'s iff(1): the SetPushLine calls and the
//    trains' patrols (PatrolEngine on the train found by its first car - TrainPatrol).
// Both are written anew when the map is saved; the rest of the file is the mission's own code
// and stays as it is. A new map gets the script of the map it was saved from when that one has
// the blocks, otherwise the template's (maps\template\template.lgc, map_tool.py new --script) -
// the settings and the SetPushLine calls read from the old script go into the new blocks. A
// script without the blocks (the campaign, the sandbox) is not changed: its settings are shown
// and its push lines drawn, read-only.
//
// The texts are the map's files, in the pack's code page in capitals (the fonts have no small letters), at most 44
// characters a line (the briefing's width), CRLF: objectives.txt, win.txt, loose.txt and the
// briefing pages brief.txt, brief2.txt ... listed in brief.lst. A map saved as
// maps\<folder>\<folder>.map gets its line in maps\custom.lst (the folder and its title), so it
// shows on the custom maps page of the menu.
//
// File > Mission settings... edits all of it in a Windows dialog (built in memory, Unicode, the
// game in GDI mode as for the file dialogs - editor.cpp).
#include "patch.h"
#include "dialog_template.h"
#include "editor_mission.h"
#include "editor_text.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>

std::vector<TrainPatrol> edit_train_patrols();   // editor_edit.cpp: the trains' patrols of the held level
std::string language_file(const std::string& path);   // language.cpp: the pack's copy of a file

namespace {

constexpr int LINE_WIDTH = 44;          // characters a line of the briefing (menutact.lgc)
constexpr int PATROLS = 10;             // ATA_PATROL0 ... ATA_PATROL9
constexpr const char* TEMPLATE_SCRIPT = "maps\\template\\template.lgc";
constexpr const char* CUSTOM_LIST = "maps\\custom.lst";

// the settings the editor writes as they were read, with the template's values
const char* const SETTINGS[][2] = {
    {"LEVEL", "0"},
    {"music0", "\"music\\\\mus08.ogg\""},
    {"music1", "\"music\\\\mus07.ogg\""},
    {"MUXI", "0"},
    {"MUXI_NVID", "268"},
    {"CIVIL", "0"},
    {"SQUALL", "1"},
    {"TORNADO", "0"},
    {"QUAKE_DELAY", "0"},
    {"RAIN_ROUND_POWER", "10"},
    {"ENV_SOUND_DAY", "0"},
    {"ENV_SOUND_NIGHT", "126"},
    {"GAMMA1", "0x00000000"},
    {"GAMMA2", "0x0000253f"},
    {"GAMMA3", "0x00715131"},
    {"GAMMA4", "0x001422f1"},
    {"RAIN_GAMMA1", "0x00221400"},
    {"RAIN_GAMMA2", "0x00453934"},
    {"RAIN_GAMMA3", "0x00796259"},
    {"RAIN_GAMMA4", "0x003c25f7"},
    {"AI_REINFORCEMENT", "1"},
    {"RENEW_TIMEOUT", "45"},
    {"FIRSTCONTACTTIME", "15"},
    {"FIRSTATTACKTIME", "30"},
    {"STG_DESTROY_TIME", "20"},
};
// the ones the editor makes itself
const char* const OWN[] = {"CUSTOM_MAP", "MONEY", "START_DAYNIGHT", "RAIN", "NUKE", "OBJECTIVES_FILE",
                           "WIN_FILE", "LOOSE_FILE", "BRIEF_LIST", "BRIEF_HEADER", "HISCORES_FILE",
                           "PLAYER_REINFORCEMENT", "LIMIT_ATACKERS"};

// the train types of TrainsTactDeclares.lgc (ATT_<size>_<kind>)
const char* const SIZES[] = {"LIGHT", "MEDIUM", "HARD", "VERY_HARD", "DIFF", "DEFENCE"};
const char* const KINDS[] = {"GUN", "MORTAIR", "ROCKET", "AIR", "FIRE", "DESANT"};

struct EnemyTrain {
    int time = 0;          // the queue: seconds
    std::string type;      // ATT_... or a number
    bool patrol = false;
    int x1 = 0, y1 = 0, x2 = 0, y2 = 0;   // screen coordinates; a fighting train starts at x1, y1
};

struct Mission {
    std::wstring title, header;
    int money = 0, daytime = 1, reinforcements = 1, limit = 2;
    bool rain = false, nuke = false;
    std::string train = "5";   // the reinforcement's VIDs
    std::vector<EnemyTrain> start, queue;
    std::wstring objectives, win, lose;
    std::vector<std::wstring> brief;
    std::map<std::string, std::string> settings;   // SETTINGS as read
    std::vector<std::pair<std::string, std::string>> extra;   // other defines of the block
};

Mission g_mission;
std::vector<PushLine> g_lines, g_script_lines;
std::vector<TrainPatrol> g_patrols;   // read from main()'s block; written from the held level's trains
bool g_managed = false;
std::string g_map;   // the map the mission was read from or saved with

// ---- text ----

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

bool starts_with(const std::string& s, const char* prefix) { return s.compare(0, std::strlen(prefix), prefix) == 0; }

// The part of a line before a // comment (not inside quotes).
std::string code_of(const std::string& line) {
    bool quoted = false;
    for (size_t i = 0; i + 1 < line.size(); ++i) {
        if (line[i] == '"') quoted = !quoted;
        if (!quoted && line[i] == '/' && line[i + 1] == '/') return line.substr(0, i);
    }
    return line;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    size_t start = 0;
    while (start <= text.size()) {
        const size_t end = text.find('\n', start);
        std::string line = text.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return lines;
}

std::string read_file(const std::string& path) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
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
    if (f == INVALID_HANDLE_VALUE) {
        patch::log("mission: %s could not be written (error %lu)", path.c_str(), GetLastError());
        return false;
    }
    DWORD written = 0;
    const bool ok = data.empty() || (WriteFile(f, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
                                     written == data.size());
    CloseHandle(f);
    return ok;
}

bool exists(const std::string& path) { return GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

// A map's texts are in the code page of the language pack they are written with (language.ini:
// 1250 Polish and Hungarian, 1251 Russian, 1252 English and German), the one its fonts draw.
std::wstring from_pack(const std::string& s) { return ui_wide(s); }
std::string to_pack(const std::wstring& w) { return ui_narrow(w); }

std::wstring upper(std::wstring w) {
    if (!w.empty()) CharUpperBuffW(&w[0], static_cast<DWORD>(w.size()));
    return w;
}

// A file of the game in the pack's code page as the text of an edit box (CRLF lines, no trailing line
// end): the pack's copy when it has one, as the game reads it (the sandbox's texts in lang\<code>\maps).
std::wstring read_text(const std::string& path) {
    std::wstring out;
    for (const std::string& line : split_lines(read_file(language_file(path)))) out += from_pack(line) + L"\r\n";
    while (out.size() >= 2 && out.compare(out.size() - 2, 2, L"\r\n") == 0) out.erase(out.size() - 2);
    return out;
}

// The lines of a text in capitals, each at most LINE_WIDTH characters (broken between words).
std::vector<std::wstring> wrapped(const std::wstring& text) {
    std::vector<std::wstring> out;
    std::wstring t = upper(text);
    size_t start = 0;
    while (start <= t.size()) {
        size_t end = t.find(L'\n', start);
        std::wstring line = t.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        while (static_cast<int>(line.size()) > LINE_WIDTH) {
            size_t cut = line.rfind(L' ', LINE_WIDTH);
            if (cut == std::wstring::npos || cut == 0) cut = LINE_WIDTH;
            out.push_back(line.substr(0, cut));
            line = line.substr(cut == static_cast<size_t>(LINE_WIDTH) && line[cut] != L' ' ? cut : cut + 1);
        }
        out.push_back(line);
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    while (!out.empty() && out.back().empty()) out.pop_back();
    return out;
}

std::string text_file(const std::wstring& text) {
    std::string out;
    for (const std::wstring& line : wrapped(text)) out += to_pack(line) + "\r\n";
    return out;
}

// A title or header for the menu font and the script: capitals, ASCII, no quotes - the same in
// every language pack (the custom maps page lists the maps of every language). Polish, German
// and Hungarian letters lose their marks, Cyrillic is written in Latin letters.
std::string ascii_upper(const std::wstring& text, size_t length) {
    static const wchar_t FROM[] = L"ĄĆĘŁŃÓŚŹŻ"   // Polish
                                  L"ÄÖÜÁÉÍÚŐŰ";  // German, Hungarian
    static const char* const TO[] = {"A", "C", "E", "L", "N", "O", "S", "Z", "Z",
                                     "AE", "OE", "UE", "A", "E", "I", "U", "O", "U"};
    static const char* const CYRILLIC[] = {"A", "B", "V", "G", "D", "E", "ZH", "Z", "I", "Y", "K", "L", "M", "N", "O", "P",
                                           "R", "S", "T", "U", "F", "KH", "TS", "CH", "SH", "SHCH", "", "Y", "", "E",
                                           "YU", "YA"};   // U+0410 ... U+042F
    std::string out;
    for (wchar_t c : upper(text)) {
        const wchar_t* p = std::wcschr(FROM, c);
        if (c == L'ß') out += "SS";
        else if (c == L'Ё') out += "YO";
        else if (c >= L'А' && c <= L'Я') out += CYRILLIC[c - L'А'];
        else if (p && c) out += TO[p - FROM];
        else if (c >= 32 && c < 127 && c != L'"' && c != L'\\') out += static_cast<char>(c);
        if (out.size() >= length) break;
    }
    if (out.size() > length) out.resize(length);
    return trim(out);
}

std::string unquote(const std::string& v) {
    std::string s = trim(v);
    if (s.size() >= 2 && s.front() == '"' && s.back() == '"') s = s.substr(1, s.size() - 2);
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == '\\') ++i;
        out += s[i];
    }
    return out;
}

std::string quoted_path(const std::string& path) {
    std::string out = "\"";
    for (char c : path) out += c == '\\' ? std::string("\\\\") : std::string(1, c);
    return out + "\"";
}

int to_int(const std::string& s, int fallback = 0) {
    const std::string t = trim(s);
    if (t.empty()) return fallback;
    char* end = nullptr;
    const long v = std::strtol(t.c_str(), &end, 0);
    return end && *end == '\0' ? static_cast<int>(v) : fallback;
}

// ---- the script ----

struct Parsed {
    std::map<std::string, std::string> defines;
    std::vector<std::string> define_order;
    std::map<std::string, std::vector<std::string>> arrays;
    std::string title;
};

void parse_line(const std::string& raw, Parsed& p) {
    const std::string line = trim(raw);
    if (starts_with(line, "// title:")) {
        p.title = trim(line.substr(9));
        return;
    }
    const std::string code = trim(code_of(line));
    if (starts_with(code, "#define")) {
        const std::string rest = trim(code.substr(7));
        const size_t space = rest.find_first_of(" \t");
        const std::string name = rest.substr(0, space);
        if (name.empty()) return;
        if (!p.defines.count(name)) p.define_order.push_back(name);
        p.defines[name] = space == std::string::npos ? "" : trim(rest.substr(space));
    } else if (starts_with(code, "int ")) {
        const size_t bracket = code.find('['), open = code.find('{'), close = code.find('}');
        if (bracket == std::string::npos || open == std::string::npos || close == std::string::npos || close < open) return;
        const std::string name = trim(code.substr(4, bracket - 4));
        std::vector<std::string> items;
        std::string item;
        for (size_t i = open + 1; i <= close; ++i) {
            if (code[i] == ',' || i == close) {
                if (!trim(item).empty()) items.push_back(trim(item));
                item.clear();
            } else {
                item += code[i];
            }
        }
        p.arrays[name] = items;
    }
}

bool parse_push_line(const std::string& raw, PushLine& out) {
    const std::string code = code_of(raw);
    const size_t at = code.find("SetPushLine");
    if (at == std::string::npos) return false;
    const size_t open = code.find('(', at);
    if (open == std::string::npos) return false;
    int v[5];
    if (std::sscanf(code.c_str() + open, "( %d , %d , %d , %d , %d )", &v[0], &v[1], &v[2], &v[3], &v[4]) != 5) return false;
    out = {v[0], v[1], v[2], v[3], v[4]};
    return true;
}

struct Blocks {
    int open = -1, close = -1, main_open = -1, main_close = -1;
    bool found() const { return open >= 0 && close > open && main_open >= 0 && main_close > main_open; }
};

Blocks find_blocks(const std::vector<std::string>& lines) {
    Blocks b;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const std::string t = trim(lines[i]);
        if (starts_with(t, "// <editor>")) b.open = i;
        else if (starts_with(t, "// </editor>")) b.close = i;
        else if (starts_with(t, "// <editor main>")) b.main_open = i;
        else if (starts_with(t, "// </editor main>")) b.main_close = i;
    }
    return b;
}

std::string type_name(const std::string& token) {   // ATT_LIGHT_GUN -> light_gun
    std::string t = starts_with(token, "ATT_") ? token.substr(4) : token;
    for (char& c : t) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return t;
}

// A train type of the dialog as the script writes it: a name of TrainsTactDeclares.lgc or a number.
bool type_token(const std::string& text, std::string& token) {
    std::string t = upper(trim(text));
    if (!t.empty() && (std::isdigit(static_cast<unsigned char>(t[0])) != 0)) {
        token = std::to_string(to_int(t, -1));
        return to_int(t, -1) >= 0;
    }
    if (!starts_with(t, "ATT_")) t = "ATT_" + t;
    for (const char* size : SIZES)
        for (const char* kind : KINDS)
            if (t == std::string("ATT_") + size + "_" + kind) {
                token = t;
                return true;
            }
    return false;
}

int patrol_of(const std::string& task) {   // ATA_PATROL3 or 23 -> 3; -1 otherwise
    if (starts_with(task, "ATA_PATROL") && task.size() == 11 && std::isdigit(static_cast<unsigned char>(task[10])))
        return task[10] - '0';
    const int n = to_int(task, -1);
    return n >= 20 && n < 20 + PATROLS ? n - 20 : -1;
}

void read_trains(const Parsed& p, Mission& m) {
    auto define_int = [&](const std::string& name) {
        auto it = p.defines.find(name);
        return it == p.defines.end() ? 0 : to_int(it->second);
    };
    auto array = [&](const char* name) {
        auto it = p.arrays.find(name);
        return it == p.arrays.end() ? std::vector<std::string>() : it->second;
    };
    auto patrol = [&](EnemyTrain& t, const std::string& task) {
        const int k = patrol_of(task);
        if (k < 0) return;
        const std::string n = "ATA_PATROL" + std::to_string(k) + "_";
        t.patrol = true;
        t.x1 = define_int(n + "X1");
        t.y1 = define_int(n + "Y1");
        t.x2 = define_int(n + "X2");
        t.y2 = define_int(n + "Y2");
    };
    const auto types = array("start_trains"), tasks = array("start_trains_task"), xs = array("start_trains_x"),
               ys = array("start_trains_y");
    for (size_t i = 0; i < types.size() && to_int(types[i], 0) != -1; ++i) {
        EnemyTrain t;
        t.type = types[i];
        if (i < xs.size()) t.x1 = to_int(xs[i]);
        if (i < ys.size()) t.y1 = to_int(ys[i]);
        if (i < tasks.size()) patrol(t, tasks[i]);
        m.start.push_back(t);
    }
    const auto times = array("level_queue_times"), trains = array("level_queue_trains"), queue_tasks = array("level_queue_task");
    for (size_t i = 0; i < times.size() && i < trains.size() && to_int(times[i]) > 0 && to_int(trains[i], 0) != -1; ++i) {
        EnemyTrain t;
        t.time = to_int(times[i]);
        t.type = trains[i];
        if (i < queue_tasks.size()) patrol(t, queue_tasks[i]);
        m.queue.push_back(t);
    }
}

std::string folder_of(const std::string& path) {
    const size_t slash = path.find_last_of('\\');
    return slash == std::string::npos ? "" : path.substr(0, slash);
}

std::string script_of(const std::string& map) {
    const size_t dot = map.find_last_of('.');
    return (dot == std::string::npos ? map : map.substr(0, dot)) + ".lgc";
}

std::string folder_name(const std::string& map) {   // maps\<name>\<name>.map -> <name>
    const std::string folder = folder_of(map);
    const size_t slash = folder.find_last_of('\\');
    return slash == std::string::npos ? folder : folder.substr(slash + 1);
}

// The title of a folder on the custom maps page, if it is there.
std::string custom_title(const std::string& folder) {
    const std::vector<std::string> lines = split_lines(read_file(CUSTOM_LIST));
    for (size_t i = 0; i + 1 < lines.size(); i += 2)
        if (lstrcmpiA(trim(lines[i]).c_str(), folder.c_str()) == 0) return trim(lines[i + 1]);
    return "";
}

Mission read_mission(const Parsed& p, bool block, const std::string& map) {
    Mission m;
    auto get = [&](const char* name, const std::string& fallback) {
        auto it = p.defines.find(name);
        return it == p.defines.end() ? fallback : it->second;
    };
    for (const auto& s : SETTINGS) m.settings[s[0]] = get(s[0], s[1]);
    m.money = to_int(get("MONEY", "0"));
    m.daytime = (std::max)(1, (std::min)(4, to_int(get("START_DAYNIGHT", "1"), 1)));
    m.rain = to_int(get("RAIN", "0")) != 0;
    m.nuke = to_int(get("NUKE", "1"), 1) != 0;   // daynight.lgc: lightning unless NUKE is 0
    m.reinforcements = to_int(get("PLAYER_REINFORCEMENT", "1"), 1);
    m.limit = to_int(get("LIMIT_ATACKERS", "2"), 2);
    auto train = p.arrays.find("player_reinforcement_train");
    if (train != p.arrays.end()) {
        m.train.clear();
        for (const std::string& v : train->second) {
            if (to_int(v) == 0) break;
            m.train += (m.train.empty() ? "" : ",") + std::to_string(to_int(v));
        }
    }
    read_trains(p, m);
    if (block)   // the block's own other defines stay; outside a block only the known settings count
        for (const std::string& name : p.define_order) {
            bool known = std::find(std::begin(OWN), std::end(OWN), name) != std::end(OWN) || starts_with(name, "ATA_PATROL");
            for (const auto& s : SETTINGS) known = known || name == s[0];
            if (!known) m.extra.push_back({name, p.defines.at(name)});
        }
    // the texts: the files the settings name, or those of the map's folder
    const std::string folder = folder_of(map);
    auto file = [&](const char* name, const char* own) {
        auto it = p.defines.find(name);
        return it != p.defines.end() ? unquote(it->second) : folder + "\\" + own;
    };
    m.objectives = read_text(file("OBJECTIVES_FILE", "objectives.txt"));
    m.win = read_text(file("WIN_FILE", "win.txt"));
    m.lose = read_text(file("LOOSE_FILE", "loose.txt"));
    for (const std::string& page : split_lines(read_file(file("BRIEF_LIST", "brief.lst")))) {
        const std::string name = trim(page);
        if (name.empty() || name.find("hiscores") != std::string::npos) continue;   // the high scores page
        m.brief.push_back(read_text(name));
    }
    std::string title = p.title;
    if (title.empty()) title = custom_title(folder_name(map));
    if (title.empty()) title = upper(folder_name(map));
    m.title = from_pack(title);
    auto header = p.defines.find("BRIEF_HEADER");
    m.header = header != p.defines.end() ? from_pack(unquote(header->second)) : m.title;
    return m;
}

// The block of the settings for the map `map` (a game path).
std::vector<std::string> settings_block(const Mission& m, const std::string& map) {
    std::vector<std::string> out;
    const std::string folder = folder_of(map);
    auto define = [&](const std::string& name, const std::string& value) { out.push_back("#define " + name + " " + value); };
    auto array = [&](const std::string& name, const std::vector<std::string>& items) {
        std::string line = "int " + name + "[]={";
        for (size_t i = 0; i < items.size(); ++i) line += (i ? "," : "") + items[i];
        out.push_back(line + "};");
    };
    out.push_back("// title: " + ascii_upper(m.title, 24));
    define("LEVEL", m.settings.count("LEVEL") ? m.settings.at("LEVEL") : "0");
    define("CUSTOM_MAP", quoted_path(map));
    define("MONEY", std::to_string(m.money));
    define("START_DAYNIGHT", std::to_string(m.daytime));
    define("RAIN", m.rain ? "1" : "0");
    define("NUKE", m.nuke ? "1" : "0");
    for (const auto& s : SETTINGS)
        if (std::strcmp(s[0], "LEVEL") != 0) define(s[0], m.settings.count(s[0]) ? m.settings.at(s[0]) : s[1]);
    define("OBJECTIVES_FILE", quoted_path(folder + "\\objectives.txt"));
    define("WIN_FILE", quoted_path(folder + "\\win.txt"));
    define("LOOSE_FILE", quoted_path(folder + "\\loose.txt"));
    define("BRIEF_LIST", quoted_path(folder + "\\brief.lst"));
    define("BRIEF_HEADER", "\"" + ascii_upper(m.header.empty() ? m.title : m.header, 32) + "\"");
    define("HISCORES_FILE", quoted_path(folder + "\\hiscores"));
    define("PLAYER_REINFORCEMENT", std::to_string(m.reinforcements));
    std::vector<std::string> train;
    for (size_t a = 0; a <= m.train.size();) {
        const size_t b = m.train.find(',', a);
        const std::string v = trim(m.train.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (to_int(v) > 0) train.push_back(std::to_string(to_int(v)));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    train.push_back("0");
    array("player_reinforcement_train", train);
    define("LIMIT_ATACKERS", std::to_string(m.limit));
    // the patrols, numbered over the start trains and the queue
    int patrols = 0;
    auto task = [&](const EnemyTrain& t) -> std::string {
        if (!t.patrol || patrols >= PATROLS) return "ATA_FIGHT";
        const std::string n = "ATA_PATROL" + std::to_string(patrols++);
        define(n + "_X1", std::to_string(t.x1));
        define(n + "_Y1", std::to_string(t.y1));
        define(n + "_X2", std::to_string(t.x2));
        define(n + "_Y2", std::to_string(t.y2));
        return n;
    };
    std::vector<std::string> types, tasks, xs, ys, renew, random, times, queue_types, queue_tasks;
    for (const EnemyTrain& t : m.start) {
        types.push_back(t.type);
        tasks.push_back(task(t));
        xs.push_back(std::to_string(t.x1));
        ys.push_back(std::to_string(t.y1));
        renew.push_back("1");
        random.push_back("0");
    }
    for (const EnemyTrain& t : m.queue) {
        times.push_back(std::to_string(t.time));
        queue_types.push_back(t.type);
        queue_tasks.push_back(task(t));
    }
    for (const auto& e : m.extra) define(e.first, e.second);
    out.push_back("#include \"maps\\script\\TrainsTactDeclares.lgc\"");
    types.push_back("-1");
    for (auto* v : {&tasks, &xs, &ys, &renew, &random, &times, &queue_tasks}) v->push_back("0");
    queue_types.push_back("-1");
    array("start_trains", types);
    array("start_trains_task", tasks);
    array("start_trains_x", xs);
    array("start_trains_y", ys);
    array("start_trains_renew", renew);
    array("start_trains_random_task", random);
    array("level_queue_times", times);
    array("level_queue_trains", queue_types);
    array("level_queue_task", queue_tasks);
    return out;
}

std::vector<std::string> push_block(const std::string& indent) {
    std::vector<std::string> out;
    for (const PushLine& l : g_lines) {
        char line[96];
        std::snprintf(line, sizeof line, "SetPushLine(%d, %d, %d, %d, %d);", l.bx, l.by, l.ex, l.ey, l.flag);
        out.push_back(indent + line);
    }
    for (const TrainPatrol& p : edit_train_patrols()) {   // the trains' patrols, as the held level has them
        char line[160];
        std::snprintf(line, sizeof line, "PatrolEngine(FindNearestSprite(GETSPRITE_VID+%d, %d, %d, 20), %d, %d);", p.nvid, p.x,
                      p.y, p.to_x, p.to_y);
        out.push_back(indent + line);
    }
    return out;
}

bool parse_patrol(const std::string& raw, TrainPatrol& out) {
    const std::string code = code_of(raw);
    const size_t at = code.find("PatrolEngine(FindNearestSprite(GETSPRITE_VID+");
    if (at == std::string::npos) return false;
    int r = 0;
    return std::sscanf(code.c_str() + at, "PatrolEngine(FindNearestSprite(GETSPRITE_VID+%d , %d , %d , %d ) , %d , %d )", &out.nvid,
                       &out.x, &out.y, &r, &out.to_x, &out.to_y) == 6;
}

// ---- custom.lst ----

void add_to_custom_list(const std::string& folder, const std::string& title) {
    std::vector<std::string> lines;
    for (const std::string& line : split_lines(read_file(CUSTOM_LIST)))
        if (!trim(line).empty()) lines.push_back(trim(line));
    bool found = false;
    for (size_t i = 0; i + 1 < lines.size(); i += 2)
        if (lstrcmpiA(lines[i].c_str(), folder.c_str()) == 0) {
            lines[i + 1] = title;
            found = true;
        }
    if (!found) {   // the page scrolls: the list has any length
        lines.push_back(folder);
        lines.push_back(title);
    }
    std::string data;
    for (size_t i = 0; i < lines.size(); ++i) data += (i ? "\r\n" : "") + lines[i];   // as the repository's
    if (write_file(CUSTOM_LIST, data)) patch::log("mission: %s on the custom maps page as %s", folder.c_str(), title.c_str());
}

// ---- the dialog ----

enum : int {
    IDC_TITLE = 100, IDC_HEADER, IDC_MONEY, IDC_TIME, IDC_RAIN, IDC_NUKE, IDC_LIMIT, IDC_WAVES, IDC_TRAIN, IDC_START,
    IDC_QUEUE, IDC_OBJECTIVES, IDC_WIN, IDC_LOSE, IDC_BRIEF
};


std::wstring widen(const std::string& s) { return from_pack(s); }

std::wstring train_lines(const std::vector<EnemyTrain>& trains, bool queue) {
    std::wstring out;
    for (const EnemyTrain& t : trains) {
        char line[128];
        const std::string type = type_name(t.type);
        if (queue && t.patrol) std::snprintf(line, sizeof line, "%d %s %d %d %d %d", t.time, type.c_str(), t.x1, t.y1, t.x2, t.y2);
        else if (queue) std::snprintf(line, sizeof line, "%d %s", t.time, type.c_str());
        else if (t.patrol) std::snprintf(line, sizeof line, "%s %d %d %d %d", type.c_str(), t.x1, t.y1, t.x2, t.y2);
        else std::snprintf(line, sizeof line, "%s %d %d", type.c_str(), t.x1, t.y1);
        out += (out.empty() ? L"" : L"\r\n") + widen(line);
    }
    return out;
}

// The trains of an edit box; false with the line in `error` when one cannot be read.
bool parse_trains(const std::wstring& text, bool queue, std::vector<EnemyTrain>& out, std::string& error) {
    out.clear();
    int number = 0;
    for (const std::string& raw : split_lines(to_pack(text))) {
        ++number;
        const std::string line = trim(raw);
        if (line.empty()) continue;
        std::vector<std::string> words;
        for (size_t a = 0; a < line.size();) {
            const size_t b = line.find_first_of(" \t,", a);
            const std::string w = line.substr(a, b == std::string::npos ? std::string::npos : b - a);
            if (!w.empty()) words.push_back(w);
            if (b == std::string::npos) break;
            a = b + 1;
        }
        EnemyTrain t;
        size_t i = 0;
        if (queue) {
            t.time = to_int(words[i++], -1);
            if (t.time <= 0) {
                error = ui_printf(ui_format("ErrLineSeconds", "line %d: the time in seconds first"), number);
                return false;
            }
        }
        if (i >= words.size() || !type_token(words[i++], t.type)) {
            error = ui_printf(ui_format("ErrLineType", "line %d: an unknown train type"), number);
            return false;
        }
        std::vector<int> v;
        for (; i < words.size(); ++i) {
            const int n = to_int(words[i], INT_MIN);
            if (n == INT_MIN) {
                error = ui_printf(ui_format("ErrLineNumber", "line %d: \"%s\" is not a number"), number, words[i].c_str());
                return false;
            }
            v.push_back(n);
        }
        if (v.size() == 4) {
            t.patrol = true;
            t.x1 = v[0];
            t.y1 = v[1];
            t.x2 = v[2];
            t.y2 = v[3];
        } else if (v.size() == 2 && !queue) {
            t.x1 = v[0];
            t.y1 = v[1];
        } else if (!(v.empty() && queue)) {
            error = queue ? ui_printf(ui_format("ErrLineQueue", "line %d: seconds type, or seconds type x1 y1 x2 y2"), number)
                          : ui_printf(ui_format("ErrLineStart", "line %d: type x y, or type x1 y1 x2 y2"), number);
            return false;
        }
        out.push_back(t);
    }
    return true;
}

std::wstring brief_text(const std::vector<std::wstring>& pages) {
    std::wstring out;
    for (size_t i = 0; i < pages.size(); ++i) out += (i ? L"\r\n---\r\n" : L"") + pages[i];
    return out;
}

std::vector<std::wstring> brief_pages(const std::wstring& text) {
    std::vector<std::wstring> pages(1);
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find(L'\n', start);
        std::wstring line = text.substr(start, end == std::wstring::npos ? std::wstring::npos : end - start);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line == L"---") pages.emplace_back();
        else pages.back() += (pages.back().empty() ? L"" : L"\r\n") + line;
        if (end == std::wstring::npos) break;
        start = end + 1;
    }
    pages.erase(std::remove_if(pages.begin(), pages.end(), [](const std::wstring& p) { return p.empty(); }), pages.end());
    return pages;
}

std::wstring item_text(HWND dialog, int id) {
    HWND item = GetDlgItem(dialog, id);
    std::wstring text(GetWindowTextLengthW(item) + 1, L'\0');
    text.resize(GetWindowTextW(item, &text[0], static_cast<int>(text.size())));
    return text;
}

// What the dialog's boxes hold (by their ids), and the mission they make; an error message when
// a box cannot be read.
using Fields = std::map<int, std::wstring>;

std::string apply_fields(const Fields& f, Mission& n) {
    auto get = [&](int id) {
        auto it = f.find(id);
        return it == f.end() ? std::wstring() : it->second;
    };
    auto number = [&](int id, int& out) {
        const std::string v = trim(to_pack(get(id)));
        out = to_int(v, INT_MIN);
        return out != INT_MIN;
    };
    std::string error, trains;
    n.title = get(IDC_TITLE);
    n.header = get(IDC_HEADER);
    if (!number(IDC_MONEY, n.money)) error = ui_text("ErrMoney", "Money: a number");
    int time = 1;
    if (number(IDC_TIME, time)) n.daytime = (std::max)(1, (std::min)(4, time));
    n.rain = get(IDC_RAIN) == L"1";
    n.nuke = get(IDC_NUKE) == L"1";
    if (!number(IDC_LIMIT, n.limit) && error.empty()) error = ui_text("ErrAttackers", "Enemy attackers at once: a number");
    if (!number(IDC_WAVES, n.reinforcements) && error.empty()) error = ui_text("ErrWaves", "Reinforcement waves: a number");
    n.train = to_pack(get(IDC_TRAIN));
    if (error.empty() && !parse_trains(get(IDC_START), false, n.start, trains))
        error = ui_printf(ui_format("ErrStartTrains", "Enemy trains at the start, %s"), trains.c_str());
    if (error.empty() && !parse_trains(get(IDC_QUEUE), true, n.queue, trains))
        error = ui_printf(ui_format("ErrQueue", "Enemy trains from its factory, %s"), trains.c_str());
    int patrols = 0;
    for (const auto* list : {&n.start, &n.queue})
        for (const EnemyTrain& t : *list) patrols += t.patrol;
    if (error.empty() && patrols > PATROLS) error = ui_text("ErrPatrols", "At most ten patrolling trains (ATA_PATROL0 ... 9)");
    n.objectives = get(IDC_OBJECTIVES);
    n.win = get(IDC_WIN);
    n.lose = get(IDC_LOSE);
    n.brief = brief_pages(get(IDC_BRIEF));
    if (ascii_upper(n.title, 24).empty() && error.empty()) error = ui_text("ErrTitle", "The title of the map");
    return error;
}

INT_PTR CALLBACK mission_proc(HWND dialog, UINT m, WPARAM w, LPARAM) {
    Mission& s = g_mission;
    if (m == WM_INITDIALOG) {
        SetDlgItemTextW(dialog, IDC_TITLE, s.title.c_str());
        SetDlgItemTextW(dialog, IDC_HEADER, s.header.c_str());
        SetDlgItemInt(dialog, IDC_MONEY, s.money, TRUE);
        for (const std::wstring& t : {ui_wtext("TimeDay", "day"), ui_wtext("TimeEvening", "evening"),
                                      ui_wtext("TimeNight", "night"), ui_wtext("TimeMorning", "morning")})
            SendDlgItemMessageW(dialog, IDC_TIME, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
        SendDlgItemMessageW(dialog, IDC_TIME, CB_SETCURSEL, s.daytime - 1, 0);
        CheckDlgButton(dialog, IDC_RAIN, s.rain ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(dialog, IDC_NUKE, s.nuke ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(dialog, IDC_LIMIT, s.limit, TRUE);
        SetDlgItemInt(dialog, IDC_WAVES, s.reinforcements, TRUE);
        SetDlgItemTextW(dialog, IDC_TRAIN, widen(s.train).c_str());
        SetDlgItemTextW(dialog, IDC_START, train_lines(s.start, false).c_str());
        SetDlgItemTextW(dialog, IDC_QUEUE, train_lines(s.queue, true).c_str());
        SetDlgItemTextW(dialog, IDC_OBJECTIVES, s.objectives.c_str());
        SetDlgItemTextW(dialog, IDC_WIN, s.win.c_str());
        SetDlgItemTextW(dialog, IDC_LOSE, s.lose.c_str());
        SetDlgItemTextW(dialog, IDC_BRIEF, brief_text(s.brief).c_str());
        return TRUE;
    }
    if (m != WM_COMMAND) return FALSE;
    if (LOWORD(w) == IDCANCEL) {
        EndDialog(dialog, 0);
        return TRUE;
    }
    if (LOWORD(w) != IDOK) return FALSE;
    Fields fields;
    for (int id = IDC_TITLE; id <= IDC_BRIEF; ++id) fields[id] = item_text(dialog, id);
    fields[IDC_TIME] = std::to_wstring(SendDlgItemMessageW(dialog, IDC_TIME, CB_GETCURSEL, 0, 0) + 1);
    fields[IDC_RAIN] = IsDlgButtonChecked(dialog, IDC_RAIN) == BST_CHECKED ? L"1" : L"0";
    fields[IDC_NUKE] = IsDlgButtonChecked(dialog, IDC_NUKE) == BST_CHECKED ? L"1" : L"0";
    Mission n = s;
    const std::string error = apply_fields(fields, n);
    if (!error.empty()) {
        MessageBoxW(dialog, ui_wide(error).c_str(), ui_wtext("MissionTitle", "Mission settings").c_str(), MB_OK | MB_ICONWARNING);
        return TRUE;
    }
    s = n;
    EndDialog(dialog, 1);
    return TRUE;
}
// The boxes as the dialog starts with them.
Fields current_fields() {
    const Mission& s = g_mission;
    return {{IDC_TITLE, s.title},
            {IDC_HEADER, s.header},
            {IDC_MONEY, std::to_wstring(s.money)},
            {IDC_TIME, std::to_wstring(s.daytime)},
            {IDC_RAIN, s.rain ? L"1" : L"0"},
            {IDC_NUKE, s.nuke ? L"1" : L"0"},
            {IDC_LIMIT, std::to_wstring(s.limit)},
            {IDC_WAVES, std::to_wstring(s.reinforcements)},
            {IDC_TRAIN, widen(s.train)},
            {IDC_START, train_lines(s.start, false)},
            {IDC_QUEUE, train_lines(s.queue, true)},
            {IDC_OBJECTIVES, s.objectives},
            {IDC_WIN, s.win},
            {IDC_LOSE, s.lose},
            {IDC_BRIEF, brief_text(s.brief)}};
}

}  // namespace
std::vector<PushLine>& mission_push_lines() { return g_lines; }
void mission_list_map(const std::string& folder, const std::string& title) { add_to_custom_list(folder, title); }
std::string mission_listed_title(const std::string& folder) { return custom_title(folder); }
const std::vector<TrainPatrol>& mission_patrols() { return g_patrols; }
const std::vector<PushLine>& mission_script_push_lines() { return g_script_lines; }
bool mission_managed() { return g_managed; }

void mission_load(const std::string& map) {
    g_mission = Mission();
    g_lines.clear();
    g_script_lines.clear();
    g_patrols.clear();
    g_map = map;
    const std::vector<std::string> lines = split_lines(read_file(script_of(map)));
    const Blocks b = find_blocks(lines);
    g_managed = b.found();
    Parsed p;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const bool in_block = i > b.open && i < b.close, in_main = i > b.main_open && i < b.main_close;
        if (!g_managed || in_block) parse_line(lines[i], p);
        PushLine l;
        if (trim(lines[i]).rfind("//", 0) != 0 && parse_push_line(lines[i], l))
            (g_managed && in_main ? g_lines : g_script_lines).push_back(l);
        TrainPatrol t;
        if (g_managed && in_main && trim(lines[i]).rfind("//", 0) != 0 && parse_patrol(lines[i], t)) g_patrols.push_back(t);
    }
    g_mission = read_mission(p, g_managed, map);
    patch::log("mission: %s - %s, %u push lines of the editor, %u of the script, %u start trains, %u in the queue",
               script_of(map).c_str(), g_managed ? "an editor mission" : "no editor blocks",
               static_cast<unsigned>(g_lines.size()), static_cast<unsigned>(g_script_lines.size()),
               static_cast<unsigned>(g_mission.start.size()), static_cast<unsigned>(g_mission.queue.size()));
}

bool mission_dialog(HWND owner) {
    using namespace dialog;
    DialogTemplate t(ui_wtext("MissionTitle", "Mission settings").c_str(), 420, 330);
    const DWORD label = SS_LEFT;
    t.item(STATIC, ui_wtext("MissionMapTitle", "Title on the custom maps page:").c_str(), -1, label, 8, 8, 104, 8);
    t.item(EDIT, L"", IDC_TITLE, LINE, 112, 6, 116, 12);
    t.item(STATIC, ui_wtext("MissionHeader", "Briefing header:").c_str(), -1, label, 236, 8, 60, 8);
    t.item(EDIT, L"", IDC_HEADER, LINE, 296, 6, 116, 12);
    t.item(STATIC, ui_wtext("MissionMoney", "Money:").c_str(), -1, label, 8, 26, 28, 8);
    t.item(EDIT, L"", IDC_MONEY, LINE | ES_NUMBER, 36, 24, 40, 12);
    t.item(STATIC, ui_wtext("MissionStartAt", "Start at:").c_str(), -1, label, 86, 26, 32, 8);
    t.item(COMBOBOX, L"", IDC_TIME, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 118, 24, 56, 80);
    t.item(BUTTON, ui_wtext("MissionRain", "Rain").c_str(), IDC_RAIN, BS_AUTOCHECKBOX | WS_TABSTOP, 184, 24, 34, 12);
    t.item(BUTTON, ui_wtext("MissionLightning", "Lightning").c_str(), IDC_NUKE, BS_AUTOCHECKBOX | WS_TABSTOP, 222, 24, 50, 12);
    t.item(STATIC, ui_wtext("MissionAttackers", "Enemy attackers at once:").c_str(), -1, label, 290, 26, 90, 8);
    t.item(EDIT, L"", IDC_LIMIT, LINE | ES_NUMBER, 380, 24, 32, 12);
    t.item(STATIC, ui_wtext("MissionWaves", "Player's reinforcement waves:").c_str(), -1, label, 8, 44, 102, 8);
    t.item(EDIT, L"", IDC_WAVES, LINE | ES_NUMBER, 112, 42, 24, 12);
    t.item(STATIC, ui_wtext("MissionTrain", "its train (VIDs, e.g. 5,20):").c_str(), -1, label, 146, 44, 92, 8);
    t.item(EDIT, L"", IDC_TRAIN, LINE, 238, 42, 174, 12);
    t.item(STATIC,
           ui_wtext("MissionStartTrains", "Enemy trains at the start, one a line: type x y (fights from there), or type "
                                          "x1 y1 x2 y2 (patrols between two rail points; screen coordinates - the Push "
                                          "line tool shows the point under the cursor):").c_str(),
           -1, label, 8, 60, 404, 16);
    t.item(EDIT, L"", IDC_START, MULTILINE, 8, 78, 404, 34);
    t.item(STATIC, ui_wtext("MissionQueue", "Enemy trains from its factory, one a line: seconds type, or seconds type "
                                        "x1 y1 x2 y2 (patrols):").c_str(),
           -1, label, 8, 116, 404, 8);
    t.item(EDIT, L"", IDC_QUEUE, MULTILINE, 8, 126, 404, 28);
    t.item(STATIC,
           ui_wtext("MissionTypes", "Types: light_, medium_, hard_, very_hard_, diff_ or defence_ with gun, mortair, "
                                    "rocket, air, fire or desant (e.g. light_gun).").c_str(),
           -1, label, 8, 158, 404, 8);
    t.item(STATIC, ui_wtext("MissionObjectives", "Objectives:").c_str(), -1, label, 8, 172, 200, 8);
    t.item(EDIT, L"", IDC_OBJECTIVES, MULTILINE, 8, 182, 200, 30);
    t.item(STATIC, ui_wtext("MissionWin", "Win:").c_str(), -1, label, 8, 216, 200, 8);
    t.item(EDIT, L"", IDC_WIN, MULTILINE, 8, 226, 200, 24);
    t.item(STATIC, ui_wtext("MissionLose", "Lose:").c_str(), -1, label, 8, 254, 200, 8);
    t.item(EDIT, L"", IDC_LOSE, MULTILINE, 8, 264, 200, 24);
    t.item(STATIC, ui_wtext("MissionBrief", "Briefing (a line --- starts the next page):").c_str(), -1, label, 214, 172, 198, 8);
    t.item(EDIT, L"", IDC_BRIEF, MULTILINE, 214, 182, 198, 106);
    t.item(STATIC,
           g_managed ? ui_wtext("MissionManaged", "The texts are written in capitals, 44 characters a line, when the map "
                                                  "is saved.").c_str()
                     : ui_wtext("MissionUnmanaged", "This script has no editor blocks: the settings are written when the "
                                                    "map is saved under a new name.").c_str(),
           -1, label, 8, 294, 404, 16);
    t.item(BUTTON, ui_wtext("ButtonOk", "OK").c_str(), IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP, 304, 312, 50, 14);
    t.item(BUTTON, ui_wtext("ButtonCancel", "Cancel").c_str(), IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 362, 312, 50, 14);
    const INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), t.get(), owner, mission_proc, 0);
    if (result == 1) patch::log("mission: settings changed");
    return result == 1;
}

void mission_save(const std::string& map) {
    const std::string script = script_of(map);
    if (!exists(script)) {
        // a new map: the script of the map it comes from when that is an editor mission, else
        // the template's, which gets the old script's settings and push lines
        std::string base;
        if (g_managed) base = read_file(script_of(g_map));
        if (base.empty()) base = read_file(TEMPLATE_SCRIPT);
        if (base.empty()) base = read_file(script_of(g_map));
        if (base.empty() || !write_file(script, base)) {
            patch::log("mission: no script for %s", map.c_str());
            return;
        }
        if (!g_managed) {
            for (const PushLine& l : g_script_lines)
                if (std::find(g_lines.begin(), g_lines.end(), l) == g_lines.end()) g_lines.push_back(l);
            g_script_lines.clear();
        }
        patch::log("mission: %s made from %s", script.c_str(),
                   g_managed ? script_of(g_map).c_str() : (exists(TEMPLATE_SCRIPT) ? TEMPLATE_SCRIPT : "the map's script"));
    }
    const std::string text = read_file(script);
    std::vector<std::string> lines = split_lines(text);
    const Blocks b = find_blocks(lines);
    if (!b.found()) {
        patch::log("mission: %s has no editor blocks, its settings are not written", script.c_str());
        g_managed = false;
        g_map = map;
        return;
    }
    // a map saved from the template keeps nothing of its name: "TEMPLATE" as the title (the custom
    // maps page) or the briefing's header (the Esc menu's first page) becomes the map's own name
    if (lstrcmpiA(folder_name(map).c_str(), "template") != 0) {
        const bool title = ascii_upper(g_mission.title, 24) == "TEMPLATE";
        if (title) g_mission.title = from_pack(upper(folder_name(map)));
        if (title || ascii_upper(g_mission.header, 32) == "TEMPLATE") g_mission.header = g_mission.title;
    }
    // the blocks anew; main()'s block keeps the indentation of its marker
    const std::string indent = lines[b.main_open].substr(0, lines[b.main_open].find_first_not_of(" \t"));
    std::vector<std::string> out(lines.begin(), lines.begin() + b.open + 1);
    for (const std::string& line : settings_block(g_mission, map)) out.push_back(line);
    out.insert(out.end(), lines.begin() + b.close, lines.begin() + b.main_open + 1);
    for (const std::string& line : push_block(indent)) out.push_back(line);
    out.insert(out.end(), lines.begin() + b.main_close, lines.end());
    const char* eol = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::string data;
    for (size_t i = 0; i < out.size(); ++i) data += out[i] + (i + 1 < out.size() ? eol : "");
    if (!write_file(script, data)) return;
    g_managed = true;
    g_map = map;
    // the texts
    const std::string folder = folder_of(map);
    write_file(folder + "\\objectives.txt", text_file(g_mission.objectives));
    write_file(folder + "\\win.txt", text_file(g_mission.win));
    write_file(folder + "\\loose.txt", text_file(g_mission.lose));
    std::vector<std::wstring> pages = g_mission.brief;
    if (pages.empty()) pages.push_back(g_mission.objectives);
    std::string list;
    for (size_t i = 0; i < pages.size(); ++i) {
        const std::string page = folder + (i ? "\\brief" + std::to_string(i + 1) + ".txt" : "\\brief.txt");
        write_file(page, text_file(pages[i]));
        list += (i ? "\r\n" : "") + page;
    }
    write_file(folder + "\\brief.lst", list);
    patch::log("mission: %s written - %u push lines, %u start trains, %u in the queue, %u briefing pages",
               script.c_str(), static_cast<unsigned>(g_lines.size()), static_cast<unsigned>(g_mission.start.size()),
               static_cast<unsigned>(g_mission.queue.size()), static_cast<unsigned>(pages.size()));
    // maps\<name>\<name>.map: the custom maps page (not the template itself)
    const std::string name = folder_name(map);
    const size_t slash = map.find_last_of('\\'), dot = map.find_last_of('.');
    const std::string base = slash == std::string::npos ? map : map.substr(slash + 1, dot - slash - 1);
    if (lstrcmpiA(folder.c_str(), ("maps\\" + name).c_str()) == 0 && lstrcmpiA(base.c_str(), name.c_str()) == 0 &&
        lstrcmpiA(name.c_str(), "template") != 0)
        add_to_custom_list(name, ascii_upper(g_mission.title, 24));
}

// Test mode (editor.cpp's WM_COPYDATA 40042): the settings of a file instead of the dialog - UTF-8
// lines "key: value" with the keys title, header, money, time (1-4), rain, nuke (0/1), limit,
// waves, train, start and queue (a line per train), objectives, win, lose and brief ("\n" in a
// value is a line end, "---" between briefing pages); the keys not given stay as they are.
bool mission_settings_from_file(const std::string& path) {
    static const std::map<std::string, int> KEYS = {
        {"title", IDC_TITLE}, {"header", IDC_HEADER}, {"money", IDC_MONEY}, {"time", IDC_TIME},
        {"rain", IDC_RAIN}, {"nuke", IDC_NUKE}, {"limit", IDC_LIMIT}, {"waves", IDC_WAVES},
        {"train", IDC_TRAIN}, {"start", IDC_START}, {"queue", IDC_QUEUE}, {"objectives", IDC_OBJECTIVES},
        {"win", IDC_WIN}, {"lose", IDC_LOSE}, {"brief", IDC_BRIEF}};
    Fields fields = current_fields();
    std::map<int, bool> cleared;
    for (const std::string& line : split_lines(read_file(path))) {
        const size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        auto key = KEYS.find(trim(line.substr(0, colon)));
        if (key == KEYS.end()) continue;
        std::string value = trim(line.substr(colon + 1));
        for (size_t at = value.find("\\n"); at != std::string::npos; at = value.find("\\n", at + 2))
            value.replace(at, 2, "\r\n");
        std::wstring wide(MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0), L'\0');
        if (!wide.empty())
            MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), &wide[0], static_cast<int>(wide.size()));
        std::wstring& field = fields[key->second];
        const bool list = key->second == IDC_START || key->second == IDC_QUEUE;
        if (list && cleared[key->second]) field += L"\r\n" + wide;
        else field = wide;
        cleared[key->second] = true;
    }
    Mission n = g_mission;
    const bool english = ui_english(true);   // LocoFix.log in English
    const std::string error = apply_fields(fields, n);
    ui_english(english);
    if (!error.empty()) {
        patch::log("mission test: %s", error.c_str());
        return false;
    }
    g_mission = n;
    patch::log("mission test: settings of %s - %u start trains, %u in the queue", path.c_str(),
               static_cast<unsigned>(n.start.size()), static_cast<unsigned>(n.queue.size()));
    return true;
}