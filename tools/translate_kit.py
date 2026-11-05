r"""A translator's kit: a language pack's texts as UTF-8 files to translate, and their import.

    python tools\translate_kit.py export <code> <folder> [--ours | --game | --all] [--zip] [--force]
    python tools\translate_kit.py import <code> <folder or zip> [--author "Name <email>"] [--dry-run] [--partial]

(both take `--captured <folder>`, the captured language files, default languages.CAPTURED)

For a volunteer who does not know git: `export` writes the texts of pack `code` (lang/<code>/) into
`<folder>` as UTF-8 files - converted from the pack's code page, at the pack's own relative paths
(game/briefings/l02/brief.txt, game/menus/menu41.txt, game/editor/edithelp6.txt,
game/strings.override.ini, language.ini) and installer.txt for Setup's window texts (TEXTS of
tools/installer.py) -, beside each page its English source as <name>.en.txt (the game's texts from
captured/en, ours from the English at the root: game/maps/edithelp*.txt, game/maps.new/<map>/*.txt),
and in the key files (strings.override.ini, language.ini, installer.txt) the English value of each
key on a line `;EN> ...` above it. The help pages' font switches (ESC and a font's number) are
written `{F7}` (the lit font, VID 7) and `{F2}` (the plain one). A text the pack does not have yet,
or has only as lang/_template's placeholder, is written as the English: the starting point.
README.txt tells the translator the rules, TODO.txt what is left (computed: files still the
English, files without a letter outside ASCII, keys of ours the pack lacks, values still the
English). `--zip` also writes <folder>.zip.

Scope: `--ours` = what we add to the game - the editor's help pages, our keys of
strings.override.ini (the keys lang/en/game/strings.override.ini defines), POLIGON's (sandbox) and
the template's texts, the launcher's labels (language.ini) and Setup's (installer.txt); `--game` =
the game's own texts - the missions' briefings, the menu pages, the other keys of strings.ini;
`--all` both. The default: --all for a fan translation (base= another language in language.ini,
Czech), --ours for a language Steam ships.

`import` reads the translator's files back (the same paths; *.en.txt, README.txt and TODO.txt are
left out; a folder or the zip), converts each to the pack's code page and checks it: a character
the code page has no byte for (named with its line), a character the pack's fonts cannot draw
(check_lang.py's fonts, every font set a fan pack may be built on), a line wider than the English
line at the same place or the kind's limit (README's numbers: 44 in the briefings, menu pages and
help pages, 40 in objectives.txt, 54 in the tutorial's script*.txt), the sections, keys and their
order of a key file, the %s / %d / %1 / {name} / \t / & of each value against the English, and
installer.txt's keys against TEXTS. Accepted files are written into lang/<code>/ with the pack's
line endings (CRLF in the pages, LF in strings.override.ini - those of the file there, else of the
English) and installer.txt into TEXTS[<code>] of tools/installer.py, and committed in one commit
with `--author` (the translator, so GitHub credits them); our own corrections after it are a commit
of their own, by hand. A rejected file stops it all - nothing written, nothing committed - unless
`--partial`; `--dry-run` only reports. After writing, check_lang.py is run for the pack and must
pass (otherwise the files are put back and nothing is committed).
"""
import argparse
import ast
import datetime
import difflib
import io
import json
import os
import re
import shutil
import subprocess
import sys
import textwrap
import unicodedata
import zipfile
from collections import Counter

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "tools"))
import check_lang  # noqa: E402
import languages  # noqa: E402

INSTALLER = os.path.join(REPO, "tools", "installer.py")
EN_CP = 1252   # the English sources: captured/en and the root's texts
MARK = ";EN>"   # the English value of the key below, in the kit's key files
FONT_MARK = re.compile(r"\{F(\d)\}")
KIT_NOTES = {"readme.txt", "todo.txt"}
NAMES = {"cs": "Czech", "de": "German", "en": "English", "hu": "Hungarian", "pl": "Polish",
         "pt": "Portuguese", "ru": "Russian"}
SCOPES = ("ours", "game", "all")
OURS_MAPS = ("sandbox", "template")   # our maps' texts (game/maps.new/<map>/)
MAP_TEXT = re.compile(r"(brief\d*|win|loose|objectives|script\d+)\.txt", re.I)   # a map's texts
WIDTHS = [("objectives.txt", 40, "objectives.txt (the mission's goals)"),
          ("script", 54, "script*.txt (the tutorial's messages, mission 1)"),
          ("", 44, "brief*.txt, win.txt, loose.txt (briefings), menu*.txt (menu pages), edithelp*.txt (help pages)")]
HELP_LINES = 14   # the lines of a help page (docs/TRANSLATING.md)
GROUPS = {"briefings": "mission briefings", "ourmaps": "POLIGON's and the template's texts",
          "menus": "menu pages", "editor": "editor help pages", "strings": "strings.ini",
          "launcher": "launcher labels", "installer": "Setup's window texts"}


# ------------------------------------------------------------------ helpers -----

def codepage(code):
    return check_lang.codepage(code)


def english_name(code):
    return NAMES.get(code) or languages.pack_value(code, "name", code)


def natural(text):
    """A sort key: the numbers in a name by their value, brief.txt before brief2.txt."""
    stem, ext = os.path.splitext(text)
    return [[int(t) if t.isdigit() else t.lower() for t in re.split(r"(\d+)", part) if t] for part in stem.split("/")] + [ext]


def read(path):
    with open(path, "rb") as f:
        return f.read()


def placeholder(data):
    """A file of lang/_template that only says what to translate."""
    return data.lstrip().startswith(b"@ Translate")


def to_kit(data, cp):
    """The text of a pack's file for the kit: decoded, \n line ends, the font switches as {F<n>}."""
    text = data.decode(f"cp{cp}").replace("\r\n", "\n")
    return re.sub("\x1b(.)", lambda m: "{F%d}" % ord(m.group(1)) if ord(m.group(1)) < 10 else m.group(0), text)


def from_kit(text):
    return FONT_MARK.sub(lambda m: "\x1b" + chr(int(m.group(1))), text)


def width(line):
    """What a line of a page takes on the screen, in characters: the font switches take nothing."""
    return len(FONT_MARK.sub("", line))


def width_limit(name):
    name = name.lower()
    return next(limit for start, limit, _ in WIDTHS if name.startswith(start))


def same_text(a, b):
    """Two texts the same but for spaces at the ends of lines and empty lines at the end."""
    def norm(t):
        return "\n".join(line.rstrip() for line in t.replace("\r\n", "\n").split("\n")).rstrip("\n")
    return norm(a) == norm(b)


def norm_line(line):
    return " ".join(FONT_MARK.sub("", line).replace("’", "'").replace("‘", "'").split())


def english_share(text, english):
    """The share of a page's lines with letters that are lines of the English page (spaces, font
    switches and the apostrophe's kind left aside): 1.0 for a page not translated at all."""
    lines = [norm_line(x) for x in text.split("\n") if letters(x) >= 2]
    known = {norm_line(x) for x in english.split("\n")}
    return sum(x in known for x in lines) / len(lines) if lines else 1.0


def letters(text):
    return sum(c.isalpha() for c in text)


def needs_letters(code):
    """Whether the pack's language writes letters outside ASCII (every one but English)."""
    return code != "en"


def char_name(c):
    return f"{c!r} (U+{ord(c):04X} {unicodedata.name(c, 'unnamed')})"


def encode_lines(text, cp, first_line=1):
    """(bytes, [problems]) of a text in code page `cp`: each character without a byte is named
    with its line and column."""
    out, problems = [], []
    for n, line in enumerate(text.split("\n"), first_line):
        try:
            out.append(line.encode(f"cp{cp}"))
        except UnicodeEncodeError:
            for col, c in enumerate(line, 1):
                try:
                    c.encode(f"cp{cp}")
                except UnicodeEncodeError:
                    problems.append(f"line {n}, column {col}: {char_name(c)} has no byte in code page {cp}"
                                    + hint(c))
            out.append(line.encode(f"cp{cp}", "replace"))
    return b"\n".join(out), problems


def hint(c):
    simple = {"–": "-", "—": "-", "…": "...", "“": '"', "”": '"', "„": '"',
              "‘": "'", "’": "'", " ": "a space"}
    return f" - write {simple[c]} instead" if c in simple else ""


def git(*args, check=True):
    r = subprocess.run(["git", "-C", REPO] + list(args), capture_output=True, text=True, encoding="utf-8")
    if check and r.returncode:
        raise SystemExit(f"git {' '.join(args)}: {r.stderr.strip()}")
    return r.stdout.strip()


# ------------------------------------------------------------- key files -----

class Item:
    """A line of a key file: 'section', 'key' (key=value), 'name' (a key without a value: an
    anchor or a -key of an override) or 'comment'. `n` is the line number (1-based)."""
    def __init__(self, kind, line, n=0, section=None, key=None, left=None, value=None):
        self.kind, self.line, self.n, self.section, self.key = kind, line, n, section, key
        self.left, self.value = left, value
        self.english = None   # the English value of a key
        self.new = False      # a key the pack lacks, written with the English value

    @property
    def ident(self):
        return ((self.section or "").lower(), self.key)

    @property
    def bare(self):
        return ((self.section or "").lower(), self.key.lstrip("!-").strip().lower())


def parse_keys(text, kit=False):
    """The Items of a key file's text (lines split at \n); with `kit` the ;EN> lines are left out."""
    items, section = [], None
    for n, line in enumerate(text.split("\n"), 1):
        line = line.rstrip("\r")
        s = line.strip()
        if kit and s.startswith(MARK):
            continue
        if not s or s.startswith(";") or s.startswith("#"):
            items.append(Item("comment", line, n))
        elif s.startswith("[") and s.endswith("]"):
            section = s[1:-1]
            items.append(Item("section", line, n, section))
        elif "=" in line:
            left, value = line.split("=", 1)
            items.append(Item("key", line, n, section, left.strip(), left, value))
        else:
            items.append(Item("name", line, n, section, s))
    return items


def values_of(items):
    """{(section lower, bare key lower): value} of the first key=value of each key."""
    out = {}
    for it in items:
        if it.kind == "key":
            out.setdefault(it.bare, it.value)
    return out


def insert_missing(items, reference, missing):
    """Adds to `items` the keys of `reference` (Items, in their order) whose bare key is in
    `missing`, with the reference's value, after the key before them in the reference that `items`
    has (else at the end of their section, or in a new section at the end)."""
    for i, ref in enumerate(reference):
        if ref.kind != "key" or ref.bare not in missing:
            continue
        item = Item("key", f"{ref.left}={ref.value}", 0, ref.section, ref.key, ref.left, ref.value)
        item.new = True
        at = None
        for prev in reversed(reference[:i]):
            if prev.kind in ("key", "name") and prev.bare[0] == ref.bare[0]:
                at = next((j for j, it in enumerate(items) if it.kind in ("key", "name") and it.bare == prev.bare), None)
                if at is not None:
                    break
        if at is None:
            sections = [j for j, it in enumerate(items) if it.kind == "section" and it.section.lower() == ref.bare[0]]
            if sections:
                at = sections[0]
                while at + 1 < len(items) and items[at + 1].kind != "section":
                    at += 1
                while items[at].kind == "comment" and not items[at].line.strip() and at > sections[0]:
                    at -= 1
            else:
                items.append(Item("comment", "", 0))
                items.append(Item("section", f"[{ref.section}]", 0, ref.section))
                at = len(items) - 1
        items.insert(at + 1, item)
        missing = missing - {ref.bare}


def filter_items(items, keep):
    """The sections and keys of `items` for which keep(item) holds, with the comments just above
    each kept key; the other lines left out."""
    out, pending, section = [], [], None
    for it in items:
        if it.kind == "section":
            section, pending = it, []
        elif it.kind == "comment":
            pending.append(it)
        elif keep(it):
            if section is not None and section not in out:
                if out:
                    out.append(Item("comment", "", 0))
                out.append(section)
            out += [c for c in pending if c.line.strip()]
            out.append(it)
            pending = []
        else:
            pending = []
    return out


def render_keys(items, with_english):
    lines = []
    for it in items:
        if it.kind == "key" and with_english and it.english is not None:
            lines.append(f"{MARK} {it.english}".rstrip())
        lines.append(it.line)
    return "\n".join(lines) + "\n"


def tokens(value):
    """The parts of a value a translation must keep: the % conversions in order, the {name}
    fields and references, the \t and the single & (the menus' Alt letter)."""
    conversions = re.findall(r"%%|%\d|%[-+ #0]*\d*(?:\.\d+)?[hlI]*[sdiuxXcfgeEp]", value)
    fields = Counter(re.findall(r"\{[A-Za-z_][A-Za-z_0-9]*(?::[^{}]*)?\}", value))
    return conversions, fields, value.count("\\t"), len(re.findall(r"(?<!&)&(?!&)", value))


def token_problems(value, reference):
    problems = []
    a, b = tokens(value), tokens(reference)
    if a[0] != b[0]:
        problems.append(f"the conversions {' '.join(a[0]) or 'none'} - the English has {' '.join(b[0]) or 'none'}"
                        " (keep them and their order)")
    if a[1] != b[1]:
        problems.append(f"the fields {' '.join(sorted(a[1])) or 'none'} - the English has "
                        f"{' '.join(sorted(b[1])) or 'none'} (keep them as they are)")
    if a[2] != b[2]:
        problems.append(f"{a[2]} \\t - the English has {b[2]}")
    if a[3] != b[3]:
        problems.append(f"{a[3]} single & - the English has {b[3]} (the letter Alt opens; && writes an &)")
    return problems


# ------------------------------------------------------------------- the kit -----

class Entry:
    """A file of the kit. kind: 'page' (a text page), 'ini' (strings.override.ini), 'launcher'
    (language.ini), 'installer' (installer.txt)."""
    def __init__(self, kit, kind, group, scope, target=None, english=None, current=None):
        self.kit, self.kind, self.group, self.scope = kit, kind, group, scope
        self.target = target     # the file of the repository it is imported into
        self.english = english   # the English text (a page) or None
        self.current = current   # the pack's text (a page) or None when the pack has none
        self.items = None        # a key file's Items as exported
        self.notes = []          # TODO lines

    @property
    def text(self):
        """What the kit holds: the pack's text, or the English as the starting point."""
        return self.current if self.current is not None else self.english


class Kit:
    def __init__(self, code, captured):
        if not os.path.isfile(os.path.join(languages.LANG, code, "language.ini")):
            raise SystemExit(f"lang/{code}/language.ini not found - a new language starts as a copy of lang/_template")
        self.code, self.captured = code, captured
        self.cp = codepage(code)
        self.pack = os.path.join(languages.LANG, code)
        self.en_maps = os.path.join(captured, "en", "maps")
        self.notes = []

    def default_scope(self):
        return "all" if languages.fan_pack(self.code) else "ours"

    def entries(self, scope):
        if scope != "ours" and self.code == "en":
            raise SystemExit("The English game texts are the game's own (captured/en): export en --ours")
        out = []
        if scope in ("game", "all"):
            out += self.pages_game()
        if scope in ("ours", "all"):
            out += self.pages_ours()
        out.append(self.ini(scope))   # one strings.override.ini of the scope's keys
        if scope in ("ours", "all"):
            out += [self.launcher(), self.installer()]
        return out

    # -- pages
    def page(self, kit, group, scope, target, english_path):
        current = english = None
        if target and os.path.isfile(target) and not placeholder(read(target)):
            current = to_kit(read(target), self.cp)
        if english_path and os.path.isfile(english_path) and read(english_path).strip():
            english = to_kit(read(english_path), EN_CP)
        if current is None and english is None:
            return None
        if self.code == "en":
            english = None if current is None else current
        return Entry(kit, "page", group, scope, target, english, current)

    def pack_pages(self, folder):
        path = os.path.join(self.pack, "game", *folder.split("/"))
        pattern = MAP_TEXT if folder.startswith("briefings") else re.compile(r".*\.txt", re.I)
        return {f for f in os.listdir(path) if pattern.fullmatch(f)} if os.path.isdir(path) else set()

    def pages_ours(self):
        out = []
        root = os.path.join(REPO, "game", "maps")
        english = {f for f in os.listdir(root) if re.fullmatch(r"edithelp\d+\.txt", f.lower())}
        for name in sorted(english | self.pack_pages("editor"), key=natural):
            target = os.path.join(root, name) if self.code == "en" else os.path.join(self.pack, "game", "editor", name)
            out.append(self.page(f"game/editor/{name}", "editor", "ours", target, os.path.join(root, name)))
        for m in OURS_MAPS:
            folder = os.path.join(REPO, "game", "maps.new", m)
            english = {f for f in os.listdir(folder) if MAP_TEXT.fullmatch(f)}
            for name in sorted(english | self.pack_pages(f"briefings/{m}"), key=natural):
                target = os.path.join(folder, name) if self.code == "en" else \
                    os.path.join(self.pack, "game", "briefings", m, name)
                out.append(self.page(f"game/briefings/{m}/{name}", "ourmaps", "ours", target, os.path.join(folder, name)))
        return [e for e in out if e]

    def pages_game(self):
        out = []
        if not os.path.isdir(self.en_maps):
            self.notes.append(f"No English capture ({self.en_maps}): the game's texts have no English beside them.")
        maps = {d for d in (os.listdir(self.en_maps) if os.path.isdir(self.en_maps) else [])
                if re.fullmatch(r"l\d\d", d.lower())}
        briefings = os.path.join(self.pack, "game", "briefings")
        maps |= {d for d in (os.listdir(briefings) if os.path.isdir(briefings) else []) if re.fullmatch(r"l\d\d", d.lower())}
        for m in sorted(maps, key=natural):
            folder = os.path.join(self.en_maps, m)
            english = {f for f in (os.listdir(folder) if os.path.isdir(folder) else []) if MAP_TEXT.fullmatch(f)}
            for name in sorted(english | self.pack_pages(f"briefings/{m}"), key=natural):
                out.append(self.page(f"game/briefings/{m}/{name}", "briefings", "game",
                                     os.path.join(briefings, m, name), os.path.join(folder, name)))
        english = {f for f in (os.listdir(self.en_maps) if os.path.isdir(self.en_maps) else [])
                   if re.fullmatch(r"menu\d+\.txt", f.lower())}
        for name in sorted(english | self.pack_pages("menus"), key=natural):
            out.append(self.page(f"game/menus/{name}", "menus", "game", os.path.join(self.pack, "game", "menus", name),
                                 os.path.join(self.en_maps, name)))
        return [e for e in out if e]

    # -- strings.override.ini
    def ini_items(self):
        """The pack's strings.override.ini as Items (decoded), [] when it has none."""
        path = os.path.join(self.pack, "game", "strings.override.ini")
        return parse_keys(read(path).decode(f"cp{self.cp}").replace("\r\n", "\n")) if os.path.isfile(path) else []

    def ini(self, scope):
        en_override = parse_keys(read(os.path.join(languages.LANG, "en", "game", "strings.override.ini"))
                                 .decode(f"cp{EN_CP}").replace("\r\n", "\n"))
        ours = {it.bare for it in en_override if it.kind == "key"}
        base_path = check_lang.find_file(os.path.join(self.captured, "en"), "strings.ini")
        en_base = parse_keys(read(base_path).decode(f"cp{EN_CP}").replace("\r\n", "\n")) if base_path else []
        english = values_of(en_base)
        english.update(values_of(en_override))
        items = self.ini_items()
        have = {it.bare for it in items if it.kind in ("key", "name")}
        # our keys the pack lacks; for a language Steam ships not those its own strings.ini has (the
        # English corrections of the English game's words)
        own_path = check_lang.find_file(os.path.join(self.captured, self.code), "strings.ini")
        own = {k for k, v in values_of(parse_keys(read(own_path).decode(f"cp{self.cp}", "replace"))).items()
               if v.strip()} if own_path and not languages.fan_pack(self.code) else set()
        missing = ours - have - own
        game_keys = [it for it in items if it.kind in ("key", "name") and it.bare not in ours]
        if scope in ("game", "all") and languages.fan_pack(self.code) and not game_keys and en_base:
            # a new fan translation: the English strings.ini is its starting point
            insert_missing(items, en_base, {it.bare for it in en_base if it.kind == "key" and it.value.strip()} - have - ours)
        if scope in ("ours", "all"):
            insert_missing(items, en_override, missing)
        if scope == "ours":
            items = filter_items(items, lambda it: it.kind == "key" and it.bare in ours)
        elif scope == "game":
            items = filter_items(items, lambda it: it.bare not in ours) if self.code != "en" else []
        for it in items:
            if it.kind == "key" and self.code != "en":
                it.english = english.get(it.bare)
        e = Entry("game/strings.override.ini", "ini", "strings", scope,
                  os.path.join(self.pack, "game", "strings.override.ini"))
        e.items = items
        if scope in ("game", "all") and languages.fan_pack(self.code) and en_base and game_keys:
            left = [it for it in en_base if it.kind == "key" and it.bare not in have and it.bare not in ours]
            if left:
                e.notes.append(f"{len(left)} key(s) of the English strings.ini the pack does not set (they show the "
                               f"base's text; not in the kit, e.g. [{left[0].section}] {left[0].key})")
        return e

    # -- language.ini
    def launcher(self):
        path = os.path.join(self.pack, "language.ini")
        items = parse_keys(read(path).decode("utf-8-sig").replace("\r\n", "\n"))
        en_items = parse_keys(read(os.path.join(languages.LANG, "en", "language.ini")).decode("utf-8-sig").replace("\r\n", "\n"))
        launcher = {it.bare for it in en_items if it.kind == "key" and it.bare[0] == "launcher"}
        have = {it.bare for it in items if it.kind == "key"}
        if not any(it.kind == "section" and it.section.lower() == "launcher" for it in items) and launcher - have:
            items.append(Item("comment", "", 0))
        insert_missing(items, en_items, launcher - have)
        english = values_of(en_items)
        for it in items:
            if it.kind == "key" and self.code != "en" and (it.bare[0] == "launcher" or it.bare == ("language", "name")):
                it.english = english.get(it.bare)
        e = Entry("language.ini", "launcher", "launcher", "ours", path)
        e.items = items
        return e

    # -- installer.txt
    def installer(self):
        texts = installer_texts()
        en, own = texts["en"], texts.get(self.code, {})
        items = [Item("comment", "; Setup's window (tools/installer.py TEXTS): key=value, \\n is a new line", 0)]
        for key, value in en.items():
            it = Item("key", f"{key}={escape(own.get(key, value))}", 0, None, key, key, escape(own.get(key, value)))
            it.english = escape(value) if self.code != "en" else None
            it.new = key not in own
            items.append(it)
        e = Entry("installer.txt", "installer", "installer", "ours", INSTALLER)
        e.items = items
        return e


def escape(value):
    return value.replace("\\", "\\\\").replace("\n", "\\n")


def unescape(value):
    return re.sub(r"\\(.)", lambda m: "\n" if m.group(1) == "n" else m.group(1), value)


def installer_source():
    return read(INSTALLER).decode("utf-8")


def texts_node(source):
    tree = ast.parse(source)
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == "TEXTS" for t in node.targets):
            return node.value
    raise SystemExit("TEXTS not found in tools/installer.py")


def installer_texts(source=None):
    return ast.literal_eval(texts_node(source or installer_source()))


# ------------------------------------------------------------------ the TODO -----

def todo(kit, entries, scope):
    out = []
    english_pages = [e for e in entries if e.kind == "page" and e.english is not None and kit.code != "en"
                     and english_share(e.text, e.english) >= 0.5]
    absent = [e for e in english_pages if e.current is None]
    plain = [e for e in entries if e.kind == "page" and needs_letters(kit.code) and e not in english_pages
             and letters(e.text) >= 40 and all(ord(c) < 128 for c in e.text)]
    out.append(f"Files still the English (not translated, or most of their lines): {len(english_pages)}")
    for e in english_pages:
        share = english_share(e.text, e.english)
        out.append(f"  {e.kit}" + ("   (the pack has none yet: the English is the starting point)" if e in absent
                                   else f"   ({share:.0%} of its lines are the English's)" if share < 1 else ""))
    if needs_letters(kit.code):
        out.append("")
        out.append(f"Files without any letter outside ASCII (no diacritics yet): {len(plain)}")
        out += [f"  {e.kit}" for e in plain]
    for e in entries:
        if e.items is None:
            continue
        keys = [it for it in e.items if it.kind == "key"]
        new = [it for it in keys if it.new]
        same = [it for it in keys if not it.new and it.english is not None and it.value == it.english
                and letters(re.sub(r"\{[^{}]*\}|%\w", "", it.value)) >= 3 and kit.code != "en"]
        out.append("")
        out.append(f"{e.kit}: {len(keys)} keys")
        if new:
            out.append(f"  keys the pack does not have yet (written with the English text - translate them): {len(new)}")
            out += [f"    {f'[{it.section}] ' if it.section else ''}{it.key}" for it in new]
        if same:
            out.append(f"  values still the same as the English (translate them, or leave a name that stays): {len(same)}")
            out += [f"    {f'[{it.section}] ' if it.section else ''}{it.key}={it.value}" for it in same]
        out += ["  " + n for n in e.notes]
    out += [""] + kit.notes if kit.notes else []
    return out


README = """\
OpenLocoland - translation kit for {name} ({code})
{line}

{intro}

Exported {date} from commit {commit}, scope: {scope}.

HOW THE FOLDER IS MADE

- Every file is UTF-8. Open it in any editor that saves UTF-8 (Notepad++, VS Code, even Windows
  Notepad) and write {name} normally, with all its letters. We convert it to the game's code page
  ({cp}) when we take the files in, so do not convert anything yourself.
- Beside each page there is its English original, <name>.en.txt (brief.en.txt beside brief.txt).
  It is only for reference: change the file without .en.
- In strings.override.ini, language.ini and installer.txt each line ";EN> ..." shows the English
  of the key below it. Translate only the text after "=" of the key line; the ";EN>" lines and the
  other lines starting with ";" are notes and are ignored.
- TODO.txt lists what is left: files still in English, files with no {name} letters yet, keys
  still the same as the English.

THE RULES (the import checks them; a file that breaks one comes back to you with the line)

1. Keep every file name and folder. Do not add or remove files.
2. A line may not be wider than the limit of its kind (or than the English line at the same
   place, when that one is wider):
{widths}
   The game does not wrap text: break long sentences into more lines yourself. A page should not
   get many more lines than the English one, or it may not fit its frame; a help page
   (edithelp*.txt) has at most 14 lines, its first line is the page's title.
3. Capitals where the original has them: the headers, the mission titles, the objectives and
   other lines that are in capitals in English are in capitals in the translation (some fonts of
   the game have no small letters).
4. Do not change section names ([strings], [Editor] ...), the keys (the part before "="), their
   order, or these marks inside a text: %s %d %1 %2 (filled in by the game, in the same order),
   {{name}} fields, \\t (a tab: keep it), & (marks the letter of a menu entry that Alt opens:
   one per entry; && writes an & itself). \\n starts a new line inside an editor's text: keep as
   many as the English has when you can.
5. The help pages have {{F7}} and {{F2}} in their lines: they switch the font (the lit one and the
   plain one). Keep them around the same words.
6. A character the game's fonts cannot draw is refused (typographic quotes, long dashes, the
   ellipsis character ...): write " ' - ... instead.

SENDING IT BACK

Send the whole folder or the zip back (Discord is fine). Your name stays on the commit that brings
your files into the game, so GitHub credits you.
"""


def write_kit(kit, entries, folder, scope):
    commit = git("rev-parse", "--short", "HEAD", check=False) or "?"
    with_english = kit.code != "en"
    for e in entries:
        path = os.path.join(folder, *e.kit.split("/"))
        os.makedirs(os.path.dirname(path), exist_ok=True)
        text = e.text if e.kind == "page" else render_keys(e.items, with_english)
        with open(path, "w", encoding="utf-8", newline="\r\n") as f:
            f.write(text)
        if e.kind == "page" and e.english is not None and with_english:
            stem = os.path.splitext(path)[0]
            with open(stem + ".en.txt", "w", encoding="utf-8", newline="\r\n") as f:
                f.write(e.english)
    name = english_name(kit.code)
    what = {"ours": "the texts we add to the game (the map editor's help pages and labels, the "
                    "launcher, Setup, the POLIGON map)",
            "game": "the game's own texts (the missions' briefings, the menu pages, strings.ini)",
            "all": "the game's own texts (the missions' briefings, the menu pages, strings.ini) and the "
                   "texts we add to it (the map editor's help pages and labels, the launcher, Setup, "
                   "the POLIGON map)"}[scope]
    widths = "\n".join(textwrap.fill(label, 96, initial_indent=f"     {limit} characters  ", subsequent_indent=" " * 20)
                       for _, limit, label in WIDTHS)
    intro = textwrap.fill(f"This folder holds the texts of the {name} language pack of OpenLocoland, the revival "
                          f"of the game Locoland: {what}. Translate them in place and send the whole folder (or "
                          f"a zip of it) back.", 96)
    title = f"OpenLocoland - translation kit for {name} ({kit.code})"
    readme = README.format(name=name, code=kit.code, line="=" * len(title), intro=intro, cp=f"CP{kit.cp}",
                           date=datetime.date.today().isoformat(), commit=commit, scope=scope, widths=widths)
    with open(os.path.join(folder, "README.txt"), "w", encoding="utf-8", newline="\r\n") as f:
        f.write(readme)
    lines = [f"TODO - {name} ({kit.code}), scope {scope}, exported {datetime.date.today().isoformat()} "
             f"from commit {commit}", ""] + todo(kit, entries, scope)
    with open(os.path.join(folder, "TODO.txt"), "w", encoding="utf-8", newline="\r\n") as f:
        f.write("\n".join(lines) + "\n")
    return lines


def export(args):
    kit = Kit(args.code, args.captured)
    scope = args.scope or kit.default_scope()
    folder = os.path.abspath(args.folder)
    if os.path.isdir(folder) and os.listdir(folder):
        if not args.force:
            raise SystemExit(f"{folder} is not empty (--force replaces a kit exported there before)")
        if not os.path.isfile(os.path.join(folder, "README.txt")) or \
                "translation kit" not in read(os.path.join(folder, "README.txt")).decode("utf-8", "replace"):
            raise SystemExit(f"{folder} holds something else than a kit - not replaced")
        shutil.rmtree(folder)
    os.makedirs(folder, exist_ok=True)
    entries = kit.entries(scope)
    lines = write_kit(kit, entries, folder, scope)
    pages = sum(e.kind == "page" for e in entries)
    print(f"{args.code}: {pages} pages and {len(entries) - pages} key files ({scope}) in {folder}")
    if args.zip:
        archive = folder.rstrip("\\/") + ".zip"
        top = os.path.basename(folder.rstrip("\\/"))
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
            for directory, _, files in os.walk(folder):
                for f in sorted(files):
                    p = os.path.join(directory, f)
                    z.write(p, os.path.join(top, os.path.relpath(p, folder)).replace("\\", "/"))
        print(f"zip: {archive}")
    print("\n".join(lines[:12]) + ("\n..." if len(lines) > 12 else ""))


# ------------------------------------------------------------------ import -----

def read_folder(path):
    """{kit path: bytes} of the translator's files (a folder or a zip; one top folder of a zip is
    left out), without the English (*.en.txt) and our notes."""
    files = {}
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as z:
            names = [n for n in z.namelist() if not n.endswith("/")]
            tops = {n.split("/", 1)[0] for n in names}
            strip = len(tops) == 1 and all("/" in n for n in names)
            for n in names:
                rel = n.split("/", 1)[1] if strip else n
                files[rel] = z.read(n)
    elif os.path.isdir(path):
        for directory, _, names in os.walk(path):
            for n in names:
                p = os.path.join(directory, n)
                files[os.path.relpath(p, path).replace("\\", "/")] = read(p)
    else:
        raise SystemExit(f"{path}: neither a folder nor a zip")
    out = {}
    for rel, data in files.items():
        parts = rel.split("/")
        if ".." in parts or rel.startswith("/") or ":" in rel:
            raise SystemExit(f"{rel}: not a path inside the kit")
        if rel.lower().endswith(".en.txt") or (len(parts) == 1 and rel.lower() in KIT_NOTES):
            continue
        out[rel] = data
    return out


def export_commit(path):
    """The commit a kit was exported from (README.txt), or None."""
    try:
        data = read_folder_note(path, "README.txt")
        m = re.search(r"from commit ([0-9a-f]{4,40})", data)
        return m.group(1) if m else None
    except (OSError, KeyError):
        return None


def read_folder_note(path, name):
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as z:
            n = next(n for n in z.namelist() if n.split("/")[-1].lower() == name.lower())
            return z.read(n).decode("utf-8", "replace")
    return read(os.path.join(path, name)).decode("utf-8", "replace")


def decode_utf8(data):
    """(text with \n line ends, problem or None)."""
    if data.startswith(b"\xef\xbb\xbf"):
        data = data[3:]
    try:
        return data.decode("utf-8").replace("\r\n", "\n").replace("\r", "\n"), None
    except UnicodeDecodeError as e:
        line = data[:e.start].count(b"\n") + 1
        return None, f"line {line}: not UTF-8 (byte 0x{data[e.start]:02X}) - save the file as UTF-8"


class Checker:
    """The fonts a pack's texts are checked with (check_lang): every font set of check()."""
    def __init__(self, code, captured):
        self.code, self.cp = code, codepage(code)
        self.sets = []
        for font_code in check_lang.font_codes(code, captured):
            fonts, cyrillic, _, _ = check_lang.load_fonts(code, captured, font_code)
            if len(fonts) == len(check_lang.FONT_FILES):
                self.sets.append((font_code or languages.pack_base(code), fonts, cyrillic))

    def problems(self, items):
        """{line: [reason]} of check_lang items whose `where` is <label>:<line>[ ...]; a character
        missing in several font sets is one reason naming them."""
        plain, missing_at = {}, {}   # line -> {text}; line -> {(font, what): {char: [base]}}
        for base, fonts, cyrillic in self.sets:
            problems, missing = check_lang.glyph_problems(items, self.cp, fonts, cyrillic)
            for text in problems:
                plain.setdefault(int(re.match(r"[^:]*:(\d+)", text).group(1)), set()).add(text.split(": ", 1)[1])
            for (vid, byte, char, what), places in missing.items():
                for n in {int(re.match(r"[^:]*:(\d+)", where).group(1)) for where in places}:
                    bases = missing_at.setdefault(n, {}).setdefault((vid, what), {}).setdefault(char, [])
                    if base not in bases:
                        bases.append(base)
        out = {n: sorted(texts) for n, texts in plain.items()}
        for n, reasons in missing_at.items():
            for (vid, what), chars in sorted(reasons.items()):
                by_sets = {}
                for char, bases in chars.items():
                    by_sets.setdefault(tuple(bases), []).append(char)
                for bases, group in by_sets.items():
                    sets = "every font set" if len(bases) == len(self.sets) > 1 else "the fonts of " + ", ".join(bases)
                    names = ", ".join(char_name(c) for c in group) if len(group) <= 2 else " ".join(repr(c) for c in group)
                    verb = what if len(group) == 1 else what.replace("has", "have").replace("is drawn", "are drawn")
                    note = " - this font has capitals only" if vid in (3, 14) and all(c.islower() for c in group) else \
                        hint(group[0]) if len(group) == 1 else ""
                    out.setdefault(n, []).append(f"{names} {verb} in {check_lang.FONT_FILES[vid]} ({sets}){note}")
        return out


class Result:
    def __init__(self, entry):
        self.entry, self.problems, self.warnings = entry, [], []
        self.data = None        # the new file's bytes (or the new TEXTS values for installer.txt)
        self.changed = 0        # lines or keys changed
        self.skip = None        # why the file is not written although nothing is wrong


def check_page(entry, text, checker):
    r = Result(entry)
    if same_text(text, entry.text):
        r.skip = "unchanged"
        return r
    if entry.current is None and entry.english is not None and same_text(text, entry.english):
        r.skip = "still the English - not added to the pack"
        return r
    name = entry.kit.rsplit("/", 1)[-1]
    limit = width_limit(name)
    english = entry.english.split("\n") if entry.english is not None else []
    lines = text.split("\n")
    while lines and lines[-1] == "":
        lines.pop()
    for n, line in enumerate(lines, 1):
        en_width = width(english[n - 1]) if n <= len(english) else 0
        allowed = max(limit, en_width)
        if width(line) > allowed:
            r.problems.append(f"line {n}: {width(line)} characters, wider than {allowed}"
                              + (f" (the English line {n}: {en_width})" if n <= len(english) else "")
                              + f": {line.strip()[:60]}")
        bad = [c for c in FONT_MARK.sub("", line) if ord(c) < 0x20 and c != "\t"]
        if bad:
            r.problems.append(f"line {n}: a control character {bad[0]!r}")
        m = re.search(r"\{F[^0-9}]*\}|\{F\d\d+\}", line)
        if m:
            r.problems.append(f"line {n}: {m.group(0)} is not a font switch ({{F7}} or {{F2}})")
        if n <= len(english):
            en = FONT_MARK.sub("", english[n - 1])
            if letters(en) >= 3 and en.upper() == en and FONT_MARK.sub("", line).upper() != FONT_MARK.sub("", line):
                r.warnings.append(f"line {n}: the English line is in capitals, this one is not: {line.strip()[:60]}")
    english_lines = len(english) - (english[-1:] == [""])
    if name.lower().startswith("edithelp") and len(lines) > HELP_LINES:
        r.problems.append(f"{len(lines)} lines - a help page holds at most {HELP_LINES}")
    elif english and len(lines) > english_lines + 2:
        r.warnings.append(f"{len(lines)} lines, the English has {english_lines} - check that the page still fits")
    data, problems = encode_lines(from_kit("\n".join(lines)), checker.cp)
    r.problems += problems   # (the characters without a byte are a ? in `data`: the fonts check the rest)
    for n, reasons in sorted(checker.problems(check_lang.map_texts(name, data, "kit")).items()):
        r.problems += [f"line {n}: {reason}" for reason in reasons]
    # the line ends of the pack's file (a page it does not have yet: CRLF as the game's), and its last one
    old = read(entry.target) if entry.current is not None else None
    crlf = b"\r\n" in old if old else True
    final = old.endswith(b"\n") if old else True
    data = data.replace(b"\n", b"\r\n") if crlf else data
    r.data = data + ((b"\r\n" if crlf else b"\n") if final else b"")
    before = (entry.text or "").rstrip("\n").split("\n")
    r.changed = sum(max(a2 - a1, b2 - b1) for tag, a1, a2, b1, b2 in
                    difflib.SequenceMatcher(None, before, lines, autojunk=False).get_opcodes() if tag != "equal") or 1
    return r


def check_keys(entry, text, kit, checker):
    """A key file of the translator against the kit's: the same sections and keys in the same order."""
    r = Result(entry)
    theirs = parse_keys(text, kit=True)
    ours = entry.items
    seq_t = [(it.ident, it) for it in theirs if it.kind in ("key", "name")]
    seq_k = [(it.ident, it) for it in ours if it.kind in ("key", "name")]
    sec_t = [it.section for it in theirs if it.kind == "section"]
    sec_k = [it.section for it in ours if it.kind == "section"]
    if [s for s in sec_t] != [s for s in sec_k]:
        extra = [s for s in sec_t if s not in sec_k]
        gone = [s for s in sec_k if s not in sec_t]
        r.problems.append("the sections changed" + (f": [{extra[0]}] is not one of the kit's" if extra else "")
                          + (f": [{gone[0]}] is missing" if gone else "") + " - keep the sections as they are")
    if [k for k, _ in seq_t] != [k for k, _ in seq_k]:
        sm = difflib.SequenceMatcher(None, [k for k, _ in seq_k], [k for k, _ in seq_t], autojunk=False)
        for tag, a1, a2, b1, b2 in sm.get_opcodes():
            if tag == "equal":
                continue
            if b1 < len(seq_t):
                where = f"line {seq_t[b1][1].n}"
            else:
                where = "at the end"
            want = ", ".join(f"[{k[0]}] {k[1]}" for k, _ in seq_k[a1:a2][:3])
            got = ", ".join(f"[{k[0]}] {k[1]}" for k, _ in seq_t[b1:b2][:3])
            r.problems.append(f"{where}: " + (f"expected {want}" if want else "") + (" - " if want and got else "")
                              + (f"found {got}" if got else " missing") + " (keys and their order must stay)")
            if len(r.problems) >= 5:
                break
        return r
    if entry.kind == "launcher":
        return check_launcher(r, seq_k, seq_t)
    if entry.kind == "installer":
        return check_installer(r, seq_k, seq_t)
    changes = []
    for (_, k), (_, t) in zip(seq_k, seq_t):
        if k.kind != "key" or t.value == k.value:
            continue
        if k.new and k.english is not None and t.value == k.english:
            continue   # a new key left English: the game's English stays
        reference = k.english if k.english is not None else k.value
        for p in token_problems(t.value, reference):
            r.problems.append(f"line {t.n}: [{t.section}] {t.key}: {p}")
        data, problems = encode_lines(t.value, kit.cp, t.n)
        r.problems += [f"{p} ([{t.section}] {t.key})" for p in problems]
        # the fonts that draw the value (check_lang: the headers' capitals-only font too)
        items = check_lang.ini_texts("x", f"[{t.section}]\n{k.left}=".encode(f"cp{kit.cp}") + data, "kit")
        for n, reasons in checker.problems([(f"kit:{t.n}" + w[w.index(" "):], d, f) for w, d, f in items[-1:]]).items():
            r.problems += [f"line {n}: [{t.section}] {t.key}: {reason}" for reason in reasons]
        newlines = (t.value.count("\\n"), reference.count("\\n"))
        if newlines[0] != newlines[1]:
            r.warnings.append(f"line {t.n}: [{t.section}] {t.key}: {newlines[0]} \\n, the English has {newlines[1]}"
                              " - check that it still fits")
        changes.append((k, t.value))
    r.changed = len(changes)
    if not changes:
        r.skip = "unchanged"
        return r
    if not r.problems:
        r.data = merge_keys(entry.target, kit.cp, ours, changes)
    return r


def merge_keys(path, cp, kit_items, changes):
    """The bytes of key file `path` (code page `cp`, or "utf-8") with the changed values: a key the
    file has keeps its line's left part and end; a new one goes after the key before it in the kit."""
    enc = "utf-8" if cp == "utf-8" else f"cp{cp}"
    data = read(path) if os.path.isfile(path) else b""
    bom = data.startswith(b"\xef\xbb\xbf")
    lines = re.split(r"(?<=\n)", data[3:].decode(enc) if bom else data.decode(enc)) if data else []
    if lines and lines[-1] == "":
        lines.pop()
    newline = "\r\n" if "\r\n" in "".join(lines) and "".join(lines).count("\r\n") * 2 > "".join(lines).count("\n") else "\n"
    items = parse_keys("".join(lines).replace("\r\n", "\n"))   # n = index + 1 into lines
    changed = {id(k): v for k, v in changes}
    occurrence = {}
    for it in items:
        if it.kind in ("key", "name"):
            occurrence.setdefault(it.ident, []).append(it)
    seen = Counter()
    pending_new = []
    for k in kit_items:
        if k.kind not in ("key", "name"):
            continue
        if k.new:
            if id(k) in changed:
                pending_new.append(k)
            continue
        i = seen[k.ident]
        seen[k.ident] += 1
        if id(k) not in changed:
            continue
        line_item = occurrence[k.ident][i]
        old = lines[line_item.n - 1]
        ending = old[len(old.rstrip("\r\n")):]
        left = old.split("=", 1)[0]
        lines[line_item.n - 1] = f"{left}={changed[id(k)]}{ending}"
    # the new keys: after the key before them in the kit that the file has, else at its section's end
    for k in pending_new:
        items = parse_keys("".join(lines).replace("\r\n", "\n"))
        before = kit_items[:kit_items.index(k)]
        at = None
        for prev in reversed(before):
            if prev.kind in ("key", "name") and not (prev.new and prev not in pending_new[:pending_new.index(k)]):
                hits = [it for it in items if it.kind in ("key", "name") and it.ident == prev.ident]
                if hits:
                    at = hits[0].n
                    break
            if prev.kind == "section":
                hits = [it for it in items if it.kind == "section" and it.section.lower() == prev.section.lower()]
                if hits:
                    at = hits[0].n
                    break
        line = f"{k.left}={changed[id(k)]}{newline}"
        if at is None:
            if lines and not lines[-1].endswith("\n"):
                lines[-1] += newline
            lines += [newline, f"[{k.section}]{newline}", line]
        else:
            if not lines[at - 1].endswith("\n"):
                lines[at - 1] += newline
            lines.insert(at, line)
    out = "".join(lines).encode(enc)
    return (b"\xef\xbb\xbf" if bom else b"") + out


def check_launcher(r, seq_k, seq_t):
    changes = []
    for (_, k), (_, t) in zip(seq_k, seq_t):
        if k.kind != "key" or t.value == k.value:
            continue
        if k.bare[0] != "launcher" and k.bare != ("language", "name"):
            r.problems.append(f"line {t.n}: [{t.section}] {t.key} is ours to set - leave it as it was ({k.value.strip()})")
            continue
        if k.new and k.english is not None and t.value == k.english:
            continue
        for p in token_problems(t.value, k.english if k.english is not None else k.value):
            r.problems.append(f"line {t.n}: [{t.section}] {t.key}: {p}")
        changes.append((k, t.value))
    r.changed = len(changes)
    if not changes:
        r.skip = "unchanged"
    elif not r.problems:
        r.data = merge_keys(r.entry.target, "utf-8", r.entry.items, changes)
    return r


def check_installer(r, seq_k, seq_t):
    values = {}
    changed = 0
    for (_, k), (_, t) in zip(seq_k, seq_t):
        if k.kind != "key":
            continue
        value = unescape(t.value)
        english = unescape(k.english) if k.english is not None else unescape(k.value)
        a = set(re.findall(r"\{[a-z_]+\}", value))
        b = set(re.findall(r"\{[a-z_]+\}", english))
        if a != b:
            r.problems.append(f"line {t.n}: {t.key}: the fields {' '.join(sorted(a)) or 'none'} - the English has "
                              f"{' '.join(sorted(b)) or 'none'} (keep them as they are)")
        values[k.key] = value
        changed += t.value != k.value and not (k.new and t.value == k.english)
    r.changed = changed
    if not changed:
        r.skip = "unchanged"
    elif not r.problems:
        r.data = values
    return r


def literal(value, indent):
    """A Python string literal of `value` in the style of TEXTS: double quotes, longer ones split
    at spaces into implicitly joined pieces, each continuation under the first quote."""
    pieces, current = [], ""
    for word in re.split(r"(?<= )", value):
        if current and len(json.dumps(current + word, ensure_ascii=False)) + indent > 108:
            pieces.append(current)
            current = ""
        current += word
    pieces.append(current)
    return ("\n" + " " * indent).join(json.dumps(p, ensure_ascii=False) for p in pieces)


def rewrite_installer(source, code, values):
    """installer.py's source with TEXTS[code] holding `values` (keys in TEXTS["en"]'s order): the
    entries whose value changes are written again, missing ones added at the end of the language's
    block, a language TEXTS lacks gets a block of its own at the end."""
    node = texts_node(source)
    lines = source.split("\n")
    block = next((v for k, v in zip(node.keys, node.values) if isinstance(k, ast.Constant) and k.value == code), None)
    old = installer_texts(source).get(code, {})
    edits = []   # (first line, first col, last line, last col, text) - 1-based lines, 0-based cols
    if block is not None:
        for k, v in zip(block.keys, block.values):
            if k.value in values and values[k.value] != old[k.value]:
                text = f'"{k.value}": ' + literal(values[k.value], k.col_offset + len(k.value) + 4)
                edits.append((k.lineno, k.col_offset, v.end_lineno, v.end_col_offset, text))
        add = [key for key in values if key not in old]
        if add:
            indent = block.keys[0].col_offset if block.keys else 8
            text = "".join(f'{" " * indent}"{key}": {literal(values[key], indent + len(key) + 4)},\n' for key in add)
            edits.append((block.end_lineno, 0, block.end_lineno, 0, text))
    else:
        indent = node.keys[0].col_offset
        body = "".join(f'{" " * (indent + 4)}"{key}": {literal(value, indent + 4 + len(key) + 4)},\n'
                       for key, value in values.items())
        text = f'{" " * indent}"{code}": {{\n{body}{" " * indent}}},\n'
        edits.append((node.end_lineno, 0, node.end_lineno, 0, text))
    for l1, c1, l2, c2, text in sorted(edits, reverse=True):
        head, tail = lines[l1 - 1][:c1], lines[l2 - 1][c2:]
        lines[l1 - 1:l2] = (head + text + tail).split("\n")
    out = "\n".join(lines)
    check = installer_texts(out)
    want = dict(old, **values)
    if check.get(code) != want or any(check[c] != v for c, v in installer_texts(source).items() if c != code):
        raise SystemExit("rewriting TEXTS of tools/installer.py went wrong - nothing written")
    compile(out, INSTALLER, "exec")
    return out


def import_kit(args):
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    kit = Kit(args.code, args.captured)
    files = read_folder(args.folder)
    checker = Checker(args.code, args.captured)
    # the kit as it would be exported now, for each scope the strings.override.ini may come from
    by_scope = {}
    for scope in ([kit.default_scope(), "ours", "game", "all"] if args.code != "en" else ["ours"]):
        if scope not in by_scope:
            by_scope[scope] = {e.kit: e for e in Kit(args.code, args.captured).entries(scope)}
    known = {}
    for entries in by_scope.values():
        for path, e in entries.items():
            known.setdefault(path, e)
    results, unknown = [], []
    for rel in sorted(files, key=natural):
        e = known.get(rel)
        if e is None:
            unknown.append(rel)
            continue
        text, problem = decode_utf8(files[rel])
        if problem:
            r = Result(e)
            r.problems.append(problem)
            results.append(r)
            continue
        if e.kind == "page":
            results.append(check_page(e, text, checker))
            continue
        # a key file: the scope whose keys it has
        candidates = [by_scope[s][rel] for s in by_scope if rel in by_scope[s]]
        theirs = [it.ident for it in parse_keys(text, kit=True) if it.kind in ("key", "name")]

        def overlap(c):
            return difflib.SequenceMatcher(None, [it.ident for it in c.items if it.kind in ("key", "name")],
                                           theirs, autojunk=False).ratio()
        e = max(candidates, key=overlap)
        results.append(check_keys(e, text, kit, checker))
    accepted = [r for r in results if not r.problems and r.data is not None]
    rejected = [r for r in results if r.problems]
    skipped = [r for r in results if not r.problems and r.data is None]
    name = english_name(kit.code)
    print(f"Import of {name} ({kit.code}, code page {kit.cp}) from {args.folder}")
    if not checker.sets:
        print("  (no capture of the pack's fonts: the characters are checked against the code page only)")
    since = export_commit(args.folder)
    if since:
        changed = git("diff", "--name-only", since, "HEAD", "--", f"lang/{kit.code}", "game/maps", "game/maps.new",
                      "tools/installer.py", check=False)
        if changed:
            print(f"  note: the kit was exported from {since}; since then these changed (their newer text may be "
                  f"overwritten - check the diff):\n    " + changed.replace("\n", "\n    "))
    print(f"\naccepted ({len(accepted)}):")
    for r in accepted:
        print(f"  {r.entry.kit}   ({changes_of(r)} changed)")
        for w in r.warnings:
            print(f"      warning: {w}")
    print(f"\nrejected ({len(rejected)}):")
    for r in rejected:
        print(f"  {r.entry.kit}")
        for p in sorted(r.problems, key=lambda p: int(m.group(1)) if (m := re.match(r"line (\d+)", p)) else 0):
            print(f"      {p}")
        for w in r.warnings:
            print(f"      warning: {w}")
    if skipped:
        reasons = Counter(r.skip for r in skipped)
        print(f"\nnot written ({len(skipped)}): " + ", ".join(f"{n} {why}" for why, n in reasons.items()))
    if unknown:
        print(f"\nnot files of the kit, left out ({len(unknown)}): " + ", ".join(unknown))
    missing = sorted(set(by_scope[kit.default_scope()]) - set(files), key=natural)
    if missing and len(missing) < len(by_scope[kit.default_scope()]):
        print(f"\n{len(missing)} file(s) of the kit not in the folder (only what is there is imported): "
              + ", ".join(missing[:5]) + (" ..." if len(missing) > 5 else ""))
    if args.dry_run:
        print("\n--dry-run: nothing written")
        return 1 if rejected else 0
    if rejected and not args.partial:
        print("\nNothing written: a file is rejected (--partial writes and commits the accepted ones)")
        return 1
    if not accepted:
        print("\nNothing to write")
        return 1 if rejected else 0
    return write_and_commit(kit, accepted, args)


def write_and_commit(kit, accepted, args):
    targets = sorted({r.entry.target for r in accepted})
    rels = [os.path.relpath(t, REPO).replace("\\", "/") for t in targets]
    dirty = git("status", "--porcelain", "--", *rels, check=False)
    if dirty:
        print(f"\nNothing written: these files have changes of their own - commit or set them aside first:\n{dirty}")
        return 1
    backup = {t: read(t) if os.path.isfile(t) else None for t in targets}
    for r in accepted:
        if r.entry.kind == "installer":
            data = rewrite_installer(installer_source(), kit.code, r.data).encode("utf-8")
        else:
            data = r.data
        os.makedirs(os.path.dirname(r.entry.target), exist_ok=True)
        with open(r.entry.target, "wb") as f:
            f.write(data)
    print(f"\nwritten: {len(accepted)} file(s)")
    out = io.StringIO()
    stdout, sys.stdout = sys.stdout, out
    try:
        ok = check_lang.check(kit.code, args.captured)
    finally:
        sys.stdout = stdout
    print("check_lang.py " + kit.code + ": " + ("passed" if ok else "FAILED") + "\n  "
          + out.getvalue().strip().replace("\n", "\n  "))
    if not ok:
        for t, data in backup.items():
            if data is None:
                os.remove(t)
            else:
                with open(t, "wb") as f:
                    f.write(data)
        print("The files are put back; nothing committed")
        return 1
    message = commit_message(kit, accepted, args)
    git("add", "--", *rels)
    cmd = ["commit", "-q", "-m", message]
    if args.author:
        cmd += ["--author", args.author]
    git(*cmd, "--", *rels)
    print(f"committed {git('log', '-1', '--format=%h %an <%ae>: %s')}")
    return 0


def changes_of(r):
    unit = "key" if r.entry.items is not None else "line"
    return f"{r.changed} {unit}{'s' if r.changed != 1 else ''}"


def commit_message(kit, accepted, args):
    name = english_name(kit.code)
    groups = {}
    for r in accepted:
        groups.setdefault(r.entry.group, []).append(r)
    parts = []
    for group in GROUPS:
        if group not in groups:
            continue
        rs = groups[group]
        if group == "briefings":
            missions = sorted({r.entry.kit.split("/")[2] for r in rs}, key=natural)
            parts.append(f"{GROUPS[group]} {', '.join(missions)}" if len(missions) <= 4 else
                         f"{GROUPS[group]} of {len(missions)} missions")
        elif group in ("editor", "menus"):
            numbers = sorted((re.sub(r"\D", "", r.entry.kit.rsplit("/", 1)[-1]) for r in rs), key=int)
            parts.append(f"{GROUPS[group]} {', '.join(numbers)}" if len(numbers) <= 6 else f"{len(numbers)} {GROUPS[group]}")
        else:
            parts.append(GROUPS[group])
    subject = f"{name} translation: " + ", ".join(parts)
    translator = re.sub(r"\s*<.*", "", args.author) if args.author else "the translator"
    cp = f"CP{kit.cp}"
    body = (f"{translator}'s files from the translation kit (tools/translate_kit.py), UTF-8, converted to "
            f"{cp}, the code page of the pack, with the pack's line endings; keys, sections, their order, "
            f"%s/%d, {{fields}}, \\t and & unchanged, no line wider than the English original's, every "
            f"character drawn by the pack's fonts (check_lang.py passes).")
    lines = []
    for group in GROUPS:
        for r in groups.get(group, []):
            lines.append(f"- {os.path.relpath(r.entry.target, REPO).replace(chr(92), '/')} ({changes_of(r)})")
    return subject + "\n\n" + textwrap.fill(body, 72) + "\n\n" + "\n".join(lines)


def main():
    sys.stdout.reconfigure(encoding="utf-8", errors="backslashreplace")
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = p.add_subparsers(dest="command", required=True)
    e = sub.add_parser("export", help="write a pack's texts as a kit")
    e.add_argument("code")
    e.add_argument("folder")
    g = e.add_mutually_exclusive_group()
    g.add_argument("--ours", dest="scope", action="store_const", const="ours", help="the texts we add")
    g.add_argument("--game", dest="scope", action="store_const", const="game", help="the game's own texts")
    g.add_argument("--all", dest="scope", action="store_const", const="all", help="both")
    e.add_argument("--zip", action="store_true", help="also write <folder>.zip")
    e.add_argument("--force", action="store_true", help="replace a kit exported into the folder before")
    i = sub.add_parser("import", help="check a translator's kit and commit it")
    i.add_argument("code")
    i.add_argument("folder", help="the translator's folder or zip")
    i.add_argument("--author", help='the translator, "Name <email>" (the commit\'s author)')
    i.add_argument("--dry-run", action="store_true", help="only report")
    i.add_argument("--partial", action="store_true", help="write and commit the accepted files even when some are rejected")
    for s in (e, i):
        s.add_argument("--captured", default=languages.CAPTURED, help="the captured language files")
    args = p.parse_args()
    if args.command == "export":
        export(args)
        return 0
    if args.author and not re.fullmatch(r"[^<>]+ <[^<>\s]+@[^<>\s]+>", args.author):
        raise SystemExit('--author "Name <email>"')
    return import_kit(args)


if __name__ == "__main__":
    sys.exit(main())
