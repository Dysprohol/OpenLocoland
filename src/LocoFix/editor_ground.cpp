// The ground's picture in the editor (DEVELOPERS.md "Editor mode", MAP_RULES.md 2): the held level's
// ground image read into memory (its colours and heights - editor_newmap.cpp reads a ground image),
// painted by the Paint and Water tools of editor_edit.cpp, shown at once by writing the changed
// texels into the image's own Direct3D surfaces, and written as the map's own image when the map is
// saved.
//
// A map ground (VID 1024, GroundVid) is a grid of 256 x 256 tiles; each tile shows a colour texture
// (R5G6B5 in the files; in memory whatever Texture_Construct made of it - 565, 555, 1555 or 4444)
// and is followed by its height texture (D16), which GroundVid_Draw blits into the depth buffer: the
// objects and the water are drawn against those heights. The picture is drawn centred on the ground
// object, in screen coordinates of the map (x, y - z), so a pixel of the picture is the point of the
// screen it covers. Its heights are 1026 + 8 a GRID level (picture_height).
//
// The textures come from the player's own campaign grounds (game folder: editor\textures.tsv names
// a 128 x 128 patch of a maps\lNN\lNN.vid for each, laid mirrored so it tiles without seams); nothing
// of the pictures is in the repository. A stroke keeps the rectangle it changed before and after
// (PaintPatch, one step of the history). A map made on a campaign terrain keeps pointing at the
// player's lNN.vid until it is painted; then it gets <name>.vid of its own when saved.
//
// The relief: when the ground's heights (GRID) change, the picture is drawn again there for them
// (ground_relief) - raised ground shows higher on the screen, shaded by its slope, and its height
// texture goes up with it, so the objects on it stand on what is seen and what stands behind a hill
// is hidden by it.
#include "patch.h"
#include "editor_ground.h"
#include "rail_rules.h"

#include <ddraw.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>

// editor_newmap.cpp
std::string map_ground_image(const std::string& map);
bool read_ground_picture(const std::string& image, int& w, int& h, std::vector<uint16_t>& pixels,
                         std::vector<uint16_t>& heights);
bool write_ground_picture(const std::string& image, int w, int h, const std::vector<uint16_t>& pixels,
                          const std::vector<uint16_t>& heights);
bool set_map_ground_image(const std::string& map, const std::string& image);
bool read_sprite_frame(const std::string& image, int frame, int& w, int& h, std::vector<uint32_t>& argb);

namespace {

// GroundVid (re/types.txt): its tiles, texture count and Texture objects; a Texture's surface
constexpr size_t GV_TILES = 0x41c, GV_TEXTURE_COUNT = 0x420, GV_TEXTURES = 0x424, V_IMAGE_FLAGS = 0x2a0;
constexpr size_t TEXTURE_SURFACE = 0x4;
constexpr int PATCH = 128;   // a texture's sample

struct MemoryTile {   // GroundTile, 0x24 bytes
    int unknown, texture, offset_x, offset_y, width, height, x, y, next;
};

struct Texture {
    GroundTexture info;
    std::string map;
    int x = 0, y = 0;
    bool sampled = false;
    std::vector<uint16_t> patch;   // PATCH x PATCH
};

std::vector<Texture> g_textures;
std::vector<GroundTexture> g_texture_infos;
bool g_textures_read = false;

// the picture
bool g_ready = false, g_painted = false;
std::string g_image;   // the image file it was read from
int g_w = 0, g_h = 0;
float g_left = 0, g_top = 0;   // where its top left corner is (screen coordinates of the map)
uint8_t* g_vid = nullptr;
std::vector<uint16_t> g_pixels, g_heights, g_original_pixels, g_original_heights;
// the stroke: the picture before it and the rectangle it changed
std::vector<uint16_t> g_stroke_pixels, g_stroke_heights;
std::vector<uint8_t> g_textures_of, g_stroke_textures;   // the texture painted on each pixel (NO_TEXTURE: the original)
bool g_stroke = false;
int g_x0 = 0, g_y0 = 0, g_x1 = -1, g_y1 = -1;
// the rectangle changed since the last frame, to be written into the textures (ground_flush)
int g_pending_x0 = 0, g_pending_y0 = 0, g_pending_x1 = -1, g_pending_y1 = -1;
GroundTimes g_times;

std::vector<std::string> fields(const std::string& line) {
    std::vector<std::string> out;
    size_t a = 0;
    while (true) {
        const size_t b = line.find('\t', a);
        out.push_back(line.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    return out;
}

std::string read_text(const char* path) {
    HANDLE f = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return "";
    std::string d(GetFileSize(f, nullptr), '\0');
    DWORD got = 0;
    if (!d.empty() && !ReadFile(f, &d[0], static_cast<DWORD>(d.size()), &got, nullptr)) d.clear();
    CloseHandle(f);
    return d;
}

void read_textures() {
    if (g_textures_read) return;
    g_textures_read = true;
    const std::string text = read_text("editor\\textures.tsv");
    bool header = true;
    size_t a = 0;
    while (a < text.size()) {
        size_t b = text.find('\n', a);
        if (b == std::string::npos) b = text.size();
        std::string line = text.substr(a, b - a);
        a = b + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        if (header) {   // key english map x y
            header = false;
            continue;
        }
        const std::vector<std::string> f = fields(line);
        if (f.size() < 5) continue;
        Texture t;
        t.info.key = f[0];
        t.info.english = f[1];
        t.map = f[2];
        t.x = std::atoi(f[3].c_str());
        t.y = std::atoi(f[4].c_str());
        g_textures.push_back(t);
    }
    // sampled now: every campaign ground is read once, its patches cut out
    std::map<std::string, std::vector<size_t>> by_map;
    for (size_t i = 0; i < g_textures.size(); ++i) by_map[g_textures[i].map].push_back(i);
    for (const auto& m : by_map) {
        int w = 0, h = 0;
        std::vector<uint16_t> pixels, heights;
        const std::string image = "maps\\" + m.first + "\\" + m.first + ".vid";
        if (!read_ground_picture(image, w, h, pixels, heights)) {
            patch::log("ground: %s cannot be read - its textures are left out", image.c_str());
            continue;
        }
        for (size_t i : m.second) {
            Texture& t = g_textures[i];
            if (t.x < 0 || t.y < 0 || t.x + PATCH > w || t.y + PATCH > h) continue;
            t.patch.resize(PATCH * PATCH);
            uint32_t r = 0, g = 0, bl = 0;
            for (int y = 0; y < PATCH; ++y)
                for (int x = 0; x < PATCH; ++x) {
                    const uint16_t c = pixels[static_cast<size_t>(t.y + y) * w + t.x + x];
                    t.patch[y * PATCH + x] = c;
                    r += c >> 11;
                    g += (c >> 5) & 63;
                    bl += c & 31;
                }
            const uint32_t n = PATCH * PATCH;
            t.info.mean = static_cast<uint16_t>(((r / n) << 11) | ((g / n) << 5) | (bl / n));
            t.info.available = t.sampled = true;
        }
    }
    for (const Texture& t : g_textures) g_texture_infos.push_back(t.info);
    patch::log("ground: %u textures", static_cast<unsigned>(g_textures.size()));
}

uint16_t sample(const Texture& t, int x, int y) {
    int u = x & 255, v = y & 255;   // mirrored every 128 px: no seams
    if (u >= PATCH) u = 255 - u;
    if (v >= PATCH) v = 255 - v;
    return t.patch[v * PATCH + u];
}

uint16_t blend(uint16_t a, uint16_t b, float t) {
    if (t <= 0) return a;
    if (t >= 1) return b;
    const int ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31, br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
    const int r = static_cast<int>(ar + (br - ar) * t + 0.5f), g = static_cast<int>(ag + (bg - ag) * t + 0.5f),
              bl = static_cast<int>(ab + (bb - ab) * t + 0.5f);
    return static_cast<uint16_t>((r << 11) | (g << 5) | bl);
}

uint32_t hash(int x, int y) {
    uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

// The weight of a brush at a distance: full inside, falling off over the outer 45 %, the edge
// broken by a grain so it does not look drawn with a compass; a hard brush is full to its edge.
float weight(float distance, float radius, int x, int y, bool hard = false) {
    const float d = distance / radius + ((hash(x, y) & 255) / 255.0f - 0.5f) * (hard ? 0.03f : 0.12f);
    if (d >= 1) return 0;
    if (hard) return 1;
    const float t = d <= 0.55f ? 1.0f : (1.0f - d) / 0.45f;
    return t * t * (3 - 2 * t);
}

void touch(int x, int y) {
    if (g_x1 < g_x0) {
        g_x0 = g_x1 = x;
        g_y0 = g_y1 = y;
        return;
    }
    g_x0 = (std::min)(g_x0, x);
    g_y0 = (std::min)(g_y0, y);
    g_x1 = (std::max)(g_x1, x);
    g_y1 = (std::max)(g_y1, y);
}

// ---- the gravel bed's band ----
//
// Its colours are the top of the bed decal of the kind: frame 1 of the decal (a straight piece across
// the screen) from column BED_U0 and row BED_V0, BED_U x BED_V px, each row brought to the mean
// brightness (the decal is a rounded embankment, lit from above) and darkened to 13/16 - what the
// engine shows of the decal baked into the ground in play (l02: ~110, 100, 60 where the sprite is
// ~140, 115, 60). Along a rail the columns are laid mirrored, a whole number of BED_U to the rail
// (its ends meet the next rail's on the same column); across it the rows go from the line out to the
// band's edge on either side, so two rails joined either way round match. map_tool.py paint lays
// the band the same way.
constexpr int BED_U0 = 45, BED_U = 60, BED_V0 = 52, BED_V = 36, BED_SEGMENTS = 16, BED_RISE = 7;
BedKind g_bed_kinds[BED_KINDS] = {{612, "BedMeadow", "meadow"}, {615, "BedIce", "ice"}, {616, "BedVolcanic", "volcanic"},
                                  {617, "BedDesert", "desert"}};
std::vector<uint16_t> g_bed_patch[BED_KINDS];   // BED_V rows of BED_U
bool g_beds_read = false;

void read_beds() {
    if (g_beds_read) return;
    g_beds_read = true;
    for (int k = 0; k < BED_KINDS; ++k) {
        int w = 0, h = 0;
        std::vector<uint32_t> argb;
        const std::string image = "vid\\" + std::to_string(g_bed_kinds[k].nvid) + ".vid";
        if (!read_sprite_frame(image, 1, w, h, argb) || w < BED_U0 + BED_U || h < BED_V0 + BED_V) {
            patch::log("ground: %s cannot be read - no bed of that kind", image.c_str());
            continue;
        }
        bool whole = true;
        long long rows[BED_V] = {}, total = 0;
        for (int v = 0; v < BED_V; ++v)
            for (int u = 0; u < BED_U; ++u) {
                const uint32_t c = argb[static_cast<size_t>(BED_V0 + v) * w + BED_U0 + u];
                whole = whole && (c >> 24) != 0;
                rows[v] += ((c >> 16) & 255) + ((c >> 8) & 255) + (c & 255);
            }
        for (long long r : rows) total += r;
        if (!whole || total == 0) continue;
        std::vector<uint16_t>& p = g_bed_patch[k];
        uint32_t r5 = 0, g6 = 0, b5 = 0;
        for (int v = 0; v < BED_V; ++v)
            for (int u = 0; u < BED_U; ++u) {
                const uint32_t c = argb[static_cast<size_t>(BED_V0 + v) * w + BED_U0 + u];
                const long long div = static_cast<long long>(BED_V) * (rows[v] ? rows[v] : 1) * 16;
                auto level = [&](uint32_t x) { return static_cast<uint32_t>((std::min)(255LL, x * total * 13 / div)); };
                const uint32_t r = level((c >> 16) & 255), g = level((c >> 8) & 255), b = level(c & 255);
                p.push_back(static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)));
                r5 += r >> 3;
                g6 += g >> 2;
                b5 += b >> 3;
            }
        const uint32_t n = BED_U * BED_V;
        g_bed_kinds[k].mean = static_cast<uint16_t>(((r5 / n) << 11) | ((g6 / n) << 5) | (b5 / n));
        g_bed_kinds[k].available = true;
    }
}

// A rail's line on the picture: BED_SEGMENTS straight pieces of the quadratic curve from its end 0
// through its middle to its end 1 (a straight rail's middle is on the line between its ends), each
// end at its point's height less the rail's altitude - the rail's own height, a ramp's raised end
// 11 higher -, the middle half way; all of it BED_RISE px up the screen, where the rail's sprite
// shows its sleepers (the middle of frame 1 is 7 px above the cell's), so the band is centred on
// the rail seen and not on its place.
struct BedLine {
    float x[BED_SEGMENTS + 1], y[BED_SEGMENTS + 1], s[BED_SEGMENTS + 1];
    float l, t, r, b;   // its box
};

BedLine bed_line(const BedRail& rail, float left, float top) {
    const rail::Rail q = rail::rail_of(rail.x, rail.y, rail.z, rail.direction);
    const rail::Point a = rail::end_of(q, 0), c = rail::end_of(q, 1);
    const float za = static_cast<float>(q.z0 - rail::ALTITUDE + BED_RISE), zc = static_cast<float>(q.z1 - rail::ALTITUDE + BED_RISE);
    const float ax = static_cast<float>(a.x) - left, ay = static_cast<float>(a.y) - za - top;
    const float bx = rail.x - left, by = rail.y - (za + zc) / 2 - top;
    const float cx = static_cast<float>(c.x) - left, cy = static_cast<float>(c.y) - zc - top;
    BedLine line;
    line.l = line.t = 1e9f;
    line.r = line.b = -1e9f;
    for (int i = 0; i <= BED_SEGMENTS; ++i) {
        const float t = static_cast<float>(i) / BED_SEGMENTS, m = 1 - t;
        line.x[i] = m * m * ax + 2 * t * m * bx + t * t * cx;
        line.y[i] = m * m * ay + 2 * t * m * by + t * t * cy;
        line.s[i] = i ? line.s[i - 1] + std::hypot(line.x[i] - line.x[i - 1], line.y[i] - line.y[i - 1]) : 0;
        line.l = (std::min)(line.l, line.x[i]);
        line.t = (std::min)(line.t, line.y[i]);
        line.r = (std::max)(line.r, line.x[i]);
        line.b = (std::max)(line.b, line.y[i]);
    }
    return line;
}

// ---- the live image ----

struct Format {
    bool z = false;
    int bits = 0;
    uint32_t r = 0, g = 0, b = 0, a = 0;
};

uint16_t convert(uint16_t c, const Format& f) {
    if (f.r == 0xf800) return c;   // R5G6B5
    const int r = c >> 11, g = (c >> 5) & 63, b = c & 31;
    if (f.r == 0x7c00) return static_cast<uint16_t>((f.a ? 0x8000 : 0) | (r << 10) | ((g >> 1) << 5) | b);   // 555 / 1555
    if (f.r == 0x0f00) return static_cast<uint16_t>(0xf000 | ((r >> 1) << 8) | ((g >> 2) << 4) | (b >> 1));   // 4444
    return c;
}

// Writes a rectangle of one texture of the live image from the picture.
bool upload_texture(uint8_t* texture, const MemoryTile& t, int x0, int y0, int x1, int y1, bool heights) {
    if (!texture) return false;
    auto* surface = *reinterpret_cast<IDirectDrawSurface7**>(texture + TEXTURE_SURFACE);
    if (!surface) return false;
    DDSURFACEDESC2 d = {};
    d.dwSize = sizeof d;
    if (surface->Lock(nullptr, &d, DDLOCK_WAIT | DDLOCK_WRITEONLY | DDLOCK_NOSYSLOCK, nullptr) != DD_OK) return false;
    ++g_times.locks;
    Format f;
    f.z = (d.ddpfPixelFormat.dwFlags & DDPF_ZBUFFER) != 0;
    f.bits = f.z ? static_cast<int>(d.ddpfPixelFormat.dwZBufferBitDepth) : static_cast<int>(d.ddpfPixelFormat.dwRGBBitCount);
    f.r = d.ddpfPixelFormat.dwRBitMask;
    f.g = d.ddpfPixelFormat.dwGBitMask;
    f.b = d.ddpfPixelFormat.dwBBitMask;
    f.a = (d.ddpfPixelFormat.dwFlags & DDPF_ALPHAPIXELS) ? d.ddpfPixelFormat.dwRGBAlphaBitMask : 0;
    bool ok = f.bits == 16 && (heights == f.z);
    if (ok)
        for (int y = y0; y < y1; ++y) {
            auto* row = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(d.lpSurface) + (t.offset_y + y - t.y) * d.lPitch);
            for (int x = x0; x < x1; ++x) {
                const size_t i = static_cast<size_t>(y) * g_w + x;
                row[t.offset_x + x - t.x] = heights ? g_heights[i] : convert(g_pixels[i], f);
            }
        }
    surface->Unlock(nullptr);
    return ok;
}

// The rectangle (x0, y0) - (x1, y1) of the picture into the live image's textures.
void write_textures(int x0, int y0, int x1, int y1) {
    if (!g_vid) return;
    const double began = ground_clock();
    ++g_times.uploads;
    const auto* tiles = *reinterpret_cast<MemoryTile**>(g_vid + GV_TILES);
    auto** textures = *reinterpret_cast<uint8_t***>(g_vid + GV_TEXTURES);
    const int count = *reinterpret_cast<short*>(g_vid + GV_TEXTURE_COUNT);
    const bool with_heights = (*reinterpret_cast<uint16_t*>(g_vid + V_IMAGE_FLAGS) & 4) != 0;
    if (!tiles || !textures) return;
    static bool warned = false;
    for (int i = 0, guard = 0; guard < 4096; ++guard) {
        const MemoryTile& t = tiles[i];
        const int tx0 = (std::max)(x0, t.x), ty0 = (std::max)(y0, t.y);
        const int tx1 = (std::min)({x1, t.x + t.width, g_w}), ty1 = (std::min)({y1, t.y + t.height, g_h});
        if (tx0 < tx1 && ty0 < ty1 && t.texture >= 0 && t.texture < count) {
            const bool colour = upload_texture(textures[t.texture], t, tx0, ty0, tx1, ty1, false);
            const bool height = !with_heights || (t.texture + 1 < count && upload_texture(textures[t.texture + 1], t, tx0, ty0, tx1, ty1, true));
            if ((!colour || !height) && !warned) {
                warned = true;
                patch::log("ground: a texture of the live image could not be written (colour %d, heights %d)", colour, height);
            }
        }
        if (t.next <= 0) break;
        i = t.next;
    }
    g_times.upload += ground_clock() - began;
}

// A change of the rectangle (x0, y0) - (x1, y1) of the picture: written into the textures by the
// next frame (several dabs or steps a frame, one lock of each texture).
void upload(int x0, int y0, int x1, int y1) {
    if (g_pending_x1 < g_pending_x0) {
        g_pending_x0 = x0;
        g_pending_y0 = y0;
        g_pending_x1 = x1;
        g_pending_y1 = y1;
        return;
    }
    g_pending_x0 = (std::min)(g_pending_x0, x0);
    g_pending_y0 = (std::min)(g_pending_y0, y0);
    g_pending_x1 = (std::max)(g_pending_x1, x1);
    g_pending_y1 = (std::max)(g_pending_y1, y1);
}

bool to_picture(float x, float y, int& px, int& py) {
    px = static_cast<int>(std::floor(x - g_left));
    py = static_cast<int>(std::floor(y - g_top));
    return px >= 0 && py >= 0 && px < g_w && py < g_h;
}

template <typename F> void each_pixel(float x, float y, float radius, F f, bool hard = false) {
    int cx, cy;
    to_picture(x, y, cx, cy);
    const int r = static_cast<int>(std::ceil(radius));
    const int x0 = (std::max)(0, cx - r), y0 = (std::max)(0, cy - r), x1 = (std::min)(g_w, cx + r + 1),
              y1 = (std::min)(g_h, cy + r + 1);
    if (x0 >= x1 || y0 >= y1) return;
    const double began = ground_clock();
    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px) {
            const float w = weight(std::hypot(static_cast<float>(px - cx), static_cast<float>(py - cy)), radius, px, py, hard);
            if (w > 0) f(px, py, static_cast<size_t>(py) * g_w + px, w);
        }
    g_times.paint += ground_clock() - began;
    ++g_times.paints;
    touch(x0, y0);
    touch(x1 - 1, y1 - 1);
    upload(x0, y0, x1, y1);
}

// ---- the relief ----
//
// A pixel of the picture is a point of the screen (x, y - z): ground raised by dz shows dz px higher
// and hides the ground behind it, ground lowered shows lower and may be hidden by the ground in front.
// So a column of the picture is drawn front to back - the ground's world y from the bottom of the
// screen up -, each pixel getting the first ground point that reaches it, and that point's texel is
// where the old heights showed it. Between the cells' middles the heights are interpolated (the
// engine stands an object on its cell's height, which is the value at the middle).

struct Grid {
    const short* z;
    int w, h;
    float at(float x, float y) const {
        const float fx = x / 8 - 0.5f, fy = y / 8 - 0.5f;
        const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
        const float tx = fx - x0, ty = fy - y0;
        auto cell = [&](int cx, int cy) {
            cx = (std::max)(0, (std::min)(w - 1, cx));
            cy = (std::max)(0, (std::min)(h - 1, cy));
            return static_cast<float>(z[cy * w + cx]);
        };
        const float top = cell(x0, y0) + (cell(x0 + 1, y0) - cell(x0, y0)) * tx;
        const float bottom = cell(x0, y0 + 1) + (cell(x0 + 1, y0 + 1) - cell(x0, y0 + 1)) * tx;
        return top + (bottom - top) * ty;
    }
    // the light on the ground at a point: from the upper left of the screen and high, half of it
    // ambient (a steep face toward the viewer stays readable), 1 on flat ground
    float light(float x, float y) const {
        const float dx = (at(x + 4, y) - at(x - 4, y)) / 8, dy = (at(x, y + 4) - at(x, y - 4)) / 8;
        const float lx = -0.42f, ly = -0.50f, lz = 0.76f;
        const float n = std::sqrt(dx * dx + dy * dy + 1);
        return 0.5f + 0.5f * (std::max)(0.0f, (-dx * lx - dy * ly + lz) / n) / lz;
    }
};

uint16_t lit(uint16_t c, float f) {
    if (f > 0.995f && f < 1.005f) return c;
    const int r = (std::min)(31, static_cast<int>((c >> 11) * f + 0.5f)), g = (std::min)(63, static_cast<int>(((c >> 5) & 63) * f + 0.5f)),
              b = (std::min)(31, static_cast<int>((c & 31) * f + 0.5f));
    return static_cast<uint16_t>((r << 11) | (g << 5) | b);
}

}  // namespace

bool ground_prepare(const std::string& map, float x, float y, void* vid, std::string& why) {
    if (g_ready) return true;
    const std::string image = map_ground_image(map);
    if (image.empty() || !read_ground_picture(image, g_w, g_h, g_pixels, g_heights)) {
        why = image.empty() ? map : image;
        return false;
    }
    g_image = image;
    g_textures_of.assign(g_pixels.size(), NO_TEXTURE);
    g_original_pixels = g_pixels;
    g_original_heights = g_heights;
    g_left = x - g_w / 2.0f;
    g_top = y - g_h / 2.0f;
    g_vid = static_cast<uint8_t*>(vid);
    g_ready = true;
    patch::log("ground: %s %dx%d ready to paint", image.c_str(), g_w, g_h);
    return true;
}

bool ground_ready() { return g_ready; }

void ground_flush() {
    if (g_pending_x1 < g_pending_x0) return;
    const int x0 = g_pending_x0, y0 = g_pending_y0, x1 = g_pending_x1, y1 = g_pending_y1;
    g_pending_x0 = 0;
    g_pending_x1 = -1;
    if (g_ready) write_textures(x0, y0, x1, y1);
}

void ground_reset() {
    g_ready = g_painted = g_stroke = false;
    g_vid = nullptr;
    g_w = g_h = 0;
    g_x0 = g_y0 = 0;
    g_x1 = g_y1 = -1;
    g_pending_x0 = g_pending_y0 = 0;
    g_pending_x1 = g_pending_y1 = -1;
    g_image.clear();
    g_pixels.clear();
    g_heights.clear();
    g_original_pixels.clear();
    g_original_heights.clear();
    g_stroke_pixels.clear();
    g_stroke_heights.clear();
    g_textures_of.clear();
    g_stroke_textures.clear();
}

const std::vector<GroundTexture>& ground_textures() {
    read_textures();
    return g_texture_infos;
}

bool ground_texture_sample(int texture, std::vector<uint16_t>& patch) {
    read_textures();
    if (texture < 0 || texture >= static_cast<int>(g_textures.size()) || !g_textures[texture].sampled) return false;
    patch = g_textures[texture].patch;
    return true;
}

void ground_stroke_begin() {
    if (!g_ready) return;
    g_stroke = true;
    g_stroke_pixels = g_pixels;
    g_stroke_heights = g_heights;
    g_stroke_textures = g_textures_of;
    g_x0 = 0;
    g_x1 = -1;
}

void ground_paint(float x, float y, float radius, int texture, bool restore, float strength, bool hard) {
    read_textures();
    if (!g_ready || (!restore && (texture < 0 || texture >= static_cast<int>(g_textures.size()) || !g_textures[texture].sampled)))
        return;
    each_pixel(x, y, radius, [&](int px, int py, size_t i, float w) {
        const float a = w * strength;
        g_pixels[i] = blend(g_pixels[i], restore ? g_original_pixels[i] : sample(g_textures[texture], px, py), a);
        if (restore) g_heights[i] = static_cast<uint16_t>(g_heights[i] + (g_original_heights[i] - g_heights[i]) * a);
        if (a >= 0.5f) g_textures_of[i] = restore ? NO_TEXTURE : static_cast<uint8_t>(texture);
    }, hard);
}

int ground_texture_at(float x, float y) {
    int px, py;
    if (!g_ready || !to_picture(x, y, px, py)) return -1;
    const uint8_t t = g_textures_of[static_cast<size_t>(py) * g_w + px];
    if (t != NO_TEXTURE) return t;
    // not painted in this session: the texture whose sample most of the pixels around are (within
    // a step of each channel)
    read_textures();
    int best = -1, best_count = 12;   // of 25
    for (size_t k = 0; k < g_textures.size(); ++k) {
        if (!g_textures[k].sampled) continue;
        int count = 0;
        for (int yy = (std::max)(0, py - 2); yy <= (std::min)(g_h - 1, py + 2); ++yy)
            for (int xx = (std::max)(0, px - 2); xx <= (std::min)(g_w - 1, px + 2); ++xx) {
                const uint16_t a = g_pixels[static_cast<size_t>(yy) * g_w + xx], b = sample(g_textures[k], xx, yy);
                if (std::abs((a >> 11) - (b >> 11)) <= 1 && std::abs(((a >> 5) & 63) - ((b >> 5) & 63)) <= 1 &&
                    std::abs((a & 31) - (b & 31)) <= 1)
                    ++count;
            }
        if (count > best_count) {
            best_count = count;
            best = static_cast<int>(k);
        }
    }
    return best;
}

const BedKind* ground_bed_kinds() {
    read_beds();
    return g_bed_kinds;
}

float ground_bed_distance(const BedRail& rail, float x, float y) {
    const BedLine l = bed_line(rail, 0, 0);
    float best = 1e9f;
    for (int i = 0; i < BED_SEGMENTS; ++i) {
        const float ux = l.x[i + 1] - l.x[i], uy = l.y[i + 1] - l.y[i], len2 = ux * ux + uy * uy;
        float t = len2 > 0 ? ((x - l.x[i]) * ux + (y - l.y[i]) * uy) / len2 : 0;
        t = (std::max)(0.0f, (std::min)(1.0f, t));
        best = (std::min)(best, std::hypot(x - l.x[i] - ux * t, y - l.y[i] - uy * t));
    }
    return best;
}

void ground_bed(const std::vector<BedRail>& rails, size_t paint, int kind, float width, float strength, bool hard,
                bool restore) {
    read_beds();
    if (!g_ready || kind < 0 || kind >= BED_KINDS || (!restore && !g_bed_kinds[kind].available) || width < 2) return;
    const double began = ground_clock();
    const float hw = width / 2;
    const int x0 = 0, y0 = 0, x1 = g_w, y1 = g_h;
    std::vector<BedLine> lines;
    std::vector<bool> painted;   // a line of the rails to paint, else one that only shares the pixels
    for (size_t n = 0; n < rails.size(); ++n) {
        const BedLine l = bed_line(rails[n], g_left, g_top);
        if (l.r + hw >= x0 && l.l - hw <= x1 && l.b + hw >= y0 && l.t - hw <= y1) {
            lines.push_back(l);
            painted.push_back(n < paint);
        }
    }
    if (lines.empty()) return;
    const std::vector<uint16_t>& patch = g_bed_patch[kind];
    int px0 = g_w, py0 = g_h, px1 = -1, py1 = -1;
    // a square of the picture at a time: the nearest line of each pixel, its distance and the place
    // along its rail (in the decal's columns)
    constexpr int SQUARE = 128;
    std::vector<float> nearest(SQUARE * SQUARE), along(SQUARE * SQUARE);
    std::vector<uint8_t> mine(SQUARE * SQUARE);
    for (int sy = y0; sy < y1; sy += SQUARE)
        for (int sx = x0; sx < x1; sx += SQUARE) {
            const int ex = (std::min)(x1, sx + SQUARE), ey = (std::min)(y1, sy + SQUARE);
            std::fill(nearest.begin(), nearest.end(), hw);
            bool any = false;
            for (size_t n = 0; n < lines.size(); ++n) {
                const BedLine& l = lines[n];
                if (l.r + hw < sx || l.l - hw > ex || l.b + hw < sy || l.t - hw > ey) continue;
                const float length = l.s[BED_SEGMENTS];
                const float scale = length > 0 ? (std::max)(1.0f, std::round(length / BED_U)) * BED_U / length : 0;
                for (int i = 0; i < BED_SEGMENTS; ++i) {
                    const float ax = l.x[i], ay = l.y[i], ux = l.x[i + 1] - ax, uy = l.y[i + 1] - ay;
                    const float len2 = ux * ux + uy * uy;
                    const int lx = (std::max)(sx, static_cast<int>(std::floor((std::min)(ax, ax + ux) - hw)));
                    const int rx = (std::min)(ex, static_cast<int>(std::ceil((std::max)(ax, ax + ux) + hw)) + 1);
                    const int ty = (std::max)(sy, static_cast<int>(std::floor((std::min)(ay, ay + uy) - hw)));
                    const int by = (std::min)(ey, static_cast<int>(std::ceil((std::max)(ay, ay + uy) + hw)) + 1);
                    for (int py = ty; py < by; ++py)
                        for (int px = lx; px < rx; ++px) {
                            const float qx = px + 0.5f - ax, qy = py + 0.5f - ay;
                            float t = len2 > 0 ? (qx * ux + qy * uy) / len2 : 0;
                            t = (std::max)(0.0f, (std::min)(1.0f, t));
                            const float d = std::hypot(qx - ux * t, qy - uy * t);
                            const size_t k = static_cast<size_t>(py - sy) * SQUARE + (px - sx);
                            if (d >= nearest[k]) continue;
                            nearest[k] = d;
                            along[k] = (l.s[i] + t * (l.s[i + 1] - l.s[i])) * scale;
                            mine[k] = painted[n];
                            any = any || painted[n];
                        }
                }
            }
            if (!any) continue;
            for (int py = sy; py < ey; ++py)
                for (int px = sx; px < ex; ++px) {
                    const size_t k = static_cast<size_t>(py - sy) * SQUARE + (px - sx);
                    const float d = nearest[k];
                    if (d >= hw || !mine[k]) continue;
                    const float a = weight(d, hw, px, py, hard) * strength;
                    if (a <= 0) continue;
                    const size_t i = static_cast<size_t>(py) * g_w + px;
                    if (restore) {
                        g_pixels[i] = blend(g_pixels[i], g_original_pixels[i], a);
                        if (a >= 0.5f) g_textures_of[i] = NO_TEXTURE;
                    } else {
                        const int v = (std::min)(BED_V - 1, static_cast<int>(d / hw * BED_V));
                        int u = static_cast<int>(std::floor(along[k])) % (2 * BED_U);
                        if (u < 0) u += 2 * BED_U;
                        if (u >= BED_U) u = 2 * BED_U - 1 - u;
                        g_pixels[i] = blend(g_pixels[i], patch[static_cast<size_t>(v) * BED_U + u], a);
                        if (a >= 0.5f) g_textures_of[i] = static_cast<uint8_t>(BED_TEXTURE + kind);
                    }
                    px0 = (std::min)(px0, px);
                    py0 = (std::min)(py0, py);
                    px1 = (std::max)(px1, px);
                    py1 = (std::max)(py1, py);
                }
        }
    g_times.paint += ground_clock() - began;
    ++g_times.paints;
    if (px1 < px0) return;
    touch(px0, py0);
    touch(px1, py1);
    upload(px0, py0, px1 + 1, py1 + 1);
}

int ground_bed_at(float x, float y) {
    read_beds();
    int px, py;
    if (!g_ready || !to_picture(x, y, px, py)) return -1;
    int r = 0, g = 0, b = 0, n = 0;
    for (int yy = (std::max)(0, py - 3); yy <= (std::min)(g_h - 1, py + 3); ++yy)
        for (int xx = (std::max)(0, px - 3); xx <= (std::min)(g_w - 1, px + 3); ++xx) {
            const uint16_t c = g_pixels[static_cast<size_t>(yy) * g_w + xx];
            r += c >> 11;
            g += (c >> 5) & 63;
            b += c & 31;
            ++n;
        }
    int best = -1, best_d = 12;   // 565 steps, green counted in halves
    for (int k = 0; k < BED_KINDS; ++k) {
        if (!g_bed_kinds[k].available) continue;
        const uint16_t m = g_bed_kinds[k].mean;
        const int d = std::abs(r / n - (m >> 11)) + std::abs(g / n - ((m >> 5) & 63)) / 2 + std::abs(b / n - (m & 31));
        if (d < best_d) {
            best_d = d;
            best = k;
        }
    }
    return best;
}

void ground_lower(float x, float y, float radius, int bed, int texture, bool restore) {
    read_textures();
    if (!g_ready) return;
    const bool tex = texture >= 0 && texture < static_cast<int>(g_textures.size()) && g_textures[texture].sampled;
    const float bed_height = picture_height(bed);
    each_pixel(x, y, radius, [&](int px, int py, size_t i, float w) {
        if (restore) {
            g_heights[i] = static_cast<uint16_t>(g_heights[i] + (g_original_heights[i] - g_heights[i]) * w);
            g_pixels[i] = blend(g_pixels[i], g_original_pixels[i], w);
            if (w >= 0.5f) g_textures_of[i] = NO_TEXTURE;
            return;
        }
        const float lowered = g_heights[i] + (bed_height - g_heights[i]) * w;
        if (lowered < g_heights[i]) g_heights[i] = static_cast<uint16_t>(lowered);
        if (tex) g_pixels[i] = blend(g_pixels[i], sample(g_textures[texture], px, py), w * 0.85f);
        if (tex && w * 0.85f >= 0.5f) g_textures_of[i] = static_cast<uint8_t>(texture);
    });
}

void ground_relief(const short* from, const short* to, int gw, int gh, int cx0, int cy0, int cx1, int cy1, int z_lo,
                   int z_hi) {
    if (!g_ready || !g_stroke || g_stroke_pixels.size() != g_pixels.size() || !from || !to) return;
    const double began = ground_clock();
    const Grid f{from, gw, gh}, t{to, gw, gh};
    // the cells around the changed ones too: the heights between the middles and the light use them
    cx0 = (std::max)(0, cx0 - 2);
    cy0 = (std::max)(0, cy0 - 2);
    cx1 = (std::min)(gw - 1, cx1 + 2);
    cy1 = (std::min)(gh - 1, cy1 + 2);
    int z_min = z_lo, z_max = z_hi, column_min = 1 << 20, column_max = -(1 << 20);
    for (int cy = 0; cy < gh; ++cy)
        for (int cx = cx0; cx <= cx1; ++cx) {
            const int a = from[cy * gw + cx], b = to[cy * gw + cx];
            column_min = (std::min)({column_min, a, b});
            column_max = (std::max)({column_max, a, b});
            if (cy >= cy0 && cy <= cy1) {
                z_min = (std::min)({z_min, a, b});
                z_max = (std::max)({z_max, a, b});
            }
        }
    // the screen rows where the changed ground showed or shows now
    const int s_lo = cy0 * 8 - z_max - 1, s_hi = (cy1 + 1) * 8 - z_min + 1;
    const float y_start = static_cast<float>(s_hi + column_max + 1), y_end = static_cast<float>(s_lo + column_min - 1);
    int px0 = g_w, px1 = -1, py0 = g_h, py1 = -1;
    for (int x = cx0 * 8; x < (cx1 + 1) * 8; ++x) {
        const int px = static_cast<int>(std::floor(x - g_left));
        if (px < 0 || px >= g_w) continue;
        const float fx = x + 0.5f;
        int top = s_hi + 1;   // the rows from here down are drawn
        // front to back by the stretches between the cells' middles (8 px, the height linear along
        // each): a stretch coming up the screen gives each row it reaches the ground point that
        // reaches it - a sample a row; one going down the screen (rising steeper than 1:1) is hidden
        float ya = y_start, za = t.at(fx, ya);
        while (ya > y_end && top > s_lo) {
            const float yb = (std::max)(y_end, 8 * std::floor((ya - 4.0001f) / 8) + 4), zb = t.at(fx, yb);
            const float sa = ya - za, sb = yb - zb;
            if (sb < sa) {
                const float k = (sa - sb) / (ya - yb);
                const int first = (std::min)(top - 1, static_cast<int>(std::ceil(sa)) - 1);
                const int last = (std::max)(s_lo, static_cast<int>(std::ceil(sb)));
                for (int r = first; r >= last; --r) {
                    const float y = ya - (sa - r) / k;
                    const float zt = za + (zb - za) * (ya - y) / (ya - yb), zf = f.at(fx, y);
                    const int src_row = static_cast<int>(std::floor(y - zf + 0.5f - g_top));
                    const size_t src = static_cast<size_t>((std::max)(0, (std::min)(g_h - 1, src_row))) * g_w + px;
                    const float light = std::fabs(zt - zf) < 0.01f ? 1.0f : t.light(fx, y) / f.light(fx, y);
                    const int height = static_cast<int>(std::lround(g_stroke_heights[src] + 8 * (zt - zf)));
                    const int py = static_cast<int>(std::floor(r - g_top));
                    top = r;
                    if (py < 0 || py >= g_h) continue;
                    const size_t i = static_cast<size_t>(py) * g_w + px;
                    g_pixels[i] = lit(g_stroke_pixels[src], (std::max)(0.55f, (std::min)(1.4f, light)));
                    g_heights[i] = static_cast<uint16_t>((std::max)(0, (std::min)(65535, height)));
                    if (g_stroke_textures.size() == g_textures_of.size()) g_textures_of[i] = g_stroke_textures[src];
                    py0 = (std::min)(py0, py);
                    py1 = (std::max)(py1, py);
                }
            }
            ya = yb;
            za = zb;
        }
        px0 = (std::min)(px0, px);
        px1 = (std::max)(px1, px);
    }
    g_times.relief += ground_clock() - began;
    ++g_times.reliefs;
    if (px1 < px0 || py1 < py0) return;
    touch(px0, py0);
    touch(px1, py1);
    upload(px0, py0, px1 + 1, py1 + 1);
}

bool campaign_ground_image(const std::string& image) {
    // maps\lNN\lNN.vid
    std::string s = image;
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (s.size() != 16 || s.compare(0, 5, "maps\\") != 0 || s[8] != '\\' || s.compare(12, 4, ".vid") != 0) return false;
    return s[5] == 'l' && std::isdigit(static_cast<unsigned char>(s[6])) && std::isdigit(static_cast<unsigned char>(s[7])) &&
           s.compare(5, 3, s, 9, 3) == 0;
}

bool ground_own() { return g_ready && (g_painted || !campaign_ground_image(g_image)); }

std::shared_ptr<PaintPatch> ground_stroke_end() {
    if (!g_stroke) return nullptr;
    g_stroke = false;
    std::shared_ptr<PaintPatch> p;
    if (g_x1 >= g_x0) {
        p = std::make_shared<PaintPatch>();
        p->x = g_x0;
        p->y = g_y0;
        p->w = g_x1 - g_x0 + 1;
        p->h = g_y1 - g_y0 + 1;
        for (int y = 0; y < p->h; ++y)
            for (int x = 0; x < p->w; ++x) {
                const size_t i = static_cast<size_t>(p->y + y) * g_w + p->x + x;
                p->colour_before.push_back(g_stroke_pixels[i]);
                p->height_before.push_back(g_stroke_heights[i]);
                p->colour_after.push_back(g_pixels[i]);
                p->height_after.push_back(g_heights[i]);
                p->texture_before.push_back(g_stroke_textures.empty() ? NO_TEXTURE : g_stroke_textures[i]);
                p->texture_after.push_back(g_textures_of[i]);
            }
        g_painted = true;
    }
    g_stroke_pixels.clear();
    g_stroke_heights.clear();
    g_stroke_textures.clear();
    return p;
}

void ground_apply(const PaintPatch& p, bool after) {
    if (!g_ready) return;
    for (int y = 0; y < p.h; ++y)
        for (int x = 0; x < p.w; ++x) {
            const size_t i = static_cast<size_t>(p.y + y) * g_w + p.x + x, k = static_cast<size_t>(y) * p.w + x;
            g_pixels[i] = after ? p.colour_after[k] : p.colour_before[k];
            g_heights[i] = after ? p.height_after[k] : p.height_before[k];
            if (k < p.texture_after.size()) g_textures_of[i] = after ? p.texture_after[k] : p.texture_before[k];
        }
    upload(p.x, p.y, p.x + p.w, p.y + p.h);
    g_painted = true;
}

int ground_level_at(float x, float y) {
    int px, py;
    if (!g_ready || !to_picture(x, y, px, py)) return 0;
    return (g_heights[static_cast<size_t>(py) * g_w + px] - 1026) / 8;
}

double ground_clock() {
    static LARGE_INTEGER frequency = {};
    if (!frequency.QuadPart) QueryPerformanceFrequency(&frequency);
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart * 1000.0 / frequency.QuadPart;
}

GroundTimes ground_times(bool reset) {
    const GroundTimes t = g_times;
    if (reset) g_times = GroundTimes();
    return t;
}

bool ground_save(const std::string& map) {
    if (!g_ready || !g_painted) return true;
    // <folder>\<name>.vid next to the map - never over a campaign ground of the player's game
    std::string image = map.substr(0, map.find_last_of('.'));
    const size_t slash = image.find_last_of('\\');
    const std::string name = slash == std::string::npos ? image : image.substr(slash + 1);
    if (name.size() == 3 && name[0] == 'l' && std::isdigit(static_cast<unsigned char>(name[1])) &&
        std::isdigit(static_cast<unsigned char>(name[2])))
        image += "_painted";
    image += ".vid";
    if (!write_ground_picture(image, g_w, g_h, g_pixels, g_heights) || !set_map_ground_image(map, image)) {
        patch::log("ground: %s could not be written", image.c_str());
        return false;
    }
    patch::log("ground: the painted picture saved as %s", image.c_str());
    return true;
}
