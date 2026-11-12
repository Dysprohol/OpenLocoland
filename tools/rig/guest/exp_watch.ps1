# Experiment (script job): logs the guest's display every second for -Seconds (the host moves or
# resizes the sandbox window meanwhile).
param([string]$Dir, [string]$Game = "", [int]$Seconds = 60)
. (Join-Path $PSScriptRoot "display.ps1")
$last = ""
for ($i = 0; $i -lt $Seconds; $i++) {
  $r = Get-DisplayReport
  if ($r -ne $last) { "$(Get-Date -Format 'HH:mm:ss') $r | $(Get-NewProcessReport)"; $last = $r }
  Start-Sleep -Seconds 1
}
