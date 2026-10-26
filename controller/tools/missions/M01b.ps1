# M01b end to end (docs\missions\M01b.md) with the harness and the autopilot: launch, formation with Epsilon 1, the
# prison ship's two Rogue waves, the waypoint, Beta 4's attackers, the scouts, the Rogue base (platforms, waves,
# bombers), the clean-up and the flight home (no dock) - until "Mission Result: Success".
# Start: docked at Pittsburgh in M01b (campaign save Save5f61.fl, docs\missions\SAVES.md:
#   tools\deploy.ps1 -Background -Save <research\saves\bc46-all-missions\Save5f61.fl>).
# -From 5 resumes after the prison fight (the mission's own save 32110 "Continue the Search Pattern" puts you there).
# -God: the game's invincibility (test aid). Prints one line per step; stops at a failure / death.
# Verified 18 Oct 2026 without -God: Success (wave 1 4 kills in 27 s, wave 2 4 in 20 s, Beta 4 5 in ~60 s, base 7);
# a clean unattended rerun: Success in 824 s.
param([switch]$God, [int]$From = 0)
. (Join-Path $PSScriptRoot '..\flctl.ps1')
$t0 = Get-Date
function Step($m) { "{0:HH:mm:ss} {1}" -f (Get-Date), $m }
$start = Start-FLEventLog
if ($God) { $null = Send-FL 'godmode on' }
try {
    if ($From -le 4) {
        Step 'step 0: launch'
        $null = Start-FLLaunch
        Step 'step 1-2: formation objective 22010'
        $null = Wait-FLObjective '22010' $start 180
        $lead = (Get-FLMissionShips -Mission M01b | Where-Object { $_.Nickname -eq 'gmg_leader' } | Select-Object -First 1).Id
        $null = Start-FLFormation $lead
        Step "formed on Epsilon 1 ($lead); step 3-4: defend the prison ship (22020)"
        $null = Wait-FLObjective '22020' $start 400
        Step ("prison ship fight -> " + (Invoke-FLMissionFight '22025' $start 600))
    }
    # Fair information (user, 19 Oct 2026): fly the mission's own waypoints (what the player sees), never coordinates
    # from the runbook - a route to the Rogue Base appeared before the player could know where it is.
    Step 'step 5: the waypoint'
    $null = Start-FLTravel -NoWait
    $null = Wait-FLObjective '22045' $start 300
    $null = Send-FL 'ap stop'
    Step ("step 6: Beta 4's attackers -> " + (Invoke-FLMissionFight '22040' $start 600))
    Step 'step 7: the next waypoint'
    $null = Start-FLTravel -NoWait
    $null = Wait-FLObjective '22080' $start 300
    $null = Send-FL 'ap stop'
    Step ("step 8: scouts -> " + (Invoke-FLMissionFight '22085' $start 120))
    # to the base by the mission's waypoint (fair information), the fight starts in reach
    $null = Start-FLTravel -NoWait
    $r = Invoke-FLMissionFight '22095|22055' $start 600
    Step "step 9-11: the base -> $r"
    if ($r -eq '22095') { Step ("step 12: clean-up -> " + (Invoke-FLMissionFight '22055' $start 600)) }
    Step 'step 13: home (no dock)'
    $null = Start-FLTravel -NoWait   # the mission's waypoint list (no second, player waypoint on top)
    $null = Wait-FLUntil {
        $null = Close-FLBlocker
        foreach ($l in (Get-FLEventLog $start)) {
            if ("$l" -match 'mission-message type=FAILURE|player-death') { throw "mission failed: $l" }
            if ("$l" -match 'mission-message type=SUCCESS') { return $true }
        }
        $false
    } 'Mission Result: Success' 300
    Step 'SUCCESS'
} catch {
    Step "STOPPED: $_"
} finally {
    $null = Send-FL 'ap stop'
    Step ("total {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds)
}
