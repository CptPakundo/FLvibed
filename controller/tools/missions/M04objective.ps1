# M04 "Escape from Liberty" by the autopilot's objective mode (`ap start objective`): it flies every space part alone
# (the escape via TL147, West Point and TL134, Benford's defence, the Magellan gate, the bounty hunters, Mactan, the
# Rheinland wing, the Leeds hole, Planet Leeds); the harness only does the rooms (Benford's bar scene and forced launch,
# the Mactan launch, the Tobias scene at Leeds).
# Start: docked at Manhattan with the M04 offer accepted (campaign save Save6380.fl, docs\missions\SAVES.md); the
# harness does the player's base stops (Manhattan before the launch, Mactan before the Rheinland fight).
# Runbook: docs\missions\M04.md. Prints SUCCESS / FAILED and a total.
param([int]$TimeoutSec = 2400)
. (Join-Path $PSScriptRoot '..\flctl.ps1')
$t0 = Get-Date
$start = Get-FLEventSeq
# (to the host stream: inside Fly a plain output would become part of its return value, not the log)
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
# A room scene that ends in the mission's own launch (or the mission's end): skipped like a player's Escape.
function SceneThenSpace([string]$what) {
    Step "room: $what"
    $null = Wait-FLUntil {
        $null = Close-FLBlocker
        if (Result) { return $true }
        if ((Send-FL 'hotspots') -match 'busy: scene') { $null = Skip-FLAnimation }
        (Send-FL 'state') -match 'mode=(space|cutscene)'
    } $what 300
}
# The player's base stop at Manhattan first (repair, restock, then the launch): the save starts with 1 nanobot and no
# batteries, the user launched M04 with 14 / 14 and 50 countermeasures bought here (2020 footage; 20 Oct 2026: a run
# with 1 / 0 died at the Unity battle with nothing left to press).
Step 'room: Manhattan - base stop (repair, restock), launch'
$null = Complete-FLBaseStory -Until { [bool](Result) }
$r = Fly 'the escape, West Point, to Benford'
if ($r -eq 'docked') {
    SceneThenSpace 'Benford: the Van Pelt scene, the forced launch'
    $r = Result; if (-not $r) { $r = Fly 'Benford, the Magellan gate, bounty hunters, Mactan' }
}
if ($r -eq 'docked') {
    # the player's base stop before the Rheinland fight (no scene, no timer while docked: runbook step 14): repair +
    # restock, then launch. Was a bare launch - 2 of 3 runs died in that fight with their nanobots / batteries spent
    # (20 Oct 2026); the user restocked at stops in this mission (kb consumables.md: countermeasures 0 -> 50).
    Step 'room: Mactan - base stop (repair, restock), launch'
    $null = Complete-FLBaseStory -Until { [bool](Result) }
    $r = Fly 'the Rheinland wing, the Leeds hole, Planet Leeds'
}
if ($r -eq 'docked') {
    Step 'room: Leeds - Equipment (Tobias)'
    $null = Wait-FLUntil { $null = Close-FLBlocker; (Send-FL 'hotspots') -match 'ready|busy' } 'Leeds base' 60
    if (-not (Result)) { try { $null = Goto-FLRoom Equipment } catch { Step "Equipment: $_" } }
    $null = Wait-FLUntil { $null = Close-FLBlocker; if ((Send-FL 'hotspots') -match 'busy: scene') { $null = Skip-FLAnimation }; [bool](Result) } 'the Tobias scene, the mission result' 300
    $r = Result
}
$total = [int]((Get-Date) - $t0).TotalSeconds
if ($r -eq 'SUCCESS') { "SUCCESS total $total s" } else { "FAILED ($r) total $total s" }
