// Rows of choices of the editor's game-style panels (editor_rows.h).
#include "editor_rows.h"

#include <algorithm>
#include <utility>

// editor_edit.cpp
float edit_draw_text(int font, float x, float y, const char* text, int step);
bool edit_font_size(int font, float& w, float& h);
void edit_fill(float l, float t, float r, float b, uint32_t argb);

void PanelRows::begin(POINT mouse) {
    mouse_ = mouse;
    items_.clear();
    tips_.clear();
    holds_.clear();
    drawn_ = GetTickCount();
    edit_font_size(FONT, cw, ch);
}

void PanelRows::tip(float l, float t, float r, float b, const std::string& text) { tips_.push_back({l, t, r, b, text}); }

const char* PanelRows::tip_at(POINT m) const {
    if (GetTickCount() - drawn_ > 250) return nullptr;
    for (const Cell& c : cells_)   // the strip of a held choice covers the rows
        if (held() && m.x >= c.l && m.x < c.r && m.y >= c.t && m.y < c.b) return nullptr;
    for (const Tip& t : tips_)
        if (m.x >= t.l && m.x < t.r && m.y >= t.t && m.y < t.b) return t.text.c_str();
    return nullptr;
}

void PanelRows::hold_area(float l, float t, float r, float b, std::function<void()> click, std::vector<int> choices,
                          std::function<void(int)> pick) {
    holds_.push_back({l, t, r, b, std::move(click), std::move(choices), std::move(pick)});
}

float PanelRows::hold_text(float x, float y, const std::string& text, std::function<void()> click, std::vector<int> choices,
                           std::function<void(int)> pick) {
    const float w = text.size() * cw, t = y - ch / 2;
    const bool over = hover(x - 2, y - ROW / 2, x + w + 2, y + ROW / 2) || (pressed_ && hold_.l == x - 2 && hold_.t == y - ROW / 2);
    edit_draw_text(over ? LIT : FONT, x, t, text.c_str(), STEP);
    hold_area(x - 2, y - ROW / 2, x + w + 2, y + ROW / 2, std::move(click), std::move(choices), std::move(pick));
    return x + w + cw;
}

bool PanelRows::press(POINT m) {
    for (const Hold& h : holds_)
        if (m.x >= h.l && m.x < h.r && m.y >= h.t && m.y < h.b) {
            hold_ = h;
            pressed_ = true;
            since_ = GetTickCount();
            cells_.clear();
            return true;
        }
    return false;
}

bool PanelRows::held() const { return pressed_ && GetTickCount() - since_ >= HOLD_MS; }

bool PanelRows::release(POINT m) {
    if (!pressed_) return false;
    const bool long_press = held();
    pressed_ = false;
    const Hold h = hold_;   // the act may change the rows
    if (!long_press) {
        if (m.x >= h.l && m.x < h.r && m.y >= h.t && m.y < h.b && h.click) h.click();
    } else {
        for (const Cell& c : cells_)
            if (m.x >= c.l && m.x < c.r && m.y >= c.t && m.y < c.b && h.pick) {
                h.pick(c.vid);
                break;
            }
    }
    cells_.clear();
    return true;
}

int PanelRows::draw_hold(float size, float left, float right, const std::function<void(int, float, float, bool)>& icon) {
    if (!held() || hold_.choices.empty()) return 0;
    const int n = static_cast<int>(hold_.choices.size());
    const int per_row = (std::max)(1, (std::min)(n, static_cast<int>((right - left) / size)));
    const int rows = (n + per_row - 1) / per_row;
    float l = hold_.l;
    if (l + per_row * size > right) l = right - per_row * size;
    if (l < left) l = left;
    const float b = hold_.t - 4, t = b - rows * size;
    edit_fill(l - 3, t - 3, l + per_row * size + 3, b + 3, 0xe0202010);
    cells_.clear();
    int under = 0;
    for (int i = 0; i < n; ++i) {
        const float cl = l + (i % per_row) * size, ct = t + (i / per_row) * size;
        const bool lit = hover(cl, ct, cl + size, ct + size);
        if (lit) under = hold_.choices[i];
        icon(hold_.choices[i], cl + size / 2, ct + size / 2, lit);
        cells_.push_back({cl, ct, cl + size, ct + size, hold_.choices[i]});
    }
    return under;
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
