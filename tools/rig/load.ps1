# What the box takes from the host, sampled every -Every seconds into a CSV (host side; reads
# performance counters only, changes nothing):
#   cpu_box    % of the WHOLE machine (all logical processors) used by the sandbox's VM (vmmemWindowsSandbox)
#   cpu_client % of the whole machine used by the box's client window (remote desktop decoding)
#   mem_box    MB of host memory the VM holds (its working set)
#   gpu_box    % of the GPU's 3D engine used by the VM's process (its GPU-PV work shows under it)
#   gpu_all    % of the GPU's 3D engine used by everything on the host (for comparison)
#   .\tools\rig\load.ps1 -Out load.csv -Seconds 300 [-Every 2]
param([string]$Out, [int]$Seconds = 300, [int]$Every = 2)
$cores = [Environment]::ProcessorCount
$inv = [Globalization.CultureInfo]::InvariantCulture
# counter names are in the system's language (Polish here): looked up by their fixed indexes
Add-Type @"
using System; using System.Text; using System.Runtime.InteropServices;
public static class PerfName {
  [DllImport("pdh.dll", CharSet = CharSet.Unicode)] static extern int PdhLookupPerfNameByIndexW(string m, int i, StringBuilder b, ref int n);
  public static string Of(int i) { var b = new StringBuilder(256); int n = 256; PdhLookupPerfNameByIndexW(null, i, b, ref n); return b.ToString(); }
}
"@
$proc = [PerfName]::Of(230); $cpuName = [PerfName]::Of(6); $wsName = [PerfName]::Of(180)
$paths = @("\$proc(vmmemWindowsSandbox)\$cpuName", "\$proc(vmmemWindowsSandbox)\$wsName",
           "\$proc(WindowsSandboxRemoteSession)\$cpuName", "\GPU Engine(*engtype_3D)\Utilization Percentage")
function Sum($samples, [string]$like) { $t = 0.0; foreach ($s in $samples) { if ($s.Path -like $like) { $t += $s.CookedValue } }; $t }
"time,cpu_box,cpu_client,mem_box,gpu_box,gpu_all" | Set-Content $Out -Encoding ascii
$end = (Get-Date).AddSeconds($Seconds)
while ((Get-Date) -lt $end) {
  $vm = Get-Process vmmemWindowsSandbox -ErrorAction SilentlyContinue | Select-Object -First 1
  $c = (Get-Counter $paths -SampleInterval $Every -MaxSamples 1 -ErrorAction SilentlyContinue).CounterSamples
  $cpuBox = (Sum $c "*vmmemwindowssandbox)\$cpuName") / $cores
  $memBox = (Sum $c "*vmmemwindowssandbox)\$wsName") / 1MB
  $cpuCl = (Sum $c "*windowssandboxremotesession)\$cpuName") / $cores
  $gpuAll = Sum $c "*engtype_3d)\*"
  $gpuBox = if ($vm) { Sum $c "*pid_$($vm.Id)_*engtype_3d)\*" } else { 0 }
  [string]::Format($inv, "{0:HH:mm:ss},{1:0.0},{2:0.0},{3:0},{4:0.0},{5:0.0}", (Get-Date), $cpuBox, $cpuCl, $memBox, $gpuBox, $gpuAll) | Add-Content $Out -Encoding ascii
}
