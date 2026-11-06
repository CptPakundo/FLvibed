# A held strafe must end with its release, on every install. FLvibed fires commands by the game's own ids; flightio::Hold sends a key-up only when the key map says so, and
# USER_MANEUVER_SLIDE_EVADE_UP / DOWN are in HDE's key map but not in vanilla 1.1's or a mod's built on it (docs\MODULES_PLAN.md "Design: commands the key map lacks").
# One background game: the pad's right stick held up, then let go, then down, with left as the control (a command every map lists). Judged: while held the ship slides,
# and within 2 s of the release it has stopped sliding. Covers the pad's strafe through Hold; the autopilot's slide claims the same Hold.
# Usage (PowerShell tool): tests\strafe_release.ps1 [-Instance 1] [-Save SaveC702.fl] [-Preset 1]   build\release. Exit 1 = a failure.
param([string]$Instance = '1', [string]$Save = 'SaveC702.fl', [int]$Preset = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = $Instance
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
# the ship's local velocity, right and up (debug `ship`: "local vel: right=x up=y forward=z")
function Get-Slide { $s = "$(Send-FL 'ship')"; [Math]::Max([Math]::Abs([double][regex]::Match($s, 'right=(-?[\d.]+)').Groups[1].Value), [Math]::Abs([double][regex]::Match($s, 'local vel: right=\S+ up=(-?[\d.]+)').Groups[1].Value)) }
function Test-Slide([string]$name, [double]$rx, [double]$ry) {
    Set-FLPad -RX $rx -RY $ry
    $top = 0.0; $t = [Diagnostics.Stopwatch]::StartNew()
    while ($t.Elapsed.TotalSeconds -lt 1.5) { $top = [Math]::Max($top, (Get-Slide)); Start-Sleep -Milliseconds 100 }
    Clear-FLPad
    $t.Restart(); $end = 99.0
    while ($t.Elapsed.TotalSeconds -lt 2.5) { $end = Get-Slide; Start-Sleep -Milliseconds 100 }
    Check ($top -gt 5) "${name}: the stick slides the ship (top $top m/s)"
    Check ($end -lt 2) "${name}: it stops after the release ($end m/s 2.5 s later)"
}
& (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save $Save | Select-Object -Last 1
try {
    "preset: $(Send-FL "set pad_preset $Preset")"
    Set-FLPad -LX 0.01; $null = Wait-FLFramesPassed 20; Clear-FLPad; $null = Wait-FLFramesPassed 20
    Test-Slide 'left' -1 0
    Test-Slide 'up' 0 1
    Test-Slide 'down' 0 -1
} finally {
    Clear-FLPad
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
if ($bad.Count) { "strafe release: $($bad.Count) FAILED"; exit 1 }
'strafe release: OK'
