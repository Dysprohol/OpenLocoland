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
labels for the language) and changes to the captured files - <file>.patch and
strings.override.ini (see patch_text.py). The four developer mode scripts
(maps\script\*.lgc) are the same in every language version; their patches apply to the
game's own files. build.py assembles each pack into <game>\lang\<code>\ and LocoFix reads
files from the chosen pack first (src/LocoFix/language.cpp).
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
CAPTURED = os.path.join(REPO, "captured")
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
    """Codes of the language packs in the repo, e.g. ["en", "pl"]."""
    if not os.path.isdir(LANG):
        return []
    return sorted(d for d in os.listdir(LANG) if os.path.isfile(os.path.join(LANG, d, "language.ini")))


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


def pack_base(code):
    """The language whose captured files pack `code` is built on: its own, or for a translation
    Steam does not ship the one `base=` names in [language] of its language.ini (Czech: en)."""
    import configparser
    ini = configparser.ConfigParser(interpolation=None)
    ini.read(os.path.join(LANG, code, "language.ini"), encoding="utf-8")
    return ini.get("language", "base", fallback=code).strip() or code


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
    pack_base), then our
    files from lang/<code>/ (copied, or applied when they are changes). The originals of
    changed files come from the capture, or from `game_folder` for files that are the same
    in every language (scripts). `copy(src, dst)` copies one file incrementally;
    `read_original(path)` reads an original (patch_text.read when not given). Returns the
    number of files written by the changes."""
    pack, source = os.path.join(target, "lang", code), os.path.join(CAPTURED, pack_base(code))
    changed_names = set()
    ours = []
    for directory, _, files in os.walk(os.path.join(LANG, code)):
        for f in files:
            rel = safe_relpath(os.path.join(directory, f), os.path.join(LANG, code))
            ours.append(rel)
            changed_names.add(os.path.normcase(patch_text.target_name(rel) or rel))

    for directory, _, files in os.walk(source):
        for f in files:
            rel = safe_relpath(os.path.join(directory, f), source)
            if os.path.normcase(rel) not in changed_names:
                copy(os.path.join(source, rel), os.path.join(pack, rel))

    written = 0
    for rel in ours:
        src = os.path.join(LANG, code, rel)
        name = patch_text.target_name(rel)
        if name is None:
            copy(src, os.path.join(pack, rel))
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
