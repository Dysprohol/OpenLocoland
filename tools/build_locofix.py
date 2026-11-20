r"""Compiles src\LocoFix into build\LocoFix\LocoFix.dll with MSVC (32-bit).

    python tools\build_locofix.py

Finds Visual Studio / Build Tools with the C++ tools through vswhere. The C runtime is
linked statically (/MT), so the DLL does not need the redistributable.
Returns the DLL path; compiles only when the sources are newer than the result.
tools/release.py builds the release's DLL into its own folder with the version
(`build(out_dir, version)`: LOCOFIX_VERSION, shown in LocoFix.log and the launcher).

The DLL carries a Windows version resource (version_rc, compiled with rc.exe): product and company
"OpenLocoland" - the project, never a person -, the version and its numbers as Setup's
(release.version_resource); a build without a version (build.py's) is "dev", 0.0.0.0.
LOCOFIX_MSVC=<14.29 ...> picks that MSVC toolset (the installation that has it; vcvars32's
-vcvars_ver) and LOCOFIX_WINSDK=<10.0.19041.0 ...> that Windows SDK: the release workflow
(.github/workflows/release.yml) builds with the toolset and the SDK of the local releases, so the
two DLLs can be compared (tools/compare_builds.py).

A build with a version (a release's) gives the same DLL on every machine. MSVC names each file's
anonymous namespace ?A0x<hash>, the hash a CRC-32 of the computer's name followed by the file's full
path in lower case, and the names order the code: two copies of the repository in two folders, or
one folder on two computers (a local release and the release workflow's runner, whose name changes
with every run), gave two different DLLs. So a release build compiles a copy of the sources at one
fixed place, FIXED_SOURCES (removed afterwards), and runs the compiler with the computer name
BUILD_HOST: Windows' GetComputerName answers with the variable _CLUSTER_NETWORK_NAME_ when it is set
(meant for programs on a cluster), in the compiler as in any program it starts.
"""
import contextlib
import glob
import os
import re
import shutil
import subprocess
import sys
import time

REPO = getattr(sys, "_MEIPASS", None) or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCES = os.path.join(REPO, "src", "LocoFix")
OUT_DIR = os.path.join(REPO, "build", "LocoFix")
DLL = os.path.join(OUT_DIR, "LocoFix.dll")
# system DLLs only the launcher's update check uses (update_check.cpp): loaded at its first call
DELAY_LOAD = " ".join(f"/DELAYLOAD:{d}" for d in ("winhttp.dll", "bcrypt.dll", "shell32.dll", "ole32.dll"))
DEV_VERSION = "dev"   # patch.h's LOCOFIX_VERSION when none is given
# where a release build compiles (the same path locally and in the release workflow; see above)
FIXED_SOURCES = r"C:\ProgramData\OpenLocoland\LocoFix-src"
# the computer name a release build's compiler sees (_CLUSTER_NETWORK_NAME_; see above)
BUILD_HOST = "OPENLOCOLAND"
LOCK_STALE = 1800   # seconds
# the version resources' copyright line (LocoFix.dll and Setup.exe; installer.CREDIT in Setup's window)
COPYRIGHT = "Copyright (C) 2026 Dysprohol and OpenLocoland contributors. Locoland is a trademark of its owners."


def version_numbers(ver):
    """The four numbers of a version resource: v0.2.11 -> (0, 2, 11, 0), v0.2.11-3-gabc1234 -> (0, 2, 11, 3)
    (the commits after the tag); a version without a tag (a short hash, "dev") -> (0, 0, 0, 0)."""
    m = re.match(r"v?(\d+)\.(\d+)\.(\d+)(?:-(\d+)-g)?", ver)
    return tuple(int(n or 0) for n in m.groups()) if m else (0, 0, 0, 0)


def version_rc(ver):
    """The resource script of LocoFix.dll's version resource for the version `ver`."""
    numbers = ",".join(str(n) for n in version_numbers(ver))
    strings = [("CompanyName", "OpenLocoland"),
               ("FileDescription", "OpenLocoland fixes and map editor for Locoland"),
               ("FileVersion", ver),
               ("InternalName", "LocoFix"),
               ("LegalCopyright", COPYRIGHT),
               ("OriginalFilename", "LocoFix.dll"),
               ("ProductName", "OpenLocoland"),
               ("ProductVersion", ver)]
    values = "\n".join(f'            VALUE "{k}", "{v.replace(chr(34), chr(34) * 2)}"' for k, v in strings)
    return f"""#include <winver.h>
#pragma code_page(65001)
VS_VERSION_INFO VERSIONINFO
 FILEVERSION {numbers}
 PRODUCTVERSION {numbers}
 FILEFLAGSMASK VS_FFI_FILEFLAGSMASK
 FILEFLAGS 0x0L
 FILEOS VOS_NT_WINDOWS32
 FILETYPE VFT_DLL
 FILESUBTYPE VFT2_UNKNOWN
BEGIN
    BLOCK "StringFileInfo"
    BEGIN
        BLOCK "040904B0"
        BEGIN
{values}
        END
    END
    BLOCK "VarFileInfo"
    BEGIN
        VALUE "Translation", 0x409, 1200
    END
END
"""


def msvc_pins():
    """LOCOFIX_MSVC (an MSVC toolset, 14.29) and LOCOFIX_WINSDK (a Windows SDK, 10.0.19041.0): the
    versions vcvars32 sets up instead of the installation's newest; empty when not given."""
    pins = []
    for name in ("LOCOFIX_MSVC", "LOCOFIX_WINSDK"):
        value = os.environ.get(name, "").strip()
        if value and not re.fullmatch(r"\d+(\.\d+)*", value):
            sys.exit(f"{name}={value!r}: a version such as 14.29 or 10.0.19041.0")
        pins.append(value)
    return pins


def vcvars32():
    vswhere = os.path.join(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)"),
                           "Microsoft Visual Studio", "Installer", "vswhere.exe")
    if not os.path.isfile(vswhere):
        return None
    paths = subprocess.run(
        [vswhere, "-all", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property", "installationPath"],
        capture_output=True, text=True).stdout.splitlines()
    toolset = msvc_pins()[0]
    for p in paths:
        bat = os.path.join(p, "VC", "Auxiliary", "Build", "vcvars32.bat")
        # with LOCOFIX_MSVC, the installation that has that toolset
        if toolset and not glob.glob(os.path.join(p, "VC", "Tools", "MSVC", toolset + "*")):
            continue
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
    out_dir = os.path.abspath(out_dir)
    dll = os.path.join(out_dir, "LocoFix.dll")
    files = glob.glob(os.path.join(SOURCES, "*.cpp")) + glob.glob(os.path.join(SOURCES, "*.h")) + [__file__]
    if version is None and os.path.isfile(dll) and \
            os.path.getmtime(dll) >= max(os.path.getmtime(p) for p in files):
        return dll
    bat = vcvars32()
    if not bat:
        sys.exit("No MSVC compiler with the C++ tools (Visual Studio / Build Tools).")
    os.makedirs(out_dir, exist_ok=True)
    remove_stale(out_dir)
    if version is None:
        return compile_dll(bat, SOURCES, out_dir, None)
    with fixed_sources() as sources:
        return compile_dll(bat, sources, out_dir, version)


@contextlib.contextmanager
def fixed_sources():
    """A copy of src\\LocoFix at FIXED_SOURCES for a release build, one build at a time (a lock file
    beside it; one older than LOCK_STALE seconds is a stopped build's), removed afterwards."""
    base = os.path.dirname(FIXED_SOURCES)
    os.makedirs(base, exist_ok=True)
    lock = FIXED_SOURCES + ".lock"
    waited = 0
    while True:
        try:
            os.close(os.open(lock, os.O_CREAT | os.O_EXCL | os.O_WRONLY))
            break
        except FileExistsError:
            try:
                if time.time() - os.path.getmtime(lock) > LOCK_STALE:
                    os.remove(lock)
                    continue
            except OSError:
                continue
            if waited == 0:
                print(f"waiting for another release build of LocoFix ({lock})")
            if waited > LOCK_STALE:
                sys.exit(f"{lock}: another build holds it")
            time.sleep(2)
            waited += 2
    try:
        if os.path.isdir(FIXED_SOURCES):
            shutil.rmtree(FIXED_SOURCES)
        shutil.copytree(SOURCES, FIXED_SOURCES)
        yield FIXED_SOURCES
    finally:
        shutil.rmtree(FIXED_SOURCES, ignore_errors=True)
        os.remove(lock)
        try:
            os.rmdir(base)   # only when empty
        except OSError:
            pass


def compile_dll(bat, sources, out_dir, version):
    """Compiles the .cpp files of `sources` into `out_dir`\\LocoFix.dll, with the version resource."""
    dll = os.path.join(out_dir, "LocoFix.dll")
    script = os.path.join(out_dir, "build.bat")
    # the sources by their names, compiled in their folder (the objects go to out_dir)
    cpp = " ".join(f'"{os.path.basename(p)}"' for p in sorted(glob.glob(os.path.join(sources, "*.cpp"))))
    define = f'/DLOCOFIX_VERSION=\\"{version}\\" ' if version else ""
    with open(os.path.join(out_dir, "version.rc"), "w", encoding="utf-8") as f:
        f.write(version_rc(version or DEV_VERSION))
    toolset, sdk = msvc_pins()
    pins = (f" {sdk}" if sdk else "") + (f" -vcvars_ver={toolset}" if toolset else "")
    with open(script, "w", encoding="ascii") as f:
        f.write("@echo off\r\n")
        f.write(f'call "{bat}"{pins} >nul || exit /b 1\r\n')
        if version:
            f.write(f"set _CLUSTER_NETWORK_NAME_={BUILD_HOST}\r\n")
        f.write(f'cd /d "{sources}"\r\n')
        f.write(f'rc /nologo /fo "{out_dir}\\version.res" "{out_dir}\\version.rc" || exit /b 1\r\n')
        f.write(f'cl /nologo /O2 /MT /LD /EHsc /utf-8 /W4 /std:c++17 {define}{cpp} "{out_dir}\\version.res" '
                f'/Fo"{out_dir}\\\\" /Fe"{dll}" /link user32.lib advapi32.lib winhttp.lib bcrypt.lib '
                f"shell32.lib ole32.lib delayimp.lib {DELAY_LOAD}\r\n")
    result = subprocess.run(["cmd", "/c", script], capture_output=True, text=True, errors="replace")
    if result.returncode != 0 or not os.path.isfile(dll):
        sys.exit("LocoFix build failed:\n" + result.stdout + result.stderr)
    return dll


if __name__ == "__main__":
    print(build())
