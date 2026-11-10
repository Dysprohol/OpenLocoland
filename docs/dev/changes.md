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
     in, so the game area keeps its size there too,
  3. picture and mouse are computed relative to the game area, not the whole window
     (6 GetWindowRect calls),
  4. more resolutions in the launcher: the built-in list (640x480, 800x600, 1024x768, in
     the settings struct `0x47b1f0`) gets 1280x720 ... 3840x2160 and the desktop
     resolution; the game shows those the card supports,
  5. a map smaller than the screen sits in the middle instead of at the right and bottom
     edge (`SetPosition` hooked - while the camera is clamped, the map bounds are widened
     to the view size; for a level held in editor mode by 300 px more on every side),
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
  19. test mode (only with `LOCOFIX_TEST=1`) - described below in "Working with the code",
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
      that is gone is not saved; the ground the engine makes itself does not make it copy OBJ;
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
      ticked checks as the launcher opens, a click on it saves it,
  34. the neutrals and the pirates of an editor map (`factions.cpp`, MAP_RULES.md 6d): on a map
      whose script has `#define FACTIONS 1` (the editor writes it, read from `<map>.lgc` in the
      `Level_Load` wrapper of `frame_limit.cpp`) the army test of `Object_FindTarget` (0x449f7b ...
      0x449ff3) jumps to `may_target`, and the two `ACT_DAMAGE` calls of an explosion
      (`Object_Action` 15, 0x447e9c and 0x447eb0) first call `note_hit`: a neutral hit by an army,
      and every armed neutral that sees it, answer that army - and only it - to the end of the level,
      and that army may fire at them; the pirates may fire at the neutrals. On any other map
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
