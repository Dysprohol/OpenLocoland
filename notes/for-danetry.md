# For Danetry: the Czech translation kit

A message to paste into Discord (two parts, each under Discord's 2000 characters).

---

**Part 1/2**

Hi Danetry! To make the Czech translation easier for you (no git needed), I made a translation kit.

**What you get:** a zip, `openlocoland-cs.zip`. Unpack it anywhere. Inside are all the Czech texts of the game as plain text files, in the same folders as in the pack (`game/briefings/l02/brief.txt`, `game/menus/menu41.txt`, `game/editor/edithelp6.txt`, `game/strings.override.ini` ...). Beside each page there is its English original, `brief.en.txt` beside `brief.txt` - only for reference. In `strings.override.ini`, `language.ini` and `installer.txt` the line `;EN> ...` above each key shows the English.

`TODO.txt` lists what is left, `README.txt` has the rules in full.

**How to edit:** any editor that saves UTF-8 - Notepad++, VS Code, even Windows Notepad. Write Czech normally, with all the letters (č ř ů ...). We convert the files to the game's encoding ourselves; please don't convert anything. Keep the file names and folders, don't add or remove files.

**The rules** (a tool checks them and tells me the exact line when something breaks):
1. A line may not be wider than the English one: 44 characters in briefings, menu pages and help pages, 40 in `objectives.txt`, 54 in the tutorial's `script*.txt`. The game does not wrap text, so break long sentences yourself. A help page has at most 14 lines.
2. Capitals where the English has them (headers, mission titles, objectives) - some fonts have no small letters.
3. Don't change section names (`[strings]`, `[Editor]`), keys (the part before `=`), their order, or `%s %d %1 %2`, `{...}`, `\t`, `&` (one per menu entry).
4. `{F7}` and `{F2}` in the help pages switch the font - keep them around the same words.
5. No typographic quotes, long dashes or the `…` character: use `"` `'` `-` `...`.

---

**Part 2/2**

**Sending it back:** zip the whole folder (or just the files you changed, in their folders) and send it here on Discord. I run one command that checks everything and brings it into the game. **Your name and e-mail stay on the commit**, so GitHub shows the work as yours, as with your earlier commits. If a file breaks a rule, I send you back the exact line and the reason.

**What to do first, in this order:**
1. **The 7 files still in English:** `game/briefings/l01/loose.txt`, `game/briefings/l02/brief.txt`, `brief2.txt`, `loose.txt`, `objectives.txt`, `win.txt` and `game/briefings/l11/brief3.txt`.
2. **The diacritics, mission by mission:** the other briefings (l01 ... l20), the menu pages and the editor's help pages 1-23 are in Czech but without háčky and čárky (they were written before the game could draw them). Now the fonts have every Czech letter, so they can be written properly. One mission at a time is fine - send each when it is done. (The credits pages `menu52x`, `menu53`, `menu54` are mostly names; only their headings are text.)
3. **Proofreading our Czech:** the editor's help pages 24-29 (`game/editor/edithelp24.txt` ... `edithelp29.txt`) and the `[Editor]` section of `strings.override.ini` were written by us, not by a native speaker. Please correct whatever reads wrong.

Thank you!

*(Optional, only if you like git: you can still send pull requests to github.com/Dysprohol/OpenLocoland as before - the files are then in `lang/cs/` in CP1250. The kit is just the easier way.)*
