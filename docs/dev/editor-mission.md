# Editor mode: mission, arrivals, orders, factories, AI

Part of the developer documentation; the index of all parts is [DEVELOPERS.md](../../DEVELOPERS.md).

**Mission settings and push lines** (`src/LocoFix/editor_mission.cpp`, the Push line tool of
`editor_edit.cpp`). A mission is its map and its script, and the editor keeps what it manages
in two marked blocks of the script (`<map>.lgc`), which it writes anew whenever the map is
saved - the rest of the file, the mission's own code, stays as it is:

- `// <editor>` ... `// </editor>` before `#include "maps\script\header.lgc"`: the settings the
  scripts of `maps\script\` read (MAP_RULES.md 8) - `CUSTOM_MAP`, `MONEY`, the time of day
  (`START_DAYNIGHT`), `RAIN`, lightning (`NUKE`), the text files (`OBJECTIVES_FILE`, `WIN_FILE`,
  `LOOSE_FILE`, `BRIEF_LIST`, `BRIEF_HEADER`, `HISCORES_FILE`), the player's reinforcement
  (`PLAYER_REINFORCEMENT`, `player_reinforcement_train[]`), the enemy (`LIMIT_ATACKERS`, the
  patrol points `ATA_PATROLn_X1` ... `_Y2`, `start_trains*[]`, `level_queue_*[]`) and the other
  settings of the template (music, weather, the gammas, the AI timings) as they were read; a
  `// title:` line keeps the map's title for the custom maps page - for a custom map, like
  `BRIEF_HEADER`, its name in capitals, written so on every save (`formats.md` "Map files"),
- `// <editor main>` ... `// </editor main>` in `main()`'s `iff(1)` block: the `SetPushLine`
  calls.

File > Mission settings... (or the gear of the bar's strip) shows them in a Windows dialog -
the title on the custom maps page and the briefing's header (read-only for a custom map and the new
map without a name: they are the map's name, which Rename changes; editable for POLIGON, the
template and the campaign), money at the start (`SetMoney(0, MONEY)`
of `initdefault.lgc`), the time of day at the start (`START_DAYNIGHT` 1-4: day, evening, night,
morning; `daynight.lgc` then goes on, 5 minutes each), the weather (`RAIN_TYPE`: rain, snow, clouds,
storm clouds, meteorites - `environmenttact.lgc` makes the last three) and how often (`RAIN`: 0
never, 1 now and then - it comes and goes, about half a minute each -, 2 all the time;
`daynight.lgc`'s own comment has 1 and 2 the other way round, its code does this), lightning
(`NUKE`: a flash and its sound at random in a clear sky, with rain only while it rains, never with
the other weathers), the fliers over the map (since v0.2.12, FLIERS OVER THE MAP: none or the kind - 268
the dragon-fly of missions 2, 3, 4, 8, 9, 416 the bird of 12-14 and 20, 422 the winter hornet of 16-19, by
the game's names - and how many, 0-50: `MUXI` and `MUXI_NVID` as the missions have them; `environmenttact.lgc`
calls `Muxi()` of `civil.lgc`, which brings one more in from beyond a random map edge in army 2 while fewer
are there; none writes `MUXI 0` and keeps the kind; test file keys `flies:` (the VID, 0 none) and
`flies_count:`; 268 and 422 are in the palette's units beside 416 now; checked on a POLIGON copy: 422 x 7
written, 5 hornets at 5 s, 7 from 10 s on), the level's music - none, one file or a list of
up to 30 played in turn (MUSIC, PLAYED IN TURN: the list, a drop-down of the files of the game's
`music\` folder but the menu's `musicmenu.ogg` - `mission_music_files` -, Add (after the file chosen in
the list, or at its end), Remove, Up, Down; `mus00` is the victory's, `menuend.lgc`). The standard scripts
play `music0` when the level starts (`initdefault.lgc`) and, each time one ends (`IsPlayMusic` 0,
`gametact.lgc`), `music1` and `music0` by turns - the missions pair two files, l05 `mus07` / `mus08`. The
editor's block keeps them as they are and makes both the next file of the list: `#define MUSIC_FILES n`,
`MUSIC_FILE0` ... (`"music\\mus05.ogg"`), `#define music0 MissionMusic()` and the same `music1`, and a
function `MissionMusic()` in the block that steps `music_turn` round the list and returns its file, or
`""` with none (`music_of` reads it back, `settings_block` writes it). With none `PlayMusicFile("", 0)`
fades out what plays and loads nothing - the engine's sound update (0x444950) loads a file only for a
name that is not empty -, so the level is silent; one file starts again each time it ends (with loop 0
`Sound_LoadWave` keeps no name, so the engine does not restart it by itself; the same name again reuses
the open stream, without a new "open" line in the log). A script without `MUSIC_FILES` - every map made
before 2026-11-05, the campaign's, the template's - is read as the list of its `music0` and `music1`
(the template's mus08 / mus07 when it has neither), and its next save writes the list. The test file's
key is `music` (the files separated by commas; empty: none). Checked in the game (test mode's
`LocoFix.log`, which names every file opened, and the music's state logged when a run ends - "the run ends
- its music ..."): POLIGON's dialog showing mus08, mus07; three files (two 3 s test files and mus00)
saved and run: opened in turn twice round in 20 s; none: nothing opened, the editor's own music faded out
and the run ended with "none, not playing"; one 3 s file: "playing" again at 15 s; the list's buttons
driven in the open dialog (mus00 up, mus05 added after it, the first removed, Down greyed on the last),
OK, saved - `MUSIC_FILE0` mus00, mus05, the test file -, the dialog read back the same), the reinforcement
waves and their train (VIDs), the enemy trains attacking at once,
the enemy's trains at the start (a line each: its type - `light_gun`, `medium_mortair`, ... the
`ATT_` names of `TrainsTactDeclares.lgc` - and the point it starts at, or two rail points it
patrols between, screen coordinates), the trains its factory builds later (the time, the type and
optionally the patrol), the objectives, the win and the loss texts and the briefing's pages (a
line `---` starts the next page; the dialog shows the texts the game shows - the pack's copy of a
file when it has one). A save writes the texts into the map's folder (or the pack's copy of a file, see the sandbox below) in capitals, in the pack's code page,
at most 44 characters a line (broken between words), CRLF: `objectives.txt`, `win.txt`,
`loose.txt`, the pages `brief.txt`, `brief2.txt` ... and `brief.lst`. The dialog had rain as a
check box that wrote `RAIN 1` - a campaign map's `RAIN 2` became 1 when it was saved under a new
name - and dropped `RAIN_TYPE`, so a map with snow got rain; its labels were cut short in some
packs. Now each setting has a row with its label in full words, the map's settings on the right of the trains'
rows (which stay as they were). Checked in play: money 12345, night, snow all the time and a new
objectives text set, saved and played - $12345 in the state bar, the night's gamma, the snow
falling all the time, the objectives line at the start. The dialog as it is now, filled in on
POLIGON saved under three names and each played (Polish, English, Polish): money 7777, night, rain
all the time, lightning, a title and header, objectives, win, loss and a two-page briefing with
Polish letters - $7777, the night, the rain, flashes about every 20 s, "CEL: ZNISZCZ WROGA ŻÓŁTY",
the header (in ASCII capitals, as written) over the briefing's first page in the Esc menu, the
win's text on the victory screen; money 4321, morning, snow now and then, no lightning - the
morning's light, the snow, no flash in a minute, the second briefing page (the right arrow), the
loss's text on the defeat screen; money 100, evening, meteorites all the time, lightning ticked -
the meteorites, no flash (lightning comes only in a clear sky or the rain). A map saved as
`maps\<name>\<name>.map` shows on the WLASNE MAPY page with the title of its script ("Map files";
before, the editor added a line to `maps\custom.lst` and `build.py` merged the repository's list with
the lines the editor had added - a folder renamed or deleted by hand left its line behind). A new map
(no script of its name yet) gets the script of the map it was saved from when that one has the
blocks, otherwise the template's (`maps\template\template.lgc`, made by `map_tool.py new
--script`: a whole mission like the sandbox - `CUSTOM_MAP`, the texts, the reinforcement, the
win when the enemy had units and has none left, the loss when the player had units and has
neither units nor a factory, and `MAP_WIN_TEST`, which destroys the enemy at 20 s to check the
end). A map saved from the template does not keep its name: a title or a briefing header "TEMPLATE"
(the template's own - the Esc menu's first page showed it in a map played from the editor) becomes
the map's folder name in capitals (`mission_save`; checked: the template saved as `maps\tpltest\` has
"TPLTEST" in its script and on the custom maps page). A script without the blocks (the campaign) is not changed: its settings and
its push lines are read and shown, read-only, and saved under a new name the map becomes an
editor mission with them.

The sandbox (POLIGON) is an editor mission too: its script has both blocks, written as File > Save
writes them (the order of the settings, `int start_trains[]={ATT_LIGHT_GUN,ATT_LIGHT_MORTAIR,-1};`,
the bare `SetPushLine` calls), so a save gives the same script back (checked by repeating the
parse and the write of `editor_mission.cpp` on the file). What the editor does not manage stays
outside the blocks: the mission's own car lists of the train types (`int this_train0[]` ... - they
come before the block, as `TrainsTactDeclares.lgc` takes them only when they are declared before
it), the comments of the push lines (above the `// <editor main>` marker), the tests
(`SANDBOX_DRIVE_TEST` takes the start trains out of the lists in `iff(1)`, before `TrainsTact()`
first reads them, instead of an `#ifdef` inside the block, which a save would lose) and its own
win and loss. Its texts had lines of 45-47 characters, which a save would have broken; they are
broken at the sentence. A save keeps the briefing's high scores page (`hiscores.txt`, last in
`brief.lst`, as the sandbox and the campaign have it) when the list had it and the folder has the
page, and writes a text the language pack has a copy of (the sandbox's,
`lang\<code>\maps\sandbox\`) into that copy - the game reads it instead of the root's, so an edit
written to the root never showed.

**A building's line** (2026-11-17, the maintainer's request and decision; `editor_edit.cpp`
`line_buildings`, `LineDrag`, `set_line_building`). A factory's exit line is two points, and the engine
pushes the new cars along the shortest way between them (`SetPushLine`); the editor's own line for a
factory (`building_line`) walks 140 px from the dead end and takes, at a switch, the branch nearest to
the spur's way. Since v0.2.12 nothing could change that branch: a line drawn by hand to the other branch
stayed the map maker's - not the factory's for its lights, its capture or its plan (the maintainer's
map "Vulcanic Uprising": before v0.2.12 the order of laying the track chose the branch). Now:

- *An end of a line is dragged.* In the Select or the Push line tool the button pressed within 10 px of
  a line's bar or arrow selects the line (as a click did) and drags that end: it follows the rail point
  under the cursor - the line it would be drawn in cyan, the status line saying what it would do - and
  let go there the line runs to it, one step of the history (`OpLineEnd` / `OpLineBegin`), keeping its
  flag and its BUILDING'S. The only refusal is the engine's: no way between the two points ("Can't
  PushLineFindDot") - the line then stays as it was; let go where it began it is only selected; any other
  end of the drag (another tool, a panel, the window left) leaves it. The line's lights go with it: the
  posts that stood as its lights (`post_owners`: its building's when it was the building's rule line, or
  the line's own) move to the places its new way gives (`line_posts`: red at the end, green at a begin
  that is joined on), a missing one is made and one too many deleted; a line without lights gets none,
  and a post's own line (a light's L line, a pair's - not a building's) leaves its post as it is. So a factory's exit is
  dragged from the branch the editor chose onto the other one at the switch, and its red light with it.
- *A line through a building is its line.* A factory's or repair point's own line is its rule line (the
  one `building_line` gives it where it stands, as before) or else a line through it: one that leaves the
  factory from the dead end it stands at (that rail point is the line's begin - a line ending there
  pushes trains into the factory, no exit) or that runs over the repair point's rail point
  (`building_dot`: its leg's point within 40 px) anywhere on its way - drawn by hand or with an end
  moved; on a line through several buildings the first met from its begin. A building has one line: its
  rule line first, else the first line through it in the editor's order (which a save keeps). A line of
  two buildings' rule (one standing on the other's place) is neither's, as before. A line through a
  building is the building's even when a post stands at its end as if it owned it (`owns`): a factory's
  exit with its red light at the end has that shape - POLIGON's own line, and every exit whose end was
  dragged with its light - so the building comes first, as for a rule line, and that post has no line of
  its own. The 2026-11-14 rule holds: a building's move, turn or deletion takes only its rule
  line and the lights of it - a line drawn or changed by the map maker is never erased by a building, its
  lights stay with it -, and a building moved or placed where a line runs through it keeps that line
  (`MsgBuildingLineThrough`) instead of getting a second one. Turning a repair point round (R) turns only
  its rule line; a line through it is turned with the line's own R.
- *BUILDING'S: YES / NO.* The line's panel (the right button on it) has the row when the line runs through
  a building or was set to no building's; B on a selected line switches it. NO keeps the line from every
  building: in the editor it is the map maker's (no building's move takes it), in play no factory's exit
  and no building's capture line; the building it ran through has then no line, or the next one through
  it. The status line says " - no building's (its panel)". It is kept in the script as a comment after
  the line's call, `SetPushLine(...);   // no building's line` (`NO_BUILDING_MARK`): the engine reads the
  call alone, the editor reads the comment back.

What the runner takes as a building's line is the editor's answer: the settings' block has, beside
`push_bx/by/ex/ey[]`, `push_of_x[]` and `push_of_y[]` - the map point of the building each line is the own
line of, 0 for none (`edit_line_buildings`) - and the runner's `OwnLine(b)` is the line whose point is the
building's within 4 px. It replaces `FactoryLine` and `CaptureLine`, which guessed the line from its shape
(a factory's: a begin within 75 px; a repair point's: a begin within 240 px, a chord of at most 480 px
passing within 90 px of it), so a long hand line through a repair point was never its line in play, and a
line the map maker set apart could still be taken. `PlanExit` (the plan waits while a car stands on the
factory's exit) and `CaptureTact` read `OwnLine`; the editor's own `edit_factory_exit_car` (the factory
panel's `A TRAIN ON ITS EXIT`) reads `lines_of`, the same answer. A map gets it when it is saved again in
the editor (the runner and the arrays are written together); POLIGON's shipped script keeps its older
runner (`CaptureLine` by shape) until it is saved.

**The author's own script** (2026-11-25; `g_own_script`, `own_script_save` in `editor_mission.cpp`; a
modder asked how to keep the editor from filling his `.lgc`). The rule above was hidden: the editor
writes only its four blocks (`// <editor>`, `// <editor main>`, `// <editor arrivals>`,
`// <editor tact>`, each to its `// </editor ...>` line), a script without them is left as it is (its
title line and `BRIEF_HEADER` follow a custom map's name, nothing else), but a save under a new name
builds the new map's script from the template with the blocks - so a hand-written script could not be
saved under another name without being changed. Now Mission settings has a box, **The editor writes the
map's script (its marked blocks)**, ticked by default. Unticked, the choice is kept in the script itself,
where the editor keeps the map's other settings, so it goes with the map (export, a folder sent by hand):
the next save puts the line `// <editor off> ...` first (`OWN_LINE`; the line anywhere in the script counts,
so an author can type it in), and from then on a save writes nothing into the script - no block, no title,
no text file (`objectives.txt` ...): in place the file is left as it is, a save under a new name and Run
the level's copy get it byte for byte from the map the editor stands on (`save_map` passes it as `from`;
a copy written only when its bytes differ). The settings, push lines, orders and plans of such a map are
shown, not saved (`mission_managed()` is false, so the Push line tool says why and the building lines are
not taken or given, CONVERT is refused with `ArrOwnScript`); the dialog's last line says so while the box is
unticked, the status line when the map is loaded and on every save ("saved as ... - its script as it was").
The paths in the script are the author's: a copy under a new name still names the old folder's files where
the script does. Ticking it again takes the line out at the next save, and the blocks are written as before
(the push lines of `main()`'s block were read as the editor's all the while). Rename and Import under another
name still put the new folder's name into the script's paths and title (`exchange_renamed`), as for any map.
A map without the line - every map before - behaves as before. `LocoFix.log` gives the script's size and
CRC-32 when it is loaded and when an own script is saved; test mode's settings file takes `own_script: 0|1`.

**The enemy's trains of the script** (`draw_enemy_trains`, `editor_edit.cpp`). The start trains
and the factory's queue (`start_trains*`, `level_queue_*`) are made by the AI while the level
runs (`TrainsTact.lgc`, `ai_events.lgc`), so the map has no object for them. On the held level the
editor marks them: the reinforcement point's icon in a red frame where a start train starts, a
patrol's way along the track (traced as a push line is) in red to a ringed far end, and the icon on
the enemy's factory (VID 102, army 1) for the trains it builds that attack; the mouse over a mark
describes it below the status line ("enemy train: light gun, patrol 1810,1061 - 1810,736", the
queue's with its time). The start trains are edited in File > Mission settings..., the queue in
an enemy factory's panel (its plan); the marks follow them at once.

The Push line tool (the strip, the rails under a green arrow) picks two points of the rail
network - the cursor's point is shown in cyan with its screen coordinates, which are also the
ones the dialog's patrol points take - the begin and then the end of a line, and checks it the
way `SetPushLine` (0x4346b0) builds it: both ends are the rail points with links nearest to the
given screen points (`RailMap_FindNearestDot`: x, and y against the point's y - z), the begin
needs a link ("Can't SetPushLine"), an end on the same point as the begin moves to its neighbour
nearest to the end, and there must be a way between them (`RailDot_Flood`: "Can't PushLineFindDot").
A line that would fail is refused with the engine's reason; the lines of a loaded map are checked
and logged too (`LocoFix.log`: "push line ... ok"). The lines are drawn over the track: green for
push_flag 1, yellow for 0, light blue for the script's own lines, each with a bar across the track
at its begin, a small arrow on every link pointing the way it runs and an arrowhead at its end, on a
dark outline (in their colours whatever the tool draws before them: a light's preview left the
device's specular light on and turned them white - see "Editing objects", the bar); the mouse over
a line names it in a status line, in any tool: its begin and end and
what it does (push_flag 1: the trains on it are pushed the way of the arrows and none may go
against them; 0: none may go against them, none is pushed). A click on an end of a line selects it - its bar or arrow, in the
Select tool too, where a line is an object like the others: it turns white, the status line names it
(its begin, end, what it does and whose it is - a semaphore's, a factory's or repair point's, the
level's entry or exit) while the mouse is over no other, and the right button opens its panel in the
game's style (DIRECTION: TURN ROUND (R), ACTION (T): PUSHES TRAINS / ONE WAY ONLY, DELETE (DEL)): T switches its flag
(a new line has 1: the level entry, factory exits, repair point legs; the level exit has 0), R (or E,
Ctrl+wheel) turns it round, Del deletes it - all undoable - and S puts its semaphores
the way the missions have them (MAP_RULES.md 7): a post faces the trains it is for and stands
24 px to their right, a red one at the line's end for the trains that would enter it against
its way, a green one at its begin for those it lets in (none at a dead end - a factory's); a line
that begins past the edge of the map (the level's entry or exit) has the pair at its end, moved
33 px into the map when the end is nearer the edge than 40 px; a post already standing within
30 px of such a place is moved, turned and coloured instead of a new one (the rule as the
sandbox's ten posts and the missions' stand).

Checked end to end in the game: from "EDYTOR MAP" (the template) a new mission - a main line from
past the south-west corner of the map to past the north-east one (18 rails), a junction with a
spur to the player's factory (direction 192, at the dead end), a spur to the enemy's factory, a fort
on the main line, the reinforcement's point past the edge, four push lines (the entry from past
the edge, flag 1; the exit pointing into the map, flag 0; both factory exits, flag 1) with their
semaphores put by S, money 500, an enemy train patrolling the main line, Polish texts and a
two-page briefing - saved as `maps\editortest\editortest.map`: `map_tool.py check-track` finds
no problem, the semaphores stand as the rule says, the script compiles; the map shows as
"PROBA EDYTORA" on the WLASNE MAPY page with its briefing, starts from its start button with the
objectives and the money, its trains run, `error.log` has no `R_MAP` or `LOGIC` line, and with
`MAP_WIN_TEST` the win comes with the map's own text. File > Play the map played it and came
back to the editor after the end screen. A variant with two trains of the factory's queue (one
patrolling), night and rain plays without errors too.

**Train arrivals** (`src/LocoFix/editor_arrivals.cpp` the panel, `editor_mission.cpp` the script,
`editor_edit.cpp` "arrivals on the map"). The missions bring trains from past the edge of the map
in a few ways: the player's reinforcement - `AddReinforcement()` of `reinforcement.lgc` makes
`player_reinforcement_train[]` at the place of the army-3 VID 5 locomotive that `ReinforcementInit`
takes away, at most `PLAYER_REINFORCEMENT` times, each mission calling it every 5 s when the player
has no locomotive (and, l02-l20, a neutral factory still stands); seven missions really give one
(l01-l04, l06, l08, l11; l04 sells it for 300) - and the queues of templates of l13 (the player's)
and l19 (both sides: times, nine entries with their push lines, "fewer than 3 locomotives" bringing
a train earlier, the enemy's trains given to the AI, the win and loss waiting for the queues).
`AI_REINFORCEMENT` and the text `EnemyReinforcementArrived` exist and are never used. The editor
follows l19's way. An **arrival** (`Arrival`, `editor_mission.h`) is the army, the cars, the entry (a
rail point: screen x, y and its z), the first time, the time between waves and their number (0: no
end), a condition (`ArrivalIf`: none, the player has no train / fewer than n / fewer units than n /
no factory, a neutral factory stands, the enemy has fewer than n trains, fewer than n attack, the
player pays n, the campaign's "no train and a neutral factory"; for a factory's plan also the player has more than n trains - code 11, after the others, so the codes of older maps stay, shown in the list beside fewer than n, GitHub issue #13; maps using it need v0.2.12 to be edited - the editor of v0.2.11 clamps an unknown code to 10 and saves the entry as NO TRAIN, A FACTORY OF ITS OWN) that either gates the time ("only
then", as l01-l13) or brings the wave earlier ("also earlier", as l19), the task after it (the
enemy's: `ATA_FIGHT`, `ATA_FIGHT_FORT`, `ATA_STANDn` at a point, `ATA_PATROLn` between two points -
the AI of `TrainsTact` runs them, only army 1 has one -; ours, the neutrals' and the pirates':
`ACT_MOVE` to a point on the train's first car once it is whole, a patrol - `ACT_MOVE` to its first
point, then `PatrolEngine` to the other end once a car of it is within 40 px of it (`ArrivalOnPoint`,
`MissionAt`: the train stops there with its front or its back, as `ACT_MOVE` brings it; the
arrays `arrival_to2_x/y[]` and `arrival_unit[]`, task -3) - or "stays", no order: in play such a
train stood behind the entry and held every later arrival at it, so the panel warns when another one
comes from there), the enemy's raids
(`ai_trains_random_task`) and the message (`ReinforcementArrived` / `EnemyReinforcementArrived` with
sound 100, the game's own texts).

In the script (`editor_mission.cpp`, "arrivals", has the details): `#define ARRIVALS n`, a train of
its own cars as `int this_trainNN[]={...,0}` - the authors' way of redefining a template, with a type
no template has (ones digit 4, 6, 7 or 8; the tens digit the kind the AI treats it as: 1x mortars, 2x
rockets, 3x airships, 5x flame, 6x landing, 7x the super weapons, 0x the rest; a template's cars as
they are keep its ATT_ type) -, the posts and patrols (`ATA_STANDn_X/Y`, `ATA_PATROLn_*`, numbered
after the start trains' and the queue's) before `TrainsTactDeclares.lgc`, the columns
`arrival_army[]` ... `arrival_last[]` (each with a 0 after the last arrival) after it. The runner is
the block `// <editor arrivals>` after `header.lgc` - the same text in every map (`ARRIVALS_RUNNER` of
`editor_mission.cpp` and of `map_tool.py`, kept the same), so an exported map runs in any game -,
called from the block `// <editor tact>` after main()'s `TrainsTact();`. Once a second `ArrivalTry`
looks at each arrival: its time and condition; then it waits while a car stands within 180 px of the
entry (`FindNearestSprite(GETSPRITE_CLASS+B_ENGINE, ...)`, as l13 - `ARRIVAL n wave w waits at t s: VID
v of army a stands within 180 px of its entry`, once a wave) or the AI has its 16 trains (`find_num()`).
It makes the cars (up to 30: the scripts' `temp_array`, which `arr_cpp` fills) in the army (behave | 1
for the other armies) the way `StartCreateTrain` (`trainstactfunctions.lgc`) and `AddReinforcement` make
a train: `Create` on an occupied place, where the engine lays the car on the nearest free track and
couples it (engine.md "How the cars follow") - the first car with `Create(vid, x, y+z, z)` on the entry's
rail point, each next one with `Create` on the place of the last car made (`arrival_head/car/at_x/at_y[]`).
The authors put every car on the one point, and the engine lays a new car behind the train's last car
only when it lies nearer that end, turning the train round when it lies nearer its first car - so their
way gives 20,5,20 as 5,20,20 and 5,20,25,80,85 as 80,25,5,20,85 (seen in play, on the reporter's entry
and on the map); on the last car's place every car is laid behind it and the train keeps its order. The
next car comes at once while the train stands, and while the entry's push line moves it (`ACT_GET_SPEED`)
only once the last car has left its place by 40 px, so a long train does not reach back past the end of
the track behind the entry: a lone car is not pushed (seen: the first car stood on the reporter's entry
until the second came), two are, and the cars are laid behind the entry - it needs the track of two
cars behind it (48 px each, engine.md "How the cars follow"), of the whole train where no push line moves
it. A car that did not couple (`ACT_IN_TRAIN` of the first car) is taken away and made again a second
later; after 30 s without a car the train goes as it is. Once it is whole and stands - pushed off the
entry's line, 30 s at most, as `OrderTry` waits for a train on a push line -, the runner registers the
enemy's train in `ai_trains[]` (as l19 and `TrainsTact`'s start), gives the others' first car its drive,
shows the message and logs `ARRIVAL n army a wave w at t s task k slot s cars <planned>` and `ARRIVAL n
made <the train's cars from its first one>` in `error.log` (with `- not whole: k of its n cars, sent as it
is` when a car never coupled; `lost` when a car was destroyed while it was made); a paid arrival is paid
as its first car comes. The runner of v0.2.9 and before made a car once the last one had moved 36 px
off the point and, after 5 s, on the last one: on the reporter's map (an entry past the west edge, issue
W2 of the stabilization patch) the lone first car stood for 5 s, the second came on it and the two were
pushed off, the third came after they had left - alone at the entry, not coupled - and got the drive,
so the log said `ARRIVAL 1 made 20,` and the 20,5 stood at the end of the push line. A map saved by such
a version keeps that runner in its script until it is saved again: Run the level writes its copy with
the new one (`mission_save` writes the whole block at every save), a map played from the custom maps page
runs the runner it was saved with - open it in the editor and save it once. `ArrivalsLeft(army)` counts the waves still to
come (those of a gating condition only while it holds); the template's win and loss wait for it. The
template (`map_tool.py new --script`) has both blocks, no arrivals and no reinforcement waves.
The editor sets no limits of its own: any number of arrivals, waves and trains in the factory's queue;
an arrival's train up to 30 cars (`temp_array`), a train of the queue up to 16 (the AI builds and
takes `ARR_LENGTH` cars). The Arrivals panel's list (its last rows) and a factory's plan (the row above its
line) name a limit of the game's AI only once it is reached: more than 10 patrols or posts (the rest attack), more than 16 enemy trains (they wait for a
free place in `ai_trains`).

An editor map made before keeps its reinforcement and its blocks until the panel's CONVERT: the new
blocks are added at the next save (refused with the reason when `main()` has no `TrainsTact();` after
`header.lgc`), the reinforcement becomes two arrivals at its point (the first wave at the start, the
others when the player has no locomotive and has a factory of its own - `IF_OWN_FACTORY`, condition
10 of `MissionIf`, as the old template and POLIGON bring their second wave), driving where the marker's
order 33 sent it, `PLAYER_REINFORCEMENT` becomes 0 and the point locomotives go. The script's start
trains (`start_trains*`, which the AI makes when the level starts and the editor showed as marks on the
track) become trains of the map (`place_start_train`): the cars of their type as the map's script has
them (its own `this_trainNN[]` wherever it stands in the script, else the authors'), the enemy's, laid
from the start point along the track as a drag lays a train (the first car at the point), with the
order in their panel - `ATA_PATROLn` a patrol to the far end of its patrol, else an attack - which
`OrdersTact` hands to the AI as before; the arrays are written empty. Rebuilding a lost start train
(`start_trains_renew`, read only for the start trains by `ai_events.lgc` DepoFree) and its raids
(`start_trains_random_task`) are not kept: a train of the map has no such setting. The queue
(`level_queue_*`) is already the enemy factory's plan in a map that runs arrivals. All of it is one
step of the history (the start trains are in it: `Operation::start`). Until a map is converted, a left
or right click on a start train's mark opens the Arrivals panel with CONVERT (`start_mark_click`), not
the panel of the rail under it, and the status line says what the mark is. Checked in play on POLIGON
converted: the light gun train came out as 20,5,20 at 1810,1061 and the light mortar train as 25,10,25
at 1437,1299, both patrolled between the same points as before for 90 s (the AI's `STTEST` log of the
old and the converted map side by side), the queue's first train was queued at 240 s and the AI took
it; SANDBOX_DRIVE_TEST (points 0-4 reached, the enemy's 6 cars taken away first), SANDBOX_WIN_TEST
(VICTORY) and SANDBOX_LOSE_TEST (DEFEAT, no second wave without a factory) pass.
`game/maps.new/sandbox` is POLIGON so converted (its map and script). A campaign mission or the sandbox (no editor blocks) shows
the reason only. A script edited outside the editor so that it lost `TrainsTact();` is saved without
the arrivals, unit limits and later units; the editor keeps them and says so in the status line. The marker locomotive's panel (VID 5, army 3) shows what it is in play - "ours arrive
here" with reinforcement waves, "taken away at the start" without - instead of its parking army 3,
and opens the Arrivals panel.

**Capturing** on the maps of the editor (`CaptureTact` of the same runner). The campaign captures in
two ways: a fort or a mining plant by its event handler `F162_0` / `F165_0` (`ai_default_event.lgc`,
every map has it: a train of another army within 75 px and no gun left within 150 px - none needed
for a neutral one), and a neutral factory or repair point only by the calls `LDepoCapture` and
`LMasterCapture` (`Captures.lgc`) that each mission makes in its own `main()` for its one such building
at a point of its own (MAP_RULES.md 7, "Capturing a neutral factory or repair point", has the table): the
factory's point lies at the outer end of its exit line, where a train coming in stops at the light (the
line pushes out), the repair point's where trains enter its one-way leg; radius 50-150, the player's
trains only, and a building taken stays the player's. So a map made in the editor, with none of those
calls, never captured those two (and `LDepoCapture` takes once "the" factory of army 2 or 0, which on a map
with the player's factory can be that one). `CaptureTact` (v0.2.0 to v0.2.11) took every neutral factory
and repair point when a train of the player's came within 100 px of the building's middle - but the editor
puts a factory 22 px inside its dead end with a 140 px exit line and a red light at its end, so a train
driven to the factory stops at the light about 120-170 px from its middle and nothing happened (issue of a
player on Discord, 2026-11-13; reproduced on main: the engine stopped 15 px from the line's end, 167 px from
the factory, which stayed neutral). Now `CaptureTact` takes a neutral factory when a car of the player's
comes within 90 px (`CAPTURE_FACTORY`, the missions' median) of the outer end of its own line, a neutral
repair point within 50 px (`CAPTURE_REPAIR`, the missions' most common) of the begin of its own line, and
either within 100 px of the building as before. It is a radius, not a stop: a train passing within 90 px of
the light takes the factory too, as the missions' radii (up to 150 at l07, l16) do. The runner reads the
lines from `PUSH_LINES`, `push_bx/by/ex/ey[]`, which the editor writes into the settings' block with the
arrivals' arrays - the same lines as main()'s `SetPushLine` calls, in the screen's points (y less the
height). A building's own line is, since 2026-11-25, the editor's answer (`OwnLine`, `push_of_x/y[]`: "A
building's line"); until then (`CaptureLine`) it was one of the shape the editor gives it: a factory's begins
within 75 px of its middle (`building_line` takes the dead end within 40 px of a point 22 px behind the
middle; rail and ground heights apart add a little) and its end is farther from it than its begin - the
nearest such begin; a repair point's begins within 240 px of it, is at most 480 px from begin to end and
passes within 90 px of it - the nearest such line. The editor's repair line follows the track 120 px each
way and stops at the first point past that, so a rail more (up to about 87 px) can end it; on a track
that curves within those 120-210 px (a radius of 250 px) the building stands about 80 px off the straight
from begin to end (the review of 2026-11-14 found the 30 px first used too tight there). Any other line - a light's, a level entry's, a
hand-drawn one, another building's - is not taken, and a building without a line of its own (no dead end
near, the line deleted with its rails, a map through CONVERT whose lines are elsewhere) keeps the 100 px
alone. The vector of a long line is halved until it is short before the products, so nothing passes 32
bits (a 1300 px line did). The trains are compared on the screen too: `CaptureTrain` walks the engine's
box round the point (`FirstInBox`, map coordinates, reaching 400 px further down since a car stands that
much lower on the screen than on the map by its height) and takes a car of class B_ENGINE and army 0, not dying (its
animation below ANI_DEATH, as the engine's `AskPlace` leaves dying ones out), whose GetX, GetY - GetZ lies
within the radius - so the height of the track at the light (a ramp, a bridge, high
ground) does not move the point. The neutral buildings are listed first and looked at after, so the units'
walk and the box's are never under way together. Without the arrays (a script written before) the runner
keeps the 100 px alone (`#ifdef PUSH_LINES`); POLIGON (`maps.new/sandbox`) has its six lines written in. The moment is the missions':
`PlaySFX(25)`, the green flash and `act_setarmy 0`, whose ACT_SET_ARMY fires the engine's `BuildingCapture`
event (`Depo_Action` 0x404984; `Unit_Action` 0x44d61f for VID 104), so `dBuildingCapture` shows
`DepoCaptured` / `MasterCaptured` - the runner called `BuildingCapture` itself too, a second message, and
no longer does; a `CAPTURE` line in `error.log` names the train and its distance. A repair point taking
over the enemy's wagons brought by the player's locomotive is the engine's own (`Building_UpdateWorkshop`),
on any map. A map gets the new runner when it is saved (a map with the arrivals' blocks: every map made
since v0.2.0) or by CONVERT in the Arrivals panel (an older one); a map saved now still runs in v0.2.11's
game (the runner is script; v0.2.11's editor, saving it again, writes its own runner and keeps
`PUSH_LINES` as an unknown define, harmless). Checked in play (rig, a 1024x768 map made in the editor:
Track tool main line and spur, a factory placed with army 2 - snapped to the spur's dead end with its line and
light -, an engine of the player's ordered to the factory): on main the engine stopped at the light, 167 px
from the factory, which stayed neutral; with the change, the map re-saved, the same stop captured it
(`CAPTURE vid 102 ... by a train at 434,376, 168 px from it`). A neutral repair point on the main line
(facing east, line 656 -> 940) was captured by a train driving through it, 180 px before the building at its
line's begin (main took it too, within 100 px, as the train went on through). With a build list set on the neutral factory
(5 20), F1 on it once captured queued a locomotive, which was built - the list survives the capture. The
released v0.2.11 game, a map saved by v0.2.11: not captured at the light (the player's report); the same game,
a map saved with the change: captured, with one message (`DepoCaptured`). The 20 missions pass. A neutral
factory placed in the editor starts with an empty build list (`copy_unit_state` gives the 14 units to the
player's only; every mission's neutral factory has a list), so the map maker must tick its units, or the
captured factory builds nothing. After the review of 2026-11-14 (a building's own line, the screen
compare): the same map re-saved, the factory taken as the train came into its light (30 px from the line's
end on the screen); a new map made in the editor wrote both buildings' lines, and its repair point was taken
by a train 1 px from its line's begin. Several lines, the arrays changed by hand on that map: a line
beginning 280 px away that passes 6 px from the factory and ends where the player's engine stands, a 5400
px line through the factory and a light's 70 px line ending at the engine - the runner before took the
factory in the first second (`by a train at 250,475, 403 px from it`), the new one never did (the engine
stopped at the light, 167 px away: no line of its own, the 100 px alone); with the factory's own line among
them it was taken at its light. The 20 missions pass. The second review (a dying car, a factory's reach,
a repair point on a curve): a repair point placed by the editor on the straight point after a bend (the
Track tool's run from a straight line into a 35-degree diagonal) got its line round the bend, 272 px from
begin to end with the building 30 px off that straight, and a train driving through took it at the line's
begin (3 px on the screen); the re-saved factory map was taken as before; the 20 missions pass. A dying car
was read in the code only. Not checked in play: CONVERT of a map without the
arrivals' blocks; a light on a ramp or a bridge (the test map is flat - the point does not use any height
but the car's own).

**The AI and the trains of an editor map - the review of 2026-11-05.** What the editor gives the trains,
read in the code and the scripts and tried on POLIGON copies: the Train arrivals panel (an army's train made
on a rail point past the edge at a time, n times, under a condition, then driving, patrolling or - the
enemy's - given to its AI as `ATA_FIGHT` / `ATA_STANDn` / `ATA_PATROLn`), a factory's plan (the enemy's
through the authors' queue and `DepoFree`, the others' car by car by the runner), the AI's knobs
(`LIMIT_ATACKERS`, the start trains' renew and raids, `CLEVER_ATTACK`), the trains of the map with their
orders (`OrdersTact`), the FACTIONS rules (fix 34: the neutrals answer only who hit them, the pirates fight
all) and the push lines with their posts (the post only shows the line; the line is what holds trains, for
every army). What was missing or wrong, and is done since: a train placed on a line that pushes got no
order (found too late - below); an order past a one-way line looked like a train doing nothing (the editor
now names the line); no train of any army could be let through a line (fix 35, the neutrals' and pirates'
red lights); a lost train of the map was never built again (the start trains' renew did not survive
CONVERT - "Rebuilt when lost" below); no order to attack a given object, no delayed start, no way to let the
enemy fire at neutral trains ("A target and a start" below); and the panels had no word on what their rows
do (the status line now says it for every row of the object panel, the Arrivals panel and a factory's
panel, "The panel" below). Left as it is: the start trains' own renew and raids in a map not converted,
the AI's own linkers (`BuildLinker`, a locomotive the AI builds to fetch a lost train's wagons - seen
leaving the enemy factory in the tests), and the AI's fixed limits (16 trains, 10 patrols and posts).

**The trains' orders** (`editor_edit.cpp` "trains", `OrdersTact` of the runner). A train of the map has
an order in its object panel, kept with its first car in the editor's state (undoable) - the same kinds
as the arrivals' and the factory queue's: in a map that runs arrivals, ORDERS: NONE, DRIVE TO... and
PATROL... for ours, the neutrals' and the pirates' trains, NONE, ATTACK, GUARD... and PATROL... for the
enemy's (its AI: `ATA_FIGHT`, `ATA_STANDn`, `ATA_PATROLn` between where it stands and the point, with a
train type of its cars); in an older map the patrol alone, as `PatrolEngine` lines in `main()`. The
block's arrays `ORDERS`, `order_vid/x/y[]` (the first car, as it stands), `order_task[]` (-2 drive, -3
patrol, an `ATA_` task), `order_type[]`, `order_to_x/y/z[]`, `order_from_x/y/z[]` (below) and the runner's
`order_unit/state[]`; `OrdersFind` runs on the level's first frame and finds each train with
`FindNearestSprite` (20 px), `OrdersTact` once a second gives each found train its order and logs `ORDER n
task t train 1 slot k at <s> s at <x,y>` (`train 0: none at x,y` for one not found).
**A train placed on a light's line did nothing** (the maintainer's report, reproduced on a POLIGON copy):
before 2026-11-05 the runner looked for the trains at its first 1-second tick, and a train standing on a
line that pushes (push_flag 1 - a factory's exit, a repair point's leg, an editor post's line) had been
pushed on by then - `ORDER 1 task -3 train 0`, the train at the line's end with no order. Now it is found
on the first frame, and the editor writes the end of such a line as the order's start (`order_from`,
`edit_train_patrols` / `pushed_to_end`): the order waits until the train has stopped there (within its
length and 40 px, `ACT_GET_SPEED` 0; at most 30 s), so a patrol runs between that end and its point (a
patrol from a point inside a factory's spur could never come back into it: the line forbids it) and the
enemy's AI patrol gets that end as its first point. The second way a train "does nothing": an order whose
point lies past a one-way line the train may not enter (`RailDot_CanPass`) - the engine drives to the point
nearest to the goal it can reach and stops at the line. The editor walks the network as the lines allow
(`reachable`, every link both ways but against a line, the red lights of `PASS_RED_LIGHTS` counted - in
play a factory's exit line keeps the rule for every army since 2026-11-10, changes.md 35, which
`reachable` does not know yet; a reversal's room is not) and says it when the order is given and in the ORDERS row's tip ("319,1000 cannot
be reached: the one-way line 319,936 -> 319,1061 is in the way - the train stops at it"; the line named is
the one without which the point would be reached), and "it stands on the line ..., which pushes it to ...
first". Checked in play: a neutral train on the player's repair point line with a patrol - found, ordered at
1 s, two rounds of the loop in 60 s; one in the player's factory spur with a patrol to the main line - found,
pushed out, ordered at 3 s past the spur's end, three rounds in 65 s (with the old runner: `train 0`, it
stood at the line's end); one ordered into the factory spur - the warning above, and in play it stopped
at 319,1184, below the line's end. A train the AI gets must be armed: a bare locomotive is made a ram (`ATA_CLASH`)
and waits. Checked in play: the enemy's trains with a gun car went to their post and stayed, patrolled
between their two points and drove at the player's train to attack it; ours drove to its point and
patrolled. Test mode: `order: vid x y drive|patrol|attack|guard [to_x to_y to_z]` lines of `M<file>`.

**A target and a start** (2026-11-05). What else the engine does for a train cheaply, and the scripts
can tell it: `Action(train, ACT_ATTACK, object)` (the player's own attack command: COM_ENGINE_ATTACK, the
train drives to shooting distance of the object and its turrets fire at it - for any army, outside the AI)
and simply waiting. A train's panel (a map that runs arrivals) has TARGET: PICK... - the next click takes a
unit or building of another army, a fort's or a mining plant's first gun for the host (the host has no hit
points) - which replaces its other order (`TASK_TARGET`, `order_task` -4, the object found at the start by
its VID `order_tvid` and place, 30 px): `OrderTry` gives ACT_ATTACK and `OrderTarget` watches it (state
3) until the object is gone, then the enemy's train goes to its AI to attack (`ATA_FIGHT`), the others'
stays; and STARTS: AT ONCE / AFTER n S (`order_wait`; - + by 10 s, with Shift by 1 s - issue #13: to the second, so that
trains with orders can be kept apart; the runner counts whole seconds once a second, `t` of `OrdersTact`, so a
second is exact - checked on a POLIGON copy: the order set to 13 s began "at 13 s") for any order: the train stands until
then. A train with a target is not rebuilt (the AI's queue has no such task). Checked in play on a POLIGON
copy: a neutral train 5,20 stood until 20 s, drove to the enemy fort and destroyed the gun it was told to
(hp 40, 30, 15, 5, gone at 55 s, `ORDER n target gone`), then stood; the enemy's patrolling train told to
attack the player's factory drove round to the factory's spur and fired at it. Per army, beside the red
lights and the AI's knobs: the enemy factory's AI screen has NEUTRAL TRAINS: FIRES / ONLY WHEN HIT
(`AiKnobs.neutral_trains`, `#define NEUTRAL_TRAINS 1`, the runner calls `SetEnemyCanAttackNeutralTrains(1)`
- World flags 2, which `Object_FindTarget` and `factions.cpp`'s `may_target` read): with it the neutral
train above was shot from 250 to 0 hp by the enemy on its way, without it not once. Not offered: "hold
until something happens" beyond the start time (each would be a condition of `MissionIf` on a train;
cheap to add per condition when wanted), the AI's aggressiveness beyond CLEVER ATTACK and LIMIT_ATACKERS
(the tactics scripts read no other per-army value), the pirates' and neutrals' own AI (they have none:
their trains do only what the orders and the arrivals tell them).
**Every army the same orders; guarding an area** (2026-11-05, the maintainer: every side can be given every
kind of order and keeps its own ways by default). A train's panel has ORDERS: NONE / DRIVE TO... / PATROL...
and FIGHTS: ATTACK / GUARD... / AREA... for every army; the enemy's ATTACK, GUARD and PATROL stay its AI's
tasks (`ATA_FIGHT`, `ATA_STANDn`, `ATA_PATROLn` - the default way of the enemy), the rest is the runner's:
-2 drive and -3 patrol (now for the enemy too, outside its AI), -4 target, -5 area, -6 hunt (ours', the
neutrals' and the pirates' ATTACK: the nearest hostile train of the whole map, one after another), -7 guard
(theirs: the point, 150 px). AREA (`TASK_AREA`, `order_radius`, `order_chase`) first held a post - the point - and
(it watches from the train's own place now: the next paragraph),
once a second, takes the nearest hostile train within the radius of it (`OrderFoe`: per hostile army a
`FindNearestSprite` of class ENGINE with that army's `GETSPRITE_ARMYn`, the nearest by `SizeTo`) with
ACT_ATTACK (`OrderArea`, state 5); back to the post with ACT_MOVE when the target is gone or - RETURNS, the
default - when the target is 1.5 radii from the post or the train 2 radii; with CHASES only when the target
is gone. The panel's AREA row: - n PX + (50 ... 2000, steps of 50) and RETURNS / CHASES; the selected
train's area is a ring of dots in its army's colour round the post (a GUARD of the others' too, 150 px).
Hostile follows the FACTIONS rules (`OrderHostile`, `OrderPeaceful`): ours and the enemy fight each other, a
peaceful army (the neutrals by default) attacks nobody first and is attacked first only by an army against
all sides (the pirates) or, its trains, by the enemy with NEUTRAL_TRAINS - so a neutral's AREA reacts to
nothing unless its army fights all sides. REBUILT WHEN LOST is now for every army and order: the rebuilt
train gets its order again from the order's runner (`PlanReorder`; the enemy's train of a non-AI order is
taken out of the AI's slot), ours from the player's factory. A map saved before keeps its own runner and
arrays; a save writes the new ones (`order_radius`, `order_chase` only matter for -5 ... -7). Checked in
play on a POLIGON copy: the enemy's train on the east line guarding 450 px round where it stood (RETURNS)
attacked the player's patrolling train each time it came within the area (12, 41, 69 s; 340-443 px from the
post) and went back to the post each time it left (19, 49, 77 s); with CHASES it followed the target west to
1178,1330, came back when it was gone (57 s) and took the next one at 68 s.

**AREA watches from where the train stands** (2026-11-05, the maintainer: AREA went to its point like
GUARD and clashed with it). GUARD (-7) goes to the point and holds it; AREA (-5) does not move: the area
(`order_radius` round `order_to`) can be anywhere - in the editor a click off the track takes the ground
point under it (`map_point`), near a track the rail point as before - and the train watches it from its
own place, where it stands when it first gets the order (after a pushing line; a train built again
drives back to the lost one's place, "Rebuilt when lost"): `order_home_x/y`, two arrays of the runner the save writes (zeros, filled by `OrderTry`; the hunt's
place is its start, the guard's its point). Once a second `OrderArea` takes the nearest hostile train
within the radius + 120 px of the area's centre - one coming near is taken before it is in - and lets it go
at the radius + 240 px (RETURNS; the second margin so that it does not turn back and forth at the edge),
CHASES only when it is gone; then it takes the next hostile train in the area, else it drives back to its
own place (ACT_MOVE `order_home`; a train attacked again on its way back keeps that place). A lost train is
logged (`ORDER n area: the train is lost`). The guard keeps its old rule (1.5 radii, the train 2 radii
from its point). The editor draws the selected train's area as before - the ring in its army's colour -
and a line of small dots from its first car to the area's middle; the AREA row is unchanged. Checked in
play on a POLIGON copy (Polish, 1024x768, the bottom line's mortar train made ours patrolling west, the
east line's gun train watching 150 px round 700,1299 from 1810,1128): it stood until the patrol came
260 px from the centre (9 s), went back to 1810,1128 at 25 s (the target 400 px off), was taken again on
its way back at 35 s and back at 42 s, again 53 - 95 s; a patrol turning 240 px from the centre (in the
margin, never in the area) was attacked at 254 - 266 px (10, 25, 42, 57, 74 s) and let go each time, one
turning 310 px away caused nothing in 70 s; with CHASES and the roles swapped (the mortar train watching
1810,700 from 1437,1375) it set off at 3 s (238 px), followed, took the next car at 85 s and was back on
1437,1375 at 100 s once the train was gone. In the editor: AREA... and a click on the grass set the area
on 1550,930, the ring there and the dotted line from the train.

**A captured train loses its order** (2026-11-21, the maintainer's report: trains that had a guard or patrol
order in the editor kept it after the player captured them). How the player takes an enemy train is the
engine's (the original's exploit, kept): its locomotive destroyed, the player's locomotive couples to its
wagons (`Train_HandleContact`: both trains' commands are cleared, attacks on the wagons are called off,
the script event `EnemyLinked` frees the AI's slot - the wagons stay the enemy's) and tows them; a repair
point the player's train passes takes the towed wagons over (`Building_UpdateWorkshop`, army 0 from then
on). The runner never looked at the army: an AREA, hunt or guard order (`OrderArea`) went on ordering the
order's first car - and so the player's whole train - with `ACT_ATTACK` and `ACT_MOVE`, and chose its
"foes" by the first car's army. Reproduced on a POLIGON copy (the enemy's 25,10,25 on the bottom line
watching 500 px round its place; a test script destroyed its locomotive and other wagon at 8 s, made a
locomotive of the player's 284 px west and gave it `ACT_LINK_ENGINE` to the wagon, then at 15 s the
player's order to drive to 674,950 past the player's repair point): coupled at 12 s, driving west at 112
px/s, then `ORDER 1 area: back to its place 1437,1375 at 22 s` turned the player's train round and `ORDER
1 area: attacks VID 5 army 0 at 28 s` - its own locomotive - left it standing at 954,1375 with
`COM_ENGINE_ATTACK` to the end of the run (113 s); the maintainer's SandStand log has the same lines
(`back to its place 1913,172 at 891 s from 1384,300`, then `attacks VID 90 army 1` once the repair point
had made the wagon his). Now the runner keeps, for each order, the army of its train when it is found
(`order_army`, a new array the save writes as zeros like `order_unit`; a train built again gets its
entry's army, `PlanReorder`), and once a second, for every order not yet done or watching (states 1-5,
2 included: a drive, a patrol, the AI's post or patrol), `OrderKept` asks `MissionOwner` which army has
the train now - that of a locomotive (VID 5, 10, 97) of another army in it, else the first car's own.
When it is another army's, the order is dropped for good (`order_state` 6; taken back later, the train is
not ordered again) and `MissionDrop` frees the AI's slot that holds the train (as `EnemyLinked` does) and
takes back a drive, patrol or attack command it still has (`ACT_STOP`; the coupling has cleared it
already, so in play it stops nothing). The log says it once: `ORDER n task t: the train is now army a's -
order dropped at s s at x,y, its command c`. A patrol of an arrival or of a factory's plan still on its
way to its first point (`ArrivalOnPoint`, `PlanOnPoint`) is dropped the same way (`ARRIVAL n: its train is
now army a's - its patrol dropped`, `ARRIVAL Fn: ...`) instead of being given `PatrolEngine` there. Not
touched: how a train is captured (the coupling, the repair point), a train the order keeps (its own army's
locomotive, e.g. the AI's linker), and a train already patrolling between its points (the engine's
`COM_ENGINE_PATROL` goes with the destroyed locomotive and the coupling: seen as command 0 after it). A
`REBUILT` train of the map dropped before `PlanAdopt` took it is adopted all the same (state 6 counts),
so its loss is seen and it is built again. Checked in play on the same POLIGON copy: dropped at 11 s
(`its command 0`), the player's order taken at 15 s, the train drove through the repair point (the wagon
army 0 at 28 s) and stood at its goal 674,939 to the end, no runner line after the drop; a runner patrol
(-3) and an AI patrol (`ATA_PATROL0`) of the east line's 20,5,20 captured the same way - dropped at 23 s
(command 0, the AI's slot already free), the player's drive taken, the trains the same as with the old
runner apart from the drop's line; the orders not captured (the bottom line's AI patrol, the AREA train
untouched) logged and moved as before. The maintainer's SandStand loaded in the editor, saved and run (Run
the level), with the editor before and after the change: the script of the run's copy grew from 134088 to
135834 bytes of code (the new runner, written at the save as before), and the 9 orders, the plans and the
arrival logged the same lines at the same seconds for 90 s. A map gets the new runner when it is saved
again in the editor; one not saved again keeps its old runner and the old behaviour.

**The armies' fight rules** (2026-11-05, the same request: every side keeps its own ways by default and can
be set otherwise). `factions.cpp` reads `#define FACTIONS_PEACEFUL <mask>` beside FACTIONS (bit n army n,
4 - the neutrals - without it; only the neutrals' and pirates' bits count: ours and the enemy always fight
each other): a peaceful army fires only at an army it holds a grudge against (one that hit it, `note_hit`,
now for any peaceful army), and is fired at first only by an army against all sides (a non-peaceful army 2
or 3), a weapon of flag 0x80 or, the neutrals' trains, the enemy with NEUTRAL_TRAINS. A neutral's or a pirate's
panel: IN A FIGHT: ANSWERS ATTACKERS / ALL SIDES (`AiKnobs.peaceful`, one step of the history; the define
written only when it is not the default). IT FIRES (the armed members' behave bit 1) is now offered for
every army: a peaceful one's WHEN ATTACKED / NEVER, the others' ON ITS OWN / ONLY WHEN TOLD; AT RED LIGHTS
(fix 35) for every army's trains. The runner's hostility (`OrderPeaceful`) reads the same define, so an
AREA order of a neutral reacts only when the neutrals fight all sides. Checked in play on a POLIGON copy: a
neutral train guarding 400 px of the bottom line with the neutrals on ALL SIDES took the enemy's mortar
train 337 px from its post at 2 s, followed it east and came back at 53 s (`factions: ... peaceful armies
mask 0`); with the default it stayed on its post the whole run (mask 4).

**The pirates for the player** (2026-11-21, the maintainer's decision): on a map with FACTIONS the pirates
look and handle like an enemy. The minimap draws them black (`minimap.cpp`, at Minimap_Draw's two loads of
the colour it gives armies 2 and 3 - yellow - for their trains, buildings and factories; what is drawn stays
the engine's, so their other armed units are not shown, as for the neutrals). The player's mouse takes them
as the enemy (`factions.cpp`, `enemy_test`): the engine's four tests of army 1 - Player_HandleMouse's attack
test (an object of spriteType above 2: the attack cursor 5, far 6, out of reach 12, no ammo 1; the right
click attacks, ACT 0x20) and its repair point test (the enemy's repair point is a target, not a place to be
repaired at), Player_HandleInput's hidden objects (the enemy's mines are never under the cursor) and its
click (ACT 9, the answer of a clicked unit, not sent to the enemy) - take army 3 too. The pirates that answer
only whoever attacks them are attacked by the right click as well (the hit gives them their grudge), so they
get the attack cursor too. The money crates of an editor map are army 3 but are never under the cursor (the
engine's pick takes no crate) and the minimap does not draw them: nothing changes for them. The neutrals keep
the engine's tests (their trains can be attacked by a right click, their buildings captured), and so does a
map without FACTIONS - the campaign, where army 3 is the crates and the reinforcements' point locomotive.
Checked on the rig on a POLIGON copy with our mortar train, a pirate train, the pirates' factory and repair
point, the neutrals' factory and repair point and the enemy's fort: the minimap the old build's but for the
pirates' 42 pixels, yellow to black; over a pirate the attack cursor (5 near, 6 far; the old build's move
cursor 2, and over their repair point its repair cursor 4), and a right click made the pirate the train's
goal (command 28, its hp falling) in both fight modes - the old build moved there (command 23); the enemy's
towers and the neutrals' repair point (capture, 15) as before, and the same map without FACTIONS as before.
Mission 17's crates: the same cursor (2), click (a move) and minimap dots in both builds.

**What a row does, and click-and-hold** (2026-11-05; `editor_rows.cpp` `PanelRows::tip` / `hold_area`,
`ContextRow.tip` and `LABEL_TIPS` of `editor_edit.cpp`, `ROW_TIPS` of `editor_arrivals.cpp` and
`editor_factory.cpp`). The status line at the top names what the row under the mouse does - every row of an
object's panel that has a tip (army, in a fight, it fires, at red lights, train, orders, target, starts,
rebuilt, a post's way, colour and line, a line's action and direction, height, delete), and the labelled
rows of the Arrivals panel and of a factory's plan entry (who, cars, entry, when, repeat, next, then, the
points, the train); an order the train cannot carry out puts the reason there in place of the row's words
(the ORDERS row). A train's panel is grouped by meaning: the army and its rules (army, in a fight, it fires,
at red lights - the last three are rules of the whole army), then the train (turn round, couple), its orders
(orders, target, starts, rebuilt) and delete. The maintainer's idea for the arrivals, the building menus of
the Anno games: a car icon or + CAR of an arrival or of a factory's train held down for 350 ms opens a strip
of the game's 14 factory units above it (two rows of seven, inside the panel's width), the status line names
the unit under the mouse, and letting go on one puts it in that car's place (or adds it at the end for
+ CAR) - one step of the history; a short click does what it did (chooses the car, opens the Vehicles
palette), letting go elsewhere nothing. The panels' `WM_LBUTTONUP` now reaches the rows (`panel_message`),
which keep the pressed area of the last frame (`press` / `release`). Checked in the game (Polish 1024x768,
English 1920x1080): the CARS and REPEAT tips in the status line, the train panel's tips and order, the strip
over arrival 1's second car with "Miotacz ognia (VID 90): pusc, by go tu wstawic", the car replaced by it
on letting go (undo 1), the factory entry's NEXT row.

**The templates** (TEMPLATE... in the Arrivals panel and in the enemy factory's queue) are the authors'
trains exactly, read from the game's own scripts when the list is first shown
(`mission_author_trains`, `editor_mission.cpp`): every `int this_trainNN[]={...}` of
`maps\script\TrainsTactDeclares.lgc`, named by the `#define ATT_...` before it, and every one a
campaign mission redefines before it includes that file (`maps\lNN\lNN.lgc`, lines that are not
comments; 107 different compositions with the 20 missions - the missions put the locomotive in the
middle or at the back, e.g. `{20,5,30,20}`, `{80,5,80}`). The cars keep the authors' order; a mission's
own is named "(MISSION n)". The missions' queues (`level_queue_trains`, `start_trains`, l13/l19's
`our_queue`/`enemy_queue`) take their trains from these types, so they add no compositions of their own.
The engine drives a train whatever the locomotive's place (checked in play: 20,5,20 and 20,20,5 drove
to their points), so the only rule is a locomotive somewhere in the train.

**The panel** (the strip's Train arrivals entry, a click on an arrival's entry or point on the map, the marker's
panel, Mission settings' Arrivals... button, which keeps the dialog's settings as OK does) is drawn
in editor_panel.cpp's frame, in three screens. The list: each arrival in two rows of plain words
(number, army, entry, the first car's name and the number of cars; when and how often), a click
opening it, + NEW (ours, a locomotive and two gun cars, at the entry of the one chosen), COPY n and
DELETE n for the one opened last, pages of five (every row stays above the button bar). An arrival, one thing a row: WHO; CARS (the palette's
icons over three rows, in the train's order - a click chooses a car, < and > move it, REMOVE takes it
away; + CAR opens the Vehicles palette, which adds the car picked at the end and goes back; TEMPLATE... the list of the authors' trains of the normal difficulty by name,
the defence trains aside - they have no locomotive); ENTRY (PICK: the next click takes the
rail point nearest to the cursor, shown in cyan; "line ok" when a push_flag 1 line of the editor or
the script runs from it); WHEN - one choice of AT THE START, AFTER n s, WHEN THE PLAYER HAS NO TRAIN,
WHEN THE PLAYER PAYS n (the first time, or the condition `IF_NO_TRAIN` / `IF_MONEY` "only then" with
the first time as NOT BEFORE); an older map's other condition is shown as it is; REPEAT - ONCE, n TIMES,
WITHOUT END, EVERY m s (- + by 10 s, with Shift by 1 s, as every time of the panels: AFTER, NOT BEFORE, a
factory plan's AT and EVERY, REBUILT IN - `edit_time_step`; Shift reaches the editor's key state also while a
panel takes the keys); THEN and the POINT(S) (PICK; a patrol's two ends one after
the other); MESSAGE (ours and the enemy's) on the title's line at its right. The strip above the title names a real problem with the button that
mends it: no entry (PICK THE ENTRY), no track at it, no entry line - the line and its lights are one
thing (the posts own a line that ends at them, `edit_post_line` of the signals' side): with a pair of
posts within 400 px on the way in (`edit_posts_on_way`) LINE TO THE LIGHTS lays the line from the
entry to them, else PUT LIGHTS puts the pair the first place 50 px inside the map with its line
(`edit_place_entry_posts`); the entry row shows "LIGHTS x,y" once they are there (checked on a POLIGON
copy without the top-left exit's two posts and line, an arrival entering there: "! NO LIGHTS AT THE
ENTRY - PUT LIGHTS", one click, `entry line -195,149 -> 52,149 with its lights`, the pair at the map's
edge and LIGHTS 52,149 in the row; played, the train came in at 11 s and stopped at them) -, no locomotive anywhere in the train (ADD ONE), no point or no other
end (PICK IT), an endless enemy - the map is not won by destroying it - (10 TIMES), a train that stays
where the next ones come (DRIVE ON). The list's last rows: a limit of the game's AI once reached, the
authors' reinforcement to convert. The arrow at the bottom left, Esc and the right button go a screen
back, from the list they close the panel. Each change is one step of the history
(`edit_commit_arrivals`; Ctrl+Z / Ctrl+Y reach it while the panel is open). The entries are drawn on
the map ("ENTRY n"), the chosen arrival's points too, joined to its entry. An entry's mark is dragged with the
Select tool (since v0.2.12, GitHub issue #9; `start_entry_drag`, `finish_entry_drag`): pressed on its square or
its "ENTRY n" the mark follows the rail point under the cursor (`cursor_dot`, drawn cyan), and let go there
every arrival of that entry gets the rail point - one step of the history, `OpArrivalEntry` as the panel's
ENTRY pick; let go within 3 px of where it began it is the click it was (the arrival in its panel), off the
view or on its own rail point the entry stays (`MsgEntryStays`). Only the button let go ends it so: any other
end of the strokes (`end_strokes`: a tool chosen, a panel opened, undo or redo, a save, the menu, Alt+Tab, the
capture lost) cancels it - the mark where it was, no step, no panel. (The Select tool's object drag and the
Region tool's rectangle do take what they have at such an end: `end_drag` and `finish_region` run for any
reason.) A deletion of rails that
takes an entry's or a point's rail point unsets it in the same step of the history
(`edit_arrivals_track_gone`), so no marker stays in the grass and Ctrl+Z brings it back. Test mode: `arrival:` lines
of the `M<file>` settings (`arrival_of_line`: `army=`, `cars=`, `at=x,y,z`, `first=`, `every=`,
`times=`, `if=`, `n=`, `earlier=`, `task=`, `to=`, `to2=`, `raid=`, `msg=`).

Checked in play (three test maps on POLIGON's ground and track, scripts in the editor's format with a
logger of the trains' places every 5 s - the `ARRIVAL` lines of the runner in `error.log`; the maps
and their generator are kept outside the repository): every arrival came at its time - at the start
(1 s), at a set time (70 and 90 s), every 45 s three times (70, 115, 160 s), when the player's last
train was lost (4 s after the enemy destroyed it, never while a train was left), while a neutral
factory stood, the campaign's "no train and a neutral factory" (and not again once that factory was
gone), when the player had no factory, when the player had no units (the game counts the cars and the
guns, `ARMY0_NVID_ARRAY`, not the factory), "also earlier" while the player had fewer than 3 trains
(at 7 s for a time of 999 s), only when fewer than 1 enemy train attacked, not while the enemy had 2
trains or more, and the bought one at its time with the money taken; a wave waited while a train
stood at the entry. After it: the player's, the neutrals' and the pirates' trains drove to their
points and stopped there (the pirates fired at the player's trains in passing), the enemy's patrolled
between its two points again and again, went to its post and stayed, and attacked the player's trains;
the messages showed. "Stays at the entry": such a train stood behind the entry line and held every
later arrival there - the panel warns when another one comes from there. Not offered: "attacks the
forts" (`ATA_FIGHT_FORT` drove at the player's base, not at the player's fort beside it), the raids
(no sortie seen) and the conditions other than "no train" and "pays" (read from a map and kept). The template's script compiles
and runs with no arrivals; POLIGON and a copy of it saved on the template (its reinforcement not
converted) bring the player's train as before (in the player's unit bar); a map's CONVERT turns
POLIGON's reinforcement into two arrivals of its cars at its entry (with "line ok").

**A factory's build options** (`src/LocoFix/editor_factory.cpp` the panel, `editor_mission.cpp`
"factories", `editor_edit.cpp` "a factory's panel's side"). How the missions do it: what a factory
builds is its build list, the `Unit.items` of its SPRD record, in the order of its keys (F1-F12, [
and ] - `Player_SetPanel`); the campaign grows the player's units mission by mission through these
lists alone (l02's neutral factory has 5 and 20, l14's player's ten, l15-l20 the super weapons) -
no script adds or removes an item (`ACT_ADD_ITEM` 54 and `ACT_DELETE_ALL_ITEM` 57 are not called by
any mission). The enemy's factories have empty lists in 11 of the 13 missions with one: the AI
(`ai_events.lgc` `DepoFree`, only for a factory of army 1 - the factories of the neutrals and the
pirates build nothing by themselves; a neutral one is there to be captured, a pirate one is not:
`Captures.lgc` `LDepoCapture` and the runner's `CaptureTact` take army 2 only) builds with `ACT_BUILD_UNIT`
commands what `level_queue_*` and the rebuilding of lost start trains ask for, in its first free
factory, and ignores the list. Unit limits: `InitDefault` would call `AddUnitLimit(0, nvid, LIMITxx)`
for `#define LIMITxx`, but every one of those defines is commented out in the missions, and the
function takes its arguments swapped (the first is the limit, the last the slot: 255 the total,
else the army - `BUGS.md`), so that path would set a limit 0 in the slot LIMITxx; the editor calls
`AddUnitLimit(max, nvid, army)`, which `Player_CanBuild` checks against the army's units on the map
and in the factories' queues (with LocoFix fix 14) - a script's `Create` is not limited by it.
**The panel** (the factory's object panel: FACTORY: WHAT IT BUILDS...): for ours, the neutrals' and
the pirates' factory the game's 14 factory units as the palette's icons - a click ticks or unticks
one, the ticked ones keep the game's order and show their key -, and for the unit clicked last IN
THE LIST: at the start or after a time (steps of 30 s) and AT MOST: the player's limit of it (army 0,
0 none); a neutral factory says that the AI builds nothing there and the list is what the
player builds after capturing it, a pirate one that the player captures only neutral factories. For the enemy's factory:
its plan (below). In the script: the start list stays the factory's
SPRD items (the map's file), the units it gets later are `UNLOCKS` and `unlock_x/y/vid/time[]` (the
factory by its place, kept with the factory when it is moved and dropped with it when it is deleted,
written as it stands at the save) and the limits `UNIT_LIMITS` and `limit_army/vid/max[]`, both in the
settings' block and run by `FactoryTact` of the arrivals' runner (the limits once at the start; a
unit `ACT_ADD_ITEM` at its time, logged `UNLOCK vid n at t s` in `error.log`) - so these two need a map
that runs arrivals (the panel says so and leads to the conversion), while the list and the enemy's
plan (once each, at a time) work in any editor map. Every
change is one step of the history (the factory's list is in the editor's state of an object now).
Test mode: `limit: army nvid max` and `unlock: x y nvid seconds` lines of the `M<file>` settings.
Checked in play: with a limit of 1 on VID 20, F3 twice queued one and F4 (no limit) twice queued two;
a unit put into the list at 30 s (`UNLOCK vid 30 at 30 s, factory 1`) was there the next time the
factory was selected; the keys stay the game's own per unit (a list without the airship cart leaves F8
out). In play the factory's panel shows both (`depot_rows.cpp`, engine.md "The factory panels"): a
unit with a limit has `used/limit` in its icon's bottom-left corner (the price is top left, the key
bottom right) - used as `Player_CanBuild` counts it, the army's units of the VID on the map and the VID
in the factories' queues; at the limit the icon is grey -, and a unit that comes later (the runner's
`unlock_x/y/vid/time[]` of a factory within 30 px, its time still to come, read from the running
script's variables) stands after the list's units in the order of the times, greyed (its click does
nothing) with no key and the time left, `m:ss`, by the runner's clock (`(GetTime() - time_game_total)
/ 1000` against `unlock_time`); at its time the runner puts it into the list and the open panel is made
again, so it lights up with its key in the same place. The mouse tip over such an icon (the unit's name)
adds "- at most 2 on this map, 1 built or queued" / "- can be built in 0:35" (`[Editor]` keys
`PlayFactoryLimit`, `PlayFactoryUnlock`, in every pack). A map without the runner shows no later units;
a unit without a limit gets an empty text, so a list without either looks as before.
**The panel's rows** (2026-11-13; the maintainer's report: in the enemy factory's list of the authors' trains
the last row lay on the page arrow, and every page should read more plainly - what can be clicked and what
cannot). A page has 13 rows (`ROWS` from `TOP`, 14 px apart, the last clear of the button bar) and the strip
above the title, and a row means the same in every army's factory (`ROW_HEAD`, `ROW_NOTE`, `ROW_LINE`,
`ROW_ACTIONS` ... of `editor_factory.cpp`, named for the coming interface scale): row 0 the tabs (ours, the
neutrals', the pirates': BUILD LIST, TRAINS IT BUILDS) or what the list under it is (the enemy's plan, the
authors' trains, the AI's settings); row 1 (rows 1-2 on the build list) a note on what the factory does with
it - ours: a click puts a unit in or takes it out, a neutral or pirate one: the AI builds nothing there -;
then the page; under a list a groove line (row 11) and the actions (row 12) at the left - + NEW, COPY n,
DELETE n, AI... - with the pages `< n/m >` at the row's right end; an entry's problem and the button that
mends it in the strip above the title, as an arrival's in the Arrivals panel. The plan shows 4 entries a
page (two rows each, row 10 kept for the game AI's limit once reached), the authors' trains 10 - both from the
rows there are (`LIST_PAGE`, `TEMPLATE_PAGE`). The editor's own signs, the same on every page: a label ends
with ":" and a note is in parentheses - neither is ever clicked -; a choice is lit under the mouse, and lit and
underlined when it is set; "..." at its end opens another page (TEMPLATE..., AI..., CONVERT IT IN THE
ARRIVALS PANEL...); - + change a number, < > move a car or turn the pages; a list entry's two rows are one
choice. What is neither: a word that names the number after it (NOT BEFORE, EVERY, N:, the Arrivals panel's
words, shared), a knob's authors' name under its label (a value), a warning beginning with "!" (with the button
that mends it after it), and the counts 1/3. A text longer than its row is cut and ends with "." (an abbreviation's dot; trailing spaces and commas
dropped first, and never inside a number or a time: back to the space before it, as "EVERY 1:3." would read
as another time) and its whole text is the status line's while the mouse is over it (`fit`, `cut_to`; a cut
note's whole text over all its rows) - a list entry's
task and a train's price stay whole, its name is cut (`fit_head`); a long note wraps at spaces into its rows
(`note`); a value starts at the value column, or a character after a label longer than that column (English
IN THE LIST:, Hungarian KIVÁLASZTVA:, a knob's Czech NEUTRÁLNÍ VLAKY:), and a train's car icons a character
after the longest of CARS:, + CAR, TEMPLATE..., REMOVE (English TEMPLATE... took clicks of the first car's
click-and-hold area, which is checked first). The arrow back's tip only while the factory panel is drawn
(`PanelRows::fresh`). The arrow back at the bottom left says so in the status line ("Back one
page", Esc and the right button too). What the old layout got wrong (found with the test steps `H` and `G<n>`,
below, and `tools\rig\panel_rows_check.py`): the authors' trains' 12th row lay on the page arrows' row - a
click on > took that train into the entry instead of turning the page; a neutral or pirate factory's note
was drawn over the limit's note; with WHILE and a number, a repeat, NEXT and a point an entry's problem line
fell into the button bar; texts cut at the panel's edge without a mark, a condition's > could go past it;
"(CONVERT IT IN THE ARRIVALS PANEL)" was a choice that looked like a note and FROM THE START, AFTER THE LAST,
AFTER TRAIN n IS MADE notes that looked like choices; a list entry joined name and task with ": " after
names that hold one ("MORTAR + CARS: 7: PATROLS", now " - "). What the panel does did not change: the same
choices, the same steps of the history, the same data. Test mode: `H` writes the open factory panel's texts
and choices with their boxes to `LocoFix.log` (WM_COPYDATA 40052), `G<n>` clicks the middle of the n-th one
(40051) as `k<x>:<y>` does - the same numbers in every language, so one list of steps walks all six;
`T[<k>|]<text>` clicks the k-th whose text begins so, one list of steps for builds numbered differently.
The groove line is two Direct3D fills: drawn among the queued texts, the texts' lock waits for the GPU. On the
enemy's plan and the authors' trains that was the page's only Direct3D drawing (+1 to 1.5 ms a frame there),
so it is drawn after `panel_draw`'s last lock (`factory_draw_after`); the other pages draw Direct3D before
that lock anyway - the icons, and a set choice's underline (`PanelRows::put`), e.g. the open tab of our
factory's plan. Checked in the rig (2026-11-13/14, a test map from
TheFactory with a factory of every army, 12 + 7 + 3 + 1 entries): every page walked in all six languages at
1024 x 768, 1920 x 1080 and 640 x 480 and the map without arrivals - `panel_rows_check.py` 0 faults on every
page (the old layout 8-16 a walk); the same edits by text in the old and the new build - every row's choices,
- +, < >, the problem's button, the pages, the tabs, + CAR (the Vehicles palette), the click-and-hold strip
on + CAR and on a car, the convert link, the arrow back - gave the same history lines, a byte-identical
saved script and the same map, undo x5 + redo x5 the same script again, and a load + save the panel's
entries, unlocks, limits and AI values unchanged; the panel's draw time with the box alone (`EditorProfile`,
60 fps throughout) the same as before or less (1024 x 768: the plan 1.99 ms against 2.03, the authors'
trains 1.70 against 1.79, an entry 3.20 against 3.06, the build list 2.7-2.9 against 3.3); the 20 missions
passed.

**A factory's plan** (`editor_factory.cpp` the panel, `editor_mission.cpp` "factories" and the
runner's `PlanTact`). What a script can make a factory build, seen in play (probe maps of the
sandbox, `ARRIVAL`/`FQP` lines in `error.log`): `AddCommand(f, ACT_BUILD_UNIT, v)` - what the AI's
`BuildTrain` does - makes a factory of any army build the cars in the order given, each paid from
that army's money when it starts (the price the factory panel shows: the build time in s - 5: 30,
20: 25, 25: 30, 10: 50; `GetVidData(v, VID_BUILDTIME)` gives it for locomotives only, 0 for cars,
so the editor writes the prices, `price_vid/price_of[]`), and drops a car the money does not cover;
the factory's build list does not limit it and stays as it is; the cars line up on the factory's
push line and link into one train (without a push line each new car is made on the last one, which
is destroyed). The AI's `BuildTrain` (`ai_functions.lgc`) swaps a train's first two cars before it
builds them, so the authors' queue builds LIGHT GUN `{5,20,20}` as 20,5,20; types ending in 5 change
with the difficulty (`arr_gettype`); the destruction of any enemy factory (`DepoDestroy`) resets the
AI's one build (`depo_task`, `depo_array`), so a train under way in another factory loses its AI
slot while that factory still makes the cars already ordered (in fqtest, a copy of arrtest2, our
fort's two guns - VID 115 at 1171,192 and 1171,259 - destroy the enemy factory at 1313,565 at 15 s:
"ENEMY FACTORY DESTROYED" is that one; the factory switched from ours to the enemy stands). The
original's queue and these quirks stay; the editor's plan works around them.
**The data**: `FactoryState.queue`, entries (`FactoryTrain`) of the enemy (`factory` null) and of a
factory of ours, the neutrals' or the pirates' (the factory by its place `fx, fy`, found by
`edit_level_held` and placed by `edit_place_plans` at a save - an entry of a deleted factory or of
one that became the enemy's is not written); each with its cars as the train is to leave the
factory, WHEN (`when`: at `time` s, `time` s after the first train of the entry before it of the
same factory is made, or while a condition holds and not before `time` s - the conditions are one
list, `mission_conditions()`, tested by the runner's `MissionIf(c, n)`, which the arrivals use
too), REPEAT (`times` 1, n or 0 without end, the next one `every` s after the last one was made) and
THEN (the enemy's: `ATA_FIGHT`, `ATA_STANDn`, `ATA_PATROLn`; the others': stays, drives to a point,
patrols, as an arrival's -1 / -2 / -3), and `AiKnobs` (below).
**The script**: a map that runs arrivals gets `PLANS` and the columns
`plan_type/task/when/first/every/times/if/if_n/prev/fx/fy/army/to_x/to_y/to_z/to2_x/to2_y[]`, the
runner's state `plan_state/done/last/since/slot/fifo/next/base/unit[]`, `PRICES` with the prices,
and an empty authors' queue with a place for each entry. `PlanTact` puts an enemy wave that is due
into the queue's first empty place with a time already past (`DepoFree` builds the queue's first
one, in the AI's first free factory, while fewer than `LIMIT_ATACKERS` attack) and follows it to the
AI's slot (`ATT_DEPO_CREATED`) and the train made; an enemy entry's type is its cars with the first
two swapped (`declared_order`), so `BuildTrain` swaps them back, and never one ending in 5, so the
difficulty does not change it. The others' factory is ordered car by car (`PlanBuild`): a car when
the factory is free (`ACT_GET_COMMAND` not `COM_BUILD` and its `GetCommands` back to its length
before) and the army has its price, then the train gets its order on its first car once it is whole
(its last car of its army within 300 px of the factory, its cars linked) and stands - the factory's
push line moves it until its last car is past the line's end, as `OrderTry` waits for a train on a
push line -, 30 s at most (`- not whole` in the
log when cars are missing then); a patrol starts from its first point once a car of it is within 40 px
(`PlanOnPoint`, `MissionAt`); an entry runs only while its factory stands and has its army, one entry at a time in a
factory (seen in play: two entries started together mixed their cars, 5,20 and 5,25 came as 5,25,5,20)
and not while a car stands on the factory's exit (`PlanExit`). The exit is where the new cars go:
`Depo_ReleaseUnit` makes each one at the factory's place and deletes what stands there, and the push line
pushes them along until the last one is past its end, coupling them to a train of their army they touch
(the cars of the next train joined one that stayed at the door) and pushing another army's along
(engine.md "How the cars follow"). So the runner takes the factory's own line (`OwnLine`, the editor's
answer - "A building's line": its rule line, or an exit dragged onto another branch or drawn by hand from
its dead end; until 2026-11-25 `FactoryLine`, the editor's line whose begin was within 75 px of the factory
and whose end was not nearer) from its begin, behind the place, to its end and on past it along
its direction by 48 px a car of the entry's train, 20 px to either side, and waits while a car of any
army (not a dying one) has its screen point there; a factory without a line of its own waits while a
car is within 48 px of its place, where the cars are made on one another. Until 2026-11 it waited while
any train stood within 250 px of the factory, so a factory with trains parked beside it - not on its
spur - never built (the maintainer's Capture The Gear: the pirates' and the neutrals' factory stayed idle
with cars standing 165-216 px from them, off the spur). The wait is logged once, when it begins
(`ARRIVAL F<n> wave w waits at t s: a train on the factory's exit (VID v army a at x,y)`, `plan_wait[]`:
its start + 1), and the `ordered` line after it adds `- it waited for its exit from t s`. The rule is
in the runner, which the editor writes anew at every save (the block "// <editor arrivals>"): a map
saved before keeps the 250 px rule until it is saved again in the editor (Run the level writes its copy
with the new runner already). Checked on the rig (2026-11-20, copies of Capture The Gear): as the map
was saved (the old runner) the pirates' and the neutrals' factory ordered nothing in 100 s while the
enemy's queued its wave at 60 s; saved again in the editor and run, both ordered at 31 s and built
5,20,20 at 136 s, the enemy's as before (queued 60 s, taken 62 s), and the pirates' next entry waited 4 s
while the train just built was leaving its line's end; with a pirate locomotive placed on the pirates'
spur (its push line pushed it to the line's end) the pirates' first entry waited from 30 s, the
neutrals' was ordered at 30 s and built at 135 s, and after the test copy's own code sent the
locomotive away at 100 s the pirates' was ordered at 102 s and built 5,20,20 at 207 s, its next entry
ordered at 211 s. The panel says it of the held level: on the list of ours, the neutrals' or the
pirates' factory, while a car of the map stands on that exit for the longest of its entries
(`edit_factory_exit_car`, the same numbers), the spare row reads `! A TRAIN ON ITS EXIT: THE PLAN
WAITS` with the whole reason in the status line - only then, so the page's texts keep their numbers
(checked: with that locomotive placed, the pirates' list showed it under its 4th entry, the neutrals'
list nothing). Every
wave is in `error.log`: the enemy's `ARRIVAL F<n> wave w queued ... cars <planned, as built>`,
`taken ... slot k`, `built ... made <the cars from the first one>` or `lost` (the AI dropped it);
the others' `ordered ... cars` and `built ... army a made` or `lost` (the factory gone); an
arrival's line has its `cars` and then `ARRIVAL n made <the train's cars>`. A map that
does not run arrivals writes the enemy's entries into the authors' queue as before (each once, at
its time; the panel offers no more there). Mission settings shows the number of the enemy's entries
and opens an enemy factory's panel (Plan...); a test file's `queue:` lines still give the enemy's
entries (each once), and `plan:` lines (`plan_of_line`: `cars= when=at|after|if first= if= n=
times= every= task= to= to2= factory=x,y`) the whole plan.
**Rebuilt when lost** (2026-11-05; the runner's `PlanLoco`, `PlanMade`, `PlanWatch`, `PlanAdopt`). How the
authors rebuild: `start_trains_renew[i]` of a start train - `DepoFree` (`ai_events.lgc`) builds it again,
`RENEW_TIMEOUT` +- half at random after it misses a train with its task (`start_trains_task[i]`), while the
enemy has fewer units than its start trains need (4 each, + `REBUILD_ADDON` + 6 + its buildings) - only
army 1's, only start trains; CONVERT makes those trains of the map, which had no such setting. Now: an entry
of a factory of the enemy, the neutrals or the pirates with REPEAT n TIMES or WITHOUT END has, beside EVERY
n s, AFTER THE LAST IS MADE (as before) or LOST (`FactoryTrain.renew`, `plan_renew[]`): the runner keeps the
locomotive of the train it made (`plan_loco`, the first car of VID 5, 10 or 97 - `PlanLoco`) and the next wave
is due `plan_every` s after that locomotive is destroyed or taken by another army (`plan_lost`; -1 while it
is watched, as a variable of an object is 0 once the object is gone - a first try, testing the locomotive
for 0, never saw a loss). A train of the map gets the same in its panel (an enemy's, a neutral's or a
pirate's train with an order, in a map that runs arrivals): REBUILT: NO / ONCE / 3 TIMES / ALWAYS and REBUILT
IN: n S AFTER THE LOSS (`Order.rebuild`, `rebuild_wait`, in the object's editor state, so undoable), written
as one more plan entry of `plan_when` 3 (`WHEN_LOST_TRAIN`, never in the panel's list) whose `plan_order` is
its order's number: `PlanAdopt` takes the train `OrdersFind` found as the entry's first wave, and a lost one
is built again with the same cars and order - the enemy's through the AI's queue as any enemy entry, the
others' car by car in the factory of its army nearest to the train at the save (`rebuild_factory`), driving
to where the lost one stood when it patrolled. `PlanAdopt` takes the map's train once its order has been
given (`order_state` 2 and up: a drive, a patrol or the AI's task 2, a target 3, an AREA watch, hunt or guard
4 and 5) or once the train is lost or dropped (2, 6: another army took it - lost); until 2026-11-21 it took
only states 2 and 6, and a train with an AREA watch, a hunt or a guard is in 4 or 5 for as long as it lives,
so such a train was never adopted: its loss was not watched and it was never built again. A train built
again for one of those orders first drives to the lost one's own place (`order_home`, which `OrderTry` fills
only while it is 0, so the first train's place stays) and watches, hunts or guards from there: `ORDER n task
t train 1 watches x,y radius r chase c from its place x,y (built again at x,y, drives there)`. Checked in play on POLIGON copies saved by the editor (each train's every car destroyed by a
test script at 20 s, REBUILT ONCE 10 s): the enemy's 25,10,25 watching 500 px from 1437,1375 - with the old
runner adopted only when it was lost (`watched: 0`) and never built again in 280 s; with the new one adopted
at 0 s, lost at 20 s, queued at 30 s, built at 167 s in the enemy factory and back on 1423,1375 at 230 s,
watching; a neutral 20,5,20 guarding 1810,1000 (the player's factory made the neutrals') and the same train
hunting from 1810,1128 - the old runner never again, the new one ordered at 30 s, built at 134 s, at its
point 1810,1005 and its place 1802,1080 at 170-175 s; the enemy's drive (state 2) the same with both runners
(lost 20 s, built 167 s, at its goal 1139,1375 from 225 s). Read back, such an entry gives its order its REBUILT value
(`read_orders`) and is left out of the factory's plan. The train built again gets the IT FIRES of the one it replaces (since v0.2.12, GitHub issue #13): `PlanAdopt` keeps whether a car of the map's train fires on its own (`plan_fires`, behave bit 1, `PlanFires`), and `PlanMade` sets or clears the bit on every car of the new one (`PlanFire`) - a factory gives its army's usual one, so a pirate train set to ONLY WHEN TOLD came back firing on its own. Checked on a POLIGON copy (a pirate train 10,20 with its gun ONLY WHEN TOLD, REBUILT ONCE 5 s, the pirates' factory; its locomotive destroyed at 15 s): the old runner's new gun had behave 1, the new runner's 0. The player's factory and trains are the player's:
none of it is offered for army 0. A pirate train needed a locomotive 10: a VID 5 of army 3 is the authors'
reinforcement marker, which `ReinforcementInit` takes away at the start (seen: `ORDER 1 ... train 0`) - since
v0.2.12 only on a map with reinforcement waves (`PLAYER_REINFORCEMENT` > 0; `game/maps/script/reinforcement.lgc.patch`
keeps it on a custom map without them when it stands on the map - the rule: x and y of the engine, the ground
under it, within `MapSizeX()` x `MapSizeY()`; one beyond the edge, as the missions' marks are, goes all the same,
`BUGS.md` F12), and only the first one `FirstUnit` finds. The editor follows it (`reinforcement_marker`,
`on_map_xy` with the grid's size): on such a map the first such engine beyond the edge is named the marker,
IN PLAY: TAKEN AWAY AT THE START (OUTSIDE THE MAP), with its note `MsgPirateEngineOutside` when it is placed,
dragged or given army 3 there (the editor's view stops at the map's edge, so in practice such an engine comes
from a mission's copy); when a level is held LocoFix.log names the marker and what it is in play. `FirstUnit` walks the
unit list of `g_objectMap` from its end, which a loaded level fills in the map file's order, and `Level_Save`
writes each layer's list from its end (0x41e486), so in a map the editor saved the one taken is the first
one in its layer's list - the first the editor's `for_each_object` meets, which `is_marker` names alone (the
status line's note when it is placed or given army 3, its panel, CONVERT's point). A map loaded and saved
again has that order turned round: with two or more such engines another one may be the marker after the
next load and save, and the editor names the one of the map as it stands. Checked on POLIGON copies with
`PROBE` logs: without waves the engine stays (before the patch it was gone, its point kept); with one wave
of a map made by `map_tool place` the last in the file went and the reinforcement arrived there; the same
map loaded in the editor - the other engine named the marker, the first one a pirate train - and saved: in
play that one went and the others stayed.
Checked in play on a POLIGON copy (the player's factory switched to the pirates, a pirate train 10,20 with
a drive and ALWAYS, 10 s; the enemy's patrolling train 20,5,20 with ONCE, 10 s; a test script destroying
the locomotives of armies 1 and 3 at 25 s and 90 s): both adopted as wave 1 at 1 s, both lost at 25 s, the
pirates' factory ordered 10,20 at 35 s and made it at 131 s (it then stopped at a red post's line on its
way east - the one-way rule), the enemy's queued at 35 s and taken by the AI at 95 s (the AI's own linker
for the mortar train's wagons held the factory until then).
**The AI's knobs** (the enemy factory's AI... screen, `AiKnobs`, written as the defines and arrays
the scripts read): `LIMIT_ATACKERS` (the plan waits while that many attack), `start_trains_renew`
and `RENEW_TIMEOUT` (a lost start train is built again, before the plan, after that many s +- half
at random), `start_trains_random_task` (a start train's raid every n s +- 15), `FIRSTATTACKTIME` (no
raids before it, x2 on easy, x1.5 on normal), `FIRSTCONTACTTIME` (how long a raid lasts),
`SetCleverEnemyAttack` (`CLEVER_ATTACK`, set by the runner after main's own call; the engine's
attack with the weapons' range) and `STG_DESTROY_TIME` (read by no script but mission 2's own code,
shown as such). The start trains' renew and raids are one value for all of them in the panel, "each
its own" while they differ. Test mode: `F<x>:<y>` opens the panel of the factory at a map point.
Checked in play: the table in `arrivals-tests` of the session notes (each army's entries, the cars
planned and made). The WHILE conditions, every one of the list: POLIGON with five neutral and eight
pirate factories off the track, an entry of WHILE each condition at our factory, a neutral or a
pirate one (14 entries, most with `first` inside a time the condition did not hold), an arrival of the
neutrals while the enemy has fewer than 2 trains and one of the pirates while the player has no
train, and a script that took the conditions' states away one by one (our train at 75 s, money 1000
at 50 s, an enemy train at 90 s, our factory at 105 s, the neutral factories at 135 s, an attacker
coming at 100 s) and logged `MissionIf` of each every second: each entry was ordered in the second its
condition began to hold (no train 75 s, fewer than 3 units 25 s, no factory 105 s, a neutral factory
25 s, the enemy fewer than 2 trains 90 s, fewer than 1 attacker 143 s, 500 money 50 s, no train and
a neutral factory 75 s, no locomotive and a factory of its own 75 s, fewer than 2 trains 75 s after
a time with 2), the two arrivals at 75 and 90 s, and the three entries whose condition had ended
before their `first` never. An enemy entry repeated without end over a long game: POLIGON with one
entry (5,20 attacking, at 20 s, every 60 s, no end), its attackers destroyed by the test's script
once a minute so `LIMIT_ATACKERS` did not hold it, the game sped up to x2 with + (the engine's key:
x2 at most, x5 in the developer mode) - in 13.5 minutes of the game seven waves, each queued 60 s
after the one before was built (20, 150, 281, 411, 541, 671, 801 s) and built about 70 s later as
5,20, the queue's one place used again each time. The difficulty: every train a panel writes has a
type of a template or the map's own, never one ending in 5, so easy and hard build what the panel
shows - checked on POLIGON with an enemy entry LIGHT GUN, one of its own cars 5,25,20, one of our
factory 5,20 and an enemy arrival 5,80, played on easy and on hard (a test copy of `GetDifficulty`
reading another registry value): the same cars as planned on both, while the authors' `ATT_DIFF_GUN`
made 5,20 on easy and 10,20,20,20 on hard. The variant that goes: a campaign script's entry of such
a type (the authors' "BY DIFFICULTY" trains) is read as the normal difficulty's train and written
so, the same on every difficulty.
