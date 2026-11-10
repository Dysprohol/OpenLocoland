# How the game is built

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

## How the game is built

**Engine.** C++ compiled with Visual C++ 6.0 SP5 (exe from 2003-10-07, internal project
name "Khon-Kaa-Doo"; "Steam" is a working name and has nothing to do with Valve). The C
runtime is linked statically, the exe has no relocations (always at 0x400000). It uses
DirectDraw 7 + Direct3D 7 (16-bit), DirectSound, OGG music, movies through DirectShow.

**Graphics and dgVoodoo.** On Windows 10/11 the system DirectDraw does not set the 16-bit
mode (`Couldn't set the display mode`) and corrupts the picture, so dgVoodoo2 is a
permanent part of the game - it translates DirectX 7 to modern APIs. Settings in
`game/dgVoodoo.conf`: `AppControlledScreenMode = true` (the game picks the mode),
`DesktopBitDepth = 16` (without it the launcher blocks windowed mode because the desktop
depth differs from the game's), `CenterAppWindow = true`, `Bilinear2DOperations = false`
(with it on dgVoodoo filters every Blt and shifts the window's picture by a pixel, see
LocoFix fix 23 below; the price is a nearest-pixel stretch of mission 1 and the menus in a
window bigger than 1024x768), `FullscreenAttributes = fake` (full screen is a window as big as the
screen and not exclusive full screen: with exclusive full screen every Windows dialog of the editor -
Mission settings, New map, Map size, Load - took the activation and the game went to the taskbar
until the dialog closed; with the screen-size window the game stays behind the dialog. Checked by the
maintainer in full screen on his machine, 2026-11-10; not checked by a script - a full-screen test
started from a script draws nothing).

**Configuration.** The game reads `<exe name>.cfg`, i.e. `Locoland.cfg`. Graphics settings
(ScreenX, ScreenY, BPP, Device, FullScreen, SoundHighQuality, WindowPositionX/Y) are kept in
the registry under `RegPath`; the `HKEY_CURRENT_USER\` prefix picks the hive, so saving
works without admin rights. Player data is saved by the scripts separately under
`SOFTWARE\Gromada\steam\Players` - HKLM, without a prefix; current Windows no longer
redirects a normal user's writes there to the VirtualStore, so LocoFix moves that key to
`HKEY_CURRENT_USER\SOFTWARE\Gromada\Locoland\Players` (fix 24 below, `BUGS.md` F15).
LocoFix keeps its own settings next to them, chosen in the launcher: `Language`,
`FrameLimit` (frames per second, default 60, 0 = no limit), `FpsCounter` (0/1) and the
options `Cheats` and `UnlockMissions` (0/1, DWORD; "Quick start", `options.cpp`), and for the update
check `UpdateCheckAtStart` (0/1, DWORD) and `UpdateLastCheck` (the date and the tag it found,
`update_check.cpp`).

**Game logic in scripts.** Missions, AI, menus and end screens are in `maps\**\*.lgc` - a
C-like language of its own, compiled to bytecode on every start. Engine functions available
to scripts are declared in `maps\script\export.lgc` (132 numbered functions). Scripts are
in CP1251 (comments in Russian). The compiler and the bytecode: "Script machine" below.

**Data.**
- `objects.res` - 687 object types ("VID", numbers 0-937, `OBJ` section), weapons (`WEAP`, 55
  entries of 68 bytes), sounds (`SFX`), constants (`CNST`). `re/data/objects.tsv` (from
  `unpack.py`) has the number, name, weapon index and icon frame of every object.
- A map = `lNN.vid` (pre-rendered ground), `lNN.map` (objects: chunks `OBJ`, `GRPH`, `HEAD`,
  `GRID`, `SPR`, `SPRD`, `PLAY`, `GROU`) and `lNN.lgc` (mission script) + briefing texts.
- `.res`, `.map`, `.vid` files and demos share a container: `RES ` + size, type (`MAP `,
  `VID `, `DATA`, `DEMO`) and chunks with four-letter names. Layouts: "Data formats" below.
- `vid\455icon.vid` - unit bar icons: a 256x256 RGB565 texture from 0x52 and a table of
  54 frames. A unit's frame number = field 13 of its WEAP entry.

**Texts.** `strings.ini` (CP1250, without Polish diacritics - that's how the existing
translation looks) is read with GetPrivateProfileString: leading spaces in a value need
quotes. The mouse tip is the name of the object under the cursor (`[Units]`, keyed by the
object number) and, when there is none, an entry of `[MouseTips]` for the menu item under the
cursor: `MenuVid<nvid>AllDir` first, then `MenuVid<nvid>Dir<dir>` (direction of the item's icon,
`MenuNVidUnderCursor`/`MenuNDirUnderCursor`); `World_GetMouseTipText` 0x41f5c0.
Briefing and help texts (`maps\*.txt`) are in CP1250 in the Polish version. The fonts in
`fonts\*.bmp` (a 16x16 grid of characters, byte 0x00 top left) partly have Polish letters in
place of Cyrillic; `shiftb.bmp` (the Shift+B menu) does not. The English fonts hold the
original Cyrillic in 0xC0-0xFF and no letters with marks; `font-MainMenuTitles.bmp` (VID 3,
the menu's headers) and `font-Objectives.bmp` (VID 14, the messages) have capitals only. Each
cell has a one-pixel grid line on its right and bottom; the fonts are 24-bit, the objectives' and
the tips' 16-bit (5-5-5) in the English, Hungarian and Russian versions. The Czech pack pastes
its own letters into them ("Languages" below).

**Developer mode.** `#define KEYS_ENABLE 1` (in `maps\script\constants.lgc`, off in the
original game) enables the keys from `dokeys.lgc`: h - repair, W - victory, B - create units (menu `maps\shiftb.txt`),
A - change army, K - destroy the enemy, D - AI debug view, M - money, # - big train.
(`#` creates 16 wagons of army 0 - the nuclear mortar, the arsenal, mortars, the repairer and
rocket carts, no locomotive - at the pointer, and the engine puts them on the track point
nearest to it, `Engine_PutOnRails`: with the pointer away from the track they appear on the
nearest one, possibly outside the view).
The exe also kept the editor mode of the authors' map editor - a File menu with Load and Save
and a map saving function (`Level_Save`, script `Save()`) that writes the same `.map` format
the game loads; LocoFix brings it back (`--editor`, see "Editor mode").
In OpenLocoland the define comes from the launcher's Cheats option: `constants.lgc` (the root's and
the `lang/pl`, `lang/en` patches) includes `maps\script\cheats.lgc`, which defines nothing, and
LocoFix opens `cheats_on.lgc` (`#define KEYS_ENABLE 1`) in its place when the option is on
(`options.cpp`, fix 30 below); the scripts are compiled at every level load, so the launcher's
choice counts from that start on. The keys are read from `WM_CHAR` (`Mouse.key`, script `GetKey()`),
so W, B, A, K, M are the capital letters (Shift).

**Languages.** The language versions differ only in data files: `strings.ini`, `fonts\*.bmp`,
`vid\logo.bmp`, `maps\*.txt` and `maps\lNN\*.txt` (briefings, objectives, win/lose
texts). The exe, `objects.res` and the scripts are the same. The English version is called
"Steamland" in its texts and logo. Each language is captured once from the player's Steam files into `captured/<code>/`
(`tools\languages.py`, run by `build.py`). The
repository keeps only our translation work in `lang/<code>/` (see "Repository layout"): the
completed `strings.ini` as `game/strings.override.ini`, our texts in `game/briefings/`,
`game/menus/` and `game/editor/`, the developer mode texts as patches of `maps\shiftb.txt`
and four scripts in `game/scripts/`, and a pack's own letters in `fonts/` (Czech, below).
`build.py` assembles each captured pack with those changes
into `<game>\lang\<code>\`, every file at its place there (`languages.pack_path`).
English is the fallback, in three ways. The root of the assembled game gets the English
capture's (`captured/en`) original language files, unchanged, whatever language Steam has
installed (without an English capture Steam's stay, with a note) - so the root `strings.ini` has
none of our keys and no `[Editor]` section; our own files at the root (the custom maps' texts,
the editor's help pages in `game/maps/`) are English. Our own keys fall back to English text
given in the code: the editor's (`[Editor]`) to the English of `ui_text("PanelNew", "NEW MAP")`
in LocoFix, and the keys our scripts add (the custom maps page's `MenuCustomEditor`,
`MenuBackToEditor` ...) to the English in the script (`if(s=="") s="MAP EDITOR";`). And LocoFix
takes the English pack for a saved language without a pack, or without an English pack the pack
of the root's language - the one `build.py` writes into `lang\root.txt`: `en`, or with no English
capture the language Steam installed.
With no saved choice it follows the system language when there is a pack for it (Polish,
German, Hungarian, Russian, Czech), otherwise English; a saved choice is kept, and the launcher's
list switches it. Portuguese has no pack: its Steam depot is byte for byte the English one, so a
Portuguese system gets English.

**Czech** is a seventh language that Steam does not ship: the Czech fan translation (unofficial)
of the English version - `strings.ini`, the menus' and the missions' texts (a player who has the
Czech edition of the game gets its own texts and fonts instead: "The Czech edition" below). A pack like it names
its base in `language.ini` (`base=en` in `[language]`, `languages.pack_base`) and is marked
unofficial in the launcher's and Setup's lists (`[launcher] unofficial=` of the chosen language,
Setup's `TEXTS`): `build.py` assembles it from that language's capture with `lang/cs/` over it -
or, when the base is not captured (a player with the Polish Steam copy only), from another
captured language (`languages.build_base`: en, pt, de, pl, hu, ru, the first there is), with a
line in the build's log; `needs_base=1` in `[language]` would keep a pack to its own base, and
Setup would then list it greyed with that base's `download_depot` line. On another base the
logo and the fonts are that language's, with the pack's Czech letters pasted in (ASCII and those
letters draw the same with all of them; `check_lang.py` checks a fan pack with every captured
font set, the letters pasted in) and so are the files the pack does not have:
`maps\menu2.txt` and `menu3.txt` (empty in every version), `shiftb.txt` (the developer keys'
help, English in every version) and `l08\brief2.txt` / `l15\brief2.txt` (empty, only the Polish
version has them) - none shows anything of the base's language. What differs is `strings.ini`:
the Polish one leaves 80 keys empty that the English one fills with references
(`MenuGamer={strings:HeadPlayerName}:`, the tooltips `MenuVid455Dir*`, `MenuVid468Dir*`, units
`117={Units:115}` ...) or has other words (`MenuVid471Dir0=Zadania`), and German, Hungarian and
Russian a few too, so `strings.override.ini` sets all of these to the English values (84 keys)
and the pack reads the same on any base; `check_lang.py` reports such keys a fan pack leaves to
its base. `lang/cs/game/` holds the fan translation's texts converted
to what the game reads: the briefings, menu pages and the editor's help pages in ASCII with CRLF
(it was UTF-8 with two files in CP1250, and its few Czech letters, dashes and a Cyrillic O were
written without marks while the fonts had no Czech letters; their diacritics are to come), and
`strings.override.ini` its `strings.ini` on the English keys - its two renamed keys (`Penize`,
`Rychlost casu`) and the empty or English values completed in Czech -, our keys and the editor's,
in CP1250 with every diacritic: the revision by the translator Danetry (merged from his fork with
his commits), in his words (Továrna for VID 102, Opravovací depo for 104, Důl for
165, Mina for 86). The game
does not wrap a text, so the fan translation's long lines - it wrote paragraphs on one line - are
broken at the English files' widths (briefings, menu pages, win and loss 44 characters, objectives
40, the tutorial's scripts 54), the key columns of the controls pages (`menu41.txt`, which had lost
its ESC 7 / ESC 2, and `menu42.txt`) are aligned again, and mission 1's briefing, too long for one
page then, has a third page (`l01\brief3.txt`, listed by the pack's own `l01\brief.lst`); the
menu's and the missions' headers (`MenuHeader*`, `MapHeader*`, drawn with the capitals-only VID 3)
and the objectives (the messages' font VID 14, capitals only too) are in capitals.

**The Czech letters** are the pack's own (`tools/font_cells.py`): none of the captured fonts has
them, and the repository holds no copy of the game's fonts. `lang/cs/fonts/<font>.png` is a
picture the size of the font with only our marks in it, each in the cell of its CP1250 byte (Á C1
á E1, Č C8 č E8, Ď CF ď EF, É C9 é E9, Ě CC ě EC, Í CD í ED, Ň D2 ň F2, Ó D3 ó F3, Ř D8 ř F8, Š 8A
š 9A, Ť 8D ť 9D, Ú DA ú FA, Ů D9 ů F9, Ý DD ý FD, Ž 8E ž 9E), in four colours: white the font's
ink, grey 128 its shadow (the second letter colour, font-Objectives' dark edge), black its paper
(erases a pixel of the letter), transparent the base's pixel. `languages.install` makes every cell
of a sheet from the base font's own Latin letter - the code page's letter without its mark
(`unicodedata` NFD: Č -> C) - and paints the sheet's pixels over it in that font file's own colours
(the paper of the space's cell, the ink and shadow the two commonest other colours of A-Z, the
grid line's colour left out), writing the BMP back in its own format (16-bit 5-5-5 or 24-bit,
Steam's header) with only those cells changed. A sheet applies to every font whose name starts
with its own: `font8x13.png` (VID 2, the texts) also to `font8x13light.bmp` (7, the lit text),
`font-Objectives.png` (14) to its `-green`, `-red` and `-yellow` copies, each in its own colours;
`font-Mousetips.png` (6) and `font-MainMenuTitles.png` (3) to theirs. font8x13, font8x13light and
font-Mousetips get the 30 letters, font-Objectives and font-MainMenuTitles the 15 capitals (their
lower case is empty in every version); font5x5, font-unitnumbersonscreen and shiftb.bmp are left
alone. On the Russian base the cells replace Cyrillic letters, on the Polish one its Ś Ź ś ź (8C
8F 9C 9F; the Czech pack's Š Ť Ž are 8A 8D 8E) and Ó ó in the same bytes. The marks follow the
Polish fonts' and the Czech edition's (a Czech edition of the game had all its fonts with
diacritics; two screenshots of it were the reference): in font8x13 the capitals' acute and caron
two pixels high with a free row above the letter as the Polish Ś, the small letters' above the
x-height with one free row, ů's ring three pixels, ď and ť an apostrophe right of the stem (the d
moved a pixel to the left for it); in font-Mousetips the capitals' marks sit on the letter (no
row is free) - Ť's caron a pixel right of the stem, so it does not read as a cross, and Ů's ring
in the mouth of the U; in font-Objectives the marks are drawn two pixels wide with the font's shadow, the
caron three rows; in font-MainMenuTitles, whose letters fill the cell, the acute and the caron are
cut into the letter's top bar with black slits and hang one or two pixels into the opening below
(Í, Ť, Ž have none under the bar; Ú, Ý, Ů get the mark in their open top), Ň's caron over the
bar's left part. `python tools\font_cells.py preview cs [--base <code>]` writes the assembled
fonts and a picture of every changed cell, enlarged, under its base letter, into
`build\fonts-cs-on-<base>\`; the sheets were made with a draft script (letter + mark) and then
corrected by eye at 4-8x, and are edited with any picture editor that keeps the four colours.
LocoFix needs nothing for them: the fonts are opened with CreateFileA, which takes the chosen
pack's copy (`language.cpp`), as the Polish pack's own fonts are.

**The Czech edition.** The game was sold in the Czech Republic on CD as "Steamland" (US Action,
2003): the Steam exe of 2003-10-07 byte for byte, the same `objects.res`, maps and scripts, with
Czech `strings.ini`, menu pages and mission texts, and fonts that have every Czech letter in its
CP1250 cell (the Cyrillic gone; no `vid\logo.bmp` - the root's English "Steamland" logo fits
it). The inventory of a tester's copy, its slips and what was decided are in
`czech-edition.md`. `languages.py` knows the edition by the SHA-256 of its `strings.ini`
(`EDITIONS`), and `build.py` and Setup capture it like a depot into `captured/cs` (`capture_all`,
`edition_folders`): from the game folder itself when that is the edition - a player without Steam
points `build.py --steam` or Setup's Steam folder at it, and the rest of the build is as with
Steam - or from a folder named `Steamland` in or next to the Steam game's (`EDITION_FOLDER`); by
hand, `languages.py capture --steam <folder>`. With that capture the Czech pack is built on it
(`build_base`: a pack's own capture before its `base=`) and is not a fan translation any more:
`languages.install` keeps the edition's texts and fonts and takes from `lang/cs/` only what the
edition does not have - the files the capture has no file for (the editor's help pages,
POLIGON's and the template's texts; the pack's translations of the edition's menu pages and
mission texts are left out, `translates`), of `strings.override.ini` the keys the edition lacks
or leaves empty (ours and the editor's; `patch_text.merge_ini` `fill_only`) and the lines marked
`!`, corrections of the edition's own slips (three typos in headers, two messages ending in a `…`
its fonts cannot draw; elsewhere a `!` line is ignored), and of the font sheets only the cells
the fonts leave empty (`font_cells.apply` `only_empty`: none on the edition's). The assembled
`language.ini` gets `base=` the capture built on (`pack_ini`), so the launcher's mark
(`language.cpp`) and Setup's (`languages.unofficial`) show only on another language's capture.
`check_lang.py` checks the pack against the edition's fonts as well, and `font_cells.py preview
cs --base cs` shows what its fonts have already. Without the edition nothing changes: the pack
builds on the English capture, or another captured language, as before.

Each pack's texts are bytes in its own code page, the one its fonts are drawn for:
`codepage=` in `[language]` of `language.ini` - 1250 Polish and Hungarian, 1251 Russian, 1252
English and German. LocoFix converts with it wherever text crosses into Windows (the editor's
menus, dialogs and message boxes are the W functions: `editor_text.cpp`, `language_codepage()`
of `language.cpp`), and the map editor writes a map's texts in it. `tools\check_lang.py` checks
our texts of every pack against its code page and against the captured fonts (with the pack's own
letters pasted in, as they are assembled): that each
character has a picture in the fonts that draw it, and - in a pack that is not Russian - that
the picture is not still the original Cyrillic letter of the English fonts (an Ö of the German
pack drawn as Ц). A pack without a capture is checked against its code page only;
`--fonts en` previews a pack with the English fonts.

The texts of our maps are per language the way the missions' are: `language.cpp` reads a file
from `lang\<code>\` when the pack has one, so `maps\sandbox\brief.txt` at the root is English
and `lang/pl/game/briefings/sandbox/brief.txt` (`de`, `hu`, `ru`) replaces it in that pack - POLIGON's
objectives, briefing pages, win and loss texts, and the template's (`lang/<code>/game/briefings/template/`),
which File > New copies into a new map in the language it is made in. Not the high score page
(`hiscores.txt`): the game writes it at the root after a win, with the pack's own header, so a
pack copy would hide it; until the first win it shows the root's English header.

**Logs** are in `logs\` of the game folder. The game writes `logs\error<date> <time>.log` and
renames it to `logs\error.log` on a normal exit (replacing the previous one) - by itself, once the
folder is there (`build.py` makes it, and LocoFix at startup when it is missing; without it the
game writes next to the exe, see "The error log"). After a crash or a killed process the dated
file stays behind; LocoFix keeps the 3 newest of those and deletes the rest at startup.
`logs\LocoFix.log` starts afresh every run, and Setup's `--silent` / `--auto` write
`logs\install.log`. An update moves the logs an older build left in the root into `logs\`
(`build.py` `clean_up`, see "Updating"; one `logs\` has already is deleted instead).
