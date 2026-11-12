# The rig's agent: runs INSIDE the sandbox, started by its logon command (tools\rig\rig.ps1 writes
# the .wsb). It serves jobs one after another until told to stop or idle for too long, so a whole
# round of tests is one sandbox start and one close.
#
#   C:\rig\io\queue\<id>.json     a job waiting (rig.ps1 writes it as .tmp and renames it)
#   C:\rig\io\jobs\<id>\          the running / finished job: job.json, runner output, results,
#                                 done.json at the end ({exit, seconds})
#   C:\rig\io\jobs\<id>\cancel    rig.ps1 asks for the job to be killed (timeout, or by hand)
#   C:\rig\io\stop                rig.ps1 asks the agent to shut the sandbox down
#   C:\rig\io\agent\state.json    heartbeat: state, current job, jobs done, fresh (no job run yet)
#   C:\rig\io\agent\screen.png    the guest's screen, every few seconds while a job runs (to look in)
#   C:\rig\io\agent\agent.log
param([string]$Io = "C:\rig\io")
$ErrorActionPreference = "Continue"
$agentDir = Join-Path $Io "agent"; $queue = Join-Path $Io "queue"; $jobs = Join-Path $Io "jobs"
New-Item -ItemType Directory -Force $agentDir, $queue, $jobs | Out-Null
$config = @{ IdleMinutes = 10; ScreenSeconds = 5 }
$cfgFile = Join-Path $agentDir "config.json"
if (Test-Path $cfgFile) { (Get-Content $cfgFile -Raw | ConvertFrom-Json).PSObject.Properties | ForEach-Object { $config[$_.Name] = $_.Value } }
$logFile = Join-Path $agentDir "agent.log"
function Log($s) { Add-Content -Path $logFile -Value "$(Get-Date -Format 'HH:mm:ss.fff') $s" -Encoding utf8 }
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
. (Join-Path $PSScriptRoot "display.ps1")   # the heartbeat carries the guest's screen (rig.ps1 sizes the window by it)
. (Join-Path $PSScriptRoot "audio.ps1")     # the guest's sound is muted: it would come out of the host's speakers
function Save-Screen {
  try {
    $b = [System.Windows.Forms.SystemInformation]::VirtualScreen
    $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height
    $g = [Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size); $g.Dispose()
    $tmp = Join-Path $agentDir "screen.tmp.png"; $bmp.Save($tmp, [Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
    Move-Item $tmp (Join-Path $agentDir "screen.png") -Force
  } catch { }
}
$boot = (Get-CimInstance Win32_OperatingSystem).LastBootUpTime
$done = 0; $current = $null; $lastWork = Get-Date
function Beat($state) {
  $s = [ordered]@{ time = (Get-Date).ToString("o"); state = $state; job = $current; done = $done; fresh = ($done -eq 0 -and -not $current)
                   uptime = [int]((Get-Date) - $boot).TotalSeconds; pid = $PID; screen = [GuestDisplay]::Screen() }
  $tmp = Join-Path $agentDir "state.tmp"; ($s | ConvertTo-Json -Compress) | Set-Content $tmp -Encoding utf8
  Move-Item $tmp (Join-Path $agentDir "state.json") -Force
}
Log "sound: $(Set-GuestMute $true)"
Log "agent up; uptime $([int]((Get-Date) - $boot).TotalSeconds) s; idle limit $($config.IdleMinutes) min"
Beat "idle"
while ($true) {
  if (Test-Path (Join-Path $Io "stop")) { Log "stop asked"; break }
  if (((Get-Date) - $lastWork).TotalMinutes -ge $config.IdleMinutes) { Log "idle for $($config.IdleMinutes) min - shutting down"; break }
  $next = Get-ChildItem $queue -Filter *.json -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -First 1
  if (-not $next) { Beat "idle"; Start-Sleep -Milliseconds 700; continue }
  $id = $next.BaseName; $current = $id
  $dir = Join-Path $jobs $id; New-Item -ItemType Directory -Force $dir | Out-Null
  Move-Item $next.FullName (Join-Path $dir "job.json") -Force
  $job = Get-Content (Join-Path $dir "job.json") -Raw | ConvertFrom-Json
  $timeout = if ($job.TimeoutMinutes) { [double]$job.TimeoutMinutes } else { 30 }
  Log "job $id ($($job.Job)) started, timeout $timeout min"
  # every job starts muted (also after someone unmuted by hand while looking in); -Sound unmutes in runjob.ps1
  $muteLine = Set-GuestMute $true
  "$(Get-Date -Format 'HH:mm:ss') before the job: $muteLine" | Add-Content (Join-Path $dir "audio.txt") -Encoding utf8
  $lastSessions = ""
  Beat "running"
  $t = Get-Date
  $runner = if ($job.Runner) { $job.Runner } else { Join-Path $PSScriptRoot "runjob.ps1" }
  $p = Start-Process powershell -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$runner`"", "-Dir", "`"$dir`"", "-Fresh", $(if ($done -eq 0) { "1" } else { "0" })) `
         -RedirectStandardOutput (Join-Path $dir "runner.txt") -RedirectStandardError (Join-Path $dir "runner.err.txt") -PassThru -WindowStyle Minimized
  $null = $p.Handle   # keeps the process handle, without it ExitCode stays empty after a redirected start
  $why = ""; $nextShot = Get-Date
  while (-not $p.HasExited) {
    Start-Sleep -Milliseconds 500
    if ($config.ScreenSeconds -gt 0 -and (Get-Date) -ge $nextShot) {
      Save-Screen; $nextShot = (Get-Date).AddSeconds($config.ScreenSeconds)
      # which processes have a sound stream open (the game's sound runs, only into a muted endpoint)
      # the guest's memory (for the box's memory size): in use, committed, the games' working sets
      $os = Get-CimInstance Win32_OperatingSystem
      $games = @(Get-Process Locoland, Setup -ErrorAction SilentlyContinue | ForEach-Object { [int]($_.WorkingSet64 / 1MB) })
      "$(Get-Date -Format 'HH:mm:ss') used $([int](($os.TotalVisibleMemorySize - $os.FreePhysicalMemory) / 1KB)) MB, committed $([int](($os.TotalVirtualMemorySize - $os.FreeVirtualMemory) / 1KB)) MB, games $($games -join '+') MB" |
        Add-Content (Join-Path $dir "memory.txt") -Encoding utf8
      $sess = Get-AudioSessions
      if ($sess -ne $lastSessions) { "$(Get-Date -Format 'HH:mm:ss') audio streams: $sess" | Add-Content (Join-Path $dir "audio.txt") -Encoding utf8; $lastSessions = $sess }
    }
    Beat "running"
    if (Test-Path (Join-Path $dir "cancel")) { $why = "cancelled" }
    elseif (((Get-Date) - $t).TotalMinutes -ge $timeout) { $why = "timeout after $timeout min" }
    elseif (Test-Path (Join-Path $Io "stop")) { $why = "stop asked" }
    if ($why) {
      Log "job ${id}: $why - killing its processes"
      & taskkill /PID $p.Id /T /F 2>&1 | Out-Null
      Get-Process Locoland, Setup -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
      break
    }
  }
  $p.WaitForExit(10000) | Out-Null
  "$(Get-Date -Format 'HH:mm:ss') after the job: $(Set-GuestMute $true)" | Add-Content (Join-Path $dir "audio.txt") -Encoding utf8
  $code = if ($why) { 124 } else { $p.ExitCode }
  $secs = [int]((Get-Date) - $t).TotalSeconds
  Save-Screen
  ([ordered]@{ exit = $code; seconds = $secs; killed = $why; uptime = [int]((Get-Date) - $boot).TotalSeconds } | ConvertTo-Json -Compress) | Set-Content (Join-Path $dir "done.json") -Encoding utf8
  Log "job $id ended: exit $code in $secs s $why"
  $done++; $current = $null; $lastWork = Get-Date
  Beat "idle"
}
Beat "stopping"
Log "shutting down"
shutdown /s /t 1 /f
