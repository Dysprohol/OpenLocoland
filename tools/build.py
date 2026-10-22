r"""Assembles the ready game from the Steam original and the repo contents.

    python tools\build.py [--target <folder>] [--steam <game folder in Steam>]
                          [--locofix <LocoFix.dll>] [--captured <folder>]

The default target is the folder Locoland next to the repository (..\Locoland).
--locofix takes a prebuilt LocoFix.dll instead of compiling src\LocoFix (the installer,
tools/release.py), --captured keeps the language captures in another folder than captured/.

Order:
  1. game files from Steam (without Steam.exe, steam.cfg and the movies in video/); the
     language files (languages.LANGUAGE_FILES) are taken from the English capture
     (captured/en/) instead of the language Steam has installed, so the root of the game -
     the fallback of every pack - is English (with no English capture, Steam's, and a note),
  2. the files from game/ in the repo: copied, or applied to the Steam file when they are
     changes (<file>.patch, see patch_text.py),
  3. the language packs into lang\<code>\: the installed language and the language
     depots downloaded next to it are captured into captured/<code>/ if they are not there
     yet, then each pack of lang/ that has a capture is assembled with our changes (see
     languages.py),
  4. dgVoodoo2 (DDraw.dll, D3DImm.dll) from external/,
  5. LocoFix.dll - the C++ fixes from src/LocoFix (compiled when the sources changed, or the
     prebuilt one of --locofix),
  6. objects.res with the sound fix, vid\110.vid with the fort cannon frames and
     vid\450side.vid, the side tiles of the state bar, and vid\939edit.vid, the icons of the
     editor bar (patch_data.py),
  7. Locoland.exe = Steam.exe after the patches from patch_exe.py (LocoFix loading, launcher).
Then objects.res gets the values changed in game/objects.override/ (TSV tables, see
res_tool.py), the maps with a folder in game/maps.override/ get their changed objects
and every folder in game/maps.new/ is packed as a whole new map (map_tool.py).

Copying is incremental (skips files with the same size and date). Results the game
writes itself (high score tables) are not overwritten once they exist in the target
folder. Other files in the target folder are left alone.
"""
import argparse
import os
import shutil
import sys

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

STEAM = languages.STEAM
TARGET = os.path.join(os.path.dirname(REPO), "Locoland")

SKIPPED = {"steam.exe", "steam.cfg", "steam_appid.txt",
           "objects.res"}               # objects.res is written with the fix (patch_data.py)
FORT_CANNON = os.path.join("vid", "110.vid")   # written with the fix (patch_data.py)
SKIPPED_DIRS = {"video"}                # intros (the outro is a scene in maps/outro)
DGVOODOO = ["DDraw.dll", "D3DImm.dll"]  # the game only uses DirectDraw/Direct3D 7
# files from earlier versions of this folder that are no longer needed
OBSOLETE = ["Steam.exe", "steam.cfg", "steam_appid.txt", "D3D8.dll", "D3D9.dll", "dgVoodooCpl.exe"]
OBJECTS_OVERRIDE = "objects.override"   # in game/: changed values of objects.res (res_tool.py)
MAPS_OVERRIDE = "maps.override"         # in game/: changed objects of maps (map_tool.py)
MAPS_NEW = "maps.new"                   # in game/: whole new maps (map_tool.py new)
EDITOR_DATA = "editor"                  # in game/: the editor's palette (patch_data.make_editor_palette)
MAP_MODEL = {"map.json", "objects.tsv", "groups.txt", "grid.txt", "vids.tsv",
             "vid_animations.tsv"}      # an unpacked .map, packed as <name>.map


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
            rel = os.path.relpath(os.path.join(directory, f), folder)
            if languages.is_language_file(rel):
                out[os.path.normcase(rel)] = os.path.join(directory, f)
    return out


def game_files(game):
    """Files of game/ relative to it, without the objects.res and map overrides."""
    for directory, subdirs, files in os.walk(game):
        if directory == game:
            subdirs[:] = [d for d in subdirs
                          if d.lower() not in (OBJECTS_OVERRIDE, MAPS_OVERRIDE, MAPS_NEW, EDITOR_DATA)]
        for f in files:
            yield os.path.relpath(os.path.join(directory, f), game)


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
        map_tool.pack(folder, os.path.join(out, name + ".map"))
        vid = os.path.join(folder, "vid")
        if os.path.isfile(os.path.join(vid, "vid.json")):
            map_tool.pack(vid, os.path.join(out, name + ".vid"))
        for directory, subdirs, files in os.walk(folder):
            rel = os.path.relpath(directory, folder)
            if rel == ".":
                subdirs[:] = [d for d in subdirs if d.lower() != "vid"]
            for f in files:
                if rel == "." and f.lower() in MAP_MODEL:
                    continue
                dst = os.path.join(out, f) if rel == "." else os.path.join(out, rel, f)
                if player_data(os.path.relpath(dst, target)) and os.path.exists(dst):
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


def copy(src, dst, counter):
    if os.path.exists(dst):
        a, b = os.stat(src), os.stat(dst)
        if a.st_size == b.st_size and int(a.st_mtime) == int(b.st_mtime):
            return
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)
    counter[0] += 1


def build(steam, target, locofix=None, captured=None, progress=None):
    """Assembles the game from `steam` into `target`: see the module's description. `locofix`
    is a prebuilt LocoFix.dll (None compiles src/LocoFix), `captured` the folder of the language
    captures (None: captured/ of the repository) and `progress(fraction)` is told how far it is.
    Stops with SystemExit and the reason when something is wrong."""
    def done(fraction):
        if progress:
            progress(fraction)

    a = argparse.Namespace(steam=steam, target=target)
    if captured:
        languages.CAPTURED = captured
    if not os.path.isfile(os.path.join(a.steam, "Steam.exe")):
        sys.exit(f"No game in {a.steam}")
    if os.path.normcase(os.path.abspath(a.target)) == os.path.normcase(os.path.abspath(a.steam)):
        sys.exit("The target is the Steam game folder itself - the game is built next to it")
    copied = [0]
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
        rel = os.path.relpath(directory, a.steam)
        if rel == ".":
            subdirs[:] = [d for d in subdirs if d.lower() not in SKIPPED_DIRS]
        for f in files:
            seen += 1
            done(0.6 * seen / steam_files)
            path = os.path.normpath(os.path.join(rel, f))
            if rel == "." and f.lower() in SKIPPED:
                continue
            if os.path.normcase(path) in from_repo or os.path.normcase(path) == FORT_CANNON:
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
            copy(os.path.join(game, rel), os.path.join(a.target, rel), copied)
        else:
            copied[0] += patch_text.write_if_changed(
                os.path.join(a.target, name),
                patch_text.changed_file(os.path.join(game, rel), os.path.join(a.steam, name)))

    done(0.62)
    depots = {code: depot for depot, code in languages.DEPOT_LANGUAGES.items()}
    for code in languages.packs():
        if not languages.captured(languages.pack_base(code)):
            print(f"lang/{code}: skipped - not downloaded yet (Steam console: "
                  f"download_depot {languages.STEAM_APP_ID} {depots.get(languages.pack_base(code), '?')})")
            continue
        written = languages.install(code, a.steam, a.target, lambda src, dst: copy(src, dst, copied))
        copied[0] += written

    done(0.75)
    for f in DGVOODOO:
        copy(os.path.join(REPO, "external", "dgVoodoo2", f), os.path.join(a.target, f), copied)

    copy(locofix or build_locofix.build(), os.path.join(a.target, "LocoFix.dll"), copied)
    patch_exe.build(os.path.join(a.steam, "Steam.exe"), os.path.join(a.target, "Locoland.exe"))
    done(0.8)
    # only now, when the target folder has all game and repo files: the patch checks
    # which sounds are really missing in the assembled game
    missing = patch_data.fix_objects_res(os.path.join(a.steam, "objects.res"),
                                         os.path.join(a.target, "wav"),
                                         os.path.join(a.target, "objects.res"))
    if missing:
        print(f"objects.res: {len(missing)} sounds without a file -> wav\\null.wav")
    if not patch_data.fix_fort_cannon(os.path.join(a.steam, FORT_CANNON),
                                      os.path.join(a.target, FORT_CANNON)):
        print(f"{FORT_CANNON}: unknown layout, copied without the fix")
    if not patch_data.make_status_bar_sides(os.path.join(a.steam, "vid", "450sbar.vid"),
                                            os.path.join(a.target, "vid", "450side.vid")):
        print("vid\\450sbar.vid: unknown layout, no side tiles for the state bar")
    if not patch_data.make_editor_icons(os.path.join(a.steam, "vid"), os.path.join(a.target, "vid", "939edit.vid")):
        print("vid: unknown images, no icons for the editor bar")
    if not patch_data.make_editor_palette(a.steam, os.path.join(game, EDITOR_DATA, "palette.tsv"),
                                          os.path.join(a.target, "editor", "palette.tsv"),
                                          os.path.join(a.target, "vid", "944pal.vid")):
        print("vid: unknown images, no palette for the editor")
    # the ground painting's textures: where to sample them in the player's campaign grounds
    copy(os.path.join(game, EDITOR_DATA, "textures.tsv"), os.path.join(a.target, "editor", "textures.tsv"), copied)
    if not patch_data.make_editor_buttons(os.path.join(a.steam, "vid"), os.path.join(a.target, "vid", "942help.vid"),
                                          os.path.join(a.target, "vid", "943menu.vid")):
        print("vid: unknown images, no Help and Menu buttons for the editor bar")

    done(0.9)
    override = os.path.join(game, OBJECTS_OVERRIDE)
    if os.path.isdir(override):
        objects_res = os.path.join(a.target, "objects.res")
        try:
            changed = res_tool.apply(objects_res, override, objects_res)
        except res_tool.DataError as e:
            sys.exit(f"objects.res: {e}")
        print(f"objects.res: {changed} values changed by game/{OBJECTS_OVERRIDE}")

    for rel, folder in maps.items():
        try:
            changed = map_tool.apply_map(os.path.join(a.steam, rel), folder, os.path.join(a.target, rel),
                                         os.path.join(a.target, "objects.res"))
        except res_tool.DataError as e:
            sys.exit(f"{rel}: {e}")
        print(f"{rel}: {changed} values changed by game/{MAPS_OVERRIDE}/{os.path.basename(folder)}")

    try:
        built = new_maps(game, a.target, copied)
    except res_tool.DataError as e:
        sys.exit(f"game/{MAPS_NEW}: {e}")
    for name in built:
        print(f"maps\\{name}: new map from game/{MAPS_NEW}/{name}")

    removed = 0
    for f in OBSOLETE:
        path = os.path.join(a.target, f)
        if os.path.isfile(path):
            os.remove(path)
            removed += 1
    if os.path.isdir(os.path.join(a.target, "video")):
        shutil.rmtree(os.path.join(a.target, "video"))
        removed += 1

    done(1.0)
    print(f"Done: {a.target} ({copied[0]} files copied, {removed} obsolete removed)")


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--steam", default=STEAM)
    p.add_argument("--target", default=TARGET)
    p.add_argument("--locofix", help="a prebuilt LocoFix.dll instead of compiling src/LocoFix")
    p.add_argument("--captured", help="the folder of the language captures (default: captured/)")
    a = p.parse_args()
    build(a.steam, a.target, a.locofix, a.captured)


if __name__ == "__main__":
    main()
