# The engine: rendering, scripts, pictures, trains, player

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

## Rendering

The renderer is one object, `g_graphics` (struct `Graphics` in `re/types.txt`, created by
`World_Construct`, set up by `Graphics_Init`): DirectDraw 7 surfaces (front buffer, back
buffer, a hardware z-buffer and a system-memory 16-bit depth buffer) and a Direct3D 7 HAL
device. Each frame `GameWorld_Run` calls `Graphics_BeginScene` (restores lost surfaces),
`Graphics_DrawScene` and `Graphics_EndScene`.

`Graphics_DrawScene` draws the world layers 0-10 (`World_DrawLayer`, `Vid.layer`):
- layers 0-1 (map ground, texture tiles of `GroundVid`) as Direct3D textured quads; tiles
  with height data are also blitted into the depth buffer;
- layers 2-4 (RLE sprites, `SpriteVid`) in software, straight into the locked back buffer,
  pixel by pixel against the depth buffer and `g_drawDepth`, clipped to `g_drawClip*`;
- then the scene is closed, the depth buffer is copied into the hardware z-buffer
  (`Graphics_CopyDepthToZBuffer`) and a second scene draws layers 5-10 with Direct3D (alpha
  blending, z test), the shadows, the weather (rain, snow, snow on the ground, fog of
  regions, squall, earthquake - `ENV_` bits from `SetEnvironment` and the map's `GRPH`);
- finally the timed screen effects of script `Effect` (`Graphics_DrawEffects`: `EFF_NUKE`
  flash, `SCROLL`, `FADE`/`FADE_IN`/`FADE_OUT` as blended full-screen quads, `ALPHAAPPEAR`
  fading out a snapshot of the previous screen).

`Graphics_EndScene` presents: `Flip` in full screen, `Blt` of the view rectangle to the
window otherwise. Colours of VID images pass through `Gamma` (multiply + add per channel):
the map gamma (`SetGamma`, `GRPH`) is applied to every VID's palette when it changes, not
per frame. Movies (`PlayMovie`) play through DirectShow in a 640x480 child window over the
game; while one plays the frame is not presented. Debug overlays use `Graphics_DrawLine` /
`Graphics_DrawRect` (2x2 points in the back buffer) and `Graphics_Printf` (GDI text).

**Locks under dgVoodoo.** The engine draws in 16-bit colour, but dgVoodoo draws the back buffer in 32-bit
colour. A lock of the back buffer waits for the GPU to finish what was drawn and gives the CPU a 16-bit copy (each
channel cut to its top 5 or 6 bits); at the unlock dgVoodoo puts back from that copy only around what the CPU
changed: for every pixel whose value changed, the 2 x 2 block with that pixel at its bottom right - the pixel and
its left, upper and upper-left neighbours - gets the copy's 16-bit colour (widened again by repeating the top
bits); every other pixel keeps its finer colour, and a pixel written with the value it had changes nothing (found
in held frames of mission 13 with test pixels written into the minimap's lock: single pixels, runs, a column, the
same value written back). So what Direct3D drew after a frame's last lock is shown with finer colours than the
16-bit picture, and the picture depends on where the locks are: a lock that is moved or left out changes the colour
of a few pixels - those next to software-drawn pixels on a part Direct3D blended after the lock before. A lock in
which nothing changes leaves the picture as it was. A Blt of the back buffer into a 16-bit surface (a texture or an
off-screen surface in video memory) cuts the colour exactly as a lock's copy does, and a Blt back widens it as the
unlock does, without waiting for the GPU (LocoFix fix 58 does the minimap's locks that way). Each lock after Direct3D
drawing costs a wait for the GPU whatever its rectangle (the minimap's 400 x 300 lock as much as the whole
buffer); a lock with no Direct3D drawing since the last unlock costs almost nothing. The depth buffer is a
system-memory surface, but after a Blt into it (the ground's heights, `Graphics_Clear`'s depth fill) its next lock
waits as well (LocoFix fix 56). A Blt from a system-memory surface has dgVoodoo upload the whole surface, whatever
rectangle is copied: a 1920 x 1080 one 1.2 ms a Blt in the test rig, one of a text's size 0.02 ms (LocoFix fix 60).

**Lost surfaces** (`src/LocoFix/lost_surfaces.cpp`, `BUGS.md` F22). DirectDraw takes the surfaces
away whenever a full-screen game is not the window in front - Alt+Tab, a window or a notification
over it, the lock screen; in a window a display mode change does it - and dgVoodoo reports it as
the real DirectDraw does: `IsLost`, `Lock`, `Flip`, `Blt` and the Direct3D calls return
`DDERR_SURFACELOST` (0x887601C2), and `RestoreAllSurfaces` fails with `DDERR_NOEXCLUSIVEMODE`
(0x887600E1) until the game is in front again. The engine's only answer is in
`Graphics_BeginScene`: front or back buffer lost -> "Restore surface", `RestoreAllSurfaces`,
`Graphics_ReloadPalettes`, and the frame goes on whatever the result. On surfaces it could not
restore, the Direct3D calls fail ("3dBeginScene for PreTact", a "DrawPrimitive" error per call) and
the lazy software locks (`Graphics_LockBackBuffer`, `Graphics_LockDepthBuffer` and the copies of
them inlined in `SpriteVid_Draw` and the other software drawing: lock when `backBufferBits` /
`depthBits` is 0) log "Couldn't lock", leave the pointer 0 and draw through it: an access violation
in `SpriteVid_Draw` (0x4516B9, 0x451D74), "EXCEPTION!" in `error.log`, and the engine's exception
filter (0x419820) faults on top of it. LocoFix now:
- checks the front, back and depth buffer before every frame (its own call of `Graphics_BeginScene`
  in `GameWorld_Run`, 0x4236b8): when one is lost it calls `RestoreAllSurfaces` and
  `Graphics_ReloadPalettes` itself, and while that fails it returns 1, which `GameWorld_Run`
  already takes as "no frame" (the original never returned it): nothing is drawn or updated, the
  game time is held as the engine holds it for an inactive game (`g_gameTime = g_prevGameTime`) and
  the loop sleeps 10 ms. The frame after a successful restore draws the whole picture again (the
  textures keep their pictures under dgVoodoo; nothing reloads them);
- guards `Lock` and `Unlock` of the surfaces' `IDirectDrawSurface7` vtable (dgVoodoo's, found from
  `g_graphics`' surfaces at the first loading frame): a lock from the game's code or LocoFix's that
  fails - a surface lost in the middle of a frame - gets a stand-in buffer of the surface's size and
  `DD_OK`, so that frame's software drawing goes into it, and its `Unlock` ends the stand-in; the next
  frame restores the surfaces. Any other caller gets the real result.
`LocoFix.log` has one line when the surfaces are lost and one when they are back ("restored (N frames
skipped, ms, locks given a stand-in)"); `error.log` gets none of the failed frames' errors.
Test mode (`LOCOFIX_TEST`) keeps the game active behind other windows, so a full-screen test game
that is not in front skips its frames until it is.

**The fog buffer** (`src/LocoFix/fog_buffer.cpp`, `BUGS.md` F23). `Graphics_Init` makes three
256x256 scratch textures: the grey one (+0xbd4, P8 with a grey palette, R5G6B5 without P8), the rgb
one (+0xbd8) and the alpha one (+0xbdc). `Graphics_DrawFog` (a region's fog) and
`Graphics_DrawGroundSnow` (`ENV_GROUND_SNOW`) build a mask from the depth buffer at a quarter of
the screen's resolution in the grey texture - the fog the visible part of its region, the snow the
whole screen - and draw it over the screen; both step through it with the pitch its lock returns,
and `Texture_Draw` / `Texture_DrawZ` divide the texels by the texture's own width and height, so
only the texture's size ties them to 1024x1024 pixels. `Graphics_DrawLight` takes the grey and the
rgb texture in turn for a light of at most 0x200 to each side, `EFF_ALPHAAPPEAR` draws its snapshot
in 256x256 pieces through the rgb texture and `GroundVid2_Draw` puts one sprite through the alpha
texture: none of them depends on the screen. LocoFix gives fog and ground snow a texture of their
own, put at +0xbd4 for the length of those two calls: the smallest power of two holding a quarter of
the chosen resolution on each side (512x512 at 1920x1080, 1024x512 at 2560x1440, 1024x1024 at
3840x2160; a logical screen is never bigger), made like the grey one (P8 with its palette, or what
`Texture_Construct` falls back to) at the first `Graphics_ReloadPalettes` (the end of
`Graphics_Init`) and again at the later ones (after a surface restore), as the engine remakes the
grey one. Up to 1024x1024 pixels none is made. The grey texture stays 256x256 for the lights, so
their cost stays as it was (a light locks and fills it, or the rgb texture, every time; a first
version that made the grey texture itself bigger measured slower in mission 14 at 2560x1440, though
within the spread of runs on a busy machine). If the texture cannot be made, fog and ground snow
are left out on a screen the grey texture does not cover (logged once) instead of writing past it.
`LocoFix.log` says "fog buffer: 512x512 (P8) for the 1920x1080 screen". No shipped mission turns
ground snow on (no map's `GRPH` has 0x40 and no script sets it).

**The rain** (`src/LocoFix/rain.cpp`, `BUGS.md` F27). `Graphics_DrawRain` draws `ENV_RAIN` (the
missions' `StartRain` of `daynight.lgc`: missions 3 and 6 now and then, 15 by its script) as drops
of two vertices each (a line list, source-alpha blended, z-tested against the ground): a drop of
length 15-65 starts at a depth that it loses as it falls, so it lives for 250-750 px of fall, slanted
by the wind (`g_sinTable[windDirection] * wind`). The engine keeps 250, and a drop made again
starts in the top 40 px - only the 50th and later of one frame anywhere down the view, which steady
rain (about 7 a frame) never reaches. So the rain was made for 768 px: there it thins toward the
bottom (by rows of 64 px from about 0.57 drops near the top to 0.02 at the bottom, simulated), on a
higher view the rest stays dry. LocoFix has the function in C++ with the same steps and the same
calls of `rand()`, and from a view bigger than 1024x768 on gives it as many drops as the original
has per 1024 x 768 px (and the "50" with them), and on a view higher than 768 px a drop made again
starts in the top band only with the chance (768 / height)², else anywhere in the view - so the rain
reaches the bottom (simulated at 1080 px: from 0.49 at the top to 0.17 at the bottom, at 1440 px
from 0.42 to 0.26). Up to 1024x768 it is the original. Falling snow (`Graphics_DrawSnow`, 1000
flakes) wraps round the view and covers it whole; its flakes are not more on a bigger view, so the
snow is thinner there (4.7 times at 2560x1440).

**The lights** (`src/LocoFix/light_fill.cpp`, `light_textures.cpp`). A colour-box VID (class 0x80,
`ColorVid`, layer 8: lamps, fires, glows) draws its light with `Graphics_DrawLight(x, y, z, sizeX, sizeY,
colour)` (the object's screen position and height, the VID's box): nothing for a colour of 0 (`& 0xffffff`),
else half-sizes of 3/4 of the box rounded down to a multiple of 4 and at most 0x200, nothing when the box is
off the view; the scratch texture - the grey one or the rgb one, in turn (`g_lightBufferToggle` flips at
every call, also for the lights it skips) - locked for a rectangle of one texel per 4 x 4 screen pixels (up
to 256 x 256 texels from (0, 0)); per texel two depth samples, at the texel's corner and 3 px right and down,
the lower height `v` of the two (`depth >> 3` - 128) against the light's: `d = (v - z)²/4 + (v - k + dy)²·9/4
+ dx²` (`k` = 2z/3 + 8, `dx`, `dy` the texel's offset from the middle), brightness `256 - d / (w·h/500)`
clamped to 0..255, written as a byte into the P8 grey texture or as `grayColors[brightness]` into the 16-bit
rgb one; a texel whose both samples are off the view keeps what the texture had. Then unlocked and drawn over
the screen inset by one texel, blended `DESTCOLOR, ONE` (the screen times 1 + the light) at the light's depth,
z-tested. So a light brightens the ground and the sprites in front of the light's height, more where they are
low and near its middle, and the P8 and the 16-bit texture give it 256 and 32 levels of grey: with an odd
number of light calls a frame, each light changes texture from one frame to the next and its brightness
steps by a few levels with it (invisible in motion; it shows in pixel comparisons of a held frame).
Its cost under dgVoodoo was mostly not the fill but the reuse of the two textures: one locked again while
the GPU still draws it makes the next draw wait for the GPU and upload it (LocoFix fixes 54 and 55 in
`changes.md`).

Worth knowing when changing it: `Graphics_Init` always clears the flags 0x10 (Windows
cursor drawn with GDI), 0x20 (software device) and 0x40 (triple buffering), so those paths
are dead code. Effects 4 (`OPEN_WINDOW`) and 8 (`SCALE_WINDOW`) have no drawing code;
6 (`PIXELAPPEAR`) and 7 (`LINESHIFT`) only stop `Graphics_EndScene` from presenting until
`Effect(0)`. Script `SetGraphDetail` (139) does nothing.

## Script machine

What `Script_Load`, the compiler (`Script_Compile` 0x4388e0 ... 0x43c6f0), the interpreter
(`Script_CallFunction` 0x43c940) and `Script_CallExtern` (0x43e5c0) show; structs `Script`,
`ScriptValue`, `ScriptSymbol` in `re/types.txt`.

**Loading.** `Level_Load` gives `Script_Load` the map's `lNN.lgc`. A file whose first dword
has a zero high byte is taken as precompiled (stack size, the values, code size, code); the
game ships only source, which `Script_Compile` turns into bytecode in one pass: a recursive
descent compiler working straight on the text (no token list). `Script_SkipSpace` is the
lexer: it skips comments, evaluates `#ifdef/#ifndef/#else/#endif` and expands `#define`
names in place; `#include "file"` is compiled where it stands. Errors go to the log as
`LOGIC '<text>' line <n>` with the source around them; almost every error then ends the
game (`exit(1)`).

**Language** (as compiled, not as documented in the scripts): `int` and `string` variables
(`int x[5] = {..}`, `int *p`), functions `Name(int a, string b = "x") { ... }`, `extern
Name(params) <number>;`, `if/else`, `iff`, `while`, `do-while`, `for`, `break`, `goto` and
labels, `return`, `sizeof`, `'c'` and `0x` constants. A backslash in a string is not a path
separator: `"music\mus08.ogg"` reaches the game as `musicmus08.ogg` (the sound log says so),
so a path needs two - `"music\\mus08.ogg"` (`BUGS.md` F11). Expressions: unary `- ~ ! & ++ --`,
then `* / %`, then `+ -`, then one level for `>= >> > <= << < == !=`, then one level for
`^ && & || |` (left to right; `&&` and `||` always evaluate both sides). `?:` is rejected.
`iff (cond) body` runs the body the first time the condition is true and never again: the
interpreter overwrites the start of the statement with a jump past it.

**Values and variables.** The machine has one stack of 12-byte `ScriptValue`s (flags, int,
String). Every variable - global, function parameter or a variable declared in a function -
is a fixed slot at the bottom of that stack, numbered by the compiler; temporaries and call
frames go above them. So functions are not reentrant (a recursive call overwrites its own
parameters); names declared inside `{ }` go out of scope at the closing brace, their slots
stay. The whole script state is the stack: demos record it after loading the
level and restore it on playback. Flags: 1 string, 2 int, 4 array, 8 initialised, 0x10
object reference (cleared when the object is deleted), 0x20 pointer, 0x40 not initialised.
Strings convert to numbers with `atoi` (or hex for "0x..."), + of two strings concatenates,
== and != compare strings by text, division by zero gives 0xfffffff.

**Calls from the engine.** `Level_Load` looks up the functions named `F<nvid>_<ANI>`,
`F<nvid>_DESTROY` and the engine events (`main`, `TrainNotAmmo`... `g_scriptEventNames`);
`World_CallScript` runs one through `Script_CallFunction(function, object, arg)`, which
stores object and arg in the function's first two parameters. The return value is 1 when
the script destroyed that object (`Destroy` on it), so the caller stops using it.

**Bytecode.** One opcode byte, some with an operand. `<i32>` is a little-endian int;
jumps are relative to the position of their operand; `slot` is a stack slot number.

| Opcode | Operand | Meaning |
|---|---|---|
| 0x01 | i32 | push int |
| 0x02 | zero-terminated text | push string |
| 0x03 / 0x04 / 0x05 | | top = -top / ~top / !top |
| 0x06-0x17 | | pop right, apply to left: 06 `/`, 07 `%`, 08 `+`, 09 `-`, 0a `^`, 0b `\|`, 0c `&`, 0d `==`, 0e `\|\|`, 0f `>`, 10 `<`, 11 `>=`, 12 `<=`, 13 `*`, 14 `!=`, 15 `&&`, 16 `>>`, 17 `<<` |
| 0x18 | i32 | pop; jump when 0 (`if`, loop conditions); the stack is reset to the frame |
| 0x19 | i32 line | end of statement: reset the stack to the frame ("stack error" in the log when more than one value was left) |
| 0x1a | | pop |
| 0x1c | i32 | jump |
| 0x1d | i32 | `iff`: pop; jump when 0, otherwise turn the statement into a jump past it |
| 0x1e | i32 code offset | call a script function: push the frame base and the return address, new frame |
| 0x1f | | return: take the value on top (if any), drop the frame, pop the return address and the caller's frame base, push the value |
| 0x20 / 0x21 | slot | push variable, then ++ / -- it (x++, x--) |
| 0x22 / 0x23 | slot | ++ / -- the variable, then push it |
| 0x24 | slot | push variable (an array without an index pushes its slot number) |
| 0x25 | slot | push the slot number (`&x`) |
| 0x26 | slot | store the top into the variable (stays on the stack); a string variable with an index gets one character |
| 0x27 | slot, op byte | compound assignment `x op= top` with binary opcode op |
| 0x28 | | pop an index for the next variable opcode (`x[i]`; through the slot's value for pointers) |
| 0x41-0xfe | | engine function (extern) with this number: `Script_CallExtern` |

0x00, 0x1b and 0x29-0x40 are not used. Arguments of an extern are pushed in order; the
engine pops them (the last on top) and pushes the result - about 50 functions that return
nothing push nothing, so using one inside an expression reads whatever is on the stack.
The numbers and names are those of `maps\script\export.lgc`. `Script_CallExtern` implements
122 of them; each case carries a comment in `re/c` with the number, the declaration and what
the engine does (`c` lines in `symbols.csv`). Declared but not implemented ("Unknown
extern"): 81 `ReCalcGridZ`, the network functions 192-197, 210 `GetStatistics`, 211
`CalcStatistics`, 248 `PatrolEngineToPoints` - no shipped script calls them.

Quirks found on the way (to keep in mind for a rewrite - scripts may rely on them):
`AddUnitLimit` (249) uses army and value swapped; `TrainProperty` `PROP_FREE` always
returns 1 and leaves a value on the stack per busy car; `FEof` returns 16, not 1;
`CanMoveEngineTo`/`CanAttackEngine` (253/254) are stubs; `SetAutoReBirth` (247) does
nothing and `Exit` ignores its argument; `FirstSprite` looks only at layer 0; `FirstTrain`
clamps the army to 0-3; `GetVidData`/`SetVidData` accept 17 child entries, not the 32 the
comments of export.lgc promise; a positive `SetGamma` byte darkens, a negative one
brightens. Three of them are fixed by LocoFix: `Log` no longer uses its text as a printf
format, `Random(-1)` no longer divides by zero and `a % 0` no longer kills the process
(BUGS.md F6-F8).

## Support library

The small pieces the rest of the engine is built on: the `String`, the error log and the CRC32.
Functions `String_*` (0x4187f0-0x427080) and `Log_*` (0x425bf0-0x426330), `CRC32_Update`; struct
`Log` in `re/types.txt`.

**String** is not a class but an owning `char*`: a function that makes text returns a `String`
(through a `char**` parameter or the hidden result), the empty string is the shared static at
0x4a9d6c (never a NULL), and `String_Destruct` (0x408860) frees it unless it is that static.
`String_Assign` (0x4264c0, from another `String`), `String_AssignCStr` (0x426540), `String_Append`
(0x4265b0), `String_AppendCStr` (0x426660), `String_Format` (0x426930, `sprintf` through a 4 KB
stack buffer), `String_LoadFile` (0x426830, a whole file at once), `String_ReadLine` (0x426790, a
line through the `File` class) and `String_ReadFileLine` (0x426710, from a `FILE*`) fill it. Text is cut with
`String_BeforeSubstring` (0x426da0), `String_AfterSubstring` (0x426e30), `String_BeforeLast`
(0x426f00) and `String_AfterLastSubstring` (0x426fa0); the script compiler trims `" \n\r\t"` off
every line with `String_TrimRight` (0x426c20), and `String_Replace` (0x426c60) turns the colons of a
time into h, m and s for the log and screenshot names. `String_Concat` (0x426450) allocates a + b;
its three wrappers differ in where the two operands come from (a register or the stack):
`String_ConcatStringString` (0x4187f0), `String_ConcatStringCStr` (0x418820) and
`String_ConcatCStrString` (0x418850).

**The error log** is one object, `g_log`, created by `Log_Construct` (0x425bf0) in `World_Construct`:
a struct `Log` (vtable 0x475758, 0x40c bytes) with the window for error boxes (+0x04), the `FILE*`
(+0x08) and the path of this run (+0x0c). The game passes 1, so it opens
`logs\error<YYYY-MM-DD HHhMMmSS>.log` for writing (`String_LocalTime`/`String_LocalDate`, the colons
of the time replaced with h and m); when the `logs` directory is missing - the Steam game has
none; ours has it, see "Logs" - the open fails and the `logs\` prefix is dropped, so the file
lands next to the exe. `Log_Destruct` (0x425ea0) closes it and renames it to `logs\error.log`
(`error.log` without the folder), which is why the last run is always `error.log` and a crashed
one keeps its dated name. The first line is
`----< date time >----< path of the exe >----`. `Log_Write` (0x426080) appends a formatted line;
`Log_Error` (0x426140) writes `!!!ERROR <time>!!!` + the caller's message (its `format` and the
variadic arguments, e.g. `RES '%s' '%.4s'`) + a category picked by `code` from 15 fixed texts
(`0x%X Invalid %s`, filled with the value and the text), and its wrappers
name the module - `Graphics_LogError`, `World_MapError`, `Vid_Error`, `GameObject_LogError`;
`Log_ShowError` (0x425f70) also shows a MessageBoxA "Error" (a level that can not be loaded);
`Log_Overwrite` (0x426000) writes a progress line that the next write replaces; `Log_Fatal`
(0x4260e0) writes, closes and exits with code 1 (out of memory).

**CRC32** (`CRC32_Update`, 0x426400, table `g_crc32Table` at 0x47e0b4) is the checksum the VID
converter (0x428000) computes over the image data, two bytes per step.

**`File`** (vtable 0x47575c, struct `File`) is the engine's file stream: two fields (the vtable and a
`FILE*` at +4) and two virtuals, `File_Read` (0x4277d0, `fread`) and `File_Write` (0x427800,
`fwrite`), which return the number of bytes that could *not* be transferred - a caller tests for 0.
`String_ReadLine` and the script compiler read their text through it; `String_LoadFile` keeps one on
the stack only as a guard that closes the file if something throws.

**The ini file and the registry.** The game reads `strings.ini` (the String `g_stringsIni`, 0x4a9d34)
with `IniFile_GetString` (0x427520, `GetPrivateProfileStringA` into a 4 KB buffer, with `{section:key}`
references expanded into each other) and `IniFile_GetInt` (0x427500, `GetPrivateProfileIntA`).
`Registry_ParsePath` (0x427340) turns `HKEY_LOCAL_MACHINE\Software\...` into the hive handle
(0x80000002) and the rest of the path, which `Registry_ReadInt`/`Registry_WriteInt`
(0x436340/0x436490) then use. `String_ToWide` (0x4274c0, `MultiByteToWideChar`) and `String_Init`
(0x4274e0) are the small helpers of that area; `String_AddToNumber` (0x427080) adds a delta to the
number at the end of a String keeping the zero padding, which is how a picture finds the next
numbered file.

## Pictures and animations

What `Picture*` (0x412e50-0x4294c0), the implementations (`PictureImpl_*`, `PictureTga_*`,
`PictureBmp_*`, `PictureFlic_*`, `PictureZ_*`, 0x4277b0-0x429880) and the `VidConverter` (0x4223f0,
0x429ad0-0x42cb00) show; structs `Picture`, `PictureImpl`, `VidConverter` in `re/types.txt`.

**`Picture`** is a wrapper of three fields (vtable 0x475410): the implementation pointer at +4 and the
format type at +8. `Picture_Construct` (0x429480) makes an empty one (the implementation is 0x42c
bytes, vtable 0x4757a8), `Picture_Create` (0x4293d0) one with a size and a pixel format (24-bit for a
format 1 or 2, 16-bit for 5) and `Picture_Load` (0x4294c0, wrapper slot 3) one for a file: the
extension picks the class - `.tga`/`.TGA` (1), `.bmp`/`.BMP` (2), `.flc`/`.FLC` (3), `.z`/`.Z` (5) -
and anything else is a plain picture of type 0 that logs `!!!ERROR!!!PICTURE '<name>' - Unknown format`.
The other wrapper slots are `Picture_NextFrame` (0x413160) and `Picture_Rewind` (0x413170), which
forward to the implementation, and `Picture_Delete` (0x413180).

**`PictureImpl`** is an encoded file with a palette and exactly one decoded frame: `frames`, `frame`,
`frameTime` (ms per frame, 71 unless the file says otherwise), `width`, `height`, `bytesPerPixel`
(1 palette, 2 = 16-bit 555, 3 = 24-bit, 4 = 32-bit), the name String, the `FILE*`, the 256-entry
palette at +0x24, the frame buffer at +0x424 and the file offset of the frame data at +0x428. The five
virtuals are 0 delete, 1 next frame, 2 rewind, 3 open, 4 free; the format classes override 0, 1 and 3.
`PictureImpl_SetSize` (0x4278f0) allocates the zeroed buffer and `PictureImpl_Free` (0x427990)
releases it and closes the file. The pixel accessors are `PictureImpl_GetColor` (0x427e80) and
`PictureImpl_SetColor` (0x427d80), which honour the depth (the 8-bit pictures through the palette, the
16-bit ones in 555), and the raw `PictureImpl_GetPixel` (0x427bf0) and `PictureImpl_SetPixel`
(0x427ca0); a pixel outside the picture is read as transparent (0xff000000).

**Three ways to animate.** The `.flc`/`.FLC` class is an Autodesk FLIC player: `PictureFlic_Open`
(0x428140) checks the 0xAF11 (FLI) and 0xAF12 (FLC) magic, takes the size, the frame count and the
delay from the header (an FLI speed is in jiffies, so *1000/70 ms; an FLC one is already in ms) and
`PictureFlic_ReadFrame` (0x428250) decodes one frame chunk. The `.tga` and `.bmp` classes get their
animation from the file name: when it ends with a digit, `PictureTga_Open` (0x4289e0) and
`PictureBmp_Open` (0x428ec0) count the following numbered files (`name1.tga`, `name2.tga`, ...), so a
series of single screen grabs plays as an animation. The `.z`/`.Z` class reads Locoland's own 16-bit
`ZBuf` format - a 16-byte header with the magic 0x6675425a ("ZBuf"), then RLE frames decoded by
`PictureZ_ReadFrame` (0x429290).

**Screenshots** are TGAs: `Graphics_SaveScreenshot` builds a `Picture` of the screen size, draws the
HUD text into it with `Picture_SetColor` (0x413120) and writes it with `PictureImpl_SaveTga`
(0x4279d0) to `Screens\<date> <time>.tga` - an 18-byte header, the rows from the bottom up, 24-bit
(32-bit with alpha for a 4-byte picture) and no colour map.

**The VID converter** (0x4223f0, vtable 0x4756bc, 0x430 bytes) turns pictures into the `.vid`
resources of the game: it holds up to three `Picture`s (the image, a second one for the colour box or
the alpha, a third one for the shadow/Z data), the palette at +0x28, an index map at +0x428 and the
HEAD flags at +0x42c. `VidConverter_Load` (0x4298e0) opens them, `VidConverter_GetColor` (0x429c10),
`VidConverter_GetAlpha` (0x429d40) and `VidConverter_GetShadow` (0x429db0) read one pixel of the
combination, `VidConverter_Checksum` (0x429af0) is the CRC32 that lets repeated frames be written
once and `VidConverter_GetBounds` (0x42a200) the box of the drawn part. `VidConverter_SaveVid`
(0x42cbd0) writes the HEAD record and picks a writer by the flags: `VidConverter_WriteColorVid`
(0x42caa0) for the 2x2 colour boxes, `VidConverter_WriteGroundVid` (0x42a7a0) for the 256x256 texture
tiles of a ground image (each tile becomes a DirectDraw surface through `VidConverter_MakeSurface`,
0x42a3c0) and `VidConverter_WriteSpriteVid` (0x42c4f0) for the sprites - an optional `PAL ` record (the
256-entry palette built by `VidConverter_BuildPalette`, 0x42bca0) and one `DATA` record per frame with
the RLE runs, the shadow points (`VidConverter_BuildShadowPoints`, 0x42b670) and the bounding box. The
errors of the picture classes go through `Picture_LogError` (0x42c470) as `PICTURE '<name>'`.

**Compressed frames.** A `.vid` whose HEAD flags carry bit 0x100 is stored arithmetic-coded. The
loader builds an `ImageCodec` object - 0x2808 bytes: a vtable, a mode and 256 frequency models - and
gives it to `ResFile_ReadPacked` (0x436a10), which calls the codec through its vtable slot +0x20;
the converters give one to `ResFile_WritePacked` (0x436a60, slot +0x1c). The class has nine virtuals:
the base vtable 0x475830 (its Encode and Decode are `_purecall`), the real one 0x47580c with
`ImageCodec_Init` (0x403790, the default frequencies of the 256 models), `ImageCodec_Encode`
(0x4037b0) and `ImageCodec_Decode` (0x403900). Records shorter than 10 bytes are copied as they are;
otherwise symbol 0x100 ends the record (the decoder logs `!!!ERROR!!! decode` and exits when the
output would overflow). The mode says how the bytes lie in the buffer and comes from the HEAD flags
(0x800 chooses 1): 2, the usual `.vid` layout, takes the even byte positions first and then the odd
ones.

**The coder** is a range coder: `RangeCoder_StartEncode` (0x402fd0), `RangeCoder_EncodeSymbol`
(0x403010), `RangeCoder_DoneEncode` (0x403120) and, for reading, `RangeCoder_StartDecode` (0x4032e0),
`RangeCoder_DecodeFreq` (0x403340) and `RangeCoder_DecodeUpdate` (0x4033c0). Intervals are 12 bits
wide (1 << 12) and `low` is kept in the shape of a positive float: bits 23-30 are the next byte to
write, bits 0-22 the precision, so a step normalises by writing that byte and shifting `low` and
`range` left, passing carries on as runs of 0xff (0x00 for a borrow). The first byte of a stream is
always 0 (`ImageCodec_Encode` starts the coder with the pending byte 0 and `RangeCoder_StartDecode`
refuses a stream that starts with anything else).

**The model** (`RangeCoderModel_*`, 0x403400-0x403760) has 257 symbols (0-255 and the end marker), a
frequency table, a cumulative table and an index for the decoder's binary search
(`RangeCoderModel_GetSymbol`); it rescales itself (`RangeCoderModel_Update`) once the symbols have
been counted `totalFreq` times - every frequency is halved, the total doubles up to 2000 and the
rounding error is spread over the next updates. One model per value of the *previous* byte makes the
coding context-sensitive, and `ImageCodec` keeps those 256 models in one array (built by the CRT
vector constructor iterator from `RangeCoderModel_Construct` and `RangeCoderModel_Destruct`).

## Trains and rails

What `Engine_*`, `Train_*`, `RailDot_*`, `RailMap_*`, `RailPos_*` and `Depo_*` show (0x404640-0x40c2d0,
0x4325d0-0x4355b0); structs `Engine`, `RailDot`, `RailLink`, `RailPos`, `RailMap`, `TrainStats`,
`Depo` in `re/types.txt`.

**The rail network** (`g_railMap`, "R_MAP" in the log) is a graph of points (`RailDot`, "R_DOT").
Every rail object (B_RAIL) puts two points when its direction is set (`Rail_Action`
ACT_CHANGE_DIRECTION): not at its ends but a quarter of the sprite size from its centre
toward each end (12-direction tables of +-1 at 0x47e908/0x47e938 and 0x47e998/0x47e9c8,
times sizeX/sizeY and 0.25; z + the VID's altitude), links them, and links each one to the
point it finds three quarters out (scale 0.75) - which is the near point of the next rail
only when that rail's end at the joint points the opposite way. So rails join where their
end vectors are opposite: a straight horizontal rail and a straight diagonal that touch are
not linked, the curve shapes join them (mission 1 never puts a horizontal straight next to
a diagonal). Points closer than 2 px with the same z are shared (reference counted; the
coordinates are truncated to whole px, `RailMap_FindDot` takes |dx| and |dy| under 2), and the look-out
finds a point the same way - so a rail laid half a rail along over another merges into it (The Factory's
1739.5,725, MAP_RULES.md 3 "Where rails link"), and rails moved off the lattice link within 2 px. This
is how a branch works: two rails with the same centre (a straight and a curve) share their
first point. The shape and the height come from the frame of the direction,
`((direction + dirHalfStep) & 255) * nDir >> 8` (3 and 37 for VID 610; the flat shapes are
frames 0-11, the directions 0, 6, 13 ... 76): frame % 12 picks the end vectors, frame / 12 the
group. The height of the points: z + the VID's altitude + an offset from two tables
picked by the group - zero in the exe (.bss, 0x4a9d84 and 0x4a9d94), and
`Rail_Action` stores sizeZ in the second entry of the first and the third entry of the second
on its first call - so groups 0 and 3 are flat, group 1 raises the point of end 0 and group 2
the point of end 1 by sizeZ, 11 (ramps; l11 and l15 climb with them, e.g. direction 179 is
frame 26: a diagonal rising toward its end 1). The flat rails of the maps put their points at
z + 10: 76 for rails at 66, as the saved engines of the game's maps store them. The look-out
point of a rail end has the height of its own point, and `RailMap_FindDot` wants the same z, so
rails link only where their points are at the same height; the rail ends pointing the same way
at a joint share one point (a junction's point: the far points of its rails and the point
across). `map_tool.py rail_of`, `rail_point_links` and `check-track` model this; on missions
11, 12 and 15 the links logged in the game (the editor's test command `D`) are exactly these.
A rail links only through its own two look-outs, when its direction is set (laid, loaded, put back):
`RailDot_Connect` adds the link both ways, but nothing looks again from the rails already there, so the
look-out of a rail laid earlier never finds a point of one laid later. Ends meeting the opposite way find
each other's point both ways, so there the order does not matter; where only one look-out finds the
other's point - an end in the middle of a curve, two diagonals whose tips are a lattice step apart, a
rail laid half along past another's end - the link is there or not by the rails' order (loading: the
map's order). Found on a tester's map (issue #24, CaptureTheGear 745.5,250: a diagonal ending in the
middle of a curve laid after it, its point 763,237 with one link only, `D`). The editor's marks lay the
rails on a copy of the network in the map's order and ask it (`rail_net.h`, editor-objects.md "The
network's answer"); on the 20 missions that copy and `g_railMap` give the same answer at every rail end.
A point has at most 6 links (a 7th is dropped on the full side only - the other point keeps its half,
"R_DOT: Too many links"); a link stores the other point, its
length (y counts 1.5 times), the index of the way back and its direction. After a level is
loaded `RailMap_FindCrossings` marks links that cross another track (a grid of 75 px cells),
because a moving train there blocks the other track. Per point: `broken` (a destroyed rail
here), `semaphore` (script `SetSemaphore`; the value 4+army on both ends of a link closes it
for that army - the AI scripts use it to hold trains), `pushLink`/`pushFlag` (script
`SetPushLine`: a one-way line - no train may enter it against its direction; with
push_flag the train on it is pushed: factory exits) and `occupant` (the car standing there).
The rail debug view (World flag 0x2000) draws all of it.

**A train** is a chain of `Engine` objects (B_ENGINE - locomotives, wagons, the repair car
VID 85, the arsenal VID 45, the brander VID 97 ...) through `prevCar`/`nextCar`; the first car
(no prevCar) drives it, the others only follow (`Train_Update` returns at once for them). A
car has two rail positions, `front` and `rear` (`RailPos`: the point left, the link, the
distance along it), `Weapon.length` (48 px) apart; it is drawn halfway between them and turned
along them (`Engine_UpdatePosition`). A locomotive is a car whose `Weapon.power` exceeds its
`Weapon.mass` by more than 1. `TrainStats_Compute` sums a train: power, mass, the speed of the
slowest locomotive reduced by the load (PROP_SPEED), hp, firepower, ammo - `TrainProperty`
returns these, `Engine_Move` sends the script events TrainNotAmmo (ammo below 11 %) and
TrainDamage (below half hp) once a second.

**Commands** (`Engine_SetTrainCommand`, set on every car): COM_ENGINE_MOVE (23) to a point
(`goalDot`; ACT_MOVE takes the nearest point), COM_ENGINE_MINE (24) - to a point where the
repair car lays a mine (VID 86, after `Options.mineLayTime`), COM_ENGINE_PATROL (25) between
`patrolFrom` and `patrolTo` (swapped at each end), COM_ENGINE_LINK (26) - couple to the goal's
train, COM_ENGINE_CLASH (27) - ram it, COM_ENGINE_ATTACK/ONEATTACK (28/29) - drive to shooting
distance of the goal (the shortest range of the train's turrets) while the turrets get
COM_ATTACK/COM_ONEATTACK. ACT_ATTACK, ACT_LINK_ENGINE (150), ACT_CLASH_ENGINE (151),
ACT_FORCELINK_ENGINE (152) of an engine map to these.

**Driving** (`Train_Update`, every frame for the first car): the train accelerates toward
`targetSpeed` (`Engine_Accelerate`; `Train_StartRoute` sets it to the stats speed, its sign is
the direction), the front advances along its link (distance + a 16-bit fraction), and at each
point `RailPos_FindRoute` chooses the next link by a route search: `RailMap_BeginSearch`
resets the costs, `RailDot_Search` goes depth first over the links, keeping the best cost per
point and incoming link. A link is allowed by `RailDot_CanPass` (semaphore of the army,
another train standing on the next point unless it has the same goal, push line); a turn
of 0x20 or more from the link it came on (`Math_DirectionOfVector` of the points' dx, dy; rails linked end
to end turn at most ~35 degrees, 25 of 256, at a point - only a V, two rails sharing a point, makes a
train turn round between them) means reversing, which needs room for the whole train behind a switch
(`RailDot_HasRoomToReverse`) and costs the train length. The route ends at the goal point,
at a car of the goal train or at shooting distance; when the goal cannot be reached the point
nearest to it is used. The route is kept in `Engine.path` (the player's train shows it with
VID 603 markers, `Train_ShowRoute`). A negative result reverses the train (`Train_Reverse`
swaps the chain and the ends of every car). Arriving stops the train (`Train_Stop`) and sends
TrainArrive; a broken point limits the speed to `Options.brokenRailSpeed` (smoke VID 588) and
the rails around it are repaired when the train passes (`Rail_RepairAt`) - only the repair car
keeps its speed. Then `Train_HandleContact`: touching another train couples them
(`Engine_CanCoupleWith`: one links to the other, or a pushed train of the same army) - an
enemy car coupled to a locomotive changes sides (EnemyLinked) - or collides (`Train_Collide`:
the speeds are averaged by mass; ramming above `Options.crashSpeed` damages the hit cars,
halving along the train; TrainClash). Finally `Train_MoveCars` moves every car behind the
previous one and runs over objects with VID flag 0x4000 (5 damage).

**How the cars follow** (`BUGS.md` F24, `train_fold.cpp`). The cars keep no path of their own. A
car's two ends are `RailPos`es: the front end counts from the point behind it along the link ahead
(`front.dot` is the point the front left), the rear end from the point on the car's front side along
a link toward the back - so `Train_Reverse` only swaps the two. Each frame `Train_MoveCars` puts every
following car's front end exactly on the rear end of the car in front (the same place, the link the
other way round) and `Engine_UpdateRear` (0x40a310) lays its rear end `Weapon.length` (48 px) behind,
from `front.dot`, the link back from it and the old `rear.dot`; `Engine_UpdatePosition` draws the
car halfway between the two ends and turns it from the rear end to the front. Its branches: the front
over 48 px along its link - the rear on the same link (`rear.dot` the link's far point,
`rear.distance` = link length - front distance + 48, more than the link when the rear is past the
end of the track: it goes on along the link's line); `rear.dot` is `front.dot` - 48 - front distance
along `rear.link`; the front did not cross a point - the link from `front.dot` to `rear.dot` (none:
`ENGINE n: 0 rail 1` and the link nearest the way back), on it when it is long enough, else 48 - its
length - front distance beyond `rear.dot` along `rear.link`; the front crossed a point - the same
from the point it left (`zmdots5` / `zmdots6`, which short curve pieces give in normal play too).
This holds while every `rear.link` leads away from the car and there is track behind every rear end.
Two cases broke it. A car with a car in front whose `front.dot` is a dead end (a point of one link: a
buffer stop, the end of a factory's spur) - a car coupled behind a car hanging past a buffer stop -
had its rear laid forward along the one link: turned round, its rear end under the car in front (a
wagon 12 px past a buffer stop, coupled behind a train, came out 12 px from the car in front). And a
`rear.link` leading from `rear.dot` back to `front.dot` while the rear belongs beyond `rear.dot` made
"beyond" go back onto the car: at a dead end the one link is that way, and `Engine_PutOnRails` (each
end on the link nearest to it, from the nearer point) leaves `rear.link` so for cars made on open
track too - a factory standing on its spur's dead end (ten of the campaign's: l06, l08, l09 two, l10,
l12, l15, l16, l18, l20) released every car 34 px long and drawn 12 px forward, and two carts made by
a script `Create` 48 px apart on a main line came out 12 px apart, the second on the first. The
engine noticed only the jump when such a car moved again and its rear was laid anew: a car more
than 30 px from where it was in x logs `zm-error` (the authors' "the cars collapse") in `error.log`.
`train_fold.cpp` runs `Engine_UpdateRear` and corrects only a result that is folded (the rear end
nearer the car in front than the front end, or the ends under 40 px apart with the front end farther
from the dead end or turning point than the rear end), in those two cases only: the rear end past the
dead end on the line of its link, or beyond `rear.dot` on the link that goes on from it the
straightest. A car alone or first in its train with its front at a dead end is left as the original
lays it: the missions save single cars against a buffer stop with both ends laid on its link, drawn
where they were placed (l06, l12, l15; a car made by script right on a buffer stop collapses the same
way), and l12 a neutral train facing a buffer stop with its front end at it. Reproduced and checked on
a test copy of POLIGON (`notes` of the train-fold session: three straight spurs with a buffer stop
each, cars made by script at set distances from it, the factory moved onto its spur's dead end, cars
made on the main line). What it does not touch: `Depo_ReleaseUnit` deletes whatever stands on the
factory's place before it makes the next car (`AskPlace`), a car of its queue still at the door too (the
train loses it); a train pushed out of a factory moves until its last car has left the push line and
pushes whatever stands in its way (a train of another army, a braking locomotive), so the queue does
not back up to the door in play; a pushed train stopped by a red light of its own army reverses there
(`Train_Update`), and the next pushed cars can then roll onto it from behind; a script `Create` on an
occupied point puts the car on the nearest free place (the authors' `StartCreateTrain` lays a train car
by car that way); `Train_HandleContact` couples the moving train's first car behind the touched car
(reversing the touched train for the contacts front-to-front and rear-to-rear, or when it has a car
behind).

**Repair car and arsenal**: once a second the repair car (VID 85) heals the most damaged car
of its train by `Options.repairAmount` and rebuilds destroyed turrets; the arsenal (VID 45)
gives `Options.arsenalAmmo` to the car missing the most ammo. The repair point (building
VID 104, `Building_UpdateWorkshop`) repairs cars of its army that stop next to it - only
the car nearest to it, within 50 px (checked in the sandbox: a locomotive standing in the
repair point went from 45 to 150 hp at once, the gun wagon behind it stayed at 22 hp; with the
repair point's one-way push line (push_flag 1, as the missions have it) the train is pushed through
and the wagon came to 75 - full, the player's army has 1.5x hp - as it passed), and takes over
enemy wagons brought by a locomotive of another army.

**Factories** (B_DEPO) build cars from a queue of up to 10 VIDs (`Depo_AddToQueue`...
`Depo_ReleaseUnit`); the price is the build time in seconds, taken from `Player.money`. A built
car appears at the factory (objects in the way are deleted), gets the factory's train number and
flag 0x1; the cars of one queue form a train that is pushed out along the factory's push line.
When it leaves the line the cars lose flag 0x1, the locomotives are registered with the player
and the last car of the queue sends TrainCreated.

**The balloon** (B_BALLOON, `Balloon_*` 0x401000-0x401920, struct `Balloon`) is the airship an
airship cart carries - the cart is an `Engine` (VID 35) whose `vid->link` is the balloon's VID.
`Balloon_AdjustAltitude` (vtable slot 9) is its flight: it hovers at `vid->altitude` above the
ground (`World_GetGroundZForVid`), reserves the cart it is heading for (`Engine.balloonAboard`) and
in its state 2 descends onto it until it can dock (`Balloon_AttachCart` 0x401180, which takes the
cart's behave over and calls `Object_SetLink`). Carried (`uplink`), `Balloon_UpdateAttached` (slot
10) refills its ammo by `Options.balloonAmmo` once a second and turns it with the cart, and once the
cart comes within `weapon.battleRange` of the goal it lets go and attacks (command 3 of
`Balloon_SetCommand` 0x4018b0). Free, it attacks what comes within `detectRange` of it and, with
less than 64 rounds left (or no goal), it looks for the nearest free cart of its army
(`Balloon_FindCart` 0x401060: the same `vid->link`, the same army, no link of its own) to be carried
again.

## Player and the state bar

What `Player_*`, `PlayerBase_*`, `StateBar_*` and `MessageList_*` show (0x424840-0x4325b0, the
message list at 0x424840-0x4252b0); structs `PlayerBase`, `Player`, `MessageList` and `MessageItem`
in `re/types.txt`.

**One Player per army** (`World.players`, vtable 0x4758c4). `World_Construct` creates four with the
types 1, 0, 2, 0; only type 1 is the human player whose input is read (`Player_HandleInput`,
vtable slot 6). The base class `PlayerBase` (vtable 0x475894, ctor 0x42d9b0) holds the army's state:
`money` (1000 on a new level, script `SetMoney`/`GetMoney`), `type`, `army` (0 = the human), the
`flagman` and the object under the cursor. `Player` adds the state bar, the message list and the ten
bar slots.

**The flagman** is the unit under control: `Player_SetFlagman` (vtable slot 5, script
`SetFlagman(army,unit)`/`Flagman(army)`) makes an object the flagman, holds a reference and switches
the panel to the train or to the factory build list. `Player_CanSelect` lets only a train car (class
0x15 = B_ENGINE) or the factory (VID 102) become the flagman, and - with the debug key `E`
("CanSelectEnemy" on screen) - objects of other armies. The flagman is saved in the PLAY record
(`Player_Save`/`Player_Load`). The object under the cursor is `Player+0x24`
(`World_GetSelectedUnit`, picked every frame by `Player_HandleInput`), its name for the tooltip
comes from `Player_GetUnitName` (section `[Units]` of `strings.ini`; the same for every army). With a
train as the flagman, `Player_HandleMouse` picks the cursor (`Cursor_SetCursor`, the types of
`g_cursorNames`) and the right click's order by that object and its army: an object of the enemy (army 1)
or a neutral train, of spriteType above 2, gets the attack cursor (5, far 6, out of reach 12, no ammo 1, an
unarmed train's clash 3) and is attacked (ACT 0x20 / 0x97); a repair point not of the enemy repairs (4); a
building or factory of the neutrals, or a fort or mine (VID 162, 165) of any army with no cannon or rocket
tower within 150 px, gets the capture cursor (15, the click a move there; `Object_HasTowerNearby`); the rest
of the map is a move (2). The enemy's hidden objects (its mines) are never under the cursor. On an editor
map LocoFix takes the pirates as the enemy in these tests (editor-mission.md "The pirates for the player").

**The state bar** is a row of objects on layer 2000, created by `Player_ShowStateBar` (vtable slot 7)
and hidden by `Player_HideStateBar` (slot 8). The order in which they are created fixes what each one
shows: 0-9 the number of the bar slot (VID 4), 10-19 the texts (VID 8), 20 the marker (0x1c8), 21 the
0x1f2, 22 the VID 499, 23-32 the type icons (0x1c7), 33-42 the 0x1cf, 43-52 hp (0x1c9), 53-62 ammo
(0x1ca), 63-72 the behaviour (0x1cb), 73-79 the rest (0x1d0, 0x1c2, 0x1d5, VID 2 twice, 0x1f6,
0x1f7). `Player_SetPanel` adds objects 80-89 for the factory list (LocoFix adds a second row
after them, fix 22; the side tiles of fix 23 are separate objects, not in this list). `Player_UpdateStateBar` runs every
frame and picks the frame of each icon with `Object_SetDirection` (0x100 = a full turn): hp and ammo
from `Object_GetHp255`/`Unit_GetAmmo255`, the behaviour from `Unit_GetBehaveIndicator`, the type from
the `iconFrame` of the unit's weapon (`StateBar_SetUnitType`/`StateBar_SetVidType`), the number from
the bar slot (a train car uses `Unit.slotNumber`, below 1 the last frame = no number). It also draws
the minimap (`Minimap_Draw`; a click on it moves the camera) and prints money (object 77) and the
game speed (76).

**The minimap** (`src/LocoFix/minimap.cpp`, `BUGS.md` F26) is object 73 of the bar: VID 464
(`vid\464map.vid`, a 104 x 78 brass frame with a dark green inside; its menu box, `sizeX`/`sizeY`,
100 x 74), placed by `Player_ShowStateBar` in the bottom right corner of the view by the picture's
size (`viewRight - width / 2`, `viewBottom - height / 2`). There is no picture of the map: every
frame `Player_UpdateStateBar` takes the rectangle inside the frame (`Minimap_PlaceOver`) and
`Minimap_Draw` writes into the locked back buffer every rail point as one pixel (`Minimap_PutPoint`,
grey), in editor mode the terrain objects too, the units as 2 x 2 dots (`Minimap_PutDot`) and the
buildings and factories as 4 x 4 (`Minimap_PutBigDot`), each scaled by `mapWidth` / `mapHeight`
into the rectangle - by army: ours (trains, armed units, buildings, factories; planes and balloons in a
colour of their own) green, the enemy's the same kinds red, armies 2 and 3 only their trains, buildings and
factories, yellow (the money crates, army 3 in the campaign, are not drawn; on an editor map LocoFix draws
army 3, the pirates, black: editor-mission.md "The pirates for the player"), and brightens the view (`Graphics_BrightenRect`, a Direct3D quad that
multiplies the screen by 1.8: the rails in it turn white). The menu item under the cursor being VID
464 (`MenuNVidUnderCursor`) makes a left click move the camera there (`Minimap_Click`,
`SetPosition` mode 2; holding the button and dragging does not scroll, in the original either), a right click an order to that
map point (the mouse's map position is taken from the rectangle), and the cursor and mouse tip the
minimap's. The frame's opening is columns 8-97 and rows 9-73 of the picture; the engine's
rectangle starts at row 10 (one too low) and the view's highlight follows the camera 48 px past the
map's bottom (`boundsBottom` = map height + 50 - 2, so the map's last rows can come up over the
bar) - fixed in LocoFix. In LocoFix the minimap follows the interface's size ("The interface's
size" below) or the player's own choice: a row of ours on the game's settings page
(`game/maps/script/optfields.lgc`) keeps `MinimapSize` in the Players key like the sound settings -
"0" or absent = as the interface, "1"-"3" = that size ("1" is a fixed normal minimap under a bigger
interface - a script never released wrote "1" for NORMAL, so no player has it from before); read each time the bar is built, so it
applies from the next mission or when the Esc menu closes; a size is used only where it keeps the
frame clear of the bar's slots (228 px right of the screen's middle, times the interface's scale),
else the biggest that does. The bigger frames are VIDs 950 and 951
- the frame enlarged pixel by pixel (`patch_data.make_minimap_frames`), the menu box with it - put
into `World.vids[464]` while `Player_ShowStateBar` builds the bar, so the engine places it by its
own size; `MenuNVidUnderCursor` answers 464 for them, and the points, dots and the smallest
highlight (7 x 5 px) grow by the same step. The frame stands above the bar in the map's bottom right
corner - the original one hides the map's last 30 rows there, the bigger ones would hide 208 x 108 or
312 x 186 px for good - so with a frame bigger than the interface the camera may go below the map as
far as the frame is high (`boundsBottom` = map height + the frame's height, set after every
`Player_ShowStateBar`); with a frame of the interface's own scale the bar is as many times higher and
the engine's own room (the bar's height less 2) leaves the corner hidden in the original's proportion,
with no strip below the map; there
the engine draws what it draws outside a map: the edge of the map's ground (flat lava on mission
13, water on mission 8) or black (mission 17); at the original size the bounds are the engine's
own. The held editor shows no minimap: it releases the state bar
(`Player_ReleaseLevel`) and draws its own bar; Run the level has the game's.

**The interface's size** (LocoFix `interface_scale.cpp`): the bar, its texts and the minimap at two or
three times their size on a big screen, the world still drawn one map pixel to one screen pixel. The
engine already sizes most of the bar by its pictures - the base at the bottom of the view by its
picture's height, the minimap in the corner and the money plate at the top by their pictures and
boxes, a text's letters by its font's box, hit-testing by each item's box (`Menu_Update`, the OBJ
record's sizeX/sizeY), and the camera's room below the map by the base's height - so the build makes
bigger pictures (`patch_data.add_interface_rows` / `make_interface_pictures`: every picture of the bar
and its fonts enlarged pixel by pixel, VIDs 1100 + i for twice and 1150 + i for three times, i the
place in `INTERFACE_VIDS`; their rows are the edition's own rows of the originals with the box times
the scale) and the engine places them itself. What the engine has as fixed numbers is ours in C++
with one scale factor: `Player_ShowStateBar` (`status_bar.cpp`: the slots around width / 2 - 225 with a
pitch of 41, the buttons, the lines, the texts from the view's right edge - every offset times the
scale; at the normal size the same VIDs at the same places and depths as the engine's), the factory
list (`depot_rows.cpp`: `World.vids[455/497/8]` name the bigger VIDs while `Player_SetPanel` makes the
list - an object is counted per VID (`Vid+0x358`), so its VID cannot be changed afterwards - and the
list's offsets from the first slot's icon are then multiplied), and the slot under the cursor
(`Player_HandleInput` divides the icon's distance by 41 three times; replaced at 0x431d6b by the same
with 41 times the scale). `MenuNVidUnderCursor` answers the original's number for a bigger VID, so the
bar's clicks, the scripts' `MenuVid()` and the mouse tips work unchanged; the icon atlas' army colours
(the scripts' `SetVid(455, VID_GAMMA + army, ...)`) are copied to its bigger copies whenever the bar is
built. The choice is `InterfaceScale` "1"-"3" of the Players key (a row of ours on the settings page,
which offers only what the window takes: script function 207 `InterfaceSizeMax()`), read when the bar
is built and used only as far as the game's screen takes it: twice from 960 px of height and 1328 of
width, three times from 1440 and 1992 (the bar at most about a tenth of the screen's height, and the
minimap at the same scale clear of the bar's controls); a small map's logical screen (mission 1, 1024 x
768, stretched by `screen_scaling.cpp`) keeps the normal size, already shown bigger by the stretch. A
texture the engine loads is at most 256 x 256 at 16 bits (`GroundVid_LoadImage` unpacks it into a
buffer of 0x20008 bytes - a bigger one overruns it), so the enlarged texture pictures (the icon atlas
455, the buttons 498/499, the message strip 449) have their tiles packed anew into sheets of that size.
A font picture is a 16 x 16 grid of cells, each a letter and a one-pixel grid line right and below
(`FontVidConverter_Load` cuts a sixteenth of the width and height less one), so a bigger font is made
cell by cell with the line kept one pixel. The messages, the objectives, the mouse tip and the cursor
follow the same scale (`interface_texts.cpp`): the message list steps its lines by the font's picture
height + 1 (`MessageList_ShowLine`, `_Scroll`, and `_Update` to find the clicked line), so a bigger font
is made s - 1 rows taller than its letters and that step comes out s times the original's; its x 20 and
y 5 and the click strip 10 px left of the text are times the scale (`MessageList_ShowLine` wrapped, the
strip's VID 449 swapped in `World.vids` while the engine makes it). A text switches its font by number
in the middle of a line (`<Font=16>`, the ESC byte: the messages' red, yellow and green), so while a text
in a bigger font is drawn (`Text_Draw`) the interface's font numbers name their bigger copies. The mouse
tip (`MouseTip_Update`, font VID 6, 5 px right of and 10 px above the cursor) is made in the bigger
font with those offsets times the scale while the bar is up in play (its place laid out again the engine's
way - flipped below the cursor at the top, held at the right edge - so a bigger tip stays on the screen); the menus and the held editor keep
the normal one. The cursors (`Cursor_Show`, Windows `.ani` files through `LoadCursorFromFileA` at the
system's size) are loaded with `LoadImage` s times the system's cursor size - Windows scales the
picture and its hot spot together (the arrow's 6,5 of 32 x 32 is 18,15 of 96 x 96) - while the play
bar is up at that scale, and at the normal size again when the bar is hidden or given up (the menus,
the end screens, the held editor, TAB), as the mouse tip. A size changed in the Esc menu clears the
message lines still on the screen at the next message (moved by the new step they would overlap, and a
click would find a line never written). The list takes its numbers from the last bar's scale, so with the
bar hidden at a mission's start (TAB remembered) the objectives come at that scale - at the game's start the
normal one - and the first message after the bar is built at another scale clears them. With the bar hidden, a click on a factory makes
`Player_SetPanel(2)` build the bar itself and then the list in the same call; the list takes the scale
that bar gets. Messages are one line each, with no wrapping: the longest
objective is 48 letters (German), the longest event message 44 - 1728 px at three times, inside
2560. While the settings page is open its preview (`menu_version.cpp`) shows the bottom of the window as
the game will lay it out there: the bar at the interface's size across the window (`vidarpreview.bmp`,
`patch_data.make_bar_preview`: the bar's own pictures laid out as `Player_ShowStateBar` lays them out on a
screen 3840 px wide, the middle of it enlarged pixel by pixel - it matches a 1x screenshot of the game's
bar within the 16-bit colours' rounding) and the minimap's frame at its size on it in the corner.
The editor's own interface has a size of its own (`EditorInterfaceScale`, the page's **EDITOR INTERFACE**
row, script function 208) with the same rule for what fits, drawn by our code, not the engine's; the same
table of bigger VIDs holds its pictures too (1123-1135, 1173-1185) - editor-ui.md "The interface's size in
the editor".

**Zoom** (LocoFix `zoom.cpp`, `zoom_view.cpp`, working.md "Zoom with the wheel"): the mouse wheel in play
draws the map bigger or smaller, the interface at its own size (in the map editor too, with the editor's
interface drawn in the second pass: editor-ui.md "Zoom in the editor"; a level the editor runs zooms as play). A zoom z (window pixels per map pixel) is a
logical screen of the window / z, at most the map on each axis, made between two frames (`screen_set_view`:
the back, z and depth surfaces of that size, also in full screen, the screen size and `Graphics_SetViewport`;
the fog texture big enough, `fog_buffer_cover`), and the camera put with `SetPosition` so that the map point
under the cursor stays under it. The frame is drawn in two passes. The interface is every object of class
FRAME (10) or TEXT (0x13): `Frame_Construct` (0x40c4d0) puts each on `World.menu`, and a text's constructor
calls it (`GameWorld_CreateObject` case 0x13) - the bar, the minimap's frame, the money plate, the messages,
the mouse tip and the scripts' texts (`PrintFile` is `CreateTextFromFile`; l01's
`PrintTextAtCenterAndBottomFromFile` draws nothing). They are drawn from the same layer lists as the world;
the world pass is `Graphics_DrawScene` without them (the culling test at 0x41faf2 and the shadow test
`view_culling.cpp` replaces, and the draw call of layers 0 and 7, 0x41fbe0, which have none, ask
`zoom_draw_filter`). After Direct3D's `EndScene` the logical picture is stretched into the window-sized back
buffer - a Blt at a whole zoom (3, the top of the ladder), else a Blt into a texture drawn as one linear-filtered quad
(vertices at -0.5: pixel for pixel at zoom 1, checked) - and the interface's pass draws layers 2-10 again with
only those objects at the window's size. Its software sprites (layers 2-4, 10) go into a system-memory
overlay put on with a colour-keyed Blt, their depth buffer is memory of ours (`depthBits` / `depthPitch`: the
engine locks only when they are 0), and the hardware z-buffer is cleared to the engine's empty value (0x3ff)
for its Direct3D items; without a depth copy the order stands in for the depth test (the interface's items are
flat, z 2000-2005: software sprites up to the highest z of the Direct3D ones before them, the rest after -
the bar's numbers, VID 4 and 8 at z 2005, over the type icons of VID 455 at 2003). Locking the window-sized
picture for the software sprites instead cost dgVoodoo a GPU sync and copies each time: 16-17 ms a frame at
1920x1080 against about 3.3 ms this way (the test rig's virtual adapter). `Minimap_Draw` (0x405800) is
called by `Player_UpdateStateBar` in the update, after the frame's drawing: under a zoom its rectangle is
kept and it is drawn in the interface's pass, its highlight from the world's view and after the overlay, so it
brightens the dots as before. `Graphics_DrawEffects` (the fades, the nuke's flash, the snapshot of
`Effect` 5) runs its timing in the world pass but draws in the interface's, over the whole window, as the
original covers the bar; the scroll effect moves the camera and runs only in the world pass. The mouse: the
engine's cursor stays logical (`WM_NCHITTEST` mapped) for world picking, orders and edge scrolling;
`Menu_Update`, `MouseTip_Update` and `Player_UpdateStateBar` (the minimap's hover and click) get it in the
window's pixels, and the minimap's right-click order its rectangle in logical pixels (0x431fce: the engine
takes the map point as (screenX - left) * map / width). The bar is built with the screen and the view set to
the window's (so it has the window's layout and the interface's scale) and again when a zoom view starts or
ends; the camera's room below the map is its height / z in map pixels. Script functions 119 `ScreenX` and 120
`ScreenY` and `MenuLoad` give the window's size under a zoom view, as on a map as big as the window, so the
scripts' texts and the menus are laid out as at zoom 1; a menu up gets a window-sized screen drawn in one
pass. Edge and arrow scrolling move the camera in map pixels a frame; `World_UpdateScroll`'s move
(`SetPosition` at 0x41cb3f) is taken 1 / z times as far, so it is as fast on the screen at every zoom (not
while the camera follows the flagman). The wheel is the engine's own `Mouse+0x04` (`World+0x234`), stored by
`Mouse_HandleMessage` and cleared by `Mouse_BeginFrame`; nothing in a mission read it before. The bar built
again when a zoom view starts or ends goes through `Player_SetPanel`, which plays the panel's click (SFX 109,
0x42f27f) whenever the panel changes; that call is left silent while the bar is rebuilt, so a zoom makes no
sound (a click on a slot still plays it). The surfaces are made with room for two notches more out and kept
while they are big enough and at most 2.5 times the view's area, so a run of notches makes them once; after
lost surfaces (`lost_surfaces.cpp`) the view's surfaces, texture and overlay are made anew before the next
frame. At the level's own zoom none of this runs: every hook passes the engine's call through.

**Panels** (`Player.panel`, `Player_SetPanel`): 0 the unit bar, 1 the cars of the flagman train, 2 the
build list of the flagman factory, 5 the queue of that factory, 6 hidden. The ten bar slots
(`Player.barUnits`) are filled by `Player_SetBarSlot` - `Shift`+1..9 puts the flagman there (a train
car puts its locomotive); 1..9 alone make a slot the flagman again (`Player_HandleHotkey`).
`Player_AddBarUnit` (vtable slot 10) would add an object to the first free slot, but nothing in this
exe calls it (see BUGS.md "Checked, not bugs").

**The factory panels** list the buildable units of the flagman (`Depo`, VID 102 and the stations - the
`Unit.items` of the OBJ record). Every entry shows its function key (`F1`..`F12`, `[` and `]` for the
two extra units) and its price, the build time in seconds. `Player_CanBuild` refuses an entry at
`VID_MAXUNIT`/`VID_MAXUNITARMY` (it counts the same VID in the queue of every factory on the map) and
`Player_CanAfford` needs money for the build time - only the difference to the entry that is being
replaced while it is picked. A click on an icon and the function keys add or replace an entry
(`Player_HandlePanelAction`, `Depo_AddToQueue`...); the picked entry is edited from panel 5 (pause,
up, down, remove). `StateBar_DrawUnitIcon` draws an entry.

**Messages**: `Player.messages` is a `MessageList` (`MessageList_Construct`, vtable 0x47571c): three
lines, the text in the message font (VID 14) with a clickable strip (VID 449, `vid\449.vid`, 30x21)
under it for a message that has a place on the map - a click moves the camera there
(`MenuNVidUnderCursor`). Script 114 `MessageText` (`MessageText`, `Player` vtable slot 9, forwards to
`MessageList_Add`) queues a message; it waits `Options.messageTime` (CNST 25) ms before
`MessageList_Update` shows it on the bottom line (`MessageList_ShowLine`, SFX 106), and the lines move
up every 15 s without a message or when the bottom line is still busy. `Player_ObjectDeleted`
(slot 1) takes a deleted object out of the lines, `Player_ReleaseLevel` clears the list.
