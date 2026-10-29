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
// and, in a map whose arrivals the editor runs (the Arrivals panel, below "arrivals"):
//  - "// <editor arrivals>" ... "// </editor arrivals>" after #include "maps\script\header.lgc":
//    ArrivalsTact(), which brings the trains, and ArrivalsLeft(army), the waves still to come,
//  - "// <editor tact>" ... "// </editor tact>" after main()'s TrainsTact(): ArrivalsTact();
// All of them are written anew when the map is saved; the rest of the file is the mission's own
// code and stays as it is. A new map gets the script of the map it was saved from when that one has
// the blocks, otherwise the template's (maps\template\template.lgc, map_tool.py new --script) -
// the settings and the SetPushLine calls read from the old script go into the new blocks. A
// script without the blocks (the campaign) is not changed: its settings are shown and its push
// lines drawn, read-only.
//
// The texts are the map's files, in the pack's code page in capitals (the fonts have no small letters), at most 44
// characters a line (the briefing's width), CRLF: objectives.txt, win.txt, loose.txt and the
// briefing pages brief.txt, brief2.txt ... listed in brief.lst (with the high scores page,
// hiscores.txt, last when the list had it - the game writes that one); a text the language pack
// has a copy of (the sandbox's, lang\<code>\maps\sandbox) is written to that copy. A map saved as
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
#include <functional>
#include <map>
#include <set>

std::vector<TrainPatrol> edit_train_patrols();   // editor_edit.cpp: the trains' patrols of the held level
void edit_message_text(const char* key, const char* english);   // editor_edit.cpp: a message of the status line
bool editor_unsaved_map(const std::string& map);   // editor.cpp: the editor's new map, not saved yet
std::string editor_new_map_title();                // editor.cpp: its title until it is saved
std::vector<UnitUnlock> edit_place_unlocks(std::vector<UnitUnlock>& unlocks);   // editor_edit.cpp: the factories' places now, the units to write
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
                           "PLAYER_REINFORCEMENT", "LIMIT_ATACKERS", "ARRIVALS", "UNLOCKS", "UNIT_LIMITS", "ORDERS"};
constexpr int POSTS = 10;               // ATA_STAND0 ... ATA_STAND9

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
    // the weather (daynight.lgc, environmenttact.lgc): RAIN 0 none, 1 now and then (it comes and goes,
    // about 30 s each), 2 all the time - daynight.lgc's comment has 1 and 2 the other way round, its
    // code does this -, of the kind RAIN_TYPE: 0 rain, 1 snow, 2 clouds, 3 storm clouds, 4 meteorites
    int rain = 0, weather = 0;
    bool nuke = false;   // lightning: in a clear sky (RAIN 0), else only while rain (not snow ...) falls
    std::string train = "5";   // the reinforcement's VIDs
    std::vector<EnemyTrain> start;
    FactoryState factories;   // the enemy's queue (level_queue_*), the units that come later, the unit limits
    std::wstring objectives, win, lose;
    std::vector<std::wstring> brief;
    bool scores_page = false;   // brief.lst ends with the high scores page (hiscores.txt)
    std::map<std::string, std::string> settings;   // SETTINGS as read
    std::vector<std::pair<std::string, std::string>> extra;   // other defines of the block
};

Mission g_mission;
std::vector<PushLine> g_lines, g_script_lines;
std::vector<TrainPatrol> g_patrols;   // read from main()'s block; written from the held level's trains
bool g_managed = false;
std::string g_map;   // the map the mission was read from or saved with
ArrivalsState g_arrivals;
int g_arrivals_refusal = 0;   // why the script cannot run arrivals: 0 it can, 1 no editor blocks, 2 no TrainsTact();

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
    int arrivals_open = -1, arrivals_close = -1, tact_open = -1, tact_close = -1;
    int header = -1, trains_tact = -1;   // #include "maps\script\header.lgc", the first TrainsTact(); after it
    bool found() const { return open >= 0 && close > open && main_open >= 0 && main_close > main_open; }
    bool arrivals() const { return arrivals_open >= 0 && arrivals_close > arrivals_open && tact_open >= 0 && tact_close > tact_open; }
};

Blocks find_blocks(const std::vector<std::string>& lines) {
    Blocks b;
    for (int i = 0; i < static_cast<int>(lines.size()); ++i) {
        const std::string t = trim(lines[i]);
        if (starts_with(t, "// <editor>")) b.open = i;
        else if (starts_with(t, "// </editor>")) b.close = i;
        else if (starts_with(t, "// <editor main>")) b.main_open = i;
        else if (starts_with(t, "// </editor main>")) b.main_close = i;
        else if (starts_with(t, "// <editor arrivals>")) b.arrivals_open = i;
        else if (starts_with(t, "// </editor arrivals>")) b.arrivals_close = i;
        else if (starts_with(t, "// <editor tact>")) b.tact_open = i;
        else if (starts_with(t, "// </editor tact>")) b.tact_close = i;
        const std::string code = trim(code_of(t));
        if (b.header < 0 && starts_with(code, "#include") && lstrcmpiA(code.c_str(), "#include \"maps\\script\\header.lgc\"") == 0)
            b.header = i;
        else if (b.header >= 0 && b.trains_tact < 0 && code == "TrainsTact();") b.trains_tact = i;
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
}

// ---- arrivals ----
//
// The Arrivals panel's trains (editor_mission.h), in the settings' block as a column each:
//   before TrainsTactDeclares.lgc  #define ARRIVALS <n>, a train of its own cars as the authors
//                                  redefine a template (int this_trainNN[]={...,0}; NN a type no
//                                  template has: the ones digit 4, 6, 7 or 8 - 5 depends on the
//                                  difficulty, 9 is a defence train -, the tens digit the kind the
//                                  AI treats it as: 1x mortars, 2x rockets, 3x airships, 5x flame,
//                                  6x landing, 7x the super weapons, 0x the rest), the posts and
//                                  patrols of the enemy's ones (ATA_STANDn_X/Y, ATA_PATROLn_X1 ...
//                                  _Y2, numbered after the start trains' and the queue's patrols)
//   after it                       int arrival_army[]  0-3
//                                      arrival_type[]  the train type (arr_gettype)
//                                      arrival_x/y/z[] the entry: a rail point, screen x, y, its z
//                                      arrival_first[], arrival_every[], arrival_times[]  s, s, waves
//                                      arrival_if[], arrival_if_n[]  the condition (ArrivalIf),
//                                      arrival_or[]  1: it brings the wave earlier, 0: only then
//                                      arrival_task[]  the enemy's ATA_..., -1 none, -2 drive to:
//                                      arrival_to_x/y/z[]  the point (screen, its height)
//                                      arrival_raid[]  the enemy's raids, s; arrival_msg[]  0/1
//                                      arrival_done[], arrival_last[]  the waves that came, when
// each with a 0 after the last column (an array is never empty). The runner is the script's block
// "// <editor arrivals>" (ARRIVALS_RUNNER: the same in every map, so a map exported to another game
// runs as it is), called from the block "// <editor tact>" after main()'s TrainsTact().

const char* const ARRIVALS_RUNNER[] = {
    "// ArrivalsTact() brings the trains of the arrays arrival_*[] (the settings' block above), once a",
    "// second: a wave comes when its time is there (the first after arrival_first s, the next ones",
    "// arrival_every s after the one before, arrival_times of them or without end) and its condition",
    "// holds - or, with arrival_or, also earlier when the condition holds; a train the player pays for",
    "// comes only while the money is there and is paid once it is made. It waits while a train",
    "// stands at its entry (180 px) or the AI has its 16 trains. The cars are made on the entry's rail",
    "// point in the army; the enemy's train goes to the AI (TrainsTact) with its task, the others' last",
    "// car gets the order to drive to arrival_to. ArrivalsLeft(army): the waves of an army still to come.",
    "ArrivalTrains(int army)",
    "{",
    "  return GetVid(5,VID_COUNT0+army)+GetVid(10,VID_COUNT0+army)+GetVid(97,VID_COUNT0+army);",
    "}",
    "ArrivalIf(int i)",
    "{",
    "  int c,n,k,m;",
    "  c=arrival_if[i];",
    "  n=arrival_if_n[i];",
    "  if( c==1 )",
    "    return ArrivalTrains(0)==0;",
    "  if( c==2 )",
    "    return ArrivalTrains(0)<n;",
    "  if( c==3 )",
    "    return NoArmyUnits(0)<n;",
    "  if( c==4 )",
    "    return GetVid(102,VID_COUNT0)==0;",
    "  if( c==5 )",
    "    return GetVid(102,VID_COUNT2)>0;",
    "  if( c==6 )",
    "    return ArrivalTrains(1)<n;",
    "  if( c==7 )",
    "  {",
    "    m=0;",
    "    for( k=0; k<ARR_LENGTH; k++ )",
    "      if( ai_trains[k] && ai_trains_task[k]/10==1 )",
    "        m++;",
    "    return m<n;",
    "  }",
    "  if( c==8 )",
    "    return GetMoney(0)>=n;",
    "  if( c==9 )",
    "    return ArrivalTrains(0)==0 && GetVid(102,VID_COUNT2)>0;",
    "  return 1;",
    "}",
    "ArrivalsLeft(int army)",
    "{",
    "  int i,n;",
    "  n=0;",
    "  for( i=0; i<ARRIVALS; i++ )",
    "    if( arrival_army[i]==army )",
    "      if( !arrival_times[i] || arrival_done[i]<arrival_times[i] )",
    "        if( !arrival_if[i] || arrival_or[i] || ArrivalIf(i) )",
    "          n++;",
    "  return n;",
    "}",
    "ArrivalTry(int i,int t)",
    "{",
    "  int j,k,u,go;",
    "  string s;",
    "  if( arrival_times[i] && arrival_done[i]>=arrival_times[i] )",
    "    return 0;",
    "  if( arrival_done[i] )",
    "    go=t-arrival_last[i]>=arrival_every[i];",
    "  else",
    "    go=t>=arrival_first[i];",
    "  if( arrival_or[i] )",
    "  {",
    "    if( arrival_if[i] && ArrivalIf(i) )",
    "      go=1;",
    "  }",
    "  else if( !ArrivalIf(i) )",
    "    go=0;",
    "  if( !go )",
    "    return 0;",
    "  if( arrival_if[i]==8 && !ArrivalIf(i) )",
    "    return 0;",
    "  if( FindNearestSprite(GETSPRITE_CLASS+B_ENGINE,arrival_x[i],arrival_y[i]+arrival_z[i],180) )",
    "    return 0;",
    "  k=-1;",
    "  if( arrival_army[i]==1 )",
    "  {",
    "    k=find_num();",
    "    if( k<0 )",
    "      return 0;",
    "  }",
    "  arr_gettype(temp_array,arrival_type[i]);",
    "  u=0;",
    "  for( j=0; j<ARR_LENGTH && temp_array[j]; j++ )",
    "  {",
    "    u=Create(temp_array[j],arrival_x[i],arrival_y[i]+arrival_z[i],arrival_z[i]);",
    "    Action(u,ACT_SET_ARMY,arrival_army[i],0);",
    "    if( arrival_army[i] )",
    "      Action(u,ACT_SET_BEHAVE,Action(u,ACT_GET_BEHAVE)|1);",
    "  }",
    "  if( !u )",
    "    return 0;",
    "  if( arrival_if[i]==8 )",
    "    SetMoney(0,GetMoney(0)-arrival_if_n[i]);",
    "  if( k>=0 )",
    "  {",
    "    ai_trains[k]=u;",
    "    ai_trains_type[k]=arrival_type[i];",
    "    ai_trains_task[k]=arrival_task[i];",
    "    ai_trains_task2[k]=ATA_NONE;",
    "    ai_trains_task_backup[k]=ATA_NONE;",
    "    ai_trains_behave[k]=0;",
    "    ai_trains_time[k]=GetTime();",
    "    ai_trains_random_task[k]=arrival_raid[i];",
    "  }",
    "  else if( arrival_task[i]==-2 )",
    "    Action(u,ACT_MOVE,arrival_to_x[i],arrival_to_y[i]+arrival_to_z[i]);",
    "  if( arrival_msg[i] )",
    "  {",
    "    PlaySFX(100);",
    "    if( arrival_army[i]==1 )",
    "      Message(\"EnemyReinforcementArrived\",GetX(u),GetY(u));",
    "    else",
    "      Message(\"ReinforcementArrived\",GetX(u),GetY(u));",
    "  }",
    "  arrival_done[i]=arrival_done[i]+1;",
    "  arrival_last[i]=t;",
    "  s=\"ARRIVAL \"+itoa(i+1)+\" army \"+itoa(arrival_army[i])+\" wave \"+itoa(arrival_done[i]);",
    "  Log(s+\" at \"+itoa(t)+\" s task \"+itoa(arrival_task[i])+\" slot \"+itoa(k));",
    "  return 1;",
    "}",
    "// FactoryTact(t): the unit limits at the start (AddUnitLimit takes the limit first and the army",
    "// last), the units the factories get into their lists later (unlock_*[], the factory by its place).",
    "FactoryTact(int t)",
    "{",
    "  int i,u;",
    "  iff(1)",
    "  {",
    "    for( i=0; i<UNIT_LIMITS; i++ )",
    "      AddUnitLimit(limit_max[i],limit_vid[i],limit_army[i]);",
    "  }",
    "  for( i=0; i<UNLOCKS; i++ )",
    "    if( unlock_time[i]>=0 && t>=unlock_time[i] )",
    "    {",
    "      u=FindNearestSprite(GETSPRITE_VID+102,unlock_x[i],unlock_y[i],30);",
    "      if( u )",
    "        Action(u,ACT_ADD_ITEM,unlock_vid[i]);",
    "      Log(\"UNLOCK vid \"+itoa(unlock_vid[i])+\" at \"+itoa(t)+\" s, factory \"+itoa(u!=0));",
    "      unlock_time[i]=-1;",
    "    }",
    "}",
    "// CaptureTact(): a neutral factory or repair point becomes the player's when a train of the player's",
    "// comes within 100 px of it (the missions call LDepoCapture / LMasterCapture of Captures.lgc for",
    "// their one such building; forts and mining plants are captured by F162_0 of ai_default_event.lgc).",
    "CaptureTact()",
    "{",
    "  int b,v;",
    "  for( b=FirstUnit(u_monster); b; b=NextUnit() )",
    "  {",
    "    v=GetUnitVid(b);",
    "    if( (v==102 || v==104) && Action(b,act_getarmy,0,0)==2 )",
    "      if( FindNearestSprite(GETSPRITE_UNIT+GETSPRITE_CLASS+B_ENGINE+GETSPRITE_ARMY0,GetX(b),GetY(b),100) )",
    "      {",
    "        PlaySFX(25);",
    "        Effect(eff_nuke,0x006000);",
    "        Action(b,act_setarmy,0,0);",
    "        BuildingCapture(b);",
    "        Log(\"CAPTURE vid \"+itoa(v)+\" at \"+itoa(GetX(b))+\",\"+itoa(GetY(b)));",
    "      }",
    "  }",
    "}",
    "// OrdersTact(): the orders of the map's trains (order_*[]), once at the start: each train found by its",
    "// first car's VID and place, then driven or patrolling (-2, -3), the enemy's given to the AI with its task.",
    "OrdersTact()",
    "{",
    "  int i,u,k;",
    "  for( i=0; i<ORDERS; i++ )",
    "  {",
    "    u=FindNearestSprite(GETSPRITE_VID+order_vid[i],order_x[i],order_y[i],20);",
    "    k=-1;",
    "    if( u && order_task[i]==-2 )",
    "      Action(u,ACT_MOVE,order_to_x[i],order_to_y[i]+order_to_z[i]);",
    "    if( u && order_task[i]==-3 )",
    "      PatrolEngine(u,order_to_x[i],order_to_y[i]);",
    "    if( u && order_task[i]>=0 )",
    "    {",
    "      k=find_num();",
    "      if( k>=0 )",
    "      {",
    "        ai_trains[k]=u;",
    "        ai_trains_type[k]=order_type[i];",
    "        ai_trains_task[k]=order_task[i];",
    "        ai_trains_task2[k]=ATA_NONE;",
    "        ai_trains_task_backup[k]=ATA_NONE;",
    "        ai_trains_behave[k]=0;",
    "        ai_trains_time[k]=GetTime();",
    "        ai_trains_random_task[k]=0;",
    "      }",
    "    }",
    "    Log(\"ORDER \"+itoa(i+1)+\" task \"+itoa(order_task[i])+\" train \"+itoa(u!=0)+\" slot \"+itoa(k));",
    "  }",
    "}",
    "// PiecesTact(), every frame: the wholes the editor places work as in the missions. A citadel (VID",
    "// 149 with its tower 117) as l04 and l18 (pirates.lgc GratesTact): while its tower stands, the first",
    "// unit of another army than the tower's (the neutrals aside) within 100 px of it gets ACT_STOP and",
    "// 300 damage (SFX 45), its grates (146) rise 34 px in 1 s, stay 1 s and sink in 1 s; when the",
    "// tower is gone the grates stay down and its corkscrews (142) break (147). Charybdis (254 with its",
    "// eye 256) as l06: a unit of another army within 90 px is pulled in (ACT_MOVE to it) while its",
    "// teeth (255) chew (SFX 38), and one of the player's within 40 px is eaten - the enemy's, for a",
    "// Charybdis of the player's; its guns (258, 260) sleep until one of them or the eye is down to 80 %",
    "// or lost. A fly nest (265) as l06's two (its direction: 0 waves, 64 on",
    "// guard): green flies (263) in waves of 2-4 (2 for every AA gun of the map, less up to as many)",
    "// 25 s after the last of them is gone, or grey flies (262) on guard: 1-2 while a unit of another",
    "// army (not a factory or repair point) is within 400 px and none is left, 3 s after, and 6-9 when",
    "// the nest is destroyed; the flies are of the nest's army.",
    "int pc_n=-1,pc_x[8],pc_y[8],pc_z[8],pc_army[8],pc_g1[8],pc_g2[8],pc_mode[8],pc_time[8];",
    "int ch_n=-1,ch_x[4],ch_y[4],ch_army[4],ch_teeth[4],ch_guns[4],ch_peace[4],ch_ate[4],ch_time[4];",
    "int fn_n=-1,fn_x[8],fn_y[8],fn_army[8],fn_guard[8],fn_left[8],fn_vid[8],fn_time[8],fn_dead[8],fn_check=0;",
    "PieceFoe(int u,int army)",
    "{",
    "  int a;",
    "  a=Action(u,ACT_GET_ARMY);",
    "  return a!=army && a!=2;",
    "}",
    "PiecesGrates(int i,int z)",
    "{",
    "  if( pc_g1[i] )",
    "    Action(pc_g1[i],ACT_CHANGE_COOR,GetX(pc_g1[i]),GetY(pc_g1[i]),z);",
    "  if( pc_g2[i] )",
    "    Action(pc_g2[i],ACT_CHANGE_COOR,GetX(pc_g2[i]),GetY(pc_g2[i]),z);",
    "}",
    "CitadelTact()",
    "{",
    "  int i,k,u,c,t;",
    "  if( pc_n<0 )",
    "  {",
    "    pc_n=0;",
    "    for( u=FirstUnit(u_monster); u; u=NextUnit() )",
    "      if( GetUnitVid(u)==117 && pc_n<8 )",
    "      {",
    "        c=FindNearestSprite(GETSPRITE_VID+149,GetX(u),GetY(u),20);",
    "        if( c )",
    "        {",
    "          pc_x[pc_n]=GetX(c);",
    "          pc_y[pc_n]=GetY(c);",
    "          pc_z[pc_n]=GetZ(c);",
    "          pc_army[pc_n]=Action(u,ACT_GET_ARMY);",
    "          pc_g1[pc_n]=FindNearestSprite(GETSPRITE_VID+146,pc_x[pc_n]-34,pc_y[pc_n]+23,15);",
    "          pc_g2[pc_n]=FindNearestSprite(GETSPRITE_VID+146,pc_x[pc_n]+36,pc_y[pc_n]+25,15);",
    "          pc_mode[pc_n]=0;",
    "          pc_time[pc_n]=GetTime();",
    "          Log(\"CITADEL \"+itoa(pc_n+1)+\" at \"+itoa(pc_x[pc_n])+\",\"+itoa(pc_y[pc_n])+\" army \"+itoa(pc_army[pc_n])+\" grates \"+itoa((pc_g1[pc_n]!=0)+(pc_g2[pc_n]!=0)));",
    "          pc_n++;",
    "        }",
    "      }",
    "  }",
    "  t=GetTime();",
    "  for( i=0; i<pc_n; i++ )",
    "  {",
    "    if( pc_mode[i]>=0 && !FindNearestSprite(GETSPRITE_VID+117,pc_x[i],pc_y[i],20) )",
    "    {",
    "      PiecesGrates(i,pc_z[i]-33);",
    "      for( k=0; k<4; k++ )",
    "      {",
    "        c=FindNearestSprite(GETSPRITE_VID+142,pc_x[i],pc_y[i],90);",
    "        if( c )",
    "        {",
    "          CreateSprite(147,GetX(c),GetY(c),GetZ(c));",
    "          Destroy(c);",
    "        }",
    "      }",
    "      pc_mode[i]=-1;",
    "      Log(\"CITADEL \"+itoa(i+1)+\" tower down\");",
    "    }",
    "    if( pc_mode[i]==0 && t-pc_time[i]>500 )",
    "    {",
    "      pc_time[i]=t;",
    "      for( u=FirstUnit(u_monster); u; u=NextUnit() )",
    "        if( SizeTo(u,pc_x[i],pc_y[i])<100 && PieceFoe(u,pc_army[i]) )",
    "        {",
    "          Action(u,ACT_STOP,1);",
    "          Action(u,ACT_DAMAGE,300);",
    "          PlaySFX(45);",
    "          pc_mode[i]=1;",
    "          Log(\"CITADEL \"+itoa(i+1)+\" gate hits vid \"+itoa(GetUnitVid(u))+\" army \"+itoa(Action(u,ACT_GET_ARMY)));",
    "          break;",
    "        }",
    "    }",
    "    else if( pc_mode[i]==1 )",
    "    {",
    "      if( t-pc_time[i]>1000 )",
    "      {",
    "        pc_mode[i]=2;",
    "        pc_time[i]=t;",
    "      }",
    "      else",
    "        PiecesGrates(i,pc_z[i]-33+34*(t-pc_time[i])/1000);",
    "    }",
    "    else if( pc_mode[i]==2 )",
    "    {",
    "      if( t-pc_time[i]>1000 )",
    "      {",
    "        pc_mode[i]=3;",
    "        pc_time[i]=t;",
    "        PlaySFX(45);",
    "      }",
    "    }",
    "    else if( pc_mode[i]==3 )",
    "    {",
    "      if( t-pc_time[i]>1000 )",
    "      {",
    "        PiecesGrates(i,pc_z[i]-33);",
    "        pc_mode[i]=0;",
    "        pc_time[i]=t;",
    "      }",
    "      else",
    "        PiecesGrates(i,pc_z[i]+1-34*(t-pc_time[i])/1000);",
    "    }",
    "  }",
    "}",
    "CharybdisGuns(int i,int on)",
    "{",
    "  int u,v;",
    "  for( u=FirstUnit(u_monster); u; u=NextUnit() )",
    "  {",
    "    v=GetUnitVid(u);",
    "    if( (v==258 || v==260) && SizeTo(u,ch_x[i],ch_y[i])<90 )",
    "    {",
    "      if( on )",
    "        Action(u,ACT_SET_BEHAVE,Action(u,ACT_GET_BEHAVE)|1);",
    "      else",
    "        Action(u,ACT_SET_BEHAVE,Action(u,ACT_GET_BEHAVE)&(~1));",
    "    }",
    "  }",
    "}",
    "CharybdisTact()",
    "{",
    "  int i,u,v,b,n,found,t,prey;",
    "  if( ch_n<0 )",
    "  {",
    "    ch_n=0;",
    "    for( u=FirstUnit(u_monster); u; u=NextUnit() )",
    "      if( GetUnitVid(u)==256 && ch_n<4 )",
    "      {",
    "        b=FindNearestSprite(GETSPRITE_VID+254,GetX(u)+34,GetY(u)-22,20);",
    "        if( b )",
    "        {",
    "          ch_x[ch_n]=GetX(b);",
    "          ch_y[ch_n]=GetY(b);",
    "          ch_army[ch_n]=Action(u,ACT_GET_ARMY);",
    "          ch_teeth[ch_n]=FindNearestSprite(GETSPRITE_VID+255,ch_x[ch_n]-40,ch_y[ch_n]+27,15);",
    "          ch_peace[ch_n]=1;",
    "          ch_ate[ch_n]=0;",
    "          ch_time[ch_n]=GetTime();",
    "          ch_n++;",
    "        }",
    "      }",
    "    for( i=0; i<ch_n; i++ )",
    "    {",
    "      n=0;",
    "      for( u=FirstUnit(u_monster); u; u=NextUnit() )",
    "        if( (GetUnitVid(u)==258 || GetUnitVid(u)==260) && SizeTo(u,ch_x[i],ch_y[i])<90 )",
    "          n++;",
    "      ch_guns[i]=n;",
    "      CharybdisGuns(i,0);",
    "      Log(\"CHARYBDIS \"+itoa(i+1)+\" at \"+itoa(ch_x[i])+\",\"+itoa(ch_y[i])+\" army \"+itoa(ch_army[i])+\" guns \"+itoa(n)+\" teeth \"+itoa(ch_teeth[i]!=0));",
    "    }",
    "  }",
    "  t=GetTime();",
    "  for( i=0; i<ch_n; i++ )",
    "    if( ch_army[i]>=0 && t-ch_time[i]>500 )",
    "    {",
    "      ch_time[i]=t;",
    "      if( !FindNearestSprite(GETSPRITE_VID+256,ch_x[i]-34,ch_y[i]+22,20) )   // the eye is dead",
    "      {",
    "        CharybdisGuns(i,1);",
    "        ch_army[i]=-1;",
    "        Log(\"CHARYBDIS \"+itoa(i+1)+\" eye down\");",
    "      }",
    "      else",
    "      {",
    "        if( ch_peace[i] )",
    "        {",
    "          n=0;",
    "          for( u=FirstUnit(u_monster); u; u=NextUnit() )",
    "          {",
    "            v=GetUnitVid(u);",
    "            if( (v==258 || v==260 || v==256) && SizeTo(u,ch_x[i],ch_y[i])<90 )",
    "            {",
    "              if( v!=256 )",
    "                n++;",
    "              if( Action(u,ACT_GET_PERCENT_HP)<=80 )",
    "                ch_peace[i]=0;",
    "            }",
    "          }",
    "          if( n<ch_guns[i] )",
    "            ch_peace[i]=0;",
    "          if( !ch_peace[i] )",
    "          {",
    "            CharybdisGuns(i,1);",
    "            Log(\"CHARYBDIS \"+itoa(i+1)+\" wakes\");",
    "          }",
    "        }",
    "        found=0;",
    "        prey=0;",
    "        if( ch_army[i]==0 )",
    "          prey=1;",
    "        for( u=FirstUnit(u_monster); u; u=NextUnit() )",
    "          if( SizeTo(u,ch_x[i],ch_y[i])<90 && Action(u,ACT_GET_ARMY)!=ch_army[i] )",
    "          {",
    "            found=1;",
    "            Action(u,ACT_MOVE,ch_x[i],ch_y[i]);",
    "            if( SizeTo(u,ch_x[i],ch_y[i])<40 && Action(u,ACT_GET_ARMY)==prey )",
    "            {",
    "              Log(\"CHARYBDIS \"+itoa(i+1)+\" eats vid \"+itoa(GetUnitVid(u)));",
    "              Destroy(u);",
    "              ch_ate[i]=1;",
    "            }",
    "          }",
    "        if( found && ch_teeth[i] )",
    "        {",
    "          Action(ch_teeth[i],ACT_CLEAR_STACK,0,0);",
    "          AddCommand(ch_teeth[i],act_open,0,0);",
    "          AddCommand(ch_teeth[i],act_stand_open,0,0);",
    "          AddCommand(ch_teeth[i],act_close,0,0);",
    "          AddCommand(ch_teeth[i],act_stand,0,0);",
    "          AddCommand(ch_teeth[i],ACT_CYCLE_STACK,4,0);",
    "          PlaySFX(38);",
    "        }",
    "        if( !found && ch_ate[i] && ch_teeth[i] )",
    "        {",
    "          Action(ch_teeth[i],ACT_CLEAR_STACK,0,0);",
    "          AddCommand(ch_teeth[i],act_close,0,0);",
    "          AddCommand(ch_teeth[i],act_stand,0,0);",
    "          AddCommand(ch_teeth[i],act_salut,0,0);",
    "          ch_ate[i]=0;",
    "        }",
    "      }",
    "    }",
    "}",
    "NestTact()",
    "{",
    "  int i,u,v,n,x,y,z,f,t;",
    "  if( fn_n<0 )",
    "  {",
    "    fn_n=0;",
    "    for( u=FirstUnit(u_monster); u; u=NextUnit() )",
    "      if( GetUnitVid(u)==265 && fn_n<8 )",
    "      {",
    "        fn_x[fn_n]=GetX(u);",
    "        fn_y[fn_n]=GetY(u);",
    "        fn_army[fn_n]=Action(u,ACT_GET_ARMY);",
    "        fn_guard[fn_n]=GetDirection(u)>=64;",
    "        fn_vid[fn_n]=263-fn_guard[fn_n];",
    "        fn_left[fn_n]=0;",
    "        fn_dead[fn_n]=0;",
    "        Log(\"NEST \"+itoa(fn_n+1)+\" at \"+itoa(fn_x[fn_n])+\",\"+itoa(fn_y[fn_n])+\" army \"+itoa(fn_army[fn_n])+\" guard \"+itoa(fn_guard[fn_n]));",
    "        fn_n++;",
    "      }",
    "  }",
    "  if( !OneTimeInSecond() )",
    "    return 0;",
    "  t=GetTime();",
    "  for( i=0; i<fn_n; i++ )",
    "  {",
    "    if( !fn_dead[i] && !FindNearestSprite(GETSPRITE_VID+265,fn_x[i],fn_y[i],10) )   // destroyed",
    "    {",
    "      fn_dead[i]=1;",
    "      fn_left[i]=0;",
    "      if( fn_guard[i] )",
    "      {",
    "        fn_left[i]=6+Random(3);",
    "        fn_time[i]=t-22000;",
    "      }",
    "      Log(\"NEST \"+itoa(i+1)+\" destroyed, last flies \"+itoa(fn_left[i]));",
    "    }",
    "    if( !fn_dead[i] && fn_left[i]==0 && GetVid(fn_vid[i],VID_COUNT)==0 )",
    "    {",
    "      if( !fn_guard[i] )",
    "      {",
    "        n=0;",
    "        for( u=FirstUnit(u_monster); u; u=NextUnit() )",
    "          if( GetUnitVid(u)==30 )",
    "            n++;",
    "        fn_left[i]=n*2-Random(n);",
    "        if( fn_left[i]<2 )",
    "          fn_left[i]=2;",
    "        if( fn_left[i]>4 )",
    "          fn_left[i]=4;",
    "        fn_time[i]=t;",
    "      }",
    "      else",
    "      {",
    "        f=0;",
    "        for( u=FirstUnit(u_monster); u; u=NextUnit() )",
    "        {",
    "          v=GetUnitVid(u);",
    "          if( v!=102 && v!=104 && SizeTo(u,fn_x[i],fn_y[i])<400 && PieceFoe(u,fn_army[i]) )",
    "            f=1;",
    "        }",
    "        if( f )",
    "        {",
    "          fn_left[i]=1+Random(1);",
    "          fn_time[i]=t-22000;",
    "        }",
    "      }",
    "    }",
    "    if( fn_left[i]>0 && t-fn_time[i]>25000 )",
    "    {",
    "      x=fn_x[i]+5-Random(10);",
    "      y=fn_y[i]+5-Random(10);",
    "      z=120+5-Random(10);",
    "      if( !AskPlace(fn_vid[i],x,y,z) )",
    "      {",
    "        u=Create(fn_vid[i],x,y,z);",
    "        Action(u,act_changeaction,0,Random(255));",
    "        Action(u,act_setarmy,fn_army[i],0);",
    "        fn_left[i]--;",
    "        Log(\"NEST \"+itoa(i+1)+\" fly vid \"+itoa(fn_vid[i])+\", \"+itoa(fn_left[i])+\" to come\");",
    "      }",
    "    }",
    "  }",
    "  return 1;",
    "}",
    "PiecesTact()",
    "{",
    "  CitadelTact();",
    "  CharybdisTact();",
    "  NestTact();",
    "}",
    "ArrivalsTact()",
    "{",
    "  int i,t;",
    "  PiecesTact();",
    "  if( !OneTimeInSecond() )",
    "    return 0;",
    "  t=(GetTime()-time_game_total)/1000;",
    "  iff(1)",
    "    OrdersTact();",
    "  FactoryTact(t);",
    "  CaptureTact();",
    "  for( i=0; i<ARRIVALS; i++ )",
    "    ArrivalTry(i,t);",
    "  return 1;",
    "}",
};

const char ARRIVALS_OPEN[] = "// <editor arrivals> the trains that arrive (the Arrivals panel) - written by the map editor when the map is saved";
const char ARRIVALS_CLOSE[] = "// </editor arrivals>";
const char TACT_OPEN[] = "// <editor tact> the arrivals - written by the map editor when the map is saved";
const char TACT_CLOSE[] = "// </editor tact>";

// The authors' templates (TrainsTactDeclares.lgc, the normal difficulty), the defence trains aside
// (they have no locomotive and do not drive in).
const std::vector<TrainTemplate>& templates() {
    static const std::vector<TrainTemplate> list = {
        {0, "TplLightGun", "LIGHT GUN", {5, 20, 20}},
        {1, "TplMediumGun", "MEDIUM GUN", {5, 20, 20, 20}},
        {2, "TplHardGun", "HEAVY GUN", {5, 20, 20, 85, 20}},
        {3, "TplVeryHardGun", "VERY HEAVY GUN", {10, 10, 20, 85, 20, 20, 20, 45, 20}},
        {10, "TplLightMortar", "LIGHT MORTAR", {5, 25, 25}},
        {11, "TplMediumMortar", "MEDIUM MORTAR", {10, 25, 25, 25}},
        {12, "TplHardMortar", "HEAVY MORTAR", {10, 25, 25, 85, 25}},
        {13, "TplVeryHardMortar", "VERY HEAVY MORTAR", {10, 10, 25, 45, 25, 25, 85, 25}},
        {20, "TplLightRocket", "LIGHT ROCKETS", {5, 80, 80}},
        {21, "TplMediumRocket", "MEDIUM ROCKETS", {10, 80, 80, 80}},
        {22, "TplHardRocket", "HEAVY ROCKETS", {10, 80, 80, 85, 80}},
        {23, "TplVeryHardRocket", "VERY HEAVY ROCKETS", {10, 10, 45, 80, 80, 80, 85, 80}},
        {30, "TplLightAir", "LIGHT AIRSHIPS", {5, 35, 35}},
        {31, "TplMediumAir", "MEDIUM AIRSHIPS", {10, 35, 35, 35}},
        {32, "TplHardAir", "HEAVY AIRSHIPS", {10, 35, 35, 85, 35}},
        {33, "TplVeryHardAir", "VERY HEAVY AIRSHIPS", {10, 10, 35, 35, 35, 85, 35}},
        {50, "TplLightFire", "LIGHT FLAME", {5, 90, 90}},
        {51, "TplMediumFire", "MEDIUM FLAME", {10, 90, 90, 90}},
        {52, "TplHardFire", "HEAVY FLAME", {10, 90, 90, 85, 90}},
        {53, "TplVeryHardFire", "VERY HEAVY FLAME", {10, 10, 90, 90, 90, 85, 90}},
        {60, "TplLightDesant", "LIGHT LANDING", {5, 82, 82}},
        {61, "TplMediumDesant", "MEDIUM LANDING", {10, 82, 82, 82}},
        {62, "TplHardDesant", "HEAVY LANDING", {10, 82, 82, 85, 82}},
        {63, "TplVeryHardDesant", "VERY HEAVY LANDING", {10, 10, 82, 82, 82, 85, 82}},
        {46, "TplBrander", "BRANDER", {97}},
        {70, "TplSuperRocket", "BIG ROCKET", {5, 45, 62}},
        {71, "TplSuperMortar", "BIG MORTAR", {5, 75, 45}},
    };
    return list;
}

std::vector<int> declared_cars(int type);   // below

// The cars of a train type the block has no this_trainNN for: a template's (x5: the normal
// difficulty's, a locomotive and two of its car), else a locomotive.
std::vector<int> template_cars(int type, const Parsed& p) {
    auto own = p.arrays.find("this_train" + std::to_string(type));
    std::vector<int> cars;
    if (own != p.arrays.end()) {
        for (const std::string& v : own->second) {
            if (to_int(v) <= 0) break;
            cars.push_back(to_int(v));
        }
        if (!cars.empty() && type % 10 != 5) return cars;
    }
    for (const TrainTemplate& t : templates())
        if (t.type == type) return t.cars;
    if (type % 10 == 5 && !cars.empty()) return {5, cars[0], cars[0]};
    if (!declared_cars(type).empty()) return declared_cars(type);
    return {5};
}

// The ATT_ name of a train type (ATT_<size>_<kind> and the special ones), else its number.
std::string type_token_of(int type) {
    static const char* const ONES[] = {"LIGHT", "MEDIUM", "HARD", "VERY_HARD", nullptr, "DIFF", nullptr, nullptr, nullptr, "DEFENCE"};
    static const char* const TENS[] = {"GUN", "MORTAIR", "ROCKET", "AIR", nullptr, "FIRE", "DESANT", nullptr, nullptr, nullptr};
    if (type == 46) return "ATT_KAMIKAZE_UNIT";
    if (type == 70 || type == 71) return "ATT_HUYACHILO" + std::to_string(type - 70);
    if (type >= 0 && type < 100 && ONES[type % 10] && TENS[type / 10])
        return std::string("ATT_") + ONES[type % 10] + "_" + TENS[type / 10];
    return std::to_string(type);
}

// A type of the script: an ATT_ name or a number; -1 when it is neither.
int type_number_of(const std::string& token) {
    const std::string t = trim(token);
    for (int n = 0; n < 100; ++n)
        if (type_token_of(n) == t) return n;
    return to_int(t, -1);
}

// The cars of TrainsTactDeclares.lgc's types the panel does not offer: the defence trains (no
// locomotive) and the difficulty's (x5: a locomotive and one, two or three of its car - the
// normal difficulty's two here).
std::vector<int> declared_cars(int type) {
    switch (type) {
    case 9: return {20, 20, 85, 20, 45, 20, 20};
    case 19: return {25, 25, 25, 45, 25};
    case 29: return {80, 80, 45, 80};
    case 39: return {35, 35, 35, 85, 35};
    case 59: return {90, 90, 45, 90, 90};
    case 69: return {82, 82, 45, 82, 82};
    case 5: return {5, 20, 20};
    case 15: return {5, 25, 25};
    case 25: return {5, 80, 80};
    case 35: return {5, 35, 35};
    case 55: return {5, 90, 90};
    case 65: return {5, 82, 82};
    default: return {};
    }
}

// The tens of a train's type: the kind the AI treats it as (its cars' weapons, most of them).
int kind_of(const std::vector<int>& cars) {
    std::map<int, int> count;
    for (int c : cars) {
        const int kind = c == 25 ? 1 : c == 80 ? 2 : c == 35 ? 3 : c == 90 ? 5 : c == 82 ? 6 : c == 75 || c == 62 ? 7 : c == 20 ? 0 : -1;
        if (kind >= 0) ++count[kind];
    }
    int best = 0, most = 0;
    for (const auto& k : count)
        if (k.second > most) {
            most = k.second;
            best = k.first;
        }
    return best;
}

// The task of the script's arrays: ATA_FIGHT, ATA_FIGHT_FORT, ATA_STANDn, ATA_PATROLn or a number.
int task_number(const std::string& token) {
    const std::string t = trim(token);
    if (t == "ATA_FIGHT") return 10;
    if (t == "ATA_FIGHT_FORT") return 11;
    if (t == "ATA_NONE") return -1;
    if (starts_with(t, "ATA_STAND") && t.size() == 10 && std::isdigit(static_cast<unsigned char>(t[9]))) return t[9] - '0';
    if (starts_with(t, "ATA_PATROL") && t.size() == 11 && std::isdigit(static_cast<unsigned char>(t[10]))) return 20 + t[10] - '0';
    return to_int(t, -1);
}

// The enemy's queue (level_queue_*), the units the factories get later and the unit limits.
void read_factories(const Parsed& p, FactoryState& f) {
    auto array = [&](const char* name) {
        auto it = p.arrays.find(name);
        return it == p.arrays.end() ? std::vector<std::string>() : it->second;
    };
    auto define_int = [&](const std::string& name) {
        auto it = p.defines.find(name);
        return it == p.defines.end() ? 0 : to_int(it->second);
    };
    const auto times = array("level_queue_times"), trains = array("level_queue_trains"), tasks = array("level_queue_task");
    for (size_t i = 0; i < times.size() && i < trains.size() && to_int(times[i]) > 0 && to_int(trains[i], 0) != -1; ++i) {
        FactoryTrain t;
        t.time = to_int(times[i]);
        t.type = type_number_of(trains[i]);
        t.cars = t.type_cars = template_cars(t.type, p);
        const int task = i < tasks.size() ? task_number(tasks[i]) : 10;
        if (task >= 0 && task < POSTS) {
            t.task = TASK_GUARD;
            t.tx = define_int("ATA_STAND" + std::to_string(task) + "_X");
            t.ty = define_int("ATA_STAND" + std::to_string(task) + "_Y");
        } else if (task >= 20 && task < 20 + PATROLS) {
            t.task = TASK_PATROL;
            const std::string k = "ATA_PATROL" + std::to_string(task - 20) + "_";
            t.tx = define_int(k + "X1");
            t.ty = define_int(k + "Y1");
            t.tx2 = define_int(k + "X2");
            t.ty2 = define_int(k + "Y2");
        }
        f.queue.push_back(t);
    }
    const auto ux = array("unlock_x"), uy = array("unlock_y"), uv = array("unlock_vid"), ut = array("unlock_time");
    for (int i = 0; i < define_int("UNLOCKS") && i < static_cast<int>((std::min)({ux.size(), uy.size(), uv.size(), ut.size()})); ++i)
        f.unlocks.push_back({to_int(ux[i]), to_int(uy[i]), to_int(uv[i]), to_int(ut[i]), nullptr});
    const auto la = array("limit_army"), lv = array("limit_vid"), lm = array("limit_max");
    for (int i = 0; i < define_int("UNIT_LIMITS") && i < static_cast<int>((std::min)({la.size(), lv.size(), lm.size()})); ++i)
        f.limits.push_back({to_int(la[i]), to_int(lv[i]), to_int(lm[i])});
}

// The trains' orders of the arrays order_*[] (written by settings_block), beside the patrols' lines.
void read_orders(const Parsed& p, std::vector<TrainPatrol>& out) {
    auto define = p.defines.find("ORDERS");
    const int n = define == p.defines.end() ? 0 : to_int(define->second);
    auto column = [&](const char* name, int i) -> std::string {
        auto it = p.arrays.find(name);
        return it == p.arrays.end() || i >= static_cast<int>(it->second.size()) ? "0" : it->second[i];
    };
    auto define_int = [&](const std::string& name) {
        auto it = p.defines.find(name);
        return it == p.defines.end() ? 0 : to_int(it->second);
    };
    for (int i = 0; i < n && i < 256; ++i) {
        TrainPatrol t{to_int(column("order_vid", i)), to_int(column("order_x", i)), to_int(column("order_y", i)),
                      to_int(column("order_to_x", i)), to_int(column("order_to_y", i))};
        t.to_z = to_int(column("order_to_z", i));
        const int task = task_number(column("order_task", i));
        t.army = task >= 0 ? 1 : 0;
        if (task == -2) t.task = TASK_DRIVE;
        else if (task == -3) t.task = TASK_PATROL;
        else if (task == 10 || task == 11) t.task = TASK_ATTACK;
        else if (task >= 0 && task < POSTS) {
            t.task = TASK_GUARD;
            t.to_x = define_int("ATA_STAND" + std::to_string(task) + "_X");
            t.to_y = define_int("ATA_STAND" + std::to_string(task) + "_Y");
        } else if (task >= 20 && task < 20 + PATROLS) {
            t.task = TASK_PATROL;
            t.to_x = define_int("ATA_PATROL" + std::to_string(task - 20) + "_X2");
            t.to_y = define_int("ATA_PATROL" + std::to_string(task - 20) + "_Y2");
        } else {
            continue;
        }
        out.push_back(t);
    }
}

void read_arrivals(const Parsed& p, ArrivalsState& s) {
    s.list.clear();
    auto define = p.defines.find("ARRIVALS");
    const int n = define == p.defines.end() ? 0 : to_int(define->second);
    auto column = [&](const char* name, int i) -> std::string {
        auto it = p.arrays.find(name);
        return it == p.arrays.end() || i >= static_cast<int>(it->second.size()) ? "0" : it->second[i];
    };
    auto value = [&](const char* name, int i) { return to_int(column(name, i)); };
    auto define_int = [&](const std::string& name) {
        auto it = p.defines.find(name);
        return it == p.defines.end() ? 0 : to_int(it->second);
    };
    for (int i = 0; i < n && i < 64; ++i) {
        Arrival a;
        a.army = (std::max)(0, (std::min)(3, value("arrival_army", i)));
        a.cars = template_cars(type_number_of(column("arrival_type", i)), p);
        a.x = value("arrival_x", i);
        a.y = value("arrival_y", i);
        a.z = value("arrival_z", i);
        a.entry = true;
        a.first = value("arrival_first", i);
        a.every = value("arrival_every", i);
        a.times = value("arrival_times", i);
        a.cond = (std::max)(0, (std::min)(IF_COUNT - 1, value("arrival_if", i)));
        a.cond_n = value("arrival_if_n", i);
        a.earlier = value("arrival_or", i) != 0;
        a.raid = value("arrival_raid", i);
        a.message = value("arrival_msg", i) != 0;
        a.tx = value("arrival_to_x", i);
        a.ty = value("arrival_to_y", i);
        a.tz = value("arrival_to_z", i);
        const int task = task_number(column("arrival_task", i));
        if (task == -2) a.task = TASK_DRIVE;
        else if (a.army != 1 || task == -1) a.task = TASK_NONE;
        else if (task == 10) a.task = TASK_ATTACK;
        else if (task == 11) a.task = TASK_FORTS;
        else if (task >= 0 && task < POSTS) {
            a.task = TASK_GUARD;
            a.tx = define_int("ATA_STAND" + std::to_string(task) + "_X");
            a.ty = define_int("ATA_STAND" + std::to_string(task) + "_Y");
        } else if (task >= 20 && task < 20 + PATROLS) {
            a.task = TASK_PATROL;
            const std::string k = "ATA_PATROL" + std::to_string(task - 20) + "_";
            a.tx = define_int(k + "X1");
            a.ty = define_int(k + "Y1");
            a.tx2 = define_int(k + "X2");
            a.ty2 = define_int(k + "Y2");
        } else {
            a.task = TASK_ATTACK;
        }
        s.list.push_back(a);
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
    m.rain = (std::max)(0, (std::min)(2, to_int(get("RAIN", "0"))));
    m.weather = (std::max)(0, (std::min)(4, to_int(get("RAIN_TYPE", "0"))));
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
    read_factories(p, m.factories);
    if (block)   // the block's own other defines stay; outside a block only the known settings count
        for (const std::string& name : p.define_order) {
            bool known = std::find(std::begin(OWN), std::end(OWN), name) != std::end(OWN) || starts_with(name, "ATA_PATROL") ||
                         starts_with(name, "ATA_STAND");
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
        if (name.empty()) continue;
        if (name.find("hiscores") != std::string::npos) {   // the high scores page: the game writes it
            m.scores_page = true;
            continue;
        }
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
std::vector<std::string> settings_block(const Mission& m, const ArrivalsState& arrivals, const std::string& map) {
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
    define("RAIN", std::to_string(m.rain));
    if (m.weather) define("RAIN_TYPE", std::to_string(m.weather));   // rain is daynight.lgc's default
    define("NUKE", m.nuke ? "1" : "0");
    for (const auto& s : SETTINGS)
        if (std::strcmp(s[0], "LEVEL") != 0) define(s[0], m.settings.count(s[0]) ? m.settings.at(s[0]) : s[1]);
    define("OBJECTIVES_FILE", quoted_path(folder + "\\objectives.txt"));
    define("WIN_FILE", quoted_path(folder + "\\win.txt"));
    define("LOOSE_FILE", quoted_path(folder + "\\loose.txt"));
    define("BRIEF_LIST", quoted_path(folder + "\\brief.lst"));
    define("BRIEF_HEADER", "\"" + ascii_upper(m.header.empty() ? m.title : m.header, 32) + "\"");
    define("HISCORES_FILE", quoted_path(folder + "\\hiscores"));
    define("PLAYER_REINFORCEMENT", std::to_string(arrivals.on ? arrivals.waves : m.reinforcements));
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
    // the trains' types: a template's as it is, else a type of the map's own (int this_trainNN[])
    std::map<std::vector<int>, int> own;
    std::set<int> used;
    auto type_of = [&](const std::vector<int>& cars) -> std::string {
        for (const TrainTemplate& t : templates())
            if (t.cars == cars) return type_token_of(t.type);
        if (own.count(cars)) return std::to_string(own[cars]);
        int type = -1;
        const int tens = kind_of(cars);
        for (int k = 0; k < 10 && type < 0; ++k)   // its kind first, then the others'
            for (int ones : {4, 6, 7, 8}) {
                const int t = ((tens + k) % 10) * 10 + ones;
                if ((tens + k) % 10 == 4 || used.count(t)) continue;   // 4x: the AI's helpers
                type = t;
                break;
            }
        if (type < 0) type = 0;
        used.insert(type);
        own[cars] = type;
        std::vector<std::string> list;
        for (int c : cars) list.push_back(std::to_string(c));
        list.push_back("0");
        array("this_train" + std::to_string(type), list);
        return std::to_string(type);
    };
    // the enemy's posts, numbered over the queue and the arrivals
    int posts = 0;
    auto post = [&](int x, int y) -> std::string {
        if (posts >= POSTS) return "ATA_FIGHT";
        const std::string n = "ATA_STAND" + std::to_string(posts++);
        define(n + "_X", std::to_string(x));
        define(n + "_Y", std::to_string(y));
        return n;
    };
    auto patrol = [&](int x1, int y1, int x2, int y2) {
        EnemyTrain t;
        t.patrol = true;
        t.x1 = x1, t.y1 = y1, t.x2 = x2, t.y2 = y2;
        return task(t);
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
    for (const FactoryTrain& t : m.factories.queue) {
        times.push_back(std::to_string(t.time));
        // an authors' type as it was read while its cars are the ones it gives (a difficulty's train);
        // the map's own types are made anew from the cars
        const bool authors = t.type >= 0 && t.cars == t.type_cars && starts_with(type_token_of(t.type), "ATT_");
        queue_types.push_back(authors ? type_token_of(t.type) : type_of(t.cars));
        queue_tasks.push_back(t.task == TASK_PATROL ? patrol(t.tx, t.ty, t.tx2, t.ty2) : t.task == TASK_GUARD ? post(t.tx, t.ty) : "ATA_FIGHT");
    }
    // the arrivals' trains, posts and patrols; the trains' orders
    std::vector<std::string> a_type, a_task;
    struct Ordered {
        TrainPatrol order;
        std::string task, type;
    };
    std::vector<Ordered> orders;
    if (arrivals.on) {
        define("ARRIVALS", std::to_string(arrivals.list.size()));
        for (const Arrival& a : arrivals.list) {
            a_type.push_back(type_of(a.cars));
            if (a.army != 1) a_task.push_back(a.task == TASK_DRIVE && (a.tx || a.ty) ? "-2" : "-1");   // no point: no order
            else if (a.task == TASK_GUARD) a_task.push_back(post(a.tx, a.ty));
            else if (a.task == TASK_PATROL) a_task.push_back(patrol(a.tx, a.ty, a.tx2, a.ty2));
            else a_task.push_back(a.task == TASK_FORTS ? "ATA_FIGHT_FORT" : "ATA_FIGHT");
        }
        // the trains' orders (OrdersTact of the runner)
        for (const TrainPatrol& o : edit_train_patrols()) {
            std::string task;
            if (o.army == 1 && o.task == TASK_ATTACK) task = "ATA_FIGHT";
            else if (o.army == 1 && o.task == TASK_GUARD) task = post(o.to_x, o.to_y);
            else if (o.army == 1 && o.task == TASK_PATROL) task = patrol(o.from_x, o.from_y, o.to_x, o.to_y);
            else if (o.army != 1 && o.task == TASK_DRIVE) task = "-2";
            else if (o.army != 1 && o.task == TASK_PATROL) task = "-3";
            else continue;   // an order of another army (the train's army changed since)
            orders.push_back({o, task, o.army == 1 ? type_of(o.cars) : "0"});
        }
        define("ORDERS", std::to_string(orders.size()));
        // the factories: units that come later, unit limits (FactoryTact of the runner)
        const FactoryState& f = m.factories;
        define("UNLOCKS", std::to_string(f.unlocks.size()));
        define("UNIT_LIMITS", std::to_string(f.limits.size()));
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
    if (arrivals.on) {
        auto column = [&](const char* name, const std::function<std::string(size_t)>& of) {
            std::vector<std::string> items;
            for (size_t i = 0; i < arrivals.list.size(); ++i) items.push_back(of(i));
            items.push_back("0");
            array(name, items);
        };
        const std::vector<Arrival>& l = arrivals.list;
        auto number = [&](int Arrival::*field) { return [&, field](size_t i) { return std::to_string(l[i].*field); }; };
        column("arrival_army", number(&Arrival::army));
        column("arrival_type", [&](size_t i) { return a_type[i]; });
        column("arrival_x", number(&Arrival::x));
        column("arrival_y", number(&Arrival::y));
        column("arrival_z", number(&Arrival::z));
        column("arrival_first", number(&Arrival::first));
        column("arrival_every", number(&Arrival::every));
        column("arrival_times", number(&Arrival::times));
        column("arrival_if", number(&Arrival::cond));
        column("arrival_if_n", number(&Arrival::cond_n));
        column("arrival_or", [&](size_t i) { return std::string(l[i].earlier ? "1" : "0"); });
        column("arrival_task", [&](size_t i) { return a_task[i]; });
        // the point the task drives to (the enemy's posts and patrols are the AI's, above)
        auto drive = [&](size_t i) { return l[i].army != 1 && l[i].task == TASK_DRIVE; };
        column("arrival_to_x", [&](size_t i) { return std::to_string(drive(i) ? l[i].tx : 0); });
        column("arrival_to_y", [&](size_t i) { return std::to_string(drive(i) ? l[i].ty : 0); });
        column("arrival_to_z", [&](size_t i) { return std::to_string(drive(i) ? l[i].tz : 0); });
        column("arrival_raid", [&](size_t i) { return std::to_string(l[i].army == 1 ? l[i].raid : 0); });
        column("arrival_msg", [&](size_t i) { return std::string(l[i].message && l[i].army < 2 ? "1" : "0"); });
        column("arrival_done", [](size_t) { return std::string("0"); });
        column("arrival_last", [](size_t) { return std::string("0"); });
        auto texts = [&](const char* name, const std::function<std::string(const Ordered&)>& of) {
            std::vector<std::string> items;
            for (const Ordered& o : orders) items.push_back(of(o));
            items.push_back("0");
            array(name, items);
        };
        texts("order_vid", [](const Ordered& o) { return std::to_string(o.order.nvid); });
        texts("order_x", [](const Ordered& o) { return std::to_string(o.order.x); });
        texts("order_y", [](const Ordered& o) { return std::to_string(o.order.y); });
        texts("order_task", [](const Ordered& o) { return o.task; });
        texts("order_type", [](const Ordered& o) { return o.type; });
        texts("order_to_x", [](const Ordered& o) { return std::to_string(o.order.to_x); });
        texts("order_to_y", [](const Ordered& o) { return std::to_string(o.order.to_y); });
        texts("order_to_z", [](const Ordered& o) { return std::to_string(o.order.to_z); });
        const FactoryState& f = m.factories;
        auto columns = [&](const char* name, size_t n, const std::function<int(size_t)>& of) {
            std::vector<std::string> items;
            for (size_t i = 0; i < n; ++i) items.push_back(std::to_string(of(i)));
            items.push_back("0");
            array(name, items);
        };
        columns("unlock_x", f.unlocks.size(), [&](size_t i) { return f.unlocks[i].x; });
        columns("unlock_y", f.unlocks.size(), [&](size_t i) { return f.unlocks[i].y; });
        columns("unlock_vid", f.unlocks.size(), [&](size_t i) { return f.unlocks[i].nvid; });
        columns("unlock_time", f.unlocks.size(), [&](size_t i) { return f.unlocks[i].time; });
        columns("limit_army", f.limits.size(), [&](size_t i) { return f.limits[i].army; });
        columns("limit_vid", f.limits.size(), [&](size_t i) { return f.limits[i].nvid; });
        columns("limit_max", f.limits.size(), [&](size_t i) { return f.limits[i].max; });
    }
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
        if (g_arrivals.on || p.task != TASK_PATROL) continue;   // the runner's arrays have them
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
    IDC_QUEUE, IDC_OBJECTIVES, IDC_WIN, IDC_LOSE, IDC_BRIEF, IDC_ARRIVALS, IDC_WEATHER
};
bool g_open_arrivals = false;   // the dialog ended with its Arrivals... button


std::wstring widen(const std::string& s) { return from_pack(s); }

std::wstring train_lines(const std::vector<EnemyTrain>& trains) {
    std::wstring out;
    for (const EnemyTrain& t : trains) {
        char line[128];
        const std::string type = type_name(t.type);
        if (t.patrol) std::snprintf(line, sizeof line, "%s %d %d %d %d", type.c_str(), t.x1, t.y1, t.x2, t.y2);
        else std::snprintf(line, sizeof line, "%s %d %d", type.c_str(), t.x1, t.y1);
        out += (out.empty() ? L"" : L"\r\n") + widen(line);
    }
    return out;
}

std::vector<std::string> words_of(const std::string& line) {
    std::vector<std::string> words;
    for (size_t a = 0; a < line.size();) {
        const size_t b = line.find_first_of(" \t,", a);
        const std::string w = line.substr(a, b == std::string::npos ? std::string::npos : b - a);
        if (!w.empty()) words.push_back(w);
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return words;
}

// The trains at the start of an edit box (type x y, or type x1 y1 x2 y2); false with the line in
// `error` when one cannot be read.
bool parse_trains(const std::wstring& text, std::vector<EnemyTrain>& out, std::string& error) {
    out.clear();
    int number = 0;
    for (const std::string& raw : split_lines(to_pack(text))) {
        ++number;
        const std::vector<std::string> words = words_of(trim(raw));
        if (words.empty()) continue;
        EnemyTrain t;
        if (!type_token(words[0], t.type)) {
            error = ui_printf(ui_format("ErrLineType", "line %d: an unknown train type"), number);
            return false;
        }
        std::vector<int> v;
        for (size_t i = 1; i < words.size(); ++i) {
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
        } else if (v.size() == 2) {
            t.x1 = v[0];
            t.y1 = v[1];
        } else {
            error = ui_printf(ui_format("ErrLineStart", "line %d: type x y, or type x1 y1 x2 y2"), number);
            return false;
        }
        out.push_back(t);
    }
    return true;
}

// The enemy's queue in the dialog: a line a train, "seconds train" with the train a type's name
// (light_gun ...) or its cars (5+20+20), then "x1 y1 x2 y2" for a patrol or "guard x y" for a post.
std::string cars_text(const std::vector<int>& cars) {
    std::string out;
    for (int c : cars) out += (out.empty() ? "" : "+") + std::to_string(c);
    return out;
}

std::string queue_train_name(const FactoryTrain& t) {   // as the dialog's line names it: a type or its cars
    std::string train = cars_text(t.cars);
    const std::string token = t.type >= 0 && t.cars == t.type_cars ? type_token_of(t.type) : "";
    if (starts_with(token, "ATT_")) train = type_name(token);
    for (const TrainTemplate& m : templates())
        if (token.empty() && m.cars == t.cars) train = type_name(type_token_of(m.type));
    return train;
}

std::wstring queue_lines(const std::vector<FactoryTrain>& queue) {
    std::wstring out;
    for (const FactoryTrain& t : queue) {
        const std::string train = queue_train_name(t);
        char line[160];
        if (t.task == TASK_PATROL) std::snprintf(line, sizeof line, "%d %s %d %d %d %d", t.time, train.c_str(), t.tx, t.ty, t.tx2, t.ty2);
        else if (t.task == TASK_GUARD) std::snprintf(line, sizeof line, "%d %s guard %d %d", t.time, train.c_str(), t.tx, t.ty);
        else std::snprintf(line, sizeof line, "%d %s", t.time, train.c_str());
        out += (out.empty() ? L"" : L"\r\n") + widen(line);
    }
    return out;
}

bool parse_queue(const std::wstring& text, const std::vector<FactoryTrain>& was, std::vector<FactoryTrain>& out, std::string& error) {
    out.clear();
    int number = 0;
    for (const std::string& raw : split_lines(to_pack(text))) {
        ++number;
        const std::vector<std::string> words = words_of(trim(raw));
        if (words.empty()) continue;
        FactoryTrain t;
        t.time = to_int(words[0], -1);
        if (t.time <= 0) {
            error = ui_printf(ui_format("ErrLineSeconds", "line %d: the time in seconds first"), number);
            return false;
        }
        std::string token;
        if (words.size() > 1 && words[1].find('+') != std::string::npos) {   // its cars
            t.cars.clear();
            for (size_t a = 0; a <= words[1].size();) {
                const size_t b = words[1].find('+', a);
                const int v = to_int(words[1].substr(a, b == std::string::npos ? std::string::npos : b - a), -1);
                if (v <= 0) {
                    error = ui_printf(ui_format("ErrLineType", "line %d: an unknown train type"), number);
                    return false;
                }
                t.cars.push_back(v);
                if (b == std::string::npos) break;
                a = b + 1;
            }
        } else if (words.size() > 1 && type_token(words[1], token)) {
            t.type = type_number_of(token);
            Parsed none;
            t.cars = t.type_cars = template_cars(t.type, none);
            for (const FactoryTrain& w : was)   // a type of the map's own keeps its cars
                if (w.type == t.type && !w.type_cars.empty()) t.cars = t.type_cars = w.type_cars;
        } else {
            error = ui_printf(ui_format("ErrLineType", "line %d: an unknown train type"), number);
            return false;
        }
        size_t i = 2;
        if (i < words.size() && lstrcmpiA(words[i].c_str(), "guard") == 0) {
            t.task = TASK_GUARD;
            ++i;
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
        if (t.task == TASK_GUARD && v.size() == 2) {
            t.tx = v[0];
            t.ty = v[1];
        } else if (t.task != TASK_GUARD && v.size() == 4) {
            t.task = TASK_PATROL;
            t.tx = v[0];
            t.ty = v[1];
            t.tx2 = v[2];
            t.ty2 = v[3];
        } else if (!(t.task != TASK_GUARD && v.empty())) {
            error = ui_printf(ui_format("ErrLineQueue2", "line %d: seconds train, seconds train x1 y1 x2 y2 or seconds train guard x y"),
                              number);
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
    int rain = 0, weather = 0;
    if (number(IDC_RAIN, rain)) n.rain = (std::max)(0, (std::min)(2, rain));
    if (f.count(IDC_WEATHER) && number(IDC_WEATHER, weather)) n.weather = (std::max)(0, (std::min)(4, weather));
    n.nuke = get(IDC_NUKE) == L"1";
    if (!number(IDC_LIMIT, n.limit) && error.empty()) error = ui_text("ErrAttackers", "Enemy attackers at once: a number");
    if (f.count(IDC_WAVES)) {   // a map whose arrivals the editor runs has the Arrivals panel instead
        if (!number(IDC_WAVES, n.reinforcements) && error.empty()) error = ui_text("ErrWaves", "Reinforcement waves: a number");
        n.train = to_pack(get(IDC_TRAIN));
    }
    if (error.empty() && !parse_trains(get(IDC_START), n.start, trains))
        error = ui_printf(ui_format("ErrStartTrains", "Enemy trains at the start, %s"), trains.c_str());
    const std::vector<FactoryTrain> was = n.factories.queue;
    if (error.empty() && !parse_queue(get(IDC_QUEUE), was, n.factories.queue, trains))
        error = ui_printf(ui_format("ErrQueue", "Enemy trains from its factory, %s"), trains.c_str());
    int patrols = 0;
    for (const EnemyTrain& t : n.start) patrols += t.patrol;
    for (const FactoryTrain& t : n.factories.queue) patrols += t.task == TASK_PATROL;
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
        for (const std::wstring& t : {ui_wtext("WeatherRain", "rain"), ui_wtext("WeatherSnow", "snow"),
                                      ui_wtext("WeatherClouds", "clouds"), ui_wtext("WeatherStorm", "storm clouds"),
                                      ui_wtext("WeatherMeteorites", "meteorites")})
            SendDlgItemMessageW(dialog, IDC_WEATHER, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
        SendDlgItemMessageW(dialog, IDC_WEATHER, CB_SETCURSEL, s.weather, 0);
        for (const std::wstring& t : {ui_wtext("RainNever", "never"), ui_wtext("RainSometimes", "now and then"),
                                      ui_wtext("RainAlways", "all the time")})
            SendDlgItemMessageW(dialog, IDC_RAIN, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(t.c_str()));
        SendDlgItemMessageW(dialog, IDC_RAIN, CB_SETCURSEL, s.rain, 0);
        CheckDlgButton(dialog, IDC_NUKE, s.nuke ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemInt(dialog, IDC_LIMIT, s.limit, TRUE);
        SetDlgItemInt(dialog, IDC_WAVES, s.reinforcements, TRUE);
        SetDlgItemTextW(dialog, IDC_TRAIN, widen(s.train).c_str());
        SetDlgItemTextW(dialog, IDC_START, train_lines(s.start).c_str());
        SetDlgItemTextW(dialog, IDC_QUEUE, queue_lines(s.factories.queue).c_str());
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
    if (LOWORD(w) != IDOK && LOWORD(w) != IDC_ARRIVALS) return FALSE;
    Fields fields;
    for (int id = IDC_TITLE; id <= IDC_BRIEF; ++id)
        if (GetDlgItem(dialog, id)) fields[id] = item_text(dialog, id);
    fields[IDC_TIME] = std::to_wstring(SendDlgItemMessageW(dialog, IDC_TIME, CB_GETCURSEL, 0, 0) + 1);
    fields[IDC_RAIN] = std::to_wstring(SendDlgItemMessageW(dialog, IDC_RAIN, CB_GETCURSEL, 0, 0));
    fields[IDC_WEATHER] = std::to_wstring(SendDlgItemMessageW(dialog, IDC_WEATHER, CB_GETCURSEL, 0, 0));
    fields[IDC_NUKE] = IsDlgButtonChecked(dialog, IDC_NUKE) == BST_CHECKED ? L"1" : L"0";
    Mission n = s;
    const std::string error = apply_fields(fields, n);
    if (!error.empty()) {
        MessageBoxW(dialog, ui_wide(error).c_str(), ui_wtext("MissionTitle", "Mission settings").c_str(), MB_OK | MB_ICONWARNING);
        return TRUE;
    }
    s = n;
    g_open_arrivals = LOWORD(w) == IDC_ARRIVALS;   // the settings as OK keeps them, then the panel
    EndDialog(dialog, 1);
    return TRUE;
}
// The boxes as the dialog starts with them.
Fields current_fields() {
    const Mission& s = g_mission;
    Fields f = {{IDC_TITLE, s.title},
            {IDC_HEADER, s.header},
            {IDC_MONEY, std::to_wstring(s.money)},
            {IDC_TIME, std::to_wstring(s.daytime)},
            {IDC_RAIN, std::to_wstring(s.rain)},
            {IDC_WEATHER, std::to_wstring(s.weather)},
            {IDC_NUKE, s.nuke ? L"1" : L"0"},
            {IDC_LIMIT, std::to_wstring(s.limit)},
            {IDC_WAVES, std::to_wstring(s.reinforcements)},
            {IDC_TRAIN, widen(s.train)},
            {IDC_START, train_lines(s.start)},
            {IDC_QUEUE, queue_lines(s.factories.queue)},
            {IDC_OBJECTIVES, s.objectives},
            {IDC_WIN, s.win},
            {IDC_LOSE, s.lose},
            {IDC_BRIEF, brief_text(s.brief)}};
    if (g_arrivals.on) {   // as the dialog has them
        f.erase(IDC_WAVES);
        f.erase(IDC_TRAIN);
    }
    return f;
}

}  // namespace
std::vector<PushLine>& mission_push_lines() { return g_lines; }
void mission_list_map(const std::string& folder, const std::string& title) { add_to_custom_list(folder, title); }
std::string mission_listed_title(const std::string& folder) { return custom_title(folder); }
const std::vector<TrainPatrol>& mission_patrols() { return g_patrols; }
const std::vector<PushLine>& mission_script_push_lines() { return g_script_lines; }
bool mission_managed() { return g_managed; }
std::vector<EnemyTrainMark> mission_enemy_trains() {
    std::vector<EnemyTrainMark> out;
    for (const EnemyTrain& t : g_mission.start) out.push_back({type_name(t.type), t.time, t.patrol, t.x1, t.y1, t.x2, t.y2});
    for (const FactoryTrain& t : g_mission.factories.queue)   // a guard is marked at its factory, like an attack
        out.push_back({queue_train_name(t), t.time, t.task == TASK_PATROL, t.tx, t.ty, t.tx2, t.ty2});
    return out;
}

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
    read_orders(p, g_patrols);
    g_arrivals = ArrivalsState();
    g_arrivals.on = g_managed && b.arrivals();
    g_arrivals.waves = g_mission.reinforcements;
    if (g_arrivals.on) read_arrivals(p, g_arrivals);
    g_arrivals_refusal = !g_managed ? 1 : !g_arrivals.on && (b.trains_tact < 0 || b.header < 0) ? 2 : 0;
    patch::log("mission: %s - %s, %u push lines of the editor, %u of the script, %u start trains, %u in the queue, %s",
               script_of(map).c_str(), g_managed ? "an editor mission" : "no editor blocks",
               static_cast<unsigned>(g_lines.size()), static_cast<unsigned>(g_script_lines.size()),
               static_cast<unsigned>(g_mission.start.size()), static_cast<unsigned>(g_mission.factories.queue.size()),
               g_arrivals.on ? ui_printf("%u arrivals", static_cast<unsigned>(g_arrivals.list.size())).c_str()
                             : "the authors' reinforcement");
}

ArrivalsState& mission_arrivals() { return g_arrivals; }
FactoryState& mission_factories() { return g_mission.factories; }
int mission_reinforcement_waves() { return g_mission.reinforcements; }
std::vector<int> mission_reinforcement_train() {
    std::vector<int> cars;
    for (size_t a = 0; a <= g_mission.train.size();) {
        const size_t b = g_mission.train.find(',', a);
        const int v = to_int(g_mission.train.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (v > 0) cars.push_back(v);
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return cars;
}
std::string mission_arrivals_refusal() {
    if (g_arrivals.on || !g_arrivals_refusal) return "";
    return g_arrivals_refusal == 1 ? ui_text("ArrNoBlocks", "THIS MAP'S SCRIPT HAS NO EDITOR BLOCKS (A CAMPAIGN MISSION, THE "
                                                            "SANDBOX): SAVE THE MAP UNDER A NEW NAME FIRST")
                                   : ui_text("ArrNoTact", "THIS SCRIPT'S MAIN() HAS NO LINE TRAINSTACT(); TO CALL THE ARRIVALS "
                                                          "FROM");
}
const std::vector<TrainTemplate>& mission_train_templates() { return templates(); }

void mission_ai_use(int& patrols, int& posts, int& start_trains, int& queue_trains) {
    patrols = posts = 0;
    for (const EnemyTrain& t : g_mission.start) patrols += t.patrol;
    for (const FactoryTrain& t : g_mission.factories.queue) {
        patrols += t.task == TASK_PATROL;
        posts += t.task == TASK_GUARD;
    }
    for (const Arrival& a : g_arrivals.list)
        if (a.army == 1) {
            patrols += a.task == TASK_PATROL;
            posts += a.task == TASK_GUARD;
        }
    if (g_arrivals.on)   // the enemy trains' orders (OrdersTact)
        for (const TrainPatrol& o : edit_train_patrols())
            if (o.army == 1) {
                patrols += o.task == TASK_PATROL;
                posts += o.task == TASK_GUARD;
            }
    start_trains = static_cast<int>(g_mission.start.size());
    queue_trains = static_cast<int>(g_mission.factories.queue.size());
}

bool Arrival::operator==(const Arrival& o) const {
    return army == o.army && cars == o.cars && x == o.x && y == o.y && z == o.z && entry == o.entry && first == o.first &&
           every == o.every && times == o.times && cond == o.cond && cond_n == o.cond_n && earlier == o.earlier &&
           task == o.task && tx == o.tx && ty == o.ty && tz == o.tz && tx2 == o.tx2 && ty2 == o.ty2 && raid == o.raid &&
           message == o.message;
}

bool mission_dialog(HWND owner, bool* open_arrivals) {
    using namespace dialog;
    // the trains' rows on the left as they were; the map's name on top of them, the money, the time
    // and the weather in a column on the right, each with its label in full words
    DialogTemplate t(ui_wtext("MissionTitle", "Mission settings").c_str(), 620, 330);
    const DWORD label = SS_LEFT;
    t.item(STATIC, ui_wtext("MissionMapTitle", "Title on the custom maps page:").c_str(), -1, label, 8, 8, 150, 8);
    t.item(EDIT, L"", IDC_TITLE, LINE, 160, 6, 252, 12);
    t.item(STATIC, ui_wtext("MissionHeader", "Briefing header:").c_str(), -1, label, 8, 26, 150, 8);
    t.item(EDIT, L"", IDC_HEADER, LINE, 160, 24, 252, 12);
    const short cx = 424, cw = 188;   // the column on the right
    t.item(STATIC, ui_wtext("MissionMoney", "Money at the start:").c_str(), -1, label, cx, 8, 100, 8);
    t.item(EDIT, L"", IDC_MONEY, LINE | ES_NUMBER, cx + 104, 6, cw - 104, 12);
    t.item(STATIC, ui_wtext("MissionStartAt", "Time of day at the start:").c_str(), -1, label, cx, 26, 100, 8);
    t.item(COMBOBOX, L"", IDC_TIME, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, cx + 104, 24, cw - 104, 80);
    t.item(STATIC, ui_wtext("MissionDayNote", "Then it goes on: day, evening, night, morning, 5 minutes each.").c_str(), -1,
           label, cx, 40, cw, 16);
    t.item(STATIC, ui_wtext("MissionWeather", "Weather:").c_str(), -1, label, cx, 64, 100, 8);
    t.item(COMBOBOX, L"", IDC_WEATHER, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, cx + 104, 62, cw - 104, 90);
    t.item(STATIC, ui_wtext("MissionRain", "How often:").c_str(), -1, label, cx, 82, 100, 8);
    t.item(COMBOBOX, L"", IDC_RAIN, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, cx + 104, 80, cw - 104, 60);
    t.item(STATIC, ui_wtext("MissionWeatherNote", "Now and then: it comes and goes, about half a minute each. Clouds "
                                                  "and storm clouds pass over the map.").c_str(),
           -1, label, cx, 96, cw, 24);
    t.item(BUTTON, ui_wtext("MissionLightning", "Lightning").c_str(), IDC_NUKE, BS_AUTOCHECKBOX | WS_TABSTOP, cx, 124, cw, 12);
    t.item(STATIC, ui_wtext("MissionLightningNote", "Flashes in a clear sky; with rain, only while it rains; none with "
                                                    "snow, clouds or meteorites.").c_str(),
           -1, label, cx, 138, cw, 24);
    t.item(STATIC, ui_wtext("MissionAttackers", "Enemy attackers at once:").c_str(), -1, label, cx, 170, 150, 8);
    t.item(EDIT, L"", IDC_LIMIT, LINE | ES_NUMBER, cx + 156, 168, cw - 156, 12);
    if (g_arrivals.on) {   // the trains that arrive: the Arrivals panel
        t.item(STATIC,
               ui_wide(ui_printf(ui_format("MissionArrivals", "Train arrivals: %u - set in the Arrivals panel (key 0 of the bar)"),
                                 static_cast<unsigned>(g_arrivals.list.size())))
                   .c_str(),
               -1, label, 8, 44, 300, 8);
        t.item(BUTTON, ui_wtext("MissionOpenArrivals", "Arrivals...").c_str(), IDC_ARRIVALS, BS_PUSHBUTTON | WS_TABSTOP, 332, 42, 80, 12);
    } else {
        t.item(STATIC, ui_wtext("MissionWaves", "Player's reinforcement waves:").c_str(), -1, label, 8, 44, 102, 8);
        t.item(EDIT, L"", IDC_WAVES, LINE | ES_NUMBER, 112, 42, 24, 12);
        t.item(STATIC, ui_wtext("MissionTrain", "its train (VIDs, e.g. 5,20):").c_str(), -1, label, 146, 44, 92, 8);
        t.item(EDIT, L"", IDC_TRAIN, LINE, 238, 42, 174, 12);
    }
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
    t.item(BUTTON, ui_wtext("ButtonOk", "OK").c_str(), IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP, 504, 312, 50, 14);
    t.item(BUTTON, ui_wtext("ButtonCancel", "Cancel").c_str(), IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 562, 312, 50, 14);
    g_open_arrivals = false;
    const INT_PTR result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), t.get(), owner, mission_proc, 0);
    if (result == 1) patch::log("mission: settings changed");
    if (open_arrivals) *open_arrivals = result == 1 && g_open_arrivals;
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
    // a map saved from the template or from the editor's new map keeps nothing of its name:
    // "TEMPLATE" or the new map's "NEW MAP" as the title (the custom maps page) or the briefing's
    // header (the Esc menu's first page) becomes the map's own name
    if (lstrcmpiA(folder_name(map).c_str(), "template") != 0 && !editor_unsaved_map(map)) {
        const std::string unnamed = editor_unsaved_map(g_map) ? editor_new_map_title() : "";
        auto placeholder = [&](const std::wstring& text, size_t length) {
            const std::string a = ascii_upper(text, length);
            return a == "TEMPLATE" || (!unnamed.empty() && a == unnamed);
        };
        const bool title = placeholder(g_mission.title, 24);
        if (title) g_mission.title = from_pack(upper(folder_name(map)));
        if (title || placeholder(g_mission.header, 32)) g_mission.header = g_mission.title;
    }
    // the blocks anew: each one's lines after its first marker up to its last (main()'s keep the
    // indentation of their marker); a map converted to arrivals gets their two blocks, the runner
    // after header.lgc's line and its call after TrainsTact();
    auto indent_of = [&](int i) { return lines[i].substr(0, lines[i].find_first_not_of(" \t")); };
    if (b.arrivals() && !g_arrivals.on) {   // a new map on the template's script (it runs arrivals)
        g_arrivals.on = true;
        g_arrivals.waves = g_mission.reinforcements;
    }
    // what is written: a script that lost its TrainsTact(); (edited outside the editor) cannot run
    // the arrivals - they are not written, but the editor keeps them and says so
    ArrivalsState arrivals = g_arrivals;
    if (arrivals.on && !b.arrivals() && !(b.header >= 0 && b.trains_tact > b.header)) {
        patch::log("mission: %s has no TrainsTact(); after header.lgc - its arrivals are not written", script.c_str());
        edit_message_text("MsgArrivalsNotWritten", "saved without the arrivals, limits and later units: the script has no TrainsTact();");
        arrivals.on = false;
    }
    struct Splice {
        int after, resume;   // the lines up to `after` stay, then `lines`, then from `resume` on
        std::vector<std::string> lines;
    };
    // the factories as they stand; a deleted one's units are not written
    Mission written = g_mission;
    written.factories.unlocks = edit_place_unlocks(g_mission.factories.unlocks);
    std::vector<Splice> splices = {{b.open, b.close, settings_block(written, arrivals, map)},
                                   {b.main_open, b.main_close, push_block(indent_of(b.main_open))}};
    if (arrivals.on) {
        const std::vector<std::string> runner(std::begin(ARRIVALS_RUNNER), std::end(ARRIVALS_RUNNER));
        if (b.arrivals()) {
            splices.push_back({b.arrivals_open, b.arrivals_close, runner});
            splices.push_back({b.tact_open, b.tact_close, {indent_of(b.tact_open) + "ArrivalsTact();"}});
        } else {
            std::vector<std::string> block{ARRIVALS_OPEN};
            block.insert(block.end(), runner.begin(), runner.end());
            block.push_back(ARRIVALS_CLOSE);
            splices.push_back({b.header, b.header + 1, block});
            const std::string indent = indent_of(b.trains_tact);
            splices.push_back({b.trains_tact, b.trains_tact + 1, {indent + TACT_OPEN, indent + "ArrivalsTact();", indent + TACT_CLOSE}});
            patch::log("mission: %s converted to arrivals", script.c_str());
        }
    }
    std::sort(splices.begin(), splices.end(), [](const Splice& a, const Splice& c) { return a.after < c.after; });
    std::vector<std::string> out;
    int next = 0;
    for (const Splice& s : splices) {
        out.insert(out.end(), lines.begin() + next, lines.begin() + s.after + 1);
        out.insert(out.end(), s.lines.begin(), s.lines.end());
        next = s.resume;
    }
    out.insert(out.end(), lines.begin() + next, lines.end());
    const char* eol = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::string data;
    for (size_t i = 0; i < out.size(); ++i) data += out[i] + (i + 1 < out.size() ? eol : "");
    if (!write_file(script, data)) return;
    g_managed = true;
    g_map = map;
    // the texts - into the pack's copy when the map has one (the sandbox's, lang\<code>\maps\...),
    // which the game reads instead of the root's
    const std::string folder = folder_of(map);
    auto write_text = [](const std::string& path, const std::string& data) { write_file(language_file(path), data); };
    write_text(folder + "\\objectives.txt", text_file(g_mission.objectives));
    write_text(folder + "\\win.txt", text_file(g_mission.win));
    write_text(folder + "\\loose.txt", text_file(g_mission.lose));
    std::vector<std::wstring> pages = g_mission.brief;
    if (pages.empty()) pages.push_back(g_mission.objectives);
    std::string list;
    for (size_t i = 0; i < pages.size(); ++i) {
        const std::string page = folder + (i ? "\\brief" + std::to_string(i + 1) + ".txt" : "\\brief.txt");
        write_text(page, text_file(pages[i]));
        list += (i ? "\r\n" : "") + page;
    }
    // the high scores page stays last when the list had it and the folder has the page
    if (g_mission.scores_page && exists(folder + "\\hiscores.txt")) list += "\r\n" + folder + "\\hiscores.txt";
    write_file(folder + "\\brief.lst", list);
    patch::log("mission: %s written - %u push lines, %u start trains, %u in the queue, %u briefing pages",
               script.c_str(), static_cast<unsigned>(g_lines.size()), static_cast<unsigned>(g_mission.start.size()),
               static_cast<unsigned>(g_mission.factories.queue.size()), static_cast<unsigned>(pages.size()));
    // maps\<name>\<name>.map: the custom maps page (not the template itself, nor the editor's new map
    // written into its scratch folder, as Run the level does)
    const std::string name = folder_name(map);
    const size_t slash = map.find_last_of('\\'), dot = map.find_last_of('.');
    const std::string base = slash == std::string::npos ? map : map.substr(slash + 1, dot - slash - 1);
    if (lstrcmpiA(folder.c_str(), ("maps\\" + name).c_str()) == 0 && lstrcmpiA(base.c_str(), name.c_str()) == 0 &&
        lstrcmpiA(name.c_str(), "template") != 0 && !editor_unsaved_map(map))
        add_to_custom_list(name, ascii_upper(g_mission.title, 24));
}

// An arrival of the test file: words key=value - army=0-3, cars=5,20,20, at=x,y,z (the entry),
// first=, every=, times= (s, s, waves; 0 without end), if=<ArrivalIf number>, n=, earlier=0/1,
// task=none|attack|forts|guard|patrol|drive, to=x,y,z (its point), to2=x,y (the patrol's second),
// raid=, msg=0/1.
static bool arrival_of_line(const std::string& text, Arrival& a) {
    static const char* const TASKS[] = {"none", "attack", "forts", "guard", "patrol", "drive"};
    a = Arrival();
    for (size_t p = 0; p < text.size();) {
        const size_t e = text.find(' ', p);
        const std::string word = text.substr(p, e == std::string::npos ? std::string::npos : e - p);
        p = e == std::string::npos ? text.size() : e + 1;
        if (word.empty()) continue;
        const size_t eq = word.find('=');
        if (eq == std::string::npos) return false;
        const std::string key = word.substr(0, eq), value = word.substr(eq + 1);
        std::vector<int> v;
        for (size_t q = 0; q <= value.size();) {
            const size_t c = value.find(',', q);
            v.push_back(to_int(value.substr(q, c == std::string::npos ? std::string::npos : c - q)));
            if (c == std::string::npos) break;
            q = c + 1;
        }
        auto three = [&](int& x, int& y, int* z) {
            if (v.size() < 2) return false;
            x = v[0];
            y = v[1];
            if (z) *z = v.size() > 2 ? v[2] : 0;
            return true;
        };
        if (key == "army") a.army = (std::max)(0, (std::min)(3, v[0]));
        else if (key == "cars") a.cars = v;
        else if (key == "at") {
            if (!three(a.x, a.y, &a.z)) return false;
            a.entry = true;
        } else if (key == "first") a.first = v[0];
        else if (key == "every") a.every = v[0];
        else if (key == "times") a.times = v[0];
        else if (key == "if") a.cond = (std::max)(0, (std::min)(IF_COUNT - 1, v[0]));
        else if (key == "n") a.cond_n = v[0];
        else if (key == "earlier") a.earlier = v[0] != 0;
        else if (key == "raid") a.raid = v[0];
        else if (key == "msg") a.message = v[0] != 0;
        else if (key == "to") {
            if (!three(a.tx, a.ty, &a.tz)) return false;
        } else if (key == "to2") {
            if (!three(a.tx2, a.ty2, nullptr)) return false;
        } else if (key == "task") {
            const auto t = std::find_if(std::begin(TASKS), std::end(TASKS), [&](const char* s) { return value == s; });
            if (t == std::end(TASKS)) return false;
            a.task = static_cast<int>(t - std::begin(TASKS));
        } else {
            return false;
        }
    }
    return a.entry && !a.cars.empty();
}

// Test mode (editor.cpp's WM_COPYDATA 40042): the settings of a file instead of the dialog - UTF-8
// lines "key: value" with the keys title, header, money, time (1-4), rain (0-2), weather (0-4), nuke (0/1), limit,
// waves, train, start and queue (a line per train), objectives, win, lose and brief ("\n" in a
// value is a line end, "---" between briefing pages); the keys not given stay as they are. The
// lines "arrival: ..." (arrival_of_line) replace the map's arrivals and make the map run them
// ("arrivals: keep" keeps the authors' reinforcement waves, "arrivals:" alone sets them to 0); the
// lines "limit: army nvid max" and "unlock: x y nvid seconds" (a factory's place on the map) replace
// its unit limits and the units the factories get later.
bool mission_settings_from_file(const std::string& path) {
    static const std::map<std::string, int> KEYS = {
        {"title", IDC_TITLE}, {"header", IDC_HEADER}, {"money", IDC_MONEY}, {"time", IDC_TIME},
        {"rain", IDC_RAIN}, {"weather", IDC_WEATHER}, {"nuke", IDC_NUKE}, {"limit", IDC_LIMIT}, {"waves", IDC_WAVES},
        {"train", IDC_TRAIN}, {"start", IDC_START}, {"queue", IDC_QUEUE}, {"objectives", IDC_OBJECTIVES},
        {"win", IDC_WIN}, {"lose", IDC_LOSE}, {"brief", IDC_BRIEF}};
    Fields fields = current_fields();
    std::map<int, bool> cleared;
    ArrivalsState arrivals = g_arrivals;
    bool arrivals_given = false;
    FactoryState factories;   // the file's limits and later units replace the map's
    bool factories_given = false;
    bool orders_given = false;   // the file's orders replace the map's
    for (const std::string& line : split_lines(read_file(path))) {
        const size_t colon = line.find(':');
        if (colon == std::string::npos) continue;
        const std::string name = trim(line.substr(0, colon));
        if (name == "order") {   // order: vid x y drive|patrol|attack|guard [to_x to_y to_z] (a car of the map)
            const std::vector<std::string> w = words_of(line.substr(colon + 1));
            static const char* const TASKS[] = {"", "attack", "", "guard", "patrol", "drive"};
            int task = 0;
            for (int k = 1; k < 6; ++k)
                if (w.size() > 3 && w[3] == TASKS[k] && *TASKS[k]) task = k;
            if (w.size() < 4 || !task) {
                patch::log("mission test: a wrong order line: %s", line.c_str());
                return false;
            }
            TrainPatrol t{to_int(w[0]), to_int(w[1]), to_int(w[2]), w.size() > 4 ? to_int(w[4]) : 0, w.size() > 5 ? to_int(w[5]) : 0};
            t.task = task;
            t.to_z = w.size() > 6 ? to_int(w[6]) : 0;
            if (!orders_given) g_patrols.clear();
            orders_given = true;
            g_patrols.push_back(t);
            continue;
        }
        if (name == "limit" || name == "unlock") {   // limit: army nvid max / unlock: x y nvid seconds
            std::vector<int> v;
            for (const std::string& w : words_of(line.substr(colon + 1))) v.push_back(to_int(w));
            if (v.size() != (name == "limit" ? 3u : 4u)) {
                patch::log("mission test: a wrong %s line: %s", name.c_str(), line.c_str());
                return false;
            }
            factories_given = true;
            if (name == "limit") factories.limits.push_back({v[0], v[1], v[2]});
            else factories.unlocks.push_back({v[0], v[1], v[2], v[3], nullptr});
            continue;
        }
        if (name == "arrivals" || name == "arrival") {
            if (!arrivals_given) arrivals.list.clear();   // the file's arrivals replace the map's
            arrivals_given = true;
            arrivals.on = true;
            if (name == "arrivals") arrivals.waves = trim(line.substr(colon + 1)) == "keep" ? arrivals.waves : 0;
            Arrival a;
            if (name == "arrival" && !arrival_of_line(trim(line.substr(colon + 1)), a)) {
                patch::log("mission test: a wrong arrival line: %s", line.c_str());
                return false;
            }
            if (name == "arrival") arrivals.list.push_back(a);
            continue;
        }
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
    if (arrivals.on && !g_arrivals.on && g_arrivals_refusal) {
        patch::log("mission test: the map cannot run arrivals (reason %d)", g_arrivals_refusal);
        return false;
    }
    if (factories_given) {
        n.factories.limits = factories.limits;
        n.factories.unlocks = factories.unlocks;
    }
    g_mission = n;
    g_arrivals = arrivals;
    patch::log("mission test: settings of %s - %u start trains, %u in the queue, %u arrivals", path.c_str(),
               static_cast<unsigned>(n.start.size()), static_cast<unsigned>(n.factories.queue.size()),
               static_cast<unsigned>(g_arrivals.list.size()));
    return true;
}