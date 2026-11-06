# Editor mode: the mode, menu, saving, starting

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

## Editor mode

The authors edited their maps with a separate program (`MapEdit.exe`, not shipped; `export.lgc`
mentions it at `SetGamma` 140, `CURSOR_DELETE` and `ACT_UNDO_REMOVE`/`INSERT` 132/133), and the
game exe kept the engine's side of it: the editor mode, World flags 0x1, which `World_Construct`
copies from bit 0 of `g_settings`+0x220 (0x47b410) - 0 in the exe, and nothing sets it. LocoFix
sets it with `--editor` (`src/LocoFix/editor.cpp`): `Locoland.exe --editor --test-map
maps\l01\l01.map` in the game folder opens a map that way (`edit_map.cmd [map]` of a
`build.py --dev` build too, the sandbox by default), and `--editor` alone starts with the intro and the main menu, where
a mission picked on the strategic map opens in the editor. In the game, "EDYTOR MAP" on the
custom maps page of the menu starts it (see "Starting it and going back" below). The switch is taken out of the
process command line before `WinMain` passes it on (`World_Construct` would open a command line
without ".cfg" in it as the map). The mode is fixed for the run: decals are converted while
`objects.res` loads.

What the mode changes in the engine:

| Where | Effect |
|---|---|
| `World_Construct` 0x41a658 | the window class gets the menu `AppMenu`: File (Load, Save, About, Exit) and Options |
| `Vid_Read` 0x44e35d | decals (spriteClass 8, 61 VIDs: grass, bushes, flowers, reeds, stones, paving, the gravel bed of the tracks, road posts, wreckage) are loaded as class 0, ordinary objects drawn as sprites - so they can be moved and are saved with the map. Outside editor mode a decal is painted into the ground (`GroundVid_BakeObject`) and removed as it is created |
| `Object_Construct` 0x445b0b | no `World_SetGroundZ` for VIDs with flag 8 - seven decals: the gun and unit wreckage 96 / 98, the curb stones 817 / 853, the desert fence 849, the road posts 869 / 871 (outside editor mode the cells under them go up to their top, z + sizeZ): the ground is not raised under them, GRID stays as the map file has it |
| `Level_Load`, `World_CreateObject` | no `ANI_BIRTH` handlers of the loaded objects, no BIRTH script of new ones |
| `Minimap_Draw` | the terrain objects are drawn on the minimap too |
| `Graphics_Construct` | Graphics flag 0x400: in full screen the frame is presented with Blt like in a window, not flipped, so a menu bar would stay visible (no effect in a window; the editor has no window menu, "No window menu" below) |

The other bits of the same byte: 1 is VSYNC, 2 shows the technical boxes of the launcher
(Full-Screen, "Do'nt show this dialog at startup", Triple video buffer, Use Palette, Low detail,
Save demo, Sound High Quality - `Launcher_DialogProc` hides them otherwise), set from the .cfg's
`StartDialogIsFull` (`World_Construct`); they are developer options, not part of the editor, and
LocoFix leaves them alone.

**The level is held.** The rest of the game runs as usual in that mode - the mission script
(its `main` brings reinforcements, sets the weather, opens the interface), the AI, the trains,
the red semaphores taking their first command - and a save would write that running state
(tried: 139 puffs of steam and smoke, the reinforcement trains and the semaphores without their
command in the saved sandbox). So LocoFix holds every map in the current format (with `GRPH` -
the logo and the menu maps run as usual) as soon as it is loaded: paused as script `Pause(1)`
does (`World_Pause`, no object updates) and with the script events off (World flags 0x20000,
not even `main` runs). The map shows as authored, without the interface; the camera moves with
the screen edge, the arrow keys (the scroll type the script would set is set) and the middle
mouse button. Options > Run the level runs it from its saved file and holds it again (below).

**The menu.** The exe's window procedure handles only Exit (40004) and the screenshot key
(Shift+F9, 40023); Load, Save and About lost their handlers (comdlg32 is not even imported) and
Options (0xFFFF) never had a popup. In editor mode LocoFix handles them in a window procedure in
front of the game's:

- File > Load... (Alt+F3): a Windows open dialog in `maps\` with no name proposed (a map of the
  scratch folders `maps\_new*\` and `maps\_run\` is refused with "This is the editor's scratch
  folder - choose a map in another folder.", `ScratchFolder`); the map is
  then loaded the way script `Load()` does it (`World.nextLevelFile` and flags 0x40, loaded by
  `Game_Step`) and held,
- File > Save... (Alt+F2): a save dialog, then `Level_Save` with the name as a String of the
  game's heap (a new folder of the name is made), then the map's mission: its script, texts and
  its line on the custom maps page ("Mission settings and push lines" below); the status line says
  "saved as ...",
- Ctrl+S: the same save into the map's own file without the dialog - the file it was loaded from or
  last saved as -, and with the dialog when it has none: a campaign map, a map of the release or the
  editor's new map; a Yes to "Save the changes?" saves the same way. The maps of the release,
  POLIGON (`maps\sandbox\`) and the template, are written again by every Setup update, so edits
  saved into them were lost (a user's track added to POLIGON went with the next update): the editor
  treats them as the campaign's maps (`release_map`, part of `game_folder_map`) - the save dialog
  opens in `maps\` with the first free `poligon<n>.map` (POLIGON) or `map<n>.map` and says why in
  its title and the status line ("POLIGON is a map of the release - save it under a name of its
  own, an update would overwrite it"), a release map chosen in it is not saved over (the same text
  in a box), Run the level runs them from a copy (below) and an unchanged one from its own file, and
  Map size, Export and Import leave them alone.
  Checked in the game (Polish and English): POLIGON painted, Ctrl+R refused, Ctrl+S opening the
  dialog titled with the reason and `poligon1.map`, saved as `maps\poligon1\poligon1.map` (its
  ground, script and texts; on the custom maps page), then Ctrl+S there saving without the dialog
  and Ctrl+R running it; the template loaded, Ctrl+S proposing `map1.map` with its reason,
- File > New...: a new map in its own folder, on a campaign map's terrain or a meadow (below),
- File > Map size...: the map's size (below),
- File > Mission settings...: the mission's settings and texts in a dialog (below),
- Rename or delete (the Menu panel's RENAME OR DELETE, id 40018): the map's folder renamed, or moved
  to the Recycle Bin ("Map files" in the data section),
- File > Play the map: the game starts again in normal mode on the current map (the one last
  loaded or saved), to play what was edited, and comes back to the editor when the map is left
  for the main menu; File > Back to the menu: the same to the main menu (both below),
- File > About (Alt+F10) and File > Exit (Alt+F4); with edits not saved, Load, Play, Back, Exit
  and closing the window ask whether to save them first,
- Edit: Undo (Ctrl+Z), Redo (Ctrl+Y), Delete (Del), Turn right (R), Turn left (E), Next army
  (A), Semaphore red/green (T) - the commands of "Editing objects" below (ids 40030-40036),
- Options > Run the level (a check item; LocoFix's own, id 40010).

The menu of the exe says F3, F2 and F1, but its accelerators (`AppAccel`, loaded in every mode)
are Alt+F3, Alt+F2 and Alt+F10 - the plain function keys are the factory build list - so LocoFix
shows those. A dialog runs with the game in GDI mode (`Graphics_EnterGdiMode`, as the menu loop:
drawing stops, `FlipToGDISurface` in full screen) and the Windows cursor shown (the game hides
it for its own; it is also shown over the menu bar and the frame), the game's current
directory is kept, and the menu commands wait while a dialog is open; the sound is fed meanwhile
(`editor_feed_sound`: the game loop's own per-frame update, `SoundBuffer_Process(g_sound)` 0x444950 -
the stream's feed, its restart at the end, the fades -, called every 30 ms from a timer of the thread,
whose `WM_TIMER` every modal loop dispatches; `DialogScope`, `ui_message_box`, the menu loop and
`level_save.cpp`'s boxes turn it on and off; before, a dialog left the DirectSound buffer with nothing
new in it, looping its last fragment - with the music player on, Mission settings opened to a stutter;
the count fed and the stream's state go to `LocoFix.log` when the loop ends). Paths inside the game
folder are passed as the game writes them (`maps\...`). Outside editor mode the accelerators do
nothing, as before.

**Saving.** `Level_Save` writes the level as it is in memory: OBJ copied from the map file it
was loaded from (the level's own VIDs: the ground), GRPH, HEAD with the current camera, GRID,
SPR and SPRD of every object (its id is its address in this session; turrets, smoke and other
objects linked to a parent are left to the parent, menu objects are skipped), PLAY and GROU.
LocoFix makes it safe (`level_save.cpp`, `BUGS.md` F16-F20): the file is written as
`<target>.saving` and replaces the target only when that worked, so the loaded map is never
renamed or truncated and a failed save keeps the old file. Checked: the sandbox and mission 1
(212 objects, 69 of them decals) saved in editor mode, the saved sandbox loaded and saved over
itself, also under another spelling of its name, and to a new name - every object comes back
with its place, direction, army, commands and unit state, GRID, GROU and the OBJ record are
the same and the files have the size of the originals; HEAD differs only in the camera (the
view at the save) and the time (0). The saved sandbox plays from `--test-map` like the
original. Outside editor mode a save loses the decals and holds the raised GRID and the
running state (`BUGS.md`, Quirks).

**Starting it and going back** (`src/LocoFix/restart.cpp`). The mode is fixed for a run, so the
menu starts the editor by starting the game again: "EDYTOR MAP" on the custom maps page calls
`StartEditor()` (`game/maps/menu.lgc.patch`), which calls `StartEditorMode("maps\\template\\
template.map")` - a script function of LocoFix's, declared there as `extern
StartEditorMode(string map) 200;` (200 is free in `export.lgc`; `Script_CallExtern` is hooked
for it and pops the name, pushing nothing). The game starts `Locoland.exe --editor --test-map
<map> --no-launcher --after <pid>` and closes itself (`WM_CLOSE`, a normal exit that saves its
settings and the window position). `--after` makes the new game wait (at most 30 s) for the old
one to end before anything else; `--no-launcher` skips the launcher, whose settings the game has
already read from the registry by then (the cfg's `StartDialog=0` would do the same) - so the
editor opens without a click. File > Play the map and Back to the menu start the game the same
way without `--editor`, with the current map or none (the intro is off, so the main menu comes
up). Play the map adds `--return-editor`: when the played map is left for the main menu (the end
screen of a win or a loss, or "exit to the main menu" of the Esc menu - the next level loaded is
`maps\menu.map`), the game starts again in editor mode on that map (`restart_level_loading`,
from the `Level_Load` wrapper of `frame_limit.cpp`); the Esc menu's restart of the map does not
count. In editor mode already, `StartEditorMode` just loads the map. Without LocoFix the extern
is unknown ("Unknown extern" in the log) and the entry does nothing. Checked through the menu
(player screen, the arrow to the custom page, a click on "EDYTOR MAP" -> the template map held
in the editor; File > Back to the menu -> the main menu) and from a script (a mission calling
`StartEditorMode` -> the editor on the sandbox; edits, File > Save, File > Play the map -> the
saved map playing with its interface).

**Back to the editor** (`restart.cpp`, `game/maps/script/menutact.lgc.patch`). A map played from
the editor (Play the map: `--return-editor`) also has a way straight back: the Esc menu's exit page
(ABORT GAME, `SetMenuState(5)`) gets a row like the custom maps page's - the lamp VID 472 and BACK TO
THE EDITOR (`[strings]` `MenuBackToEditor`: WROC DO EDYTORA, ZURUCK ZUM EDITOR ...) - under the
question, next to YES, EXIT TO MAIN MENU; a click on the row (the lamp or the text, tested by the
screen point on the click's frame, as the custom page does) starts the game again in editor mode on
the map. The script asks LocoFix two new externs (free in `export.lgc`, handled by the hook of
`Script_CallExtern`): `PlayedFromEditor() 202` (1 in such a map, so a mission played from the menu
shows no row) and `BackToEditor() 203`. Tried first the way back as it was - a map played from the
editor, won (the template's `MAP_WIN_TEST`, VICTORY, CONTINUE), lost, and left through Esc - ABORT
GAME - YES: each came back to the editor on the same map, held, nothing of the map lost (the map file
is the one played; the game writes the map's `hiscores` and `hiscores.txt` next to it, which export
leaves out). What was wrong: the way back first loads the main menu (`menu.map`; the restart starts at
its `Level_Load`) and the menu never starts, so the menu's `return` flag of the Players key, which
every custom map's `InitDefault` sets to 2 (the menu opens on the custom maps page after it) and the
menu sets back to 1 when it starts, stayed 2 - the next start of the game opened on the custom maps
page. Now the value it had before the map was played (read when the game starts with
`--return-editor`, before the map's script runs) is put back on the way to the editor, by either way.
Checked in English and Polish: the row on the exit page of a map played from the editor and not on a
mission played normally, a click on it -> the editor on the same map, `return` 1 as before; YES, EXIT
TO MAIN MENU -> the editor, `return` 1 too.

A tester's way into a running map: Run the level (Ctrl+R) runs the level inside the editor's own game,
with the game's interface and its Esc menu. The script is compiled when a level loads, so resuming the
held level would run the script of the load - without what the panels changed since (an arrival set in
the Arrivals panel never came). So Run the level (the Menu panel's QUICK TEST, NO SAVE / TEST BEZ ZAPISU) writes the
map and loads it again running, never asking: a map of its own is written as a save writes it; a map
not written in place - a campaign map, POLIGON or the template, and the start's new map without a
name - is written with its script (its editor blocks anew; a script without them as it is), ground
and texts into a hidden copy, `maps\_run\` (marked as `maps\_new\` is, written for every run, removed
when the editor leaves the map and swept at the next start), which runs. Back, the copy is held again
with the editor on the original map (Ctrl+S, the dialogs' names, "not saved" and the history as
before the run; a close of the game during the run asks to save, and Yes saves from the copy held
again); while a level runs, the menu's other commands are not taken. Checked in Polish and English:
POLIGON with a semaphore placed, Ctrl+R at once, the semaphore in the running level, Ctrl+R and the
Esc menu's way back - the semaphore there, "not saved", Ctrl+Z undoing it; a second run from the
copy; Ctrl+S then asking for a name; the start map and mission 1 run the same way; a map of its own
written in place; `maps\_run` gone after the game is closed. File > Play the map (PLAY THE SAVED MAP /
ZAGRAJ ZAPISANA MAPE) needs the saved map for the separate game and asks to save first; in the Menu
panel the status line says what each of the two does (`TipRun`, `TipPlay`). The run
(`run_level`: `g_running` on, `editor_level_loaded` does not hold it). Back - Ctrl+R, BACK TO
THE EDITOR in the Esc menu, or any level change while it runs: ABORT GAME's menu, a won or lost
level's next map (`editor_level_redirect` in the Level_Load wrapper of `frame_limit.cpp` turns the
name into the map's, in a string of the game's heap, which the load frees) - stops the music
(`StopMusic(g_sound)`, 0x444e50, as script function 134, and the music file's name at g_sound
+0x3d8 made empty: the sound's update, every frame, starts a stopped stream again from that name, and
the victory's music, `music\mus00.ogg`, came back in the held editor a few seconds after a won run;
checked by the stream's state - its playing slot, the name - for 30 s after a won and a lost run
and after Play the map's way back, where the editor's new game has no music at all) and loads the saved map again, held: the
state of the run does not stay, the editor's bar comes back as before (the state bar's icons, VID
455, and the editor's, 939, get the menu gammas the editor gives them again), the view and
the chosen object (found again by VID and place) and the tool as they were, and the undo history: before
the run the editor keeps it with every object of the level in the world's order and the rail points
its cars stand on (their places and their links' other points); after the load held again
(`edit_restore_history`) the new objects are matched by that order, VID and place (the save keeps the
order), a deleted one the history can bring back is made again and hidden, and the history's objects,
the cars' couplings and rail positions and the later units' factories point at the new ones. RESTART of the same map keeps it running.

**Hints under the cursor** (`editor_edit.cpp`, "the hint under the cursor"). The status line explains
the tool; a hint is a few words beside the cursor - the keys of the tool on the map, or of the bar's
entry under the cursor (`hint_of`: `HintSelect` ... `HintRegion`, the Paint tool's gravel bed mode
`HintBed`) - in the mouse tips' font, 14 px right of and 22 below the cursor (above it over the bar,
kept inside the view), shown when the cursor has rested 350 ms (700 until 2026-11-06, which a tester
found too long) and gone with its next move - any move starts the rest anew, so none shows while the
cursor goes across a row; never while a button is held, a stroke or a drag is under way, a panel is
open or the cursor is on the rows. The strip's last two icons of its upper row, Train arrivals and
Mission settings (panels, not tools), had none - the tester's "two icons at the top right" -: now
`HintArrivals` and `HintSettings`, so every entry of the bar has one. The other buttons that are not
tools have theirs too since then: the music player's name, |<, || / > and >|
(`HintMusicName`, `...Previous`, `...Stop` / `...Play`, `...Next`) and the bar's Help and Menu buttons
(`HintHelpButton`, `HintMenuButton`; `panel_on_help` / `panel_on_menu`). The rows' buttons are their own
words and the strip's icons have their name above them at once (`draw_row_tip`), so they have none.
Checked in Polish: each of the four parts of the player, the two buttons, Train arrivals and Mission
settings, the hint after a rest (the strip's empty slot, no button, has none). The Menu panel's line in the strip above its title, HINTS UNDER THE
CURSOR: ON / OFF (`PanelHintsOn` / `Off`, its tip `TipHints`), turns them off and on; `EditorHints`
(DWORD, next to `FrameLimit`, on when missing) keeps it. Checked in Polish and English: the Track
tool's hint after a rest, the Decals entry's above the bar, the Paint tool's at the view's right edge
pulled inside it, the Menu line turning them off - no hint after a rest then.

**Music while editing** (`editor_edit.cpp`, "the editor's music"). A held level plays nothing (its script
does not run, and the way back from a run stops the music, above), so the editor has a small player in the
top right corner of the view, in the status line's font: the file's name, `|<`, `>` (`||` while it plays)
and `>|` - the files of the game's `music\` folder but the menu's (`mission_music_files`, the list of
Mission settings' drop-downs), the one under the mouse underlined and said in the status line; it stands
on the first line of the top whose status text does not reach it (`g_status_ends`), and its box is part of
`on_options`, so nothing under it is painted, picked or placed, also with a panel open. The engine's
`PlayMusicFile(g_sound, file, loop)` (0x445090, script function 136) only names the file (+0x3d8) and the
sound's update plays it and starts it again when it ends, so a file plays on until another is chosen;
`||` is `stop_music` (StopMusic and the name taken away), and the file starts from its beginning when
played again. Off by default; `EditorMusic` (DWORD) and `EditorMusicFile` (the name) next to `FrameLimit`
keep the choice, so the editor comes back silent unless it was playing. A dialog does not stop it (the
sound is fed while one is open, "The menu" above). Only the held level ticks it
(`music_tick` from the overlay): Run the level loads the map running and its script's
`PlayMusicFile(music0)` names the level's file, Play the map is a game of its own, and the way back
stops the run's music as before - then the player names its file again. Checked (Polish, English, the
files opened in `LocoFix.log` of test mode): `>` played mus01, `>|` mus02; Ctrl+R opened POLIGON's own
mus08, Ctrl+R back the player's mus02 again; `||` stopped it; turned on and the game closed, the next
start of the editor played mus02 at once.

**The map the editor starts on** (`editor.cpp`, "the map the editor starts on"; `editor_newmap.cpp`
`make_start_map`). Started on the template - `StartEditorMode("maps\\template\\template.map")` from
the menu, or `--editor --test-map maps\template\template.map` as the desktop shortcut passes
it (`test_map.cpp` asks `editor_start_map` before the first `Level_Load`) - the editor opens a new
empty map instead, at once and without a question: what File > New makes with "an empty map: Grass"
and 2048 x 1536 (the template's script and texts), a map with no name yet, titled NEW MAP (the pack's
`NewMapTitle`); the view starts at its top-left corner. The engine loads a level only from its files
(`Level_Load` reads the map, the script is compiled from its file, the ground from its image), so
the map is held in a hidden scratch folder, `maps\_new\` (`_new2` ... while an older one is still in
use), marked with `editor_new.txt`; it is never put on the custom maps page. The first save - File >
Save, Ctrl+S, a Yes to "Save the changes?" (of Load, New, Play, Back, Exit), Export, Map size - asks
for the name in the save dialog, which opens in `maps\` with the first free `mapN` offered; a name
typed straight into `maps\` becomes `maps\<name>\<name>.map` (a map already there is overwritten
only after a question), the map's own folder with its ground image (`<name>.vid`), script and texts,
and so on the custom maps page; a title or briefing header NEW MAP becomes the folder's name in
capitals, as TEMPLATE does, and POLIGON's own "POLIGON" when POLIGON is saved under a name (checked:
POLIGON and the template loaded and saved as `pp1` / `tt1` - PP1 and TT1 in the script's title,
`BRIEF_HEADER` and `custom.lst`; POLIGON kept its title before). A map is not saved outside the game folder ("A map is saved in the maps
folder of this game."): its script and texts are found by game paths, and not into a scratch
folder (refused as Load refuses it). The file dialogs are `IFileOpenDialog` / `IFileSaveDialog`
with `SetFolder`: the old `GetOpenFileNameW` opened in the folder it remembers for the program -
every `Locoland.exe`, another game folder's too - and not in `lpstrInitialDir`, so a save went into
another installation of the game. Run the level runs it from a copy in
`maps\_run\` (above), so the map stays unnamed, its edits not saved, and leaving it asks. The scratch
folder goes when the editor leaves the map - another map loaded, made or imported, Back to the menu,
Exit, the window closed ("the new map maps\_new left unsaved - removed" in `LocoFix.log`). Play the map
and Export ask for the name first, as a save does, so the played game never runs from the scratch
folder (the next start's sweep could remove it under that game) and a package never holds it; a marked
folder left behind (the game was ended) is removed at the next start of the editor, but the folder of
the map it opens. The template stays as it was: it can be loaded, and every new map takes its script and texts
from it. Checked in the game
(in a window, `--test-map` on the template): `maps\_new\_new.map` made, hidden, nothing added to
`custom.lst`; a hill raised, a fort placed, Ctrl+S -> the Save map dialog in the game's own `maps\`
with `map1.map` offered, "edtest2" typed -> `maps\edtest2\edtest2.map` with its `.vid`, script and
texts, "edtest2 / EDTEST2" on the custom maps page, the script titled EDTEST2; a second Ctrl+S saved
without a dialog; the game closed -> `maps\_new` removed.

**A level change resets the editor** (`editor_level_loading` -> `edit_reset`, `editor_level_loaded`).
Every level that loads - File > Load, New, Import, Map size, StartEditorMode, a mission picked in the
game's menus, Run the level's return - first clears what the editor kept of the old one: the history,
the selection, a drag, a line of track or a push line under way, a stroke of the ground's tools and
its repeat timer, the picture's buffers and painted textures (`ground_reset`), the panels and their
picks (`panel_reset`, `arrivals_reset`, `factory_reset`), the trains' orders, the status line's texts
and its message, the hit boxes of the last frame and the heights' view (H); the tool goes back to
Select. The mission's settings, push lines and arrivals are read again from the new map's script
(`mission_load`), and the map Ctrl+S saves into becomes the level loaded (before, a level loaded
another way than File > Load kept the old map's path, so Ctrl+S would have saved into it). The
brushes' settings and the palette (read from `objects.res`) stay. Checked in the game: a map with
track, a hill (saved), the heights' view on and an unsaved stroke, then the sandbox loaded - the
Select tool, no heights' view, "nothing to undo" - and the map again: its saved track and hill, no
unsaved stroke, nothing to undo; the sandbox painted (unsaved), another map loaded and the sandbox
again: its picture as it was.
