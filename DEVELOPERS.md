# OpenLocoland - developer documentation

The technical side of the project: how the game is assembled, what we know about the engine and
its data, the fixes, the editor, the release. The front page for players is `README.md`.

An open, maintained version of the Locoland engine (Gromada / ESDigital Games, 2003), in the
spirit of OpenRCT2 and OpenTTD. It needs the original game - the Steam version - and builds
on top of it: today the original exe runs with our fixes loaded as LocoFix.dll; step by step
the engine is documented (names and struct types for the decompiled code) and its functions
are rewritten in readable C++ that replaces the originals one at a time, so the game stays
playable all the way. Not affiliated with the rights holders of Locoland.

The goals: run well on current Windows (window, any resolution, no external setup), fix the
game's bugs, open it up for mods, new missions and translations.

The principle for what goes where: the repository is for our own work - code, tools,
documentation, names and types, and our changes to game files as patches. Everything that
belongs to the game is taken from the player's own copy when the game is assembled, and
everything derived from it (decompiled code, unpacked data) is generated locally (see
"Distribution").

Code, tools and documentation are in English. Other languages appear only in the game's
translations (`lang/`).

Buildings are named the way the game names them to the player: VID 102 is the **factory**
(Polish "Fabryka", strings.ini `102=`) and VID 104 the **repair point** (Polish "Punkt
naprawczy"; the English game calls it "Repair Depot", which is why "depot" is not used for
either). VID 165 (`завод`, the industrial building with two tower sockets like the fort's) is
the **mine** - Polish "Kopalnia", English "Mining plant" (the game has no name for it; ours are
in `lang/<code>/game/strings.override.ini`) - and VID 86 is the explosive **mine** the repair car lays
(`МИНА`, English "Mine"): the Polish translation called it "Kopalnia", mistaking it for a mine
pit, so the Polish pack now names it "Mina" and keeps "Kopalnia" for VID 165 alone. Our texts in
the other languages take the names of that language's own `strings.ini` (German Fabrik and
Werkstatt, Hungarian Gyáregység and Szervízdepó, the authors' Russian Депо and Ремонтная
мастерская), so they read like the rest of that version of the game. The engine's
own identifiers keep the authors' names - `B_DEPO`, `Depo_*`, `IsNoDepo`
come from the Russian "депо", `Building_UpdateWorkshop` is ours - and so do the names of
files and functions already in the code.

## Quick start

**Players** need only Locoland installed through Steam and `Setup.exe` of a release (see
"Release"; no Python, no Visual Studio). Setup finds the Steam game, installs OpenLocoland into
a folder of its own (`%USERPROFILE%\Games\OpenLocoland` by default; the Steam folder is only
read and never changed) and makes the shortcuts "Locoland" and "Locoland - edytor map" ("map
editor" in English) on the desktop and in the Start menu. In its window, under the game's logo
(read from the Steam copy): the window's language (the system's when the window has it -
English, Czech, German, Hungarian, Polish, Portuguese, Russian - otherwise English), the Steam
folder and the install folder (both can be chosen by hand), the game's language (for a new
install the system's when there is a pack for it, otherwise English; an update keeps the one
chosen; only the languages that can be built are listed, with the Steam console commands for
the others) and the two options below. Running Setup again over the install updates it; Uninstall
in the same window removes it. The player's guide is `docs/PRZEWODNIK.md` / `docs/GUIDE.md`.

**Options** (off by default; chosen in Setup and as two boxes in the game's launcher window,
under the frame limit - the launcher's choice counts from that start on, nothing is reinstalled):

- **Cheats** ("Kody twórców" / "Cheat keys"): the developer keys of the original game -
  h repair, W win, B create units, A change army, K destroy the enemy, M money, # a big train
  (wagons on the track nearest to the pointer), x an explosion, D the AI's debug view (`maps\script\dokeys.lgc`; see "Developer mode").
- **All missions** ("Wszystkie misje"): every mission of the strategic map can be played. The
  player's progress is not written: the later missions show as playable while the stored progress
  stays as it was, and a win advances it only when the mission won is the next one in line (as
  always in normal play, where a later mission cannot be played) - so turning the option off
  shows the real progress again. The missions' scores are saved as before.

**Updates.** "Sprawdź aktualizacje" / "Check for updates" in the launcher asks GitHub for the latest
release; a newer one than the running version is offered with its Setup.exe, downloaded into the
Downloads folder and started on a click (LocoFix fix 33); the box "przy starcie" / "at start" next to
it (off by default) checks every time the launcher opens. Nothing is updated by itself.

**Developers** build the game from the repository instead. Requirements: the game installed
through Steam, Python 3, Visual Studio or Build Tools with the C++ tools (for LocoFix.dll); for
decompiling, Java 21 and Ghidra.

```bat
python -m venv .venv
.venv\Scripts\pip install -r tools\requirements.txt
.venv\Scripts\python tools\build.py
```

The game lands in `Locoland` next to the repository folder (`..\Locoland`; change with
`--target`; `--locofix <dll>` takes a prebuilt LocoFix.dll instead of compiling it and
`--captured <folder>` keeps the language captures elsewhere - both for the installer, see
"Release"; `--dev` adds the helpers `run_map.cmd` and `edit_map.cmd` of `game/`, which a build
without it and Setup leave out). Ghidra is looked for in `..\_tools\ghidra_*_PUBLIC` (or `--ghidra`). Run `Locoland.exe`. The
launcher picks the resolution, the mode (full screen unticked = window) and the language.
The choice is remembered. A map starts without the intro and the menu with
`Locoland.exe --test-map maps\l01\l01.map` in the game folder, in the editor with
`Locoland.exe --editor --test-map <map>` (the editor shortcut gives the template, which opens a
new empty map), or by a double-click on `run_map.cmd` / `edit_map.cmd` of a `--dev` build.

`build.py` copies only changed files and does not overwrite the high score tables, so it
can be run after every change in the repo. A file is left as it is when it has the same size and
date as its source, or the same content (Setup unpacks our files anew at every start, so their
dates are new each time); the patched and generated files (the patched scripts, `Locoland.exe`,
`objects.res`, the editor's images, the new maps) are made in memory or a temporary folder and
written only when they differ. An update with nothing new writes nothing but `logs\install.log`
and `OpenLocoland.json`.

The Steam copy has to be the original. `build.py` (and so Setup) stops when `Steam.exe` is not
the exe of 2003-10-07 that LocoFix is made for (its SHA-256 in `build.py`; the same exe for every
language) and asks to restore it in Steam (Properties > Installed Files > Verify integrity of
game files; Setup says it in its own languages). So it does, before anything is built, when a
file our patches change differs from the original (the targets of `game/**/*.patch` and of the
script patches of `lang/<code>/`, by their SHA-256 in `build.py`, the same in every language):
an old way to turn on the developer keys was to edit `maps\script\constants.lgc` in the Steam
folder, and the patch then stopped Setup with a Python error. A `#define KEYS_ENABLE` line added
there is left out by itself when the file without it is the original exactly - the launcher's
Cheats option does that job now. Of the Steam folder's root only the game's own
files and folders are used (`Steam.exe`, `objects.res`, `strings.ini`, `steam.cfg`, `cursores`,
`fonts`, `maps`, `music`, `vid`, `video`, `wav`): anything a player added there - another
DirectDraw wrapper (dgVoodoo2, DDrawCompat, cnc-ddraw), DxWnd, their settings - is ignored and
listed in the log, and such a file that an older build copied into the game folder is removed
(the same name and size, unless the build writes a file of that name itself).

**Languages.** Every language is a separate Steam depot with the whole game (about 180 MB
to download each). Steam ships six of them: English, German, Hungarian, Polish, Portuguese
and Russian. Steam installs one; the others can be downloaded next to it, without
touching the installed game, once - open the Steam console (`steam://open/console` in the
browser or Win+R) and enter the ones you want:

```
download_depot 352130 352131    English
download_depot 352130 352132    German
download_depot 352130 352133    Hungarian
download_depot 352130 352134    Polish
download_depot 352130 352135    Portuguese
download_depot 352130 352136    Russian
```

(The Steam name of each depot is `Locoland ENG/GER/HUN/POL/POR/RUS`. 352135 Portuguese -
Portugal is listed with exactly the size of 352131 English - 551,618,874 bytes on disk and
180,658,752 to download - so it is probably a copy of the English files; no `lang/pt`
until that is checked.) Steam
puts them in `steamapps\content\app_352130\depot_<id>\`. `build.py` captures the language
files of the installed game and of every downloaded depot into `captured/<code>/` (outside
git) and assembles the packs that have a capture (a fan translation such as Czech: its base's,
else any captured language - "Czech" below); the others are skipped with the command to
download them. The depot folders can be deleted after the build; keep `captured/`. A
checkout without `captured/` (a new worktree) uses the installer's captures in
`%LOCALAPPDATA%\OpenLocoland\captured` when there are any (`languages.CAPTURED`); `--captured`
of `build.py` and `check_lang.py` names another folder.
`python tools\languages.py capture [<code>] [--force]` captures the installed language by
hand (`--force` replaces an existing capture, e.g. after a Steam update).

For working on the engine, generate the local reverse-engineering output (not in git):
`.venv\Scripts\python tools\unpack.py` (`re/exe`, `re/data`) and
`.venv\Scripts\python tools\decompile.py` (`re/c`, needs Ghidra).

## Repository layout

| Folder | Contents |
|---|---|
| `game/` | Our files for the game folder (`Locoland.cfg`, `dgVoodoo.conf`, `maps/custom.lst` - the custom maps of the menu) and changes to original files as `<file>.patch` (`maps/logo.lgc.patch`, `maps/menu.lgc.patch`) - same structure as the game folder; `maps/script/cheats.lgc`, `unlock.lgc` and their `_on` variants are the launcher's options (LocoFix fix 30); `update.txt` is the launcher's notice when the update check cannot reach GitHub (fix 33). `objects.override/` would hold changed values of `objects.res` (see "Editing objects.res"), `maps.override/<map>/` changed objects of a map (`l11`: a rail of mission 11, `BUGS.md` F21) and `maps.new/<map>/` whole new maps (see "Editing maps and images"); `maps.new/template` (the empty template) and `maps.new/sandbox` (the one to play) at the moment, and `run_map.cmd` - a double-click at the game starts the sample map, `run_map <map>` any map; `edit_map.cmd` the same in editor mode (see "Editor mode"); these two are developer helpers, copied only by `build.py --dev`. |
| `docs/` | The player's guide for the release, in Polish and English only (the German, Hungarian and Russian players get the English one): `PRZEWODNIK.md` (Polish) and `GUIDE.md` (English) - installing, what is new in the game, the custom maps page, the map editor with a tutorial and its keys - and a few screenshots of the game in `docs/img/`. Version 0.2; keep the guides in step with the in-game help pages (`maps/edithelp*.txt`) and "Editor mode" below. |
| `lang/<code>/` | Our translation work for each language pack (`cs`, `de`, `en`, `hu`, `pl`, `ru`): `language.ini` (the launcher's name and labels, `codepage`, `base` for a translation Steam does not ship) and in `game/` the texts by kind, each assembled into its place in `<game>\lang\<code>\` (`languages.pack_path`): `game/strings.override.ini` -> `strings.ini` (changes to the captured file), `game/briefings/<map>/` -> `maps\<map>\` (the missions', POLIGON's and the template's texts), `game/menus/menu*.txt` -> `maps\` (the menu's pages), `game/editor/edithelp*.txt` -> `maps\` (the editor's help pages), `game/scripts/*.lgc.patch` -> `maps\script\` and `game/scripts/shiftb.txt.patch` -> `maps\` (the developer mode). A pack ships only what it changes; the rest comes from its capture. `lang/_template/` is the starting point of a new translation (a placeholder for every file and `README.md` notes per folder) and not a pack: `languages.packs()` leaves out folders starting with `_`, and no `README.md` is shipped. Each pack's texts are in its code page (CP1250/1251/1252); format details in `docs/TRANSLATING.md` and `tools/patch_text.py`. |
| `external/dgVoodoo2/` | dgVoodoo2 2.87.5 (`DDraw.dll`, `D3DImm.dll`, unmodified; credit in `NOTICE.md`). |
| `src/LocoFix/` | Game fixes in C++ - a library loaded by the exe before WinMain. |
| `tools/` | Game assembly, language capture, text/exe/data patches, LocoFix build, decompilation, data unpacking, editing of objects.res, maps and images, tests; the installer (`installer.py`) and the release (`release.py`, see "Release"). |
| `re/symbols.csv` | Names and descriptions of functions and variables in the exe - everything we learn about the code goes here. |
| `re/types.txt` | Struct types (fields at their offsets), vtables, types of globals and function prototypes. |

Local only (in `.gitignore`), made from the player's own copy:

| Folder | Contents |
|---|---|
| `captured/<code>/` | Original language files captured from Steam, one folder per language (`languages.py`). |
| `re/c/` | C code from Ghidra (`decompile.py`, with names from `symbols.csv` and types from `types.txt`; `types.h` lists the structs). |
| `re/exe/`, `re/data/` | Imports, strings, dialogs of the exe; objects and weapons from `objects.res` (`unpack.py`). |
| `build/` | Ghidra project, LocoFix build, test results, previews. |

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
window bigger than 1024x768).

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
the menu's headers) has capitals only.

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
`game/menus/` and `game/editor/`, and the developer mode texts as patches of `maps\shiftb.txt`
and four scripts in `game/scripts/`. `build.py` assembles each captured pack with those changes
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
of the English version - `strings.ini`, the menus' and the missions' texts. A pack like it names
its base in `language.ini` (`base=en` in `[language]`, `languages.pack_base`) and is marked
unofficial in the launcher's and Setup's lists (`[launcher] unofficial=` of the chosen language,
Setup's `TEXTS`): `build.py` assembles it from that language's capture with `lang/cs/` over it -
or, when the base is not captured (a player with the Polish Steam copy only), from another
captured language (`languages.build_base`: en, pt, de, pl, hu, ru, the first there is), with a
line in the build's log; `needs_base=1` in `[language]` would keep a pack to its own base, and
Setup would then list it greyed with that base's `download_depot` line. On another base the
fonts and the logo are that language's (ASCII draws the same with all of them; `check_lang.py`
checks a fan pack with every captured font set) and so are the files the pack does not have:
`maps\menu2.txt` and `menu3.txt` (empty in every version), `shiftb.txt` (the developer keys'
help, English in every version) and `l08\brief2.txt` / `l15\brief2.txt` (empty, only the Polish
version has them) - none shows anything of the base's language. What differs is `strings.ini`:
the Polish one leaves 80 keys empty that the English one fills with references
(`MenuGamer={strings:HeadPlayerName}:`, the tooltips `MenuVid455Dir*`, `MenuVid468Dir*`, units
`117={Units:115}` ...) or has other words (`MenuVid471Dir0=Zadania`), and German, Hungarian and
Russian a few too, so `strings.override.ini` sets all of these to the English values (84 keys)
and the pack reads the same on any base; `check_lang.py` reports such keys a fan pack leaves to
its base. `lang/cs/game/` holds the fan translation's texts converted
to what the game reads (ASCII, CRLF; it was UTF-8 with two files in CP1250, and its few Czech
letters, dashes and a Cyrillic O are written without marks: the English fonts it is drawn with
have Cyrillic in 0xC0-0xFF and no Czech letters, so `check_lang.py` would flag them), and
`strings.override.ini` its `strings.ini` on the English keys - its two renamed keys (`Penize`,
`Rychlost casu`) and the empty or English values completed in Czech -, our keys and the editor's,
in the fan translation's words (Tovarna, Opravne depo, Dul for VID 165, Mina for VID 86). The game
does not wrap a text, so the fan translation's long lines - it wrote paragraphs on one line - are
broken at the English files' widths (briefings, menu pages, win and loss 44 characters, objectives
40, the tutorial's scripts 54), the key columns of the controls pages (`menu41.txt`, which had lost
its ESC 7 / ESC 2, and `menu42.txt`) are aligned again, and mission 1's briefing, too long for one
page then, has a third page (`l01rief3.txt`, listed by the pack's own `l01rief.lst`); the
menu's and the missions' headers (`MenuHeader*`, `MapHeader*`, drawn with the capitals-only VID 3)
and the objectives (the messages' font VID 14, capitals only too) are in capitals.

Each pack's texts are bytes in its own code page, the one its fonts are drawn for:
`codepage=` in `[language]` of `language.ini` - 1250 Polish and Hungarian, 1251 Russian, 1252
English and German. LocoFix converts with it wherever text crosses into Windows (the editor's
menus, dialogs and message boxes are the W functions: `editor_text.cpp`, `language_codepage()`
of `language.cpp`), and the map editor writes a map's texts in it. `tools\check_lang.py` checks
our texts of every pack against its code page and against the captured fonts: that each
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

## Data formats

What the loading code shows (`ResFile` methods 0x436590-0x4370b0, `Objects_Load`,
`World_LoadVid`, `Vid_Read`, `Level_Load`, `Level_Save`, object `Action(ACT_SAVE/RESTORE)`),
checked against objects.res and all maps. All numbers are little-endian; the structs the
fields land in are in `re/types.txt`.

**Container** (`.res`, `.map`, `.vid`, demos):

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | `RES ` (`RIFF` is accepted too, without the record layer) |
| 4 | 4 | size of the rest (file length - 8) |
| 8 | 4 | type: `MAP `, `VID `, `DATA` (objects.res), `DEMO` |
| 12 | | chunks |

A chunk is a four-letter id, a size (bytes after these 8; the next chunk starts at the size
rounded up to even), then a header and records. Header: a dword `flags`; with bit 31 set two
dwords follow (`extra` - 0 in all game files - and the record count), with bit 31 clear (old
files) `flags` is the record count. Each record is a 4-byte size + data. An id may appear in
several chunks; reading record by record (`ResFile_NextRecord`) walks into the next chunk of
the same id, and the order of the chunks matters because the loaders search forward. In all
game files every chunk has the bit-31 header, the size in the container header counts the
chunks without their pad bytes, and the last chunk is not padded.

**objects.res** (type `DATA`): `OBJ ` 687 records, `CNST` one record of 26 ints, `WEAP` 55
records of 0x44 bytes, `SFX ` 164 records (one byte, not identified, + 8 strings with
alternative `wav\...` files). `Objects_Load` reads WEAP into `World.weapons`, then each OBJ
record through `World_LoadVid` and `Vid_Read`. OBJ record (strings end with 0; offsets after
the name count from the byte after its 0):

| Offset | Size | Field (`Vid` offset) | Meaning |
|---|---|---|---|
| 0 | 4 | nvid | object number, < 0x800 |
| 4 | var | name (+0x08) | CP1251 name |
| +0x00 | 4 | spriteType (+0x0c) | `U_...` flags |
| +0x04 | 4 | spriteClass (+0x10) | `B_...` class; 8 = decoration baked into the ground |
| +0x08 | 4 | flags (+0x14) | 0x400 hidden, 0x800 screen coordinates, 0x10000 bobbing... |
| +0x0c | 4 | moveMask (+0x18) | `VID_MOVE_MASK` |
| +0x10 | 3x4 | sizeX, sizeY, sizeZ (+0x1c) | float, pixels |
| +0x1c | 4 | maxHp (+0x28) | |
| +0x20 | 4 | speed (+0x2c) | float, px/s (stored x0.001); 999999 = none |
| +0x24 | 4 | speedZ (+0x30) | float (stored x0.001) |
| +0x28 | 2x4 | acceleration, deceleration (+0x34) | float (stored x1e-6); 999999 = instant |
| +0x30 | 4 | rotation time (+0x3c) | float, ms per full turn (stored as 256/x) |
| +0x34 | 4 | weaponIndex (+0x40) | WEAP record |
| +0x38 | 4 | damageRadius (+0x44) | float |
| +0x3c | 4 | damage (+0x48) | done around on death |
| +0x40 | 3x4 | link offset x, y, z (+0x4c) | float, position of the linked object (turret...) |
| +0x4c | 4 | linkNvid (+0x58) | linked VID (`VID_LINK`) |
| +0x50 | 4 | altitude (+0x60) | float: flight height / max step / bobbing amplitude |
| +0x54 | 28 | - | skipped, always zero |
| +0x70 | 4 | nDir (+0x64) | number of directions |
| +0x74 | 17x4 | animFrames (+0x68) | frames per direction of each animation (`ANI_...`) |
| +0xb8 | 17x4 | animSfx (+0xac) | sound per animation |
| +0xfc | 3x17x4 | childForward, childSide, childZ (+0xf0) | float spawn offsets per animation |
| +0x1c8 | 17x4 | childNvid (+0x1bc) | object created with each animation (`VID_CHILD`) |
| +0x20c | 17x4 | childCount (+0x244) | how many (`VID_NO_CHILD`) |
| +0x250 | 4x4 | gamma R, G, B, A | -255..255 colour shift (+0x288) |
| +0x260 | 3x4 | scale x, y, z (+0x290) | float, 1.0 for almost all |
| +0x26c | var | imageFile (+0x29c) | e.g. `vid\005.vid`; `.bmp`/`.tga` are converted to a temporary .vid |

Objects with the same image file share it (`Clone`). The image (`.vid`, type `VID `) has
`HEAD` (one record of 10 bytes: u16 imageFlags, u16 frameSpeed in ms, s16 frameCount, s16
width, s16 height), then chunks by the VID class that imageFlags pick (the 545 images of the
game: 368 sprites with a palette, 32 without, 137 texture images, 8 colour boxes):

- 0x80 colour boxes (`ColorVid`): `DATA`, one record with a uint ARGB colour per frame.
- 0x20 texture tiles (`GroundVid` - map grounds, cursors, many effects): `SURF`, one record:
  short texture count, then per texture short width, short height, (with imageFlags 8 a
  0x300-byte palette - no file has it), int size and the pixels, 2 bytes each: A4R4G4B4 when
  imageFlags has both 1 and 2, otherwise R5G6B5; with imageFlags 4 a height texture follows
  each colour one (int size + 16-bit heights, same size, counted in the texture count, so
  colour textures are the even ones). `DATA`, one record with the tiles: an unknown int
  (uninitialised memory in most files), texture (index into the textures), offsetX, offsetY,
  width, height (the part of the texture), x, y (where it goes in the frame), next (the
  next tile of the same frame, 0 = last) - ints with imageFlags 0x10 (`GroundTile` as in
  memory), otherwise the int and 8 shorts. Frame f starts at tile f. Most images keep many
  frames in one texture (atlases); a map ground (imageFlags 0x35 or 0x25, one frame) is a
  grid of 256x256 textures listed column by column (tile 1 is below tile 0), each tile
  showing the whole texture its texture field names (the colour textures are the even
  ones, so field 2k is the k-th picture). The original converter sorted them: in every
  ground of the game (l01, l02, l05, l08) tile 0 shows the picture `count / 2` (its field
  is the number of tiles) and tile count / 2 the picture 0; `map_tool.py` follows the
  fields both ways. The ground image is drawn centred on the ground object, which stands
  in the middle of the map (512, 384 in l01, 1024, 768 in l05). The loader would look for a
  `SHAD` chunk next;
  no file has one.
- otherwise software RLE sprites (`SpriteVid`, 16- or 32-bit by the screen depth): with
  imageFlags 8 `PAL ` (256 R, G, B triplets, or with imageFlags 0x10 256 B, G, R, A), then
  `DATA` with one record per frame.

A sprite frame (one `DATA` record) is an int size of the rest, short n, n shadow points (3
shorts x, y, z), short top, short height, then each row from top to top + height - 1: runs
of byte skip, byte count and count pixels, ended by two zero bytes. A pixel is a palette
byte with imageFlags 8 - with imageFlags 4 as well the run holds count shorts of height
first, then the count palette bytes - and an R5G6B5 short without a palette. The game's
encoder was canonical: a run covers every drawn pixel up to the next gap, more than 255
pixels continue in runs with skip 0, a skip over 255 is written as (255, 0) runs, the first
and last row hold drawn pixels and an empty frame is top 0, height 0 - so the pixels and a
mask give the same bytes back (all 11264 frames). A record with size 2 holds the index of
an earlier frame, which is reused (2311 frames); one frame of `vid\344.vid` is cut short.
Frames are ordered by animation (`ANI_...`), then direction, then animation step: frame =
start of the animation + direction x frames per direction + step, where direction =
((angle + 128 / nDir) & 255) x nDir / 256.

WEAP record (0x44 bytes, `Weapon`): 0x00 kind (0/4/8/12/32, unclear), 0x04 flags, for train
cars three floats - 0x08 length (px between the car's front and rear on the rails, 48), 0x0c
mass, 0x10 power (a car with power - mass > 1 is a locomotive; see "Trains and rails") - 0x14
detect range, 0x18 weapon range (0 = none), 0x1c aim (floats),
0x20 reload time ms (guess), 0x24 build time ms, 0x28 life time ms, 0x2c ammo (999999 =
unlimited), 0x30 army of created objects (guess), 0x34 initial `BEH_...` (guess), 0x38 icon
frame in `vid\455icon.vid`, 0x3c target priority (guess), 0x40 float (unknown).

**Editing objects.res** (`tools\res_tool.py`). `unpack <objects.res> <folder>` writes the
chunks as TSV tables (UTF-8, tab-separated, a header row; lines starting with `#` are
comments) and `pack <folder> <objects.res>` builds the file again. Unpack checks that the
tables pack back to the same bytes and refuses otherwise (the Steam file and the one with
the sound fix both round-trip). Columns are the record fields in file order, named as in
`re/types.txt`, with the values as stored in the file (the OBJ table above: speed in px/s,
rotateTime in ms per turn...): ints in decimal, bit fields in hex (`0x...`; decimal is
accepted too), floats in the shortest form that keeps the exact value, `spriteClass` as the
`B_...` name without `B_` (`UNIT`; a number for the classes export.lgc does not name, e.g. 8).

| Table | Rows | Columns |
|---|---|---|
| `objects.tsv` | OBJ, one per object type, in file order (687) | nvid, name, spriteType, spriteClass, flags, moveMask, sizeX, sizeY, sizeZ, maxHp, speed, speedZ, acceleration, deceleration, rotateTime, weaponIndex, damageRadius, damage, linkOffsetX/Y/Z, linkNvid, altitude, unknown_54 (the 28 skipped bytes in hex, empty when zero), nDir, gammaR/G/B/A, scaleX/Y/Z, imageFile |
| `animations.tsv` | the 17-entry arrays of OBJ: one row per object and animation, only those not all zero (10993) | nvid, anim (`ANI_...` without `ANI_`: STAND, STOP_MOVE, GO, START_MOVE, L_ROTATE, R_ROTATE, OPEN, CLOSE, FIGHT, SALUT, STAND_OPEN, LOAD, UNLOAD, WOUND, BIRTH, DEATH, DEATH2; or 0-16), frames, sfx, childForward, childSide, childZ, childNvid, childCount |
| `weapons.tsv` | WEAP (55) | index, kind, flags, length, mass, power, detectRange, battleRange, aim, reloadTime, buildTime, lifeTime, ammo, army, behave, iconFrame, priority, unknown_40 |
| `constants.tsv` | CNST: 26 ints, read into `Options` (`g_options`) by `Options_LoadConstants`, which scales 0, 1, 6, 7, 24 by 0.001 and 2, 3 by 1e-6. Known: 0/1 scroll speed, 3 gravity of shells, 4 hp the repair car gives per second, 5 ammo the arsenal gives per second, 6 speed limit on a broken rail, 9 factory build time unit (guess), 10 unused (DebugMode comes from the .cfg), 13 mouse tip delay ms, 14/15 hp per second regenerated by factories/buildings, 17 time to lay a mine ms, 19 ammo given by a balloon, 24 speed above which collisions and hard stops damage trains | index, value |
| `sounds.tsv` | SFX (164) | index, param (the unidentified byte), file1 ... file8 |

The tables are made from the game's data, so like `re/data` they stay local (unpack them
into `build\`). The repository keeps only changed values, in `game/objects.override/`: TSV
files named like the tables, each with the key columns (`nvid`; `nvid` and `anim`;
`index`) and only the rows and columns that change. An empty cell keeps the value. A row
with an nvid that objects.res does not have adds an object type (the columns it leaves out
are 0 or empty, so it needs at least nDir, the animation frames in `animations.tsv` and an
image; `objects.tsv` is applied first, so `animations.tsv` can give the new type its frames).
VID 938, the side tiles of the state bar, and VID 939, the icons of the editor's bar, are added that way (`game/objects.override`). A row
with the next free index adds a weapon or a sound. `build.py` applies the folder after the
sound fix and prints how many values changed (`res_tool.py
apply <objects.res> <folder> <output>` does the same by hand); an unknown table, column or
key, or a bad value, stops the build with the file and line. For example
`game/objects.override/objects.tsv`:

```
# the small locomotive: more hit points, faster
nvid	maxHp	speed
5	150	200
```

**Map** (`.map`, type `MAP `) - read by `Level_Load`, written by `Level_Save`, chunks in this
order:

| Chunk | Records | Content |
|---|---|---|
| `OBJ ` | 1 | Level-local VIDs as in objects.res (`Objects_Load`). In every mission it is VID 1024, the ground, whose image is `maps\lNN\lNN.vid`; the record goes on after the image file name with the rest of the editor's buffer (uninitialised memory, 816 bytes in l01). |
| `GRPH` | 1 | 20 bytes (`Graphics_ReadMapSettings`): int environment flags (`ENV_...`), 8 bytes gamma, int wind direction and float wind (script `SetWind`). Its presence marks the current format. |
| `HEAD` | 1 | 24 bytes: float width, height, cameraX, cameraY (top-left of the first view), uint time (ignored; the clock restarts at 10 ms; the shipped maps hold the time of the authors' editor session, LocoFix saves 0), int version (10; before 10 the four values were ints). |
| `GRID` | 1 | short ground height per 8x8 pixel cell, row-major, (width/8) x (height/8). Missing or wrong size -> flat. |
| `SPR ` | 1 | Objects, 28 bytes each, until id -1 (`Level_ReadSpr`): int id, int nvid, float x, y, z (ints before version 10), int direction (0-255, 0x40 = quarter turn), int army. Objects are created in this order; the first is the ground. |
| `SPRD` | n | One record per object: int id + what the object's `Action(ACT_RESTORE)` reads; a last record with id -1. |
| `PLAY` | 1 | 4 ints: the Flagman id of each army (0 = none). |
| `GROU` | 1 | Unit groups: member ids and -1 for each group, then a final -1. |

Ids are the object addresses in the editor session that saved the map; while loading they
are mapped to the new objects (`IdMap`, World +0x1e4), so references in commands survive.
Saving skips objects linked to another one (`uplink`: turrets, smoke - their parent recreates
them) and menu objects. SPRD data by class (what `ACT_SAVE` writes):

| Class | Data after the id |
|---|---|
| every object | command count (4) + commands of 16 bytes `act, var1, var2, var3` (`ACT_...`; a stack, the last entry runs first) |
| Unit and derived (classes UNIT, BUILDING, AVIA, CIV_ROBOT, ENGINE, DEPO, CREATURE, BALLOON) | + behave (4, `BEH_...`) + item count (4) + items (2 bytes each, nvids; the factories list what they build) |
| Engine | Unit data + front and rear rail position (16 bytes each: the point's x, y, z and the next point's x, y, z as shorts, distance << 16; `RailPos_Save`) + the ids of the car in front and behind |
| Text | base + behave (4) + source text (zero-terminated) |
| Region | base + 80 bytes (sizes, colours, VID, 6 pairs of object ids) |

In the shipped maps the records are 4 bytes for terrain, objects, rails, decals and plain
sprites, 12 for units, 52 for engines (68 with one command), 84 for regions. After the chunks
`Level_Load` compiles `<map name>.lgc`, binds the script functions `F<nvid>_<ANI>` and
`F<nvid>_DESTROY` (VID event handlers, `Vid.animScript`) and the engine events (`main`,
`TrainNotAmmo`...), and runs the `ANI_BIRTH` handler of every object.

Old format (no `GRPH`, version 6: `menu.map` and `logo.map`, both without objects): `HEAD` =
int width, height, short cameraX, cameraY, uint time (used), int version, then int, 4 gamma
bytes, byte, short (the record has 16 more bytes, not read); `SPR ` entries of 14 bytes (int
id, short nvid, x, y, z, byte direction, byte); `SPRD` read with action 200; `PLAY` (33
bytes in both files) and `GROU` not read.

**Demo** (type `DEMO`): recorded after script `SaveDemo(file)` for the next map loaded,
played when `Level_Load` is given a demo. One record: map file name, uint random seed, uint
start time, then per frame uint game time + the 28-byte mouse state (World +0x230); -1 ends
it. No demos are shipped.

**Editing maps and images** (`tools\map_tool.py`). `unpack <file> <folder>` turns a `.map` or
a `.vid` into text, TSV and PNG files, `pack <folder> <file>` builds the file again, and
`check <game folder>` unpacks and packs every map and image of a game through the disk and
compares the results: all 23 maps and 545 images of the game come back byte for byte (the
Steam folder and the assembled game, about 4 s). TSV files are as in res_tool.py (UTF-8,
tab-separated, `#` comments, floats in the shortest exact form); anything the tool does not
read stays in hex, so nothing is lost. A map unpacks to:

| File | Content |
|---|---|
| `map.json` | the chunk order, `graphics` (GRPH: environment flags, gamma bytes, wind direction, wind), `head` (HEAD: width, height, cameraX, cameraY, time, version; the old format adds the unread bytes as `rest`), `players` (PLAY) and `raw`: the records of chunks the tool does not read, in hex (the old-format PLAY) |
| `objects.tsv` | SPR + SPRD, one row per object in file order: id, nvid, name (from objects.res - only for reading, pack ignores it), x, y, z, direction, army, commands (`act,var1,var2,var3` separated by spaces), behave and items (the Unit data; both empty for classes without it), data (the rest of the SPRD record in hex: engine rail positions, region data) |
| `groups.txt` | GROU, one group per line: the member ids |
| `grid.txt` | GRID, one row of 8x8 cells per line: the ground heights |
| `vids.tsv`, `vid_animations.tsv` | OBJ, the level's own VIDs, columns as `objects.tsv`/`animations.tsv` of res_tool.py plus `trailing` (the bytes after the image file name, hex) |

The object classes that decide the SPRD layout come from objects.res: the one of the game the
map belongs to (found up to three folders above it) or `--objects <objects.res>`. An image
unpacks to `vid.json` (the chunk order, HEAD, the kind and how the files are laid out) and:

| Kind | Files |
|---|---|
| sprite | `frames.png`: every frame in a cell of a sheet (16 columns, cells of the HEAD size, frame i at column i % 16, row i / 16), `frames.tsv`: frame, reference (the frame it reuses; its cell stays empty), shadow (the points as `x,y,z` separated by spaces), raw (a record that is not a whole frame, in hex). With a palette `frames.png` is an indexed image of the palette indices and `palette.tsv` the palette (index, red, green, blue, alpha with imageFlags 0x10; it counts, not the PNG palette); undrawn pixels have `transparentIndex` (an index no frame uses), or, when a sprite uses all 256 (46 files), `mask.png` marks the drawn pixels. `depth.png` (imageFlags 4) holds the 16-bit heights. Without a palette `frames.png` is RGBA (alpha 0 = undrawn). Top, height and the runs are computed from the drawn pixels. |
| texture | `texture_NNN.png` per colour texture (RGBA for A4R4G4B4, RGB for R5G6B5) and `height_NNN.png` (imageFlags 4, 16-bit), or for a map ground one `ground.png` and `height.png` of the whole map, cut back into the textures by the tiles; `tiles.tsv`: tile, unknown_00 (hex), texture, offsetX, offsetY, width, height, x, y, next. The textures of a ground come in pairs - the picture and its height - so the tile of the picture k is the texture 2k (with the wrong number the game cannot blit the height into the z-buffer) |
| color | `colors.tsv`: frame, color (0xAARRGGBB) |

Colours keep their exact values: the 5-, 6- and 4-bit channels are widened to 8 bits by
repeating their top bits, and packing takes the top bits again, so a colour painted in an
editor is rounded down to what the format holds. Indexed images must stay indexed.

The repository keeps changed objects of a map in `game/maps.override/<map name>/objects.tsv`
(`l01` for `maps\l01\l01.map`, `menu` for `maps\menu.map`): the key column id and only the
rows and columns that change, an empty cell keeps the value. A row with an id the map does
not have adds an object (any unused number; it needs nvid, the columns left out are 0 or
empty, behave 0 for the classes with Unit data - engines and regions also need `data`, copy
it from a similar object), a column `remove` with 1 deletes the object. `build.py` writes
such maps into the game from the Steam file with the changes and prints how many values
changed (`map_tool.py apply <map> <folder> <output>` by hand); a bad column, key or value
stops the build. For example (tried in mission 1: the tower left the palace and stood on
the square, the tree grew on the maze):

```
# a tower of the palace moved to the square, a new round tree on the maze
id	nvid	x	y	z
10180224		420	600	70
1	923	520	380	62
```

z is the ground height under the object: the `grid.txt` value of the cell (x / 8, y / 8).

**New maps** (`map_tool.py new <folder> <template.map> [--vid <image>] [--script]`). A map
the game does not have starts as a template: the terrain of an existing map - its ground
object (VID 1024) with the image, GRPH, HEAD and GRID - without the other objects, the groups
and the players; `--script` also writes `<folder name>.lgc`, a mission script that compiles
and runs with the standard scripts (`InitDefault`, the game tactics and the menu) - a whole
mission like the sandbox, with the map editor's blocks for its settings and push lines (see
"Editor mode": `CUSTOM_MAP`, its own texts, the reinforcement, the win and loss checks) - and
its texts (`objectives.txt`, `win.txt`, `loose.txt`, `brief.txt`, `brief.lst`), and `--vid`
points the ground at another image. The script declares the arrays the tactics read
(`start_trains[]={-1}` and the other `start_trains_*`, before `header.lgc`): without them the
defaults of `TrainsTactFunctions.lgc` are 0 and `TrainsTact()` puts an AI train at (0, 0)
(`BUGS.md` F10). The template map shares the ground image of the map it
came from (here `maps\l01\l01.vid`), so the terrain looks the same until a `.vid` of its own
is packed into the game. `--size 2048x1536` makes the map bigger: the ground height of the
cells is flat then, the camera looks at the middle of the map and the ground object moves
to the middle too - the ground image is drawn centred on that object (l01 has it at 512,
384, l05 at 1024, 768), so left at the template's place the picture would be drawn off by
the difference and part of the map would have no ground at all.

**A map to play** - the way the missions do it. `map_tool.py copy <folder> <map> <nvid>` adds
every object of one VID of another map, so the template map takes the rails of mission 1 with
`copy game/maps.new/<name> maps\l01\l01.map 610 --objects <game>\objects.res`. `track` lays a
new line of rails instead: each step of the path must be one step of the rail lattice
(71 px in x and 50 px in y, or their halves) and `--clear` removes the old rails first. The
command validates the geometry the engine needs: each rail must join the one before it the
way `Rail_Action` links rails (see "Trains and rails") - its end at the joint pointing the
opposite way - so a straight run turns only through a curve, e.g. `track
game/maps.new/<name> 70.5,750 141.5,750 212.5,725 283.5,675` (a horizontal rail, a curve, a
diagonal); a horizontal rail followed straight by a diagonal does not link in the game and
is refused. The first and the last point join the rails already there by the same rule (and a
rail that is already there, the same shape at the same place, is refused too), so
a branch starts from a track (`track <folder> 425.5,575 496.5,550 567.5,550` leaves a
diagonal through a curve); a track that goes on past the map edge has negative points, which
go after the options and `--` (`track --z 66 --objects <res> -- <folder> -213.5,1375 ...`;
`--z` because there is no ground under the first rail; a track that continues a rail takes the
height of that rail's point, so it links to a ramp's raised end too). Consecutive steps may not turn sharper than 45 degrees (trains
would reverse at the node), and no rail point may get more than six links, the rails already
there counted (the engine's "Too many links" limit; the rail ends pointing the same way at a
joint share one point, so the limit is per point, not per joint - `rail_point_links`). Every rule
counts the ramps (the rail frames 12-35, one point 11 higher - `rail_of`) and the heights of the
rail points: two rails link only where their points are at the same height. `check-track <map>`
looks at every rail of a map the way the engine links them (checked against the engine's own
links: every point of the 20 missions): a rail laid twice, a rail point with more than six
links, and a joint where several rails end but a rail end has no rail ending the opposite way at
its height (a dead end of two rails, or rails at different heights) - the sandbox and the
editor's tracks pass it; the missions have a few (BUGS.md, Quirks). Every grid cell crossed by a rail
is flattened to the track height and the one-cell buffer around it is ramped toward it,
so there are no cliffs under the track -- the rail sits 4 px below the ground surface,
as in the game's own maps. Objects go in with
`place <folder> <nvid> <x> <y> --from <a mission map> --objects <game>\objects.res` -- it
copies the object's state (army, behave, items) from the reference map so the level starts.

`game/maps.new/sandbox` is such a map - 2048x1536, a flat ground painted for it (meadow,
gravel under the track, packed earth under the bases) and a track laid out the way mission 5
does it: a factory stands at the dead end of a straight spur that meets the line through a Y
(two curves each way), with a push line from the factory down the spur, and a repair point
stands on a track that trains drive through. The main line runs along the bottom from the
west edge, up the east side and west along the top. The player's base is in the south-west:
the main line comes from past the west edge of the map and goes back past it at the top,
as in the campaign: the reinforcement (VID 5 army 3 locomotive outside the map, `BUGS.md` F12)
is pushed onto the map by the level-entry push line, the level exit lets no train off the map,
and semaphores mark both and the factory exits - red for the way out of the map and into the
factories, green for the way in (checked in the game: the four red posts run animation 10). The factory (direction 128, exit south)
stands at the top of a spur north of the line and the repair point on the west leg of a loop
next to it. Every spur and loop leg meets the line through a Y, so trains reach the loops and
leave them both ways, and the repair point legs are one way, as in mission 5 (north through the
player's, south through the enemy's), with semaphores at their ends. The east line has a passing loop; the enemy factory (direction 0, exit north)
hangs on a spur below the top line, the enemy repair point on the west leg of a loop hanging
from the top line (a Y at each leg, like the player's), and a fort with
two guns stands on the east line below the corner - a gate the trains drive through, so the
player has to break through it to reach the enemy base and destroy it. The enemy (army 1)
patrols the east line and the bottom line with trains (`start_trains`, `TrainsTact`,
`SetCleverEnemyAttack`), more gun trains come from its factory over time (`level_queue_*`),
and the player gets a second reinforcement wave when its train is gone. The queue
(`DepoFree` in `ai_events.lgc`) builds an entry when the enemy factory is free, its time has
passed and fewer than `LIMIT_ATACKERS` trains attack, after rebuilding lost start trains -
so the times are the earliest ones: in a 10-minute game the trains of 240 and 420 s left the
factory at 280 and 456 s. Push lines and patrol
points in the script are screen coordinates (world y minus the 76 of the rail points:
`RailMap_FindNearestDot`), `start_trains_x/y` too (the tactics add the ground height), but
`Action(train, ACT_MOVE, x, y)` of a script takes the world position (`Engine_MoveToPoint`
with screen 0, a 3D search; only the mouse passes screen 1). `#define
SANDBOX_DRIVE_TEST 1` in the script turns the enemy trains off and drives the player's
trains through every part of the track, logging their positions to `error.log` ("DRIVE").

Buildings have 8 directions (nDir 8 for the factory, the repair point and the fort): the shipped
maps turn a factory so that its direction is the way out (0 north, 64 east, 128 south, 192
west), a repair point on its track along it, and a fort is a gate: it stands on the track (0-6
px from it in every shipped map, l10 ... l20), turned along it (0/128 on a vertical track,
64/192 on a horizontal one, 32/96/160/224 on a diagonal), and its guns stand on its wall - at
x +-47, z +92/+93 for direction 0 or 128, at (-32, -24) and (+33, +23), z +93 for 32 or
160. A fort cannot be destroyed: it is a static object without hit points (`U_OBJECT`,
maxHp 0); its launchers (VID 115) are the units. The usual win check `!NoArmyUnits(1)` counts
the VIDs of `ARMY1_NVID_ARRAY` (`constants.lgc`: the trains' cars, the factory 102, the
launchers 115 ...) - not the fort and not the repair point 104.

The main line of a mission does not end on the map: its rails go on past the edge (17 of the
20 missions have rails outside the map), and the point locomotive of the reinforcement (VID 5,
army 3) usually stands out there (l02 at -107, 1327). The script gives the line two push lines
(see "Trains and rails"), the **level entry** - from the point outside the map onto it, with
push_flag 1, so the arriving train is pushed in (l02 `SetPushLine(-107,1203,54,1465,1)`) - and
the **level exit** at the other end, short and pointing into the map with push_flag 0
(l05 `SetPushLine(1952,35,1952,60,0)`), so no train may drive off the map into the hidden end
of the track. **Semaphores** (VID 152 `семафор`, an `OBJECT` with 8 directions, army 0, on the
ground) are posts 16-29 px beside the track where such one-way sections are: a pair either side
of the track at the level entry and at the exit, one at the factory exits and at both ends of the
repair point lines (l05: the pair (82, 1545) and (129, 1544) by the entry, (485, 1198) by the
player's factory line), turned along the track, red or green (below). They only show the way:
no script refers to VID 152, and the semaphores of the rail network are something else - `SetSemaphore(x, y, value, army)`
(231) sets `RailDot.semaphore` of the two nearest points, and 4 + army on both ends of a link
closes it for that army (`RailDot_CanPass`, `Train_Update` stops in front of it). The scripts
use it for mines: an armed mine (VID 86, once the repair car that laid it has moved away)
closes its point for its own army, so its trains do not run over it, and its destruction
opens it again (`F086_0`, `F086_DESTROY` in `ai_default_event.lgc`); mission 3 has calls for
its entry, exit, factories and repair points written and commented out. The post has a green state
(`ANI_STAND`, the default) and a red one (`ANI_STAND_OPEN`), with its light (VID 154 halo,
VID 155 spot: green and red); no code ties it to the rails - the map's author sets it: a red
post has the command `10,0,0,0` (act 10 = `ANI_STAND_OPEN`, an animation run as the first
command when the level loads), and 126 of the 188 posts of the missions have it. A post faces
the trains it is for (its direction points at them) and is red where the push line forbids
their way, green where it lets them go: l05's entry pair is red for the trains heading off the
map and green for those coming in, the posts of the factories are red for the trains that would
drive into them, a repair point line has a green post at its way in and a red one for the way
back. Of the 196 posts of the 20 missions, 192 stand within 90 px of an end of a push line
named in the script (the entries 50, the exits 55, the repair points 49 and the factories 29). VID 831 (`указатель стрелки`) is a decorative
switch sign about 50 px from a junction (6 missions), with no code either.

The mission has its own texts: `objectives.txt` (shown at the start, instead of mission 1's
two lines - the script sets `OBJECTIVES_FILE`, `WIN_FILE` and `LOOSE_FILE`, small
backward-compatible hooks in `basefunctions.lgc` and `menuend.lgc`), `win.txt` and
`loose.txt`, and the briefing of the Esc menu: `BRIEF_LIST` names the pages (`brief.lst`:
`brief.txt`, `brief2.txt` with hints, `hiscores.txt`; in the pack's code page, at most 44
characters a line, CRLF; English at the root of a map of the repository and the other
languages in `lang/<code>/game/briefings/<map>/`, see "Languages") and `BRIEF_HEADER` the title (ASCII - the script is CP1251), hooks in `menutact.lgc`.
Without them a map shows the briefing and the header of mission `LEVEL + 1`. `HISCORES_FILE`
(hooks in `menuend.lgc`) keeps the map's high score table (`hiscores`, written on a win) and
its page (`hiscores.txt`) in its own folder - without it a win went into mission 1's table
(`maps\l01\hiscores`). `build.py` copies a `hiscores.txt` of `maps.new` only while the game
has not written one. It is won when no enemy unit remains (`!NoArmyUnits(1)`) and lost when the
player has no units and no factory (`IsNoDepo()`). `test_game.ps1 -Map maps\<name>\<name>.map`
starts it in a window (LocoFix's test map, `--test-map`), and `Locoland.exe --test-map
maps\<name>\<name>.map` in the game folder starts it the same way by hand, after the launcher
(window or full screen as chosen there); in a `build.py --dev` build `run_map.cmd` does that too:
a double-click starts the sample, `run_map maps\<name>\<name>.map` any map.

**The custom maps page** (`game/maps/menu.lgc.patch`). The strategic map of the main menu has
a second page, "WLASNE MAPY" / "CUSTOM MAPS": the page buttons of the briefings (VID 476 next,
VID 475 previous; also the arrow keys) lead from the map of the campaign to it and back. It
uses only graphics of the game: the empty picture with the button bar (VID 484, like the help
pages) and, for each entry, the lamp of the in-game strategic map (VID 472, the mission dots
VID 468 would show the name of a campaign mission as their mouse tip) with its title in the
menu font; a click on a row (lamp or title) picks it. "EDYTOR MAP" / "MAP EDITOR" (keys
`MenuHeader7` and `MenuCustomEditor` of `strings.ini`; without them the English text is shown) is
the first row, and under it six rows of the list of maps, which has any length and scrolls: the
up and down arrows beside it (the arrows of the player list's difficulty, VID 447/448, as types of
their own, VIDs 940/941 in `game/objects.override`, so their mouse tips say "Przewin liste w
gore/dol" - `MenuVid940AllDir`/`MenuVid941AllDir` - and not the difficulty's; each shows only when
the list goes on that way), the mouse wheel (LocoFix's script function `GetMouseWheel()`, extern
201, `menu_input.cpp`: the engine keeps the wheel's turn in the mouse state but no script function
read it), the up and down keys and Page Up / Page Down. The entries come from `maps\custom.lst`
(`game/maps/custom.lst`, ASCII, CRLF), two lines per map - its folder in `maps\` and its title
- so a map is added without changing the script (the script language cannot list a folder;
the map editor adds the maps it saves, and `build.py` keeps those lines when it writes the
repository's list into the game). A map shows its
briefing pages (`maps\<folder>\brief.lst`) under its title, with a previous button back to the
list on the first page, and the start button loads `maps\<folder>\<folder>.map` the way a
campaign mission starts. "EDYTOR MAP" calls `StartEditor(map)` in `menu.lgc`, the one place the
menu starts the editor from: fade, music off, then LocoFix's `StartEditorMode` (extern 200)
starts the game again in editor mode on `maps\template\template.map`, which opens as a new empty
map (see "Editor mode", "The map the editor starts on"). Each map row has an "edit the map" icon
right of its title (VID 945, `Custom maps edit`: the hammer and the wrench of the state bar's atlas,
VID 455 frame 2, as the page's green glyph, made by `patch_data.make_custom_edit_icon` into
`vid\945edit.vid`; dim, lit under the mouse as the lamps are - STOP_MOVE / START_MOVE -, its mouse
tip "Edytuj mape" / "Edit the map", `MenuVid945AllDir`), which starts the editor on that row's
`maps\<folder>\<folder>.map` - POLIGON too, which the editor then saves only under a name of its
own. Its column is not part of the row: a click there is the icon's (`MenuVid()` 945, the row by
its y), the rest of the row still shows the briefing.

`CUSTOM_MAP` (the map's own file, `#define CUSTOM_MAP "maps\\sandbox\\sandbox.map"` before
`header.lgc`) marks a map that is not a campaign mission, hooks in `menuend.lgc` (the root and
the `lang/pl`, `lang/en` versions), `menutact.lgc` and `initdefault.lgc`: a win does not
unlock the next mission (`l<player>`), does not write the mission score `s<player>l<LEVEL>`,
the rank or the hall of fame (`maps\hiscores`) - the end screen shows the map's score, its own
high score table (`HISCORES_FILE`) and the player's unchanged total; "restart the mission" in
the Esc menu loads the map itself instead of `maps\l<LEVEL+1>`; and `InitDefault` sets the
menu's `return` flag to 2 instead of 1, so the main menu comes back on the custom maps page
(an older menu reads 2 as 1). The sandbox and the template define it. Checked in the game
through the menu: the page, POLIGON's three briefing pages and back, the sandbox loading from
its start button and its script compiling, the restart in its Esc menu, "EDYTOR MAP" loading
the template, mission 1 starting from the first page after a visit to the second, and the
page opening at the menu's start when the flag matches, and - with the player data in
HKCU (`BUGS.md` F15) - the return: after the sandbox, "Wyjdz do menu glownego" of its Esc menu
opens the main menu on the custom maps page, and so does a new start of the game straight
into `maps\menu.map` after a run of the sandbox (the flag 2 read from the registry).

**The ground of a map** (`map_tool.py ground <folder> <width> <height>`). The ground of a
map is a picture cut into 256x256 tiles (`ground.png`, its height `height.png` and the
tiles in `tiles.tsv`); the command writes an empty one of the given size, so a map can
have a terrain of its own: paint the picture, then point the map at it with `--vid`. The
tiles of a ground point at the textures in pairs - the picture and its height - so the
tile of the picture k is the texture 2k.

The height of the picture is **1026 + 8 a GRID level** (`map_tool.picture_height`: l02 holds
1025-1028 at GRID 0, l01 1586 at 70; the editor's `picture_height` in `editor_ground.cpp` and
`FLAT_PICTURE_HEIGHT` of `editor_newmap.cpp` are the same). The engine blits it into the depth
buffer and draws every object and its shadow against it, so it has to match the map's GRID:
`ground` writes a flat ground at 70 (1586), the level of a new map. Until 2026-10-28 it wrote 0,
and POLIGON (made with it, GRID 70) had a height of 0 under the whole map - its objects stood
~1500 above the ground the engine saw, their shadows detached from them (the "floating" the
user saw in the editor); its `height.png` is 1586 now. The missions set many objects a few px
below their GRID (factories 9-11, repair points 6-8, semaphores 0-5); setting POLIGON's the same
way was tried and dropped: with the height right, its objects at the GRID's height look as
grounded as the missions' (their shadows attached, side by side with missions 2, 5 and 10), so
the editor and `place` keep putting an object at the ground's height.

**A picture painted as the editor paints** (`map_tool.py paint <folder> <game folder>`). POLIGON's
picture was painted by `map_tool` with colours of its own (a meadow, gravel, earth), which the
editor's pipette could not name and which looked like nothing a player paints. `paint` paints an
unpacked map's `vid\ground.png` the way the Paint tool does (`editor_ground.cpp`, the arithmetic
ported - doubles where the editor has floats, so an edge pixel may differ by a step): the 128 x 128
patches that the game folder's `editor	extures.tsv` names in the campaign grounds (read with
`ground_picture`), laid mirrored at the pixel's place - the grass everywhere, light grass in soft
strokes of 3-6 dabs (an LCG of its own, so the picture comes out the same every time), the sand as
packed earth under the factories, repair points and forts (three dabs of 72 px across each) - and
the meadow gravel bed under every plain rail (`paint_bed`, as `ground_bed`: the band of the decal's
top along each rail's line, 28 px across, 7 px up the screen). The height stays as it is (POLIGON's
1586, its GRID 70). `game/maps.new/sandbox/vid/ground.png` is painted so: in the editor Alt+click
names the grass, the light grass, the sand and the meadow bed, and in play the factories' shadows
lie on the ground as before and `SANDBOX_DRIVE_TEST` reaches its points 0-4 (the train stops at
the west edge).

The repository keeps such maps in `game/maps.new/<name>/`: `build.py` packs the folder as
`maps\<name>\<name>.map`, its `vid/` folder (an unpacked image, optional) as
`maps\<name>\<name>.vid`, and copies every other file (the script, briefing texts) next to
them. `game/maps.new/template/` is what the tool makes from mission 1 - it loads with
`test_game.ps1 -Map maps\template\template.map`, shows mission 1's terrain and has no units
on it: a starting point for a mission of its own (set LEVEL, MONEY and the rest in the
script).

The mission number in the script (`LEVEL`) also selects the briefing of the level
(`maps\l<LEVEL+1>\brief.lst`) and its header text (`MapHeader<LEVEL+1>` in
`strings.ini`), so a map of its own reuses the number of a mission until the script and
the texts are changed.

**Editing a map** (`map_tool.py list <file.map>`, `vids <objects.res>`, `place <folder>
<nvid> <x> <y>`). `list` prints the size, the ground image, the objects with their
positions and the grid; `vids` prints the objects of `objects.res` (the ones to place,
`--grep` filters by name or number); `place` puts an object into an unpacked map: it takes
the id, the position, the direction and the army, the height from the ground (the grid
cell) and copies the state (`behave`, `items`, `data`) of another object of the same VID -
in the map itself or in the map given with `--from` - because the engine reads that state
when the level starts. A unit without such an object is refused; the commands of the source
are not copied, they are the orders of its mission (a move to its own place). Pass `--objects
<game>/objects.res` as well: without it the class of the object is unknown, so the check
cannot tell a unit (an engine, a building - they need the state) from a rail or a rock, and
the tool only warns; a unit placed that way has no state and the level does not start.
`delete <folder> <nvid>` removes every object of one VID (the old rails before a new
track, for example).

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
directory is kept, and the menu commands wait while a dialog is open. Paths inside the game
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
(`StopMusic(g_sound)`, 0x444e50, as script function 134) and loads the saved map again, held: the
state of the run does not stay, the editor's bar comes back as before (the state bar's icons, VID
455, get back their untinted gammas: the level's `SetArmyGamma` lights them for army 0), the view and
the chosen object (found again by VID and place) and the tool as they were, and the undo history: before
the run the editor keeps it with every object of the level in the world's order and the rail points
its cars stand on (their places and their links' other points); after the load held again
(`edit_restore_history`) the new objects are matched by that order, VID and place (the save keeps the
order), a deleted one the history can bring back is made again and hidden, and the history's objects,
the cars' couplings and rail positions and the later units' factories point at the new ones. RESTART of the same map keeps it running.

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
and its line in `maps\custom.lst`; a title or briefing header NEW MAP becomes the folder's name in
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

**Editing objects** (`src/LocoFix/editor_edit.cpp`). On the held level LocoFix does the editing
with the engine's own pieces (scripts and object updates are off, so nothing else would):

| Action | Keys and mouse | How |
|---|---|---|
| select | a click on an object (the smallest one under the cursor; rails only when nothing else is there) | the selection circle VID 460 ("Select Unit Circle", its outline frame) is drawn under it and the guns in its sockets |
| its panel | the right button on an object (Select tool) | the object's panel (below): army, turn, delete, a semaphore's colour and line, the gun in each socket |
| move | drag a selected object | `Object_MoveTo` (what `ACT_CHANGE_COOR` 63 calls); the height follows the ground (`World_GetGroundZForVid`), keeping the object's own height above it |
| height | PgUp / PgDn by 4 (Shift: 16) or Shift+wheel, Home back on the ground; HEIGHT: LOWER / RAISE / ON THE GROUND in its panel | the object and its guns `Object_MoveTo` by the change of its height above the ground (z less `World_GetGroundZForVid` under it), -32 to 400; shown in the status line ("height above the ground: N") and after "selected"; saved as its SPR z ("The relief and the heights of objects" below) |
| turn | R / E, the mouse wheel, Edit > Turn right / left or the panel's TURN | by what is turned (`turn` in `editor_edit.cpp`, help page 15): `Object_SetDirection` by one of the VID's `nDir` directions, R right and E left - the guns in its sockets go to their new places, a semaphore's lamp halo (VID 154) is turned with it so the light spot it links (155) moves with it, a decal's directions are its looks, an object of one direction (the citadel, a crate, a region) says so; turned round, R and E alike - a train (`Train_Reverse`), a wreck on the track, a post beside a track (across the track, below), in the Push line tool the selected line; turned by its track - a factory, a repair point, a blocker at a dead end (drag it along the track; the panel's TURN row says BY ITS TRACK); a rail is not turned (laid again), and the Track tool turns nothing. Before placing, R / E turn the preview: a step of the VID's directions (the direction placed is one of its frames: a direction left from an object of more directions is rounded to them), round for a car or a post, and an object the track turns says so |
| army | A (0 -> 1 -> 2 -> 3: ours, enemy, neutral, pirates) | `ACT_SET_ARMY` 97; the armies' colours as in play (below) |
| semaphore red / green | T | red is the command `10,0,0,0` (MAP_RULES.md 7), shown with that animation - also the red posts of a loaded map, whose first command never runs on a held level (the command stays as it is); a post's own line changes with it ("On the track") |
| delete | Del, or the Delete tool of the bar (the filled circle marks what a click takes, the cursor is the game's delete cursor); Shift+Del or Shift+click on a rail: its section, as far as one rail continues it at each end (up to a junction or an end) | `ACT_UNDO_REMOVE` 132 - made for the authors' MapEdit: the object leaves the layer lists (not drawn, not updated, not saved; a train car leaves its rail points and couplings); a rail also leaves the rail network (below) |
| undo / redo | Ctrl+Z / Ctrl+Y | every change keeps the state of the objects it touched before and after (position, direction, army, present, red); undo puts the "before" back - a deleted object returns with `ACT_UNDO_INSERT` 133 - and redo the "after"; the last 500 steps are kept, at most 256 MB of them (below) |
| place | the bar at the bottom: 1-0 or a click picks an entry, a click on the map places it, Esc or the right button go back to selecting | World `CreateObject` at the point under the cursor on the ground, after `AskPlace` (161) - a place another object with a common `moveMask` takes is refused; a preview of the object (its own sprite in the chosen direction, `Graphics_DrawVid`) follows the cursor |

The bar looks like the game's state bar and is drawn with its pictures over the scene, after
`Graphics_DrawScene` (`Graphics_DrawVid`, in front of everything): the bar's base (VID 450) in
the middle of the bottom edge with its side tiles (VID 938, as LocoFix fix 23 lines them up) out
to the edges of the view, the first ten entries in the base's ten slots (where the unit bar shows
its units, 41 px apart) and the others in the factory panel's strip (VID 497) a slot higher, the
chosen one in the slot frame (VID 456), each with its key in the factory panel's small font (VID 8,
right-aligned at the icon's lower edge as the panel's F-keys). The icons come from the state
bar's atlas where it has one (VID 455: the cross 3, the factory 10, the wrench 2, the cannon 36,
the locomotive 11) and otherwise from the editor's own atlas (VID 939, `vid\939edit.vid`, made
when the game is assembled, below): the arrow, the track, a section to delete, the fort, the
semaphores green and red, a decal. The status line at the top is in the mouse tips' font (VID 6,
each character in its orange box - the look of the game's tips): the tool or the entry under the
cursor with its key, the undo and redo counts, "not saved" and the last message. A text is drawn
the way `Text_Draw` writes one: a frame of the font VID per character (its code), the VID's
width apart. A click on the bar's plates does nothing on the map under them. Its entries: Select
(1), Delete (2), Track (3, below), the factory 102 (4), the repair point 104 (5), the fort 162 (6, army
1), All objects (7: the palette, below), a green (8) and a red (9) semaphore 152, Train arrivals (0: a
panel, "Train arrivals" below), and in the strip Delete a section of track (a click on a rail deletes its section,
as Shift+click with Delete), Decals (below; [ and ] pick which of the 61 decals of `objects.res` -
read from the file: the editor mode loads them as class 0 -, its slot shows the chosen one's
palette icon), the Push line tool, Mission
settings (the dialog of the File menu; both below), Ground height, a money crate (VID 169, army 3)
and Vehicles (the cars' palette, below), Paint the ground, Water and lava and Region (below).
The camera of a held level goes 300 px past
every edge of the map (`camera.cpp` widens the bounds `SetPosition` clamps to while the level is
held), so the bar does not hide the bottom of a small map and the parts of the track out of
sight - the main line's ends, the reinforcement's point - can be laid and picked. The rules of MAP_RULES.md the editor applies: a new
unit gets the behave and the build list of an object of the same VID on the map (map_tool.py
place copies them the same way; one of the same army first - the enemy factory of a mission has no
list; with none on the map, the state of one on a mission's map, from the palette's table), a new
player's factory without such an object the game's 14 factory units; a fort, a mining plant or a
citadel comes with the missions' most common guns in its sockets (MAP_RULES.md 6), which move, turn,
change army and are deleted with it; a reinforcement point is put on the nearest track by
`Engine_Construct` itself; a factory, repair point, fort or citadel snaps onto the track ("On the
track" below). Rails are not moved or turned (a rail is laid again instead); train cars move with their train along the track (below) - a car
stands on its rail points).

Checked on the sandbox (`test_game.ps1 -Editor` with the steps of "Working with the code"):
select, drag, turn and change the army of the player's factory, undo twice and redo, a red
semaphore made green and a green one red, a fort with its guns, a factory, a semaphore, a
reinforcement point on the main line and a decal placed, the repair point deleted, undone and
deleted again, saved and compared with `map_tool.py unpack`: 191 objects - the 185 of the map
with the factory moved and turned, the repair point gone and the six new ones - GRID, GROU and the OBJ
record unchanged, the semaphores' commands as set, the new factory with the 14 units; then File >
Play the map: the edited map plays with its interface, and `error.log` has no `LOGIC`, `R_MAP`
or `RAIL` line.

**The history's limits** (`editor_edit.cpp`, "object state and the undo history"). Undoing and
redoing far crashed the editor: a train car's state kept the pointers to the rail points its two ends
stand on (`RailPos`), but undo and redo take rails away and lay them again - `RailDot_Release`
deletes a point no rail uses any more and `RailMap_AddDot` makes a new one -, so a car put back by
`ACT_UNDO_INSERT` (which runs `Engine_OccupyDots`) wrote into a freed point and `Engine_UpdatePosition`
read from one. A car's ends are now kept by the places of their points and of the link's other end
and found again when the car is put back (`RailMap_FindDot`, `RailDot_FindLink`, as `RailPos_Load`
does); a point that is gone leaves the end empty instead. The history is bounded too: a stroke of the
ground's tools keeps the rectangle of the picture it changed before and after (10 bytes a pixel, tens
of MB for a stroke across a big map) and the game has 2 GB, so at most 500 steps and 256 MB of them
are kept, the oldest going first (`trim_history`; "the history keeps the last ..." in `LocoFix.log`).
Checked on the sandbox: the reinforcement's locomotive deleted, the main line's section under it
deleted, 4 rails laid, three strokes of Ground height, a factory and a fort placed, then undone past
the start and redone past the end three times - the build before crashed at the undo that put the
locomotive back (`EXCEPTION!`, EIP 0x40a241 in `Engine_LeaveDots`), this one does not, and the
locomotive stands on its points again; 510 turns of a fort left 500 steps.

**Laying track** (the Track tool, key 3; `src/LocoFix/rail_rules.h`). The first click starts a
line - at the end of a rail of the map near the cursor, or at the point of the rail lattice of
the nearest rail end - and every next click lays one rail toward the cursor: the step of the
lattice (one of the 12 shapes of VID 610, 71 x 50 px or their halves) whose end is nearest to
it. The preview draws that rail and marks its ends green, or marks them red and names the rule
in the status line ("does not join the rail before it - a turn needs a curve", "turns more than
45 degrees", "this rail is already there"...). The right button or Esc ends the line (a second one the
tool). A click of the middle button (a drag of it pans the map, `mouse_pan.cpp`) deletes the rail under
the cursor (of the rails whose box holds it, the one whose middle is nearest), with its bed and what
stands on it (below), as one step of the history; when it was the last rail of the line being laid,
the line goes on from the point before at that point's height. (For a while the right button deleted
the rail instead: a right click meant to end a line took its last curve away and the next clicks of a
star junction - several curves from one point - were laid from the wrong point and refused.) The rules
are those of `map_tool.py track` (MAP_RULES.md 3) in C++: `rail_rules.h` mirrors `plan_track`,
`rail_candidates`, `rail_ends_at` and `track_cells` of map_tool.py, and `tools\test_rail_rules.py`
compares the two on generated cases (see "Working with the code"). A rail is created with World
`CreateObject` at the shape's direction (`RAIL_DIRECTION`) and the line's height: `Rail_Construct`
runs `ACT_CHANGE_DIRECTION`, which puts its two points into `g_railMap` and links them to the
rails at its ends, as loading does. The rails of the map are read with their ramps and point
heights (`rail::rail_of`), so a line starts at a ramp's end as well, and the rules see only the
rails whose points are at the line's height. A line continuing a rail takes the height of its
rail point there (and its VID); a new one
the ground under its first rail - the rails stand 4 below the ground - and the ground is flattened
the way `map_tool.py track` does: the cells a rail crosses get the track's level, the cells around
them half the way, from their height before the line (so a line laid in one go gives exactly
map_tool's grid; checked on the template map), written with `World_SetGroundZ` and undone with
the rail. Extern 81 `ReCalcGridZ(int unit)`, which `export.lgc` describes as recalculating the
grid with a unit's size, is not used: it is not implemented in the exe and is about one object's
footprint, not the cells along a track.

`Rail_Action` has no case for `ACT_UNDO_REMOVE`/`INSERT`, so a deleted rail would leave its two
points and their links in the rail network and trains would go on driving there. The editor
removes the rail's own link and releases its points (`RailDot_Release` deletes a point no other
rail uses, with its links - a junction's shared point stays with the other rail's links), and
undo runs `ACT_CHANGE_DIRECTION` again. Rails deleted (Del, the Delete and Section tools, the
Track tool's right button) take along what depends on their points, in the same step of the
history: the train cars with an end on one of their links (an earlier version refused the deletion
instead, which left a car hanging in the grass when it did go), the money crates on a point that
goes, the editor's push lines with an end on a point that goes or whose way is cut (`SetPushLine`
would take the nearest other point - the line would jump onto other track; the script's own lines
are only counted in the status line), and an arrival's entry or task point there
(`edit_arrivals_track_gone`); the semaphores beside them and a blocker stay. `RailDot_RemoveLink`
reorders the links of a point next to a deleted rail, which a car's rail position indexes, so the
editor keeps with each car the point its ends' links lead to and finds the link again after the
deletion and after an undo or redo (`pos_relink`). Checked with the rail points logged before and after (the test
command `D`): a rail of a new loop deleted - its two points gone, the neighbours' links to them
too - and undone; the loop's section (8 rails, 14 points) deleted with Shift+Del and undone, the
junction's point keeping the main line's links.

Checked on the sandbox: a branch from the bottom main line through a curve sharing its centre with
the main line's rail (a junction), down, east and back up to the main line at another junction -
a loop of 8 rails; saved, `map_tool.py check-track` finds nothing, `unpack` shows the map with
the 8 rails added and the GRID unchanged (the ground there is flat); played with the sandbox's
`SANDBOX_DRIVE_TEST` driving the reinforcement to points on the loop: it went from the main line
through the first junction onto the loop, round to the second junction and back - `error.log`
without `R_DOT`, `R_MAP` or `RAIL` lines.
**Debug keys.** With `DebugMode=1` in `Locoland.cfg` the game has its keys (`GameWorld_Run`).
While the level is held only O (ground height grid and object debug, World flags 0x8800) gets
the debug drawing called at all (flag 0x8000), after which H (object numbers, GameWorld flag 4; but
the editor takes H for the ground's heights while the level is held)
and G (the object map, 8) show as well; I is the flagman debug; P freezes the game entirely (no
drawing either) and ~ reloads the VIDs. R and E are the editor's keys while the level is held
(the rail debug view R works when it runs), and E (select units of any army) is player input,
which is off then. (Read from the code, not tried in editor mode.)

**Mission settings and push lines** (`src/LocoFix/editor_mission.cpp`, the Push line tool of
`editor_edit.cpp`). A mission is its map and its script, and the editor keeps what it manages
in two marked blocks of the script (`<map>.lgc`), which it writes anew whenever the map is
saved - the rest of the file, the mission's own code, stays as it is:

- `// <editor>` ... `// </editor>` before `#include "maps\script\header.lgc"`: the settings the
  scripts of `maps\script\` read (MAP_RULES.md 8) - `CUSTOM_MAP`, `MONEY`, the time of day
  (`START_DAYNIGHT`), `RAIN`, lightning (`NUKE`), the text files (`OBJECTIVES_FILE`, `WIN_FILE`,
  `LOOSE_FILE`, `BRIEF_LIST`, `BRIEF_HEADER`, `HISCORES_FILE`), the player's reinforcement
  (`PLAYER_REINFORCEMENT`, `player_reinforcement_train[]`), the enemy (`LIMIT_ATACKERS`, the
  patrol points `ATA_PATROLn_X1` ... `_Y2`, `start_trains*[]`, `level_queue_*[]`) and the other
  settings of the template (music, weather, the gammas, the AI timings) as they were read; a
  `// title:` line keeps the map's title for the custom maps page,
- `// <editor main>` ... `// </editor main>` in `main()`'s `iff(1)` block: the `SetPushLine`
  calls.

File > Mission settings... (or the gear of the bar's strip) shows them in a Windows dialog -
the title on the custom maps page, the briefing's header, money at the start (`SetMoney(0, MONEY)`
of `initdefault.lgc`), the time of day at the start (`START_DAYNIGHT` 1-4: day, evening, night,
morning; `daynight.lgc` then goes on, 5 minutes each), the weather (`RAIN_TYPE`: rain, snow, clouds,
storm clouds, meteorites - `environmenttact.lgc` makes the last three) and how often (`RAIN`: 0
never, 1 now and then - it comes and goes, about half a minute each -, 2 all the time;
`daynight.lgc`'s own comment has 1 and 2 the other way round, its code does this), lightning
(`NUKE`: a flash and its sound at random in a clear sky, with rain only while it rains, never with
the other weathers), the reinforcement waves and their train (VIDs), the enemy trains attacking at once,
the enemy's trains at the start (a line each: its type - `light_gun`, `medium_mortair`, ... the
`ATT_` names of `TrainsTactDeclares.lgc` - and the point it starts at, or two rail points it
patrols between, screen coordinates), the trains its factory builds later (the time, the type and
optionally the patrol), the objectives, the win and the loss texts and the briefing's pages (a
line `---` starts the next page; the dialog shows the texts the game shows - the pack's copy of a
file when it has one). A save writes the texts into the map's folder (or the pack's copy of a file, see the sandbox below) in capitals, in the pack's code page,
at most 44 characters a line (broken between words), CRLF: `objectives.txt`, `win.txt`,
`loose.txt`, the pages `brief.txt`, `brief2.txt` ... and `brief.lst`. The dialog had rain as a
check box that wrote `RAIN 1` - a campaign map's `RAIN 2` became 1 when it was saved under a new
name - and dropped `RAIN_TYPE`, so a map with snow got rain; its labels were cut short in some
packs. Now each setting has a row with its label in full words, the map's settings on the right of the trains'
rows (which stay as they were). Checked in play: money 12345, night, snow all the time and a new
objectives text set, saved and played - $12345 in the state bar, the night's gamma, the snow
falling all the time, the objectives line at the start. The dialog as it is now, filled in on
POLIGON saved under three names and each played (Polish, English, Polish): money 7777, night, rain
all the time, lightning, a title and header, objectives, win, loss and a two-page briefing with
Polish letters - $7777, the night, the rain, flashes about every 20 s, "CEL: ZNISZCZ WROGA ŻÓŁTY",
the header (in ASCII capitals, as written) over the briefing's first page in the Esc menu, the
win's text on the victory screen; money 4321, morning, snow now and then, no lightning - the
morning's light, the snow, no flash in a minute, the second briefing page (the right arrow), the
loss's text on the defeat screen; money 100, evening, meteorites all the time, lightning ticked -
the meteorites, no flash (lightning comes only in a clear sky or the rain). A map saved as
`maps\<name>\<name>.map` gets its line in `maps\custom.lst` (the folder and its title in ASCII
capitals; not the template), so it shows on the WLASNE MAPY page; `build.py`
keeps the lines the editor added when it writes the repository's list into the game. A new map
(no script of its name yet) gets the script of the map it was saved from when that one has the
blocks, otherwise the template's (`maps\template\template.lgc`, made by `map_tool.py new
--script`: a whole mission like the sandbox - `CUSTOM_MAP`, the texts, the reinforcement, the
win when the enemy had units and has none left, the loss when the player had units and has
neither units nor a factory, and `MAP_WIN_TEST`, which destroys the enemy at 20 s to check the
end). A map saved from the template does not keep its name: a title or a briefing header "TEMPLATE"
(the template's own - the Esc menu's first page showed it in a map played from the editor) becomes
the map's folder name in capitals (`mission_save`; checked: the template saved as `maps\tpltest\` has
"TPLTEST" in its script and on the custom maps page). A script without the blocks (the campaign) is not changed: its settings and
its push lines are read and shown, read-only, and saved under a new name the map becomes an
editor mission with them.

The sandbox (POLIGON) is an editor mission too: its script has both blocks, written as File > Save
writes them (the order of the settings, `int start_trains[]={ATT_LIGHT_GUN,ATT_LIGHT_MORTAIR,-1};`,
the bare `SetPushLine` calls), so a save gives the same script back (checked by repeating the
parse and the write of `editor_mission.cpp` on the file). What the editor does not manage stays
outside the blocks: the mission's own car lists of the train types (`int this_train0[]` ... - they
come before the block, as `TrainsTactDeclares.lgc` takes them only when they are declared before
it), the comments of the push lines (above the `// <editor main>` marker), the tests
(`SANDBOX_DRIVE_TEST` takes the start trains out of the lists in `iff(1)`, before `TrainsTact()`
first reads them, instead of an `#ifdef` inside the block, which a save would lose) and its own
win and loss. Its texts had lines of 45-47 characters, which a save would have broken; they are
broken at the sentence. A save keeps the briefing's high scores page (`hiscores.txt`, last in
`brief.lst`, as the sandbox and the campaign have it) when the list had it and the folder has the
page, and writes a text the language pack has a copy of (the sandbox's,
`lang\<code>\maps\sandbox\`) into that copy - the game reads it instead of the root's, so an edit
written to the root never showed.

**The enemy's trains of the script** (`draw_enemy_trains`, `editor_edit.cpp`). The start trains
and the factory's queue (`start_trains*`, `level_queue_*`) are made by the AI while the level
runs (`TrainsTact.lgc`, `ai_events.lgc`), so the map has no object for them. On the held level the
editor marks them: the reinforcement point's icon in a red frame where a start train starts, a
patrol's way along the track (traced as a push line is) in red to a ringed far end, and the icon on
the enemy's factory (VID 102, army 1) for the trains it builds that attack; the mouse over a mark
describes it below the status line ("enemy train: light gun, patrol 1810,1061 - 1810,736", the
queue's with its time). The start trains are edited in File > Mission settings..., the queue in
an enemy factory's panel (its plan); the marks follow them at once.

The Push line tool (the strip, the rails under a green arrow) picks two points of the rail
network - the cursor's point is shown in cyan with its screen coordinates, which are also the
ones the dialog's patrol points take - the begin and then the end of a line, and checks it the
way `SetPushLine` (0x4346b0) builds it: both ends are the rail points with links nearest to the
given screen points (`RailMap_FindNearestDot`: x, and y against the point's y - z), the begin
needs a link ("Can't SetPushLine"), an end on the same point as the begin moves to its neighbour
nearest to the end, and there must be a way between them (`RailDot_Flood`: "Can't PushLineFindDot").
A line that would fail is refused with the engine's reason; the lines of a loaded map are checked
and logged too (`LocoFix.log`: "push line ... ok"). The lines are drawn over the track: green for
push_flag 1, yellow for 0, light blue for the script's own lines, each with a bar across the track
at its begin, a small arrow on every link pointing the way it runs and an arrowhead at its end, on a
dark outline; the mouse over a line names it in a status line, in any tool: its begin and end and
what it does (push_flag 1: the trains on it are pushed the way of the arrows and none may go
against them; 0: none may go against them, none is pushed). A click on an end of a line selects it - its bar or arrow, in the
Select tool too, where a line is an object like the others: it turns white, the status line names it
(its begin, end, what it does and whose it is - a semaphore's, a factory's or repair point's, the
level's entry or exit) while the mouse is over no other, and the right button opens its panel in the
game's style (DIRECTION: TURN ROUND (R), ACTION (T): PUSHES TRAINS / ONE WAY ONLY, DELETE (DEL)): T switches its flag
(a new line has 1: the level entry, factory exits, repair point legs; the level exit has 0), R (or E,
the wheel) turns it round, Del deletes it - all undoable - and S puts its semaphores
the way the missions have them (MAP_RULES.md 7): a post faces the trains it is for and stands
24 px to their right, a red one at the line's end for the trains that would enter it against
its way, a green one at its begin for those it lets in (none at a dead end - a factory's); a line
that begins past the edge of the map (the level's entry or exit) has the pair at its end, moved
33 px into the map when the end is nearer the edge than 40 px; a post already standing within
30 px of such a place is moved, turned and coloured instead of a new one (the rule as the
sandbox's ten posts and the missions' stand).

Checked end to end in the game: from "EDYTOR MAP" (the template) a new mission - a main line from
past the south-west corner of the map to past the north-east one (18 rails), a junction with a
spur to the player's factory (direction 192, at the dead end), a spur to the enemy's factory, a fort
on the main line, the reinforcement's point past the edge, four push lines (the entry from past
the edge, flag 1; the exit pointing into the map, flag 0; both factory exits, flag 1) with their
semaphores put by S, money 500, an enemy train patrolling the main line, Polish texts and a
two-page briefing - saved as `maps\editortest\editortest.map`: `map_tool.py check-track` finds
no problem, the semaphores stand as the rule says, the script compiles; the map shows as
"PROBA EDYTORA" on the WLASNE MAPY page with its briefing, starts from its start button with the
objectives and the money, its trains run, `error.log` has no `R_MAP` or `LOGIC` line, and with
`MAP_WIN_TEST` the win comes with the map's own text. File > Play the map played it and came
back to the editor after the end screen. A variant with two trains of the factory's queue (one
patrolling), night and rain plays without errors too.

**Train arrivals** (`src/LocoFix/editor_arrivals.cpp` the panel, `editor_mission.cpp` the script,
`editor_edit.cpp` "arrivals on the map"). The missions bring trains from past the edge of the map
in a few ways: the player's reinforcement - `AddReinforcement()` of `reinforcement.lgc` makes
`player_reinforcement_train[]` at the place of the army-3 VID 5 locomotive that `ReinforcementInit`
takes away, at most `PLAYER_REINFORCEMENT` times, each mission calling it every 5 s when the player
has no locomotive (and, l02-l20, a neutral factory still stands); seven missions really give one
(l01-l04, l06, l08, l11; l04 sells it for 300) - and the queues of templates of l13 (the player's)
and l19 (both sides: times, nine entries with their push lines, "fewer than 3 locomotives" bringing
a train earlier, the enemy's trains given to the AI, the win and loss waiting for the queues).
`AI_REINFORCEMENT` and the text `EnemyReinforcementArrived` exist and are never used. The editor
follows l19's way. An **arrival** (`Arrival`, `editor_mission.h`) is the army, the cars, the entry (a
rail point: screen x, y and its z), the first time, the time between waves and their number (0: no
end), a condition (`ArrivalIf`: none, the player has no train / fewer than n / fewer units than n /
no factory, a neutral factory stands, the enemy has fewer than n trains, fewer than n attack, the
player pays n, the campaign's "no train and a neutral factory") that either gates the time ("only
then", as l01-l13) or brings the wave earlier ("also earlier", as l19), the task after it (the
enemy's: `ATA_FIGHT`, `ATA_FIGHT_FORT`, `ATA_STANDn` at a point, `ATA_PATROLn` between two points -
the AI of `TrainsTact` runs them, only army 1 has one -; ours, the neutrals' and the pirates':
`ACT_MOVE` to a point on the last car, as the marker's order 33, a patrol - `ACT_MOVE` to its first
point, then `PatrolEngine` to the other end once it is within 40 px of it (`ArrivalOnPoint`; the
arrays `arrival_to2_x/y[]` and `arrival_unit[]`, task -3) - or "stays", no order: in play such a
train stood behind the entry and held every later arrival at it, so the panel warns when another one
comes from there), the enemy's raids
(`ai_trains_random_task`) and the message (`ReinforcementArrived` / `EnemyReinforcementArrived` with
sound 100, the game's own texts).

In the script (`editor_mission.cpp`, "arrivals", has the details): `#define ARRIVALS n`, a train of
its own cars as `int this_trainNN[]={...,0}` - the authors' way of redefining a template, with a type
no template has (ones digit 4, 6, 7 or 8; the tens digit the kind the AI treats it as: 1x mortars, 2x
rockets, 3x airships, 5x flame, 6x landing, 7x the super weapons, 0x the rest; a template's cars as
they are keep its ATT_ type) -, the posts and patrols (`ATA_STANDn_X/Y`, `ATA_PATROLn_*`, numbered
after the start trains' and the queue's) before `TrainsTactDeclares.lgc`, the columns
`arrival_army[]` ... `arrival_last[]` (each with a 0 after the last arrival) after it. The runner is
the block `// <editor arrivals>` after `header.lgc` - the same text in every map (`ARRIVALS_RUNNER` of
`editor_mission.cpp` and of `map_tool.py`, kept the same), so an exported map runs in any game -,
called from the block `// <editor tact>` after main()'s `TrainsTact();`. Once a second `ArrivalTry`
looks at each arrival: its time and condition; then it waits while a car stands within 180 px of the
entry (`FindNearestSprite(GETSPRITE_CLASS+B_ENGINE, ...)`, as l13) or the AI has its 16 trains
(`find_num()`); it makes the cars (up to 30: the scripts' `temp_array`, which `arr_cpp` fills) with
`Create(vid, x, y+z, z)` in the army (behave | 1 for the
other armies, as `StartCreateTrain`) one at a time - the next one once the last one has moved 36 px
off the rail point (the entry's push line moves it; 5 s at most), `arrival_part/car[]` - because cars
made on one point at once push each other apart out of their order (seen in play: 5,20,25 came as
20,5,25 and 5,20,25,80,85 as 80,25,5,20,85) -,
then registers the enemy's train in `ai_trains[]` (as l19 and
`TrainsTact`'s start), gives the others' last car its drive, shows the message and logs `ARRIVAL n
army a wave w at t s task k slot s cars <planned>` and a second later `ARRIVAL n made <the train's
cars from its first one>` in `error.log`; a paid arrival is paid as its first car comes. `ArrivalsLeft(army)` counts the waves still to
come (those of a gating condition only while it holds); the template's win and loss wait for it. The
template (`map_tool.py new --script`) has both blocks, no arrivals and no reinforcement waves.
The editor sets no limits of its own: any number of arrivals, waves and trains in the factory's queue;
an arrival's train up to 30 cars (`temp_array`), a train of the queue up to 16 (the AI builds and
takes `ARR_LENGTH` cars). The panel's last line names a limit of the game's AI only once it is
reached: more than 10 patrols or posts (the rest attack), more than 16 enemy trains (they wait for a
free place in `ai_trains`).

An editor map made before keeps its reinforcement and its blocks until the panel's CONVERT: the new
blocks are added at the next save (refused with the reason when `main()` has no `TrainsTact();` after
`header.lgc`), the reinforcement becomes two arrivals at its point (the first wave at the start, the
others when the player has no locomotive and has a factory of its own - `IF_OWN_FACTORY`, condition
10 of `MissionIf`, as the old template and POLIGON bring their second wave), driving where the marker's
order 33 sent it, `PLAYER_REINFORCEMENT` becomes 0 and the point locomotives go. The script's start
trains (`start_trains*`, which the AI makes when the level starts and the editor showed as marks on the
track) become trains of the map (`place_start_train`): the cars of their type as the map's script has
them (its own `this_trainNN[]` wherever it stands in the script, else the authors'), the enemy's, laid
from the start point along the track as a drag lays a train (the first car at the point), with the
order in their panel - `ATA_PATROLn` a patrol to the far end of its patrol, else an attack - which
`OrdersTact` hands to the AI as before; the arrays are written empty. Rebuilding a lost start train
(`start_trains_renew`, read only for the start trains by `ai_events.lgc` DepoFree) and its raids
(`start_trains_random_task`) are not kept: a train of the map has no such setting. The queue
(`level_queue_*`) is already the enemy factory's plan in a map that runs arrivals. All of it is one
step of the history (the start trains are in it: `Operation::start`). Until a map is converted, a left
or right click on a start train's mark opens the Arrivals panel with CONVERT (`start_mark_click`), not
the panel of the rail under it, and the status line says what the mark is. Checked in play on POLIGON
converted: the light gun train came out as 20,5,20 at 1810,1061 and the light mortar train as 25,10,25
at 1437,1299, both patrolled between the same points as before for 90 s (the AI's `STTEST` log of the
old and the converted map side by side), the queue's first train was queued at 240 s and the AI took
it; SANDBOX_DRIVE_TEST (points 0-4 reached, the enemy's 6 cars taken away first), SANDBOX_WIN_TEST
(VICTORY) and SANDBOX_LOSE_TEST (DEFEAT, no second wave without a factory) pass.
`game/maps.new/sandbox` is POLIGON so converted (its map and script). A campaign mission or the sandbox (no editor blocks) shows
the reason only. A script edited outside the editor so that it lost `TrainsTact();` is saved without
the arrivals, unit limits and later units; the editor keeps them and says so in the status line. The marker locomotive's panel (VID 5, army 3) shows what it is in play - "ours arrive
here" with reinforcement waves, "taken away at the start" without - instead of its parking army 3,
and opens the Arrivals panel.

**Capturing** on the maps of the editor (`CaptureTact` of the same runner). The campaign captures in
two ways: a fort or a mining plant by its event handler `F162_0` / `F165_0` (`ai_default_event.lgc`,
every map has it: a train of another army within 75 px and no gun left within 150 px - none needed
for a neutral one), and a neutral factory or repair point only by the calls `LDepoCapture` and
`LMasterCapture` (`Captures.lgc`) that each mission makes in its own `main()` for its one such building
at a point of its own - so a map made in the editor, with none of them, never captured those two, and
`LDepoCapture` even takes once "the" factory of army 2 or 0, which on a map with the player's factory
can be that one. `CaptureTact` turns every neutral factory and repair point to the player when a train
of the player's comes within 100 px (`PlaySFX(25)`, the flash, `BuildingCapture` with its message, a
`CAPTURE` line in `error.log`). A repair point taking over the enemy's wagons brought by the player's
locomotive is the engine's own (`Building_UpdateWorkshop`), on any map. Checked in play: a neutral repair
point and a neutral factory beside the main line captured as trains passed them, and an enemy wagon in a
player's train taken over going through the player's repair point. Older editor maps get it with
CONVERT in the Arrivals panel.

**The trains' orders** (`editor_edit.cpp` "trains", `OrdersTact` of the runner). A train of the map has
an order in its object panel, kept with its first car in the editor's state (undoable) - the same kinds
as the arrivals' and the factory queue's: in a map that runs arrivals, ORDERS: NONE, DRIVE TO... and
PATROL... for ours, the neutrals' and the pirates' trains, NONE, ATTACK, GUARD... and PATROL... for the
enemy's (its AI: `ATA_FIGHT`, `ATA_STANDn`, `ATA_PATROLn` between where it stands and the point, with a
train type of its cars); in an older map the patrol alone, as `PatrolEngine` lines in `main()`. The
block's arrays `ORDERS`, `order_vid/x/y[]` (the first car, as it stands), `order_task[]` (-2 drive, -3
patrol, an `ATA_` task), `order_type[]`, `order_to_x/y/z[]`; `OrdersTact` runs once at the start (after
the AI's own start trains took their slots), finds each train with `FindNearestSprite` and logs `ORDER n
task t train 1 slot k`. A train the AI gets must be armed: a bare locomotive is made a ram (`ATA_CLASH`)
and waits. Checked in play: the enemy's trains with a gun car went to their post and stayed, patrolled
between their two points and drove at the player's train to attack it; ours drove to its point and
patrolled. Test mode: `order: vid x y drive|patrol|attack|guard [to_x to_y to_z]` lines of `M<file>`.

**The templates** (TEMPLATE... in the Arrivals panel and in the enemy factory's queue) are the authors'
trains exactly, read from the game's own scripts when the list is first shown
(`mission_author_trains`, `editor_mission.cpp`): every `int this_trainNN[]={...}` of
`maps\script\TrainsTactDeclares.lgc`, named by the `#define ATT_...` before it, and every one a
campaign mission redefines before it includes that file (`maps\lNN\lNN.lgc`, lines that are not
comments; 107 different compositions with the 20 missions - the missions put the locomotive in the
middle or at the back, e.g. `{20,5,30,20}`, `{80,5,80}`). The cars keep the authors' order; a mission's
own is named "(MISSION n)". The missions' queues (`level_queue_trains`, `start_trains`, l13/l19's
`our_queue`/`enemy_queue`) take their trains from these types, so they add no compositions of their own.
The engine drives a train whatever the locomotive's place (checked in play: 20,5,20 and 20,20,5 drove
to their points), so the only rule is a locomotive somewhere in the train.

**The panel** (the bar's entry 0, a click on an arrival's entry or point on the map, the marker's
panel, Mission settings' Arrivals... button, which keeps the dialog's settings as OK does) is drawn
in editor_panel.cpp's frame, in three screens. The list: each arrival in two rows of plain words
(number, army, entry, the first car's name and the number of cars; when and how often), a click
opening it, + NEW (ours, a locomotive and two gun cars, at the entry of the one chosen), COPY n and
DELETE n for the one opened last, pages of five (every row stays above the button bar). An arrival, one thing a row: WHO; CARS (the palette's
icons over three rows, in the train's order - a click chooses a car, < and > move it, REMOVE takes it
away; + CAR opens the Vehicles palette, which adds the car picked at the end and goes back; TEMPLATE... the list of the authors' trains of the normal difficulty by name,
the defence trains aside - they have no locomotive); ENTRY (PICK: the next click takes the
rail point nearest to the cursor, shown in cyan; "line ok" when a push_flag 1 line of the editor or
the script runs from it); WHEN - one choice of AT THE START, AFTER n s, WHEN THE PLAYER HAS NO TRAIN,
WHEN THE PLAYER PAYS n (the first time, or the condition `IF_NO_TRAIN` / `IF_MONEY` "only then" with
the first time as NOT BEFORE); an older map's other condition is shown as it is; REPEAT - ONCE, n TIMES,
WITHOUT END, EVERY m s (steps of 10 s); THEN and the POINT(S) (PICK; a patrol's two ends one after
the other); MESSAGE (ours and the enemy's) on the title's line at its right. The strip above the title names a real problem with the button that
mends it: no entry (PICK THE ENTRY), no track at it, no entry line - the line and its lights are one
thing (the posts own a line that ends at them, `edit_post_line` of the signals' side): with a pair of
posts within 400 px on the way in (`edit_posts_on_way`) LINE TO THE LIGHTS lays the line from the
entry to them, else PUT LIGHTS puts the pair the first place 50 px inside the map with its line
(`edit_place_entry_posts`); the entry row shows "LIGHTS x,y" once they are there -, no locomotive anywhere in the train (ADD ONE), no point or no other
end (PICK IT), an endless enemy - the map is not won by destroying it - (10 TIMES), a train that stays
where the next ones come (DRIVE ON). The list's last rows: a limit of the game's AI once reached, the
authors' reinforcement to convert. The arrow at the bottom left, Esc and the right button go a screen
back, from the list they close the panel. Each change is one step of the history
(`edit_commit_arrivals`; Ctrl+Z / Ctrl+Y reach it while the panel is open). The entries are drawn on
the map ("ENTRY n"), the chosen arrival's points too, joined to its entry; a deletion of rails that
takes an entry's or a point's rail point unsets it in the same step of the history
(`edit_arrivals_track_gone`), so no marker stays in the grass and Ctrl+Z brings it back. Test mode: `arrival:` lines
of the `M<file>` settings (`arrival_of_line`: `army=`, `cars=`, `at=x,y,z`, `first=`, `every=`,
`times=`, `if=`, `n=`, `earlier=`, `task=`, `to=`, `to2=`, `raid=`, `msg=`).

Checked in play (three test maps on POLIGON's ground and track, scripts in the editor's format with a
logger of the trains' places every 5 s - the `ARRIVAL` lines of the runner in `error.log`; the maps
and their generator are kept outside the repository): every arrival came at its time - at the start
(1 s), at a set time (70 and 90 s), every 45 s three times (70, 115, 160 s), when the player's last
train was lost (4 s after the enemy destroyed it, never while a train was left), while a neutral
factory stood, the campaign's "no train and a neutral factory" (and not again once that factory was
gone), when the player had no factory, when the player had no units (the game counts the cars and the
guns, `ARMY0_NVID_ARRAY`, not the factory), "also earlier" while the player had fewer than 3 trains
(at 7 s for a time of 999 s), only when fewer than 1 enemy train attacked, not while the enemy had 2
trains or more, and the bought one at its time with the money taken; a wave waited while a train
stood at the entry. After it: the player's, the neutrals' and the pirates' trains drove to their
points and stopped there (the pirates fired at the player's trains in passing), the enemy's patrolled
between its two points again and again, went to its post and stayed, and attacked the player's trains;
the messages showed. "Stays at the entry": such a train stood behind the entry line and held every
later arrival there - the panel warns when another one comes from there. Not offered: "attacks the
forts" (`ATA_FIGHT_FORT` drove at the player's base, not at the player's fort beside it), the raids
(no sortie seen) and the conditions other than "no train" and "pays" (read from a map and kept). The template's script compiles
and runs with no arrivals; POLIGON and a copy of it saved on the template (its reinforcement not
converted) bring the player's train as before (in the player's unit bar); a map's CONVERT turns
POLIGON's reinforcement into two arrivals of its cars at its entry (with "line ok").

**A factory's build options** (`src/LocoFix/editor_factory.cpp` the panel, `editor_mission.cpp`
"factories", `editor_edit.cpp` "a factory's panel's side"). How the missions do it: what a factory
builds is its build list, the `Unit.items` of its SPRD record, in the order of its keys (F1-F12, [
and ] - `Player_SetPanel`); the campaign grows the player's units mission by mission through these
lists alone (l02's neutral factory has 5 and 20, l14's player's ten, l15-l20 the super weapons) -
no script adds or removes an item (`ACT_ADD_ITEM` 54 and `ACT_DELETE_ALL_ITEM` 57 are not called by
any mission). The enemy's factories have empty lists in 11 of the 13 missions with one: the AI
(`ai_events.lgc` `DepoFree`, only for a factory of army 1 - the factories of the neutrals and the
pirates build nothing by themselves; a neutral one is there to be captured, a pirate one is not:
`Captures.lgc` `LDepoCapture` and the runner's `CaptureTact` take army 2 only) builds with `ACT_BUILD_UNIT`
commands what `level_queue_*` and the rebuilding of lost start trains ask for, in its first free
factory, and ignores the list. Unit limits: `InitDefault` would call `AddUnitLimit(0, nvid, LIMITxx)`
for `#define LIMITxx`, but every one of those defines is commented out in the missions, and the
function takes its arguments swapped (the first is the limit, the last the slot: 255 the total,
else the army - `BUGS.md`), so that path would set a limit 0 in the slot LIMITxx; the editor calls
`AddUnitLimit(max, nvid, army)`, which `Player_CanBuild` checks against the army's units on the map
and in the factories' queues (with LocoFix fix 14) - a script's `Create` is not limited by it.
**The panel** (the factory's object panel: FACTORY: WHAT IT BUILDS...): for ours, the neutrals' and
the pirates' factory the game's 14 factory units as the palette's icons - a click ticks or unticks
one, the ticked ones keep the game's order and show their key -, and for the unit clicked last IN
THE LIST: at the start or after a time (steps of 30 s) and AT MOST: the player's limit of it (army 0,
0 none); a neutral factory says that the AI builds nothing there and the list is what the
player builds after capturing it, a pirate one that the player captures only neutral factories. For the enemy's factory:
its plan (below). In the script: the start list stays the factory's
SPRD items (the map's file), the units it gets later are `UNLOCKS` and `unlock_x/y/vid/time[]` (the
factory by its place, kept with the factory when it is moved and dropped with it when it is deleted,
written as it stands at the save) and the limits `UNIT_LIMITS` and `limit_army/vid/max[]`, both in the
settings' block and run by `FactoryTact` of the arrivals' runner (the limits once at the start; a
unit `ACT_ADD_ITEM` at its time, logged `UNLOCK vid n at t s` in `error.log`) - so these two need a map
that runs arrivals (the panel says so and leads to the conversion), while the list and the enemy's
plan (once each, at a time) work in any editor map. Every
change is one step of the history (the factory's list is in the editor's state of an object now).
Test mode: `limit: army nvid max` and `unlock: x y nvid seconds` lines of the `M<file>` settings.
Checked in play: with a limit of 1 on VID 20, F3 twice queued one and F4 (no limit) twice queued two;
a unit put into the list at 30 s (`UNLOCK vid 30 at 30 s, factory 1`) was there the next time the
factory was selected (a factory already selected keeps showing its old list); the keys stay the game's
own per unit (a list without the airship cart leaves F8 out).

**A factory's plan** (`editor_factory.cpp` the panel, `editor_mission.cpp` "factories" and the
runner's `PlanTact`). What a script can make a factory build, seen in play (probe maps of the
sandbox, `ARRIVAL`/`FQP` lines in `error.log`): `AddCommand(f, ACT_BUILD_UNIT, v)` - what the AI's
`BuildTrain` does - makes a factory of any army build the cars in the order given, each paid from
that army's money when it starts (the price the factory panel shows: the build time in s - 5: 30,
20: 25, 25: 30, 10: 50; `GetVidData(v, VID_BUILDTIME)` gives it for locomotives only, 0 for cars,
so the editor writes the prices, `price_vid/price_of[]`), and drops a car the money does not cover;
the factory's build list does not limit it and stays as it is; the cars line up on the factory's
push line and link into one train (without a push line each new car is made on the last one, which
is destroyed). The AI's `BuildTrain` (`ai_functions.lgc`) swaps a train's first two cars before it
builds them, so the authors' queue builds LIGHT GUN `{5,20,20}` as 20,5,20; types ending in 5 change
with the difficulty (`arr_gettype`); the destruction of any enemy factory (`DepoDestroy`) resets the
AI's one build (`depo_task`, `depo_array`), so a train under way in another factory loses its AI
slot while that factory still makes the cars already ordered (in fqtest, a copy of arrtest2, our
fort's two guns - VID 115 at 1171,192 and 1171,259 - destroy the enemy factory at 1313,565 at 15 s:
"ENEMY FACTORY DESTROYED" is that one; the factory switched from ours to the enemy stands). The
original's queue and these quirks stay; the editor's plan works around them.
**The data**: `FactoryState.queue`, entries (`FactoryTrain`) of the enemy (`factory` null) and of a
factory of ours, the neutrals' or the pirates' (the factory by its place `fx, fy`, found by
`edit_level_held` and placed by `edit_place_plans` at a save - an entry of a deleted factory or of
one that became the enemy's is not written); each with its cars as the train is to leave the
factory, WHEN (`when`: at `time` s, `time` s after the first train of the entry before it of the
same factory is made, or while a condition holds and not before `time` s - the conditions are one
list, `mission_conditions()`, tested by the runner's `MissionIf(c, n)`, which the arrivals use
too), REPEAT (`times` 1, n or 0 without end, the next one `every` s after the last one was made) and
THEN (the enemy's: `ATA_FIGHT`, `ATA_STANDn`, `ATA_PATROLn`; the others': stays, drives to a point,
patrols, as an arrival's -1 / -2 / -3), and `AiKnobs` (below).
**The script**: a map that runs arrivals gets `PLANS` and the columns
`plan_type/task/when/first/every/times/if/if_n/prev/fx/fy/army/to_x/to_y/to_z/to2_x/to2_y[]`, the
runner's state `plan_state/done/last/since/slot/fifo/next/base/unit[]`, `PRICES` with the prices,
and an empty authors' queue with a place for each entry. `PlanTact` puts an enemy wave that is due
into the queue's first empty place with a time already past (`DepoFree` builds the queue's first
one, in the AI's first free factory, while fewer than `LIMIT_ATACKERS` attack) and follows it to the
AI's slot (`ATT_DEPO_CREATED`) and the train made; an enemy entry's type is its cars with the first
two swapped (`declared_order`), so `BuildTrain` swaps them back, and never one ending in 5, so the
difficulty does not change it. The others' factory is ordered car by car (`PlanBuild`): a car when
the factory is free (`ACT_GET_COMMAND` not `COM_BUILD` and its `GetCommands` back to its length
before) and the army has its price, then the train at the door (the last car within 160 px, once
its cars are linked, 10 s at most) gets its order; an entry runs only while its factory stands and has its army, one entry at a time in a
factory and not while a train stands within 250 px of it - at its door or on its spur (seen in play: two entries started together
mixed their cars, 5,20 and 5,25 came as 5,25,5,20, and the cars of the next train joined one that
stayed at the door). Every
wave is in `error.log`: the enemy's `ARRIVAL F<n> wave w queued ... cars <planned, as built>`,
`taken ... slot k`, `built ... made <the cars from the first one>` or `lost` (the AI dropped it);
the others' `ordered ... cars` and `built ... army a made` or `lost` (the factory gone); an
arrival's line has its `cars` and a second later `ARRIVAL n made <the train's cars>`. A map that
does not run arrivals writes the enemy's entries into the authors' queue as before (each once, at
its time; the panel offers no more there). Mission settings shows the number of the enemy's entries
and opens an enemy factory's panel (Plan...); a test file's `queue:` lines still give the enemy's
entries (each once), and `plan:` lines (`plan_of_line`: `cars= when=at|after|if first= if= n=
times= every= task= to= to2= factory=x,y`) the whole plan.
**The AI's knobs** (the enemy factory's AI... screen, `AiKnobs`, written as the defines and arrays
the scripts read): `LIMIT_ATACKERS` (the plan waits while that many attack), `start_trains_renew`
and `RENEW_TIMEOUT` (a lost start train is built again, before the plan, after that many s +- half
at random), `start_trains_random_task` (a start train's raid every n s +- 15), `FIRSTATTACKTIME` (no
raids before it, x2 on easy, x1.5 on normal), `FIRSTCONTACTTIME` (how long a raid lasts),
`SetCleverEnemyAttack` (`CLEVER_ATTACK`, set by the runner after main's own call; the engine's
attack with the weapons' range) and `STG_DESTROY_TIME` (read by no script but mission 2's own code,
shown as such). The start trains' renew and raids are one value for all of them in the panel, "each
its own" while they differ. Test mode: `F<x>:<y>` opens the panel of the factory at a map point.
Checked in play: the table in `arrivals-tests` of the session notes (each army's entries, the cars
planned and made).

**Ground, gravel bed, decals** (`editor_edit.cpp`, `ground.h`, `ground.cpp`). The strip's Ground height
tool raises, lowers, flattens or smooths the GRID cells (8 x 8 px) in a circle around the cursor while
the button is held (M the brush, the mouse wheel or + and - the radius - the wheel sizes the brush of
the Decals, Paint and Water tools too, where it turned nothing, and Shift+wheel stays the selected
object's height in the Select tool; checked: three notches up, one down -> radius 5, then 4; a stroke
ends - one step of the history - also when the window or the mouse's capture is lost or the tool
changes, and the brush's repeat timer stops when the button is found let go; the brush and the picture: "The relief and the
heights of objects" below), shows the brush's cells at their height coloured by it, leaves the track's
cells and their ramp as they are, puts the objects back on the new ground and makes the stroke one
undo step (World_SetGroundZ). New track gets the gravel bed decal the missions use
(VID 612, 615 ice, 616 volcanic, 617 desert: the rail's place and direction, 18 below it; B cycles it,
the map's most common one by default), deleted and undone with its rail. In the editor a bed is an
object drawn 18 px under its rail: an embankment whose slope the ground's picture hides, as the
missions' grounds do, so it shows as the band of gravel under the rails wherever the rails show
(where the ground's picture stands higher than the track, a lawn of the template, both are hidden);
in play it is painted into the ground. The
Decals tool places the chosen decal - its name (the palette's) in the status line, its icon in the
bar's slot, its sprite at the cursor: a click puts one at the cursor, dragging scatters 1 + radius of
them a step in the circle (+ and -; radius 0 one a step along the drag), each turned at random (a
decal's directions are its variants), none nearer than the spacing (< and >, 4-64 px, 16 by default)
to a decal already there (beds aside), so they do not stack; Shift takes the decals in the circle
away. [ and ] pick the decal, and a click on the chosen Decals entry opens the palette, where a click
on a decal makes it the brush's. A map's first decal is the ground cover it has most of (the beds,
wrecks, buildings and track pieces aside), else the round bush 936. A placed factory, repair point or fort gets its footprint flattened as map_tool.py place
does, and script function 81 `ReCalcGridZ(int unit)` - declared in export.lgc, missing in the exe -
now does the same (`ground.cpp`, through the hook of `restart.cpp`). Track is laid flat (the ramp
shapes of the rails, frame groups 1 and 2, are not used): the brushes leave its cells alone, so
trains never meet a slope. Checked in the game: raise and flatten strokes beside the main line of the
test map - saved, 126 cells changed and none of the track's cells or their ring (compared with
`map_tool.py unpack`), a factory's footprint then the same cell for cell as map_tool.py place makes
it on that grid, `check-track` clean, the map played with its trains running and no error; a rail
with its bed deleted and undone (rail points logged), the bed seen under new rails in the editor and
painted into the ground in play; 15 decals scattered, taken away with Shift and brought back by undo;
a test script calling `ReCalcGridZ` for every factory ("ReCalcGridZ: 192 cells ..." in
`LocoFix.log`, no "Unknown extern").

**The relief and the heights of objects** (`editor_ground.cpp` `ground_relief`, `editor_edit.cpp`
the sections "the ground" and "the height of an object"). The ground's picture is in screen
coordinates (a pixel is the screen point x, y - z; MAP_RULES.md 1) and its D16 height texture holds
1026 + 8 a GRID level, the "depth" every sprite is drawn against (a sprite pixel shows where its
height - 1024 + 8 z and its own depth - is not below the buffer's: `Vid_BlitSpanDepth`). So raising
the GRID alone made the objects stand dz px higher than the ground they were drawn on - they floated,
and the tester's "how do I change an object's height" had no answer either. Now:

- *The relief.* When the GRID changes, the picture is drawn again there: a column at a time, front to
  back (the ground's world y from the bottom of the screen up, in quarter-pixel steps), every pixel
  gets the first ground point that reaches it - raised ground shows higher and hides what is behind
  it, lowered ground may be hidden by the ground in front -, that point's texel from where the old
  heights showed it, lightened or darkened by the new slope against the old one (light from the upper
  left, half of it ambient: a steep face toward the viewer stays readable), and its height texel
  raised by 8 a level. The heights between the cells' middles are interpolated (the engine stands an
  object on its cell's value, the value at the middle). Within a Ground height stroke every step is
  drawn from the stroke's start picture, so nothing is lost step by step; the stroke keeps the
  rectangle it changed (`PaintPatch`) and undo puts the picture back exactly. The track's and a
  placed building's flattening are drawn the same way, one step each. Always for the Ground height
  tool; for the track and buildings only when the picture is the map's own (an empty map, a meadow, a
  grown map, a painted one, or one the tool already redrew): a campaign terrain's `lNN.vid` is left
  as it came until the tool is used there, and then the map gets `<name>.vid` when saved, as a painted
  one does (`ground_save`: never over `lNN.vid`). The Water tool keeps its own way (it lowers the
  picture's heights where it paints, MAP_RULES.md 2). Chosen over an overlay alone because it shows
  in the game too: the hill is in the saved picture, the objects stand on what is seen, the depth hides
  what is behind it; the cost is a picture of the map's own (a few MB) once the ground is shaped.
- *The brush.* Raise and lower add the strength (1-5, 2 at first) a step times the edge's weight -
  soft: a smoothstep of the distance (full in the middle, 0 at the edge), hard: full to the edge -,
  the parts of a level kept per cell for its next step, so hills come out round instead of as
  flat-topped discs; flatten and smooth pull toward their aim by the weight times 0.4 a strength
  (`blend`), rough adds a random -0.5 to +0.5 of the strength times the weight (`mix` of the stroke's
  seed, its step and the cell), ramp sets the cells within the brush of a dragged line to the level
  along it (`ramp_apply`, when the drag ends). The right button lowers in the Raise mode; Shift
  flattens and Ctrl smooths for a stroke (`Stroke.brush`); Alt+click takes the level under the
  cursor for Flatten (`g_flatten_level`; the Alt keys are kept, `WM_SYSKEYDOWN`/`UP` of VK_MENU are
  taken so Alt alone does not open the window's system menu). Held still the brush goes on every 150 ms (a window timer, `HEIGHT_TIMER`) - before,
  it worked only when the cursor entered a new cell. Flatten's level is the cell where the stroke
  begins, now named in the status line ("flatten to level 92"): begun on the track it gives a plateau
  at the track's level - the way to put a line and its factory on a hill: lay the line up its ramps
  (Track tool PgUp, 11 levels each), raise the ground around the top, flatten it starting on the track.
- *Reading the heights.* Contour lines every 4 levels over the whole view (a dash where two
  neighbouring cells are on two sides of a level, at their height, coloured blue - green - red by it,
  every 20 levels brighter), always with the Ground height tool and with any tool after H.
- *The ground under the cursor* (`map_point`, used by the brush, placing, the Track tool's start and
  the decals) is the ground point nearest to the viewer whose y - z reaches the cursor, searched down
  the column as far as its highest cell allows; it was two guesses from the flat ground, which on a
  hill or a plateau took the wrong cell (a flatten begun on the track's top took the ground below).
- *An object's own height.* The object's height above the ground is its z less the ground under it
  (`World_GetGroundZForVid`); a drag and a Ground height stroke keep it. PgUp / PgDn (Shift: 16; Track
  and Push line tools aside, where PgUp / PgDn are the ramps), Shift+wheel and Home set it for the
  selection (a fort's or mining plant's guns go with it), -32 to 400, each change one step of the
  history; the status line shows it. Saved as the SPR z and kept when the map is loaded:
  `Object_Construct` takes the z it is given, except for the 17 VIDs with flag 0x200, which it puts
  on the ground (units 70 and 517-519, the civil robots 72 and 77, the objects 74, 79 and 414, the
  creature 421 and the generators and guns 401, 403, 404, 427, 556, 573, 706) - the editor refuses
  those with the reason, as it refuses train cars (they stand on their rails: the Track tool's ramps
  raise them) and regions (their z is their water's or fog's level). In play a unit or a creature
  that moves takes the ground's height again (`Object_Move`, `Cannon_Move`, `Creature_Move`,
  `CivRobot_Move` read it; read from the code), so a lifted unit stays lifted only while it stands; a
  lifted decal is painted into the ground picture where it is drawn.

Checked in the game (English and Polish): an empty 2048 x 1536 map; a line with two ramps up to level
92, a hill raised around its top end and flattened from the track (the log: "flatten to level 92"),
a factory at the line's dead end on the plateau, a fort beside it lifted by 12 ("12 above the ground";
after saving and loading the map: "selected VID 162 ... 12 above the ground"), the fort's panel with
WYSOKOSC: NIZEJ / WYZEJ / NA ZIEMI, a hill raised, undone (the picture flat again) and redone, the
contours with H in the Select tool; played: the hills and the plateau in the saved picture, the
objects standing on it, a locomotive placed in the editor patrolling up both ramps onto the plateau
and back, `error.log` without rail or logic errors; mission 1's terrain: a hill raised and the map
saved as `maps\camptest\camptest.map` with `camptest.vid` (`l01.vid` unchanged), loaded back with the
hill; `test_missions.ps1` all 20, `test_rail_rules.py` 3000 of 3000.

**A new map** (File > New..., `src/LocoFix/editor_newmap.cpp`). The dialog takes the map's folder in
`maps\` and its ground: an empty map - a plain ground of one of the painting's textures (grass,
sand, snow, rock ...; the 128 px sample laid mirrored, its brightness varied in soft patches so the
repeat does not show), flat at 70, nothing on it, ready to paint -, a meadow, or the terrain of one of
the 20 campaign maps (its size with it). An empty map or a meadow is any size from 512 to 4096 px in
steps of 256 (the ground's tile; the camera, the GRID and the ground's tiles handle that): - and +
beside the width and the height step by 256 within those limits, five buttons set the campaign's sizes
1024 x 768, 2048 x 1536, 3072 x 2304 and 1536 x 1024, 4096 x 3072, and the line beside them shows the
result in pixels and in tiles. File > Map size has the same row. The map is made the way `map_tool.py new` makes one: the chunks of the
campaign map with only the ground object (VID 1024 in OBJ, SPR and SPRD; no players, no groups), the
ground's image staying the game's own `maps\lNN\lNN.vid` (nothing is copied). A meadow is written as
`maps\<name>\<name>.vid` - value noise in two greens with drier patches, laid out as `map_tool.py
ground` lays a ground (256 x 256 R5G6B5 textures column by column, each with its height texture,
imageFlags 0x35) - on mission 1's map with the size set, the ground object in the middle (the picture
is drawn centred on it) and the GRID flat at 70. The picture's height is 1586 everywhere, what the
campaign's grounds hold where their GRID is 70 (they hold 1026 + 8 a level: l06 and l13 at 0 hold
1025-1027, l15 at 144 2181): the depth of the objects is measured against it, so the lower part of what stands below the
ground (a bed's slope) is hidden as on the campaign's maps. The template's script (with the
editor's blocks) and texts are copied under the new name, and the editor loads the map.

File > Map size... resizes the saved map: the right and the bottom edge move, from 512 to 4096 px in
steps of 256. The objects that would fall outside are deleted - rails, beds and cars up to 300 px
past the new edge stay, the main line goes on out of sight - after a question with their count;
then the map is saved, its file is resized - HEAD's size, the GRID (a new cell gets the height of
the old cell nearest to it) - and loaded again (not undone). Growing gives the map a ground image of
its own: the old picture with its heights (read from its texture tiles: the map's own or the
campaign's) and a meadow beyond it, the ground object moving to the middle. Cropping cuts the same
way: the map gets an image of its own of exactly the new size - the old picture with its heights
where it lay on the map (drawn centred on the ground object) up to the new edge - and the ground
object goes to the middle, so nothing of the old ground is drawn past the edge; before, the image was
kept and the old terrain still showed in the 300 px the camera goes past the map. The Water tool's
tiles outside go too, and the cars outside are deleted before the rails (a rail is not deleted while
a car stands on it). A campaign map or the template is not resized in place: it is saved under a
name of its own first, and the editor's new map gets its name first. Checked: a 1280 x 1024 meadow
made, a rail laid, saved, grown to 1792 x 1280 (the old picture kept, the rail where it was) and
cropped to 1024 x 768; a 2048 x 1536 meadow with a line of 8 rails from x 816 to 1420, a fort at
x 1400 and a hill, cropped to 1024 x 768: the ground ends at the new edge (the grid beyond it), the
line goes on to x 1384 out of sight (7 rails; the one past 300 px, the fort and its guns deleted),
the hill kept, `crop3.vid` of 1024 x 768, the ground object at 512, 384; `map_tool.py check`
round-trips every map and image of the game folder.

**The map's frame.** The camera of a held level goes 300 px past the map, where nothing is drawn;
the outside gets a dim 32 px grid and the map's edge a frame, so an empty map stands out - the track
out there stays visible between the lines.

**The armies' colours.** The colours of the armies are not in `objects.res`: the mission script sets
them when the level starts (`SetArmyGamma()` of `maps\script\setarmygamma.lgc`, called by
`InitDefault`: `SetVid(nvid, VID_GAMMA + army, ...)` for the units, the buildings' flags - VID 175 and
770-774 - and the state bar's icons). A held level runs no script, so every army looked the same;
the editor sets the same gammas when it holds a level (`SetVidData` 18-21: `Gamma_FromScriptInt`,
then the VID's `SetGamma`, vtable slot 7; not the icons of VID 455, which its bar draws untinted). The
buildings' flags are their linked objects (`Object_CreateLinked` at construction: the fort and the
mine link VID 175, the factory and the repair point a light that links it), so they follow the army:
green for ours, red for the enemy, yellow for the neutrals, black for the pirates (checked with the
sandbox's factory, A three times). The status line names the army ("army 1 (enemy)").

**On the track** (`editor_edit.cpp`, the section of the same name). The authors put these on the
track (measured over the 20 missions): every factory (VID 102) at the dead end of a straight spur,
turned the way out along it (the nearest rail 1-38 px away), every repair point (104) on a straight
leg, turned along it like a gate, a post (152) 15-58 px beside the track (median 31), facing the
trains it is for, a money crate (169, army 3; `F169_8` gives the player 100 when a unit of
theirs takes it) on the track (1-46 px from a rail, median 17) and a blocker - the buffer stop, 150
("Blocker", the palette's Railway group) - at a dead end (all 99 of them within 16 px of a rail point
with one link, median 5 px past it). Placing them snaps there - the preview shows where -,
elsewhere they are refused with the reason in the status line:

| Object | Where it goes | With it |
|---|---|---|
| factory | the dead end (a rail point with one link) nearest to the cursor within 150 px, 22 px inside along the spur (the sandbox and l05), turned the way out (multiples of 32) | its exit push line as the missions have them: from the dead end along the spur, 140 px (they are 100-175), push_flag 1 - built cars appear in the factory and are pushed clear - with its post |
| repair point | the point of a straight piece (two links going opposite ways) nearest within 100 px, turned along it the way nearest to the direction chosen (at first the way of its link with the smaller direction: north on a vertical leg, as l05's); R / E turn it round | the push line through its leg, 120 px each side (they are 139-304), push_flag 1, one way in the direction it faces, with its posts |
| post | 24 px beside the track point nearest to the cursor, on the cursor's side of the track (R / E or the wheel put the preview on the other side), facing along the track the way that has that side on the right of its trains - its side follows from its way (184 of the missions' 188 posts stand so, 12-40 px from the track, median 23); the status line names its trains ("for the trains going left"); R / E, the wheel or the panel's FOR TRAINS turn a post beside a track round and put it across the track, as far from it as it stood (the track a post is beside is the one its trains pass on their right: the rail point nearest to 24 px on the post's other side - at a junction other track is as near), a post with no track within 70 px turns a step of its 8 directions | its own line at once, as the missions' posts have theirs (measured: each of the 188 posts against the nearest end of its script's lines): a line ending at the post's rail point - a red post's coming from behind it (107 of the 124 red posts; 85 with push_flag 1), so the trains it faces would enter it at its end against it and stop at the post; a green post's coming from the trains it faces (29 of the 61 green ones; 26 more at the begin of a line going on), so they run along it, pushed, and leave it at the post. The editor's are 70 px long with push_flag 1 (the red post's then pushes the trains of the other way toward it, as the missions' do). A pair at an entry - green facing the edge for the trains coming in, red facing in for those going out - owns one line from past the edge to the pair: the missions' entry (l02: `SetPushLine(-107,1203,54,1465,1)` with its pair at 54,1465). A line is a post's own by that shape alone, of any length (`owns`), so a loaded map's posts keep theirs, a pair shares one and the line of S or the Arrivals panel's entry belongs to its posts; never a factory's or repair point's line. None for a post that owns one already or stands within 40 px of an end of a line no post owns (a building's, the script's). Turning the post round, T or moving it makes its line again (a line another post owns too stays, the post gets one beside it); deleting the post deletes its line unless another post owns it (one step of the history each); L / Shift+L give a selected post its line green / red, the panel's LINE row says what the line does - STOPS THEM (red) or LETS THEM IN (green), the trains of its FOR TRAINS row - or NONE; the preview draws the line it will get. A post's line is drawn where it acts: a green post's line green (the way its trains are let in); a red post's not where the engine's line lies (behind it) but in front of it, where its trains come from and stop - red, from 35 px toward them to the post, the arrowhead and a red stop bar at the post; a pair's line green (the way in) with the red post's red mark in front of the pair (none goes back out); the status line says it in words ("the red semaphore's at 443,1399: the trains going right stop there (514,1299 -> 443,1299)"). A factory's or repair point's line keeps its flag's colour. Checked in play (`SANDBOX_DRIVE_TEST` on a copy of the sandbox): a red post on the main line for the trains going right, `SetPushLine(443,1299,301,1299,1)` - the reinforcement coming east onto that piece stopped at 387,1366 and stayed; the same post green, `SetPushLine(159,1299,301,1299,1)` - it went on east and reached its points; a pair at 194 (green south for the trains going right, red north for those going left) made one line, `SetPushLine(52,1299,194,1299,1)` - the reinforcement from past the west edge passed it east, and sent back west it stopped at 216,1375 and stayed |
| money crate | the rail point nearest within 80 px | - |
| blocker (150) | the dead end nearest to the cursor within 150 px (not a factory's: within 40 px of one), 5 px past it, across the track: its 4 frames (nDir 4) are the 4 ways of the rail lattice - 0 on a vertical track, 64 rising to the right, 128 horizontal, 192 falling to the right, as the missions turn them (their 32 and 160 are the same frames) | - (it is a sign: a train stops at the dead end anyway, the blocker's moveMask 0x10 has no bit of the cars') |
| fort | the line of the straight rail nearest within 60 px (`rail_line_spot`: its two ends' segment), where the cursor falls on it, turned along it the way nearest to the direction chosen - all 20 forts of the missions stand 0-6 px from such a line (R / E turn it round there) | its guns |
| Charybdis | the line of the straight rail nearest within 100 px through its middle - l06's diagonal passes 1 px from it, so what it pulls in comes within the 40 px it eats in | its pieces |
| citadel | over the diagonal rail nearest within 80 px, the rail's line 8 px north of its middle - the three citadels of l04 and l18 stand over the crossing of two diagonal tracks, which cross there, a grate on each southern arm | its pieces and tower |
| crusher | across the straight rail nearest within 60 px, its middle on the rail's line, the drums on both sides - its form (the stands' direction) by the track's way, as `Pirate_Crasher` puts the ten of l04 and l18 | its second stand and drums |
| Scylla | on the diagonal rail nearest within 60 px, the stand on its line, the jaws across it - direction 0 on a track rising to the right, 128 on one falling, as l06's two | its jaws |

Dragging one of them puts it at the place on the track nearest to where it is dropped (with a new
line and posts for a factory or repair point; a fort, a citadel or Charybdis with all that goes with
it); deleting a factory or a repair point takes its line and posts with it - each one step of the
history. A factory is not turned by hand (its spur turns it), nor is a blocker at a dead end; a repair
point turns round on its leg - R / E, or TURN: ROUND in its panel, also while it is being placed -,
and its line is made anew the other way with its posts (a green one at the new way in, a red one at
the far end), one step of the history. Checked in the game: a mission made on the template with the main line, a spur
with a factory at its dead end (line and red post made), a repair point on the main line (line,
green and red posts), a crate on the line and the reinforcement point; played with a test script in
the game folder's copy: the reinforcement, damaged to 60 hp, was pushed through the repair point and
came out at 150, took the crate (money 1975 -> 2075), and the factory's train appeared inside it and
was pushed out to its line's end; `check-track` clean.

**The object's panel, the guns' sockets, the palette** (`editor_edit.cpp`, `editor_panel.cpp`). The
right button on an object in the Select tool opens its panel beside it - drawn in the game's style,
not a Windows menu: rows of the factory panel's strip (VID 497, 41 px), the label in the menu font
(VID 2) and the choices in the lit font (VID 7), the current one underlined; a click on a choice
applies it as one step of the history, the right button elsewhere closes it. Its rows: the army
(ours, enemy, neutral, pirates), turn left / right, for a semaphore the trains it is for (FOR
TRAINS: the way they go, the other way turns it round), its colour and what its own line does (STOPS THEM / LETS THEM IN, or NONE;
L / Shift+L), a row for each socket of a fort, a mining plant or a citadel
(empty, a tower with a cannon 110, a missile tower 115; the citadel's the pirates' tower 117), and
delete. A neutral object (army 2) and a pirate one (army 3) have a row IN A FIGHT, which says what
it does by the rules of the map (MAP_RULES.md 6c, `faction_rows`). On a map of the editor (its
settings have `FACTIONS 1`, LocoFix fix 33): a neutral one ONLY WHOEVER ATTACKS IT and, armed (a gun,
a fort's guns, a train with a turret), IT FIRES: WHEN ATTACKED or NEVER - its behave's bit 1,
`BEH_AGGRESSIVE`, set or cleared on its armed members, a step of the history (the editor's object
state has the behave); a pirate one AGAINST ALL SIDES (a click sets the bit on an armed one that
lacks it). A unit that becomes a neutral's or a pirate's - placed in that army or its army changed -
gets the bit (`faction_behave`). On a campaign map the engine's rules: a neutral one NO ONE FIRES AT
IT (`Object_FindTarget` 0x449ea0 takes a target of any other army but army 2, unless the shooter's
weapon has flag 0x80 or the enemy may hit neutral trains after `SetEnemyCanAttackNeutralTrains`)
and, armed, IT FIRES: AT ALL IN RANGE or NO - with the bit it fires at the player, the enemy and the
pirates alike; the missions' neutrals (civil trains and robots) have the bit clear. Checked in play
under the engine's rules: a neutral missile tower with the bit took a player's train from 320 to 15 hp in
60 s and was not fired at by it, one without the bit left a train beside it at 450. A, R, E, T, L and Del stay as the keys for them. A gun goes exactly into its socket (the
missions' offsets, MAP_RULES.md 6), with the host's army (on a citadel of army 0 the enemy's, as in
the missions) and direction, and follows it. A gun in a socket is part of its host: a click on it
selects the host (`pick`, `host_of`) and the right button opens the host's panel, where each socket
has one row - SOCKET 1, SOCKET 2, numbered on the wall while the panel is open, an empty one framed -
and a click on EMPTY, CANNON or MISSILES puts that gun in at once, underlined. The bar's entry 7, All objects, opens the palette: a
panel like Help with the groups down the left (meadow, forest, desert, ice, volcanic, swamp, park,
pirates, city, industry, railway, flags and crates, wrecks, units and creatures: the 236 kinds of
object of the missions, trains and cars aside; the chosen group's name is the panel's title, ALL
OBJECTS / VEHICLES in the strip above it - the title holds about 20 letters, and "WSZYSTKIE OBIEKTY:
JEDNOSTKI, ISTOTY" in one ran past the frame), 12 icons a page on the right (VID 944, made from the
objects' own sprites when the game is assembled), the arrows or the wheel for more and the name of the
one under the mouse (a unit's from the game's `[Units]`, the others' from `[Editor]`); a click picks
it and the map takes it in the army the missions give it, a unit with the behave and build list of
one of its kind on the map or, with none there, of the first on the missions' maps. Under the icons
an army row (ARMY: AS IN THE MISSIONS - All objects only, its first choice -, OURS, ENEMY, NEUTRAL,
PIRATES; the Vehicles palette starts on OURS) sets the army of what is placed next, the chosen one
underlined (`panel_palette_army`); A in the Place tool steps through the armies the same way, the
status line names the one placed ("..., army 1 (enemy)"), and a car coupled to a train still takes the
train's army. Checked in the game in Polish and English: a fort's panel with its army changed to ours and its sockets set to a
cannon and a missile tower (both turned with it), a mining plant with its cannon, a citadel with
its tower, a robot, a pirate wall and a bush picked from the palette and placed, decals clicked and
dragged (a band of bushes 16 px apart, none stacked), the map saved, loaded and saved again (the same
60 objects); played: the enemy's citadel, fort and mining plant fire on the player's factory and
train, the enemy's train shoots the guns of the player's fort and its missile tower fires at an
enemy robot.

**Wholes, fly nests and mines** (`editor_edit.cpp`, the sections "wholes", "the fly nest" and
"mines"; the runner's `PiecesTact`, `editor_mission.cpp`; MAP_RULES.md 6 and 6b). The citadel (149)
and Charybdis (254) are placed with the pieces the missions always give them (`pieces_of_kind`: the
citadel's two grates and four corkscrews, Charybdis's roots, teeth, eye, four mortars and two
cannons, found again by their offsets - `pieces_on`), as one object: preview, place, drag, height,
army and delete take them all, a click on a piece selects the whole, and its panel puts back missing
pieces. The faction is the army of what fights (the tower; the teeth, eye and guns; on the missions'
wholes, army 0 themselves, the enemy's - `faction_of`): a whole is placed in its faction's army (the
enemy's from the palette) and its panel's ARMY row shows and sets the faction. Charybdis snaps with the
line of the nearest straight track through its middle, as l06's stands, and eats the player's units. In play the
runner does what the missions' scripts do: `CitadelTact` (l04 / l18 `GratesTact`) for every citadel
whose tower stands, `CharybdisTact` (l06) for every Charybdis whose eye lives, `NestTact` (l06's two
nests) for every nest - the panel's FLIES: GREEN, IN WAVES / GREY, ON GUARD, kept as the nest's
direction (0 / 64), which a nest of one direction does not show.
The crusher and Scylla, which l04 / l18 and l06 make by script, and the decorations' sets the
missions always give their host (MAP_RULES.md 6b, 6c) are wholes the same way: a `Piece` may stand by
the host's frame (`pieces_of_kind(host, direction)`, the engine's `((direction + 128 / nDir) & 255) *
nDir >> 8`: a crusher's form, a chimney's height, a station's ridge) and a slot may hold one of
several VIDs or nothing (`choices`, `optional`: the panel's row - CORKSCREW: WHOLE / BROKEN, ON TOP:
CORKSCREW / STONE / NONE, FLAG:, SMOKE:, FOUNTAIN: -, `set_piece`, one step of the history); turning
or a snap onto a track of another way puts the pieces at their new places (`arrange_pieces`). The
crusher's host is its first stand (144): it snaps across the straight track nearest within 60 px,
its form by the track's way; Scylla's stand (252) onto a diagonal track; the track turns both. The
runner's `CrusherTact` bites as `Pirate_Crasher` (all the map's crushers together every 25 s, the
units of other armies within 20 px, paused while one of its own is within 60 px; dead drums once
its army's towers 117 are all gone - the map had some -, or never: the panel's STOPS WHEN: row,
`crusher_end`, kept as 8 * the choice past the stand's 64 * form, NEVER for one placed on a map
without a tower of its army, which the status line says), `ScyllaTact` as `scillas.lgc` (every 15 s,
every ground unit within 50 px; a Scylla given an army spares it), and `CitadelTact` breaks every
corkscrew of the map once no tower stands, as `ReplaceUnit` in l04 and l18. A mission's own records
are found by their offsets (10 px, the slot's height within 30), so editing l20 moves a flag with its
pole. The survey of all 20 missions behind these - which sets are real, which only look so - is in
MAP_RULES.md 6b / 6c. Checked in the game (a map of four straight tracks on the sandbox's ground):
the crusher took form 1 across a track falling to the right, Scylla forms 0 and 1 on the two
diagonals, every set came from the palette with its pieces, a border post's panel made its corkscrew
broken and back with Ctrl+Z, dragging and Del took the whole (`deleted VID 145 ... and 1 more`), a
station turned three steps kept its flag on its tower, records laid as the missions have them (a
flagpole and flag of l20, an exit and stone of l09, chimneys and smoke of l14, a station and flag of
l18) were each selected and deleted as one; played: the crusher and Scylla destroyed the locomotives
of ours in their reach and spared one 40 px away, an enemy Scylla spared the enemy's two beside it
(`ours 1 -> 0, enemy 2 -> 2` at its bite), and with the citadel's tower destroyed by a test line the
corkscrews broke and the crusher of its army stopped (`CRUSHER 1 stops: no tower of its army left`).
The explosive mine (86) snaps to the
rail point nearest within 40 px, is placed in the army of the palette's row and is marked with its
army by the editor (`draw_mines`: a square in the army's colour and MINE: ENEMY; the engine hides a
mine of another army than the player's, also on the held level, so the editor draws its sprite, and
`pick` takes it although its VID has the screen flag 0x800); the authors' marker (89) is marked MINE
1 IN 3: ENEMY, and its panel makes it a mine of the enemy's. In play the engine shows a mine to its
own army only (VID flag 0x20000), as it always did.
Checked in the game (a map made in the editor: a line, four of our locomotives, a citadel, Charybdis
beside the track, an enemy mine, a nest of each kind; played): the gate hit the locomotive beside
the citadel (`CITADEL 1 gate hits vid 5 army 0` in `error.log`) while its tower fired, Charybdis
pulled in and ate the one beside it, the mine blew up the two over it, the waves nest sent its green
flies and the guard nest a grey one while a train of ours was within 400 px; the citadel's ARMY row
set to OURS turned its tower green and back with Ctrl+Z.

**The rows of the ground's tools** (`editor_edit.cpp`, the section of the same name). While the
Ground height or the Paint tool is chosen, a row of buttons stands above the bar's strip: the depot
panel's strip (VID 497) laid as long as the row, centred on the view, the menu font (VID 2; VID 7 lit
for what is set and what is under the mouse, the set choice underlined). Ground height: the modes
RAISE/LOWER, FLATTEN, SMOOTH, RAMP, ROUGH, SIZE - n +, STRENGTH - n +, EDGE SOFT / HARD; Paint: a
swatch of each texture (its mean colour, the chosen one framed), GRAVEL BED (below: then KIND and WIDTH
too, and UNDER THE WHOLE TRACK), SIZE, STRENGTH (a fifth to all of the texture a pass: `ground_paint`'s
strength), EDGE (`weight` full to the edge). A group that does not fit
beside the others goes into a row above (the window's or the screen's width). A click on a button
sets it; nothing under the rows is painted, raised or picked (`on_options`, as `on_bar`). The right
button stays with the Ground height, Paint and Water tools (Esc leaves them): Raise lowers with it,
Paint and Water put the original back with it. Paint's pipette, Alt+click, reads `ground_texture_at`:
the editor keeps the texture of each pixel it paints (a byte a pixel, `NO_TEXTURE` for the original;
in each stroke's `PaintPatch` for undo, carried by the relief); a pixel it did not paint in this
session is recognized by the picture itself - a texture laid at full strength is its sample at the
pixel's place (mirrored every 128 px), so the texture whose sample 13 or more of the 25 pixels around
match (within a step of each 565 channel) is the one there: a picture painted and saved before, or
painted by `map_tool.py paint`, names its textures too. Checked in the game (English and Polish): the
rows of both tools, HARD and STRENGTH set, the right button lowering, the wheel, a hill, a ramp from
level 70 up to it (106), rough ground, the level taken with Alt+click and Flatten (also with Shift)
going to it, undo; sand chosen in the row and painted, the right button putting the original back,
Alt+click on the sand ("texture: Sand (taken from the picture)") and on the original picture.

**The gravel bed in the Paint tool** (`editor_edit.cpp` "the gravel bed in the Paint tool",
`editor_ground.cpp` "the gravel bed's band"). How the authors bedded their track: l02/l09/l15, l11,
l12 and l18 put a decal under every rail and have nothing under the track in the picture; l10
(desert), l16 (ice) and l14 (volcanic) painted the band into the picture and have no decals (l10's
~28 px across, l16's ~20, centred on the rails seen); l13 has neither. In play l02's decal beds show
as a band ~28 px across, darker than the sprite (~110, 100, 60 where the sprite's top is ~140, 115,
60). No campaign ground has a patch of gravel to sample, and a patch mirrored out of the decal's
60 x 36 px top repeats like a kaleidoscope, so the band is laid along the rail instead: GRAVEL BED in
the Paint tool's row (then KIND - meadow 612, ice 615, volcanic 616, desert 617, the same variable as
the Track tool's B -, WIDTH 8-64 px, 28 by default, SIZE, STRENGTH, EDGE and UNDER THE WHOLE TRACK)
beds the plain and destroyed rails whose line comes within the brush, each once a stroke. A rail's
line is the quadratic curve from its end 0 through its middle to its end 1 (ENDS of `rail_rules.h`,
so curves, switches and ramps - each end at its point's height - are followed), 16 straight pieces,
7 px up the screen: the rail's sprite shows its sleepers 7 px above its place (frame 1's middle row
68 of a 150 px cell), and the band centred on the place looked 7 px low beside them. A pixel within
half the width of a line takes the nearest line's colour: the row of the decal's top (frame 1 of
`vid\<nvid>.vid`, columns 45-104, rows 52-87, each row brought to the mean brightness and darkened
to 13/16) by its distance from the line - the same either side, so rails joined either way round
match - and the column by the place along the rail, laid mirrored a whole number of 60 px to the
rail, so every rail's ends are on the same column. The rails beside those being painted share the
pixels without being painted, so a rail painted in a later stroke meets its neighbour without a
seam. On a picture of the map's own (`ground_own`) the band is painted (a pixel's texture becomes
`BED_TEXTURE` + the kind) and the rails' decal beds are taken away - the band is their bed now, and a
decal over it would show the bed twice; on a campaign terrain whose `lNN.vid` is not yet the map's
own the decal of the kind is laid under each rail instead (one of another kind replaced). The right
button puts the original picture back in the band, or takes the decals away; Alt+click takes the
kind painted there, else the kind of the nearest rail's decal, else the kind whose mean colour the
picture has near a rail (`ground_bed_at`). A stroke or the button is one step of the history (the
picture's rectangle and the decals laid and taken). New track still gets the decal of the kind (B),
the band's look and width in play. Checked in the game (Polish and English): POLIGON's junctions
and curves bedded with a stroke (meadow), the kind ICE and WIDTH 40 over the main line, undone and
redone, desert UNDER THE WHOLE TRACK (166 rails, one step), the right button putting POLIGON's
original back over three rails, Alt+click naming meadow and ice; on mission 2 (a campaign terrain)
a stroke replaced 18 meadow decals with ice ones, undone and redone, Alt+click "taken from the
decal"; ICE chosen in the Paint tool and the Track tool's status line showing the bed 615, B
cycling it on; Alt+click on a band of a picture saved before (POLIGON's, painted by
`map_tool.py paint`) - also in the texture mode, near a rail - naming its kind.

**Wrecks on the track** (`editor_edit.cpp`, the section of the same name). The palette's Wrecks group
has five objects - the burnt carts and cars 13, 94, 57, 22 and 92 (class OBJECT, flags 0x4040,
moveMask 0x17, 5-7 hp) - and three decals (93, 96, 98: painted into the ground in play). The missions
stand 42 of the objects' 45 on the track: 0-40 px from a rail's middle, 6-8 above the rail (3 below
its points), turned along it (a rail of direction 6 has them at 64 or 192, 13 at 32 or 160, 20 at 96
or 224, 0 at 0 or 128); the decals they keep off it. So a wreck object placed or dragged within 40 px
of a rail link on the screen (`track_spot`, the cars' reach) goes onto the link nearest to the
cursor, 3 below its points, turned along it the way nearer to the direction chosen (R turns it
round there, also a selected wreck on the track), the preview and the status line showing it;
elsewhere it stays where it is put. The decals are not snapped. In play a wreck on the track does
not stop a train - the cars' moveMask (0x8) has no bit of its - but is run over: `Train_MoveCars`
gives 5 damage to what has VID flag 0x4000 ("run over") in the first car's box, so the first train
that passes crushes it (seen: a wreck put on the track of a patrolling locomotive was gone after its
first pass).

**Trains, the vehicles' palette, patrols** (`editor_edit.cpp`, the section "trains"). The strip's
Vehicles entry opens the palette with the 23 ENGINE VIDs of `objects.res` in their own groups -
locomotives (5, 10), combat cars (20, 25, 27, 30, 80, 62, 75, 90, 82, the zeppelin car 35, the
kamikaze 97, the bogie 15), supply (the ammo car 45, the repair car 85, the tankers 55 and 56), civil
(the tub 60, the passenger car 50, the goods car 95) and obstacles (the big stone 351, the invisible
railway killer 99) - named by the game's `[Units]` (27 and 99, without one, by `[Editor]`), their
icons the state bar's (`iconFrame` of the turret's weapon, else of the car's; the stone's frame 29
is a flat cell, so it gets its own sprite); All objects leaves these groups out. A car is placed
on the track: the rail link nearest to the cursor within 40 px (`g_railMap`, the screen position
y - z), facing along it - R turns the preview the other way -, created there so that
`Engine_Construct` (`Engine_PutOnRails`) puts its two ends on the rails; at a train's free end
(within 70 px) it is coupled behind it (`Engine_AttachToTrain`) and takes its army, on a car in
the middle of a train it is refused. The selected car's train: U uncouples the car from the car in
front, C couples an end car to the nearest free end of another train (the other train is laid
behind it car by car and takes its army), R / E turn the train round - `Train_Reverse` swaps its
head and toggles every car's "stands backwards" flag (engineFlags bit 0), which keeps each car facing
the way it was (nothing turned on the screen, a lone car neither); the editor toggles the flags back,
so every car faces the other way and the train's order is reversed (a car must not be left with the
flag set: the flag is not saved with the map, so after a load such a car looked turned and R did nothing
- CONVERT's start trains swap such a car's ends and clear it); a gun cart's turret (its linked object,
`link` +0x40, made with the car and not kept in the map; at rest it faces along its car) turns half a turn
with it, in the same step of the history -, a drag
moves the whole train along the track (the car to where it is dropped, the others before and
behind it in their order), Del deletes the car and Shift+Del (the Delete tool with Shift) the
train, A sets the army of the train. Its panel (the right button) has the army, TRAIN: turn round /
uncouple / couple, PATROL: none / to a point... (the next click on the track is the far end) and
DELETE: car / train. Every change keeps the couplings, both ends (`RailPos`), the turned-round flag
and the patrol of every car of the map before and after it, so undo puts every train back
(`ACT_UNDO_REMOVE`/`INSERT` of a car unlink and relink its neighbours; the couplings are set again
after them). A patrol is not a command kept in the map - `Engine_Action` has no case for
`ACT_PATROL` (36), so it would do nothing - but the script's `PatrolEngine` in main()'s editor
block: `PatrolEngine(FindNearestSprite(GETSPRITE_VID+<first car's VID>, x, y, 20), to_x, to_y);`
with the far end's screen position (`Engine_CommandToPoint`: COM_ENGINE_PATROL from where the train
stands to that point and back); a loaded map gives the block's patrols to the trains standing there.
A train needs a locomotive to drive (a tub alone stays: the missions' civil trains are a locomotive
with tubs). Saved, a car's SPRD is the missions' 40 bytes - both `RailPos` and the ids of the cars in
front and behind -, its couplings the same both ways as in all 20 missions (checked over the 197
cars). Checked in the game in Polish and English: a player's train of a locomotive and two gun cars
placed and coupled, uncoupled and coupled again, undone and redone; an enemy train (locomotive and
mortar, army set to the enemy), the big stone on the rails, a neutral locomotive with a tub on a
patrol; saved and loaded; played: the player's train driven with the mouse to the enemy, the enemy
train and fort fighting it, the neutral train going to and fro.

**Rail kinds and ramps** (the Track tool). V switches the kind of the next rails: plain (610),
swamp (600), destroyed (611) and destroyed swamp (601). A swamp rail is laid as the missions have it
(l06-l08): 28 px over the swamp's ground (the line's level is the ground + 29) on its embankment, VID
602 (TERRAIN, 37 frames like the rail, in the rail's place and direction, 28 below it), the ground
left as it is; its points stand 7 above it (the VID's altitude) where a plain rail's stand 10 - the
track rules take that into account. The destroyed rails are the death VIDs of the others:
`Rail_Action` marks their points `broken`, trains slow down there (`Options.brokenRailSpeed`) and the
repair car mends them as it passes (`Rail_RepairAt`); deleting one clears the mark of the points the
other rails keep. PgUp makes the next rail climb, PgDn descend: the same shape in a ramp frame -
frames 12-23 raise the point of end 0, 24-35 that of end 1, by sizeZ 11 (direction (shape + 12 or
24) * 256 / 37: 83, 89 ... 159 and 166 ... 242, the directions of the missions' 98 ramps of l11, l12
and l15) - with its far end raised (climbing) or its near end (descending, the rail then 11 lower);
the line goes on 11 higher or lower, the ground under the ramp slopes from one level to the other
and its bed is the bed VID in the rail's direction (the missions' ramp beds are, all but 7 of 98).
Checked: a new map on mission 6's swamp terrain with a swamp line on its embankment and a plain line
climbing a ramp onto a plateau raised with the Ground height tool, a train on each with a patrol,
`check-track` clean; played: both trains drove to and fro, over the swamp line and up and down the
ramp.

**The ground's picture, water, regions** (`src/LocoFix/editor_ground.cpp`, `editor_edit.cpp`;
MAP_RULES.md 2). The editor reads the held level's ground image into memory (its colours and its
heights, `editor_newmap.cpp`'s reader) when a Paint or Water stroke begins, and shows every change at
once by writing the changed texels into the image's own Direct3D surfaces: `GroundVid.tiles` and
`textures` - a colour texture (565, 555, 1555 or 4444, whatever `Texture_Construct` made) and its D16
height texture per 256 x 256 tile, locked and written in place. A stroke is one step of the history
(the rectangle it changed before and after, the GRID cells, the objects made or taken away). When the
map is saved, a painted picture is written as `maps\<name>\<name>.vid` (the layout of File > New's
grounds, imageFlags 0x35) and the map's ground record points at it - never over a campaign
`lNN.vid` (a campaign map saved under its own name gets `<name>_painted.vid` if the name would clash).
**Paint the ground**: the textures are 128 x 128 patches of the player's campaign grounds named by
`editor\textures.tsv` (game/editor/: the map and the place, nothing of the pictures in the repository)
- grass (l02), light grass (l15), forest floor (l07), sand (l11), desert (l10), snow (l18), ice (l16),
rock and dark rock (l20), lava (l13), swamp (l06), lake bed (l15) -, laid mirrored, the brush's edge
soft and grainy; [ and ] pick one, + and - the brush (8-200 px), Shift puts the original picture
back. **Water and lava** (W picks the kind) does what the authors did (MAP_RULES.md 2): it lowers the
picture and the GRID under the brush to the bed (the level - 25, the shore sloping), paints the bed,
adds the kind's tiles where missing on the 128 grid at the map's level of that kind, and when the
stroke ends scatters its generators (water circles 504 and noise 510, swamp bubbles 206 and circles
337, lava eruptions 441) as far apart as the missions have them and grows one region turning
explosions there into splashes on water or lava; Shift takes it back, with the tiles and generators no
longer over water. **Region**: drag its rectangle; it is picked at its dashed frame (inside it the
objects under it are), its panel sets the size, the explosions (as they are, on water, on lava), the
fog (none or the missions' mist, dust, haze, glow, set around its level), over the region or the
whole view, pulsing - all of it kept in the history (`Region_InitFog` for the fog, the VID pairs set
as `Region_Action` loads them). The palette's Effects group has the generators (water circles and
noise, bubbles, ripples, polar bears, lava eruptions, chimney smoke, sparks, fire, glows) and the
three regions. Checked in the game in Polish and English: an empty 768 x 512 map of grass in its
frame, File > New's and File > Map size's dialogs (the - and + buttons and a preset), a 1536 x 1024
empty map with a lake of light water, a lava field, a sand path, a snow patch, a region with mist and
splashes, a fire from the palette - saved, loaded (the painted picture came back from its `.vid`),
`map_tool.py check` round-tripping the map and its ground image, played: the water rippling, the lava
erupting, the fog over the region, the fire burning.

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
and lists and its ground record renamed. The map is added to `maps\custom.lst` (its title from the
script's "// title:", " 2" ... when renamed) and offered to be opened in the editor (after the
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
`maps\edithelp1.txt` ... `edithelp20.txt` - in the order a new user needs them: what the editor
is (the bar, the status line, the two buttons), the bar's tools, selecting and placing (the mouse,
undo), the keys, laying track, trains, the object's panel, all objects and decals, the mission,
saving and playing, the ground (height, paint, water, regions, map size), the ground tools' rows,
then the train arrivals and the orders (13, 14) turning (15) and the citadel, Charybdis, fly nests and mines (16; page 1 names 13-16), a factory's plan (17), the crusher, Scylla and the decorations' sets (18), the gravel bed (19), the neutrals and the pirates (20) - in the format of those pages (CP1250, CRLF, the first line the page's title, at most
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
TRAINS (5, 6), OBJECTS (7, 8, 16), MISSION, SAVING (9, 10), GROUND (11, 12), ARRIVALS, FACTORIES (13,
14, 17). The 23 rows of categories and titles (up to 37 characters) do not fit a page's 14 lines, so
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
above still name the File / Edit / Options items they were - the ids are the same. The dialogs (GDI
mode above) are not tested in full screen.
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
  `build.py` does not copy the Steam `video\` folder (`SKIPPED_DIRS`) and `game/maps/logo.lgc.patch`
  comments the four `PlayMovie` lines out, so the start is the title board alone: `game/objects.override`
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
- The strategic map of the menu has a second page with the custom maps (`maps\custom.lst`:
  the sandbox "POLIGON") and the map editor entry (`game/maps/menu.lgc.patch`; "The custom
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
  picture) and a region (a dashed rectangle).
- `editor\palette.tsv` and `vid\944pal.vid`, the editor's palette ("All objects"):
  `patch_data.make_editor_palette` reads `game/editor/palette.tsv` (the 236 kinds of object the
  missions use, by group, with the army the missions give them and the `[Editor]` key of the
  name, or none for a unit named by the game's `[Units]`) and writes it to the game folder with,
  for each object, its icon's frame and - for a unit - the behave and build list of the first one
  on the missions' maps; the icons, the object's first frame of its own sprite scaled into a 40 x
  40 atlas cell, are VID 944 (`Editor palette`, `game/objects.override`, one direction per icon).
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
      (`UpdateCheckAtStart`, off by default: the check runs as the launcher opens), a link "GitHub"
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
      (tests). Checked in the game with DLLs built as v0.1.9 (New version v0.1.10, Download: 37158157
      bytes and the release's SHA-256, the zone mark, Run Setup's question; Cancel halfway removed the
      part), v0.1.10 and v0.1.10-17-gcef9557 (the latest version) and dev, in all six languages;
      unreachable: a host that does not exist, a repository that does not exist (404) and an address
      that does not answer (the notice after 5.2 s), with the shipped `update.txt`, a replaced one
      (its notice, and its `link=` copied by the menu) and none (the built-in English); the box
      ticked checks as the launcher opens, OK saves it,
  34. the neutrals and the pirates of an editor map (`factions.cpp`, MAP_RULES.md 6d): on a map
      whose script has `#define FACTIONS 1` (the editor writes it, read from `<map>.lgc` in the
      `Level_Load` wrapper of `frame_limit.cpp`) the army test of `Object_FindTarget` (0x449f7b ...
      0x449ff3) jumps to `may_target`, and the two `ACT_DAMAGE` calls of an explosion
      (`Object_Action` 15, 0x447e9c and 0x447eb0) first call `note_hit`: a neutral hit by an army,
      and every armed neutral that sees it, answer that army - and only it - to the end of the level,
      and that army may fire at them; the pirates may fire at the neutrals. On any other map
      `may_target` is the engine's test and nothing is noted, so the missions play as before,
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
  file replaces the captured one), add font mapping if needed (`fonts/mapping/<code>.json`),
  run `python tools/fonts/generate.py --lang <code> --all`, then `python tools\check_lang.py <code>`,
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
  their links to `LocoFix.log` (test mode; `V` takes negative points too, the editor's camera
  goes past the map), `M<file>` sets the mission settings from a file instead of the dialog
  (test mode; the format is in `editor_mission.cpp`), `N<name>:<line>[:<w>:<h>]` makes a new map as File >
  New would with the dialog's ground line (test mode), `Z<w>:<h>` resizes the map as File > Map size (test mode), `^<code>` / `+<code>` press a key with
  Ctrl / Shift (`^90` = Ctrl+Z), `K<code>` / `U<code>` only press / only let go a key (`K16`
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
  direction, joints with rails of every shape (`python tools\test_rail_rules.py [cases]`, 3000 by
  default; every kind of rule failure comes up).
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

## Status and known bugs

Smoke test (`test_missions.ps1`): all 20 missions load in windowed mode, mission scripts
compile without errors, the game closes normally.

All bugs found - fixed, confirmed, suspected, quirks scripts may rely on - are listed in
`BUGS.md`. The rules a map has to follow - track, junctions, the ends of the main line,
one-way sections, buildings, semaphores, the mission script - are collected in `MAP_RULES.md`,
the specification for a map editor. New findings go there first; a suspected bug is fixed only once the code around it
is understood, except obvious ones with a small, safe fix.

## Release

The first release, 0.1, is a test release on the GitHub Releases page: one `Setup.exe` built from the same
`build.py` code, with nothing of the game in it - the game is assembled on the player's machine
from their own Steam copy, as `build.py` does it for a developer.

```bat
.venv\Scripts\pip install -r tools\requirements-release.txt
.venv\Scripts\python tools\release.py
```

`tools/release.py` writes `build\release\Setup.exe` (about 25 MB):

1. the version is `git describe --tags --always --dirty` (the short hash while there is no tag);
   it is in Setup's title, in `OpenLocoland.json` of the install, in the first line of
   `LocoFix.log` ("LocoFix <version>") and in the launcher's caption ("... - OpenLocoland
   <version>"; a `build.py` build says "dev"),
2. LocoFix.dll is compiled with that version (`build_locofix.build(out_dir, version)`, the define
   `LOCOFIX_VERSION`, into `build\release\LocoFix`) - the player gets it prebuilt,
3. the payload (`build\release\payload`) has our files in the repository's layout: `game\`,
   `lang\` and `tools\` (only files git knows; the packs stay in the `lang/<code>/game/` layout
   and are assembled at install time, `lang\_template` is carried but not built),
   `external\dgVoodoo2\` (the two DLLs and the notice),
   `LocoFix.dll` and `version.txt`; a file with an extension of the game's files (`.exe`, `.res`,
   `.vid`, `.map`, `.bmp`, `.wav`, `.ogg` ...) stops the release,
4. PyInstaller (`--onefile --windowed`) freezes `tools/installer.py` - the window - with
   `build.py` and every module it imports (Pillow for the generated pictures included) and the
   payload, which the frozen modules find in `sys._MEIPASS` (`build.REPO`, `languages.REPO`).

The launcher's update check (LocoFix fix 33) reads the release GitHub calls the latest - the newest
that is neither a draft nor a pre-release - so a release is found by it when its tag is
`vX.Y.Z` (compared by the numbers) and its Setup is an asset whose name ends with `Setup.exe`.

The icon of Setup.exe (the file and its window) is `tools/icon/openlocoland.ico`: a golden cog in the
brass look of the game, our own drawing - `tools/make_icon.py` writes it as an SVG
(`tools/icon/openlocoland.svg`), renders it with a headless Chrome or Edge and packs the Windows
sizes; the cog is plain (its rings and the axle, no windows) so it reads down to 16 px.

The contents can be listed with `.venv\Scripts\pyi-archive_viewer --list build\release\Setup.exe`:
the Python runtime, Tcl/Tk, Pillow, the payload - no game file.

**What Setup does** (`tools/installer.py`): it finds the game through Steam's registry key
(`HKCU\Software\Valve\Steam` `SteamPath`, then HKLM `InstallPath`) and the libraries of
`steamapps\libraryfolders.vdf` (`steamapps\common\Locoland\Steam.exe`), or takes a folder chosen
by hand; it refuses an install folder that is the Steam game folder or inside it, asks before
using a non-empty folder without an install, and refuses while `Locoland.exe` of that folder
runs. Then `build.build(steam, target, locofix=<the prebuilt DLL>, captured=...)` (the same steps
as for a developer, the log and a progress bar in the window), the two options into the registry
(`Cheats`, `UnlockMissions` next to LocoFix's settings - Setup shows their current values, so
running it again keeps them), the game's language (`Language`: a new install sets the one chosen -
the system's display language when there is a pack for it, else English; an update writes it only
when it was changed in the window, so the language chosen in the game's launcher stays), the
shortcuts (through PowerShell's `WScript.Shell`; the editor's
runs `Locoland.exe --editor --test-map maps\template\template.map` - the template is what the
menu's "EDYTOR MAP" opens, and both open a new empty map in its place) and `OpenLocoland.json` in the install folder (the
version, the Steam folder, the captures' folder, the shortcuts, so an update or uninstall
removes the old ones, and `files`, what the build installed, for the next update's clean-up - see
"Updating"). The language captures go to `%LOCALAPPDATA%\OpenLocoland\captured`
(`build.py --captured`), not into a repository: the language Steam installed and every depot
downloaded with `download_depot` (`languages.downloaded_depots`: `steamapps\content\app_352130`
of the game's library and of Steam's own folder, where the console puts them). The "Game
language" list (a menu, so entries can be greyed) shows every pack of `lang\`: those whose
language is captured or can be captured (`languages.available`) can be chosen - a translation
Steam does not ship (Czech) on its `base=` or, without it, on any captured language
(`languages.build_base`) -, the others are greyed with the reason beside them ("needs the Steam
depot: download_depot 352130 352132"; for a fan pack with `needs_base=1` "needs the files of
English: ..."), fan translations carry "(unofficial)" in the window's language, and the log
below keeps the `download_depot` lines. So a pack added to `lang\<code>\` is picked up by the
next release. `--captured <folder>` keeps the captures in another folder (tests with only some
languages; Uninstall leaves that folder). Steam's Portuguese depot is the English
game byte for byte (checked on the captures), so it is captured but has no pack. The window
itself has its texts in the languages of `TEXTS` (Steam's six and Czech; the list is made from it,
English first, and the system's display language picks the first one), and its header is our own
OpenLocoland wordmark (`tools\icon\openlocoland_logo.png`, transparent, cut to the letters) drawn
on black across the whole width of the window and redrawn when it is resized (PIL at run time:
nothing of the game is in Setup.exe); without that file it is the game's `vid\logo.bmp` cropped to
its letters - the Steam copy's, or another captured language's when Steam has the English one,
which says "Steamland" - and before a game is found it is the project's name in text. `Setup.exe --silent --target <folder> [--steam
<folder>] [--cheats 0|1] [--unlock 0|1] [--game-lang <code>] [--no-shortcuts] [--lang <code>] [--no-clean]`
(`--lang` picks the language of the shortcuts' descriptions; both default as in the window) does the same without
the window, with its log in `logs\install.log` of the install folder and exit code 0 when it worked;
`Setup.exe --auto [--target <folder>] [--no-shortcuts]` opens the window, which installs at once with what it
shows and closes at the end (the same log and exit code - a test of the window
itself, since Tk's buttons take no posted clicks). The Install / Update button has the focus,
so Enter or Space installs.

**Updating** is running a newer Setup over the install: `build.py` copies only what changed and
leaves alone what it does not write, so the player keeps the high score tables (not
overwritten once the game wrote them), the maps made in the editor (`maps\<name>\`), the lines
the editor added to `maps\custom.lst` (merged with the repository's list, see "The custom maps
page"), the exported and imported maps, and everything in the registry: the settings, the
options as they were and the campaign progress (`Players`). The two maps of the release itself,
`maps\sandbox` and `maps\template`, are written anew - a map edited in place there is lost, a map
saved under a name of its own is not.

What earlier versions left and this one does not write is removed after the build (`build.py`
`clean_up`), each file named in `logs\install.log` ("removed <path> (<reason>)"); `--no-clean` (of
Setup and of `build.py`) skips it. The candidates:

- the files the previous install's `OpenLocoland.json` lists under `files`: every file the build
  wrote or found up to date (Setup records the list from this version on, so the next update removes
  exactly what a version stopped writing - a help page, a pack file, a file of one of our maps),
- for installs made before that record, the known leftovers: `OBSOLETE` (`Steam.exe`, `steam.cfg`,
  `steam_appid.txt` - copied by 0.1 -, `D3D8.dll`, `D3D9.dll`, `dgVoodooCpl.exe`), the movies of
  `video\` (the Steam intros 0.1 copied and the logo clip `openlocoland.wmv` of the test builds after
  v0.2.0), `DEV_FILES` (`run_map.cmd`, `edit_map.cmd`; not with `--dev`), the files
  of `lang\` and `editor\` the build did not write (an older pack layout, e.g.
  `lang\cs\maps\l08\brief2.txt` of v0.1.10), help pages `maps\edithelp*.txt` / `.lst` beyond the
  current ones, and files of the Steam folder's root an older build copied (the same size, not ours),
- the editor's scratch: `maps\_new*\` folders marked with `editor_new.txt` and `*.saving` files (a
  save that did not finish, `level_save.cpp`) - in the player's map folders too,
- the logs of the root, which move into `logs\` (one `logs\` has already is removed).

Guards, checked for every candidate: the candidates are computed only when the build has finished,
and nothing it wrote in this run is one (so no game file copied or patched from Steam, no DLL we
ship); the path must resolve inside the install folder - not the folder itself, not through a link
or junction, not inside the Steam folder (and nothing is removed when the two overlap), so a list
naming `..\x` or an absolute path is refused; the player's data is excluded by name whatever a list
says: the high scores (`hiscores`, `hiscores.txt`), `maps\custom.lst`, `OpenLocoland.json`,
`Locoland.cfg` and `logs\`; and a `lang\<code>\` or `maps\<name>\` the build wrote nothing into this
time - a player's map, a pack whose language could not be built - is left alone (apart from the
scratch above), so a missing capture never takes a working pack with it. Folders the removals leave
empty go too. The settings and the progress are in the registry, which the clean-up never touches.

**Uninstalling**: the Uninstall button of Setup (after a question) removes the shortcuts, the
install folder with everything in it and `%LOCALAPPDATA%\OpenLocoland`; deleting the install
folder by hand does the same apart from the shortcuts and the captures. The registry is left as
it is: `HKCU\SOFTWARE\Gromada\Locoland` holds the settings and the player profiles, which the
Steam game reads too.

**Checked** (Setup.exe of this commit, PATH without Python and Visual Studio): a silent install
into a new folder (11 s), `test_missions.ps1` (all 20 missions) on it, the custom maps page with
POLIGON, "EDYTOR MAP" of the menu and the editor's desktop shortcut opening the template in the
editor, both options on and off in the installed game (above), a map made and saved in the editor
(`maps\reltest`, its `custom.lst` line), then an update through the window (`--auto`): the map's
files and `custom.lst` byte for byte the same, the options as they were (both on), the settings and
a test player's progress and score unchanged; Uninstall of a second install (folder, captures).
Then the window's languages and the game language: a new install set `Language` to `en` and built
the en, hu and pl packs from the captures there were (de and ru listed with their
`download_depot` lines), mission 2 showing its English objectives; an update of the other install
kept `pl`, kept the high score tables (one marked by hand stayed as it was) and passed all 20
missions.

**By hand**, the player may have to: allow the unsigned `Setup.exe` past SmartScreen ("More info"
> "Run anyway") or an antivirus that flags PyInstaller programs; download another language's
Steam depot (`download_depot`, "Quick start" above; Setup's window lists the lines) and run Setup
again to get its pack - Setup captures only the language Steam has installed and the depots
downloaded, and the launcher lists only the packs that were built.

## Distribution

The repository is public. What publishing the fixes rests on (checked October 2026; this is
not legal advice):

**Rights holders.** Steam lists Gromada and ESDigital Games as developers and ESDigital Games
as publisher, "©2015 ESDigital Games Ltd", with the trademark shared with Alawar
Entertainment. Steam ships six languages: English, German, Hungarian, Polish, Portuguese,
Russian.

**Working on one's own copy.** EU law (Directive 2009/24/EC, in Poland art. 75 of the
copyright act) lets the lawful user decompile a program to correct its errors (art. 5(1);
CJEU C-13/20 *Top System*) and for interoperability with other programs (art. 6; contracts
cannot override it, art. 8). Making the game run on current Windows and fixing its bugs on
a bought copy is that case. The Steam Subscriber Agreement forbids reverse engineering
without consent; in the EU such clauses cannot remove those rights.

**What can be shared** - our own work, the way fan patches like the Widescreen Fixes Pack and
the early OpenRCT2 are shared:
- LocoFix.dll and its source, the tools, the documentation, `symbols.csv`, `types.txt`;
- an installer that applies the exe and data patches to the player's own Steam copy at
  install time (what `build.py` does) - `Setup.exe` of "Release";
- dgVoodoo2 `DDraw.dll` and `D3DImm.dll` unmodified - its readme (2.87.5) allows shipping
  "a game or game mod with unmodified individual dgVoodoo components"; credit Dege.

**What must not be shared** without the rights holder's permission:
- a patched `Locoland.exe` or any other game file;
- original language files (texts, fonts, logo) - packs are captured on the player's machine
  from their own install (`captured/`), and our translation work is shipped as changes
  (`lang/`);
- modified game scripts as whole files - shipped as patches (`game/`, `lang/`);
- `re/c/`, `re/exe/`, `re/data/` - output derived directly from the exe and data; generated
  locally with `decompile.py` and `unpack.py`.

The working tree follows these rules: game files and decompiler output are in `.gitignore`,
our changes are patches (with a line of original context around each change) and override
files. The repository's history starts from such a tree, so it holds no game file either.

**Published that way:** the README states that the project is unofficial and needs the
original game. Still to do: ideally ask ESDigital Games for permission - or for the
source code, the way Warzone 2100 was released. The name uses the Locoland trademark the
way OpenTTD and OpenRCT2 do; if the owners object, it can change.

## Next

1. Code reconnaissance of the engine is complete: all 993 functions in the engine range
   (0x00401000..0x00456a00) are named (100% of the engine bytes). The remaining
   unnamed functions (442) are CRT/other (DirectDraw, DirectSound, CRT) outside the
   engine range. Next: rewriting understood functions in LocoFix. The script value
   helpers are the start (`String_ToInt`, `ScriptValue_BinaryOp`, `ScriptValue_ToString`,
   `World_PopInt`, `World_PopString`, `World_PopObject`, `World_PopVid` and
   `Script_PopObject`, with a differential test in `tools\test_script_value.py`); the
   push functions of the script stack (they grow the stack with the game's vector) and
   then the effects are next. The audio decoder at
   0x455260..0x456640 is named `SoundStream_*` (the bundled OGG Vorbis decoder that
   `Sound_LoadSound` uses); only 0x4551d0 (`GroundVid2_InitLayer`) is a `GroundVid2`
   vtable slot. The mission smoke test no longer clicks through the menu: the game is
   started with `--test-map <map>` (LocoFix, `test_map.cpp`), so each mission's map and
   script are loaded straight away and `LocoFix.log` says whether the map really loaded.
   The GUI test path still exists (`test_game.ps1 -Menu`, `t` for tooltips) for the menu
   itself.
2. More LocoFix fixes: bugs found while playing; optionally scaling the
   interface up at high resolutions.
3. Fixes from `BUGS.md` and bugs found while playing.
4. Map editor and mission template, on top of `map_tool.py` (maps and images are readable
   and writable; `objects.res` with `res_tool.py`). Whole new maps work: `map_tool.py new`
   makes one from a template - the terrain, an empty object list and a script that runs -
   and `build.py` packs `game/maps.new/<name>/` into the game (`template` is there, and
   `test_game.ps1 -Map maps\template\template.map` loads it). A map is edited with
   `list`, `vids` and `place` (an object with the state of an object of the same VID),
   and `ground` writes the empty picture of a ground, so a map can have a terrain of its
   own. `track` lays rails the way the engine links them and `place` checks the factory,
   repair point and fort rules. `game/maps.new/sandbox` is a whole mission made this way and
   checked in the game: every part of its track is driven (`SANDBOX_DRIVE_TEST`), the factory
   sends its trains out, the repair point repairs the car nearest to it, and the mission ends in
   a win or a loss with its own texts (`SANDBOX_WIN_TEST`, `SANDBOX_LOSE_TEST`). It is played
   from the custom maps page of the menu, whose "EDYTOR MAP" entry (`StartEditor()`) opens the
   template in the engine's own editor mode ("Editor mode": the game restarts into it, the
   level held, objects selected, moved, turned, deleted with undo and placed from a bar, safe
   saving, Play the map to try it, track laid on the rail lattice by the rules of map_tool.py).
   Next: the mission settings (push lines, the level entry and exit, semaphore colours from them,
   money, enemy trains, texts, the script), the meaning of the command (`ACT_...`), engine rail and region
   data in `objects.tsv`, and terrain with heights (the sandbox is flat; ramps are the rail
   frame groups 1 and 2, see "Trains and rails").
