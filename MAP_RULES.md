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
  18 below it. **[editor]** new track gets one (B picks which), deleted with its rail.
- **[editor]** A new map (File > New) is an empty map - a plain ground of one texture of the
  painting's, flat, nothing on it -, a generated meadow, or a campaign map's terrain with only the
  ground object; an empty map or a meadow is any size from 512 to 4096 in steps of 256 (the ground's
  tile; - and + or a preset: 1024 x 768, 2048 x 1536, 3072 x 2304, 1536 x 1024, 4096 x 3072) with the
  GRID flat at 70 and the picture's height 1586. File > Map size grows (a meadow beyond the old
  picture) or crops a map at its right and bottom edge, with the same size row.
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
  the main line on out of sight, the reinforcement's point is placed there and the Push line tool
  picks the entry's and the exit's points there.

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
| 102 factory | At the dead end of a straight spur (6+ rails in the missions) that meets the line through a Y - a curve pair each way - standing on the track; direction = the way out (0 north, 64 east, 128 south, 192 west); a factory push line (5). The player's factory needs a build list (`items`): the game's 14 factory units, in the order of their hotkeys, are `5 10 20 25 80 85 45 30 35 82 97 90 75 62` (F1-F12, `[`, `]`); the panel shows ten in a row, LocoFix adds a second row for the 11th-20th. **[tool]** rail point within 100 px, build list. **[editor]** a new factory takes the build list of one of the same army on the map, or these 14; a warning without track within 100 px | l02, l05 (spur, Y, 128/0); sandbox test: a built train leaves through the Y |
| 104 repair point | On a track trains drive through (a loop leg), the gate over the rails, turned along the track (0/128 on vertical track); every end of the loop meets the line through a Y each way; the leg is one way (5). It repairs only the car nearest to it (50 px), so the one-way push line through it matters: the train is pushed through and every car is repaired fully as it passes; a train standing in a repair point without one gets only its nearest car repaired. **[tool]** within 20 px of track | l05; sandbox tests: without a push line the loco 45 -> 150 hp, the wagon behind stayed at 22; with it the loco 45 -> 150 and the wagon 22 -> 75 (full: the player's army has 1.5x hp) |
| 162 fort | A gate standing on the track (0-6 px in all 20 forts), turned along it (0/128 vertical, 64/192 horizontal, 32/96/160/224 diagonal). Guns (VID 115) on its wall: x +-47, z +92/+93 for 0/128; (-32, -24) and (+33, +23), z +93 for 32/160. It cannot be destroyed (U_OBJECT, maxHp 0); its guns can. **[tool]** within 8 px of track. **[editor]** a new fort comes with its guns at these places (64/192: (0, -33) and (0, +34); 96/224: (-33, +23) and (+33, -24), measured in l16 and l20), they move, turn and go with it; a warning without track within 40 px. Its sockets: below | l10-l20; sandbox test: trains drive through it |
| 165 mining plant | Not on the track. One gun on its wall, on the side its direction picks (sockets below) | l09, l11, l14, l15, l18 (30) |
| 149 citadel | The pirates' keep (nDir 1, army 0 in the missions). One gun on its top: the pirates' missile tower (VID 117) at (0, 0), z +112, of army 1 | l04, l18 (3) |

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

Measured over the 20 missions: all 37 factories and repair points stand on the track (the nearest
rail 1-38 px, one at 68); a factory's exit line runs 100-175 px from inside it along its spur, a
repair point's line 139-304 px through its leg; money crates (VID 169, army 3; `F169_8` gives the
player 100 when a unit of theirs takes it) lie on the track, 1-46 px from a rail (median 17).
**[editor]** Enforced when placing: a factory snaps to the nearest dead end (22 px inside, turned the
way out) and gets its exit line (140 px, push_flag 1) and post; a repair point snaps to a straight
piece, turned along it, and gets its line (120 px each side, flag 1, one way the way it faces) and
posts; a crate snaps onto the track; elsewhere they are refused. Moving or deleting the building
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

## 7. Semaphores (VID 152)

- A post with a lamp (halo VID 154, light spot VID 155), army 0, on the ground, 16-29 px
  beside the track, turned along it. Two states: green (`ANI_STAND`, the default) and red
  (`ANI_STAND_OPEN`).
- Beside the track: 15-58 px from the nearest rail's centre (median 31) in the missions. **[editor]** A post placed snaps 31 px beside the nearest track, facing along it; L / Shift+L on a post attach a line that lets its trains in (green) or keeps them out (red).
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
  (`NUKE 0` - lightning is on by default and flashes the screen purple; `RAIN`, day/night
  gammas `START_DAYNIGHT`, `GAMMA1..4`, `RAIN_GAMMA1..4`), the reinforcement
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
  456 s).
- A script path needs two backslashes (`"music\\mus08.ogg"`, `BUGS.md` F11).
- **[editor]** The editor keeps all of these settings - and the push lines of `main()` - in two
  marked blocks of the script, `// <editor>` ... `// </editor>` before `header.lgc` and
  `// <editor main>` ... `// </editor main>` in `main()`'s `iff(1)`, and writes them anew from File >
  Mission settings... and the Push line tool when the map is saved; the rest of the script is
  kept. `map_tool.py new --script` writes a script with both blocks (the template); a script
  without them is only read.

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
