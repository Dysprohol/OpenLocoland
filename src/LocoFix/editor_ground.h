// The ground's picture in the editor (editor_ground.cpp): painting it with textures sampled from the
// campaign grounds, lowering it for water, and saving it as the map's own image.
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// The picture's height for a GRID level: the campaign grounds hold 1026 + 8 a unit (l06 and l13 at 0:
// 1025-1027, at 70: 1586, at 144: 2181); objects and water are drawn against these depths.
inline uint16_t picture_height(int level) { return static_cast<uint16_t>(1026 + 8 * level); }

// A stroke's rectangle of the picture before and after it (one step of the history).
struct PaintPatch {
    int x = 0, y = 0, w = 0, h = 0;
    std::vector<uint16_t> colour_before, height_before, colour_after, height_after;
};

struct GroundTexture {
    std::string key, english;
    uint16_t mean = 0;   // its average colour (R5G6B5), for the swatch
    bool available = false;
};

// Makes the held level's ground paintable: its picture read from the image of the map file `map`,
// drawn centred on the ground object at (x, y) with the live image `vid`. False (with the reason
// in `why`) when the image cannot be read.
bool ground_prepare(const std::string& map, float x, float y, void* vid, std::string& why);
bool ground_ready();
// The level goes: everything forgotten.
void ground_reset();
// The textures (game folder's editor\textures.tsv), sampled at the first use.
const std::vector<GroundTexture>& ground_textures();
// A texture's 128 x 128 sample (laid mirrored it tiles without seams); false when it is not available.
bool ground_texture_sample(int texture, std::vector<uint16_t>& patch);

// A stroke: begin, any number of steps, end (the patch for the history, null when nothing changed).
void ground_stroke_begin();
// Paints a texture into a circle of the picture (screen coordinates of the map: x, y - z) with a soft
// edge; `restore` puts the original picture back there instead.
void ground_paint(float x, float y, float radius, int texture, bool restore);
// Lowers the picture in a circle to `bed` (a GRID level) with a sloping shore, painting the bed
// with a texture (-1: none); `restore` puts the original heights and colours back.
void ground_lower(float x, float y, float radius, int bed, int texture, bool restore);
std::shared_ptr<PaintPatch> ground_stroke_end();
// The relief (inside a stroke): the picture redrawn where the ground's heights went from `from` to
// `to` (GRID, gw x gh cells; the cells cx0..cx1, cy0..cy1 changed, having been between z_lo and z_hi
// in between) - the ground drawn where it now shows (raised ground higher on the screen, hiding
// what stands behind it), its texels taken from the stroke's start picture, shaded by the new slope
// against the old one, and its height texture raised or lowered with it (8 a level).
void ground_relief(const short* from, const short* to, int gw, int gh, int cx0, int cy0, int cx1, int cy1, int z_lo,
                   int z_hi);
// The picture is the map's own (not a campaign map's maps\lNN\lNN.vid as it came), or was changed.
bool ground_own();
// The image file of a map file's ground is a campaign map's own maps\lNN\lNN.vid.
bool campaign_ground_image(const std::string& image);
// Undo / redo of a stroke.
void ground_apply(const PaintPatch& patch, bool after);
// The picture's height at a screen point of the map (0 outside).
int ground_level_at(float x, float y);
// After the map was saved as `map` (a game path): when the picture was painted, it is written as
// <map's folder>\<name>.vid and the map points at it. False when that failed.
bool ground_save(const std::string& map);
