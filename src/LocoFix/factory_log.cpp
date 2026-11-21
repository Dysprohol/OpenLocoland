// Every unit a factory queues and releases, in LocoFix.log - so a log always shows who built what.
//
// A factory (B_DEPO, struct Depo) builds from a queue of up to maxQueue (10) VIDs (engine.md
// "Factories"): Depo_AddToQueue (0x404dd0, ACT_BUILD_UNIT of Depo_Action) appends a VID when there is
// room and the army has the money, Depo_ReplaceInQueue (0x404fd0, the player's panel) exchanges the
// VID of an entry, and Depo_ReleaseUnit (0x405290, when the entry's time is up) makes the unit of the
// entry being built and takes it out of the queue. The three are hooked only to log, one line per
// event, around the engine's own code, which runs unchanged:
//
//   factory: army 1 at 1313,565 queued VID 45 (queue 3/10) at 61 s - a command
//   factory: army 0 at 319,1035 queued VID 20 (queue 1/10) at 75 s - the player's panel
//   factory: army 1 at 1313,565 refused VID 45 (queue 10/10, money 120) at 80 s - a command
//   factory: army 0 at 319,1035 replaced entry 2 VID 20 by VID 25 (queue 2/10) at 90 s
//   factory: army 1 at 1313,565 released VID 45 (queue 2/10) at 95 s
//
// "the player's panel" is a build asked for while Player_HandlePanelAction runs (a click or a hotkey
// in the factory's panel calls the factory's Action(ACT_BUILD_UNIT) at once; depot_rows.cpp hooks it
// and says so here); anything else is "a command" - a script's Action / AddCommand (the authors' AI,
// the editor's runner, a mission's own code) carried out by the factory. A refusal, or a release that
// fails ("Depo can't create unit"), that repeats for the same factory and VID within 5 s is logged once. The place is the factory's map point, the
// time the game's clock (g_gameTime).
#include "patch.h"

#include <cstdio>

namespace {

constexpr uintptr_t ADD_TO_QUEUE = 0x404dd0;
constexpr uintptr_t REPLACE_IN_QUEUE = 0x404fd0;
constexpr uintptr_t RELEASE_UNIT = 0x405290;
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GAME_TIME = 0x4a9d64;   // ms, advanced by Game_Step

// GameObject / Depo / World / Player fields (re/types.txt)
constexpr int OBJ_FLAGS = 0x28;        // bits 10-11 army
constexpr int OBJ_X = 0x30;
constexpr int OBJ_Y = 0x34;
constexpr int DEPO_QUEUE = 0xa4;       // short[100] VIDs
constexpr int DEPO_BUILDING = 0x48c;   // 1-based entry being built, 0 idle
constexpr int DEPO_MAX_QUEUE = 0x490;
constexpr int DEPO_COUNT = 0x494;
constexpr int WORLD_PLAYERS = 0x220;   // Player*[4], by army
constexpr int PLAYER_MONEY = 0x04;

using AddToQueue_t = int(__fastcall*)(uint8_t* depo, void* edx, int nvid);
using ReplaceInQueue_t = int(__fastcall*)(uint8_t* depo, void* edx, int index, int nvid);
using ReleaseUnit_t = int(__fastcall*)(uint8_t* depo, void* edx, int a, int b);

AddToQueue_t g_add = nullptr;
ReplaceInQueue_t g_replace = nullptr;
ReleaseUnit_t g_release = nullptr;
int g_panel = 0;   // > 0 while Player_HandlePanelAction runs

// the last refusal logged, so that one repeating is logged once
const uint8_t* g_refused_depo = nullptr;
int g_refused_vid = -1;
uint32_t g_refused_time = 0;

template <typename T> T at(const uint8_t* p, int offset) { return *reinterpret_cast<const T*>(p + offset); }

uint32_t game_ms() { return *reinterpret_cast<uint32_t*>(G_GAME_TIME); }
int army_of(const uint8_t* o) { return static_cast<int>((at<uint32_t>(o, OBJ_FLAGS) >> 10) & 3); }
int count_of(const uint8_t* d) { return at<int>(d, DEPO_COUNT); }

int money_of(int army) {
    const uint8_t* world = *reinterpret_cast<uint8_t**>(G_WORLD);
    const uint8_t* player = world ? at<uint8_t*>(world, WORLD_PLAYERS + 4 * army) : nullptr;
    return player ? at<int>(player, PLAYER_MONEY) : 0;
}

// "factory: army a at x,y <what> (queue n/max<extra>) at t s<tail>"
void line(const uint8_t* d, const char* what, const char* extra, const char* tail) {
    patch::log("factory: army %d at %d,%d %s (queue %d/%d%s) at %u s%s", army_of(d),
               static_cast<int>(at<float>(d, OBJ_X)), static_cast<int>(at<float>(d, OBJ_Y)), what, count_of(d),
               at<int>(d, DEPO_MAX_QUEUE), extra, game_ms() / 1000, tail);
}

int __fastcall add_to_queue(uint8_t* depo, void* edx, int nvid) {
    const int before = count_of(depo);
    const int result = g_add(depo, edx, nvid);
    const char* who = g_panel > 0 ? " - the player's panel" : " - a command";
    char what[48];
    if (count_of(depo) > before) {
        std::snprintf(what, sizeof what, "queued VID %d", nvid);
        line(depo, what, "", who);
        return result;
    }
    const uint32_t now = game_ms();
    const bool again = depo == g_refused_depo && nvid == g_refused_vid && now - g_refused_time < 5000;
    g_refused_depo = depo;
    g_refused_vid = nvid;
    g_refused_time = now;
    if (!again) {
        char extra[32];
        std::snprintf(what, sizeof what, "refused VID %d", nvid);
        std::snprintf(extra, sizeof extra, ", money %d", money_of(army_of(depo)));
        line(depo, what, extra, who);
    }
    return result;
}

int __fastcall replace_in_queue(uint8_t* depo, void* edx, int index, int nvid) {
    const int old = index >= 0 && index < count_of(depo) ? at<short>(depo, DEPO_QUEUE + 2 * index) : -1;
    const int result = g_replace(depo, edx, index, nvid);
    const int now = index >= 0 && index < count_of(depo) ? at<short>(depo, DEPO_QUEUE + 2 * index) : -1;
    if (now != old) {
        char what[64];
        std::snprintf(what, sizeof what, "replaced entry %d VID %d by VID %d", index + 1, old, now);
        line(depo, what, "", g_panel > 0 ? " - the player's panel" : "");
    }
    return result;
}

int __fastcall release_unit(uint8_t* depo, void* edx, int a, int b) {
    const int building = at<int>(depo, DEPO_BUILDING);
    const int before = count_of(depo);
    const int nvid = building >= 1 && building <= before ? at<short>(depo, DEPO_QUEUE + 2 * (building - 1)) : -1;
    const int result = g_release(depo, edx, a, b);
    char what[48];
    if (count_of(depo) < before) {
        std::snprintf(what, sizeof what, "released VID %d", nvid);
        line(depo, what, "", "");
        return result;
    }
    const uint32_t now = game_ms();   // "Depo can't create unit": logged once while it repeats
    const bool again = depo == g_refused_depo && -nvid - 1 == g_refused_vid && now - g_refused_time < 5000;
    g_refused_depo = depo;
    g_refused_vid = -nvid - 1;
    g_refused_time = now;
    if (!again) {
        std::snprintf(what, sizeof what, "could not release VID %d", nvid);
        line(depo, what, "", "");
    }
    return result;
}

}  // namespace

// depot_rows.cpp: Player_HandlePanelAction begins (1) and ends (-1) - a build asked for meanwhile is the panel's
void factory_log_panel(int step) { g_panel += step; }

void install_factory_log() {
    // push ebx / push esi / push edi / mov edi,ecx
    const uint8_t add_start[] = {0x53, 0x56, 0x57, 0x8B, 0xF9};
    // push ebp / mov ebp,[esp+8] / push edi / mov edi,ecx
    const uint8_t replace_start[] = {0x55, 0x8B, 0x6C, 0x24, 0x08, 0x57, 0x8B, 0xF9};
    // sub esp,0xc / push ebp / push esi / push edi / mov esi,ecx
    const uint8_t release_start[] = {0x83, 0xEC, 0x0C, 0x55, 0x56, 0x57, 0x8B, 0xF1};
    g_add = reinterpret_cast<AddToQueue_t>(patch::hook(ADD_TO_QUEUE, add_start, sizeof add_start,
                                                       reinterpret_cast<void*>(&add_to_queue), "Depo_AddToQueue"));
    g_replace = reinterpret_cast<ReplaceInQueue_t>(patch::hook(REPLACE_IN_QUEUE, replace_start, sizeof replace_start,
                                                               reinterpret_cast<void*>(&replace_in_queue),
                                                               "Depo_ReplaceInQueue"));
    g_release = reinterpret_cast<ReleaseUnit_t>(patch::hook(RELEASE_UNIT, release_start, sizeof release_start,
                                                            reinterpret_cast<void*>(&release_unit), "Depo_ReleaseUnit"));
}
