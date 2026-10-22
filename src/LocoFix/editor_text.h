// The texts of the editor in the language of the game (editor_text.cpp).
//
// Every text the editor shows - its menus, dialogs, bar, status line and messages - is looked up
// by a key in the section [Editor] of strings.ini of the chosen language pack (lang/<code>/
// strings.override.ini in the repository), with the English text as the default for the packs
// that have no translation. The keys and the English texts are listed in DEVELOPERS.md ("Editor
// mode", "The texts of the editor").
#pragma once
#include <windows.h>
#include <string>

// The text of `key`, or `english` when the pack has none. "\n" and "\t" in the ini value stand
// for a new line and a tab (strings.ini holds one line a key). The pointer stays valid.
const char* ui_text(const char* key, const char* english);

// The same for a printf format: the translation is used only when it has the conversions of the
// English text (%d, %s, %.1f ...) in the same order, so a wrong translation cannot read the wrong
// arguments; otherwise the English text, and the log says why.
const char* ui_format(const char* key, const char* english);

// ui_text for the W functions (the dialogs): a pack's texts are in its code page (language.ini:
// 1250 Polish and Hungarian, 1251 Russian, 1252 English and German).
std::wstring ui_wtext(const char* key, const char* english);
std::wstring ui_wide(const std::string& text);
std::string ui_narrow(const std::wstring& text);   // back to the pack's code page
unsigned ui_codepage();

// Menus and message boxes with a pack's text, through the W functions, so the text shows right
// whatever the system's code page (a Russian pack on a Polish Windows).
BOOL ui_modify_menu(HMENU menu, UINT position, UINT flags, UINT_PTR id, const char* text);
BOOL ui_insert_menu(HMENU menu, UINT position, UINT flags, UINT_PTR id, const char* text);
BOOL ui_append_menu(HMENU menu, UINT flags, UINT_PTR id, const char* text);
int ui_message_box(HWND owner, const char* text, const char* title, UINT type);

// While on, ui_text and ui_format give the English texts: a message is made twice, for the
// status line in the pack's language and for LocoFix.log in English (tests read the log).
// Returns the state before, to be set back.
bool ui_english(bool on);

// printf into a string (at most 1023 characters).
std::string ui_printf(const char* format, ...);

// A text kept by its key until it is shown (a reason found now and shown later).
struct UiText {
    const char* key = nullptr;   // none: `english` as it is
    const char* english = "";
    const char* get() const;
};

// The game's own name of an object ([Units] of strings.ini, keyed by its VID number), or nullptr
// when the pack has none (decorations: the editor names them, [Editor] Obj<nvid>).
const char* ui_unit_name(int nvid);
