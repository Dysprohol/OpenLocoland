# Experiment (script job): real mouse clicks in the guest (as when someone opens the box's window
# and clicks into it) while tools\test_game.ps1 drives the game with its virtual test cursor.
# The engine's mouse is read by the run's "w" steps before and after the clicks.
param([string]$Dir, [string]$Game, [string]$Tools = "C:\host\Locoland-src\.claude\worktrees\test-rig\tools")
Add-Type @"
using System; using System.Runtime.InteropServices;
public static class Clk {
  [StructLayout(LayoutKind.Sequential)] struct MOUSEINPUT { public int dx, dy, data, flags, time; public IntPtr extra; }
  [StructLayout(LayoutKind.Sequential)] struct INPUT { public int type; public MOUSEINPUT mi; }
  [DllImport("user32.dll")] static extern uint SendInput(uint n, INPUT[] i, int size);
  [DllImport("user32.dll")] static extern int GetSystemMetrics(int i);
  static uint One(int flags, int x, int y) { INPUT[] i = new INPUT[1]; i[0].type = 0; i[0].mi.dx = x; i[0].mi.dy = y; i[0].mi.flags = flags; return SendInput(1, i, Marshal.SizeOf(typeof(INPUT))); }
  // a real left click at a screen point (absolute move, down, up)
  public static uint Click(int x, int y) { int ax = x * 65535 / (GetSystemMetrics(0) - 1), ay = y * 65535 / (GetSystemMetrics(1) - 1);
    return One(0x8001, ax, ay) + One(0x8002, ax, ay) + One(0x8004, ax, ay); }
}
"@
$out = Join-Path $Dir "test_game_during_clicks.txt"
$args_ = "-NoProfile -ExecutionPolicy Bypass -File `"$Tools\test_game.ps1`" -Exe `"$Game\Locoland.exe`" -ClickOk 1 -SetFull 0 -Map maps\l01\l01.map -WaitFor `"test map: loaded`" -Visible -Shot `"$Dir\click.png`" -Steps `"w@8,w@1,m100:100@4,w@1`""
$p = Start-Process powershell -ArgumentList $args_ -RedirectStandardOutput $out -PassThru -WindowStyle Minimized
$null = $p.Handle
Start-Sleep -Seconds 14   # the map is loaded and the first "w" step done
foreach ($pt in @(@(200, 200), @(900, 600), @(1500, 300), @(1000, 1000))) { "real click at $($pt[0]),$($pt[1]): sent $([Clk]::Click($pt[0], $pt[1]))"; Start-Sleep -Milliseconds 700 }
$p.WaitForExit(120000) | Out-Null
"test_game.ps1 ended: $($p.HasExited)"
Get-Content $out | Select-String "Mouse:|Closed|Screenshot|STOP"
exit 0
