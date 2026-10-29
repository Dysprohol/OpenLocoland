// The Arrivals panel (editor mode): the trains that arrive on the map - the player's reinforcements,
// the enemy's waves, the neutrals' and the pirates' trains - from a rail point, usually on the
// hidden track past the edge of the map (DEVELOPERS.md "Editor mode", the arrivals; the data and
// the script: editor_mission.h / .cpp).
//
// The bar's entry 0, a reinforcement locomotive's panel and the Arrivals... button of Mission
// settings open it. It is one of the panels of editor_panel.cpp - the game's menu page (VID 484) in
// the menu device - and keeps the game's look: the menu font (VID 2), lit (VID 7) for what is set and
// what is under the mouse, the set choice underlined, the cars as the palette's icons (VID 944). Its
// rows, for the arrival chosen in the first one (1, 2, 3 ... + ADD, COPY, DELETE):
//   WHO       the army: ours, enemy, neutral, pirates
//   TRAIN     its cars (a click on one takes it away, + adds one from the Vehicles palette) and
//             the authors' templates to start from
//   FROM      the entry: a rail point picked on the map, one the other arrivals use, and its push
//             line into the map
//   FIRST / EVERY / TIMES   the first wave after T s, the next ones every N s, K of them or no end
//   IF / AS   a condition, only then or also earlier
//   THEN      what it does: the enemy's attack, forts, a post or a patrol (the AI), the others' drive
//             to a point
//   RAIDS / MESSAGE
// and a line of warnings and the AI's use (patrols, posts) at the bottom. Every change is one step
// of the editor's history (edit_commit_arrivals). A map whose script cannot run arrivals shows why;
// an older editor map is converted first (CONVERT: its authors' reinforcement becomes arrivals).
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
// the rows under the title (the menu titles' font down to about -80), the summary above the button bar
constexpr float Y_LIST = -70, Y_WHO = -56, Y_TRAIN = -28, Y_TEMPLATE = 0, Y_FROM = 14, Y_FIRST = 28, Y_TIMES = 42,
                Y_IF = 56, Y_THEN = 70, Y_POINT = 84, Y_EXTRA = 98, Y_SUMMARY = 114;
constexpr int TABS = 9;            // arrivals shown in the first row at once
constexpr int MAX_ARRIVALS = 32;
constexpr int SHOWN_CARS = 6;

// What the panel offers - only what was seen working in play (DEVELOPERS.md, the arrivals' table).
constexpr bool ARMY_OFFERED[] = {true, true, true, true};
// Not "stays": a train standing behind the entry holds every later arrival there; not "the forts":
// ATA_FIGHT_FORT drove at the player's base, not at the player's fort (both seen in play).
constexpr bool TASK_OFFERED[] = {false, true, false, true, true, true};   // ArrivalTask
// The enemy's raids (ai_trains_random_task): no sortie was seen in play.
constexpr bool RAIDS_OFFERED = false;
constexpr bool IF_OFFERED[] = {true, true, true, true, true, true, true, true, true, true};   // ArrivalIf

int g_selected = 0, g_first_tab = 0, g_template = 0;
bool g_recipes = false;   // + ADD: the recipes are shown
float g_cw = 8, g_ch = 13;
float g_px = 0, g_py = 0;   // the picture's middle
float g_times_end = 0;      // where the TIMES row ends (the condition's way goes after it)

PanelRows g_rows;   // the choices where the last frame drew them

const char* const ARMY_KEYS[] = {"CtxOurs", "CtxEnemy", "CtxNeutral", "CtxPirates"};
const char* const ARMY_NAMES[] = {"OURS", "ENEMY", "NEUTRAL", "PIRATES"};
const char* const TASK_KEYS[] = {"ArrTaskNone", "ArrTaskAttack", "ArrTaskForts", "ArrTaskGuard", "ArrTaskPatrol", "ArrTaskDrive"};
const char* const TASK_NAMES[] = {"STAYS", "ATTACKS", "THE FORTS", "GUARDS", "PATROLS", "DRIVES TO"};
const char* const IF_KEYS[] = {"ArrIfNone", "ArrIfNoTrain", "ArrIfFewTrains", "ArrIfFewUnits", "ArrIfNoFactory",
                               "ArrIfNeutralFactory", "ArrIfEnemyFewTrains", "ArrIfFewAttackers", "ArrIfMoney", "ArrIfCampaign"};
const char* const IF_NAMES[] = {"ALWAYS",
                                "THE PLAYER HAS NO TRAIN",
                                "THE PLAYER HAS FEWER TRAINS THAN",
                                "THE PLAYER HAS FEWER UNITS THAN",
                                "THE PLAYER HAS NO FACTORY",
                                "A NEUTRAL FACTORY STANDS",
                                "THE ENEMY HAS FEWER TRAINS THAN",
                                "FEWER ENEMY ATTACKERS THAN",
                                "THE PLAYER PAYS",
                                "NO TRAIN, A NEUTRAL FACTORY"};
const bool IF_NUMBER[] = {false, false, true, true, false, false, true, true, true, false};

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

// The tasks an army's train can get: the enemy's the AI does, the others' a drive.
std::vector<int> tasks_of(int army) {
    std::vector<int> out;
    for (int t : army == 1 ? std::vector<int>{TASK_ATTACK, TASK_FORTS, TASK_GUARD, TASK_PATROL}
                           : std::vector<int>{TASK_NONE, TASK_DRIVE})
        if (TASK_OFFERED[t]) out.push_back(t);
    return out;
}

void set_army(Arrival& a, int army) {
    a.army = army;
    const std::vector<int> tasks = tasks_of(army);
    if (std::find(tasks.begin(), tasks.end(), a.task) == tasks.end()) a.task = tasks.empty() ? TASK_NONE : tasks[0];
    if (army > 1) a.message = false;
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

// ---- the rows ----

void draw_list() {
    float x = put(g_px + LEFT, g_py + Y_LIST, ui_text("ArrList", "ARRIVALS:"));
    const int n = static_cast<int>(list().size());
    g_first_tab = (std::max)(0, (std::min)(g_first_tab, n - TABS));
    if (g_selected < g_first_tab) g_first_tab = g_selected;
    if (g_selected >= g_first_tab + TABS) g_first_tab = g_selected - TABS + 1;
    if (g_first_tab > 0) x = put(x, g_py + Y_LIST, "<", false, [] { --g_first_tab; });
    for (int i = g_first_tab; i < n && i < g_first_tab + TABS; ++i)
        x = put(x, g_py + Y_LIST, std::to_string(i + 1), i == g_selected, [i] { g_selected = i; });
    if (g_first_tab + TABS < n) x = put(x, g_py + Y_LIST, ">", false, [] { ++g_first_tab; });
    if (n < MAX_ARRIVALS)
        x = put(x + g_cw, g_py + Y_LIST, ui_text("ArrAdd", "+ ADD"), g_recipes, [] { g_recipes = !g_recipes; });
    if (valid()) {
        if (n < MAX_ARRIVALS)
            x = put(x, g_py + Y_LIST, ui_text("ArrCopy", "COPY"), false, [] {
                change_all({"OpArrivalCopy", "copy arrival"}, [](ArrivalsState& s) {
                    s.list.insert(s.list.begin() + g_selected + 1, s.list[g_selected]);
                    ++g_selected;
                });
            });
        put(x, g_py + Y_LIST, ui_text("ArrDelete", "DELETE"), false, [] {
            change_all({"OpArrivalDelete", "delete arrival"}, [](ArrivalsState& s) {
                s.list.erase(s.list.begin() + g_selected);
                g_selected = (std::max)(0, (std::min)(g_selected, static_cast<int>(s.list.size()) - 1));
            });
        });
    }
}

void draw_who(const Arrival& a) {
    float x = label(g_py + Y_WHO, "ArrWho", "WHO:");
    for (int army = 0; army < 4; ++army)
        if (ARMY_OFFERED[army])
            x = put(x, g_py + Y_WHO, ui_text(ARMY_KEYS[army], ARMY_NAMES[army]), a.army == army,
                    [army] { change({"OpArrivalArmy", "arrival army"}, [army](Arrival& r) { set_army(r, army); }); });
}

void draw_train(const Arrival& a) {
    float x = label(g_py + Y_TRAIN, "ArrTrain", "TRAIN:");
    const float y = g_py + Y_TRAIN;
    for (int i = 0; i < static_cast<int>(a.cars.size()) && i < SHOWN_CARS; ++i) {
        const float cx = x + ICON / 2;
        const int frame = palette_icon(a.cars[i]);
        if (frame >= 0) edit_draw_picture(VID_ICONS, frame, cx, y, STEP - 1);
        else put(x + 4, y, std::to_string(a.cars[i]));
        const bool over = hover(x, y - ICON / 2, x + ICON, y + ICON / 2);
        if (over) edit_draw_picture(VID_FRAME, 0, cx, y, STEP);
        if (a.cars.size() > 1)   // a click takes the car away
            g_rows.area(x, y - ICON / 2, x + ICON, y + ICON / 2, [i] {
                change({"OpArrivalCar", "arrival train"}, [i](Arrival& r) {
                    if (i < static_cast<int>(r.cars.size()) && r.cars.size() > 1) r.cars.erase(r.cars.begin() + i);
                });
            });
        x += ICON;
    }
    if (static_cast<int>(a.cars.size()) > SHOWN_CARS) x = put(x + 2, y, ui_printf("+%d", static_cast<int>(a.cars.size()) - SHOWN_CARS));
    if (a.cars.size() < 16) put(x + 4, y, ui_text("ArrAddCar", "+ CAR"), false, [] { panel_open_vehicles_for_train(false); });
}

void draw_template() {
    const std::vector<TrainTemplate>& t = mission_train_templates();
    if (t.empty()) return;
    g_template = (g_template % static_cast<int>(t.size()) + static_cast<int>(t.size())) % static_cast<int>(t.size());
    const float y = g_py + Y_TEMPLATE;
    float x = label(y, "ArrTemplate", "TEMPLATE:");
    x = put(x, y, "<", false, [] { --g_template; });
    x = put(x, y, ui_text(t[g_template].key, t[g_template].english), false, [] {
        const std::vector<int> cars = mission_train_templates()[g_template].cars;
        change({"OpArrivalTemplate", "arrival template"}, [cars](Arrival& r) { r.cars = cars; });
    });
    put(x, y, ">", false, [] { ++g_template; });
}

void draw_from(const Arrival& a) {
    const float y = g_py + Y_FROM;
    float x = label(y, "ArrFrom", "FROM:");
    if (!a.entry) {
        x = put(x, y, ui_text("ArrNoEntry", "NONE"));
    } else {
        x = put(x, y, ui_printf(ui_format("ArrEntry", "%d (%d,%d)"), entry_number(a), a.x, a.y));
    }
    x = put(x, y, ui_text("ArrPick", "PICK"), false, [] { edit_pick_arrival(1); });
    if (a.entry) {
        const int state = edit_entry_state(g_selected);
        if (state == 2) x = put(x, y, ui_text("ArrAddLine", "+ ENTRY LINE"), false, [] { edit_add_entry_line(g_selected); });
        else if (state == 1) x = put(x, y, ui_text("ArrLineOk", "LINE OK"));
        else if (state == 0) x = put(x, y, ui_text("ArrOnMap", "ON THE MAP"));
    }
    const std::vector<Entry> e = entries();
    if (e.size() > 1)   // another arrival's entry
        put(x, y, ui_text("ArrNextEntry", "NEXT"), false, [] {
            const std::vector<Entry> all = entries();
            const int at = entry_number(list()[g_selected]);
            const Entry next = all[at % all.size()];
            change({"OpArrivalEntry", "arrival entry"}, [next](Arrival& r) {
                r.x = next.x, r.y = next.y, r.z = next.z, r.entry = true;
            });
        });
}

void draw_when(const Arrival& a) {
    float x = label(g_py + Y_FIRST, "ArrFirst", "FIRST:");
    x = number(x, g_py + Y_FIRST, a.first ? seconds(a.first) : ui_text("ArrAtStart", "AT THE START"), [](int by) {
        change({"OpArrivalTime", "arrival time"}, [by](Arrival& r) { r.first = (std::max)(0, r.first + by * 10); });
    });
    x = put(x + g_cw, g_py + Y_FIRST, ui_text("ArrEvery", "EVERY:"));
    number(x, g_py + Y_FIRST, a.every ? seconds(a.every) : ui_text("ArrNoGap", "AT ONCE"), [](int by) {
        change({"OpArrivalTime", "arrival time"}, [by](Arrival& r) { r.every = (std::max)(0, r.every + by * 10); });
    });
    x = label(g_py + Y_TIMES, "ArrTimes", "TIMES:");
    x = number(x, g_py + Y_TIMES, a.times ? std::to_string(a.times) : ui_text("ArrEndless", "WITHOUT END"), [](int by) {
        change({"OpArrivalTimes", "arrival waves"}, [by](Arrival& r) { r.times = (std::max)(0, (std::min)(99, r.times + by)); });
    });
    g_times_end = put(x + g_cw, g_py + Y_TIMES, ui_text("ArrEndlessChoice", "WITHOUT END"), a.times == 0,
        [] { change({"OpArrivalTimes", "arrival waves"}, [](Arrival& r) { r.times = r.times ? 0 : 1; }); });
}

void draw_if(const Arrival& a) {
    float x = label(g_py + Y_IF, "ArrIf", "IF:");
    auto cycle = [](int by) {
        change({"OpArrivalIf", "arrival condition"}, [by](Arrival& r) {
            int c = r.cond;
            for (int k = 0; k < IF_COUNT; ++k) {
                c = (c + by + IF_COUNT) % IF_COUNT;
                if (IF_OFFERED[c]) break;
            }
            r.cond = c;
            if (IF_NUMBER[c] && r.cond_n <= 0) r.cond_n = c == IF_MONEY ? 300 : 1;
        });
    };
    x = put(x, g_py + Y_IF, "<", false, [cycle] { cycle(-1); });
    x = put(x, g_py + Y_IF, ui_text(IF_KEYS[a.cond], IF_NAMES[a.cond]), false, [cycle] { cycle(1); });
    x = put(x, g_py + Y_IF, ">", false, [cycle] { cycle(1); });
    if (IF_NUMBER[a.cond])
        number(x, g_py + Y_IF, std::to_string(a.cond_n), [](int by) {
            change({"OpArrivalIf", "arrival condition"}, [by](Arrival& r) {
                r.cond_n = (std::max)(r.cond == IF_MONEY ? 50 : 1, r.cond_n + by * (r.cond == IF_MONEY ? 50 : 1));
            });
        });
    if (a.cond == IF_NONE) return;
    // the condition's way on the TIMES row, a click switches it: only then / also earlier
    put(g_times_end + g_cw, g_py + Y_TIMES, a.earlier ? ui_text("ArrEarlier", "ALSO EARLIER") : ui_text("ArrOnlyThen", "ONLY THEN"), true,
        [] { change({"OpArrivalIf", "arrival condition"}, [](Arrival& r) { r.earlier = !r.earlier; }); });
}

void draw_then(const Arrival& a) {
    float x = label(g_py + Y_THEN, "ArrThen", "THEN:");
    const std::vector<int> tasks = tasks_of(a.army);
    for (int t : tasks)
        x = put(x, g_py + Y_THEN, ui_text(TASK_KEYS[t], TASK_NAMES[t]), a.task == t,
                [t] { change({"OpArrivalTask", "arrival task"}, [t](Arrival& r) { r.task = t; }); });
    if (a.task == TASK_GUARD || a.task == TASK_PATROL || a.task == TASK_DRIVE) {
        x = label(g_py + Y_POINT, "ArrPoint", "POINT:");
        if (a.tx || a.ty) x = put(x, g_py + Y_POINT, ui_printf("%d,%d", a.tx, a.ty));
        if (a.task == TASK_PATROL && (a.tx2 || a.ty2)) x = put(x, g_py + Y_POINT, ui_printf("- %d,%d", a.tx2, a.ty2));
        put(x, g_py + Y_POINT, a.task == TASK_PATROL ? ui_text("ArrPickTwo", "PICK BOTH ENDS") : ui_text("ArrPick", "PICK"), false,
            [] { edit_pick_arrival(2); });
    }
    const bool raids = a.army == 1 && RAIDS_OFFERED;
    x = label(g_py + Y_EXTRA, raids ? "ArrRaids" : "ArrMessage", raids ? "RAIDS:" : "MESSAGE:");
    if (raids)
        x = number(x, g_py + Y_EXTRA, a.raid ? seconds(a.raid) : ui_text("ArrNoRaids", "NONE"), [](int by) {
            change({"OpArrivalRaids", "arrival raids"}, [by](Arrival& r) { r.raid = (std::max)(0, r.raid + by * 30); });
        });
    if (a.army > 1) {
        put(x, g_py + Y_EXTRA, ui_text("ArrNoMessage", "NONE (ONLY OURS AND THE ENEMY HAVE ONE)"));
        return;
    }
    if (raids) x = put(x + g_cw, g_py + Y_EXTRA, ui_text("ArrMessageShort", "MESSAGE:"));
    x = put(x, g_py + Y_EXTRA, ui_text("WordYes", "YES"), a.message,
            [] { change({"OpArrivalMessage", "arrival message"}, [](Arrival& r) { r.message = true; }); });
    put(x, g_py + Y_EXTRA, ui_text("WordNo", "NO"), !a.message,
        [] { change({"OpArrivalMessage", "arrival message"}, [](Arrival& r) { r.message = false; }); });
}

// ---- recipes ----
//
// + ADD offers the ways the missions bring trains (DEVELOPERS.md, the arrivals) as starting points;
// the new arrival takes the entry of the chosen one.
struct Recipe {
    const char* key;
    const char* english;
    std::function<void(Arrival&)> make;
};

const std::vector<Recipe>& recipes() {
    static const std::vector<Recipe> list = {
        {"RecipeStart", "THE PLAYER'S TRAIN AT THE START", [](Arrival& a) { a.cars = {5}; }},
        {"RecipeCampaign", "REINFORCEMENTS AS IN THE CAMPAIGN (2 WAVES)",
         [](Arrival& a) {
             a.cars = {5};
             a.cond = IF_CAMPAIGN;
             a.times = 2;
         }},
        {"RecipeBought", "BOUGHT REINFORCEMENTS (300, AS IN MISSION 4)",
         [](Arrival& a) {
             a.cars = {10, 25, 25, 97, 45};
             a.cond = IF_MONEY;
             a.cond_n = 300;
         }},
        {"RecipeWaves", "ENEMY WAVES: FROM 2:00 EVERY 1:30, 4 TIMES",
         [](Arrival& a) {
             set_army(a, 1);
             a.cars = {5, 20, 20};
             a.first = 120;
             a.every = 90;
             a.times = 4;
             a.task = TASK_ATTACK;
             a.cond = IF_FEW_ATTACKERS;
             a.cond_n = 2;
         }},
        {"RecipeQueue", "AN ENEMY QUEUE AS IN MISSION 19 (10 TRAINS)",
         [](Arrival& a) {
             set_army(a, 1);
             a.cars = {5, 80, 80};
             a.every = 80;
             a.times = 10;
             a.task = TASK_ATTACK;
             a.cond = IF_ENEMY_FEW_TRAINS;
             a.cond_n = 3;
             a.earlier = true;
         }},
        {"RecipeEmpty", "A LOCOMOTIVE, TO SET BY HAND", [](Arrival& a) { a.cars = {5}; }},
    };
    return list;
}

void draw_recipes() {
    float y = g_py + Y_WHO;
    put(g_px + LEFT, y, ui_text("ArrRecipes", "A NEW ARRIVAL:"));
    for (size_t r = 0; r < recipes().size(); ++r) {
        y += ROW + 2;
        const Recipe& recipe = recipes()[r];
        put(g_px + LEFT + 2 * g_cw, y, ui_text(recipe.key, recipe.english), false, [r] {
            g_recipes = false;
            change_all({"OpArrivalAdd", "add arrival"}, [r](ArrivalsState& s) {
                Arrival a;
                if (!s.list.empty()) {   // at the entry of the chosen one
                    const Arrival& p = s.list[(std::min)(g_selected, static_cast<int>(s.list.size()) - 1)];
                    a.x = p.x, a.y = p.y, a.z = p.z, a.entry = p.entry;
                }
                recipes()[r].make(a);
                s.list.push_back(a);
                g_selected = static_cast<int>(s.list.size()) - 1;
            });
        });
    }
    put(g_px + LEFT + 2 * g_cw, y + 2 * ROW, ui_text("ArrCancel", "CANCEL"), false, [] { g_recipes = false; });
}

// The warnings of the chosen arrival, else the map's use of the AI.
std::string summary(const Arrival* a) {
    if (a) {
        if (!a->entry) return ui_text("ArrWarnEntry", "! PICK ITS ENTRY ON THE MAP");
        if (a->cars.empty() || (a->cars[0] != 5 && a->cars[0] != 10 && a->cars[0] != 97))
            return ui_text("ArrWarnLoco", "! NO LOCOMOTIVE FIRST: THE TRAIN STAYS");
        if (a->army == 1 && a->times == 0 && !(a->cond != IF_NONE && !a->earlier))
            return ui_text("ArrWarnEndless", "! ENDLESS ENEMY: NOT WON BY DESTROYING IT");
        if ((a->task == TASK_GUARD || a->task == TASK_PATROL || a->task == TASK_DRIVE) && !a->tx && !a->ty)
            return ui_text("ArrWarnPoint", "! PICK THE POINT OF ITS TASK ON THE MAP");
        if (a->task == TASK_PATROL && !a->tx2 && !a->ty2) return ui_text("ArrWarnSecond", "! PICK THE PATROL'S OTHER END");
        if (edit_entry_state(g_selected) == 2) return ui_text("ArrWarnLine", "! NO ENTRY LINE: THE TRAIN STAYS PAST THE EDGE");
        if (edit_entry_state(g_selected) < 0) return ui_text("ArrWarnTrack", "! NO TRACK AT ITS ENTRY");
    }
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
    return ui_printf(ui_format("ArrSummary", "ENEMY TRAINS %s%d OF 16, PATROLS %d OF 10, POSTS %d OF 10"), endless ? "> " : "",
                     enemy, patrols, posts);
}

// A map whose script does not run arrivals: why, or the authors' reinforcement and CONVERT.
void draw_legacy() {
    const std::string refusal = mission_arrivals_refusal();
    if (!refusal.empty()) {
        put(g_px + LEFT, g_py + Y_LIST, refusal.substr(0, 46));
        if (refusal.size() > 46) put(g_px + LEFT, g_py + Y_LIST + ROW, refusal.substr(46));
        return;
    }
    float y = g_py + Y_LIST;
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
    panel_frame(VID_PICTURE, ui_text("ArrHeader", "TRAIN ARRIVALS"));
    panel_picture(g_px, g_py);
    ArrivalsState& s = mission_arrivals();
    if (!s.on) {
        draw_legacy();
        return;
    }
    draw_list();
    if (s.waves > 0) {   // the authors' reinforcement goes on beside the arrivals
        int x, y, z;
        if (edit_reinforcement_marker(x, y, z))
            put(g_px + LEFT, g_py + Y_SUMMARY, ui_text("ArrLegacyConvert", "+ THE AUTHORS' REINFORCEMENT: CONVERT"), false,
                [] { edit_convert_arrivals(); });
    }
    if (g_recipes) {
        draw_recipes();
        return;
    }
    if (!valid()) {
        g_selected = (std::max)(0, (std::min)(g_selected, static_cast<int>(s.list.size()) - 1));
        if (!valid()) {
            put(g_px + LEFT, g_py + Y_WHO, ui_text("ArrEmpty", "NO ARRIVALS YET: + ADD MAKES ONE"));
            return;
        }
    }
    const Arrival a = s.list[g_selected];
    draw_who(a);
    draw_train(a);
    draw_template();
    draw_from(a);
    draw_when(a);
    draw_if(a);
    draw_then(a);
    if (s.waves <= 0) put(g_px + LEFT, g_py + Y_SUMMARY, summary(&a));
}

// A click in the panel (not on its close button): true when it hit a choice.
bool arrivals_click(POINT m) { return g_rows.click(m); }

// The Vehicles palette picked a car for the chosen arrival's train.
void arrivals_add_car(int nvid) {
    change({"OpArrivalCar", "arrival train"}, [nvid](Arrival& r) {
        if (r.cars.size() < 16) r.cars.push_back(nvid);
    });
}

int arrivals_selected() { return g_selected; }
void arrivals_select(int i) { g_selected = i; }
void arrivals_reset() {
    g_recipes = false;
    g_selected = 0;
    g_first_tab = 0;
}
