# OpenLocoland - developer documentation

The technical side of the project: how the game is assembled, what we know about the engine and
its data, the fixes, the editor, the release. The front page for players is `README.md`.

An open, maintained version of the Locoland engine (Gromada / ESDigital Games, 2003), in the
spirit of OpenRCT2 and OpenTTD. It needs the original game - the Steam version - and builds
on top of it: today the original exe runs with our fixes loaded as LocoFix.dll; step by step
the engine is documented (names and struct types for the decompiled code) and its functions
are rewritten in readable C++ that replaces the originals one at a time, so the game stays
playable all the way. Not affiliated with the rights holders of Locoland.

The goals: run well on current Windows (window, any resolution, no external setup), fix the
game's bugs, open it up for mods, new missions and translations.

The principle for what goes where: the repository is for our own work - code, tools,
documentation, names and types, and our changes to game files as patches. Everything that
belongs to the game is taken from the player's own copy when the game is assembled, and
everything derived from it (decompiled code, unpacked data) is generated locally (see
"Distribution").

Code, tools and documentation are in English. Other languages appear only in the game's
translations (`lang/`).

Buildings are named the way the game names them to the player: VID 102 is the **factory**
(Polish "Fabryka", strings.ini `102=`) and VID 104 the **repair point** (Polish "Punkt
naprawczy"; the English game calls it "Repair Depot", which is why "depot" is not used for
either). VID 165 (`завод`, the industrial building with two tower sockets like the fort's) is
the **mine** - Polish "Kopalnia", English "Mining plant" (the game has no name for it; ours are
in `lang/<code>/game/strings.override.ini`) - and VID 86 is the explosive **mine** the repair car lays
(`МИНА`, English "Mine"): the Polish translation called it "Kopalnia", mistaking it for a mine
pit, so the Polish pack now names it "Mina" and keeps "Kopalnia" for VID 165 alone. Our texts in
the other languages take the names of that language's own `strings.ini` (German Fabrik and
Werkstatt, Hungarian Gyáregység and Szervízdepó, the authors' Russian Депо and Ремонтная
мастерская), so they read like the rest of that version of the game. The engine's
own identifiers keep the authors' names - `B_DEPO`, `Depo_*`, `IsNoDepo`
come from the Russian "депо", `Building_UpdateWorkshop` is ours - and so do the names of
files and functions already in the code.

## Where things are

The documentation is split by area into `docs/dev/`. Read the part of the area you work on;
a reference such as `DEVELOPERS.md "Editor mode"` or `see "Updating"` (in the code, in `BUGS.md`,
`MAP_RULES.md` and in the parts themselves) names a section or a paragraph's bold lead below -
find the name here and open its file. New findings go into the part they belong to, not here;
a new section or bold lead is added to this list.

### [setup.md](docs/dev/setup.md) (~15 KB)

Sections: "Quick start"; "Repository layout".

Paragraphs: Players; Options; Updates; Developers; Languages.

### [build.md](docs/dev/build.md) (~19 KB)

Sections: "How the game is built".

Paragraphs: Engine; Graphics and dgVoodoo; Configuration; Game logic in scripts; Data; Texts; Developer mode; Languages; Czech; The Czech letters; The Czech edition; Logs.

### [czech-edition.md](docs/dev/czech-edition.md) (~7 KB)

Sections: "The Czech edition of the game"; "What it is"; "The same as Steam"; "What differs"; "Slips in the edition's texts"; "What OpenLocoland does with it".

### [formats.md](docs/dev/formats.md) (~46 KB)

Sections: "Data formats".

Paragraphs: Container; objects.res; Editing objects.res; Map; Demo; Editing maps and images; New maps; A map to play; The custom maps page; Map files; Rename or delete; The ground of a map; A picture painted as the editor paints; Editing a map.

### [engine.md](docs/dev/engine.md) (~41 KB)

Sections: "Rendering"; "Script machine"; "Support library"; "Pictures and animations"; "Trains and rails"; "Player and the state bar".

Paragraphs: Lost surfaces; The fog buffer; Loading; Language; Values and variables; Calls from the engine; Bytecode; String; The error log; CRC32; `File`; The ini file and the registry; `Picture`; `PictureImpl`; Three ways to animate; Screenshots; The VID converter; Compressed frames; The coder; The model; The rail network; A train; Commands; Driving; How the cars follow; Repair car and arsenal; Factories; The balloon; One Player per army; The flagman; The state bar; Panels; The factory panels; Messages.

### [editor.md](docs/dev/editor.md) (~24 KB)

Sections: "Editor mode".

Paragraphs: The level is held; The menu; Saving; Starting it and going back; Back to the editor; Hints under the cursor; Music while editing; The map the editor starts on; A level change resets the editor.

### [editor-objects.md](docs/dev/editor-objects.md) (~22 KB)

Sections: "Editor mode" (continued).

Paragraphs: Editing objects; Placing over objects, the track as a help; Placing helps: the grid, onto the track, the height, the turn, scattering; Copy, paste, duplicate; The history's limits; Laying track; The ground under rising rails; Debug keys.

### [editor-mission.md](docs/dev/editor-mission.md) (~58 KB)

Sections: "Editor mode" (continued).

Paragraphs: Mission settings and push lines; The enemy's trains of the script; Train arrivals; Capturing; The AI and the trains of an editor map - the review of 2026-11-05; The trains' orders; A train placed on a light's line did nothing; A target and a start; Every army the same orders; guarding an area; AREA watches from where the train stands; The armies' fight rules; What a row does, and click-and-hold; The templates; The panel; A factory's build options; A factory's plan; The data; The script; Rebuilt when lost; The AI's knobs.

### [editor-ground.md](docs/dev/editor-ground.md) (~95 KB)

Sections: "Editor mode" (continued).

Paragraphs: Ground, gravel bed, decals; The relief and the heights of objects; A new map; How big a map can be; The map's frame; The armies' colours; On the track; The object's panel, the guns' sockets, the palette; Wholes, fly nests and mines; Flags; The rows of the ground's tools; The gravel bed in the Paint tool; Wrecks on the track; Trains, the vehicles' palette, patrols; On the train's own track; Rail kinds and ramps; The ground's picture, water, regions; Paint the ground; Regions, reviewed; A fog that rises or pulses; Sheets of water and lava.

### [editor-ui.md](docs/dev/editor-ui.md) (~16 KB)

Sections: "Editor mode" (continued).

Paragraphs: Map exchange; The texts of the editor; The Help and Menu buttons; Ctrl+R; No window menu; Full screen, 1920 x 1080; The frame and the profiler.

### [changes.md](docs/dev/changes.md) (~43 KB)

Sections: "Changes from the original".

### [working.md](docs/dev/working.md) (~18 KB)

Sections: "Working with the code"; "Resolutions and widescreen"; "Status and known bugs".

Paragraphs: Frame rate at high resolutions; CI.

### [release.md](docs/dev/release.md) (~21 KB)

Sections: "Release"; "Distribution"; "Next".

Paragraphs: Every edition before the tag; What Setup does; Updating; Uninstalling; Checked; By hand; Rights holders; Working on one's own copy; What can be shared; What must not be shared; Published that way.
