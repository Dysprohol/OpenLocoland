# Translating OpenLocoland

This guide explains how the language packs are laid out and how to add or update one.
`DEVELOPERS.md` ("Languages", "Repository layout") has the details.

The repository holds only our work: the texts we add or change. Everything else of a
language comes from the player's own Steam copy - `build.py` (and the installer) capture
each language Steam has into `captured/<code>/` and assemble a pack from that capture with
the files of `lang/<code>/` over it, into `<game>\lang\<code>\`. The game's window, the
launcher and the editor then read the chosen pack first and fall back to the English root.

The installer's own window texts are not part of a pack: they are in `TEXTS` of
`tools/installer.py`.


## Layout of a pack

```
lang/<code>/
├── language.ini                 name, code page, base; the launcher's labels
└── game/
    ├── strings.override.ini     -> strings.ini        changes to the captured file
    ├── briefings/<map>/*.txt    -> maps\<map>\        l01-l20, sandbox (POLIGON), template
    ├── menus/menu*.txt          -> maps\              the menu's pages
    ├── editor/edithelp*.txt     -> maps\              the editor's help pages (F1)
    └── scripts/
        ├── *.lgc.patch          -> maps\script\       the developer mode scripts
        └── shiftb.txt.patch     -> maps\              the developer mode's help
```

The arrow is where `build.py` puts the file in the pack (`languages.pack_path`). A file
ending in `.patch` or `.override.ini` is a change applied to the captured file (the scripts:
to the Steam file); any other file replaces the captured one as a whole. So a pack holds
only the files it changes - a file that is missing comes from the capture.

`lang/_template/` is the starting point of a new translation: a placeholder for every file,
with a `README.md` per folder. It is not a pack (`languages.packs()` leaves out folders
starting with `_`), and no `README.md` is shipped.

Packs now: `cs` (Czech fan translation, `base=en`), `de`, `en`, `hu`, `pl`, `ru`.


## Adding a language

1. Copy `lang/_template` to `lang/<code>/` (ISO 639-1 code, e.g. `sk`).
2. Edit `language.ini`:
   ```ini
   [language]
   name=Slovensky
   ; the code page of the pack's texts: 1250 Central European, 1251 Cyrillic, 1252 Western
   codepage=1250
   ; the language whose Steam files the pack is built on, for a language Steam does not ship
   base=en

   [launcher]
   full_screen=...
   ```
   Copy the `[launcher]` keys from an existing pack (e.g. `lang/en/language.ini`).
3. Translate the files under `game/`, and delete the ones you do not translate - a file of the
   pack replaces the captured one, so a placeholder left in would be shown in the game.
4. Check and build (see below).


## The files

**Encoding.** The game's texts are bytes in the pack's code page (`codepage=`), not UTF-8,
with CRLF line ends. `language.ini` itself is UTF-8.

**strings.override.ini** - only the keys you change, under their sections:
```ini
[strings]
Money=PENIZE
[Editor]
MenuLoad=&Nahrat...\tAlt+F3
```
`key=value` sets the key of the captured `strings.ini`; a section the original does not have
(`[Editor]`, the map editor's texts) is added at its end. A bare `key` line only names a place
to insert the next keys after. Syntax in `tools/patch_text.py`.

**briefings/**, **menus/**, **editor/** - plain text files in the format of the game's own
(at most 44 characters a line in briefings and menu pages; ESC 7 / ESC 2 switch to the lit
font and back). A map's briefing pages are listed by its `brief.lst`.

**scripts/** - unified diffs against the Steam files, made with
`python tools\patch_text.py diff <original> <edited> <patch>`: edit the assembled file
(e.g. `<game>\lang\pl\maps\shiftb.txt`), then make the patch against the original
(`captured\<base>\maps\shiftb.txt`, or the Steam folder's `maps\script\*.lgc`). Translate the
texts and comments; leave the code as it is.


## Fonts

The game draws texts with bitmap fonts (`fonts\*.bmp`, a 16 x 16 grid, one cell per byte). A
pack uses the fonts of its base's capture, so a letter can be in the code page and still have
no picture. `tools/fonts/generate.py --lang <code> --all` makes font pictures from
`fonts/mapping/<code>.json` into `fonts/generated/` (see `fonts/README.md`); the build does not
use them yet, so a pack's texts must stay within the letters its base's fonts have (the Czech
pack is written without diacritics for that reason).


## Checking a translation

```bash
python tools\check_lang.py <code>
```
checks every text of the pack against its code page and the captured fonts (each character
must have a picture, and outside Russian not the original Cyrillic one). Then build into a test
folder and look at it in the game:

```bash
python tools\build.py --target <test folder>
```
```powershell
.\tools\test_game.ps1 -Exe <test folder>\Locoland.exe -Language <code> -ClickOk 1 -SetFull 0
```

The game does not wrap lines, so check the long ones on screen.


## Language codes

| Code | Language   | Code page | Steam depot      |
|------|------------|-----------|------------------|
| en   | English    | 1252      | 352131           |
| de   | German     | 1252      | 352132           |
| hu   | Hungarian  | 1250      | 352133           |
| pl   | Polish     | 1250      | 352134           |
| pt   | Portuguese | 1252      | 352135 (the English files; no pack) |
| ru   | Russian    | 1251      | 352136           |
| cs   | Czech      | 1250      | none (fan translation, `base=en`) |
