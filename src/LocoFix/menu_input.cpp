// Script function 201 GetMouseWheel(): the turn of the mouse wheel in this frame.
//
//   extern GetMouseWheel() 201;
//
// (201 is free in maps\script\export.lgc; the menu declares it in menu.lgc, game/maps/
// menu.lgc.patch, and scrolls the list of its custom maps page with it.) The engine keeps the
// wheel in the mouse state - Mouse_HandleMessage stores WM_MOUSEWHEEL's delta / 120 at
// Mouse+0x04 (World+0x234) and Mouse_BeginFrame clears it at the start of every Game_Step,
// before the messages are dispatched, so the scripts of that step see it - but no script
// function read it. restart.cpp's hook of Script_CallExtern passes the number here. Pops
// nothing, pushes the notches: > 0 away from the user (up), < 0 toward the user (down).
#include "patch.h"

namespace {

constexpr uintptr_t SCRIPT_PUSH_INT = 0x442700;   // void __thiscall Script_PushInt(Script*, int)
constexpr size_t WORLD_SCRIPT = 0x14c;
constexpr size_t WORLD_MOUSE_WHEEL = 0x230 + 0x04;

using PushInt_t = void(__fastcall*)(uint8_t* script, void* edx, int value);

}  // namespace

int mouse_wheel_extern(uint8_t* world) {
    const int notches = *reinterpret_cast<int*>(world + WORLD_MOUSE_WHEEL);
    reinterpret_cast<PushInt_t>(SCRIPT_PUSH_INT)(world + WORLD_SCRIPT, nullptr, notches);
    return 0;
}
