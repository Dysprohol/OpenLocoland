// Map exchange (DEVELOPERS.md "Editor mode"): File > Export map packs a map of the custom maps page into one
// .zip file, File > Import map unpacks such a file into maps\<name>\ of another install
// (editor.cpp asks the questions; this file does the packing).
//
// The package is an ordinary zip (deflate, or stored; zips made again by Windows or any tool are read
// too): <name>/<file> for every file of the map's folder - the .map, its ground image when the map has
// one of its own (a painted or generated ground: <name>.vid), the script, the texts, the briefing
// pages - but not the high scores (hiscores, hiscores.txt), which belong to the install. A map on a
// campaign terrain carries no picture of the game: its ground record points at maps\lNN\lNN.vid,
// which every install has. A ground image kept elsewhere (a map saved from another custom map) is
// packed as <name>.vid and the packed map points at it.
//
// Importing under another name (the folder is taken, or the name is a folder of the game) renames
// the files that carry the old name (<old>.map, .lgc, .vid) and the paths in the script, the lists
// and the map's ground record (maps\<old>\ -> maps\<new>\). Only the files of the map's own folder
// are read from the package, by their names alone, so nothing is written outside maps\<new>\.
#include "patch.h"
#include "editor_exchange.h"
#include "zip_read.h"
#include "custom_maps.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

// editor_newmap.cpp
std::string map_ground_image(const std::string& map);
std::string map_ground_image_of(const std::string& map_bytes);
std::string map_with_ground_image(const std::string& map_bytes, const std::string& image);

namespace {

// the zip reader and what deflate shares with it (zip_read.cpp)
using zipfile::crc32;
using zipfile::DIST_BASE;
using zipfile::DIST_EXTRA;
using zipfile::Entry;
using zipfile::LENGTH_BASE;
using zipfile::LENGTH_EXTRA;
using zipfile::read_zip;

// ---- files ----

std::string read_file(const std::string& path) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return "";
    std::string data(GetFileSize(f, nullptr), '\0');
    DWORD got = 0;
    if (!data.empty() && (!ReadFile(f, &data[0], static_cast<DWORD>(data.size()), &got, nullptr) || got != data.size()))
        data.clear();
    CloseHandle(f);
    return data;
}

bool write_file(const std::string& path, const std::string& data) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = data.empty() || (WriteFile(f, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) &&
                                     written == data.size());
    CloseHandle(f);
    return ok;
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool starts_with_ci(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && lower(s.substr(0, prefix.size())) == lower(prefix);
}

// Every occurrence of `from` (any case) replaced by `to`.
std::string replace_ci(const std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return s;
    const std::string ls = lower(s), lf = lower(from);
    std::string out;
    size_t at = 0;
    for (size_t hit = ls.find(lf); hit != std::string::npos; hit = ls.find(lf, at)) {
        out += s.substr(at, hit - at) + to;
        at = hit + from.size();
    }
    return out + s.substr(at);
}

// ---- deflate (RFC 1951): a greedy LZ77 over a 32 KB window, fixed Huffman codes ----

struct BitWriter {
    std::string out;
    uint32_t bits = 0;
    int count = 0;
    void put(uint32_t value, int n) {   // LSB first
        bits |= value << count;
        count += n;
        while (count >= 8) {
            out += static_cast<char>(bits & 0xff);
            bits >>= 8;
            count -= 8;
        }
    }
    void code(uint32_t code, int n) {   // a Huffman code, MSB first
        uint32_t r = 0;
        for (int i = 0; i < n; ++i) r |= ((code >> i) & 1) << (n - 1 - i);
        put(r, n);
    }
    void flush() {
        if (count) out += static_cast<char>(bits & 0xff);
        bits = 0;
        count = 0;
    }
};

void literal(BitWriter& w, int v) {   // the fixed code of a literal/length symbol
    if (v < 144) w.code(0x30 + v, 8);
    else if (v < 256) w.code(0x190 + v - 144, 9);
    else if (v < 280) w.code(v - 256, 7);
    else w.code(0xc0 + v - 280, 8);
}

std::string deflate(const std::string& d) {
    BitWriter w;
    w.put(1, 1);   // the last block
    w.put(1, 2);   // fixed Huffman codes
    const size_t n = d.size();
    const unsigned char* p = reinterpret_cast<const unsigned char*>(d.data());
    constexpr int HASH = 1 << 15, WINDOW = 32768, CHAIN = 48;
    std::vector<int> head(HASH, -1), prev(WINDOW, -1);
    auto hash = [&](size_t i) { return ((p[i] << 10) ^ (p[i + 1] << 5) ^ p[i + 2]) & (HASH - 1); };
    auto insert = [&](size_t i) {
        if (i + 2 >= n) return;
        const int h = hash(i);
        prev[i & (WINDOW - 1)] = head[h];
        head[h] = static_cast<int>(i);
    };
    for (size_t i = 0; i < n;) {
        int best = 0, dist = 0;
        if (i + 2 < n) {
            int candidate = head[hash(i)];
            for (int chain = 0; candidate >= 0 && chain < CHAIN; ++chain) {
                const size_t c = static_cast<size_t>(candidate);
                if (i - c > WINDOW - 1) break;
                int len = 0;
                const int limit = static_cast<int>((std::min)(static_cast<size_t>(258), n - i));
                while (len < limit && p[c + len] == p[i + len]) ++len;
                if (len > best) {
                    best = len;
                    dist = static_cast<int>(i - c);
                    if (len == 258) break;
                }
                const int next = prev[c & (WINDOW - 1)];
                if (next >= candidate) break;
                candidate = next;
            }
        }
        if (best >= 3) {
            int k = 28;
            while (LENGTH_BASE[k] > best) --k;
            literal(w, 257 + k);
            w.put(best - LENGTH_BASE[k], LENGTH_EXTRA[k]);
            int j = 29;
            while (DIST_BASE[j] > dist) --j;
            w.code(j, 5);
            w.put(dist - DIST_BASE[j], DIST_EXTRA[j]);
            for (int k2 = 0; k2 < best; ++k2) insert(i + k2);
            i += best;
        } else {
            literal(w, p[i]);
            insert(i);
            ++i;
        }
    }
    literal(w, 256);
    w.flush();
    return w.out;
}

// ---- zip ----

void put16(std::string& d, uint32_t v) { d.append(reinterpret_cast<const char*>(&v), 2); }
void put32(std::string& d, uint32_t v) { d.append(reinterpret_cast<const char*>(&v), 4); }

std::string write_zip(const std::vector<Entry>& entries) {
    SYSTEMTIME t;
    GetLocalTime(&t);
    const uint32_t time = (t.wHour << 11) | (t.wMinute << 5) | (t.wSecond / 2);
    const uint32_t date = ((t.wYear - 1980) << 9) | (t.wMonth << 5) | t.wDay;
    std::string out, directory;
    for (const Entry& e : entries) {
        const uint32_t crc = crc32(e.data);
        std::string packed = deflate(e.data);
        const bool stored = packed.size() >= e.data.size();
        if (stored) packed = e.data;
        const uint32_t offset = static_cast<uint32_t>(out.size());
        put32(out, 0x04034b50);
        put16(out, 20);
        put16(out, 0);
        put16(out, stored ? 0 : 8);
        put16(out, time);
        put16(out, date);
        put32(out, crc);
        put32(out, static_cast<uint32_t>(packed.size()));
        put32(out, static_cast<uint32_t>(e.data.size()));
        put16(out, static_cast<uint32_t>(e.name.size()));
        put16(out, 0);
        out += e.name + packed;
        put32(directory, 0x02014b50);
        put16(directory, 20);
        put16(directory, 20);
        put16(directory, 0);
        put16(directory, stored ? 0 : 8);
        put16(directory, time);
        put16(directory, date);
        put32(directory, crc);
        put32(directory, static_cast<uint32_t>(packed.size()));
        put32(directory, static_cast<uint32_t>(e.data.size()));
        put16(directory, static_cast<uint32_t>(e.name.size()));
        put16(directory, 0);
        put16(directory, 0);
        put16(directory, 0);
        put16(directory, 0);
        put32(directory, 0);
        put32(directory, offset);
        directory += e.name;
    }
    const uint32_t at = static_cast<uint32_t>(out.size());
    out += directory;
    put32(out, 0x06054b50);
    put16(out, 0);
    put16(out, 0);
    put16(out, static_cast<uint32_t>(entries.size()));
    put16(out, static_cast<uint32_t>(entries.size()));
    put32(out, static_cast<uint32_t>(directory.size()));
    put32(out, at);
    put16(out, 0);
    return out;
}

std::string base_name(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string folder_part(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? "" : path.substr(0, slash + 1);
}

bool campaign_image(const std::string& image) {   // maps\lNN\lNN.vid of the game
    const std::string l = lower(image);
    return l.size() == 16 && l.compare(0, 6, "maps\\l") == 0 && std::isdigit(static_cast<unsigned char>(l[6])) &&
           std::isdigit(static_cast<unsigned char>(l[7])) && l.compare(8, 2, "\\l") == 0 && l.compare(10, 2, l.substr(6, 2)) == 0 &&
           l.compare(12, 4, ".vid") == 0;
}

}  // namespace

// ---- export ----

// Packs the map file `map` (maps\<name>\<name>.map - a game path, as the map's ground record has
// them - saved) and its folder into the zip `zip`.
bool exchange_export(const std::string& map, const std::string& zip, std::string& error, int& files) {
    const std::string folder = folder_part(map), name = base_name(map).substr(0, base_name(map).find_last_of('.'));
    std::vector<Entry> entries;
    std::string map_bytes = read_file(map);
    if (map_bytes.empty()) {
        error = map + " cannot be read";
        return false;
    }
    const std::string image = map_ground_image_of(map_bytes);
    WIN32_FIND_DATAA f;
    HANDLE h = FindFirstFileA((folder + "*").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) {
        error = folder + " cannot be read";
        return false;
    }
    do {
        if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const std::string file = f.cFileName, l = lower(file);
        if (l == "hiscores" || l == "hiscores.txt" || l == lower(base_name(map))) continue;   // the scores stay; the map below
        // a picture that is not the map's ground (left by an earlier save): not packed
        if (l.size() > 4 && l.compare(l.size() - 4, 4, ".vid") == 0 && lower(folder + file) != lower(image)) continue;
        entries.push_back({name + "/" + file, read_file(folder + file)});
    } while (FindNextFileA(h, &f));
    FindClose(h);
    // a ground image of its own kept outside the folder: packed as <name>.vid
    if (!image.empty() && !campaign_image(image) && !starts_with_ci(image, folder)) {
        const std::string picture = read_file(image);
        if (picture.empty()) {
            error = "the ground's image " + image + " cannot be read";
            return false;
        }
        entries.push_back({name + "/" + name + ".vid", picture});
        map_bytes = map_with_ground_image(map_bytes, folder + name + ".vid");
    }
    entries.insert(entries.begin(), {name + "/" + base_name(map), map_bytes});
    if (!write_file(zip, write_zip(entries))) {
        error = zip + " cannot be written";
        return false;
    }
    files = static_cast<int>(entries.size());
    patch::log("exchange: %s packed into %s (%d files)", map.c_str(), zip.c_str(), files);
    return true;
}

// ---- import ----

bool exchange_read(const std::string& zip, Package& p, std::string& error) {
    std::vector<Entry> entries;
    if (!read_zip(read_file(zip), entries, error)) return false;
    std::string map_entry;
    for (const Entry& e : entries)
        if (lower(e.name).size() > 4 && lower(e.name).compare(lower(e.name).size() - 4, 4, ".map") == 0 &&
            (map_entry.empty() || e.name.size() < map_entry.size()))
            map_entry = e.name;
    if (map_entry.empty()) {
        error = "there is no .map in it";
        return false;
    }
    const std::string dir = folder_part(map_entry);
    p.name = base_name(map_entry).substr(0, base_name(map_entry).size() - 4);
    for (const Entry& e : entries)
        if (folder_part(e.name) == dir) p.files.push_back({base_name(e.name), e.data});
    for (const PackageFile& e : p.files) {
        if (lower(e.name) == lower(p.name + ".map")) p.ground = map_ground_image_of(e.data);
        if (lower(e.name) == lower(p.name + ".lgc")) {
            const size_t at = e.data.find("// title:");
            if (at != std::string::npos) {
                const size_t eol = e.data.find_first_of("\r\n", at);
                p.title = e.data.substr(at + 9, eol == std::string::npos ? std::string::npos : eol - at - 9);
                p.title.erase(0, p.title.find_first_not_of(' '));
                while (!p.title.empty() && p.title.back() == ' ') p.title.pop_back();
            }
        }
    }
    if (p.title.empty())
        for (char c : p.name) p.title += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const bool own = !p.ground.empty() && !campaign_image(p.ground);
    if (own && !starts_with_ci(p.ground, "maps\\" + p.name + "\\")) {
        error = "its ground image " + p.ground + " is not in it";
        return false;
    }
    if (own) {
        bool found = false;
        for (const PackageFile& e : p.files) found |= lower(e.name) == lower(base_name(p.ground));
        if (!found) {
            error = "its ground image " + base_name(p.ground) + " is not in it";
            return false;
        }
    }
    return true;
}

// The package's files as maps\<name>\ holds them: the files that carry the package's name (<old>.map,
// .lgc, .vid) renamed, the folder in the paths of the script, the lists and the texts (maps\<old>\ ->
// maps\<name>\, either spelling of the backslash) and the map's ground record pointing into the
// new folder when its picture is one of the package's files (a ground kept elsewhere - a campaign
// terrain, another map's picture - stays where it is).
std::vector<PackageFile> exchange_renamed(const Package& p, const std::string& name) {
    const std::string folder = "maps\\" + name + "\\";
    const std::string old_single = "maps\\" + p.name + "\\", old_double = "maps\\\\" + p.name + "\\\\";
    const std::string new_single = folder, new_double = "maps\\\\" + name + "\\\\";
    bool own_ground = false;
    for (const PackageFile& e : p.files) own_ground |= !p.ground.empty() && lower(e.name) == lower(base_name(p.ground));
    std::vector<PackageFile> out;
    for (const PackageFile& e : p.files) {
        std::string file = e.name;
        if (starts_with_ci(file, p.name + ".")) file = name + file.substr(p.name.size());
        std::string data = e.data;
        const std::string ext = lower(file.substr(file.find_last_of('.') == std::string::npos ? file.size() : file.find_last_of('.')));
        if (lower(name) != lower(p.name) && (ext == ".lgc" || ext == ".lst" || ext == ".txt")) {
            // the folder in the paths, then the files that carry the name (<old>.map ...)
            data = replace_ci(replace_ci(data, old_double, new_double), old_single, new_single);
            data = replace_ci(data, new_double + p.name + ".", new_double + name + ".");
            data = replace_ci(data, new_single + p.name + ".", new_single + name + ".");
            if (lower(file) == lower(name + ".lgc")) data = custom_map_retitled(data, p.name, name);   // a title that was the old name
        }
        if (ext == ".map" && own_ground && !campaign_image(p.ground))
            data = map_with_ground_image(data, folder + (starts_with_ci(base_name(p.ground), p.name + ".")
                                                             ? name + base_name(p.ground).substr(p.name.size())
                                                             : base_name(p.ground)));
        out.push_back({file, data});
    }
    return out;
}

// Writes a package into maps\<name>\ (`overwrite`: the files of an old map there go first), the
// names and paths renamed from the package's name. Returns the map's path, empty with `error`.
std::string exchange_install(const Package& p, const std::string& name, bool overwrite, std::string& error) {
    const std::string folder = "maps\\" + name + "\\";
    if (overwrite) {   // the old map's files (not its subfolders - a map has none)
        WIN32_FIND_DATAA f;
        HANDLE h = FindFirstFileA((folder + "*").c_str(), &f);
        if (h != INVALID_HANDLE_VALUE) {
            do
                if (!(f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) DeleteFileA((folder + f.cFileName).c_str());
            while (FindNextFileA(h, &f));
            FindClose(h);
        }
    }
    CreateDirectoryA(folder.c_str(), nullptr);
    std::string map;
    for (const PackageFile& e : exchange_renamed(p, name)) {
        if (lower(e.name) == lower(name + ".map")) map = folder + e.name;
        if (!write_file(folder + e.name, e.data)) {
            error = folder + e.name + " cannot be written";
            return "";
        }
    }
    patch::log("exchange: %s (%s) unpacked into %s, %u files", p.name.c_str(), p.title.c_str(), folder.c_str(),
               static_cast<unsigned>(p.files.size()));
    return map;
}

// Every file of the folder `folder` (a game path, maps\<folder>) as a package whose files and paths
// carry the name `name` - a map folder copied or renamed by hand (custom_maps.cpp), or one renamed in
// the editor. The high scores come along: they are the map's on this install.
bool exchange_read_folder(const std::string& folder, const std::string& name, Package& p, std::string& error) {
    p = Package();
    p.name = name;
    WIN32_FIND_DATAA f;
    HANDLE h = FindFirstFileA((folder + "\\*").c_str(), &f);
    if (h == INVALID_HANDLE_VALUE) {
        error = folder + " cannot be read";
        return false;
    }
    do {
        if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const std::string path = folder + "\\" + f.cFileName;
        std::string data = read_file(path);
        if (data.empty() && (f.nFileSizeLow || f.nFileSizeHigh)) {
            FindClose(h);
            error = path + " cannot be read";
            return false;
        }
        if (lower(f.cFileName) == lower(name + ".map")) p.ground = map_ground_image_of(data);
        p.files.push_back({f.cFileName, data});
    } while (FindNextFileA(h, &f));
    FindClose(h);
    return true;
}

bool exchange_campaign_ground(const Package& p) { return p.ground.empty() || campaign_image(p.ground); }
