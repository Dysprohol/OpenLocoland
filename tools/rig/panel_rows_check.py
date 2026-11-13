r"""Checks the factory panel's pages as a test run drew them (the H steps of tools\test_game.ps1).

    python tools\rig\panel_rows_check.py <run folder> [<run folder> ...] [--width 1024 --height 768]

A run folder is a rig job's results (game-logs\LocoFix.log, test_game.txt). Every "panel rows: n"
block of LocoFix.log is one page as the panel's last frame drew it: each text and choice with its
box. For each page this prints what is wrong (counted):

- OVERLAP     two texts (or a text and a choice that is not the row it stands in) cover each other;
- RIGHT       a text runs past the row's right end (the panel's 46 characters from its left);
- BOTTOM/TOP  a text is below the last row (into the button bar) or above the first (the strip above
              the title, an entry's problem, is a row of its own);

and, with --notes, what to look at (not counted):

- EDGE        a text ends exactly at the right end: an exact fit, or (the old panel) a cut without a mark;
- CUT         a text cut to its row with the mark (".").

The panel's place is read from each page's first text, its head row (editor_factory.cpp: LEFT -182,
TOP -70, BOTTOM 98, 46 characters of 8 px, the strip above the title at -128).
"""
import argparse
import pathlib
import re

LEFT, TOP, BOTTOM, WIDTH, CW, CH, ROW, STRIP = -182, -70, 98, 46, 8, 13, 14, -128

ROW_LINE = re.compile(r"panel row (\d+) (choice|text) (-?\d+),(-?\d+)-(-?\d+),(-?\d+) ?(.*)$")


def pages(log_text):
    out, cur, left = [], None, 0
    for line in log_text.splitlines():
        m = re.search(r"panel rows: (\d+)", line)
        if m:
            cur, left = [], int(m.group(1))
            out.append(cur)
            continue
        m = ROW_LINE.search(line)
        if m and cur is not None and left > 0 and "clicked" not in line:
            i, kind, l, t, r, b, text = m.groups()
            cur.append({"i": int(i), "choice": kind == "choice", "l": int(l), "t": int(t), "r": int(r), "b": int(b), "text": text})
            left -= 1
    return out


def inside(a, b):   # a lies inside b
    return a["l"] >= b["l"] and a["r"] <= b["r"] and a["t"] >= b["t"] and a["b"] <= b["b"]


def check(page, px, py, notes=False):
    right = px + LEFT + WIDTH * CW
    top, bottom = py + TOP - CH / 2 - 1, py + BOTTOM + CH / 2 + 1
    faults = []
    page = [a for a in page if a["r"] > a["l"]]   # an empty text (a note's unused row) takes no room
    for k, a in enumerate(page):
        is_area = a["choice"] and (a["text"] == "" or a["text"].startswith(("unit ", "car ")))
        if not is_area:
            if a["r"] > right:
                faults.append(f"RIGHT  {a['r'] - right:+d} px: {a['text']!r}")
            elif a["r"] == right and notes and a["text"] not in (">",):
                kind = "CUT  " if a["text"].endswith(".") and not a["text"].endswith("...") else "EDGE "
                faults.append(f"{kind}  ends at the row's end: {a['text']!r}")
            if a["b"] > bottom:
                faults.append(f"BOTTOM {a['b'] - bottom:+.0f} px: {a['text']!r}")
            on_strip = abs((a["t"] + a["b"]) / 2 - (py + STRIP)) <= ROW / 2
            if a["t"] < top and not on_strip:
                faults.append(f"TOP    {top - a['t']:+.0f} px: {a['text']!r}")
        for b in page[k + 1:]:
            if a["l"] >= b["r"] or b["l"] >= a["r"] or a["t"] >= b["b"] or b["t"] >= a["b"]:
                continue
            a_area = a["choice"] and (a["text"] == "" or a["text"].startswith(("unit ", "car ")))
            b_area = b["choice"] and (b["text"] == "" or b["text"].startswith(("unit ", "car ")))
            if (a_area and inside(b, a)) or (b_area and inside(a, b)):
                continue   # a row's own texts in its click area, an icon's key on the icon
            if a_area and b_area:
                # icons side by side share their edge pixel only
                if min(a["r"], b["r"]) - max(a["l"], b["l"]) <= 1 or min(a["b"], b["b"]) - max(a["t"], b["t"]) <= 1:
                    continue
            faults.append(f"OVERLAP {a['text'] or '(row)'!r} [{a['l']},{a['t']}-{a['r']},{a['b']}] and "
                          f"{b['text'] or '(row)'!r} [{b['l']},{b['t']}-{b['r']},{b['b']}]")
    return faults


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("runs", nargs="+")
    ap.add_argument("--width", type=int, default=0)
    ap.add_argument("--height", type=int, default=0)
    ap.add_argument("--notes", action="store_true", help="also EDGE and CUT (not counted)")
    a = ap.parse_args()
    total = 0
    for run in a.runs:
        run = pathlib.Path(run)
        w, h = a.width, a.height
        if not w:   # the job's registry
            job = (run / "job.json").read_text(encoding="utf-8-sig")
            w = int(re.search(r'"ScreenX":\s*(\d+)', job).group(1))
            h = int(re.search(r'"ScreenY":\s*(\d+)', job).group(1))
        px, py = w / 2 + 22, h / 2 - 18
        log = (run / "game-logs" / "LocoFix.log").read_text(encoding="cp1250", errors="replace")
        ps = pages(log)
        print(f"== {run.name}: {len(ps)} pages at {w}x{h}")
        for n, page in enumerate(ps, 1):
            if page:   # the page's first text is its head row at LEFT, TOP: the panel's place on this screen
                px, py = page[0]["l"] - LEFT, page[0]["t"] + CH // 2 + 1 - TOP
            faults = check(page, px, py, a.notes)
            total += sum(1 for f in faults if not f.startswith(("EDGE", "CUT")))
            head = page[0]["text"] if page else ""
            for f in faults:
                print(f"  page {n} ({head[:30]}): {f}")
    print(f"{total} faults")
    return 1 if total else 0


if __name__ == "__main__":
    raise SystemExit(main())
