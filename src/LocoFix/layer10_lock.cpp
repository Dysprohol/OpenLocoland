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
//
// The world's pass of a zoom view (zoom_view.cpp) leaves the interface's objects out - the messages are
// drawn later, in the interface's pass, into an overlay of ours - so there layer 10 draws nothing and its
// lock was a wait for the GPU and nothing else (7 ms a frame zoomed out to the whole of mission 13 in the
// test rig): an object the zoom's filter leaves out counts as none (fix 57, perf.h). A lock in which nothing
// is changed changes nothing on the picture (dgVoodoo puts back only what the CPU changed).
//
// The messages without a lock of the picture (fix 59). With messages in layer 10 the engine's lock comes after
// the Direct3D layers 5-9: a wait for the GPU whenever a message line is on the screen (the missions' objectives
// keep one there) - 2-4 ms a frame at 1920x1080 in the test rig. Layer 10 holds the message lines and the
// scripts' texts: Text objects whose font VID is a 16-bit software sprite (VID 14 and the bigger fonts of
// interface_texts.cpp), drawn letter by letter by SpriteVid_Draw - plain writes of the font's 16-bit pixels where
// the depth test lets them, no blending. Here layer 10 is drawn into memory of ours twice, once filled with one
// value and once with another, against the real depth buffer (locked as the engine locks it): a pixel is drawn
// where either pass changed it, its value the first pass's (the second pass, seeing the first one's depths, has
// the same last writer everywhere). The memory is watched for writes (MEM_WRITE_WATCH), so only the rows the
// letters touched are read and filled again. Those pixels are put on the picture as dgVoodoo's unlock would
// (engine.md "Locks under dgVoodoo"; minimap.cpp, fix 58, does the same): the letters' area copied by the GPU
// through a 16-bit texture and back through a clip list of the 2 x 2 blocks up-left of every pixel drawn, then
// the pixels with a colour-keyed Blt. A letter drawn in exactly the 16-bit colour already under it would be no
// change to dgVoodoo, and here its block gets the 16-bit colour: where the colour under it was finer (lights,
// fog, shadows drawn after the sprites' lock) up to 4 pixels would differ by less than one 16-bit step. The
// engine's way stays for anything else in layer 10 (an object that is not such a text, a text that switches to
// another VID with escape 0x1b or reads a script variable when drawn, a font that is no 16-bit sprite or would
// blend, a cursor drawn there), a 32-bit picture, a picture already locked; and when a surface of ours cannot be
// made or a Blt fails, the pixels go into the engine's lock as it would have drawn them. The drawing into memory of ours
// and the putting on are soft_overlay.cpp's, shared with the editor's overlay (fix 61).
#include "patch.h"
#include "perf.h"
#include "profile.h"
#include "soft_overlay.h"
#include <ddraw.h>
#include <algorithm>
#include <vector>

int zoom_draw_filter(const uint8_t* object);   // zoom_view.cpp: 0 = left out of this pass

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
        if (items[i] && (perf::off(57) || zoom_draw_filter(static_cast<const uint8_t*>(items[i])) != 0)) return 0;
    const uint8_t* cursor = *reinterpret_cast<uint8_t* const*>(G_CURSOR);
    if (cursor && !*reinterpret_cast<const int*>(cursor + CURSOR_SYSTEM))
        for (const uint8_t* o = cursor; o; o = *reinterpret_cast<uint8_t* const*>(o + 0x40)) {   // link
            const uint8_t* vid = *reinterpret_cast<uint8_t* const*>(o + 0x1c);
            if (vid && *reinterpret_cast<const int*>(vid + VID_LAYER) == LAYER) return 0;
        }
    return 1;
}

// ---- the messages without a lock (fix 59) ----

constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t DRAW_LAYER = 0x41fa70;     // World_DrawLayer
constexpr uintptr_t SURFACE_LOCK = 0x4123c0;   // Surface_Lock(surface, &pitch, flags)
constexpr uintptr_t TEXT_VTABLE = 0x475580, SPRITE_VID_VTABLE = 0x4755c8;

template <typename T> T& at(void* base, size_t offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset); }
template <typename T> const T& at(const void* base, size_t offset) {
    return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(base) + offset);
}

// A text the overlay can draw: its letters come from a 16-bit sprite font that does not blend, nothing else.
bool plain_text(const uint8_t* o) {
    if (at<uint32_t>(o, 0x28) & 0x8000) return true;   // invisible: World_DrawLayer leaves it out
    if (at<uintptr_t>(o, 0) != TEXT_VTABLE) return false;
    const uint8_t* vid = at<uint8_t*>(o, 0x1c);
    if (!vid || at<uintptr_t>(vid, 0) != SPRITE_VID_VTABLE || at<int>(vid, 0x64) <= 0x7e) return false;   // else GDI
    if ((at<uint16_t>(vid, 0x2a0) & 0xb) == 0xb) return false;   // Vid_BlendSpanDepth16
    if ((at<uint32_t>(o, 0x7c) & 0x70) == 0x60) return false;     // the text read from a script variable when drawn
    const char* text = at<char*>(o, 0x70);
    const int length = at<int>(o, 0x78);
    if (!text) return true;
    for (int i = 0; i < length; ++i)
        if (text[i] == 0x1b) return false;   // another VID's frame inside the text
    return true;
}

soft_overlay::Overlay g_overlay("layer 10");

// Layer 10 drawn by ours (see the top); false: nothing done, the engine's code runs.
bool draw_ours() {
    uint8_t* g = *reinterpret_cast<uint8_t**>(G_GRAPHICS);
    uint8_t* world = *reinterpret_cast<uint8_t**>(G_WORLD);
    if (perf::off(59) || !g || !world || (at<uint32_t>(g, 0) & 2) || at<void*>(g, 0x208)) return false;
    auto* back = at<IDirectDrawSurface7*>(g, 0xc04);
    DDSURFACEDESC2 bd = {};
    bd.dwSize = sizeof bd;
    if (!back || FAILED(back->GetSurfaceDesc(&bd)) || bd.ddpfPixelFormat.dwRGBBitCount != 16) return false;
    const int w = static_cast<int>(bd.dwWidth), h = static_cast<int>(bd.dwHeight);
    if (!g_overlay.make(w, h)) return false;
    // the depth buffer, as the engine locks it (0x40f62f)
    if (!at<void*>(g, 0x218)) {
        using Lock_t = void*(__stdcall*)(void* surface, int* pitch, unsigned flags);
        int& pitch = at<int>(g, 0x21c);
        at<void*>(g, 0x218) = reinterpret_cast<Lock_t>(SURFACE_LOCK)(at<void*>(g, 0xbf8), &pitch, 0);
        pitch /= 2;
    }
    const int kept_pitch = at<int>(g, 0x214);
    using DrawLayer_t = void(__fastcall*)(void* world, void* edx, int layer);
    for (int k = 0; k < 2; ++k) {
        at<void*>(g, 0x208) = g_overlay.pass(k);
        at<int>(g, 0x214) = w;
        reinterpret_cast<DrawLayer_t>(DRAW_LAYER)(world, nullptr, LAYER);
    }
    at<void*>(g, 0x208) = nullptr;
    at<int>(g, 0x214) = kept_pitch;
    if (g_overlay.collect() && !g_overlay.put(g, back)) {
        // the engine's way: its lock (left for the unlock after World_DrawLayer(10)) and the pixels drawn
        using Lock_t = void*(__stdcall*)(void* surface, int* pitch, unsigned flags);
        int& pitch = at<int>(g, 0x214);
        at<void*>(g, 0x208) = reinterpret_cast<Lock_t>(SURFACE_LOCK)(back, &pitch, 0);
        pitch /= 2;
        if (auto* bits = at<uint16_t*>(g, 0x208)) g_overlay.write_into(bits, pitch);
    }
    g_overlay.clear();   // the rows written filled again for the next frame
    return true;
}

// 1: layer 10 needs no lock of the engine (empty, or drawn by ours); 0: the engine's code.
int __cdecl layer10_route() {
    if (layer10_empty()) return 1;
    const uint8_t* world = *reinterpret_cast<uint8_t* const*>(G_WORLD);
    const uint8_t* list = world + WORLD_LAYERS + LAYER * 0x10;
    const int count = *reinterpret_cast<const int*>(list + 4);
    void* const* items = *reinterpret_cast<void* const* const*>(list + 0xc);
    for (int i = 0; i < count; ++i)
        if (items[i] && zoom_draw_filter(static_cast<const uint8_t*>(items[i])) != 0 &&
            !plain_text(static_cast<const uint8_t*>(items[i])))
            return 0;
    const uint8_t* cursor = *reinterpret_cast<uint8_t* const*>(G_CURSOR);
    if (cursor && !*reinterpret_cast<const int*>(cursor + CURSOR_SYSTEM))
        for (const uint8_t* o = cursor; o; o = *reinterpret_cast<uint8_t* const*>(o + 0x40)) {
            const uint8_t* vid = *reinterpret_cast<uint8_t* const*>(o + 0x1c);
            if (vid && *reinterpret_cast<const int*>(vid + VID_LAYER) == LAYER) return 0;
        }
    return draw_ours() ? 1 : 0;
}

uintptr_t g_lock_back_buffer = LOCK_BACK_BUFFER, g_back_buffer_locked = BACK_BUFFER_LOCKED;
uintptr_t g_after_layer10 = AFTER_LAYER10;

__declspec(naked) void lock_for_layer10() {
    __asm {
        pushad
        call layer10_route
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
