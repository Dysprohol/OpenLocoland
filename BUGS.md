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
| F25 | A region's fog table is freed twice. `Region_InitFog` (low, high, colour) stores the fog's values, frees the fog table (`+0x78`) when there is one and makes a new one only when low < high, so a fog taken away (low >= high) leaves the pointer to the freed table in place; the next `Region_InitFog` - a fog chosen again - frees it a second time, and so does the region's deletion. The heap is corrupted and the game dies a little later somewhere else. The engine calls the function once a region, when a map is loaded (`Region_Action`), so the campaign never meets it; the editor's region panel and its undo call it again. Seen on a player's map (2026-11-10): FOG: MIST, an undo of it (no fog), FOG: MIST again - `crash: exception C0000005 at 0041FB38` (`World_DrawLayer` reading a layer list that was overwritten), then the engine's filter at 0x419864. Reproduced on the editor's start map: a region, FOG: MIST, FOG: NONE, FOG: DUST - the same address at the third click | `Region_InitFog` 0x435c50 (`re/c/00434000.c`: `crt_delete` of `+0x78` without clearing it) | LocoFix `region_fog.cpp`: a hook on `Region_InitFog` clears the pointer after the original ran with no fog to make. Checked in the game: see docs/dev/changes.md 41 |
| F24 | A car of a train laid onto itself or onto the car in front of it ("the wagons collapse under the train", GitHub issue #13's screenshots at a factory's exit with a buffer stop; reported for the original game too). The cars keep no path: each frame `Train_MoveCars` puts every following car's front end on the rear end of the car in front and `Engine_UpdateRear` lays the car's rear end 48 px behind from the front's point (`front.dot`), the link back from it and the old rear point (DEVELOPERS.md "How the cars follow"). Two cases lay the rear end on the car's own side. (1) A car with a car in front whose `front.dot` is a dead end (a point of one link: a buffer stop, a factory spur's end) - a car coupled behind a car that hangs past a buffer stop: branch two (`rear.dot` is `front.dot`) lays the rear 48 - front distance forward along the one link, branch three with link length + front distance up to 48 lays it 48 - length - distance from the far point toward the front, so the car comes out turned round with its rear end under the car in front. (2) `rear.link` leading from `rear.dot` back to `front.dot` while the rear end belongs beyond `rear.dot` (branches three, four, five): "beyond" goes back onto the car. At a dead end its one link is that way; `Engine_PutOnRails` (each end on the link nearest to it, counted from the nearer point) leaves `rear.link` so for cars made on open track too. The engine logs only the jump when such a car moves again (`zm-error`, the authors' "the cars collapse", in `error.log`). Reproduced on a test copy of POLIGON: on a straight spur with a buffer stop, a wagon made by a script `Create` 12 px past the stop and coupled behind a train lay 12 px from the car in front, turned round, under it (`train fold check: FOLDED car 50 at 1140,1525 (span 24.0, rear toward the car in front, back 1) after Engine_AttachToTrain`) and jumped with `zm-error` when the train was sent off; with the factory moved onto its spur's dead end - where ten of the campaign's factories stand (l06, l08, l09 two, l10, l12, l15, l16, l18, l20; the others have the dead end one link of 35-51 px behind the door) - every car it released came out 34.5 px long and drawn 12 px forward until it had moved half a car; two enemy carts made 48 px apart on the main line came out 12 px apart, the second on the first (`ENGINE 25: 0 rail 1`), and a locomotive made next to a junction 32 px long. The 20 missions, 10 s each without the fix, logged none of this except single cars and a train the missions save against a buffer stop (l06, l12, l15: both ends laid on the stop's link, drawn where they were placed) - left as they are. Not reproduced: a factory's queue backing up to its door in play - a pushed train moves until its last car has left the push line and pushes what stands in its way (a train of another army, a braking locomotive), and `Depo_ReleaseUnit` deletes a car still standing on the factory's place; a script `Create` on an occupied point puts the car on the nearest free place, so `StartCreateTrain` lays its trains car by car; the reporter's map (2.5 min of play) showed no fold | `Engine_UpdateRear` 0x40a310 (branch two at 0x40a3b2, branch three's last line at 0x40a4ed, `re/c/00408000.c`), called by `Train_MoveCars` 0x40adf0, `Engine_AttachToTrain` 0x409270, `Engine_PutOnRails` 0x4095b0 | LocoFix `train_fold.cpp`: a hook on `Engine_UpdateRear` runs the original and corrects only a result that is folded (the rear end nearer the car in front than the front end, or the ends under 40 px apart with the front end farther from the dead end or turning point than the rear end), in the two cases only - (1) the rear end past the dead end on the line of its link (the formula the engine's long-link branch uses, which branch three already gives when the link is long enough), (2) the same distance beyond `rear.dot` on the link that goes on from it the straightest (`RailDot_LinkTowards`), at a dead end past it on the line of its link; every other result is left as the original made it, and LocoFix.log gets one `train fold:` line per car it lays so. Checked on the same test map (screenshots and logs in the hand-over note): the coupled wagon stands whole behind the car in front, hanging over the buffer stop as it was made, every car the factory released and the carts made on the main line stand whole, 48 px from the next, no `zm-error` (a car made by script right on a buffer stop, alone, is left collapsed as the original lays it); the trains drove off and came back to the buffer stop (the turn at a track end unchanged); the player's locomotive coupled the enemy carts (the kept trick); the factory queues came out as before. The 20 missions (`test_missions.ps1` at 1024x768): all pass with and without the fix, with the same error lines in each, no `zm-error` and no `train fold:` line |
| F23 | The fog mask did not fit a view over 1024 px. `Graphics_Init` makes one 256x256 scratch texture (`grayTexture`, `Texture_Construct(..., 0x100, 0x100, ...)`, made again in `Graphics_ReloadPalettes`) and `Graphics_DrawFog` writes a region's fog into it at quarter resolution from the depth buffer, so the texture covers 1024x1024 px of the view - enough for the original's largest mode, 1024x768. A fog region whose visible part is wider or taller than that (a region over the whole view, region flag 8: missions l07, l13, l18; the editor's region panel with OVER: THE WHOLE MAP) wrote past the texture: mission l07 in a 1920x1080 window died within a minute - `LocoFix.log` "crash: exception C0000005 at 0041083E (Locoland.exe+1083E), writing ...", `error.log` "EXCEPTION!" and then the engine's own filter faulting at 0x419864; the same with l07 held in the editor at 1920x1080, l13 at 2560x1440, and a fog region over the whole sandbox map in the editor (at 0x41081A). `Graphics_DrawGroundSnow` writes the whole view into the same texture the same way (no shipped mission turns ground snow on). Both step through the texture with the pitch of its lock, and `Texture_Draw` / `Texture_DrawZ` take the texels against the texture's own size; the other users (`Graphics_DrawLight`, `EFF_ALPHAAPPEAR` through the rgb texture) do not depend on the view | `Graphics_Init` 0x40dc10, `Graphics_ReloadPalettes` 0x40eea0, `Graphics_DrawFog` 0x4103c0 (the writes at 0x41081A and 0x41083E), `Graphics_DrawGroundSnow` 0x410920 | LocoFix `fog_buffer.cpp` (DEVELOPERS.md "Rendering", "The fog buffer"; GitHub issue #8): fog and ground snow are drawn through a texture of their own, put in place of the grey one for those two calls - the smallest power of two holding a quarter of the chosen resolution on each side (512x512 at 1920x1080, 1024x512 at 2560x1440), made like the grey one at the first `Graphics_ReloadPalettes` and again after a surface restore; the grey texture stays 256x256 for the lights, and up to 1024x1024 px nothing changes. Without that texture fog and ground snow are left out on a screen the grey one does not cover. Checked in windows: at 1920x1080 l07 in play (lost by the AI at 0:59, then its end screen; 3.5 min loaded) and 196 s held in the editor, l13, l18 (night, lamps), l01, l05, l13 with ground snow turned on (`GRPH` 0x41), a whole-view fog region over the sandbox map in the editor; at 2560x1440 l13, l07, the same snow and fog maps - no crash line, no "EXCEPTION!", the fog and the snow over the whole view; at 1024x768 no texture is made, and l07 and the fog-covered sandbox held in the editor at the same view differ from a build without the fix only as much as two runs of that build differ (animation) |
| F22 | The game crashes when its DirectDraw surfaces are lost and cannot be restored at once - in full screen whenever another window is in front (Alt+Tab, a window or notification over it): `Graphics_BeginScene` calls `RestoreAllSurfaces`, which fails (`DDERR_NOEXCLUSIVEMODE`) while the game is not in front, and draws the frame anyway; the software sprites' lazy locks fail ("Couldn't lock", `DDERR_SURFACELOST`), leave `backBufferBits` / `depthBits` 0 and `SpriteVid_Draw` writes through the null pointer - "EXCEPTION!" at 0x4516B9 or 0x451D74 in `error.log`, then the engine's exception filter faults too. Seen at 1920x1080 full screen in play and in the editor (in test mode, which keeps the game running behind other windows, at the first Alt+Tab every time) | `Graphics_BeginScene` 0x414cd0 (the restore, its result not used), `Graphics_LockBackBuffer` 0x412560, `Graphics_LockDepthBuffer` 0x419720 and their inlined copies in `SpriteVid_Draw` 0x450f10 | LocoFix `lost_surfaces.cpp` (DEVELOPERS.md "Rendering", "Lost surfaces"): before each frame the lost surfaces are restored, and while they cannot be the frame is skipped (`GameWorld_Run`'s own path for a non-zero `Graphics_BeginScene`, game time held); a lock that still fails gets a stand-in buffer. Checked on POLIGON at 1920x1080 full screen (`test_game.ps1` `A<s>` steps, 3-10 s away): without the fix 6 of 6 runs crashed (play 4, editor 2) at the start or the first Alt+Tab; with it 0 of 5 (play 3, editor 2, 5-10 cycles each, 45 cycles) and 0 of 2 in a window (8 cycles), the picture whole after each return, `error.log` with only the lines a run without Alt+Tab has |
| F21 | Mission 11 (l11) at 745,2100: rail 10467264 (745,2075, z 83, direction 83 - frame 12, a ramp whose end 0 is 11 higher) is a ramp the wrong way up. Its high end meets the line at x 745 coming from the north (rail points at 104), its low end (93) the same line going on south (104), so the two do not link (`map_tool.py check-track`: "rail ends (0, 1) at height 93 meet the opposite ends at height 104"; the editor's `D` shows the point 745,2087 at 93 with one link). A ramp curve at the same place (rail 10460128) joins the line going south to the lower fort-gate line, and the line from the north ended in the stub. A locomotive sent from the north line (745,1967) to 620,2212 went north, reversed and came round through the fort gate (arrived after about 19 s). The rail is also drawn lower than the line, which shows as a gap | `maps\l11\l11.map` | `game/maps.override/l11/objects.tsv`: the rail laid flat (z 94, direction 0 - the same shape, points at 104 at both ends). check-track then lists only the doubled rails; the same trip goes straight down the x 745 line (about 6 s), the fort-gate line still joins it through the ramp curve (driven from 883,1968 to 620,2212), and the line is drawn without the gap. What changes in play: trains - the player's and the AI's - can now go between the north line and the south-west corner at x 745 instead of round through the fort gate. Nothing else of the map changes (2 values of one object) |
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
| C4 | `GETSPRITE_ARMY(army)` of the script library returns `GETSPRITE_ARMY0*(army+1)`: 0x10000 and 0x20000 for armies 0 and 1, but 0x30000 (armies 0 and 1 together) for army 2 and 0x40000 (army 2) for army 3 - the engine tests the bit `0x10000 << army` (`FindNearestSprite`, the masks `GETSPRITE_ARMY0`-`3` of `export.lgc`). So every search "of army 2" or "of army 3" made with it finds the wrong armies. The fort's capture handler `F162_0` (also the mine's, `F165_*`) sets the building's new army and then hands the dead gun sockets (VIDs 112, 116) of the OLD army to the new one with it: a fort taken from the player or the enemy changes hands whole, a fort taken back from the pirates (3) or from the neutral army (2) keeps its sockets, which rebuild their guns in their own army - the fort is the player's and builds pirate guns (GitHub issue #10, a player's report; read in the scripts, not run yet). The other uses: `Captures.lgc` line 31, `ai_default_event_inherit.lgc` lines 9 and 25 (the nearest repair point of an army), `ai_functions.lgc` lines 228-239 (the nearest fort, mine and guns of an army) | `maps\script\export.lgc` line 390; `maps\script\ai_default_event.lgc` lines 606-638; `FindNearestSprite` 0x420840 | Open: a one-line patch of the helper (`GETSPRITE_ARMY0<<army`) would correct every use; it changes how neutral and pirate forts behave in the shipped missions, so it waits for a decision and a run of those missions |
| C5 | The rain is made for a view 768 px high. `Graphics_DrawRain` keeps 250 drops (`g_rainDropCount = 0xfa`). A drop of length `len` (15-65) starts at depth `(len + 10) * 0.00122 + 0.0156` and loses 0.000122 per pixel fallen until 0.0156, so it lives for `(len + 10) * 10` = 250-750 px of fall; a drop made again starts at y 0-40 (only from the 50th one made again in a frame anywhere down to `viewBottom`, which steady rain does not reach). Nothing falls below about 790 px: at 768 px the rain only thins toward the bottom, on a higher view the lower part stays dry (a player's screenshot at 1920x1016, GitHub issue #13), and the 250 drops are spread over the wider view. Falling snow (`Graphics_DrawSnow`) wraps at `viewBottom` and is whole | `Graphics_DrawRain` 0x410e20 (the start band at 0x4110d9 on) | Open: cosmetic, to be fixed in LocoFix (the start band, then the number of drops) |

## Suspected

| # | What | Where | Status |
|---|---|---|---|

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
- `NoArmyUnits(army)` (`maps\script\basefunctions.lgc`) returns the number of that army's units
  of its list, not "there are none": for armies 1-3 the list (`ARMY1_NVID_ARRAY`) holds the factory
  and the fort's guns too, so it stays above 0 while such an army has a building.
- `GetGroundZ(x, y)` (158, 0x420ec0) reads the height grid without testing the point against the
  map: a point outside the map reads past the grid. The shipped scripts ask only for points on the
  map.
- `ReinforcementInit()` takes the first plain engine (VID 5) of army 3 as the place where
  reinforcements arrive and removes it (F12) - on any map, so a pirate VID 5 placed in the editor
  is gone when the map starts (GitHub issue #13); LocoFix's editor names it as that marker when it
  is selected.

Engine:

- `SCROLL_WITH_MOUSE` is declared in export.lgc but was never implemented (LocoFix adds
  middle-button panning instead).
- The health and ammunition bars over a unit are drawn for armies 0 and 1 only: `Unit_Draw`
  skips them when the unit's army is above 1, so neutral and pirate units never show them, also
  with the unit information switched on (GitHub issue #10).
- There is no line of sight: `Object_CanSee` (0x44b4a0), which `Object_FindTarget` asks, always
  returns 1, so a unit sees every target within its range whatever stands between them.
- A region's flag 1 is not a pulse: with it `Region_Draw` (0x435770) does not set the fog's top to
  its level every frame but raises it by 2 (eighths of a GRID level) each time `g_gameTime & 7` is
  smaller than the frame before, from 0 up to the level, and sets a top above the level to 0 - so
  the fog comes up once after the map is loaded and stays, and how fast goes by the frames' lengths
  (at 60 frames a second 14.4 units a second at game speed 1 - level 80 after 44 s -, at speed 2
  the same rise in 26 s; measured). Nothing takes it down again, and no map of the game has the
  flag. Kept as it is; the editor names it RISES AT THE START (it said PULSING), and a real pulse
  is LocoFix's own on the unused bits 0x10, 0x20, 0x40 (DEVELOPERS.md "A fog that rises or
  pulses", "Changes from the original" 40). Flag 2 makes the fog darken the picture instead of
  adding its colour (`Graphics_DrawFog`: source blend ZERO for ONE; l01 and l07).
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
- The log goes to `logs\error<date> <time>.log`, but the Steam game has no `logs`
  directory, so the open fails and the game falls back to the same name next to the exe
  (the deliberate second attempt in `Log_Construct`; our build makes the folder, DEVELOPERS.md
  "Logs"); on a normal exit the run's log is
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
  the game logs with the editor's test command `D` on l11, l12 and l15). The maps are not changed,
  except l11 745,2100 (F21).
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
  - l11 745,2100: a rail laid as a ramp the wrong way up, so the line at x 745 ended there -
    fixed by a map override, F21.
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

Player tricks (reported by testers; part of the game, kept as they are - checked in the sandbox,
the last three with a test copy of its script):

- Building at once: `Depo_ReplaceInQueue` (0x404fd0) gives the replaced entry the new unit's
  full build time in `timeLeft` but starts the building again only when `building != 1` - the
  number of the first slot, not of the replaced entry. Replacing the first entry of a factory's
  queue while it is being built leaves the factory's timer running down for the old unit, and the
  new one comes out when the old one would have. In play: queue a quick unit, click its slot (the
  queue edit panel: remove, pause, back, forward, replace), choose replace (the circling arrows)
  and click another unit of the list - the locomotive (VID 5, 30 s) replaced after 22 s by the
  nuclear rocket (VID 62, 120 s) gave the rocket about 30 s after the locomotive was queued.
  Replacing at the last moment gives any unit at once. The money is right: the old price comes
  back and the new one is paid (300 -> 180), and the list offers only the units the money covers
  (their icons are grey, a click on them does nothing). Pausing, moving and removing entries save
  the time of the entry being built before they start building again; a replace while a later
  entry is being built (the first one paused) starts again without saving it, so that entry
  loses its progress.
- Taking over enemy trains: `Engine_CanCoupleWith` (0x40ab30) couples two trains whenever one
  has the link command aimed at a car of the other, whatever their armies, and
  `Player_HandleMouse` offers the link order (action 0x96) on a car of another train when that
  train has a car of the player's army, or has no locomotive (`Train_IsUnpowered`) and the
  player's locomotive drives alone with power to spare (or Ctrl is held). So the wagons of an
  enemy train whose locomotive was destroyed can be coupled to the player's locomotive and driven
  away: `Train_HandleContact` cancels the attacks on them and calls the script event
  `EnemyLinked`, whose handler in `ai_events.lgc` (a comment: the player linked the train, "for
  repainting") drops the train from the AI's plans. A repair point (VID 104) then makes them the
  player's: `Building_UpdateWorkshop` gives a car of another army, in a train whose locomotive is
  of another army, to the repair point's army. Checked: two army-1 mortar carts (VID 25) without
  a locomotive, an army-0 locomotive ordered to link - they coupled, were driven round the loop
  and changed to army 0 one after the other as they passed the player's repair point. Selecting
  an enemy train directly (the key E, "CanSelectEnemy" in `Player_HandleHotkey`) needs the
  engine's DebugMode and is not part of normal play.
- One unit destroys a factory: the factory (VID 102) has 150 hit points, and the explosion of a
  unit with damage when it dies (`Object_Action` 15) gives that damage in full to every object in
  a box of its damage radius around it (with VID flag 0x4000000 it falls off with the distance).
  The brander (VID 97, a locomotive that blows up, 40 hit points) deals 500 within 140 px, so
  one brander that dies next to an enemy factory - shot by its defenders, or ramming a train
  there (`Train_Collide`) - destroys it. Checked: an army-0 brander on the track in front of
  the enemy factory, destroyed: the factory went from 150 hit points to gone within a second.
  The explosive mine (VID 86) deals 1000 within 100 px, falling off with the distance.
- Turning a train round at a track end: every train reverses as one piece with no loop -
  `Train_Reverse` (0x409930) makes the last car the leading one and the first car takes over
  the speed and the route; the cars keep facing as they were, so the locomotive pushes. A
  stopped train reverses anywhere; a moving train plans a reversal only where the track goes on
  for the train's length (`RailDot_HasRoomToReverse`, called from `RailDot_Search` only while
  the train moves), so any spur or dead end serves to change direction. Checked: a locomotive
  with two carts sent up the spur to the player's factory stopped there and, sent back to the
  main line, backed out with the carts leading and the locomotive pushing behind them.

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
- GitHub issue #4 (2026-11-05): Setup 0.2.2 refused a verified, freshly installed Russian Steam
  copy - `maps\logo.lgc`, `menu.lgc`, `script\constants.lgc` and `script\menutact.lgc` "modified".
  They are the Russian depot's own (352136; the other five depots have the files of the known
  SHA-256): `logo.lgc` plays the movies as `.avi` (`video\` has `.avi` files), `menu.lgc` and
  `menutact.lgc` lay out the score line of the player's header differently (`%-13s` / `%32s`
  instead of `%-20s` / `%25s`) and `menu.lgc` takes a player's name of up to 12 letters up to
  `z` and a backspace on an empty name, `menutact.lgc` has an empty first line, `constants.lgc`
  two more entries (258, 260) in a table of VIDs and their values - its `logo.lgc` and
  `constants.lgc` are byte for byte those of the English beta/demo disc of 2003, so the Russian
  depot has part of the scripts of that older build. Since 0.2.7 a Steam file that
  is not the known one is accepted when our patches can be placed in it (by their lines, not only
  their line numbers: `patch_text.apply_patch`), `logo.lgc.patch` no longer touches the movies'
  lines, and a file our changes do not fit stops the build naming the file and the change - or,
  with "Skip verification of the game files", stays as the player's with a warning of what is missing
  (DEVELOPERS.md "Quick start"). Checked: built from the Russian depot (all patches placed, the
  nine changes of `menutact.lgc` a line lower), the intro to the menu and POLIGON with its Esc
  menu briefing in the game, no LOGIC or MOVIE errors in `error.log`; on a copy of the English
  depot with a line our `menu.lgc.patch` needs changed and three lines added to the top of
  `menutact.lgc` - refused naming `menu.lgc`'s first change; with `--skip-verify` (`build.py`, and
  Setup's `--silent` in Polish) built with `menu.lgc` as it was and the warning. Then every
  edition at hand with `tools\editions.py check --run` (DEVELOPERS.md "Release"): the six depots,
  the Czech and the Polish CD, the Italian CD and the English beta/demo - all built, every patch
  placed, the main menu reached and POLIGON played with no LOGIC error; the beta/demo's and the
  Italian CD's exes are other builds, so "Skip verification of the game files" copies them
  unchanged and the game runs as the original, without LocoFix (the Italian exe, wrapped by a copy
  protection, ends at once without its disc; LocoFix now also checks the exe's image size, which
  alone tells that exe from the Steam one). That first build of such a copy still put our script
  patches in, so its menu called LocoFix's externs: the beta's `error.log` filled with "LOGIC:
  Unknown extern Function 20N" and "couldn't open maps\custom.lst". Now such a copy is installed
  as it is (`build.build_as_is`, DEVELOPERS.md "Quick start"): its own files unchanged, with
  dgVoodoo2, `Locoland.cfg` and `logs\`, and nothing of ours that needs LocoFix. Checked with
  `editions.py check --run en-beta-demo it-cd` (2026-11-06): both "as is", the beta through its
  own intro movies to the menu with no LOGIC error and none of those lines - its 36 `!!!ERROR`
  lines are the original's (25 sounds its `objects.res` names and its `wav\` lacks, the fort
  cannon's frames of `vid\110.vid`, the old-format `logo.map` and `menu.map`, a movie it lacks).
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
