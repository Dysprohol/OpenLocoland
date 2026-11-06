# Quick start and repository layout

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

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
it (on by default; unticked, it stays off) checks as the launcher opens, at most once a day.
Nothing is updated by itself.

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
game files; Setup says it in its own languages). The files our patches change (the targets of
`game/**/*.patch` and of the script patches of `lang/<code>/`) are checked before anything is
built: a file with the SHA-256 in `build.py` (`ORIGINAL_SHA256`: these files are byte for byte the
same in the English, German, Hungarian, Polish and Portuguese depots) takes the patches as it is; any other is
fine as long as every patch that targets it can be placed, and the log says so once ("maps\logo.lgc:
not the known Steam file, our changes still apply"). The Russian depot is such a copy: four of its
scripts differ (`BUGS.md`, "Running on current Windows"). `patch_text.apply_patch` places each
change (hunk) by its lines, byte for byte - at its line number, else where they occur exactly
once in the file (the Russian `menutact.lgc` has an empty line more at its top, so all nine of its
changes land a line lower) - and our patches avoid lines a depot changes: `logo.lgc.patch` does
not touch the movies' lines, whose clip names differ (below, "the intro"). When a change cannot be
placed (its lines are not there, or twice), `build.py` and Setup stop and name the file and the
change (`build.modified_steam_files`, the one decision for both; Setup's `modified_files` text
asks to verify the game files in Steam). Setup's "Skip verification of the game files - install as it is" box (`--skip-verify`
of `build.py` and Setup) goes on instead: such a file is left as the player's, and the log and the
window say which of our features it carries will be missing (`build.FEATURES`; Setup's
`feature_*` texts). A `Steam.exe` of another version (the Italian CD's, wrapped by a copy
protection, or the English beta/demo's) is installed as it is (`build.build_as_is`): our exe
patches and LocoFix fit only the Steam exe (LocoFix, loaded by a patched exe only, also checks the
exe's build date and image size and does nothing on another), and everything else of ours needs
LocoFix - the patched scripts call its externs 200-206 (the custom maps page, the editor's entry:
on the beta they filled `error.log` with thousands of "Unknown extern" and "couldn't open
maps\custom.lst" lines) or include its option files (`cheats.lgc`), the added VIDs of
`game/objects.override` and their pictures are drawn by it, the new maps of `game/maps.new` are
reached only through its custom maps page or `--test-map`, the packs of `lang\` are read only
through it (`language.cpp`), and the editor's help pages, data and shortcut are its editor's. So
such a game gets the copy's own files unchanged - its intros (`video\`), its own language at the
root (not the English capture), its `objects.res` and `vid\110.vid` without our data fixes, the
scripts and maps as they are - with `Steam.exe` as `Locoland.exe`, dgVoodoo2 with `dgVoodoo.conf`,
`Locoland.cfg` and `logs\`; nothing is captured, Setup writes none of LocoFix's settings (the
options, the game's language) and makes only the game's shortcut. The log says it once
("Steam.exe is not the Steam version of 2003-10-07: installed as it is - no fixes, launcher
options, editor, custom maps page", and what was left out; `build.AS_IS`), and Setup's window
ends with the same line in its language (`as_is`). So a player with the game's files can always
install. An old way to turn on the developer keys was
to edit `maps\script\constants.lgc` in the Steam folder, and the patch then stopped Setup with a
Python error: a `#define KEYS_ENABLE` line there is left out by itself - the launcher's Cheats
option does that job now. Of the Steam folder's root only the game's own
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
Portugal is a copy of 352131 English, every file byte for byte (`tools\editions.py diff en-steam
pt-steam`, 2026-11-06); so there is no `lang/pt`.) Steam
puts them in `steamapps\content\app_352130\depot_<id>\`. `build.py` captures the language
files of the installed game and of every downloaded depot into `captured/<code>/` (outside
git) and assembles the packs that have a capture (a fan translation such as Czech: its base's,
else any captured language - "Czech" below); the others are skipped with the command to
download them. The Czech edition of the game sold on CD ("Steamland") is captured the same way
when its files are at hand - the Steam folder itself when it is the edition, or a folder named
`Steamland` in or next to it ("The Czech edition" below). The depot folders can be deleted after the build; keep `captured/`. A
checkout without `captured/` (a new worktree) uses the installer's captures in
`%LOCALAPPDATA%\OpenLocoland\captured` when there are any (`languages.CAPTURED`); `--captured`
of `build.py` and `check_lang.py` names another folder.
`python tools\languages.py capture [<code>] [--force]` captures the installed language by
hand (`--force` replaces an existing capture, e.g. after a Steam update; `--steam <folder>`
another folder, e.g. the Czech edition's).

For working on the engine, generate the local reverse-engineering output (not in git):
`.venv\Scripts\python tools\unpack.py` (`re/exe`, `re/data`) and
`.venv\Scripts\python tools\decompile.py` (`re/c`, needs Ghidra).

## Repository layout

| Folder | Contents |
|---|---|
| `game/` | Our files for the game folder (`Locoland.cfg`, `dgVoodoo.conf`) and changes to original files as `<file>.patch` (`maps/logo.lgc.patch`, `maps/menu.lgc.patch`) - same structure as the game folder; `maps/script/cheats.lgc`, `unlock.lgc` and their `_on` variants are the launcher's options (LocoFix fix 30); `update.txt` is the launcher's notice when the update check cannot reach GitHub (fix 33). `objects.override/` would hold changed values of `objects.res` (see "Editing objects.res"), `maps.override/<map>/` changed objects of a map (`l11`: a rail of mission 11, `BUGS.md` F21) and `maps.new/<map>/` whole new maps (see "Editing maps and images"); `maps.new/template` (the empty template) and `maps.new/sandbox` (the one to play) at the moment, and `run_map.cmd` - a double-click at the game starts the sample map, `run_map <map>` any map; `edit_map.cmd` the same in editor mode (see "Editor mode"); these two are developer helpers, copied only by `build.py --dev`. |
| `docs/` | The player's guide for the release, in Polish and English only (the German, Hungarian and Russian players get the English one): `PRZEWODNIK.md` (Polish) and `GUIDE.md` (English) - installing, what is new in the game, the custom maps page, the map editor with a tutorial and its keys - and a few screenshots of the game in `docs/img/`. Version 0.2.9; keep the guides in step with the in-game help pages (`maps/edithelp*.txt`) and "Editor mode" below. |
| `lang/<code>/` | Our translation work for each language pack (`cs`, `de`, `en`, `hu`, `pl`, `ru`): `language.ini` (the launcher's name and labels, `codepage`, `base` for a translation Steam does not ship) and in `game/` the texts by kind, each assembled into its place in `<game>\lang\<code>\` (`languages.pack_path`): `game/strings.override.ini` -> `strings.ini` (changes to the captured file), `game/briefings/<map>/` -> `maps\<map>\` (the missions', POLIGON's and the template's texts), `game/menus/menu*.txt` -> `maps\` (the menu's pages), `game/editor/edithelp*.txt` -> `maps\` (the editor's help pages), `game/scripts/*.lgc.patch` -> `maps\script\` and `game/scripts/shiftb.txt.patch` -> `maps\` (the developer mode); and in `fonts/` a pack's own letters, `<font>.png` pasted into the captured `fonts\<font>*.bmp` (`tools/font_cells.py`, Czech - see "Languages"). A pack ships only what it changes; the rest comes from its capture. `lang/_template/` is the starting point of a new translation (a placeholder for every file and `README.md` notes per folder) and not a pack: `languages.packs()` leaves out folders starting with `_`, and no `README.md` is shipped. Each pack's texts are in its code page (CP1250/1251/1252); format details in `docs/TRANSLATING.md` and `tools/patch_text.py`. |
| `docs/dev/` | This developer documentation, one file per area; `DEVELOPERS.md` is its index. |
| `.github/workflows/ci.yml` | CI on every push and pull request: builds `LocoFix.dll` (MSVC x86) and runs the tests that need no copy of the game. |
| `external/dgVoodoo2/` | dgVoodoo2 2.87.5 (`DDraw.dll`, `D3DImm.dll`, unmodified; credit in `NOTICE.md`). |
| `src/LocoFix/` | Game fixes in C++ - a library loaded by the exe before WinMain. |
| `tools/` | Game assembly, language capture, text/exe/data patches, LocoFix build, decompilation, data unpacking, editing of objects.res, maps and images, tests; the installer (`installer.py`) and the release (`release.py`, and `editions.py`, the editions every release is checked on - see "Release"); the translator's kit (`translate_kit.py`: a pack's texts exported as UTF-8 files with the English beside them for a volunteer without git, and imported back checked and committed with the translator as the author - `docs/TRANSLATING.md` "For translators without git"). |
| `re/symbols.csv` | Names and descriptions of functions and variables in the exe - everything we learn about the code goes here. |
| `re/types.txt` | Struct types (fields at their offsets), vtables, types of globals and function prototypes. |

Local only (in `.gitignore`), made from the player's own copy:

| Folder | Contents |
|---|---|
| `captured/<code>/` | Original language files captured from Steam, one folder per language (`languages.py`). |
| `editions/<code>/` | The files of the game's editions for `tools/editions.py` (en-steam whole, the others what differs from it; their manifests, in git, are `tools/editions/<code>.tsv`; see "Release"). |
| `re/c/` | C code from Ghidra (`decompile.py`, with names from `symbols.csv` and types from `types.txt`; `types.h` lists the structs). |
| `re/exe/`, `re/data/` | Imports, strings, dialogs of the exe; objects and weapons from `objects.res` (`unpack.py`). |
| `build/` | Ghidra project, LocoFix build, test results, previews. |
