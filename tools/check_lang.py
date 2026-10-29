r"""Checks our texts of each language pack against its code page and its fonts.

    python tools\check_lang.py [<code> ...] [--captured <folder>] [--fonts <code>]

A pack's texts are bytes in its code page (`codepage=` in lang/<code>/language.ini: 1250
Polish and Hungarian, 1251 Russian, 1252 English and German), and the game draws each byte
with the frame of that number in a font picture: fonts\*.bmp, a 16 x 16 grid of cells, byte
0x00 in the top-left cell, the cells of the bytes without a letter empty. The fonts differ by
pack - the English ones hold Cyrillic in 0xC0-0xFF, the Polish ones Polish letters in place of
some of it - so a letter may be in the code page and still have no picture in the pack's fonts.

For every pack this checks what we ship in lang/<code>/ (game/strings.override.ini, the texts of
game/briefings, game/menus and game/editor)
and, for English, the root texts every pack falls back to (game/maps/*.txt, the texts of
game/maps.new/<map>/): that each text is valid in the code page and that each character has
a picture in the fonts that draw such texts -

    strings.override.ini   font8x13 (VID 2), font8x13light (7), font-Mousetips (6); the
                           headers also font-MainMenuTitles (3): the menu's and the
                           missions' (MenuHeader*, MapHeader*) and the editor's panels'
                           (the titles, the palette's Group*)
    maps\*.txt             font8x13 (2) and font8x13light (7), objectives.txt font8x13
                           and the messages' font-Objectives (14)

A pack without a capture (captured/<code>/ or the installer's, see languages.CAPTURED) is checked against its code
page only, and the command to download its Steam depot is printed; `--fonts <code>` checks
against another pack's fonts instead (a preview: the English fonts hold the game's original
Cyrillic). Exit code 1 when anything is missing.
"""
import argparse
import configparser
import glob
import os
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "tools"))
import languages  # noqa: E402
from safe_path import safe_relpath  # noqa: E402

FONT_FILES = {2: "font8x13.bmp", 3: "font-MainMenuTitles.bmp", 6: "font-Mousetips.bmp",
              7: "font8x13light.bmp", 14: "font-Objectives.bmp"}
STRINGS_FONTS = (2, 6, 7)
MAP_TEXT_FONTS = (2, 7)
OBJECTIVES_FONTS = (2, 14)
TITLE_FONT = 3
EDITOR_TITLES = {"HelpHeader", "PanelMenuHeader", "PaletteHeader", "VehiclesHeader"}   # editor_panel.cpp
ESC = 0x1b


def codepage(code):
    ini = configparser.ConfigParser(interpolation=None)
    ini.read(os.path.join(REPO, "lang", code, "language.ini"), encoding="utf-8")
    return int(ini.get("language", "codepage", fallback="1252"))


def find_file(folder, name):
    """A file of the game by name, in any case (the captures keep Steam's spelling)."""
    for entry in os.listdir(folder) if os.path.isdir(folder) else []:
        if entry.lower() == name.lower():
            return os.path.join(folder, entry)
    return None


def glyphs(path):
    """The bytes that have a picture in a font: the cells that differ from the cell of 0x00."""
    from PIL import Image
    image = Image.open(path).convert("RGB")
    width, height = image.size
    cw, ch = width // 16, height // 16
    pixels = image.load()

    def cell(byte):
        x0, y0 = (byte % 16) * cw, (byte // 16) * ch
        return [pixels[x0 + x, y0 + y] for y in range(ch) for x in range(cw)]

    empty = cell(0)
    out = {byte: cell(byte) for byte in range(256)}
    return {byte: c for byte, c in out.items() if c != empty or byte == 0x20}


def ini_texts(path):
    """(where, value bytes, fonts) of every value of an override file."""
    out = []
    section = ""
    for n, raw in enumerate(open(path, "rb").read().split(b"\n"), 1):
        line = raw.rstrip(b"\r")
        if not line.strip() or line.startswith(b";"):
            continue
        if line.startswith(b"["):
            section = line.strip(b"[]").decode("ascii", "replace")
            continue
        if b"=" not in line:
            continue
        key, value = line.split(b"=", 1)
        if len(value) >= 2 and value[:1] == b'"' and value[-1:] == b'"':
            value = value[1:-1]
        key = key.decode("ascii", "replace")
        header = (section == "strings" and key.startswith(("MenuHeader", "MapHeader"))) or \
            (section == "Editor" and (key in EDITOR_TITLES or key.startswith("Group")))
        fonts = STRINGS_FONTS + ((TITLE_FONT,) if header else ())
        # \n and \t of the editor's texts are escapes, not letters
        out.append((f"{safe_relpath(path, REPO)}:{n} [{section}] {key}",
                    value.replace(b"\\n", b" ").replace(b"\\t", b" "), fonts))
    return out


def map_texts(path):
    out = []
    data = open(path, "rb").read()
    used = OBJECTIVES_FONTS if os.path.basename(path).lower() == "objectives.txt" else MAP_TEXT_FONTS
    for n, line in enumerate(data.split(b"\n"), 1):
        line = line.rstrip(b"\r")
        # ESC + a font number switches the font of a help page
        clean = bytearray()
        i = 0
        while i < len(line):
            if line[i] == ESC:
                i += 2
                continue
            clean.append(line[i])
            i += 1
        out.append((f"{safe_relpath(path, REPO)}:{n}", bytes(clean), used))
    return out


def texts(code):
    """The texts the pack ships (languages.pack_files): strings.override.ini and the maps' texts.
    The script patches are left out: their lines hold the game's frame bytes (shiftb.txt) and
    font switches."""
    out = []
    folder = os.path.join(REPO, "lang", code)
    files = []
    for rel, path in languages.pack_files(code):
        if path.lower() == "strings.override.ini":
            out += ini_texts(os.path.join(folder, rel))
        elif path.lower().endswith(".txt"):
            files.append(os.path.join(folder, rel))
    if code == "en":   # the root texts are English and every pack's fallback
        files += glob.glob(os.path.join(REPO, "game", "maps", "*.txt"))
        files += glob.glob(os.path.join(REPO, "game", "maps.new", "*", "*.txt"))
    for path in files:
        out += map_texts(path)
    return out


def ini_values(path):
    """{(section, key): value bytes} of a strings.ini."""
    out, section = {}, ""
    for raw in open(path, "rb").read().splitlines():
        line = raw.strip()
        if line.startswith(b"["):
            section = line.strip(b"[]").decode("ascii", "replace")
        elif b"=" in line and not line.startswith(b";"):
            key, value = line.split(b"=", 1)
            out[(section, key.strip().decode("ascii", "replace"))] = value
    return out


def left_to_base(code, captured, bases):
    """The keys of strings.ini a fan translation leaves to its base whose value differs between
    the captured languages: on another base (languages.build_base) they would read otherwise."""
    folder = os.path.join(REPO, "lang", code)
    ours = {}
    for rel, path in languages.pack_files(code):
        if path.lower() == "strings.override.ini":
            ours = ini_values(os.path.join(folder, rel))
    values = [ini_values(path) for path in (find_file(os.path.join(captured, c), "strings.ini") for c in bases) if path]
    keys = set().union(*values) if values else set()
    return sorted(k for k in keys if k not in ours and len({v.get(k) for v in values}) > 1)


def check(code, captured, font_code=None):
    """A pack's texts against its fonts; a fan translation, which may be built on any captured
    language (languages.build_base), against the fonts of each and for keys left to its base."""
    if font_code or not languages.fan_pack(code) or languages.needs_base(code):
        return check_fonts(code, captured, font_code)
    bases = [c for c in languages.FALLBACK_BASES if os.path.isdir(os.path.join(captured, c))]
    ok = all([check_fonts(code, captured, base if base != languages.pack_base(code) else None, False)
              for base in bases or [languages.pack_base(code)]])
    keys = left_to_base(code, captured, bases)
    if keys:
        print(f"  {len(keys)} key(s) of strings.ini left to the base differ between the captured languages "
              f"(set them in strings.override.ini), e.g. [{keys[0][0]}] {keys[0][1]}")
    return ok and not keys


def check_fonts(code, captured, font_code=None, depot_note=True):
    cp = codepage(code)
    problems = []
    items = texts(code)
    fonts_folder = os.path.join(captured, font_code or languages.pack_base(code), "fonts")   # a translation: its base's
    fonts, cyrillic = {}, {}
    for vid, name in FONT_FILES.items():
        path = find_file(fonts_folder, name)
        if path:
            fonts[vid] = glyphs(path)
        # the English fonts keep the game's original Cyrillic in 0xC0-0xFF: in a pack of another
        # code page a letter drawn with the very same picture comes out as the Cyrillic one
        path = find_file(os.path.join(captured, "en", "fonts"), name)
        if path and cp != 1251:
            cyrillic[vid] = {b: c for b, c in glyphs(path).items() if b >= 0xC0}
    missing = {}   # (font, byte, char, what) -> places
    for where, data, used in items:
        try:
            text = data.decode(f"cp{cp}")
        except UnicodeDecodeError as e:
            problems.append(f"{where}: byte 0x{data[e.start]:02X} is not in code page {cp}")
            continue
        for byte, char in zip(data, text):
            if byte == 0x09:   # a tab, as the English l19\win.txt has: blank
                continue
            if byte < 0x20:
                problems.append(f"{where}: control byte 0x{byte:02X}")
                continue
            for vid in used:
                if vid not in fonts:
                    continue
                if byte not in fonts[vid]:
                    missing.setdefault((vid, byte, char, "has no picture"), []).append(where)
                elif vid in cyrillic and cyrillic[vid].get(byte) == fonts[vid][byte]:
                    letter = bytes([byte]).decode("cp1251")
                    missing.setdefault((vid, byte, char, f"is drawn as the Cyrillic {letter!r}"), []).append(where)
    for (vid, byte, char, what), places in sorted(missing.items()):
        problems.append(f"{char!r} (0x{byte:02X}) {what} in {FONT_FILES[vid]} (VID {vid}): "
                        f"{len(places)} line(s), e.g. {places[0]}")
    if len(fonts) != len(FONT_FILES):
        status = "code page only - not captured"
    else:
        status = f"checked with the fonts of {font_code}" if font_code else "fonts checked"
    print(f"{code}: code page {cp}, {len(items)} texts, {status}")
    if depot_note and (len(fonts) != len(FONT_FILES) or font_code):
        depot = {v: k for k, v in languages.DEPOT_LANGUAGES.items()}.get(languages.pack_base(code))
        if depot:
            print(f"  the fonts come with the depot: download_depot {languages.STEAM_APP_ID} {depot}, then build.py")
    for p in problems:
        print("  " + p)
    return not problems


def main():
    sys.stdout.reconfigure(errors="backslashreplace")   # the letters of every code page in any console
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("codes", nargs="*", help="the packs (default: every lang/<code> with a language.ini)")
    parser.add_argument("--captured", default=languages.CAPTURED, help="the captured language files")
    parser.add_argument("--fonts", help="check against the fonts of this pack instead of each pack's own")
    args = parser.parse_args()
    print(f"captures: {args.captured}")
    codes = args.codes or languages.packs()
    ok = all([check(code, args.captured, args.fonts) for code in codes])
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
