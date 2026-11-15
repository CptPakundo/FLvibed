# Read-only recorder: the real mouse's raw counts (Raw Input, a listener of this process) beside the game's own cursor
# (read from the game's memory), Windows' pointer and the mouse object's flags. Changes nothing in the game.
param([string]$PathLike = '*\Freelancer.exe', [int]$WaitSeconds = 600, [int]$MaxSeconds = 300, [string]$Out = "$PSScriptRoot\mouse_gain.csv", [int]$ProcessId = 0)
$ns = 'G' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
Add-Type -TypeDefinition @"
using System; using System.Collections.Generic; using System.Runtime.InteropServices; using System.Threading; using System.Diagnostics; using System.Text;
namespace $ns {
public static class Rec {
  [StructLayout(LayoutKind.Sequential)] struct RID { public ushort page, usage; public uint flags; public IntPtr target; }
  [StructLayout(LayoutKind.Sequential)] struct MSG { public IntPtr hwnd; public uint message; public IntPtr wParam, lParam; public uint time; public int x, y; }
  [StructLayout(LayoutKind.Sequential)] struct POINT { public int x, y; }
  [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern IntPtr CreateWindowExW(uint ex, string cls, string name, uint style, int x, int y, int w, int h, IntPtr parent, IntPtr menu, IntPtr inst, IntPtr param);
  [DllImport("user32.dll", SetLastError = true)] static extern bool RegisterRawInputDevices(RID[] d, uint n, uint size);
  [DllImport("user32.dll")] static extern int GetMessageW(out MSG m, IntPtr h, uint a, uint b);
  [DllImport("user32.dll")] static extern IntPtr DispatchMessageW(ref MSG m);
  [DllImport("user32.dll")] static extern uint GetRawInputData(IntPtr h, uint cmd, byte[] data, ref uint size, uint header);
  [DllImport("user32.dll")] static extern bool PostThreadMessageW(uint id, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] static extern bool GetCursorPos(out POINT p);
  [DllImport("user32.dll")] static extern IntPtr GetForegroundWindow();
  [DllImport("user32.dll")] static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("kernel32.dll")] static extern uint GetCurrentThreadId();
  [DllImport("kernel32.dll", SetLastError = true)] static extern IntPtr OpenProcess(uint access, bool inherit, int pid);
  [DllImport("kernel32.dll")] static extern bool ReadProcessMemory(IntPtr h, IntPtr addr, byte[] buf, int n, out IntPtr read);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  [DllImport("winmm.dll")] static extern uint timeBeginPeriod(uint ms);
  [DllImport("winmm.dll")] static extern uint timeEndPeriod(uint ms);
  static long rx, ry, rn, ix, iy, inn; static uint listenThread; static volatile bool listening; public static string ListenError = "";
  static void Listen() {
    listenThread = GetCurrentThreadId();
    IntPtr wnd = CreateWindowExW(0, "STATIC", "", 0, 0, 0, 0, 0, new IntPtr(-3), IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
    if (wnd == IntPtr.Zero) { ListenError = "window " + Marshal.GetLastWin32Error(); listening = true; return; }
    RID[] d = { new RID { page = 1, usage = 2, flags = 0x100, target = wnd } }; // RIDEV_INPUTSINK: also while another window is in front
    if (!RegisterRawInputDevices(d, 1, (uint)Marshal.SizeOf(typeof(RID)))) { ListenError = "register " + Marshal.GetLastWin32Error(); listening = true; return; }
    listening = true;
    byte[] buf = new byte[128]; MSG m; uint header = (uint)(8 + 2 * IntPtr.Size);
    while (GetMessageW(out m, IntPtr.Zero, 0, 0) > 0) {
      if (m.message == 0x00FF) { // WM_INPUT
        uint size = (uint)buf.Length;
        if (GetRawInputData(m.lParam, 0x10000003, buf, ref size, header) != 0xFFFFFFFF && BitConverter.ToInt32(buf, 0) == 0) {
          int o = (int)header; ushort fl = BitConverter.ToUInt16(buf, o);
          if ((fl & 1) == 0) { // relative motion
            int dx = BitConverter.ToInt32(buf, o + 12), dy = BitConverter.ToInt32(buf, o + 16);
            bool injected = (IntPtr.Size == 8 ? BitConverter.ToInt64(buf, 8) : BitConverter.ToInt32(buf, 8)) == 0;
            if (injected) { Interlocked.Add(ref ix, dx); Interlocked.Add(ref iy, dy); Interlocked.Increment(ref inn); }
            else { Interlocked.Add(ref rx, dx); Interlocked.Add(ref ry, dy); Interlocked.Increment(ref rn); }
          }
        }
      }
      DispatchMessageW(ref m);
    }
  }
  public static void Start() { var t = new Thread(Listen); t.IsBackground = true; t.Start(); while (!listening) Thread.Sleep(5); }
  public static string Counts() { return Interlocked.Read(ref rn) + " real events (" + Interlocked.Read(ref rx) + "," + Interlocked.Read(ref ry) + "), " + Interlocked.Read(ref inn) + " injected (" + Interlocked.Read(ref ix) + "," + Interlocked.Read(ref iy) + ")"; }
  // rows: t_us, rawX, rawY, rawN, injX, injY, cursorX, cursorY, pointerX, pointerY, flags, foreground
  public static int Record(int pid, int maxSeconds, string path) {
    IntPtr h = OpenProcess(0x0410, false, pid); // PROCESS_VM_READ | PROCESS_QUERY_INFORMATION
    if (h == IntPtr.Zero) return -Marshal.GetLastWin32Error();
    var sb = new StringBuilder(1 << 22); sb.Append("t_us,rawX,rawY,rawN,injX,injY,cx,cy,wx,wy,flags,fg\n");
    byte[] b = new byte[8], f = new byte[1]; IntPtr got; int rows = 0;
    long lrx = long.MinValue, lry = 0, lix = 0, liy = 0; int lcx = 0, lcy = 0, lwx = 0, lwy = 0, lfl = -1, lfg = -1;
    timeBeginPeriod(1); var sw = Stopwatch.StartNew(); Process p = Process.GetProcessById(pid);
    while (sw.Elapsed.TotalSeconds < maxSeconds) {
      if (!ReadProcessMemory(h, new IntPtr(0x616840), b, 8, out got)) { if (p.HasExited) break; Thread.Sleep(1); continue; }
      ReadProcessMemory(h, new IntPtr(0x6167CC), f, 1, out got);
      long ax = Interlocked.Read(ref rx), ay = Interlocked.Read(ref ry), an = Interlocked.Read(ref rn), bx = Interlocked.Read(ref ix), by = Interlocked.Read(ref iy);
      int cx = BitConverter.ToInt32(b, 0), cy = BitConverter.ToInt32(b, 4); POINT w; GetCursorPos(out w);
      uint fpid; GetWindowThreadProcessId(GetForegroundWindow(), out fpid); int fg = fpid == (uint)pid ? 1 : 0;
      if (ax != lrx || ay != lry || bx != lix || by != liy || cx != lcx || cy != lcy || w.x != lwx || w.y != lwy || f[0] != lfl || fg != lfg) {
        sb.Append(sw.ElapsedTicks * 1000000L / Stopwatch.Frequency).Append(',').Append(ax).Append(',').Append(ay).Append(',').Append(an).Append(',').Append(bx).Append(',').Append(by).Append(',')
          .Append(cx).Append(',').Append(cy).Append(',').Append(w.x).Append(',').Append(w.y).Append(',').Append(f[0].ToString("X2")).Append(',').Append(fg).Append('\n');
        lrx = ax; lry = ay; lix = bx; liy = by; lcx = cx; lcy = cy; lwx = w.x; lwy = w.y; lfl = f[0]; lfg = fg; ++rows;
      }
      Thread.Sleep(1);
    }
    timeEndPeriod(1); CloseHandle(h); System.IO.File.WriteAllText(path, sb.ToString()); return rows;
  }
}}
"@
$rec = "$ns.Rec" -as [type]
$rec::Start()
if ($rec::ListenError) { "listener failed: $($rec::ListenError)"; return }
$deadline = (Get-Date).AddSeconds($WaitSeconds); $p = $null
while (-not $p -and (Get-Date) -lt $deadline) {
    $p = if ($ProcessId) { Get-Process -Id $ProcessId -ErrorAction SilentlyContinue } else { Get-Process Freelancer -ErrorAction SilentlyContinue | Where-Object { $_.Path -like $PathLike } | Select-Object -First 1 }
    if (-not $p) { Start-Sleep -Milliseconds 250 }
}
if (-not $p) { 'no game appeared'; return }
"game pid $($p.Id) $($p.Path), started $($p.StartTime.ToString('HH:mm:ss.fff')); recording from $((Get-Date).ToString('HH:mm:ss.fff'))"
Set-Content "$Out.start.txt" (Get-Date).ToString('yyyy-MM-dd HH:mm:ss.fff')
$rows = $rec::Record($p.Id, $MaxSeconds, $Out)
"rows $rows -> $Out; $($rec::Counts()); ended $((Get-Date).ToString('HH:mm:ss.fff'))"
