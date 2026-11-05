// Rows of choices in the editor's game-style panels (the Arrivals panel, a factory's panel): texts
// in the menu font (VID 2), a choice lit (VID 7) under the mouse and when it is set (then
// underlined too), and where the last frame drew each choice, for the next click.
#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include <vector>

class PanelRows {
public:
    static constexpr int FONT = 2, LIT = 7, STEP = 13;   // in front of the panel's picture (editor_panel.cpp's steps)
    static constexpr float ROW = 14;

    // A new frame: no choices yet, the mouse where it is.
    void begin(POINT mouse);
    // A text at x (y is the row's middle); with `act` a choice. Returns where the next one goes.
    float put(float x, float y, const std::string& text, bool current = false, std::function<void()> act = nullptr);
    // - n + : `by` gets -1 or 1.
    float number(float x, float y, const std::string& shown, std::function<void(int)> by);
    // A choice of any shape (an icon).
    void area(float l, float t, float r, float b, std::function<void()> act);
    bool hover(float l, float t, float r, float b) const;
    // A click: runs the choice under it; true when there was one.
    bool click(POINT m);

    // What a row does, for the status line while the mouse is over the band l..r, t..b; tip_at gives
    // the one under the mouse, nothing when the rows were not drawn in the last 250 ms (the panel closed).
    void tip(float l, float t, float r, float b, const std::string& text);
    const char* tip_at(POINT m) const;

    // Click-and-hold, the way building menus of the Anno games do it: on a hold area a short click
    // runs `click`; held HOLD_MS, a strip of `choices` (VIDs) opens above it and letting go on one runs
    // `pick` with it (elsewhere: nothing). hold_text draws a text choice as put does.
    static constexpr unsigned HOLD_MS = 350;
    void hold_area(float l, float t, float r, float b, std::function<void()> click, std::vector<int> choices,
                   std::function<void(int)> pick);
    float hold_text(float x, float y, const std::string& text, std::function<void()> click, std::vector<int> choices,
                    std::function<void(int)> pick);
    bool press(POINT m);     // WM_LBUTTONDOWN: true on a hold area (nothing runs yet)
    bool release(POINT m);   // WM_LBUTTONUP after a press: the click or the pick; true when there was a press
    bool held() const;       // pressed long enough: the strip is open
    // While held: the strip of `size` px cells above the pressed area, inside left..right, each choice
    // drawn by `icon(vid, cx, cy, lit)`; returns the VID under the mouse, 0 none.
    int draw_hold(float size, float left, float right, const std::function<void(int, float, float, bool)>& icon);

    float cw = 8, ch = 13;   // the font's character

private:
    struct Item {
        float l, t, r, b;
        std::function<void()> act;
    };
    struct Tip {
        float l, t, r, b;
        std::string text;
    };
    struct Hold {
        float l, t, r, b;
        std::function<void()> click;
        std::vector<int> choices;
        std::function<void(int)> pick;
    };
    struct Cell {
        float l, t, r, b;
        int vid;
    };
    std::vector<Item> items_;
    std::vector<Tip> tips_;
    std::vector<Hold> holds_;
    std::vector<Cell> cells_;
    Hold hold_;
    bool pressed_ = false;
    unsigned since_ = 0, drawn_ = 0;
    POINT mouse_{};
};
