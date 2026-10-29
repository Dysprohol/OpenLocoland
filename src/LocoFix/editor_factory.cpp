// A factory's panel (editor mode): what a factory (VID 102) builds (DEVELOPERS.md "Editor mode",
// a factory's plan; the data: editor_mission.h "factories", the script: editor_mission.cpp PlanTact).
//
// The factory's object panel (the right button) and Mission settings' Plan... button open it - one of
// editor_panel.cpp's panels, in the game's menu look and the Arrivals panel's way (editor_arrivals.cpp):
// a list screen, one screen per entry with one thing a row, a problem line with the button that
// mends it. What it shows follows what the game does with the factories:
//  - the enemy's: the enemy's plan. Its AI (army 1 only, ai_events.lgc DepoFree) builds the authors'
//    queue (level_queue_*) in its first free factory and ignores the build list; in a map that runs
//    arrivals the runner puts each entry into that queue when it is due (at a time, after the
//    previous entry's train, while a condition holds; once, n times or without end), else each
//    entry goes into the queue once at its time. Beside it the AI's knobs (AI...), a line each;
//  - ours, the neutrals' and the pirates': two tabs - the build list (the game's 14 factory units as
//    icons, a click ticks or unticks one; the ticked ones keep the order of the game's list and so
//    their keys, F1-F12, [ and ], as Player_SetPanel numbers them; for the unit chosen last: from when
//    it is in the list, ACT_ADD_ITEM, and the player's limit of it, AddUnitLimit) and the trains the
//    runner orders this factory to build, car by car, paid by its army (the build list does not
//    limit them) - both need a map that runs arrivals. A neutral or pirate factory builds nothing by
//    itself, and only a neutral one is captured.
// An entry's rows: TRAIN (its name - an authors' train or its own - and the cars: a click chooses a
// car, < > move it, REMOVE takes it away; + CAR the Vehicles palette, TEMPLATE... the authors'
// trains), WHEN, REPEAT, THEN (the enemy's: attacks, guards a point, patrols - the AI's; the others':
// stays, drives to a point, patrols) and its point(s). The conditions are editor_mission.h's one list.
// The arrow at the bottom left, Esc and the right button go a screen back. Every change is a step
// of the editor's history.
#include "patch.h"
#include "editor_mission.h"
#include "editor_rows.h"
#include "editor_text.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

// editor_edit.cpp
void edit_draw_picture(int nvid, int frame, float x, float y, int step);
void edit_fill(float l, float t, float r, float b, uint32_t argb);
int edit_object_army(void* o);
bool edit_object_gone(void* o);
void edit_object_place(void* o, int& x, int& y);
std::vector<int> edit_factory_items(void* o);
void edit_factory_change(void* factory, const std::vector<int>& items, const FactoryState& before, const UiText& what);
void edit_commit_factories(const UiText& what, const FactoryState& before);
void edit_pick_queue_point(void* factory, int train);
// editor_panel.cpp
void panel_frame(int picture_vid, const char* title);
void panel_picture(float& x, float& y);
void panel_open_vehicles_for_train(bool factory);
void panel_open_arrivals();
int palette_icon(int nvid);
const char* object_name(int nvid);
// depot_rows.cpp
int unit_price(int nvid);

namespace {

constexpr int STEP = PanelRows::STEP;
constexpr int VID_PICTURE = 484, VID_ICONS = 944, VID_FRAME = 456;
constexpr float LEFT = -182, LABEL = 88, ROW = PanelRows::ROW, ICON = 41;
constexpr float TOP = -70, BOTTOM = 98;    // the first and the last row under the title, clear of the button bar
constexpr int WIDTH = 46;                  // characters a row
constexpr int UNITS[] = {5, 10, 20, 25, 80, 85, 45, 30, 35, 82, 97, 90, 75, 62};   // the game's list, F1 ... ]
const char* const KEYS[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "[", "]"};
constexpr int UNIT_COUNT = sizeof UNITS / sizeof UNITS[0];
constexpr int SHOWN_CARS = 6;
constexpr int MAX_CARS = 16;   // the AI builds and takes a train of ARR_LENGTH (16) cars (ai_events.lgc, TrainsTactFunctions.lgc)
constexpr int LIST_PAGE = 5;   // entries a page of the list (two rows each)
constexpr int TEMPLATE_PAGE = 12;

// What the panel offers - only what was seen working in play (DEVELOPERS.md, the factories).
constexpr bool LATER_OFFERED = true;   // a unit in the list from a time on
constexpr bool LIMIT_OFFERED = true;   // the player's unit limits

enum class Screen { Builds, Plan, Entry, Templates, Ai };
Screen g_screen = Screen::Plan;
void* g_factory = nullptr;
int g_chosen = 0;                  // the build list's unit chosen last
int g_entry = -1;                  // the plan entry opened (its index in mission_factories().queue)
int g_list_page = 0, g_template_page = 0, g_ai_page = 0;
int g_car = -1, g_first_car = 0;   // the train's chosen car (-1 none), the first one shown
PanelRows g_rows;
float g_px = 0, g_py = 0, g_cw = 8;

const char* const ARMY_TITLES[][2] = {{"FactoryOurs", "FACTORY: OURS"}, {"FactoryEnemy", "FACTORY: ENEMY"},
                                      {"FactoryNeutral", "FACTORY: NEUTRAL"}, {"FactoryPirates", "FACTORY: PIRATES"}};

float put(float x, float y, const std::string& text, bool current = false, std::function<void()> act = nullptr) {
    return g_rows.put(x, y, text, current, std::move(act));
}

float label(float y, const char* key, const char* english) {
    put(g_px + LEFT, y, ui_text(key, english));
    return g_px + LEFT + LABEL;
}

float number(float x, float y, const std::string& shown, std::function<void(int)> by) { return g_rows.number(x, y, shown, std::move(by)); }

std::string seconds(int s) { return ui_printf("%d:%02d", s / 60, s % 60); }
const char* unit_name(int nvid) {
    const char* n = object_name(nvid);
    return n ? n : "?";
}

// A text cut to the characters left of a row from x.
std::string fit(const std::string& text, float x) {
    const int room = static_cast<int>((g_px + LEFT + WIDTH * g_cw - x) / g_cw);
    return room <= 0 ? std::string() : static_cast<int>(text.size()) <= room ? text : text.substr(0, room);
}

int army() { return (std::max)(0, (std::min)(3, edit_object_army(g_factory))); }
bool enemy() { return army() == 1; }
bool runs() { return mission_arrivals().on; }   // the map's script has the runner (PlanTact)

// ---- the build list (ours, the neutrals' and the pirates') ----

// The unit's later time in this factory's list, -1 when it has none.
int later_of(int nvid) {
    for (const UnitUnlock& u : mission_factories().unlocks)
        if (u.factory == g_factory && u.nvid == nvid) return u.time;
    return -1;
}

int limit_of(int nvid) {
    for (const UnitLimit& l : mission_factories().limits)
        if (l.army == 0 && l.nvid == nvid) return l.max;
    return 0;
}

// The list in the game's order with `nvid` in or out, and its later time set (0: at the start).
void set_unit(int nvid, bool in, int later, const UiText& what) {
    const FactoryState before = mission_factories();
    FactoryState& f = mission_factories();
    std::vector<int> items = edit_factory_items(g_factory);
    int x, y;
    edit_object_place(g_factory, x, y);
    f.unlocks.erase(std::remove_if(f.unlocks.begin(), f.unlocks.end(),
                                   [&](const UnitUnlock& u) { return u.factory == g_factory && u.nvid == nvid; }),
                    f.unlocks.end());
    items.erase(std::remove(items.begin(), items.end(), nvid), items.end());
    if (in && later > 0) f.unlocks.push_back({x, y, nvid, later, g_factory});
    else if (in) items.push_back(nvid);
    std::vector<int> ordered;   // the game's order (the keys), then any others the map had
    for (int u : UNITS)
        if (std::find(items.begin(), items.end(), u) != items.end()) ordered.push_back(u);
    for (int u : items)
        if (std::find(std::begin(UNITS), std::end(UNITS), u) == std::end(UNITS)) ordered.push_back(u);
    edit_factory_change(g_factory, ordered, before, what);
}

void set_limit(int nvid, int max) {
    const FactoryState before = mission_factories();
    FactoryState& f = mission_factories();
    f.limits.erase(std::remove_if(f.limits.begin(), f.limits.end(), [&](const UnitLimit& l) { return l.army == 0 && l.nvid == nvid; }),
                   f.limits.end());
    if (max > 0) f.limits.push_back({0, nvid, max});
    if (!(f == before)) edit_commit_factories({"OpUnitLimit", "unit limit"}, before);
}

int plan_size();   // below

// The tabs of ours, the neutrals' and the pirates' factory: the build list, the trains of its plan.
void draw_tabs(float y) {
    float x = put(g_px + LEFT, y, ui_text("FactoryTabList", "BUILD LIST"), g_screen == Screen::Builds, [] { g_screen = Screen::Builds; });
    put(x + 2 * g_cw, y, ui_printf(ui_format("FactoryTabTrains", "TRAINS IT BUILDS: %d"), plan_size()), g_screen != Screen::Builds,
        [] { g_screen = Screen::Plan; });
}

void draw_list() {
    const std::vector<int> items = edit_factory_items(g_factory);
    draw_tabs(g_py + TOP);
    int key = 0;
    for (int i = 0; i < UNIT_COUNT; ++i) {
        const int nvid = UNITS[i];
        const float x = g_px + LEFT + (i % 7) * ICON + ICON / 2, y = g_py - 40 + (i / 7) * ICON;
        const bool in = std::find(items.begin(), items.end(), nvid) != items.end();
        const int later = later_of(nvid);
        const int frame = palette_icon(nvid);
        if (frame >= 0) edit_draw_picture(VID_ICONS, frame, x, y, STEP - 1);
        if (!in && later < 0) edit_fill(x - 20, y - 20, x + 20, y + 20, 0xa0000000);   // not built: dim
        if (i == g_chosen || g_rows.hover(x - 20, y - 20, x + 20, y + 20)) edit_draw_picture(VID_FRAME, 0, x, y, STEP);
        if (in) put(x - 18, y + 13, KEYS[(std::min)(key++, UNIT_COUNT - 1)]);
        else if (later >= 0) put(x - 18, y + 13, seconds(later));
        g_rows.area(x - 20, y - 20, x + 20, y + 20, [i, in, later] {
            g_chosen = i;
            set_unit(UNITS[i], !(in || later >= 0), 0, {"OpFactoryList", "factory build list"});
        });
    }
    const int nvid = UNITS[g_chosen];
    const bool in = std::find(items.begin(), items.end(), nvid) != items.end(), later = later_of(nvid) >= 0;
    float x = label(g_py + 34, "FactoryChosen", "CHOSEN:");
    put(x, g_py + 34, fit(ui_printf("%s (VID %d)", unit_name(nvid), nvid), x));
    if (!runs()) {
        put(g_px + LEFT, g_py + 50, ui_text("FactoryNeedsArrivals", "LATER UNITS, LIMITS: IN A MAP WITH ARRIVALS"));
        put(g_px + LEFT, g_py + 64, ui_text("FactoryConvert", "(CONVERT IT IN THE ARRIVALS PANEL)"), false, [] { panel_open_arrivals(); });
    } else {
        if (LATER_OFFERED && (in || later)) {
            x = label(g_py + 50, "FactoryFrom", "IN THE LIST:");
            const int t = (std::max)(0, later_of(nvid));
            number(x, g_py + 50, t ? ui_printf(ui_format("FactoryAfter", "AFTER %s"), seconds(t).c_str()) : ui_text("ArrAtStart", "AT THE START"),
                   [nvid, t](int by) { set_unit(nvid, true, (std::max)(0, t + by * 30), {"OpFactoryLater", "factory unit later"}); });
        }
        if (LIMIT_OFFERED) {
            x = label(g_py + 64, "FactoryLimit", "AT MOST:");
            const int max = limit_of(nvid);
            number(x, g_py + 64, max ? std::to_string(max) : ui_text("FactoryNoLimit", "NO LIMIT"),
                   [nvid, max](int by) { set_limit(nvid, (std::max)(0, max + by)); });
            put(g_px + LEFT, g_py + 78, ui_text("FactoryLimitNote", "(OF THE PLAYER'S ON THE MAP, BUILT OR QUEUED)"));
        }
    }
    if (army() == 2) {
        put(g_px + LEFT, g_py + BOTTOM - ROW, ui_text("FactoryIdle1", "THE AI BUILDS NOTHING HERE: THE PLAYER BUILDS"));
        put(g_px + LEFT, g_py + BOTTOM, ui_text("FactoryIdle2", "THIS LIST ONCE THE FACTORY IS CAPTURED"));
    } else if (army() == 3) {   // only a neutral one is captured (CaptureTact, as the missions' LDepoCapture)
        put(g_px + LEFT, g_py + BOTTOM - ROW, ui_text("FactoryPirate1", "THE AI BUILDS NOTHING HERE, AND THE PLAYER"));
        put(g_px + LEFT, g_py + BOTTOM, ui_text("FactoryPirate2", "CAPTURES ONLY NEUTRAL FACTORIES"));
    }
}

// ---- the plan ----

std::vector<FactoryTrain>& queue() { return mission_factories().queue; }

// The entries of this panel: the enemy's plan in an enemy factory's, the factory's own in the others'.
bool mine(const FactoryTrain& t) {
    if (enemy()) return !t.factory && !(t.fx || t.fy);
    if (t.factory) return t.factory == g_factory;
    int x, y;   // read from a test file: the factory by its place until a save finds it
    edit_object_place(g_factory, x, y);
    return (t.fx || t.fy) && std::abs(t.fx - x) <= 30 && std::abs(t.fy - y) <= 30;
}
std::vector<int> entries() {
    std::vector<int> out;
    for (int i = 0; i < static_cast<int>(queue().size()); ++i)
        if (mine(queue()[i])) out.push_back(i);
    return out;
}
int plan_size() { return static_cast<int>(entries().size()); }
bool valid() { return g_entry >= 0 && g_entry < static_cast<int>(queue().size()) && mine(queue()[g_entry]); }
int number_of(int index) {   // 1, 2 ... in this panel's list
    const std::vector<int> e = entries();
    return static_cast<int>(std::find(e.begin(), e.end(), index) - e.begin()) + 1;
}

// One change of the opened entry, one step of the history.
void change(const UiText& what, const std::function<void(FactoryTrain&)>& f) {
    if (!valid()) return;
    const FactoryState before = mission_factories();
    f(queue()[g_entry]);
    if (!(mission_factories() == before)) edit_commit_factories(what, before);
}

void change_all(const UiText& what, const std::function<void(FactoryState&)>& f) {
    const FactoryState before = mission_factories();
    f(mission_factories());
    if (!(mission_factories() == before)) edit_commit_factories(what, before);
}

bool locomotive(int nvid) { return nvid == 5 || nvid == 10 || nvid == 97; }
bool needs_point(const FactoryTrain& t) { return t.task == TASK_GUARD || t.task == TASK_PATROL || t.task == TASK_DRIVE; }
std::vector<int> tasks_here() {
    return enemy() ? std::vector<int>{TASK_ATTACK, TASK_GUARD, TASK_PATROL} : std::vector<int>{TASK_NONE, TASK_DRIVE, TASK_PATROL};
}

// The train's name: an authors' train with these cars, else the first car and the number of the others.
std::string train_words(const FactoryTrain& t) {
    for (const AuthorTrain& a : mission_author_trains())
        if (a.cars == t.cars) return mission_author_train_name(a);
    if (t.cars.empty()) return "";
    const std::string first = unit_name(t.cars[0]);
    if (t.cars.size() == 1) return first;
    return ui_printf(ui_format("ArrListCars", "%s + CARS: %d"), first.c_str(), static_cast<int>(t.cars.size()) - 1);
}

int price_of(const FactoryTrain& t) {
    int p = 0;
    for (int c : t.cars) p += unit_price(c);
    return p;
}

const char* const TASK_KEYS[] = {"ArrTaskNone", "ArrTaskAttack", "ArrTaskForts", "ArrTaskGuard", "ArrTaskPatrol", "ArrTaskDrive"};
const char* const TASK_NAMES[] = {"STAYS", "ATTACKS", "THE FORTS", "GUARDS A POINT", "PATROLS", "DRIVES TO A POINT"};

std::string condition_words(int code, int n) {
    const Condition& c = mission_condition(code);
    std::string w = ui_text(c.key, c.english);
    if (c.number) w += " " + std::to_string(n);
    return w;
}

std::string when_words(const FactoryTrain& t, int index) {
    std::string w;
    if (t.when == WHEN_AFTER) {
        const std::vector<int> e = entries();
        const auto at = std::find(e.begin(), e.end(), index);
        w = at == e.begin() || at == e.end() ? ui_printf(ui_format("ArrListAfter", "AFTER %s"), seconds(t.time).c_str())
                                             : ui_printf(ui_format("PlanListAfter", "%s AFTER %d"), seconds(t.time).c_str(),
                                                         static_cast<int>(at - e.begin()));
    } else if (t.when == WHEN_IF) {
        w = ui_printf(ui_format("PlanListIf", "WHILE %s"), condition_words(t.cond, t.cond_n).c_str());
    } else {
        w = t.time ? ui_printf(ui_format("ArrListAfter", "AFTER %s"), seconds(t.time).c_str()) : ui_text("ArrAtStart", "AT THE START");
    }
    if (t.times == 1) return w + ", " + ui_text("ArrOnce", "ONCE");
    const std::string every = seconds(t.every);
    if (t.times == 0) return w + ", " + ui_printf(ui_format("ArrListEndless", "WITHOUT END EVERY %s"), every.c_str());
    return w + ", " + ui_printf(ui_format("ArrListTimes", "%d TIMES EVERY %s"), t.times, every.c_str());
}

// ---- the problem of an entry, with the button that mends it ----

struct Problem {
    std::string text, button;
    std::function<void()> act;
};

bool problem_of(const FactoryTrain& t, Problem& p) {
    const std::vector<int> e = entries();
    const bool first = !e.empty() && e.front() == g_entry;
    const int same = enemy() ? 0 : static_cast<int>(e.size());   // this factory's: the next one waits while a train stands at the door
    if (std::none_of(t.cars.begin(), t.cars.end(), locomotive)) {
        p = {ui_text("ArrProbLoco", "! NO LOCOMOTIVE IN THE TRAIN"), ui_text("ArrFixLoco", "ADD ONE"), [] {
                 change({"OpQueueCars", "factory queue train"}, [](FactoryTrain& r) {
                     if (r.cars.size() < MAX_CARS) r.cars.insert(r.cars.begin(), 5);
                 });
             }};
    } else if (needs_point(t) && !t.tx && !t.ty) {
        p = {ui_text("ArrProbPoint", "! NO POINT YET"), ui_text("ArrFixPick", "PICK IT"), [] { edit_pick_queue_point(g_factory, g_entry); }};
    } else if (t.task == TASK_PATROL && !t.tx2 && !t.ty2) {
        p = {ui_text("ArrProbSecond", "! NO OTHER END YET"), ui_text("ArrFixPick", "PICK IT"), [] { edit_pick_queue_point(g_factory, g_entry); }};
    } else if (t.when == WHEN_AFTER && first) {
        p = {ui_text("PlanProbFirst", "! NONE BEFORE IT: FROM THE START"), ui_text("PlanFixAt", "AT A TIME"),
             [] { change({"OpPlanWhen", "factory plan time"}, [](FactoryTrain& r) { r.when = WHEN_AT; }); }};
    } else if (!enemy() && t.task == TASK_NONE && (t.times != 1 || same > 1)) {
        p = {ui_text("ArrProbStays", "! IT STAYS: THE NEXT ONES WAIT"), ui_text("ArrFixDrive", "DRIVE ON"), [] {
                 change({"OpQueueTask", "factory queue task"}, [](FactoryTrain& r) { r.task = TASK_DRIVE; });
                 edit_pick_queue_point(g_factory, g_entry);
             }};
    } else {
        return false;
    }
    return true;
}

// The limits of the game's AI, said once one is reached (TrainsTact.lgc).
std::string ai_limit() {
    int patrols, posts, start, queued;
    mission_ai_use(patrols, posts, start, queued);
    if (patrols > 10 || posts > 10) return ui_text("ArrWarnAi", "! OVER 10 PATROLS OR POSTS: THE REST ATTACK");
    return "";
}

// ---- the list ----

void open_entry(int i) {
    g_entry = i;
    g_car = -1;
    g_first_car = 0;
    g_screen = Screen::Entry;
}

void draw_plan_list() {
    float y = g_py + TOP;
    if (enemy()) put(g_px + LEFT, y, ui_text("PlanEnemyHead", "THE AI BUILDS THEM IN ITS FIRST FREE FACTORY:"));
    else draw_tabs(y);
    y += ROW;
    if (!enemy() && !runs()) {   // the runner orders the factory: a map that runs arrivals
        put(g_px + LEFT, y, ui_text("PlanNeedsArrivals", "TRAINS BY THE SCRIPT: IN A MAP WITH ARRIVALS"));
        put(g_px + LEFT, y + ROW, ui_text("FactoryConvert", "(CONVERT IT IN THE ARRIVALS PANEL)"), false, [] { panel_open_arrivals(); });
        return;
    }
    const std::vector<int> e = entries();
    const int n = static_cast<int>(e.size());
    const int pages = (std::max)(1, (n + LIST_PAGE - 1) / LIST_PAGE);
    g_list_page = (std::max)(0, (std::min)(g_list_page, pages - 1));
    if (n == 0)
        put(g_px + LEFT, y, enemy() ? ui_text("PlanEmpty", "NOTHING YET: + NEW MAKES ONE")
                                    : ui_text("PlanEmptyOwn", "NOTHING YET: + NEW ORDERS A TRAIN"));
    for (int k = g_list_page * LIST_PAGE; k < n && k < (g_list_page + 1) * LIST_PAGE; ++k) {
        const int i = e[k];
        const FactoryTrain& t = queue()[i];
        const float x = g_px + LEFT;
        std::string head = std::to_string(k + 1) + ". " + train_words(t) + ": " + ui_text(TASK_KEYS[t.task], TASK_NAMES[t.task]);
        const bool over = g_rows.hover(x - 2, y - ROW / 2, x + WIDTH * g_cw, y + ROW * 1.5f);
        put(x, y, fit(head, x), i == g_entry || over);
        put(x + 3 * g_cw, y + ROW, fit(when_words(t, i), x + 3 * g_cw), i == g_entry || over);
        g_rows.area(x - 2, y - ROW / 2, x + WIDTH * g_cw, y + ROW * 1.5f, [i] { open_entry(i); });
        y += 2 * ROW;
    }
    // what the factory does with them
    std::string note = enemy() ? ui_printf(ui_format("PlanEnemyNote", "ONE AT A TIME, WHILE FEWER THAN %d ATTACK"), mission_factories().ai.limit)
                               : ui_text("PlanOwnNote", "CAR BY CAR, PAID BY ITS ARMY, ANY CARS");
    if (enemy() && !ai_limit().empty()) note = ai_limit();
    put(g_px + LEFT, g_py + BOTTOM - ROW, fit(note, g_px + LEFT));
    // + NEW, COPY n, DELETE n (the entry opened last), AI..., the pages
    float x = put(g_px + LEFT, g_py + BOTTOM, ui_text("ArrNew", "+ NEW"), false, [] {
        change_all({"OpQueueAdd", "factory queue train"}, [](FactoryState& f) {
            FactoryTrain t;
            const std::vector<int> e = entries();
            if (!e.empty()) t.time = f.queue[e.back()].time + 60;
            if (!enemy()) {
                t.factory = g_factory;
                edit_object_place(g_factory, t.fx, t.fy);
                t.task = TASK_DRIVE;
                t.time = e.empty() ? 30 : t.time;
            }
            f.queue.push_back(t);
            open_entry(static_cast<int>(f.queue.size()) - 1);
        });
    });
    if (valid()) {
        const int shown = number_of(g_entry);
        x = put(x + g_cw, g_py + BOTTOM, ui_printf(ui_format("ArrCopyN", "COPY %d"), shown), false, [] {
            change_all({"OpPlanCopy", "factory plan copy"}, [](FactoryState& f) { f.queue.insert(f.queue.begin() + g_entry + 1, f.queue[g_entry]); });
        });
        x = put(x + g_cw, g_py + BOTTOM, ui_printf(ui_format("ArrDeleteN", "DELETE %d"), shown), false, [] {
            change_all({"OpQueueDelete", "factory queue train deleted"}, [](FactoryState& f) { f.queue.erase(f.queue.begin() + g_entry); });
            g_entry = -1;
        });
    }
    if (enemy()) x = put(x + g_cw, g_py + BOTTOM, ui_text("PlanAi", "AI..."), false, [] { g_screen = Screen::Ai; });
    if (pages > 1) {
        x = put(x + g_cw, g_py + BOTTOM, "<", false, [] { --g_list_page; });
        x = put(x, g_py + BOTTOM, ui_printf("%d/%d", g_list_page + 1, pages));
        put(x, g_py + BOTTOM, ">", false, [pages] { g_list_page = (std::min)(pages - 1, g_list_page + 1); });
    }
}

// ---- an entry ----

float draw_name(const FactoryTrain& t, float y) {
    float x = label(y, "PlanTrain", "TRAIN:");
    std::string name = train_words(t);
    if (!enemy()) name += ui_printf(ui_format("PlanPrice", ", PRICE %d"), price_of(t));
    put(x, y, fit(name, x));
    return y + ROW;
}

// The cars as icons over three rows, in the train's order (as it leaves the factory); in the
// label's column + CAR and TEMPLATE... - or, for the car chosen with a click, < > and REMOVE.
float draw_cars(const FactoryTrain& t, float y) {
    const int n = static_cast<int>(t.cars.size());
    if (g_car >= n) g_car = -1;
    put(g_px + LEFT, y, ui_text("ArrCars", "CARS:"));
    if (g_car < 0) {
        if (n < MAX_CARS) put(g_px + LEFT, y + ROW, ui_text("ArrAddCar", "+ CAR"), false, [] { panel_open_vehicles_for_train(true); });
        put(g_px + LEFT, y + 2 * ROW, ui_text("ArrTemplates", "TEMPLATE..."), false, [] {
            g_screen = Screen::Templates;
            g_template_page = 0;
        });
    } else {
        auto move = [](int by) {
            return [by] {
                change({"OpQueueCars", "factory queue train"}, [by](FactoryTrain& r) {
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
                change({"OpQueueCars", "factory queue train"}, [](FactoryTrain& r) {
                    if (g_car < static_cast<int>(r.cars.size()) && r.cars.size() > 1) r.cars.erase(r.cars.begin() + g_car);
                });
                g_car = -1;
            });
    }
    if (g_car >= 0 && g_car < g_first_car) g_first_car = g_car;
    if (g_car >= g_first_car + SHOWN_CARS) g_first_car = g_car - SHOWN_CARS + 1;
    g_first_car = (std::max)(0, (std::min)(g_first_car, n - SHOWN_CARS));
    float x = g_px + LEFT + LABEL;
    const float cy = y + ROW;
    for (int i = g_first_car; i < n && i < g_first_car + SHOWN_CARS; ++i) {
        const float cx = x + ICON / 2;
        const int frame = palette_icon(t.cars[i]);
        if (frame >= 0) edit_draw_picture(VID_ICONS, frame, cx, cy, STEP - 1);
        else put(x + 4, cy, std::to_string(t.cars[i]));
        if (i == g_car || g_rows.hover(x, cy - ICON / 2, x + ICON, cy + ICON / 2)) edit_draw_picture(VID_FRAME, 0, cx, cy, STEP);
        g_rows.area(x, cy - ICON / 2, x + ICON, cy + ICON / 2, [i] { g_car = g_car == i ? -1 : i; });
        x += ICON;
    }
    if (n > SHOWN_CARS) {
        const float sx = put(x + 2, cy, "<", false, [] { g_first_car = (std::max)(0, g_first_car - 1); });
        put(sx, cy, ">", false, [] { ++g_first_car; });
    }
    return y + 3 * ROW;
}

// The conditions this panel offers (the enemy's factory: not the player paying), and the next one.
int next_condition(int code, int by) {
    std::vector<int> codes;
    for (const Condition& c : mission_conditions())
        if (c.code != IF_NONE && (c.enemy || !enemy())) codes.push_back(c.code);
    auto at = std::find(codes.begin(), codes.end(), code);
    const int i = at == codes.end() ? 0 : static_cast<int>(at - codes.begin());
    return codes[(i + by + codes.size()) % codes.size()];
}

float draw_when(const FactoryTrain& t, float y) {
    auto pick = [](int when) {
        return [when] {
            change({"OpPlanWhen", "factory plan time"}, [when](FactoryTrain& r) {
                if (when == WHEN_IF && r.when != WHEN_IF) {
                    r.cond = next_condition(r.cond, 0);
                    r.cond_n = mission_condition(r.cond).first;
                }
                r.when = when;
            });
        };
    };
    auto time = [](int by) {
        change({"OpPlanWhen", "factory plan time"}, [by](FactoryTrain& r) { r.time = (std::max)(0, r.time + by * 10); });
    };
    float x = label(y, "ArrWhen", "WHEN:");
    x = put(x, y, ui_text("PlanWhenAt", "AT"), t.when == WHEN_AT, pick(WHEN_AT));
    if (!runs()) {   // the authors' queue: each entry once, at its time
        number(g_px + LEFT + LABEL, y + ROW, seconds(t.time), time);
        put(g_px + LEFT + LABEL, y + 2 * ROW, ui_text("PlanMoreArrivals", "MORE IN A MAP WITH ARRIVALS"), false, [] { panel_open_arrivals(); });
        return y + 3 * ROW;
    }
    x = put(x + g_cw, y, ui_text("PlanWhenAfter", "AFTER ANOTHER"), t.when == WHEN_AFTER, pick(WHEN_AFTER));
    put(x + g_cw, y, ui_text("PlanWhenIf", "WHILE"), t.when == WHEN_IF, pick(WHEN_IF));
    y += ROW;
    const float vx = g_px + LEFT + LABEL;
    if (t.when == WHEN_IF) {   // the condition: < > go through the list, - n + its number
        x = put(vx, y, "<", false, [] { change({"OpPlanWhen", "factory plan time"}, [](FactoryTrain& r) {
                                        r.cond = next_condition(r.cond, -1);
                                        r.cond_n = mission_condition(r.cond).first;
                                    }); });
        const Condition& c = mission_condition(t.cond);
        x = put(x, y, fit(ui_text(c.key, c.english), x), true);
        x = put(x, y, ">", false, [] { change({"OpPlanWhen", "factory plan time"}, [](FactoryTrain& r) {
                                        r.cond = next_condition(r.cond, 1);
                                        r.cond_n = mission_condition(r.cond).first;
                                    }); });
        y += ROW;
        if (c.number) {
            const int step = c.step;
            x = put(vx, y, ui_text("PlanNumber", "N:"));
            number(x, y, std::to_string(t.cond_n), [step](int by) {
                change({"OpPlanWhen", "factory plan time"}, [step, by](FactoryTrain& r) { r.cond_n = (std::max)(step, r.cond_n + by * step); });
            });
            y += ROW;
        }
        x = put(vx, y, ui_text("ArrNotBefore", "NOT BEFORE"));
        number(x, y, seconds(t.time), time);
    } else if (t.when == WHEN_AFTER) {   // the entry before it in this list
        x = number(vx, y, seconds(t.time), time);
        const int before = number_of(g_entry) - 1;
        if (before > 0) put(x, y, fit(ui_printf(ui_format("PlanAfterBuilt", "AFTER TRAIN %d IS MADE"), before), x));
    } else {
        x = number(vx, y, seconds(t.time), time);
        put(x, y, fit(ui_text("PlanFromStart", "FROM THE START"), x));
    }
    return y + ROW;
}

float draw_repeat(const FactoryTrain& t, float y) {
    if (!runs()) return y;
    float x = label(y, "ArrRepeat", "REPEAT:");
    x = put(x, y, ui_text("ArrOnce", "ONCE"), t.times == 1, [] { change({"OpPlanRepeat", "factory plan repeat"}, [](FactoryTrain& r) { r.times = 1; }); });
    x = number(x + g_cw, y, std::to_string(t.times >= 2 ? t.times : 3), [](int by) {
        change({"OpPlanRepeat", "factory plan repeat"}, [by](FactoryTrain& r) { r.times = (std::max)(2, (r.times >= 2 ? r.times : 3) + by); });
    });
    x = put(x, y, ui_text("ArrTimesWord", "TIMES"), t.times >= 2, [] {
        change({"OpPlanRepeat", "factory plan repeat"}, [](FactoryTrain& r) {
            if (r.times < 2) r.times = 3;
        });
    });
    put(x + g_cw, y, ui_text("ArrEndlessChoice", "WITHOUT END"), t.times == 0,
        [] { change({"OpPlanRepeat", "factory plan repeat"}, [](FactoryTrain& r) { r.times = 0; }); });
    y += ROW;
    if (t.times != 1) {
        x = put(g_px + LEFT + LABEL, y, ui_text("ArrEveryWord", "EVERY"));
        x = number(x, y, seconds(t.every), [](int by) {
            change({"OpPlanRepeat", "factory plan repeat"}, [by](FactoryTrain& r) { r.every = (std::max)(10, r.every + by * 10); });
        });
        put(x, y, fit(ui_text("PlanEveryNote", "AFTER THE LAST"), x));
        y += ROW;
    }
    return y;
}

float draw_then(const FactoryTrain& t, float y) {
    float x = label(y, "ArrThen", "THEN:");
    for (int task : tasks_here())
        x = put(x, y, ui_text(TASK_KEYS[task], TASK_NAMES[task]), t.task == task,
                [task] { change({"OpQueueTask", "factory queue task"}, [task](FactoryTrain& r) { r.task = task; }); });
    y += ROW;
    if (needs_point(t)) {
        x = label(y, t.task == TASK_PATROL ? "ArrPoints" : "ArrPoint", t.task == TASK_PATROL ? "POINTS:" : "POINT:");
        if (t.tx || t.ty) x = put(x, y, ui_printf("%d,%d", t.tx, t.ty));
        if (t.task == TASK_PATROL && (t.tx2 || t.ty2)) x = put(x, y, ui_printf("- %d,%d", t.tx2, t.ty2));
        put(x + g_cw, y, ui_text("ArrPick", "PICK"), false, [] { edit_pick_queue_point(g_factory, g_entry); });
        y += ROW;
    }
    return y;
}

void draw_entry() {
    if (!valid()) {
        g_screen = Screen::Plan;
        return;
    }
    const FactoryTrain t = queue()[g_entry];
    float y = g_py + TOP;
    y = draw_name(t, y);
    y = draw_cars(t, y);
    y = draw_when(t, y);
    y = draw_repeat(t, y);
    y = draw_then(t, y);
    Problem p;
    if (problem_of(t, p)) {
        const float py = (std::max)(y, g_py + BOTTOM);
        const float x = put(g_px + LEFT, py, p.text);
        put(x, py, fit(p.button, x), false, p.act);
    }
}

// ---- the authors' trains by name: a click puts one into the entry ----

void draw_templates() {
    const std::vector<AuthorTrain>& t = mission_author_trains();
    const int pages = (std::max)(1, (static_cast<int>(t.size()) + TEMPLATE_PAGE - 1) / TEMPLATE_PAGE);
    g_template_page = (std::max)(0, (std::min)(g_template_page, pages - 1));
    float y = g_py + TOP;
    put(g_px + LEFT, y, ui_text("ArrTemplatesHead", "THE AUTHORS' TRAINS (A CLICK TAKES ONE):"));
    y += ROW;
    for (int i = g_template_page * TEMPLATE_PAGE; i < static_cast<int>(t.size()) && i < (g_template_page + 1) * TEMPLATE_PAGE; ++i) {
        put(g_px + LEFT + 2 * g_cw, y, fit(mission_author_train_name(t[i]), g_px + LEFT + 2 * g_cw), false, [i] {
            const std::vector<int> cars = mission_author_trains()[i].cars;
            change({"OpQueueCars", "factory queue train"}, [cars](FactoryTrain& r) {
                r.cars = cars;
                if (r.cars.size() > MAX_CARS) r.cars.resize(MAX_CARS);
            });
            g_screen = Screen::Entry;
            g_car = -1;
        });
        y += ROW;
    }
    if (pages > 1) {
        float x = put(g_px + LEFT, g_py + BOTTOM, "<", false, [] { --g_template_page; });
        x = put(x, g_py + BOTTOM, ui_printf("%d/%d", g_template_page + 1, pages));
        put(x, g_py + BOTTOM, ">", false, [pages] { g_template_page = (std::min)(pages - 1, g_template_page + 1); });
    }
}

// ---- the AI's knobs, three rows each: what it is and its value, the authors' name, what the game
// does with it ----

constexpr int AI_PAGES = 3;
constexpr float KNOB_LABEL = 128;   // the value's column

void change_ai(const std::function<void(AiKnobs&)>& f) {
    change_all({"OpAiKnob", "AI setting"}, [&f](FactoryState& s) { f(s.ai); });
}

float knob_rows(float y, const char* key, const char* english, const char* name, const char* note_key, const char* note) {
    put(g_px + LEFT, y, ui_text(key, english));
    put(g_px + LEFT + 2 * g_cw, y + ROW, name);
    put(g_px + LEFT + 2 * g_cw, y + 2 * ROW, fit(ui_text(note_key, note), g_px + LEFT + 2 * g_cw));
    return y + 3 * ROW + 4;
}

float knob_number(float y, const char* key, const char* english, const char* name, const std::string& shown, std::function<void(int)> by,
                  const char* note_key, const char* note) {
    number(g_px + LEFT + KNOB_LABEL, y, shown, std::move(by));
    return knob_rows(y, key, english, name, note_key, note);
}

float knob_choice(float y, const char* key, const char* english, const char* name,
                  const std::vector<std::pair<std::string, std::function<void()>>>& choices, int current, const char* note_key, const char* note) {
    float x = g_px + LEFT + KNOB_LABEL;
    for (size_t i = 0; i < choices.size(); ++i) x = put(x, y, choices[i].first, static_cast<int>(i) == current, choices[i].second) + g_cw;
    return knob_rows(y, key, english, name, note_key, note);
}

void draw_ai() {
    const AiKnobs ai = mission_factories().ai;
    g_ai_page = (std::max)(0, (std::min)(g_ai_page, AI_PAGES - 1));
    float y = g_py + TOP;
    put(g_px + LEFT, y, ui_text("AiHead", "THE AI'S SETTINGS:"));
    y += ROW + 4;
    if (g_ai_page == 0) {
        y = knob_number(y, "AiLimit", "ATTACK AT ONCE:", "LIMIT_ATACKERS", std::to_string(ai.limit),
                        [](int by) { change_ai([by](AiKnobs& a) { a.limit = (std::max)(1, a.limit + by); }); }, "AiLimitNote",
                        "THE PLAN WAITS WHILE SO MANY ATTACK");
        y = knob_choice(y, "AiRenew", "START TRAINS:", "START_TRAINS_RENEW",
                        {{ui_text("AiRenewYes", "BUILT AGAIN"), [] { change_ai([](AiKnobs& a) { a.renew = 1; }); }},
                         {ui_text("AiRenewNo", "ONCE"), [] { change_ai([](AiKnobs& a) { a.renew = 0; }); }}},
                        ai.renew == 1 ? 0 : ai.renew == 0 ? 1 : -1, "AiRenewNote", "A LOST ONE IS BUILT AGAIN BEFORE THE PLAN");
        y = knob_number(y, "AiRenewWait", "AGAIN AFTER:", "RENEW_TIMEOUT", seconds(ai.renew_wait),
                        [](int by) { change_ai([by](AiKnobs& a) { a.renew_wait = (std::max)(0, a.renew_wait + by * 5); }); }, "AiRenewWaitNote",
                        "+- HALF OF IT AT RANDOM");
    } else if (g_ai_page == 1) {
        y = knob_number(y, "AiRaid", "RAIDS:", "START_TRAINS_RANDOM_TASK",
                        ai.raid > 0 ? ui_printf(ui_format("AiEvery", "EVERY %s"), seconds(ai.raid).c_str())
                                    : ai.raid == 0 ? ui_text("AiNone", "NONE") : ui_text("AiEach", "EACH ITS OWN"),
                        [](int by) { change_ai([by](AiKnobs& a) { a.raid = (std::max)(0, (std::max)(0, a.raid) + by * 30); }); }, "AiRaidNote",
                        "A START TRAIN LEAVES ITS TASK TO ATTACK");
        y = knob_number(y, "AiRaidsFrom", "RAIDS FROM:", "FIRSTATTACKTIME", seconds(ai.raids_from),
                        [](int by) { change_ai([by](AiKnobs& a) { a.raids_from = (std::max)(0, a.raids_from + by * 10); }); }, "AiRaidsFromNote",
                        "X2 ON EASY, X1.5 NORMAL, X1 HARD");
        y = knob_number(y, "AiRaidLength", "A RAID LASTS:", "FIRSTCONTACTTIME", seconds(ai.raid_length),
                        [](int by) { change_ai([by](AiKnobs& a) { a.raid_length = (std::max)(5, a.raid_length + by * 5); }); },
                        "AiRaidLengthNote", "THEN THE TRAIN GOES BACK TO ITS TASK");
    } else {
        y = knob_choice(y, "AiClever", "CLEVER ATTACK:", "SETCLEVERENEMYATTACK",
                        {{ui_text("WordYes", "YES"), [] { change_ai([](AiKnobs& a) { a.clever = 1; }); }},
                         {ui_text("WordNo", "NO"), [] { change_ai([](AiKnobs& a) { a.clever = 0; }); }}},
                        ai.clever ? 0 : 1, runs() ? "AiCleverNote" : "AiCleverArrivals",
                        runs() ? "THE ENGINE'S, WITH THE WEAPONS' RANGE" : "ONLY IN A MAP WITH ARRIVALS");
        y = knob_number(y, "AiAllAttack", "ALL ATTACK:", "STG_DESTROY_TIME", ui_printf(ui_format("AiMinutes", "%d MIN"), ai.all_attack),
                        [](int by) { change_ai([by](AiKnobs& a) { a.all_attack = (std::max)(0, a.all_attack + by); }); }, "AiAllAttackNote",
                        "ONLY MISSION 2 READS IT: NO EFFECT HERE");
    }
    float x = put(g_px + LEFT, g_py + BOTTOM, "<", false, [] { g_ai_page = (std::max)(0, g_ai_page - 1); });
    x = put(x, g_py + BOTTOM, ui_printf("%d/%d", g_ai_page + 1, AI_PAGES));
    put(x, g_py + BOTTOM, ">", false, [] { g_ai_page = (std::min)(AI_PAGES - 1, g_ai_page + 1); });
}

}  // namespace

// editor_panel.cpp: the panel of the factory `factory_open` named.
void factory_draw(POINT m) {
    g_rows.begin(m);
    g_cw = g_rows.cw;
    if (edit_object_gone(g_factory)) {
        panel_frame(VID_PICTURE, ui_text("FactoryHeader", "FACTORY"));
        return;
    }
    const int a = army();
    if (a == 1 && g_screen == Screen::Builds) g_screen = Screen::Plan;   // the enemy's builds its plan, not its list
    const std::string title = g_screen == Screen::Entry && valid() ? ui_printf(ui_format("PlanEntryHeader", "TRAIN %d"), number_of(g_entry))
                                                                   : std::string(ui_text(ARMY_TITLES[a][0], ARMY_TITLES[a][1]));
    panel_frame(VID_PICTURE, title.c_str());
    panel_picture(g_px, g_py);
    switch (g_screen) {
    case Screen::Builds: draw_list(); break;
    case Screen::Plan: draw_plan_list(); break;
    case Screen::Entry: draw_entry(); break;
    case Screen::Templates: draw_templates(); break;
    case Screen::Ai: draw_ai(); break;
    }
}

bool factory_click(POINT m) { return g_rows.click(m); }

// A screen back (an entry -> the list, the templates -> the entry, the AI's settings -> the list);
// false when there is none.
bool factory_has_back() { return g_screen == Screen::Entry || g_screen == Screen::Templates || g_screen == Screen::Ai; }
bool factory_back() {
    if (!factory_has_back()) return false;
    g_screen = g_screen == Screen::Templates ? Screen::Entry : Screen::Plan;
    return true;
}

void factory_open(void* factory) {
    if (factory != g_factory) {
        g_chosen = 0;
        g_entry = -1;
        g_list_page = 0;
        g_screen = edit_object_army(factory) == 1 ? Screen::Plan : Screen::Builds;
    }
    g_factory = factory;
    if (g_screen == Screen::Templates || (g_screen == Screen::Entry && !valid())) g_screen = Screen::Plan;
}

// The Vehicles palette picked a car for the opened entry's train.
void factory_add_car(int nvid) {
    change({"OpQueueCars", "factory queue train"}, [nvid](FactoryTrain& r) {
        if (r.cars.size() < MAX_CARS) r.cars.push_back(nvid);
    });
}

void factory_reset() {
    g_screen = Screen::Plan;
    g_car = -1;
    g_factory = nullptr;
    g_chosen = 0;
    g_entry = -1;
}
