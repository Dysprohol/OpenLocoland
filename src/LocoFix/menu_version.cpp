// "OpenLocoland v0.2.x" in the main menu, so the version can be seen in full screen too (the
// launcher's caption and the window's title bar have it, options.cpp and screen_scaling.cpp).
//
// The text is drawn in the menu's own font - VID 2, fonts\font8x13.bmp, the green 8 x 13 letters
// of the menu's lists - at half its brightness, on the final picture just before it is presented
// (screen_scaling.cpp, where the FPS counter is drawn too): the menu is a 1024x768 picture,
// stretched over a bigger window keeping its proportions, and the black bars beside it exist only
// in the final picture. The text goes into the bottom right corner of the window when a bar there
// has room for it (beside the picture on a wide screen, under it on a 5:4 one), else into the
// picture's own bottom right corner: the menu's background (VID 465, a metal wall) fills the whole
// picture, and that corner has only the wall's dark lower rim - the panel of the pages, the
// buttons and every text of the menu stay far from it in all six languages (looked at at
// 1024x768; the "Help" button is in the top left corner, a pipe in the bottom left one). The text
// is put on with its black as the colour key, so the wall shows through between the letters.
// It is not a menu item - nothing reacts to the mouse there.
//
// Only on the main menu: the map whose script (menu.lgc) makes the seven buttons of VID 467 down
// its left side, and not while a briefing is open there (its picture is VID 501). A mission, its
// Esc menu (buttons VID 471, menutact.lgc), the end-of-mission screen and the editor have no 467.
#include "patch.h"
#include <ddraw.h>
#include <vector>

#pragma comment(lib, "gdi32.lib")

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr int VID_MENU_BUTTONS = 467;
constexpr int VID_BRIEFING = 501;
constexpr int MENU_HEIGHT = 768;
// fonts\font8x13.bmp: 16 x 16 cells of 9 x 14 px (a grey line right and below each), the letter
// in the top left 8 x 13 of its cell by its code
constexpr int CELL_W = 9, CELL_H = 14, GLYPH_W = 8, GLYPH_H = 13;
constexpr int MARGIN = 8;        // px from the window's (or the picture's) edges, times the size
constexpr int DIM_PERCENT = 50;  // of the font's own colour

int vid_count(int nvid) {
    auto* world = *reinterpret_cast<uint8_t**>(G_WORLD);
    if (!world || nvid >= *reinterpret_cast<int*>(world + 0x290)) return 0;
    auto* vid = reinterpret_cast<uint8_t**>(world + 0x294)[nvid];
    if (!vid) return 0;
    const int* count = reinterpret_cast<int*>(vid + 0x358);   // Vid.count, per army
    return count[0] + count[1] + count[2] + count[3];
}

bool main_menu_on_screen() { return vid_count(VID_MENU_BUTTONS) > 0 && vid_count(VID_BRIEFING) == 0; }

// The text as a 32-bit picture, black where nothing is written; made once.
struct Text {
    int w = 0, h = 0;
    std::vector<uint32_t> pixels;
    bool tried = false;
};
Text g_text;

void make_text() {
    g_text.tried = true;
    const std::string text = std::string("OpenLocoland ") + LOCOFIX_VERSION;
    const std::string font_file = patch::game_folder() + "fonts\\font8x13.bmp";
    HBITMAP bitmap = static_cast<HBITMAP>(
        LoadImageA(nullptr, font_file.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION));
    if (!bitmap) {
        patch::log("menu version: no %s", font_file.c_str());
        return;
    }
    BITMAP info = {};
    GetObject(bitmap, sizeof info, &info);
    if (info.bmWidth < 16 * CELL_W || info.bmHeight < 8 * CELL_H) {
        DeleteObject(bitmap);
        patch::log("menu version: unexpected font picture %dx%d", info.bmWidth, info.bmHeight);
        return;
    }
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = info.bmWidth;
    bi.bmiHeader.biHeight = -info.bmHeight;   // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    std::vector<uint32_t> font(static_cast<size_t>(info.bmWidth) * info.bmHeight);
    HDC dc = GetDC(nullptr);
    const int rows = GetDIBits(dc, bitmap, 0, info.bmHeight, font.data(), &bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    DeleteObject(bitmap);
    if (rows != info.bmHeight) return;

    g_text.w = GLYPH_W * static_cast<int>(text.size());
    g_text.h = GLYPH_H;
    g_text.pixels.assign(static_cast<size_t>(g_text.w) * g_text.h, 0);
    for (size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        const int cx = (c % 16) * CELL_W, cy = (c / 16) * CELL_H;
        for (int y = 0; y < GLYPH_H; ++y)
            for (int x = 0; x < GLYPH_W; ++x) {
                const uint32_t p = font[static_cast<size_t>(cy + y) * info.bmWidth + cx + x] & 0xffffff;
                const uint32_t r = (p >> 16 & 0xff) * DIM_PERCENT / 100, g = (p >> 8 & 0xff) * DIM_PERCENT / 100,
                               b = (p & 0xff) * DIM_PERCENT / 100;
                g_text.pixels[static_cast<size_t>(y) * g_text.w + i * GLYPH_W + x] = r << 16 | g << 8 | b;
            }
    }
}

// The text drawn into a surface of its own once, put on the picture with a Blt keyed on black:
// a GetDC or a lock of the window-sized picture every frame makes dgVoodoo copy it to memory and
// back (the main menu fell from 438 to 200 fps at 1920x1080 and from 432 to 138 at 2560x1440).
const GUID IID_DirectDraw7_ = {0x15e65ec0, 0x3b9c, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b}};
IDirectDrawSurface7* g_surface = nullptr;
int g_surface_size = 0;
DDPIXELFORMAT g_surface_format = {};
int g_failures = 0;

void release_text() {
    if (g_surface) g_surface->Release();
    g_surface = nullptr;
    g_surface_size = 0;
}

// One channel of 0xRRGGBB (shift 16, 8, 0) into the bits of `mask`.
uint32_t channel(uint32_t rgb, int shift, uint32_t mask) {
    if (!mask) return 0;
    int low = 0, bits = 0;
    while (!(mask >> low & 1)) ++low;
    while (low + bits < 32 && (mask >> (low + bits) & 1)) ++bits;
    const uint32_t v = (rgb >> shift & 0xff) * ((1u << bits) - 1) / 255;
    return v << low & mask;
}

// The text, `size` times, into the surface (black, the colour key, where nothing is written).
bool fill(IDirectDrawSurface7* s, int size) {
    DDSURFACEDESC2 desc = {};
    desc.dwSize = sizeof desc;
    if (FAILED(s->Lock(nullptr, &desc, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr))) return false;
    const DDPIXELFORMAT& f = desc.ddpfPixelFormat;
    const int bytes = static_cast<int>(f.dwRGBBitCount) / 8;
    for (int y = 0; y < g_text.h * size; ++y) {
        auto* row = static_cast<uint8_t*>(desc.lpSurface) + static_cast<size_t>(y) * desc.lPitch;
        for (int x = 0; x < g_text.w * size; ++x) {
            const uint32_t rgb = g_text.pixels[static_cast<size_t>(y / size) * g_text.w + x / size];
            const uint32_t v = channel(rgb, 16, f.dwRBitMask) | channel(rgb, 8, f.dwGBitMask) | channel(rgb, 0, f.dwBBitMask);
            if (bytes == 2) reinterpret_cast<uint16_t*>(row)[x] = static_cast<uint16_t>(v);
            else if (bytes == 4) reinterpret_cast<uint32_t*>(row)[x] = v;
        }
    }
    s->Unlock(nullptr);
    return bytes == 2 || bytes == 4;
}

IDirectDrawSurface7* text_surface(IDirectDrawSurface7* target, int size) {
    if (g_failures >= 3) return nullptr;
    DDPIXELFORMAT format = {};
    format.dwSize = sizeof format;
    if (FAILED(target->GetPixelFormat(&format))) return nullptr;
    if (g_surface && g_surface_size == size && std::memcmp(&format, &g_surface_format, sizeof format) == 0) return g_surface;
    release_text();
    IUnknown* unknown = nullptr;
    if (FAILED(target->GetDDInterface(reinterpret_cast<void**>(&unknown))) || !unknown) return nullptr;
    IDirectDraw7* dd = nullptr;
    unknown->QueryInterface(IID_DirectDraw7_, reinterpret_cast<void**>(&dd));
    unknown->Release();
    if (!dd) return nullptr;
    DDSURFACEDESC2 desc = {};
    desc.dwSize = sizeof desc;
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CKSRCBLT;
    desc.dwWidth = g_text.w * size;
    desc.dwHeight = g_text.h * size;
    desc.ddpfPixelFormat = format;
    desc.ddckCKSrcBlt.dwColorSpaceLowValue = desc.ddckCKSrcBlt.dwColorSpaceHighValue = 0;   // black
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    HRESULT hr = dd->CreateSurface(&desc, &g_surface, nullptr);
    if (FAILED(hr)) {
        desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
        hr = dd->CreateSurface(&desc, &g_surface, nullptr);
    }
    dd->Release();
    if (FAILED(hr) || !fill(g_surface, size)) {
        if (FAILED(hr)) g_surface = nullptr;
        release_text();
        if (++g_failures == 3) patch::log("menu version: cannot make the text's surface (%08X)", static_cast<unsigned>(hr));
        return nullptr;
    }
    g_surface_size = size;
    g_surface_format = format;
    return g_surface;
}

}  // namespace

// screen_scaling.cpp, before the frame is presented: `target` holds the final picture of the
// window `window`, the menu's 1024x768 picture (stretched or not) at `picture`.
void draw_menu_version(IDirectDrawSurface7* target, const RECT& window, const RECT& picture) {
    if (!target || !main_menu_on_screen()) return;
    if (!g_text.tried) make_text();
    if (g_text.pixels.empty()) return;
    // whole steps of the menu's stretch, so the letters stay sharp
    int size = (picture.bottom - picture.top) / MENU_HEIGHT;
    if (size < 1) size = 1;
    const int w = g_text.w * size, h = g_text.h * size, margin = MARGIN * size;
    const bool right_bar = window.right - picture.right >= w + 2 * margin;
    const bool bottom_bar = window.bottom - picture.bottom >= h + 2 * margin;
    int x, y;
    if (right_bar || bottom_bar) {
        x = window.right - margin - w;
        y = window.bottom - margin - h;
    } else {
        x = picture.right - margin - w;
        y = picture.bottom - margin - h;
    }
    IDirectDrawSurface7* text = text_surface(target, size);
    if (!text) return;
    RECT to = {x, y, x + w, y + h};
    HRESULT hr = text->IsLost() == DDERR_SURFACELOST ? DDERR_SURFACELOST
                                                    : target->Blt(&to, text, nullptr, DDBLT_WAIT | DDBLT_KEYSRC, nullptr);
    if (SUCCEEDED(hr)) {
        g_failures = 0;
    } else if (hr == DDERR_SURFACELOST) {   // a lost surface (Alt+Tab in full screen): filled again next frame
        if (FAILED(text->Restore()) || !fill(text, size)) release_text();
    } else if (FAILED(hr)) {
        release_text();
        if (++g_failures == 3) patch::log("menu version: cannot draw the text (%08X)", static_cast<unsigned>(hr));
    }
}
