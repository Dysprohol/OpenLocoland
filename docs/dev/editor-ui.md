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
Windows or any tool: stored and deflate entries, fixed and dynamic Huffman blocks, CRC checked; a
damaged one (a record cut off, data past the end of the file, a size its data cannot hold) is
refused with Import's failure message - the reader, `zip_read.cpp`, checks every record against
the file's end and lets no exception out (a cut-off zip ended the game up to v0.2.10) -
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
there, why a rail does not fit). `ui_upper` puts a pack's text in capitals in the pack's code page,
the letters with marks too (an object panel's title, the custom maps page's texts) - `std::toupper` knows
only a-z, and the title read "LáVOVý šTěRK" in Czech; a title the script and the map's files keep in
ASCII (`ascii_upper`, the custom maps' `clean_title`) is made otherwise. In a value `\n` is a new line and `\t` a tab, and a value
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
then the train arrivals and the orders (13, 14) turning (15) and the citadel, Charybdis, fly nests and mines (16), a factory's plan (17), the crusher, Scylla and the decorations' sets (18), the gravel bed (19), the neutrals and the pirates (20), the map files (21: a map is its folder, the
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
own (a new `HelpCat...` key in the six packs) or an existing one. Six now: BASICS (1-4, 15, 31), TRACK AND
TRAINS (5, 6, 23, 24, 28, 33), OBJECTS (7, 8, 16, 18, 22, 30), MISSION, SAVING (9, 10, 20, 21, 29), GROUND (11, 12,
19, 32), ARRIVALS, FACTORIES (13, 14, 17, 25, 26, 27). The contents has a row for every page of a category and no
second page, and the panel holds six rows: a category of more runs over the bottom strip and out of the panel
(OBJECTS had 9 pages in v0.2.11, TRACK AND TRAINS 7 with page 33; 20, 26, 29 and 31 were moved on 2026-11-15) -
a new page goes to a category with fewer than six. The 29 rows of categories and titles (up to 37 characters) do not fit a page's 14 lines, so
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
"Save the changes?", the save before Play the map; not Run the level's copy, which saves nothing) shows SAVED (`SavedNotice`, in the
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
after each). While a dialog was open the game's window went to the taskbar and came back when it closed - gone since
v0.2.10, where full screen is a screen-size window (`FullscreenAttributes = fake` in `game/dgVoodoo.conf`,
"Graphics and dgVoodoo"). Not clean but not broken: an object's panel on a rail was titled "(VID 610)" without a name (since
2026-11-11 the Track row's word and the rail's kind, "SZYNA: ZWYKLE (VID 610)"; the New map dialog the to-do
list had without Cancel has had one since 2026-10-22 - its id 2 found and clicked in the game, the dialog
closed with no map made); the
palette's list of categories starts right under its title and ends close to the bottom strip; the
grid's points of the placing helps are faint on bright ground. 2560 x 1440 is not checked.

**The interface's size in the editor** (`editor_edit.cpp` "the editor's interface size", `interface_scale.cpp`
`interface_editor_scale`). The editor's own interface - the bar and its strip, the rows above it, the status
lines, the music player, the hints under the cursor and the strip's and rows' tips, an object's panel, the
Help and Menu buttons and every panel (Help, Menu, the palettes, Arrivals, a factory's), the SAVED notice -
can be drawn twice or three times as big, with a size of its own: **EDITOR INTERFACE** on the game's
settings page under **INTERFACE SIZE** (`EditorInterfaceScale` "1"-"3" in the Players key, absent =
normal; arrows VIDs 956/957, script function 208 `EditorInterfaceSizeMax()`). It is used as far as the
editor's screen takes it, by the play interface's rule (twice from 960 px of height and 1328 of width,
three times from 1440 and 1992; a small map's stretched logical screen divides it by the stretch), and
only when the bigger copies of the editor's own pictures are there too; it is read at most 4 times a
second, so a change shows when the editor is drawn next. How: the interface is laid out in a screen of
the view's size divided by the scale (`ui_view`) by the same numbers as at the normal size, and its
drawing multiplies every place by the scale (`draw_screen`, `draw_text` through it, `ui_fill` /
`edit_fill`, all while a `UiDrawing` scope is open) and draws the bigger copy of each picture
(`interface_vid`): the play interface's enlarged pictures and fonts (the bar 450, its side tiles 938, the
icon atlas 455, the slot frame 456, the strip 497, fonts 2, 6 and 8) and the editor's own, enlarged by
the same build step (`INTERFACE_VIDS`: the editor's icons 939, the buttons 942/943, the palette's icons
944, the panels' device 466 - its bigger copies only the middle 584 x 532 of its 1024 x 768, where all of
it is drawn -, the pictures 484/486, the lamp 472, the buttons 475-477, the titles' font 3 and the lit
font 7). A text advances by its original font's box, so its bigger letters land exactly s times as far
apart. The mouse is divided by the scale for the interface's hit tests (`ui_mouse`), so the hit boxes,
the click-and-hold strips and the tips move with what is drawn; the panels get the interface's screen
from `edit_view_box` and the mouse already divided. What is drawn on the map stays one map pixel to a
screen pixel: the marks, the mines' and sockets' labels, the regions, the push lines, the tools'
previews. The bar and a tool's rows (up to three, 212 px at the normal size) stand s times as high and take the
clicks over them, so the camera goes 300 px past the map's edges times the scale (`camera.cpp`: 600 / 900 px) -
the map's bottom row comes out from under three rows of the Paint tool at three times too. A picture with no bigger copy is drawn at its own size in its place: the Track tool's kinds
of rail (the rails' own pictures) and a decal's picture where the palette has no icon. A panel stands
on the menu device, whose visible part is 579 x 479 round its middle, so it fits at both sizes (the
interface's screen is 960 x 540 at twice on 1920 x 1080, 853 x 480 at three times on 2560 x 1440); where
the screen is too short for it centred (three times on 2560 x 1440) the device moves up into it
(`device()` in `editor_panel.cpp`; never at the normal size), and the first status line - it says what the
panel's row under the mouse does - is drawn again in front of it. The cursor follows the editor's size while
the level is held (`interface_cursor_scale` every held frame); Run the level shows the game's own bar at
the game's **INTERFACE SIZE**, the cursor with it, and back in the editor the next frame puts the
editor's again. At the normal size every number and every picture is what it was. The settings page's
preview shows the play bar only (no picture of the editor's bar). The test mode's factory panel rows
(`H`) are logged in the interface's screen (so `tools\rig\panel_rows_check.py` reads them at any size),
and `G` / `T` click their middle times the scale, in the window's pixels (`logical_to_window`: a map narrower
than the window has its logical screen centred in it). Checked in the rig (2026-11-14, a copy of The Factory with
the factory panel's test settings): at the normal size the editor's bar, strip, rows and every panel
pixel-identical to the build before at 1024 x 768, 1920 x 1080 and 2560 x 1440 in the same 34 steps (only the
map's animations differ); at twice (1920 x 1080, 2560 x 1440) and three times (2560 x 1440) every bar entry, the
palettes (a pick), the Arrivals panel, Help (contents and a page), the Menu panel, the Track and Paint rows, a
hint, an object's panel by the right button, a drag and its undo, a click-and-hold strip, Run the level and back,
Save and Load; the factory panel's 30 pages in Polish, German and Hungarian at both sizes with no fault of
`panel_rows_check.py`; the settings page's row (its arrows, its tip, back in the editor at the size chosen).

**Zoom in the editor** (`zoom.cpp`, `zoom_view.cpp`, `editor_edit.cpp`; engine.md "Zoom"). The **mouse
wheel** over the map zooms the held level's map in and out by play's steps and caps (zoom.cpp's ladder, its
ends and its glide, whatever they are - except that out it goes on to the whole map with the void and the
map's frame round it, where play stops at the map filling the window), the map point under the cursor kept, as in a mission; **Ctrl+0** goes
back to 100 % (help page 3). What the wheel did for the tools until then moved to **Ctrl+wheel**: a brush's
size, the Track tool's ramp, turning what is placed or selected, the scatter's radius, a post's side (the keys
stay: R / E, + / -, PgUp / PgDn); Shift+wheel is as before (the selected object's height); an open panel takes
the wheel first (Help's pages, the palette's pages); over the bar, its rows, an object's panel and the music
player the plain wheel does nothing (as in play over the bar); the wheel's click is unchanged. The two meanings
are one pair of constants in `editor_edit.cpp` (`WHEEL_ZOOM_KEYS`, `WHEEL_TOOL_KEYS`), to swap if wanted. The
status line says " - zoom 50% (Ctrl+0: 100%)" (`StatusZoom`) after "not saved" while zoomed, nothing at 100 %.
No zoom while a panel is open or a drag or a stroke is under way (`refused`, in the test log). How: the map is
drawn at the zoom in a logical screen of the window / z and stretched to the window, as in play; the editor's
drawing is split in two - `draw_overlay` (the marks on the map, the tools' previews, the cursor) right after
`Graphics_DrawScene` in logical pixels, so it is stretched with the map, and `draw_interface` (the bar, the rows,
the status lines, the music player, an object's panel, the tips, the hints, the panels, the SAVED notice) after
it at zoom 1 - the same calls in the same order as before -, under a zoom in zoom_view.cpp's second pass over the
stretched picture at the window's pixels (`edit_interface_pass`: the game's window-sized surfaces, the
window's software depth surface cleared to the engine's empty value and locked by the engine as it draws, the
hardware z-buffer cleared, the render states `Graphics_DrawScene` leaves; play's interface pass is not run, the
held level has no interface objects). Not memory of ours for the depth, as play's pass has: the engine's
`Graphics_DrawVid` locks the back buffer itself for a picture drawn outside a lock and unlocks the back and the
depth buffer after it, so the window's own depth surface came in, never cleared there - the texts of earlier
frames kept the rows' strip and a panel's device out of their letters (the map showed through as old text), and
that surface stayed locked, so back at 100 % its locks failed ("surface busy") and the frames lost pictures. Under a zoom the interface's screen is the window's (`ui_view`), its scale is
chosen for the window (`interface_editor_scale`), and its mouse is the message's own window point
(`g_win_mouse`, `ui_mouse`, `ui_of`) divided by the scale; everything on the map keeps the logical mouse
(`g_mouse`): picking, drags, the tools, the regions' handles, the edge scrolling. The two places that put
the interface by a map point convert it (`map_to_ui`): an object's panel beside the object and the mines'
labels' test against the bar; the mines' labels and the socket numbers of an object's panel are drawn with
the map then. The camera's margin past the map (camera.cpp, 300 px times the interface's size) is in map
pixels, times `zoom_ratio()` (at the normal size 600 at 1/2, 150 at 2). Run the level loads its level at its own zoom,
and the running level zooms with the plain wheel as a mission does (the game's own wheel, play's refusals: over the
bar, in a menu, paused); editor.cpp keeps the editor's zoom and the view's middle (`zoom_editor_kept`) and asks for
it again when the editor holds the map again (`zoom_editor_restore`, after the next frame). The editor's way of the
wheel is taken only while a level is held (`editor_zoom` in zoom.cpp): the editor mode's flag (World flags 0x1) stays
for the whole game, and until 2026-11-20 zoom.cpp chose the editor's way by that flag, so in a running level it
waited for the editor's window procedure, which passes the wheel only while the level is held, and refused ("the
level not held") - the wheel did nothing in Run the level. Checked in the rig (POLIGON, 1024 x 768): the editor at
125 %, Ctrl+R, two notches in (125 %, the map point under the cursor kept), three out (89 %), two over the bar not
taken ("over the interface"), the Esc menu and back still at 89 %, a notch in (100 %), Ctrl+R - the editor's 125 %
back, its wheel zooming as before; a mission's wheel as before. After that the plain wheel still zoomed no more
in the editor once back from a run (the maintainer, 2026-11-20): the editor's tools know Ctrl, Shift and Alt by
their own key messages besides the keyboard (a test posts its keys, which are not in the keyboard's state), and
those come only while the level is held - Ctrl of the Ctrl+R that started the run was let go once the run's level
was loading (the load is the next frame), so for the tools it stayed down and every plain notch was Ctrl+wheel,
the tools'. Back by Ctrl+R its own Ctrl, let go after the load, put
it right - the control's check came back that way; back by the Esc menu's BACK TO THE EDITOR (or a won or lost
run) nothing did. Now the editor forgets the three keys when it holds a level and when a dialog closes, as it
did after Alt+Tab (`keys_forget`, `edit_keys_forget`; a key still held is read from the keyboard), and the test
log says "edit: wheel N: the tool's (Ctrl held)" for a notch the tools take. Checked in the rig (mission 13 in
the editor, Ctrl let go seconds after R): before - back by the Esc menu, two notches and no step (back by Ctrl+R,
the test's keys or the guest's own keyboard, Ctrl let go 0.1 - 0.3 s after R: zooming); now - back by
the Esc menu zooming (1024 x 768, after a run zoomed and after one not), back by Ctrl+R (full screen 1920 x 1080,
two runs in a row), with real keys (Ctrl+R with the guest's keyboard both ways), Ctrl+wheel the tools', Ctrl+0
100 %; mission 5's wheel as before, the 20 missions load. Load, New map and a level change reset it (the level's
load, as in play). The test mode's factory panel click (`G` / `T`) takes the interface's point as a window
point under a zoom. Checked in the rig (2026-11-14, mission 13's map, 1920 x 1080, the interface at the normal
size and twice): at 2/3, 1/2, the whole map (47 %), 1.5, 2 and 3 a house placed lands on the map point under the
cursor and dragged lands under the new one, track starts at the clicked point's lattice spot, Shift+wheel lifts
the selected object, Ctrl+wheel sets the Track tool's ramp, undo, Help and its pages; the wheel over the bar
zooms nothing; Ctrl+0; Run the level and back at 67 %; Save. Paint strokes at the map's four corners at 2/3, 1/2,
the whole map, 1.5 and 2, the camera's limits exactly the margin in map pixels past the map. At 100 % the editor is
pixel for pixel 3cf0f07's (27 steps; only the map's own animations differ) and as fast (alone in the test box:
47.6 / 49.8 fps). Frame time zoomed out (the test box's virtual adapter, alone, no frame limit, mission 13): 100 %
50.8 fps, 2/3 23.9, 1/2 17.7, the whole map 17.5 (57 ms); 1.5 / 2 / 3: 63 / 75 / 107; private memory 301 -> 314
MB, the address space 659 -> 678 MB of 2048. A 4096 x 3072 map: 100 % 78.5 fps, 2/3 33.8 (its last step: 1/2 and
the whole map are over the pixel cap), 327 MB private. Zoomed out the void's 1-pixel grid past the map's edge
thins with the shrink.

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
the editor walked, the locks a frame and the locks by part; then the time in `Graphics_DrawLight` and the
lights a frame (`in lights`, `lights`: it is part of `d3d`) and the locks by the code that took them
(`locks by caller`: the return address in the exe, how many a frame and their ms - in play 0x40F294 and
0x40F2CE are the sprites' back and depth buffer, 0x40F39E the depth buffer again after the depth copy,
0x40F613 and 0x40F64D the pair of layer 10). Locks the engine takes through the surface's vtable
itself are not among them (the minimap's dots in `Graphics_PutPoint` / `Graphics_PutPoint2x2`; their
time falls into `other`), so the line ends with every lock of the back and the depth buffer, counted
where the surfaces' `Lock` and `Unlock` are guarded (`lost_surfaces.cpp`): `bb` the back buffer's locks
a frame and their ms, the ms of its unlocks, `z` the same for the depth buffer, and `bb by caller` -
a lock taken inside `Surface_Lock` under the address that called it, the others under the address
after their own call (in play 0x405D5D the minimap's rail points, 0x405EA5 its dots; addresses outside
the exe are LocoFix's). With dgVoodoo each back-buffer lock after Direct3D drawing is a wait for the
GPU; the depth buffer's first lock of a frame (0x40F2CE, after the ground's height blits into it) costs
time as well (1.7 ms a frame on mission 13 at 1920x1080 in the test rig, before fix 56). `in zcopy` is the
time of `Graphics_CopyDepthToZBuffer`, the depth buffer's copy into the hardware z-buffer between the scene's two
halves (part of `scene-rest`). `EditorProfile` = 2 (or `LOCOFIX_PROFILE=2`) also starts the sampler
(`src/LocoFix/sampler.cpp`): a thread of its own stops the main thread about every millisecond and notes
where it is - its instruction pointer, the first return address into the exe on its stack (for the time
spent in dgVoodoo or the driver: the engine's call that led there) and the view's width (to tell a zoom
out apart) - into `logs\sampler.bin`, the modules into `logs\sampler.txt`; `tools\profile_samples.py
<logs folder> [--from s] [--to s] [--view-over w] [--map LocoFix.map]` prints the shares (exe functions
by `re/symbols.csv`, LocoFix's by a link map when one is given). Nothing is allocated, logged or loaded
while the main thread is stopped. What it found on The Factory (1889
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

Measured again on 2026-11-11 (issue #9: "FPS drops from 70 to 22 upon opening ESC menu"; the to-do's 56-58 fps
on The Factory at 1920 x 1080). The Menu panel cost 11.5-12 ms a frame at 1024 x 768 on POLIGON (0.13 without
it; still 60 fps there, with 0.5-1.6 ms to spare): timed inside `draw_menu`, the device's and the picture's
pictures 1 ms, the lamps 1.4, its fifteen texts 10 ms - each text a lock of its own after Direct3D drawing, a
sync each, more costly on a bigger screen. A panel's texts are now queued while it is drawn and written after
its pictures under one lock (`edit_texts_begin` / `edit_texts_flush` / `edit_texts_end` of `editor_edit.cpp`,
around `panel_draw`; the Arrivals and factory panels flush before their click-and-hold strip, which comes over
their rows): the Menu 2.1-2.4 ms, Help 2.1-2.3 ms, the same picture. The Factory at 1920 x 1080 in a window
(the profiler's lines): Select 53-55 fps, Track 45-53, the Menu open 52; of its ~18.5 ms the engine's parts
(sprites 2.6, Direct3D 1.2, the rest of the scene 6 ms) and our software passes - the mines' labels 2.9 ms (a
lock of their own right before the status lines took theirs), the bar 2.3, the status lines 1.9. The mines'
labels and hidden pictures are now drawn in the status lines' lock (`draw_mine_labels`; their squares stay
where they were), so they come over what is drawn between - the push lines' arrows, the tool, the bar -,
which only matters for a mine under the bar - there the label's characters and a hidden mine's picture that
fall on the bar or the rows are left out (`draw_mine_labels`, the review of 2026-11-11: they came out over the
strip; checked with a mine on POLIGON's track under the strip, its label over the strip before, covered
now): Select 59-60 fps with 3-3.7 ms to spare, Track 59-60, the Menu
open 57.5-60 in that run - but the frame had no time to spare with it (wait 0-0.7 ms), and the control
session's run of the same build with the game-picture commits measured 49-53 fps with the Menu (ed-panels
4.26 ms, 16 locks), as did one of two runs of mine. The 16 locks: the music player's four texts (8, cheap - no
Direct3D between them), the queued texts (2), and the device (466), the picture (484) and the start button
(477), software sprites (layers 2-4) that locked for themselves after Direct3D drawing - a sync each. Those
go into the same queue now, in call order (`edit_draw_picture`), with a flush after the panel's background
(`draw_frame`, so the lamps and icons Direct3D draws come over it) and one at the end: 12 locks, 2 syncs.
Measured back to back on The Factory, 1920 x 1080 in a window, 14 s Select then 14 s Menu, two pairs: before
57.2-59.9 and 49.3-50.4 fps with the Menu (ed-panels 3.6-4.8 ms, max 5.2-7.1, wait 0), after 59.8-60.2 in both
(3.6-3.7 ms, max 4.2-4.7, wait 0.1-0.7 ms); Select 60 in all; Help, the Arrivals panel and the palette drawn as
before. Not measured in full screen (the automatic tests do not switch the mode); the bar's own lock is
needed for its plates, which are software pictures. The track's joints of issue #7 (the Track tool's marks,
the preview's dot, snap, Check the track's ring) are Direct3D triangles in the regions' batch and their
answers are kept while the rails are the same: see editor-objects.md "What the joints cost a frame".
