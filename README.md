![OpenLocoland logo](docs/img/openlocoland_logo.png)

![A mission of Locoland on a wide screen](docs/img/widescreen.jpg)

**Locoland** (Gromada, 2003) is a little-known strategy game about armed trains: you lay track,
send out locomotives with cannons and rocket launchers, and drive the invaders out of a land of
living trains. **OpenLocoland** brings it back to today's Windows: it runs in a window or on full
screen in any resolution, plays well on a wide screen, fixes some of the old crashes and bugs - and it has a **map editor inside the game** (work in progress), so you can make your own missions and share
them with friends.

It is a fan project, not affiliated with the makers of Locoland. It does not contain the game: you
need your own copy of [Locoland from Steam](https://store.steampowered.com/app/352130/Locoland/),
and OpenLocoland is put together from it on your computer. Your Steam game stays untouched.

## What you get

- **The game on today's Windows.** It runs in a window or on full screen in any resolution up to
  4K, shows more of the map on a wide screen and keeps a steady 60 frames a second.
- **Old bugs fixed.** Some of the original's crashes, missing sounds and a car the factory could
  never build are fixed; the ones still open are listed in [BUGS.md](BUGS.md).
- **Languages with their gaps filled.** English, German, Hungarian, Polish and Russian get their
  missing texts, a Czech fan translation (unofficial) works on any Steam copy, and Setup lists them
  all.
- **Custom maps.** A second page of the strategic map holds **POLIGON**, a ready mission to try,
  and every map you make or get from a friend.
- **A map editor inside the game (work in progress).** Make your own missions - lay the track,
  shape the ground, put up buildings with their guns, give trains their orders, send in new trains
  during the mission, plan what the enemy's factory builds, decide who fights whom - and play them
  at once.
- **Maps for friends.** A map goes to a friend as one file and turns up on their custom maps page.
- **Updates.** The launcher tells you when there is a newer version and downloads it, and an update
  keeps your maps, scores and progress.
- **Two extras, off by default**: the developers' cheat keys and all missions open from the start
  (your real progress is not touched).

## Pictures

| | |
|---|---|
| ![A battle](docs/img/battle.jpg) | ![The custom maps page](docs/img/custom-maps-en.jpg) |
| A battle over the lava at 1920 x 1080 | Custom maps: POLIGON, your own maps and the editor |
| ![The map editor](docs/img/editor-factory-en.jpg) | ![The train arrivals](docs/img/arrivals-en.jpg) |
| The editor on POLIGON: the two factories | The trains that come during the mission |
| ![The editor's help](docs/img/help-contents-en.jpg) | ![The launcher](docs/img/launcher-update-en.jpg) |
| Help inside the editor | The launcher, checking for updates |

## Installing

1. Install Locoland through Steam (any language).
2. Download `Setup.exe` from [Releases](https://github.com/Dysprohol/OpenLocoland/releases) and run
   it (Windows may warn about an unknown publisher: "More info" > "Run anyway"). It finds the
   Steam game and installs OpenLocoland into a folder of its own; you choose the language and the
   two extras there.
3. Start **Locoland** or **Locoland - map editor** from the desktop or the Start menu.

Running a newer `Setup.exe` updates the game and keeps your maps, scores and progress; **Check for
updates** in the launcher downloads a newer one for you. The
player's guide tells the rest, the editor with a step-by-step first mission included:
[GUIDE.md](docs/GUIDE.md) (English), [PRZEWODNIK.md](docs/PRZEWODNIK.md) (Polish).

This is the first regular release; the map editor is still work in progress. Bugs and ideas are
welcome in [Issues](https://github.com/Dysprohol/OpenLocoland/issues).

## How it was made

Nobody has the source code of Locoland, so the work started with the program itself. The 2003
`Locoland.exe` was taken apart with the [Ghidra](https://ghidra-sre.org/) decompiler, and step by
step every one of the engine's 993 functions got a name and a description: how the trains drive,
how a map is stored, how the mission scripts run. The fixes are written in C++ in `LocoFix.dll`,
which the original program loads at start, so the game stays itself and keeps working at every
step. Graphics go through [dgVoodoo2](http://dege.freeweb.hu/dgVoodoo2/), which turns the old
DirectX 7 into something today's Windows draw properly.

Along the way the authors' own map editor turned up inside the engine - the mode they made the
campaign with, switched off in the game they sold. OpenLocoland switches it back on and builds the
rest around it: the bar, the tools and the help in the look of the game.

## Plans

- **Multiplayer.** The engine already lists network functions for its mission scripts, but the
  authors never wrote them. The goal is to play with or against a friend.
- **New missions.** Our own missions and an alternative campaign made in the editor, next to the
  original twenty.
- **More fixes and polish.** The bugs still open in [BUGS.md](BUGS.md), the interface scaled up on
  big screens, the editor tried in full screen, and whatever turns up while playing.
- **The engine in readable code.** The understood parts of the engine rewritten in C++ one by one,
  so the game can grow past the limits of the original program.
- **Ideas for later**: a modding tool for new objects, music and sounds of our own.

## For developers

- [DEVELOPERS.md](DEVELOPERS.md) - building from the repository, the layout, everything known
  about the engine and its data, the fixes, the editor, the release.
- [BUGS.md](BUGS.md) - the bugs of the original game, fixed and open.
- [MAP_RULES.md](MAP_RULES.md) - what a map needs to work.

Nothing of the game is in this repository: game files are taken from the player's own Steam copy
when the game is put together, and our changes to them are kept as patches.

## Thanks

OpenLocoland is better thanks to the people who gave something of their own to it:

- **[JohnVitoScaletta](https://steamcommunity.com/profiles/76561199078710882/)** (Steam) - the
  Czech fan translation of the game.
- **Danetry** (Discord) - support of the Czech fan translation of the game).
- **just_Martin** (Discord) - the OpenLocoland logo (the installer's header and the launcher's
  picture).

## Credits

Locoland is by Gromada and ESDigital Games. dgVoodoo2 is by Dege (see [NOTICE.md](external/dgVoodoo2/NOTICE.md)).
OpenLocoland is a fan project, not affiliated with the rights holders of Locoland.
