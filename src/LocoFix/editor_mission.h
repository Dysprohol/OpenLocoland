// The mission of the map in the editor (editor_mission.cpp): its settings and push lines, kept
// in the editor's blocks of the map's script.
#pragma once
#include <windows.h>
#include <string>
#include <vector>

// SetPushLine(begin_x, begin_y, end_x, end_y, push_flag): screen coordinates (MAP_RULES.md 5).
struct PushLine {
    int bx, by, ex, ey, flag;
    bool operator==(const PushLine& o) const {
        return bx == o.bx && by == o.by && ex == o.ex && ey == o.ey && flag == o.flag;
    }
};

// The push lines of the editor's block in main(): the Push line tool edits them.
std::vector<PushLine>& mission_push_lines();
// The SetPushLine calls of the script outside that block (shown, not edited).
const std::vector<PushLine>& mission_script_push_lines();
// A train's patrol: PatrolEngine(FindNearestSprite(GETSPRITE_VID + nvid, x, y, 20), to_x, to_y) in
// main()'s block - the train is found by its first car (VID and map position at the start), the
// far end is a rail point (screen coordinates, as Engine_CommandToPoint takes it).
// With the arrivals' runner (mission_arrivals().on) a train has an order of the Arrival's kinds
// instead (`task`: TASK_DRIVE / TASK_PATROL for any army; TASK_ATTACK / TASK_GUARD / TASK_PATROL the
// enemy's AI, editor_mission.cpp "orders"); without it only the patrol is written.
struct TrainPatrol {
    int nvid, x, y, to_x, to_y;
    int task = 4;                // TASK_PATROL (ArrivalTask, below)
    int to_z = 0;                // the point's height (a drive)
    int army = 0;
    int from_x = 0, from_y = 0;  // the first car's rail point, screen (the enemy's patrol's other end)
    std::vector<int> cars;       // the train's cars, the first one first (the enemy's train type)
};
// The patrols read from the block (editor_edit.cpp gives them to the trains of the held level).
const std::vector<TrainPatrol>& mission_patrols();
// An enemy train of the mission (start_trains*, level_queue_*): the script's AI makes it while
// the level runs, so the map has no object for it - the editor marks where it comes from.
struct EnemyTrainMark {
    std::string type;     // as the dialog shows it: light_gun, or a number
    int time;             // 0: at the start; from the factory's queue: the earliest second
    bool patrol;          // patrols between (x1, y1) and (x2, y2); otherwise it attacks
    int x1, y1, x2, y2;   // screen points; a start train starts at (x1, y1)
};
// The start trains, then the queue (read from the script, as File > Mission settings... has them).
std::vector<EnemyTrainMark> mission_enemy_trains();
// The script has the editor's blocks, so the settings and push lines are written on a save.
bool mission_managed();
// After a level is loaded in editor mode: reads the mission of the map's script.
void mission_load(const std::string& map);
// File > Mission settings...: true when the settings were changed; `open_arrivals` true when it
// ended with its Arrivals... button (a map whose arrivals the editor runs).
bool mission_dialog(HWND owner, bool* open_arrivals = nullptr);
// After the map was saved as `map`: its script (a new map gets one), the texts and its entry
// on the custom maps page.
void mission_save(const std::string& map);
// The custom maps page (maps\custom.lst): a map's folder with its title added (or its title
// changed), and the title a folder has there (empty: not on the page).
void mission_list_map(const std::string& folder, const std::string& title);
std::string mission_listed_title(const std::string& folder);
// Test mode: the settings of a file (editor_mission.cpp says how) instead of the dialog.
bool mission_settings_from_file(const std::string& path);

// ---- train arrivals (the Arrivals panel, editor_arrivals.cpp) ----
//
// A train that comes from a rail point - usually on the hidden track past the edge of the map,
// where an entry push line pushes it in - at a time, again every N s, K times or without end, when
// a condition holds; the enemy's is given to the AI (TrainsTact.lgc) with a task. Written into the
// script's blocks (editor_mission.cpp says how) and run by its ArrivalsTact().
enum ArrivalTask : int {
    TASK_NONE = 0,     // stays behind the entry (no order; read from a map, not offered: it holds the entry)
    TASK_ATTACK,       // the enemy: ATA_FIGHT
    TASK_FORTS,        // the enemy: ATA_FIGHT_FORT
    TASK_GUARD,        // the enemy: ATA_STANDn, the point
    TASK_PATROL,       // the enemy: ATA_PATROLn, the point and the second point
    TASK_DRIVE,        // ours, the neutrals, the pirates: drive to the point (ACT_MOVE)
};
enum ArrivalIf : int {
    IF_NONE = 0,
    IF_NO_TRAIN,         // the player has no locomotive (5, 10) nor brander (97)
    IF_FEW_TRAINS,       // the player has fewer than n of them
    IF_FEW_UNITS,        // the player has fewer than n units (NoArmyUnits)
    IF_NO_FACTORY,       // the player has no factory
    IF_NEUTRAL_FACTORY,  // a neutral factory still stands
    IF_ENEMY_FEW_TRAINS, // the enemy has fewer than n locomotives and branders
    IF_FEW_ATTACKERS,    // fewer than n of the AI's trains attack
    IF_MONEY,            // the player has n money - and pays it
    IF_CAMPAIGN,         // no train and a neutral factory stands (the campaign's reinforcements)
    IF_COUNT
};
struct Arrival {
    int army = 0;                      // 0 ours, 1 enemy, 2 neutral, 3 pirates
    std::vector<int> cars{5};          // VIDs, the first one in front
    int x = 0, y = 0, z = 0;           // the entry: a rail point, screen x and y, its height
    bool entry = false;                // picked yet
    int first = 0, every = 0, times = 1;   // s from the start, s after the previous one, waves (0: no end)
    int cond = IF_NONE, cond_n = 0;
    bool earlier = false;              // the condition brings the wave earlier (otherwise: only then)
    int task = TASK_DRIVE;
    int tx = 0, ty = 0, tz = 0;        // the task's point (screen, the rail point's height)
    int tx2 = 0, ty2 = 0;              // the patrol's second point
    int raid = 0;                      // the enemy: a raid on the player every n s (0: none)
    bool message = true;               // "reinforcements arrived" / "enemy reinforcements arrived"
    bool operator==(const Arrival& o) const;
};
// The arrivals of the map and whether its script runs them (its blocks "// <editor arrivals>" and
// "// <editor tact>"); an older editor map runs the authors' reinforcement instead
// (PLAYER_REINFORCEMENT, its point locomotive) until it is converted.
// The authors' reinforcement goes on in such a map as long as it has `waves` (PLAYER_REINFORCEMENT)
// and its point locomotive; the panel turns it into an arrival.
struct ArrivalsState {
    bool on = false;
    std::vector<Arrival> list;
    int waves = 0;
    bool operator==(const ArrivalsState& o) const { return on == o.on && list == o.list && waves == o.waves; }
};
ArrivalsState& mission_arrivals();

// ---- factories (a factory's panel, editor_factory.cpp) ----
//
// What the factories build is the map's: each factory's build list is its Unit.items (SPRD, the
// order of its F keys). The script adds what the map cannot hold: the enemy's queue - the AI of
// army 1 builds it in its first free factory (level_queue_*, ai_events.lgc DepoFree; it ignores
// the build list, and the factories of the other armies build nothing by themselves) -, the units
// a factory gets later in play and the player's unit limits (FactoryTact of the runner, so these
// two need a map that runs arrivals).
struct FactoryTrain {
    int time = 60;                    // s: the earliest it is built (level_queue_times)
    std::vector<int> cars{5, 20, 20};
    int type = -1;                    // the type it was read with (a difficulty's train), -1 none
    std::vector<int> type_cars;       // the cars that type gave: while they are its cars, the type stays
    int task = TASK_ATTACK;           // TASK_ATTACK, TASK_GUARD (tx, ty) or TASK_PATROL (tx ... ty2)
    int tx = 0, ty = 0, tx2 = 0, ty2 = 0;
    bool operator==(const FactoryTrain& o) const {
        return time == o.time && cars == o.cars && type == o.type && task == o.task && tx == o.tx && ty == o.ty &&
               tx2 == o.tx2 && ty2 == o.ty2;
    }
};
struct UnitLimit {   // AddUnitLimit: the army has at most `max` of the VID on the map
    int army, nvid, max;
    bool operator==(const UnitLimit& o) const { return army == o.army && nvid == o.nvid && max == o.max; }
};
struct UnitUnlock {   // the factory at the map point x, y gets the VID into its list after `time` s
    int x, y, nvid, time;
    void* factory = nullptr;   // the held level's factory (editor_edit.cpp finds it; x, y follow it at a save)
    bool operator==(const UnitUnlock& o) const {
        return x == o.x && y == o.y && nvid == o.nvid && time == o.time && factory == o.factory;
    }
};
struct FactoryState {
    std::vector<FactoryTrain> queue;
    std::vector<UnitLimit> limits;
    std::vector<UnitUnlock> unlocks;
    bool operator==(const FactoryState& o) const { return queue == o.queue && limits == o.limits && unlocks == o.unlocks; }
};
FactoryState& mission_factories();
// The authors' reinforcement of the script as it was read: waves and their train.
int mission_reinforcement_waves();
std::vector<int> mission_reinforcement_train();
// Why the map's script cannot run arrivals (empty: it can) - a campaign map, the sandbox, a script
// without "TrainsTact();" in its main().
std::string mission_arrivals_refusal();
// The authors' templates of TrainsTactDeclares.lgc a train can start from (the cars by the
// normal difficulty), and the name of a template's key.
struct TrainTemplate {
    int type;   // ATT_... of TrainsTactDeclares.lgc
    const char* key;
    const char* english;
    std::vector<int> cars;
};
const std::vector<TrainTemplate>& mission_train_templates();
// The AI's limits a map's arrivals and start trains share: patrols and posts (ATA_PATROL0-9,
// ATA_STAND0-9), and the enemy trains at the start and from its factory. The patrols and posts
// past ten are written as attacks (ATA_FIGHT); the panels warn of them.
void mission_ai_use(int& patrols, int& posts, int& start_trains, int& queue_trains);
