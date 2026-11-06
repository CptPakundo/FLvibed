# Windows 7 load check (a player's 0.1.0 report, 28 Oct 2026: an import of GetDpiForWindow, Windows 10 1607+, kept
# FLvibed.dll from loading on Windows 7 at all). Windows refuses a DLL or exe whose import table names a function its
# system DLLs lack, before any of our code runs; a newer function must be resolved at run time (GetProcAddress) instead.
# Every import (static and delay-load) of the shipped binaries is looked up in tools\win7_exports.tsv: the export tables
# of the real Windows 7 SP1 x86 system DLLs, 6.1.7601.23403, from Microsoft's update KB3125574. No function is taken on
# trust: an import that table lacks fails, whatever its documentation says. tools\win7_imports.tsv holds only what the
# update does not carry (ws2_32 by ordinal, the server's FLHook.dll, the names below).
# It also refuses api-ms-win-* / ext-ms-* API-set imports, a DLL outside both tables, and an operating-system or subsystem
# version in the PE header above 6.1. Then the run-time look-ups: every name in the DLL's text that a current system DLL
# exports and Windows 7 does not must be listed in core\host\win7sim.cpp, so FLVIBED_SIM_WIN7 refuses it in a test game
# and the fallback is exercised; a new one is checked for a fallback and listed.
# The toolset: a binary's Rich header names the build of every tool that made a part of it, so a toolset that no longer targets
# Windows 7 is refused even in one old static library. 14.44 stops at build 35229; 14.51's parts are 35721 and 36260.
# A real Windows 7 run is not proved here: this says the loader finds everything, the simulation that the fallbacks run.
# Usage (PowerShell): tools\win7_check.ps1 [-Files a.dll,b.exe]   (default: build\release\FLvibed.dll,
# build\setup\FLvibed-Setup.exe, build\flhook\flvibed_server.dll; a binary not built is skipped)   exit 1 = attention
#   tools\win7_check.ps1 -Reference <folder of the update's expanded DLLs>   rewrites tools\win7_exports.tsv
# tools\package.ps1 runs it before packaging (a release fails); tools\drift_check.ps1 reports it as 'win7'.
param([string[]]$Files = @(), [string]$Reference = '')
$root = Split-Path $PSScriptRoot -Parent
$table = Join-Path $PSScriptRoot 'win7_exports.tsv'
if (-not $Files) {
    $Files = 'build\release\FLvibed.dll', 'build\setup\FLvibed-Setup.exe', 'build\flhook\flvibed_server.dll' |
        ForEach-Object { Join-Path $root $_ }
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = if (Test-Path $vswhere) { & $vswhere -latest -products * -property installationPath } else { $null }
$dumpbin = if ($vs) {
    Get-ChildItem (Join-Path $vs 'VC\Tools\MSVC') -Directory -ErrorAction SilentlyContinue | Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName 'bin\Hostx64\x86\dumpbin.exe' } | Where-Object { Test-Path $_ } |
        Select-Object -First 1
}
if (-not $dumpbin) { 'win7: dumpbin.exe not found (Visual Studio C++ tools)'; exit 1 }

# name and ordinal of each export; a forwarded export counts: Windows 7's loader follows it inside its own DLLs
function Get-Exports([string]$file) {
    foreach ($l in (& $dumpbin /nologo /exports $file)) {
        if ($l -match '^\s+(\d+)\s+[0-9A-F]+\s+(?:[0-9A-F]{8}\s+)?([A-Za-z_?]\S*)') { [pscustomobject]@{ Ordinal = $Matches[1]; Name = $Matches[2] } }
    }
}

# The newest tool build in a binary's Rich header (0 = no header): the entries between the XOR-masked "DanS" and "Rich"
$noWin7Build = 35700 # VS 2026's toolsets begin above this; build.cmd's pin is below it
function Get-NewestTool([string]$file) {
    $b = [IO.File]::ReadAllBytes($file)
    $pe = [BitConverter]::ToInt32($b, 0x3C)
    for ($end = 0x80; $end -lt $pe - 4; $end += 4) { if ([BitConverter]::ToUInt32($b, $end) -eq 0x68636952) { break } }
    if ($end -ge $pe - 4) { return 0 }
    $key = [BitConverter]::ToUInt32($b, $end + 4)
    for ($start = $end - 8; $start -ge 0x80; $start -= 4) { if (([BitConverter]::ToUInt32($b, $start) -bxor $key) -eq 0x536E6144) { break } }
    if ($start -lt 0x80) { return 0 }
    $newest = 0
    for ($i = $start + 16; $i -lt $end; $i += 8) { $newest = [Math]::Max($newest, ([BitConverter]::ToUInt32($b, $i) -bxor $key) -band 0xFFFF) }
    $newest
}

if ($Reference) {
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add('# The export tables of the real Windows 7 SP1 x86 system DLLs, 6.1.7601.23403 (KB3125574, windows6.1-kb3125574-v4-x86_ba1ff5537312561795cc04db0b02fbb0a74b2cbd.msu,')
    $lines.Add('# SHA-1 as named, from download.windowsupdate.com). Written by tools\win7_check.ps1 -Reference. comctl32 = the side-by-side 6.0 (the setup''s manifest asks for it).')
    $lines.Add('# dll<TAB>name<TAB>ordinal')
    $want = 'advapi32', 'cfgmgr32', 'comctl32', 'gdi32', 'kernel32', 'ntdll', 'ole32', 'setupapi', 'shell32', 'user32', 'version', 'winmm'
    foreach ($n in $want) {
        $f = Get-ChildItem $Reference -Recurse -File -Filter "$n.dll" | Where-Object { $_.FullName -notmatch 'shell-comctl32-v5' } | Select-Object -First 1
        if (-not $f) { "win7: $n.dll not in $Reference"; exit 1 }
        foreach ($x in (Get-Exports $f.FullName | Sort-Object Name)) { $lines.Add("$n.dll`t$($x.Name)`t$($x.Ordinal)") }
    }
    [IO.File]::WriteAllLines($table, $lines)
    "win7: $($lines.Count - 3) exports written to $table"
    exit 0
}

$known = @{}; $ordinals = @{}; $names = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($line in [IO.File]::ReadLines($table)) {
    if ($line.StartsWith('#')) { continue }
    $c = $line -split "`t"
    $known[$c[0]] = $true
    $ordinals["$($c[0])|#$($c[2])"] = $true
    [void]$names.Add("$($c[0])|$($c[1])")
}
# the exceptions: dll <TAB> function ('#n' = by ordinal, '*' = every function of a DLL that is not Windows)
$allow = @{}; $textOk = New-Object 'System.Collections.Generic.HashSet[string]'
foreach ($line in Get-Content (Join-Path $PSScriptRoot 'win7_imports.tsv')) {
    if ($line -match '^\s*(#|$)') { continue }
    $c = $line -split "`t"
    $allow["$($c[0].ToLower())|$($c[1])"] = $true
    [void]$textOk.Add($c[1])
}
function Found([string]$dll, [string]$fn) {
    if ($allow["$dll|$fn"] -or $allow["$dll|*"]) { return $true }
    if ($fn.StartsWith('#')) { return [bool]$ordinals["$dll|$fn"] }
    $names.Contains("$dll|$fn")
}

$bad = @(); $checked = 0; $count = 0; $newestTool = 0
foreach ($f in $Files) {
    if (-not (Test-Path $f)) { continue }
    $checked++
    $name = Split-Path $f -Leaf
    $tool = Get-NewestTool $f
    if ($tool -eq 0) { $bad += "${name}: no Rich header, so its toolset cannot be told" }
    elseif ($tool -ge $noWin7Build) { $bad += "${name}: a part built by tool build $tool, a toolset that no longer targets Windows 7 (build.cmd's pin; a stale library in build\sdl3?)" }
    $newestTool = [Math]::Max($newestTool, $tool)
    $hdr = & $dumpbin /nologo /headers $f
    foreach ($k in 'operating system version', 'subsystem version') {
        $v = ($hdr | Where-Object { $_ -match "^\s*([\d.]+) $k" } | Select-Object -First 1) -replace '^\s*([\d.]+).*', '$1'
        if ($v -and [version]$v -gt [version]'6.1') { $bad += "${name}: $k $v (Windows 7 = 6.1)" }
    }
    $dll = $null
    foreach ($line in (& $dumpbin /nologo /imports $f)) {
        if ($line -match '^\s+Summary\s*$') { break } # the section sizes follow, not imports
        if ($line -match '^\s{4}(\S+\.(dll|DLL|exe|EXE))\s*$') { $dll = $Matches[1].ToLower(); continue }
        if (-not $dll) { continue }
        $fn = if ($line -match '^\s+[0-9A-F]+\s+(\S+)\s*$') { $Matches[1] }
              elseif ($line -match '^\s+Ordinal\s+(\d+)\s*$') { "#$($Matches[1])" } else { $null }
        if (-not $fn) { continue }
        $count++
        if ($dll -match '^(api|ext)-ms-') { $bad += "${name}: $dll!$fn (an API set: not on Windows 7)"; continue }
        if (-not (Found $dll $fn)) {
            $why = if ($known[$dll]) { 'Windows 7 does not export it' } else { 'a DLL outside tools\win7_exports.tsv and tools\win7_imports.tsv' }
            $bad += "${name}: $dll!$fn ($why)"
        }
    }
}
if ($checked -eq 0) { 'win7: no binary built (nothing to check)'; exit 0 }

# the run-time look-ups of the DLL
$dllFile = $Files | Where-Object { (Split-Path $_ -Leaf) -eq 'FLvibed.dll' -and (Test-Path $_) } | Select-Object -First 1
$lookups = 0
if ($dllFile) {
    $win7 = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($k in $names) { [void]$win7.Add(($k -split '\|', 2)[1]) }
    $now = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($n in 'kernel32', 'kernelbase', 'user32', 'gdi32', 'ole32', 'combase', 'shell32', 'shcore', 'advapi32', 'ntdll', 'winmm', 'setupapi', 'cfgmgr32', 'version', 'dwmapi', 'bcrypt', 'bcryptprimitives', 'oleaut32', 'imm32') {
        $p = Join-Path $env:windir "SysWOW64\$n.dll"
        if (Test-Path $p) { foreach ($x in (Get-Exports $p)) { [void]$now.Add($x.Name) } }
    }
    $listed = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($m in [regex]::Matches((Get-Content -Raw (Join-Path $root 'core\host\win7sim.cpp')), '^\s+"(\w+)",', 'Multiline')) { [void]$listed.Add($m.Groups[1].Value) }
    $text = [Text.Encoding]::GetEncoding(28591).GetString([IO.File]::ReadAllBytes($dllFile))
    $seen = New-Object 'System.Collections.Generic.HashSet[string]'
    foreach ($m in [regex]::Matches($text, '[A-Za-z_][A-Za-z0-9_]{3,80}')) { [void]$seen.Add($m.Value) }
    foreach ($s in $seen) {
        if (-not $now.Contains($s) -or $win7.Contains($s) -or $textOk.Contains($s)) { continue }
        $lookups++
        if (-not $listed.Contains($s)) { $bad += "FLvibed.dll: names $s, newer than Windows 7, not in core\host\win7sim.cpp (check its fallback, list it)" }
    }
}

if ($bad) {
    "win7: $($bad.Count) problem(s) in $checked binaries ($count imports):"
    $bad | ForEach-Object { "  $_" }
    '  -> a newer function: resolve it at run time with a fallback'
    exit 1
}
"win7: OK - $checked binaries, $count imports found in the real Windows 7 export tables (PE versions <= 6.1), newest tool build $newestTool, $lookups run-time look-ups of newer APIs all simulated"
exit 0
