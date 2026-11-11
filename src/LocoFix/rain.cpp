// The rain over the whole view at any size (BUGS.md F27, before C5).
//
// Graphics_DrawRain (0x410e20) keeps 250 drops, each a line of two vertices (TLVertex: x, y, z,
// rhw, colour) of length `len` (15-65), slanted by the wind. A drop starts at depth
// (len + 10) * 0.00122 + 0.0156 and loses 0.000122 for every pixel it falls, so it lives for
// (len + 10) * 10 = 250-750 px of fall (the z test hides it where it reaches the ground) or until
// it leaves the view. A drop made again starts in the top 40 px - only from the 50th one made
// again in the same frame anywhere down to the view's bottom, which steady rain never reaches
// (about 7 a frame are made again). Nothing falls below about 790 px: made for 768 px of height,
// where the rain thins toward the bottom (in rows of 64 px, from about 0.57 drops a row-band near
// the top to 0.02 at the bottom, simulated), and on a higher view the lower
// part stays dry - a player's screenshot at 1920x1016, GitHub issue #13. The 250 drops are also
// spread over the whole width, so a wider view gets thinner rain.
//
// This is the engine's function in C++, the same steps in the same order, the same calls of the
// C runtime's rand(), its own globals for the number of drops, the wind's last values and the
// slant; the drops are ours, as many as the view needs. Up to 1024x768 nothing changes. On a
// bigger view:
// - the number of drops is the original's 250 for every 1024 x 768 px of the view (2.6 times at
//   1920x1080, 4.7 at 2560x1440), and so is the "50 in a frame" of the first rule; while the rain
//   starts or stops, that many more drops come or go a frame, so it takes the original's time;
// - on a view higher than 768 px a drop made again under the first rule starts in the top band
//   only with the chance (768 / height)^2, otherwise anywhere down the view as under the second
//   rule: the rain reaches the bottom (simulated: at 1080 px from about 0.49 at the top to 0.17 at
//   the bottom, at 1440 px from 0.42 to 0.26).
#include "patch.h"
#include <vector>

namespace {

constexpr uintptr_t DRAW_RAIN = 0x410e20;
constexpr uintptr_t G_WORLD = 0x4a9d38;
constexpr uintptr_t SET_ENVIRONMENT = 0x410270;
constexpr uintptr_t SET_BLEND = 0x40f000;
constexpr uintptr_t DRAW_PRIMITIVE = 0x415e40;
constexpr uintptr_t CRT_RAND = 0x456efc;
constexpr uintptr_t G_SIN_TABLE = 0x47c894;
constexpr uintptr_t G_GAME_TIME = 0x4a9d64, G_PREV_GAME_TIME = 0x4a9d60;
// the engine's state of the rain
constexpr uintptr_t G_RAIN_DROP_COUNT = 0x4a9a2c;
constexpr uintptr_t G_LAST_WIND_DIRECTION = 0x47a5ec, G_LAST_WIND = 0x47a5f0;
constexpr uintptr_t G_RAIN_SLANT = 0x4a9588;   // sin(wind direction) * wind * 1000

// Graphics (re/types.txt)
constexpr int G_FLAGS = 0, G_SCREEN_WIDTH = 0x20c, G_VIEW = 0x220, G_ENVIRONMENT = 0xba8, G_WIND_DIRECTION = 0xbac,
              G_WIND = 0xbb0, G_D3D_DEVICE = 0xbf4;
constexpr uint32_t ENV_RAIN = 0xc00, ENV_RAIN_STARTING = 0x800, ENV_RAIN_STOPPING = 0x400;

constexpr int ORIGINAL_DROPS = 250, ORIGINAL_FIRST_RULE = 50;
constexpr float ORIGINAL_W = 1024, ORIGINAL_H = 768, TOP_BAND = 40;

struct TLVertex {
    float x, y, z, rhw;
    uint32_t color;
};
struct Drop {
    TLVertex head, tail;   // the lower end, then the upper one
};

std::vector<Drop> g_drops;

template <typename T>
T& at(void* base, int offset) {
    return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset);
}
template <typename T>
T& global(uintptr_t address) {
    return *reinterpret_cast<T*>(address);
}
int crt_rand() { return reinterpret_cast<int(__cdecl*)()>(CRT_RAND)(); }

void __fastcall draw_rain(uint8_t* g, void*) {
    int& count = global<int>(G_RAIN_DROP_COUNT);
    uint32_t& environment = at<uint32_t>(g, G_ENVIRONMENT);
    if ((environment & ENV_RAIN) == 0) {
        count = 0;
        return;
    }
    uint8_t* world = global<uint8_t*>(G_WORLD);
    if ((at<uint32_t>(g, G_FLAGS) & 0x20) || (at<uint32_t>(world, 4) & 0x10)) return;

    const float* view = &at<float>(g, G_VIEW);   // left, right, top, bottom
    const float view_w = view[1] - view[0], view_h = view[3] - view[2];
    const float area = view_w * view_h / (ORIGINAL_W * ORIGINAL_H);
    const int most = area > 1 ? static_cast<int>(ORIGINAL_DROPS * area + 0.5f) : ORIGINAL_DROPS;
    const int first_rule = most * ORIGINAL_FIRST_RULE / ORIGINAL_DROPS;
    const int step = (most + ORIGINAL_DROPS - 1) / ORIGINAL_DROPS;   // drops a frame while it starts or stops
    const float band_chance = view_h > ORIGINAL_H ? (ORIGINAL_H / view_h) * (ORIGINAL_H / view_h) : 1;
    if (static_cast<int>(g_drops.size()) < most) g_drops.resize(most, Drop{});   // new drops: colour 0, made at once
    if (count > most) count = most;   // a smaller view than before (a level of another size)

    // a new wind turns the drops in the air
    float& slant = global<float>(G_RAIN_SLANT);
    const int direction = at<int>(g, G_WIND_DIRECTION);
    const float wind = at<float>(g, G_WIND);
    if (global<int>(G_LAST_WIND_DIRECTION) != direction || global<float>(G_LAST_WIND) != wind) {
        const float old = slant;
        slant = reinterpret_cast<const float*>(G_SIN_TABLE)[direction] * wind * 1000.0f;
        const float change = slant - old;
        for (int i = 0; i < count; ++i) {
            Drop& d = g_drops[i];
            d.tail.x += (d.tail.y - d.head.y) * change * 0.005f;
        }
        global<int>(G_LAST_WIND_DIRECTION) = direction;
        global<float>(G_LAST_WIND) = wind;
    }

    switch (environment & ENV_RAIN) {
    case ENV_RAIN:
        count = most;
        break;
    case ENV_RAIN_STARTING:
        if (count < most)
            count = count + step < most ? count + step : most;
        else
            reinterpret_cast<void(__fastcall*)(void*, void*, uint32_t)>(SET_ENVIRONMENT)(g, nullptr, ENV_RAIN);
        break;
    case ENV_RAIN_STOPPING:
        if (count < 1)
            environment &= ~ENV_RAIN;
        else
            count = count > step ? count - step : 0;
        break;
    }

    const int dt = global<int>(G_GAME_TIME) - global<int>(G_PREV_GAME_TIME);
    const float screen_w = at<float>(g, G_SCREEN_WIDTH);
    int made = 0;
    for (int i = 0; i < count; ++i) {
        Drop& d = g_drops[i];
        if (d.head.color != 0 && d.head.x >= view[0] && d.head.x < view[1] && d.head.y >= view[2] &&
            d.head.y < view[3] && d.head.z >= 0.015625f) {
            // falling: by its length, the step's time and the slant
            const float fall = (d.tail.z - d.head.z) * static_cast<float>(dt) * 200.0f;
            float dx = slant * fall * 0.005f;
            if (dx + d.head.x < 0) dx += screen_w;
            if (screen_w < dx + d.head.x) dx -= screen_w;
            d.head.x += dx;
            d.head.y += fall;
            d.head.z -= fall * 0.00012207031f;
            d.tail.x += dx;
            d.tail.y += fall;
            d.tail.z -= fall * 0.00012207031f;
            continue;
        }
        // made again
        ++made;
        const float length = static_cast<float>(crt_rand() % 51) + 15.0f;
        const float drift = slant * length * -0.005f;
        const float x = static_cast<float>(crt_rand()) * (view[1] - 1.0f) * 3.051851e-05f;
        float band = view[3];   // the second rule: anywhere down the view
        if (made < first_rule &&
            (band_chance >= 1 || static_cast<float>(crt_rand()) * 3.051851e-05f < band_chance))
            band = TOP_BAND;
        const float y = static_cast<float>(crt_rand()) * band * 3.051851e-05f;
        const float z = (length + 10.0f) * 0.0012207031f + 0.015625f;
        d.head = {x, y, z, 1.0f, 0x70e0e0ff};
        d.tail = {x + drift, y - length, length * 0.00012207031f + z, 1.0f, 0x308080ff};
    }

    void* device = at<void*>(g, G_D3D_DEVICE);
    using SetTexture_t = HRESULT(__stdcall*)(void*, DWORD, void*);
    reinterpret_cast<SetTexture_t>((*reinterpret_cast<void***>(device))[0x8c / 4])(device, 0, nullptr);
    reinterpret_cast<void(__fastcall*)(void*, void*, int, int)>(SET_BLEND)(g, nullptr, 5, 6);
    using DrawPrimitive_t = void(__fastcall*)(void*, void*, int, int, void*, int, int);
    reinterpret_cast<DrawPrimitive_t>(DRAW_PRIMITIVE)(g, nullptr, 2, 0x44, g_drops.data(), 0x14, count * 2);
}

}  // namespace

void install_rain() {
    // sub esp,0x34 / mov eax,[ecx+0xba8]
    const uint8_t start[] = {0x83, 0xEC, 0x34, 0x8B, 0x81, 0xA8, 0x0B, 0x00, 0x00};
    patch::redirect(DRAW_RAIN, start, sizeof start, reinterpret_cast<void*>(&draw_rain), "Graphics_DrawRain");
}
