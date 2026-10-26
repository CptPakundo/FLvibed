# M01a end to end (docs\missions\M01a.md) with the harness and the autopilot: the Manhattan base part (Bar, Juni,
# the offer in the Equipment room), the launch and intro, the Order fight at Donau, TL146 -> 141, formation with the
# USV Brandt, the convoy's lane ride, the pirate ambush, the run to TL34 in formation, Pittsburgh - until "Mission
# Result: Success" (docked at Pittsburgh). Start: docked at Manhattan after the Freeport 7 prologue (a new game:
# tools\missions\Campaign.ps1, or Save2b9b.fl in the Bar). -From <step> resumes at a runbook step (the mission's own
# saves put you at steps 4 and 10). -God: the game's invincibility (test aid). One line per step; stops at a failure.
param([switch]$God, [int]$From = 0)
. (Join-Path $PSScriptRoot '..\flctl.ps1')
$t0 = Get-Date
function Step($m) { "{0:HH:mm:ss} {1}" -f (Get-Date), $m }
$start = Start-FLEventLog
function Mark { Update-FLEventLog; $script:FLLogSeq }
if ($God) { $null = Send-FL 'godmode on' }
try {
    if ($From -le 0) {
        Step 'step 0: Bar, Juni, the offer'
        $null = Wait-FLRoomReady
        if ((Send-FL 'talkers') -notmatch 'talk_story') { $null = Goto-FLRoom Bar }
        $null = Wait-FLUntil { $null = Close-FLBlocker; (Send-FL 'talkers') -match 'talk_story' } 'Juni in the Bar' 60
        $null = Send-FL 'talk story'
        $null = Wait-FLObjective '21820' $start 120
        # The offer plays in the Equipment room (autoplay scene, then the offer dialog).
        $null = Goto-FLRoom Equipment
        $null = Wait-FLUntil {
            $null = Close-FLBlocker
            if ((Send-FL 'visible OfferDialogAcceptButton') -match '^visible') { return $true }
            $null = Skip-FLAnimation
            $false
        } 'the offer dialog' 180
        $null = Send-FL 'click OfferDialogAcceptButton'
        $null = Wait-FLUntil { $null = Close-FLBlocker; (Send-FL 'hotspots') -match 'IDS_HOTSPOT_LAUNCH|Cityscape.*ready' } 'accepted (moved to Cityscape)' 120
        $null = Wait-FLObjective '21825' $start 60
        Step 'step 1-3: launch, intro (cutscenes waited out)'
        $null = Start-FLLaunch -TimeoutSec 300
    }
    if ($From -le 4) {
        $null = Wait-FLObjective '21830' $start 400
        $m = Mark
        Step ("step 4: the Order fighters -> " + (Invoke-FLMissionFight '21660' $m 600))
    }
    if ($From -le 7) {
        # Wait for King's instruction (21840 "Fly to Waypoint"): leaving early cuts the intro's dialogue (user, 18 Oct
        # 2026; an early arrival also cancels King's 0380 / 0390). A ship idle here died once ~30 s after the fight with
        # no attacker in the events (18 Oct 2026) - the wait logs hull and the big ships around until that is explained.
        Step 'step 5-7: wait for King, then to TL146 (within 2 km)'
        $st = @{ low = 2.0 }
        $null = Wait-FLUntil {
            $null = Close-FLBlocker
            $h = (Get-FLAutopilot).Hull
            if ($h -ge 0 -and $h -lt $st.low - 0.02) {
                $st.low = $h
                $big = (Get-FLContacts -All | Where-Object { $_.Ship -and $_.Type -notin 'fighter' -and $_.Distance -lt 3000 } |
                        ForEach-Object { "$($_.Type) $($_.Id) $($_.Distance)m h=$($_.Health)" }) -join '; '
                Step ("  hull {0:N2} near: {1}" -f $h, $big)
            }
            foreach ($l in (Get-FLEventLog $m)) { if ("$l" -match 'player-death|mission-message type=FAILURE') { throw "mission failed: $l" } }
            @(Get-FLEventLog $m | Where-Object { "$_" -match 'objective .*obj=(21840|21835) ' }).Count -gt 0
        } 'objective 21840 (Fly to Waypoint)' 240
        $null = Wait-FLFramesPassed 3
        for ($i = 0; $i -lt 5 -and (Close-FLBlocker); $i++) { $null = Wait-FLFramesPassed 2 }
        $null = Start-FLGoto Li01_Trade_Lane_Ring_146 -Within 1500
        $null = Stop-FLManeuver
    }
    if ($From -le 9) {
        Step 'step 8-9: dock with TL146 -> 145, exit at 141'
        $null = Wait-FLObjective '21875' $start 300
        $null = Start-FLTradeLane Li01_Trade_Lane_Ring_146 -Toward Li01_Trade_Lane_Ring_145
        $null = Wait-FLObjective '21845' $start 300
    }
    if ($From -le 11) {
        Step 'step 10-11: formation with the USV Brandt'
        # The Brandt is the ship 21845 "Fly to the Transport USV Brandt" marks (its own target=); forming during King's
        # dialog is allowed (pmn_P1_forms is armed at the exit of ring 141, runbook step 10).
        $brandt = $null
        foreach ($l in (Get-FLEventLog $start)) { if ("$l" -match ' objective count=.* obj=21845 .* target=(\d+)') { $brandt = [uint32]$Matches[1] } }
        if ($brandt) { $brandt = Resolve-FLObjectiveTarget $brandt } # a reputation instance, not the ship's id
        if (-not $brandt -or -not (Get-FLContacts -All | Where-Object { $_.Id -eq $brandt -and $_.Ship })) {
            $brandt = (Get-FLMissionShips -Mission M01a | Where-Object { $_.Nickname -eq 'transport1' } | Select-Object -First 1).Id
        }
        $m = Mark
        $null = Start-FLFormation $brandt
        Step "formed on the Brandt ($brandt)"
    }
    if ($From -le 13) {
        Step 'step 12-13: the convoy rides TL30 -> 31; the pirate ambush'
        $null = Wait-FLObjective '21860' $m 400
        $m = Mark
        $null = Stop-FLManeuver
        Step ("pirates -> " + (Invoke-FLMissionFight '21890' $m 600))
    }
    $m = Mark
    $hit = $null
    if ($From -le 15) {
        Step 'step 14-15: re-enter formation with the Brandt (rides TL34 -> 36)'
        if (-not $brandt -or -not (Get-FLContacts -All | Where-Object { $_.Id -eq $brandt -and $_.Ship })) {
            $brandt = (Get-FLMissionShips -Mission M01a | Where-Object { $_.Nickname -eq 'transport1' } | Select-Object -First 1).Id
        }
        $null = Start-FLFormation $brandt
    }
    Step 'step 16: dock at Pittsburgh'
    if ($From -le 15) { $hit = Wait-FLObjective '21870|21855' $m 600 }
    if ($hit -eq '21855') {
        # out of formation when the Brandt took TL34: take it yourself (runbook step 15, "bad" branch)
        Step 'not in formation: TL34 -> 35 -> 36 alone'
        $null = Start-FLTradeLane Li01_Trade_Lane_Ring_34 -Toward Li01_Trade_Lane_Ring_35
        $null = Wait-FLObjective '21870' $m 300
    }
    $r = Start-FLTravel Li01_02_dock_ring -TimeoutSec 400
    Step "Pittsburgh: $($r.Why)"
    $null = Wait-FLUntil {
        $null = Close-FLBlocker
        foreach ($l in (Get-FLEventLog $start)) {
            if ("$l" -match 'mission-message type=FAILURE|player-death') { throw "mission failed: $l" }
            if ("$l" -match 'mission-message type=SUCCESS') { return $true }
        }
        $false
    } 'Mission Result: Success' 120
    Step 'SUCCESS'
} catch {
    Step "STOPPED: $_"
} finally {
    $null = Send-FL 'ap stop'
    Step ("total {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds)
}
