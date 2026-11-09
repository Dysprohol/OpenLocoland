// The fog and ground snow buffer big enough for the screen (BUGS.md F23, GitHub issue #8).
//
// Graphics_Init makes one 256x256 scratch texture, the grey texture (+0xbd4, P8 with a grey
// palette, R5G6B5 when the card has no P8). Two users write the screen into it at a quarter
// of its resolution, so it covers 1024x1024 pixels - enough for the original's largest mode,
// 1024x768:
// - Graphics_DrawFog 0x4103c0 (a region's fog, Region_Draw): the visible part of the region,
//   ceil(w / 4) x ceil(h / 4) texels, made from the depth buffer;
// - Graphics_DrawGroundSnow 0x410920 (ENV_GROUND_SNOW 0x40): the whole screen.
// On a bigger screen both wrote past the texture: a fog region over the whole view (region
// flag 8: missions l07, l13, l18; the editor's region OVER: THE WHOLE MAP) crashed the game at
// 1920x1080 within a minute (an access violation at 0x41083E). Both take the texture from
// +0xbd4, lock it and step through it with the pitch the lock returns, and Texture_Draw /
// Texture_DrawZ divide the texels by the texture's own width and height (+0x0c, +0x10), so a
// bigger texture is all they need.
//
// The grey texture itself stays 256x256: Graphics_DrawLight locks and fills it for every other
// light (a light is at most 0x200 to each side, whatever the screen), and a bigger one would only
// make each of those locks dearer (it measured slower, if within the spread of the runs). So fog
// and ground snow get a texture of their own:
// the smallest power of two holding a quarter of the chosen resolution on each side (512x512
// at 1920x1080, 1024x512 at 2560x1440, 1024x1024 at 3840x2160; a logical screen is never
// bigger), made like the grey one (P8 with the same palette, or what Texture_Construct falls
// back to), and put at +0xbd4 for the length of those two calls only. Up to 1024x1024 pixels
// none is made and nothing changes. It is made at the first Graphics_ReloadPalettes (the end of
// Graphics_Init) and, like the grey texture, made again at the later ones (after a surface
// restore). Should it not be made, fog and ground snow are left out on a screen the grey texture
// does not cover (instead of writing past it) - logged once. It is not released when the game
// ends (one texture, gone with the process).
#include "patch.h"

namespace {

constexpr uintptr_t RELOAD_PALETTES = 0x40eea0;     // Graphics_ReloadPalettes (thiscall)
constexpr uintptr_t DRAW_FOG = 0x4103c0;            // Graphics_DrawFog (thiscall, 10 stack dwords)
constexpr uintptr_t DRAW_GROUND_SNOW = 0x410920;    // Graphics_DrawGroundSnow (thiscall)
constexpr uintptr_t TEXTURE_CONSTRUCT = 0x4161b0;   // Texture_Construct(width, height, format, flags)
constexpr uintptr_t TEXTURE_SET_PALETTE = 0x415fc0; // Texture_SetPalette(uint entries[256])
constexpr uintptr_t CRT_NEW = 0x456b11;
constexpr size_t G_SCREEN_W = 0x20c, G_SCREEN_H = 0x210, G_GRAY_TEXTURE = 0xbd4;
constexpr size_t T_SURFACE = 0x04, T_FORMAT = 0x08, T_WIDTH = 0x0c, T_HEIGHT = 0x10;
constexpr size_t TEXTURE_SIZE = 0x1c;
constexpr int FORMAT_P8 = 0x29;
constexpr int ORIGINAL = 0x100;

using ReloadPalettes_t = void(__fastcall*)(uint8_t* g, void* edx);
using DrawFog_t = void(__fastcall*)(uint8_t* g, void* edx, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t,
                                    uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
using DrawGroundSnow_t = void(__fastcall*)(uint8_t* g, void* edx);
using Construct_t = uint8_t*(__fastcall*)(uint8_t* texture, void* edx, int width, int height, int format, uint32_t flags);
using SetPalette_t = void(__fastcall*)(uint8_t* texture, void* edx, const uint32_t* entries);
using Delete_t = void*(__fastcall*)(uint8_t* texture, void* edx, int flags);
using CrtNew_t = void*(__cdecl*)(size_t size);

ReloadPalettes_t g_reload_original = nullptr;
DrawFog_t g_fog_original = nullptr;
DrawGroundSnow_t g_snow_original = nullptr;
bool g_sized = false;          // the size was chosen (the first Graphics_ReloadPalettes)
int g_width = 0, g_height = 0; // the size of ours; 0 = none needed
uint8_t* g_texture = nullptr;  // ours (a game Texture), when made
bool g_warned = false;         // "left out" logged

template <class T> T& at(uint8_t* p, size_t offset) { return *reinterpret_cast<T*>(p + offset); }

// The smallest power of two holding a quarter of `pixels`, at least the original 256.
int texels_for(float pixels) {
    const int need = (static_cast<int>(pixels) + 3) / 4;
    int size = ORIGINAL;
    while (size < need && size < 4096) size *= 2;
    return size;
}

void destroy(uint8_t*& texture) {
    if (!texture) return;
    const auto delete_fn = *reinterpret_cast<Delete_t*>(*reinterpret_cast<uint8_t**>(texture));   // vtable slot 0
    delete_fn(texture, nullptr, 1);
    texture = nullptr;
}

// As Graphics_Init and Graphics_ReloadPalettes make the grey texture: P8 if the card has it
// (Texture_Construct falls back to a 16-bit format otherwise), with a palette of 256 greys.
uint8_t* make_texture(int width, int height) {
    auto* memory = static_cast<uint8_t*>(reinterpret_cast<CrtNew_t>(CRT_NEW)(TEXTURE_SIZE));
    if (!memory) return nullptr;
    uint8_t* t = reinterpret_cast<Construct_t>(TEXTURE_CONSTRUCT)(memory, nullptr, width, height, FORMAT_P8, 0);
    if (!at<void*>(t, T_SURFACE) || at<int>(t, T_WIDTH) < width || at<int>(t, T_HEIGHT) < height) {
        destroy(t);
        return nullptr;
    }
    if (at<int>(t, T_FORMAT) == FORMAT_P8) {
        uint32_t greys[256];
        for (uint32_t v = 0; v < 256; ++v) greys[v] = 0xff000000u | v << 16 | v << 8 | v;
        reinterpret_cast<SetPalette_t>(TEXTURE_SET_PALETTE)(t, nullptr, greys);
    }
    return t;
}

void __fastcall reload_palettes(uint8_t* g, void* edx) {
    g_reload_original(g, edx);
    if (!g_sized) {
        g_sized = true;
        g_width = texels_for(at<float>(g, G_SCREEN_W));
        g_height = texels_for(at<float>(g, G_SCREEN_H));
        if (g_width == ORIGINAL && g_height == ORIGINAL) g_width = g_height = 0;   // the grey texture covers it
        if (!g_width) return;
    } else if (!g_width || (g_texture && at<int>(g_texture, T_FORMAT) != FORMAT_P8)) {
        return;   // the engine makes only a P8 grey texture again; a 16-bit one keeps its picture
    }
    destroy(g_texture);
    g_texture = make_texture(g_width, g_height);
    if (g_texture)
        patch::log("fog buffer: %dx%d (%s) for the %.0fx%.0f screen", at<int>(g_texture, T_WIDTH),
                   at<int>(g_texture, T_HEIGHT), at<int>(g_texture, T_FORMAT) == FORMAT_P8 ? "P8" : "16-bit",
                   at<float>(g, G_SCREEN_W), at<float>(g, G_SCREEN_H));
    else
        patch::log("fog buffer: no %dx%d texture for the %.0fx%.0f screen", g_width, g_height,
                   at<float>(g, G_SCREEN_W), at<float>(g, G_SCREEN_H));
}

// The texture fog and ground snow are drawn through: ours when there is one, else the grey one
// when it covers the screen, else none.
uint8_t* buffer_for(uint8_t* g, const char* what) {
    uint8_t* t = g_texture ? g_texture : at<uint8_t*>(g, G_GRAY_TEXTURE);
    if (t && at<void*>(t, T_SURFACE) && at<int>(t, T_WIDTH) * 4 >= static_cast<int>(at<float>(g, G_SCREEN_W)) &&
        at<int>(t, T_HEIGHT) * 4 >= static_cast<int>(at<float>(g, G_SCREEN_H)))
        return t;
    if (!g_warned) {
        g_warned = true;
        patch::log("fog buffer: %s left out - the %dx%d texture does not cover the %.0fx%.0f screen", what,
                   t ? at<int>(t, T_WIDTH) : 0, t ? at<int>(t, T_HEIGHT) : 0, at<float>(g, G_SCREEN_W),
                   at<float>(g, G_SCREEN_H));
    }
    return nullptr;
}

void __fastcall draw_fog(uint8_t* g, void* edx, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e,
                         uint32_t f, uint32_t h, uint32_t i, uint32_t j, uint32_t k) {
    uint8_t* buffer = buffer_for(g, "fog");
    if (!buffer) return;
    uint8_t* gray = at<uint8_t*>(g, G_GRAY_TEXTURE);
    at<uint8_t*>(g, G_GRAY_TEXTURE) = buffer;
    g_fog_original(g, edx, a, b, c, d, e, f, h, i, j, k);
    at<uint8_t*>(g, G_GRAY_TEXTURE) = gray;
}

void __fastcall draw_ground_snow(uint8_t* g, void* edx) {
    uint8_t* buffer = buffer_for(g, "ground snow");
    if (!buffer) {
        // without ENV_GROUND_SNOW the original only resets the snow's growth: let it
        if (!(at<uint32_t>(g, 0xba8) & 0x40)) g_snow_original(g, edx);
        return;
    }
    uint8_t* gray = at<uint8_t*>(g, G_GRAY_TEXTURE);
    at<uint8_t*>(g, G_GRAY_TEXTURE) = buffer;
    g_snow_original(g, edx);
    at<uint8_t*>(g, G_GRAY_TEXTURE) = gray;
}

}  // namespace

void install_fog_buffer() {
    const uint8_t reload_start[] = {0x64, 0xA1, 0x00, 0x00, 0x00, 0x00};   // mov eax,fs:[0]
    const uint8_t fog_start[] = {0x81, 0xEC, 0xC0, 0x00, 0x00, 0x00};      // sub esp,0xc0
    const uint8_t snow_start[] = {0x81, 0xEC, 0xB4, 0x00, 0x00, 0x00};     // sub esp,0xb4
    if (!patch::check(RELOAD_PALETTES, reload_start, sizeof reload_start) ||
        !patch::check(DRAW_FOG, fog_start, sizeof fog_start) ||
        !patch::check(DRAW_GROUND_SNOW, snow_start, sizeof snow_start)) {
        patch::log("skipped fog buffer: unexpected bytes");
        return;
    }
    g_fog_original = reinterpret_cast<DrawFog_t>(
        patch::hook(DRAW_FOG, fog_start, sizeof fog_start, reinterpret_cast<void*>(&draw_fog), "Graphics_DrawFog"));
    g_snow_original = reinterpret_cast<DrawGroundSnow_t>(patch::hook(
        DRAW_GROUND_SNOW, snow_start, sizeof snow_start, reinterpret_cast<void*>(&draw_ground_snow), "Graphics_DrawGroundSnow"));
    g_reload_original = reinterpret_cast<ReloadPalettes_t>(patch::hook(
        RELOAD_PALETTES, reload_start, sizeof reload_start, reinterpret_cast<void*>(&reload_palettes),
        "Graphics_ReloadPalettes"));
}
