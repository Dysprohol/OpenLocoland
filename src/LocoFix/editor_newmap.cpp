// File > New... and File > Map size... of the editor (DEVELOPERS.md "Editor mode").
//
// A new map is a map file of its own made from the terrain of a campaign map of the player's game,
// as map_tool.py new does it: the chunks of maps\lNN\lNN.map with only the ground object left - OBJ
// (the ground's VID 1024, whose image stays maps\lNN\lNN.vid of the game: nothing is copied), GRPH,
// HEAD, GRID, SPR and SPRD with the ground alone, no players (PLAY) and no groups (GROU). Or with a
// ground of its own: a meadow drawn here (value noise, the colours of the sandbox's ground) of any
// size from 512 to 4096 px a side in steps of 256 (the ground's tile; at most 4096 x 3072 as a
// whole), written as <name>.vid the way
// map_tool.py ground lays a ground out (a 256 x 256 R5G6B5 texture per tile, column by column, each
// followed by its height texture), on the structure of mission 1's map with the size set, its GRID
// flat at 70, the picture's height that of a campaign ground at 70 (FLAT_PICTURE_HEIGHT) and the
// ground object in the middle (the picture is drawn centred on it). Either way the map gets the
// template's script (maps\template\template.lgc, with the editor's blocks) and texts under its
// own name, and the editor loads it; File > Mission settings and Save do the rest.
//
// Map size moves the right and the bottom edge of a saved map's file: HEAD's size, the GRID (a new
// cell gets the height of the old cell nearest to it); growing writes a ground image of its own -
// the old picture with its heights (read from its texture tiles, the map's own or the campaign's)
// and a meadow beyond it - and puts the ground object in the middle; cropping cuts the image the same
// way. The images are read and written a tile at a time (below), the resized map and its image put in
// place together (replace_map_files, which the editor's Save uses too), and the sizes the editor makes
// are bounded in area (map_area_band; DEVELOPERS.md "How big a map can be").
#include "patch.h"
#include "dialog_template.h"
#include "editor_ground.h"
#include "editor_text.h"
#include "custom_maps.h"

std::string language_file(const std::string& path);   // language.cpp: the pack's copy of a file

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwchar>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <vector>

void editor_load_map(const std::string& map);   // editor.cpp
std::string editor_new_map_title();               // editor.cpp: the new map's title until it is saved

namespace {

constexpr int GROUND_NVID = 1024;
constexpr short FLAT_LEVEL = 70;   // the level of a generated ground (rails at 66, as the missions)
constexpr int TILE = 256;
constexpr uint32_t CHUNK_FLAGS = 0x80000000;
constexpr size_t OBJ_IMAGE_AFTER_NAME = 0x26c;   // the image file name in an OBJ record, after the name
constexpr const char* TEMPLATE_FOLDER = "maps\\template\\";

// the campaign maps: their number and size (DEVELOPERS.md "Resolutions and widescreen")
struct Terrain {
    int mission;
    int width, height;
};
const Terrain CAMPAIGN[] = {{1, 1024, 768},   {2, 2048, 1536},  {3, 2048, 1536},  {4, 2048, 1536},  {5, 2048, 1536},
                            {6, 2048, 1536},  {7, 2048, 1536},  {8, 3072, 2304},  {9, 3072, 2304},  {10, 2048, 1536},
                            {11, 3072, 2304}, {12, 2048, 1536}, {13, 3072, 2304}, {14, 3072, 2304}, {15, 3072, 2304},
                            {16, 3072, 2304}, {17, 3072, 2304}, {18, 3072, 2304}, {19, 2048, 1536}, {20, 3072, 2304}};

// ---- the RES container (DEVELOPERS.md "Data formats") ----

struct Chunk {
    std::string id;
    std::vector<std::string> records;
};

uint32_t u32(const std::string& d, size_t at) {
    uint32_t v = 0;
    if (at + 4 <= d.size()) std::memcpy(&v, &d[at], 4);
    return v;
}

void put32(std::string& d, uint32_t v) { d.append(reinterpret_cast<const char*>(&v), 4); }
void put16(std::string& d, uint16_t v) { d.append(reinterpret_cast<const char*>(&v), 2); }

bool read_res(const std::string& d, const char* kind, std::vector<Chunk>& out) {
    if (d.size() < 12 || d.compare(0, 4, "RES ") != 0 || d.compare(8, 4, kind) != 0) return false;
    for (size_t pos = 12; pos + 8 <= d.size();) {
        Chunk c;
        c.id = d.substr(pos, 4);
        const uint32_t size = u32(d, pos + 4);
        const size_t end = pos + 8 + size;
        if (end > d.size()) return false;
        const uint32_t flags = u32(d, pos + 8);
        uint32_t count = flags;
        size_t at = pos + 12;
        if (flags & CHUNK_FLAGS) {
            count = u32(d, pos + 16);
            at = pos + 20;
        }
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t n = u32(d, at);
            if (at + 4 + n > end) return false;
            c.records.push_back(d.substr(at + 4, n));
            at += 4 + n;
        }
        out.push_back(c);
        pos = end + (size & 1);
    }
    return true;
}

// res_tool.write_chunks: every chunk padded to an even size but the last; the RES size counts the
// chunks without the padding.
std::string write_res(const char* kind, const std::vector<Chunk>& chunks) {
    std::string body;
    uint32_t unpadded = 4;
    for (size_t i = 0; i < chunks.size(); ++i) {
        std::string payload;
        put32(payload, CHUNK_FLAGS);
        put32(payload, 0);
        put32(payload, static_cast<uint32_t>(chunks[i].records.size()));
        for (const std::string& r : chunks[i].records) {
            put32(payload, static_cast<uint32_t>(r.size()));
            payload += r;
        }
        body += chunks[i].id;
        put32(body, static_cast<uint32_t>(payload.size()));
        body += payload;
        unpadded += 8 + static_cast<uint32_t>(payload.size());
        if ((payload.size() & 1) && i + 1 < chunks.size()) body += '\0';
    }
    std::string out = "RES ";
    put32(out, unpadded);
    return out + kind + body;
}

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

std::string replace_all(std::string s, const std::string& from, const std::string& to) {
    for (size_t at = s.find(from); at != std::string::npos; at = s.find(from, at + to.size())) s.replace(at, from.size(), to);
    return s;
}

// ---- a generated ground ----

uint32_t hash(int x, int y, uint32_t seed) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// Smooth value noise in 0..1 with features about `cell` px wide.
float noise(int x, int y, int cell, uint32_t seed) {
    const int gx = x / cell, gy = y / cell;
    float fx = static_cast<float>(x % cell) / cell, fy = static_cast<float>(y % cell) / cell;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    auto v = [&](int i, int j) { return (hash(gx + i, gy + j, seed) & 0xffff) / 65535.0f; };
    const float top = v(0, 0) + (v(1, 0) - v(0, 0)) * fx, bottom = v(0, 1) + (v(1, 1) - v(0, 1)) * fx;
    return top + (bottom - top) * fy;
}

uint16_t rgb565(float r, float g, float b) {
    auto c = [](float v) { return static_cast<int>(v < 0 ? 0 : v > 255 ? 255 : v); };
    return static_cast<uint16_t>(((c(r) >> 3) << 11) | ((c(g) >> 2) << 5) | (c(b) >> 3));
}

// A meadow: grass of two greens in big soft patches, drier patches and a fine grain.
uint16_t meadow(int x, int y, uint32_t seed) {
    const float patch = noise(x, y, 160, seed) * 0.6f + noise(x, y, 48, seed + 1) * 0.3f + noise(x, y, 12, seed + 2) * 0.1f;
    const float dry = noise(x, y, 220, seed + 3);
    const float grain = (hash(x, y, seed + 4) & 0xff) / 255.0f - 0.5f;
    float r = 70 + 60 * patch, g = 110 + 55 * patch, b = 35 + 20 * patch;
    if (dry > 0.62f) {   // drier grass
        const float t = (std::min)(1.0f, (dry - 0.62f) * 4);
        r += (150 - r) * t * 0.6f;
        g += (145 - g) * t * 0.5f;
        b += (70 - b) * t * 0.5f;
    }
    return rgb565(r + grain * 14, g + grain * 18, b + grain * 8);
}

// The height of the picture of a flat ground at FLAT_LEVEL: what the campaign's grounds hold where
// their GRID is 70 (l01: 1586 on the paving, 1558 at 62 - about 3.5 a unit). The depth of the
// objects is measured against it, so a generated ground hides the lower part of what stands below
// its level (the slope of a gravel bed) as the campaign's grounds do.
constexpr uint16_t FLAT_PICTURE_HEIGHT = 1586;

// A ground's picture: its colours (R5G6B5) and heights, row by row.
struct Picture {
    int width = 0, height = 0;
    std::vector<uint16_t> pixels, heights;
};

// ---- a ground image read and written a tile at a time ----
//
// A ground image holds the whole picture (4 bytes a pixel: the colours and the heights), and the
// editor runs in the 2 GB of a 32-bit process (DEVELOPERS.md "How big a map can be"). Reading the
// file whole, its chunks' records, SURF and then the picture, or building the file in memory as
// a string grown chunk by chunk, took about seven whole copies of the picture at once - Save of a
// painted 4096 x 4096 map ran out of memory that way. Here the file is read where it is needed
// (the chunks' headers walked, each texture read when a tile shows it) and written as it is laid
// out, one 256 x 256 tile at a time through a buffer: the picture itself is the only whole copy.

// A file read at any place (a ground image can be hundreds of MB).
class FileReader {
public:
    explicit FileReader(const std::string& path)
        : f_(CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr)) {
        if (f_ != INVALID_HANDLE_VALUE) size_ = GetFileSize(f_, nullptr);   // as read_file: the low 32 bits
    }
    ~FileReader() {
        if (f_ != INVALID_HANDLE_VALUE) CloseHandle(f_);
    }
    FileReader(const FileReader&) = delete;
    FileReader& operator=(const FileReader&) = delete;
    bool open() const { return f_ != INVALID_HANDLE_VALUE && size_ != INVALID_FILE_SIZE; }
    size_t size() const { return size_; }
    // `n` bytes at `at`; false when they are not all there
    bool read(size_t at, void* to, size_t n) {
        if (at > size_ || n > size_ - at) return false;
        if (!n) return true;
        LARGE_INTEGER pos;
        pos.QuadPart = static_cast<LONGLONG>(at);
        DWORD got = 0;
        return SetFilePointerEx(f_, pos, nullptr, FILE_BEGIN) && ReadFile(f_, to, static_cast<DWORD>(n), &got, nullptr) && got == n;
    }
    // u32 as read_res reads it: 0 where the 4 bytes are not inside the first `limit` bytes
    uint32_t u32(size_t at, size_t limit) {
        uint32_t v = 0;
        if (at + 4 <= limit && read(at, &v, 4)) return v;
        return 0;
    }

private:
    HANDLE f_;
    size_t size_ = 0;
};

// The first record of a RES file's chunks with an id, found as read_res would find it (the whole
// container checked the same way, the last chunk of that id with a record wins) without reading
// the records themselves: where it starts in the file and its size.
struct RecordAt {
    size_t at = 0, size = 0;
    bool found = false;
};

bool find_records(FileReader& f, const char* kind, const char* const* ids, RecordAt* out, int count) {
    char head[12];
    if (!f.open() || f.size() < 12 || !f.read(0, head, 12) || std::memcmp(head, "RES ", 4) != 0 || std::memcmp(head + 8, kind, 4) != 0)
        return false;
    const size_t total = f.size();
    for (size_t pos = 12; pos + 8 <= total;) {
        char id[4];
        if (!f.read(pos, id, 4)) return false;
        const uint32_t size = f.u32(pos + 4, total);
        const size_t end = pos + 8 + size;
        if (end > total || end < pos) return false;
        const uint32_t flags = f.u32(pos + 8, total);
        uint32_t n = flags;
        size_t at = pos + 12;
        if (flags & CHUNK_FLAGS) {
            n = f.u32(pos + 16, total);
            at = pos + 20;
        }
        RecordAt first;
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t length = f.u32(at, total);
            if (at + 4 + length > end || at + 4 + length < at) return false;
            if (i == 0) first = {at + 4, length, true};
            at += 4 + length;
        }
        if (first.found)
            for (int k = 0; k < count; ++k)
                if (std::memcmp(id, ids[k], 4) == 0) out[k] = first;
        pos = end + (size & 1);
    }
    return true;
}

// The size of a ground image's picture (its HEAD), without reading the picture.
bool ground_size(const std::string& path, int& w, int& h) {
    FileReader f(path);
    const char* const ids[] = {"HEAD"};
    RecordAt head;
    uint8_t bytes[10];
    if (!find_records(f, "VID ", ids, &head, 1) || !head.found || head.size < 10 || !f.read(head.at, bytes, 10)) return false;
    uint16_t flags, pw, ph;
    std::memcpy(&flags, bytes, 2);
    std::memcpy(&pw, bytes + 6, 2);
    std::memcpy(&ph, bytes + 8, 2);
    if (!(flags & 0x20)) return false;
    w = pw;
    h = ph;
    return true;
}

// The picture of a ground image (DEVELOPERS.md "Data formats": a texture image of one frame whose tiles
// show whole textures; with imageFlags 4 every colour texture is followed by its heights, which a
// tile's texture field counts; tiles are 9 ints with imageFlags 0x10, else an int and 8 shorts). The
// result is what read_ground_whole (the reading of the whole file, kept for the tests) makes of it.
bool read_ground_tiles(const std::string& path, Picture& p) {
    memory_test_point("read");
    FileReader f(path);
    const char* const ids[] = {"HEAD", "SURF", "DATA"};
    RecordAt rec[3];
    if (!find_records(f, "VID ", ids, rec, 3)) return false;
    std::string head(rec[0].found ? rec[0].size : 0, '\0'), data(rec[2].found ? rec[2].size : 0, '\0');
    const size_t surf_at = rec[1].at, surf_size = rec[1].found ? rec[1].size : 0;
    if (head.size() < 10 || surf_size < 2 || !f.read(rec[0].at, &head[0], head.size()) ||
        (!data.empty() && !f.read(rec[2].at, &data[0], data.size())))
        return false;
    uint16_t flags, w, h;
    std::memcpy(&flags, &head[0], 2);
    std::memcpy(&w, &head[6], 2);
    std::memcpy(&h, &head[8], 2);
    if (!(flags & 0x20)) return false;
    struct Texture {
        int w, h;
        size_t at, heights;   // the colours and the heights (0: none), from SURF's start
    };
    std::vector<Texture> colours;
    uint16_t count = 0;
    if (!f.read(surf_at, &count, 2)) return false;
    size_t at = 2;
    for (int k = 0; k < count && at + 8 <= surf_size; ++k) {
        int16_t tw = 0, th = 0;
        f.read(surf_at + at, &tw, 2);
        f.read(surf_at + at + 2, &th, 2);
        const uint32_t size = f.u32(surf_at + at + 4, surf_at + surf_size);
        Texture texture = {tw, th, at + 8, 0};
        at += 8 + size;
        if (flags & 4) {   // the heights after the colours
            ++k;
            texture.heights = at + 4;
            at += 4 + f.u32(surf_at + at, surf_at + surf_size);
        }
        colours.push_back(texture);
    }
    p.width = w;
    p.height = h;
    p.pixels.assign(static_cast<size_t>(w) * h, 0);
    p.heights.assign(static_cast<size_t>(w) * h, FLAT_PICTURE_HEIGHT);
    // the texture a tile shows, read once while the tiles after it show it too
    std::vector<uint16_t> colour, height;
    size_t height_count = 0;   // how many of `height` the file has (the rest is not read: past SURF)
    int loaded = -1;
    const size_t tile_size = (flags & 0x10) ? 36 : 20;
    for (size_t t = 0; t + tile_size <= data.size(); t += tile_size) {
        int v[8];   // texture, offsetX, offsetY, width, height, x, y, next
        if (flags & 0x10) {
            std::memcpy(v, &data[t + 4], 32);
        } else {
            int16_t s[8];
            std::memcpy(s, &data[t + 4], 16);
            for (int k = 0; k < 8; ++k) v[k] = s[k];
        }
        const int index = (flags & 4) ? v[0] / 2 : v[0];
        if (index < 0 || index >= static_cast<int>(colours.size())) continue;
        const Texture& tex = colours[index];
        if (index != loaded) {
            loaded = index;
            const size_t n = static_cast<size_t>((std::max)(0, tex.w)) * (std::max)(0, tex.h);
            colour.assign(n, 0);
            height.assign(n, 0);
            // the colours as far as SURF has them, the heights only where a whole one is inside it
            const size_t colours_in = tex.at < surf_size ? (std::min)(n, (surf_size - tex.at) / 2) : 0;
            f.read(surf_at + tex.at, colour.data(), colours_in * 2);
            height_count = tex.heights && tex.heights < surf_size ? (std::min)(n, (surf_size - tex.heights) / 2) : 0;
            f.read(surf_at + tex.heights, height.data(), height_count * 2);
        }
        for (int y = 0; y < v[4]; ++y)
            for (int x = 0; x < v[3]; ++x) {
                const int sx = v[1] + x, sy = v[2] + y, dx = v[5] + x, dy = v[6] + y;
                if (sx < 0 || sy < 0 || sx >= tex.w || sy >= tex.h || dx < 0 || dy < 0 || dx >= w || dy >= h) continue;
                const size_t to = static_cast<size_t>(dy) * w + dx, from = static_cast<size_t>(sy) * tex.w + sx;
                p.pixels[to] = colour[from];
                if (from < height_count) p.heights[to] = height[from];
            }
    }
    return true;
}

// A file written from the start to the end through a buffer.
class FileWriter {
public:
    explicit FileWriter(const std::string& path)
        : path_(path), f_(CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)) {
        buffer_.reserve(BUFFER);
    }
    ~FileWriter() {
        if (f_ != INVALID_HANDLE_VALUE) {   // not closed: a failure on the way, the file goes
            CloseHandle(f_);
            DeleteFileA(path_.c_str());
        }
    }
    FileWriter(const FileWriter&) = delete;
    FileWriter& operator=(const FileWriter&) = delete;
    bool open() const { return f_ != INVALID_HANDLE_VALUE; }
    void put(const void* data, size_t n) {
        const char* c = static_cast<const char*>(data);
        while (n && ok_) {
            const size_t room = (std::min)(n, BUFFER - buffer_.size());
            buffer_.insert(buffer_.end(), c, c + room);
            c += room;
            n -= room;
            if (buffer_.size() == BUFFER) flush();
        }
    }
    void put32(uint32_t v) { put(&v, 4); }
    void put16(uint16_t v) { put(&v, 2); }
    void put(const std::string& s) { put(s.data(), s.size()); }
    // the rest written and the file closed; false (and the file deleted) when anything failed
    bool close() {
        flush();
        const bool ok = ok_ && CloseHandle(f_);
        f_ = INVALID_HANDLE_VALUE;
        if (!ok) DeleteFileA(path_.c_str());
        return ok;
    }

private:
    static constexpr size_t BUFFER = 1 << 20;
    void flush() {
        if (!ok_ || buffer_.empty()) return;
        DWORD written = 0;
        ok_ = WriteFile(f_, buffer_.data(), static_cast<DWORD>(buffer_.size()), &written, nullptr) && written == buffer_.size();
        buffer_.clear();
    }
    std::string path_;
    HANDLE f_;
    std::vector<char> buffer_;
    bool ok_ = true;
};

// A ground image of width x height (whole tiles) as ground_vid_whole lays it out and write_res
// packs it - byte for byte -, written into `path`: map_tool.py ground's layout (256 x 256 R5G6B5
// textures column by column, each with its height texture, imageFlags 0x35), the colour and the
// height of every pixel from `pixel(px, py, colour, height)`. False when the file could not be written.
template <typename F> bool write_ground_vid(const std::string& path, int width, int height, F pixel) {
    memory_test_point("write");
    const int columns = width / TILE, rows = height / TILE, count = columns * rows;
    const uint32_t texture_bytes = TILE * TILE * 2;
    // the three records and their chunks (write_res: flags, 0, the count, each record's size; every
    // chunk padded to an even size but the last - these three are even)
    const uint32_t head_size = 10, surf_size = 2 + static_cast<uint32_t>(count) * (8 + texture_bytes + 4 + texture_bytes),
                   data_size = static_cast<uint32_t>(count) * 36;
    const uint32_t unpadded = 4 + (8 + 16 + head_size) + (8 + 16 + surf_size) + (8 + 16 + data_size);
    FileWriter out(path);
    if (!out.open()) return false;
    auto chunk = [&](const char* id, uint32_t size) {
        out.put(id, 4);
        out.put32(16 + size);
        out.put32(CHUNK_FLAGS);
        out.put32(0);
        out.put32(1);
        out.put32(size);
    };
    out.put("RES ", 4);
    out.put32(unpadded);
    out.put("VID ", 4);
    chunk("HEAD", head_size);
    out.put16(0x35);   // imageFlags: textures, height textures, tiles as ints
    out.put16(71);     // frameSpeed
    out.put16(1);      // frameCount
    out.put16(static_cast<uint16_t>(width));
    out.put16(static_cast<uint16_t>(height));
    chunk("SURF", surf_size);
    out.put16(static_cast<uint16_t>(count * 2));
    std::vector<uint16_t> colours(TILE * TILE), heights(TILE * TILE);
    for (int k = 0; k < count; ++k) {
        const int x0 = k / rows * TILE, y0 = k % rows * TILE;   // column by column
        for (int y = 0; y < TILE; ++y)
            for (int x = 0; x < TILE; ++x) pixel(x0 + x, y0 + y, colours[y * TILE + x], heights[y * TILE + x]);
        out.put16(TILE);
        out.put16(TILE);
        out.put32(texture_bytes);
        out.put(colours.data(), texture_bytes);
        out.put32(texture_bytes);
        out.put(heights.data(), texture_bytes);
    }
    chunk("DATA", data_size);
    for (int k = 0; k < count; ++k) {
        const int32_t tile[9] = {0, 2 * k, 0, 0, TILE, TILE, k / rows * TILE, k % rows * TILE, (k + 1) % count};
        out.put(tile, sizeof tile);
    }
    return out.close();
}

// A plain ground: a texture's 128 x 128 sample laid mirrored (no seams), its brightness varied in
// soft patches a few hundred px wide so the repeat does not show as a pattern.
uint16_t plain(const std::vector<uint16_t>& sample, int x, int y) {
    int u = x & 255, v = y & 255;
    if (u >= 128) u = 255 - u;
    if (v >= 128) v = 255 - v;
    const uint16_t c = sample[static_cast<size_t>(v) * 128 + u];
    const float k = 0.9f + 0.2f * (noise(x, y, 300, 7) * 0.7f + noise(x, y, 90, 8) * 0.3f);
    return rgb565((c >> 11) * 8.2f * k, ((c >> 5) & 63) * 4.05f * k, (c & 31) * 8.2f * k);
}

#if LOCOFIX_SAVE_TEST
// The test build: the image read and built whole, the way of v0.2.9 - every image the editor reads
// or writes is compared with it (LOCOFIX_VID_COMPARE=1; read_ground_whole below).
bool read_ground_whole(const std::string& path, Picture& p) {
    std::vector<Chunk> chunks;
    if (!read_res(read_file(path), "VID ", chunks)) return false;
    std::string head, surf, data;
    for (const Chunk& c : chunks) {
        if (c.records.empty()) continue;
        if (c.id == "HEAD") head = c.records[0];
        else if (c.id == "SURF") surf = c.records[0];
        else if (c.id == "DATA") data = c.records[0];
    }
    if (head.size() < 10 || surf.size() < 2) return false;
    uint16_t flags, w, h;
    std::memcpy(&flags, &head[0], 2);
    std::memcpy(&w, &head[6], 2);
    std::memcpy(&h, &head[8], 2);
    if (!(flags & 0x20)) return false;
    struct Texture {
        int w, h;
        size_t at, heights;   // the colours and the heights (0: none)
    };
    std::vector<Texture> colours;
    uint16_t count;
    std::memcpy(&count, &surf[0], 2);
    size_t at = 2;
    for (int k = 0; k < count && at + 8 <= surf.size(); ++k) {
        int16_t tw, th;
        std::memcpy(&tw, &surf[at], 2);
        std::memcpy(&th, &surf[at + 2], 2);
        const uint32_t size = u32(surf, at + 4);
        Texture texture = {tw, th, at + 8, 0};
        at += 8 + size;
        if (flags & 4) {   // the heights after the colours
            ++k;
            texture.heights = at + 4;
            at += 4 + u32(surf, at);
        }
        colours.push_back(texture);
    }
    p.width = w;
    p.height = h;
    p.pixels.assign(static_cast<size_t>(w) * h, 0);
    p.heights.assign(static_cast<size_t>(w) * h, FLAT_PICTURE_HEIGHT);
    const size_t tile_size = (flags & 0x10) ? 36 : 20;
    for (size_t t = 0; t + tile_size <= data.size(); t += tile_size) {
        int v[8];   // texture, offsetX, offsetY, width, height, x, y, next
        if (flags & 0x10) {
            std::memcpy(v, &data[t + 4], 32);
        } else {
            int16_t s[8];
            std::memcpy(s, &data[t + 4], 16);
            for (int k = 0; k < 8; ++k) v[k] = s[k];
        }
        const int index = (flags & 4) ? v[0] / 2 : v[0];
        if (index < 0 || index >= static_cast<int>(colours.size())) continue;
        const Texture& tex = colours[index];
        for (int y = 0; y < v[4]; ++y)
            for (int x = 0; x < v[3]; ++x) {
                const int sx = v[1] + x, sy = v[2] + y, dx = v[5] + x, dy = v[6] + y;
                if (sx >= tex.w || sy >= tex.h || dx < 0 || dy < 0 || dx >= w || dy >= h) continue;
                const size_t to = static_cast<size_t>(dy) * w + dx, from = (static_cast<size_t>(sy) * tex.w + sx) * 2;
                std::memcpy(&p.pixels[to], &surf[tex.at + from], 2);
                if (tex.heights && tex.heights + from + 2 <= surf.size()) std::memcpy(&p.heights[to], &surf[tex.heights + from], 2);
            }
    }
    return true;
}

// The ground image built whole (DEVELOPERS.md "Data formats": texture tiles), as map_tool.py ground lays it out: the
// picture of `old` where it reaches, elsewhere a meadow - or, with `sample` (a texture's 128 x 128,
// editor_ground.cpp), that texture alone: an empty map's ground.
std::string ground_vid_whole(int width, int height, uint32_t seed, const Picture* old = nullptr,
                       const std::vector<uint16_t>* sample = nullptr) {
    const int columns = width / TILE, rows = height / TILE, count = columns * rows;
    std::string head;
    put16(head, 0x35);    // imageFlags: textures, height textures, tiles as ints
    put16(head, 71);      // frameSpeed
    put16(head, 1);       // frameCount
    put16(head, static_cast<uint16_t>(width));
    put16(head, static_cast<uint16_t>(height));
    std::string surf;
    put16(surf, static_cast<uint16_t>(count * 2));
    std::string tiles;
    for (int k = 0; k < count; ++k) {
        const int x0 = k / rows * TILE, y0 = k % rows * TILE;   // column by column
        put16(surf, TILE);
        put16(surf, TILE);
        put32(surf, TILE * TILE * 2);
        for (int y = 0; y < TILE; ++y)
            for (int x = 0; x < TILE; ++x) {
                const int px = x0 + x, py = y0 + y;
                put16(surf, old && px < old->width && py < old->height ? old->pixels[static_cast<size_t>(py) * old->width + px]
                            : sample && sample->size() == 128 * 128               ? plain(*sample, px, py)
                                                                                   : meadow(px, py, seed));
            }
        put32(surf, TILE * TILE * 2);
        for (int y = 0; y < TILE; ++y)   // the heights: the old picture's, else a flat ground's
            for (int x = 0; x < TILE; ++x) {
                const int px = x0 + x, py = y0 + y;
                put16(surf, old && px < old->width && py < old->height ? old->heights[static_cast<size_t>(py) * old->width + px]
                                                                        : FLAT_PICTURE_HEIGHT);
            }
        const int32_t tile[9] = {0, 2 * k, 0, 0, TILE, TILE, x0, y0, (k + 1) % count};
        tiles.append(reinterpret_cast<const char*>(tile), sizeof tile);
    }
    return write_res("VID ", {{"HEAD", {head}}, {"SURF", {surf}}, {"DATA", {tiles}}});
}


// The test build's checks (LOCOFIX_VID_COMPARE=1): an image just written against the one the whole
// way builds, a picture just read against the one the whole reading makes. Logged, never fatal.
bool g_force_compare = false;   // vid_check_all
bool vid_compare() {
    char value[8] = "";
    return g_force_compare || (GetEnvironmentVariableA("LOCOFIX_VID_COMPARE", value, sizeof value) && value[0] == '1');
}

void compare_written(const std::string& path, const std::string& whole) {
    const std::string file = read_file(path);
    size_t at = 0;
    while (at < file.size() && at < whole.size() && file[at] == whole[at]) ++at;
    if (file.size() == whole.size() && at == file.size())
        patch::log("vid check: %s written tile by tile = built whole (%u bytes)", path.c_str(), static_cast<unsigned>(file.size()));
    else
        patch::log("vid check: %s DIFFERS from the whole build at byte %u (%u and %u bytes)", path.c_str(), static_cast<unsigned>(at),
                   static_cast<unsigned>(file.size()), static_cast<unsigned>(whole.size()));
}

void compare_read(const std::string& path, bool ok, const Picture& p) {
    Picture q;
    const bool whole = read_ground_whole(path, q);
    if (ok == whole && (!ok || (p.width == q.width && p.height == q.height && p.pixels == q.pixels && p.heights == q.heights)))
        patch::log("vid check: %s read tile by tile = read whole (%dx%d%s)", path.c_str(), p.width, p.height, ok ? "" : ", both refuse it");
    else
        patch::log("vid check: %s read DIFFERS from the whole reading (%s / %s, %dx%d / %dx%d)", path.c_str(), ok ? "read" : "refused",
                   whole ? "read" : "refused", p.width, p.height, q.width, q.height);
}
#endif

bool read_ground(const std::string& path, Picture& p) {
    const bool ok = read_ground_tiles(path, p);
#if LOCOFIX_SAVE_TEST
    if (vid_compare()) compare_read(path, ok, p);
#endif
    return ok;
}

// A ground image written by write_ground_vid; the test build compares it with what `whole` builds
// (WHOLE(ground_vid_whole(...)) - only there, the shipped build has no whole way).
#if LOCOFIX_SAVE_TEST
#define WHOLE(...) [&] { return __VA_ARGS__; }
#else
#define WHOLE(...) [] { return std::string(); }
#endif
template <typename F, typename W> bool write_ground(const std::string& path, int width, int height, F pixel, W whole) {
    const bool ok = write_ground_vid(path, width, height, pixel);
#if LOCOFIX_SAVE_TEST
    if (ok && vid_compare()) compare_written(path, whole());
#else
    (void)whole;
#endif
    return ok;
}

// ---- the map ----

// A side of a map the editor makes: 512 to 4096 px in steps of 256 (the ground's tile).
constexpr int MIN_SIZE = 512, MAX_SIZE = 4096;
// Its area, in three bands (DEVELOPERS.md "How big a map can be": the editor keeps the ground's
// picture in memory several times over, so what runs out is the area). Up to LARGE_W x LARGE_H as a
// whole (the campaign's largest) nothing is said; over it, up to LIMIT_W x LIMIT_H, the dialogs and a
// load say it is a big map; over that File > New and a growing File > Map size refuse it, and a map
// that comes so big from elsewhere keeps the ground tools off.
constexpr int LARGE_W = 3072, LARGE_H = 2304;
constexpr int LIMIT_W = 4096, LIMIT_H = 3072;

bool valid_side(int w, int h) {
    return w % TILE == 0 && h % TILE == 0 && w >= MIN_SIZE && h >= MIN_SIZE && w <= MAX_SIZE && h <= MAX_SIZE;
}

// Whether a size grows the map `ow` x `oh` on a side (File > Map size; File > New grows from nothing).
bool grows(int w, int h, int ow, int oh) { return w > ow || h > oh; }

}  // namespace

// The area's band of w x h: 0 up to LARGE, 1 up to LIMIT, 2 over it. The test build's
// LOCOFIX_NO_BOUND=1 lifts the bound (2 is never given) for the tests that make bigger maps.
int map_area_band(int w, int h) {
    const long long area = static_cast<long long>(w) * h;
    if (area <= static_cast<long long>(LARGE_W) * LARGE_H) return 0;
#if LOCOFIX_SAVE_TEST
    static const bool lifted = [] {
        char value[8] = "";
        return GetEnvironmentVariableA("LOCOFIX_NO_BOUND", value, sizeof value) && value[0] == '1';
    }();
    if (lifted) return 1;
#endif
    return area <= static_cast<long long>(LIMIT_W) * LIMIT_H ? 1 : 2;
}

void map_area_limit(int& w, int& h) {
    w = LIMIT_W;
    h = LIMIT_H;
}

// A size the editor makes (File > New: `ow` = `oh` = 0) or resizes the map `ow` x `oh` to: the sides
// as valid_side, and the area within the bound - or, in File > Map size, no side growing (cropping a
// map that is over the bound is its way out).
bool map_size_allowed(int w, int h, int ow, int oh) {
    return valid_side(w, h) && (map_area_band(w, h) < 2 || !grows(w, h, ow, oh));
}

std::string size_error() {
    return ui_printf(ui_format("SizeError", "a map is 512 to 4096 px wide and high, in steps of 256, and no bigger than %d x %d as a whole"),
                     LIMIT_W, LIMIT_H);
}

std::string english_size_error() {   // for LocoFix.log
    const bool was = ui_english(true);
    const std::string e = size_error();
    ui_english(was);
    return e;
}

#if LOCOFIX_SAVE_TEST
// The test build's forced failures: LOCOFIX_FAIL=<place>[:<n>][,...] - the n-th time (1 by default)
// the editor comes to that place a big allocation fails there (std::bad_alloc, as a real one).
void memory_test_point(const char* place) {
    static std::vector<std::pair<std::string, int>> plan = [] {
        std::vector<std::pair<std::string, int>> p;
        char value[256] = "";
        if (!GetEnvironmentVariableA("LOCOFIX_FAIL", value, sizeof value)) return p;
        for (char* item = std::strtok(value, ","); item; item = std::strtok(nullptr, ",")) {
            char* colon = std::strchr(item, ':');
            int n = 1;
            if (colon) {
                *colon = 0;
                n = std::atoi(colon + 1);
            }
            p.push_back({item, n});
        }
        return p;
    }();
    for (auto& f : plan)
        if (f.first == place && f.second > 0 && --f.second == 0) {
            patch::log("save test: the allocation at %s fails (LOCOFIX_FAIL)", place);
            throw std::bad_alloc();
        }
}
#endif

// Puts a saved map and its ground image in place as one step, after both were written whole as
// temporary files next to them (`map_temp`, `image_temp`; no image: `image_temp` empty): the old
// image goes aside as <image>.old, the new one and the map take their names, the old image is
// deleted. When a step fails the steps before it are undone - the old map keeps the old image -
// and no temporary file is left. False (with the reason in the log) when nothing was replaced.
bool replace_map_files(const std::string& map_temp, const std::string& map, const std::string& image_temp, const std::string& image) {
    const DWORD flags = MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH;
    auto drop = [&] {
        DeleteFileA(map_temp.c_str());
        if (!image_temp.empty()) DeleteFileA(image_temp.c_str());
    };
    std::string aside;
    if (!image_temp.empty()) {
        if (GetFileAttributesA(image.c_str()) != INVALID_FILE_ATTRIBUTES) {
            aside = image + ".old";
            DeleteFileA(aside.c_str());
            if (!MoveFileExA(image.c_str(), aside.c_str(), flags)) {
                patch::log("save: %s could not be put aside (error %lu) - nothing replaced", image.c_str(), GetLastError());
                drop();
                return false;
            }
        }
        if (!MoveFileExA(image_temp.c_str(), image.c_str(), flags)) {
            patch::log("save: %s could not be replaced (error %lu) - nothing replaced", image.c_str(), GetLastError());
            if (!aside.empty()) MoveFileExA(aside.c_str(), image.c_str(), flags);
            drop();
            return false;
        }
    }
    bool map_moved = true;
#if LOCOFIX_SAVE_TEST
    try {
        memory_test_point("commit");   // a map that cannot be replaced once the image is in place
    } catch (const std::bad_alloc&) {
        map_moved = false;
        SetLastError(ERROR_ACCESS_DENIED);
    }
#endif
    if (!map_moved || !MoveFileExA(map_temp.c_str(), map.c_str(), flags)) {
        patch::log("save: %s could not be replaced (error %lu) - nothing replaced", map.c_str(), GetLastError());
        if (!image_temp.empty()) {
            DeleteFileA(image.c_str());
            if (!aside.empty() && !MoveFileExA(aside.c_str(), image.c_str(), flags))
                patch::log("save: %s could not be put back from %s (error %lu)", image.c_str(), aside.c_str(), GetLastError());
        }
        drop();
        return false;
    }
    if (!aside.empty()) DeleteFileA(aside.c_str());
    return true;
}

namespace {

// The image file of the ground's OBJ record and where it starts.
std::string ground_image(const std::string& record, size_t& start) {
    const size_t name_end = record.find('\0', 4);
    start = name_end == std::string::npos ? std::string::npos : name_end + 1 + OBJ_IMAGE_AFTER_NAME;
    if (start == std::string::npos || start >= record.size()) return "";
    return record.substr(start, record.find('\0', start) - start);
}

Chunk* chunk(std::vector<Chunk>& chunks, const char* id) {
    for (Chunk& c : chunks)
        if (c.id == id && c.records.size() >= 1) return &c;
    return nullptr;
}

// A map's size: HEAD's width and height (the camera kept inside), the GRID - each new cell the height
// of the old cell nearest to it, or `flat` - and, when `centre`, the ground object in the middle (the
// ground's picture is drawn centred on it).
void set_size(std::vector<Chunk>& chunks, int w, int h, bool centre, short flat = -1) {
    Chunk* head = chunk(chunks, "HEAD");
    Chunk* grid = chunk(chunks, "GRID");
    Chunk* spr = chunk(chunks, "SPR ");
    if (!head || head->records[0].size() < 16 || !grid) return;
    std::string& hr = head->records[0];
    float f[4];
    std::memcpy(f, &hr[0], 16);
    const int ow = static_cast<int>(f[0]), oh = static_cast<int>(f[1]);
    f[0] = static_cast<float>(w);
    f[1] = static_cast<float>(h);
    f[2] = (std::max)(0.0f, (std::min)(f[2], static_cast<float>(w - 1024)));
    f[3] = (std::max)(0.0f, (std::min)(f[3], static_cast<float>(h - 768)));
    std::memcpy(&hr[0], f, 16);
    const int ogw = ow / 8, ogh = oh / 8, gw = w / 8, gh = h / 8;
    const std::string old = grid->records[0];
    std::string cells(static_cast<size_t>(gw) * gh * 2, '\0');
    for (int y = 0; y < gh; ++y)
        for (int x = 0; x < gw; ++x) {
            short z = flat;
            if (flat < 0) {
                const size_t o = (static_cast<size_t>((std::min)(y, ogh - 1)) * ogw + (std::min)(x, ogw - 1)) * 2;
                if (o + 2 <= old.size()) std::memcpy(&z, &old[o], 2);
                else z = FLAT_LEVEL;
            }
            std::memcpy(&cells[(static_cast<size_t>(y) * gw + x) * 2], &z, 2);
        }
    grid->records[0] = cells;
    if (centre && spr)
        for (size_t at = 0; at + 28 <= spr->records[0].size() && static_cast<int32_t>(u32(spr->records[0], at)) != -1; at += 28)
            if (static_cast<int>(u32(spr->records[0], at + 4)) == GROUND_NVID) {
                const float xy[2] = {w / 2.0f, h / 2.0f};
                std::memcpy(&spr->records[0][at + 8], xy, 8);
                break;
            }
}

// The chunks of `source` with only the ground object; `image`, when given, becomes the ground's
// image (a new one of the size `w` x `h`, the GRID flat at FLAT_LEVEL).
bool new_map_chunks(const std::string& source, const std::string& image, int w, int h, std::vector<Chunk>& out,
                    std::string& error) {
    std::vector<Chunk> chunks;
    if (!read_res(read_file(source), "MAP ", chunks)) {
        error = ui_printf(ui_format("NewNotMap", "%s is not a map of the game"), source.c_str());
        return false;
    }
    int32_t ground_id = 0;
    for (Chunk& c : chunks) {
        if (c.id == "OBJ ") {   // the ground's VID only
            std::vector<std::string> kept;
            for (std::string& r : c.records)
                if (static_cast<int>(u32(r, 0)) == GROUND_NVID) {
                    if (!image.empty()) {
                        size_t start;
                        ground_image(r, start);
                        if (start == std::string::npos) continue;
                        r = r.substr(0, start) + image + '\0';
                    }
                    kept.push_back(r);
                }
            c.records = kept;
        } else if (c.id == "SPR " && c.records.size() == 1) {   // 28-byte entries until id -1
            const std::string& r = c.records[0];
            std::string kept;
            for (size_t at = 0; at + 28 <= r.size() && static_cast<int32_t>(u32(r, at)) != -1; at += 28)
                if (static_cast<int>(u32(r, at + 4)) == GROUND_NVID && !ground_id) {
                    ground_id = static_cast<int32_t>(u32(r, at));
                    kept = r.substr(at, 28);
                }
            put32(kept, 0xffffffffu);
            c.records = {kept};
        }
    }
    for (Chunk& c : chunks) {
        if (c.id == "SPRD") {
            std::vector<std::string> kept;
            for (const std::string& r : c.records)
                if (static_cast<int32_t>(u32(r, 0)) == ground_id) kept.push_back(r);
            std::string end;
            put32(end, 0xffffffffu);
            kept.push_back(end);
            c.records = kept;
        } else if (c.id == "PLAY") {
            c.records = {std::string(16, '\0')};
        } else if (c.id == "GROU") {
            std::string end;
            put32(end, 0xffffffffu);
            c.records = {end};
        }
    }
    if (!ground_id) {
        error = ui_printf(ui_format("NewNoGround", "%s has no ground object"), source.c_str());
        return false;
    }
    if (!image.empty()) set_size(chunks, w, h, true, FLAT_LEVEL);
    out = chunks;
    return true;
}

// Makes maps\<name>\: the map (a campaign terrain, or a meadow of w x h - or an empty map, a plain
// ground of the texture `sample`), the template's script and texts, titled `title` (the name in
// capitals by default). Returns the map's path, or empty with `error`.
std::string make_map(const std::string& name, bool generated, const Terrain& terrain, int w, int h, std::string& error,
                     const std::vector<uint16_t>* sample = nullptr, const std::string& title = "") {
    char number[8];
    std::snprintf(number, sizeof number, "l%02d", generated ? 1 : terrain.mission);
    const std::string folder = "maps\\" + name, map = folder + "\\" + name + ".map";
    const std::string source = std::string("maps\\") + number + "\\" + number + ".map";
    if (GetFileAttributesA(folder.c_str()) != INVALID_FILE_ATTRIBUTES) {
        error = ui_printf(ui_format("NewExists", "%s is there already"), folder.c_str());
        return "";
    }
    if (generated && !map_size_allowed(w, h, 0, 0)) {
        error = size_error();
        return "";
    }
    const std::string image = generated ? folder + "\\" + name + ".vid" : "";
    std::vector<Chunk> chunks;
    if (!new_map_chunks(source, image, w, h, chunks, error)) return "";
    CreateDirectoryA(folder.c_str(), nullptr);
    // the ground (a meadow, or one texture laid as a plain ground) written a tile at a time; out of
    // memory or not written, nothing of the new map stays
    bool written = !generated;
    try {
        if (generated) {
            memory_test_point("new");
            const uint32_t seed = GetTickCount();
            const bool textured = sample && sample->size() == 128 * 128;
            written = write_ground(
                image, w, h,
                [&](int px, int py, uint16_t& colour, uint16_t& height) {
                    colour = textured ? plain(*sample, px, py) : meadow(px, py, seed);
                    height = FLAT_PICTURE_HEIGHT;
                },
                WHOLE(ground_vid_whole(w, h, seed, nullptr, sample)));
            if (!written) error = ui_printf(ui_format("NewNotWritten", "%s could not be written"), image.c_str());
        }
        if (written && !write_file(map, write_res("MAP ", chunks))) {
            written = false;
            error = ui_printf(ui_format("NewNotWritten", "%s could not be written"), map.c_str());
        }
    } catch (const std::bad_alloc&) {
        written = false;
        error = ui_text("NewNoMemory", "Not enough memory to make the map: nothing was made.");
    }
    if (!written) {
        if (!image.empty()) DeleteFileA(image.c_str());
        DeleteFileA(map.c_str());
        RemoveDirectoryA(folder.c_str());
        return "";
    }
    // the template's mission under the new name (the editor writes its blocks anew on a save)
    const std::string upper = !title.empty() ? title : [&] {
        std::string u = name;
        for (char& c : u) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        return u;
    }();
    std::string script = read_file(std::string(TEMPLATE_FOLDER) + "template.lgc");
    script = replace_all(script, "maps\\\\template\\\\template.map", "maps\\\\" + name + "\\\\" + name + ".map");
    script = replace_all(script, "maps\\\\template\\\\", "maps\\\\" + name + "\\\\");
    script = replace_all(script, "// title: TEMPLATE", "// title: " + upper);
    script = replace_all(script, "#define BRIEF_HEADER \"TEMPLATE\"", "#define BRIEF_HEADER \"" + upper + "\"");
    write_file(folder + "\\" + name + ".lgc", script);
    for (const char* text : {"objectives.txt", "win.txt", "loose.txt", "brief.txt"})
        write_file(folder + "\\" + text, read_file(language_file(std::string(TEMPLATE_FOLDER) + text)));   // the pack's texts
    write_file(folder + "\\brief.lst", folder + "\\brief.txt");
    patch::log("new map: %s from %s%s (%dx%d)", map.c_str(), source.c_str(),
               sample ? ", an empty ground of one texture" : generated ? ", a generated ground" : "",
               generated ? w : terrain.width, generated ? h : terrain.height);
    return map;
}

bool valid_name(const std::string& name) { return map_name_problem(name).empty(); }   // custom_maps.cpp's rules

// ---- resizing ----

// Where the ground object stands (SPR); false when the map has none.
bool ground_place(std::vector<Chunk>& chunks, float& x, float& y) {
    Chunk* spr = chunk(chunks, "SPR ");
    if (!spr) return false;
    const std::string& r = spr->records[0];
    for (size_t at = 0; at + 28 <= r.size() && static_cast<int32_t>(u32(r, at)) != -1; at += 28)
        if (static_cast<int>(u32(r, at + 4)) == GROUND_NVID) {
            std::memcpy(&x, &r[at + 8], 4);
            std::memcpy(&y, &r[at + 12], 4);
            return true;
        }
    return false;
}

// Where the top-left corner of a ground picture of w x h drawn centred on (gx, gy) lies on the map.
void picture_corner(float gx, float gy, int w, int h, int& ox, int& oy) {
    ox = static_cast<int>(std::lround(gx - w / 2.0f));
    oy = static_cast<int>(std::lround(gy - h / 2.0f));
}

#if LOCOFIX_SAVE_TEST
// The part w x h of the map from its top-left corner of a ground picture drawn centred on (gx, gy),
// as a picture of its own; where the old one does not reach, a meadow at the flat ground's height
// (the test build's whole way; resize_map_file writes the same pixels a tile at a time).
Picture picture_on_map(const Picture& old, float gx, float gy, int w, int h, uint32_t seed) {
    const int ox = static_cast<int>(std::lround(gx - old.width / 2.0f)), oy = static_cast<int>(std::lround(gy - old.height / 2.0f));
    Picture p;
    p.width = w;
    p.height = h;
    p.pixels.resize(static_cast<size_t>(w) * h);
    p.heights.resize(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int sx = x - ox, sy = y - oy;
            const size_t to = static_cast<size_t>(y) * w + x;
            if (sx >= 0 && sy >= 0 && sx < old.width && sy < old.height) {
                p.pixels[to] = old.pixels[static_cast<size_t>(sy) * old.width + sx];
                p.heights[to] = old.heights[static_cast<size_t>(sy) * old.width + sx];
            } else {
                p.pixels[to] = meadow(x, y, seed);
                p.heights[to] = FLAT_PICTURE_HEIGHT;
            }
        }
    return p;
}
#endif

// The map file `map` (saved just before) at the size w x h: the right and the bottom edge move. The
// map gets a ground image of its own, <map>.vid, of exactly its new size: the old picture (its own or
// the campaign's) where it lay on the map - grown, a meadow where it did not reach; cropped, cut at
// the new edge, so nothing of the old ground is left past it - with the ground object in the middle.
bool resize_map_file(const std::string& map, int w, int h, std::string& error) {
    memory_test_point("resize");
    std::vector<Chunk> chunks;
    if (!read_res(read_file(map), "MAP ", chunks)) {
        error = ui_printf(ui_format("SizeNotRead", "%s cannot be read"), map.c_str());
        return false;
    }
    Chunk* head = chunk(chunks, "HEAD");
    Chunk* obj = chunk(chunks, "OBJ ");
    if (!head || head->records[0].size() < 16 || !obj) {
        error = ui_printf(ui_format("SizeNoHead", "%s has no HEAD or OBJ"), map.c_str());
        return false;
    }
    float f[2];
    std::memcpy(f, &head->records[0][0], 8);
    const int ow = static_cast<int>(f[0]), oh = static_cast<int>(f[1]);
    if (!map_size_allowed(w, h, ow, oh)) {
        error = size_error();
        return false;
    }
    // the new image and the map written as temporary files first, put in place together
    const std::string map_temp = map + ".saving";
    std::string image, image_temp;
    if (w != ow || h != oh) {
        std::string* record = nullptr;
        for (std::string& r : obj->records)
            if (static_cast<int>(u32(r, 0)) == GROUND_NVID) record = &r;
        size_t start = std::string::npos;
        const std::string old_image = record ? ground_image(*record, start) : "";
        Picture old;
        float gx = ow / 2.0f, gy = oh / 2.0f;
        ground_place(chunks, gx, gy);
        if (!record || old_image.empty() || !read_ground(old_image, old)) {
            error = ui_printf(ui_format("SizeNoImage", "the ground's image %s cannot be read"), old_image.c_str());
            return false;
        }
        const size_t dot = map.find_last_of('.');
        image = map.substr(0, dot) + ".vid";
        image_temp = image + ".saving";
        const uint32_t seed = GetTickCount();
        int ox, oy;
        picture_corner(gx, gy, old.width, old.height, ox, oy);
        const bool written = write_ground(
            image_temp, w, h,
            [&](int px, int py, uint16_t& colour, uint16_t& height) {
                const int sx = px - ox, sy = py - oy;
                if (px < w && py < h && sx >= 0 && sy >= 0 && sx < old.width && sy < old.height) {
                    colour = old.pixels[static_cast<size_t>(sy) * old.width + sx];
                    height = old.heights[static_cast<size_t>(sy) * old.width + sx];
                } else {
                    colour = meadow(px, py, seed);
                    height = FLAT_PICTURE_HEIGHT;
                }
            },
            WHOLE([&] {
                const Picture cut = picture_on_map(old, gx, gy, w, h, seed);
                return ground_vid_whole(w, h, seed, &cut);
            }()));
        if (!written) {
            error = ui_printf(ui_format("NewNotWritten", "%s could not be written"), image.c_str());
            return false;
        }
        *record = record->substr(0, start) + image + '\0';
    }
    set_size(chunks, w, h, true);
    if (!write_file(map_temp, write_res("MAP ", chunks))) {
        DeleteFileA(map_temp.c_str());
        if (!image_temp.empty()) DeleteFileA(image_temp.c_str());
        error = ui_printf(ui_format("NewNotWritten", "%s could not be written"), map.c_str());
        return false;
    }
    if (!replace_map_files(map_temp, map, image_temp, image)) {
        error = ui_printf(ui_format("NewNotWritten", "%s could not be written"), map.c_str());
        return false;
    }
    patch::log("map size: %s %dx%d -> %dx%d, a ground image of its own of that size", map.c_str(), ow, oh, w, h);
    return true;
}

}  // namespace

// ---- for the ground painting (editor_ground.cpp) ----

// The image file of the ground of the map file `map` ("" when it has none).
std::string map_ground_image(const std::string& map) {
    std::vector<Chunk> chunks;
    if (!read_res(read_file(map), "MAP ", chunks)) return "";
    if (Chunk* obj = chunk(chunks, "OBJ "))
        for (const std::string& r : obj->records)
            if (static_cast<int>(u32(r, 0)) == GROUND_NVID) {
                size_t start;
                return ground_image(r, start);
            }
    return "";
}

// A ground image's picture: its colours (R5G6B5) and heights, row by row.
bool read_ground_picture(const std::string& image, int& w, int& h, std::vector<uint16_t>& pixels,
                         std::vector<uint16_t>& heights) {
    Picture p;
    if (!read_ground(image, p)) return false;
    w = p.width;
    h = p.height;
    pixels.swap(p.pixels);
    heights.swap(p.heights);
    return true;
}

// A frame of a sprite image (DEVELOPERS.md "Data formats": RLE rows of palette indices, each run's
// heights first with imageFlags 4) as ARGB, 0 where it is transparent; w x h is the image's cell.
bool read_sprite_frame(const std::string& image, int frame, int& w, int& h, std::vector<uint32_t>& argb) {
    std::vector<Chunk> chunks;
    if (!read_res(read_file(image), "VID ", chunks)) return false;
    const std::vector<std::string>* frames = nullptr;
    std::string head, palette;
    for (const Chunk& c : chunks) {
        if (c.id == "HEAD" && !c.records.empty()) head = c.records[0];
        else if (c.id == "PAL " && !c.records.empty()) palette = c.records[0];
        else if (c.id == "DATA") frames = &c.records;
    }
    if (head.size() < 10 || !frames || (palette.size() != 768 && palette.size() != 1024)) return false;
    uint16_t flags, cw, ch;
    std::memcpy(&flags, &head[0], 2);
    std::memcpy(&cw, &head[6], 2);
    std::memcpy(&ch, &head[8], 2);
    if ((flags & 0xa0) || !(flags & 8)) return false;   // a sprite with a palette
    const size_t depth = (flags & 4) ? 2 : 0;
    for (int guard = 0; guard < 4 && frame >= 0 && frame < static_cast<int>(frames->size()); ++guard) {
        const std::string& r = (*frames)[frame];
        if (r.size() == 6 && u32(r, 0) == 2) {   // a reference to another frame
            int16_t to;
            std::memcpy(&to, &r[4], 2);
            frame = to;
            continue;
        }
        if (r.size() < 6 || u32(r, 0) != r.size() - 4) return false;
        int16_t n, top, rows;
        std::memcpy(&n, &r[4], 2);
        size_t o = 6 + 6 * static_cast<size_t>((std::max)(static_cast<int16_t>(0), n));
        if (o + 4 > r.size()) return false;
        std::memcpy(&top, &r[o], 2);
        std::memcpy(&rows, &r[o + 2], 2);
        o += 4;
        w = cw;
        h = ch;
        argb.assign(static_cast<size_t>(w) * h, 0);
        for (int y = 0; y < rows; ++y)
            for (int x = 0;;) {
                if (o + 2 > r.size()) return false;
                const int skip = static_cast<uint8_t>(r[o]), count = static_cast<uint8_t>(r[o + 1]);
                o += 2;
                if (!skip && !count) break;
                x += skip;
                o += depth * count;
                if (o + count > r.size()) return false;
                for (int k = 0; k < count; ++k, ++x) {
                    const int i = static_cast<uint8_t>(r[o + k]), py = top + y;
                    if (x >= w || py < 0 || py >= h) continue;
                    uint32_t rgb;
                    if (palette.size() == 1024)   // B, G, R, A
                        rgb = (static_cast<uint8_t>(palette[4 * i + 2]) << 16) | (static_cast<uint8_t>(palette[4 * i + 1]) << 8) |
                              static_cast<uint8_t>(palette[4 * i]);
                    else
                        rgb = (static_cast<uint8_t>(palette[3 * i]) << 16) | (static_cast<uint8_t>(palette[3 * i + 1]) << 8) |
                              static_cast<uint8_t>(palette[3 * i + 2]);
                    argb[static_cast<size_t>(py) * w + x] = 0xff000000 | rgb;
                }
                o += count;
            }
        return true;
    }
    return false;
}

// Writes a picture as a ground image of its own (the layout of File > New's grounds).
// Written a tile at a time straight from `pixels` and `heights` (the whole ground_vid_whole's bytes:
// the size cut to whole tiles, every pixel of it inside the picture).
bool write_ground_picture(const std::string& image, int w, int h, const std::vector<uint16_t>& pixels,
                          const std::vector<uint16_t>& heights) {
    if (w <= 0 || h <= 0 || pixels.size() < static_cast<size_t>(w) * h || heights.size() < static_cast<size_t>(w) * h) return false;
    return write_ground(
        image, w - w % TILE, h - h % TILE,
        [&](int px, int py, uint16_t& colour, uint16_t& height) {
            const size_t i = static_cast<size_t>(py) * w + px;
            colour = pixels[i];
            height = heights[i];
        },
        WHOLE([&] {
            Picture p;
            p.width = w;
            p.height = h;
            p.pixels = pixels;
            p.heights = heights;
            return ground_vid_whole(w - w % TILE, h - h % TILE, 0, &p);
        }()));
}

// The size of a ground image's picture, read from its HEAD alone.
bool ground_image_size(const std::string& image, int& w, int& h) { return ground_size(image, w, h); }

// The same for a map file's bytes (editor_exchange.cpp).
std::string map_ground_image_of(const std::string& map_bytes) {
    std::vector<Chunk> chunks;
    if (!read_res(map_bytes, "MAP ", chunks)) return "";
    if (Chunk* obj = chunk(chunks, "OBJ "))
        for (const std::string& r : obj->records)
            if (static_cast<int>(u32(r, 0)) == GROUND_NVID) {
                size_t start;
                return ground_image(r, start);
            }
    return "";
}

// A map file's bytes with its ground pointing at `image` (the bytes as they were when it has none).
std::string map_with_ground_image(const std::string& map_bytes, const std::string& image) {
    std::vector<Chunk> chunks;
    if (!read_res(map_bytes, "MAP ", chunks)) return map_bytes;
    Chunk* obj = chunk(chunks, "OBJ ");
    if (!obj) return map_bytes;
    for (std::string& r : obj->records)
        if (static_cast<int>(u32(r, 0)) == GROUND_NVID) {
            size_t start;
            ground_image(r, start);
            if (start == std::string::npos) return map_bytes;
            r = r.substr(0, start) + image + '\0';
        }
    return write_res("MAP ", chunks);
}

// Points the ground of the map file `map` at `image`.
bool set_map_ground_image(const std::string& map, const std::string& image) {
    std::vector<Chunk> chunks;
    if (!read_res(read_file(map), "MAP ", chunks)) return false;
    Chunk* obj = chunk(chunks, "OBJ ");
    if (!obj) return false;
    bool done = false;
    for (std::string& r : obj->records)
        if (static_cast<int>(u32(r, 0)) == GROUND_NVID) {
            size_t start;
            ground_image(r, start);
            if (start == std::string::npos) return false;
            r = r.substr(0, start) + image + '\0';
            done = true;
        }
    return done && write_file(map, write_res("MAP ", chunks));
}

#if LOCOFIX_SAVE_TEST
// The test build's c40090: every ground image of maps\*\ (the campaign's and every custom map's) read
// both ways and compared, and its picture written again both ways (into maps\_vidcheck.tmp, deleted).
void vid_check_all() {
    g_force_compare = true;
    WIN32_FIND_DATAA folder;
    HANDLE f = FindFirstFileA("maps\\*", &folder);
    int images = 0;
    for (BOOL more = f != INVALID_HANDLE_VALUE; more; more = FindNextFileA(f, &folder)) {
        if (!(folder.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || folder.cFileName[0] == '.') continue;
        WIN32_FIND_DATAA file;
        const std::string dir = std::string("maps\\") + folder.cFileName + "\\";
        HANDLE g = FindFirstFileA((dir + "*.vid").c_str(), &file);
        for (BOOL m = g != INVALID_HANDLE_VALUE; m; m = FindNextFileA(g, &file)) {
            const std::string image = dir + file.cFileName;
            Picture p;
            ++images;
            if (!read_ground(image, p)) continue;   // compared too: both refuse it
            const std::string out = "maps\\_vidcheck.tmp";
            std::vector<uint16_t> pixels = p.pixels, heights = p.heights;
            write_ground_picture(out, p.width, p.height, pixels, heights);
            // an image the editor wrote (any version: the layout of File > New's grounds) comes out
            // as the very file it was read from
            const bool same = read_file(out) == read_file(image);
            patch::log("vid check: %s written again %s its file", image.c_str(), same ? "= byte for byte" : "differs from (another layout)");
            DeleteFileA(out.c_str());
        }
        if (g != INVALID_HANDLE_VALUE) FindClose(g);
    }
    if (f != INVALID_HANDLE_VALUE) FindClose(f);
    g_force_compare = false;
    patch::log("vid check: %d images", images);
}
#endif

namespace {

// ---- the dialogs ----
//
// File > New offers the ground in one list: an empty map - flat, nothing on it, the ground one
// texture of the painting's (editor\textures.tsv: grass, sand, snow, rock ...) -, the meadow drawn
// here, then the terrains of the 20 missions. The size (of an empty map or a meadow; a mission's
// terrain has its own) is set with - and + (256 px a step, 512 to 4096: the ground's tiles are
// 256 x 256 and the engine's GRID and camera handle up to 4096) or a preset button, the result shown
// in pixels and in tiles, and a note under the presets gives the area's band: nothing, a big map, or
// too big as a whole (OK refuses it). File > Map size has the same row; it crops a map whatever its
// area and grows one only within the bound.

enum : int {
    IDC_NAME = 100, IDC_GROUND, IDC_WIDTH, IDC_HEIGHT, IDC_W_MINUS, IDC_W_PLUS, IDC_H_MINUS, IDC_H_PLUS, IDC_SIZE_INFO,
    IDC_SIZE_NOTE,   // the line under the presets: a big map, or one too big as a whole
    IDC_PRESET = 120   // IDC_PRESET + i: the presets
};
constexpr int CAMPAIGN_COUNT = static_cast<int>(sizeof CAMPAIGN / sizeof CAMPAIGN[0]);
constexpr int MEADOW = CAMPAIGN_COUNT;      // Choice.ground: 0-19 a mission's terrain, 20 the meadow,
constexpr int EMPTY = CAMPAIGN_COUNT + 1;   // 21 + k an empty map of texture k
struct Preset {
    int w, h;
};
const Preset PRESETS[] = {{1024, 768}, {2048, 1536}, {3072, 2304}, {1536, 1024}, {4096, 3072}};
constexpr int PRESET_COUNT = static_cast<int>(sizeof PRESETS / sizeof PRESETS[0]);

struct Choice {
    std::string name;
    int ground = EMPTY;
    int width = 2048, height = 1536;
};

int clamp_size(int v) { return (std::max)(MIN_SIZE, (std::min)(MAX_SIZE, (v + TILE / 2) / TILE * TILE)); }

// The size the open dialog starts from: File > Map size's map (a crop of it is allowed whatever its
// area), 0 x 0 in File > New; and whether File > New's ground has the size row (an empty map, a meadow).
int g_from_w = 0, g_from_h = 0;
bool g_size_row = true;

// SizeBad's box: what a size may be.
void size_refused(HWND dialog) {
    MessageBoxW(dialog,
                ui_wide(ui_printf(ui_format("SizeBad", "The size: 512 to 4096 px wide and high, in steps of 256, and as a whole no bigger "
                                                       "than %d x %d. The campaign's maps are 1024 x 768, 2048 x 1536 and 3072 x 2304."),
                                  LIMIT_W, LIMIT_H))
                    .c_str(),
                ui_wtext("ResizeTitle", "Map size").c_str(), MB_OK | MB_ICONWARNING);
}

// The line under the size: "= 2048 x 1536 px, 8 x 6 tiles of 256 px" (or why it is not a size).
void show_size(HWND dialog) {
    BOOL ok1 = FALSE, ok2 = FALSE;
    const int w = static_cast<int>(GetDlgItemInt(dialog, IDC_WIDTH, &ok1, FALSE));
    const int h = static_cast<int>(GetDlgItemInt(dialog, IDC_HEIGHT, &ok2, FALSE));
    const std::wstring text =
        ok1 && ok2 && valid_side(w, h)
            ? ui_wide(ui_printf(ui_format("SizeInfo", "= %d x %d px, %d x %d tiles of 256 px"), w, h, w / TILE, h / TILE))
            : ui_wtext("SizeInfoBad", "512 to 4096 px, in steps of 256");
    SetDlgItemTextW(dialog, IDC_SIZE_INFO, text.c_str());
    // the line under the presets, by the area's band: nothing, a big map, too big (OK refuses it); a
    // crop of a map that is over the bound is allowed and says its ground cannot be painted
    std::wstring note;
    if (g_size_row && ok1 && ok2 && valid_side(w, h)) {
        const int band = map_area_band(w, h);
        if (band == 2 && !grows(w, h, g_from_w, g_from_h)) {
            note = ui_wide(ui_printf(ui_format("MsgGroundTooBig", "the ground of a map this big cannot be painted (at most %d x %d px as a whole)"),
                                     LIMIT_W, LIMIT_H));
            if (!note.empty()) CharUpperBuffW(&note[0], 1);
        } else if (band == 2) {
            note = ui_wide(ui_printf(ui_format("SizeNoteOver", "Too big as a whole: at most %d x %d px in area (a wider map must be lower)."),
                                     LIMIT_W, LIMIT_H));
        } else if (band == 1) {
            note = ui_wtext("SizeNoteLarge", "A big map: painting its ground uses a lot of memory. Save often.");
        }
    }
    SetDlgItemTextW(dialog, IDC_SIZE_NOTE, note.c_str());
}

// The commands of the size row: - and + (a step of 256 within the limits), the presets.
bool size_command(HWND dialog, int id, int code) {
    if ((id == IDC_WIDTH || id == IDC_HEIGHT) && code == EN_CHANGE) {
        show_size(dialog);
        return true;
    }
    if (id >= IDC_W_MINUS && id <= IDC_H_PLUS) {
        const int field = id <= IDC_W_PLUS ? IDC_WIDTH : IDC_HEIGHT, step = (id == IDC_W_PLUS || id == IDC_H_PLUS) ? TILE : -TILE;
        BOOL ok = FALSE;
        const int v = static_cast<int>(GetDlgItemInt(dialog, field, &ok, FALSE));
        SetDlgItemInt(dialog, field, clamp_size((ok ? v : MIN_SIZE) + step), FALSE);
        patch::log("size: %s %d -> %d", field == IDC_WIDTH ? "width" : "height", v, clamp_size((ok ? v : MIN_SIZE) + step));
        return true;
    }
    if (id >= IDC_PRESET && id < IDC_PRESET + PRESET_COUNT) {
        SetDlgItemInt(dialog, IDC_WIDTH, PRESETS[id - IDC_PRESET].w, FALSE);
        SetDlgItemInt(dialog, IDC_HEIGHT, PRESETS[id - IDC_PRESET].h, FALSE);
        return true;
    }
    return false;
}

// The size row at `y`: width - + x height - +, the line with the result, the presets below and the
// note under them (42 units in all).
void size_items(dialog::DialogTemplate& t, short x, short y) {
    using namespace dialog;
    t.item(EDIT, L"", IDC_WIDTH, LINE | ES_NUMBER, x, y, 34, 12);
    t.item(BUTTON, L"-", IDC_W_MINUS, BS_PUSHBUTTON | WS_TABSTOP, static_cast<short>(x + 36), y, 12, 12);
    t.item(BUTTON, L"+", IDC_W_PLUS, BS_PUSHBUTTON | WS_TABSTOP, static_cast<short>(x + 50), y, 12, 12);
    t.item(STATIC, L"x", -1, SS_CENTER, static_cast<short>(x + 64), static_cast<short>(y + 2), 8, 8);
    t.item(EDIT, L"", IDC_HEIGHT, LINE | ES_NUMBER, static_cast<short>(x + 74), y, 34, 12);
    t.item(BUTTON, L"-", IDC_H_MINUS, BS_PUSHBUTTON | WS_TABSTOP, static_cast<short>(x + 110), y, 12, 12);
    t.item(BUTTON, L"+", IDC_H_PLUS, BS_PUSHBUTTON | WS_TABSTOP, static_cast<short>(x + 124), y, 12, 12);
    t.item(STATIC, L"", IDC_SIZE_INFO, SS_LEFT, static_cast<short>(x + 142), static_cast<short>(y + 2), 140, 8);
    for (int i = 0; i < PRESET_COUNT; ++i) {
        wchar_t label[24];
        std::swprintf(label, 24, L"%d x %d", PRESETS[i].w, PRESETS[i].h);
        t.item(BUTTON, label, IDC_PRESET + i, BS_PUSHBUTTON | WS_TABSTOP, static_cast<short>(x + i * 56), static_cast<short>(y + 16), 52,
               12);
    }
    t.item(STATIC, L"", IDC_SIZE_NOTE, SS_LEFT | SS_NOPREFIX, x, static_cast<short>(y + 31), 284, 8);
}

bool read_size(HWND dialog, int& w, int& h) {
    BOOL ok1 = FALSE, ok2 = FALSE;
    w = static_cast<int>(GetDlgItemInt(dialog, IDC_WIDTH, &ok1, FALSE));
    h = static_cast<int>(GetDlgItemInt(dialog, IDC_HEIGHT, &ok2, FALSE));
    if (ok1 && ok2 && map_size_allowed(w, h, g_from_w, g_from_h)) return true;
    patch::log("size: %d x %d refused - %s", w, h, english_size_error().c_str());
    size_refused(dialog);
    return false;
}

// The size row is for an empty map and the meadow; a mission's terrain has its size.
void enable_size(HWND dialog, bool on) {
    for (int id = IDC_WIDTH; id <= IDC_SIZE_NOTE; ++id)
        if (HWND item = GetDlgItem(dialog, id)) EnableWindow(item, on);
    for (int i = 0; i < PRESET_COUNT; ++i)
        if (HWND item = GetDlgItem(dialog, IDC_PRESET + i)) EnableWindow(item, on);
}

int chosen_ground(HWND dialog) {
    const LRESULT i = SendDlgItemMessageW(dialog, IDC_GROUND, CB_GETCURSEL, 0, 0);
    return i == CB_ERR ? EMPTY : static_cast<int>(SendDlgItemMessageW(dialog, IDC_GROUND, CB_GETITEMDATA, i, 0));
}

void add_ground(HWND dialog, const std::wstring& text, int ground) {
    const LRESULT i = SendDlgItemMessageW(dialog, IDC_GROUND, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
    SendDlgItemMessageW(dialog, IDC_GROUND, CB_SETITEMDATA, i, ground);
}

INT_PTR CALLBACK new_map_proc(HWND dialog, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) {
        SetWindowLongPtrW(dialog, DWLP_USER, l);
        SetDlgItemTextW(dialog, IDC_NAME, L"mymap");
        const std::vector<GroundTexture>& textures = ground_textures();
        for (size_t k = 0; k < textures.size(); ++k)
            if (textures[k].available)
                add_ground(dialog,
                           ui_wide(ui_printf(ui_format("NewEmpty", "an empty map: %s"), ui_text(textures[k].key.c_str(), textures[k].english.c_str()))),
                           EMPTY + static_cast<int>(k));
        add_ground(dialog, ui_wtext("NewMeadow", "a meadow of its own (the size below)"), MEADOW);
        for (int i = 0; i < CAMPAIGN_COUNT; ++i)
            add_ground(dialog,
                       ui_wide(ui_printf(ui_format("NewMission", "the terrain of mission %d (%d x %d)"), CAMPAIGN[i].mission, CAMPAIGN[i].width,
                                         CAMPAIGN[i].height)),
                       i);
        SendDlgItemMessageW(dialog, IDC_GROUND, CB_SETCURSEL, 0, 0);
        SetDlgItemInt(dialog, IDC_WIDTH, 2048, FALSE);
        SetDlgItemInt(dialog, IDC_HEIGHT, 1536, FALSE);
        show_size(dialog);
        return TRUE;
    }
    if (m != WM_COMMAND) return FALSE;
    if (LOWORD(w) == IDC_GROUND && HIWORD(w) == CBN_SELCHANGE) {
        g_size_row = chosen_ground(dialog) >= MEADOW;
        enable_size(dialog, g_size_row);
        show_size(dialog);   // a mission's terrain has its own size: no note
        return TRUE;
    }
    if (size_command(dialog, LOWORD(w), HIWORD(w))) return TRUE;
    if (LOWORD(w) == IDCANCEL) {
        EndDialog(dialog, 0);
        return TRUE;
    }
    if (LOWORD(w) != IDOK) return FALSE;
    auto* choice = reinterpret_cast<Choice*>(GetWindowLongPtrW(dialog, DWLP_USER));
    char name[MAX_PATH] = "";
    GetDlgItemTextA(dialog, IDC_NAME, name, sizeof name);
    if (!valid_name(name)) {
        MessageBoxW(dialog, ui_wide(map_name_problem(name) + "\n\n" + map_name_rule()).c_str(), ui_wtext("NewTitle", "New map").c_str(),
                    MB_OK | MB_ICONWARNING);
        return TRUE;
    }
    choice->name = name;
    choice->ground = chosen_ground(dialog);
    if (choice->ground >= MEADOW && !read_size(dialog, choice->width, choice->height)) return TRUE;
    EndDialog(dialog, 1);
    return TRUE;
}

void create(const Choice& c, HWND owner) {
    const bool generated = c.ground >= MEADOW;
    const Terrain& t = CAMPAIGN[(std::max)(0, (std::min)(c.ground, CAMPAIGN_COUNT - 1))];
    std::vector<uint16_t> sample;
    if (c.ground >= EMPTY && !ground_texture_sample(c.ground - EMPTY, sample)) sample.clear();
    std::string error;
    const std::string map = make_map(c.name, generated, t, c.width, c.height, error, sample.empty() ? nullptr : &sample);
    if (map.empty()) {
        patch::log("new map: %s", error.c_str());
        if (owner) ui_message_box(owner, error.c_str(), ui_text("NewTitle", "New map"), MB_OK | MB_ICONWARNING);
        return;
    }
    editor_load_map(map);
}

INT_PTR CALLBACK size_proc(HWND dialog, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_INITDIALOG) {
        SetWindowLongPtrW(dialog, DWLP_USER, l);
        const int* size = reinterpret_cast<int*>(l);
        SetDlgItemInt(dialog, IDC_WIDTH, size[0], FALSE);
        SetDlgItemInt(dialog, IDC_HEIGHT, size[1], FALSE);
        show_size(dialog);
        return TRUE;
    }
    if (m != WM_COMMAND) return FALSE;
    if (size_command(dialog, LOWORD(w), HIWORD(w))) return TRUE;
    if (LOWORD(w) == IDCANCEL) {
        EndDialog(dialog, 0);
        return TRUE;
    }
    if (LOWORD(w) != IDOK) return FALSE;
    int* size = reinterpret_cast<int*>(GetWindowLongPtrW(dialog, DWLP_USER));
    if (!read_size(dialog, size[0], size[1])) return TRUE;
    EndDialog(dialog, 1);
    return TRUE;
}

}  // namespace

// File > New... (editor.cpp runs it as a dialog of the game; the Menu panel's New map too).
void new_map_dialog(HWND owner) {
    using namespace dialog;
    DialogTemplate t(ui_wtext("NewTitle", "New map").c_str(), 300, 152);
    t.item(STATIC, ui_wtext("NewFolder", "Name (its folder in maps\\):").c_str(), -1, SS_LEFT, 8, 10, 150, 8);
    t.item(EDIT, L"", IDC_NAME, LINE, 160, 8, 132, 12);
    t.item(STATIC, ui_wtext("NewGround", "Ground:").c_str(), -1, SS_LEFT, 8, 30, 40, 8);
    t.item(COMBOBOX, L"", IDC_GROUND, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 50, 28, 242, 200);
    t.item(STATIC, ui_wtext("NewSize", "Size (an empty map, a meadow):").c_str(), -1, SS_LEFT, 8, 48, 284, 8);
    size_items(t, 8, 60);
    t.item(STATIC, ui_wtext("NewGroundNote", "An empty map is flat and bare, one texture to paint over; a mission's terrain stays the game's own picture.").c_str(),
           -1, SS_LEFT, 8, 106, 284, 16);
    t.item(BUTTON, ui_wtext("ButtonOk", "OK").c_str(), IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP, 184, 130, 50, 14);
    t.item(BUTTON, ui_wtext("ButtonCancel", "Cancel").c_str(), IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 242, 130, 50, 14);
    g_from_w = g_from_h = 0;   // a new map grows from nothing: the bound holds
    g_size_row = true;         // the first ground is an empty map
    Choice c;
    if (DialogBoxIndirectParamW(GetModuleHandleW(nullptr), t.get(), owner, new_map_proc, reinterpret_cast<LPARAM>(&c)) == 1)
        create(c, owner);
}

// The map the editor starts on (editor.cpp): what File > New makes with "an empty map: Grass" and
// 2048 x 1536, in the scratch folder maps\_new\ (_new2, _new3 ... while an older one is still there),
// titled with editor_new_map_title(). Returns its path (empty when it could not be made: the reason
// is in LocoFix.log).
std::string make_start_map() {
    std::vector<uint16_t> sample;
    const std::vector<GroundTexture>& textures = ground_textures();
    for (size_t k = 0; k < textures.size() && sample.empty(); ++k)
        if (textures[k].key == "TexGrass") ground_texture_sample(static_cast<int>(k), sample);
    for (int n = 1; n < 100; ++n) {
        const std::string name = n == 1 ? "_new" : "_new" + std::to_string(n);
        if (GetFileAttributesA(("maps\\" + name).c_str()) != INVALID_FILE_ATTRIBUTES) continue;
        std::string error;
        const std::string map = make_map(name, true, CAMPAIGN[0], 2048, 1536, error, sample.empty() ? nullptr : &sample,
                                         editor_new_map_title());
        if (map.empty()) patch::log("new map: %s", error.c_str());
        return map;
    }
    return "";
}

// Test mode (editor.cpp's WM_COPYDATA 40043): "<name> <ground> [<width> <height>]" without the dialog
// - ground 0-19 a mission's terrain, 20 the meadow, 21 + k an empty map of texture k.
bool new_map_for_test(const std::string& text) {
    char name[64] = "";
    int ground = 0, w = 2048, h = 1536;
    if (std::sscanf(text.c_str(), "%63s %d %d %d", name, &ground, &w, &h) < 2 || !valid_name(name)) return false;
    Choice c;
    c.name = name;
    c.ground = ground;
    c.width = w;
    c.height = h;
    const bool english = ui_english(true);   // LocoFix.log in English
    create(c, nullptr);
    ui_english(english);
    return true;
}

// File > Map size... (editor.cpp): the new size asked for; false when cancelled.
bool map_size_dialog(HWND owner, int& w, int& h) {
    using namespace dialog;
    DialogTemplate t(ui_wtext("ResizeTitle", "Map size").c_str(), 300, 98);
    t.item(STATIC, ui_wtext("SizeLabel", "The map's size (px, steps of 256; the right and the bottom edge move):").c_str(), -1, SS_LEFT,
           8, 8, 284, 16);
    size_items(t, 8, 28);
    t.item(BUTTON, ui_wtext("ButtonOk", "OK").c_str(), IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP, 184, 76, 50, 14);
    t.item(BUTTON, ui_wtext("ButtonCancel", "Cancel").c_str(), IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 242, 76, 50, 14);
    g_from_w = w;   // cropping the map is allowed whatever its area, growing it only within the bound
    g_from_h = h;
    g_size_row = true;
    int size[2] = {w, h};
    if (DialogBoxIndirectParamW(GetModuleHandleW(nullptr), t.get(), owner, size_proc, reinterpret_cast<LPARAM>(size)) != 1)
        return false;
    w = size[0];
    h = size[1];
    return true;
}

// The resize of the saved map file (editor.cpp saves before and loads it again after).
// Out of memory nothing is replaced (the map stays as saved before) and `error` says so.
bool resize_map(const std::string& map, int w, int h, std::string& error) {
    if (!valid_side(w, h)) {
        error = size_error();
        return false;
    }
    try {
        return resize_map_file(map, w, h, error);
    } catch (const std::bad_alloc&) {
        DeleteFileA((map + ".saving").c_str());
        DeleteFileA((map.substr(0, map.find_last_of('.')) + ".vid.saving").c_str());
        patch::log("map size: %s - out of memory, nothing resized", map.c_str());
        error = ui_text("ResizeNoMemory", "Not enough memory to resize the map. It stays saved at its old size, without the objects that were "
                                          "outside the new one; its picture is as it was.");
        return false;
    }
}