# Experiment / check (rig -Job script, under any agent): runs a second agent (this worktree's
# agent.ps1, -NoShutdown) inside the box with a private io folder (C:\work\io2-<scenario>) and feeds it
# jobs, to check the parallel agent without touching the box's own queue. The display cannot be
# changed from here (only the host can), so every job here names no display.
#   -Scenarios three,hang,exclusive,rr,missions   (comma list; each starts its own nested agent)
#   three      three builds at once (build\game, game2, game3), each its own language and frame limit
#   hang       a job that hangs (timeout 0.5 min) beside a normal one: only the hung one is killed
#   exclusive  a job, then an -Exclusive one, then another: the exclusive one waits and holds the box
#   rr         one caller queues 3 jobs, another 1 after them, one job at a time: the second caller's job goes second
#   missions   the 20 missions (4 lanes) alone, then again beside two other game jobs
param([string]$Dir, [string]$Game, [string]$Scenarios = "three,hang,exclusive,rr")
$root = Split-Path (Split-Path (Split-Path $PSScriptRoot))   # the worktree as the guest sees it
$tools = "$root\tools"; $build = "$root\build"
function L($s) { "$(Get-Date -Format 'HH:mm:ss.fff') $s" }
$script:agentProc = $null; $script:io2 = $null
function Start-Agent([string]$name, [hashtable]$cfg) {
  $script:io2 = "C:\work\io2-$name"
  if (Test-Path $io2) { Remove-Item $io2 -Recurse -Force }
  New-Item -ItemType Directory -Force "$io2\agent", "$io2\queue", "$io2\jobs" | Out-Null
  $c = @{ IdleMinutes = 30; ScreenSeconds = 5; MaxJobs = 4; MaxGames = 6 }; foreach ($k in $cfg.Keys) { $c[$k] = $cfg[$k] }
  $c | ConvertTo-Json | Set-Content "$io2\agent\config.json" -Encoding utf8
  $script:agentProc = Start-Process powershell -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$PSScriptRoot\agent.ps1`"", "-Io", $io2, "-NoShutdown") -PassThru -WindowStyle Minimized
  $null = $agentProc.Handle
  $deadline = (Get-Date).AddSeconds(60); while (-not (Test-Path "$io2\agent\state.json") -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 300 }
  L "[$name] nested agent up: $((Get-Content "$io2\agent\agent.log") -join ' | ')"
}
$seq = 0
function Q([string]$name, [string]$caller, [hashtable]$spec) {
  $script:seq++
  $id = "{0:yyyyMMdd-HHmmss}-{1:D2}-{2}" -f (Get-Date), $seq, $name
  $s = [ordered]@{ Job = "game"; TimeoutMinutes = 5; Tools = $tools; Proto = 2; Caller = $caller }
  foreach ($k in $spec.Keys) { $s[$k] = $spec[$k] }
  ($s | ConvertTo-Json -Depth 5) | Set-Content "$io2\queue\$id.tmp" -Encoding utf8; Move-Item "$io2\queue\$id.tmp" "$io2\queue\$id.json"
  return $id
}
function Wait-All([string[]]$ids, [int]$max = 600) {
  $deadline = (Get-Date).AddSeconds($max); $last = ""
  while ((Get-Date) -lt $deadline) {
    $left = @($ids | Where-Object { -not (Test-Path "$io2\jobs\$_\done.json") }); if (-not $left.Count) { return $true }
    $st = try { Get-Content "$io2\agent\state.json" -Raw | ConvertFrom-Json } catch { $null }
    if ($st) { $now = "running: $((@($st.running) | ForEach-Object { "$($_.id)[slot $($_.slot)]" }) -join ', '); waiting: $((@($st.queue) | ForEach-Object { "$($_.id): $($_.why)" }) -join ' | ')"; if ($now -ne $last) { L "  $now"; $last = $now } }
    Start-Sleep -Milliseconds 700
  }
  L "  NOT all done in $max s"; return $false
}
function Show([string[]]$ids) {
  foreach ($id in $ids) {
    $d = "$io2\jobs\$id"
    $done = try { Get-Content "$d\done.json" -Raw | ConvertFrom-Json } catch { $null }
    $run = try { Get-Content "$d\run.json" -Raw | ConvertFrom-Json } catch { $null }
    $start = if ($run) { ([datetime]$run.started).ToString("HH:mm:ss") } else { "-" }
    $end = if ($run -and $done) { ([datetime]$run.started).AddSeconds($done.seconds).ToString("HH:mm:ss") } else { "-" }
    $lang = (Get-Content "$d\game-logs\LocoFix.log" -ErrorAction SilentlyContinue | Select-String "^.*language: \w+" | Select-Object -First 1).Line
    $fl = (Get-Content "$d\game-logs\LocoFix.log" -ErrorAction SilentlyContinue | Select-String "frame limit|FrameLimit|frames per second" | Select-Object -First 1).Line
    $key = (Get-Content "$d\test_game.txt" -ErrorAction SilentlyContinue | Select-String "Test key:").Line
    $closed = [bool](Get-Content "$d\test_game.txt" -ErrorAction SilentlyContinue | Select-String "Closed normally")
    L "  $id : exit $($done.exit) $($done.killed) | slot $($run.slot) $($run.user) | $start-$end ($($done.seconds) s, waited $($run.waited) s) | alongside $(@($done.alongside) -join ',') | peak games $($done.peakGames) | display $($run.display) changed '$($done.displayChanged)' | closed normally $closed | $key | $lang | $fl"
  }
}
function Stop-Agent {
  New-Item -ItemType File -Force "$io2\stop" | Out-Null
  if (-not $agentProc.WaitForExit(90000)) { L "  nested agent did not end - killing it"; & taskkill /F /T /PID $agentProc.Id | Out-Null }
  $name = Split-Path $io2 -Leaf
  robocopy $io2 (Join-Path $Dir $name) /E /NFL /NDL /NJH /NJS /NP /XF *.png | Out-Null
  Copy-Item "$io2\jobs\*\*.png" -Destination (New-Item -ItemType Directory -Force (Join-Path $Dir "$name-shots")) -ErrorAction SilentlyContinue
  L "  agent log:"; Get-Content "$io2\agent\agent.log" | ForEach-Object { "    $_" }
}
$l01 = @{ Map = "maps\l01\l01.map"; WaitFor = "test map: loaded"; ClickOk = 1; SetFull = 0; LastShotOnly = $true }
function GameArgs([hashtable]$more) { $h = @{}; foreach ($k in $l01.Keys) { $h[$k] = $l01[$k] }; foreach ($k in $more.Keys) { $h[$k] = $more[$k] }; $h }

foreach ($sc in ($Scenarios -split ",")) {
  switch ($sc) {
    "three" {
      Start-Agent "three" @{}
      $ids = @(
        (Q "pl" "build-a" @{ Game = "$build\game"; Args = (GameArgs @{ Language = "pl"; FrameLimit = 60; Steps = "w@8" }) }),
        (Q "en" "build-b" @{ Game = "$build\game2"; Args = (GameArgs @{ Language = "en"; FrameLimit = 30; Steps = "w@8" }) }),
        (Q "de" "build-c" @{ Game = "$build\game3"; Args = (GameArgs @{ Language = "de"; FrameLimit = 0; Steps = "w@8" }) }))
      [void](Wait-All $ids 400); Show $ids; Stop-Agent
    }
    "hang" {
      Start-Agent "hang" @{}
      $ids = @((Q "hung" "caller-a" @{ Game = "$build\game"; TimeoutMinutes = 0.5; Args = (GameArgs @{ WaitFor = "this never appears in a log"; LogWait = 600 }) }),
               (Q "normal" "caller-b" @{ Game = "$build\game2"; Args = (GameArgs @{ Language = "en"; Steps = "w@25" }) }))
      [void](Wait-All $ids 300); Show $ids
      L "  Locoland processes left: $(@(Get-Process Locoland -ErrorAction SilentlyContinue).Count)"
      Stop-Agent
    }
    "exclusive" {
      Start-Agent "exclusive" @{}
      $a = Q "first" "caller-a" @{ Game = "$build\game"; Args = (GameArgs @{ Steps = "w@15" }) }
      Start-Sleep 3
      $b = Q "alone" "caller-b" @{ Game = "$build\game2"; Exclusive = $true; Args = (GameArgs @{ Steps = "w@10" }) }
      $c = Q "after" "caller-c" @{ Game = "$build\game3"; Args = (GameArgs @{ Steps = "w@5" }) }
      [void](Wait-All @($a, $b, $c) 400); Show @($a, $b, $c); Stop-Agent
    }
    "rr" {
      Start-Agent "rr" @{ MaxJobs = 1 }
      $ids = @((Q "a1" "caller-a" @{ Game = "$build\game"; Args = $l01 }), (Q "a2" "caller-a" @{ Game = "$build\game"; Args = $l01 }),
               (Q "a3" "caller-a" @{ Game = "$build\game"; Args = $l01 }), (Q "b1" "caller-b" @{ Game = "$build\game2"; Args = $l01 }))
      [void](Wait-All $ids 400); Show $ids
      L "  order started: $((($ids | ForEach-Object { $r = Get-Content "$io2\jobs\$_\run.json" -Raw | ConvertFrom-Json; [pscustomobject]@{ id = $_; t = [datetime]$r.started } }) | Sort-Object t | ForEach-Object { $_.id.Split('-')[-1] }) -join ', ')"
      Stop-Agent
    }
    "stress" {
      # the 20 missions as 2, then 3 jobs of 4 lanes at once (8, then 12 games): how long, and do they pass
      Start-Agent "stress" @{ MaxJobs = 6; MaxGames = 12 }
      foreach ($k in 2, 3) {
        $ids = @(for ($i = 1; $i -le $k; $i++) { Q "x$k-$i" "caller-$i" @{ Job = "missions"; Game = "$build\game$(if ($i -gt 1) { $i })"; Parallel = 4; TimeoutMinutes = 15 } })
        [void](Wait-All $ids 900); Show $ids
        foreach ($m in $ids) { L "  $m : $((Get-Content "$io2\jobs\$m\test_missions.txt" | Select-Object -Last 1)) | $((Get-Content "$io2\jobs\$m\memory.txt" | ForEach-Object { if ($_ -match 'used (\d+) MB') { [int]$Matches[1] } } | Measure-Object -Maximum).Maximum) MB guest peak" }
      }
      Stop-Agent
    }
    "missions" {
      Start-Agent "missions" @{}
      $m1 = Q "alone" "caller-m" @{ Job = "missions"; Game = "$build\game"; Parallel = 4; TimeoutMinutes = 15 }
      [void](Wait-All @($m1) 900); Show @($m1)
      $m2 = Q "loaded" "caller-m" @{ Job = "missions"; Game = "$build\game"; Parallel = 4; TimeoutMinutes = 15 }
      $g1 = Q "load1" "caller-x" @{ Game = "$build\game2"; TimeoutMinutes = 10; Args = (GameArgs @{ Map = "maps\l08\l08.map"; Steps = "w@40,w@40" }) }
      $g2 = Q "load2" "caller-y" @{ Game = "$build\game3"; TimeoutMinutes = 10; Args = (GameArgs @{ Map = "maps\l15\l15.map"; Steps = "w@40,w@40" }) }
      [void](Wait-All @($m2, $g1, $g2) 900); Show @($m2, $g1, $g2)
      foreach ($m in $m1, $m2) { L "  $m report:"; Get-Content "$io2\jobs\$m\missions\report.txt" | ForEach-Object { "    $_" }; L "  $((Get-Content "$io2\jobs\$m\test_missions.txt" | Select-Object -Last 1))" }
      Stop-Agent
    }
  }
}
L "C: free $([int]((Get-PSDrive C).Free / 1GB)) GB; game copies: $((Get-ChildItem C:\work\games -Directory | ForEach-Object { $_.Name }) -join ', ')"
exit 0
