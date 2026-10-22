// Mouse_HandleMessage reads the cursor position of WM_NCHITTEST without its sign.
//
// The engine takes the mouse position from WM_NCHITTEST, whose lParam holds the cursor in
// screen coordinates as two signed 16-bit numbers. Mouse_HandleMessage (0x417a10) splits it
// with `and ecx,0xffff` and `shr eax,0x10` - unsigned - and subtracts the top-left corner
// of the game area (GetWindowRect, the game area since LocoFix window.cpp). Screen
// coordinates are negative on a monitor left of or above the primary one, and Windows puts
// a minimised window at -32000: there x = -31488 is read as 34048 and the game area x comes
// out as 65536 + 512, which the clamp to the view turns into its right or bottom edge. The
// game then scrolls the map toward that edge, sees the HUD bar under the cursor (VID 450)
// and so no unit: a click selects nothing and a right click gives no order. LocoFix test
// mode met it whenever the test window was minimised.
//
// Sign extension instead, with instructions of the same length: `movsx ecx,cx` + 3 nops
// for the x, `sar eax,0x10` for the y (GET_X_LPARAM / GET_Y_LPARAM). Positive coordinates
// give the same numbers as before.
#include "patch.h"

namespace {

constexpr uintptr_t X_PART = 0x417a77;   // and ecx,0xffff
constexpr uintptr_t Y_PART = 0x417a8f;   // shr eax,0x10

}  // namespace

void install_mouse_coords() {
    const uint8_t x_expected[] = {0x81, 0xE1, 0xFF, 0xFF, 0x00, 0x00};
    const uint8_t y_expected[] = {0xC1, 0xE8, 0x10};
    if (!patch::check(X_PART, x_expected, sizeof x_expected) ||
        !patch::check(Y_PART, y_expected, sizeof y_expected)) {
        patch::log("skipped signed mouse coordinates @ %08X: unexpected bytes",
                   static_cast<unsigned>(X_PART));
        return;
    }
    const uint8_t x_fixed[] = {0x0F, 0xBF, 0xC9, 0x90, 0x90, 0x90};   // movsx ecx,cx
    const uint8_t y_fixed[] = {0xC1, 0xF8, 0x10};                     // sar eax,0x10
    patch::write(X_PART, x_fixed, sizeof x_fixed);
    patch::write(Y_PART, y_fixed, sizeof y_fixed);
}
