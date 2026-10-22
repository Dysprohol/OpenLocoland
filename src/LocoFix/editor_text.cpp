// The texts of the editor in the language of the game: [Editor] of strings.ini (editor_text.h).
//
// The lookup goes through the game's own import of GetPrivateProfileStringA (IAT 0x475118), which
// language.cpp points at the chosen pack's copy of strings.ini (lang\<code>\strings.ini), with the
// game folder's strings.ini - the file the game reads its strings from - so the editor speaks the
// language the launcher chose, and a pack without the section (de, hu, ru)
// gets the English texts. A text is read once and kept: the language does not change while the
// game runs.
//
// With LOCOFIX_TEXT_TEST=1 in the environment, install_editor_text logs a few lookups (the
// pack's text of a key, a missing key, a format that matches and one that does not), so a test
// can see which pack answered ("text test" in LocoFix.log).
#include "patch.h"
#include "editor_text.h"

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <unordered_map>

unsigned language_codepage();   // language.cpp

namespace {

constexpr uintptr_t IAT_GetPrivateProfileStringA = 0x475118;
constexpr const char* SECTION = "Editor";
constexpr const char* MISSING = "\x01";

using GetString_t = DWORD(WINAPI*)(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);

std::unordered_map<std::string, std::string> g_texts;   // key -> the text in use
bool g_english = false;                                  // ui_english

std::string ini_path() { return patch::game_folder() + "strings.ini"; }

// The pack's value of the key, "" with found = false when it has none.
std::string read(const char* key, bool& found) {
    char value[4096];
    const auto get = *reinterpret_cast<GetString_t*>(IAT_GetPrivateProfileStringA);
    get(SECTION, key, MISSING, value, sizeof value, ini_path().c_str());
    found = std::strcmp(value, MISSING) != 0;
    if (!found) return "";
    std::string out;
    for (const char* c = value; *c; ++c) {
        if (c[0] == '\\' && (c[1] == 'n' || c[1] == 't')) {
            out += c[1] == 'n' ? '\n' : '\t';
            ++c;
        } else {
            out += *c;
        }
    }
    return out;
}

// The conversions of a printf format, one letter (and its length) each: "%d at %.0f,%.0f%s"
// gives "d f f s"; "%%" is none.
std::string conversions(const std::string& format) {
    std::string out;
    for (size_t i = 0; i < format.size(); ++i) {
        if (format[i] != '%') continue;
        if (++i < format.size() && format[i] == '%') continue;
        while (i < format.size() && std::strchr("-+ #0", format[i])) ++i;            // flags
        while (i < format.size() && (std::isdigit(static_cast<unsigned char>(format[i])) || format[i] == '*')) {
            if (format[i] == '*') out += "* ";                                          // width
            ++i;
        }
        if (i < format.size() && format[i] == '.') {                                    // precision
            ++i;
            while (i < format.size() && (std::isdigit(static_cast<unsigned char>(format[i])) || format[i] == '*')) {
                if (format[i] == '*') out += "* ";
                ++i;
            }
        }
        std::string length;
        while (i < format.size() && std::strchr("hlLzjtI3264", format[i])) length += format[i++];
        out += length + (i < format.size() ? format[i] : '?') + std::string(" ");
    }
    return out;
}

const std::string& lookup(const char* key, const char* english, bool format) {
    const std::string id = std::string(format ? "f:" : "t:") + key;
    auto it = g_texts.find(id);
    if (it != g_texts.end()) return it->second;
    bool found = false;
    std::string value = read(key, found);
    if (!found) {
        patch::log("text: [%s] %s is not in %s - the English text is used", SECTION, key, ini_path().c_str());
        value = english;
    } else if (format && conversions(value) != conversions(english)) {
        patch::log("text: [%s] %s does not have the conversions of \"%s\" - the English text is used", SECTION, key,
                   english);
        value = english;
    }
    return g_texts.emplace(id, value).first->second;
}

}  // namespace

const char* ui_text(const char* key, const char* english) {
    return g_english ? english : lookup(key, english, false).c_str();
}

const char* ui_format(const char* key, const char* english) {
    return g_english ? english : lookup(key, english, true).c_str();
}

bool ui_english(bool on) {
    const bool was = g_english;
    g_english = on;
    return was;
}

std::string ui_printf(const char* format, ...) {
    char text[1024];
    va_list a;
    va_start(a, format);
    std::vsnprintf(text, sizeof text, format, a);
    va_end(a);
    return text;
}

const char* UiText::get() const { return key ? ui_text(key, english) : english; }

unsigned ui_codepage() { return language_codepage(); }

std::wstring ui_wide(const std::string& text) {
    if (text.empty()) return L"";
    const UINT cp = ui_codepage();
    const int n = MultiByteToWideChar(cp, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(cp, 0, text.c_str(), static_cast<int>(text.size()), &out[0], n);
    return out;
}

std::string ui_narrow(const std::wstring& text) {
    if (text.empty()) return "";
    const UINT cp = ui_codepage();
    const int n = WideCharToMultiByte(cp, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(cp, 0, text.c_str(), static_cast<int>(text.size()), &out[0], n, nullptr, nullptr);
    return out;
}

BOOL ui_modify_menu(HMENU menu, UINT position, UINT flags, UINT_PTR id, const char* text) {
    const std::wstring w = text ? ui_wide(text) : L"";
    return ModifyMenuW(menu, position, flags, id, text ? w.c_str() : nullptr);
}

BOOL ui_insert_menu(HMENU menu, UINT position, UINT flags, UINT_PTR id, const char* text) {
    const std::wstring w = text ? ui_wide(text) : L"";
    return InsertMenuW(menu, position, flags, id, text ? w.c_str() : nullptr);
}

BOOL ui_append_menu(HMENU menu, UINT flags, UINT_PTR id, const char* text) {
    const std::wstring w = text ? ui_wide(text) : L"";
    return AppendMenuW(menu, flags, id, text ? w.c_str() : nullptr);
}

int ui_message_box(HWND owner, const char* text, const char* title, UINT type) {
    return MessageBoxW(owner, ui_wide(text).c_str(), ui_wide(title).c_str(), type);
}

std::wstring ui_wtext(const char* key, const char* english) {
    return ui_wide(ui_text(key, english));
}

// After install_language, which redirects the lookups to the chosen pack.
void install_editor_text() {
    char value[8];
    if (!GetEnvironmentVariableA("LOCOFIX_TEXT_TEST", value, sizeof value) || value[0] != '1') return;
    patch::log("text test: strings.ini %s", ini_path().c_str());
    patch::log("text test: MenuFile = \"%s\"", ui_text("MenuFile", "&File"));
    patch::log("text test: MsgDeleted = \"%s\"", ui_format("MsgDeleted", "deleted VID %d at %.0f,%.0f%s"));
    patch::log("text test: a missing key = \"%s\"", ui_text("NoSuchKeyForTheTest", "the English default"));
    patch::log("text test: a format that does not match = \"%s\"", ui_format("MenuSave", "&Save %d maps"));
}

const char* ui_unit_name(int nvid) {
    static std::unordered_map<int, std::string> names;
    auto it = names.find(nvid);
    if (it == names.end()) {
        char value[256];
        const auto get = *reinterpret_cast<GetString_t*>(IAT_GetPrivateProfileStringA);
        get("Units", std::to_string(nvid).c_str(), MISSING, value, sizeof value, ini_path().c_str());
        it = names.emplace(nvid, std::strcmp(value, MISSING) == 0 ? std::string() : std::string(value)).first;
    }
    return it->second.empty() ? nullptr : it->second.c_str();
}