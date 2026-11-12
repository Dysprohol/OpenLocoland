# What the rig must leave alone on the host, read without changing anything (host side):
#   - the host's playback endpoints: mute and volume (the rig never touches the host's mixer)
#   - the processes with a sound stream open on the host, e.g. the box's client (the guest's sound
#     comes out through it - with the guest muted it should not be "playing")
#   - when the player's own registry key HKCU\SOFTWARE\Gromada\Locoland was last written (only the
#     key's time is read, no value)
#   .\tools\rig\hostcheck.ps1 [-Out file]  (one line per item, appended)
param([string]$Out = "")
. (Join-Path $PSScriptRoot "guest\audio.ps1")
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class KeyTime {
  [DllImport("advapi32.dll", CharSet = CharSet.Unicode)] static extern int RegOpenKeyExW(IntPtr root, string sub, int opt, int sam, out IntPtr key);
  [DllImport("advapi32.dll", CharSet = CharSet.Unicode)] static extern int RegQueryInfoKeyW(IntPtr k, IntPtr c, IntPtr cl, IntPtr r, out int subkeys, IntPtr msk, IntPtr mcl, out int values, IntPtr mvn, IntPtr mvl, IntPtr sd, out long ft);
  [DllImport("advapi32.dll")] static extern int RegCloseKey(IntPtr k);
  public static string Of(string sub) {
    IntPtr k; int r = RegOpenKeyExW(new IntPtr(unchecked((int)0x80000001)), sub, 0, 0x20019 /*KEY_READ*/, out k);
    if (r != 0) return "not there (" + r + ")";
    int sk, v; long ft; RegQueryInfoKeyW(k, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, out sk, IntPtr.Zero, IntPtr.Zero, out v, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero, out ft); RegCloseKey(k);
    return "last written " + DateTime.FromFileTime(ft).ToString("yyyy-MM-dd HH:mm:ss") + ", " + v + " values, " + sk + " subkeys";
  }
}
"@
$t = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
$lines = @("$t host sound: $(try { [GuestAudio]::State() } catch { $_.Exception.Message })",
           "$t host sound streams: $(Get-AudioSessions)",
           "$t host key HKCU\SOFTWARE\Gromada\Locoland: $([KeyTime]::Of('SOFTWARE\Gromada\Locoland'))")
if ($Out) { $lines | Add-Content $Out -Encoding utf8 }
$lines
