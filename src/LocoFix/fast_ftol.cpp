// A faster float-to-integer conversion for the whole engine.
//
// The exe converts every float to an integer through the C runtime's _ftol (crt_ftol,
// 0x456a0c, Visual C++ 6.0): it saves the FPU control word, sets rounding toward zero, stores
// st0 with fistp as a 64-bit integer, puts the old control word back and returns the value
// in edx:eax. Changing the control word twice stalls the FPU, and the engine calls it for
// every coordinate it turns into a pixel - 351 call sites, inside the software sprite
// drawing (SpriteVid_Draw), the light buffers (Graphics_DrawLight) and the minimap dots. At
// 2560x1440 in mission 13 it was 8% of the main thread's time (a sampling profile).
//
// SSE3's fisttp stores st0 truncated toward zero whatever the rounding mode, so the function
// becomes: sub esp,8 / fisttp qword [esp] / pop eax / pop edx / ret - the same 64-bit
// result for every value (out of range and NaN give the same integer indefinite, 0x8000...),
// st0 popped as before, ecx and the control word untouched. Only with SSE3 (every x86 CPU
// since 2005); without it the original stays.
#include "patch.h"

namespace {

constexpr uintptr_t CRT_FTOL = 0x456a0c;

}  // namespace

void install_fast_ftol() {
    // push ebp / mov ebp,esp / add esp,-0xc / wait / fnstcw [ebp-2] / wait / mov ax,[ebp-2] /
    // or ah,0xc / mov [ebp-4],ax / fldcw [ebp-4] / fistp qword [ebp-0xc] / fldcw [ebp-2] /
    // mov eax,[ebp-0xc] / mov edx,[ebp-8] / leave / ret
    const uint8_t expected[] = {0x55, 0x8B, 0xEC, 0x83, 0xC4, 0xF4, 0x9B, 0xD9, 0x7D, 0xFE, 0x9B, 0x66, 0x8B,
                                0x45, 0xFE, 0x80, 0xCC, 0x0C, 0x66, 0x89, 0x45, 0xFC, 0xD9, 0x6D, 0xFC, 0xDF,
                                0x7D, 0xF4, 0xD9, 0x6D, 0xFE, 0x8B, 0x45, 0xF4, 0x8B, 0x55, 0xF8, 0xC9, 0xC3};
    if (!patch::check(CRT_FTOL, expected, sizeof expected)) {
        patch::log("skipped fast ftol @ %08X: unexpected bytes", static_cast<unsigned>(CRT_FTOL));
        return;
    }
    if (!IsProcessorFeaturePresent(PF_SSE3_INSTRUCTIONS_AVAILABLE)) {
        patch::log("skipped fast ftol: no SSE3");
        return;
    }
    // sub esp,8 / fisttp qword [esp] / pop eax / pop edx / ret
    const uint8_t fast[] = {0x83, 0xEC, 0x08, 0xDD, 0x0C, 0x24, 0x58, 0x5A, 0xC3};
    patch::write(CRT_FTOL, fast, sizeof fast);
}
