# Regression sweep: every area in its own section (PASS / FAIL with evidence), on to the next on failure. -Only <sections>.
# Results: research\experience\runs\sweep_<time>.txt; every section's time (and a mission's time per objective) goes to
# the benchmark history (tools\flbench.ps1, Show-FLBench). Each section starts on the fixture that reaches its subject
# fastest.
# Speed (user, 20 Oct 2026): one build per sweep (sections deploy -NoBuild); the short sections reuse the running game,
# loading their fixture natively (Load-FLSave) instead of a ~35 s restart. Safeguards (user: state that only resets at
# launch must not hide or fake a result): a fresh game for the first section, after any failure, for fresh-only sections
# (missions, the fight matrix, the release build), after $ReuseMax sections or $ReuseMinutes in one game, and when the
# pre-section check finds leftovers (autopilot on, spawns alive, wrong mode); a section that FAILS in a reused game runs
# again in a fresh one - passing then is reported as STATE-LEAK (a bug of its own). -Fresh: every section in a fresh
# game (full / milestone sweeps).
# Parallel (20 Oct 2026, user: "faster and more efficient testing is an important goal"): -Parallel <N> builds dev +
# release once, splits the sections into N groups balanced by their median time (benchmark history) and runs each group
# as its own sweep worker on game instance 1..N (deploy.ps1 -Instance: hard-linked install mirrors, own saves, logs,
# screenshots, debug channels) at the same time; the workers' results are merged into this sweep's file.
# -Instance <n> -SkipBuild: one such worker (what -Parallel starts).
param([string[]]$Only, [switch]$Fresh, [int]$ReuseMax = 5, [double]$ReuseMinutes = 20, [int]$Parallel = 0,
      [int]$Instance = 0, [switch]$SkipBuild, [switch]$Attended)
# -Attended: the user watches and the screen may be taken (display-matrix adds exclusive fullscreen); never for a
# background sweep while the user works.
$fl = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$deploy = Join-Path $fl 'tools\deploy.ps1'
if ($Instance -gt 0) { $env:FLVIBED_INSTANCE = "$Instance" }
$Only = @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ })   # 'a,b' from a pwsh -File command line
$res = Join-Path $fl ("research\experience\runs\sweep_{0:yyyy-MM-dd_HHmm}{1}.txt" -f (Get-Date), $(if ($Instance -gt 0) { "_i$Instance" } else { '' }))
. (Join-Path $fl 'controller\tools\flctl.ps1')
. (Join-Path $fl 'subtitles\tools\flsub.ps1')
. (Join-Path $fl 'tools\flbench.ps1')
if ($Parallel -gt 1) {
    $t0 = Get-Date
    $all = @([regex]::Matches((Get-Content -Raw $PSCommandPath), "(?m)^Section '([^']+)'") | ForEach-Object { $_.Groups[1].Value })
    $todo = if ($Only) { @($all | Where-Object { $_ -in $Only }) } else { $all }
    $hist = @(Get-FLBench | Where-Object { $_.suite -eq 'sweep' -and $_.outcome -eq 'PASS' })
    $cost = @{}
    foreach ($s in $todo) {
        $v = @($hist | Where-Object case -eq $s | ForEach-Object { [double]$_.seconds } | Sort-Object)
        $cost[$s] = if ($v.Count) { $v[[int][Math]::Floor(($v.Count - 1) / 2)] } else { 120 }
    }
    # longest first onto the least loaded worker (keeps the slowest worker - the sweep's time - short)
    $groups = @(1..$Parallel | ForEach-Object { [pscustomobject]@{ n = $_; secs = 0.0; list = [Collections.Generic.List[string]]::new() } })
    foreach ($s in ($todo | Sort-Object { -$cost[$_] })) {
        $g = $groups | Sort-Object secs | Select-Object -First 1
        $g.list.Add($s); $g.secs += $cost[$s]
    }
    "{0:HH:mm:ss} parallel sweep, {1} workers: {2}" -f (Get-Date), $Parallel, (($groups | ForEach-Object { "i$($_.n) ~$([int]$_.secs) s: $($_.list -join ',')" }) -join ' | ') | Tee-Object -FilePath $res
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    try { $null = & $deploy -Release -NoStart 2>&1; $null = & $deploy -NoStart 2>&1 } catch { "BUILD FAILED: $_" | Tee-Object -Append -FilePath $res; return }
    $procs = foreach ($g in $groups | Where-Object { $_.list.Count }) {
        $a = @('-NoProfile', '-File', ('"' + $PSCommandPath + '"'), '-Instance', "$($g.n)", '-SkipBuild', '-Only') + @($g.list -join ',')
        if ($Fresh) { $a += '-Fresh' }
        if ($Attended) { $a += '-Attended' }
        Start-Process pwsh -WindowStyle Hidden -ArgumentList $a -PassThru
    }
    $procs | Wait-Process
    $runs = Join-Path $fl 'research\experience\runs'
    foreach ($f in Get-ChildItem $runs -Filter 'sweep_*_i*.txt' | Where-Object LastWriteTime -ge $t0) {
        Add-Content $res "---- $($f.Name)"; Add-Content $res (Get-Content $f.FullName)
    }
    "{0:HH:mm:ss} parallel sweep done in {1:N0} s (serial estimate {2:N0} s): {3}" -f (Get-Date), ((Get-Date) - $t0).TotalSeconds,
        (($cost.Values | Measure-Object -Sum).Sum), $res | Tee-Object -Append -FilePath $res
    return
}
# Screenshots: every fresh Boot (deploy.ps1 -Background) starts the sampler into research\experience\shots\<time>
function Out-R($s) { $l = "{0:HH:mm:ss} {1}" -f (Get-Date), $s; $l; Add-Content $res $l }
$script:game = @{ fresh = $true; started = $null; sections = 0; reused = $false }
# Quality, not just completion (user, 20 Oct 2026: navigation passed for days at 11 min while flying past a lane): each
# section runs under a time budget - its own ($budget) or 1.5 x the median of its earlier passes (benchmark history,
# 3+ runs) plus a game start - enforced live (Set-FLDeadline: every harness wait throws OVER BUDGET) and again at the
# end; an overrun is a FAIL with the last events kept in the results.
function Get-SectionBudget([string]$name, [double]$budget) {
    if ($budget -gt 0) { return $budget }
    $ok = @(Get-FLBench | Where-Object { $_.suite -eq 'sweep' -and $_.case -eq $name -and $_.outcome -eq 'PASS' } |
            ForEach-Object { [double]$_.seconds } | Sort-Object)
    if ($ok.Count -lt 3) { return 0 }
    1.5 * $ok[[int][Math]::Floor(($ok.Count - 1) / 2)] + 60
}
function Invoke-Budgeted([scriptblock]$body, [double]$limit) {
    Set-FLDeadline $limit 'section budget'
    $t0 = Get-Date
    try { $r = & $body } finally { Set-FLDeadline 0 }
    $sec = ((Get-Date) - $t0).TotalSeconds
    if ($limit -gt 0 -and $sec -gt $limit) { throw "OVER BUDGET (section budget $([int]$limit) s): took $([int]$sec) s" }
    $r
}
function Section([string]$name, [scriptblock]$body, [double]$budget = 0) {
    if ($Only -and $name -notin $Only) { return }
    $limit = Get-SectionBudget $name $budget
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $script:game.reused = $false
    $script:bootSecs = 0
    $script:fpsSw = $null
    $first = $null
    try { $d = Invoke-Budgeted $body $limit; $ok = $true } catch { $ok = $false; $first = "$_" -replace '\s+', ' ' }
    if (-not $ok -and $first -match 'OVER BUDGET') {
        try { Add-Content $res (@(Get-FLEventLog $script:secStart) | Select-Object -Last 30 | ForEach-Object { "    $name event: $_" }) } catch {}
        try { $null = Send-FL 'ap stop' } catch {}
    }
    $leak = ''
    if (-not $ok -and $script:game.reused) {
        # failed in a reused game: once more in a fresh one before it counts
        Out-R ("RETRY-FRESH {0} (failed in a reused game after {1:N0} s): {2}" -f $name, $sw.Elapsed.TotalSeconds, $first.Substring(0, [Math]::Min(300, $first.Length)))
        $script:game.fresh = $true
        $sw.Restart()
        try { $d = Invoke-Budgeted $body $limit; $ok = $true; $leak = ' STATE-LEAK: failed reused, passed fresh' } catch { $first = "$_" -replace '\s+', ' ' }
    }
    $how = if ($script:game.reused) { 'reused' } else { 'fresh' }
    $fps = Get-SectionFps
    if ($ok) { Out-R ("PASS {0} ({1:N0} s, {2}{5}){3}: {4}" -f $name, $sw.Elapsed.TotalSeconds, $how, $leak, $d, $fps) }
    else {
        Out-R ("FAIL {0} ({1:N0} s, {2}{4}): {3}" -f $name, $sw.Elapsed.TotalSeconds, $how, $first.Substring(0, [Math]::Min(400, $first.Length)), $fps)
        $script:game.fresh = $true # never reuse a game a section failed in
    }
    try {
        $inst = if ($Instance -gt 0) { " i$Instance" } else { '' }
        Add-FLBench -Suite sweep -Case $name -Context "$how$inst" -Outcome ($(if ($ok) { 'PASS' } else { 'FAIL' })) `
            -Seconds $sw.Elapsed.TotalSeconds -Stats ("boot=$([int]$script:bootSecs)$fps $($leak.Trim())".Trim())
        if ($script:secStart -ge 0) { Add-FLBenchObjectives -Suite "mission $name" -Context $how -Lines @(Get-FLEventLog $script:secStart) }
    } catch {}
}
# Starts the section's game: a fresh one (deploy, no build) or, for a reusable section, the running one with the fixture
# loaded natively after the leftovers check. $FreshOnly: missions (story state, Content.dll), the release build.
function Boot([string]$save, [switch]$Release, [switch]$FreshOnly) {
    # per-phase time (P4, 20 Oct 2026): the boot (game start or the reused game's load) is added to the section's bench
    # row as boot=<s>, the rest is the subject's own time
    $bootSw = [Diagnostics.Stopwatch]::StartNew()
    try { BootInner $save -Release:$Release -FreshOnly:$FreshOnly } finally { $script:bootSecs += $bootSw.Elapsed.TotalSeconds }
    # the game's frame rate over the section (user, 20 Oct 2026: load context for every result - 3 parallel games ran
    # 154-163 fps each vs 191 alone, and load-sensitive tests failed under it): frames from here to the section's end
    try { $script:fpsF0 = Get-FLFrame; $script:fpsSw = [Diagnostics.Stopwatch]::StartNew() } catch { $script:fpsSw = $null }
}
function Get-SectionFps {   # the game's average frame rate since the boot, '' when unknown (no game, frame counter reset)
    try {
        if (-not $script:fpsSw -or $script:fpsSw.Elapsed.TotalSeconds -lt 5) { return '' }
        $f = Get-FLFrame
        if ($f -le $script:fpsF0) { return '' }
        " fps={0:N0}" -f (($f - $script:fpsF0) / $script:fpsSw.Elapsed.TotalSeconds)
    } catch { '' }
}
function BootInner([string]$save, [switch]$Release, [switch]$FreshOnly) {
    $g = $script:game
    $running = Get-FLGameProcess
    $tooOld = $g.started -and (((Get-Date) - $g.started).TotalMinutes -ge $ReuseMinutes -or $g.sections -ge $ReuseMax)
    # Freelancer lists the saves it finds at its start: a fixture copied in later could not be loaded by name
    $listed = -not (Test-Path -LiteralPath $save) -or (Test-Path -LiteralPath (Join-Path (Get-FLSaveDir) (Split-Path $save -Leaf)))
    $reuse = -not ($Fresh -or $g.fresh -or $Release -or $FreshOnly -or -not $running -or $tooOld -or -not $listed)
    if ($reuse) {
        try {
            $null = Send-FL 'ap stop'; $null = Send-FL 'bot off'; $null = Send-FL 'spawn clear'; Clear-FLPad
            $null = Load-FLSave (Split-Path $save -Leaf)
            $ap = (Get-FLAutopilot).Mode
            $left = [int][regex]::Match((Send-FL 'spawn list'), 'spawned (\d+)').Groups[1].Value
            if ($ap -ne 'off' -or $left -gt 0) { throw "leftovers after the load: autopilot $ap, spawns $left" }
            $g.sections++
            $g.reused = $true
            $null = Start-FLEventLog
            $script:secStart = $script:FLLogSeq
            return
        } catch { Out-R "  reuse refused ($_): a fresh game" }
    }
    $a = @{ Background = $true; Restart = $true; NoBuild = $true; Instance = $Instance }
    if ($save) { $a.Save = $save }
    if ($Release) { $a.Release = $true; if (-not $SkipBuild) { $a.Remove('NoBuild') } } # its own build (a parallel sweep built it first)
    $null = & $deploy @a 2>&1
    $g.fresh = $false; $g.started = Get-Date; $g.sections = 1; $g.reused = $false
    $null = Start-FLEventLog
    $script:secStart = $script:FLLogSeq
}
$script:secStart = -1
# One build for the whole sweep (each section used to build; a build during a sweep is never allowed anyway)
Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue   # this instance's game only
if (-not $SkipBuild) { try { $null = & $deploy -NoStart 2>&1 } catch { Out-R "BUILD FAILED: $_"; return } }
function Speed { $p0 = (Get-FLPilot).Pos; $sw = [Diagnostics.Stopwatch]::StartNew(); $null = Wait-FLFramesPassed 20
                 $p1 = (Get-FLPilot).Pos; (Get-FLDistance $p0 $p1) / $sw.Elapsed.TotalSeconds }

Section 'base-rooms' {
    Boot SaveC701.fl
    if ((Get-FLMode) -ne 'docked') { throw "not docked: $(Get-FLStoryState)" }
    $null = Goto-FLRoom Bar
    $null = Wait-FLRoomReady
    $null = Start-FLLaunch; $null = Wait-FLSpace
    "Bar reached, launched: mode $(Get-FLMode)"
}

# One travel leg judged by its quality (user, 20 Oct 2026): the autopilot's own plan is the yardstick - the leg's time
# against the last plan's estimate (plus when that plan was made: a re-plan after a disrupted lane resets the clock),
# every re-plan with its reason, and the lanes planned vs ridden. FAIL: over 1.5 x the estimate (+15 s), or a lane in
# the first plan given up without a disruption / closure reason.
function Measure-TravelLeg([int]$Since, [string]$Leg) {
    $ev = @(Get-FLEventLog $Since)
    $tick = { param($l) [double](("$l" -split ' ')[1]) }
    $start = $ev | Where-Object { "$_" -match 'autopilot start mode=travel' } | Select-Object -First 1
    $plans = @($ev | Where-Object { "$_" -match 'autopilot travel plan ' })
    $stop = $ev | Where-Object { "$_" -match 'autopilot stop mode=travel why=(\S+) .*? time=([\d.]+)' } | Select-Object -Last 1
    if (-not $start -or -not $plans.Count -or -not $stop) { throw "$Leg`: no complete travel in the events" }
    $null = "$stop" -match 'why=(\S+) .*? time=([\d.]+)'; $why = $Matches[1]; $took = [double]$Matches[2]
    $last = $plans[-1]; $null = "$last" -match 'total=([\d.]+)'; $est = [double]$Matches[1]
    $offset = ((& $tick $last) - (& $tick $start)) / 1000
    $budget = 1.5 * ($offset + $est) + 15
    $reasons = @($ev | Where-Object { "$_" -match 'autopilot travel replan why=(.+?) step=' } | ForEach-Object { $Matches[1] })
    $disrupted = @($ev | Where-Object { "$_" -match 'lane-disrupted|lane-closed' }).Count
    $firstLanes = ([regex]::Matches("$($plans[0])", '\blane enter=')).Count
    $rides = @($ev | Where-Object { "$_" -match 'autopilot travel ride ' }).Count
    $s = "$Leg $([int]$took) s (plan $([int]$est) s at +$([int]$offset) s, budget $([int]$budget) s, direct " +
         "$([regex]::Match("$($plans[0])", 'direct=([\d.]+)').Groups[1].Value) s), lanes planned $firstLanes ridden $rides, " +
         "re-plans $($reasons.Count)$(if ($reasons) { ': ' + ($reasons -join '; ') })"
    Add-FLBench -Suite navigation -Case $Leg -Context $(if ($script:game.reused) { 'reused' } else { 'fresh' }) `
        -Outcome $why -Seconds $took -Stats "plan=$([int]$est) offset=$([int]$offset) lanes=$firstLanes rides=$rides replans=$($reasons.Count)"
    if ($took -gt $budget) { throw "SLOW $s" }
    if ($firstLanes -gt 0 -and $rides -eq 0 -and -not $disrupted) { throw "LANE SKIPPED $s" }
    $s
}

Section 'navigation' {
    # The autopilot's travel mode, the way a player travels (user, 20 Oct 2026: was mostly harness helpers - the plain
    # game goto to the gate took 7.5 min past a trade lane and passed): every leg is the travel mode, judged by
    # Measure-TravelLeg. Legs: space near Manhattan -> fast dock Manhattan; launch -> the Li01->Li02 gate, through it.
    Boot SaveC703.fl
    $seq = Get-FLEventSeq
    $null = Start-FLTravel 2755957258 -TimeoutSec 300   # Manhattan Docking Ring (the planet itself is not docked with)
    if ((Get-FLMode) -ne 'docked') { throw "travel to Manhattan: not docked ($(Get-FLStoryState))" }
    $a = Measure-TravelLeg $seq 'to-Manhattan'
    $null = Start-FLLaunch; $null = Wait-FLSpace
    $sys0 = (Get-FLPilot).System
    $seq = Get-FLEventSeq
    $null = Start-FLTravel Li01_to_Li02 -TimeoutSec 400
    $null = Wait-FLUntil { $p = Get-FLPilot; $p -and $p.System -ne $sys0 -and -not $p.Jumping } 'arrival in Li02' 120
    $b = Measure-TravelLeg $seq 'to-Li02-gate'
    "$a | $b"
} -budget 600 # a game start + both legs as planned (~66 + ~230 s) + launch + jump, with margin; the legs are judged by Measure-TravelLeg

Section 'pad-input' {
    Boot SaveC702.fl
    $null = Send-FL 'ai dir idle'; $null = Wait-FLFramesPassed 60
    $v0 = Speed
    Set-FLPad -LY 1; $null = Wait-FLFramesPassed 90; Set-FLPad
    $v1 = Speed
    Clear-FLPad
    if ($v1 -lt 20) { throw "throttle up by pad: speed $([int]$v0) -> $([int]$v1)" }
    "pad throttle: speed $([int]$v0) -> $([int]$v1) m/s"
}

Section 'combat-missiles' {
    Boot SaveC705.fl
    $start = $script:FLLogSeq
    $null = Send-FL 'spawn fc_lr_pi_fighter_d3 fc_lr_grp 3 1500 level=5'
    $null = Wait-FLFramesPassed 30
    $null = Send-FL 'ap start combat'
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalSeconds -lt 120) {
        if (-not @(Get-FLContacts -Hostile | Where-Object { $_.Distance -lt 5000 }).Count) { break }
        if ((Get-FLAutopilot).Mode -in 'off', 'hold') { $null = Send-FL 'ap start combat' }
        $null = Wait-FLFramesPassed 60
    }
    $null = Send-FL 'ap stop'
    $ev = @(Get-FLEventLog $start)
    $kills = @($ev | Where-Object { "$_" -match 'target-destroyed' }).Count
    $stray = @($ev | Where-Object { "$_" -match 'missile-round-left .*frames-since-tap=(\d+)' -and [int]$Matches[1] -gt 5 }).Count
    $out = @($ev | Where-Object { "$_" -match 'missile-outcome (\w+) (\w+)' } | ForEach-Object { "$($Matches[1])/$($Matches[2])" })
    # the pirates spawn beside Manhattan: its police may take one (19 Oct 2026: 2 kills, the third gone without ours,
    # twice) - pass when all three are gone and at least two are ours
    $left = [int][regex]::Match((Send-FL 'spawn list'), 'spawned (\d+)').Groups[1].Value
    $null = Send-FL 'spawn clear'
    if ($left -gt 0 -or $kills -lt 2) { throw "kills $kills of 3, $left of ours still alive" }
    if ($stray) { throw "$stray missile round(s) without a tap" }
    "kills $kills, stray rounds 0, missiles: $($out -join ' ')"
}

Section 'base-repair' {
    # Repair at a base like the player (user, 20 Oct 2026; playthrough "DAMAGED ITEMS" window): damaged in a real fight
    # (hull below 60 %, parts too), the autopilot's travel docks at Manhattan, Invoke-FLBaseRepair repairs natively.
    # PASS: the game charged for it and the ship's value is back to the undamaged value.
    # Fresh only: in a reused game (after combat-missiles) the ship died 11 s in, twice (STATE-LEAK 20 Oct 2026; suspect:
    # the hull read from 'ap status' while the autopilot is off is stale from the last fight - open)
    Boot SaveC705.fl -FreshOnly
    $full = [double][regex]::Match((Send-FL 'worth'), 'assets=([\d.]+)').Groups[1].Value
    # level-3 pirates (was d6): the hull must fall slower than the harness polls - with 3 parallel games each poll took
    # longer and d6 fire killed the ship between the 60 % check and 'spawn clear' (20 Oct 2026, parallel sweep)
    # The game itself despawns them at 60 % hull ('spawn clearat', checked every frame): the harness poll lagged under
    # parallel load and the ship died even fresh (20 Oct 2026)
    # The autopilot's own repairs off until docked (21 Oct 2026: repairs in every mode spent batteries / nanobots on the
    # deliberate damage - the hull hardly fell and the used consumables kept the assets below the full value)
    # PASS judges the repair, not the pirates' aim (21 Oct 2026: the fight destroyed the shield generator - `self`
    # shield=-1.00 from then on - and no repair restores a destroyed item, so "assets back to the full value" failed
    # while the repair itself was complete): the game charged for it, its own window then has nothing left to repair,
    # the ship value did not fall through the repair, and with nothing destroyed it is back at the full value. The charge
    # is NOT the value restored (measured 21 Oct 2026: 207 credits charged, ship value +56 - the repair price and the
    # net-worth value of hull points are different formulas), so no 1:1 check. Destroyed equipment (gone from the
    # server's equipment list) is reported; the full value's shortfall is then its value.
    $null = Send-FL 'ap set repair 0'
    $mounted = { @([regex]::Matches((Send-FL 'equipment'), 'arch=(\d+) id=(\d+) count=\d+ mounted=1') | ForEach-Object { $_.Groups[2].Value }) }
    $items0 = & $mounted
    try {
        $null = Send-FL 'spawn clearat 0.6'
        $null = Send-FL 'spawn fc_lr_pi_fighter_d3 fc_lr_grp 3 600'
        $null = Wait-FLUntil { (Send-FL 'spawn list') -match '^spawned 0\b' } 'pirates despawned at 60 % hull' 120
        $null = Send-FL 'spawn clearat off'
        $hurt = [double][regex]::Match((Send-FL 'worth'), 'assets=([\d.]+)').Groups[1].Value
        $null = Start-FLTravel 2755957258 -TimeoutSec 300   # Manhattan Docking Ring
    } finally { $null = Send-FL 'ap set repair 1' }
    if ((Get-FLMode) -ne 'docked') { throw "not docked: $(Get-FLStoryState)" }
    $null = Wait-FLRoomReady
    $items1 = & $mounted
    $destroyed = @($items0 | Where-Object { $_ -notin $items1 }).Count
    $r = Invoke-FLBaseRepair
    $after = [double][regex]::Match((Send-FL 'worth'), 'assets=([\d.]+)').Groups[1].Value
    $paid = if ($r -match '^repaired (\d+)') { [double]$Matches[1] } else { -1 }
    $again = Invoke-FLBaseRepair
    $note = "assets $full full, $hurt damaged, $after after; destroyed items $destroyed (value $($full - $after))"
    if ($paid -le 0) { throw "repair: $r; $note" }
    if ($again -ne 'nothing to repair') { throw "repair left something: second visit '$again'; $note" }
    if ($after -lt $hurt) { throw "ship value fell through the repair ($hurt -> $after); $note" }
    if ($destroyed -eq 0 -and $after -lt $full) { throw "nothing destroyed, yet the ship value is not back at full; $note" }
    "$r credits; ship value $hurt -> $after (full $full); second visit: $again; destroyed items $destroyed"
} -budget 300

Section 'base-restock' {
    # Restock at the equipment dealer like the player (user, 20 Oct 2026): nanobots, batteries and the ammo of the
    # MOUNTED launchers. Campaign Save1148 docked at Li01_12 (or_elite: torpedo, 2 missile01_mark03, mine01, ge_s_cm_03
    # mounted, their ammo below 50; nanobots 26 of 29; and 2 missile01_mark02_ammo carried with no mark02 launcher).
    # Invoke-FLBaseRestock buys natively (dealer list, QuantitySlider, BuySellTradeButton; the game sends GFGoodBuy /
    # ReqAddItem / ReqChangeCash). PASS: nanobots + every mounted munition bought, the cash fell by what the assets rose,
    # the unmounted launcher's ammo untouched, and a second visit buys nothing (full).
    Boot (Join-Path $fl 'research\saves\bc46-all-missions\Save1148.fl')
    $null = Wait-FLRoomReady
    $carried = { param($nick) [int][regex]::Match((Send-FL 'equipment'), "arch=$([uint32](Get-FLHash $nick)) id=\d+ count=(\d+) mounted=0").Groups[1].Value }
    $mk2 = & $carried 'missile01_mark02_ammo'
    $w0 = Send-FL 'worth'
    $r = Invoke-FLBaseRestock
    $w1 = Send-FL 'worth'
    $v = { param($w, $k) [double][regex]::Match($w, "$k=(-?[\d.]+)").Groups[1].Value }
    $spent = (& $v $w0 'cash') - (& $v $w1 'cash'); $gained = (& $v $w1 'assets') - (& $v $w0 'assets')
    $need = 'ge_s_repair_01', 'missile01_mark03_ammo', 'torpedo01_mark01_ammo', 'mine01_mark01_ammo', 'ge_s_cm_03_ammo'
    $missing = @($need | Where-Object { $r -notmatch "$_ [1-9]" })
    if ($missing -or $spent -le 0 -or [math]::Abs($spent - $gained) -gt 1 -or (& $carried 'missile01_mark02_ammo') -ne $mk2) {
        throw "restock: $r; spent $spent, assets +$gained; not bought: $($missing -join ', '); unmounted mark02 ammo $mk2 -> $(& $carried 'missile01_mark02_ammo')"
    }
    $null = Wait-FLRoomReady
    $r2 = Invoke-FLBaseRestock
    if ($r2 -match ' [1-9]\d* \(') { throw "second visit bought again: $r2" }
    "$r; assets +$gained; unmounted ammo kept ($mk2); second visit: $r2"
} -budget 240

Section 'hold-park' {
    Boot SaveC702.fl
    $start = $script:FLLogSeq
    Set-FLPad -LY 1; $null = Wait-FLFramesPassed 60; Set-FLPad; Clear-FLPad
    $null = Send-FL 'ap start hold'
    $null = Wait-FLFramesPassed 300
    $p0 = (Get-FLPilot).Pos; $null = Wait-FLFramesPassed 300; $p1 = (Get-FLPilot).Pos
    $drift = Get-FLDistance $p0 $p1
    $mode = (Get-FLAutopilot).Mode
    $null = Send-FL 'ap stop'
    $leash = @(Get-FLEventLog $start | Where-Object { "$_" -match 'hold leash' }).Count
    if ($mode -ne 'hold' -or $drift -gt 20 -or $leash -gt 1) { throw "mode $mode drift $([int]$drift) m leash runs $leash" }
    "hold keeps station: drift $([int]$drift) m over ~5 s, leash runs $leash"
}

Section 'subtitles-dialogue' {
    Boot Save05c7.fl -FreshOnly
    $s0 = Get-FLSLast
    $null = Start-FLLaunch
    $st = @{ busy = $false }
    $null = Wait-FLUntil { if (Get-FLDialogueBusy 0) { $st.busy = $true }; $st.busy } 'a mission line playing' 90
    $s1 = Get-FLSLast
    if ($s1 -le $s0) { throw "no subtitle shown ($s0 -> $s1)" }
    "mission line flagged by the radio state; subtitles shown $s0 -> $s1"
}

Section 'mission-M01a' {
    Boot Save2b9b.fl -FreshOnly
    $o = & (Join-Path $fl 'controller\tools\missions\M01a.ps1') *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M01a $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M01a: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
}

Section 'mission-M01b' {
    Boot (Join-Path $fl 'research\saves\bc46-all-missions\Save5f61.fl') -FreshOnly
    $o = & (Join-Path $fl 'controller\tools\missions\M01b.ps1') *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M01b $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M01b: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
}

Section 'mission-M02-scenes' {
    $s = Start-FLScene M02 8
    if (-not $s.Valid) { throw "scene 8 stale: $($s.Why)" }
    $o = & (Join-Path $fl 'controller\tools\missions\M02.ps1') -From 8 *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M02 $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M02 from 8: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
}

Section 'objective-M03' {
    # the autopilot's objective mode flies every space part of M03; the harness only does the rooms
    Boot (Join-Path $fl 'research\saves\bc46-all-missions\Save1e7d.fl') -FreshOnly
    $o = & (Join-Path $fl 'controller\tools\missions\M03objective.ps1') *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M03o $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M03 objective mode: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
} -budget 1400

Section 'objective-M04' {
    # M04 "Escape from Liberty" by the objective mode (space); the harness only does the rooms
    Boot (Join-Path $fl 'research\saves\bc46-all-missions\Save6380.fl') -FreshOnly
    $o = & (Join-Path $fl 'controller\tools\missions\M04objective.ps1') *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M04o $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M04 objective mode: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
} -budget 1800

Section 'objective-M05' {
    # M05 (Cambridge -> Omega-3 -> Leeds) by the objective mode (space); the harness only does the rooms
    # (Complete-FLBaseStory at Sprague and Baxter, the Tobias scene at Leeds)
    Boot (Join-Path $fl 'research\saves\bc46-all-missions\Save1d87.fl') -FreshOnly
    $o = & (Join-Path $fl 'controller\tools\missions\M05objective.ps1') *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M05o $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M05 objective mode: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
} -budget 1900

Section 'fight-matrix' {
    # The free-roam retreat must not abandon fights the autopilot wins (user, 19 Oct 2026): controller\tools\
    # flfights.ps1 against 15 enemy kinds, on the starter ship (SaveC705) and a strong one (SaveC706, the M09 bh_elite;
    # built from the campaign save by tools\mkfixture.js when missing), the normal setting (caution 1). Truth from runs
    # never leaving (caution 0): every kind winnable for both ships except 4 heavy fighters vs the starter ship.
    # FAIL: a retreat from a winnable kind, a hopeless kind not left, a spawn that failed. Losses without a retreat are
    # reported (the combat, not the retreat logic). ~40-60 min.
    $c706 = Join-Path (Get-FLSaveDir) 'SaveC706.fl'
    if (-not (Test-Path $c706)) {
        $null = & node (Join-Path $fl 'tools\mkfixture.js') (Join-Path (Get-FLSaveDir) 'SaveC705.fl') `
            (Join-Path $fl 'research\saves\bc46-all-missions\Save4686.fl') $c706 'CTL free roam - space Manhattan, M09 bh_elite loadout'
        if (-not (Test-Path $c706)) { throw 'SaveC706 could not be built (tools\mkfixture.js)' }
    }
    # Borderline: kinds whose prior odds sit at the retreat threshold, so retreating and staying are both right and the
    # outcome varies run to run (c705 heavy_d12_x2, prior 0.05 = retreat_odds: 3 runs on 21 Oct 2026 died / won /
    # retreated). Reported, never failed.
    $ships = @(@{ save = 'SaveC705'; tag = 'c705'; hopeless = @('heavy_d12_x4', 'heavy_d12_x4_far'); borderline = @('heavy_d12_x2') },
        @{ save = 'SaveC706'; tag = 'c706'; hopeless = @(); borderline = @() })
    $rows = @()
    foreach ($s in $ships) {
        Boot "$($s.save).fl" -FreshOnly # a fresh game per ship (long spawn sessions once exhausted the spawner)
        $rows += @(& (Join-Path $fl 'controller\tools\flfights.ps1') -Save $s.save -Tag $s.tag) |
            ForEach-Object {
                $_ | Add-Member -NotePropertyName Hopeless -NotePropertyValue ($_.Scenario -in $s.hopeless)
                $_ | Add-Member -NotePropertyName Borderline -NotePropertyValue ($_.Scenario -in $s.borderline) -PassThru
            }
    }
    Add-Content $res ($rows | ForEach-Object { "    fight $($_.Ship) $($_.Scenario): $($_.Outcome) retreat=$($_.Retreated) kills=$($_.Kills) $($_.Seconds)s odds=$($_.OddsPrior)/$($_.OddsMeasured)$(if ($_.Borderline) { ' (borderline)' })" })
    $wrong = @($rows | Where-Object { -not $_.Hopeless -and -not $_.Borderline -and $_.Retreated })
    $stayed = @($rows | Where-Object { $_.Hopeless -and -not $_.Retreated })
    $failed = @($rows | Where-Object { $_.Outcome -eq 'spawn-failed' })
    $lost = @($rows | Where-Object { -not $_.Hopeless -and -not $_.Borderline -and -not $_.Retreated -and $_.Outcome -ne 'won' })
    $border = @($rows | Where-Object Borderline)
    $names = { param($l) ($l | ForEach-Object { "$($_.Ship)/$($_.Scenario)" }) -join ', ' }
    if ($wrong -or $stayed -or $failed) {
        throw "wrong retreats: $(& $names $wrong); hopeless not left: $(& $names $stayed); spawn failed: $(& $names $failed)"
    }
    "$($rows.Count) fights: won $(@($rows | Where-Object Outcome -eq 'won').Count), wrong retreats 0, hopeless left " +
        "$(@($rows | Where-Object { $_.Hopeless -and $_.Retreated }).Count); lost without a retreat: $(if ($lost) { & $names $lost } else { 'none' }); " +
        "borderline: $(if ($border) { ($border | ForEach-Object { "$($_.Ship)/$($_.Scenario) $($_.Outcome) retreat=$($_.Retreated)" }) -join ', ' } else { 'none' })"
}

Section 'release-build' {
    Boot SaveC702.fl -Release
    $p = Get-FLPilot
    $sub = Send-FLS 'state'
    if (-not $p) { throw 'release: controller not answering (pilot)' }
    if ($sub -notmatch 'lines \d+') { throw "release: subtitles not answering: $sub" }
    "release build: controller + subtitles answer in space ($(($sub -split "`n")[0]))"
}

Section 'display-matrix' {
    # The stated must "any resolution, aspect ratio and DPI; borderless windowed the main mode" measured instead of
    # assumed (audit, 21 Oct 2026: verified only at 3840x2160 borderless 200 % until then). One game instance (this
    # worker's, or instance 9 in a serial sweep: the main install keeps the user's options), one fresh start per
    # configuration (tools\fldata.ps1 Set-FLInstanceDisplay: PerfOptions size + the exe's window-style bytes, as HDE's
    # installer sets them). DPI: HDE's exe is DPI-aware (its manifest), so at any Windows scaling it renders at the
    # physical size - the same as 100 % DPI; the other case, a DPI-unaware process (an exe without the manifest), is run
    # through the per-process compatibility layer (__COMPAT_LAYER=DPIUNAWARE in the game's environment only): Windows
    # then shows it a 1920x1080 logical desktop at 200 % and FL# clamps the resolution to it. Fullscreen (exclusive)
    # takes the screen: only with -Attended. Per configuration: both modules answer, the back buffer is the requested
    # size, the window has / lacks its border as the mode says, the DPI awareness is what was asked, the HUD controls
    # project inside the screen, the pad throttles the ship, a subtitle shows, and a screenshot (evidence, one per mode)
    # has the client size. Not covered: the movie overlay (startup movies are skipped), the real pad, mouse hand-back.
    if (-not ('FLDispWin' -as [type])) {
        Add-Type -TypeDefinition @'
using System; using System.Runtime.InteropServices;
public static class FLDispWin {
  [DllImport("user32.dll")] public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetAwarenessFromDpiAwarenessContext(IntPtr c);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  public struct RECT { public int L, T, R, B; }
}
'@
    }
    # this process reads window rects in physical pixels (an unaware reader gets them halved at 200 %: the first run's
    # "client 1920x1080" for a 3840x2160 back buffer, 21 Oct 2026)
    [void][FLDispWin]::SetProcessDpiAwarenessContext([IntPtr](-4))
    $inst = if ($Instance -gt 0) { $Instance } else { 9 }
    $savedInstance = $env:FLVIBED_INSTANCE
    $env:FLVIBED_INSTANCE = "$inst"
    $null = New-FLInstance $inst
    $homeSize = Get-FLDisplaySize
    if (-not $homeSize) { throw 'the main install has no [Display] size to restore' }
    $shotDir = Join-Path (Split-Path $res) ("display_{0:yyyy-MM-dd_HHmm}" -f (Get-Date))
    $configs = @(
        @{ name = 'borderless-4k';     mode = 'borderless'; size = '3840, 2160'; aspect = '16:9' },
        @{ name = 'windowed-4x3';      mode = 'windowed';   size = '1600, 1200'; aspect = '4:3' },
        @{ name = 'windowed-21x9';     mode = 'windowed';   size = '2560, 1080'; aspect = '21:9' },
        @{ name = 'borderless-720p';   mode = 'borderless'; size = '1280, 720';  aspect = '16:9' },
        @{ name = 'dpi-unaware';       mode = 'borderless'; size = '3840, 2160'; aspect = '16:9'; dpiUnaware = $true }
    )
    if ($Attended) { $configs += @{ name = 'fullscreen-4k'; mode = 'fullscreen'; size = '3840, 2160'; aspect = '16:9' } }
    $rows = [Collections.Generic.List[string]]::new()
    $bad = [Collections.Generic.List[string]]::new()
    try {
        foreach ($c in $configs) {
            Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
            $null = Set-FLInstanceDisplay -N $inst -Mode $c.mode -Size $c.size
            if ($c.dpiUnaware) { $env:__COMPAT_LAYER = 'DPIUNAWARE' }
            try { $null = & $deploy -Instance $inst -Background -Restart -NoBuild -Save SaveC702.fl 2>&1 }
            finally { $env:__COMPAT_LAYER = $null }
            $want = [regex]::Match($c.size, '(\d+)\s*,\s*(\d+)')
            $wantW = [int]$want.Groups[1].Value; $wantH = [int]$want.Groups[2].Value
            $st = Send-FLS 'state'
            $scr = [regex]::Match($st, '(?m)^screen (\d+)x(\d+)')
            $w = [int]$scr.Groups[1].Value; $h = [int]$scr.Groups[2].Value
            $p = Get-FLGameProcess
            $hwnd = $p.MainWindowHandle
            $cr = New-Object FLDispWin+RECT; $wr = New-Object FLDispWin+RECT
            [void][FLDispWin]::GetClientRect($hwnd, [ref]$cr); [void][FLDispWin]::GetWindowRect($hwnd, [ref]$wr)
            $cw = $cr.R - $cr.L; $ch = $cr.B - $cr.T; $ww = $wr.R - $wr.L; $wh = $wr.B - $wr.T
            $aware = [FLDispWin]::GetAwarenessFromDpiAwarenessContext([FLDispWin]::GetWindowDpiAwarenessContext($hwnd))
            # HUD controls projected through the UI camera land inside the screen
            $items = @([regex]::Matches((Send-FL 'uiitems'), 'center=\((-?\d+),(-?\d+)\)') | ForEach-Object { [int]$_.Groups[1].Value, [int]$_.Groups[2].Value })
            $n = [int]($items.Count / 2); $outside = 0
            for ($i = 0; $i -lt $items.Count; $i += 2) { if ($items[$i] -lt 0 -or $items[$i] -gt $w -or $items[$i + 1] -lt 0 -or $items[$i + 1] -gt $h) { $outside++ } }
            # the pad flies, a subtitle shows
            $null = Send-FL 'ai dir idle'; $null = Wait-FLFramesPassed 30
            $v0 = Speed; Set-FLPad -LY 1; $null = Wait-FLFramesPassed 90; Set-FLPad; $v1 = Speed
            # HUD 2 (docs\UI_HUD2.md): with the pad the device and hud_style 1 / 2 it replaces the game's panels
            $hs = (Send-FL 'hud2 state') -split "`n"
            $hudStyle = if (($hs -join ' ') -match 'settings style=(\d)') { [int]$Matches[1] } else { -1 }
            $hud2On = ($hs[0] -match 'hud2 active=1')
            Clear-FLPad
            $s0 = Get-FLSLast
            $null = Send-FLS "test 1 4 Display|$($c.name) $($w)x$($h)"
            $shown = try { Wait-FLSShown 'Display' -After $s0 -TimeoutSec 8 } catch { $null }
            if ($shown -eq 'timeout') { $shown = $null }   # Wait-FLSShown returns the word, not $null
            $shot = Save-FLShot "display_$($c.name)" -Dir $shotDir
            $img = [Drawing.Image]::FromFile($shot); $sw = $img.Width; $sh = $img.Height; $img.Dispose()
            $row = "$($c.name) ($($c.aspect)): screen ${w}x${h}, client ${cw}x${ch}, window ${ww}x${wh}, dpi-aware $aware, hud items $n outside $outside, hud2 $(if ($hud2On) { 'on' } else { 'off' }), pad $([int]$v0)->$([int]$v1) m/s, subtitle $(if ($shown) { 'shown' } else { 'NOT shown' }), shot ${sw}x${sh}"
            $rows.Add($row)
            if ($c.dpiUnaware) {
                # an aware caller reads the unaware window's rects scaled to physical pixels: geometry is reported, not judged
                if ($aware -ne 0) { $bad.Add("$($c.name): still DPI-aware ($aware)") }
                if ($w -le 0) { $bad.Add("$($c.name): no back buffer") }
            } else {
                if ($w -ne $wantW -or $h -ne $wantH) { $bad.Add("$($c.name): back buffer ${w}x${h}, wanted ${wantW}x${wantH}") }
                if ($aware -eq 0) { $bad.Add("$($c.name): the game runs DPI-unaware") }
                if ($cw -ne $w -or $ch -ne $h) { $bad.Add("$($c.name): client ${cw}x${ch} is not the back buffer ${w}x${h}") }
                if ($c.mode -eq 'windowed' -and ($ww -le $cw -or $wh -le $ch)) { $bad.Add("$($c.name): no window border (windowed mode not applied)") }
                if ($c.mode -ne 'windowed' -and ($ww -ne $cw -or $wh -ne $ch)) { $bad.Add("$($c.name): a border in $($c.mode) mode") }
                if ($sw -ne $cw -or $sh -ne $ch) { $bad.Add("$($c.name): screenshot ${sw}x${sh} is not the client ${cw}x${ch}") }
            }
            if ($n -lt 5) { $bad.Add("$($c.name): only $n HUD items") } elseif ($outside -gt $n / 5) { $bad.Add("$($c.name): $outside of $n HUD items outside the screen") }
            if ($v1 -lt 20) { $bad.Add("$($c.name): pad throttle $([int]$v0) -> $([int]$v1)") }
            if ($hudStyle -ge 1 -and -not $hud2On) { $bad.Add("$($c.name): HUD 2 did not come on with the pad (hud_style $hudStyle)") }
            if (-not $shown) { $bad.Add("$($c.name): subtitle not shown") }
        }
    } finally {
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
        $null = Set-FLInstanceDisplay -N $inst -Mode borderless -Size $homeSize
        $env:FLVIBED_INSTANCE = $savedInstance
    }
    Add-Content $res ($rows | ForEach-Object { "    display $_" })
    if ($bad.Count) { throw ($bad -join '; ') }
    "$($configs.Count) configurations OK (shots $shotDir): " + (($rows | ForEach-Object { ($_ -split ' \(')[0] }) -join ', ')
} -budget 600

Section 'display-subtitles' {
    # Real dialogue in the two extreme aspect ratios (user, 21 Oct 2026: "on-screen subtitle analysis - in space and on
    # a planet; are cutscenes working as intended, is spacing reasonable"): from Save2b9b (M01, Manhattan bar) Juni's
    # room scene (letterboxed), the offer accepted, the launch with its animations, the M01a opening cutscene in space
    # (cutscene camera, letterboxed), then King's flight comms. Every shown line is judged by the module's own layout
    # report (debug `layout`: the frame's geometry, no pixel reading) against the design rules (subtitles\docs\
    # DISPLAY_PLAN.md): font 3.6 % H; rows at most 0.8 W wide; the flight stack hangs from 0.162 H (0.235 H under the
    # objective banner) and ends above 0.44 H; letterboxed text ends 0.025 H above the bottom; nothing off screen. The
    # game's letterbox bars are 0.140 H (measured 21 Oct 2026, every aspect ratio): how much of each block sits above the
    # bar, over the picture, is reported (a name row + two text rows is 0.147 H, a second speaker stacks above the first:
    # the design question is open, docs\kb\display.md). One screenshot per context and mode as evidence.
    $inst = if ($Instance -gt 0) { $Instance } else { 9 }
    $savedInstance = $env:FLVIBED_INSTANCE
    $env:FLVIBED_INSTANCE = "$inst"
    $null = New-FLInstance $inst
    $homeSize = Get-FLDisplaySize
    $shotDir = Join-Path (Split-Path $res) ("display_subs_{0:yyyy-MM-dd_HHmm}" -f (Get-Date))
    $null = New-Item -ItemType Directory -Force $shotDir
    # the main mode (4K borderless) and the smallest (720p) too since the top panel (21 Oct 2026): its room depends on the
    # aspect ratio (the moved maneuver bar and the NeuroNet box share the top edge with it)
    $modes = @(@{ name = 'b16x9-3840x2160'; mode = 'borderless'; size = '3840, 2160' }, @{ name = 'w4x3-1600x1200'; mode = 'windowed'; size = '1600, 1200' },
        @{ name = 'w21x9-2560x1080'; mode = 'windowed'; size = '2560, 1080' }, @{ name = 'b16x9-1280x720'; mode = 'borderless'; size = '1280, 720' })
    $bad = [Collections.Generic.List[string]]::new()
    $summary = [Collections.Generic.List[string]]::new()
    $script:nameColours = @{} # speaker -> the name colour first seen (the same in every context and mode)
    # samples: context -> list of @{ top; bottom; w; px; sw; sh; anchor; fromTop; lb }
    function Read-Layout {
        $t = Send-FLS 'layout'
        $g = [regex]::Match($t, 'screen (\d+)x(\d+) px (\d+) .*? maxW (\d+) context (\w+) letterbox (\d) anchor ([\d.]+) (top|bottom)')
        if (-not $g.Success) { return $null }
        $sw = [int]$g.Groups[1].Value; $sh = [int]$g.Groups[2].Value
        $blocks = @([regex]::Matches($t, 'block y=([\d.-]+) h=([\d.]+) w=([\d.]+)( corner)?(?: name=(\w{6})(?: hostile)? speaker=([^\r\n]*))?') | Where-Object { -not $_.Groups[4].Success } |
            ForEach-Object { @{ top = [double]$_.Groups[1].Value / $sh; bottom = ([double]$_.Groups[1].Value + [double]$_.Groups[2].Value) / $sh; w = [double]$_.Groups[3].Value / $sw
                                name = $_.Groups[5].Value; speaker = $_.Groups[6].Value } })
        # the top panel (space_position 2) and the objective banner, when present
        $p = [regex]::Match($t, 'panel x=([\d.-]+) w=([\d.-]+) h=([\d.-]+) yoff=([\d.e-]+) s=([\d.]+) bar-moved=(\d) bar-right=([\d.-]+) nn-left=([\d.-]+)(?: top=([\d.-]+))?')
        $q = [regex]::Match($t, 'tab=([\d.-]+) body=([\d.-]+) cruise=([\d.-]+),([\d.-]+)')
        $panel = if ($p.Success) {
            @{ x = [double]$p.Groups[1].Value; w = [double]$p.Groups[2].Value; h = [double]$p.Groups[3].Value; yoff = [double]$p.Groups[4].Value
               s = [double]$p.Groups[5].Value; moved = $p.Groups[6].Value -eq '1'; barRight = [double]$p.Groups[7].Value; nnLeft = [double]$p.Groups[8].Value
               top = if ($p.Groups[9].Success) { [double]$p.Groups[9].Value } else { 0 }
               tab = if ($q.Success) { [double]$q.Groups[1].Value } else { 0 }; body = if ($q.Success) { [double]$q.Groups[2].Value } else { 0 }
               cruiseRight = if ($q.Success) { [double]$q.Groups[3].Value } else { 0 }; cruiseBottom = if ($q.Success) { [double]$q.Groups[4].Value } else { 0 } }
        }
        $bn = [regex]::Match($t, 'banner x=([\d.-]+) y=([\d.-]+)')
        $banner = if ($bn.Success) { @{ y = [double]$bn.Groups[2].Value } }
        @{ sw = $sw; sh = $sh; px = [int]$g.Groups[3].Value; context = $g.Groups[5].Value; lb = [int]$g.Groups[6].Value; anchor = [double]$g.Groups[7].Value / $sh; fromTop = $g.Groups[8].Value -eq 'top'; blocks = $blocks; panel = $panel; banner = $banner }
    }
    function Judge([string]$tag, $lay) {
        $want = [Math]::Round($lay.sh * 0.036)
        if ([Math]::Abs($lay.px - $want) -gt 1) { $bad.Add("${tag}: font $($lay.px) px, expected $want (3.6 % of $($lay.sh))") }
        foreach ($b in $lay.blocks) {
            if ($b.top -lt -0.001 -or $b.bottom -gt 1.001) { $bad.Add("${tag}: a block off screen ($($b.top.ToString('0.000'))..$($b.bottom.ToString('0.000')) H)") }
            if ($b.w -gt 0.801) { $bad.Add("${tag}: a row wider than 0.8 W ($($b.w.ToString('0.000')))") }
            # name colours (user, 21 Oct 2026): Trent always the player's amber, every character one colour throughout
            if ($b.speaker) {
                if ($b.speaker -eq 'Trent' -and $b.name -ne 'FFD27A') { $bad.Add("${tag}: Trent's name in $($b.name), not amber FFD27A") }
                if (-not $script:nameColours.ContainsKey($b.speaker)) { $script:nameColours[$b.speaker] = $b.name }
                elseif ($script:nameColours[$b.speaker] -ne $b.name) { $bad.Add("${tag}: $($b.speaker) in $($b.name), earlier $($script:nameColours[$b.speaker])") }
            }
        }
        if ($lay.context -eq 'flight' -and $lay.panel) {
            # the top panel (space_position 2, the default): from the top edge, inside the room between the moved
            # maneuver bar and the NeuroNet box, above the crosshair zone, the objective banner below it
            $pn = $lay.panel; $sh = $lay.sh
            if (-not $pn.moved) { $bad.Add("${tag}: top panel shown but the maneuver bar was not moved") }
            # flush with the top edge (rows from 0.016 H), or below the top HUD row on narrow screens (space_narrow 3:
            # its top at 0.095 H, rows 0.016 H below that)
            $want = 0.016 + $pn.top / $sh
            if ([Math]::Abs($lay.anchor - $want) -gt 0.003) { $bad.Add("${tag}: top panel rows start at $($lay.anchor.ToString('0.000')) H ($($want.ToString('0.000')) expected)") }
            if ($pn.top -gt 0 -and $pn.top / $sh -lt 0.083) { $bad.Add("${tag}: the panel below the top row starts at $(($pn.top / $sh).ToString('0.000')) H, inside the bar's row (0.083)") }
            if ($pn.s -ge 1 -and $pn.tab -gt 0) {
                # the name-tab layout (narrow screens, space_narrow 5): the tab between the bar and the NeuroNet box, the
                # body below the bar's row, clear of the cruise box under the bar's left end, on screen
                $tl = $pn.x - $pn.tab / 2; $tr = $pn.x + $pn.tab / 2; $bl = $pn.x - $pn.w / 2; $br = $pn.x + $pn.w / 2
                if ($tl -lt $pn.barRight + 1 -or $tr -gt $pn.nnLeft - 1) { $bad.Add("${tag}: the name tab ($([int]$tl)..$([int]$tr) px) leaves the room between bar ($([int]$pn.barRight)) and NeuroNet box ($([int]$pn.nnLeft))") }
                if ($pn.body / $sh -lt 0.083) { $bad.Add("${tag}: the body starts at $(($pn.body / $sh).ToString('0.000')) H, inside the bar's row (0.083)") }
                if ($pn.cruiseBottom -gt $pn.body -and $bl -lt $pn.cruiseRight - 1) { $bad.Add("${tag}: the body ($([int]$bl) px) overlaps the cruise box (to $([int]$pn.cruiseRight))") }
                if ($bl -lt 0 -or $br -gt $lay.sw) { $bad.Add("${tag}: the body ($([int]$bl)..$([int]$br) px) leaves the screen") }
                $script:tabSeen++
            } elseif ($pn.s -ge 1 -and $pn.top -le 0) {
                $left = $pn.x - $pn.w / 2; $right = $pn.x + $pn.w / 2
                if ($left -lt $pn.barRight + 1) { $bad.Add("${tag}: top panel ($([int]$left) px) overlaps the maneuver bar (ends $([int]$pn.barRight))") }
                if ($right -gt $pn.nnLeft - 1) { $bad.Add("${tag}: top panel ($([int]$right) px) overlaps the NeuroNet box (starts $([int]$pn.nnLeft))") }
            }
            if ($pn.s -ge 1) {
                if ($pn.h / $sh -gt 0.441) { $bad.Add("${tag}: top panel reaches $(($pn.h / $sh).ToString('0.000')) H (crosshair zone)") }
                # the banner's text starts 0.0125 H below its control's anchor (the reported y)
                $btop = if ($lay.banner) { $lay.banner.y + 0.0125 * $sh }
                if ($lay.banner -and $btop -lt $pn.h - 1) { $bad.Add("${tag}: the objective banner's text ($([int]$btop) px) is under the top panel (ends $([int]$pn.h))") }
            }
            $script:panelSeen++
        } elseif ($lay.context -eq 'flight') {
            if ([Math]::Abs($lay.anchor - 0.162) -gt 0.005 -and [Math]::Abs($lay.anchor - 0.235) -gt 0.005) { $bad.Add("${tag}: flight anchor $($lay.anchor.ToString('0.000')) H (0.162 / 0.235 expected)") }
            foreach ($b in $lay.blocks) { if ($b.bottom -gt 0.441) { $bad.Add("${tag}: the flight stack reaches $($b.bottom.ToString('0.000')) H (crosshair zone)") } }
        } elseif ($lay.lb -or $lay.context -eq 'cutscene') {
            $low = ($lay.blocks | ForEach-Object { $_.bottom } | Measure-Object -Maximum).Maximum
            if ($lay.blocks.Count -and [Math]::Abs($low - 0.975) -gt 0.003) { $bad.Add("${tag}: letterboxed text ends at $($low.ToString('0.000')) H (0.975 expected)") }
        }
    }
    function Capture-Lines([string]$tag, [int]$seconds, [int]$max, [string]$shotName) {
        $last = Get-FLSLast; $n = 0; $t0 = Get-Date; $over = 0; $stacked = 0; $shot = $false; $ctx = @{}
        while (((Get-Date) - $t0).TotalSeconds -lt $seconds -and $n -lt $max) {
            $null = Close-FLBlocker
            $hit = Wait-FLSShown '.' -After $last -TimeoutSec 3
            if ($hit -eq 'timeout') { continue }
            $last = Get-FLSLast
            Start-Sleep -Milliseconds 400
            $lay = Read-Layout
            if (-not $lay -or -not $lay.blocks.Count) { continue }
            $n++
            $ctx[$lay.context] = 1
            Judge "$tag #$n" $lay
            if ($lay.lb -or $lay.context -eq 'cutscene') {
                if (($lay.blocks | Where-Object { $_.top -lt 0.86 }).Count) { $over++ }
                if ($lay.blocks.Count -gt 1) { $stacked++ }
            }
            if (-not $shot) { $null = Save-FLShot $shotName -Dir $shotDir; $shot = $true }
        }
        "$tag $n lines ($(($ctx.Keys | Sort-Object) -join '/'))" + $(if ($lay -and ($lay.lb -or $lay.context -eq 'cutscene')) { ", over the picture $over, stacked $stacked" })
    }
    try {
        foreach ($m in $modes) {
            Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
            $null = Set-FLInstanceDisplay -N $inst -Mode $m.mode -Size $m.size
            $null = & $deploy -Instance $inst -Background -Restart -NoBuild -Save Save2b9b.fl 2>&1
            $null = Start-FLEventLog; $start = $script:FLLogSeq
            $null = Wait-FLRoomReady
            $null = Wait-FLUntil { $null = Close-FLBlocker; (Send-FL 'talkers') -match 'talk_story' } 'Juni in the Bar' 60
            $null = Send-FL 'talk story'
            $summary.Add((Capture-Lines "$($m.name) room" 60 8 "subs_$($m.name)_room"))
            $null = Wait-FLObjective '21820' $start 120
            $null = Goto-FLRoom Equipment
            $null = Wait-FLUntil { $null = Close-FLBlocker; if ((Send-FL 'visible OfferDialogAcceptButton') -match '^visible') { return $true }; $null = Skip-FLAnimation; $false } 'the offer dialog' 180
            $null = Send-FL 'click OfferDialogAcceptButton'
            $null = Wait-FLUntil { $null = Close-FLBlocker; (Send-FL 'hotspots') -match 'IDS_HOTSPOT_LAUNCH|Cityscape.*ready' } 'accepted' 120
            Wait-FLRoomReady -Watch
            if ((Send-FL 'hotspots') -notmatch 'IDS_HOTSPOT_LAUNCH') { $null = Goto-FLRoom Cityscape -Watch }
            $r = Send-FL 'goto launch'; if ($r -notmatch '^clicked') { throw "launch: $r" }
            $summary.Add((Capture-Lines "$($m.name) cutscene" 150 10 "subs_$($m.name)_cutscene"))
            if (-not (Wait-FLSpace 60)) { throw "not in flight after the cutscene: $(Get-FLStoryState)" }
            $script:panelSeen = 0; $script:tabSeen = 0
            $summary.Add((Capture-Lines "$($m.name) flight" 90 4 "subs_$($m.name)_flight"))
            # space_position 2 is the default: in flight the lines go to the top panel; narrow screens (4:3: aspect
            # < ~1.42) use the name-tab layout (space_narrow 5, the default), wide ones the plain panel
            if ($script:panelSeen -eq 0) { $bad.Add("$($m.name): no flight line in the top panel (space_position 2)") }
            $wh = [double[]]($m.size -split ',\s*'); $narrow = $wh[0] / $wh[1] -lt 1.42
            if ($narrow -and $script:tabSeen -eq 0) { $bad.Add("$($m.name): narrow screen without the name-tab layout (space_narrow 5)") }
            if (-not $narrow -and $script:tabSeen -gt 0) { $bad.Add("$($m.name): the name-tab layout on a wide screen") }
        }
    } finally {
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
        $null = Set-FLInstanceDisplay -N $inst -Mode borderless -Size $homeSize
        $env:FLVIBED_INSTANCE = $savedInstance
    }
    Add-Content $res ($summary | ForEach-Object { "    subtitles $_" })
    $seen = ($summary -join ' ')
    foreach ($need in 'room', 'cutscene', 'flight') { if ($seen -notmatch "$need \d+ lines" -or $seen -match "$need 0 lines") { $bad.Add("no $need lines captured") } }
    if ($bad.Count) { throw ($bad -join '; ') }
    $colours = ($script:nameColours.GetEnumerator() | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Value)" }) -join ' '
    "rules hold in 16:9 4K / 720p, 4:3 and 21:9 (room scene, space cutscene, flight: the top panel); " + ($summary -join ' | ') + "; names $colours (shots $shotDir)"
} -budget 1500

# Wait for the exit: a game still dying counted as running for deploy, which then hot-reloaded into it and threw
# "FLSubtitles shared memory not found" at the end of every sweep (exit code 1 after PASS runs, 20 Oct 2026)
$g = @(Get-FLGameProcess); $g | Stop-Process -Force -ErrorAction SilentlyContinue
$g | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
$null = & $deploy -NoStart -NoBuild -Instance $Instance 2>&1 # back to the dev build
Out-R "sweep done: $res"
