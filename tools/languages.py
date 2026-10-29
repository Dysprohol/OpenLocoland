r"""Language packs: the game files that differ between language versions.

    python tools\languages.py capture [<code>] [--steam <game folder>] [--force]

The language versions differ only in data files: texts (strings.ini, briefings, menus,
objectives), fonts (the Polish ones have Polish letters in place of Cyrillic) and the
logo. Each language is a separate Steam depot with the whole game. Steam installs one of
them; the others can be downloaded next to it without touching the installed game, with
the Steam console (steam://open/console):

    download_depot 352130 352131      English      download_depot 352130 352134  Polish
    download_depot 352130 352132      German       download_depot 352130 352135  Portuguese
    download_depot 352130 352133      Hungarian    download_depot 352130 352136  Russian

They land in steamapps\contentpp_352130\depot_<id>\. build.py captures the language files
of the installed game and of every downloaded depot into captured/<code>/ (outside git) and
keeps them there.

The repository holds only our work in lang/<code>/: language.ini (the launcher's name and
labels for the language) and in game/ the texts, by kind (pack_path gives each one's place in
the pack):

    game/strings.override.ini        strings.ini (changes, see patch_text.py)
    game/briefings/<map>/*.txt       maps\<map>\ (the missions', sandbox's and template's texts)
    game/menus/*.txt                 maps\ (the menu's pages)
    game/editor/*.txt                maps\ (the editor's help pages)
    game/scripts/*.lgc.patch         maps\script\ (the developer mode scripts)
    game/scripts/shiftb.txt.patch    maps\ (the developer mode's help)

A file is copied, or applied to the captured one when it is a change (<file>.patch,
<file>.override.ini). The four developer mode scripts (maps\script\*.lgc) are the same in every
language version; their patches apply to the game's own files. lang/_template/ is the starting
point of a new translation, not a pack (README.md files there and anywhere in a pack are not
shipped). build.py assembles each pack into <game>\lang\<code>\ and LocoFix reads files from the
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


def local_languages(game_folder):
    """Codes of languages found in a non-Steam game folder (looks for strings.ini with language info)."""
    codes = set()
    # Look for lang/<code>/strings.ini or similar structure
    lang_dir = os.path.join(game_folder, "lang")
    if os.path.isdir(lang_dir):
        for d in os.listdir(lang_dir):
            ini = os.path.join(lang_dir, d, "language.ini")
            if os.path.isfile(ini):
                codes.add(d)
    # Also check root for strings.ini (single language install)
    root_ini = os.path.join(game_folder, "strings.ini")
    if os.path.isfile(root_ini):
        try:
            import configparser
            cp = configparser.ConfigParser(interpolation=None)
            cp.read(root_ini, encoding="utf-8", errors="replace")
            # Try to guess language from content or use 'en' as default
            codes.add("en")
        except Exception:
            pass
    return codes


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
    base= when that is captured (in `have`, default: the captures of CAPTURED); for a fan
    translation without it, another captured language (FALLBACK_BASES) unless needs_base."""
    if have is None:
        have = {d for d in os.listdir(CAPTURED) if captured(d)} if os.path.isdir(CAPTURED) else set()
    base = pack_base(code)
    if base in have:
        return base
    if not fan_pack(code) or needs_base(code):
        return None
    return next((c for c in FALLBACK_BASES + sorted(have) if c in have and c != code), None)


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


def available(game_folder, local=False):
    """Codes of the languages that can be captured or are captured: the one Steam installed,
    the downloaded depots and the captures already made. If local=True, also look for
    languages in the game folder itself (for CD/beta-demo versions)."""
    codes = {code for code, _ in downloaded_depots(game_folder)}
    if steam_language(game_folder):
        codes.add(steam_language(game_folder))
    if local:
        codes |= local_languages(game_folder)
    if os.path.isdir(CAPTURED):
        codes |= {d for d in os.listdir(CAPTURED) if captured(d)}
    return codes


def capture_all(game_folder, local=False):
    """Captures the installed language and every downloaded depot that is not captured yet.
    If local=True, also captures languages found in the game folder itself.
    Returns {code: number of files}."""
    sources = []
    installed = steam_language(game_folder)
    if installed:
        sources.append((installed, game_folder))
    sources += downloaded_depots(game_folder)
    if local:
        for code in local_languages(game_folder):
            if code not in [c for c, _ in sources]:
                sources.append((code, game_folder))
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


def install(code, game_folder, target, copy, read_original=None):
    """Assembles pack `code` into <target>\\lang\\<code>\\: the captured files (of its base, see
    build_base), then our files from lang/<code>/ at their place in the pack (pack_path; copied,
    or applied when they are changes). The originals of changed files come from the capture, or
    from `game_folder` for files that are the same in every language (scripts). `copy(src, dst)` copies one file incrementally;
    `read_original(path)` reads an original (patch_text.read when not given). Returns the
    number of files written by the changes."""
    pack, source = os.path.join(target, "lang", code), os.path.join(CAPTURED, build_base(code))
    ours = pack_files(code)
    changed_names = {os.path.normcase(patch_text.target_name(path) or path) for _, path in ours}

    for directory, _, files in os.walk(source):
        for f in files:
            rel = safe_relpath(os.path.join(directory, f), source)
            if os.path.normcase(rel) not in changed_names:
                copy(os.path.join(source, rel), os.path.join(pack, rel))

    written = 0
    for rel, path in ours:
        src = os.path.join(LANG, code, rel)
        name = patch_text.target_name(path)
        if name is None:
            copy(src, os.path.join(pack, path))
            continue
        original = os.path.join(source, name)
        if not os.path.isfile(original):
            original = os.path.join(game_folder, name)
        written += patch_text.write_if_changed(
            os.path.join(pack, name), patch_text.changed_file(src, original, (read_original or patch_text.read)(original)))
    return written


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("command", choices=["capture"])
    p.add_argument("code", nargs="?", help="language code (default: the language Steam has)")
    p.add_argument("--steam", default=STEAM, help="game folder to capture from")
    p.add_argument("--force", action="store_true", help="replace an existing capture")
    a = p.parse_args()
    code = a.code or steam_language(a.steam)
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
