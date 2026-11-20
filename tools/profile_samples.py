"""Where the game's main thread spent its time: reads a run's logs\\sampler.bin and sampler.txt.

    python tools\\profile_samples.py <game logs folder> [--from S] [--to S] [--view-over W] [--map LocoFix.map]

The sampler (src/LocoFix/sampler.cpp, on with the registry DWORD EditorProfile = 2 or LOCOFIX_PROFILE=2)
notes about every millisecond where the main thread is: its instruction pointer, the first return
address into the exe on its stack and the view's width. This prints, for the samples between --from and
--to seconds of the run (and with a view wider than --view-over pixels: a zoom out), the share of:
- each place's own time: an exe function (by re/symbols.csv), a LocoFix function (by a link map, when
  one is given: a build with /MAP), or a DLL (dgVoodoo's DDraw.dll, the driver, ntdll's waits);
- the time outside the exe by the exe's call that led there (the engine's call into DirectDraw or
  Direct3D: a lock, a draw, the present);
- each exe function with what it called outside the exe.
"""
import argparse
import bisect
import collections
import os
import re
import struct

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE_TEXT = (0x401000, 0x479000)


def exe_functions():
    funcs = []
    with open(os.path.join(REPO, "re", "symbols.csv"), encoding="utf-8", errors="replace") as f:
        for line in f:
            p = line.split(";")
            if len(p) >= 3 and p[1] == "f":
                try:
                    funcs.append((int(p[0], 16), p[2]))
                except ValueError:
                    pass
    return sorted(funcs)


def map_functions(path):
    funcs = []
    with open(path, errors="replace") as f:
        for line in f:
            m = re.match(r"\s*0001:[0-9a-f]+\s+(\S+)\s+([0-9a-f]{8})\s", line)
            if m:
                name = m.group(1)
                d = re.match(r"\?([^@]+)@([^@]*)@", name)
                if d:
                    name = d.group(2) + "::" + d.group(1) if d.group(2) and not d.group(2).startswith("?") else d.group(1)
                funcs.append((int(m.group(2), 16), name))
    return sorted(funcs)


def name_of(funcs, address):
    i = bisect.bisect_right(funcs, (address, "￿")) - 1
    return funcs[i][1] if i >= 0 else "?"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--from", dest="t0", type=float, default=0)
    ap.add_argument("--to", dest="t1", type=float, default=1e9)
    ap.add_argument("--view-over", type=int, default=0)
    ap.add_argument("--map", default="")
    ap.add_argument("--top", type=int, default=30)
    a = ap.parse_args()
    data = open(os.path.join(a.logs, "sampler.bin"), "rb").read()
    modules, locofix = [], 0
    with open(os.path.join(a.logs, "sampler.txt")) as f:
        for line in f:
            if line.startswith("locofix"):
                locofix = int(line.split()[1], 16)
            elif line.startswith("module"):
                _, base, size, path = line.rstrip("\n").split(" ", 3)
                modules.append((int(base, 16), int(size, 16), os.path.basename(path)))
    exe = exe_functions()
    lf = map_functions(a.map) if a.map else []

    def where(ip):
        if EXE_TEXT[0] <= ip < EXE_TEXT[1]:
            return "exe:" + name_of(exe, ip)
        for base, size, name in modules:
            if base <= ip < base + size:
                if base == locofix:
                    return "LocoFix:" + (name_of(lf, ip - base + 0x10000000) if lf else "+%x" % (ip - base))
                return name
        return "?%08x" % ip

    samples = []
    for i in range(0, len(data) - 11, 12):
        ip, caller, view, t = struct.unpack_from("<IIHH", data, i)
        seconds = t * 16 / 1000
        if a.t0 <= seconds < a.t1 and view > a.view_over:
            samples.append((ip, caller))
    n = len(samples)
    if not n:
        print("no samples")
        return
    print(f"{n} samples ({n / 1000:.1f} s)")
    own = collections.Counter(where(ip) for ip, _ in samples)
    below = collections.Counter()
    inclusive = collections.Counter()
    for ip, caller in samples:
        w = where(ip)
        if w.startswith("exe:"):
            inclusive[w] += 1
        else:
            c = name_of(exe, caller) if caller else "-"
            below[f"{c} @{caller:06x}"] += 1
            inclusive["exe:" + c] += 1
    for title, counter in (("own time", own), ("outside the exe, by the exe's call", below),
                           ("exe functions with what they called outside it", inclusive)):
        print(f"-- {title}:")
        for k, c in counter.most_common(a.top):
            print(f"  {100 * c / n:5.1f}%  {k}")


if __name__ == "__main__":
    main()
