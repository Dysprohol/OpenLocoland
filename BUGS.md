# Bugs of the original game

Everything found while reverse engineering Locoland and testing it, in one list, kept up to
date as the code becomes readable. Each entry says where it is (function, address) and what
shows it.

How an entry moves through the list:

- **Suspected** - looks wrong in the code we have, but the whole picture is not known yet
  (the decompiler can mislead, a caller may guard against it, a script may rely on it).
  Stays here until the surrounding code is understood.
- **Confirmed** - checked in the machine code and the situation that triggers it is known.
  Obvious ones with a small, safe fix are fixed right away; the rest wait until the code
  around them is rewritten.
- **Fixed** - with where the fix is (LocoFix, data patch, text patch).
- **Quirks** - wrong-looking behaviour that scripts or saved maps may depend on. Kept as it
  is; a rewrite has to reproduce it unless every use is checked.
- **Checked, not bugs** - looked like a bug, turned out not to be, so nobody checks again.

Problems of running on current Windows (DirectDraw modes, window size...) are listed at the
end; they are not bugs of the game itself.

## Fixed

| # | What | Where | Fix |
|---|---|---|---|
| F20 | `ResFile_CopyChunk`, which `Level_Save` uses to copy the OBJ chunk (the level's own VIDs) from the loaded map, copies the chunk as one record of `chunkSize` bytes read from the first record's data on - but `chunkSize` also counts the 12-byte chunk header and the 4-byte record size, so it takes 16 bytes too many: every save appends the pad byte and the header of the next chunk (`\0GRPH...`) to the OBJ record, and a map saved again and again grows by 16 bytes each time (harmless to the loader, which stops after the image file name). A chunk of several records would come out as one record holding them all | `ResFile_CopyChunk` 0x4370b0 (`size = source->chunkSize` after `ResFile_FindFirst`) | LocoFix `level_save.cpp`: the function is rewritten to copy record by record with the game's `ResFile_FindFirst`/`NextRecord`/`BeginRecord`/`EndRecord`; a saved sandbox or mission 1 keeps the size of the original byte for byte |
| F19 | A ground the engine made itself (`World_CreateEmptyGround`: a map without VID 1024 that has decals) carries the level flag 0x200 like the VIDs read from the map (imageFlags 0x225), so `Level_Save` copies OBJ from the loaded map, which does not describe that ground | `GroundVid_ConstructEmpty` 0x452f00, the loop at 0x41e2bc | LocoFix `level_save.cpp`: the self-made ground (its name "Self Created Hardware Prerendered Ground") loses the flag for the save |
| F18 | `Level_Save` creates the target first (`ResFile_Create` at 0x41e23b truncates it) and then copies OBJ from the loaded map; a save that fails on the way leaves a broken map, and when the loaded map cannot be opened it shows "Can't open file ... needed for ..." and writes the map without its VIDs | `Level_Save` 0x41e0b0 | LocoFix `level_save.cpp`: every save goes to `<target>.saving` next to the target and replaces it (`MoveFileEx`) only when it was written - a read-only target stays as it was (checked); a level whose VIDs come from a map file that is gone is not saved (checked) |
| F17 | `Level_Save` recognises a save over the loaded map by comparing the names byte by byte, so the same file under another spelling (`Maps\L01\L01.map` for `maps\l01\l01.map`) counts as another file: `ResFile_Create` truncates it, and the OBJ chunk is then copied from the truncated file - the saved map loses its ground and the original is gone | `Level_Save` 0x41e0b0, the byte compare at 0x41e17d | LocoFix `level_save.cpp`: the target is never written before the save is complete (F18), so the loaded map is intact while OBJ is copied from it; checked by saving the loaded sandbox as `Maps\Sandbox\SAVED.MAP` |
| F16 | Saving over the loaded map first renames it to `tmp_del!.map` in the current directory (OBJ is copied from it later) and sets `World.levelFile` to that name; when the new file cannot be created, the map stays under `tmp_del!.map` and the level believes it was loaded from there | `Level_Save` 0x41e0b0, `rename` (`crt_4580f1`) at 0x41e205 | LocoFix `level_save.cpp`: the save never names the loaded map (it writes `<target>.saving`), so the rename is not reached; `levelFile` stays the map's own name |
| F15 | The player data of the scripts - players, progress, scores, rank, difficulty, sound settings and the menu's `return` flag - was not saved on current Windows 11 (26200, a normal user). `SetReg`/`GetReg` (183/182) use `SOFTWARE\Gromada\steam\Players` without a hive prefix, so HKLM, which a normal user may only read; the game relied on UAC registry virtualization to send the writes to the user's VirtualStore. There every `RegCreateKeyExA` on an `HKLM\SOFTWARE` key fails with `ERROR_ACCESS_DENIED` (5) from a 32-bit process without a manifest, whatever the access mask (`KEY_ALL_ACCESS` as `SetReg` asks, `KEY_SET_VALUE`, `KEY_READ \| KEY_WRITE`), although the token has virtualization allowed and enabled, `EnableVirtualization` is 1, `luafv` runs and the keys have no `REG_KEY_DONT_VIRTUALIZE`; reads no longer see the VirtualStore either. `SetReg` ignores the error, so nothing was saved and `GetReg` read the real HKLM key. Older VirtualStore values (`s0l1`, `s0l15`) show that it once worked | `Registry_ParsePath` 0x427340 (the hive), `SetReg` 0x436400 (`RegCreateKeyExA`, result not reported), `GetReg` 0x436130 | LocoFix `player_registry.cpp`: `Registry_ParsePath` sends that key (and keys under it) to `HKEY_CURRENT_USER\SOFTWARE\Gromada\Locoland\Players`, next to the game's settings; at the first start that key is filled from the HKLM key with the VirtualStore values over them (the view the game had while virtualization worked), so the player keeps the progress |
| F14 | In a window the game saves the window rectangle on exit (`WM_DESTROY`: `GetWindowRect`, `WindowPositionX/Y` in the registry) also when the window is minimised, where Windows keeps it at (-32000, -32000); the next start creates the window there, out of sight on every monitor | `World_HandleMessage` 0x41c360, `GetWindowRect` at 0x41c423; read back in `World_Construct` for `CreateWindowExA` at 0x41a832 | LocoFix `window.cpp`: a saved position on no monitor (`MonitorFromRect`) is treated like none - the window starts in the middle of the work area |
| F13 | The mouse position of `WM_NCHITTEST` is read without its sign: `and ecx,0xffff` and `shr eax,0x10` split lParam, so a cursor at a negative screen coordinate - on a monitor left of or above the primary one, or in a minimised window, which Windows moves to -32000 - comes out 65536 px off and is clamped to the right or bottom edge of the view. The game then sees the HUD bar (VID 450) under the cursor instead of the map: no unit is picked (no tooltip), a click selects nothing, a right click gives no order, and the map scrolls toward that edge | `Mouse_HandleMessage` 0x417a10, 0x417a77 and 0x417a8f | LocoFix `mouse_coords.cpp`: `movsx ecx,cx` + 3 nops and `sar eax,0x10` (same length) read the position signed; found by a test whose window was minimised |
| F12 | `ReinforcementInit()` (called by `InitDefault`) takes the first VID 5 locomotive of army 3 in the map, keeps its place in `player_x/y/z` (`reinforcement.lgc`) and destroys it: that locomotive is the point the reinforcements arrive at, not the unit the player drives, so a mission that puts the player's locomotive there loses it | `maps\script\reinforcement.lgc` lines 9-20 (`FirstUnit(u_monster)`, `Destroy`), `maps\script\initdefault.lgc` line 96 | `tools/map_tool.py` (`MISSION_SCRIPT`) and `game/maps.new/template/template.lgc`: `PLAYER_REINFORCEMENT 1`, `player_reinforcement_train[]={5,0}` and `AddReinforcement()` after `InitDefault()` when the point exists; DEVELOPERS.md "A map to play" |
| F11 | A path in a script string written with one backslash loses it: `#define music0 "music\mus08.ogg"` makes the game look for `musicmus08.ogg` (the sound log prints that name) and the level has no music | the strings of a mission script; `maps\l01\l01.lgc` line 1 writes `"music\\mus08.ogg"` | `tools/map_tool.py` (`MISSION_SCRIPT`) and `game/maps.new/template/template.lgc`: two backslashes in the music paths; DEVELOPERS.md "Script machine" |
| F10 | A mission script that does not declare `start_trains` gets the default `int start_trains[]={0};` of `TrainsTactFunctions.lgc`, so `TrainsTact()` creates a train of type 0 (a locomotive and two wagons of `this_train0`) of army 1 at map (0, 0) - in an empty map a red train stands in the corner of the view | `maps\script\TrainsTactFunctions.lgc` line 7 (`#ifndef start_trains`), `maps\script\TrainsTact.lgc` lines 86-88 (`StartCreateTrain`) | `tools/map_tool.py` (`MISSION_SCRIPT`) and `game/maps.new/template/template.lgc`: `int start_trains[]={-1};` and the other `start_trains_*` arrays declared before `header.lgc`, as mission 1 does |
| F9 | `String_ToInt("0x")` (a text with `x` as its second character but no number) returns the address of the String object: the output slot of `sscanf` starts as `this` and a failed `%i` leaves it unchanged. `World_PopInt`, `World_PopObject` and `Script_PopObject` do the same with the World or the script machine in the slot, `World_PopVid` with the text of the caller | `String_ToInt` 0x43d8c0, `lea ecx,[esp]` at 0x43d8d4; `World_PopInt` 0x4429c0, `push ecx` at 0x4429c0; `World_PopObject` 0x442a40; `Script_PopObject` 0x442670; `World_PopVid` 0x442ae0, `lea edx,[esp+4]` at 0x442b18 | LocoFix `script_value.cpp`/`script_value.h` and `world_script.cpp`/`world_script.h`: all the functions are rewritten in C++ and return 0 when `%i` converts nothing |
| F8 | Script `a % 0` divides by zero: the modulo case of the binary opcode has no zero check (unlike `/`), so its `idiv` raises `EXCEPTION_INT_DIVIDE_BY_ZERO`; `INT_MIN / -1` and `INT_MIN % -1` overflow the same way | `ScriptValue_BinaryOp` 0x43da60, `idiv edi` at 0x43db74 (modulo) and 0x43db4f (division) | LocoFix `script_value.cpp`: the function is rewritten in C++; `% 0` returns 0x0FFFFFFF like `/ 0`, `INT_MIN / -1` returns INT_MIN and `INT_MIN % -1` returns 0 |
| F7 | Script `Log(text)` uses the text as the printf format: a `%` in a logged text reads stack garbage or crashes on `%s` | `Script_CallExtern` case Log (0x4405d1, extern 154), `call Log_Write` at 0x4405fb | LocoFix `script_log.cpp`: replaces the call with a trampoline that passes `"%s"` as the format (`Log_Write(g_log, "%s", text)`) |
| F6 | Script `Random(-1)` divides by zero: modulo by `(max + 1)` raises `EXCEPTION_INT_DIVIDE_BY_ZERO` when max is -1 | `Script_CallExtern` case Random (0x440608, extern 155), `idiv edi` at 0x440645 | LocoFix `script_random.cpp`: replaces `call crt_rand` at 0x44063a with a trampoline that turns a zero divisor into 1, so a negative max returns 0 |
| F1 | Sounds of `objects.res` point to `wav\...` files missing from every release (Polish box 2003, Steam PL/EN); missions log "Invalid nsfx" and the engine searches for them | `objects.res` SFX | `patch_data.py`: `wav\null.wav` (the game's own empty sound) unless the file exists in the assembled game. 25 references, 24 files (`shotgun_reload` appears twice, with a different case): env_desert_day1, env_desert_night1, env_luga_day1, env_swamp_day1, meteor_fall, mill, mouche_shot1, mouche_shot2, napalm, night, onchangescreen, rat_sal, repair_railroad (`repair_rail.wav` exists), robot_salute, shot4, shot6, shotgun_reload, start_machine_gun, stonefall, temple, tunnel, waterfall, wind2, zavod. Scripts call only the daytime ambiences 136, 140, 141, 143 (missions 4, 6-8, 10, 11); `arsenal_working.wav` exists with no references |
| F2 | Fort cannon (`vid\110.vid`): the birth animation (rising out of its pit) is rendered only for directions 0-19 of 32 and the file ends early; `Vid_Read` drops the animation ("noCadr for noAnimCadr and noDir"), a built cannon just appears | data | `patch_data.py`: directions 20-31 reuse existing frames |
| F3 | Objects created facing direction 0 (the `CreateSprite` default) and VIDs with one direction play only the first frame of their birth/death animation: `Object_Construct` sets the last frame to count - 1 without the first frame, and `Object_SetDirection` skips an unchanged direction | `Object_Construct`, 0x445a28 | LocoFix `birth_animation.cpp` (mov -> add) |
| F4 | English `strings.ini`: key 251 duplicated (250/251 = Water Dragon); gaps in the unit and button texts | `strings.ini` (EN) | `lang/en/strings.override.ini` |
| F5 | `VidConverter_BuildShadowPoints` (0x42b670): when the second `new` (the "visited" map) fails, the first buffer (shadow points, width * height * 4 bytes) is leaked; the function logs "dot without shadow" and returns 2 without freeing it | 0x42b670, leak at 0x42b757-0x42b772 | LocoFix `shadow_leak.cpp`: frees psVar3 (at [esp+8] after the three pops) before returning 2 |

## Confirmed

| # | What | Where | Status |
|---|---|---|---|
| C1 | `Engine_AdoptNeighbourCommand`: the second branch (take the command of the car in front) reads the command and goal of the car **behind** (edx = nextCar instead of eax = prevCar). With no car behind it reads address 0x28 and the game crashes; with a car behind it copies the wrong command. Called by `Building_UpdateWorkshop` when the repair point takes over a wagon whose goal is a car of its new army | 0x407d46 (`mov eax,[edx+0x3c]` / `mov edx,[edx+0x28]`) | Fixed in LocoFix `adopt_command.cpp` |
| C2 | `Player_CanBuild` tests the per-army unit limit (`vid->maxUnitArmy[army]`) but compares the per-army count against the **total** limit `vid->maxUnit`. A VID with a per-army limit and no total limit (`maxUnit = -1`, what `Vid_ResetCounters` sets, and what script `AddUnitLimit` (249) leaves behind) can never be built: `count[army] + queued < -1` is always false, so the factory build list greys the entry out for good. With both limits set the per-army limit does nothing (`GameWorld_CreateObject` enforces only `maxUnit`, this is the only read of `maxUnitArmy`). No shipped mission is affected - every `#define LIMIT..` in the scripts is commented out, so the per-army limits look abandoned during production rather than tested. The fix is kept because it is small and changes nothing for the shipped missions | 0x432492 `mov ecx,[esi+0x344]` (maxUnit, while `ecx` already holds `maxUnitArmy[army]` from 0x432480) | Fixed in LocoFix `unit_limit.cpp` (6 nops, same length) |

## Suspected

Nothing at the moment - the open questions were either answered or are quirks of the design
(see below).

## Quirks (keep)

Script engine functions (DEVELOPERS.md "Script machine"):

- `SetPositionCenter(x, y)` in `maps\script\ai_functions.lgc` adds half the screen to the point
  before calling `SetPosition`, which already takes the centre of the view, so it would move the
  camera half a screen right and down. No script calls it.

- `AddUnitLimit` (249) uses army and value swapped: the limit stored is the `army` argument,
  the slot it goes to (255 = the total limit, else `maxUnitArmy[value]`, not range-checked) is
  the `value` one (see C2, which is about the only reader of a per-army limit).
- `TrainProperty` `PROP_FREE` always returns 1 and leaves a value on the stack per busy car.
- `FEof` returns 16, not 1.
- `CanMoveEngineTo` / `CanAttackEngine` (253/254) are stubs; `SetAutoReBirth` (247) does
  nothing; `Exit` ignores its argument; `SetGraphDetail` (139) does nothing.
- `FirstSprite` looks only at layer 0; `FirstTrain` clamps the army to 0-3, so the "any army"
  value 4 of `Train_FindNth` is unreachable.
- `GetVidData`/`SetVidData` accept 17 child entries, not the 32 the comments of export.lgc
  promise.
- A positive `SetGamma` byte darkens, a negative one brightens.
- Declared but not implemented ("Unknown extern"): 81 `ReCalcGridZ`, the network functions
  192-197, 210 `GetStatistics`, 211 `CalcStatistics`, 248 `PatrolEngineToPoints` - no shipped
  script calls them.
- About 50 engine functions that return nothing push nothing on the script stack; used in an
  expression they would read whatever is there. The VM is untyped and does not check it, and
  no shipped mission script does it (checked in every `maps\*.lgc` and `maps\script\*.lgc`).

Engine:

- `SCROLL_WITH_MOUSE` is declared in export.lgc but was never implemented (LocoFix adds
  middle-button panning instead).
- `Effect` 4 (`EFF_OPEN_WINDOW`) and 8 (`EFF_SCALE_WINDOW`) do nothing, and 6
  (`EFF_PIXELAPPEAR`) and 7 (`EFF_LINESHIFT`) only stop `Graphics_EndScene` from presenting
  (0x414000.c:651): `Graphics_DrawEffects` has no code for them, so the picture stays frozen
  until `Effect(0)`. No shipped script uses them - every `Effect` call passes 0, 1
  (`EFF_NUKE`), 3 (`EFF_FADE`) or 5 (`EFF_ALPHAAPPEAR`) - but a custom map should keep to
  those or 2, 9, 10, which are drawn.
- `Depo_ReleaseUnit` (0x405290) deletes every object standing where the new car appears (the
  AskPlace loop), whatever army it belongs to: a factory keeps its exit pad clear, so a tight
  map cannot jam a production queue - a standard choice for fixed track geometry, and blocking
  production instead would trip the AI.
- `Train_StartRoute` starts only when the integer acceleration
  `(int)(TrainStats.power / TrainStats.mass * 8)` is over 7. The truncation makes that
  `power >= mass`: a locomotive must have at least as much power as the whole train mass.
  VID 97 (power 61, mass 60) is balanced as a solo engine and refuses a 5-mass wagon
  (61/65*8 = 7.5 -> 7), as designed.
- `Graphics_Init` always clears the flags 0x10 (GDI cursor), 0x20 (software device) and 0x40
  (triple buffering) - those paths are dead code.
- The log goes to `logs\error<date> <time>.log`, but the installed game has no `logs`
  directory, so the open fails and the game falls back to the same name next to the exe
  (the deliberate second attempt in `Log_Construct`); on a normal exit the run's log is
  renamed to `error.log` (`Log_Destruct`), so only crashed runs leave their dated file.
- `Level_Save` (script `Save()`) is meant for the editor mode (DEVELOPERS.md "Editor mode"): outside
  it, the level it writes has lost its decals (spriteClass 8 objects are baked into the ground
  and removed as they are created), GRID holds the ground raised under the buildings
  (`Object_Construct` calls `World_SetGroundZ` for VID flag 8) and the objects are where the
  running mission has taken them - checked with a temporary script calling `Save()` in mission 1:
  157 objects of 212, the 69 decals gone, another GRID. In editor mode with the level held the
  same map comes back object for object and with the same GRID.
- The rails of the missions, as the engine links them (`map_tool.py check-track`, which models
  `Rail_Action` with the ramps and the point heights - DEVELOPERS.md "Trains and rails"; its links are those
  the game logs with the editor's test command `D` on l11, l12 and l15). The maps are not changed.
  Checked in the game by driving a locomotive (army 0, enemy trains off) across each place
  (`ACT_MOVE` from a test copy of the mission script, positions in `error.log`); the load logs of
  l11, l12 and l15 have no `R_MAP`, `R_DOT`, `RAIL` or "Too many links" line.
  - 65 rails are there twice (the same shape, place and height - the authors' editor; one of them
    the ramp of l11's fort gate at 958,1925). Harmless: the second one only adds a reference to the
    two points of the first (`RailMap_AddDot`) and no link (`RailDot_Connect` skips a link that
    exists); trains drive over them as over one rail (l11 fort gate, l15).
  - No rail point of any mission has more than 4 links (the engine allows 6). The joints with 7 or
    8 rail ends (l11 2626.5,2100, l15 2555.5,450, l20 1277.5,1450 and 567.5,1950; l17's came from a
    rail laid twice) are crossings with branches: the ends pointing the same way share one point,
    which has at most 4 links. A train sent across the l11 and l15 ones went straight through.
  - The joints the old check called "do not link" in l11 (922.5,1950 - the fort gate -,
    2555.5,2075, 2839.5,775) and l15 (780.5,875, 1135.5,2225, 1703.5,825, 1916.5,1800,
    2129.5,1950) continue on ramps (rail frames 12-35, which the old check left out): they link.
    Driven: the l11 fort gate and the l15 joint at 1703.5,825 (down and up the ramps), the l15
    level crossing at 1916.5,1800 - the trains passed and arrived.
  - l12 922.5,850, 993.5,800, 1064.5,950 and 1135.5,900: the two parallel lines end at the
    volcanic rocks in a V each side (a diagonal and a curve meeting at one point, nothing beyond;
    under the rocks only gravel-bed decals and mines). Real dead ends, as designed: a train sent
    to the V stops there; a train sent across the rocks takes the bypass that runs between them
    (from line A's leg along y 825 and down to its far side) and arrived - over an enemy mine
    (VID 86 at 1033,878) that destroyed the first locomotive and broke the rail, which the next
    one crawled over at the broken-rail speed. Intentional.
  - l11 745,2100: rail 2128 (745,2075, z 83, direction 83) is a ramp the wrong way up - its low
    end (point at 93) meets the south line at 104, so the north line (104) does not continue south
    there; a ramp curve at the same place (rail 2159) joins the south line to the fort-gate line.
    Driven from the north line to the south (620,2212): the train reversed twice and came round
    through the fort gate (15 s instead of about 3). An authoring error worth fixing: the rail laid
    flat (direction 0, z 94) links at both ends.
  - l15 1881,350: rail 552 (1881,325, z 132, flat) lies 11 below rail 576 at the same place and
    links to nothing - a stray copy. Harmless: a train drove along the track there (1756,275 to
    1881,462) as if it were not there.
  The sandbox and the editor's tracks have none of it.
- `Rail_Action` has no case for `ACT_UNDO_REMOVE` / `ACT_UNDO_INSERT` (132/133, made for the
  authors' MapEdit): a rail taken away with them keeps its points and links in the rail network.
  LocoFix's editor releases them itself (DEVELOPERS.md "Editor mode").
- `CURSOR_DELETE` is 15 in `export.lgc` ("for deleting in MapEdit"), but in the cursor table of
  `Cursor_Show` (0x47bc80: arrow, noammo, move, clash, repair, attack, farattack, select, nomove,
  cycle, link, unlink, cantmove, patrol, delete, capture, mine, unmine, then the small ones) the
  delete cursor (`cursores\delete.ani`) is 14 and 15 is "capture" - one off; no script uses the
  name. LocoFix's editor sets 14.
- The weapon of the fort's gun (VID 115, WEAP 24) names icon frame 29 of the state bar atlas
  (`vid\455icon.vid`), and frames 9, 20, 22-27, 29, 30, 37 and 41-53 all point at the same empty
  cell of the atlas, so such units show an empty icon. The cannon of the atlas is frame 36.- The menu of the editor mode says F3, F2 and F1 for Load, Save and About, but its accelerators
  (`AppAccel`) are Alt+F3, Alt+F2 and Alt+F10 - the plain function keys belong to the factory
  build list. LocoFix shows the real keys in the menu.

Harmless log errors:

- "Reference count non zero after delete", "noRef for SPRITE with EmptyVid" (SPRITE -1, an
  object of `g_emptyVid` - a marker - still referenced when the game exits),
  "PTR_SPRITE ... not clear" and "ENGINE ... rail" while a level closes: the wholesale
  level release tears down the objects and the rail graph
  in an order that leaves those counters above zero. Nothing in play depends on them, and the
  memory goes with the level - no leak grows during a game.
- "Invalid resource type" / "Section can't found (load 'VID')" on every map and menu:
  `Level_Load` first tries every map as a demo file, and the menu maps have no `OBJ` chunk.

## Checked, not bugs

- The developer key `#` (big train, launcher option Cheats) works: it creates 16 wagons of
  army 0 with no locomotive (`maps\script\dokeys.lgc`, `KEY_CREATE_BIG_TRAIN`) at the pointer,
  and `Engine_PutOnRails` moves them to the nearest track point. A report that it did nothing
  came from a test that sent the key without moving its mouse onto the map, so the wagons
  appeared on a track outside the view; with the pointer over the map they appear there
  (checked in the sandbox and mission 13).

- `Level_Save` writes the current game time into HEAD, which looks like it should be 0: the
  shipped maps all carry the time of the authors' editor session (93368 ms in l01, 1911886 in
  l18) and `Level_Load` ignores it for a version 10 map. LocoFix writes 0 all the same, so a map
  saved twice gives the same file.

- `Depo_Action` ACT_SET_ARMY looks like `Object_SetArmy(this, act)` in the decompiled code
  (always army 1); the machine code passes var1 (Ghidra reused the stack slot of act).
- `Player_AddBarUnit` (0x4309a0, `Player` vtable slot 10) is called by nothing in this exe and the
  `ObjectList` at `Player+0x34` is only ever removed from (nothing adds to it). The ten bar slots
  (`Player+0x358`) are written by `Player_SetBarSlot` alone, so the state bar shows one unit or the
  cars of a train; no leak or crash follows from the two dead members, they are left as they are.
- The balloon code writes `+0xe8` of objects taken from the object list (`Balloon_FindCart`,
  `Balloon_AttachCart`, `Balloon_SetCommand`), which looks like a read outside a 0x70-byte
  `GameObject` - it is `Engine.balloonAboard`, the airship cart the balloon is flying to: the
  list entry is filtered by `vid->link == balloon->vid`, so it is always a train car (0xad0 bytes).
- `SetSemaphore` (231) does not use its `army` argument: the scripts pass
  `Action(unit, ACT_GET_ARMY) + 4` as the semaphore value and 0 as the army
  (`ai_default_event.lgc`), and `RailDot_CanPass` closes a link when both ends have `4 + army`
  - the caller names the army, so the redundant argument changes nothing.
- The compressed-record tail: `RangeCoder_DoneEncode` writes 4 more bytes after the last symbol
  (one taken from the final low, then the three bytes of the byte counter, big-endian).
  `ImageCodec_Decode` stops at symbol 0x100 and never reads them, so they only cost 4 bytes per
  compressed record.
- The game always opens on "WYBIERZ GRACZA" even with `return` 1 in the player key: the intro
  sets `return` 0 right before it loads the menu (`maps\logo.lgc`), so `maps\menu.lgc` takes
  the player branch; the menu and `maps\script\initdefault.lgc` set it back to 1. Reading the
  registry after the game has closed therefore shows 1 and says nothing about what the menu
  saw (the player data is in `HKEY_CURRENT_USER\SOFTWARE\Gromada\Locoland\Players` since F15).

## Running on current Windows (fixed in LocoFix / setup)

- The system DirectDraw does not set the 16-bit mode and corrupts the picture - dgVoodoo2 is
  part of the game.
- The launcher forces full screen with dgVoodoo (test at 0x40d95e skipped).
- In a window the game area was smaller than the chosen resolution and the picture and mouse
  were computed from the whole window (AdjustWindowRectEx, 6 GetWindowRect calls).
- Only 640x480-1024x768 in the launcher; maps smaller than the screen sat at the right and
  bottom edge; menus drawn for 1024x768 in a corner (logical screen, `screen_scaling.cpp`).
- Objects skipped outside a box sized for 1024x768: shadows cut off at the sides above
  1024x768, whole objects missing above 2048x1024 (`view_culling.cpp`).
- No frame limit: 180-600 fps and a whole CPU core busy (`frame_limit.cpp`).
- Low frame rate at high resolutions (reported: 50-55 fps in missions on a Ryzen 9800X3D with
  an RTX 5080): the engine draws on one thread, locks the whole back buffer and depth buffer
  every frame for its software sprites (through dgVoodoo a copy from the GPU and back) and
  fills a texture on the CPU for every light. LocoFix fixes 31 (`fast_ftol.cpp`) and 32
  (`layer10_lock.cpp`) take about a sixth off a frame at 2560x1440 (mission 13: 37-39 fps
  -> 45); the rest belongs to the renderer's design (DEVELOPERS.md "Frame rate at high
  resolutions").
- Dated logs left behind after every crash pile up (LocoFix keeps the 3 newest).
- A player who had put dgVoodoo2 into the Steam folder of the game (for its resolutions) built
  with Setup from it, and the game crashed at the start; after verifying the game files in Steam
  it worked. The build copied every file of the Steam folder's root but `Steam.exe`, `steam.cfg`
  and `objects.res`; our `DDraw.dll`, `D3DImm.dll` and `dgVoodoo.conf` replaced the player's and
  `D3D8.dll`/`D3D9.dll` were removed, so the other files (or a changed `Steam.exe`, which only
  the PE date was checked of) are the likely cause - which ones the player had is not known.
  `build.py` and Setup now stop with a request to verify the game files when `Steam.exe` is not
  the original (SHA-256) and use nothing of the Steam folder's root but the game's own files and
  folders (DEVELOPERS.md "Quick start"); checked with a copy of the Steam folder with a wrapper's
  files added (ignored, the game runs) and with a changed `Steam.exe` (stopped, Polish and German
  texts of Setup).
- A player's Setup of 0.1.1 stopped with "ValueError: ...constants.lgc.patch: line 37 differs from
  the original the patch was made for": `maps\script\constants.lgc` of the Steam folder had a
  line added after line 36 - most likely `#define KEYS_ENABLE 1`, the old way to turn on the
  developer keys. `build.py` and Setup now compare every Steam file our patches change with the
  original (SHA-256) before building and stop with a request to verify the game files (Setup's
  languages, naming the files); the `#define KEYS_ENABLE` lines alone are left out instead, when
  that gives the original exactly. Checked on a copy of the Steam folder: the define added after
  line 36 - built, the scripts the same as from the clean copy; another change of
  `constants.lgc` and a line added to `maps\menu.lgc` - stopped (`build.py`, and Setup's
  `--silent` with the Polish text, nothing installed).
- Reported by a player of the original game on Windows 10: Alt+Tab spoils the game, and the
  train icons of the factory panel do not show. Neither happens in OpenLocoland (checked with
  `test_game.ps1`'s Alt+Tab step, `A`): in a window (1024x768, 1920x1080) and in full screen at
  the desktop's resolution (2560x1440) and below it (1024x768 and 1920x1080, where the window is
  minimised while away and the engine restores its lost surfaces - "Restore surface" in the
  log - when it is back), the game pauses while away and comes back drawing the map, the bar
  and the factory panel; the panel shows both rows of icons (all 14 units of the sandbox
  factory) at 800x600, 1024x768, 1920x1080 and in full screen, before and after Alt+Tab. The
  original was not run
  (full screen without dgVoodoo2 switches the display mode); the likely cause there is the
  system DirectDraw, which OpenLocoland replaces with dgVoodoo2 (first entry of this list).
