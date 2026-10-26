<#
Dumps every string-table entry and infocard (RT_HTML) of Freelancer's resource DLLs into a tab-separated
file: <global ids>\t<kind>\t<text>. Global ids = (dll index << 16) | local id, dll index 0 = resources.dll,
then the [Resources] order of EXE\freelancer.ini (the ids the game and mission scripts use).
The output is game content: write it outside the repository.

  dump-resources.ps1 -Game '<install>\' -Out '<file.tsv>'
#>
param(
    [Parameter(Mandatory)] [string] $Game,
    [Parameter(Mandatory)] [string] $Out
)
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;
public static class FlRes {
    [DllImport("kernel32", SetLastError=true, CharSet=CharSet.Unicode)]
    static extern IntPtr LoadLibraryEx(string path, IntPtr file, uint flags);
    [DllImport("kernel32")] static extern bool FreeLibrary(IntPtr h);
    [DllImport("kernel32")] static extern IntPtr FindResource(IntPtr h, IntPtr name, IntPtr type);
    [DllImport("kernel32")] static extern IntPtr LoadResource(IntPtr h, IntPtr res);
    [DllImport("kernel32")] static extern IntPtr LockResource(IntPtr res);
    [DllImport("kernel32")] static extern uint SizeofResource(IntPtr h, IntPtr res);
    delegate bool EnumNameProc(IntPtr h, IntPtr type, IntPtr name, IntPtr param);
    [DllImport("kernel32")] static extern bool EnumResourceNames(IntPtr h, IntPtr type, EnumNameProc proc, IntPtr param);

    static byte[] Raw(IntPtr h, IntPtr type, IntPtr name) {
        IntPtr r = FindResource(h, name, type); if (r == IntPtr.Zero) return null;
        uint n = SizeofResource(h, r); IntPtr p = LockResource(LoadResource(h, r));
        byte[] b = new byte[n]; Marshal.Copy(p, b, 0, (int)n); return b;
    }
    // returns (local id, kind, text)
    public static List<Tuple<int,string,string>> Dump(string path) {
        var list = new List<Tuple<int,string,string>>();
        IntPtr h = LoadLibraryEx(path, IntPtr.Zero, 0x2 /*AS_DATAFILE*/);
        if (h == IntPtr.Zero) return list;
        // string tables: block id b holds ids (b-1)*16 .. +15
        EnumResourceNames(h, (IntPtr)6, (hh, t, name, prm) => {
            int block = (int)name; byte[] b = Raw(hh, t, name); int o = 0;
            for (int i = 0; i < 16 && o + 2 <= b.Length; i++) {
                int len = BitConverter.ToUInt16(b, o); o += 2;
                if (len > 0) list.Add(Tuple.Create((block - 1) * 16 + i, "str", Encoding.Unicode.GetString(b, o, len * 2)));
                o += len * 2;
            }
            return true;
        }, IntPtr.Zero);
        EnumResourceNames(h, (IntPtr)23, (hh, t, name, prm) => {
            byte[] b = Raw(hh, t, name);
            int skip = (b.Length >= 2 && b[0] == 0xFF && b[1] == 0xFE) ? 2 : 0;
            list.Add(Tuple.Create((int)name, "html", Encoding.Unicode.GetString(b, skip, b.Length - skip)));
            return true;
        }, IntPtr.Zero);
        FreeLibrary(h);
        return list;
    }
}
'@

$exe = Join-Path $Game 'EXE'
$names = @('resources.dll')
$section = ''
foreach ($line in Get-Content (Join-Path $exe 'freelancer.ini')) {
    if ($line -match '^\s*\[([^\]]+)\]') { $section = $Matches[1] }
    elseif ($section -eq 'Resources' -and $line -match '^\s*DLL\s*=\s*([^;\s]+)') { $names += $Matches[1] }
}
$sb = [System.Text.StringBuilder]::new()
for ($i = 0; $i -lt $names.Count; $i++) {
    foreach ($e in [FlRes]::Dump((Join-Path $exe $names[$i]))) {
        $text = $e.Item3 -replace "`r", '\r' -replace "`n", '\n' -replace "`t", '\t'
        [void]$sb.Append((($i -shl 16) -bor $e.Item1)).Append("`t").Append($e.Item2).Append("`t").Append($text).Append("`n")
    }
    Write-Host ("{0,2} {1}" -f $i, $names[$i])
}
[IO.File]::WriteAllText($Out, $sb.ToString(), [Text.UTF8Encoding]::new($false))
Write-Host "wrote $Out"
