# The story from a New Game, unattended: the Freeport 7 prologue, M01a, M01b, job board work until the game offers
# Mission 2, M02. Start: the game at its main menu (tools\deploy.ps1 -Background -Restart, no -Save).
# -From newgame | m01a | m01b | earn | m02 resumes at a phase (the game must be in that phase's start state).
# Between M01b and M02 the game wants more net worth (Content.dll story code, 18 Oct 2026): at M01b's end the target
# is the net worth then (cash + ship, equipment and cargo value) + 3500; it is checked as you dock / land, and then
# Mission 2 is loaded (objective 22210 "Meet Juni in the Manhattan Bar"). Jobs: the best paid visible Kill Ships job of
# the base you are docked at (Start-FLJob / Invoke-FLJob).
param([ValidateSet('newgame', 'm01a', 'm01b', 'earn', 'm02')][string]$From = 'newgame', [switch]$God, [int]$MaxJobs = 8)
. (Join-Path $PSScriptRoot '..\flctl.ps1')
. (Join-Path $PSScriptRoot '..\..\..\subtitles\tools\flsub.ps1')   # Send-FLS movieskip (a new game's intro movie)
$t0 = Get-Date
function Step($m) { "{0:HH:mm:ss} {1}" -f (Get-Date), $m }
$order = 'newgame', 'm01a', 'm01b', 'earn', 'm02'
$at = [array]::IndexOf($order, $From)
# A mission script prints one line per step and "SUCCESS" at the end.
function Mission($name) {
    $ok = $false
    $args2 = @{}; if ($God) { $args2.God = $true }
    & (Join-Path $PSScriptRoot "$name.ps1") @args2 | ForEach-Object { "  $name $_"; if ("$_" -match ' SUCCESS$') { $ok = $true } }
    if (-not $ok) { throw "$name did not succeed" }
}
try {
    if ($at -le 0) {
        Step 'New Game (the Freeport 7 prologue is a 68.5 s space cutscene, then the landing at Manhattan)'
        $r = Send-FL 'menu new'; if ($r -notmatch '^clicked') { throw "menu new: $r" }
        $null = Wait-FLUntil {
            $null = Close-FLBlocker
            $null = Send-FLS 'movieskip'
            (Get-FLMode) -eq 'docked'
        } 'docked at Manhattan' 600
        Step "landed: $(Get-FLStoryState)"
    }
    if ($at -le 1) { Step 'M01a'; Mission 'M01a' }
    if ($at -le 2) { Step 'M01b'; Mission 'M01b' }
    if ($at -le 3) {
        $null = Start-FLEventLog
        $offered = { @(Get-FLHistory 'obj=22210 ').Count -gt 0 }
        $jobs = 0
        # Mission 2 wants the net worth at M01b's end + 3500 (Content.dll story rule): job pay is the progress (a
        # resumed phase may need less; the game's own check decides).
        # What is still needed, from the game itself: its target (`worth-target`, the value the story code hands to
        # SetMoneyNeededToNextRank at M01b's end) minus its net worth now (debug `worth`: the same GetAssetValue +
        # InspectCash the story code uses). Fallbacks: StoryLvl's ini, then the vanilla 3500.
        $need = Get-FLWorthNeeded
        if ($null -eq $need) {
            $need = 3500
            $lvl = Join-Path $script:FLRoot 'Install\Freelancer HD Edition\DATA\MISSIONS\storylvl.ini'
            if (Test-Path -LiteralPath $lvl) {
                $v = Get-Content -LiteralPath $lvl | Where-Object { $_ -match '^\s*freetime_01_02\s*=\s*([\d.]+)' } | Select-Object -First 1
                if ($v -match '=\s*([\d.]+)') { $need = [double]$Matches[1] }
            }
        }
        Step "credits still needed for Mission 2: $need"
        while (-not (& $offered)) {
            if ($jobs -ge $MaxJobs) { throw "still no Mission 2 after $jobs jobs" }
            if ((Get-FLMode) -ne 'docked') {
                # The best board for the trip (Get-FLJobBases: game data pay / (job + travel time)); a refused dock (a
                # mooring fixture: NPC-only in the vanilla game) is skipped for the next.
                $skip = @()
                while ((Get-FLMode) -ne 'docked') {
                    $base = Get-FLJobBases -Need $need -Skip $skip | Select-Object -First 1
                    if (-not $base) { throw 'no job board to dock at' }
                    Step ("dock at {0} (jobs up to {1} credits, {2} s away, {3} job(s) for {4} credits: ~{5} s)" -f
                          $base.Name, $base.Pay, $base.Travel, $base.Jobs, $need, $base.Total)
                    $r = Start-FLTravel $base.Dock
                    # The travel ends as the docking cutscene starts: judge once it is over.
                    $null = Wait-FLUntil { $null = Skip-FLAnimation; (Get-FLMode) -in 'docked', 'space', 'space-ui' } 'docking over' 60
                    if ((Get-FLMode) -ne 'docked') { Step "travel: $($r.Why)"; $skip += $base.Dock }
                }
                $null = Wait-FLRoomReady
                if (& $offered) { break }
            }
            $job = Start-FLJob
            Step "job $($jobs + 1): type $($job.Type), $($job.Reward) credits"
            $r = Invoke-FLJob -Since $job.Since -Offer $job
            $live = Get-FLWorthNeeded
            $need = if ($null -ne $live) { $live } elseif ($r.Result -eq 'SUCCESS') { [Math]::Max(0, $need - $job.Reward) } else { $need }
            Step "job $($jobs + 1): $($r.Result), $($r.Secs) s; about $need credits still needed"
            $jobs++
        }
        Step "Mission 2 offered after $jobs jobs"
    }
    if ($at -le 4) {
        Step 'M02: to the Manhattan Bar, Juni, accept'
        if ((Get-FLMode) -ne 'docked' -or (Send-FL 'hotspots') -notmatch 'IDS_HOTSPOT_MISSIONVENDOR|Bar') {
            if ((Get-FLMode) -eq 'docked') { $null = Start-FLLaunch }
            $null = Start-FLTravel 2755957258   # Manhattan Docking Ring
        }
        $null = Wait-FLRoomReady
        $null = Goto-FLRoom Bar
        $null = Wait-FLUntil { $null = Close-FLBlocker; $null = Skip-FLAnimation; (Send-FL 'talkers') -match 'talk_story' } 'Juni (talk_story)' 120
        $null = Send-FL 'talk story'
        $null = Wait-FLUntil {
            $null = Close-FLBlocker
            if ((Send-FL 'visible OfferDialogAcceptButton') -match '^visible') { return $true }
            $null = Skip-FLAnimation
            $false
        } 'the Mission 2 offer' 180
        $null = Send-FL 'click OfferDialogAcceptButton'
        $null = Wait-FLObjective '22220' 0 120
        $null = Wait-FLRoomReady
        Step 'M02'; Mission 'M02'
    }
    Step 'CAMPAIGN DONE (New Game to M02 Success)'
} catch {
    Step "STOPPED: $_"
} finally {
    $null = Send-FL 'ap stop'
    Step ("total {0:N0} s" -f ((Get-Date) - $t0).TotalSeconds)
}
