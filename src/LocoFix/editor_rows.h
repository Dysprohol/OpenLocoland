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

    float cw = 8, ch = 13;   // the font's character

private:
    struct Item {
        float l, t, r, b;
        std::function<void()> act;
    };
    std::vector<Item> items_;
    POINT mouse_{};
};
