# Font Generation System

## Overview

The game uses bitmap fonts (`.bmp` files, 16×16 grid = 256 glyphs). Polish letters replace Cyrillic in the original fonts. This system generates BMP fonts from TTF sources with custom character mappings.

## Structure

```
fonts/
├── source/              # Source TTF/OTF fonts (SIL OFL licensed)
│   ├── font8x13.ttf
│   ├── font-MainMenuTitles.ttf
│   ├── font-Mousetips.ttf
│   ├── font8x13light.ttf
│   └── font-Objectives.ttf
├── mapping/             # Character mapping JSON files
│   ├── latin-extended.json   # Full CP1250 character table
│   ├── czech.json            # Czech-specific overrides
│   └── <lang>.json           # Per-language mappings
├── generated/             # Output BMPs (gitignored, created at build)
│   ├── font8x13.bmp
│   └── ...
├── generate.py            # Generation script
└── README.md
```

## How It Works

1. **Source TTF** → loaded with FreeType (via `freetype-py`)
2. **Mapping JSON** → defines which Unicode char goes to which CP1250 slot
3. **Generate** → renders each glyph at correct size, packs into 16×16 BMP
4. **Output** → `fonts/generated/*.bmp` used by installer at build time

## Per-Language Mapping

Each language can have its own mapping file (e.g., `czech.json`) that extends `latin-extended.json`:
- Only specify glyphs that differ from the base mapping
- `codepage` must match the language's `language.ini` codepage

## Usage

```bash
# Generate all fonts for a language
python tools/fonts/generate.py --lang cs

# Generate specific font
python tools/fonts/generate.py --lang cs --font font8x13.bmp

# List available mappings
python tools/fonts/generate.py --list
```

## Adding a New Language

1. Create `fonts/mapping/<lang>.json` (copy `czech.json` as template)
2. Add TTF to `fonts/source/` if needed (check license!)
3. Add `mapping = <lang>.json` to `lang/<lang>/language.ini` under `[font]`
4. Run generation: `python tools/fonts/generate.py --lang <lang>`
5. Build installer - it will pick up generated BMPs

## Licensing

- Only use SIL OFL or similarly permissive fonts in `source/`
- Generated BMPs are derivative works - same license applies
- Document font sources in each mapping file

## Current Fonts

| Font File | Size | Purpose |
|-----------|------|---------|
| `font8x13.bmp` | 8×13 | Main game text |
| `font-MainMenuTitles.bmp` | variable | Main menu titles |
| `font-Mousetips.bmp` | variable | Tooltips |
| `font8x13light.bmp` | 8×13 | Light variant |
| `font-Objectives.bmp` | variable | Mission objectives |

## Dependencies

- `Pillow` (already in requirements)
- `freetype-py` (needs `pip install freetype-py`)
- System: `libfreetype6` (Linux) / included in Windows Python