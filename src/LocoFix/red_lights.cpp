// Trains of chosen armies pass the red lights of an editor map: on a map whose script has
// `#define PASS_RED_LIGHTS <mask>` (bit n: army n - the editor offers 4, the neutrals, and 8, the
// pirates; editor_mission.cpp writes it), a train of such an army may run along a one-way line
// against its direction, where every other train stops. The campaign's maps (no define) keep the
// engine's rule.
//
// The engine's rule. A one-way line (script SetPushLine) sets RailDot.pushLink of every point of its
// way to the link toward its end; a red post (VID 152) only shows where one ends - it is not in the
// rail network. Two places test a line, both against the train's army bits (flags >> 10 & 3), the
// one property of a train the engine keeps per army and the map saves:
//  - RailDot_CanPass (0x433780), asked by the route search (RailDot_Search, RailDot_HasRoomToReverse)
//    for every link: its last test refuses a link whose far point's pushLink leads back
//    (0x433860: mov edx,[ecx+0xc] / xor ecx,ecx / cmp edx,esi / setne cl - ecx the far point, esi
//    the link's backLink, ebp the engine or 0);
//  - Train_Update (0x40b4d0), every frame for each car of a train: a car whose front heads into such
//    a link stops the train, sounds 0x94, turns it round and - moving to a point - drops its command
//    (0x40b60e: cmp [edx+0xc],edi / je 0x40b636 - edx the far point, edi the backLink, esi the
//    train's first car).
// With the define, both tests let a train of a chosen army through. A line's push (push_flag 1) is
// unchanged: it pushes a train that runs along the line, any army's, and one running against it is
// not pushed (Train_Update's push test needs the car's link to be the line's). Per army and not per
// train: a train's cars change ends when it turns round and trains couple and uncouple in play, so a
// mark of one train would have to follow every car, and the map keeps nothing of the kind; the army
// is kept by every car and saved with the map.
#include "patch.h"
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {

constexpr uintptr_t CAN_PASS_TEST = 0x433860, CAN_PASS_BACK = 0x433865;
constexpr uintptr_t AGAINST_TEST = 0x40b60e, AGAINST_YES = 0x40b636, AGAINST_NO = 0x40b613;

unsigned g_mask = 0;   // the armies whose trains pass (bit n: army n)
uintptr_t g_can_pass_back = CAN_PASS_BACK, g_against_yes = AGAINST_YES, g_against_no = AGAINST_NO;

// RailDot_CanPass's last test: edx gets the far point's pushLink; for a train of a chosen army a
// value that is never the backLink (esi + 1), so the link passes.
__declspec(naked) void can_pass_test() {
    __asm {
        mov edx, [ecx + 0xc]
        test ebp, ebp
        jz done
        mov ecx, [ebp + 0x28]
        shr ecx, 10
        and ecx, 3
        mov eax, g_mask
        shr eax, cl
        test al, 1
        jz done
        lea edx, [esi + 1]
    done:
        xor ecx, ecx
        jmp dword ptr [g_can_pass_back]
    }
}

// Train_Update's test of a car heading into a one-way line against it: a train of a chosen army
// goes on as if the line were not there. ecx and edx are loaded again on both ways on.
__declspec(naked) void against_test() {
    __asm {
        cmp [edx + 0xc], edi
        jne no
        mov ecx, [esi + 0x28]
        shr ecx, 10
        and ecx, 3
        mov edx, g_mask
        shr edx, cl
        test dl, 1
        jnz no
        jmp dword ptr [g_against_yes]
    no:
        jmp dword ptr [g_against_no]
    }
}

// The value of `#define PASS_RED_LIGHTS <n>` on a line of its own in the map's script, 0 without it.
unsigned script_mask(const char* map) {
    std::string path = map ? map : "";
    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos) return 0;
    std::ifstream in(patch::game_folder() + path.substr(0, dot) + ".lgc");
    static const std::string KEY = "#define PASS_RED_LIGHTS";
    std::string line;
    while (std::getline(in, line)) {
        const size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos || line.compare(start, KEY.size(), KEY) != 0) continue;
        const std::string rest = line.substr(start + KEY.size());
        if (rest.empty() || (rest[0] != ' ' && rest[0] != '\t')) continue;
        return static_cast<unsigned>(std::strtoul(rest.c_str(), nullptr, 0)) & 15u;
    }
    return 0;
}

}  // namespace

// frame_limit.cpp's Level_Load wrapper, before a level loads.
void red_lights_level_loading(const char* map) {
    const unsigned was = g_mask;
    g_mask = script_mask(map);
    if (g_mask || was)
        patch::log("red lights: %s - trains of armies mask %u pass the one-way lines", map ? map : "?", g_mask);
}

unsigned red_lights_mask() { return g_mask; }

void install_red_lights() {
    const uint8_t can_pass[] = {0x8B, 0x51, 0x0C, 0x33, 0xC9};   // mov edx,[ecx+0xc] / xor ecx,ecx
    const uint8_t can_pass_back[] = {0x3B, 0xD6};                // cmp edx,esi
    const uint8_t against[] = {0x39, 0x7A, 0x0C, 0x74, 0x23};    // cmp [edx+0xc],edi / je 0x40b636
    const uint8_t against_no[] = {0x8B, 0x50, 0x28};             // mov edx,[eax+0x28]
    if (!patch::check(CAN_PASS_TEST, can_pass, sizeof can_pass) || !patch::check(CAN_PASS_BACK, can_pass_back, sizeof can_pass_back) ||
        !patch::check(AGAINST_TEST, against, sizeof against) || !patch::check(AGAINST_NO, against_no, sizeof against_no)) {
        patch::log("skipped red lights: unexpected bytes");
        return;
    }
    patch::write_call(CAN_PASS_TEST, nullptr, 0, reinterpret_cast<void*>(&can_pass_test), sizeof can_pass, true);
    patch::write_call(AGAINST_TEST, nullptr, 0, reinterpret_cast<void*>(&against_test), sizeof against, true);
}
