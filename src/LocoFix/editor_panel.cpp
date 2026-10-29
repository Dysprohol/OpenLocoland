// The editor's Help and Menu buttons and their panels (editor mode, editor_edit.cpp draws the bar).
//
// The two round sockets at the left end of the bar, where the game puts its pause button (VID 498,
// 25 px) and its "up to the menu" button (VID 499, 38 px) - at x = the bar's middle - 274 and - 236,
// y = the view's bottom - 19 and - 24, as Player_ShowStateBar puts them - hold the editor's buttons:
// VID 942, "?" (help), and VID 943, three bars (menu), made from those two by build.py
// (patch_data.make_editor_buttons; frame 0 dark, 1 lit - lit under the mouse and while its panel
// is open). F1 opens the help too.
//
// A panel looks like the game's own menu pages and is drawn with their pictures over the held
// level (the scripts are off there): the in-game menu device (VID 466, 1024 x 768) in the middle of
// the view, in it the picture with the button bar (VID 486 for Help, 484 for Menu) where the menu
// script puts its pictures (22 px right of the device's middle, 18 up), the title in the menu
// titles' font (VID 3), the text in the menu font (VID 2, ESC 7 / ESC 2 switch to the lit one, VID
// 7, and back, as in maps\menu41.txt), the page arrows (VIDs 475 / 476) and the start button (VID
// 477) with "back to the map". Help shows the pages of maps\edithelp.lst (edithelp1.txt ... - the
// chosen language pack's copies: the files are opened through the game's own CreateFileA import,
// which language.cpp redirects). It opens on its contents: the categories of the list's second
// column down the left, the pages of the one under the mouse on the right (a lamp and the page's
// first line), a click opens a page; a page has its category for the title, CONTENTS and its number
// above it, and Esc, the right button or CONTENTS go back to the contents. Menu has a row per command - the lamp of the custom maps page (VID
// 472) and the text - and a click sends the command of the window's menu (WM_COMMAND), so it does
// what File, Edit and Options do: New, Load, Save, Mission settings, Map size, Play the map, Undo,
// Redo, Run the level, Back to the menu, Exit. The panel is modal: Esc, the right button or "back
// to the map" close it; Left / Right, Page Up / Down, the wheel and the arrows turn the help's pages
// (the contents before the first).
//
// The palette (the bar's "All objects", key 7) is a panel of the same kind with every object kind
// the missions use (game/editor/palette.tsv, the game's copy editor\palette.tsv with each one's
// icon in VID 944 and a unit's state from the missions - patch_data.make_editor_palette): the
// groups down the left (meadow, forest, desert, ice, volcanic, swamp, park, pirates, city, industry,
// railway, flags and crates, wrecks, units and creatures), a page of 16 icons on the right, the name
// of the one under the mouse below them, the page arrows when a group has more. A click on an icon
// picks the object - the bar's entry places it, in the army the palette gives it - and closes the
// panel. Opened for the Arrivals panel's train (panel_open_vehicles_for_train), the Vehicles
// palette adds the car picked to it and goes back there.
//
// The Arrivals panel (the bar's entry 0) and a factory's panel are drawn and clicked by
// editor_arrivals.cpp and editor_factory.cpp in this frame; Ctrl+Z / Ctrl+Y reach the editor's
// history while one is open.
#include "patch.h"
#include "editor_text.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// editor_edit.cpp: drawing in front of the scene, the view and the bar
void edit_draw_picture(int nvid, int frame, float x, float y, int step);
float edit_draw_text(int font, float x, float y, const char* text, int step);
bool edit_font_size(int font, float& w, float& h);
void edit_view_box(float& l, float& t, float& r, float& b);
void edit_bar_box(float& l, float& t, float& r, float& b);
void edit_pick_object(int nvid, int army);
// editor_arrivals.cpp: the Arrivals panel
void arrivals_draw(POINT mouse);
bool arrivals_click(POINT mouse);
void arrivals_add_car(int nvid);
bool arrivals_back();   // a step back inside the panel (an arrival -> the list); false at the list
bool arrivals_has_back();
// editor_factory.cpp: a factory's panel
void factory_draw(POINT mouse);
bool factory_click(POINT mouse);
void factory_add_car(int nvid);
bool factory_back();   // the templates -> the queue; false at the queue
bool factory_has_back();
void factory_open(void* factory);
bool edit_arrival_picking();   // editor_edit.cpp: a point for the Arrivals panel is being picked
void edit_fill(float l, float t, float r, float b, uint32_t argb);
bool edit_escape();            // editor_edit.cpp: Esc ends what is under way there (true when it did)
bool editor_level_held();      // editor.cpp

namespace {

constexpr uintptr_t IAT_CreateFileA = 0x4750f8;   // language.cpp redirects it to the chosen pack
constexpr int VID_HELP_BUTTON = 942, VID_MENU_BUTTON = 943;
constexpr int VID_DEVICE = 466, VID_HELP_PICTURE = 486, VID_MENU_PICTURE = 484;
constexpr int VID_PREVIOUS = 475, VID_NEXT = 476, VID_START = 477, VID_LAMP = 472;
constexpr int FONT_TITLE = 3, FONT_TEXT = 2, FONT_LIT = 7;
constexpr float HELP_X = -274, HELP_Y = -19, HELP_R = 12;    // from the bar's middle and the view's bottom
constexpr float MENU_X = -236, MENU_Y = -24, MENU_R = 19;
constexpr float PICTURE_DX = 22, PICTURE_DY = -18;           // the picture from the device's middle
constexpr float TITLE_X = -182, TITLE_Y = -101, TEXT_X = -175, TEXT_Y = -70;   // from the picture's middle
constexpr float BUTTONS_Y = 131, PREVIOUS_X = -151, NEXT_X = 152;
constexpr float ARROW_W = 63, START_W = 234, BUTTON_H = 22;
constexpr float ROW_Y = -55, ROW_DY = 26, LEFT_X = -160, RIGHT_X = 10, LAMP_TEXT = 16, ROW_W = 160;
// the help's contents (rows of two lines, a category's name GROUP_W wide), the strip above the title
// (a page's CONTENTS and its number), the text's right end
constexpr float CONTENTS_Y = -57, CONTENTS_DY = 30, GROUP_W = 125, PAGE_LAMP_X = -32, TEXT_RIGHT = 177;
constexpr float STRIP_X = -168, STRIP_Y = -132;
constexpr int STEP = 10;   // in front of the bar (its parts are steps 0-4)

// the window menu's commands (editor.cpp)
constexpr int ID_LOAD = 40001, ID_SAVE = 40002, ID_EXIT = 40004, ID_RUN = 40010, ID_BACK = 40011, ID_PLAY = 40012,
              ID_MISSION = 40013, ID_NEW = 40014, ID_RESIZE = 40015, ID_EXPORT = 40016, ID_IMPORT = 40017, ID_UNDO = 40030,
              ID_REDO = 40031;

using CreateFileA_t = HANDLE(WINAPI*)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);

struct Command {
    const char* key;
    const char* english;
    int id;
};
// two columns of seven: the file, then playing, editing and leaving
const Command COMMANDS[] = {
    {"PanelNew", "NEW MAP", ID_NEW},          {"PanelLoad", "LOAD MAP", ID_LOAD},
    {"PanelSave", "SAVE MAP", ID_SAVE},       {"PanelExport", "EXPORT MAP", ID_EXPORT},
    {"PanelImport", "IMPORT MAP", ID_IMPORT}, {"PanelMission", "MISSION SETTINGS", ID_MISSION},
    {"PanelMapSize", "MAP SIZE", ID_RESIZE},  {"PanelPlay", "PLAY THE MAP", ID_PLAY},
    {"PanelUndo", "UNDO", ID_UNDO},           {"PanelRedo", "REDO", ID_REDO},
    {"PanelRun", "RUN THE LEVEL", ID_RUN},    {"PanelBack", "BACK TO THE MENU", ID_BACK},
    {"PanelExit", "EXIT THE GAME", ID_EXIT},
};
constexpr int COMMAND_COUNT = sizeof COMMANDS / sizeof COMMANDS[0];
constexpr int ROWS = 7;

enum class Panel { None, Help, Menu, Palette, Arrivals, Factory };
Panel g_panel = Panel::None;
// the Vehicles palette adds a car to the train of this panel (Arrivals, Factory) and goes back there
Panel g_for_train = Panel::None;
int g_page = -1;                                 // -1: the contents
std::vector<std::vector<std::string>> g_pages;   // the help's pages, their lines (CP1250)
struct HelpGroup {
    std::string key, english;   // its [Editor] key and the English name
};
std::vector<HelpGroup> g_help_groups;   // in the order of their first pages
std::vector<int> g_page_group;          // each page's
int g_help_group = 0;                   // the one whose pages the contents shows

// ---- the help's pages ----

std::string read_text(const std::string& path) {
    const auto open = *reinterpret_cast<CreateFileA_t*>(IAT_CreateFileA);
    HANDLE f = open(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return "";
    std::string data(GetFileSize(f, nullptr), '\0');
    DWORD got = 0;
    if (data.empty() || !ReadFile(f, &data[0], static_cast<DWORD>(data.size()), &got, nullptr)) got = 0;
    CloseHandle(f);
    data.resize(got);
    return data;
}

std::vector<std::string> lines_of(const std::string& text) {
    std::vector<std::string> lines;
    size_t a = 0;
    while (a <= text.size()) {
        size_t b = text.find('\n', a);
        if (b == std::string::npos) b = text.size();
        std::string line = text.substr(a, b - a);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
        a = b + 1;
    }
    return lines;
}

std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> out;
    size_t a = 0;
    while (true) {
        const size_t b = line.find('\t', a);
        out.push_back(line.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}

// maps\edithelp.lst: a line per page - its file, the [Editor] key of its category and the category's
// English name, tab-separated; a page without them is in the category of the page before.
void load_pages() {
    if (!g_pages.empty()) return;
    for (const std::string& line : lines_of(read_text("maps\\edithelp.lst"))) {
        const std::vector<std::string> f = fields(line);
        if (f[0].empty()) continue;
        g_pages.push_back(lines_of(read_text(f[0])));
        if (f.size() > 1 && !f[1].empty()) {
            const auto same = [&](const HelpGroup& g) { return g.key == f[1]; };
            const auto at = std::find_if(g_help_groups.begin(), g_help_groups.end(), same);
            if (at == g_help_groups.end()) g_help_groups.push_back({f[1], f.size() > 2 ? f[2] : f[1]});
            g_page_group.push_back(static_cast<int>(std::find_if(g_help_groups.begin(), g_help_groups.end(), same) -
                                                    g_help_groups.begin()));
        } else {
            g_page_group.push_back(g_page_group.empty() ? -1 : g_page_group.back());
        }
    }
    if (g_pages.empty()) {
        g_pages.push_back({ui_text("HelpMissing", "NO HELP PAGES (MAPS\\EDITHELP.LST)")});
        g_page_group.push_back(-1);
    }
    if (std::find(g_page_group.begin(), g_page_group.end(), -1) != g_page_group.end()) {   // the first pages had none
        g_help_groups.insert(g_help_groups.begin(), {"HelpHeader", "EDITOR HELP"});
        for (int& g : g_page_group) ++g;
    }
    patch::log("panel: %u help pages in %u categories", static_cast<unsigned>(g_pages.size()),
               static_cast<unsigned>(g_help_groups.size()));
}

const char* group_name(int group) {
    if (group < 0 || group >= static_cast<int>(g_help_groups.size())) return "";
    return ui_text(g_help_groups[group].key.c_str(), g_help_groups[group].english.c_str());
}

std::vector<int> group_pages(int group) {
    std::vector<int> out;
    for (int i = 0; i < static_cast<int>(g_page_group.size()); ++i)
        if (g_page_group[i] == group) out.push_back(i);
    return out;
}

// A page's title for the contents: its first line without the font switches.
std::string page_title(int page) {
    std::string out;
    const std::string& line = g_pages[page].empty() ? out : g_pages[page][0];
    for (size_t i = 0; i < line.size(); ++i)
        if (line[i] == 0x1b) ++i;
        else out += line[i];
    return out;
}

// `text` in lines of at most `width` characters, broken at spaces.
std::vector<std::string> wrap(const std::string& text, size_t width) {
    std::vector<std::string> out;
    std::string line;
    size_t a = 0;
    while (a < text.size()) {
        size_t b = text.find(' ', a);
        if (b == std::string::npos) b = text.size();
        std::string word = text.substr(a, b - a);
        a = b + 1;
        if (word.empty()) continue;
        if (!line.empty() && line.size() + 1 + word.size() > width) {
            out.push_back(line);
            line.clear();
        }
        while (word.size() > width) {   // a word longer than a line: cut
            out.push_back(word.substr(0, width));
            word.erase(0, width);
        }
        line += (line.empty() ? "" : " ") + word;
    }
    if (!line.empty() || out.empty()) out.push_back(line);
    return out;
}

// ---- the palette's objects ----

struct PaletteObject {
    std::string group, key, english, items;
    int nvid = 0, army = 0, frame = -1, behave = 0;
    bool has_state = false;
};
std::vector<PaletteObject> g_palette;
std::vector<std::string> g_all_groups;   // their keys, in the list's order
std::vector<std::string> g_groups;       // the ones shown: all but the vehicles' (GroupTrain...), or those
bool g_vehicles = false;                 // the palette of the Vehicles entry
const char VEHICLE_GROUP[] = "GroupTrain";

bool vehicle_group(const std::string& g) { return g.compare(0, sizeof VEHICLE_GROUP - 1, VEHICLE_GROUP) == 0; }
int g_group = 0, g_palette_page = 0;
constexpr int VID_PALETTE_ICONS = 944, VID_FRAME = 456, PALETTE_COLUMNS = 4, PALETTE_ROWS = 3;
constexpr float GROUP_X = -165, GROUP_Y = -78, GROUP_DY = 13, GRID_X = -20, GRID_Y = -80, CELL = 41, NAME_Y = 92;
// the army of what is placed next, under the icons: ARMY: (and AS IN THE MISSIONS for All objects),
// then the four armies two a line
constexpr float ARMY_Y = GRID_Y + PALETTE_ROWS * CELL + 4, ARMY_DY = 14, ARMY_DX = 92;
// the army chosen in each palette (0 All objects, 1 Vehicles); -1: the one the missions give it
int g_palette_army[2] = {-1, 0};
const char* const ARMY_KEYS[] = {"CtxOurs", "CtxEnemy", "CtxNeutral", "CtxPirates"};
const char* const ARMY_WORDS[] = {"OURS", "ENEMY", "NEUTRAL", "PIRATES"};

void load_palette() {
    if (!g_palette.empty()) return;
    bool header = true;
    for (const std::string& line : lines_of(read_text("editor\\palette.tsv"))) {
        if (line.empty() || line[0] == '#') continue;
        if (header) {   // group nvid army key english frame behave items
            header = false;
            continue;
        }
        const std::vector<std::string> f = fields(line);
        if (f.size() < 6) continue;
        PaletteObject o;
        o.group = f[0];
        o.nvid = std::atoi(f[1].c_str());
        o.army = std::atoi(f[2].c_str());
        o.key = f[3];
        o.english = f[4];
        o.frame = std::atoi(f[5].c_str());
        if (f.size() > 6 && !f[6].empty()) {
            o.behave = std::atoi(f[6].c_str());
            o.items = f.size() > 7 ? f[7] : "";
            o.has_state = true;
        }
        if (std::find(g_all_groups.begin(), g_all_groups.end(), o.group) == g_all_groups.end())
            g_all_groups.push_back(o.group);
        g_palette.push_back(o);
    }
    patch::log("panel: palette of %u objects in %u groups", static_cast<unsigned>(g_palette.size()),
               static_cast<unsigned>(g_all_groups.size()));
}

extern "C" void reset_palette_cache() {
    g_palette.clear();
    g_all_groups.clear();
    g_groups.clear();
    load_palette();
}

void show_groups(bool vehicles) {
    if (vehicles != g_vehicles || g_groups.empty()) {
        g_group = 0;
        g_palette_page = 0;
    }
    g_vehicles = vehicles;
    g_groups.clear();
    for (const std::string& g : g_all_groups)
        if (vehicle_group(g) == vehicles) g_groups.push_back(g);
}

std::vector<const PaletteObject*> group_objects(int group) {
    std::vector<const PaletteObject*> out;
    if (group < 0 || group >= static_cast<int>(g_groups.size())) return out;
    for (const PaletteObject& o : g_palette)
        if (o.group == g_groups[group]) out.push_back(&o);
    return out;
}

const char* name_of(const PaletteObject& o) {
    if (const char* game = ui_unit_name(o.nvid)) return game;
    return o.key.empty() ? "" : ui_text(o.key.c_str(), o.english.c_str());
}

// ---- where things are ----

struct Place {
    float x, y;
};

Place help_button() {
    float l, t, r, b, vl, vt, vr, vb;
    edit_bar_box(l, t, r, b);
    edit_view_box(vl, vt, vr, vb);
    return {(l + r) / 2 + HELP_X, vb + HELP_Y};
}

Place menu_button() {
    float l, t, r, b, vl, vt, vr, vb;
    edit_bar_box(l, t, r, b);
    edit_view_box(vl, vt, vr, vb);
    return {(l + r) / 2 + MENU_X, vb + MENU_Y};
}

Place device() {
    float l, t, r, b;
    edit_view_box(l, t, r, b);
    return {(l + r) / 2, (t + b) / 2};
}

Place picture() {
    const Place d = device();
    return {d.x + PICTURE_DX, d.y + PICTURE_DY};
}

bool inside(POINT m, float x, float y, float w, float h) {
    return std::fabs(m.x - x) < w / 2 && std::fabs(m.y - y) < h / 2;
}

bool on_circle(POINT m, Place c, float r) {
    const float dx = m.x - c.x, dy = m.y - c.y;
    return dx * dx + dy * dy <= r * r;
}

Place row_place(int i) {
    const Place p = picture();
    return {p.x + (i < ROWS ? LEFT_X : RIGHT_X), p.y + ROW_Y + (i % ROWS) * ROW_DY};
}

int row_at(POINT m) {
    for (int i = 0; i < COMMAND_COUNT; ++i) {
        const Place r = row_place(i);
        if (m.x >= r.x - 12 && m.x < r.x + ROW_W && std::fabs(m.y - r.y) < ROW_DY / 2) return i;
    }
    return -1;
}

bool on_close(POINT m) {
    const Place p = picture();
    return inside(m, p.x, p.y + BUTTONS_Y, START_W, BUTTON_H);
}

bool on_previous(POINT m) {
    const Place p = picture();
    const bool shown = (g_panel == Panel::Help && g_page >= 0) ||(g_panel == Panel::Arrivals && arrivals_has_back()) ||
                       (g_panel == Panel::Factory && factory_has_back());
    return shown && inside(m, p.x + PREVIOUS_X, p.y + BUTTONS_Y, ARROW_W, BUTTON_H);
}

bool on_next(POINT m) {
    const Place p = picture();
    return g_panel == Panel::Help && g_page + 1 < static_cast<int>(g_pages.size()) &&
           inside(m, p.x + NEXT_X, p.y + BUTTONS_Y, ARROW_W, BUTTON_H);
}

// The help's contents: the categories down the left, the pages of the chosen one on the right - the
// lamp and the title, two lines at most -, a row each.
Place contents_row(int i, bool page) {
    const Place p = picture();
    return {p.x + (page ? PAGE_LAMP_X : TEXT_X), p.y + CONTENTS_Y + i * CONTENTS_DY};
}

int help_group_at(POINT m) {
    for (int i = 0; i < static_cast<int>(g_help_groups.size()); ++i) {
        const Place r = contents_row(i, false);
        if (m.x >= r.x - 6 && m.x < r.x + GROUP_W && std::fabs(m.y - r.y) < CONTENTS_DY / 2) return i;
    }
    return -1;
}

int help_page_at(POINT m) {
    const std::vector<int> pages = group_pages(g_help_group);
    const Place p = picture();
    for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
        const Place r = contents_row(i, true);
        if (m.x >= r.x - 12 && m.x < p.x + TEXT_RIGHT && std::fabs(m.y - r.y) < CONTENTS_DY / 2) return pages[i];
    }
    return -1;
}

float text_width(int font, const char* text) {
    float w, h;
    if (!edit_font_size(font, w, h)) return 0;
    float n = 0;
    for (const char* c = text; *c; ++c)
        if (*c == 0x1b && c[1]) ++c;   // ESC + font: no width
        else ++n;
    return n * w;
}

// a page's CONTENTS in the strip above the title
bool on_contents(POINT m) {
    const Place p = picture();
    float w, h;
    edit_font_size(FONT_TEXT, w, h);
    const float x = p.x + STRIP_X, y = p.y + STRIP_Y + h / 2;
    return g_panel == Panel::Help && g_page >= 0 && m.x >= x - 12 &&
           m.x < x + LAMP_TEXT + text_width(FONT_TEXT, ui_text("HelpContents", "CONTENTS")) + 4 && std::fabs(m.y - y) < 10;
}

// ---- drawing ----

// A line of the menu font with its ESC 7 / ESC 2 switches (the lit font and back).
void draw_line(float x, float y, const std::string& line, int font) {
    std::string part;
    auto flush = [&] {
        if (!part.empty()) x = edit_draw_text(font, x, y, part.c_str(), STEP + 3);
        part.clear();
    };
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == 0x1b && i + 1 < line.size()) {
            flush();
            font = static_cast<unsigned char>(line[++i]);
        } else {
            part += line[i];
        }
    }
    flush();
}

void draw_centered(float x, float y, const char* text, int font) {
    float w, h;
    edit_font_size(font, w, h);
    edit_draw_text(font, x - text_width(font, text) / 2, y - h / 2, text, STEP + 3);
}

void draw_frame(int picture_vid, const char* title) {
    const Place d = device(), p = picture();
    edit_draw_picture(VID_DEVICE, 0, d.x, d.y, STEP);
    edit_draw_picture(picture_vid, 0, p.x, p.y, STEP + 1);
    edit_draw_text(FONT_TITLE, p.x + TITLE_X, p.y + TITLE_Y, title, STEP + 3);
}

void draw_close(POINT m) {
    const Place p = picture();
    edit_draw_picture(VID_START, 0, p.x, p.y + BUTTONS_Y, STEP + 2);
    draw_centered(p.x, p.y + BUTTONS_Y, ui_text("PanelClose", "BACK TO THE MAP"), on_close(m) ? FONT_LIT : FONT_TEXT);
}

// The lamp of a row (VID 472), its glow (frames 1-4) pulsing under the mouse as on the in-game map.
void draw_lamp(float x, float y, bool lit) {
    edit_draw_picture(VID_LAMP, lit ? 1 + static_cast<int>(GetTickCount() / 120 % 4) : 1, x, y, STEP + 2);
}

// The contents: the categories (the chosen one lit and underlined; the mouse over one chooses it)
// and the chosen one's pages, a click opens one.
void draw_contents(POINT m) {
    draw_frame(VID_HELP_PICTURE, ui_text("HelpHeader", "EDITOR HELP"));
    const Place p = picture();
    float w, h;
    edit_font_size(FONT_TEXT, w, h);
    edit_draw_text(FONT_LIT, p.x + STRIP_X - 7, p.y + STRIP_Y, ui_text("HelpContents", "CONTENTS"), STEP + 3);
    if (help_group_at(m) >= 0) g_help_group = help_group_at(m);
    g_help_group = (std::max)(0, (std::min)(g_help_group, static_cast<int>(g_help_groups.size()) - 1));
    for (int i = 0; i < static_cast<int>(g_help_groups.size()); ++i) {
        const Place r = contents_row(i, false);
        const std::vector<std::string> lines = wrap(group_name(i), static_cast<size_t>(GROUP_W / w));
        const float top = r.y - lines.size() * h / 2;
        for (size_t k = 0; k < lines.size(); ++k)
            edit_draw_text(i == g_help_group ? FONT_LIT : FONT_TEXT, r.x, top + k * h, lines[k].c_str(), STEP + 3);
        if (i == g_help_group) {
            float longest = 0;
            for (const std::string& l : lines) longest = (std::max)(longest, text_width(FONT_TEXT, l.c_str()));
            edit_fill(r.x, top + lines.size() * h + 1, r.x + longest, top + lines.size() * h + 3, 0xe0f0e080);
        }
    }
    const std::vector<int> pages = group_pages(g_help_group);
    const int hover = help_page_at(m);
    for (int i = 0; i < static_cast<int>(pages.size()); ++i) {
        const Place r = contents_row(i, true);
        std::vector<std::string> lines = wrap(page_title(pages[i]), static_cast<size_t>((p.x + TEXT_RIGHT - r.x - LAMP_TEXT) / w));
        if (lines.size() > 2) lines.resize(2);
        draw_lamp(r.x, r.y, pages[i] == hover);
        const float top = r.y - lines.size() * h / 2;
        for (size_t k = 0; k < lines.size(); ++k)
            edit_draw_text(pages[i] == hover ? FONT_LIT : FONT_TEXT, r.x + LAMP_TEXT, top + k * h, lines[k].c_str(), STEP + 3);
    }
}

void draw_help(POINT m) {
    load_pages();
    g_page = (std::max)(-1, (std::min)(g_page, static_cast<int>(g_pages.size()) - 1));
    const Place p = picture();
    if (g_page < 0) {
        draw_contents(m);
    } else {
        // the page's category for the title; above it CONTENTS and the page's number
        draw_frame(VID_HELP_PICTURE, group_name(g_page_group[g_page]));
        float w, h;
        edit_font_size(FONT_TEXT, w, h);
        draw_lamp(p.x + STRIP_X, p.y + STRIP_Y + h / 2, on_contents(m));
        edit_draw_text(on_contents(m) ? FONT_LIT : FONT_TEXT, p.x + STRIP_X + LAMP_TEXT, p.y + STRIP_Y,
                       ui_text("HelpContents", "CONTENTS"), STEP + 3);
        const std::string number =
            ui_printf(ui_format("HelpPageNumber", "%d / %d"), g_page + 1, static_cast<int>(g_pages.size()));
        edit_draw_text(FONT_TEXT, p.x + TEXT_RIGHT - text_width(FONT_TEXT, number.c_str()), p.y + STRIP_Y, number.c_str(), STEP + 3);
        float y = p.y + TEXT_Y;
        for (const std::string& line : g_pages[g_page]) {
            draw_line(p.x + TEXT_X, y, line, FONT_TEXT);
            y += h;
        }
    }
    if (g_page >= 0) edit_draw_picture(VID_PREVIOUS, 0, p.x + PREVIOUS_X, p.y + BUTTONS_Y, STEP + 2);
    if (g_page + 1 < static_cast<int>(g_pages.size())) edit_draw_picture(VID_NEXT, 0, p.x + NEXT_X, p.y + BUTTONS_Y, STEP + 2);
    draw_close(m);
}

void draw_menu(POINT m) {
    draw_frame(VID_MENU_PICTURE, ui_text("PanelMenuHeader", "EDITOR"));
    const int hover = row_at(m);
    float w, h;
    edit_font_size(FONT_TEXT, w, h);
    for (int i = 0; i < COMMAND_COUNT; ++i) {
        const Place r = row_place(i);
        draw_lamp(r.x, r.y, i == hover);
        edit_draw_text(i == hover ? FONT_LIT : FONT_TEXT, r.x + LAMP_TEXT, r.y - h / 2, ui_text(COMMANDS[i].key, COMMANDS[i].english),
                       STEP + 3);
    }
    draw_close(m);
}

Place group_place(int i) {
    const Place p = picture();
    return {p.x + GROUP_X, p.y + GROUP_Y + i * GROUP_DY};
}

int group_at(POINT m) {
    for (int i = 0; i < static_cast<int>(g_groups.size()); ++i) {
        const Place g = group_place(i);
        if (m.x >= g.x - 4 && m.x < g.x + 140 && m.y >= g.y && m.y < g.y + GROUP_DY) return i;
    }
    return -1;
}

Place cell_place(int i) {   // the middle of the page's cell i
    const Place p = picture();
    return {p.x + GRID_X + (i % PALETTE_COLUMNS) * CELL + CELL / 2, p.y + GRID_Y + (i / PALETTE_COLUMNS) * CELL + CELL / 2};
}

int pages_of(int group) {
    const int n = static_cast<int>(group_objects(group).size()), per = PALETTE_COLUMNS * PALETTE_ROWS;
    return (std::max)(1, (n + per - 1) / per);
}

const PaletteObject* object_at(POINT m) {
    const std::vector<const PaletteObject*> objects = group_objects(g_group);
    const int per = PALETTE_COLUMNS * PALETTE_ROWS;
    for (int i = 0; i < per; ++i) {
        const int k = g_palette_page * per + i;
        if (k >= static_cast<int>(objects.size())) break;
        const Place c = cell_place(i);
        if (inside(m, c.x, c.y, CELL, CELL)) return objects[k];
    }
    return nullptr;
}

// The palette's army choices where they are drawn: -1 (as in the missions, All objects only), 0-3.
struct ArmyChoice {
    int army;
    const char* text;
    float x, y, w;
};

std::vector<ArmyChoice> army_choices() {
    std::vector<ArmyChoice> out;
    if (g_for_train != Panel::None) return out;   // a car for a train takes the train's army
    const Place p = picture();
    float w, h;
    edit_font_size(FONT_TEXT, w, h);
    const float label = text_width(FONT_TEXT, ui_text("CtxArmy", "ARMY:")) + w;
    if (!g_vehicles) {
        const char* missions = ui_text("PaletteArmyMissions", "AS IN THE MISSIONS");
        out.push_back({-1, missions, p.x + GRID_X + label, p.y + ARMY_Y, text_width(FONT_TEXT, missions)});
    }
    for (int a = 0; a < 4; ++a) {
        const char* text = ui_text(ARMY_KEYS[a], ARMY_WORDS[a]);
        out.push_back({a, text, p.x + GRID_X + (a % 2) * ARMY_DX, p.y + ARMY_Y + (1 + a / 2) * ARMY_DY, text_width(FONT_TEXT, text)});
    }
    return out;
}

const ArmyChoice* army_at(POINT m, const std::vector<ArmyChoice>& choices) {
    float w, h;
    edit_font_size(FONT_TEXT, w, h);
    for (const ArmyChoice& c : choices)
        if (m.x >= c.x - 3 && m.x < c.x + c.w + 3 && m.y >= c.y - 1 && m.y < c.y + h + 1) return &c;
    return nullptr;
}

bool on_palette_previous(POINT m) {
    const Place p = picture();
    return g_palette_page > 0 && inside(m, p.x + PREVIOUS_X, p.y + BUTTONS_Y, ARROW_W, BUTTON_H);
}

bool on_palette_next(POINT m) {
    const Place p = picture();
    return g_palette_page + 1 < pages_of(g_group) && inside(m, p.x + NEXT_X, p.y + BUTTONS_Y, ARROW_W, BUTTON_H);
}

void draw_palette(POINT m) {
    load_palette();
    g_group = (std::max)(0, (std::min)(g_group, static_cast<int>(g_groups.size()) - 1));
    const std::string group = g_groups.empty() ? "" : g_groups[g_group];
    const std::string title = std::string(g_vehicles ? ui_text("VehiclesHeader", "VEHICLES") : ui_text("PaletteHeader", "ALL OBJECTS")) +
                              (group.empty() ? "" : std::string(": ") + ui_text(group.c_str(), group.c_str() + 5));
    draw_frame(VID_MENU_PICTURE, title.c_str());
    const Place p = picture();
    const int hover_group = group_at(m);
    for (int i = 0; i < static_cast<int>(g_groups.size()); ++i) {
        const Place g = group_place(i);
        edit_draw_text(i == g_group || i == hover_group ? FONT_LIT : FONT_TEXT, g.x, g.y,
                       ui_text(g_groups[i].c_str(), g_groups[i].c_str() + 5), STEP + 3);
    }
    const std::vector<const PaletteObject*> objects = group_objects(g_group);
    const int per = PALETTE_COLUMNS * PALETTE_ROWS;
    const PaletteObject* hover = object_at(m);
    for (int i = 0; i < per; ++i) {
        const int k = g_palette_page * per + i;
        if (k >= static_cast<int>(objects.size())) break;
        const Place c = cell_place(i);
        if (objects[k]->frame >= 0) edit_draw_picture(VID_PALETTE_ICONS, objects[k]->frame, c.x, c.y, STEP + 2);
        if (objects[k] == hover) edit_draw_picture(VID_FRAME, 0, c.x, c.y, STEP + 3);
    }
    if (hover) {
        char line[128];
        std::snprintf(line, sizeof line, "%s (VID %d)", name_of(*hover), hover->nvid);
        edit_draw_text(FONT_LIT, p.x + GRID_X, p.y + NAME_Y, line, STEP + 3);
    }
    const std::vector<ArmyChoice> armies = army_choices();
    if (!armies.empty()) {   // the army of what is placed next, the chosen one underlined
        float w, h;
        edit_font_size(FONT_TEXT, w, h);
        edit_draw_text(FONT_TEXT, p.x + GRID_X, p.y + ARMY_Y, ui_text("CtxArmy", "ARMY:"), STEP + 3);
        const ArmyChoice* over = army_at(m, armies);
        for (const ArmyChoice& c : armies) {
            const bool chosen = g_palette_army[g_vehicles ? 1 : 0] == c.army;
            edit_draw_text(chosen || &c == over ? FONT_LIT : FONT_TEXT, c.x, c.y, c.text, STEP + 3);
            if (chosen) edit_fill(c.x, c.y + h + 1, c.x + c.w, c.y + h + 3, 0xe0f0e080);
        }
    }
    if (g_palette_page > 0) edit_draw_picture(VID_PREVIOUS, 0, p.x + PREVIOUS_X, p.y + BUTTONS_Y, STEP + 2);
    if (g_palette_page + 1 < pages_of(g_group)) edit_draw_picture(VID_NEXT, 0, p.x + NEXT_X, p.y + BUTTONS_Y, STEP + 2);
    draw_close(m);
}

// ---- input ----

void open(Panel panel) {
    g_panel = panel;
    if (panel == Panel::Help) {   // on the contents, at the category of the page last read
        load_pages();
        g_page = -1;
    }
    if (panel != Panel::Palette) g_for_train = Panel::None;
    patch::log("panel: %s", panel == Panel::Help ? "help" : panel == Panel::Menu ? "menu" : panel == Panel::Palette ? "palette"
                            : panel == Panel::Arrivals ? "arrivals" : panel == Panel::Factory ? "factory" : "closed");
}

// The help's page (-1 the contents, before the first page).
void show_page(int page) {
    page = (std::max)(-1, (std::min)(page, static_cast<int>(g_pages.size()) - 1));
    if (page == g_page) return;
    g_page = page;
    if (g_page >= 0) {
        g_help_group = g_page_group[g_page];
        patch::log("panel: help page %d", g_page + 1);
    } else {
        patch::log("panel: help contents");
    }
}

void turn_page(int by) { show_page(g_page + by); }

// A click on the help: a category or a page of the contents, a page's CONTENTS.
void help_click(POINT m) {
    if (g_page < 0 && help_group_at(m) >= 0) g_help_group = help_group_at(m);
    else if (g_page < 0 && help_page_at(m) >= 0) show_page(help_page_at(m));
    else if (on_contents(m)) show_page(-1);
}

void run(HWND window, int i) {
    open(Panel::None);
    patch::log("panel: command %s (%d)", COMMANDS[i].english, COMMANDS[i].id);
    PostMessageA(window, WM_COMMAND, MAKEWPARAM(COMMANDS[i].id, 0), 0);
}

}  // namespace

bool panel_open() { return g_panel != Panel::None; }

// editor_edit.cpp, after the bar: the two buttons and the open panel.
void panel_draw(POINT m) {
    const Place h = help_button(), n = menu_button();
    edit_draw_picture(VID_HELP_BUTTON, g_panel == Panel::Help || on_circle(m, h, HELP_R) ? 1 : 0, h.x, h.y, 3);
    edit_draw_picture(VID_MENU_BUTTON, g_panel == Panel::Menu || on_circle(m, n, MENU_R) ? 1 : 0, n.x, n.y, 3);
    if (g_panel == Panel::Help) draw_help(m);
    else if (g_panel == Panel::Menu) draw_menu(m);
    else if (g_panel == Panel::Palette) draw_palette(m);
    else if (g_panel == Panel::Arrivals) {
        arrivals_draw(m);
        draw_close(m);
        if (arrivals_has_back()) {   // the arrow back to the list of arrivals
            const Place p = picture();
            edit_draw_picture(VID_PREVIOUS, 0, p.x + PREVIOUS_X, p.y + BUTTONS_Y, STEP + 2);
        }
    } else if (g_panel == Panel::Factory) {
        factory_draw(m);
        draw_close(m);
        if (factory_has_back()) {   // the arrow back to the queue
            const Place p = picture();
            edit_draw_picture(VID_PREVIOUS, 0, p.x + PREVIOUS_X, p.y + BUTTONS_Y, STEP + 2);
        }
    }
}

// The name of the button under the mouse for the status line, or nullptr.
const char* panel_tip(POINT m) {
    if (on_circle(m, help_button(), HELP_R)) return ui_text("ButtonHelp", "Help (F1)");
    if (on_circle(m, menu_button(), MENU_R)) return ui_text("ButtonMenu", "Menu");
    return nullptr;
}

// editor_edit.cpp's edit_message first: true when the message was the panel's. While a panel is
// open every click and key is.
bool panel_message(HWND window, UINT m, WPARAM w, POINT mouse) {
    if (g_panel == Panel::None) {
        if (m == WM_LBUTTONDOWN && on_circle(mouse, help_button(), HELP_R)) open(Panel::Help);
        else if (m == WM_LBUTTONDOWN && on_circle(mouse, menu_button(), MENU_R)) open(Panel::Menu);
        else if (m == WM_KEYDOWN && w == VK_F1) open(Panel::Help);
        else if (m == WM_KEYDOWN && w == VK_ESCAPE && !edit_arrival_picking()) {
            if (!edit_escape()) open(Panel::Menu);   // Esc opens Menu when no panel is open and nothing is under way
        }
        else return false;
        return true;
    }
    // the Arrivals panel's changes are steps of the editor's history: Ctrl+Z / Ctrl+Y go there
    if ((g_panel == Panel::Arrivals || g_panel == Panel::Factory) && (m == WM_KEYDOWN || m == WM_KEYUP) &&
        (w == VK_CONTROL || ((w == 'Z' || w == 'Y') && GetKeyState(VK_CONTROL) < 0)))
        return false;
    switch (m) {
    case WM_LBUTTONDOWN:
        if (on_circle(mouse, help_button(), HELP_R)) open(g_panel == Panel::Help ? Panel::None : Panel::Help);
        else if (on_circle(mouse, menu_button(), MENU_R)) open(g_panel == Panel::Menu ? Panel::None : Panel::Menu);
        else if (on_close(mouse)) open(g_for_train);
        else if (on_previous(mouse) && g_panel == Panel::Arrivals) arrivals_back();
        else if (on_previous(mouse) && g_panel == Panel::Factory) factory_back();
        else if (on_previous(mouse)) turn_page(-1);
        else if (on_next(mouse)) turn_page(1);
        else if (g_panel == Panel::Menu && row_at(mouse) >= 0) run(window, row_at(mouse));
        else if (g_panel == Panel::Help) help_click(mouse);
        else if (g_panel == Panel::Arrivals) arrivals_click(mouse);
        else if (g_panel == Panel::Factory) factory_click(mouse);
        else if (g_panel == Panel::Palette) {
            if (on_palette_previous(mouse)) --g_palette_page;
            else if (on_palette_next(mouse)) ++g_palette_page;
            else if (const ArmyChoice* a = army_at(mouse, army_choices())) {
                g_palette_army[g_vehicles ? 1 : 0] = a->army;
                patch::log("panel: palette army %d", a->army);
            } else if (group_at(mouse) >= 0) {
                g_group = group_at(mouse);
                g_palette_page = 0;
            } else if (const PaletteObject* o = object_at(mouse)) {
                patch::log("panel: palette picks VID %d", o->nvid);
                if (g_for_train != Panel::None) {   // a car for the train of the panel it came from
                    const Panel back = g_for_train;
                    open(back);
                    if (back == Panel::Arrivals) arrivals_add_car(o->nvid);
                    else factory_add_car(o->nvid);
                } else {
                    open(Panel::None);
                    edit_pick_object(o->nvid, o->army);   // in the army of the row (palette_army)
                }
            }
        }
        return true;
    case WM_RBUTTONDOWN:
        if (g_panel == Panel::Help && g_page >= 0) show_page(-1);   // a help page: back to the contents
        else if (!(g_panel == Panel::Arrivals && arrivals_back()) && !(g_panel == Panel::Factory && factory_back())) open(g_for_train);
        return true;
    case WM_MOUSEWHEEL:
        if (g_panel == Panel::Help) turn_page(GET_WHEEL_DELTA_WPARAM(w) > 0 ? -1 : 1);
        if (g_panel == Panel::Palette)
            g_palette_page = (std::max)(0, (std::min)(g_palette_page + (GET_WHEEL_DELTA_WPARAM(w) > 0 ? -1 : 1), pages_of(g_group) - 1));
        return true;
    case WM_KEYDOWN:
        if (w == VK_ESCAPE) {
            // Esc toggles the Menu panel: opens it when no panel is open, closes it when Menu is open,
            // closes Help/Palette without opening Menu (a help page goes back to the contents first)
            if (g_panel == Panel::None) open(Panel::Menu);
            else if (g_panel == Panel::Menu) open(Panel::None);
            else if (g_panel == Panel::Help && g_page >= 0) show_page(-1);
            else if (!(g_panel == Panel::Arrivals && arrivals_back()) && !(g_panel == Panel::Factory && factory_back())) open(g_for_train);   // Help, Palette, Arrivals, Factory: close without opening Menu
        } else if (w == VK_F1) open(Panel::None);
        else if (g_panel == Panel::Help && g_page < 0 && (w == VK_UP || w == VK_DOWN))   // the contents' categories
            g_help_group = (std::max)(0, (std::min)(g_help_group + (w == VK_UP ? -1 : 1), static_cast<int>(g_help_groups.size()) - 1));
        else if (g_panel == Panel::Help && g_page < 0 && w == VK_RETURN && !group_pages(g_help_group).empty())
            show_page(group_pages(g_help_group)[0]);
        else if (g_panel == Panel::Help && (w == VK_LEFT || w == VK_PRIOR || w == VK_UP)) turn_page(-1);
        else if (g_panel == Panel::Help && (w == VK_RIGHT || w == VK_NEXT || w == VK_DOWN)) turn_page(1);
        else if (g_panel == Panel::Palette && (w == VK_UP || w == VK_DOWN)) {
            g_group = (std::max)(0, (std::min)(g_group + (w == VK_UP ? -1 : 1), static_cast<int>(g_groups.size()) - 1));
            g_palette_page = 0;
        } else if (g_panel == Panel::Palette && (w == VK_LEFT || w == VK_PRIOR)) g_palette_page = (std::max)(0, g_palette_page - 1);
        else if (g_panel == Panel::Palette && (w == VK_RIGHT || w == VK_NEXT))
            g_palette_page = (std::min)(pages_of(g_group) - 1, g_palette_page + 1);
        return true;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_KEYUP:
    case WM_CHAR:
        return true;
    default:
        return false;   // the mouse moves (the view scrolls at the edge as usual)
    }
}

// ---- the notice of a save ----
//
// After every save (Ctrl+S, Save map, the write of Run the level and Play the map, a Yes to "Save
// the changes?") SAVED in the menu titles' font in the middle of the view, the map's file under it
// in the menu font when it was saved under a name, on a dark box that fades (over the held level
// only: a running level's lighting turns the box light); 1.5 s from the first frame drawn after the
// save (Run the level loads the level first), in front of everything and taking no input. The
// menus' click (SFX 24, PlaySFX(24) of menu.lgc) sounds with it. Play the map starts the game again
// right after its save, so only the click is heard there.

namespace {
constexpr uintptr_t G_SOUND = 0x5d3a54, PLAY_SFX = 0x4447e0;   // g_sound; PlaySFX(g_sound, nsfx, 0, 0)
using PlaySfx_t = void(__fastcall*)(void* sound, void* edx, int nsfx, int a, int b);
constexpr int SFX_CLICK = 24, SAVED_STEP = 20;
constexpr DWORD SAVED_TIME = 1500, SAVED_FADE = 400;
constexpr float SAVED_PAD_X = 28, SAVED_PAD_Y = 14, SAVED_GAP = 8;
bool g_saved_new = false;   // to be shown from the next frame drawn
DWORD g_saved_at = 0;       // the frame it was first drawn, 0 when not shown
std::string g_saved_file;
}  // namespace

// editor.cpp's save_map: the map was written - into `file` when it was saved under a name, empty for
// the scratch folder of a new map (Run the level).
void panel_saved(const std::string& file) {
    g_saved_new = true;
    g_saved_file = file;
    if (void* sound = *reinterpret_cast<void**>(G_SOUND)) reinterpret_cast<PlaySfx_t>(PLAY_SFX)(sound, nullptr, SFX_CLICK, 0, 0);
}

// editor_edit.cpp's DrawScene_editor: every frame drawn, the level held or running.
void panel_draw_saved() {
    if (g_saved_new) {
        g_saved_new = false;
        g_saved_at = GetTickCount();
    }
    if (!g_saved_at) return;
    const DWORD age = GetTickCount() - g_saved_at;
    if (age >= SAVED_TIME) {
        g_saved_at = 0;
        return;
    }
    const char* saved = ui_text("SavedNotice", "SAVED");
    float tw, th, w, h;
    edit_font_size(FONT_TITLE, tw, th);
    edit_font_size(FONT_TEXT, w, h);
    const float width = (std::max)(text_width(FONT_TITLE, saved), text_width(FONT_TEXT, g_saved_file.c_str())) + 2 * SAVED_PAD_X;
    const float height = th + (g_saved_file.empty() ? 0 : SAVED_GAP + h) + 2 * SAVED_PAD_Y;
    const Place d = device();
    const float l = d.x - width / 2, t = d.y - height / 2, r = d.x + width / 2, b = d.y + height / 2;
    const float fade = age + SAVED_FADE > SAVED_TIME ? static_cast<float>(SAVED_TIME - age) / SAVED_FADE : 1.0f;
    const uint32_t box = static_cast<uint32_t>(0xd0 * fade) << 24, edge = static_cast<uint32_t>(0xf0 * fade) << 24 | 0xf0e080;
    if (editor_level_held()) {
        edit_fill(l, t, r, b, box);
        edit_fill(l, t, r, t + 2, edge);
        edit_fill(l, b - 2, r, b, edge);
        edit_fill(l, t, l + 2, b, edge);
        edit_fill(r - 2, t, r, b, edge);
    }
    edit_draw_text(FONT_TITLE, d.x - text_width(FONT_TITLE, saved) / 2, t + SAVED_PAD_Y, saved, SAVED_STEP);
    if (!g_saved_file.empty())
        edit_draw_text(FONT_TEXT, d.x - text_width(FONT_TEXT, g_saved_file.c_str()) / 2, t + SAVED_PAD_Y + th + SAVED_GAP,
                       g_saved_file.c_str(), SAVED_STEP);
}

// The army the palette's row sets for what it places (0 All objects, 1 Vehicles; -1: the one the
// missions give the object), and A in the Place tool setting it.
int panel_palette_army(int which) { return g_palette_army[which & 1]; }
void panel_set_palette_army(int which, int army) { g_palette_army[which & 1] = army; }

// A level goes (editor.cpp's edit_reset): no panel over the next one.
void panel_reset() {
    g_panel = Panel::None;
    g_for_train = Panel::None;
}

// The palette (the bar's "All objects", editor_edit.cpp), or with `vehicles` the cars' (the bar's
// "Vehicles": the groups GroupTrain...).
void panel_open_palette(bool vehicles) {
    g_for_train = Panel::None;
    load_palette();
    show_groups(vehicles);
    open(Panel::Palette);
}

// The Vehicles palette for a car of the Arrivals panel's train: its pick goes there, and closing it
// goes back to that panel.
void panel_open_vehicles_for_train(bool factory) {
    panel_open_palette(true);
    g_for_train = factory ? Panel::Factory : Panel::Arrivals;
}

// The Arrivals panel (editor_arrivals.cpp): the bar's entry 0, a reinforcement's panel, Mission settings.
void panel_open_arrivals() { open(Panel::Arrivals); }

// A factory's panel (editor_factory.cpp): the factory's object panel.
void panel_open_factory(void* factory) {
    factory_open(factory);
    open(Panel::Factory);
}

// editor_arrivals.cpp's frame: the device and the picture with the title, the picture's middle.
void panel_frame(int picture_vid, const char* title) { draw_frame(picture_vid, title); }
void panel_picture(float& x, float& y) {
    const Place p = picture();
    x = p.x;
    y = p.y;
}

// The palette's name of an object (the game's [Units] name first), or nullptr when it has none.
const char* object_name(int nvid) {
    load_palette();
    for (const PaletteObject& o : g_palette)
        if (o.nvid == nvid) {
            const char* name = name_of(o);
            return *name ? name : nullptr;
        }
    return ui_unit_name(nvid);
}

// The frame of an object's icon in VID 944, or -1 when the palette has none.
int palette_icon(int nvid) {
    load_palette();
    for (const PaletteObject& o : g_palette)
        if (o.nvid == nvid) return o.frame;
    return -1;
}

// The state of a unit the missions have (behave and build list), for one placed where the map has
// none of its kind (map_tool.py place's rule).
bool palette_state(int nvid, int& behave, std::vector<short>& items) {
    load_palette();
    for (const PaletteObject& o : g_palette)
        if (o.nvid == nvid && o.has_state) {
            behave = o.behave;
            items.clear();
            for (const std::string& n : lines_of(o.items))
                for (size_t a = 0; a < n.size();) {
                    const size_t b = n.find(' ', a);
                    const std::string v = n.substr(a, b == std::string::npos ? std::string::npos : b - a);
                    if (!v.empty()) items.push_back(static_cast<short>(std::atoi(v.c_str())));
                    if (b == std::string::npos) break;
                    a = b + 1;
                }
            return true;
        }
    return false;
}