r"""Maps and images (.map, .vid) as editable files: unpack, pack back, apply overrides.

    python tools\map_tool.py unpack <file.map | file.vid> <folder> [--objects <objects.res>]
    python tools\map_tool.py pack <folder> <file>
    python tools\map_tool.py new <folder> <template.map> [--vid <image>] [--script] [--size 2048x1536] [--objects <objects.res>]
    python tools\map_tool.py ground <folder> <width> <height>
    python tools\map_tool.py paint <folder> <game folder>
    python tools\map_tool.py apply <file.map> <override folder> <output.map> [--objects <objects.res>]
    python tools\map_tool.py list <file.map> [--objects <objects.res>]
    python tools\map_tool.py vids <objects.res> [--grep <text>]
    python tools\map_tool.py place <folder> <nvid> <x> <y> [--from <map>] [--army <n>]
                           [--direction <n>] [--id <n>] [--z <n>] [--objects <objects.res>]
    python tools\map_tool.py copy <folder> <source.map> <nvid> [--objects <objects.res>]
    python tools\map_tool.py check <game folder>
    python tools\map_tool.py check-track <file.map | folder> [--objects <objects.res>]

A new map keeps the terrain of its template and nothing else: the ground object (VID
1024) with its image, GRPH, HEAD and GRID stay, all other objects, the groups and the
players go. `--vid` points the ground at another image (a map built on another
ground), `--script` writes `<folder name>.lgc`, a mission that runs as it is, and
`--size` makes the map bigger (the ground height of the cells is flat then).

`ground` writes the image of a map ground: the empty picture of the given size, its
height and the tiles that cut it up - the terrain is painted into the picture.

`paint` paints the picture of an unpacked map (its vid\ground.png) with the editor's textures, as
the editor's Paint tool paints: the textures that the game folder's editor	extures.tsv names in the
campaign grounds - grass, patches of light grass, sand as packed earth under the bases -, and the
gravel bed's band (the meadow bed decal's top) under every rail; the height is left as it is.

`list` prints what a map holds, `vids` the objects of objects.res (the ones to place),
`place` puts an object into an unpacked map: its state (behave, items, data) is
copied from another object of the same VID, because the engine reads that state when
the level starts. `copy` adds every object of one VID of another map - the rails of a
map into a new map on the same terrain, for example - with new ids and without their
commands (the orders of the source mission).

A map (RES type MAP) unpacks to:

  map.json            the chunk order, GRPH (environment, gamma, wind), HEAD (size, camera,
                      version), PLAY (the Flagman id of each army) and, as hex, every chunk
                      the tool does not read
  objects.tsv         SPR + SPRD, one row per object in file order (the first is the ground):
                      id, nvid, name, x, y, z, direction, army, commands, behave, items, data
  groups.txt          GROU, one unit group per line: the member ids
  grid.txt            GRID, the ground height of each 8x8 pixel cell, one row per line
  vids.tsv            OBJ, the level's own VIDs (VID 1024, the ground) with the columns of
  vid_animations.tsv  objects.tsv and animations.tsv of res_tool.py, plus `trailing`: bytes
                      the editor left after the image file name (uninitialised memory), hex

An image (RES type VID) unpacks to vid.json (the chunk order and HEAD: imageFlags,
frameSpeed, frameCount, width, height) and, by the kind of image:

  sprite   (RLE frames)  frames.png - every frame in a cell of a sheet, frames.tsv - one row
           per frame (reference, shadow, raw), palette.tsv, depth.png (imageFlags 4), mask.png
  texture  (imageFlags 0x20)  texture_NNN.png (+ height_NNN.png with imageFlags 4), or for a
           map ground one ground.png (+ height.png), and tiles.tsv
  color    (imageFlags 0x80)  colors.tsv

The formats are described in DEVELOPERS.md ("Data formats"). pack builds the file again from the
folder; unpack of a map checks that its tables pack back to the same bytes and refuses
otherwise, and check unpacks and packs every map and image of a game folder through the disk
and compares the results byte for byte.

objects.res (found next to the game's maps, or --objects) gives the object names and their
classes, which say how the SPRD record of an object is laid out; without it the whole record
after the commands lands in `data`.

apply takes a folder with objects.tsv holding only the rows and columns to change: the key
column id and any others; an empty cell keeps the value. A row with an id the map does not
have adds an object (needs nvid), a row with `remove` = 1 deletes one. build.py applies
game/maps.override/<map name>/ this way (see DEVELOPERS.md "Data formats").
"""
import argparse
import array
import concurrent.futures
import json
import math
import os
import re
import shutil
import struct
import sys
import tempfile
from collections import Counter

from PIL import Image, ImageChops

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from res_tool import DataError, load_tsv, parse_int, save_tsv, show  # noqa: E402
import res_tool  # noqa: E402
from safe_path import safe_relpath  # noqa: E402

CHUNK_FLAGS = 0x80000000        # header of every chunk in the shipped files: flags, extra 0
END = -1                        # end marker of SPR, SPRD, GROU


# ------------------------------------------------------------------ common ----


def read_container(data, kind):
    """Chunks [(id, flags, extra, records)] of a RES file of type `kind`. Refuses a layout
    the writer would not give back exactly (padding, sizes, chunk headers)."""
    chunks = res_tool.read_chunks(data, kind)
    if res_tool.write_chunks(chunks, kind) != data:
        raise DataError("container layout not reproduced (padding or sizes)")
    for cid, flags, extra, _ in chunks:
        if (flags, extra) != (CHUNK_FLAGS, 0):
            raise DataError(f"chunk {cid!r}: header flags {flags:#x}, extra {extra}")
    ids = [c[0] for c in chunks]
    if len(set(ids)) != len(ids):
        raise DataError("a chunk id appears twice")
    return {c[0]: c[3] for c in chunks}, ids


def write_container(kind, ids, records):
    return res_tool.write_chunks([(cid, CHUNK_FLAGS, 0, records[cid]) for cid in ids], kind)


def chunk_name(cid):
    return cid.decode("latin-1")


def chunk_id(name):
    cid = name.encode("latin-1")
    if len(cid) != 4:
        raise DataError(f"chunk id {name!r} is not 4 characters")
    return cid


def float_text(raw):
    return show("f", raw)


def float_bytes(text):
    return res_tool.store("f", str(text))


def json_float(raw):
    """A float32 for JSON: a number, or the bits as "0x..." when it has no exact text."""
    t = float_text(raw)
    return t if t.startswith("0x") else float(t)


def save_json(path, value):
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(value, f, indent=2, ensure_ascii=False)
        f.write("\n")


def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def ints_text(values):
    return " ".join(str(v) for v in values)


def text_ints(text):
    return [parse_int(t) for t in text.split()]


def cell(path, row, column, fn):
    """fn(row[column]); a bad value raises DataError naming the file, row and column."""
    try:
        return fn(row[column].strip())
    except DataError:
        raise
    except Exception as e:  # noqa: BLE001 - the message names the cell
        raise DataError(f"{path}: id {row.get('id', '?')}, {column} {row[column]!r}: {e}") from None


# -------------------------------------------------------------------- maps ----

OBJECT_COLUMNS = ["id", "nvid", "name", "x", "y", "z", "direction", "army", "commands",
                  "behave", "items", "data"]
VID_COLUMNS = res_tool.COLUMNS["objects"] + ["trailing"]
VID_ANIM_COLUMNS = res_tool.COLUMNS["animations"]
# classes whose SPRD record continues with the Unit data (behave, item count, items)
UNIT_CLASSES = {"UNIT", "BUILDING", "AVIA", "CIV_ROBOT", "ENGINE", "DEPO", "CREATURE", "BALLOON"}
# object classes that stand on a flat foundation (their sprite has sizeX/sizeY)
BUILDING_CLASSES = {"BUILDING", "DEPO"}
# special VIDs
RAIL_NVID = 610                 # "droga železная обычная"
DEPO_NVID = 102                 # "ДЕПО"
WORKSHOP_NVID = 104             # "МАСТЕРСКАЯ РЕМОНТНАЯ"
FORT_NVID = 162                 # "ФОРТ"
# a depot's exit must snap to a rail node within this distance (px)
DEPOT_RAIL_RADIUS = 100
# a workshop must be no farther than this from a track (Building_UpdateWorkshop)
WORKSHOP_TRACK_LIMIT = 20
# a fort is a gate the track runs through: in the game's maps it stands 0-6 px from the
# track (l10, l11, l15, l16, l20), turned along it
FORT_TRACK_LIMIT = 8
# one grid cell in pixels
GRID_CELL = 8
# SPR entry: id, nvid, x, y, z, direction, army; by format (current/old) and version
SPR_CURRENT = struct.Struct("<iifffii")
SPR_INTS = struct.Struct("<iiiiiii")      # current format before version 10
SPR_OLD = struct.Struct("<ihhhhBB")       # old format (no GRPH): the last byte is unused


def object_info(objects_res):
    """{nvid: (name, class)} from objects.res, or {} without it."""
    if not objects_res:
        return {}
    tables = res_tool.read_tables(open(objects_res, "rb").read())
    return {int(r["nvid"]): (r["name"], r["spriteClass"]) for r in tables["objects"]}


def find_objects_res(path):
    """objects.res of the game a map belongs to: in the map's folder or up to 3 above."""
    d = os.path.dirname(os.path.abspath(path))
    for _ in range(4):
        candidate = os.path.join(d, "objects.res")
        if os.path.isfile(candidate):
            return candidate
        d = os.path.dirname(d)
    return None


def vid_info(objects_res):
    """{nvid: full row dict} from objects.res (sizeX, sizeY, nDir, ...), or {} without it."""
    if not objects_res:
        return {}
    tables = res_tool.read_tables(open(objects_res, "rb").read())
    return {int(r["nvid"]): r for r in tables["objects"]}


def rail_nodes(model):
    """End points (rail-network nodes) of every rail object in the map, in map pixels."""
    return [(round(x + end[0] * 35.5, 3), round(y + end[1] * 25.0, 3))
            for x, y, shape, _z0, _z1 in map_rails(model) for end in RAIL_ENDS[shape]]


def dist_point_to_segment(px, py, ax, ay, bx, by):
    """Distance from point (px, py) to segment (a, b), in pixels."""
    dx, dy = bx - ax, by - ay
    if dx == 0 and dy == 0:
        return math.hypot(px - ax, py - ay)
    t = ((px - ax) * dx + (py - ay) * dy) / (dx * dx + dy * dy)
    t = max(0.0, min(1.0, t))
    cx, cy = ax + t * dx, ay + t * dy
    return math.hypot(px - cx, py - cy)


def _cell_range(center, half):
    """Grid cell indices covered by [center - half, center + half] in pixels."""
    return range(int(math.floor((center - half) / GRID_CELL)),
                 int(math.floor((center + half) / GRID_CELL)) + 1)


def _flatten_footprint(model, x, y, size_x, size_y, z):
    """Flatten the grid under a building: every cell covered by the footprint gets z,
    and the one-cell buffer around it is ramped half way toward z, so the building
    neither sinks into a slope nor levitates above one."""
    width, height = grid_size(model["info"])
    grid = model["grid"]
    cells = [(cx, cy)
             for cy in _cell_range(y, size_y / 2)
             for cx in _cell_range(x, size_x / 2)
             if 0 <= cx < width and 0 <= cy < height]
    for cx, cy in cells:
        grid[cy][cx] = z
    buffer = set()
    for cx, cy in cells:
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                nx, ny = cx + dx, cy + dy
                if (0 <= nx < width and 0 <= ny < height
                        and (nx, ny) not in cells):
                    buffer.add((nx, ny))
    for cx, cy in buffer:
        original = grid[cy][cx]
        grid[cy][cx] = original + (z - original) // 2


def spr_format(model):
    if model["current"]:
        return SPR_CURRENT if model["info"]["head"]["version"] >= 10 else SPR_INTS
    return SPR_OLD


def read_map(data, info):
    """The map as a model: {"info": map.json, "objects", "groups", "grid", "vids",
    "vid_animations"} (missing keys = chunk not present or kept raw) and "names": `info`
    ({nvid: (name, class)} of objects.res) with the level's own VIDs added."""
    records, ids = read_container(data, b"MAP ")
    info = dict(info)
    model = {"current": b"GRPH" in records, "names": info}
    meta = {"chunks": [chunk_name(c) for c in ids]}
    model["info"] = meta
    raw = {}

    def single(cid, size=None):
        recs = records[cid]
        if len(recs) != 1 or size is not None and len(recs[0]) != size:
            raise DataError(f"{chunk_name(cid)}: unexpected records")
        return recs[0]

    readers = []

    def reader(cid):
        def wrap(fn):
            readers.append((cid, fn))
            return fn
        return wrap

    @reader(b"OBJ ")
    def _obj():
        vids, anims = [], []
        for rec in records[b"OBJ "]:
            row, a = res_tool.object_rows(rec, trailing=True)
            vids.append(row)
            info[int(row["nvid"])] = (row["name"], row["spriteClass"])
            anims += a
        model["vids"], model["vid_animations"] = vids, anims

    @reader(b"GRPH")
    def _grph():
        r = single(b"GRPH", 20)
        env, = struct.unpack_from("<I", r, 0)
        wind_dir, = struct.unpack_from("<i", r, 12)
        meta["graphics"] = {"environment": "0x%x" % env, "gamma": r[4:12].hex(),
                            "windDirection": wind_dir, "wind": json_float(r[16:20])}

    @reader(b"HEAD")
    def _head():
        if model["current"]:
            r = single(b"HEAD", 24)
            version, = struct.unpack_from("<i", r, 20)
            if version >= 10:
                values = [json_float(r[i:i + 4]) for i in range(0, 16, 4)]
            else:
                values = list(struct.unpack_from("<4i", r, 0))
            head = dict(zip(["width", "height", "cameraX", "cameraY"], values))
            head["time"], head["version"] = struct.unpack_from("<Ii", r, 16)
        else:
            r = single(b"HEAD")
            w, h, cx, cy, t, version = struct.unpack_from("<iihhIi", r, 0)
            head = {"width": w, "height": h, "cameraX": cx, "cameraY": cy, "time": t,
                    "version": version, "rest": r[20:].hex()}
        meta["head"] = head

    @reader(b"GRID")
    def _grid():
        r = single(b"GRID")
        w, h = grid_size(meta)
        if len(r) != w * h * 2:
            raise DataError("GRID does not match the map size")
        values = struct.unpack(f"<{w * h}h", r)
        model["grid"] = [values[y * w:(y + 1) * w] for y in range(h)]

    @reader(b"SPR ")
    def _spr():
        if b"SPRD" not in records:
            raise DataError("SPR without SPRD")
        fmt = spr_format(model)
        r = single(b"SPR ")
        if (len(r) - 4) % fmt.size or struct.unpack_from("<i", r, len(r) - 4)[0] != END:
            raise DataError("SPR entries")
        sprd = records[b"SPRD"]
        count = (len(r) - 4) // fmt.size
        if len(sprd) != count + 1 or sprd[-1] != struct.pack("<i", END):
            raise DataError("SPRD records do not match SPR")
        rows = []
        for k in range(count):
            e = fmt.unpack_from(r, k * fmt.size)
            rows.append(object_row(e, sprd[k], fmt, info))
        model["objects"] = rows

    @reader(b"SPRD")
    def _sprd():
        if b"SPR " not in records:
            raise DataError("SPRD without SPR")

    @reader(b"PLAY")
    def _play():
        meta["players"] = list(struct.unpack("<4i", single(b"PLAY", 16)))

    @reader(b"GROU")
    def _grou():
        r = single(b"GROU")
        if len(r) % 4:
            raise DataError("GROU size")
        values = struct.unpack(f"<{len(r) // 4}i", r)
        groups, current = [], []
        for v in values[:-1]:
            if v == END:
                groups.append(current)
                current = []
            else:
                current.append(v)
        if values[-1] != END or current:
            raise DataError("GROU end marker")
        model["groups"] = groups

    readable = dict(readers)
    for cid in ids:
        fn = readable.get(cid)
        try:
            if fn is None:
                raise DataError("unknown chunk")
            if cid == b"GRID" and "head" not in meta:
                raise DataError("GRID before HEAD")
            if cid == b"SPR " and "head" not in meta:
                raise DataError("SPR before HEAD")
            fn()
        except (DataError, struct.error, ValueError, UnicodeDecodeError):
            raw[chunk_name(cid)] = [r.hex() for r in records[cid]]
            if cid in (b"SPR ", b"SPRD"):      # read together: both raw or none
                for other in (b"SPR ", b"SPRD"):
                    if other in records:
                        raw[chunk_name(other)] = [r.hex() for r in records[other]]
                model.pop("objects", None)
    if raw:
        meta["raw"] = raw
    return model


def grid_size(meta):
    head = meta["head"]
    return int(float(head["width"])) // 8, int(float(head["height"])) // 8


def object_row(entry, sprd, fmt, info):
    id_, nvid, x, y, z, direction, army = entry
    if fmt is SPR_CURRENT:
        x, y, z = (float_text(struct.pack("<f", v)) for v in (x, y, z))
    row = {"id": str(id_), "nvid": str(nvid), "name": info.get(nvid, ("", ""))[0],
           "x": str(x), "y": str(y), "z": str(z), "direction": str(direction),
           "army": str(army)}
    rid, n = struct.unpack_from("<ii", sprd, 0)
    if rid != id_:
        raise DataError(f"SPRD record for {rid} where {id_} was expected")
    if not 0 <= n <= (len(sprd) - 8) // 16:
        raise DataError(f"object {id_}: command count {n}")
    cmds = [struct.unpack_from("<4i", sprd, 8 + 16 * k) for k in range(n)]
    row["commands"] = " ".join(",".join(str(v) for v in c) for c in cmds)
    rest = sprd[8 + 16 * n:]
    row["behave"] = row["items"] = ""
    if info.get(nvid, ("", ""))[1] in UNIT_CLASSES and len(rest) >= 8:
        behave, count = struct.unpack_from("<ii", rest, 0)
        if 0 <= count and 8 + 2 * count <= len(rest):
            row["behave"] = str(behave)
            row["items"] = ints_text(struct.unpack_from(f"<{count}h", rest, 8))
            rest = rest[8 + 2 * count:]
    row["data"] = rest.hex()
    return row


def object_entry(row, fmt, path="objects.tsv"):
    """(SPR entry bytes, SPRD record bytes) of an objects row."""
    ints = [cell(path, row, c, parse_int) for c in ("id", "nvid", "direction", "army")]
    try:
        if fmt is SPR_CURRENT:      # floats as their exact bits, also for "0x..." texts
            entry = struct.pack("<ii", *ints[:2]) + b"".join(
                cell(path, row, c, float_bytes) for c in ("x", "y", "z")) + struct.pack("<ii", *ints[2:])
        else:
            entry = fmt.pack(*ints[:2], *(cell(path, row, c, parse_int) for c in ("x", "y", "z")), *ints[2:])
    except struct.error as e:
        raise DataError(f"{path}: id {row['id']}: {e}") from None

    def commands(text):
        cmds = [[parse_int(v) for v in c.split(",")] for c in text.split()]
        if any(len(c) != 4 for c in cmds):
            raise ValueError("a command is act,var1,var2,var3")
        return struct.pack("<i", len(cmds)) + b"".join(struct.pack("<4i", *c) for c in cmds)

    sprd = struct.pack("<i", ints[0]) + cell(path, row, "commands", commands)
    if row["behave"].strip():
        items = cell(path, row, "items", text_ints)
        sprd += struct.pack("<ii", cell(path, row, "behave", parse_int), len(items))
        sprd += struct.pack(f"<{len(items)}h", *items)
    elif row["items"].strip():
        raise DataError(f"{path}: id {row['id']}: items without behave")
    sprd += cell(path, row, "data", bytes.fromhex)
    return entry, sprd


def write_map(model):
    meta = model["info"]
    raw = meta.get("raw", {})
    ids = [chunk_id(c) for c in meta["chunks"]]
    records = {}
    for cid in ids:
        name = chunk_name(cid)
        if name in raw:
            records[cid] = [bytes.fromhex(r) for r in raw[name]]
        elif cid == b"OBJ ":
            anims = {}
            for r in model["vid_animations"]:
                anims.setdefault(r["nvid"].strip(), []).append(r)
            try:
                records[cid] = [res_tool.object_record(r, anims.get(r["nvid"].strip(), []))
                                for r in model["vids"]]
            except (ValueError, KeyError) as e:
                raise DataError(f"vids.tsv: {e}") from None
        elif cid == b"GRPH":
            g = meta["graphics"]
            gamma = bytes.fromhex(g["gamma"])
            if len(gamma) != 8:
                raise DataError("map.json: graphics.gamma is 8 bytes in hex")
            records[cid] = [struct.pack("<I", parse_int(str(g["environment"]))) + gamma +
                            struct.pack("<i", g["windDirection"]) + float_bytes(g["wind"])]
        elif cid == b"HEAD":
            h = meta["head"]
            if model["current"]:
                if h["version"] >= 10:
                    four = b"".join(float_bytes(h[k]) for k in ("width", "height", "cameraX", "cameraY"))
                else:
                    four = struct.pack("<4i", *(int(h[k]) for k in ("width", "height", "cameraX", "cameraY")))
                records[cid] = [four + struct.pack("<Ii", h["time"], h["version"])]
            else:
                records[cid] = [struct.pack("<iihhIi", h["width"], h["height"], h["cameraX"],
                                            h["cameraY"], h["time"], h["version"]) +
                                bytes.fromhex(h.get("rest", ""))]
        elif cid == b"GRID":
            w, h = grid_size(meta)
            grid = model["grid"]
            if len(grid) != h or any(len(r) != w for r in grid):
                raise DataError(f"grid.txt: needs {h} rows of {w} values (map size / 8)")
            records[cid] = [b"".join(struct.pack(f"<{w}h", *r) for r in grid)]
        elif cid == b"SPR ":
            fmt = spr_format(model)
            entries, sprd = [], []
            seen = set()
            for row in model["objects"]:
                e, s = object_entry(row, fmt)
                if row["id"].strip() in seen:
                    raise DataError(f"objects.tsv: id {row['id']} twice")
                seen.add(row["id"].strip())
                entries.append(e)
                sprd.append(s)
            records[cid] = [b"".join(entries) + struct.pack("<i", END)]
            records[b"SPRD"] = sprd + [struct.pack("<i", END)]
        elif cid == b"SPRD":
            if b"SPR " not in ids:
                raise DataError("SPRD without SPR")
        elif cid == b"PLAY":
            records[cid] = [struct.pack("<4i", *meta["players"])]
        elif cid == b"GROU":
            body = b"".join(struct.pack(f"<{len(g) + 1}i", *g, END) for g in model["groups"])
            records[cid] = [body + struct.pack("<i", END)]
        else:
            raise DataError(f"map.json: chunk {name} has no data (not in raw)")
    return write_container(b"MAP ", ids, records)


def save_map(model, folder):
    os.makedirs(folder, exist_ok=True)
    save_json(os.path.join(folder, "map.json"), model["info"])
    if "objects" in model:
        save_tsv(os.path.join(folder, "objects.tsv"), OBJECT_COLUMNS, model["objects"])
    if "vids" in model:
        save_tsv(os.path.join(folder, "vids.tsv"), VID_COLUMNS, model["vids"])
        save_tsv(os.path.join(folder, "vid_animations.tsv"), VID_ANIM_COLUMNS, model["vid_animations"])
    if "grid" in model:
        with open(os.path.join(folder, "grid.txt"), "w", newline="\n") as f:
            f.writelines(ints_text(r) + "\n" for r in model["grid"])
    if "groups" in model:
        with open(os.path.join(folder, "groups.txt"), "w", newline="\n") as f:
            f.writelines(ints_text(g) + "\n" for g in model["groups"])


def load_table(path, columns):
    cols, rows = load_tsv(path)
    missing = [c for c in columns if c not in cols]
    if missing:
        raise DataError(f"{path}: missing columns {', '.join(missing)}")
    return [r for _, r in rows]


def load_lines(path):
    """Lines of numbers; blank lines and # comments are skipped."""
    out = []
    with open(path, encoding="utf-8-sig") as f:
        for n, line in enumerate(f, 1):
            if line.strip() and not line.startswith("#"):
                try:
                    out.append(text_ints(line))
                except ValueError:
                    raise DataError(f"{path}:{n}: not a list of numbers") from None
    return out


def load_map(folder):
    meta = load_json(os.path.join(folder, "map.json"))
    model = {"info": meta, "current": "GRPH" in meta["chunks"]}
    raw = meta.get("raw", {})
    present = [c for c in meta["chunks"] if c not in raw]
    if "SPR " in present:
        model["objects"] = load_table(os.path.join(folder, "objects.tsv"), OBJECT_COLUMNS)
    if "OBJ " in present:
        model["vids"] = load_table(os.path.join(folder, "vids.tsv"), VID_COLUMNS)
        model["vid_animations"] = load_table(os.path.join(folder, "vid_animations.tsv"), VID_ANIM_COLUMNS)
    if "GRID" in present:
        model["grid"] = load_lines(os.path.join(folder, "grid.txt"))
    if "GROU" in present:
        model["groups"] = load_lines(os.path.join(folder, "groups.txt"))
    return model


def unpack_map(path, folder, objects_res=None):
    data = open(path, "rb").read()
    model = read_map(data, object_info(objects_res or find_objects_res(path)))
    if write_map(model) != data:
        raise DataError(f"{path} would not pack back to the same bytes")
    save_map(model, folder)
    return model


# The runner of the arrivals: the block "// <editor arrivals>" of a mission script, as the map
# editor writes it (ARRIVALS_RUNNER of src/LocoFix/editor_mission.cpp - keep the two the same).
ARRIVALS_RUNNER = r"""// ArrivalsTact() brings the trains of the arrays arrival_*[] (the settings' block above), once a
// second: a wave comes when its time is there (the first after arrival_first s, the next ones
// arrival_every s after the one before, arrival_times of them or without end) and its condition
// holds - or, with arrival_or, also earlier when the condition holds; a train the player pays for
// comes only while the money is there and is paid as it starts to come. It waits while a train
// stands at its entry (180 px) or the AI has its 16 trains. The cars are made on the entry's rail
// point in the army, one at a time (ArrivalTry); the enemy's train goes to the AI (TrainsTact) with
// its task, the others' last car gets the order to drive to arrival_to (arrival_part, arrival_car:
// the cars made so far of a train under way, its last one). An arrival without its entry (0,0)
// never comes.
// ArrivalsLeft(army): the waves of an army still to come.
ArrivalTrains(int army)
{
  return GetVid(5,VID_COUNT0+army)+GetVid(10,VID_COUNT0+army)+GetVid(97,VID_COUNT0+army);
}
// MissionIf(c,n): the condition c (the editor's list: 1 the player has no train, 2 fewer than n,
// 3 fewer units than n, 4 no factory, 5 a neutral factory stands, 6 the enemy has fewer trains than
// n, 7 fewer of the AI's trains attack than n, 8 the player has n money, 9 1 and 5), 0 always.
MissionIf(int c,int n)
{
  int k,m;
  if( c==1 )
    return ArrivalTrains(0)==0;
  if( c==2 )
    return ArrivalTrains(0)<n;
  if( c==3 )
    return NoArmyUnits(0)<n;
  if( c==4 )
    return GetVid(102,VID_COUNT0)==0;
  if( c==5 )
    return GetVid(102,VID_COUNT2)>0;
  if( c==6 )
    return ArrivalTrains(1)<n;
  if( c==7 )
  {
    m=0;
    for( k=0; k<ARR_LENGTH; k++ )
      if( ai_trains[k] && ai_trains_task[k]/10==1 )
        m++;
    return m<n;
  }
  if( c==8 )
    return GetMoney(0)>=n;
  if( c==9 )
    return ArrivalTrains(0)==0 && GetVid(102,VID_COUNT2)>0;
  if( c==10 )
    return GetVid(5,VID_COUNT0)+GetVid(10,VID_COUNT0)==0 && GetVid(102,VID_COUNT0)>0;
  return 1;
}
ArrivalIf(int i)
{
  return MissionIf(arrival_if[i],arrival_if_n[i]);
}
// MissionCars(u): mission_cars gets the VIDs of the train of u, its first car first.
string mission_cars;
int mission_count=0;
MissionCars(int u)
{
  int c;
  mission_cars="";
  mission_count=0;
  for( c=Action(u,ACT_FIRST_ENGINE); c; c=Action(c,ACT_NEXT_ENGINE) )
  {
    mission_cars=mission_cars+itoa(GetUnitVid(c))+",";
    mission_count++;
  }
}
// MissionTypeCars(type,built): mission_cars gets the VIDs of a train type (arr_gettype, as this
// difficulty makes it); built: in the order a factory builds them (BuildTrain swaps the first two).
MissionTypeCars(int type,int built)
{
  int j;
  arr_gettype(temp_array,type);
  if( built && temp_array[0] && temp_array[1] )
  {
    j=temp_array[0];
    temp_array[0]=temp_array[1];
    temp_array[1]=j;
  }
  mission_cars="";
  for( j=0; j<30 && temp_array[j]; j++ )
    mission_cars=mission_cars+itoa(temp_array[j])+",";
}
// ArrivalMade(u,i) and ArrivalsMade(): an arrival's train just made (its last car u) is logged with
// its cars once they are all linked (10 s at most).
int arrival_made[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int arrival_made_i[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
int arrival_made_w[]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
ArrivalMade(int u,int i)
{
  int j;
  for( j=0; j<16; j++ )
    if( !arrival_made[j] )
    {
      arrival_made[j]=u;
      arrival_made_i[j]=i+1;
      arrival_made_w[j]=0;
      return 1;
    }
  return 0;
}
ArrivalsMade()
{
  int j,k,n;
  for( j=0; j<16; j++ )
    if( arrival_made[j] )
    {
      if( GetUnitVid(arrival_made[j]) )
      {
        MissionCars(arrival_made[j]);
        arr_gettype(temp_array,arrival_type[arrival_made_i[j]-1]);
        n=0;
        for( k=0; k<30 && temp_array[k]; k++ )
          n++;
        arrival_made_w[j]=arrival_made_w[j]+1;
        if( mission_count>=n || arrival_made_w[j]>=10 )
        {
          Log("ARRIVAL "+itoa(arrival_made_i[j])+" made "+mission_cars);
          arrival_made[j]=0;
        }
      }
      else
        arrival_made[j]=0;
    }
}
ArrivalsLeft(int army)
{
  int i,n;
  n=0;
  for( i=0; i<ARRIVALS; i++ )
    if( arrival_army[i]==army && (arrival_x[i] || arrival_y[i]) )
      if( !arrival_times[i] || arrival_done[i]<arrival_times[i] )
        if( !arrival_if[i] || arrival_or[i] || ArrivalIf(i) )
          n++;
  return n;
}
// ArrivalCar(i,j): the car j of arrival i's train on its entry's rail point, in its army.
ArrivalCar(int i,int j)
{
  int u;
  arr_gettype(temp_array,arrival_type[i]);
  u=Create(temp_array[j],arrival_x[i],arrival_y[i]+arrival_z[i],arrival_z[i]);
  Action(u,ACT_SET_ARMY,arrival_army[i],0);
  if( arrival_army[i] )
    Action(u,ACT_SET_BEHAVE,Action(u,ACT_GET_BEHAVE)|1);
  return u;
}
// ArrivalDone(i,t): the train of arrival i is made (its last car arrival_car): its order, its message.
ArrivalDone(int i,int t)
{
  int j,k,u;
  string s;
  u=arrival_car[i];
  arrival_part[i]=0;
  arr_gettype(temp_array,arrival_type[i]);
  s="";
  for( j=0; j<30 && temp_array[j]; j++ )
    s=s+itoa(temp_array[j])+",";
  k=-1;
  if( arrival_army[i]==1 )
    k=find_num();
  if( k>=0 )
  {
    ai_trains[k]=u;
    ai_trains_type[k]=arrival_type[i];
    ai_trains_task[k]=arrival_task[i];
    ai_trains_task2[k]=ATA_NONE;
    ai_trains_task_backup[k]=ATA_NONE;
    ai_trains_behave[k]=0;
    ai_trains_time[k]=GetTime();
    ai_trains_random_task[k]=arrival_raid[i];
  }
  else if( arrival_task[i]==-2 || arrival_task[i]==-3 )
    Action(u,ACT_MOVE,arrival_to_x[i],arrival_to_y[i]+arrival_to_z[i]);
  if( arrival_task[i]==-3 )
  {
    if( arrival_unit[i] )
      PatrolEngine(arrival_unit[i],arrival_to2_x[i],arrival_to2_y[i]);
    arrival_unit[i]=u;
  }
  if( arrival_msg[i] )
  {
    PlaySFX(100);
    if( arrival_army[i]==1 )
      Message("EnemyReinforcementArrived",GetX(u),GetY(u));
    else
      Message("ReinforcementArrived",GetX(u),GetY(u));
  }
  arrival_done[i]=arrival_done[i]+1;
  arrival_last[i]=t;
  s=" cars "+s;
  s="ARRIVAL "+itoa(i+1)+" army "+itoa(arrival_army[i])+" wave "+itoa(arrival_done[i])+" at "+itoa(t)+" s task "+itoa(arrival_task[i])+" slot "+itoa(k)+s;
  Log(s);
  ArrivalMade(u,i);
  return 1;
}
// ArrivalTry(i,t): a wave of arrival i when it is due; its cars one at a time, the next once the
// last one has left the rail point (the entry's push line moves it on; 5 s at most) - cars made on
// one point all at once push each other apart out of their order.
ArrivalTry(int i,int t)
{
  int u,go;
  if( arrival_part[i] )   // a train being made: its next car
  {
    u=arrival_car[i];
    if( GetUnitVid(u) )
      if( SizeTo(u,arrival_x[i],arrival_y[i]+arrival_z[i])<36 && t-arrival_last[i]<5 )
        return 0;
    arr_gettype(temp_array,arrival_type[i]);
    if( arrival_part[i]<30 && temp_array[arrival_part[i]] )
    {
      arrival_car[i]=ArrivalCar(i,arrival_part[i]);
      arrival_part[i]=arrival_part[i]+1;
      arrival_last[i]=t;
      arr_gettype(temp_array,arrival_type[i]);
      if( arrival_part[i]<30 && temp_array[arrival_part[i]] )
        return 0;
    }
    return ArrivalDone(i,t);
  }
  if( !arrival_x[i] && !arrival_y[i] )
    return 0;
  if( arrival_times[i] && arrival_done[i]>=arrival_times[i] )
    return 0;
  if( arrival_done[i] )
    go=t-arrival_last[i]>=arrival_every[i];
  else
    go=t>=arrival_first[i];
  if( arrival_or[i] )
  {
    if( arrival_if[i] && ArrivalIf(i) )
      go=1;
  }
  else if( !ArrivalIf(i) )
    go=0;
  if( !go )
    return 0;
  if( arrival_if[i]==8 && !ArrivalIf(i) )
    return 0;
  if( FindNearestSprite(GETSPRITE_CLASS+B_ENGINE,arrival_x[i],arrival_y[i]+arrival_z[i],180) )
    return 0;
  if( arrival_army[i]==1 )
    if( find_num()<0 )
      return 0;
  arrival_car[i]=ArrivalCar(i,0);
  if( !arrival_car[i] )
    return 0;
  if( arrival_if[i]==8 )   // paid as the train starts to come
    SetMoney(0,GetMoney(0)-arrival_if_n[i]);
  arrival_part[i]=1;
  arrival_last[i]=t;
  arr_gettype(temp_array,arrival_type[i]);
  if( temp_array[1] )
    return 0;
  return ArrivalDone(i,t);
}
// PlanTact(t): the factories' plans (plan_*[], a factory's panel). A wave is due - the first one by
// plan_when (0: plan_first s from the start, 1: plan_first s after the first train of the entry
// plan_prev was built, 2: while MissionIf(plan_if, plan_if_n) holds, not before plan_first s), the
// next ones plan_every s after the one before was built (and while the condition holds), plan_times
// of them or without end. The enemy's (plan_fx, plan_fy 0) goes into the authors' queue
// (level_queue_*, which DepoFree of ai_events.lgc builds from its first place on, in the AI's first
// free factory, while fewer than LIMIT_ATACKERS of its trains attack) at the first empty place, with
// a time already past, and is followed from there to the AI's slot of the train being built
// (ATT_DEPO_CREATED) to the train made; plan_type's cars are the train's with the first two swapped,
// as BuildTrain swaps them back. The others' factory builds its train as PlanBuild says. The cars
// planned and the cars made are logged ("ARRIVAL F<n>").
int plan_queued=0;
PlanTaken(int t)
{
  int i,j,k;
  i=plan_fifo[0];
  for( j=0; j<PLANS-1; j++ )
    plan_fifo[j]=plan_fifo[j+1];
  plan_state[i]=2;
  plan_slot[i]=-1;
  for( k=0; k<ARR_LENGTH; k++ )
    if( ai_trains[k] && ai_trains_type[k]>=ATT_DEPO_CREATED )
      plan_slot[i]=k;
  Log("ARRIVAL F"+itoa(i+1)+" wave "+itoa(plan_done[i]+1)+" taken at "+itoa(t)+" s slot "+itoa(plan_slot[i]));
}
PlanFollow(int i,int t)
{
  int k,u;
  k=plan_slot[i];
  if( k>=0 )
    if( ai_trains_type[k]>=ATT_DEPO_CREATED )
      return 0;
  plan_state[i]=0;
  plan_done[i]=plan_done[i]+1;
  plan_last[i]=t;
  if( plan_done[i]==1 )
    plan_since[i]=t;
  u=0;
  if( k>=0 )
    u=ai_trains[k];
  if( u )
    if( GetUnitVid(u)==102 )
      u=0;
  if( u )
  {
    MissionCars(u);
    Log("ARRIVAL F"+itoa(i+1)+" wave "+itoa(plan_done[i])+" built at "+itoa(t)+" s made "+mission_cars);
  }
  else
    Log("ARRIVAL F"+itoa(i+1)+" wave "+itoa(plan_done[i])+" lost at "+itoa(t)+" s: the AI dropped its build");
  return 1;
}
// PlanPrice(v): what a car costs (price_*[], the factory's panel's prices).
PlanPrice(int v)
{
  int j;
  for( j=0; j<PRICES; j++ )
    if( price_vid[j]==v )
      return price_of[j];
  return 0;
}
// A train of ours, the neutrals' or the pirates' factory (plan_fx, plan_fy, while it is plan_army's):
// the runner orders the factory to build it car by car (AddCommand, as the AI's BuildTrain does, in
// the train's order), a car once the factory is free and the army has its price - a car the money
// does not cover would be left out -, then gives the train its order (plan_task -1 none, -2 drive
// to plan_to, -3 drive there, then to and fro to plan_to2).
PlanFactory(int i)
{
  int f;
  f=FindNearestSprite(GETSPRITE_VID+102,plan_fx[i],plan_fy[i],30);
  if( f )
    if( Action(f,act_getarmy,0,0)!=plan_army[i] )
      f=0;
  return f;
}
PlanBuild(int i,int t)
{
  int f,v,u;
  f=plan_slot[i];
  if( GetUnitVid(f)!=102 )
    f=0;
  if( f )
    if( Action(f,act_getarmy,0,0)!=plan_army[i] )
      f=0;
  if( !f )
  {
    plan_state[i]=0;
    plan_done[i]=plan_done[i]+1;
    plan_last[i]=t;
    Log("ARRIVAL F"+itoa(i+1)+" wave "+itoa(plan_done[i])+" lost at "+itoa(t)+" s: its factory is gone or not of army "+itoa(plan_army[i]));
    return 0;
  }
  arr_gettype(temp_array,plan_type[i]);
  if( plan_state[i]==4 )   // all made: logged and sent off once the cars are linked (10 s at most)
  {
    if( t-plan_last[i]<3 )
      return 0;
    u=FindNearestSprite(GETSPRITE_VID+temp_array[plan_next[i]-1],GetX(f),GetY(f),160);
    if( u )
    {
      MissionCars(u);
      if( mission_count<plan_next[i] && t-plan_last[i]<10 )
        return 0;
    }
    plan_state[i]=0;
    plan_done[i]=plan_done[i]+1;
    plan_last[i]=t;
    if( plan_done[i]==1 )
      plan_since[i]=t;
    if( !u )
    {
      Log("ARRIVAL F"+itoa(i+1)+" wave "+itoa(plan_done[i])+" lost at "+itoa(t)+" s: no train at the factory");
      return 0;
    }
    MissionCars(u);
    Log("ARRIVAL F"+itoa(i+1)+" wave "+itoa(plan_done[i])+" built at "+itoa(t)+" s army "+itoa(plan_army[i])+" made "+mission_cars);
    if( plan_task[i]==-2 || plan_task[i]==-3 )
      Action(u,ACT_MOVE,plan_to_x[i],plan_to_y[i]+plan_to_z[i]);
    if( plan_task[i]==-3 )
      plan_unit[i]=u;
    return 1;
  }
  if( Action(f,ACT_GET_COMMAND)==COM_BUILD )
    return 0;
  if( strlen(GetCommands(f))>plan_base[i] )
    return 0;
  v=temp_array[plan_next[i]];
  if( !v )
  {
    plan_state[i]=4;
    plan_last[i]=t;
    return 0;
  }
  if( GetMoney(plan_army[i])<PlanPrice(v) )
    return 0;
  AddCommand(f,ACT_BUILD_UNIT,v);
  plan_next[i]=plan_next[i]+1;
  return 0;
}
// PlanOnPoint(i): a factory's train of a patrol (-3) that reached its point patrols from there.
PlanOnPoint(int i)
{
  if( plan_task[i]!=-3 || !plan_unit[i] )
    return 0;
  if( !GetUnitVid(plan_unit[i]) )
  {
    plan_unit[i]=0;
    return 0;
  }
  if( SizeTo(plan_unit[i],plan_to_x[i],plan_to_y[i]+plan_to_z[i])>=40 )
    return 0;
  PatrolEngine(plan_unit[i],plan_to2_x[i],plan_to2_y[i]);
  plan_unit[i]=0;
  return 1;
}
PlanTry(int i,int t)
{
  int go,n,f;
  if( plan_times[i] && plan_done[i]>=plan_times[i] )
    return 0;
  if( plan_done[i] )
    go=t-plan_last[i]>=plan_every[i];
  else if( plan_when[i]==1 && plan_prev[i]>=0 )
  {
    go=0;
    if( plan_done[plan_prev[i]] )
      go=t-plan_since[plan_prev[i]]>=plan_first[i];
  }
  else
    go=t>=plan_first[i];
  if( plan_when[i]==2 && !MissionIf(plan_if[i],plan_if_n[i]) )
    go=0;
  if( !go )
    return 0;
  if( plan_fx[i] || plan_fy[i] )   // ours, the neutrals' or the pirates': when its factory is free
  {
    f=PlanFactory(i);
    if( !f )
      return 0;
    if( Action(f,ACT_GET_COMMAND)==COM_BUILD )
      return 0;
    for( n=0; n<PLANS; n++ )   // one entry at a time in a factory
      if( plan_state[n]>=3 && plan_slot[n]==f )
        return 0;
    if( FindNearestSprite(GETSPRITE_CLASS+B_ENGINE,GetX(f),GetY(f),250) )   // a train at its door or on its spur: the cars would join it
      return 0;
    plan_slot[i]=f;
    plan_base[i]=strlen(GetCommands(f));
    plan_next[i]=0;
    plan_state[i]=3;
    MissionTypeCars(plan_type[i],0);
    Log("ARRIVAL F"+itoa(i+1)+" wave "+itoa(plan_done[i]+1)+" ordered at "+itoa(t)+" s army "+itoa(plan_army[i])+" task "+itoa(plan_task[i])+" cars "+mission_cars);
    return 1;
  }
  n=plan_queued;
  level_queue_times[n]=1;
  level_queue_trains[n]=plan_type[i];
  level_queue_task[n]=plan_task[i];
  plan_fifo[n]=i;
  plan_queued=n+1;
  plan_state[i]=1;
  MissionTypeCars(plan_type[i],1);
  Log("ARRIVAL F"+itoa(i+1)+" wave "+itoa(plan_done[i]+1)+" queued at "+itoa(t)+" s place "+itoa(n)+" type "+itoa(plan_type[i])+" task "+itoa(plan_task[i])+" cars "+mission_cars);
  return 1;
}
PlanTact(int t)
{
  int i,j,m,n;
  n=0;
  for( j=0; j<PLANS; j++ )
    if( level_queue_times[j] )
      n=j+1;
  for( m=n; m<plan_queued; m++ )
    PlanTaken(t);
  if( plan_queued>n )
    plan_queued=n;
  for( i=0; i<PLANS; i++ )
  {
    PlanOnPoint(i);
    if( plan_state[i]==2 )
      PlanFollow(i,t);
    else if( plan_state[i]>=3 )
      PlanBuild(i,t);
    else if( !plan_state[i] )
      PlanTry(i,t);
  }
}
// FactoryTact(t): the unit limits at the start (AddUnitLimit takes the limit first and the army
// last), the units the factories get into their lists later (unlock_*[], the factory by its place).
FactoryTact(int t)
{
  int i,u;
  iff(1)
  {
    for( i=0; i<UNIT_LIMITS; i++ )
      AddUnitLimit(limit_max[i],limit_vid[i],limit_army[i]);
  }
  for( i=0; i<UNLOCKS; i++ )
    if( unlock_time[i]>=0 && t>=unlock_time[i] )
    {
      u=FindNearestSprite(GETSPRITE_VID+102,unlock_x[i],unlock_y[i],30);
      if( u )
        Action(u,ACT_ADD_ITEM,unlock_vid[i]);
      Log("UNLOCK vid "+itoa(unlock_vid[i])+" at "+itoa(t)+" s, factory "+itoa(u!=0));
      unlock_time[i]=-1;
    }
}
// CaptureTact(): a neutral factory or repair point becomes the player's when a train of the player's
// comes within 100 px of it (the missions call LDepoCapture / LMasterCapture of Captures.lgc for
// their one such building; forts and mining plants are captured by F162_0 of ai_default_event.lgc).
CaptureTact()
{
  int b,v;
  for( b=FirstUnit(u_monster); b; b=NextUnit() )
  {
    v=GetUnitVid(b);
    if( (v==102 || v==104) && Action(b,act_getarmy,0,0)==2 )
      if( FindNearestSprite(GETSPRITE_UNIT+GETSPRITE_CLASS+B_ENGINE+GETSPRITE_ARMY0,GetX(b),GetY(b),100) )
      {
        PlaySFX(25);
        Effect(eff_nuke,0x006000);
        Action(b,act_setarmy,0,0);
        BuildingCapture(b);
        Log("CAPTURE vid "+itoa(v)+" at "+itoa(GetX(b))+","+itoa(GetY(b)));
      }
  }
}
// OrdersTact(): the orders of the map's trains (order_*[]), once at the start: each train found by its
// first car's VID and place, then driven or patrolling (-2, -3), the enemy's given to the AI with its task.
OrdersTact()
{
  int i,u,k;
  for( i=0; i<ORDERS; i++ )
  {
    u=FindNearestSprite(GETSPRITE_VID+order_vid[i],order_x[i],order_y[i],20);
    k=-1;
    if( u && order_task[i]==-2 )
      Action(u,ACT_MOVE,order_to_x[i],order_to_y[i]+order_to_z[i]);
    if( u && order_task[i]==-3 )
      PatrolEngine(u,order_to_x[i],order_to_y[i]);
    if( u && order_task[i]>=0 )
    {
      k=find_num();
      if( k>=0 )
      {
        ai_trains[k]=u;
        ai_trains_type[k]=order_type[i];
        ai_trains_task[k]=order_task[i];
        ai_trains_task2[k]=ATA_NONE;
        ai_trains_task_backup[k]=ATA_NONE;
        ai_trains_behave[k]=0;
        ai_trains_time[k]=GetTime();
        ai_trains_random_task[k]=0;
      }
    }
    Log("ORDER "+itoa(i+1)+" task "+itoa(order_task[i])+" train "+itoa(u!=0)+" slot "+itoa(k));
  }
}
// ArrivalOnPoint(i): a train of a patrol (-3) that reached its point patrols to the other end from there.
ArrivalOnPoint(int i)
{
  if( arrival_task[i]!=-3 || !arrival_unit[i] )
    return 0;
  if( !GetUnitVid(arrival_unit[i]) )
  {
    arrival_unit[i]=0;
    return 0;
  }
  if( SizeTo(arrival_unit[i],arrival_to_x[i],arrival_to_y[i]+arrival_to_z[i])>=40 )
    return 0;
  PatrolEngine(arrival_unit[i],arrival_to2_x[i],arrival_to2_y[i]);
  arrival_unit[i]=0;
  return 1;
}
// PiecesTact(), every frame: the wholes the editor places work as in the missions. A citadel (VID
// 149 with its tower 117) as l04 and l18 (pirates.lgc GratesTact): while its tower stands, the first
// unit of another army than the tower's (the neutrals aside) within 100 px of it gets ACT_STOP and
// 300 damage (SFX 45), its grates (146) rise 34 px in 1 s, stay 1 s and sink in 1 s; when the
// tower is gone the grates stay down and its corkscrews (142) break (147). Charybdis (254 with its
// eye 256) as l06: a unit of another army within 90 px is pulled in (ACT_MOVE to it) while its
// teeth (255) chew (SFX 38), and one of the player's within 40 px is eaten - the enemy's, for a
// Charybdis of the player's; its guns (258, 260) sleep until one of them or the eye is down to 80 %
// or lost. A fly nest (265) as l06's two (its direction: 0 waves, 64 on
// guard): green flies (263) in waves of 2-4 (2 for every AA gun of the map, less up to as many)
// 25 s after the last of them is gone, or grey flies (262) on guard: 1-2 while a unit of another
// army (not a factory or repair point) is within 400 px and none is left, 3 s after, and 6-9 when
// the nest is destroyed; the flies are of the nest's army. When the pirates' towers (117) of the map
// are all gone, every corkscrew (142) breaks, as ReplaceUnit in l04 and l18. A crusher (two stands
// 144 and two drums 143 across the track; the first stand at +S from its middle, turned 64 * its
// form) as l04 and l18's (pirates.lgc Pirate_Crasher): every 25 s the crushers bite together - the
// drums close half way in 0.3 s (a unit of another army than the crusher's, the neutrals aside,
// within 20 px is stopped), chew 2 s (SFX 33) and open in 0.3 s, 200 damage a frame to those units
// meanwhile -, but not while a unit of a crusher's army is within 60 px of it; when the map had towers
// of its army and none is left, its drums die (139) and it stops. Scylla (252 with its jaws 253) as
// l06's (scillas.lgc): every 15 s the Scyllas bite together - the jaws close in 0.3 s (every ground
// unit within 50 px is stopped), chew 2 s (an earthquake, SFX 57) and open in 0.3 s, 200 damage a
// frame to every ground unit within 50 px meanwhile; a neutral Scylla (army 2, l06's) bites every
// army, one of another army spares its own.
int pc_n=-1,pc_x[8],pc_y[8],pc_z[8],pc_army[8],pc_g1[8],pc_g2[8],pc_mode[8],pc_time[8];
int ch_n=-1,ch_x[4],ch_y[4],ch_army[4],ch_teeth[4],ch_guns[4],ch_peace[4],ch_ate[4],ch_time[4];
int fn_n=-1,fn_x[8],fn_y[8],fn_army[8],fn_guard[8],fn_left[8],fn_vid[8],fn_time[8],fn_dead[8],fn_check=0;
int pc_towers=0;
int cr_n=-1,cr_x[16],cr_y[16],cr_z[16],cr_army[16],cr_d1[16],cr_d2[16],cr_cx[16],cr_cy[16],cr_tower[16];
int cr_mode=0,cr_time=0,cr_bite=0,cr_check=0;
int cr_sx[]={0,-22,-33,-22};
int cr_sy[]={22,15,0,-15};
int cr_dx[]={0,-30,-45,-30};
int cr_dy[]={30,20,0,-20};
int sc_n=-1,sc_x[8],sc_y[8],sc_z[8],sc_army[8],sc_j1[8],sc_j2[8],sc_cx[8],sc_cy[8],sc_mode=0,sc_time=0,sc_bite=0;
PieceFoe(int u,int army)
{
  int a;
  a=Action(u,ACT_GET_ARMY);
  return a!=army && a!=2;
}
PiecesGrates(int i,int z)
{
  if( pc_g1[i] )
    Action(pc_g1[i],ACT_CHANGE_COOR,GetX(pc_g1[i]),GetY(pc_g1[i]),z);
  if( pc_g2[i] )
    Action(pc_g2[i],ACT_CHANGE_COOR,GetX(pc_g2[i]),GetY(pc_g2[i]),z);
}
CitadelTact()
{
  int i,k,u,c,t;
  if( pc_n<0 )
  {
    pc_n=0;
    pc_towers=GetVid(117,VID_COUNT);
    for( u=FirstUnit(u_monster); u; u=NextUnit() )
      if( GetUnitVid(u)==117 && pc_n<8 )
      {
        c=FindNearestSprite(GETSPRITE_VID+149,GetX(u),GetY(u),20);
        if( c )
        {
          pc_x[pc_n]=GetX(c);
          pc_y[pc_n]=GetY(c);
          pc_z[pc_n]=GetZ(c);
          pc_army[pc_n]=Action(u,ACT_GET_ARMY);
          pc_g1[pc_n]=FindNearestSprite(GETSPRITE_VID+146,pc_x[pc_n]-34,pc_y[pc_n]+23,15);
          pc_g2[pc_n]=FindNearestSprite(GETSPRITE_VID+146,pc_x[pc_n]+36,pc_y[pc_n]+25,15);
          pc_mode[pc_n]=0;
          pc_time[pc_n]=GetTime();
          Log("CITADEL "+itoa(pc_n+1)+" at "+itoa(pc_x[pc_n])+","+itoa(pc_y[pc_n])+" army "+itoa(pc_army[pc_n])+" grates "+itoa((pc_g1[pc_n]!=0)+(pc_g2[pc_n]!=0)));
          pc_n++;
        }
      }
  }
  t=GetTime();
  if( pc_towers>0 && !GetVid(117,VID_COUNT) )
  {
    ReplaceUnit(142,147);
    pc_towers=0;
    Log("CITADEL all towers down: the corkscrews break");
  }
  for( i=0; i<pc_n; i++ )
  {
    if( pc_mode[i]>=0 && !FindNearestSprite(GETSPRITE_VID+117,pc_x[i],pc_y[i],20) )
    {
      PiecesGrates(i,pc_z[i]-33);
      for( k=0; k<4; k++ )
      {
        c=FindNearestSprite(GETSPRITE_VID+142,pc_x[i],pc_y[i],90);
        if( c )
        {
          CreateSprite(147,GetX(c),GetY(c),GetZ(c));
          Destroy(c);
        }
      }
      pc_mode[i]=-1;
      Log("CITADEL "+itoa(i+1)+" tower down");
    }
    if( pc_mode[i]==0 && t-pc_time[i]>500 )
    {
      pc_time[i]=t;
      for( u=FirstUnit(u_monster); u; u=NextUnit() )
        if( SizeTo(u,pc_x[i],pc_y[i])<100 && PieceFoe(u,pc_army[i]) )
        {
          Action(u,ACT_STOP,1);
          Action(u,ACT_DAMAGE,300);
          PlaySFX(45);
          pc_mode[i]=1;
          Log("CITADEL "+itoa(i+1)+" gate hits vid "+itoa(GetUnitVid(u))+" army "+itoa(Action(u,ACT_GET_ARMY)));
          break;
        }
    }
    else if( pc_mode[i]==1 )
    {
      if( t-pc_time[i]>1000 )
      {
        pc_mode[i]=2;
        pc_time[i]=t;
      }
      else
        PiecesGrates(i,pc_z[i]-33+34*(t-pc_time[i])/1000);
    }
    else if( pc_mode[i]==2 )
    {
      if( t-pc_time[i]>1000 )
      {
        pc_mode[i]=3;
        pc_time[i]=t;
        PlaySFX(45);
      }
    }
    else if( pc_mode[i]==3 )
    {
      if( t-pc_time[i]>1000 )
      {
        PiecesGrates(i,pc_z[i]-33);
        pc_mode[i]=0;
        pc_time[i]=t;
      }
      else
        PiecesGrates(i,pc_z[i]+1-34*(t-pc_time[i])/1000);
    }
  }
}
CharybdisGuns(int i,int on)
{
  int u,v;
  for( u=FirstUnit(u_monster); u; u=NextUnit() )
  {
    v=GetUnitVid(u);
    if( (v==258 || v==260) && SizeTo(u,ch_x[i],ch_y[i])<90 )
    {
      if( on )
        Action(u,ACT_SET_BEHAVE,Action(u,ACT_GET_BEHAVE)|1);
      else
        Action(u,ACT_SET_BEHAVE,Action(u,ACT_GET_BEHAVE)&(~1));
    }
  }
}
CharybdisTact()
{
  int i,u,v,b,n,found,t,prey;
  if( ch_n<0 )
  {
    ch_n=0;
    for( u=FirstUnit(u_monster); u; u=NextUnit() )
      if( GetUnitVid(u)==256 && ch_n<4 )
      {
        b=FindNearestSprite(GETSPRITE_VID+254,GetX(u)+34,GetY(u)-22,20);
        if( b )
        {
          ch_x[ch_n]=GetX(b);
          ch_y[ch_n]=GetY(b);
          ch_army[ch_n]=Action(u,ACT_GET_ARMY);
          ch_teeth[ch_n]=FindNearestSprite(GETSPRITE_VID+255,ch_x[ch_n]-40,ch_y[ch_n]+27,15);
          ch_peace[ch_n]=1;
          ch_ate[ch_n]=0;
          ch_time[ch_n]=GetTime();
          ch_n++;
        }
      }
    for( i=0; i<ch_n; i++ )
    {
      n=0;
      for( u=FirstUnit(u_monster); u; u=NextUnit() )
        if( (GetUnitVid(u)==258 || GetUnitVid(u)==260) && SizeTo(u,ch_x[i],ch_y[i])<90 )
          n++;
      ch_guns[i]=n;
      CharybdisGuns(i,0);
      Log("CHARYBDIS "+itoa(i+1)+" at "+itoa(ch_x[i])+","+itoa(ch_y[i])+" army "+itoa(ch_army[i])+" guns "+itoa(n)+" teeth "+itoa(ch_teeth[i]!=0));
    }
  }
  t=GetTime();
  for( i=0; i<ch_n; i++ )
    if( ch_army[i]>=0 && t-ch_time[i]>500 )
    {
      ch_time[i]=t;
      if( !FindNearestSprite(GETSPRITE_VID+256,ch_x[i]-34,ch_y[i]+22,20) )   // the eye is dead
      {
        CharybdisGuns(i,1);
        ch_army[i]=-1;
        Log("CHARYBDIS "+itoa(i+1)+" eye down");
      }
      else
      {
        if( ch_peace[i] )
        {
          n=0;
          for( u=FirstUnit(u_monster); u; u=NextUnit() )
          {
            v=GetUnitVid(u);
            if( (v==258 || v==260 || v==256) && SizeTo(u,ch_x[i],ch_y[i])<90 )
            {
              if( v!=256 )
                n++;
              if( Action(u,ACT_GET_PERCENT_HP)<=80 )
                ch_peace[i]=0;
            }
          }
          if( n<ch_guns[i] )
            ch_peace[i]=0;
          if( !ch_peace[i] )
          {
            CharybdisGuns(i,1);
            Log("CHARYBDIS "+itoa(i+1)+" wakes");
          }
        }
        found=0;
        prey=0;
        if( ch_army[i]==0 )
          prey=1;
        for( u=FirstUnit(u_monster); u; u=NextUnit() )
          if( SizeTo(u,ch_x[i],ch_y[i])<90 && Action(u,ACT_GET_ARMY)!=ch_army[i] )
          {
            found=1;
            Action(u,ACT_MOVE,ch_x[i],ch_y[i]);
            if( SizeTo(u,ch_x[i],ch_y[i])<40 && Action(u,ACT_GET_ARMY)==prey )
            {
              Log("CHARYBDIS "+itoa(i+1)+" eats vid "+itoa(GetUnitVid(u)));
              Destroy(u);
              ch_ate[i]=1;
            }
          }
        if( found && ch_teeth[i] )
        {
          Action(ch_teeth[i],ACT_CLEAR_STACK,0,0);
          AddCommand(ch_teeth[i],act_open,0,0);
          AddCommand(ch_teeth[i],act_stand_open,0,0);
          AddCommand(ch_teeth[i],act_close,0,0);
          AddCommand(ch_teeth[i],act_stand,0,0);
          AddCommand(ch_teeth[i],ACT_CYCLE_STACK,4,0);
          PlaySFX(38);
        }
        if( !found && ch_ate[i] && ch_teeth[i] )
        {
          Action(ch_teeth[i],ACT_CLEAR_STACK,0,0);
          AddCommand(ch_teeth[i],act_close,0,0);
          AddCommand(ch_teeth[i],act_stand,0,0);
          AddCommand(ch_teeth[i],act_salut,0,0);
          ch_ate[i]=0;
        }
      }
    }
}
NestTact()
{
  int i,u,v,n,x,y,z,f,t;
  if( fn_n<0 )
  {
    fn_n=0;
    for( u=FirstUnit(u_monster); u; u=NextUnit() )
      if( GetUnitVid(u)==265 && fn_n<8 )
      {
        fn_x[fn_n]=GetX(u);
        fn_y[fn_n]=GetY(u);
        fn_army[fn_n]=Action(u,ACT_GET_ARMY);
        fn_guard[fn_n]=GetDirection(u)>=64;
        fn_vid[fn_n]=263-fn_guard[fn_n];
        fn_left[fn_n]=0;
        fn_dead[fn_n]=0;
        Log("NEST "+itoa(fn_n+1)+" at "+itoa(fn_x[fn_n])+","+itoa(fn_y[fn_n])+" army "+itoa(fn_army[fn_n])+" guard "+itoa(fn_guard[fn_n]));
        fn_n++;
      }
  }
  if( !OneTimeInSecond() )
    return 0;
  t=GetTime();
  for( i=0; i<fn_n; i++ )
  {
    if( !fn_dead[i] && !FindNearestSprite(GETSPRITE_VID+265,fn_x[i],fn_y[i],10) )   // destroyed
    {
      fn_dead[i]=1;
      fn_left[i]=0;
      if( fn_guard[i] )
      {
        fn_left[i]=6+Random(3);
        fn_time[i]=t-22000;
      }
      Log("NEST "+itoa(i+1)+" destroyed, last flies "+itoa(fn_left[i]));
    }
    if( !fn_dead[i] && fn_left[i]==0 && GetVid(fn_vid[i],VID_COUNT)==0 )
    {
      if( !fn_guard[i] )
      {
        n=0;
        for( u=FirstUnit(u_monster); u; u=NextUnit() )
          if( GetUnitVid(u)==30 )
            n++;
        fn_left[i]=n*2-Random(n);
        if( fn_left[i]<2 )
          fn_left[i]=2;
        if( fn_left[i]>4 )
          fn_left[i]=4;
        fn_time[i]=t;
      }
      else
      {
        f=0;
        for( u=FirstUnit(u_monster); u; u=NextUnit() )
        {
          v=GetUnitVid(u);
          if( v!=102 && v!=104 && SizeTo(u,fn_x[i],fn_y[i])<400 && PieceFoe(u,fn_army[i]) )
            f=1;
        }
        if( f )
        {
          fn_left[i]=1+Random(1);
          fn_time[i]=t-22000;
        }
      }
    }
    if( fn_left[i]>0 && t-fn_time[i]>25000 )
    {
      x=fn_x[i]+5-Random(10);
      y=fn_y[i]+5-Random(10);
      z=120+5-Random(10);
      if( !AskPlace(fn_vid[i],x,y,z) )
      {
        u=Create(fn_vid[i],x,y,z);
        Action(u,act_changeaction,0,Random(255));
        Action(u,act_setarmy,fn_army[i],0);
        fn_left[i]--;
        Log("NEST "+itoa(i+1)+" fly vid "+itoa(fn_vid[i])+", "+itoa(fn_left[i])+" to come");
      }
    }
  }
  return 1;
}
CrusherDrums(int i,int part)
{
  if( cr_d1[i] )
    Action(cr_d1[i],ACT_CHANGE_COOR,cr_x[i]+cr_cx[i]*part/1000,cr_y[i]+cr_cy[i]*part/1000,cr_z[i]);
  if( cr_d2[i] )
    Action(cr_d2[i],ACT_CHANGE_COOR,cr_x[i]-cr_cx[i]*part/1000,cr_y[i]-cr_cy[i]*part/1000,cr_z[i]);
}
CrusherHits(int i,int damage)
{
  int u;
  if( !cr_d1[i] || !cr_d2[i] )
    return 0;
  for( u=FirstUnit(u_monster); u; u=NextUnit() )
    if( SizeTo(u,cr_x[i],cr_y[i])<20 && PieceFoe(u,cr_army[i]) )
    {
      if( damage )
        Action(u,ACT_DAMAGE,damage);
      else
        Action(u,ACT_STOP,1);
    }
  return 1;
}
CrusherTact()
{
  int i,k,s,f,u,t,halt;
  if( cr_n<0 )
  {
    cr_n=0;
    k=0;
    for( s=FirstSprite(); s; s=NextSprite() )
      if( GetUnitVid(s)==144 && k<16 )
      {
        cr_d1[k]=s;
        k++;
      }
    for( i=0; i<k; i++ )
    {
      s=cr_d1[i];
      f=(GetDirection(s)/64)%4;
      cr_x[cr_n]=GetX(s)-cr_sx[f];
      cr_y[cr_n]=GetY(s)-cr_sy[f];
      cr_z[cr_n]=GetZ(s);
      cr_cx[cr_n]=cr_dx[f];
      cr_cy[cr_n]=cr_dy[f];
      cr_army[cr_n]=Action(s,ACT_GET_ARMY);
      if( FindNearestSprite(GETSPRITE_VID+144,cr_x[cr_n]-cr_sx[f],cr_y[cr_n]-cr_sy[f],8) )
      {
        cr_d2[cr_n]=FindNearestSprite(GETSPRITE_VID+143,cr_x[cr_n]-cr_cx[cr_n],cr_y[cr_n]-cr_cy[cr_n],10);
        cr_d1[cr_n]=FindNearestSprite(GETSPRITE_VID+143,cr_x[cr_n]+cr_cx[cr_n],cr_y[cr_n]+cr_cy[cr_n],10);
        cr_tower[cr_n]=FindNearestSprite(GETSPRITE_VID+117+GETSPRITE_ARMY(cr_army[cr_n]),cr_x[cr_n],cr_y[cr_n],100000)!=0;
        Log("CRUSHER "+itoa(cr_n+1)+" at "+itoa(cr_x[cr_n])+","+itoa(cr_y[cr_n])+" form "+itoa(f)+" army "+itoa(cr_army[cr_n])+" drums "+itoa((cr_d1[cr_n]!=0)+(cr_d2[cr_n]!=0)));
        cr_n++;
      }
    }
  }
  if( !cr_n )
    return 0;
  t=GetTime();
  if( t-cr_check>1000 )
  {
    cr_check=t;
    for( i=0; i<cr_n; i++ )
      if( cr_tower[i] && !FindNearestSprite(GETSPRITE_VID+117+GETSPRITE_ARMY(cr_army[i]),cr_x[i],cr_y[i],100000) )
      {
        cr_tower[i]=0;
        CrusherDrums(i,1000);
        if( cr_d1[i] )
        {
          CreateSprite(139,GetX(cr_d1[i]),GetY(cr_d1[i]),GetZ(cr_d1[i]));
          Destroy(cr_d1[i]);
        }
        if( cr_d2[i] )
        {
          CreateSprite(139,GetX(cr_d2[i]),GetY(cr_d2[i]),GetZ(cr_d2[i]));
          Destroy(cr_d2[i]);
        }
        cr_d1[i]=0;
        cr_d2[i]=0;
        Log("CRUSHER "+itoa(i+1)+" stops: no tower of its army left");
      }
  }
  halt=0;
  for( u=FirstUnit(u_monster); u; u=NextUnit() )
    for( i=0; i<cr_n; i++ )
      if( cr_d1[i] && Action(u,ACT_GET_ARMY)==cr_army[i] && SizeTo(u,cr_x[i],cr_y[i])<60 )
        halt=1;
  if( cr_mode==0 )
  {
    if( !halt && t-cr_bite>25000 )
    {
      cr_mode=1;
      cr_bite=t;
      cr_time=t;
    }
  }
  else if( cr_mode==1 )
  {
    if( halt )
    {
      cr_mode=3;
      cr_time=t;
    }
    else if( t-cr_time>300 )
    {
      cr_mode=2;
      cr_time=t;
      PlaySFX(33);
      for( i=0; i<cr_n; i++ )
        if( cr_d1[i] && cr_d2[i] )
        {
          CrusherDrums(i,500);
          Action(cr_d1[i],ANI_SALUT);
          Action(cr_d2[i],ANI_SALUT);
        }
      Log("CRUSHERS bite");
    }
    else
      for( i=0; i<cr_n; i++ )
        if( CrusherHits(i,0) )
          CrusherDrums(i,1000-500*(t-cr_time)/300);
  }
  else if( cr_mode==2 )
  {
    if( halt || t-cr_time>2000 )
    {
      cr_mode=3;
      cr_time=t;
    }
    else
      for( i=0; i<cr_n; i++ )
        CrusherHits(i,200);
  }
  else
  {
    if( t-cr_time>300 )
    {
      cr_mode=0;
      for( i=0; i<cr_n; i++ )
        CrusherDrums(i,1000);
    }
    else
      for( i=0; i<cr_n; i++ )
      {
        if( !halt )
          CrusherHits(i,200);
        CrusherDrums(i,500+500*(t-cr_time)/300);
      }
  }
  return 1;
}
ScyllaJaws(int i,int part)
{
  if( sc_j1[i] )
    Action(sc_j1[i],ACT_CHANGE_COOR,sc_x[i]+sc_cx[i]*part/1000,sc_y[i]+sc_cy[i]*part/1000,sc_z[i]);
  if( sc_j2[i] )
    Action(sc_j2[i],ACT_CHANGE_COOR,sc_x[i]-sc_cx[i]*part/1000,sc_y[i]-sc_cy[i]*part/1000,sc_z[i]);
}
ScyllaHits(int i,int damage)
{
  int u;
  for( u=FirstUnit(u_monster); u; u=NextUnit() )
    if( SizeTo(u,sc_x[i],sc_y[i])<50 && (sc_army[i]==2 || Action(u,ACT_GET_ARMY)!=sc_army[i]) )
    {
      if( damage )
        Action(u,ACT_DAMAGE,damage);
      else
        Action(u,ACT_STOP,1);
    }
}
ScyllaTact()
{
  int i,k,s,t;
  if( sc_n<0 )
  {
    sc_n=0;
    k=0;
    for( s=FirstSprite(); s; s=NextSprite() )
      if( GetUnitVid(s)==252 && k<8 )
      {
        sc_j1[k]=s;
        k++;
      }
    for( i=0; i<k; i++ )
    {
      s=sc_j1[i];
      sc_x[sc_n]=GetX(s);
      sc_y[sc_n]=GetY(s);
      sc_z[sc_n]=GetZ(s)+20;
      sc_army[sc_n]=Action(s,ACT_GET_ARMY);
      sc_cx[sc_n]=-15;
      sc_cy[sc_n]=-10;
      if( GetDirection(s)>=64 && GetDirection(s)<192 )
        sc_cy[sc_n]=10;
      sc_j1[sc_n]=FindNearestSprite(GETSPRITE_VID+253,sc_x[sc_n]+sc_cx[sc_n],sc_y[sc_n]+sc_cy[sc_n],10);
      sc_j2[sc_n]=FindNearestSprite(GETSPRITE_VID+253,sc_x[sc_n]-sc_cx[sc_n],sc_y[sc_n]-sc_cy[sc_n],10);
      if( sc_j1[sc_n] && sc_j2[sc_n] )
      {
        Log("SCYLLA "+itoa(sc_n+1)+" at "+itoa(sc_x[sc_n])+","+itoa(sc_y[sc_n])+" army "+itoa(sc_army[sc_n]));
        sc_n++;
      }
    }
  }
  if( !sc_n )
    return 0;
  t=GetTime();
  if( sc_mode==0 )
  {
    if( t-sc_bite>15000 )
    {
      sc_mode=1;
      sc_bite=t;
      sc_time=t;
    }
  }
  else if( sc_mode==1 )
  {
    if( t-sc_time>300 )
    {
      sc_mode=2;
      sc_time=t;
      SetEnvironment(ENV_EARTHQUAKE);
      PlaySFX(57);
      Log("SCYLLA bites");
    }
    else
      for( i=0; i<sc_n; i++ )
      {
        ScyllaHits(i,0);
        ScyllaJaws(i,1000-1000*(t-sc_time)/300);
      }
  }
  else if( sc_mode==2 )
  {
    if( t-sc_time>2000 )
    {
      sc_mode=3;
      sc_time=t;
      SetEnvironment(ENV_EARTHQUAKE+ENV_STOP);
      for( i=0; i<sc_n; i++ )
        ScyllaJaws(i,0);
    }
    else
      for( i=0; i<sc_n; i++ )
      {
        ScyllaHits(i,200);
        Action(sc_j1[i],ACT_CHANGE_COOR,sc_x[i]+Random(3),sc_y[i]+Random(2),sc_z[i]);
        Action(sc_j2[i],ACT_CHANGE_COOR,sc_x[i]+Random(3),sc_y[i]+Random(2),sc_z[i]);
      }
  }
  else
  {
    if( t-sc_time>300 )
    {
      sc_mode=0;
      for( i=0; i<sc_n; i++ )
        ScyllaJaws(i,1000);
    }
    else
      for( i=0; i<sc_n; i++ )
      {
        ScyllaHits(i,200);
        ScyllaJaws(i,1000*(t-sc_time)/300);
      }
  }
  return 1;
}
PiecesTact()
{
  CitadelTact();
  CharybdisTact();
  NestTact();
  CrusherTact();
  ScyllaTact();
}
ArrivalsTact()
{
  int i,t;
  PiecesTact();
  if( !OneTimeInSecond() )
    return 0;
  t=(GetTime()-time_game_total)/1000;
  iff(1)
  {
    OrdersTact();
#ifdef CLEVER_ATTACK
    SetCleverEnemyAttack(CLEVER_ATTACK);
#endif
  }
  FactoryTact(t);
  PlanTact(t);
  CaptureTact();
  ArrivalsMade();
  for( i=0; i<ARRIVALS; i++ )
  {
    ArrivalOnPoint(i);
    ArrivalTry(i,t);
  }
  return 1;
}
"""

MISSION_SCRIPT = r"""// Mission script of a map: the map, its ground image and this script make a level.
// The blocks between "// <editor>" and "// </editor>", "// <editor main>" and "// </editor main>",
// "// <editor arrivals>" and "// </editor arrivals>", "// <editor tact>" and "// </editor tact>"
// belong to the map editor of the game (LocoFix, DEVELOPERS.md "Editor mode"): File > Mission
// settings..., the Push line tool and the Arrivals panel change them and the editor writes them
// anew when the map is saved. The rest of the file stays as it is - the mission's own code goes there.
// main() runs every frame; iff(...) runs its block once. The scripts in maps\script\ read the
// settings of the first block (MAP_RULES.md 8).
// <editor> the mission's settings - written by the map editor when the map is saved
// title: @TITLE@
#define LEVEL 0
#define CUSTOM_MAP "maps\\@NAME@\\@NAME@.map"
#define MONEY 0
#define START_DAYNIGHT 1
#define RAIN 0
#define NUKE 0
#define music0 "music\\mus08.ogg"
#define music1 "music\\mus07.ogg"
#define MUXI 0
#define MUXI_NVID 268
#define CIVIL 0
#define SQUALL 1
#define TORNADO 0
#define QUAKE_DELAY 0
#define RAIN_ROUND_POWER 10
#define ENV_SOUND_DAY 0
#define ENV_SOUND_NIGHT 126
#define GAMMA1 0x00000000
#define GAMMA2 0x0000253f
#define GAMMA3 0x00715131
#define GAMMA4 0x001422f1
#define RAIN_GAMMA1 0x00221400
#define RAIN_GAMMA2 0x00453934
#define RAIN_GAMMA3 0x00796259
#define RAIN_GAMMA4 0x003c25f7
#define AI_REINFORCEMENT 1
#define RENEW_TIMEOUT 45
#define FIRSTCONTACTTIME 15
#define FIRSTATTACKTIME 30
#define STG_DESTROY_TIME 20
#define OBJECTIVES_FILE "maps\\@NAME@\\objectives.txt"
#define WIN_FILE "maps\\@NAME@\\win.txt"
#define LOOSE_FILE "maps\\@NAME@\\loose.txt"
#define BRIEF_LIST "maps\\@NAME@\\brief.lst"
#define BRIEF_HEADER "@TITLE@"
#define HISCORES_FILE "maps\\@NAME@\\hiscores"
#define PLAYER_REINFORCEMENT 0
int player_reinforcement_train[]={5,0};
#define LIMIT_ATACKERS 2
#define ARRIVALS 0
#define ORDERS 0
#define UNLOCKS 0
#define UNIT_LIMITS 0
#define PLANS 0
#define CLEVER_ATTACK 1
#define PRICES 0
#define FACTIONS 1
#include "maps\script\TrainsTactDeclares.lgc"
int start_trains[]={-1};
int start_trains_task[]={0};
int start_trains_x[]={0};
int start_trains_y[]={0};
int start_trains_renew[]={0};
int start_trains_random_task[]={0};
int level_queue_times[]={0};
int level_queue_trains[]={-1};
int level_queue_task[]={0};
int arrival_army[]={0};
int arrival_type[]={0};
int arrival_x[]={0};
int arrival_y[]={0};
int arrival_z[]={0};
int arrival_first[]={0};
int arrival_every[]={0};
int arrival_times[]={0};
int arrival_if[]={0};
int arrival_if_n[]={0};
int arrival_or[]={0};
int arrival_task[]={0};
int arrival_to_x[]={0};
int arrival_to_y[]={0};
int arrival_to_z[]={0};
int arrival_to2_x[]={0};
int arrival_to2_y[]={0};
int arrival_raid[]={0};
int arrival_msg[]={0};
int arrival_done[]={0};
int arrival_last[]={0};
int arrival_unit[]={0};
int arrival_part[]={0};
int arrival_car[]={0};
int order_vid[]={0};
int order_x[]={0};
int order_y[]={0};
int order_task[]={0};
int order_type[]={0};
int order_to_x[]={0};
int order_to_y[]={0};
int order_to_z[]={0};
int unlock_x[]={0};
int unlock_y[]={0};
int unlock_vid[]={0};
int unlock_time[]={0};
int limit_army[]={0};
int limit_vid[]={0};
int limit_max[]={0};
int plan_type[]={0};
int plan_task[]={0};
int plan_when[]={0};
int plan_first[]={0};
int plan_every[]={0};
int plan_times[]={0};
int plan_if[]={0};
int plan_if_n[]={0};
int plan_prev[]={0};
int plan_fx[]={0};
int plan_fy[]={0};
int plan_army[]={0};
int plan_to_x[]={0};
int plan_to_y[]={0};
int plan_to_z[]={0};
int plan_to2_x[]={0};
int plan_to2_y[]={0};
int plan_state[]={0};
int plan_done[]={0};
int plan_last[]={0};
int plan_since[]={0};
int plan_slot[]={0};
int plan_fifo[]={0};
int plan_next[]={0};
int plan_base[]={0};
int plan_unit[]={0};
int price_vid[]={0};
int price_of[]={0};
// </editor>
// What the settings mean: LEVEL the mission number (from 0), CUSTOM_MAP marks a map that is not
// a campaign mission (no campaign progress or scores, the Esc menu's restart loads this map, the
// menu comes back on the custom maps page), MONEY the money at the start, START_DAYNIGHT the time
// of day at the start (1 day, 2 evening, 3 night, 4 morning) and GAMMA1-4 the light of each,
// RAIN and NUKE (lightning: on unless 0) the weather, the *_FILE, BRIEF_LIST and BRIEF_HEADER the
// texts of the map, PLAYER_REINFORCEMENT the player's reinforcement waves (the first one brings
// the player's train at the start) in the train player_reinforcement_train (VIDs, 0 ends it),
// LIMIT_ATACKERS the enemy trains attacking at once, start_trains* the enemy's trains at the start
// (TrainsTactDeclares.lgc: ATT_... types, ATA_FIGHT or ATA_PATROLn with the points ATA_PATROLn_X1
// ... _Y2, screen coordinates; -1 ends the list - without the arrays TrainsTact() puts a train at
// (0, 0), BUGS.md F10), level_queue_* the trains its depot builds later (the earliest times, s)
// and ARRIVALS, arrival_*[] the trains that arrive from the entries of the map (the Arrivals panel),
// ORDERS, order_*[] the trains' orders (a train's panel), UNLOCKS, unlock_*[] the units a factory
// gets later, UNIT_LIMITS, limit_*[] the unit limits, PLANS, plan_*[] the enemy factory's plan (a
// factory's panel: the next block puts each entry into level_queue_* when it is due), CLEVER_ATTACK
// SetCleverEnemyAttack's value (RENEW_TIMEOUT, FIRSTCONTACTTIME, FIRSTATTACKTIME, start_trains_renew
// and start_trains_random_task are the AI's other knobs of the panel), FACTIONS 1 the editor's
// rules of a fight, which LocoFix applies to a map with it: a neutral (army 2) fires only back, at
// an army that hit it or a neutral it sees, the pirates (army 3) at every side (MAP_RULES.md 6c).
#include "maps\script\header.lgc"
// <editor arrivals> the trains that arrive (the Arrivals panel) - written by the map editor when the map is saved
@RUNNER@// </editor arrivals>

int script_status=0;   // stage of the mission (for the mission's own code)
int map_enemy=0;       // the enemy had units (the win waits for them)
int map_player=0;      // the player had units (the loss waits for them)

// MAP_WIN_TEST (define it at the top of the game folder's copy to check the end of a map):
// at 20 s every unit of the enemy is destroyed, one a frame, so the mission is won.
#ifdef MAP_WIN_TEST
MapWinTest()
{
  int u;
  if( GetTime()<20000 )
    return 0;
  for( u=FirstUnit(U_UNIT); u; u=NextUnit() )
    if( Action(u,act_getarmy,0,0)==1 )
    {
      Log("MAPTEST destroying vid "+itoa(GetUnitVid(u))+" at "+itoa(GetX(u))+","+itoa(GetY(u)));
      Destroy(u);
      return 0;
    }
}
#endif

main()
{
  iff(1)
  {
    InitDefault();
    // The authors' reinforcement (the trains of the Arrivals panel come in its place): with
    // PLAYER_REINFORCEMENT waves the player's train arrives where the map has its point locomotive
    // (VID 5, army 3) - InitDefault takes that locomotive away and keeps its place in player_x/y/z
    // (reinforcement.lgc). A map without one has no place, so nothing arrives.
    if(player_x || player_y)
      AddReinforcement();
    // <editor main> the push lines - written by the map editor when the map is saved
    // </editor main>
    SetCleverEnemyAttack(1);
  }
  if( !MenuEnd() )
    if( !MenuTact() )
    {
      GameTact();
      TrainsTact();
      // <editor tact> the arrivals - written by the map editor when the map is saved
      ArrivalsTact();
      // </editor tact>
#ifdef MAP_WIN_TEST
      MapWinTest();
#endif
      // The mission goes here: change script_status, print messages.
      if( OneTimeIn5Second() )
      {
        // the next reinforcement wave when the player's train is gone and the depot stands
        if( (GetVid(5,VID_COUNT0)+GetVid(10,VID_COUNT0))==0 && GetVid(102,VID_COUNT0) )
          AddReinforcement();
        // won when the enemy had units and has none left (NoArmyUnits counts them,
        // ARMY1_NVID_ARRAY of constants.lgc: the trains, the depot, the fort's guns) and no
        // more of its trains are to arrive
        if( NoArmyUnits(1) )
          map_enemy=1;
        else if( map_enemy && !ArrivalsLeft(1) )
          Win();
        // lost when the player had units and has none left, nor a depot, nor trains to arrive
        if( NoArmyUnits(0) )
          map_player=1;
        else if( map_player && IsNoDepo() && !ArrivalsLeft(0) )
          Loose();
      }
    }
}
"""

# the texts of a new map (map_tool.py new --script), CP1250 in capitals as the game's
MISSION_SCRIPT = MISSION_SCRIPT.replace("@RUNNER@", ARRIVALS_RUNNER)

MISSION_TEXTS = {
    "objectives.txt": "DESTROY THE ENEMY TRAINS AND FACTORY\r\n",
    "win.txt": "THE ENEMY IS BEATEN. VICTORY!\r\n",
    "loose.txt": "WE HAVE LOST OUR TRAINS AND OUR FACTORY.\r\n",
    "brief.txt": "DESTROY THE ENEMY TRAINS AND FACTORY.\r\n",
    "brief.lst": "maps\\@NAME@\\brief.txt",
}


# The heights of a ground's picture (height.png, the texture after each colour texture), which the
# engine blits into the depth buffer and draws every object and its shadow against: the campaign's
# grounds hold 1026 + 8 a GRID level (l02: 1025-1028 at 0, l01: 1586 at 70). The editor makes the
# same (editor_ground.cpp picture_height, editor_newmap.cpp FLAT_PICTURE_HEIGHT; DEVELOPERS.md
# "The ground of a map"). A picture of 0 under a GRID of 70 puts every object ~1500 above the
# ground the engine sees: detached shadows.
PICTURE_HEIGHT_BASE, PICTURE_HEIGHT_STEP = 1026, 8


def picture_height(level):
    """The height of a ground's picture where its GRID is `level`."""
    return PICTURE_HEIGHT_BASE + PICTURE_HEIGHT_STEP * level


def new_ground(folder, width, height, tile=256, level=70):
    """Writes `folder` as the image of a map ground: the picture of the map (ground.png),
    its height (height.png: a flat ground at the GRID `level`, 70 as the new maps have it)
    and the tiles that cut it up (tiles.tsv). The picture is empty - the terrain is painted
    into it."""
    if width <= 0 or height <= 0:
        raise DataError("the size of the ground must be positive")
    columns = (width + tile - 1) // tile
    rows = (height + tile - 1) // tile
    count = columns * rows
    tiles = []
    for index in range(count):
        # column by column, as in every ground of the game (l01, l05)
        x = (index // rows) * tile
        y = (index % rows) * tile
        # The textures of a ground are in the file in pairs - the picture and its height -
        # so the tile of the picture k points at the texture 2k.
        texture = 2 * index
        tiles.append({"tile": str(index), "unknown_00": "0x00000000",
                      "texture": str(texture),
                      "offsetX": "0", "offsetY": "0",
                      "width": str(min(tile, width - x)),
                      "height": str(min(tile, height - y)),
                      "x": str(x), "y": str(y), "next": str((index + 1) % count)})
    os.makedirs(folder, exist_ok=True)
    Image.new("RGB", (width, height), (0, 0, 0)).save(os.path.join(folder, "ground.png"))
    Image.new("I;16", (width, height), picture_height(level)).save(os.path.join(folder, "height.png"))
    save_tsv(os.path.join(folder, "tiles.tsv"), TILE_COLUMNS, tiles)
    meta = {"chunks": ["HEAD", "SURF", "DATA"], "imageFlags": "0x35", "frameSpeed": 71,
            "frameCount": 1, "width": width, "height": height, "kind": "texture",
            "textures": count, "ground": True}
    save_json(os.path.join(folder, "vid.json"), meta)
    return meta


# ------------------------------------------------------------------ paint ----
#
# The picture of a map painted as the editor's Paint tool paints it (src/LocoFix/editor_ground.cpp,
# DEVELOPERS.md "The ground's picture, water, regions" and "The gravel bed in the Paint tool"): the
# textures are the 128 x 128 patches that the game folder's editor\textures.tsv names in the campaign
# grounds, laid mirrored at the pixel's place, so the editor's pipette names them; a dab blends a
# texture into a circle with the brush's soft, grainy edge; the gravel bed's band is laid along each
# rail from the top of the bed decal. The same arithmetic as the editor's, in doubles where it has
# floats (an edge pixel may differ by a step; a pixel laid at full strength is the sample itself).

PATCH = 128
BED_KINDS = {612: "meadow", 615: "ice", 616: "volcanic", 617: "desert"}
BED_U0, BED_U, BED_V0, BED_V, BED_SEGMENTS, BED_RISE, BED_WIDTH = 45, 60, 52, 36, 16, 7, 28
BED_RAILS = (610, 611)                  # the plain and destroyed rails (a swamp rail has its embankment)


def ground_picture(path):
    """(width, height, pixels) of a map ground's image: its picture as R5G6B5 values, row by row."""
    records, _ = read_container(open(path, "rb").read(), b"VID ")
    flags, _, _, w, h = HEAD.unpack(records[b"HEAD"][0])
    textures = read_textures(records[b"SURF"][0], flags)
    (data,) = records[b"DATA"]
    ts = tile_struct(flags)
    tiles = [ts.unpack_from(data, k) for k in range(0, len(data), ts.size)]
    order = ground_textures(tiles, flags, len(textures))
    if order is None:
        raise DataError(f"{path}: not a map ground")
    pixels = array.array("H", bytes(2 * w * h))
    for k, t in zip(order, tiles):
        tw, th, colour, _ = textures[k]
        source = array.array("H", colour)
        for y in range(min(th, h - t[7])):
            n = min(tw, w - t[6])
            pixels[(t[7] + y) * w + t[6]:(t[7] + y) * w + t[6] + n] = source[y * tw:y * tw + n]
    return w, h, pixels


def editor_textures(game):
    """{key: 128 x 128 patch} of the editor's textures (editor\\textures.tsv of the game folder)."""
    rows = load_table(os.path.join(game, "editor", "textures.tsv"), ["key", "map", "x", "y"])
    by_map, out = {}, {}
    for r in rows:
        by_map.setdefault(r["map"], []).append(r)
    for name, entries in by_map.items():
        w, h, pixels = ground_picture(os.path.join(game, "maps", name, name + ".vid"))
        for r in entries:
            x0, y0 = int(r["x"]), int(r["y"])
            if x0 < 0 or y0 < 0 or x0 + PATCH > w or y0 + PATCH > h:
                continue
            out[r["key"]] = [pixels[(y0 + y) * w + x0 + x] for y in range(PATCH) for x in range(PATCH)]
    return out


def bed_patch(game, nvid):
    """The band's colours of a bed kind (editor_ground.cpp read_beds): the decal's top, frame 1 of
    vid\\<nvid>.vid, BED_U x BED_V px, each row brought to the mean brightness and darkened to 13/16."""
    records, _ = read_container(open(os.path.join(game, "vid", f"{nvid}.vid"), "rb").read(), b"VID ")
    flags = HEAD.unpack(records[b"HEAD"][0])[0]
    palette = records[b"PAL "][0]
    depth = 2 if flags & 4 else 0
    rec = records[b"DATA"][1]
    _, _, runs = parse_frame(rec, 1 + depth)
    image = {}
    for y, x, count, o in runs:
        o += depth * count
        for k in range(count):
            i = rec[o + k]
            if len(palette) == 1024:
                image[(x + k, y)] = (palette[4 * i + 2], palette[4 * i + 1], palette[4 * i])
            else:
                image[(x + k, y)] = (palette[3 * i], palette[3 * i + 1], palette[3 * i + 2])
    cells = [[image.get((BED_U0 + u, BED_V0 + v)) for u in range(BED_U)] for v in range(BED_V)]
    if any(c is None for row in cells for c in row):
        raise DataError(f"vid\\{nvid}.vid: frame 1 does not cover the bed's top")
    sums = [sum(sum(c) for c in row) for row in cells]
    total = sum(sums)
    out = []
    for v, row in enumerate(cells):
        div = BED_V * (sums[v] or 1) * 16
        for c in row:
            r, g, b = (min(255, x * total * 13 // div) for x in c)
            out.append(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
    return out


def _hash(x, y):
    h = (x * 374761393 + y * 668265263) & 0xffffffff
    h = ((h ^ (h >> 13)) * 1274126177) & 0xffffffff
    return h ^ (h >> 16)


def _weight(distance, radius, x, y, hard=False):
    """The brush's weight at a distance (editor_ground.cpp weight): full inside, falling off over
    the outer 45 %, the edge broken by a grain; a hard brush is full to its edge."""
    d = distance / radius + ((_hash(x, y) & 255) / 255.0 - 0.5) * (0.03 if hard else 0.12)
    if d >= 1:
        return 0.0
    if hard:
        return 1.0
    t = 1.0 if d <= 0.55 else (1 - d) / 0.45
    return t * t * (3 - 2 * t)


def _blend(a, b, t):
    if t <= 0:
        return a
    if t >= 1:
        return b
    ar, ag, ab = a >> 11, (a >> 5) & 63, a & 31
    br, bg, bb = b >> 11, (b >> 5) & 63, b & 31
    return (int(ar + (br - ar) * t + 0.5) << 11) | (int(ag + (bg - ag) * t + 0.5) << 5) | \
        int(ab + (bb - ab) * t + 0.5)


def _sample(patch, x, y):
    u, v = x & 255, y & 255             # mirrored every 128 px: no seams
    if u >= PATCH:
        u = 255 - u
    if v >= PATCH:
        v = 255 - v
    return patch[v * PATCH + u]


def paint_dab(pixels, w, h, x, y, radius, patch, strength=1.0, hard=False):
    """A dab of the Paint tool (ground_paint) at the picture's point x, y."""
    cx, cy, r = math.floor(x), math.floor(y), math.ceil(radius)
    for py in range(max(0, cy - r), min(h, cy + r + 1)):
        for px in range(max(0, cx - r), min(w, cx + r + 1)):
            a = _weight(math.hypot(px - cx, py - cy), radius, px, py, hard) * strength
            if a > 0:
                i = py * w + px
                pixels[i] = _blend(pixels[i], _sample(patch, px, py), a)


def bed_line(x, y, z, direction):
    """A rail's line on the picture (editor_ground.cpp bed_line): BED_SEGMENTS pieces of the
    quadratic curve from its end 0 through its middle to its end 1 at its points' heights, BED_RISE
    px up the screen where its sprite shows the sleepers; the points and their lengths along it."""
    _, _, shape, z0, z1 = rail_of(x, y, z, direction)
    za, zc = z0 - RAIL_ALTITUDE + BED_RISE, z1 - RAIL_ALTITUDE + BED_RISE
    (e0x, e0y), (e1x, e1y) = RAIL_ENDS[shape]
    ax, ay = x + e0x * RAIL_HALF[0], y + e0y * RAIL_HALF[1] - za
    bx, by = x, y - (za + zc) / 2
    cx, cy = x + e1x * RAIL_HALF[0], y + e1y * RAIL_HALF[1] - zc
    points, lengths = [], []
    for i in range(BED_SEGMENTS + 1):
        t = i / BED_SEGMENTS
        m = 1 - t
        p = (m * m * ax + 2 * t * m * bx + t * t * cx, m * m * ay + 2 * t * m * by + t * t * cy)
        lengths.append(lengths[-1] + math.hypot(p[0] - points[-1][0], p[1] - points[-1][1]) if points else 0.0)
        points.append(p)
    return points, lengths


def paint_bed(pixels, w, h, rails, patch, width=BED_WIDTH, strength=1.0, hard=False):
    """The gravel bed's band under the rails (ground_bed): a pixel within half the width of a line
    takes the nearest line's colour - the decal's row by its distance from the line, its column by
    the place along the rail, mirrored, a whole number of BED_U to the rail."""
    hw = width / 2
    nearest, along = {}, {}
    for rail in rails:
        points, lengths = bed_line(*rail)
        length = lengths[-1]
        scale = max(1.0, float(round(length / BED_U))) * BED_U / length if length > 0 else 0
        for i in range(BED_SEGMENTS):
            ax, ay = points[i]
            ux, uy = points[i + 1][0] - ax, points[i + 1][1] - ay
            len2 = ux * ux + uy * uy
            for py in range(max(0, math.floor(min(ay, ay + uy) - hw)), min(h, math.ceil(max(ay, ay + uy) + hw) + 1)):
                for px in range(max(0, math.floor(min(ax, ax + ux) - hw)), min(w, math.ceil(max(ax, ax + ux) + hw) + 1)):
                    qx, qy = px + 0.5 - ax, py + 0.5 - ay
                    t = max(0.0, min(1.0, (qx * ux + qy * uy) / len2)) if len2 > 0 else 0.0
                    d = math.hypot(qx - ux * t, qy - uy * t)
                    k = py * w + px
                    if d < hw and d < nearest.get(k, hw):
                        nearest[k] = d
                        along[k] = (lengths[i] + t * (lengths[i + 1] - lengths[i])) * scale
    for k, d in nearest.items():
        px, py = k % w, k // w
        a = _weight(d, hw, px, py, hard) * strength
        if a <= 0:
            continue
        v = min(BED_V - 1, int(d / hw * BED_V))
        u = math.floor(along[k]) % (2 * BED_U)
        if u >= BED_U:
            u = 2 * BED_U - 1 - u
        pixels[k] = _blend(pixels[k], patch[v * BED_U + u], a)


# What POLIGON's picture is painted with: the editor's grass, light grass in soft patches, packed
# earth (the editor's sand) under the bases - the factories, repair points and forts -, the meadow
# bed under the whole track.
PAINT_BASE, PAINT_PATCHES, PAINT_EARTH = "TexGrass", "TexLightGrass", "TexSand"
PAINT_BASES = (102, 104, 162)


def paint_map(folder, game, bed=612, seed=7):
    """Paints the picture of the unpacked map `folder` (its vid\\ground.png) with the editor's
    textures sampled in the game folder `game`, as the Paint tool would: the grass base, patches
    of light grass, the earth under the bases and the gravel bed of the kind `bed` under every
    rail. The height (vid\\height.png) is left as it is."""
    vid = os.path.join(folder, "vid")
    meta = load_json(os.path.join(vid, "vid.json"))
    w, h = meta["width"], meta["height"]
    textures = editor_textures(game)
    for key in (PAINT_BASE, PAINT_PATCHES, PAINT_EARTH):
        if key not in textures:
            raise DataError(f"{game}: the texture {key} cannot be sampled")
    model = load_map(folder)
    objects = model["objects"]
    ground = next(o for o in objects if int(o["nvid"]) == 1024)
    left, top = float(ground["x"]) - w / 2, float(ground["y"]) - h / 2
    base = textures[PAINT_BASE]
    pixels = array.array("H", (_sample(base, x, y) for y in range(h) for x in range(w)))
    state = seed
    def random(n):                      # an LCG of its own: the same picture every time
        nonlocal state
        state = (state * 1103515245 + 12345) & 0x7fffffff
        return state % n
    for _ in range(w * h // 150000):    # strokes of a few dabs, each patch its own shape
        x, y, r, angle = random(w), random(h), 60 + random(70), random(360) * math.pi / 180
        for _ in range(3 + random(4)):
            paint_dab(pixels, w, h, x, y, r, textures[PAINT_PATCHES])
            angle += (random(91) - 45) * math.pi / 180
            x, y = x + math.cos(angle) * r * 0.7, y + math.sin(angle) * r * 0.7
    for o in objects:
        if int(o["nvid"]) in PAINT_BASES:
            x, y = float(o["x"]) - left, float(o["y"]) - float(o["z"]) - top + 10
            for dx in (-36, 0, 36):
                paint_dab(pixels, w, h, x + dx, y, 72, textures[PAINT_EARTH])
    rails = [(float(o["x"]) - left, float(o["y"]) - top, float(o["z"]), int(o["direction"]))
             for o in objects if int(o["nvid"]) in BED_RAILS]
    paint_bed(pixels, w, h, rails, bed_patch(game, bed))
    rgb565_image(pixels.tobytes(), w, h).save(os.path.join(vid, "ground.png"))
    return len(rails)


def new_map(template, folder, objects_res=None, vid=None, script=False, size=None):
    """Writes `folder` as a new map made from `template`: its terrain (the ground object,
    the ground's OBJ record, GRPH, HEAD and GRID) and nothing else - no other objects, no
    groups, no players. `vid` points the ground at another image, `script` writes
    `<folder name>.lgc`, a mission that runs as it is (MISSION_SCRIPT, with the map editor's
    blocks), and its texts (MISSION_TEXTS)."""
    model = read_map(open(template, "rb").read(),
                     object_info(objects_res or find_objects_res(template)))
    for key in ("objects", "vids", "grid"):
        if key not in model:
            raise DataError(f"{template}: no readable {key}")
    ground = model["objects"][0]
    if parse_int(ground["nvid"]) != 1024:
        raise DataError(f"{template}: the first object is not the ground (nvid 1024)")
    model["objects"] = [ground]
    model["vids"] = [r for r in model["vids"] if parse_int(r["nvid"]) == 1024]
    if not model["vids"]:
        raise DataError(f"{template}: no OBJ record for the ground (nvid 1024)")
    if vid:
        model["vids"][0]["imageFile"] = vid
    # the bytes after the image file name are the editor's leftover buffer, not data
    model["vids"][0]["trailing"] = ""
    model["vid_animations"] = []
    model["groups"] = []
    model["info"]["players"] = [0, 0, 0, 0]
    if size:
        # A map of another size: the ground height of the cells is flat and the camera
        # looks at the middle of the map. The ground image is drawn centred on the ground
        # object, so the object moves to the middle of the map as well (as in l01 and l05;
        # left where it was, the image is drawn off by the difference and the rest of the
        # map has no ground).
        head = model["info"]["head"]
        if (int(head["width"]), int(head["height"])) != size:
            head["width"] = float(size[0])
            head["height"] = float(size[1])
            head["cameraX"] = float(max(0, (size[0] - 1024) // 2))
            head["cameraY"] = float(max(0, (size[1] - 768) // 2))
            ground["x"] = f"{size[0] / 2:g}"
            ground["y"] = f"{size[1] / 2:g}"
            w, h = grid_size(model["info"])
            model["grid"] = [[0] * w for _ in range(h)]
    save_map(model, folder)
    if script:
        name = os.path.basename(os.path.normpath(folder))
        title = name.upper()
        with open(os.path.join(folder, name + ".lgc"), "w", encoding="latin-1",
                  newline="\n") as f:
            f.write(MISSION_SCRIPT.replace("@NAME@", name).replace("@TITLE@", title))
        for file, text in MISSION_TEXTS.items():
            with open(os.path.join(folder, file), "w", encoding="cp1250", newline="") as f:
                f.write(text.replace("@NAME@", name))
    return model


def catalog(objects_res):
    """(nvid, name, class) of every object in objects.res - the objects to place."""
    tables = res_tool.read_tables(open(objects_res, "rb").read())
    return [(int(r["nvid"]), r["name"], r["spriteClass"]) for r in tables["objects"]]


def list_map(path, objects_res=None):
    """Prints what a map holds: its size, the ground image, the objects and the grid."""
    model = read_map(open(path, "rb").read(),
                     object_info(objects_res or find_objects_res(path)))
    head = model["info"]["head"]
    print(f"{path}: {head['width']:.0f}x{head['height']:.0f}, version {head['version']}, "
          f"camera {head['cameraX']:.0f},{head['cameraY']:.0f}")
    for row in model.get("vids", []):
        if parse_int(row["nvid"]) == 1024:
            print(f"ground: {row['imageFile']}")
    objects = model.get("objects", [])
    print(f"{len(objects)} objects")
    for row in sorted(objects, key=lambda r: (float(r["y"]), float(r["x"]))):
        print(f"  nvid {row['nvid']:>5} {row.get('name', '')} at "
              f"({row['x']}, {row['y']}, {row['z']}) direction {row['direction']} "
              f"army {row['army']}")
    groups = model.get("groups", [])
    if groups:
        print(f"{len(groups)} groups")
    grid = model.get("grid")
    if grid:
        heights = [h for row in grid for h in row]
        print(f"grid {len(grid[0])}x{len(grid)} cells, height {min(heights)}..{max(heights)}")


def place_object(folder, nvid, x, y, objects_res=None, army=0, direction=0, z=None,
                 object_id=None, reference=None, items=None):
    """Adds an object to an unpacked map (the folder). The state of the object (behave,
    items, data) is copied from another object of the same VID - in the map itself or in
    the reference map - because the engine reads that state when the level starts. The
    commands of the source are not copied: they are the orders of its own mission (a move
    to its own place). The height is the ground height of the cell (x / 8, y / 8) unless
    `z` is given.

    Building rules (from objects.res):
    * a sprite with nDir 1 (most buildings) is drawn without rotation, so `direction`
      is forced to 0 - other values break its birth/death animation frames;
    * a building or depot flattens the terrain under its footprint (sizeX x sizeY from
      objects.res, 8x8 px grid cells) to a single height, with a one-cell ramp around it;
    * a depot (VID 102) must sit next to a rail node (its exit snaps to the first RailDot
      of the track), and a player-owned depot (army 0) needs a build list (`items`);
    * a workshop (VID 104) must be within 20 px of a track (Building_UpdateWorkshop);
    * a fort (VID 162) is a gate: it stands on a track (within 8 px), turned along it
      (0/128 on a vertical track, 64/192 on a horizontal one)."""
    model = load_map(folder)
    if "objects" not in model:
        raise DataError(f"{folder}: no objects")
    info = object_info(objects_res or find_objects_res(folder))
    vids = vid_info(objects_res or find_objects_res(folder))
    if info and nvid not in info:
        raise DataError(f"{folder}: no object {nvid} in objects.res")
    name, sprite_class = info.get(nvid, ("", ""))
    vid = vids.get(nvid)
    if not info:
        # Without objects.res the class of the object is unknown, so a unit that needs its
        # state copied (an engine, a building) would be placed without it and the game
        # would not start the level.
        print(f"warning: {folder}: no objects.res (pass --objects) - the class of {nvid} "
              f"is unknown and the state is not checked")
    source = None
    for row in model["objects"]:
        if parse_int(row["nvid"]) == nvid:
            source = row
            break
    if source is None and reference:
        other = read_map(open(reference, "rb").read(), info)
        for row in other.get("objects", []):
            if parse_int(row["nvid"]) == nvid:
                source = row
                break
    if source is None and sprite_class in UNIT_CLASSES:
        raise DataError(f"{folder}: no object {nvid} to copy the state from - use "
                        "--from <map> with a map that has one")
    ids = [parse_int(row["id"]) for row in model["objects"]]
    if object_id is None:
        object_id = max(ids) + 1 if ids else 1
    elif object_id in ids:
        raise DataError(f"{folder}: the id {object_id} is already used")
    if z is None:
        w, h = grid_size(model["info"])
        cell_x, cell_y = int(x) // 8, int(y) // 8
        if not 0 <= cell_x < w or not 0 <= cell_y < h:
            raise DataError(f"{folder}: ({x}, {y}) is outside the map")
        z = model["grid"][cell_y][cell_x]

    # ---- building rules -------------------------------------------------
    if vid:
        ndir = parse_int(vid["nDir"])
        if ndir == 1 and direction != 0:
            print(f"note: {folder}: nvid {nvid} ({sprite_class}) has nDir 1, "
                  f"forcing direction {direction} -> 0")
            direction = 0
        if sprite_class in BUILDING_CLASSES:
            size_x = float(vid["sizeX"])
            size_y = float(vid["sizeY"])
            _flatten_footprint(model, x, y, size_x, size_y, z)
    if nvid == DEPO_NVID:
        nodes = rail_nodes(model)
        if nodes:
            nearest = min(math.hypot(x - nx, y - ny) for nx, ny in nodes)
            if nearest > DEPOT_RAIL_RADIUS:
                raise DataError(f"{folder}: depot {nvid} at ({x}, {y}) is {nearest:.0f} px "
                                f"from the nearest rail node (limit {DEPOT_RAIL_RADIUS} px) - "
                                f"lay track with `track` first so its exit snaps to a RailDot")
        else:
            print(f"note: {folder}: depot {nvid} has no rail node in the map - lay track "
                  f"with `track` and add SetPushLine to the mission script")
        if army == 0 and not (items or (source and source["items"])):
            raise DataError(f"{folder}: player depot {nvid} has no build list (items) - "
                            f"pass --items '5 10 20 25 ...' or copy from a map via --from")
    elif nvid == WORKSHOP_NVID:
        nodes = rail_nodes(model)
        if nodes:
            nearest = min(dist_point_to_segment(x, y, *n) for n in _segment_ends(model))
            if nearest > WORKSHOP_TRACK_LIMIT:
                print(f"note: {folder}: workshop {nvid} at ({x}, {y}) is {nearest:.0f} px "
                      f"from the track (limit {WORKSHOP_TRACK_LIMIT} px) - wagons may not "
                      f"reach it for repairs")
    elif nvid == FORT_NVID:
        nodes = rail_nodes(model)
        if nodes:
            nearest = min(dist_point_to_segment(x, y, *n) for n in _segment_ends(model))
            if nearest > FORT_TRACK_LIMIT:
                print(f"note: {folder}: fort {nvid} at ({x}, {y}) is {nearest:.0f} px "
                      f"from the track (limit {FORT_TRACK_LIMIT} px) - a fort is a gate "
                      f"that stands on the track, the trains pass through it")
    # ---------------------------------------------------------------------
    row = {"id": str(object_id), "nvid": str(nvid), "name": name, "x": str(x), "y": str(y),
           "z": str(z), "direction": str(direction), "army": str(army),
           "commands": "",
           "behave": source["behave"] if source else "",
           "items": items if items is not None else (source["items"] if source else ""),
           "data": source["data"] if source else ""}
    model["objects"].append(row)
    save_map(model, folder)
    return row


def _segment_ends(model):
    """((ax, ay, bx, by), ...) track segments of the map's rails."""
    return [(x + RAIL_ENDS[shape][0][0] * 35.5, y + RAIL_ENDS[shape][0][1] * 25.0,
             x + RAIL_ENDS[shape][1][0] * 35.5, y + RAIL_ENDS[shape][1][1] * 25.0)
            for x, y, shape, _z0, _z1 in map_rails(model)]


def delete_objects(folder, nvid):
    """Removes every object of one VID from an unpacked map (the folder): the old rails
    before a new track, for example. Returns the number of objects removed."""
    model = load_map(folder)
    if "objects" not in model:
        raise DataError(f"{folder}: no objects")
    keep = [row for row in model["objects"] if parse_int(row["nvid"]) != nvid]
    removed = len(model["objects"]) - len(keep)
    if not removed:
        raise DataError(f"{folder}: no object {nvid}")
    model["objects"] = keep
    save_map(model, folder)
    return removed


# The track of one rail goes from an end to the other. The ends are in halves of the sprite
# size: 35.5 px in x (sizeX 71) and 25 px in y (sizeY 50) for the rail VID 610, and the 12
# shapes are the 4 straight rails and the 8 curves of the sprite. Rail_Action (0x432740)
# does not put the rail-network points at the ends, though: it puts them a quarter of the
# size from the centre in the direction of each end (tables 0x47e908/0x47e938 and
# 0x47e998/0x47e9c8, scale 0.25) and links each one to the point it finds three quarters
# out (scale 0.75) - the near point of the next rail only when that rail's end at the
# joint points the opposite way. So two rails join only where their end vectors are
# opposite: a straight horizontal rail and a straight diagonal do not link, a curve
# between them does.
RAIL_ENDS = [((0, -1), (0, 1)), ((1, 0), (-1, 0)), ((1, -1), (-1, 1)), ((-1, -1), (1, 1)),
             ((1, -1), (0, 1)), ((-1, -1), (0, 1)), ((1, 1), (0, -1)), ((-1, 1), (0, -1)),
             ((1, 1), (-1, 0)), ((1, -1), (-1, 0)), ((-1, -1), (1, 0)), ((-1, 1), (1, 0))]
# The direction byte of each flat shape, in the same order. Rail_Action takes the frame of
# the direction, ((direction + dirHalfStep) & 255) * nDir >> 8 with nDir 37 and dirHalfStep
# 128 / 37 = 3, as the shape (frame % 12, the order of RAIL_ENDS) and the height of its two
# points (frame / 12, see rail_of), so the flat shapes 0-11 are frames 0-11, drawn by the
# directions 0, 6, 13, 20, 27, 34, 41, 48, 55, 62, 69 and 76.
RAIL_DIRECTION = (0, 6, 13, 20, 27, 34, 41, 48, 55, 62, 69, 76)
RAIL_FRAMES = 37                      # nDir of VID 610
RAIL_HALF_STEP = 128 // RAIL_FRAMES   # Vid.dirHalfStep
RAIL_ALTITUDE = 10                    # VID 610: the rail points stand this much above the rail
RAIL_RAMP = 11                        # sizeZ of VID 610: a ramp raises one of its points by it


def rail_of(x, y, z, direction):
    """(x, y, shape, z0, z1) of a rail object as Rail_Action puts its two points: the shape is
    the frame % 12 (RAIL_ENDS) and z0, z1 the heights of the points at its end 0 and end 1 -
    z + RAIL_ALTITUDE, frames 12-23 (group 1) raise the point of end 0 by RAIL_RAMP, frames
    24-35 (group 2) the point of end 1: the ramps; frames 0-11 and 36 are flat. The engine
    truncates the height to an int, and two rails link only where their points have the same
    height."""
    frame = ((direction + RAIL_HALF_STEP) & 255) * RAIL_FRAMES >> 8
    group = frame // 12
    return (x, y, frame % 12, int(z + RAIL_ALTITUDE + (RAIL_RAMP if group == 1 else 0)),
            int(z + RAIL_ALTITUDE + (RAIL_RAMP if group == 2 else 0)))


def rail_candidates(a, b, size=(71, 50)):
    """(x, y, shape, first, second) of the rails that connect the rail ends a and b, best
    first: a straight rail, then the other shapes in sprite order; first and second are
    the end vectors (RAIL_ENDS) of the rail at a and at b. Two rails of mirrored sprites
    can join the same two points; the first is the one whose object stands at a - first
    end, the second at a - second end."""
    hx, hy = size[0] / 2, size[1] / 2
    out = []
    for swap in (0, 1):
        for shape, ends in enumerate(RAIL_ENDS):
            first, second = ends if not swap else ends[::-1]
            x, y = a[0] - first[0] * hx, a[1] - first[1] * hy
            if (round(x + second[0] * hx, 3), round(y + second[1] * hy, 3)) == (b[0], b[1]):
                out.append((x, y, shape, first, second))
    return out


RAIL_HALF = (35.5, 25.0)              # half the rail sprite (71 x 50)
MAX_RAIL_LINKS = 6                    # links of one rail point ("Too many links")


class TrackError(DataError):
    """A track the engine would not link. `code` names the rule (the same codes as
    src/LocoFix/rail_rules.h, which tools/test_rail_rules.py compares with this):
    step - not a step of the lattice; join - the rail does not join the one before it;
    sharp - a turn over 45 degrees; last - the last rail does not join the rails at the
    end; links - a rail point with more than 6 links; duplicate - the same rail is there."""

    def __init__(self, code, step, message):
        super().__init__(message)
        self.code = code
        self.step = step


def map_rails(model, nvid=RAIL_NVID):
    """(x, y, shape, z0, z1) of the map's rails of one VID, ramps included (rail_of)."""
    rails = []
    for row in model.get("objects", []):
        if parse_int(row["nvid"]) != nvid:
            continue
        rails.append(rail_of(float(row["x"]), float(row["y"]), float(row["z"]),
                             parse_int(row["direction"])))
    return rails


def _ends(rail):
    """((end vector, height of its point), (the other end, its height)) of a rail and the
    same the other way round."""
    _x, _y, shape, z0, z1 = rail
    first, second = RAIL_ENDS[shape]
    return (((first, z0), (second, z1)), ((second, z1), (first, z0)))


def _ends_at(rail, point):
    hx, hy = RAIL_HALF
    for (end, height), far in _ends(rail):
        if abs(rail[0] + end[0] * hx - point[0]) < 0.01 and abs(rail[1] + end[1] * hy - point[1]) < 0.01:
            yield end, height, far


def rail_ends_at(rails, point, z=None):
    """End vectors (RAIL_ENDS) of the rails (x, y, shape, z0, z1) that end at `point` with
    their point there at the height `z` (at any height when z is None)."""
    return [end for rail in rails for end, height, _far in _ends_at(rail, point)
            if z is None or height == z]


def rail_point_links(rails, point):
    """The rail points the engine makes at a joint `point` and their links: {(end vector,
    height): links}. The rail ends there that point the same way at the same height share
    one point (RailMap_AddDot), which links to the far point of each of those rails (a rail
    laid twice adds no link) and to the point of the rails ending the opposite way at that
    height (Rail_Action) - the engine's limit of 6 (RailDot_Connect) is per point, not per
    joint."""
    hx, hy = RAIL_HALF
    far_points = {}
    for rail in rails:
        for end, height, (far_end, far_height) in _ends_at(rail, point):
            far = (round(rail[0] + far_end[0] * hx / 2, 3), round(rail[1] + far_end[1] * hy / 2, 3),
                   far_height)
            far_points.setdefault((end, height), set()).add(far)
    return {(end, height): len(far) + (((-end[0], -end[1]), height) in far_points)
            for (end, height), far in far_points.items()}


def plan_track(rails, points, shapes=None, z=None):
    """The rails that lay a track along `points` (rail-network points, see track_objects)
    next to the existing `rails` (x, y, shape, z0, z1, see rail_of):
    [(a, b, x, y, shape, end0, end1), ...], or TrackError. `z` is the height of the track's
    rail points (its rails' z + RAIL_ALTITUDE): it joins only the rails whose points are at
    that height; None takes the rails at any height. The rules of the engine's linking
    (Rail_Action) - the same in the in-game editor (src/LocoFix/rail_rules.h)."""
    hx, hy = RAIL_HALF

    def heading(a, b):
        return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))

    segments = []
    previous = None
    second = None
    # end vectors a rail may have at its first point: opposite to the rails ending there
    entry = {(-e[0], -e[1]) for e in rail_ends_at(rails, points[0], z)} or None
    for n, (a, b) in enumerate(zip(points, points[1:])):
        candidates = rail_candidates(a, b)
        if not candidates:
            raise TrackError("step", n, f"step {n} ({a} -> {b}): a step must be 71 in x and 50 "
                                        f"in y (or halves of them)")
        if entry is not None:
            joined = [c for c in candidates if c[3] in entry]
            if not joined:
                raise TrackError("join", n, f"step {n} ({a} -> {b}): no rail here joins the rail "
                                            f"before it - the engine links two rails only where "
                                            f"their ends point opposite ways, so a turn needs a "
                                            f"curve (a step of 71, 25 or 35.5, 50)")
            candidates = joined
        if shapes is not None:
            pick = [c for c in candidates if c[2] == shapes[n]]
            if not pick:
                raise TrackError("step", n, f"step {n} ({a} -> {b}): no rail of shape {shapes[n]}, "
                                            f"the shapes here are {sorted({c[2] for c in candidates})}")
            x, y, shape, _first, second = pick[0]
        else:
            x, y, shape, _first, second = candidates[0]
        entry = {(-second[0], -second[1])}
        h = heading(a, b)
        if previous is not None:
            turn = (h - previous + 180) % 360 - 180
            if abs(turn) > 45.0:
                raise TrackError("sharp", n, f"step {n} ({a} -> {b}): the track turns "
                                             f"{turn:.1f} deg, sharper than 45 deg (0x20 of 256) "
                                             f"- trains would reverse")
        previous = h
        if any(abs(rx - x) < 0.01 and abs(ry - y) < 0.01 and rs == shape and
               (z is None or (r0, r1) == (z, z)) for rx, ry, rs, r0, r1 in rails):
            raise TrackError("duplicate", n, f"step {n} ({a} -> {b}): this rail is already there")
        end0 = (round(x + RAIL_ENDS[shape][0][0] * hx, 3),
                round(y + RAIL_ENDS[shape][0][1] * hy, 3))
        end1 = (round(x + RAIL_ENDS[shape][1][0] * hx, 3),
                round(y + RAIL_ENDS[shape][1][1] * hy, 3))
        segments.append((a, b, x, y, shape, end0, end1))
    meeting = rail_ends_at(rails, points[-1], z)
    if meeting and (-second[0], -second[1]) not in meeting:
        raise TrackError("last", len(points) - 2,
                         f"the last rail ({points[-2]} -> {points[-1]}) does not join the rails "
                         f"ending at {points[-1]} - their ends must point opposite ways")

    # no rail point may get more links than the engine allows: at every joint of the track,
    # with the rails already there (rail_point_links)
    level = 0 if z is None else z     # None: the rails at any height as if at one
    laid = [(x, y, shape, z0 if z is not None else level, z1 if z is not None else level)
            for x, y, shape, z0, z1 in rails]
    laid += [(x, y, shape, level, level) for _a, _b, x, y, shape, *_e in segments]
    over = {}
    for segment in segments:
        for point in segment[5:7]:
            for (end, height), links in rail_point_links(laid, point).items():
                if height == level and links > MAX_RAIL_LINKS:
                    over[(point, end)] = links
    if over:
        raise TrackError("links", -1, "rail points with more than 6 links (engine limit): " +
                                      ", ".join(f"{p} {e} -> {c}" for (p, e), c in sorted(over.items())))
    return segments


def track_cells(segments, width, height):
    """The grid cells a track crosses and the one-cell buffer around them (two sets of
    (column, row)): every point of each segment a pixel apart, as track_objects flattens."""
    cells = set()
    for a, b, *_rest in segments:
        steps = max(abs(b[0] - a[0]), abs(b[1] - a[1]))
        for i in range(int(steps) + 1):
            t = i / steps if steps else 0.0
            cx = int(a[0] + (b[0] - a[0]) * t) // GRID_CELL
            cy = int(a[1] + (b[1] - a[1]) * t) // GRID_CELL
            if 0 <= cx < width and 0 <= cy < height:
                cells.add((cx, cy))
    buffer = set()
    for cx, cy in cells:
        for dy in (-1, 0, 1):
            for dx in (-1, 0, 1):
                nx, ny = cx + dx, cy + dy
                if 0 <= nx < width and 0 <= ny < height and (nx, ny) not in cells:
                    buffer.add((nx, ny))
    return cells, buffer


def track_problems(rails):
    """The joints of a map's rails (x, y, shape, z0, z1; ramps included, see rail_of) where
    the engine's network is not what the track looks like: the same rail twice (it only
    shares the points of the first, RailMap_AddDot), a rail point with more than 6 links
    (rail_point_links: the engine drops the others, "Too many links"), and a joint where
    several rails end but some rail end has no rail ending the opposite way at the height of
    its point - there the track ends for the trains (a dead end of two rails, or rails at
    different heights that do not link). A rail end alone is an end of the track.
    [(point, text), ...]."""
    hx, hy = RAIL_HALF
    seen = Counter((round(x, 3), round(y, 3), shape, z0, z1) for x, y, shape, z0, z1 in rails)
    problems = [((x, y), f"rail of shape {shape} at {x:g},{y:g} (points at {z0}, {z1}) {count} times")
                for (x, y, shape, z0, z1), count in sorted(seen.items()) if count > 1]
    unique = list(seen)
    joints = {}
    for rail in unique:
        for end in RAIL_ENDS[rail[2]]:
            point = (round(rail[0] + end[0] * hx, 3), round(rail[1] + end[1] * hy, 3))
            joints[point] = joints.get(point, 0) + 1
    for point, count in sorted(joints.items()):
        points = rail_point_links(unique, point)
        for (end, height), links in sorted(points.items()):
            if links > MAX_RAIL_LINKS:
                problems.append((point, f"the rail point of the ends {end} at height {height} has "
                                        f"{links} links (more than {MAX_RAIL_LINKS}: 'Too many links')"))
        if count > 1:
            for (end, height) in sorted(points):
                if ((-end[0], -end[1]), height) in points:
                    continue
                others = sorted(h for (e, h) in points if e == (-end[0], -end[1]))
                if others:
                    text = (f"rail ends {end} at height {height} meet the opposite ends at height "
                            f"{', '.join(map(str, others))}: they do not link")
                else:
                    text = (f"rail ends {end} at height {height} have no rail ending the opposite "
                            f"way: the track ends there")
                problems.append((point, text))
    return problems


def check_track(path, objects_res=None, nvid=RAIL_NVID):
    """map_tool.py check-track <map>: the track rules on every rail of a .map (or an
    unpacked folder). Returns the problems (printed by main)."""
    if os.path.isdir(path):
        model = load_map(path)
    else:
        with open(path, "rb") as f:
            model = read_map(f.read(), object_info(objects_res or find_objects_res(path)))
    return track_problems(map_rails(model, nvid))


def track_objects(folder, points, nvid=610, z=None, clear=False, shapes=None,
                  objects_res=None):
    """Lays a rail (VID 610 by default) along a path of rail-network points on an unpacked
    map: the points are the places where two rails meet, the ends of the track. Every step
    must be one step of the isometric rail lattice - 71 px in x and 50 px in y, or their
    halves (35.5 / 25) - so a straight run is a list of points 71, -50 apart and a bend is
    made of the curve shapes. Each rail must join the previous one the way the engine links
    rails (see RAIL_ENDS): its end at the joint points opposite to the previous rail's end,
    so a straight run turns only through a curve (e.g. 0,0 71,0 142,-25 213,-75: a
    horizontal rail, a curve, a diagonal). The first and the last point join the rails
    already there by the same rule, which lets a branch start from a track. Consecutive
    steps may not turn sharper than 45 degrees (0x20 of 256): sharper turns force the
    engine's trains into reversals (RailDot_HasRoomToReverse), and no rail end may meet
    more than six others (the engine's "Too many links" limit). The shape of each step is
    the one that joins; `shapes` picks it by hand. The height is the ground of the rail
    minus 4, where the game's own maps put their rails, unless `z` is given. `clear`
    removes the old rails of the same VID first. The track flattens the terrain: every
    8x8 grid cell crossed by a rail takes the track height, and the one-cell buffer around
    the track is ramped gently toward it, so there are no vertical cliffs under the track.
    Returns the number of rails added."""
    model = load_map(folder)
    if "objects" not in model:
        raise DataError(f"{folder}: no objects")
    if len(points) < 2:
        raise DataError("a track needs at least 2 points")
    if shapes is not None and len(shapes) != len(points) - 1:
        raise DataError(f"--shapes needs {len(points) - 1} shapes, one per step")
    if clear:
        model["objects"] = [row for row in model["objects"]
                            if parse_int(row["nvid"]) != nvid]
    info = object_info(objects_res or find_objects_res(folder))
    name = info.get(nvid, ("", ""))[0]
    ids = [parse_int(row["id"]) for row in model["objects"]]
    object_id = max(ids) + 1 if ids else 1
    width, height = grid_size(model["info"])
    grid = model.get("grid")
    if grid is None:
        raise DataError(f"{folder}: no grid (the map needs GRID)")

    rails = map_rails(model, nvid)
    # the track height: given with --z, otherwise that of the rail point it starts from
    # (continuing a track), otherwise the ground under the first rail. The rail sits 4 px
    # below the ground surface, so the surface is rail_z + 4.
    if z is not None:
        track_level = z + 4
    else:
        start = [h for rail in rails for _e, h, _f in _ends_at(rail, points[0])]
        if start:
            track_level = start[0] - RAIL_ALTITUDE + 4
        else:
            first = plan_track(rails, points, shapes)[0]
            track_level = grid[int(first[3]) // 8][int(first[2]) // 8]
    segments = plan_track(rails, points, shapes, track_level - 4 + RAIL_ALTITUDE)

    # flatten: every grid cell crossed by a rail takes the track height, and the
    # one-cell buffer around the track is ramped gently toward it
    track_cells_, buffer = track_cells(segments, width, height)
    new_grid = [row[:] for row in grid]
    for cx, cy in track_cells_:
        new_grid[cy][cx] = track_level
    for cx, cy in buffer:
        original = grid[cy][cx]
        new_grid[cy][cx] = original + (track_level - original) // 2
    model["grid"] = new_grid

    added = 0
    for a, b, x, y, shape, *_rest in segments:
        model["objects"].append({"id": str(object_id), "nvid": str(nvid), "name": name,
                                 "x": f"{x:g}", "y": f"{y:g}", "z": str(track_level - 4),
                                 "direction": str(RAIL_DIRECTION[shape]), "army": "0",
                                 "commands": "", "behave": "", "items": "", "data": ""})
        object_id += 1
        added += 1
    save_map(model, folder)
    return added


def copy_objects(folder, source, nvid, objects_res=None):
    """Adds to an unpacked map (the folder) every object of one VID of another map: the
    rails of a map into a new map on the same terrain, for example. The objects keep their
    positions, directions, armies and state, get free ids, and lose their commands (they
    are the orders of the source mission). Returns the number of objects added."""
    model = load_map(folder)
    if "objects" not in model:
        raise DataError(f"{folder}: no objects")
    info = object_info(objects_res or find_objects_res(folder))
    other = read_map(open(source, "rb").read(), info)
    rows = [r for r in other.get("objects", []) if parse_int(r["nvid"]) == nvid]
    if not rows:
        raise DataError(f"{source}: no object {nvid}")
    ids = [parse_int(row["id"]) for row in model["objects"]]
    object_id = max(ids) + 1 if ids else 1
    for r in rows:
        model["objects"].append({"id": str(object_id), "nvid": str(nvid), "name": r["name"],
                                 "x": r["x"], "y": r["y"], "z": r["z"],
                                 "direction": r["direction"], "army": r["army"],
                                 "commands": "", "behave": r["behave"],
                                 "items": r["items"], "data": r["data"]})
        object_id += 1
    save_map(model, folder)
    return len(rows)


def apply_map_overrides(model, folder, info):
    """Applies objects.tsv of an override folder to the model. Returns the number of
    changed cells (an added or removed object counts as one)."""
    path = os.path.join(folder, "objects.tsv")
    if not os.path.isfile(path):
        return 0
    if "objects" not in model:
        raise DataError(f"{path}: the map has no readable objects")
    columns, rows = load_tsv(path)
    if "id" not in columns:
        raise DataError(f"{path}: the key column id is missing")
    for c in columns:
        if c not in OBJECT_COLUMNS and c != "remove":
            raise DataError(f"{path}: no column {c} in objects.tsv")
    objects = model["objects"]
    index = {r["id"].strip(): r for r in objects}
    changed = 0
    for n, row in rows:
        key = row["id"].strip()
        target = index.get(key)
        if row.get("remove", "").strip() not in ("", "0"):
            if target is None:
                raise DataError(f"{path}:{n}: no object {key} to remove")
            objects.remove(target)
            del index[key]
            changed += 1
            continue
        if target is None:
            if not row.get("nvid", "").strip():
                raise DataError(f"{path}:{n}: a new object {key} needs nvid")
            nvid = parse_int(row["nvid"])
            target = {c: "0" for c in OBJECT_COLUMNS}
            target.update(id=key, name="", commands="", items="", data="",
                          behave="0" if info.get(nvid, ("", ""))[1] in UNIT_CLASSES else "")
            objects.append(target)
            index[key] = target
            changed += 1
        for c in columns:
            if c not in ("id", "remove", "name") and row[c] != "" and target[c] != row[c]:
                target[c] = row[c]
                changed += 1
        object_entry(target, spr_format(model), f"{path}:{n}")     # checks the values
    return changed


def apply_map(path, folder, output, objects_res=None):
    """Writes `output`: the map `path` with the overrides from `folder`. Returns the number
    of changed values."""
    model = read_map(open(path, "rb").read(), object_info(objects_res or find_objects_res(path)))
    changed = apply_map_overrides(model, folder, model["names"])
    out = write_map(model)
    if not os.path.isfile(output) or open(output, "rb").read() != out:
        os.makedirs(os.path.dirname(os.path.abspath(output)), exist_ok=True)
        open(output, "wb").write(out)
    return changed


# ------------------------------------------------------------------ images ----

HEAD = struct.Struct("<HHhhh")      # imageFlags, frameSpeed, frameCount, width, height
SHEET_COLUMNS = 16
# 565 and 4444 pixels: 5/6/4-bit values widened to 8 bits by repeating the top bits, so
# that taking the top bits gives the stored value back
R5 = [(v & 0xf8) | (v >> 5) for v in range(256)]                  # from the high byte
B5 = [((v & 31) << 3) | ((v & 31) >> 2) for v in range(256)]       # from the low byte


def rgb565_image(data, w, h):
    lo, hi = Image.frombytes("LA", (w, h), data).split()
    g = ImageChops.add(ImageChops.add(hi.point(lambda v: (v & 7) << 5),
                                      lo.point(lambda v: (v >> 5) << 2)),
                       hi.point(lambda v: (v & 7) >> 1))
    return Image.merge("RGB", (hi.point(R5), g, lo.point(B5)))


def image_rgb565(im):
    r, g, b = im.convert("RGB").split()
    hi = ImageChops.add(r.point(lambda v: v & 0xf8), g.point(lambda v: v >> 5))
    lo = ImageChops.add(g.point(lambda v: (v << 3) & 0xe0), b.point(lambda v: v >> 3))
    return Image.merge("LA", (lo, hi)).tobytes()


def argb4444_image(data, w, h):
    lo, hi = Image.frombytes("LA", (w, h), data).split()
    return Image.merge("RGBA", (hi.point(lambda v: (v & 15) * 17), lo.point(lambda v: (v >> 4) * 17),
                                lo.point(lambda v: (v & 15) * 17), hi.point(lambda v: (v >> 4) * 17)))


def image_argb4444(im):
    r, g, b, a = im.convert("RGBA").split()
    hi = ImageChops.add(a.point(lambda v: v & 0xf0), r.point(lambda v: v >> 4))
    lo = ImageChops.add(g.point(lambda v: v & 0xf0), b.point(lambda v: v >> 4))
    return Image.merge("LA", (lo, hi)).tobytes()


def depth_image(data, w, h):
    return Image.frombytes("I;16", (w, h), data)


def image_depth(im):
    if im.mode != "I;16":
        im = im.convert("I;16")
    return im.tobytes()


def open_image(path, size=None):
    try:
        im = Image.open(path)
        im.load()
    except OSError as e:
        raise DataError(f"{path}: {e}") from None
    if size is not None and im.size != tuple(size):
        raise DataError(f"{path}: {im.size[0]}x{im.size[1]}, expected {size[0]}x{size[1]}")
    return im


def kind_of(flags):
    if flags & 0x80:
        return "color"
    if flags & 0x20:
        return "texture"
    return "sprite"


def unpack_vid(path, folder):
    data = open(path, "rb").read()
    records, ids = read_container(data, b"VID ")
    if b"HEAD" not in records or len(records[b"HEAD"]) != 1 or len(records[b"HEAD"][0]) != HEAD.size:
        raise DataError("HEAD")
    flags, speed, count, w, h = HEAD.unpack(records[b"HEAD"][0])
    meta = {"chunks": [chunk_name(c) for c in ids], "imageFlags": "0x%x" % flags,
            "frameSpeed": speed, "frameCount": count, "width": w, "height": h,
            "kind": kind_of(flags)}
    expected = {"color": [b"HEAD", b"DATA"], "texture": [b"HEAD", b"SURF", b"DATA"],
                "sprite": [b"HEAD", b"PAL ", b"DATA"] if flags & 8 else [b"HEAD", b"DATA"]}
    if ids != expected[meta["kind"]]:
        raise DataError(f"chunks {[chunk_name(c) for c in ids]} for imageFlags {flags:#x}")
    os.makedirs(folder, exist_ok=True)
    {"color": unpack_colors, "texture": unpack_textures, "sprite": unpack_sprite}[meta["kind"]](
        records, meta, folder)
    save_json(os.path.join(folder, "vid.json"), meta)
    return meta


def pack_vid(folder):
    meta = load_json(os.path.join(folder, "vid.json"))
    flags = parse_int(meta["imageFlags"])
    records = {b"HEAD": [HEAD.pack(flags, meta["frameSpeed"], meta["frameCount"],
                                   meta["width"], meta["height"])]}
    kind = meta.get("kind", kind_of(flags))
    records.update({"color": pack_colors, "texture": pack_textures, "sprite": pack_sprite}[kind](
        meta, flags, folder))
    return write_container(b"VID ", [chunk_id(c) for c in meta["chunks"]], records)


# colour boxes


def unpack_colors(records, meta, folder):
    (rec,) = records[b"DATA"]
    if len(rec) % 4:
        raise DataError("colour DATA size")
    rows = [{"frame": str(i), "color": "0x%08x" % c}
            for i, c in enumerate(struct.unpack(f"<{len(rec) // 4}I", rec))]
    save_tsv(os.path.join(folder, "colors.tsv"), ["frame", "color"], rows)


def pack_colors(meta, flags, folder):
    rows = load_table(os.path.join(folder, "colors.tsv"), ["frame", "color"])
    return {b"DATA": [b"".join(struct.pack("<I", parse_int(r["color"]) & 0xffffffff) for r in rows)]}


# texture tiles

TILE_COLUMNS = ["tile", "unknown_00", "texture", "offsetX", "offsetY", "width", "height",
                "x", "y", "next"]


def tile_struct(flags):
    """A tile record: 9 ints with imageFlags 0x10 (GroundTile as in memory), else an int
    and 8 shorts."""
    return struct.Struct("<9i" if flags & 0x10 else "<i8h")


def texture_codec(flags):
    if flags & 8:
        raise DataError("textures with a palette (imageFlags 8) are not supported")
    if flags & 3 == 3:
        return argb4444_image, image_argb4444       # D3DFMT_A4R4G4B4
    return rgb565_image, image_rgb565               # D3DFMT_R5G6B5


def read_textures(surf, flags):
    """[(w, h, colour bytes, depth bytes or None)] of a SURF record."""
    n, = struct.unpack_from("<h", surf, 0)
    o, out, step = 2, [], 2 if flags & 4 else 1
    if n % step:
        raise DataError("odd texture count with height textures")
    for _ in range(n // step):
        w, h, size = struct.unpack_from("<hhI", surf, o)
        o += 8
        if size != w * h * 2:
            raise DataError(f"texture {len(out)}: {size} bytes for {w}x{h}")
        colour, o = surf[o:o + size], o + size
        depth = None
        if flags & 4:
            zsize, = struct.unpack_from("<I", surf, o)
            if zsize != w * h * 2:
                raise DataError(f"height texture {len(out)}: {zsize} bytes for {w}x{h}")
            depth, o = surf[o + 4:o + 4 + zsize], o + 4 + zsize
        out.append((w, h, colour, depth))
    if o != len(surf):
        raise DataError("SURF: bytes after the textures")
    return out


def ground_textures(tiles, flags, count):
    """The texture (index into the colour textures) each tile of a ground shows: its
    texture field - over 2 when every colour texture is followed by its height (imageFlags
    4). The engine draws a tile with the texture its field names, so the picture of tile 0
    of the game's grounds is in the texture `count / 2` and that of tile count / 2 in
    texture 0 (the original converter sorted them). None unless every texture is used once."""
    order = [t[1] // 2 if flags & 4 else t[1] for t in tiles]
    return order if sorted(order) == list(range(count)) else None


def ground_layout(meta, textures, tiles, flags):
    """True when the image is one frame that its textures cover exactly, every tile showing
    a whole texture of its own (map grounds): then it is saved as one picture."""
    if meta["frameCount"] != 1 or len(tiles) != len(textures) or not tiles:
        return False
    order = ground_textures(tiles, flags, len(textures))
    if order is None:
        return False
    covered = 0
    for k, t in zip(order, tiles):
        w, h = textures[k][0], textures[k][1]
        if (t[2], t[3], t[4], t[5]) != (0, 0, w, h) or t[6] < 0 or t[7] < 0 \
                or t[6] + w > meta["width"] or t[7] + h > meta["height"]:
            return False
        covered += w * h
    if covered != meta["width"] * meta["height"]:
        return False
    seen = set()                        # no overlaps: tiles on a grid of their own size
    for k, t in zip(order, tiles):
        w, h = textures[k][0], textures[k][1]
        for yy in range(t[7], t[7] + h, 8):
            for xx in range(t[6], t[6] + w, 8):
                if (xx, yy) in seen:
                    return False
                seen.add((xx, yy))
    return True


def unpack_textures(records, meta, folder):
    flags = parse_int(meta["imageFlags"])
    to_image, _ = texture_codec(flags)
    (surf,) = records[b"SURF"]
    textures = read_textures(surf, flags)
    (data,) = records[b"DATA"]
    ts = tile_struct(flags)
    if len(data) % ts.size:
        raise DataError("tile table size")
    tiles = [ts.unpack_from(data, k) for k in range(0, len(data), ts.size)]
    save_tsv(os.path.join(folder, "tiles.tsv"), TILE_COLUMNS, [
        dict(zip(TILE_COLUMNS, [str(i), "0x%08x" % (t[0] & 0xffffffff)] + [str(v) for v in t[1:]]))
        for i, t in enumerate(tiles)])
    meta["textures"] = len(textures)
    meta["ground"] = ground_layout(meta, textures, tiles, flags)
    if meta["ground"]:
        size = (meta["width"], meta["height"])
        picture = Image.new("RGBA" if flags & 3 == 3 else "RGB", size)
        heights = Image.new("I;16", size) if flags & 4 else None
        for k, t in zip(ground_textures(tiles, flags, len(textures)), tiles):
            w, h, colour, depth = textures[k]
            picture.paste(to_image(colour, w, h), (t[6], t[7]))
            if heights is not None:
                heights.paste(depth_image(depth, w, h), (t[6], t[7]))
        picture.save(os.path.join(folder, "ground.png"))
        if heights is not None:
            heights.save(os.path.join(folder, "height.png"))
        return
    for k, (w, h, colour, depth) in enumerate(textures):
        to_image(colour, w, h).save(os.path.join(folder, "texture_%03d.png" % k))
        if depth is not None:
            depth_image(depth, w, h).save(os.path.join(folder, "height_%03d.png" % k))


def pack_textures(meta, flags, folder):
    _, from_image = texture_codec(flags)
    rows = load_table(os.path.join(folder, "tiles.tsv"), TILE_COLUMNS)
    tiles = [[parse_int(r["unknown_00"])] + [parse_int(r[c]) for c in TILE_COLUMNS[2:]] for r in rows]
    ts = tile_struct(flags)
    try:
        data = b"".join(ts.pack(t[0] - (1 << 32) if t[0] >= 1 << 31 else t[0], *t[1:]) for t in tiles)
    except struct.error as e:
        raise DataError(f"tiles.tsv: {e}") from None
    count = meta["textures"]
    parts = []
    if meta.get("ground"):
        picture = open_image(os.path.join(folder, "ground.png"), (meta["width"], meta["height"]))
        heights = open_image(os.path.join(folder, "height.png"), picture.size) if flags & 4 else None
        order = ground_textures(tiles, flags, count)
        if len(tiles) != count or order is None:
            raise DataError("tiles.tsv: a ground needs one tile per texture, the texture "
                            "fields naming every texture once")
        parts = [None] * count
        for k, t in zip(order, tiles):
            box = (t[6], t[7], t[6] + t[4], t[7] + t[5])
            parts[k] = (picture.crop(box), heights.crop(box) if heights else None)
    else:
        for k in range(count):
            im = open_image(os.path.join(folder, "texture_%03d.png" % k))
            hm = open_image(os.path.join(folder, "height_%03d.png" % k), im.size) if flags & 4 else None
            parts.append((im, hm))
    surf = bytearray(struct.pack("<h", count * (2 if flags & 4 else 1)))
    for im, hm in parts:
        colour = from_image(im)
        surf += struct.pack("<hhI", im.width, im.height, len(colour)) + colour
        if hm is not None:
            depth = image_depth(hm)
            surf += struct.pack("<I", len(depth)) + depth
    return {b"SURF": [bytes(surf)], b"DATA": [data]}


# RLE sprites

FRAME_COLUMNS = ["frame", "reference", "shadow", "raw"]
RUNS = re.compile(rb"\x01+")


def parse_frame(rec, pixel):
    """(shadow points, top, [(row, x, count, offset of the pixels)]) of a frame record, or
    None when it is not a complete frame. `pixel` = bytes per pixel."""
    try:
        size, n = struct.unpack_from("<Ih", rec, 0)
        if size != len(rec) - 4 or n < 0:
            return None
        o = 6 + 6 * n
        shadow = [struct.unpack_from("<3h", rec, 6 + 6 * k) for k in range(n)]
        top, height = struct.unpack_from("<hh", rec, o)
        o += 4
        if top < 0 or height < 0:
            return None
        runs = []
        for y in range(height):
            x = 0
            while True:
                skip, count = rec[o], rec[o + 1]
                o += 2
                if skip == 0 and count == 0:
                    break
                x += skip
                if count:
                    runs.append((top + y, x, count, o))
                o += count * pixel
                x += count
        if o != len(rec) or height and (not runs or runs[0][0] != top or runs[-1][0] != top + height - 1):
            return None
        if height == 0 and top != 0:
            return None
        return shadow, top, runs
    except (struct.error, IndexError):
        return None


def unpack_sprite(records, meta, folder):
    flags = parse_int(meta["imageFlags"])
    palette = records[b"PAL "][0] if flags & 8 else None
    depth = bool(flags & 4)
    if depth and not flags & 8:
        raise DataError("height data without a palette is not supported")
    pixel = (1 if palette else 2) + (2 if depth else 0)
    frames = records[b"DATA"]
    rows, parsed = [], []
    cw, ch = max(meta["width"], 1), max(meta["height"], 1)
    used = set()
    for i, rec in enumerate(frames):
        row = {"frame": str(i), "reference": "", "shadow": "", "raw": ""}
        p = None
        if len(rec) == 6 and struct.unpack_from("<I", rec, 0)[0] == 2:
            row["reference"] = str(struct.unpack_from("<h", rec, 4)[0])
        else:
            p = parse_frame(rec, pixel)
            if p is None:
                row["raw"] = rec.hex()
            else:
                row["shadow"] = " ".join(",".join(str(v) for v in pt) for pt in p[0])
                for y, x, count, o in p[2]:
                    cw, ch = max(cw, x + count), max(ch, y + 1)
                    if palette:
                        used.update(rec[o + 2 * count * depth:o + (2 * depth + 1) * count])
        rows.append(row)
        parsed.append(p)
    columns = max(1, min(SHEET_COLUMNS, len(frames)))
    sw, sh = columns * cw, max(1, math.ceil(len(frames) / columns)) * ch
    meta["sheet"] = {"columns": columns, "cellWidth": cw, "cellHeight": ch}
    if palette:
        free = [i for i in range(256) if i not in used]
        tindex = free[-1] if free else None
        meta["transparentIndex"] = tindex
        index = bytearray([tindex or 0]) * (sw * sh)
        mask = bytearray(sw * sh) if tindex is None else None
        heights = bytearray(2 * sw * sh) if depth else None
    else:
        rgba = bytearray(4 * sw * sh)
        colours = [bytes([R5[v >> 8], ((v >> 5) & 63) << 2 | ((v >> 5) & 63) >> 4, B5[v & 255], 255])
                   for v in range(65536)]
    for i, (rec, p) in enumerate(zip(frames, parsed)):
        if p is None:
            continue
        x0, y0 = (i % columns) * cw, (i // columns) * ch
        for y, x, count, o in p[2]:
            base = (y0 + y) * sw + x0 + x
            if not palette:
                rgba[4 * base:4 * (base + count)] = b"".join(
                    colours[v] for v in struct.unpack_from(f"<{count}H", rec, o))
                continue
            if depth:
                heights[2 * base:2 * (base + count)] = rec[o:o + 2 * count]
                o += 2 * count
            index[base:base + count] = rec[o:o + count]
            if mask is not None:
                mask[base:base + count] = b"\xff" * count
    save_tsv(os.path.join(folder, "frames.tsv"), FRAME_COLUMNS, rows)
    if palette:
        alpha = len(palette) == 1024
        entries = [palette[i * 4:i * 4 + 4] for i in range(256)] if alpha else \
            [palette[i * 3:i * 3 + 3] for i in range(256)]
        if alpha:                                       # B, G, R, A
            prow = [{"index": str(i), "red": str(e[2]), "green": str(e[1]), "blue": str(e[0]),
                     "alpha": str(e[3])} for i, e in enumerate(entries)]
        else:
            prow = [{"index": str(i), "red": str(e[0]), "green": str(e[1]), "blue": str(e[2])}
                    for i, e in enumerate(entries)]
        save_tsv(os.path.join(folder, "palette.tsv"),
                 ["index", "red", "green", "blue"] + (["alpha"] if alpha else []), prow)
        im = Image.frombytes("P", (sw, sh), bytes(index))
        im.putpalette(b"".join(bytes([int(r["red"]), int(r["green"]), int(r["blue"])]) for r in prow))
        if tindex is not None:
            im.save(os.path.join(folder, "frames.png"), transparency=tindex)
        else:
            im.save(os.path.join(folder, "frames.png"))
            Image.frombytes("L", (sw, sh), bytes(mask)).save(os.path.join(folder, "mask.png"))
        if depth:
            depth_image(bytes(heights), sw, sh).save(os.path.join(folder, "depth.png"))
    else:
        Image.frombytes("RGBA", (sw, sh), bytes(rgba)).save(os.path.join(folder, "frames.png"))


def pack_sprite(meta, flags, folder):
    out = {}
    depth = bool(flags & 4)
    sheet = meta["sheet"]
    columns, cw, ch = sheet["columns"], sheet["cellWidth"], sheet["cellHeight"]
    rows = load_table(os.path.join(folder, "frames.tsv"), FRAME_COLUMNS)
    sw = columns * cw
    sh = max(1, math.ceil(len(rows) / columns)) * ch
    picture = open_image(os.path.join(folder, "frames.png"), (sw, sh))
    if flags & 8:
        prow = load_table(os.path.join(folder, "palette.tsv"), ["index", "red", "green", "blue"])
        if len(prow) != 256:
            raise DataError("palette.tsv: needs 256 entries")
        if "alpha" in prow[0]:
            out[b"PAL "] = [b"".join(bytes([int(r["blue"]), int(r["green"]), int(r["red"]),
                                            int(r["alpha"])]) for r in prow)]
        else:
            out[b"PAL "] = [b"".join(bytes([int(r["red"]), int(r["green"]), int(r["blue"])]) for r in prow)]
        if picture.mode != "P":
            raise DataError("frames.png: must stay an indexed (palette) image")
        index = picture.tobytes()
        tindex = meta.get("transparentIndex")
        if tindex is None:
            mask = open_image(os.path.join(folder, "mask.png"), (sw, sh)).convert("L").tobytes()
            mask = mask.translate(bytes([0] + [1] * 255))
        else:
            mask = index.translate(bytes(0 if i == tindex else 1 for i in range(256)))
        heights = image_depth(open_image(os.path.join(folder, "depth.png"), (sw, sh))) if depth else None
    else:
        rgba = picture.convert("RGBA").tobytes()
        mask = rgba[3::4].translate(bytes([0] + [1] * 255))
    frames = []
    for i, r in enumerate(rows):
        if r["reference"].strip():
            frames.append(struct.pack("<Ih", 2, parse_int(r["reference"])))
            continue
        if r["raw"].strip():
            frames.append(bytes.fromhex(r["raw"]))
            continue
        x0, y0 = (i % columns) * cw, (i // columns) * ch
        try:
            shadow = [[parse_int(v) for v in pt.split(",")] for pt in r["shadow"].split()]
            if any(len(pt) != 3 for pt in shadow):
                raise ValueError("a shadow point is x,y,z")
        except ValueError as e:
            raise DataError(f"frames.tsv: frame {i}, shadow: {e}") from None
        lines = []
        for y in range(ch):
            start = (y0 + y) * sw + x0
            lines.append([m.span() for m in RUNS.finditer(mask, start, start + cw)])
        drawn = [y for y, spans in enumerate(lines) if spans]
        top = drawn[0] if drawn else 0
        height = drawn[-1] - top + 1 if drawn else 0
        body = bytearray(struct.pack("<h", len(shadow)))
        for pt in shadow:
            body += struct.pack("<3h", *pt)
        body += struct.pack("<hh", top, height)
        for y in range(top, top + height):
            start = (y0 + y) * sw + x0
            x = start
            for a, b in lines[y]:
                skip = a - x
                while skip > 255:
                    body += b"\xff\x00"
                    skip -= 255
                while a < b:
                    count = min(255, b - a)
                    body += bytes([skip, count])
                    if flags & 8:
                        if depth:
                            body += heights[2 * a:2 * (a + count)]
                        body += index[a:a + count]
                    else:
                        for p in range(a, a + count):
                            red, green, blue = rgba[4 * p:4 * p + 3]
                            body += struct.pack("<H", (red >> 3) << 11 | (green >> 2) << 5 | blue >> 3)
                    skip, a = 0, a + count
                x = b
            body += b"\0\0"
        frames.append(struct.pack("<I", len(body)) + body)
    out[b"DATA"] = frames
    return out


# ------------------------------------------------------------------- check ----


def file_kind(path):
    with open(path, "rb") as f:
        head = f.read(12)
    if head[:4] != b"RES ":
        return None
    return {b"MAP ": "map", b"VID ": "vid"}.get(head[8:12])


def unpack(path, folder, objects_res=None):
    kind = file_kind(path)
    if kind == "map":
        return unpack_map(path, folder, objects_res)
    if kind == "vid":
        return unpack_vid(path, folder)
    raise DataError(f"{path}: not a .map or .vid file (RES type MAP or VID)")


def pack(folder, path):
    if os.path.isfile(os.path.join(folder, "map.json")):
        data = write_map(load_map(folder))
    elif os.path.isfile(os.path.join(folder, "vid.json")):
        data = pack_vid(folder)
    else:
        raise DataError(f"{folder}: no map.json or vid.json")
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    open(path, "wb").write(data)
    return data


def round_trip(path, work, objects_res):
    """None when unpacking and packing `path` through the disk gives the same bytes,
    otherwise the reason."""
    folder = tempfile.mkdtemp(dir=work)
    try:
        unpack(path, folder, objects_res)
        if pack(folder, os.path.join(folder, "packed")) != open(path, "rb").read():
            return "packed file differs"
        return None
    except (DataError, OSError, ValueError, KeyError, struct.error) as e:
        return f"{type(e).__name__}: {e}"
    finally:
        shutil.rmtree(folder, ignore_errors=True)


def check(game):
    """Round trip of every .map and .vid of a game folder. Returns the number of failures."""
    files = []
    for directory, _, names in os.walk(game):
        for n in names:
            p = os.path.join(directory, n)
            if n.lower().endswith((".map", ".vid")) and file_kind(p):
                files.append(p)
    objects_res = os.path.join(game, "objects.res")
    objects_res = objects_res if os.path.isfile(objects_res) else None
    work = tempfile.mkdtemp(prefix="map_tool_check_")
    failed = 0
    try:
        with concurrent.futures.ProcessPoolExecutor() as pool:
            results = pool.map(round_trip, files, [work] * len(files), [objects_res] * len(files))
            for p, problem in zip(files, results):
                if problem:
                    failed += 1
                    print(f"FAILED {safe_relpath(p, game)}: {problem}")
    finally:
        shutil.rmtree(work, ignore_errors=True)
    maps = sum(f.lower().endswith(".map") for f in files)
    print(f"{len(files) - failed} of {len(files)} files ({maps} maps, {len(files) - maps} images) "
          f"round-trip byte for byte")
    return failed


def main(argv):
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(errors="replace")     # the names are in the game's encoding
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0],
                                formatter_class=argparse.RawDescriptionHelpFormatter,
                                epilog=__doc__.split("\n\n")[1])
    p.add_argument("command", choices=["unpack", "pack", "new", "ground", "paint", "apply", "check",
                                       "list", "vids", "place", "copy", "delete", "track",
                                       "check-track"])
    p.add_argument("paths", nargs="+")
    p.add_argument("--objects", help="objects.res for object names and classes")
    p.add_argument("--vid", help="new: image file of the ground (a game path)")
    p.add_argument("--script", action="store_true", help="new: also write <folder name>.lgc")
    p.add_argument("--size", help="new: size of the map, e.g. 2048x1536")
    p.add_argument("--from", dest="reference",
                   help="place: map to copy the state of the object from")
    p.add_argument("--army", type=int, default=0, help="place: army of the object")
    p.add_argument("--direction", type=int, default=0, help="place: direction of the object")
    p.add_argument("--id", type=int, help="place: id of the object (a free one by default)")
    p.add_argument("--z", type=int, help="place: height of the object (the ground by default)")
    p.add_argument("--grep", help="vids: only the objects with this text in the name")
    p.add_argument("--items", help="place: build list of an object (a depot), e.g. 5 10 20 25")
    p.add_argument("--clear", action="store_true", help="track: remove the old rails first")
    p.add_argument("--shapes", help="track: the sprite shape of each step, e.g. 2,2,3")
    p.add_argument("--rail", type=int, default=610, help="track: VID of the rail (610)")
    a = p.parse_args(argv)
    counts = {"unpack": 2, "pack": 2, "new": 2, "ground": 3, "paint": 2, "apply": 3, "check": 1,
              "list": 1, "vids": 1, "place": 4, "copy": 3, "delete": 2, "track": 2,
              "check-track": 1}
    if a.command == "track":
        if len(a.paths) < 3:
            p.error("track takes a folder and at least 2 points, e.g. 141.5,750 212.5,700")
    elif len(a.paths) != counts[a.command]:
        p.error(f"{a.command} takes {counts[a.command]} paths")
    size = None
    if a.size:
        match = re.fullmatch(r"(\d+)[xX](\d+)", a.size)
        if not match:
            p.error("--size is a size like 2048x1536")
        size = (int(match.group(1)), int(match.group(2)))
    try:
        if a.command == "unpack":
            result = unpack(a.paths[0], a.paths[1], a.objects)
            if "objects" in result:
                print(f"{len(result['objects'])} objects, {len(result.get('groups', []))} groups")
            elif "kind" in result:
                print(f"{result['kind']}: {result['frameCount']} frames, {result['width']}x{result['height']}")
        elif a.command == "pack":
            pack(a.paths[0], a.paths[1])
        elif a.command == "new":
            model = new_map(a.paths[1], a.paths[0], a.objects, a.vid, a.script, size)
            w, h = grid_size(model["info"])
            print(f"new map {w * 8}x{h * 8}: the terrain of {a.paths[1]}")
        elif a.command == "ground":
            meta = new_ground(a.paths[0], int(a.paths[1]), int(a.paths[2]))
            print(f"ground {meta['width']}x{meta['height']}: {meta['textures']} tiles")
        elif a.command == "paint":
            print(f"painted: the gravel bed under {paint_map(a.paths[0], a.paths[1])} rails")
        elif a.command == "apply":
            print(f"{apply_map(*a.paths, a.objects)} values changed")
        elif a.command == "list":
            list_map(a.paths[0], a.objects)
        elif a.command == "vids":
            for nvid, name, sprite_class in catalog(a.paths[0]):
                if a.grep and a.grep.lower() not in name.lower() and a.grep != str(nvid):
                    continue
                print(f"{nvid:>5} {sprite_class:<12} {name}")
        elif a.command == "place":
            row = place_object(a.paths[0], int(a.paths[1]), int(a.paths[2]), int(a.paths[3]),
                               a.objects, a.army, a.direction, a.z, a.id, a.reference,
                               a.items)
            print(f"placed nvid {row['nvid']} ({row['name']}) at ({row['x']}, {row['y']}, "
                  f"{row['z']}) as id {row['id']}")
        elif a.command == "copy":
            print(f"copied {copy_objects(a.paths[0], a.paths[1], int(a.paths[2]), a.objects)}"
                  f" objects of nvid {a.paths[2]}")
        elif a.command == "delete":
            print(f"deleted {delete_objects(a.paths[0], int(a.paths[1]))} objects of nvid "
                  f"{a.paths[1]}")
        elif a.command == "check-track":
            problems = check_track(a.paths[0], a.objects, a.rail)
            for point, text in problems:
                print(f"{point[0]:g},{point[1]:g}: {text}")
            print(f"{len(problems)} track problems")
            return 1 if problems else 0
        elif a.command == "track":
            points = []
            for text in a.paths[1:]:
                try:
                    x, y = text.split(",")
                    points.append((float(x), float(y)))
                except ValueError:
                    p.error(f"track: {text} is not a point like 141.5,750")
            shapes = [int(s) for s in a.shapes.split(",")] if a.shapes else None
            print(f"laid {track_objects(a.paths[0], points, a.rail, a.z, a.clear, shapes, a.objects)}"
                  f" rails")
        else:
            sys.exit(1 if check(a.paths[0]) else 0)
    except DataError as e:
        sys.exit(f"error: {e}")


if __name__ == "__main__":
    main(sys.argv[1:])
