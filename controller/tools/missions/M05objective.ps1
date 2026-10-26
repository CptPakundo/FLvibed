# M05 (Cambridge -> Omega-3 -> Leeds) by the autopilot's objective mode (`ap start objective`): it flies every space
# part alone (the lanes, the Omega-3 gate, Sprague and the cloaked ambush, the flight from Sprague to the nebula,
# Baxter, the Baxter fight, the Cambridge hole, the traitors, Wotan, the escort, the Leeds hole, Planet Leeds); the
# harness only does the rooms (Sprague's scene chain and Baxter's bar scene: Complete-FLBaseStory; the Tobias scene at
# Leeds).
# Start: docked at Cambridge with the M05 offer accepted (campaign save Save1d87.fl, docs\missions\SAVES.md).
# Runbook: docs\missions\M05.md. Prints SUCCESS / FAILED and a total.
param([int]$TimeoutSec = 2400)
. (Join-Path $PSScriptRoot '..\flctl.ps1')
$t0 = Get-Date
$start = Get-FLEventSeq
# (to the host stream: inside Fly / Base a plain output would become part of their return value, not the log)
function Step($m) { Write-Host ("{0:HH:mm:ss} {1}" -f (Get-Date), $m) }
function Result {
    foreach ($l in (Get-FLEventLog $start)) {
        if ("$l" -match 'mission-message type=FAILURE|player-death') { return "FAILED: $l" }
        if ("$l" -match 'mission-message type=SUCCESS') { return 'SUCCESS' }
    }
}
# The objective mode in space until the ship docks (a room), the mission ends, or the time is up.
function Fly([string]$what) {
    Step "space: objective mode ($what)"
    $null = Send-FL 'ap start objective'
    $box = @{ end = $null }
    $null = Wait-FLUntil {
        $r = Result
        if ($r) { $box.end = $r; return $true }
        if ((Send-FL 'state') -match 'mode=docked') { $box.end = 'docked'; return $true }
        $false
    } "the objective mode's $what" $TimeoutSec # (Wait-FLUntil: no plan B - it would act on its own)
    $null = Send-FL 'ap stop'
    Step "-> $($box.end)"
    $box.end
}
# A base whose story plays out in rooms, then the launch.
function Base([string]$what) {
    Step "room: $what"
    $r = Complete-FLBaseStory -Until { [bool](Result) }
    Step "-> $r"
    $r
}
$null = Base 'the start base - base stop (repair, restock), launch' # never a bare launch (M04 20 Oct 2026)
$r = Fly 'lanes, the Omega-3 gate, Sprague, the cloaked ambush'
if ($r -eq 'docked') {
    $null = Base 'Sprague - the scenes with Juni (Planetscape, Digsite, Planetscape2, Spaceflight), launch'
    $r = Result; if (-not $r) { $r = Fly 'the flight from Sprague, the nebula, Baxter' }
}
if ($r -eq 'docked') {
    $null = Base 'Baxter - the bar scene, launch'
    $r = Result; if (-not $r) { $r = Fly 'the Baxter fight, the Cambridge hole, the traitors, Wotan, the escort, Leeds' }
}
if ($r -eq 'docked') {
    Step 'room: Leeds - Equipment (Tobias)'
    $null = Wait-FLUntil { $null = Close-FLBlocker; (Send-FL 'hotspots') -match 'ready|busy' } 'Leeds base' 60
    if (-not (Result) -and ((((Send-FL 'hotspots') -split "`n")[-1]) -notmatch 'busy')) {
        try { $null = Goto-FLRoom Equipment } catch { Step "Equipment: $_" }
    }
    $null = Wait-FLUntil { $null = Close-FLBlocker; if ((Send-FL 'hotspots') -match 'busy: scene') { $null = Skip-FLAnimation }; [bool](Result) } 'the Tobias scene, the mission result' 300
    $r = Result
}
$total = [int]((Get-Date) - $t0).TotalSeconds
if ($r -eq 'SUCCESS') { "SUCCESS total $total s" } else { "FAILED ($r) total $total s" }
