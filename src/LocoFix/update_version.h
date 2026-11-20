// The launcher's update check (update_check.cpp): which version is newer, and which release of
// GitHub's answer is offered - in a header of its own, without Windows, so tools\test_release_versions.py
// tests the rule (tools\test_release_versions.cpp holds the table).
//
// A version is a tag as `git describe --tags` gives it: v<numbers>[-<pre-release>][-<n>-g<hash>][-dirty].
//   - The numbers (two at least, with dots): v0.1.10 is newer than v0.1.9; a missing one counts as 0
//     (0.2 == 0.2.0).
//   - A pre-release is a word of letters right after the numbers, with a number of its own or not
//     (beta.1, beta1, rc.2, beta - "dirty" is not one): older than the same numbers without it
//     (v0.2.12 < v0.2.13-beta.1 < v0.2.13), and two with the same numbers compare by the word
//     (alphabetically, any case: alpha < beta < rc) and then by its number (beta.2 < beta.10).
//   - The rest - the commits after the tag (-17-gcef9557) and -dirty - is left out: a build after a tag
//     counts as that tag (v0.2.12-3-gabc as v0.2.12, v0.2.13-beta.1-2-gabc as v0.2.13-beta.1), so it is
//     offered what is newer than its tag.
//   - Anything else is not a version - "dev", a bare commit hash (5476e33, 1234567), v0.2.13x - and a
//     build that has none is never offered a download (its line says what the latest is).
//
// The list of releases (the box "Include pre-releases": GitHub's /releases, newest first, which a
// visitor gets without drafts): the release offered is the one with the highest version by the rule
// above, drafts and tags that are no version left out, the first listed of equal ones. By the version,
// not the date: the one offered is then always the one the comparison ranks highest, and a release made
// later for an older version (a fix of an older line, one published again) cannot hide a newer one.
#pragma once
#include <string>
#include <vector>

namespace update_version {

struct Version {
    bool valid = false;
    std::vector<int> numbers;
    std::string pre;      // the pre-release's word in lowercase; empty for a release
    int pre_number = 0;   // its number (0 when it has none)
};

inline bool digit(char c) { return c >= '0' && c <= '9'; }
inline bool letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

// A number at `i` (at most 6 digits: a larger one is no version's).
inline bool read_number(const std::string& s, size_t& i, int& n) {
    n = 0;
    size_t start = i;
    while (i < s.size() && digit(s[i])) {
        if (i - start >= 6) return false;
        n = n * 10 + (s[i++] - '0');
    }
    return i > start;
}

inline Version parse(const std::string& v) {
    Version r;
    size_t i = !v.empty() && (v[0] == 'v' || v[0] == 'V') ? 1 : 0;
    for (;;) {
        int n = 0;
        if (!read_number(v, i, n)) return {};
        r.numbers.push_back(n);
        if (i < v.size() && v[i] == '.') {
            ++i;
            continue;
        }
        break;
    }
    if (r.numbers.size() < 2 || (i < v.size() && v[i] != '-')) return {};
    if (i + 1 < v.size() && letter(v[i + 1])) {
        size_t j = i + 1;
        std::string word;
        while (j < v.size() && letter(v[j])) word += static_cast<char>(v[j++] | 0x20);
        if (word != "dirty") {
            if (j + 1 < v.size() && v[j] == '.' && digit(v[j + 1])) ++j;
            int n = 0;
            if (j < v.size() && digit(v[j]) && !read_number(v, j, n)) return {};
            if (j < v.size() && v[j] != '-') return {};
            r.pre = word;
            r.pre_number = n;
        }
    }
    r.valid = true;
    return r;
}

// Like strcmp: < 0 when `a` is older than `b`. Two versions that are no versions are equal; a version
// is newer than one that is none.
inline int compare(const Version& a, const Version& b) {
    if (!a.valid || !b.valid) return a.valid - b.valid;
    for (size_t i = 0; i < a.numbers.size() || i < b.numbers.size(); ++i) {
        const int x = i < a.numbers.size() ? a.numbers[i] : 0, y = i < b.numbers.size() ? b.numbers[i] : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    if (a.pre.empty() != b.pre.empty()) return a.pre.empty() ? 1 : -1;   // the release after its pre-releases
    if (a.pre != b.pre) return a.pre < b.pre ? -1 : 1;
    if (a.pre_number != b.pre_number) return a.pre_number < b.pre_number ? -1 : 1;
    return 0;
}

inline int compare(const std::string& a, const std::string& b) { return compare(parse(a), parse(b)); }

// ---- GitHub's JSON, read as text: enough for a release's flat fields ----

// The position of the value of "key" at or after `from`, or npos.
inline size_t json_value(const std::string& json, const char* key, size_t from) {
    const std::string quoted = std::string("\"") + key + "\"";
    for (size_t at = json.find(quoted, from); at != std::string::npos; at = json.find(quoted, at + 1)) {
        size_t i = json.find_first_not_of(" \t\r\n", at + quoted.size());
        if (i == std::string::npos || json[i] != ':') continue;
        i = json.find_first_not_of(" \t\r\n", i + 1);
        if (i != std::string::npos) return i;
    }
    return std::string::npos;
}

inline std::string json_string(const std::string& json, size_t at) {
    std::string s;
    if (at == std::string::npos || json[at] != '"') return s;
    for (size_t i = at + 1; i < json.size() && json[i] != '"'; ++i) {
        if (json[i] == '\\' && i + 1 < json.size()) ++i;   // \" \\ \/ (the fields read have no others)
        s += json[i];
    }
    return s;
}

// The first "key" of the text is true (a release's draft and prerelease: its assets have neither).
inline bool json_true(const std::string& json, const char* key) {
    const size_t at = json_value(json, key, 0);
    return at != std::string::npos && json.compare(at, 4, "true") == 0;
}

// The objects of a JSON array (GitHub's list of releases), each as a text of its own; none when the
// text is no array. A last object cut off (the answer read only up to its bound) is left out.
inline std::vector<std::string> json_array_objects(const std::string& json) {
    std::vector<std::string> objects;
    size_t i = json.find_first_not_of(" \t\r\n\xEF\xBB\xBF");
    if (i == std::string::npos || json[i] != '[') return objects;
    int depth = 0;
    size_t start = 0;
    for (++i; i < json.size(); ++i) {
        const char c = json[i];
        if (c == '"') {   // a string: its brackets are text
            for (++i; i < json.size() && json[i] != '"'; ++i)
                if (json[i] == '\\') ++i;
        } else if (c == '{' || c == '[') {
            if (depth++ == 0) start = i;
        } else if (c == '}' || c == ']') {
            if (depth == 0) break;   // the array's end
            if (--depth == 0 && c == '}') objects.push_back(json.substr(start, i - start + 1));
        }
    }
    return objects;
}

// Of the releases of a list, the index of the one offered (see above), or -1 when none is: the highest
// version, drafts left out; when no tag is a version, the first that is no draft.
inline int newest_release(const std::vector<std::string>& releases) {
    int best = -1, first = -1;
    Version best_version;
    for (size_t k = 0; k < releases.size(); ++k) {
        if (json_true(releases[k], "draft")) continue;
        if (first < 0) first = static_cast<int>(k);
        const Version v = parse(json_string(releases[k], json_value(releases[k], "tag_name", 0)));
        if (v.valid && (best < 0 || compare(v, best_version) > 0)) {
            best = static_cast<int>(k);
            best_version = v;
        }
    }
    return best >= 0 ? best : first;
}

}  // namespace update_version
