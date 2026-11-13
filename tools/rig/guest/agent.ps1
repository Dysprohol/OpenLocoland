# The rig's agent: runs INSIDE the sandbox, started by its logon command (tools\rig\rig.ps1 writes
# the .wsb). It serves the jobs of every session from one queue until told to stop or idle for too
# long, so a whole round of tests is one sandbox start and one close.
#
# Several jobs run at once (protocol 2). Quiet game runs and mission runs share the box: each runs
# as a guest user of its own (rigslot1, rigslot2, ...: its own HKCU, so its own test registry key,
# its own %TEMP%), in its own copy of the game folder, the game on a desktop of its own as
# test_game.ps1 runs quietly. Up to MaxJobs jobs and MaxGames games run at once. What the guest has
# only once runs alone (exclusive): the installer (it also needs a fresh box), a script job (it may
# move the real cursor or change the guest), a visible game run (the guest's own desktop, real keys,
# full screen), a job with its sound on (one sound device), a job that keeps an earlier job's
# registry, and any job queued with -Exclusive. The guest has ONE display, which only the host can
# change (rig.ps1 sizes the box's window): every job runs under the display it asked for - a job
# that names none under the default (the box's starting size at 100 %) - so a job that needs another
# display waits until no job runs, the agent asks the host for it (display-want.json; any waiting
# rig.ps1 makes it) and starts the job once the guest has it.
# Turns go round-robin between callers (the worktree a job came from), the oldest job of the caller
# served longest ago first; the job whose turn it is decides - while it waits (for the box to empty,
# for its display, for room), no later job overtakes it.
#
#   C:\rig\io\queue\<id>.json     a job waiting (rig.ps1 writes it as .tmp and renames it)
#   C:\rig\io\jobs\<id>\          the running / finished job: job.json, run.json (where and how it
#                                 runs), runner output, results, done.json at the end
#   C:\rig\io\jobs\<id>\cancel    rig.ps1 asks for the job to be killed (timeout, or by hand)
#   C:\rig\io\stop                rig.ps1 asks the agent to shut the sandbox down: no new job starts,
#                                 the running ones finish (each within its own timeout)
#   C:\rig\io\agent\state.json    heartbeat: running jobs, the queue with why each one waits, display
#   C:\rig\io\agent\display-want.json / display-done.json   the display asked of the host / its answer
#   C:\rig\io\agent\screen.png    the guest's screen, every few seconds while a job runs (to look in)
#   C:\rig\io\agent\agent.log
# -NoShutdown: end the agent without shutting the guest down (a nested test agent, guest\exp_nested.ps1)
param([string]$Io = "C:\rig\io", [switch]$NoShutdown)
$ErrorActionPreference = "Continue"
$Proto = 2
$agentDir = Join-Path $Io "agent"; $queue = Join-Path $Io "queue"; $jobs = Join-Path $Io "jobs"
New-Item -ItemType Directory -Force $agentDir, $queue, $jobs | Out-Null
$config = @{ IdleMinutes = 10; ScreenSeconds = 5; MaxGames = 6; MaxJobs = 4; DefaultResolution = ""; MinFreeGB = 6 }
$cfgFile = Join-Path $agentDir "config.json"
if (Test-Path $cfgFile) { (Get-Content $cfgFile -Raw | ConvertFrom-Json).PSObject.Properties | ForEach-Object { $config[$_.Name] = $_.Value } }
$logFile = Join-Path $agentDir "agent.log"
function Log($s) { Add-Content -Path $logFile -Value "$(Get-Date -Format 'HH:mm:ss.fff') $s" -Encoding utf8 }
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
. (Join-Path $PSScriptRoot "display.ps1")   # the heartbeat carries the guest's screen (rig.ps1 sizes the window by it)
. (Join-Path $PSScriptRoot "audio.ps1")     # the guest's sound is muted: it would come out of the host's speakers
$runner = Join-Path $PSScriptRoot "runjob.ps1"   # always the agent's own runner (a job's "Runner" is ignored)
function Save-Screen {
  try {
    $b = [System.Windows.Forms.SystemInformation]::VirtualScreen
    $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
    $g = [Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size); $g.Dispose()
    $tmp = Join-Path $agentDir "screen.tmp.png"; $bmp.Save($tmp, [Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    Move-Item $tmp (Join-Path $agentDir "screen.png") -Force
  } catch { }
}
function WriteJson($file, $obj) { $tmp = "$file.tmp"; ($obj | ConvertTo-Json -Depth 6 -Compress) | Set-Content $tmp -Encoding utf8; Move-Item $tmp $file -Force }
# The display as "WxH@<scale %>" (the heartbeat's "screen" stays "WxH@<dpi>" for older rig.ps1)
function ScreenKey { $s = [GuestDisplay]::Screen(); $r, $d = $s.Split("@"); "$r@$([int]([int]$d * 100 / 96))" }
$boot = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
# The default display: the size the box had when it started (or the configured one), at 100 %.
$startScreen = ScreenKey
$defRes = if ($config.DefaultResolution) { $config.DefaultResolution } else { $startScreen.Split("@")[0] }
$defaultKey = "$defRes@100"
$lastKey = ScreenKey; $keySince = (Get-Date).AddSeconds(-10)   # the display and since when the guest has had it
# The slot users' game copies live under C:\work, which they may write to.
New-Item -ItemType Directory -Force "C:\work\games" | Out-Null
& icacls C:\work /grant "*S-1-5-32-545:(OI)(CI)M" /Q 2>&1 | Out-Null   # Users: modify

$done = 0; $lastWork = Get-Date
$running = [ordered]@{}        # id -> the running job
$slots = @{}                   # n -> @{ User; Cred; Job }
$lastServed = @{}              # caller -> when its last job started
$waitWhy = @{}                 # queued id -> why it waits
$parsed = @{}                  # queue file -> its parsed job (files are only ever renamed in whole)
$want = $null; $wantN = 0      # the display asked of the host
$needFresh = $null             # the job that waits for the host to restart the box
$notes = @{}                   # id -> a note for run.json (what was allowed and why)

function Slot-User([int]$n) {
  if ($slots[$n]) { return $slots[$n] }
  $u = "rigslot$n"
  # a throw-away guest account of this box only; its password lives in this process alone
  $pw = (-join ((65..90) + (97..122) + (48..57) | Get-Random -Count 24 | ForEach-Object { [char]$_ })) + "-a1"
  $o = (& net user $u $pw /add /y 2>&1) -join " "
  if ($LASTEXITCODE -ne 0) { $o = (& net user $u $pw 2>&1) -join " " }
  $cred = New-Object Management.Automation.PSCredential($u, (ConvertTo-SecureString $pw -AsPlainText -Force))
  $slots[$n] = @{ User = $u; Cred = $cred; Job = $null }
  Log "slot ${n}: guest user $u ready ($o)"
  return $slots[$n]
}
# Ends whatever a slot's user still runs (a lane, a game that started itself again): as that user,
# since the agent's own user cannot end another user's processes without elevation.
function Clear-Slot([int]$n) {
  $s = $slots[$n]; if (-not $s) { return }
  try {
    $p = Start-Process taskkill -Credential $s.Cred -ArgumentList @("/F", "/FI", "`"USERNAME eq $($s.User)`"") -WorkingDirectory C:\work -WindowStyle Hidden -PassThru `
           -RedirectStandardOutput "C:\work\kill-$n.txt" -RedirectStandardError "C:\work\kill-$n.err.txt"
    $null = $p.Handle; [void]$p.WaitForExit(20000)
  } catch { Log "slot ${n}: clearing its processes failed: $($_.Exception.Message)" }
}

function Caller-Of($spec) {
  if ($spec.Caller) { return [string]$spec.Caller }
  if ($spec.Tools -match '\\worktrees\\([^\\]+)\\') { return $Matches[1] }
  return "main"
}
function Classify($spec) {
  $a = $spec.Args
  $excl = ""
  switch ($spec.Job) {
    "setup" { $excl = "the installer (a fresh box of its own)" }
    "script" { if (-not $spec.Shared) { $excl = "a script job (it may use the real cursor or change the guest)" } }
  }
  if (-not $excl -and $spec.Job -eq "game" -and $a -and ($a.Visible -or $a.Screen -or "$($a.SetFull)" -eq "1" -or "$($a.Steps)" -match '(^|,)\s*[AQ]')) {
    $excl = "a visible game run (the guest's own desktop, real keys or full screen)" }
  if (-not $excl -and $spec.Sound) { $excl = "its sound is on (one sound device for the whole guest)" }
  if (-not $excl -and $spec.KeepRegistry) { $excl = "it keeps the registry an earlier job left" }
  if (-not $excl -and $spec.Exclusive) { $excl = "asked to run alone (-Exclusive)" }
  $games = 1
  if ($spec.Job -eq "probe") { $games = 0 }
  if ($spec.Job -eq "missions") {
    $n = if ($a -and $a.Missions) { @($a.Missions).Count } elseif ($a -and $a.Quick) { 3 } else { 20 }
    $lanes = if ($spec.Parallel) { [int]$spec.Parallel } else { 1 }
    $games = [Math]::Max(1, [Math]::Min($lanes, $n))
  }
  $res = if ($spec.Resolution) { [string]$spec.Resolution } else { $defRes }
  $scale = if ($spec.Scale) { [int]$spec.Scale } else { 100 }
  @{ Excl = $excl; Games = $games; Display = "$res@$scale"; Res = $res; Scale = $scale; Named = [bool]($spec.Resolution -or $spec.Scale)
     Fresh = ($spec.Job -eq "setup" -or [bool]$spec.Fresh); Caller = (Caller-Of $spec); Legacy = (-not $spec.Proto) }
}
function Read-Queue {
  $list = @()
  foreach ($f in @(Get-ChildItem $queue -Filter *.json -ErrorAction SilentlyContinue | Sort-Object Name)) {
    if (-not $parsed.ContainsKey($f.Name)) {
      try { $spec = Get-Content $f.FullName -Raw | ConvertFrom-Json } catch { continue }
      $parsed[$f.Name] = @{ Id = $f.BaseName; File = $f.FullName; Spec = $spec; Cls = (Classify $spec); Since = Get-Date }
    }
    $list += $parsed[$f.Name]
  }
  foreach ($k in @($parsed.Keys)) { if (-not ($list | Where-Object { $_.File -like "*\$k" })) { $parsed.Remove($k) } }
  return , $list
}
function Box-Fresh { return ($done -eq 0 -and $running.Count -eq 0) }
function Games-Running { $t = 0; foreach ($r in $running.Values) { $t += $r.Games }; $t }
# The job whose turn it is: on a fresh box a job that needs one; otherwise the oldest job of the
# caller whose last job started longest ago (a caller never served comes first).
function Pick($waiting) {
  if (Box-Fresh) { $f = $waiting | Where-Object { $_.Cls.Fresh } | Select-Object -First 1; if ($f) { return $f } }
  $best = $null; $bestKey = $null
  foreach ($w in $waiting) {
    $served = if ($lastServed.ContainsKey($w.Cls.Caller)) { $lastServed[$w.Cls.Caller] } else { [datetime]::MinValue }
    $key = "{0:yyyyMMddHHmmssfff}|{1}" -f $served, $w.Id
    if (-not $best -or [string]::CompareOrdinal($key, $bestKey) -lt 0) { $best = $w; $bestKey = $key }
  }
  return $best
}
# Why the job cannot start now ("" = it can). May ask the host for a display or a fresh box.
function Blocked($c) {
  $cls = $c.Cls; $n = $running.Count
  $script:needFresh = $null
  if ($cls.Fresh -and -not (Box-Fresh)) {
    if ($n) { return "needs a fresh box: waits for $n running job(s) to end, then the box is restarted" }
    $script:needFresh = $c.Id
    if ($cls.Legacy -and ((Get-Date) - $c.Since).TotalSeconds -gt 90) {
      $notes[$c.Id] = "not a fresh box: an older rig.ps1 queued it and no host script restarted the box"; return "" }
    return "needs a fresh box: waiting for a host script (rig.ps1) to restart the box"
  }
  if ($cls.Excl) { if ($n) { return "runs alone - $($cls.Excl): waits for $n running job(s) to end" } }
  else {
    $held = $running.Values | Where-Object { $_.Excl } | Select-Object -First 1
    if ($held) { return "the box is held by $($held.Id) (runs alone)" }
    if ($n -ge $config.MaxJobs) { return "all $($config.MaxJobs) job slots are busy" }
    $g = Games-Running
    if ($n -and $g + $cls.Games -gt $config.MaxGames) { return "$g of $($config.MaxGames) games running, it needs $($cls.Games)" }
  }
  $now = ScreenKey
  $ans = $null; try { $ans = Get-Content (Join-Path $agentDir "display-done.json") -Raw -ErrorAction Stop | ConvertFrom-Json } catch { }
  $answered = [bool]($want -and $ans -and [int]$ans.n -eq $want.n)
  if ($now -ne $cls.Display) {
    if ($n) { return "needs the display $($cls.Display) % (now $now %): waits for $n running job(s) to end" }
    if ($answered -and -not $ans.ok) { return "FAIL:the display $($cls.Display) % cannot be produced: $($ans.text)" }
    # asked anew when nothing was asked for it yet, or the host said it was made and the guest drifted since
    if (-not $want -or $want.Display -ne $cls.Display -or $answered) {
      $script:wantN++
      $script:want = @{ n = $wantN; res = $cls.Res; scale = $cls.Scale; Display = $cls.Display; for = $c.Id; time = (Get-Date).ToString("o"); Since = Get-Date }
      WriteJson (Join-Path $agentDir "display-want.json") ([ordered]@{ n = $want.n; res = $want.res; scale = $want.scale; for = $want.for; time = $want.time })
      Log "display $($cls.Display) % asked of the host for $($c.Id) (now $now %)"
    }
    $age = ((Get-Date) - $want.Since).TotalSeconds
    if ($cls.Legacy -and -not $cls.Named -and $age -gt 15) {
      $notes[$c.Id] = "display left at $now % (an older rig.ps1 queued it, named no display, and no host script set the default)"; return "" }
    if ($cls.Legacy -and $cls.Named -and $age -gt 120) {
      return "FAIL:the display $($cls.Display) % was not made: an older rig.ps1 queued the job and no newer host script was waiting to set it - run it again" }
    return "waits for the host to set the display $($cls.Display) % (now $now %)"
  }
  # The display is right. When the host was asked for it, its answer counts (it answers once the guest
  # has kept the display for 3 s); otherwise the display must have stayed so for 2 s - a job never
  # starts on a size the client passes through.
  if (-not $n) {
    if ($want -and $want.Display -eq $cls.Display) {
      if (-not $answered) {
        if (-not ($cls.Legacy -and ((Get-Date) - $want.Since).TotalSeconds -gt 15)) { return "waits for the host to finish setting the display $($cls.Display) %" } }
      elseif (-not $ans.ok) { return "FAIL:the display $($cls.Display) % cannot be produced: $($ans.text)" }
    }
    if (((Get-Date) - $script:keySince).TotalSeconds -lt 2) { return "waits for the display $($cls.Display) % to stay put" }
  }
  return ""
}
# A shared job that may start right now beside the running ones, with no display change and no
# waiting for the box to empty (see the queue loop: jobs that start while an earlier turn waits).
function Fits($o) {
  if ($o.Cls.Excl -or $o.Cls.Fresh -or $o.Cls.Display -ne (ScreenKey)) { return $false }
  if ($running.Values | Where-Object { $_.Excl }) { return $false }
  if ($running.Count -ge $config.MaxJobs) { return $false }
  return ((Games-Running) + $o.Cls.Games -le $config.MaxGames)
}
function Fail-Queued($c, [string]$why) {
  $dir = Join-Path $jobs $c.Id; New-Item -ItemType Directory -Force $dir | Out-Null
  Move-Item $c.File (Join-Path $dir "job.json") -Force -ErrorAction SilentlyContinue
  "$(Get-Date -Format 'HH:mm:ss.fff') not run: $why" | Add-Content (Join-Path $dir "job.log") -Encoding utf8
  WriteJson (Join-Path $dir "done.json") ([ordered]@{ exit = 3; seconds = 0; killed = "not run: $why"; uptime = [int]((Get-Date) - $boot).TotalSeconds })
  Log "job $($c.Id) not run: $why"
  $script:done++; $script:lastWork = Get-Date
}
# Keeps at least MinFreeGB free on the guest's disk: the copies of game folders no running job uses
# are removed, least recently used first (a later job with the same build copies it again).
function Free-Disk {
  $free = (Get-PSDrive C).Free / 1GB
  if ($free -ge $config.MinFreeGB) { return }
  $busy = @($slots.Keys | Where-Object { $slots[$_].Job } | ForEach-Object { "-s$_" })
  $old = Get-ChildItem "C:\work\games" -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime
  foreach ($d in $old) {
    if ((Get-PSDrive C).Free / 1GB -ge $config.MinFreeGB) { break }
    if ($busy | Where-Object { $d.Name -like "*$_" -or $d.Name -like "*$_-lane*" }) { continue }
    if (-not $busy.Count -or $d.Name -match '-s\d+') { Remove-Item $d.FullName -Recurse -Force -ErrorAction SilentlyContinue; Log "disk: removed the unused game copy $($d.Name)" }
  }
}
function Start-Run($c) {
  $cls = $c.Cls; $id = $c.Id
  $muteLine = Set-GuestMute $true   # every job starts muted (also after someone unmuted by hand while looking in)
  Free-Disk
  $slot = 0; $user = $env:USERNAME
  if (-not $cls.Excl) {
    $slot = 1; while ($slots[$slot] -and $slots[$slot].Job) { $slot++ }
    $s = Slot-User $slot; $s.Job = $id; $user = $s.User
  }
  $dir = Join-Path $jobs $id; New-Item -ItemType Directory -Force $dir | Out-Null
  # The heartbeat says "running" BEFORE the job leaves the queue: an older rig.ps1 changes the display
  # when it sees the agent idle and the queue empty, and must never see both while a job starts.
  $script:starting = $id; Beat "running"; Start-Sleep -Milliseconds 300
  Move-Item $c.File (Join-Path $dir "job.json") -Force
  "$(Get-Date -Format 'HH:mm:ss') before the job: $muteLine" | Add-Content (Join-Path $dir "audio.txt") -Encoding utf8
  $with = @($running.Keys)
  $info = [ordered]@{ slot = $slot; user = $user; caller = $cls.Caller; exclusive = $cls.Excl; display = (ScreenKey); asked = $cls.Display
    default = $defaultKey; games = $cls.Games; gamesRunning = (Games-Running); alongside = $with; limits = "$($config.MaxJobs) jobs, $($config.MaxGames) games"
    waited = [int]((Get-Date) - $c.Since).TotalSeconds; legacy = $cls.Legacy; note = $notes[$id]; started = (Get-Date).ToString("o") }
  WriteJson (Join-Path $dir "run.json") $info
  $timeout = if ($c.Spec.TimeoutMinutes) { [double]$c.Spec.TimeoutMinutes } else { 30 }
  $argList = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$runner`"", "-Dir", "`"$dir`"", "-Fresh", $(if ($done -eq 0 -and -not $running.Count) { "1" } else { "0" }), "-Slot", "$slot")
  $out = Join-Path $dir "runner.txt"; $err = Join-Path $dir "runner.err.txt"
  try {
    if ($slot) { $p = Start-Process powershell -Credential $slots[$slot].Cred -LoadUserProfile -WorkingDirectory C:\work -ArgumentList $argList -RedirectStandardOutput $out -RedirectStandardError $err -PassThru -WindowStyle Hidden }
    else { $p = Start-Process powershell -ArgumentList $argList -RedirectStandardOutput $out -RedirectStandardError $err -PassThru -WindowStyle Minimized }
    $null = $p.Handle   # keeps the process handle, without it ExitCode stays empty after a redirected start
  } catch {
    Log "job $id could not start: $($_.Exception.Message)"
    if ($slot) { $slots[$slot].Job = $null }
    WriteJson (Join-Path $dir "done.json") ([ordered]@{ exit = 3; seconds = 0; killed = "could not start: $($_.Exception.Message)"; uptime = [int]((Get-Date) - $boot).TotalSeconds })
    $script:done++; return
  }
  foreach ($r in $running.Values) { $r.With += $id }
  $running[$id] = @{ Id = $id; Spec = $c.Spec; Cls = $cls; Proc = $p; Slot = $slot; User = $user; Games = $cls.Games; Excl = $cls.Excl; Dir = $dir
                     Start = Get-Date; Timeout = $timeout; Why = ""; With = $with; Peak = (Games-Running) + $cls.Games; Display = (ScreenKey); Changed = ""; LastSessions = "" }
  $lastServed[$cls.Caller] = Get-Date
  $script:lastWork = Get-Date
  Log "job $id ($($c.Spec.Job), $($cls.Caller)) started in slot $slot as $user, display $(ScreenKey) %, $($cls.Games) game(s), alongside $($with.Count) job(s)$(if ($cls.Excl) { ", alone: $($cls.Excl)" }), timeout $timeout min"
}
function Finish-Run($r) {
  $p = $r.Proc
  if ($r.Why) { try { $p.Kill() } catch { }; & taskkill /PID $p.Id /T /F 2>&1 | Out-Null }
  [void]$p.WaitForExit(10000)
  if ($r.Slot) { Clear-Slot $r.Slot; $slots[$r.Slot].Job = $null }
  elseif ($r.Why) { Get-Process Locoland, Setup -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue }   # it ran alone
  "$(Get-Date -Format 'HH:mm:ss') after the job: $(Set-GuestMute $true)" | Add-Content (Join-Path $r.Dir "audio.txt") -Encoding utf8
  $code = if ($r.Why) { 124 } else { $p.ExitCode }
  $secs = [int]((Get-Date) - $r.Start).TotalSeconds
  if (-not $running.Count -or $running.Count -eq 1) { Save-Screen }
  WriteJson (Join-Path $r.Dir "done.json") ([ordered]@{ exit = $code; seconds = $secs; killed = $r.Why; uptime = [int]((Get-Date) - $boot).TotalSeconds
    slot = $r.Slot; user = $r.User; display = $r.Display; displayChanged = $r.Changed; alongside = @($r.With | Select-Object -Unique); peakGames = $r.Peak })
  Log "job $($r.Id) ended: exit $code in $secs s $($r.Why)$(if ($r.Changed) { " - DISPLAY CHANGED: $($r.Changed)" })"
  $running.Remove($r.Id)
  $script:done++; $script:lastWork = Get-Date
}
function Beat($state) {
  $q = @(); $i = 0
  foreach ($w in $script:waitingNow) { $i++; $q += [ordered]@{ id = $w.Id; caller = $w.Cls.Caller; pos = $i; games = $w.Cls.Games; display = $w.Cls.Display; alone = $w.Cls.Excl; why = $waitWhy[$w.Id] } }
  $rl = @(foreach ($r in $running.Values) { [ordered]@{ id = $r.Id; caller = $r.Cls.Caller; slot = $r.Slot; games = $r.Games; alone = $r.Excl; display = $r.Display; seconds = [int]((Get-Date) - $r.Start).TotalSeconds; timeout = $r.Timeout } })
  $first = if ($running.Count) { @($running.Keys)[0] } else { $null }
  $s = [ordered]@{ time = (Get-Date).ToString("o"); state = $state; job = $first; done = $done; fresh = (Box-Fresh)
                   uptime = [int]((Get-Date) - $boot).TotalSeconds; pid = $PID; screen = [GuestDisplay]::Screen()
                   proto = $Proto; display = (ScreenKey); default = $defaultKey; maxJobs = $config.MaxJobs; maxGames = $config.MaxGames
                   games = (Games-Running); running = $rl; queue = $q; needFresh = $needFresh; want = $(if ($want) { $want.n } else { 0 }) }
  WriteJson (Join-Path $agentDir "state.json") $s
}

Log "sound: $(Set-GuestMute $true)"
Log "agent up (protocol $Proto); uptime $([int]((Get-Date) - $boot).TotalSeconds) s; idle limit $($config.IdleMinutes) min; at most $($config.MaxJobs) jobs and $($config.MaxGames) games at once; default display $defaultKey %; disk free $([int]((Get-PSDrive C).Free / 1GB)) GB"
$script:waitingNow = @()
Beat "idle"
$nextShot = Get-Date
while ($true) {
  $stop = Test-Path (Join-Path $Io "stop")
  # the running jobs: ended, cancelled, over their time
  foreach ($r in @($running.Values)) {
    if ($r.Proc.HasExited) { Finish-Run $r; continue }
    if (Test-Path (Join-Path $r.Dir "cancel")) { $r.Why = "cancelled" }
    elseif (((Get-Date) - $r.Start).TotalMinutes -ge $r.Timeout) { $r.Why = "timeout after $($r.Timeout) min" }
    if ($r.Why) { Log "job $($r.Id): $($r.Why) - killing its processes (slot $($r.Slot) only)"; Finish-Run $r }
  }
  # what the running jobs run under: the display (a peek window, or an older rig.ps1, can change it)
  $now = ScreenKey
  if ($now -ne $script:lastKey) { $script:lastKey = $now; $script:keySince = Get-Date }
  foreach ($r in $running.Values) { if ($now -ne $r.Display -and -not $r.Changed) { $r.Changed = "$(Get-Date -Format 'HH:mm:ss') the guest became $now %"; Log "job $($r.Id): the display changed under it ($($r.Display) -> $now)" } }
  if ($running.Count -and (Get-Date) -ge $nextShot) {
    $nextShot = (Get-Date).AddSeconds([Math]::Max(1, $config.ScreenSeconds))
    if ($config.ScreenSeconds -gt 0) { Save-Screen }
    # the guest's memory (for the box's memory size): in use, committed, the games' working sets
    $os = Get-CimInstance Win32_OperatingSystem
    $gs = @(Get-CimInstance Win32_Process -Filter "Name='Locoland.exe' or Name='Setup.exe'" | ForEach-Object { [int]($_.WorkingSetSize / 1MB) })
    $line = "$(Get-Date -Format 'HH:mm:ss') used $([int](($os.TotalVisibleMemorySize - $os.FreePhysicalMemory) / 1KB)) MB, committed $([int](($os.TotalVirtualMemorySize - $os.FreeVirtualMemory) / 1KB)) MB, games $($gs -join '+') MB, jobs $($running.Count)"
    # which processes have a sound stream open (the game's sound runs, only into a muted endpoint)
    $sess = Get-AudioSessions
    foreach ($r in $running.Values) {
      if ($gs.Count -gt $r.Peak) { $r.Peak = $gs.Count }
      $line | Add-Content (Join-Path $r.Dir "memory.txt") -Encoding utf8
      if ($sess -ne $r.LastSessions) { "$(Get-Date -Format 'HH:mm:ss') audio streams: $sess" | Add-Content (Join-Path $r.Dir "audio.txt") -Encoding utf8; $r.LastSessions = $sess }
    }
  }
  # the queue: start what may start, in turn
  $waiting = Read-Queue
  $waitWhy.Clear()
  if ($stop) { foreach ($w in $waiting) { $waitWhy[$w.Id] = "the box is stopping" } }
  else {
    while ($waiting.Count) {
      $c = Pick $waiting
      $why = Blocked $c
      if ($why.StartsWith("FAIL:")) { Fail-Queued $c $why.Substring(5) }
      elseif (-not $why) { Start-Run $c }
      else {
        $waitWhy[$c.Id] = $why
        # While the job whose turn it is waits for the box to empty (another display, to run alone, a
        # fresh box), the jobs queued BEFORE it that fit the display and the room start meanwhile:
        # jobs of one display go together, and the waiting job is held up by no job newer than itself.
        if ($running.Count -and ($c.Cls.Excl -or $c.Cls.Fresh -or $now -ne $c.Cls.Display)) {
          foreach ($o in @($waiting | Where-Object { $_.Id -ne $c.Id -and [string]::CompareOrdinal($_.Id, $c.Id) -lt 0 } | Sort-Object Id)) {
            if (Fits $o) { $notes[$o.Id] = "started while $($c.Id) (its turn first) waited for the box to empty: queued before it, same display"; Start-Run $o
                           $waiting = @($waiting | Where-Object { $_.Id -ne $o.Id }) }
          }
        }
        foreach ($w in $waiting) { if ($w.Id -ne $c.Id) { $waitWhy[$w.Id] = "waits its turn: $($c.Id) goes first" } }
        break
      }
      $waiting = @($waiting | Where-Object { $_.Id -ne $c.Id })
      if ($want -and $running.Count) { $want = $null; Remove-Item (Join-Path $agentDir "display-want.json") -ErrorAction SilentlyContinue }
    }
    if (-not $waiting.Count) { $needFresh = $null }
  }
  $script:waitingNow = $waiting
  if ($running.Count -or $waiting.Count) { $lastWork = Get-Date }
  if ($stop -and -not $running.Count) { Log "stop asked"; break }
  if (((Get-Date) - $lastWork).TotalMinutes -ge $config.IdleMinutes) { Log "idle for $($config.IdleMinutes) min - shutting down"; break }
  Beat $(if ($running.Count) { "running" } else { "idle" })
  Start-Sleep -Milliseconds 500
}
Beat "stopping"
if ($NoShutdown) { Log "agent ends (-NoShutdown)"; foreach ($n in @($slots.Keys)) { Clear-Slot $n }; return }
Log "shutting down"
shutdown /s /t 1 /f
