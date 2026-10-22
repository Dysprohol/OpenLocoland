r"""Builds the release: build\release\Setup.exe, the installer of OpenLocoland.

    .venv\Scripts\pip install -r tools\requirements-release.txt
    .venv\Scripts\python tools\release.py

1. the version: `git describe --tags --always --dirty` (a commit without a tag gives its
   short hash),
2. LocoFix.dll compiled with that version (build\release\LocoFix; build_locofix.py, needs
   Visual Studio) - the player gets it prebuilt,
3. the payload in build\release\payload: our files only, in the repository's layout - game\
   (patches, our files, the new maps, the editor's tables), lang\, external\dgVoodoo2\ (the
   two DLLs and the notice), LocoFix.dll and version.txt. Nothing of the game: the installer
   assembles it from the player's Steam copy (build.py); the payload is checked against the
   game's file names and extensions before it is packed,
4. PyInstaller freezes tools\installer.py (with build.py and the modules it imports, Pillow
   included) and the payload into one Setup.exe (--onefile --windowed), which needs neither
   Python nor Visual Studio on the player's machine.
"""
import os
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "tools"))
import build_locofix  # noqa: E402

OUT = os.path.join(REPO, "build", "release")
PAYLOAD = os.path.join(OUT, "payload")
DGVOODOO = ["DDraw.dll", "D3DImm.dll", "NOTICE.md"]
# extensions of game files that never belong in the payload (our own files of these kinds are
# .tsv/.json/.txt/.lgc tables, patches and texts; pictures are made at install time)
GAME_KINDS = {".exe", ".res", ".vid", ".map", ".bmp", ".wav", ".ogg", ".avi", ".mpg", ".flc", ".tga", ".cur"}


def git(*args):
    return subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True, check=True).stdout.strip()


def version():
    return git("describe", "--tags", "--always", "--dirty")


def collect(dll, ver):
    if os.path.isdir(PAYLOAD):
        shutil.rmtree(PAYLOAD)
    for folder in ("game", "lang"):
        shutil.copytree(os.path.join(REPO, folder), os.path.join(PAYLOAD, folder))
    os.makedirs(os.path.join(PAYLOAD, "external", "dgVoodoo2"))
    for f in DGVOODOO:
        shutil.copy2(os.path.join(REPO, "external", "dgVoodoo2", f), os.path.join(PAYLOAD, "external", "dgVoodoo2", f))
    shutil.copy2(dll, os.path.join(PAYLOAD, "LocoFix.dll"))
    open(os.path.join(PAYLOAD, "version.txt"), "w", encoding="utf-8").write(ver + "\n")

    # only files git knows (no local test output) and no game file
    tracked = {os.path.normcase(os.path.normpath(p)) for p in git("ls-files", "game", "lang").splitlines()}
    for directory, _, files in os.walk(PAYLOAD):
        for f in files:
            rel = os.path.relpath(os.path.join(directory, f), PAYLOAD)
            top = rel.split(os.sep)[0].lower()
            if top in ("game", "lang") and os.path.normcase(rel) not in tracked:
                os.remove(os.path.join(directory, f))
                print(f"left out (not in git): {rel}")
            elif os.path.splitext(f)[1].lower() in GAME_KINDS:
                sys.exit(f"{rel}: looks like a game file - not packed")


def main():
    ver = version()
    print(f"OpenLocoland {ver}")
    dll = build_locofix.build(os.path.join(OUT, "LocoFix"), ver)
    print(f"LocoFix.dll: {dll}")
    collect(dll, ver)
    tools = os.path.join(REPO, "tools")
    work = os.path.join(OUT, "pyinstaller")
    icon = os.path.join(tools, "icon", "openlocoland.ico")   # tools/make_icon.py
    subprocess.run([sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean", "--onefile", "--windowed",
                    "--name", "Setup", "--distpath", OUT, "--workpath", work, "--specpath", work,
                    "--paths", tools, "--add-data", f"{PAYLOAD}{os.pathsep}.",
                    "--icon", icon, "--add-data", f"{icon}{os.pathsep}.",
                    os.path.join(tools, "installer.py")], check=True)
    setup = os.path.join(OUT, "Setup.exe")
    print(f"{setup}: {os.path.getsize(setup) / 1e6:.1f} MB, version {ver}")


if __name__ == "__main__":
    main()
