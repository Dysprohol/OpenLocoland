# Editor mode: map exchange, texts, Help/Menu, profiler

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

**Map exchange** (`src/LocoFix/editor_exchange.cpp`, `editor.cpp`; MAP_RULES.md 11). File > Export
map (and the Menu panel's EXPORT MAP) packs the current map of the custom maps page into an ordinary
`.zip` - `<folder>/<file>` for the map, its own ground image when it has one, the script, the texts
and the briefing pages; not the high scores, not a stale `.vid` - written by our own deflate (LZ77
over 32 KB, fixed Huffman codes; a painted 1536 x 1024 ground of 6 MB packs into 1.8 MB, a map on a
campaign terrain into a few KB, since its ground stays the player's own `maps\lNN\lNN.vid`). The save
dialog starts on the Desktop; unsaved edits are offered to be saved first, and a campaign map or the
template is not exported. File > Import map (IMPORT MAP) reads a `.zip` - ours or one made again by
Windows or any tool: stored and deflate entries, fixed and dynamic Huffman blocks, CRC checked -
takes the folder that holds its `.map`, only the files directly in it and by their names alone (so
nothing is written outside `maps\<folder>\`), and checks that a map with a ground of its own carries
it. A folder of that name on the custom maps page is overwritten when the player answers Yes
(No: the map comes as `<folder>_2` ...; Cancel: nothing); a game folder (a campaign map, the
template, the scripts) is never overwritten. A renamed map gets its files, the paths of its script
and lists and its ground record renamed, and a title that was the old name follows the new one. The
folder keeps the package's name when the rules of "Map files" allow it (before: small letters, digits
and `_` only, so `Point` became `point`); the page lists it as it does a folder copied in by hand, and
it is offered to be opened in the editor (after the
question about unsaved edits). The question about unsaved edits comes before every way the level
goes - Load, New, Play the map, Back to the menu, Exit, closing the window, opening an imported map -
from the window's menu and the Menu panel alike. Test mode: `E<zip>` and `I<zip>|overwrite|rename|ask`
steps of `test_game.ps1`; `LOCOFIX_TEST_ASK=1` lets a test see the questions. Checked in the game in
Polish and English: world4 (a painted ground) exported, its folder deleted, imported back - on the
WLASNE MAPY page, started from there and played with its picture; rampswamp (mission 6's terrain)
exported (6 KB) and imported over the existing one: the question shown in both languages, No - it
came as rampswamp_2 (RAMPSWAMP 2), its script's paths renamed, played on the player's `l06.vid`; the
packages read by Python's `zipfile` (testzip clean); the question about unsaved edits on BACK TO THE
MENU of the Menu panel, Cancel keeping the editor.

**The texts of the editor** (`src/LocoFix/editor_text.h`, `editor_text.cpp`). Every text the editor
shows - its menus, the file dialogs and message boxes, File > New, File > Map size and File > Mission
settings, the bar's tools, the status line and its messages, the panels below - has a key in the
section `[Editor]` of `strings.ini`: `ui_text("MenuLoad", "&Load...\tAlt+F3")` returns the chosen
language pack's text or, when the pack has none, the English one given in the code;
`ui_format` is the same for a printf format and uses the translation only when it has the English
text's conversions (`%d`, `%s`, `%.1f` ...) in the same order, otherwise the English text and a
line in `LocoFix.log`; `ui_wtext` gives UTF-16 for the W dialogs, converted from the pack's code
page (`language.ini`), and the menus, file dialogs and message boxes are the W functions too
(`ui_modify_menu`, `ui_message_box` ...), so every pack's letters show whatever the system's code
page; `UiText`
keeps a key with its English text for a reason found now and shown later (why a factory cannot go
there, why a rail does not fit). In a value `\n` is a new line and `\t` a tab, and a value
starting or ending with a space is quoted. The lookup calls the game's own import of
`GetPrivateProfileStringA`, which `language.cpp` points at `lang\<code>\strings.ini`, with the
game folder's `strings.ini` (the String at 0x4a9d34 that the game uses is not a plain pointer
there - read through it the lookups found nothing), and keeps each text (the language does not
change in a run); a key not found is logged once. The status line's messages are made twice: in
the pack's language for the screen and in English for `LocoFix.log` (`ui_english`), so tests read
the same log lines in any language; the test commands (`WM_COPYDATA`) run in English as well. The
670 keys are in `[Editor]` of `lang/en/game/strings.override.ini` (the English texts),
`lang/pl/game/strings.override.ini` (Polish without diacritics, the Polish game's names: Fabryka, Punkt
naprawczy, Fortyfikacja, Wieza rakietowa) and the German, Hungarian and Russian ones (with the
names of each pack's own `strings.ini`: Fabrik / Gyáregység / Депо, Werkstatt / Szervízdepó /
Ремонтная мастерская; Bergwerk / Bánya / Рудник for VID 165, which the games do not name), grouped by where they are used; `patch_text.py merge` adds a
section the original `strings.ini` does not have at its end. `LOCOFIX_TEXT_TEST=1` logs a few
lookups at the start ("text test" in `LocoFix.log`). Checked in the game with the pl and en packs:
the menus (Plik, Edycja, Opcje), the status line, the file dialog's title, the panels; and in all
five packs (pl, en, de, hu, ru, on a Polish Windows): the custom maps page and POLIGON's briefing
pages, the bar and the status line, Help and the Menu panel, File > Mission settings (POLIGON's
texts of the pack) and the Load dialog - the Russian dialogs in Cyrillic -, and File > New, whose
new map got the template's texts in the pack's language.

**The Help and Menu buttons** (`src/LocoFix/editor_panel.cpp`). The bar's two round sockets at its
left end - in play the pause button (VID 498, 25 px, at x = the bar's middle - 274, y = the view's
bottom - 19: `Player_ShowStateBar`) and "up to the menu" (VID 499, 38 px, at the middle - 236, the
bottom - 24) - hold the editor's Help and Menu buttons: VIDs 942 and 943 (`game/objects.override`),
whose images `vid\942help.vid` and `vid\943menu.vid` `build.py` makes from the two buttons
(`patch_data.make_editor_buttons`): the same discs with their sign taken away (filled from the
disc itself, turned) and "?" or three bars drawn in its colours, frame 0 dark and 1 lit like the
models - lit under the mouse and while their panel is open, the status line naming them. A panel
looks like the game's own menu pages and is drawn with their pictures in front of the held level
(the scripts are off there): the in-game menu device (VID 466) in the middle of the view, in it
the picture with the button bar (VID 486 for Help, 484 for Menu) where the menu script puts its
pictures (22 px right of the device's middle, 18 up), the title in the menu titles' font (VID 3),
the text in the menu font (VID 2), the page arrows (VIDs 475/476) and the start button (VID 477)
with "back to the map". **Help** (the ? button or F1) shows the editor's help pages the way the
game shows its own (`maps\menu4.lst` -> `menu41.txt` ...): `maps\edithelp.lst` lists
`maps\edithelp1.txt` ... `edithelp23.txt` - in the order a new user needs them: what the editor
is (the bar, the status line, the two buttons), the bar's tools, selecting and placing (the mouse,
undo), the keys, laying track, trains, the object's panel, all objects and decals, the mission,
saving and playing, the ground (height, paint, water, regions, map size), the ground tools' rows,
then the train arrivals and the orders (13, 14) turning (15) and the citadel, Charybdis, fly nests and mines (16; page 1 names 13-16), a factory's plan (17), the crusher, Scylla and the decorations' sets (18), the gravel bed (19), the neutrals and the pirates (20), the map files (21: a map is its folder, the
page finds it, Rename or delete, the names), placing - the track as a help, overlaps, flags (22), the rails' lattice (23), the placing helps (30: the grid, onto the track, the height, the turn, scattering), copy and paste and the ground under rising rails (31) - in the format of those pages (CP1250, CRLF, the first line the page's title, at most
44 characters a line - the ESC pairs not counted, so a line with lit words is longer in the file - and 14
lines, ESC 7 / ESC 2 switch to the lit font VID 7 and back to VID 2):
English in `game/maps/` (the fallback of every pack) and the others in `lang/<code>/game/editor/`; the files are
opened through the game's own `CreateFileA` import, so the chosen pack's copies are read. Help opens on
its **contents**, built from the pages' first lines and the categories of `edithelp.lst`: a line of the
list is the page's file, the `[Editor]` key of its category and the category's English name,
tab-separated (`maps\edithelp5.txt<TAB>HelpCatTrack<TAB>TRACK AND TRAINS`); the categories come in
the order of their first pages, a page keeps its number (its line), and a line with the file alone is
in the category of the page before - so a new page is one line of the list, with a category of its
own (a new `HelpCat...` key in the six packs) or an existing one. Six now: BASICS (1-4, 15), TRACK AND
TRAINS (5, 6, 23), OBJECTS (7, 8, 16, 18, 20, 22, 30, 31), MISSION, SAVING (9, 10, 21), GROUND (11, 12, 19), ARRIVALS, FACTORIES (13,
14, 17). The 29 rows of categories and titles (up to 37 characters) do not fit a page's 14 lines, so
the contents are laid out as the palette: the categories down the left (two lines at most, the chosen
one lit and underlined; the mouse over one, Up / Down or a click chooses it), the chosen one's pages on
the right as the Menu panel's rows - the lamp (VID 472, pulsing under the mouse) and the page's title
in two lines at most, lit under the mouse; a click or Enter opens a page. A page has its category for
the title (VID 3, so a category's name is at most 19 characters) and in the strip above it CONTENTS
with its lamp and the page's number (`HelpPageNumber`, "5 / 17"); CONTENTS, Esc or the right button go
back to the contents, at that page's category; the arrows, Left / Right, Page Up / Down and the wheel
turn the pages with the contents before page 1 (LocoFix.log: "panel: help page 5", "panel: help
contents"). `check_lang.py` checks the `HelpCat...` keys against the title font as well. **Menu**
has a row per command - the lamp of the custom maps page (VID 472, pulsing under the mouse) and the
text, lit under the mouse - New map, Load, Save, Export, Import, Mission settings, Map size (two
columns of seven), Play the map, Undo, Redo, Run the level, Back to the menu and Exit the game; a click closes the panel and posts the
window menu's command (`WM_COMMAND` with `ID_NEW` ... of `editor.cpp`), so it does exactly what
File, Edit and Options do and the panel adds no logic of its own - the way to reach every command
when the Windows menu bar does not open. A panel is modal: Esc, the right button or "back to the
map" close it; Left / Right, Page Up / Down, the wheel and the arrows turn the help's pages. Esc
with no panel open opens Menu only when nothing is under way in the editor (`edit_escape`): it first
closes an object's or a line's panel, drops a train's point being picked, ends a line of track being
laid or a push line whose begin is picked, or leaves a tool for Select - before, Esc always opened
Menu and none of these could be done with it.
**Ctrl+R** runs the level or holds it again (Options > Run the level): the bar and its buttons are
not there while the level runs. **Every save** (`save_map` in `editor.cpp`: Ctrl+S, Save map, a Yes to
"Save the changes?", the write of Run the level and Play the map) shows SAVED (`SavedNotice`, in the
menu titles' font VID 3) in the middle of the view, the map's file under it in the menu font when it
was saved under a name (not the scratch folder of a new map), on a dark box whose last 0.4 s fade -
the box over the held level only, as a running level's lighting turns the untextured quad light -,
for 1.5 s from the first frame drawn after the save (`panel_draw_saved` from `DrawScene_editor`, so
Run the level shows it once the level is loaded and running), in front of everything and taking no
input; the status line's "saved as ..." stays. The menus' click sounds with it: `PlaySFX(g_sound, 24,
0, 0)` (0x4447e0, thiscall; `menu.lgc` plays SFX 24 for its clicks). Play the map starts the game
again right after its save, so only the click is heard there. Checked in the game in Polish and English: the bar with both
buttons, the six help pages and back, the Menu panel; from it Save (the dialog titled "Zapisz mape"
/ "Save map"; saved into a new folder: the map, its script and texts written and its line on the
custom maps page), Undo, Run the level and Ctrl+R back (the log: "the level runs", "is held").

Not there yet: moving train cars, placing new cars (only the reinforcement point), ramps (rail frame
groups 1 and 2), painting the ground's picture itself, crossings of new track with old
(`RailMap_FindCrossings` runs when a level loads, so a crossing laid in the editor counts after
the map is loaded again), more object types in the bar, picking patrol points with the mouse (the
dialog takes their coordinates).

**No window menu.** The editor leaves the exe's menu (AppMenu) out of the window class
(`RegisterClassA_editor` in `editor.cpp`), so its window has no menu bar: the Menu panel of the bar has every
command, the panel posts the same `WM_COMMAND` ids, and the game's accelerators (Alt+F2, Alt+F3,
Alt+F10) stay. Testers found the bar both broken and in the way: in a window bigger than the map it
did not open (fix 6 below), and in full screen it pushed the game area down by its height while the
picture stayed at the top, so every click landed below the cursor. The commands and menu sections
above still name the File / Edit / Options items they were - the ids are the same.

**Full screen, 1920 x 1080** (2026-11-06, Polish, a copy of POLIGON on a 2560 x 1440 screen; `-SetFull 1`,
the registry's ScreenX / ScreenY / FullScreen put back after). Seen with nothing cut off or off the
screen: the bar and its strip, Help (the contents and a page), the Menu panel, the palette (also the
long title JEDNOSTKI, ISTOTY, which the to-do list had outside the frame - it fits), the rows of the Place
tool with the placing helps (beside the strip on one row at this width), of Paint, Ground height and
Track, an object's panel, an enemy factory's panel, the Arrivals panel, and the dialogs Load, Mission
settings, New map and Map size (opened from the Menu panel, closed with Cancel; the editor draws again
after each). Not clean but not broken: while a dialog is open the game's window goes to the taskbar and
comes back when it closes; an object's panel on a rail is titled "(VID 610)" without a name; the
palette's list of categories starts right under its title and ends close to the bottom strip; the
grid's points of the placing helps are faint on bright ground. 2560 x 1440 is not checked.

**The frame and the profiler** (`src/LocoFix/profile.cpp`, `profile.h`). `LOCOFIX_PROFILE=1` in the
environment (`$env:LOCOFIX_PROFILE = "1"` before `test_game.ps1`) or the DWORD `EditorProfile` = 1 next
to `FrameLimit` turns on a per-frame profiler for the run, in the editor and in play; off it costs one
bool test per measuring point. It hooks `World_DrawLayer` (the layers 0-1 ground, 2-4 software
sprites, 5-10 Direct3D), the call of `Graphics_DrawScene` in `GameWorld_Run` (the rest of the scene:
locks, the depth copy, shadows, weather) and `Surface_Lock`; the overlay's parts (`prof::Lap` in
`draw_overlay`: heights, marks, regions, push lines, enemy trains, arrivals, tool, bar, rows, status,
panels), the present and the frame limiter's wait add their own, and the rest of the frame is
"other". Every ~2 s `LocoFix.log` gets a `profile:` line: fps, avg/max ms of every part, the time in
locks, the objects listed and drawn (and drawn only by the engine's "below the view" rule), the objects
the editor walked, the locks a frame and the locks by part. What it found on The Factory (1889
objects, 17 push lines, 23 posts, 3 mines; 2026-11-06, Polish, a 2560 x 1440 screen): the editor ran
at 42-55 fps at 1024 x 768 in a window and 23-33 fps at 1920 x 1080 in full screen (play: 55-58).
The decals drawn as sprites were not the cost (layers 2-4 took 2.3-3.9 ms against 2.3 in play, where
they are baked) and neither was `pick` (under 0.1 ms); the cost was the overlay: (1) the push lines
searched every line's way and every post's rail point again each frame - 8.8 ms, now kept while the
rail network is the same (`RailMemo`, a hash of its points and links), 0.5 ms; (2) every character of
a text locked the back and the depth buffer for itself (`SoftwareLock`), and with dgVoodoo a lock after
Direct3D drawing reads the picture back - 190-310 locks a frame, 12-25 ms in full screen; now one lock
a text, one for all the status lines, the mines' labels after their squares under one lock, the rows
above the bar in three passes (strips, Direct3D, software) - 17-23 locks; (3) the arrows' triangles
were a FillRect a row of pixels, ~3000 calls a frame - now one fan each (`fill_triangle`), and the
heights' contours one list of triangles (`FillBatch`); (4) `track_cells` (the Ground height tool's
brush) is kept for the same rails. Now: 60 fps at 1024 x 768 with ~8 ms to spare, and at 1920 x 1080
in full screen 58-60 fps with Select, Paint and Ground height (0-1.5 ms to spare). What is left is the
locks themselves, ~8 ms at 1920 x 1080: the engine's own three and the overlay's mines' labels, bar,
status lines and rows, each a sync with the GPU of 1-2 ms there; fewer would need the overlay's
software pictures drawn together, which changes what is drawn over what.
