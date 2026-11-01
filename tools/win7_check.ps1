# Windows 7 load check (a player's 0.1.0 report, 28 Oct 2026: an import of GetDpiForWindow, Windows 10 1607+, kept
# FLvibed.dll from loading on Windows 7 at all). Windows refuses a DLL or exe whose import table names a function its
# system DLLs lack, before any of our code runs; a newer function must be resolved at run time (GetProcAddress) instead.
# This lists every import (static and delay-load) of the shipped binaries with dumpbin and compares it with
# tools\win7_imports.tsv, the allowlist of imports verified to exist on Windows 7 SP1. It also refuses api-ms-win-* /
# ext-ms-* API-set imports and an operating-system / subsystem version in the PE header above 6.1.
# A new import fails the check: look up its "Minimum supported client" on learn.microsoft.com; Windows 7 or older ->
# add "dll<TAB>function" to the allowlist (with the date); newer -> resolve it at run time with a fallback.
# Usage (PowerShell): tools\win7_check.ps1 [-Files a.dll,b.exe]   (default: build\release\FLvibed.dll,
# build\setup\FLvibed-Setup.exe, build\flhook\flvibed_server.dll; a binary not built is skipped)   exit 1 = attention
# tools\package.ps1 runs it before packaging (a release fails); tools\drift_check.ps1 reports it as 'win7'.
param([string[]]$Files = @())
$root = Split-Path $PSScriptRoot -Parent
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

# the allowlist: dll <TAB> function ('#n' = by ordinal, '*' = every function of a DLL that is not Windows')
$allow = @{}
foreach ($line in Get-Content (Join-Path $PSScriptRoot 'win7_imports.tsv')) {
    if ($line -match '^\s*(#|$)') { continue }
    $c = $line -split "`t"
    $allow["$($c[0].ToLower())|$($c[1])"] = $true
}
function Allowed([string]$dll, [string]$fn) { $allow["$dll|$fn"] -or $allow["$dll|*"] }

$bad = @(); $checked = 0; $count = 0
foreach ($f in $Files) {
    if (-not (Test-Path $f)) { continue }
    $checked++
    $name = Split-Path $f -Leaf
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
        if (-not (Allowed $dll $fn)) { $bad += "${name}: $dll!$fn (not in tools\win7_imports.tsv)" }
    }
}
if ($checked -eq 0) { 'win7: no binary built (nothing to check)'; exit 0 }
if ($bad) {
    "win7: $($bad.Count) import(s) / header(s) not verified for Windows 7 in $checked binaries ($count imports):"
    $bad | ForEach-Object { "  $_" }
    '  -> look up its "Minimum supported client"; Windows 7 or older: add it to tools\win7_imports.tsv; else resolve it at run time'
    exit 1
}
"win7: OK - $checked binaries, $count imports, all verified for Windows 7 (PE versions <= 6.1)"
exit 0
