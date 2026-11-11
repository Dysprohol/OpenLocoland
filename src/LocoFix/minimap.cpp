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
// The size is the player's choice on the game's settings page (the main menu's and the Esc
// menu's; game/maps/script/optfields.lgc): the script keeps it with SetReg next to the sound and
// music settings, the value MinimapSize ("1", "2" or "3") of the Players key (player_registry.cpp:
// HKCU\SOFTWARE\Gromada\Locoland\Players); absent = the original size, the default. It is read
// each time the bar is built - at a mission's start and when the Esc menu closes (the bar is given
// up while it is hidden, so it is built again) - and used only where it fits the game's screen:
// never so wide that the frame would reach the bar's controls (the slots end 740 px into the
// centred 1024 px base), else the biggest that fits (a small map's logical screen,
// screen_scaling.cpp - mission 1: 1024x768, stretched as a whole - takes twice at most).
// The bigger frames are VIDs 950 and 951 (game/objects.override): the frame's picture enlarged
// two and three times pixel by pixel (patch_data.make_minimap_frames), the size of the menu
// item's box with it. Player_ShowStateBar takes the frame's VID from World.vids[464], so while
// it builds the bar (status_bar.cpp) that slot names the bigger VID and the engine places it in
// the corner by its own size; MenuNVidUnderCursor gives 464 for them, so the bar's clicks, the
// cursor and the mouse tip treat them as the minimap. The dots are drawn as blocks of the
// step's size at their own scaled place.
#include "patch.h"
#include <cstdlib>

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t PLACE_OVER = 0x405500;      // Minimap_PlaceOver
constexpr uintptr_t PUT_POINT = 0x4055a0;       // Minimap_PutPoint
constexpr uintptr_t PUT_DOT = 0x405630;         // Minimap_PutDot
constexpr uintptr_t PUT_BIG_DOT = 0x4056c0;     // Minimap_PutBigDot
constexpr uintptr_t DRAW_BRIGHTEN = 0x405940;   // Minimap_Draw: call Graphics_BrightenRect (the view)
constexpr uintptr_t BRIGHTEN_RECT = 0x412ba0;   // Graphics_BrightenRect
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
constexpr int BAR_WIDTH = 1024, BAR_CONTROLS_END = 740;   // the slots end there in the base
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
// DWORD is taken too); 1 when it is absent or not 1-3.
int chosen_size() {
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return 1;
    char text[16] = {};
    DWORD size = sizeof text - 1, type = 0;
    const LONG r = RegQueryValueExA(key, "MinimapSize", nullptr, &type, reinterpret_cast<BYTE*>(text), &size);
    RegCloseKey(key);
    int value = 0;
    if (r == ERROR_SUCCESS && type == REG_SZ) value = std::atoi(text);
    if (r == ERROR_SUCCESS && type == REG_DWORD && size == 4) std::memcpy(&value, text, 4);
    return value >= 1 && value <= MAX_SIZE ? value : 1;
}

// The frame stays right of the bar's controls (the bar's base is centred).
bool fits(int size, int screen_w) {
    return size <= 1 || screen_w - size * FRAME_W >= (screen_w - BAR_WIDTH) / 2 + BAR_CONTROLS_END;
}

int size_for(int screen_w) {
    int size = chosen_size();
    while (size > 1 && !fits(size, screen_w)) --size;
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
    auto clamp = [](float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; };
    const float x0 = clamp(cam_x, 0, map_w), x1 = clamp(cam_x + view[1] - view[0], 0, map_w);
    const float y0 = clamp(cam_y, 0, map_h), y1 = clamp(cam_y + view[3] - view[2], 0, map_h);
    float l = x0 * m->width / map_w + m->left, r = x1 * m->width / map_w + m->left;
    float t = y0 * m->height / map_h + m->top, b = y1 * m->height / map_h + m->top;
    fit(l, r, m->left, m->width, HIGHLIGHT_MIN_W * g_size);
    fit(t, b, m->top, m->height, HIGHLIGHT_MIN_H * g_size);
    reinterpret_cast<Brighten_t>(BRIGHTEN_RECT)(g, nullptr, l, t, r, b, color);
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
    return nvid;
}

bool g_installed = false;

}  // namespace

// status_bar.cpp, around Player_ShowStateBar when it creates the bar: the frame's VID for this
// screen into World.vids[464] (the original's slot back afterwards). Returns the original VID
// to put back, or nullptr when nothing was changed.
void* minimap_frame_for_screen() {
    if (!g_installed || !world() || !graphics()) return nullptr;
    const float* screen = reinterpret_cast<float*>(graphics() + 0x20c);   // width, height
    const int k = size_for(static_cast<int>(screen[0]));
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

// menu_version.cpp: the size the bar would get on a screen this wide (the settings page's preview).
int minimap_size_for_screen(int screen_w) { return size_for(screen_w); }

void minimap_frame_back(void* original) {
    if (original) vid_slot(VID_MINIMAP) = static_cast<uint8_t*>(original);
}

// status_bar.cpp, after every Player_ShowStateBar: the camera's room below the map for a bigger
// frame. The engine lets the camera go past the map's bottom by the bar's height less 2
// (boundsBottom = map height + 48), so the bar hides none of the map; the frame stands above the
// bar in the corner - the original's 104 x 78 one hides the map's last 30 rows there, a part the
// original never shows. A bigger frame would hide 208 x 108 px (twice) or 312 x 186 px (three
// times) of the map's bottom right corner for good: units, trains and buildings there could not be
// seen or clicked. So with a bigger frame the camera goes down as far as the frame is high, the
// map's bottom edge up to the frame's top, and nothing the original shows is hidden. Below the map
// the engine draws black (Graphics_DrawScene clears the view where it leaves the map).
void minimap_bounds(void* player) {
    if (!g_installed || !world()) return;
    const int count = at<int>(player, 0x18);   // PlayerBase.bar
    if (count <= BAR_MINIMAP) return;
    uint8_t* frame = at<uint8_t**>(player, 0x20)[BAR_MINIMAP];
    if (!frame || size_of_frame(frame) < 2) return;
    const float height = static_cast<float>(at<short>(at<uint8_t*>(frame, 0x1c), 0x2a8));
    float& bottom = at<float>(world(), WORLD_BOUNDS_BOTTOM);
    const float wanted = at<float>(world(), 0x2c) + height;
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
    g_installed = true;
    if (chosen_size() > 1) patch::log("minimap: size %d chosen on the settings page", chosen_size());
}
