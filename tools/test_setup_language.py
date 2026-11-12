r"""The game language Setup chooses, without running Setup: the language list and the first choice
(installer.game_languages, installer.first_game_language) for each copy of the game at hand, each
system language, a new install and an update, with no captures or with fake ones.

    python tools\test_setup_language.py [--editions <folder>]

The copies are the editions' files (tools\editions.py's store, build\editions\<code>\steam of the
main checkout by default; read only), each in two layouts: "as is" - no Steam library around it, so
no appmanifest, as the test rig maps a copy or a player copies the game elsewhere - and "library", in
a Steam library whose appmanifest names the depot's language (the Steam editions only; the manifest
is stood in for). The system language and the saved language are set in the test (nothing of Windows
or the registry is read; this machine's Steam and its downloaded depots are left out); the captures
are an empty temporary folder, or one with fake English and Polish captures. Prints a table and
checks the rule (docs/dev/release.md "What Setup does"): the copy's own language can always be
built; the choice can be built and is empty only when nothing can be; on an update a saved language
that can be built; else the system's language when it can be built, else English, else the copy's
own, else the first that is not a fan translation.
"""
import argparse
import os
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import installer  # noqa: E402
import languages  # noqa: E402

DEFAULT_EDITIONS = os.path.join(os.path.dirname(HERE), "build", "editions")
if not os.path.isdir(DEFAULT_EDITIONS):   # a worktree: the main checkout's store
    DEFAULT_EDITIONS = os.path.normpath(os.path.join(HERE, "..", "..", "..", "..", "build", "editions"))
# what each copy's own language is (by what it is, not by the code under test)
OWN = {"en-steam": "en", "pt-steam": "en", "en-beta-demo": "en", "pl-steam": "pl", "pl-cd": "pl", "de-steam": "de",
       "hu-steam": "hu", "ru-steam": "ru", "cs-cd": "cs", "it-cd": None}
# the language Steam's appmanifest names for each depot (Portuguese: "portuguese", though its files are English)
MANIFEST = {"en-steam": "en", "pt-steam": "pt", "pl-steam": "pl", "de-steam": "de", "hu-steam": "hu", "ru-steam": "ru"}


def layout(manifest):
    """The appmanifest beside the copy names `manifest` (a code), or there is none."""
    if hasattr(languages, "manifest_language"):   # the files first, the manifest second
        languages.manifest_language = lambda folder: manifest
    else:   # v0.2.11: the manifest alone
        languages.steam_language = lambda folder: manifest


def choose(steam, system, saved=None, update=False):
    installer.system_language = lambda: system
    installer.saved_game_language = lambda: saved
    buildable, missing, have = installer.game_languages(steam)
    own = languages.steam_language(steam) if installer.is_game(steam) else None
    try:
        return buildable, installer.first_game_language(buildable, update, own)
    except TypeError:   # Setup before the copy's own language was a choice (v0.2.11)
        return buildable, installer.first_game_language(buildable, update)


def want(edition, buildable, system, saved, update):
    if update and saved in buildable:
        return saved
    code = next((c for c in (system, "en", OWN[edition]) if c and c in buildable), None)
    if code is None and buildable:
        code = next((c for c in buildable if not languages.fan_pack(c)), buildable[0])
    return code


def main():
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--editions", default=DEFAULT_EDITIONS)
    a = p.parse_args()
    languages.steam_root = lambda: None   # not this machine's Steam: its downloaded depots are not the copy's
    failures = 0
    with tempfile.TemporaryDirectory() as empty, tempfile.TemporaryDirectory() as fake:
        for code in ("en", "pl"):   # fake captures: a strings.ini is all build_base looks for
            os.makedirs(os.path.join(fake, code))
            open(os.path.join(fake, code, "strings.ini"), "w").close()
        print(f"{'copy':13} {'layout':8} {'captures':9} {'system':7} {'buildable':13} {'new':8} "
              f"{'update, de saved':17} update, none saved")
        for edition in sorted(os.listdir(a.editions)):
            steam = os.path.join(a.editions, edition, "steam")
            if not installer.is_game(steam) or edition not in OWN:
                continue
            own = OWN[edition]
            layouts = [("as is", None)] + ([("library", MANIFEST[edition])] if edition in MANIFEST else [])
            for where, manifest in layouts:
                layout(manifest)
                for captures, folder in (("none", empty), ("en,pl", fake)):
                    languages.CAPTURED = installer.CAPTURED = folder
                    for system in ("pl", "en", "de", "cs", "pt"):
                        row, wrong = [], []
                        for saved, update in ((None, False), ("de", True), (None, True)):
                            buildable, chosen = choose(steam, system, saved, update)
                            expected = want(edition, buildable, system, saved, update)
                            ok = chosen == expected and (own not in languages.packs() or own in buildable)
                            row.append(chosen or "(files)")
                            if not ok:
                                wrong.append(f"want {expected or '(files)'}"
                                             + ("" if own not in languages.packs() or own in buildable
                                                else f", {own} not buildable"))
                        failures += len(wrong)
                        print(f"{edition:13} {where:8} {captures:9} {system:7} {','.join(buildable) or '-':13} "
                              f"{row[0]:8} {row[1]:17} {row[2]}{'   <-- WRONG: ' + '; '.join(wrong) if wrong else ''}")
    print(f"\n{failures} wrong")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
