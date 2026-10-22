// The whole start animation of objects created facing direction 0.
//
// Object_Construct picks the start animation (ANI_BIRTH when the VID has one and no level
// is loading, ANI_DEATH when it has no ANI_STAND, else ANI_STAND) and sets its frame range:
// firstFrame = frame = animFirstFrame[animation], but lastFrame = animFrameCount[animation]
// - 1, without the first frame. Object_SetDirection then computes the range for the
// object's direction - but returns early when the direction does not change, and a new
// object has direction 0. So an object created facing 0 (the default of script
// CreateSprite) and every object of a VID with one direction got lastFrame below
// firstFrame and played only the first frame of its birth (or death) animation.
//
// At 0x445a28 edx still holds firstFrame, so turning `mov edx, animFrameCount[...]` into
// `add edx, animFrameCount[...]` makes the following `dec edx` store firstFrame + count - 1.
#include "patch.h"

namespace {

constexpr uintptr_t LAST_FRAME = 0x445a28;   // in Object_Construct

}  // namespace

void install_birth_animation() {
    // mov [esi+8],edx / mov [esi+0xc],edx / mov edx,[eax+ecx*4+0x2f0] / mov eax,[esp+0x34] /
    // dec edx / push eax / mov ecx,esi / mov [esi+0x10],edx
    const uint8_t expected[] = {0x89, 0x56, 0x08, 0x89, 0x56, 0x0C, 0x8B, 0x94, 0x88, 0xF0, 0x02, 0x00,
                                0x00, 0x8B, 0x44, 0x24, 0x34, 0x4A, 0x50, 0x8B, 0xCE, 0x89, 0x56, 0x10};
    const uintptr_t start = LAST_FRAME - 6;
    if (!patch::check(start, expected, sizeof expected)) {
        patch::log("skipped birth animation fix @ %08X: unexpected bytes", static_cast<unsigned>(start));
        return;
    }
    const uint8_t add = 0x03;   // add edx, [eax+ecx*4+0x2f0]
    patch::write(LAST_FRAME, &add, 1);
}
