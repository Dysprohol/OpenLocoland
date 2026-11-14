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
  pass `WM_ACTIVATEAPP` FALSE to the game, which would pause it and ignore the mouse). A run is
  quiet by default: the script starts itself again on a Win32 desktop of its own (`CreateDesktop`,
  `STARTUPINFO.lpDesktop`, closed after the run), so neither the start window nor the game's window
  appears on the user's screen, takes the focus or gets a taskbar button (the start window is a
  dialog that activates itself - only another desktop keeps that off the user's), and
  `LOCOFIX_QUIET=1` makes LocoFix hold the sound and music volume at 0 for the run (`test.cpp`: the
  engine's two volume setters and the values themselves, never the registry or the sound devices);
  output and exit code come back to the caller as before. The game draws there as usual; its
  screenshots come from `PrintWindow`, or, when that is black (nothing composes a desktop that is
  not shown), from the game's own screenshot (menu command 40023, the back buffer - the logical
  picture before the stretch). The frame rate there is lower than on the user's screen (13-18 % at
  1920x1080 and 2560x1440), so frame-rate measurements run with `-Visible`, the old way on the
  user's desktop; so do the `A` and `Q` steps (real keys, a file dialog), `-SetFull 1` and `-Screen`,
  automatically.
  A test run keeps its settings in a key of its own, `HKCU\SOFTWARE\Gromada\Locoland-test`: in test mode
  LocoFix opens that key wherever the engine or LocoFix opens `HKCU\SOFTWARE\Gromada\Locoland` or a key
  under it (`test_registry.cpp`: `RegOpenKeyExA`, `RegCreateKeyExA` and `RegGetValueA` replaced in the
  exe's and LocoFix's import tables, before the exe is recognised, so another edition's exe too) - the
  start window's settings, the window's place, the scripts' `Players`, LocoFix's options, the editor's
  settings. A test never reads or writes the player's own key on the same computer, so nothing is
  saved and put back around it. `test_game.ps1` writes the run's settings into the test key before
  every start: `-Resolution` (1024x768), `-SetFull`, `-Language` (pl), `-Cheats`, `-UnlockMissions`
  (0), `-FrameLimit` (60), `-FpsCounter` (0), the update check at start off, and `-Reg "Name=1,Other=s:text"`
  for more; the rest of the test key (Players, the window's place, the editor's settings) stays from
  run to run, `-ResetTestKey` starts from an empty one (its `Players` is not filled from old data, as
  `player_registry.cpp` does for a player's first start). `LOCOFIX_TEST_KEY=1` gives the test key without
  the rest of test mode, for a scripted check with the real mouse and focus (full screen). The script puts
  the `LOCOFIX_*` variables it sets back as they were when it ends, so a game started later from the same
  PowerShell session is not in test mode (it was, and so on the test key). Checked 2026-11-12: two runs (a mission at
  1280x720 in English, the editor at 1600x900 in German with `EditorHints` and `EditorScatter` set)
  left `reg export` of the player's key and its `Players` byte for byte the same. What still uses the
  player's key: Setup and the installed game (`tools\release.py`, `tools\installer.py`, the release
  gate's install and start tests) and a build without LocoFix.dll (`build.py`'s as-is editions) - those
  runs read and write the real key as a player's game does.
  Full screen is not part of the automatic tests - switching the mode
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
- `tools\rig\rig.ps1` - the test rig: runs `test_game.ps1`, `test_missions.ps1` or the installer
  inside Windows Sandbox, off the user's screen, at a chosen guest resolution, several sessions' jobs
  side by side; see "The test rig".
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

**The test rig.** `tools\rig\rig.ps1` runs the tests in Windows Sandbox (the Windows feature
"Windows Sandbox" must be on), so they neither show on the user's screen nor share his game's
registry key, and so the guest - a throw-away Windows of its own - can be set up as a test needs:

    .\tools\rig\rig.ps1 -Job missions -Game build\game                 # the 20 missions (-TestArgs @{ Quick = $true })
    .\tools\rig\rig.ps1 -Job game -Game build\game -TestArgs @{ Map = "maps\l01\l01.map"; WaitFor = "test map: loaded"; ClickOk = 1; SetFull = 0; Visible = $true }
    .\tools\rig\rig.ps1 -Job game -Game build\game -Resolution 1366x768 -Registry @{ FrameLimit = 0 } -TestArgs @{ ... }
    .\tools\rig\rig.ps1 -Job setup -Setup build\setup\<Setup>.exe [-SetupArgs --auto]   # a clean install
    .\tools\rig\rig.ps1 -Status | -Monitors | -Look | -Show | -Hide | -Stop | -Kill

- *How it runs.* One sandbox (the box) serves every session's jobs from one queue, several at once
  (see *Side by side* below): the first job starts it (about 12 s), it stays up until `-Stop` or 10 minutes without a job
  (`-IdleMinutes`; it shuts itself down), and only one Windows Sandbox can run on the machine at a
  time (a second start is refused, `CO_E_APPSINGLEUSE`). The box is started with `wsb.exe`, the
  Sandbox app's command line (its `wsb` alias is not registered, `rig.ps1` finds it through the
  package's folder), with no window; its client window is opened on a Win32 desktop of the rig's
  own (`CreateDesktop`, as `test_game.ps1` runs quietly), which is never shown - nothing comes up
  on the user's screen, takes the focus, flashes or gets a taskbar button (the start logs the
  user's desktop with a WinEvent hook: `<host folder>\rig\start-desktop-events.txt`). The client is
  needed: the guest's user logs on only once a client connects, and the game cannot create its
  DirectDraw device in a disconnected session. The host folder that holds the repository (all
  worktrees and builds) is mapped read-only as `C:\host`, the Steam original read-only as
  `C:\steam`, and `<host folder>\rig\io` read-write as `C:\rig\io` (not under `%LOCALAPPDATA%`:
  the sandbox fails to start with a mapped folder or `.wsb` there, 0x80070003). Inside, an agent
  (`tools\rig\guest\agent.ps1`, the logon command) takes the queue's jobs and runs each in a process
  of its own (`runjob.ps1`, always the agent's own copy), killed at `-Timeout`; a job copies the game
  in (`C:\work\games\...`, robocopy: 1-3 s for 550 MB the first time, 0 s when the same build ran in
  that slot before), starts with an empty `HKCU\SOFTWARE\Gromada` (plus `-Registry`
  values), runs this repository's `tools\test_game.ps1` / `test_missions.ps1`, and its logs,
  screenshots and the game's `logs\` / `Screens\` come back to `build\rig\<job id>` (`-Out`), with
  `run.json` (slot, guest user, display, the jobs beside it) and `done.json` (exit, time, how many
  games ran at once at most, whether the display changed during the run).
  `-Job setup` gets a box no earlier job has touched: it waits until the box is empty, the box is
  restarted (the queue is kept) and it runs first in the new one. The guest is Windows 11 Enterprise (the host's build), 8 GB (`-MemoryMB`), no
  network, one playback device (the remote desktop's, muted - see Sound), Windows PowerShell 5.1
  with the C# compiler (all the test scripts need), no Python.
- *The picture.* dgVoodoo's Direct3D 11 runs on the host's GPU (vGPU: the guest lists the RTX 4070
  Ti SUPER and the integrated AMD, dgVoodoo enumerates the NVIDIA one), 60 fps with the default
  limit, 207-280 fps without it in mission 8 at 640x480. `<VGpu>Disable</VGpu>` is ignored by
  Sandbox 0.8.114 (the guest keeps the GPU), so `-Warp` sets dgVoodoo's own `OutputAPI = d3d11warp`
  (Direct3D 11 in software, 106 fps there) in the guest's copy (`-DgVoodoo @{...}` for any
  setting). The game's screenshots are real pictures; the frame rate does not depend on the box's
  window being on a desktop nobody sees. The 20 missions take 1.3-1.9 minutes in 4 lanes, 5 one after another (3.9 with WARP; the host: 6-8).
- *Resolution and scale.* The guest's screen is the client window's area (remote desktop with
  dynamic resolution), and its scale is the scale of the host monitor that window is on; inside the
  guest neither can be changed (the scale's range is 100..100 %, `ChangeDisplaySettingsEx` reports
  success and changes nothing, the only mode listed is the current one). So `-Resolution WxH`
  sizes the window (on the rig's desktop, invisible) until the guest has exactly that size
  (1024x768, 1366x768, 1920x1080, 2560x1440 checked; a new size is there in about a second, for
  running programs too), and `-Scale <percent>` puts the window on a host monitor with that scale -
  or says that none has it (`-Monitors` lists them). The window cannot be wider than the host's
  screens together: 3840 wide needs the 4K TV switched on. Every job logs what the guest really
  had (`job.log`: real pixels, dpi, what a DPI-unaware program sees) and is not run on another.
- *The real mouse.* Inside the guest a script moves the guest's own cursor - `SetCursorPos` and
  `SendInput` (relative and absolute) - with the game full screen there, without touching the
  host's cursor; the engine follows it (`tools\rig\guest\cursor_job.ps1`: the cursor's place, the
  clip rectangle and the engine's mouse at the corners and edges).
- *Sound.* The guest's sound would come out of the host's speakers (the remote desktop's audio
  redirection; the `.wsb` file can switch off only audio input). The agent mutes every playback
  endpoint of the guest when the box starts and again before every job (`guest\audio.ps1`, Core
  Audio): the devices stay, so the game's DirectSound start and playing are the same as on a real
  machine, only silent - while a job runs the agent logs the processes with an open sound stream
  (`audio.txt`; the game's own logs say nothing about its sound start). Stopping the guest's audio
  service instead would leave the game without any device, a different test. `-Sound` lets one job's
  sound out. The host's own volume mixer is never touched. Scripted runs stay quiet as before too
  (`LOCOFIX_QUIET`), the mute is the outer guarantee.
- *Looking in.* `tools\rig\peek.cmd` (`rig.ps1 -Peek`) opens a window onto the running box on the
  user's desktop, minimised to a taskbar button (`-Front`: shown at once). It takes the session over:
  the rig's own hidden client is left with "connection lost", the guest has the peek window's size
  and scale while it is open, and a running job goes on (checked: a game run with the session moved
  away and back drew and closed normally). Its sound is muted inside - the guest's own speaker icon
  in that window unmutes it, until the next job mutes it again. Closing the window closes only the
  view: the box runs on, and the rig connects its own client again at once (a game cannot start in a
  disconnected session) and puts back the size the last job asked for. Clicks into the guest do not
  disturb a run with the virtual test cursor (the engine ignores the real mouse in test mode -
  checked with real clicks sent inside the guest). `-Look` opens the guest's screen as the agent saved
  it last (every 5 s while a job runs); `test_missions.ps1` runs the game on a desktop of its own
  inside the guest too, so only `-Visible` game runs show on the guest's screen.
- *Another worktree's build.* `-Game <its build\game> -Tools <its tools>` runs it with its own
  scripts (a branch's `test_game.ps1` may set things the main one does not, e.g. a test registry key).
- *Missions in parallel.* `-Job missions` runs 4 lanes at once by default (`-Parallel`, 1 = one after
  another as on the host): the missions dealt out round-robin, each lane with its own copy of the
  game folder and results folder, every run on its own desktop inside the guest (as `test_game.ps1`
  runs quietly), the lanes' reports merged into one `report.txt`.
- *Side by side.* Only one Windows Sandbox can run on the machine (a second `wsb start` gives
  `CO_E_APPSINGLEUSE`, a second `WindowsSandbox.exe` "only one instance is allowed"; more boxes would
  need Hyper-V virtual machines set up by the maintainer), so the agent runs several jobs at once
  inside the one box. *Together:* quiet `-Job game` runs, `-Job missions` and `-Job probe` - each as a
  guest user of its own (`rigslot1`, `rigslot2`, ... made by the agent), so each has its own
  `HKCU` and with it its own test key `HKCU\SOFTWARE\Gromada\Locoland-test` (two jobs with different
  languages, resolutions or editor settings do not overwrite each other; the game is unchanged), its
  own `%TEMP%`, its own copy of the game folder (`C:\work\games\<build>-s<slot>`, mission lanes
  `...-lane<n>`), its game on a desktop of its own; a hung job's timeout ends only that user's
  processes. *Alone* (the job waits until no job runs and nothing starts beside it): `-Job setup`
  (in a fresh box), `-Job script` (it may move the real cursor or change the guest), a visible game
  run (`Visible`, `Screen`, `SetFull = 1`, steps `A` or `Q`), `-Sound` (one sound device),
  `KeepRegistry`, and `-Exclusive`. *The display* belongs to the whole guest: every job runs under
  the display it asked for (`-Resolution`, `-Scale`) - a job that names none under the default, the
  box's starting size at 100 % (`-DefaultResolution`), never what an earlier job left - so the agent
  changes it only while no job runs (it asks the host in `agent\display-want.json`; whichever
  `rig.ps1` is waiting for a job sizes the window - first onto the monitor, since Windows resizes a
  window that lands on a monitor of another scale, then to the size - and answers once the guest has
  kept it for 3 s) and a job that needs another display than the running ones waits for them to
  end; meanwhile jobs queued before it that fit the current display may still start, so jobs of one
  display go together. If the display changes under a running job anyway (a peek
  window, an older `rig.ps1`), its `done.json` and the caller's last lines say so. *Turns:*
  round-robin between callers (the worktree a job comes from): the oldest job of the caller served
  longest ago goes next, and while it waits (for the box to empty, its display, room) no later job
  overtakes it. *Limits* (set when the box starts): at most `-MaxJobs` 6 jobs and `-MaxGames` 9 games (4 and 6 until 2026-11-14; raised for seven sessions sharing the box on a 24-thread, 64 GB host)
  at once (a missions job counts its lanes; a single larger job still runs, then alone), box memory
  `-MemoryMB` 12288. *Checked* (2026-11-13, in the shared box while other sessions used it): three
  builds at once at one resolution, each its own language and frame limit; two resolutions never at
  once; a job asking 150 % (the TV) and the next naming none getting the default; a hung job killed
  at its timeout while the jobs beside it closed normally; an exclusive job waiting for the running
  ones and then holding the box; a fresh-box job restarting the box with the queue kept; the turns of
  two callers; an older `rig.ps1` queuing into the new agent and the other way round; the 20 missions
  alone and beside two other game jobs (6 games at once): the same report rows (screenshots differ as
  much as any two runs do), 89 and 90 s. 8 and 12 games at once still passed (missions 10-20 % and
  30-45 % slower). Under that load `test_game.ps1` once read the game's screenshot before the game
  had finished writing it (it waited a fixed 300 ms); it now waits until the file is whole.
- *Reading `-Status`.* `agent protocol 2: several jobs at once`, then the display (and the default),
  how many games and jobs run of the limits, one line per running job (slot, or `ALONE` and why;
  seconds of its timeout) and one per waiting job with why it waits: `waits its turn: <job> goes
  first`, `runs alone - ...: waits for N running job(s) to end`, `needs the display ... : waits for N
  running job(s) to end`, `waits for the host to set the display`, `all N job slots are busy`, `N of
  6 games running, it needs M`, `the box is held by <job>`, `needs a fresh box`. A waiting `rig.ps1`
  prints its own job's line whenever it changes. `agent protocol 1` is a box an older `rig.ps1`
  started: one job at a time; this `rig.ps1` then queues the old way and says so in one line. An
  older `rig.ps1` queuing into a protocol 2 box works too (its job runs with the agent's own runner;
  a job of it that names no display runs under the current one if no newer `rig.ps1` is waiting to
  set the default, with a note in `run.json`). `-Restart` puts a new box with this `rig.ps1`'s agent
  in once nothing runs or waits.
- *For sessions.* Just queue: quiet game runs and mission runs share the box, and a job's results
  are the same as alone (checked above). Ask for `-Exclusive` only when the job's result depends on
  having the guest to itself - a frame-rate or timing measurement, anything that touches the whole
  guest (its display settings, its sound, the real cursor) - not to make a job "safer": it makes every
  other session wait until it is done. Give a job the `-Resolution` / `-Scale` it needs rather than
  relying on what the guest has; jobs that ask different displays run one after another. Keep
  `-Timeout` near what the job needs: a hung job holds a slot until then. A peek window
  (`peek.cmd`) changes the guest's display while it is open: jobs that start then wait for it to close.
- *The same results as the host* (2026-11-12, `tools\rig\compare_runs.py <host results> <rig
  results>`): the fullscreen-cursor branch's build, whose 20 missions had run on the host with its own
  scripts, run in the box with the same build and scripts (`-Tools`), one after another and in 4
  lanes: all three reports the same row for row - every verdict and every script's compile line
  (byte code, variables, defines, stack). The screenshots: where the view is the same (missions 1-5,
  9, 10, 19: 0.6-5 % of the pixels differ) only moving things differ - units, smoke, fire, fountains;
  ground, buildings, the bar, texts and the minimap are the same to the pixel, so the picture is drawn
  the same. Elsewhere the view itself is a few to a hundred pixels off, or lava and water animate the
  whole screen: the test cursor starts where the real cursor was and the edge scrolling moves a big
  map's view before the screenshot, and the shot is taken at a slightly different moment of the
  animation - and two runs in the box differ from each other exactly as much as the host's run
  differs from the box's. So verdicts, logs and script results are comparable; screenshots are
  comparable in what is drawn, not pixel for pixel, on the host either.
- *What it takes from the machine* (`tools\rig\load.ps1`, performance counters, 24 logical
  processors, 64 GB): an idle box with the agent waiting 0.3 % of the CPU (at most 1.7 %), no GPU,
  2.8 GB of memory; the 20 missions in 4 lanes 11 % of the CPU on average (at most 23 %, about five
  cores), memory up to the box's 6 GB (the guest itself used at most 2.9 GB, the rest its file cache),
  the host's whole 3D engine at 3 % on average (the VM's own share cannot be told apart in the
  counters); the installer 5 % (at most 31 %) and 4.2 GB. Side by side (2026-11-13): each game
  running adds about 4 % of the machine on average (1 game 4-5 %, 2 games 8 %, 3 games 13-14 %,
  peaks 7 / 16 / 32 %) and about 0.35 GB of guest memory (the game's working set 185 MB plus its
  test script; 2.0 GB idle, 4.1 GB with 6 games); an idle box 0.5 %. The box shuts itself down after 10 idle
  minutes. Not chosen: a faster game clock - the missions test waits for a load, not for game time,
  and a different clock would be a different test.
- *Not covered* - these still need the real machine: frame-rate measurements that matter (the
  host's own desktop, monitor and refresh), the GPU's own driver path (the guest's is the
  paravirtual one), the monitors' real scales and modes (125, 150 and 175 % only where a host
  monitor has them), exclusive full screen and display-mode switches, how the sound sounds, what SmartScreen and
  Defender do with the unsigned Setup (Defender's service is stopped in the sandbox and SmartScreen
  has no network), Steam itself, and anything older than the host's Windows build.

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

The launcher offers only modes that fit the screen (`resolutions.cpp`): no wider and no taller than
a monitor in real pixels (the game is DPI-aware, `dpi_aware.cpp`), with each monitor's own size; a
device's list has room for 16 (`BUGS.md` F30), so the settings' list (room for 31) keeps the modes in
their order of worth - the game's own, each monitor's size, ours by use - and each device takes the first
16 of them that fit its monitor. With a 4K TV next to a 2560x1440 screen (2026-11-12) the primary got 16
(with one list of 16 for all it had 14, two places going to modes only the TV has), the TV 16 with its
3840x2160 and 2560x1600 (left out: 1280x960, 1920x1440, 2560x1080), the 1920x1080 monitor the 12 that fit. `DirectDrawEnumerateExA` lists every
monitor as a display device (here: 0 the primary display driver, 1 `\\.\DISPLAY2`, 2
`\\.\DISPLAY1`), each with its monitor (`Graphics_EnumDevice`, hooked to note it), and dgVoodoo lists
a device's modes only up to that monitor; the start window's test of the chosen mode and the saved
mode's check use the device's own monitor. The start window has no device list (`patch_exe.py`
moved it out), so the device is the registry's `Device`, 0 unless set by hand; a device other than
the primary display driver cannot render in a window, so the engine starts it in full screen, on
that monitor. Checked 2026-11-12 (quiet runs, test key): Device 2 listed up to 1920x1080 and a saved
2560x1440 ran at 1920x1080 and stayed saved; with the primary taken as 1600x900 (`-TestScreen`)
Device 2 still listed up to 1920x1080, devices 0 and 1 up to 1600x900. dgVoodoo lists only modes up to
the desktop's size and refuses a bigger one in full screen ("Couldn't set the display mode", seen
with 2560x1600 on a 2560x1440 screen), so a mode the list offers is one dgVoodoo can set. A saved
mode bigger than the screen is replaced, for that run, by the largest mode of the device's list
that fits, right after `Graphics_Construct` (the call at 0x41a518), before the start window selects
it and also when the start window is skipped (StartDialog=0, a restart) - a listed mode, since the
screen's raw size may not be one dgVoodoo can set (a remote desktop's 1916x1036). `Graphics_Init`
saves the mode it starts with (ScreenX, ScreenY); when that is still the replacement, LocoFix writes
the player's saved mode back right after it (the call at 0x41a887), so one start on a smaller
screen does not lower it for good; a mode chosen in the start window is saved as usual. Checked
2026-11-12 with a test screen of 1916x1036 (`test_game.ps1 -TestScreen`, test mode only) and a
saved 2560x1440: 1600x900 for the run and 2560x1440 still saved, with the start window and without
it; with 1280 x 720 picked in the start window (`-PickMode`) 1280x720 was saved. Before, the start window measured the mode against the screen as Windows reported it to an
unaware process - at 125 % or 150 % smaller than the real one - and ticked and greyed the full
screen box for every mode bigger than that; the full screen then was dgVoodoo's window of the
scaled screen while the game's picture had the real mode's size, and the cursor could not reach
the lower part of it (reported on v0.2.11; reproduced on a 4K TV at 200 % with the build before the fixes, where the
engine got the cursor only up to 1920 x 1080 of a 3840x2160 or 2560x1440 full screen - `BUGS.md`
"Running on current Windows"). In full screen dgVoodoo holds the real cursor to
(0,0)-(the mode) and the engine reads its screen position as game pixels: seen with the real cursor
at 1600x1200 on a 2560x1440 screen (2026-11-12), where the cursor stopped at 1599,1199 and every edge
and the bar were reached; full screen opens on the primary monitor even when the window's saved
place is on another one.

**Zoom with the wheel** (`zoom.cpp`, `zoom_view.cpp`, engine.md "Zoom"). In a mission the mouse wheel away
from the player draws the map bigger, towards the player smaller, 12 % a notch, with a glide of 220 ms eased
out (quick at first, settling softly; a notch during a glide goes on from where it is, so a fast turn is one
smooth move); the map point under the cursor stays under it (at the map's edges the camera's limits win). The
steps are a ladder with the level's own zoom on it (1, or mission 1's stretch of its 1024x768 screen): inward
1.12, 1.25, 1.40 ... up to 3 times (the last step 3 itself; 10 notches from 1), outward 0.89, 0.80, 0.71 ...
while the view stays within the map on at least one axis, then the whole map (its longer side, against the
window, fills the window: black at the sides or above and below) when that shows at least 3 % more; a step
within half a notch of the whole map is left to it. Changing the zoom makes no sound. A view over
`MAX_PIXELS` map pixels - the whole of the biggest original map, 3072 x 2304 - is not offered (in test mode
the test key's DWORD `ZoomMaxPixels` moves it, `ZoomGlide` sets the glide's ms, 0 none, and `ZoomFilter`
the stretch: 0 nearest, 1 filtered, 2 by the zoom). The interface is drawn apart from the world, at the
window's own pixels, so the bar, the minimap, the messages, the money plate, the tooltip and the cursor keep
their size at every step and at every interface size; the world is stretched nearest-pixel at 3
(exact big pixels) and filtered elsewhere. Border and arrow scrolling are as fast on the screen at every
zoom. The wheel does nothing over the interface, in a menu, while paused, while the scripts hold the camera
(an intro) and in the editor (its own: editor-ui.md "Zoom in the editor"); a menu keeps the zoom and looks as at zoom 1; a level change goes back to the
level's own zoom. At the level's own zoom nothing of it runs: the frame is the game's as before (checked
against the build before it at 1920x1080: the bar's dumps the same, the interface's pixels the same, the
frame rate not lower). The cost of a frame follows the logical pixels, plus the interface's pass (about
3.5 ms at 1920x1080, 6.5 at 2560x1440). In the test rig (its virtual adapter, mission 13, frame limit 60,
other games beside it - the host's own numbers are the table below):

| window | zoom (view) | fps |
|---|---|---|
| 1920x1080 window | 1 (1920x1080) / 1.5 / 2 / 2/3 (2880x1620) / 1/2 (3072x2160) / whole map (3072x2304) | 30 / 45 / 57 / 18 / 14 / 13 |
| 2560x1440 window, interface 3x | 1.5 / 2 / 2/3 (3072x2160) / whole map (3072x2304) | 34 / 46 / 13.4 / 12.8 |

A step takes 0.3-9 ms (new surfaces only when the view grows) and the process used at most 685 MB of its
2 GB of addresses. LocoFix.log gets each step's view, the switch's time and memory, and the average frame
time of its first 3 s.

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
The shadows' box had no margin at the sides even at 1024x768, so a shadow vanished as soon as the
middle of its object passed the left edge, while the shadow (it falls to the right and up) was
still on the screen (`BUGS.md` F28); they now have the objects' 512 px there too.
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
