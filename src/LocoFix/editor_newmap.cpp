// File > New... and File > Map size... of the editor (DEVELOPERS.md "Editor mode").
//
// A new map is a map file of its own made from the terrain of a campaign map of the player's game,
// as map_tool.py new does it: the chunks of maps\lNN\lNN.map with only the ground object left - OBJ
// (the ground's VID 1024, whose image stays maps\lNN\lNN.vid of the game: nothing is copied), GRPH,
// HEAD, GRID, SPR and SPRD with the ground alone, no players (PLAY) and no groups (GROU). Or with a
// ground of its own: a meadow drawn here (value noise, the colours of the sandbox's ground) of any
// size from 512 to 4096 px in steps of 256 (the ground's tile), written as <name>.vid the way
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
// and a meadow beyond it - and puts the ground object in the middle; cropping keeps the image.
#include "patch.h"
#include "dialog_template.h"
#include "editor_ground.h"
#include "editor_text.h"

std::string language_file(const std::string& path);   // language.cpp: the pack's copy of a file

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cwchar>
#include <cstdio>
#include <cstring>
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

// The picture of a ground image (DEVELOPERS.md "Data formats": a texture image of one frame whose tiles
// show whole textures; with imageFlags 4 every colour texture is followed by its heights, which a
// tile's texture field counts; tiles are 9 ints with imageFlags 0x10, else an int and 8 shorts).
bool read_ground(const std::string& path, Picture& p) {
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

// The ground image (DEVELOPERS.md "Data formats": texture tiles), as map_tool.py ground lays it out: the
// picture of `old` where it reaches, elsewhere a meadow - or, with `sample` (a texture's 128 x 128,
// editor_ground.cpp), that texture alone: an empty map's ground.
std::string ground_vid(int width, int height, uint32_t seed, const Picture* old = nullptr,
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

// ---- the map ----

constexpr int MIN_SIZE = 512, MAX_SIZE = 4096;

bool valid_size(int w, int h) {
    return w % TILE == 0 && h % TILE == 0 && w >= MIN_SIZE && h >= MIN_SIZE && w <= MAX_SIZE && h <= MAX_SIZE;
}

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
    if (generated && !valid_size(w, h)) {
        error = ui_text("SizeError", "a map is 512 to 4096 px wide and high, in steps of 256");
        return "";
    }
    const std::string image = generated ? folder + "\\" + name + ".vid" : "";
    std::vector<Chunk> chunks;
    if (!new_map_chunks(source, image, w, h, chunks, error)) return "";
    CreateDirectoryA(folder.c_str(), nullptr);
    if (generated && !write_file(image, ground_vid(w, h, GetTickCount(), nullptr, sample))) {
        error = ui_printf(ui_format("NewNotWritten", "%s could not be written"), image.c_str());
        return "";
    }
    if (!write_file(map, write_res("MAP ", chunks))) {
        error = ui_printf(ui_format("NewNotWritten", "%s could not be written"), map.c_str());
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

bool valid_name(const std::string& name) {
    if (name.empty() || name.size() > 24 || name == "template") return false;
    for (char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}

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

// The part w x h of the map from its top-left corner of a ground picture drawn centred on (gx, gy),
// as a picture of its own; where the old one does not reach, a meadow at the flat ground's height.
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

// The map file `map` (saved just before) at the size w x h: the right and the bottom edge move. The
// map gets a ground image of its own, <map>.vid, of exactly its new size: the old picture (its own or
// the campaign's) where it lay on the map - grown, a meadow where it did not reach; cropped, cut at
// the new edge, so nothing of the old ground is left past it - with the ground object in the middle.
bool resize_map_file(const std::string& map, int w, int h, std::string& error) {
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
    if (w != ow || h != oh) {
        std::string* record = nullptr;
        for (std::string& r : obj->records)
            if (static_cast<int>(u32(r, 0)) == GROUND_NVID) record = &r;
        size_t start;
        const std::string old_image = record ? ground_image(*record, start) : "";
        Picture old;
        float gx = ow / 2.0f, gy = oh / 2.0f;
        ground_place(chunks, gx, gy);
        if (!record || old_image.empty() || !read_ground(old_image, old)) {
            error = ui_printf(ui_format("SizeNoImage", "the ground's image %s cannot be read"), old_image.c_str());
            return false;
        }
        const size_t dot = map.find_last_of('.');
        const std::string image = map.substr(0, dot) + ".vid";
        const uint32_t seed = GetTickCount();
        const Picture cut = picture_on_map(old, gx, gy, w, h, seed);
        if (!write_file(image, ground_vid(w, h, seed, &cut))) {
            error = ui_printf(ui_format("NewNotWritten", "%s could not be written"), image.c_str());
            return false;
        }
        *record = record->substr(0, start) + image + '\0';
    }
    set_size(chunks, w, h, true);
    if (!write_file(map, write_res("MAP ", chunks))) {
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
bool write_ground_picture(const std::string& image, int w, int h, const std::vector<uint16_t>& pixels,
                          const std::vector<uint16_t>& heights) {
    Picture p;
    p.width = w;
    p.height = h;
    p.pixels = pixels;
    p.heights = heights;
    return write_file(image, ground_vid(w - w % TILE, h - h % TILE, 0, &p));
}

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

namespace {

// ---- the dialogs ----
//
// File > New offers the ground in one list: an empty map - flat, nothing on it, the ground one
// texture of the painting's (editor\textures.tsv: grass, sand, snow, rock ...) -, the meadow drawn
// here, then the terrains of the 20 missions. The size (of an empty map or a meadow; a mission's
// terrain has its own) is set with - and + (256 px a step, 512 to 4096: the ground's tiles are
// 256 x 256 and the engine's GRID and camera handle up to 4096) or a preset button, the result shown
// in pixels and in tiles. File > Map size has the same row.

enum : int {
    IDC_NAME = 100, IDC_GROUND, IDC_WIDTH, IDC_HEIGHT, IDC_W_MINUS, IDC_W_PLUS, IDC_H_MINUS, IDC_H_PLUS, IDC_SIZE_INFO,
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

// The line under the size: "= 2048 x 1536 px, 8 x 6 tiles of 256 px" (or why it is not a size).
void show_size(HWND dialog) {
    BOOL ok1 = FALSE, ok2 = FALSE;
    const int w = static_cast<int>(GetDlgItemInt(dialog, IDC_WIDTH, &ok1, FALSE));
    const int h = static_cast<int>(GetDlgItemInt(dialog, IDC_HEIGHT, &ok2, FALSE));
    const std::wstring text =
        ok1 && ok2 && valid_size(w, h)
            ? ui_wide(ui_printf(ui_format("SizeInfo", "= %d x %d px, %d x %d tiles of 256 px"), w, h, w / TILE, h / TILE))
            : ui_wtext("SizeInfoBad", "512 to 4096 px, in steps of 256");
    SetDlgItemTextW(dialog, IDC_SIZE_INFO, text.c_str());
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

// The size row at `y`: width - + x height - +, the line with the result, the presets below.
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
}

bool read_size(HWND dialog, int& w, int& h) {
    BOOL ok1 = FALSE, ok2 = FALSE;
    w = static_cast<int>(GetDlgItemInt(dialog, IDC_WIDTH, &ok1, FALSE));
    h = static_cast<int>(GetDlgItemInt(dialog, IDC_HEIGHT, &ok2, FALSE));
    if (ok1 && ok2 && valid_size(w, h)) return true;
    MessageBoxW(dialog,
                ui_wtext("SizeBad", "The size: 512 to 4096 px wide and high, in steps of 256 (the campaign's maps are "
                                    "1024 x 768, 2048 x 1536 and 3072 x 2304).").c_str(),
                ui_wtext("ResizeTitle", "Map size").c_str(), MB_OK | MB_ICONWARNING);
    return false;
}

// The size row is for an empty map and the meadow; a mission's terrain has its size.
void enable_size(HWND dialog, bool on) {
    for (int id = IDC_WIDTH; id <= IDC_SIZE_INFO; ++id)
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
        enable_size(dialog, chosen_ground(dialog) >= MEADOW);
        return TRUE;
    }
    if (size_command(dialog, LOWORD(w), HIWORD(w))) return TRUE;
    if (LOWORD(w) == IDCANCEL) {
        EndDialog(dialog, 0);
        return TRUE;
    }
    if (LOWORD(w) != IDOK) return FALSE;
    auto* choice = reinterpret_cast<Choice*>(GetWindowLongPtrW(dialog, DWLP_USER));
    char name[64] = "";
    GetDlgItemTextA(dialog, IDC_NAME, name, sizeof name);
    if (!valid_name(name)) {
        MessageBoxW(dialog, ui_wtext("NewBadName", "The name of the map's folder: small letters, digits and _, at most 24.").c_str(),
                    ui_wtext("NewTitle", "New map").c_str(), MB_OK | MB_ICONWARNING);
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
    DialogTemplate t(ui_wtext("NewTitle", "New map").c_str(), 300, 140);
    t.item(STATIC, ui_wtext("NewFolder", "Folder in maps\\ (small letters, digits, _):").c_str(), -1, SS_LEFT, 8, 10, 150, 8);
    t.item(EDIT, L"", IDC_NAME, LINE, 160, 8, 132, 12);
    t.item(STATIC, ui_wtext("NewGround", "Ground:").c_str(), -1, SS_LEFT, 8, 30, 40, 8);
    t.item(COMBOBOX, L"", IDC_GROUND, CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 50, 28, 242, 200);
    t.item(STATIC, ui_wtext("NewSize", "Size (an empty map, a meadow):").c_str(), -1, SS_LEFT, 8, 48, 284, 8);
    size_items(t, 8, 60);
    t.item(STATIC, ui_wtext("NewGroundNote", "An empty map is flat and bare, one texture to paint over; a mission's terrain stays the game's own picture.").c_str(),
           -1, SS_LEFT, 8, 94, 284, 16);
    t.item(BUTTON, ui_wtext("ButtonOk", "OK").c_str(), IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP, 184, 118, 50, 14);
    t.item(BUTTON, ui_wtext("ButtonCancel", "Cancel").c_str(), IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 242, 118, 50, 14);
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
    DialogTemplate t(ui_wtext("ResizeTitle", "Map size").c_str(), 300, 86);
    t.item(STATIC, ui_wtext("SizeLabel", "The map's size (px, steps of 256; the right and the bottom edge move):").c_str(), -1, SS_LEFT,
           8, 8, 284, 16);
    size_items(t, 8, 28);
    t.item(BUTTON, ui_wtext("ButtonOk", "OK").c_str(), IDOK, BS_DEFPUSHBUTTON | WS_TABSTOP, 184, 64, 50, 14);
    t.item(BUTTON, ui_wtext("ButtonCancel", "Cancel").c_str(), IDCANCEL, BS_PUSHBUTTON | WS_TABSTOP, 242, 64, 50, 14);
    int size[2] = {w, h};
    if (DialogBoxIndirectParamW(GetModuleHandleW(nullptr), t.get(), owner, size_proc, reinterpret_cast<LPARAM>(size)) != 1)
        return false;
    w = size[0];
    h = size[1];
    return true;
}

// The resize of the saved map file (editor.cpp saves before and loads it again after).
bool resize_map(const std::string& map, int w, int h, std::string& error) {
    if (!valid_size(w, h)) {
        error = ui_text("SizeError", "a map is 512 to 4096 px wide and high, in steps of 256");
        return false;
    }
    return resize_map_file(map, w, h, error);
}