# Data formats

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

## Data formats

What the loading code shows (`ResFile` methods 0x436590-0x4370b0, `Objects_Load`,
`World_LoadVid`, `Vid_Read`, `Level_Load`, `Level_Save`, object `Action(ACT_SAVE/RESTORE)`),
checked against objects.res and all maps. All numbers are little-endian; the structs the
fields land in are in `re/types.txt`.

**Container** (`.res`, `.map`, `.vid`, demos):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `RES ` (`RIFF` is accepted too, without the record layer) |
| 4 | 4 | size of the rest (file length - 8) |
| 8 | 4 | type: `MAP `, `VID `, `DATA` (objects.res), `DEMO` |
| 12 | | chunks |

A chunk is a four-letter id, a size (bytes after these 8; the next chunk starts at the size
rounded up to even), then a header and records. Header: a dword `flags`; with bit 31 set two
dwords follow (`extra` - 0 in all game files - and the record count), with bit 31 clear (old
files) `flags` is the record count. Each record is a 4-byte size + data. An id may appear in
several chunks; reading record by record (`ResFile_NextRecord`) walks into the next chunk of
the same id, and the order of the chunks matters because the loaders search forward. In all
game files every chunk has the bit-31 header, the size in the container header counts the
chunks without their pad bytes, and the last chunk is not padded.

**objects.res** (type `DATA`): `OBJ ` 687 records, `CNST` one record of 26 ints, `WEAP` 55
records of 0x44 bytes, `SFX ` 164 records (one byte, not identified, + 8 strings with
alternative `wav\...` files). `Objects_Load` reads WEAP into `World.weapons`, then each OBJ
record through `World_LoadVid` and `Vid_Read`. OBJ record (strings end with 0; offsets after
the name count from the byte after its 0):

| Offset | Size | Field (`Vid` offset) | Meaning |
|---|---|---|---|
| 0 | 4 | nvid | object number, < 0x800 |
| 4 | var | name (+0x08) | CP1251 name |
| +0x00 | 4 | spriteType (+0x0c) | `U_...` flags |
| +0x04 | 4 | spriteClass (+0x10) | `B_...` class; 8 = decoration baked into the ground |
| +0x08 | 4 | flags (+0x14) | 0x400 hidden, 0x800 screen coordinates, 0x10000 bobbing... |
| +0x0c | 4 | moveMask (+0x18) | `VID_MOVE_MASK` |
| +0x10 | 3x4 | sizeX, sizeY, sizeZ (+0x1c) | float, pixels |
| +0x1c | 4 | maxHp (+0x28) | |
| +0x20 | 4 | speed (+0x2c) | float, px/s (stored x0.001); 999999 = none |
| +0x24 | 4 | speedZ (+0x30) | float (stored x0.001) |
| +0x28 | 2x4 | acceleration, deceleration (+0x34) | float (stored x1e-6); 999999 = instant |
| +0x30 | 4 | rotation time (+0x3c) | float, ms per full turn (stored as 256/x) |
| +0x34 | 4 | weaponIndex (+0x40) | WEAP record |
| +0x38 | 4 | damageRadius (+0x44) | float |
| +0x3c | 4 | damage (+0x48) | done around on death |
| +0x40 | 3x4 | link offset x, y, z (+0x4c) | float, position of the linked object (turret...) |
| +0x4c | 4 | linkNvid (+0x58) | linked VID (`VID_LINK`) |
| +0x50 | 4 | altitude (+0x60) | float: flight height / max step / bobbing amplitude |
| +0x54 | 28 | - | skipped, always zero |
| +0x70 | 4 | nDir (+0x64) | number of directions |
| +0x74 | 17x4 | animFrames (+0x68) | frames per direction of each animation (`ANI_...`) |
| +0xb8 | 17x4 | animSfx (+0xac) | sound per animation |
| +0xfc | 3x17x4 | childForward, childSide, childZ (+0xf0) | float spawn offsets per animation |
| +0x1c8 | 17x4 | childNvid (+0x1bc) | object created with each animation (`VID_CHILD`) |
| +0x20c | 17x4 | childCount (+0x244) | how many (`VID_NO_CHILD`) |
| +0x250 | 4x4 | gamma R, G, B, A | -255..255 colour shift (+0x288) |
| +0x260 | 3x4 | scale x, y, z (+0x290) | float, 1.0 for almost all |
| +0x26c | var | imageFile (+0x29c) | e.g. `vid\005.vid`; `.bmp`/`.tga` are converted to a temporary .vid |

Objects with the same image file share it (`Clone`). The image (`.vid`, type `VID `) has
`HEAD` (one record of 10 bytes: u16 imageFlags, u16 frameSpeed in ms, s16 frameCount, s16
width, s16 height), then chunks by the VID class that imageFlags pick (the 545 images of the
game: 368 sprites with a palette, 32 without, 137 texture images, 8 colour boxes):

- 0x80 colour boxes (`ColorVid`): `DATA`, one record with a uint ARGB colour per frame.
- 0x20 texture tiles (`GroundVid` - map grounds, cursors, many effects): `SURF`, one record:
  short texture count, then per texture short width, short height, (with imageFlags 8 a
  0x300-byte palette - no file has it), int size and the pixels, 2 bytes each: A4R4G4B4 when
  imageFlags has both 1 and 2, otherwise R5G6B5; with imageFlags 4 a height texture follows
  each colour one (int size + 16-bit heights, same size, counted in the texture count, so
  colour textures are the even ones). `DATA`, one record with the tiles: an unknown int
  (uninitialised memory in most files), texture (index into the textures), offsetX, offsetY,
  width, height (the part of the texture), x, y (where it goes in the frame), next (the
  next tile of the same frame, 0 = last) - ints with imageFlags 0x10 (`GroundTile` as in
  memory), otherwise the int and 8 shorts. Frame f starts at tile f. Most images keep many
  frames in one texture (atlases); a map ground (imageFlags 0x35 or 0x25, one frame) is a
  grid of 256x256 textures listed column by column (tile 1 is below tile 0), each tile
  showing the whole texture its texture field names (the colour textures are the even
  ones, so field 2k is the k-th picture). The original converter sorted them: in every
  ground of the game (l01, l02, l05, l08) tile 0 shows the picture `count / 2` (its field
  is the number of tiles) and tile count / 2 the picture 0; `map_tool.py` follows the
  fields both ways. The ground image is drawn centred on the ground object, which stands
  in the middle of the map (512, 384 in l01, 1024, 768 in l05). The loader would look for a
  `SHAD` chunk next;
  no file has one.
- otherwise software RLE sprites (`SpriteVid`, 16- or 32-bit by the screen depth): with
  imageFlags 8 `PAL ` (256 R, G, B triplets, or with imageFlags 0x10 256 B, G, R, A), then
  `DATA` with one record per frame.

A sprite frame (one `DATA` record) is an int size of the rest, short n, n shadow points (3
shorts x, y, z), short top, short height, then each row from top to top + height - 1: runs
of byte skip, byte count and count pixels, ended by two zero bytes. A pixel is a palette
byte with imageFlags 8 - with imageFlags 4 as well the run holds count shorts of height
first, then the count palette bytes - and an R5G6B5 short without a palette. The game's
encoder was canonical: a run covers every drawn pixel up to the next gap, more than 255
pixels continue in runs with skip 0, a skip over 255 is written as (255, 0) runs, the first
and last row hold drawn pixels and an empty frame is top 0, height 0 - so the pixels and a
mask give the same bytes back (all 11264 frames). A record with size 2 holds the index of
an earlier frame, which is reused (2311 frames); one frame of `vid\344.vid` is cut short.
Frames are ordered by animation (`ANI_...`), then direction, then animation step: frame =
start of the animation + direction x frames per direction + step, where direction =
((angle + 128 / nDir) & 255) x nDir / 256.

WEAP record (0x44 bytes, `Weapon`): 0x00 kind (0/4/8/12/32, unclear), 0x04 flags, for train
cars three floats - 0x08 length (px between the car's front and rear on the rails, 48), 0x0c
mass, 0x10 power (a car with power - mass > 1 is a locomotive; see "Trains and rails") - 0x14
detect range, 0x18 weapon range (0 = none), 0x1c aim (floats),
0x20 reload time ms (guess), 0x24 build time ms, 0x28 life time ms, 0x2c ammo (999999 =
unlimited), 0x30 army of created objects (guess), 0x34 initial `BEH_...` (guess), 0x38 icon
frame in `vid\455icon.vid`, 0x3c target priority (guess), 0x40 float (unknown).

**Editing objects.res** (`tools\res_tool.py`). `unpack <objects.res> <folder>` writes the
chunks as TSV tables (UTF-8, tab-separated, a header row; lines starting with `#` are
comments) and `pack <folder> <objects.res>` builds the file again. Unpack checks that the
tables pack back to the same bytes and refuses otherwise (the Steam file and the one with
the sound fix both round-trip). Columns are the record fields in file order, named as in
`re/types.txt`, with the values as stored in the file (the OBJ table above: speed in px/s,
rotateTime in ms per turn...): ints in decimal, bit fields in hex (`0x...`; decimal is
accepted too), floats in the shortest form that keeps the exact value, `spriteClass` as the
`B_...` name without `B_` (`UNIT`; a number for the classes export.lgc does not name, e.g. 8).

| Table | Rows | Columns |
|---|---|---|
| `objects.tsv` | OBJ, one per object type, in file order (687) | nvid, name, spriteType, spriteClass, flags, moveMask, sizeX, sizeY, sizeZ, maxHp, speed, speedZ, acceleration, deceleration, rotateTime, weaponIndex, damageRadius, damage, linkOffsetX/Y/Z, linkNvid, altitude, unknown_54 (the 28 skipped bytes in hex, empty when zero), nDir, gammaR/G/B/A, scaleX/Y/Z, imageFile |
| `animations.tsv` | the 17-entry arrays of OBJ: one row per object and animation, only those not all zero (10993) | nvid, anim (`ANI_...` without `ANI_`: STAND, STOP_MOVE, GO, START_MOVE, L_ROTATE, R_ROTATE, OPEN, CLOSE, FIGHT, SALUT, STAND_OPEN, LOAD, UNLOAD, WOUND, BIRTH, DEATH, DEATH2; or 0-16), frames, sfx, childForward, childSide, childZ, childNvid, childCount |
| `weapons.tsv` | WEAP (55) | index, kind, flags, length, mass, power, detectRange, battleRange, aim, reloadTime, buildTime, lifeTime, ammo, army, behave, iconFrame, priority, unknown_40 |
| `constants.tsv` | CNST: 26 ints, read into `Options` (`g_options`) by `Options_LoadConstants`, which scales 0, 1, 6, 7, 24 by 0.001 and 2, 3 by 1e-6. Known: 0/1 scroll speed, 3 gravity of shells, 4 hp the repair car gives per second, 5 ammo the arsenal gives per second, 6 speed limit on a broken rail, 9 factory build time unit (guess), 10 unused (DebugMode comes from the .cfg), 13 mouse tip delay ms, 14/15 hp per second regenerated by factories/buildings, 17 time to lay a mine ms, 19 ammo given by a balloon, 24 speed above which collisions and hard stops damage trains | index, value |
| `sounds.tsv` | SFX (164) | index, param (the unidentified byte), file1 ... file8 |

The tables are made from the game's data, so like `re/data` they stay local (unpack them
into `build\`). The repository keeps only changed values, in `game/objects.override/`: TSV
files named like the tables, each with the key columns (`nvid`; `nvid` and `anim`;
`index`) and only the rows and columns that change. An empty cell keeps the value. A row
with an nvid that objects.res does not have adds an object type (the columns it leaves out
are 0 or empty, so it needs at least nDir, the animation frames in `animations.tsv` and an
image; `objects.tsv` is applied first, so `animations.tsv` can give the new type its frames).
VID 938, the side tiles of the state bar, and VID 939, the icons of the editor's bar, are added that way (`game/objects.override`). A row
with the next free index adds a weapon or a sound. `build.py` applies the folder after the
sound fix and prints how many values changed (`res_tool.py
apply <objects.res> <folder> <output>` does the same by hand); an unknown table, column or
key, or a bad value, stops the build with the file and line. For example
`game/objects.override/objects.tsv`:

```
# the small locomotive: more hit points, faster
nvid	maxHp	speed
5	150	200
```

**Map** (`.map`, type `MAP `) - read by `Level_Load`, written by `Level_Save`, chunks in this
order:

| Chunk | Records | Content |
|---|---|---|
| `OBJ ` | 1 | Level-local VIDs as in objects.res (`Objects_Load`). In every mission it is VID 1024, the ground, whose image is `maps\lNN\lNN.vid`; the record goes on after the image file name with the rest of the editor's buffer (uninitialised memory, 816 bytes in l01). |
| `GRPH` | 1 | 20 bytes (`Graphics_ReadMapSettings`): int environment flags (`ENV_...`), 8 bytes gamma, int wind direction and float wind (script `SetWind`). Its presence marks the current format. |
| `HEAD` | 1 | 24 bytes: float width, height, cameraX, cameraY (top-left of the first view), uint time (ignored; the clock restarts at 10 ms; the shipped maps hold the time of the authors' editor session, LocoFix saves 0), int version (10; before 10 the four values were ints). |
| `GRID` | 1 | short ground height per 8x8 pixel cell, row-major, (width/8) x (height/8). Missing or wrong size -> flat. |
| `SPR ` | 1 | Objects, 28 bytes each, until id -1 (`Level_ReadSpr`): int id, int nvid, float x, y, z (ints before version 10), int direction (0-255, 0x40 = quarter turn), int army. Objects are created in this order; the first is the ground. |
| `SPRD` | n | One record per object: int id + what the object's `Action(ACT_RESTORE)` reads; a last record with id -1. |
| `PLAY` | 1 | 4 ints: the Flagman id of each army (0 = none). |
| `GROU` | 1 | Unit groups: member ids and -1 for each group, then a final -1. |

Ids are the object addresses in the editor session that saved the map; while loading they
are mapped to the new objects (`IdMap`, World +0x1e4), so references in commands survive.
Saving skips objects linked to another one (`uplink`: turrets, smoke - their parent recreates
them) and menu objects. SPRD data by class (what `ACT_SAVE` writes):

| Class | Data after the id |
|---|---|
| every object | command count (4) + commands of 16 bytes `act, var1, var2, var3` (`ACT_...`; a stack, the last entry runs first) |
| Unit and derived (classes UNIT, BUILDING, AVIA, CIV_ROBOT, ENGINE, DEPO, CREATURE, BALLOON) | + behave (4, `BEH_...`) + item count (4) + items (2 bytes each, nvids; the factories list what they build) |
| Engine | Unit data + front and rear rail position (16 bytes each: the point's x, y, z and the next point's x, y, z as shorts, distance << 16; `RailPos_Save`) + the ids of the car in front and behind |
| Text | base + behave (4) + source text (zero-terminated) |
| Region | base + 80 bytes (sizes, colours, VID, 6 pairs of object ids) |

In the shipped maps the records are 4 bytes for terrain, objects, rails, decals and plain
sprites, 12 for units, 52 for engines (68 with one command), 84 for regions. After the chunks
`Level_Load` compiles `<map name>.lgc`, binds the script functions `F<nvid>_<ANI>` and
`F<nvid>_DESTROY` (VID event handlers, `Vid.animScript`) and the engine events (`main`,
`TrainNotAmmo`...), and runs the `ANI_BIRTH` handler of every object.

Old format (no `GRPH`, version 6: `menu.map` and `logo.map`, both without objects): `HEAD` =
int width, height, short cameraX, cameraY, uint time (used), int version, then int, 4 gamma
bytes, byte, short (the record has 16 more bytes, not read); `SPR ` entries of 14 bytes (int
id, short nvid, x, y, z, byte direction, byte); `SPRD` read with action 200; `PLAY` (33
bytes in both files) and `GROU` not read.

**Demo** (type `DEMO`): recorded after script `SaveDemo(file)` for the next map loaded,
played when `Level_Load` is given a demo. One record: map file name, uint random seed, uint
start time, then per frame uint game time + the 28-byte mouse state (World +0x230); -1 ends
it. No demos are shipped.

**Editing maps and images** (`tools\map_tool.py`). `unpack <file> <folder>` turns a `.map` or
a `.vid` into text, TSV and PNG files, `pack <folder> <file>` builds the file again, and
`check <game folder>` unpacks and packs every map and image of a game through the disk and
compares the results: all 23 maps and 545 images of the game come back byte for byte (the
Steam folder and the assembled game, about 4 s). TSV files are as in res_tool.py (UTF-8,
tab-separated, `#` comments, floats in the shortest exact form); anything the tool does not
read stays in hex, so nothing is lost. A map unpacks to:

| File | Content |
|---|---|
| `map.json` | the chunk order, `graphics` (GRPH: environment flags, gamma bytes, wind direction, wind), `head` (HEAD: width, height, cameraX, cameraY, time, version; the old format adds the unread bytes as `rest`), `players` (PLAY) and `raw`: the records of chunks the tool does not read, in hex (the old-format PLAY) |
| `objects.tsv` | SPR + SPRD, one row per object in file order: id, nvid, name (from objects.res - only for reading, pack ignores it), x, y, z, direction, army, commands (`act,var1,var2,var3` separated by spaces), behave and items (the Unit data; both empty for classes without it), data (the rest of the SPRD record in hex: engine rail positions, region data) |
| `groups.txt` | GROU, one group per line: the member ids |
| `grid.txt` | GRID, one row of 8x8 cells per line: the ground heights |
| `vids.tsv`, `vid_animations.tsv` | OBJ, the level's own VIDs, columns as `objects.tsv`/`animations.tsv` of res_tool.py plus `trailing` (the bytes after the image file name, hex) |

The object classes that decide the SPRD layout come from objects.res: the one of the game the
map belongs to (found up to three folders above it) or `--objects <objects.res>`. An image
unpacks to `vid.json` (the chunk order, HEAD, the kind and how the files are laid out) and:

| Kind | Files |
|---|---|
| sprite | `frames.png`: every frame in a cell of a sheet (16 columns, cells of the HEAD size, frame i at column i % 16, row i / 16), `frames.tsv`: frame, reference (the frame it reuses; its cell stays empty), shadow (the points as `x,y,z` separated by spaces), raw (a record that is not a whole frame, in hex). With a palette `frames.png` is an indexed image of the palette indices and `palette.tsv` the palette (index, red, green, blue, alpha with imageFlags 0x10; it counts, not the PNG palette); undrawn pixels have `transparentIndex` (an index no frame uses), or, when a sprite uses all 256 (46 files), `mask.png` marks the drawn pixels. `depth.png` (imageFlags 4) holds the 16-bit heights. Without a palette `frames.png` is RGBA (alpha 0 = undrawn). Top, height and the runs are computed from the drawn pixels. |
| texture | `texture_NNN.png` per colour texture (RGBA for A4R4G4B4, RGB for R5G6B5) and `height_NNN.png` (imageFlags 4, 16-bit), or for a map ground one `ground.png` and `height.png` of the whole map, cut back into the textures by the tiles; `tiles.tsv`: tile, unknown_00 (hex), texture, offsetX, offsetY, width, height, x, y, next. The textures of a ground come in pairs - the picture and its height - so the tile of the picture k is the texture 2k (with the wrong number the game cannot blit the height into the z-buffer) |
| color | `colors.tsv`: frame, color (0xAARRGGBB) |

Colours keep their exact values: the 5-, 6- and 4-bit channels are widened to 8 bits by
repeating their top bits, and packing takes the top bits again, so a colour painted in an
editor is rounded down to what the format holds. Indexed images must stay indexed.

The repository keeps changed objects of a map in `game/maps.override/<map name>/objects.tsv`
(`l01` for `maps\l01\l01.map`, `menu` for `maps\menu.map`): the key column id and only the
rows and columns that change, an empty cell keeps the value. A row with an id the map does
not have adds an object (any unused number; it needs nvid, the columns left out are 0 or
empty, behave 0 for the classes with Unit data - engines and regions also need `data`, copy
it from a similar object), a column `remove` with 1 deletes the object. `build.py` writes
such maps into the game from the Steam file with the changes and prints how many values
changed (`map_tool.py apply <map> <folder> <output>` by hand); a bad column, key or value
stops the build. For example (tried in mission 1: the tower left the palace and stood on
the square, the tree grew on the maze):

```
# a tower of the palace moved to the square, a new round tree on the maze
id	nvid	x	y	z
10180224		420	600	70
1	923	520	380	62
```

z is the ground height under the object: the `grid.txt` value of the cell (x / 8, y / 8).

**New maps** (`map_tool.py new <folder> <template.map> [--vid <image>] [--script]`). A map
the game does not have starts as a template: the terrain of an existing map - its ground
object (VID 1024) with the image, GRPH, HEAD and GRID - without the other objects, the groups
and the players; `--script` also writes `<folder name>.lgc`, a mission script that compiles
and runs with the standard scripts (`InitDefault`, the game tactics and the menu) - a whole
mission like the sandbox, with the map editor's blocks for its settings and push lines (see
"Editor mode": `CUSTOM_MAP`, its own texts, the reinforcement, the win and loss checks) - and
its texts (`objectives.txt`, `win.txt`, `loose.txt`, `brief.txt`, `brief.lst`), and `--vid`
points the ground at another image. The script declares the arrays the tactics read
(`start_trains[]={-1}` and the other `start_trains_*`, before `header.lgc`): without them the
defaults of `TrainsTactFunctions.lgc` are 0 and `TrainsTact()` puts an AI train at (0, 0)
(`BUGS.md` F10). The template map shares the ground image of the map it
came from (here `maps\l01\l01.vid`), so the terrain looks the same until a `.vid` of its own
is packed into the game. `--size 2048x1536` makes the map bigger: the ground height of the
cells is flat then, the camera looks at the middle of the map and the ground object moves
to the middle too - the ground image is drawn centred on that object (l01 has it at 512,
384, l05 at 1024, 768), so left at the template's place the picture would be drawn off by
the difference and part of the map would have no ground at all.

**A map to play** - the way the missions do it. `map_tool.py copy <folder> <map> <nvid>` adds
every object of one VID of another map, so the template map takes the rails of mission 1 with
`copy game/maps.new/<name> maps\l01\l01.map 610 --objects <game>\objects.res`. `track` lays a
new line of rails instead: each step of the path must be one step of the rail lattice
(71 px in x and 50 px in y, or their halves) and `--clear` removes the old rails first. The
command validates the geometry the engine needs: each rail must join the one before it the
way `Rail_Action` links rails (see "Trains and rails") - its end at the joint pointing the
opposite way - so a straight run turns only through a curve, e.g. `track
game/maps.new/<name> 70.5,750 141.5,750 212.5,725 283.5,675` (a horizontal rail, a curve, a
diagonal); a horizontal rail followed straight by a diagonal does not link in the game and
is refused. The first and the last point join the rails already there by the same rule (and a
rail that is already there, the same shape at the same place, is refused too), so
a branch starts from a track (`track <folder> 425.5,575 496.5,550 567.5,550` leaves a
diagonal through a curve); a track that goes on past the map edge has negative points, which
go after the options and `--` (`track --z 66 --objects <res> -- <folder> -213.5,1375 ...`;
`--z` because there is no ground under the first rail; a track that continues a rail takes the
height of that rail's point, so it links to a ramp's raised end too). Consecutive steps may not turn sharper than 45 degrees (trains
would reverse at the node), and no rail point may get more than six links, the rails already
there counted (the engine's "Too many links" limit; the rail ends pointing the same way at a
joint share one point, so the limit is per point, not per joint - `rail_point_links`). Every rule
counts the ramps (the rail frames 12-35, one point 11 higher - `rail_of`) and the heights of the
rail points: two rails link only where their points are at the same height. `check-track <map>`
looks at every rail of a map the way the engine links them (checked against the engine's own
links: every point of the 20 missions): a rail laid twice, a rail point with more than six
links, and a joint where several rails end but a rail end has no rail ending the opposite way at
its height (a dead end of two rails, or rails at different heights) - the sandbox and the
editor's tracks pass it; the missions have a few (BUGS.md, Quirks). Every grid cell crossed by a rail
whose ground is at most `FLAT_SLACK` 4 below the track height, or above it (a cutting), is
flattened to the track height and such cells of the one-cell buffer around it are ramped half way
toward it, so there are no cliffs under the track -- the rail sits 4 px below the ground surface,
as in the game's own maps; a rail above the ground (`--z` over the terrain, a track continuing a
raised rail) leaves the ground under it as it is, as the editor's Track tool does (since 2026-11-05;
checked on a copy of POLIGON: a track on its flat ground and one at `--z 88` changed no cell, one
through a hill of 80 cut it to 70 with a ring of 75). Objects go in with
`place <folder> <nvid> <x> <y> --from <a mission map> --objects <game>\objects.res` -- it
copies the object's state (army, behave, items) from the reference map so the level starts.

`game/maps.new/sandbox` is such a map - 2048x1536, a flat ground painted for it (meadow,
gravel under the track, packed earth under the bases) and a track laid out the way mission 5
does it: a factory stands at the dead end of a straight spur that meets the line through a Y
(two curves each way), with a push line from the factory down the spur, and a repair point
stands on a track that trains drive through. The main line runs along the bottom from the
west edge, up the east side and west along the top. The player's base is in the south-west:
the main line comes from past the west edge of the map and goes back past it at the top,
as in the campaign: the reinforcement (VID 5 army 3 locomotive outside the map, `BUGS.md` F12)
is pushed onto the map by the level-entry push line, the level exit lets no train off the map,
and semaphores mark both and the factory exits - red for the way out of the map and into the
factories, green for the way in (checked in the game: the four red posts run animation 10). The factory (direction 128, exit south)
stands at the top of a spur north of the line and the repair point on the west leg of a loop
next to it. Every spur and loop leg meets the line through a Y, so trains reach the loops and
leave them both ways, and the repair point legs are one way, as in mission 5 (north through the
player's, south through the enemy's), with semaphores at their ends. The east line has a passing loop; the enemy factory (direction 0, exit north)
hangs on a spur below the top line, the enemy repair point on the west leg of a loop hanging
from the top line (a Y at each leg, like the player's), and a fort with
two guns stands on the east line below the corner - a gate the trains drive through, so the
player has to break through it to reach the enemy base and destroy it. The enemy (army 1)
patrols the east line and the bottom line with trains (`start_trains`, `TrainsTact`,
`SetCleverEnemyAttack`), more gun trains come from its factory over time (`level_queue_*`),
and the player gets a second reinforcement wave when its train is gone. The queue
(`DepoFree` in `ai_events.lgc`) builds an entry when the enemy factory is free, its time has
passed and fewer than `LIMIT_ATACKERS` trains attack, after rebuilding lost start trains -
so the times are the earliest ones: in a 10-minute game the trains of 240 and 420 s left the
factory at 280 and 456 s. Push lines and patrol
points in the script are screen coordinates (world y minus the 76 of the rail points:
`RailMap_FindNearestDot`), `start_trains_x/y` too (the tactics add the ground height), but
`Action(train, ACT_MOVE, x, y)` of a script takes the world position (`Engine_MoveToPoint`
with screen 0, a 3D search; only the mouse passes screen 1). `#define
SANDBOX_DRIVE_TEST 1` in the script turns the enemy trains off and drives the player's
trains through every part of the track, logging their positions to `error.log` ("DRIVE").

Buildings have 8 directions (nDir 8 for the factory, the repair point and the fort): the shipped
maps turn a factory so that its direction is the way out (0 north, 64 east, 128 south, 192
west), a repair point on its track along it, and a fort is a gate: it stands on the track (0-6
px from it in every shipped map, l10 ... l20), turned along it (0/128 on a vertical track,
64/192 on a horizontal one, 32/96/160/224 on a diagonal), and its guns stand on its wall - at
x +-47, z +92/+93 for direction 0 or 128, at (-32, -24) and (+33, +23), z +93 for 32 or
160. A fort cannot be destroyed: it is a static object without hit points (`U_OBJECT`,
maxHp 0); its launchers (VID 115) are the units. The usual win check `!NoArmyUnits(1)` counts
the VIDs of `ARMY1_NVID_ARRAY` (`constants.lgc`: the trains' cars, the factory 102, the
launchers 115 ...) - not the fort and not the repair point 104.

The main line of a mission does not end on the map: its rails go on past the edge (17 of the
20 missions have rails outside the map), and the point locomotive of the reinforcement (VID 5,
army 3) usually stands out there (l02 at -107, 1327). The script gives the line two push lines
(see "Trains and rails"), the **level entry** - from the point outside the map onto it, with
push_flag 1, so the arriving train is pushed in (l02 `SetPushLine(-107,1203,54,1465,1)`) - and
the **level exit** at the other end, short and pointing into the map with push_flag 0
(l05 `SetPushLine(1952,35,1952,60,0)`), so no train may drive off the map into the hidden end
of the track. **Semaphores** (VID 152 `семафор`, an `OBJECT` with 8 directions, army 0, on the
ground) are posts 16-29 px beside the track where such one-way sections are: a pair either side
of the track at the level entry and at the exit, one at the factory exits and at both ends of the
repair point lines (l05: the pair (82, 1545) and (129, 1544) by the entry, (485, 1198) by the
player's factory line), turned along the track, red or green (below). They only show the way:
no script refers to VID 152, and the semaphores of the rail network are something else - `SetSemaphore(x, y, value, army)`
(231) sets `RailDot.semaphore` of the two nearest points, and 4 + army on both ends of a link
closes it for that army (`RailDot_CanPass`, `Train_Update` stops in front of it). The scripts
use it for mines: an armed mine (VID 86, once the repair car that laid it has moved away)
closes its point for its own army, so its trains do not run over it, and its destruction
opens it again (`F086_0`, `F086_DESTROY` in `ai_default_event.lgc`); mission 3 has calls for
its entry, exit, factories and repair points written and commented out. The post has a green state
(`ANI_STAND`, the default) and a red one (`ANI_STAND_OPEN`), with its light (VID 154 halo,
VID 155 spot: green and red); no code ties it to the rails - the map's author sets it: a red
post has the command `10,0,0,0` (act 10 = `ANI_STAND_OPEN`, an animation run as the first
command when the level loads), and 126 of the 188 posts of the missions have it. A post faces
the trains it is for (its direction points at them) and is red where the push line forbids
their way, green where it lets them go: l05's entry pair is red for the trains heading off the
map and green for those coming in, the posts of the factories are red for the trains that would
drive into them, a repair point line has a green post at its way in and a red one for the way
back. Of the 196 posts of the 20 missions, 192 stand within 90 px of an end of a push line
named in the script (the entries 50, the exits 55, the repair points 49 and the factories 29). VID 831 (`указатель стрелки`) is a decorative
switch sign about 50 px from a junction (6 missions), with no code either.

The mission has its own texts: `objectives.txt` (shown at the start, instead of mission 1's
two lines - the script sets `OBJECTIVES_FILE`, `WIN_FILE` and `LOOSE_FILE`, small
backward-compatible hooks in `basefunctions.lgc` and `menuend.lgc`), `win.txt` and
`loose.txt`, and the briefing of the Esc menu: `BRIEF_LIST` names the pages (`brief.lst`:
`brief.txt`, `brief2.txt` with hints, `hiscores.txt`; in the pack's code page, at most 44
characters a line, CRLF; English at the root of a map of the repository and the other
languages in `lang/<code>/game/briefings/<map>/`, see "Languages") and `BRIEF_HEADER` the title (ASCII - the script is CP1251), hooks in `menutact.lgc`.
Without them a map shows the briefing and the header of mission `LEVEL + 1`. `HISCORES_FILE`
(hooks in `menuend.lgc`) keeps the map's high score table (`hiscores`, written on a win) and
its page (`hiscores.txt`) in its own folder - without it a win went into mission 1's table
(`maps\l01\hiscores`). `build.py` copies a `hiscores.txt` of `maps.new` only while the game
has not written one. It is won when no enemy unit remains (`!NoArmyUnits(1)`) and lost when the
player has no units and no factory (`IsNoDepo()`). `test_game.ps1 -Map maps\<name>\<name>.map`
starts it in a window (LocoFix's test map, `--test-map`), and `Locoland.exe --test-map
maps\<name>\<name>.map` in the game folder starts it the same way by hand, after the launcher
(window or full screen as chosen there); in a `build.py --dev` build `run_map.cmd` does that too:
a double-click starts the sample, `run_map maps\<name>\<name>.map` any map.

**The custom maps page** (`game/maps/menu.lgc.patch`). The strategic map of the main menu has
a second page, "WLASNE MAPY" / "CUSTOM MAPS": the page buttons of the briefings (VID 476 next,
VID 475 previous; also the arrow keys) lead from the map of the campaign to it and back. It
uses only graphics of the game: the empty picture with the button bar (VID 484, like the help
pages) and, for each entry, the lamp of the in-game strategic map (VID 472, the mission dots
VID 468 would show the name of a campaign mission as their mouse tip) with its title in the
menu font; a click on a row (lamp or title) picks it. "EDYTOR MAP" / "MAP EDITOR" (keys
`MenuHeader7` and `MenuCustomEditor` of `strings.ini`; without them the English text is shown) is
the first row, and under it six rows of the list of maps, which has any length and scrolls: the
up and down arrows beside it (the arrows of the player list's difficulty, VID 447/448, as types of
their own, VIDs 940/941 in `game/objects.override`, so their mouse tips say "Przewin liste w
gore/dol" - `MenuVid940AllDir`/`MenuVid941AllDir` - and not the difficulty's; each shows only when
the list goes on that way), the mouse wheel (LocoFix's script function `GetMouseWheel()`, extern
201, `menu_input.cpp`: the engine keeps the wheel's turn in the mouse state but no script function
read it), the up and down keys and Page Up / Page Down. The entries come from `maps\custom.lst`
(ASCII, CRLF), two lines per map - its folder in `maps\` and its title -, which LocoFix writes
from the folders of `maps\` each time the page is made (the script language cannot list a folder:
"Map files" below); the menu shows what the list holds. A map shows its
briefing pages (`maps\<folder>\brief.lst`) under its title, with a previous button back to the
list on the first page, and the start button loads `maps\<folder>\<folder>.map` the way a
campaign mission starts. "EDYTOR MAP" calls `StartEditor(map)` in `menu.lgc`, the one place the
menu starts the editor from: fade, music off, then LocoFix's `StartEditorMode` (extern 200)
starts the game again in editor mode on `maps\template\template.map`, which opens as a new empty
map (see "Editor mode", "The map the editor starts on"). Each map row has an "edit the map" icon
right of its title (VID 945, `Custom maps edit`: the hammer and the wrench of the state bar's atlas,
VID 455 frame 2, as the page's green glyph, made by `patch_data.make_custom_edit_icon` into
`vid\945edit.vid`; dim, lit under the mouse as the lamps are - STOP_MOVE / START_MOVE -, its mouse
tip "Edytuj mape" / "Edit the map", `MenuVid945AllDir`), which starts the editor on that row's
`maps\<folder>\<folder>.map` - POLIGON too, which the editor then saves only under a name of its
own. Its column is not part of the row: a click there is the icon's (`MenuVid()` 945, the row by
its y), the rest of the row still shows the briefing.

`CUSTOM_MAP` (the map's own file, `#define CUSTOM_MAP "maps\\sandbox\\sandbox.map"` before
`header.lgc`) marks a map that is not a campaign mission, hooks in `menuend.lgc` (the root and
the `lang/pl`, `lang/en` versions), `menutact.lgc` and `initdefault.lgc`: a win does not
unlock the next mission (`l<player>`), does not write the mission score `s<player>l<LEVEL>`,
the rank or the hall of fame (`maps\hiscores`) - the end screen shows the map's score, its own
high score table (`HISCORES_FILE`) and the player's unchanged total; "restart the mission" in
the Esc menu loads the map itself instead of `maps\l<LEVEL+1>`; and `InitDefault` sets the
menu's `return` flag to 2 instead of 1, so the main menu comes back on the custom maps page
(an older menu reads 2 as 1). The sandbox and the template define it. Checked in the game
through the menu: the page, POLIGON's three briefing pages and back, the sandbox loading from
its start button and its script compiling, the restart in its Esc menu, "EDYTOR MAP" loading
the template, mission 1 starting from the first page after a visit to the second, and the
page opening at the menu's start when the flag matches, and - with the player data in
HKCU (`BUGS.md` F15) - the return: after the sandbox, "Wyjdz do menu glownego" of its Esc menu
opens the main menu on the custom maps page, and so does a new start of the game straight
into `maps\menu.map` after a run of the sandbox (the flag 2 read from the registry).

**Map files** (`src/LocoFix/custom_maps.cpp`). A custom map is a folder of `maps\` that holds
`<folder>.map` - with its script `<folder>.lgc`, its texts, `brief.lst` and, with a picture of its
own, `<folder>.vid`; nothing else makes it one. The page lists every such folder, however it came
(saved in the editor, imported, copied in by hand, renamed in Explorer), but the campaign's `lNN`,
`script`, `outro`, the template and the editor's scratch (`_new*`, `_run`, a folder marked with
`editor_new.txt`): the menu's script calls LocoFix's `ListCustomMaps()` (extern 204, free in
`export.lgc`) in `ReadCustomList`, which writes `maps\custom.lst` anew when it changed - POLIGON
first, then the maps in the order the list had them, then the new ones by name; a folder is written
as spelled on disk and compared without case, so a map is never listed twice and a folder that is
gone leaves the list. The title is the script's `// title:` line, else its `BRIEF_HEADER`, else the
folder's name, in ASCII capitals (the menu's font, the same list in every pack; the briefing pages
have no header of their own, so `brief.lst` gives none); two maps of one title show as TITLE, TITLE 2 ....
While the page is open, `CustomMapsChanged()` (205: a change notification on `maps\` and its
subfolders - a file or folder added, removed or renamed) makes the page again, so a folder copied in
or deleted shows at once. Before a row's briefing, its edit icon or the briefing's start button,
`CustomMapProblem(folder)` (206: the reason in the pack's words, in capitals, or "") checks the map:
gone, its folder's name not ASCII, the `.map` unreadable, its ground image or its script missing; the
reason shows on the line under the list (`CUSTOM_MSG_Y`) and the page is made again, so a ghost
leaves it - before, a row of a missing folder showed an empty briefing and its start left the game on a
black screen (the user's `custom.lst` had kept `map1` after the folder became `point`). Any level
whose `.map` is not there loads the main menu instead (`custom_maps_level_guard` in the `Level_Load`
wrapper of `frame_limit.cpp`). A map whose ground image is missing plays on a black ground, so the
page refuses it, and the editor says so in the status line and asks before saving it: the engine
holds a black ground of its own then, which a save does not write - the saved map has no ground at
all (checked: a copy of `village` without its `.vid`, saved: 164 objects instead of 165, no OBJ; the
user's POINT lost its picture so, `point.vid` lying unused next to it).

A folder whose files or paths carry another name - `maps\<new>\<old>.map` (the only `.map` of a
folder without `<new>.map`), or a script whose `CUSTOM_MAP` names `maps\<old>\` - is brought in line
when the list is made, as an import under a new name does it (`exchange_read_folder` and
`exchange_renamed` of `editor_exchange.cpp`): the files `<old>.*` renamed, the folder in the paths of
the script, the lists and the texts, the ground record when its picture is one of the folder's files,
and a title or briefing header that was the old name in capitals becomes the new one's - the new files
written first, then the old names removed; a folder written to in the last 3 s (a copy under way) is
looked at again 3 s later. So a map renamed or copied by hand, a package unzipped by hand and an
imported one end the same (checked: a package imported as `point_2` and unzipped by hand as `point_4`,
every file the same but the names).

The names (`map_name_problem`, the one rule for File > New, the save dialog, Rename and Import): 1 to
64 characters (the paths stay far below MAX_PATH), printable ASCII (the script and the list keep the
name as bytes, which another code page reads as other letters), none of Windows' `\ / : * ? " < > |`,
no space at either end and no dot at the end, not a device name (CON, NUL, COM1 ...), not a folder of
the game (`lNN`, `script`, `outro`, `sandbox`, `template`, `_new...`, `_run`). Capitals, spaces,
dots and signs are fine: a folder with a space was checked in the engine (its script, texts,
briefing and the page). Before, a name was small letters, digits and `_`, at most 24 - none of it
needed. Every map the editor saves is `maps\<name>\<name>.map`, whatever folder the save dialog was
in (before, only a name typed into `maps\` got its folder; one typed into another map's folder or a
campaign folder was saved there, off the page or over the campaign's files).

**Rename or delete** (the Menu panel's RENAME OR DELETE, `editor.cpp`): a dialog with the map's title
and folder, its new name (`IDC_FILES_NAME` 1001, so `test_game.ps1`'s `Q` step types into it), the
rule and Rename / Delete the map... / Cancel, for a map of its own (not a campaign map, POLIGON, the
template or the new map without a name). Rename saves edits first (asked), copies the folder under the
new name with every file renamed and its paths and title fixed (as above; the high scores come along),
checks each file written, opens the copy and moves the old folder to the Recycle Bin once the copy is
loaded; the map keeps its place in the list. A name that breaks the rule or is taken is refused with
the reason, and the old folder stays as it was when anything fails. Delete asks, then the editor goes
on with a new empty map and the folder goes to the Recycle Bin (`SHFileOperationW`, `FOF_ALLOWUNDO`).
A map saved from another (its ground image in the other's folder) gets a copy of the picture before
that folder is renamed or deleted (`custom_maps_keep_grounds`), and Save As takes the picture along
whenever it lies in another map's folder (before: only from the scratch folder). Test mode: `R<name>`
and `X` steps of `test_game.ps1` (WM_COPYDATA 40048 / 40049, no dialog).

**The ground of a map** (`map_tool.py ground <folder> <width> <height>`). The ground of a
map is a picture cut into 256x256 tiles (`ground.png`, its height `height.png` and the
tiles in `tiles.tsv`); the command writes an empty one of the given size, so a map can
have a terrain of its own: paint the picture, then point the map at it with `--vid`. The
tiles of a ground point at the textures in pairs - the picture and its height - so the
tile of the picture k is the texture 2k.

The height of the picture is **1026 + 8 a GRID level** (`map_tool.picture_height`: l02 holds
1025-1028 at GRID 0, l01 1586 at 70; the editor's `picture_height` in `editor_ground.cpp` and
`FLAT_PICTURE_HEIGHT` of `editor_newmap.cpp` are the same). The engine blits it into the depth
buffer and draws every object and its shadow against it, so it has to match the map's GRID:
`ground` writes a flat ground at 70 (1586), the level of a new map. Until 2026-10-28 it wrote 0,
and POLIGON (made with it, GRID 70) had a height of 0 under the whole map - its objects stood
~1500 above the ground the engine saw, their shadows detached from them (the "floating" the
user saw in the editor); its `height.png` is 1586 now. The missions set many objects a few px
below their GRID (factories 9-11, repair points 6-8, semaphores 0-5); setting POLIGON's the same
way was tried and dropped: with the height right, its objects at the GRID's height look as
grounded as the missions' (their shadows attached, side by side with missions 2, 5 and 10), so
the editor and `place` keep putting an object at the ground's height.

**A picture painted as the editor paints** (`map_tool.py paint <folder> <game folder>`). POLIGON's
picture was painted by `map_tool` with colours of its own (a meadow, gravel, earth), which the
editor's pipette could not name and which looked like nothing a player paints. `paint` paints an
unpacked map's `vid\ground.png` the way the Paint tool does (`editor_ground.cpp`, the arithmetic
ported - doubles where the editor has floats, so an edge pixel may differ by a step): the 128 x 128
patches that the game folder's `editor	extures.tsv` names in the campaign grounds (read with
`ground_picture`), laid mirrored at the pixel's place - the grass everywhere, light grass in soft
strokes of 3-6 dabs (an LCG of its own, so the picture comes out the same every time), the sand as
packed earth under the factories, repair points and forts (three dabs of 72 px across each) - and
the meadow gravel bed under every plain rail (`paint_bed`, as `ground_bed`: the band of the decal's
top along each rail's line, 28 px across, 7 px up the screen). The height stays as it is (POLIGON's
1586, its GRID 70). `game/maps.new/sandbox/vid/ground.png` is painted so: in the editor Alt+click
names the grass, the light grass, the sand and the meadow bed, and in play the factories' shadows
lie on the ground as before and `SANDBOX_DRIVE_TEST` reaches its points 0-4 (the train stops at
the west edge).

The repository keeps such maps in `game/maps.new/<name>/`: `build.py` packs the folder as
`maps\<name>\<name>.map`, its `vid/` folder (an unpacked image, optional) as
`maps\<name>\<name>.vid`, and copies every other file (the script, briefing texts) next to
them. `game/maps.new/template/` is what the tool makes from mission 1 - it loads with
`test_game.ps1 -Map maps\template\template.map`, shows mission 1's terrain and has no units
on it: a starting point for a mission of its own (set LEVEL, MONEY and the rest in the
script).

The mission number in the script (`LEVEL`) also selects the briefing of the level
(`maps\l<LEVEL+1>\brief.lst`) and its header text (`MapHeader<LEVEL+1>` in
`strings.ini`), so a map of its own reuses the number of a mission until the script and
the texts are changed.

**Editing a map** (`map_tool.py list <file.map>`, `vids <objects.res>`, `place <folder>
<nvid> <x> <y>`). `list` prints the size, the ground image, the objects with their
positions and the grid; `vids` prints the objects of `objects.res` (the ones to place,
`--grep` filters by name or number); `place` puts an object into an unpacked map: it takes
the id, the position, the direction and the army, the height from the ground (the grid
cell) and copies the state (`behave`, `items`, `data`) of another object of the same VID -
in the map itself or in the map given with `--from` - because the engine reads that state
when the level starts. A unit without such an object is refused; the commands of the source
are not copied, they are the orders of its mission (a move to its own place). Pass `--objects
<game>/objects.res` as well: without it the class of the object is unknown, so the check
cannot tell a unit (an engine, a building - they need the state) from a rail or a rock, and
the tool only warns; a unit placed that way has no state and the level does not start.
`delete <folder> <nvid>` removes every object of one VID (the old rails before a new
track, for example).
