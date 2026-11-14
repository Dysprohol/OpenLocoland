// The interface's size for the messages, the objectives, the mouse tip and the cursor (interface_scale.cpp has
// the scale and the bigger VIDs).
//
// Messages and objectives: the human player's MessageList (Player+0x44, MessageList_Construct: the font VID 14,
// the click strip VID 449, x 20, y 5, three lines) shows a message on the bottom line (MessageList_ShowLine), moves
// the lines up a line at a time (MessageList_Scroll) and finds the clicked line by its strip's height on the screen
// (MessageList_Update) - the line's step is the font's picture height + 1 everywhere. The bigger message fonts are
// made one row per step taller (patch_data.enlarge_font: a cell s times the original's height + 1, less 1), so
// "height + 1" is exactly s times the original step and scrolling and the click follow by themselves. Before every
// new line the list gets the scale's numbers - x 20 s, y 5 s, the bigger font's VID - and World.vids[449] names the
// bigger strip while the engine makes it; its place, the font's x less 10, is moved to x less 10 s afterwards. The
// objectives (key O and a mission's start, the scripts' PrintObjectives) are messages too.
//
// Coloured text: a text switches its font in the middle of a line by number (`<Font=16>` and the ESC byte, Text_Draw)
// - the messages' red, yellow and green (VIDs 16-18). While a text drawn in a bigger font is drawn, World.vids of
// the interface's fonts name their bigger copies of the same scale, so the switch keeps the size.
//
// The mouse tip (MouseTip_Update: font VID 6, 5 px right of the cursor and 10 px above it, or below at the top
// edge) is made with the bigger font and those offsets times the scale while the state bar is up in play; the
// menus (a 1024x768 picture stretched to the window) and the held editor keep the normal tip.
//
// The cursor: the game loads Windows cursors from cursores\*.ani (Cursor_Show, LoadCursorFromFileA - the system's
// cursor size). At the interface's scale they are loaded s times the system's size (LoadImage; Windows scales the
// picture and its hot spot together, so a click still lands on the point the cursor shows), and loaded again when
// the bar is built at another scale.
#include "patch.h"

int interface_scale();                         // interface_scale.cpp
uint8_t* interface_vid(int nvid, int scale);
int interface_nvid(int nvid, int scale);
int interface_scale_of(int nvid);
bool menu_background_shown();                  // screen_scaling.cpp

namespace {

constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t G_GRAPHICS = 0x4a9a14;
constexpr uintptr_t G_CURSOR = 0x4a9d44;
constexpr uintptr_t TEXT_DRAW = 0x40c660;
constexpr uintptr_t SHOW_LINE = 0x424cf0;         // MessageList_ShowLine
constexpr uintptr_t MOUSE_TIP_UPDATE = 0x4257f0;
constexpr uintptr_t CURSOR_SHOW = 0x4254e0;
constexpr uintptr_t CURSOR_HIDE = 0x425660;
constexpr uintptr_t CALL_LOAD_CURSOR = 0x4255e2;  // Cursor_Show: call [LoadCursorFromFileA]
constexpr uintptr_t IAT_LOAD_CURSOR = 0x47518c;
constexpr uintptr_t OBJECT_MOVE_TO = 0x446640;

constexpr int FONTS[] = {2, 6, 8, 14, 16, 17, 18};   // the interface's fonts (patch_data.INTERFACE_VIDS)
constexpr int MESSAGE_FONT = 14, MESSAGE_STRIP = 449, TIP_FONT = 6;
constexpr float MESSAGE_X = 20, MESSAGE_Y = 5, STRIP_LEFT = 10;   // MessageList_Construct, MessageList_ShowLine
constexpr float TIP_RIGHT = 5, TIP_UP = 10;                       // MouseTip_Update
// MessageList (re/types.txt)
constexpr int ML_LINES = 0x04, ML_TEXT_X = 0x0c, ML_TEXT_Y = 0x10, ML_VID_TEXT = 0x14, ML_CLICK_BARS = 0x24;

template <typename T>
T& at(void* base, int offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}
uint8_t* world() { return *reinterpret_cast<uint8_t**>(G_WORLD); }
uint8_t* graphics() { return *reinterpret_cast<uint8_t**>(G_GRAPHICS); }
uint8_t*& vid_slot(int nvid) { return reinterpret_cast<uint8_t**>(world() + 0x294)[nvid]; }
int nvid_of(uint8_t* object) { return at<int>(at<uint8_t*>(object, 0x1c), 4); }

using MoveTo_t = void(__fastcall*)(void* object, void* edx, float x, float y, float z);
void move_to(uint8_t* o, float x, float y) {
    reinterpret_cast<MoveTo_t>(OBJECT_MOVE_TO)(o, nullptr, x, y, at<float>(o, 0x38));
}

// World.vids[n] of the given numbers name their bigger copies of scale s until this goes out of scope.
class Swap {
public:
    Swap(const int* numbers, int count, int s) : count_(0) {
        if (s <= 1) return;
        for (int i = 0; i < count; ++i) {
            uint8_t* big = interface_vid(numbers[i], s);
            if (!big || big == vid_slot(numbers[i])) continue;
            numbers_[count_] = numbers[i];
            kept_[count_++] = vid_slot(numbers[i]);
            vid_slot(numbers[i]) = big;
        }
    }
    ~Swap() {
        for (int i = 0; i < count_; ++i) vid_slot(numbers_[i]) = kept_[i];
    }

private:
    int numbers_[8] = {}, count_;
    uint8_t* kept_[8] = {};
};

// ---- coloured text ----

using Text_t = void(__fastcall*)(void* text, void* edx);
Text_t g_text_draw = nullptr;

void __fastcall text_draw(uint8_t* text, void* edx) {
    const int s = at<uint8_t*>(text, 0x1c) ? interface_scale_of(nvid_of(text)) : 1;
    if (s <= 1) return g_text_draw(text, edx);
    Swap swap(FONTS, sizeof FONTS / sizeof FONTS[0], s);
    g_text_draw(text, edx);
}

// ---- messages ----

using ShowLine_t = void(__fastcall*)(void* list, void* edx, void* item, float x, float y);
ShowLine_t g_show_line = nullptr;

void __fastcall show_line(uint8_t* list, void* edx, void* item, float x, float y) {
    const int s = interface_scale();
    at<float>(list, ML_TEXT_X) = MESSAGE_X * s;
    at<float>(list, ML_TEXT_Y) = MESSAGE_Y * s;
    at<int>(list, ML_VID_TEXT) = interface_nvid(MESSAGE_FONT, s);
    const int last = at<int>(list, ML_LINES) - 1;
    uint8_t** strips = reinterpret_cast<uint8_t**>(list + ML_CLICK_BARS);
    uint8_t* before = last >= 0 ? strips[last] : nullptr;
    {
        const int strip[] = {MESSAGE_STRIP};
        Swap swap(strip, 1, s);
        g_show_line(list, edx, item, x, y);
    }
    if (s > 1 && last >= 0 && strips[last] && strips[last] != before)
        move_to(strips[last], at<float>(strips[last], 0x30) - STRIP_LEFT * (s - 1), at<float>(strips[last], 0x34));
}

// ---- the mouse tip ----

using MouseTip_t = void(__fastcall*)(void* tip, void* edx, void* mouse);
MouseTip_t g_mouse_tip = nullptr;

bool bar_up() {
    uint8_t* player = at<uint8_t*>(world(), 0x220);   // World.players[0], the human
    return player && at<int>(player, 0x18) > 0 && !menu_background_shown();
}

void __fastcall mouse_tip(uint8_t* tip, void* edx, uint8_t* mouse) {
    const int s = world() && bar_up() ? interface_scale() : 1;
    if (s <= 1) return g_mouse_tip(tip, edx, mouse);
    uint8_t* before = at<uint8_t*>(tip, 0x04);
    {
        const int font[] = {TIP_FONT};
        Swap swap(font, 1, s);
        g_mouse_tip(tip, edx, mouse);
    }
    uint8_t* text = at<uint8_t*>(tip, 0x04);
    if (!text || text == before) return;
    // the engine put it 5 px right of the cursor (unless held at the right edge) and 10 px above it (or, at the
    // top edge, below it): those offsets times the scale
    const float cam_x = at<float>(world(), 0x44), cam_y = at<float>(world(), 0x48);
    const float mx = at<float>(mouse, 0x10), my = at<float>(mouse, 0x14);
    float x = at<float>(text, 0x30) - cam_x, y = at<float>(text, 0x34) - cam_y;   // y as created: screen y + z
    const float size_y = at<float>(at<uint8_t*>(text, 0x1c), 0x20);
    if (x == mx + TIP_RIGHT) x = mx + TIP_RIGHT * s;
    if (y == ((my - size_y) + 3000.0f) - TIP_UP) y = ((my - size_y) + 3000.0f) - TIP_UP * s;
    else y += TIP_UP * (s - 1);
    move_to(text, x + cam_x, y + cam_y);
}

// ---- the cursor ----

int g_cursor_scale = 1;   // the scale the cursors were loaded at

HCURSOR WINAPI load_cursor(LPCSTR path) {
    const int s = interface_scale();
    g_cursor_scale = s;
    if (s > 1) {
        const int w = GetSystemMetrics(SM_CXCURSOR) * s, h = GetSystemMetrics(SM_CYCURSOR) * s;
        if (HANDLE big = LoadImageA(nullptr, path, IMAGE_CURSOR, w, h, LR_LOADFROMFILE))
            return static_cast<HCURSOR>(big);
        // (cursores\small-nomove.ani is named by the game and shipped by no edition: the system's arrow then)
        if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
            patch::log("interface: the cursor %s could not be loaded %d times bigger", path, s);
    }
    return LoadCursorFromFileA(path);
}

bool g_installed = false;

}  // namespace

// status_bar.cpp, after the bar is built: the cursors again when the scale changed.
void interface_cursor_follow() {
    if (!g_installed || interface_scale() == g_cursor_scale) return;
    void* cursor = *reinterpret_cast<void**>(G_CURSOR);
    if (!cursor) return;
    using Cursor_t = void(__fastcall*)(void* cursor, void* edx);
    reinterpret_cast<Cursor_t>(CURSOR_HIDE)(cursor, nullptr);
    reinterpret_cast<Cursor_t>(CURSOR_SHOW)(cursor, nullptr);
    if (HCURSOR c = GetCursor()) {
        ICONINFO info = {};
        if (GetIconInfo(c, &info)) {
            BITMAP bm = {};
            if (info.hbmColor) GetObject(info.hbmColor, sizeof bm, &bm);
            else if (info.hbmMask) GetObject(info.hbmMask, sizeof bm, &bm);
            patch::log("interface: cursors at scale %d (the shown one %dx%d, hot spot %d,%d)", g_cursor_scale,
                       bm.bmWidth, info.hbmColor ? bm.bmHeight : bm.bmHeight / 2, info.xHotspot, info.yHotspot);
            if (info.hbmColor) DeleteObject(info.hbmColor);
            if (info.hbmMask) DeleteObject(info.hbmMask);
        }
    }
}

void install_interface_texts() {
    // mov eax,fs:[0] / push -1 / push 0x4725f8 (Text_Draw)
    const uint8_t text_start[] = {0x64, 0xA1, 0x00, 0x00, 0x00, 0x00, 0x6A, 0xFF, 0x68, 0xF8, 0x25, 0x47, 0x00};
    // push ebx / push ebp / push esi / mov esi,ecx / push edi (MessageList_ShowLine)
    const uint8_t line_start[] = {0x53, 0x55, 0x56, 0x8B, 0xF1, 0x57};
    // push -1 / push 0x473818 (MouseTip_Update)
    const uint8_t tip_start[] = {0x6A, 0xFF, 0x68, 0x18, 0x38, 0x47, 0x00};
    g_text_draw = reinterpret_cast<Text_t>(patch::hook(TEXT_DRAW, text_start, sizeof text_start,
                                                       reinterpret_cast<void*>(&text_draw), "Text_Draw"));
    g_show_line = reinterpret_cast<ShowLine_t>(patch::hook(SHOW_LINE, line_start, sizeof line_start,
                                                           reinterpret_cast<void*>(&show_line), "MessageList_ShowLine"));
    g_mouse_tip = reinterpret_cast<MouseTip_t>(patch::hook(MOUSE_TIP_UPDATE, tip_start, sizeof tip_start,
                                                           reinterpret_cast<void*>(&mouse_tip), "MouseTip_Update"));
    g_installed = patch::redirect_iat_call(CALL_LOAD_CURSOR, IAT_LOAD_CURSOR, reinterpret_cast<void*>(&load_cursor),
                                           "Cursor_Show: LoadCursorFromFileA");
}
