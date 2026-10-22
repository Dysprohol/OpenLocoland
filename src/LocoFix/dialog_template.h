// A dialog built in memory (DLGTEMPLATE and DLGITEMTEMPLATE, Unicode) for the editor's own dialogs
// (editor_mission.cpp, editor_newmap.cpp): LocoFix has no resources.
#pragma once
#include <windows.h>
#include <vector>

namespace dialog {

class DialogTemplate {
public:
    DialogTemplate(const wchar_t* title, short cx, short cy) {
        dword(WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT | DS_CENTER);
        dword(0);
        count_at_ = data_.size();
        word(0);   // items
        word(0);
        word(0);
        word(static_cast<WORD>(cx));
        word(static_cast<WORD>(cy));
        word(0);   // no menu
        word(0);   // the dialog class
        text(title);
        word(8);
        text(L"MS Shell Dlg");
    }
    void item(WORD atom, const wchar_t* text_, int id, DWORD style, short x, short y, short cx, short cy) {
        if (data_.size() % 2) word(0);   // DWORD aligned
        dword(style | WS_CHILD | WS_VISIBLE);
        dword(0);
        word(static_cast<WORD>(x));
        word(static_cast<WORD>(y));
        word(static_cast<WORD>(cx));
        word(static_cast<WORD>(cy));
        word(static_cast<WORD>(id));
        word(0xFFFF);
        word(atom);
        text(text_);
        word(0);
        ++data_[count_at_];
    }
    const DLGTEMPLATE* get() const { return reinterpret_cast<const DLGTEMPLATE*>(data_.data()); }

private:
    void word(WORD w) { data_.push_back(w); }
    void dword(DWORD d) {
        word(LOWORD(d));
        word(HIWORD(d));
    }
    void text(const wchar_t* t) {
        for (; *t; ++t) word(*t);
        word(0);
    }
    std::vector<WORD> data_;
    size_t count_at_ = 0;
};

constexpr WORD BUTTON = 0x80, EDIT = 0x81, STATIC = 0x82, COMBOBOX = 0x85;
constexpr DWORD MULTILINE = ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_BORDER | WS_TABSTOP;
constexpr DWORD LINE = ES_AUTOHSCROLL | WS_BORDER | WS_TABSTOP;

}  // namespace dialog
