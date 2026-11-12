# The installer job, INSIDE a fresh sandbox (runjob.ps1 calls it): Setup.exe from the host
# (read-only) is copied in, marked as downloaded from the internet (the Zone.Identifier a browser
# writes, so SmartScreen and Defender treat it as a player's download), and started through the
# shell as a double-click would - with the Steam original (C:\steam, read-only) and a target in the
# guest. Then the installed game is started once (tools\test_game.ps1, mission 1).
# Results: setup_<n>.png (the guest's screen every 2 s while Setup runs), install.log, the game run.
param($Job, [string]$Dir, [string]$Fresh = "0")
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
function Log($s) { "$(Get-Date -Format 'HH:mm:ss.fff') $s" }
function Shot($name) { $b = [System.Windows.Forms.SystemInformation]::VirtualScreen; $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height; $g = [Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size); $g.Dispose(); $bmp.Save((Join-Path $Dir "$name.png")); $bmp.Dispose() }
Log "fresh box: $Fresh (a Windows no earlier job has touched)"
try { $mp = Get-MpComputerStatus -ErrorAction Stop; Log "Defender: service $($mp.AMServiceEnabled), real-time $($mp.RealTimeProtectionEnabled), antivirus $($mp.AntivirusEnabled), signatures $($mp.AntivirusSignatureVersion)" } catch { Log "Defender: $($_.Exception.Message)" }
Log "WinDefend service: $((Get-Service WinDefend -ErrorAction SilentlyContinue).Status); SmartScreen (Explorer policy): '$((Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer' -ErrorAction SilentlyContinue).SmartScreenEnabled)'"
Log "python: $([bool](Get-Command python -ErrorAction SilentlyContinue)); Steam: $([bool](Get-ItemProperty 'HKCU:\SOFTWARE\Valve\Steam' -ErrorAction SilentlyContinue)); Visual Studio: $(Test-Path 'C:\Program Files\Microsoft Visual Studio'); VC runtime x86: $(Test-Path "$env:WINDIR\SysWOW64\vcruntime140.dll")"
$work = "C:\work\setup"; New-Item -ItemType Directory -Force $work | Out-Null
$exe = Join-Path $work (Split-Path $Job.Setup -Leaf)
Copy-Item $Job.Setup $exe -Force
if (-not $Job.NoMotw) { Set-Content -Path $exe -Stream Zone.Identifier -Value "[ZoneTransfer]`r`nZoneId=3`r`nHostUrl=https://github.com/" }
Log "Setup: $exe ($([int]((Get-Item $exe).Length / 1MB)) MB), Zone.Identifier: $((Get-Content $exe -Stream Zone.Identifier -ErrorAction SilentlyContinue) -join ' ')"
$target = if ($Job.Target) { $Job.Target } else { "C:\Games\OpenLocoland" }
$argline = "$($Job.SetupArgs) --steam `"$($Job.Steam)`" --target `"$target`""
Log "start (shell, as a double-click): $argline"
$t = Get-Date
$p = Start-Process $exe -ArgumentList $argline -PassThru -WorkingDirectory $work
$null = $p.Handle
$n = 0
while (-not $p.HasExited -and ((Get-Date) - $t).TotalMinutes -lt 10) {
  Start-Sleep -Seconds 2; $n++
  if ($n -le 60) { Shot ("setup_{0:D2}" -f $n) }
}
$secs = [int]((Get-Date) - $t).TotalSeconds
if (-not $p.HasExited) { Log "Setup still running after 10 min - killed"; Stop-Process -Id $p.Id -Force; exit 1 }
Log "Setup exit $($p.ExitCode) after $secs s"
Get-Process | Where-Object { $_.ProcessName -match "smartscreen|Setup" } | ForEach-Object { Log "process left: $($_.ProcessName) $($_.Id)" }
foreach ($f in "$target\logs\install.log") { if (Test-Path $f) { Copy-Item $f (Join-Path $Dir "install.log") } }
Get-WinEvent -LogName "Microsoft-Windows-Windows Defender/Operational" -MaxEvents 20 -ErrorAction SilentlyContinue | ForEach-Object { Log "Defender event $($_.Id): $($_.Message.Split("`n")[0])" }
Get-WinEvent -LogName "Microsoft-Windows-SmartScreen/Debug" -MaxEvents 20 -ErrorAction SilentlyContinue | ForEach-Object { Log "SmartScreen event $($_.Id): $($_.Message.Split("`n")[0])" }
if (-not (Test-Path "$target\Locoland.exe")) { Log "no Locoland.exe in $target - not installed"; exit 1 }
Log "installed: $((Get-ChildItem $target -Recurse -File).Count) files, $([int]((Get-ChildItem $target -Recurse -File | Measure-Object Length -Sum).Sum / 1MB)) MB"
Get-ChildItem "$env:PUBLIC\Desktop", "$env:USERPROFILE\Desktop", "$env:APPDATA\Microsoft\Windows\Start Menu\Programs" -Filter *.lnk -Recurse -ErrorAction SilentlyContinue | ForEach-Object { Log "shortcut: $($_.FullName)" }
if ($Job.NoGame) { exit 0 }
$tools = if ($Job.Tools) { $Job.Tools } else { Split-Path (Split-Path $PSScriptRoot) }
$text = & (Join-Path $tools "test_game.ps1") -Exe "$target\Locoland.exe" -ClickOk 1 -SetFull 0 -Map "maps\l01\l01.map" -WaitFor "test map: loaded" -Shot (Join-Path $Dir "installed_game.png") -LastShotOnly *>&1 | Out-String -Width 400
$text | Set-Content (Join-Path $Dir "installed_game.txt") -Encoding utf8
Log "installed game: $(if ($text -match 'Closed normally') { 'ran mission 1 and closed normally' } else { 'FAILED - see installed_game.txt' })"
if (Test-Path "$target\logs") { robocopy "$target\logs" (Join-Path $Dir "installed-logs") /E /NFL /NDL /NJH /NJS /NP | Out-Null }
exit $(if ($text -match 'Closed normally') { 0 } else { 1 })
