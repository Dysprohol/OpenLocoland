// Pictures of ours in the bottom right corner of the window, drawn on the final picture just before
// it is presented (screen_scaling.cpp, where the FPS counter is drawn too): the menus are a 1024x768
// picture stretched over a bigger window keeping its proportions, so the window's corner - the black
// bar beside the picture on a wide screen - exists only in the final picture, out of reach of the
// menus' own sprites. Each picture is made once into a surface of its own and put on with a Blt keyed
// on its transparent colour: a GetDC or a lock of the window-sized picture every frame makes dgVoodoo
// copy it to memory and back (the main menu fell from 438 to 200 fps at 1920x1080 and from 432 to
// 138 at 2560x1440 with the version drawn that way).
//
// On the main menu: "OpenLocoland v0.2.x" (LOCOFIX_VERSION, "OpenLocoland dev" for a build of
// build.py) in the menu's own font - VID 2, fonts\font8x13.bmp, the green 8 x 13 letters of the
// menu's lists - at half its brightness, so the version can be seen in full screen too (the
// launcher's caption and the window's title bar have it, options.cpp and screen_scaling.cpp). It goes
// into the window's corner when a bar there has room for it (beside the picture on a wide screen,
// under it on a 5:4 one), else into the picture's own corner: the menu's background (VID 465, a metal
// wall) fills the whole picture, and that corner has only the wall's dark lower rim - the panel of the
// pages, the buttons and every text of the menu stay far from it in all six languages (looked at at
// 1024x768; the "Help" button is in the top left corner, a pipe in the bottom left one). Only on the
// main menu: the map whose script (menu.lgc) makes the seven buttons of VID 467 down its left side,
// and not while a briefing is open there (its picture is VID 501). A mission, its Esc menu (buttons
// VID 471, menutact.lgc), the end-of-mission screen and the editor have no 467.
//
// While the settings page is open - the main menu's or the Esc menu's, while the arrows of its
// minimap row (VID 952, game/maps/script/optfields.lgc) exist - the bottom of the window shows the
// interface as the game will put it there, in real pixels, as one picture: the state bar across the
// window at the interface's size the game would use on this window (interface_scale.cpp; from
// vid\barpreview.bmp, patch_data.make_bar_preview: the bar's own pictures laid out as the game lays them
// out, made bigger pixel by pixel as the bigger VIDs are), and on it, in the bottom right corner, the
// minimap's frame at the size the game would use (minimap.cpp: the player's choice as far as it fits),
// from vid\464preview1-3.bmp (patch_data.make_minimap_previews, the same frame as VIDs 464/950/951);
// for a minimap bigger than the interface a line above it says that a strip shows under the map (the
// pack's strings.ini [strings] MenuOptionsMinimapStrip, in the pack's own font). It follows a click on
// either row at once and goes with the page. The version gives way meanwhile. (A mission on a small map
// - mission 1, whose logical screen is stretched - has the normal size, stretched; the preview shows a
// big map's.) In the Esc menu the engine has the bar hidden and given up while the menu is open, so the
// real bar cannot show the change behind it; it is built with the new size when the menu closes.
#include "patch.h"
#include <ddraw.h>
#include <vector>

#pragma comment(lib, "gdi32.lib")

std::string language_code();                  // language.cpp
int minimap_size_for_screen(int screen_w);    // minimap.cpp
int interface_scale_for_window();             // interface_scale.cpp

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr int VID_MENU_BUTTONS = 467;
constexpr int VID_BRIEFING = 501;
constexpr int VID_MINIMAP_ROW = 952;   // the settings page's minimap row (its left arrow)
constexpr int MENU_HEIGHT = 768;
// fonts\font8x13.bmp: 16 x 16 cells of 9 x 14 px (a grey line right and below each), the letter
// in the top left 8 x 13 of its cell by its code (the pack's code page)
constexpr int CELL_W = 9, CELL_H = 14, GLYPH_W = 8, GLYPH_H = 13;
constexpr int MARGIN = 8;              // px from the window's (or the picture's) edges, times the size
constexpr uint32_t KEY_BLACK = 0, KEY_MAGENTA = 0xff00ff;   // patch_data.PREVIEW_KEY

int vid_count(int nvid) {
    auto* world = *reinterpret_cast<uint8_t**>(G_WORLD);
    if (!world || nvid >= *reinterpret_cast<int*>(world + 0x290)) return 0;
    auto* vid = reinterpret_cast<uint8_t**>(world + 0x294)[nvid];
    if (!vid) return 0;
    const int* count = reinterpret_cast<int*>(vid + 0x358);   // Vid.count, per army
    return count[0] + count[1] + count[2] + count[3];
}

bool main_menu_on_screen() { return vid_count(VID_MENU_BUTTONS) > 0 && vid_count(VID_BRIEFING) == 0; }
bool settings_page_open() { return vid_count(VID_MINIMAP_ROW) > 0; }

bool exists(const std::string& path) {
    const DWORD a = GetFileAttributesA(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

// The chosen pack's copy of a game file (lang\<code>\...), else the game folder's.
std::string pack_file(const char* rel) {
    const std::string pack = patch::game_folder() + "lang\\" + language_code() + "\\" + rel;
    return exists(pack) ? pack : patch::game_folder() + rel;
}

// A picture as 0x00RRGGBB, `key` where it is transparent.
struct Picture {
    int w = 0, h = 0;
    std::vector<uint32_t> pixels;
    uint32_t key = KEY_BLACK;
};

// A BMP file's pixels, top row first; empty when it cannot be read.
std::vector<uint32_t> load_bmp(const std::string& path, int& w, int& h) {
    std::vector<uint32_t> out;
    HBITMAP bitmap = static_cast<HBITMAP>(
        LoadImageA(nullptr, path.c_str(), IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION));
    if (!bitmap) return out;
    BITMAP info = {};
    GetObject(bitmap, sizeof info, &info);
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = info.bmWidth;
    bi.bmiHeader.biHeight = -info.bmHeight;   // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    out.resize(static_cast<size_t>(info.bmWidth) * info.bmHeight);
    HDC dc = GetDC(nullptr);
    const int rows = GetDIBits(dc, bitmap, 0, info.bmHeight, out.data(), &bi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, dc);
    DeleteObject(bitmap);
    if (rows != info.bmHeight) return {};
    for (uint32_t& p : out) p &= 0xffffff;
    w = info.bmWidth;
    h = info.bmHeight;
    return out;
}

// `text` (bytes of the font's code page) in font8x13 at `percent` of its colour, black around it.
Picture text_picture(const std::string& text, const std::string& font_file, int percent) {
    Picture p;
    int fw = 0, fh = 0;
    const std::vector<uint32_t> font = load_bmp(font_file, fw, fh);
    if (font.empty() || fw < 16 * CELL_W || fh < 16 * CELL_H) {
        patch::log("menu corner: no usable font %s", font_file.c_str());
        return p;
    }
    p.w = GLYPH_W * static_cast<int>(text.size());
    p.h = GLYPH_H;
    p.pixels.assign(static_cast<size_t>(p.w) * p.h, 0);
    for (size_t i = 0; i < text.size(); ++i) {
        const auto c = static_cast<unsigned char>(text[i]);
        const int cx = (c % 16) * CELL_W, cy = (c / 16) * CELL_H;
        for (int y = 0; y < GLYPH_H; ++y)
            for (int x = 0; x < GLYPH_W; ++x) {
                const uint32_t v = font[static_cast<size_t>(cy + y) * fw + cx + x];
                const uint32_t r = (v >> 16 & 0xff) * percent / 100, g = (v >> 8 & 0xff) * percent / 100,
                               b = (v & 0xff) * percent / 100;
                p.pixels[static_cast<size_t>(y) * p.w + i * GLYPH_W + x] = r << 16 | g << 8 | b;
            }
    }
    return p;
}

Picture bmp_picture(const std::string& path) {
    Picture p;
    p.pixels = load_bmp(path, p.w, p.h);
    p.key = KEY_MAGENTA;
    if (p.pixels.empty()) patch::log("menu corner: no %s", path.c_str());
    return p;
}

const GUID IID_DirectDraw7_ = {0x15e65ec0, 0x3b9c, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b}};

// One channel of 0xRRGGBB (shift 16, 8, 0) into the bits of `mask`.
uint32_t channel(uint32_t rgb, int shift, uint32_t mask) {
    if (!mask) return 0;
    int low = 0, bits = 0;
    while (!(mask >> low & 1)) ++low;
    while (low + bits < 32 && (mask >> (low + bits) & 1)) ++bits;
    const uint32_t v = (rgb >> shift & 0xff) * ((1u << bits) - 1) / 255;
    return v << low & mask;
}

uint32_t convert(uint32_t rgb, const DDPIXELFORMAT& f) {
    return channel(rgb, 16, f.dwRBitMask) | channel(rgb, 8, f.dwGBitMask) | channel(rgb, 0, f.dwBBitMask);
}

// A Picture made into a surface once (`size` times, pixel by pixel) and put on with one keyed Blt.
struct Overlay {
    Picture picture;
    bool loaded = false;
    IDirectDrawSurface7* surface = nullptr;
    int size = 0;
    DDPIXELFORMAT format = {};
    int failures = 0;

    void release() {
        if (surface) surface->Release();
        surface = nullptr;
        size = 0;
    }

    bool fill() {
        DDSURFACEDESC2 desc = {};
        desc.dwSize = sizeof desc;
        if (FAILED(surface->Lock(nullptr, &desc, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr))) return false;
        const DDPIXELFORMAT& f = desc.ddpfPixelFormat;
        const int bytes = static_cast<int>(f.dwRGBBitCount) / 8;
        for (int y = 0; y < picture.h * size; ++y) {
            auto* row = static_cast<uint8_t*>(desc.lpSurface) + static_cast<size_t>(y) * desc.lPitch;
            for (int x = 0; x < picture.w * size; ++x) {
                const uint32_t v = convert(picture.pixels[static_cast<size_t>(y / size) * picture.w + x / size], f);
                if (bytes == 2) reinterpret_cast<uint16_t*>(row)[x] = static_cast<uint16_t>(v);
                else if (bytes == 4) reinterpret_cast<uint32_t*>(row)[x] = v;
            }
        }
        surface->Unlock(nullptr);
        return bytes == 2 || bytes == 4;
    }

    IDirectDrawSurface7* ready(IDirectDrawSurface7* target, int wanted) {
        if (failures >= 3 || picture.pixels.empty()) return nullptr;
        DDPIXELFORMAT f = {};
        f.dwSize = sizeof f;
        if (FAILED(target->GetPixelFormat(&f))) return nullptr;
        if (surface && size == wanted && std::memcmp(&f, &format, sizeof f) == 0) return surface;
        release();
        IUnknown* unknown = nullptr;
        if (FAILED(target->GetDDInterface(reinterpret_cast<void**>(&unknown))) || !unknown) return nullptr;
        IDirectDraw7* dd = nullptr;
        unknown->QueryInterface(IID_DirectDraw7_, reinterpret_cast<void**>(&dd));
        unknown->Release();
        if (!dd) return nullptr;
        DDSURFACEDESC2 desc = {};
        desc.dwSize = sizeof desc;
        desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_CKSRCBLT;
        desc.dwWidth = picture.w * wanted;
        desc.dwHeight = picture.h * wanted;
        desc.ddpfPixelFormat = f;
        desc.ddckCKSrcBlt.dwColorSpaceLowValue = desc.ddckCKSrcBlt.dwColorSpaceHighValue = convert(picture.key, f);
        desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
        HRESULT hr = dd->CreateSurface(&desc, &surface, nullptr);
        if (FAILED(hr)) {
            desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
            hr = dd->CreateSurface(&desc, &surface, nullptr);
        }
        dd->Release();
        size = wanted;
        if (FAILED(hr) || !fill()) {
            if (FAILED(hr)) surface = nullptr;
            release();
            if (++failures == 3) patch::log("menu corner: cannot make a surface (%08X)", static_cast<unsigned>(hr));
            return nullptr;
        }
        format = f;
        return surface;
    }

    // Puts the picture with its top left at (x, y).
    void draw(IDirectDrawSurface7* target, int x, int y, int wanted) {
        draw_part(target, x, y, wanted, 0, picture.w * wanted);
    }

    // Puts `width` px of the picture (`wanted` times bigger) from its column `from` with their top left at (x, y).
    void draw_part(IDirectDrawSurface7* target, int x, int y, int wanted, int from, int width) {
        IDirectDrawSurface7* s = ready(target, wanted);
        if (!s) return;
        RECT to = {x, y, x + width, y + picture.h * wanted};
        RECT part = {from, 0, from + width, picture.h * wanted};
        const HRESULT hr = s->IsLost() == DDERR_SURFACELOST
                               ? DDERR_SURFACELOST
                               : target->Blt(&to, s, &part, DDBLT_WAIT | DDBLT_KEYSRC, nullptr);
        if (SUCCEEDED(hr)) {
            failures = 0;
        } else if (hr == DDERR_SURFACELOST) {   // a lost surface (Alt+Tab in full screen): filled again
            if (FAILED(s->Restore()) || !fill()) release();
        } else {
            release();
            if (++failures == 3) patch::log("menu corner: cannot draw (%08X)", static_cast<unsigned>(hr));
        }
    }
};

Overlay g_version;
Overlay g_preview[3];   // the minimap's frame at sizes 1-3
Overlay g_bar[3];       // the bottom of the screen with the state bar, for the interface's sizes 1-3
Overlay g_strip;        // "a strip shows under the map"

void load_once() {
    static bool done = false;
    if (done) return;
    done = true;
    g_version.picture = text_picture(std::string("OpenLocoland ") + LOCOFIX_VERSION,
                                     patch::game_folder() + "fonts\\font8x13.bmp", 50);
    for (int k = 0; k < 3; ++k) {
        char name[64];
        wsprintfA(name, "vid\\464preview%d.bmp", k + 1);
        g_preview[k].picture = bmp_picture(patch::game_folder() + name);
    }
    // vidarpreview.bmp (patch_data.make_bar_preview): the bar at the normal size on a screen 3840 px wide; for
    // the size s its middle 3840 / s px, which the Overlay makes s times bigger
    const Picture bar = bmp_picture(patch::game_folder() + "vid\\barpreview.bmp");
    for (int k = 0; k < 3 && bar.w > 0; ++k) {
        Picture& p = g_bar[k].picture;
        p.key = bar.key;
        p.w = (bar.w + k) / (k + 1);
        p.h = bar.h;
        const int from = (bar.w - p.w) / 2;
        p.pixels.resize(static_cast<size_t>(p.w) * p.h);
        for (int y = 0; y < p.h; ++y)
            for (int x = 0; x < p.w; ++x) p.pixels[static_cast<size_t>(y) * p.w + x] = bar.pixels[static_cast<size_t>(y) * bar.w + from + x];
    }
    char text[256] = {};
    GetPrivateProfileStringA("strings", "MenuOptionsMinimapStrip", "", text, sizeof text,
                             pack_file("strings.ini").c_str());
    g_strip.picture = text_picture(*text ? text : "A STRIP SHOWS UNDER THE MAP", pack_file("fonts\\font8x13.bmp"), 100);
}

}  // namespace

// screen_scaling.cpp, before the frame is presented: `target` holds the final picture of the
// window `window`, the menu's 1024x768 picture (stretched or not) at `picture`.
void draw_menu_version(IDirectDrawSurface7* target, const RECT& window, const RECT& picture) {
    if (!target) return;
    const bool page = settings_page_open();
    if (!page && !main_menu_on_screen()) return;
    load_once();
    // whole steps of the menu's stretch, so the letters stay sharp
    int size = (picture.bottom - picture.top) / MENU_HEIGHT;
    if (size < 1) size = 1;
    const int margin = MARGIN * size;
    if (page) {
        // the bar as it will stand across the window's bottom, at the interface's size the game would use there
        const int window_w = window.right - window.left;
        const int s = interface_scale_for_window();
        Overlay& bar = g_bar[(s >= 1 && s <= 3 ? s : 1) - 1];
        if (!bar.picture.pixels.empty()) {
            const int full = bar.picture.w * s, shown = full < window_w ? full : window_w;
            bar.draw_part(target, window.left + (window_w - shown) / 2, window.bottom - bar.picture.h * s, s,
                          (full - shown) / 2, shown);
        }
        // and on it, in the corner, the minimap's frame at its own size
        const int k = minimap_size_for_screen(window_w);
        Overlay& frame = g_preview[(k >= 1 && k <= 3 ? k : 1) - 1];
        if (frame.picture.pixels.empty()) return;
        const int top = window.bottom - frame.picture.h;
        frame.draw(target, window.right - frame.picture.w, top, 1);
        if (k > interface_scale_for_window() && !g_strip.picture.pixels.empty())   // bigger than the interface
            g_strip.draw(target, window.right - margin - g_strip.picture.w * size,
                         top - margin / 2 - g_strip.picture.h * size, size);
        return;
    }
    const int w = g_version.picture.w * size, h = g_version.picture.h * size;
    const bool right_bar = window.right - picture.right >= w + 2 * margin;
    const bool bottom_bar = window.bottom - picture.bottom >= h + 2 * margin;
    if (right_bar || bottom_bar)
        g_version.draw(target, window.right - margin - w, window.bottom - margin - h, size);
    else
        g_version.draw(target, picture.right - margin - w, picture.bottom - margin - h, size);
}
