# Formats the C++ sources with clang-format (the Visual Studio LLVM toolset's x64 binary) by the repository's
# .clang-format - the style the code already uses, written down (24 Oct 2026). Formatting is whitespace only: build
# afterwards (tools\buildlock.ps1 dev / release / test) and commit the formatting on its own.
# Usage (PowerShell): tools\format.ps1 [-Check]   -Check: list the files that would change, exit 1 if any
param([switch]$Check)
$root = Split-Path $PSScriptRoot -Parent
$vs = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -property installationPath
$cf = Get-ChildItem "$vs\VC\Tools\Llvm\x64\bin\clang-format.exe" -ErrorAction SilentlyContinue
if (-not $cf) { $cf = Get-ChildItem "$vs\VC\Tools\Llvm\bin\clang-format.exe" -ErrorAction SilentlyContinue }
if (-not $cf) { throw "clang-format.exe not found under $vs\VC\Tools\Llvm (install the C++ Clang tools component)" }
$files = Get-ChildItem -Recurse -File -Include *.cpp, *.h (Join-Path $root 'core'), (Join-Path $root 'controller\src'),
         (Join-Path $root 'subtitles\src'), (Join-Path $root 'tests'), (Join-Path $root 'tools\sigtool') | ForEach-Object FullName
$changed = 0
foreach ($f in $files) {
    if ($Check) {
        $before = Get-Content -LiteralPath $f -Raw
        $after = & $cf.FullName --style=file "$f" | Out-String
        if ($before.Replace("`r`n", "`n").TrimEnd() -ne $after.Replace("`r`n", "`n").TrimEnd()) { $changed++; $f.Substring($root.Length + 1) }
    } else {
        & $cf.FullName --style=file -i "$f"
    }
}
if ($Check) { "$changed of $($files.Count) files would change"; exit [int]($changed -gt 0) }
"formatted $($files.Count) files with $($cf.FullName)"
