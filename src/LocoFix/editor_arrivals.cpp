// The Arrivals panel (editor mode): the trains that arrive on the map - the player's reinforcements,
// the enemy's waves, the neutrals' and the pirates' trains - from a rail point, usually on the
// hidden track past the edge of the map (DEVELOPERS.md "Editor mode", the arrivals; the data and
// the script: editor_mission.h / .cpp).
//
// The bar's entry 0, a click on an arrival's marker on the map, a reinforcement locomotive's panel
// and the Arrivals... button of Mission settings open it. It is one of the panels of
// editor_panel.cpp - the game's menu page (VID 484) in the menu device - and keeps the game's look:
// the menu font (VID 2), lit (VID 7) for what is set and what is under the mouse, the set choice
// underlined, the cars as the palette's icons (VID 944). Three screens:
//   the list    every arrival in plain words, two lines each (a click opens it); + NEW, COPY n,
//               DELETE n for the arrival opened last
//   an arrival  one thing a row: WHO, CARS (in the train's order; a click chooses a car, < > move it,
//               REMOVE takes it away; + CAR the Vehicles palette, TEMPLATE... the authors' trains), ENTRY (PICK ON THE MAP), WHEN (at
//               the start, after n s, when the player has no train, when the player pays n - the last
//               two not before n s), REPEAT (once, n times, without end, every m s), THEN (ours, the
//               neutrals' and the pirates': stays, drives to a point, patrols between two points; the
//               enemy's: attacks, guards a point, patrols - the AI), the POINT(S), MESSAGE, and a line
//               for a real problem with the button that mends it
//   templates   the authors' trains by name
// The arrow at the bottom left, Esc and the right button go a screen back (from the list they close
// the panel). Every change is one step of the editor's history (edit_commit_arrivals). A map whose
// script cannot run arrivals shows why; an older editor map is converted first (CONVERT: its
// authors' reinforcement becomes arrivals).
#include "patch.h"
#include "editor_mission.h"
#include "editor_rows.h"
#include "editor_text.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

// editor_edit.cpp
void edit_draw_picture(int nvid, int frame, float x, float y, int step);
void edit_commit_arrivals(const UiText& what, const ArrivalsState& before);
void edit_convert_arrivals();
bool edit_reinforcement_marker(int& x, int& y, int& z);
void edit_pick_arrival(int what);   // 1 the entry, 2 the task's point (a patrol's second next)
int edit_entry_state(int arrival);  // 0 on the map, 1 its entry line is there, 2 none yet, -1 no track there
void edit_add_entry_line(int arrival);
bool edit_entry_lights(int arrival, POINT& at);   // the pair of posts on the entry's way in
// editor_panel.cpp
void panel_frame(int picture_vid, const char* title);
void panel_picture(float& x, float& y);
void panel_open_vehicles_for_train(bool factory);
int palette_icon(int nvid);
const char* object_name(int nvid);

namespace {

constexpr int STEP = PanelRows::STEP;
constexpr int VID_PICTURE = 484, VID_ICONS = 944, VID_FRAME = 456;
constexpr float LEFT = -182, LABEL = 88, ROW = PanelRows::ROW, ICON = 41;
constexpr float TOP = -70, BOTTOM = 112;   // the first and the last row under the title, above the button bar
constexpr int WIDTH = 46;                  // characters a row
constexpr int SHOWN_CARS = 6;
constexpr int MAX_CARS = 30;   // the scripts' train array (temp_array of TrainsTactDeclares.lgc, arr_cpp copies 30)
constexpr int LIST_PAGE = 6;   // arrivals a page of the list (two rows each)
constexpr int TEMPLATE_PAGE = 12;

enum class Screen { List, Arrival, Templates };
Screen g_screen = Screen::List;
int g_selected = 0, g_list_page = 0, g_template_page = 0;
int g_car = -1, g_first_car = 0;   // the train's chosen car (-1 none), the first one shown
float g_cw = 8, g_ch = 13;
float g_px = 0, g_py = 0;   // the picture's middle

PanelRows g_rows;   // the choices where the last frame drew them

const char* const ARMY_KEYS[] = {"CtxOurs", "CtxEnemy", "CtxNeutral", "CtxPirates"};
const char* const ARMY_NAMES[] = {"OURS", "ENEMY", "NEUTRAL", "PIRATES"};

std::vector<Arrival>& list() { return mission_arrivals().list; }
bool valid() { return g_selected >= 0 && g_selected < static_cast<int>(list().size()); }

// ---- drawing a row ----

bool hover(float l, float t, float r, float b) { return g_rows.hover(l, t, r, b); }

float put(float x, float y, const std::string& text, bool current = false, std::function<void()> act = nullptr) {
    return g_rows.put(x, y, text, current, std::move(act));
}

float label(float y, const char* key, const char* english) {
    put(g_px + LEFT, y, ui_text(key, english));
    return g_px + LEFT + LABEL;
}

float number(float x, float y, const std::string& shown, std::function<void(int)> by) { return g_rows.number(x, y, shown, std::move(by)); }

std::string seconds(int s) { return ui_printf("%d:%02d", s / 60, s % 60); }

// A text cut to the characters left of a row from x.
std::string fit(const std::string& text, float x) {
    const int room = static_cast<int>((g_px + LEFT + WIDTH * g_cw - x) / g_cw);
    return room <= 0 ? std::string() : static_cast<int>(text.size()) <= room ? text : text.substr(0, room);
}

// ---- changes ----

// One change of the chosen arrival, one step of the history.
void change(const UiText& what, const std::function<void(Arrival&)>& f) {
    if (!valid()) return;
    const ArrivalsState before = mission_arrivals();
    f(list()[g_selected]);
    if (!(mission_arrivals() == before)) edit_commit_arrivals(what, before);
}

void change_all(const UiText& what, const std::function<void(ArrivalsState&)>& f) {
    const ArrivalsState before = mission_arrivals();
    f(mission_arrivals());
    if (!(mission_arrivals() == before)) edit_commit_arrivals(what, before);
}

// The tasks an army's train can get: the enemy's the AI does, the others' stay, drive or patrol.
std::vector<int> tasks_of(int army) {
    return army == 1 ? std::vector<int>{TASK_ATTACK, TASK_GUARD, TASK_PATROL} : std::vector<int>{TASK_NONE, TASK_DRIVE, TASK_PATROL};
}

void set_army(Arrival& a, int army) {
    a.army = army;
    const std::vector<int> tasks = tasks_of(army);
    if (std::find(tasks.begin(), tasks.end(), a.task) == tasks.end()) {
        if (a.task == TASK_DRIVE) a.task = TASK_GUARD;        // to a point either way
        else if (a.task == TASK_GUARD) a.task = TASK_DRIVE;
        else a.task = tasks[army == 1 ? 0 : 1];               // the enemy attacks, the others drive
    }
    if (army > 1) a.message = false;
}

bool locomotive(int nvid) { return nvid == 5 || nvid == 10 || nvid == 97; }
bool needs_point(const Arrival& a) { return a.task == TASK_GUARD || a.task == TASK_PATROL || a.task == TASK_DRIVE; }

// ---- WHEN and REPEAT: the arrival's time and condition as one choice ----

enum class When { AtStart, After, NoTrain, Pays, Other };

When when_of(const Arrival& a) {
    if (a.cond == IF_NONE) return a.first > 0 ? When::After : When::AtStart;
    if (a.cond == IF_NO_TRAIN && !a.earlier) return When::NoTrain;
    if (a.cond == IF_MONEY && !a.earlier) return When::Pays;
    return When::Other;   // another condition of an older map: shown as it is
}

void set_when(Arrival& a, When w) {
    if (w == When::AtStart || w == When::After) {
        a.cond = IF_NONE;
        a.earlier = false;
        if (w == When::AtStart) a.first = 0;
        else if (a.first <= 0) a.first = 30;
    } else {
        if (w == When::Pays && a.cond != IF_MONEY) a.cond_n = 300;
        a.cond = w == When::NoTrain ? IF_NO_TRAIN : IF_MONEY;
        a.earlier = false;
    }
}

// The entries of the map: the arrivals' rail points, numbered as they first come.
struct Entry {
    int x, y, z;
};
std::vector<Entry> entries() {
    std::vector<Entry> out;
    for (const Arrival& a : list())
        if (a.entry && std::none_of(out.begin(), out.end(), [&](const Entry& e) { return e.x == a.x && e.y == a.y; }))
            out.push_back({a.x, a.y, a.z});
    return out;
}

int entry_number(const Arrival& a) {
    const std::vector<Entry> e = entries();
    for (size_t i = 0; i < e.size(); ++i)
        if (a.entry && e[i].x == a.x && e[i].y == a.y) return static_cast<int>(i) + 1;
    return 0;
}

// The words of an arrival for the list: who and the train, then the entry, when and how often.
std::string train_words(const Arrival& a) {
    if (a.cars.empty()) return "";
    const char* name = object_name(a.cars[0]);
    const std::string first = name ? name : std::to_string(a.cars[0]);
    if (a.cars.size() == 1) return first;
    return ui_printf(ui_format("ArrListCars", "%s + CARS: %d"), first.c_str(), static_cast<int>(a.cars.size()) - 1);
}

std::string when_words(const Arrival& a) {
    std::string w;
    switch (when_of(a)) {
    case When::AtStart: w = ui_text("ArrAtStart", "AT THE START"); break;
    case When::After: w = ui_printf(ui_format("ArrListAfter", "AFTER %s"), seconds(a.first).c_str()); break;
    case When::NoTrain: w = ui_text("ArrListNoTrain", "WHEN THE PLAYER HAS NO TRAIN"); break;
    case When::Pays: w = ui_printf(ui_format("ArrListPays", "WHEN THE PLAYER PAYS %d"), a.cond_n); break;
    case When::Other: w = ui_text(mission_condition(a.cond).key, mission_condition(a.cond).english); break;
    }
    if (a.times == 1) return w + ", " + ui_text("ArrOnce", "ONCE");
    const std::string every = seconds(a.every);
    if (a.times == 0) return w + ", " + ui_printf(ui_format("ArrListEndless", "WITHOUT END EVERY %s"), every.c_str());
    return w + ", " + ui_printf(ui_format("ArrListTimes", "%d TIMES EVERY %s"), a.times, every.c_str());
}

// ---- the problem of an arrival, with the button that mends it ----

struct Problem {
    std::string text, button;
    std::function<void()> act;
};

bool problem_of(const Arrival& a, int i, Problem& p) {
    const int state = a.entry ? edit_entry_state(i) : -1;
    if (!a.entry) {
        p = {ui_text("ArrProbEntry", "! NO ENTRY YET"), ui_text("ArrFixPickEntry", "PICK THE ENTRY"), [] { edit_pick_arrival(1); }};
    } else if (state < 0) {
        p = {ui_text("ArrProbTrack", "! NO TRACK AT THE ENTRY"), ui_text("ArrFixPickEntry", "PICK THE ENTRY"),
             [] { edit_pick_arrival(1); }};
    } else if (state == 2) {
        POINT at;
        if (edit_entry_lights(i, at))   // lights on the way in: the line goes to them
            p = {ui_text("ArrProbLine", "! NO ENTRY LINE"), ui_text("ArrFixLineLights", "LINE TO THE LIGHTS"), [] { edit_add_entry_line(g_selected); }};
        else
            p = {ui_text("ArrProbLights", "! NO LIGHTS AT THE ENTRY"), ui_text("ArrFixLights", "PUT LIGHTS"), [] { edit_add_entry_line(g_selected); }};
    } else if (std::none_of(a.cars.begin(), a.cars.end(), locomotive)) {   // anywhere in the train: it drives
        p = {ui_text("ArrProbLoco", "! NO LOCOMOTIVE IN THE TRAIN"), ui_text("ArrFixLoco", "ADD ONE"), [] {
                 change({"OpArrivalCar", "arrival train"}, [](Arrival& r) {
                     if (r.cars.size() < MAX_CARS) r.cars.push_back(5);
                 });
             }};
    } else if (needs_point(a) && !a.tx && !a.ty) {
        p = {ui_text("ArrProbPoint", "! NO POINT YET"), ui_text("ArrFixPick", "PICK IT"), [] { edit_pick_arrival(2); }};
    } else if (a.task == TASK_PATROL && !a.tx2 && !a.ty2) {
        p = {ui_text("ArrProbSecond", "! NO OTHER END YET"), ui_text("ArrFixPick", "PICK IT"), [] { edit_pick_arrival(2); }};
    } else if (a.army == 1 && a.times == 0 && when_of(a) != When::NoTrain && when_of(a) != When::Pays) {
        p = {ui_text("ArrProbEndless", "! WITHOUT END: THE MAP IS NOT WON"), ui_text("ArrFixTen", "10 TIMES"),
             [] { change({"OpArrivalTimes", "arrival waves"}, [](Arrival& r) { r.times = 10; }); }};
    } else if (a.task == TASK_NONE &&
               (a.times != 1 || std::count_if(list().begin(), list().end(), [&](const Arrival& o) { return o.entry && o.x == a.x && o.y == a.y; }) > 1)) {
        p = {ui_text("ArrProbStays", "! IT STAYS: THE NEXT ONES WAIT"), ui_text("ArrFixDrive", "DRIVE ON"), [] {
                 change({"OpArrivalTask", "arrival task"}, [](Arrival& r) { r.task = TASK_DRIVE; });
                 edit_pick_arrival(2);
             }};
    } else {
        return false;
    }
    return true;
}

// The limits of the game's AI, said once one is reached (TrainsTact.lgc).
std::string ai_limit() {
    int patrols, posts, start, queue;
    mission_ai_use(patrols, posts, start, queue);
    int enemy = start + queue;
    bool endless = false;
    for (const Arrival& r : list())
        if (r.army == 1) {
            enemy += r.times;
            endless = endless || r.times == 0;
        }
    if (patrols > 10 || posts > 10) return ui_text("ArrWarnAi", "! OVER 10 PATROLS OR POSTS: THE REST ATTACK");
    if (enemy > 16 || endless) return ui_text("ArrWarnAi16", "! THE AI LEADS 16 TRAINS AT ONCE: MORE WAIT");
    return "";
}

// ---- the list ----

void open_arrival(int i) {
    g_selected = i;
    g_car = -1;
    g_first_car = 0;
    g_screen = Screen::Arrival;
}

void draw_list_screen() {
    const int n = static_cast<int>(list().size());
    const int pages = (std::max)(1, (n + LIST_PAGE - 1) / LIST_PAGE);
    g_list_page = (std::max)(0, (std::min)(g_list_page, pages - 1));
    float y = g_py + TOP;
    if (n == 0) put(g_px + LEFT, y, ui_text("ArrListEmpty", "NO ARRIVALS YET: + NEW MAKES ONE"));
    for (int i = g_list_page * LIST_PAGE; i < n && i < (g_list_page + 1) * LIST_PAGE; ++i) {
        const Arrival& a = list()[i];
        const bool chosen = i == g_selected;
        const float x = g_px + LEFT;
        std::string head = std::to_string(i + 1) + ". " + ui_text(ARMY_KEYS[a.army & 3], ARMY_NAMES[a.army & 3]) + ", ";
        head += a.entry ? ui_printf(ui_format("ArrListEntry", "ENTRY %d"), entry_number(a)) : ui_text("ArrListNoEntry", "NO ENTRY");
        head += ": " + train_words(a);
        // the two rows are one choice: lit together under the mouse
        const bool over = hover(x - 2, y - ROW / 2, x + WIDTH * g_cw, y + ROW * 1.5f);
        put(x, y, fit(head, x), chosen || over);
        put(x + 3 * g_cw, y + ROW, fit(when_words(a), x + 3 * g_cw), chosen || over);
        g_rows.area(x - 2, y - ROW / 2, x + WIDTH * g_cw, y + ROW * 1.5f, [i] { open_arrival(i); });
        y += 2 * ROW;
    }
    // the authors' reinforcement goes on beside the arrivals; the AI's limits
    if (mission_arrivals().waves > 0) {
        int ex, ey, ez;
        if (edit_reinforcement_marker(ex, ey, ez))
            put(g_px + LEFT, g_py + BOTTOM - ROW, ui_text("ArrLegacyConvert", "+ THE AUTHORS' REINFORCEMENT: CONVERT"), false,
                [] { edit_convert_arrivals(); });
    } else {
        const std::string limit = ai_limit();
        if (!limit.empty()) put(g_px + LEFT, g_py + BOTTOM - ROW, limit);
    }
    // + NEW, COPY n, DELETE n (the arrival opened last), the pages
    float x = put(g_px + LEFT, g_py + BOTTOM, ui_text("ArrNew", "+ NEW"), false, [] {
        change_all({"OpArrivalAdd", "add arrival"}, [](ArrivalsState& s) {
            Arrival a;
            a.cars = {5, 20, 20};
            if (!s.list.empty()) {   // at the entry of the chosen one
                const Arrival& p = s.list[(std::max)(0, (std::min)(g_selected, static_cast<int>(s.list.size()) - 1))];
                a.x = p.x, a.y = p.y, a.z = p.z, a.entry = p.entry;
            }
            s.list.push_back(a);
            open_arrival(static_cast<int>(s.list.size()) - 1);
        });
    });
    if (valid()) {
        x = put(x + g_cw, g_py + BOTTOM, ui_printf(ui_format("ArrCopyN", "COPY %d"), g_selected + 1), false, [] {
            change_all({"OpArrivalCopy", "copy arrival"}, [](ArrivalsState& s) {
                s.list.insert(s.list.begin() + g_selected + 1, s.list[g_selected]);
                ++g_selected;
            });
        });
        x = put(x + g_cw, g_py + BOTTOM, ui_printf(ui_format("ArrDeleteN", "DELETE %d"), g_selected + 1), false, [] {
            change_all({"OpArrivalDelete", "delete arrival"}, [](ArrivalsState& s) {
                s.list.erase(s.list.begin() + g_selected);
                g_selected = (std::max)(0, (std::min)(g_selected, static_cast<int>(s.list.size()) - 1));
            });
        });
    }
    if (pages > 1) {
        x = put(x + g_cw, g_py + BOTTOM, "<", false, [] { --g_list_page; });
        x = put(x, g_py + BOTTOM, ui_printf("%d/%d", g_list_page + 1, pages));
        put(x, g_py + BOTTOM, ">", false, [pages] { g_list_page = (std::min)(pages - 1, g_list_page + 1); });
    }
}

// ---- an arrival ----

float draw_who(const Arrival& a, float y) {
    float x = label(y, "ArrWho", "WHO:");
    for (int army = 0; army < 4; ++army)
        x = put(x, y, ui_text(ARMY_KEYS[army], ARMY_NAMES[army]), a.army == army,
                [army] { change({"OpArrivalArmy", "arrival army"}, [army](Arrival& r) { set_army(r, army); }); });
    return y + ROW;
}

// The cars as icons over three rows, in the train's order; their label and, in the label's column,
// + CAR (at the end) and TEMPLATE... - or, for the car chosen with a click, < > to move it and REMOVE.
float draw_cars(const Arrival& a, float y) {
    const int n = static_cast<int>(a.cars.size());
    if (g_car >= n) g_car = -1;
    put(g_px + LEFT, y, ui_text("ArrCars", "CARS:"));
    if (g_car < 0) {
        if (n < MAX_CARS) put(g_px + LEFT, y + ROW, ui_text("ArrAddCar", "+ CAR"), false, [] { panel_open_vehicles_for_train(false); });
        put(g_px + LEFT, y + 2 * ROW, ui_text("ArrTemplates", "TEMPLATE..."), false, [] {
            g_screen = Screen::Templates;
            g_template_page = 0;
        });
    } else {
        auto move = [](int by) {
            return [by] {
                change({"OpArrivalCar", "arrival train"}, [by](Arrival& r) {
                    const int to = g_car + by;
                    if (to < 0 || to >= static_cast<int>(r.cars.size())) return;
                    std::swap(r.cars[g_car], r.cars[to]);
                    g_car = to;
                });
            };
        };
        const float mx = put(g_px + LEFT, y + ROW, "<", false, move(-1));
        put(mx + g_cw, y + ROW, ">", false, move(1));
        if (n > 1)
            put(g_px + LEFT, y + 2 * ROW, ui_text("ArrRemoveCar", "REMOVE"), false, [] {
                change({"OpArrivalCar", "arrival train"}, [](Arrival& r) {
                    if (g_car < static_cast<int>(r.cars.size()) && r.cars.size() > 1) r.cars.erase(r.cars.begin() + g_car);
                });
                g_car = -1;
            });
    }
    // the cars shown: SHOWN_CARS from g_first_car, the chosen one among them
    if (g_car >= 0 && g_car < g_first_car) g_first_car = g_car;
    if (g_car >= g_first_car + SHOWN_CARS) g_first_car = g_car - SHOWN_CARS + 1;
    g_first_car = (std::max)(0, (std::min)(g_first_car, n - SHOWN_CARS));
    float x = g_px + LEFT + LABEL;
    const float cy = y + ROW;   // the icons' middle
    for (int i = g_first_car; i < n && i < g_first_car + SHOWN_CARS; ++i) {
        const float cx = x + ICON / 2;
        const int frame = palette_icon(a.cars[i]);
        if (frame >= 0) edit_draw_picture(VID_ICONS, frame, cx, cy, STEP - 1);
        else put(x + 4, cy, std::to_string(a.cars[i]));
        if (i == g_car || hover(x, cy - ICON / 2, x + ICON, cy + ICON / 2)) edit_draw_picture(VID_FRAME, 0, cx, cy, STEP);
        g_rows.area(x, cy - ICON / 2, x + ICON, cy + ICON / 2, [i] { g_car = g_car == i ? -1 : i; });   // chosen, or not again
        x += ICON;
    }
    if (n > SHOWN_CARS) {   // the rest of a long train: scrolled
        const float sx = put(x + 2, cy, "<", false, [] { g_first_car = (std::max)(0, g_first_car - 1); });
        put(sx, cy, ">", false, [] { ++g_first_car; });
    }
    return y + 3 * ROW;
}

float draw_entry(const Arrival& a, float y) {
    float x = label(y, "ArrEntryLabel", "ENTRY:");
    if (a.entry) x = put(x, y, ui_printf(ui_format("ArrEntry", "%d (%d,%d)"), entry_number(a), a.x, a.y));
    x = put(x, y, ui_text("ArrPick", "PICK"), false, [] { edit_pick_arrival(1); });
    // the entry's line and lights: where the lights stand (the problem row says when there are none)
    POINT at;
    if (a.entry && edit_entry_state(g_selected) == 1 && edit_entry_lights(g_selected, at)) {
        const std::string lights = ui_printf(ui_format("ArrLights", "LIGHTS %d,%d"), at.x, at.y);
        if (fit(lights, x) == lights) put(x, y, lights);
    }
    return y + ROW;
}

float draw_when(const Arrival& a, float y) {
    const When w = when_of(a);
    auto pick = [](When to) {
        return [to] { change({"OpArrivalWhen", "arrival time"}, [to](Arrival& r) { set_when(r, to); }); };
    };
    float x = label(y, "ArrWhen", "WHEN:");
    x = put(x, y, ui_text("ArrAtStart", "AT THE START"), w == When::AtStart, pick(When::AtStart));
    x = put(x + g_cw, y, ui_text("ArrAfter", "AFTER"), w == When::After, pick(When::After));
    if (w == When::After || w == When::AtStart)
        number(x, y, seconds(a.first), [](int by) {
            change({"OpArrivalWhen", "arrival time"}, [by](Arrival& r) {
                set_when(r, When::After);
                r.first = (std::max)(10, r.first + by * 10);
            });
        });
    y += ROW;
    const float vx = g_px + LEFT + LABEL;
    put(vx, y, ui_text("ArrWhenNoTrain", "WHEN THE PLAYER HAS NO TRAIN"), w == When::NoTrain, pick(When::NoTrain));
    y += ROW;
    x = put(vx, y, ui_text("ArrWhenPays", "WHEN THE PLAYER PAYS"), w == When::Pays, pick(When::Pays));
    if (w == When::Pays)
        number(x, y, std::to_string(a.cond_n), [](int by) {
            change({"OpArrivalWhen", "arrival time"}, [by](Arrival& r) { r.cond_n = (std::max)(50, r.cond_n + by * 50); });
        });
    y += ROW;
    if (w == When::Other) {   // a condition of an older map, as it is
        const Condition& c = mission_condition(a.cond);   // the conditions' one list (editor_mission.h)
        std::string text = ui_text(c.key, c.english);
        if (c.number) text += " " + std::to_string(a.cond_n);
        put(vx, y, fit(text, vx), true);
        y += ROW;
    }
    if (w != When::AtStart && w != When::After) {
        x = put(vx, y, ui_text("ArrNotBefore", "NOT BEFORE"));
        number(x, y, seconds(a.first), [](int by) {
            change({"OpArrivalWhen", "arrival time"}, [by](Arrival& r) { r.first = (std::max)(0, r.first + by * 10); });
        });
        y += ROW;
    }
    return y;
}

float draw_repeat(const Arrival& a, float y) {
    float x = label(y, "ArrRepeat", "REPEAT:");
    x = put(x, y, ui_text("ArrOnce", "ONCE"), a.times == 1,
            [] { change({"OpArrivalTimes", "arrival waves"}, [](Arrival& r) { r.times = 1; }); });
    auto repeat = [](Arrival& r) {
        if (r.every <= 0) r.every = 60;
    };
    x = number(x + g_cw, y, std::to_string(a.times >= 2 ? a.times : 3), [repeat](int by) {
        change({"OpArrivalTimes", "arrival waves"}, [by, repeat](Arrival& r) {
            r.times = (std::max)(2, (r.times >= 2 ? r.times : 3) + by);
            repeat(r);
        });
    });
    x = put(x, y, ui_text("ArrTimesWord", "TIMES"), a.times >= 2, [repeat] {
        change({"OpArrivalTimes", "arrival waves"}, [repeat](Arrival& r) {
            if (r.times < 2) r.times = 3;
            repeat(r);
        });
    });
    put(x + g_cw, y, ui_text("ArrEndlessChoice", "WITHOUT END"), a.times == 0, [repeat] {
        change({"OpArrivalTimes", "arrival waves"}, [repeat](Arrival& r) {
            r.times = 0;
            repeat(r);
        });
    });
    y += ROW;
    if (a.times != 1) {
        x = put(g_px + LEFT + LABEL, y, ui_text("ArrEveryWord", "EVERY"));
        number(x, y, seconds(a.every), [](int by) {
            change({"OpArrivalTime", "arrival time"}, [by](Arrival& r) { r.every = (std::max)(10, r.every + by * 10); });
        });
        y += ROW;
    }
    return y;
}

float draw_then(const Arrival& a, float y) {
    static const char* const KEYS[] = {"ArrTaskNone", "ArrTaskAttack", "ArrTaskForts", "ArrTaskGuard", "ArrTaskPatrol", "ArrTaskDrive"};
    static const char* const NAMES[] = {"STAYS", "ATTACKS", "THE FORTS", "GUARDS A POINT", "PATROLS", "DRIVES TO A POINT"};
    float x = label(y, "ArrThen", "THEN:");
    for (int t : tasks_of(a.army))
        x = put(x, y, ui_text(KEYS[t], NAMES[t]), a.task == t,
                [t] { change({"OpArrivalTask", "arrival task"}, [t](Arrival& r) { r.task = t; }); });
    y += ROW;
    if (needs_point(a)) {
        x = label(y, a.task == TASK_PATROL ? "ArrPoints" : "ArrPoint", a.task == TASK_PATROL ? "POINTS:" : "POINT:");
        if (a.tx || a.ty) x = put(x, y, ui_printf("%d,%d", a.tx, a.ty));
        if (a.task == TASK_PATROL && (a.tx2 || a.ty2)) x = put(x, y, ui_printf("- %d,%d", a.tx2, a.ty2));
        put(x, y, ui_text("ArrPick", "PICK"), false, [] { edit_pick_arrival(2); });   // a patrol's two ends one after the other
        y += ROW;
    }
    return y;
}

float draw_message(const Arrival& a, float y) {
    if (a.army > 1) return y;   // only ours and the enemy's have one
    float x = label(y, "ArrMessage", "MESSAGE:");
    x = put(x, y, ui_text("WordYes", "YES"), a.message,
            [] { change({"OpArrivalMessage", "arrival message"}, [](Arrival& r) { r.message = true; }); });
    put(x, y, ui_text("WordNo", "NO"), !a.message,
        [] { change({"OpArrivalMessage", "arrival message"}, [](Arrival& r) { r.message = false; }); });
    return y + ROW;
}

void draw_arrival_screen() {
    if (!valid()) {
        g_screen = Screen::List;
        return;
    }
    const Arrival a = list()[g_selected];
    float y = g_py + TOP;
    y = draw_who(a, y);
    y = draw_cars(a, y);
    y = draw_entry(a, y);
    y = draw_when(a, y);
    y = draw_repeat(a, y);
    y = draw_then(a, y);
    Problem p;
    const bool problem = problem_of(a, g_selected, p);
    // the last row is the problem's when there is one (the message is set the same, shown again after)
    if (!problem || y < g_py + BOTTOM) y = draw_message(a, y);
    if (problem) {
        const float py = (std::min)(y, g_py + BOTTOM);
        const float x = put(g_px + LEFT, py, p.text);
        put(x, py, fit(p.button, x), false, p.act);
    }
}

// ---- the templates ----

void draw_templates_screen() {
    const std::vector<AuthorTrain>& t = mission_author_trains();
    const int pages = (std::max)(1, (static_cast<int>(t.size()) + TEMPLATE_PAGE - 1) / TEMPLATE_PAGE);
    g_template_page = (std::max)(0, (std::min)(g_template_page, pages - 1));
    float y = g_py + TOP;
    put(g_px + LEFT, y, ui_text("ArrTemplatesHead", "THE AUTHORS' TRAINS (A CLICK TAKES ONE):"));
    y += ROW;
    for (int i = g_template_page * TEMPLATE_PAGE; i < static_cast<int>(t.size()) && i < (g_template_page + 1) * TEMPLATE_PAGE; ++i) {
        put(g_px + LEFT + 2 * g_cw, y, fit(mission_author_train_name(t[i]), g_px + LEFT + 2 * g_cw), false, [i] {
            const std::vector<int> cars = mission_author_trains()[i].cars;
            change({"OpArrivalTemplate", "arrival template"}, [cars](Arrival& r) { r.cars = cars; });
            g_screen = Screen::Arrival;
        });
        y += ROW;
    }
    if (pages > 1) {
        float x = put(g_px + LEFT, g_py + BOTTOM, "<", false, [] { --g_template_page; });
        x = put(x, g_py + BOTTOM, ui_printf("%d/%d", g_template_page + 1, pages));
        put(x, g_py + BOTTOM, ">", false, [pages] { g_template_page = (std::min)(pages - 1, g_template_page + 1); });
    }
}

// A map whose script does not run arrivals: why, or the authors' reinforcement and CONVERT.
void draw_legacy() {
    const std::string refusal = mission_arrivals_refusal();
    if (!refusal.empty()) {
        put(g_px + LEFT, g_py + TOP, refusal.substr(0, WIDTH));
        if (refusal.size() > WIDTH) put(g_px + LEFT, g_py + TOP + ROW, refusal.substr(WIDTH));
        return;
    }
    float y = g_py + TOP;
    put(g_px + LEFT, y, ui_text("ArrLegacy1", "THIS MAP HAS THE AUTHORS' REINFORCEMENT:"));
    y += ROW;
    std::string train;
    for (int c : mission_reinforcement_train()) train += (train.empty() ? "" : ",") + std::to_string(c);
    put(g_px + LEFT, y, ui_printf(ui_format("ArrLegacy2", "%d WAVES OF %s AT ITS POINT LOCOMOTIVE"), mission_reinforcement_waves(),
                                  train.c_str()));
    y += 2 * ROW;
    put(g_px + LEFT, y, ui_text("ArrConvert", "CONVERT TO ARRIVALS"), false, [] { edit_convert_arrivals(); });
}

}  // namespace

// editor_panel.cpp draws the panel's frame, then this.
void arrivals_draw(POINT m) {
    g_rows.begin(m);
    g_cw = g_rows.cw;
    g_ch = g_rows.ch;
    ArrivalsState& s = mission_arrivals();
    if (s.on && g_screen != Screen::List && !valid()) g_screen = Screen::List;
    const std::string title = s.on && g_screen == Screen::Arrival ? ui_printf(ui_format("ArrEditHeader", "ARRIVAL %d"), g_selected + 1)
                                                                  : std::string(ui_text("ArrHeader", "TRAIN ARRIVALS"));
    panel_frame(VID_PICTURE, title.c_str());
    panel_picture(g_px, g_py);
    if (!s.on) {
        draw_legacy();
        return;
    }
    if (g_screen == Screen::Arrival) draw_arrival_screen();
    else if (g_screen == Screen::Templates) draw_templates_screen();
    else draw_list_screen();
}

// A click in the panel (not on its close button): true when it hit a choice.
bool arrivals_click(POINT m) { return g_rows.click(m); }

// A screen back: an arrival or the templates -> the list / the arrival; false at the list.
bool arrivals_has_back() { return mission_arrivals().on && g_screen != Screen::List; }
bool arrivals_back() {
    if (!arrivals_has_back()) return false;
    g_screen = g_screen == Screen::Templates ? Screen::Arrival : Screen::List;
    return true;
}

// The Vehicles palette picked a car for the chosen arrival's train.
void arrivals_add_car(int nvid) {
    change({"OpArrivalCar", "arrival train"}, [nvid](Arrival& r) {
        if (r.cars.size() < MAX_CARS) r.cars.push_back(nvid);
    });
}

int arrivals_selected() { return g_selected; }
// An arrival chosen on the map: the panel opens on it.
void arrivals_select(int i) {
    g_selected = i;
    g_car = -1;
    g_screen = Screen::Arrival;
}
void arrivals_reset() {
    g_screen = Screen::List;
    g_selected = 0;
    g_list_page = 0;
}
