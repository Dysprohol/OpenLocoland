// The fill of a light's texture, the same texels with less work (perf.h; docs/dev/changes.md fix 54).
//
// Graphics_DrawLight (0x411dd0) draws the light of a colour-box VID (layer 8: the lamps, fires,
// glows): it locks a scratch texture (the grey one or the rgb one, in turn), fills a rectangle of it
// - one texel for every 4 x 4 screen pixels of the light, up to 128 x 128 - from the depth buffer,
// unlocks it and adds it to the screen with Texture_DrawZ. For each texel (0x41207b ... 0x412314) it
// takes two depth samples, at (x, y) and (x + 3, y + 3) from the texel's corner, the lower height of
// the two against the light's height gives a brightness, 0..255, written as a byte (P8 texture) or
// as grayColors[brightness] (16-bit); a texel whose both samples fall outside the view is left as it
// was. The engine's loop does per texel what is the same for the whole light - four float to integer
// conversions (crt_ftol) of the light's position and eight float comparisons with the view's edges -
// and a division; in a sampling profile of mission 13 at 1920x1080 the loop was about 7 % of the main
// thread.
//
// Here the loop is C++: the view tests are made once per column and once per row with the very x87
// instructions of the engine (the same rounding, whatever the FPU's precision), the light's position
// converted once, and a texel whose distance is past the light's reach gets 0 without the division
// (the quotient would be 256 or more, and 256 - it is clamped to 0). The arithmetic, the texels
// written and those left alone are the engine's. The rest of Graphics_DrawLight - the size, the
// texture's lock and unlock, the draw - is the engine's own code.
#include "patch.h"
#include "perf.h"

uint8_t* light_texture_after_fill(uint8_t* frame, const uint8_t* bits, int pitch);   // light_textures.cpp

namespace {

constexpr uintptr_t FILL = 0x41207b;           // mov eax,[esp+0x20] / neg ebp: the loops' start
constexpr uintptr_t FILL_ORIGINAL = 0x412081;  // the engine's loops, after the two instructions
constexpr uintptr_t FILLED = 0x412318;         // the unlock, edi = the texture
constexpr uintptr_t UNLOCKED = 0x412326;       // after the unlock: the draw's states, Texture_DrawZ(edi)
constexpr int P8 = 0x29;                       // D3DFMT_P8, Texture.format

// Graphics_DrawLight's stack frame at FILL (esp)
constexpr int F_TEXTURE = 0x10, F_PITCH = 0x18, F_HALF_W = 0x1c, F_HALF_H = 0x20, F_K = 0x3c, F_BITS = 0x44,
              F_Z = 0x4c, F_AREA = 0x50, F_X = 0xf0, F_Y = 0xf4;
// Graphics
constexpr int G_GRAY_COLORS = 0x8, G_DEPTH_BITS = 0x218, G_DEPTH_PITCH = 0x21c, G_VIEW = 0x220;

const float THREE = 3.0f;   // the engine's constant at 0x4753a8

// The engine's view test of one column (x of the texel's corner and x + 3) - FILD, FADD, FCOM as
// at 0x4120c8 ... 0x412170: the C0 flag (below) of each comparison.
void column_test(int col_dx, float x, const float* view, bool& first, bool& second) {
    unsigned short l1, r1, l2, r2;
    __asm {
        mov ecx, view
        fild dword ptr [col_dx]
        fadd dword ptr [x]
        fcom dword ptr [ecx]        ; viewLeft
        fnstsw ax
        mov l1, ax
        fcom dword ptr [ecx + 4]    ; viewRight
        fnstsw ax
        mov r1, ax
        fadd dword ptr [THREE]
        fcom dword ptr [ecx]
        fnstsw ax
        mov l2, ax
        fcomp dword ptr [ecx + 4]
        fnstsw ax
        mov r2, ax
    }
    first = !(l1 & 0x100) && (r1 & 0x100);
    second = !(l2 & 0x100) && (r2 & 0x100);
}

// ... of one row: y stored as a float (the engine's [esp+0x48]) for the first sample, y + 3 kept in
// the FPU for the second (0x4120a8 ... 0x41210f, 0x41217f ... 0x412197).
void row_test(int row_offset, float y, const float* view, bool& first, bool& second) {
    unsigned short t1, b1, t2, b2;
    float stored;
    __asm {
        mov ecx, view
        fild dword ptr [row_offset]
        fadd dword ptr [y]
        fld st(0)
        fadd dword ptr [THREE]
        fxch st(1)
        fstp dword ptr [stored]
        fcom dword ptr [ecx + 8]    ; viewTop
        fnstsw ax
        mov t2, ax
        fcomp dword ptr [ecx + 12]  ; viewBottom
        fnstsw ax
        mov b2, ax
        fld dword ptr [stored]
        fcom dword ptr [ecx + 8]
        fnstsw ax
        mov t1, ax
        fcomp dword ptr [ecx + 12]
        fnstsw ax
        mov b1, ax
    }
    first = !(t1 & 0x100) && (b1 & 0x100);
    second = !(t2 & 0x100) && (b2 & 0x100);
}

int truncate(float f) { return static_cast<int>(static_cast<long long>(f)); }   // crt_ftol

void __cdecl fill(uint8_t* frame, uint8_t* g) {
    const auto at = [frame](int offset) { return *reinterpret_cast<int*>(frame + offset); };
    const int half_w = at(F_HALF_W), half_h = at(F_HALF_H), k = at(F_K), z = at(F_Z), area = at(F_AREA);
    const int pitch = at(F_PITCH);
    uint8_t* bits = *reinterpret_cast<uint8_t**>(frame + F_BITS);
    const bool p8 = *reinterpret_cast<int*>(*reinterpret_cast<uint8_t**>(frame + F_TEXTURE) + 8) == P8;
    const float x = *reinterpret_cast<float*>(frame + F_X), y = *reinterpret_cast<float*>(frame + F_Y);
    const float* view = reinterpret_cast<const float*>(g + G_VIEW);
    const uint16_t* gray = reinterpret_cast<const uint16_t*>(g + G_GRAY_COLORS);
    const uint16_t* depth = *reinterpret_cast<uint16_t* const*>(g + G_DEPTH_BITS);
    const int depth_pitch = *reinterpret_cast<int*>(g + G_DEPTH_PITCH);
    if (-half_h >= half_h || -half_w >= half_w) return;

    // the columns' tests, once (at most 0x200 / 4 * 2 = 256 columns)
    bool col1[256], col2[256];
    const int columns = (2 * half_w + 3) / 4;
    for (int i = 0; i < columns && i < 256; ++i) column_test(-half_w + 4 * i, x, view, col1[i], col2[i]);
    // the sample's place in the depth buffer: (ftol(y) + dy) * pitch + dx + ftol(x), as the engine has it
    const int x0 = truncate(x), y0 = truncate(y);
    // c = 256 - d / area is 0 for d >= 256 * area (area > 0): no division needed there
    const long long reach = area > 0 ? 256LL * area : 0;

    for (int dy = -half_h, row = 0; dy < half_h; dy += 4, row += 4) {
        bool row1, row2;
        row_test(dy, y, view, row1, row2);
        const int r = row / 4;
        for (int dx = -half_w, col = 0, i = 0; dx < half_w; dx += 4, col += 4, ++i) {
            int v = 0x7fff;
            if (col1[i] && row1) {
                v = (depth[(y0 + dy) * depth_pitch + dx + x0] >> 3) - 0x80;
            }
            if (col2[i] && row2) {
                const int index = (y0 + 3 + dy) * depth_pitch + dx + x0 + 3;
                const int v2 = (depth[index] >> 3) - 0x80;
                if (v2 < v) v = v2;
            }
            if (v == 0x7fff) continue;
            // the engine's 32-bit arithmetic (imul wraps), divisions by 4 toward zero
            const int a = static_cast<int>(static_cast<uint32_t>(v - z) * static_cast<uint32_t>(v - z));
            const int b0 = v - k + dy;
            const int b = static_cast<int>(static_cast<uint32_t>(b0) * static_cast<uint32_t>(b0) * 9u);
            const int d = static_cast<int>(static_cast<uint32_t>(a / 4) + static_cast<uint32_t>(b / 4) +
                                           static_cast<uint32_t>(dx) * static_cast<uint32_t>(dx));
            int c;
            if (area > 0 && d >= reach) c = 0;
            else c = area ? 0x100 - d / area : 0x100 - d;
            if (c < 0) c = 0;
            else if (c > 0xff) c = 0xff;
            const int texel = r * pitch + col / 4;
            if (p8) bits[texel] = static_cast<uint8_t>(c);
            else reinterpret_cast<uint16_t*>(bits)[texel] = gray[c];
        }
    }
}

uintptr_t g_filled = FILLED, g_fill_original = FILL_ORIGINAL, g_unlocked = UNLOCKED;

// light_textures.cpp: the filled rectangle into a texture of the light's own; the texture to draw or 0
uint8_t* __cdecl after_fill(uint8_t* frame, uint8_t* g) {
    (void)g;
    const int bytes = *reinterpret_cast<int*>(*reinterpret_cast<uint8_t**>(frame + F_TEXTURE) + 8) == P8 ? 1 : 2;
    return light_texture_after_fill(frame, *reinterpret_cast<uint8_t**>(frame + F_BITS),
                                    *reinterpret_cast<int*>(frame + F_PITCH) * bytes);
}

__declspec(naked) void fill_light() {
    __asm {
        cmp byte ptr [perf::g_original], 0
        jne original
        mov eax, esp
        push ebx            // g_graphics
        push eax            // the frame
        call fill
        add esp, 8
        mov eax, esp
        push ebx
        push eax
        call after_fill
        add esp, 8
        test eax, eax
        jz own
        mov edi, eax                      // the light's texture; the engine's is unlocked
        jmp dword ptr [g_unlocked]
    own:
        mov edi, dword ptr [esp + 0x10]   // the texture, as the engine's loops leave it
        jmp dword ptr [g_filled]
    original:
        mov eax, dword ptr [esp + 0x20]
        neg ebp
        jmp dword ptr [g_fill_original]
    }
}

}  // namespace

void install_light_fill() {
    // mov eax,[esp+0x20] / neg ebp / cmp ebp,eax / mov [esp+0x14],ebp / jge 0x412318
    const uint8_t expected[] = {0x8B, 0x44, 0x24, 0x20, 0xF7, 0xDD, 0x3B, 0xE8, 0x89, 0x6C, 0x24, 0x14, 0x0F, 0x8D, 0x8B, 0x02, 0x00, 0x00};
    // mov edi,[esp+0x10] / mov eax,[edi+4]: the unlock after the loops
    const uint8_t filled[] = {0x8B, 0x7C, 0x24, 0x10, 0x8B, 0x47, 0x04};
    if (!patch::check(FILL, expected, sizeof expected) || !patch::check(FILLED - 4, filled, sizeof filled)) {
        patch::log("skipped the light fill: unexpected bytes");
        return;
    }
    patch::write_call(FILL, nullptr, 0, reinterpret_cast<void*>(&fill_light), 6, true);
}
