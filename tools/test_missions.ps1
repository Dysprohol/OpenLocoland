# Mission smoke test: for each mission starts the game in a window with the mission's map
# as the test map (LocoFix --test-map: the game loads the map at once, without the intro and
# the menu), waits until the map and its script are loaded, takes a screenshot and collects
# the mission script compile result and errors from error.log. Nothing is clicked, so a
# mission that the player has not unlocked is tested just the same.
# A mission passes when it loaded, its script compiled without errors, the game logged no
# "EXCEPTION!" (the game catches a crash, logs that and quits) and closed normally; the
# script exits with 1 when a mission fails.
#
#   .\tools\test_missions.ps1 [-Missions 1,2,3] [-Quick] [-Wait 30] [-Results build\tests] [-Game <folder>]
#
# -Quick    only missions 1, 2 and 8 - one of each map size (1024x768, 2048x1536,
#           3072x2304); about a minute, for everyday checks. The full 20 before bigger merges.
# -Wait     at most this many seconds for a mission to load
# -Game     the assembled game to test (default ..\Locoland next to the repo), e.g. a copy made with
#           build.py --target, so tests of different changes don't share one folder
param([int[]]$Missions = 1..20, [switch]$Quick, [int]$Wait = 30, [string]$Results = "", [string]$Game = (Join-Path (Split-Path (Split-Path $PSScriptRoot)) "Locoland"))

if ($Quick) { $Missions = 1, 2, 8 }
$repo = Split-Path $PSScriptRoot
if (-not $Results) { $Results = Join-Path $repo "build\tests" }
New-Item -ItemType Directory -Force $Results | Out-Null
$game = $Game

$report = @()
foreach ($m in $Missions) {
  $nr = '{0:D2}' -f $m
  $shot = Join-Path $Results "mission_$nr.png"
  # The mission is in when its map is loaded and, after that, its script compiled. LocoFix
  # logs whether the map really loaded (World->levelFile), the game's log has the map and
  # the script compile result.
  $map = "maps\l$nr\l$nr.map"
  $started = Get-Date
  $loadedPattern = "maps\\l$nr\\l$nr\.map.*LoadScript::ByteCode"
  $out = & (Join-Path $PSScriptRoot "test_game.ps1") -Wait 15 -Exe (Join-Path $game "Locoland.exe") -ClickOk 1 -SetFull 0 `
    -Map $map -WaitFor $loadedPattern -LogWait $Wait -Shot $shot -LastShotOnly 2>&1 | Out-String
  # The game writes error<date> <time>.log and renames it to error.log on a normal exit; a
  # killed game leaves the dated one and error.log is from an earlier run. So the log of this
  # run is the newest one written since it started (none: the game did not get that far).
  $logFile = Get-ChildItem (Join-Path $game "error*.log") -ErrorAction SilentlyContinue |
    Where-Object { $_.LastWriteTime -ge $started.AddSeconds(-2) } | Sort-Object LastWriteTime | Select-Object -Last 1
  $log = if ($logFile) { Get-Content $logFile.FullName } else { @() }
  $script = ($log | Select-String "LoadScript::ByteCode" | Select-Object -Last 1).Line
  $scriptErrors = @($log | Select-String "LOGIC|Undeclared|redefinition").Count
  $loaded = [bool]($log | Select-String "maps\\l$nr\\l$nr.map")
  # LocoFix.log: "test map: loaded <map>" once the level load is over and World->levelFile
  # is the test map.
  $fixLog = Get-Content (Join-Path $game "LocoFix.log") -ErrorAction SilentlyContinue
  $confirmed = [bool]($fixLog | Select-String "test map: loaded maps\\l$nr\\l$nr\.map")
  $closed = $out -match 'Closed normally'
  $exceptions = @($log | Select-String "EXCEPTION!").Count
  $passed = $loaded -and $confirmed -and $scriptErrors -eq 0 -and $exceptions -eq 0 -and $closed
  $report += [pscustomobject]@{
    Mission = $m; Passed = $passed; Loaded = $loaded; LoadConfirmed = $confirmed; ScriptErrors = $scriptErrors
    Exceptions = $exceptions; ClosedNormally = $closed; Script = $script
  }
  if ($logFile) { Copy-Item $logFile.FullName (Join-Path $Results "mission_$nr.log") -Force -ErrorAction SilentlyContinue }
  "mission ${m}: passed=$passed loaded=$loaded confirmed=$confirmed script_errors=$scriptErrors exceptions=$exceptions closed=$closed"
}
$report | Format-Table -AutoSize | Out-String -Width 200 | Set-Content (Join-Path $Results "report.txt") -Encoding utf8
$report | Format-Table -AutoSize
$failed = @($report | Where-Object { -not $_.Passed })
if ($failed.Count) {
  "FAILED: mission(s) $(($failed | ForEach-Object { $_.Mission }) -join ', ')"
  exit 1
}
"all $($report.Count) mission(s) passed"
