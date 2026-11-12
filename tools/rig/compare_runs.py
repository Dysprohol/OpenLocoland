"""Compares two test_missions.ps1 result folders - e.g. a run on the host and the same build's run in
the test rig: the report's rows (verdict, the script compile line) and the screenshots pixel by
pixel, also after the best shift of the view (the test cursor starts where the real cursor was, and the
engine's edge scrolling moves a big map's view toward it before the screenshot - a few pixels of camera,
not of drawing). Writes <out>/diff_<nn>.png (pixels that differ in red over a faded copy of the first picture)
and prints a table.

    python tools/rig/compare_runs.py <folder A> <folder B> [--out <folder>]
"""
import argparse
import os
import re

from PIL import Image, ImageChops, ImageStat


def report_rows(folder):
    rows = {}
    path = os.path.join(folder, "report.txt")
    with open(path, encoding="utf-8-sig") as f:
        for line in f:
            m = re.match(r"\s*(\d+)\s+(True|False)\s+(True|False)\s+(True|False)\s+(\d+)\s+(\d+)\s+(True|False)\s+(.*)$", line)
            if m:
                rows[int(m.group(1))] = {"passed": m.group(2), "loaded": m.group(3), "confirmed": m.group(4),
                                         "script_errors": m.group(5), "exceptions": m.group(6),
                                         "closed": m.group(7), "script": m.group(8).strip()}
    return rows


def changed_share(ia, ib):
    """share of pixels that differ at all, and the mask"""
    mask = ImageChops.difference(ia, ib).convert("L").point(lambda v: 255 if v > 0 else 0)
    return ImageStat.Stat(mask).mean[0] / 255, mask


def crop_pair(ia, ib, dx, dy):
    """the overlap of A and B with B's view moved by (dx, dy)"""
    w, h = ia.size
    return (ia.crop((max(0, dx), max(0, dy), min(w, w + dx), min(h, h + dy))),
            ib.crop((max(0, -dx), max(0, -dy), min(w, w - dx), min(h, h - dy))))


def best_shift(ia, ib, reach=96):
    """the shift of B's view that matches A best: coarse on quarter-size grey copies, then exact"""
    qa, qb = (x.convert("L").resize((x.size[0] // 4, x.size[1] // 4)) for x in (ia, ib))
    best = (1e9, 0, 0)
    for dy in range(-reach // 4, reach // 4 + 1):
        for dx in range(-reach // 4, reach // 4 + 1):
            ca, cb = crop_pair(qa, qb, dx, dy)
            best = min(best, (ImageStat.Stat(ImageChops.difference(ca, cb)).mean[0], dx, dy))
    _, cx, cy = best
    best = (2.0, 0, 0)
    for dy in range(cy * 4 - 4, cy * 4 + 5):
        for dx in range(cx * 4 - 4, cx * 4 + 5):
            ca, cb = crop_pair(ia, ib, dx, dy)
            best = min(best, (changed_share(ca, cb)[0], dx, dy))
    return best


def compare_pictures(a, b, diff_path):
    ia, ib = Image.open(a).convert("RGB"), Image.open(b).convert("RGB")
    if ia.size != ib.size:
        return f"sizes differ: {ia.size} vs {ib.size}", None
    share, mask = changed_share(ia, ib)
    changed = int(share * ia.size[0] * ia.size[1])
    total = ia.size[0] * ia.size[1]
    shifted = ""
    if share > 0.06:
        s2, dx, dy = best_shift(ia, ib)
        if (dx, dy) != (0, 0):
            shifted = f"; view shifted by ({dx},{dy}) px: then {s2 * 100:.2f} % differ"
    bbox = mask.getbbox()
    faded = Image.blend(ia, Image.new("RGB", ia.size, (255, 255, 255)), 0.6)
    red = Image.new("RGB", ia.size, (255, 0, 0))
    Image.composite(red, faded, mask).save(diff_path)
    return f"{changed / total * 100:6.2f} % of pixels differ" + shifted, changed


def main():
    p = argparse.ArgumentParser()
    p.add_argument("a")
    p.add_argument("b")
    p.add_argument("--out", default=None)
    args = p.parse_args()
    out = args.out or os.path.join(args.b, "compare")
    os.makedirs(out, exist_ok=True)
    ra, rb = report_rows(args.a), report_rows(args.b)
    same_verdicts = 0
    for n in sorted(set(ra) | set(rb)):
        x, y = ra.get(n), rb.get(n)
        verdict = "only in one" if not (x and y) else (
            "same report row" if x == y else
            "same verdict, script line differs" if {k: v for k, v in x.items() if k != "script"} == {k: v for k, v in y.items() if k != "script"} else
            "DIFFERENT VERDICT")
        if x and y and x["passed"] == y["passed"]:
            same_verdicts += 1
        pa, pb = (os.path.join(f, f"mission_{n:02d}.png") for f in (args.a, args.b))
        pic = compare_pictures(pa, pb, os.path.join(out, f"diff_{n:02d}.png"))[0] if os.path.exists(pa) and os.path.exists(pb) else "no picture"
        print(f"mission {n:2d}: {verdict:34s} | {pic}")
    print(f"verdicts equal: {same_verdicts} of {len(set(ra) | set(rb))}; diff pictures in {out}")


if __name__ == "__main__":
    main()
