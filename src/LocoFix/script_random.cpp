// Script Random(max) (extern 155) divides by zero for a negative max.
//
// In Script_CallExtern the case 0x9b at 0x440608 pops the argument and evaluates
// rand() % (max + 1):
//
//   44063a: call crt_rand        ; e8 bd 68 01 00
//   44063f: cdq
//   440640: inc edi              ; edi = max + 1
//   440641: lea ecx,[esp+0x34]
//   440645: idiv edi             ; rand() / (max + 1)
//   440647: push edx             ; remainder = result
//   440648: call ScriptValue_InitInt
//
// Random(-1) makes the divisor 0 and the idiv raises EXCEPTION_INT_DIVIDE_BY_ZERO,
// which the game does not handle - the process dies. Any max < 0 is affected:
// max = -2 divides by -1, which works but is meaningless.
//
// The fix replaces the five bytes of the rand() call with a jump to a trampoline that
// does the original work and clamps a non-positive divisor to 1 before the idiv, so a
// negative max returns 0 instead of crashing. The stack is untouched (a jump, not a
// call), so the lea of the local ScriptValue still points at the right slot.
#include "patch.h"

namespace {

constexpr uintptr_t RAND_CALL = 0x44063a;   // call crt_rand in the Random case
constexpr uintptr_t CRT_RAND  = 0x456efc;   // C runtime rand()
constexpr uintptr_t CONTINUE  = 0x440648;   // push edx / call ScriptValue_InitInt

}  // namespace

void install_script_random() {
    // call crt_rand
    const uint8_t expected[] = {0xE8, 0xBD, 0x68, 0x01, 0x00};
    if (!patch::check(RAND_CALL, expected, sizeof expected)) {
        patch::log("skipped script Random fix @ %08X: unexpected bytes",
                   static_cast<unsigned>(RAND_CALL));
        return;
    }

    auto* tramp = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp) {
        patch::log("skipped script Random fix: VirtualAlloc failed");
        return;
    }

    size_t n = 0;
    const auto here = [&] { return reinterpret_cast<uintptr_t>(tramp) + n; };

    tramp[n++] = 0xE8;                                   // call crt_rand
    int32_t rel = static_cast<int32_t>(CRT_RAND - (here() + 4));
    std::memcpy(tramp + n, &rel, 4); n += 4;

    tramp[n++] = 0x99;                                   // cdq
    tramp[n++] = 0x47;                                   // inc edi  (max + 1)
    tramp[n++] = 0x85; tramp[n++] = 0xFF;                // test edi,edi
    tramp[n++] = 0x75; tramp[n++] = 0x05;                // jne +5 (only a zero divisor is fatal)
    tramp[n++] = 0xBF;                                   // mov edi,1
    tramp[n++] = 0x01; tramp[n++] = 0x00; tramp[n++] = 0x00; tramp[n++] = 0x00;
    tramp[n++] = 0x8D; tramp[n++] = 0x4C; tramp[n++] = 0x24; tramp[n++] = 0x34;  // lea ecx,[esp+0x34]
    tramp[n++] = 0xF7; tramp[n++] = 0xFF;                // idiv edi
    tramp[n++] = 0x52;                                   // push edx
    tramp[n++] = 0xE9;                                   // jmp CONTINUE
    rel = static_cast<int32_t>(CONTINUE - (here() + 4));
    std::memcpy(tramp + n, &rel, 4); n += 4;

    uint8_t jump[5];
    jump[0] = 0xE9;
    rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(tramp) - (RAND_CALL + 5));
    std::memcpy(jump + 1, &rel, 4);
    patch::write(RAND_CALL, jump, sizeof jump);
    patch::log("script Random fix installed @ %08X (negative max returns 0)",
               static_cast<unsigned>(RAND_CALL));
}
