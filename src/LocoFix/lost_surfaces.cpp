// Lost surfaces: the game no longer crashes when DirectDraw takes its surfaces away.
//
// In full screen the surfaces are lost whenever the game is not the window in front (Alt+Tab, a
// window or a notification over it, a screen capture taking the screen, the lock screen); in a
// window a display mode change does it. The engine's only answer is at the start of a frame:
// Graphics_BeginScene (0x414cd0) asks the front and back buffers IsLost and calls
// RestoreAllSurfaces and Graphics_ReloadPalettes, and goes on whatever the result. While the
// surfaces cannot be restored (the game is not in front yet) the frame is drawn on lost surfaces:
// the Direct3D calls fail ("3dBeginScene for PreTact", "DrawPrimitive" errors every frame), and
// the software sprites' lazy locks (Graphics_LockBackBuffer, Graphics_LockDepthBuffer and their
// copies inlined in SpriteVid_Draw and the other software drawing) log "Couldn't lock", leave
// backBufferBits / depthBits 0 and draw through them anyway: an access violation in
// SpriteVid_Draw (0x4516B9, 0x451D74 ...), "EXCEPTION!" in error.log, and the engine's own
// exception filter (0x419820) faults on top of it. Seen at 1920x1080 in full screen, in play and
// in the editor (BUGS.md).
//
// Two pieces here:
// - The call of Graphics_BeginScene in GameWorld_Run (0x4236b8) goes through begin_scene_run:
//   when the front, back or depth buffer is lost it calls RestoreAllSurfaces and
//   Graphics_ReloadPalettes itself (what the engine would do), and while that fails it returns
//   1 - GameWorld_Run then returns before the frame (its own path for that value, which the
//   original BeginScene never used): nothing is drawn, nothing moves, the game time is held as
//   the engine holds it while the game is not active (g_gameTime = g_prevGameTime) and the loop
//   sleeps 10 ms a turn. The first frame after a restore draws everything again.
// - A surface can still be lost in the middle of a frame. Lock and Unlock of the surfaces'
//   IDirectDrawSurface7 vtables (dgVoodoo's; each vtable once, found from the game's surfaces)
//   are guarded: a Lock called from the game's code or LocoFix's that fails gets a stand-in -
//   a buffer of the surface's size in memory, DD_OK - so the frame's software drawing goes into
//   it and is thrown away, and the matching Unlock only ends the stand-in. The next frame's
//   begin_scene_run restores the surfaces. A lock from anywhere else (dgVoodoo itself) gets the
//   real result.
// LocoFix.log says when the surfaces were lost, how many frames were skipped and when they
// came back; error.log no longer fills with the failed frames' errors.
#include "patch.h"

#include <ddraw.h>
#include <intrin.h>

namespace {

constexpr uintptr_t G_GRAPHICS = 0x4a9a14;          // Graphics* g_graphics
constexpr uintptr_t G_GAME_TIME = 0x4a9d64;         // g_gameTime, ms
constexpr uintptr_t G_PREV_GAME_TIME = 0x4a9d60;    // g_prevGameTime
constexpr uintptr_t BEGIN_SCENE = 0x414cd0;         // Graphics_BeginScene (thiscall, returns int)
constexpr uintptr_t RELOAD_PALETTES = 0x40eea0;     // Graphics_ReloadPalettes (thiscall)
constexpr uintptr_t RUN_CALL_BEGIN_SCENE = 0x4236b8;       // GameWorld_Run
constexpr uintptr_t LOADING_CALL_BEGIN_SCENE = 0x40f7b6;   // Graphics_ShowLoadingFrame
constexpr uintptr_t EXE_START = 0x400000, EXE_END = 0x608000;
constexpr size_t G_DDRAW = 0xbec, G_DEPTH_BUFFER = 0xbf8, G_FRONT_BUFFER = 0xc00, G_BACK_BUFFER = 0xc04;
constexpr int SLOT_LOCK = 25, SLOT_UNLOCK = 32;      // IDirectDrawSurface7 vtable
constexpr int MAX_TABLES = 4, MAX_STAND_INS = 16;

using BeginScene_t = int(__fastcall*)(uint8_t* g, void* edx);
using ReloadPalettes_t = void(__fastcall*)(uint8_t* g, void* edx);
using Lock_t = HRESULT(STDMETHODCALLTYPE*)(IDirectDrawSurface7*, RECT*, DDSURFACEDESC2*, DWORD, HANDLE);
using Unlock_t = HRESULT(STDMETHODCALLTYPE*)(IDirectDrawSurface7*, RECT*);

struct Table {   // a guarded vtable and its own Lock / Unlock
    void** vtable;
    Lock_t lock;
    Unlock_t unlock;
};
Table g_tables[MAX_TABLES];
int g_table_count = 0;

struct StandIn {   // the memory a failed lock of a surface was given
    IDirectDrawSurface7* surface;
    uint8_t* bits;
    size_t size;
    bool locked;
};
StandIn g_stand_ins[MAX_STAND_INS];

HMODULE g_self = nullptr;
bool g_lost = false;           // the surfaces were found lost and are not restored yet
int g_skipped = 0;             // frames skipped since then
int g_stand_in_locks = 0;      // locks given a stand-in since then
DWORD g_lost_since = 0;

uint8_t* graphics() { return *reinterpret_cast<uint8_t**>(G_GRAPHICS); }
template <class T> T* field(uint8_t* g, size_t offset) { return *reinterpret_cast<T**>(g + offset); }

const Table* table_of(void* object) {
    void** vtable = *static_cast<void***>(object);
    for (int i = 0; i < g_table_count; ++i)
        if (g_tables[i].vtable == vtable) return &g_tables[i];
    return nullptr;
}

bool from_game(void* return_address) {
    const auto a = reinterpret_cast<uintptr_t>(return_address);
    if (a >= EXE_START && a < EXE_END) return true;
    HMODULE module = nullptr;
    return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                              static_cast<LPCSTR>(return_address), &module) && module == g_self;
}

StandIn* stand_in_of(IDirectDrawSurface7* s, bool make) {
    StandIn* free_one = nullptr;
    for (auto& f : g_stand_ins) {
        if (f.surface == s) return &f;
        if (!f.surface && !free_one) free_one = &f;
    }
    if (!make || !free_one) return nullptr;
    free_one->surface = s;
    return free_one;
}

HRESULT STDMETHODCALLTYPE guarded_lock(IDirectDrawSurface7* s, RECT* rect, DDSURFACEDESC2* desc, DWORD flags, HANDLE event) {
    const Table* t = table_of(s);
    if (!t) return DDERR_GENERIC;   // cannot happen: only guarded vtables point here
    const HRESULT hr = t->lock(s, rect, desc, flags, event);
    if (SUCCEEDED(hr) || !desc || desc->dwSize < sizeof(DDSURFACEDESC2) || !from_game(_ReturnAddress())) return hr;
    DDSURFACEDESC2 d = {};
    d.dwSize = sizeof d;
    if (FAILED(s->GetSurfaceDesc(&d)) || !d.dwWidth || !d.dwHeight) return hr;
    DWORD bits_per_pixel = d.ddpfPixelFormat.dwRGBBitCount;   // a union: dwZBufferBitDepth of a z surface
    if (bits_per_pixel < 8 || bits_per_pixel > 32) bits_per_pixel = 16;
    const LONG pitch = static_cast<LONG>((d.dwWidth * (bits_per_pixel / 8) + 3) & ~3u);
    const size_t size = static_cast<size_t>(pitch) * d.dwHeight;
    StandIn* f = stand_in_of(s, true);
    if (!f || f->locked) return hr;
    if (f->size < size) {
        if (f->bits) VirtualFree(f->bits, 0, MEM_RELEASE);
        f->bits = static_cast<uint8_t*>(VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        f->size = f->bits ? size : 0;
        if (!f->bits) return hr;
    }
    f->locked = true;
    d.dwFlags |= DDSD_PITCH | DDSD_LPSURFACE;
    d.lPitch = pitch;
    d.lpSurface = f->bits;
    if (rect && rect->left >= 0 && rect->top >= 0 && static_cast<DWORD>(rect->left) < d.dwWidth &&
        static_cast<DWORD>(rect->top) < d.dwHeight)
        d.lpSurface = f->bits + rect->top * pitch + rect->left * static_cast<LONG>(bits_per_pixel / 8);
    *desc = d;
    if (g_stand_in_locks++ == 0)
        patch::log("lost surfaces: a lock of surface %p failed (%08lX) - drawn into a stand-in until they are restored",
                   static_cast<void*>(s), static_cast<unsigned long>(hr));
    return DD_OK;
}

HRESULT STDMETHODCALLTYPE guarded_unlock(IDirectDrawSurface7* s, RECT* rect) {
    if (StandIn* f = stand_in_of(s, false); f && f->locked) {
        f->locked = false;
        return DD_OK;
    }
    const Table* t = table_of(s);
    return t ? t->unlock(s, rect) : DDERR_GENERIC;
}

void guard(void* surface) {
    if (!surface || table_of(surface) || g_table_count >= MAX_TABLES) return;
    void** vtable = *static_cast<void***>(surface);
    if (vtable[SLOT_LOCK] == reinterpret_cast<void*>(&guarded_lock)) return;
    Table& t = g_tables[g_table_count];
    t.vtable = vtable;
    t.lock = reinterpret_cast<Lock_t>(vtable[SLOT_LOCK]);
    t.unlock = reinterpret_cast<Unlock_t>(vtable[SLOT_UNLOCK]);
    ++g_table_count;   // before the writes: a lock right after them finds its table
    void* lock = reinterpret_cast<void*>(&guarded_lock);
    void* unlock = reinterpret_cast<void*>(&guarded_unlock);
    patch::write(reinterpret_cast<uintptr_t>(&vtable[SLOT_LOCK]), &lock, sizeof lock);
    patch::write(reinterpret_cast<uintptr_t>(&vtable[SLOT_UNLOCK]), &unlock, sizeof unlock);
    patch::log("lost surfaces: Lock/Unlock of vtable %p guarded", static_cast<void*>(vtable));
}

void guard_surfaces(uint8_t* g) {
    guard(field<void>(g, G_FRONT_BUFFER));
    guard(field<void>(g, G_BACK_BUFFER));
    guard(field<void>(g, G_DEPTH_BUFFER));
}

bool is_lost(void* surface) {
    return surface && static_cast<IDirectDrawSurface7*>(surface)->IsLost() == DDERR_SURFACELOST;
}

bool any_lost(uint8_t* g) {
    return is_lost(field<void>(g, G_FRONT_BUFFER)) || is_lost(field<void>(g, G_BACK_BUFFER)) ||
           is_lost(field<void>(g, G_DEPTH_BUFFER));
}

int __fastcall begin_scene_run(uint8_t* g, void*) {
    guard_surfaces(g);
    if (any_lost(g)) {
        auto* ddraw = field<IDirectDraw7>(g, G_DDRAW);
        const HRESULT hr = ddraw ? ddraw->RestoreAllSurfaces() : DDERR_GENERIC;
        if (FAILED(hr) || any_lost(g)) {
            if (!g_lost) {
                g_lost = true;
                g_skipped = 0;
                g_lost_since = GetTickCount();
                patch::log("lost surfaces: lost, restore failed (%08lX) - frames skipped until they come back",
                           static_cast<unsigned long>(hr));
            }
            ++g_skipped;
            *reinterpret_cast<uint32_t*>(G_GAME_TIME) = *reinterpret_cast<uint32_t*>(G_PREV_GAME_TIME);
            Sleep(10);
            return 1;   // GameWorld_Run returns before the frame
        }
        reinterpret_cast<ReloadPalettes_t>(RELOAD_PALETTES)(g, nullptr);
        patch::log("lost surfaces: restored (%d frames skipped, %lu ms, %d locks given a stand-in)", g_skipped,
                   static_cast<unsigned long>(g_lost ? GetTickCount() - g_lost_since : 0), g_stand_in_locks);
        g_lost = false;
        g_skipped = 0;
        g_stand_in_locks = 0;
    }
    return reinterpret_cast<BeginScene_t>(BEGIN_SCENE)(g, nullptr);
}

int __fastcall begin_scene_loading(uint8_t* g, void*) {
    guard_surfaces(g);   // the first level loads before the first frame
    return reinterpret_cast<BeginScene_t>(BEGIN_SCENE)(g, nullptr);
}

bool redirect_call(uintptr_t at, uintptr_t target, void* to, const char* name) {
    uint8_t expected[5] = {0xE8};
    const int32_t rel = static_cast<int32_t>(target - (at + 5));
    std::memcpy(expected + 1, &rel, 4);
    if (!patch::check(at, expected, sizeof expected)) {
        patch::log("skipped %s @ %08X: unexpected bytes", name, static_cast<unsigned>(at));
        return false;
    }
    uint8_t code[5] = {0xE8};
    const int32_t to_rel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(to) - (at + 5));
    std::memcpy(code + 1, &to_rel, 4);
    patch::write(at, code, sizeof code);
    return true;
}

}  // namespace

void install_lost_surfaces() {
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCSTR>(&install_lost_surfaces), &g_self);
    if (redirect_call(RUN_CALL_BEGIN_SCENE, BEGIN_SCENE, reinterpret_cast<void*>(&begin_scene_run), "lost surfaces (frame)") &&
        redirect_call(LOADING_CALL_BEGIN_SCENE, BEGIN_SCENE, reinterpret_cast<void*>(&begin_scene_loading), "lost surfaces (loading)"))
        patch::log("lost surfaces: frames wait for a restore, failed locks get a stand-in");
}
