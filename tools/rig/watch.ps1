# Watches the user's desktop while rig jobs run (host side; changes nothing): every foreground
# change and every window a sandbox process shows there, with the time, into -Out. A check that the
# rig puts nothing on the user's screen: lines naming WindowsSandbox*, powershell, conhost or
# Locoland would be the rig's; the rest is the user's own work.
#   .\tools\rig\watch.ps1 -Out watch.txt -Minutes 90
param([string]$Out, [double]$Minutes = 60)
$rigDir = $PSScriptRoot
if (-not ("RigHost" -as [type])) { Add-Type -TypeDefinition (Get-Content (Join-Path $rigDir "RigHost.cs") -Raw) -ReferencedAssemblies System.Drawing }
$t0 = Get-Date
[RigHost]::Watch($false)   # log only: nothing is minimised or touched
"watch from $($t0.ToString('yyyy-MM-dd HH:mm:ss')) for $Minutes min" | Set-Content $Out -Encoding utf8
$written = 0
$end = $t0.AddMinutes($Minutes)
while ((Get-Date) -lt $end) {
  Start-Sleep -Seconds 5
  $ev = [RigHost]::Events()
  if ($ev.Count -gt $written) { $ev[$written..($ev.Count - 1)] | ForEach-Object { if ($_ -match '^(\d+) ms (.*)$') { "{0:HH:mm:ss} {1}" -f $t0.AddMilliseconds([double]$Matches[1]), $Matches[2] } else { $_ } } | Add-Content $Out -Encoding utf8; $written = $ev.Count }
}
[RigHost]::StopWatch()
"end $(Get-Date -Format 'HH:mm:ss')" | Add-Content $Out -Encoding utf8
