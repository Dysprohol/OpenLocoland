r"""Differential test of the rail rules: the in-game editor's C++ (src/LocoFix/rail_rules.h)
against tools/map_tool.py, so the two do not drift apart.

    python tools\test_rail_rules.py [cases]

Builds tools\test_rail_rules.cpp (which includes rail_rules.h) with MSVC into build\tests,
generates cases - pairs of points for rail_candidates, tracks next to existing rails for
plan_track (lattice steps with some invalid ones, branches from the rails' ends, hand-picked
shapes, rails and ramps at other heights, the track's height given or not), segments for
track_cells, rail objects for rail_of (every direction, ramps), the rail points of crowded
joints for rail_point_links - and compares the answers. Then the tracks of SCENARIOS and CLUSTERS,
each in its four mirror images and among denser track, laid as the editor's Track tool lays them
(a click a rail, `lay`: only a point of more than 6 links is refused) and as map_tool.py track lays
them, with the answers each must get; and every rail of the missions' maps and of the player's own
maps of the installed game (OpenLocoland's folder, else ..\Locoland, else build\game; a map or a
folder that is not there is skipped with a note) laid again where it lies, with the rails before it
in the map there - each must be laid. Exits with 1 on a difference or a wrong answer.
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


def height_text(z):
    return "any" if z is None else str(z)


def model_plan(rails, points, shapes, z, through=False, lay=False, ramp=0):
    try:
        segments = map_tool.plan_track(rails, points, shapes or None, z, through, lay, ramp)
    except map_tool.TrackError as e:
        return f"{e.code} {e.step}"
    return "ok -1" + "".join(f" {num(x)} {num(y)} {shape} {num(e0[0])} {num(e0[1])} {num(e1[0])} {num(e1[1])}"
                             f" {height_text(z0)} {height_text(z1)};"
                             for _a, _b, x, y, shape, e0, e1, z0, z1 in segments)


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
    for _ in range(count // 2 - count // 8):
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
        cases.append(("P", rails, points, shapes, z, rng.random() < 0.5, rng.random() < 0.5,
                      rng.choice((0, 0, 0, 1, -1)) if z is not None else 0))
    for _ in range(count // 8):
        # a rail laid as the editor lays a line, a click at a time (through), ending at a joint of
        # other track: the rails of a straight or curved track ending there, crossed by a step of any shape
        point = random_point(rng)
        rails = []
        for _ in range(rng.randrange(1, 4)):
            shape = rng.randrange(12)
            end = rng.choice(map_tool.RAIL_ENDS[shape])
            rails.append((round(point[0] - end[0] * map_tool.RAIL_HALF[0], 3),
                          round(point[1] - end[1] * map_tool.RAIL_HALF[1], 3), shape, LEVEL, LEVEL))
        dx, dy = rng.choice(STEPS)
        start = (round(point[0] - dx, 3), round(point[1] - dy, 3))
        z = rng.choice((None, LEVEL))
        cases.append(("P", rails, [start, point], [], z, rng.random() < 0.7, rng.random() < 0.7,
                      rng.choice((0, 0, 1, -1)) if z is not None else 0))
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


# Tracks over other track and crowded joints, laid as the editor's Track tool lays them - a plan a
# click, one rail, `through` (the line goes on) and `lay` (it lays what is clicked: only a point of
# more than 6 links is refused) - and, for SCENARIOS, as map_tool.py track lays them: the whole line in
# one plan with every rule. Each in its four mirror images (the lattice is symmetric: the shapes map
# onto shapes, so the answers must be the same) and among denser track (DENSE: a track through every
# joint 71 / 50 px around, parallels half a step away on both sides).
HX, HY = map_tool.RAIL_HALF


def track(x0, y0, dx, dy, shape, n):
    """n rails of a shape in a row from the joint (x0, y0), each a step (dx, dy) on."""
    return [(round(x0 + dx * k + dx / 2, 3), round(y0 + dy * k + dy / 2, 3), shape, LEVEL, LEVEL) for k in range(n)]


H_TRACK = track(-142, 0, 71, 0, 1, 4)        # joints x -142 ... 142, y 0
V_TRACK = track(0, -100, 0, 50, 0, 4)        # joints y -100 ... 100, x 0
D_TRACK = track(-71, -50, 71, 50, 3, 3)      # a diagonal through (0, 0)
C_TRACK = [(35.5, 25.0, 8, LEVEL, LEVEL)]   # a curve from the joint (0, 0) (end (-1, 0)) to (71, 50)
H_BELOW = track(-142, 50, 71, 0, 1, 6)       # a parallel a step below
H_HALF = track(-142, 25, 71, 0, 1, 4)        # a parallel half a step below
V_71 = track(71, -100, 0, 50, 0, 4)          # a vertical through the next joint (71, 0)
DENSE = (track(-213, -50, 71, 0, 1, 6) + track(-213, 50, 71, 0, 1, 6) + track(-213, -25, 71, 0, 1, 6) +
         track(-213, 25, 71, 0, 1, 6) + track(-71, -150, 0, 50, 0, 6) + track(71, -150, 0, 50, 0, 6))

# (name, the rails there, the line's points, what map_tool.py track says of the whole line); the
# editor lays every one of them a click a rail
SCENARIOS = [
    ("vertical across a horizontal at its joint", H_TRACK, [(0, -100), (0, -50), (0, 0), (0, 50), (0, 100)], "ok"),
    ("vertical across a horizontal mid-rail", H_TRACK, [(35.5, -100), (35.5, -50), (35.5, 0), (35.5, 50)], "ok"),
    ("horizontal across a vertical at its joint", V_TRACK, [(-142, 0), (-71, 0), (0, 0), (71, 0), (142, 0)], "ok"),
    ("diagonal across a horizontal at its joint", H_TRACK, [(-142, -100), (-71, -50), (0, 0), (71, 50)], "ok"),
    ("diagonal across a horizontal mid-rail", H_TRACK, [(-106.5, -75), (-35.5, -25), (35.5, 25), (106.5, 75)], "ok"),
    ("horizontal across a diagonal at its joint", D_TRACK, [(-142, 0), (-71, 0), (0, 0), (71, 0), (142, 0)], "ok"),
    ("diagonal across a diagonal at its joint", D_TRACK, [(-71, 50), (0, 0), (71, -50)], "ok"),
    ("vertical across a curve's joint", C_TRACK, [(0, -100), (0, -50), (0, 0), (0, 50)], "ok"),
    ("parallel horizontal a half step below", H_TRACK, [(-142, 25), (-71, 25), (0, 25), (71, 25), (142, 25)], "ok"),
    ("parallel vertical a half step aside", V_TRACK, [(35.5, -100), (35.5, -50), (35.5, 0), (35.5, 50)], "ok"),
    ("horizontal over a horizontal, half a rail along", H_TRACK, [(-106.5, 0), (-35.5, 0)], "ok"),
    ("the same rail again", H_TRACK, [(-71, 0), (0, 0)], "duplicate"),
    ("a branch: a curve from a joint", H_TRACK, [(0, 0), (71, 25), (142, 75)], "ok"),
    ("starting across at another track's joint", H_TRACK, [(0, 0), (0, 50), (0, 100)], "join"),
    ("ending across at another track's joint", H_TRACK, [(0, -100), (0, -50), (0, 0)], "last"),
    ("ending on a rail end the same way (a fork with no stem)", [(35.5, 0.0, 1, LEVEL, LEVEL)], [(142, 75), (71, 25), (0, 0)], "last"),
    ("a crossover between parallels a step apart", H_TRACK + H_BELOW, [(0, 0), (71, 25), (142, 50)], "ok"),
    ("a curve onto a parallel half a step away", H_TRACK + H_HALF, [(0, 0), (71, 25)], "last"),
    ("a vertical between two crossings 71 px apart", H_TRACK + V_TRACK + V_71, [(35.5, -100), (35.5, -50), (35.5, 0), (35.5, 50)], "ok"),
    ("a sharp turn", [], [(0, 0), (71, 0), (71, 50)], "join"),
    ("a turn of 90 degrees in two curves", [], [(0, 0), (71, 0), (142, 25), (177.5, 75)], "ok"),
]

P = (0.0, 0.0)


def star_clicks():
    """From the joint P of H_TRACK, every other rail of the lattice that ends there, out to its far end."""
    clicks = []
    for shape, ends in enumerate(map_tool.RAIL_ENDS):
        for i in range(2):
            e, other = ends[i], ends[1 - i]
            centre = (-e[0] * HX, -e[1] * HY)
            far = (round(centre[0] + other[0] * HX, 3), round(centre[1] + other[1] * HY, 3))
            if shape == 1 and far in ((71.0, 0.0), (-71.0, 0.0)):
                continue   # the track's own two rails
            clicks.append((P, far, 0, shape))   # the shape: some two points are joined by two
    return clicks


# (name, the rails there, the clicks (from, to, ramp), the answer of each click); "ok" but where the
# engine's limit of 6 links a point refuses
CLUSTERS = [
    ("a star: every rail of the lattice from one joint", H_TRACK, star_clicks(), ["ok"] * 22),
    ("ramps from one joint to its 6-link limit", H_TRACK,
     [(P, (71, 25), 0), (P, (71, -25), 0), (P, (71, 0), 1), (P, (71, 25), 1), (P, (71, -25), 1), (P, (71, 0), -1), (P, (71, 25), -1)],
     ["ok", "ok", "ok", "ok", "links", "links", "links"]),
    ("a crossing, then a branch at its joint", H_TRACK,
     [((0, -100), (0, -50), 0), ((0, -50), P, 0), (P, (0, 50), 0), ((0, 50), (0, 100), 0), (P, (71, 25), 0)], ["ok"] * 5),
    ("a branch, then a crossing at its joint", H_TRACK,
     [(P, (71, 25), 0), ((0, -100), (0, -50), 0), ((0, -50), P, 0), (P, (0, 50), 0)], ["ok"] * 4),
    ("switches both ways and both sides at one joint, then straights from it", H_TRACK,
     [(P, (71, 25), 0), (P, (71, -25), 0), (P, (-71, 25), 0), (P, (-71, -25), 0), (P, (0, 50), 0), (P, (71, 0), 0)], ["ok"] * 6),
    ("lines beside a crowded joint and across it mid-rail", H_TRACK, star_clicks() + [
        ((-142, 25), (-71, 25), 0), ((-71, 25), (0, 25), 0), ((-35.5, 25), (35.5, -25), 0)], ["ok"] * 25),
]


def mirror_table(fx, fy):
    """For each shape its mirror: (shape, the ends swapped)."""
    out = []
    for f, s in map_tool.RAIL_ENDS:
        mf, ms = (f[0] * fx, f[1] * fy), (s[0] * fx, s[1] * fy)
        for k, (g, h) in enumerate(map_tool.RAIL_ENDS):
            if (g, h) == (mf, ms):
                out.append((k, False))
                break
            if (g, h) == (ms, mf):
                out.append((k, True))
                break
        else:
            raise SystemExit(f"no mirror of shape {len(out)} ({fx}, {fy}): a shape table is wrong")
    return out


MIRRORS = [("", 1, 1), (" (mirrored in x)", -1, 1), (" (mirrored in y)", 1, -1), (" (mirrored in x and y)", -1, -1)]


def mirror_rails(rails, fx, fy):
    table = mirror_table(fx, fy)
    return [(round(x * fx, 3) + 0.0, round(y * fy, 3) + 0.0, table[sh][0], *((z1, z0) if table[sh][1] else (z0, z1)))
            for x, y, sh, z0, z1 in rails]


def mirror_point(p, fx, fy):
    return (round(p[0] * fx, 3) + 0.0, round(p[1] * fy, 3) + 0.0)


def lay_clicks(cases, rails, clicks):
    """The clicks (from, to, ramp[, shape]) laid one after another as the Track tool lays them; the
    answer of each."""
    laid, answers = list(rails), []
    for a, b, ramp, *shape in clicks:
        cases.append(("P", list(laid), [a, b], shape, LEVEL, True, True, ramp))
        answer = model_plan(laid, [a, b], shape, LEVEL, True, True, ramp).split()[0]
        answers.append(answer)
        if answer == "ok":
            laid += [(x, y, sh, z0, z1) for _a, _b, x, y, sh, _e0, _e1, z0, z1 in
                     map_tool.plan_track(laid, [a, b], shape or None, LEVEL, True, True, ramp)]
    return answers, laid


def scenario_cases():
    """The P cases of the scenarios and clusters, and the checks (name, the answer wanted, the model's)."""
    cases, checks = [], []
    for suffix, fx, fy in MIRRORS:
        for dense in (False, True):
            more = DENSE if dense else []
            tag = suffix + (" among denser track" if dense else "")
            for name, rails, points, want_line in SCENARIOS:
                pts = [mirror_point((round(x, 3), round(y, 3)), fx, fy) for x, y in points]
                there = mirror_rails(rails + more, fx, fy)
                answers, _laid = lay_clicks(cases, there, [(a, b, 0) for a, b in zip(pts, pts[1:])])
                checks.append((name + tag + ", click by click", ["ok"] * (len(pts) - 1), answers))
                if not dense:
                    cases.append(("P", there, pts, [], LEVEL, False, False, 0))
                    checks.append((name + tag + ", in one go", want_line, model_plan(there, pts, [], LEVEL).split()[0]))
            for name, rails, clicks, want in CLUSTERS:
                there = mirror_rails(rails + more, fx, fy)
                table = mirror_table(fx, fy)
                answers, laid = lay_clicks(cases, there, [(mirror_point(a, fx, fy), mirror_point(b, fx, fy), r, *[table[k][0] for k in sh])
                                                          for a, b, r, *sh in clicks])
                checks.append((name + tag, want, answers))
                if name.startswith("a star"):   # at one height a point takes 3 rails and the one opposite
                    links = map_tool.rail_point_links(laid, mirror_point(P, fx, fy))
                    checks.append((name + tag + ": links of each point", [4] * 8, sorted(links.values())))
    return cases, checks


def continuing():
    """From each end of a rail of every shape the steps of the lattice a line takes there without
    another rule broken: three (straight on and a curve to each side) - the ends that have another number."""
    wrong = []
    for shape in range(12):
        rail = (0.0, 0.0, shape, LEVEL, LEVEL)
        for end in map_tool.RAIL_ENDS[shape]:
            p = (round(end[0] * HX, 3), round(end[1] * HY, 3))
            ok = 0
            for dx, dy in STEPS:
                if model_plan([rail], [p, (round(p[0] + dx, 3), round(p[1] + dy, 3))], [], LEVEL, True).startswith("ok"):
                    ok += 1
            if ok != 3:
                wrong.append(f"shape {shape} end {end}: {ok}")
    return wrong


# ---- every rail of the game's maps laid again where it lies ----

RAIL_VIDS = {610: 0, 611: 0, 600: -3, 601: -3}   # the swamp rails' points stand 7 over them, not 10


def game_folder():
    for folder in (os.path.join(os.environ.get("USERPROFILE", ""), "Games", "OpenLocoland"),
                   os.path.join(os.path.dirname(REPO), "Locoland"), os.path.join(REPO, "build", "game")):
        if os.path.isfile(os.path.join(folder, "objects.res")) and os.path.isdir(os.path.join(folder, "maps")):
            return folder
    return None


def map_rails_in_order(model):
    rails = []
    for row in model.get("objects", []):
        nvid = map_tool.parse_int(row["nvid"])
        if nvid in RAIL_VIDS:
            x, y, shape, z0, z1 = map_tool.rail_of(float(row["x"]), float(row["y"]), float(row["z"]), map_tool.parse_int(row["direction"]))
            rails.append((x, y, shape, z0 + RAIL_VIDS[nvid], z1 + RAIL_VIDS[nvid]))
    return rails


def map_rail_cases():
    """The P cases of every rail of the missions, POLIGON, the template and the player's maps (each laid
    as one click of the Track tool, the rails before it in the map there - those within 180 px), and the
    lines of the report: per map the rails, those laid, and what the old rules refused of them."""
    folder = game_folder()
    if not folder:
        return [], ["maps: no game folder found - the maps' rails are not checked"]
    info = map_tool.object_info(os.path.join(folder, "objects.res"))
    maps = [os.path.join(folder, "maps", f"l{n:02d}", f"l{n:02d}.map") for n in range(1, 21)]
    maps += [os.path.join(folder, "maps", d, d + ".map") for d in ("sandbox", "template")]
    custom = os.path.join(folder, "maps", "custom.lst")
    if os.path.isfile(custom):
        for line in open(custom, encoding="latin-1"):
            name = line.split()[0] if line.split() else ""
            path = os.path.join(folder, "maps", name, name + ".map")
            if name and os.path.normcase(path) not in map(os.path.normcase, maps) and os.path.isfile(path):
                maps.append(path)
    cases, report = [], [f"maps of {folder}:"]
    for path in maps:
        if not os.path.isfile(path):
            report.append(f"  {os.path.basename(path)}: not there, skipped")
            continue
        rails = map_rails_in_order(map_tool.read_map(open(path, "rb").read(), info))
        cells = {}
        laid, strict, odd = 0, {}, 0
        refused = {}
        for r in rails:
            x, y, shape, z0, z1 = r
            near = [q for cx in (-1, 0, 1) for cy in (-1, 0, 1) for q in cells.get((int(x // 180) + cx, int(y // 180) + cy), [])
                    if abs(q[0] - x) < 180 and abs(q[1] - y) < 180]
            cells.setdefault((int(x // 180), int(y // 180)), []).append(r)
            if z1 - z0 not in (-11, 0, 11):
                odd += 1
                continue
            f, s2 = map_tool.RAIL_ENDS[shape]
            a = (round(x + f[0] * HX, 3), round(y + f[1] * HY, 3))
            b = (round(x + s2[0] * HX, 3), round(y + s2[1] * HY, 3))
            ramp = (z1 - z0) // 11
            cases.append(("P", near, [a, b], [shape], z0, True, True, ramp))
            answer = model_plan(near, [a, b], [shape], z0, True, True, ramp).split()[0]
            if answer == "ok":
                laid += 1
            else:
                refused[answer] = refused.get(answer, 0) + 1
            old = model_plan(near, [a, b], [shape], z0, True, False, ramp).split()[0]
            if old != "ok":
                strict[old] = strict.get(old, 0) + 1
        note = "" if not refused else "  NOT LAID " + ", ".join(f"{k} {v}" for k, v in sorted(refused.items()))
        old = ", ".join(f"{k} {v}" for k, v in sorted(strict.items())) or "none"
        report.append(f"  {os.path.basename(path)}: {len(rails)} rails, {laid} laid again{note}; "
                      f"the old rules refused: {old}" + (f"; {odd} of other heights skipped" if odd else ""))
    return cases, report


def rails_text(rails):
    return " ".join([str(len(rails))] + [f"{x} {y} {s} {z0} {z1}" for x, y, s, z0, z1 in rails])


def line_of(case):
    if case[0] == "C":
        _k, a, b = case
        return f"C {a[0]} {a[1]} {b[0]} {b[1]}"
    if case[0] == "P":
        _k, rails, points, shapes, z, through, lay, ramp = case
        return ("P " + " ".join([rails_text(rails)] +
                                [str(len(points))] + [f"{x} {y}" for x, y in points] +
                                [str(len(shapes))] + [str(s) for s in shapes] +
                                ["any" if z is None else str(z), "1" if through else "0", "1" if lay else "0", str(ramp)]))
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
        return model_plan(*case[1:])
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
    extra, checks = scenario_cases()
    cases += extra
    map_cases, map_report = map_rail_cases()
    cases += map_cases
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
    wrong = 0
    for name, want, got in checks:
        if want != got:
            wrong += 1
            print(f"SCENARIO {name}: {got} (want {want})")
    print(f"{len(checks) - wrong} of {len(checks)} track scenarios as they should be (mirrored and among denser track)")
    ends = continuing()
    for e in ends:
        print(f"CONTINUING {e} steps, not 3")
    print(f"{24 - len(ends)} of 24 rail ends go on in 3 steps")
    for line in map_report:
        print(line)
        if " NOT LAID" in line:
            wrong += 1
    return 1 if failed or wrong or ends else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
