r"""Decompiles Steam.exe with Ghidra and writes readable C code to re/c/.

    python tools\decompile.py [--ghidra <Ghidra folder>] [--steam <game folder>] [--reuse]

The Ghidra project is created from scratch in build/ghidra (outside the repo). Names and
descriptions from re/symbols.csv, then struct types, vtables and function prototypes from
re/types.txt are applied before the export, so to rename or retype something in the code,
edit those files and run this script again.
--reuse skips the import and analysis and applies the files to the project left by the
previous run (faster while working on types; a name or type removed from the files stays
in the project until a full run).
Needs Java 21 (JAVA_HOME or java on PATH). A full run takes a few minutes.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def find_ghidra():
    tools = os.path.join(os.path.dirname(REPO), "_tools")   # next to the repository
    for p in sorted(glob.glob(os.path.join(tools, "ghidra_*_PUBLIC")), reverse=True):
        return p
    return None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ghidra", default=find_ghidra())
    ap.add_argument("--steam", default=r"C:\Program Files (x86)\Steam\steamapps\common\Locoland")
    ap.add_argument("--reuse", action="store_true", help="apply to the already analysed project")
    a = ap.parse_args()
    if not a.ghidra:
        sys.exit("Ghidra not found - pass --ghidra")

    project = os.path.join(REPO, "build", "ghidra")
    exe = os.path.join(project, "Steam.exe")
    if a.reuse and os.path.exists(os.path.join(project, "Locoland.gpr")):
        source = ["-process", "Steam.exe", "-noanalysis"]
    else:
        shutil.rmtree(project, ignore_errors=True)
        os.makedirs(project)
        # analyzeHeadless.bat cannot handle parentheses in paths ("Program Files (x86)")
        shutil.copy2(os.path.join(a.steam, "Steam.exe"), exe)
        source = ["-import", exe]

    env = dict(os.environ)
    if "JAVA_HOME" not in env:
        java = shutil.which("java")
        if java:
            env["JAVA_HOME"] = os.path.dirname(os.path.dirname(os.path.realpath(java)))

    cmd = [
        os.path.join(a.ghidra, "support", "analyzeHeadless.bat"),
        project, "Locoland",
        *source,
        "-scriptPath", os.path.join(REPO, "tools", "ghidra"),
        "-postScript", "ApplySymbols.java", os.path.join(REPO, "re", "symbols.csv"),
        "-postScript", "ApplyTypes.java", os.path.join(REPO, "re", "types.txt"),
        "-postScript", "ExportC.java", os.path.join(REPO, "re", "c"),
        "-log", os.path.join(project, "analysis.log"),
    ]
    result = subprocess.run(cmd, env=env)
    sys.exit(result.returncode)


if __name__ == "__main__":
    main()
