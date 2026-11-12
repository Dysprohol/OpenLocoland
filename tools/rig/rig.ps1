# The test rig: runs the game's tests inside Windows Sandbox, off the user's screen.
# See docs/dev/working.md "The test rig" for what it covers and what it cannot do.
#
# One sandbox (the "box") serves many jobs, one after another: the first job starts it, it stays up
# until -Stop or until it has been idle for -IdleMinutes (it shuts itself down then), and every
# session's jobs go through the same queue. Only one Windows Sandbox can run on the machine at a time.
# A job that needs a Windows no earlier job has touched (-Job setup, or -Fresh) closes the box and
# starts a new one. The box never shows on the user's screen: it is started with no window
# (wsb start) and its client window lives on a Win32 desktop of the rig's own, which is never shown,
# so nothing flashes, takes the focus or gets a taskbar button.
#
#   .\tools\rig\rig.ps1 -Job missions -Game build\game [-TestArgs @{ Quick = $true }]
#   .\tools\rig\rig.ps1 -Job game -Game build\game -TestArgs @{ Map = "maps\l01\l01.map"; WaitFor = "test map: loaded"; ClickOk = 1; SetFull = 0 }
#   .\tools\rig\rig.ps1 -Job game -Game build\game -Resolution 1366x768 -Registry @{ FrameLimit = 0 } -TestArgs @{ ... }
#   .\tools\rig\rig.ps1 -Job setup -Setup build\setup\OpenLocoland-...-Setup.exe [-SetupArgs "--auto"]
#   .\tools\rig\rig.ps1 -Job probe [-Resolution 1920x1080] [-Scale 200]
#   .\tools\rig\rig.ps1 -Status | -Monitors | -Look | -Show | -Hide | -Stop | -Kill
#
# -Game        the built game folder (build.py --target); read-only for the box, copied inside
# -TestArgs    parameters of tools\test_game.ps1 (-Job game) or tools\test_missions.ps1 (-Job missions);
#              the box runs THIS repository's tools\ (a worktree's own scripts)
# -Registry    values set under the guest's HKCU\SOFTWARE\Gromada\Locoland (or -RegistryKey) before
#              the run, e.g. @{ FrameLimit = 0; FpsCounter = 1 }; the guest's key starts empty each job
# -Resolution  the guest's screen WxH for this job (the box window's size; later jobs keep it)
# -Scale       the guest's display scale in % for this job: only the scale of a host monitor (the box
#              window is put on it) - said plainly when no host monitor has it; see -Monitors
# -Out         where the job's results go (default build\rig\<job id> of this repository)
# -Timeout     minutes before the job is killed inside the box (default 30)
# -Fresh       run in a newly started box (always for -Job setup)
# -SetupArgs   Setup's options for -Job setup (default --silent; --auto = its window, installing by
#              itself); Setup is copied in and marked as downloaded (Zone.Identifier) unless -NoMotw;
#              the installed game then runs mission 1 unless -NoGame
# -Script      (-Job script) a .ps1 under the host folder run inside the box; -ScriptArgs its parameters
# -IdleMinutes the box shuts itself down after this long without a job (default 20; set when it starts)
# -DgVoodoo    values set in the guest's copy of dgVoodoo.conf, e.g. @{ OutputAPI = "d3d11warp" }
# -Warp        the same as -DgVoodoo @{ OutputAPI = "d3d11warp" }: Direct3D 11 in software (WARP). The box
#              itself always has the GPU: Windows Sandbox 0.8 ignores <VGpu>Disable</VGpu>
# -Status      the box, its agent, the queue          -Monitors  the host's monitors and their scales
# -Look        the guest's screen as a picture (taken every 5 s while a job runs), opened
# -Show        a window onto the box on YOUR desktop (it takes the session over: the guest then has
#              that window's size and scale); -Hide closes it and gives the session back to the rig
# -Stop        shut the box down (waits for a running job); -Kill stops it at once
param(
  [ValidateSet("", "probe", "game", "missions", "setup", "script")][string]$Job = "",
  [string]$Game = "", [hashtable]$TestArgs = @{}, [hashtable]$Registry = @{}, [string]$RegistryKey = "",
  [int]$Scale = 0, [string]$Resolution = "",
  [string]$Setup = "", [string]$SetupArgs = "--silent", [switch]$NoMotw, [switch]$NoGame,
  [string]$Script = "", [hashtable]$ScriptArgs = @{},
  [hashtable]$DgVoodoo = @{}, [switch]$Warp,
  [string]$Out = "", [double]$Timeout = 30, [switch]$Fresh, [int]$IdleMinutes = 20,
  [switch]$Status, [switch]$Monitors, [switch]$Look, [switch]$Show, [switch]$Hide, [switch]$Stop, [switch]$Kill
)
$ErrorActionPreference = "Stop"
$rigDir = $PSScriptRoot
$repo = Split-Path (Split-Path $rigDir)
# The host folder mapped read-only as C:\host: the folder that holds the main checkout (and with it
# every worktree under .claude\worktrees, every build folder, the test maps).
$main = if ($repo -match '^(.*?)\\\.claude\\worktrees\\') { $Matches[1] } else { $repo }
$hostRoot = Split-Path $main
$steam = "C:\Program Files (x86)\Steam\steamapps\common\Locoland"   # mapped read-only as C:\steam
# The rig's own folder: the box's io folder (mapped read-write as C:\rig\io), its state, the last
# start's desktop log. Not under %LOCALAPPDATA%: Windows Sandbox fails to start ("path not found",
# 0x80070003) with a .wsb or a mapped folder there. Inside the read-only C:\host it is fine (the
# nested read-write mapping wins).
$home_ = Join-Path $hostRoot "rig"
$io = Join-Path $home_ "io"
$boxFile = Join-Path $home_ "box.json"
$desktop = "OpenLocolandRig"
New-Item -ItemType Directory -Force $home_ | Out-Null
if (-not ("RigHost" -as [type])) { Add-Type -TypeDefinition (Get-Content (Join-Path $rigDir "RigHost.cs") -Raw) -ReferencedAssemblies System.Drawing }

function Say($s) { "[rig $(Get-Date -Format 'HH:mm:ss')] $s" }
function GuestPath([string]$p) {
  $full = [IO.Path]::GetFullPath($(if ([IO.Path]::IsPathRooted($p)) { $p } else { Join-Path (Get-Location) $p }))
  if ($full.StartsWith($hostRoot + "\", [StringComparison]::OrdinalIgnoreCase)) { return "C:\host\" + $full.Substring($hostRoot.Length + 1) }
  if ($full.StartsWith($steam, [StringComparison]::OrdinalIgnoreCase)) { return "C:\steam" + $full.Substring($steam.Length) }
  return $null
}

# The box is started with wsb.exe, Windows Sandbox's command line (in the Sandbox app, 0.8.114 here;
# its "wsb" alias is not registered for the user, so it is found through the package's folder):
# `wsb start` makes the sandbox with no window at all, and `wsb connect`, started on the rig's own
# desktop, gives it the client the guest needs - the user logs on only once a client connects, and
# the game needs a connected session (DirectDraw fails in a disconnected one). A box started that way
# outlives its client: when the client is closed, or the user opens his own window to look (which
# takes the session over), the rig connects a new one on its desktop before the next job.
# Without wsb.exe the rig falls back to WindowsSandbox.exe <file>.wsb on its desktop; that box ends
# when its client is closed.
function WsbExe {
  $k = Get-ChildItem "HKCU:\Software\Classes\Local Settings\Software\Microsoft\Windows\CurrentVersion\AppModel\Repository\Packages" -ErrorAction SilentlyContinue |
    Where-Object { $_.PSChildName -like "MicrosoftWindows.WindowsSandbox_*_x64__*" } | Select-Object -Last 1
  if ($k) { $f = Join-Path (Get-ItemProperty $k.PSPath).PackageRootFolder "wsb.exe"; if (Test-Path $f) { return $f } }
  return $null
}
$wsbExe = WsbExe
function BoxIds { if ($wsbExe) { @(& $wsbExe list 2>$null | Where-Object { $_ -match '^[0-9a-f-]{36}$' }) } else { @() } }
function Clients { @(Get-Process WindowsSandboxRemoteSession -ErrorAction SilentlyContinue) }
function BoxRunning { if ($wsbExe) { return [bool](BoxIds).Count } else { return [bool](Clients).Count } }
function AgentState {
  $f = Join-Path $io "agent\state.json"
  if (-not (Test-Path $f)) { return $null }
  try { $s = Get-Content $f -Raw | ConvertFrom-Json } catch { return $null }
  $s | Add-Member age ((Get-Date) - [datetime]$s.time).TotalSeconds -PassThru
}
function BoxUp { $s = AgentState; return ($s -and $s.age -lt 15 -and $s.state -ne "stopping" -and (BoxRunning)) }
function Queued { @(Get-ChildItem (Join-Path $io "queue") -Filter *.json -ErrorAction SilentlyContinue) }
# the box's client windows: the rig's, on its own desktop, and the user's own (-Show)
function HiddenClient { [void][RigHost]::SandboxWindows($desktop); [RigHost]::ClientWindow($desktop) }
function UserClient { [RigHost]::ClientWindow($null) }
function ClientWidth($h) { if ($h -eq [IntPtr]::Zero) { return 0 }; $c = New-Object RigHost+RECT; [void][RigHost]::GetClientRect($h, [ref]$c); return $c.R }

# Makes sure a client is connected on the rig's desktop (wsb boxes). One that lost its session (the
# "connection lost - reconnect?" box, 620x320) is closed and a new one connected. The user's own
# window, when he has one open, is left alone: it holds the session.
function Attach-Client {
  if (-not $wsbExe) { return }
  $id = (BoxIds) | Select-Object -First 1; if (-not $id) { throw "no box" }
  if ((UserClient) -ne [IntPtr]::Zero) { Say "your own window onto the box is open: the box shows there, and the guest has that window's size and scale"; return }
  if ((ClientWidth (HiddenClient)) -gt 700) { return }
  Clients | Stop-Process -Force -ErrorAction SilentlyContinue   # none of them is the user's (checked above)
  Start-Sleep -Milliseconds 500
  $r = [RigHost]::Run($wsbExe, ('"' + $wsbExe + '" connect --id ' + $id), $desktop)
  if ($r -le 0) { throw "wsb connect did not start (error $(-$r))" }
  $deadline = (Get-Date).AddSeconds(60)
  do { Start-Sleep -Milliseconds 500; $h = HiddenClient } while ((ClientWidth $h) -le 700 -and (Get-Date) -lt $deadline)
  if ((ClientWidth $h) -le 700) {
    if ($h -ne [IntPtr]::Zero) { [void][RigHost]::Capture($h, (Join-Path $home_ "client-error.png")) }
    throw "the box's client did not connect in 60 s (its window: $home_\client-error.png)"
  }
}
function Close-Clients([int]$grace = 10) {
  $deadline = (Get-Date).AddSeconds($grace)
  while ((Clients).Count -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
  Clients | Stop-Process -Force -ErrorAction SilentlyContinue
  Get-Process WindowsSandbox -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
}
function Kill-Box {
  foreach ($id in BoxIds) { & $wsbExe stop --id $id 2>&1 | Out-Null }
  Close-Clients 2   # a box started without wsb.exe ends with its client
  $deadline = (Get-Date).AddSeconds(30)
  while ((Get-Process vmmemWindowsSandbox -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
}
function Stop-Box([int]$wait = 60) {
  if (-not (BoxRunning)) { Close-Clients 0; return "no box running" }
  New-Item -ItemType File -Force (Join-Path $io "stop") | Out-Null   # the agent finishes the running job first
  $deadline = (Get-Date).AddSeconds($wait)
  $s = AgentState; if ($s -and $s.state -eq "running") { $deadline = $deadline.AddMinutes(30) }
  while ((BoxRunning) -and (Get-Date) -lt $deadline) { Start-Sleep -Seconds 1 }
  $how = "shut down by its agent"
  if (BoxRunning) { Kill-Box; $how = "stopped from the host (the agent did not shut it down)" }
  Close-Clients
  Remove-Item (Join-Path $io "stop") -ErrorAction SilentlyContinue
  return $how
}

function Start-Box {
  if (BoxRunning) {
    if (BoxUp) { Attach-Client; return }
    throw "a Windows Sandbox is running that is not the rig's, or its agent does not answer - only one can run at a time; close it, or rig.ps1 -Kill"
  }
  Close-Clients 0   # a client left from a box that shut itself down
  # A fresh io folder for each box (the old box's results went out with its jobs).
  if (Test-Path $io) { Remove-Item $io -Recurse -Force }
  New-Item -ItemType Directory -Force (Join-Path $io "agent"), (Join-Path $io "queue"), (Join-Path $io "jobs") | Out-Null
  @{ IdleMinutes = $IdleMinutes; ScreenSeconds = 5 } | ConvertTo-Json | Set-Content (Join-Path $io "agent\config.json") -Encoding utf8
  $agent = GuestPath (Join-Path $rigDir "guest\agent.ps1")
  if (-not $agent) { throw "the rig must live under $hostRoot (it is mapped into the box from there)" }
  if ($agent -match '[ "]') { throw "the agent's path in the box has a space or a quote: $agent" }
  $maps = @("<MappedFolder><HostFolder>$hostRoot</HostFolder><SandboxFolder>C:\host</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>",
            "<MappedFolder><HostFolder>$io</HostFolder><SandboxFolder>C:\rig\io</SandboxFolder><ReadOnly>false</ReadOnly></MappedFolder>")
  if (Test-Path $steam) { $maps += "<MappedFolder><HostFolder>$steam</HostFolder><SandboxFolder>C:\steam</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>" }
  $gpu = "Enable"   # Disable is ignored by Windows Sandbox 0.8.114 (the guest keeps the GPU) - -Warp instead
  # no double quotes anywhere: the text goes to wsb.exe on its command line
  $config = "<Configuration><VGpu>$gpu</VGpu><Networking>Disable</Networking><AudioInput>Disable</AudioInput><VideoInput>Disable</VideoInput>" +
            "<ClipboardRedirection>Disable</ClipboardRedirection><PrinterRedirection>Disable</PrinterRedirection><MemoryInMB>8192</MemoryInMB>" +
            "<MappedFolders>$($maps -join '')</MappedFolders>" +
            "<LogonCommand><Command>powershell -NoProfile -ExecutionPolicy Bypass -WindowStyle Minimized -File $agent</Command></LogonCommand></Configuration>"
  $wsbFile = Join-Path $home_ "box.wsb"
  Set-Content $wsbFile $config -Encoding utf8
  [RigHost]::Watch($true)   # logs the user's desktop during the start, minimises any sandbox window there
  $t = Get-Date
  try {
    if ($wsbExe) {
      $out = (& $wsbExe start --config $config 2>&1) -join " "
      $id = ([regex]::Match($out, '[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}')).Value
      if (-not $id) { throw "wsb start: $out" }
      Say "box $id started with no window (vGPU $gpu); its client goes on the rig's own desktop '$desktop'"
      Attach-Client
    } else {
      $pid_ = [RigHost]::Start($wsbFile, $desktop)
      if ($pid_ -le 0) { throw "WindowsSandbox.exe did not start (error $(-$pid_))" }
      Say "box starting (no wsb.exe: WindowsSandbox.exe $pid_ on the rig's own desktop '$desktop', vGPU $gpu)"
    }
    $deadline = (Get-Date).AddSeconds(180)
    while (-not (AgentState) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
  } finally {
    [RigHost]::StopWatch()
    $ev = [RigHost]::Events()
    $ev | Set-Content (Join-Path $home_ "start-desktop-events.txt") -Encoding utf8
    $shown = @($ev | Where-Object { $_ -match "SHOWN" })
    if ($shown.Count) { Say "a sandbox window came up on your desktop during the start: $($shown -join ' | ')" }
  }
  if (-not (AgentState)) {
    $h = HiddenClient; if ($h -ne [IntPtr]::Zero) { [void][RigHost]::Capture($h, (Join-Path $home_ "start-error.png")) }
    Kill-Box; throw "the box's agent did not come up in 180 s - box stopped (its window: $home_\start-error.png)"
  }
  @{ started = $t.ToString("o"); vgpu = $gpu; wsb = [bool]$wsbExe } | ConvertTo-Json | Set-Content $boxFile -Encoding utf8
  Say "box up in $([int]((Get-Date) - $t).TotalSeconds) s"
}

# The guest's screen is the box window's client area (remote desktop with dynamic resolution) and its
# scale is the scale of the host monitor the window is on - nothing inside the guest can change either
# (guest\display.ps1 says why). So a resolution is made by sizing the window (it stays on the rig's
# desktop, invisible) and a scale by putting it on a host monitor of that scale. The agent's heartbeat
# says what the guest got; when that is not what was asked, the job is not run.
function Set-BoxDisplay([string]$res, [int]$scale) {
  if ((UserClient) -ne [IntPtr]::Zero) { throw "your own window onto the box is open - the guest's display follows it; rig.ps1 -Hide first" }
  $win = HiddenClient
  if ($win -eq [IntPtr]::Zero) { throw "the box's window was not found" }
  $mons = [RigHost]::Monitors()
  $list = ($mons | ForEach-Object { $_.ToString() }) -join "; "
  $mon = if ($scale) { $mons | Where-Object { $_.Scale -eq $scale } | Select-Object -First 1 } else { $mons | Where-Object { $_.Primary } | Select-Object -First 1 }
  if (-not $mon) { throw "scale $scale % cannot be produced: the guest takes the scale of the host monitor its window is on, and the host's monitors are: $list" }
  $s = AgentState
  if ($s.screen -notmatch '^(\d+)x(\d+)@(\d+)$') { throw "no screen in the agent's heartbeat (an agent older than this rig - rig.ps1 -Stop and start again)" }
  if ($res) { if ($res -notmatch '^(\d+)x(\d+)$') { throw "-Resolution '$res' is not WxH" }; $w = [int]$Matches[1]; $h = [int]$Matches[2] }
  else { $null = $s.screen -match '^(\d+)x(\d+)@'; $w = [int]$Matches[1]; $h = [int]$Matches[2] }
  $want = "${w}x${h}@$($mon.Dpi)"
  if ($s.screen -eq $want) { return "guest display $w x $h at $($mon.Scale) % (as it was)" }
  # The client area holds the guest's screen and the window's own title strip; the strip's height is
  # corrected from what the guest gets.
  $strip = [int](33 * $mon.Dpi / 96); $placed = ""
  for ($try = 1; $try -le 3; $try++) {
    $placed = [RigHost]::Place($win, $mon.X, $mon.Y, $w, $h + $strip)
    $deadline = (Get-Date).AddSeconds(10)
    do { Start-Sleep -Milliseconds 500; $s = AgentState } while ($s.screen -ne $want -and (Get-Date) -lt $deadline)
    if ($s.screen -eq $want) { return "guest display $w x $h at $($mon.Scale) % (window $placed on $($mon.Name))" }
    if ($s.screen -match '^(\d+)x(\d+)@(\d+)$' -and [int]$Matches[1] -eq $w -and [int]$Matches[3] -eq $mon.Dpi) { $strip += $h - [int]$Matches[2] } else { break }
  }
  throw "display $w x $h at $($mon.Scale) % cannot be produced: the guest shows $($s.screen) (window $placed on $($mon.Name)); the window cannot be larger than the host's screens allow - host monitors: $list"
}

# ---- commands
if ($Monitors) { [RigHost]::Monitors() | ForEach-Object { Say $_.ToString() }; Say "scales the box can have: $((([RigHost]::Monitors() | ForEach-Object { "$($_.Scale) %" }) | Sort-Object -Unique) -join ', ')"; return }
if ($Kill) { Kill-Box; Say "killed; sandboxes running: $((BoxIds).Count), clients: $((Clients).Count)"; return }
if ($Stop) { Say (Stop-Box); return }
if ($Status) {
  if (-not (BoxRunning)) { Say "no box running$(if ((Clients).Count) { " ($((Clients).Count) client window(s) left - rig.ps1 -Stop closes them)" })"; return }
  $s = AgentState
  Say "box $((BoxIds) -join ','): $(if ($s) { "agent $($s.state)$(if ($s.job) { " ($($s.job))" }), $($s.done) job(s) done, fresh $($s.fresh), guest screen $($s.screen), up $($s.uptime) s, heartbeat $([int]$s.age) s ago" } else { 'no agent heartbeat' })"
  Say "clients: rig's $(if ((ClientWidth (HiddenClient)) -gt 700) { 'connected' } else { 'not connected' }), yours $(if ((UserClient) -ne [IntPtr]::Zero) { 'OPEN' } else { 'none' })"
  Queued | ForEach-Object { Say "queued: $($_.BaseName)" }
  return
}
if ($Look) {
  $png = Join-Path $io "agent\screen.png"
  if (Test-Path $png) { Say "the guest's screen at $((Get-Item $png).LastWriteTime.ToString('HH:mm:ss')) (taken every 5 s while a job runs): $png"; Invoke-Item $png } else { Say "no picture of the guest's screen yet" }
  return
}
if ($Show) {
  if (-not $wsbExe -or -not (BoxRunning)) { Say "no box to show$(if (-not $wsbExe) { ' (and no wsb.exe: the box''s window cannot be opened twice)' })"; return }
  Start-Process $wsbExe -ArgumentList @("connect", "--id", ((BoxIds) | Select-Object -First 1))
  Say "a window onto the box opens on your desktop; it holds the session while open (the guest takes its size and scale). Close it or rig.ps1 -Hide when done."
  return
}
if ($Hide) {
  $u = UserClient
  while ($u -ne [IntPtr]::Zero) { $procId = 0; [void][RigHost]::SandboxWindows($null); $p = Get-Process WindowsSandboxRemoteSession | Where-Object { $_.MainWindowHandle -eq $u }; if ($p) { $p | Stop-Process -Force } else { break }; Start-Sleep 1; $u = UserClient }
  if (BoxRunning) { Attach-Client; Say "the session is back with the rig's own (hidden) window" } else { Say "no box running" }
  return
}
if (-not $Job) { Get-Help $PSCommandPath; return }

# ---- one job
if ($Job -eq "setup") { $Fresh = $true }
$lock = $null
try {
  # one session at a time starts, replaces or re-attaches a box
  $lockFile = Join-Path $home_ "start.lock"
  $deadline = (Get-Date).AddMinutes(40)
  while (-not $lock) { try { $lock = [IO.File]::Open($lockFile, 'OpenOrCreate', 'ReadWrite', 'None') } catch { if ((Get-Date) -gt $deadline) { throw "the rig's start lock was held for 40 min" }; Start-Sleep 1 } }
  if ($Fresh -and (BoxRunning)) {
    $s = AgentState
    if (-not ($s -and $s.fresh -and -not (Queued).Count)) { Say "a fresh Windows is needed - closing the running box (after its running job): $(Stop-Box)" }
  }
  Start-Box
  if ($Resolution -or $Scale) {
    # the display belongs to the whole box: changed only while no job runs or waits
    $deadline = (Get-Date).AddMinutes($Timeout + 30)
    while (((AgentState).state -ne "idle" -or (Queued).Count) -and (Get-Date) -lt $deadline) { Start-Sleep 2 }
    Say (Set-BoxDisplay $Resolution $Scale)
  }
} finally { if ($lock) { $lock.Dispose() } }

$id = "{0:yyyyMMdd-HHmmss}-{1}-{2}" -f (Get-Date), $Job, (Get-Random -Maximum 9999)
$spec = [ordered]@{ Job = $Job; TimeoutMinutes = $Timeout; Runner = (GuestPath (Join-Path $rigDir "guest\runjob.ps1")); Tools = (GuestPath (Join-Path $repo "tools")) }
if ($Scale) { $spec.Scale = $Scale }
if ($Resolution) { $spec.Resolution = $Resolution }
if ($TestArgs.Count) { $spec.Args = $TestArgs }
if ($Registry.Count) { $spec.Registry = $Registry }
if ($RegistryKey) { $spec.RegistryKey = $RegistryKey }
if ($Warp) { $DgVoodoo["OutputAPI"] = "d3d11warp" }
if ($DgVoodoo.Count) { $spec.DgVoodoo = $DgVoodoo }
if ($Game) {
  if (-not (Test-Path (Join-Path $Game "Locoland.exe"))) { throw "no Locoland.exe in $Game" }
  $g = GuestPath $Game
  if (-not $g) {
    # outside the mapped folder: copied into the io folder (removed with the box)
    $stage = Join-Path $io "stage\$id"; Say "the game is outside $hostRoot - copying it into the rig's folder"
    robocopy (Resolve-Path $Game) $stage /E /NFL /NDL /NJH /NJS /NP /MT:16 | Out-Null
    $g = "C:\rig\io\stage\$id"
  }
  $spec.Game = $g
}
if ($Job -eq "setup") {
  $s = GuestPath $Setup; if (-not $s) { throw "-Setup must be under $hostRoot" }
  $spec.Setup = $s; $spec.SetupArgs = $SetupArgs; $spec.Steam = "C:\steam"
  if ($NoMotw) { $spec.NoMotw = $true }
  if ($NoGame) { $spec.NoGame = $true }
}
if ($Job -eq "script") { $spec.Script = GuestPath $Script; if (-not $spec.Script) { throw "-Script must be under $hostRoot" }; if ($ScriptArgs.Count) { $spec.ScriptArgs = $ScriptArgs } }
$q = Join-Path $io "queue\$id"
($spec | ConvertTo-Json -Depth 5) | Set-Content "$q.tmp" -Encoding utf8
Move-Item "$q.tmp" "$q.json"
Say "job $id queued$(if ((Queued).Count -gt 1) { " behind $((Queued).Count - 1) other(s)" })"
$jobDir = Join-Path $io "jobs\$id"; $doneFile = Join-Path $jobDir "done.json"
$deadline = (Get-Date).AddMinutes($Timeout + 5); $n = 0
while (-not (Test-Path $doneFile)) {
  Start-Sleep -Seconds 1; $n++
  if ($n % 10 -eq 0) {
    if (-not (BoxRunning)) { throw "the box went away during job $id" }
    # a job of this session waits in the queue while others run; the client must stay connected
    if ((Test-Path $jobDir) -and (UserClient) -eq [IntPtr]::Zero -and (ClientWidth (HiddenClient)) -le 700) { Say "the box's client lost its session - connecting a new one"; Attach-Client }
  }
  if ((Test-Path $jobDir) -and (Get-Date) -gt $deadline) { New-Item -ItemType File -Force (Join-Path $jobDir "cancel") | Out-Null; Say "job ${id} over its time - cancel asked"; $deadline = (Get-Date).AddMinutes(2) }
}
Start-Sleep -Milliseconds 500
$done = Get-Content $doneFile -Raw | ConvertFrom-Json
if (-not $Out) { $Out = Join-Path $repo "build\rig\$id" }
New-Item -ItemType Directory -Force $Out | Out-Null
robocopy $jobDir $Out /E /NFL /NDL /NJH /NJS /NP | Out-Null
Remove-Item $jobDir -Recurse -Force -ErrorAction SilentlyContinue
Say "job ${id}: exit $($done.exit) in $($done.seconds) s$(if ($done.killed) { " ($($done.killed))" }) - results in $Out"
if (Test-Path (Join-Path $Out "job.log")) { Get-Content (Join-Path $Out "job.log") | Where-Object { $_ -match "display:|failed|exit|installed|Setup exit" } | ForEach-Object { "  $_" } }
if ($Job -eq "setup") { Say "closing the box after the installer test: $(Stop-Box)" }
exit $done.exit
