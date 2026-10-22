// The ground height grid (World.grid: a short per 8 x 8 px cell): flattening the ground under a
// building, as map_tool.py place does it (_flatten_footprint) - for the editor (editor_edit.cpp)
// and the script function 81 ReCalcGridZ (ground.cpp).
#pragma once
#include <cmath>
#include <set>
#include <utility>
#include <vector>

namespace ground {

constexpr int CELL = 8;

struct Height {
    int cx, cy;
    short z;
};

// map_tool.py _cell_range: the cells covered by [centre - half, centre + half].
inline std::pair<int, int> cell_range(double centre, double half) {
    return {static_cast<int>(std::floor((centre - half) / CELL)), static_cast<int>(std::floor((centre + half) / CELL))};
}

// The new heights of the cells under a footprint (size_x x size_y around x, y) and of the one-cell
// ring around it: the footprint gets z, the ring half way toward z from its height
// (`grid` has `width` x `height` cells, row by row).
inline std::vector<Height> footprint(const short* grid, int width, int height, double x, double y, double size_x,
                                     double size_y, int z) {
    std::vector<Height> out;
    std::set<std::pair<int, int>> cells;
    const auto xs = cell_range(x, size_x / 2), ys = cell_range(y, size_y / 2);
    for (int cy = ys.first; cy <= ys.second; ++cy)
        for (int cx = xs.first; cx <= xs.second; ++cx)
            if (cx >= 0 && cx < width && cy >= 0 && cy < height) cells.insert({cx, cy});
    std::set<std::pair<int, int>> ring;
    for (const auto& c : cells)
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx) {
                const std::pair<int, int> n = {c.first + dx, c.second + dy};
                if (n.first >= 0 && n.first < width && n.second >= 0 && n.second < height && !cells.count(n))
                    ring.insert(n);
            }
    for (const auto& c : cells) out.push_back({c.first, c.second, static_cast<short>(z)});
    for (const auto& c : ring) {
        const int original = grid[c.second * width + c.first];
        const int d = z - original;   // Python's floor division by 2
        out.push_back({c.first, c.second, static_cast<short>(original + (d >= 0 ? d / 2 : -((-d + 1) / 2)))});
    }
    return out;
}

}  // namespace ground
