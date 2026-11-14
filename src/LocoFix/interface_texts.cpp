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
// cursor size). While the play bar is up at a bigger scale they are loaded s times the system's size (LoadImage;
// Windows scales the picture and its hot spot together, so a click still lands on the point the cursor shows);
// when the bar is hidden or given up (menus, end screens, the held editor, TAB) they are loaded at the normal
// size again, as the mouse tip is normal there.
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
constexpr uintptr_t MESSAGE_LIST_CLEAR = 0x424980;
constexpr uintptr_t TIP_TEXT = 0x4a9d58;            // MouseTip_Update: the String with the tip's text
constexpr int CURSOR_LOADED = 0x104;              // Cursor: the Windows cursors are loaded (Cursor_Show / _Hide)

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
    // the size changed since the lines on the screen were made (the settings page in the Esc menu): those lines
    // go - moved by the new step they would overlap, and the line a click finds would be one never written
    if (at<int>(list, ML_VID_TEXT) != interface_nvid(MESSAGE_FONT, s))
        reinterpret_cast<void(__fastcall*)(void*, void*)>(MESSAGE_LIST_CLEAR)(list, nullptr);
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

// ---- the lines again at another size (a zoom) ----

constexpr int ML_TEXTS = 0xd8, ML_MESSAGE_X = 0x18c, ML_MESSAGE_Y = 0x240;
constexpr int TEXT_STRING = 0x70;   // Text: the shown text (a String: char*)
constexpr int ACT_SET_TEXT = 0x78;
constexpr int MAX_LINES = 45;       // MessageList's arrays: 0xb4 bytes apart

using CreateObject_t = uint8_t*(__fastcall*)(void* world, void* edx, void* vid, float x, float y, float z,
                                             unsigned direction, void* parent);
using Action_t = int(__fastcall*)(void* object, void* edx, int action, int a, int b, int c);

uint8_t* vid_or_empty(int nvid) {
    uint8_t* v = interface_vid(nvid, 1);
    return v ? v : *reinterpret_cast<uint8_t**>(0x5d3a60);   // g_emptyVid
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
    // Its place laid out again as MouseTip_Update lays it out, with the offsets times the scale: 5 s px right of the
    // cursor and 10 s px above it; below it (10 s px) when the tip would reach above the view's top; held at the
    // view's right edge when "{text}" would reach past it - so the bigger offsets never push a tip off the screen.
    const float cam_x = at<float>(world(), 0x44), cam_y = at<float>(world(), 0x48);
    const float mx = at<float>(mouse, 0x10), my = at<float>(mouse, 0x14);
    uint8_t* font = at<uint8_t*>(text, 0x1c);
    const float size_x = at<float>(font, 0x1c), size_y = at<float>(font, 0x20);
    const float view_right = at<float>(graphics(), 0x224), view_top = at<float>(graphics(), 0x228);
    const char* tip_text = *reinterpret_cast<const char* const*>(TIP_TEXT);
    const float width = static_cast<float>((tip_text ? std::strlen(tip_text) : 0) + 2) * size_x;   // with the braces
    float x = mx + TIP_RIGHT * s;
    float y = (my - size_y) - TIP_UP * s;                     // the screen row the engine keeps as y - z
    if (y + size_y * 0.5f <= view_top) y = size_y * 0.5f + my + TIP_UP * s;
    if (view_right <= width + x) x = view_right - width;
    move_to(text, x + cam_x, y + 3000.0f + cam_y);            // z 3000, as the engine makes it
}

// ---- the cursor ----

int g_cursor_scale = 1;    // the scale the cursors were loaded at
int g_cursor_wanted = 1;   // the play bar's scale while it is up, else 1 (interface_cursor_scale)

HCURSOR WINAPI load_cursor(LPCSTR path) {
    const int s = g_cursor_wanted;
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

// zoom.cpp, after the bar was built again at another scale: the message lines on the screen made again in the
// scale's font, at the scale's places - the text, the click strip and the map point of each line kept, as
// MessageList_ShowLine makes a line (without its sound and its scrolling). Without it the lines would stay
// at the size they were made at until the next message clears them (a quarter of their size after a zoom to
// a half and back).
void interface_messages_rescale() {
    if (!world()) return;
    uint8_t* player = at<uint8_t*>(world(), 0x220);   // World.players[0], the human
    if (!player) return;
    uint8_t* list = player + 0x44;                      // Player.messages
    const int s = interface_scale();
    if (at<int>(list, ML_VID_TEXT) == interface_nvid(MESSAGE_FONT, s)) return;
    const int lines = at<int>(list, ML_LINES);
    if (lines <= 0 || lines > MAX_LINES) return;
    struct Line {
        char* text;
        bool strip;
    } kept[MAX_LINES] = {};
    int count = 0;
    for (int i = 0; i < lines; ++i) {
        uint8_t* text = at<uint8_t*>(list, ML_TEXTS + 4 * i);
        const char* shown = text ? at<const char*>(text, TEXT_STRING) : nullptr;
        if (shown) {
            const size_t n = lstrlenA(shown) + 1;
            kept[i].text = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, n));
            if (kept[i].text) lstrcpynA(kept[i].text, shown, static_cast<int>(n));
            kept[i].strip = at<uint8_t*>(list, ML_CLICK_BARS + 4 * i) != nullptr;
            ++count;
        }
    }
    reinterpret_cast<void(__fastcall*)(void*, void*)>(MESSAGE_LIST_CLEAR)(list, nullptr);
    at<float>(list, ML_TEXT_X) = MESSAGE_X * s;
    at<float>(list, ML_TEXT_Y) = MESSAGE_Y * s;
    at<int>(list, ML_VID_TEXT) = interface_nvid(MESSAGE_FONT, s);
    uint8_t* font = interface_vid(MESSAGE_FONT, s) ? interface_vid(MESSAGE_FONT, s) : vid_or_empty(MESSAGE_FONT);
    uint8_t* strip = interface_vid(MESSAGE_STRIP, s) ? interface_vid(MESSAGE_STRIP, s) : vid_or_empty(MESSAGE_STRIP);
    const int step = at<short>(font, 0x2a8) + 1;   // the line's step: the font's picture height + 1
    const float top = at<float>(graphics(), 0x228);
    const float x = MESSAGE_X * s, y = MESSAGE_Y * s;
    auto create = reinterpret_cast<CreateObject_t>(at<void**>(world(), 0)[0x20 / 4]);
    for (int i = 0; i < lines; ++i) {
        if (!kept[i].text) continue;
        uint8_t* text = create(world(), nullptr, font, x, static_cast<float>(i * step) + y + top + 2000.0f, 2000.0f, 0,
                               nullptr);
        at<uint8_t*>(list, ML_TEXTS + 4 * i) = text;
        if (text) {
            char* string = kept[i].text;   // ACT_SET_TEXT takes a String (a pointer to the char*)
            reinterpret_cast<Action_t>(at<void**>(text, 0)[1])(text, nullptr, ACT_SET_TEXT,
                                                               reinterpret_cast<int>(&string), 0, 0);
        }
        if (kept[i].strip)
            at<uint8_t*>(list, ML_CLICK_BARS + 4 * i) =
                create(world(), nullptr, strip, x - STRIP_LEFT * s,
                       static_cast<float>(i * step + step / 2) + y + top + 2000.0f, 2000.0f, 0, nullptr);
        HeapFree(GetProcessHeap(), 0, kept[i].text);
    }
    if (count) patch::log("interface: %d message line(s) made again at scale %d", count, s);
}

// status_bar.cpp: the cursors at the play bar's scale while the bar is up (`s`), at the normal size when it
// is hidden or given up (the menus, the end screens, the held editor, TAB) - loaded again when that changes.
// While the game has its cursors unloaded (the window not active, SetCursor(-1)) only the size is noted:
// Cursor_Show loads them at it.
void interface_cursor_scale(int s) {
    g_cursor_wanted = s;
    if (!g_installed || s == g_cursor_scale) return;
    void* cursor = *reinterpret_cast<void**>(G_CURSOR);
    if (!cursor || !at<int>(cursor, CURSOR_LOADED)) return;
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
