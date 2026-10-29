# OpenLocoland - player's guide

Version 0.1 - a test release. [Polish version: PRZEWODNIK.md](PRZEWODNIK.md)

**OpenLocoland** is an unofficial, fan-made new edition of **Locoland** (Gromada / ESDigital
Games, 2003): the original game runs with fixes for today's Windows, a few improvements, a page
of custom maps in the menu and a **map editor** inside the game. It is not affiliated with the
rights holders of Locoland.

It does **not** contain the game. It needs your own copy of **Locoland from Steam**: Setup takes
the game's files from it and puts OpenLocoland together in a folder of its own. Your Steam
installation is not changed - you can still start the original from Steam.

Contents:

1. [Installing, updating, uninstalling](#1-installing-updating-uninstalling)
2. [The first start](#2-the-first-start)
3. [What is new in the game](#3-what-is-new-in-the-game)
4. [Custom maps and POLIGON](#4-custom-maps-and-poligon)
5. [The map editor](#5-the-map-editor)
6. [Tutorial: your first mission](#6-tutorial-your-first-mission)
7. [Keys and tools of the editor](#7-keys-and-tools-of-the-editor)
8. [Questions and problems](#8-questions-and-problems)

## 1. Installing, updating, uninstalling

You need: Windows 10 or 11 and Locoland installed through Steam (any of its languages).

**Installing.** Run `Setup.exe`. It finds your Steam copy of Locoland and installs
OpenLocoland into `C:\Users\<you>\Games\OpenLocoland` (you can choose another folder). It makes these shortcuts:
- **Locoland** - the game,
- **Locoland - map editor** - the editor on an empty map,

on the desktop and in the Start menu (Setup can leave them out).

**Game language.** Setup's list shows all the game's languages. The ones you can choose are those
of your Steam copy, plus the fan translations marked **(unofficial)** (Czech), which Setup builds on
whatever language you have. The others are greyed, with the line that gets them from Steam
(e.g. `download_depot 352130 352132` for German): type it into the Steam console
(`steam://open/console` in the browser's address bar or Win+R), wait until Steam has downloaded it
and run Setup again - it keeps your chosen language and adds the new one.

The game folder is self-contained: the maps you make, your high scores and the logs live there.
Your profile (the player's name, the missions won, the scores, the sound settings) is kept in your
Windows user's registry, as the original kept it.

**Updating.** Run a newer `Setup.exe` and press **Update**: it finds the installed game and copies only what changed. Your own maps (in `maps\` of the game folder) and your profile are kept.

To find out whether there is a newer version, press **Check for updates** in the launcher (see
[the first start](#2-the-first-start)). It asks the project's page on GitHub and says "You have the
latest version" or "New version ... is available"; then **Download** saves the new `Setup.exe` in
your Downloads folder, and **Run Setup** starts it (after a question) and closes the game, so Setup
can update it - Windows may ask whether to run a downloaded program, as it does after a browser's
download. The **GitHub** link opens the releases page in your browser instead (right-click it to
copy the address). Tick **at start** to check every time the launcher opens; it is off by default,
and nothing is downloaded or installed without your click. When GitHub cannot be reached (no
internet, or the project has moved) the launcher says so with the project's address, taken from
`update.txt` in the game folder - a later Setup may change that file, and you may too.

**Uninstalling.** Run `Setup.exe` and press **Uninstall**: it removes the shortcuts and the game folder with everything in it (OpenLocoland is not listed in Windows "Apps"). The Steam game stays as it is. Copy your own maps somewhere first
if you want to keep them (each one is a folder in `maps\`, see [sharing a map](#13-save-export-and-share)).

## 2. The first start

The game starts with its launcher (the window with the Locoland picture):

![The launcher](img/launcher.jpg)

- **Resolution** - besides 640 x 480 ... 1024 x 768 the list has the widescreen modes of your
  monitor (1280 x 720 up to 3840 x 2160) and your desktop's own resolution.
- **Full screen** - the box under the picture. Unticked, the game runs in a **window** (the window
  remembers its place).
- **Language** - the list next to it: the languages you have (Polish, English, ...; a fan
  translation is marked "(unofficial)"). The launcher's labels change with it. If the language you
  chose is no longer there, the game starts in English, or without English in your Steam copy's.
- **Limit FPS** and **FPS counter** - the row under the resolution: how many frames a second the game
  draws (60 by default; ∞ = no limit) and a small counter at the top of the screen.
- **Check for updates** - the row above OK: whether there is a newer OpenLocoland, see
  [updating](#1-installing-updating-uninstalling).

Your choice is remembered. The intro movies are skipped: the game opens on the player's screen.
Make a player (or pick yours) and the strategic map with the campaign's 20 missions comes up.

## 3. What is new in the game

The missions play as they always did. What changed:

- **A window or full screen, any resolution.** The game area of the window is exactly the chosen
  resolution, the picture and the mouse are right in a window, and the window starts in the middle
  of your screen (it also never comes back off-screen after the game was closed while minimised).
- **Widescreen.** On a big screen you see more of the map; the interface goes to the edges. Mission 1
  (a 1024 x 768 map) and the menus are scaled up to fill the height of the window. The objects and
  the shadows are drawn across the whole screen.

  ![A mission at 1920 x 1080](img/widescreen.jpg)

- **The state bar across the whole screen.** On a wide screen the bar at the bottom runs from edge
  to edge instead of stopping in the middle.
- **A second row in the factory panel.** A factory can build up to 14 kinds of cars (keys F1 - F12,
  `[` and `]`), but its panel showed ten. The 11th and further now show in a second row above the
  first, and a click on them works.
- **The middle mouse button** drags the map, in the missions too.
- **A steady frame rate** (60 by default) instead of the game using a whole processor core.
- **Your progress carries over.** The profile you had in the original (missions won, scores,
  settings) is taken over the first time you start OpenLocoland.
- **Fixes** of crashes and mistakes of the original: a crash when the repair point took over an
  enemy car, crashes of some mission scripts, a car kind the factory could never build, the mouse
  stuck at the bottom of the screen on a second monitor left of or above the main one, sounds that
  were missing, the fort's cannon rising out of its pit in every direction, and more.
- **The translations** are completed (Polish and English): the missing tips and unit names. The
  explosive mine (**Mine**) and the mining plant (**Mining plant**) have names of their own.

## 4. Custom maps and POLIGON

The strategic map has a **second page**: the `►►►` button at its bottom right leads to
**CUSTOM MAPS**, `◄◄◄` back to the campaign.

![The custom maps page](img/custom-maps-en.jpg)

- **MAP EDITOR** (the first row) starts the editor - see [the map editor](#5-the-map-editor).
- Under it are the custom maps: those that came with the game and those you make or get from
  friends. A long list scrolls - the arrows beside it, the mouse wheel, the up and down keys.
- A click on a map shows its briefing; **START GAME** plays it like a campaign mission.

**POLIGON** is a ready mission made with the same tools as the editor: the enemy has built a
bastion in the east - a fort with two guns guards its gates, behind it stand its factory and repair
point, and new trains come out of the factory. Break through and destroy them. (Its texts are in
Polish only.)

A custom map is not a campaign mission: winning it does not unlock missions or add to your
campaign score, rank or the hall of fame. It keeps its own high score table, "restart the mission"
in the Esc menu plays it again, and after it the menu comes back on the custom maps page.

## 5. The map editor

The editor is the engine's own editor mode, which the authors used for their maps and which the
game never offered: what you place is what the game plays. You can open any map in it - a new
one, POLIGON or a campaign mission (save a campaign map under a name of its own before changing it;
POLIGON and the template too - every update of the game writes them again, so the editor always saves
them under a new name, proposing e.g. `poligon1`).

**Starting it:** **MAP EDITOR** on the custom maps page - the game starts again in the editor, on a
new empty map of 2048 x 1536 grass that has no name yet (NEW MAP). The desktop shortcut **Locoland -
map editor** opens one straight away. `Ctrl+S` (or Menu → **SAVE MAP**) saves it: the first time it
asks for the name - type e.g. `bridge_1` and the map goes into a folder of its own,
`maps\bridge_1\bridge_1.map`, and onto the custom maps page. A new map never saved is gone when you
leave it (another map, back to the menu, exit), so trying the editor out leaves nothing behind.

![The editor](img/editor-factory-en.jpg)

What you see:

- **The map** as authored, standing still: the level is *held* - nothing moves, no script runs.
  Outside the map there is a dim grid and the map's edge has a frame (you can scroll 300 px past
  the edge: the track goes on out there).
- **The bar** at the bottom in the look of the game's state bar: tools 1 - 0 in its slots, more
  tools in the **strip** above it. The chosen one is framed.
- **The status line** at the top: the tool (or the one under the mouse), the undo and redo counts,
  "not saved" and the last message - why something could not be done is always written there.
- **The ? and menu buttons** at the left end of the bar (where the game has its pause button).
  **?** (or F1) opens the help: eighteen pages with everything of this chapter in short. It starts on
  its **contents**: the categories (BASICS, TRACK AND TRAINS, OBJECTS, MISSION, SAVING, GROUND,
  ARRIVALS, FACTORIES) on the left, the pages of the one under the mouse on the right - a click opens
  one. A page has its category for the title and its number (5 / 17); the arrows, Page Up / Down and
  the wheel turn the pages, and **CONTENTS**, `Esc` or the right button go back to the contents.

  ![The help](img/editor-help-en.jpg)

  The **menu button** opens the editor's menu with every command - new, load, save, mission
  settings, map size, play the saved map, undo, redo, quick test, back to the menu, exit.

  ![The editor's menu](img/editor-menu-en.jpg)

**Moving around:** the screen edge, the arrow keys, or drag with the middle mouse button.

**The basic editing** (the Select tool, key 1):

| Action | How |
|---|---|
| select | click an object (the smallest under the cursor); the circle marks it |
| move | drag it |
| turn | R / E or the mouse wheel |
| raise / lower it | PgUp / PgDn by 4 (with Shift by 16), or Shift+wheel; Home puts it back on the ground - the status line shows its height above the ground |
| change its army | A: ours, enemy, neutral, pirates - the flags change colour |
| delete | Del (Shift+Del: a whole train; Shift+click with the Delete tool: a whole section of track) |
| undo / redo | Ctrl+Z / Ctrl+Y - every change; the last 500 steps are kept |
| its panel | the right mouse button on it |

The **object's panel** (right button) has its army, turning, its height (lower, raise, on the ground) and delete, a semaphore's colour and
its line, and - for a fort, a mining plant or a citadel - the gun in each of its sockets (empty, a
cannon or missiles; while the panel is open the sockets are numbered on the wall, and a click on a
gun opens its host's panel). The right button elsewhere or Esc closes it.

![A fort's panel](img/editor-fort-panel-en.jpg)

## 6. Tutorial: your first mission

We will make a small mission: the player's train arrives from past the west edge, the player has
a factory and a repair point; the enemy has a fort with two guns on the line and a train on patrol.
The mission is won when the enemy has no units left.

### 1. A new map

The editor starts on an empty 2048 x 1536 grass map - you can build the mission right on it and
save it under a name of your own (`Ctrl+S` asks for it the first time, e.g. `bridge_1`). For another
ground or size, Menu button → **NEW MAP**:

![The New map dialog](img/new-map-en.jpg)

- **Folder** - the map's name: small letters, digits and `_` (e.g. `bridge_1`). The map is
  `maps\bridge_1\bridge_1.map`.
- **Ground** - *an empty map* of one texture (grass, sand, snow, rock ... - flat and bare, to paint
  on), *a meadow*, or the terrain of one of the campaign's maps.
- **Size** - for an empty map or a meadow: from 512 to 4096 px each way in steps of 256 (`-` and
  `+`), or a preset: 1024 x 768 (small, like mission 1), 2048 x 1536 (most missions), 3072 x 2304
  (big). Take 2048 x 1536 for a start.

OK - the editor opens the new map. It already has a mission script and texts made from the
template; you will fill them in at step 10. (**MAP SIZE** in the menu changes the size later: a
smaller map loses the ground and the objects past its new edge - the track may go on 300 px further,
out of sight.)

### 2. Paint the ground

The strip's **Paint the ground**: its row of buttons above the bar has the textures (grass, light
grass, forest floor, sand, desert, snow, ice, rock, dark rock, lava, swamp, lake bed - taken from the
campaign's maps; `[` and `]` pick them too), **SIZE** (8 - 200 px; the mouse wheel too), **STRENGTH**
(how much a pass covers: lower it to blend textures) and **EDGE** soft or hard. Paint by dragging
with the left button; the right button (or Shift+drag) puts the original picture back; Alt+click
takes the texture under the cursor (also one of a picture painted and saved before). Paint a sand path where the track will run and some
patches of other textures. The painted picture is saved with the map.

### 3. Water, swamp, lava

The strip's **Water and lava**: `W` picks water, swamp or lava; drag to lay it. It lowers the
ground into a basin with a shore, lays the water's surface and adds its effects (circles on the
water, bubbles, lava eruptions) and a region where explosions splash. Shift+drag takes it away.
One kind of water has one level on a map, as in the missions.

The **Ground height** tool shapes the ground with its row of buttons above the bar: the mode -
**RAISE/LOWER** (the left button raises, the right one lowers), **FLATTEN**, **SMOOTH**, **RAMP** (drag
from one point to another: an even slope between their levels, for a driveway up to a plateau) and
**ROUGH** (a slight random unevenness); **SIZE** (the mouse wheel too), **STRENGTH** (how fast it works)
and **EDGE** soft or hard (hard for cliffs and plateaus). Shift flattens and Ctrl smooths for a stroke
without changing the mode; `M` steps through the modes. The brush goes on while you hold it still,
and the level under the cursor is always in the status line. The ground's picture follows: a hill shows as a
shaded hill and hides what stands behind it, in the editor and in the game (the picture is saved
with the map; on a campaign terrain it becomes the map's own). Contour lines every 4 levels show
the heights while the tool is chosen; `H` shows them with any tool. **Flatten** takes the level
where the stroke begins - begin it on the track to make a plateau at the track's level - or the level
you take with Alt+click. Objects
stay on the ground; the tool leaves the track and the ground beside it alone.

An object can stand higher or lower on its own: select it and press `PgUp` / `PgDn` (or Shift+wheel,
or HEIGHT in its panel); `Home` puts it back on the ground. Train cars and rails stay on the track.
A few units and guns are always put on the ground by the game itself; the status line says so.

### 4. The main line

**Track** (key 3). Click to start a line - at the end of a rail of the map or anywhere - and every
next click lays **one rail** toward the cursor; the right button (or Esc) ends the line, and a click
of the mouse wheel deletes the rail under the cursor (Ctrl+Z puts it back). The preview shows
the next rail: **green** ends - it fits, **red** - the status line says which rule it breaks:

- one step is 71 x 50 px or its half (the rails' lattice);
- a turn goes through a **curve** and turns at most 45° (a straight rail does not join a diagonal
  directly);
- at most 6 rails meet at a point;
- a **junction**: start a new line at a point of an existing line - a branch is a straight rail and
  a curve with the same middle, a **Y junction** like in the missions.

Lay the main line the way the missions do: from **past the west edge** (scroll out over the grid)
along the map, so the trains come and go out of sight. Then a spur for the factory: a branch off
the line through a Y, a straight piece ending in a **dead end**.

`V` switches the kind of the next rails: plain, swamp (on its embankment), destroyed or destroyed
swamp. `PgUp` / `PgDn` make the next rail climb or descend (a ramp onto higher ground: each ramp
11 levels). For a line on a hill, lay it up its ramps first, then raise the ground around the top and
flatten it starting on the track. `B` picks the gravel bed under new rails. Del or the **Delete a section of track** tool removes rails.

**The gravel bed.** Once the track is laid, **Paint the ground** -> **GRAVEL BED** lays the bed
under the rails the brush goes over: **KIND** (meadow, ice, volcanic, desert - the bed `B` lays
under new rails is the same one), **WIDTH** (28 px, as the missions have it), **SIZE**,
**STRENGTH**, **EDGE**. On a map with a ground of its own the band is painted into the picture,
centred on the rails and following curves, switches and ramps, and those rails' bed decals go (the
band is their bed now); on a campaign map's terrain the bed decals are laid instead. The right
button puts the original picture back, Alt+click takes the kind, **UNDER THE WHOLE TRACK** beds
every rail of the map at once. A stroke is one undo.

### 5. Train arrivals

**Train arrivals** (key 0) opens a panel with the trains that come onto the map during the mission -
the player's train at the start, reinforcements, enemy waves. It first lists them, one arrival in two
lines of plain words ("1. OURS, ENTRY 1: LOCOMOTIVE + CARS: 2", "AT THE START, ONCE"); a click on one
opens it, **+ NEW** makes one, **COPY n** and **DELETE n** act on the one opened last. An arrival has
one thing a row:

- **WHO** - ours, the enemy, the neutrals or the pirates;
- **CARS** - in the train's order: **+ CAR** adds one at the end from the Vehicles palette,
  **TEMPLATE...** lists the authors' trains by name; a click on a car chooses it - **<** and **>** move
  it, **REMOVE** takes it away. The locomotive may stand anywhere in the train;
- **ENTRY** - **PICK**, then click the main line **past the west edge**: the train is made
  there;
- **WHEN** - **AT THE START**, **AFTER** a time, **WHEN THE PLAYER HAS NO TRAIN** or **WHEN THE PLAYER
  PAYS** an amount (those two **NOT BEFORE** a time);
- **REPEAT** - **ONCE**, a number of **TIMES** or **WITHOUT END**, **EVERY** so many seconds;
- **THEN** - ours, the neutrals' and the pirates' train **STAYS**, **DRIVES TO A POINT** or **PATROLS**
  between two points; the enemy's **ATTACKS**, **GUARDS A POINT** or **PATROLS** (the game's AI does
  it). **PICK** the points on the track, off the main line;
- **MESSAGE** ("reinforcements have arrived", for ours and the enemy's).

The strip above the title names a real problem with the button that mends it - no entry yet (**PICK THE ENTRY**),
no lights at the entry (**PUT LIGHTS**: a pair of posts the first place inside the map and their line,
which pushes the train in - the entry's line and its lights are one thing, as in the missions) or lights
on the way in without a line to them (**LINE TO THE LIGHTS**), no locomotive in the train, no point. A
wave waits while a train stands at its entry. The arrow at the bottom left, Esc or the right button go
back to the list; a click on an entry or a point on the map opens its arrival. Every change is undone
with Ctrl+Z. Maps made with an older version keep their reinforcement and the enemy's start trains of
their script (shown as marks on the track; a click on one opens this panel) until **CONVERT TO
ARRIVALS** in the panel: the start trains then stand on the map as trains with their orders (patrol or
attack), editable like any train.

### 6. The factory, the repair point, one-way lines and semaphores

**Factory** (key 4) and **Repair point** (key 5) go where the authors put them - the preview
shows where, and placing them elsewhere is refused with the reason:

- a **factory** at the **dead end of a spur**, turned the way out. It comes with its **exit line**:
  a one-way line along the spur (built cars appear in the factory and are pushed out) and a red
  semaphore at it;
- a **repair point** on a **straight piece** of track, turned along it like a gate. It comes with a
  one-way line through it (trains are pushed through and every car is repaired) and its semaphores.
  `R` or `E` (or **TURN: ROUND** in its panel) turns it round on its leg: the line then
  runs the other way and the semaphores change places.

Dragging one of them moves it, with its line, to the nearest right place; deleting it deletes the
line too. A **blocker** (the buffer stop, in the RAILWAY group of All objects) goes to the nearest
end of a track, across it, as the missions have theirs.

**One-way lines** (the strip's **Push line**): a line on the track no train may enter against its
direction, as the missions mark the entry onto the map, the exit, the factories and the repair
points. Click its beginning and its end on the track - on the map a line has a bar at its beginning
and arrows the way it runs, and the mouse over it tells what it does; then, on a selected line - the status line names its beginning, its end and what it does; in the
Select tool too a click on a line's bar or arrow selects it, and the right button opens its panel -, `T` switches
pushing on or off, `R` turns it round, `S` puts its semaphores, Del deletes it. Make two:

- the **map's entry**: from the main line's end past the edge onto the map, pushing on;
- the **map's exit** at the other end, short and pointing into the map, pushing off - so no train
  drives off the map.

**Semaphores** (keys 8 green, 9 red) are signs only - they show the way of the one-way lines and
stand beside the track, to the right of the trains they face, as in the missions: one goes on the
side of the track you point at (`R` / `E` or the wheel: the other side), and on a placed one `R` / `E`
turn it round - it goes across the track to the trains of the other way; the status line and its
panel (FOR TRAINS) say whose it is.
`T` switches the colour. A semaphore you place gets its own one-way line at once, as the missions'
semaphores have them, and the preview shows it: a **green** semaphore's way in is drawn green, a **red** one's stop as a short red mark in front of it, ending at it: a **green** one
lets the trains it faces in (the line comes from them and pushes them past it), a **red** one stops
them at it (the line comes from behind it, and they may not go against it). A green and a red one
across the track from each other share one line - the map's entry, if you put them where the main
line comes in. Turning it round, `T` or moving it changes the line, deleting it deletes it (unless the
other one of a pair still has it). One put at an end of another line belongs to that line and gets none; `L` (green) and `Shift+L` (red) give a
selected semaphore its own line all the same, and its panel's LINE row takes it away (NONE).

**What a factory builds**: the right button on a factory, **WHAT IT BUILDS...**. For your factory
(and a neutral one, whose list is yours once you capture it) click the units it may build - they keep
the game's order and keys F1-F12, [ and ]; for the unit clicked last **IN THE LIST** sets from when (at
the start or after a time) and **AT MOST** how many of them the player may have.

**What a factory builds by the script - its plan**: the enemy's factory shows the trains its AI
builds (only the enemy has an AI), in the second tab **TRAINS IT BUILDS** of yours, a neutral one or a
pirate one the trains the map's script orders it to build. A list of them, and a screen for each,
like the Arrivals panel: the **TRAIN** (its cars in order - the locomotive anywhere -, **+ CAR**,
**TEMPLATE...** the authors' trains), **WHEN** (**AT** a time, **AFTER ANOTHER** - so long after the
train before it in the list is made -, or **WHILE** a condition holds, e.g. the player has fewer
trains than n, not before a time), **REPEAT** (once, n times or without end, every so long after the
last one) and **THEN** (the enemy's attacks, guards a point or patrols; the others' stay, drive to a
point or patrol). A train of yours, the neutrals' or the pirates' is built car by car when the factory
is free and its side has the money for the next car (the price under each), whatever the factory's
list; the enemy builds one at a time while fewer of its trains attack than **AI...** allows. **AI...**
has the authors' other settings of the enemy, a line on each: whether a lost start train is built
again and after how long, the start trains' raids, when raids begin and how long they last, the clever
attack. Everything except a train at a time once needs a map that runs arrivals (older maps: CONVERT
in the Arrivals panel).

**Orders of a train**: the right button on a train, **ORDERS**: yours, the neutrals' and the pirates'
**DRIVE TO...** a point or **PATROL...** between where they stand and a point; the enemy's (with a gun car -
its AI makes a bare locomotive a ram) **ATTACK**, **GUARD...** a point or **PATROL...**. The point is a click
on the track. ORDERS are there only in a map that runs arrivals; a map made with an older version has
**PATROL** only, until **CONVERT TO ARRIVALS** in the Arrivals panel.

**Capturing**: a neutral factory or repair point becomes yours when one of your trains comes close to it
(a fort or a mining plant once its guns are gone).

**Neutrals and pirates**: the neutrals (army 2) never fire first. When a side hits a neutral gun, fort
or train, it and the neutrals that see it answer that side only - to the end of the mission, and that
side may fire at them then. The panel of an armed neutral object has **IT FIRES: WHEN ATTACKED** (as it
starts) or **NEVER**. The pirates (army 3) fight every side, and every side fights them (**IN A FIGHT:
AGAINST ALL SIDES**). A map of the editor plays so; the campaign's missions keep the game's old rules.

### 7. The enemy

- **Fort** (key 6, the enemy's): it goes **onto the track**, turned along it - it is a gate the
  trains drive through (`R` or `E` turns it round there). It comes armed: its panel (right button) sets the gun in each socket - a cannon or
  missiles. The fort itself cannot be destroyed; its guns can.
- **All objects** (key 7) opens the palette with every object the missions use, by theme (meadow,
  forest, desert, ice, volcanic, swamp, park, pirates, city, industry, railway, flags and crates,
  wrecks, units and creatures, effects): click one, then place it. Under the icons the **ARMY** row
  sets whose it will be - AS IN THE MISSIONS, OURS, ENEMY, NEUTRAL or PIRATES (the Vehicles palette
  starts on ours); `A` while placing does the same. Here are the **mining plant** (one gun socket),
  the citadel, walls, buildings, robots. A **wreck** (a burnt
  cart or car) put or dragged near a rail goes onto the track, along it (`R` turns it round), as the
  missions have them; elsewhere it stays where you put it. In the game a train runs it over and
  crushes it - it does not stop the train.

  ![All objects](img/editor-palette-en.jpg)

- **Citadel** (pirates): it stands over a **diagonal track**, with its tower, the grates of its gate
  and its corkscrews, as one object. In the game, while its tower stands, its gate stops and hits whatever of another army than
  the tower's comes near (the neutrals aside); without the tower it is harmless.
- **Charybdis** (swamp): it goes **on the track** - the track runs through its mouth -, with its
  eye, teeth and guns, as one object. It pulls in the trains of other armies that pass it and eats
  yours; its guns sleep until it is hurt. Kill
  its eye to destroy it.
- **Fly nest** (units and creatures): its panel chooses **GREEN, IN WAVES** (waves of green flies)
  or **GREY, ON GUARD** (grey flies when your trains come near, a swarm when it is destroyed).
- **Mine** (units and creatures): it goes on the track, in the army of the palette's row. The editor
  marks every mine with its army (MINE: ENEMY); in the game only its own army sees it - a train of
  another army sets it off, its own trains stop in front of it.

- **Trains**: the strip's **Vehicles** opens the cars - locomotives, combat cars, supply cars
  (ammunition, repair car, tankers), civil cars, obstacles. A car goes on the track under the
  cursor; placed at a train's end it is coupled to it. Make the enemy's train: a locomotive and a
  gun car, then `A` on it for the enemy's army. `U` uncouples a car, `C` couples to another train,
  `R` turns the train round, a drag moves it along the track. A train needs a **locomotive** to
  move.
- **A patrol**: the train's panel → PATROL → to a point..., then click the far end on the track.
  The train drives there and back.
- More enemy trains can be set in the mission settings (step 10): trains at the start and trains
  the enemy's factory sends later. For those the enemy needs a factory: put one (key 4) at a dead
  end of its own and `A` to the enemy.
- A **money crate** (strip) lies on the track: a unit of the player that takes it gets 100.

Remember the rule of the game: the factory, the repair point and the fort stand **on the track or
right next to it**.

### 8. Decorate

The strip's **Decals**: grass, bushes, flowers, stones, wrecks... `[` `]` (or a click on the
entry) picks one; a click puts one, a drag scatters them in the radius (wheel, `+` `-`), never nearer than
the spacing (`<` `>`). Shift+drag takes them away. Trees, houses and the rest are in **All objects**.
A **Region** (strip): drag its rectangle; its panel sets fog (mist, dust, haze, glow) and splashes.

### 9. Look at it working

Menu → **QUICK TEST, NO SAVE** (or `Ctrl+R`): the map runs in the editor as it is now, saved or not, as it
will in the game - the trains drive, the guns shoot, the arrivals come. Nothing is asked: your own map
is saved first, and POLIGON, the template, a campaign map or a new map without a name yet run from a
hidden copy, so they stay as they were. `Ctrl+R` brings the editor back with the map as you left it -
the edits not saved still not saved, `Ctrl+Z` undoing them as before -, and so do the game's own `Esc`
menu (ABORT GAME → BACK TO THE EDITOR or YES) and the end of the level (won or lost).

### 10. Mission settings and texts

Menu → **MISSION SETTINGS** (or the gear in the strip):

- **Title** on the custom maps page and the **briefing's header**;
- **Money at the start**; the **time of day at the start** (day, evening, night, morning - then it
  goes on, 5 minutes each);
- **Weather** - rain, snow, clouds, storm clouds or meteorites - and **how often**: never, now and
  then (it comes and goes, about half a minute each) or all the time;
- **Lightning** - flashes in a clear sky; with rain only while it rains, none with snow, clouds or
  meteorites;
- **Enemy attackers at once** - how many enemy trains attack together;
- **Train arrivals** - their count and the button to their panel (step 5); a map made with an older
  version has the **player's reinforcement waves** and **their train** (VIDs, e.g. `5,20`) here
  until it is converted;
- **Enemy trains at the start**, one a line: `type x y` (it fights from there) or `type x1 y1 x2 y2`
  (it patrols between two points of the track). The types are `light_`, `medium_`, `hard_`,
  `very_hard_`, `diff_` or `defence_` with `gun`, `mortair`, `rocket`, `air`, `fire` or `desant`, e.g.
  `light_gun`. The points are screen points: the **Push line** tool shows the track's point under the
  cursor in the status line;
- **Enemy factory's plan**: how many trains it has; **Plan...** opens an enemy factory's panel, where
  they are set;
- **Objectives**, the **win** and **loss** texts and the **briefing** (a line `---` starts the next
  page). They are written in capitals, 44 characters a line, when the map is saved.

The mission is **won** when the enemy had units and has none left (its trains, its factory, the
fort's guns), and **lost** when the player has no units left and no factory.

### 11. Play it

Menu → **PLAY THE SAVED MAP**: the game starts again with your saved map as a mission - the briefing in the Esc
menu, the reinforcement arriving, the enemy attacking. When it ends, the game comes back to the
editor on your map; to go back at once, press Esc, the exit button (ABORT GAME) and **BACK TO THE
EDITOR**. Unsaved changes are asked about first.

### 12. Save

`Ctrl+S` saves the map into its file; a map that has none yet (a campaign map, a new map) asks for
the name - and so do POLIGON and the template, which an update would overwrite (the dialog proposes
`poligon1`, `map1` ... and its title says why). Menu → **SAVE MAP** (Alt+F2) always asks - to save under another name. A map saved as
`maps\<name>\<name>.map` is written with its script and texts and appears on the **custom maps** page
under its title. Open it again with **LOAD MAP** (Alt+F3). Every save shows a large **SAVED** with the
map's file in the middle of the view for a moment, with the menus' click.

### 13. Save, export and share

**Export:** Menu -> **EXPORT MAP** packs the current map - the map, its painted ground, its script, texts and briefing - into one `.zip` file; the save dialog starts on the Desktop. Save your edits first (the editor offers it). Campaign maps and the empty template cannot be exported.

**Import:** Menu -> **IMPORT MAP** takes such a `.zip` (also one packed again by Windows) and puts the map on the **CUSTOM MAPS** page. When you already have a map of that name, the editor asks: **Yes** overwrites yours, **No** keeps both (the new one comes as `<name>_2`), **Cancel** stops. Then it offers to open the map in the editor.

A map is its folder: `maps\<name>\` in the game folder holds the map (`<name>.map`), its painted
ground (`<name>.vid`), its script (`<name>.lgc`) and its texts. To give it to a friend by hand:
send the whole folder; they put it into `maps\` of their game folder and add two lines at the end of
`maps\custom.lst` (Notepad): the folder's name and the title in capitals, e.g.

```
bridge_1
BRIDGE OF NO RETURN
```

## 7. Keys and tools of the editor

**The bar** (slots 1 - 0):

| Key | Tool | |
|---|---|---|
| 1 | Select | select, drag, turn, army, panel |
| 2 | Delete | a click deletes; Shift+click on a rail: its section |
| 3 | Track | lays rails (V kind, PgUp/PgDn ramp, B bed) |
| 4 | Factory | at a dead end, with its exit line |
| 5 | Repair point | on a straight piece, with its line |
| 6 | Fort | on the track, armed |
| 7 | All objects | the palette |
| 8 / 9 | Semaphore green / red | beside the track |
| 0 | Train arrivals | the panel of the trains that arrive |

**The strip** (above the bar, left to right): Delete a section of track, Decals, Push line,
Mission settings, Ground height, Money crate, Vehicles, Paint the ground, Water and lava, Region.

**Keys:**

| Key | |
|---|---|
| R / E, wheel | turn: a step of the object's directions (R right, E left); turn round a train, a wreck on the track, a semaphore beside the track and, in the Push line tool, the line; a factory, a repair point and a blocker follow their track (help page 15) |
| A | next army |
| T | semaphore red / green; on a line: pushing on / off |
| L / Shift+L | a line at the selected semaphore: lets in / keeps out |
| Del / Shift+Del | delete / a whole train or section |
| Ctrl+Z / Ctrl+Y | undo / redo |
| U / C | uncouple / couple a car |
| `[` `]` | the decal, the texture |
| `<` `>` | the decals' spacing |
| wheel, `+` `-` | the brush's size (Ground height, Decals, Paint, Water) |
| M | the ground brush: raise, lower, flatten, smooth |
| H | the ground's heights (contour lines) with any tool |
| B | the gravel bed of new track |
| V | the kind of new rails |
| PgUp / PgDn | the selected object up / down by 4 (Shift: 16); Track tool: the next rail climbs / descends |
| Home | the selected object back on the ground |
| W | water, swamp or lava |
| S | the selected line's semaphores (Push line tool) |
| Shift (with a brush) | take away |
| Esc, right button | back to Select; end a line of track (in the Track tool a wheel click deletes the rail under the cursor) |
| Ctrl+R | run / hold the level |
| F1 | help |
| Ctrl+S | save the map |
| Alt+F2 / Alt+F3 | save under a name / load |

## 8. Questions and problems

- **The game does not start, or Setup does not find Locoland.** Locoland must be installed through
  Steam (any language).
- **Where are my maps?** In `maps\` of the game folder (C:\Users\<you>\Games\OpenLocoland), one folder each; the
  list of the custom maps page is `maps\custom.lst`.
- **Something went wrong.** The game writes `error.log` and `LocoFix.log` into `logs\` of its folder; send them
  with a description of what happened.
- **Full screen.** It works, but windowed mode is the tested one; if the screen stays black, untick
  "full screen" in the launcher.

---

OpenLocoland is an unofficial project; Locoland belongs to its rights holders (Gromada, ESDigital
Games, Alawar). It uses dgVoodoo2 by Dege for the graphics on current Windows.
