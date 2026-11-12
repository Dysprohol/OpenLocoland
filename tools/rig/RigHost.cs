// Host-side helpers of tools\rig\rig.ps1: start Windows Sandbox without its window reaching the
// user's screen, and watch the user's desktop while it starts.
//
// Start(): WindowsSandbox.exe is started on a Win32 desktop of its own (CreateDesktop, the way
//   tools\test_game.ps1 runs the game quietly) and with SW_SHOWMINNOACTIVE; the processes it
//   starts inherit that desktop, so its windows are never shown on the user's.
// Watch(): a WinEvent hook on the user's desktop (out of context, its own message loop) that
//   logs every foreground change and every window a sandbox process shows there, and minimises
//   such a window at once without activating it - a safety net for what does come up there.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;

public static class RigHost {
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] struct STARTUPINFO {
    public int cb; public string lpReserved; public string lpDesktop; public string lpTitle;
    public int dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
    public short wShowWindow, cbReserved2; public IntPtr lpReserved2, hStdInput, hStdOutput, hStdError; }
  [StructLayout(LayoutKind.Sequential)] struct PROCESS_INFORMATION { public IntPtr hProcess, hThread; public int dwProcessId, dwThreadId; }
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr CreateDesktopW(string name, IntPtr dev, IntPtr mode, int flags, uint access, IntPtr sa);
  [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr OpenDesktopW(string name, int flags, bool inherit, uint access);
  [DllImport("user32.dll")] static extern bool CloseDesktop(IntPtr h);
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool CreateProcessW(string app, string cmd, IntPtr pa, IntPtr ta, bool inherit, uint flags, IntPtr env, string dir, ref STARTUPINFO si, out PROCESS_INFORMATION pi);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetWindowTextW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassNameW(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern bool IsIconic(IntPtr h);
  [DllImport("user32.dll")] public static extern IntPtr GetAncestor(IntPtr h, uint flags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] static extern bool EnumWindows(EnumProc f, IntPtr p);
  [DllImport("user32.dll")] static extern bool EnumDesktopWindows(IntPtr desk, EnumProc f, IntPtr p);
  delegate void WinEventProc(IntPtr hook, uint ev, IntPtr h, int obj, int child, uint thread, uint time);
  [DllImport("user32.dll")] static extern IntPtr SetWinEventHook(uint min, uint max, IntPtr mod, WinEventProc f, uint pid, uint tid, uint flags);
  [DllImport("user32.dll")] static extern bool UnhookWinEvent(IntPtr h);
  [StructLayout(LayoutKind.Sequential)] struct MSG { public IntPtr h; public uint m; public IntPtr w, l; public uint t; public int x, y; }
  [DllImport("user32.dll")] static extern int GetMessageW(out MSG m, IntPtr h, uint a, uint b);
  [DllImport("user32.dll")] static extern bool PostThreadMessageW(uint tid, uint m, IntPtr w, IntPtr l);
  [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();

  public static string Title(IntPtr h) { var s = new StringBuilder(256); GetWindowTextW(h, s, 256); return s.ToString(); }
  public static string Class(IntPtr h) { var s = new StringBuilder(256); GetClassNameW(h, s, 256); return s.ToString(); }
  public static string ProcName(IntPtr h) {
    uint pid; GetWindowThreadProcessId(h, out pid);
    try { return Process.GetProcessById((int)pid).ProcessName; } catch { return "pid" + pid; } }
  public static bool IsSandbox(IntPtr h) { string n = ProcName(h); return n.StartsWith("WindowsSandbox", StringComparison.OrdinalIgnoreCase); }
  public static string Describe(IntPtr h) { return h == IntPtr.Zero ? "(none)" : string.Format("0x{0:X} {1} [{2}] '{3}'", h.ToInt64(), ProcName(h), Class(h), Title(h)); }

  // Starts WindowsSandbox.exe <wsb> on desktop `desktop` (created if needed; null = the user's
  // desktop, minimised without activation). Returns the pid, or -error.
  static IntPtr desk = IntPtr.Zero;
  public static int Start(string wsb, string desktop) {
    string exe = Environment.ExpandEnvironmentVariables(@"%WINDIR%\System32\WindowsSandbox.exe");
    return Run(exe, "\"" + exe + "\" \"" + wsb + "\"", desktop);
  }
  // Starts any program on that desktop (null = the user's, minimised without activation).
  public static int Run(string exe, string cmdline, string desktop) {
    var si = new STARTUPINFO(); si.cb = Marshal.SizeOf(si);
    si.dwFlags = 1; si.wShowWindow = 7;   // STARTF_USESHOWWINDOW, SW_SHOWMINNOACTIVE
    if (!string.IsNullOrEmpty(desktop)) {
      desk = OpenDesktopW(desktop, 0, false, 0x10000000);
      if (desk == IntPtr.Zero) desk = CreateDesktopW(desktop, IntPtr.Zero, IntPtr.Zero, 0, 0x10000000, IntPtr.Zero);
      if (desk == IntPtr.Zero) return -Marshal.GetLastWin32Error();
      si.lpDesktop = "WinSta0\\" + desktop;
    }
    PROCESS_INFORMATION pi;
    if (!CreateProcessW(exe, cmdline, IntPtr.Zero, IntPtr.Zero, false, 0x08000000 /*CREATE_NO_WINDOW*/, IntPtr.Zero, null, ref si, out pi))
      return -Marshal.GetLastWin32Error();
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return pi.dwProcessId;
  }

  // The top-level windows of sandbox processes on a desktop (null = this thread's, the user's).
  static IntPtr held = IntPtr.Zero; static string heldName;
  public static List<IntPtr> SandboxWindows(string desktop) {
    var list = new List<IntPtr>();
    EnumProc f = (h, p) => { if (IsSandbox(h)) list.Add(h); return true; };
    if (string.IsNullOrEmpty(desktop)) EnumWindows(f, IntPtr.Zero);
    else {
      // The handle stays open: windows of another desktop can be read (class, title, process)
      // and captured only while this process holds that desktop open.
      if (held == IntPtr.Zero || heldName != desktop) { held = OpenDesktopW(desktop, 0, false, 0x10000000 /*GENERIC_ALL*/); heldName = desktop; }
      if (held != IntPtr.Zero) EnumDesktopWindows(held, f, IntPtr.Zero);
    }
    return list;
  }

  // The sandbox's own window (the remote session's client) on a desktop.
  public static IntPtr ClientWindow(string desktop) {
    foreach (IntPtr h in SandboxWindows(desktop))
      if (IsWindowVisible(h) && Class(h) == "WinUIDesktopWin32WindowClass" && ProcName(h) == "WindowsSandboxRemoteSession") return h;
    return IntPtr.Zero;
  }
  // ---- the host's monitors and the box window's place (in real pixels: a per-monitor-aware thread)
  [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr c);
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] struct MONITORINFOEX { public int cb; public RECT m, w; public int f; [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string dev; }
  delegate bool MonProc(IntPtr h, IntPtr dc, ref RECT r, IntPtr d);
  [DllImport("user32.dll")] static extern bool EnumDisplayMonitors(IntPtr dc, IntPtr clip, MonProc f, IntPtr d);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern bool GetMonitorInfoW(IntPtr h, ref MONITORINFOEX mi);
  [DllImport("shcore.dll")] static extern int GetDpiForMonitor(IntPtr m, int t, out uint x, out uint y);
  public class Monitor { public string Name; public int X, Y, W, H, Dpi; public bool Primary;
    public int Scale { get { return Dpi * 100 / 96; } }
    public override string ToString() { return string.Format("{0} {1}x{2} at {3},{4}, {5} %{6}", Name, W, H, X, Y, Scale, Primary ? ", primary" : ""); } }
  public static List<Monitor> Monitors() {
    var l = new List<Monitor>(); IntPtr old = SetThreadDpiAwarenessContext(new IntPtr(-4));
    EnumDisplayMonitors(IntPtr.Zero, IntPtr.Zero, (IntPtr h, IntPtr dc, ref RECT r, IntPtr d) => {
      var mi = new MONITORINFOEX(); mi.cb = Marshal.SizeOf(mi); GetMonitorInfoW(h, ref mi); uint x, y; GetDpiForMonitor(h, 0, out x, out y);
      l.Add(new Monitor { Name = mi.dev, X = mi.m.L, Y = mi.m.T, W = mi.m.R - mi.m.L, H = mi.m.B - mi.m.T, Dpi = (int)x, Primary = (mi.f & 1) != 0 }); return true; }, IntPtr.Zero);
    SetThreadDpiAwarenessContext(old); return l;
  }
  // Puts the window with its CLIENT area cw x ch at x, y (the frame is measured, not guessed);
  // never activates it, never changes its z-order. Returns the client size it got.
  public static string Place(IntPtr h, int x, int y, int cw, int ch) {
    IntPtr old = SetThreadDpiAwarenessContext(new IntPtr(-4));
    RECT r, c; GetWindowRect(h, out r); GetClientRect(h, out c);
    int fw = (r.R - r.L) - c.R, fh = (r.B - r.T) - c.B;
    bool ok = SetWindowPos(h, IntPtr.Zero, x, y, cw + fw, ch + fh, 0x0014 /*SWP_NOZORDER|SWP_NOACTIVATE*/);
    GetWindowRect(h, out r); GetClientRect(h, out c);
    SetThreadDpiAwarenessContext(old);
    return string.Format("{0}: window at {1},{2}, client {3}x{4}", ok ? "placed" : "SetWindowPos failed", r.L, r.T, c.R, c.B);
  }

  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f);
  // A picture of a window, also of one on a desktop that is not shown (PW_RENDERFULLCONTENT).
  public static string Capture(IntPtr h, string file) {
    RECT r; if (!GetWindowRect(h, out r)) return "no window";
    int w = r.R - r.L, hh = r.B - r.T; if (w <= 0 || hh <= 0) return "empty window";
    using (var bmp = new System.Drawing.Bitmap(w, hh)) {
      using (var g = System.Drawing.Graphics.FromImage(bmp)) { IntPtr dc = g.GetHdc(); bool ok = PrintWindow(h, dc, 2); g.ReleaseHdc(dc); if (!ok) return "PrintWindow failed"; }
      bmp.Save(file, System.Drawing.Imaging.ImageFormat.Png);
    }
    return string.Format("{0}x{1} at {2},{3}", w, hh, r.L, r.T);
  }

  // ---- the watcher on the user's desktop
  static Thread watchThread; static uint watchTid; static readonly List<string> events = new List<string>();
  static Stopwatch clock; static WinEventProc proc; static bool minimise;
  public static void Watch(bool minimiseSandboxWindows) {
    minimise = minimiseSandboxWindows; clock = Stopwatch.StartNew();
    lock (events) events.Add("0 ms start, foreground " + Describe(GetForegroundWindow()));
    var ready = new ManualResetEvent(false);
    watchThread = new Thread(() => {
      watchTid = GetCurrentThreadId();
      proc = (hook, ev, h, obj, child, thread, time) => {
        if (obj != 0 || h == IntPtr.Zero) return;   // OBJID_WINDOW only
        if (ev == 3) { lock (events) events.Add(clock.ElapsedMilliseconds + " ms foreground -> " + Describe(h)); }
        else if (ev == 0x8002 && GetAncestor(h, 2) == h && IsSandbox(h)) {   // EVENT_OBJECT_SHOW of a top-level window
          string what = clock.ElapsedMilliseconds + " ms SHOWN on the user's desktop: " + Describe(h) + (IsIconic(h) ? " (minimised)" : "");
          if (minimise && !IsIconic(h)) { ShowWindow(h, 6 /*SW_MINIMIZE: the next window is activated*/); what += " -> minimised"; }
          lock (events) events.Add(what);
        }
      };
      IntPtr h1 = SetWinEventHook(3, 3, IntPtr.Zero, proc, 0, 0, 0);            // EVENT_SYSTEM_FOREGROUND
      IntPtr h2 = SetWinEventHook(0x8002, 0x8002, IntPtr.Zero, proc, 0, 0, 0);  // EVENT_OBJECT_SHOW
      ready.Set();
      MSG m; while (GetMessageW(out m, IntPtr.Zero, 0, 0) > 0) { }
      UnhookWinEvent(h1); UnhookWinEvent(h2);
    });
    watchThread.IsBackground = true; watchThread.Start(); ready.WaitOne();
  }
  public static string[] Events() { lock (events) return events.ToArray(); }
  public static void StopWatch() {
    if (watchThread == null) return;
    PostThreadMessageW(watchTid, 0x0012, IntPtr.Zero, IntPtr.Zero); watchThread.Join(2000); watchThread = null;
    lock (events) events.Add(clock.ElapsedMilliseconds + " ms stop, foreground " + Describe(GetForegroundWindow()));
  }
}
