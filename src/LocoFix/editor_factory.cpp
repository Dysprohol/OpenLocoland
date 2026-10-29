// A factory's panel (editor mode): what a factory (VID 102) builds (DEVELOPERS.md "Editor mode",
// a factory's build options; the data: editor_mission.h "factories").
//
// The factory's object panel (the right button) opens it - one of editor_panel.cpp's panels, in the
// game's menu look like the Arrivals panel (editor_arrivals.cpp). What it shows follows what the game
// does with the factories:
//  - ours, the neutrals' and the pirates': the build list - the game's 14 factory units as icons, a
//    click ticks or unticks one; the ticked ones keep the order of the game's list and so their keys
//    (F1-F12, [ and ], as Player_SetPanel numbers them). A neutral or pirate factory builds nothing by
//    itself; the list is what the player builds once it is captured. For the unit chosen last: from
//    when it is in the list (the factory gets it in play then, ACT_ADD_ITEM) and the player's limit of
//    it on the map (AddUnitLimit) - both run by the map's script, so only in a map that runs arrivals;
//  - the enemy's: its AI (army 1 only, ai_events.lgc DepoFree) builds the map's queue (level_queue_*)
//    in its first free factory and ignores the build list: the queue's trains, each with its earliest
//    time, its cars and what it does (attacks, guards a point, patrols between two points), the same
//    as Mission settings' "trains from its factory".
// Every change is a step of the editor's history (the build list with the factory's state).
#include "patch.h"
#include "editor_mission.h"
#include "editor_rows.h"
#include "editor_text.h"

#include <algorithm>
#include <cstdio>
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

namespace {

constexpr int STEP = PanelRows::STEP;
constexpr int VID_PICTURE = 484, VID_ICONS = 944, VID_FRAME = 456, FONT_KEYS = 8;
constexpr float LEFT = -182, LABEL = 104, ROW = PanelRows::ROW, ICON = 41;
constexpr int UNITS[] = {5, 10, 20, 25, 80, 85, 45, 30, 35, 82, 97, 90, 75, 62};   // the game's list, F1 ... ]
const char* const KEYS[] = {"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "[", "]"};
constexpr int UNIT_COUNT = sizeof UNITS / sizeof UNITS[0];
constexpr int MAX_QUEUE = 16, SHOWN_CARS = 6;

// What the panel offers - only what was seen working in play (DEVELOPERS.md, the factories).
constexpr bool LATER_OFFERED = true;   // a unit in the list from a time on
constexpr bool LIMIT_OFFERED = true;   // the player's unit limits

void* g_factory = nullptr;
int g_chosen = 0, g_train = 0, g_template = 0;
PanelRows g_rows;
float g_px = 0, g_py = 0;

const char* const ARMY_TITLES[][2] = {{"FactoryOurs", "FACTORY: OURS"}, {"FactoryEnemy", "FACTORY: ENEMY"},
                                      {"FactoryNeutral", "FACTORY: NEUTRAL"}, {"FactoryPirates", "FACTORY: PIRATES"}};

float put(float x, float y, const std::string& text, bool current = false, std::function<void()> act = nullptr) {
    return g_rows.put(x, y, text, current, std::move(act));
}

float label(float y, const char* key, const char* english) {
    put(g_px + LEFT, y, ui_text(key, english));
    return g_px + LEFT + LABEL;
}

std::string seconds(int s) { return ui_printf("%d:%02d", s / 60, s % 60); }
const char* unit_name(int nvid) {
    const char* n = object_name(nvid);
    return n ? n : "?";
}

// ---- the build list ----

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

void draw_list(bool scripted) {
    const std::vector<int> items = edit_factory_items(g_factory);
    put(g_px + LEFT, g_py - 72, ui_text("FactoryBuilds", "BUILDS (A CLICK TICKS; THE KEYS F1-F12 [ ]):"));
    int key = 0;
    for (int i = 0; i < UNIT_COUNT; ++i) {
        const int nvid = UNITS[i];
        const float x = g_px + LEFT + (i % 7) * ICON + ICON / 2, y = g_py - 46 + (i / 7) * ICON;
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
    put(x, g_py + 34, ui_printf("%s (VID %d)", unit_name(nvid), nvid));
    if (!scripted) {
        put(g_px + LEFT, g_py + 50, ui_text("FactoryNeedsArrivals", "LATER UNITS, LIMITS: IN A MAP WITH ARRIVALS"));
        put(g_px + LEFT, g_py + 64, ui_text("FactoryConvert", "(CONVERT IT IN THE ARRIVALS PANEL)"), false, [] { panel_open_arrivals(); });
        return;
    }
    if (LATER_OFFERED && (in || later)) {
        x = label(g_py + 50, "FactoryFrom", "IN THE LIST:");
        const int t = (std::max)(0, later_of(nvid));
        g_rows.number(x, g_py + 50, t ? ui_printf(ui_format("FactoryAfter", "AFTER %s"), seconds(t).c_str())
                                      : ui_text("ArrAtStart", "AT THE START"),
                      [nvid, t](int by) {
                          set_unit(nvid, true, (std::max)(0, t + by * 30), {"OpFactoryLater", "factory unit later"});
                      });
    }
    if (LIMIT_OFFERED) {
        x = label(g_py + 64, "FactoryLimit", "AT MOST:");
        const int max = limit_of(nvid);
        g_rows.number(x, g_py + 64, max ? std::to_string(max) : ui_text("FactoryNoLimit", "NO LIMIT"),
                      [nvid, max](int by) { set_limit(nvid, (std::max)(0, max + by)); });
        put(g_px + LEFT, g_py + 78, ui_text("FactoryLimitNote", "(OF THE PLAYER'S ON THE MAP, BUILT OR QUEUED)"));
    }
}

// ---- the enemy's queue ----

std::vector<FactoryTrain>& queue() { return mission_factories().queue; }
bool valid_train() { return g_train >= 0 && g_train < static_cast<int>(queue().size()); }

void change_train(const UiText& what, const std::function<void(FactoryTrain&)>& f) {
    if (!valid_train()) return;
    const FactoryState before = mission_factories();
    f(queue()[g_train]);
    if (!(mission_factories() == before)) edit_commit_factories(what, before);
}

void draw_queue() {
    put(g_px + LEFT, g_py - 72, ui_text("FactoryAi1", "THE ENEMY'S AI BUILDS THIS QUEUE IN ITS FIRST"));
    put(g_px + LEFT, g_py - 58, ui_text("FactoryAi2", "FREE FACTORY (ONLY ARMY 1 HAS AN AI):"));
    float x = put(g_px + LEFT, g_py - 42, ui_text("FactoryQueue", "QUEUE:"));
    const int n = static_cast<int>(queue().size());
    g_train = (std::max)(0, (std::min)(g_train, n - 1));
    for (int i = 0; i < n; ++i) x = put(x, g_py - 42, std::to_string(i + 1), i == g_train, [i] { g_train = i; });
    if (n < MAX_QUEUE)
        x = put(x + g_rows.cw, g_py - 42, ui_text("ArrAdd", "+ ADD"), false, [] {
            const FactoryState before = mission_factories();
            FactoryTrain t;
            if (!queue().empty()) t.time = queue().back().time + 60;
            queue().push_back(t);
            g_train = static_cast<int>(queue().size()) - 1;
            edit_commit_factories({"OpQueueAdd", "factory queue train"}, before);
        });
    if (!valid_train()) {
        put(g_px + LEFT, g_py - 14, ui_text("FactoryNoQueue", "NOTHING YET: + ADD"));
        return;
    }
    put(x, g_py - 42, ui_text("ArrDelete", "DELETE"), false, [] {
        const FactoryState before = mission_factories();
        queue().erase(queue().begin() + g_train);
        edit_commit_factories({"OpQueueDelete", "factory queue train deleted"}, before);
    });
    const FactoryTrain t = queue()[g_train];
    // its cars
    x = label(g_py - 14, "ArrTrain", "TRAIN:");
    for (int i = 0; i < static_cast<int>(t.cars.size()) && i < SHOWN_CARS; ++i) {
        const int frame = palette_icon(t.cars[i]);
        if (frame >= 0) edit_draw_picture(VID_ICONS, frame, x + ICON / 2, g_py - 14, STEP - 1);
        if (g_rows.hover(x, g_py - 34, x + ICON, g_py + 6)) edit_draw_picture(VID_FRAME, 0, x + ICON / 2, g_py - 14, STEP);
        if (t.cars.size() > 1)
            g_rows.area(x, g_py - 34, x + ICON, g_py + 6, [i] {
                change_train({"OpQueueCars", "factory queue train"}, [i](FactoryTrain& r) {
                    if (i < static_cast<int>(r.cars.size()) && r.cars.size() > 1) r.cars.erase(r.cars.begin() + i);
                });
            });
        x += ICON;
    }
    if (static_cast<int>(t.cars.size()) > SHOWN_CARS) x = put(x + 2, g_py - 14, ui_printf("+%d", static_cast<int>(t.cars.size()) - SHOWN_CARS));
    if (t.cars.size() < 16) put(x + 4, g_py - 14, ui_text("ArrAddCar", "+ CAR"), false, [] { panel_open_vehicles_for_train(true); });
    // a template
    const std::vector<TrainTemplate>& tpl = mission_train_templates();
    g_template = (g_template % static_cast<int>(tpl.size()) + static_cast<int>(tpl.size())) % static_cast<int>(tpl.size());
    x = label(g_py + 20, "ArrTemplate", "TEMPLATE:");
    x = put(x, g_py + 20, "<", false, [] { --g_template; });
    x = put(x, g_py + 20, ui_text(tpl[g_template].key, tpl[g_template].english), false, [] {
        const std::vector<int> cars = mission_train_templates()[g_template].cars;
        change_train({"OpQueueCars", "factory queue train"}, [cars](FactoryTrain& r) { r.cars = cars; });
    });
    put(x, g_py + 20, ">", false, [] { ++g_template; });
    // when
    x = label(g_py + 34, "FactoryEarliest", "NOT BEFORE:");
    g_rows.number(x, g_py + 34, seconds(t.time), [](int by) {
        change_train({"OpQueueTime", "factory queue time"}, [by](FactoryTrain& r) { r.time = (std::max)(10, r.time + by * 10); });
    });
    // what it does
    x = label(g_py + 48, "ArrThen", "THEN:");
    const int tasks[] = {TASK_ATTACK, TASK_GUARD, TASK_PATROL};
    const char* const keys[] = {"ArrTaskAttack", "ArrTaskGuard", "ArrTaskPatrol"};
    const char* const names[] = {"ATTACKS", "GUARDS", "PATROLS"};
    for (int k = 0; k < 3; ++k) {
        const int task = tasks[k];
        x = put(x, g_py + 48, ui_text(keys[k], names[k]), t.task == task,
                [task] { change_train({"OpQueueTask", "factory queue task"}, [task](FactoryTrain& r) { r.task = task; }); });
    }
    if (t.task != TASK_ATTACK) {
        x = label(g_py + 62, "ArrPoint", "POINT:");
        if (t.tx || t.ty) x = put(x, g_py + 62, ui_printf("%d,%d", t.tx, t.ty));
        if (t.task == TASK_PATROL && (t.tx2 || t.ty2)) x = put(x, g_py + 62, ui_printf("- %d,%d", t.tx2, t.ty2));
        put(x, g_py + 62, t.task == TASK_PATROL ? ui_text("ArrPickTwo", "PICK BOTH ENDS") : ui_text("ArrPick", "PICK"), false,
            [] { edit_pick_queue_point(g_factory, g_train); });
    }
    put(g_px + LEFT, g_py + 84, ui_text("FactoryQueueNote1", "BUILT WHEN ITS TIME HAS COME, THE FACTORY IS"));
    put(g_px + LEFT, g_py + 98, ui_text("FactoryQueueNote2", "FREE AND FEWER ATTACK THAN THE SETTINGS ALLOW"));
}

}  // namespace

// editor_panel.cpp: the panel of the factory `factory_open` named.
void factory_draw(POINT m) {
    g_rows.begin(m);
    if (edit_object_gone(g_factory)) {
        panel_frame(VID_PICTURE, ui_text("FactoryHeader", "FACTORY"));
        return;
    }
    const int army = (std::max)(0, (std::min)(3, edit_object_army(g_factory)));
    panel_frame(VID_PICTURE, ui_text(ARMY_TITLES[army][0], ARMY_TITLES[army][1]));
    panel_picture(g_px, g_py);
    if (army == 1) {
        draw_queue();
        return;
    }
    draw_list(mission_arrivals().on);
    if (army == 2) {
        put(g_px + LEFT, g_py + 96, ui_text("FactoryIdle1", "THE AI BUILDS NOTHING HERE: THE PLAYER BUILDS"));
        put(g_px + LEFT, g_py + 110, ui_text("FactoryIdle2", "THIS LIST ONCE THE FACTORY IS CAPTURED"));
    } else if (army == 3) {   // only a neutral one is captured (CaptureTact, as the missions' LDepoCapture)
        put(g_px + LEFT, g_py + 96, ui_text("FactoryPirate1", "THE AI BUILDS NOTHING HERE, AND THE PLAYER"));
        put(g_px + LEFT, g_py + 110, ui_text("FactoryPirate2", "CAPTURES ONLY NEUTRAL FACTORIES"));
    }
}

bool factory_click(POINT m) { return g_rows.click(m); }

void factory_open(void* factory) {
    if (factory != g_factory) {
        g_chosen = 0;
        g_train = 0;
    }
    g_factory = factory;
}

// The Vehicles palette picked a car for the chosen train of the queue.
void factory_add_car(int nvid) {
    change_train({"OpQueueCars", "factory queue train"}, [nvid](FactoryTrain& r) {
        if (r.cars.size() < 16) r.cars.push_back(nvid);
    });
}

void factory_reset() {
    g_factory = nullptr;
    g_chosen = 0;
    g_train = 0;
}
