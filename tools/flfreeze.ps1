# Freeze watchdog (21 Oct 2026: game 1 froze for 4 min 40 s in an M05 flee after two missile hits and resumed by itself;
# the comsvcs MiniDump is blocked by Defender, so nothing was learned). Dot-source it (tools\flcheckin.ps1 does):
#   Test-FLFrozen [-WindowSec 3]   the game process alive but its frame heartbeat (the controller host's shared memory,
#                                  offset 8, +1 every frame) unchanged over the window - read straight from the mapping,
#                                  no command to the game (a frozen game answers none). Also: the window responding.
#   Save-FLFreezeEvidence          into research\experience\freeze\<time>_i<n>\: threads.txt (every thread: CPU over a
#                                  2 s window, state / wait reason, start module+offset; the busy ones and the main
#                                  thread sampled 5x: EIP module+offset and the return-address candidates on the stack),
#                                  modules.txt, our logs (EXE\FL*.log) and the game's FLSpew.txt (shared by all
#                                  instances: AppData\Local\Freelancer), test.txt (the heartbeat test).
# Threads are suspended for microseconds per sample (Wow64GetThreadContext needs it); nothing else touches the game.
# $env:FLVIBED_INSTANCE picks the game as for the harness (unset = the main install).
. (Join-Path $PSScriptRoot 'fldata.ps1')

function Get-FLHeartbeat {
    $name = if ($env:FLVIBED_INSTANCE) { "Local\FLControllerShm_$($env:FLVIBED_INSTANCE)" } else { 'Local\FLControllerShm' }
    try { $mmf = [System.IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting($name) } catch { return $null }
    try {
        $v = $mmf.CreateViewAccessor()
        try { if ($v.ReadUInt32(0) -ne 0x54434C46) { return $null }; return $v.ReadUInt32(8) } finally { $v.Dispose() }
    } finally { $mmf.Dispose() }
}

function Test-FLFrozen([double]$WindowSec = 3) {
    $p = Get-FLGameProcess
    if (-not $p) { return [pscustomobject]@{ Running = $false; Frozen = $false } }
    $h0 = Get-FLHeartbeat
    $sw = [Diagnostics.Stopwatch]::StartNew()
    Start-Sleep -Milliseconds ([int]($WindowSec * 1000))
    $h1 = Get-FLHeartbeat
    $p.Refresh()
    $alive = -not $p.HasExited
    [pscustomobject]@{
        Running = $alive; Pid = $p.Id
        Frozen = $alive -and $null -ne $h0 -and $h0 -eq $h1
        Heartbeat0 = $h0; Heartbeat1 = $h1; Frames = if ($null -ne $h0 -and $null -ne $h1) { $h1 - $h0 } else { -1 }
        WindowSec = [math]::Round($sw.Elapsed.TotalSeconds, 2)
        Responding = $(try { $p.Responding } catch { $null })
    }
}

if (-not ('FLFreeze' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class FLFreeze
{
    [DllImport("kernel32")] static extern IntPtr OpenThread(int access, bool inherit, int id);
    [DllImport("kernel32")] static extern IntPtr OpenProcess(int access, bool inherit, int pid);
    [DllImport("kernel32")] static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32")] static extern int SuspendThread(IntPtr h);
    [DllImport("kernel32")] static extern int ResumeThread(IntPtr h);
    [DllImport("kernel32")] static extern bool Wow64GetThreadContext(IntPtr h, [In, Out] byte[] ctx);
    [DllImport("kernel32")] static extern bool ReadProcessMemory(IntPtr p, IntPtr addr, [Out] byte[] buf, int size, out IntPtr read);
    [DllImport("ntdll")] static extern int NtQueryInformationThread(IntPtr h, int cls, out IntPtr info, int len, IntPtr ret);
    [DllImport("psapi")] static extern bool EnumProcessModulesEx(IntPtr p, [Out] IntPtr[] mods, int cb, out int needed, int filter);
    [DllImport("psapi", CharSet = CharSet.Unicode)] static extern int GetModuleBaseNameW(IntPtr p, IntPtr mod, StringBuilder sb, int size);
    [StructLayout(LayoutKind.Sequential)] struct MODULEINFO { public IntPtr Base; public int Size; public IntPtr Entry; }
    [DllImport("psapi")] static extern bool GetModuleInformation(IntPtr p, IntPtr mod, out MODULEINFO mi, int cb);

    public class Module { public string Name; public long Base; public long Size; }
    public static List<Module> Modules(int pid)
    {
        var list = new List<Module>();
        IntPtr p = OpenProcess(0x0410, false, pid); // QUERY_INFORMATION | VM_READ
        if (p == IntPtr.Zero) return list;
        try
        {
            var mods = new IntPtr[1024];
            int needed;
            if (!EnumProcessModulesEx(p, mods, mods.Length * IntPtr.Size, out needed, 3)) return list; // LIST_MODULES_ALL
            for (int i = 0; i < Math.Min(needed / IntPtr.Size, mods.Length); ++i)
            {
                var sb = new StringBuilder(260);
                GetModuleBaseNameW(p, mods[i], sb, 260);
                MODULEINFO mi;
                if (GetModuleInformation(p, mods[i], out mi, Marshal.SizeOf(typeof(MODULEINFO))))
                    list.Add(new Module { Name = sb.ToString(), Base = mi.Base.ToInt64(), Size = mi.Size });
            }
        }
        finally { CloseHandle(p); }
        return list;
    }

    public static string Where(List<Module> mods, long a)
    {
        foreach (var m in mods)
            if (a >= m.Base && a < m.Base + m.Size) return m.Name + "+0x" + (a - m.Base).ToString("X");
        return "0x" + a.ToString("X");
    }

    public static long StartAddress(int tid)
    {
        IntPtr t = OpenThread(0x0040, false, tid); // QUERY_INFORMATION
        if (t == IntPtr.Zero) return 0;
        try { IntPtr a; return NtQueryInformationThread(t, 9, out a, IntPtr.Size, IntPtr.Zero) == 0 ? a.ToInt64() : 0; } // Win32StartAddress
        finally { CloseHandle(t); }
    }

    // One sample of a 32-bit thread: EIP, then the stack's dwords that point into a module image (return-address
    // candidates, innermost first; data pointers into images show up too).
    public static string Sample(int pid, int tid, List<Module> mods, int maxFrames)
    {
        IntPtr t = OpenThread(0x0002 | 0x0008, false, tid); // SUSPEND_RESUME | GET_CONTEXT
        if (t == IntPtr.Zero) return "(no access)";
        uint eip = 0, esp = 0;
        try
        {
            var ctx = new byte[716];                                         // WOW64_CONTEXT
            BitConverter.GetBytes(0x00010001).CopyTo(ctx, 0);                 // WOW64_CONTEXT_CONTROL
            if (SuspendThread(t) < 0) return "(suspend failed)";
            bool ok = Wow64GetThreadContext(t, ctx);
            ResumeThread(t);
            if (!ok) return "(no context)";
            eip = BitConverter.ToUInt32(ctx, 184);
            esp = BitConverter.ToUInt32(ctx, 196);
        }
        finally { CloseHandle(t); }
        var sb = new StringBuilder("eip " + Where(mods, eip) + " | stack");
        IntPtr p = OpenProcess(0x0010, false, pid);
        if (p == IntPtr.Zero) return sb.ToString();
        try
        {
            var buf = new byte[4096];
            IntPtr read;
            if (ReadProcessMemory(p, new IntPtr(esp), buf, buf.Length, out read))
            {
                int n = 0;
                for (int i = 0; i + 4 <= read.ToInt32() && n < maxFrames; i += 4)
                {
                    long v = BitConverter.ToUInt32(buf, i);
                    string w = Where(mods, v);
                    if (w.StartsWith("0x")) continue;
                    sb.Append(' ').Append(w);
                    ++n;
                }
            }
        }
        finally { CloseHandle(p); }
        return sb.ToString();
    }
}
'@
}

function Save-FLFreezeEvidence($Test = $null) {
    $p = Get-FLGameProcess
    if (-not $p) { return $null }
    $root = Split-Path $PSScriptRoot -Parent
    $tag = if ($env:FLVIBED_INSTANCE) { "i$($env:FLVIBED_INSTANCE)" } else { 'i0' }
    $dir = Join-Path $root "research\experience\freeze\$(Get-Date -Format 'yyyy-MM-dd_HHmmss')_$tag"
    New-Item -ItemType Directory -Force $dir | Out-Null
    if ($Test) { $Test | Format-List | Out-String | Set-Content (Join-Path $dir 'test.txt') }
    $mods = [FLFreeze]::Modules($p.Id)
    $mods | Sort-Object Base | ForEach-Object { '{0:X8} {1,10} {2}' -f $_.Base, $_.Size, $_.Name } | Set-Content (Join-Path $dir 'modules.txt')
    # per-thread CPU over a 2 s window
    $cpu0 = @{}; $p.Refresh(); foreach ($t in $p.Threads) { try { $cpu0[$t.Id] = $t.TotalProcessorTime.TotalMilliseconds } catch {} }
    $sw = [Diagnostics.Stopwatch]::StartNew(); Start-Sleep -Seconds 2; $win = $sw.Elapsed.TotalMilliseconds
    $p.Refresh()
    $rows = foreach ($t in $p.Threads) {
        $ms = try { $t.TotalProcessorTime.TotalMilliseconds } catch { -1 }
        $d = if ($cpu0.ContainsKey($t.Id) -and $ms -ge 0) { $ms - $cpu0[$t.Id] } else { -1 }
        $start = try { $t.StartTime } catch { $null }
        [pscustomobject]@{ Id = $t.Id; CpuPct = [math]::Round(100 * $d / $win, 1); CpuMs = [math]::Round($ms)
            State = "$($t.ThreadState)$(if ("$($t.ThreadState)" -eq 'Wait') { '/' + $t.WaitReason })"
            Start = [FLFreeze]::Where($mods, [FLFreeze]::StartAddress($t.Id)); StartTime = $start }
    }
    $main = $rows | Where-Object StartTime | Sort-Object StartTime | Select-Object -First 1
    $out = @("pid $($p.Id)  window $([math]::Round($win)) ms  threads $(@($rows).Count)  main thread $($main.Id)", '')
    $out += $rows | Sort-Object CpuPct -Descending | Format-Table Id, CpuPct, CpuMs, State, Start -AutoSize | Out-String -Width 220
    # the busy threads and the main thread: 5 samples each
    $hot = @($rows | Where-Object { $_.CpuPct -ge 10 -or $_.Id -eq $main.Id } | Sort-Object CpuPct -Descending | Select-Object -First 6)
    foreach ($r in $hot) {
        $out += "== thread $($r.Id) cpu $($r.CpuPct) % start $($r.Start)$(if ($r.Id -eq $main.Id) { ' (main)' })"
        for ($s = 0; $s -lt 5; $s++) { $out += "  $([FLFreeze]::Sample($p.Id, $r.Id, $mods, 16))"; Start-Sleep -Milliseconds 200 }
    }
    $out | Set-Content (Join-Path $dir 'threads.txt')
    $exe = Join-Path (Get-FLInstallDir) 'EXE'
    Get-ChildItem $exe -Filter 'FL*.log' -ErrorAction SilentlyContinue | Copy-Item -Destination $dir
    $spew = Join-Path $env:LOCALAPPDATA 'Freelancer\FLSpew.txt'
    if (Test-Path $spew) { Copy-Item $spew $dir }
    return $dir
}
