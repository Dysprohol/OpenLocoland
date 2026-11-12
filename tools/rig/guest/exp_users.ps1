# Experiment (rig -Job script): can games run side by side as separate guest users, each with a
# registry (HKCU) of its own? Two local users, each runs test_game.ps1 quietly at the same time with a
# different language; the script reports who ran, what each wrote, whether the mapped io folder is
# writable for them and how long the user/profile set-up took.
param([string]$Dir, [string]$Game)
function L($s) { "$(Get-Date -Format 'HH:mm:ss.fff') $s" }
$tools = Split-Path (Split-Path $PSScriptRoot)
L "whoami: $(whoami); elevated: $(([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole('Administrators'))"
L "seclogon: $((Get-Service seclogon).Status) / $((Get-Service seclogon).StartType)"
New-Item -ItemType Directory -Force C:\work | Out-Null
& icacls C:\work /grant "*S-1-5-32-545:(OI)(CI)M" /Q | Out-Null
$slots = @(@{ U = "rigslot1"; Lang = "pl" }, @{ U = "rigslot2"; Lang = "en" })
$procs = @()
foreach ($s in $slots) {
  $pw = (-join ((65..90) + (97..122) + (48..57) | Get-Random -Count 24 | ForEach-Object { [char]$_ })) + "-a1"
  $t = Get-Date
  $o = (& net user $s.U $pw /add /y 2>&1) -join " "
  L "net user $($s.U): $o ($([int]((Get-Date) - $t).TotalMilliseconds) ms)"
  $cred = New-Object Management.Automation.PSCredential($s.U, (ConvertTo-SecureString $pw -AsPlainText -Force))
  $w = "C:\work\$($s.U)"; New-Item -ItemType Directory -Force $w | Out-Null
  $t = Get-Date
  robocopy $Game "$w\game" /MIR /NFL /NDL /NJH /NJS /NP /MT:16 | Out-Null
  L "copy of the game for $($s.U): $([int]((Get-Date) - $t).TotalSeconds) s, $([int]((Get-ChildItem "$w\game" -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)) MB"
  $inner = @"
`$ErrorActionPreference = 'Continue'
"user: `$(whoami); profile: `$env:USERPROFILE; temp: `$env:TEMP"
try { Set-Content '$Dir\written_by_$($s.U).txt' 'hello' -ErrorAction Stop; 'io folder: writable' } catch { "io folder: NOT writable: `$(`$_.Exception.Message)" }
& '$tools\test_game.ps1' -Exe '$w\game\Locoland.exe' -ClickOk 1 -SetFull 0 -Map 'maps\l01\l01.map' -WaitFor 'test map: loaded' -Shot '$w\shot.png' -LastShotOnly -Language $($s.Lang) *>&1 | Out-String -Width 300
"test key Language: `$((Get-ItemProperty 'HKCU:\SOFTWARE\Gromada\Locoland-test' -ErrorAction SilentlyContinue).Language)"
"@
  Set-Content "$w\inner.ps1" $inner -Encoding utf8
  $t = Get-Date
  try {
    $p = Start-Process powershell -Credential $cred -LoadUserProfile -WorkingDirectory $w -WindowStyle Hidden -PassThru `
      -ArgumentList @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "$w\inner.ps1") -RedirectStandardOutput "$w\out.txt" -RedirectStandardError "$w\err.txt"
    $null = $p.Handle
    L "started as $($s.U): pid $($p.Id) in $([int]((Get-Date) - $t).TotalMilliseconds) ms"
    $procs += @{ P = $p; S = $s; W = $w; T = $t }
  } catch { L "start as $($s.U) FAILED: $($_.Exception.Message)" }
}
foreach ($x in $procs) {
  if (-not $x.P.WaitForExit(240000)) { L "$($x.S.U): still running after 240 s - killing"; & taskkill /F /T /PID $x.P.Id | Out-Null }
  L "$($x.S.U): exit $($x.P.ExitCode) after $([int]((Get-Date) - $x.T).TotalSeconds) s"
  Get-Content "$($x.W)\out.txt", "$($x.W)\err.txt" -ErrorAction SilentlyContinue | Where-Object { $_ -match 'user:|io folder|Quiet|Closed|Screen:|Test key|test key|loaded|error|Error|denied' } | ForEach-Object { "   $_" }
  Copy-Item "$($x.W)\out.txt" (Join-Path $Dir "out_$($x.S.U).txt") -ErrorAction SilentlyContinue
  Copy-Item "$($x.W)\shot.png" (Join-Path $Dir "shot_$($x.S.U).png") -ErrorAction SilentlyContinue
  Get-Content "$($x.W)\game\logs\LocoFix.log" -ErrorAction SilentlyContinue | Select-String "test mode|language|Language|lang" | Select-Object -First 4 | ForEach-Object { "   LocoFix.log: $_" }
}
L "main user's test key Language: '$((Get-ItemProperty 'HKCU:\SOFTWARE\Gromada\Locoland-test' -ErrorAction SilentlyContinue).Language)' (none expected)"
Get-CimInstance Win32_UserProfile | Where-Object { $_.LocalPath -match 'rigslot' } | ForEach-Object { L "profile $($_.LocalPath) loaded $($_.Loaded)" }
