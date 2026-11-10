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
    std::vector<uint8_t> texture_before, texture_after;   // the painted texture of each pixel (NO_TEXTURE: the original)
};

constexpr uint8_t NO_TEXTURE = 255;

struct GroundTexture {
    std::string key, english;
    uint16_t mean = 0;   // its average colour (R5G6B5), for the swatch
    bool available = false;
};

// Makes the held level's ground paintable: its picture read from the image of the map file `map`,
// drawn centred on the ground object at (x, y) with the live image `vid`. False (with the reason
// in `why`) when the image cannot be read, or - `no_memory` - when the picture and its copies do not
// fit in memory (then nothing of it is kept, and ground_prepare_failed() says so until the next
// level or ground_prepare).
bool ground_prepare(const std::string& map, float x, float y, void* vid, std::string& why, bool& no_memory);
bool ground_prepare_failed();
bool ground_ready();
// The level goes: everything forgotten (and its memory given back).
void ground_reset();
// Run the level writes the map's copy and loads it, so the picture is read again from the copy: before
// the run, ground_keep keeps what the copy cannot tell - the original picture the right button or Shift
// restores, the textures painted on it, whether it was painted (nothing when the picture was not read) -
// moved out, not copied, and the rest of the picture freed for the run (the level goes next);
// back, once the picture is ready again, ground_take_kept puts that back (ground_kept: there is any).
void ground_keep();
bool ground_kept();
void ground_take_kept();
// The textures (game folder's editor\textures.tsv), sampled at the first use.
const std::vector<GroundTexture>& ground_textures();
// A texture's 128 x 128 sample (laid mirrored it tiles without seams); false when it is not available.
bool ground_texture_sample(int texture, std::vector<uint16_t>& patch);

// A stroke: begin, any number of steps, end (the patch for the history, null when nothing changed).
void ground_stroke_begin();
// Paints a texture into a circle of the picture (screen coordinates of the map: x, y - z) - `strength`
// (0-1) of it a pass, the edge soft or hard; `restore` puts the original picture back there instead.
void ground_paint(float x, float y, float radius, int texture, bool restore, float strength = 1, bool hard = false);
// The texture painted at a screen point of the map (its index; BED_TEXTURE + a kind for a bed's band),
// -1 where the picture is the original. A picture saved before is recognized by its pixels: a texture
// laid at full strength is its sample at the pixel's place (mirrored every 128 px).
int ground_texture_at(float x, float y);
// The gravel bed's band under the track: the four kinds of the missions' bed decals (VID 612 meadow,
// 615 ice, 616 volcanic, 617 desert), the band's colours taken from the top of the decal itself
// (vid\<nvid>.vid, frame 1); a pixel of a band keeps BED_TEXTURE + its kind as its texture.
constexpr int BED_KINDS = 4;
constexpr uint8_t BED_TEXTURE = 200;
constexpr int BED_WIDTH = 28;   // px across: the missions' band (l10's painted one, l02's decals in play)
struct BedKind {
    int nvid;
    const char* key;
    const char* english;
    uint16_t mean = 0;   // R5G6B5, for the swatch and the pipette
    bool available = false;
};
const BedKind* ground_bed_kinds();
struct BedRail {   // a rail of the map (its object's place and direction)
    float x, y, z;
    int direction;
};
// Paints the band of a bed kind along the rails, `width` px across and centred on each rail's line
// (a quadratic curve through its two ends at the rail's height, a ramp's climbing with it); where
// two rails' bands meet a pixel takes the nearer line's. Only the first `paint` rails are painted; the
// others (rails beside them) only keep their own pixels, so a rail painted later meets its neighbour
// without a seam. `strength` and `hard` as ground_paint, `restore` puts the original picture back in
// the band instead.
void ground_bed(const std::vector<BedRail>& rails, size_t paint, int kind, float width, float strength, bool hard,
                bool restore);
// The distance of a screen point of the map (x, y - z) from a rail's line.
float ground_bed_distance(const BedRail& rail, float x, float y);
// A rail's line as ground_bed lays the band along it: up to `max` screen points of the map (x, y - z), the
// count written.
int ground_bed_path(const BedRail& rail, float* x, float* y, int max);
// The bed kind whose colour the picture has around a screen point (a band saved before), -1 when none.
int ground_bed_at(float x, float y);
// Lowers the picture in a circle to `bed` (a GRID level) with a sloping shore, painting the bed
// with a texture (-1: none); `restore` puts the original heights and colours back.
void ground_lower(float x, float y, float radius, int bed, int texture, bool restore);
std::shared_ptr<PaintPatch> ground_stroke_end();
// The last ground_stroke_end had no memory for the patch (it said so in the status line): the caller
// keeps no step of the history for the stroke. Asked once (the answer is taken).
bool ground_stroke_lost();
// The picture's changes into the live image's textures: the brushes and the relief only note the
// rectangle they changed, and a frame writes it once (a texture locked once a frame, not once a dab).
void ground_flush();
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
// Saving (editor.cpp save_map): whether the picture was painted and has to be written with the map;
// the image file it is written as for the map file `map` (a game path) - <map's folder>\<name>.vid,
// never a campaign ground's name; ground_write writes the picture into `file` a tile at a time (the
// temporary file of the save: editor.cpp puts it in place with the map). ground_write may throw
// std::bad_alloc; false when the file could not be written.
bool ground_save_needed();
std::string ground_save_image(const std::string& map);
bool ground_write(const std::string& file);

// Puts a map and its ground image, written whole as temporary files, in place as one step
// (editor_newmap.cpp; nothing replaced and the temporary files gone when it fails).
bool replace_map_files(const std::string& map_temp, const std::string& map, const std::string& image_temp, const std::string& image);

// The test build (LOCOFIX_SAVE_TEST, a define given to the compiler: never in a release) makes the
// n-th pass of a named place fail as an allocation does (LOCOFIX_FAIL, editor_newmap.cpp).
#if LOCOFIX_SAVE_TEST
void memory_test_point(const char* place);
#else
inline void memory_test_point(const char*) {}
#endif

// The clock for the editor's timings (ms, QueryPerformanceCounter).
double ground_clock();
// The time the strokes spent (ms) since the last reset: the relief's drawing, the writes into the
// live image's textures (and how many textures they locked), the brushes' pixels; `reset` zeroes it.
struct GroundTimes {
    double relief = 0, upload = 0, paint = 0;
    int reliefs = 0, uploads = 0, locks = 0, paints = 0;
};
GroundTimes ground_times(bool reset);
