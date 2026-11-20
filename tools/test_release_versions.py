r"""Test of the launcher's version rule (src/LocoFix/update_version.h, used by update_check.cpp).

    python tools\test_release_versions.py

Builds tools\test_release_versions.cpp (which includes update_version.h and holds the table of cases)
with MSVC into build\tests and runs it: what is a version (v0.2.13-beta.1, v0.2.12-3-gabc1234,
not "dev" or a commit hash), which of two is newer (v0.2.12 < v0.2.13-beta.1 < v0.2.13; a build
after a tag counts as the tag), the launcher's "newer" or "current" for a build and the release
GitHub gives, and which release of GitHub's list the box "Include pre-releases" offers (the highest
version, drafts left out). Exits with 1 when a case fails. (Not named test_update_...: Windows asks
for administrator rights to start a program without a manifest whose name says "update".)
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
OUT_DIR = os.path.join(REPO, "build", "tests")
TEST_EXE = os.path.join(OUT_DIR, "test_release_versions.exe")
sys.path.insert(0, HERE)
from build_locofix import vcvars32  # noqa: E402


def build_test_host():
    os.makedirs(OUT_DIR, exist_ok=True)
    source = os.path.join(HERE, "test_release_versions.cpp")
    header = os.path.join(REPO, "src", "LocoFix", "update_version.h")
    if os.path.isfile(TEST_EXE) and os.path.getmtime(TEST_EXE) >= max(os.path.getmtime(source),
                                                                        os.path.getmtime(header)):
        return
    bat = vcvars32()
    if not bat:
        sys.exit("No MSVC compiler with the C++ tools.")
    script = os.path.join(OUT_DIR, "test_release_versions.bat")
    with open(script, "w", encoding="ascii") as f:
        f.write("@echo off\r\n")
        f.write(f'call "{bat}" >nul || exit /b 1\r\n')
        f.write(f'cd /d "{OUT_DIR}"\r\n')
        f.write(f'cl /nologo /O2 /MT /EHsc /utf-8 /W4 /std:c++17 "{source}" /Fe:test_release_versions.exe\r\n')
    result = subprocess.run(["cmd", "/c", script], capture_output=True, text=True, errors="replace")
    if result.returncode != 0:
        sys.exit("test host build failed:\n" + result.stdout + result.stderr)


def main():
    build_test_host()
    result = subprocess.run([TEST_EXE], capture_output=True, text=True)
    print(result.stdout, end="")
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
