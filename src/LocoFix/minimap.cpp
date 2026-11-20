// The minimap: its rectangle fitted to the frame's opening, the view's highlight kept on the
// map and big enough to find, and the whole minimap bigger on a high screen.
//
// The minimap is the state bar's object 73, VID 464 (vid\464map.vid, a 104 x 78 brass frame
// with a dark green inside), put by Player_ShowStateBar in the bottom right corner of the view.
// Each frame Player_UpdateStateBar takes the rectangle inside the frame (Minimap_PlaceOver) and
// Minimap_Draw puts every rail point into it as a pixel (Minimap_PutPoint), the units as 2 x 2
// dots (Minimap_PutDot) and the buildings as 4 x 4 (Minimap_PutBigDot), all scaled by the map's
// size, and brightens the part of the map in view (Graphics_BrightenRect). A click in the
// rectangle moves the camera there (Minimap_Click), a right click there is an order to that
// point - both scaled from the same rectangle.
//
// Two faults of the engine (BUGS.md F26):
// - Minimap_PlaceOver puts the rectangle 10 px below the top of the frame's picture, while the
//   opening of the picture starts at row 9 (columns 8-97, rows 9-73): the rectangle is a row too
//   low - its top row of the opening is never used, its bottom row lies on the opening's dark
//   edge, so are the dots of what stands at the map's bottom edge.
// - the highlight is the whole view, not clamped to the map: the camera scrolls 48 px past the
//   map's bottom (Player_ShowStateBar's boundsBottom = map height + the bar's 50 - 2, so the
//   map's last rows can come up over the bar), and the highlight followed it onto the frame's
//   border - 3 rows at 1024x768 on mission 13 (a map 2304 high; a lower map more). On a big map
//   it is a few pixels in all (about 9 x 5 on 10240 x 10240), half of them on the border.
// Ours: the rectangle one row higher, the highlight the part of the map in view, at least
// 7 x 5 px (times the size below) around the view's middle and inside the rectangle; the dots
// kept inside the rectangle too.
//
// The size follows the interface's (interface_scale.cpp) unless the player chose one on the game's
// settings page (the main menu's and the Esc menu's; game/maps/script/optfields.lgc): the script
// keeps it with SetReg next to the sound and music settings, the value MinimapSize of the Players key
// (player_registry.cpp: HKCU\SOFTWARE\Gromada\Locoland\Players) - "0" or absent: as the interface
// (the default; at the normal interface the original size), "1", "2" or "3": that size whatever the
// interface's. It is read each time the bar is built - at a mission's start and when the Esc menu
// closes (the bar is given up while it is hidden, so it is built again) - and used only where it fits
// the game's screen: never so wide that the frame would reach the bar's controls (the slots end 228
// px right of the screen's middle, times the interface's scale), else the biggest that fits (a small
// map's logical screen, screen_scaling.cpp - mission 1: 1024x768, stretched as a whole - takes twice
// at most). The camera's room below the map is the engine's (the bar's height) while the frame is no
// bigger than the interface - at the same scale the corner it hides is the original's in proportion;
// a frame bigger than the interface gets its own room (minimap_bounds), a strip below the map.
// The bigger frames are VIDs 950 and 951 (game/objects.override): the frame's picture enlarged
// two and three times pixel by pixel (patch_data.make_minimap_frames), the size of the menu
// item's box with it. Player_ShowStateBar takes the frame's VID from World.vids[464], so while
// it builds the bar (status_bar.cpp) that slot names the bigger VID and the engine places it in
// the corner by its own size; MenuNVidUnderCursor gives 464 for them, so the bar's clicks, the
// cursor and the mouse tip treat them as the minimap. The dots are drawn as blocks of the
// step's size at their own scaled place.
//
// The points without a wait for the GPU (perf.h; docs/dev/changes.md fix 58). The engine writes the rail points
// and the dots straight into the back buffer: Graphics_PutPoint / Graphics_PutPoint2x2 lock it through the
// surface (not Surface_Lock) when it is not locked, and leave it locked - the rail points, then the view's
// highlight (a Direct3D quad, which unlocks first), then the dots, unlocked by Graphics_EndScene. Under dgVoodoo a
// lock after Direct3D drawing waits for the GPU and reads the picture back; the dots' lock always follows the
// highlight, the rail points' follows the bar's drawing when no message is on the screen - 2-3 ms each a frame in
// the test rig. What such a lock leaves on the picture is dgVoodoo's: it draws in 32-bit colour and, at the unlock,
// puts back from the 16-bit copy only around what the CPU changed - for every pixel whose value changed, the 2 x 2
// block with that pixel at its bottom right (the pixel, its left, upper and upper-left neighbours) gets the 16-bit
// colour of the copy; the rest keeps its finer colour (found in held frames: test pixels written into the dots'
// lock; a pixel written with the value it had changes nothing). Here, at the level's own zoom and in 16-bit colour,
// the points go into a buffer of ours - the same pixels by the engine's own tests and its 16-bit colour - and each
// of the two batches (the rail points, put before the highlight; the dots, at the end of Minimap_Draw) is put on
// the picture as that unlock would: the minimap's rectangle (and the column and row up-left of it) copied by the
// GPU through a 16-bit texture (DirectDraw's Blt into a 16-bit surface cuts the colour as the lock's copy does),
// copied back through a clip list of exactly those 2 x 2 blocks, then the points with a colour-keyed Blt (a key no
// point has). The same picture, no lock, no wait. The one case it cannot know without reading the picture: a point
// drawn in exactly the 16-bit colour already under it is no change for dgVoodoo (nothing put back around it), and
// here its block gets the 16-bit colour - where that colour was finer (Direct3D blending after the last lock: the
// view's highlight) a few pixels would differ by less than one step of 16-bit colour. When a surface of ours cannot
// be made or a Blt fails, the batch is written the engine's way (its lock, left locked as the engine leaves it).
// A zoom's interface pass (zoom_view.cpp) has its own overlay and is left as it is; so is a 32-bit picture, and a
// picture already locked when the minimap starts.
#include "patch.h"
#include "perf.h"
#include <ddraw.h>
#include <cstdlib>
#include <vector>

int interface_scale();                 // interface_scale.cpp
int interface_original_nvid(int nvid);
int interface_scale_for_window();
bool zoom_view_world_view(float& w, float& h);   // zoom_view.cpp: the world's view while drawn in its second pass
bool zoom_view_defer_brighten(float l, float t, float r, float b, uint32_t color);
double zoom_view_room();

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t PLACE_OVER = 0x405500;      // Minimap_PlaceOver
constexpr uintptr_t PUT_POINT = 0x4055a0;       // Minimap_PutPoint
constexpr uintptr_t PUT_DOT = 0x405630;         // Minimap_PutDot
constexpr uintptr_t PUT_BIG_DOT = 0x4056c0;     // Minimap_PutBigDot
constexpr uintptr_t DRAW_BRIGHTEN = 0x405940;   // Minimap_Draw: call Graphics_BrightenRect (the view)
constexpr uintptr_t BRIGHTEN_RECT = 0x412ba0;   // Graphics_BrightenRect
constexpr uintptr_t MINIMAP_DRAW = 0x405800;    // Minimap_Draw
constexpr uintptr_t GRAPHICS_PUT_POINT = 0x405cd0;
constexpr uintptr_t GRAPHICS_PUT_POINT_2X2 = 0x405e70;
constexpr uintptr_t MENU_NVID_UNDER_CURSOR = 0x4247f0;
constexpr char REG_KEY[] = "SOFTWARE\\Gromada\\Locoland\\Players";   // the scripts' settings (player_registry.cpp)

constexpr int VID_MINIMAP = 464;
constexpr int VID_MINIMAP_BIG[] = {950, 951};   // two and three times
constexpr int MAX_SIZE = 3;
// the opening of the frame's picture (104 x 78): from (8, 9), 14 and 13 px narrower
constexpr int OPENING_LEFT = 8, OPENING_TOP = 9, OPENING_LESS_W = 14, OPENING_LESS_H = 13;
constexpr int FRAME_W = 104;
constexpr int BAR_CONTROLS_RIGHT = 228;   // the slots end there right of the screen's middle (times the scale)
constexpr float HIGHLIGHT_MIN_W = 7, HIGHLIGHT_MIN_H = 5;
constexpr int BAR_MINIMAP = 73;            // the frame's place in PlayerBase.bar
constexpr int WORLD_BOUNDS_BOTTOM = 0x40;  // World: the camera's limits (left, right, top, bottom from +0x34)

struct Minimap {   // re/types.txt
    uint32_t flags;
    float left, top, width, height;
};

template <typename T>
T& at(void* base, int offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}
uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }
uint8_t* graphics() { return *reinterpret_cast<uint8_t**>(G_GRAPHICS); }
uint8_t*& vid_slot(int nvid) { return reinterpret_cast<uint8_t**>(world() + 0x294)[nvid]; }
int vid_count() { return at<int>(world(), 0x290); }

int g_size = 1;   // the step of the minimap the frame in use was made for
// The player's choice: MinimapSize of the Players key, the text the script's SetReg writes (a
// DWORD is taken too); 0 (as the interface) when it is absent or not 1-3.
int read_size();
// Read when the bar is built (`fresh`); the settings page's preview, asking every frame, gets the value read
// at most 4 times a second.
int chosen_size(bool fresh = false) {
    static int value = 0;
    static DWORD at = 0;
    const DWORD now = GetTickCount();
    if (fresh || at == 0 || now - at >= 250) {
        value = read_size();
        at = now ? now : 1;
    }
    return value;
}

int read_size() {
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return 0;
    char text[16] = {};
    DWORD size = sizeof text - 1, type = 0;
    const LONG r = RegQueryValueExA(key, "MinimapSize", nullptr, &type, reinterpret_cast<BYTE*>(text), &size);
    RegCloseKey(key);
    int value = 0;
    if (r == ERROR_SUCCESS && type == REG_SZ) value = std::atoi(text);
    if (r == ERROR_SUCCESS && type == REG_DWORD && size == 4) std::memcpy(&value, text, 4);
    return value >= 1 && value <= MAX_SIZE ? value : 0;
}

// The frame stays right of the bar's controls (the bar's base is centred).
bool fits(int size, int bar_scale, int screen_w) {
    return size <= 1 || screen_w - size * FRAME_W >= screen_w / 2 + BAR_CONTROLS_RIGHT * bar_scale;
}

int size_for(int screen_w, int bar_scale, bool fresh = false) {
    int size = chosen_size(fresh);
    if (size == 0) size = bar_scale;
    while (size > 1 && !fits(size, bar_scale, screen_w)) --size;
    return size < 1 ? 1 : size;
}

int size_of_frame(const uint8_t* frame) {
    const int nvid = at<int>(at<uint8_t*>(const_cast<uint8_t*>(frame), 0x1c), 4);
    for (int i = 0; i < 2; ++i)
        if (nvid == VID_MINIMAP_BIG[i]) return i + 2;
    return 1;
}

// ---- the rectangle ----

Minimap* __fastcall place_over(Minimap* m, void*, uint8_t* frame) {
    uint8_t* vid = at<uint8_t*>(frame, 0x1c);
    const int k = size_of_frame(frame);
    g_size = k;
    const int w = at<short>(vid, 0x2a6), h = at<short>(vid, 0x2a8);
    m->flags &= ~1u;
    m->left = at<float>(frame, 0x30) - at<float>(world(), 0x44) - static_cast<float>(w / 2) + OPENING_LEFT * k;
    m->top = at<float>(frame, 0x34) - at<float>(frame, 0x38) - at<float>(world(), 0x48) - static_cast<float>(h / 2) +
             OPENING_TOP * k;
    m->width = static_cast<float>(w - OPENING_LESS_W * k);
    m->height = static_cast<float>(h - OPENING_LESS_H * k);
    return m;
}

// ---- the points without a lock (fix 58) ----

struct Points {
    bool on = false;                       // the points go here, not into the picture
    int left = 0, top = 0, w = 0, h = 0;   // the minimap's rectangle
    std::vector<uint16_t> color;
    std::vector<uint8_t> set;
    int count = 0;                         // pixels set in this batch
};
Points g_points;
IDirectDrawSurface7* g_overlay = nullptr;   // system memory, the rectangle's size: the points
IDirectDrawSurface7* g_cut = nullptr;       // a 16-bit texture: the picture's colour cut as a lock's copy cuts it
IDirectDrawClipper* g_blocks = nullptr;     // the 2 x 2 blocks of a batch
int g_overlay_w = 0, g_overlay_h = 0, g_cut_w = 0, g_cut_h = 0;
bool g_failure_logged = false;

// The engine's 16-bit colour of a point (Graphics_PutPoint: the shifts and masks of the 16-bit format).
uint16_t color16(uint32_t color) {
    const int green_shift = *reinterpret_cast<int*>(0x47a5e4), red_shift = *reinterpret_cast<int*>(0x47a5e0);
    const uint32_t green_mask = *reinterpret_cast<uint32_t*>(0x47a5dc), red_mask = *reinterpret_cast<uint32_t*>(0x47a5d8);
    return static_cast<uint16_t>(((color >> ((8 - green_shift) & 0x1f)) & green_mask) |
                                 ((color >> ((16 - red_shift) & 0x1f)) & red_mask) | ((color >> 3) & 0x1f));
}

void set_pixel(int x, int y, uint16_t c) {
    const int i = x - g_points.left, j = y - g_points.top;
    if (i < 0 || j < 0 || i >= g_points.w || j >= g_points.h) return;   // put_block keeps them inside
    const size_t k = static_cast<size_t>(j) * g_points.w + i;
    g_points.color[k] = c;
    g_points.set[k] = 1;
    ++g_points.count;
}

int truncate(float f) { return static_cast<int>(static_cast<long long>(f)); }   // crt_ftol

// Graphics_PutPoint's test and pixel: (x, y) inside the view (left, right, top, bottom at +0x220).
void point(const float* view, float x, float y, uint32_t color) {
    if (view[0] <= x && x < view[1] && view[2] <= y && y < view[3]) set_pixel(truncate(x), truncate(y), color16(color));
}

// Graphics_PutPoint2x2's: one pixel less at the right and the bottom edge.
void point2x2(const float* view, float x, float y, uint32_t color) {
    if (!(view[0] <= x) || !(view[2] <= y) || !(x < view[1] - 1.0f) || !(y < view[3] - 1.0f)) return;
    const int px = truncate(x), py = truncate(y);
    const uint16_t c = color16(color);
    set_pixel(px, py, c);
    set_pixel(px + 1, py, c);
    set_pixel(px, py + 1, c);
    set_pixel(px + 1, py + 1, c);
}

IDirectDraw7* ddraw_of(IDirectDrawSurface7* s) {
    static const GUID IID_DD7 = {0x15e65ec0, 0x3b9c, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b}};
    IUnknown* unknown = nullptr;
    if (FAILED(s->GetDDInterface(reinterpret_cast<void**>(&unknown))) || !unknown) return nullptr;
    IDirectDraw7* dd = nullptr;
    unknown->QueryInterface(IID_DD7, reinterpret_cast<void**>(&dd));
    unknown->Release();
    return dd;
}

// A surface of the back buffer's format, made again when the size changes; restored when lost.
bool ensure_surface(IDirectDrawSurface7*& s, int& sw, int& sh, IDirectDrawSurface7* back, int w, int h, DWORD caps) {
    if (s && sw == w && sh == h) return s->IsLost() != DDERR_SURFACELOST || SUCCEEDED(s->Restore());
    if (s) s->Release();
    s = nullptr;
    IDirectDraw7* dd = ddraw_of(back);
    if (!dd) return false;
    DDSURFACEDESC2 desc = {};
    desc.dwSize = sizeof desc;
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    desc.dwWidth = w;
    desc.dwHeight = h;
    desc.ddpfPixelFormat.dwSize = sizeof desc.ddpfPixelFormat;
    back->GetPixelFormat(&desc.ddpfPixelFormat);
    desc.ddsCaps.dwCaps = caps;
    const HRESULT hr = desc.ddpfPixelFormat.dwRGBBitCount == 16 ? dd->CreateSurface(&desc, &s, nullptr) : E_FAIL;
    if (!g_blocks && SUCCEEDED(hr)) dd->CreateClipper(0, &g_blocks, nullptr);
    dd->Release();
    if (FAILED(hr) || !s) {
        s = nullptr;
        if (!g_failure_logged) {
            g_failure_logged = true;
            patch::log("minimap: no %dx%d surface (%08X) - its points lock the picture as before", w, h, static_cast<unsigned>(hr));
        }
        return false;
    }
    sw = w;
    sh = h;
    return true;
}

void device_scene(uint8_t* g, int slot) {   // IDirect3DDevice7::BeginScene (5) / EndScene (6)
    void* device = at<void*>(g, 0xbf4);
    reinterpret_cast<HRESULT(__stdcall*)(void*)>((*reinterpret_cast<void***>(device))[slot])(device);
}

// The batch as dgVoodoo's unlock leaves it, by the GPU (see the top). False: the points are not on the picture.
bool put_by_gpu(uint8_t* g, IDirectDrawSurface7* back) {
    const int w = g_points.w, h = g_points.h;
    const int bw = w + 1, bh = h + 1, bl = g_points.left - 1, bt = g_points.top - 1;   // the blocks' area
    if (bl < 0 || bt < 0) return false;
    if (!ensure_surface(g_overlay, g_overlay_w, g_overlay_h, back, w, h, DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY) ||
        !ensure_surface(g_cut, g_cut_w, g_cut_h, back, bw, bh, DDSCAPS_TEXTURE | DDSCAPS_VIDEOMEMORY) || !g_blocks)
        return false;
    // the blocks: a set pixel (i, j) of the rectangle marks (i - 1 .. i, j - 1 .. j) of it, (i .. i + 1, j .. j + 1) here
    static std::vector<uint8_t> mask;
    mask.assign(static_cast<size_t>(bw) * bh, 0);
    static uint8_t used[0x10000 / 8];
    std::memset(used, 0, sizeof used);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            const size_t k = static_cast<size_t>(j) * w + i;
            if (!g_points.set[k]) continue;
            uint8_t* m = &mask[static_cast<size_t>(j) * bw + i];
            m[0] = m[1] = m[bw] = m[bw + 1] = 1;
            used[g_points.color[k] >> 3] |= static_cast<uint8_t>(1 << (g_points.color[k] & 7));
        }
    static std::vector<uint8_t> region;
    region.assign(sizeof(RGNDATAHEADER), 0);
    DWORD rects = 0;
    for (int j = 0; j < bh; ++j)
        for (int i = 0; i < bw;) {
            if (!mask[static_cast<size_t>(j) * bw + i]) {
                ++i;
                continue;
            }
            int e = i;
            while (e < bw && mask[static_cast<size_t>(j) * bw + e]) ++e;
            const RECT r = {bl + i, bt + j, bl + e, bt + j + 1};
            const auto* b = reinterpret_cast<const uint8_t*>(&r);
            region.insert(region.end(), b, b + sizeof r);
            ++rects;
            i = e;
        }
    auto* rgn = reinterpret_cast<RGNDATA*>(region.data());
    rgn->rdh.dwSize = sizeof(RGNDATAHEADER);
    rgn->rdh.iType = RDH_RECTANGLES;
    rgn->rdh.nCount = rects;
    rgn->rdh.nRgnSize = rects * sizeof(RECT);
    rgn->rdh.rcBound = {bl, bt, bl + bw, bt + bh};
    uint16_t key = 0xF81F;   // magenta, unless a point has it
    while (used[key >> 3] & (1 << (key & 7))) ++key;
    DDSURFACEDESC2 d = {};
    d.dwSize = sizeof d;
    if (FAILED(g_overlay->Lock(nullptr, &d, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr))) return false;
    for (int j = 0; j < h; ++j) {
        auto* row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(d.lpSurface) + j * d.lPitch);
        for (int i = 0; i < w; ++i) {
            const size_t k = static_cast<size_t>(j) * w + i;
            row[i] = g_points.set[k] ? g_points.color[k] : key;
        }
    }
    g_overlay->Unlock(nullptr);
    DDCOLORKEY ck = {key, key};
    RECT area = {bl, bt, bl + bw, bt + bh}, cut = {0, 0, bw, bh};
    RECT to = {g_points.left, g_points.top, g_points.left + w, g_points.top + h}, from = {0, 0, w, h};
    if (FAILED(g_overlay->SetColorKey(DDCKEY_SRCBLT, &ck)) || FAILED(g_blocks->SetClipList(rgn, 0))) return false;
    device_scene(g, 6);
    bool ok = SUCCEEDED(g_cut->Blt(&cut, back, &area, DDBLT_WAIT, nullptr));
    if (ok && SUCCEEDED(back->SetClipper(g_blocks))) {
        ok = SUCCEEDED(back->Blt(&area, g_cut, &cut, DDBLT_WAIT, nullptr));
        back->SetClipper(nullptr);
    } else {
        ok = false;
    }
    // the points themselves (if this fails after the blocks went back, the engine's way below writes them over
    // the same 16-bit colours)
    if (ok) ok = SUCCEEDED(back->Blt(&to, g_overlay, &from, DDBLT_KEYSRC | DDBLT_WAIT, nullptr));
    device_scene(g, 5);
    return ok;
}

// The batch onto the picture: by the GPU, else as the engine does it (its lock, left locked).
void put_points() {
    if (!g_points.count) return;
    uint8_t* g = graphics();
    auto* back = at<IDirectDrawSurface7*>(g, 0xc04);
    if (!at<void*>(g, 0x208) && !put_by_gpu(g, back)) {
        DDSURFACEDESC2 d = {};
        d.dwSize = sizeof d;
        if (SUCCEEDED(back->Lock(nullptr, &d, 0, nullptr))) {   // Graphics_PutPoint's lock
            at<void*>(g, 0x208) = d.lpSurface;
            at<int>(g, 0x214) = d.lPitch / 2;
        }
    }
    if (auto* bits = at<uint16_t*>(g, 0x208)) {   // the engine's way: into the locked picture
        const int pitch = at<int>(g, 0x214);
        for (int j = 0; j < g_points.h; ++j)
            for (int i = 0; i < g_points.w; ++i) {
                const size_t k = static_cast<size_t>(j) * g_points.w + i;
                if (g_points.set[k]) bits[static_cast<size_t>(g_points.top + j) * pitch + g_points.left + i] = g_points.color[k];
            }
    }
    std::fill(g_points.set.begin(), g_points.set.end(), static_cast<uint8_t>(0));
    g_points.count = 0;
}

// ---- the dots ----

using PutPoint_t = void(__fastcall*)(void* g, void* edx, float x, float y, uint32_t color);

// A block of w x h pixels from (x, y), moved inside the rectangle.
void put_block(const Minimap* m, float x, float y, int w, int h, uint32_t color) {
    int x0 = static_cast<int>(x), y0 = static_cast<int>(y);
    const int left = static_cast<int>(m->left), top = static_cast<int>(m->top);
    const int right = left + static_cast<int>(m->width), bottom = top + static_cast<int>(m->height);
    if (x0 + w > right) x0 = right - w;
    if (y0 + h > bottom) y0 = bottom - h;
    if (x0 < left) x0 = left;
    if (y0 < top) y0 = top;
    void* g = graphics();
    if (g_points.on) {   // into our buffer, by the engine's own tests
        const float* view = reinterpret_cast<const float*>(static_cast<uint8_t*>(g) + 0x220);
        if (w % 2 == 0 && h % 2 == 0) {
            for (int j = 0; j < h; j += 2)
                for (int i = 0; i < w; i += 2) point2x2(view, static_cast<float>(x0 + i), static_cast<float>(y0 + j), color);
        } else {
            for (int j = 0; j < h; ++j)
                for (int i = 0; i < w; ++i) point(view, static_cast<float>(x0 + i), static_cast<float>(y0 + j), color);
        }
        return;
    }
    if (w % 2 == 0 && h % 2 == 0) {
        const auto put = reinterpret_cast<PutPoint_t>(GRAPHICS_PUT_POINT_2X2);
        for (int j = 0; j < h; j += 2)
            for (int i = 0; i < w; i += 2) put(g, nullptr, static_cast<float>(x0 + i), static_cast<float>(y0 + j), color);
    } else {
        const auto put = reinterpret_cast<PutPoint_t>(GRAPHICS_PUT_POINT);
        for (int j = 0; j < h; ++j)
            for (int i = 0; i < w; ++i) put(g, nullptr, static_cast<float>(x0 + i), static_cast<float>(y0 + j), color);
    }
}

// The scaled place of a map point, when it is inside the rectangle (as the originals test it).
bool scaled(const Minimap* m, float x, float y, float& sx, float& sy) {
    sx = x * m->width / at<float>(world(), 0x28) + m->left;
    sy = y * m->height / at<float>(world(), 0x2c) + m->top;
    return m->left <= sx && m->top <= sy && sx < m->left + m->width && sy < m->height + m->top;
}

void __fastcall put_point(Minimap* m, void*, float x, float y, uint32_t color) {
    float sx, sy;
    if (scaled(m, x, y, sx, sy)) put_block(m, sx, sy, g_size, g_size, color);
}

void __fastcall put_dot(Minimap* m, void*, float x, float y, uint32_t color) {
    float sx, sy;
    if (scaled(m, x, y, sx, sy)) put_block(m, sx, sy, 2 * g_size, 2 * g_size, color);
}

void __fastcall put_big_dot(Minimap* m, void*, float x, float y, uint32_t color) {
    float sx, sy;
    if (scaled(m, x, y, sx, sy)) put_block(m, sx - 2 * g_size, sy - 2 * g_size, 4 * g_size, 4 * g_size, color);
}

// ---- the view's highlight ----

using Brighten_t = void(__fastcall*)(void* g, void* edx, float l, float t, float r, float b, uint32_t color);

// [lo, hi) on one axis: the map's part in view, at least `least` px, inside [from, from + length)
void fit(float& lo, float& hi, float from, float length, float least) {
    if (hi - lo < least) {
        const float middle = (lo + hi) / 2;
        lo = middle - least / 2;
        hi = lo + least;
    }
    if (hi > from + length) {
        lo -= hi - (from + length);
        hi = from + length;
    }
    if (lo < from) {
        hi += from - lo;
        lo = from;
    }
    if (hi > from + length) hi = from + length;
    lo = static_cast<float>(static_cast<int>(lo + 0.5f));
    hi = static_cast<float>(static_cast<int>(hi + 0.5f));
}

void __fastcall brighten_view(void* g, void*, const Minimap* m, float, float, float, float, uint32_t color) {
    uint8_t* w = world();
    const float map_w = at<float>(w, 0x28), map_h = at<float>(w, 0x2c);
    const float cam_x = at<float>(w, 0x44), cam_y = at<float>(w, 0x48);
    const float* view = reinterpret_cast<float*>(static_cast<uint8_t*>(g) + 0x220);   // left, right, top, bottom
    float view_w = view[1] - view[0], view_h = view[3] - view[2];
    zoom_view_world_view(view_w, view_h);   // drawn in a zoom's interface pass: the world's view, not the window's
    auto clamp = [](float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; };
    const float x0 = clamp(cam_x, 0, map_w), x1 = clamp(cam_x + view_w, 0, map_w);
    const float y0 = clamp(cam_y, 0, map_h), y1 = clamp(cam_y + view_h, 0, map_h);
    float l = x0 * m->width / map_w + m->left, r = x1 * m->width / map_w + m->left;
    float t = y0 * m->height / map_h + m->top, b = y1 * m->height / map_h + m->top;
    fit(l, r, m->left, m->width, HIGHLIGHT_MIN_W * g_size);
    fit(t, b, m->top, m->height, HIGHLIGHT_MIN_H * g_size);
    if (zoom_view_defer_brighten(l, t, r, b, color)) return;   // a zoom's pass: after the dots reach the picture
    if (g_points.on) put_points();   // the rail points before the highlight, as the engine has them
    reinterpret_cast<Brighten_t>(BRIGHTEN_RECT)(g, nullptr, l, t, r, b, color);
}

// Minimap_Draw: its points into our buffer (fix 58).
using MinimapDraw_t = void(__fastcall*)(Minimap* m, void* edx);
MinimapDraw_t g_minimap_draw = nullptr;
void __fastcall minimap_draw(Minimap* m, void* edx) {
    float vw, vh;
    uint8_t* g = graphics();
    if (perf::off(58) || !g || (at<uint32_t>(g, 0) & 2) || at<void*>(g, 0x208) || zoom_view_world_view(vw, vh) ||
        !(m->width >= 1 && m->height >= 1 && m->width <= 4096 && m->height <= 4096))
        return g_minimap_draw(m, edx);
    g_points.left = static_cast<int>(m->left);
    g_points.top = static_cast<int>(m->top);
    g_points.w = static_cast<int>(m->width);
    g_points.h = static_cast<int>(m->height);
    const size_t n = static_cast<size_t>(g_points.w) * g_points.h;
    if (g_points.color.size() != n) {
        g_points.color.assign(n, 0);
        g_points.set.assign(n, 0);
    }
    g_points.count = 0;
    g_points.on = true;
    g_minimap_draw(m, edx);
    put_points();   // the dots
    g_points.on = false;
}

// At the call in Minimap_Draw: ecx = g_graphics, the five arguments pushed, ebp = the Minimap.
// The Minimap goes in as the first stack argument; brighten_view (fastcall) takes all six off.
__declspec(naked) void brighten_call() {
    __asm {
        pop eax          // the return address
        push ebp         // the Minimap
        push eax
        jmp brighten_view
    }
}

// ---- the menu item under the cursor ----

int __fastcall menu_nvid_under_cursor(uint8_t* menu) {
    uint8_t* hover = at<uint8_t*>(menu, 0x14);
    if (!hover) return 0;
    const int nvid = at<int>(at<uint8_t*>(hover, 0x1c), 4);
    for (int big : VID_MINIMAP_BIG)
        if (nvid == big) return VID_MINIMAP;
    return interface_original_nvid(nvid);   // the interface's bigger VIDs
}

bool g_installed = false;

}  // namespace

// status_bar.cpp, around Player_ShowStateBar when it creates the bar: the frame's VID for this
// screen into World.vids[464] (the original's slot back afterwards). Returns the original VID
// to put back, or nullptr when nothing was changed.
void* minimap_frame_for_screen() {
    if (!g_installed || !world() || !graphics()) return nullptr;
    const float* screen = reinterpret_cast<float*>(graphics() + 0x20c);   // width, height
    const int k = size_for(static_cast<int>(screen[0]), interface_scale(), true);
    if (k < 2) return nullptr;
    const int nvid = VID_MINIMAP_BIG[k - 2];
    if (nvid >= vid_count() || !vid_slot(nvid) || !vid_slot(VID_MINIMAP)) {
        patch::log("minimap: no VID %d for size %d - the original frame", nvid, k);
        return nullptr;
    }
    // build.py copies the original frame there when it cannot enlarge an edition's picture
    const short w = at<short>(vid_slot(nvid), 0x2a6), h = at<short>(vid_slot(nvid), 0x2a8);
    if (w != k * at<short>(vid_slot(VID_MINIMAP), 0x2a6) || h != k * at<short>(vid_slot(VID_MINIMAP), 0x2a8)) {
        patch::log("minimap: VID %d is %dx%d, not %d times the frame - the original size", nvid, w, h, k);
        return nullptr;
    }
    uint8_t* original = vid_slot(VID_MINIMAP);
    vid_slot(VID_MINIMAP) = vid_slot(nvid);
    patch::log("minimap: size %d (VID %d) for a %dx%d screen", k, nvid, static_cast<int>(screen[0]),
               static_cast<int>(screen[1]));
    return original;
}

// menu_version.cpp: the size the bar would get on a screen this wide (the settings page's preview), with the
// interface's scale as it would be on the window.
int minimap_size_for_screen(int screen_w) { return size_for(screen_w, interface_scale_for_window()); }

void minimap_frame_back(void* original) {
    if (original) vid_slot(VID_MINIMAP) = static_cast<uint8_t*>(original);
}

// status_bar.cpp, after every Player_ShowStateBar: the camera's room below the map for a frame bigger
// than the interface. The engine lets the camera go past the map's bottom by the bar's height less 2
// (boundsBottom = map height + 48), so the bar hides none of the map; the frame stands above the
// bar in the corner - the original's 104 x 78 one hides the map's last 30 rows there, a part the
// original never shows. A bigger frame would hide 208 x 108 px (twice) or 312 x 186 px (three
// times) of the map's bottom right corner for good: units, trains and buildings there could not be
// seen or clicked. So with a bigger frame the camera goes down as far as the frame is high, the
// map's bottom edge up to the frame's top, and nothing the original shows is hidden. Below the map
// the engine draws black (Graphics_DrawScene clears the view where it leaves the map). A frame of the
// interface's own scale needs none of it: the bar is as many times higher, the engine's room with it
// (the bar's height less 2), and the frame hides the original's corner in proportion - no strip.
void minimap_bounds(void* player) {
    if (!g_installed || !world()) return;
    const int count = at<int>(player, 0x18);   // PlayerBase.bar
    if (count <= BAR_MINIMAP) return;
    uint8_t* frame = at<uint8_t**>(player, 0x20)[BAR_MINIMAP];
    if (!frame || size_of_frame(frame) <= interface_scale()) return;
    const float height = static_cast<float>(at<short>(at<uint8_t*>(frame, 0x1c), 0x2a8));
    float& bottom = at<float>(world(), WORLD_BOUNDS_BOTTOM);
    const float wanted = at<float>(world(), 0x2c) + static_cast<float>(height * zoom_view_room());
    if (bottom < wanted) bottom = wanted;
}

void install_minimap() {
    // mov eax,[ecx] / push esi / mov esi,[esp+8]
    const uint8_t place_start[] = {0x8B, 0x01, 0x56, 0x8B, 0x74, 0x24, 0x08};
    // fld [esp+4] / fmul [ecx+0xc]
    const uint8_t point_start[] = {0xD9, 0x44, 0x24, 0x04, 0xD8, 0x49, 0x0C};
    // push ecx / fld [esp+8] / fmul [ecx+0xc]
    const uint8_t big_start[] = {0x51, 0xD9, 0x44, 0x24, 0x08, 0xD8, 0x49, 0x0C};
    // mov eax,[ecx+0x14] / test eax,eax
    const uint8_t nvid_start[] = {0x8B, 0x41, 0x14, 0x85, 0xC0};
    // call Graphics_BrightenRect
    const uint8_t brighten[] = {0xE8, 0x5B, 0xD2, 0x00, 0x00};
    if (!patch::check(PLACE_OVER, place_start, sizeof place_start) ||
        !patch::check(PUT_POINT, point_start, sizeof point_start) ||
        !patch::check(PUT_DOT, point_start, sizeof point_start) ||
        !patch::check(PUT_BIG_DOT, big_start, sizeof big_start) ||
        !patch::check(MENU_NVID_UNDER_CURSOR, nvid_start, sizeof nvid_start) ||
        !patch::check(DRAW_BRIGHTEN, brighten, sizeof brighten)) {
        patch::log("skipped minimap: unexpected bytes");
        return;
    }
    patch::redirect(PLACE_OVER, place_start, sizeof place_start, reinterpret_cast<void*>(&place_over), "Minimap_PlaceOver");
    patch::redirect(PUT_POINT, point_start, sizeof point_start, reinterpret_cast<void*>(&put_point), "Minimap_PutPoint");
    patch::redirect(PUT_DOT, point_start, sizeof point_start, reinterpret_cast<void*>(&put_dot), "Minimap_PutDot");
    patch::redirect(PUT_BIG_DOT, big_start, sizeof big_start, reinterpret_cast<void*>(&put_big_dot), "Minimap_PutBigDot");
    patch::redirect(MENU_NVID_UNDER_CURSOR, nvid_start, sizeof nvid_start,
                    reinterpret_cast<void*>(&menu_nvid_under_cursor), "MenuNVidUnderCursor");
    patch::write_call(DRAW_BRIGHTEN, nullptr, 0, reinterpret_cast<void*>(&brighten_call), 5);
    // sub esp,0xc / push ebx / push ebp / mov ebp,ecx
    const uint8_t draw_start[] = {0x83, 0xEC, 0x0C, 0x53, 0x55, 0x8B, 0xE9};
    g_minimap_draw = reinterpret_cast<MinimapDraw_t>(
        patch::hook(MINIMAP_DRAW, draw_start, sizeof draw_start, reinterpret_cast<void*>(&minimap_draw), "Minimap_Draw"));
    g_installed = true;
    if (chosen_size() > 0) patch::log("minimap: size %d chosen on the settings page", chosen_size());
}
