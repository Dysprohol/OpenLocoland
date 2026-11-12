# The game language Setup chooses, INSIDE the box (rig.ps1 -Job script -Fresh): one or more Setup runs one
# after another in the same Windows, each followed by the installed game's start in the language it got.
#
#   .\tools\rig\rig.ps1 -Job script -Fresh -Script tools\rig\guest\setup_language_job.ps1 -ScriptArgs @{ Runs = "..." }
#
# Runs: runs separated by ";", each "<setup>|<mode>|<steam>|<manifest>|<target>|<clean>|<more args>":
#   setup     a Setup exe under SetupDir (default: build\setups of the repository this script is in)
#   mode      auto (the window, installing by itself) | silent
#   steam     the game's original files in the box: C:\steam (the Steam original the rig maps) or another
#             mapped folder, e.g. C:\host\Locoland-src\build\editions\pl-steam\steam
#   manifest  - : Setup gets that folder as it is (no Steam library around it, so no appmanifest);
#             <steam language name> (english, polish, ...): the folder is copied into a Steam library's
#             layout, C:\SteamLib\steamapps\common\Locoland, with an appmanifest_352130.acf naming that
#             language, as a player's Steam has it
#   target    the install folder
#   clean     1: the box's Setup and game state is removed first (HKCU\SOFTWARE\Gromada, %LOCALAPPDATA%\
#             OpenLocoland, the target, the shortcuts) - a Windows OpenLocoland never ran on; 0: kept (update);
#             lang=<code>: kept, and the saved game language set to <code> first (as the game's start window
#             saves a player's choice)
#   more      more arguments for Setup (e.g. --game-lang de)
# Results per run n: run<n>_setup_<k>.png (the screen while the window runs), run<n>_install.log, run<n>_menu.png
# (the installed game's screen after the start window's OK) and a summary in script.txt.
param([string]$Dir, [string]$Runs, [string]$SetupDir = "")
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
function Log($s) { "$(Get-Date -Format 'HH:mm:ss.fff') $s" }
function Shot($name) { $b = [System.Windows.Forms.SystemInformation]::VirtualScreen; $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height; $g = [Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size); $g.Dispose(); $bmp.Save((Join-Path $Dir "$name.png")); $bmp.Dispose() }
$tools = Split-Path (Split-Path $PSScriptRoot)
if (-not $SetupDir) { $SetupDir = Join-Path (Split-Path $tools) "build\setups" }
Add-Type -Namespace W -Name K -MemberDefinition '[DllImport("kernel32.dll")] public static extern ushort GetUserDefaultUILanguage();'
Log "system UI language: $([Globalization.CultureInfo]::GetCultureInfo([int][W.K]::GetUserDefaultUILanguage()).Name); culture $((Get-Culture).Name)"
$key = "HKCU:\SOFTWARE\Gromada\Locoland"
$links = { Get-ChildItem "$env:PUBLIC\Desktop", "$env:USERPROFILE\Desktop", "$env:APPDATA\Microsoft\Windows\Start Menu\Programs" -Filter *.lnk -Recurse -ErrorAction SilentlyContinue }
$n = 0; $lastLib = ""
foreach ($run in ($Runs -split ";" | Where-Object { $_.Trim() })) {
  $n++
  $f = $run.Split("|"); $setup, $mode, $steam, $manifest, $target, $clean, $more = $f + @("", "", "", "", "", "", "")
  Log "== run ${n}: $run"
  if ($clean -eq "1") {
    Remove-Item "HKCU:\SOFTWARE\Gromada" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item "$env:LOCALAPPDATA\OpenLocoland" -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $target -Recurse -Force -ErrorAction SilentlyContinue
    & $links | Where-Object { $_.Name -match "Locoland" } | Remove-Item -Force
    Log "clean: no HKCU\SOFTWARE\Gromada, no %LOCALAPPDATA%\OpenLocoland, no $target, no OpenLocoland shortcuts"
  } else {
    if ($clean -like "lang=*") {
      New-Item $key -Force | Out-Null
      Set-ItemProperty $key -Name Language -Value $clean.Substring(5) -Type String
      Log "saved game language set to '$($clean.Substring(5))' (as the start window saves it)"
    }
    Log "kept: registry Language = '$((Get-ItemProperty $key -ErrorAction SilentlyContinue).Language)'; captured: $((Get-ChildItem "$env:LOCALAPPDATA\OpenLocoland\captured" -Directory -ErrorAction SilentlyContinue).Name -join ',')"
  }
  if ($manifest -and $manifest -ne "-") {
    $lib = "C:\SteamLib\steamapps"; $game = "$lib\common\Locoland"
    if ($lastLib -ne $steam) {
      Remove-Item "C:\SteamLib" -Recurse -Force -ErrorAction SilentlyContinue
      robocopy $steam $game /E /NFL /NDL /NJH /NJS /NP /MT:16 | Out-Null
      $lastLib = $steam
    }
    @"
"AppState"
{
	"appid"		"352130"
	"name"		"Locoland"
	"installdir"		"Locoland"
	"UserConfig"
	{
		"language"		"$manifest"
	}
	"MountedConfig"
	{
		"language"		"$manifest"
	}
}
"@ | Set-Content "$lib\appmanifest_352130.acf" -Encoding ascii
    Log "Steam library: $steam copied to $game, appmanifest language '$manifest'"
    $steam = $game
  } else { Log "Steam folder as it is (no Steam library around it): $steam" }
  $exe = Join-Path $SetupDir $setup
  $argline = $(if ($mode -eq "auto") { "--auto" } else { "--silent" }) + " --steam `"$steam`" --target `"$target`" $more"
  Log "start: $setup $argline"
  $t = Get-Date
  $p = Start-Process $exe -ArgumentList $argline -PassThru
  $null = $p.Handle
  $k = 0
  while (-not $p.HasExited -and ((Get-Date) - $t).TotalMinutes -lt 10) {
    Start-Sleep -Milliseconds $(if ($k -lt 8) { 700 } else { 3000 }); $k++
    if ($mode -eq "auto" -and $k -le 40) { Shot ("run{0}_setup_{1:D2}" -f $n, $k) }
  }
  if (-not $p.HasExited) { Log "Setup still running after 10 min - killed"; Stop-Process -Id $p.Id -Force; continue }
  Log "Setup exit $($p.ExitCode) after $([int]((Get-Date) - $t).TotalSeconds) s"
  if (Test-Path "$target\logs\install.log") {
    Copy-Item "$target\logs\install.log" (Join-Path $Dir "run${n}_install.log")
    Get-Content "$target\logs\install.log" | Where-Object { $_ -match "language|lang/|captured|Not available|download_depot|FAILED|^OK" } | ForEach-Object { Log "  log: $_" }
  }
  $saved = (Get-ItemProperty $key -ErrorAction SilentlyContinue).Language
  Log "registry Language: '$saved'"
  Log "lang\ packs built: $((Get-ChildItem "$target\lang" -Directory -ErrorAction SilentlyContinue).Name -join ','); root.txt: '$((Get-Content "$target\lang\root.txt" -ErrorAction SilentlyContinue) -join '')'"
  Log "captured: $((Get-ChildItem "$env:LOCALAPPDATA\OpenLocoland\captured" -Directory -ErrorAction SilentlyContinue).Name -join ',')"
  & $links | Where-Object { $_.Name -match "Locoland" } | ForEach-Object { Log "shortcut: $($_.FullName)" }
  if (-not (Test-Path "$target\Locoland.exe")) { Log "no Locoland.exe - not installed"; continue }
  # the game in the language the player's start would take: the saved one (test mode reads a test key, so the
  # saved value is handed over; without one, a code no pack has - the game then falls back as with none saved
  # when there are no packs, which is the only case Setup writes none on a new install)
  $lang = if ($saved) { $saved } else { "none" }
  Remove-Item "$target\logs\LocoFix.log" -ErrorAction SilentlyContinue
  $text = & (Join-Path $tools "test_game.ps1") -Exe "$target\Locoland.exe" -Language $lang -ClickOk 12 -SetFull 0 -Shot (Join-Path $Dir "run${n}_menu.png") -LastShotOnly *>&1 | Out-String -Width 400
  $text | Set-Content (Join-Path $Dir "run${n}_game.txt") -Encoding utf8
  Log "game (test key Language '$lang'): $(if ($text -match 'Closed normally') { 'started and closed normally' } else { 'see run' + $n + '_game.txt' })"
  Get-Content "$target\logs\LocoFix.log" -ErrorAction SilentlyContinue | Where-Object { $_ -match "language:" } | ForEach-Object { Log "  LocoFix: $_" }
}
Log "done"
