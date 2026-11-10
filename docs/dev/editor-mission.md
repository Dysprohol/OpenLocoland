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
  `// title:` line keeps the map's title for the custom maps page,
- `// <editor main>` ... `// </editor main>` in `main()`'s `iff(1)` block: the `SetPushLine`
  calls.

File > Mission settings... (or the gear of the bar's strip) shows them in a Windows dialog -
the title on the custom maps page, the briefing's header, money at the start (`SetMoney(0, MONEY)`
of `initdefault.lgc`), the time of day at the start (`START_DAYNIGHT` 1-4: day, evening, night,
morning; `daynight.lgc` then goes on, 5 minutes each), the weather (`RAIN_TYPE`: rain, snow, clouds,
storm clouds, meteorites - `environmenttact.lgc` makes the last three) and how often (`RAIN`: 0
never, 1 now and then - it comes and goes, about half a minute each -, 2 all the time;
`daynight.lgc`'s own comment has 1 and 2 the other way round, its code does this), lightning
(`NUKE`: a flash and its sound at random in a clear sky, with rain only while it rains, never with
the other weathers), the level's music - none, one file or a list of
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
dark outline; the mouse over a line names it in a status line, in any tool: its begin and end and
what it does (push_flag 1: the trains on it are pushed the way of the arrows and none may go
against them; 0: none may go against them, none is pushed). A click on an end of a line selects it - its bar or arrow, in the
Select tool too, where a line is an object like the others: it turns white, the status line names it
(its begin, end, what it does and whose it is - a semaphore's, a factory's or repair point's, the
level's entry or exit) while the mouse is over no other, and the right button opens its panel in the
game's style (DIRECTION: TURN ROUND (R), ACTION (T): PUSHES TRAINS / ONE WAY ONLY, DELETE (DEL)): T switches its flag
(a new line has 1: the level entry, factory exits, repair point legs; the level exit has 0), R (or E,
the wheel) turns it round, Del deletes it - all undoable - and S puts its semaphores
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
player pays n, the campaign's "no train and a neutral factory") that either gates the time ("only
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
takes `ARR_LENGTH` cars). The panel's last line names a limit of the game's AI only once it is
reached: more than 10 patrols or posts (the rest attack), more than 16 enemy trains (they wait for a
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
at a point of its own - so a map made in the editor, with none of them, never captured those two, and
`LDepoCapture` even takes once "the" factory of army 2 or 0, which on a map with the player's factory
can be that one. `CaptureTact` turns every neutral factory and repair point to the player when a train
of the player's comes within 100 px (`PlaySFX(25)`, the flash, `BuildingCapture` with its message, a
`CAPTURE` line in `error.log`). A repair point taking over the enemy's wagons brought by the player's
locomotive is the engine's own (`Building_UpdateWorkshop`), on any map. Checked in play: a neutral repair
point and a neutral factory beside the main line captured as trains passed them, and an enemy wagon in a
player's train taken over going through the player's repair point. Older editor maps get it with
CONVERT in the Arrivals panel.

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
(`reachable`, every link both ways but against a line, the red lights of `PASS_RED_LIGHTS` counted; a
reversal's room is not) and says it when the order is given and in the ORDERS row's tip ("319,1000 cannot
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
stays; and STARTS: AT ONCE / AFTER n S (`order_wait`, steps of 10 s) for any order: the train stands until
then. A train with a target is not rebuilt (the AI's queue has no such task). Checked in play on a POLIGON
copy: a neutral train 5,20 stood until 20 s, drove to the enemy fort and destroyed the gun it was told to
(hp 40, 30, 15, 5, gone at 55 s, `ORDER n target gone`), then stood; the enemy's patrolling train told to
attack the player's factory drove round to the factory's spur and fired at it. Per army, beside the red
lights and the AI's knobs: the enemy factory's AI screen has NEUTRAL TRAINS: FIRES AT THEM / ONLY WHEN HIT
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
own place, where it stands when it gets the order (after a pushing line, or where a rebuilt train is
made): `order_home_x/y`, two arrays of the runner the save writes (zeros, filled by `OrderTry`; the hunt's
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
WITHOUT END, EVERY m s (steps of 10 s); THEN and the POINT(S) (PICK; a patrol's two ends one after
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
the map ("ENTRY n"), the chosen arrival's points too, joined to its entry; a deletion of rails that
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
factory was selected (a factory already selected keeps showing its old list); the keys stay the game's
own per unit (a list without the airship cart leaves F8 out).

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
factory and not while a train stands within 250 px of it - at its door or on its spur (seen in play: two entries started together
mixed their cars, 5,20 and 5,25 came as 5,25,5,20, and the cars of the next train joined one that
stayed at the door). Every
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
to where the lost one stood when it patrolled. Read back, such an entry gives its order its REBUILT value
(`read_orders`) and is left out of the factory's plan. The player's factory and trains are the player's:
none of it is offered for army 0. A pirate train needs a locomotive 10: a VID 5 of army 3 is the authors'
reinforcement marker, which `ReinforcementInit` takes away at the start (seen: `ORDER 1 ... train 0`).
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
