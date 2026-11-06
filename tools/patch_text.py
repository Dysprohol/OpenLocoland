r"""Changes to the game's text files, kept as patches instead of whole files.

The repository holds only our changes; the original file comes from the player's own copy
of the game when it is assembled (build.py). Two kinds of change files:

<file>.patch          a unified diff against the original <file> (scripts, menus). Made
                      and applied byte for byte: the game files are CP1250/CP1251 with CRLF
                      line endings, which the patch keeps. Each change (hunk) is placed by
                      its lines, byte for byte: at its line number, or where they occur once
                      in the file (an original with other lines added or changed elsewhere,
                      as another language's Steam depot has); otherwise applying fails and
                      says which change could not be placed.
<name>.override.ini   values for <name>.ini (strings.ini): only the keys we change, so the
                      file reads like a translation and holds nothing of the original.

Override syntax (bytes are copied as they are - write it in the encoding of the original):

    ; comment
    [section]       a section the original does not have is added at its end
    key=value       sets the value of the first `key` line of the section, in place; a key
                    the original does not have is inserted after the key named before it
                    in this file (or at the end of the section)
    key             only names an existing key, as a place to insert the next keys after
    -key            removes the later duplicates of `key` (the engine reads the first one)
    !key=value      a correction of the original's own value, applied only when the override
                    fills the original (fill_only of merge_ini: a pack assembled on a capture
                    of its own language, languages.install), where key=value sets only the
                    keys the original lacks or leaves empty; otherwise the line is left out

    python tools\patch_text.py diff <original> <changed> <file.patch>
    python tools\patch_text.py apply <original> <file.patch> <output>
    python tools\patch_text.py merge <original.ini> <override.ini> <output>
    python tools\patch_text.py test        (the placement's own tests)
"""
import difflib
import os
import re
import sys


def _lines(data):
    """Lines with their endings; only \n ends a line (a lone \r stays inside it)."""
    return [line for line in re.split(rb"(?<=\n)", data) if line]


# ------------------------------------------------------------------ patch -----

def make_patch(original, changed, name, context=1):
    """Unified diff of two byte strings; empty when they are equal."""
    a, b = _lines(original), _lines(changed)
    out = [f"--- {name}\n+++ {name}\n".encode()]
    matcher = difflib.SequenceMatcher(None, a, b, autojunk=False)
    for group in matcher.get_grouped_opcodes(context):
        i1, i2, j1, j2 = group[0][1], group[-1][2], group[0][3], group[-1][4]
        out.append(f"@@ -{i1 + 1},{i2 - i1} +{j1 + 1},{j2 - j1} @@\n".encode())
        for tag, a1, a2, b1, b2 in group:
            rows = [(b" ", line) for line in a[a1:a2]] if tag == "equal" else \
                [(b"-", line) for line in a[a1:a2]] + [(b"+", line) for line in b[b1:b2]]
            for sign, line in rows:
                out.append(sign + line)
                if not line.endswith(b"\n"):
                    out.append(b"\n\\ No newline at end of file\n")
    return b"".join(out) if len(out) > 1 else b""


class PatchError(ValueError):
    """A change (hunk) of a patch that cannot be placed in the file: `hunk` is its number (from
    1) of `hunks`, `line` the line of its first changed line in the original the patch was made
    for (from 1), `found` how many times its lines occur in the file (0, more than once, or once
    but before the hunk ahead of it), `text` that changed line (shortened)."""
    def __init__(self, name, hunk, hunks, line, found, text):
        self.name, self.hunk, self.hunks, self.line, self.found, self.text = \
            name, hunk, hunks, line, found, text
        where = "not found" if found == 0 else f"found {found} times" if found > 1 else \
            "found before the change ahead of it"
        super().__init__(f"{name}: change {hunk} of {hunks} (line {line}: {text}) - its lines "
                         f"{where} in the file")

    def short(self):
        """'change 2 of 4 (line 57: ...): not found' - for a message that names the file."""
        where = "not found" if self.found == 0 else f"found {self.found} times" \
            if self.found > 1 else "out of order"
        return f"change {self.hunk} of {self.hunks} (line {self.line}: {self.text}): {where}"


def _hunks(patch):
    """The hunks of a patch: (start in the original, from 0; [[sign, line], ...])."""
    hunks, rows = [], None
    for line in _lines(patch):
        m = re.match(rb"@@ -(\d+)(?:,(\d+))? \+\d+(?:,\d+)? @@", line)
        if m:
            start = int(m.group(1)) - (0 if m.group(2) == b"0" else 1)
            rows = []
            hunks.append((start, rows))
        elif rows is not None and line[:1] in (b" ", b"-", b"+"):
            rows.append([line[:1], line[1:]])
        elif rows is not None and line.startswith(b"\\"):   # "\ No newline at end of file"
            rows[-1][1] = rows[-1][1][:-1]
    return hunks


def apply_patch(original, patch, name="file"):
    """Applies a patch from make_patch (or diff -u). Each hunk is placed by its lines: at its line
    number when the file has its "before" lines (the removed and context ones, byte for byte)
    there, else where they occur exactly once in the file - an original with lines added, removed
    or changed elsewhere (another language's Steam depot) takes the patch too. Raises PatchError
    (a ValueError) when a hunk's lines occur nowhere, more than once, or before the hunk ahead of
    it."""
    src = _lines(original)
    hunks = _hunks(patch)
    out, pos = [], 0
    for number, (start, rows) in enumerate(hunks, 1):
        before = [text for sign, text in rows if sign != b"+"]
        n = len(before)
        if not (start >= pos and src[start:start + n] == before):
            found = [i for i in range(len(src) - n + 1) if src[i:i + n] == before] if n else []
            if len(found) != 1 or found[0] < pos:
                first = next((i for i, (sign, _) in enumerate(rows) if sign != b" "), 0)
                text = rows[first][1].strip().decode("latin-1") if rows else ""
                raise PatchError(name, number, len(hunks), start + first + 1, len(found),
                                 text[:40] + ("..." if len(text) > 40 else ""))
            start = found[0]
        out += src[pos:start]
        pos = start
        for sign, text in rows:
            if sign != b"-":
                out.append(text)
            if sign != b"+":
                pos += 1
    out += src[pos:]
    return b"".join(out)


def tests():
    """Checks apply_patch's placement; raises AssertionError on a failure."""
    original = b"".join(b"line %d\r\n" % i for i in range(1, 21))
    changed = original.replace(b"line 5\r\n", b"line 5\r\nnew a\r\n").replace(b"line 15\r\n", b"LINE 15\r\n")
    patch = make_patch(original, changed, "t.txt")
    assert apply_patch(original, patch) == changed                      # at its line numbers
    shifted = b"added 1\r\nadded 2\r\n" + original
    assert apply_patch(shifted, patch) == b"added 1\r\nadded 2\r\n" + changed
    middle = original.replace(b"line 10\r\n", b"line 10\r\nmore\r\nmore\r\nmore\r\n")
    assert apply_patch(middle, patch) == changed.replace(b"line 10\r\n", b"line 10\r\nmore\r\nmore\r\nmore\r\n")
    assert apply_patch(original.replace(b"\r\n", b"\n"), patch.replace(b"\r\n", b"\n")) == \
        changed.replace(b"\r\n", b"\n")                                  # line endings kept as they are
    cp = b"\xe8\xf9\xe0\r\n" + original                                  # bytes of a code page
    assert apply_patch(cp, patch) == b"\xe8\xf9\xe0\r\n" + changed
    for broken, found in [(original.replace(b"line 15\r\n", b"line 15 \r\n"), 0),   # changed
                          (original.replace(b"line 16\r\n", b""), 0),               # missing
                          (shifted.replace(b"added 1\r\n", b"line 14\r\nline 15\r\nline 16\r\n"), 2)]:
        try:
            apply_patch(broken, patch, "t.txt")
        except PatchError as e:
            assert (e.hunk, e.hunks, e.found, e.line) == (2, 2, found, 15), (e.hunk, e.found, e.line)
        else:
            raise AssertionError("a patch placed where its lines are not, or twice")
    # the lines of the second hunk found only before the first one: out of order
    swapped = original.replace(b"line 14\r\nline 15\r\nline 16\r\n", b"").replace(
        b"line 1\r\n", b"line 1\r\nline 14\r\nline 15\r\nline 16\r\n", 1)
    try:
        apply_patch(b"x\r\n" + swapped, patch)
    except PatchError as e:
        assert e.found == 1 and e.hunk == 2
    else:
        raise AssertionError("hunks placed out of order")
    print("patch_text: placement tests passed")


# --------------------------------------------------------------- override -----

def merge_ini(original, override, name="file.ini", fill_only=False):
    """Applies an .override.ini (see the module description) to the original INI bytes. With
    `fill_only` the original's own values stay: a key=value line sets only a key the original
    lacks or leaves empty, and the lines marked ! (its corrections) are applied."""
    src = _lines(original)
    newline = b"\r\n" if b"\r\n" in original else b"\n"

    def key_of(line):
        s = line.strip()
        return s.split(b"=", 1)[0].strip().lower() if b"=" in s and not s.startswith(b";") else None

    # sections of the original: name -> [first line, end line)
    def sections():
        found, current = {}, None
        for i, line in enumerate(src):
            s = line.strip()
            if s.startswith(b"[") and s.endswith(b"]"):
                current = s[1:-1].lower()
                found.setdefault(current, [i + 1, i + 1])
            if current is not None and s:
                found[current][1] = i + 1
        return found

    def find(section, key):
        start, end = sections()[section]
        for i in range(start, end):
            if key_of(src[i]) == key:
                return i
        return None

    section, after = None, None
    for raw in _lines(override):
        line = raw.rstrip(b"\r\n")
        s = line.strip()
        if not s or s.startswith(b";"):
            continue
        if s.startswith(b"[") and s.endswith(b"]"):
            section, after = s[1:-1].lower(), None
            if section not in sections():   # a section of our own: added at the end
                if src and not src[-1].endswith(b"\n"):
                    src[-1] += newline
                src += [newline, s + newline]
            continue
        if section is None:
            raise ValueError(f"{name}: '{s.decode('latin-1')}' outside a section")
        if s.startswith(b"-"):
            key = s[1:].strip().lower()
            first = find(section, key)
            start, end = sections()[section]
            for i in reversed(range(start, end)):
                if i != first and key_of(src[i]) == key:
                    del src[i]
            after = None
            continue
        correction = s.startswith(b"!")
        if correction:
            if b"=" not in s:
                raise ValueError(f"{name}: '{s.decode('latin-1')}' - a correction needs a value")
            if not fill_only:
                continue
            line = s = s[1:].lstrip()
        key = (s.split(b"=", 1)[0] if b"=" in s else s).strip().lower()
        i = find(section, key)
        if b"=" not in s:
            if i is None:
                raise ValueError(f"{name}: no key {s.decode('latin-1')} in [{section.decode('latin-1')}]")
        elif i is not None:
            if fill_only and not correction and src[i].split(b"=", 1)[1].strip():
                after = i   # the original's own value stays
                continue
            ending = src[i][len(src[i].rstrip(b"\r\n")):] or newline
            src[i] = line.lstrip() + ending
        else:
            i = after + 1 if after is not None else sections()[section][1]
            if i > 0 and not src[i - 1].endswith(b"\n"):
                src[i - 1] += newline
            src.insert(i, line.lstrip() + newline)
        after = i
    return b"".join(src)


# ------------------------------------------------------------------- files -----

def read(path):
    with open(path, "rb") as f:
        return f.read()


def write_if_changed(path, data):
    """Writes the file only when its content changes. Returns True if it wrote."""
    if os.path.isfile(path) and read(path) == data:
        return False
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(data)
    return True


def changed_file(change_file, original_file, original=None, fill_only=False):
    """Content of the game file that `change_file` (.patch or .override.ini) describes; the
    original is read from `original_file` unless its content is given. `fill_only` is
    merge_ini's: an override then only fills and corrects the original."""
    name = os.path.basename(change_file)
    if original is None:
        original = read(original_file)
    if name.endswith(".patch"):
        return apply_patch(original, read(change_file), change_file)
    return merge_ini(original, read(change_file), change_file, fill_only)


def target_name(rel_path):
    """Game file a repo file stands for: x.patch -> x, x.override.ini -> x.ini, or None
    when the file is copied as it is."""
    if rel_path.endswith(".patch"):
        return rel_path[:-len(".patch")]
    if rel_path.endswith(".override.ini"):
        return rel_path[:-len(".override.ini")] + ".ini"
    return None


if __name__ == "__main__":
    if len(sys.argv) == 5 and sys.argv[1] == "diff":
        data = make_patch(read(sys.argv[2]), read(sys.argv[3]), os.path.basename(sys.argv[2]))
        open(sys.argv[4], "wb").write(data)
    elif len(sys.argv) == 5 and sys.argv[1] == "apply":
        open(sys.argv[4], "wb").write(apply_patch(read(sys.argv[2]), read(sys.argv[3]), sys.argv[3]))
    elif len(sys.argv) == 5 and sys.argv[1] == "merge":
        open(sys.argv[4], "wb").write(merge_ini(read(sys.argv[2]), read(sys.argv[3]), sys.argv[3]))
    elif sys.argv[1:] == ["test"]:
        tests()
    else:
        sys.exit(__doc__)
