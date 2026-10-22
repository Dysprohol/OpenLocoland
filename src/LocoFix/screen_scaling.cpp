// Logical screen: the game field fills the window at any resolution.
//
// The engine lays out everything - camera limits, unit bar, minimap, counters, messages,
// menus - against the screen size (graphics +0x20c/+0x210) and the view rectangle
// (+0x220..+0x22c). When the map is smaller than the screen (mission 1 and the 1024x768
// menu map at widescreen resolutions), the map sat in the middle while the interface went
// to the window corners, far from the map.
//
// So after each level load the game gets a logical screen of min(window, map) on each
// axis: it renders into the top-left part of the back buffer exactly as it would at that
// resolution, interface included. Before the frame is presented that part is stretched
// over the window keeping its proportions. The same goes for menus drawn for 1024x768
// inside a bigger logical screen (background VID 465 in the main menu and end screens,
// 466 in the in-mission menu): while such a background exists, the 1024x768 centre is
// stretched instead.
//
// The stretch goes through a copy (a Blt between off-screen surfaces, filtered only with
// dgVoodoo's Bilinear2DOperations, which game/dgVoodoo.conf leaves off - with it dgVoodoo
// shifts every Blt to the window by a pixel; see DEVELOPERS.md, LocoFix fix 23); in a window
// the frame is then presented here instead of by Graphics_EndScene, and the game gets a
// back buffer and depth buffers of the logical size (see "surfaces at the logical size").
// The mouse is mapped the other way in WM_NCHITTEST (the game takes its position from
// there). The fade-in effect (Effect 5) snapshots the screen; after a stretched frame the
// snapshot gets the unstretched picture, otherwise it would be stretched twice. The FPS
// counter (frame_limit.cpp) is drawn on the final picture, or with logical-sized surfaces
// on the logical one, stretched with it.
#include "patch.h"
#include <ddraw.h>
#include <windowsx.h>

// frame_limit.cpp: the FPS counter, drawn on the final picture
void draw_fps_counter(IDirectDrawSurface7* back_buffer, const RECT& picture, double scale);

namespace {

constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t LEVEL_LOAD = 0x41cff0;
constexpr uintptr_t AFTER_D3D_ENDSCENE = 0x414da9;   // Graphics_EndScene after IDirect3DDevice7::EndScene
constexpr uintptr_t END_SCENE_EPILOGUE = 0x414f4e;   // Graphics_EndScene: pop edi ... ret 4
constexpr uintptr_t EFFECT = 0x415a20;
constexpr uintptr_t SET_VIEWPORT = 0x414f60;
constexpr uintptr_t CREATE_SURFACE = 0x4155d0;
constexpr uintptr_t DESTRUCT = 0x414490;   // Graphics_Destruct
constexpr uintptr_t IAT_RegisterClassA = 0x4751b8;
constexpr int MENU_WIDTH = 1024;
constexpr int MENU_HEIGHT = 768;
constexpr int MENU_BACKGROUND_VIDS[] = {465, 466};

const GUID IID_DirectDraw7 = {0x15e65ec0, 0x3b9c, 0x11d2, {0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b}};

using SetViewport_t = void(__fastcall*)(void* graphics, void* edx, float left, float top, float right, float bottom);
const auto g_set_viewport = reinterpret_cast<SetViewport_t>(SET_VIEWPORT);

uint8_t* graphics() { return *reinterpret_cast<uint8_t**>(G_GRAPHICS); }
float* screen_size(uint8_t* g) { return reinterpret_cast<float*>(g + 0x20c); }   // width, height

// ---- logical screen ----

int g_window_w = 0, g_window_h = 0;   // the resolution chosen in the launcher
int g_logical_w = 0, g_logical_h = 0; // 0 = the whole window

void set_screen(uint8_t* g, int w, int h) {
    screen_size(g)[0] = static_cast<float>(w);
    screen_size(g)[1] = static_cast<float>(h);
    g_set_viewport(g, nullptr, 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h));
}

// ---- surfaces at the logical size ----
//
// The back buffer, the hardware z-buffer and the software depth buffer are made at the
// window size. With a logical screen the game draws only its top-left part, but every
// frame it still locks the back buffer and the depth buffer whole for the software
// sprites and fills the whole depth buffer, and dgVoodoo copies (and converts) the whole
// surface each time: at 1920x1080 a 1024x768 picture cost as much as a full one. So in a
// window, while a logical screen is in effect, the game gets surfaces of that size and
// Direct3D renders into them; its own surfaces come back for the next level load and
// before Graphics_Destruct. In full screen the back buffer belongs to the flip chain and
// stays.

using CreateSurface_t = IDirectDrawSurface7*(__fastcall*)(uint8_t* g, void* edx, int width, int height, int kind,
                                                         DDPIXELFORMAT* format);
const auto g_create_surface = reinterpret_cast<CreateSurface_t>(CREATE_SURFACE);
constexpr int BACK_BUFFER = 0xc04, Z_BUFFER = 0xbfc, DEPTH_BUFFER = 0xbf8, D3D_DEVICE = 0xbf4;
constexpr int SURFACE_KINDS[] = {1, 5, 6};   // Graphics_CreateSurface: 3D back buffer, z-buffer, system-memory z
constexpr int SURFACE_FIELDS[] = {BACK_BUFFER, Z_BUFFER, DEPTH_BUFFER};

IDirectDrawSurface7*& surface(uint8_t* g, int offset) { return *reinterpret_cast<IDirectDrawSurface7**>(g + offset); }

IDirectDrawSurface7* g_small[3] = {};   // ours, in SURFACE_FIELDS order
int g_small_w = 0, g_small_h = 0;
IDirectDrawSurface7* g_game[3] = {};    // the game's, while ours are in use

void release_small() {
    for (auto*& s : g_small) {
        if (s) s->Release();
        s = nullptr;
    }
}

// As Graphics_EndScene does before presenting: software drawing locks again on demand.
void unlock(uint8_t* g) {
    if (*reinterpret_cast<void**>(g + 0x208)) {   // backBufferBits
        surface(g, BACK_BUFFER)->Unlock(nullptr);
        *reinterpret_cast<void**>(g + 0x208) = nullptr;
    }
    if (*reinterpret_cast<void**>(g + 0x218)) {   // depthBits
        surface(g, DEPTH_BUFFER)->Unlock(nullptr);
        *reinterpret_cast<void**>(g + 0x218) = nullptr;
    }
}

bool set_render_target(uint8_t* g, IDirectDrawSurface7* back_buffer) {
    // IDirect3DDevice7::SetRenderTarget (vtable slot 8); the z-buffer attached to it goes along
    using SetRenderTarget_t = HRESULT(__stdcall*)(void* device, IDirectDrawSurface7* target, DWORD flags);
    void* device = *reinterpret_cast<void**>(g + D3D_DEVICE);
    if (!device) return false;
    const auto set = reinterpret_cast<SetRenderTarget_t>((*reinterpret_cast<void***>(device))[8]);
    const HRESULT hr = set(device, back_buffer, 0);
    if (FAILED(hr)) patch::log("screen scaling: SetRenderTarget failed (%08X)", static_cast<unsigned>(hr));
    return SUCCEEDED(hr);
}

bool create_small(uint8_t* g, int w, int h) {
    if (g_small[0] && g_small_w == w && g_small_h == h) return true;
    release_small();
    for (int i = 0; i < 3; ++i) {
        DDPIXELFORMAT format = {};
        format.dwSize = sizeof format;
        auto* game = surface(g, SURFACE_FIELDS[i]);
        if (!game || FAILED(game->GetPixelFormat(&format)) ||
            !(g_small[i] = g_create_surface(g, nullptr, w, h, SURFACE_KINDS[i], &format))) {
            patch::log("screen scaling: cannot create %dx%d surfaces, the window-sized ones stay", w, h);
            release_small();
            return false;
        }
    }
    if (FAILED(g_small[0]->AddAttachedSurface(g_small[1]))) {
        patch::log("screen scaling: cannot attach the %dx%d z-buffer, the window-sized surfaces stay", w, h);
        release_small();
        return false;
    }
    g_small_w = w;
    g_small_h = h;
    return true;
}

void use_small_surfaces(uint8_t* g, int w, int h) {
    if (*reinterpret_cast<uint32_t*>(g) & 0x80) return;   // full screen
    if (g_game[0] || !create_small(g, w, h)) return;
    unlock(g);
    if (!set_render_target(g, g_small[0])) return;
    for (int i = 0; i < 3; ++i) {
        g_game[i] = surface(g, SURFACE_FIELDS[i]);
        surface(g, SURFACE_FIELDS[i]) = g_small[i];
    }
}

void use_game_surfaces(uint8_t* g) {
    if (!g_game[0]) return;
    unlock(g);
    set_render_target(g, g_game[0]);
    for (int i = 0; i < 3; ++i) {
        surface(g, SURFACE_FIELDS[i]) = g_game[i];
        g_game[i] = nullptr;
    }
}

using Destruct_t = void(__fastcall*)(uint8_t* g, void* edx);
Destruct_t g_destruct = nullptr;

void __fastcall Destruct_hook(uint8_t* g, void* edx) {
    use_game_surfaces(g);
    release_small();
    g_destruct(g, edx);
}

using LevelLoad_t = int(__fastcall*)(void* world, void* edx, const char* name);
LevelLoad_t g_level_load = nullptr;

int __fastcall LevelLoad_hook(void* world, void* edx, const char* name) {
    auto* g = graphics();
    if (g && !g_window_w) {
        g_window_w = static_cast<int>(screen_size(g)[0]);
        g_window_h = static_cast<int>(screen_size(g)[1]);
    }
    if (g && g_logical_w) {          // load with the whole window, as the game expects
        use_game_surfaces(g);
        set_screen(g, g_window_w, g_window_h);
        g_logical_w = g_logical_h = 0;
    }
    const int result = g_level_load(world, edx, name);
    auto* w = static_cast<uint8_t*>(world);
    const int map_w = static_cast<int>(*reinterpret_cast<float*>(w + 0x28));
    const int map_h = static_cast<int>(*reinterpret_cast<float*>(w + 0x2c));
    if (g && map_w > 0 && map_h > 0 && (map_w < g_window_w || map_h < g_window_h)) {
        g_logical_w = map_w < g_window_w ? map_w : g_window_w;
        g_logical_h = map_h < g_window_h ? map_h : g_window_h;
        use_small_surfaces(g, g_logical_w, g_logical_h);
        set_screen(g, g_logical_w, g_logical_h);
    }
    return result;
}

// ---- stretching ----

struct Scaling {
    RECT source;   // part of the back buffer with the picture
    RECT target;   // where it goes, stretched
};

bool g_last_frame_scaled = false;
Scaling g_last = {};

bool menu_on_screen() {
    auto* world = *reinterpret_cast<uint8_t**>(G_WORLD);
    if (!world) return false;
    const int count = *reinterpret_cast<int*>(world + 0x290);
    auto** vids = reinterpret_cast<uint8_t**>(world + 0x294);
    for (int n : MENU_BACKGROUND_VIDS) {
        if (n < count && vids[n] && *reinterpret_cast<int*>(vids[n] + 0x358) > 0) return true;
    }
    return false;
}

bool compute(uint8_t* g, Scaling& s) {
    const int window_w = g_window_w ? g_window_w : static_cast<int>(screen_size(g)[0]);
    const int window_h = g_window_h ? g_window_h : static_cast<int>(screen_size(g)[1]);
    int w = g_logical_w ? g_logical_w : window_w;
    int h = g_logical_h ? g_logical_h : window_h;
    int x = 0, y = 0;
    if (w >= MENU_WIDTH && h >= MENU_HEIGHT && (w > MENU_WIDTH || h > MENU_HEIGHT) && menu_on_screen()) {
        x = (w - MENU_WIDTH) / 2;
        y = (h - MENU_HEIGHT) / 2;
        w = MENU_WIDTH;
        h = MENU_HEIGHT;
    }
    if (w == window_w && h == window_h) return false;

    int tw = window_w, th = window_w * h / w;
    if (th > window_h) {
        th = window_h;
        tw = window_h * w / h;
    }
    const int tx = (window_w - tw) / 2, ty = (window_h - th) / 2;
    s.source = {x, y, x + w, y + h};
    s.target = {tx, ty, tx + tw, ty + th};
    return true;
}

IDirectDrawSurface7* g_copy = nullptr;          // unstretched picture of the last frame
IDirectDrawSurface7* g_copy_owner = nullptr;    // back buffer the copy was made for
int g_copy_w = 0, g_copy_h = 0;

void release_copy() {
    if (g_copy) g_copy->Release();
    g_copy = nullptr;
    g_copy_owner = nullptr;
}

bool prepare_copy(IDirectDrawSurface7* back_buffer, int w, int h) {
    if (g_copy && g_copy_owner == back_buffer && g_copy_w == w && g_copy_h == h) {
        if (g_copy->IsLost() == DDERR_SURFACELOST && FAILED(g_copy->Restore())) release_copy();
        if (g_copy) return true;
    }
    release_copy();

    IUnknown* unknown = nullptr;
    if (FAILED(back_buffer->GetDDInterface(reinterpret_cast<void**>(&unknown))) || !unknown) return false;
    IDirectDraw7* dd = nullptr;
    unknown->QueryInterface(IID_DirectDraw7, reinterpret_cast<void**>(&dd));
    unknown->Release();
    if (!dd) return false;

    DDSURFACEDESC2 desc = {};
    desc.dwSize = sizeof desc;
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
    desc.dwWidth = w;
    desc.dwHeight = h;
    desc.ddpfPixelFormat.dwSize = sizeof desc.ddpfPixelFormat;
    back_buffer->GetPixelFormat(&desc.ddpfPixelFormat);
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_VIDEOMEMORY;
    HRESULT hr = dd->CreateSurface(&desc, &g_copy, nullptr);
    if (FAILED(hr)) {
        desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
        hr = dd->CreateSurface(&desc, &g_copy, nullptr);
    }
    dd->Release();
    if (FAILED(hr)) {
        g_copy = nullptr;
        patch::log("screen scaling: cannot create a %dx%d surface (%08X)", w, h, static_cast<unsigned>(hr));
        return false;
    }
    g_copy_owner = back_buffer;
    g_copy_w = w;
    g_copy_h = h;
    return true;
}

void clear(IDirectDrawSurface7* surface, RECT* rect) {
    DDBLTFX fx = {};
    fx.dwSize = sizeof fx;
    surface->Blt(rect, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fx);
}

// Black bars: the parts of `area` around `picture`.
void clear_bars(IDirectDrawSurface7* target, const RECT& area, const RECT& picture) {
    RECT bars[4] = {
        {area.left, area.top, area.right, picture.top},
        {area.left, picture.bottom, area.right, area.bottom},
        {area.left, picture.top, picture.left, picture.bottom},
        {picture.right, picture.top, area.right, picture.bottom},
    };
    for (RECT& r : bars) {
        if (r.left < r.right && r.top < r.bottom) clear(target, &r);
    }
}

bool presents_with_flip(uint8_t* g) {
    const uint32_t flags = *reinterpret_cast<uint32_t*>(g);
    return (flags & 0x80) && !(flags & 0x400);
}

// Copies the window-sized picture to the game area of the window, as Graphics_EndScene does.
bool present_in_window(uint8_t* g, IDirectDrawSurface7* picture, const RECT& window) {
    auto* front_buffer = surface(g, 0xc00);
    POINT origin = {0, 0};
    if (!front_buffer || !ClientToScreen(*reinterpret_cast<HWND*>(g + 0xbd0), &origin)) return false;
    RECT target = window;
    OffsetRect(&target, origin.x, origin.y);
    return SUCCEEDED(front_buffer->Blt(&target, picture, const_cast<RECT*>(&window), DDBLT_WAIT, nullptr));
}

// Called in Graphics_EndScene after the scene ends; returns 1 when the frame is presented
// here and the game's own present is to be skipped.
//
// The picture is copied aside and stretched back from the copy: with Bilinear2DOperations
// on, dgVoodoo filters a stretched Blt only from a plain off-screen surface, not from the
// back buffer or into the window (the setting is off now, see above). With logical-sized surfaces (in a window) the game's own
// back buffer is free and gets the stretched picture; otherwise it goes back into the back
// buffer, where only the bars around it are cleared. In full screen the game flips the back
// buffer; in a window the picture is copied to the window here.
int __cdecl scale_frame(uint8_t* g, int present) {
    // the same conditions as for presenting in Graphics_EndScene
    if (!present || *reinterpret_cast<int*>(g + 0xbc0) || *reinterpret_cast<int*>(g + 0xb38) ||
        *reinterpret_cast<int*>(g + 0xb3c))
        return 0;
    g_last_frame_scaled = false;
    auto* back_buffer = surface(g, BACK_BUFFER);
    RECT window = {0, 0, g_window_w ? g_window_w : static_cast<int>(screen_size(g)[0]),
                   g_window_h ? g_window_h : static_cast<int>(screen_size(g)[1])};
    Scaling s;
    if (!compute(g, s)) {
        draw_fps_counter(back_buffer, window, 1.0);
        return 0;
    }
    if (!back_buffer) return 0;

    // With logical-sized surfaces the counter is drawn at the logical scale and stretched with
    // the picture: drawing on the window-sized one would make dgVoodoo copy it to memory and back.
    auto* picture = g_game[0] ? g_game[0] : back_buffer;
    const int w = s.source.right - s.source.left, h = s.source.bottom - s.source.top;
    if (picture != back_buffer) draw_fps_counter(back_buffer, s.source, 1.0);
    if (!prepare_copy(back_buffer, w, h) || FAILED(g_copy->Blt(nullptr, back_buffer, &s.source, DDBLT_WAIT, nullptr))) {
        release_copy();
        if (picture == back_buffer) draw_fps_counter(back_buffer, s.source, 1.0);
        return 0;
    }
    clear_bars(picture, window, s.target);
    picture->Blt(&s.target, g_copy, nullptr, DDBLT_WAIT, nullptr);
    g_last_frame_scaled = true;
    g_last = s;
    if (picture == back_buffer)
        draw_fps_counter(back_buffer, s.target, static_cast<double>(s.target.right - s.target.left) / w);
    if (presents_with_flip(g)) return 0;
    return present_in_window(g, picture, window) ? 1 : 0;
}

// Replaces `mov eax,[esi+0xbc0]` (esi = graphics, function argument at [esp+0x48]).
uintptr_t g_after_d3d_endscene_return = AFTER_D3D_ENDSCENE + 6;
uintptr_t g_end_scene_epilogue = END_SCENE_EPILOGUE;

__declspec(naked) void after_d3d_endscene() {
    __asm {
        pushad
        mov eax, [esp + 0x68]
        push eax
        push esi
        call scale_frame
        add esp, 8
        test eax, eax
        popad
        jnz presented
        mov eax, [esi + 0xbc0]
        jmp dword ptr [g_after_d3d_endscene_return]
    presented:
        jmp dword ptr [g_end_scene_epilogue]
    }
}

using Effect_t = void(__fastcall*)(uint8_t* g, void* edx, int effect, int a, int b, int duration);
Effect_t g_effect = nullptr;

// The fade-in effect snapshots the window; after a stretched frame the snapshot gets the
// picture at the logical size instead.
void __fastcall Effect_hook(uint8_t* g, void* edx, int effect, int a, int b, int duration) {
    g_effect(g, edx, effect, a, b, duration);
    if (effect != 5 || !g_last_frame_scaled) return;
    auto* snapshot = surface(g, 0xc08);
    if (!snapshot || !g_copy) return;
    clear(snapshot, nullptr);
    snapshot->Blt(&g_last.source, g_copy, nullptr, DDBLT_WAIT, nullptr);
}

// ---- mouse ----

WNDPROC g_game_wndproc = nullptr;
using RegisterClassA_t = ATOM(WINAPI*)(const WNDCLASSA*);
RegisterClassA_t g_register_class = nullptr;

bool to_logical(POINT& p) {
    Scaling s;
    auto* g = graphics();
    if (!g || !compute(g, s)) return false;
    p.x = s.source.left + MulDiv(p.x - s.target.left, s.source.right - s.source.left, s.target.right - s.target.left);
    p.y = s.source.top + MulDiv(p.y - s.target.top, s.source.bottom - s.source.top, s.target.bottom - s.target.top);
    return true;
}

LRESULT CALLBACK scaling_wndproc(HWND window, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_NCHITTEST) {
        POINT p = {GET_X_LPARAM(l), GET_Y_LPARAM(l)};
        ScreenToClient(window, &p);
        if (to_logical(p)) {
            ClientToScreen(window, &p);
            l = MAKELPARAM(p.x, p.y);
        }
    }
    return CallWindowProcA(g_game_wndproc, window, m, w, l);
}

ATOM WINAPI RegisterClassA_scaling(const WNDCLASSA* cls) {
    WNDCLASSA copy = *cls;
    g_game_wndproc = copy.lpfnWndProc;
    copy.lpfnWndProc = &scaling_wndproc;
    return g_register_class(&copy);
}

}  // namespace

// A point in the game area of the window -> the logical screen the game works in.
void window_to_logical(POINT& p) { to_logical(p); }

// True while a menu drawn for 1024x768 is shown (main menu, end screens, Esc menu).
bool menu_background_shown() { return menu_on_screen(); }

// Install after test mode: we take over the RegisterClassA entry it set and call it in
// turn, so test mode supplies the virtual cursor and we map it.
void install_screen_scaling() {
    const uint8_t after_d3d[] = {0x8B, 0x86, 0xC0, 0x0B, 0x00, 0x00};   // mov eax,[esi+0xbc0]
    // pop edi / pop esi / pop ebp / pop ebx / add esp,0x34 / ret 4
    const uint8_t epilogue[] = {0x5F, 0x5E, 0x5D, 0x5B, 0x83, 0xC4, 0x34, 0xC2, 0x04, 0x00};
    // push -1 / push 0x47308e (exception frame)
    const uint8_t level_load[] = {0x6A, 0xFF, 0x68, 0x8E, 0x30, 0x47, 0x00};
    // mov eax,[esp+4] / sub esp,0x128
    const uint8_t effect[] = {0x8B, 0x44, 0x24, 0x04, 0x81, 0xEC, 0x28, 0x01, 0x00, 0x00};
    // mov eax,[esp+4] / sub esp,0x48
    const uint8_t set_viewport[] = {0x8B, 0x44, 0x24, 0x04, 0x83, 0xEC, 0x48};
    // push -1 / push 0x47280c (exception frame)
    const uint8_t destruct[] = {0x6A, 0xFF, 0x68, 0x0C, 0x28, 0x47, 0x00};
    // sub esp,0x80 / push ebx / push esi (Graphics_CreateSurface)
    const uint8_t create_surface[] = {0x81, 0xEC, 0x80, 0x00, 0x00, 0x00, 0x53, 0x56};
    if (!patch::check(AFTER_D3D_ENDSCENE, after_d3d, sizeof after_d3d) ||
        !patch::check(END_SCENE_EPILOGUE, epilogue, sizeof epilogue) ||
        !patch::check(LEVEL_LOAD, level_load, sizeof level_load) ||
        !patch::check(EFFECT, effect, sizeof effect) ||
        !patch::check(SET_VIEWPORT, set_viewport, sizeof set_viewport) ||
        !patch::check(DESTRUCT, destruct, sizeof destruct) ||
        !patch::check(CREATE_SURFACE, create_surface, sizeof create_surface)) {
        patch::log("skipped screen scaling: unexpected bytes");
        return;
    }
    g_effect = reinterpret_cast<Effect_t>(
        patch::hook(EFFECT, effect, sizeof effect, reinterpret_cast<void*>(&Effect_hook), "Effect"));
    g_destruct = reinterpret_cast<Destruct_t>(
        patch::hook(DESTRUCT, destruct, sizeof destruct, reinterpret_cast<void*>(&Destruct_hook), "Graphics_Destruct"));
    g_level_load = reinterpret_cast<LevelLoad_t>(
        patch::hook(LEVEL_LOAD, level_load, sizeof level_load, reinterpret_cast<void*>(&LevelLoad_hook), "LevelLoad"));
    if (!g_effect || !g_destruct || !g_level_load) return;

    patch::write_call(AFTER_D3D_ENDSCENE, nullptr, 0, reinterpret_cast<void*>(&after_d3d_endscene),
                      sizeof after_d3d, true);

    g_register_class = *reinterpret_cast<RegisterClassA_t*>(IAT_RegisterClassA);
    void* hook_fn = reinterpret_cast<void*>(&RegisterClassA_scaling);
    patch::write(IAT_RegisterClassA, &hook_fn, sizeof hook_fn);
}
