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
// is added to the picture (its black adds nothing), so the wall shows through between the letters.
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
    HDC dc;
    if (FAILED(target->GetDC(&dc))) return;
    BITMAPINFO bi = {};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = g_text.w;
    bi.bmiHeader.biHeight = -g_text.h;
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    // OR onto the picture: black adds nothing, so only the letters show
    StretchDIBits(dc, x, y, w, h, 0, 0, g_text.w, g_text.h, g_text.pixels.data(), &bi, DIB_RGB_COLORS, SRCPAINT);
    target->ReleaseDC(dc);
}
