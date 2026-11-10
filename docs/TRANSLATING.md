# Translating OpenLocoland

This guide explains how the language packs are laid out and how to add or update one.
`DEVELOPERS.md` ("Languages", "Repository layout") has the details.

The repository holds only our work: the texts we add or change. Everything else of a
language comes from the player's own Steam copy - `build.py` (and the installer) capture
each language Steam has into `captured/<code>/` and assemble a pack from that capture with
the files of `lang/<code>/` over it, into `<game>\lang\<code>\`. The game's window, the
launcher and the editor then read the chosen pack first and fall back to the root (English, or
the Steam language when there is no English capture).

The installer's own window texts are not part of a pack: they are in `TEXTS` of
`tools/installer.py`.


## Layout of a pack

```
lang/<code>/
├── language.ini                 name, code page, base; the launcher's labels
├── fonts/<font>.png             -> fonts\<font>*.bmp  the pack's own letters (see "Fonts")
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
to the Steam file), and so is a font sheet (its cells pasted into the captured fonts); any
other file replaces the captured one as a whole. So a pack holds only the files it changes - a
file that is missing comes from the capture.

`lang/_template/` is the starting point of a new translation: a placeholder for every file,
with a `README.md` per folder. It is not a pack (`languages.packs()` leaves out folders
starting with `_`), and no `README.md` is shipped.

Packs now: `cs` (Czech: a fan translation on `base=en`, or on the Czech edition's own files when a
player has them - "A capture of the pack's own language" below), `de`, `en`, `hu`, `pl`, `ru`.


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
   Copy the `[launcher]` keys from an existing pack (e.g. `lang/en/language.ini`); `unofficial=`
   is the mark the launcher puts after a fan translation's name in this language.
3. Translate the files under `game/`, and delete the ones you do not translate - a file of the
   pack replaces the captured one, so a placeholder left in would be shown in the game.
4. Check and build (see below).


## The files

**Encoding.** The game's texts are bytes in the pack's code page (`codepage=`), not UTF-8,
with CRLF line ends. `language.ini` itself is UTF-8.

**The editor's help** - `editor/edithelp<n>.txt`, one file per page in the order of
`game/maps/edithelp.lst` (shared by every pack): the first line is the page's title, which the
help's contents list as well; at most 14 lines of 44 characters (ESC 7 / ESC 2, the lit font and
back, not counted). The contents group the pages by the categories of `edithelp.lst` (its second
column, an `[Editor]` key `HelpCat...`); translate those keys in `strings.override.ini` with
`HelpContents` and `HelpPageNumber` - a category's name is the title of its pages in the big
title font, so at most 19 characters. A new page is a new line of `edithelp.lst` (the file, the
category's key and its English name, tab-separated; the file alone puts it in the category of the
page before) and its file in each pack.

**strings.override.ini** - only the keys you change, under their sections:
```ini
[strings]
Money=PENIZE
[Editor]
MenuLoad=&Nahrat...\tAlt+F3
```
`key=value` sets the key of the captured `strings.ini`; a section the original does not have
(`[Editor]`, the map editor's texts) is added at its end. A bare `key` line only names a place
to insert the next keys after, `-key` drops the later duplicates of a key, and `!key=value`
corrects a value of an edition in the pack's own language (below). Syntax in `tools/patch_text.py`.

In the editor's `Menu*` entries `&` marks the letter that opens the item with Alt (exactly one
per entry, and no two items of one menu on the same letter), `\t` separates the keyboard
shortcut shown at the right (`\tAlt+F3`; keep the shortcut as it is), and `&&` writes an
ampersand itself.

**briefings/**, **menus/**, **editor/** - plain text files in the format of the game's own
(at most 44 characters a line in briefings and menu pages; ESC 7 / ESC 2 switch to the lit
font and back). A map's briefing pages are listed by its `brief.lst`.

**scripts/** - unified diffs against the Steam files, made with
`python tools\patch_text.py diff <original> <edited> <patch>`: edit the assembled file
(e.g. `<game>\lang\pl\maps\shiftb.txt`), then make the patch against the original
(`captured\<base>\maps\shiftb.txt`, or the Steam folder's `maps\script\*.lgc`). Translate the
texts and comments; leave the code as it is.


## Fonts

The game draws texts with bitmap fonts (`fonts\*.bmp`, a 16 x 16 grid, one cell per byte of the
code page). A pack uses the fonts of its base's capture, so a letter can be in the code page and
still have no picture: the English fonts hold Cyrillic in 0xC0-0xFF, the Polish ones the Polish
letters, the German and Hungarian ones theirs.

A pack whose letters its base's fonts do not have draws them itself, in `fonts/<font>.png`: a
picture the size of the font with only its marks, each in the cell of its byte - white the
letters' colour, grey 128 the shadow of font-Objectives, black the background (erases a pixel),
transparent the base's pixel. When the pack is assembled each such cell is made from the base
font's own Latin letter (for 0xC8, Č in CP1250: the C) with these pixels painted over it in that
font's colours (`tools/font_cells.py`). A sheet applies to every font whose name starts with its
own - `font8x13.png` also to `font8x13light.bmp`, `font-Objectives.png` to its `-green`, `-red`
and `-yellow` copies. Draw the marks with any editor that keeps the four colours exact, and look
at the result with

```bash
python tools\font_cells.py preview <code>
```

(the assembled fonts and every changed cell enlarged, in `build\fonts-<code>-on-<base>\`).

The Czech pack has its own: the letters of Czech in their CP1250 bytes (Á C1 á E1, Č C8 č E8,
Ď CF ď EF, É C9 é E9, Ě CC ě EC, Í CD í ED, Ň D2 ň F2, Ó D3 ó F3, Ř D8 ř F8, Š 8A š 9A, Ť 8D ť 9D,
Ú DA ú FA, Ů D9 ů F9, Ý DD ý FD, Ž 8E ž 9E) in font8x13 and font8x13light (the texts), font-Mousetips
(the tips) and, capitals only as those fonts are, font-Objectives (the messages) and
font-MainMenuTitles (the headers). So its texts are written with diacritics, in CP1250; any
other letter beyond ASCII still has no picture. font5x5, font-unitnumbersonscreen and
shiftb.bmp (the developer menu) are left as they are.


## A fan translation on another base

A pack with `base=` another language is a fan translation: the launcher and Setup mark it
"(unofficial)". Many players have only one Steam language, so when its base is not captured
`build.py` builds it on another captured language (`languages.build_base`: en, pt, de, pl, hu,
ru, the first there is) - the Czech pack on the Polish files for a player with the Polish Steam
copy. Then:

- the fonts and the logo are that language's: keep the texts to ASCII, which every font set
  draws, and the letters of the pack's own font sheets, which are pasted into whichever fonts it
  is built on (`check_lang.py` checks a fan pack with each captured font set, its cells pasted in);
- the files the pack does not have come from that language: for Czech these are `menu2.txt` and
  `menu3.txt` (empty in every version), `shiftb.txt` (English in every version) and the Polish
  version's empty `l08\brief2.txt` and `l15\brief2.txt` - nothing of the base's language shows;
- `strings.ini` is that language's under the pack's `strings.override.ini`, and the versions
  differ in more than the words: Polish leaves keys empty that English fills with references
  such as `MenuGamer={strings:HeadPlayerName}:`. Set every such key in the override;
  `check_lang.py` lists those the pack leaves to its base.

A pack that really needs its base's files (letters only those fonts have) says `needs_base=1` in
`[language]`; Setup then lists it greyed with that language's `download_depot` line.


## A capture of the pack's own language

When a player has an edition of the game in the pack's language that Steam does not ship (the
Czech edition sold on CD, `docs/dev/czech-edition.md`), `build.py` captures it into `captured/<code>`
(`languages.EDITIONS` names such editions by the SHA-256 of their `strings.ini`; the folder is the
game folder itself, or one named `Steamland` in or next to the Steam game's) and builds the pack
on it instead of on `base=`. The edition's texts and fonts then stay: of the pack only its own
files are taken (the editor's help pages, POLIGON's and the template's texts; a translation of a
menu page or a mission text the capture has is left out), `strings.override.ini` fills only the
keys the capture lacks or leaves empty, and a line `!key=value` corrects a value of the edition
itself (on any other base the line is ignored). The font sheets paint only empty cells. The
launcher shows no "(unofficial)" then, and `check_lang.py` checks the pack with that capture's
fonts as well.


## Checking a translation

```bash
python tools\check_lang.py <code>
```
checks every text of the pack against its code page and the captured fonts, with the pack's
own letters pasted in (each character must have a picture, and outside Russian not the original
Cyrillic one). Then build into a test folder and look at it in the game:

```bash
python tools\build.py --target <test folder>
```
```powershell
.\tools\test_game.ps1 -Exe <test folder>\Locoland.exe -Language <code> -ClickOk 1 -SetFull 0
```

The game does not wrap lines, so check the long ones on screen.


## For translators without git

A volunteer does not need git, Python or the code pages: we send a folder of UTF-8 files, they
translate them in any editor, they send the folder back, and one command checks it and commits it
in their name.

```bash
python tools\translate_kit.py export <code> <folder> [--ours | --game | --all] [--zip]
```

writes the pack's texts into `<folder>` (and with `--zip` also `<folder>.zip`, the one to send):

```
<folder>/
├── README.txt                   the rules, in English, for the translator
├── TODO.txt                     what is left (computed, below)
├── language.ini                 the launcher's labels
├── installer.txt                Setup's window texts (TEXTS of tools/installer.py), key=value
└── game/
    ├── strings.override.ini     the keys of the scope
    ├── briefings/<map>/*.txt    each with <name>.en.txt beside it: the English page
    ├── menus/menu*.txt
    └── editor/edithelp*.txt
```

Every file is UTF-8, converted from the pack's code page, at the pack's own path. Beside each page
is its English original (`brief.en.txt` beside `brief.txt`): the game's texts from `captured/en`,
ours from the English at the root (`game/maps/edithelp*.txt`, `game/maps.new/<map>/*.txt`). In the
key files the English of each key is on a `;EN> ...` line above it; the help pages' font switches
(ESC 7 / ESC 2) are written `{F7}` / `{F2}`. A text the pack does not have yet - or has only as
`lang/_template`'s placeholder - is written as the English, so a new language (a copy of
`lang/_template`, see "Adding a language") gets the whole English as its starting point.

The scope: `--ours` is what we add to the game - the editor's help pages, our keys of
`strings.override.ini` (those `lang/en/game/strings.override.ini` defines), POLIGON's and the
template's texts, the launcher's labels and Setup's; `--game` the game's own texts - the missions'
briefings, the menu pages and the other keys of `strings.ini`; `--all` both. Without one, a fan
translation (`base=` another language, Czech) gets `--all` and a language Steam ships `--ours`:
its game texts are Steam's.

`TODO.txt` lists the pages still in English (or most of their lines), the pages without any letter
outside ASCII (for every language but English: diacritics still to come), the keys of ours the
pack lacks (in the kit with the English text, to translate) and the values still the same as the
English. `README.txt` gives the rules: the widths per kind of file (44 characters in the
briefings, menu pages and help pages, 40 in `objectives.txt`, 54 in the tutorial's `script*.txt`,
or the English line's at that place when it is wider; 14 lines a help page), capitals where the
original has them, the section names, keys, `%s` `%d` `%1` `{name}` `\t` `&` and the file names
unchanged, the language written normally with its letters, and to send the whole folder back.

```bash
python tools\translate_kit.py import <code> <folder or zip> --author "Name <email>" [--dry-run] [--partial]
```

reads the files back (`*.en.txt`, `README.txt` and `TODO.txt` are left out), converts each to the
pack's code page and checks it, and prints a report: the accepted files with the number of lines
or keys changed, the rejected ones with each line and its reason, the unchanged ones by count. A
file is rejected for a character the code page has no byte for (its line and column), a character
the pack's fonts cannot draw (`check_lang.py`'s check, with every font set a fan pack may be built
on), a line wider than its limit, a help page over 14 lines, sections or keys changed or moved, a
value whose `%` conversions (and their order), `{fields}`, `\t` or single `&` differ from the
English, a setting of `language.ini` that is ours (`codepage`, `base`), or `installer.txt`'s
`{fields}` against `TEXTS`. A line in small letters where the English has capitals, a page much
longer than the English and a changed number of `\n` are warnings. A page left as the English that
the pack did not have is not added.

The accepted files are written into `lang/<code>/` with the line endings of the file there (CRLF
in the pages, LF in `strings.override.ini`; a new page CRLF), only the changed keys of a key file
rewritten in place (a new key after the one before it) and `installer.txt` into `TEXTS[<code>]` of
`tools/installer.py`. Then `check_lang.py` runs for the pack - it must pass, or the files are put
back - and one commit is made with `--author`, the translator's name and e-mail, so GitHub credits
them, and a message naming the pack and the file groups. Our own corrections afterwards are a
commit of their own, by hand. A rejected file stops it all - nothing written, nothing committed -
unless `--partial`; `--dry-run` only reports, and the files to import must have no changes of
their own in the checkout. The kit's `README.txt` names the commit it was exported from; when a
file of the pack has changed since, the report says so (the translator's file would replace it).
Both commands take `--captured <folder>` for the captures (`languages.CAPTURED` by default).


## Language codes

| Code | Language   | Code page | Steam depot      |
|------|------------|-----------|------------------|
| en   | English    | 1252      | 352131           |
| de   | German     | 1252      | 352132           |
| hu   | Hungarian  | 1250      | 352133           |
| pl   | Polish     | 1250      | 352134           |
| pt   | Portuguese | 1252      | 352135 (the English files; no pack) |
| ru   | Russian    | 1251      | 352136           |
| cs   | Czech      | 1250      | none (fan translation, `base=en`; any captured language without it; the Czech CD edition's own files when at hand) |
