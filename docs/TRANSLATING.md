# Translating OpenLocoland

**Welcome!** This guide explains how to add or update a language translation for OpenLocoland.

The game has two separate text domains:
1. **Installer UI** - the Setup.exe window (language selector, options, errors)
2. **Game texts** - everything inside the game (menus, missions, editor, strings.ini)

Both are organized per-language in `lang/<code>/` with a clear, simple structure.


## Quick Start: Adding a New Language

```bash
# 1. Copy the template
cp -r lang/_template lang/sk    # 'sk' = ISO 639-1 code for Slovak

# 2. Edit metadata
# lang/sk/language.ini
#   name = Slovak
#   code = sk
#   codepage = 1250
#   base = en          # or your language if it has a Steam depot

# 3. Translate installer UI
# lang/sk/installer/texts.ini    # all keys, keep placeholders like {version}

# 4. Translate game texts
# lang/sk/game/strings.override.ini     # strings.ini overrides
# lang/sk/game/briefings/l01/brief.txt  # mission texts
# lang/sk/game/menus/menu1.txt          # menu texts
# lang/sk/game/editor/edithelp1.txt     # editor help
# lang/sk/game/scripts/...              # script patches (only text in quotes)

# 5. If new characters needed (e.g., ěščřž)
#    Create fonts/mapping/sk.json (copy czech.json)
#    Add mapping = sk.json to lang/sk/language.ini [font] section

# 6. Test
# python tools/installer.py --silent --lang sk --target test_install

# 7. Commit & PR
git add lang/sk/
git commit -m "Add Slovak translation"
git push && gh pr create
```

That's it! The CI will validate encoding, missing keys, and INI syntax.


## Repository Structure

```
lang/
├── _template/              # COPY THIS for new languages
│   ├── language.ini        # metadata
│   ├── installer/texts.ini # installer UI texts
│   └── game/               # game texts
│       ├── strings.override.ini
│       ├── briefings/      # mission texts (l01-l20, sandbox, template)
│       ├── menus/          # menu*.txt
│       ├── editor/         # edithelp*.txt
│       └── scripts/        # .lgc.patch files
│
├── en/                     # English (base, complete)
├── de/                     # German (Steam)
├── hu/                     # Hungarian (Steam)
├── pl/                     # Polish (Steam)
├── ru/                     # Russian (Steam)
├── cs/                     # Czech (fan, base=en)
└── sk/                     # your new language!
```

### language.ini

```ini
[language]
name = Slovak              # Shown in launcher/installer
code = sk                  # ISO 639-1
codepage = 1250            # 1250=Central EU, 1251=Cyrillic, 1252=Western EU
base = en                  # Steam files to use as base (empty = own depot)

[font]
mapping = sk.json          # Optional: custom font mapping

[installer]
; installer texts in installer/texts.ini
```

### installer/texts.ini

- **Format**: INI, UTF-8
- **Keys**: Fixed (left side), do not change
- **Values**: Translate (right side)
- **Placeholders**: Keep `{version}`, `{path}`, `{files}`, `\n` exactly
- **Sections**: `[metadata]`, `[window]`, `[labels]`, `[depots]`, `[options]`, `[shortcuts]`, `[buttons]`, `[errors]`, `[status]`, `[uninstall]`, `[shortcut_names]`


## Game Texts Details

### strings.override.ini

Overrides keys in Steam's `strings.ini`. Format:
```ini
251 = Water Dragon
252 = Fire Dragon
```
Only include keys you change. Key = numeric ID as string.

### briefings/

Mission texts. Each mission has:
```
briefings/l01/
├── brief.txt      # Main briefing
├── brief2.txt     # Page 2 (if any)
├── brief3.txt     # Page 3 (rare)
├── brief.lst      # Briefing list (optional)
├── loose.txt      # "Loose" text
├── objectives.txt # Objectives list
├── script1.txt    # In-mission script text
├── script2.txt
├── script3.txt
├── script4.txt
└── win.txt        # Victory message
```
Plus `sandbox/` and `template/`.

### menus/

In-game menu texts: `menu1.txt`, `menu6.txt`...`menu54.txt`. Same format as Steam.

### editor/

Editor help (F1): `edithelp1.txt`...`edithelp12.txt`.

### scripts/

Unified diff patches for `.lgc` scripts. **Only translate text in quotes.**
```diff
-    "Cheats enabled"
+    "Czyty włączone"
```
Test: `python tools/map_tool.py check`


## Fonts & Extended Characters

The game uses bitmap fonts (16×16 grid, 256 glyphs). Polish replaces Cyrillic.

### To add characters like ěščřžýáíéóůú:

1. **Create mapping**: `fonts/mapping/sk.json` (copy `czech.json`)
2. **Map CP1250 slots** to Unicode characters
3. **Add to language.ini**:
   ```ini
   [font]
   mapping = sk.json
   ```
4. **Generate BMPs**:
   ```bash
   pip install freetype-py
   python tools/fonts/generate.py --lang sk --all
   ```
5. **Build installer** - picks up `fonts/generated/*.bmp`

See `fonts/README.md` for full details.


## Testing Your Translation

### Installer only (fast)
```bash
python tools/installer.py --silent --lang sk --target test_sk
```

### Full game build
```bash
python tools/build.py <steam_folder> test_build --lang sk
```

### Verify font generation
```bash
python tools/fonts/generate.py --lang sk --all
ls fonts/generated/
```

### Check script patches apply
```bash
python tools/map_tool.py check
```

## CI Validation

On PR, GitHub Actions will check:
- All `.ini` files are valid UTF-8
- All required keys present in `installer/texts.ini`
- `language.ini` has required fields
- Script patches apply cleanly
- No missing briefing files for missions 1-20

## Tips

- **Use a proper editor** (VS Code, Notepad++) with UTF-8 support
- **Test in-game** - some texts have length limits (UI width)
- **Keep tone consistent** - military/railway terminology
- **Don't translate** variable names, code, or placeholders
- **Ask questions** - open an issue or discussion if unsure

## Language Codes Reference

| Code | Language | Codepage | Steam Depot |
|------|----------|----------|-------------|
| en   | English  | 1252     | 352131      |
| de   | German   | 1252     | 352132      |
| hu   | Hungarian| 1250     | 352133      |
| pl   | Polish   | 1250     | 352134      |
| pt   | Portuguese| 1252    | 352135      |
| ru   | Russian  | 1251     | 352136      |
| cs   | Czech    | 1250     | (fan, base=en) |
| sk   | Slovak   | 1250     | (fan, base=en) |
| ...  | your language | ... | ... |

---

**Questions?** Open a GitHub Issue or Discussion. Thanks for contributing!