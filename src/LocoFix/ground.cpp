// Script function 81 ReCalcGridZ(int unit).
//
// export.lgc declares it - "recalculate the Z grid for the unit's footprint", a function of the
// authors' map editor - but Script_CallExtern has no case for it: a script calling it logged
// "Unknown extern". restart.cpp's hook of Script_CallExtern passes it here: the cells under the
// unit's footprint (its VID's sizeX x sizeY around it) get the height of the ground under its
// middle, the ring of cells around them half way toward it - what map_tool.py place and the
// editor (editor_edit.cpp) do for a building, so it neither sinks into a slope nor floats above
// one. The heights go through World_SetGroundZ. Pops the unit, pushes nothing.
#include "patch.h"
#include "ground.h"

namespace {

constexpr uintptr_t WORLD_POP_OBJECT = 0x442a40;   // int __thiscall World_PopObject(World*)
constexpr uintptr_t SET_GROUND_Z = 0x421170;       // World_SetGroundZ(world, x, y, z)
constexpr size_t W_GRID = 0x204, W_GRID_WIDTH = 0x208, W_GRID_HEIGHT = 0x20c;
constexpr size_t O_VID = 0x1c, O_X = 0x30, O_Y = 0x34;
constexpr size_t V_SIZE_X = 0x1c, V_SIZE_Y = 0x20;

using PopObject_t = int(__fastcall*)(uint8_t* world, void* edx);
using SetGroundZ_t = void(__fastcall*)(uint8_t* world, void* edx, float x, float y, float z);

template <typename T> T& at(void* base, size_t offset) { return *reinterpret_cast<T*>(static_cast<uint8_t*>(base) + offset); }

}  // namespace

int recalc_grid_z_extern(uint8_t* world) {
    auto* o = reinterpret_cast<uint8_t*>(static_cast<uintptr_t>(reinterpret_cast<PopObject_t>(WORLD_POP_OBJECT)(world, nullptr)));
    const short* grid = at<short*>(world, W_GRID);
    const int width = at<int>(world, W_GRID_WIDTH), height = at<int>(world, W_GRID_HEIGHT);
    if (!o || !grid || !at<uint8_t*>(o, O_VID)) return 0;
    uint8_t* vid = at<uint8_t*>(o, O_VID);
    const float x = at<float>(o, O_X), y = at<float>(o, O_Y);
    const int cx = static_cast<int>(x) / ground::CELL, cy = static_cast<int>(y) / ground::CELL;
    if (cx < 0 || cx >= width || cy < 0 || cy >= height) return 0;
    const int z = grid[cy * width + cx];
    const auto cells = ground::footprint(grid, width, height, x, y, at<float>(vid, V_SIZE_X), at<float>(vid, V_SIZE_Y), z);
    for (const ground::Height& h : cells)
        reinterpret_cast<SetGroundZ_t>(SET_GROUND_Z)(world, nullptr, h.cx * 8 + 4.0f, h.cy * 8 + 4.0f, h.z);
    patch::log("ReCalcGridZ: %u cells at %.0f,%.0f to %d", static_cast<unsigned>(cells.size()), x, y, z);
    return 0;
}
