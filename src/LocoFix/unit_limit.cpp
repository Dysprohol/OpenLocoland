// Player_CanBuild checks the per-army unit limit (VID_MAXUNITARMY) against the wrong field.
//
// The method decides whether a depot may add a VID to its build queue. It has two limits to
// respect: the total limit (vid->maxUnit, +0x344) and the per-army limit (vid->maxUnitArmy
// [army], +0x348). Then:
//
//   if (maxUnitArmy[army] < 0 || count[army] + queued < maxUnit)      <- here
//       if (maxUnit < 0 || count[0..3] + queued < maxUnit) return 1;  <- and here
//   return 0;
//
// The first line tests the per-army limit but compares against maxUnit (the total limit).
// A VID with a per-army limit and no total limit (maxUnit = -1, the default set by
// Vid_ResetCounters) can never be built at all: count[army] + queued < -1 is always false.
// A VID with both limits only ever has the per-army count checked against the total limit,
// so the per-army limit does nothing. The branch is meant to compare against
// maxUnitArmy[army] (the value it tested). GameWorld_CreateObject (0x423ee0) enforces only
// maxUnit, so this method is the only place the per-army limit is read.
//
// Reached through the state bar: Player_SetPanel / Player_HandlePanelAction call
// Player_CanBuild to grey out an entry of the depot build list (BUGS.md C2). The shipped
// campaign never sets a per-army limit (every `#define LIMIT..` in the mission scripts is
// commented out), so the fix changes nothing in the original missions - it makes the
// script function AddUnitLimit (249) able to enforce a per-army limit.
//
// At 0x432492 `mov ecx,[esi+0x344]` (maxUnit) overwrites ecx, which at 0x432480 was loaded
// with maxUnitArmy[army] and is not touched in between, right before `cmp eax,ecx` compares
// the per-army count + queue length. Six nops leave that value in ecx; the instruction
// length and every branch stay the same.
#include "patch.h"

namespace {

constexpr uintptr_t PER_ARMY_LIMIT = 0x432492;   // in Player_CanBuild

}  // namespace

void install_unit_limit() {
    // mov ecx,[esi+0x344]   (maxUnit, should be maxUnitArmy[army] already in ecx)
    const uint8_t expected[] = {0x8B, 0x8E, 0x44, 0x03, 0x00, 0x00};
    if (!patch::check(PER_ARMY_LIMIT, expected, sizeof expected)) {
        patch::log("skipped per-army limit fix @ %08X: unexpected bytes",
                   static_cast<unsigned>(PER_ARMY_LIMIT));
        return;
    }
    const uint8_t fixed[] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
    patch::write(PER_ARMY_LIMIT, fixed, sizeof fixed);
}
