# Runs INSIDE the sandbox: reports what the guest Windows looks like (display, scale, GPU,
# Defender, tools the test scripts need). Output: <out>\probe.txt
param([string]$Out = "C:\rig\out", [string]$Name = "probe")
$ErrorActionPreference = "Continue"
$file = Join-Path $Out "$Name.txt"
function W($s) { Add-Content -Path $file -Value $s -Encoding utf8 }
Add-Type @"
using System; using System.Runtime.InteropServices;
public class P {
  [StructLayout(LayoutKind.Sequential)] public struct PT { public int X, Y; }
  [DllImport("user32.dll")] public static extern IntPtr MonitorFromPoint(PT p, uint f);
  [DllImport("shcore.dll")] public static extern int GetDpiForMonitor(IntPtr m, int t, out uint x, out uint y);
  [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
  [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
  [DllImport("user32.dll")] public static extern IntPtr GetDC(IntPtr h);
  [DllImport("gdi32.dll")] public static extern int GetDeviceCaps(IntPtr hdc, int i);
  [DllImport("user32.dll")] public static extern uint GetDpiForSystem();
  public static string Metrics() { return GetSystemMetrics(0) + "x" + GetSystemMetrics(1) + " (virtual " + GetSystemMetrics(78) + "x" + GetSystemMetrics(79) + ")"; }
  public static string MonDpi() { uint x, y; PT p = new PT(); IntPtr m = MonitorFromPoint(p, 1); int r = GetDpiForMonitor(m, 0, out x, out y); return "dpi " + x + " (hr " + r + ")"; }
  public static string Caps() { IntPtr dc = GetDC(IntPtr.Zero); return "HORZRES " + GetDeviceCaps(dc, 8) + " VERTRES " + GetDeviceCaps(dc, 10) + " DESKTOPHORZRES " + GetDeviceCaps(dc, 118) + " DESKTOPVERTRES " + GetDeviceCaps(dc, 117) + " LOGPIXELSX " + GetDeviceCaps(dc, 88); }
}
"@
W "== $Name $(Get-Date -Format o)"
W "uptime s: $([int]((Get-Date) - (Get-CimInstance Win32_OperatingSystem).LastBootUpTime).TotalSeconds)"
W "os: $((Get-CimInstance Win32_OperatingSystem).Caption) $([Environment]::OSVersion.Version) user $env:USERNAME"
W "ps: $($PSVersionTable.PSVersion) clr $($PSVersionTable.CLRVersion)"
W "unaware (this process): $([P]::Metrics()) | $([P]::MonDpi()) | $([P]::Caps()) | system dpi $([P]::GetDpiForSystem())"
$old = [P]::SetThreadDpiAwarenessContext([IntPtr](-4))
W "per-monitor v2 thread: $([P]::Metrics()) | $([P]::MonDpi()) | $([P]::Caps())"
[void][P]::SetThreadDpiAwarenessContext($old)
W "registry LogPixels: $((Get-ItemProperty 'HKCU:\Control Panel\Desktop' -ErrorAction SilentlyContinue).LogPixels) Win8DpiScaling: $((Get-ItemProperty 'HKCU:\Control Panel\Desktop' -ErrorAction SilentlyContinue).Win8DpiScaling)"
foreach ($v in Get-CimInstance Win32_VideoController) { W "video: $($v.Name) | driver $($v.DriverVersion) | mode $($v.CurrentHorizontalResolution)x$($v.CurrentVerticalResolution) $($v.CurrentRefreshRate) Hz | status $($v.Status)" }
foreach ($m in Get-CimInstance Win32_DesktopMonitor) { W "monitor: $($m.Name) $($m.ScreenWidth)x$($m.ScreenHeight)" }
Add-Type -AssemblyName System.Windows.Forms
foreach ($s in [System.Windows.Forms.Screen]::AllScreens) { W "screen: $($s.DeviceName) $($s.Bounds) primary $($s.Primary)" }
try { $mp = Get-MpComputerStatus -ErrorAction Stop; W "defender: AMService $($mp.AMServiceEnabled) RealTime $($mp.RealTimeProtectionEnabled) Antivirus $($mp.AntivirusEnabled)" } catch { W "defender: Get-MpComputerStatus failed: $($_.Exception.Message)" }
W "WinDefend service: $((Get-Service WinDefend -ErrorAction SilentlyContinue).Status)"
W "SmartScreen policy: $((Get-ItemProperty 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer' -ErrorAction SilentlyContinue).SmartScreenEnabled)"
W "csc: $(Test-Path "$env:WINDIR\Microsoft.NET\Framework\v4.0.30319\csc.exe") python: $([bool](Get-Command python -ErrorAction SilentlyContinue))"
W "audio endpoints: $((Get-CimInstance Win32_SoundDevice | ForEach-Object Name) -join '; ')"
W "d3d dlls: d3d11 $(Test-Path "$env:WINDIR\SysWOW64\d3d11.dll") dxgi $(Test-Path "$env:WINDIR\SysWOW64\dxgi.dll") ddraw $(Test-Path "$env:WINDIR\SysWOW64\ddraw.dll")"
W "memory MB: $([int]((Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory/1MB)) cpus: $((Get-CimInstance Win32_ComputerSystem).NumberOfLogicalProcessors)"
W "session: $((quser 2>&1) -join ' / ')"
W "languages: culture $((Get-Culture).Name), UI $((Get-UICulture).Name), system locale $((Get-WinSystemLocale).Name), user list $((Get-WinUserLanguageList).LanguageTag -join '/')"
W "== end"
