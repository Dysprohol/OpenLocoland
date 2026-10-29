// Rows of choices of the editor's game-style panels (editor_rows.h).
#include "editor_rows.h"

#include <utility>

// editor_edit.cpp
float edit_draw_text(int font, float x, float y, const char* text, int step);
bool edit_font_size(int font, float& w, float& h);
void edit_fill(float l, float t, float r, float b, uint32_t argb);

void PanelRows::begin(POINT mouse) {
    mouse_ = mouse;
    items_.clear();
    edit_font_size(FONT, cw, ch);
}

bool PanelRows::hover(float l, float t, float r, float b) const {
    return mouse_.x >= l && mouse_.x < r && mouse_.y >= t && mouse_.y < b;
}

float PanelRows::put(float x, float y, const std::string& text, bool current, std::function<void()> act) {
    const float w = text.size() * cw, t = y - ch / 2;
    const bool over = act && hover(x - 2, y - ROW / 2, x + w + 2, y + ROW / 2);
    edit_draw_text(current || over ? LIT : FONT, x, t, text.c_str(), STEP);
    if (current && act) edit_fill(x, t + ch, x + w, t + ch + 2, 0xe0f0e080);
    if (act) items_.push_back({x - 2, y - ROW / 2, x + w + 2, y + ROW / 2, std::move(act)});
    return x + w + cw;
}

float PanelRows::number(float x, float y, const std::string& shown, std::function<void(int)> by) {
    x = put(x, y, "-", false, [by] { by(-1); });
    x = put(x, y, shown);
    return put(x, y, "+", false, [by] { by(1); });
}

void PanelRows::area(float l, float t, float r, float b, std::function<void()> act) {
    items_.push_back({l, t, r, b, std::move(act)});
}

bool PanelRows::click(POINT m) {
    for (const Item& i : items_)
        if (m.x >= i.l && m.x < i.r && m.y >= i.t && m.y < i.b) {
            const std::function<void()> act = i.act;   // the act may change the items
            act();
            return true;
        }
    return false;
}
