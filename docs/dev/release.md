# Release, distribution, next steps

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

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
   `LocoFix.log` ("LocoFix <version>"), in the launcher's caption and in the game window's title
   ("... - OpenLocoland <version>", `scaling_wndproc` at WM_CREATE; a `build.py` build says "dev"),
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

**Every edition before the tag.** `python tools\editions.py check --run` builds the game from
every edition of it we know and starts each one (an hour or so for all of them); a release waits
until its table has no FAILED, "no menu" or LOGIC. `tools/editions.py` is a developer's tool, not
part of Setup. It keeps a manifest of each edition - `tools/editions/<code>.tsv`, every file with
its size and SHA-256 (in git: nothing of the game) - and the files in a local store,
`editions/<code>/` (outside git, as `captured/`): `en-steam`, the base, whole (about 550 MB), the
others only the files that differ from it. The codes are `<language>-<source>`: `en-steam`,
`de-steam`, `hu-steam`, `pl-steam`, `pt-steam`, `ru-steam` - the depots, which `download_depot
352130 3521NN` in the Steam console puts into `steamapps\content\app_352130\depot_3521NN` (see
"Languages") - and the discs, `cs-cd` (the Czech edition), `pl-cd` (the Polish CD 1.02: the
Polish depot's files with a `license.txt` and a `readme.txt`), `it-cd` (the Italian one: the
English depot's scripts and data with Italian texts, and an exe of its own, wrapped by a copy
protection that ends it at once without the disc) and `en-beta-demo` (the English beta/demo disc:
another exe; its `logo.lgc` and `constants.lgc` are the Russian depot's, which so has part of
the scripts of that older build). A new edition is one folder of its files - an install of the
disc, by our convention in `C:\Locoland Decomp\notes\diffs\<name>` - and `editions.py capture
<code> <folder>`; an installer's own files (`unins*`, logs) and a disc install's score tables
(`hiscores`, the player's; the depots ship theirs) are left out, a file that cannot be read (the
Italian CD's `CDPRO.BIN`, taken by the antivirus) is named and left out, and `refresh <code>
<folder>` captures again and says what changed (a Steam update). `list` shows the editions
(language, source, exe, files, how many differ from `en-steam`, the nearest other edition with
the files they have with other content and the files only this one has, when and where from -
the manifest says only "steam depot 352136" or the folder's name, the full path is kept beside
the store, `editions/<code>.source.txt`),
`diff <a> <b>` the files that differ, marking our patches' targets, and `assemble <code>
<folder>` puts a whole edition together (hard links into the store). `check [--run] [<code>
...]` assembles each into `build\editions\<code>\steam`, reports for each of our patches whether
it is placed at its line numbers, shifted or not at all, builds it with `build.py` into
`build\editions\<code>\game` (with `--skip-verify` only when the build is refused; its log next
to it) and with `--run` starts it with `test_game.ps1`: the intro to the main menu, and POLIGON
with its Esc menu where LocoFix runs (the Steam exe), looking for LOGIC errors in `error.log`. An
edition with another exe is installed as it is ("Quick start": none of our patches goes in), so
its patches column says "as is" instead of counting them, POLIGON is "n/a (no LocoFix)", and the
wait for the menu is 300 s instead of 60 (its own intro movies play first). The errors column counts
the `!!!ERROR` lines of the menu's run (the most common kinds are printed under each edition) and
names any "Unknown extern" or `custom.lst` line, which would mean our scripts went in without
LocoFix - that fails the check, as FAILED, "no menu" and LOGIC do. An edition's own exe that ends
before the engine starts (no `error.log` at all: the Italian CD without its disc) is "did not start
(its exe)", a limit of that edition, not a failure. On 2026-11-06 all ten built (the beta/demo and
the Italian CD with `--skip-verify`, for their exes), every patch was placed - at its line numbers
but the Russian `menutact.lgc`'s, a line lower - and each reached the main menu but the Italian
CD, POLIGON too on the Steam exe, with no LOGIC error; again after the as-is install: the beta/demo
and the Italian CD "as is", the beta reaching the menu with 36 `!!!ERROR` lines, all the
original's own (sounds its `wav\` lacks, the fort cannon's frames, its old-format `logo.map` and
`menu.map`, a movie it lacks), en-steam and ru-steam as before.

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
overwritten once the game wrote them), the maps made in the editor (`maps\<name>\`, which the custom maps page finds by itself -
`maps\custom.lst` is LocoFix's list of them, which the build no longer writes), the exported and imported maps, and everything in the registry: the settings, the
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
