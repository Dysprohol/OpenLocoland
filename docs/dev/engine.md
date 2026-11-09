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
a diagonal). Points closer than 2 px with the same z are shared (reference counted), which
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
A point has at most 6 links; a link stores the other point, its
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
sharper than 0x20 means reversing, which needs room for the whole train behind a switch
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
comes from `Player_GetUnitName` (section `[Units]` of `strings.ini`).

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
