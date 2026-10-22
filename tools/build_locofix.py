r"""Compiles src\LocoFix into build\LocoFix\LocoFix.dll with MSVC (32-bit).

    python tools\build_locofix.py

Finds Visual Studio / Build Tools with the C++ tools through vswhere. The C runtime is
linked statically (/MT), so the DLL does not need the redistributable.
Returns the DLL path; compiles only when the sources are newer than the result.
tools/release.py builds the release's DLL into its own folder with the version
(`build(out_dir, version)`: LOCOFIX_VERSION, shown in LocoFix.log and the launcher).
"""
import glob
import os
import subprocess
import sys

REPO = getattr(sys, "_MEIPASS", None) or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = os.path.join(REPO, "src", "LocoFix")
OUT_DIR = os.path.join(REPO, "build", "LocoFix")
DLL = os.path.join(OUT_DIR, "LocoFix.dll")


def vcvars32():
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                           "Microsoft Visual Studio", "Installer", "vswhere.exe")
    if not os.path.isfile(vswhere):
        return None
    paths = subprocess.run(
        [vswhere, "-all", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
        capture_output=True, text=True).stdout.splitlines()
    for p in paths:
        bat = os.path.join(p, "VC", "Auxiliary", "Build", "vcvars32.bat")
        if os.path.isfile(bat):
            return bat
    return None


def remove_stale(out_dir):
    """Deletes objects of sources that no longer exist and scripts other than build.bat."""
    names = {os.path.splitext(os.path.basename(p))[0].lower()
             for p in glob.glob(os.path.join(SOURCES, "*.cpp"))}
    for p in glob.glob(os.path.join(out_dir, "*.obj")):
        if os.path.splitext(os.path.basename(p))[0].lower() not in names:
            os.remove(p)
    for p in glob.glob(os.path.join(out_dir, "*.bat")):
        if os.path.basename(p).lower() != "build.bat":
            os.remove(p)


def build(out_dir=OUT_DIR, version=None):
    """Compiles into `out_dir`; with a `version` always (the define changes nothing a date shows)."""
    dll = os.path.join(out_dir, "LocoFix.dll")
    files = glob.glob(os.path.join(SOURCES, "*.cpp")) + glob.glob(os.path.join(SOURCES, "*.h"))
    if version is None and os.path.isfile(dll) and \
            os.path.getmtime(dll) >= max(os.path.getmtime(p) for p in files):
        return dll
    bat = vcvars32()
    if not bat:
        sys.exit("No MSVC compiler with the C++ tools (Visual Studio / Build Tools).")
    os.makedirs(out_dir, exist_ok=True)
    remove_stale(out_dir)
    script = os.path.join(out_dir, "build.bat")
    cpp = " ".join(f'"{p}"' for p in glob.glob(os.path.join(SOURCES, "*.cpp")))
    define = f'/DLOCOFIX_VERSION=\\"{version}\\" ' if version else ""
    with open(script, "w", encoding="ascii") as f:
        f.write("@echo off\r\n")
        f.write(f'call "{bat}" >nul || exit /b 1\r\n')
        f.write(f'cd /d "{out_dir}"\r\n')
        f.write(f"cl /nologo /O2 /MT /LD /EHsc /utf-8 /W4 /std:c++17 {define}{cpp} "
                f"/Fe:LocoFix.dll /link user32.lib advapi32.lib\r\n")
    result = subprocess.run(["cmd", "/c", script], capture_output=True, text=True, errors="replace")
    if result.returncode != 0 or not os.path.isfile(dll):
        sys.exit("LocoFix build failed:\n" + result.stdout + result.stderr)
    return dll


if __name__ == "__main__":
    print(build())
