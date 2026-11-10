r"""The installer of OpenLocoland: a window around build.py, frozen into Setup.exe by
tools/release.py (PyInstaller, one file).

    python tools\installer.py      (the same window from the repository, for trying it)
    Setup.exe [--steam <folder>] [--target <folder>]      (the window with these folders; without
        --target it opens on the existing install: find_install)
    Setup.exe --silent [--steam <folder>] [--target <folder>] [--cheats 0|1] [--unlock 0|1]
              [--game-lang <code>] [--no-shortcuts] [--lang <code>] [--no-clean]
        installs without the window: the log goes to logs\install.log in the install folder and
        the exit code is 0 when it worked (scripted installs, tests)
    Setup.exe --auto [--steam <folder>] [--target <folder>] [--no-shortcuts]
        the window, which installs at once with what it shows and closes at the end, its log
        in logs\install.log (tests of the window itself)
    --captured <folder> (with any of them) keeps the language captures there instead (tests:
        a folder with only some languages; Uninstall leaves it)
    --no-clean (with any of them) removes nothing that earlier versions left in the install
        folder (build.clean_up; DEVELOPERS.md "Updating")

It finds the player's Steam copy of Locoland (Steam's registry key, its libraries in
steamapps\libraryfolders.vdf, or a folder chosen by hand), proposes an install folder of its
own (%USERPROFILE%\Games\OpenLocoland - never the Steam folder, which is only read) and runs
build.build() into it with the prebuilt LocoFix.dll packed in Setup.exe: nothing of the game
is in the installer, the game is assembled from the player's files on their machine - the
logo of the window's header is our own OpenLocoland wordmark (the game's, read from the Steam
copy, only without it). The language
captures go to %LOCALAPPDATA%\OpenLocoland\captured; every language Steam has (the installed
one and the depots downloaded next to it) is captured. The game language list shows every pack
of lang\<code>\: those with a capture (a fan translation, base= another language: any capture,
see languages.build_base) can be chosen, the others are greyed with the Steam console line that
gets their language; fan translations are marked unofficial.

The window is in English unless another of its languages is picked; a new install sets the
game's language to English (when that pack can be built) or the one picked. Running it again
over an install updates it: build.py writes only our files and the game's originals, so the
player's high scores and maps stay (the custom maps page finds every map folder), and the progress in the registry
is not touched - the two options are written with the values shown (the current ones of the
registry), and the game's language only when it was changed in the window; what earlier versions
left and this one does not write is removed (build.clean_up, listed in the log). OpenLocoland.json
in the install folder records the version, the shortcuts and the files installed (for the next
update's clean-up); Uninstall removes the shortcuts, the
folder and the captures after a question, and leaves the registry (the progress and the
profiles are the original game's too).
"""
import json
import os
import queue
import shutil
import sys


def _keep_modules_in_memory():
    """Setup.exe (PyInstaller, one file) keeps its Python modules in an archive inside the exe and reads
    each one from the exe the first time it is imported - so a module first needed half way through
    the install failed when an antivirus had taken the exe meanwhile (the OSError of v0.2.10:
    "[Errno 22] Invalid argument: '...Setup.exe'"). The archive's part with the modules is read into
    memory once at the start, and the modules are taken from there: after the start Setup reads
    nothing of its own exe. Returns the bytes kept, or 0 outside a frozen Setup (or when the
    archive is not the kind this knows: then the modules are read from the exe as before)."""
    importers = sys.modules.get("pyimod02_importers")
    archive = getattr(importers, "pyz_archive", None)
    reader = sys.modules.get("pyimod01_archive")
    if archive is None or reader is None or not hasattr(archive, "_filename") or not hasattr(archive, "toc"):
        return 0
    import marshal
    import zlib
    toc = archive.toc
    end = max((offset + length for _, offset, length in toc.values()), default=0)
    with open(archive._filename, "rb") as f:
        f.seek(archive._start_offset)
        data = f.read(end)
    if len(data) != end:
        return 0
    # every constant read here, once: a PyInstaller without one falls back now, not at a later import
    code_kinds = (int(reader.PYZ_ITEM_MODULE), int(reader.PYZ_ITEM_PKG))
    namespace_kind = int(reader.PYZ_ITEM_NSPKG)

    def extract(name, raw=False):   # ZlibArchiveReader.extract, from memory
        entry = toc.get(name)
        if entry is None:
            raise KeyError(f"No entry named {name!r} found in the archive!")
        kind, offset, length = entry
        if kind == namespace_kind:
            return None
        obj = zlib.decompress(data[offset:offset + length])
        if kind in code_kinds and not raw:
            try:
                obj = marshal.loads(obj)
            except EOFError as e:
                raise ImportError(f"Failed to unmarshal PYZ entry {name!r}!") from e
        return obj
    archive.extract = extract
    return len(data)


def keep_modules_in_memory():
    """_keep_modules_in_memory, and when anything of PyInstaller's internals is not as it knows them
    (an attribute, a constant, the archive's layout): 0, the modules read from the exe as before."""
    try:
        return _keep_modules_in_memory()
    except Exception:
        return 0


MODULES_IN_MEMORY = keep_modules_in_memory()

import threading  # noqa: E402
import tkinter as tk  # noqa: E402
import winreg  # noqa: E402
from tkinter import filedialog, messagebox, ttk  # noqa: E402
from tkinter.scrolledtext import ScrolledText  # noqa: E402

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build  # noqa: E402
import languages  # noqa: E402

BUNDLE = getattr(sys, "_MEIPASS", None)
# what the bootloader unpacked, listed at the start, and the payload's list that release.py packs
# (payload.txt): a failure can then name the file of Setup that went missing (shutil.copy2 and others
# raise without the file's name), and the install checks the payload before it builds anything
BUNDLE_FILES = [os.path.join(d, f) for d, _, fs in os.walk(BUNDLE) for f in fs] if BUNDLE else []
PAYLOAD_LIST = "payload.txt"


def missing_own_files():
    """The files of Setup that are gone: its exe, the payload's (payload.txt) and what was unpacked at
    the start - an antivirus may take them while Setup runs."""
    if not BUNDLE:
        return []
    listed = []
    try:
        with open(os.path.join(BUNDLE, PAYLOAD_LIST), encoding="utf-8") as f:
            listed = [os.path.join(BUNDLE, line.strip()) for line in f if line.strip()]
    except OSError:
        listed = [os.path.join(BUNDLE, PAYLOAD_LIST)]
    return [f for f in [sys.executable] + listed + BUNDLE_FILES if not os.path.isfile(f)]
REPO = build.REPO
APP = "OpenLocoland"
MANIFEST = "OpenLocoland.json"
SETTINGS_KEY = r"SOFTWARE\Gromada\Locoland"   # RegPath of Locoland.cfg: LocoFix's settings too
DEFAULT_TARGET = os.path.join(os.environ.get("USERPROFILE", os.path.expanduser("~")), "Games", APP)
DEFAULT_CAPTURED = os.path.join(os.environ.get("LOCALAPPDATA", os.path.expanduser("~")), APP, "captured")
CAPTURED = DEFAULT_CAPTURED   # --captured <folder> for tests
# Setup's own state on this machine (next to the captures): the folder of the last install finished
# through the window, which the window opens on next time (find_install). --state-folder <folder>
# (tests) keeps it elsewhere, so no test install becomes the folder the player's Setup opens on.
STATE_FOLDER = os.path.dirname(DEFAULT_CAPTURED)
STATE_GIVEN = False   # --state-folder given
LAST_INSTALL = "last-install.json"
EDITOR_MAP = r"maps\template\template.map"   # what the menu's "EDYTOR MAP" opens
# the editor shortcut: the game in editor mode on the template, which opens a new empty map
# (LocoFix editor.cpp), without the intro and the menu (test_map.cpp)
EDITOR_ARGUMENTS = "--editor --test-map " + EDITOR_MAP
LOG = os.path.join(build.LOGS, "install.log")   # the log of --silent and --auto in the install folder
DEFAULT_GAME_LANGUAGE = "en"
languages.CAPTURED = CAPTURED   # languages.available() and build() look there


def version():
    path = os.path.join(BUNDLE or REPO, "version.txt")
    try:
        return open(path, encoding="utf-8").read().strip()
    except OSError:
        return "dev"


# The window's texts, one entry per language; the window lists them (English first) and starts in
# the system's language when it has it. Steam's Portuguese depot is the English game, but the
# window has Portuguese texts of its own; Czech is a fan translation without a Steam depot.
TEXTS = {
    "en": {
        "name": "English",
        "title": "OpenLocoland {version} - setup",
        "intro": "OpenLocoland is built from your own Steam copy of Locoland into a folder of its own. "
                 "The Steam game is only read and stays as it is.",
        "language": "Language:",
        "steam": "Locoland in Steam:",
        "target": "Install into:",
        "browse": "Browse...",
        "game_language": "Game language:",
        "more_languages": "Other languages: see the note below.",
        "depots": "The game's other languages come from Steam: open the Steam console (type "
                  "steam://open/console into the browser's address bar or Win+R), enter the line of each "
                  "language you want, wait until Steam has downloaded it and run Setup again:",
        "depots_local": "The game's other languages are not found (no Steam depots in skip-verify mode):",
        "no_language": "No language of the game can be built - is Locoland installed through Steam?",
        "unofficial": "(unofficial)",
        "needs_depot": "needs the Steam depot: {line}",
        "needs_files": "needs the files of {base}: {line}",
        "not_found": "not found in the game folder",
        "options": "Options (they can be changed later in the game's launcher window)",
        "cheats": "Cheats (developer keys: h, W, B, A, K, M, #, x)",
        "unlock": "All missions unlocked (your progress stays as it is)",
        "skip_verify": "Skip verification of the game files - install as it is",
        "skip_verify_hint": "For versions of the game other than the Steam one. The game will install and run, but you "
                            "may not get our fixes, the editor or custom maps.",
        "shortcuts": "Shortcuts",
        "desktop": "On the desktop",
        "start_menu": "In the Start menu",
        "install": "Install",
        "update": "Update",
        "uninstall": "Uninstall",
        "run": "Play",
        "close": "Close",
        "github": "GitHub",
        "no_steam": "Locoland was not found in Steam. Install it through Steam or choose its folder "
                    "(the one with Steam.exe).",
        "bad_steam": "This folder has no Locoland from Steam (Steam.exe is missing):\n{path}",
        "inside_steam": "The install folder must not be the Steam folder of the game or inside it.",
        "modified_steam": "Steam.exe in this folder is not the original file of the game - it was changed or replaced (e.g. by a resolution fix):\n{path}\n\nRestore the original in Steam: right-click Locoland > Properties > Installed Files > Verify integrity of game files. Then run Setup again.",
        "modified_files": "Your Locoland files in Steam differ from those OpenLocoland knows, and our changes do not fit them:\n{files}\n\n{path}\n\nRestore them in Steam: right-click Locoland > Properties > Installed Files > Verify integrity of game files. Then run Setup again. Or tick \"Skip verification of the game files\": these files are left as they are, without our changes. OpenLocoland has its own option for the developer keys: Cheats, below.",
        "unplaced": "Warning: {file} is left as in Steam (our changes do not fit it) - missing: {feature}",
        "feature_intro": "the intro without the movies",
        "feature_custom_maps": "the custom maps page and the map editor in the menu, All missions",
        "feature_custom_play": "the custom maps' objectives, scores and the way back to their page",
        "feature_esc_menu": "the custom maps' Esc menu (briefing, restart, back to the editor)",
        "feature_cheats": "the Cheats option",
        "feature_dev_texts": "the translated texts of the developer mode",
        "feature_other": "our changes of it",
        "as_is": "Steam.exe is not the Steam version of 2003-10-07: installed as it is - no fixes, launcher options, editor or custom maps page",
        "not_empty": "The folder is not empty and has no OpenLocoland install:\n{path}\n\n"
                     "Install there anyway? Its other files are left alone.",
        "running": "OpenLocoland is running from this folder. Close the game first.",
        "working": "Installing...",
        "done": "Done. OpenLocoland {version} is in {path}",
        "failed": "Installation failed: {error}",
        "uninstall_ask": "Remove OpenLocoland from\n{path}?\n\nThe folder is deleted with everything in it "
                         "(also maps made in the editor and the high scores), with the shortcuts and the "
                         "language files kept in {captured}.\nYour profiles, progress and settings in the "
                         "registry stay (the Steam game uses them too).",
        "uninstalled": "OpenLocoland was removed from {path}",
        "game_name": "Locoland",
        "editor_name": "Locoland - map editor",
        "editor_desc": "The map editor of OpenLocoland",
        "damaged": "Setup cannot read its own file:\n{file}\n\nAn antivirus program may have removed or blocked "
                   "Setup.exe or the files it unpacked while it was running. Download Setup again from the "
                   "project's page on GitHub and run it.",
        "kept": "The game in {path} is as it was before this run.",
        "resume": "The update of {path} stopped half way: run Setup again and it completes it.",
        "no_space": "There is not enough free space on {drive} - Setup needs about {need} MB while it works, "
                    "{free} MB are free. Free some space there and run Setup again.",
    },
    "de": {
        "name": "Deutsch",
        "title": "OpenLocoland {version} - Installation",
        "intro": "OpenLocoland wird aus deiner eigenen Steam-Kopie von Locoland in einem eigenen Ordner "
                 "erstellt. Das Spiel in Steam wird nur gelesen und bleibt unverändert.",
        "language": "Sprache:",
        "steam": "Locoland in Steam:",
        "target": "Installieren nach:",
        "browse": "Durchsuchen...",
        "game_language": "Spielsprache:",
        "more_languages": "Weitere Sprachen: siehe den Hinweis unten.",
        "depots": "Die weiteren Sprachen des Spiels kommen aus Steam: öffne die Steam-Konsole "
                  "(steam://open/console in die Adresszeile des Browsers oder in Win+R eingeben), gib die "
                  "Zeile jeder gewünschten Sprache ein, warte, bis Steam sie heruntergeladen hat, und starte "
                  "Setup erneut:",
        "no_language": "Keine Sprache des Spiels kann erstellt werden - ist Locoland über Steam installiert?",
        "depots_local": "Die weiteren Sprachen des Spiels wurden nicht gefunden (ohne Versionsprüfung keine Steam-Depots):",
        "unofficial": "(inoffiziell)",
        "needs_depot": "braucht das Steam-Depot: {line}",
        "needs_files": "braucht die Dateien von {base}: {line}",
        "not_found": "nicht im Spielordner gefunden",
        "options": "Optionen (später im Startfenster des Spiels änderbar)",
        "cheats": "Cheats (Entwicklertasten: h, W, B, A, K, M, #, x)",
        "unlock": "Alle Missionen freigeschaltet (dein Fortschritt bleibt erhalten)",
        "skip_verify": "Prüfung der Spieldateien überspringen - so installieren, wie es ist",
        "skip_verify_hint": "Für andere Versionen des Spiels als die von Steam. Das Spiel lässt sich installieren und "
                            "starten, aber unsere Korrekturen, der Editor oder die eigenen Karten fehlen dann vielleicht.",
        "shortcuts": "Verknüpfungen",
        "desktop": "Auf dem Desktop",
        "start_menu": "Im Startmenü",
        "install": "Installieren",
        "update": "Aktualisieren",
        "uninstall": "Deinstallieren",
        "run": "Spielen",
        "close": "Schließen",
        "github": "GitHub",
        "no_steam": "Locoland wurde in Steam nicht gefunden. Installiere es über Steam oder wähle seinen "
                    "Ordner (den mit Steam.exe).",
        "bad_steam": "In diesem Ordner ist kein Locoland aus Steam (Steam.exe fehlt):\n{path}",
        "inside_steam": "Der Installationsordner darf nicht der Steam-Ordner des Spiels sein oder darin liegen.",
        "modified_steam": "Die Steam.exe in diesem Ordner ist nicht die Originaldatei des Spiels - sie wurde geändert oder ersetzt (z. B. durch einen Auflösungs-Fix):\n{path}\n\nStelle das Original in Steam wieder her: Rechtsklick auf Locoland > Eigenschaften > Installierte Dateien > Dateien auf Fehler überprüfen. Dann Setup erneut starten.",
        "modified_files": "Deine Locoland-Dateien in Steam weichen von denen ab, die OpenLocoland kennt, und unsere Änderungen passen nicht zu ihnen:\n{files}\n\n{path}\n\nStelle sie in Steam wieder her: Rechtsklick auf Locoland > Eigenschaften > Installierte Dateien > Dateien auf Fehler überprüfen. Dann Setup erneut starten. Oder kreuze \"Prüfung der Spieldateien überspringen\" an: diese Dateien bleiben, wie sie sind, ohne unsere Änderungen. OpenLocoland hat für die Entwicklertasten eine eigene Option: Cheats, unten.",
        "unplaced": "Warnung: {file} bleibt wie in Steam (unsere Änderungen passen nicht dazu) - es fehlt: {feature}",
        "feature_intro": "das Intro ohne die Filme",
        "feature_custom_maps": "die Seite der eigenen Karten und der Karteneditor im Menü, Alle Missionen",
        "feature_custom_play": "Ziele, Punkte und der Weg zurück zur Seite der eigenen Karten",
        "feature_esc_menu": "das Esc-Menü der eigenen Karten (Einweisung, Neustart, zurück zum Editor)",
        "feature_cheats": "die Option Cheats",
        "feature_dev_texts": "die übersetzten Texte des Entwicklermodus",
        "feature_other": "unsere Änderungen daran",
        "as_is": "Steam.exe ist nicht die Steam-Version vom 2003-10-07: so installiert, wie es ist - ohne Korrekturen, Launcher-Optionen, Editor und Seite der eigenen Karten",
        "not_empty": "Der Ordner ist nicht leer und enthält keine OpenLocoland-Installation:\n{path}\n\n"
                     "Trotzdem dort installieren? Seine anderen Dateien bleiben unberührt.",
        "running": "OpenLocoland läuft aus diesem Ordner. Beende zuerst das Spiel.",
        "working": "Installation läuft...",
        "done": "Fertig. OpenLocoland {version} ist in {path}",
        "failed": "Die Installation ist fehlgeschlagen: {error}",
        "uninstall_ask": "OpenLocoland aus\n{path}\nentfernen?\n\nDer Ordner wird mit seinem ganzen Inhalt "
                         "gelöscht (auch den im Editor erstellten Karten und den Bestenlisten), dazu die "
                         "Verknüpfungen und die Sprachdateien in {captured}.\nProfile, Fortschritt und "
                         "Einstellungen in der Registry bleiben (das Steam-Spiel nutzt sie auch).",
        "uninstalled": "OpenLocoland wurde aus {path} entfernt",
        "game_name": "Locoland",
        "editor_name": "Locoland - Karteneditor",
        "editor_desc": "Der Karteneditor von OpenLocoland",
        "damaged": "Setup kann eine eigene Datei nicht lesen:\n{file}\n\nVielleicht hat ein Virenschutzprogramm "
                   "Setup.exe oder die von ihm entpackten Dateien während der Ausführung entfernt oder gesperrt. "
                   "Lade Setup erneut von der Seite des Projekts auf GitHub herunter und starte es.",
        "kept": "Das Spiel in {path} ist so wie vor diesem Lauf.",
        "resume": "Die Aktualisierung von {path} ist mittendrin stehen geblieben: starte Setup erneut, dann wird "
                  "sie abgeschlossen.",
        "no_space": "Auf {drive} ist nicht genug Platz frei: Setup braucht während der Arbeit etwa {need} MB, "
                    "frei sind {free} MB. Schaffe dort Platz und starte Setup erneut.",
    },
    "hu": {
        "name": "Magyar",
        "title": "OpenLocoland {version} - telepítő",
        "intro": "Az OpenLocoland a Locoland saját Steam-példányodból készül, külön mappába. A Steames "
                 "játékot csak olvassa, az változatlan marad.",
        "language": "Nyelv:",
        "steam": "Locoland a Steamben:",
        "target": "Telepítés helye:",
        "browse": "Tallózás...",
        "game_language": "A játék nyelve:",
        "more_languages": "További nyelvek: lásd a lenti megjegyzést.",
        "depots": "A játék további nyelvei a Steamből jönnek: nyisd meg a Steam konzolt (írd be a "
                  "steam://open/console címet a böngészőbe vagy a Win+R ablakba), add meg a kívánt nyelvek "
                  "sorát, várd meg, amíg a Steam letölti, és futtasd újra a telepítőt:",
        "no_language": "A játék egyik nyelve sem építhető fel - a Locoland a Steamen keresztül van telepítve?",
        "depots_local": "A játék további nyelvei nem találhatók (verzióellenőrzés nélkül nincsenek Steam depotok):",
        "unofficial": "(nem hivatalos)",
        "needs_depot": "a Steam depot kell hozzá: {line}",
        "needs_files": "{base} fájljai kellenek hozzá: {line}",
        "not_found": "nincs a játék mappájában",
        "options": "Beállítások (később a játék indítóablakában is módosíthatók)",
        "cheats": "Csalások (fejlesztői billentyűk: h, W, B, A, K, M, #, x)",
        "unlock": "Minden küldetés elérhető (az előrehaladásod nem változik)",
        "skip_verify": "A játékfájlok ellenőrzésének kihagyása - telepítés úgy, ahogy van",
        "skip_verify_hint": "A Steames változattól eltérő kiadásokhoz. A játék feltelepül és elindul, de lehet, hogy nem "
                            "kapod meg a javításainkat, a szerkesztőt és a saját térképeket.",
        "shortcuts": "Parancsikonok",
        "desktop": "Az asztalon",
        "start_menu": "A Start menüben",
        "install": "Telepítés",
        "update": "Frissítés",
        "uninstall": "Eltávolítás",
        "run": "Játék",
        "close": "Bezárás",
        "github": "GitHub",
        "no_steam": "A Locoland nem található a Steamben. Telepítsd a Steamen keresztül, vagy válaszd ki a "
                    "mappáját (amelyikben a Steam.exe van).",
        "bad_steam": "Ebben a mappában nincs Steames Locoland (hiányzik a Steam.exe):\n{path}",
        "inside_steam": "A telepítési mappa nem lehet a játék Steam-mappája, és nem lehet azon belül.",
        "modified_steam": "Ebben a mappában a Steam.exe nem a játék eredeti fájlja - módosították vagy kicserélték (pl. egy felbontásjavítás):\n{path}\n\nÁllítsd vissza az eredetit a Steamben: jobb klikk a Locolandon > Tulajdonságok > Telepített fájlok > Játékfájlok sértetlenségének ellenőrzése. Utána futtasd újra a Setupot.",
        "modified_files": "A Locoland Steames fájljai eltérnek azoktól, amelyeket az OpenLocoland ismer, és a változtatásaink nem illeszkednek hozzájuk:\n{files}\n\n{path}\n\nÁllítsd vissza őket a Steamben: jobb klikk a Locolandon > Tulajdonságok > Telepített fájlok > Játékfájlok sértetlenségének ellenőrzése. Utána futtasd újra a Setupot. Vagy jelöld be a \"A játékfájlok ellenőrzésének kihagyása\" négyzetet: ezek a fájlok úgy maradnak, ahogy vannak, a változtatásaink nélkül. Az OpenLocolandnak saját beállítása van a fejlesztői billentyűkhöz: Csalások, lent.",
        "unplaced": "Figyelem: {file} úgy marad, mint a Steamben (a változtatásaink nem illeszkednek hozzá) - hiányozni fog: {feature}",
        "feature_intro": "az intro a filmek nélkül",
        "feature_custom_maps": "a saját térképek oldala és a térképszerkesztő a menüben, Minden küldetés",
        "feature_custom_play": "a saját térképek céljai, pontszámai és a visszaút az oldalukra",
        "feature_esc_menu": "a saját térképek Esc menüje (eligazítás, újraindítás, vissza a szerkesztőbe)",
        "feature_cheats": "a Csalások beállítás",
        "feature_dev_texts": "a fejlesztői mód lefordított szövegei",
        "feature_other": "a változtatásaink benne",
        "as_is": "A Steam.exe nem a 2003-10-07-i Steam-változat: úgy települt, ahogy van - javítások, indítóbeállítások, szerkesztő és a saját térképek oldala nélkül",
        "not_empty": "A mappa nem üres, és nincs benne OpenLocoland:\n{path}\n\n"
                     "Mégis ide telepítsem? A többi fájlja érintetlen marad.",
        "running": "Az OpenLocoland ebből a mappából fut. Előbb zárd be a játékot.",
        "working": "Telepítés...",
        "done": "Kész. Az OpenLocoland {version} itt van: {path}",
        "failed": "A telepítés nem sikerült: {error}",
        "uninstall_ask": "Eltávolítod az OpenLocolandot innen?\n{path}\n\nA mappa minden tartalmával törlődik "
                         "(a szerkesztőben készült pályákkal és a ranglistákkal is), a parancsikonokkal és a "
                         "{captured} mappában tárolt nyelvi fájlokkal együtt.\nA profilok, az előrehaladás és a "
                         "beállítások a rendszerleíró adatbázisban maradnak (a Steames játék is használja őket).",
        "uninstalled": "Az OpenLocoland eltávolítva innen: {path}",
        "game_name": "Locoland",
        "editor_name": "Locoland - pályaszerkesztő",
        "editor_desc": "Az OpenLocoland pályaszerkesztője",
        "damaged": "A telepítő nem tudja olvasni a saját fájlját:\n{file}\n\nLehet, hogy egy víruskereső futás "
                   "közben eltávolította vagy zárolta a Setup.exe-t vagy az általa kicsomagolt fájlokat. Töltsd "
                   "le újra a telepítőt a projekt GitHub-oldaláról, és futtasd.",
        "kept": "A játék itt: {path} ugyanolyan, mint a futtatás előtt.",
        "resume": "A(z) {path} frissítése félúton megállt: futtasd újra a telepítőt, és befejezi.",
        "no_space": "Nincs elég szabad hely itt: {drive}. A telepítőnek futás közben körülbelül {need} MB "
                    "kell, szabad {free} MB. Szabadíts fel helyet, és futtasd újra a telepítőt.",
    },
    "pl": {
        "name": "Polski",
        "title": "OpenLocoland {version} - instalator",
        "intro": "OpenLocoland powstaje z Twojej kopii Locoland ze Steama, w osobnym folderze. "
                 "Gra w Steamie jest tylko czytana i pozostaje bez zmian.",
        "language": "Język:",
        "steam": "Locoland w Steamie:",
        "target": "Zainstaluj w:",
        "browse": "Wybierz...",
        "game_language": "Język gry:",
        "more_languages": "Inne języki: zobacz uwagę poniżej.",
        "depots": "Pozostałe języki gry pochodzą ze Steama: otwórz konsolę Steama (wpisz steam://open/console "
                  "w pasku adresu przeglądarki albo w Win+R), wpisz wiersz każdego potrzebnego języka, poczekaj, "
                  "aż Steam go pobierze, i uruchom instalator ponownie:",
        "no_language": "Nie da się zbudować żadnego języka gry - czy Locoland jest zainstalowany przez Steam?",
        "depots_local": "Pozostałych języków gry nie znaleziono (bez weryfikacji wersji nie ma depotów Steama):",
        "unofficial": "(nieoficjalne)",
        "needs_depot": "wymaga depotu Steama: {line}",
        "needs_files": "wymaga plików wersji {base}: {line}",
        "not_found": "nie ma w folderze gry",
        "options": "Opcje (można je później zmienić w oknie startowym gry)",
        "cheats": "Kody (klawisze twórców: h, W, B, A, K, M, #, x)",
        "unlock": "Wszystkie misje odblokowane (Twoje postępy pozostają bez zmian)",
        "skip_verify": "Pomiń weryfikację plików gry - instaluj jak jest",
        "skip_verify_hint": "Dla innych wersji gry niż ta ze Steama. Gra się zainstaluje i uruchomi, ale możesz nie "
                            "dostać naszych poprawek, edytora ani własnych map.",
        "shortcuts": "Skróty",
        "desktop": "Na pulpicie",
        "start_menu": "W menu Start",
        "install": "Zainstaluj",
        "update": "Aktualizuj",
        "uninstall": "Odinstaluj",
        "run": "Graj",
        "close": "Zamknij",
        "github": "GitHub",
        "no_steam": "Nie znaleziono Locoland w Steamie. Zainstaluj grę przez Steam albo wskaż jej folder "
                    "(ten z plikiem Steam.exe).",
        "bad_steam": "W tym folderze nie ma Locoland ze Steama (brak Steam.exe):\n{path}",
        "inside_steam": "Folder instalacji nie może być folderem gry w Steamie ani leżeć w nim.",
        "modified_steam": "Steam.exe w tym folderze nie jest oryginalnym plikiem gry - został zmieniony lub podmieniony (np. przez poprawkę rozdzielczości):\n{path}\n\nPrzywróć oryginał w Steamie: prawy klik na Locoland > Właściwości > Zainstalowane pliki > Sprawdź spójność plików gry. Potem uruchom Setup jeszcze raz.",
        "modified_files": "Twoje pliki Locoland w Steamie różnią się od tych, które zna OpenLocoland, i nasze zmiany do nich nie pasują:\n{files}\n\n{path}\n\nPrzywróć je w Steamie: prawy klik na Locoland > Właściwości > Zainstalowane pliki > Sprawdź spójność plików gry. Potem uruchom Setup jeszcze raz. Albo zaznacz \"Pomiń weryfikację plików gry\": te pliki zostaną takie, jakie są, bez naszych zmian. OpenLocoland ma własną opcję klawiszy twórców: Kody, poniżej.",
        "unplaced": "Uwaga: {file} zostaje taki jak w Steamie (nasze zmiany do niego nie pasują) - zabraknie: {feature}",
        "feature_intro": "intra bez filmów",
        "feature_custom_maps": "strony własnych map i edytora map w menu, opcji Wszystkie misje",
        "feature_custom_play": "celów i wyników własnych map oraz powrotu na ich stronę",
        "feature_esc_menu": "menu Esc własnych map (odprawa, restart, powrót do edytora)",
        "feature_cheats": "opcji Kody",
        "feature_dev_texts": "przetłumaczonych tekstów trybu twórców",
        "feature_other": "naszych zmian w nim",
        "as_is": "Steam.exe nie jest wersją Steam z 2003-10-07: zainstalowano jak jest - bez poprawek, opcji launchera, edytora i strony własnych map",
        "not_empty": "Folder nie jest pusty i nie ma w nim OpenLocoland:\n{path}\n\n"
                     "Zainstalować mimo to? Pozostałe pliki zostaną nietknięte.",
        "running": "OpenLocoland jest uruchomiony z tego folderu. Najpierw zamknij grę.",
        "working": "Instalowanie...",
        "done": "Gotowe. OpenLocoland {version} jest w {path}",
        "failed": "Instalacja nie powiodła się: {error}",
        "uninstall_ask": "Usunąć OpenLocoland z\n{path}?\n\nFolder zostanie usunięty z całą zawartością "
                         "(także z mapami z edytora i tabelami wyników), razem ze skrótami i plikami języków "
                         "w {captured}.\nProfile, postępy i ustawienia w rejestrze zostają (używa ich też gra "
                         "ze Steama).",
        "uninstalled": "Usunięto OpenLocoland z {path}",
        "game_name": "Locoland",
        "editor_name": "Locoland - edytor map",
        "editor_desc": "Edytor Map OpenLocoland",
        "damaged": "Instalator nie może odczytać własnego pliku:\n{file}\n\nByć może program antywirusowy usunął "
                   "lub zablokował Setup.exe albo rozpakowane przez niego pliki w trakcie działania. Pobierz "
                   "instalator ponownie ze strony projektu na GitHubie i uruchom go.",
        "kept": "Gra w {path} jest taka jak przed tym uruchomieniem instalatora.",
        "resume": "Aktualizacja {path} zatrzymała się w połowie: uruchom instalator ponownie, a dokończy ją.",
        "no_space": "Za mało wolnego miejsca na {drive} - instalator potrzebuje w trakcie pracy około {need} "
                    "MB, wolne jest {free} MB. Zwolnij tam trochę miejsca i uruchom instalator ponownie.",
    },
    "pt": {
        "name": "Português",
        "title": "OpenLocoland {version} - instalação",
        "intro": "O OpenLocoland é criado a partir da tua cópia de Locoland do Steam, numa pasta própria. "
                 "O jogo do Steam é apenas lido e fica como está.",
        "language": "Idioma:",
        "steam": "Locoland no Steam:",
        "target": "Instalar em:",
        "browse": "Procurar...",
        "game_language": "Idioma do jogo:",
        "more_languages": "Outros idiomas: vê a nota abaixo.",
        "depots": "Os outros idiomas do jogo vêm do Steam: abre a consola do Steam (escreve "
                  "steam://open/console na barra de endereços do navegador ou em Win+R), introduz a linha de "
                  "cada idioma que queres, espera que o Steam o descarregue e executa o Setup de novo:",
        "no_language": "Nenhum idioma do jogo pode ser criado - o Locoland está instalado através do Steam?",
        "depots_local": "Os outros idiomas do jogo não foram encontrados (sem verificação de versão não há depots do Steam):",
        "unofficial": "(não oficial)",
        "needs_depot": "precisa do depot do Steam: {line}",
        "needs_files": "precisa dos ficheiros de {base}: {line}",
        "not_found": "não encontrado na pasta do jogo",
        "options": "Opções (podem ser alteradas mais tarde na janela inicial do jogo)",
        "cheats": "Batotas (teclas de programador: h, W, B, A, K, M, #, x)",
        "unlock": "Todas as missões desbloqueadas (o teu progresso fica como está)",
        "skip_verify": "Ignorar a verificação dos ficheiros do jogo - instalar como está",
        "skip_verify_hint": "Para versões do jogo que não sejam a do Steam. O jogo instala-se e funciona, mas podes "
                            "ficar sem as nossas correções, o editor ou os mapas próprios.",
        "shortcuts": "Atalhos",
        "desktop": "No ambiente de trabalho",
        "start_menu": "No menu Iniciar",
        "install": "Instalar",
        "update": "Atualizar",
        "uninstall": "Desinstalar",
        "run": "Jogar",
        "close": "Fechar",
        "github": "GitHub",
        "no_steam": "O Locoland não foi encontrado no Steam. Instala-o através do Steam ou escolhe a sua "
                    "pasta (a que tem o Steam.exe).",
        "bad_steam": "Esta pasta não tem o Locoland do Steam (falta o Steam.exe):\n{path}",
        "inside_steam": "A pasta de instalação não pode ser a pasta do jogo no Steam nem estar dentro dela.",
        "modified_steam": "O Steam.exe desta pasta não é o ficheiro original do jogo - foi alterado ou substituído (p. ex. por uma correção de resolução):\n{path}\n\nReponha o original no Steam: clique com o botão direito em Locoland > Propriedades > Ficheiros instalados > Verificar a integridade dos ficheiros do jogo. Depois execute o Setup outra vez.",
        "modified_files": "Os seus ficheiros do Locoland no Steam são diferentes dos que o OpenLocoland conhece, e as nossas alterações não encaixam neles:\n{files}\n\n{path}\n\nReponha-os no Steam: clique com o botão direito em Locoland > Propriedades > Ficheiros instalados > Verificar a integridade dos ficheiros do jogo. Depois execute o Setup outra vez. Ou marque \"Ignorar a verificação dos ficheiros do jogo\": estes ficheiros ficam como estão, sem as nossas alterações. O OpenLocoland tem uma opção própria para as teclas de programador: Batotas, abaixo.",
        "unplaced": "Aviso: {file} fica como no Steam (as nossas alterações não encaixam nele) - vai faltar: {feature}",
        "feature_intro": "a introdução sem os filmes",
        "feature_custom_maps": "a página dos mapas próprios e o editor de mapas no menu, Todas as missões",
        "feature_custom_play": "os objetivos e pontuações dos mapas próprios e o regresso à sua página",
        "feature_esc_menu": "o menu Esc dos mapas próprios (briefing, reiniciar, voltar ao editor)",
        "feature_cheats": "a opção Batotas",
        "feature_dev_texts": "os textos traduzidos do modo de programador",
        "feature_other": "as nossas alterações nele",
        "as_is": "O Steam.exe não é a versão Steam de 2003-10-07: instalado como está - sem correções, opções do lançador, editor nem página dos mapas próprios",
        "not_empty": "A pasta não está vazia e não tem uma instalação do OpenLocoland:\n{path}\n\n"
                     "Instalar mesmo assim? Os outros ficheiros ficam intactos.",
        "running": "O OpenLocoland está a correr a partir desta pasta. Fecha primeiro o jogo.",
        "working": "A instalar...",
        "done": "Concluído. O OpenLocoland {version} está em {path}",
        "failed": "A instalação falhou: {error}",
        "uninstall_ask": "Remover o OpenLocoland de\n{path}?\n\nA pasta é apagada com todo o conteúdo (também "
                         "os mapas feitos no editor e as pontuações), com os atalhos e os ficheiros de idioma "
                         "guardados em {captured}.\nOs perfis, o progresso e as definições no registo ficam (o "
                         "jogo do Steam também os usa).",
        "uninstalled": "O OpenLocoland foi removido de {path}",
        "game_name": "Locoland",
        "editor_name": "Locoland - editor de mapas",
        "editor_desc": "O Editor de Mapas do OpenLocoland",
        "damaged": "O Setup não consegue ler um ficheiro seu:\n{file}\n\nTalvez um antivírus tenha removido ou "
                   "bloqueado o Setup.exe ou os ficheiros que ele extraiu enquanto corria. Descarrega o Setup de "
                   "novo da página do projeto no GitHub e executa-o.",
        "kept": "O jogo em {path} está como antes desta execução.",
        "resume": "A atualização de {path} parou a meio: executa o Setup de novo e ele termina-a.",
        "no_space": "Não há espaço livre suficiente em {drive} - o Setup precisa de cerca de {need} MB enquanto "
                    "trabalha e há {free} MB livres. Liberta espaço e executa o Setup de novo.",
    },
    "ru": {
        "name": "Русский",
        "title": "OpenLocoland {version} - установка",
        "intro": "OpenLocoland собирается из вашей копии Locoland из Steam в отдельной папке. "
                 "Игра в Steam только читается и остаётся без изменений.",
        "language": "Язык:",
        "steam": "Locoland в Steam:",
        "target": "Установить в:",
        "browse": "Обзор...",
        "game_language": "Язык игры:",
        "more_languages": "Другие языки: см. примечание ниже.",
        "depots": "Остальные языки игры берутся из Steam: откройте консоль Steam (введите "
                  "steam://open/console в адресную строку браузера или в Win+R), введите строку каждого "
                  "нужного языка, дождитесь, пока Steam его скачает, и запустите установку снова:",
        "no_language": "Ни один язык игры не может быть собран - установлена ли Locoland через Steam?",
        "depots_local": "Другие языки игры не найдены (без проверки версии депотов Steam нет):",
        "unofficial": "(неофициальный)",
        "needs_depot": "нужен депот Steam: {line}",
        "needs_files": "нужны файлы версии {base}: {line}",
        "not_found": "нет в папке игры",
        "options": "Параметры (их можно изменить позже в окне запуска игры)",
        "cheats": "Читы (клавиши разработчиков: h, W, B, A, K, M, #, x)",
        "unlock": "Все миссии открыты (ваш прогресс не меняется)",
        "skip_verify": "Пропустить проверку файлов игры - установить как есть",
        "skip_verify_hint": "Для версий игры не из Steam. Игра установится и запустится, но наших исправлений, редактора "
                            "и своих карт может и не быть.",
        "shortcuts": "Ярлыки",
        "desktop": "На рабочем столе",
        "start_menu": "В меню «Пуск»",
        "install": "Установить",
        "update": "Обновить",
        "uninstall": "Удалить",
        "run": "Играть",
        "close": "Закрыть",
        "github": "GitHub",
        "no_steam": "Locoland не найдена в Steam. Установите её через Steam или выберите её папку "
                    "(ту, где лежит Steam.exe).",
        "bad_steam": "В этой папке нет Locoland из Steam (нет Steam.exe):\n{path}",
        "inside_steam": "Папка установки не может быть папкой игры в Steam или находиться внутри неё.",
        "modified_steam": "Steam.exe в этой папке - не оригинальный файл игры: он изменён или заменён (например, исправлением разрешения):\n{path}\n\nВосстановите оригинал в Steam: правый щелчок по Locoland > Свойства > Установленные файлы > Проверить целостность файлов игры. Затем снова запустите Setup.",
        "modified_files": "Ваши файлы Locoland в Steam отличаются от известных OpenLocoland, и наши изменения к ним не подходят:\n{files}\n\n{path}\n\nВосстановите их в Steam: правый щелчок по Locoland > Свойства > Установленные файлы > Проверить целостность файлов игры. Затем снова запустите Setup. Или отметьте \"Пропустить проверку файлов игры\": эти файлы останутся как есть, без наших изменений. У OpenLocoland есть своя настройка для клавиш разработчиков: Читы, ниже.",
        "unplaced": "Внимание: {file} остаётся как в Steam (наши изменения к нему не подходят) - не будет: {feature}",
        "feature_intro": "заставки без видеороликов",
        "feature_custom_maps": "страницы своих карт и редактора карт в меню, опции Все миссии",
        "feature_custom_play": "целей и очков своих карт и возврата на их страницу",
        "feature_esc_menu": "меню Esc своих карт (брифинг, перезапуск, возврат в редактор)",
        "feature_cheats": "опции Читы",
        "feature_dev_texts": "переведённых текстов режима разработчика",
        "feature_other": "наших изменений в нём",
        "as_is": "Steam.exe - не версия Steam от 2003-10-07: установлено как есть - без исправлений, настроек лаунчера, редактора и страницы своих карт",
        "not_empty": "Папка не пуста, и в ней нет OpenLocoland:\n{path}\n\n"
                     "Всё равно установить туда? Остальные файлы останутся нетронутыми.",
        "running": "OpenLocoland запущена из этой папки. Сначала закройте игру.",
        "working": "Установка...",
        "done": "Готово. OpenLocoland {version} находится в {path}",
        "failed": "Установка не удалась: {error}",
        "uninstall_ask": "Удалить OpenLocoland из\n{path}?\n\nПапка будет удалена со всем содержимым (и с "
                         "картами из редактора, и с таблицами рекордов), вместе с ярлыками и языковыми файлами "
                         "в {captured}.\nПрофили, прогресс и настройки в реестре останутся (их использует и игра "
                         "из Steam).",
        "uninstalled": "OpenLocoland удалена из {path}",
        "game_name": "Locoland",
        "editor_name": "Locoland - редактор карт",
        "editor_desc": "Редактор Карт OpenLocoland",
        "damaged": "Установщик не может прочитать собственный файл:\n{file}\n\nВозможно, антивирус удалил или "
                   "заблокировал Setup.exe или распакованные им файлы во время работы. Скачайте установщик заново "
                   "со страницы проекта на GitHub и запустите его.",
        "kept": "Игра в {path} осталась такой же, как до этого запуска.",
        "resume": "Обновление {path} остановилось на полпути: запустите установщик снова, и он его завершит.",
        "no_space": "Недостаточно свободного места на {drive} - установщику во время работы нужно около {need} "
                    "МБ, свободно {free} МБ. Освободите место и запустите установщик снова.",
    },
    "cs": {
        "name": "Čeština",
        "title": "OpenLocoland {version} - instalace",
        "intro": "OpenLocoland se sestaví z tvé kopie Locolandu ze Steamu do vlastní složky. "
                 "Hra ve Steamu se jen čte a zůstane beze změny.",
        "language": "Jazyk:",
        "steam": "Locoland ve Steamu:",
        "target": "Instalovat do:",
        "browse": "Procházet...",
        "game_language": "Jazyk hry:",
        "more_languages": "Další jazyky: viz poznámka níže.",
        "depots": "Další jazyky hry pocházejí ze Steamu: otevři konzoli Steamu (zadej steam://open/console "
                  "do adresního řádku prohlížeče nebo do Win+R), zadej řádek každého jazyka, který chceš, "
                  "počkej, až ho Steam stáhne, a spusť instalaci znovu:",
        "no_language": "Nelze sestavit žádný jazyk hry - je Locoland nainstalován přes Steam?",
        "depots_local": "Další jazyky hry nebyly nalezeny (bez kontroly verze nejsou depoty Steamu):",
        "unofficial": "(neoficiální)",
        "needs_depot": "vyžaduje depot Steamu: {line}",
        "needs_files": "vyžaduje soubory verze {base}: {line}",
        "not_found": "není ve složce hry",
        "options": "Volby (lze je později změnit v úvodním okně hry)",
        "cheats": "Cheaty (klávesy vývojářů: h, W, B, A, K, M, #, x)",
        "unlock": "Všechny mise odemčené (tvůj postup zůstane, jak je)",
        "skip_verify": "Přeskočit kontrolu souborů hry - instalovat, jak je",
        "skip_verify_hint": "Pro jiné verze hry než tu ze Steamu. Hra se nainstaluje a spustí, ale možná nebudeš mít "
                            "naše opravy, editor ani vlastní mapy.",
        "shortcuts": "Zástupci",
        "desktop": "Na ploše",
        "start_menu": "V nabídce Start",
        "install": "Instalovat",
        "update": "Aktualizovat",
        "uninstall": "Odinstalovat",
        "run": "Hrát",
        "close": "Zavřít",
        "github": "GitHub",
        "no_steam": "Locoland nebyl ve Steamu nalezen. Nainstaluj ho přes Steam nebo vyber jeho složku "
                    "(tu se Steam.exe).",
        "bad_steam": "V této složce není Locoland ze Steamu (chybí Steam.exe):\n{path}",
        "inside_steam": "Instalační složka nesmí být složkou hry ve Steamu ani v ní ležet.",
        "modified_steam": "Steam.exe v této složce není původní soubor hry - byl změněn nebo nahrazen (např. opravou rozlišení):\n{path}\n\nObnovte originál ve Steamu: pravým tlačítkem na Locoland > Vlastnosti > Nainstalované soubory > Ověřit integritu herních souborů. Pak spusťte Setup znovu.",
        "modified_files": "Vaše soubory Locoland ve Steamu se liší od těch, které OpenLocoland zná, a naše změny do nich nepasují:\n{files}\n\n{path}\n\nObnovte je ve Steamu: pravým tlačítkem na Locoland > Vlastnosti > Nainstalované soubory > Ověřit integritu herních souborů. Pak spusťte Setup znovu. Nebo zaškrtněte \"Přeskočit kontrolu souborů hry\": tyto soubory zůstanou, jak jsou, bez našich změn. OpenLocoland má pro klávesy vývojářů vlastní volbu: Cheaty, níže.",
        "unplaced": "Pozor: {file} zůstává jako ve Steamu (naše změny do něj nepasují) - bude chybět: {feature}",
        "feature_intro": "úvod bez filmů",
        "feature_custom_maps": "stránka vlastních map a editor map v menu, Všechny mise",
        "feature_custom_play": "cíle a skóre vlastních map a návrat na jejich stránku",
        "feature_esc_menu": "menu Esc vlastních map (briefing, restart, zpět do editoru)",
        "feature_cheats": "volba Cheaty",
        "feature_dev_texts": "přeložené texty vývojářského režimu",
        "feature_other": "naše změny v něm",
        "as_is": "Steam.exe není verze ze Steamu z 2003-10-07: nainstalováno, jak je - bez oprav, voleb spouštěče, editoru a stránky vlastních map",
        "not_empty": "Složka není prázdná a není v ní OpenLocoland:\n{path}\n\n"
                     "Přesto do ní instalovat? Ostatní soubory zůstanou nedotčené.",
        "running": "OpenLocoland běží z této složky. Nejdřív hru ukonči.",
        "working": "Instalace...",
        "done": "Hotovo. OpenLocoland {version} je v {path}",
        "failed": "Instalace se nezdařila: {error}",
        "uninstall_ask": "Odebrat OpenLocoland ze složky\n{path}?\n\nSložka se smaže s celým obsahem (i s mapami "
                         "z editoru a tabulkami výsledků), spolu se zástupci a jazykovými soubory v {captured}.\n"
                         "Profily, postup a nastavení v registru zůstanou (používá je i hra ze Steamu).",
        "uninstalled": "OpenLocoland byl odebrán ze složky {path}",
        "game_name": "Locoland",
        "editor_name": "Locoland - editor map",
        "editor_desc": "Editor Map OpenLocoland",
        "damaged": "Instalátor nemůže přečíst vlastní soubor:\n{file}\n\nMožná antivirový program během běhu "
                   "odstranil nebo zablokoval Setup.exe nebo soubory, které rozbalil. Stáhni instalátor znovu ze "
                   "stránky projektu na GitHubu a spusť ho.",
        "kept": "Hra v {path} je taková jako před tímto spuštěním.",
        "resume": "Aktualizace {path} se zastavila v polovině: spusť instalátor znovu a dokončí ji.",
        "no_space": "Na {drive} není dost volného místa: instalátor během práce potřebuje asi {need} MB, "
                    "volných je {free} MB. Uvolni tam místo a spusť instalátor znovu.",
    },
}
UI_LANGUAGES = [("en", TEXTS["en"]["name"])] + sorted(((c, t["name"]) for c, t in TEXTS.items() if c != "en"),
                                                      key=lambda ct: ct[0])


def system_language():
    """The code of Windows' display language ("pl" for pl-PL), or None."""
    import ctypes
    import locale
    try:
        name = locale.windows_locale.get(ctypes.windll.kernel32.GetUserDefaultUILanguage(), "")
    except (AttributeError, OSError):
        return None
    return name.split("_")[0].lower() or None


def first_ui_language():
    """The window's language: the system's when the window has it, else English."""
    code = system_language()
    return code if code in TEXTS else "en"


# ---- Steam ----

def steam_folders():
    """Steam's own folder from the registry (the user's key first)."""
    found = []
    for hive, key, value in [(winreg.HKEY_CURRENT_USER, r"Software\Valve\Steam", "SteamPath"),
                             (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\WOW6432Node\Valve\Steam", "InstallPath"),
                             (winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Valve\Steam", "InstallPath")]:
        try:
            with winreg.OpenKey(hive, key) as k:
                path = os.path.normpath(winreg.QueryValueEx(k, value)[0])
        except OSError:
            continue
        if os.path.isdir(path) and path.lower() not in (f.lower() for f in found):
            found.append(path)
    return found


def libraries(steam):
    """The library folders of a Steam install (steamapps\\libraryfolders.vdf), itself first."""
    found = [steam]
    try:
        text = open(os.path.join(steam, "steamapps", "libraryfolders.vdf"), encoding="utf-8", errors="replace").read()
    except OSError:
        return found
    import re
    for m in re.finditer(r'"path"\s+"([^"]+)"', text):
        path = os.path.normpath(m.group(1).replace("\\\\", "\\"))
        if path.lower() not in (f.lower() for f in found) and os.path.isdir(path):
            found.append(path)
    return found


def is_game(folder):
    """True if folder contains Steam.exe (original) or Locoland.exe (patched)."""
    return bool(folder) and (os.path.isfile(os.path.join(folder, "Steam.exe"))
                             or os.path.isfile(os.path.join(folder, "Locoland.exe")))


def true_case(path):
    """`path` as the disk spells it (Steam's registry keeps its folder in lower case)."""
    drive, rest = os.path.splitdrive(os.path.abspath(path))
    result = drive.upper() + "\\"
    for part in [p for p in rest.split("\\") if p]:
        try:
            part = next((n for n in os.listdir(result) if n.lower() == part.lower()), part)
        except OSError:
            pass
        result = os.path.join(result, part)
    return result


def find_game():
    """The folder of Locoland in one of the Steam libraries, or None."""
    for steam in steam_folders():
        for library in libraries(steam):
            folder = os.path.join(library, "steamapps", "common", "Locoland")
            if is_game(folder):
                return true_case(folder)
    return languages.STEAM if is_game(languages.STEAM) else None


def inside(path, folder):
    path, folder = os.path.normcase(os.path.abspath(path)), os.path.normcase(os.path.abspath(folder))
    return path == folder or path.startswith(folder.rstrip("\\") + "\\")


# ---- languages and the logo ----

def pack_value(code, key):
    """A value of [language] in lang\\<code>\\language.ini, or None."""
    try:
        section = None
        for line in open(os.path.join(REPO, "lang", code, "language.ini"), encoding="utf-8-sig"):
            line = line.strip()
            if line.startswith("["):
                section = line
            elif section == "[language]" and line.startswith(key + "="):
                return line.split("=", 1)[1].strip()
    except OSError:
        pass
    return None


def pack_name(code):
    """The name of a language pack, or of the language of a Steam depot without one."""
    return pack_value(code, "name") or TEXTS.get(code, {}).get("name") or code


def pack_base(code):
    """The language whose Steam files a pack is built on: its own, or for a translation Steam does
    not ship (Czech) the one `base=` names in its language.ini (languages.pack_base, if there)."""
    if hasattr(languages, "pack_base"):
        return languages.pack_base(code)
    return pack_value(code, "base") or code


def game_languages(steam, skip_verify=False):
    """(buildable, missing, have): the codes of the packs of lang\\ that can be built from the
    captures, Steam's languages and the editions at hand (languages.available), of those whose
    language Steam still has to download, and of the languages there are. A fan translation is
    built on any captured language when its own base is not there (languages.build_base)."""
    packs = languages.packs()
    have = languages.available(steam) if is_game(steam) else set()
    buildable = [c for c in packs if languages.build_base(c, have)]
    return buildable, [c for c in packs if c not in buildable], have


def is_fan_pack(code, have=None):
    """Marked unofficial: a translation Steam does not ship, built on another language's files
    (languages.unofficial - not when its own edition is at hand)."""
    if hasattr(languages, "unofficial"):
        return languages.unofficial(code, have)
    return hasattr(languages, "fan_pack") and languages.fan_pack(code)


def depot_line(code):
    """The Steam console line of the language a pack needs (its base's for a translation), or None."""
    depot = {c: d for d, c in languages.DEPOT_LANGUAGES.items()}.get(pack_base(code))
    return f"download_depot {languages.STEAM_APP_ID} {depot}" if depot else None


def depot_lines(codes):
    """The Steam console line of each pack's language (its base's for a translation), once each."""
    lines, seen = [], set()
    for c in codes:
        base = pack_base(c)
        if depot_line(c) and base not in seen:
            seen.add(base)
            lines.append(f"    {depot_line(c)}    {pack_name(base)}")
    return lines


def custom_logo_file():
    """Our own OpenLocoland wordmark (tools\\icon\\openlocoland_logo.png), or None."""
    if BUNDLE:
        path = os.path.join(BUNDLE, "tools", "icon", "openlocoland_logo.png")
    else:
        path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "icon", "openlocoland_logo.png")
    return path if os.path.isfile(path) else None


def logo_file(steam):
    """The game's logo (vid\\logo.bmp): the Steam copy's, unless it is the English one, which says
    "Steamland" - then that of another captured or downloaded language, if there is one."""
    found = [(languages.steam_language(steam), os.path.join(steam, "vid", "logo.bmp"))]
    if os.path.isdir(CAPTURED):
        found += [(c, os.path.join(CAPTURED, c, "vid", "logo.bmp")) for c in sorted(os.listdir(CAPTURED))]
    found += [(c, os.path.join(folder, "vid", "logo.bmp")) for c, folder in languages.downloaded_depots(steam)]
    found = [(c, path) for c, path in found if os.path.isfile(path)]
    for code, path in found:
        if code not in ("en", "pt"):   # Steam's Portuguese is the English game
            return path
    return found[0][1] if found else None


def logo_image(steam, height=56):
    """The game's logo cropped to its letters (the picture is 640x480 with the word on black), or None."""
    path = logo_file(steam) if is_game(steam) else None
    if not path:
        return None
    try:
        from PIL import Image, ImageTk
        picture = Image.open(path).convert("RGB")
        box = picture.convert("L").point(lambda v: 255 if v > 40 else 0).getbbox()
        if not box:
            return None
        picture = picture.crop((box[0] - 6, box[1] - 6, box[2] + 6, box[3] + 6))
        width = picture.width * height // picture.height
        return ImageTk.PhotoImage(picture.resize((width, height), Image.LANCZOS))
    except Exception:   # an unreadable picture only costs the header its logo
        return None


def logo_banner(width):
    """Our wordmark as the header's banner: the transparent margins cut, black behind it (the
    header's colour) and scaled to the whole width of the window, so it fills the header."""
    path = custom_logo_file()
    if not path or width < 64:
        return None
    try:
        from PIL import Image, ImageOps, ImageTk
        picture = Image.open(path).convert("RGBA")
        box = picture.getbbox()   # the transparent 1920x1080 canvas around the word
        if not box:
            return None
        picture = ImageOps.expand(picture.crop(box), border=(10, 12, 10, 12), fill=(0, 0, 0, 255))
        black = Image.new("RGBA", picture.size, (0, 0, 0, 255))
        picture = Image.alpha_composite(black, picture).convert("RGB")
        height = max(1, round(picture.height * width / picture.width))
        return ImageTk.PhotoImage(picture.resize((width, height), Image.LANCZOS))
    except Exception:   # an unreadable picture only costs the header its logo
        return None


# ---- registry, shortcuts, the install's record ----

def read_setting(name):
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, SETTINGS_KEY) as k:
            return winreg.QueryValueEx(k, name)
    except OSError:
        return None, None


def read_option(name):
    value, kind = read_setting(name)
    return kind == winreg.REG_DWORD and value != 0


def write_setting(name, value):
    with winreg.CreateKey(winreg.HKEY_CURRENT_USER, SETTINGS_KEY) as k:
        if isinstance(value, str):
            winreg.SetValueEx(k, name, 0, winreg.REG_SZ, value)
        else:
            winreg.SetValueEx(k, name, 0, winreg.REG_DWORD, 1 if value else 0)


def saved_game_language():
    value, kind = read_setting("Language")   # language.cpp
    return value if kind == winreg.REG_SZ else None


def first_game_language(buildable, update):
    """The game language the window starts with: on an update the one chosen before (when it can
    be built), otherwise the system's language when there is a pack for it, else English, or the
    first one there is."""
    saved = saved_game_language()
    if update and saved in buildable:
        return saved
    for code in (system_language(), DEFAULT_GAME_LANGUAGE):
        if code in buildable:
            return code
    return buildable[0] if buildable else None


def special_folder(csidl):
    import ctypes
    buffer = ctypes.create_unicode_buffer(260)
    ctypes.windll.shell32.SHGetFolderPathW(None, csidl, None, 0, buffer)
    return buffer.value


SHORTCUT_ROOT = None   # --shortcut-root <folder> (tests): Desktop\ and Programs\ in it instead of the real ones


def shortcut_folders(desktop, start_menu):
    """The folders the shortcuts go into: the desktop and the Start menu's Programs\\OpenLocoland."""
    folders = []
    if desktop:
        folders.append(os.path.join(SHORTCUT_ROOT, "Desktop") if SHORTCUT_ROOT
                       else special_folder(0x10))   # CSIDL_DESKTOPDIRECTORY
    if start_menu:
        folders.append(os.path.join(os.path.join(SHORTCUT_ROOT, "Programs") if SHORTCUT_ROOT
                                    else special_folder(0x02), APP))   # CSIDL_PROGRAMS
    return folders


def com_method(pointer, index, *argtypes):
    """Method `index` of the vtable of the COM interface `pointer` (HRESULT: a failure raises OSError)."""
    import ctypes
    vtable = ctypes.cast(pointer, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
    prototype = ctypes.WINFUNCTYPE(ctypes.HRESULT, ctypes.c_void_p, *argtypes)
    return lambda *args: prototype(vtable[index])(pointer, *args)


def guid(text):
    import ctypes
    value = (ctypes.c_byte * 16)()
    ctypes.oledll.ole32.CLSIDFromString(text, ctypes.byref(value))
    return value


CLSID_SHELL_LINK = "{00021401-0000-0000-C000-000000000046}"
IID_SHELL_LINK_W = "{000214F9-0000-0000-C000-000000000046}"
IID_PERSIST_FILE = "{0000010B-0000-0000-C000-000000000046}"


def make_shortcut(path, target, arguments, folder, icon, description):
    """A .lnk through the shell's own ShellLink object (IShellLinkW and IPersistFile, in this process -
    what WScript.Shell's CreateShortcut does inside): the target, its arguments, the working folder, the
    icon (`icon`, index 0), the description and a normal window (SW_SHOWNORMAL)."""
    import ctypes
    from ctypes import c_int, c_void_p, c_wchar_p
    ole32 = ctypes.oledll.ole32
    os.makedirs(os.path.dirname(path), exist_ok=True)
    started = ctypes.windll.ole32.CoInitializeEx(None, 2)   # COINIT_APARTMENTTHREADED; S_FALSE: already
    link, persist = c_void_p(), c_void_p()
    try:
        ole32.CoCreateInstance(ctypes.byref(guid(CLSID_SHELL_LINK)), None, 1,   # CLSCTX_INPROC_SERVER
                               ctypes.byref(guid(IID_SHELL_LINK_W)), ctypes.byref(link))
        com_method(link, 20, c_wchar_p)(target)                   # SetPath
        com_method(link, 11, c_wchar_p)(arguments)                # SetArguments
        com_method(link, 9, c_wchar_p)(folder)                    # SetWorkingDirectory
        com_method(link, 17, c_wchar_p, c_int)(icon, 0)           # SetIconLocation
        com_method(link, 7, c_wchar_p)(description)               # SetDescription
        com_method(link, 15, c_int)(1)                            # SetShowCmd(SW_SHOWNORMAL)
        com_method(link, 0, c_void_p, c_void_p)(ctypes.byref(guid(IID_PERSIST_FILE)), ctypes.byref(persist))
        com_method(persist, 6, c_wchar_p, c_int)(path, 1)         # IPersistFile::Save(path, remember)
    finally:
        for pointer in (persist, link):
            if pointer.value:
                ctypes.WINFUNCTYPE(ctypes.c_ulong, c_void_p)(
                    ctypes.cast(pointer, ctypes.POINTER(ctypes.POINTER(c_void_p))).contents[2])(pointer)   # Release
        if started in (0, 1):   # S_OK, S_FALSE: this call's to undo; RPC_E_CHANGED_MODE: not ours
            ctypes.windll.ole32.CoUninitialize()


def remembered_install():
    """The folder the last install finished through the window went to, or None."""
    try:
        with open(os.path.join(STATE_FOLDER, LAST_INSTALL), encoding="utf-8") as f:
            folder = json.load(f).get("install")
        return folder if isinstance(folder, str) and folder else None
    except (OSError, ValueError, AttributeError):
        return None


def remember_install(target):
    os.makedirs(STATE_FOLDER, exist_ok=True)
    write_json_safely(os.path.join(STATE_FOLDER, LAST_INSTALL), {"install": os.path.abspath(target)})
    print(f"Remembered for the next run of Setup: {os.path.abspath(target)} ({os.path.join(STATE_FOLDER, LAST_INSTALL)})")


def forget_install(target):
    """Uninstall: the remembered folder is forgotten when it is the one removed."""
    remembered = remembered_install()
    if remembered and os.path.normcase(os.path.abspath(remembered)) == os.path.normcase(os.path.abspath(target)):
        try:
            os.remove(os.path.join(STATE_FOLDER, LAST_INSTALL))
        except OSError:
            pass


def remove_setup_state(folder):
    """Uninstall: %LOCALAPPDATA%\\OpenLocoland goes with the captures and unpacked Setups in it - but the
    remembered folder stays when it names another install that is still there (forget_install has
    already dropped it when it named the one removed)."""
    remembered = remembered_install()
    keep = (remembered and os.path.normcase(os.path.abspath(folder)) == os.path.normcase(os.path.abspath(STATE_FOLDER))
            and os.path.isdir(remembered) and read_manifest(remembered) is not None)
    if not keep:
        shutil.rmtree(folder, ignore_errors=True)
        return
    for name in os.listdir(folder) if os.path.isdir(folder) else []:
        path = os.path.join(folder, name)
        if name == LAST_INSTALL:
            continue
        if os.path.isdir(path):
            shutil.rmtree(path, ignore_errors=True)
        else:
            try:
                os.remove(path)
            except OSError:
                pass


def shortcut_target(path):
    """The program a .lnk starts (IShellLinkW, in this process; nothing is written), or None."""
    import ctypes
    from ctypes import c_int, c_ulong, c_void_p, c_wchar_p
    if not os.path.isfile(path):
        return None
    ole32 = ctypes.oledll.ole32
    started = ctypes.windll.ole32.CoInitializeEx(None, 2)
    link, persist = c_void_p(), c_void_p()
    try:
        ole32.CoCreateInstance(ctypes.byref(guid(CLSID_SHELL_LINK)), None, 1, ctypes.byref(guid(IID_SHELL_LINK_W)),
                               ctypes.byref(link))
        com_method(link, 0, c_void_p, c_void_p)(ctypes.byref(guid(IID_PERSIST_FILE)), ctypes.byref(persist))
        com_method(persist, 5, c_wchar_p, c_ulong)(path, 0)   # IPersistFile::Load, STGM_READ
        buffer = ctypes.create_unicode_buffer(32768)
        com_method(link, 3, c_wchar_p, c_int, c_void_p, c_ulong)(buffer, 32768, None, 4)   # GetPath, SLGP_RAWPATH
        return buffer.value or None
    except Exception:   # a damaged or odd .lnk: no target
        return None
    finally:
        for pointer in (persist, link):
            if pointer.value:
                ctypes.WINFUNCTYPE(ctypes.c_ulong, c_void_p)(
                    ctypes.cast(pointer, ctypes.POINTER(ctypes.POINTER(c_void_p))).contents[2])(pointer)
        if started in (0, 1):
            ctypes.windll.ole32.CoUninitialize()


def local_folder(folder):
    """Whether `folder` is on a local drive (fixed, removable or RAM - GetDriveTypeW of its root, which
    does not touch the folder): a network path (UNC or a mapped drive) is not looked at, so a server
    that does not answer cannot hold Setup's start."""
    import ctypes
    folder = os.path.abspath(folder)
    if folder.startswith("\\\\"):
        return False
    drive = os.path.splitdrive(folder)[0]
    return bool(drive) and ctypes.windll.kernel32.GetDriveTypeW(drive + "\\") in (2, 3, 6)


def find_install():
    """The folder the window opens on without --target: the folder of a running Locoland.exe (a player
    with two installs who updates one from its own launcher lands on that one), the remembered one (the
    last install finished through the window), the folder the game's shortcut of the Start menu or of
    the desktop starts (installs of v0.2.0-v0.2.10 remembered nothing), else the default - each only
    when it is on a local drive and holds an install's record. Nothing of it can stop Setup: a candidate
    that cannot be read is skipped. Returns (folder, how it was found)."""
    def candidates():
        try:
            for path in running_images("Locoland.exe"):
                yield os.path.dirname(path), "a running game"
        except Exception:
            pass
        yield remembered_install(), "remembered"
        try:
            folders = shortcut_folders(True, True)[::-1]   # the Start menu's first, then the desktop's
        except Exception:
            folders = []
        for folder in folders:
            target = shortcut_target(os.path.join(folder, "Locoland.lnk"))
            yield (os.path.dirname(target) if target else None), "a shortcut " + folder

    try:
        for folder, how in candidates():
            try:
                if folder and local_folder(folder) and os.path.isdir(folder) and read_manifest(folder) is not None:
                    return os.path.abspath(folder), how
            except Exception:
                continue
    except Exception:
        pass
    return DEFAULT_TARGET, "the default"


def read_manifest(target):
    try:
        return json.load(open(os.path.join(target, MANIFEST), encoding="utf-8"))
    except (OSError, ValueError):
        return None


def running_images(name):
    """The full paths of the running processes whose program is `name` (Locoland.exe): the system's
    process list (Toolhelp) and each one's image path (QueryFullProcessImageNameW), in this process.
    A process that cannot be opened (another user's) is left out, as Get-Process gave it no path."""
    import ctypes
    from ctypes import wintypes
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
    kernel32.OpenProcess.restype = wintypes.HANDLE

    class ProcessEntry(ctypes.Structure):   # PROCESSENTRY32W
        _fields_ = [("dwSize", wintypes.DWORD), ("cntUsage", wintypes.DWORD), ("th32ProcessID", wintypes.DWORD),
                    ("th32DefaultHeapID", ctypes.c_size_t), ("th32ModuleID", wintypes.DWORD),
                    ("cntThreads", wintypes.DWORD), ("th32ParentProcessID", wintypes.DWORD),
                    ("pcPriClassBase", wintypes.LONG), ("dwFlags", wintypes.DWORD),
                    ("szExeFile", wintypes.WCHAR * 260)]

    snapshot = kernel32.CreateToolhelp32Snapshot(0x2, 0)   # TH32CS_SNAPPROCESS
    if not snapshot or snapshot == wintypes.HANDLE(-1).value:
        raise OSError(ctypes.get_last_error(), "the list of processes cannot be read")
    paths = []
    try:
        entry = ProcessEntry()
        entry.dwSize = ctypes.sizeof(entry)
        more = kernel32.Process32FirstW(snapshot, ctypes.byref(entry))
        while more:
            if entry.szExeFile.lower() == name.lower():
                process = kernel32.OpenProcess(0x1000, False, entry.th32ProcessID)   # QUERY_LIMITED_INFORMATION
                if process:
                    try:
                        buffer = ctypes.create_unicode_buffer(32768)
                        size = wintypes.DWORD(len(buffer))
                        if kernel32.QueryFullProcessImageNameW(process, 0, buffer, ctypes.byref(size)):
                            paths.append(buffer.value)
                    finally:
                        kernel32.CloseHandle(process)
            more = kernel32.Process32NextW(snapshot, ctypes.byref(entry))
    finally:
        kernel32.CloseHandle(snapshot)
    return paths


def game_running(target):
    """Whether Locoland.exe of this folder runs (the processes' image paths: running_images)."""
    exe = os.path.join(target, "Locoland.exe")
    if not os.path.isfile(exe):
        return False
    return any(os.path.normcase(path) == os.path.normcase(os.path.abspath(exe)) for path in running_images("Locoland.exe"))


def remove_shortcuts(manifest):
    for path in (manifest or {}).get("shortcuts", []):
        try:
            os.remove(path)
        except OSError:
            pass
        folder = os.path.dirname(path)
        if os.path.basename(folder) == APP and os.path.isdir(folder) and not os.listdir(folder):
            os.rmdir(folder)


def steam_files_check(steam, lang, skip_verify=False):
    """build.modified_steam_files for Setup, in the language `lang`: (the message that stops the
    install, or None; the warnings for the files left as the player's with skip_verify)."""
    modified = build.modified_steam_files(steam)
    texts = TEXTS[lang]
    if modified and not skip_verify:
        return texts["modified_files"].format(path=steam, files=build.describe_modified(modified, "\n")), []
    return None, [texts["unplaced"].format(file=name, feature=texts["feature_" + build.feature_of(name)])
                  for name in modified]


# ---- a run that is stopped half way leaves a whole game ----
#
# The build goes into <target>\.openlocoland-setup\new-<n>, which nothing of the game reads. Only when
# it has finished, the files that differ from the install's go in (commit): first their list is written
# as a journal (commit.json, flushed to the disk), then each file goes in with one rename on the same
# disk (milliseconds for the whole lot) and the folders the build made are made, then the folder is
# removed (read-only files too; one that cannot be removed waits for the next run). So a run killed (or stopped by
# a missing file of its own) while building leaves the game as it was, and only a run killed in the
# middle of the renames leaves a mixed one - which the next run of this Setup or a later one finishes
# from the journal before anything else (and any older Setup repairs by building over it). After the
# commit come the clean-up of what earlier versions left, the options, the shortcuts and, last, the
# record OpenLocoland.json.

WORK = ".openlocoland-setup"   # in the install folder: the files a run prepares (new-<n>\) and its journal
JOURNAL = "commit.json"
TEST_STOP = None   # --test-stop <point> (tests): ends the process hard there, as a kill would
TEST_FREE_MB = None   # --test-free-mb <n> (tests): the free space the check sees instead of the disk's


def test_stop(point):
    if TEST_STOP == point:
        print(f"--test-stop {point}: the process ends here")
        sys.stdout.flush()
        os._exit(3)


def work_folder(target):
    return os.path.join(target, WORK)


def record_stamp(target):
    """The size and time of the install's record, or None: a journal written before another Setup
    finished an install there is stale (that Setup built the whole game again)."""
    try:
        st = os.stat(os.path.join(target, MANIFEST))
    except OSError:
        return None
    return [st.st_size, st.st_mtime_ns]


def write_json_safely(path, data):
    """`data` as JSON at `path` through a temporary file flushed to the disk and a rename: the file is
    either the old one or the whole new one."""
    temp = path + ".tmp"
    with open(temp, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
        f.flush()
        os.fsync(f.fileno())
    replace_file(temp, path)


def writable(path):
    """Clears the read-only mark of `path` (files copied from a CD or a read-only copy keep it)."""
    import stat
    try:
        os.chmod(path, stat.S_IWRITE | stat.S_IREAD)
    except OSError:
        pass


def replace_file(src, dst):
    """os.replace, also over a read-only file of the install (an update replaces it, as a player expects)."""
    try:
        os.replace(src, dst)
    except PermissionError:
        if not os.path.isfile(dst):
            raise
        writable(dst)
        os.replace(src, dst)


def remove_tree(folder):
    """Removes `folder` with everything in it, read-only files too (their mark cleared and the removal
    tried again). True when it is gone; False (the reason printed) when something of it stays."""
    def again(function, path, _):
        writable(path)
        function(path)
    if not os.path.exists(folder):
        return True
    try:
        if sys.version_info >= (3, 12):
            shutil.rmtree(folder, onexc=again)
        else:
            shutil.rmtree(folder, onerror=again)
        return True
    except OSError as e:
        print(f"{folder} could not be removed ({type(e).__name__}: {e}) - the next run of Setup tries again")
        return False


def same_file(a, b):
    """build.copy's test: the same size and date, or the same content."""
    import filecmp
    sa, sb = os.stat(a), os.stat(b)
    return sa.st_size == sb.st_size and (int(sa.st_mtime) == int(sb.st_mtime) or filecmp.cmp(a, b, shallow=False))


def move_in(target, staged, moves, folders=()):
    """Puts each staged file of `moves` (relative paths) at its place in the install folder with a
    rename - over a read-only file too; one moved before an interruption is skipped - and makes the
    `folders` the build made (logs\\ and any other, empty ones too). Returns how many it moved."""
    moved = 0
    for i, rel in enumerate(moves):
        if i == len(moves) // 2:
            test_stop("moving")
        src, dst = os.path.join(staged, rel), os.path.join(target, rel)
        if not os.path.isfile(src):
            continue
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        replace_file(src, dst)
        moved += 1
    for rel in folders:
        path = os.path.join(target, rel)
        if not os.path.exists(path):
            os.makedirs(path)
    return moved


def finish_interrupted(target, status):
    """What an earlier run left in the install folder's work folder: a journal (it was killed while
    moving its files in) - the rest is moved in, so the game is that version's whole again, unless
    another Setup has finished an install there since; no journal (killed while preparing, or a work
    folder that could not be removed) - the prepared files are removed, the game was not changed.
    `status["phase"]` is "moving" while the rest goes in (a failure there leaves a half put in game).
    Returns (the line for the log or None, the files it put in: the previous record's for the
    clean-up, as what an earlier version may have left)."""
    work = work_folder(target)
    if not os.path.isdir(work):
        return None, []
    try:
        with open(os.path.join(work, JOURNAL), encoding="utf-8") as f:
            journal = json.load(f)
    except (OSError, ValueError):
        journal = None
    put = []
    if journal and journal.get("record") == record_stamp(target):
        moves = journal.get("moves", [])
        status["phase"] = "moving"
        moved = move_in(target, os.path.join(work, journal.get("staged", "new")), moves, journal.get("folders", []))
        status["phase"] = "preparing"
        put = moves
        line = (f"An earlier run of Setup ({journal.get('version')}) stopped while putting its files in: "
                f"the other {moved} of its {len(moves)} files put in now")
    elif journal:
        line = ("An earlier run of Setup stopped while putting its files in, and another Setup has updated "
                "the install since: its files removed")
    else:
        line = ("An earlier run of Setup left its work folder (it stopped while preparing its files, or the "
                "folder could not be removed): removed (the game was not changed by it)")
    try:
        os.remove(os.path.join(work, JOURNAL))   # first: what stays of the folder is then only leftovers
    except OSError:
        pass
    remove_tree(work)
    return line, put


def new_stage(target):
    """A new folder for this run's build in the work folder: new-1, new-2 ... (one that an earlier run
    could not remove is left for the next run to try again)."""
    for n in range(1, 1000):
        staged = os.path.join(work_folder(target), f"new-{n}")
        if not os.path.exists(staged):
            os.makedirs(staged)
            return staged
    sys.exit(f"{work_folder(target)}: no room for a new work folder")


def commit(target, staged, ver, status):
    """Puts what the build prepared in `staged` into the install folder: the files that differ from
    the install's (the same test as build.copy's; the high scores of an install stay the player's, as
    build.py leaves them) and the folders it made, through the journal and renames. Then the work
    folder goes - when it cannot (a file held), the install is done all the same and the next run
    tries again."""
    moves, folders, same = [], [], 0
    for directory, subdirs, files in os.walk(staged):
        for d in subdirs:
            folders.append(os.path.relpath(os.path.join(directory, d), staged))
        for f in files:
            rel = os.path.relpath(os.path.join(directory, f), staged)
            dst = os.path.join(target, rel)
            if os.path.isfile(dst) and (build.player_data(rel) or same_file(os.path.join(directory, f), dst)):
                same += 1
            else:
                moves.append(rel)
    moves.sort()
    test_stop("before-journal")
    journal = os.path.join(work_folder(target), JOURNAL)
    write_json_safely(journal, {"version": ver, "record": record_stamp(target), "staged": os.path.basename(staged),
                                "moves": moves, "folders": sorted(folders)})
    status["phase"] = "moving"
    move_in(target, staged, moves, sorted(folders))
    os.remove(journal)
    status["phase"] = "moved"
    print(f"Put into {target}: {len(moves)} files new or changed, {same} left as they were (the same, or the player's)")
    remove_tree(work_folder(target))


def free_space_check(steam, target, lang):
    """Stops (in the window's language) when the install folder's disk has less free space than the
    game takes - the build is made whole in the work folder before it goes in: the size of the Steam
    folder's files and 64 MB."""
    need = 64 << 20
    for directory, _, files in os.walk(steam):
        for f in files:
            try:
                need += os.path.getsize(os.path.join(directory, f))
            except OSError:
                pass
    existing = os.path.abspath(target)
    while not os.path.isdir(existing) and os.path.dirname(existing) != existing:
        existing = os.path.dirname(existing)
    free = TEST_FREE_MB << 20 if TEST_FREE_MB is not None else shutil.disk_usage(existing).free
    drive = os.path.splitdrive(existing)[0] + os.sep if os.path.splitdrive(existing)[0] else existing
    print(f"Free space on {drive}: {free >> 20} MB, needed about {need >> 20} MB")
    if free < need:
        sys.exit(TEXTS[lang]["no_space"].format(drive=drive, need=need >> 20, free=free >> 20))


def install(steam, target, choices, ver, progress=None, skip_verify=False, status=None):
    """Assembles the game into `target`, writes the options (and the game's language when one is
    given), the shortcuts and the record of the install. `choices`: cheats, unlock, desktop,
    start_menu (bool), game_language (a code or None: keep the saved one) and lang (the language
    of the shortcuts' names). `skip_verify`: bypass Steam.exe version check and leave the Steam files
    our changes do not fit as they are. The game is built in the work folder and put in at the end
    (commit, above); `status["phase"]` says how far it got: "preparing" (the game as it was),
    "moving" (half put in, here or by finish_interrupted: the next run finishes it), "moved". Prints its log; stops with SystemExit
    and the reason. Returns the warnings of the files left so (steam_files_check)."""
    status = {} if status is None else status
    status["phase"] = "preparing"
    if not is_game(steam):
        sys.exit(f"No Locoland from Steam in {steam}")
    if inside(target, steam):
        sys.exit("The install folder must not be the Steam folder of the game or inside it")
    if not build.steam_exe_original(steam, skip_verify=skip_verify):
        if skip_verify:
            print(f"Warning: Steam.exe in {steam} does not match a known version (continuing with --skip-verify)")
        else:
            sys.exit(TEXTS[choices["lang"]]["modified_steam"].format(path=steam))
    # another exe (skip_verify): the game goes in as it is - none of our changes, which need
    # LocoFix and so the Steam exe (build.AS_IS); its warning instead of those of single files
    as_is = skip_verify and build.foreign_exe(steam)
    if as_is:
        warnings = [TEXTS[choices["lang"]]["as_is"]]
    else:
        stop, warnings = steam_files_check(steam, choices["lang"], skip_verify)
        if stop:
            sys.exit(stop)
    if game_running(target):
        sys.exit("Locoland is running from the install folder - close it first")
    locofix = os.path.join(BUNDLE, "LocoFix.dll") if BUNDLE else None
    missing = missing_own_files()
    if missing:   # taken before the build: stop at once, the game untouched (failure_text names it)
        raise FileNotFoundError(2, f"{len(missing)} files of Setup are missing", missing[0])
    os.makedirs(CAPTURED, exist_ok=True)
    print(f"OpenLocoland {ver}: {steam} -> {target}")
    if BUNDLE:
        print(f"Setup's modules: {MODULES_IN_MEMORY / 1e6:.1f} MB read into memory at the start"
              if MODULES_IN_MEMORY else "Setup's modules: read from Setup.exe when first used")
    line, put = finish_interrupted(target, status)
    if line:
        print(line)
    free_space_check(steam, target, choices["lang"])
    manifest = read_manifest(target)
    staged = new_stage(target)
    # the whole game into the work folder (nothing of the install is read or written meanwhile); the
    # clean-up of what earlier versions left runs on the install folder after the commit
    try:
        files = build.build(steam, staged, locofix=locofix, captured=CAPTURED, skip_verify=skip_verify,
                            progress=(lambda f: progress(0.95 * f)) if progress else None,
                            clean=choices.get("clean", True))
        test_stop("built")
        if game_running(target):   # started while the game was being built
            sys.exit("Locoland is running from the install folder - close it first")
    except BaseException:   # stopped before the commit: the game is as it was, the prepared files go
        remove_tree(work_folder(target))
        raise
    commit(target, staged, ver, status)
    build.WRITTEN.clear()   # what this build wrote, at its place in the install folder (build.clean_up)
    build.wrote(*(os.path.join(target, rel) for rel in files))
    if choices.get("clean", True):
        game = os.path.join(build.REPO, "game")
        ours = build.DGVOODOO + ["LocoFix.dll", build.OUTPUT_EXE] + os.listdir(game)
        # what the record lists and what an interrupted run put in (no record lists those yet)
        previous = sorted(set((manifest or {}).get("files", [])) | set(put))
        removed = build.clean_up(target, steam, previous, False, build.foreign_root_entries(steam), ours)
        print(f"Clean-up of {target}: {len(removed)} files of earlier versions removed")
    if as_is:   # LocoFix's settings: nothing reads them in a game installed as it is
        print("Options and game language: not written (installed as it is - no LocoFix to read them)")
    else:
        write_setting("Cheats", choices["cheats"])
        write_setting("UnlockMissions", choices["unlock"])
        print(f"Options: Cheats {int(choices['cheats'])}, UnlockMissions {int(choices['unlock'])}")
        if choices.get("game_language"):
            write_setting("Language", choices["game_language"])
            print(f"Game language: {choices['game_language']}")
    remove_shortcuts(manifest)
    shortcuts = make_shortcuts(target, choices, ver, editor=not as_is)
    test_stop("record")
    write_json_safely(os.path.join(target, MANIFEST),
                      {"version": ver, "steam": steam, "captured": CAPTURED, "shortcuts": shortcuts, "files": files})
    status["phase"] = "done"
    if choices.get("remember"):
        try:
            remember_install(target)
        except OSError as e:   # only the next run's starting folder: never a failed install
            print(f"The install's folder could not be remembered ({e})")
    for line in warnings:
        print(line)
    return warnings


def make_shortcuts(target, choices, ver, editor=True):
    """The game's shortcut and, with `editor` (LocoFix: not in a game installed as it is), the
    editor's in the folders chosen; returns their paths."""
    texts = TEXTS[choices["lang"]]
    exe = os.path.join(target, "Locoland.exe")   # actual exe name (for config/registry)
    made = []
    for folder in shortcut_folders(choices["desktop"], choices["start_menu"]):
        game = os.path.join(folder, texts["game_name"] + ".lnk")
        make_shortcut(game, exe, "", target, exe, APP + " " + ver)
        made.append(game)
        if editor:
            path = os.path.join(folder, texts["editor_name"] + ".lnk")
            make_shortcut(path, exe, EDITOR_ARGUMENTS, target, exe, texts["editor_desc"])
            made.append(path)
        print("Shortcuts: " + folder)
    return made


def own_file(e):
    """The file of Setup itself an error is about - the exe, or what its bootloader unpacked into the
    temporary folder (sys._MEIPASS) - or None."""
    exe = os.path.normcase(os.path.abspath(sys.executable))
    names = [getattr(e, "filename", None), getattr(e, "filename2", None), getattr(e, "path", None)]
    for name in names:
        if isinstance(name, str) and name and (os.path.normcase(os.path.abspath(name)) == exe
                                              or (BUNDLE and inside(name, BUNDLE))):
            return name
    # PyInstaller's own words when the exe is gone and a module has to be read from it
    if BUNDLE and isinstance(e, SystemExit) and sys.executable in str(e.code):
        return sys.executable
    if BUNDLE and isinstance(e, (OSError, ImportError)):   # an error without a name: is a file of Setup gone?
        return next(iter(missing_own_files()), None)
    return None


def failure_text(lang, e, target, status):
    """What a failed run says, in the window's language: the reason - in plain words when Setup could
    not read a file of its own (an antivirus took Setup.exe or what it unpacked) - and the state of the
    game: as it was (stopped before the commit) or half put in (the next run finishes it)."""
    texts = TEXTS[lang]
    own = own_file(e)
    technical = str(e.code) if isinstance(e, SystemExit) else f"{type(e).__name__}: {e}"
    reason = texts["damaged"].format(file=own) + f"\n({technical})" if own else technical
    phase = (status or {}).get("phase")
    if phase in (None, "preparing"):
        reason += "\n\n" + texts["kept"].format(path=target)
    elif phase == "moving":
        reason += "\n\n" + texts["resume"].format(path=target)
    return reason


# ---- the window ----

class Tooltip:
    """A hint for a widget: after the mouse has rested on it for a moment, a small window with
    `text()` (its current language) next to the pointer, kept inside the screen; gone when the mouse
    leaves, a button is pressed or the widget is used by the keyboard."""
    def __init__(self, widget, text, wrap):
        self.widget, self.text, self.wrap = widget, text, wrap
        self.window = None
        self.pending = None
        widget.bind("<Enter>", self.schedule, add="+")
        widget.bind("<Leave>", self.hide, add="+")
        widget.bind("<ButtonPress>", self.hide, add="+")
        widget.bind("<KeyPress>", self.hide, add="+")

    def schedule(self, _=None):
        self.hide()
        self.pending = self.widget.after(600, self.show)

    def show(self):
        self.pending = None
        text = self.text()
        if not text or self.window:
            return
        w = self.window = tk.Toplevel(self.widget)
        w.wm_overrideredirect(True)
        w.attributes("-topmost", True)
        tk.Label(w, text=text, justify="left", wraplength=self.wrap, background="#ffffe1", foreground="black",
                 relief="solid", borderwidth=1, padx=6, pady=4).pack()
        w.update_idletasks()
        x, y = self.widget.winfo_pointerx() + 12, self.widget.winfo_pointery() + 18
        sw, sh = w.winfo_screenwidth(), w.winfo_screenheight()
        width, height = w.winfo_reqwidth(), w.winfo_reqheight()
        x = max(0, min(x, sw - width - 4))
        if y + height > sh - 4:   # no room under the pointer: above it
            y = max(0, self.widget.winfo_pointery() - height - 8)
        w.wm_geometry(f"+{x}+{y}")

    def hide(self, _=None):
        if self.pending:
            self.widget.after_cancel(self.pending)
            self.pending = None
        if self.window:
            self.window.destroy()
            self.window = None


class LogWriter:
    """build.py prints its log: the lines go to the window through the queue."""
    def __init__(self, events):
        self.events = events

    def write(self, text):
        if text:
            self.events.put(("log", text))

    def flush(self):
        pass


class Installer:
    def __init__(self, root, steam=None, target=None, auto=False, shortcuts=True, clean=True):
        self.root = root
        self.scale = max(1.0, root.winfo_fpixels("1i") / 96)   # 1.5 at a scale of 150 %
        self.lang = first_ui_language()
        self.version = version()
        self.events = queue.Queue()
        self.busy = False
        self.auto = auto   # --auto: install at once, no questions, close at the end
        self.clean = clean   # False: --no-clean
        self.result = None
        self.widgets = {}
        self.logo = None
        self.banner = False        # our wordmark as the header, scaled to the window's width
        self.banner_width = 0
        self.buildable, self.missing = [], []
        self.first_language = None   # the game language shown first: written only when changed
        self.steam = tk.StringVar(value=steam or find_game() or "")
        self.target = tk.StringVar(value=target or DEFAULT_TARGET)
        self.cheats = tk.BooleanVar(value=read_option("Cheats"))
        self.unlock = tk.BooleanVar(value=read_option("UnlockMissions"))
        self.skip_verify = tk.BooleanVar(value=False)
        self.desktop = tk.BooleanVar(value=shortcuts)
        self.start_menu = tk.BooleanVar(value=shortcuts)
        self.ui_language = tk.StringVar(value=TEXTS[self.lang]["name"])
        self.game_language = tk.StringVar()        # the list's text: the chosen pack's name
        self.game_language_code = tk.StringVar()   # its code
        self.layout()
        self.game_language_code.trace_add("write", lambda *_: self.show_game_language())
        self.target.trace_add("write", lambda *_: self.target_changed())
        self.steam.trace_add("write", lambda *_: self.steam_changed())
        self.skip_verify.trace_add("write", lambda *_: self.steam_changed())
        self.steam_changed()
        self.root.after(100, self.poll)
        if auto:
            self.root.after(1000, self.install)

    def px(self, size):
        """A size in pixels at the screen's scale: Setup is DPI aware (its manifest), Tk scales the fonts
        (points) itself, the sizes given in pixels are scaled here (1.0 at 100 %)."""
        return round(size * self.scale)

    def t(self, key, **values):
        return TEXTS[self.lang][key].format(version=self.version, **values)

    def open_github(self):
        import webbrowser
        webbrowser.open("https://github.com/Dysprohol/OpenLocoland")

    def layout(self):
        r = self.root
        r.resizable(True, True)
        w = self.widgets
        # the header: our wordmark across the whole width, else the game's logo, else the name
        # (a canvas, so the picture drawn for the current width does not fix the window's size)
        w["header"] = tk.Canvas(r, height=self.px(84), background="black", bd=0, highlightthickness=0)
        w["header"].grid(row=0, column=0, sticky="ew")
        w["header"].bind("<Configure>", self.header_resized)
        self.header_item = w["header"].create_text(0, 42, text=APP, font=("Segoe UI", 18, "bold"),
                                                   fill="white", anchor="center")
        frame = ttk.Frame(r, padding=12)
        frame.grid(row=1, column=0, sticky="nsew")
        r.columnconfigure(0, weight=1)
        r.rowconfigure(1, weight=1)
        frame.columnconfigure(1, weight=1)

        top = ttk.Frame(frame)
        top.grid(row=0, column=0, columnspan=3, sticky="ew")
        top.columnconfigure(0, weight=1)
        w["intro"] = ttk.Label(top, wraplength=self.px(520), justify="left")
        w["intro"].grid(row=0, column=0, sticky="w")
        w["language"] = ttk.Label(top)
        w["language"].grid(row=0, column=1, sticky="e", padx=(12, 4))
        choice = ttk.Combobox(top, textvariable=self.ui_language, values=[n for _, n in UI_LANGUAGES],
                              state="readonly", width=10)
        choice.grid(row=0, column=2, sticky="e")
        choice.bind("<<ComboboxSelected>>", lambda e: self.switch_language())
        # GitHub link
        w["github"] = ttk.Label(top, text=self.t("github"), cursor="hand2", foreground="#0366d6")
        w["github"].grid(row=0, column=3, sticky="e", padx=(4, 0))
        w["github"].bind("<Button-1>", lambda e: self.open_github())

        for row, (key, var, command) in enumerate([("steam", self.steam, self.browse_steam),
                                                   ("target", self.target, self.browse_target)], start=1):
            pad = (10 if row == 1 else 4, 0)
            w[key] = ttk.Label(frame)
            w[key].grid(row=row, column=0, sticky="w", pady=pad)
            ttk.Entry(frame, textvariable=var, width=60).grid(row=row, column=1, sticky="ew", padx=6, pady=pad)
            w["browse_" + key] = ttk.Button(frame, command=command)
            w["browse_" + key].grid(row=row, column=2, pady=pad)

        w["game_language"] = ttk.Label(frame)
        w["game_language"].grid(row=3, column=0, sticky="w", pady=(4, 0))
        row = ttk.Frame(frame)
        row.grid(row=3, column=1, columnspan=2, sticky="ew", padx=6, pady=(4, 0))
        # a menu, not a combobox: the languages that cannot be built are listed greyed, with why
        w["game_language_choice"] = ttk.Menubutton(row, textvariable=self.game_language, width=24)
        w["game_language_menu"] = tk.Menu(w["game_language_choice"], tearoff=False)
        w["game_language_choice"]["menu"] = w["game_language_menu"]
        w["game_language_choice"].grid(row=0, column=0, sticky="w")
        w["more_languages"] = ttk.Label(row)
        w["more_languages"].grid(row=0, column=1, sticky="w", padx=(10, 0))

        w["options"] = ttk.LabelFrame(frame, padding=6)
        w["options"].grid(row=4, column=0, columnspan=3, sticky="ew", pady=(10, 0))
        w["cheats"] = ttk.Checkbutton(w["options"], variable=self.cheats)
        w["cheats"].grid(row=0, column=0, sticky="w")
        w["unlock"] = ttk.Checkbutton(w["options"], variable=self.unlock)
        w["unlock"].grid(row=1, column=0, sticky="w")
        w["skip_verify"] = ttk.Checkbutton(w["options"], variable=self.skip_verify)
        w["skip_verify"].grid(row=2, column=0, sticky="w")
        # what the box gives up, before the install: a hint under the mouse (and the log's first lines
        # while it is ticked, show_language_note - the window does not grow for it)
        self.skip_hint = Tooltip(w["skip_verify"], lambda: self.t("skip_verify_hint"), self.px(380))

        w["shortcuts"] = ttk.LabelFrame(frame, padding=6)
        w["shortcuts"].grid(row=5, column=0, columnspan=3, sticky="ew", pady=(6, 0))
        w["desktop"] = ttk.Checkbutton(w["shortcuts"], variable=self.desktop)
        w["desktop"].grid(row=0, column=0, sticky="w", padx=(0, 20))
        w["start_menu"] = ttk.Checkbutton(w["shortcuts"], variable=self.start_menu)
        w["start_menu"].grid(row=0, column=1, sticky="w")

        self.progress = ttk.Progressbar(frame, maximum=1000)
        self.progress.grid(row=6, column=0, columnspan=3, sticky="ew", pady=(10, 4))
        self.log = ScrolledText(frame, height=10, width=90, state="disabled", font=("Consolas", 9), takefocus=0,
                              wrap="word")
        self.log.grid(row=7, column=0, columnspan=3, sticky="nsew")
        frame.rowconfigure(7, weight=1)

        buttons = ttk.Frame(frame)
        buttons.grid(row=8, column=0, columnspan=3, sticky="e", pady=(10, 0))
        for i, (key, command) in enumerate([("install", self.install), ("uninstall", self.uninstall),
                                            ("run", self.run_game), ("close", self.root.destroy)]):
            w["button_" + key] = ttk.Button(buttons, command=command)
            w["button_" + key].grid(row=0, column=i, padx=(6, 0))
        w["button_install"].focus_set()   # Space or Enter installs
        r.bind("<Return>", lambda e: e.widget.invoke() if isinstance(e.widget, ttk.Button) else None)
        self.apply_texts()

    def apply_texts(self):
        w = self.widgets
        self.root.title(self.t("title"))
        for key in ["intro", "language", "steam", "target", "game_language", "options", "cheats", "unlock",
                    "skip_verify", "shortcuts", "desktop", "start_menu"]:
            w[key].configure(text=self.t(key))
        w["more_languages"].configure(text=self.t("more_languages") if self.missing else "")
        w["browse_steam"].configure(text=self.t("browse"))
        w["browse_target"].configure(text=self.t("browse"))
        for key in ["uninstall", "run", "close"]:
            w["button_" + key].configure(text=self.t(key))
        self.fill_language_menu()
        self.refresh()
        self.show_language_note()

    def switch_language(self):
        self.lang = next(c for c, n in UI_LANGUAGES if n == self.ui_language.get())
        self.apply_texts()

    def header_resized(self, event):
        """Our banner follows the width of the window; the rest of the header stays centred."""
        if self.banner:
            self.show_banner(event.width)
        else:
            self.widgets["header"].coords(self.header_item, event.width // 2, event.height // 2)

    def show_banner(self, width):
        if width < 64 or width == self.banner_width:
            return
        image = logo_banner(width)
        if not image:
            return
        header = self.widgets["header"]
        self.banner_width = width
        self.logo = image
        if self.header_item is not None:
            header.delete(self.header_item)
        header.configure(height=image.height())
        self.header_item = header.create_image(0, 0, image=image, anchor="nw")

    def show_header_image(self, image):
        header = self.widgets["header"]
        self.logo = image
        if self.header_item is not None:
            header.delete(self.header_item)
        header.configure(height=max(image.height(), self.px(84)))
        self.header_item = header.create_image(0, 0, image=image, anchor="center")
        header.coords(self.header_item, header.winfo_width() // 2, header.winfo_height() // 2)

    def show_header_text(self):
        header = self.widgets["header"]
        if self.header_item is not None:
            header.delete(self.header_item)
        header.configure(height=self.px(84))
        self.header_item = header.create_text(0, 0, text=APP, font=("Segoe UI", 18, "bold"),
                                              fill="white", anchor="center")
        header.coords(self.header_item, header.winfo_width() // 2, header.winfo_height() // 2)

    def steam_changed(self):
        """The header's logo (ours, else the game's) and the game languages."""
        steam = self.steam.get().strip()
        self.banner = custom_logo_file() is not None
        if self.banner:
            self.banner_width = 0
            self.show_banner(self.widgets["header"].winfo_width())
        else:
            image = logo_image(steam, self.px(56))
            if image:
                self.show_header_image(image)
            else:
                self.show_header_text()
        self.buildable, self.missing, self.have = game_languages(steam, skip_verify=self.skip_verify.get())
        self.target_changed()
        self.apply_texts()

    def target_changed(self):
        """An install there or not: the game language shown first is the saved one or English."""
        target = self.target.get().strip()
        self.first_language = first_game_language(self.buildable, bool(target) and read_manifest(target) is not None)
        self.game_language_code.set(self.first_language or "")
        self.refresh()

    def language_label(self, code):
        return pack_name(code) + (" " + self.t("unofficial") if is_fan_pack(code, self.have) else "")

    def language_reason(self, code):
        """Why a pack cannot be built, and what to do about it."""
        if self.skip_verify.get() or not depot_line(code):
            return self.t("not_found")
        if is_fan_pack(code, self.have):   # a translation that needs its own base (needs_base=1) or no capture at all
            return self.t("needs_files", base=pack_name(pack_base(code)), line=depot_line(code))
        return self.t("needs_depot", line=depot_line(code))

    def fill_language_menu(self):
        """Every pack of lang\\: the ones that can be built to choose, the others greyed with the reason."""
        menu = self.widgets["game_language_menu"]
        menu.delete(0, "end")
        for code in sorted(self.buildable + self.missing):
            can = code in self.buildable
            menu.add_radiobutton(label=self.language_label(code), variable=self.game_language_code, value=code,
                                 state="normal" if can else "disabled",
                                 accelerator="" if can else self.language_reason(code))
        self.show_game_language()

    def show_game_language(self):
        code = self.game_language_code.get()
        self.game_language.set(self.language_label(code) if code else "")

    def show_language_note(self):
        """The log starts with what the box "install as it is" gives up while it is ticked, then how to get
        the game's other languages from Steam."""
        lines = [self.t("skip_verify_hint"), ""] if self.skip_verify.get() else []
        if not self.buildable and is_game(self.steam.get().strip()):
            lines.append(self.t("no_language"))
        if self.missing:
            if self.skip_verify.get():
                lines += [self.t("depots_local"), ""] + [f"  {self.language_label(c)}" for c in self.missing]
            else:
                lines += [self.t("depots"), ""] + depot_lines(self.missing)
        if not self.busy and self.progress["value"] == 0:
            self.log.configure(state="normal")
            self.log.delete("1.0", "end")
            self.log.insert("end", "\n".join(lines) + ("\n\n" if lines else ""))
            self.log.configure(state="disabled")

    def chosen_game_language(self):
        code = self.game_language_code.get()
        return code if code in self.buildable else None

    def refresh(self):
        """Install or Update, and whether Uninstall and Play can be used, by the install folder."""
        target = self.target.get().strip()
        installed = bool(target) and read_manifest(target) is not None
        w = self.widgets
        w["button_install"].configure(text=self.t("update" if installed else "install"),
                                      state="disabled" if self.busy else "normal")
        w["button_uninstall"].configure(state="normal" if installed and not self.busy else "disabled")
        w["button_run"].configure(state="normal" if target and os.path.isfile(os.path.join(target, "Locoland.exe"))
                                  and installed and not self.busy else "disabled")

    def browse_steam(self):
        folder = filedialog.askdirectory(initialdir=self.steam.get() or None, mustexist=True)
        if folder:
            self.steam.set(os.path.normpath(folder))

    def browse_target(self):
        folder = filedialog.askdirectory(initialdir=os.path.dirname(self.target.get()) or None)
        if folder:
            folder = os.path.normpath(folder)
            if not os.path.basename(folder).lower().startswith(APP.lower()) and os.path.isdir(folder) \
                    and os.listdir(folder) and read_manifest(folder) is None:
                folder = os.path.join(folder, APP)   # a parent folder was picked: a folder of its own in it
            self.target.set(folder)

    def say(self, text):
        self.log.configure(state="normal")
        self.log.insert("end", text)
        self.log.see("end")
        self.log.configure(state="disabled")

    # ---- install ----

    def error(self, text):
        if self.auto:
            self.finish(False, text)
        else:
            messagebox.showerror(APP, text)

    def finish(self, ok, text):
        """--auto: the window's log into logs\\install.log of the install folder, then close."""
        self.result = ok
        target = self.target.get().strip()
        if os.path.isdir(target):
            os.makedirs(os.path.join(target, build.LOGS), exist_ok=True)
            with open(os.path.join(target, LOG), "w", encoding="utf-8") as f:
                f.write(self.log.get("1.0", "end") + ("OK" if ok else "FAILED: " + text) + "\n")
        self.root.destroy()

    def install(self):
        steam, target = self.steam.get().strip(), self.target.get().strip()
        if not is_game(steam):
            self.error(self.t("bad_steam", path=steam) if steam else self.t("no_steam"))
            return
        if not target or inside(target, steam):
            self.error(self.t("inside_steam"))
            return
        steam_exe = os.path.join(steam, "Steam.exe")
        locoland_exe = os.path.join(steam, "Locoland.exe")
        has_steam = os.path.isfile(steam_exe)
        has_locoland = os.path.isfile(locoland_exe)
        skip_verify = self.skip_verify.get()
        if has_steam and not build.steam_exe_original(steam, skip_verify=skip_verify):
            if skip_verify:
                self.say(f"Warning: Steam.exe in {steam} does not match a known version (continuing with skip verification)\n")
            else:
                self.error(self.t("modified_steam", path=steam))
                return
        if has_locoland and has_steam:
            self.error(self.t("modified_steam", path=steam) + " (both Steam.exe and Locoland.exe present)")
            return
        stop, _ = steam_files_check(steam, self.lang, skip_verify)
        if stop:
            self.error(stop)
            return
        target = os.path.abspath(target)
        update = read_manifest(target) is not None
        unfinished = os.path.isdir(work_folder(target))   # a run that stopped: ours, finished by this one
        if os.path.isdir(target) and os.listdir(target) and not update and not unfinished and not self.auto:
            if not messagebox.askyesno(APP, self.t("not_empty", path=target)):
                return
        if game_running(target):
            self.error(self.t("running"))
            return
        self.target.set(target)
        self.busy = True
        self.refresh()
        self.progress["value"] = 0
        self.say(self.t("working") + "\n")
        language = self.chosen_game_language()
        choices = {"cheats": self.cheats.get(), "unlock": self.unlock.get(), "desktop": self.desktop.get(), "clean": self.clean,
                   # the window remembers its install for the next run; --auto (tests) only into a --state-folder
                   "remember": not self.auto or STATE_GIVEN,
                   "start_menu": self.start_menu.get(), "lang": self.lang, "skip_verify": self.skip_verify.get(),
                   # a new install sets the game's language; an update only when it was changed here
                   "game_language": language if not update or language != self.first_language else None}
        threading.Thread(target=self.work, args=(steam, target, choices), daemon=True).start()

    def work(self, steam, target, choices):
        out, err = sys.stdout, sys.stderr
        sys.stdout = sys.stderr = LogWriter(self.events)
        status = {}
        try:
            warnings = install(steam, target, choices, self.version,
                               lambda f: self.events.put(("progress", f)),
                               skip_verify=choices.get("skip_verify", False), status=status)
            self.events.put(("done", "\n\n".join([self.t("done", path=target)] + warnings)))
        except (SystemExit, Exception) as e:   # anything: shown in words, not lost in a closed console
            self.events.put(("failed", failure_text(self.lang, e, target, status)))
        finally:
            sys.stdout, sys.stderr = out, err

    def uninstall(self):
        target = os.path.abspath(self.target.get().strip())
        manifest = read_manifest(target)
        if manifest is None or inside(target, self.steam.get().strip() or "\0"):
            return
        if not messagebox.askyesno(APP, self.t("uninstall_ask", path=target, captured=CAPTURED), icon="warning"):
            return
        if game_running(target):
            messagebox.showerror(APP, self.t("running"))
            return
        remove_shortcuts(manifest)
        shutil.rmtree(target, ignore_errors=True)
        forget_install(target)
        if CAPTURED == DEFAULT_CAPTURED:   # a folder given with --captured stays
            remove_setup_state(os.path.dirname(CAPTURED))
        self.say(self.t("uninstalled", path=target) + "\n")
        self.refresh()

    def run_game(self):
        target = self.target.get().strip()
        os.startfile(os.path.join(target, "Locoland.exe"), cwd=target)   # as a double-click starts it

    def poll(self):
        try:
            while True:
                kind, value = self.events.get_nowait()
                if kind == "log":
                    self.say(value)
                elif kind == "progress":
                    self.progress["value"] = int(value * 1000)
                else:
                    self.busy = False
                    if kind == "done":
                        self.progress["value"] = 1000
                        self.say(value + "\n")
                        if self.auto:
                            self.finish(True, value)
                            return
                        messagebox.showinfo(APP, value)
                    else:
                        self.say(self.t("failed", error=value) + "\n")
                        if self.auto:
                            self.finish(False, value)
                            return
                        messagebox.showerror(APP, self.t("failed", error=value))
                    self.refresh()
        except queue.Empty:
            pass
        self.root.after(100, self.poll)


def silent(a):
    """--silent: the install without the window, its log in <target>\\logs\\install.log."""
    target = os.path.abspath(a.target or DEFAULT_TARGET)
    os.makedirs(os.path.join(target, build.LOGS), exist_ok=True)
    steam = a.steam or find_game() or ""
    update = read_manifest(target) is not None
    skip_verify = getattr(a, "skip_verify", False)
    buildable, missing, _ = game_languages(steam, skip_verify=skip_verify)
    language = a.game_lang or (None if update else first_game_language(buildable, False))
    choices = {"cheats": a.cheats == "1", "unlock": a.unlock == "1", "desktop": not a.no_shortcuts, "clean": not a.no_clean,
               "start_menu": not a.no_shortcuts, "lang": a.lang or first_ui_language(), "game_language": language, "skip_verify": skip_verify}
    status = {}
    # line by line, so a run that is killed leaves its log up to that point
    with open(os.path.join(target, LOG), "w", encoding="utf-8", buffering=1) as log:
        sys.stdout = sys.stderr = log
        try:
            if a.game_lang and a.game_lang not in buildable:
                sys.exit(f"Game language {a.game_lang} cannot be built (it can: {', '.join(buildable) or 'none'})")
            if missing:
                if skip_verify:
                    print("Not available yet (no Steam depots in skip-verify mode; ensure language files are in the game folder):")
                else:
                    print("Not available yet (download_depot in the Steam console, then run Setup again):")
                    print("\n".join(depot_lines(missing)))
            install(steam, target, choices, version(), skip_verify=getattr(a, "skip_verify", False), status=status)
        except (SystemExit, Exception) as e:   # the log is all a silent install has
            print("FAILED: " + failure_text(choices["lang"], e, target, status))
            return 1
        print("OK")
    return 0


def message_box(text, error=False):
    import ctypes
    ctypes.windll.user32.MessageBoxW(None, text, APP + " Setup", 0x10 if error else 0x40)   # MB_ICONERROR / INFORMATION


def arguments_parser():
    """argparse, whose --help and errors go into a message box when Setup has no console to print
    them into (Setup.exe is a windowed program: they were lost)."""
    import argparse

    class Parser(argparse.ArgumentParser):
        def _print_message(self, message, file=None):
            if not message:
                return
            stream = file or sys.stderr
            if stream is not None:
                try:
                    stream.write(message)
                    return
                except (AttributeError, OSError, ValueError):
                    pass
            message_box(message)

        def error(self, message):
            text = f"{self.format_usage()}\n{self.prog}: error: {message}\n"
            if sys.stderr is not None:
                try:
                    sys.stderr.write(text)
                    sys.exit(2)
                except (AttributeError, OSError, ValueError):
                    pass
            message_box(text, error=True)
            sys.exit(2)

    # in a message box: lines short enough for its width (argparse wraps for an 80 column console)
    narrow = (lambda prog: argparse.HelpFormatter(prog, width=58, max_help_position=20)) if sys.stderr is None else None
    return Parser(prog="Setup.exe", description=APP + " installer", **({"formatter_class": narrow} if narrow else {}))


def window_target(a):
    """The folder the window opens on: --target; without it the existing install (find_install) - but
    --auto, a test of the window, keeps the default folder as --silent does. (folder or None, how)."""
    if a.target:
        return a.target, "--target"
    if a.auto:
        return None, "the default (--auto)"
    return find_install()


def main():
    import argparse
    codes = [c for c, _ in UI_LANGUAGES]
    p = arguments_parser()
    p.add_argument("--steam", help="the Steam folder of Locoland (default: found through Steam)")
    p.add_argument("--target", help="the install folder (default: " + DEFAULT_TARGET + ")")
    p.add_argument("--silent", action="store_true", help="install without the window")
    p.add_argument("--auto", action="store_true",
                   help="the window installs at once and closes at the end (tests of the window)")
    p.add_argument("--cheats", choices=["0", "1"], help="the option (default: as it is in the registry)")
    p.add_argument("--unlock", choices=["0", "1"], help="the option (default: as it is in the registry)")
    p.add_argument("--game-lang", help="the game's language (default for a new install: the system's if there is a pack, else English; kept on an update)")
    p.add_argument("--no-shortcuts", action="store_true", help="no shortcuts (the window: both boxes unticked)")
    p.add_argument("--lang", choices=codes, help="the language of the shortcuts' names (default: the system's, else en)")
    p.add_argument("--no-clean", action="store_true",
                   help="remove nothing that earlier versions left in the install folder")
    p.add_argument("--skip-verify", action="store_true",
                   help="go on with a Steam.exe of another version (CD/beta-demo) and leave the Steam files our changes do not fit as they are, with a warning")
    p.add_argument("--captured", help="the folder of the language captures (default: " + DEFAULT_CAPTURED +
                   "; tests: a folder with only some languages)")
    # tests: the shortcuts go into <folder>\Desktop and <folder>\Programs\OpenLocoland, never the real ones
    p.add_argument("--shortcut-root", help=argparse.SUPPRESS)
    # tests: the process ends hard at that point of the install, as a kill there would end it
    p.add_argument("--test-stop", choices=["built", "before-journal", "moving", "record"], help=argparse.SUPPRESS)
    p.add_argument("--test-free-mb", type=int, help=argparse.SUPPRESS)   # tests: the free space the check sees
    # tests: Setup's own state (the remembered install) in that folder instead of %LOCALAPPDATA%\OpenLocoland
    p.add_argument("--state-folder", help=argparse.SUPPRESS)
    a = p.parse_args()
    if a.shortcut_root:
        global SHORTCUT_ROOT
        SHORTCUT_ROOT = os.path.abspath(a.shortcut_root)
    if a.test_stop:
        global TEST_STOP
        TEST_STOP = a.test_stop
    if a.test_free_mb is not None:
        global TEST_FREE_MB
        TEST_FREE_MB = a.test_free_mb
    if a.state_folder:
        global STATE_FOLDER, STATE_GIVEN
        STATE_FOLDER, STATE_GIVEN = os.path.abspath(a.state_folder), True
    if a.captured:
        global CAPTURED
        CAPTURED = languages.CAPTURED = os.path.abspath(a.captured)
    if a.silent:
        a.cheats = a.cheats or ("1" if read_option("Cheats") else "0")
        a.unlock = a.unlock or ("1" if read_option("UnlockMissions") else "0")
        sys.exit(silent(a))
    root = tk.Tk()
    icon = os.path.join(BUNDLE or os.path.join(os.path.dirname(os.path.abspath(__file__)), "icon"), "openlocoland.ico")
    if os.path.exists(icon):
        root.iconbitmap(default=icon)
    window = Installer(root, a.steam, window_target(a)[0], a.auto, not a.no_shortcuts, not a.no_clean)
    root.mainloop()
    if a.auto:
        sys.exit(0 if window.result else 1)


if __name__ == "__main__":
    main()
