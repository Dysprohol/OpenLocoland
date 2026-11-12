# Experiment (script job): the REAL cursor inside the guest while the game is full screen.
# Starts the game WITHOUT test mode (LOCOFIX_TEST unset, so it follows the real mouse), ticks
# "full screen" in the start window, loads -Map, then moves the guest's cursor with SetCursorPos
# and SendInput to the screen's corners and edges and logs where it ends up (GetCursorPos) and
# the clip rectangle (GetClipCursor - dgVoodoo clips the cursor to the game in full screen).
# Screenshots of the guest's screen (CopyFromScreen) after each move: cursor_<n>.png.
param([string]$Dir, [string]$Game, [string]$Map = "maps\l01\l01.map", [int]$Full = 1, [int]$LoadSeconds = 20)
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
Add-Type @"
using System; using System.Runtime.InteropServices; using System.Text;
public static class Cur {
  [StructLayout(LayoutKind.Sequential)] public struct PT { public int X, Y; }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [StructLayout(LayoutKind.Sequential)] struct MOUSEINPUT { public int dx, dy, data, flags, time; public IntPtr extra; }
  [StructLayout(LayoutKind.Sequential)] struct INPUT { public int type; public MOUSEINPUT mi; }   // 40 bytes on x64 (the union's largest member is MOUSEINPUT)
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out PT p);
  [DllImport("user32.dll")] public static extern bool GetClipCursor(out RECT r);
  [DllImport("user32.dll", SetLastError = true)] static extern uint SendInput(uint n, INPUT[] i, int size);
  [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
  [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowW(string cls, string title);
  // every visible top-level window: process id, class, rectangle
  public static string List() { var sb = new StringBuilder(); EnumWindows((h, x) => { if (IsWindowVisible(h)) { uint id; GetWindowThreadProcessId(h, out id); RECT r; GetWindowRect(h, out r); var c = new StringBuilder(64); GetClassNameW(h, c, 64);
    sb.AppendLine(string.Format("  pid {0} [{1}] ({2},{3})-({4},{5})", id, c, r.L, r.T, r.R, r.B)); } return true; }, IntPtr.Zero); return sb.ToString(); }
  // relative move by (dx, dy) mickeys, as a real mouse sends it
  public static uint Move(int dx, int dy) { INPUT[] i = new INPUT[1]; i[0].type = 0; i[0].mi.dx = dx; i[0].mi.dy = dy; i[0].mi.flags = 1; return SendInput(1, i, Marshal.SizeOf(typeof(INPUT))); }
  [DllImport("kernel32.dll")] static extern IntPtr OpenProcess(int a, bool i, int pid);
  [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, int n, out int r);
  static int Rd(int pid, long addr) { IntPtr h = OpenProcess(0x10, false, pid); byte[] b = new byte[4]; int r; ReadProcessMemory(h, new IntPtr(addr), b, 4, out r); return BitConverter.ToInt32(b, 0); }
  public static int GamePid;
  // where the ENGINE has the mouse (World.mouse screen x/y, floats at World+0x240/0x244; tools	est_game.ps1 step w)
  public static string Engine() { if (GamePid == 0) return ""; long w = (uint)Rd(GamePid, 0x4a9d38); if (w == 0) return " engine -";
    float x = BitConverter.ToSingle(BitConverter.GetBytes(Rd(GamePid, w + 0x240)), 0), y = BitConverter.ToSingle(BitConverter.GetBytes(Rd(GamePid, w + 0x244)), 0);
    return string.Format(" | engine mouse {0},{1}", x, y); }
  // absolute move to (x, y) in screen pixels, as a tablet / remote desktop sends it
  public static uint MoveTo(int x, int y) { INPUT[] i = new INPUT[1]; i[0].type = 0; i[0].mi.dx = (x * 65535) / (GetSystemMetrics(0) - 1); i[0].mi.dy = (y * 65535) / (GetSystemMetrics(1) - 1); i[0].mi.flags = 0x8001; return SendInput(1, i, Marshal.SizeOf(typeof(INPUT))); }
  public static int LastError() { return Marshal.GetLastWin32Error(); }
  public static string Where() { PT p; GetCursorPos(out p); RECT c; GetClipCursor(out c); return string.Format("cursor {0},{1} clip ({2},{3})-({4},{5})", p.X, p.Y, c.L, c.T, c.R, c.B) + Engine(); }
}
"@
[void][Cur]::SetThreadDpiAwarenessContext([IntPtr](-4))   # real pixels
function Shot($n) { $b = [System.Windows.Forms.SystemInformation]::VirtualScreen; $bmp = New-Object Drawing.Bitmap $b.Width, $b.Height; $g = [Drawing.Graphics]::FromImage($bmp); $g.CopyFromScreen($b.Left, $b.Top, 0, 0, $bmp.Size, [Drawing.CopyPixelOperation]::SourceCopy); $g.Dispose(); $bmp.Save((Join-Path $Dir "cursor_$n.png")); $bmp.Dispose() }
function Wins($procId) { $l = New-Object Collections.ArrayList; [void][Cur]::EnumWindows({ param($h, $x) $id = 0; [void][Cur]::GetWindowThreadProcessId($h, [ref]$id); if ($id -eq $procId -and [Cur]::IsWindowVisible($h)) { [void]$l.Add($h) }; $true }, [IntPtr]::Zero); $l }
function Cls($h) { $s = New-Object Text.StringBuilder 64; [void][Cur]::GetClassNameW($h, $s, 64); $s.ToString() }
Remove-Item Env:LOCOFIX_TEST, Env:LOCOFIX_QUIET -ErrorAction SilentlyContinue
"screen: $([Cur]::GetSystemMetrics(0))x$([Cur]::GetSystemMetrics(1)) (real pixels); before the game: $([Cur]::Where())"
$p = Start-Process "$Game\Locoland.exe" -WorkingDirectory $Game -ArgumentList @("--test-map", "`"$Map`"") -PassThru
$deadline = (Get-Date).AddSeconds(15); $dlg = $null
while (-not $dlg -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 300; foreach ($h in Wins $p.Id) { if ([Cur]::GetDlgItem($h, 1) -ne [IntPtr]::Zero) { $dlg = $h } } }
if (-not $dlg) { "no start window"; Stop-Process -Id $p.Id -Force; exit 1 }
$fs = [Cur]::GetDlgItem($dlg, 1005)
[void][Cur]::SendMessage($fs, 0xF1, [IntPtr]$Full, [IntPtr]::Zero)
"full screen box -> $([Cur]::SendMessage($fs, 0xF0, [IntPtr]::Zero, [IntPtr]::Zero))"
[void][Cur]::PostMessage($dlg, 0x0111, [IntPtr]1, [Cur]::GetDlgItem($dlg, 1))
$deadline = (Get-Date).AddSeconds($LoadSeconds + 30); $gw = $null
# the game's window (class Locoland) may belong to a new process of the same exe, so it is looked
# for by its class
while (-not $gw -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500; $f = [Cur]::FindWindowW("Locoland", [NullString]::Value); if ($f -ne [IntPtr]::Zero) { $gw = $f } }
if (-not $gw) {
  "no game window; start process alive $(-not $p.HasExited); Locoland processes: $((Get-Process Locoland -ErrorAction SilentlyContinue | ForEach-Object { "$($_.Id) session $($_.SessionId)" }) -join ', '); this process: $PID session $((Get-Process -Id $PID).SessionId); top-level windows:"
  [Cur]::List()
  Shot 0; exit 1 }
Start-Sleep -Seconds $LoadSeconds
$gpid = 0; [void][Cur]::GetWindowThreadProcessId($gw, [ref]$gpid); [Cur]::GamePid = [int]$gpid
$r = New-Object Cur+RECT; [void][Cur]::GetWindowRect($gw, [ref]$r)
"game window ($($r.L),$($r.T))-($($r.R),$($r.B)), foreground: $([Cur]::GetForegroundWindow() -eq $gw); screen now $([Cur]::GetSystemMetrics(0))x$([Cur]::GetSystemMetrics(1))"
"after load: $([Cur]::Where())"
$w = [Cur]::GetSystemMetrics(0); $h = [Cur]::GetSystemMetrics(1); $n = 0
foreach ($pt in @(@(0, 0), @(($w - 1), ($h - 1)), @([int]($w / 2), ($h - 1)), @(($w - 1), [int]($h / 2)), @([int]($w / 2), [int]($h / 2)))) {
  [void][Cur]::SetCursorPos($pt[0], $pt[1]); Start-Sleep -Milliseconds 400
  $n++; "SetCursorPos($($pt[0]),$($pt[1])): $([Cur]::Where())"; Shot $n
}
# SendInput, relative: far down and right, then far up and left
$k = [Cur]::Move(5000, 5000); $e = [Cur]::LastError(); Start-Sleep -Milliseconds 400; $n++; "SendInput +5000,+5000 (sent $k, error $e): $([Cur]::Where())"; Shot $n
$k = [Cur]::Move(-5000, -5000); $e = [Cur]::LastError(); Start-Sleep -Milliseconds 400; $n++; "SendInput -5000,-5000 (sent $k, error $e): $([Cur]::Where())"; Shot $n
foreach ($pt in @(@(($w - 1), ($h - 1)), @([int]($w / 2), ($h - 1)), @(100, 100))) {
  $k = [Cur]::MoveTo($pt[0], $pt[1]); $e = [Cur]::LastError(); Start-Sleep -Milliseconds 400; $n++; "SendInput absolute $($pt[0]),$($pt[1]) (sent $k, error $e): $([Cur]::Where())"; Shot $n
}
[void][Cur]::PostMessage($gw, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero)
$deadline = (Get-Date).AddSeconds(15)
while ([Cur]::FindWindowW("Locoland", [NullString]::Value) -ne [IntPtr]::Zero -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 300 }
"game closed: $([Cur]::FindWindowW("Locoland", [NullString]::Value) -eq [IntPtr]::Zero); after: $([Cur]::Where())"
Get-Process Locoland -ErrorAction SilentlyContinue | Stop-Process -Force

exit 0
