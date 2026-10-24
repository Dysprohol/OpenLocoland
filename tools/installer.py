r"""The installer of OpenLocoland: a window around build.py, frozen into Setup.exe by
tools/release.py (PyInstaller, one file).

    python tools\installer.py      (the same window from the repository, for trying it)
    Setup.exe [--steam <folder>] [--target <folder>]      (the window with these folders)
    Setup.exe --silent [--steam <folder>] [--target <folder>] [--cheats 0|1] [--unlock 0|1]
              [--game-lang <code>] [--no-shortcuts] [--lang <code>]
        installs without the window: the log goes to install.log in the install folder and
        the exit code is 0 when it worked (scripted installs, tests)
    Setup.exe --auto [--steam <folder>] [--target <folder>]
        the window, which installs at once with what it shows and closes at the end, its log
        in install.log (tests of the window itself)

It finds the player's Steam copy of Locoland (Steam's registry key, its libraries in
steamapps\libraryfolders.vdf, or a folder chosen by hand), proposes an install folder of its
own (%USERPROFILE%\Games\OpenLocoland - never the Steam folder, which is only read) and runs
build.build() into it with the prebuilt LocoFix.dll packed in Setup.exe: nothing of the game
is in the installer, the game is assembled from the player's files on their machine - the
logo of the window's header too, read from the Steam copy once it is found. The language
captures go to %LOCALAPPDATA%\OpenLocoland\captured; every language Steam has (the installed
one and the depots downloaded next to it) is captured, and the game languages offered are those
with a capture and a pack in lang\<code>\.

The window is in English unless another of its languages is picked; a new install sets the
game's language to English (when that pack can be built) or the one picked. Running it again
over an install updates it: build.py writes only our files and the game's originals, so the
player's high scores, maps and lines of maps\custom.lst stay, and the progress in the registry
is not touched - the two options are written with the values shown (the current ones of the
registry), and the game's language only when it was changed in the window. OpenLocoland.json in
the install folder records the version and the shortcuts; Uninstall removes the shortcuts, the
folder and the captures after a question, and leaves the registry (the progress and the
profiles are the original game's too).
"""
import json
import os
import queue
import shutil
import subprocess
import sys
import threading
import tkinter as tk
import winreg
from tkinter import filedialog, messagebox, ttk
from tkinter.scrolledtext import ScrolledText

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build  # noqa: E402
import languages  # noqa: E402

BUNDLE = getattr(sys, "_MEIPASS", None)
REPO = build.REPO
APP = "OpenLocoland"
MANIFEST = "OpenLocoland.json"
SETTINGS_KEY = r"SOFTWARE\Gromada\Locoland"   # RegPath of Locoland.cfg: LocoFix's settings too
DEFAULT_TARGET = os.path.join(os.environ.get("USERPROFILE", os.path.expanduser("~")), "Games", APP)
CAPTURED = os.path.join(os.environ.get("LOCALAPPDATA", os.path.expanduser("~")), APP, "captured")
EDITOR_MAP = r"maps\template\template.map"   # what the menu's "EDYTOR MAP" opens
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
        "no_language": "No language of the game can be built - is Locoland installed through Steam?",
        "options": "Options (they can be changed later in the game's launcher window)",
        "cheats": "Cheats (developer keys: h, W, B, A, K, M, #, x)",
        "unlock": "All missions unlocked (your progress stays as it is)",
        "shortcuts": "Shortcuts",
        "desktop": "On the desktop",
        "start_menu": "In the Start menu",
        "install": "Install",
        "update": "Update",
        "uninstall": "Uninstall",
        "run": "Play",
        "close": "Close",
        "no_steam": "Locoland was not found in Steam. Install it through Steam or choose its folder "
                    "(the one with Steam.exe).",
        "bad_steam": "This folder has no Locoland from Steam (Steam.exe is missing):\n{path}",
        "inside_steam": "The install folder must not be the Steam folder of the game or inside it.",
        "modified_steam": "Steam.exe in this folder is not the original file of the game - it was changed or replaced (e.g. by a resolution fix):\n{path}\n\nRestore the original in Steam: right-click Locoland > Properties > Installed Files > Verify integrity of game files. Then run Setup again.",
        "modified_files": "Your Locoland files in Steam are modified ({files}):\n{path}\n\nRestore them in Steam: right-click Locoland > Properties > Installed Files > Verify integrity of game files. Then run Setup again. OpenLocoland has its own option for the developer keys: Cheats, below.",
        "not_empty": "The folder is not empty and has no OpenLocoland install:\n{path}\n\n"
                     "Install there anyway? Its other files are left alone.",
        "running": "Locoland is running from this folder. Close the game first.",
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
        "options": "Optionen (später im Startfenster des Spiels änderbar)",
        "cheats": "Cheats (Entwicklertasten: h, W, B, A, K, M, #, x)",
        "unlock": "Alle Missionen freigeschaltet (dein Fortschritt bleibt erhalten)",
        "shortcuts": "Verknüpfungen",
        "desktop": "Auf dem Desktop",
        "start_menu": "Im Startmenü",
        "install": "Installieren",
        "update": "Aktualisieren",
        "uninstall": "Deinstallieren",
        "run": "Spielen",
        "close": "Schließen",
        "no_steam": "Locoland wurde in Steam nicht gefunden. Installiere es über Steam oder wähle seinen "
                    "Ordner (den mit Steam.exe).",
        "bad_steam": "In diesem Ordner ist kein Locoland aus Steam (Steam.exe fehlt):\n{path}",
        "inside_steam": "Der Installationsordner darf nicht der Steam-Ordner des Spiels sein oder darin liegen.",
        "modified_steam": "Die Steam.exe in diesem Ordner ist nicht die Originaldatei des Spiels - sie wurde geändert oder ersetzt (z. B. durch einen Auflösungs-Fix):\n{path}\n\nStelle das Original in Steam wieder her: Rechtsklick auf Locoland > Eigenschaften > Installierte Dateien > Dateien auf Fehler überprüfen. Dann Setup erneut starten.",
        "modified_files": "Deine Locoland-Dateien in Steam sind verändert ({files}):\n{path}\n\nStelle sie in Steam wieder her: Rechtsklick auf Locoland > Eigenschaften > Installierte Dateien > Dateien auf Fehler überprüfen. Dann Setup erneut starten. OpenLocoland hat für die Entwicklertasten eine eigene Option: Cheats, unten.",
        "not_empty": "Der Ordner ist nicht leer und enthält keine OpenLocoland-Installation:\n{path}\n\n"
                     "Trotzdem dort installieren? Seine anderen Dateien bleiben unberührt.",
        "running": "Locoland läuft aus diesem Ordner. Beende zuerst das Spiel.",
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
        "options": "Beállítások (később a játék indítóablakában is módosíthatók)",
        "cheats": "Csalások (fejlesztői billentyűk: h, W, B, A, K, M, #, x)",
        "unlock": "Minden küldetés elérhető (az előrehaladásod nem változik)",
        "shortcuts": "Parancsikonok",
        "desktop": "Az asztalon",
        "start_menu": "A Start menüben",
        "install": "Telepítés",
        "update": "Frissítés",
        "uninstall": "Eltávolítás",
        "run": "Játék",
        "close": "Bezárás",
        "no_steam": "A Locoland nem található a Steamben. Telepítsd a Steamen keresztül, vagy válaszd ki a "
                    "mappáját (amelyikben a Steam.exe van).",
        "bad_steam": "Ebben a mappában nincs Steames Locoland (hiányzik a Steam.exe):\n{path}",
        "inside_steam": "A telepítési mappa nem lehet a játék Steam-mappája, és nem lehet azon belül.",
        "modified_steam": "Ebben a mappában a Steam.exe nem a játék eredeti fájlja - módosították vagy kicserélték (pl. egy felbontásjavítás):\n{path}\n\nÁllítsd vissza az eredetit a Steamben: jobb klikk a Locolandon > Tulajdonságok > Telepített fájlok > Játékfájlok sértetlenségének ellenőrzése. Utána futtasd újra a Setupot.",
        "modified_files": "A Locoland Steames fájljai módosítva vannak ({files}):\n{path}\n\nÁllítsd vissza őket a Steamben: jobb klikk a Locolandon > Tulajdonságok > Telepített fájlok > Játékfájlok sértetlenségének ellenőrzése. Utána futtasd újra a Setupot. Az OpenLocolandnak saját beállítása van a fejlesztői billentyűkhöz: Csalások, lent.",
        "not_empty": "A mappa nem üres, és nincs benne OpenLocoland:\n{path}\n\n"
                     "Mégis ide telepítsem? A többi fájlja érintetlen marad.",
        "running": "A Locoland ebből a mappából fut. Előbb zárd be a játékot.",
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
        "options": "Opcje (można je później zmienić w oknie startowym gry)",
        "cheats": "Kody (klawisze twórców: h, W, B, A, K, M, #, x)",
        "unlock": "Wszystkie misje odblokowane (Twoje postępy pozostają bez zmian)",
        "shortcuts": "Skróty",
        "desktop": "Na pulpicie",
        "start_menu": "W menu Start",
        "install": "Zainstaluj",
        "update": "Aktualizuj",
        "uninstall": "Odinstaluj",
        "run": "Graj",
        "close": "Zamknij",
        "no_steam": "Nie znaleziono Locoland w Steamie. Zainstaluj grę przez Steam albo wskaż jej folder "
                    "(ten z plikiem Steam.exe).",
        "bad_steam": "W tym folderze nie ma Locoland ze Steama (brak Steam.exe):\n{path}",
        "inside_steam": "Folder instalacji nie może być folderem gry w Steamie ani leżeć w nim.",
        "modified_steam": "Steam.exe w tym folderze nie jest oryginalnym plikiem gry - został zmieniony lub podmieniony (np. przez poprawkę rozdzielczości):\n{path}\n\nPrzywróć oryginał w Steamie: prawy klik na Locoland > Właściwości > Zainstalowane pliki > Sprawdź spójność plików gry. Potem uruchom Setup jeszcze raz.",
        "modified_files": "Twoje pliki Locoland w Steamie są zmienione ({files}):\n{path}\n\nPrzywróć je w Steamie: prawy klik na Locoland > Właściwości > Zainstalowane pliki > Sprawdź spójność plików gry. Potem uruchom Setup jeszcze raz. OpenLocoland ma własną opcję klawiszy twórców: Kody, poniżej.",
        "not_empty": "Folder nie jest pusty i nie ma w nim OpenLocoland:\n{path}\n\n"
                     "Zainstalować mimo to? Pozostałe pliki zostaną nietknięte.",
        "running": "Locoland jest uruchomiony z tego folderu. Najpierw zamknij grę.",
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
        "editor_desc": "Edytor map OpenLocoland",
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
        "options": "Opções (podem ser alteradas mais tarde na janela inicial do jogo)",
        "cheats": "Batotas (teclas de programador: h, W, B, A, K, M, #, x)",
        "unlock": "Todas as missões desbloqueadas (o teu progresso fica como está)",
        "shortcuts": "Atalhos",
        "desktop": "No ambiente de trabalho",
        "start_menu": "No menu Iniciar",
        "install": "Instalar",
        "update": "Atualizar",
        "uninstall": "Desinstalar",
        "run": "Jogar",
        "close": "Fechar",
        "no_steam": "O Locoland não foi encontrado no Steam. Instala-o através do Steam ou escolhe a sua "
                    "pasta (a que tem o Steam.exe).",
        "bad_steam": "Esta pasta não tem o Locoland do Steam (falta o Steam.exe):\n{path}",
        "inside_steam": "A pasta de instalação não pode ser a pasta do jogo no Steam nem estar dentro dela.",
        "modified_steam": "O Steam.exe desta pasta não é o ficheiro original do jogo - foi alterado ou substituído (p. ex. por uma correção de resolução):\n{path}\n\nReponha o original no Steam: clique com o botão direito em Locoland > Propriedades > Ficheiros instalados > Verificar a integridade dos ficheiros do jogo. Depois execute o Setup outra vez.",
        "modified_files": "Os seus ficheiros do Locoland no Steam foram alterados ({files}):\n{path}\n\nReponha-os no Steam: clique com o botão direito em Locoland > Propriedades > Ficheiros instalados > Verificar a integridade dos ficheiros do jogo. Depois execute o Setup outra vez. O OpenLocoland tem uma opção própria para as teclas de programador: Batotas, abaixo.",
        "not_empty": "A pasta não está vazia e não tem uma instalação do OpenLocoland:\n{path}\n\n"
                     "Instalar mesmo assim? Os outros ficheiros ficam intactos.",
        "running": "O Locoland está a correr a partir desta pasta. Fecha primeiro o jogo.",
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
        "editor_desc": "O editor de mapas do OpenLocoland",
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
        "options": "Параметры (их можно изменить позже в окне запуска игры)",
        "cheats": "Читы (клавиши разработчиков: h, W, B, A, K, M, #, x)",
        "unlock": "Все миссии открыты (ваш прогресс не меняется)",
        "shortcuts": "Ярлыки",
        "desktop": "На рабочем столе",
        "start_menu": "В меню «Пуск»",
        "install": "Установить",
        "update": "Обновить",
        "uninstall": "Удалить",
        "run": "Играть",
        "close": "Закрыть",
        "no_steam": "Locoland не найдена в Steam. Установите её через Steam или выберите её папку "
                    "(ту, где лежит Steam.exe).",
        "bad_steam": "В этой папке нет Locoland из Steam (нет Steam.exe):\n{path}",
        "inside_steam": "Папка установки не может быть папкой игры в Steam или находиться внутри неё.",
        "modified_steam": "Steam.exe в этой папке - не оригинальный файл игры: он изменён или заменён (например, исправлением разрешения):\n{path}\n\nВосстановите оригинал в Steam: правый щелчок по Locoland > Свойства > Установленные файлы > Проверить целостность файлов игры. Затем снова запустите Setup.",
        "modified_files": "Ваши файлы Locoland в Steam изменены ({files}):\n{path}\n\nВосстановите их в Steam: правый щелчок по Locoland > Свойства > Установленные файлы > Проверить целостность файлов игры. Затем снова запустите Setup. У OpenLocoland есть своя настройка для клавиш разработчиков: Читы, ниже.",
        "not_empty": "Папка не пуста, и в ней нет OpenLocoland:\n{path}\n\n"
                     "Всё равно установить туда? Остальные файлы останутся нетронутыми.",
        "running": "Locoland запущена из этой папки. Сначала закройте игру.",
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
        "editor_desc": "Редактор карт OpenLocoland",
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
        "options": "Volby (lze je později změnit v úvodním okně hry)",
        "cheats": "Cheaty (klávesy vývojářů: h, W, B, A, K, M, #, x)",
        "unlock": "Všechny mise odemčené (tvůj postup zůstane, jak je)",
        "shortcuts": "Zástupci",
        "desktop": "Na ploše",
        "start_menu": "V nabídce Start",
        "install": "Instalovat",
        "update": "Aktualizovat",
        "uninstall": "Odinstalovat",
        "run": "Hrát",
        "close": "Zavřít",
        "no_steam": "Locoland nebyl ve Steamu nalezen. Nainstaluj ho přes Steam nebo vyber jeho složku "
                    "(tu se Steam.exe).",
        "bad_steam": "V této složce není Locoland ze Steamu (chybí Steam.exe):\n{path}",
        "inside_steam": "Instalační složka nesmí být složkou hry ve Steamu ani v ní ležet.",
        "modified_steam": "Steam.exe v této složce není původní soubor hry - byl změněn nebo nahrazen (např. opravou rozlišení):\n{path}\n\nObnovte originál ve Steamu: pravým tlačítkem na Locoland > Vlastnosti > Nainstalované soubory > Ověřit integritu herních souborů. Pak spusťte Setup znovu.",
        "modified_files": "Vaše soubory Locoland ve Steamu jsou změněné ({files}):\n{path}\n\nObnovte je ve Steamu: pravým tlačítkem na Locoland > Vlastnosti > Nainstalované soubory > Ověřit integritu herních souborů. Pak spusťte Setup znovu. OpenLocoland má pro klávesy vývojářů vlastní volbu: Cheaty, níže.",
        "not_empty": "Složka není prázdná a není v ní OpenLocoland:\n{path}\n\n"
                     "Přesto do ní instalovat? Ostatní soubory zůstanou nedotčené.",
        "running": "Locoland běží z této složky. Nejdřív hru ukonči.",
        "working": "Instalace...",
        "done": "Hotovo. OpenLocoland {version} je v {path}",
        "failed": "Instalace se nezdařila: {error}",
        "uninstall_ask": "Odebrat OpenLocoland ze složky\n{path}?\n\nSložka se smaže s celým obsahem (i s mapami "
                         "z editoru a tabulkami výsledků), spolu se zástupci a jazykovými soubory v {captured}.\n"
                         "Profily, postup a nastavení v registru zůstanou (používá je i hra ze Steamu).",
        "uninstalled": "OpenLocoland byl odebrán ze složky {path}",
        "game_name": "Locoland",
        "editor_name": "Locoland - editor map",
        "editor_desc": "Editor map OpenLocoland",
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
    """True if folder contains Locoland.exe (the game executable)."""
    return bool(folder) and os.path.isfile(os.path.join(folder, "Locoland.exe"))


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


def game_languages(steam):
    """(buildable, missing): the codes of the packs of lang\\ that can be built from the captures
    and Steam's languages, and of those whose language Steam still has to download."""
    packs = languages.packs()
    have = languages.available(steam) if is_game(steam) else set()
    return [c for c in packs if pack_base(c) in have], [c for c in packs if pack_base(c) not in have]


def depot_lines(codes):
    """The Steam console line of each pack's language (its base's for a translation), once each."""
    depots = {code: depot for depot, code in languages.DEPOT_LANGUAGES.items()}
    lines, seen = [], set()
    for c in codes:
        base = pack_base(c)
        if base in depots and base not in seen:
            seen.add(base)
            lines.append(f"    download_depot {languages.STEAM_APP_ID} {depots[base]}    {pack_name(base)}")
    return lines


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
    """The logo cropped to its letters (the picture is 640x480 with the word on black), or None."""
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


def make_shortcut(path, target, arguments, folder, icon, description, minimized=False):
    """A .lnk through the shell's WScript.Shell (PowerShell is on every Windows 10 and 11);
    `minimized` for a .cmd, so its console does not flash up."""
    def quoted(s):
        return "'" + s.replace("'", "''") + "'"
    script = ("$s = (New-Object -ComObject WScript.Shell).CreateShortcut({}); $s.TargetPath = {}; "
              "$s.Arguments = {}; $s.WorkingDirectory = {}; $s.IconLocation = {}; $s.Description = {}; "
              "$s.WindowStyle = {}; $s.Save()").format(quoted(path), quoted(target), quoted(arguments), quoted(folder),
                                                       quoted(icon + ",0"), quoted(description), 7 if minimized else 1)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-Command", script], check=True,
                   capture_output=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))


def read_manifest(target):
    try:
        return json.load(open(os.path.join(target, MANIFEST), encoding="utf-8"))
    except (OSError, ValueError):
        return None


def game_running(target):
    """Whether a Locoland.exe of this folder runs (tasklist does not give paths: PowerShell does)."""
    exe = os.path.join(target, "Locoland.exe")
    if not os.path.isfile(exe):
        return False
    result = subprocess.run(
        ["powershell", "-NoProfile", "-NonInteractive", "-Command",
         "Get-Process Locoland -ErrorAction SilentlyContinue | ForEach-Object { $_.Path }"],
        capture_output=True, text=True, creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
    return any(os.path.normcase(line.strip()) == os.path.normcase(exe) for line in result.stdout.splitlines())


def remove_shortcuts(manifest):
    for path in (manifest or {}).get("shortcuts", []):
        try:
            os.remove(path)
        except OSError:
            pass
        folder = os.path.dirname(path)
        if os.path.basename(folder) == APP and os.path.isdir(folder) and not os.listdir(folder):
            os.rmdir(folder)


def install(steam, target, choices, ver, progress=None):
    """Assembles the game into `target`, writes the options (and the game's language when one is
    given), the shortcuts and the record of the install. `choices`: cheats, unlock, desktop,
    start_menu (bool), game_language (a code or None: keep the saved one) and lang (the language
    of the shortcuts' names). Prints its log; stops with SystemExit and the reason."""
    if not is_game(steam):
        sys.exit(f"No Locoland from Steam in {steam}")
    if inside(target, steam):
        sys.exit("The install folder must not be the Steam folder of the game or inside it")
    if not build.steam_exe_original(steam):
        sys.exit(TEXTS[choices["lang"]]["modified_steam"].format(path=steam))
    modified = build.modified_steam_files(steam)
    if modified:
        sys.exit(TEXTS[choices["lang"]]["modified_files"].format(path=steam, files=", ".join(modified)))
    if game_running(target):
        sys.exit("Locoland is running from the install folder - close it first")
    locofix = os.path.join(BUNDLE, "LocoFix.dll") if BUNDLE else None
    os.makedirs(CAPTURED, exist_ok=True)
    print(f"OpenLocoland {ver}: {steam} -> {target}")
    build.build(steam, target, locofix=locofix, captured=CAPTURED, progress=progress)
    write_setting("Cheats", choices["cheats"])
    write_setting("UnlockMissions", choices["unlock"])
    print(f"Options: Cheats {int(choices['cheats'])}, UnlockMissions {int(choices['unlock'])}")
    if choices.get("game_language"):
        write_setting("Language", choices["game_language"])
        print(f"Game language: {choices['game_language']}")
    remove_shortcuts(read_manifest(target))
    shortcuts = make_shortcuts(target, choices, ver)
    with open(os.path.join(target, MANIFEST), "w", encoding="utf-8") as f:
        json.dump({"version": ver, "steam": steam, "captured": CAPTURED, "shortcuts": shortcuts}, f, indent=2)


def make_shortcuts(target, choices, ver):
    texts = TEXTS[choices["lang"]]
    exe = os.path.join(target, "Locoland.exe")
    folders = []
    if choices["desktop"]:
        folders.append(special_folder(0x10))   # CSIDL_DESKTOPDIRECTORY
    if choices["start_menu"]:
        folders.append(os.path.join(special_folder(0x02), APP))   # CSIDL_PROGRAMS
    made = []
    for folder in folders:
        game = os.path.join(folder, texts["game_name"] + ".lnk")
        make_shortcut(game, exe, "", target, exe, APP + " " + ver)
        editor = os.path.join(folder, texts["editor_name"] + ".lnk")
        make_shortcut(editor, os.path.join(target, "edit_map.cmd"), EDITOR_MAP, target, exe, texts["editor_desc"],
                      minimized=True)
        made += [game, editor]
        print("Shortcuts: " + folder)
    return made


# ---- the window ----

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
    def __init__(self, root, steam=None, target=None, auto=False):
        self.root = root
        self.lang = first_ui_language()
        self.version = version()
        self.events = queue.Queue()
        self.busy = False
        self.auto = auto   # --auto: install at once, no questions, close at the end
        self.result = None
        self.widgets = {}
        self.logo = None
        self.buildable, self.missing = [], []
        self.first_language = None   # the game language shown first: written only when changed
        self.steam = tk.StringVar(value=steam or find_game() or "")
        self.target = tk.StringVar(value=target or DEFAULT_TARGET)
        self.cheats = tk.BooleanVar(value=read_option("Cheats"))
        self.unlock = tk.BooleanVar(value=read_option("UnlockMissions"))
        self.desktop = tk.BooleanVar(value=True)
        self.start_menu = tk.BooleanVar(value=True)
        self.ui_language = tk.StringVar(value=TEXTS[self.lang]["name"])
        self.game_language = tk.StringVar()
        self.layout()
        self.target.trace_add("write", lambda *_: self.target_changed())
        self.steam.trace_add("write", lambda *_: self.steam_changed())
        self.steam_changed()
        self.root.after(100, self.poll)
        if auto:
            self.root.after(1000, self.install)

    def t(self, key, **values):
        return TEXTS[self.lang][key].format(version=self.version, **values)

    def layout(self):
        r = self.root
        r.resizable(True, True)
        w = self.widgets
        # the header: the game's logo on black once the game is found, else the name in text
        w["header"] = tk.Label(r, text=APP, font=("Segoe UI", 18, "bold"), pady=10)
        w["header"].grid(row=0, column=0, sticky="ew")
        self.header_colours = (w["header"].cget("background"), w["header"].cget("foreground"))
        frame = ttk.Frame(r, padding=12)
        frame.grid(row=1, column=0, sticky="nsew")
        r.columnconfigure(0, weight=1)
        r.rowconfigure(1, weight=1)
        frame.columnconfigure(1, weight=1)

        top = ttk.Frame(frame)
        top.grid(row=0, column=0, columnspan=3, sticky="ew")
        top.columnconfigure(0, weight=1)
        w["intro"] = ttk.Label(top, wraplength=520, justify="left")
        w["intro"].grid(row=0, column=0, sticky="w")
        w["language"] = ttk.Label(top)
        w["language"].grid(row=0, column=1, sticky="e", padx=(12, 4))
        choice = ttk.Combobox(top, textvariable=self.ui_language, values=[n for _, n in UI_LANGUAGES],
                              state="readonly", width=10)
        choice.grid(row=0, column=2, sticky="e")
        choice.bind("<<ComboboxSelected>>", lambda e: self.switch_language())

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
        w["game_language_choice"] = ttk.Combobox(row, textvariable=self.game_language, state="readonly", width=14)
        w["game_language_choice"].grid(row=0, column=0, sticky="w")
        w["more_languages"] = ttk.Label(row)
        w["more_languages"].grid(row=0, column=1, sticky="w", padx=(10, 0))

        w["options"] = ttk.LabelFrame(frame, padding=6)
        w["options"].grid(row=4, column=0, columnspan=3, sticky="ew", pady=(10, 0))
        w["cheats"] = ttk.Checkbutton(w["options"], variable=self.cheats)
        w["cheats"].grid(row=0, column=0, sticky="w")
        w["unlock"] = ttk.Checkbutton(w["options"], variable=self.unlock)
        w["unlock"].grid(row=1, column=0, sticky="w")

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
                    "shortcuts", "desktop", "start_menu"]:
            w[key].configure(text=self.t(key))
        w["more_languages"].configure(text=self.t("more_languages") if self.missing else "")
        w["browse_steam"].configure(text=self.t("browse"))
        w["browse_target"].configure(text=self.t("browse"))
        for key in ["uninstall", "run", "close"]:
            w["button_" + key].configure(text=self.t(key))
        self.refresh()
        self.show_language_note()

    def switch_language(self):
        self.lang = next(c for c, n in UI_LANGUAGES if n == self.ui_language.get())
        self.apply_texts()

    def steam_changed(self):
        """The header's logo and the game languages come from the Steam copy."""
        steam = self.steam.get().strip()
        self.logo = logo_image(steam)
        header = self.widgets["header"]
        if self.logo:
            header.configure(image=self.logo, text="", background="black")
        else:
            header.configure(image="", text=APP, background=self.header_colours[0])
        self.buildable, self.missing = game_languages(steam)
        self.widgets["game_language_choice"].configure(values=[pack_name(c) for c in self.buildable])
        self.target_changed()
        self.apply_texts()

    def target_changed(self):
        """An install there or not: the game language shown first is the saved one or English."""
        target = self.target.get().strip()
        self.first_language = first_game_language(self.buildable, bool(target) and read_manifest(target) is not None)
        self.game_language.set(pack_name(self.first_language) if self.first_language else "")
        self.refresh()

    def show_language_note(self):
        """The log starts with how to get the game's other languages from Steam."""
        lines = []
        if not self.buildable and is_game(self.steam.get().strip()):
            lines.append(self.t("no_language"))
        if self.missing:
            lines += [self.t("depots"), ""] + depot_lines(self.missing)
        if not self.busy and self.progress["value"] == 0:
            self.log.configure(state="normal")
            self.log.delete("1.0", "end")
            self.log.insert("end", "\n".join(lines) + ("\n\n" if lines else ""))
            self.log.configure(state="disabled")

    def chosen_game_language(self):
        name = self.game_language.get()
        return next((c for c in self.buildable if pack_name(c) == name), None)

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
        """--auto: the window's log into install.log of the install folder, then close."""
        self.result = ok
        target = self.target.get().strip()
        if os.path.isdir(target):
            with open(os.path.join(target, "install.log"), "w", encoding="utf-8") as f:
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
        if not build.steam_exe_original(steam):
            self.error(self.t("modified_steam", path=steam))
            return
        modified = build.modified_steam_files(steam)
        if modified:
            self.error(self.t("modified_files", path=steam, files=", ".join(modified)))
            return
        target = os.path.abspath(target)
        update = read_manifest(target) is not None
        if os.path.isdir(target) and os.listdir(target) and not update and not self.auto:
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
        choices = {"cheats": self.cheats.get(), "unlock": self.unlock.get(), "desktop": self.desktop.get(),
                   "start_menu": self.start_menu.get(), "lang": self.lang,
                   # a new install sets the game's language; an update only when it was changed here
                   "game_language": language if not update or language != self.first_language else None}
        threading.Thread(target=self.work, args=(steam, target, choices), daemon=True).start()

    def work(self, steam, target, choices):
        out, err = sys.stdout, sys.stderr
        sys.stdout = sys.stderr = LogWriter(self.events)
        try:
            install(steam, target, choices, self.version, lambda f: self.events.put(("progress", f)))
            self.events.put(("done", self.t("done", path=target)))
        except SystemExit as e:
            self.events.put(("failed", str(e.code)))
        except Exception as e:   # anything else: shown, not lost in a closed console
            self.events.put(("failed", f"{type(e).__name__}: {e}"))
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
        shutil.rmtree(os.path.dirname(CAPTURED), ignore_errors=True)
        self.say(self.t("uninstalled", path=target) + "\n")
        self.refresh()

    def run_game(self):
        target = self.target.get().strip()
        subprocess.Popen([os.path.join(target, "Locoland.exe")], cwd=target)

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
    """--silent: the install without the window, its log in <target>\\install.log."""
    target = os.path.abspath(a.target or DEFAULT_TARGET)
    os.makedirs(target, exist_ok=True)
    steam = a.steam or find_game() or ""
    update = read_manifest(target) is not None
    buildable, missing = game_languages(steam)
    language = a.game_lang or (None if update else first_game_language(buildable, False))
    choices = {"cheats": a.cheats == "1", "unlock": a.unlock == "1", "desktop": not a.no_shortcuts,
               "start_menu": not a.no_shortcuts, "lang": a.lang or first_ui_language(), "game_language": language}
    with open(os.path.join(target, "install.log"), "w", encoding="utf-8") as log:
        sys.stdout = sys.stderr = log
        try:
            if a.game_lang and a.game_lang not in buildable:
                sys.exit(f"Game language {a.game_lang} cannot be built (it can: {', '.join(buildable) or 'none'})")
            if missing:
                print("Not available yet (download_depot in the Steam console, then run Setup again):")
                print("\n".join(depot_lines(missing)))
            install(steam, target, choices, version())
        except SystemExit as e:
            print(f"FAILED: {e.code}")
            return 1
        except Exception as e:   # the log is all a silent install has
            print(f"FAILED: {type(e).__name__}: {e}")
            return 1
        print("OK")
    return 0


def main():
    import argparse
    codes = [c for c, _ in UI_LANGUAGES]
    p = argparse.ArgumentParser(description=APP + " installer")
    p.add_argument("--steam", help="the Steam folder of Locoland (default: found through Steam)")
    p.add_argument("--target", help="the install folder (default: " + DEFAULT_TARGET + ")")
    p.add_argument("--silent", action="store_true", help="install without the window")
    p.add_argument("--auto", action="store_true",
                   help="the window installs at once and closes at the end (tests of the window)")
    p.add_argument("--cheats", choices=["0", "1"], help="the option (default: as it is in the registry)")
    p.add_argument("--unlock", choices=["0", "1"], help="the option (default: as it is in the registry)")
    p.add_argument("--game-lang", help="the game's language (default for a new install: the system's if there is a pack, else English; kept on an update)")
    p.add_argument("--no-shortcuts", action="store_true")
    p.add_argument("--lang", choices=codes, help="the language of the shortcuts' names (default: the system's, else en)")
    a = p.parse_args()
    if a.silent:
        a.cheats = a.cheats or ("1" if read_option("Cheats") else "0")
        a.unlock = a.unlock or ("1" if read_option("UnlockMissions") else "0")
        sys.exit(silent(a))
    root = tk.Tk()
    icon = os.path.join(BUNDLE or os.path.join(os.path.dirname(os.path.abspath(__file__)), "icon"), "openlocoland.ico")
    if os.path.exists(icon):
        root.iconbitmap(default=icon)
    window = Installer(root, a.steam, a.target, a.auto)
    root.mainloop()
    if a.auto:
        sys.exit(0 if window.result else 1)


if __name__ == "__main__":
    main()
