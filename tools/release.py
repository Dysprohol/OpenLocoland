r"""Builds the release: build\release\Setup.exe, the installer of OpenLocoland.

    .venv\Scripts\pip install -r tools\requirements-release.txt
    .venv\Scripts\python tools\release.py [--zip]

1. the version: `git describe --tags --always --dirty` (a commit without a tag gives its
   short hash),
2. LocoFix.dll compiled with that version (build\release\LocoFix; build_locofix.py, needs
   Visual Studio) - the player gets it prebuilt,
3. the payload in build\release\payload: our files only, in the repository's layout - game\
   (patches, our files, the new maps, the editor's tables), lang\, external\dgVoodoo2\ (the
   two DLLs and the notice), LocoFix.dll, version.txt and payload.txt (the list of its files,
   which Setup checks before it builds). Nothing of the game: the installer
   assembles it from the player's Steam copy (build.py); the payload is checked against the
   game's file names and extensions before it is packed,
4. PyInstaller freezes tools\installer.py (with build.py and the modules it imports, Pillow
   included) and the payload into one Setup.exe (--onefile --windowed), which needs neither
   Python nor Visual Studio on the player's machine. The exe gets a Windows version resource
   (version_resource: the version's numbers, "OpenLocoland Setup", the company "OpenLocoland" -
   the project, never a person) and its own manifest (MANIFEST: asInvoker, system DPI aware, long
   paths, the common controls), and no UPX (--noupx: a packed exe looks like what packers hide).
5. with --zip (off by default) also the same Setup as one folder (PyInstaller --onedir: Setup.exe and
   its _internal\ folder, nothing unpacked into %TEMP% when it runs) packed into
   build\release\Setup.zip - Setup.exe at the zip's root, as the launcher's update check
   (update_check.cpp, PREFER_SETUP_ZIP) expects it. Not a release asset unless the maintainer makes
   it one; the one-file Setup.exe stays the asset every installed launcher finds.
"""
import argparse
import os
import re
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(REPO, "tools"))
import build_locofix  # noqa: E402
from safe_path import safe_relpath  # noqa: E402

OUT = os.path.join(REPO, "build", "release")
PAYLOAD = os.path.join(OUT, "payload")
DGVOODOO = ["DDraw.dll", "D3DImm.dll", "NOTICE.md"]
PAYLOAD_LIST = "payload.txt"   # installer.PAYLOAD_LIST
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
    for folder in ("game", "lang", "tools"):
        shutil.copytree(os.path.join(REPO, folder), os.path.join(PAYLOAD, folder))
    os.makedirs(os.path.join(PAYLOAD, "external", "dgVoodoo2"))
    for f in DGVOODOO:
        shutil.copy2(os.path.join(REPO, "external", "dgVoodoo2", f), os.path.join(PAYLOAD, "external", "dgVoodoo2", f))
    shutil.copy2(dll, os.path.join(PAYLOAD, "LocoFix.dll"))
    open(os.path.join(PAYLOAD, "version.txt"), "w", encoding="utf-8").write(ver + "\n")

    # only files git knows (no local test output) and no game file
    tracked = {os.path.normcase(os.path.normpath(p)) for p in git("ls-files", "game", "lang", "tools").splitlines()}
    for directory, _, files in os.walk(PAYLOAD):
        for f in files:
            rel = safe_relpath(os.path.join(directory, f), PAYLOAD)
            top = rel.split(os.sep)[0].lower()
            if top in ("game", "lang", "tools") and os.path.normcase(rel) not in tracked:
                os.remove(os.path.join(directory, f))
                print(f"left out (not in git): {rel}")
            elif os.path.splitext(f)[1].lower() in GAME_KINDS:
                sys.exit(f"{rel}: looks like a game file - not packed")
    # the list of the payload's files, which Setup checks before it builds (installer.missing_own_files):
    # a file an antivirus took from what the bootloader unpacked is then named at once
    files = sorted(safe_relpath(os.path.join(d, f), PAYLOAD) for d, _, fs in os.walk(PAYLOAD) for f in fs)
    with open(os.path.join(PAYLOAD, PAYLOAD_LIST), "w", encoding="utf-8") as f:
        f.write("\n".join(files) + "\n")


# The manifest of Setup.exe: PyInstaller's own (asInvoker, the Windows versions it is made for, long
# paths, the common controls 6) and DPI awareness for the whole system's scale, which Tk follows (its
# fonts are in points; installer.py scales the few sizes given in pixels): without it Windows
# stretches the window as a picture on a screen with a scale above 100 %, blurred.
MANIFEST = """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="asInvoker" uiAccess="false"/>
      </requestedPrivileges>
    </security>
  </trustInfo>
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{e2011457-1546-43c5-a5fe-008deee3d3f0}"/>
      <supportedOS Id="{35138b9a-5d96-4fbd-8e2d-a2440225f93a}"/>
      <supportedOS Id="{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}"/>
      <supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}"/>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
    </application>
  </compatibility>
  <application xmlns="urn:schemas-microsoft-com:asm.v3">
    <windowsSettings>
      <dpiAware xmlns="http://schemas.microsoft.com/SMI/2005/WindowsSettings">true</dpiAware>
      <dpiAwareness xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">system</dpiAwareness>
      <longPathAware xmlns="http://schemas.microsoft.com/SMI/2016/WindowsSettings">true</longPathAware>
    </windowsSettings>
  </application>
  <dependency>
    <dependentAssembly>
      <assemblyIdentity type="win32" name="Microsoft.Windows.Common-Controls" version="6.0.0.0" processorArchitecture="*" publicKeyToken="6595b64144ccf1df" language="*"/>
    </dependentAssembly>
  </dependency>
</assembly>
"""


def version_numbers(ver):
    """The four numbers of a version resource: v0.2.11 -> (0, 2, 11, 0), v0.2.11-3-gabc1234 -> (0, 2, 11, 3)
    (the commits after the tag); a version without a tag (a short hash) -> (0, 0, 0, 0)."""
    m = re.match(r"v?(\d+)\.(\d+)\.(\d+)(?:-(\d+)-g)?", ver)
    return tuple(int(n or 0) for n in m.groups()) if m else (0, 0, 0, 0)


def version_resource(ver, path):
    """Writes PyInstaller's description of the version resource (--version-file) to `path`."""
    numbers = version_numbers(ver)
    strings = [("CompanyName", "OpenLocoland"),
               ("FileDescription", f"OpenLocoland {ver} Setup"),
               ("FileVersion", ver),
               ("InternalName", "Setup"),
               ("LegalCopyright", "Copyright (C) 2026 Dysprohol and OpenLocoland contributors. "
                                 "Locoland is a trademark of its owners."),   # installer.CREDIT in the window
               ("OriginalFilename", f"OpenLocoland-{ver}-Setup.exe"),
               ("ProductName", "OpenLocoland Setup"),
               ("ProductVersion", ver),
               ("Comments", "Builds OpenLocoland from the player's own Steam copy of Locoland")]
    with open(path, "w", encoding="utf-8") as f:
        f.write(f"""VSVersionInfo(
  ffi=FixedFileInfo(filevers={numbers}, prodvers={numbers}, mask=0x3f, flags=0x0, OS=0x40004,
                    fileType=0x1, subtype=0x0, date=(0, 0)),
  kids=[
    StringFileInfo([StringTable('040904B0', [{", ".join(f"StringStruct({k!r}, {v!r})" for k, v in strings)}])]),
    VarFileInfo([VarStruct('Translation', [1033, 1200])])
  ]
)
""")


def pack_zip(folder, zip_path):
    """The one-folder Setup `folder` (Setup.exe, _internal\\) as a deflated zip, Setup.exe at its root."""
    import zipfile
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for directory, _, files in os.walk(folder):
            for f in sorted(files):
                path = os.path.join(directory, f)
                z.write(path, safe_relpath(path, folder).replace(os.sep, "/"))


def main():
    p = argparse.ArgumentParser(description="Builds build\\release\\Setup.exe (and with --zip Setup.zip).")
    p.add_argument("--zip", action="store_true",
                   help="also the one-folder Setup packed as build\\release\\Setup.zip (not a release asset by default)")
    a = p.parse_args()
    ver = version()
    print(f"OpenLocoland {ver}")
    dll = build_locofix.build(os.path.join(OUT, "LocoFix"), ver)
    print(f"LocoFix.dll: {dll}")
    collect(dll, ver)
    tools = os.path.join(REPO, "tools")
    work = os.path.join(OUT, "pyinstaller")
    icon = os.path.join(tools, "icon", "openlocoland.ico")   # tools/make_icon.py
    os.makedirs(work, exist_ok=True)
    version_file, manifest = os.path.join(work, "version.txt"), os.path.join(work, "Setup.manifest")
    version_resource(ver, version_file)
    with open(manifest, "w", encoding="utf-8") as f:
        f.write(MANIFEST)
    def freeze(shape, dist, workpath):
        subprocess.run([sys.executable, "-m", "PyInstaller", "--noconfirm", "--clean", shape, "--windowed",
                        "--noupx", "--version-file", version_file, "--manifest", manifest,
                        "--name", "Setup", "--distpath", dist, "--workpath", workpath, "--specpath", workpath,
                        "--paths", tools, "--add-data", f"{PAYLOAD}{os.pathsep}.",
                        "--icon", icon, "--add-data", f"{icon}{os.pathsep}.",
                        os.path.join(tools, "installer.py")], check=True)
    freeze("--onefile", OUT, work)
    setup = os.path.join(OUT, "Setup.exe")
    print(f"{setup}: {os.path.getsize(setup) / 1e6:.1f} MB, version {ver}")
    if a.zip:
        onedir = os.path.join(OUT, "onedir")
        freeze("--onedir", onedir, os.path.join(OUT, "pyinstaller-onedir"))
        zip_path = os.path.join(OUT, "Setup.zip")
        pack_zip(os.path.join(onedir, "Setup"), zip_path)
        print(f"{zip_path}: {os.path.getsize(zip_path) / 1e6:.1f} MB (one folder; not a release asset by default)")


if __name__ == "__main__":
    main()
