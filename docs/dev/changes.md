# Changes from the original

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

## Changes from the original

- Language packs with a choice in the launcher: `lang/pl`, `lang/en`, `lang/de`, `lang/hu`,
  `lang/ru` and the Czech fan translation `lang/cs` (each with the custom maps page, the map editor and its help, POLIGON's texts; the
  German, Hungarian and Russian ones are assembled once their Steam depot is downloaded). Polish: translated developer mode and completed `strings.ini` (tips
  for the unit bar, buttons, strategic map, unit names). English: the same developer mode
  texts and `strings.ini` gaps filled in, the duplicate key 251 fixed (250/251 = Water
  Dragon).
- New languages use `lang/_template` as a starting point: copy to `lang/<code>/`, edit `language.ini`,
  translate the files of `game/`, add font mapping if needed. See `docs/TRANSLATING.md`.
- The intro: the OpenLocoland logo instead of the original's movies and title. The original starts
  with a chain of movies that `maps\logo.lgc` plays with `PlayMovie` (script function 142,
  `Movie_Play` 0x412ec0: a DirectShow filter graph - `RenderFile` - in a 640x480 child window over
  the game, any key or click stops the one playing and the next starts): `video\publisher1.wmv`
  (not shipped by Steam, so the original logs a `MOVIE ... RenderFile` error at every start and goes
  on), `publisher2.wmv` (the publisher, 12 s), `developer.wmv` (the developer's logo, 10 s) and
  `intro.wmv` (the story, 80 s; all Windows Media Video 8, 640x480, with a Windows Media Audio 2
  track), then two still pictures - VID 500 (`vid\500menu.vid`, a 640x480 sprite of (8, 8, 8),
  shown for the first half of a fade) and VID 481 (`vid\logo.bmp` of the language: LOCOLAND /
  STEAMLAND on black, 640x480, 24-bit BMP converted to a sprite at load) for 6 s (`WAIT_TIME`) or
  until a key or a click, both `MenuCreate`d at the screen's centre - then a fade and the menu.
  (The Russian depot has the same movies as `.avi` files, and its `logo.lgc` plays those names.)
  `build.py` does not copy the Steam `video\` folder (`SKIPPED_DIRS`) and `game/maps/logo.lgc.patch`
  puts the movies' part of the chain under `#ifdef ORIGINAL_MOVIES` (never defined), with an `#else`
  that takes the same steps without the movies (the same `iff` blocks, each `Pause(1)`) - so the
  patch does not touch the movies' lines and fits both depots (until 0.2.2 it commented the four
  `PlayMovie` lines out, which the Russian file does not have) - and the start is the title board alone: `game/objects.override`
  points VID 481 at `vid\481logo.vid`, which `build.py` makes at install time
  (`patch_data.make_logo_board`, so nothing of it is in the repository or the payload): a sprite of
  the board's kind and size (640x480, 16-bit, no palette, one frame) with only the logo's pixels
  drawn - `tools/icon/openlocoland_logo.png` (drawn by just_Martin, used as it is) cut to its letters,
  560 px wide, the soft edge laid onto black -, the rest undrawn, so the board is the logo over the
  game's black and nothing else, with the script's own fade in, 6 s and fade out. The board sits at
  the centre of the game area at every size: the logo map is 1024x768, so LocoFix's logical screen
  (`screen_scaling.cpp`) stretches that 4:3 picture to the window's height and centres it. Measured
  on screenshots (the bounding box of the non-black pixels against the centre of the game area): a
  1024x768 window (512, 384), a 1920x1080 window (960, 540), full screen 2560x1440 (1280, 720);
  every sampled point off the logo (0, 0, 0). The game's loading hourglass goes before the fade begins
  (shots at 60 ms steps), during the board the Windows cursor is hidden (the script's `SetCursor(-1)`,
  `Cursor_Hide`) and the menu shows the game's cursor again (sampled with `GetCursorInfo` through a
  full-screen start). The Steam `vid\logo.bmp` of the root and the packs
  stays in place, unused by the game (Setup's header still reads it as its fallback). A test build
  after v0.2.0 played an 8-second WMV clip of the logo first (`video\openlocoland.wmv`,
  `make_logo_video.py`); it went: the engine's 640x480 DirectShow window is not placed with the
  logical screen (it sat at the top left of a big window), `Movie_Play` moves the cursor to the
  window's corner and the game's loading hourglass (`vid\000load.vid`, drawn at the cursor while a
  level loads) froze there, since no frame is presented while a movie plays, and the logo came twice.
  An update removes that file and the empty `video\` folder (`OBSOLETE`, `clean_up`). The outro is a
  scene in `maps\outro` and stays.
- Mission 11 (`maps\l11\l11.map`): a rail at 745,2075 laid as a ramp the wrong way up
  is laid flat (`game/maps.override/l11/objects.tsv`, `BUGS.md` F21), so the line at x 745
  goes on south there instead of ending; trains no longer go round through the fort gate.
- A fort or a mine taken back from the pirates or the neutral army changes hands whole:
  `game/maps/script/export.lgc.patch` corrects the script library's `GETSPRITE_ARMY` (the mask of
  army 2 and 3 named the wrong armies, `BUGS.md` F29, GitHub issue #10), so the capture handler
  hands the dead gun sockets over with the building and the guns rebuilt there are the new owner's,
  not the pirates'.
- Maps made in the editor: a train that another army takes - the player's locomotive coupled to the
  wagons of an enemy train whose locomotive is gone, or a repair point taking towed wagons over - loses
  the order the editor gave it (an AREA watch, hunt, guard, target, drive or patrol, the AI's post or
  patrol, the patrol of an arrival or of a factory's plan on its way): the runner of the editor's
  arrivals block logs `ORDER n task t: the train is now army a's - order dropped`, frees the AI's slot
  that held it and gives it nothing more, so the player's train does only what the player orders. It
  used to drive the player's train back to the order's place and at the order's "foes". A map gets the
  new runner when it is saved again in the editor (editor-mission.md "A captured train loses its order").
- Maps made in the editor: a train of the map with an AREA watch, a hunt or a guard and REBUILT WHEN LOST
  is built again when it is lost, as a train with a drive or a patrol always was; the runner never took
  such a train as its plan entry's first train (it looked only for an order in the state of a drive or a
  patrol), so its loss went unseen. The train built again goes back to the lost one's place and watches,
  hunts or guards from there. A map gets the new runner when it is saved again in the editor
  (editor-mission.md "Rebuilt when lost").
- The strategic map of the menu has a second page with the custom maps (every map folder of
  `maps\`, POLIGON first: "Map files") and the map editor entry (`game/maps/menu.lgc.patch`; "The custom
  maps page" above).
- `objects.res`: 24 sounds pointing to non-existent files get `wav\null.wav` (the game's
  empty sound), so the engine does not look for them and missions do not report "Invalid
  nsfx". Sounds as in the original (`tools/patch_data.py`). The patch checks the `wav`
  folder of the assembled game: a sound whose file is found there (e.g. added to
  `game/wav/`) is left unchanged.
- `vid\110.vid`, the fort cannon: the animation of a newly built cannon rising out of its
  pit (`ANI_BIRTH`, 6 frames per direction) was rendered only for directions 0-19 of 32 and
  the file ends early, so `Vid_Read` logged "noCadr for noAnimCadr and noDir" and "14/15/16
  noCadr and noAnimCadr and noDir" (animations 14-16, not directions) and dropped the
  animation: a built cannon just appeared. Directions 20-31 now reuse frames the file
  already has - the pit, the rising frames of the nearest rendered direction and the
  cannon standing in its own direction (`tools/patch_data.py`) - and every direction has
  it (direction 0 needed the LocoFix fix of `Object_Construct` below).
  Cannons placed on the map at the start never play it (no birth while a level loads).
- `vid\450side.vid`, a new image: the side tiles of the state bar for screens wider than
  1024 (LocoFix fix 23). `patch_data.make_status_bar_sides` cuts them from the bar's own
  image (`vid\450sbar.vid`, 1024 x 50) when the game is assembled: the left end with its
  seam (columns 2-107) and the right one (916-1021), each as a tile repeated outward and
  as one next to the bar, with 16 columns at every join cross-faded, so tile after tile
  and the bar run into each other without an edge. `game/objects.override` adds VID 938
  (`Statebar side`, like VID 450: screen coordinates, layer 2000, 4 directions = the 4
  tiles) for it.
- `vid\939edit.vid`, a new image: the icons of the editor's bar that the state bar's atlas
  (`vid\455icon.vid`) has no icon for (see "Editor mode"). `patch_data.make_editor_icons`
  makes them from the game's own pictures when the game is assembled, in the atlas' style -
  a 40 x 40 cell with the gold frame and the dark striped ground of an atlas cell (cut from
  one) and the object in light grey: the cursor's arrow (`vid\001curs.vid`), the middle of a
  diagonal rail (`vid\610.vid`), the same with the atlas' cross over it (a section to
  delete), the fort (`vid\162.vid`), the semaphore with its green and its red lamp
  (`vid\152.vid`, the lamp keeps its colour), a bush (`vid\135.vid`) and a horizontal rail
  under the green arrow of the atlas' exit button (a push line), as a texture image like the
  atlas (a 256 x 256 R5G6B5 texture, six cells a row). `game/objects.override` adds VID 939
  (`Editor icons`, like VID 455, one direction per icon) for it; besides those, the ground
  height brush, the money crate and the palette (a fort, a bush, a semaphore and a crate in
  four quarters), the ground's paint (a brush stroke), water (a pool of the light water's own
  picture), a region (a dashed rectangle) and Mission settings (a gear: eight teeth round a ring).
- `editor\palette.tsv` and `vid\944pal.vid`, the editor's palette ("All objects"):
  `patch_data.make_editor_palette` reads `game/editor/palette.tsv` (the 244 kinds of object the
  missions use, by group, with the army the missions give them and the `[Editor]` key of the
  name, or none for a unit named by the game's `[Units]`) and writes it to the game folder with,
  for each object, its icon's frame and - for a unit - the behave and build list of the first one
  on the missions' maps; the icons, the object's first frame of its own sprite scaled into a 40 x
  40 atlas cell, are VID 944 (`Editor palette`, `game/objects.override`, one direction per icon);
  a car of the Vehicles palette has the state bar's icon of `vid\455icon.vid` instead (its turret's
  weapon's `iconFrame`, else its own), but the explosive tank (56) - the oil tank's sprite - has the
  passenger car's icon 31 (its weapon 32; the state bar in play shows that too), so it gets its own
  sprite (`STATE_BAR_ICON_NOT_ITS_OWN`; the two had looked like one car twice).
  `build.py` leaves `game/editor/` out of the plain copy.
- `Locoland.exe` = `Steam.exe` after the patches from `tools/patch_exe.py`:
  1. loads `LocoFix.dll` before WinMain (the call at 0x45952b goes through code at
     0x474830: `LoadLibraryA`, then a jump to WinMain; without the library the game starts
     without the fixes),
  2. launcher: the "Pełny ekran" box under the picture, no device list (they are all
     dgVoodoo) and no technical fields, "Anuluj"; the picture (`START_BITMAP`) gets our
     OpenLocoland wordmark over the original artwork (transparent PNG cut to its letters,
     soft black gradient behind it, written in place - the bitmap keeps its size).
- `LocoFix.dll` (`src/LocoFix`) - in-memory fixes applied on load:
  1. the launcher does not force full screen with dgVoodoo (test at 0x40d95e skipped),
  2. the window's game area equals the chosen resolution (AdjustWindowRectEx), and without
     a saved position it starts in the middle of the monitor's work area (CreateWindowExA
     at 0x41a832) - and so it does when the saved position is on no monitor: the game saves
     the window rectangle on exit, which is (-32000, -32000) for a minimised window, so
     the next start put the window out of sight; the menu bar of the editor mode is counted
     in, so the game area keeps its size there too; the frame is computed for the monitor the
     window opens on, and a window moved to a monitor of another display scale keeps its game
     area's size (`WM_DPICHANGED`, the game being DPI-aware - fix 46),
  3. picture and mouse are computed relative to the game area, not the whole window
     (6 GetWindowRect calls),
  4. more resolutions in the launcher: the built-in list (640x480, 800x600, 1024x768, in
     the settings struct `0x47b1f0`) gets 1280x720 ... 3840x2160 and the desktop
     resolution - only the modes no wider and no taller than a monitor in real pixels (fix 46),
     each monitor's own size among them, each device the first 16 of them that fit its monitor
     (fix 45), and each display device (a monitor:
     the registry's Device, 0 = the primary display driver) measured against its own monitor - its
     list, the start window's test and the saved mode's check; the game shows those the card supports. A saved mode bigger than the
     screen (another monitor, another display scale, a remote desktop) is replaced for that run
     by the largest mode of the list that fits (after `Graphics_Construct`, before the start
     window), so full screen is never bigger than the screen; the saved one stays in the
     registry unless the player chooses another in the start window (written back after
     `Graphics_Init`, which saves the mode it starts with),
  5. a map smaller than the screen sits in the middle instead of at the right and bottom
     edge (`SetPosition` hooked - while the camera is clamped, the map bounds are widened
     to the view size; for a level held in editor mode by 300 px more on every side, times the
     editor's interface size),
  6. logical screen (`screen_scaling.cpp`, see "Resolutions and widescreen"): on a map
     smaller than the window the game runs at min(window, map) and the picture is
     stretched to the window; menus made for 1024x768 (background VID 465 or 466) are
     stretched the same way; the mouse is mapped back in `WM_NCHITTEST` - only a point in the
     game area: mapped, a point over the editor's menu bar landed outside the window, the hit test
     said HTNOWHERE and File / Edit / Options never opened in a window bigger than the map,
  7. log clean-up at startup (see "Logs" above),
  8. languages (`language.cpp`): the game reads files from `lang\<code>\` of the chosen pack
     first (`CreateFileA` and `GetPrivateProfileStringA/IntA` imports replaced) - every open of
     an existing file, also one that asks for writing: script `FOpen` opens with `"r+t"`, and
     until the installer's English default showed it (English fonts, the Polish Steam copy's
     objectives) the mission texts - objectives, win and loss - came from the game folder's
     language whatever pack was chosen; files the game creates (high scores, logs) are not
     redirected and no pack holds player data (`languages.NOT_LANGUAGE_FILES`); the launcher
     gets a language list next to the full screen box, which switches the launcher labels
     (`[launcher]` in `language.ini`; the game's own Full screen and Cancel are ANSI controls,
     whose `SetWindowTextW` went through the system's code page - "??????" for Russian on a
     Polish Windows -, so their text is set with `DefWindowProcW`, kept as it is; checked in
     Russian and Czech) and is saved in the registry (`Language` next to the
     game's settings); without a saved choice the system language is used if there is a pack,
     and a saved language without a pack (a depot not downloaded) falls back to English, or
     without it to the root's language (`lang\root.txt`); the list is owner-drawn so that the
     open list can add the unofficial mark after a fan translation's name (`base=` another
     language) while the narrow closed box shows the name only,
  9. objects and shadows across the whole screen (`view_culling.cpp`): before drawing, the
     engine skips objects outside a fixed box around the screen centre (x +-1024, y +-512;
     for shadows x +-512, y +-512), so at 1920x1080 shadows ended 512 pixels left and right
     of the centre, and on screens wider than 2048 or taller than 1024 pixels whole objects
     were missing at the edges; the box now grows with the screen (the same up to 1024x768),
     and the shadows get the objects' margin at the sides (`BUGS.md` F28): a shadow no longer
     vanishes when the middle of its object passes the left edge of the screen (GitHub issue #13),
  10. camera panning with the middle mouse button (`mouse_pan.cpp`): hold it and drag the
     map, next to the arrows and the screen edge (the engine declares `SCROLL_WITH_MOUSE`
     but never implemented it). `World_UpdateScroll` puts the camera at the start
     position minus the cursor movement in logical pixels through `SetPosition`, so the
     map bounds apply; only when the script allows border or arrow scrolling, not while
     paused or in a menu,
  11. frame rate limit and counter (`frame_limit.cpp`): the game drew as many frames as it
     could (about 500-600 fps on the logo, 310-330 in the menu, 180 in a mission, a whole
     CPU core busy); now `GameWorld_Run` waits after presenting each frame until the next
     one is due (Sleep with a 1 ms timer resolution and a short spin for the last 1.5 ms).
     The launcher gets a row under the resolution: "Limit FPS" (30, 60, 75, 120, 144, 165,
     240 or ∞ = none; default 60) and "Licznik FPS", the counter, drawn on the final picture
     at the top edge left of the game speed and money boxes, so it covers no part of the
     interface. The game speed does not change: every game step advances the game clock
     by the real time since the previous one (at most 71 ms). `LocoFix.log` gets the frame
     rate of every level with the game clock against real time (the game's own "Average
     fps" in `error.log` comes out as 0 for missions),
  12. the whole birth animation of objects created facing direction 0
     (`birth_animation.cpp`): `Object_Construct` set the last frame of the start
     animation to its frame count - 1 without adding its first frame, and
     `Object_SetDirection`, which computes the range for the other directions, skips a
     direction that has not changed - so an object created facing 0 (the default of
     script `CreateSprite`), and any object of a VID with one direction, played only the
     first frame of its birth (or death) animation. One byte at 0x445a28 (`mov` -> `add`)
     makes it first frame + count - 1,
  13. `Engine_AdoptNeighbourCommand` (`adopt_command.cpp`, `BUGS.md` C1): the branch that
     takes the command and goal from the car in front read them from the car behind
     instead, and read from address 0x28 (a crash) when there was no car behind. Two
     instructions at 0x407d46 now use the car in front, like the branch before them,
  14. `Player_CanBuild` (`unit_limit.cpp`, `BUGS.md` C2): the check of the per-army unit
      limit (`VID_MAXUNITARMY`) compared the per-army count against the **total** limit
      (`VID_MAXUNIT`), so a VID with only a per-army limit could never be added to a factory
      build list. Six nops at 0x432492 leave the per-army limit in the register for the
      comparison; nothing changes in the original missions (they set no per-army limit),
  15. script `Random(max)` (`script_random.cpp`, `BUGS.md` F6): the engine evaluates
      `rand() % (max + 1)`, so `Random(-1)` divided by zero and killed the process with
      `EXCEPTION_INT_DIVIDE_BY_ZERO`. `call crt_rand` at 0x44063a is replaced by a
      trampoline that turns a zero divisor into 1 - a negative max now returns 0,
  16. script `Log(text)` (`script_log.cpp`, `BUGS.md` F7): the text went to `Log_Write` as
      the printf format, so a `%` in a logged string read stack garbage or crashed on `%s`.
      The `call Log_Write` at 0x4405fb is replaced by a trampoline that passes "%s" as
      the format and the text as its argument,
  17. the shadow point buffer leak of `VidConverter_BuildShadowPoints` (`shadow_leak.cpp`,
      `BUGS.md` F5): the failure path returned 2 without freeing the first buffer,
  18. the script value helpers rewritten natively (`script_value.cpp`, `script_value.h`,
      `world_script.cpp`, `world_script.h`): `String_ToInt` (0x43d8c0),
      `ScriptValue_BinaryOp` (0x43da60), `ScriptValue_ToString` (0x4424c0),
      `World_PopInt` (0x4429c0), `World_PopString` (0x442a20),
      `World_PopObject` (0x442a40), `World_PopVid` (0x442ae0) and `Script_PopObject`
      (0x442670) - the helpers every script extern reads its arguments with - are
      reimplemented in C++ and hooked at their entries instead of patched byte by byte.
      `a % 0` now returns 0x0FFFFFFF like `/ 0` instead of raising
      `EXCEPTION_INT_DIVIDE_BY_ZERO`, and `INT_MIN / -1` / `INT_MIN % -1` no longer
      overflow the `idiv` (`BUGS.md` F8); a text that `sscanf` does not convert gives 0
      instead of the String pointer (`String_ToInt`), the World pointer (`World_PopInt`,
      `World_PopObject`), the script machine (`Script_PopObject`) or the text of the
      caller (`World_PopVid`) it did not overwrite (`BUGS.md` F9).
      `tools\test_script_value.py` checks the C++ against the original emulated in Unicorn
      on 2000 random cases plus edge cases,
  19. test mode (only with `LOCOFIX_TEST=1`; with `LOCOFIX_QUIET=1` as well the sound and music
      stay at 0; the settings in `HKCU\SOFTWARE\Gromada\Locoland-test`, not the player's key,
      `test_registry.cpp`) - described below in "Working with the code",
  20. test map (`test_map.cpp`, only with `--test-map <map>` or `-map <map>` on the command
      line): the game loads that map in place of the first level, so the intro and the menu
      are skipped and a test starts a mission the way the menu's "Uruchom grę" does with a
      script's `Load()` (`World->levelFile` then shows whether it really loaded). The
      switch cannot be passed to the game as it is: `World_Construct` takes a non-empty
      command line as the map to start with, so the name is replaced just before
      `Level_Load`. `Level_Load` takes the name as a String by value and frees it, so the
      replacement is allocated on the game's heap (`crt_new`) and the requested one is
      freed in its place (a static buffer of the DLL there made the game's heap crash
      after loading the script once other variables moved in front of it),
  21. the mouse position with its sign (`mouse_coords.cpp`, `BUGS.md` F13):
      `Mouse_HandleMessage` split the `WM_NCHITTEST` position with `and 0xffff` / `shr 16`,
      so on a monitor left of or above the primary one, and in a minimised window (Windows
      puts it at -32000), the cursor came out 65536 px off and was clamped to the right or
      bottom edge of the view - over the HUD bar, so no unit could be selected or ordered
      and the map scrolled to that edge. `movsx` / `sar` (same length) read it signed,
  22. a second row in the factory panel (`depot_rows.cpp`): the panel shows ten units of a
      factory's build list in one row (bar objects 80-89), but the game has 14 factory units
      (the hotkeys F1-F12, `[` and `]`). After `Player_SetPanel` builds the list, a row 41 px
      above it shows units 11-20 - a strip (VID 497), icons (VID 455) with the affordability
      of the engine's own (`Player_CanBuild`), build times and hotkey labels (VID 8) - and
      `Player_HandlePanelAction` turns a click on it into the right entry (the engine would
      read another row as a queue slot) and refuses an entry over ten the factory cannot build.
      Checked on the sandbox (a factory with all 14 units): a click on the new row queues the
      eleventh unit, the first row and F12 work as before; at 1024x768 and 1920x1080 the row
      stays 41 px above the first one while the map scrolls,
  23. the state bar across the whole screen (`status_bar.cpp`): the bar's base (VID 450)
      is 1024 px wide and centred, so on a wider screen the map showed on both sides of it.
      When `Player_ShowStateBar` creates the bar, side tiles (VID 938, `vid\450side.vid`,
      above) are lined up from both ends of the base to the edges of the view on its layer
      (z 2000, under the minimap and the other parts of the bar), and
      `Player_HideStateBar`/`Player_ShowStateBar` hide and show VID 938 with the bar (the
      Esc menu, the strategic map). Without VID 938 in objects.res nothing is added.
      Checked at 1920x1080 on the sandbox: ten tiles, the plates and seams run on from the
      bar to both edges.
  24. the player data in `HKEY_CURRENT_USER` (`player_registry.cpp`, `BUGS.md` F15): the
      scripts keep the players, their progress, scores, rank, sound settings and the menu's
      `return` flag with `GetReg`/`SetReg` under `SOFTWARE\Gromada\steam\Players` in HKLM,
      and current Windows 11 refuses those writes (`ERROR_ACCESS_DENIED`) instead of
      virtualizing them, so nothing was saved. `Registry_ParsePath` (0x427340) is hooked:
      that key and the keys under it go to `HKEY_CURRENT_USER\SOFTWARE\Gromada\Locoland\Players`
      (the path is allocated with the game's `crt_new`, the old one freed with `crt_delete`).
      At the first start that key does not exist; it is created and filled with the values of
      the HKLM key and then those of the VirtualStore copy
      (`HKCU\Software\Classes\VirtualStore\MACHINE\SOFTWARE\WOW6432Node\Gromada\steam\Players`)
      over them, so the player keeps the progress; `LocoFix.log` says how many values came
      from each. Deleting the HKCU key makes the next start copy the old data again.
      Test runs of the game save there too (a test map writes the `return` flag; the missions
      are not won, so no scores),
  25. the editor mode (`editor.cpp`, only with `--editor` on the command line): the engine's
      own mode for the authors' map editor, with the File menu's commands handled, the
      loaded map held so that it is saved as authored, and its objects edited in the game
      (`editor_edit.cpp`: select, move, turn, army, delete, undo and redo, a bar in the state
      bar's look to place new ones, the Track tool laying rails by the rules of `rail_rules.h`, a
      deleted rail leaving the rail network, the Push line tool, the ground's heights, the gravel bed and
      decals in quantity; `editor_newmap.cpp`: File > New; `editor_mission.cpp`: the
      mission's settings, texts and push lines in the script's editor blocks) - see "Editor mode",
  26. safe map saving (`level_save.cpp`, `BUGS.md` F16-F20): `Level_Save` writes every map to
      `<target>.saving` and replaces the target only when that worked, so the loaded map is
      never renamed to `tmp_del!.map` (in the current directory, and left there when the
      save failed), another spelling of its name no longer truncates it before its OBJ chunk
      is copied, and a failed save keeps the old file; a level whose VIDs come from a map file
      that is gone is not saved (but the editor's Run the level copy held again, whose OBJ is
      taken from the map it was written from - "Saving" in `editor.md`); the ground the engine
      makes itself does not make it copy OBJ;
      `ResFile_CopyChunk` is rewritten to copy record by record (it appended 16 bytes of the
      next chunk to the OBJ record on every save); and HEAD gets the time 0 instead of the
      game time, so a map saved twice gives the same file,
  27. restarting the game (`restart.cpp`): script `StartEditorMode(map)` (extern 200, used by
      the menu's "EDYTOR MAP") and the editor's File > Play the map / Back to the menu start the
      game again with or without the editor mode; `--no-launcher` skips the launcher (the
      settings are in the registry already), `--after <pid>` waits for the old game to end and
      `--return-editor` (Play the map) goes back to the editor when the played map is left for
      the main menu.
  28. the mouse wheel for scripts (`menu_input.cpp`): script `GetMouseWheel()` (extern 201,
      dispatched by `restart.cpp`'s hook of `Script_CallExtern`) pushes the wheel's turn of the
      frame (Mouse+0x04, delta / 120, which `Mouse_HandleMessage` keeps but no script function
      read); the custom maps page of the menu scrolls its list with it,
  29. the editor's texts in the chosen language (`editor_text.cpp`: `[Editor]` of `strings.ini`, the
      English text when the pack has none) and its Help and Menu buttons in the bar's round
      sockets with their panels in the look of the game's menu pages (`editor_panel.cpp`, F1 the
      help, Ctrl+R runs or holds the level) - see "Editor mode",
  30. the player's options (`options.cpp`): the launcher gets a row under the frame limit with
      "Kody twórców" / "Cheat keys" and "Wszystkie misje" / "All missions" (labels `cheats`,
      `all_missions` of `[launcher]` in `language.ini`), saved as `Cheats` and `UnlockMissions`
      next to the game's settings; both off by default (see "Quick start"). The scripts read
      them at compile time from two files of `game/maps/script/`: `constants.lgc` includes
      `cheats.lgc` (nothing defined: no developer keys) and `menu.lgc` includes `unlock.lgc`
      (`option_unlock_all=0`); when an option is on, the game's `CreateFileA` of that file opens
      `cheats_on.lgc` (`#define KEYS_ENABLE 1`) or `unlock_on.lgc` (`option_unlock_all=1`)
      instead - the hook is in front of the language packs' (`language.cpp`), whose lookup gets
      the new name. A file the compiler includes must end with a declaration: one ending with a
      directive (`#define`) stops the compile with "End of file", so each holds an `int option_...`.
      `CreateLevelMenu` of `menu.lgc` does not disable the dots after the progress while
      `option_unlock_all` is set; the progress itself (`l<player>` under `Players`) is written by
      `menuend.lgc` (the win and `InstantWin`, the root's and the `lang/pl`, `lang/en` patches)
      only when the mission won is the next one in line (`defle==LEVEL`; the original's
      `defle<=LEVEL` would let a later mission won while unlocked move it forward). The launcher's
      caption also shows the version (`LOCOFIX_VERSION`, "Release"). Checked in the game: with
      Cheats on M gives the money (300 -> 1000) and W wins mission 2, with it off neither does;
      with All missions on a test player of progress 3 sees all 20 dots playable, plays mission 10
      and wins it (W): the score `s1l9` is saved and the progress stays 3; with it off the map
      shows the progress 3 again (mission 10 cannot be clicked) and a win of mission 4 makes it 4,
  31. a faster float-to-integer conversion (`fast_ftol.cpp`): the exe turns every float into an
      integer through VC6's `_ftol` (`crt_ftol` 0x456a0c, 351 call sites - the software sprites,
      the light buffers, every coordinate), which sets the FPU's rounding to zero and back around
      a `fistp`; the two control word changes stall the FPU. With SSE3 (every x86 CPU since 2005)
      the function becomes `fisttp`, which truncates by itself: the same 64-bit result in
      edx:eax for every value (out of range and NaN give the same integer indefinite), st0
      popped as before. In a sampling profile of mission 13 at 2560x1440 `crt_ftol` was 8% of the
      main thread, after it 1.4%,
  32. no second lock for an empty layer 10 (`layer10_lock.cpp`): `Graphics_DrawScene` locks the
      back buffer and the depth buffer once for the software sprites of layers 2-4 and again, after
      the Direct3D layers 5-9, for layer 10 - which holds only the on-screen messages (VID 14) and
      is empty most of the time. Each lock of a surface the GPU drew on makes dgVoodoo wait for the
      GPU and copy the whole surface to the CPU (and back at the unlock); at 2560x1440 the locks
      were about a quarter of the main thread. With nothing in layer 10 (no object, and no member
      of the cursor chain drawn there) the block is skipped; with a message on the screen the
      original code runs. 5 locks per frame became 3,
  33. "check for updates" in the launcher (`update_check.cpp`): a row under the options with the
      button "Sprawdź aktualizacje" / "Check for updates", the box "przy starcie" / "at start"
      (`UpdateCheckAtStart`: the check runs as the launcher opens; ON by default since 2026-11-06 -
      no value in the registry is on, the player's saved choice stays, so whoever unticked it keeps
      it off; the box is saved as soon as it is clicked, not only with OK; at most once a day: not
      when `UpdateLastCheck` is today's - "update check at start: skipped - already checked today",
      the button always checks; no window, only the line, and without a connection the check at
      the start says calmly `update_offline_start` - "No connection - the check at start was
      skipped; the box beside turns it off." - instead of `update.txt`'s notice, which stays for
      the button; before, it was off so that the game asked the internet only when the player said
      so; checked with LOCOFIX_UPDATE_URL on a host that does not exist (the calm line at once),
      GitHub itself (up to date), a second start the same day (no request in the log), the box
      unticked and the launcher started again ("off (the player's choice)", no check), the test's
      release (New version and Download at start), the line in Polish and German), a link "GitHub"
      (the releases page, or the new release's; its right button opens it or copies the address)
      and a result line with a button beside it (labels `check_updates`, `check_at_start`,
      `update_*`, `open_link`, `copy_link` of `[launcher]`, filled in with %1 / %2). The check
      runs in a thread of its own: a GET of
      `https://api.github.com/repos/Dysprohol/OpenLocoland/releases/latest` with WinHTTP (a
      User-Agent "OpenLocoland/<version>", 5 s timeouts and a 5 s deadline of the dialog's own -
      WinHTTP let a host that does not answer take 8 s; an answer after it still counts), and the
      answer is read as text: `tag_name`, the release's `html_url` and of the asset whose name ends
      with `Setup.exe` its `browser_download_url`, `size` and `digest`. The tag is compared with
      `LOCOFIX_VERSION` by its numbers (a leading "v" dropped, a missing number 0): a newer tag
      shows "Jest nowa wersja ... (masz ...)" / "New version ... (you have ...)" with Download, the same
      or an older one "Masz aktualną wersję" / "You have the latest version"; a build after a tag
      (v0.1.10-17-gcef9557) counts as the tag, and "dev" has no numbers, so it shows the latest
      version without Download. Download writes the Setup into the Downloads folder
      (`FOLDERID_Downloads`) as `<name>.part` with the progress in the line and Cancel on the button,
      checks its size and SHA-256 against the release's (`bcrypt`), marks it as downloaded from the
      internet (`Zone.Identifier`, as a browser does: Windows checks it when it starts) and renames
      it; Run Setup asks first and starts it, then closes the game (the launcher's Cancel), so Setup
      can update the install. No answer, an HTTP error (404: the repository gone) or an answer
      without a release shows the notice of `update.txt` in the game folder (`game/update.txt`,
      UTF-8: a line per language pack code, `en=` for the packs without one, and `link=`, the
      address the link opens then) or, without the file, the English one built in; a later Setup or
      the player can replace the file. The date and the tag of the last answer are kept as
      `UpdateLastCheck` and shown until the next check. WinHTTP, bcrypt, shell32 and ole32 are
      delay-loaded (`build_locofix.py`): a launcher where nobody checks loads none of them. The
      controls use the TrueType Microsoft Sans Serif in the dialog font's size: the bitmap MS Sans
      Serif has only the letters of code page 1252, and a button measured its Russian label with it
      but drew it with a wider fallback, cut off. `LOCOFIX_UPDATE_URL` replaces the API's address
      (tests). `LOCOFIX_UPDATE_TEST=<file.json>` makes the check read that file instead of asking
      GitHub (an answer of the API saved, or written by hand: `tag_name`, `html_url` with
      `/releases/tag/`, an asset with `size`, `digest` "sha256:<hex>" and `browser_download_url`),
      and the Setup may then be a `file:///` address (`%20` for a space), read from the disk with
      the same checks, zone mark and Downloads folder; every step is logged ("update check: the
      test's release ...", "update download: the test's Setup ...", "update: Setup ... started, the
      game closes", "update: the link ... opened"). The flow against a real Setup: build it
      (`tools\release.py`), a game with its LocoFix.dll (`build.py --target build\game_upd --locofix
      build\release\LocoFix\LocoFix.dll`: a numbered version, which a dev build is not), copy
      Setup.exe to a name of its own ending in Setup.exe (Downloads may hold the player's
      Setup.exe), write the JSON with a higher tag, its size and SHA-256, start the game with the
      variable set, then Check, Download, Run Setup, Yes. Windows then shows its own "Open File -
      Security Warning" (unknown publisher: the file carries the internet zone mark, as a browser's
      download does) before Setup starts, and the game stays open until it is answered; Run starts
      Setup and the game closes. Checked 2026-11-06 with v0.2.2-30-g0fbebfe-dirty against a v9.9.9
      of its own Setup (24554127 bytes): New version, Download (size and SHA-256 checked), the
      question, the warning, Setup's window open with the game closed (then closed unused, the test
      file deleted), and a left click on GitHub opened the release page in the browser. Not tried:
      Cancel in the warning (ShellExecute then fails and the game should stay - "did not start").
      The check writes `UpdateLastCheck` into the game's registry key, shared with an installed
      game: put the old value back after such a test. Checked in the game with DLLs built as v0.1.9 (New version v0.1.10, Download: 37158157
      bytes and the release's SHA-256, the zone mark, Run Setup's question; Cancel halfway removed the
      part), v0.1.10 and v0.1.10-17-gcef9557 (the latest version) and dev, in all six languages;
      unreachable: a host that does not exist, a repository that does not exist (404) and an address
      that does not answer (the notice after 5.2 s), with the shipped `update.txt`, a replaced one
      (its notice, and its `link=` copied by the menu) and none (the built-in English); the box
      ticked checks as the launcher opens, a click on it saves it; since v0.2.13-beta.1 a box under
      the button, "Także wersje testowe" / "Include pre-releases" (`check_prereleases`; off by
      default: `UpdatePrereleases`, DWORD, no value is off, saved as soon as it is clicked; disabled
      while a check or a download runs, and a click puts a shown result back to the idle line, so no
      Download of the other setting stays), asks for
      `https://api.github.com/repos/Dysprohol/OpenLocoland/releases?per_page=10` instead and offers
      the release with the highest version, pre-releases included and drafts left out; a release
      marked a pre-release (or whose tag is one) is named with `update_prerelease`, "%1 (test
      version)". The versions are compared by `update_version.h` (`tools\test_release_versions.py`):
      a pre-release word after the numbers (`v0.2.13-beta.1`) makes a version older than the same
      numbers without it and newer than the version before; the commits after a tag and `-dirty` are
      left out as before; a bare commit hash, which used to read as a number when it began with a
      digit, is no version now. With the box off nothing changes for a stable install, and a player on
      a pre-release is offered the next release, never an older one. `LOCOFIX_UPDATE_TEST_LIST` gives
      a test the list's answer beside `LOCOFIX_UPDATE_TEST`'s latest. Checked 2026-11-20 on the test
      rig with DLLs built as v0.2.12, v0.2.13-beta.1 and v0.2.13-beta.1-2-gabc1234 against answers
      written by hand (the latest v0.2.12 or v0.2.13; a list holding v0.2.13-beta.1 marked a
      pre-release, or v0.2.13 over it): v0.2.12 with no value - the box unticked, "You have the latest
      version"; ticked, "New version v0.2.13-beta.1 (test version)" with Download (all six
      languages); a click on the box saved 1 and put the line back to "Last check", the button then
      offered the beta; v0.2.13-beta.1 with the box off and the latest v0.2.12 - "the latest
      version", no Download; with v0.2.13 out - offered with the box off and on, and to the build
      after the beta tag; the beta with the box on and only the beta listed - "the latest version";
      the launcher's row with the box in Polish, English and Russian looked at in its screenshots,
      and missions 1, 2 and 8 loading after its OK,
  34. the neutrals and the pirates of an editor map (`factions.cpp`, MAP_RULES.md 6d): on a map
      whose script has `#define FACTIONS 1` (the editor writes it, read from `<map>.lgc` in the
      `Level_Load` wrapper of `frame_limit.cpp`) the army test of `Object_FindTarget` (0x449f7b ...
      0x449ff3) jumps to `may_target`, and the two `ACT_DAMAGE` calls of an explosion
      (`Object_Action` 15, 0x447e9c and 0x447eb0) first call `note_hit`: a neutral hit by an army,
      and every armed neutral that sees it, answer that army - and only it - to the end of the level,
      and that army may fire at them; the pirates may fire at the neutrals. The player's mouse takes
      the pirates as the enemy (`enemy_test` in place of the four `and eax,0xc00 / cmp eax,0x400` of
      `Player_HandleMouse` 0x431914 and 0x431a17 and `Player_HandleInput` 0x4320b9 and 0x432120: the
      attack cursor and order, no repairs at their repair point, their hidden objects not under the
      cursor, no answer to a click) and the minimap draws them black (`minimap.cpp`, the loads of the
      colour of armies 2 and 3 at 0x405bd9 and 0x405c08) - editor-mission.md "The pirates for the
      player". On any other map
      `may_target` is the engine's test and nothing is noted, so the missions play as before,
  35. the red lights for chosen armies (`red_lights.cpp`, MAP_RULES.md 5): on a map whose script has
      `#define PASS_RED_LIGHTS <mask>` (bit n: army n; the editor writes it from any train's panel - every
      army's since the fight rules, at first the neutrals' and pirates' only -, AT RED LIGHTS: DRIVES ON, kept in `AiKnobs.red_lights`, so a change is one
      step of the history) a train of such an army runs along a one-way line against its way. The
      engine tests a line in two places, both against the car's army bits: `RailDot_CanPass`'s last
      test (0x433860: the far point's pushLink against the link's backLink, ebp the engine) - ours
      gives a chosen army's train a value that never matches - and `Train_Update`'s stop of a car
      heading into a line (0x40b60e, `cmp [edx+0xc],edi / je 0x40b636`: it stops the train, sounds
      0x94, turns it round and, moving to a point, drops its command) - ours goes on as if no line
      were there. A line's push is untouched: a train running along a push_flag 1 line is pushed,
      one running against it is not (the push needs the car's link to be the line's). Per army, not
      per train: the army is the one thing every car keeps and the map saves, while a train's cars
      swap ends when it turns round and trains couple in play. Checked in play on a POLIGON copy: a
      neutral train north of the player's repair point, ordered south against its line - with the
      rule off it drove round the loop's other leg (937,864 at 5 s), with `PASS_RED_LIGHTS 4` straight
      down the repair point's line (674,1164 at 5 s); both reached the point.
      A factory's exit line keeps the engine's rule (2026-11-10): a train of a chosen army ordered to a
      point on its factory's line drove up it to the door (a copy of a reporter's map with the pirates'
      factory at 501,1500 and its line 479,1424 -> 621,1424: with `PASS_RED_LIGHTS 8` the pirate train
      stood at 503..551,1500, without it past the line's end at 644..692,1500), where it holds the
      factory's next car. Both tests follow the point the train would enter back along its line to the
      line's first point, and a line starting within 100 px of a factory (VID 102; the campaign's start
      22-79 px from theirs) is not passed - LocoFix.log: `red lights: the line from x,y is the exit of
      the factory at x,y`, once a line. With it the pirate train stops past the line's end as without
      the rule. A repair point's line, which runs through the building, stays passable.
  36. lost surfaces (`lost_surfaces.cpp`, `BUGS.md` F22, "Rendering"): a frame waits until lost
      surfaces can be restored, and a lock that fails in the middle of a frame gets a stand-in
      buffer instead of the null pointer the software sprites drew through - Alt+Tab in full screen
      no longer crashes the game,
  37. the crash log (`crash_log.cpp`): a vectored exception handler writes an exception that is a bug
      (access violation, division by zero, illegal instruction, stack overflow ...) to `LocoFix.log`
      first chance - the code, the address as module+offset, the address read or written, the
      registers and the top 24 words of the stack, each pointer into a module's code named the same
      way - at most 4 a run; it handles nothing (the engine's filter still writes "EXCEPTION!" to
      `error.log` and ends the game),
  38. the fog buffer (`fog_buffer.cpp`, `BUGS.md` F23, "Rendering"): region fog and snow on the
      ground are drawn through a scratch texture of their own, big enough for the screen (the
      smallest power of two holding a quarter of it on each side: 512x512 at 1920x1080, 1024x512
      at 2560x1440; up to 1024x1024 the engine's 256x256 grey texture as before) - a fog over the
      whole view (missions 7, 13, 18, the editor's OVER: THE WHOLE MAP) no longer crashes the game
      above 1024 pixels,
  39. a car's rear end laid on its own side (`train_fold.cpp`, `BUGS.md` F24, "How the cars
      follow"): `Engine_UpdateRear` laid the rear end of a car coupled behind a car hanging past a
      buffer stop forward along the stop's one link - turned round, under the car in front - and
      sent a rear end meant to lie beyond a point back onto the car when the rear's link led back
      toward the front (at a dead end, and for cars `Engine_PutOnRails` made on open track); only a
      result that comes out folded is corrected - the rear end laid past the dead end, or beyond the
      point on the way on - and every other one is left as it was; LocoFix.log has a "train fold:"
      line for each car it lays so,
  40. a region's fog that pulses (`region_pulse.cpp`, `BUGS.md` Quirks, "A fog that rises or
      pulses"): the engine's flag 1 of a region only lets its fog rise once after the map is loaded
      (kept as it is), so a pulse is added on flag bits the engine does not read - with flags 1 and
      0x10 the Region vtable's Draw (0x4759a0 -> `Region_Draw`) goes through a hook that sets the
      fog's top (+0x70) from the game time before the original runs, swinging between the fog's low
      and high level once in 6 s (flag 0x20: 12 s, 0x40: 3 s), and the last count (+0x74) to 0, so
      the original leaves the top alone and draws as ever; it follows the game speed, stands under
      a pause and moves in the held editor; a region without both flags is drawn exactly as before,
      and without the hook such a map shows the one rise; `LOCOFIX_REGION_LOG=1` logs the top of
      every region with flag 1,
  41. a region's fog table freed twice (`region_fog.cpp`, `BUGS.md` F25): `Region_InitFog` frees the fog
      table of a region and keeps the pointer when the new fog is none, so choosing a fog again - or
      deleting the region - freed it a second time and the game crashed a little later (the editor:
      FOG: MIST, FOG: NONE or an undo, FOG: MIST); the pointer is cleared after the original ran with
      no fog to make,
  42. the minimap (`minimap.cpp`, `BUGS.md` F26, "The minimap"): its rectangle fitted to the frame's
      opening (a row higher - a click lands one row of the minimap higher than in the original) and
      the view's highlight kept on the map - the engine's ran up to three rows onto the frame at the
      bottom - and at least 7 x 5 px, so it can be found on a huge map; the dots stay inside. The
      minimap's size can be chosen on the game's settings page (main menu and Esc menu, a row of
      ours under the page's own: `game/maps/script/optfields.lgc`, arrows VIDs 952/953): the normal
      size (the default), twice (208 x 156) or three times as big, used as far as it keeps the frame
      clear of the bar's slots; the frames are VIDs 950 and 951
      (`vid\464x2.vid`, `vid\464x3.vid`, the frame enlarged pixel by pixel by `build.py`; an edition
      whose frame it cannot enlarge gets the frame copied as it is, which LocoFix keeps at its size), the dots
      grow with them, and clicks, the right-click orders, the cursor and the mouse tip treat them
      as the minimap; with a bigger frame the camera may go below the map as far as the frame is
      high, so its corner hides no more of the map than the original's. A small map's logical
      screen (mission 1: 1024 px wide) takes twice at most and is stretched as before. The choice is
      `MinimapSize` of the Players key, written by the script next to the sound settings ("1"-"3";
      absent = the normal size, so Setup writes nothing), read each time the bar is built,
  43. the version in the main menu (`menu_version.cpp`): "OpenLocoland v0.2.x" (`LOCOFIX_VERSION`,
      "OpenLocoland dev" for a build of `build.py`) in the menu's own font (VID 2,
      `fonts\font8x13.bmp`) at half its brightness, in the bottom right corner of the window - in
      the black bar beside the menu's picture on a wide screen (under it on a 5:4 one), on a 4:3
      screen in the picture's own bottom right corner, where the menu's wall has only its dark rim
      in every language - so the version can be seen in full screen too. It is drawn on the final
      picture before it is presented (`screen_scaling.cpp`, as the FPS counter), whole steps of the
      menu's stretch big (at 2160 px of height twice), only while the main menu's buttons (VID 467)
      are there and no briefing (VID 501): not in a mission or its Esc menu, not on the end screen,
      not in the editor; nothing reacts to the mouse there. While the settings page is open (its
      minimap row's arrows, VID 952, exist - in the main menu or the Esc menu) the corner shows the
      minimap's frame instead, in real pixels and where the bar will put it, at the size the game
      would use on this window (`vid\464preview1-3.bmp`, made by `build.py` from the same frame), and
      for a bigger one the line "A STRIP SHOWS UNDER THE MAP" above it (the pack's text and font);
      the version gives way meanwhile, and both follow a click and go with the page,
  44. the rain over the whole view (`rain.cpp`, `BUGS.md` F27, "The rain"): `Graphics_DrawRain` in C++,
      the same up to 1024x768; on a bigger view as many drops as the original has per 1024 x 768 px, and
      on a view higher than 768 px a drop made again starts at the top only with the chance
      (768 / height)², else anywhere in the view - the rain reaches the bottom of a big screen (it
      stopped at about 790 px, GitHub issue #13),
  45. 16 display modes at most (`mode_list.cpp`, `BUGS.md` F30): `Graphics_FilterDisplayMode` wrote
      every matching mode into a device's lists of 16 without a check, so a 17th overwrote the first
      mode's height and format (the start window's first line read "640 x 2560 x 0bpp" on a 2560 x
      1440 screen) and more would reach the device's flags; a mode that finds the list full is left
      out and named in LocoFix.log, and `resolutions.cpp` gives each device the first 16 of its list
      that fit its monitor,
  46. real pixels at any display scale (`dpi_aware.cpp`, "Graphics and dgVoodoo"): the game declares
      itself per-monitor DPI-aware when LocoFix loads, before any window; at 125 % or 150 % Windows no
      longer shrinks the screen the game measures (a 1920x1080 screen was 1536x864 / 1280x720 to it)
      nor enlarges the game's window as a picture. The start window opens unaware and looks as
      before (its picture has fixed pixels); its test of the screen size gets the real pixels,
  47. the interface's size (`interface_scale.cpp`, `status_bar.cpp`, `depot_rows.cpp`, `minimap.cpp`,
      engine.md "The interface's size"): the state bar - its base and side tiles, the slot numbers,
      icons, lines and texts, the two round buttons, the factory list and its second row, the money
      plate with the speed and money texts - and the minimap twice or three times as big, the map
      drawn as before. `Player_ShowStateBar` is ours in C++ (at the normal size the same objects at
      the same places as the engine's), the pictures are VIDs 1100-1172 made by `build.py` (the
      originals enlarged pixel by pixel, their boxes with them; an edition whose pictures cannot be
      enlarged keeps the normal size), the slot under the cursor is found with the bigger pitch and
      the camera's room below the map follows the bar's height by itself. The choice is a row of ours
      on the game's settings page (**INTERFACE SIZE**, arrows VIDs 954/955, `InterfaceScale` "1"-"3"
      of the Players key; absent = normal), offering only what the window takes (twice from 960 px of
      height and 1328 of width, three times from 1440 and 1992); applied when the bar is built (a mission's start, the way back
      from the Esc menu); a small map's logical screen (mission 1) keeps the normal size. The
      minimap's row got a first value, as the interface (`MinimapSize` "0" or absent, the default);
      only a minimap bigger than the interface leaves a strip below the map,
  48. the messages, the objectives, the mouse tip and the cursor at the interface's size
      (`interface_texts.cpp`, engine.md "The interface's size"): the message lines in the bigger fonts
      (red, yellow and green too) with their click strips and spacing times the scale, the mouse tip in
      the bigger font beside the cursor while the bar is up, and the Windows cursors loaded as many times
      bigger with their hot spots in proportion,
  49. the settings page's preview (`menu_version.cpp`): while the page is open the bottom of the window
      shows the state bar at the interface's size the game would use there, across the window, and the
      minimap's frame at its size on it in the corner - one picture where they will stand, following a
      click on either row; it replaces the corner's minimap-only preview (`vidarpreview.bmp`, made by
      `build.py` from the bar's own pictures),
  50. the selected-slot marker off the screen (`depot_rows.cpp`, `BUGS.md` F31): the engine hides it by moving
      it 1000 px left, which leaves it on a screen wider than about 1960 px (a white frame at x 280 on a 2560
      one) when the bar was just built; after every `Player_SetPanel` a marker under no slot goes far off
      the screen,
  51. the editor's interface size (`editor_edit.cpp`, `editor_panel.cpp`, `interface_scale.cpp`, editor-ui.md
      "The interface's size in the editor"): the editor's bar and its strip, the rows above it, the status
      lines, the music player, the hints and tips, an object's panel, the Help and Menu buttons and every panel
      twice or three times as big, the map and its marks drawn as before; a row of its own on the game's
      settings page (**EDITOR INTERFACE** under **INTERFACE SIZE**, arrows VIDs 956/957,
      `EditorInterfaceScale` "1"-"3" of the Players key; absent = normal), offering what the window takes by
      the play interface's rule; the editor's own pictures (icons, buttons, the palette's icons, the panels'
      device, pictures, lamp and buttons, fonts 3 and 7) enlarged by the build as the bar's are (VIDs
      1123-1135, 1173-1185); the cursor at the editor's size while the level is held, Run the level at the
      game's,
  52. zoom with the mouse wheel in play (`zoom.cpp`, `zoom_view.cpp`, engine.md "Zoom", working.md "Zoom
      with the wheel"): away from the player in, towards the player out, 12 % a notch (up to 3 times in, out
      to the whole map), with a soft glide of 220 ms and the map point under the cursor kept, no sound; the world is drawn
      at the zoom and stretched to the window, the interface after it at the window's own pixels, so it keeps
      its size at every zoom and interface size; up to the whole view of a 3072 x 2304 map; border and arrow
      scrolling as fast on the screen at every zoom; nothing over the interface, in a menu, while paused,
      during an intro or in the editor (its own: 53); a menu keeps the zoom and looks as at zoom 1; a level change goes back
      to the level's own zoom, at which nothing of it runs. The engine stored the wheel and nothing in a
      mission read it,
  53. zoom in the map editor (`zoom.cpp`, `zoom_view.cpp`, `editor_edit.cpp`, editor-ui.md "Zoom in the
      editor"): the mouse wheel over the map through play's steps, the map point under the cursor kept, Ctrl+0
      back to 100 %, the zoom in the status line; the tools' wheel (turning, a brush's size, the ramp, the
      scatter's radius, a post's side) is Ctrl+wheel now; the map and its marks drawn at the zoom, the editor's own interface after the
      stretch at the window's pixels and its own size; picking, drags and the tools on the map as before; the
      camera's margin past the map in map pixels; Run the level at the level's own zoom and the editor's zoom
      again after it,
  54. the lights' fill (`light_fill.cpp`, engine.md "The lights"): `Graphics_DrawLight` fills a light's texture
      texel by texel (one per 4 x 4 screen pixels, up to 128 x 128) and did per texel what is the same for the
      whole light - four float-to-integer conversions of its position, eight comparisons with the view's edges -
      and a division. The loop (0x41207b-0x412314) is C++ now: the view tests once per column and once per row
      with the engine's own x87 instructions (the same rounding at any FPU precision), the position converted
      once, no division where the brightness is 0 anyway; the same texels written, the same left alone. In the
      test rig, mission 13 at 1920x1080 (35-52 lights a frame), the lights took 10.6 ms a frame against 13.2
      with the engine's loop (the two switched in one run); a held frame is the same to the pixel both ways,
  55. a texture of its own for each light (`light_textures.cpp`, engine.md "The lights"): the engine draws every
      light through one of its two scratch textures, in turn, so under dgVoodoo the next light but one locks a
      texture the GPU is still drawing, and the draw after the fill waits for the GPU and uploads it - 160 us a
      light against 1.4 us for an ordinary textured draw. The engine still fills its own texture (so it keeps
      the texels it always had, which a later light reuses where it writes nothing), then the filled rectangle
      is copied into a texture of a pool - the same size, format and grey palette - and that one is drawn with
      the same rectangle; the engine's texture is never drawn, so never uploaded. A pool texture is used once a
      frame (64 of each kind; more lights take them again from the first). In the test rig, mission 13 at
      1920x1080 (35-52 lights a frame), with 54: the lights 12.2 -> 2.2 ms a frame, `d3d` 14.1 -> 4.2 ms, the
      frame's CPU time 28.8 -> 18.7 ms (part of the wait moves to the next lock of the back buffer, which then
      waits for the GPU instead); a held frame is the same to the pixel both ways,
  56. the depth buffer's fill and the ground's heights by the CPU (`depth_blits.cpp`): the ground's tiles with
      heights (imageFlags 4 - the missions' grounds; seen on missions 1, 3, 7, 13 and 18) are put into the
      system-memory depth buffer with a DirectDraw Blt each (`Graphics_BlitToDepthBuffer`), and `Graphics_Clear`
      fills it with the empty depth 0x3ff (a depth-fill Blt) whenever the view is not all map - zoomed out to the
      whole map, a camera past an edge. Under dgVoodoo the first lock of the depth buffer after those Blts (for
      the software sprites) waits for them and copies the buffer back. The same fill and the same rectangles are
      now written by the CPU into the locked depth buffer, clipped by its clipper's rectangles as the Blt was, and
      it stays locked for the sprites as the engine's lock would leave it; anything else (a stretch, a
      rectangle outside the texture, no clip list) goes the engine's way. In the test rig, mission 13, the two
      switched in one run: at 1920x1080 the depth lock 1.69 -> 0.03 ms a frame, the ground 1.0 -> 1.6 ms (the
      copies), so about 1.1 ms less; zoomed out to the whole map (3072x2304) the lock 4.8 -> 0.1 ms, the ground
      1.0 -> 2.2 ms; missions 1, 3, 7 and 18 at 1920x1080 the lock 1.5-1.7 -> 0.03 ms. Held frames are the same
      to the pixel both ways (missions 13 at 1920x1080 and zoomed out, 1, 3, 7, 18),
  57. no layer 10 lock in a zoom view's world pass (`layer10_lock.cpp`): zoomed in or out, the world is drawn in
      one pass without the interface's objects and the interface in a second pass into an overlay of ours
      (`zoom_view.cpp`); the messages are interface objects, so in the world pass layer 10 draws nothing, but
      fix 32 saw objects in it and kept the engine's lock of the back and the depth buffer - a wait for the GPU
      and nothing else. An object the zoom's filter leaves out of the pass now counts as none. A lock in which
      nothing changes leaves the picture as it was (engine.md "Locks under dgVoodoo").
      Mission 13 zoomed out to the whole map in the test rig: that lock 5.7 -> 0 ms a frame; a held frame is
      the same to the pixel both ways,
  58. the minimap's points without a lock of the picture (`minimap.cpp`): the engine writes the rail points and
      the dots into the back buffer, locking it through the surface - the dots' lock always after the view's
      highlight (a Direct3D quad), the rail points' after the bar's drawing when no message is on the screen;
      each such lock is a wait for the GPU. The points now go into a buffer of ours (the same pixels by the
      engine's tests and its 16-bit colour), and each batch - the rail points before the highlight, the dots at
      the end - is put on the picture as dgVoodoo's unlock leaves it (engine.md "Locks under dgVoodoo"): the
      minimap's rectangle copied by the GPU through a 16-bit texture and back through a clip list of the 2 x 2
      blocks up-left of every point, then the points with a colour-keyed Blt. Round 1 tried the overlay alone and
      found 35 pixels in the view's highlight a few levels off - those blocks. A point drawn in exactly the 16-bit
      colour already under it would be no change to dgVoodoo, and here its block would get the 16-bit colour: where
      the colour under it was finer (the view's highlight), up to 4 pixels by less than one 16-bit step - not seen
      in any comparison. If a surface of ours cannot be made or a Blt fails, the batch goes the engine's way.
      Mission 13 at 1920x1080 in the test rig, switched in one run: the minimap's locks 2.0 -> 0 ms a frame,
      `other` 3.3 -> 1.8 ms; a held frame with the minimap drawn (c40054) is the same to the pixel both ways
      (screen captures, not the game's own 16-bit screenshot),
  59. the messages without a lock of the picture (`layer10_lock.cpp`): with a message line on the screen (the
      missions' objectives keep one there) `Graphics_DrawScene` locks the back buffer for layer 10 after the
      Direct3D layers 5-9 - a wait for the GPU every frame. Layer 10 holds the message lines and the scripts'
      texts: Text objects whose font is a 16-bit software sprite, written letter by letter with no blending. It
      is now drawn into memory of ours twice (filled with two different values, against the real depth buffer),
      so every pixel it draws is known exactly; only the rows the letters touched are read (the memory's writes
      are watched). The pixels go on the picture as dgVoodoo's unlock would leave them, the same way as fix 58.
      Anything else in layer 10 (another kind of object, a text that switches to another VID or reads a script
      variable when drawn, a font that is no 16-bit sprite or blends, a cursor drawn there) keeps the engine's
      lock; so does a failure of a surface or a Blt. The same exception as 58: a letter drawn in exactly the
      16-bit colour already under it, on a part Direct3D drew with finer colour after the sprites' lock, would
      leave up to 4 pixels less than one 16-bit step off - not seen in any comparison. With 57 and 58 a frame in
      play now has one lock of the back buffer, the sprites'. Mission 13 at 1920x1080 in the test rig, switched
      in one run: the back buffer's locks 7.3 -> 3.9 ms a frame (4 -> 1), `other` 3.6 -> 1.5 ms; held frames with
      the minimap drawn are the same to the pixel both ways on missions 13, 7 (fog), 1 and 3 and on mission 18
      (night) but for a white spark that moves from frame to frame even in a held level (screen captures),
  60. the messages' pixels put on from a surface of their own size (`soft_overlay.cpp`, engine.md "Locks under
      dgVoodoo"): fix 59 put the letters on the picture with a colour-keyed Blt from a system-memory surface as big
      as the screen, and dgVoodoo uploads the whole of such a surface at every Blt from it - 1.2 ms a frame at
      1920x1080 in the test rig, a wait of its own while a message line is on the screen. The pixels now go into a
      surface the size of their box (rounded up to 64 x 32; a few kept), and the Blt takes 0.02 ms; the same
      pixels, the same place. Fix 59's way of drawing and putting on is shared now (`soft_overlay.cpp`) with the
      editor's overlay (61). Mission 13 at 1920x1080 in the test rig, switched in one run (c40057): the frame's
      CPU time 17.4-18.2 -> 14.9-15.4 ms (the put 1.2 ms less, and the sprites' lock of the next frame about 1 ms
      less: it no longer waits for that upload); held frames the same to the pixel both ways,
  61. the editor's overlay without locks of the picture (`editor_edit.cpp`): its texts and software pictures - the
      bar's pieces and keys, the status lines, the rows', the panels' and the mines' texts - were drawn into the
      locked back buffer, a lock of their own after Direct3D drawing each (6 a frame on mission 13, about 5 ms in
      the test rig). Within the overlay's part of a frame such a picture is drawn twice into memory of ours, as
      fix 59 draws the messages, and put on the picture the same way at the moment the engine's code unlocked: a
      picture of its own at once, inside one of the overlay's locks at its end or before anything that unlocks the
      picture inside it (a Direct3D picture or rectangle - their unlock through the surface comes to
      `edit_soft_unlock`). Only SpriteVid_Draw's kinds that write the same pixels when drawn again are drawn so (one
      depth for the picture, no blending); a picture with a depth per pixel (a building, a rail) or one that blends
      keeps the engine's lock, inside one of the overlay's locks to its end as before. The editor on mission 13 at
      1920x1080 in the test rig, switched in one run: the back buffer's locks 7 -> 1 a frame, the frame's CPU time
      14.8-15.0 -> 10.6-10.8 ms (the bar 4.0 -> 1.4 ms, the status lines 2.2 -> 0.2 ms); held frames the same to
      the pixel both ways with the Select tool, the Menu and Help panels, a help page, the Track tool's rows and the
      Factory and Repair point tools' previews (where a part blinks or a status line changes by itself between two
      shots, the same part differs between two shots of the same way),
  62. every factory's builds in `LocoFix.log` (`factory_log.cpp`, engine.md "Factories"): `Depo_AddToQueue`
      (0x404dd0), `Depo_ReplaceInQueue` (0x404fd0) and `Depo_ReleaseUnit` (0x405290) are hooked only to log, one
      line per unit queued, refused, replaced, released or not released - `factory: army 1 at 643,1100 queued
      VID 35 (queue 1/10) at 10 s - a command`, `factory: army 0 at 319,1035 queued VID 5 (queue 1/10) at 15 s -
      the player's panel`, `factory: army 1 at 643,1100 released VID 35 (queue 0/10) at 46 s` -, so a log shows
      who built what; nothing the engine does changes. "the player's panel" is a build asked for while
      `Player_HandlePanelAction` runs (a click or a hotkey there), "a command" any other (a script's AI, the
      editor's runner, a mission). A refusal or a failed release that repeats within 5 s is logged once.
      Checked in the test rig: POLIGON's player factory with F1 twice (two lines "the player's panel", the
      release 34 s later), an editor map's enemy plan (35, 5, 35, then 90, each queued as the one before was
      released, "a command"), mission 18's enemy AI (80, 5, 80 from 1 s); the 20 missions pass,
  63. a script's variable reads 0 once its object is gone, for every object (`script_refs.cpp`, `BUGS.md` F32,
      engine.md "Values and variables"): the engine cleared the script values holding a deleted object only
      for units, regions, menu frames and objects something else still held, so a decoration or a sprite that
      only a script held stayed in its variable after it was freed - `GetX`/`GetY` read freed memory and the
      next `Destroy` crashed. After every object's destructor the engine's own clearing (`Script_ObjectDeleted`)
      now runs for it; one of the script's own variables cleared only so gets a `LocoFix.log` line with its
      name (`script: object 0x... (VID n at x,y) was deleted while variable fruit held it - it reads 0 now`,
      the first 20 objects of a run). What a script sees changes only there: such a variable reads 0 instead
      of freed memory, as a unit's always did,
  64. the title of the editor's object panel in capitals in the pack's code page (`ui_upper`, editor-ui.md
      "The texts of the editor"): the title was made capitals byte by byte with `std::toupper`, which changes
      only a-z, so a name's letters with marks stayed small - Czech "LáVOVý šTěRK (VID 294)", a Russian name in
      small Cyrillic letters; now "LÁVOVÝ ŠTĚRK (VID 294)". Checked in the test rig with the Czech, Russian,
      Polish and English packs; the 20 missions pass,
  65. the player's sound and music volumes in a game that does not start at the main menu
      (`sound_volumes.cpp`, `BUGS.md` F33, editor.md "Starting it and going back"): only the main menu's
      script sets the volumes saved in the Players key, so the editor, Run the level, a map played from the
      editor and the editor after the way back played everything at full volume until a volume was touched
      on the settings page. Before the first level of any game loads, LocoFix reads `sound` and `music`
      and sets them as the menu does (a missing value 29, one outside 0-29 14, the volume `v * 100 / 29`,
      through `SetSoundVolume` and `SetMusicVolume`); a game that starts at the menu gets the same values
      from it again, so nothing changes there. `LocoFix.log` says the volumes before every level load.
      Checked in the test rig, old and new LocoFix, sound 11 and music 20 saved (37 and 68): every editor
      path and the way back at 100 before, at 37 and 68 now; mission 1 from the main menu 37 and 68 both
      ways; the 20 missions pass.
  66. the factory's panel in play (`depot_rows.cpp`, `BUGS.md` F34, engine.md "The factory panels",
      editor-mission.md "A factory's build options"; issues #20 point 2 and #23 points 2 and 3): the list
      follows what the factory can build while it is open - a unit that became affordable or whose limit
      freed up lights up at once (its click and key work again), a unit put into the list appears at once.
      On an editor map with the factory build options a unit with a limit shows `used/limit` in its
      icon's bottom-left corner (its units on the map and in the factories' queues, as the engine counts
      them), and a unit that comes later stands after the list's units, greyed, with no key and the time
      left (`m:ss`, by the runner's own clock); the mouse tip over such an icon adds it in words
      (`[Editor] PlayFactoryLimit`, `PlayFactoryUnlock`). A list with no limit and no later unit - every
      campaign mission - looks as before. Every language pack's copy of the price's font (VID 8) gets `/`
      and `:` (`lang/<code>/fonts/font-unitnumbersonscreen.png`, build.md "The Czech letters").
  The parts of the state bar are FRAME objects (texts too): `Frame_Construct` (0x40c4d0)
  takes a screen position, adds the camera to it and puts the object on `World.menu`, whose
  objects `SetPosition` moves with the camera, so they stay on the screen. Objects that
  fixes 22 and 23 add are created at the screen position (an object's position minus the
  camera); given a map position, they came out shifted by the camera - the second factory row
  showed as a sliver at the bottom right edge of a scrolled view.
  The last row and the last column of the window showed an old picture (a line of the map
  under the bar): with `Bilinear2DOperations = true` dgVoodoo filters every Blt, the game's
  1:1 present of the back buffer to the window too, and the picture came out one pixel up
  and to the left, blended, so the window's edge row and column were never refreshed. The
  back buffer is right (read in the game: the bar fills its last row). `game/dgVoodoo.conf`
  now has the setting off, and the window matches the back buffer pixel for pixel (checked
  at 1024x768 and 1920x1080; mission 1 stretched to 1440x1080 ends exactly at its edge).
  The stretch of LocoFix's screen scaling (mission 1, the menus in a window bigger than
  1024x768) is nearest-pixel since - a filtered stretch of its own (Direct3D) would bring
  the smoothing back.
  Every fix checks the expected bytes; when something doesn't match, it skips the fix and
  writes the reason to `logs\LocoFix.log` of the game folder. The library works only with the exe
  from 2003-10-07 (date in the PE header).
