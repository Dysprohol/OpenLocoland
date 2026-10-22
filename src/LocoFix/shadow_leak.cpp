// VidConverter_BuildShadowPoints (0x42b670) leaks the first buffer (the shadow points,
// width * height * 4 bytes) when the second allocation (the "visited" map) fails: it
// logs "dot without shadow" and returns 2 without freeing psVar3.
//
// Building_UpdateWorkshop calls it when a repair workshop (VID 104) takes over a wagon
// whose goal was a train of its new army (BUGS.md C1).
//
// BUGS.md S9: only triggers under out-of-memory conditions, and the picture is thrown
// away right after, so the practical impact is small - but the fix is a single free.
//
// At 0x42b769 the five bytes are `mov eax, 2` (B8 02 00 00 00) on the failure path,
// right after the three pops (edi, esi, ebp). By then psVar3 lives at [esp+8], so a
// trampoline frees it (when non-null) and then reproduces the original mov + return.
#include "patch.h"

namespace {

constexpr uintptr_t SHADOW_LEAK = 0x42b769;   // `mov eax, 2` on the failure path
constexpr uintptr_t CRT_DELETE   = 0x456a00;   // crt_delete (frees a heap block)
constexpr uintptr_t RETURN_POINT = 0x42b76e;   // `pop ebx`

}  // namespace

void install_shadow_leak() {
    // expected: mov eax, 2
    const uint8_t expected[] = {0xB8, 0x02, 0x00, 0x00, 0x00};
    if (!patch::check(SHADOW_LEAK, expected, sizeof expected)) {
        patch::log("skipped shadow leak fix @ %08X: unexpected bytes",
                   static_cast<unsigned>(SHADOW_LEAK));
        return;
    }
    // trampoline: free psVar3 (at [esp+8]) if non-null, then mov eax,2 and jump back
    auto* tramp = static_cast<uint8_t*>(
        VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!tramp) {
        patch::log("skipped shadow leak fix @ %08X: VirtualAlloc failed",
                   static_cast<unsigned>(SHADOW_LEAK));
        return;
    }
    size_t n = 0;
    tramp[n++] = 0x8B; tramp[n++] = 0x44; tramp[n++] = 0x24; tramp[n++] = 0x08; // mov eax,[esp+8]
    tramp[n++] = 0x85; tramp[n++] = 0xC0;                                          // test eax,eax
    tramp[n++] = 0x74; tramp[n++] = 0x09;                                          // je +9
    tramp[n++] = 0x50;                                                             // push eax
    tramp[n++] = 0xE8;                                                             // call crt_delete
    const int32_t rel = static_cast<int32_t>(CRT_DELETE - (reinterpret_cast<uintptr_t>(tramp) + n + 4));
    std::memcpy(tramp + n, &rel, 4); n += 4;
    tramp[n++] = 0x83; tramp[n++] = 0xC4; tramp[n++] = 0x04;                      // add esp,4
    tramp[n++] = 0xB8; tramp[n++] = 0x02; tramp[n++] = 0x00; tramp[n++] = 0x00; tramp[n++] = 0x00; // mov eax,2
    tramp[n++] = 0xE9;                                                             // jmp RETURN_POINT
    const int32_t back = static_cast<int32_t>(RETURN_POINT - (reinterpret_cast<uintptr_t>(tramp) + n + 4));
    std::memcpy(tramp + n, &back, 4); n += 4;

    // replace the 5 bytes at SHADOW_LEAK with a jump to the trampoline
    uint8_t jump[5];
    jump[0] = 0xE9;
    const int32_t jrel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(tramp) - (SHADOW_LEAK + 5));
    std::memcpy(jump + 1, &jrel, 4);
    patch::write(SHADOW_LEAK, jump, sizeof jump);
    patch::log("shadow leak fix installed @ %08X", static_cast<unsigned>(SHADOW_LEAK));
}
