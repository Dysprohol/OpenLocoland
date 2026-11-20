// A texture of its own for every light of a frame (perf.h; docs/dev/changes.md fix 55).
//
// Graphics_DrawLight fills a scratch texture for each light and draws it at once - always one of the
// same two (the grey one and the rgb one, in turn: g_lightBufferToggle). Under dgVoodoo the texture
// is then still being drawn by the GPU when the next light but one locks it again, and the draw after
// that waits for the GPU and uploads it: in mission 13 at 1920x1080 (34 lights in view) the lights'
// 34 Texture_DrawZ calls took 5.5 ms a frame in the test rig, an ordinary textured draw 1.4 us. With a
// texture of its own for each light that wait is gone (0.4 ms for the 34).
//
// The engine still locks and fills its own scratch texture (light_fill.cpp) - so that texture keeps
// exactly the texels it always had, which the next light reuses where it writes nothing (texels whose
// samples fall outside the view). Then, instead of drawing it, the filled rectangle is copied into a
// texture of a pool - the same size and format (and for P8 the same grey palette) - and that one is
// drawn, with the engine's own Texture_DrawZ and the same rectangle: the GPU samples the same texels
// (the drawn part lies one texel inside the rectangle). The scratch texture is unlocked and never
// drawn, so it is never uploaded. A pool texture is used once per frame (World.frameCount); up to
// POOL lights of a kind a frame get one, more take them again from the first (still correct, only
// slower). The pool is made anew when the engine makes its grey texture anew (Graphics_ReloadPalettes,
// after a surface restore).
#include "patch.h"
#include "perf.h"
#include <ddraw.h>

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t TEXTURE_CONSTRUCT = 0x4161b0;   // Texture_Construct(width, height, format, flags)
constexpr uintptr_t TEXTURE_SET_PALETTE = 0x415fc0; // Texture_SetPalette(uint entries[256])
constexpr uintptr_t CRT_NEW = 0x456b11;
constexpr size_t G_GRAY_TEXTURE = 0xbd4, G_RGB_TEXTURE = 0xbd8;
constexpr size_t T_SURFACE = 0x04, T_FORMAT = 0x08, T_WIDTH = 0x0c, T_HEIGHT = 0x10, TEXTURE_SIZE = 0x1c;
constexpr size_t W_FRAME_COUNT = 0x20;
constexpr int P8 = 0x29;
constexpr int POOL = 64;
// Graphics_DrawLight's stack frame at the fill (light_fill.cpp)
constexpr int F_TEXTURE = 0x10, F_COLUMNS = 0x34, F_ROWS = 0x38;

using Construct_t = uint8_t*(__fastcall*)(uint8_t* texture, void* edx, int width, int height, int format, uint32_t flags);
using SetPalette_t = void(__fastcall*)(uint8_t* texture, void* edx, const uint32_t* entries);
using Delete_t = void*(__fastcall*)(uint8_t* texture, void* edx, int flags);
using CrtNew_t = void*(__cdecl*)(size_t size);

template <class T> T& at(uint8_t* p, size_t offset) { return *reinterpret_cast<T*>(p + offset); }

struct Pool {
    uint8_t* source = nullptr;   // the engine's texture the pool is made for
    int format = 0;
    uint8_t* textures[POOL] = {};
    int made = 0, next = 0;
};
Pool g_pools[2];   // for the grey and the rgb texture
int g_frame = -1;
uint8_t* g_gray = nullptr;   // the grey texture the pools were made with
bool g_failed_logged = false;

void destroy(uint8_t* t) {
    const auto delete_fn = *reinterpret_cast<Delete_t*>(*reinterpret_cast<uint8_t**>(t));   // vtable slot 0
    delete_fn(t, nullptr, 1);
}

void clear(Pool& p) {
    for (int i = 0; i < p.made; ++i) destroy(p.textures[i]);
    p = Pool{};
}

// As the engine makes its scratch textures (Graphics_Init, Graphics_ReloadPalettes): 256 x 256 in the
// format the engine's one has, a P8 one with the palette of 256 greys.
uint8_t* make_like(uint8_t* source) {
    auto* memory = static_cast<uint8_t*>(reinterpret_cast<CrtNew_t>(CRT_NEW)(TEXTURE_SIZE));
    if (!memory) return nullptr;
    const int format = at<int>(source, T_FORMAT);
    uint8_t* t = reinterpret_cast<Construct_t>(TEXTURE_CONSTRUCT)(memory, nullptr, at<int>(source, T_WIDTH),
                                                                   at<int>(source, T_HEIGHT), format, 0);
    if (!at<void*>(t, T_SURFACE) || at<int>(t, T_FORMAT) != format || at<int>(t, T_WIDTH) != at<int>(source, T_WIDTH) ||
        at<int>(t, T_HEIGHT) != at<int>(source, T_HEIGHT)) {
        destroy(t);
        return nullptr;
    }
    if (format == P8) {
        uint32_t greys[256];
        for (uint32_t v = 0; v < 256; ++v) greys[v] = 0xff000000u | v << 16 | v << 8 | v;
        reinterpret_cast<SetPalette_t>(TEXTURE_SET_PALETTE)(t, nullptr, greys);
    }
    return t;
}

// The pool texture for this light of `source`, or nullptr (the engine's texture is drawn then).
uint8_t* take(uint8_t* source) {
    uint8_t* g = *reinterpret_cast<uint8_t**>(G_GRAPHICS);
    uint8_t* gray = at<uint8_t*>(g, G_GRAY_TEXTURE);
    if (gray != g_gray) {   // the engine made its textures anew: so do we
        clear(g_pools[0]);
        clear(g_pools[1]);
        g_gray = gray;
    }
    const int kind = source == gray ? 0 : source == at<uint8_t*>(g, G_RGB_TEXTURE) ? 1 : -1;
    if (kind < 0) return nullptr;
    const int frame = at<int>(*reinterpret_cast<uint8_t**>(G_WORLD), W_FRAME_COUNT);
    if (frame != g_frame) {
        g_frame = frame;
        g_pools[0].next = g_pools[1].next = 0;
    }
    Pool& p = g_pools[kind];
    if (p.source != source || p.format != at<int>(source, T_FORMAT)) {
        clear(p);
        p.source = source;
        p.format = at<int>(source, T_FORMAT);
    }
    if (p.next == p.made && p.made < POOL) {
        uint8_t* t = make_like(source);
        if (t) p.textures[p.made++] = t;
        else if (!g_failed_logged) {
            g_failed_logged = true;
            patch::log("light textures: no texture like the engine's (format %d) - %d made", p.format, p.made);
        }
    }
    if (!p.made) return nullptr;
    if (p.next >= p.made) p.next = 0;   // more lights than textures: again from the first
    return p.textures[p.next++];
}

}  // namespace

// light_fill.cpp, after the fill, the engine's texture still locked (its rectangle from (0, 0): `bits`,
// `pitch` bytes a row): the rectangle copied into a pool texture, the engine's texture unlocked.
// Returns the texture to draw, or nullptr: the engine goes on as ever (unlocks and draws its own).
uint8_t* light_texture_after_fill(uint8_t* frame, const uint8_t* bits, int pitch) {
    if (perf::g_original) return nullptr;
    uint8_t* source = at<uint8_t*>(frame, F_TEXTURE);
    const int columns = at<int>(frame, F_COLUMNS), rows = at<int>(frame, F_ROWS);
    if (columns <= 0 || rows <= 0 || columns > at<int>(source, T_WIDTH) || rows > at<int>(source, T_HEIGHT)) return nullptr;
    uint8_t* t = take(source);
    if (!t) return nullptr;
    auto* surface = at<IDirectDrawSurface7*>(t, T_SURFACE);
    RECT r = {0, 0, columns, rows};
    DDSURFACEDESC2 d = {};
    d.dwSize = sizeof d;
    if (FAILED(surface->Lock(&r, &d, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr)) || !d.lpSurface) return nullptr;
    const size_t row_bytes = static_cast<size_t>(columns) * (at<int>(source, T_FORMAT) == P8 ? 1 : 2);
    for (int y = 0; y < rows; ++y)
        std::memcpy(static_cast<uint8_t*>(d.lpSurface) + static_cast<size_t>(y) * d.lPitch, bits + static_cast<size_t>(y) * pitch, row_bytes);
    surface->Unlock(&r);
    at<IDirectDrawSurface7*>(source, T_SURFACE)->Unlock(nullptr);   // as the engine unlocks it (0x412318)
    return t;
}
