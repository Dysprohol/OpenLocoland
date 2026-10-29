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
struct TrainPatrol {
    int nvid, x, y, to_x, to_y;
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
// File > Mission settings...: true when the settings were changed.
bool mission_dialog(HWND owner);
// After the map was saved as `map`: its script (a new map gets one), the texts and its entry
// on the custom maps page.
void mission_save(const std::string& map);
// The custom maps page (maps\custom.lst): a map's folder with its title added (or its title
// changed), and the title a folder has there (empty: not on the page).
void mission_list_map(const std::string& folder, const std::string& title);
std::string mission_listed_title(const std::string& folder);
// Test mode: the settings of a file (editor_mission.cpp says how) instead of the dialog.
bool mission_settings_from_file(const std::string& path);
