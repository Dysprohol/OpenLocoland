// Engine_AdoptNeighbourCommand reads the car in front from the wrong register.
//
// The method gives a car the command and goal of a neighbouring car: first the car behind
// (nextCar), then the car in front (prevCar), otherwise "no command". The second branch
// tests prevCar (eax) but then reads the command and goal from edx, which still holds
// nextCar - or 0 when there is no car behind. With no car behind it reads address 0x28 and
// the game crashes; with a car behind the car gets the wrong command and goal.
//
// Building_UpdateWorkshop calls it when a repair workshop (VID 104) takes over an enemy
// wagon whose goal was a train of its new army (BUGS.md C1).
//
// At 0x407d46 the two reads must use prevCar in eax, exactly like the first branch, so the
// fix only swaps the registers of two 3-byte instructions.
#include "patch.h"

namespace {

constexpr uintptr_t ADOPT_COMMAND = 0x407d46;   // in Engine_AdoptNeighbourCommand

}  // namespace

void install_adopt_command() {
    // mov eax,[edx+0x3c] / mov edx,[edx+0x28]   (edx = nextCar or 0)
    const uint8_t expected[] = {0x8B, 0x42, 0x3C, 0x8B, 0x52, 0x28};
    if (!patch::check(ADOPT_COMMAND, expected, sizeof expected)) {
        patch::log("skipped adopt command fix @ %08X: unexpected bytes",
                   static_cast<unsigned>(ADOPT_COMMAND));
        return;
    }
    // mov edx,[eax+0x28] / mov eax,[eax+0x3c]   (eax = prevCar)
    const uint8_t fixed[] = {0x8B, 0x50, 0x28, 0x8B, 0x40, 0x3C};
    patch::write(ADOPT_COMMAND, fixed, sizeof fixed);
}
