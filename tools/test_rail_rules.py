r"""Differential test of the rail rules: the in-game editor's C++ (src/LocoFix/rail_rules.h)
against tools/map_tool.py, so the two do not drift apart.

    python tools\test_rail_rules.py [cases]

Builds tools\test_rail_rules.cpp (which includes rail_rules.h) with MSVC into build\tests,
generates cases - pairs of points for rail_candidates, tracks next to existing rails for
plan_track (lattice steps with some invalid ones, branches from the rails' ends, hand-picked
shapes, rails and ramps at other heights, the track's height given or not), segments for
track_cells, rail objects for rail_of (every direction, ramps), the rail points of crowded
joints for rail_point_links - and compares the answers. Exits with 1 on a difference.
"""
import os
import random
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
OUT_DIR = os.path.join(REPO, "build", "tests")
TEST_EXE = os.path.join(OUT_DIR, "test_rail_rules.exe")
sys.path.insert(0, HERE)
from build_locofix import vcvars32  # noqa: E402
import map_tool  # noqa: E402


def build_test_host():
    os.makedirs(OUT_DIR, exist_ok=True)
    source = os.path.join(HERE, "test_rail_rules.cpp")
    header = os.path.join(REPO, "src", "LocoFix", "rail_rules.h")
    if os.path.isfile(TEST_EXE) and os.path.getmtime(TEST_EXE) >= max(os.path.getmtime(source),
                                                                        os.path.getmtime(header)):
        return
    bat = vcvars32()
    if not bat:
        sys.exit("No MSVC compiler with the C++ tools.")
    script = os.path.join(OUT_DIR, "test_rail_rules.bat")
    with open(script, "w", encoding="ascii") as f:
        f.write("@echo off\r\n")
        f.write(f'call "{bat}" >nul || exit /b 1\r\n')
        f.write(f'cd /d "{OUT_DIR}"\r\n')
        f.write(f'cl /nologo /O2 /MT /EHsc /utf-8 /W4 /std:c++17 "{source}" /Fe:test_rail_rules.exe\r\n')
    result = subprocess.run(["cmd", "/c", script], capture_output=True, text=True, errors="replace")
    if result.returncode != 0:
        sys.exit("test host build failed:\n" + result.stdout + result.stderr)


def num(v):
    return f"{v:.3f}"


def model_candidates(a, b):
    return ";".join(f"{num(x)} {num(y)} {shape} {f[0]} {f[1]} {s[0]} {s[1]}"
                    for x, y, shape, f, s in map_tool.rail_candidates(a, b))


def model_plan(rails, points, shapes, z):
    try:
        segments = map_tool.plan_track(rails, points, shapes or None, z)
    except map_tool.TrackError as e:
        return f"{e.code} {e.step}"
    return "ok -1" + "".join(f" {num(x)} {num(y)} {shape} {num(e0[0])} {num(e0[1])} {num(e1[0])} {num(e1[1])};"
                             for _a, _b, x, y, shape, e0, e1 in segments)


def model_rail(x, y, z, direction):
    x, y, shape, z0, z1 = map_tool.rail_of(x, y, z, direction)
    return f"{num(x)} {num(y)} {shape} {z0} {z1}"


def model_points(rails, point):
    return "".join(sorted(f"{e[0]} {e[1]} {z} {links};"
                          for (e, z), links in map_tool.rail_point_links(rails, point).items()))


def model_cells(width, height, segments):
    track, buffer = map_tool.track_cells([(a, b) for a, b in segments], width, height)
    return ("".join(f"{x},{y} " for x, y in sorted(track)) + "|" +
            "".join(f"{x},{y} " for x, y in sorted(buffer)))


# The steps of the lattice: from one end of a rail to the other, for every shape.
STEPS = sorted({(round((s[0] - f[0]) * map_tool.RAIL_HALF[0], 3), round((s[1] - f[1]) * map_tool.RAIL_HALF[1], 3))
                for f, s in map_tool.RAIL_ENDS} |
               {(round((f[0] - s[0]) * map_tool.RAIL_HALF[0], 3), round((f[1] - s[1]) * map_tool.RAIL_HALF[1], 3))
                for f, s in map_tool.RAIL_ENDS})
WRONG_STEPS = [(71, 50), (35.5, 0), (0, 25), (106.5, 50), (71, 75)]


def random_point(rng):
    return (round(rng.randrange(-20, 60) * 35.5 + 0.5, 3), round(rng.randrange(-20, 60) * 25.0, 3))


def walk(rng, start, length, wrong=0.05):
    points = [start]
    for _ in range(length):
        dx, dy = rng.choice(WRONG_STEPS) if rng.random() < wrong else rng.choice(STEPS)
        x, y = points[-1]
        points.append((round(x + dx, 3), round(y + dy, 3)))
    return points


LEVEL = 76    # the height of the rail points of the generated tracks (rails at 66)


def valid_walk(rng, rails, start, length, z=None):
    """A walk whose steps the model accepts, now and then a random one."""
    points = [start]
    for _ in range(length):
        x, y = points[-1]
        options = []
        for dx, dy in STEPS:
            nxt = (round(x + dx, 3), round(y + dy, 3))
            try:
                map_tool.plan_track(rails, points + [nxt], None, z)
                options.append(nxt)
            except map_tool.TrackError:
                pass
        if options and rng.random() < 0.9:
            points.append(rng.choice(options))
        else:
            dx, dy = rng.choice(STEPS + WRONG_STEPS)
            points.append((round(x + dx, 3), round(y + dy, 3)))
    return points


def generate(rng, count):
    cases = []
    for _ in range(count // 4):
        a = random_point(rng)
        dx, dy = rng.choice(STEPS + WRONG_STEPS)
        cases.append(("C", a, (round(a[0] + dx, 3), round(a[1] + dy, 3))))
    for _ in range(count // 2):
        rails = []
        segments = []
        # existing rails: a valid track laid by the model, sometimes with a few loose rails,
        # sometimes with ramps (one point 11 higher or lower) or a whole track at another height
        base_z = LEVEL if rng.random() < 0.8 else LEVEL + rng.choice((-11, 11, 22))
        base = valid_walk(rng, [], random_point(rng), rng.randrange(1, 8))
        try:
            segments = map_tool.plan_track([], base)
            rails = [(x, y, s, base_z, base_z) for _a, _b, x, y, s, *_r in segments]
        except map_tool.TrackError:
            segments = []
        for _ in range(rng.randrange(0, 3)):
            x, y = random_point(rng)
            rails.append((x, y, rng.randrange(12), LEVEL, LEVEL))
        if rails and rng.random() < 0.3:
            i = rng.randrange(len(rails))
            x, y, s, z0, z1 = rails[i]
            rails[i] = (x, y, s, z0 + rng.choice((0, 11, -11)), z1 + rng.choice((0, 11, -11)))
        z = rng.choice((None, LEVEL, LEVEL, LEVEL + 11))
        if rails and rng.random() < 0.7:   # start at an end of an existing rail: a branch
            x, y, s, _z0, _z1 = rng.choice(rails)
            end = rng.choice(map_tool.RAIL_ENDS[s])
            start = (round(x + end[0] * map_tool.RAIL_HALF[0], 3), round(y + end[1] * map_tool.RAIL_HALF[1], 3))
        else:
            start = random_point(rng)
        kind = rng.random()
        if kind < 0.1 and len(segments) > 1:     # a rail of the track again, from a joint
            a, b, *_r = segments[rng.randrange(1, len(segments))]
            points = [a, b]
        elif kind < 0.2 and len(segments) > 1:   # a crowded joint: its rails there several times
            i = rng.randrange(1, len(segments))
            a = segments[i][0]
            rails = rails + [rails[i]] * rng.randrange(2, 7)
            points = valid_walk(rng, rails, a, rng.randrange(1, 4), z)
        elif kind < 0.3 and len(segments) > 1:   # ramps from one point: far points at many heights
            x, y, s, z0, z1 = rails[rng.randrange(1, len(segments))]
            rails = rails + [(x, y, s, z0, z1 + 11 * k) for k in range(1, rng.randrange(3, 9))]
            end = map_tool.RAIL_ENDS[s][0]
            a = (round(x + end[0] * map_tool.RAIL_HALF[0], 3), round(y + end[1] * map_tool.RAIL_HALF[1], 3))
            dx, dy = rng.choice(STEPS)
            points = [a, (round(a[0] + dx, 3), round(a[1] + dy, 3))]
            z = z0 if rng.random() < 0.8 else z
        else:                             # mostly steps the model accepts
            points = valid_walk(rng, rails, start, rng.randrange(1, 7), z)
        shapes = [rng.randrange(12) for _ in points[1:]] if rng.random() < 0.1 else []
        cases.append(("P", rails, points, shapes, z))
    for _ in range(count // 16):
        x, y = random_point(rng)
        cases.append(("R", x, y, rng.choice((66, 66.5, 0, -7.25, round(rng.uniform(-50, 300), 3))),
                      rng.randrange(256)))
    for _ in range(count // 8):
        # a joint with rails of every shape ending there, at a few heights, some twice
        point = random_point(rng)
        rails = []
        for _ in range(rng.randrange(1, 14)):
            shape = rng.randrange(12)
            end = rng.choice(map_tool.RAIL_ENDS[shape])
            x = round(point[0] - end[0] * map_tool.RAIL_HALF[0], 3)
            y = round(point[1] - end[1] * map_tool.RAIL_HALF[1], 3)
            rails.append((x, y, shape, rng.choice((LEVEL, LEVEL, LEVEL + 11)),
                          rng.choice((LEVEL, LEVEL + 11, LEVEL - 11))))
            if rng.random() < 0.2:
                rails.append(rails[-1])
        cases.append(("J", rails, point))
    for _ in range(count - len(cases)):
        width, height = rng.randrange(1, 40), rng.randrange(1, 40)
        segments = []
        for _ in range(rng.randrange(1, 4)):
            a = (rng.uniform(-20, width * 8 + 20), rng.uniform(-20, height * 8 + 20))
            dx, dy = rng.choice(STEPS)
            segments.append((a, (a[0] + dx, a[1] + dy)))
        cases.append(("T", width, height, segments))
    return cases


def rails_text(rails):
    return " ".join([str(len(rails))] + [f"{x} {y} {s} {z0} {z1}" for x, y, s, z0, z1 in rails])


def line_of(case):
    if case[0] == "C":
        _k, a, b = case
        return f"C {a[0]} {a[1]} {b[0]} {b[1]}"
    if case[0] == "P":
        _k, rails, points, shapes, z = case
        return ("P " + " ".join([rails_text(rails)] +
                                [str(len(points))] + [f"{x} {y}" for x, y in points] +
                                [str(len(shapes))] + [str(s) for s in shapes] +
                                ["any" if z is None else str(z)]))
    if case[0] == "R":
        return "R {} {} {} {}".format(*case[1:])
    if case[0] == "J":
        _k, rails, point = case
        return f"J {rails_text(rails)} {point[0]} {point[1]}"
    _k, width, height, segments = case
    return f"T {width} {height} {len(segments)} " + " ".join(f"{a[0]!r} {a[1]!r} {b[0]!r} {b[1]!r}"
                                                             for a, b in segments)


def model_of(case):
    if case[0] == "C":
        return model_candidates(case[1], case[2])
    if case[0] == "P":
        return model_plan(case[1], case[2], case[3], case[4])
    if case[0] == "R":
        return model_rail(*case[1:])
    if case[0] == "J":
        return model_points(case[1], case[2])
    return model_cells(case[1], case[2], case[3])


def main(argv):
    count = int(argv[0]) if argv else 3000
    build_test_host()
    rng = random.Random(610)
    cases = generate(rng, count)
    result = subprocess.run([TEST_EXE], input="\n".join(line_of(c) for c in cases) + "\n",
                            capture_output=True, text=True)
    answers = result.stdout.splitlines()
    if len(answers) != len(cases):
        sys.exit(f"the test host answered {len(answers)} of {len(cases)} cases")
    failed = 0
    kinds = {}
    for case, answer in zip(cases, answers):
        expected = model_of(case)
        key = case[0] if case[0] != "P" else "P " + expected.split()[0]
        kinds[key] = kinds.get(key, 0) + 1
        if answer.strip() != expected.strip():
            failed += 1
            if failed <= 10:
                print(f"DIFFERENT: {line_of(case)}\n  C++:    {answer}\n  Python: {expected}")
    print("cases: " + ", ".join(f"{k} {v}" for k, v in sorted(kinds.items())))
    print(f"{len(cases) - failed} of {len(cases)} the same")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
