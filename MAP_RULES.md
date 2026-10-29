# Map rules

The rules a map of Locoland has to follow so that the game builds its track, runs its trains
and plays its mission the way the campaign does - the specification for a map editor. Each
rule says where it comes from: the code of the engine (`re/symbols.csv`, `re/c`), the 20
missions of the game (measured on their `.map` and `.lgc` files) or a test in the game.
`game/maps.new/sandbox` follows all of them and is the reference map; its script has
`SANDBOX_DRIVE_TEST`, `SANDBOX_WIN_TEST` and `SANDBOX_LOSE_TEST` to check them (see 10).

`map_tool.py` (DEVELOPERS.md "Editing maps and images") already enforces the rules marked **[tool]**;
the editor of the game (DEVELOPERS.md "Editor mode", `editor_edit.cpp`) applies those marked **[editor]**.

## 1. Coordinates

| | |
|---|---|
| World | x, y on the map plane, z the height. Objects (`objects.tsv`) are in world coordinates. |
| Screen | x, y - z: where a point is drawn (with the camera subtracted). The ground image is in screen coordinates: an object at world (x, y) on ground of height 70 is drawn at (x, y - 70). |
| Rail points | z = the rail's z + the rail VID's altitude (10): 76 for rails at 66. |
| Script, screen | `SetPushLine`, patrol points (`ATA_PATROL*`), `start_trains_x/y` (the tactics add the ground height), `GetSpriteScr` - the rail point nearest to (x, y) with y compared against y - z (`RailMap_FindNearestDot`). |
| Script, world | `Action(train, ACT_MOVE, x, y)` (`Engine_MoveToPoint` with screen 0, a 3D search); `GetX/GetY` return world. A mouse order is screen. |
| Camera | HEAD `cameraX/Y` is the top-left of the first view; `SetPosition(x, y)` takes the centre of the view, clamped to the map. Level_Load calls `SetPosition(cameraX + w/2, cameraY + h/2)`. |

## 2. Map, grid and ground

- Sizes of the game: 1024x768, 2048x1536, 3072x2304. Grid: one height per 8x8 px cell,
  (width / 8) x (height / 8).
- The ground object (VID 1024) stands in the middle of the map (512, 384 / 1024, 768) - its
  image is drawn centred on it; elsewhere part of the map has no ground. **[tool]** `new --size`
- The ground image is cut into 256x256 textures listed column by column; each tile shows the
  texture its field names (colour textures are the even ones). **[tool]** `ground`, `pack`
- The ground picture is screen space (1): paint the track bed at y - 70 for flat ground at 70.
- Rails sit 4 px below the ground (rail z = ground - 4). The track flattens the cells under it.
  **[tool]** `track`
- **[editor]** The Ground height tool changes the cells (raise, lower, flatten, smooth) but never
  those of the track and the ring around them; objects stay on the ground (each keeps its own height
  above it, which PgUp / PgDn / Home set - its z in SPR). The picture is drawn again for the new
  heights (the relief: raised ground shows higher and hides what is behind it, its height texture
  8 a level higher), so the picture becomes the map's own `<name>.vid`; the track's and buildings'
  flattening is drawn the same way once the picture is the map's own. A building placed in the
  editor gets its footprint at the height under its middle and the ring half way, as `place` does
  (script `ReCalcGridZ(unit)`, function 81, does the same in the game).
- The gravel bed: the missions that have one put a decal under every rail - VID 612 (l02, l09,
  l15), 615 ice (l18), 616 volcanic (l12), 617 desert (l11) - at the rail's x, y and direction,
  18 below it. **[editor]** new track gets one (B picks which), deleted with its rail. Other maps
  have the band painted into their picture instead, without decals: l10 (desert, ~28 px across),
  l16 (ice, ~20 px), l14 (volcanic); l13 has none. In play a decal bed shows as a band ~28 px
  across. **[editor]** The Paint tool's GRAVEL BED mode paints that band (28 px by default, the
  colours of the decal's top) into a picture of the map's own under the rails the brush goes over,
  centred on the rail seen (the sprite's sleepers are 7 px above its place) and taking their decal
  beds away, or lays the decals on a campaign terrain; UNDER THE WHOLE TRACK does every rail.
  **[tool]** `paint` repaints a map's picture with the editor's textures and this band.
- **[editor]** A new map (File > New) is an empty map - a plain ground of one texture of the
  painting's, flat, nothing on it -, a generated meadow, or a campaign map's terrain with only the
  ground object; an empty map or a meadow is any size from 512 to 4096 in steps of 256 (the ground's
  tile; - and + or a preset: 1024 x 768, 2048 x 1536, 3072 x 2304, 1536 x 1024, 4096 x 3072) with the
  GRID flat at 70 and the picture's height 1586. File > Map size grows (a meadow beyond the old
  picture) or crops a map at its right and bottom edge, with the same size row; either way the map
  gets a ground image of its own of the new size (cropped, the picture ends at the new edge).
- **The ground's picture holds heights** (a D16 texture after every colour texture, blitted into the
  depth buffer): the campaign grounds hold **1026 + 8 a GRID level** (l06 and l13 at 0: 1025-1027,
  at 70: 1586, l15 at 144: 2181). Objects, water and fog are drawn against these depths, not the GRID.
- **Ground painting [editor]:** the Paint tool paints the picture with textures sampled from the
  player's campaign grounds (`editor\textures.tsv` names the 128 x 128 patch of each); a painted
  map gets its own `<name>.vid` when saved (a campaign terrain keeps pointing at the game's `lNN.vid`
  until it is painted). The heights are kept as they are.
- **Water and lava** (l01, l05, l06-l09, l11-l13, l15-l17): animated tiles of 128 x 128 - light
  water 204, dark water 199, swamp water 205 and 229, lava 293 (GroundVid with flag 0x10000: layer 6,
  drawn after the ground with the depth test) - at (64 + 128 i, 64 + 128 j), all at one height per
  map: the water's level (l09: 40, l15: 21, l17: 63, l13 lava: 5). Most maps lay them over the whole
  map; the water shows only where the ground's picture is lower than its level - the authors painted
  the lakes into the picture with their beds below the level and their shores sloping up. The GRID
  under the water is the bed (l09: 1 under water at 40). Over it the missions put generators at the
  water's level + 1: water circles 504 (about 40-60 px apart), water noise 510 (one or two), swamp
  bubbles 206 (200-400 apart), swamp circles 337, lava eruptions 441 (about 300 apart), polar bears 505
  (l16). **[editor]** The Water tool does this with a brush: the picture lowered to the level - 25
  with a sloping shore and painted with the bed's texture, the GRID lowered the same way (the track's
  cells left alone), the tiles of the kind where missing (one kind per tile: a tile shows wherever the
  picture is lower than its level), the generators at the missions' spacing and a splash region
  over it; a new water's level is the ground - 20 (swamp - 16, lava - 12).
- **Regions** (B_REGION, VID 751 plain, 752 with rain circles, 750 with the light water's image; 41
  kinds on 17 maps): a rectangle (width, height around its place, drawn at y - z). Its 80 bytes of
  SPRD: flags (8: the fog over the whole view, 1: pulsing), the fog's high and low level and colour
  (ARGB), width, height, an int, its image VID and six VID pairs. **Fog** (`Graphics_DrawFog`): the
  ground whose depth - 1024 (8 a level) is below `low` is fully fogged, fading out up to `high` - the
  missions' fogs: l05 blue mist 5-35, l01 dust 20-60, l07 red haze 30-45 (flag 8), l13 lava glow 7-20,
  l18 (8, 120-70). **Pairs**: an object of the first VID created inside the region, less than 25 above
  it, is made of the second (`Vid_GetTerrainVariant`): the missions turn the big and the small
  explosion (308, 309) into the explosion on water (338) or on lava (528) over their lakes. 750 and 752
  grow rain circles (VID 339) while it rains (`daynight.lgc`). Most of the 114 regions are markers
  with nothing set. **[editor]** The Region tool drags one; its panel sets the size, the explosions
  (as they are, on water, on lava), a fog (none, mist, dust, haze, glow - the missions' colours, around
  the region's level), over the region or the view, pulsing.
- The colours of the armies come from the mission script (`SetArmyGamma`); **[editor]** the editor
  sets the same on a held level, so the buildings' flags (VID 175, linked) show the army.

## 3. Track (VID 610)

**[editor]** The Track tool of the editor applies every rule of this section that is marked **[tool]**
(the same code in `rail_rules.h`, compared with map_tool.py by `tools/test_rail_rules.py`), and
`map_tool.py check-track <map>` checks the joints of every rail of a map.

- The 12 shapes: 4 straights and 8 curves. Shape s is drawn with direction
  `RAIL_DIRECTION[s]` = 0, 6, 13, 20, 27, 34, 41, 48, 55, 62, 69, 76 and has the end vectors
  `RAIL_ENDS[s]` (`map_tool.py`), in halves of the sprite: 35.5 px in x, 25 px in y. The engine
  takes the frame `((direction + 3) & 255) * 37 >> 8` of any direction: frame % 12 is the shape,
  frame / 12 the group (ramps, below) - `map_tool.py rail_of`.
- A rail's two network points lie a quarter of the size from its centre; each links to the
  point it finds three quarters out - the near point of the next rail only when that rail's end
  at the joint points the opposite way and its point there has the same height (`Rail_Action`,
  tables 0x47e908...; `RailMap_FindDot` wants the same z). **Two rails join only where their
  end vectors are opposite and their points are at the same height.** **[tool]** `track`
- So a turn needs a curve: horizontal -> diagonal is horizontal, curve (step 71, -25),
  diagonal (71, -50); vertical -> horizontal is vertical, curve (35.5, 50), curve (71, 25),
  horizontal. Consecutive steps may not turn more than 45 degrees (sharper means a reversal).
  **[tool]**
- A junction is two rails with the same centre - a straight and a curve - sharing their
  first point, the way mission 1 builds its junctions; the branch's first rail must join the
  existing rail by the end-vector rule. **[tool]** (first and last point checked)
- A point has at most 6 links ("Too many links"). The rail ends at a joint that point the same
  way at the same height share one point, which links to the far point of each of those rails
  and to the point of the rails ending the opposite way - so the limit is per point, not per
  joint: a crossing with branches has 7 or 8 rail ends at its joint and 3 or 4 links per point
  (`map_tool.py rail_point_links`). A rail laid twice adds no link. **[tool]**
- Heights: the points stand at the rail's z + 10 (altitude of VID 610); frame group 0 (and
  frame 36) is flat, group 1 raises the point of end 0 by 11 (sizeZ) and group 2 that of end 1:
  the ramps the missions use where the track climbs (l11, l15). `check-track` and the Track
  tool take the ramps and the heights into account; the tool lays rails at the height of the
  rail point it continues. **[tool]**
- **Ramps** (98 in the missions: l11 48, l15 60 - counted with their beds -, l12 3): a ramp is
  the shape's frame of group 1 or 2, direction (shape + 12 or 24) * 256 / 37 = 83, 89 ... 159
  (group 1) and 166, 172 ... 242 (group 2), the missions' directions exactly. The track goes on
  11 higher at the raised end. The bed under a ramp is the bed VID in the rail's own direction
  (91 of 98; 7 have a flat bed's direction), 17-18 below it as under a flat rail. **[editor]**
  PgUp / PgDn: the next rail climbs or descends, its far or near end raised; the ground under it
  slopes from one level to the other and the bed follows the rail's direction.
- **Swamp rails** (l06-l08, 1163 rails): VID 600 (altitude 7: its points stand 7 above it) on the
  swamp embankment VID 602 (TERRAIN, 71 x 50 x 11, 37 frames) in the same place and direction, 27-28
  below the rail; the rails stand about 28 px over the swamp's ground (l06: z 28 over GRID 0, l08: 27
  over 12), which is not flattened. A swamp rail joins the others by the same rule - its points
  are 3 lower than a plain rail's at the same z. **[editor]** V: swamp rails, laid 28 over the ground
  with their embankment 28 below, the ground left as it is.
- **Destroyed rails** VID 611 (plain, l01: 2) and 601 (swamp, l07: 24): the death VIDs of 610 and
  600 (maxHp 0). `Rail_Action` marks the points of a rail whose death VID number is smaller than its
  own `broken`: a train slows down to `Options.brokenRailSpeed` there (smoke VID 588) and, passing,
  `Rail_RepairAt` turns the destroyed rails around the point back into whole ones - so they are
  obstacles the trains, and the repair car at full speed, mend. **[editor]** V: destroyed rails;
  deleting one clears `broken` on the points the other rails keep.

## 4. Main line and its ends

- The main line goes on past the map edge at both ends (17 of 20 missions), so the trains
  come and go out of sight.
- The reinforcement's point is a VID 5 locomotive of army 3 standing on that hidden part
  (`ReinforcementInit` takes its place and destroys it; `BUGS.md` F12). Its `data` is an
  engine's rail position: two `<6hi` records (point x, y, z, next point x, y, z, distance <<
  16) and two car ids.
- **Level entry:** a push line from the hidden end onto the map, push_flag 1 - the arriving
  train is pushed in (l02 `SetPushLine(-107,1203,54,1465,1)`).
- **Level exit:** a short push line at the other end pointing into the map, push_flag 0 - no
  train may drive off the map (l05 `SetPushLine(1952,35,1952,60,0)`).
- Tested (sandbox): the reinforcement arrives from past the edge; a train sent past the exit
  stops at the edge (x 39).
- **[editor]** The editor's camera goes 300 px past the edges of the map, so the Track tool lays
  the main line on out of sight and the Push line tool picks the entry's and the exit's points
  there. The Arrivals panel's entries are rail points there too (a train is made on the point, at
  its height); "+ ENTRY LINE" lays the level entry from the entry to the first rail point 50 px
  inside the map, push_flag 1, with its semaphores (7). Its trains replace the reinforcement's
  point in new maps.

## 5. One-way sections (push lines)

`SetPushLine(begin_x, begin_y, end_x, end_y, push_flag)` (screen coordinates) marks the way
from the point nearest to begin to the point nearest to end: no train may enter it against
that direction (`RailDot_CanPass`); with push_flag 1 a train on it is pushed along. It fails
with "R_MAP: Can't PushLineFindDot" when the two points are not connected.

| Where | Direction | push_flag | Effect |
|---|---|---|---|
| level entry | from the hidden end onto the map | 1 | the reinforcement is pushed in |
| level exit | into the map | 0 | no train leaves the map |
| factory | from the factory's dead end along its spur (about 5 points) | 1 | a built train is pushed out; no train drives in |
| repair point | through the repair point's leg | 1 | the repair point is passed one way |

**[editor]** The Push line tool picks the two rail points and checks the line as `SetPushLine`
does (the nearest points with links, a begin with a link, a way between them - no "Can't
SetPushLine" or "Can't PushLineFindDot"); T switches push_flag, R turns the line round. The lines
are kept in the editor's block of `main()` (8).

## 6. Buildings

The factory, the repair point, the fort and the mining plant are nDir 8: the direction is chosen,
not forced to 0.

| VID | Rule | Source |
|---|---|---|
| 102 factory | At the dead end of a straight spur (6+ rails in the missions) that meets the line through a Y - a curve pair each way - standing on the track; direction = the way out (0 north, 64 east, 128 south, 192 west); a factory push line (5). The player's factory needs a build list (`items`): the game's 14 factory units, in the order of their hotkeys, are `5 10 20 25 80 85 45 30 35 82 97 90 75 62` (F1-F12, `[`, `]`); the panel shows ten in a row, LocoFix adds a second row for the 11th-20th. **[tool]** rail point within 100 px, build list. **[editor]** a new factory takes the build list of one of the same army on the map, or these 14; a warning without track within 100 px; its panel ticks the list (the game's order), the units it gets later and the player's limits (8). The enemy's AI (army 1 only) ignores the list and builds `level_queue_*` (the editor's plan, 8); a neutral or pirate factory builds nothing by itself (a map's plan orders it, 8), and only a neutral one is captured - on an editor map a train of the player's within 100 px of a neutral factory or repair point captures it (the runner's `CaptureTact`; the missions call `LDepoCapture`/`LMasterCapture` themselves) | l02, l05 (spur, Y, 128/0); sandbox test: a built train leaves through the Y |
| 104 repair point | On a track trains drive through (a loop leg), the gate over the rails, turned along the track (0/128 on vertical track); every end of the loop meets the line through a Y each way; the leg is one way (5). It repairs only the car nearest to it (50 px), so the one-way push line through it matters: the train is pushed through and every car is repaired fully as it passes; a train standing in a repair point without one gets only its nearest car repaired. **[tool]** within 20 px of track | l05; sandbox tests: without a push line the loco 45 -> 150 hp, the wagon behind stayed at 22; with it the loco 45 -> 150 and the wagon 22 -> 75 (full: the player's army has 1.5x hp) |
| 162 fort | A gate standing on the track (0-6 px in all 20 forts), turned along it (0/128 vertical, 64/192 horizontal, 32/96/160/224 diagonal). Guns (VID 115) on its wall: x +-47, z +92/+93 for 0/128; (-32, -24) and (+33, +23), z +93 for 32/160. It cannot be destroyed (U_OBJECT, maxHp 0); its guns can. **[tool]** within 8 px of track. **[editor]** a new fort comes with its guns at these places (64/192: (0, -33) and (0, +34); 96/224: (-33, +23) and (+33, -24), measured in l16 and l20), they move, turn and go with it; it snaps onto the line of the nearest straight track within 60 px, turned along it (R / E turn it round there), and is refused elsewhere. Its sockets: below | l10-l20; sandbox test: trains drive through it |
| 165 mining plant | Not on the track. One gun on its wall, on the side its direction picks (sockets below) | l09, l11, l14, l15, l18 (30) |
| 149 citadel | The pirates' keep (nDir 1, army 0 in the missions), a gate over the crossing of two diagonal tracks (they cross 8 px north of its middle). One gun on its top: the pirates' missile tower (VID 117) at (0, 0), z +112, of army 1. **[editor]** it snaps over the nearest diagonal track within 80 px, the track 8 px north of its middle; refused elsewhere | l04, l18 (3) |

**Guns and their sockets.** Every one of the 73 guns of the missions stands in a socket of a fort
(40), a mining plant (30) or a citadel (3) - a tower with a cannon (VID 110) or a missile tower (115),
on a citadel the pirates' missile tower (117); all 73 are army 1. The pirate walls (141) near some forts
(l09, l18, l20) carry none: the guns beside them stand 40 px from a fort, in its sockets. The gun
stands at the host's place plus the socket's offset, turned as the host:

| Host | Sockets (dx, dy), by the host's direction | z | The missions' fits |
|---|---|---|---|
| 162 fort | 0/128: (-47, 0), (+47, 0); 32/160: (-32, -24), (+33, +23); 64/192: (0, -33), (0, +34); 96/224: (-33, +23), (+33, -24) | +93 | two missile towers 8, a cannon and a missile tower 8, two cannons 4 |
| 165 mining plant | one, by direction / 32 (0 ... 7): (-46, +1), (-33, -23), (+1, -31), (+34, -22), (+47, +2), (+34, +23), (-1, +31), (-33, +24) | +94 | a cannon 17, a missile tower 13 |
| 149 citadel | (0, 0) | +112 | the pirates' missile tower 3 |

**[editor]** A new fort comes with two missile towers, a mining plant with a cannon, a citadel with
its tower; the object's panel (a right click) puts a gun into each socket, takes one away or changes
it. A gun has its host's army - on a citadel of army 0, as the missions' are, the enemy's - and moves,
turns, changes army and is deleted with it. A gun of the player's army fights for the player: in play
the enemy's trains shoot it.
**[editor]** A host is one object with its guns: a click on a gun selects the host and opens its
panel, whose row for each socket (SOCKET 1, SOCKET 2, numbered on the wall while the panel is open)
sets the gun in it at once.

**The citadel and its gate** (l04, l18). The three citadels of the missions stand with the same
pieces: the tower in its socket, two grates (146) on the two tracks through the citadel at (-34, +23)
and (+36, +25), 33 below it (sunk: the gate open; directions 128 and 0), and four corkscrews (142) at
(0, -49), (+2, +57), (-73, +3) and (+77, +5), 76-78 up; the citadel, the grates and the corkscrews are
army 0, the tower army 1. Nothing in the engine ties them: the kill system is the missions' script
(`pirates.lgc` `GratesTact`): while the tower stands, the first unit of the player's within 100 px of
the citadel is stopped and gets 300 damage, the grates rise 34 px in 1 s, stay up 1 s and sink in 1 s
(SFX 45); when the tower is destroyed the grates stay down and every corkscrew of the map breaks
(`ReplaceUnit(142,147)`: the citadel's, the border posts' and the level exits' alike; l18 waits until
all its towers are down). The crushers of `pirates.lgc` (143 / 144, 6b) are made by the script at
points of the track far from the citadels and are not part of them. **[editor]** a citadel is placed with all its pieces as one object
(moved, raised and deleted with them; a click on a piece selects it; its panel puts missing pieces
back); the faction is its tower's army (the missions' citadels are army 0 with an enemy tower): a new
citadel is placed in its faction's army - the enemy's from the palette - and its panel's ARMY row shows
and sets the faction; the runner's `CitadelTact` (8) works the gate of every citadel whose tower stands: the
first unit within 100 px of another army than the tower's, the neutrals aside; once no tower of the map
stands, every corkscrew breaks.

Measured over the 20 missions: all 37 factories and repair points stand on the track (the nearest
rail 1-38 px, one at 68); a factory's exit line runs 100-175 px from inside it along its spur, a
repair point's line 139-304 px through its leg; money crates (VID 169, army 3; `F169_8` gives the
player 100 when a unit of theirs takes it) lie on the track, 1-46 px from a rail (median 17); the
blockers (VID 150, the buffer stop, nDir 4) stand at dead ends - all 99 within 16 px of a rail point
with one link, median 5 px past it -, turned by the track's way: 0 on a vertical track, 64 (or 32)
rising to the right, 128 horizontal, 192 (or 160) falling to the right - four frames.
**[editor]** Enforced when placing: a factory snaps to the nearest dead end (22 px inside, turned the
way out) and gets its exit line (140 px, push_flag 1) and post; a repair point snaps to a straight
piece, turned along it, and gets its line (120 px each side, flag 1, one way the way it faces) and
posts - R / E turn it round on its leg, its line and posts with it; a crate snaps onto the track; a
blocker to the nearest dead end, 5 px past it, turned by the track's way; elsewhere they are refused.
Moving or deleting the building
moves or deletes its line and posts.

## 6a. Trains on the map

- **What stands there:** 197 cars of 18 VIDs in the 20 missions (all spriteClass ENGINE): the
  player's trains (army 0: a locomotive 5 or 10 with gun, mortar, anti-air, flamethrower, landing,
  zeppelin cars, the repair car 85, the ammo car 45), the enemy's (army 1), the neutrals' (army 2:
  25 tubs 60 with locomotives in l09, tankers 55/56 in l06, goods cars 95), the reinforcement's
  point (a locomotive 5 of army 3, 4) and the big stone on the rails (351, army 1, l01: a car that
  blocks the line; `Train_IsUnpowered` does not count it).
- **How the map stores a car:** SPR (VID, x, y, z, direction, army) and an SPRD of 40 bytes: the
  front and the rear `RailPos` (`<6hi`: the point's x, y, z, the next point's x, y, z, distance << 16)
  and the ids of the car in front (prevCar) and behind (nextCar), 0 at a train's ends. In all 20
  missions the couplings agree both ways. The car stands halfway between its two ends, which are
  `Weapon.length` (48; the stone 63) apart along the rails; the first car (no prevCar) drives the
  train. A car's z is its rail's + 10 (76 on rails at 66).
- **Commands:** a reinforcement's point has `33,x,y,0` (ACT_MOVE to the map) - the only commands
  the missions keep on cars. `Engine_Action` has no case for ACT_PATROL (36), so a patrol cannot be
  stored this way: the scripts give it - `civil.lgc`'s `CivilPatrol` sends a civil train (army 2)
  from point to point of `civil_train_x/y[]` with ACT_MOVE, the AI's enemy trains patrol with
  `PatrolEngine`.
- **[editor]** The Vehicles entry places cars on the track and couples them (Engine_AttachToTrain),
  U / C uncouple and couple, R turns a train round, a drag moves it along the track, the panel sets
  a patrol: `PatrolEngine(FindNearestSprite(GETSPRITE_VID + <first car's VID>, x, y, 20), to_x,
  to_y)` in main()'s editor block of the script. The saved cars are the missions' 40-byte records.

## 6b. Creatures and mines

**Charybdis** (l06, the only map with it). The body (254, army 0) stands with its roots (266, a
decal) at (-12, -4, -11), its teeth (255) at (-40, +27, +15), its eye (256, 100 hp - the only part
that can be killed) at (-34, +22, +56), four mortars (258) at (+5, +27, +64), (-6, -28, +61),
(+38, +6, +61), (-42, -7, +64) and two cannons (260) at (-9, +44, +45), (-67, +4, +45), all army 1
in play; the swamp track (a diagonal) runs straight through its middle (1 px; the nearest rail's own
middle is 26 px off). The script of l06 makes it work: every 500 ms
a unit not of army 1 within 90 px is pulled in (`ACT_MOVE` to the body) while the teeth chew (SFX
38), one of the player's within 40 px is eaten (`Destroy`); the guns sleep (behave bit 1 cleared)
until a gun or the eye is down to 80 % or a gun is lost; the eye's death (`F256_15` of
`ai_default_event.lgc`) blows it up and wins l06. **[editor]** placed from the palette (swamp) with
all its pieces as one object, the line of the nearest straight track within 100 px through its
middle as in l06, in its faction's army - the enemy's from the palette, its ARMY row the faction; the
runner's `CharybdisTact` (8) does what l06 does for every Charybdis: it pulls in every other army's
units and eats the player's (a Charybdis of the player's eats the enemy's).

**Crushers** (l04 2, l18 8; not in the maps). `pirates.lgc` `Pirate_Crasher` makes each one on a
point of a straight track: two stands (144, turned 64 * its form) and two drums (143) on both sides of
the track - from its middle the stands at +-S and the drums at +-C, S / C (0, 22) / (0, 30) across a
horizontal track (form 0), (-22, 15) / (-30, 20) across one falling to the right (1, rails of
direction 20), (-33, 0) / (-45, 0) across a vertical one (2), (-22, -15) / (-30, -20) across one rising
(3, rails 13); the missions have forms 1 and 3 only, their middles 0-20 px from a rail point. All the
map's crushers bite together: 25 s after the last bite the drums close half way in 300 ms (the units
within 20 px are stopped, SFX 33, the drums SALUT), chew 2 s and open in 300 ms, 200 damage a frame
to the units within 20 px meanwhile - of any army in l04, the player's in l18 -, and not while a unit of
army 1 is within 60 px of a crusher (the pirates' own trains pass). When the towers (117) are down
the drums die (143 -> 139) and the crushers stop. **[editor]** the crusher (pirates; its host is the
first stand) is placed across the straight track nearest within 60 px, its form by the track's way
(turned by its track), in its faction's army - the enemy's from the palette: the runner's
`CrusherTact` (8) bites the units of other armies (the neutrals aside), pauses while a unit of its own
army is within 60 px, and - its panel's STOPS WHEN: - stops with dead drums once the map had towers of
its army and none is left (ITS ARMY'S TOWERS FALL, as the missions', where the crushers stand with the
citadels; a crusher placed on a map with a tower of its army) or never stops (NEVER; one placed on a
map without such a tower, which the status line says) - kept in its first stand's direction, 8 * the
choice past 64 * its form.

**Scylla** (l06 2; `scillas.lgc`). The map holds the two stands (252) only - the designer's marks; the
script makes a stand 20 below its point and two jaws (253) +-(15, 10) beside it across the swamp
track: (-15, -10) direction 0 and (+15, +10) direction 128 on a track rising to the right (the stand's
direction 0), (-15, +10) 64 and (+15, -10) 192 on one falling (128). Both bite together from 3.5 s
into the mission, every 15 s: the jaws close in 300 ms (every ground unit within 50 px is stopped),
chew 2 s (an earthquake, SFX 57; the jaws jitter) and open in 300 ms, 200 damage a frame to every
ground unit within 50 px, of any army; it never dies. **[editor]** placed (swamp) on the diagonal
track nearest within 60 px, the stand on its line and the jaws 20 above it, its form by the track's
way, neutral (army 2) from the palette; the runner's `ScyllaTact` (8) bites as l06's - a neutral
Scylla every army, one given an army by its ARMY row all but that army.

**Fly nests** (265, l06). A nest (a unit, 225 hp, army 1) spawns nothing by itself; l06's script
does: the eastern nest waves of green flies (263) - 2 for every AA gun (VID 30) on the map, less up
to as many, at least 2 and at most 4 -, one a second from 25 s after the last wave of them was gone;
the western one grey flies (262) on guard - 1 or 2, 3 s after a unit of the player's (not a factory
or repair point) came within 400 px while none of them was left, and 6-9 when it is destroyed. The
flies (AVIA, weapon 28: 2000 px) find their targets themselves. **[editor]** a nest's panel sets
which of the two it is - kept as its direction (a nest has one, nDir 1, so it does not show): 0
waves, 64 on guard - and the runner's `NestTact` (8) spawns its flies in the nest's army.

**Mines.** The explosive mine (86) is what the repair car (85, one mine) lays: after 2 s
(`mineLayTime`) the engine makes the mine with the car as its parent - the car's army - and the lay
effect 595. Its VID flag 0x20000 hides a mine of another army than the player's (`Object_Construct`,
`Object_SetArmy`): **the engine shows a mine to its own army only**. `ai_default_event.lgc` runs it:
`F086_14` makes it unarmed, `F086_0` arms it once its car has gone and closes its rail point to its
own army (`SetSemaphore(x, y - z, 4 + army)`, 7), any other train within 30 px sets it off (VID flag
0x4000000: 1000 damage in 100 px), a repair car of another army takes it (`F086_DESTROY` opens the
point again). The mine marker (89, 23 on five maps) is the authors' placeholder: `TrainsTact` makes the
first five of a map an enemy mine one time in three and deletes them all. One mine stands on a map
(l12, army 1). **[editor]** the mine is placed itself, in the army chosen (the palette's army row),
on the rail point nearest within 40 px; every mine and marker is marked on the map with its army
(MINE: ENEMY, MINE 1 IN 3: ENEMY), the editor's drawing only - in play the engine hides it from the
other armies; a marker's panel makes it a mine. The palette has the mine, not the marker.

## 6c. Sets of the decorations

Some decorations are always placed with another object at a fixed offset (measured over the 20
missions, `piece - host`, the stored y; the survey's table is in DEVELOPERS.md "Editor mode"):

| host | piece | offset | how often |
|---|---|---|---|
| 145 border post | corkscrew 142 (l16: broken 147) | (+1, +1, +58) | 14 / 14 |
| 100 level exit | corkscrew 142 / stone on a pedestal 411 / none | (+1, 0, +106) / (+1, -1, +100) | 10 / 4 / 1 of 15 |
| 769 flagpole | flag 770 / 771, direction 32 (the wind) | (0, 0, +118) / (0, 0, +92) | 74 / 12 of 86 |
| 170 station | flag 773 / 770 on its ridge, direction 32 | by its frame: (0, -4) (3, -2) (3, 1) (0, 3) (-3, 2) (-3, -2) at 0, 32, 96, 128, 160, 224; +121 / +127 | 23 / 6 / 2 none of 31 |
| 631 oil derrick | orange glow 365 (the torch 883 is the engine's link) | (+1, -4, +150) | 30 / 32 |
| 880 factory chimneys | smoke generator 291 | (0, +2..+4), by its frame (nDir 7): +100, +114, +128, +140, +100, +114, +128 | 81 / 114 |
| 881 wide chimneys | chimney glow 555 | by its frame (nDir 3): (0, +4, +102), (0, +2, +102), (-1, +5, +82) | 29 / 29 |
| 928 pool | water 930; fountain 903 | (+1, +1, +26); (0, -2, +24) | 3 / 3; 2 / 3 |

A frame is the engine's: `((direction + 128 / nDir) & 255) * nDir >> 8`. Not sets (the authors
dressed them by eye or on one map): the mine annex 168 with a glow (61 %, offsets wander 10 px), the
ice houses' and the swamp chimney's smoke (under half), the pirate walls with fence posts (runs),
storeys of a building stacked on copies of it, piles of crates. **[editor]** each is placed, moved,
turned, raised and deleted as one object; a click on a piece selects the host; the panel has a row for
a piece of several kinds (CORKSCREW: WHOLE / BROKEN, ON TOP:, FLAG:, SMOKE:, FOUNTAIN:) and puts back a
missing one; a map's own records are found again by their offsets.

## 6d. Neutrals and pirates in a fight

**The engine** (the campaign's rules). A unit fires on its own only with `BEH_AGGRESSIVE` (behave
bit 1); its target comes from `Object_FindTarget` (0x449ea0), which takes a unit of another army
within the weapon's detect range - but never one of army 2 (the neutrals), unless the weapon has
flag 0x80 or the shooter is the enemy (army 1), the target a train and the script called
`SetEnemyCanAttackNeutralTrains(1)` (l02, l04, l17, l20). So no one fires at a neutral on its own,
the pirates (army 3) fire at ours and the enemy but not at the neutrals, and a neutral with the bit
fires at the player, the enemy and the pirates alike. The missions' neutrals (civil trains, robots,
the factories and repair points to capture) are unarmed or have the bit clear: they never fire, and
no mission script makes one fight back; a neutral fort or mining plant changes army when it is
captured (`F162_0` of `ai_default_event.lgc`, to the army of the train that took it).

**A map of the editor** (`#define FACTIONS 1` in its settings - the editor writes it, the template
has it - read by LocoFix `factions.cpp` as the level loads; a map without it keeps the engine's
rules):
- **A neutral never fires first.** When a unit of another army damages a neutral object (a shot's
  explosion, `Object_Action` 15, the projectile has its shooter's army), that object and every armed
  neutral that sees it (within its weapon's detect range) answer that army - only that one, as a
  captured neutral goes over to one army in the missions - to the end of the level; that army's units
  may then fire at them on their own as well. An armed neutral's panel: IT FIRES: WHEN ATTACKED (the
  bit set; new neutral units get it) or NEVER (clear: it does not answer either).
- **The pirates fight every side**: their guns and trains fire at ours, the enemy and the neutrals,
  and everyone fires at them. A unit that becomes a pirate's gets the bit.
- Checked in play (sandbox track, test maps of the `factions` branch): a neutral fort's missile tower
  beside the player's train fired nothing for 80 s, then the train was told to attack it - the
  towers answered at once, the train destroyed that one and the fort's other tower shot the train
  down; a neutral armed train
  patrolled through an enemy fort for 175 s with no shot either way; a pirate fort shot down a
  player's armed train (which destroyed one of its towers), an enemy's and a neutral locomotive; a
  pirate armed train and an armed train of ours, the enemy's or a neutral one 300 px apart fired at
  each other (the neutral one after it was hit).

## 7. Semaphores (VID 152)

- A post with a lamp (halo VID 154, light spot VID 155), army 0, on the ground, 16-29 px
  beside the track, turned along it. Two states: green (`ANI_STAND`, the default) and red
  (`ANI_STAND_OPEN`).
- Beside the track: 15-58 px from the nearest rail's centre (median 31) in the missions. **[editor]** A post placed snaps 24 px beside the nearest track point on the cursor's side (R / E: the other side), facing along the track so that it stands on the right of the trains it faces (as 184 of the missions' 188 posts: 12-40 px from the track, median 23); turned (R / E, the panel) it turns round and goes across the track.
- **Its line** (measured: each of the 188 posts with a push line end within 90 px - 3 have none - against the nearest end of its script's lines): of the 124 red posts, 107 stand at the end of a line coming from behind them (85 of them push_flag 1, 22 push_flag 0) - the trains a post faces would enter there against the line, so they stop at the post -, 10 at the begin of a push_flag 0 line toward their trains, 6 at an end with the line running across, 1 otherwise; of the 61 green ones, 29 stand at the end of a line coming from the trains they face (14 push_flag 1, 15 push_flag 0), 26 at the begin of a line going on (21 push_flag 1), 5 with the line across, 1 otherwise. An entry's pair shares one line: from past the edge to the pair, push_flag 1 (l02). **[editor]** A post placed gets such a line at once, 70 px, push_flag 1, drawn where it acts (a green post's line green; a red post's as a red mark in front of it, 35 px toward its trains, ending at the post with a stop bar - the engine's line lies behind it), unless it owns one already (a pair's) or stands at an end of a line no post owns; turning it, T and moving it make the line again, deleting it deletes it unless another post owns it; L / Shift+L give a post its line green / red. Checked in play: a red post stopped the trains it faces, the same post green let them through, and a pair on the main line (one line, 52,1299 -> 194,1299, flag 1) let a train in from the west edge and stopped it at the pair when it was sent back out.
- **Where:** at the ends of every one-way section (5) - 192 of the 196 posts of the missions
  stand within 90 px of one: a pair either side of the track at the level entry and at the
  exit, one at the end of each factory line, one at each end of a repair point line.
- **Which way:** a post faces the trains it is for - its direction points at them (a post of
  direction 0 is seen by trains going south).
- **Colour:** red where the one-way section forbids those trains' way, green where it lets
  them in: the entry/exit pair is red for the trains heading off the map and green for those
  coming in, a factory post is red for the trains that would drive in, a repair point line has a
  green post at its way in and a red one at its far end.
- **How the map stores it:** a red post has the command `10,0,0,0` (act 10 =
  `ANI_STAND_OPEN`, run when the level loads); a green post has no command. 126 of the 188
  posts of the missions are red. Nothing in the engine ties a post to the track - the editor
  must set the colour from the push lines. **[editor]** a red or a green post is placed from the
  bar, T switches one (the command is set or cleared); S of the Push line tool puts the posts of
  a line by these rules: 24 px to the right of the trains a post is for, facing them, red at the
  line's end (for the trains that would enter against it), green at its begin (none at a dead
  end); a line beginning past the map edge has the pair at its end (33 px into the map when the
  end is within 40 px of the edge); a post within 30 px of such a place is moved and coloured.
- The rail network's own semaphore is something else: `SetSemaphore(x, y, 4 + army, 0)`
  closes the link at (x, y) for that army (`RailDot_CanPass`; trains stop in front of it).
  The game uses it only for mines (an armed mine closes its point for its own army).
- Tested (sandbox): the posts with the command run animation 10, the others 0.
- VID 831 (`указатель стрелки`) is a decorative switch sign about 50 px from a junction
  (6 missions), with no code.

## 8. Mission script

- Before `#include "maps\script\header.lgc"`: `LEVEL`, `MONEY`, music, the environment
  (`NUKE 0` - lightning is on by default and flashes the screen purple, in a clear sky or while
  rain falls; `RAIN` 0 none, 1 now and then, 2 all the time - `daynight.lgc`'s comment has 1 and 2
  swapped -, of the kind `RAIN_TYPE` 0 rain, 1 snow, 2 clouds, 3 storm clouds, 4 meteorites; the
  day's start `START_DAYNIGHT` 1-4 day, evening, night, morning, 5 minutes each; gammas `GAMMA1..4`,
  `RAIN_GAMMA1..4`), the reinforcement
  (`PLAYER_REINFORCEMENT`, `player_reinforcement_train[]`), the AI (`start_trains*` -
  declare them even when empty, `BUGS.md` F10; `level_queue_*`, `LIMIT_ATACKERS`,
  `RENEW_TIMEOUT`, `FIRSTCONTACTTIME`, `FIRSTATTACKTIME`) and the texts of a map that is not a
  campaign level: `OBJECTIVES_FILE`, `WIN_FILE`, `LOOSE_FILE`, the Esc menu briefing
  `BRIEF_LIST` (a list of pages, CP1250, 44 characters a line) and `BRIEF_HEADER` (ASCII),
  and `HISCORES_FILE` - without them the map shows mission `LEVEL + 1`'s briefing and writes
  its wins into that mission's high score table. `CUSTOM_MAP` (the map's own file) keeps it
  out of the campaign: a win does not unlock missions or change the campaign scores, rank and
  hall of fame, the Esc menu's restart loads the map itself and the main menu comes back on
  the custom maps page (11).
- In `main()`, once (`iff(1)`): `InitDefault()`, `AddReinforcement()` when the point exists,
  every push line of 4-6, `SetCleverEnemyAttack(1)`, the camera (`SetPosition`).
- Every frame: `GameTact()`, `TrainsTact()`, and every 5 s the win check `!NoArmyUnits(1)`
  (it counts `ARMY1_NVID_ARRAY` of `constants.lgc`: cars, the factory, the fort's guns - not the
  fort, not the repair point) and the loss check `!NoArmyUnits(0) && IsNoDepo()`.
- `level_queue` times are the earliest: an entry is built when its time has passed, the enemy
  factory is free and fewer than `LIMIT_ATACKERS` trains attack (sandbox: 240 / 420 s -> 280 /
  456 s). The AI's `BuildTrain` swaps a train's first two cars (LIGHT GUN `{5,20,20}` leaves the
  factory as 20,5,20), the types ending in 5 change with the difficulty, and an enemy factory's
  destruction drops the train the AI is building in another one. A factory needs its push line:
  without it each new car is made on the last one, which is destroyed.
- A script path needs two backslashes (`"music\\mus08.ogg"`, `BUGS.md` F11).
- **[editor]** The editor keeps all of these settings - and the push lines of `main()` - in two
  marked blocks of the script, `// <editor>` ... `// </editor>` before `header.lgc` and
  `// <editor main>` ... `// </editor main>` in `main()`'s `iff(1)`, and writes them anew from File >
  Mission settings... and the Push line tool when the map is saved; the rest of the script is
  kept. `map_tool.py new --script` writes a script with both blocks (the template, and the
  sandbox has them too); a script without them is only read. What a mission needs besides the
  settings goes outside the blocks: its own car lists of the train types (`int this_trainNN[]`)
  before `// <editor>`, as `TrainsTactDeclares.lgc` takes them only when declared before it, and
  no `#ifdef` inside a block (a save writes the block without it). The enemy's trains have no
  object on the map (the AI makes them while the level runs); the editor marks where they start,
  their patrols and the factory that builds them.
- **[editor]** Train arrivals (the Arrivals panel): arrays `arrival_*[]` and `ARRIVALS` in the
  first block (a column per arrival; its own cars as `int this_trainNN[]`, a type no template uses,
  the enemy's posts and patrols as `ATA_STANDn_X/Y` and `ATA_PATROLn_*`), the factories' later
  units (`UNLOCKS`, `unlock_*[]`, `ACT_ADD_ITEM` at their time) and the unit limits (`UNIT_LIMITS`,
  `limit_*[]`: `AddUnitLimit(max, nvid, army)` - its arguments are swapped against export.lgc, and
  the `LIMITxx` defines `InitDefault` reads would set the wrong slot), run by `ArrivalsTact()`
  of a block `// <editor arrivals>` after `header.lgc` and called from `// <editor tact>` after
  `TrainsTact();`. The same block's `PiecesTact()` (every frame, from `ArrivalsTact`) works the
  citadels, Charybdis, the fly nests, the crushers and Scylla of the map (6, 6b); it finds them at
  the start by their towers, eyes, nests and stands (the stands with `FirstSprite`, once) and logs
  `CITADEL`, `CHARYBDIS`, `NEST`, `CRUSHER` and `SCYLLA` lines in `error.log`. The win and loss of the template wait for `ArrivalsLeft(army)`. Limits: the AI
  has 16 trains (`ai_trains`), 10 patrols and 10 posts for the start trains, the queue and the
  arrivals together; only army 1 has an AI.
- **[editor]** The factories' plans (a factory's panel): `PLANS`, `plan_*[]` and `PRICES`,
  `price_vid/price_of[]` in the first block, run by `PlanTact()` of the same runner. The enemy's
  entries go into `level_queue_*` (empty at the start, a place for each) when they are due - at a
  time, after the entry before, while a condition holds, once, n times or without end - with their
  cars' first two swapped so that `BuildTrain` makes the planned train, and never a type ending in
  5. Ours, the neutrals' and the pirates' factory gets `AddCommand(f, ACT_BUILD_UNIT, v)` car by
  car, each when the factory is free and its army has the price (a car the money does not cover
  would be left out), whatever its build list; one entry at a time, and not while a train stands at
  the factory's door (its cars would join it). An arrival's cars are made one at a time too: cars made
  on one rail point at once push each other out of their order. The AI's knobs: `LIMIT_ATACKERS`, `RENEW_TIMEOUT`,
  `start_trains_renew`, `start_trains_random_task`, `FIRSTATTACKTIME`, `FIRSTCONTACTTIME`,
  `CLEVER_ATTACK` (`SetCleverEnemyAttack`) and `STG_DESTROY_TIME` (read only by mission 2's own code).

## 9. Ordering of objects

The ground object first, then the rails, then the rest (`objects.tsv` is the creation order).

## 10. Checking a map

1. `map_tool.py check <game>` - every map and image of the game still round-trips.
2. `build.py` - the map packs.
3. `test_missions.ps1 -Quick` - the campaign still loads.
4. `test_game.ps1 -Map maps\<name>\<name>.map` - `error.log` has no `R_MAP` or `LOGIC` line
   (the "Invalid resource type" at load and the "EmptyVid" at exit are harmless, `BUGS.md`).
5. The sandbox's tests, switched on in the game folder's copy of its script:
   `SANDBOX_DRIVE_TEST` drives trains over every part of the track and logs their positions
   ("DRIVE"), `SANDBOX_WIN_TEST` / `SANDBOX_LOSE_TEST` damage the train, check the repair
   ("END ... hp") and end the mission with its own text.

## 11. On the custom maps page

The second page of the strategic map in the main menu lists the maps of `maps\custom.lst`
(`game/maps/custom.lst` in the repository; DEVELOPERS.md "The custom maps page"). To put a map there:

- The map lives in `maps\<folder>\` as `<folder>.map` with `<folder>.lgc` - what `build.py`
  makes of `game/maps.new/<folder>/`.
- Its briefing: `maps\<folder>\brief.lst`, the pages one per line (CP1250, 44 characters a
  line), as for `BRIEF_LIST` (8); the list shows them under the map's title and the start
  button loads the map.
- Two lines in `custom.lst` (ASCII, CRLF): the folder, then the title (upper case, the menu
  font). The list has any length: the page shows six maps under the editor's row and scrolls.
- The script defines `CUSTOM_MAP "maps\\<folder>\\<folder>.map"` (8) and its own texts and high
  scores (`OBJECTIVES_FILE` ... `HISCORES_FILE`), so it plays as a mission of its own.
- **[editor]** A map the editor saves as `maps\<folder>\<folder>.map` gets all of this: its
  script's block, the texts (capitals, CP1250, 44 characters a line), `brief.lst` and its line in
  `custom.lst` (not the template).
- **Exchange [editor]:** File > Export map packs `maps\<folder>\` into one `.zip`
  (`<folder>/<file>`, deflate): the map, its own ground image if it has one, the script, the texts
  and the briefing - not `hiscores` / `hiscores.txt` (the install's) and not a `.vid` the map does
  not use. A map on a campaign terrain points at `maps\lNN\lNN.vid`, which every install has: its
  package holds no picture of the game (a few KB; one with a painted ground a few MB). File >
  Import map unpacks such a file into `maps\<folder>\` and adds its line to `custom.lst`; the
  folder of a map already on the page is overwritten only when the player says so, otherwise -
  and always for the game's own folders - the map comes as `<folder>_2` ..., its files, the paths
  in its script and lists (`maps\\<folder>\\<folder>.map` ...) and its ground record renamed.

## 12. Not known yet

- Ramps and terrain with heights (rail frame groups 1 and 2).
- Switching a semaphore or closing track while the game runs (possible from a script:
  `Action(post, ACT_CHANGE_ANIMATION, ANI_STAND_OPEN)` and `SetSemaphore`; no mission does it).
- The meaning of the commands in `objects.tsv` beyond animations (act < 17) and the engine
  and region data.
