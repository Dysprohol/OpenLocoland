# The guest's sound, INSIDE the sandbox (dot-sourced by agent.ps1 and runjob.ps1).
# The sandbox plays the guest's sound on the host (the remote desktop's audio redirection; the .wsb
# has a switch only for audio input). So the agent mutes every playback endpoint of the guest when the
# box starts: the devices stay (the game's DirectSound start and its playing are the same as on a real
# machine, only silent), the host's own volume mixer is never touched. A job with Sound = true
# (rig.ps1 -Sound) unmutes for its run and mutes again afterwards.
#   Set-GuestMute $true|$false   -> one line: endpoints, their mute and volume after the change
#   Get-AudioSessions            -> the processes that have an audio stream open (e.g. Locoland)
Add-Type @"
using System; using System.Runtime.InteropServices; using System.Collections.Generic;
[ComImport, Guid("5CDF2C82-841E-4546-9722-0CF74078229A"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioEndpointVolume {
  int RegisterControlChangeNotify(IntPtr p); int UnregisterControlChangeNotify(IntPtr p); int GetChannelCount(out uint c);
  int SetMasterVolumeLevel(float l, ref Guid ctx); int SetMasterVolumeLevelScalar(float l, ref Guid ctx);
  int GetMasterVolumeLevel(out float l); int GetMasterVolumeLevelScalar(out float l);
  int SetChannelVolumeLevel(uint ch, float l, ref Guid ctx); int SetChannelVolumeLevelScalar(uint ch, float l, ref Guid ctx);
  int GetChannelVolumeLevel(uint ch, out float l); int GetChannelVolumeLevelScalar(uint ch, out float l);
  int SetMute([MarshalAs(UnmanagedType.Bool)] bool m, ref Guid ctx); int GetMute([MarshalAs(UnmanagedType.Bool)] out bool m); }
[ComImport, Guid("D666063F-1587-4E43-81F1-B948E807363F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDevice { int Activate(ref Guid iid, int ctx, IntPtr p, [MarshalAs(UnmanagedType.IUnknown)] out object o);
  int OpenPropertyStore(int a, out IntPtr s); int GetId([MarshalAs(UnmanagedType.LPWStr)] out string id); int GetState(out int s); }
[ComImport, Guid("0BD7A1BE-7A1A-44DB-8397-CC5392387B5E"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDeviceCollection { int GetCount(out uint n); int Item(uint i, out IMMDevice d); }
[ComImport, Guid("A95664D2-9614-4F35-A746-DE8DB63617E6"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IMMDeviceEnumerator { int EnumAudioEndpoints(int flow, int mask, out IMMDeviceCollection c); int GetDefaultAudioEndpoint(int flow, int role, out IMMDevice d); }
[ComImport, Guid("BCDE0395-E52F-467C-8E3D-C4579291692E")] class MMDeviceEnumeratorCo { }
[ComImport, Guid("77AA99A0-1BD6-484F-8BC7-2C654C9A9B6F"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioSessionManager2 { int GetAudioSessionControl(IntPtr g, int f, out IntPtr c); int GetSimpleAudioVolume(IntPtr g, int f, out IntPtr v);
  int GetSessionEnumerator(out IAudioSessionEnumerator e); }
[ComImport, Guid("E2F5BB11-0570-40CA-ACDD-3AA01277DEE8"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioSessionEnumerator { int GetCount(out int n); int GetSession(int i, out IAudioSessionControl2 s); }
[ComImport, Guid("BFB7FF88-7239-4FC9-8FA2-07C950BE9C6D"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAudioSessionControl2 { int GetState(out int s); int GetDisplayName(out IntPtr n); int SetDisplayName(IntPtr n, IntPtr g); int GetIconPath(out IntPtr p);
  int SetIconPath(IntPtr p, IntPtr g); int GetGroupingParam(out Guid g); int SetGroupingParam(IntPtr g, IntPtr c); int RegisterAudioSessionNotification(IntPtr n);
  int UnregisterAudioSessionNotification(IntPtr n); int GetSessionIdentifier(out IntPtr s); int GetSessionInstanceIdentifier(out IntPtr s); int GetProcessId(out uint pid); }
public static class GuestAudio {
  static List<IMMDevice> Render() {
    var l = new List<IMMDevice>(); var en = (IMMDeviceEnumerator)new MMDeviceEnumeratorCo(); IMMDeviceCollection c;
    if (en.EnumAudioEndpoints(0 /*eRender*/, 1 /*ACTIVE*/, out c) != 0) return l;
    uint n; c.GetCount(out n); for (uint i = 0; i < n; i++) { IMMDevice d; c.Item(i, out d); l.Add(d); } return l; }
  static IAudioEndpointVolume Vol(IMMDevice d) { Guid iid = typeof(IAudioEndpointVolume).GUID; object o; d.Activate(ref iid, 23, IntPtr.Zero, out o); return (IAudioEndpointVolume)o; }
  public static string Mute(bool mute) {
    var parts = new List<string>(); Guid ctx = Guid.Empty;
    foreach (var d in Render()) { var v = Vol(d); v.SetMute(mute, ref ctx); bool m; float s; v.GetMute(out m); v.GetMasterVolumeLevelScalar(out s); string id; d.GetId(out id);
      parts.Add(string.Format("{0}: mute {1}, volume {2:0} %", id.Length > 20 ? id.Substring(id.Length - 12) : id, m, s * 100)); }
    return parts.Count == 0 ? "no playback endpoint" : parts.Count + " playback endpoint(s): " + string.Join("; ", parts.ToArray()); }
  public static string Sessions() {
    var pids = new List<string>();
    foreach (var d in Render()) {
      Guid iid = typeof(IAudioSessionManager2).GUID; object o; if (d.Activate(ref iid, 23, IntPtr.Zero, out o) != 0) continue;
      IAudioSessionEnumerator e; ((IAudioSessionManager2)o).GetSessionEnumerator(out e); int n; e.GetCount(out n);
      for (int i = 0; i < n; i++) { IAudioSessionControl2 s; e.GetSession(i, out s); uint pid; s.GetProcessId(out pid); int st; s.GetState(out st);
        string name = "?"; try { name = System.Diagnostics.Process.GetProcessById((int)pid).ProcessName; } catch { }
        if (pid != 0) pids.Add(name + " " + pid + (st == 1 ? " (playing)" : st == 0 ? " (inactive)" : " (expired)")); } }
    return pids.Count == 0 ? "none" : string.Join(", ", pids.ToArray()); }
}
"@
function Set-GuestMute([bool]$mute) { try { [GuestAudio]::Mute($mute) } catch { "audio: $($_.Exception.Message)" } }
function Get-AudioSessions { try { [GuestAudio]::Sessions() } catch { "audio sessions: $($_.Exception.Message)" } }
