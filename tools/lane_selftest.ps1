# Self-test of tools\lane.ps1's path matching (no lane, no game, no registry touched): a lane's processes are matched by
# a path BOUNDARY, never by prefix. Written 13 Nov 2026 after `lane.ps1 remove fixes` was refused for the game of lane
# fixes3 (..\FLvibed-lane-fixes3 starts with ..\FLvibed-lane-fixes).
# How: two fake lane folders x and x3 in the temp folder; a copy of ping.exe runs from x3 with x3's path on its command
# line; lane.ps1's own functions (taken from its AST, the script itself is not run) must see it for x3 and not for x.
# Usage (PowerShell tool): tools\lane_selftest.ps1     exit 0 = PASS, 1 = FAIL.
$ErrorActionPreference = 'Stop'
$lane = Join-Path $PSScriptRoot 'lane.ps1'
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($lane, [ref]$tokens, [ref]$errors)
if ($errors.Count) { Write-Host "FAIL: lane.ps1 does not parse: $($errors[0].Message)"; exit 1 }
$want = 'Get-Ancestors', 'Get-LaneProcesses', 'Test-InFolder', 'Test-CommandLineInFolder', 'Get-Marker', 'Test-BuildInput', 'Copy-MarkerToTip'
foreach ($fn in $ast.FindAll({ param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] }, $true)) {
    if ($want -contains $fn.Name) { . ([scriptblock]::Create($fn.Extent.Text)) }
}
$fails = 0
function Check([string]$what, [bool]$ok) { Write-Host ("  {0}  {1}" -f $(if ($ok) { 'ok  ' } else { 'FAIL' }), $what); if (-not $ok) { $script:fails++ } }

$base = Join-Path ([IO.Path]::GetTempPath()) ("lane_selftest_" + [Guid]::NewGuid().ToString('N').Substring(0, 8))
$x = Join-Path $base 'FLvibed-lane-x'; $x3 = Join-Path $base 'FLvibed-lane-x3'
$null = [IO.Directory]::CreateDirectory($x); $null = [IO.Directory]::CreateDirectory($x3)
$exe = Join-Path $x3 'ping.exe'
[IO.File]::Copy((Join-Path $env:SystemRoot 'System32\PING.EXE'), $exe)
$p = Start-Process -FilePath $exe -ArgumentList '-n 60 127.0.0.1' -WindowStyle Hidden -PassThru
try {
    $t = [Diagnostics.Stopwatch]::StartNew()
    while (-not (Get-CimInstance Win32_Process -Filter "ProcessId=$($p.Id)") -and $t.Elapsed.TotalSeconds -lt 10) { Start-Sleep -Milliseconds 100 }
    Write-Host "lane.ps1 path matching (process pid $($p.Id) runs from $x3):"
    $inX3 = @(Get-LaneProcesses $x3 | Where-Object { $_.ProcessId -eq $p.Id })
    $inX = @(Get-LaneProcesses $x | Where-Object { $_.ProcessId -eq $p.Id })
    Check 'lane x3 sees the process run from x3' ($inX3.Count -eq 1)
    Check 'lane x does NOT see the process run from x3 (prefix of its folder)' ($inX.Count -eq 0)
    if (Get-Command Test-InFolder -ErrorAction SilentlyContinue) {
        Check 'Test-InFolder: x3\a.exe is in x3' (Test-InFolder "$x3\a.exe" $x3)
        Check 'Test-InFolder: x3 itself is in x3' (Test-InFolder $x3 $x3)
        Check 'Test-InFolder: x3\a.exe is not in x' (-not (Test-InFolder "$x3\a.exe" $x))
        Check 'Test-InFolder: case-insensitive' (Test-InFolder "$($x.ToUpperInvariant())\B" $x)
        Check 'Test-CommandLineInFolder: "x\tools\a.ps1" -k' (Test-CommandLineInFolder "pwsh -File `"$x\tools\a.ps1`" -k" $x)
        Check 'Test-CommandLineInFolder: x at the end' (Test-CommandLineInFolder "git -C $x" $x)
        Check 'Test-CommandLineInFolder: "x" quoted' (Test-CommandLineInFolder "git -C `"$x`" status" $x)
        Check 'Test-CommandLineInFolder: not x3\...' (-not (Test-CommandLineInFolder "`"$x3\ping.exe`" -n 60" $x))
        Check 'Test-CommandLineInFolder: not x-old' (-not (Test-CommandLineInFolder "cmd /c dir `"$x-old`"" $x))
        Check 'Test-CommandLineInFolder: x3 seen after an x-prefixed miss' (Test-CommandLineInFolder "a ${x3}b b `"$x3\c`"" $x3)
    } else { Check 'lane.ps1 defines Test-InFolder and Test-CommandLineInFolder' $false }
    # a sync that brings in no build input keeps the verify marker (14 Nov 2026)
    Write-Host 'lane.ps1 marker kept over a sync with no build input:'
    if ((Get-Command Test-BuildInput -ErrorAction SilentlyContinue) -and (Get-Command Copy-MarkerToTip -ErrorAction SilentlyContinue)) {
        foreach ($f in 'docs/TESTING.md', 'docs\kb\missions\M01.md', 'CHANGELOG.md', 'tests/base_pages.ps1', 'tools/nets.ps1', 'tools/claude/hooks/a.py', 'HANDOFF.txt') {
            Check "not a build input: $f" (-not (Test-BuildInput $f))
        }
        foreach ($f in 'core/flvibed.cpp', 'modules/hud/src/a.h', 'tests/hud_test.cpp', 'tools/buildlock.ps1', 'tools/lane.ps1', 'tools/module_lint.py', 'tools/hud_zones.tsv', 'docs/module_map.tsv', 'build.cmd', 'tools/setup/x.ps1') {
            Check "a build input: $f" (Test-BuildInput $f)
        }
        $wt = Join-Path $base 'wt'; $null = [IO.Directory]::CreateDirectory((Join-Path $wt '.lane\verified'))
        $old = 'a' * 40; $new = 'b' * 40; $tip = 'c' * 40
        $m = [ordered]@{ lane = 'x'; commit = $old; main = 'd' * 40; ok = $true; checks = @([ordered]@{ name = 'unit'; ran_by = 'lane.ps1'; exit = 0 }) }
        [IO.File]::WriteAllText((Get-Marker $wt $old), ($m | ConvertTo-Json -Depth 5))
        $r = Copy-MarkerToTip $wt $old $new $tip @('docs/A.md', 'tests/x.ps1')
        Check 'docs and net scripts only: a message is returned' ([bool]$r)
        $nm = if (Test-Path (Get-Marker $wt $new)) { Get-Content (Get-Marker $wt $new) -Raw | ConvertFrom-Json } else { $null }
        Check 'the new marker names the new tip, main, the old tip, and keeps the checks' ($nm -and $nm.commit -eq $new -and $nm.main -eq $tip -and $nm.carried_from -eq $old -and $nm.ok -and $nm.checks[0].name -eq 'unit')
        $r2 = Copy-MarkerToTip $wt $old ('e' * 40) $tip @('docs/A.md', 'core/x.cpp')
        Check 'a build input brought in: no marker, no message' (-not $r2 -and -not (Test-Path (Get-Marker $wt ('e' * 40))))
        $r3 = Copy-MarkerToTip $wt ('f' * 40) ('1' * 40) $tip @('docs/A.md')
        Check 'an unverified old tip: nothing carried' (-not $r3 -and -not (Test-Path (Get-Marker $wt ('1' * 40))))
    } else { Check 'lane.ps1 defines Test-BuildInput and Copy-MarkerToTip' $false }
} finally {
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
    $p.WaitForExit(5000) | Out-Null
    [IO.Directory]::Delete($base, $true)
}
if ($fails) { Write-Host "FAIL: $fails check(s)"; exit 1 }
Write-Host 'PASS'
exit 0
