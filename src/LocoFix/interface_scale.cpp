// The interface's size: the state bar, its texts and the minimap at two or three times their size
// on a big screen. The world is drawn as before, one map pixel to one screen pixel.
//
// How: bigger pictures, made at build time - every picture of the bar and its fonts enlarged two
// and three times pixel by pixel (tools/patch_data.py add_interface_rows / make_interface_pictures)
// into VIDs of their own: 1100 + i for twice and 1150 + i for three times, i the place of the
// original in INTERFACE_VIDS below (the same table as the build's). Their objects.res rows are the
// originals' with the box (sizeX/Y/Z) as many times bigger, so the engine places them by their
// pictures (the bar's base at the bottom of the view, the minimap in the corner, the money plate at
// the top), hit-tests them by their boxes, spaces a text by its font's box and limits the camera by
// the base's height (Player_ShowStateBar: the map's bottom plus the bar's height less 2) - all by
// itself. What the engine has as fixed numbers - the bar's layout around the screen's middle, the
// slots' pitch of 41 px, the factory list's rows - is ours in C++ with the one scale factor of this
// file: status_bar.cpp (Player_ShowStateBar and the side tiles), depot_rows.cpp (the factory list,
// Player_SetPanel), the slot under the cursor below (Player_HandleInput), minimap.cpp (the minimap
// follows the interface).
//
// The scale is the player's choice on the game's settings page (game/maps/script/optfields.lgc):
// InterfaceScale "1", "2" or "3" in the Players key next to MinimapSize, absent = normal. It is read
// each time the bar is built - a mission's start and the way back from the Esc menu (the bar is given
// up while the menu is open) - and used only as far as it fits the game's screen at that moment:
//   - twice from 960 px of height and 1328 of width, three times from 1440 and 1992 - the bar at most
//     about a tenth of the screen's height, and the minimap at the same scale clear of the bar's
//     controls (they end 228 px right of the middle at the normal size, the frame is 104 px wide);
//   - the screen is the game's logical one: a map smaller than the window (mission 1) is drawn at
//     that size and stretched (screen_scaling.cpp), its bar with it - so mission 1's 1024x768 gets the
//     normal size, already shown 1.4 times (1920x1080) or 1.9 times (2560x1440) by the stretch; where a
//     stretched screen still has room, the scale is the chosen one divided by the stretch, rounded;
//   - an edition whose pictures the build could not enlarge keeps the normal size (each VID's picture
//     must really be that many times its original's).
// The settings page offers only what fits the window (script function 207 InterfaceSizeMax()).
//
// The engine names an interface item by its VID number: MenuNVidUnderCursor (the bar's clicks, the
// scripts' MenuVid(), the mouse tips' [MouseTips] MenuVid<n>) answers the original's number for a
// bigger VID (minimap.cpp's hook); the icon atlas' army colours, set by the scripts on VID 455
// (SetVid(455, VID_GAMMA + army, ...)), are copied to its bigger copies whenever the bar is built.
#include "patch.h"
#include <algorithm>
#include <cstdlib>

void window_size(int& w, int& h);   // screen_scaling.cpp: the game area of the window, real pixels

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr char REG_KEY[] = "SOFTWARE\\Gromada\\Locoland\\Players";   // the scripts' settings (player_registry.cpp)

constexpr int MAX_SCALE = 3;
constexpr int FIRST_VID[MAX_SCALE + 1] = {0, 0, 1100, 1150};
// tools/patch_data.py INTERFACE_VIDS: the same numbers in the same order
constexpr int VIDS[] = {450, 938, 455, 456, 457, 458, 459, 463, 469, 497, 498, 499, 502, 503, 4, 2, 8, 6, 14, 16, 17,
                        18, 449,
                        // the editor's own (its interface size, below): icons, Help and Menu buttons, palette icons,
                        // the panels' device, pictures, lamp and buttons, the titles' and the lit menu font
                        939, 942, 943, 944, 466, 484, 486, 472, 475, 476, 477, 3, 7};
constexpr int COUNT = sizeof VIDS / sizeof VIDS[0];
constexpr int PLAY_COUNT = 23;   // the play interface's are the first ones
// The panels' device (VID 466, 1024 x 768, mostly empty) is enlarged only round its middle: its bigger copies
// are this much of it, centred (patch_data.INTERFACE_CROP), so they are drawn at the same place.
constexpr int CROP_NVID = 466, CROP_W = 584, CROP_H = 532;
constexpr bool box_only(int nvid) { return nvid == 502 || nvid == 503; }   // vid\empty.vid: only the box grows
// What each scale needs of the screen: the bar at most about a tenth of the height, and the minimap at
// the same scale clear of the bar's controls (228 px right of the middle + a 104 px frame, times s).
constexpr int HEIGHT_PER_STEP = 480;
constexpr int WIDTH_PER_STEP = 2 * (228 + 104);
constexpr int VID_GAMMA_SLOT = 7;          // Vid vtable: SetGamma(Gamma*, army)
constexpr int VID_ARMY_GAMMA = 0x398;      // Vid: Gamma[4] armyGamma

template <typename T>
T& at(void* base, int offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}
uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }
uint8_t* graphics() { return *reinterpret_cast<uint8_t**>(G_GRAPHICS); }
uint8_t* vid(int nvid) {
    uint8_t* w = world();
    if (!w || nvid < 0 || nvid >= at<int>(w, 0x290)) return nullptr;
    return reinterpret_cast<uint8_t**>(w + 0x294)[nvid];
}

int g_scale = 1;          // the scale of the bar built last
int g_ready[MAX_SCALE + 1] = {1, 1, -1, -1};   // -1 not looked at yet

// The player's choice: InterfaceScale of the Players key, the text the script's SetReg writes (a DWORD
// is taken too); 1 when it is absent or not 1-3. Read from the registry when the bar is built (`fresh`);
// the settings page's preview asks every frame and gets the value read at most 4 times a second, so a
// click on the row shows within a quarter of a second.
int read_chosen(const char* name = "InterfaceScale");
int chosen(bool fresh = false) {
    static int value = 1;
    static DWORD at = 0;
    const DWORD now = GetTickCount();
    if (fresh || at == 0 || now - at >= 250) {
        value = read_chosen();
        at = now ? now : 1;
    }
    return value;
}

int read_chosen(const char* name) {
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return 1;
    char text[16] = {};
    DWORD size = sizeof text - 1, type = 0;
    const LONG r = RegQueryValueExA(key, name, nullptr, &type, reinterpret_cast<BYTE*>(text), &size);
    RegCloseKey(key);
    int value = 0;
    if (r == ERROR_SUCCESS && type == REG_SZ) value = std::atoi(text);
    if (r == ERROR_SUCCESS && type == REG_DWORD && size == 4) std::memcpy(&value, text, 4);
    return value >= 1 && value <= MAX_SCALE ? value : 1;
}

// Every bigger VID of the scale there and its picture that many times its original's: the play interface's
// (`from` 0, `to` PLAY_COUNT) or the editor's (PLAY_COUNT, COUNT).
bool ready(int s, int* flags = g_ready, int from = 0, int to = PLAY_COUNT) {
    if (s <= 1) return true;
    if (flags[s] >= 0) return flags[s] != 0;
    if (!world()) return false;
    flags[s] = 1;
    for (int i = from; i < to; ++i) {
        uint8_t* original = vid(VIDS[i]);
        uint8_t* big = vid(FIRST_VID[s] + i);
        if (!original) continue;   // the edition has none: the engine uses its empty VID for both
        // a font is s - 1 rows taller than its letters (patch_data.enlarge_font: the line step height + 1 times s)
        const short w = at<short>(original, 0x2a6), h = at<short>(original, 0x2a8);
        const short bw = big ? at<short>(big, 0x2a6) : 0, bh = big ? at<short>(big, 0x2a8) : 0;
        const bool ok = big && (box_only(VIDS[i]) || (VIDS[i] == CROP_NVID ? bw == s * CROP_W && bh == s * CROP_H
                                                       : bw == s * w && (bh == s * h || bh == s * (h + 1) - 1)));
        if (!ok) {
            patch::log("interface: VID %d is not %d times VID %d - no %dx %s interface", FIRST_VID[s] + i, s, VIDS[i], s,
                       from ? "editor" : "play");
            flags[s] = 0;
            break;
        }
    }
    return flags[s] != 0;
}

bool fits(int s, int w, int h) { return s <= 1 || (h >= HEIGHT_PER_STEP * s && w >= WIDTH_PER_STEP * s); }

int biggest(int w, int h) {
    int s = MAX_SCALE;
    while (s > 1 && (!fits(s, w, h) || !ready(s))) --s;
    return s;
}

}  // namespace

// The scale of the bar built last (and being built): status_bar.cpp, depot_rows.cpp, minimap.cpp.
int interface_scale() { return g_scale; }

// Player_ShowStateBar, when it builds the bar: the scale for the game's screen now.
int interface_choose_scale() {
    const float* screen = reinterpret_cast<float*>(graphics() + 0x20c);   // the logical screen
    const int w = static_cast<int>(screen[0]), h = static_cast<int>(screen[1]);
    int window_w = w, window_h = h;
    window_size(window_w, window_h);
    const int want = chosen(true);
    int s = want < biggest(w, h) ? want : biggest(w, h);
    // a logical screen stretched to the window (screen_scaling.cpp keeps its proportions)
    const double stretch = (std::min)(static_cast<double>(window_w) / w, static_cast<double>(window_h) / h);
    if (stretch > 1.01) {
        const int real = static_cast<int>(want / stretch + 0.5);
        if (real < s) s = real < 1 ? 1 : real;
    }
    if (s != g_scale || want > 1)
        patch::log("interface: scale %d (chosen %d) for a %dx%d screen in a %dx%d window", s, want, w, h, window_w,
                   window_h);
    g_scale = s;
    return s;
}

// The biggest scale that fits the window (the settings page offers up to it; script function 207).
int interface_max_for_window() {
    int w = 0, h = 0;
    window_size(w, h);
    if (w <= 0 || h <= 0) return 1;
    return biggest(w, h);
}

// The scale the bar would get on the window's own screen (the settings page's previews).
int interface_scale_for_window() {
    const int max = interface_max_for_window();
    const int want = chosen();
    return want < max ? want : max;
}

// The VID of `nvid` at the scale `s`: its bigger copy, or itself (not one of the interface's, scale 1,
// or the copy missing).
uint8_t* interface_vid(int nvid, int s) {
    if (s > 1)
        for (int i = 0; i < COUNT; ++i)
            if (VIDS[i] == nvid) {
                if (uint8_t* big = vid(FIRST_VID[s] + i)) return big;
                break;
            }
    return vid(nvid);
}

// The number of the bigger copy of `nvid` at the scale s (the original's number at 1, or when it has none).
int interface_nvid(int nvid, int s) {
    if (s > 1)
        for (int i = 0; i < COUNT; ++i)
            if (VIDS[i] == nvid) return vid(FIRST_VID[s] + i) ? FIRST_VID[s] + i : nvid;
    return nvid;
}

// The scale of a bigger copy's number (2 or 3), 1 for any other number.
int interface_scale_of(int nvid) {
    for (int s = 2; s <= MAX_SCALE; ++s)
        if (nvid >= FIRST_VID[s] && nvid < FIRST_VID[s] + COUNT) return s;
    return 1;
}

// The number of the original for a bigger VID's number, else the number itself.
int interface_original_nvid(int nvid) {
    for (int s = 2; s <= MAX_SCALE; ++s)
        if (nvid >= FIRST_VID[s] && nvid < FIRST_VID[s] + COUNT) return VIDS[nvid - FIRST_VID[s]];
    return nvid;
}

// The army colours of the originals (the scripts set them on VID 455) onto the bigger copies of scale s.
void interface_copy_colours(int s) {
    if (s <= 1) return;
    using SetGamma_t = void(__fastcall*)(void* vid, void* edx, void* gamma, unsigned army);
    for (int i = 0; i < COUNT; ++i) {
        uint8_t* original = vid(VIDS[i]);
        uint8_t* big = vid(FIRST_VID[s] + i);
        if (!original || !big || original == big) continue;   // (a slot naming its copy: nothing to copy from)
        for (unsigned army = 0; army < 4; ++army) {
            auto* gamma = &at<uint32_t>(original, VID_ARMY_GAMMA + 8 * army);
            auto* has = &at<uint32_t>(big, VID_ARMY_GAMMA + 8 * army);
            if ((gamma[0] || gamma[1]) && (gamma[0] != has[0] || gamma[1] != has[1]))
                reinterpret_cast<SetGamma_t>(at<void**>(big, 0)[VID_GAMMA_SLOT])(big, nullptr, gamma, army);
        }
    }
}

// The hidden flag of every bigger copy of `nvid` with the original's (the side tiles are hidden and shown
// by VID with the bar).
void interface_set_hidden(int nvid, int hidden) {
    using SetInt_t = void(__fastcall*)(void* object, void* edx, int value);
    for (int s = 2; s <= MAX_SCALE; ++s)
        if (uint8_t* big = interface_vid(nvid, s); big && big != vid(nvid))
            reinterpret_cast<SetInt_t>(0x44e490)(big, nullptr, hidden);   // Vid_SetHidden
}

// ---- the editor's interface size (editor_edit.cpp, editor_panel.cpp) ----
// The editor's own interface - its bar and the rows above it, the status lines, the hints, the music player,
// an object's panel and the Help, Menu, palette, Arrivals and factory panels - has a size of its own:
// EditorInterfaceScale "1"-"3" in the Players key (the settings page's EDITOR INTERFACE SIZE), absent = normal.
// It is used as far as the editor's screen takes it, by the play interface's rule (twice from 960 x 1328, three
// times from 1440 x 1992; a panel - the menu device's 579 x 479 visible part - fits then too), and only when the
// bigger copies of the editor's own pictures are there as well as the bar's. Read at most 4 times a second (the
// editor asks every frame), so a change on the settings page shows when the editor is drawn next.

namespace {
int g_editor_ready[MAX_SCALE + 1] = {1, 1, -1, -1};
bool editor_ready(int s) { return ready(s) && ready(s, g_editor_ready, PLAY_COUNT, COUNT); }
int editor_biggest(int w, int h) {
    int s = MAX_SCALE;
    while (s > 1 && (!fits(s, w, h) || !editor_ready(s))) --s;
    return s;
}
int editor_chosen() {
    static int value = 1;
    static DWORD at = 0;
    const DWORD now = GetTickCount();
    if (at == 0 || now - at >= 250) {
        value = read_chosen("EditorInterfaceScale");
        at = now ? now : 1;
    }
    return value;
}
}  // namespace

// The editor's scale for its screen now (the game's logical screen, stretched to the window for a small map).
int interface_editor_scale() {
    static int last = 1, last_want = 1;
    const float* screen = reinterpret_cast<float*>(graphics() + 0x20c);
    const int w = static_cast<int>(screen[0]), h = static_cast<int>(screen[1]);
    if (w <= 0 || h <= 0) return 1;
    int window_w = w, window_h = h;
    window_size(window_w, window_h);
    const int want = editor_chosen();
    int s = (std::min)(want, editor_biggest(w, h));
    const double stretch = (std::min)(static_cast<double>(window_w) / w, static_cast<double>(window_h) / h);
    if (stretch > 1.01) {
        const int real = static_cast<int>(want / stretch + 0.5);
        if (real < s) s = real < 1 ? 1 : real;
    }
    if (s != last || want != last_want)
        patch::log("interface: editor scale %d (chosen %d) for a %dx%d screen in a %dx%d window", s, want, w, h, window_w,
                   window_h);
    last = s;
    last_want = want;
    return s;
}

// The biggest editor scale that fits the window (the settings page's EDITOR INTERFACE SIZE; script function 208).
int interface_editor_max_for_window() {
    int w = 0, h = 0;
    window_size(w, h);
    if (w <= 0 || h <= 0) return 1;
    return editor_biggest(w, h);
}

namespace {

// ---- the slot under the cursor (Player_HandleInput) ----
// The engine turns a click on a type icon (VID 455) into an entry of the panel by the icon's distance
// from the first, divided by the slots' pitch of 41 px - three times: the factory list's row ((x - icon
// 80's x) / 41 + 1), its queue or the train's cars (+ 100 / not: (x - the 499 button's x) / 41, the
// button 11 px left of the first slot's pitch). The same with the pitch times the scale.
constexpr uintptr_t SLOT_DIVISION = 0x431d6b;     // cmp dword [esi+0x18],0x50 ... to 0x431e29
constexpr uintptr_t SLOT_DONE = 0x431e29;         // mov eax,[esi+0x2c]
constexpr float SLOT_PITCH = 41.0f;

int __cdecl slot_entry(uint8_t* player, uint8_t* hover) {
    const int pitch = static_cast<int>(SLOT_PITCH) * g_scale;
    uint8_t** items = at<uint8_t**>(player, 0x20);
    const int count = at<int>(player, 0x18);
    const int x = static_cast<int>(at<float>(hover, 0x30));
    if (count > 0x50 && at<float>(items[0x50], 0x34) == at<float>(hover, 0x34))
        return (x - static_cast<int>(at<float>(items[0x50], 0x30))) / pitch + 1;
    const int panel = at<int>(player, 0x2c) & ~0x10;
    const int entry = (x - static_cast<int>(at<float>(items[22], 0x30))) / pitch;
    return panel == 2 || panel == 5 ? entry + 100 : entry;
}

// At SLOT_DIVISION: esi = the Player, edi = the hovered icon; the entry goes to [esp+0x10].
__declspec(naked) void slot_division() {
    __asm {
        push edi
        push esi
        call slot_entry
        add esp, 8
        mov [esp + 0x10], eax
        mov eax, SLOT_DONE
        jmp eax
    }
}

}  // namespace

void install_interface_texts();   // interface_texts.cpp

void install_interface_scale() {
    // cmp dword [esi+0x18],0x50 / jle +0x47 / mov edx,[esi+0x20]
    const uint8_t division[] = {0x83, 0x7E, 0x18, 0x50, 0x7E, 0x47, 0x8B, 0x56, 0x20};
    if (!patch::check(SLOT_DIVISION, division, sizeof division)) {
        patch::log("skipped interface scale: unexpected bytes");
        g_ready[2] = g_ready[3] = 0;
        return;
    }
    patch::write_call(SLOT_DIVISION, nullptr, 0, reinterpret_cast<void*>(&slot_division), sizeof division, true);
    install_interface_texts();
    if (chosen() > 1) patch::log("interface: scale %d chosen on the settings page", chosen());
}
