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
   `LocoFix.dll`, `version.txt` and `payload.txt` (the list of the payload's files, which Setup checks before it builds); a file with an extension of the game's files (`.exe`, `.res`,
   `.vid`, `.map`, `.bmp`, `.wav`, `.ogg` ...) stops the release,
4. PyInstaller (`--onefile --windowed --noupx`) freezes `tools/installer.py` - the window - with
   `build.py` and every module it imports (Pillow for the generated pictures included) and the
   payload, which the frozen modules find in `sys._MEIPASS` (`build.REPO`, `languages.REPO`). The
   exe gets a Windows version resource (`release.version_resource`: the numbers of the tag -
   v0.2.11 is 0.2.11.0, three commits after it 0.2.11.3 -, FileDescription "OpenLocoland <version>
   Setup", ProductName "OpenLocoland Setup", CompanyName "OpenLocoland", OriginalFilename
   `OpenLocoland-<version>-Setup.exe`: the project, never a person) and its own manifest
   (`release.MANIFEST`: PyInstaller's - asInvoker, the Windows versions, long paths, the common
   controls - and system DPI awareness, so on a scaled screen the window is drawn sharp instead
   of stretched; the window's few sizes in pixels follow the scale, `Installer.px`); no UPX.

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
the Python runtime, Tcl/Tk, Pillow, the payload - no game file. At every start the one-file exe's
bootloader unpacks them (1626 files, 47 MB) into `%TEMP%\_MEI<n>` and runs Python from there in a
second process of the same exe, which it waits for and which removes the folder at the end (a
killed Setup leaves it). The Python modules stay in an archive inside the exe, which PyInstaller
reads each one from the first time it is imported; Setup reads that archive into memory at the
start (`installer.keep_modules_in_memory`, about 3 MB; `install.log`: "Setup's modules: 2.9 MB read
into memory at the start"), so after the start it never reads its own exe again - v0.2.10 stopped
half way with "OSError: [Errno 22] Invalid argument: '...Setup.exe'" when a player's Defender took
the exe while it ran, at the first module imported after that. Setup starts no other program: the
shortcuts and the check whether the game runs are made in its own process (below), Play starts
the game through the shell, the GitHub link opens the browser through it - both only on a click.

**What Setup does** (`tools/installer.py`): it finds the game through Steam's registry key
(`HKCU\Software\Valve\Steam` `SteamPath`, then HKLM `InstallPath`) and the libraries of
`steamapps\libraryfolders.vdf` (`steamapps\common\Locoland\Steam.exe`), or takes a folder chosen
by hand; it refuses an install folder that is the Steam game folder or inside it, asks before
using a non-empty folder without an install, and refuses while `Locoland.exe` of that folder
runs (the process list and each process's image path through the Windows API,
`installer.running_images`). Then `build.build(steam, <target>\.openlocoland-setup\new,
locofix=<the prebuilt DLL>, captured=...)` (the same steps as for a developer, the log and a
progress bar in the window) and the build's files put into the install folder at the end (see "A
run stopped half way"), the clean-up of what earlier versions left (`build.clean_up` on the
install folder, as before), the two options into the registry
(`Cheats`, `UnlockMissions` next to LocoFix's settings - Setup shows their current values, so
running it again keeps them), the game's language (`Language`: a new install sets the one chosen -
the system's display language when there is a pack for it, else English; an update writes it only
when it was changed in the window, so the language chosen in the game's launcher stays), the
shortcuts (in Setup's own process through the shell's ShellLink object - IShellLinkW and
IPersistFile through `ctypes`, `installer.make_shortcut`: the same calls WScript.Shell makes, and the
`.lnk` files are byte for byte the ones PowerShell's `WScript.Shell` made up to v0.2.10; the editor's
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
so Enter or Space installs. The box "Skip verification of the game files - install as it is" says before the install what it
gives up (`skip_verify_hint`, seven languages): in a hint when the mouse rests on it (a small Tk
window, `installer.Tooltip`) and, while it is ticked, in the first lines of the log area - the window
does not grow for it; its label is as before.

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

**A run stopped half way** - killed, the computer switched off, or a file of Setup taken by an
antivirus - leaves a game that starts: the old version whole, or (only when it was stopped in
the last moment) a half put in one that the next run finishes. Setup builds the whole game into
`.openlocoland-setup\new-<n>` inside the install folder, which nothing of the game reads; only when
that build has finished, the files that differ from the install's (`build.copy`'s test: the same
size and date, or the same content; the high score tables of an install stay the player's) go in
(`installer.commit`): their list is written first as a journal (`.openlocoland-setup\commit.json`,
flushed to the disk), then each file goes in with one rename on the same disk - milliseconds for
the whole lot; a read-only file of the install, as a game from a CD or a read-only copy has them, is
replaced as any other -, the folders the build made are made (`logs\` and any empty one: the clean-up
moves the logs of the root into `logs\`), and the folder is removed, read-only files too (their mark
cleared); one that still cannot be removed does not fail the install - the log says so, the next run
tries again and builds into a new `new-<n>` meanwhile. After that come the clean-up, the options, the
shortcuts and, last, `OpenLocoland.json` (through a temporary file and a rename, so it is never
half written). The next run of this Setup or a later one starts with what an earlier run left
(`installer.finish_interrupted`, a line in the log): without a journal (stopped while building) the
prepared files are removed - the game was not changed -, with one (stopped while renaming) the
rest is put in from the journal, so the game is that version's whole again before this run builds
its own (a failure while doing that says the game is half put in, not as it was); a journal older
than the record (another Setup finished an install there since) is dropped. The files it puts in
belong to no record until this run writes its own: when this run builds the same files (the same
version run again, the usual case) nothing is left over, and the clean-up takes them as files of the
previous record, so one this run does not write (another version's) is removed as any file a version
stopped writing. An older Setup (v0.2.10 and before) knows nothing of the folder and repairs by building
over the game, as any update does; the folder stays until a newer Setup or Uninstall removes it.
Updates take longer than before - the whole game is copied into the work folder: v0.2.9 -> this
version through the window (`--auto`) took 24 s instead of v0.2.10's 15 s here - and need the
game's size free for a moment: before the build Setup checks the free space of the install's disk
against the size of the Steam folder's files and 64 MB, and stops with a plain sentence when it is
short (`no_space`, the seven languages of the window; `--test-free-mb <n>`, tests only, sets the free
space it sees). A run that fails before the commit removes its work folder itself. When Setup cannot
read a file of its own - its exe, or what its bootloader unpacked (`installer.own_file`: the
error's file, or the first of the files listed at the start that is gone) - it says so in the
window's language instead of the Python error (`damaged`: an antivirus may have taken Setup.exe
or its files, download Setup again), with the error itself in brackets, and every failure ends
with the state of the game (`kept`: as it was before this run; `resume`: stopped half way, run
Setup again). The log of `--silent` is written line by line, so a killed run leaves it up to that
point (the window's log, `--auto`, is written at the end, as before). `--test-stop built |
before-journal | moving | record` (tests, not in `--help`) ends the process hard at that point.

**The package's shape.** An installed game updates itself through the launcher (LocoFix fix 33,
`update_check.cpp`): it takes, of the latest release's assets, the first whose
`browser_download_url` is an https address ending with `Setup.exe`, checks its size and SHA-256
against the release's, saves it in the Downloads folder under its own name with the internet zone
mark, and on Run Setup starts it with `ShellExecuteW(open)` - no arguments, no other file - and
closes the game. So every release needs one exe ending with `Setup.exe` that works alone, as long
as installs of v0.2.0 and later are to update; another asset (a `.zip`) is ignored by them.
Measured with this version (2026-11-10): the one-file Setup.exe is 25.1 MB, shows its window after
about 1.5 s, runs as two processes and unpacks 1626 files (47 MB) into `%TEMP%` at every start; the
same Setup built as one folder (`--onedir`) is a 3.5 MB exe with 1626 files beside it (51 MB; a zip
of it 25.2 MB deflated, 50.7 MB stored), shows its window after 0.3-0.5 s, runs as one process and
unpacks nothing. A small starter with the payload beside it does not work for the installed
launchers, which download only the exe (it would have to download the payload itself, which looks
worse); a self-extracting archive (IExpress, 7-Zip) unpacks into `%TEMP%` as PyInstaller does. The
release stays the one-file exe. Ready for a later release to switch, the switch the maintainer's:
`release.py --zip` (off by default) also builds the one-folder Setup and packs it into
`build\release\Setup.zip` (Setup.exe at its root), and from this version on the launcher can take
such an asset - one ending with `Setup.zip` - instead of the exe: `PREFER_SETUP_ZIP` in
`update_check.cpp` says which a release with both gives (`false`, the default: the exe, as before;
a release with only the zip gives the zip). The zip is downloaded and checked as the exe is (size,
SHA-256, the zone mark, Downloads), its central directory is checked before anything is unpacked
(at most 10000 files and 512 MB, the zip itself at most 256 MB, every entry a plain relative path -
no drive, no root, no `..`, no name Windows reserves -, `Setup.exe` at the root), and it is
unpacked with the map exchange's zip reader (`zip_read.cpp`, moved out of `editor_exchange.cpp`)
into a new empty folder of its own, `%LOCALAPPDATA%\OpenLocoland\setup\<tag>` (`<tag>-2` ... when that
is taken; Uninstall removes it with the captures); only that `Setup.exe` gets the zone mark (so
Windows shows its "Open File - Security Warning" as for the exe) and is started, in its folder, with
`--target <the game's folder>` - quoted by the command line's rules, so a game at a drive root reaches
Setup as `C:\` and a folder with spaces as it is (`LOCOFIX_UPDATE_TARGET`, only with
`LOCOFIX_UPDATE_TEST`, sets it in a test) -; the exe is started as before, without arguments, so it
opens on the default folder. The Setups unpacked before are removed once a newer one is in. Any
failure - an exception of the zip step too (a damaged zip, no memory in the 32-bit game) - removes the
folder and falls back to the release's `Setup.exe`, or, without one, to "The download failed" with
the GitHub link. The inflater has no limit of its own on
what a stream unpacks to (each entry is checked against its declared size and CRC only when it is
done): the bounds hold for the sizes the zip declares, and the archive is the one whose size and
SHA-256 the release states. So the switch for a later release: build with `--zip`, upload
`OpenLocoland-<version>-Setup.zip` next to the exe (the exe stays: installs up to this version find
only it), and set `PREFER_SETUP_ZIP = true` in a version whose launchers should take the zip.
`LOCOFIX_UPDATE_PREFER=zip|exe` overrides the constant in a test (only with `LOCOFIX_UPDATE_TEST`).
Not done: building PyInstaller's bootloader from its source (the exe would not share the stock
bootloader's bytes, which many unrelated programs carry); code signing, which needs a certificate.

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

**Checked** for the installer's rework (2026-11-10, Setup.exe of v0.2.10-4-g48d2c5a and the commit
after it, against v0.2.10's released Setup; registry key exported before and restored after,
the shortcuts only into a folder of the worktree - `--shortcut-root` - and every `.lnk` of the real
desktop and Start menu compared before and after: unchanged): a fresh `--silent --no-shortcuts`
install and an `--auto --no-shortcuts` update over a copy of a v0.2.9 install give the same files
with the same SHA-256 as v0.2.10's Setup (2067 and 2139 files; only `LocoFix.dll` differs, by the
version compiled into it) and the same `files` in `OpenLocoland.json`; an update with leftovers
(D3D8.dll, a root log, an editor scratch folder, a file the old record lists) removes the same
four things with the same log lines and keeps a marked high score table; mission 1 loads in a
1024 x 768 window from both installs, with the three `!!!ERROR` lines of the original that an
install of v0.2.10 has too. During an install the new Setup's process tree is its own two
processes and nothing else (v0.2.10's: two `powershell.exe` with their `conhost.exe`, even with
`--no-shortcuts`). The shortcuts made in Setup's process have every property of the PowerShell
ones (target, arguments, working folder, icon, description, window style, hotkey) and the same
bytes but the copy of the target's last access time in the header. The game-running check refuses
the folder a running Locoland.exe is from and not another one. Killed (the whole process tree)
after 2, 5, 6, 9, 13 and 16 s of an update and stopped at `built` and `before-journal`, the game
was the v0.2.9 one file for file; stopped half way through the renames, mixed (5 of 10 files) -
the next run put the other 5 in ("An earlier run of Setup ... stopped while putting its files in");
stopped before the record, the new game whole with the old record; after each, a run of the new
Setup gave the same files as an uninterrupted update, and v0.2.10's Setup over a half put in game
gave its own update whole (the journal it leaves is then dropped by the next new run).
v0.2.10's Setup killed after 3, 7 and 11 s left 3, 7 and 9 of the 10 files new - a mixed game.
Setup.exe renamed 4 s into an install: the new one finished, v0.2.10's stopped ("appears to have
been moved or deleted"); `LocoFix.dll` deleted from `%TEMP%\_MEI...` right after the start: the
install stopped before building with the plain message, in English (`--silent --lang en`) and in
Polish (the window, `--auto`), and the folder as before. The window looked at in Polish (the
frozen Setup) and English (the same code from the repository); `--help` and a wrong argument show
their message box (v0.2.10's showed nothing). A custom Defender scan (`MpCmdRun -Scan -ScanType 3
-File ... -DisableRemediation`, definitions 1.463.226.0) finds nothing in the new Setup nor in
v0.2.10's - which says nothing of the cloud's judgement that quarantined v0.2.10 on a player's
machine. Not checked: the window at a screen scale above 100 % (this screen is 96 DPI; DPI
awareness is new), Uninstall, a CD edition or `--skip-verify` install, the launcher's update
check against the one-file Setup through Windows' warning (below), other Windows versions.
The zip path of the update check (LocoFix of v0.2.10-7-g6478b7d, stand-in releases through
`LOCOFIX_UPDATE_TEST` with `file:///` assets, `LOCOFIX_UPDATE_PREFER=zip`): a good zip next to the
exe - downloaded, SHA-256 checked, 1628 files unpacked into `setup\v9.9.9-test`, the same files as
the one-folder build, only `Setup.exe` with the zone mark, and Run Setup asked to start it with
`--target <the game's folder>`; Windows' "Open File - Security Warning" came up and was cancelled
(not run), so the log says "did not start" - the unpacked Setup started directly with those
arguments opened its window on the game's folder, one process, nothing in `%TEMP%`; a second run took
`setup\v9.9.9-test-2`; a zip with a `../evil.txt` entry and one with `C:/evil.txt` - refused before
anything was unpacked ("an entry that is not a plain path inside the folder"), no folder made, no
`evil.txt` anywhere, the release's Setup.exe downloaded instead; a truncated zip without an exe -
"not a zip file", "The download failed" with the link; a release with only the exe, and the good
release without the override - the exe, as before. The one-folder Setup installs the same files as
the one-file one. The map exchange with the moved zip reader: two reporters' zips imported, the
imported map exported and imported again - the export's entries (packed bytes too) and the 24
imported files the same as with the LocoFix before the move. The test downloads, the `setup`
folder and `UpdateLastCheck` were put back afterwards.
After the control's checks (Setup of v0.2.10-13-g93bb79b; v0.2.10's for comparison): an install
without `logs\` and with `error.log` in its root, updated through the window (`--auto`) - v0.2.10's
OK, the earlier build of this rework failed after its commit, this one OK with the log moved into
`logs\`; a first install through the window ends with `logs\`; a game source with every file read-only
(a copy of the Steam folder): three runs in a row OK and no work folder left (the earlier build failed
from the second run on), and a real update over that install - a read-only Steam file and a read-only
file of ours changed by hand - put both back (v0.2.10's stopped with "Permission denied"); a run
stopped half way through the renames, then the next one made to fail while finishing them (a file
held open): "stopped half way: run Setup again", and the run after that finished it to the same
files as an uninterrupted update; the free-space check with the free space set to 100 MB
(`--test-free-mb`, a mocked value - no small disk was made): the sentence in English and Polish, the
game as it was, no work folder; `keep_modules_in_memory` from the source with PyInstaller's modules
replaced by ones lacking the constants, the offset or with another table: 0, no exception. The zip
path: a zip whose last central directory record is cut off - "a damaged zip file", the exe instead,
the game kept running (the editor's import of such a zip ends the game: `read_zip` reads the record's
name before checking it fits - a follow-up, the reader left as it was); `--target` for
`C:\Games\Loco\`, `C:\` and a path with spaces parsed back by the C runtime and by
`CommandLineToArgvW` as `C:\Games\Loco`, `C:\` and the path; the older unpacked Setup removed when a
newer one was in. Again: a fresh install and an update the same files as v0.2.10's but `LocoFix.dll`,
the same record, the process tree Setup's own two processes, a kill after 9 s the game all old,
Defender's custom scan clean.

**By hand**, the player may have to: allow the unsigned `Setup.exe` past SmartScreen ("More info"
> "Run anyway") or an antivirus that flags PyInstaller programs (in November 2026 a player's
Defender quarantined v0.2.10's Setup while it ran - "Trojan", a judgement of the cloud that a scan
of the same file here did not repeat; a false-positive report went to Microsoft); download another language's
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
