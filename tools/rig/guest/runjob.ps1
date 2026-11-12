# One job of the rig, INSIDE the sandbox (started by agent.ps1 in a process of its own, so a hung
# job can be killed). Reads <Dir>\job.json, writes results into <Dir>; exit code = the job's.
#
# job.json: Job      probe | game | missions | setup | script
#           Game     the built game folder as the guest sees it (C:\host\... - read-only), copied to
#                    C:\work\games\<n> before the run (robocopy /MIR: a later job with the same game
#                    copies only what changed and starts from the built state again)
#           Tools    the tools folder whose test_game.ps1 / test_missions.ps1 run (C:\host\...)
#           Args     parameters of test_game.ps1 / test_missions.ps1 (an object: name -> value)
#           Scale    guest display scale in %, Resolution "WxH": made by rig.ps1, checked here
#           Setup    the Setup.exe (guest path), SetupArgs, Steam (guest path of the original)
#           Script   (script job) a .ps1 to run, ScriptArgs its parameters
#           DgVoodoo values set in the guest copy's dgVoodoo.conf, e.g. OutputAPI = d3d11warp (software rendering)
#           KeepRegistry  leave HKCU\SOFTWARE\Gromada as an earlier job left it (default: removed)
#           Registry values set before the run under HKCU\SOFTWARE\Gromada\<RegistryKey> (default
#                    Locoland; a number is a DWORD, the rest strings), e.g. FrameLimit 0, ScreenX 1920
param([string]$Dir, [string]$Fresh = "0")
$ErrorActionPreference = "Continue"
$job = Get-Content (Join-Path $Dir "job.json") -Raw | ConvertFrom-Json
$log = Join-Path $Dir "job.log"
function Log($s) { $line = "$(Get-Date -Format 'HH:mm:ss.fff') $s"; Add-Content -Path $log -Value $line -Encoding utf8; $line }
function ArgsOf($o) { $h = @{}; if ($o) { foreach ($k in $o.PSObject.Properties) { $h[$k.Name] = $k.Value } }; $h }
$code = 0
try {
  Log "job $($job.Job) in $Dir (fresh box: $Fresh)"
  # a game or Setup an earlier job left behind (killed on its timeout, or a script that did not close it)
  $left = @(Get-Process Locoland, Setup -ErrorAction SilentlyContinue)
  if ($left.Count) { Log "ending $($left.Count) process(es) left by an earlier job: $(($left | ForEach-Object { "$($_.ProcessName) $($_.Id)" }) -join ', ')"; $left | Stop-Process -Force; Start-Sleep 1 }
  . (Join-Path $PSScriptRoot "display.ps1")
  # rig.ps1 makes the display from the host (window size, monitor) before it queues the job; here
  # it is only checked, so a job never runs on a display other than the one asked for.
  Log "display: $(Get-DisplayReport)"
  $scr = [GuestDisplay]::Screen()
  if ($job.Resolution -and -not $scr.StartsWith("$($job.Resolution)@")) { throw "the guest's screen is $scr, not the $($job.Resolution) asked for" }
  if ($job.Scale -and [int]($scr.Split("@")[1]) * 100 / 96 -ne [int]$job.Scale) { throw "the guest's scale is $scr, not the $($job.Scale) % asked for" }
  if (-not $job.KeepRegistry) { Remove-Item "HKCU:\SOFTWARE\Gromada" -Recurse -Force -ErrorAction SilentlyContinue }
  if ($job.Registry) {
    $key = "HKCU:\SOFTWARE\Gromada\" + $(if ($job.RegistryKey) { $job.RegistryKey } else { "Locoland" })
    if (-not (Test-Path $key)) { New-Item $key -Force | Out-Null }
    foreach ($v in $job.Registry.PSObject.Properties) {
      $type = if ($v.Value -is [int] -or $v.Value -is [long]) { "DWord" } else { "String" }
      New-ItemProperty $key -Name $v.Name -Value $v.Value -PropertyType $type -Force | Out-Null
    }
    Log "registry ${key}: $(($job.Registry.PSObject.Properties | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join ', ')"
  }
  $game = $null
  if ($job.Game) {
    $n = [BitConverter]::ToString((New-Object Security.Cryptography.SHA1Managed).ComputeHash([Text.Encoding]::UTF8.GetBytes($job.Game.ToLower()))).Replace("-", "").Substring(0, 10)
    $game = "C:\work\games\$n"
    $t = Get-Date
    robocopy $job.Game $game /MIR /NFL /NDL /NJH /NJS /NP /MT:16 | Out-Null
    Log "game $($job.Game) -> $game in $([int]((Get-Date) - $t).TotalSeconds) s (robocopy $LASTEXITCODE)"
    if (-not (Test-Path "$game\Locoland.exe")) { throw "no Locoland.exe in $($job.Game)" }
    if ($job.DgVoodoo) {
      # values of dgVoodoo.conf in the guest's copy (the next job's copy gets the built file back)
      $conf = Join-Path $game "dgVoodoo.conf"; $text = [IO.File]::ReadAllText($conf)
      foreach ($v in $job.DgVoodoo.PSObject.Properties) {
        $re = "(?m)^" + [regex]::Escape($v.Name) + "(\s*)=.*$"
        if (-not [regex]::IsMatch($text, $re)) { throw "dgVoodoo.conf has no setting $($v.Name)" }
        $text = [regex]::Replace($text, $re, { param($m) $v.Name + $m.Groups[1].Value + "= " + $v.Value })
      }
      [IO.File]::WriteAllText($conf, $text)
      Log "dgVoodoo.conf: $(($job.DgVoodoo.PSObject.Properties | ForEach-Object { "$($_.Name) = $($_.Value)" }) -join ', ')"
    }
  }
  $tools = if ($job.Tools) { $job.Tools } else { Split-Path (Split-Path $PSScriptRoot) }
  switch ($job.Job) {
    "probe" { & (Join-Path $PSScriptRoot "probe.ps1") -Out $Dir -Name "probe" | Out-Null }
    "game" {
      $p = ArgsOf $job.Args; $p["Exe"] = "$game\Locoland.exe"
      if (-not $p.ContainsKey("Shot")) { $p["Shot"] = "shot.png" }
      if (-not [IO.Path]::IsPathRooted($p["Shot"])) { $p["Shot"] = Join-Path $Dir $p["Shot"] }
      $t = Get-Date
      $text = & (Join-Path $tools "test_game.ps1") @p *>&1 | Out-String -Width 400
      $text | Set-Content (Join-Path $Dir "test_game.txt") -Encoding utf8
      # test_game.ps1 has no exit code of its own: the run is good when the game closed normally
      $code = if ($text -match "Closed normally") { 0 } else { 1 }
      Log "test_game.ps1 exit $code in $([int]((Get-Date) - $t).TotalSeconds) s"
    }
    "missions" {
      $p = ArgsOf $job.Args
      $t = Get-Date
      $text = & (Join-Path $tools "test_missions.ps1") -Game $game -Results (Join-Path $Dir "missions") @p *>&1 | Out-String -Width 400
      $text | Set-Content (Join-Path $Dir "test_missions.txt") -Encoding utf8
      $code = if ($text -match "all \d+ mission\(s\) passed") { 0 } else { 1 }
      Log "test_missions.ps1 exit $code in $([int]((Get-Date) - $t).TotalSeconds) s"
    }
    "setup" {
      & (Join-Path $PSScriptRoot "setup_job.ps1") -Job $job -Dir $Dir -Fresh $Fresh *>&1 | Out-String -Width 400 | Tee-Object -FilePath (Join-Path $Dir "setup.txt") | Out-Null
      $code = $LASTEXITCODE
    }
    "script" {
      $p = ArgsOf $job.ScriptArgs; $p["Dir"] = $Dir
      if ($game) { $p["Game"] = $game }
      & $job.Script @p *>&1 | Out-String -Width 400 | Tee-Object -FilePath (Join-Path $Dir "script.txt") | Out-Null
      $code = $LASTEXITCODE; if ($null -eq $code) { $code = 0 }
    }
    default { Log "unknown job $($job.Job)"; $code = 2 }
  }
  if ($game) {
    foreach ($d in @("logs", "Screens")) {
      if (Test-Path "$game\$d") { robocopy "$game\$d" (Join-Path $Dir "game-$d") /E /NFL /NDL /NJH /NJS /NP | Out-Null }
    }
  }
} catch {
  Log "job failed: $($_.Exception.Message) $($_.InvocationInfo.PositionMessage)"
  $code = 3
}
Log "exit $code"
exit $code
