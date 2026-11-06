# The autopilot's dock past the game's dock range. The game refuses such a request; the autopilot flies to the range by the game's Goto, then docks. On an install whose
# range is 10 km (vanilla 1.1, mods on it) that is any far station; HDE 0.7.1's 50 km hides it. One background game, a launch from the save's base, the nearest dockable
# base past the range. Judged: the dock does not end at once, the ship closes the distance, and it docks (a hostile death is a run to repeat, not a verdict).
# Usage (PowerShell tool): tests\dock_far.ps1 [-Instance 1] [-Save SaveC701.fl] [-TimeoutSec 260]   build\release. Exit 1 = a failure.
param([string]$Instance = '1', [string]$Save = 'SaveC701.fl', [int]$TimeoutSec = 260)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = $Instance
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
& (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save $Save | Select-Object -Last 1
try {
    if ((Get-FLMode) -eq 'docked') { "launch: $(Start-FLLaunch)" }
    $range = [double][regex]::Match("$(Send-FL 'pilot')", 'dockrange=(\d+)').Groups[1].Value
    $far = @(Get-FLContacts -Dock -All) | Where-Object { -not $_.Ship -and $_.Type -notmatch 'lane|ring|gate|hole' -and $_.Distance -gt $range } | Sort-Object Distance | Select-Object -First 1
    if (-not $far) { "no base past the dock range of $range m here: nothing to judge"; return }
    "range $range m, target $($far.Id) at $([int]$far.Distance) m: $("$(Send-FL "ap start dock $($far.Id)")".Substring(0, 60))"
    Start-Sleep -Seconds 3
    $st = "$(Send-FL 'ap status')"
    Check ($st -match 'mode=dock') "the dock goes on past the range ($([regex]::Match($st, 'mode=\S+').Value))"
    $t = [Diagnostics.Stopwatch]::StartNew(); $mode = ''
    while ($t.Elapsed.TotalSeconds -lt $TimeoutSec) {
        $mode = Get-FLMode
        if ($mode -ne 'space') { break }
        Start-Sleep -Milliseconds 500
    }
    Check ($mode -eq 'docked' -or $mode -eq 'cutscene') "it docks ($mode after $([int]$t.Elapsed.TotalSeconds) s)"
} finally {
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
if ($bad.Count) { "dock far: $($bad.Count) FAILED"; exit 1 }
'dock far: OK'
