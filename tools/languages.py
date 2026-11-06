r"""Language packs: the game files that differ between language versions.

    python tools\languages.py capture [<code>] [--steam <game folder>] [--force]

(the folder's language is told from Steam's appmanifest or from an edition's files, else give
its code)

The language versions differ only in data files: texts (strings.ini, briefings, menus,
objectives), fonts (the Polish ones have Polish letters in place of Cyrillic) and the
logo. Each language is a separate Steam depot with the whole game. Steam installs one of
them; the others can be downloaded next to it without touching the installed game, with
the Steam console (steam://open/console):

    download_depot 352130 352131      English      download_depot 352130 352134  Polish
    download_depot 352130 352132      German       download_depot 352130 352135  Portuguese
    download_depot 352130 352133      Hungarian    download_depot 352130 352136  Russian

They land in steamapps\content\app_352130\depot_<id>\. build.py captures the language files
of the installed game and of every downloaded depot into captured/<code>/ (outside git) and
keeps them there.

A Czech edition of the game ("Steamland", US Action, 2003) was sold on CD and never on Steam:
the Steam exe and files with Czech texts and fonts (notes/czech-edition.md). build.py knows it
by the SHA-256 of its strings.ini (EDITIONS) and captures it like a depot into captured/cs: from
the game folder itself when that is the edition (a player without Steam gives it to build.py or
Setup) or from a folder named Steamland (EDITION_FOLDER) in or next to the Steam game's. The
Czech pack is then built on that capture (build_base: a pack's own capture before its base=)
with only what the edition does not have from lang/cs/ (install).

The repository holds only our work in lang/<code>/: language.ini (the launcher's name and
labels for the language) and in game/ the texts, by kind (pack_path gives each one's place in
the pack):

    game/strings.override.ini        strings.ini (changes, see patch_text.py)
    game/briefings/<map>/*.txt       maps\<map>\ (the missions', sandbox's and template's texts)
    game/menus/*.txt                 maps\ (the menu's pages)
    game/editor/*.txt                maps\ (the editor's help pages)
    game/scripts/*.lgc.patch         maps\script\ (the developer mode scripts)
    game/scripts/shiftb.txt.patch    maps\ (the developer mode's help)
    fonts/<font>.png                 fonts\<font>*.bmp (the pack's own letters, see font_cells.py)

A file is copied, or applied to the captured one when it is a change (<file>.patch,
<file>.override.ini; a font sheet has its cells pasted into the base's fonts). The four
developer mode scripts (maps\script\*.lgc) are the same in every language version; their
patches apply to the game's own files. lang/_template/ is the starting point of a new
translation, not a pack (README.md files there and anywhere in a pack are not shipped). build.py assembles each pack into <game>\lang\<code>\ and LocoFix reads files from the
chosen pack first (src/LocoFix/language.cpp).
"""
import argparse
import fnmatch
import os
import re
import shutil
import sys

REPO = getattr(sys, "_MEIPASS", None) or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

sys.path.insert(0, os.path.join(REPO, "tools"))
import font_cells  # noqa: E402
import patch_text  # noqa: E402
from safe_path import safe_relpath  # noqa: E402

LANG = os.path.join(REPO, "lang")
# The captures: captured/ of the repository, or - in a checkout without one (a new worktree) - the
# installer's (%LOCALAPPDATA%\OpenLocoland\captured, tools/installer.py) when it has them.
CAPTURED = os.path.join(REPO, "captured")
INSTALLER_CAPTURED = os.path.join(os.environ.get("LOCALAPPDATA", os.path.expanduser("~")), "OpenLocoland", "captured")
if not os.path.isdir(CAPTURED) and os.path.isdir(INSTALLER_CAPTURED):
    CAPTURED = INSTALLER_CAPTURED
STEAM = r"C:\Program Files (x86)\Steam\steamapps\common\Locoland"
STEAM_APP_ID = 352130
DEPOT_LANGUAGES = {352131: "en", 352132: "de", 352133: "hu", 352134: "pl", 352135: "pt", 352136: "ru"}
# Editions of the game outside Steam, by the SHA-256 of their strings.ini: the Czech edition
# "Steamland" (US Action, 2003) - the Steam exe and files with Czech texts, all its fonts with the
# Czech letters, no vid\logo.bmp (notes/czech-edition.md).
EDITIONS = {"85bec1a91b013ca0001a6917ade6dff771bf21e5707a6a1346e16b35788401c0": "cs"}
EDITION_FOLDER = "Steamland"   # an edition's own folder name, looked for in and next to the Steam game's

# Paths relative to the game folder, compared case-insensitively.
LANGUAGE_FILES = [
    "strings.ini",
    "fonts\\*.bmp",
    "vid\\logo.bmp",
    "maps\\*.txt",
    "maps\\l??\\*.txt",
]
NOT_LANGUAGE_FILES = ["*\\hiscores.txt", "maps\\hiscores"]   # player data

# Steam's language names (appmanifest) for the six languages the game ships in.
STEAM_LANGUAGES = {"english": "en", "german": "de", "hungarian": "hu", "polish": "pl",
                   "portuguese": "pt", "brazilian": "pt", "russian": "ru"}


def is_language_file(rel_path):
    p = rel_path.lower().replace("/", "\\")
    if any(fnmatch.fnmatch(p, pat) for pat in NOT_LANGUAGE_FILES):
        return False
    return any(fnmatch.fnmatch(p, pat) and p.count("\\") == pat.count("\\") for pat in LANGUAGE_FILES)


def packs():
    """Codes of the language packs in the repo, e.g. ["en", "pl"] (not lang/_template)."""
    if not os.path.isdir(LANG):
        return []
    return sorted(d for d in os.listdir(LANG)
                  if not d.startswith("_") and os.path.isfile(os.path.join(LANG, d, "language.ini")))


def pack_files(code):
    """(file in lang/<code>/, its path in the pack) of each file the pack ships (see pack_path)."""
    folder = os.path.join(LANG, code)
    out = []
    for directory, _, files in os.walk(folder):
        for f in files:
            rel = safe_relpath(os.path.join(directory, f), folder)
            path = pack_path(rel)
            if path:
                out.append((rel, path))
    return sorted(out)


def pack_path(rel):
    """The path in the pack (as in the game folder) of a file of lang/<code>/ - with .patch or
    .override.ini still on it for a change -, or None for a file that is not shipped."""
    parts = rel.replace("/", "\\").split("\\")
    name = parts[-1]
    if name.lower() == "readme.md":
        return None
    if parts == ["language.ini"]:
        return name
    if parts[0] == "fonts" and len(parts) == 2 and name.lower().endswith(".png"):
        return os.path.join("fonts", name)   # a font sheet: pasted into fonts\<name>*.bmp (install)
    if parts[0] != "game":
        return None
    if parts[1:] == ["strings.override.ini"]:
        return name
    kind = parts[1] if len(parts) > 2 else None
    if kind == "briefings" and len(parts) == 4:
        return os.path.join("maps", parts[2], name)
    if kind in ("menus", "editor") and len(parts) == 3:
        return os.path.join("maps", name)
    if kind == "scripts" and len(parts) == 3:
        script = name.lower().endswith((".lgc", ".lgc.patch"))
        return os.path.join("maps", "script", name) if script else os.path.join("maps", name)
    return None


def captured(code):
    return os.path.isdir(os.path.join(CAPTURED, code))


def edition_language(folder):
    """The code of the known edition of the game (EDITIONS) whose files are in `folder`, or None."""
    import hashlib
    try:
        with open(os.path.join(folder, "strings.ini"), "rb") as f:
            return EDITIONS.get(hashlib.sha256(f.read()).hexdigest())
    except OSError:
        return None


def edition_folders(game_folder):
    """(code, folder) of each known edition at hand: the game folder itself when it is one, or a
    folder named EDITION_FOLDER in it or next to it."""
    found = []
    for folder in (game_folder, os.path.join(game_folder, EDITION_FOLDER),
                   os.path.join(game_folder, "..", EDITION_FOLDER)):
        code = edition_language(folder)
        if code and code not in {c for c, _ in found}:
            found.append((code, os.path.normpath(folder)))
    return found


def pack_value(code, key, default=None):
    """A value of [language] in lang/<code>/language.ini."""
    import configparser
    ini = configparser.ConfigParser(interpolation=None)
    ini.read(os.path.join(LANG, code, "language.ini"), encoding="utf-8-sig")
    return ini.get("language", key, fallback=default)


def pack_base(code):
    """The language whose captured files pack `code` is meant to be built on: its own, or for a
    translation Steam does not ship the one `base=` names in [language] of its language.ini
    (Czech: en)."""
    return (pack_value(code, "base") or "").strip() or code


def fan_pack(code):
    """A translation Steam does not ship (base= another language): marked unofficial in the lists."""
    return pack_base(code) != code


def needs_base(code):
    """A fan translation that can only be built on its own base= (needs_base=1 in its
    language.ini: it uses letters or files only that language has)."""
    return (pack_value(code, "needs_base") or "0").strip() == "1"


# The captures a fan translation falls back on when its base= is not captured, in this order:
# Steam's Portuguese is the English game, then the Latin fonts before the Cyrillic ones. Every
# font set draws the ASCII letters, which is all a fan translation over another language may use.
FALLBACK_BASES = ["en", "pt", "de", "pl", "hu", "ru"]


def build_base(code, have=None):
    """The captured language pack `code` is assembled on, or None when it cannot be built: its
    own when that is captured (an edition of the game in its language, EDITIONS), else its base=
    when that is captured (in `have`, default: the captures of CAPTURED); for a fan translation
    without it, another captured language (FALLBACK_BASES) unless needs_base."""
    if have is None:
        have = {d for d in os.listdir(CAPTURED) if captured(d)} if os.path.isdir(CAPTURED) else set()
    if code in have:
        return code
    base = pack_base(code)
    if base in have:
        return base
    if not fan_pack(code) or needs_base(code):
        return None
    return next((c for c in FALLBACK_BASES + sorted(have) if c in have and c != code), None)


def unofficial(code, have=None):
    """Whether pack `code` is assembled as a fan translation - on another language's capture
    (build_base): a translation Steam does not ship, without a capture of its own edition. A
    pack that cannot be built counts by what it is (fan_pack)."""
    base = build_base(code, have)
    return fan_pack(code) if base is None else base != code


def steam_language(game_folder):
    """Code of the language Steam installed in `game_folder` (from its appmanifest), or
    None when it cannot be told (not a Steam folder, unknown language)."""
    manifest = os.path.join(game_folder, "..", "..", f"appmanifest_{STEAM_APP_ID}.acf")
    try:
        text = open(manifest, encoding="utf-8", errors="replace").read()
    except OSError:
        return None
    m = re.search(r'"UserConfig"\s*\{[^}]*"language"\s+"([^"]+)"', text)
    return STEAM_LANGUAGES.get(m.group(1).lower()) if m else None


def steam_root():
    """Steam's own folder (HKCU SteamPath), or None."""
    try:
        import winreg
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam") as k:
            return os.path.normpath(winreg.QueryValueEx(k, "SteamPath")[0])
    except (ImportError, OSError):
        return None


def downloaded_depots(game_folder):
    """(code, folder) of each language depot downloaded with download_depot: in steamapps\\content
    of the game's library, or of Steam's own folder (where the console puts them when the game
    is in another library)."""
    contents = [os.path.join(game_folder, "..", "..", "content")]
    if steam_root():
        contents.append(os.path.join(steam_root(), "steamapps", "content"))
    found, seen = [], set()
    for content in contents:
        for depot, code in DEPOT_LANGUAGES.items():
            folder = os.path.normpath(os.path.join(content, f"app_{STEAM_APP_ID}", f"depot_{depot}"))
            if code not in seen and os.path.isfile(os.path.join(folder, "strings.ini")):
                found.append((code, folder))
                seen.add(code)
    return found


def available(game_folder):
    """Codes of the languages that can be captured or are captured: the one Steam installed,
    the downloaded depots, the editions at hand (edition_folders) and the captures already made."""
    codes = {code for code, _ in downloaded_depots(game_folder)}
    if steam_language(game_folder):
        codes.add(steam_language(game_folder))
    codes |= {code for code, _ in edition_folders(game_folder)}
    if os.path.isdir(CAPTURED):
        codes |= {d for d in os.listdir(CAPTURED) if captured(d)}
    return codes


def capture_all(game_folder):
    """Captures the installed language, every downloaded depot and every edition at hand
    (edition_folders) that is not captured yet. Returns {code: number of files}."""
    sources = []
    installed = steam_language(game_folder)
    if installed:
        sources.append((installed, game_folder))
    sources += downloaded_depots(game_folder) + edition_folders(game_folder)
    done = {}
    for code, folder in sources:
        if code not in done and not captured(code):
            done[code] = capture(code, folder)
    return done


def capture(code, game_folder, force=False):
    """Copies the language files of `game_folder` into captured/<code>/. An existing capture
    is left alone unless `force` (it is the only copy once Steam switches languages)."""
    target = os.path.join(CAPTURED, code)
    if os.path.isdir(target):
        if not force:
            return 0
        shutil.rmtree(target)
    copied = 0
    for directory, _, files in os.walk(game_folder):
        for f in files:
            src = os.path.join(directory, f)
            rel = safe_relpath(src, game_folder)
            if not is_language_file(rel):
                continue
            dst = os.path.join(target, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
            copied += 1
    return copied


def install(code, game_folder, target, copy, read_original=None, write=None, skip=()):
    """Assembles pack `code` into <target>\\lang\\<code>\\: the captured files (of its base, see
    build_base), then our files from lang/<code>/ at their place in the pack (pack_path; copied,
    or applied when they are changes). The originals of changed files come from the capture, or
    from `game_folder` for files that are the same in every language (scripts). `copy(src, dst)` copies one file incrementally;
    `read_original(path)` reads an original (patch_text.read when not given), `write(path, data)`
    writes a changed one (patch_text.write_if_changed when not given). The pack's font sheets
    (lang/<code>/fonts/*.png) are pasted into the base's fonts they apply to (font_files).
    On the pack's own capture (an edition of the game in its language, EDITIONS) the capture's
    texts stay: the pack's translations of files the capture has are left out (translates),
    strings.override.ini fills only the keys the capture lacks or leaves empty and corrects the
    ones marked `!` (patch_text.merge_ini fill_only), and the font sheets only the cells the
    capture's fonts leave empty. language.ini is written with base= the capture built on (the
    launcher marks a pack on another language's capture unofficial, language.cpp).
    `skip`: change files (normcased paths) left out - a script patch that cannot be placed in the
    player's Steam file (build.py, skip_verify), so the pack has no copy and the root's is read.
    Returns the number of files written by the changes."""
    base = build_base(code)
    own = base == code
    pack, source = os.path.join(target, "lang", code), os.path.join(CAPTURED, base)
    ours = [(rel, path) for rel, path in pack_files(code)
            if not is_font_sheet(path) and not (own and translates(path, source))]
    fonts = font_files(code, source)
    changed_names = {os.path.normcase(patch_text.target_name(path) or path) for _, path in ours}
    changed_names |= {os.path.normcase(rel) for rel in fonts}
    write = write or patch_text.write_if_changed

    for directory, _, files in os.walk(source):
        for f in files:
            rel = safe_relpath(os.path.join(directory, f), source)
            if os.path.normcase(rel) not in changed_names:
                copy(os.path.join(source, rel), os.path.join(pack, rel))

    written = 0
    for rel, path in ours:
        src = os.path.join(LANG, code, rel)
        if path.lower() == "language.ini":
            written += write(os.path.join(pack, path), pack_ini(src, base))
            continue
        name = patch_text.target_name(path)
        if name is None:
            copy(src, os.path.join(pack, path))
            continue
        if os.path.normcase(src) in skip:
            continue
        original = os.path.join(source, name)
        if not os.path.isfile(original):
            original = os.path.join(game_folder, name)
        written += write(os.path.join(pack, name), patch_text.changed_file(
            src, original, (read_original or patch_text.read)(original), fill_only=own))
    codepage = int(pack_value(code, "codepage", "1252"))
    for rel, sheet in fonts.items():
        written += write(os.path.join(pack, rel), font_cells.apply(
            patch_text.read(os.path.join(source, rel)), sheet, codepage, only_empty=own))
    return written


def translates(path, source):
    """Whether a pack file (its path in the pack) stands in for a text of the capture `source`: a
    copied file the capture has - a menu page, or a mission's text when the capture has that
    mission's folder (maps\\<map>\\) - and not a change (.patch, .override.ini) or language.ini."""
    if patch_text.target_name(path) or path.lower() == "language.ini":
        return False
    parts = path.replace("/", "\\").split("\\")
    if len(parts) == 3 and parts[0].lower() == "maps":
        return os.path.isdir(os.path.join(source, parts[0], parts[1]))
    return os.path.isfile(os.path.join(source, path))


def pack_ini(path, base):
    """lang/<code>/language.ini (bytes) with base= in [language] set to the capture the pack is
    built on; a file without the line is left as it is."""
    section, out = None, []
    for line in patch_text.read(path).split(b"\n"):
        s = line.strip()
        if s.startswith(b"["):
            section = s.lower()
        elif section == b"[language]" and b"=" in s and s.split(b"=", 1)[0].strip().lower() == b"base":
            line = b"base=" + base.encode() + (b"\r" if line.endswith(b"\r") else b"")
        out.append(line)
    return b"\n".join(out)


def is_font_sheet(path):
    """A pack path of a font sheet (pack_path of lang/<code>/fonts/<font>.png)."""
    parts = path.replace("/", "\\").split("\\")
    return len(parts) == 2 and parts[0] == "fonts" and parts[1].lower().endswith(".png")


def font_files(code, source):
    """{path of a font in the capture `source` (fonts\\<name>.bmp): the sheet of lang/<code>/fonts/
    pasted into it} - every font whose name starts with a sheet's name (font_cells.sheet_for)."""
    sheets = font_cells.sheets(os.path.join(LANG, code))
    folder = os.path.join(source, "fonts")
    if not sheets or not os.path.isdir(folder):
        return {}
    out = {}
    for name in sorted(os.listdir(folder)):
        sheet = font_cells.sheet_for(name, sheets) if name.lower().endswith(".bmp") else None
        if sheet:
            out[os.path.join("fonts", name)] = sheet
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("command", choices=["capture"])
    p.add_argument("code", nargs="?", help="language code (default: the language Steam has)")
    p.add_argument("--steam", default=STEAM, help="game folder to capture from")
    p.add_argument("--force", action="store_true", help="replace an existing capture")
    a = p.parse_args()
    code = a.code or steam_language(a.steam) or edition_language(a.steam)
    if not code:
        sys.exit(f"Cannot tell the language of {a.steam} - give its code")
    installed = steam_language(a.steam)
    if installed and installed != code:
        sys.exit(f"{a.steam} has language '{installed}', not '{code}' - switch it in Steam first")
    if captured(code) and not a.force:
        sys.exit(f"captured/{code} exists (use --force to replace it)")
    print(f"captured/{code}: {capture(code, a.steam, a.force)} files")


if __name__ == "__main__":
    main()
