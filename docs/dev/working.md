# Working with the code, resolutions, status

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

## Working with the code

- `tools\decompile.py` - import into Ghidra, apply `re/symbols.csv` and `re/types.txt`, export
  C to `re/c/` (about a minute; `--reuse` skips the import and analysis while working on
  types).
  The Ghidra project is created in `build/` and can be opened in Ghidra for browsing;
  findings go into `symbols.csv` and `types.txt` so they survive the next decompilation.
- Types (`re/types.txt`, applied by `tools/ghidra/ApplyTypes.java` after the names; the
  syntax is in the file header): a `struct` lists only the fields we know, each at its
  offset - the bytes in between stay unknown (`unknown_XXX` in `re/c/types.h`, `field_0x..`
  in the code), so a new finding is one line. A `vtable` block builds a struct of function
  pointers from the table in the exe (slots get the names of the functions in them, typed
  from their prototypes, and functions Ghidra missed are created). `method <address>
  <Struct>` makes a function `__thiscall` with a typed `this` (kept as an explicit first
  parameter in ECX, so calls show `Function(object, ...)`); `func` gives a whole prototype;
  `global` and `local` type variables. Errors are printed as `types.txt:<line>: ...` in
  the Ghidra output and the run goes on.
- `tools\merge_symbols.py` - merges new `address;kind;name;description` lines into
  `symbols.csv` (a line with the same address and kind is replaced, the others are added to a
  section): `python tools\merge_symbols.py re\symbols.csv <new.csv> "<header>" "<anchor>"`.
- `tools\unpack.py` - writes `re/exe` and `re/data`, font and icon previews in
  `build/graphics`.
- `tools\res_tool.py` - `objects.res` as TSV tables and the overrides in
  `game/objects.override/` (see "Editing objects.res"); e.g. `.venv\Scripts\python
  tools\res_tool.py unpack <game>\objects.res build\objects` to look up the values to change.
- `tools\map_tool.py` - maps (`.map`) and images (`.vid`) as text, TSV and PNG files and
  back, the overrides in `game/maps.override/` and the new maps in `game/maps.new/` (see
  "Editing maps and images"); e.g. `.venv\Scripts\python tools\map_tool.py unpack
  <game>\maps\l01\l01.map build\l01` to find the objects to change, `... new build\mymap
  <game>\maps\l01\l01.map --script` to start a map of your own, `... list
  <game>\maps\l01\l01.map` to see what is on a map, `... check <game>` after changing the
  tool.
- Changing a game text file: edit the assembled file (e.g. `<game>\lang\pl\maps\shiftb.txt`)
  and turn it into a patch against the original: `python tools\patch_text.py diff
  captured\pl\maps\shiftb.txt <edited file> lang\pl\game\scripts\shiftb.txt.patch` (scripts:
  against the Steam file). `strings.ini` changes go straight into `strings.override.ini`.
  A new language: copy `lang/_template` to `lang/<code>/`, edit `language.ini` (name, code page,
  base), translate the files under `game/` (and delete the ones it does not translate: a pack's
  file replaces the captured one), draw the letters its base's fonts do not have in
  `lang/<code>/fonts/<font>.png` if it needs any (see "Languages", the Czech letters; `python
  tools\font_cells.py preview <code>`), then `python tools\check_lang.py <code>`,
  build it (`python tools\build.py --target <test folder>`) and look at it in the game
  (`tools\test_game.ps1 -Language <code>`). See `docs/TRANSLATING.md`.
- `tools\test_game.ps1` - starts the game (`-Language <code>` sets the language), clicks OK in
  the launcher, waits for a log entry (`-WaitFor "menu\.map"`), performs steps
  (`-Steps "k371:489@5,13@20"`: mouse `m`/`k`/`p` at a point of the game area, a middle
  button drag `d<x>:<y>-<x>:<y>`, the mouse wheel `s<x>:<y>:<notches>` (> 0 up; the turn rides
  in the high word of the test message, `test.cpp`) or a key code, seconds after `@`; a lone `t` prints the
  tooltip under the pointer, so a hover can be checked to hit the menu item it was aimed
  at; `@30?<regex>` waits up to 30 s for the logs to match instead, and a step whose
  regex already matches is skipped, so retries can be listed), takes screenshots
  (`-LastShotOnly`: only the final one) and closes the game normally (only then is
  `error.log` complete). `-Menu` takes the `m`/`k`/`p` points in the menu's 1024x768
  canvas: the game scales that 4:3 picture to fit the window and centres it (pillarboxes
  in a 16:9 window), so the points are scaled and moved the same way. The logs it watches
  (in `logs\` of the game folder) are the game's own and `LocoFix.log`, where test mode also lists every file the game
  opens. In test mode the game keeps running behind other windows (`test.cpp` does not
  pass `WM_ACTIVATEAPP` FALSE to the game, which would pause it and ignore the mouse), so a test
  never brings the window forward and the user keeps the focus; only the game's own start (the
  launcher dialog) takes it once. Full screen is not part of the automatic tests - switching the mode
  breaks the redrawing of other windows; `-SetFull 1` runs a single test in it. `-Map maps\l01\l01.map` starts straight into that
  map (LocoFix `--test-map`: no intro, no menu) and `-LogWait` sets how long `-WaitFor` may
  take (60 s). `-Editor` starts the game in editor mode (`--editor`); there the steps
  `S<file>` and `L<file>` save and load a map without the file dialog (sent as
  `WM_COPYDATA`, which LocoFix takes only in test mode), and `c<id>` posts a menu command
  (`c40010` = Options > Run the level; not Load, Save or About - their dialogs would wait for
  a click); `Q<file>` types a file into the game's open file dialog (File > Load / Save, the
  Menu panel: the name box, the Edit with id 1001) and presses OK, `Q` alone Cancel, after a
  screenshot of the dialog; `V<x>:<y>` puts the top-left of the view at a map point (test mode), so the
  points of the next steps are known, and the test cursor in the middle of the game area (the
  cursor starts where the real one was, and at an edge the engine's edge scrolling moved the view
  away again until an `m` step brought it in), `D<x>:<y>` writes the rail points near a map point with
  their links to `LocoFix.log` (and a point's semaphore value, `sem n`: 4 + army closes it for that army; test mode; `V` takes negative points too, the editor's camera
  goes past the map), `P<nvid>[:<army>]` picks that object of the palette to place, in that army (-1 or none: the missions'; test mode), `M<file>` sets the mission settings from a file instead of the dialog
  (test mode; the format is in `editor_mission.cpp`), `N<name>:<line>[:<w>:<h>]` makes a new map as File >
  New would with the dialog's ground line (test mode), `Z<w>:<h>` resizes the map as File > Map size (test mode), `R<name>` renames the map and `X` deletes it (Rename or delete without the dialog, test mode), `^<code>` / `+<code>` press a key with
  Ctrl / Shift (`^90` = Ctrl+Z), `j<x>:<y>` / `J<x>:<y>` only press / only let go the left button there, `K<code>` / `U<code>` only press / only let go a key (`K16`
  before a drag holds Shift), `C<code>` sends a character (`WM_CHAR`, which the scripts'
  `GetKey()` reads - a posted key gives the lower case letter: `C77` is the developer key M), `A<seconds>`
  presses Alt+Tab (real keys) and after that many seconds Alt+Tab again, with a screenshot of the
  game window while it is away (`<Shot>_<n>_away.png`),
  `l<x>:<y>-<x>:<y>` drags with the left button, and `x<seconds>` follows the game when it
  starts again (`StartEditorMode`, Play the map, Back to the menu) - the test goes on with the
  new process of the same exe; when a step ends with the old game gone the test looks for a
  restarted one by itself.
- `tools\test_missions.ps1` - for each mission: starts the game with the mission's map as
  the test map (`-Map`, so the intro, the player screen and the strategic map are skipped -
  missions the player has not unlocked are tested just the same), waits until the map and
  its script are loaded, takes a screenshot and reports from `LocoFix.log` that the map
  really loaded (`World->levelFile`) and from `error.log` the script compile result and
  errors. A mission passes when it loaded, its script compiled without errors, the game
  logged no `EXCEPTION!` (the game catches a crash, logs that and quits - a crash that
  closes the window "normally" all the same) and closed; the script exits with 1 when one
  fails. It reads the newest `logs\error*.log` written since the mission started, since a killed
  game leaves its dated log and an older `error.log`. Results in `build\tests`. About 10 s per mission; `-Quick` runs only missions 1, 2
  and 8 (one of each map size) for everyday checks, the full 20 before bigger merges.
  `-Game <folder>` tests another assembled copy
  (`build.py --target <folder>`), so tests of different changes don't share one
  game folder; `test_game.ps1` takes `-Exe`.
- `tools\test_rail_rules.py` - differential test of the track rules: builds
  `tools\test_rail_rules.cpp` (which includes `src/LocoFix/rail_rules.h`, the rules of the editor's
  Track tool) and compares it with `map_tool.py` (`rail_candidates`, `plan_track`, `track_cells`,
  `rail_of`, `rail_point_links`) on generated cases - lattice steps and wrong ones, tracks next to
  existing rails, branches from their ends, rails laid again, crowded points, ramps and rails at
  other heights, the track's height given or not, hand-picked shapes, rail objects of every
  direction, joints with rails of every shape, single rails laid as the editor lays them (`through`,
  `lay`, ramps) onto joints of other track (`python tools\test_rail_rules.py [cases]`, 3000 by
  default; every kind of rule failure comes up); then 21 tracks over other track (crossings of a
  straight, a diagonal and a curve at a joint and mid-rail, parallels half a step and a step apart, a
  crossover, the same rail again, branches, lines started or ended across a joint, a fork with no
  stem, a sharp turn) and 6 clusters of clicks at one joint (every rail of the lattice from it, ramps
  to its 6-link limit, a crossing and a branch both ways round, switches both ways and sides, lines
  beside and across a crowded joint), each in its four mirror images and among denser track, with
  the answer each must get - the clicks laid as the editor lays them, the lines in one go as map_tool;
  the 24 rail ends, each going on in three steps; and every rail of the missions' maps, POLIGON, the
  template and the player's maps of the installed game (OpenLocoland's folder, else `..\Locoland`,
  else `build\game`), laid again where it lies with the rails before it in the map there: each must
  be laid (the report says what the old rules refused of them).
- `tools\test_script_value.py` - differential test of the rewritten script value helpers:
  builds `tools\test_script_value.cpp` (which includes `src/LocoFix/script_value.h` and
  `src/LocoFix/world_script.h`), emulates
  the original functions in Unicorn with stubbed
  `atoi`/`sscanf`/`itoa`/`String_Append`/`String_Assign`/`Log_Write`,
  and compares a reference model, the original and the C++ on 2000 random cases and edge
  cases (`python tools\test_script_value.py [cases]`).
- Tests run the game with `LOCOFIX_TEST=1`: LocoFix then ignores the real mouse and takes
  mouse input only from the test (message `WM_APP+1`), so tests do not move the user's
  cursor. The test's cursor starts where the real cursor was when the game started
  (`install_test_mode` reads it with `GetCursorPos`), usually outside the game area, so until
  a step moves it the game scrolls a bigger map toward it (border scrolling) - and the steps
  begin only after `-WaitFor`, by when the camera has moved: a test that needs a known view
  sets it from the mission script after its first move step (`SetPosition`, and
  `SetScrollType(0)` to keep it). The test window may be minimised while a test runs (the
  game keeps running); that is how the unsigned mouse coordinates of the engine were found
  (LocoFix fix 21) - with them the game saw the cursor over the HUD bar and no unit. Now a
  click on a train selects it and a right click on a track sends it there, in a test as by
  hand: the units are picked at their logical position (world y - z, on the rails under a
  train, not the drawn sprite), and `w` prints what the engine thinks of the mouse. The engine takes the mouse position from `WM_NCHITTEST` (`Mouse_HandleMessage`),
  which Windows sends straight to the window procedure - that's why test mode also
  replaces the window procedure.
- Names in `symbols.csv`: engine functions called from scripts keep their original names
  from `export.lgc` (`PlayMovie`, `GetGroundZ`...), the rest are ours, as `Module_Action`
  (`Graphics_InitDirectDraw`, `Level_Load`), C runtime - `crt_...` (the widely used ones are
  identified: `crt_atoi`/`crt_atol`, `crt_strncmp`, `crt_strchr`/`crt_mbschr`,
  `crt_memmove`/`crt_memchr`, `crt_fopen`/`crt_fclose`/`crt_fseek`, `crt_sprintf`/
  `crt_vsprintf`/`crt_fprintf`, `crt_initterm`/`crt_cinit`, the ctype family and the VC6
  integer-to-string helpers; the rest keep `crt_<address>`). Kind `c` puts a comment
  on a statement inside a function (the cases of `Script_CallExtern` carry the export.lgc
  number, declaration and what the engine does).
- Entry points: `WinMain` (0x4186f0) creates the world (`GameWorld_Construct` ->
  `World_Construct`: cfg, launcher, window, objects.res) and calls its `Run` (vtable slot 3,
  `GameWorld_Run`: `Game_Step`, drawing, object updates) until it returns 1. Drawing:
  `Graphics_BeginScene`, `Graphics_DrawScene`, `Graphics_EndScene`. Scripts: `Script_Load`
  (compiler `Script_Compile`), `Script_CallFunction` (interpreter) and `Script_CallExtern`
  (engine functions 65-254). Data: `Objects_Load`, `Level_Load`, `Sound_LoadSFX`.
- Addresses: in this exe file offset = VA - 0x400000 for `.text`, `.rdata`, `.data`;
  `.rsrc` is placed differently (VA 0x5d6000, file 0x8a000).

## Resolutions and widescreen

The engine copes with a bigger screen by itself: more of the map is visible, the unit bar
is centred, the minimap and counters go to the window edges. The limit is the map size
(ground `lNN.vid`):

| Map | Missions |
|---|---|
| 1024x768 | 1 (tutorial) |
| 2048x1536 | 2-7, 10, 12, 19 |
| 3072x2304 | 8, 9, 11, 13-18, 20 |

When the window is bigger than the map, the game would show the black background, the
objects the authors placed just past the map edge (clouds, tracks, buildings), and the
interface in the window corners, far from the map. So LocoFix gives the game a logical
screen after every level load: min(window, map) on each axis. The game lays everything
out for that size exactly as it would at that resolution, renders into the top-left part
of the back buffer, and before presenting (`Graphics_EndScene`, 0x414da9) that part is
stretched over the window keeping its proportions (copied aside and stretched back with a
Blt - nearest-pixel, since dgVoodoo's filtering is off, see LocoFix fix 23; in a window
LocoFix presents it itself). In a window the game also gets a back
buffer, z-buffer and depth buffer of the logical size for such a level: the engine locks the
back buffer and the depth buffer whole every frame for the software sprites, and dgVoodoo
copies the whole surface each time, so window-sized surfaces made a 1024x768 picture at
1920x1080 cost as much as a full one (menu 155 fps, 365 with them, no frame limit). The menu
map is 1024x768 too, so the menu follows the same rule. Menus drawn for 1024x768 inside a bigger logical screen (the Esc menu in a big
mission, background VID 466) get their centre stretched. The mouse is mapped back in
`WM_NCHITTEST`, and the snapshot of the fade-in effect (`Effect` 5) gets the unstretched
picture.

The result: at 1920x1080 mission 1 and the menus are 1024x768 stretched to 1440x1080, the
other missions use the whole 1920x1080 with the interface at the window edges (the map
fills it). In full screen dgVoodoo scales the chosen mode to the monitor
(`ScalingMode = stretched_ar`).

**Frame rate at high resolutions.** The game draws on one thread, and the cost of a frame grows
with the screen: the engine draws the ground with Direct3D, then locks the back buffer and the
depth buffer whole for the software sprites (dgVoodoo waits for the GPU and copies both to the
CPU and back), and every light of a colour-box VID (`Graphics_DrawLight`) fills a texture on the
CPU and draws it. Measured in mission 13 (a big map, many lights; Ryzen 9 7900X, RTX 4070 Ti
SUPER; `LocoFix.log`'s frame rate of the level, about 30 s each; the numbers vary by a few fps
between runs):

| | 1024x768 | 1920x1080 | 2560x1440 |
|---|---|---|---|
| window, limit 60 | 60 / 60 | 60 / 60 | 39 / 45 |
| window, no limit | 201 / 200 | 67 / 81 | 37 / 45 |
| full screen, limit 60 | 60 / 60 | 58 / 60 | 36.5 / 42 |

(before / after LocoFix fixes 31 and 32). The limit itself keeps its rate whenever a frame takes
less than its period. What remains at 2560x1440 (a sampling profile of the main thread): the
lights - `Texture_DrawZ` and the driver under it about a third, `Graphics_DrawLight` a fifth -,
the three surface locks and the software sprites (`SpriteVid_Draw`). The editor draws the
decals and every light of the map and is slower (mission 13 at 2560x1440: about 18 fps, 20
after the fixes). Less than that needs a renderer of our own instead of the engine's mix of
software drawing and Direct3D 7 through dgVoodoo. dgVoodoo's `FastVideoMemoryAccess` breaks
the ground (`Load Can't decode` and an exception when a map loads), and
`DDLOCK_DISCARDCONTENTS` on the light textures made no difference.

Before drawing, the engine skips objects far from the screen centre, with a box sized for
1024x768: objects (`World_DrawLayer`) within x +-1024 and y +-512, their shadows (the shadow
pass of `Graphics_DrawScene`) within x +-512 and y +-512. LocoFix makes the box as big as
the screen plus the original margin (`view_culling.cpp`); otherwise shadows stop in the
middle 1024 pixels at 1920x1080 and objects disappear at the edges at 2560x1080 or 1920x1200.
The fog of regions and the snow on the ground go through a scratch texture that covers 1024x1024
pixels; on a bigger screen LocoFix draws them through one as big as the screen needs
(`fog_buffer.cpp`, see "The fog buffer") - before that a fog over the whole view crashed the game.

## Status and known bugs

Smoke test (`test_missions.ps1`): all 20 missions load in windowed mode, mission scripts
compile without errors, the game closes normally.

**CI** (`.github/workflows/ci.yml`, GitHub Actions on Windows, every push to `main` and every
pull request): the Python tools compile, `LocoFix.dll` builds with MSVC x86 (`build_locofix.py`,
the DLL is kept as the run's artifact) and `test_rail_rules.py` passes - there without the
installed game's maps, which it skips. What needs the game stays local: `test_script_value.py`
(Steam.exe), `check_lang.py` (the captured fonts), `test_missions.ps1` and `test_game.ps1`.

All bugs found - fixed, confirmed, suspected, quirks scripts may rely on - are listed in
`BUGS.md`. The rules a map has to follow - track, junctions, the ends of the main line,
one-way sections, buildings, semaphores, the mission script - are collected in `MAP_RULES.md`,
the specification for a map editor. New findings go there first; a suspected bug is fixed only once the code around it
is understood, except obvious ones with a small, safe fix.
