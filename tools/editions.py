r"""The store of the game's editions - a developer's tool for a release, not part of Setup.

Every edition of the game we know of (the six Steam depots, the CDs, the beta/demo) is checked
with our changes before a release: `check` assembles each one, places our patches in it, builds
the game from it and, with --run, starts the game.

    python tools\editions.py capture <code> <folder>    record an edition (and store its files)
    python tools\editions.py refresh <code> <folder>    the same again, with what changed since
    python tools\editions.py list                       the editions recorded
    python tools\editions.py diff <code> <code>         the files that differ between two
    python tools\editions.py assemble <code> <folder>   the whole edition in a folder (links)
    python tools\editions.py check [--run] [<code> ...] build (and start) each edition
                                   [--captured <folder>] [--store <folder>]

A code is <language>-<source>: en-steam, de-steam, hu-steam, pl-steam, pt-steam, ru-steam (the
depots: `download_depot 352130 3521NN` in the Steam console puts depot 3521NN into
steamapps\content\app_352130\depot_3521NN), and for an edition from a disc cs-cd, pl-cd, it-cd,
en-beta-demo... A new edition is one folder of the game's files (an install of the disc;
`C:\Locoland Decomp\notes\diffs\<name>` by our convention) and `capture <code> <folder>`.

The manifest tools/editions/<code>.tsv (in git: names, sizes and SHA-256 - nothing of the game)
lists every file of the edition. The files themselves go to the local store editions/<code>/
(outside git, as captured/): en-steam, the base, as a whole copy (about 550 MB - the Steam
folder and the depots change when Steam switches the language or updates, so the store keeps
its own copy), every other edition only the files that differ from en-steam or that en-steam
does not have. An installer's own files (unins*.*, *.log, a logs folder) are left out, and so
are the score tables (hiscores) of a disc's install - the player's; the depots ship their own.

`check` assembles each edition into build\editions\<code>\steam (hard links into the store),
reports for each of our patches whether it is placed at its line numbers, shifted, or cannot be
placed (patch_text), builds the game with build.py into build\editions\<code>\game - without
--skip-verify first, and with it when that is refused - and with --run starts it with
tools\test_game.ps1: to the main menu (the intro), and POLIGON with its Esc menu when LocoFix is
loaded (the Steam exe), each time looking for LOGIC errors in error.log. A table ends it.
"""
import argparse
import datetime
import fnmatch
import hashlib
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
sys.path.insert(0, HERE)
import build  # noqa: E402
import patch_text  # noqa: E402

MANIFESTS = os.path.join(HERE, "editions")
STORE = os.path.join(REPO, "editions")
WORK = os.path.join(REPO, "build", "editions")
BASE = "en-steam"
LEFT_OUT = ["unins*", "*.log"]          # an installer's files, the game's logs
LEFT_OUT_DIRS = {"logs"}
LEFT_OUT_INSTALLED = ["hiscores"]       # of a disc's install: the player's score tables (the depots ship theirs)


def sha256(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def scan(folder, installed=False):
    """{relative path: (size, sha256)} of the edition's files in `folder` (`installed`: a disc's
    install, whose score tables are the player's)."""
    files = {}
    left_out = LEFT_OUT + (LEFT_OUT_INSTALLED if installed else [])
    for directory, dirs, names in os.walk(folder):
        dirs[:] = [d for d in dirs if d.lower() not in LEFT_OUT_DIRS]
        for name in names:
            if any(fnmatch.fnmatch(name.lower(), p) for p in left_out):
                continue
            path = os.path.join(directory, name)
            try:
                files[os.path.relpath(path, folder)] = (os.path.getsize(path), sha256(path))
            except OSError as e:   # e.g. taken by the antivirus (the Italian CD's CDPRO.BIN)
                print(f"  {os.path.relpath(path, folder)}: cannot be read, left out ({e.strerror})")
    return files


def manifest_path(code):
    return os.path.join(MANIFESTS, code + ".tsv")


def read_manifest(code):
    """(meta {source, captured}, files {path: (size, sha)}) or None when there is none."""
    try:
        lines = open(manifest_path(code), encoding="utf-8").read().splitlines()
    except OSError:
        return None
    meta, files = {}, {}
    for line in lines:
        if line.startswith("# ") and "\t" in line:
            key, value = line[2:].split("\t", 1)
            meta[key] = value
        elif line and not line.startswith("#") and not line.startswith("path\t"):
            path, size, sha = line.split("\t")
            files[path] = (int(size), sha)
    return meta, files


def source_description(folder):
    """What the manifest (public, in git) says the edition came from - no local path: "steam depot
    352136", "steam install", or the folder's name with its parent's ("Steamland CZ/Steamland",
    written with a backslash)."""
    parts = os.path.normpath(os.path.abspath(folder)).split(os.sep)
    depot = re.fullmatch(r"depot_(\d+)", parts[-1], re.I)
    if depot:
        return f"steam depot {depot.group(1)}"
    if len(parts) > 2 and parts[-2].lower() == "common" and parts[-3].lower() == "steamapps":
        return "steam install"
    if len(parts) > 1 and parts[-2].lower() not in ("diffs", "") and not parts[-2].endswith(":"):
        return parts[-2] + "\\" + parts[-1]
    return parts[-1]


def source_file(code, store=STORE):
    """The local note of the folder an edition was captured from (beside its store, outside git)."""
    return os.path.join(store, code + ".source.txt")


def local_source(code, store=STORE):
    try:
        return open(source_file(code, store), encoding="utf-8").read().strip()
    except OSError:
        return None


def write_manifest(code, source, files):
    os.makedirs(MANIFESTS, exist_ok=True)
    with open(manifest_path(code), "w", encoding="utf-8", newline="\n") as f:
        f.write("# An edition of the game: every file, its size and SHA-256 (tools/editions.py)\n")
        f.write(f"# code\t{code}\n# source\t{source}\n# captured\t{datetime.date.today().isoformat()}\n")
        f.write("path\tsize\tsha256\n")
        for path in sorted(files, key=str.lower):
            f.write(f"{path}\t{files[path][0]}\t{files[path][1]}\n")


def lower_map(files):
    return {p.lower(): v for p, v in files.items()}


def differing(files, base):
    """The paths of `files` that `base` does not have or has with other content."""
    b = lower_map(base)
    return [p for p, v in files.items() if b.get(p.lower(), (None, None))[1] != v[1]]


def exe_of(files):
    """(size, sha) of the edition's Steam.exe (any case), or None."""
    return next((v for p, v in files.items() if p.lower() == "steam.exe"), None)


def exe_text(files):
    exe = exe_of(files)
    if not exe:
        return "no exe"
    return "Steam 2003-10-07" if exe[1] == build.STEAM_EXE_SHA256 else f"other, {exe[0]} B"


def capture(code, folder, store=STORE, report_changes=False):
    if not re.fullmatch(r"[a-z]{2}-[a-z0-9-]+", code):
        sys.exit(f"{code}: a code is <language>-<source>, e.g. ru-steam, cs-cd")
    if not os.path.isdir(folder):
        sys.exit(f"No folder {folder}")
    base = read_manifest(BASE)
    if code != BASE and not base:
        sys.exit(f"Capture {BASE} first: the other editions keep only what differs from it")
    old = read_manifest(code)
    files = scan(folder, installed=source_kind(code) != "steam")
    keep = list(files) if code == BASE else differing(files, base[1])
    target = os.path.join(store, code)
    stored = 0
    for path in keep:
        dst = os.path.join(target, path)
        if os.path.isfile(dst) and os.path.getsize(dst) == files[path][0] and sha256(dst) == files[path][1]:
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copy2(os.path.join(folder, path), dst)
        stored += 1
    wanted = {p.lower() for p in keep}
    for directory, _, names in os.walk(target, topdown=False):   # what the store keeps no more
        for name in names:
            path = os.path.join(directory, name)
            if os.path.relpath(path, target).lower() not in wanted:
                os.remove(path)
        if directory != target and not os.listdir(directory):
            os.rmdir(directory)
    write_manifest(code, source_description(folder), files)
    os.makedirs(store, exist_ok=True)
    with open(source_file(code, store), "w", encoding="utf-8") as f:
        f.write(os.path.abspath(folder) + "\n")
    print(f"{code}: {len(files)} files ({exe_text(files)}), {len(keep)} in the store"
          f"{'' if code == BASE else f' (differ from {BASE})'}, {stored} copied now")
    if report_changes:
        if not old:
            print(f"{code}: no earlier manifest")
            return
        before, after = lower_map(old[1]), lower_map(files)
        added = sorted(p for p in after if p not in before)
        removed = sorted(p for p in before if p not in after)
        changed = sorted(p for p in after if p in before and after[p][1] != before[p][1])
        print(f"{code}: since {old[0].get('captured', '?')}: {len(added)} added, {len(removed)} removed, "
              f"{len(changed)} changed")
        for title, paths in (("added", added), ("removed", removed), ("changed", changed)):
            for p in paths:
                print(f"  {title}: {p}")


def codes():
    if not os.path.isdir(MANIFESTS):
        return []
    found = [f[:-4] for f in os.listdir(MANIFESTS) if f.endswith(".tsv")]
    return sorted(found, key=lambda c: (c.split("-")[0], c))


def source_kind(code):
    return "steam" if code.endswith("-steam") else "cd"


def list_editions():
    base = read_manifest(BASE)
    manifests = {code: read_manifest(code) for code in codes()}
    rows = [("code", "language", "source", "exe", "files", f"differ from {BASE}", "nearest (changed, only here)",
             "captured", "from")]
    for code, (meta, files) in manifests.items():
        differ = "-" if code == BASE or not base else str(len(differing(files, base[1])))
        rows.append((code, code.split("-")[0], source_kind(code), exe_text(files), str(len(files)), differ,
                     nearest(code, files, manifests), meta.get("captured", "?"),
                     local_source(code) or meta.get("source", "?")))
    table(rows)


def nearest(code, files, manifests):
    """The other edition closest to this one: its code, the files both have with other content and
    the files only this one has."""
    best = None
    for other, (_, other_files) in manifests.items():
        if other == code:
            continue
        o = lower_map(other_files)
        changed = sum(1 for p, v in files.items() if p.lower() in o and o[p.lower()][1] != v[1])
        only = sum(1 for p in files if p.lower() not in o)
        if best is None or (changed + only, changed) < (best[1] + best[2], best[1]):
            best = (other, changed, only)
    return f"{best[0]} ({best[1]}, {best[2]})" if best else "-"


def table(rows):
    widths = [max(len(r[i]) for r in rows) for i in range(len(rows[0]))]
    for n, r in enumerate(rows):
        print("  ".join(c.ljust(w) for c, w in zip(r, widths)).rstrip())
        if n == 0:
            print("  ".join("-" * w for w in widths))


def patch_targets():
    return {os.path.normcase(name) for _, name in build.steam_changes()}


def diff(a, b):
    ma, mb = read_manifest(a), read_manifest(b)
    for code, m in ((a, ma), (b, mb)):
        if not m:
            sys.exit(f"No manifest of {code}")
    fa, fb = lower_map(ma[1]), lower_map(mb[1])
    targets = patch_targets()
    names = {p.lower(): p for p in list(ma[1]) + list(mb[1])}
    shown = 0
    for key in sorted(names):
        if fa.get(key, (0, None))[1] == fb.get(key, (0, None))[1]:
            continue
        state = f"only in {a}" if key not in fb else f"only in {b}" if key not in fa else "differs"
        mark = "  * our patch" if os.path.normcase(names[key]) in targets else ""
        print(f"{names[key]}\t{state}{mark}")
        shown += 1
    print(f"{shown} files differ between {a} and {b}")


def assemble(code, folder, store=STORE):
    """The whole edition in `folder`: hard links into the store (copies where links fail)."""
    m = read_manifest(code)
    if not m:
        sys.exit(f"No manifest of {code}")
    base = read_manifest(BASE)
    if os.path.exists(folder):
        inside = os.path.normcase(os.path.abspath(folder)).startswith(os.path.normcase(os.path.join(REPO, "build")) + os.sep)
        if os.listdir(folder) and not inside:
            sys.exit(f"{folder} is not empty (only a folder in build\\ is cleared)")
        shutil.rmtree(folder)
    own = {p.lower() for p in (m[1] if code == BASE else differing(m[1], base[1]))}
    for path, (size, sha) in m[1].items():
        src = os.path.join(store, code if path.lower() in own else BASE, path)
        if not os.path.isfile(src) or os.path.getsize(src) != size:
            sys.exit(f"{code}: {path} is not in the store - capture the edition again")
        dst = os.path.join(folder, path)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        try:
            os.link(src, dst)
        except OSError:
            shutil.copy2(src, dst)
    return folder


def placement(folder):
    """For each of our patches of the Steam files: (change file, target, 'exact' / 'shifted +n' /
    'not placed: ...' / 'missing')."""
    out = []
    for change, name in build.steam_changes():
        path = os.path.join(folder, name)
        if not os.path.isfile(path):
            out.append((change, name, "missing"))
            continue
        placed = []
        try:
            patch_text.apply_patch(build.steam_original(folder, path), patch_text.read(change), change, placed)
        except patch_text.PatchError as e:
            out.append((change, name, "not placed: " + e.short()))
            continue
        shifts = sorted({s for s in placed if s})
        out.append((change, name, "exact" if not shifts else "shifted " + "/".join(f"{s:+d}" for s in shifts)))
    return out


def run_test(game, name, args):
    """test_game.ps1 on the built game; (output, error.log text, LocoFix.log text)."""
    for log in ("error.log", "LocoFix.log"):
        try:
            os.remove(os.path.join(game, "logs", log))
        except OSError:
            pass
    shot = os.path.join(os.path.dirname(game), name + ".png")
    cmd = ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", os.path.join(HERE, "test_game.ps1"),
           "-Exe", os.path.join(game, "Locoland.exe"), "-SetFull", "0", "-ClickOk", "15", "-LastShotOnly",
           "-Shot", shot] + args
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, errors="replace", timeout=420)
        output = result.stdout + result.stderr
    except subprocess.TimeoutExpired:
        output = "test_game.ps1 timed out"
    # a game left running (a disc's copy protection waiting for its CD...) is ended
    exe = os.path.join(game, "Locoland.exe").replace("'", "''")
    subprocess.run(["powershell", "-NoProfile", "-Command",
                    f"Get-Process Locoland -ErrorAction SilentlyContinue | Where-Object {{ $_.Path -eq '{exe}' }} | "
                    "Stop-Process -Force"], capture_output=True)

    def text(log):
        try:
            return open(os.path.join(game, "logs", log), encoding="cp1251", errors="replace").read()
        except OSError:
            return ""
    return output, text("error.log"), text("LocoFix.log")


ERROR_LINE = re.compile(r"!!!ERROR[^!]*!!!\s*(.*)")
# what an install as it is must not log: our scripts calling LocoFix's externs, the custom maps' list
AS_IS_ERRORS = ("Unknown extern", "custom.lst")


def error_kinds(log):
    """The `!!!ERROR` lines of an error.log: (count, [(n, kind)] most common first) - a kind is the
    message with its quoted and bracketed names (VID [110-<its Russian name>]) and numbers left out,
    in ASCII (the console's code page may not have the names' letters)."""
    kinds = {}
    for line in log.splitlines():
        m = ERROR_LINE.search(line)
        if m:
            kind = re.sub(r"(?<![A-Za-z])'[^']*'", "'.'", re.sub(r"\[[^\]]*\]", "[.]", m.group(1).strip()))
            kind = re.sub(r"\d+", "N", kind)[:70].encode("ascii", "replace").decode()
            kinds[kind] = kinds.get(kind, 0) + 1
    return sum(kinds.values()), sorted(((n, k) for k, n in kinds.items()), reverse=True)


def check(selected, run, captured, store=STORE):
    all_codes = codes()
    rows = [("edition", "exe", "patches", "build", "menu", "POLIGON", "LOGIC", "errors")]
    for code in selected or all_codes:
        if code not in all_codes:
            print(f"== {code}: no manifest (capture it first)")
            rows.append((code, "-", "no manifest", "-", "-", "-", "-"))
            continue
        files = read_manifest(code)[1]
        work = os.path.join(WORK, code)
        src = assemble(code, os.path.join(work, "steam"), store)
        print(f"== {code} ({exe_text(files)})")
        places = placement(src)
        for change, name, state in places:
            if state != "exact":
                print(f"  {os.path.relpath(change, REPO)} -> {name}: {state}")
        exact = sum(s == "exact" for _, _, s in places)
        shifted = sum(s.startswith("shifted") for _, _, s in places)
        unplaced = len(places) - exact - shifted
        summary = f"{exact} exact, {shifted} shifted, {unplaced} not placed"
        game = os.path.join(work, "game")
        build_state = "built"
        cmd = [sys.executable, os.path.join(HERE, "build.py"), "--steam", src, "--target", game]
        if captured:
            cmd += ["--captured", captured]
        with open(os.path.join(work, "build.log"), "w", encoding="utf-8") as log:
            result = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT)
            if result.returncode:
                log.write("\n--- refused; again with --skip-verify ---\n")
                log.flush()
                result = subprocess.run(cmd + ["--skip-verify"], stdout=log, stderr=subprocess.STDOUT)
                build_state = "skip-verify" if result.returncode == 0 else "FAILED"
        # a foreign exe is installed as it is (build.AS_IS): none of the patches goes in
        if getattr(build, "AS_IS", None) and build.AS_IS in open(os.path.join(work, "build.log"), encoding="utf-8",
                                                               errors="replace").read():
            summary = "as is"
        print(f"  patches: {summary}; build: {build_state} (log: {os.path.relpath(os.path.join(work, 'build.log'), REPO)})")
        menu = poligon = logic = errors = "-"
        if run and build_state != "FAILED":
            steam_exe = bool(exe_of(files)) and exe_of(files)[1] == build.STEAM_EXE_SHA256
            # installed as it is, another exe plays the game's own intro movies first (over a minute)
            out, error_log, locofix_log = run_test(game, "menu", ["-WaitFor", r"menu\.map"] +
                                                   ([] if steam_exe else ["-LogWait", "300"]))
            menu = "ok" if "WaitFor 'menu\\.map': True" in out else "no menu"
            if menu != "ok" and not steam_exe and not error_log:
                # the edition's own exe ended before the engine started (the Italian CD's copy
                # protection without its disc): a limit of that edition, not of our build
                menu = "did not start (its exe)"
            logic_lines = [l for l in error_log.splitlines() if "LOGIC" in l]
            inactive = "inactive" in locofix_log or not locofix_log
            if steam_exe:
                out2, error_log2, _ = run_test(game, "poligon", ["-Map", r"maps\sandbox\sandbox.map", "-Steps", "27@3"])
                poligon = "ok" if "sandbox.map" in error_log2 and "Closed normally" in out2 else "FAILED"
                logic_lines += [l for l in error_log2.splitlines() if "LOGIC" in l]
            else:
                poligon = "n/a (no LocoFix)" if inactive else "LocoFix active?!"
            logic = "none" if not logic_lines else f"{len(logic_lines)}: {logic_lines[0][:60]}"
            count, kinds = error_kinds(error_log)
            ours = {s: error_log.count(s) for s in AS_IS_ERRORS if s in error_log}
            errors = str(count) + (" (" + ", ".join(f"{s} {n}" for s, n in ours.items()) + ")" if ours else "")
            print(f"  menu: {menu}, POLIGON: {poligon}, LOGIC: {logic}, !!!ERROR lines of the menu's run: {errors}")
            for n, kind in kinds[:5]:
                print(f"    {n:5d}  {kind}")
        rows.append((code, exe_text(files), summary, build_state, menu, poligon, logic, errors))
    print()
    table(rows)
    return 1 if any(r[3] == "FAILED" or r[4] == "no menu" or r[5] == "FAILED" or r[6] not in ("none", "-")
                    or "(" in r[7] for r in rows[1:]) else 0


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--store", default=STORE, help="the local store of the editions' files (default: editions/)")
    sub = p.add_subparsers(dest="command", required=True)
    for name in ("capture", "refresh", "assemble"):
        s = sub.add_parser(name)
        s.add_argument("code")
        s.add_argument("folder")
    sub.add_parser("list")
    s = sub.add_parser("diff")
    s.add_argument("a")
    s.add_argument("b")
    s = sub.add_parser("check")
    s.add_argument("codes", nargs="*")
    s.add_argument("--run", action="store_true", help="start each built game (test_game.ps1)")
    s.add_argument("--captured", help="the language captures for build.py (default: build.py's)")
    a = p.parse_args()
    if a.command in ("capture", "refresh"):
        capture(a.code, a.folder, a.store, report_changes=a.command == "refresh")
    elif a.command == "list":
        list_editions()
    elif a.command == "diff":
        diff(a.a, a.b)
    elif a.command == "assemble":
        print(assemble(a.code, a.folder, a.store))
    else:
        return check(a.codes, a.run, a.captured, a.store)
    return 0


if __name__ == "__main__":
    sys.exit(main())
