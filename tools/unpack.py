r"""Extracts game data in a readable form.

    python tools\unpack.py [--steam <game folder in Steam>]

re/exe/          imports, strings and dialogs from Steam.exe, the launcher bitmap
re/data/         objects from objects.res (number, name, weapon, icon), the WEAP table
build/graphics/  fonts and icon frames as PNG (for viewing, outside the repo)

The C code of the game comes from Ghidra - see tools/decompile.py.
"""
import argparse, os, re, struct, collections
import pefile
from PIL import Image, ImageDraw

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ap = argparse.ArgumentParser()
ap.add_argument("--steam", default=r"C:\Program Files (x86)\Steam\steamapps\common\Locoland")
STEAM = ap.parse_args().steam
BASE = 0x400000


def out(*parts):
    p = os.path.join(REPO, *parts)
    os.makedirs(os.path.dirname(p), exist_ok=True)
    return p


# ------------------------------------------------------------------ exe -----
EXE = os.path.join(STEAM, "Steam.exe")
raw = open(EXE, "rb").read()
pe = pefile.PE(EXE)

imports = {}
for imp in pe.DIRECTORY_ENTRY_IMPORT:
    for f in imp.imports:
        if f.name:
            imports[f.address] = f"{imp.dll.decode()}!{f.name.decode()}"


def cstring(va):
    o = va - BASE
    if not (0x75000 <= o < len(raw)):
        return None
    e = raw.find(b"\0", o, o + 200)
    if e - o < 3:
        return None
    s = raw[o:e]
    if all(32 <= b < 127 or b in (9, 10, 13) or b >= 0xC0 for b in s):
        return s.decode("cp1251").replace("\n", "\\n").replace("\r", "\\r")
    return None


with open(out("re", "exe", "imports.txt"), "w", encoding="utf-8") as f:
    for a, n in sorted(imports.items()):
        f.write(f"{a:#x}  {n}\n")

with open(out("re", "exe", "strings.txt"), "w", encoding="utf-8") as f:
    for m in re.finditer(rb"[\x20-\x7e\xc0-\xff]{4,}\x00", raw[0x75000:0x8a000]):
        s = m.group()[:-1].decode("cp1251")
        f.write(f"{BASE + 0x75000 + m.start():#x}  {s}\n")


# dialogs and bitmaps from the resources
def sz_or_ord(b, o):
    w = struct.unpack_from("<H", b, o)[0]
    if w == 0:
        return "", o + 2
    if w == 0xFFFF:
        return f"#{struct.unpack_from('<H', b, o + 2)[0]}", o + 4
    e = o
    while struct.unpack_from("<H", b, e)[0]:
        e += 2
    return b[o:e].decode("utf-16-le"), e + 2


CLS = {0x80: "Button", 0x81: "Edit", 0x82: "Static", 0x83: "ListBox", 0x84: "ScrollBar", 0x85: "ComboBox"}
with open(out("re", "exe", "dialogs.txt"), "w", encoding="utf-8") as f:
    for t in pe.DIRECTORY_ENTRY_RESOURCE.entries:
        for e in t.directory.entries:
            name = str(e.name) if e.name else str(e.id)
            l = e.directory.entries[0]
            rva, size = l.data.struct.OffsetToData, l.data.struct.Size
            b = pe.get_data(rva, size)
            if t.id == 2:   # RT_BITMAP -> .bmp
                hdr = struct.unpack_from("<I", b, 0)[0]
                bits = 14 + hdr + (4 * (1 << struct.unpack_from('<H', b, 14)[0]) if struct.unpack_from('<H', b, 14)[0] <= 8 else 0)
                open(out("re", "exe", f"{name}.bmp"), "wb").write(b"BM" + struct.pack("<IHHI", 14 + len(b), 0, 0, bits) + b)
            if t.id != 5:
                continue
            style, exst, n, x, y, cx, cy = struct.unpack_from("<IIHhhhh", b, 0)
            o = 18
            _, o = sz_or_ord(b, o)
            _, o = sz_or_ord(b, o)
            title, o = sz_or_ord(b, o)
            if style & 0x40:
                o += 2
                _, o = sz_or_ord(b, o)
            f.write(f"== {name}  file offset {pe.get_offset_from_rva(rva):#x}  '{title}' ({x},{y},{cx},{cy})\n")
            for i in range(n):
                o = (o + 3) & ~3
                start = o
                st, ex, x, y, cx, cy, cid = struct.unpack_from("<IIhhhhH", b, o)
                o += 18
                c, o = sz_or_ord(b, o)
                txt, o = sz_or_ord(b, o)
                o += 2 + struct.unpack_from("<H", b, o)[0]
                c = CLS.get(int(c[1:]), c) if c.startswith("#") else c
                f.write(f"  +{start:#05x} {c:9} id={cid:<6} style={st:#010x} ({x},{y},{cx},{cy}) '{txt}'\n")
            f.write("\n")

# -------------------------------------------------------------- objects.res --
res = open(os.path.join(STEAM, "objects.res"), "rb").read()
recs = {}
for m in re.finditer(rb"[\x20-\x7e\xa8\xb8\xc0-\xff]{3,}\x00", res):
    pre = res[max(0, m.start() - 24):m.start()]
    if len(pre) < 24:
        continue
    ints = struct.unpack("<6I", pre)
    if 600 < ints[4] < 800 and 0 < ints[5] < 1100 and ints[5] not in recs:
        recs[ints[5]] = (m.group()[:-1].decode("cp1251"), m.end())
wb = res.find(b"WEAP") + 8 + 16
weap = [struct.unpack_from("<17i", res, wb + 72 * i + 4) for i in range(55)]
units = collections.OrderedDict()
with open(out("re", "data", "objects.tsv"), "w", encoding="utf-8") as f:
    f.write("vid\tname_ru\tweap\ticon_455\n")
    for v in sorted(recs):
        name, end = recs[v]
        w = struct.unpack_from("<i", res, end + 52)[0]
        icon = weap[w][13] if 0 < w < 55 else ""
        f.write(f"{v}\t{name}\t{w}\t{icon}\n")
with open(out("re", "data", "weap.tsv"), "w", encoding="utf-8") as f:
    f.write("# WEAP section of objects.res: 55 entries of 68 bytes; field 13 = icon frame in vid\\455icon.vid\n")
    for i, w in enumerate(weap):
        f.write(f"{i}\t" + "\t".join(str(x) for x in w) + "\n")

# --------------------------------------------------------------- graphics ---
for fn in os.listdir(os.path.join(STEAM, "fonts")):
    if fn.lower().endswith(".bmp"):
        im = Image.open(os.path.join(STEAM, "fonts", fn))
        im.resize((im.width * 3, im.height * 3), Image.NEAREST).save(out("build", "graphics", "fonts", fn[:-4] + ".png"))

d = open(os.path.join(STEAM, "vid", "455icon.vid"), "rb").read()
px = struct.unpack_from("<%dH" % (256 * 256), d, 0x52)
atlas = Image.new("RGB", (256, 256))
atlas.putdata([(((p >> 11) & 31) << 3, ((p >> 5) & 63) << 2, (p & 31) << 3) for p in px])
t = d[0x52 + 131072:]
sheet = Image.new("RGB", (9 * 64, 6 * 64), (40, 40, 40))
dr = ImageDraw.Draw(sheet)
for i in range(54):
    x, y, w, h = struct.unpack_from("<4h", t, 28 + 20 * i)
    sheet.paste(atlas.crop((x, y, x + w, y + h)), ((i % 9) * 64 + 12, (i // 9) * 64 + 2))
    dr.text(((i % 9) * 64 + 2, (i // 9) * 64 + 44), str(i), fill=(255, 255, 0))
sheet.resize((sheet.width * 2, sheet.height * 2), Image.NEAREST).save(out("build", "graphics", "icons_455_frames.png"))

print("Done: re/ and build/graphics/")
