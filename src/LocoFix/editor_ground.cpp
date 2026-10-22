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
#include "patch.h"
#include "editor_ground.h"

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
int g_w = 0, g_h = 0;
float g_left = 0, g_top = 0;   // where its top left corner is (screen coordinates of the map)
uint8_t* g_vid = nullptr;
std::vector<uint16_t> g_pixels, g_heights, g_original_pixels, g_original_heights;
// the stroke: the picture before it and the rectangle it changed
std::vector<uint16_t> g_stroke_pixels, g_stroke_heights;
bool g_stroke = false;
int g_x0 = 0, g_y0 = 0, g_x1 = -1, g_y1 = -1;

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
// broken by a grain so it does not look drawn with a compass.
float weight(float distance, float radius, int x, int y) {
    const float d = distance / radius + ((hash(x, y) & 255) / 255.0f - 0.5f) * 0.12f;
    if (d >= 1) return 0;
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
void upload(int x0, int y0, int x1, int y1) {
    if (!g_vid) return;
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
}

bool to_picture(float x, float y, int& px, int& py) {
    px = static_cast<int>(std::floor(x - g_left));
    py = static_cast<int>(std::floor(y - g_top));
    return px >= 0 && py >= 0 && px < g_w && py < g_h;
}

template <typename F> void each_pixel(float x, float y, float radius, F f) {
    int cx, cy;
    to_picture(x, y, cx, cy);
    const int r = static_cast<int>(std::ceil(radius));
    const int x0 = (std::max)(0, cx - r), y0 = (std::max)(0, cy - r), x1 = (std::min)(g_w, cx + r + 1),
              y1 = (std::min)(g_h, cy + r + 1);
    if (x0 >= x1 || y0 >= y1) return;
    for (int py = y0; py < y1; ++py)
        for (int px = x0; px < x1; ++px) {
            const float w = weight(std::hypot(static_cast<float>(px - cx), static_cast<float>(py - cy)), radius, px, py);
            if (w > 0) f(px, py, static_cast<size_t>(py) * g_w + px, w);
        }
    touch(x0, y0);
    touch(x1 - 1, y1 - 1);
    upload(x0, y0, x1, y1);
}

}  // namespace

bool ground_prepare(const std::string& map, float x, float y, void* vid, std::string& why) {
    if (g_ready) return true;
    const std::string image = map_ground_image(map);
    if (image.empty() || !read_ground_picture(image, g_w, g_h, g_pixels, g_heights)) {
        why = image.empty() ? map : image;
        return false;
    }
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

void ground_reset() {
    g_ready = g_painted = g_stroke = false;
    g_vid = nullptr;
    g_pixels.clear();
    g_heights.clear();
    g_original_pixels.clear();
    g_original_heights.clear();
    g_stroke_pixels.clear();
    g_stroke_heights.clear();
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
    g_x0 = 0;
    g_x1 = -1;
}

void ground_paint(float x, float y, float radius, int texture, bool restore) {
    read_textures();
    if (!g_ready || (!restore && (texture < 0 || texture >= static_cast<int>(g_textures.size()) || !g_textures[texture].sampled)))
        return;
    each_pixel(x, y, radius, [&](int px, int py, size_t i, float w) {
        g_pixels[i] = blend(g_pixels[i], restore ? g_original_pixels[i] : sample(g_textures[texture], px, py), w);
        if (restore) g_heights[i] = static_cast<uint16_t>(g_heights[i] + (g_original_heights[i] - g_heights[i]) * w);
    });
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
            return;
        }
        const float lowered = g_heights[i] + (bed_height - g_heights[i]) * w;
        if (lowered < g_heights[i]) g_heights[i] = static_cast<uint16_t>(lowered);
        if (tex) g_pixels[i] = blend(g_pixels[i], sample(g_textures[texture], px, py), w * 0.85f);
    });
}

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
            }
        g_painted = true;
    }
    g_stroke_pixels.clear();
    g_stroke_heights.clear();
    return p;
}

void ground_apply(const PaintPatch& p, bool after) {
    if (!g_ready) return;
    for (int y = 0; y < p.h; ++y)
        for (int x = 0; x < p.w; ++x) {
            const size_t i = static_cast<size_t>(p.y + y) * g_w + p.x + x, k = static_cast<size_t>(y) * p.w + x;
            g_pixels[i] = after ? p.colour_after[k] : p.colour_before[k];
            g_heights[i] = after ? p.height_after[k] : p.height_before[k];
        }
    upload(p.x, p.y, p.x + p.w, p.y + p.h);
    g_painted = true;
}

int ground_level_at(float x, float y) {
    int px, py;
    if (!g_ready || !to_picture(x, y, px, py)) return 0;
    return (g_heights[static_cast<size_t>(py) * g_w + px] - 1026) / 8;
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
