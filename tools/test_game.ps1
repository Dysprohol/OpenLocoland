# Starts the game and checks the window - for testing after changes.
#
#   .\tools\test_game.ps1 [-Exe <path>] [-Wait 3] [-ClickOk 10] [-SetFull 0] [-Shot file.png] [-Screen] [-Steps ...]
#
# -Wait     at most this many seconds for the launcher to appear
# -SetFull  0/1 - sets the full screen box before OK (0 = window)
# -ClickOk  clicks OK and waits the given number of seconds
# -Shot     screenshot of the game window to a file; with -Screen it is a screenshot of
#           the screen with the game window on top
# -Steps    steps for the game, each followed by a screenshot: a key code (13 = Enter) or
#           the mouse in the game area: m<x>:<y> move, k<x>:<y> left button, p<x>:<y> right
#           button (k<x>:<y>:<ms> / p<x>:<y>:<ms> hold it that long, 100 ms by default),
#           d<x>:<y>-<x>:<y> drag with the middle button, s<x>:<y>:<n> turn the mouse
#           wheel by n notches there (> 0 up), w - print the engine's mouse
#           state (position, buttons, camera, menu item under the cursor) and the state bar
#           (panel, objects, depot list icons, the depot's queue), t - print the tooltip
#           under the cursor (to check that a hover hit the wanted menu item); after @ the
#           seconds to wait, e.g. "k371:489@5,13@20"; "@30?<regex>" waits up to 30 s until
#           the game's logs match the regex instead (e.g. a mission's map and script
#           loaded), and a step whose regex already matches is skipped - so a click can be
#           listed twice as a retry that only happens when the first one did not take;
#           with -Editor, S<file> saves the map and L<file> loads one (game paths, e.g.
#           "Smaps\sandbox\saved.map@3" - File > Save / Load without the dialog, sent as
#           WM_COPYDATA, src/LocoFix/editor.cpp); c<id> posts a menu command (WM_COMMAND),
#           e.g. c40010 for Options > Run the level - not one that opens a dialog;
#           Q<file> types the file into the game's open file dialog (File > Load / Save, the
#           editor's Menu panel) and presses OK, Q alone presses Cancel (a screenshot of the
#           dialog comes first, <Shot>_<n>_dialog.png);
#           V<x>:<y> puts the top-left of the view at that map point (editor test mode);
#           D<x>:<y> writes the rail points near that map point to LocoFix.log (editor test mode);
#           M<file> sets the mission settings from a file (editor test mode, src/LocoFix/editor_mission.cpp);
#           Z<w>:<h> resizes the map (editor test mode: File > Map size);
#           E<zip> exports the map, I<zip>|overwrite|rename|ask imports one (File > Export / Import map);
#           N<name>:<line>[:<w>:<h>] makes a new map (editor test mode: File > New, line = the dialog's ground);
#           K<code> / U<code> only press / only let go a key (K16,l...,U16 drags with Shift held);
#           C<code> the character with that code (WM_CHAR: C77 = 'M', a developer key);
#           A<seconds> Alt+Tab away from the game (real keys), a screenshot of its window
#           (<Shot>_<n>_away.png), and Alt+Tab back after that many seconds;
#           ^<code> a key with Ctrl (^90 = Ctrl+Z), +<code> with Shift, l<x>:<y>-<x>:<y> a drag with the left
#           button (moving an object in the editor), x<seconds> follows the game when it starts
#           again (LocoFix restart.cpp: StartEditorMode, File > Play the map / Back to the
#           menu) - the test goes on with the new process of the same exe
# -Editor   start the game in editor mode (LocoFix --editor; the File menu, decals as objects)
# -Menu    the m/k/p points are points of the menu, which the scripts lay out for a
#           1024x768 screen and which the game scales to fit and centres in the game area
#           (the 4:3 picture pillarboxed in a 16:9 window), so they are scaled and moved
# -LastShotOnly  no screenshot after each step, only the final one
# -Language language pack code (pl, en...) saved as the game's choice before the start
# -WaitFor  before the steps, wait (up to 60 s) until the running game's log contains this
#           regex, e.g. "menu\.map" - the game loads at different speeds
# -Map      load this map straight away instead of the intro and the menu (LocoFix
#           --test-map, game paths such as "maps\l01\l01.map"); -Steps may be left out then
# -LogWait  how long -WaitFor may take (default 60 s)
# At the end it closes the game with WM_CLOSE, so error.log is complete.
# If the full screen box is disabled and the test should run in a window, the game is
# closed without clicking OK - so the display mode is not switched.
param([string]$Exe = (Join-Path (Split-Path (Split-Path $PSScriptRoot)) "Locoland\Locoland.exe"), [int]$Wait = 10, [string]$Shot = "", [int]$ClickOk = 0, [switch]$Screen, [int]$SetFull = -1, [string]$Steps = "", [string]$WaitFor = "", [int]$LogWait = 60, [string]$Language = "", [string]$Map = "", [switch]$LastShotOnly, [switch]$Menu, [switch]$Editor)
Add-Type @"
using System; using System.Runtime.InteropServices; using System.Text;
public class W {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern int GetWindowLong(IntPtr h, int i);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc f, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  [DllImport("user32.dll")] public static extern IntPtr GetDlgItem(IntPtr h, int id);
  [DllImport("user32.dll")] public static extern bool IsWindowEnabled(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
  [DllImport("kernel32.dll")] public static extern IntPtr OpenProcess(int a, bool i, int pid);
  [DllImport("kernel32.dll")] public static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, int n, out int r);
  public static int Rd(int pid, long addr) { IntPtr h = OpenProcess(0x10, false, pid); byte[] b = new byte[4]; int r; ReadProcessMemory(h, new IntPtr(addr), b, 4, out r); return BitConverter.ToInt32(b, 0); }
  public static string Str(int pid, long addr, int n) { IntPtr h = OpenProcess(0x10, false, pid); byte[] b = new byte[n]; int r; ReadProcessMemory(h, new IntPtr(addr), b, n, out r); int i = Array.IndexOf(b, (byte)0); if (i < 0) i = n; return Encoding.GetEncoding(1251).GetString(b, 0, i); }
  [DllImport("user32.dll")] public static extern IntPtr SendMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern IntPtr GetMenu(IntPtr h);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern IntPtr FindWindowEx(IntPtr p, IntPtr after, string cls, string title);
  [DllImport("user32.dll", CharSet = CharSet.Unicode, EntryPoint = "SendMessageW")] public static extern IntPtr SendText(IntPtr h, uint m, IntPtr w, string l);
  [DllImport("user32.dll")] public static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
  [DllImport("user32.dll")] public static extern int GetDlgCtrlID(IntPtr h);
  // the child window with a control id (the file name box of a file dialog is 1001, an Edit)
  public static IntPtr ChildById(IntPtr parent, int id, string cls) {
    IntPtr found = IntPtr.Zero;
    EnumChildWindows(parent, (h, x) => { StringBuilder c = new StringBuilder(64); GetClassName(h, c, 64);
      if (found == IntPtr.Zero && GetDlgCtrlID(h) == id && c.ToString() == cls) found = h; return true; }, IntPtr.Zero);
    return found; }
  [StructLayout(LayoutKind.Sequential)] public struct COPYDATA { public IntPtr Data; public int Size; public IntPtr Bytes; }
  [DllImport("user32.dll")] public static extern IntPtr SendMessageTimeout(IntPtr h, uint m, IntPtr w, ref COPYDATA d, uint flags, uint ms, out IntPtr result);
  // WM_COPYDATA with a command id and a text (editor test commands, src/LocoFix/editor.cpp)
  public static int Command(IntPtr h, int id, string text) {
    byte[] b = Encoding.GetEncoding(1250).GetBytes(text + "\0"); IntPtr p = Marshal.AllocHGlobal(b.Length); Marshal.Copy(b, 0, p, b.Length);
    COPYDATA d = new COPYDATA(); d.Data = new IntPtr(id); d.Size = b.Length; d.Bytes = p; IntPtr r;
    IntPtr ok = SendMessageTimeout(h, 0x4A, IntPtr.Zero, ref d, 0, 60000, out r); Marshal.FreeHGlobal(p);
    return ok == IntPtr.Zero ? -1 : r.ToInt32(); }
}
"@
Add-Type -AssemblyName System.Drawing, System.Windows.Forms
$scr = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
"Screen: $($scr.Width)x$($scr.Height)"
$dir = Split-Path $Exe
# LOCOFIX_TEST=1: the game listens only to the mouse from the test (src/LocoFix/test.cpp),
# so clicks land where they should and the user's mouse is neither moved nor taken into account.
$env:LOCOFIX_TEST = "1"
if ($Language) {
  $key = "HKCU:\SOFTWARE\Gromada\Locoland"
  if (-not (Test-Path $key)) { New-Item $key | Out-Null }
  Set-ItemProperty $key Language $Language
}
$start = @{ FilePath = $Exe; WorkingDirectory = $dir; PassThru = $true }
$arguments = @()
if ($Editor) { $arguments += "--editor" }
if ($Map) { $arguments += @("--test-map", "`"$Map`"") }
if ($arguments) { $start.ArgumentList = $arguments }
$p = Start-Process @start
function Get-Wins { $script:wins = New-Object System.Collections.ArrayList
  $cb = [W+EnumProc]{ param($h, $x) $id = 0; [void][W]::GetWindowThreadProcessId($h, [ref]$id); if ($id -eq $p.Id -and [W]::IsWindowVisible($h)) { [void]$script:wins.Add($h) }; $true }
  [void][W]::EnumWindows($cb, [IntPtr]::Zero) }
# wait for the first window of the game (the launcher), at most $Wait seconds
$deadline = (Get-Date).AddSeconds($Wait)
do { Start-Sleep -Milliseconds 250; Get-Wins; $p.Refresh() } while ($wins.Count -eq 0 -and -not $p.HasExited -and (Get-Date) -lt $deadline)
Start-Sleep -Milliseconds 500
"Process alive: $(-not $p.HasExited)"

# The logs of the running game as one text: the game's own (error<date>.log while it
# runs, renamed to error.log on exit) followed by LocoFix.log, where test mode also lists
# every file the game opens (e.g. "open maps\l01rief.txt"). The game keeps its log open
# for writing, so it is read shared.
function Read-Logs {
  $text = ""
  $files = @(Get-ChildItem (Join-Path $dir "error*.log") | Sort-Object LastWriteTime | Select-Object -Last 1) +
           @(Get-Item (Join-Path $dir "LocoFix.log") -ErrorAction SilentlyContinue)
  foreach ($log in $files) {
    if (-not $log) { continue }
    $f = [IO.File]::Open($log.FullName, 'Open', 'Read', 'ReadWrite')
    $text += (New-Object IO.StreamReader($f, [Text.Encoding]::GetEncoding(1251))).ReadToEnd() + "`n"
    $f.Dispose()
  }
  return $text
}

# The game's own window (class "Locoland"); null while only the launcher dialog is up.
function Find-GameWin {
  Get-Wins
  $wins | Where-Object { $k = New-Object Text.StringBuilder 64; [void][W]::GetClassName($_, $k, 64); $k.ToString() -eq 'Locoland' } | Select-Object -First 1
}
# The game opens that window a moment after OK, so it is waited for rather than used as
# soon as the launcher's process exists.
function Wait-GameWin([int]$max = 10) {
  $deadline = (Get-Date).AddSeconds($max)
  while ($true) {
    $w = Find-GameWin
    if ($w) { return $w }
    if ((Get-Date) -ge $deadline) { return $null }
    Start-Sleep -Milliseconds 250
  }
}
# The game only runs while its window is in front, so the window is brought forward
# whenever the test waits for it.
function Focus-Game {
  $w = Find-GameWin
  if ($w) { [void][W]::SetForegroundWindow($w) }
}

# Waits at most $max seconds until the logs match $pattern (".*" can span lines).
function Wait-Log($pattern, $max) {
  $deadline = (Get-Date).AddSeconds($max)
  do {
    Focus-Game
    if ([regex]::IsMatch((Read-Logs), $pattern, 'Singleline')) { return $true }
    Start-Sleep -Milliseconds 500
  } while ((Get-Date) -lt $deadline)
  return $false
}
if ($ClickOk -gt 0) {
  Get-Wins
  $gp = [W]::Rd($p.Id, 0x4a9a14); "at the dialog: P=0x{0:X8} flags=0x{1:X8}" -f $gp, [W]::Rd($p.Id, $gp)
  foreach ($h in $wins) { $fs = [W]::GetDlgItem($h, 1005); if ($fs -ne [IntPtr]::Zero) { "Box enabled: " + [W]::IsWindowEnabled($fs); if ($SetFull -eq 0 -and -not [W]::IsWindowEnabled($fs)) { "STOP: box disabled - not clicking OK"; Stop-Process -Id $p.Id -Force; return }; "Full screen ticked: " + [W]::SendMessage($fs, 0xF0, [IntPtr]::Zero, [IntPtr]::Zero); $cur = [int][W]::SendMessage($fs, 0xF0, [IntPtr]::Zero, [IntPtr]::Zero); if ($SetFull -ge 0 -and $cur -ne $SetFull) { [void][W]::SendMessage($fs, 0xF1, [IntPtr]$SetFull, [IntPtr]::Zero); "Setting the box -> " + [W]::SendMessage($fs, 0xF0, [IntPtr]::Zero, [IntPtr]::Zero) } }
    $ok = [W]::GetDlgItem($h, 1); if ($ok -ne [IntPtr]::Zero) { "Clicking OK"; [void][W]::PostMessage($h, 0x0111, [IntPtr]1, $ok) } }
  Start-Sleep -Seconds $ClickOk
  $p.Refresh(); "Process alive after OK: $(-not $p.HasExited)"
}

function Save-Shot($h, $file) {
  $r = New-Object W+RECT; [void][W]::GetWindowRect($h, [ref]$r)
  [void][W]::SetForegroundWindow($h); Start-Sleep -Milliseconds 800
  $bmp = New-Object Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
  $g = [Drawing.Graphics]::FromImage($bmp)
  # PrintWindow draws just the game window, even when something covers it; when the
  # result is black (some Direct3D modes), we copy the screen instead.
  $hdc = $g.GetHdc(); [void][W]::PrintWindow($h, $hdc, 2); $g.ReleaseHdc($hdc)
  $mid = $bmp.GetPixel([int]($bmp.Width / 2), [int]($bmp.Height / 2))
  if ($mid.R + $mid.G + $mid.B -eq 0) { $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size) }
  $bmp.Save($file); $g.Dispose(); $bmp.Dispose()
  "Screenshot: $file"
}

# The game started again (LocoFix restart.cpp: a new process of the same exe, the old one ends):
# the test goes on with the new one. False when none came within $max seconds.
function Follow-Restart([int]$max) {
  $old = $script:p; $deadline = (Get-Date).AddSeconds($max); $new = $null
  $path = (Resolve-Path $Exe).Path
  do {
    $new = Get-Process Locoland -ErrorAction SilentlyContinue | Where-Object { $_.Id -ne $old.Id -and $_.Path -eq $path -and $_.StartTime -gt $old.StartTime } | Select-Object -First 1
    if (-not $new) { Start-Sleep -Milliseconds 300 }
  } while (-not $new -and (Get-Date) -lt $deadline)
  if (-not $new) { Write-Host "STOP: the game did not start again"; return $false }
  [void]$old.WaitForExit(30000)
  $script:p = $new; Write-Host "Restarted: pid $($new.Id) (the old game ended: $($old.HasExited))"
  $script:game = Wait-GameWin 30
  if (-not $script:game) { Write-Host "STOP: the window of the restarted game did not appear"; return $false }
  return $true
}

# Steps sent to the game window: "code@seconds,code@seconds" (virtual key code, e.g.
# 13 = Enter, or a mouse step), each followed by a screenshot <Shot>_<n>.png.
# The user's mouse is not moved.
if ($WaitFor) {
  $seen = Wait-Log $WaitFor $LogWait
  "WaitFor '$WaitFor': $seen"; Start-Sleep -Seconds 3   # the menu needs a moment after loading
}
if ($Steps) {
  $game = Wait-GameWin 10
  $p.Refresh()
  if (-not $game -and $p.HasExited -and (Follow-Restart 15)) { $game = $script:game }   # restarted already
  if (-not $game) {
    "STOP: the Locoland window did not appear within 10 s - is the game still on the launcher dialog?"
    Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
    return
  }
  # -Menu: the scripts lay the menu out for a 1024x768 screen and the game fits that 4:3
  # picture into the game area (scaled to the height, centred - pillarboxes in a 16:9
  # window), so a menu point is scaled and moved to get the point of the game area.
  $ms = 1.0; $mox = 0.0; $moy = 0.0
  if ($Menu) {
    $c = New-Object W+RECT; [void][W]::GetClientRect($game, [ref]$c)
    $cw = $c.R - $c.L; $ch = $c.B - $c.T
    $ms = [Math]::Min($cw / 1024.0, $ch / 768.0)
    $mox = ($cw - 1024 * $ms) / 2; $moy = ($ch - 768 * $ms) / 2
    "Game area ${cw}x${ch}, menu scale $([Math]::Round($ms, 4)) offset $([Math]::Round($mox, 1)),$([Math]::Round($moy, 1))"
  }
  $n = 0
  foreach ($step in $Steps.Split(',')) {
    $vk, $sec = $step.Split('@', 2); $n++
    if ($sec -match '^(\d+)\?(.+)$' -and [regex]::IsMatch((Read-Logs), $Matches[2], 'Singleline')) {
      "Step ${n}: already done"; continue
    }
    [void][W]::SetForegroundWindow($game)
    if ($vk -match '^d(\d+):(\d+)-(\d+):(\d+)$') {
      # middle button drag (camera panning, src/LocoFix/mouse_pan.cpp): press at the first
      # point, move to the second in 10 steps over half a second, release
      $x0, $y0, $x1, $y1 = [int]$Matches[1], [int]$Matches[2], [int]$Matches[3], [int]$Matches[4]
      [void][W]::PostMessage($game, 0x8001, [IntPtr]0x0200, [IntPtr](($y0 -shl 16) -bor $x0))
      Start-Sleep -Milliseconds 150
      [void][W]::PostMessage($game, 0x8001, [IntPtr]0x0207, [IntPtr](($y0 -shl 16) -bor $x0))
      for ($i = 1; $i -le 10; $i++) {
        Start-Sleep -Milliseconds 50
        $x = $x0 + [int](($x1 - $x0) * $i / 10); $y = $y0 + [int](($y1 - $y0) * $i / 10)
        [void][W]::PostMessage($game, 0x8001, [IntPtr]0x0200, [IntPtr](($y -shl 16) -bor $x))
      }
      Start-Sleep -Milliseconds 100
      [void][W]::PostMessage($game, 0x8001, [IntPtr]0x0208, [IntPtr](($y1 -shl 16) -bor $x1))
    } elseif ($vk -eq 'w') {
      # the engine's mouse state (World.mouse at World+0x230, struct Mouse in re/types.txt) and
      # the menu item under the cursor (World.menu.hover, World+0x260), to see where the game
      # thinks the cursor is
      $world = [long][W]::Rd($p.Id, 0x4a9d38)
      $f = { param($o) [BitConverter]::ToSingle([BitConverter]::GetBytes([W]::Rd($p.Id, $world + $o)), 0) }
      $hover = [long][W]::Rd($p.Id, $world + 0x260)
      $hoverVid = if ($hover -ne 0) { [W]::Rd($p.Id, [long][W]::Rd($p.Id, $hover + 0x1c) + 4) } else { 0 }
      "Mouse: buttons=0x{0:x} world={1},{2} screen={3},{4} camera={5},{6} hover vid={7}" -f ([W]::Rd($p.Id, $world + 0x230)), (& $f 0x238), (& $f 0x23c), (& $f 0x240), (& $f 0x244), (& $f 0x44), (& $f 0x48), $hoverVid
      # the human player's state bar (World.players[0], struct Player): panel, bar objects,
      # the screen position of bar objects 80 and 102 (the first icon of the depot list and of
      # its second row, LocoFix depot_rows.cpp) and the flagman depot's queue
      $player = [long][W]::Rd($p.Id, $world + 0x220)
      $count = [W]::Rd($p.Id, $player + 0x18); $items = [long][W]::Rd($p.Id, $player + 0x20)
      $g = { param($o, $k) [BitConverter]::ToSingle([BitConverter]::GetBytes([W]::Rd($p.Id, $o + $k)), 0) }
      $pos = { param($i) if ($i -lt $count) { $o = [long][W]::Rd($p.Id, $items + 4 * $i); "{0},{1}" -f [int](& $g $o 0x30), [int]((& $g $o 0x34) - (& $g $o 0x38)) } else { "-" } }
      $flagman = [long][W]::Rd($p.Id, $player + 0x10)
      $queue = if ($flagman -ne 0) { $qn = [W]::Rd($p.Id, $flagman + 0x494); $q = @(); for ($i = 0; $i -lt [Math]::Min($qn, 10); $i++) { $q += [W]::Rd($p.Id, $flagman + 0xa4 + 2 * $i) -band 0xffff }; "$qn [" + ($q -join " ") + "]" } else { "-" }
      $baseAt = "-"; for ($i = 0; $i -lt $count; $i++) { $o = [long][W]::Rd($p.Id, $items + 4 * $i); if ($o -ne 0 -and ([W]::Rd($p.Id, [long][W]::Rd($p.Id, $o + 0x1c) + 4)) -eq 450) { $baseAt = "{0},{1},{2}" -f [int](& $g $o 0x30), [int](& $g $o 0x34), [int](& $g $o 0x38) } }
      "Bar base (VID 450) x,y,z: $baseAt"
      "Bar: panel={0} objects={1} icon80={2} icon102={3} queue={4}" -f (([W]::Rd($p.Id, $player + 0x2c)) -band 0xffef), $count, (& $pos 80), (& $pos 102), $queue
    } elseif ($vk -eq 't') {
      # the tooltip under the virtual cursor: MouseTip_Update 0x4257f0 keeps the current
      # tip text in the String at 0x4a9d58, so a test can check that a hover hit the
      # wanted menu item before clicking it
      $tipAddr = [long][W]::Rd($p.Id, 0x4a9d58)
      $tip = if ($tipAddr -ne 0) { [W]::Str($p.Id, $tipAddr, 128) } else { "" }
      "Tooltip: '$tip'"
    } elseif ($vk -cmatch '^([SL])(.+)$') {
      # editor mode: File > Save / Load of that map without the dialog
      $id = if ($Matches[1] -eq 'S') { 40002 } else { 40001 }
      $what = if ($Matches[1] -eq 'S') { "Save" } else { "Load" }
      $r = [W]::Command($game, $id, $Matches[2])
      "${what} $($Matches[2]): " + $(if ($r -eq 1) { "ok" } elseif ($r -lt 0) { "no answer" } else { "failed" })
    } elseif ($vk -cmatch '^Q(.*)$') {
      # the game's file dialog (a #32770 window of the process): the name typed into its file
      # name box (the Edit with id 1001) and OK, or Cancel
      $file = $Matches[1]
      $dlg = $null
      $cb = [W+EnumProc]{ param($h, $x) $id = 0; [void][W]::GetWindowThreadProcessId($h, [ref]$id); $c = New-Object Text.StringBuilder 64; [void][W]::GetClassName($h, $c, 64); if ($id -eq $p.Id -and [W]::IsWindowVisible($h) -and $c.ToString() -eq '#32770') { $script:dlg = $h }; $true }
      [void][W]::EnumWindows($cb, [IntPtr]::Zero)
      if (-not $dlg) { "Dialog: none open" } else {
        $t = New-Object Text.StringBuilder 128; [void][W]::GetWindowText($dlg, $t, 128); "Dialog: '$($t.ToString())'"
        if ($Shot) { Save-Shot $dlg ($Shot -replace '\.png$', "_${n}_dialog.png") }
        if ($file) {
          $edit = [W]::ChildById($dlg, 1001, "Edit")
          if ($edit -eq [IntPtr]::Zero) { $edit = [W]::GetDlgItem($dlg, 1152) }   # the old dialog
          [void][W]::SendText($edit, 0x000C, [IntPtr]::Zero, $file)
          [void][W]::PostMessage($dlg, 0x0111, [IntPtr]1, [IntPtr]::Zero)   # IDOK
        } else {
          [void][W]::PostMessage($dlg, 0x0111, [IntPtr]2, [IntPtr]::Zero)   # IDCANCEL
        }
      }
    } elseif ($vk -cmatch '^V(-?\d+):(-?\d+)$') {
      # editor test mode: the top-left of the view on the map
      "View $($Matches[1]),$($Matches[2]): " + [W]::Command($game, 40040, "$($Matches[1]) $($Matches[2])")
    } elseif ($vk -cmatch '^D(-?\d+):(-?\d+)$') {
      # editor test mode: the rail points near a map point into LocoFix.log
      "Rail points $($Matches[1]),$($Matches[2]): " + [W]::Command($game, 40041, "$($Matches[1]) $($Matches[2])")
    } elseif ($vk -cmatch '^([KU])(\d+)$') {
      # a key only pressed (K) or only let go (U), to hold Shift over a drag: K16,l..,U16
      $down = $Matches[1] -eq 'K'
      [void][W]::PostMessage($game, $(if ($down) { 0x0100 } else { 0x0101 }), [IntPtr][int]$Matches[2], [IntPtr]$(if ($down) { 1 } else { 0xC0000001 }))
      "Key $($Matches[2]) " + $(if ($down) { "down" } else { "up" })
    } elseif ($vk -cmatch '^Z(\d+):(\d+)$') {
      # editor test mode: File > Map size without the dialogs
      $r = [W]::Command($game, 40044, "$($Matches[1]) $($Matches[2])")
      "Map size $($Matches[1]) x $($Matches[2]): " + $(if ($r -eq 1) { "ok" } else { "failed" })
    } elseif ($vk -cmatch '^E(.+)$') {
      # editor test mode: File > Export map into that .zip without the dialogs
      $r = [W]::Command($game, 40045, $Matches[1])
      "Export $($Matches[1]): " + $(if ($r -eq 1) { "ok" } else { "failed" })
    } elseif ($vk -cmatch '^I(.+)$') {
      # editor test mode: File > Import map of that .zip, <zip>|overwrite|rename|ask (ask: the question is shown)
      $r = [W]::Command($game, 40046, $Matches[1])
      "Import $($Matches[1]): " + $(if ($r -eq 1) { "ok" } else { "failed" })
    } elseif ($vk -cmatch '^N(.+)$') {
      # editor test mode: File > New without the dialog, <name>:<ground line>[:<width>:<height>]
      $r = [W]::Command($game, 40043, ($Matches[1] -replace ':', ' '))
      "New map $($Matches[1]): " + $(if ($r -eq 1) { "ok" } else { "failed" })
    } elseif ($vk -cmatch '^M(.+)$') {
      # editor test mode: the mission settings of a file instead of File > Mission settings
      $r = [W]::Command($game, 40042, $Matches[1])
      "Mission settings $($Matches[1]): " + $(if ($r -eq 1) { "ok" } else { "failed" })
    } elseif ($vk -cmatch '^C(\d+)$') {
      # a character: the scripts' GetKey() reads WM_CHAR, and a posted key gives the lower case
      # one (the Shift of a posted key is not in the keyboard state) - C77 is the developer key M
      [void][W]::PostMessage($game, 0x0102, [IntPtr][int]$Matches[1], [IntPtr]1)
      "Character $($Matches[1])"
    } elseif ($vk -cmatch '^A(\d+)$') {
      # Alt+Tab away from the game (real keys, as a player would press them), a screenshot of
      # the game window while another one is in front, and after that many seconds Alt+Tab back
      $alttab = { [W]::keybd_event(0x12, 0, 0, [IntPtr]::Zero); Start-Sleep -Milliseconds 100; [W]::keybd_event(0x09, 0, 0, [IntPtr]::Zero)
        Start-Sleep -Milliseconds 50; [W]::keybd_event(0x09, 0, 2, [IntPtr]::Zero); Start-Sleep -Milliseconds 300; [W]::keybd_event(0x12, 0, 2, [IntPtr]::Zero) }
      & $alttab; Start-Sleep -Seconds 2
      "Alt+Tab away: game in front = $([W]::GetForegroundWindow() -eq $game), minimised = $([W]::IsIconic($game))"
      if ($Shot) {
        $r = New-Object W+RECT; [void][W]::GetWindowRect($game, [ref]$r); $file = $Shot -replace '\.png$', "_${n}_away.png"
        if ($r.R -gt $r.L) { $bmp = New-Object Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T); $g = [Drawing.Graphics]::FromImage($bmp)
          $hdc = $g.GetHdc(); [void][W]::PrintWindow($game, $hdc, 2); $g.ReleaseHdc($hdc); $bmp.Save($file); $g.Dispose(); $bmp.Dispose(); "Screenshot: $file" }
      }
      Start-Sleep -Seconds ([Math]::Max(0, [int]$Matches[1] - 2)); & $alttab; Start-Sleep -Seconds 1
      "Alt+Tab back: game in front = $([W]::GetForegroundWindow() -eq $game), minimised = $([W]::IsIconic($game))"
    } elseif ($vk -cmatch '^c(\d+)$') {
      [void][W]::PostMessage($game, 0x0111, [IntPtr][int]$Matches[1], [IntPtr]::Zero)
      "Command $($Matches[1])"
    } elseif ($vk -match '^([\^+])(\d+)$') {
      # a key with Ctrl (^) or Shift (+): the editor follows the modifier key's own messages
      # (posted keys do not change the keyboard state)
      $code = [int]$Matches[2]; $mod = if ($Matches[1] -eq '^') { 17 } else { 16 }
      [void][W]::PostMessage($game, 0x0100, [IntPtr]$mod, [IntPtr]1)
      [void][W]::PostMessage($game, 0x0100, [IntPtr]$code, [IntPtr]1)
      Start-Sleep -Milliseconds 100
      [void][W]::PostMessage($game, 0x0101, [IntPtr]$code, [IntPtr]0xC0000001)
      [void][W]::PostMessage($game, 0x0101, [IntPtr]$mod, [IntPtr]0xC0000001)
    } elseif ($vk -match '^l(\d+):(\d+)-(\d+):(\d+)$') {
      # left button drag: press at the first point, move to the second in 10 steps, release
      $x0, $y0, $x1, $y1 = [int]$Matches[1], [int]$Matches[2], [int]$Matches[3], [int]$Matches[4]
      [void][W]::PostMessage($game, 0x8001, [IntPtr]0x0200, [IntPtr](($y0 -shl 16) -bor $x0))
      Start-Sleep -Milliseconds 150
      [void][W]::PostMessage($game, 0x8001, [IntPtr]0x0201, [IntPtr](($y0 -shl 16) -bor $x0))
      for ($i = 1; $i -le 10; $i++) {
        Start-Sleep -Milliseconds 50
        $x = $x0 + [int](($x1 - $x0) * $i / 10); $y = $y0 + [int](($y1 - $y0) * $i / 10)
        [void][W]::PostMessage($game, 0x8001, [IntPtr]0x0200, [IntPtr](($y -shl 16) -bor $x))
      }
      Start-Sleep -Milliseconds 100
      [void][W]::PostMessage($game, 0x8001, [IntPtr]0x0202, [IntPtr](($y1 -shl 16) -bor $x1))
    } elseif ($vk -match '^x(\d+)$') {
      # a restart the step before started; when that step's end already followed it, the
      # game stays and this only waits (the @ part)
      $until = (Get-Date).AddSeconds(5)
      while (-not $p.HasExited -and (Get-Date) -lt $until) { Start-Sleep -Milliseconds 300; $p.Refresh() }
      if ($p.HasExited -and -not (Follow-Restart ([int]$Matches[1]))) { return }
    } elseif ($vk -match '^([kpm])(\d+):(\d+)(?::(\d+))?$') {
      # mouse at a point of the game area: m - move only, k - left button, p - right button.
      # WM_APP+1 with the mouse message in wParam - LocoFix test mode turns it into an
      # ordinary message and ignores the real mouse (src/LocoFix/test.cpp)
      $mx = [int][Math]::Round([int]$Matches[2] * $ms + $mox)
      $my = [int][Math]::Round([int]$Matches[3] * $ms + $moy)
      $l = [IntPtr](($my -shl 16) -bor $mx)
      [void][W]::PostMessage($game, 0x8001, [IntPtr]0x0200, $l)
      if ($Matches[1] -ne 'm') {
        $down, $up = if ($Matches[1] -eq 'k') { 0x0201, 0x0202 } else { 0x0204, 0x0205 }
        Start-Sleep -Milliseconds 150
        [void][W]::PostMessage($game, 0x8001, [IntPtr]$down, $l)
        # the game sees a world click (select, order) only when a frame runs while the
        # button is down; :<ms> holds it longer (e.g. p676:330:600)
        $hold = if ($Matches[4]) { [int]$Matches[4] } else { 100 }
        Start-Sleep -Milliseconds $hold
        [void][W]::PostMessage($game, 0x8001, [IntPtr]$up, $l)
      }
    } elseif ($vk -match '^s(\d+):(\d+):(-?\d+)$') {
      # the mouse wheel at a point: s<x>:<y>:<notches>, > 0 away from the user (up); the
      # turn rides in the high word of WM_APP+1's wParam (src/LocoFix/test.cpp)
      $mx = [int][Math]::Round([int]$Matches[1] * $ms + $mox)
      $my = [int][Math]::Round([int]$Matches[2] * $ms + $moy)
      $l = [IntPtr](($my -shl 16) -bor $mx)
      [void][W]::PostMessage($game, 0x8001, [IntPtr]0x0200, $l)
      Start-Sleep -Milliseconds 100
      $turn = ([int]$Matches[3] * 120) -band 0xffff
      [void][W]::PostMessage($game, 0x8001, [IntPtr](($turn -shl 16) -bor 0x020A), $l)
    } else {
      [void][W]::PostMessage($game, 0x0100, [IntPtr][int]$vk, [IntPtr]1)
      if ([int]$vk -eq 13 -or [int]$vk -eq 32 -or [int]$vk -eq 27) { [void][W]::PostMessage($game, 0x0102, [IntPtr][int]$vk, [IntPtr]1) }
      Start-Sleep -Milliseconds 100
      [void][W]::PostMessage($game, 0x0101, [IntPtr][int]$vk, [IntPtr]0xC0000001)
    }
    if ($sec -match '^(\d+)\?(.+)$') {
      $seen = Wait-Log $Matches[2] ([int]$Matches[1]); "Step ${n}: log matched = $seen"; Start-Sleep -Seconds 2
    } else {
      Start-Sleep -Seconds ([int]$sec)
    }
    $p.Refresh(); if ($p.HasExited -and -not (Follow-Restart 15)) { "Game exited after step $n"; return }
    if ($Shot -and -not $LastShotOnly) { Save-Shot $game ($Shot -replace '\.png$', "_$n.png") }
  }
}
$wins = New-Object System.Collections.ArrayList
$cb = [W+EnumProc]{ param($h, $x) $id = 0; [void][W]::GetWindowThreadProcessId($h, [ref]$id); if ($id -eq $p.Id -and [W]::IsWindowVisible($h)) { [void]$wins.Add($h) }; $true }
[void][W]::EnumWindows($cb, [IntPtr]::Zero)
foreach ($h in $wins) {
  $r = New-Object W+RECT; $c = New-Object W+RECT
  [void][W]::GetWindowRect($h, [ref]$r); [void][W]::GetClientRect($h, [ref]$c)
  $t = New-Object Text.StringBuilder 256; [void][W]::GetWindowText($h, $t, 256)
  $k = New-Object Text.StringBuilder 256; [void][W]::GetClassName($h, $k, 256)
  $style = [W]::GetWindowLong($h, -16)
  $caption = ($style -band 0x00C00000) -ne 0
  $menu = [W]::GetMenu($h) -ne [IntPtr]::Zero
  "Window '$t' class=$k rect=($($r.L),$($r.T))-($($r.R),$($r.B)) client=$($c.R)x$($c.B) style=0x{0:X8} frame/caption=$caption menu=$menu" -f $style
  if ($Shot -and $c.R -gt 0) {
    [void][W]::SetForegroundWindow($h); Start-Sleep -Milliseconds 1500
    $bmp = New-Object Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
    $g = [Drawing.Graphics]::FromImage($bmp)
    if ($Screen) { $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size) } else { $hdc = $g.GetHdc(); [void][W]::PrintWindow($h, $hdc, 2); $g.ReleaseHdc($hdc) }
    $bmp.Save($Shot); $g.Dispose(); $bmp.Dispose()
    "Screenshot: $Shot"
  }
}
$p.Refresh(); $run = ((Get-Date) - $p.StartTime).TotalSeconds
"CPU time: {0:N1} s of {1:N1} s running ({2:N0}% of one core)" -f $p.TotalProcessorTime.TotalSeconds, $run, (100 * $p.TotalProcessorTime.TotalSeconds / $run)
foreach ($h in $wins) { [void][W]::PostMessage($h, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) }
$deadline = (Get-Date).AddSeconds(10)
do { Start-Sleep -Milliseconds 250; $p.Refresh() } while (-not $p.HasExited -and (Get-Date) -lt $deadline)
if (-not $p.HasExited) { "Did not close by itself - killing it"; Stop-Process -Id $p.Id -Force } else { "Closed normally" }
