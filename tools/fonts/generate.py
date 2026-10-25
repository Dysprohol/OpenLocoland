#!/usr/bin/env python3
"""
Font Generator for OpenLocoland

Generates bitmap fonts (.bmp) from TTF sources using character mappings.
Requires: Pillow, freetype-py
"""
import argparse
import json
import os
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    print("Error: Pillow required. pip install Pillow")
    sys.exit(1)

try:
    import freetype
except ImportError:
    print("Error: freetype-py required. pip install freetype-py")
    sys.exit(1)

ROOT = Path(__file__).parent.parent
FONTS_DIR = ROOT / "fonts"
SOURCE_DIR = FONTS_DIR / "source"
MAPPING_DIR = FONTS_DIR / "mapping"
GENERATED_DIR = FONTS_DIR / "generated"

# Default font sizes (can be overridden in mapping)
DEFAULT_SIZES = {
    "font8x13.bmp": 13,
    "font-MainMenuTitles.bmp": 18,
    "font-Mousetips.bmp": 12,
    "font8x13light.bmp": 13,
    "font-Objectives.bmp": 14,
}

# 16x16 grid = 256 glyphs
GRID_W, GRID_H = 16, 16
CELL_W, CELL_H = 8, 13  # base cell size for font8x13


def load_mapping(lang_code):
    """Load and merge latin-extended + language-specific mapping."""
    base_path = MAPPING_DIR / "latin-extended.json"
    lang_path = MAPPING_DIR / f"{lang_code}.json"

    with open(base_path, encoding="utf-8") as f:
        base = json.load(f)

    if lang_path.exists():
        with open(lang_path, encoding="utf-8") as f:
            lang = json.load(f)
        # Merge: language overrides base
        merged = {**base}
        if "characters" in lang:
            merged["characters"] = {**base.get("characters", {}), **lang["characters"]}
        if "cp1250_slots" in lang:
            merged["cp1250_slots"] = {**base.get("cp1250_slots", {}), **lang["cp1250_slots"]}
        if "fonts" in lang:
            merged["fonts"] = {**base.get("fonts", {}), **lang["fonts"]}
        return merged
    return base


def get_unicode_char(char_name, mapping):
    """Resolve character name to Unicode codepoint."""
    # Direct Unicode (U+XXXX)
    if char_name.startswith("U+"):
        return chr(int(char_name[2:], 16))
    # Named character from mapping
    if "characters" in mapping and char_name in mapping["characters"]:
        return get_unicode_char(mapping["characters"][char_name], mapping)
    # Try as hex
    try:
        return chr(int(char_name, 16))
    except ValueError:
        pass
    return None


def generate_font(font_name, mapping, output_path, ttf_path, font_size):
    """Generate a single BMP font."""
    print(f"  Generating {font_name} at {font_size}pt...")

    # Load TTF with freetype
    face = freetype.Face(str(ttf_path))
    face.set_char_size(font_size * 64)  # 26.6 fixed point

    # Create canvas: 16x16 grid
    img_w = GRID_W * CELL_W
    img_h = GRID_H * CELL_H
    canvas = Image.new("RGB", (img_w, img_h), (0, 0, 0))
    draw = ImageDraw.Draw(canvas)

    # Pilot font for rendering (Pillow)
    pilot = ImageFont.truetype(str(ttf_path), font_size)

    # Determine which slots to fill
    slots = mapping.get("cp1250_slots", {})
    fonts_config = mapping.get("fonts", {}).get(font_name, {})
    glyph_overrides = fonts_config.get("glyphs", {})

    filled = 0
    for slot_hex, char_name in slots.items():
        slot = int(slot_hex, 16)
        if slot >= 256:
            continue

        # Check for font-specific override
        actual_char = glyph_overrides.get(slot_hex, char_name)
        ch = get_unicode_char(actual_char, mapping)

        if ch is None:
            continue

        # Render with freetype for accuracy
        face.load_char(ch)
        bitmap = face.glyph.bitmap
        if bitmap.width == 0 or bitmap.rows == 0:
            continue

        # Convert to PIL image
        glyph_img = Image.frombytes(
            "L", (bitmap.width, bitmap.rows), bitmap.buffer, "raw", "L", 0, 1
        )

        # Position in grid
        gx = (slot % GRID_W) * CELL_W
        gy = (slot // GRID_W) * CELL_H

        # Center glyph in cell
        ox = gx + (CELL_W - bitmap.width) // 2
        oy = gy + (CELL_H - bitmap.rows) // 2 + face.glyph.bitmap_top

        # Paste (white on black)
        for y in range(bitmap.rows):
            for x in range(bitmap.width):
                v = bitmap.buffer[y * bitmap.width + x]
                if v > 0:
                    px, py = ox + x, oy + y
                    if 0 <= px < img_w and 0 <= py < img_h:
                        canvas.putpixel((px, py), (255, 255, 255))
        filled += 1

    # Also render ASCII printable (32-126) if not mapped
    for cp in range(32, 127):
        if cp in [int(k, 16) for k in slots.keys()]:
            continue
        face.load_char(chr(cp))
        bitmap = face.glyph.bitmap
        if bitmap.width == 0 or bitmap.rows == 0:
            continue
        glyph_img = Image.frombytes(
            "L", (bitmap.width, bitmap.rows), bitmap.buffer, "raw", "L", 0, 1
        )
        # Find empty slot or skip
        # For simplicity, skip unmapped ASCII - they should be in mapping

    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output_path)
    print(f"    Saved: {output_path} ({filled} mapped glyphs)")


def main():
    parser = argparse.ArgumentParser(description="Generate bitmap fonts for OpenLocoland")
    parser.add_argument("--lang", help="Language code (e.g., cs, pl, sk)")
    parser.add_argument("--font", help="Specific font to generate (e.g., font8x13.bmp)")
    parser.add_argument("--list", action="store_true", help="List available mappings")
    parser.add_argument("--all", action="store_true", help="Generate all fonts for --lang")
    args = parser.parse_args()

    if args.list:
        print("Available language mappings:")
        for f in MAPPING_DIR.glob("*.json"):
            if f.name != "latin-extended.json":
                print(f"  {f.stem}")
        return

    if not args.lang:
        parser.error("--lang required (or --list)")

    mapping = load_mapping(args.lang)

    fonts_to_generate = []
    if args.font:
        fonts_to_generate = [args.font]
    elif args.all:
        fonts_to_generate = list(DEFAULT_SIZES.keys())
    else:
        # Default: generate the main font
        fonts_to_generate = ["font8x13.bmp"]

    for font_name in fonts_to_generate:
        ttf_name = font_name.replace(".bmp", ".ttf")
        ttf_path = SOURCE_DIR / ttf_name
        if not ttf_path.exists():
            print(f"Warning: Source font not found: {ttf_path}")
            continue

        font_size = DEFAULT_SIZES.get(font_name, 13)
        output_path = GENERATED_DIR / font_name

        generate_font(font_name, mapping, output_path, ttf_path, font_size)

    print(f"\nDone. Generated fonts in: {GENERATED_DIR}")


if __name__ == "__main__":
    main()