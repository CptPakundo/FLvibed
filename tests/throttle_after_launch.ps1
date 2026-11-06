# After a launch the ship holds full throttle while the variable PlayerThrottle reads 0. Under HUD 2 the throttle keys' step built on that 0, so the first
# W took the ship from 80 m/s to about 30 (FrontierSpace runs, 6 Nov 2026; every install). One background game: a save docked, a launch, then a short press of
# USER_INC_THROTTLE. Judged: the throttle reads the ship's 1 straight after the launch, and the press leaves the speed where it was. Covers fl::GetThrottle's
# callers on the keyboard path; the pad's step (controller_flight.cpp) reads the same function. Leaves out multiplayer: the game's getter is the same code there.
# Usage (PowerShell tool): tests\throttle_after_launch.ps1 [-Instance 1] [-Save SaveC701.fl]   build\release. Exit 1 = a failure.
param([string]$Instance = '1', [string]$Save = 'SaveC701.fl')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = $Instance
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Get-Reading([string]$key) { [double][regex]::Match("$(Send-FL 'ship')", "$key=(\S+)").Groups[1].Value }
& (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save $Save | Select-Object -Last 1
try {
    "launch: $(Start-FLLaunch)"
    $speed = Get-Reading 'speed'; $throttle = Get-Reading 'throttle'
    Check ($throttle -gt 0.99) "the throttle reads the ship's own after a launch ($throttle at $speed m/s)"
    $null = Send-FL 'fire USER_INC_THROTTLE 1'
    $low = $speed; $t = [Diagnostics.Stopwatch]::StartNew()
    while ($t.Elapsed.TotalSeconds -lt 0.3) { $low = [Math]::Min($low, (Get-Reading 'speed')) }
    $null = Send-FL 'fire USER_INC_THROTTLE 0'
    while ($t.Elapsed.TotalSeconds -lt 2) { $low = [Math]::Min($low, (Get-Reading 'speed')) }
    Check ($low -gt $speed * 0.9) "a short press keeps the speed (lowest $low of $speed m/s)"
} finally {
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
if ($bad.Count) { "throttle after launch: $($bad.Count) FAILED"; exit 1 }
'throttle after launch: OK'
