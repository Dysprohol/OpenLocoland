r"""A language pack's own letters, pasted into the fonts of the language it is built on.

    python tools\font_cells.py preview <code> [--base <code>] [--out <folder>] [--zoom 6]

The game draws text with the font pictures fonts\*.bmp: a 16 x 16 grid of cells, byte 0x00 in
the top-left cell, every cell with a one-pixel grid line on its right and bottom (the colour of
which the game does not draw). A pack whose letters the captured fonts do not have - Czech, a
translation Steam does not ship - keeps them in lang/<code>/fonts/<font>.png: a sheet the size of
the font with only our marks in it, each in the cell of the byte it is for, in four colours:

    transparent      the base font's pixel stays
    white            ink: the colour of the font's letters
    grey (128)       shadow: the font's second letter colour (font-Objectives' dark edge)
    black            paper: the font's background (erases a pixel of the letter)

When a pack is assembled (languages.install) every cell of the sheet that holds anything is
made from the base font's own Latin letter of that byte - the letter without its marks, from
the pack's code page (0xC8 of CP1250, Č: C) - with the sheet's pixels painted over it in the
colours of that very font file. A sheet <font>.png applies to every font of the base whose name
starts with <font>: font8x13.png also to font8x13light.bmp, font-Objectives.png also to its
-green, -red and -yellow copies, each in its own colours. The file keeps its own format (16- or
24-bit, Steam's header) and only the sheet's cells change, so the fonts of any captured language
take the letters (on the Russian fonts they replace Cyrillic, on the Polish ones the Polish
letters of the same bytes).

`preview` writes the pack's fonts as they would be assembled on a captured language (default:
the pack's base) into a folder, with a picture of the changed cells enlarged next to the base
letters, for checking the cells by eye.
"""
import argparse
import os
import struct
import sys
import unicodedata

TRANSPARENT, INK, SHADOW, PAPER = None, (255, 255, 255), (128, 128, 128), (0, 0, 0)
ROLES = {INK: "ink", SHADOW: "shadow", PAPER: "paper"}


class Bitmap:
    """An uncompressed 16-bit (5-5-5) or 24-bit BMP as the game's fonts are, with its pixels as the
    raw bytes of the file: what is not changed is written back byte for byte."""

    def __init__(self, data):
        if data[:2] != b"BM":
            raise ValueError("not a BMP")
        self.data = bytearray(data)
        self.offset = struct.unpack_from("<I", data, 10)[0]
        self.width, height = struct.unpack_from("<ii", data, 18)
        self.bpp, compression = struct.unpack_from("<HI", data, 28)
        if self.bpp not in (16, 24) or compression != 0:
            raise ValueError(f"a {self.bpp}-bit BMP with compression {compression}, not 16- or 24-bit RGB")
        self.bottom_up = height > 0
        self.height = abs(height)
        self.size = self.bpp // 8
        self.stride = (self.width * self.size + 3) & ~3
        if self.offset + self.stride * self.height > len(data):
            raise ValueError("BMP shorter than its pixels")

    def _at(self, x, y):
        row = self.height - 1 - y if self.bottom_up else y
        return self.offset + row * self.stride + x * self.size

    def get(self, x, y):
        """The raw pixel (bytes)."""
        i = self._at(x, y)
        return bytes(self.data[i:i + self.size])

    def put(self, x, y, raw):
        i = self._at(x, y)
        self.data[i:i + self.size] = raw

    def cell(self, byte):
        """(x0, y0, width, height) of the cell of `byte`."""
        cw, ch = self.width // 16, self.height // 16
        return (byte % 16) * cw, (byte // 16) * ch, cw, ch


def colours(bitmap):
    """{"ink", "shadow", "paper"} -> raw pixel of a font: the paper is the space's, the ink the most
    common other colour of the capital letters, the shadow the next one (the grid line's colour, of
    the cells' last column and row, left out)."""
    x0, y0, cw, ch = bitmap.cell(0x20)
    paper, grid = bitmap.get(x0, y0), bitmap.get(x0 + cw - 1, y0 + ch - 1)
    count = {}
    for byte in range(ord("A"), ord("Z") + 1):
        x0, y0, cw, ch = bitmap.cell(byte)
        for y in range(ch - 1):
            for x in range(cw - 1):
                p = bitmap.get(x0 + x, y0 + y)
                if p not in (paper, grid):
                    count[p] = count.get(p, 0) + 1
    ranked = sorted(count, key=lambda p: -count[p])
    ink = ranked[0] if ranked else paper
    return {"paper": paper, "ink": ink, "shadow": ranked[1] if len(ranked) > 1 else ink}


def base_letter(byte, codepage):
    """The byte of the Latin letter a letter of the code page is made from (Č -> C), or None."""
    try:
        char = bytes([byte]).decode(f"cp{codepage}")
    except UnicodeDecodeError:
        return None
    letter = unicodedata.normalize("NFD", char)[0]
    return ord(letter) if letter.isascii() and letter.isalpha() and letter != char else None


def sheet_cells(sheet_path):
    """{byte: [(x, y, role)]} of a sheet: the pixels of each cell that hold anything."""
    from PIL import Image
    image = Image.open(sheet_path).convert("RGBA")
    width, height = image.size
    cw, ch = width // 16, height // 16
    pixels = image.load()
    cells = {}
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            role = ROLES.get((r, g, b))
            if role is None:
                raise ValueError(f"{sheet_path}: pixel {x},{y} has colour {r},{g},{b} - only white, grey 128, "
                                 "black or transparent")
            cells.setdefault((y // ch) * 16 + x // cw, []).append((x % cw, y % ch, role))
    return image.size, cells


def apply(font_data, sheet_path, codepage):
    """The font file `font_data` with the cells of a sheet made from its own Latin letters (see the
    module's text). Raises ValueError when the sheet does not fit the font."""
    bitmap = Bitmap(font_data)
    size, cells = sheet_cells(sheet_path)
    if size != (bitmap.width, bitmap.height):
        raise ValueError(f"{sheet_path} is {size[0]} x {size[1]}, the font {bitmap.width} x {bitmap.height}")
    paint = colours(bitmap)
    for byte, pixels in sorted(cells.items()):
        letter = base_letter(byte, codepage)
        if letter is None:
            raise ValueError(f"{sheet_path}: cell 0x{byte:02X} is not a letter with a mark in code page {codepage}")
        sx, sy, cw, ch = bitmap.cell(letter)
        tx, ty, _, _ = bitmap.cell(byte)
        letter_pixels = [[bitmap.get(sx + x, sy + y) for x in range(cw)] for y in range(ch)]
        for y in range(ch):
            for x in range(cw):
                bitmap.put(tx + x, ty + y, letter_pixels[y][x])
        for x, y, role in pixels:
            bitmap.put(tx + x, ty + y, paint[role])
    return bytes(bitmap.data)


def sheets(pack_folder):
    """{font name without .png (lower case): sheet path} of lang/<code>/fonts/."""
    folder = os.path.join(pack_folder, "fonts")
    if not os.path.isdir(folder):
        return {}
    return {f[:-4].lower(): os.path.join(folder, f) for f in sorted(os.listdir(folder)) if f.lower().endswith(".png")}


def sheet_for(font_name, pack_sheets):
    """The sheet that applies to a font file name of the base (the longest name it starts with), or None."""
    stem = os.path.splitext(font_name)[0].lower()
    names = [n for n in pack_sheets if stem.startswith(n)]
    return pack_sheets[max(names, key=len)] if names else None


def preview(code, base=None, out=None, zoom=6):
    """Writes the fonts of pack `code` assembled on captured language `base` into `out`, with
    <font>.cells.png: each changed cell, enlarged, under the base's cell of that byte and its letter."""
    from PIL import Image
    import languages
    base = base or languages.build_base(code)
    if not base:
        sys.exit(f"{code}: no captured language to build it on")
    out = out or os.path.join(languages.REPO, "build", f"fonts-{code}-on-{base}")
    os.makedirs(out, exist_ok=True)
    pack_sheets = sheets(os.path.join(languages.LANG, code))
    codepage = int(languages.pack_value(code, "codepage", "1252"))
    folder = os.path.join(languages.CAPTURED, base, "fonts")
    for name in sorted(os.listdir(folder)):
        sheet = sheet_for(name, pack_sheets)
        if not sheet:
            continue
        original = open(os.path.join(folder, name), "rb").read()
        made = apply(original, sheet, codepage)
        open(os.path.join(out, name), "wb").write(made)
        _, cells = sheet_cells(sheet)
        before, after = Image.open(os.path.join(folder, name)).convert("RGB"), Image.open(os.path.join(out, name)).convert("RGB")
        cw, ch = before.width // 16, before.height // 16
        picture = Image.new("RGB", (len(cells) * (cw + 2) * zoom, (3 * ch + 4) * zoom), (255, 0, 255))
        for i, byte in enumerate(sorted(cells)):
            for row, (image, b) in enumerate(((before, base_letter(byte, codepage)), (before, byte), (after, byte))):
                x0, y0 = (b % 16) * cw, (b // 16) * ch
                piece = image.crop((x0, y0, x0 + cw, y0 + ch)).resize((cw * zoom, ch * zoom), Image.NEAREST)
                picture.paste(piece, (i * (cw + 2) * zoom, row * (ch + 2) * zoom))
        picture.save(os.path.join(out, os.path.splitext(name)[0] + ".cells.png"))
        print(f"{name}: {len(cells)} cells -> {out}")


def main():
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("command", choices=["preview"])
    p.add_argument("code", help="the pack (lang/<code>/fonts/)")
    p.add_argument("--base", help="the captured language to build it on (default: as build.py would)")
    p.add_argument("--out", help="the folder to write into (default: build/fonts-<code>-on-<base>/)")
    p.add_argument("--zoom", type=int, default=6)
    a = p.parse_args()
    preview(a.code, a.base, a.out, a.zoom)


if __name__ == "__main__":
    main()
