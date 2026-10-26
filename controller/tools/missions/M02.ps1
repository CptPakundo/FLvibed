# M02 end to end (docs\missions\M02.md) with the harness and the autopilot: King's two trade lane lessons, the jump to
# Colorado, the waypoint and the practice scan, disabling Cosmo, the lane to Pueblo and its defence, the convoy hunt,
# Ashcroft and his pod, the jump home, the jailbreak fight, the last lane and Battleship Missouri - until "Mission
# Result: Success". Start: Manhattan with the mission accepted (objective 22220; campaign save Save05c7.fl,
# docs\missions\SAVES.md). -From <step> resumes at a runbook step (the mission's own saves put you at steps 3, 10, 11,
# 16). -God: the game's invincibility (test aid). Prints one line per step; stops at a failure / death.
# -SceneSaves: save natively at the scene points below and archive the mission's own checkpoints (scene saves:
# Start-FLScene M02 <step> then this script with -From <step>; catalogue docs\missions\scene_saves.tsv).
param([switch]$God, [int]$From = 0, [switch]$SceneSaves)
. (Join-Path $PSScriptRoot '..\flctl.ps1')
$t0 = Get-Date
function Step($m) { "{0:HH:mm:ss} {1}" -f (Get-Date), $m }
function MissionShip($nick) {
    (Get-FLMissionShips -Mission M02 | Where-Object { $_.Nickname -eq $nick } | Select-Object -First 1).Id
}
# A resumed run (-From, after Start-FLScene restarted the game on the scene) reads the session's whole history: the
# scene's objective came with the load, before this script (was: step 8 missed 22240, waited for 22245 and the scan
# then waited for the objective after it, holding still under Cosmo's fire, 19 Oct 2026).
$start = Start-FLEventLog
if ($From -gt 0) { $start = 0; $script:FLLogSeq = 0; $script:FLLog.Clear() }
$script:mark = $start
# The log position now (an objective that repeats is waited for only after this).
function Mark { Update-FLEventLog; $script:FLLogSeq }
# A scene save at a step (only with -SceneSaves, and not at the step this run resumed from: that save exists).
function Scene([int]$step, [string]$label) { if ($SceneSaves -and $From -ne $step) { $null = Save-FLScene M02 $step $label } }
if ($God) { $null = Send-FL 'godmode on' }
try {
    if ($From -le 1) {
        Step 'step 0-1: launch, intro scenes'
        $null = Start-FLLaunch
        $null = Wait-FLSpace
    }
    if ($From -le 3) {
        Step 'step 2: to the lane point with King (the mission waypoint)'
        # the mission's own waypoint list (Start-FLGoto -Position put a second, player waypoint on top: user, 19 Oct 2026)
        $null = Start-FLTravel -NoWait
        $null = Wait-FLObjective '22225' $start 120
        $null = Send-FL 'ap stop'
        Step 'step 3: lane 146 -> 145 (to 141)'
        $script:mark = Mark # 22225 repeats (take_tl1/2/4/5): later waits count only newer events
        $null = Start-FLTradeLane Li01_Trade_Lane_Ring_146 -Toward Li01_Trade_Lane_Ring_145
    }
    if ($From -le 4) {
        Step 'step 4: lane 16 -> 17 (to 20)'
        $null = Wait-FLObjective '22225' $script:mark 120
        $null = Start-FLTradeLane Li01_Trade_Lane_Ring_16 -Toward Li01_Trade_Lane_Ring_17
    }
    if ($From -le 5) {
        Step 'step 5: to the Colorado gate (within 3200 m), jump'
        $null = Start-FLTravel -NoWait   # the mission's waypoint by the gate (a point: the travel never jumps)
        $null = Wait-FLObjective '22230' $start 120
        $null = Start-FLJump Li01_to_Li03
    }
    if ($From -le 7) {
        Step 'step 6-7: Colorado waypoint 6311,0,81989'
        $null = Wait-FLObjective '22235' $start 120
        $null = Start-FLTravel -Position 6311,0,81989
    }
    if ($From -le 8) {
        Step 'step 8: practice scan - stop at the waypoint, scan every ship until Cosmo is scanned'
        # King: cut the engines at the waypoint and scan each ship (user, 18 Oct 2026: waiting for the 40 s timer
        # while King spots Cosmo was a bug).
        $null = Wait-FLObjective '22240|22245' $start 120 -NoPlanB
        Scene 8 'scan-stop'
        $m8 = Mark   # the scan ends at the next objective after this
        $n = Invoke-FLScanShips -After $m8 -TimeoutSec 240 -At 6311, 0, 81989   # hold at the waypoint (King: cut engines there)
        Step "scanned $n ship(s)"
        $null = Wait-FLObjective '22245' $start 300
    }
    if ($From -le 9) {
        Step 'step 9: disable Cosmo to 50 % (not destroy)'
        $cosmo = MissionShip 'PEscort1'
        $null = Start-FLAutopilot combat -Target $cosmo -Settings @{ engage = 0; disable_at = 0.5 }
        $null = Wait-FLObjective '22250' $start 300
        $null = Send-FL 'ap stop'
        $null = Send-FL 'ap set disable_at 0'
        $null = Send-FL 'ap set engage 1'
        Step 'Cosmo stands down; the long dialog'
        $null = Wait-FLObjective '22255' $start 240
    }
    if ($From -le 10) {
        Step 'step 10: lane 57 -> 58 (to Pueblo)'
        $null = Start-FLTradeLane Li03_Trade_Lane_Ring_57 -Toward Li03_Trade_Lane_Ring_58
    }
    if ($From -le 11) {
        Step ("step 11: defend Pueblo -> " + (Invoke-FLMissionFight '22260' $start 600))
    }
    if ($From -le 13) {
        Step 'step 12: fly to the marked ship (-28904,0,48313)'
        $null = Start-FLTravel -Position -28904,0,48313 -NoWait
        $null = Wait-FLObjective '22269' $start 400
        $null = Send-FL 'ap stop'
        Step ("step 13: the convoy escorts -> " + (Invoke-FLMissionFight '22262|22263' $start 600))
    }
    if ($From -le 14) {
        # The chase is a battle (nag 13093 at 5 km from King): escort King (stays with him, fights what threatens him)
        # rather than race ahead (cruise left King behind: "You ran from the battle", 18 Oct 2026).
        Step 'step 14: chase Ashcroft with King, bring Ashcroft to 35 %'
        # Two nags at once: toward the run point (23198) and within 5 km of King (13093): fly toward the point,
        # holding whenever King has fallen 3 km behind (a state condition each poll).
        $king = MissionShip 'King'
        $st = @{ hit = $null }
        $null = Wait-FLUntil {
            $null = Close-FLBlocker
            foreach ($l in (Get-FLEventLog $start)) {
                if ("$l" -match 'mission-message type=FAILURE|player-death') { throw "mission failed: $l" }
                if ("$l" -match 'objective .*obj=(22263) ') { $st.hit = $Matches[1] }
            }
            if ($st.hit) { return $true }
            $k = Get-FLContacts -All | Where-Object { $_.Id -eq $king } | Select-Object -First 1
            $mode = (Get-FLAutopilot).Mode
            if ($k -and $k.Distance -gt 2000) { if ($mode -ne 'off') { $null = Send-FL 'ap stop' } }
            elseif ($mode -eq 'off') { $null = Send-FL 'ap start travel -22908 0 74513' }
            $false
        } 'Ashcroft objective 22263' 300
        $null = Send-FL 'ap stop'
        $null = Send-FL 'ap stop'
        # The fight must stay within 5 km of King (nag 13093): escort King (fights what threatens him within
        # escort_radius, rejoins him) - the script destroys Ashcroft at 35 %.
        # flee 0 / caution 0: leaving King fails the mission - defend with repairs and evasion instead
        $null = Start-FLAutopilot escort -Target $king -Settings @{ escort_radius = 1500; flee = 0; caution = 0 }
        $null = Wait-FLObjective '22265' $start 300
        $null = Send-FL 'ap stop'
        $null = Send-FL 'ap set flee -1'; $null = Send-FL 'ap set caution 1'
        # Ashcroft's thugs are still around: the Loot behaviour does not fight (died tractoring, 18 Oct 2026) - clear
        # hostiles within 3 km first (the pod's timer allows 180 s from Ashcroft's end).
        Step 'clear the thugs near the pod'
        $null = Wait-FLUntil {
            $null = Close-FLBlocker
            $near = @(Get-FLContacts -Hostile | Where-Object { $_.Distance -lt 3000 })
            if (-not $near.Count) { return $true }
            if ((Get-FLAutopilot).Mode -in 'off', 'hold') { $null = Send-FL 'ap start combat' }
            $false
        } 'no hostiles within 3 km' 120
        $null = Send-FL 'ap stop'
        Step 'tractor the pod (the game Loot behaviour)'
        $pod = (Get-FLContacts -All | Where-Object { $_.Type -match 'loot' } | Sort-Object Distance | Select-Object -First 1).Id
        $null = Send-FL "ai dir loot $pod client=1"
        $null = Wait-FLObjective '22267|22268' $start 300
    }
    if ($From -le 15) {
        Step 'step 15: jump home'
        $script:mark = Mark
        $null = Start-FLJump Li03_to_Li01
    }
    if ($From -le 16) {
        Step 'step 16: lane 20 -> 19 (to 16)'
        if ($From -ge 16) { $script:mark = $start }
        $null = Wait-FLObjective '22225' $script:mark 180
        $script:mark = Mark
        $null = Start-FLTradeLane Li01_Trade_Lane_Ring_20 -Toward Li01_Trade_Lane_Ring_19
    }
    if ($From -le 17) {
        if ($From -ge 17) { $script:mark = $start }
        $null = Wait-FLObjective '22269' $script:mark 240
        Scene 17 'jailbreak-fight'
        $script:mark = Mark
        Step ("step 17: jailbreak wing -> " + (Invoke-FLMissionFight '22225' $script:mark 600))
    }
    Scene 18 'missouri-leg'
    Step 'step 18: lane 178 -> 169, dock at Missouri'
    $null = Start-FLTradeLane Li01_Trade_Lane_Ring_178
    $r = Start-FLTravel 2660772418 -TimeoutSec 240
    Step "Missouri: $($r.Why)"
    Step 'step 19: leave Deck2'
    $null = Wait-FLRoomReady
    $null = Goto-FLRoom Bar
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
    if ($SceneSaves) { try { Save-FLCheckpoints M02 $start } catch { Step "checkpoint archive: $_" } }
    Step ("total {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds)
}
