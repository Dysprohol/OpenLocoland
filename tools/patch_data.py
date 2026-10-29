"""Fixes to the game data files applied during the build.

vid\\110.vid, the fort cannon: the birth animation is missing for 12 of 32 directions;
fix_fort_cannon fills them with frames the file already has (details there).

objects.res, SFX section: 24 sounds point to files that do not exist in any release
(the Polish boxed one from 2003, Steam PL, Steam EN). At startup the engine looks for
them and writes errors to error.log, and the scripts of a few missions call four of them
("SOUND: Invalid nsfx"). Such names are replaced with wav\\null.wav - the empty sound the
game itself uses for "no sound". The game sounds as it did (silence); only the errors go.

Only what is really missing gets replaced: the patch checks the wav folder of the
assembled game (after the files from Steam and the repo are copied). If a missing sound
turns up there - from another release, from game/wav in the repo or dropped in by hand -
its entry stays unchanged. If the game has no null.wav either, the patch changes nothing.

vid\\450side.vid, the side tiles of the state bar: the bar (VID 450, vid\\450sbar.vid) is
1024 px wide, so on a wider screen the map shows on both sides of it. make_status_bar_sides
cuts two tiles of its own plates - the left end with its seam and the right one - into a
new image for VID 938 (game/objects.override), each as a repeated tile and as one next to
the bar, their joins cross-faded; LocoFix (status_bar.cpp) lines them up out to the edges
of the screen.

vid\\939edit.vid, the icons of the editor bar: make_editor_icons makes the icons LocoFix's
editor (editor_edit.cpp) needs and the state bar's atlas (vid\\455icon.vid) does not have -
the select arrow, track, a section to delete, the fort, the semaphores green and red, a
decal, a push line, the ground's height, a money crate, all objects - from the game's own sprites in the atlas' style, for VID 939
(game/objects.override).

objects.res layout: "RES " + size, "DATA", then chunks (4-character name + size).
SFX is the last chunk: entry count, then entries [length u32][data], where data is a
parameter byte, zero-terminated file names and parameter bytes.
"""
import os
import struct

EMPTY = b"wav\\null.wav"


def fix_objects_res(objects_res, wav_folder, output):
    """Writes to `output` a copy of `objects_res` in which sounds without a file in
    `wav_folder` point to wav\\null.wav. Returns the list of replaced names."""
    d = open(objects_res, "rb").read()
    present = {f.lower() for f in os.listdir(wav_folder)} if os.path.isdir(wav_folder) else set()
    if EMPTY.split(b"\\", 1)[1].decode() not in present:
        open(output, "wb").write(d)
        return []

    sfx = d.find(b"SFX ")
    size = struct.unpack_from("<I", d, sfx + 4)[0]
    if sfx + 8 + size != len(d):
        raise ValueError("SFX is not the last chunk of objects.res - a different data version?")
    chunk = d[sfx + 8:]
    header = chunk[:12]
    count = struct.unpack_from("<I", chunk, 8)[0]

    entries, o, replaced = [], 12, []
    for _ in range(count):
        length = struct.unpack_from("<I", chunk, o)[0]
        data = chunk[o + 4:o + 4 + length]
        o += 4 + length
        names = [n for n in data[1:].split(b"\0") if n.lower().startswith(b"wav\\")]
        for n in names:
            if n.split(b"\\", 1)[1].decode().lower() not in present:
                data = data.replace(n + b"\0", EMPTY + b"\0", 1)
                replaced.append(n.decode())
        entries.append(struct.pack("<I", len(data)) + data)
    if o != len(chunk):
        raise ValueError("unexpected end of the SFX section")

    new_chunk = header + b"".join(entries)
    delta = len(new_chunk) - len(chunk)
    result = bytearray(d[:sfx]) + b"SFX " + struct.pack("<I", size + delta) + new_chunk
    struct.pack_into("<I", result, 4, struct.unpack_from("<I", d, 4)[0] + delta)   # RES
    open(output, "wb").write(result)
    return replaced


def _chunks(d):
    """Offsets of the chunks of a RES container: {id: offset of the id}."""
    found, o = {}, 12
    while o + 8 <= len(d):
        found.setdefault(d[o:o + 4], o)
        o += 8 + (struct.unpack_from("<I", d, o + 4)[0] + 1 & ~1)
    return found


FORT_CANNON_STAND = 32           # frames 0-31: ANI_STAND, one per direction
FORT_CANNON_BIRTH = 128          # after ANI_FIGHT (3 frames x 32 directions)
FORT_CANNON_RENDERED = 20        # directions that have the birth animation


def fix_fort_cannon(vid_file, output):
    """Writes to `output` a copy of vid\\110.vid (the fort cannon, VID 110) with the
    birth animation completed. objects.res gives it 32 directions and 1 + 3 + 6 frames
    per direction (ANI_STAND, ANI_FIGHT, ANI_BIRTH = the cannon rising out of its pit),
    320 frames; the file has 249: the birth was rendered only for directions 0-19, and
    the last frame is a stray copy of frame 129. Vid_Read then reports "noCadr for
    noAnimCadr and noDir" and "14/15/16 noCadr and noAnimCadr and noDir" (animation
    numbers, not directions) and drops the birth animation for all directions, so a
    built cannon just appears.

    The missing directions 20-31 get reference records (data = the index of an earlier
    frame, which SpriteVid_LoadImage shares), no new pictures: the pit, the three
    rising frames of the nearest rendered direction (19 or 0) and twice the standing
    frame of the direction itself, so the animation ends facing the right way.
    Returns True if it patched the file; any other layout is copied unchanged."""
    d = open(vid_file, "rb").read()
    chunks = _chunks(d)
    head, data = chunks.get(b"HEAD"), chunks.get(b"DATA")
    frame_count_at = head + 8 + 12 + 4 + 4 if head is not None else None
    ok = (d[:4] == b"RES " and d[8:12] == b"VID " and head is not None and data is not None
          and struct.unpack_from("<h", d, frame_count_at)[0] == 249
          and data + 8 + struct.unpack_from("<I", d, data + 4)[0] == len(d)
          and struct.unpack_from("<III", d, data + 8) == (0x80000000, 0, 249))
    if ok:
        records, o = [], data + 20
        for _ in range(249):
            length = struct.unpack_from("<I", d, o)[0]
            records.append(d[o:o + 4 + length])
            o += 4 + length
        pit = records[FORT_CANNON_BIRTH]
        ok = (o == len(d) and records[248] == _frame_ref(FORT_CANNON_BIRTH + 1)
              and all(len(records[FORT_CANNON_BIRTH + 6 * k]) == len(pit)   # the pit
                      for k in range(FORT_CANNON_RENDERED)))
    if not ok:
        open(output, "wb").write(d)
        return False

    records = records[:248]
    for direction in range(FORT_CANNON_RENDERED, FORT_CANNON_STAND):
        nearest = (FORT_CANNON_RENDERED - 1
                   if direction - (FORT_CANNON_RENDERED - 1) <= FORT_CANNON_STAND - direction else 0)
        first = FORT_CANNON_BIRTH + 6 * nearest
        for frame in (FORT_CANNON_BIRTH, first + 1, first + 2, first + 3, direction, direction):
            records.append(_frame_ref(frame))
    body = b"".join(records)
    result = bytearray(d[:data]) + b"DATA" + struct.pack("<I", 12 + len(body))
    result += struct.pack("<III", 0x80000000, 0, len(records)) + body
    struct.pack_into("<h", result, frame_count_at, len(records))
    struct.pack_into("<I", result, 4, len(result) - 8)
    open(output, "wb").write(result)
    return True


# The tiles of the state bar's sides, cut from vid\450sbar.vid (1024 x 50): the left one
# from the start of the bar to past its seam (columns 2-107: riveted plate, seam), the right
# one from the right seam to the end (916-1021: seam, riveted plate), so that repeating them
# outward goes plate, seam, plate, ... SIDE_BLEND columns at their joins are cross-faded so
# that the tiles run into each other and into the bar without a visible edge.
SIDE_TILE_WIDTH = 106
SIDE_LEFT, SIDE_RIGHT = 2, 916
SIDE_BLEND = 16


def make_status_bar_sides(bar_vid, output):
    """Writes `output` (vid\\450side.vid): four frames cut from the state bar image `bar_vid`
    in the bar's own sprite format - 0 the left tile, repeated toward the left edge, 1 the
    left tile next to the bar, 2 the right tile next to the bar, 3 the right tile repeated
    toward the right edge (VID 938 has four directions: 0, 64, 128, 192).
    The repeated tiles have their wrap cross-faded (the end of a tile runs into the start of
    the next one, as the bar's own columns would continue), the ones next to the bar fade
    their inner edge into the mirrored first (last) columns of the bar.
    Returns False (and writes nothing) when the bar image is not the 1024 x 50 one."""
    import json
    import shutil
    import tempfile
    from PIL import Image
    import map_tool

    work = tempfile.mkdtemp(prefix="statusbar_")
    try:
        bar = os.path.join(work, "bar")
        map_tool.unpack(bar_vid, bar)
        meta = json.load(open(os.path.join(bar, "vid.json"), encoding="utf-8"))
        if meta.get("kind") != "sprite" or (meta["width"], meta["height"], meta["frameCount"]) != (1024, 50, 1):
            return False
        src = Image.open(os.path.join(bar, "frames.png")).convert("RGBA")
        w, h, f = SIDE_TILE_WIDTH, meta["height"], SIDE_BLEND

        def col(image, x):
            return image.crop((x, 0, x + 1, h))

        def fade(tile, x, other, weight):   # column x of tile toward `other` by weight
            tile.paste(Image.blend(col(tile, x), other, weight), (x, 0))

        # 0: left, repeated - its first columns fade from what follows its end (the plate
        # after the seam) into its own start, so tile after tile runs on
        left = src.crop((SIDE_LEFT, 0, SIDE_LEFT + w, h))
        for x in range(f):
            fade(left, x, col(src, SIDE_LEFT + w + x), 1 - (x + 0.5) / f)
        # 1: left, next to the bar - its last columns fade into the bar's first ones, mirrored
        left_inner = left.copy()
        for k in range(f):
            fade(left_inner, w - 1 - k, col(src, 1 + k), 1 - (k + 0.5) / f)
        # 3: right, repeated - its last columns fade into what comes before its start
        right = src.crop((SIDE_RIGHT, 0, SIDE_RIGHT + w, h))
        for k in range(f):
            fade(right, w - f + k, col(src, SIDE_RIGHT - f + k), (k + 0.5) / f)
        # 2: right, next to the bar - its first columns fade into the bar's last ones, mirrored
        right_inner = right.copy()
        for k in range(f):
            fade(right_inner, k, col(src, 1022 - k), 1 - (k + 0.5) / f)

        frames = [left, left_inner, right_inner, right]
        sheet = Image.new("RGBA", (w * len(frames), h))
        for k, tile in enumerate(frames):
            sheet.paste(tile, (k * w, 0))
        side = os.path.join(work, "side")
        os.makedirs(side)
        sheet.save(os.path.join(side, "frames.png"))
        with open(os.path.join(side, "frames.tsv"), "w", encoding="utf-8", newline="\n") as fo:
            fo.write("frame\treference\tshadow\traw\n")
            for k in range(len(frames)):
                fo.write(f"{k}\t\t\t\n")
        meta.update(frameCount=len(frames), width=w,
                    sheet={"columns": len(frames), "cellWidth": w, "cellHeight": h})
        with open(os.path.join(side, "vid.json"), "w", encoding="utf-8") as f:
            json.dump(meta, f, indent=2)
        map_tool.pack(side, output)
        return True
    finally:
        shutil.rmtree(work, ignore_errors=True)


# The editor's icons (vid\939edit.vid, VID 939): the tools of LocoFix's editor bar that the
# state bar's atlas (vid\455icon.vid) has no icon for, made from the game's own sprites in
# the atlas' style - a 40 x 40 cell with the gold frame and the dark striped ground of the
# atlas' cells and the object in light grey. Frame: (what, the sprites drawn into the cell).
EDITOR_ICON = 40
EDITOR_ICON_ART = 34                    # the picture fits in this square of the cell
EDITOR_ICONS = ["select", "track", "section", "fort", "semaphore_green", "semaphore_red", "decal",
                "push_line", "height", "crate", "palette", "paint", "water", "region"]
# the flat straight rails of vid\610.vid
RAIL_DIAGONAL, RAIL_HORIZONTAL = 2, 1         # the flat frames of shapes 2 and 1
ATLAS_BACKGROUND = 5                    # frame of 455icon.vid whose left columns are plain ground
ATLAS_CROSS, ATLAS_EXIT = 3, 1          # the cross (delete) and the green arrow (exit) of the atlas
ATLAS_NEXT = 6                          # the atlas' ">" (next)


def make_editor_icons(vid_folder, output):
    """Writes `output` (vid\\939edit.vid), a texture image like vid\\455icon.vid with one
    40 x 40 frame per EDITOR_ICONS entry, made from the images in `vid_folder` (the game's
    vid\\ folder): the cursor arrow (001curs), two rails (610),
    the fort (162), the semaphore in its green and red state (152), a bush (135) and the
    cross and the green arrow of the atlas (455icon). Returns False (and writes nothing)
    when one of them is not the image this expects."""
    import csv
    import json
    import shutil
    import tempfile
    from PIL import Image, ImageChops
    import map_tool

    work = tempfile.mkdtemp(prefix="editoricons_")
    try:
        def unpack(name):
            folder = os.path.join(work, name)
            path = os.path.join(vid_folder, name + ".vid")
            if not os.path.isfile(path):
                return None, None
            map_tool.unpack(path, folder)
            return folder, json.load(open(os.path.join(folder, "vid.json"), encoding="utf-8"))

        def sprite(name, frame):   # an RLE sprite frame as RGBA
            folder, meta = unpack(name)
            if not meta or meta["kind"] != "sprite" or frame >= meta["frameCount"]:
                return None
            sheet = Image.open(os.path.join(folder, "frames.png"))
            w, h = meta["width"], meta["height"]
            cols = meta["sheet"]["columns"]
            cell = (frame % cols * w, frame // cols * h, frame % cols * w + w, frame // cols * h + h)
            if sheet.mode == "P":
                with open(os.path.join(folder, "palette.tsv"), encoding="utf-8") as f:
                    palette = {int(r["index"]): (int(r["red"]), int(r["green"]), int(r["blue"]))
                               for r in csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t")}
                index = sheet.crop(cell)
                mask = None
                if os.path.isfile(os.path.join(folder, "mask.png")):
                    mask = Image.open(os.path.join(folder, "mask.png")).convert("L").crop(cell)
                out = Image.new("RGBA", index.size)
                src, dst = index.load(), out.load()
                for y in range(index.size[1]):
                    for x in range(index.size[0]):
                        i = src[x, y]
                        drawn = mask.getpixel((x, y)) if mask else i != meta.get("transparentIndex")
                        if drawn:
                            dst[x, y] = palette.get(i, (0, 0, 0)) + (255,)
                return out
            return sheet.convert("RGBA").crop(cell)

        def texture(name, frame):  # a one-tile texture frame as RGB
            folder, meta = unpack(name)
            if not meta or meta["kind"] != "texture" or frame >= meta["frameCount"]:
                return None
            with open(os.path.join(folder, "tiles.tsv"), encoding="utf-8") as f:
                t = list(csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"))[frame]
            image = Image.open(os.path.join(folder, "texture_%03d.png" % int(t["texture"]))).convert("RGB")
            x, y = int(t["offsetX"]), int(t["offsetY"])
            return image.crop((x, y, x + int(t["width"]), y + int(t["height"])))

        atlas_meta = unpack("455icon")[1]
        if not atlas_meta or (atlas_meta["kind"], atlas_meta["width"], atlas_meta["height"]) != ("texture", 40, 40):
            return False
        cell_frame = texture("455icon", ATLAS_BACKGROUND)
        cross = texture("455icon", ATLAS_CROSS)
        exit_arrow = texture("455icon", ATLAS_EXIT)
        next_arrow = texture("455icon", ATLAS_NEXT)
        arrow = texture("001curs", 0)
        rail_diagonal, rail_horizontal = sprite("610", RAIL_DIAGONAL), sprite("610", RAIL_HORIZONTAL)
        fort = sprite("162", 0)
        green, red = sprite("152", 4), sprite("152", 12)   # STAND and STAND_OPEN, direction 4
        bush = sprite("135", 0)
        crate = sprite("169", 0)
        water = texture("204", 0)   # the light water's tile
        parts = [cell_frame, cross, exit_arrow, next_arrow, arrow, rail_diagonal, rail_horizontal, fort, green, red, bush, crate,
                 water]
        if any(p is None for p in parts):
            return False

        # the cell: the gold frame of an atlas cell, inside it the ground of its left columns
        # (plain, the stripes run across) repeated
        size = EDITOR_ICON
        ground = cell_frame.copy()
        strip = cell_frame.crop((1, 1, 7, size - 1))
        for x in range(1, size - 1, strip.size[0]):
            ground.paste(strip.crop((0, 0, min(strip.size[0], size - 1 - x), strip.size[1])), (x, 1))

        def keyed(image):          # a texture picture on black: black is not drawn
            out = image.convert("RGBA")
            alpha = image.convert("L").point(lambda v: 255 if v > 24 else 0)
            out.putalpha(alpha)
            return out

        def pictogram(image):      # the white sign of an atlas cell, without its ground
            inner = image.crop((1, 1, size - 1, size - 1)).convert("RGBA")
            inner.putalpha(image.crop((1, 1, size - 1, size - 1)).convert("L").point(lambda v: 255 if v > 90 else 0))
            return inner

        def grey(image, keep=lambda r, g, b: False, gain=1.25):
            """The object in the atlas' light grey; pixels for which `keep` is true keep their
            colour (the semaphore's lamp)."""
            out = image.copy()
            px = out.load()
            for y in range(out.size[1]):
                for x in range(out.size[0]):
                    r, g, b, a = px[x, y]
                    if a and not keep(r, g, b):
                        v = min(255, int((0.299 * r + 0.587 * g + 0.114 * b) * gain + 12))
                        px[x, y] = (v, v, v, a)
            return out

        def fit(image, box=EDITOR_ICON_ART, grow=3.0):
            image = image.crop(image.getbbox())
            scale = min(box / image.size[0], box / image.size[1], grow)
            return image.resize((max(1, round(image.size[0] * scale)), max(1, round(image.size[1] * scale))),
                                Image.LANCZOS if scale < 1 else Image.NEAREST)

        def icon(*layers):         # pictures centred on the cell, the first at the bottom
            out = ground.copy().convert("RGBA")
            for picture in layers:
                out.alpha_composite(picture, ((size - picture.size[0]) // 2, (size - picture.size[1]) // 2))
            return out.convert("RGB")

        def middle(rail, span):    # the middle of a rail (the whole one is a thin line in a cell)
            out = rail.copy()
            l, t, r, b = out.getbbox()
            cx, cy = (l + r) // 2, (t + b) // 2
            return out.crop((cx - span // 2, cy - span // 2, cx + span // 2, cy + span // 2))

        lamp = lambda r, g, b: (g > r + 30 and g > b + 30) or (r > 150 and g < 90 and b < 90)
        track = fit(grey(middle(rail_diagonal, 48), gain=1.6), grow=1.0)
        line = fit(grey(middle(rail_horizontal, 48), gain=1.6), grow=1.0)
        # the green arrow of the atlas' exit button, without its gold plate, turned to the right
        exit_sign = exit_arrow.crop((1, 1, size - 1, size - 1)).convert("RGBA")
        px = exit_sign.load()
        for y in range(exit_sign.size[1]):
            for x in range(exit_sign.size[0]):
                r, g, b, _ = px[x, y]
                px[x, y] = (r, g, b, 255 if g > r + 40 and g > b + 40 else 0)
        exit_sign = fit(exit_sign.rotate(-90, expand=True), box=22)
        # the ground's height: the atlas' ">" turned up and down, side by side
        up = fit(pictogram(next_arrow).rotate(90, expand=True), box=16)
        down = fit(pictogram(next_arrow).rotate(-90, expand=True), box=16)
        arrows = Image.new("RGBA", (up.size[0] + down.size[0] + 2, max(up.size[1], down.size[1]) + 6))
        arrows.alpha_composite(up, (0, 0))
        arrows.alpha_composite(down, (up.size[0] + 2, 6))
        # all objects: four of them in the quarters of the cell
        quarters = Image.new("RGBA", (34, 34))
        for k, picture in enumerate([fit(grey(fort), box=16), fit(grey(bush), box=16), fit(grey(green, lamp, gain=0.95), box=16),
                                     fit(grey(crate, gain=1.1), box=16)]):
            quarters.alpha_composite(picture, (k % 2 * 18 + (16 - picture.size[0]) // 2, k // 2 * 18 + (16 - picture.size[1]) // 2))
        from PIL import ImageDraw
        # the ground's paint: a brush stroke in the atlas' light grey, round at its ends
        stroke = Image.new("RGBA", (34, 34))
        ImageDraw.Draw(stroke).line([(6, 27), (14, 14), (22, 18), (28, 7)], fill=(215, 215, 215, 255), width=7, joint="curve")
        for x, y in ((6, 27), (28, 7)):
            ImageDraw.Draw(stroke).ellipse((x - 3, y - 3, x + 3, y + 3), fill=(215, 215, 215, 255))
        # water: a round pool of the light water's own picture
        pool = water.convert("RGBA").resize((30, 22))
        pool_mask = Image.new("L", pool.size, 0)
        ImageDraw.Draw(pool_mask).ellipse((0, 0, pool.size[0] - 1, pool.size[1] - 1), fill=255)
        pool.putalpha(pool_mask)
        # a region: a dashed rectangle
        frame = Image.new("RGBA", (32, 24))
        d = ImageDraw.Draw(frame)
        for x in range(0, 32, 6):
            d.line([(x, 0), (min(31, x + 3), 0)], fill=(215, 215, 215, 255), width=2)
            d.line([(x, 23), (min(31, x + 3), 23)], fill=(215, 215, 215, 255), width=2)
        for y in range(0, 24, 6):
            d.line([(0, y), (0, min(23, y + 3))], fill=(215, 215, 215, 255), width=2)
            d.line([(31, y), (31, min(23, y + 3))], fill=(215, 215, 215, 255), width=2)
        icons = {
            "paint": icon(stroke),
            "water": icon(pool),
            "region": icon(frame),
            "select": icon(fit(keyed(arrow), box=30)),
            "track": icon(track),
            "section": icon(track, fit(pictogram(cross), box=26)),
            "fort": icon(fit(grey(fort))),
            "semaphore_green": icon(fit(grey(green, lamp, gain=0.95))),
            "semaphore_red": icon(fit(grey(red, lamp, gain=0.95))),
            "decal": icon(fit(grey(bush))),
            "push_line": icon(line, exit_sign),
            "height": icon(arrows),
            "crate": icon(fit(grey(crate, gain=1.1))),
            "palette": icon(quarters),
        }

        # the texture: 6 cells a row, as the atlas
        columns = 256 // size
        sheet = Image.new("RGB", (256, 256))
        out = os.path.join(work, "out")
        os.makedirs(out)
        with open(os.path.join(out, "tiles.tsv"), "w", encoding="utf-8", newline="\n") as f:
            f.write("tile\tunknown_00\ttexture\toffsetX\toffsetY\twidth\theight\tx\ty\tnext\n")
            for k, name in enumerate(EDITOR_ICONS):
                x, y = k % columns * size, k // columns * size
                sheet.paste(icons[name], (x, y))
                f.write(f"{k}\t0x00000000\t0\t{x}\t{y}\t{size}\t{size}\t0\t0\t0\n")
        sheet.save(os.path.join(out, "texture_000.png"))
        meta = dict(atlas_meta, frameCount=len(EDITOR_ICONS), textures=1)
        with open(os.path.join(out, "vid.json"), "w", encoding="utf-8") as f:
            json.dump(meta, f, indent=2)
        map_tool.pack(out, output)
        return True
    finally:
        shutil.rmtree(work, ignore_errors=True)


# The editor's two buttons in the round sockets at the left end of the bar, where the game puts
# its pause button (VID 498, 25 px) and its "up to the menu" button (VID 499, 38 px): the same
# discs with another sign - "?" (the editor's help) on the small one and three bars (the
# editor's menu) on the big one. Each image has the two frames of its model, lit and dark.
EDITOR_BUTTONS = [("498", "help", 0.33), ("499", "menu", 0.35)]   # model, name, sign area
BUTTON_SIGNS = {
    "help": [".####.",
             "##..##",
             "....##",
             "...##.",
             "..##..",
             "..##..",
             "......",
             "..##..",
             "..##.."],
    "menu": ["################",
             "################",
             "################",
             "................",
             "................",
             "................",
             "################",
             "################",
             "################",
             "................",
             "................",
             "................",
             "################",
             "################",
             "################"],
}


# The editor's palette (editor_edit.cpp): every object kind the missions use, grouped by theme,
# listed in game/editor/palette.tsv. make_editor_palette writes the game's copy of the list with
# each object's icon frame and, for a unit, the state (behave, build list) of the first one on the
# missions' maps - an object the editor places takes it, as map_tool.py place does - and the icons:
# vid\944pal.vid (VID 944), a texture image of 40 x 40 cells, six a row in 256 x 256 textures, each
# the atlas' dark cell with the object's own picture (frame 0, in colour) fitted into it.
PALETTE_UNIT_CLASSES = {"UNIT", "BUILDING", "AVIA", "CIV_ROBOT", "ENGINE", "DEPO", "CREATURE", "BALLOON"}
VEHICLE_GROUP = "GroupTrain"   # the groups of the vehicles palette (the cars, ENGINE)


def _image_frame(path, frame, work):
    """Frame `frame` of the image file `path` as RGBA - a sprite's (following a frame that reuses
    another) or a one-tile texture's (black not drawn) - or None."""
    import csv
    import json
    from PIL import Image
    import map_tool
    if not os.path.isfile(path):
        return None
    folder = os.path.join(work, "img%d" % len(os.listdir(work)))
    try:
        map_tool.unpack(path, folder)
    except Exception:   # an image the tool cannot read: no icon
        return None
    meta = json.load(open(os.path.join(folder, "vid.json"), encoding="utf-8"))

    def table(name):
        with open(os.path.join(folder, name), encoding="utf-8") as f:
            return list(csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"))
    if meta["kind"] == "sprite":
        rows = table("frames.tsv")
        seen = set()
        while frame < len(rows) and rows[frame]["reference"].strip() and frame not in seen:
            seen.add(frame)
            frame = int(rows[frame]["reference"])
        if frame >= meta["frameCount"]:
            return None
        sheet = Image.open(os.path.join(folder, "frames.png"))
        w, h, cols = meta["width"], meta["height"], meta["sheet"]["columns"]
        cell = (frame % cols * w, frame // cols * h, frame % cols * w + w, frame // cols * h + h)
        if sheet.mode != "P":
            return sheet.convert("RGBA").crop(cell)
        palette = {int(r["index"]): (int(r["red"]), int(r["green"]), int(r["blue"])) for r in table("palette.tsv")}
        index = sheet.crop(cell)
        mask = None
        if os.path.isfile(os.path.join(folder, "mask.png")):
            mask = Image.open(os.path.join(folder, "mask.png")).convert("L").crop(cell)
        out = Image.new("RGBA", index.size)
        src, dst = index.load(), out.load()
        for y in range(index.size[1]):
            for x in range(index.size[0]):
                i = src[x, y]
                if (mask.getpixel((x, y)) if mask else i != meta.get("transparentIndex")):
                    dst[x, y] = palette.get(i, (0, 0, 0)) + (255,)
        return out
    if meta["kind"] == "texture" and not meta.get("ground"):
        rows = table("tiles.tsv")
        if frame >= len(rows):
            return None
        t = rows[frame]
        name = os.path.join(folder, "texture_%03d.png" % int(t["texture"]))
        if not os.path.isfile(name):
            return None
        image = Image.open(name).convert("RGB")
        x, y = int(t["offsetX"]), int(t["offsetY"])
        part = image.crop((x, y, x + int(t["width"]), y + int(t["height"]))).convert("RGBA")
        part.putalpha(part.convert("L").point(lambda v: 255 if v > 24 else 0))
        return part
    return None


def make_editor_palette(game_folder, palette_tsv, output_tsv, output_vid):
    """Writes `output_tsv` (the game's editor\\palette.tsv) and `output_vid` (vid\\944pal.vid) from
    the repository's `palette_tsv`, with the images and maps of `game_folder` (the Steam game).
    Returns the number of objects, 0 when the atlas cannot be made."""
    import csv
    import json
    import shutil
    import tempfile
    from PIL import Image
    import map_tool
    import res_tool

    with open(palette_tsv, encoding="utf-8") as f:
        rows = list(csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"))
    tables = res_tool.read_tables(open(os.path.join(game_folder, "objects.res"), "rb").read())
    objects = {r["nvid"]: r for r in tables["objects"]}
    # the state of a unit: the first one of the missions' maps
    units = {r["nvid"] for r in rows if objects.get(r["nvid"], {}).get("spriteClass") in PALETTE_UNIT_CLASSES}
    state = {}
    info = map_tool.object_info(os.path.join(game_folder, "objects.res"))
    for n in range(1, 21):
        path = os.path.join(game_folder, "maps", "l%02d" % n, "l%02d.map" % n)
        if len(state) == len(units) or not os.path.isfile(path):
            continue
        model = map_tool.read_map(open(path, "rb").read(), info)
        for o in model.get("objects", []):
            if o["nvid"] in units and o["nvid"] not in state:
                state[o["nvid"]] = (o["behave"].strip(), o["items"].strip())
    work = tempfile.mkdtemp(prefix="palette_")
    try:
        atlas = os.path.join(work, "atlas")
        frames_dir = os.path.join(work, "frames")
        os.makedirs(frames_dir)
        # the cell: as the editor's own icons, the gold frame and dark ground of an atlas cell
        icon_vid = os.path.join(game_folder, "vid", "455icon.vid")
        background = _image_frame(icon_vid, 5, frames_dir)
        if background is None:
            return 0
        size, art = EDITOR_ICON, EDITOR_ICON_ART
        ground = background.copy()
        strip = background.crop((1, 1, 7, size - 1))
        for x in range(1, size - 1, strip.size[0]):
            ground.paste(strip.crop((0, 0, min(strip.size[0], size - 1 - x), strip.size[1])), (x, 1))
        weapons = {w["index"]: w for w in tables["weapons"]}

        def state_bar_icon(o):
            """A car's icon of the state bar's atlas: its turret's weapon iconFrame, else its own;
            None when that is 0 or an empty cell."""
            frame = 0
            turret = objects.get(o.get("linkNvid", "0")) if o.get("linkNvid", "0") != "0" else None
            for source in (turret, o):
                if source and frame == 0:
                    frame = int(weapons.get(source["weaponIndex"], {}).get("iconFrame", "0"))
            if frame <= 0:
                return None
            icon = _image_frame(icon_vid, frame, frames_dir)
            if icon is None or icon.size != background.size:
                return None
            from PIL import ImageStat
            inner = icon.convert("L").crop((4, 4, size - 4, size - 4))   # a flat cell (the stone's 29) is no icon
            return icon.convert("RGB") if ImageStat.Stat(inner).stddev[0] > 12 else None

        cells = []
        for r in rows:
            o = objects.get(r["nvid"])
            if o and r["group"].startswith(VEHICLE_GROUP):   # a car: the state bar's own icon
                icon = state_bar_icon(o)
                if icon is not None:
                    cells.append(icon)
                    continue
            picture = _image_frame(os.path.join(game_folder, o["imageFile"]), 0, frames_dir) if o else None
            cell = ground.copy().convert("RGBA")
            box = picture.getbbox() if picture is not None else None
            if box:
                picture = picture.crop(box)
                scale = min(art / picture.size[0], art / picture.size[1], 2.0)
                picture = picture.resize((max(1, round(picture.size[0] * scale)), max(1, round(picture.size[1] * scale))),
                                         Image.LANCZOS if scale < 1 else Image.NEAREST)
                cell.alpha_composite(picture, ((size - picture.size[0]) // 2, (size - picture.size[1]) // 2))
            cells.append(cell.convert("RGB"))
        per_texture = (256 // size) ** 2
        textures = (len(cells) + per_texture - 1) // per_texture
        os.makedirs(atlas)
        with open(os.path.join(atlas, "tiles.tsv"), "w", encoding="utf-8", newline="\n") as f:
            f.write("tile\tunknown_00\ttexture\toffsetX\toffsetY\twidth\theight\tx\ty\tnext\n")
            for k in range(len(cells)):
                i = k % per_texture
                f.write(f"{k}\t0x00000000\t{k // per_texture}\t{i % 6 * size}\t{i // 6 * size}\t{size}\t{size}\t0\t0\t0\n")
        for t in range(textures):
            sheet = Image.new("RGB", (256, 256))
            for i, cell in enumerate(cells[t * per_texture:(t + 1) * per_texture]):
                sheet.paste(cell, (i % 6 * size, i // 6 * size))
            sheet.save(os.path.join(atlas, "texture_%03d.png" % t))
        meta = {"chunks": ["HEAD", "SURF", "DATA"], "imageFlags": "0x21", "frameSpeed": 71,
                "frameCount": len(cells), "width": size, "height": size, "kind": "texture",
                "textures": textures, "ground": False}
        with open(os.path.join(atlas, "vid.json"), "w", encoding="utf-8") as f:
            json.dump(meta, f, indent=2)
        map_tool.pack(atlas, output_vid)
        os.makedirs(os.path.dirname(output_tsv), exist_ok=True)
        with open(output_tsv, "w", encoding="utf-8", newline="\n") as f:
            f.write("group\tnvid\tarmy\tkey\tenglish\tframe\tbehave\titems\n")
            for k, r in enumerate(rows):
                behave, items = state.get(r["nvid"], ("", ""))
                f.write("\t".join([r["group"], r["nvid"], r["army"], r["key"], r["english"], str(k), behave,
                                   items]) + "\n")
        return len(rows)
    finally:
        shutil.rmtree(work, ignore_errors=True)

def make_editor_buttons(vid_folder, help_output, menu_output):
    """Writes `help_output` (vid\\942help.vid) and `menu_output` (vid\\943menu.vid): the discs of
    vid\\498.vid and vid\\499.vid with their sign (pause, the arrow up) taken away and "?" or
    three bars drawn instead, in the colours of the old sign in both frames of the models (0
    dark, 1 lit: the green sign and ring). The place of the old sign is filled from the disc itself, turned by
    a quarter, a half or three quarters where that is not sign too, and the rest from its
    neighbours. Returns False (and writes nothing) when a model is not the image this expects."""
    import csv
    import json
    import shutil
    import tempfile
    from PIL import Image
    import map_tool

    work = tempfile.mkdtemp(prefix="editorbuttons_")
    try:
        made = {}
        for model, name, area in EDITOR_BUTTONS:
            path = os.path.join(vid_folder, model + ".vid")
            if not os.path.isfile(path):
                return False
            folder = os.path.join(work, name)
            map_tool.unpack(path, folder)
            meta = json.load(open(os.path.join(folder, "vid.json"), encoding="utf-8"))
            if meta["kind"] != "texture" or meta["frameCount"] != 2 or meta.get("textures") != 1:
                return False
            with open(os.path.join(folder, "tiles.tsv"), encoding="utf-8") as f:
                tiles = list(csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"))
            sheet = Image.open(os.path.join(folder, "texture_000.png")).convert("RGBA")
            for frame, t in enumerate(tiles):
                x0, y0, w, h = (int(t[k]) for k in ("offsetX", "offsetY", "width", "height"))
                disc = sheet.crop((x0, y0, x0 + w, y0 + h))
                sheet.paste(_resign(disc, BUTTON_SIGNS[name], area), (x0, y0))
            sheet.save(os.path.join(folder, "texture_000.png"))
            made[name] = folder
        for name, output in (("help", help_output), ("menu", menu_output)):
            map_tool.pack(made[name], output)
        return True
    finally:
        shutil.rmtree(work, ignore_errors=True)


# The "edit the map" icon of the custom maps page (vid\945edit.vid, VID 945): the hammer and the
# wrench of the state bar's atlas (frame ATLAS_TOOLS of vid\455icon.vid) drawn as the page's green
# glyphs, CUSTOM_EDIT_ICON px square, in the kind of image of the page's lamp (vid\472.vid): green
# with an alpha. Four frames, as the animations of VID 945: STAND and STOP_MOVE dim, GO and
# START_MOVE lit (the menu shows STOP_MOVE, act_menustandup, and START_MOVE under the mouse).
ATLAS_TOOLS = 2
CUSTOM_EDIT_ICON = 22


def make_custom_edit_icon(vid_folder, output):
    """Writes `output` (vid\\945edit.vid) from vid\\455icon.vid and vid\\472.vid in `vid_folder`.
    Returns False (and writes nothing) when they are not the images this expects."""
    import csv
    import json
    import shutil
    import tempfile
    from PIL import Image
    import map_tool

    work = tempfile.mkdtemp(prefix="customedit_")
    try:
        folders = {}
        for name in ("455icon", "472"):
            path = os.path.join(vid_folder, name + ".vid")
            if not os.path.isfile(path):
                return False
            folders[name] = os.path.join(work, name)
            map_tool.unpack(path, folders[name])
        atlas = json.load(open(os.path.join(folders["455icon"], "vid.json"), encoding="utf-8"))
        folder = folders["472"]
        lamp = json.load(open(os.path.join(folder, "vid.json"), encoding="utf-8"))
        if atlas["kind"] != "texture" or (atlas["width"], atlas["height"]) != (40, 40) or lamp["kind"] != "texture":
            return False
        with open(os.path.join(folders["455icon"], "tiles.tsv"), encoding="utf-8") as f:
            tiles = list(csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"))
        t = tiles[ATLAS_TOOLS]
        x, y = int(t["offsetX"]), int(t["offsetY"])
        cell = Image.open(os.path.join(folders["455icon"], "texture_%03d.png" % int(t["texture"]))).convert("L")
        # the light grey tools inside the gold frame, as the glyph's coverage
        size = CUSTOM_EDIT_ICON
        glyph = cell.crop((x + 3, y + 3, x + 37, y + 37)).point(lambda v: max(0, min(255, (v - 70) * 2)))
        glyph = glyph.resize((size, size), Image.LANCZOS)
        sheet = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
        frames = []
        for frame, lit in enumerate((False, False, True, True)):
            image = Image.new("RGBA", (size, size), (40, 255, 40, 0) if lit else (20, 200, 20, 0))
            image.putalpha(glyph if lit else glyph.point(lambda v: v * 9 // 20))
            frames.append((frame % 2 * 32, frame // 2 * 32))
            sheet.paste(image, frames[-1])
        sheet.save(os.path.join(folder, "texture_000.png"))
        with open(os.path.join(folder, "tiles.tsv"), encoding="utf-8") as f:
            rows = list(csv.DictReader((l for l in f if not l.startswith("#")), delimiter="\t"))
        header = list(rows[0].keys())
        with open(os.path.join(folder, "tiles.tsv"), "w", encoding="utf-8", newline="\n") as f:
            f.write("\t".join(header) + "\n")
            for k, (ox, oy) in enumerate(frames):
                row = {"tile": k, "unknown_00": rows[0]["unknown_00"] if k == 0 else "0x00000000", "texture": 0,
                       "offsetX": ox, "offsetY": oy, "width": size, "height": size, "x": 0, "y": 0, "next": 0}
                f.write("\t".join(str(row[c]) for c in header) + "\n")
        lamp.update(frameCount=len(frames), width=size, height=size, textures=1)
        json.dump(lamp, open(os.path.join(folder, "vid.json"), "w", encoding="utf-8"), indent=2)
        map_tool.pack(folder, output)
        return True
    finally:
        shutil.rmtree(work, ignore_errors=True)


def _resign(disc, sign, area):
    """The disc with its sign replaced by `sign` (rows of '#' and '.'); the old sign lies within
    `area` x the width from the middle, inside the ring. A lit disc has a bright green sign, a
    dark one a nearly black sign."""
    w, h = disc.size
    cx, cy, reach = (w - 1) / 2, (h - 1) / 2, w * area
    px = disc.load()

    def green(r, g, b):
        return g > 150 and g > r + 40 and g > b + 60

    lit = any(green(*px[x, y][:3]) for y in range(h) for x in range(w)
              if (x - cx) ** 2 + (y - cy) ** 2 <= reach ** 2)

    def is_sign(x, y):
        r, g, b, a = px[x, y]
        if not a or (x - cx) ** 2 + (y - cy) ** 2 > reach ** 2:
            return False
        if lit:
            return green(r, g, b)
        return 0.299 * r + 0.587 * g + 0.114 * b < 45

    old = {(x, y) for y in range(h) for x in range(w) if is_sign(x, y)}
    if not old:
        return disc
    colours = sorted(px[x, y] for x, y in old)
    colour = colours[len(colours) // 2]
    out = disc.copy()
    o = out.load()
    left = set()
    for x, y in old:   # the disc turned about its middle, where that is not the sign
        for turn in (1, 2, 3):
            dx, dy = x - cx, y - cy
            for _ in range(turn):
                dx, dy = -dy, dx
            sx, sy = round(cx + dx), round(cy + dy)
            if 0 <= sx < w and 0 <= sy < h and (sx, sy) not in old and px[sx, sy][3]:
                o[x, y] = px[sx, sy]
                break
        else:
            left.add((x, y))
    while left:        # the rest from the neighbours, from the outside in
        done = set()
        for x, y in left:
            near = [o[x + dx, y + dy] for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
                    if 0 <= x + dx < w and 0 <= y + dy < h and (x + dx, y + dy) not in left]
            if near:
                o[x, y] = tuple(sum(c[i] for c in near) // len(near) for i in range(4))
                done.add((x, y))
        if not done:
            break
        left -= done
    sw, sh = len(sign[0]), len(sign)
    ox, oy = round(cx - (sw - 1) / 2), round(cy - (sh - 1) / 2)
    for row, line in enumerate(sign):
        for col, c in enumerate(line):
            if c == "#":
                o[ox + col, oy + row] = colour
    return out


def _frame_ref(frame):
    """A DATA record that reuses an earlier frame: size 2, then the frame index."""
    return struct.pack("<IIh", 6, 2, frame)


if __name__ == "__main__":
    import sys
    if len(sys.argv) == 3:
        print("patched" if fix_fort_cannon(sys.argv[1], sys.argv[2]) else "unknown layout, copied")
    elif len(sys.argv) == 4:
        for n in fix_objects_res(sys.argv[1], sys.argv[2], sys.argv[3]):
            print("->", n)
    else:
        sys.exit("usage: patch_data.py <objects.res> <game wav folder> <output objects.res>\n"
                 "       patch_data.py <vid\\110.vid> <output vid>")
