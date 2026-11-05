# The Czech edition of the game ("Steamland")

What a tester's copy of the Czech edition holds, compared with the Steam files, and what
OpenLocoland does with it (November 2026). The files themselves stay outside the repository
(see "Distribution" in `DEVELOPERS.md`); only short examples of their texts are quoted here.

## What it is

The game was published in the Czech Republic on CD as **Steamland** by US Action, s.r.o.
(2003, with Gromada and Buka Entertainment in the credits of its `readme.txt`), version
1.02 (`steam.cfg`: `Title=Steamland v 1.02`; the Steam copy says `Locoland v 1.02`, the only
difference between the two files). The copy came as the installed folder
(`...\Gromada\Steamland\`, 1017 files, 529 MB): `Steam.exe`, `Uninstall.exe`, `Install.log`,
`readme.txt` (Czech: requirements, installing from the CD), `steam.cfg`, `strings.ini`,
`objects.res`, `cursores\`, `fonts\`, `maps\`, `music\`, `vid\`, `video\`, `wav\`, and the
player's own `error.log` and `hiscores` files.

## The same as Steam

- `Steam.exe`: byte for byte the Steam exe of 2003-10-07 (SHA-256
  `a31e9e51...076020`, the one `build.py` checks for), so LocoFix and every patch apply to it.
- `objects.res`, every map (`maps\lNN\lNN.map`, `.vid`, `.lgc`, `logo`, `menu`, `outro`), every
  script of `maps\script\`, the `.lst` files, `cursores\`, `music\`, `wav\` and the 524 images
  of `vid\`: identical.
- `fonts\font-unitnumbersonscreen.bmp` and `shiftb.bmp`, `maps\menu2.txt`, `menu3.txt` (empty)
  and `shiftb.txt` (the developer keys, English): identical.

## What differs

- **Texts**: `strings.ini` (the same 369 keys and sections as the English one; 198 values
  translated, 171 the same as English - the empty keys, the `{section:key}` references, names
  such as `GameName=Steamland`, `MapHeader20=KHON-KAA-DOO`, `Units` `25=Mortar`,
  `40=Zeppelin`, `252=Scylla`, `254=Charybdis`), the menu pages `maps\menu*.txt` and every
  mission text `maps\lNN\*.txt` (briefings, hints, objectives, win and loss, mission 1's
  tutorial scripts). CRLF, CP1250 with the Czech letters, in capitals where the English is, the
  lines no wider than the English files' (44 characters in the briefings).
- **Fonts**: `font8x13.bmp`, `font8x13light.bmp`, `font-mousetips.bmp`, `font-objectives*.bmp`,
  `font-mainmenutitles.bmp` and `font5x5.bmp` are redrawn, all 24-bit: the Cyrillic of
  0xC0-0xFF is gone and every Czech letter sits in its CP1250 cell - the 30 letters (Á C1 á E1,
  Č C8 č E8, Ď CF ď EF, É C9 é E9, Ě CC ě EC, Í CD í ED, Ň D2 ň F2, Ó D3 ó F3, Ř D8 ř F8, Š 8A
  š 9A, Ť 8D ť 9D, Ú DA ú FA, Ů D9 ů F9, Ý DD ý FD, Ž 8E ž 9E) in font8x13, font8x13light,
  font-mousetips and font5x5, the 15 capitals in font-objectives and font-mainmenutitles. These
  are the cells our `lang/cs/fonts/*.png` sheets draw for the Steam fonts, so the edition's fonts
  need nothing from them. Oddities: font-mainmenutitles has a stray Í in cell E5 (å), the
  ASCII cells of mousetips, objectives and the titles differ from the Steam fonts in pixels (a
  redrawn set, the same letters), and `font-yal.bmp` is an extra file no game file refers to
  (a mousetips-like font with Czech letters and a few cells 0x80-0xA8).
- **No `vid\logo.bmp`** (the Steam depots have one per language). The edition's name is
  Steamland, so the English logo ("Steamland") of the root fits it.
- `video\intro.wmv` differs and `publisher1.wmv` stands in for Steam's `publisher2.wmv` (the
  CD's publisher clip); the build leaves `video\` out as before.
- Extra files: `l08\brief2.txt` (empty) and `l15\brief2.txt`, which holds a Russian developer's
  note in CP1251 ("hints: what to write?"); neither is listed in its `brief.lst`, so neither
  shows. `maps\l11\brief3.txt` (a hint page) is there as in English.

## Slips in the edition's texts

Checked every byte of its texts against its own fonts (the way `check_lang.py` checks ours):

- `…` (0x85, no picture in any font) ends `[Messages]` `ReinforcementArrived` and
  `EnemyReinforcementArrived` - the three dots show as a blank; the second also lost its
  diacritics (`NEPRATELSKE`).
- `§` (0xA7, no picture) ends `[Units]` 72, 86, 153, both 251 lines, `[MouseTips]`
  `MenuVid467Dir0`, `[Messages]` `MineSet` and one line each of `menu42.txt` and `menu43.txt`
  (the controls pages): an invisible blank at the end of the text, harmless.
- Typos in headers the big font draws: `StrDiff3=TEŽKÁ` (TĚŽKÁ), `MenuWins=VÍTĚZTVÍ:`
  (VÍTĚZSTVÍ:), `MenuEndWin=VÍTEZSTVÍ` (VÍTĚZSTVÍ).
- `[Units]` has `251` twice (`Vodní drak`, `Bzučák`) as the English has (`Water Dragon`,
  `Buzzer`); the engine reads the first.
- Some English left: `Units` 25 `Mortar`, 40 `Zeppelin`, `GameTime2=MIN.`; the controls pages
  keep the English key names (`LMB TO OBJECT`), as the Polish version does.

## What OpenLocoland does with it

Decided with the maintainer's plan "detect it and use it as the base, our pack on top":

- `languages.py` knows the edition by the SHA-256 of its `strings.ini` (`EDITIONS`:
  `85bec1a9...8401c0` -> `cs`) and `build.py` / Setup capture it like a Steam depot into
  `captured/cs` (`languages.capture_all`, `edition_folders`): from the game folder itself when
  that is the edition (a player without Steam points `build.py --steam` or Setup's Steam folder
  at it - the exe is the Steam one, so everything else works), or from a folder named
  `Steamland` inside or next to the Steam game's folder. By hand:
  `python tools\languages.py capture --steam <the edition's folder>`.
- With `captured/cs` the Czech pack is built on it (`languages.build_base`: a pack's own capture
  before its `base=`) and is no longer marked unofficial - the launcher reads `base=cs` from the
  assembled `language.ini` (`languages.pack_ini`), Setup asks `languages.unofficial`. The
  edition's texts and fonts stay; from `lang/cs/` only what the edition does not have is taken
  (`languages.install`): the editor's help pages, POLIGON's and the template's texts, the keys of
  `strings.override.ini` the edition lacks or leaves empty (`Money`, `NotCD`, `MenuHeader7`,
  the units 56, 165, 250, the `[Editor]` section ...; `patch_text.merge_ini` `fill_only`),
  and the lines marked `!` there - our corrections of the slips above: the three typos, the
  `…` lines (`DORAZILY POSILY...`, `DORAZILY NEPŘÁTELSKÉ POSILY...`) and `MineSet` without its
  `§`; `-251` drops the duplicate as in every pack. The pack's translations of the edition's
  menu pages and mission texts are left out (`languages.translates`), and the font sheets paint
  only empty cells (`font_cells.apply` `only_empty`) - on the edition's fonts none.
- The edition's `§` endings of unit names stay (invisible), as does its wording where it differs
  from the fan translation's (`BLOKÁDA` for mission 1, `ZVOLIT HRÁČE`, `NASTAVENÍ` ...): the
  edition is the base.
- Without the edition nothing changes: the pack builds on the English capture (or another
  captured language) with the fan translation and our letters, as before.

Checked in the game (`tools\test_game.ps1 -Language cs`, both builds): the launcher's list
("Čeština", without the mark on the edition, with it on English), the main menu, the custom maps
page with our keys over the edition's texts, mission 1's briefing (the edition's) and its
objectives in the edition's message font, the editor's help pages; `check_lang.py` clean for
every pack with and without `captured/cs`; `font_cells.py preview cs --base cs` leaves every
font byte for byte.
