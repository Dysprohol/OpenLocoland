# Editor mode: editing objects and laying track

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

**Editing objects** (`src/LocoFix/editor_edit.cpp`). On the held level LocoFix does the editing
with the engine's own pieces (scripts and object updates are off, so nothing else would):

| Action | Keys and mouse | How |
|---|---|---|
| select | a click on an object (by class, `pick_class`: the train cars and units - every Unit class, the guns of the sockets and their hosts - before any other object, those before the rails, a region's inside last; within a class the smallest box under the cursor, so a small decoration beside or under a car no longer takes its click - since 2026-11-05, before the smallest box of all won; a gun in a socket still gives its host; a region inside its rectangle only when not even a rail is - on its frame as any object; since 2026-11-05, before only the frame took it, so a region was hard to pick or delete; the Delete tool's click and sweep and the Region tool's right button, which opens its panel, the same) | the selection circle VID 460 ("Select Unit Circle", its outline frame) is drawn under it and the guns in its sockets |
| its panel | the right button on an object (Select tool) | the object's panel (below): army, turn, delete, a semaphore's colour and line, the gun in each socket |
| move | drag a selected object | `Object_MoveTo` (what `ACT_CHANGE_COOR` 63 calls); the height follows the ground (`World_GetGroundZForVid`), keeping the object's own height above it |
| height | PgUp / PgDn by 4 (Shift: 16) or Shift+wheel, Home back on the ground; HEIGHT: LOWER / RAISE / ON THE GROUND in its panel | the object and its guns `Object_MoveTo` by the change of its height above the ground (z less `World_GetGroundZForVid` under it), -32 to 400; shown in the status line ("height above the ground: N") and after "selected"; saved as its SPR z ("The relief and the heights of objects" below) |
| turn | R / E, the mouse wheel, Edit > Turn right / left or the panel's TURN | by what is turned (`turn` in `editor_edit.cpp`, help page 15): `Object_SetDirection` by one of the VID's `nDir` directions, R right and E left - the guns in its sockets go to their new places, a semaphore's lamp halo (VID 154) is turned with it so the light spot it links (155) moves with it, a decal's directions are its looks, an object of one direction (the citadel, a crate, a region) says so; turned round, R and E alike - a train (`Train_Reverse`), a wreck on the track, a post beside a track (across the track, below), in the Push line tool the selected line; turned by its track - a factory, a repair point (drag it along the track; the panel's TURN row says BY ITS TRACK); a rail is not turned (laid again), and the Track tool turns nothing. Before placing, R / E turn the preview: a step of the VID's directions (the direction placed is one of its frames: a direction left from an object of more directions is rounded to them), round for a car or a post, and an object the track turns says so |
| army | A (0 -> 1 -> 2 -> 3: ours, enemy, neutral, pirates) | `ACT_SET_ARMY` 97; the armies' colours as in play (below) |
| semaphore red / green | T | red is the command `10,0,0,0` (MAP_RULES.md 7), shown with that animation - also the red posts of a loaded map, whose first command never runs on a held level (the command stays as it is); a post's own line changes with it ("On the track") |
| delete | Del, or the Delete tool of the bar (the filled circle marks what a click takes, the cursor is the game's delete cursor; its status line says so) - a click, or a sweep: with the button held every object the cursor passes over goes as a click would take it (the way between two mouse messages walked in 4 px steps, nothing under the bar, a panel or the rows; `start_sweep` / `sweep_to`, captured like the ground tools' strokes), and the sweep's deletions become one step of the history when the button is let go (`merge_steps`: each object its first state before and its last after, the push lines and arrivals from the first step to the last); Shift+Del or Shift+click on a rail: its section, as far as one rail continues it at each end (up to a junction or an end) - the strip's "Delete a section of track" tool, which did only that, is gone | `ACT_UNDO_REMOVE` 132 - made for the authors' MapEdit: the object leaves the layer lists (not drawn, not updated, not saved; a train car leaves its rail points and couplings); a rail also leaves the rail network (below) |
| undo / redo | Ctrl+Z / Ctrl+Y | every change keeps the state of the objects it touched before and after (position, direction, army, present, red); undo puts the "before" back - a deleted object returns with `ACT_UNDO_INSERT` 133 - and redo the "after"; the last 500 steps are kept, at most 256 MB of them (below) |
| place | the bar at the bottom: 1-0 or a click picks an entry, a click on the map places it, Esc or the right button go back to selecting | World `CreateObject` at the point under the cursor on the ground - over other objects too ("Placing over objects, the track as a help" below); a preview of the object (its own sprite in the chosen direction, `Graphics_DrawVid`) follows the cursor, the status line says where it goes |

The bar looks like the game's state bar and is drawn with its pictures over the scene, after
`Graphics_DrawScene` (`Graphics_DrawVid`, in front of everything): the bar's base (VID 450) in
the middle of the bottom edge with its side tiles (VID 938, as LocoFix fix 23 lines them up) out
to the edges of the view, the first ten entries in the base's ten slots (where the unit bar shows
its units, 41 px apart) and the others in the factory panel's strip (VID 497) a slot higher, the
chosen one in the slot frame (VID 456) and a gold frame on its icon's edge, the one under the mouse
a thin light one (the lit icons hide the slot frame; a lit icon draws its brightening as specular light,
which the bar turns off after its icons - left on, every `Graphics_FillRect` after them came out
white), each with its key in the factory panel's small
font (VID 8, right-aligned at the icon's lower edge as the panel's F-keys). The icons come from the
state bar's atlas where it has one (VID 455: the cross 3, the factory 10, the wrench 2, the cannon
36, the locomotive 11) and otherwise from the editor's own atlas (VID 939, `vid\939edit.vid`, made
when the game is assembled, below): the arrow, the track, the mine, the fort, the
semaphores green and red, a decal (unused now), the gear of Mission settings (it had the state bar's frame 0,
which the game shows for no option); both lit green as the game's state bar (the menu gamma, "The
armies' colours"). The status line at the top is in the mouse tips' font (VID 6,
each character in its orange box - the look of the game's tips): the tool or the entry under the
cursor with its key, the undo and redo counts, "not saved" and the last message. A text is drawn
the way `Text_Draw` writes one: a frame of the font VID per character (its code), the VID's
width apart. A click on the bar's plates does nothing on the map under them. Its entries are laid
out by kind, so that a map maker finds a thing where he expects it: the base's ten slots (keys 1-0)
hold the tools that act on what is there and the things of the track that are placed - Select (1),
Delete (2), Track (3, below), the Push line tool (4, below), the factory 102 (5), the repair point 104
(6), the fort 162 (7, army 1), a green (8) and a red (9) semaphore 152 and All objects (0: the
palette, below) - and the strip above the ground's tools, the rarer things placed and the two panels
last: Ground height, Paint the ground, Water and lava, Region (below), the mine 165 (army 1, placed
as the fort is: onto the track, with the missions' guns in its sockets; its icon the atlas' grey
picture of it), a money crate (VID 169, army 3), Vehicles (the cars' palette, below), an empty slot
(free for later; the decals are the palette's DECORATIONS category - the 61 decals of `objects.res`,
`GroupDecals` of `game/editor/palette.tsv`, one list for the strip above the bar and the palette; [
and ] from any tool put the All objects entry on it, `choose_decal` -, and a section of track is
deleted with the Delete tool's Shift+click), Train arrivals (a panel, "Train arrivals" below) and
Mission settings (the dialog of the File menu, below).
The camera of a held level goes 300 px past
every edge of the map (`camera.cpp` widens the bounds `SetPosition` clamps to while the level is
held), so the bar does not hide the bottom of a small map and the parts of the track out of
sight - the main line's ends, the reinforcement's point - can be laid and picked. The rules of MAP_RULES.md the editor applies: a new
unit gets the behave and the build list of an object of the same VID on the map (map_tool.py
place copies them the same way; one of the same army first - the enemy factory of a mission has no
list; with none on the map, the state of one on a mission's map, from the palette's table), a new
player's factory without such an object the game's 14 factory units; a fort, a mining plant or a
citadel comes with the missions' most common guns in its sockets (MAP_RULES.md 6), which move, turn,
change army and are deleted with it; a reinforcement point is put on the nearest track by
`Engine_Construct` itself; a factory, repair point, fort, mining plant or citadel near a rail snaps
onto the track ("On the track" below), elsewhere or with Shift it stays where it is put. Rails are not moved or turned (a rail is laid again instead); train cars move with their train along the track (below) - a car
stands on its rail points).

Checked on the sandbox (`test_game.ps1 -Editor` with the steps of "Working with the code"):
select, drag, turn and change the army of the player's factory, undo twice and redo, a red
semaphore made green and a green one red, a fort with its guns, a factory, a semaphore, a
reinforcement point on the main line and a decal placed, the repair point deleted, undone and
deleted again, saved and compared with `map_tool.py unpack`: 191 objects - the 185 of the map
with the factory moved and turned, the repair point gone and the six new ones - GRID, GROU and the OBJ
record unchanged, the semaphores' commands as set, the new factory with the 14 units; then File >
Play the map: the edited map plays with its interface, and `error.log` has no `LOGIC`, `R_MAP`
or `RAIL` line.

**Placing over objects, the track as a help** (`editor_edit.cpp` "where the object to place goes":
`plan_place`, which the preview and the click share). Placing asked the engine's `AskPlace` (161) and
refused a place that another object with a common `moveMask` bit took - a tree against a wall, a flag
on a roof, a fort over the bushes beside the track were refused, while the missions are full of such
overlaps and the engine minds none of them on a map. Now an object goes where it is put, and a drag
was never asked. The track is a help, not a rule: an object of the track ("On the track" below) near
a rail snaps onto it within its reach, as before; farther away, or with **Shift** held while placing or
at the drop of a drag, it stays where the cursor is, turned as chosen (R / E turn the preview of such
an object too), and the status line says which - "onto the track (Shift: where the cursor is)",
"where the cursor is (Shift)" or "where the cursor is - <the rule's reason>"; a factory or repair
point placed so gets no push line, a post a line only if a track is near. Two refusals stay, the
places the game itself would undo: a car off the rails (`place_car`) and an object in a **factory's
door** - `Depo_ReleaseUnit` (0x405290) deletes every object that `AskPlace` finds in the way of a unit
it has just built at the factory's middle, so an object whose box overlaps that of the factory's units
there and shares a `moveMask` bit with them (`factory_door`: the cars' 0x8) is refused, placed or
dropped, with "the factory's door - the factory deletes what stands there when it builds a unit" (a
locomotive placed at the dead end inside the sandbox's factory; a bush there, without the bit, goes).
A mine keeps its rule too: onto a rail point or nowhere (below). Checked in the game on POLIGON: a
factory placed 200 px off the track (where the cursor is, no line), a fort snapped across the main
line, another with Shift beside it at its own direction, a tree placed into the fort, the locomotive
refused in the factory's door and the bush put there.

**Placing helps: the grid, onto the track, the height, the turn, scattering** (`editor_edit.cpp`
"placing helps", `plan_place`, `decal_step`; GitHub issue #5, the suggestions of paulevsGitch). While an
object of the palette or a decal is to be placed, a row above the strip holds them, and the status line's
third line names them; each is a key as well, and all start off (as before):
- **GRID** (Ctrl+G; the row's GRID: OFF / ON, - n + px, 4-256, 32 at first): the object goes to the
  grid's nearest point of the map (x, y), the preview with it and the grid's points around the cursor
  dotted, the one it takes lit; the track's objects snap from there as before. A decal placed one at a
  time and a paste go to the grid too.
- **ONTO TRACK** (N; Place tool): an object that is not one of the track's own (those snap anyway, "On
  the track") within 30 px of a rail link on the screen (`track_spot`, as the wrecks) goes onto the
  link's line at the rails' level; the arched bridge 776 at its height under them as the missions have it
  - all 133 of l05, l09, l12, l13 and l15 lie within 45 px of a rail at the rail's z - 81 (the points' height
  - 91; z -15 under rails at 66), and a placed one matches (a click on l02's line at z 0 put it at -81). The turn stays the one chosen: the missions'
  bridges follow no one rule of direction against their rails (on straights across 0, on l09's diagonals
  160 and 192), so R / E turn it. Shift places where the cursor is.
- **HEIGHT** before placing (PgUp / PgDn by 4, Shift by 16, Home 0; the row's HEIGHT: - n +, -32 to
  400): the preview stands at that height above the ground and so do the objects placed (and the decals
  of the Decals tool) until it is changed; not for the VIDs the game always puts on the ground, the cars,
  regions, flags and the track's objects. Before, PgUp / PgDn worked only on the object just placed (the
  selection); they still do in the Select tool.
- **TURN: AS SHOWN / RANDOM** (Q): the next turn is drawn in advance (`roll_turn`), so the preview
  shows it, and drawn again after each placing; R / E draw another. For the objects whose turn is theirs
  (not the track's objects or a flag in the wind).
- **PLACE: ONE / SCATTER** (X), per VID (`g_scatter`, for the session): the decals of the ground's cover
  scatter by default (`ground_cover`: not the beds, wrecks, buildings, track and industry pieces of
  `NOT_COVER`) and everything else goes one at a time - before, every decal scattered on a drag, also a
  wreck or a paving stone. A decal one at a time: a click puts it as shown, a drag adds none (Shift+drag
  still takes away). An object that scatters (the decals and the plain objects - not units, buildings,
  the track's objects, wholes, hosts of guns, flags, wrecks, regions, cars: `scatter_able`) makes the
  Place tool's click and drag the Decals tool's stroke (`decal_step` with the stroke's VID and army): the
  first where the preview shows it, then 1 + the radius of them a step in the circle (the wheel, + and
  -, the row's SIZE), none nearer than the spacing (< >, SPACING) to one of the same VID, each in a look at
  random; Shift takes that VID away in the circle; one step of the history.
Checked in the game on a copy of mission 2: the arched bridge snapped onto the left line (1029,724,-81),
Ctrl+G and two flowers (220) on the grid (1216,832 and 1216,864), PgUp three times - the next one 12 above
the ground -, Q - placed at direction 16, R drew 28 -, X and a drag scattering 20 flowers, a cart
wreckage decal (93) dragged placing 1, round bushes (936) dragged scattering 15, PgUp on a selected fort
in the Select tool lifting it by 4, each undone.

**Copy, paste, duplicate** (`editor_edit.cpp` "copy, paste, duplicate"). Ctrl+C copies the selection - the
object with its guns in their sockets and its pieces (`group_of`) -, Ctrl+V puts a copy with its first
object at the map point under the cursor (on the grid when it is on), Ctrl+D one beside the original
(its width + 8 px to the right, whole steps of the grid when it is on); each is one step of the history,
and the copy is selected in the Select tool so it can be dragged at once. A copy keeps the original's
VID, turn, army, height above the ground, a post's red, a region's size, flags, fog and splash, a unit's
behave and a factory's build list (its `State`, put on the new object through `apply`); a building gets
its footprint flattened as a placed one. It does not keep links: a pasted factory or repair point has no
push line and a post no line of its own (dragged onto the track they snap and get them), and rails (the
Track tool lays track) and train cars (they stand on their rails) are not copied - the status line says
so. Checked: a fort placed across mission 2's line, copied with its two guns, pasted and duplicated
(each "and 2 more"), both undone.

**The history's limits** (`editor_edit.cpp`, "object state and the undo history"). Undoing and
redoing far crashed the editor: a train car's state kept the pointers to the rail points its two ends
stand on (`RailPos`), but undo and redo take rails away and lay them again - `RailDot_Release`
deletes a point no rail uses any more and `RailMap_AddDot` makes a new one -, so a car put back by
`ACT_UNDO_INSERT` (which runs `Engine_OccupyDots`) wrote into a freed point and `Engine_UpdatePosition`
read from one. A car's ends are now kept by the places of their points and of the link's other end
and found again when the car is put back (`RailMap_FindDot`, `RailDot_FindLink`, as `RailPos_Load`
does); a point that is gone leaves the end empty instead. The history is bounded too: a stroke of the
ground's tools keeps the rectangle of the picture it changed before and after (10 bytes a pixel, tens
of MB for a stroke across a big map) and the game has 2 GB, so at most 500 steps and 256 MB of them
are kept, the oldest going first (`trim_history`; "the history keeps the last ..." in `LocoFix.log`).
Checked on the sandbox: the reinforcement's locomotive deleted, the main line's section under it
deleted, 4 rails laid, three strokes of Ground height, a factory and a fort placed, then undone past
the start and redone past the end three times - the build before crashed at the undo that put the
locomotive back (`EXCEPTION!`, EIP 0x40a241 in `Engine_LeaveDots`), this one does not, and the
locomotive stands on its points again; 510 turns of a fort left 500 steps.

**Laying track** (the Track tool, key 3; `src/LocoFix/rail_rules.h`). The first click starts a
line - at the end of a rail of the map near the cursor, or at the point of the rail lattice of
the nearest rail end - and the next click lays a section of rails toward the cursor (`run_to_cursor`)
and ends the line, so the click after it starts a new one (at a rail's end within 20 px: going on is
one click more; before, every click went on from the last rail, and a click meant to start elsewhere
laid rails there): within a
step of the line's end (71 x 50) the step of the lattice (one of the 12 shapes of VID 610, 71 x 50 px
or their halves) whose end is nearest to it, any of them; farther off a run toward the lattice point
nearest to the cursor - each rail the one of the three that go on from the last (straight, a curve to
either side) whose end comes nearest, while that brings it nearer, at most 40 - laid with one click as
one step of the history, so a long straight or a curve round is one click. **The tool lays what is
clicked** (the user's choice): over other rails, across their joints, as a dead end, turning sharply,
at another height, where the same rail lies already - the engine takes all of them (a rail laid twice
shares its points, adds no link; rails at two heights do not link; a dead end stops the trains, a sharp
turn reverses them; the missions have each of these). The rules only choose which shape a step means -
one joining the line's last rail when there is one (`rail::plan` / `plan_track` with `lay`). The one
rail not laid is one that would give a point a 7th link, which the engine cannot take ("Too many
links"); the status line says only that ("this point already has 6 rails"), its ends red - the run
stops before it. The preview draws the rails of the click always (the 6-link one too), wherever the
cursor is, also over the bar - before, a step that broke a rule was not drawn at all and the
preview seemed to vanish with the cursor far off. The right button or Esc cancels a line started (a second one the
tool). A click of the middle button (a drag of it pans the map, `mouse_pan.cpp`) deletes the rail under
the cursor (of the rails whose box holds it, the one whose middle is nearest), with its bed and what
stands on it (below), as one step of the history; when it was the last rail of the line being laid,
the line goes on from the point before at that point's height. (For a while the right button deleted
the rail instead: a right click meant to end a line took its last curve away and the next clicks of a
star junction - several curves from one point - were laid from the wrong point and refused.) Before
this, every click was checked by all the rules of a whole line: a line across POLIGON's main line at a
joint was refused there ("does not join the rails at its end") though `map_tool.py track` laid the
same line in one go, and so were the authors' own dead ends, duplicates and joints at two heights.
Checked in the game on POLIGON at the joint 993.5,1375: four curve branches, a vertical across the
joint in one click (a run of 4), three ramps from it (PgUp) until its point (1011,1375) had 6 links
(the test command `D`), the 7th refused with its ends red, a curve laid again (shared points); a run
of 8 rails in one click, then 3 with a curve; saved, `check-track` listing the dead ends, the
duplicate and the joints at two heights; run with a locomotive of ours patrolling east through the
cluster, `error.log` without `R_DOT`, `R_MAP` or `RAIL` lines (only the four `ENGINE ...: 0 rail 1`
lines the untouched POLIGON has too). The rules
are those of `map_tool.py track` (MAP_RULES.md 3) in C++: `rail_rules.h` mirrors `plan_track`,
`rail_candidates`, `rail_ends_at` and `track_cells` of map_tool.py, and `tools\test_rail_rules.py`
compares the two on generated cases (see "Working with the code"). A rail is created with World
`CreateObject` at the shape's direction (`RAIL_DIRECTION`) and the line's height: `Rail_Construct`
runs `ACT_CHANGE_DIRECTION`, which puts its two points into `g_railMap` and links them to the
rails at its ends, as loading does. The rails of the map are read with their ramps and point
heights (`rail::rail_of`), so a line starts at a ramp's end as well, and the rules see only the
rails whose points are at the line's height. A line continuing a rail takes the height of its
rail point there (and its VID); a new one
the ground under its first rail - the rails stand 4 below the ground - and the ground is flattened
only where the rail lies on it or under it: a cell the rail crosses whose ground was at most
`FLAT_SLACK` 4 below the track's level, or above it (a cutting), gets the level, the cells around such
cells half the way, from their height before the line (the same rule as `map_tool.py track`'s, so a
flat line laid in one go gives its grid); written with `World_SetGroundZ` and undone with the rail. A rail above the
ground - a ramp's climb, a raised line, a dip under a line - leaves the GRID as it was (2026-11-05, the
user's rule): raising it to the track's level made a ridge in the picture's relief (`relief_for`), its
bed's edges ragged, while the embankment's look is the bed's - the decal, baked into the picture in
play, or the Paint tool's GRAVEL BED band, which follows the rail's height and a ramp's climb. (The
authors' l11 does have the GRID 6 above its raised rails too - z 105 and 116, a band of 3 cells over
ground 101-105 - and 10-15 above its ramps' z; their picture was painted beforehand and is never made
from the GRID, so it shows no ridge.) Checked on POLIGON (flat, GRID 70): two ramps up, four rails at
z 88 and two down - 20 cells changed, 1-4 up at the two ramps' feet, none under the raised part; the
older build's line beside it for comparison (`build\shots\_ba.png`: the ridge's ragged lower edge
gone); a locomotive of ours patrolling over it in Run the level, up, along and down and back. Extern 81 `ReCalcGridZ(int unit)`, which `export.lgc` describes as recalculating the
grid with a unit's size, is not used: it is not implemented in the exe and is about one object's
footprint, not the cells along a track.

**The ground under rising rails** (the Track tool's G, the row's GROUND UNDER RISING RAILS: AS IT IS /
SOFT / HARD; `g_raise_ground` in `lay_step`; issue #5). AS IT IS, the default, is the rule above: a rail
above the ground leaves it. HARD raises it the way a cutting is lowered: the cells the rail crosses go up
to its level (along a ramp, sloping with it), the ring around them half way (`rail::ramp`), so a climbing
line stands on an embankment of the grid; SOFT adds a slope of `RAISE_RING` (3) cells round the raised
cells, a smoothstep from the rail's level down to the ground. Only up: cells above the rail keep the
cutting's rule, cells of the line already laid stay. One step of the history with the rail, the picture
following where it is the map's own (`relief_for`). Checked on a copy of POLIGON (flat, 70): three lines
of four rails climbing from 66 to 99 - SOFT changed 70-82 cells a rail, HARD 28-32, AS IT IS none; the
relief shows the two embankments, the wide and the short slope.

`Rail_Action` has no case for `ACT_UNDO_REMOVE`/`INSERT`, so a deleted rail would leave its two
points and their links in the rail network and trains would go on driving there. The editor
removes the rail's own link and releases its points (`RailDot_Release` deletes a point no other
rail uses, with its links - a junction's shared point stays with the other rail's links), and
undo runs `ACT_CHANGE_DIRECTION` again. Rails deleted (Del, the Delete and Section tools, the
Track tool's right button) take along what depends on their points, in the same step of the
history: the train cars with an end on one of their links (an earlier version refused the deletion
instead, which left a car hanging in the grass when it did go), the money crates on a point that
goes, the editor's push lines with an end on a point that goes or whose way is cut (`SetPushLine`
would take the nearest other point - the line would jump onto other track; the script's own lines
are only counted in the status line), and an arrival's entry or task point there
(`edit_arrivals_track_gone`); the semaphores beside them and a blocker stay. `RailDot_RemoveLink`
reorders the links of a point next to a deleted rail, which a car's rail position indexes, so the
editor keeps with each car the point its ends' links lead to and finds the link again after the
deletion and after an undo or redo (`pos_relink`). Checked with the rail points logged before and after (the test
command `D`): a rail of a new loop deleted - its two points gone, the neighbours' links to them
too - and undone; the loop's section (8 rails, 14 points) deleted with Shift+Del and undone, the
junction's point keeping the main line's links.

Checked on the sandbox: a branch from the bottom main line through a curve sharing its centre with
the main line's rail (a junction), down, east and back up to the main line at another junction -
a loop of 8 rails; saved, `map_tool.py check-track` finds nothing, `unpack` shows the map with
the 8 rails added and the GRID unchanged (the ground there is flat); played with the sandbox's
`SANDBOX_DRIVE_TEST` driving the reinforcement to points on the loop: it went from the main line
through the first junction onto the loop, round to the second junction and back - `error.log`
without `R_DOT`, `R_MAP` or `RAIL` lines.
**Debug keys.** With `DebugMode=1` in `Locoland.cfg` the game has its keys (`GameWorld_Run`).
While the level is held only O (ground height grid and object debug, World flags 0x8800) gets
the debug drawing called at all (flag 0x8000), after which H (object numbers, GameWorld flag 4; but
the editor takes H for the ground's heights while the level is held)
and G (the object map, 8) show as well; I is the flagman debug; P freezes the game entirely (no
drawing either) and ~ reloads the VIDs. R and E are the editor's keys while the level is held
(the rail debug view R works when it runs), and E (select units of any army) is player input,
which is off then. (Read from the code, not tried in editor mode.)
