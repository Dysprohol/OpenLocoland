r"""objects.res as editable tables: unpack to TSV files, pack back, apply partial overrides.

    python tools\res_tool.py unpack <objects.res> <folder>
    python tools\res_tool.py pack <folder> <objects.res>
    python tools\res_tool.py apply <objects.res> <override folder> <output objects.res>

unpack writes one TSV per table (UTF-8, tab-separated, a header row of column names):

  objects.tsv     OBJ chunk, one row per object type (VID), in file order
  animations.tsv  OBJ chunk, the 17 per-animation entries of each object; only the
                  entries that are not all zero (a missing row is all zero)
  weapons.tsv     WEAP chunk, 55 records
  constants.tsv   CNST chunk, 26 ints (meaning unknown)
  sounds.tsv      SFX chunk, one row per sound

The columns are the fields of the records in file order, named as in re/types.txt (struct
Vid, Weapon), with values as they are stored in the file (not as the engine converts them
when loading, e.g. speed in px/s). Unknown bytes are kept as hex columns, so nothing is
lost: unpack checks that packing the tables gives back the same file byte for byte and
refuses otherwise. Lines starting with # are comments.

apply takes a folder with TSV files named like the tables, each with only the rows and
columns to change: the key columns (nvid; nvid + anim; index) and any others. An empty
cell keeps the value. A row with an nvid not in objects.res adds a new object type (the
columns it leaves out are 0 / empty); a row with the next free index adds a weapon or a
sound. build.py applies game/objects.override/ this way (see DEVELOPERS.md "Data formats").
"""
import decimal
import math
import os
import struct
import sys

ENCODING = "cp1251"     # object names and file names in objects.res

ANIMATIONS = ["STAND", "STOP_MOVE", "GO", "START_MOVE", "L_ROTATE", "R_ROTATE", "OPEN",
              "CLOSE", "FIGHT", "SALUT", "STAND_OPEN", "LOAD", "UNLOAD", "WOUND", "BIRTH",
              "DEATH", "DEATH2"]   # ANI_... of export.lgc (7 is also HIT, 11 LINK, 12 LAZY/CLASH)
CLASSES = {0: "TERRAIN", 1: "OBJECT", 2: "UNIT", 3: "BUILDING", 4: "AVIA", 5: "CANNON",
           6: "PRIMITIVE", 9: "SPRITE", 10: "FRAME", 12: "LINKER", 19: "TEXT",
           20: "CIV_ROBOT", 21: "ENGINE", 22: "RAIL", 23: "REGION", 24: "DEPO",
           25: "CREATURE", 26: "BALLOON", 27: "MISSILE"}   # B_... of export.lgc

# Field kinds: i int, h int shown in hex, f float, c class (B_ name or number), b<n> n raw
# bytes in hex. A field is (column, kind).
OBJ_HEAD = [("spriteType", "h"), ("spriteClass", "c"), ("flags", "h"), ("moveMask", "h"),
            ("sizeX", "f"), ("sizeY", "f"), ("sizeZ", "f"), ("maxHp", "i"),
            ("speed", "f"), ("speedZ", "f"), ("acceleration", "f"), ("deceleration", "f"),
            ("rotateTime", "f"), ("weaponIndex", "i"), ("damageRadius", "f"), ("damage", "i"),
            ("linkOffsetX", "f"), ("linkOffsetY", "f"), ("linkOffsetZ", "f"),
            ("linkNvid", "i"), ("altitude", "f"), ("unknown_54", "b28"), ("nDir", "i")]
OBJ_ANIM = [("frames", "i"), ("sfx", "i"), ("childForward", "f"), ("childSide", "f"),
            ("childZ", "f"), ("childNvid", "i"), ("childCount", "i")]   # 17 of each, in turn
OBJ_TAIL = [("gammaR", "i"), ("gammaG", "i"), ("gammaB", "i"), ("gammaA", "i"),
            ("scaleX", "f"), ("scaleY", "f"), ("scaleZ", "f")]          # then imageFile
WEAPON = [("kind", "i"), ("flags", "h"), ("length", "f"), ("mass", "f"),
          ("power", "f"), ("detectRange", "f"), ("battleRange", "f"), ("aim", "f"),
          ("reloadTime", "i"), ("buildTime", "i"), ("lifeTime", "i"), ("ammo", "i"),
          ("army", "i"), ("behave", "i"), ("iconFrame", "i"), ("priority", "i"),
          ("unknown_40", "f")]
SOUND_FILES = 8

COLUMNS = {
    "objects": ["nvid", "name"] + [c for c, _ in OBJ_HEAD + OBJ_TAIL] + ["imageFile"],
    "animations": ["nvid", "anim"] + [c for c, _ in OBJ_ANIM],
    "weapons": ["index"] + [c for c, _ in WEAPON],
    "constants": ["index", "value"],
    "sounds": ["index", "param"] + [f"file{i + 1}" for i in range(SOUND_FILES)],
}
KEYS = {"objects": ["nvid"], "animations": ["nvid", "anim"], "weapons": ["index"],
        "constants": ["index"], "sounds": ["index"]}
CHUNKS = [b"OBJ ", b"CNST", b"WEAP", b"SFX "]   # order in objects.res


class DataError(Exception):
    pass


# ---------------------------------------------------------------- values -----

def show(kind, raw):
    """Text of a field stored as `raw` bytes."""
    if kind == "f":
        v = struct.unpack("<f", raw)[0]
        if math.isfinite(v):
            for p in range(1, 10):
                s = f"{v:.{p}g}"
                if struct.pack("<f", float(s)) == raw:
                    plain = format(decimal.Decimal(s), "f")    # 300, not 3e+02
                    return plain if len(plain) <= 16 else s
        return "0x%08x" % struct.unpack("<I", raw)[0]
    if kind[0] == "b":
        return "" if not any(raw) else raw.hex()
    v = struct.unpack("<i", raw)[0]
    if kind == "h":
        return "0x%x" % (v & 0xffffffff)
    if kind == "c":
        return CLASSES.get(v, str(v))
    return str(v)


def parse_int(text):
    t = text.strip().lower()
    neg = t.startswith("-")
    if neg:
        t = t[1:]
    v = int(t, 16) if t.startswith("0x") else int(t)
    return -v if neg else v


def store(kind, text):
    """Bytes of a field from its text."""
    if kind == "f":
        if text.lower().startswith("0x"):
            return struct.pack("<I", int(text, 16))
        return struct.pack("<f", float(text))
    if kind[0] == "b":
        n = int(kind[1:])
        raw = bytes.fromhex(text) if text else bytes(n)
        if len(raw) != n:
            raise ValueError(f"needs {n} bytes in hex")
        return raw
    if kind == "c":
        names = {v: k for k, v in CLASSES.items()}
        v = names[text.upper()] if text.upper() in names else parse_int(text)
    else:
        v = parse_int(text)
    if not -0x80000000 <= v <= 0xffffffff:
        raise ValueError("does not fit in 32 bits")
    return struct.pack("<I", v & 0xffffffff)


def field(row, column, kind, empty=None):
    """Bytes of a cell; `empty` stands for an empty cell. The error names the column."""
    text = row[column] or empty or ""
    try:
        return store(kind, text)
    except (ValueError, KeyError) as e:
        raise ValueError(f"{column} {text!r}: {e}") from None


def show_text(raw):
    s = raw.decode(ENCODING)
    if any(ch in s for ch in "\t\r\n"):
        raise DataError(f"text with a tab or line break: {s!r}")
    return s


def store_text(text):
    return text.encode(ENCODING) + b"\0"


def anim_index(text):
    t = text.strip().upper()
    if t.startswith("ANI_"):
        t = t[4:]
    if t in ANIMATIONS:
        return ANIMATIONS.index(t)
    i = int(t)
    if not 0 <= i < len(ANIMATIONS):
        raise ValueError("animation is 0-16 or an ANI_ name")
    return i


# --------------------------------------------------------------- container ---

def read_chunks(data, kind=b"DATA"):
    """[(id, header flags, extra, [record bytes])] of a RES file with the record layer."""
    if data[:4] != b"RES " or data[8:12] != kind:
        raise DataError(f"not a RES file of type {kind.decode()}")
    chunks, o = [], 12
    while o < len(data):
        cid = data[o:o + 4]
        size, flags = struct.unpack_from("<II", data, o + 4)
        if not flags & 0x80000000:
            raise DataError(f"chunk {cid!r} has the old header")
        extra, count = struct.unpack_from("<II", data, o + 12)
        records, r, end = [], o + 20, o + 8 + size
        for _ in range(count):
            if r + 4 > end:
                raise DataError(f"chunk {cid!r}: records do not fill it")
            n = struct.unpack_from("<I", data, r)[0]
            records.append(data[r + 4:r + 4 + n])
            r += 4 + n
        if r != end:
            raise DataError(f"chunk {cid!r}: records do not fill it")
        chunks.append((cid, flags, extra, records))
        o = end + (size & 1)
    return chunks


def write_chunks(chunks, kind=b"DATA"):
    """The RES file. Chunks are padded to an even size except the last one; the size in
    the RES header counts the chunks without that padding (as in the shipped files)."""
    body, unpadded = bytearray(), 4
    for i, (cid, flags, extra, records) in enumerate(chunks):
        payload = struct.pack("<III", flags, extra, len(records)) + b"".join(
            struct.pack("<I", len(r)) + r for r in records)
        body += cid + struct.pack("<I", len(payload)) + payload
        unpadded += 8 + len(payload)
        if len(payload) & 1 and i < len(chunks) - 1:
            body += b"\0"
    return b"RES " + struct.pack("<I", unpadded) + kind + bytes(body)


# ------------------------------------------------------------ records/rows ---

def cstring(rec, o):
    e = rec.index(b"\0", o)
    return rec[o:e], e + 1


def object_rows(rec, trailing=False):
    """(objects row, animations rows) of an OBJ record. With `trailing` the bytes after the
    image file name are kept in row["trailing"] (hex) instead of being an error."""
    nvid = struct.unpack_from("<i", rec, 0)[0]
    name, o = cstring(rec, 4)
    row = {"nvid": str(nvid), "name": show_text(name)}
    for col, kind in OBJ_HEAD:
        n = int(kind[1:]) if kind[0] == "b" else 4
        row[col] = show(kind, rec[o:o + n])
        o += n
    anims = rec[o:o + 17 * 4 * len(OBJ_ANIM)]
    o += len(anims)
    for col, kind in OBJ_TAIL:
        row[col] = show(kind, rec[o:o + 4])
        o += 4
    image, o = cstring(rec, o)
    row["imageFile"] = show_text(image)
    if trailing:
        row["trailing"] = rec[o:].hex()
    elif o != len(rec):
        raise DataError(f"object {nvid}: {len(rec) - o} bytes after the image file")
    anim_rows = []
    for a in range(17):
        cells = [anims[(f * 17 + a) * 4:(f * 17 + a) * 4 + 4] for f in range(len(OBJ_ANIM))]
        if any(any(c) for c in cells):
            r = {"nvid": str(nvid), "anim": ANIMATIONS[a]}
            r.update((col, show(kind, c)) for (col, kind), c in zip(OBJ_ANIM, cells))
            anim_rows.append(r)
    return row, anim_rows


def object_record(row, anim_rows):
    rec = bytearray(field(row, "nvid", "i") + store_text(row["name"]))
    for col, kind in OBJ_HEAD:
        rec += field(row, col, kind)
    cells = [[b"\0" * 4] * 17 for _ in OBJ_ANIM]
    for r in anim_rows:
        a = anim_index(r["anim"])
        for f, (col, kind) in enumerate(OBJ_ANIM):
            cells[f][a] = field(r, col, kind, "0")
    for column in cells:
        rec += b"".join(column)
    for col, kind in OBJ_TAIL:
        rec += field(row, col, kind)
    return bytes(rec + store_text(row["imageFile"]) + bytes.fromhex(row.get("trailing", "")))


def sound_row(index, rec):
    row = {"index": str(index), "param": str(rec[0])}
    o = 1
    for i in range(SOUND_FILES):
        s, o = cstring(rec, o)
        row[f"file{i + 1}"] = show_text(s)
    if o != len(rec):
        raise DataError(f"sound {index}: {len(rec) - o} bytes after the files")
    return row


def sound_record(row):
    param = parse_int(row["param"])
    if not 0 <= param <= 255:
        raise ValueError("param is a byte")
    return bytes([param]) + b"".join(store_text(row[f"file{i + 1}"]) for i in range(SOUND_FILES))


def read_tables(data):
    """{table: [row]} of objects.res; a row is {column: text}."""
    chunks = read_chunks(data)
    if [c[0] for c in chunks] != CHUNKS or any(c[1:3] != (0x80000000, 0) for c in chunks):
        raise DataError("unexpected chunks - a different version of objects.res?")
    by_id = {c[0]: c[3] for c in chunks}
    tables = {"objects": [], "animations": []}
    for rec in by_id[b"OBJ "]:
        row, anims = object_rows(rec)
        tables["objects"].append(row)
        tables["animations"] += anims
    (cnst,) = by_id[b"CNST"]
    tables["constants"] = [{"index": str(i), "value": show("i", cnst[i * 4:i * 4 + 4])}
                           for i in range(len(cnst) // 4)]
    tables["weapons"] = []
    for i, rec in enumerate(by_id[b"WEAP"]):
        if len(rec) != 4 * len(WEAPON):
            raise DataError(f"weapon {i}: {len(rec)} bytes")
        row = {"index": str(i)}
        row.update((col, show(kind, rec[j * 4:j * 4 + 4])) for j, (col, kind) in enumerate(WEAPON))
        tables["weapons"].append(row)
    tables["sounds"] = [sound_row(i, rec) for i, rec in enumerate(by_id[b"SFX "])]
    return tables


def check_order(table, rows):
    for i, row in enumerate(rows):
        if row["index"] != str(i):
            raise DataError(f"{table}: index {row['index']} where {i} was expected")


def write_tables(tables):
    """objects.res bytes from the tables. Raises DataError naming the bad cell."""
    def cell(table, row, fn):
        try:
            return fn(row)
        except DataError:
            raise
        except Exception as e:  # noqa: BLE001 - the message names the row
            key = ", ".join(f"{k} {row.get(k)}" for k in KEYS[table])
            raise DataError(f"{table}, {key}: {e}") from None

    anims = {}
    for r in tables["animations"]:
        anims.setdefault(r["nvid"].strip(), []).append(r)
    known = {r["nvid"].strip() for r in tables["objects"]}
    for nvid in anims:
        if nvid not in known:
            raise DataError(f"animations: nvid {nvid} is not in objects")
    for r in tables["animations"]:     # checked first, so that an error names the row
        cell("animations", r, lambda r: [anim_index(r["anim"])] +
             [field(r, col, kind, "0") for col, kind in OBJ_ANIM])
    obj = [cell("objects", r, lambda r: object_record(r, anims.get(r["nvid"].strip(), [])))
           for r in tables["objects"]]
    for t in ("constants", "weapons", "sounds"):
        check_order(t, tables[t])
    cnst = b"".join(cell("constants", r, lambda r: field(r, "value", "i")) for r in tables["constants"])
    weap = [cell("weapons", r, lambda r: b"".join(field(r, col, kind) for col, kind in WEAPON))
            for r in tables["weapons"]]
    sfx = [cell("sounds", r, sound_record) for r in tables["sounds"]]
    return write_chunks([(b"OBJ ", 0x80000000, 0, obj), (b"CNST", 0x80000000, 0, [cnst]),
                         (b"WEAP", 0x80000000, 0, weap), (b"SFX ", 0x80000000, 0, sfx)])


# --------------------------------------------------------------------- TSV ---

def save_tsv(path, columns, rows):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\t".join(columns) + "\n")
        for r in rows:
            f.write("\t".join(r[c] for c in columns) + "\n")


def load_tsv(path):
    """(columns, [(line number, row)]); comment lines (#) and blank lines are skipped."""
    with open(path, encoding="utf-8-sig") as f:
        lines = [(n, line.rstrip("\r\n")) for n, line in enumerate(f, 1)]
    lines = [(n, l) for n, l in lines if l.strip() and not l.startswith("#")]
    if not lines:
        return [], []
    columns = lines[0][1].split("\t")
    rows = []
    for n, line in lines[1:]:
        cells = line.split("\t")
        if len(cells) > len(columns):
            raise DataError(f"{path}:{n}: {len(cells)} cells, {len(columns)} columns")
        cells += [""] * (len(columns) - len(cells))
        rows.append((n, dict(zip(columns, cells))))
    return columns, rows


def unpack(res_path, folder):
    data = open(res_path, "rb").read()
    tables = read_tables(data)
    if write_tables(tables) != data:
        raise DataError(f"{res_path} would not pack back to the same bytes")
    os.makedirs(folder, exist_ok=True)
    for t, rows in tables.items():
        save_tsv(os.path.join(folder, t + ".tsv"), COLUMNS[t], rows)
    return tables


def pack(folder, res_path):
    tables = {}
    for t, columns in COLUMNS.items():
        path = os.path.join(folder, t + ".tsv")
        cols, rows = load_tsv(path)
        missing = [c for c in columns if c not in cols]
        if missing:
            raise DataError(f"{path}: missing columns {', '.join(missing)}")
        tables[t] = [r for _, r in rows]
    data = write_tables(tables)
    open(res_path, "wb").write(data)
    return data


# --------------------------------------------------------------- overrides ---

def apply_overrides(tables, folder):
    """Applies the override TSVs in `folder` to `tables` (in place). Returns the number of
    changed cells."""
    changed = 0
    # objects first: a new object type has to exist before its animations
    for name in sorted(os.listdir(folder), key=lambda n: (n.lower() != "objects.tsv", n)):
        table, ext = os.path.splitext(name)
        path = os.path.join(folder, name)
        if ext.lower() != ".tsv":
            continue
        if table not in COLUMNS:
            raise DataError(f"{path}: no table {table} (tables: {', '.join(COLUMNS)})")
        columns, rows = load_tsv(path)
        keys = KEYS[table]
        for c in columns:
            if c not in COLUMNS[table]:
                raise DataError(f"{path}: no column {c} in {table}")
        for k in keys:
            if k not in columns:
                raise DataError(f"{path}: the key column {k} is missing")

        def key_of(row):
            if table == "animations":
                return row["nvid"].strip(), ANIMATIONS[anim_index(row["anim"])]
            return tuple(row[k].strip() for k in keys)

        index = {key_of(r): r for r in tables[table]}
        nvids = {r["nvid"].strip() for r in tables["objects"]}
        for n, row in rows:
            try:
                key = key_of(row)
            except ValueError as e:
                raise DataError(f"{path}:{n}: {e}") from None
            target = index.get(key)
            if target is None:
                if table == "animations" and key[0] not in nvids:
                    raise DataError(f"{path}:{n}: nvid {key[0]} is not in objects")
                if table in ("weapons", "sounds", "constants") and key[0] != str(len(tables[table])):
                    raise DataError(f"{path}:{n}: no {table} entry {key[0]} (a new one must be "
                                    f"{len(tables[table])})")
                if table == "constants":
                    raise DataError(f"{path}:{n}: CNST has a fixed size")
                target = {c: "" if c in ("name", "imageFile") or c.startswith("file") or
                          c == "unknown_54" else "0" for c in COLUMNS[table]}
                target.update(zip(keys, key))
                tables[table].append(target)
                index[key] = target
                if table == "objects":
                    nvids.add(key[0])
            for c in columns:
                if c not in keys and row[c] != "" and target[c] != row[c]:
                    target[c] = row[c]
                    changed += 1
    return changed


def apply(res_path, folder, output):
    """Writes `output`: `res_path` with the overrides from `folder`. Returns the number of
    changed cells."""
    tables = read_tables(open(res_path, "rb").read())
    changed = apply_overrides(tables, folder)
    data = write_tables(tables)
    open(output, "wb").write(data)
    return changed


def main(argv):
    commands = {"unpack": (unpack, 2), "pack": (pack, 2), "apply": (apply, 3)}
    if len(argv) < 1 or argv[0] not in commands or len(argv) - 1 != commands[argv[0]][1]:
        sys.exit(__doc__.split("\n\n")[1])
    fn = commands[argv[0]][0]
    try:
        result = fn(*argv[1:])
    except DataError as e:
        sys.exit(f"error: {e}")
    if argv[0] == "unpack":
        print(", ".join(f"{t}.tsv {len(r)} rows" for t, r in result.items()))
    elif argv[0] == "apply":
        print(f"{result} values changed")


if __name__ == "__main__":
    main(sys.argv[1:])
