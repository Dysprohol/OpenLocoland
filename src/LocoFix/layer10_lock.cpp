// No second lock of the back buffer and the depth buffer for an empty layer 10.
//
// Graphics_DrawScene draws the ground (layers 0-1) with Direct3D, locks the back buffer and
// the depth buffer for the software sprites of layers 2-4, then draws layers 5-9 and the
// weather with Direct3D, and at the end locks both again for layer 10 (0x40f5f5 ...
// 0x40f669): World_DrawLayer(10) and the unlocks. A lock of a surface the GPU has drawn on
// makes dgVoodoo wait for the GPU and copy the whole surface to the CPU, and the unlock
// copies it back - at 2560x1440 the locks were the biggest part of a frame (a sampling
// profile of mission 13: Surface_Lock and the driver under it about a quarter of the main
// thread). Layer 10 holds only the on-screen messages (VID 14, the message font) and is
// empty most of the time (mission 13: layers 0, 2, 3, 6-9 and 12 hold objects; over 1000
// frames layer 10 had a message in about 100), so the second lock pair mostly copied two
// screens for nothing.
//
// When layer 10 has no object and no member of the cursor chain is drawn in it (the chain
// is drawn only without Windows cursors), the block is skipped: nothing is locked, and the
// unlocks after World_DrawLayer(10) find nothing locked, as they would have after drawing
// nothing. With anything in layer 10 the original code runs.
#include "patch.h"

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_CURSOR = 0x4a9d44;
constexpr uintptr_t LOCK_FOR_LAYER10 = 0x40f5f5;   // cmp [esi+0x208],ebp / jne 0x40f62f
constexpr uintptr_t LOCK_BACK_BUFFER = 0x40f5fd;   // the lock of the back buffer
constexpr uintptr_t BACK_BUFFER_LOCKED = 0x40f62f; // the depth buffer's turn
constexpr uintptr_t AFTER_LAYER10 = 0x40f669;      // the unlocks after World_DrawLayer(10)
constexpr int LAYER = 10;
constexpr int WORLD_LAYERS = 0x4c;                 // World: ObjectList[16], 0x10 each
constexpr int CURSOR_SYSTEM = 0x70;                // Cursor.systemCursors
constexpr int VID_LAYER = 0x33c;

int __cdecl layer10_empty() {
    const uint8_t* world = *reinterpret_cast<uint8_t* const*>(G_WORLD);
    const uint8_t* list = world + WORLD_LAYERS + LAYER * 0x10;
    const int count = *reinterpret_cast<const int*>(list + 4);
    void* const* items = *reinterpret_cast<void* const* const*>(list + 0xc);
    for (int i = 0; i < count; ++i)
        if (items[i]) return 0;
    const uint8_t* cursor = *reinterpret_cast<uint8_t* const*>(G_CURSOR);
    if (cursor && !*reinterpret_cast<const int*>(cursor + CURSOR_SYSTEM))
        for (const uint8_t* o = cursor; o; o = *reinterpret_cast<uint8_t* const*>(o + 0x40)) {   // link
            const uint8_t* vid = *reinterpret_cast<uint8_t* const*>(o + 0x1c);
            if (vid && *reinterpret_cast<const int*>(vid + VID_LAYER) == LAYER) return 0;
        }
    return 1;
}

uintptr_t g_lock_back_buffer = LOCK_BACK_BUFFER, g_back_buffer_locked = BACK_BUFFER_LOCKED;
uintptr_t g_after_layer10 = AFTER_LAYER10;

__declspec(naked) void lock_for_layer10() {
    __asm {
        pushad
        call layer10_empty
        test eax, eax
        popad
        jnz skip
        cmp dword ptr [esi + 0x208], ebp   // backBufferBits, ebp = 0: the original test
        jne locked
        jmp dword ptr [g_lock_back_buffer]
    locked:
        jmp dword ptr [g_back_buffer_locked]
    skip:
        jmp dword ptr [g_after_layer10]
    }
}

}  // namespace

void install_layer10_lock() {
    // cmp [esi+0x208],ebp / jne +0x32 / mov edx,[esi+0xc04]
    const uint8_t expected[] = {0x39, 0xAE, 0x08, 0x02, 0x00, 0x00, 0x75, 0x32, 0x8B, 0x96, 0x04, 0x0C, 0x00, 0x00};
    // push 0xa / call World_DrawLayer, right before AFTER_LAYER10
    const uint8_t draw[] = {0x6A, 0x0A, 0xE8, 0x07, 0x04, 0x01, 0x00};
    if (!patch::check(LOCK_FOR_LAYER10, expected, sizeof expected) ||
        !patch::check(AFTER_LAYER10 - sizeof draw, draw, sizeof draw)) {
        patch::log("skipped layer 10 lock: unexpected bytes");
        return;
    }
    patch::write_call(LOCK_FOR_LAYER10, nullptr, 0, reinterpret_cast<void*>(&lock_for_layer10), 8, true);
}
