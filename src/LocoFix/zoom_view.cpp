// The zoom's view (zoom.cpp's part 2): the world drawn at the zoom's size, the interface drawn after it at the
// window's size - two passes - so the interface keeps its exact size at every zoom, in and out.
//
// A zoom z (window pixels per map pixel) shows a view of W / z x H / z map pixels (W x H the window), at most the
// map on each axis. The engine draws that view as its logical screen (screen_scaling.cpp's machinery: surfaces,
// screen size, view and clips, the mouse mapped in WM_NCHITTEST), so the camera's limits, culling, edge
// scrolling and world picking work in it as ever. The picture is stretched z times into the middle of the window
// (bars round it when the map is smaller than the window's view: the whole-map view); the interface is drawn
// after that, at the window's size. At the level's own zoom (1, or a small map's own stretch) nothing of this is
// on: the frame is the game's as before.
//
// THE TWO PASSES. The interface is every object of class FRAME or TEXT (Frame_Construct 0x40c4d0 adds them to
// World.menu: the state bar, the minimap's frame, the money plate, messages, the mouse tip, script texts such as
// PrintFile's): drawn from the same layer lists as the world, told apart by their VID's class where each object
// is tested (view_culling.cpp's tests, and the draw call of layers 0 and 7, which have no test).
// - The world pass is Graphics_DrawScene as it is, without the interface's objects; Graphics_DrawEffects runs its
//   timing there but draws nothing (the scroll effect moves the camera there, at the zoom's screen).
// - After Direct3D's EndScene (screen_scaling.cpp's scale_frame) the logical picture is stretched into the
//   window-sized back buffer - a nearest-pixel Blt at whole zooms (2, 3: exact big pixels), else a Blt into a
//   texture drawn as one linear-filtered quad (checked to the pixel at zoom 1: no shift) - and the interface's
//   pass draws layers 2-10 again with only the interface's objects, at the window's size, then the minimap's
//   inside (Minimap_Draw: the engine calls it in the update, after the frame's drawing - held for this pass),
//   then the screen effects over everything (the fades cover the bar, as in the original).
// - The interface's software sprites (layers 2-4, 10, the minimap's dots) go into a system-memory overlay put on
//   the picture with a colour-keyed Blt; their depth buffer is plain memory of ours. Locking the window-sized
//   picture instead cost dgVoodoo a GPU sync and copies each time - 16-17 ms a frame at 1920x1080 against 3.2-3.5
//   (zoom-in.md's numbers, the rig's virtual adapter). Without the depth copy the order does the depth test's
//   work: the interface's items are flat, z 2000-2005, so the software sprites up to the highest z of its
//   Direct3D items (the 455 icons, 498, 499 at 2003) are drawn before them, the ones above it (the numbers at
//   2005) after them; the hardware z-buffer is cleared to the engine's empty value (0x3ff) for the Direct3D ones.
//
// THE MOUSE. The engine's cursor stays logical (WM_NCHITTEST mapped): world picking, orders, edge scrolling.
// The interface's readers get the cursor in the window's pixels: Menu_Update (the hovered item), MouseTip_Update,
// Player_UpdateStateBar (the minimap's hover and click); the minimap's right-click order gets its rectangle in
// logical pixels (the engine computes the map point from the logical cursor).
//
// THE INTERFACE'S LAYOUT. While a zoom view is in effect the bar is built with the screen and the view set to the
// window's (status_bar.cpp asks zoom_view_layout_begin), so it has the window's layout and the interface's own
// scale; it is built again when the view starts or ends. The camera's room below the map is the bar's height in
// map pixels, bar height / z (status_bar.cpp asks zoom_view_room).
//
// THE SCRIPTS' SCREEN. Scripts lay out what they show by the screen's size (ScreenX, ScreenY; MenuLoad by the
// engine's): their texts (a mission's tutorial lines) are interface objects, drawn at the window's size, and the
// Esc menu and the end screens are laid out for 1024x768 in the middle of the screen. While a zoom view is in
// effect all three give the window's size, as at zoom 1 on a map as big as the window; a menu up gets a screen of
// the window's size drawn in one pass (screen_scaling.cpp takes its 1024x768 middle, as ever), and the zoom's view
// comes back when it goes - the menus look as at zoom 1. On a map smaller than the window (its own screen
// stretched, mission 1) the interface's objects on the screen move once into the window's layout when a zoom view
// starts, and back when it ends (move_interface).
#include "patch.h"
#include <ddraw.h>
#include <algorithm>
#include <cmath>

bool screen_set_view(int w, int h, int surface_w, int surface_h, const RECT* target);   // screen_scaling.cpp
bool screen_game_surfaces(IDirectDrawSurface7** out);
bool screen_set_render_target(uint8_t* g, IDirectDrawSurface7* s);
void screen_set_screen(uint8_t* g, int w, int h);
POINT screen_window_cursor();
void screen_base_size(int& w, int& h);
void window_size(int& w, int& h);
bool menu_background_shown();
void fog_buffer_cover(int w, int h);          // fog_buffer.cpp
bool status_bar_rebuild(uint8_t* player);     // status_bar.cpp
void status_bar_room(uint8_t* player);
void interface_messages_rescale();            // interface_texts.cpp
int __cdecl whole_layer_filter(const uint8_t* object);   // below

namespace {

constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t DRAW_LAYER = 0x41fa70;          // World_DrawLayer
constexpr uintptr_t SET_RENDER_STATE = 0x40f050;    // Graphics_SetRenderState
constexpr uintptr_t SURFACE_LOCK = 0x4123c0;        // Surface_Lock (stdcall)
constexpr uintptr_t MINIMAP_DRAW = 0x405800;
constexpr uintptr_t PLACE_OVER = 0x405500;
constexpr uintptr_t MENU_UPDATE = 0x424290;
constexpr uintptr_t MOUSE_TIP_UPDATE = 0x4257f0;
constexpr uintptr_t UPDATE_STATE_BAR = 0x4301e0;
constexpr uintptr_t DRAW_EFFECTS = 0x40fac0;        // Graphics_DrawEffects
constexpr uintptr_t BRIGHTEN_RECT = 0x412ba0;       // Graphics_BrightenRect
constexpr uintptr_t SAVE_SCREENSHOT = 0x40f8c0;
constexpr uintptr_t MENU_LOAD = 0x424400;           // script function 102 MenuLoad
// call sites
constexpr uintptr_t CALL_MENU_UPDATE = 0x423b7e;    // GameWorld_Run
constexpr uintptr_t CALL_MOUSE_TIP = 0x423b8a;      // GameWorld_Run
constexpr uintptr_t CALL_MINIMAP_DRAW = 0x430240;   // Player_UpdateStateBar
constexpr uintptr_t CALL_STATE_BAR = 0x4321f5;      // Player_HandleInput
constexpr uintptr_t CALL_PLACE_OVER = 0x431fce;     // Player_HandleInput: the minimap's right-click order
constexpr uintptr_t CALL_EFFECTS_A = 0x40f749;      // Graphics_DrawScene, its two ends
constexpr uintptr_t CALL_EFFECTS_B = 0x40f809;
constexpr uintptr_t WHOLE_LAYER_DRAW = 0x41fbe0;    // World_DrawLayer, layers 0 and 7: mov edx,[ecx] / call [edx+0x14]
constexpr uintptr_t SCREEN_X = 0x43f9b3;            // script function 119 ScreenX: mov eax,[g_graphics] / fld [eax+0x20c]
constexpr uintptr_t SCREEN_Y = 0x43f9c0;            // script function 120 ScreenY: mov ecx,[g_graphics] / fld [ecx+0x210]

constexpr int MOUSE = 0x230;   // World.mouse: +8/+0xc world, +0x10/+0x14 screen
constexpr int FRAME_CLASS = 10, TEXT_CLASS = 0x13;
constexpr int EFF_SCROLL = 2;
constexpr int EFFECT_START = 0xb20;   // Graphics.effectStart[16]

template <typename T> T& at(void* base, int offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset); }
uint8_t* graphics() { return *reinterpret_cast<uint8_t**>(G_GRAPHICS); }
uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }
void** vtable(void* com) { return *reinterpret_cast<void***>(com); }

// ---- the state ----

bool g_installed = false;
bool g_view = false;        // a zoom view is in effect (not the level's own screen)
bool g_menu = false;        // a menu is up: its screen, one pass
double g_z = 1.0;           // the view's factor
int g_view_w = 0, g_view_h = 0, g_surface_w = 0, g_surface_h = 0;
RECT g_target = {};
bool g_filtered = false;    // this view's stretch is the filtered one
int g_filter_setting = 2;   // the test key's ZoomFilter: 0 nearest, 1 filtered, 2 by the zoom (the default)
int g_pass = 0;             // 2 while the interface's pass draws (zoom_draw_filter)
float g_world_view_w = 0, g_world_view_h = 0;   // the world's view while the minimap is drawn in that pass

int window_w() { int w = 0, h = 0; window_size(w, h); return w; }
int window_h() { int w = 0, h = 0; window_size(w, h); return h; }
int map_w() { return static_cast<int>(at<float>(world(), 0x28)); }
int map_h() { return static_cast<int>(at<float>(world(), 0x2c)); }

// The split is drawn: a zoom view, no menu (also the frame a menu appears in, before its screen comes)
bool split_now() { return g_view && !g_menu && !menu_background_shown(); }

// ---- which objects are the interface ----

bool is_interface(const uint8_t* object) {
    const uint8_t* vid = *reinterpret_cast<uint8_t* const*>(object + 0x1c);
    if (!vid) return false;
    const int cls = *reinterpret_cast<const int*>(vid + 0x10);
    return cls == FRAME_CLASS || cls == TEXT_CLASS;
}

int g_cur_layer = -1;   // the layer the interface's pass draws
int g_sw_part = 0;      // layers 2-4 of the interface's pass: 1 z <= g_d3d_top, 2 above it
float g_d3d_top = 0;

void find_d3d_top() {
    uint8_t* w = world();
    g_d3d_top = -1e30f;
    for (int n = 5; n <= 9; ++n) {
        const int count = at<int>(w, 0x4c + n * 0x10 + 4);
        uint8_t** items = at<uint8_t**>(w, 0x4c + n * 0x10 + 0xc);
        for (int i = 0; i < count; ++i)
            if (items[i] && is_interface(items[i]) && !(at<uint32_t>(items[i], 0x28) & 0x8000) &&
                at<float>(items[i], 0x38) > g_d3d_top)
                g_d3d_top = at<float>(items[i], 0x38);
    }
}

// ---- the mouse in two systems ----

struct MouseSwap {
    float kept[4];
    bool on = false;
    void enter() {
        if (!split_now()) return;
        uint8_t* m = world() + MOUSE;
        for (int i = 0; i < 4; ++i) kept[i] = at<float>(m, 8 + 4 * i);
        POINT p = screen_window_cursor();
        p.x = (std::max)(0L, (std::min)(p.x, static_cast<LONG>(window_w() - 1)));
        p.y = (std::max)(0L, (std::min)(p.y, static_cast<LONG>(window_h() - 1)));
        at<float>(m, 0x10) = static_cast<float>(p.x);
        at<float>(m, 0x14) = static_cast<float>(p.y);
        at<float>(m, 8) = static_cast<float>(p.x) + at<float>(world(), 0x44);
        at<float>(m, 0xc) = static_cast<float>(p.y) + at<float>(world(), 0x48);
        on = true;
    }
    void leave() {
        if (!on) return;
        uint8_t* m = world() + MOUSE;
        for (int i = 0; i < 4; ++i) at<float>(m, 8 + 4 * i) = kept[i];
        on = false;
    }
};

// The screen and the view floats (not the viewport) at a size, for code that lays something out by them.
struct FloatSwap {
    float kept[6];
    bool on = false;
    void enter(int w, int h) {
        uint8_t* g = graphics();
        for (int i = 0; i < 2; ++i) kept[i] = at<float>(g, 0x20c + 4 * i);
        for (int i = 0; i < 4; ++i) kept[2 + i] = at<float>(g, 0x220 + 4 * i);
        at<float>(g, 0x20c) = static_cast<float>(w);
        at<float>(g, 0x210) = static_cast<float>(h);
        at<float>(g, 0x220) = 0;
        at<float>(g, 0x224) = static_cast<float>(w);
        at<float>(g, 0x228) = 0;
        at<float>(g, 0x22c) = static_cast<float>(h);
        on = true;
    }
    void leave() {
        if (!on) return;
        uint8_t* g = graphics();
        for (int i = 0; i < 2; ++i) at<float>(g, 0x20c + 4 * i) = kept[i];
        for (int i = 0; i < 4; ++i) at<float>(g, 0x220 + 4 * i) = kept[2 + i];
        on = false;
    }
};

using MouseCall_t = int(__fastcall*)(void* self, void* edx, void* mouse);

int __fastcall menu_update(void* menu, void* edx, void* mouse) {
    MouseSwap m;
    m.enter();
    const int r = reinterpret_cast<MouseCall_t>(MENU_UPDATE)(menu, edx, mouse);
    m.leave();
    return r;
}

int __fastcall mouse_tip_update(void* tip, void* edx, void* mouse) {
    MouseSwap m;
    FloatSwap f;
    m.enter();
    if (split_now()) f.enter(window_w(), window_h());
    const int r = reinterpret_cast<MouseCall_t>(MOUSE_TIP_UPDATE)(tip, edx, mouse);
    f.leave();
    m.leave();
    return r;
}

int __fastcall state_bar_update(void* player, void* edx, void* mouse) {
    MouseSwap m;
    m.enter();
    const int r = reinterpret_cast<MouseCall_t>(UPDATE_STATE_BAR)(player, edx, mouse);
    m.leave();
    return r;
}

struct MinimapRect { uint32_t flags; float left, top, width, height; };

// The minimap's rectangle for the right-click order in logical pixels: the engine takes the map point as
// (screenX - left) * map / width with its logical cursor.
using PlaceOver_t = void*(__fastcall*)(void* m, void* edx, void* frame);
void* __fastcall place_over_for_order(void* m, void* edx, void* frame) {
    void* r = reinterpret_cast<PlaceOver_t>(PLACE_OVER)(m, edx, frame);
    if (split_now()) {
        auto* rect = static_cast<MinimapRect*>(m);
        const float kx = static_cast<float>(g_view_w) / (g_target.right - g_target.left);
        const float ky = static_cast<float>(g_view_h) / (g_target.bottom - g_target.top);
        rect->left = (rect->left - g_target.left) * kx;
        rect->top = (rect->top - g_target.top) * ky;
        rect->width *= kx;
        rect->height *= ky;
    }
    return r;
}

bool g_minimap_pending = false;
MinimapRect g_minimap = {};
using Minimap_t = void(__fastcall*)(void* m, void* edx);
void __fastcall minimap_draw(void* m, void* edx) {
    if (!split_now()) return reinterpret_cast<Minimap_t>(MINIMAP_DRAW)(m, edx);
    g_minimap = *static_cast<MinimapRect*>(m);   // drawn in the interface's pass
    g_minimap_pending = true;
}

// Graphics_DrawEffects at the end of the world pass: with the split its timing runs (the scroll effect moves the
// camera at the zoom's screen) but nothing is drawn - the interface's pass draws the effects over the window.
using Effects_t = void(__fastcall*)(void* g, void* edx, int draw);
void __fastcall world_effects(void* g, void* edx, int draw) {
    reinterpret_cast<Effects_t>(DRAW_EFFECTS)(g, edx, split_now() ? 0 : draw);
}

// Script functions 119 ScreenX and 120 ScreenY, and MenuLoad: while a zoom view is in effect the window's size
// (see "The scripts' screen" at the top). Otherwise the engine's screen, as before.
float __cdecl script_screen_x() {
    return g_view ? static_cast<float>(window_w()) : at<float>(graphics(), 0x20c);
}
float __cdecl script_screen_y() {
    return g_view ? static_cast<float>(window_h()) : at<float>(graphics(), 0x210);
}
__declspec(naked) void screen_x_stub() {
    __asm {
        push ecx
        push edx
        call script_screen_x
        pop edx
        pop ecx
        ret
    }
}
__declspec(naked) void screen_y_stub() {
    __asm {
        push ecx
        push edx
        call script_screen_y
        pop edx
        pop ecx
        ret
    }
}

using MenuLoad_t = int(__cdecl*)(void* args);
MenuLoad_t g_menu_load = nullptr;
int __cdecl menu_load(void* args) {
    FloatSwap f;
    if (g_view) f.enter(window_w(), window_h());
    const int r = g_menu_load(args);
    f.leave();
    return r;
}

// Layers 0 and 7 have no culling test: their draw call asks the filter too (whole_layer_filter, below).
__declspec(naked) void whole_layer_draw() {
    __asm {
        push ecx
        push ecx
        call whole_layer_filter
        add esp, 4
        pop ecx
        test eax, eax
        jz skip
        mov edx, [ecx]
        jmp dword ptr [edx + 0x14]   // returns to World_DrawLayer
    skip:
        ret
    }
}

// ---- the stretch and the interface's pass ----

IDirectDraw7* ddraw_of(IDirectDrawSurface7* s) {
    static const GUID IID_DD7 = {0x15e65ec0, 0x3b9c, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b}};
    IUnknown* unknown = nullptr;
    if (FAILED(s->GetDDInterface(reinterpret_cast<void**>(&unknown))) || !unknown) return nullptr;
    IDirectDraw7* dd = nullptr;
    unknown->QueryInterface(IID_DD7, reinterpret_cast<void**>(&dd));
    unknown->Release();
    return dd;
}

IDirectDrawSurface7* make_surface(IDirectDrawSurface7* like, int w, int h, DWORD caps) {
    IDirectDraw7* dd = ddraw_of(like);
    if (!dd) return nullptr;
    DDSURFACEDESC2 desc = {};
    desc.dwSize = sizeof desc;
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    desc.dwWidth = w;
    desc.dwHeight = h;
    desc.ddpfPixelFormat.dwSize = sizeof desc.ddpfPixelFormat;
    like->GetPixelFormat(&desc.ddpfPixelFormat);
    desc.ddsCaps.dwCaps = caps;
    IDirectDrawSurface7* s = nullptr;
    const HRESULT hr = dd->CreateSurface(&desc, &s, nullptr);
    dd->Release();
    if (FAILED(hr)) {
        patch::log("zoom: cannot create a %dx%d surface, caps %08X (%08X)", w, h, static_cast<unsigned>(caps),
                   static_cast<unsigned>(hr));
        return nullptr;
    }
    return s;
}

// The filtered stretch's texture: as big as the biggest view drawn (a glide's bigger end), made once for it.
IDirectDrawSurface7* g_texture = nullptr;
int g_texture_w = 0, g_texture_h = 0;
bool g_texture_failed = false;

void release_texture() {
    if (g_texture) g_texture->Release();
    g_texture = nullptr;
    g_texture_w = g_texture_h = 0;
    g_texture_failed = false;
}

bool ensure_texture(IDirectDrawSurface7* like, int w, int h) {
    if (g_texture && g_texture_w >= w && g_texture_h >= h) {
        if (g_texture->IsLost() == DDERR_SURFACELOST) g_texture->Restore();
        return true;
    }
    if (g_texture_failed) return false;
    if (g_texture) g_texture->Release();
    g_texture = make_surface(like, w, h, DDSCAPS_TEXTURE | DDSCAPS_VIDEOMEMORY);
    if (!g_texture) {   // a power of two, if the device wants one
        int pw = 1, ph = 1;
        while (pw < w) pw *= 2;
        while (ph < h) ph *= 2;
        g_texture = make_surface(like, pw, ph, DDSCAPS_TEXTURE | DDSCAPS_VIDEOMEMORY);
        w = pw;
        h = ph;
    }
    if (!g_texture) {
        g_texture_failed = true;
        patch::log("zoom: no texture for the filtered stretch - the nearest-pixel one is used");
        return false;
    }
    g_texture_w = w;
    g_texture_h = h;
    return true;
}

using SetState_t = int(__fastcall*)(void* g, void* edx, int state, int value);
void rs(uint8_t* g, int state, int value) { reinterpret_cast<SetState_t>(SET_RENDER_STATE)(g, nullptr, state, value); }
void* device(uint8_t* g) { return at<void*>(g, 0xbf4); }
void tss(uint8_t* g, int state, int value) {
    using Tss_t = HRESULT(__stdcall*)(void*, DWORD, DWORD, DWORD);
    reinterpret_cast<Tss_t>(vtable(device(g))[37])(device(g), 0, state, value);
}
void begin_scene(uint8_t* g) { reinterpret_cast<HRESULT(__stdcall*)(void*)>(vtable(device(g))[5])(device(g)); }
void end_scene(uint8_t* g) { reinterpret_cast<HRESULT(__stdcall*)(void*)>(vtable(device(g))[6])(device(g)); }
// IDirect3DDevice7::Clear of the z-buffer to the engine's empty depth (Graphics_Clear fills 0x3ff)
void clear_z(uint8_t* g) {
    using Clear_t = HRESULT(__stdcall*)(void*, DWORD, void*, DWORD, DWORD, float, DWORD);
    reinterpret_cast<Clear_t>(vtable(device(g))[10])(device(g), 0, nullptr, 2 /*D3DCLEAR_ZBUFFER*/, 0, 0x3ff / 65535.0f, 0);
}

IDirectDrawSurface7*& field(uint8_t* g, int offset) { return at<IDirectDrawSurface7*>(g, offset); }
constexpr int BACK = 0xc04, ZBUF = 0xbfc, DEPTH = 0xbf8;

void unlock_all(uint8_t* g) {
    if (at<void*>(g, 0x208)) {
        field(g, BACK)->Unlock(nullptr);
        at<void*>(g, 0x208) = nullptr;
    }
    if (at<void*>(g, 0x218)) {
        field(g, DEPTH)->Unlock(nullptr);
        at<void*>(g, 0x218) = nullptr;
    }
}

using Lock_t = void*(__stdcall*)(void* surface, int* pitch, int flags);
void lock_back(uint8_t* g) {
    if (at<void*>(g, 0x208)) return;
    int pitch = 0;
    at<void*>(g, 0x208) = reinterpret_cast<Lock_t>(SURFACE_LOCK)(field(g, BACK), &pitch, 0);
    at<int>(g, 0x214) = pitch / ((at<uint32_t>(g, 0) & 2) ? 4 : 2);
}
void unlock_back(uint8_t* g) {
    if (!at<void*>(g, 0x208)) return;
    field(g, BACK)->Unlock(nullptr);
    at<void*>(g, 0x208) = nullptr;
}

using Layer_t = void(__fastcall*)(void* world, void* edx, int layer);
void layer(int n) {
    g_cur_layer = n;
    reinterpret_cast<Layer_t>(DRAW_LAYER)(world(), nullptr, n);
    g_cur_layer = -1;
}

// The overlay: the interface's software sprites, put on the picture with a colour-keyed Blt.
IDirectDrawSurface7* g_overlay = nullptr;
int g_overlay_w = 0, g_overlay_h = 0;
constexpr uint32_t KEY16 = 0xF81F;   // magenta in R5G6B5: the overlay's transparent colour (no picture of the
                                     // interface has it: its pictures come from 8-bit palettes through Gamma)

void release_overlay() {
    if (g_overlay) g_overlay->Release();
    g_overlay = nullptr;
    g_overlay_w = g_overlay_h = 0;
}

bool ensure_overlay(IDirectDrawSurface7* like, int w, int h) {
    if (g_overlay && g_overlay_w == w && g_overlay_h == h) {
        if (g_overlay->IsLost() == DDERR_SURFACELOST) g_overlay->Restore();
        return true;
    }
    release_overlay();
    g_overlay = make_surface(like, w, h, DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY);
    if (!g_overlay) return false;
    DDCOLORKEY key = {KEY16, KEY16};
    g_overlay->SetColorKey(DDCKEY_SRCBLT, &key);
    g_overlay_w = w;
    g_overlay_h = h;
    return true;
}

void clear_overlay() {
    DDSURFACEDESC2 d = {};
    d.dwSize = sizeof d;
    if (FAILED(g_overlay->Lock(nullptr, &d, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr))) return;
    const uint32_t pair = KEY16 | (KEY16 << 16);
    for (int y = 0; y < g_overlay_h; ++y) {
        auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(d.lpSurface) + y * d.lPitch);
        std::fill(row, row + g_overlay_w / 2, pair);
    }
    g_overlay->Unlock(nullptr);
}

void put_overlay(IDirectDrawSurface7* picture) {
    RECT r = {0, 0, g_overlay_w, g_overlay_h};
    picture->Blt(&r, g_overlay, &r, DDBLT_KEYSRC | DDBLT_WAIT, nullptr);
}

// The software layers' depth: plain memory, filled with the engine's empty value.
uint16_t* g_depth = nullptr;
int g_depth_w = 0, g_depth_h = 0;
void own_depth(uint8_t* g, int w, int h) {
    if (!g_depth || g_depth_w != w || g_depth_h != h) {
        delete[] g_depth;
        g_depth = new uint16_t[static_cast<size_t>(w) * h];
        g_depth_w = w;
        g_depth_h = h;
    }
    std::fill(g_depth, g_depth + static_cast<size_t>(w) * h, static_cast<uint16_t>(0x3ff));
    at<void*>(g, 0x218) = g_depth;
    at<int>(g, 0x21c) = w;
}

// The minimap's highlight brightens what is under it, the dots too: they reach the picture with the overlay,
// so the highlight waits for it (minimap.cpp asks zoom_view_defer_brighten).
bool g_defer_brighten = false;
bool g_brighten_pending = false;
float g_brighten[4] = {};
uint32_t g_brighten_color = 0;
using Brighten_t = void(__fastcall*)(void* g, void* edx, float l, float t, float r, float b, uint32_t color);

// The filtered stretch: `source` of `from` copied into the texture (outside a scene), drawn as one quad.
bool upload_filtered(IDirectDrawSurface7* from, const RECT& source) {
    const int w = source.right - source.left, h = source.bottom - source.top;
    if (!ensure_texture(from, (std::max)(w, g_surface_w), (std::max)(h, g_surface_h))) return false;
    RECT to = {0, 0, w, h};
    return SUCCEEDED(g_texture->Blt(&to, from, const_cast<RECT*>(&source), DDBLT_WAIT, nullptr));
}

bool draw_filtered(uint8_t* g, const RECT& source, const RECT& target) {
    const int w = source.right - source.left, h = source.bottom - source.top;
    struct Vertex { float x, y, z, rhw, u, v; };
    const float u = static_cast<float>(w) / g_texture_w, v = static_cast<float>(h) / g_texture_h;
    const float l = target.left - 0.5f, t = target.top - 0.5f, r = target.right - 0.5f, b = target.bottom - 0.5f;
    const Vertex quad[4] = {{l, t, 0, 1, 0, 0}, {r, t, 0, 1, u, 0}, {r, b, 0, 1, u, v}, {l, b, 0, 1, 0, v}};
    using SetTexture_t = HRESULT(__stdcall*)(void*, DWORD, IDirectDrawSurface7*);
    using Draw_t = HRESULT(__stdcall*)(void*, DWORD, DWORD, const void*, DWORD, DWORD);
    reinterpret_cast<SetTexture_t>(vtable(device(g))[35])(device(g), 0, g_texture);
    rs(g, 0x1b, 0);    // no blending
    rs(g, 0x17, 8);    // z: always
    rs(g, 0xe, 0);     // no z writes
    tss(g, 16, 2);     // MAGFILTER linear
    tss(g, 17, 2);     // MINFILTER linear
    tss(g, 12, 3);     // ADDRESS clamp
    const HRESULT hr = reinterpret_cast<Draw_t>(vtable(device(g))[25])(device(g), 6 /*FAN*/, 0x104 /*XYZRHW|TEX1*/,
                                                                       quad, 4, 0);
    tss(g, 16, 1);
    tss(g, 17, 1);
    tss(g, 12, 1);
    rs(g, 0xe, 1);
    reinterpret_cast<SetTexture_t>(vtable(device(g))[35])(device(g), 0, nullptr);
    return SUCCEEDED(hr);
}

// Black round the picture: the parts of the window outside `target` (a view smaller than the window's).
void clear_outside(IDirectDrawSurface7* picture, const RECT& target, int w, int h) {
    DDBLTFX fx = {};
    fx.dwSize = sizeof fx;
    RECT bars[4] = {{0, 0, w, target.top}, {0, target.bottom, w, h}, {0, target.top, target.left, target.bottom},
                    {target.right, target.top, w, target.bottom}};
    for (RECT& r : bars)
        if (r.left < r.right && r.top < r.bottom) picture->Blt(&r, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
}

// Layers 2-10 with the interface's objects only, the minimap's inside, then the effects; the scene is begun.
void interface_pass(uint8_t* g, IDirectDrawSurface7* picture, int w, int h) {
    own_depth(g, w, h);
    IDirectDrawSurface7* real_back = field(g, BACK);
    field(g, BACK) = g_overlay;
    g_pass = 2;
    find_d3d_top();
    // the software sprites up to the Direct3D items' depth
    g_sw_part = 1;
    rs(g, 0x1b, 0);
    rs(g, 0x17, 8);
    rs(g, 0xe, 1);
    tss(g, 0x10, 1);
    clear_overlay();
    lock_back(g);
    layer(2);
    layer(3);
    layer(4);
    unlock_back(g);
    end_scene(g);
    put_overlay(picture);
    begin_scene(g);
    // the Direct3D items, as Graphics_DrawScene draws layers 5-9, against an empty z-buffer
    clear_z(g);
    rs(g, 0x17, 7);
    layer(5);
    rs(g, 0xe, 0);
    rs(g, 0x1b, 1);
    tss(g, 0x10, 1);
    layer(6);
    layer(7);
    tss(g, 0x10, 2);
    layer(8);
    tss(g, 0x10, 1);
    layer(9);
    rs(g, 0xe, 1);
    tss(g, 0x10, 1);
    // the software sprites above them, layer 10 (messages), the minimap's dots
    clear_overlay();
    lock_back(g);
    g_sw_part = 2;
    layer(2);
    layer(3);
    layer(4);
    g_sw_part = 0;
    layer(10);
    unlock_back(g);
    at<void*>(g, 0x218) = nullptr;   // our depth: never Unlock-ed
    g_pass = 0;
    if (g_minimap_pending) {
        g_defer_brighten = true;
        reinterpret_cast<Minimap_t>(MINIMAP_DRAW)(&g_minimap, nullptr);
        g_defer_brighten = false;
        g_minimap_pending = false;
    }
    unlock_back(g);
    end_scene(g);
    put_overlay(picture);
    begin_scene(g);
    field(g, BACK) = real_back;
    if (g_brighten_pending) {
        reinterpret_cast<Brighten_t>(BRIGHTEN_RECT)(g, nullptr, g_brighten[0], g_brighten[1], g_brighten[2],
                                                    g_brighten[3], g_brighten_color);
        g_brighten_pending = false;
    }
    // the screen effects over the whole window (their timing ran in the world pass; the scroll effect not again)
    uint32_t& scroll = at<uint32_t>(g, EFFECT_START + 4 * EFF_SCROLL);
    const uint32_t kept_scroll = scroll;
    scroll = 0;
    reinterpret_cast<Effects_t>(DRAW_EFFECTS)(g, nullptr, 1);
    scroll = kept_scroll;
    unlock_all(g);
}

// ---- the screenshot ----

using Shot_t = void(__fastcall*)(void* g, void* edx, const char* file, int x, int y, int w, int h);
Shot_t g_shot = nullptr;
// Under a zoom (a menu too) the final picture (the window-sized back buffer the frame was presented from), not the
// logical one.
void __fastcall save_screenshot(uint8_t* g, void* edx, const char* file, int x, int y, int w, int h) {
    IDirectDrawSurface7* game[3];
    if (!g_view || !screen_game_surfaces(game)) return g_shot(g, edx, file, x, y, w, h);
    unlock_all(g);
    IDirectDrawSurface7* ours = field(g, BACK);
    FloatSwap f;
    f.enter(window_w(), window_h());
    field(g, BACK) = game[0];
    g_shot(g, edx, file, 0, 0, window_w(), window_h());
    unlock_all(g);
    field(g, BACK) = ours;
    f.leave();
}

bool call_to(uintptr_t site, uintptr_t target) {
    const auto* b = reinterpret_cast<const uint8_t*>(site);
    if (b[0] != 0xE8) return false;
    int32_t rel;
    std::memcpy(&rel, b + 1, 4);
    return site + 5 + rel == target;
}

int test_value(const char* name, int fallback) {
    char value[8] = {};
    if (!GetEnvironmentVariableA("LOCOFIX_TEST", value, sizeof value) || value[0] != '1') return fallback;
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "SOFTWARE\\Gromada\\Locoland", 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return fallback;
    DWORD v = 0, size = sizeof v, type = 0;
    const LONG r = RegQueryValueExA(key, name, nullptr, &type, reinterpret_cast<BYTE*>(&v), &size);
    RegCloseKey(key);
    return r == ERROR_SUCCESS && type == REG_DWORD ? static_cast<int>(v) : fallback;
}

}  // namespace

// view_culling.cpp (and the stub for layers 0 and 7): -1 no opinion (the usual test), 0 leave out, 1 draw.
int zoom_draw_filter(const uint8_t* object) {
    if (!g_installed || !split_now()) return -1;
    if (g_pass == 2) {
        if (!is_interface(object)) return 0;
        if (g_sw_part && g_cur_layer >= 2 && g_cur_layer <= 4) {
            const float z = *reinterpret_cast<const float*>(object + 0x38);
            return (g_sw_part == 1) == (z <= g_d3d_top) ? 1 : 0;
        }
        return 1;
    }
    return is_interface(object) ? 0 : -1;
}

int __cdecl whole_layer_filter(const uint8_t* object) { return zoom_draw_filter(object); }

// minimap.cpp: the world's view for the highlight while the minimap is drawn in the interface's pass
bool zoom_view_world_view(float& w, float& h) {
    if (g_world_view_w <= 0) return false;
    w = g_world_view_w;
    h = g_world_view_h;
    return true;
}

// minimap.cpp, before it brightens the view's part: true = kept for after the overlay
bool zoom_view_defer_brighten(float l, float t, float r, float b, uint32_t color) {
    if (!g_defer_brighten) return false;
    g_brighten[0] = l;
    g_brighten[1] = t;
    g_brighten[2] = r;
    g_brighten[3] = b;
    g_brighten_color = color;
    g_brighten_pending = true;
    return true;
}

// screen_scaling.cpp: this frame is drawn in two passes
bool zoom_view_split() { return g_installed && split_now(); }

// status_bar.cpp, around building the bar: the window's layout while a zoom view is in effect. Returns a token
// for zoom_view_layout_end (nullptr: nothing changed).
void* zoom_view_layout_begin() {
    if (!g_view || !graphics()) return nullptr;
    auto* f = new FloatSwap();
    f->enter(window_w(), window_h());
    return f;
}
void zoom_view_layout_end(void* token) {
    if (!token) return;
    auto* f = static_cast<FloatSwap*>(token);
    f->leave();
    delete f;
}

// status_bar.cpp / minimap.cpp: map pixels per window pixel of the interface's height (the camera's room below
// the map is the bar's height in map pixels), 1 at the level's own screen.
double zoom_view_room() { return g_view ? 1.0 / g_z : 1.0; }

// screen_scaling.cpp, after Direct3D's EndScene of a split frame: the stretch of `source` (the logical picture
// in `back_buffer`, our surface) into `target` of `picture` (the game's window-sized back buffer), then the
// interface's pass at the window's size. False = not done (screen_scaling's own stretch follows).
bool zoom_view_present(uint8_t* g, IDirectDrawSurface7* back_buffer, IDirectDrawSurface7* picture, const RECT& source,
                       const RECT& target, const RECT& window) {
    IDirectDrawSurface7* game[3];
    if (!screen_game_surfaces(game) || game[0] != picture) return false;
    const int W = window.right, H = window.bottom;
    if (!ensure_overlay(picture, W, H)) return false;
    unlock_all(g);
    IDirectDrawSurface7* ours[3] = {field(g, BACK), field(g, ZBUF), field(g, DEPTH)};
    const bool uploaded = g_filtered && upload_filtered(back_buffer, source);
    if (!screen_set_render_target(g, game[0])) return false;
    field(g, BACK) = game[0];
    field(g, ZBUF) = game[1];
    field(g, DEPTH) = game[2];
    g_world_view_w = static_cast<float>(source.right - source.left);
    g_world_view_h = static_cast<float>(source.bottom - source.top);
    clear_outside(picture, target, W, H);
    screen_set_screen(g, W, H);
    if (!uploaded) picture->Blt(const_cast<RECT*>(&target), back_buffer, const_cast<RECT*>(&source), DDBLT_WAIT, nullptr);
    begin_scene(g);
    if (uploaded && !draw_filtered(g, source, target)) {
        end_scene(g);
        picture->Blt(const_cast<RECT*>(&target), back_buffer, const_cast<RECT*>(&source), DDBLT_WAIT, nullptr);
        begin_scene(g);
    }
    interface_pass(g, picture, W, H);
    end_scene(g);
    g_world_view_w = g_world_view_h = 0;
    screen_set_render_target(g, ours[0]);
    field(g, BACK) = ours[0];
    field(g, ZBUF) = ours[1];
    field(g, DEPTH) = ours[2];
    screen_set_screen(g, source.right - source.left, source.bottom - source.top);
    return true;
}

// ---- the interface's objects into the window's layout and back (a map smaller than the window) ----

constexpr uintptr_t OBJECT_MOVE_TO = 0x446640;   // Object_MoveTo
using MoveTo_t = void(__fastcall*)(void* object, void* edx, float x, float y, float z);

// The level's own stretch: its screen (screen_base_size) into the window, keeping its proportions, centred.
void own_stretch(double& s, double& left, double& top) {
    int bw = 0, bh = 0;
    screen_base_size(bw, bh);
    s = (std::min)(static_cast<double>(window_w()) / bw, static_cast<double>(window_h()) / bh);
    left = (window_w() - bw * s) / 2;
    top = (window_h() - bh * s) / 2;
}

// Every interface object keeps its place on the screen while the layout changes between the level's own stretched
// screen and the window's: a screen point p of the own screen is at left/top + p * s in the window.
void move_interface(bool to_window) {
    double s, left, top;
    own_stretch(s, left, top);
    if (std::fabs(s - 1.0) < 1e-6 && std::fabs(left) < 0.5 && std::fabs(top) < 0.5) return;   // the window's already
    uint8_t* w = world();
    const float cx = at<float>(w, 0x44), cy = at<float>(w, 0x48);
    const int count = at<int>(w, 0x24c + 4);
    uint8_t** items = at<uint8_t**>(w, 0x24c + 0xc);
    for (int i = 0; i < count; ++i) {
        uint8_t* o = items[i];
        if (!o) continue;
        const float z = at<float>(o, 0x38);
        double px = at<float>(o, 0x30) - cx, py = at<float>(o, 0x34) - z - cy;
        if (to_window) {
            px = left + px * s;
            py = top + py * s;
        } else {
            px = (px - left) / s;
            py = (py - top) / s;
        }
        reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(o, nullptr, static_cast<float>(cx + px), static_cast<float>(cy + py + z), z);
    }
}

// ---- zoom.cpp's view functions ----

// The level's own factor: 1, or the stretch of a small map's screen (mission 1 at 1920x1080: 1.40625).
double zoom_view_base() {
    int bw = 0, bh = 0;
    screen_base_size(bw, bh);
    if (bw <= 0 || bh <= 0) return 1.0;
    return (std::min)(static_cast<double>(window_w()) / bw, static_cast<double>(window_h()) / bh);
}

// The factor that shows the whole map (its longer side, relative to the window, fills the window).
double zoom_view_whole() {
    if (map_w() <= 0 || map_h() <= 0) return 1.0;
    return (std::min)(static_cast<double>(window_w()) / map_w(), static_cast<double>(window_h()) / map_h());
}

// The view z shows: the window / z on each axis, at most the map.
void zoom_view_size(double z, int& w, int& h) {
    w = static_cast<int>(std::lround(window_w() / z));
    h = static_cast<int>(std::lround(window_h() / z));
    if (w > map_w()) w = map_w();
    if (h > map_h()) h = map_h();
    if (w < 1) w = 1;
    if (h < 1) h = 1;
}

// zoom.cpp, between two frames: the game draws zoom z from the next frame on, in surfaces big enough for
// `surface_z` too (a glide's other end). False = nothing changed.
bool zoom_view_apply(double z, double surface_z) {
    if (!g_installed || !world() || !graphics()) return false;
    const bool was_view = g_view;
    uint8_t* player = at<uint8_t*>(world(), 0x220);   // World.players[0], the human
    if (std::fabs(z - zoom_view_base()) < 1e-6) {
        if (!screen_set_view(1, 1, 0, 0, nullptr)) return false;
        g_view = false;
        g_menu = false;
        g_z = z;
        release_texture();
        release_overlay();
    } else {
        int w, h, sw, sh;
        zoom_view_size(z, w, h);
        zoom_view_size((std::min)(z, surface_z), sw, sh);
        const int tw = static_cast<int>(std::lround(w * z)), th = static_cast<int>(std::lround(h * z));
        RECT target;
        target.left = (window_w() - (std::min)(tw, window_w())) / 2;
        target.top = (window_h() - (std::min)(th, window_h())) / 2;
        target.right = target.left + (std::min)(tw, window_w());
        target.bottom = target.top + (std::min)(th, window_h());
        if (!screen_set_view(w, h, sw, sh, &target)) return false;
        fog_buffer_cover((std::max)(w, sw), (std::max)(h, sh));
        g_view = true;
        g_menu = false;
        g_z = z;
        g_view_w = w;
        g_view_h = h;
        g_surface_w = sw;
        g_surface_h = sh;
        g_target = target;
        const bool whole = std::fabs(z - std::round(z)) < 1e-6 && z >= 1.0;   // 2, 3: exact big pixels
        g_filtered = g_filter_setting == 1 || (g_filter_setting == 2 && !whole);
    }
    if (g_view != was_view) {   // the interface's layout space changed: the window's, or the level's screen
        move_interface(g_view);
        status_bar_rebuild(player);
        interface_messages_rescale();
    } else if (player) {
        status_bar_room(player);
    }
    return true;
}

// zoom.cpp, between two frames: a menu up gets a screen of at least 1024x768 drawn in one pass, and the zoom's
// view back when it goes.
float g_menu_camera[2] = {};   // the camera when a menu came up

void zoom_view_between_frames() {
    if (!g_installed || !g_view || !graphics()) return;
    const bool menu = menu_background_shown();
    if (menu && !g_menu) {
        // the window's size, as at zoom 1 (screen_scaling.cpp's menu rule takes its 1024x768 middle)
        const int mw = window_w(), mh = window_h();
        const RECT whole = {0, 0, mw, mh};
        g_menu_camera[0] = at<float>(world(), 0x44);
        g_menu_camera[1] = at<float>(world(), 0x48);
        if (screen_set_view(mw, mh, (std::max)(mw, g_surface_w), (std::max)(mh, g_surface_h), &whole)) g_menu = true;
    } else if (!menu && g_menu) {
        if (!screen_set_view(g_view_w, g_view_h, g_surface_w, g_surface_h, &g_target)) return;
        g_menu = false;
        // the camera where it was before the menu (a menu's screen bigger than the map moves it: camera.cpp
        // centres the map in it); SetPosition moves the interface's objects with it (the bar built at the menu's end)
        using SetPosition_t = void(__fastcall*)(void* world, void* edx, float x, float y, int mode);
        reinterpret_cast<SetPosition_t>(0x41c680)(world(), nullptr, g_menu_camera[0] + g_view_w * 0.5f,
                                                  g_menu_camera[1] + g_view_h * 0.5f, 0);
    }
}

// zoom.cpp: a level is loading - its own screen again (screen_scaling.cpp does the surfaces).
void zoom_view_level_loading() {
    g_view = false;
    g_menu = false;
    g_z = 1.0;
    g_minimap_pending = false;
    g_brighten_pending = false;
}

void install_zoom_view() {
    struct Site { uintptr_t at, to; void* fn; } sites[] = {
        {CALL_MENU_UPDATE, MENU_UPDATE, reinterpret_cast<void*>(&menu_update)},
        {CALL_MOUSE_TIP, MOUSE_TIP_UPDATE, reinterpret_cast<void*>(&mouse_tip_update)},
        {CALL_MINIMAP_DRAW, MINIMAP_DRAW, reinterpret_cast<void*>(&minimap_draw)},
        {CALL_STATE_BAR, UPDATE_STATE_BAR, reinterpret_cast<void*>(&state_bar_update)},
        {CALL_PLACE_OVER, PLACE_OVER, reinterpret_cast<void*>(&place_over_for_order)},
        {CALL_EFFECTS_A, DRAW_EFFECTS, reinterpret_cast<void*>(&world_effects)},
        {CALL_EFFECTS_B, DRAW_EFFECTS, reinterpret_cast<void*>(&world_effects)},
    };
    const uint8_t whole_draw[] = {0x8B, 0x11, 0xFF, 0x52, 0x14};                    // mov edx,[ecx] / call [edx+0x14]
    const uint8_t screen_x[] = {0xA1, 0x14, 0x9A, 0x4A, 0x00, 0xD9, 0x80, 0x0C, 0x02, 0x00, 0x00};
    const uint8_t screen_y[] = {0x8B, 0x0D, 0x14, 0x9A, 0x4A, 0x00, 0xD9, 0x81, 0x10, 0x02, 0x00, 0x00};
    const uint8_t shot_start[] = {0x6A, 0xFF, 0x68, 0x88, 0x27, 0x47, 0x00};         // push -1 / push 0x472788
    const uint8_t menu_load_start[] = {0x6A, 0xFF, 0x68, 0x08, 0x37, 0x47, 0x00};    // push -1 / push 0x473708
    bool ok = patch::check(WHOLE_LAYER_DRAW, whole_draw, sizeof whole_draw) &&
              patch::check(SCREEN_X, screen_x, sizeof screen_x) && patch::check(SCREEN_Y, screen_y, sizeof screen_y) &&
              patch::check(SAVE_SCREENSHOT, shot_start, sizeof shot_start) &&
              patch::check(MENU_LOAD, menu_load_start, sizeof menu_load_start);
    for (const Site& s : sites) ok = ok && call_to(s.at, s.to);
    if (!ok) {
        patch::log("skipped the zoom's two passes: unexpected bytes (the zoom stays off)");
        return;
    }
    for (const Site& s : sites) patch::write_call(s.at, nullptr, 0, s.fn, 5);
    patch::write_call(WHOLE_LAYER_DRAW, nullptr, 0, reinterpret_cast<void*>(&whole_layer_draw), sizeof whole_draw);
    patch::write_call(SCREEN_X, nullptr, 0, reinterpret_cast<void*>(&screen_x_stub), sizeof screen_x);
    patch::write_call(SCREEN_Y, nullptr, 0, reinterpret_cast<void*>(&screen_y_stub), sizeof screen_y);
    g_shot = reinterpret_cast<Shot_t>(patch::hook(SAVE_SCREENSHOT, shot_start, sizeof shot_start,
                                                  reinterpret_cast<void*>(&save_screenshot), "Graphics_SaveScreenshot"));
    g_menu_load = reinterpret_cast<MenuLoad_t>(
        patch::hook(MENU_LOAD, menu_load_start, sizeof menu_load_start, reinterpret_cast<void*>(&menu_load), "MenuLoad"));
    g_filter_setting = test_value("ZoomFilter", 2);
    g_installed = true;
}

// zoom.cpp: whether the view can draw zooms at all
bool zoom_view_installed() { return g_installed; }
