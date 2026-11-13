# End-to-end checks of the parallel rig (host side), against the running box (its agent must be
# protocol 2 - started by this rig.ps1). Each scenario starts several rig.ps1 calls at once as
# background processes (no window), waits for them, and prints what each job ran under (run.json,
# done.json) so overlaps and displays can be read off. Results: build\rig\check-<scenario>-<name>.
#   .\tools\rig\check_parallel.ps1 -Scenarios same,differ,default,exclusive,fresh,old,missions
#   same      three builds (build\game, game2, game3), each its own language, all asking 1366x768: together
#   differ    one asks 1366x768, one 1920x1080: never together
#   default   one asks -Scale 150 (a host monitor must have it), then one names no display: the default
#   exclusive a job, an -Exclusive one and another: -Status while the exclusive one waits and runs
#   fresh     a job runs, a -Fresh probe and a job behind it: the box is restarted once empty, the queue kept
#   old       the main checkout's rig.ps1 (an older host script) queues a job into this agent
#   missions  the 20 missions alone, then again beside two other jobs (compare_runs.py on the two)
param([string]$Scenarios = "same,differ,default,exclusive", [string]$OldRig = "")
$rig = Join-Path $PSScriptRoot "rig.ps1"
$repo = Split-Path (Split-Path $PSScriptRoot)
$b = Join-Path $repo "build"
function Say($s) { "[check $(Get-Date -Format 'HH:mm:ss')] $s" }
function Launch([string]$name, [string]$argLine, [string]$script = $rig) {
  $out = Join-Path $b "rig\check-$name"
  if (Test-Path $out) { Remove-Item $out -Recurse -Force }
  $log = Join-Path $b "rig\check-$name.txt"
  $cmd = "& '$script' $argLine -Out '$out' *> '$log'"
  $p = Start-Process powershell -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-EncodedCommand", [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($cmd))) -WindowStyle Hidden -PassThru
  $null = $p.Handle
  return @{ Name = $name; P = $p; Out = $out; Log = $log }
}
function Wait-Report($runs) {
  foreach ($r in $runs) { $r.P.WaitForExit() }
  foreach ($r in $runs) {
    $run = try { Get-Content (Join-Path $r.Out "run.json") -Raw -ErrorAction Stop | ConvertFrom-Json } catch { $null }
    $done = try { Get-Content (Join-Path $r.Out "done.json") -Raw -ErrorAction Stop | ConvertFrom-Json } catch { $null }
    $st = if ($run) { ([datetime]$run.started).ToString("HH:mm:ss") } else { "-" }
    $en = if ($run -and $done) { ([datetime]$run.started).AddSeconds($done.seconds).ToString("HH:mm:ss") } else { "-" }
    $lang = (Get-Content (Join-Path $r.Out "game-logs\LocoFix.log") -ErrorAction SilentlyContinue | Select-String "language: \w+" | Select-Object -First 1).Line
    $scr = (Get-Content (Join-Path $r.Out "test_game.txt") -ErrorAction SilentlyContinue | Select-String "^Screen:").Line
    $closed = [bool](Get-Content (Join-Path $r.Out "test_game.txt") -ErrorAction SilentlyContinue | Select-String "Closed normally")
    Say "$($r.Name): exit $($r.P.ExitCode) | $st-$en | slot $($run.slot) | display $($run.display) (asked $($run.asked)) changed '$($done.displayChanged)' | beside $(@($done.alongside).Count) | waited $($run.waited) s | $scr | closed normally $closed | $lang"
    Get-Content $r.Log -ErrorAction SilentlyContinue | Where-Object { $_ -match "waiting|running|display for|fresh|restart|older|queued again|went away|THE DISPLAY|ran " } | Select-Object -First 12 | ForEach-Object { "      $_" }
  }
}
$l01 = '@{ Map = "maps\l01\l01.map"; WaitFor = "test map: loaded"; ClickOk = 1; SetFull = 0; LastShotOnly = $true'
foreach ($sc in ($Scenarios -split ",")) {
  Say "---- $sc"
  switch ($sc) {
    "same" {
      $runs = @((Launch "same-pl" "-Job game -Game '$b\game' -Resolution 1366x768 -TestArgs $l01; Language = 'pl'; Steps = 'w@8' }"),
                (Launch "same-en" "-Job game -Game '$b\game2' -Resolution 1366x768 -TestArgs $l01; Language = 'en'; Steps = 'w@8' }"),
                (Launch "same-de" "-Job game -Game '$b\game3' -Resolution 1366x768 -TestArgs $l01; Language = 'de'; Steps = 'w@8' }"))
      Wait-Report $runs
    }
    "differ" {
      $runs = @((Launch "differ-1366" "-Job game -Game '$b\game' -Resolution 1366x768 -TestArgs $l01 }"),
                (Launch "differ-1920" "-Job game -Game '$b\game2' -Resolution 1920x1080 -TestArgs $l01 }"))
      Wait-Report $runs
    }
    "default" {
      Wait-Report @(Launch "default-150" "-Job game -Game '$b\game' -Scale 150 -TestArgs $l01 }")
      Wait-Report @(Launch "default-none" "-Job game -Game '$b\game' -TestArgs $l01 }")
    }
    "exclusive" {
      $a = Launch "excl-first" "-Job game -Game '$b\game' -TestArgs $l01; Steps = 'w@20' }"
      Start-Sleep 8
      $x = Launch "excl-alone" "-Job game -Game '$b\game2' -Exclusive -TestArgs $l01 }"
      Start-Sleep 3
      $c = Launch "excl-after" "-Job game -Game '$b\game3' -TestArgs $l01 }"
      Start-Sleep 4; & $rig -Status
      Start-Sleep 25; & $rig -Status
      Wait-Report @($a, $x, $c)
    }
    "hang" {
      $runs = @((Launch "hang-hung" "-Job game -Game '$b\game' -Timeout 0.5 -TestArgs @{ Map = 'maps\l01\l01.map'; WaitFor = 'this never appears in a log'; LogWait = 600; ClickOk = 1; SetFull = 0 }"),
                (Launch "hang-normal" "-Job game -Game '$b\game2' -TestArgs $l01; Language = 'en'; Steps = 'w@25' }"))
      Wait-Report $runs
    }
    "fresh" {
      $a = Launch "fresh-first" "-Job game -Game '$b\game' -TestArgs $l01; Steps = 'w@15' }"
      Start-Sleep 8
      $f = Launch "fresh-probe" "-Job probe -Fresh"
      Start-Sleep 3
      $c = Launch "fresh-after" "-Job game -Game '$b\game2' -TestArgs $l01 }"
      Start-Sleep 5; & $rig -Status
      Wait-Report @($a, $f, $c)
    }
    "old" {
      if (-not $OldRig) { $OldRig = Join-Path (($repo -split '\\\.claude\\')[0]) "tools\rig\rig.ps1" }
      Say "older host script: $OldRig"
      $runs = @((Launch "old-game" "-Job game -Game '$b\game' -TestArgs $l01 }" $OldRig),
                (Launch "new-beside" "-Job game -Game '$b\game2' -TestArgs $l01; Language = 'en' }"))
      Wait-Report $runs
      Wait-Report @(Launch "old-res" "-Job game -Game '$b\game3' -Resolution 1366x768 -TestArgs $l01 }" $OldRig)
    }
    "missions" {
      Wait-Report @(Launch "missions-alone" "-Job missions -Game '$b\game' -Timeout 20")
      $first = Launch "missions-loaded" "-Job missions -Game '$b\game' -Timeout 20"
      Start-Sleep 6   # queued first: it starts before the two jobs that load the box beside it
      $runs = @($first,
                (Launch "missions-load1" "-Job game -Game '$b\game2' -Timeout 10 -TestArgs @{ Map = 'maps\l08\l08.map'; WaitFor = 'test map: loaded'; ClickOk = 1; SetFull = 0; LastShotOnly = `$true; Steps = 'w@40,w@40' }"),
                (Launch "missions-load2" "-Job game -Game '$b\game3' -Timeout 10 -TestArgs @{ Map = 'maps\l15\l15.map'; WaitFor = 'test map: loaded'; ClickOk = 1; SetFull = 0; LastShotOnly = `$true; Steps = 'w@40,w@40' }"))
      Wait-Report $runs
    }
  }
}
