r"""Assembles the ready game from the Steam original and the repo contents.

    python tools\build.py [--target <folder>] [--steam <game folder in Steam>]
                          [--locofix <LocoFix.dll>] [--captured <folder>] [--dev] [--no-clean]

The default target is the folder Locoland next to the repository (..\Locoland).
--locofix takes a prebuilt LocoFix.dll instead of compiling src\LocoFix (the installer,
tools/release.py), --captured keeps the language captures in another folder than captured/,
--dev copies the developer helpers of game/ too (run_map.cmd, edit_map.cmd), which a build
without it leaves out and removes from the target, --no-clean removes nothing (clean_up).

The Steam copy has to be the original: Steam.exe must be the Steam version of 2003-10-07 (its
SHA-256, the exe LocoFix is made for) - a changed or replaced exe stops the build with a request
to verify the game files in Steam - and so must every file our patches change (scripts and
menus: game/**/*.patch and the scripts of lang/<code>/; their SHA-256, the same in every
language), checked before anything is built. One edit players made is undone instead: a
`#define KEYS_ENABLE` line added to maps\script\constants.lgc to turn on the developer keys (the
launcher's Cheats option now), when without it the file is the original byte for byte. Only the
game's own files and folders of the Steam folder's root are used: anything else there (a
DirectDraw wrapper such as dgVoodoo2, DDrawCompat or cnc-ddraw, DxWnd, config files) is ignored
and listed, and such a file copied into the target by an older build is removed.

Order:
  1. game files from Steam (without Steam.exe, steam.cfg and the movies in video/ - the
     intros are off, game/maps/logo.lgc.patch); the
     language files (languages.LANGUAGE_FILES) are taken from the English capture
     (captured/en/) instead of the language Steam has installed, so the root of the game -
     the fallback of every pack - is English (with no English capture, Steam's, and a note),
  2. the files from game/ in the repo: copied, or applied to the Steam file when they are
     changes (<file>.patch, see patch_text.py),
  3. the language packs into lang\<code>\: the installed language and the language
     depots downloaded next to it are captured into captured/<code>/ if they are not there
     yet, then each pack of lang/ that has a capture is assembled with our changes (see
     languages.py; a fan translation on any captured language when its base is not there,
     languages.build_base), and lang\root.txt names the root's language (the launcher's
     fallback, src/LocoFix/language.cpp),
  4. dgVoodoo2 (DDraw.dll, D3DImm.dll) from external/,
  5. LocoFix.dll - the C++ fixes from src/LocoFix (compiled when the sources changed, or the
     prebuilt one of --locofix),
  6. objects.res with the sound fix, vid\110.vid with the fort cannon frames and
     vid\450side.vid, the side tiles of the state bar, and vid\939edit.vid, the icons of the
     editor bar (patch_data.py),
  7. Locoland.exe = Steam.exe after the patches from patch_exe.py (LocoFix loading, launcher).
Then objects.res gets the values changed in game/objects.override/ (TSV tables, see
res_tool.py), the maps with a folder in game/maps.override/ get their changed objects
and every folder in game/maps.new/ is packed as a whole new map (map_tool.py). The logs
go to logs\ of the target, made here (with the folder there the game writes
logs\error<date>.log by itself, LocoFix logs\LocoFix.log): the logs an older build's game left
in the root are moved into it.

Copying is incremental (skips files with the same size and date, or the same content), and the
patched and generated files are written only when they differ (generate). Results the game
writes itself (high score tables) are not overwritten once they exist in the target
folder. At the end clean_up removes what earlier versions left there and this build did not
write (DEVELOPERS.md "Updating"); the player's files are left alone.
"""
import argparse
import filecmp
import hashlib
import os
import re
import shutil
import sys
import tempfile

# In the installer (tools/release.py, frozen by PyInstaller) the repository's files are unpacked
# into sys._MEIPASS with the same layout.
REPO = getattr(sys, "_MEIPASS", None) or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

sys.path.insert(0, os.path.join(REPO, "tools"))
import build_locofix  # noqa: E402
import languages  # noqa: E402
import map_tool  # noqa: E402
import patch_data  # noqa: E402
import patch_exe  # noqa: E402
import patch_text  # noqa: E402
import res_tool  # noqa: E402
from safe_path import safe_relpath  # noqa: E402

STEAM = languages.STEAM
TARGET = os.path.join(os.path.dirname(REPO), "Locoland")

SKIPPED = {"steam.exe", "steam.cfg", "steam_appid.txt",
           "objects.res"}               # objects.res is written with the fix (patch_data.py)
FORT_CANNON = os.path.join("vid", "110.vid")   # written with the fix (patch_data.py)
SKIPPED_DIRS = {"video"}                # intros (the outro is a scene in maps/outro)
# the root of the game in Steam: the files and folders of the original; anything else there was
# added by the player (a DirectDraw wrapper, its settings...) and is not used
GAME_ROOT_FILES = {"steam.exe", "steam.cfg", "steam_appid.txt", "objects.res", "strings.ini"}
GAME_ROOT_DIRS = {"cursores", "fonts", "maps", "music", "vid", "video", "wav"}

# Known game versions. Each entry: {name, exe_sha256, patch_addresses?, notes}.
# - Steam: the official Steam release (2003-10-07), fully supported.
# - CD / beta-demo (Steamland): not yet in the table; add their SHA-256 and addresses when available.
#   The installer can run with --skip-hash on unknown exes; it will warn but try to proceed.
VERSIONS = {
    "steam": {
        "name": "Steam (2003-10-07)",
        "exe_sha256": "a31e9e512968d52352cb06c7e2a6c34e2a088175d86b38de6dd15e454f076020",
        "addresses": {
            "BASE": 0x400000,
            "IAT_LOAD_LIBRARY_A": 0x475104,
            "NEW_CODE": 0x474830,
            "START_DIALOG": 0x8ADB8,
            "START_BITMAP": 0x8B430,
            "call_site": 0x45952b,
            "winmain": 0x4186f0,
        },
        "notes": "Official Steam release. All patches verified."
    },
    # Future: add "cd", "beta-demo" entries here when their exes are analysed.
}

STEAM_VERSION = VERSIONS["steam"]
STEAM_EXE_SHA256 = STEAM_VERSION["exe_sha256"]

# The files of the Steam version our patches change (the same in every language): a file that
# differs was changed by the player, and the build stops before anything is written. A patch of
# a file missing here is checked by applying it.
ORIGINAL_SHA256 = {
    os.path.join("maps", "logo.lgc"): "6b055bf8124832bd334dc2f28d2566cc35a16876b80791c3c1d27c109196dce9",
    os.path.join("maps", "menu.lgc"): "0d1058f31ca2904660acc69e2e8fad7a433e5d2d93f5158884e5267dfb95fc39",
    os.path.join("maps", "script", "basefunctions.lgc"): "a0ee8eb3865434c423a71b0df701ab8ec142ff0515cdafe05415f243dbd41397",
    os.path.join("maps", "script", "constants.lgc"): "4313d929e4ea9c082c246cc9dc0e5b75eb687deb8f9c4df9cad6913d27a3d72c",
    os.path.join("maps", "script", "debug_functions.lgc"): "c425230d5851b7c776cfaa2e025e8f498d20c88f1dea1b49daa9b0b2725f8c0f",
    os.path.join("maps", "script", "initdefault.lgc"): "779bde2e9aa2606818647da9a1de590ad14b8d10a4b35afaaa49871779fb0f04",
    os.path.join("maps", "script", "menuend.lgc"): "618e3a9df1e68af3a18b985e9c3bfa7601904de80be73cdd5dafb2925910d367",
    os.path.join("maps", "script", "menutact.lgc"): "ca2479c2ab512fe112ea937c9c9225d4649f3c80e3550ce3a7768c05ff366bda",
    os.path.join("maps", "script", "trainstactfunctions.lgc"): "c6454480a5738d0af28dc7fa8df9967710e496752a869bd9e9b81038deceb5fb",
}
# A player's edit of a Steam script that is undone: the `#define KEYS_ENABLE` lines added to
# constants.lgc for the developer keys, when the file without them is the original.
CONSTANTS_LGC = os.path.join("maps", "script", "constants.lgc")
KEYS_ENABLE_LINE = re.compile(rb"^[ \t]*#define[ \t]+KEYS_ENABLE\b[^\r\n]*(\r?\n|$)", re.M)
MODIFIED_FILES = ("Locoland's files in {path} are modified: {files}. Restore them in Steam: Locoland > "
                  "Properties > Installed Files > Verify integrity of game files, then build again "
                  "(OpenLocoland has its own Cheats option for the developer keys).")
MODIFIED_STEAM = ("Steam.exe in {path} is not the original file of the game (it was changed or replaced). "
                  "Restore the original in Steam: Locoland > Properties > Installed Files > Verify integrity "
                  "of game files, then build again.")
DGVOODOO = ["DDraw.dll", "D3DImm.dll"]  # the game only uses DirectDraw/Direct3D 7
# The clean-up after the build (clean_up; DEVELOPERS.md "Updating"). Files earlier versions put
# into the target that no build writes now: the Steam launcher's files (0.1 copied them), the DLLs
# of older dgVoodoo set-ups, the Steam intros of video\ (0.1 copied the folder) and the logo clip
# video\openlocoland.wmv of the test builds after v0.2.0 (the intro's logo is the title board now,
# vid\481logo.vid); the folder itself goes when it is empty.
OBSOLETE = ["Steam.exe", "steam.cfg", "steam_appid.txt", "D3D8.dll", "D3D9.dll", "dgVoodooCpl.exe",
            os.path.join("video", "publisher1.wmv"), os.path.join("video", "publisher2.wmv"),
            os.path.join("video", "developer.wmv"), os.path.join("video", "intro.wmv"),
            os.path.join("video", "openlocoland.wmv")]
# the developer helpers of game/: copied only by --dev, without it removed like OBSOLETE
DEV_FILES = ["run_map.cmd", "edit_map.cmd"]
LOGS = "logs"   # the folder of the logs in the target
# the logs of the game (error.log, error<date> <time>.log), of LocoFix and of the installer that
# an older build left in the root of the target
ROOT_LOG = re.compile(r"^(error(\d{4}-\d\d-\d\d \d\dh\d\dm\d\d)?|locofix|install)\.log$", re.I)
# Folders that only the build fills: a file there it did not write is from an older layout (the
# help pages of maps\ are at its root, among the player's maps). A lang\<code>\ or maps\<name>\
# the build wrote nothing into this time (a pack not built, a player's map) is left alone.
OWNED_DIRS = ["lang", "editor"]
HELP_PAGE = re.compile(r"^edithelp\d*\.(txt|lst)$", re.I)
# what LocoFix's editor leaves when the game ends at the wrong moment: the scratch folder of a new
# map (maps\_new, _new2 ..., marked with editor_new.txt) and a save that did not finish (level_save.cpp)
SCRATCH_MARK = "editor_new.txt"
SCRATCH_DIR = re.compile(r"^_new\d*$", re.I)
SAVING = ".saving"
# never removed, whatever a list says: the player's own (with player_data: the high scores)
PLAYER_FILES = {os.path.join("maps", "custom.lst"), "OpenLocoland.json", "Locoland.cfg"}
OBJECTS_OVERRIDE = "objects.override"   # in game/: changed values of objects.res (res_tool.py)
MAPS_OVERRIDE = "maps.override"         # in game/: changed objects of maps (map_tool.py)
MAPS_NEW = "maps.new"                   # in game/: whole new maps (map_tool.py new)
EDITOR_DATA = "editor"                  # in game/: the editor's palette (patch_data.make_editor_palette)
LOGO_PNG = os.path.join(REPO, "tools", "icon", "openlocoland_logo.png")   # the project's logo (just_Martin): the
                                        # intro's title board vid\481logo.vid (patch_data.make_logo_board) and Setup's header
MAP_MODEL = {"map.json", "objects.tsv", "groups.txt", "grid.txt", "vids.tsv",
             "vid_animations.tsv"}      # an unpacked .map, packed as <name>.map

# Output exe name (must stay Locoland.exe for config/registry compatibility: <exe>.cfg -> Locoland.cfg)
OUTPUT_EXE = "Locoland.exe"


def player_data(rel_path):
    """High score tables, which the game overwrites itself while playing."""
    name = os.path.basename(rel_path).lower()
    return rel_path.lower().startswith("maps") and name in ("hiscores", "hiscores.txt")


def english_files():
    """The English capture's language files by their normcased path in the game folder: they make
    the root of the game English whatever language Steam has installed. Empty without one."""
    folder = os.path.join(languages.CAPTURED, "en")
    out = {}
    for directory, _, files in os.walk(folder):
        for f in files:
            rel = safe_relpath(os.path.join(directory, f), folder)
            if languages.is_language_file(rel):
                out[os.path.normcase(rel)] = os.path.join(directory, f)
    return out


ROOT_LANGUAGE = os.path.join("lang", "root.txt")   # the code of the root's language (language.cpp)


def root_language(steam):
    """The language of the Steam copy's own files (the root's without an English capture): the one
    Steam installed, or the capture whose strings.ini is the Steam folder's. None if unknown."""
    code = languages.steam_language(steam)
    if code:
        return code
    try:
        data = patch_text.read(os.path.join(steam, "strings.ini"))
    except OSError:
        return None
    return next((c for c in languages.FALLBACK_BASES if languages.captured(c) and os.path.isfile(
        os.path.join(languages.CAPTURED, c, "strings.ini")) and
        patch_text.read(os.path.join(languages.CAPTURED, c, "strings.ini")) == data), None)


def game_files(game):
    """Files of game/ relative to it, without the objects.res and map overrides."""
    for directory, subdirs, files in os.walk(game):
        if directory == game:
            subdirs[:] = [d for d in subdirs
                          if d.lower() not in (OBJECTS_OVERRIDE, MAPS_OVERRIDE, MAPS_NEW, EDITOR_DATA)]
        for f in files:
            yield safe_relpath(os.path.join(directory, f), game)


def map_overrides(game, steam):
    """{map file relative to the game folder: override folder} of game/maps.override/: a
    folder <name> stands for maps/<name>/<name>.map or maps/<name>.map."""
    found = {}
    root = os.path.join(game, MAPS_OVERRIDE)
    for name in sorted(os.listdir(root)) if os.path.isdir(root) else []:
        for rel in (os.path.join("maps", name, name + ".map"), os.path.join("maps", name + ".map")):
            if os.path.isfile(os.path.join(steam, rel)):
                found[rel] = os.path.join(root, name)
                break
        else:
            sys.exit(f"game/{MAPS_OVERRIDE}/{name}: the game has no map {name}")
    return found


def new_maps(game, target, counter):
    """Packs every folder of game/maps.new/ as maps/<name>/<name>.map: the folder is the
    unpacked map (map.json, ...), its vid/ is the unpacked image (<name>.vid) and every
    other file is a mission file (the script, texts) copied next to them - the high score
    files only while the game has not written them yet. Returns the names."""
    root = os.path.join(game, MAPS_NEW)
    built = []
    for name in sorted(os.listdir(root)) if os.path.isdir(root) else []:
        folder = os.path.join(root, name)
        if not os.path.isdir(folder) or not os.path.isfile(os.path.join(folder, "map.json")):
            sys.exit(f"game/{MAPS_NEW}/{name}: no map.json (make it with map_tool.py new)")
        out = os.path.join(target, "maps", name)
        generate(counter, lambda path: map_tool.pack(folder, path), os.path.join(out, name + ".map"))
        vid = os.path.join(folder, "vid")
        if os.path.isfile(os.path.join(vid, "vid.json")):
            generate(counter, lambda path: map_tool.pack(vid, path), os.path.join(out, name + ".vid"))
        for directory, subdirs, files in os.walk(folder):
            rel = safe_relpath(directory, folder)
            if rel == ".":
                subdirs[:] = [d for d in subdirs if d.lower() != "vid"]
            for f in files:
                if rel == "." and f.lower() in MAP_MODEL:
                    continue
                dst = os.path.join(out, f) if rel == "." else os.path.join(out, rel, f)
                if player_data(safe_relpath(dst, target)) and os.path.exists(dst):
                    continue   # the high scores of a map: the game writes them
                copy(os.path.join(directory, f), dst, counter)
        built.append(name)
    return built


CUSTOM_LIST = os.path.join("maps", "custom.lst")


def merge_custom_list(src, dst, target, counter):
    """maps\\custom.lst: the repository's maps, then those the game folder's list has besides
    them and whose map is there - the ones the map editor added (LocoFix, editor_mission.cpp)."""
    def pairs(path):
        lines = [l.strip() for l in open(path, encoding="latin-1").read().splitlines() if l.strip()]
        return [(lines[i], lines[i + 1]) for i in range(0, len(lines) - 1, 2)]
    entries = pairs(src)
    if os.path.exists(dst):
        known = {folder.lower() for folder, _ in entries}
        entries += [(folder, title) for folder, title in pairs(dst) if folder.lower() not in known
                    and os.path.isfile(os.path.join(target, "maps", folder, folder + ".map"))]
    data = "\r\n".join(f"{folder}\r\n{title}" for folder, title in entries).encode("latin-1")
    if not os.path.exists(dst) or open(dst, "rb").read() != data:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, "wb").write(data)
        counter[0] += 1


def identify_version(steam):
    """Identify the game version by its Steam.exe SHA-256. Returns (version_key, version_info) or (None, None)."""
    try:
        data = open(os.path.join(steam, "Steam.exe"), "rb").read()
    except OSError:
        return None, None
    sha = hashlib.sha256(data).hexdigest()
    for key, info in VERSIONS.items():
        if info["exe_sha256"] == sha:
            return key, info
    return None, None


def steam_exe_original(steam, version=None, skip_verify=False):
    """Whether Steam.exe of the folder matches a known version (or any exe if skip_verify)."""
    try:
        data = open(os.path.join(steam, "Steam.exe"), "rb").read()
    except OSError:
        return False
    if skip_verify:
        return True
    if version:
        return hashlib.sha256(data).hexdigest() == version["exe_sha256"]
    return hashlib.sha256(data).hexdigest() == STEAM_EXE_SHA256


_told = set()


def known_sha256(rel):
    return {os.path.normcase(k): v for k, v in ORIGINAL_SHA256.items()}.get(os.path.normcase(rel))


def steam_original(steam, path):
    """The content of a Steam file our changes apply to; maps\\script\\constants.lgc with the
    `#define KEYS_ENABLE` lines of a player left out when that gives the original exactly."""
    data = patch_text.read(path)
    rel = safe_relpath(path, steam)
    if os.path.normcase(rel) != os.path.normcase(CONSTANTS_LGC):
        return data
    if hashlib.sha256(data).hexdigest() != known_sha256(rel):
        undone = KEYS_ENABLE_LINE.sub(b"", data)
        if hashlib.sha256(undone).hexdigest() == known_sha256(rel):
            if path not in _told:
                print(f"{CONSTANTS_LGC}: the Steam file's #define KEYS_ENABLE left out "
                      "(the developer keys are the launcher's Cheats option)")
                _told.add(path)
            return undone
    return data


def modified_steam_files(steam):
    """The files of the Steam folder our patches change that differ from the original - changed
    by the player (relative paths, sorted): the targets of the patches of game/ and of the script
    patches of the language packs (the language texts' patches apply to the captured files)."""
    changes = [(os.path.join(REPO, "game", rel), patch_text.target_name(rel))
               for rel in game_files(os.path.join(REPO, "game")) if rel.endswith(".patch")]
    for code in languages.packs():
        for rel, path in languages.pack_files(code):
            if path.endswith(".patch") and not languages.is_language_file(patch_text.target_name(path)):
                changes.append((os.path.join(languages.LANG, code, rel), patch_text.target_name(path)))
    bad = set()
    for change, name in changes:
        path = os.path.join(steam, name)
        try:
            data = steam_original(steam, path)
            if known_sha256(name):
                if hashlib.sha256(data).hexdigest() != known_sha256(name):
                    bad.add(name)
            else:
                patch_text.changed_file(change, path, data)
        except (OSError, ValueError):
            bad.add(name)
    return sorted(bad, key=str.lower)


def foreign_root_entries(steam):
    """The files and folders of the Steam folder's root that are not the game's own (sorted)."""
    found = []
    for name in os.listdir(steam):
        known = GAME_ROOT_DIRS if os.path.isdir(os.path.join(steam, name)) else GAME_ROOT_FILES
        if name.lower() not in known:
            found.append(name)
    return sorted(found, key=str.lower)


WRITTEN = set()   # the files of the target this build wrote or found up to date (normcased, absolute)


def wrote(*paths):
    """Records files as the build's own - those that are there."""
    for path in paths:
        if os.path.isfile(path):
            WRITTEN.add(os.path.normcase(os.path.abspath(path)))


def write(path, data):
    """patch_text.write_if_changed, recorded."""
    changed = patch_text.write_if_changed(path, data)
    wrote(path)
    return changed


def generate(counter, make, *outputs):
    """Runs make(<a temporary path for each of `outputs`>) and puts each file it made at its
    output only when the content differs, so an unchanged one is not touched (recorded either way).
    Returns what make returned."""
    folder = tempfile.mkdtemp(prefix="openlocoland_")
    try:
        temps = [os.path.join(folder, f"{i}_{os.path.basename(out)}") for i, out in enumerate(outputs)]
        result = make(*temps)
        for temp, out in zip(temps, outputs):
            if os.path.isfile(temp):
                counter[0] += write(out, patch_text.read(temp))
        return result
    finally:
        shutil.rmtree(folder, ignore_errors=True)


def written_files(target):
    """The files this build wrote into `target`, relative and normcased (the high scores and
    PLAYER_FILES left out: the player's, even where the build writes the first one)."""
    root = os.path.normcase(os.path.abspath(target))
    player = {os.path.normcase(f) for f in PLAYER_FILES}
    out = set()
    for path in WRITTEN:
        if os.path.dirname(path).startswith(root):
            rel = os.path.relpath(path, root)
            if rel not in player and os.path.basename(rel) not in ("hiscores", "hiscores.txt"):
                out.add(rel)
    return out


def unit(rel):
    """The folder a file belongs to for the clean-up: lang\\<code> or maps\\<name> (a pack, a map),
    otherwise the top folder of the target ("" for its root)."""
    parts = os.path.normcase(rel).split(os.sep)
    if len(parts) > 2 and parts[0] in ("lang", "maps"):
        return os.path.join(parts[0], parts[1])
    return parts[0] if len(parts) > 1 else ""


def clean_up(target, steam, previous=(), dev=False, foreign=(), ours=()):
    """Removes from `target`, after the build, what earlier versions put there that this build did
    not write (DEVELOPERS.md "Updating"): the files of `previous` (the list of the last install's
    OpenLocoland.json) in a pack or map this build wrote into, OBSOLETE and,
    without `dev`, DEV_FILES, the files of OWNED_DIRS and the help pages of maps\\ it did not
    write, the editor's scratch folders and unfinished saves, and the `foreign` files of the
    Steam folder an older build copied (the same size, not named in `ours`); the logs of the root
    go into logs\\. Never outside the target, inside the Steam folder, through a link, among the
    files just written or the player's (PLAYER_FILES, the high scores, logs\\). Each removal is
    printed (install.log). Returns the removed paths, relative."""
    root = os.path.normcase(os.path.realpath(target))
    steam_root = os.path.normcase(os.path.realpath(steam)) if steam else None
    written = written_files(target)
    units = {unit(rel) for rel in written}
    player = {os.path.normcase(f) for f in PLAYER_FILES}
    removed = []

    def inside(path, folder):
        try:
            return os.path.commonpath([path, folder]) == folder
        except ValueError:   # another drive
            return False

    if steam_root and (inside(root, steam_root) or inside(steam_root, root)):
        print("clean-up skipped: the target and the Steam folder overlap")
        return removed

    def allowed(rel):
        path = os.path.join(target, rel)
        real = os.path.normcase(os.path.realpath(path))
        key = os.path.normcase(os.path.normpath(rel))
        if real == root or not inside(real, root) or (steam_root and inside(real, steam_root)):
            return False
        if os.path.islink(path) or os.path.isjunction(path):
            return False
        if key in player or key.split(os.sep)[0] == LOGS or os.path.basename(key) in ("hiscores", "hiscores.txt"):
            return False
        return not any(w == key or w.startswith(key + os.sep) for w in written)

    def remove(rel, reason, folder=False):
        path = os.path.join(target, rel)
        if not (os.path.isdir(path) if folder else os.path.isfile(path)) or not allowed(rel):
            return
        shutil.rmtree(path) if folder else os.remove(path)
        removed.append(rel)
        print(f"removed {rel}{os.sep if folder else ''} ({reason})")

    def files_under(folder):
        for directory, _, files in os.walk(os.path.join(target, folder)):
            for f in files:
                yield safe_relpath(os.path.join(directory, f), target)

    for rel in previous:
        rel = os.path.normpath(rel)
        if unit(rel) in units:
            remove(rel, "no longer installed")
    for f in OBSOLETE + ([] if dev else DEV_FILES):
        remove(f, "obsolete")
    video = os.path.join(target, "video")   # 0.1 and the test builds after v0.2.0 made it; nothing writes it now
    if os.path.isdir(video) and not os.listdir(video) and allowed("video"):
        os.rmdir(video)
        removed.append("video")
        print(f"removed video{os.sep} (empty)")
    names = {f.lower() for f in ours}
    for f in foreign:
        if (f.lower() not in names and os.path.isfile(os.path.join(target, f)) and os.path.isfile(os.path.join(steam, f))
                and os.path.getsize(os.path.join(target, f)) == os.path.getsize(os.path.join(steam, f))):
            remove(f, "copied from the Steam folder by an older build, not the game's")
    for folder in OWNED_DIRS:
        for rel in list(files_under(folder)):
            if unit(rel) in units:
                remove(rel, "an older layout")
    maps = os.path.join(target, "maps")
    for f in sorted(os.listdir(maps)) if os.path.isdir(maps) else []:
        if HELP_PAGE.match(f):
            remove(os.path.join("maps", f), "an older help page")
        elif SCRATCH_DIR.match(f) and os.path.isfile(os.path.join(maps, f, SCRATCH_MARK)):
            remove(os.path.join("maps", f), "the editor's scratch folder", folder=True)
    for rel in list(files_under("maps")):
        if rel.lower().endswith(SAVING):
            remove(rel, "an unfinished save of the editor")
    # the folders the removals left empty (not the target's own top folders)
    for rel in removed:
        folder = os.path.dirname(os.path.join(target, rel))
        while (os.path.normcase(os.path.dirname(folder)) != os.path.normcase(os.path.abspath(target))
               and os.path.isdir(folder) and not os.listdir(folder)
               and allowed(safe_relpath(folder, target))):
            os.rmdir(folder)
            print(f"removed {safe_relpath(folder, target)}{os.sep} (empty)")
            folder = os.path.dirname(folder)

    logs = os.path.join(target, LOGS)
    for f in sorted(os.listdir(target)):
        if ROOT_LOG.match(f) and os.path.isfile(os.path.join(target, f)):
            if os.path.exists(os.path.join(logs, f)):
                remove(f, f"an old log, {LOGS}{os.sep} has a newer one")
            elif allowed(f):
                os.replace(os.path.join(target, f), os.path.join(logs, f))
                print(f"moved {f} -> {LOGS}{os.sep}")
    return removed


def copy(src, dst, counter):
    """Copies `src` to `dst` unless it is there already: the same size and date, or the same
    content (Setup unpacks the repository's files anew at every start, with new dates)."""
    if os.path.exists(dst):
        a, b = os.stat(src), os.stat(dst)
        if a.st_size == b.st_size and (int(a.st_mtime) == int(b.st_mtime) or filecmp.cmp(src, dst, shallow=False)):
            wrote(dst)
            return
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)
    wrote(dst)
    counter[0] += 1


def build(steam, target, locofix=None, captured=None, progress=None, version=None, skip_verify=False, dev=False,
          previous=(), clean=True):
    """Assembles the game from `steam` into `target`. `locofix` is a prebuilt LocoFix.dll
    (None compiles src/LocoFix), `captured` the folder of language captures (None: captured/
    of the repository), `progress(fraction)` is told how far it is, `dev` copies DEV_FILES.
    `version` is a VERSIONS entry (or None to auto-detect); `skip_verify` bypasses exe checks.
    Then clean_up removes what earlier versions left (`previous`: the files the last install
    recorded), unless `clean` is False. Returns the files written, relative (sorted: the record
    of this install). Stops with SystemExit and the reason when something is wrong."""
    def done(fraction):
        if progress:
            progress(fraction)

    a = argparse.Namespace(steam=steam, target=target)
    if captured:
        languages.CAPTURED = captured
    if not os.path.isfile(os.path.join(a.steam, "Steam.exe")) and not os.path.isfile(os.path.join(a.steam, "Locoland.exe")):
        sys.exit(f"No game in {a.steam}")
    if os.path.normcase(os.path.abspath(a.target)) == os.path.normcase(os.path.abspath(a.steam)):
        sys.exit("The target is the Steam game folder itself - the game is built next to it")
    has_steam = os.path.isfile(os.path.join(a.steam, "Steam.exe"))
    has_locoland = os.path.isfile(os.path.join(a.steam, "Locoland.exe"))
    if has_steam and not steam_exe_original(a.steam, version, skip_verify):
        if skip_verify:
            print(f"Warning: Steam.exe in {a.steam} does not match a known version (continuing with --skip-verify)")
        else:
            sys.exit(MODIFIED_STEAM.format(path=a.steam))
    if has_locoland and has_steam:
        sys.exit(MODIFIED_STEAM.format(path=a.steam) + " (both Steam.exe and Locoland.exe present)")
    modified = modified_steam_files(a.steam)
    if modified:
        sys.exit(MODIFIED_FILES.format(path=a.steam, files=", ".join(modified)))
    foreign = foreign_root_entries(a.steam)
    if foreign:
        print(f"Steam folder: not the game's, not used: {', '.join(foreign)}")
    copied = [0]
    WRITTEN.clear()
    game = os.path.join(REPO, "game")
    from_repo = {os.path.normcase(patch_text.target_name(rel) or rel) for rel in game_files(game)}
    maps = map_overrides(game, a.steam)
    from_repo |= {os.path.normcase(rel) for rel in maps}     # written with the changes

    # the languages first: the English capture makes the root of the game English
    for code, n in languages.capture_all(a.steam).items():
        print(f"captured/{code}: {n} language files captured from Steam")
    english = english_files()   # the root's language files: the English ones
    if not english:
        print("captured/en: no English capture - the game's root keeps the language Steam has "
              f"(Steam console: download_depot {languages.STEAM_APP_ID} 352131)")
    steam_files = sum(len(files) for _, _, files in os.walk(a.steam)) or 1
    seen = 0
    for directory, subdirs, files in os.walk(a.steam):
        rel = safe_relpath(directory, a.steam)
        if rel == ".":
            subdirs[:] = [d for d in subdirs if d.lower() in GAME_ROOT_DIRS - SKIPPED_DIRS]
        for f in files:
            seen += 1
            done(0.6 * seen / steam_files)
            path = os.path.normpath(os.path.join(rel, f))
            if rel == "." and (f.lower() in SKIPPED or f.lower() not in GAME_ROOT_FILES):
                continue
            # the repository's files and the patched ones come later (a patch reads the Steam original)
            if os.path.normcase(path) in from_repo:
                continue
            if os.path.normcase(path) == FORT_CANNON:
                continue
            dst = os.path.join(a.target, path)
            if player_data(path) and os.path.exists(dst):
                continue
            src = os.path.join(directory, f)
            if english and languages.is_language_file(path):
                src = english.get(os.path.normcase(path), src)
            copy(src, dst, copied)

    for rel in game_files(game):
        name = patch_text.target_name(rel)
        if os.path.normcase(rel) == os.path.normcase(CUSTOM_LIST):
            merge_custom_list(os.path.join(game, rel), os.path.join(a.target, rel), a.target, copied)
        elif name is None:
            if rel.lower() in DEV_FILES and not dev:
                continue
            copy(os.path.join(game, rel), os.path.join(a.target, rel), copied)
        else:
            try:
                copied[0] += write(
                    os.path.join(a.target, name),
                    patch_text.changed_file(os.path.join(game, rel), os.path.join(a.steam, name),
                                        steam_original(a.steam, os.path.join(a.steam, name))))
            except Exception as e:
                print(f"Patch failed for {rel} -> {name}: {e} (the Steam original instead)")
                copy(os.path.join(a.steam, name), os.path.join(a.target, name), copied)

    done(0.62)
    depots = {code: depot for depot, code in languages.DEPOT_LANGUAGES.items()}
    for code in languages.packs():
        base = languages.build_base(code)
        if not base:
            print(f"lang/{code}: skipped - not downloaded yet (Steam console: "
                  f"download_depot {languages.STEAM_APP_ID} {depots.get(languages.pack_base(code), '?')})")
            continue
        if base != languages.pack_base(code):
            print(f"lang/{code}: built on captured/{base} (no captured/{languages.pack_base(code)}): "
                  f"its fonts and logo are those of {base}")
        written = languages.install(code, a.steam, a.target, lambda src, dst: copy(src, dst, copied),
                                    lambda path: steam_original(a.steam, path), write)
        copied[0] += written
    # the language of the game's root, which the launcher falls back on when the saved pack is gone
    root = "en" if english else root_language(a.steam)
    if root:
        copied[0] += write(os.path.join(a.target, ROOT_LANGUAGE), (root + "\r\n").encode())

    done(0.75)
    for f in DGVOODOO:
        copy(os.path.join(REPO, "external", "dgVoodoo2", f), os.path.join(a.target, f), copied)

    copy(locofix or build_locofix.build(), os.path.join(a.target, "LocoFix.dll"), copied)
    steam_exe = os.path.join(a.steam, "Steam.exe")
    locoland_exe = os.path.join(a.steam, "Locoland.exe")
    target_exe = os.path.join(a.target, OUTPUT_EXE)
    if os.path.isfile(steam_exe):
        generate(copied, lambda path: patch_exe.build(steam_exe, path), target_exe)
    elif os.path.isfile(locoland_exe):
        copy(locoland_exe, target_exe, copied)   # an already patched copy of the game
    else:
        sys.exit(f"No game executable found in {a.steam}")
    done(0.8)
    # only now, when the target folder has all game and repo files: the patch checks
    # which sounds are really missing in the assembled game; then the values of objects.override
    override = os.path.join(game, OBJECTS_OVERRIDE)

    def make_objects_res(path):
        missing = patch_data.fix_objects_res(os.path.join(a.steam, "objects.res"), os.path.join(a.target, "wav"), path)
        return missing, res_tool.apply(path, override, path) if os.path.isdir(override) else None
    try:
        missing, changed = generate(copied, make_objects_res, os.path.join(a.target, "objects.res"))
    except res_tool.DataError as e:
        sys.exit(f"objects.res: {e}")
    if missing:
        print(f"objects.res: {len(missing)} sounds without a file -> wav\\null.wav")
    if changed is not None:
        print(f"objects.res: {changed} values changed by game/{OBJECTS_OVERRIDE}")
    vid = os.path.join(a.target, "vid")
    if not generate(copied, lambda path: patch_data.fix_fort_cannon(os.path.join(a.steam, FORT_CANNON), path),
                    os.path.join(a.target, FORT_CANNON)):
        print(f"{FORT_CANNON}: unknown layout, copied without the fix")
    if not generate(copied, lambda path: patch_data.make_status_bar_sides(os.path.join(a.steam, "vid", "450sbar.vid"),
                                                                          path), os.path.join(vid, "450side.vid")):
        print("vid\\450sbar.vid: unknown layout, no side tiles for the state bar")
    if not generate(copied, lambda path: patch_data.make_editor_icons(os.path.join(a.steam, "vid"), path),
                    os.path.join(vid, "939edit.vid")):
        print("vid: unknown images, no icons for the editor bar")
    if not generate(copied, lambda tsv, image: patch_data.make_editor_palette(
            a.steam, os.path.join(game, EDITOR_DATA, "palette.tsv"), tsv, image),
            os.path.join(a.target, "editor", "palette.tsv"), os.path.join(vid, "944pal.vid")):
        print("vid: unknown images, no palette for the editor")
    # the ground painting's textures: where to sample them in the player's campaign grounds
    copy(os.path.join(game, EDITOR_DATA, "textures.tsv"), os.path.join(a.target, "editor", "textures.tsv"), copied)
    if not generate(copied, lambda help_vid, menu_vid: patch_data.make_editor_buttons(
            os.path.join(a.steam, "vid"), help_vid, menu_vid),
            os.path.join(vid, "942help.vid"), os.path.join(vid, "943menu.vid")):
        print("vid: unknown images, no Help and Menu buttons for the editor bar")
    if not generate(copied, lambda path: patch_data.make_custom_edit_icon(os.path.join(a.steam, "vid"), path),
                    os.path.join(vid, "945edit.vid")):
        print("vid: unknown images, no edit icon for the custom maps page")
    if not generate(copied, lambda path: patch_data.make_logo_board(LOGO_PNG, path), os.path.join(vid, "481logo.vid")):
        sys.exit(f"{LOGO_PNG} is missing - the title board of the intro (vid\\481logo.vid, VID 481) cannot be made")

    done(0.9)
    for rel, folder in maps.items():
        try:
            changed = generate(copied, lambda path: map_tool.apply_map(
                os.path.join(a.steam, rel), folder, path, os.path.join(a.target, "objects.res")),
                os.path.join(a.target, rel))
        except res_tool.DataError as e:
            sys.exit(f"{rel}: {e}")
        print(f"{rel}: {changed} values changed by game/{MAPS_OVERRIDE}/{os.path.basename(folder)}")

    try:
        built = new_maps(game, a.target, copied)
    except res_tool.DataError as e:
        sys.exit(f"game/{MAPS_NEW}: {e}")
    for name in built:
        print(f"maps\\{name}: new map from game/{MAPS_NEW}/{name}")

    # the clean-up only now, when everything this build writes is written
    os.makedirs(os.path.join(a.target, LOGS), exist_ok=True)
    removed = []
    if clean:
        ours = DGVOODOO + ["LocoFix.dll", OUTPUT_EXE] + os.listdir(game)
        removed = clean_up(a.target, a.steam, previous, dev, foreign, ours)
    else:
        print("clean-up skipped (--no-clean): nothing of earlier versions removed")

    done(1.0)
    print(f"Done: {a.target} ({copied[0]} files copied, {len(removed)} of earlier versions removed)")
    return sorted(written_files(a.target))


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--steam", default=STEAM)
    p.add_argument("--target", default=TARGET)
    p.add_argument("--locofix", help="a prebuilt LocoFix.dll instead of compiling src/LocoFix")
    p.add_argument("--captured", help="the folder of the language captures (default: captured/)")
    p.add_argument("--dev", action="store_true", help="copy the developer helpers run_map.cmd and edit_map.cmd too")
    p.add_argument("--no-clean", action="store_true", help="remove nothing earlier versions left in the target")
    a = p.parse_args()
    build(a.steam, a.target, a.locofix, a.captured, dev=a.dev, clean=not a.no_clean)


if __name__ == "__main__":
    main()
