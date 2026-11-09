# Editor mode: ground, relief, new map, palette, regions

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

**Ground, gravel bed, decals** (`editor_edit.cpp`, `ground.h`, `ground.cpp`). The strip's Ground height
tool raises, lowers, flattens or smooths the GRID cells (8 x 8 px) in a circle around the cursor while
the button is held (M the brush, the mouse wheel or + and - the radius - the wheel sizes the brush of
the Decals, Paint and Water tools too, where it turned nothing, and Shift+wheel stays the selected
object's height in the Select tool; checked: three notches up, one down -> radius 5, then 4; a stroke
ends - one step of the history - also when the window or the mouse's capture is lost or the tool
changes, and the brush's repeat timer stops when the button is found let go; the brush and the picture: "The relief and the
heights of objects" below), shows the brush's cells at their height coloured by it, leaves the track's
cells and their ring as they are (the row's TRACK: NOT COVERED, the default; COVERED, `g_track_covered`,
lets the brush change them too - the terrain may go over the rails; checked: a Raise stroke across a
raised line left its 3 rows of cells at 70 with NOT COVERED and raised them with COVERED), puts the objects back on the new ground and makes the stroke one
undo step (World_SetGroundZ). New track gets the gravel bed decal the missions use
(VID 612, 615 ice, 616 volcanic, 617 desert: the rail's place and direction, 18 below it; B cycles it,
the map's most common one by default), deleted and undone with its rail. In the editor a bed is an
object drawn 18 px under its rail: an embankment whose slope the ground's picture hides, as the
missions' grounds do, so it shows as the band of gravel under the rails wherever the rails show
(where the ground's picture stands higher than the track, a lawn of the template, both are hidden);
in play it is painted into the ground. The
Decals tool places the chosen decal - its name (the palette's) in the status line, its icon framed in
the tool's row, its sprite at the cursor in the chosen look (R / E turn it, as the Place tool's preview -
the same `g_place_direction`, so a decal picked in the row and one picked in the palette turn alike): a
click puts one at the cursor as shown, dragging scatters 1 + radius of them a step in the circle (+ and
-; radius 0 one a step along the drag, as shown), each turned at random (a decal's directions are its
variants), none nearer than the spacing (< and >, 4-64 px, 16 by default)
to a decal already there (beds aside), so they do not stack; Shift takes the decals in the circle
away. The four beds of the category (612, 615, 616, 617) are not dropped as loose decals (until
2026-11-06 a click dropped one at the cursor beside the rail's own bed, which stayed, and the loose
one could not even be picked - `pickable` leaves beds out): picked in the strip or the palette, a bed
is the swap of the track's bed (`bed_brush`). Picking it sets the kind (`set_bed_kind`: the same
`g_bed` / `g_bed_kind` as the Paint tool's KIND and the Track tool's B, so the three stay in step -
the strip's framed bed follows the other two as well); a click or a drag re-beds the plain and
destroyed rails whose line comes within the Decals brush's circle (8 px x the radius, at least 12; the
wheel or + and - size it) - the Paint tool's GRAVEL BED stroke with that circle (`start_bed_brush`,
below): on a campaign terrain `lay_bed` per rail (a decal of another kind replaced), on a picture
of the map's own the band painted and the decal beds taken away; swamp rails keep their embankment
(602); nothing is laid loose on the ground. Shift takes the rails' beds in the circle away (the
decals, and on a picture of the map's own the band too, the original put back) together with any
loose bed lying in it (`take_beds_near`), so the loose beds of a map saved with an older build go
the same way. The preview is the circle with the band's edges dotted along the rails it reaches and
the kind's colour beside it; the status line ("gravel bed: ICE - click / drag on the track ..."),
the row's tip above a bed's icon ("gravel bed: ICE - click / drag on the track") and the hint say
what the brush does. One stroke is one step of the history. Checked in the game on a copy of
mission 2 (decal beds, 612 meadow): ICE picked, a click on a rail of the left line made its bed 615
("1 rails, ... 1 decals laid, 1 taken away"), undone, a drag down the line re-bedded 5 rails, the map
saved - `map_tool.py unpack` showed 5 beds 615 and 356 612 under the 361 rails -, Shift-dragged
back to no bed (saved: none of the five), two loose beds put into it with `map_tool.py place` taken
by a Shift-click ("2 taken away", gone from the saved file); on a copy of POLIGON (its own picture)
the click painted the ice band under 2 rails ("painted into the picture"), saved with the map's own
`.vid` and shown again after loading. The decals are the palette's DECORATIONS category (`GroupDecals` of `game/editor/palette.tsv`:
every class-8 VID of `objects.res` once, in the file's order - `make_editor_palette` stops the build
when the group and the file disagree; `find_decals` reads the group through `panel_group_nvids`), so
the row above the bar and the palette show one list. The row is the strip of that category ("The rows
of the ground's tools" below): each decal its palette icon (VID 944), the chosen one framed, a click
picks it, the one under the mouse has its name right above its icon (the tips' font, `draw_row_tip`,
at once - the status line at the top is far from the row in a wide window) and in the status line; as
many as the view's width takes are a page, the others behind its < and > (the last page padded, so
the arrows stay put), and the page follows [ and ], which still pick the decal - from any other tool
they put the All objects entry on DECORATIONS with the next decal (`choose_decal`,
`panel_choose_group`); a decal picked in any category of the palette is the brush's the same way
("The object's panel ... the palette" above). A map's first decal is the ground cover it has most of (the beds,
wrecks, buildings and track pieces aside), else the round bush 936. A placed factory, repair point or fort gets its footprint flattened as map_tool.py place
does, and script function 81 `ReCalcGridZ(int unit)` - declared in export.lgc, missing in the exe -
now does the same (`ground.cpp`, through the hook of `restart.cpp`). Track is laid flat (the ramp
shapes of the rails, frame groups 1 and 2, are not used): the brushes leave its cells alone, so
trains never meet a slope. Checked in the game: raise and flatten strokes beside the main line of the
test map - saved, 126 cells changed and none of the track's cells or their ring (compared with
`map_tool.py unpack`), a factory's footprint then the same cell for cell as map_tool.py place makes
it on that grid, `check-track` clean, the map played with its trains running and no error; a rail
with its bed deleted and undone (rail points logged), the bed seen under new rails in the editor and
painted into the ground in play; 15 decals scattered, taken away with Shift and brought back by undo;
a test script calling `ReCalcGridZ` for every factory ("ReCalcGridZ: 192 cells ..." in
`LocoFix.log`, no "Unknown extern").

**The relief and the heights of objects** (`editor_ground.cpp` `ground_relief`, `editor_edit.cpp`
the sections "the ground" and "the height of an object"). The ground's picture is in screen
coordinates (a pixel is the screen point x, y - z; MAP_RULES.md 1) and its D16 height texture holds
1026 + 8 a GRID level, the "depth" every sprite is drawn against (a sprite pixel shows where its
height - 1024 + 8 z and its own depth - is not below the buffer's: `Vid_BlitSpanDepth`). So raising
the GRID alone made the objects stand dz px higher than the ground they were drawn on - they floated,
and the tester's "how do I change an object's height" had no answer either. Now:

- *The relief.* When the GRID changes, the picture is drawn again there: a column at a time, front to
  back (the ground's world y from the bottom of the screen up, by the stretches between the cells'
  middles, 8 px, the height linear along each), every pixel gets the first ground point that reaches it - raised ground shows higher and hides what is behind
  it, lowered ground may be hidden by the ground in front -, that point's texel from where the old
  heights showed it, lightened or darkened by the new slope against the old one (light from the upper
  left, half of it ambient: a steep face toward the viewer stays readable), and its height texel
  raised by 8 a level. The heights between the cells' middles are interpolated (the engine stands an
  object on its cell's value, the value at the middle). The walk counts the middles with an integer:
  it took each next middle as `8 * floor((y - 4.0001) / 8) + 4` in single precision, which from y
  2060 down the map rounds back to y itself - the walk stood still and the editor hung for good on the
  lower part of any map taller than 2048 px (issue #12; Ground height, the track's and a building's
  flattening there). Checked in the game: l08 (3072 x 2304), a raise at map y ~2150 - v0.2.9 stopped
  responding, now that stroke and three more in its lower part end at once; a new 4096 x 3072 meadow,
  all five modes and the right button's lower in its lowest rows and at its bottom-right corner, two
  rails laid there (26 cells flattened) and a factory placed on them; on the 2048 x 1536 template the
  picture after the same raise, ramp and smooth strokes is the same as v0.2.9's, pixel for pixel. Within
  a Ground height stroke every step is
  drawn from the stroke's start picture, so nothing is lost step by step; the stroke keeps the
  rectangle it changed (`PaintPatch`) and undo puts the picture back exactly. The track's and a
  placed building's flattening are drawn the same way, one step each. Always for the Ground height
  tool; for the track and buildings only when the picture is the map's own (an empty map, a meadow, a
  grown map, a painted one, or one the tool already redrew): a campaign terrain's `lNN.vid` is left
  as it came until the tool is used there, and then the map gets `<name>.vid` when saved, as a painted
  one does (`ground_save`: never over `lNN.vid`). The Water tool keeps its own way (it lowers the
  picture's heights where it paints, MAP_RULES.md 2). Chosen over an overlay alone because it shows
  in the game too: the hill is in the saved picture, the objects stand on what is seen, the depth hides
  what is behind it; the cost is a picture of the map's own (a few MB) once the ground is shaped.
- *Off the map.* The engine's `GetGroundZ` (0x420ec0; `World_GetGroundZForVid` calls it for every class
  but 7, which clamps its footprint itself) reads the GRID without bounds (`BUGS.md`, Quirks), and the
  editor asked it for points off the map - the cursor's point over the black area beyond the map's edge
  (`map_point`), the brush's circle and the grid's points there, an object dragged out. Every height the
  editor asks goes through `ground_z`, which brings the point onto the map first: off it, the height is
  the edge's (the first such point of a session is logged, "a ground height asked off the map at ...").
  Checked: every tool of the bar hovered over the black area beyond the four sides and at the four
  corners of the template's map and far past them, the brush tools and the Select and Delete tools
  dragged there too (3.5 minutes) - no exception in the logs; v0.2.9 did not fail either there (the
  reads land in the game's own memory), the point is read on the map now.
- *The brush.* Raise and lower add the strength (1-5, 2 at first) a step times the edge's weight -
  soft: a smoothstep of the distance (full in the middle, 0 at the edge), hard: full to the edge -,
  the parts of a level kept per cell for its next step, so hills come out round instead of as
  flat-topped discs; flatten and smooth pull toward their aim by the weight times 0.4 a strength
  (`blend`), rough adds a random -0.5 to +0.5 of the strength times the weight (`mix` of the stroke's
  seed, its step and the cell), ramp sets the cells within the brush of a dragged line to the level
  along it (`ramp_apply`, when the drag ends). The right button lowers in the Raise mode; Shift
  flattens and Ctrl smooths for a stroke (`Stroke.brush`); Alt+click takes the level under the
  cursor for Flatten (`g_flatten_level`; the Alt keys are kept, `WM_SYSKEYDOWN`/`UP` of VK_MENU are
  taken so Alt alone does not open the window's system menu). Held still the brush goes on every 150 ms (a window timer, `HEIGHT_TIMER`) - before,
  it worked only when the cursor entered a new cell. A stroke now steps every cell between two mouse moves (`height_step`): the moves
  come a frame or more apart, and a quick drag took the brush only to the cells they fell in - an 800
  px drag of a brush of radius 0 raised 11 cells 80 px apart, a ring drawn quickly did not close (the
  Paint and Water brushes already laid their dabs along the way). Over the bar, the tools' rows or an
  open panel nothing is raised or painted - the stroke goes on where the cursor comes out, not along
  the way under them -, and the button let go ends the stroke even when a panel takes the message: a
  panel opened during a stroke (F1) took the button-up, and a Paint stroke, which has no repeat timer
  to notice, went on painting under the panel and after it with no button held. Checked in the game
  (POLIGON, radius 0): the drag raised 101 cells in a band; a Paint stroke with Help opened and the
  button let go over it ended there ("231x81 px"), and one dragged down over the rows and back painted
  nothing under them (test steps `j` / `J`: the left button only pressed / let go). A Ramp stroke, laid
when it ends, ends at the last cell the cursor had on the map (`Stroke.ramp_to`): let go over the bar, the
rows or a panel - or ended by a panel, Alt+Tab, the capture lost - it ramped to the unseen ground under
the cursor (the one case of the strokes' review left, 2026-11-06); its preview over the bar shows that
end. Checked: a ramp dragged from 400,200 to 600,300 and down onto the bar, let go there - "ramp from cell
112,167 to cell 137,179" (before: the cell under the bar, 137,234). How a stroke
  ends (`end_strokes(why)`, each end logged "edit: stroke: <kind> ended - <why>", each begin "edit:
  stroke: <kind> begun at x,y"): its own button let go (anywhere - the mouse is captured), a tool
  changed (Esc, a number key, the bar), a panel opened (F1, the Help / Menu button: the Height timer
  had gone on raising the ground unseen under Help - 15 steps in 3 s), the window's menu or a dialog,
  the game left (Alt+Tab, `WM_ACTIVATE`) or the capture lost, a save, and Ctrl+Z / Ctrl+Y - the undo
  closes the stroke as its own step first (in its middle it had taken back the step before, and the
  stroke, which kept the cells as they were before the undo, brought it back on its own undo with
  nothing left to undo). The other button during a stroke is ignored (`g_held_button`, "goes on - the
  right button ignored"): a right click in a left Raise or Paint stroke began a new stroke over it,
  and the first one's ground stayed changed without a step of the history. Out of the window (the
  drag captured) nothing is raised or painted, as over the bar, and the Height timer waits there (a
  drag out to the corner had raised 655 cells beyond the view); the brush's outline goes when the
  cursor leaves the window (`WM_MOUSELEAVE`, the game left, a stroke let go outside). Alt, Ctrl and
  Shift are let go for the editor when the game is left or comes back: after Alt+Tab, Alt stayed down
  for it and the next click in Ground height or Paint was the pipette. The Water stroke keeps its
  kind (W during it is for the next one). Checked in the game: every case above in test mode (the
  log lines, one step each, undo to "nothing to undo"), and with the real mouse a stroke dragged out
  of the window and let go there, and the outline gone when the cursor leaves. Flatten's level is the cell where the stroke
  begins, now named in the status line ("flatten to level 92"): begun on the track it gives a plateau
  at the track's level - the way to put a line and its factory on a hill: lay the line up its ramps
  (Track tool PgUp, 11 levels each), raise the ground around the top, flatten it starting on the track.
- *Reading the heights.* Contour lines every 4 levels over the whole view (a dash where two
  neighbouring cells are on two sides of a level, at their height, coloured blue - green - red by it,
  every 20 levels brighter), always with the Ground height tool and with any tool after H.
- *The ground under the cursor* (`map_point`, used by the brush, placing, the Track tool's start and
  the decals) is the ground point nearest to the viewer whose y - z reaches the cursor, searched down
  the column as far as its highest cell allows; it was two guesses from the flat ground, which on a
  hill or a plateau took the wrong cell (a flatten begun on the track's top took the ground below).
- *An object's own height.* The object's height above the ground is its z less the ground under it
  (`World_GetGroundZForVid`); a drag and a Ground height stroke keep it. PgUp / PgDn (Shift: 16; Track
  and Push line tools aside, where PgUp / PgDn are the ramps), Shift+wheel and Home set it for the
  selection (a fort's or mining plant's guns go with it), -32 to 400, each change one step of the
  history; the status line shows it. Saved as the SPR z and kept when the map is loaded:
  `Object_Construct` takes the z it is given, except for the 17 VIDs with flag 0x200, which it puts
  on the ground (units 70 and 517-519, the civil robots 72 and 77, the objects 74, 79 and 414, the
  creature 421 and the generators and guns 401, 403, 404, 427, 556, 573, 706) - the editor refuses
  those with the reason, as it refuses train cars (they stand on their rails: the Track tool's ramps
  raise them) and regions (their z is their water's or fog's level). In play a unit or a creature
  that moves takes the ground's height again (`Object_Move`, `Cannon_Move`, `Creature_Move`,
  `CivRobot_Move` read it; read from the code), so a lifted unit stays lifted only while it stands; a
  lifted decal is painted into the ground picture where it is drawn.

Checked in the game (English and Polish): an empty 2048 x 1536 map; a line with two ramps up to level
92, a hill raised around its top end and flattened from the track (the log: "flatten to level 92"),
a factory at the line's dead end on the plateau, a fort beside it lifted by 12 ("12 above the ground";
after saving and loading the map: "selected VID 162 ... 12 above the ground"), the fort's panel with
WYSOKOSC: NIZEJ / WYZEJ / NA ZIEMI, a hill raised, undone (the picture flat again) and redone, the
contours with H in the Select tool; played: the hills and the plateau in the saved picture, the
objects standing on it, a locomotive placed in the editor patrolling up both ramps onto the plateau
and back, `error.log` without rail or logic errors; mission 1's terrain: a hill raised and the map
saved as `maps\camptest\camptest.map` with `camptest.vid` (`l01.vid` unchanged), loaded back with the
hill; `test_missions.ps1` all 20, `test_rail_rules.py` 3000 of 3000.

**A new map** (File > New..., `src/LocoFix/editor_newmap.cpp`). The dialog takes the map's folder in
`maps\` and its ground: an empty map - a plain ground of one of the painting's textures (grass,
sand, snow, rock ...; the 128 px sample laid mirrored, its brightness varied in soft patches so the
repeat does not show), flat at 70, nothing on it, ready to paint -, a meadow, or the terrain of one of
the 20 campaign maps (its size with it). An empty map or a meadow is any size from 512 to 4096 px in
steps of 256 (the ground's tile; the camera, the GRID and the ground's tiles handle that): - and +
beside the width and the height step by 256 within those limits, five buttons set the campaign's sizes
1024 x 768, 2048 x 1536, 3072 x 2304 and 1536 x 1024, 4096 x 3072, and the line beside them shows the
result in pixels and in tiles. File > Map size has the same row. The map is made the way `map_tool.py new` makes one: the chunks of the
campaign map with only the ground object (VID 1024 in OBJ, SPR and SPRD; no players, no groups), the
ground's image staying the game's own `maps\lNN\lNN.vid` (nothing is copied). A meadow is written as
`maps\<name>\<name>.vid` - value noise in two greens with drier patches, laid out as `map_tool.py
ground` lays a ground (256 x 256 R5G6B5 textures column by column, each with its height texture,
imageFlags 0x35) - on mission 1's map with the size set, the ground object in the middle (the picture
is drawn centred on it) and the GRID flat at 70. The picture's height is 1586 everywhere, what the
campaign's grounds hold where their GRID is 70 (they hold 1026 + 8 a level: l06 and l13 at 0 hold
1025-1027, l15 at 144 2181): the depth of the objects is measured against it, so the lower part of what stands below the
ground (a bed's slope) is hidden as on the campaign's maps. The template's script (with the
editor's blocks) and texts are copied under the new name, and the editor loads the map.

File > Map size... resizes the saved map: the right and the bottom edge move, from 512 to 4096 px in
steps of 256. The objects that would fall outside are deleted - rails, beds and cars up to 300 px
past the new edge stay, the main line goes on out of sight - after a question with their count;
then the map is saved, its file is resized - HEAD's size, the GRID (a new cell gets the height of
the old cell nearest to it) - and loaded again (not undone). Growing gives the map a ground image of
its own: the old picture with its heights (read from its texture tiles: the map's own or the
campaign's) and a meadow beyond it, the ground object moving to the middle. Cropping cuts the same
way: the map gets an image of its own of exactly the new size - the old picture with its heights
where it lay on the map (drawn centred on the ground object) up to the new edge - and the ground
object goes to the middle, so nothing of the old ground is drawn past the edge; before, the image was
kept and the old terrain still showed in the 300 px the camera goes past the map. The Water tool's
tiles outside go too, and the cars outside are deleted before the rails (a rail is not deleted while
a car stands on it). A campaign map or the template is not resized in place: it is saved under a
name of its own first, and the editor's new map gets its name first. Checked: a 1280 x 1024 meadow
made, a rail laid, saved, grown to 1792 x 1280 (the old picture kept, the rail where it was) and
cropped to 1024 x 768; a 2048 x 1536 meadow with a line of 8 rails from x 816 to 1420, a fort at
x 1400 and a hill, cropped to 1024 x 768: the ground ends at the new edge (the grid beyond it), the
line goes on to x 1384 out of sight (7 rails; the one past 300 px, the fort and its guns deleted),
the hill kept, `crop3.vid` of 1024 x 768, the ground object at 512, 384; `map_tool.py check`
round-trips every map and image of the game folder.

**The map's frame.** The camera of a held level goes 300 px past the map, where nothing is drawn;
the outside gets a dim 32 px grid and the map's edge a frame, so an empty map stands out - the track
out there stays visible between the lines.

**The armies' colours.** The colours of the armies are not in `objects.res`: the mission script sets
them when the level starts (`SetArmyGamma()` of `maps\script\setarmygamma.lgc`, called by
`InitDefault`: `SetVid(nvid, VID_GAMMA + army, ...)` for the units, the buildings' flags - VID 175 and
770-774 - and the state bar's icons). A held level runs no script, so every army looked the same;
the editor sets the same gammas when it holds a level (`SetVidData` 18-21: `Gamma_FromScriptInt`,
then the VID's `SetGamma`, vtable slot 7; the state bar's icons, VID 455, and the editor's own, 939, get
the script's `menu_gamma` - 0x1FF351 for the first, the one `Graphics_DrawVid`'s army-0 objects draw
with, lights them green as the game's state bar shows them; they were drawn untinted, white and grey). The
buildings' flags are their linked objects (`Object_CreateLinked` at construction: the fort and the
mine link VID 175, the factory and the repair point a light that links it), so they follow the army:
green for ours, red for the enemy, yellow for the neutrals, black for the pirates (checked with the
sandbox's factory, A three times). The status line names the army ("army 1 (enemy)").

**On the track** (`editor_edit.cpp`, the section of the same name). The authors put these on the
track (measured over the 20 missions): every factory (VID 102) at the dead end of a straight spur,
turned the way out along it (the nearest rail 1-38 px away), every repair point (104) on a straight
leg, turned along it like a gate, a post (152) 15-58 px beside the track (median 31), facing the
trains it is for, a money crate (169, army 3; `F169_8` gives the player 100 when a unit of
theirs takes it) on the track (1-46 px from a rail, median 17). Placing them snaps there - the
preview shows where -; farther than their reach, or with Shift, they stay where the cursor is (above:
"Placing over objects, the track as a help") - only a mine needs its rail point. (The missions'
blockers - the buffer stop, 150, "Blocker" of the palette's Railway group - stand at dead ends, all 99
within 16 px of a rail point with one link; the editor snapped one there too, but it could not be
turned and stood over the rail, so since v0.2.2 it is placed, dragged and turned freely, issue #3.)

| Object | Where it goes | With it |
|---|---|---|
| factory | the dead end (a rail point with one link) nearest to the cursor within 150 px, 22 px inside along the spur (the sandbox and l05), turned the way out (multiples of 32) | its exit push line as the missions have them: from the dead end along the spur, 140 px (they are 100-175), push_flag 1 - built cars appear in the factory and are pushed clear - with its post |
| repair point | the point of a straight piece (two links going opposite ways) nearest within 100 px, turned along it the way nearest to the direction chosen (at first the way of its link with the smaller direction: north on a vertical leg, as l05's); R / E turn it round | the push line through its leg, 120 px each side (they are 139-304), push_flag 1, one way in the direction it faces, with its posts |
| post | 24 px beside the track point nearest to the cursor, on the cursor's side of the track (R / E or the wheel put the preview on the other side), facing along the track the way that has that side on the right of its trains - its side follows from its way (184 of the missions' 188 posts stand so, 12-40 px from the track, median 23); the status line names its trains ("for the trains going left"); R / E, the wheel or the panel's FOR TRAINS turn a post beside a track round and put it across the track, as far from it as it stood (the track a post is beside is the one its trains pass on their right: the rail point nearest to 24 px on the post's other side - at a junction other track is as near), a post with no track within 70 px turns a step of its 8 directions | its own line at once, as the missions' posts have theirs (measured: each of the 188 posts against the nearest end of its script's lines): a line ending at the post's rail point - a red post's coming from behind it (107 of the 124 red posts; 85 with push_flag 1), so the trains it faces would enter it at its end against it and stop at the post; a green post's coming from the trains it faces (29 of the 61 green ones; 26 more at the begin of a line going on), so they run along it, pushed, and leave it at the post. The editor's are 70 px long with push_flag 1 (the red post's then pushes the trains of the other way toward it, as the missions' do). A pair at an entry - green facing the edge for the trains coming in, red facing in for those going out - owns one line from past the edge to the pair: the missions' entry (l02: `SetPushLine(-107,1203,54,1465,1)` with its pair at 54,1465). A line is a post's own by that shape alone, of any length (`owns`), so a loaded map's posts keep theirs, a pair shares one and the line of S or the Arrivals panel's entry belongs to its posts; never a factory's or repair point's line. None for a post that owns one already or stands within 40 px of an end of a line no post owns (a building's, the script's). Turning the post round, T or moving it makes its line again (a line another post owns too stays, the post gets one beside it); deleting the post deletes its line unless another post owns it (one step of the history each); L / Shift+L give a selected post its line green / red, the panel's LINE row says what the line does - STOPS THEM (red) or LETS THEM IN (green), the trains of its FOR TRAINS row - or NONE; the preview draws the line it will get. A post's line is drawn where it acts: a green post's line green (the way its trains are let in); a red post's not where the engine's line lies (behind it) but in front of it, where its trains come from and stop - red, from 35 px toward them to the post, the arrowhead and a red stop bar at the post; a pair's line green (the way in) with the red post's red mark in front of the pair (none goes back out); the status line says it in words ("the red semaphore's at 443,1399: the trains going right stop there (514,1299 -> 443,1299)"). A factory's or repair point's line keeps its flag's colour. Checked in play (`SANDBOX_DRIVE_TEST` on a copy of the sandbox): a red post on the main line for the trains going right, `SetPushLine(443,1299,301,1299,1)` - the reinforcement coming east onto that piece stopped at 387,1366 and stayed; the same post green, `SetPushLine(159,1299,301,1299,1)` - it went on east and reached its points; a pair at 194 (green south for the trains going right, red north for those going left) made one line, `SetPushLine(52,1299,194,1299,1)` - the reinforcement from past the west edge passed it east, and sent back west it stopped at 216,1375 and stayed |
| money crate | the rail point nearest within 80 px, 5 px above the point's height (`CRATE_ABOVE_POINT`): 23 of the missions' 42 crates on track stand 5 above their rail point (the others 4, 7 or 9; l05's at 80 over points at 76 and ground at 65-66; l04's four off the track 3-4 below it) - at the ground's height, as before, it sank into the rails, which stand 6-10 above the ground under them. A drag puts it onto the next point the same way; undo and redo keep its height; the status line's height above the ground is its z less the ground (11 on POLIGON's main line). Checked in play on a copy of POLIGON with `SANDBOX_DRIVE_TEST` and a crate placed at 123,1375,81 on the reinforcement's way: money 375 at 6 s against 275 without the crate - the train took it | - |
| fort | the line of the straight rail nearest within 60 px (`rail_line_spot`: its two ends' segment), where the cursor falls on it, turned along it the way nearest to the direction chosen - all 20 forts of the missions stand 0-6 px from such a line (R / E turn it round there) | its guns |
| mining plant (165) | as the fort: 29 of the missions' 30 stand 0-6 px from a straight rail's line, turned along it the same way (rails of shape 0: 0 / 128, 1: 64, 2: 160, 3: 96 / 224; l14's sixteen in rows on their tracks, l18's on the line through them); `F165_0` takes it as `F162_0` a fort - a train of another army within 75 px. Placed anywhere before | its gun |
| Charybdis | the line of the straight rail nearest within 100 px through its middle - l06's diagonal passes 1 px from it, so what it pulls in comes within the 40 px it eats in | its pieces |
| citadel | over the diagonal rail nearest within 80 px, the rail's line 8 px north of its middle - the three citadels of l04 and l18 stand over the crossing of two diagonal tracks, which cross there, a grate on each southern arm | its pieces and tower |
| crusher | across the straight rail nearest within 60 px, its middle on the rail's line, the drums on both sides - its form (the stands' direction) by the track's way, as `Pirate_Crasher` puts the ten of l04 and l18 | its second stand and drums |
| Scylla | on the diagonal rail nearest within 60 px, the stand on its line, the jaws across it - direction 0 on a track rising to the right, 128 on one falling, as l06's two | its jaws |

Dragging one of them puts it at the place on the track nearest to where it is dropped (with a new
line and posts for a factory or repair point; a fort, a citadel or Charybdis with all that goes with
it); dropped farther than its reach, or with Shift held, it stays where it is dropped (its old line
goes, it gets none) - before, it went back where it was; deleting a factory or a repair point takes its line and posts with it - each one step of the
history. A factory is not turned by hand (its spur turns it); a repair
point turns round on its leg - R / E, or TURN: ROUND in its panel, also while it is being placed -,
and its line is made anew the other way with its posts (a green one at the new way in, a red one at
the far end), one step of the history. Checked in the game: a mission made on the template with the main line, a spur
with a factory at its dead end (line and red post made), a repair point on the main line (line,
green and red posts), a crate on the line and the reinforcement point; played with a test script in
the game folder's copy: the reinforcement, damaged to 60 hp, was pushed through the repair point and
came out at 150, took the crate (money 1975 -> 2075), and the factory's train appeared inside it and
was pushed out to its line's end; `check-track` clean.

**The object's panel, the guns' sockets, the palette** (`editor_edit.cpp`, `editor_panel.cpp`). The
right button on an object in the Select tool opens its panel beside it - drawn in the game's style,
not a Windows menu: rows of the factory panel's strip (VID 497, 41 px), the label in the menu font
(VID 2) and the choices in the lit font (VID 7), the current one underlined; a click on a choice
applies it as one step of the history, the right button elsewhere closes it. Its rows: the army
(ours, enemy, neutral, pirates), turn left / right, for a semaphore the trains it is for (FOR
TRAINS: the way they go, the other way turns it round), its colour and what its own line does (STOPS THEM / LETS THEM IN, or NONE;
L / Shift+L), a row for each socket of a fort, a mining plant or a citadel
(empty, a tower with a cannon 110, a missile tower 115; the citadel's the pirates' tower 117), and
delete. A neutral object (army 2) and a pirate one (army 3) have a row IN A FIGHT, which says what
it does by the rules of the map (MAP_RULES.md 6c, `faction_rows`). On a map of the editor (its
settings have `FACTIONS 1`, LocoFix fix 33): a neutral one ONLY WHOEVER ATTACKS IT and, armed (a gun,
a fort's guns, a train with a turret), IT FIRES: WHEN ATTACKED or NEVER - its behave's bit 1,
`BEH_AGGRESSIVE`, set or cleared on its armed members, a step of the history (the editor's object
state has the behave); a pirate one AGAINST ALL SIDES (a click sets the bit on an armed one that
lacks it). A unit that becomes a neutral's or a pirate's - placed in that army or its army changed -
gets the bit (`faction_behave`). On a campaign map the engine's rules: a neutral one NO ONE FIRES AT
IT (`Object_FindTarget` 0x449ea0 takes a target of any other army but army 2, unless the shooter's
weapon has flag 0x80 or the enemy may hit neutral trains after `SetEnemyCanAttackNeutralTrains`)
and, armed, IT FIRES: AT ALL IN RANGE or NO - with the bit it fires at the player, the enemy and the
pirates alike; the missions' neutrals (civil trains and robots) have the bit clear. Checked in play
under the engine's rules: a neutral missile tower with the bit took a player's train from 320 to 15 hp in
60 s and was not fired at by it, one without the bit left a train beside it at 450. A, R, E, T, L and Del stay as the keys for them. A gun goes exactly into its socket (the
missions' offsets, MAP_RULES.md 6), with the host's army (on a citadel of army 0 the enemy's, as in
the missions) and direction, and follows it. A gun in a socket is part of its host: a click on it
selects the host (`pick`, `host_of`) and the right button opens the host's panel, where each socket
has one row - SOCKET 1, SOCKET 2, numbered on the wall while the panel is open, an empty one framed -
and a click on EMPTY, CANNON or MISSILES puts that gun in at once, underlined. The bar's entry 0, All objects, opens the palette: a
panel like Help with the groups down the left (meadow, forest, desert, ice, volcanic, swamp, park,
pirates, city, industry, railway, flags and crates, wrecks, decorations, units and creatures: the 244
kinds of object of the missions, trains and cars aside - DECORATIONS lists the 61 decals, the 53 the
terrain groups already had again (one icon each, `make_editor_palette`) and the eight they lacked (the
four gravel beds, Charybdis' roots 266, the desert paving 856, the silver grass 910, the fern 931); the chosen group's name is the panel's title, ALL
OBJECTS / VEHICLES in the strip above it - the title holds about 20 letters, and "WSZYSTKIE OBIEKTY:
JEDNOSTKI, ISTOTY" in one ran past the frame), 12 icons a page on the right (VID 944, made from the
objects' own sprites when the game is assembled), the arrows or the wheel for more and the name of the
one under the mouse (a unit's from the game's `[Units]`, the others' from `[Editor]`); a click picks
it and the map takes it in the army the missions give it, a unit with the behave and build list of
one of its kind on the map or, with none there, of the first on the missions' maps; a decal picked
anywhere in the palette makes the All objects entry the Decals tool with it (`tool()`: the scatter
brush, not a single placing - one way to place a decal wherever it was picked). The palette closed
without a pick (Esc, BACK TO THE MAP, the entry's tile again) leaves the entry with nothing picked
(`nothing_picked`: its object number 0 - and VID 0 is the engine's hourglass, the loading animation,
which the preview drew at the cursor with the Place hint until 2026-11-05): no preview, the status line
says "nothing chosen - a click opens the list", the hint "click: choose an object", R / E turn nothing,
and a click on the map opens the palette again (the entry stays chosen, so the strip's category is at
hand). Under the icons
an army row (ARMY: AS IN THE MISSIONS - All objects only, its first choice -, OURS, ENEMY, NEUTRAL,
PIRATES; the Vehicles palette starts on OURS) sets the army of what is placed next, the chosen one
underlined (`panel_palette_army`); A in the Place tool steps through the armies the same way, the
status line names the one placed ("..., army 1 (enemy)"), and a car coupled to a train still takes the
train's army. Checked in the game in Polish and English: a fort's panel with its army changed to ours and its sockets set to a
cannon and a missile tower (both turned with it), a mining plant with its cannon, a citadel with
its tower, a robot, a pirate wall and a bush picked from the palette and placed, decals clicked and
dragged (a band of bushes 16 px apart, none stacked), the map saved, loaded and saved again (the same
60 objects); played: the enemy's citadel, fort and mining plant fire on the player's factory and
train, the enemy's train shoots the guns of the player's fort and its missile tower fires at an
enemy robot.

**Wholes, fly nests and mines** (`editor_edit.cpp`, the sections "wholes", "the fly nest" and
"mines"; the runner's `PiecesTact`, `editor_mission.cpp`; MAP_RULES.md 6 and 6b). The citadel (149)
and Charybdis (254) are placed with the pieces the missions always give them (`pieces_of_kind`: the
citadel's two grates and four corkscrews, Charybdis's roots, teeth, eye, four mortars and two
cannons, found again by their offsets - `pieces_on`), as one object: preview, place, drag, height,
army and delete take them all, a click on a piece selects the whole, and its panel puts back missing
pieces. The faction is the army of what fights (the tower; the teeth, eye and guns; on the missions'
wholes, army 0 themselves, the enemy's - `faction_of`): a whole is placed in its faction's army (the
enemy's from the palette) and its panel's ARMY row shows and sets the faction. Charybdis snaps with the
line of the nearest straight track through its middle, as l06's stands, and eats the player's units. In play the
runner does what the missions' scripts do: `CitadelTact` (l04 / l18 `GratesTact`) for every citadel
whose tower stands, `CharybdisTact` (l06) for every Charybdis whose eye lives, `NestTact` (l06's two
nests) for every nest - the panel's FLIES: GREEN, IN WAVES / GREY, ON GUARD, kept as the nest's
direction (0 / 64), which a nest of one direction does not show.
The crusher and Scylla, which l04 / l18 and l06 make by script, and the decorations' sets the
missions always give their host (MAP_RULES.md 6b, 6c) are wholes the same way: a `Piece` may stand by
the host's frame (`pieces_of_kind(host, direction)`, the engine's `((direction + 128 / nDir) & 255) *
nDir >> 8`: a crusher's form, a chimney's height, a station's ridge) and a slot may hold one of
several VIDs or nothing (`choices`, `optional`: the panel's row - CORKSCREW: WHOLE / BROKEN, ON TOP:
CORKSCREW / STONE / NONE, FLAG:, SMOKE:, FOUNTAIN: -, `set_piece`, one step of the history); turning
or a snap onto a track of another way puts the pieces at their new places (`arrange_pieces`). The
crusher's host is its first stand (144): it snaps across the straight track nearest within 60 px,
its form by the track's way; Scylla's stand (252) onto a diagonal track; the track turns both. The
runner's `CrusherTact` bites as `Pirate_Crasher` (all the map's crushers together every 25 s, the
units of other armies within 20 px, paused while one of its own is within 60 px; dead drums once
its army's towers 117 are all gone - the map had some -, or never: the panel's STOPS WHEN: row,
`crusher_end`, kept as 8 * the choice past the stand's 64 * form, NEVER for one placed on a map
without a tower of its army, which the status line says), `ScyllaTact` as `scillas.lgc` (every 15 s,
every ground unit within 50 px; a Scylla given an army spares it), and `CitadelTact` breaks every
corkscrew of the map once no tower stands, as `ReplaceUnit` in l04 and l18. A mission's own records
are found by their offsets (10 px, the slot's height within 30), so editing l20 moves a flag with its
pole. The survey of all 20 missions behind these - which sets are real, which only look so - is in
MAP_RULES.md 6b / 6c. Checked in the game (a map of four straight tracks on the sandbox's ground):
the crusher took form 1 across a track falling to the right, Scylla forms 0 and 1 on the two
diagonals, every set came from the palette with its pieces, a border post's panel made its corkscrew
broken and back with Ctrl+Z, dragging and Del took the whole (`deleted VID 145 ... and 1 more`), a
station turned three steps kept its flag on its tower, records laid as the missions have them (a
flagpole and flag of l20, an exit and stone of l09, chimneys and smoke of l14, a station and flag of
l18) were each selected and deleted as one; played: the crusher and Scylla destroyed the locomotives
of ours in their reach and spared one 40 px away, an enemy Scylla spared the enemy's two beside it
(`ours 1 -> 0, enemy 2 -> 2` at its bite), and with the citadel's tower destroyed by a test line the
corkscrews broke and the crusher of its army stopped (`CRUSHER 1 stops: no tower of its army left`).
The forms the missions do not have, from the palette onto POLIGON: across the horizontal bottom line
form 0 and across a vertical track form 2 (`CRUSHER 1 at 820,1375 form 0`, `CRUSHER 2 at 958,1232
form 2`), both biting in the quick test (`CRUSHERS bite` at 25 s, the drums closed across each
track); the status line's warning of a map without a tower of its army was seen whole after the
placing in Polish and English. Factory chimneys (880) from the palette on POLIGON, played in the
quick test: the smoke (291) rose from them, a long plume drifting with the wind.
**Flags** (MAP_RULES.md 6c; `plan_place`, "where the object to place goes"). The palette's FLAGS,
CRATES group had the flagpole (769, with its flag 770) and the five flags as loose objects: a flag
picked went onto the ground at the direction last used - 0, its cloth seen edge on like a stick -,
the pole's flag was ours (green) and the loose ones the enemy's, the station's flag ours, while every
flag of the missions flies on a host facing 32 (the wind) and the pole flags are mostly the enemy's,
the station flags the neutrals'. Now a flag picked faces 32 (R / E still turn it) and flies on a
flagpole of its own where the cursor is (the pole and that flag in its slot, one whole), as the
missions' loose flags; 772, which has its own pole, stands on the ground as l09's four. With Shift it
goes onto the flagpole or station whose slot is within 24 px of the cursor (the slot's foot on the map
or the slot on the screen; it replaces the flag there, in the army chosen - `set_piece` with an army,
one step of the history); with Shift and no slot near it still flies on a pole of its own. (First it
was the other way round: the slot by default, Shift the flag alone - the user turned it; no mission has
a flag 770, 771, 773 or 774 without a host, so the bare flag went.) Both slots take all four flags (the
pole's FLAG: row 1 2 4 5). The preview shows the flag in the slot (the host circled) or on its new
pole, the status line "on a flagpole of its own (Shift: onto a flagpole or station near)" / "onto the
flagpole (Shift)" / "... - no flagpole or station near (Shift)". A flag in a slot turns too: R / E on a
flagpole turn its flag a step of the flag's directions (770-773 eight, 774 four - its four coloured
looks), Shift+R / E on a station its flag (R / E alone the station), and a turn or a drag of the host
keeps the flag's direction (`arrange_pieces` sets the other pieces' only). The five flags are five
pictures (770 a long flag, 771 a banner, 772 one with its pole, 773 a small square flag, 774 a pennant). A flag's colour is its army (`SetArmyGamma` gives 770-774 the
flag gammas; 774's four directions are its four coloured looks, which the gamma tints alike). Checked
in the game on POLIGON: a flagpole, each flag placed (770, 771 ours, 772, 773 neutral, 774 pirates - each
on its own pole but 772), 771 onto a new station (its ridge, red), 773 onto the first pole replacing its
flag; and again with Shift for the slot (below, "checked again").
The explosive mine (86) snaps to the
rail point nearest within 40 px - and only there: in play `F086_0` closes the rail point nearest to it
(`SetSemaphore` takes the nearest one however far), so a mine away from the track would close some
other track; Shift does not free it -, is placed **unarmed** (behave 0, as `F086_14` makes a mine born
in play; the map's mines are made so when the level is held, "mines unarmed until the level runs" in
`LocoFix.log`): a placed mine copied the behave of l12's (1, `BEH_AGGRESSIVE`, the palette's state
too), and armed from the start `F086_0` never closed its point - the test command `D` showed its rail
point with no semaphore in Run the level (no birth handler runs in editor mode), and its own army's
trains would drive onto it; now the point and its neighbour have `sem 4` + army a moment after the
level runs. It is placed in the army of the palette's row and is marked with its
army by the editor (`draw_mines`: a square in the army's colour and MINE: ENEMY; the engine hides a
mine of another army than the player's, also on the held level, so the editor draws its sprite, and
`pick` takes it although its VID has the screen flag 0x800); the authors' marker (89) is marked MINE
1 IN 3: ENEMY, and its panel makes it a mine of the enemy's. In play the engine shows a mine to its
own army only (VID flag 0x20000), as it always did.
Checked in the game (a map made in the editor: a line, four of our locomotives, a citadel, Charybdis
beside the track, an enemy mine, a nest of each kind; played): the gate hit the locomotive beside
the citadel (`CITADEL 1 gate hits vid 5 army 0` in `error.log`) while its tower fired, Charybdis
pulled in and ate the one beside it, the mine blew up the two over it, the waves nest sent its green
flies and the guard nest a grey one while a train of ours was within 400 px; the citadel's ARMY row
set to OURS turned its tower green and back with Ctrl+Z.

Checked again (a review of every whole, POLIGON): the fort, the mining plant, the citadel, Charybdis,
the crusher, Scylla, a fly nest, a semaphore, the flagpole, the station, the border post, the level
exit, the oil derrick, both chimneys and the pool placed away from the track (where the cursor is),
each turned with R and its army changed with A, saved, loaded and saved again - the two saves the
same object for object (254), every piece at its offset for the new direction (the fort's guns at
(-32, -24) / (33, 23) for 32, the plant's gun on its side, the station's flag on its ridge for 64, the
chimneys' smoke and glow at their frame's top) and the faction pieces in the new army; run: the
runner found each (`CITADEL`, `CHARYBDIS`, `NEST`, `CRUSHER`, `SCYLLA` lines), Scylla and the crusher
bit and the nest sent its first fly. What changed in this review: the mining plant now goes onto the
track as the missions' (it went anywhere), the flagpole's and station's slots take every flag and
their default armies are the missions' (the enemy's, the neutrals'), a mine is placed unarmed.

**The rows of the ground's tools** (`editor_edit.cpp`, the section of the same name). While an object
of the palette is to be placed - the All objects and Vehicles entries - the row above the bar is the
quick strip of the palette: for All objects its chosen category (`strip_group`, `panel_strip`; the
decals' is DECORATIONS), CATEGORY n / m: (the category's name alone while nothing of it is chosen);
for Vehicles every vehicle, the palette's groups one after another (23: the locomotives, combat cars,
supply, civil, obstacles - since 2026-11-05; before, only the group last chosen in the palette window,
so after a start the two locomotives and no wagon), labelled with the chosen car's group and its place
among all of them (WAGONY BOJOWE 5 / 23:, POJAZDY: - `VehiclesHeader` - while none is chosen; padded to
the widest label, so the icons do not move when the group changes), the palette window's group choice
leaving it whole; the
icons of a page, the chosen one framed, the one under the mouse named right above it and in the status
line (`StatusStripHover`); a click picks it as the palette would, [ and ] step through it (for the
vehicles across the groups), < and > turn the pages. While the
Track tool is chosen, its row holds RAIL: the four kinds (`RAIL_KINDS`, each drawn as its own straight
rail across, the chosen one framed; a click picks it as V does) and RAILS GO: UP / FLAT / DOWN (the
ramp of the wheel and PgUp / PgDn). While the Ground height, the Paint or the Water tool is chosen, a row of buttons stands above the bar's strip (Water: KIND - the five of `WATER_KINDS`, W cycles them - and SIZE, since 2026-11-06; before, the kind was W's alone): the depot
panel's strip (VID 497) laid as long as the row, centred on the view, the menu font (VID 2; VID 7 lit
for what is set and what is under the mouse, the set choice underlined). Ground height: the modes
RAISE/LOWER, FLATTEN, SMOOTH, RAMP, ROUGH, SIZE - n +, STRENGTH - n +, EDGE SOFT / HARD; Paint: a
MODE: TEXTURE / GRAVEL BED, then a swatch of each texture (its mean colour, the chosen one framed)
or KIND and WIDTH (GRAVEL BED, below; and UNDER THE WHOLE TRACK), SIZE, STRENGTH (a fifth to all of the texture a pass: `ground_paint`'s
strength), EDGE (`weight` full to the edge). A group that does not fit
beside the others goes into a row above (the window's or the screen's width). A click on a button
sets it; nothing under the rows is painted, raised or picked (`on_options`, as `on_bar`). The right
button stays with the Ground height, Paint and Water tools (Esc leaves them): Raise lowers with it,
Paint and Water put the original back with it. Paint's pipette, Alt+click, reads `ground_texture_at`:
the editor keeps the texture of each pixel it paints (a byte a pixel, `NO_TEXTURE` for the original;
in each stroke's `PaintPatch` for undo, carried by the relief); a pixel it did not paint in this
session is recognized by the picture itself - a texture laid at full strength is its sample at the
pixel's place (mirrored every 128 px), so the texture whose sample 13 or more of the 25 pixels around
match (within a step of each 565 channel) is the one there: a picture painted and saved before, or
painted by `map_tool.py paint`, names its textures too. Checked in the game (English and Polish): the
rows of both tools, HARD and STRENGTH set, the right button lowering, the wheel, a hill, a ramp from
level 70 up to it (106), rough ground, the level taken with Alt+click and Flatten (also with Shift)
going to it, undo; sand chosen in the row and painted, the right button putting the original back,
Alt+click on the sand ("texture: Sand (taken from the picture)") and on the original picture.

**The gravel bed in the Paint tool** (`editor_edit.cpp` "the gravel bed in the Paint tool",
`editor_ground.cpp` "the gravel bed's band"). How the authors bedded their track: l02/l09/l15, l11,
l12 and l18 put a decal under every rail and have nothing under the track in the picture; l10
(desert), l16 (ice) and l14 (volcanic) painted the band into the picture and have no decals (l10's
~28 px across, l16's ~20, centred on the rails seen); l13 has neither. In play l02's decal beds show
as a band ~28 px across, darker than the sprite (~110, 100, 60 where the sprite's top is ~140, 115,
60). No campaign ground has a patch of gravel to sample, and a patch mirrored out of the decal's
60 x 36 px top repeats like a kaleidoscope, so the band is laid along the rail instead: MODE: GRAVEL BED in
the Paint tool's row - the Paint tool has two modes, TEXTURE (painting anywhere: the swatches) and
GRAVEL BED, the first group of its row, the one on lit and underlined and only its own options
after it; the status line starts with it ("Paint - TEXTURE: ...", "Paint - GRAVEL BED (dotted: where
it goes): ..."; before, the swatches and GRAVEL BED stood side by side and the bed's status line was
an older key of the same name, `StatusBed` defined twice in every pack, so it showed "gravel bed (B):
meadow" only), and the brush's preview dots the band's two edges along each rail whose line it
reaches (`ground_bed_path`, the line `ground_bed` lays it along, `BED_RISE` up), red with Shift
(then KIND - meadow 612, ice 615, volcanic 616, desert 617, the same variable as
the Track tool's B -, WIDTH 8-64 px, 28 by default, SIZE, STRENGTH, EDGE and UNDER THE WHOLE TRACK)
beds the plain and destroyed rails whose line comes within the brush, each once a stroke. A rail's
line is the quadratic curve from its end 0 through its middle to its end 1 (ENDS of `rail_rules.h`,
so curves, switches and ramps - each end at its point's height - are followed), 16 straight pieces,
7 px up the screen: the rail's sprite shows its sleepers 7 px above its place (frame 1's middle row
68 of a 150 px cell), and the band centred on the place looked 7 px low beside them. A pixel within
half the width of a line takes the nearest line's colour: the row of the decal's top (frame 1 of
`vid\<nvid>.vid`, columns 45-104, rows 52-87, each row brought to the mean brightness and darkened
to 13/16) by its distance from the line - the same either side, so rails joined either way round
match - and the column by the place along the rail, laid mirrored a whole number of 60 px to the
rail, so every rail's ends are on the same column. The rails beside those being painted share the
pixels without being painted, so a rail painted in a later stroke meets its neighbour without a
seam. On a picture of the map's own (`ground_own`) the band is painted (a pixel's texture becomes
`BED_TEXTURE` + the kind) and the rails' decal beds are taken away - the band is their bed now, and a
decal over it would show the bed twice; on a campaign terrain whose `lNN.vid` is not yet the map's
own the decal of the kind is laid under each rail instead (one of another kind replaced) - until
2026-11-06, see below. The right
button puts the original picture back in the band, or takes the decals away; Alt+click takes the
kind painted there, else the kind of the nearest rail's decal, else the kind whose mean colour the
picture has near a rail (`ground_bed_at`). A stroke or the button is one step of the history (the
picture's rectangle and the decals laid and taken). New track still gets the decal of the kind (B),
the band's look and width in play. Since 2026-11-06 (the maintainer's rule) the two are separate:
the Paint tool's GRAVEL BED has LAYS: OBJECTS (the default, `g_paint_objects`) and LAYS: BAND.
OBJECTS swaps the game's bed objects under the rails the brush reaches on any map, the kinds shown
as their own pictures in the row (`start_ground_stroke` goes straight to `start_bed(erase, true)`
without `ground_paintable`, so the picture is neither read nor made the map's own; the right button
or Shift takes the beds and any loose bed away; UNDER THE WHOLE TRACK lays `lay_bed` under every
plain rail; STRENGTH and EDGE are not shown). BAND paints the band and only the band
(`start_bed(erase, false)`: a campaign terrain's picture becomes the map's own, as the Ground height
tool makes it) and leaves the rails' bed objects where they are. The four beds of the Decals strip
(DECORATIONS) do the OBJECTS swap with the Decals brush's circle (`start_bed_brush`,
`GroundStroke.reach`). The Track tool's row shows the four beds as pictures (GRAVEL BED: ..., NONE;
the same `g_bed` B cycles) so the bed of new track is chosen by sight. Before, the strip's beds
painted the band where the picture was the map's own and the band took the decals away, which looked
like a dummy bed to the maintainer, and the swap sat among the decorations instead of beside the
painting. Checked in the game (Polish and English): POLIGON's junctions
and curves bedded with a stroke (meadow), the kind ICE and WIDTH 40 over the main line, undone and
redone, desert UNDER THE WHOLE TRACK (166 rails, one step), the right button putting POLIGON's
original back over three rails, Alt+click naming meadow and ice; on mission 2 (a campaign terrain)
a stroke replaced 18 meadow decals with ice ones, undone and redone, Alt+click "taken from the
decal"; ICE chosen in the Paint tool and the Track tool's status line showing the bed 615, B
cycling it on; Alt+click on a band of a picture saved before (POLIGON's, painted by
`map_tool.py paint`) - also in the texture mode, near a rail - naming its kind.

**Wrecks on the track** (`editor_edit.cpp`, the section of the same name). The palette's Wrecks group
has five objects - the burnt carts and cars 13, 94, 57, 22 and 92 (class OBJECT, flags 0x4040,
moveMask 0x17, 5-7 hp) - and three decals (93, 96, 98: painted into the ground in play). The missions
stand 42 of the objects' 45 on the track: 0-40 px from a rail's middle, 6-8 above the rail (3 below
its points), turned along it (a rail of direction 6 has them at 64 or 192, 13 at 32 or 160, 20 at 96
or 224, 0 at 0 or 128); the decals they keep off it. So a wreck object placed or dragged within 40 px
of a rail link on the screen (`track_spot`, the cars' reach) goes onto the link nearest to the
cursor, 3 below its points, turned along it the way nearer to the direction chosen (R turns it
round there, also a selected wreck on the track), the preview and the status line showing it;
elsewhere it stays where it is put. The decals are not snapped. In play a wreck on the track does
not stop a train - the cars' moveMask (0x8) has no bit of its - but is run over: `Train_MoveCars`
gives 5 damage to what has VID flag 0x4000 ("run over") in the first car's box, so the first train
that passes crushes it (seen: a wreck put on the track of a patrolling locomotive was gone after its
first pass).

**Trains, the vehicles' palette, patrols** (`editor_edit.cpp`, the section "trains"). The strip's
Vehicles entry opens the palette with the 23 ENGINE VIDs of `objects.res` in their own groups -
locomotives (5, 10), combat cars (20, 25, 27, 30, 80, 62, 75, 90, 82, the zeppelin car 35, the
kamikaze 97, the bogie 15), supply (the ammo car 45, the repair car 85, the tankers 55 and 56), civil
(the tub 60, the passenger car 50, the goods car 95) and obstacles (the big stone 351, the invisible
railway killer 99) - named by the game's `[Units]` (27 and 99, without one, by `[Editor]`), their
icons the state bar's (`iconFrame` of the turret's weapon, else of the car's; the stone's frame 29
is a flat cell, so it gets its own sprite); All objects leaves these groups out. A car is placed
on the track: the rail link nearest to the cursor within 40 px (`g_railMap`, the screen position
y - z; where two tracks are within reach, the nearer one), facing along it - R turns the preview the
other way -, created there so that `Engine_Construct` (`Engine_PutOnRails`) puts its two ends on the
rails (`keep_on_spot`: PutOnRails takes the rail point nearest in 3D, which beside a crossing may be
the other track's - then the car is put again nearer the spot's point); next to a train's free end met
along the rails within 70 px it is coupled behind it (`Engine_AttachToTrain`) and takes its army, on
a car in the middle of a train on the same link it is refused. The selected car's train: U uncouples
the car from the car in front (the car and those behind it are a new train), Shift+U from the car
behind it (the car and those in front stay, the rest is the new train) - nothing is laid again: every
car keeps its place and ends; the panel's UNCOUPLE row shows IN FRONT OF IT (U) / BEHIND IT (SHIFT+U)
only where there is a coupling -, C couples an end car
to the free end of another train met along the rails (the other train is laid behind it car by car,
keeping each car's look, and takes its army), R turns the train round - `Train_Reverse` swaps its head
and every car's ends and toggles every car's "stands backwards" flag (engineFlags bit 0), which keeps
each car facing the way it was (nothing turned on the screen, a lone car neither); the editor toggles
the flags back, so every car faces the other way and the train's order is reversed, every car on its
rails -, E turns only the selected car round in its place of the train (its flag toggled,
`Engine_UpdatePosition` turns it; between the same neighbours, on the same rails; a lone car is its
train, E = R); a gun cart's turret (its linked object, `link` +0x40, made with the car and not kept in
the map; at rest it faces along its car) turns half a turn with its car, in the same step of the
history. The map keeps no flag: the load sets it again (Engine `ACT_RESTORE`) only for a car whose
saved direction is exactly opposite its front end's link (the smaller of the two differences over 127,
as `Engine_AttachToTrain` decides it), while `Engine_UpdatePosition` gives a car a direction a step or
two off its link's (193 on a link of 192) - so before every save (`edit_before_save`, from `save_map`)
a car standing backwards gets exactly that direction (a 256th of a turn: nothing to see), and a turned
car comes back from a save and a load turned (checked: a locomotive turned in the middle of a train and
a mortar at its end, saved, loaded, and in Run the level driven both ways by the enemy's AI, coupled,
each car's look kept through the AI's turns). A drag lays the whole train again with the dragged car at
the spot under the cursor and the others before and behind it in their order, each with its way in the
train, Del deletes the car and Shift+Del (the Delete tool with Shift) the train, A sets the army of the
train. Its panel (the right button) has the army, TRAIN: turn round / turn car (with a neighbour) /
couple, UNCOUPLE: in front of it / behind it, PATROL: none / to a point... (the next click on the track is the far end) and
DELETE: car / train. Every change keeps the couplings, both ends (`RailPos`), the turned-round flag
and the patrol of every car of the map before and after it, so undo puts every train back
(`ACT_UNDO_REMOVE`/`INSERT` of a car unlink and relink its neighbours; the couplings are set again
after them). A patrol is not a command kept in the map - `Engine_Action` has no case for
`ACT_PATROL` (36), so it would do nothing - but the script's `PatrolEngine` in main()'s editor
block: `PatrolEngine(FindNearestSprite(GETSPRITE_VID+<first car's VID>, x, y, 20), to_x, to_y);`
with the far end's screen position (`Engine_CommandToPoint`: COM_ENGINE_PATROL from where the train
stands to that point and back); a loaded map gives the block's patrols to the trains standing there.
A train needs a locomotive to drive (a tub alone stays: the missions' civil trains are a locomotive
with tubs). Saved, a car's SPRD is the missions' 40 bytes - both `RailPos` and the ids of the cars in
front and behind -, its couplings the same both ways as in all 20 missions (checked over the 197
cars).

**On the train's own track** (2026-11-05). What the editor does to a train never takes a car onto a
neighbouring track. R, E and U lay nothing again (the log says "rail points the same" / "ends kept").
C and a placed car find their partner by walking the rail links (`rail_walk`) from the car's free end -
the end farther from its neighbour, on from the farther of its end's two points - up to 70 px: a walk
goes on through a point only on a link that turns at most 0x30 from the way it came (a train turns
sharper only by reversing), so from a junction's stem it takes either branch but never one branch back
into the other, and it keeps to its line at a crossing; the first car met is the partner, and a train
on a parallel track or across, however near by the straight line, is not (the log names what the
straight line would have taken). The editor's track has no switch state: the engine's route search
chooses the branch when a train drives. A drag and a placed car go onto the link nearest the cursor
(`track_spot`, its distance and the next link apart in the log); the cars are laid from the spot along
the links by the engine's coupling, and the result is checked (`lay_problem`) before it is kept: every
car on the rails, none folded back on itself or over another car of its train - the engine turns the
rails back at a dead end (`RailDot_LinkTowards` at a point of one link), so a train too long for the
track from there folds -, none over a car of another train, none (for a drag) on a point the walk from
the spot does not reach; on a problem every car is put back where it was and the status line says why
("the train stays where it was: the track from there is too short for the train (a dead end)").
Engine_PutOnRails also couples to a car standing at the spot (`Engine_FindAdjacentCar`, by the points'
occupant): the cars leave their points first and `occupy_all` sets every car's points again after each
operation. The engine's chain has each car's front end toward the car in front (`Engine_AttachToTrain`,
`Train_MoveCars`), as the campaign's trains are laid (all but 4 of their couplings); POLIGON's two
trains had every car the other way round - in a run the engine logged `ENGINE 5: 0 rail 1`, `zmdots5`
and `zmdots6` (`Engine_UpdateRear`'s unusual cases) for them in `error.log` -, so R, E, C and a placed
car first give such a train's cars their ends swapped and their flag toggled (`normalize_chain`: the
same place and look). game/maps.new/sandbox now has them the campaign's way (2026-11-06): each car's
two rail positions swapped in its data (the chain's ids kept) and the east train's direction 128 ->
129, exactly opposite its new front link, so the load sets the "stands backwards" flag (128 is 127
off and would not) - the rows an E twice on a car of each train and a save give; loaded, both trains
list as before with `back 1` (`D0:0`), and a run logs none of those lines. No map shipped has such a
train any more (the 20 missions, the template), so there is no normalize at the load: a map made from
an older POLIGON keeps them until R, E or C touches the train, and only the log lines show it. The `edit:` lines of `LocoFix.log` give each decision
(`move train ... to the link ... laid / <why>`, `coupled ... px along the rails`, `couple ...: no free
end along the rails (by the straight line: ...)`, `turned car ...`, `turned round ... rail points the
same`, `uncoupled in front of / behind ... every car's place and ends kept`), and the test step `D<x>:<y>` lists the trains near the
point with each car's direction, flag and rail points (`D0:0` every train). Checked in the game on a
copy of POLIGON: R on the double track of the bottom line (two tracks 50 px apart) and beside its
junction; U; C on the same track (coupled, 2 px) and with the only other train on the parallel track
(none - 54 px by the straight line); a 3-car train dropped on the near track of the double track (the
cursor between the two), on the junction (laid through it into the loop), with its middle car at the
end of the factory spur (refused: a dead end) and in the spur's middle (laid); a car placed between the
two tracks, nearer the upper one (on it, alone) and nearer the lower one (on it, coupled 38 px along the
rails).

Checked in the game in Polish and English: a player's train of a locomotive and two gun cars
placed and coupled, uncoupled and coupled again, undone and redone; an enemy train (locomotive and
mortar, army set to the enemy), the big stone on the rails, a neutral locomotive with a tub on a
patrol; saved and loaded; played: the player's train driven with the mouse to the enemy, the enemy
train and fort fighting it, the neutral train going to and fro.

**Rail kinds and ramps** (the Track tool). V switches the kind of the next rails: plain (610),
swamp (600), destroyed (611) and destroyed swamp (601). A swamp rail is laid as the missions have it
(l06-l08): 28 px over the swamp's ground (the line's level is the ground + 29) on its embankment, VID
602 (TERRAIN, 37 frames like the rail, in the rail's place and direction, 28 below it), the ground
left as it is; its points stand 7 above it (the VID's altitude) where a plain rail's stand 10 - the
track rules take that into account. The destroyed rails are the death VIDs of the others:
`Rail_Action` marks their points `broken`, trains slow down there (`Options.brokenRailSpeed`) and the
repair car mends them as it passes (`Rail_RepairAt`); deleting one clears the mark of the points the
other rails keep. The ramp is a mode: UP (PgUp, the wheel up or the row's RAILS GO: UP) makes every rail
laid climb a step after the one before, DOWN (PgDn, the wheel down) every one descend, FLAT every one keep
the height reached, until it is changed (before, it was the next rail only and fell back to flat; a run
toward a far cursor was cut to one rail while a ramp was set) - a run of one click climbs rail by rail,
the preview draws each at its own height, the ground under each slopes with it, and a section started
at a ramp's raised end is found where that end is drawn (`line_start`: the end's point height, not the
rail's) and goes on at its height (the status line shows the level and the mode): each rail the same shape in a ramp frame -
frames 12-23 raise the point of end 0, 24-35 that of end 1, by sizeZ 11 (direction (shape + 12 or
24) * 256 / 37: 83, 89 ... 159 and 166 ... 242, the directions of the missions' 98 ramps of l11, l12
and l15) - with its far end raised (climbing) or its near end (descending, the rail then 11 lower);
the line goes on 11 higher or lower, the ground under the ramp stays as it was where the ramp is above it (it slopes
with the ramp only at the foot that lies on the ground, "Laying track" above)
and its bed is the bed VID in the rail's direction (the missions' ramp beds are, all but 7 of 98).
Checked: a new map on mission 6's swamp terrain with a swamp line on its embankment and a plain line
climbing a ramp onto a plateau raised with the Ground height tool, a train on each with a patrol,
`check-track` clean; played: both trains drove to and fro, over the swamp line and up and down the
ramp. The mode, checked in Polish on POLIGON: UP, one click laying 5 rails (z 66 ... 110, their last point
131 = 76 + 55), FLAT from its raised end 2 rails at 131 linked to it, DOWN 5 rails back to 76; Ctrl+Z took
the last run out whole (10 rail points), Ctrl+Y put it back, `check-track` of the saved map clean; on a
copy of POLIGON with `SANDBOX_DRIVE_TEST`, two rails of the reinforcement's way laid again UP and DOWN
(points at 87 between 76s): the train drove over the hill to its first point.

**The ground's picture, water, regions** (`src/LocoFix/editor_ground.cpp`, `editor_edit.cpp`;
MAP_RULES.md 2). The editor reads the held level's ground image into memory (its colours and its
heights, `editor_newmap.cpp`'s reader) when a Paint or Water stroke begins, and shows every change at
once by writing the changed texels into the image's own Direct3D surfaces: `GroundVid.tiles` and
`textures` - a colour texture (565, 555, 1555 or 4444, whatever `Texture_Construct` made) and its D16
height texture per 256 x 256 tile, locked and written in place. A stroke is one step of the history
(the rectangle it changed before and after, the GRID cells, the objects made or taken away). When the
map is saved, a painted picture is written as `maps\<name>\<name>.vid` (the layout of File > New's
grounds, imageFlags 0x35) and the map's ground record points at it - never over a campaign
`lNN.vid` (a campaign map saved under its own name gets `<name>_painted.vid` if the name would clash).
**Paint the ground**: the textures are 128 x 128 patches of the player's campaign grounds named by
`editor\textures.tsv` (game/editor/: the map and the place, nothing of the pictures in the repository)
- grass (l02), light grass (l15), forest floor (l07), sand (l11), desert (l10), snow (l18), ice (l16),
rock and dark rock (l20), lava (l13), swamp (l06), lake bed (l15) -, laid mirrored, the brush's edge
soft and grainy; [ and ] pick one, + and - the brush (8-200 px), Shift puts the original picture
back. **Water and lava** (W picks the kind) does what the authors did (MAP_RULES.md 2): it lowers the
picture and the GRID under the brush to the bed (the level - 25, the shore sloping), paints the bed,
adds the kind's tiles where missing on the 128 grid at the map's level of that kind, and when the
stroke ends scatters its generators (water circles 504 and noise 510, swamp bubbles 206 and circles
337, lava eruptions 441) as far apart as the missions have them and grows one region turning
explosions there into splashes on water or lava. Dragging one kind over another replaces it in the
squares the brush reaches (that square's tiles and generators of the other kind go, a kind left without
tiles loses its splash region). A kind new on the map begun in another kind's water takes that water's
level, so its tiles cover all of the other's water in the squares it takes over - measured from the
ground there, the other's bed, it came out below that bed, and around the brush the replaced squares
showed their bed bare; a kind already on the map keeps its level (all its tiles stand at one). The right button or Shift takes the water of every kind back: the
picture and the GRID inside the brush's circle go back to the original, and when the stroke ends a
square it went over loses a kind's tiles - and that kind's generators there - only when no water of
the kind is left in it (`square_wet`: none of its points below the level); the generators no longer
over their water go too. v0.2.9 took the tiles of every square the brush's box touched while the
ground came back only inside the circle, so the lake bed around the circle showed bare in 128 px
squares (issue #9). Checked for the five kinds: a lake of three strokes, the right button in its middle
and at its rim - only the brush's circle dry, no square; the next kind painted across it - the squares it
took over all of the new kind; Shift over all of it - nothing left; every step undone in turn. **Region**: drag its rectangle; it is picked at its dashed frame (inside it the
objects under it are), its panel sets the size, the explosions (as they are, on water, on lava), the
fog (none or the missions' mist, dust, haze, glow, set around its level), over the region or the
whole view, pulsing - all of it kept in the history (`Region_InitFog` for the fog, the VID pairs set
as `Region_Action` loads them). The palette's Effects group has the generators (water circles and
noise, bubbles, ripples, polar bears, lava eruptions, chimney smoke, sparks, fire, glows) and the
three regions. Checked in the game in Polish and English: an empty 768 x 512 map of grass in its
frame, File > New's and File > Map size's dialogs (the - and + buttons and a preset), a 1536 x 1024
empty map with a lake of light water, a lava field, a sand path, a snow patch, a region with mist and
splashes, a fire from the palette - saved, loaded (the painted picture came back from its `.vid`),
`map_tool.py check` round-tripping the map and its ground image, played: the water rippling, the lava
erupting, the fog over the region, the fire burning.

**Regions, reviewed** (2026-11-04 and 11-05, the Region tool against the engine). A region (`B_REGION`, `struct
Region` 0xd0 bytes; `Region_Action` saves 80 bytes of it: flags, the fog's high and low levels, its
colour, width, height, an unused word, its image VID, six VID pairs) is a rectangle on layer 7 around
its place, at its z. No script refers to one (`GETSPRITE_REGION` of `export.lgc` is declared and never
used; `daynight.lgc` only gives VIDs 750 and 752 rain circles as their child while it rains). The engine
does four things with it: `Region_Draw` tiles its own VID's picture over it (750: the light water's
tiles - a lake's look without the Water tool's bed) and draws the fog between its low and high levels
in its colour (`Graphics_DrawFog`; flag 1 pulses it, flag 8 lays it over the whole view): per pixel
of a quarter-resolution mask the depth buffer less 0x400 - the ground's picture is 1026 + 8 a GRID level,
so 2 + 8 a level, an object's pixels higher - against the levels times 8 (`Region_Draw` passes them,
`Graphics_DrawFog` multiplies: its pulse + (low - high) * 8): full fog below low, fading out up to high,
none above, clipped to the rectangle unless flag 8 (l05's mist 5-35 lies on its river bed, picture
depth 58-182 under the region; l18's 70-120 on ground at 563-579; l13's 7-20 on the lava at 1; l07's
30-45, flags 10, over the whole view);
`Vid_GetTerrainVariant` (from `World_CreateObject`) makes an object of a pair's first VID created inside
it, less than 25 above it, of the second - the missions' explosions 308 / 309 become the splashes 338
(water) or 528 (lava) -, and with flag 8 anywhere on the map; a creature (`Creature_Construct`,
`Creature_Move`: the underwater dragon 250, the fish 418, the golem 409) remembers the region it starts
in (`World_FindRegionAt`) and keeps to it - l11's and l17's dragons stand in theirs. The missions' 118
regions: 751 with nothing set (creature areas and markers: l04 has 29, l17 17), splash regions over
their lakes and lava, five fogs (l01 on a 750: dust 20-60, flags 2; l05 mist; l07; l13; l18 - the
values read back from the loaded level: the file holds them in `Region_Action`'s order, which
`Region_InitFog` turns into high and low), 752 in l06, l09 and l15 (l15's flood raises its 752 with
`ChangeZUnit`). The tool has every kind: the rectangle (16 px at least) at the ground's level, its
panel's size, EXPLOSIONS (as they are / on water / on lava: the pairs), FOG (none, or the missions'
mist, dust, haze, glow around its level, in GRID levels: MIST 50-80 at level 70), OVER: THE REGION /
THE WHOLE MAP (flag 8 - the fog and the pairs both; the row said FOG OVER: THE VIEW, which hid half of
it) and PULSING (flag 1); the palette's Effects group places 750, 751 and 752 as rectangles sized in the
panel; the image VID is the region's own and not set anywhere. FOG and OVER are rows of their own: a
fog chosen stays over the region until OVER: THE WHOLE MAP is set (which says so when the region has
pairs - they go over the whole map with it). A fog over the region alone on flat ground is a rectangle
of one tint with hard edges, while the missions' fogs lie over the whole map (l07, l13, l18) or in a
hollow, a lake or river bed under a big region (l01, l05), where the fog takes the terrain's shape
(`build\shots\_o_l*.png`: each mission held and run). The maintainer saw no fog after Run the level on
d43e850; in this build it shows in the run - the rectangle at 4 s and on through 40 s (`i2_fog`,
`i2_long`), and with THE WHOLE MAP the tint over the whole view (`i2_w`: the ground's colour outside
the rectangle 112,149,61 against 112,134,27 without the fog). A region of light water (750)
shows only its frame where the ground's picture is not lower than its z: its water tiles are drawn with
the depth test, so a 750 at the ground's own level (as the palette placed it) was hidden under the
ground, held or running, and the missions' lie over a lowered bed - l05's river is its 750 at level 40
over a bed at 0-27 (the map has no water tiles), l01's dust region a 750 at 60 over a hollow down to
10; lowered with the Ground height tool under it, its water shows at once in the held editor
(`i2_lower`). The palette now places it above the ground (`lift_water_region`): over a light-water
lake of the Water tool (a tile of VID 204 in its rectangle) at that water's level, one surface with it;
elsewhere `WATER_SHEET` (2) levels above the highest ground under its rectangle, a flat sheet of water
over the ground; the rectangle stays where it was put and the status line names the level and why
("light water region at level 72, 2 above the highest ground under it (70)"). Its picture is tiled in
whole 128 px tiles from the rectangle's top-left corner, so its default 192 px width drew 256 px of
water, 64 past its frame: a 750 starts 256 x 128 and its panel's size goes by 128 (other regions by
32). The Water and lava tool is the other way - a basin with a shore, the tiles, the effects and a
splash region; the guides (3. Water, swamp, lava) and help page 11 say which is for what. Checked in
the game (POLIGON): the sheet at level 72 and a 750 over a Water tool lake at its level 50 both shown
in the editor and in Run the level, the frames only in the editor (`water750b_10`, `_14`). The regions' frames (`draw_regions` of the overlay, and the engine's
`Region_DrawDebug`, vtable slot 6, only with World flag 0x8000 - the debug key O) were not seen in a
run or in Play the map (`i2_w_12`, `_18`, `_19`). Checked in the game besides: l05's and l01's regions
read in the editor (the log line of a selected region: size, flags, fog, image, pairs); a golem inside a marker region and one outside stood where they were placed through 20 s
of a run (golems do not walk by themselves - l13's stand at its script's places), so the creature area
rests on the engine's code and the missions' dragons, not on a run; the pairs on the code path only
(an explosion inside a splash region was not provoked).

**Sheets of water and lava** (the Region tool's MAKES; `editor_edit.cpp` "sheets of water and lava";
issue #5, paulevsGitch: regions of water and lava like the vanilla maps', which can be raised and
lowered). The vanilla water and lava are not regions - `objects.res` has three region VIDs, 750 light
water, 751 empty and 752 empty with rain circles, and none of lava -: they are sheets of the kind's
128 px tiles (199 dark water, 204 light, 205 and 229 swamp, 293 lava; SPRITE objects on the 128 grid at
64 + 128 i, 64 + 128 j, all at one level), drawn with the depth test, so the water shows wherever the
ground's picture is lower than the level: l13's 432 lava tiles at 5 and l15's 430 light-water tiles at 21
cover the whole map, l06 has 208 swamp tiles at 17, l12 44 lava tiles round its pits. With each sheet the
missions have one small region (29-35 px) flagged THE WHOLE MAP 1-7 levels above it with the kind's
pairs (308 / 309 -> 338 over water, 528 over lava): a 752 over light water and swamp (l06, l09, l15),
a 751 over lava and dark water (l11, l12, l13, l17); l13 has a glow fog besides (7-20 over its lava at
5, over the whole view). l15's flood raises its sheet over 80 s with `ChangeZUnit(204, z)` and
`ChangeZUnit(752, z + 1)` - extern 156, which is `Object_MoveTo` for every object of the VID. The Region
tool's row MAKES: REGION (as before) / LIGHT WATER / DARK WATER / SWAMP / SWAMP 2 / LAVA lays such
sheets: a drag lays the kind's tiles over the squares of its rectangle (not where another kind's tile is:
a tile shows wherever the picture is below its level, so two kinds in one square would show each
other) at the map's level of the kind - one level a kind, as the engine moves them together -, a new
kind `WATER_SHEET` (2) above the highest ground in the rectangle (a flat sheet over the ground, as the
palette's light-water region), and the kind's splash region where the map has none (32 x 32, flag 8,
the pairs, 1 above the sheet); Shift+drag takes the kind's tiles in the rectangle away (the region
with the last of them); THE WHOLE MAP lays the sheet over every square - a new kind 5 below the map's
lowest ground, so it shows where the ground is lowered under it -; LEVEL - / + put the whole sheet a
level lower or higher with its region (and a fog of it), one step of the history each. A lake is then
the sheet over ground lowered with the Ground height tool, or the sheet raised over the ground - no
ground is lowered and no bed painted, unlike the Water and lava tool, which stays as it was. l13's glow
fog is not laid: it tints all ground below its low level, and l13's ground stands above it; over a
sheet laid on flat ground it turned the whole view orange (the region's panel has it, FOG: GLOW).
Two things found on the way: a sheet a level or two under the ground showed now and then in the held
editor for a second or two (the tiles' own depth against the ground's: at 2 under the ground 2 of 8
screenshots a second apart showed it, at 5 under none; in Run the level the same sheet at 2 under
stayed hidden in all 8), so LEVEL lays the tiles again at the new level instead of moving them - a
moved tile was suspected first and was not the cause, the depth is -, and keep a sheet 5 or more under
the ground where it must not show; and a Ground height stroke took regions along with the ground under
them (`follows_ground`: a sheet's region went 30 levels down into a pit dug under it) - a region's z is
its water's or fog's level, so regions stay now. What a map's script can do with a sheet today: move it
the way l15 floods, `ChangeZUnit(<the kind's VID>, z)` and `ChangeZUnit(<its region's VID>, z + 1)` in
its loop - code outside the editor's blocks stays in the script when the map is saved ("Mission
settings and push lines", `editor-mission.md`); the editor adds no script for it. Checked in the game on
a copy of POLIGON (flat, ground 70): LAVA dragged over 300 x 200 px - 6 tiles at 72 and a 751 at 73
(flag 8, 308 / 309 -> 528), shown over the ground; LEVEL - three times to 69 - hidden; a pit dug with
the Ground height tool's right button - lava in it; Shift+drag took 3 tiles, undone; saved: the 6 tiles
293 at 69 on the 128 grid and the 751 at 70; loaded again - only the pits show lava; THE WHOLE MAP on a
new sheet: 191 tiles under the ground.
