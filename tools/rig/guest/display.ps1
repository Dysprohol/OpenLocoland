# Display helpers for the rig, INSIDE the sandbox (dot-sourced by runjob.ps1).
#   Get-DisplayReport          what the guest's screen is: real pixels, scale, what an unaware program sees
#   Set-GuestResolution "WxH"  the guest desktop's size (ChangeDisplaySettingsEx on the remote display)
#   Set-GuestScale <percent>   the guest's display scale without a sign-out (the per-monitor scale that
#                              Settings > Display sets: DisplayConfigSetDeviceInfo, relative to the
#                              recommended step)
# Every function returns one line saying what it did and what the guest reports afterwards.
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class GuestDisplay {
  [StructLayout(LayoutKind.Sequential)] public struct PT { public int X, Y; }
  [DllImport("user32.dll")] static extern IntPtr MonitorFromPoint(PT p, uint f);
  [DllImport("shcore.dll")] static extern int GetDpiForMonitor(IntPtr m, int t, out uint x, out uint y);
  [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
  [DllImport("user32.dll")] static extern int GetSystemMetrics(int i);
  [DllImport("user32.dll")] static extern IntPtr GetDC(IntPtr h);
  [DllImport("user32.dll")] static extern int ReleaseDC(IntPtr h, IntPtr dc);
  [DllImport("gdi32.dll")] static extern int GetDeviceCaps(IntPtr hdc, int i);

  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] public struct DEVMODE {
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmDeviceName;
    public short dmSpecVersion, dmDriverVersion, dmSize, dmDriverExtra; public int dmFields;
    public int dmPositionX, dmPositionY, dmDisplayOrientation, dmDisplayFixedOutput;
    public short dmColor, dmDuplex, dmYResolution, dmTTOption, dmCollate;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dmFormName;
    public short dmLogPixels; public int dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, dmDisplayFrequency;
    public int dmICMMethod, dmICMIntent, dmMediaType, dmDitherType, dmReserved1, dmReserved2, dmPanningWidth, dmPanningHeight; }
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern bool EnumDisplaySettingsW(string dev, int mode, ref DEVMODE dm);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int ChangeDisplaySettingsExW(string dev, ref DEVMODE dm, IntPtr h, int flags, IntPtr p);

  // what the primary monitor is, in real pixels (a per-monitor-aware thread) and as a DPI-unaware program sees it
  public static string Report() {
    PT p = new PT(); uint dx, dy;
    IntPtr old = SetThreadDpiAwarenessContext(new IntPtr(-4));
    IntPtr m = MonitorFromPoint(p, 1); GetDpiForMonitor(m, 0, out dx, out dy);
    int w = GetSystemMetrics(0), h = GetSystemMetrics(1);
    SetThreadDpiAwarenessContext(new IntPtr(-1));
    int uw = GetSystemMetrics(0), uh = GetSystemMetrics(1);
    SetThreadDpiAwarenessContext(old);
    return string.Format("{0}x{1} real, dpi {2} ({3} %), an unaware program sees {4}x{5}", w, h, dx, dx * 100 / 96, uw, uh);
  }
  // "WxH@dpi" in real pixels, for the agent's heartbeat
  public static string Screen() { IntPtr old = SetThreadDpiAwarenessContext(new IntPtr(-4)); string s = GetSystemMetrics(0) + "x" + GetSystemMetrics(1) + "@" + Dpi(); SetThreadDpiAwarenessContext(old); return s; }
  public static uint Dpi() { PT p = new PT(); uint dx, dy; IntPtr old = SetThreadDpiAwarenessContext(new IntPtr(-4)); GetDpiForMonitor(MonitorFromPoint(p, 1), 0, out dx, out dy); SetThreadDpiAwarenessContext(old); return dx; }
  public static string Modes() {
    var s = new System.Text.StringBuilder(); DEVMODE dm = new DEVMODE(); dm.dmSize = (short)Marshal.SizeOf(dm);
    var seen = new System.Collections.Generic.HashSet<string>();
    for (int i = 0; EnumDisplaySettingsW(null, i, ref dm); i++) { string k = dm.dmPelsWidth + "x" + dm.dmPelsHeight; if (seen.Add(k)) s.Append(k + " "); }
    return s.ToString().Trim();
  }
  public static int SetMode(int w, int h) {
    DEVMODE dm = new DEVMODE(); dm.dmSize = (short)Marshal.SizeOf(dm);
    EnumDisplaySettingsW(null, -1, ref dm);
    dm.dmPelsWidth = w; dm.dmPelsHeight = h; dm.dmFields = 0x80000 | 0x100000;
    return ChangeDisplaySettingsExW(null, ref dm, IntPtr.Zero, 0, IntPtr.Zero);   // 0 = DISP_CHANGE_SUCCESSFUL
  }

  // ---- the per-monitor scale (what Settings > Display > Scale does)
  [DllImport("user32.dll")] static extern int GetDisplayConfigBufferSizes(uint flags, out uint paths, out uint modes);
  [DllImport("user32.dll")] static extern int QueryDisplayConfig(uint flags, ref uint np, byte[] paths, ref uint nm, byte[] modes, IntPtr topo);
  [DllImport("user32.dll")] static extern int DisplayConfigGetDeviceInfo(byte[] packet);
  [DllImport("user32.dll")] static extern int DisplayConfigSetDeviceInfo(byte[] packet);
  public static readonly int[] Steps = { 100, 125, 150, 175, 200, 225, 250, 300, 350, 400, 450, 500 };
  static byte[] Source(out string err) {
    err = null; uint np, nm;
    int r = GetDisplayConfigBufferSizes(2, out np, out nm); if (r != 0) { err = "GetDisplayConfigBufferSizes " + r; return null; }
    byte[] paths = new byte[np * 72], modes = new byte[nm * 64];
    r = QueryDisplayConfig(2, ref np, paths, ref nm, modes, IntPtr.Zero); if (r != 0) { err = "QueryDisplayConfig " + r; return null; }
    if (np == 0) { err = "no active path"; return null; }
    byte[] src = new byte[12]; Array.Copy(paths, 0, src, 0, 12); return src;   // adapter LUID + source id of the first path
  }
  // min / current / max steps relative to the recommended one
  public static string GetScale(out int min, out int cur, out int max) {
    min = cur = max = 0; string err; byte[] src = Source(out err); if (src == null) return err;
    byte[] pk = new byte[32]; BitConverter.GetBytes(-3).CopyTo(pk, 0); BitConverter.GetBytes(32).CopyTo(pk, 4); Array.Copy(src, 0, pk, 8, 12);
    int r = DisplayConfigGetDeviceInfo(pk); if (r != 0) return "DisplayConfigGetDeviceInfo(GET_DPI_SCALE) " + r;
    min = BitConverter.ToInt32(pk, 20); cur = BitConverter.ToInt32(pk, 24); max = BitConverter.ToInt32(pk, 28); return null;
  }
  public static string SetScale(int percent) {
    int min, cur, max; string err = GetScale(out min, out cur, out max); if (err != null) return "get: " + err;
    int now = Array.IndexOf(Steps, (int)(Dpi() * 100 / 96)); if (now < 0) return "current scale not a step: dpi " + Dpi();
    int want = Array.IndexOf(Steps, percent); if (want < 0) return percent + " % is not a scale step";
    int recommended = now - cur, rel = want - recommended;
    string range = string.Format("steps {0}..{1} % (recommended {2} %)", Steps[Math.Max(0, recommended + min)], Steps[Math.Min(Steps.Length - 1, recommended + max)], Steps[Math.Max(0, recommended)]);
    if (rel < min || rel > max) return "out of range: " + range;
    string src2; byte[] src = Source(out src2);
    byte[] pk = new byte[24]; BitConverter.GetBytes(-4).CopyTo(pk, 0); BitConverter.GetBytes(24).CopyTo(pk, 4); Array.Copy(src, 0, pk, 8, 12); BitConverter.GetBytes(rel).CopyTo(pk, 20);
    int r = DisplayConfigSetDeviceInfo(pk);
    return (r == 0 ? "set" : "DisplayConfigSetDeviceInfo(SET_DPI_SCALE) " + r) + "; " + range;
  }
}
"@
function Get-DisplayReport { [GuestDisplay]::Report() }
# What a NEW process sees (a scale change reaches running programs only through WM_DPICHANGED).
function Get-NewProcessReport {
  $cmd = 'Add-Type -AssemblyName System.Windows.Forms; $b = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds; "an unaware new process sees {0}x{1}" -f $b.Width, $b.Height'
  (& powershell -NoProfile -EncodedCommand ([Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($cmd)))) -join " "
}
function Set-GuestResolution([string]$res) {
  if ($res -notmatch '^(\d+)x(\d+)$') { return "resolution: '$res' is not WxH" }
  $w = [int]$Matches[1]; $h = [int]$Matches[2]
  $before = Get-DisplayReport
  $r = [GuestDisplay]::SetMode($w, $h)
  Start-Sleep -Seconds 2
  "resolution ${res}: ChangeDisplaySettingsEx $r (0 = done); before: $before; after: $(Get-DisplayReport); modes listed: $([GuestDisplay]::Modes())"
}
function Set-GuestScale([int]$percent) {
  $before = Get-DisplayReport
  $r = [GuestDisplay]::SetScale($percent)
  Start-Sleep -Seconds 2
  "scale ${percent} %: $r; before: $before; after: $(Get-DisplayReport); $(Get-NewProcessReport)"
}
