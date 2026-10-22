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

// ---- CRC-32 ----

uint32_t crc32(const std::string& d) {
    static uint32_t table[256];
    if (!table[1])
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
    uint32_t c = 0xffffffffu;
    for (unsigned char b : d) c = table[(c ^ b) & 0xff] ^ (c >> 8);
    return c ^ 0xffffffffu;
}

// ---- deflate (RFC 1951): a greedy LZ77 over a 32 KB window, fixed Huffman codes ----

const int LENGTH_BASE[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int LENGTH_EXTRA[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int DIST_BASE[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097,
                           6145, 8193, 12289, 16385, 24577};
const int DIST_EXTRA[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

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

// ---- inflate (RFC 1951): stored, fixed and dynamic blocks ----

struct BitReader {
    const unsigned char* p;
    size_t n, at = 0;
    uint32_t bits = 0;
    int count = 0;
    bool bad = false;
    int get(int need) {
        while (count < need) {
            if (at >= n) {
                bad = true;
                return 0;
            }
            bits |= static_cast<uint32_t>(p[at++]) << count;
            count += 8;
        }
        const int v = static_cast<int>(bits & ((1u << need) - 1));
        bits >>= need;
        count -= need;
        return v;
    }
};

struct Huffman {
    short counts[16] = {};
    std::vector<short> symbols;
    void build(const short* lengths, int n) {
        std::fill(std::begin(counts), std::end(counts), 0);
        for (int i = 0; i < n; ++i) ++counts[lengths[i]];
        counts[0] = 0;
        short offsets[16] = {};
        for (int i = 1; i < 16; ++i) offsets[i] = static_cast<short>(offsets[i - 1] + counts[i - 1]);
        symbols.assign(n, 0);
        for (int i = 0; i < n; ++i)
            if (lengths[i]) symbols[offsets[lengths[i]]++] = static_cast<short>(i);
    }
    int decode(BitReader& r) const {
        int code = 0, first = 0, index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= r.get(1);
            const int count = counts[len];
            if (code - count < first) return symbols[index + (code - first)];
            index += count;
            first = (first + count) << 1;
            code <<= 1;
            if (r.bad) return -1;
        }
        return -1;
    }
};

bool inflate_codes(BitReader& r, std::string& out, const Huffman& lit, const Huffman& dist) {
    for (;;) {
        const int sym = lit.decode(r);
        if (sym < 0 || r.bad) return false;
        if (sym < 256) {
            out += static_cast<char>(sym);
        } else if (sym == 256) {
            return true;
        } else {
            const int k = sym - 257;
            if (k >= 29) return false;
            const int len = LENGTH_BASE[k] + r.get(LENGTH_EXTRA[k]);
            const int ds = dist.decode(r);
            if (ds < 0 || ds >= 30) return false;
            const size_t d = static_cast<size_t>(DIST_BASE[ds] + r.get(DIST_EXTRA[ds]));
            if (d > out.size() || r.bad) return false;
            for (int i = 0; i < len; ++i) out += out[out.size() - d];
        }
    }
}

bool inflate(const std::string& in, std::string& out, size_t expected) {
    BitReader r{reinterpret_cast<const unsigned char*>(in.data()), in.size()};
    out.clear();
    out.reserve(expected);
    for (int last = 0; !last;) {
        last = r.get(1);
        const int type = r.get(2);
        if (r.bad) return false;
        if (type == 0) {   // stored
            r.bits = 0;
            r.count = 0;
            if (r.at + 4 > r.n) return false;
            const size_t len = r.p[r.at] | (r.p[r.at + 1] << 8);
            r.at += 4;
            if (r.at + len > r.n) return false;
            out.append(reinterpret_cast<const char*>(r.p + r.at), len);
            r.at += len;
        } else if (type == 1) {   // fixed
            static Huffman lit, dist;
            if (lit.symbols.empty()) {
                short l[288], d[30];
                for (int i = 0; i < 288; ++i) l[i] = static_cast<short>(i < 144 ? 8 : i < 256 ? 9 : i < 280 ? 7 : 8);
                for (int i = 0; i < 30; ++i) d[i] = 5;
                lit.build(l, 288);
                dist.build(d, 30);
            }
            if (!inflate_codes(r, out, lit, dist)) return false;
        } else if (type == 2) {   // dynamic
            const int nlen = r.get(5) + 257, ndist = r.get(5) + 1, ncode = r.get(4) + 4;
            static const int ORDER[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            short lengths[320] = {};
            for (int i = 0; i < ncode; ++i) lengths[ORDER[i]] = static_cast<short>(r.get(3));
            Huffman code;
            code.build(lengths, 19);
            short all[320] = {};
            for (int i = 0; i < nlen + ndist;) {
                const int sym = code.decode(r);
                if (sym < 0 || r.bad) return false;
                if (sym < 16) {
                    all[i++] = static_cast<short>(sym);
                } else {
                    short v = 0;
                    int repeat;
                    if (sym == 16) {
                        if (!i) return false;
                        v = all[i - 1];
                        repeat = 3 + r.get(2);
                    } else if (sym == 17) {
                        repeat = 3 + r.get(3);
                    } else {
                        repeat = 11 + r.get(7);
                    }
                    if (i + repeat > nlen + ndist) return false;
                    while (repeat--) all[i++] = v;
                }
            }
            Huffman lit, dist;
            lit.build(all, nlen);
            dist.build(all + nlen, ndist);
            if (!inflate_codes(r, out, lit, dist)) return false;
        } else {
            return false;
        }
    }
    return true;
}

// ---- zip ----

void put16(std::string& d, uint32_t v) { d.append(reinterpret_cast<const char*>(&v), 2); }
void put32(std::string& d, uint32_t v) { d.append(reinterpret_cast<const char*>(&v), 4); }
uint32_t u16(const std::string& d, size_t at) { return at + 2 <= d.size() ? static_cast<unsigned char>(d[at]) | static_cast<unsigned char>(d[at + 1]) << 8 : 0; }
uint32_t u32(const std::string& d, size_t at) { return at + 4 <= d.size() ? u16(d, at) | u16(d, at + 2) << 16 : 0; }

struct Entry {
    std::string name, data;
};

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

bool read_zip(const std::string& z, std::vector<Entry>& entries, std::string& error) {
    size_t end = std::string::npos;
    for (size_t i = z.size() >= 22 ? z.size() - 22 : 0; i + 22 <= z.size() && i + 0x10000 + 22 >= z.size(); --i) {
        if (u32(z, i) == 0x06054b50) {
            end = i;
            break;
        }
        if (!i) break;
    }
    if (end == std::string::npos) {
        error = "not a zip file";
        return false;
    }
    const uint32_t count = u16(z, end + 10);
    size_t at = u32(z, end + 16);
    for (uint32_t k = 0; k < count; ++k) {
        if (u32(z, at) != 0x02014b50) {
            error = "a damaged zip file";
            return false;
        }
        const uint32_t method = u16(z, at + 10), crc = u32(z, at + 16), packed = u32(z, at + 20), size = u32(z, at + 24);
        const uint32_t name_len = u16(z, at + 28), extra = u16(z, at + 30), comment = u16(z, at + 32), local = u32(z, at + 42);
        Entry e;
        e.name = z.substr(at + 46, name_len);
        at += 46 + name_len + extra + comment;
        if (e.name.empty() || e.name.back() == '/' || e.name.back() == '\\') continue;   // a folder
        if (u32(z, local) != 0x04034b50) {
            error = "a damaged zip file";
            return false;
        }
        const size_t data = local + 30 + u16(z, local + 26) + u16(z, local + 28);
        if (data + packed > z.size()) {
            error = "a damaged zip file";
            return false;
        }
        const std::string raw = z.substr(data, packed);
        if (method == 0) {
            e.data = raw;
        } else if (method != 8 || !inflate(raw, e.data, size)) {
            error = "a file of the zip cannot be unpacked: " + e.name;
            return false;
        }
        if (e.data.size() != size || crc32(e.data) != crc) {
            error = "a file of the zip is damaged: " + e.name;
            return false;
        }
        entries.push_back(e);
    }
    return true;
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
    p.name = lower(base_name(map_entry).substr(0, base_name(map_entry).size() - 4));
    for (const Entry& e : entries)
        if (folder_part(e.name) == dir) p.files.push_back({base_name(e.name), e.data});
    for (const PackageFile& e : p.files) {
        if (lower(e.name) == p.name + ".map") p.ground = map_ground_image_of(e.data);
        if (lower(e.name) == p.name + ".lgc") {
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
    const std::string old_single = "maps\\" + p.name + "\\", old_double = "maps\\\\" + p.name + "\\\\";
    const std::string new_single = folder, new_double = "maps\\\\" + name + "\\\\";
    std::string map;
    for (const PackageFile& e : p.files) {
        std::string file = e.name;
        if (starts_with_ci(file, p.name + ".")) file = name + file.substr(p.name.size());
        std::string data = e.data;
        const std::string ext = lower(file.substr(file.find_last_of('.') == std::string::npos ? file.size() : file.find_last_of('.')));
        if (name != p.name && (ext == ".lgc" || ext == ".lst" || ext == ".txt")) {
            // the folder in the paths, then the files that carry the name (<old>.map ...)
            data = replace_ci(replace_ci(data, old_double, new_double), old_single, new_single);
            data = replace_ci(data, new_double + p.name + ".", new_double + name + ".");
            data = replace_ci(data, new_single + p.name + ".", new_single + name + ".");
        }
        if (ext == ".map") {
            if (!p.ground.empty() && !campaign_image(p.ground))
                data = map_with_ground_image(data, folder + (starts_with_ci(base_name(p.ground), p.name + ".")
                                                                 ? name + base_name(p.ground).substr(p.name.size())
                                                                 : base_name(p.ground)));
            if (lower(file) == lower(name + ".map")) map = folder + file;
        }
        if (!write_file(folder + file, data)) {
            error = folder + file + " cannot be written";
            return "";
        }
    }
    patch::log("exchange: %s (%s) unpacked into %s, %u files", p.name.c_str(), p.title.c_str(), folder.c_str(),
               static_cast<unsigned>(p.files.size()));
    return map;
}

bool exchange_campaign_ground(const Package& p) { return p.ground.empty() || campaign_image(p.ground); }
