// The depth buffer's fill and the ground's heights by the CPU instead of DirectDraw Blts (docs/dev/changes.md
// fix 56).
//
// GroundVid_Draw puts the height texture of every ground tile that has one (imageFlags 4 - the missions' grounds:
// seen on missions 1, 3, 7, 13 and 18) into the system-memory depth buffer with Graphics_BlitToDepthBuffer
// (0x415de0): the depth buffer unlocked, then IDirectDrawSurface7::Blt of the tile's rectangle of its D16
// texture, DDBLT_WAIT, through the depth buffer's clipper (the view, Graphics_SetViewport). When the view is not
// all map (zoomed out to the whole map, a camera past an edge), Graphics_Clear fills the depth buffer with the
// engine's empty depth (0x3ff) first, a Blt with DDBLT_DEPTHFILL. Under dgVoodoo the first lock of the depth
// buffer after such Blts (Graphics_DrawScene, 0x40f2c8, for the software sprites) waits for them and copies the
// buffer back: 1.7 ms a frame on mission 13 at 1920x1080 in the test rig, 4.8 ms zoomed out to the whole map.
//
// Here the same fill and the same rectangles are done by the CPU into the locked depth buffer, which stays
// locked (depthBits) for the sprites as the engine's own lock would leave it: the same 16-bit values at the same
// places, clipped by the same clipper's rectangles. A Blt that is not a plain copy - another size (a stretch),
// a rectangle outside the texture, a surface that is not 16-bit, no clipper with a rectangle list - and a
// failed lock go the engine's way (logged, at most 4 times). perf::off(56) (test commands 40053, 40056) switches
// it back.
#include "patch.h"
#include "perf.h"
#include <ddraw.h>
#include <algorithm>
#include <vector>

namespace {

constexpr uintptr_t BLIT_TO_DEPTH = 0x415de0;   // Graphics_BlitToDepthBuffer (thiscall: dst, src, texture)
constexpr size_t G_DEPTH_BITS = 0x218, G_DEPTH_PITCH = 0x21c, G_DEPTH_BUFFER = 0xbf8;
constexpr size_t TEXTURE_SURFACE = 0x04;

template <typename T> T& at(void* base, size_t offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset); }

using Blit_t = int(__fastcall*)(void* g, void* edx, RECT* dst, RECT* src, void* texture);
Blit_t g_blit = nullptr;
bool g_logged = false;
int g_fallbacks_logged = 0;

int engine_blit(uint8_t* g, void* edx, RECT* dst, RECT* src, void* texture, const char* why) {
    if (g_fallbacks_logged < 4) {
        ++g_fallbacks_logged;
        patch::log("depth blits: %s - the engine's Blt (%ld,%ld-%ld,%ld from %ld,%ld-%ld,%ld)", why, dst ? dst->left : 0, dst ? dst->top : 0,
                   dst ? dst->right : 0, dst ? dst->bottom : 0, src ? src->left : 0, src ? src->top : 0, src ? src->right : 0,
                   src ? src->bottom : 0);
    }
    return g_blit(g, edx, dst, src, texture);
}

// The depth buffer's clip rectangles (its clipper's list), in surface coordinates.
bool clip_rects(IDirectDrawSurface7* depth, std::vector<RECT>& rects) {
    IDirectDrawClipper* clipper = nullptr;
    if (FAILED(depth->GetClipper(&clipper)) || !clipper) return false;
    DWORD size = 0;
    bool ok = SUCCEEDED(clipper->GetClipList(nullptr, nullptr, &size)) && size >= sizeof(RGNDATAHEADER);
    if (ok) {
        std::vector<uint8_t> data(size);
        auto* region = reinterpret_cast<RGNDATA*>(data.data());
        ok = SUCCEEDED(clipper->GetClipList(nullptr, region, &size)) && region->rdh.iType == RDH_RECTANGLES;
        if (ok) {
            const auto* r = reinterpret_cast<const RECT*>(region->Buffer);
            rects.assign(r, r + region->rdh.nCount);
        }
    }
    clipper->Release();
    return ok;
}

int __fastcall blit_to_depth(uint8_t* g, void* edx, RECT* dst, RECT* src, void* texture) {
    if (perf::off(56) || !dst || !src || !texture) return g_blit(g, edx, dst, src, texture);
    auto* depth = at<IDirectDrawSurface7*>(g, G_DEPTH_BUFFER);
    auto* tex = at<IDirectDrawSurface7*>(texture, TEXTURE_SURFACE);
    const LONG w = dst->right - dst->left, h = dst->bottom - dst->top;
    if (!depth || !tex) return engine_blit(g, edx, dst, src, texture, "no surface");
    if (w != src->right - src->left || h != src->bottom - src->top || w <= 0 || h <= 0)
        return engine_blit(g, edx, dst, src, texture, "not a plain copy");
    DDSURFACEDESC2 td = {};
    td.dwSize = sizeof td;
    DDSURFACEDESC2 dd = {};
    dd.dwSize = sizeof dd;
    std::vector<RECT> clips;
    if (FAILED(tex->GetSurfaceDesc(&td)) || FAILED(depth->GetSurfaceDesc(&dd)) || td.ddpfPixelFormat.dwZBufferBitDepth != 16 ||
        dd.ddpfPixelFormat.dwZBufferBitDepth != 16)
        return engine_blit(g, edx, dst, src, texture, "not 16-bit depth surfaces");
    if (src->left < 0 || src->top < 0 || static_cast<DWORD>(src->right) > td.dwWidth || static_cast<DWORD>(src->bottom) > td.dwHeight)
        return engine_blit(g, edx, dst, src, texture, "a rectangle outside the texture");
    if (!clip_rects(depth, clips)) return engine_blit(g, edx, dst, src, texture, "no clip list");
    // the depth buffer locked as the engine locks it (Graphics_DrawScene: pitch in shorts)
    if (!at<short*>(g, G_DEPTH_BITS)) {
        DDSURFACEDESC2 l = {};
        l.dwSize = sizeof l;
        if (FAILED(depth->Lock(nullptr, &l, DDLOCK_WAIT, nullptr)) || !l.lpSurface)
            return engine_blit(g, edx, dst, src, texture, "the depth buffer not locked");
        at<short*>(g, G_DEPTH_BITS) = static_cast<short*>(l.lpSurface);
        at<int>(g, G_DEPTH_PITCH) = l.lPitch / 2;
    }
    DDSURFACEDESC2 tl = {};
    tl.dwSize = sizeof tl;
    if (FAILED(tex->Lock(nullptr, &tl, DDLOCK_WAIT | DDLOCK_READONLY, nullptr)) || !tl.lpSurface) {
        depth->Unlock(nullptr);   // the engine's way, which unlocks first
        at<short*>(g, G_DEPTH_BITS) = nullptr;
        return engine_blit(g, edx, dst, src, texture, "the texture not locked");
    }
    short* out = at<short*>(g, G_DEPTH_BITS);
    const int pitch = at<int>(g, G_DEPTH_PITCH);
    for (const RECT& c : clips) {
        const LONG x0 = (std::max)({dst->left, c.left, 0L}), y0 = (std::max)({dst->top, c.top, 0L});
        const LONG x1 = (std::min)({dst->right, c.right, static_cast<LONG>(dd.dwWidth)});
        const LONG y1 = (std::min)({dst->bottom, c.bottom, static_cast<LONG>(dd.dwHeight)});
        for (LONG y = y0; y < y1; ++y) {
            const auto* from = reinterpret_cast<const short*>(static_cast<const uint8_t*>(tl.lpSurface) +
                                                              (src->top + (y - dst->top)) * tl.lPitch) +
                               src->left + (x0 - dst->left);
            if (x1 > x0) std::memcpy(out + y * pitch + x0, from, (x1 - x0) * sizeof(short));
        }
    }
    tex->Unlock(nullptr);
    if (!g_logged) {
        g_logged = true;
        patch::log("depth blits: the ground's heights copied by the CPU (%d clip rectangle%s)", static_cast<int>(clips.size()),
                   clips.size() == 1 ? "" : "s");
    }
    return 0;
}

// Graphics_Clear's depth fill (0x4158b6 ... 0x4158ff): the depth buffer unlocked, then a Blt with DDBLT_DEPTHFILL
// of 0x3ff (the engine's empty depth) through its clipper. Graphics_DrawScene clears when the view is not all
// map (zoomed out to the whole map, a camera past an edge): done with the CPU into the locked depth buffer,
// so the heights copied after it need no lock that waits for the GPU's fill.
constexpr uintptr_t CLEAR_DEPTH = 0x4158b6, CLEAR_DEPTH_END = 0x4158ff;
constexpr size_t CLEAR_DEPTH_SIZE = CLEAR_DEPTH_END - CLEAR_DEPTH;
uint8_t* g_clear_original = nullptr;   // a copy of the engine's instructions (no relative call in them), then a jump back
uintptr_t g_clear_end = CLEAR_DEPTH_END;

bool fill_depth(uint8_t* g) {
    auto* depth = at<IDirectDrawSurface7*>(g, G_DEPTH_BUFFER);
    DDSURFACEDESC2 dd = {};
    dd.dwSize = sizeof dd;
    std::vector<RECT> clips;
    if (!depth || FAILED(depth->GetSurfaceDesc(&dd)) || dd.ddpfPixelFormat.dwZBufferBitDepth != 16 || !clip_rects(depth, clips))
        return false;
    if (!at<short*>(g, G_DEPTH_BITS)) {
        DDSURFACEDESC2 l = {};
        l.dwSize = sizeof l;
        if (FAILED(depth->Lock(nullptr, &l, DDLOCK_WAIT, nullptr)) || !l.lpSurface) return false;
        at<short*>(g, G_DEPTH_BITS) = static_cast<short*>(l.lpSurface);
        at<int>(g, G_DEPTH_PITCH) = l.lPitch / 2;
    }
    short* out = at<short*>(g, G_DEPTH_BITS);
    const int pitch = at<int>(g, G_DEPTH_PITCH);
    for (const RECT& c : clips) {
        const LONG x0 = (std::max)(c.left, 0L), y0 = (std::max)(c.top, 0L);
        const LONG x1 = (std::min)(c.right, static_cast<LONG>(dd.dwWidth)), y1 = (std::min)(c.bottom, static_cast<LONG>(dd.dwHeight));
        for (LONG y = y0; y < y1; ++y) std::fill(out + y * pitch + x0, out + y * pitch + (std::max)(x1, x0), static_cast<short>(0x3ff));
    }
    return true;
}

int __fastcall clear_depth(uint8_t* g) {   // 1: done; 0: the engine's instructions
    if (perf::off(56)) return 0;
    if (fill_depth(g)) return 1;
    if (g_fallbacks_logged < 4) {
        ++g_fallbacks_logged;
        patch::log("depth blits: the depth fill not done by the CPU - the engine's Blt");
    }
    return 0;
}

__declspec(naked) void clear_depth_call() {
    __asm {
        mov ecx, esi       // the Graphics
        call clear_depth
        test eax, eax
        jz engine
        jmp dword ptr [g_clear_end]
    engine:
        jmp dword ptr [g_clear_original]
    }
}

}  // namespace

void install_depth_blits() {
    // push esi / mov esi,ecx / mov eax,[esi+0x218]
    const uint8_t start[] = {0x56, 0x8B, 0xF1, 0x8B, 0x86, 0x18, 0x02, 0x00, 0x00};
    g_blit = reinterpret_cast<Blit_t>(
        patch::hook(BLIT_TO_DEPTH, start, sizeof start, reinterpret_cast<void*>(&blit_to_depth), "Graphics_BlitToDepthBuffer"));
    // mov eax,[esi+0x218] / test eax,eax / jz +0x1b / mov eax,[esi+0xbf8] / push 0 / push eax / mov ecx,[eax] /
    // call [ecx+0x80] / mov [esi+0x218],0 / mov esi,[esi+0xbf8] / lea eax,[esp+0x14] / push eax / push 0x3000000 /
    // push 0 / mov [esp+0x70],0x3ff / mov edx,[esi] / push 0 / push 0 / push esi / call [edx+0x14]
    const uint8_t fill[CLEAR_DEPTH_SIZE] = {0x8B, 0x86, 0x18, 0x02, 0x00, 0x00, 0x85, 0xC0, 0x74, 0x1B, 0x8B, 0x86, 0xF8, 0x0B, 0x00,
                                            0x00, 0x6A, 0x00, 0x50, 0x8B, 0x08, 0xFF, 0x91, 0x80, 0x00, 0x00, 0x00, 0xC7, 0x86, 0x18,
                                            0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8B, 0xB6, 0xF8, 0x0B, 0x00, 0x00, 0x8D, 0x44,
                                            0x24, 0x14, 0x50, 0x68, 0x00, 0x00, 0x00, 0x03, 0x6A, 0x00, 0xC7, 0x44, 0x24, 0x70, 0xFF,
                                            0x03, 0x00, 0x00, 0x8B, 0x16, 0x6A, 0x00, 0x6A, 0x00, 0x56, 0xFF, 0x52, 0x14};
    if (!g_blit || !patch::check(CLEAR_DEPTH, fill, sizeof fill)) {
        patch::log("skipped the depth fill by the CPU: unexpected bytes");
        return;
    }
    g_clear_original = static_cast<uint8_t*>(VirtualAlloc(nullptr, sizeof fill + 5, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!g_clear_original) return;
    std::memcpy(g_clear_original, fill, sizeof fill);   // its one jump (jz +0x1b) stays inside the copy
    g_clear_original[sizeof fill] = 0xE9;
    const int32_t back = static_cast<int32_t>(CLEAR_DEPTH_END - (reinterpret_cast<uintptr_t>(g_clear_original) + sizeof fill + 5));
    std::memcpy(g_clear_original + sizeof fill + 1, &back, 4);
    patch::write_call(CLEAR_DEPTH, nullptr, 0, reinterpret_cast<void*>(&clear_depth_call), 5, true);   // the rest is never reached
}
