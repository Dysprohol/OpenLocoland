// "Check for updates" in the launcher: a row under the options' with the button, a box to check at
// the start (on by default, at most once a day), a link to the project's releases on GitHub and a
// line with the result.
//
// The check asks GitHub's API for the latest release - WinHTTP in a thread of its own, the answer
// awaited 5 s, so the launcher never waits - and reads its tag, its page and the Setup.exe it holds.
// The tag is compared with LOCOFIX_VERSION by its numbers (v0.1.10 is newer than v0.1.9; a build
// after a tag, v0.1.10-17-gcef9557, counts as that tag; "dev" has no numbers and is never offered
// a download). A newer version shows with Download: Setup.exe goes into the Downloads folder (as
// a browser's download, with the internet zone mark; its size and SHA-256 are checked against the
// release's), and then Run Setup starts it after a question and closes the game, so Setup can
// update the install. Nothing is downloaded without a click: the check runs from the button or, with
// the box ticked (the default: no value in the registry is on; the player's choice is saved as soon as
// the box is clicked), as the launcher opens - at most once a day (not when UpdateLastCheck is today's)
// and without a window: only the line, which says calmly when there was no connection; the link opens
// the page in the browser, its right button copies the address.
//
// When GitHub cannot be reached (no network, the repository gone) the line shows the notice of
// update.txt in the game folder - a line per language pack code, en= for the others - or the
// English one built in here; a later Setup or the player can replace the file, and its link= line
// is the address the link opens then. The last check's date and the version it found are kept in
// the registry next to the other settings (UpdateLastCheck), the box as UpdateCheckAtStart (DWORD).
// LOCOFIX_UPDATE_URL replaces the API's address (tests: an unreachable host). LOCOFIX_UPDATE_TEST=
// <file.json> makes the check read that file instead of asking GitHub (an answer of the API saved, or
// one written by hand with a higher tag_name), and its Setup.exe may then be a file:/// address,
// copied instead of downloaded with the same checks (size, SHA-256, the zone mark, the Downloads
// folder): the whole flow - Download, Run Setup - against a Setup built by tools\release.py
// (DEVELOPERS.md "Updates": the steps).
//
// A release may also carry Setup as one folder packed into a zip (an asset ending with Setup.zip:
// Setup.exe at the zip's root and its _internal\ folder - tools\release.py --zip). Which of the two a
// release with both gives is PREFER_SETUP_ZIP (the maintainer's choice; off: the exe, as up to
// v0.2.10). The zip is downloaded as the exe is (size, SHA-256, the zone mark, Downloads) and then
// unpacked (zip_read.cpp) into a new empty folder of its own, %LOCALAPPDATA%\OpenLocoland\setup\<tag>
// (<tag>-2 ... when that is taken): every entry a plain relative path inside it - no drive, no root, no
// "..", nothing else that leaves it -, at most ZIP_MAX_FILES files and ZIP_MAX_BYTES in all, and
// Setup.exe at its root, which alone gets the zone mark and is the one started. Any failure removes
// that folder and falls back to the release's Setup.exe when it has one, else to the line that the
// download failed, with the GitHub link. Either Setup is started with --target <this game's folder>
// (quoted by the command line's rules), so it opens on this install. Releases up to v0.2.10 carry only
// the exe, which every launcher up to v0.2.10 finds as the first asset ending with Setup.exe: that
// name pattern is unchanged. LOCOFIX_UPDATE_PREFER=zip|exe (only with LOCOFIX_UPDATE_TEST) overrides
// the choice in a test.
#include "patch.h"
#include "zip_read.h"
#include <bcrypt.h>
#include <shellapi.h>
#include <shlobj.h>
#include <winhttp.h>
#include <cstdio>
#include <cwchar>
#include <exception>
#include <vector>

#ifndef WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3
#define WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3 0x00002000
#endif

void add_launcher_label(int id, const char* key);              // language.cpp
void add_launcher_relabel(void (*relabel)(HWND dialog));       // language.cpp
std::wstring launcher_text(const char* key, const wchar_t* fallback);
std::string language_code();

namespace {

constexpr uintptr_t IAT_DialogBoxParamA = 0x4751ac;
constexpr char REG_KEY[] = "SOFTWARE\\Gromada\\Locoland";   // RegPath in Locoland.cfg
constexpr char REG_AT_START[] = "UpdateCheckAtStart";
constexpr char REG_LAST[] = "UpdateLastCheck";               // "<date> <tag>"
constexpr wchar_t API_URL[] = L"https://api.github.com/repos/Dysprohol/OpenLocoland/releases/latest";
constexpr wchar_t RELEASES_URL[] = L"https://github.com/Dysprohol/OpenLocoland/releases";
constexpr wchar_t DEFAULT_NOTICE[] = L"Cannot check for updates. Project page: github.com/Dysprohol/OpenLocoland";
constexpr DWORD TIMEOUT_MS = 5000;
// A release with both a Setup.exe and a Setup.zip asset: true takes the zip (one folder, nothing
// unpacked into %TEMP% when it runs), false the exe (what every launcher up to v0.2.10 takes).
constexpr bool PREFER_SETUP_ZIP = false;
constexpr unsigned long long ZIP_MAX_DOWNLOAD = 256ull << 20;   // the zip itself
constexpr unsigned long long ZIP_MAX_BYTES = 512ull << 20;      // all its files unpacked
constexpr size_t ZIP_MAX_FILES = 10000;
constexpr int ID_RESOLUTION = 1001;
constexpr int ID_CHECK = 1130;
constexpr int ID_AT_START = 1131;
constexpr int ID_LINK = 1132;
constexpr int ID_STATUS = 1133;
constexpr int ID_GET = 1134;
constexpr int ID_MENU_OPEN = 1;
constexpr int ID_MENU_COPY = 2;
constexpr int ROW_Y = 222;        // dialog units: under the options' row (options.cpp, 208)
constexpr int ROW_HEIGHT = 44;    // dialog units added to the launcher: the buttons and three lines
constexpr UINT WM_UPDATE_DONE = WM_APP + 40;       // a check or a download ended: lParam its Result
constexpr UINT WM_UPDATE_PROGRESS = WM_APP + 41;   // bytes downloaded (wParam) of (lParam)
constexpr UINT_PTR TIMER_CHECK = 0x5C3C;   // the check's 5 s: WinHTTP's own timeouts let a connection
                                           // to a host that does not answer take 8 s

struct Release {
    std::string tag, page;                   // tag_name, the release's html_url
    std::string setup_url, setup_name;       // the Setup.exe asset's browser_download_url and name
    std::string digest;                      // its "sha256:<hex>", when GitHub gives one
    unsigned long long size = 0;             // its size in bytes
    std::string zip_url, zip_name, zip_digest;   // the same of the Setup.zip asset, when there is one
    unsigned long long zip_size = 0;
    bool use_zip = false;                    // the download takes the zip (PREFER_SETUP_ZIP)
};

enum class State { idle, checking, current, newer, dev, unreachable, downloading, downloaded, failed };

struct Result {   // what a thread hands to the dialog
    bool download = false;
    bool ok = false;
    Release release;
    std::wstring path;    // the downloaded file (the unpacked Setup.exe of a zip)
    std::wstring shown;   // the name the line shows: the file saved in Downloads
    std::wstring folder;  // a zip's: the folder it was unpacked into
};

State g_state = State::idle;
Release g_release;
std::wstring g_file;                     // the downloaded Setup.exe (a zip's, unpacked)
std::wstring g_shown;                    // what was saved in Downloads (the line shows its name)
std::wstring g_folder;                   // a zip's folder: Setup starts there, with --target
unsigned long long g_done = 0, g_total = 0;
volatile LONG g_cancel = 0;              // the dialog is gone or the download was cancelled
bool g_at_start = true;                  // UpdateCheckAtStart: on unless the player turned it off
bool g_auto = false;                     // the check under way or last done began as the launcher opened
std::string g_last;                      // UpdateLastCheck
DLGPROC g_launcher_proc = nullptr;
HFONT g_font = nullptr, g_link_font = nullptr;   // the row's font, underlined for the link

using DialogBoxParamA_t = INT_PTR(WINAPI*)(HINSTANCE, LPCSTR, HWND, DLGPROC, LPARAM);
DialogBoxParamA_t g_dialog_box = nullptr;   // restart.cpp's hook

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

// `text` with %1 and %2 replaced by `a` and `b`.
std::wstring fill(const std::wstring& text, const std::wstring& a, const std::wstring& b = {}) {
    std::wstring out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'%' && i + 1 < text.size() && (text[i + 1] == L'1' || text[i + 1] == L'2')) {
            out += text[++i] == L'1' ? a : b;
            continue;
        }
        out += text[i];
    }
    return out;
}

// ---- settings ----

void read_settings() {
    HKEY key;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return;
    DWORD value = 0, size = sizeof value, type = 0;
    // no value (a new install, or a player who never clicked the box) is on
    if (RegQueryValueExA(key, REG_AT_START, nullptr, &type, reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS &&
        type == REG_DWORD)
        g_at_start = value != 0;
    char last[64] = {};
    size = sizeof last - 1;
    if (RegQueryValueExA(key, REG_LAST, nullptr, &type, reinterpret_cast<BYTE*>(last), &size) == ERROR_SUCCESS &&
        type == REG_SZ)
        g_last = last;
    RegCloseKey(key);
}

void save_setting(const char* name, DWORD type, const void* data, DWORD size) {
    HKEY key;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, REG_KEY, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr) !=
        ERROR_SUCCESS)
        return;
    RegSetValueExA(key, name, 0, type, static_cast<const BYTE*>(data), size);
    RegCloseKey(key);
}

// update.txt of the game folder: `key=text` lines (UTF-8, ';' comments) - the notice per language
// pack code and link=. The value of `key`, or an empty string.
std::wstring update_txt(const std::string& key) {
    FILE* f = std::fopen((patch::game_folder() + "update.txt").c_str(), "rb");
    if (!f) return {};
    std::wstring value;
    char buffer[1024];
    while (std::fgets(buffer, sizeof buffer, f)) {
        std::string line = buffer;
        if (line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r' || line.back() == ' ')) line.pop_back();
        const size_t eq = line.find('=');
        if (line.empty() || line[0] == ';' || eq == std::string::npos) continue;
        std::string name = line.substr(0, eq);
        while (!name.empty() && name.back() == ' ') name.pop_back();
        if (_stricmp(name.c_str(), key.c_str()) != 0) continue;
        const size_t start = line.find_first_not_of(' ', eq + 1);
        value = start == std::string::npos ? std::wstring() : widen(line.substr(start));
        break;
    }
    std::fclose(f);
    return value;
}

std::wstring unreachable_notice() {
    std::wstring notice = update_txt(language_code());
    if (notice.empty()) notice = update_txt("en");
    return notice.empty() ? DEFAULT_NOTICE : notice;
}

// ---- versions ----

// The numbers of a version or a tag: "v0.1.10-17-gcef9557" -> 0, 1, 10; none for "dev".
std::vector<int> version_numbers(const std::string& v) {
    std::vector<int> numbers;
    size_t i = !v.empty() && (v[0] == 'v' || v[0] == 'V') ? 1 : 0;
    while (i < v.size() && v[i] >= '0' && v[i] <= '9') {
        int n = 0;
        while (i < v.size() && v[i] >= '0' && v[i] <= '9') n = n * 10 + (v[i++] - '0');
        numbers.push_back(n);
        if (i + 1 < v.size() && v[i] == '.' && v[i + 1] >= '0' && v[i + 1] <= '9') ++i;
        else break;
    }
    return numbers;
}

// Like strcmp, by the numbers (a missing one counts as 0: 0.1 == 0.1.0).
int compare_versions(const std::vector<int>& a, const std::vector<int>& b) {
    for (size_t i = 0; i < a.size() || i < b.size(); ++i) {
        const int x = i < a.size() ? a[i] : 0, y = i < b.size() ? b[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

// LOCOFIX_UPDATE_TEST's file, or an empty string.
std::wstring update_test_file() {
    wchar_t value[1024] = {};
    return GetEnvironmentVariableW(L"LOCOFIX_UPDATE_TEST", value, 1024) ? value : L"";
}

// A file:/// address as a path: C:/a%20b/Setup.exe -> C:\a b\Setup.exe.
std::wstring local_path(const std::string& url) {
    std::string p;
    for (size_t i = 8; i < url.size(); ++i) {
        if (url[i] == '%' && i + 2 < url.size()) {
            p += static_cast<char>(std::strtol(url.substr(i + 1, 2).c_str(), nullptr, 16));
            i += 2;
        } else {
            p += url[i] == '/' ? '\\' : url[i];
        }
    }
    return widen(p);
}

bool local_url(const std::string& url) { return url.compare(0, 8, "file:///") == 0 && !update_test_file().empty(); }

// ---- the release (GitHub's JSON, read as text: enough for its flat fields) ----

// The position of the value of "key" at or after `from`, or npos.
size_t json_value(const std::string& json, const char* key, size_t from) {
    const std::string quoted = std::string("\"") + key + "\"";
    for (size_t at = json.find(quoted, from); at != std::string::npos; at = json.find(quoted, at + 1)) {
        size_t i = json.find_first_not_of(" \t\r\n", at + quoted.size());
        if (i == std::string::npos || json[i] != ':') continue;
        i = json.find_first_not_of(" \t\r\n", i + 1);
        if (i != std::string::npos) return i;
    }
    return std::string::npos;
}

std::string json_string(const std::string& json, size_t at) {
    std::string s;
    if (at == std::string::npos || json[at] != '"') return s;
    for (size_t i = at + 1; i < json.size() && json[i] != '"'; ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) ++i;   // \" \\ \/ (the fields read have no others)
        s += json[i];
    }
    return s;
}

bool ends_with(const std::string& s, const char* end) {
    const size_t n = std::strlen(end);
    return s.size() >= n && _stricmp(s.c_str() + s.size() - n, end) == 0;
}

// A file name made only of plain characters (the asset's name becomes a file in Downloads).
bool plain_name(const std::string& name) {
    if (name.empty() || name[0] == '.') return false;
    for (char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '-' ||
              c == '_'))
            return false;
    return true;
}

bool parse_release(const std::string& json, Release& r) {
    r.tag = json_string(json, json_value(json, "tag_name", 0));
    if (r.tag.empty()) return false;
    // html_url is also the author's and every uploader's: the release's is the one with its tag
    for (size_t at = json_value(json, "html_url", 0); at != std::string::npos;
         at = json_value(json, "html_url", at)) {
        const std::string url = json_string(json, at);
        if (url.find("/releases/tag/") != std::string::npos) {
            r.page = url;
            break;
        }
    }
    // an asset's fields come before its browser_download_url: the size and the digest of the
    // Setup.exe asset are the last ones between the asset before it and its own address
    size_t block = json.find("\"assets\"");
    for (size_t at = block; block != std::string::npos; block = at) {
        at = json_value(json, "browser_download_url", at);
        if (at == std::string::npos) break;
        const std::string url = json_string(json, at);
        if (url.compare(0, 8, "https://") != 0 && !local_url(url)) continue;
        const bool exe = ends_with(url, "Setup.exe") && r.setup_url.empty();
        const bool zip = ends_with(url, "Setup.zip") && r.zip_url.empty();
        if (!exe && !zip) continue;
        std::string name = url.substr(url.rfind('/') + 1), digest;
        if (!plain_name(name)) name = exe ? "OpenLocoland-Setup.exe" : "OpenLocoland-Setup.zip";
        unsigned long long size = 0;
        for (size_t i = json_value(json, "size", block); i != std::string::npos && i < at;
             i = json_value(json, "size", i))
            size = std::strtoull(json.c_str() + i, nullptr, 10);
        for (size_t i = json_value(json, "digest", block); i != std::string::npos && i < at;
             i = json_value(json, "digest", i))
            digest = json_string(json, i);
        if (exe) {
            r.setup_url = url;
            r.setup_name = name;
            r.size = size;
            r.digest = digest;
        } else {
            r.zip_url = url;
            r.zip_name = name;
            r.zip_size = size;
            r.zip_digest = digest;
        }
        if (!r.setup_url.empty() && !r.zip_url.empty()) break;
    }
    // the zip when it is preferred (or the only Setup) and not beyond the bound
    wchar_t prefer[16] = {};
    const bool test_prefer = !update_test_file().empty() && GetEnvironmentVariableW(L"LOCOFIX_UPDATE_PREFER", prefer, 16);
    const bool zip_first = test_prefer ? _wcsicmp(prefer, L"zip") == 0 : PREFER_SETUP_ZIP;
    r.use_zip = !r.zip_url.empty() && r.zip_size <= ZIP_MAX_DOWNLOAD && (zip_first || r.setup_url.empty());
    return true;
}

// ---- HTTP ----

struct Http {
    HINTERNET session = nullptr, connection = nullptr, request = nullptr;
    ~Http() {
        if (request) WinHttpCloseHandle(request);
        if (connection) WinHttpCloseHandle(connection);
        if (session) WinHttpCloseHandle(session);
    }
};

// Sends a GET of `url` and reads the answer's status; false (logged) when there is no answer.
bool http_get(Http& h, const std::wstring& url, const wchar_t* headers, DWORD& status) {
    wchar_t host[256] = {}, path[2048] = {}, extra[2048] = {};
    URL_COMPONENTS parts = {};
    parts.dwStructSize = sizeof parts;
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = path;
    parts.dwUrlPathLength = 2048;
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = 2048;
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts)) {
        patch::log("update check: bad address %ls", url.c_str());
        return false;
    }
    const std::wstring agent = L"OpenLocoland/" + widen(LOCOFIX_VERSION);
    h.session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                            WINHTTP_NO_PROXY_BYPASS, 0);
    if (!h.session)   // before Windows 8.1
        h.session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS, 0);
    bool sent = false;
    if (h.session) {
        WinHttpSetTimeouts(h.session, TIMEOUT_MS, TIMEOUT_MS, TIMEOUT_MS, TIMEOUT_MS);
        DWORD protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2 | WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_3;
        if (!WinHttpSetOption(h.session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols)) {
            protocols = WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;   // a system without TLS 1.3
            WinHttpSetOption(h.session, WINHTTP_OPTION_SECURE_PROTOCOLS, &protocols, sizeof protocols);
        }
        h.connection = WinHttpConnect(h.session, host, parts.nPort, 0);
    }
    if (h.connection) {
        const std::wstring object = std::wstring(path) + extra;
        h.request = WinHttpOpenRequest(h.connection, L"GET", object.c_str(), nullptr, WINHTTP_NO_REFERER,
                                       WINHTTP_DEFAULT_ACCEPT_TYPES,
                                       parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    }
    if (h.request)
        sent = WinHttpSendRequest(h.request, headers ? headers : WINHTTP_NO_ADDITIONAL_HEADERS,
                                  headers ? static_cast<DWORD>(-1L) : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
               WinHttpReceiveResponse(h.request, nullptr);
    DWORD size = sizeof status;
    if (!sent || !WinHttpQueryHeaders(h.request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                      WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX)) {
        patch::log("update check: no answer from %ls (error %lu)", host, GetLastError());
        return false;
    }
    return true;
}

// Reads the answer in pieces; `piece` returns false to stop. False when reading failed or stopped.
template <typename Piece>
bool http_read(Http& h, Piece piece) {
    std::vector<char> buffer(64 * 1024);
    for (;;) {
        DWORD read = 0;
        if (!WinHttpReadData(h.request, buffer.data(), static_cast<DWORD>(buffer.size()), &read)) {
            patch::log("update check: reading stopped (error %lu)", GetLastError());
            return false;
        }
        if (read == 0) return true;
        if (!piece(buffer.data(), read)) return false;
    }
}

// ---- the threads ----

struct Job {
    HWND dialog;
    Release release;   // a download's
    std::wstring path;
};

void finish(HWND dialog, Result* result) {
    if (!PostMessageW(dialog, WM_UPDATE_DONE, 0, reinterpret_cast<LPARAM>(result))) delete result;   // closed
}

DWORD WINAPI check_thread(void* parameter) {
    Job* job = static_cast<Job*>(parameter);
    Result* result = new Result;
    wchar_t custom[1024] = {};
    const std::wstring url = GetEnvironmentVariableW(L"LOCOFIX_UPDATE_URL", custom, 1024) ? custom : API_URL;
    Http h;
    DWORD status = 0;
    std::string json;
    const std::wstring test = update_test_file();
    if (!test.empty()) {   // the test's release (LOCOFIX_UPDATE_TEST)
        if (FILE* f = _wfopen(test.c_str(), L"rb")) {
            char buffer[4096];
            for (size_t n; (n = std::fread(buffer, 1, sizeof buffer, f)) > 0;) json.append(buffer, n);
            std::fclose(f);
            status = 200;
            patch::log("update check: the test's release %ls (LOCOFIX_UPDATE_TEST), %u bytes", test.c_str(),
                       static_cast<unsigned>(json.size()));
        } else {
            patch::log("update check: LOCOFIX_UPDATE_TEST %ls cannot be read", test.c_str());
        }
    } else if (http_get(h, url, L"Accept: application/vnd.github+json\r\n", status)) {
        if (status == 200)
            http_read(h, [&](const char* data, DWORD n) {
                json.append(data, n);
                return json.size() < 4 * 1024 * 1024;
            });
        else
            patch::log("update check: HTTP %lu", status);
    }
    result->ok = !json.empty() && parse_release(json, result->release);
    if (status == 200 && !result->ok) patch::log("update check: no release in the answer");
    finish(job->dialog, result);
    delete job;
    return 0;
}

// The SHA-256 of what was hashed, in lowercase hex.
struct Sha256 {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    Sha256() {
        if (BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
            BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0);
    }
    ~Sha256() {
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    }
    void add(const char* data, DWORD n) {
        if (hash) BCryptHashData(hash, reinterpret_cast<UCHAR*>(const_cast<char*>(data)), n, 0);
    }
    std::string hex() {
        UCHAR digest[32];
        if (!hash || !BCRYPT_SUCCESS(BCryptFinishHash(hash, digest, sizeof digest, 0))) return {};
        std::string s;
        char two[3];
        for (UCHAR b : digest) {
            std::snprintf(two, sizeof two, "%02x", b);
            s += two;
        }
        return s;
    }
};

// ---- a Setup packed as a zip ----

// An entry's name as a relative path inside the folder (backslashes), or empty when it is not one:
// a drive or a root, "..", ".", an empty part, a character Windows does not take in a name, a
// name Windows reserves (CON, NUL...), a part ending with a dot or a space.
std::wstring zip_entry_path(const std::string& name) {
    if (name.empty() || name.size() > 240 || name[0] == '/' || name[0] == '\\') return {};
    std::wstring path, part;
    const std::wstring wide = widen(name) + L"/";
    for (wchar_t c : wide) {
        if (c == L'/' || c == L'\\') {
            if (part.empty() || part == L"." || part == L".." || part.back() == L'.' || part.back() == L' ') return {};
            std::wstring stem = part.substr(0, part.find(L'.'));
            for (wchar_t& x : stem) x = static_cast<wchar_t>(towupper(x));
            static const wchar_t* const reserved[] = {L"CON", L"PRN", L"AUX", L"NUL", L"COM1", L"COM2", L"COM3",
                                                       L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
                                                       L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6",
                                                       L"LPT7", L"LPT8", L"LPT9"};
            for (const wchar_t* r : reserved)
                if (stem == r) return {};
            path += (path.empty() ? L"" : L"\\") + part;
            part.clear();
        } else if (c < 32 || std::wcschr(L"<>:\"|?*", c)) {
            return {};
        } else {
            part += c;
        }
    }
    return path;
}

// Removes `folder` and everything in it (the folder this updater made for a zip; no links followed).
void remove_folder(const std::wstring& folder) {
    WIN32_FIND_DATAW found;
    HANDLE find = FindFirstFileW((folder + L"\\*").c_str(), &found);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring name = found.cFileName;
            if (name == L"." || name == L"..") continue;
            const std::wstring path = folder + L"\\" + name;
            if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                remove_folder(path);
            else if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                RemoveDirectoryW(path.c_str());
            else {
                SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(path.c_str());
            }
        } while (FindNextFileW(find, &found));
        FindClose(find);
    }
    RemoveDirectoryW(folder.c_str());
}

// A new empty folder for the release's zip: %LOCALAPPDATA%\OpenLocoland\setup\<tag> (then <tag>-2 ...
// -99), or empty.
std::wstring new_setup_folder(const std::string& tag) {
    PWSTR local = nullptr;
    std::wstring base;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local))) base = local;
    CoTaskMemFree(local);
    if (base.empty()) return {};
    base += L"\\OpenLocoland";
    CreateDirectoryW(base.c_str(), nullptr);
    base += L"\\setup";
    CreateDirectoryW(base.c_str(), nullptr);
    const std::wstring name = widen(plain_name(tag) ? tag : "release");
    for (int n = 1; n < 100; ++n) {
        const std::wstring folder = base + L"\\" + name + (n == 1 ? L"" : L"-" + std::to_wstring(n));
        if (CreateDirectoryW(folder.c_str(), nullptr)) return folder;   // fails when it is there already
        if (GetLastError() != ERROR_ALREADY_EXISTS) return {};
    }
    return {};
}

bool write_whole(const std::wstring& path, const std::string& data) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    size_t done = 0;
    bool ok = true;
    while (ok && done < data.size()) {
        DWORD written = 0;
        const DWORD n = static_cast<DWORD>(min(data.size() - done, static_cast<size_t>(1 << 20)));
        ok = WriteFile(file, data.data() + done, n, &written, nullptr) && written == n;
        done += written;
    }
    CloseHandle(file);
    return ok;
}

// The zip `zip` (downloaded, checked) unpacked into a new folder of its own; true with `folder` and
// `setup` (its Setup.exe, marked with `zone`), else false (logged) with nothing left behind.
bool unpack_setup_zip(const std::wstring& zip, const std::string& tag, const std::string& zone, std::wstring& folder,
                      std::wstring& setup) {
    std::string z;
    {
        HANDLE file = CreateFileW(zip.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        LARGE_INTEGER size = {};
        if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &size) ||
            static_cast<unsigned long long>(size.QuadPart) > ZIP_MAX_DOWNLOAD) {
            if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
            patch::log("update zip: %ls cannot be read or is too big", zip.c_str());
            return false;
        }
        z.resize(static_cast<size_t>(size.QuadPart));
        DWORD read = 0;
        size_t done = 0;
        while (done < z.size() && ReadFile(file, &z[done], static_cast<DWORD>(min(z.size() - done, static_cast<size_t>(1 << 20))), &read, nullptr) && read)
            done += read;
        CloseHandle(file);
        if (done != z.size()) {
            patch::log("update zip: %ls cannot be read", zip.c_str());
            return false;
        }
    }
    // the central directory first: the count, the sizes declared and the names, before anything is unpacked
    size_t end = std::string::npos;
    for (size_t i = z.size() >= 22 ? z.size() - 22 : 0; i + 22 <= z.size() && i + 0x10000 + 22 >= z.size(); --i) {
        if (zipfile::u32(z, i) == 0x06054b50) {
            end = i;
            break;
        }
        if (!i) break;
    }
    if (end == std::string::npos) {
        patch::log("update zip: not a zip file");
        return false;
    }
    const uint32_t count = zipfile::u16(z, end + 10);
    if (count == 0 || count > ZIP_MAX_FILES) {
        patch::log("update zip: %u entries (at most %u)", count, static_cast<unsigned>(ZIP_MAX_FILES));
        return false;
    }
    unsigned long long declared = 0;
    for (size_t at = zipfile::u32(z, end + 16), k = 0; k < count; ++k) {
        if (at > z.size() || z.size() - at < 46 || zipfile::u32(z, at) != 0x02014b50 ||
            z.size() - at - 46 < zipfile::u16(z, at + 28)) {
            patch::log("update zip: a damaged zip file");
            return false;
        }
        const uint32_t size = zipfile::u32(z, at + 24), name_len = zipfile::u16(z, at + 28);
        const std::string name = z.substr(at + 46, name_len);
        const bool is_folder = !name.empty() && (name.back() == '/' || name.back() == '\\');
        if (zip_entry_path(is_folder ? name.substr(0, name.size() - 1) : name).empty()) {
            patch::log("update zip: an entry that is not a plain path inside the folder: %s", name.c_str());
            return false;
        }
        declared += size;
        at += 46 + name_len + zipfile::u16(z, at + 30) + zipfile::u16(z, at + 32);
    }
    if (declared > ZIP_MAX_BYTES) {
        patch::log("update zip: %llu bytes unpacked (at most %llu)", declared, ZIP_MAX_BYTES);
        return false;
    }
    std::vector<zipfile::Entry> entries;
    std::string error;
    if (!zipfile::read_zip(z, entries, error)) {
        patch::log("update zip: %s", error.c_str());
        return false;
    }
    z.clear();
    z.shrink_to_fit();
    bool has_setup = false;
    for (const zipfile::Entry& e : entries) has_setup |= _wcsicmp(zip_entry_path(e.name).c_str(), L"Setup.exe") == 0;
    if (!has_setup) {
        patch::log("update zip: no Setup.exe at its root");
        return false;
    }
    folder = new_setup_folder(tag);
    if (folder.empty()) {
        patch::log("update zip: no folder of its own can be made in %%LOCALAPPDATA%%\\OpenLocoland\\setup");
        return false;
    }
    for (const zipfile::Entry& e : entries) {
        const std::wstring path = folder + L"\\" + zip_entry_path(e.name);
        for (size_t slash = path.find(L'\\', folder.size() + 1); slash != std::wstring::npos; slash = path.find(L'\\', slash + 1))
            CreateDirectoryW(path.substr(0, slash).c_str(), nullptr);
        if (!write_whole(path, e.data)) {
            patch::log("update zip: %ls cannot be written (error %lu)", path.c_str(), GetLastError());
            remove_folder(folder);
            return false;
        }
    }
    setup = folder + L"\\Setup.exe";
    // the zone mark of the download on the program that is started, as Explorer's unpacking gives it
    HANDLE mark = CreateFileW((setup + L":Zone.Identifier").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (mark != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(mark, zone.data(), static_cast<DWORD>(zone.size()), &written, nullptr);
        CloseHandle(mark);
    }
    patch::log("update zip: %u files (%llu bytes) unpacked into %ls", static_cast<unsigned>(entries.size()), declared,
               folder.c_str());
    // the Setups unpacked before (setup\<an older tag>): only this one is needed now
    const std::wstring base = folder.substr(0, folder.rfind(L'\\'));
    WIN32_FIND_DATAW found;
    HANDLE find = FindFirstFileW((base + L"\\*").c_str(), &found);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            const std::wstring name = found.cFileName, path = base + L"\\" + name;
            if (name == L"." || name == L".." || _wcsicmp(path.c_str(), folder.c_str()) == 0 ||
                !(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || (found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
                continue;
            remove_folder(path);
            patch::log("update zip: the older %ls removed", path.c_str());
        } while (FindNextFileW(find, &found));
        FindClose(find);
    }
    return true;
}

// `a` as one argument of a command line (CommandLineToArgvW's and the C runtime's rules): in quotes,
// a backslash run before a quote or the end doubled, a quote escaped - "C:\" stays C:\.
std::wstring quoted_argument(const std::wstring& a) {
    std::wstring out = L"\"";
    size_t backslashes = 0;
    for (wchar_t c : a) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        out.append(c == L'"' ? backslashes * 2 + 1 : backslashes, L'\\');
        backslashes = 0;
        out += c;
    }
    out.append(backslashes * 2, L'\\');
    return out + L"\"";
}

// The asset a download of `release` takes: the zip (use_zip) or the exe, as a release of its own.
Release asset(const Release& release, bool zip) {
    Release r = release;
    if (zip) {
        r.setup_url = release.zip_url;
        r.setup_name = release.zip_name;
        r.size = release.zip_size;
        r.digest = release.zip_digest;
    }
    r.use_zip = zip;
    return r;
}

bool download_file(Job* job, const Release& release, const std::wstring& path);

DWORD WINAPI download_thread(void* parameter) {
    Job* job = static_cast<Job*>(parameter);
    Result* result = new Result;
    result->download = true;
    result->release = job->release;
    result->path = result->shown = job->path;
    bool ok = false;
    if (job->release.use_zip) {
        std::wstring unpacked, setup;
        try {
            const Release zip = asset(job->release, true);
            const std::wstring folder = job->path.substr(0, job->path.rfind(L'\\') + 1);
            const std::wstring file = folder + widen(zip.setup_name);
            ok = download_file(job, zip, file);
            if (ok) {
                const std::string zone = "[ZoneTransfer]\r\nZoneId=3\r\nHostUrl=" + zip.setup_url + "\r\n";
                ok = !g_cancel && unpack_setup_zip(file, zip.tag, zone, unpacked, setup);
                if (ok) {
                    result->path = setup;
                    result->shown = file;
                    result->folder = unpacked;
                }
            }
        } catch (const std::exception& e) {
            patch::log("update zip: stopped (%s)", e.what());
            ok = false;
        } catch (...) {
            patch::log("update zip: stopped");
            ok = false;
        }
        if (!ok && !unpacked.empty()) remove_folder(unpacked);
        if (!ok && !g_cancel && !job->release.setup_url.empty()) {
            patch::log("update: the zip did not work - the release's Setup.exe instead");
            result->release.use_zip = false;
            ok = download_file(job, asset(job->release, false), job->path);
        }
    } else {
        ok = download_file(job, job->release, job->path);
    }
    result->ok = ok;
    finish(job->dialog, result);
    delete job;
    return 0;
}

// Downloads (or, in a test, copies) the asset `release` names into `path`; false (logged) when it
// fails, nothing left behind.
bool download_file(Job* job, const Release& release, const std::wstring& path) {
    const std::wstring part = path + L".part";
    Http h;
    DWORD status = 0;
    // the test's Setup (LOCOFIX_UPDATE_TEST, a file:/// address): read from the disk, the rest the same
    const bool local = local_url(release.setup_url);
    HANDLE source = INVALID_HANDLE_VALUE;
    bool ok;
    if (local) {
        const std::wstring from = local_path(release.setup_url);
        source = CreateFileW(from.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ok = source != INVALID_HANDLE_VALUE;
        patch::log("update download: the test's Setup %ls%s", from.c_str(), ok ? "" : " cannot be read");
    } else {
        ok = http_get(h, widen(release.setup_url), nullptr, status) && status == 200;
        if (!ok && status) patch::log("update download: HTTP %lu", status);
    }
    HANDLE file = ok ? CreateFileW(part.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                                   nullptr)
                     : INVALID_HANDLE_VALUE;
    if (ok && file == INVALID_HANDLE_VALUE) {
        patch::log("update download: cannot write %ls (error %lu)", part.c_str(), GetLastError());
        ok = false;
    }
    Sha256 sha;
    unsigned long long done = 0, posted = 0;
    auto piece = [&](const char* data, DWORD n) {
        DWORD written = 0;
        if (g_cancel || !WriteFile(file, data, n, &written, nullptr) || written != n) return false;
        sha.add(data, n);
        done += n;
        if (done - posted >= 256 * 1024) {
            posted = done;
            PostMessageW(job->dialog, WM_UPDATE_PROGRESS, static_cast<WPARAM>(done),
                         static_cast<LPARAM>(release.size));
        }
        return true;
    };
    if (ok && local) {
        std::vector<char> buffer(64 * 1024);
        for (DWORD read = 0; ok;) {
            ok = ReadFile(source, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr) != 0;
            if (!ok || read == 0) break;
            ok = piece(buffer.data(), read);
        }
    } else if (ok) {
        ok = http_read(h, piece);
    }
    if (source != INVALID_HANDLE_VALUE) CloseHandle(source);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    if (ok && release.size && done != release.size) {
        patch::log("update download: %llu bytes, the release has %llu", done, release.size);
        ok = false;
    }
    const std::string& digest = release.digest;
    if (ok && digest.compare(0, 7, "sha256:") == 0 && _stricmp(sha.hex().c_str(), digest.c_str() + 7) != 0) {
        patch::log("update download: SHA-256 %s, the release has %s", sha.hex().c_str(), digest.c_str() + 7);
        ok = false;
    }
    if (ok) {
        // marked as downloaded from the internet, as a browser marks it, so Windows checks it at the start
        const std::string zone = "[ZoneTransfer]\r\nZoneId=3\r\nHostUrl=" + release.setup_url + "\r\n";
        HANDLE mark = CreateFileW((part + L":Zone.Identifier").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
        if (mark != INVALID_HANDLE_VALUE) {
            DWORD written = 0;
            WriteFile(mark, zone.data(), static_cast<DWORD>(zone.size()), &written, nullptr);
            CloseHandle(mark);
        }
        ok = MoveFileExW(part.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
        if (!ok) patch::log("update download: cannot write %ls (error %lu)", path.c_str(), GetLastError());
    }
    if (!ok) {
        DeleteFileW(part.c_str());
        if (g_cancel) patch::log("update download: cancelled");
    } else
        patch::log("update download: %ls (%llu bytes, SHA-256 checked: %s)", path.c_str(), done,
                    digest.empty() ? "no digest" : "yes");
    return ok;
}

bool start_thread(LPTHREAD_START_ROUTINE routine, Job* job) {
    HANDLE thread = CreateThread(nullptr, 0, routine, job, 0, nullptr);
    if (!thread) {
        delete job;
        return false;
    }
    CloseHandle(thread);
    return true;
}

// ---- the launcher ----

bool web_address(const std::wstring& url) {
    return _wcsnicmp(url.c_str(), L"https://", 8) == 0 || _wcsnicmp(url.c_str(), L"http://", 7) == 0;
}

// What the link opens: the new release's page, update.txt's link= or the releases (only a web
// address: ShellExecute would start a program named there).
std::wstring link_url() {
    const std::wstring page = widen(g_release.page), link = update_txt("link");
    return web_address(page) ? page : web_address(link) ? link : RELEASES_URL;
}

std::wstring mb(unsigned long long bytes) {
    return std::to_wstring((bytes + 512 * 1024) / (1024 * 1024));
}

RECT dialog_rect(HWND dialog, int x, int y, int w, int h) {
    RECT r = {x, y, x + w, y + h};
    MapDialogRect(dialog, &r);
    return r;
}

int text_width(HWND control) {
    wchar_t text[256] = {};
    const int n = GetWindowTextW(control, text, 256);
    const HDC dc = GetDC(control);
    const HGDIOBJ old = SelectObject(dc, reinterpret_cast<HGDIOBJ>(SendMessageW(control, WM_GETFONT, 0, 0)));
    SIZE size = {};
    GetTextExtentPoint32W(dc, text, n, &size);
    SelectObject(dc, old);
    ReleaseDC(control, dc);
    return size.cx;
}

void place(HWND dialog, HWND control, int x, int width) {
    RECT r;
    GetWindowRect(control, &r);
    MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&r), 2);
    SetWindowPos(control, nullptr, x, r.top, width, r.bottom - r.top, SWP_NOZORDER);
}

// The check button as wide as its label (at least 86 dialog units) and the box after it.
void fit_row(HWND dialog) {
    HWND check = GetDlgItem(dialog, ID_CHECK), box = GetDlgItem(dialog, ID_AT_START);
    const RECT row = dialog_rect(dialog, 7, 0, 86, 0), gap = dialog_rect(dialog, 0, 0, 4, 0);
    const RECT tick = dialog_rect(dialog, 0, 0, 14, 0), link = dialog_rect(dialog, 153, 0, 27, 0);
    const int width = max(static_cast<int>(row.right - row.left), text_width(check) + 2 * gap.right);
    place(dialog, check, row.left, width);
    const int x = row.left + width + gap.right;
    place(dialog, box, x, min(tick.right + text_width(box), static_cast<int>(link.left) - x));
}

// The result line and the button beside it for the state, in the chosen language (also
// language.cpp's relabel callback, which has set the labels).
void set_texts(HWND dialog) {
    if (!GetDlgItem(dialog, ID_STATUS)) return;
    fit_row(dialog);
    const std::wstring version = widen(LOCOFIX_VERSION), tag = widen(g_release.tag);
    std::wstring status, button;
    switch (g_state) {
    case State::idle:
        if (g_last.size() > 11)
            status = fill(launcher_text("update_last", L"Last check %1: the latest version was %2."),
                          widen(g_last.substr(0, 10)), widen(g_last.substr(11)));
        break;
    case State::checking:
        status = launcher_text("update_checking", L"Checking for updates...");
        break;
    case State::current:
        status = fill(launcher_text("update_current", L"You have the latest version (%1)."), version);
        break;
    case State::newer:
        status = fill(launcher_text("update_new", L"New version %1 is available (you have %2)."), tag, version);
        if (!g_release.setup_url.empty() || g_release.use_zip) button = launcher_text("update_download", L"Download");
        break;
    case State::dev:
        status = fill(launcher_text("update_dev", L"The latest version is %1 (this build: %2)."), tag, version);
        break;
    case State::unreachable:
        // the check at the start without a connection is no error: a calm line, not update.txt's notice
        status = g_auto ? launcher_text("update_offline_start",
                                        L"No connection - the check at start was skipped; the box beside turns it off.")
                        : unreachable_notice();
        break;
    case State::downloading:
        status = fill(launcher_text("update_downloading", L"Downloading Setup: %1 of %2 MB..."), mb(g_done),
                      mb(g_total));
        button = launcher_text("cancel", L"Cancel");
        break;
    case State::downloaded: {
        const size_t slash = g_shown.rfind(L'\\');
        status = fill(launcher_text("update_downloaded", L"Saved in Downloads: %1"), g_shown.substr(slash + 1));
        button = launcher_text("update_run", L"Run Setup");
        break;
    }
    case State::failed:
        status = launcher_text("update_failed", L"The download failed. The GitHub link opens the release page.");
        button = launcher_text("update_download", L"Download");
        break;
    }
    HWND line = GetDlgItem(dialog, ID_STATUS), get = GetDlgItem(dialog, ID_GET);
    SetWindowTextW(get, button.c_str());
    ShowWindow(get, button.empty() ? SW_HIDE : SW_SHOW);
    // the line is as wide as the row, but beside the button when there is one
    RECT r;
    GetWindowRect(line, &r);
    MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&r), 2);
    RECT end;
    GetWindowRect(button.empty() ? GetDlgItem(dialog, ID_LINK) : get, &end);
    MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&end), 2);
    const RECT gap = {0, 0, 4, 0};
    RECT g = gap;
    MapDialogRect(dialog, &g);
    const int right = button.empty() ? end.right : end.left - g.right;
    SetWindowPos(line, nullptr, 0, 0, right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);
    SetWindowTextW(line, status.c_str());
    EnableWindow(GetDlgItem(dialog, ID_CHECK), g_state != State::checking && g_state != State::downloading);
}

void start_check(HWND dialog, bool automatic = false) {
    g_auto = automatic;
    patch::log("update check: %s", automatic ? "as the launcher opens (at start)" : "the button");
    g_state = State::checking;
    set_texts(dialog);
    if (!start_thread(check_thread, new Job{dialog, {}, {}})) {
        g_state = State::unreachable;
        set_texts(dialog);
        return;
    }
    SetTimer(dialog, TIMER_CHECK, TIMEOUT_MS, nullptr);
}

void checked(const Result& result) {
    if (!result.ok) {
        g_state = State::unreachable;
        return;
    }
    g_release = result.release;
    const std::vector<int> mine = version_numbers(LOCOFIX_VERSION), latest = version_numbers(g_release.tag);
    g_state = mine.empty() ? State::dev : compare_versions(latest, mine) > 0 ? State::newer : State::current;
    patch::log("update check: the latest release is %s (%s%s%s), this build %s%s", g_release.tag.c_str(),
               g_release.setup_name.empty() ? "no Setup.exe" : g_release.setup_name.c_str(),
               g_release.zip_url.empty() ? "" : ", ", g_release.zip_url.empty() ? "" : g_release.zip_name.c_str(), LOCOFIX_VERSION,
               g_state == State::newer ? " - newer" : g_state == State::current ? " - up to date" : "");
    SYSTEMTIME now;
    GetLocalTime(&now);
    char last[64];
    std::snprintf(last, sizeof last, "%04u-%02u-%02u %s", now.wYear, now.wMonth, now.wDay, g_release.tag.c_str());
    g_last = last;
    save_setting(REG_LAST, REG_SZ, last, static_cast<DWORD>(std::strlen(last) + 1));
}

std::wstring downloads_folder() {
    PWSTR path = nullptr;
    std::wstring folder;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_Downloads, 0, nullptr, &path))) folder = path;
    CoTaskMemFree(path);
    return folder;
}

void start_download(HWND dialog) {
    const std::wstring folder = downloads_folder();
    if (folder.empty()) {
        g_state = State::failed;
        return;
    }
    g_file = g_shown = folder + L"\\" + widen(g_release.setup_name.empty() ? "OpenLocoland-Setup.exe" : g_release.setup_name);
    g_folder.clear();
    g_done = 0;
    g_total = g_release.use_zip ? g_release.zip_size : g_release.size;
    patch::log("update download: %s", g_release.use_zip ? "the Setup.zip (PREFER_SETUP_ZIP or the only Setup)" : "the Setup.exe");
    g_cancel = 0;
    g_state = State::downloading;
    if (!start_thread(download_thread, new Job{dialog, g_release, g_file})) g_state = State::failed;
}

void shell_open(HWND dialog, const std::wstring& file) {
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    const INT_PTR opened = reinterpret_cast<INT_PTR>(ShellExecuteW(dialog, L"open", file.c_str(), nullptr, nullptr, SW_SHOWNORMAL));
    if (SUCCEEDED(com)) CoUninitialize();
    patch::log("update: the link %ls %s", file.c_str(), opened > 32 ? "opened" : "did not open");
}

void run_setup(HWND dialog) {
    const std::wstring question = launcher_text(
        "update_run_question",
        L"Setup starts now and the game closes, so Setup can update it. Your maps, scores and progress stay. Go on?");
    if (MessageBoxW(dialog, question.c_str(), L"OpenLocoland", MB_YESNO | MB_ICONQUESTION) != IDYES) return;
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    // Setup is told which install to update (--target, the game's folder: up to v0.2.10 it was started
    // without arguments and opened on the default folder); a zip's Setup starts in its own folder
    std::wstring arguments;
    {
        wchar_t exe[MAX_PATH] = {}, test[MAX_PATH] = {};
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::wstring game = exe;
        game = game.substr(0, game.rfind(L'\\'));
        // a test's game folder (LOCOFIX_UPDATE_TARGET, only with LOCOFIX_UPDATE_TEST): a drive root, a
        // trailing backslash, spaces - how it reaches Setup
        if (!update_test_file().empty() && GetEnvironmentVariableW(L"LOCOFIX_UPDATE_TARGET", test, MAX_PATH)) game = test;
        while (game.size() > 3 && game.back() == L'\\') game.pop_back();
        if (game.size() == 2 && game[1] == L':') game += L'\\';   // C: alone is the current folder on C
        arguments = L"--target " + quoted_argument(game);
    }
    const INT_PTR started = reinterpret_cast<INT_PTR>(
        ShellExecuteW(dialog, L"open", g_file.c_str(), arguments.empty() ? nullptr : arguments.c_str(),
                      g_folder.empty() ? nullptr : g_folder.c_str(), SW_SHOWNORMAL));
    if (SUCCEEDED(com)) CoUninitialize();
    patch::log("update: Setup %ls%s%ls %s", g_file.c_str(), arguments.empty() ? "" : " ", arguments.c_str(),
               started > 32 ? "started, the game closes" : "did not start");
    if (started > 32) PostMessageW(dialog, WM_COMMAND, IDCANCEL, 0);
}

void copy_text(HWND dialog, const std::wstring& text) {
    if (!OpenClipboard(dialog)) return;
    EmptyClipboard();
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    if (HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, bytes)) {
        std::memcpy(GlobalLock(memory), text.c_str(), bytes);
        GlobalUnlock(memory);
        if (!SetClipboardData(CF_UNICODETEXT, memory)) GlobalFree(memory);
    }
    CloseClipboard();
}

void link_menu(HWND dialog, LPARAM l) {
    POINT p = {static_cast<short>(LOWORD(l)), static_cast<short>(HIWORD(l))};
    if (p.x == -1 && p.y == -1) {   // the keyboard's menu key: below the link
        RECT r;
        GetWindowRect(GetDlgItem(dialog, ID_LINK), &r);
        p = {r.left, r.bottom};
    }
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_MENU_OPEN, launcher_text("open_link", L"Open in the browser").c_str());
    AppendMenuW(menu, MF_STRING, ID_MENU_COPY, launcher_text("copy_link", L"Copy the address").c_str());
    const int chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON, p.x, p.y, 0, dialog, nullptr);
    DestroyMenu(menu);
    if (chosen == ID_MENU_OPEN) shell_open(dialog, link_url());
    if (chosen == ID_MENU_COPY) copy_text(dialog, link_url());
}

HWND add_control(HWND dialog, const wchar_t* type, const wchar_t* text, DWORD style, RECT r, int id) {
    HWND control = CreateWindowExW(0, type, text, WS_CHILD | WS_VISIBLE | style, r.left, r.top, r.right - r.left,
                                   r.bottom - r.top, dialog, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                   reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(dialog, GWLP_HINSTANCE)), nullptr);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), FALSE);
    return control;
}

void move_down(HWND dialog, int id, int dy) {
    HWND control = GetDlgItem(dialog, id);
    RECT r;
    GetWindowRect(control, &r);
    MapWindowPoints(nullptr, dialog, reinterpret_cast<POINT*>(&r), 2);
    SetWindowPos(control, nullptr, r.left, r.top + dy, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
}

// The row under the options' (the check, the box, the link; the result line and its button under
// them); OK, Cancel and the bottom of the window move down to make room.
void add_update_row(HWND dialog) {
    if (!GetDlgItem(dialog, ID_RESOLUTION)) return;
    const RECT row = dialog_rect(dialog, 0, 0, 0, ROW_HEIGHT);
    move_down(dialog, IDOK, row.bottom);
    move_down(dialog, IDCANCEL, row.bottom);
    RECT w;
    GetWindowRect(dialog, &w);
    SetWindowPos(dialog, nullptr, 0, 0, w.right - w.left, w.bottom - w.top + row.bottom, SWP_NOMOVE | SWP_NOZORDER);

    // The dialog's MS Sans Serif in its TrueType form: the bitmap font has the letters of code page
    // 1252 only, and a button measures its label with it but draws the others with a wider
    // fallback - a Russian label came out cut off. The TrueType font has them all, in the same sizes.
    LOGFONTW font = {};
    GetObjectW(reinterpret_cast<HGDIOBJ>(SendMessageW(dialog, WM_GETFONT, 0, 0)), sizeof font, &font);
    lstrcpyW(font.lfFaceName, L"Microsoft Sans Serif");
    g_font = CreateFontIndirectW(&font);
    font.lfUnderline = TRUE;
    g_link_font = CreateFontIndirectW(&font);

    add_control(dialog, L"BUTTON", L"Check for updates", WS_TABSTOP | BS_PUSHBUTTON,
                dialog_rect(dialog, 7, ROW_Y, 86, 13), ID_CHECK);
    HWND box = add_control(dialog, L"BUTTON", L"at start", WS_TABSTOP | BS_AUTOCHECKBOX,
                           dialog_rect(dialog, 97, ROW_Y + 2, 55, 10), ID_AT_START);
    SendMessageW(box, BM_SETCHECK, g_at_start ? BST_CHECKED : BST_UNCHECKED, 0);
    HWND link = add_control(dialog, L"STATIC", L"GitHub", SS_NOTIFY | SS_RIGHT | SS_NOPREFIX,
                            dialog_rect(dialog, 153, ROW_Y + 3, 27, 9), ID_LINK);
    SendMessageW(link, WM_SETFONT, reinterpret_cast<WPARAM>(g_link_font), FALSE);
    add_control(dialog, L"STATIC", L"", SS_LEFT | SS_NOPREFIX, dialog_rect(dialog, 7, ROW_Y + 17, 173, 26),
                ID_STATUS);
    add_control(dialog, L"BUTTON", L"", WS_TABSTOP | BS_PUSHBUTTON, dialog_rect(dialog, 124, ROW_Y + 16, 56, 13),
                ID_GET);
    set_texts(dialog);
    if (!g_at_start) {
        patch::log("update check at start: off (the player's choice)");
        return;
    }
    // once a day at most: the date of the last answer is today's (the button always checks)
    SYSTEMTIME now;
    GetLocalTime(&now);
    char today[16];
    std::snprintf(today, sizeof today, "%04u-%02u-%02u", now.wYear, now.wMonth, now.wDay);
    if (g_last.compare(0, 10, today) == 0) {
        patch::log("update check at start: skipped - already checked today (%s)", g_last.c_str());
        return;
    }
    start_check(dialog, true);
}

INT_PTR CALLBACK launcher_proc(HWND dialog, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_COMMAND:
        if (LOWORD(w) == ID_CHECK && HIWORD(w) == BN_CLICKED) {
            start_check(dialog);
            return TRUE;
        }
        if (LOWORD(w) == ID_GET && HIWORD(w) == BN_CLICKED) {
            if (g_state == State::downloading) g_cancel = 1;
            else if (g_state == State::downloaded) run_setup(dialog);
            else start_download(dialog);
            set_texts(dialog);
            return TRUE;
        }
        if (LOWORD(w) == ID_AT_START && HIWORD(w) == BN_CLICKED) {   // saved at once (not only with OK)
            g_at_start = IsDlgButtonChecked(dialog, ID_AT_START) == BST_CHECKED;
            const DWORD on = g_at_start;
            save_setting(REG_AT_START, REG_DWORD, &on, sizeof on);
            patch::log("update check at start: %s (saved)", g_at_start ? "on" : "off");
            return TRUE;
        }
        if (LOWORD(w) == ID_LINK && HIWORD(w) == STN_CLICKED) {
            shell_open(dialog, link_url());
            return TRUE;
        }
        if (LOWORD(w) == IDOK && GetDlgItem(dialog, ID_AT_START)) {
            const DWORD on = IsDlgButtonChecked(dialog, ID_AT_START) == BST_CHECKED;
            save_setting(REG_AT_START, REG_DWORD, &on, sizeof on);
        }
        break;
    case WM_UPDATE_DONE: {
        Result* result = reinterpret_cast<Result*>(l);
        if (!result->download) {
            KillTimer(dialog, TIMER_CHECK);
            // an answer after the 5 s still counts, unless it is no answer
            if (g_state == State::checking || (g_state == State::unreachable && result->ok)) checked(*result);
        }
        else if (result->ok) {
            g_state = State::downloaded;
            g_file = result->path;
            g_shown = result->shown;
            g_folder = result->folder;
        }
        else g_state = g_cancel ? State::newer : State::failed;
        delete result;
        set_texts(dialog);
        return TRUE;
    }
    case WM_TIMER:
        if (w != TIMER_CHECK) break;
        KillTimer(dialog, TIMER_CHECK);
        if (g_state == State::checking) {
            patch::log("update check: no answer in %lu ms", TIMEOUT_MS);
            g_state = State::unreachable;
            set_texts(dialog);
        }
        return TRUE;
    case WM_UPDATE_PROGRESS:
        if (g_state == State::downloading) {
            g_done = w;
            g_total = static_cast<unsigned long long>(l);
            set_texts(dialog);
        }
        return TRUE;
    case WM_CTLCOLORSTATIC:
        if (reinterpret_cast<HWND>(l) == GetDlgItem(dialog, ID_LINK)) {
            const HDC dc = reinterpret_cast<HDC>(w);
            SetTextColor(dc, RGB(0, 0, 204));
            SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
            return reinterpret_cast<INT_PTR>(GetSysColorBrush(COLOR_BTNFACE));
        }
        break;
    case WM_SETCURSOR:
        if (reinterpret_cast<HWND>(w) == GetDlgItem(dialog, ID_LINK)) {
            SetCursor(LoadCursor(nullptr, IDC_HAND));
            SetWindowLongPtrW(dialog, DWLP_MSGRESULT, TRUE);
            return TRUE;
        }
        break;
    case WM_CONTEXTMENU:
        if (reinterpret_cast<HWND>(w) == GetDlgItem(dialog, ID_LINK)) {
            link_menu(dialog, l);
            return TRUE;
        }
        break;
    case WM_DESTROY:
        g_cancel = 1;   // a download still going stops and removes its part
        break;
    }
    const INT_PTR result = g_launcher_proc(dialog, m, w, l);
    if (m == WM_INITDIALOG) add_update_row(dialog);
    if (m == WM_DESTROY && g_font) {
        DeleteObject(g_font);
        DeleteObject(g_link_font);
        g_font = g_link_font = nullptr;
    }
    return result;
}

INT_PTR WINAPI DialogBoxParamA_update(HINSTANCE instance, LPCSTR name, HWND parent, DLGPROC proc, LPARAM param) {
    if (!IS_INTRESOURCE(name) && lstrcmpiA(name, "START_DIALOG") == 0) {
        g_launcher_proc = proc;
        proc = &launcher_proc;
    }
    return g_dialog_box(instance, name, parent, proc, param);
}

}  // namespace

// After restart.cpp: we take over its DialogBoxParamA entry and call it in turn (it skips the
// launcher for a restarted game, which then checks nothing); language.cpp's launcher proc wraps
// ours and labels the row.
void install_update_check() {
    read_settings();
    g_dialog_box = *reinterpret_cast<DialogBoxParamA_t*>(IAT_DialogBoxParamA);
    void* dialog_box = reinterpret_cast<void*>(&DialogBoxParamA_update);
    patch::write(IAT_DialogBoxParamA, &dialog_box, sizeof dialog_box);
    add_launcher_label(ID_CHECK, "check_updates");
    add_launcher_label(ID_AT_START, "check_at_start");
    add_launcher_relabel(&set_texts);
}
