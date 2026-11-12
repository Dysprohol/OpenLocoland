# Experiment (script job): what the guest's display can be set to from inside.
param([string]$Dir, [string]$Game = "")
. (Join-Path $PSScriptRoot "display.ps1")
"start: $(Get-DisplayReport)"
"modes listed: $([GuestDisplay]::Modes())"
$min = 0; $cur = 0; $max = 0; $e = [GuestDisplay]::GetScale([ref]$min, [ref]$cur, [ref]$max)
"scale steps relative to recommended: err '$e' min $min cur $cur max $max"
foreach ($s in 125, 150, 175, 200, 100) { Set-GuestScale $s }
foreach ($r in "1366x768", "1920x1080", "1024x768", "2560x1440", "3840x2160") { Set-GuestResolution $r }
"end: $(Get-DisplayReport)"
