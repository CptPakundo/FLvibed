# M03 by the autopilot's objective mode (`ap start objective`): it flies every space part alone (lane 42 -> 43, the
# convoy escort and its ambush, Willard, the Valkyries, CA Minor, Walker, the Battle of Willard); the harness only does
# the rooms (launch from Willard; the CA Minor bar scene, launch). Verified 19 Oct 2026: "Mission Result: Success".
# Start: docked at California Minor with the M03 offer accepted (campaign save Save1e7d.fl, docs\missions\SAVES.md).
# Runbook: docs\missions\M03.md. Prints SUCCESS / FAILED and a total.
param([int]$TimeoutSec = 2400)
. (Join-Path $PSScriptRoot '..\flctl.ps1')
$t0 = Get-Date
$start = Get-FLEventSeq
# (to the host stream: inside Fly a plain output would become part of its return value, not the log)
function Step($m) { Write-Host ("{0:HH:mm:ss} {1}" -f (Get-Date), $m) }
# The objective mode in space until the ship docks (a room), the mission ends, or the time is up.
function Fly([string]$what) {
    Step "space: objective mode ($what)"
    $null = Send-FL 'ap start objective'
    $box = @{ end = $null }
    $null = Wait-FLUntil {
        foreach ($l in (Get-FLEventLog $start)) {
            if ("$l" -match 'mission-message type=FAILURE|player-death') { $box.end = "FAILED: $l"; return $true }
            if ("$l" -match 'mission-message type=SUCCESS') { $box.end = 'SUCCESS'; return $true }
        }
        if ((Send-FL 'state') -match 'mode=docked') { $box.end = 'docked'; return $true }
        $false
    } "the objective mode's $what" $TimeoutSec # (Wait-FLUntil: no plan B - it would take the lane itself)
    $null = Send-FL 'ap stop'
    Step "-> $($box.end)"
    $box.end
}
# Every launch after the player's base stop (repair, restock: Complete-FLBaseStory), never a bare one (20 Oct 2026: M04
# runs launched with 1 nanobot / 0 batteries and died with nothing to press; the user restocked at stops).
function Done { foreach ($l in (Get-FLEventLog $start)) { if ("$l" -match 'mission-message type=(SUCCESS|FAILURE)|player-death') { return $true } }; $false }
Step 'room: CA Minor - base stop (repair, restock), launch'
$null = Complete-FLBaseStory -Until { Done }
$r = Fly 'lane, convoy, ambush, Willard'
if ($r -eq 'docked') {
    Step 'room: Willard - base stop, launch'
    $null = Complete-FLBaseStory -Until { Done }
    $r = Fly 'to CA Minor, Valkyries, dock'
}
if ($r -eq 'docked') {
    Step 'room: CA Minor - the bar (Walker scene), base stop, launch'
    $null = Wait-FLRoomReady
    $null = Goto-FLRoom Bar
    $null = Wait-FLObjective '22480' $start 180
    $null = Complete-FLBaseStory -Until { Done }
    $r = Fly 'Walker, the Battle of Willard'
}
$total = [int]((Get-Date) - $t0).TotalSeconds
if ($r -eq 'SUCCESS') { "SUCCESS total $total s" } else { "FAILED ($r) total $total s" }
