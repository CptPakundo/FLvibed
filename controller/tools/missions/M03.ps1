# M03 end to end (docs\missions\M03.md, docs\kb\missions\M03.md) with the harness and the autopilot: from California
# Minor (offer accepted) - lane 42 -> 43, the convoy, the Barrera passage escort and its ambush, Willard, the
# Valkyries, CA Minor and Walker, the Battle of Willard - until "Mission Result: Success".
# Start: docked at California Minor with objective 22425 (campaign save Save1e7d.fl, docs\missions\SAVES.md:
#   tools\deploy.ps1 -Background -Save <research\saves\bc46-all-missions\Save1e7d.fl>).
# Fair information: targets and destinations from the objectives' own target= / markers and the mission's marks.
# -God: the game's invincibility (test aid). -SceneSaves: native saves at the scene points (Save-FLScene).
param([switch]$God, [int]$From = 5, [switch]$SceneSaves)
. (Join-Path $PSScriptRoot '..\flctl.ps1')
$t0 = Get-Date
function Step($m) { "{0:HH:mm:ss} {1}" -f (Get-Date), $m }
function MissionShip($nick) {
    (Get-FLMissionShips -Mission M03 | Where-Object { $_.Nickname -eq $nick } | Select-Object -First 1).Id
}
# The ship an objective points at (its target= entry: what the HUD marks for the player), else the mission ship.
function ObjectiveTarget([string]$obj, [string]$fallback) {
    $id = $null
    foreach ($l in (Get-FLEventLog $start)) { if ("$l" -match " objective count=.* obj=$obj .* target=(\d+)") { $id = [uint32]$Matches[1] } }
    # the objective's own target: a reputation instance (small, overlaps ship ids), resolved to the space object once
    # the ship is listed (it can be named a moment before it spawns)
    if ($id) {
        $box = @{ r = $null }
        try { $null = Wait-FLUntil { $box.r = Resolve-FLObjectiveTarget $id; [bool]$box.r } "objective target $id" 15 } catch { }
        if ($box.r) { return $box.r }
    }
    if ($fallback) { return MissionShip $fallback }
}
function Scene([int]$step, [string]$label) { if ($SceneSaves -and $From -ne $step) { $null = Save-FLScene M03 $step $label } }
function Wait-Success([int]$TimeoutSec) {
    $null = Wait-FLUntil {
        $null = Close-FLBlocker
        foreach ($l in (Get-FLEventLog $start)) {
            if ("$l" -match 'mission-message type=FAILURE|player-death') { throw "mission failed: $l" }
            if ("$l" -match 'mission-message type=SUCCESS') { return $true }
        }
        $false
    } 'Mission Result: Success' $TimeoutSec
}
$start = Start-FLEventLog
if ($From -gt 5) { $start = 0; $script:FLLogSeq = 0; $script:FLLog.Clear() } # a resumed scene: the session's history
if ($God) { $null = Send-FL 'godmode on' }
try {
    if ($From -le 5) {
        Step 'step 5: launch, lane 42 -> 43'
        $null = Start-FLLaunch
        $null = Wait-FLObjective '22430' $start 180
        $null = Start-FLTradeLane Li02_Trade_Lane_Ring_42 -Toward Li02_Trade_Lane_Ring_43
    }
    if ($From -le 6) {
        Step 'step 6: meet the convoy'
        $null = Wait-FLObjective '22435' $start 300
        $t1 = ObjectiveTarget '22435' 'transport1'
        Step "transport1 = $t1"
        $null = Start-FLFormation $t1   # the game's formation flies to it and keeps us with it
        $null = Wait-FLObjective '22500' $start 300
    }
    if ($From -le 8) {
        Step 'step 7-8: the passage entrance, escort past the buoys'
        $t1 = MissionShip 'transport1'
        if ((Get-FLPilot).Op -ne 'formation') { $null = Start-FLFormation $t1 }
        $null = Wait-FLObjective '22455' $start 600
        Scene 9 'convoy-ambush'
    }
    if ($From -le 10) {
        Step 'step 9: the ambush - defend the convoy (escort transport1, never far from it)'
        $t1 = MissionShip 'transport1'
        $null = Start-FLAutopilot escort -Target $t1 -Settings @{ escort_radius = 900; flee = 0; caution = 0 }
        $null = Wait-FLObjective '22510|22465' $start 600
        $null = Send-FL 'ap stop'; $null = Send-FL 'ap set flee -1'; $null = Send-FL 'ap set caution 1'
        Step 'step 10: all clear - back in formation to Willard'
        $null = Start-FLFormation $t1
        $null = Wait-FLObjective '22470' $start 600
    }
    if ($From -le 11) {
        Step 'step 11: dock at Willard'
        $null = Stop-FLManeuver
        $r = Start-FLTravel Li02_04 -TimeoutSec 300
        Step "Willard: $($r.Why)"
    }
    if ($From -le 12) {
        Step 'step 12: launch, fly towards CA Minor'
        $null = Wait-FLObjective '22515' $start 120
        $null = Start-FLLaunch
        $null = Wait-FLObjective '22475' $start 180
        $null = Start-FLTravel -NoWait
    }
    if ($From -le 13) {
        $null = Wait-FLObjective '22520' $start 600
        $null = Send-FL 'ap stop'
        Scene 13 'valkyries'
        Step 'step 13: the Valkyries (stay near Juni)'
        # Juni as the HUD shows her: a marked friendly named Juni (the mission's own marks; the script's spawn names
        # do not survive a resume), else the mission ship list
        $juni = Get-FLMarkedFriendly 0 "Juni Jun'ko Zane"   # her HUD name is "Jun'ko Zane"
        if (-not $juni) { $juni = MissionShip 'juni_from_willard' }; if (-not $juni) { $juni = MissionShip 'Juni' }
        $null = Start-FLAutopilot escort -Target $juni -Settings @{ escort_radius = 2000; flee = 0; caution = 0 }
        $null = Wait-FLObjective '22525' $start 600
        $null = Send-FL 'ap stop'; $null = Send-FL 'ap set flee -1'; $null = Send-FL 'ap set caution 1'
    }
    if ($From -le 14) {
        Step 'step 14: back to CA Minor and dock'
        $null = Start-FLTravel -NoWait
        $null = Wait-FLObjective '22415' $start 600
        $null = Send-FL 'ap stop'
        $r = Start-FLTravel Li02_02_dock_ring -TimeoutSec 400
        Step "CA Minor: $($r.Why)"
    }
    if ($From -le 16) {
        Step 'step 15: the bar (Walker scene), launch, fly to Walker'
        $null = Goto-FLRoom Bar
        $null = Wait-FLObjective '22480' $start 120
        $null = Start-FLLaunch
        $null = Wait-FLObjective '22485' $start 180
        # "Fly to Walker" names Walker (the objective's target=): a moving ship - join him in formation (meets him and
        # stays with him; flying his spawn point failed "meet up with Walker's Attack Group", 19 Oct 2026)
        $walker = ObjectiveTarget '22485' 'walker'
        Step "Walker = ${walker}: $(Join-FLShip $walker -Text 'Fly to Walker')"
        $null = Wait-FLObjective '22490' $start 600
        Scene 16 'to-willard-battle'
        Step 'step 16: fly to Willard with Walker'
        $p = Get-FLPilot
        if ($p.Op -ne 'formation' -and (Get-FLAutopilot).Mode -ne 'follow') { $null = Join-FLShip $walker -Text 'Walker' }
        $null = Wait-FLObjective '22495' $start 600
        $null = Send-FL 'ap stop'; $null = Stop-FLManeuver
    }
    Step 'step 17: the Battle of Willard'
    $null = Send-FL 'ap set flee 0'; $null = Send-FL 'ap set caution 0'
    $box = @{ next = [datetime]::MinValue }
    $null = Wait-FLUntil {
        $null = Close-FLBlocker
        foreach ($l in (Get-FLEventLog $start)) {
            if ("$l" -match 'mission-message type=FAILURE|player-death') { throw "mission failed: $l" }
            if ("$l" -match 'mission-message type=SUCCESS') { return $true }
        }
        # restart the fight while hostiles remain - but back off 10 s when the autopilot finds nothing it would fight
        # (invulnerable / capital contacts it rightly skips: a restart loop at 4/s after the battle, 19 Oct 2026)
        if ((Get-Date) -ge $box.next -and (Get-FLAutopilot).Mode -in 'off', 'hold' -and
            @(Get-FLContacts -Hostile | Where-Object { $_.Distance -lt 6000 }).Count) {
            $box.next = (Get-Date).AddSeconds(10)
            $null = Start-FLMissionCombat -MaxReach 6000
        }
        $false
    } 'Mission Result: Success' 900
    Step 'SUCCESS'
} catch {
    Step "STOPPED: $_"
} finally {
    $null = Send-FL 'ap stop'
    $null = Send-FL 'ap set flee -1'; $null = Send-FL 'ap set caution 1'
    Step ("total {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds)
}
