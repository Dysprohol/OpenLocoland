# The test rig: runs the game's tests inside Windows Sandbox, off the user's screen.
# See docs/dev/working.md "The test rig" for what it covers and what it cannot do.
#
# One sandbox (the "box") serves every session's jobs from one queue: the first job starts it, it
# stays up until -Stop or until it has been idle for -IdleMinutes (it shuts itself down then). Only
# one Windows Sandbox can run on the machine at a time. Inside, the agent runs several jobs at once
# (guest\agent.ps1 says which may share the box and which run alone): quiet game and mission runs side
# by side, each as a guest user of its own; an installer, a script, a visible run, a run with sound
# or -Exclusive alone. A job that needs a Windows no earlier job has touched (-Job setup, or -Fresh)
# waits until the box is empty, which is then restarted (the queue is kept). Every job runs under
# the display it asked for - one that names none under the default (the box's starting size, 100 %);
# the agent asks for a display only while no job runs, and the rig.ps1 of any waiting job makes it.
# The box never shows on the user's screen: it is started with no window
# (wsb start) and its client window lives on a Win32 desktop of the rig's own, which is never shown,
# so nothing flashes, takes the focus or gets a taskbar button.
#
#   .\tools\rig\rig.ps1 -Job missions -Game build\game [-TestArgs @{ Quick = $true }]
#   .\tools\rig\rig.ps1 -Job game -Game build\game -TestArgs @{ Map = "maps\l01\l01.map"; WaitFor = "test map: loaded"; ClickOk = 1; SetFull = 0 }
#   .\tools\rig\rig.ps1 -Job game -Game build\game -Resolution 1366x768 -Registry @{ FrameLimit = 0 } -TestArgs @{ ... }
#   .\tools\rig\rig.ps1 -Job setup -Setup build\setup\OpenLocoland-...-Setup.exe [-SetupArgs "--auto"]
#   .\tools\rig\rig.ps1 -Job probe [-Resolution 1920x1080] [-Scale 200]
#   .\tools\rig\rig.ps1 -Job game -Game build\game -Exclusive -TestArgs @{ ... }   # the box to itself
#   .\tools\rig\rig.ps1 -Status | -Monitors | -Look | -Peek [-Front] | -Hide | -Stop | -Kill   (tools\rig\peek.cmd = -Peek)
#
# -Game        the built game folder (build.py --target); read-only for the box, copied inside
# -Tools       the tools folder whose test_game.ps1 / test_missions.ps1 run (default: this repository's),
#              e.g. another worktree's, to test its build with its own scripts
# -TestArgs    parameters of tools\test_game.ps1 (-Job game) or tools\test_missions.ps1 (-Job missions);
#              the box runs THIS repository's tools\ (a worktree's own scripts)
# -Registry    values set under the guest's HKCU\SOFTWARE\Gromada\Locoland (or -RegistryKey) before
#              the run, e.g. @{ FrameLimit = 0; FpsCounter = 1 }; the guest's key starts empty each job
# -Resolution  the guest's screen WxH for this job (the box window's size); without it the job runs
#              under the box's default size (its starting size, or -DefaultResolution)
# -Scale       the guest's display scale in % for this job (default 100): only the scale of a host
#              monitor (the box window is put on it) - said plainly when no host monitor has it; see
#              -Monitors. Jobs with another display than the running ones wait until the box is empty.
# -Exclusive   the job runs alone in the box (nothing else runs while it runs; it waits for the
#              running jobs to end) - for a timing measurement or anything that touches the whole guest.
#              Installer, script, visible, -Sound jobs run alone anyway.
# -Out         where the job's results go (default build\rig\<job id> of this repository)
# -Timeout     minutes before the job is killed inside the box (default 30)
# -Fresh       run in a newly started box (always for -Job setup)
# -SetupArgs   Setup's options for -Job setup (default --silent; --auto = its window, installing by
#              itself); Setup is copied in and marked as downloaded (Zone.Identifier) unless -NoMotw;
#              the installed game then runs mission 1 unless -NoGame
# -Parallel    (-Job missions) how many missions run at once, each lane with its own copy of the game
#              (default 4; 1 = one after another, as on the host)
# -Sound       let this job's sound out (it plays on the host's speakers): the guest's playback is
#              muted from the box's start and before every job, never the host's mixer
# -Script      (-Job script) a .ps1 under the host folder run inside the box; -ScriptArgs its parameters
# -MemoryMB    the box's memory (default 8192, set when it starts): the guest uses 1.9 GB idle and about
#              0.25 GB more per game running (file cache besides); the host gives only what the guest touches
# -MaxJobs     at most this many jobs at once in the box (default 4; set when it starts)
# -MaxGames    at most this many games at once (default 6; a missions job counts its lanes; set when
#              it starts; a single job larger than that still runs, alone)
# -DefaultResolution  the display a job that names none runs under (default: the box's starting size)
# -MaxWait     minutes a job may wait in the queue before this script withdraws it (default 120)
# -IdleMinutes the box shuts itself down after this long without a job (default 10; set when it starts)
# -DgVoodoo    values set in the guest's copy of dgVoodoo.conf, e.g. @{ OutputAPI = "d3d11warp" }
# -Warp        the same as -DgVoodoo @{ OutputAPI = "d3d11warp" }: Direct3D 11 in software (WARP). The box
#              itself always has the GPU: Windows Sandbox 0.8 ignores <VGpu>Disable</VGpu>
# -Status      the box, its agent, the running jobs and the queue with why each waits
# -Monitors    the host's monitors and their scales
# -Look        the guest's screen as a picture (taken every 5 s while a job runs), opened
# -Peek        a window onto the box on YOUR desktop, minimised to a taskbar button (-Front: shown at once).
#              It takes the session over: the job goes on, but the guest has that window's size and
#              scale while it is open. Closing it closes only the view; the rig connects its own hidden
#              client again (and puts back the size the last job asked for). -Hide closes it for you.
# -Stop        shut the box down (the running jobs finish first; queued ones wait for the next box);
#              -Kill stops it at once
# -Restart     once no job runs or waits, a new box with THIS rig.ps1's agent (after the rig changed;
#              -MaxJobs / -MaxGames / -MemoryMB take effect then)
param(
  [ValidateSet("", "probe", "game", "missions", "setup", "script")][string]$Job = "",
  [string]$Game = "", [string]$Tools = "", [hashtable]$TestArgs = @{}, [hashtable]$Registry = @{}, [string]$RegistryKey = "",
  [int]$Scale = 0, [string]$Resolution = "",
  [string]$Setup = "", [string]$SetupArgs = "--silent", [switch]$NoMotw, [switch]$NoGame,
  [string]$Script = "", [hashtable]$ScriptArgs = @{},
  [hashtable]$DgVoodoo = @{}, [switch]$Warp, [switch]$Sound, [int]$Parallel = 0, [int]$MemoryMB = 8192,
  [string]$Out = "", [double]$Timeout = 30, [switch]$Fresh, [int]$IdleMinutes = 10,
  [switch]$Exclusive, [int]$MaxJobs = 4, [int]$MaxGames = 6, [string]$DefaultResolution = "", [double]$MaxWait = 120,
  [switch]$Status, [switch]$Monitors, [switch]$Look, [Alias("Show")][switch]$Peek, [switch]$Front, [switch]$Hide, [switch]$Stop, [switch]$Kill, [switch]$Restart
)
$ErrorActionPreference = "Stop"
$rigDir = $PSScriptRoot
$repo = Split-Path (Split-Path $rigDir)
# The host folder mapped read-only as C:\host: the folder that holds the main checkout (and with it
# every worktree under .claude\worktrees, every build folder, the test maps).
$main = if ($repo -match '^(.*?)\\\.claude\\worktrees\\') { $Matches[1] } else { $repo }
# who queues: the worktree this script is in (the agent takes turns between callers)
$caller = if ($repo -match '\\\.claude\\worktrees\\([^\\]+)$') { $Matches[1] } else { "main" }
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
# whatever stops this script says why and where, also when its output goes to a file
trap { Say "rig.ps1 stopped: $($_.Exception.Message) $($_.InvocationInfo.PositionMessage -replace '\s+', ' ')"; break }
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
# the box's client windows: the rig's, on its own desktop, and the user's own (-Peek)
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
  # a new client has the default size: the display the last job asked for is made again
  $b = if (Test-Path $boxFile) { Get-Content $boxFile -Raw | ConvertFrom-Json } else { $null }
  if ($b -and $b.display) {
    $deadline = (Get-Date).AddSeconds(30); do { Start-Sleep -Milliseconds 500 } while (-not (AgentState).screen -and (Get-Date) -lt $deadline)
    Say "client connected again - $(Set-BoxDisplay $b.display.res $b.display.scale)"
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
  Remove-Item $boxFile -ErrorAction SilentlyContinue   # the old box's display request is not this one's
  # The io folder is kept: jobs other sessions queued wait for the new box, finished jobs wait for
  # their callers to fetch them. A job the old box was running when it went is marked as ended so.
  New-Item -ItemType Directory -Force (Join-Path $io "agent"), (Join-Path $io "queue"), (Join-Path $io "jobs") | Out-Null
  Get-ChildItem (Join-Path $io "agent") -File | Where-Object { $_.Name -ne "agent.log" } | Remove-Item -Force -ErrorAction SilentlyContinue
  if (Test-Path (Join-Path $io "agent\agent.log")) { Move-Item (Join-Path $io "agent\agent.log") (Join-Path $io "agent\agent-previous-box.log") -Force }
  Remove-Item (Join-Path $io "stop") -ErrorAction SilentlyContinue
  foreach ($d in @(Get-ChildItem (Join-Path $io "jobs") -Directory -ErrorAction SilentlyContinue)) {
    if (-not (Test-Path (Join-Path $d.FullName "done.json"))) {
      (@{ exit = 125; seconds = 0; killed = "the box stopped while the job ran" } | ConvertTo-Json -Compress) | Set-Content (Join-Path $d.FullName "done.json") -Encoding utf8 }
  }
  $q = @(Queued).Count; if ($q) { Say "$q job(s) of earlier calls wait in the queue for this box" }
  [ordered]@{ IdleMinutes = $IdleMinutes; ScreenSeconds = 5; MaxJobs = $MaxJobs; MaxGames = $MaxGames; DefaultResolution = $DefaultResolution } |
    ConvertTo-Json | Set-Content (Join-Path $io "agent\config.json") -Encoding utf8
  $agent = GuestPath (Join-Path $rigDir "guest\agent.ps1")
  if (-not $agent) { throw "the rig must live under $hostRoot (it is mapped into the box from there)" }
  if ($agent -match '[ "]') { throw "the agent's path in the box has a space or a quote: $agent" }
  $maps = @("<MappedFolder><HostFolder>$hostRoot</HostFolder><SandboxFolder>C:\host</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>",
            "<MappedFolder><HostFolder>$io</HostFolder><SandboxFolder>C:\rig\io</SandboxFolder><ReadOnly>false</ReadOnly></MappedFolder>")
  if (Test-Path $steam) { $maps += "<MappedFolder><HostFolder>$steam</HostFolder><SandboxFolder>C:\steam</SandboxFolder><ReadOnly>true</ReadOnly></MappedFolder>" }
  $gpu = "Enable"   # Disable is ignored by Windows Sandbox 0.8.114 (the guest keeps the GPU) - -Warp instead
  # no double quotes anywhere: the text goes to wsb.exe on its command line
  $config = "<Configuration><VGpu>$gpu</VGpu><Networking>Disable</Networking><AudioInput>Disable</AudioInput><VideoInput>Disable</VideoInput>" +
            "<ClipboardRedirection>Disable</ClipboardRedirection><PrinterRedirection>Disable</PrinterRedirection><MemoryInMB>$MemoryMB</MemoryInMB>" +
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
  # the primary monitor when it has the scale (or none is asked), else the first that has it
  $mon = if ($scale) { $mons | Where-Object { $_.Scale -eq $scale } | Sort-Object { -not $_.Primary } | Select-Object -First 1 } else { $mons | Where-Object { $_.Primary } | Select-Object -First 1 }
  if (-not $mon) { throw "scale $scale % cannot be produced: the guest takes the scale of the host monitor its window is on, and the host's monitors are: $list" }
  $s = AgentState
  if ($s.screen -notmatch '^(\d+)x(\d+)@(\d+)$') { throw "no screen in the agent's heartbeat (an agent older than this rig - rig.ps1 -Stop and start again)" }
  if ($res) { if ($res -notmatch '^(\d+)x(\d+)$') { throw "-Resolution '$res' is not WxH" }; $w = [int]$Matches[1]; $h = [int]$Matches[2] }
  else { $null = $s.screen -match '^(\d+)x(\d+)@'; $w = [int]$Matches[1]; $h = [int]$Matches[2] }
  $want = "${w}x${h}@$($mon.Dpi)"
  $b = Get-Content $boxFile -Raw | ConvertFrom-Json
  $b | Add-Member display ([pscustomobject]@{ res = "${w}x$h"; scale = $scale }) -Force
  ($b | ConvertTo-Json) | Set-Content $boxFile -Encoding utf8
  # A display counts only once the guest has kept it for 3 s: the client changes its own size a
  # moment after a move between monitors of different scales, and a job must not start on a passing one.
  function Held([string]$want) {
    $until = (Get-Date).AddSeconds(3)
    while ((Get-Date) -lt $until) { Start-Sleep -Milliseconds 500; if ((AgentState).screen -ne $want) { return $false } }
    return $true
  }
  if ($s.screen -eq $want -and (Held $want)) { return "guest display $w x $h at $($mon.Scale) % (as it was)" }
  # First onto the monitor: on one of another scale Windows resizes the window by the scales' ratio
  # (WM_DPICHANGED) right after the move, so the size is set only once that is over.
  $tl = [RigHost]::TopLeft($win)
  if ($tl[0] -lt $mon.X -or $tl[0] -ge $mon.X + $mon.W -or $tl[1] -lt $mon.Y -or $tl[1] -ge $mon.Y + $mon.H) {
    [RigHost]::Move($win, $mon.X, $mon.Y); Start-Sleep -Milliseconds 1500 }
  # The client area holds the guest's screen and the window's own title strip; both sizes are
  # corrected from what the guest gets.
  $cw = $w; $ch = $h + [int](33 * $mon.Dpi / 96); $placed = ""
  for ($try = 1; $try -le 5; $try++) {
    $placed = [RigHost]::Place($win, $mon.X, $mon.Y, $cw, $ch)
    $deadline = (Get-Date).AddSeconds(10)
    do { Start-Sleep -Milliseconds 500; $s = AgentState } while ($s.screen -ne $want -and (Get-Date) -lt $deadline)
    if ($s.screen -eq $want -and (Held $want)) { return "guest display $w x $h at $($mon.Scale) % (window $placed on $($mon.Name)$(if ($try -gt 1) { ", try $try" }))" }
    $s = AgentState
    if ($s.screen -match '^(\d+)x(\d+)@(\d+)$' -and [int]$Matches[3] -eq $mon.Dpi) { $cw += $w - [int]$Matches[1]; $ch += $h - [int]$Matches[2] }
    elseif ($s.screen -notmatch '^(\d+)x(\d+)@(\d+)$') { break }
    if ($cw -le 200 -or $ch -le 200 -or $cw -gt $mon.W * 2 -or $ch -gt $mon.H * 2) { break }
  }
  throw "display $w x $h at $($mon.Scale) % cannot be produced: the guest shows $($s.screen) (window $placed on $($mon.Name)); the window cannot be larger than the host's screens allow - host monitors: $list"
}

# ---- what a waiting rig.ps1 does for the agent (protocol 2): any caller that waits serves it,
# whosever job it is, one at a time (a lock file of its own)
function Try-Lock([string]$name) { try { return [IO.File]::Open((Join-Path $home_ $name), 'OpenOrCreate', 'ReadWrite', 'None') } catch { return $null } }
function Read-Json([string]$f) { try { return Get-Content $f -Raw -ErrorAction Stop | ConvertFrom-Json } catch { return $null } }
# The agent asks for a display only while no job runs (agent\display-want.json); the answer goes to
# agent\display-done.json.
function Serve-Display {
  $w = Read-Json (Join-Path $io "agent\display-want.json"); if (-not $w) { return }
  $d = Read-Json (Join-Path $io "agent\display-done.json"); if ($d -and [int]$d.n -eq [int]$w.n) { return }
  # while the user's own window onto the box is open the guest has that window's display: the job
  # waits (it is not failed) until the window is closed
  if ((UserClient) -ne [IntPtr]::Zero) { if (-not $script:saidPeek) { Say "job $($w.for) waits for its display: your own window onto the box is open (close it, or rig.ps1 -Hide)"; $script:saidPeek = $true }; return }
  $l = Try-Lock "display.lock"; if (-not $l) { return }
  try {
    $d = Read-Json (Join-Path $io "agent\display-done.json"); if ($d -and [int]$d.n -eq [int]$w.n) { return }
    $ok = $true
    try { $text = Set-BoxDisplay $w.res $(if ([int]$w.scale -eq 100) { 0 } else { [int]$w.scale }) } catch { $ok = $false; $text = $_.Exception.Message }
    $f = Join-Path $io "agent\display-done.json"
    ([ordered]@{ n = [int]$w.n; ok = $ok; text = "$text"; by = $caller } | ConvertTo-Json -Compress) | Set-Content "$f.tmp" -Encoding utf8; Move-Item "$f.tmp" $f -Force
    Say "the display for job $($w.for): $text"
  } finally { $l.Dispose() }
}
# A job that needs a fresh Windows is next and the box is empty: the box is restarted (the queue
# is kept, so the job runs first in the new box).
function Serve-Fresh {
  $s = AgentState; if (-not $s -or -not $s.needFresh -or @($s.running).Count) { return }
  $l = Try-Lock "start.lock"; if (-not $l) { return }
  try {
    $s = AgentState; if (-not $s -or -not $s.needFresh -or @($s.running).Count) { return }
    Say "job $($s.needFresh) needs a fresh Windows and the box is empty - restarting the box (queued jobs wait for the new one): $(Stop-Box)"
    Start-Box
  } finally { $l.Dispose() }
}
function Show-Status($s) {
  if ([int]$s.proto -lt 2) { return }
  Say "  display $($s.display) % (a job naming none gets $($s.default) %); $($s.games) of $($s.maxGames) games, $(@($s.running).Count) of $($s.maxJobs) jobs"
  foreach ($r in @($s.running)) { Say "  running: $($r.id) ($($r.caller)) $(if ($r.slot) { "slot $($r.slot)" } else { 'ALONE: ' + $r.alone }), $($r.games) game(s), $($r.seconds) s of its $($r.timeout) min" }
  foreach ($q in @($s.queue)) { Say "  waiting $($q.pos): $($q.id) ($($q.caller)) - $($q.why)" }
  if ($s.needFresh) { Say "  a fresh box is wanted for $($s.needFresh) (any waiting rig.ps1 restarts it once the box is empty)" }
}

# ---- commands
if ($Monitors) { [RigHost]::Monitors() | ForEach-Object { Say $_.ToString() }; Say "scales the box can have: $((([RigHost]::Monitors() | ForEach-Object { "$($_.Scale) %" }) | Sort-Object -Unique) -join ', ')"; return }
if ($Kill) { Kill-Box; Say "killed; sandboxes running: $((BoxIds).Count), clients: $((Clients).Count)"; return }
if ($Stop) { Say (Stop-Box); return }
if ($Restart) {
  # a new box with THIS rig's agent (e.g. after the rig changed), once the running one is idle: the
  # start lock is held meanwhile, so no rig.ps1 starts or replaces a box under it
  $lock = $null; $deadline = (Get-Date).AddMinutes($MaxWait)
  try {
    while (-not $lock) { $lock = Try-Lock "start.lock"; if (-not $lock) { if ((Get-Date) -gt $deadline) { throw "the start lock stayed held" }; Start-Sleep 1 } }
    while ((BoxRunning) -and (((AgentState).state -ne "idle") -or (Queued).Count)) {
      if ((Get-Date) -gt $deadline) { throw "the box did not become idle in $MaxWait min - not restarted" }
      Start-Sleep 2
    }
    if (BoxRunning) { Say "the box is idle and nothing waits - $(Stop-Box)" }
    Start-Box
    Say "box up with this rig's agent (protocol $((AgentState).proto))"
  } finally { if ($lock) { $lock.Dispose() } }
  return
}
if ($Status) {
  if (-not (BoxRunning)) { Say "no box running$(if ((Clients).Count) { " ($((Clients).Count) client window(s) left - rig.ps1 -Stop closes them)" })"; return }
  $s = AgentState
  Say "box $((BoxIds) -join ','): $(if ($s) { "agent $($s.state)$(if ($s.job) { " ($($s.job))" }), $($s.done) job(s) done, fresh $($s.fresh), guest screen $($s.screen), up $($s.uptime) s, heartbeat $([int]$s.age) s ago" } else { 'no agent heartbeat' })"
  Say "clients: rig's $(if ((ClientWidth (HiddenClient)) -gt 700) { 'connected' } else { 'not connected' }), yours $(if ((UserClient) -ne [IntPtr]::Zero) { 'OPEN' } else { 'none' })"
  if ($s -and [int]$s.proto -ge 2) { Say "agent protocol $($s.proto): several jobs at once"; Show-Status $s }
  else {
    if ($s) { Say "agent protocol 1 (started by an older rig.ps1): one job at a time, in the order queued" }
    Queued | ForEach-Object { Say "queued: $($_.BaseName)" }
  }
  return
}
if ($Look) {
  $png = Join-Path $io "agent\screen.png"
  if (Test-Path $png) { Say "the guest's screen at $((Get-Item $png).LastWriteTime.ToString('HH:mm:ss')) (taken every 5 s while a job runs): $png"; Invoke-Item $png } else { Say "no picture of the guest's screen yet" }
  return
}
if ($Peek) {
  if (-not $wsbExe -or -not (BoxRunning)) { Say "no box to look into$(if (-not $wsbExe) { ' (and no wsb.exe: the box''s window cannot be opened twice)' })"; return }
  if ((UserClient) -ne [IntPtr]::Zero) { Say "your window onto the box is already open"; return }
  $id = (BoxIds) | Select-Object -First 1
  if ($Front) { Start-Process $wsbExe -ArgumentList @("connect", "--id", $id) }
  else {
    # minimised without activation; the watcher minimises the window again whenever it shows itself
    # in the first 20 s (the client restores itself once while it connects)
    [RigHost]::Watch($true)
    [void][RigHost]::Run($wsbExe, ('"' + $wsbExe + '" connect --id ' + $id), $null)
    $deadline = (Get-Date).AddSeconds(20); while ((Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
    [RigHost]::StopWatch()
  }
  Say "a window onto the box is $(if ($Front) { 'open' } else { 'on your taskbar (Windows Sandbox), minimised' }). While it is open the box shows there and the guest has its size and scale; a running job goes on. Its sound is muted inside the guest - the guest's own speaker icon (bottom right in that window) unmutes it until the next job starts. Closing the window closes only the view, not the box."
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
if ($Resolution -and $Resolution -notmatch '^\d+x\d+$') { throw "-Resolution '$Resolution' is not WxH" }
if ($Scale -and -not ([RigHost]::Monitors() | Where-Object { $_.Scale -eq $Scale })) {
  throw "scale $Scale % cannot be produced: the guest takes the scale of the host monitor its window is on, and the host's monitors are: $(([RigHost]::Monitors() | ForEach-Object { $_.ToString() }) -join '; ')" }
# One session at a time starts, replaces or re-attaches a box (the start lock). A box that is up with
# its client connected needs none of that: the job is queued straight away.
function Ensure-Box {
  if ((BoxUp) -and ((UserClient) -ne [IntPtr]::Zero -or (ClientWidth (HiddenClient)) -gt 700)) { return }
  $lock = $null
  try {
    $deadline = (Get-Date).AddMinutes(40)
    while (-not $lock) { $lock = Try-Lock "start.lock"; if (-not $lock) { if ((Get-Date) -gt $deadline) { throw "the rig's start lock was held for 40 min" }; Start-Sleep 1 } }
    Start-Box
  } finally { if ($lock) { $lock.Dispose() } }
}
Ensure-Box
$agent = AgentState
$proto = if ($agent -and $agent.proto) { [int]$agent.proto } else { 1 }
if ($proto -lt 2) {
  # A box whose agent an older rig.ps1 supplied: one job at a time, the display made by the caller
  # while the box is idle - as that rig.ps1 does it.
  Say "the running box has an older agent (protocol 1, started by an older rig.ps1): jobs run one at a time there, as before"
  $lock = $null
  try {
    $deadline = (Get-Date).AddMinutes(40)
    while (-not $lock) { $lock = Try-Lock "start.lock"; if (-not $lock) { if ((Get-Date) -gt $deadline) { throw "the rig's start lock was held for 40 min" }; Start-Sleep 1 } }
    if ($Fresh -and (BoxRunning)) {
      $s = AgentState
      if (-not ($s -and $s.fresh -and -not (Queued).Count)) { Say "a fresh Windows is needed - closing the running box (after its running job): $(Stop-Box)" }
      Start-Box
    }
    if ([int](AgentState).proto -lt 2) {
      if ($Resolution -or $Scale) {
        $deadline = (Get-Date).AddMinutes($Timeout + 30)
        while (((AgentState).state -ne "idle" -or (Queued).Count) -and (Get-Date) -lt $deadline) { Start-Sleep 2 }
        Say (Set-BoxDisplay $Resolution $Scale)
      } else { Say "this job names no display: under that older agent it runs under whatever the guest has now ($((AgentState).screen) - width x height @ dpi)" }
    }
  } finally { if ($lock) { $lock.Dispose() } }
  $proto = if ((AgentState).proto) { [int](AgentState).proto } else { 1 }
}

$id = "{0:yyyyMMdd-HHmmss}-{1}-{2}" -f (Get-Date), $Job, (Get-Random -Maximum 9999)
$toolsDir = if ($Tools) { $Tools } else { Join-Path $repo "tools" }
$spec = [ordered]@{ Job = $Job; TimeoutMinutes = $Timeout; Runner = (GuestPath (Join-Path $rigDir "guest\runjob.ps1")); Tools = (GuestPath $toolsDir); Proto = 2; Caller = $caller }
if (-not $spec.Tools) { throw "-Tools must be under $hostRoot" }
if ($Scale) { $spec.Scale = $Scale }
if ($Resolution) { $spec.Resolution = $Resolution }
if ($Exclusive) { $spec.Exclusive = $true }
if ($Fresh) { $spec.Fresh = $true }
if ($TestArgs.Count) { $spec.Args = $TestArgs }
if ($Registry.Count) { $spec.Registry = $Registry }
if ($RegistryKey) { $spec.RegistryKey = $RegistryKey }
if ($Warp) { $DgVoodoo["OutputAPI"] = "d3d11warp" }
if ($Sound) { $spec.Sound = $true }
if ($Job -eq "missions") { $spec.Parallel = if ($Parallel) { $Parallel } else { 4 } }
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
$specText = $spec | ConvertTo-Json -Depth 5
$q = Join-Path $io "queue\$id"
function Queue-Job { $specText | Set-Content "$q.tmp" -Encoding utf8; Move-Item "$q.tmp" "$q.json" -Force }
Queue-Job
Say "job $id queued$(if ((Queued).Count -gt 1) { " ($((Queued).Count - 1) other(s) in the queue)" })"
$jobDir = Join-Path $io "jobs\$id"; $doneFile = Join-Path $jobDir "done.json"
$queuedAt = Get-Date; $deadline = $null; $n = 0; $said = ""; $requeued = 0
while (-not (Test-Path $doneFile)) {
  Start-Sleep -Seconds 1; $n++
  if ($n % 10 -eq 0 -and -not (BoxRunning)) {
    # the box went: while the job only waited, a new box takes it (the queue is kept); a job that ran is lost
    if ((Test-Path "$q.json") -and -not (Test-Path $jobDir)) { Say "the box went away while job $id waited - starting a new one"; Ensure-Box; continue }
    throw "the box went away during job $id"
  }
  # the client must stay connected (a game cannot start in a disconnected session): when a peek window
  # was closed, or the client lost its session, the rig's own is connected again at once
  if ($n % 2 -eq 0 -and (UserClient) -eq [IntPtr]::Zero -and (ClientWidth (HiddenClient)) -le 700 -and (BoxRunning)) { Say "the box has no connected client - connecting the rig's own again"; Attach-Client }
  $s = AgentState
  if ($s -and [int]$s.proto -ge 2) { Serve-Display; Serve-Fresh; $s = AgentState }
  # an older rig.ps1 that started a new box empties the io folder: the job is queued again
  if (-not (Test-Path "$q.json") -and -not (Test-Path $jobDir)) {
    if ($requeued -ge 3) { throw "job $id disappeared from the queue three times (an older rig.ps1 starting new boxes?) - giving up" }
    $requeued++; Say "job $id is no longer in the queue nor running (the io folder was emptied - an older rig.ps1 started a new box) - queued again"; Queue-Job; continue
  }
  # where the job is: running (and how), or waiting (and why)
  $line = ""
  if (Test-Path $jobDir) {
    $r = if ($s) { @($s.running) | Where-Object { $_.id -eq $id } } else { $null }
    if ($r) { $line = "job $id running $(if ($r.slot) { "in slot $($r.slot) beside $(@($s.running).Count - 1) other job(s)" } else { "alone in the box" }), display $($r.display) %" }
    elseif (-not $s -or [int]$s.proto -lt 2) { $line = "job $id running" }
    if (-not $deadline) { $deadline = (Get-Date).AddMinutes($Timeout + 5) }
  } elseif ($s -and [int]$s.proto -ge 2) {
    $w = @($s.queue) | Where-Object { $_.id -eq $id }
    if ($w) { $line = "job $id waiting ($($w.pos) of $(@($s.queue).Count)): $($w.why)" }
  }
  if ($line -and $line -ne $said) { Say $line; $said = $line }
  if (-not (Test-Path $jobDir) -and ((Get-Date) - $queuedAt).TotalMinutes -gt $MaxWait) {
    Remove-Item "$q.json" -ErrorAction SilentlyContinue
    if (-not (Test-Path $jobDir)) { throw "job $id waited $MaxWait min without starting - withdrawn from the queue ($said)" }
  }
  if ($deadline -and (Get-Date) -gt $deadline) { New-Item -ItemType File -Force (Join-Path $jobDir "cancel") | Out-Null; Say "job ${id} over its time - cancel asked"; $deadline = (Get-Date).AddMinutes(2) }
}
Start-Sleep -Milliseconds 500
$done = Get-Content $doneFile -Raw | ConvertFrom-Json
if (-not $Out) { $Out = Join-Path $repo "build\rig\$id" }
New-Item -ItemType Directory -Force $Out | Out-Null
robocopy $jobDir $Out /E /NFL /NDL /NJH /NJS /NP | Out-Null
Remove-Item $jobDir -Recurse -Force -ErrorAction SilentlyContinue
Say "job ${id}: exit $($done.exit) in $($done.seconds) s$(if ($done.killed) { " ($($done.killed))" }) - results in $Out"
$run = Read-Json (Join-Path $Out "run.json")
if ($run) {
  Say "  ran $(if ($run.slot) { "in slot $($run.slot) (guest user $($run.user)) beside $(@($done.alongside).Count) other job(s), up to $($done.peakGames) game(s) at once" } else { "alone in the box ($($run.exclusive))" }), display $($run.display) %$(if ($done.displayChanged) { " - THE DISPLAY CHANGED DURING THE RUN: $($done.displayChanged)" }), waited $($run.waited) s$(if ($run.note) { "; note: $($run.note)" })"
}
if (Test-Path (Join-Path $Out "job.log")) { Get-Content (Join-Path $Out "job.log") | Where-Object { $_ -match "display:|failed|exit|installed|Setup exit|not run" } | ForEach-Object { "  $_" } }
if ($Job -eq "setup") {
  $s = AgentState
  if ($proto -lt 2 -or ($s -and -not @($s.running).Count -and -not (Queued).Count)) { Say "closing the box after the installer test: $(Stop-Box)" }
  else { Say "the box stays up after the installer test: other jobs run or wait in it" }
}
exit $done.exit
