r"""Changes to the game's text files, kept as patches instead of whole files.

The repository holds only our changes; the original file comes from the player's own copy
of the game when it is assembled (build.py). Two kinds of change files:

<file>.patch          a unified diff against the original <file> (scripts, menus). Made
                      and applied byte for byte: the game files are CP1250/CP1251 with CRLF
                      line endings, which the patch keeps. The original must match exactly,
                      otherwise applying fails and says where.
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

    python tools\patch_text.py diff <original> <changed> <file.patch>
    python tools\patch_text.py apply <original> <file.patch> <output>
    python tools\patch_text.py merge <original.ini> <override.ini> <output>
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


def apply_patch(original, patch, name="file"):
    """Applies a patch from make_patch (or diff -u). Raises ValueError when the original
    differs from the one the patch was made for."""
    src = _lines(original)
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
    out, pos = [], 0
    for start, rows in hunks:
        if start < pos:
            raise ValueError(f"{name}: overlapping hunks")
        out += src[pos:start]
        pos = start
        for sign, text in rows:
            if sign == b"+":
                out.append(text)
                continue
            if pos >= len(src) or src[pos] != text:
                raise ValueError(f"{name}: line {pos + 1} differs from the original the patch "
                                 f"was made for")
            if sign == b" ":
                out.append(text)
            pos += 1
    out += src[pos:]
    return b"".join(out)


# --------------------------------------------------------------- override -----

def merge_ini(original, override, name="file.ini"):
    """Applies an .override.ini (see the module description) to the original INI bytes."""
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
        key = (s.split(b"=", 1)[0] if b"=" in s else s).strip().lower()
        i = find(section, key)
        if b"=" not in s:
            if i is None:
                raise ValueError(f"{name}: no key {s.decode('latin-1')} in [{section.decode('latin-1')}]")
        elif i is not None:
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


def changed_file(change_file, original_file):
    """Content of the game file that `change_file` (.patch or .override.ini) describes."""
    name = os.path.basename(change_file)
    if name.endswith(".patch"):
        return apply_patch(read(original_file), read(change_file), change_file)
    return merge_ini(read(original_file), read(change_file), change_file)


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
    else:
        sys.exit(__doc__)
