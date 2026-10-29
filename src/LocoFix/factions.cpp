// The factions of an editor map: the neutrals (army 2) fight only whoever attacks them, the
// pirates (army 3) fight every side. Only on a map whose script has `#define FACTIONS 1` (the
// editor writes it into the settings' block of every map it saves, editor_mission.cpp; the
// template has it); the campaign's maps keep the engine's rules.
//
// The engine's rules. Object_FindTarget (0x449ea0) is where a unit firing on its own (behave bit
// 0, BEH_AGGRESSIVE: Unit_Action, Engine_UpdateAttack, the balloons and the planes) picks its
// target from the unit list of g_objectMap. Its army test (0x449f7b ... 0x449ff3) takes a
// candidate of another army - or of any army when the shooter's weapon has flag 0x80 -, not one
// whose VID has flag 0x20000 (hidden), and of army 2 only with that weapon flag, or for a shooter
// of army 1 at a train (spriteClass 0x15) after script SetEnemyCanAttackNeutralTrains (World
// flags 0x2). So no one fires at a neutral on its own, a neutral with BEH_AGGRESSIVE fires at
// ours, the enemy and the pirates alike, and the pirates fire at ours and the enemy but not at the
// neutrals. A shot hurts through the death explosion of its projectile, Object_Action case 15
// (0x447cc3 ...): every object in the explosion's box gets ACT_DAMAGE 85 (var1 the damage, var2
// the projectile, created with its shooter's army by Object_Construct) at 0x447ea0 (with a damage
// radius, Vid flag 0x4000000) or 0x447eb2 (the whole damage).
//
// On a map with FACTIONS:
//  - the army test is ours (may_target): a neutral shooter takes only a candidate of an army that
//    provoked it (grudge, below); a neutral candidate is taken by the pirates always and by an
//    army it holds a grudge against; the rest is the engine's test, unchanged;
//  - a hit of a neutral by another army (note_hit, before each ACT_DAMAGE of an explosion) gives
//    the neutral object hit, and every armed neutral that sees it (within its weapon's
//    detectRange, the octagonal distance of Object_FindTarget), a grudge against that army: they
//    answer that army - only that one - from then on, and its units may take them as targets
//    too. A grudge lasts to the end of the level. A neutral without BEH_AGGRESSIVE (the editor's
//    NEVER) never fires on its own.
// A map without FACTIONS runs the original test exactly (may_target's last lines) and records no
// grudges.
#include "patch.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <unordered_map>

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;        // World* g_world
constexpr uintptr_t G_OBJECT_MAP = 0x4a9c10;   // the map hash: +0x34 the unit list
constexpr uintptr_t ARMY_TEST = 0x449f7b;      // in Object_FindTarget: mov ecx,[edi+0x28] ...
constexpr uintptr_t ARMY_TEST_PASSED = 0x449ff3;   // test bl,8 (the uplink test)
constexpr uintptr_t NOT_A_TARGET = 0x44a163;       // the next candidate
constexpr uintptr_t HIT_RADIUS = 0x447e9c;     // mov ecx,edi / fstp st(0) / call [ebx+4] (ACT_DAMAGE)
constexpr uintptr_t HIT_WHOLE = 0x447eb0;      // mov ecx,edi / call [edx+4] (ACT_DAMAGE)

constexpr size_t O_VID = 0x1c, O_FLAGS = 0x28, O_X = 0x30, O_Y = 0x34, O_LINK = 0x40, O_UPLINK = 0x44;
constexpr size_t V_CLASS = 0x10, V_FLAGS = 0x14, V_LINK = 0x5c, V_WEAPON = 0x404;
constexpr size_t W_KIND = 0x00, W_FLAGS = 0x04, W_DETECT = 0x14;
constexpr size_t MAP_UNITS_COUNT = 0x38, MAP_UNITS = 0x40;   // the ObjectList at +0x34
constexpr size_t WORLD_FLAGS = 0x04;
constexpr int NEUTRAL = 2, PIRATES = 3;

template <typename T> T& at(void* p, size_t offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(p) + offset);
}

int army(void* o) { return (at<uint32_t>(o, O_FLAGS) >> 10) & 3; }

bool g_on = false;   // the level's script has FACTIONS

// The armies a neutral object answers (bit n: army n), with its VID: an object made later at the
// same address is another one.
struct Grudge {
    void* vid;
    unsigned armies;
};
std::unordered_map<void*, Grudge> g_grudges;

unsigned grudge_of(void* o) {
    const auto g = g_grudges.find(o);
    return g != g_grudges.end() && g->second.vid == at<void*>(o, O_VID) ? g->second.armies : 0;
}

// The object's or its carriers' (a turret's car, a gun on its host).
bool holds_grudge(void* o, int against) {
    for (int depth = 0; o && depth < 4; ++depth, o = at<void*>(o, O_UPLINK))
        if (grudge_of(o) & (1u << against)) return true;
    return false;
}

void add_grudge(void* o, int against) {
    Grudge& g = g_grudges[o];
    if (g.vid != at<void*>(o, O_VID)) g = {at<void*>(o, O_VID), 0};
    if (g.armies & (1u << against)) return;
    g.armies |= 1u << against;
    patch::log("factions: neutral VID %d at %.0f,%.0f answers army %d", at<int>(at<void*>(o, O_VID), 4),
               at<float>(o, O_X), at<float>(o, O_Y), against);
}

// The range a unit sees in: its weapon's, or its turret's (a combat car); 0 unarmed.
float sight_of(void* o) {
    void* vid = at<void*>(o, O_VID);
    void* link = at<void*>(o, O_LINK);
    void* turret = link && at<void*>(link, O_VID) == at<void*>(vid, V_LINK) ? at<void*>(link, O_VID) : nullptr;
    for (void* v : {vid, turret}) {
        void* w = v ? at<void*>(v, V_WEAPON) : nullptr;
        if (w && at<uint32_t>(w, W_KIND) && at<float>(w, W_DETECT) > 0) return at<float>(w, W_DETECT);
    }
    return 0;
}

float distance(void* a, void* b) {   // Object_FindTarget's: the longer side and half the shorter
    float dx = at<float>(a, O_X) - at<float>(b, O_X), dy = at<float>(a, O_Y) - at<float>(b, O_Y);
    dx = dx < 0 ? -dx : dx;
    dy = dy < 0 ? -dy : dy;
    return dx > dy ? dx + dy * 0.5f : dy + dx * 0.5f;
}

// Before an explosion damages `target`: a neutral hit by another army holds a grudge against it,
// and so does every armed neutral that sees it.
void __cdecl note_hit(void* target, void* source) {
    if (!g_on || !target || !source || army(target) != NEUTRAL) return;
    const int by = army(source);
    if (by == NEUTRAL) return;
    add_grudge(target, by);
    void* map = *reinterpret_cast<void**>(G_OBJECT_MAP);
    if (!map) return;
    void** units = at<void**>(map, MAP_UNITS);
    for (int i = at<int>(map, MAP_UNITS_COUNT) - 1; units && i >= 0; --i) {
        void* u = units[i];
        if (!u || u == target || army(u) != NEUTRAL) continue;
        const float sight = sight_of(u);
        if (sight > 0 && distance(u, target) <= sight) add_grudge(u, by);
    }
}

// Object_FindTarget's army test: 1 when `shooter` may take `target` (see above).
int __cdecl may_target(void* shooter, void* target) {
    const int from = army(shooter), to = army(target);
    void* vid = at<void*>(target, O_VID);
    const bool any_army = (at<uint32_t>(at<void*>(at<void*>(shooter, O_VID), V_WEAPON), W_FLAGS) & 0x80) != 0;
    if (from == to && !any_army) return 0;
    if (at<uint32_t>(vid, V_FLAGS) & 0x20000) return 0;
    const bool neutral_trains = (at<uint32_t>(*reinterpret_cast<void**>(G_WORLD), WORLD_FLAGS) & 2) != 0 &&
                                from == 1 && at<int>(vid, V_CLASS) == 0x15;
    if (g_on) {
        if (from == NEUTRAL) return holds_grudge(shooter, to);
        if (to == NEUTRAL) return from == PIRATES || any_army || neutral_trains || holds_grudge(target, from);
        return 1;
    }
    return to != NEUTRAL || any_army || neutral_trains;
}

uintptr_t g_army_test_passed = ARMY_TEST_PASSED, g_not_a_target = NOT_A_TARGET;

// In place of the army test: esi the candidate, edi the shooter; eax, ecx, edx and ebp are free
// there (the code after the test loads what it uses).
__declspec(naked) void army_test() {
    __asm {
        push esi
        push edi
        call may_target
        add esp, 8
        test eax, eax
        jz skip
        jmp dword ptr [g_army_test_passed]
    skip:
        jmp dword ptr [g_not_a_target]
    }
}

// The two ACT_DAMAGE calls of an explosion: edi the object hit, esi the exploding projectile; the
// arguments are pushed, the return address is ours (the bytes after the call).
__declspec(naked) void hit_radius() {
    __asm {
        fstp st(0)
        pushad
        push esi
        push edi
        call note_hit
        add esp, 8
        popad
        mov ecx, edi
        jmp dword ptr [ebx + 4]
    }
}

__declspec(naked) void hit_whole() {
    __asm {
        pushad
        push esi
        push edi
        call note_hit
        add esp, 8
        popad
        mov ecx, edi
        jmp dword ptr [edx + 4]
    }
}

// `#define FACTIONS 1` on a line of its own in the map's script (<map>.lgc).
bool script_has_factions(const char* map) {
    std::string path = map ? map : "";
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return false;
    std::ifstream in(patch::game_folder() + path.substr(0, dot) + ".lgc");
    std::string line;
    while (std::getline(in, line)) {
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos && line.compare(start, std::string::npos, "#define FACTIONS 1") == 0) return true;
    }
    return false;
}

}  // namespace

// frame_limit.cpp's Level_Load wrapper, before a level loads.
void factions_level_loading(const char* map) {
    g_grudges.clear();
    const bool was = g_on;
    g_on = script_has_factions(map);
    if (g_on || was) patch::log("factions: %s %s", map ? map : "?", g_on ? "- the editor's rules (FACTIONS)" : "- the engine's rules");
}

void install_factions() {
    // mov ecx,[edi+0x28] / mov eax,[esi+0x28] / mov ebp,ecx / xor ebp,eax / test ebp,0xc00 / jne +0x13
    const uint8_t test[] = {0x8B, 0x4F, 0x28, 0x8B, 0x46, 0x28, 0x8B, 0xE9, 0x33, 0xE8,
                            0xF7, 0xC5, 0x00, 0x0C, 0x00, 0x00, 0x75, 0x13};
    // cmp ecx,0x400 / jne / cmp [edx+0x10],0x15 / jne: the end of the test
    const uint8_t test_end[] = {0x81, 0xF9, 0x00, 0x04, 0x00, 0x00, 0x0F, 0x85, 0x7A, 0x01, 0x00, 0x00,
                                0x83, 0x7A, 0x10, 0x15, 0x0F, 0x85, 0x70, 0x01, 0x00, 0x00};
    const uint8_t radius[] = {0x8B, 0xCF, 0xDD, 0xD8, 0xFF, 0x53, 0x04};
    const uint8_t whole[] = {0x8B, 0xCF, 0xFF, 0x52, 0x04};
    if (!patch::check(ARMY_TEST, test, sizeof test) ||
        !patch::check(ARMY_TEST_PASSED - sizeof test_end, test_end, sizeof test_end) ||
        !patch::check(HIT_RADIUS, radius, sizeof radius) || !patch::check(HIT_WHOLE, whole, sizeof whole)) {
        patch::log("skipped factions: unexpected bytes");
        return;
    }
    patch::write_call(ARMY_TEST, nullptr, 0, reinterpret_cast<void*>(&army_test), 5, true);   // the rest is never reached
    patch::write_call(HIT_RADIUS, nullptr, 0, reinterpret_cast<void*>(&hit_radius), sizeof radius, false);
    patch::write_call(HIT_WHOLE, nullptr, 0, reinterpret_cast<void*>(&hit_whole), sizeof whole, false);
}
