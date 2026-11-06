# Regression sweep: every area in its own section (PASS / FAIL with evidence), on to the next on failure. -Only <sections>.
# Results: research\experience\runs\sweep_<time>.txt; every section's time (and a mission's time per objective) goes to
# the benchmark history (tools\flbenchhist.ps1, Show-FLBench). Each section starts on the fixture that reaches its subject
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
. (Join-Path $fl 'tools\harness\flctl.ps1')
. (Join-Path $fl 'tools\harness\flsub.ps1')
. (Join-Path $fl 'tools\flbenchhist.ps1')
$all = @([regex]::Matches((Get-Content -Raw $PSCommandPath), "(?m)^Section '([^']+)'") | ForEach-Object { $_.Groups[1].Value })
$unknown = @($Only | Where-Object { $_ -notin $all })
if ($unknown) { throw "no such section: $($unknown -join ', ') (a sweep that runs nothing must not read as done)" }
if ($Parallel -gt 1) {
    $t0 = Get-Date
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
    foreach ($g in $groups | Where-Object { $_.list.Count }) {
        if (-not (Get-ChildItem $runs -Filter "sweep_*_i$($g.n).txt" | Where-Object LastWriteTime -ge $t0)) {
            "FAIL worker $($g.n) left no result: $($g.list -join ', ') did not run" | Tee-Object -Append -FilePath $res
        }
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

Section 'pad-device' {
    # The device-layer test pads (30 Oct 2026, IDEAS T36 slice 1, modules\controller\src\shm.h v2): a test pad on an XInput slot
    # takes a real pad's whole path. Plug slot 2 as a PlayStation pad -> found by the slot scan, glyph family follows;
    # the focus gate: closed in this background game (the stick does nothing), opened (-Foreground) with the stick still
    # held -> still nothing until released (focus_gate.h), a new push flies; the autopilot's hand-back pulse read back
    # from the rumble stream; the options page on the test pad's D-pad; unplug in flight -> lost + pause_on_disconnect;
    # replug on slot 0 as an Xbox pad -> found, family follows; off -> the family falls back to the HID list.
    Boot SaveC702.fl
    $null = Send-FL 'ai dir idle'; $null = Wait-FLFramesPassed 30
    $padLine = { (Send-FL 'state') -split "`n" | Where-Object { $_ -match '^pad ' } | Select-Object -First 1 }
    $family = { [regex]::Match((Send-FL 'padhud'), 'glyphs family=(\w+)').Groups[1].Value }
    $out = [Collections.Generic.List[string]]::new()
    try {
        $fam0 = & $family
        Enable-FLDevPads
        Set-FLDevPad -Slot 2 -Vendor playstation
        $null = Wait-FLUntil { (& $padLine) -match ' slot=2 ' } 'the test pad found on slot 2' -TimeoutSec 10
        $null = Wait-FLUntil { (& $family) -eq 'playstation' } 'glyph family playstation' -TimeoutSec 5
        $out.Add("slot 2 found, family $fam0 -> playstation")
        # gate closed (background, focus_only on): full throttle on the left stick does nothing
        Set-FLDevPad -Slot 2 -Vendor playstation -LY 1; $null = Wait-FLFramesPassed 60
        $vClosed = Speed
        if ($vClosed -gt 5) { throw "gate closed but the test pad flew: speed $([int]$vClosed)" }
        # opened while the stick is held: still ignored until released
        Enable-FLDevPads -Foreground; $null = Wait-FLFramesPassed 60
        $vHeld = Speed
        if ($vHeld -gt 5) { throw "a stick held across the gate's opening acted: speed $([int]$vHeld)" }
        Set-FLDevPad -Slot 2 -Vendor playstation; $null = Wait-FLFramesPassed 5
        Set-FLDevPad -Slot 2 -Vendor playstation -LY 1; $null = Wait-FLFramesPassed 90
        Set-FLDevPad -Slot 2 -Vendor playstation
        $vOpen = Speed
        if ($vOpen -lt 20) { throw "gate open but the test pad's new push did not fly: speed $([int]$vOpen)" }
        $out.Add("gate: closed $([int]$vClosed), held across opening $([int]$vHeld), new push $([int]$vOpen) m/s")
        # the hand-back pulse: the autopilot holds station, the test pad's stick takes the ship back -> one pulse on
        # slot 2, then the motors off
        $seq = @(Get-FLDevRumble | Select-Object -Last 1).Seq; if (-not $seq) { $seq = 0 }
        $null = Send-FL 'ap start hold'; $null = Wait-FLFramesPassed 60
        if ((Get-FLAutopilot).Mode -eq 'off') { throw "the autopilot did not start (ap start hold)" }
        Set-FLDevPad -Slot 2 -Vendor playstation -LX 1; $null = Wait-FLFramesPassed 5
        Set-FLDevPad -Slot 2 -Vendor playstation
        $null = Wait-FLUntil { @(Get-FLDevRumble -Since $seq | Where-Object { $_.Low -eq 0 -and $_.High -eq 0 }).Count } 'the pulse and its stop' -TimeoutSec 5
        $r = @(Get-FLDevRumble -Since $seq)
        $pulse = $r | Where-Object { $_.Low -gt 0 } | Select-Object -First 1
        if (-not $pulse -or $pulse.Slot -ne 2) { throw "no hand-back pulse on slot 2: $($r | ForEach-Object { "$($_.Slot):$($_.Low)/$($_.High)" })" }
        $frames = ($r[-1].Frame - $pulse.Frame)
        $out.Add(("hand-back pulse slot 2 low {0:N2} high {1:N2}, off after {2} frames" -f $pulse.Low, $pulse.High, $frames))
        # the menus' reader (uikit.cpp, its own Gamepad): the options page's focus moves by the test pad's D-pad
        $null = Send-FL 'ui options open'; $null = Wait-FLFramesPassed 10
        $focus = { [regex]::Match((Send-FL 'ui rows'), 'focus=(-?\d+)').Groups[1].Value }
        $f0 = & $focus
        Press-FLDevPad Down -Slot 2 -Vendor playstation; Press-FLDevPad Down -Slot 2 -Vendor playstation
        $f1 = & $focus
        $null = Send-FL 'ui options close'
        if (-not $f1 -or $f0 -eq $f1) { throw "the options page did not take the test pad's D-pad: focus $f0 -> $f1" }
        $out.Add("options page focus $f0 -> $f1")
        # unplugged in flight while the pad is the device: lost, and pause_on_disconnect pauses the game (its own pause
        # menu; its RESUME button clicked natively)
        $null = Start-FLEventLog
        Set-FLDevPad -Slot 2 -Connected $false
        $null = Wait-FLUntil { (& $padLine) -match ' slot=-1 ' } 'the test pad lost' -TimeoutSec 5
        $null = Wait-FLUntil { (Send-FL 'state') -match 'paused=1' } 'pause_on_disconnect pausing the game' -TimeoutSec 5
        if (-not @(Get-FLEventLog | Where-Object { "$_" -match 'device pad disconnected: pause' }).Count) { throw "paused without the disconnect event" }
        $null = Wait-FLUntil { (Send-FL 'visible PauseButtonBox0') -match '^visible' } 'the pause menu' -TimeoutSec 5
        $r = Send-FL 'click PauseButtonBox0'; if ($r -notmatch '^clicked') { throw "resume: $r" }
        $null = Wait-FLUntil { (Send-FL 'state') -match 'paused=0' } 'the pause menu resumed' -TimeoutSec 10
        $out.Add("unplugged in flight: slot -1, paused (pause_on_disconnect), resumed")
        # replugged on another slot as an Xbox pad: found at the next scan (<= 1.5 s of game time), family follows
        Set-FLDevPad -Slot 0 -Vendor xbox
        $null = Wait-FLUntil { (& $padLine) -match ' slot=0 ' } 'the test pad replugged on slot 0' -TimeoutSec 10
        $null = Wait-FLUntil { (& $family) -eq 'xbox' } 'glyph family xbox' -TimeoutSec 5
        Set-FLDevPad -Slot 0 -Connected $false
        Disable-FLDevPads
        $null = Wait-FLUntil { (& $padLine) -match ' slot=-1 ' } 'the test pads off' -TimeoutSec 5
        $null = Wait-FLUntil { (& $family) -eq $fam0 } "glyph family back to $fam0" -TimeoutSec 5
        $out.Add("replugged on slot 0 (xbox); off: slot -1, family $fam0")
    } finally { try { Disable-FLDevPads } catch {} ; $null = Send-FL 'ap stop' }
    $out -join '; '
}

Section 'pad-sdl' {
    # The SDL backend's own virtual test pads (30 Oct 2026, WORK_QUEUE Q5 slice 2, IDEAS T36 slice 2; shm.h v3, the host's
    # debug `sdlpad`): the real SDL path in the game with no device. Steps (each a done-check clause): refused while SDL is
    # off; pad_backend 1 live -> running; per type (xbox, ps5, switchpro): attach -> `state` source=sdl with SDL's type and
    # vendor, the glyph family switches, a button press and the sticks / a trigger read through `state`, the options
    # page's focus by its D-pad (the menus' own reader on the same backend), the autopilot's
    # hand-back pulse reaches the device's read-back (and its stop), a trigger rumble and an effect sent through SDL reach
    # it too, detach -> the pad gone and the family back; two pads -> the newest in use, its detach -> the older again;
    # pad_backend 0 -> SDL stopped (no backend line, status 0, attach refused).
    Boot SaveC702.fl
    $null = Send-FL 'ai dir idle'; $null = Wait-FLFramesPassed 30
    $padLine = { (Send-FL 'state') -split "`n" | Where-Object { $_ -match '^pad connected' } | Select-Object -First 1 }
    $sdlLine = { (Send-FL 'state') -split "`n" | Where-Object { $_ -match '^pad_backend=' } | Select-Object -First 1 }
    $family = { [regex]::Match((Send-FL 'padhud'), 'glyphs family=(\w+)').Groups[1].Value }
    $padVals = {
        $m = [regex]::Match((& $padLine), 'buttons=0x([0-9a-f]+) L=\(([^,]+),([^)]+)\) R=\(([^,]+),([^)]+)\) LT=(\S+) RT=(\S+)')
        if (-not $m.Success) { throw "no pad line in state" }
        [pscustomobject]@{ Buttons = [Convert]::ToInt32($m.Groups[1].Value, 16); LX = [double]$m.Groups[2].Value; LY = [double]$m.Groups[3].Value
                           RX = [double]$m.Groups[4].Value; RY = [double]$m.Groups[5].Value; LT = [double]$m.Groups[6].Value; RT = [double]$m.Groups[7].Value }
    }
    $lastOut = { $o = @(Get-FLSdlPadOut | Select-Object -Last 1); if ($o.Count -and $o[0]) { $o[0].Seq } else { 0 } }
    $kinds = @(
        @{ k = 'xbox'; type = 3; vendor = '0x045E'; fam = 'xbox' },
        @{ k = 'ps5'; type = 6; vendor = '0x054C'; fam = 'playstation' },
        @{ k = 'switchpro'; type = 7; vendor = '0x057E'; fam = 'nintendo' })
    $out = [Collections.Generic.List[string]]::new()
    try {
        $r = Send-FL 'sdlpad attach xbox'
        if ($r -notmatch '^sdlpad refused') { throw "attach with pad_backend 0 not refused: $r" }
        $null = Send-FL 'set pad_backend 1'
        $null = Wait-FLUntil { (& $sdlLine) -match 'sdl_status=2' } 'SDL running (pad_backend 1)' -TimeoutSec 10
        # What "no test pad" looks like on this machine: with a real pad plugged in, SDL rightly falls back to it after a
        # detach (sdlpad.cpp), so every detach below waits for this state, not for sdl_connected=0 (5 Nov 2026: a
        # DualSense Edge on the dev machine failed the section with the code working as designed).
        $null = Wait-FLFramesPassed 30
        $rest = [regex]::Match((& $sdlLine), 'sdl_connected=\d+ sdl_type=-?\d+ ').Value
        if (-not $rest) { throw "no SDL rest state in: $(& $sdlLine)" }
        $fam0 = & $family
        $out.Add("refused while off; SDL running: $([regex]::Match((& $sdlLine), 'sdl_init_ms=\S+').Value), rest $($rest.Trim()) $fam0")
        foreach ($t in $kinds) {
            $k = $t.k
            $r = Connect-FLSdlPad -Type $k -Slot 0 -Foreground
            if ($r -match 'NOT the expected') { throw "${k}: $r" }
            $null = Wait-FLUntil { $l = & $sdlLine; $l -match 'source=sdl' -and $l -match 'sdl_connected=1' -and $l -match "sdl_type=$($t.type) " -and $l -match "sdl_vendor=$($t.vendor)" } "${k}: state source=sdl type $($t.type) vendor $($t.vendor)" -TimeoutSec 5
            $null = Wait-FLUntil { (& $family) -eq $t.fam } "${k}: glyph family $($t.fam)" -TimeoutSec 5
            # a button (B: east, positional) and the sticks / LT through SDL's gamepad layer into `state`
            Set-FLSdlPad -Slot 0 -Buttons B
            $null = Wait-FLUntil { ((& $padVals).Buttons -band 0x2000) -ne 0 } "${k}: B read down" -TimeoutSec 5
            Set-FLSdlPad -Slot 0
            $null = Wait-FLUntil { ((& $padVals).Buttons -band 0x2000) -eq 0 } "${k}: B read up" -TimeoutSec 5
            Set-FLSdlPad -Slot 0 -LX 0.8 -RY -0.8 -LT 1
            $null = Wait-FLUntil { $p = & $padVals; $p.LX -gt 0.5 -and $p.RY -lt -0.5 -and $p.LT -gt 0.9 } "${k}: LX + / RY - / LT read" -TimeoutSec 5
            $p = & $padVals
            Set-FLSdlPad -Slot 0
            $null = Wait-FLUntil { $p2 = & $padVals; $p2.LX -eq 0 -and $p2.RY -eq 0 -and $p2.LT -eq 0 } "${k}: sticks / LT back to rest" -TimeoutSec 5
            # the menus' reader (uikit.cpp, its own Gamepad on the same backend): the options page's focus by the D-pad
            $null = Send-FL 'ui options open'; $null = Wait-FLFramesPassed 10
            $focus = { [regex]::Match((Send-FL 'ui rows'), 'focus=(-?\d+)').Groups[1].Value }
            $f0 = & $focus
            Press-FLSdlPad Down -Slot 0; Press-FLSdlPad Down -Slot 0
            $f1 = & $focus
            $null = Send-FL 'ui options close'
            if (-not $f1 -or $f0 -eq $f1) { throw "${k}: the options page did not take the test pad's D-pad: focus $f0 -> $f1" }
            # the hand-back pulse: the autopilot holds station, the test pad's stick takes the ship back
            $seq = & $lastOut
            $null = Send-FL 'ap start hold'; $null = Wait-FLFramesPassed 60
            if ((Get-FLAutopilot).Mode -eq 'off') { throw "${k}: the autopilot did not start (ap start hold)" }
            Set-FLSdlPad -Slot 0 -LX 1; $null = Wait-FLFramesPassed 5
            Set-FLSdlPad -Slot 0
            $null = Wait-FLUntil { $o = @(Get-FLSdlPadOut -Since $seq | Where-Object Kind -eq 'rumble'); @($o | Where-Object A -gt 0).Count -and $o[-1].A -eq 0 -and $o[-1].B -eq 0 } "${k}: the hand-back pulse and its stop on the device" -TimeoutSec 5
            $rum = @(Get-FLSdlPadOut -Since $seq | Where-Object Kind -eq 'rumble')
            $pulse = $rum | Where-Object A -gt 0 | Select-Object -First 1
            if ($pulse.Slot -ne 0) { throw "${k}: the pulse went to slot $($pulse.Slot)" }
            # trigger rumble and an effect, sent through SDL to the pad in use
            $seq = & $lastOut
            $r1 = Send-FL 'sdlpad send triggers 8192 32768 200'; $r2 = Send-FL 'sdlpad send effect 47'
            if ($r1 -notmatch ': ok' -or $r2 -notmatch ': ok') { throw "${k}: send: $r1 | $r2" }
            $o = @(Get-FLSdlPadOut -Since $seq)
            $tr = $o | Where-Object Kind -eq 'triggers' | Select-Object -First 1
            $fx = $o | Where-Object Kind -eq 'effect' | Select-Object -First 1
            if (-not $tr -or [Math]::Abs($tr.A - 0.125) -gt 0.001 -or [Math]::Abs($tr.B - 0.5) -gt 0.001) { throw "${k}: trigger rumble read back: $($o | ConvertTo-Json -Compress)" }
            if (-not $fx -or $fx.A -ne 47) { throw "${k}: effect read back: $($o | ConvertTo-Json -Compress)" }
            # detach: the pad gone, the family back
            $null = Disconnect-FLSdlPad -Slot 0
            $null = Wait-FLUntil { (& $sdlLine).Contains($rest) } "${k}: detached" -TimeoutSec 5
            $null = Wait-FLUntil { (& $family) -eq $fam0 } "${k}: glyph family back to $fam0" -TimeoutSec 5
            $out.Add(("{0}: type {1} vendor {2}, family {3}, B + LX {4:N2} RY {5:N2} LT {6:N2}, options focus {9} -> {10}, pulse {7:N2}/{8:N2} + stop, triggers + effect 47, detached" -f
                      $k, $t.type, $t.vendor, $t.fam, $p.LX, $p.RY, $p.LT, $pulse.A, $pulse.B, $f0, $f1))
        }
        # two at once: the newest is in use; its detach hands back to the older one
        $null = Connect-FLSdlPad -Type xbox -Slot 0 -Foreground
        $null = Connect-FLSdlPad -Type ps5 -Slot 1 -Foreground
        $null = Wait-FLUntil { (& $sdlLine) -match 'sdl_type=6 ' -and (& $family) -eq 'playstation' } 'two pads: the newest (ps5) in use' -TimeoutSec 5
        if ((Get-FLSdlPad) -notmatch 'in_use=1 ') { throw "two pads: $(Get-FLSdlPad)" }
        $null = Disconnect-FLSdlPad -Slot 1
        $null = Wait-FLUntil { (& $sdlLine) -match 'sdl_type=3 ' -and (& $family) -eq 'xbox' } 'the older (xbox) in use again' -TimeoutSec 5
        $null = Disconnect-FLSdlPad
        $null = Wait-FLUntil { (& $sdlLine).Contains($rest) } 'none attached' -TimeoutSec 5
        $out.Add("two pads: ps5 (newest) in use, its detach -> xbox again")
        # off: SDL stopped (the test pads with it)
        $null = Send-FL 'set pad_backend 0'
        $null = Wait-FLUntil { -not (& $sdlLine) } 'pad_backend 0: no backend line' -TimeoutSec 5
        if ((Get-FLSdlPad) -notmatch 'status=0 ') { throw "SDL not stopped: $(Get-FLSdlPad)" }
        $r = Send-FL 'sdlpad attach xbox'
        if ($r -notmatch '^sdlpad refused') { throw "attach after the stop not refused: $r" }
        $out.Add("pad_backend 0: SDL stopped, attach refused")
    } finally {
        try { $null = Disconnect-FLSdlPad } catch {}
        $null = Send-FL 'set pad_backend 0'; $null = Send-FL 'ap stop'
    }
    $out -join '; '
}

Section 'pad-haptics' {
    # The haptics skeleton (30 Oct 2026, WORK_QUEUE Q5 slice 3, IDEAS U30; modules\controller\src\impl\haptics_rules.h + haptics.cpp,
    # debug `haptics`): what the mixer sends, read back from the test pads' device streams. Steps = the done-check clauses:
    # (1) the XInput backend (a device-layer test pad): an injected shield hit's stream (the high motor's signature, then
    # the stop); (2) the SDL backend (an SDL test pad): the hand-back's stream - the autopilot's end by the stick, today's
    # pulse 0.36 / 0.216 at rumble 60, then its stop; (3) an injected hull hit in flight (the low motor's signature, the
    # decay, the stop; a second one inside its gap merges); (4) the confirm's two ticks; (5) nothing out of focus (the test
    # pad's foreground flag off: `silent (focus)`, no stream); (6) nothing paused (the pause menu: the world's and the
    # autopilot's effects `silent (paused)`, no stream - the game's own boxes pause the game, so the prompt still plays);
    # (7) one real hit (two level-1 pirates at 500 m, despawned at 95 % hull): a game hit's effect started (src=game) and
    # on the device. The pad is the device in use by `input_mode 1` (the background game has no input of its own).
    Boot SaveC702.fl
    $null = Send-FL 'ai dir idle'; $null = Wait-FLFramesPassed 30
    $hap = { Send-FL 'haptics status' }
    $sdlLine = { (Send-FL 'state') -split "`n" | Where-Object { $_ -match '^pad_backend=' } | Select-Object -First 1 }
    $padLine = { (Send-FL 'state') -split "`n" | Where-Object { $_ -match '^pad connected' } | Select-Object -First 1 }
    $lastOut = { $o = @(Get-FLSdlPadOut | Select-Object -Last 1); if ($o.Count -and $o[0]) { $o[0].Seq } else { 0 } }
    $rumble = { param($s) @(Get-FLSdlPadOut -Since $s | Where-Object Kind -eq 'rumble') }
    $onRuns = { param($r, $lo, $hi) $n = 0; $was = $false; foreach ($e in $r) { $on = $e.$lo -gt 0 -or $e.$hi -gt 0; if ($on -and -not $was) { $n++ }; $was = $on }; $n }
    $fmt = { param($r, $lo, $hi) ($r | ForEach-Object { '{0:N3}/{1:N3}' -f $_.$lo, $_.$hi }) -join ' ' }
    $out = [Collections.Generic.List[string]]::new()
    $ev0 = $script:secStart
    try {
        # (pause_on_disconnect off: step 1's test pad leaves in flight with the pad in use - it paused the game, the first run)
        $null = Send-FL 'set rumble 60'; $null = Send-FL 'set input_mode 1'; $null = Send-FL 'set pause_on_disconnect 0'
        # (1) XInput: a device-layer test pad on slot 0, the gate open
        Enable-FLDevPads -Foreground
        Set-FLDevPad -Slot 0 -Vendor xbox
        $null = Wait-FLUntil { (& $padLine) -match ' slot=0 ' } 'the device-layer test pad on slot 0' -TimeoutSec 10
        $null = Wait-FLUntil { (& $hap) -match 'world=open' } 'haptics open (XInput test pad)' -TimeoutSec 5
        $d = @(Get-FLDevRumble | Select-Object -Last 1).Seq; if (-not $d) { $d = 0 }
        $r = Send-FL 'haptics play hit_shield 0.8'
        if ($r -notmatch ': started') { throw "XInput: $r | $(& $hap)" }
        $null = Wait-FLUntil { $x = @(Get-FLDevRumble -Since $d); @($x | Where-Object High -gt 0).Count -and $x[-1].Low -eq 0 -and $x[-1].High -eq 0 } 'XInput: the shield hit and its stop' -TimeoutSec 5
        $x = @(Get-FLDevRumble -Since $d)
        $x1 = $x | Where-Object High -gt 0 | Select-Object -First 1
        if ([Math]::Abs($x1.High - 0.65 * 0.8 * 0.6) -gt 0.01 -or $x1.Low -ge $x1.High -or $x1.Slot -ne 0) { throw "XInput shield hit: $(& $fmt $x 'Low' 'High')" }
        $out.Add("XInput: shield hit $(& $fmt $x 'Low' 'High') in $($x[-1].Frame - $x1.Frame) frames")
        Set-FLDevPad -Slot 0 -Connected $false; Disable-FLDevPads
        # (2) SDL: the hand-back
        $null = Send-FL 'set pad_backend 1'
        $null = Wait-FLUntil { (& $sdlLine) -match 'sdl_status=2' } 'SDL running (pad_backend 1)' -TimeoutSec 10
        $null = Connect-FLSdlPad -Type xbox -Slot 0 -Foreground
        $null = Wait-FLUntil { $l = & $sdlLine; $l -match 'source=sdl' -and $l -match 'sdl_connected=1' } 'the SDL test pad in use' -TimeoutSec 5
        $null = Wait-FLUntil { (& $hap) -match 'world=open' } 'haptics open (SDL test pad)' -TimeoutSec 5
        $seq = & $lastOut
        $null = Send-FL 'ap start hold'; $null = Wait-FLFramesPassed 60
        if ((Get-FLAutopilot).Mode -eq 'off') { throw "the autopilot did not start (ap start hold)" }
        Set-FLSdlPad -Slot 0 -LX 1; $null = Wait-FLFramesPassed 5
        Set-FLSdlPad -Slot 0
        $null = Wait-FLUntil { $o = @(& $rumble $seq); @($o | Where-Object A -gt 0).Count -and $o[-1].A -eq 0 -and $o[-1].B -eq 0 } 'the hand-back pulse and its stop' -TimeoutSec 5
        $o = @(& $rumble $seq)
        $on = @($o | Where-Object A -gt 0)
        if (@($on | Where-Object { [Math]::Abs($_.A - 0.36) -gt 0.002 -or [Math]::Abs($_.B - 0.216) -gt 0.002 }).Count -or (& $onRuns $o 'A' 'B') -ne 1) { throw "hand-back stream: $(& $fmt $o 'A' 'B')" }
        $out.Add("hand-back: $(& $fmt $o 'A' 'B') ($($o[-1].Frame - $on[0].Frame) frames to the stop)")
        # (3) a hull hit injected, a second inside its gap
        $seq = & $lastOut
        $r1 = Send-FL 'haptics play hit_hull 0.6'; $r2 = Send-FL 'haptics play hit_hull 0.9'
        if ($r1 -notmatch ': started') { throw "hull hit: $r1" }
        $null = Wait-FLUntil { $o = @(& $rumble $seq); @($o | Where-Object A -gt 0).Count -and $o[-1].A -eq 0 -and $o[-1].B -eq 0 -and (& $hap) -match 'playing=0 ' } 'the hull hit(s) and the stop' -TimeoutSec 5
        $o = @(& $rumble $seq)
        $h1 = $o | Where-Object A -gt 0 | Select-Object -First 1
        if ([Math]::Abs($h1.A - 0.85 * 0.6 * 0.6) -gt 0.01 -or @($o | Where-Object { $_.A -gt 0 -and $_.A -le $_.B }).Count) { throw "hull hit stream (low over high): $(& $fmt $o 'A' 'B')" }
        if ($o.Count -gt 24) { throw "hull hit: $($o.Count) commands (a buzz?): $(& $fmt $o 'A' 'B')" }
        $out.Add("hull hit 0.6 then 0.9 ($($r2 -replace '^haptics play hit_hull: ', '')): $($o.Count) commands, $(& $onRuns $o 'A' 'B') run(s), $(& $fmt $o 'A' 'B')")
        # (4) the confirm: two ticks
        $seq = & $lastOut
        $r = Send-FL 'haptics play confirm'
        if ($r -notmatch ': started') { throw "confirm: $r" }
        $null = Wait-FLUntil { $o = @(& $rumble $seq); (& $hap) -match 'playing=0 ' -and $o.Count -and $o[-1].A -eq 0 } 'the confirm played out' -TimeoutSec 5
        $o = @(& $rumble $seq)
        if ((& $onRuns $o 'A' 'B') -ne 2) { throw "confirm: not two ticks: $(& $fmt $o 'A' 'B')" }
        $out.Add("confirm: 2 ticks $(& $fmt $o 'A' 'B')")
        # (5) out of focus: nothing
        Set-FLSdlForeground $false
        $null = Wait-FLUntil { (& $hap) -match 'world=focus' } 'haptics silent: focus' -TimeoutSec 5
        $seq = & $lastOut
        $r1 = Send-FL 'haptics play hit_hull 1'; $r2 = Send-FL 'haptics play handback'; $r3 = Send-FL 'haptics play confirm'
        $null = Wait-FLFramesPassed 60
        $o = @(& $rumble $seq | Where-Object { $_.A -gt 0 -or $_.B -gt 0 })
        if ($r1 -notmatch 'silent \(focus\)' -or $r2 -notmatch 'silent \(focus\)' -or $r3 -notmatch 'silent \(focus\)' -or $o.Count) { throw "out of focus: $r1 | $r2 | $r3 | stream $(& $fmt $o 'A' 'B')" }
        Set-FLSdlForeground $true
        $null = Wait-FLUntil { (& $hap) -match 'world=open' } 'haptics open again' -TimeoutSec 5
        $out.Add("out of focus: hit / hand-back / confirm silent (focus), no stream in 60 frames")
        # (6) paused: the world's and the autopilot's silent, the prompt plays
        $r = Send-FL 'pause'
        $null = Wait-FLUntil { (Send-FL 'state') -match 'paused=1' } 'the game paused (pause menu)' -TimeoutSec 5
        $null = Wait-FLUntil { (& $hap) -match 'world=paused' -and (& $hap) -match 'prompt=open' } 'haptics: world paused, prompt open' -TimeoutSec 5
        $seq = & $lastOut
        $r1 = Send-FL 'haptics play hit_hull 1'; $r2 = Send-FL 'haptics play handback'
        $null = Wait-FLFramesPassed 60
        $o = @(& $rumble $seq | Where-Object { $_.A -gt 0 -or $_.B -gt 0 })
        if ($r1 -notmatch 'silent \(paused\)' -or $r2 -notmatch 'silent \(paused\)' -or $o.Count) { throw "paused: $r1 | $r2 | stream $(& $fmt $o 'A' 'B')" }
        $r3 = Send-FL 'haptics play confirm'
        $null = Wait-FLUntil { $o = @(& $rumble $seq); $o.Count -and (& $onRuns $o 'A' 'B') -eq 2 -and $o[-1].A -eq 0 } 'paused: the confirm still asks (two ticks)' -TimeoutSec 5
        $null = Wait-FLUntil { (Send-FL 'visible PauseButtonBox0') -match '^visible' } 'the pause menu' -TimeoutSec 5
        $r = Send-FL 'click PauseButtonBox0'; if ($r -notmatch '^clicked') { throw "resume: $r" }
        $null = Wait-FLUntil { (Send-FL 'state') -match 'paused=0' } 'the pause menu resumed' -TimeoutSec 10
        $out.Add("paused: hit / hand-back silent (paused), no stream in 60 frames; confirm $($r3 -replace '^haptics play confirm: ', '') with 2 ticks")
        # (7) a real hit
        $seq = & $lastOut
        $null = Send-FL 'spawn clearat 0.95'
        $null = Send-FL 'spawn fc_lr_pi_fighter_d3 fc_lr_grp 2 500 level=1'
        $null = Wait-FLUntil { @(Get-FLEventLog $ev0 | Where-Object { "$_" -match 'haptics started hit_(shield|hull) \S+ src=game' }).Count -and @(& $rumble $seq | Where-Object { $_.A -gt 0 -or $_.B -gt 0 }).Count } 'a real hit felt (src=game) on the device' -TimeoutSec 120
        $hit = @(Get-FLEventLog $ev0 | Where-Object { "$_" -match 'haptics started hit_(shield|hull) \S+ src=game' })[0]
        $o = @(& $rumble $seq)
        $null = Send-FL 'spawn clear'; $null = Send-FL 'spawn clearat off'
        $out.Add("real hit: '$(("$hit" -split ' ', 3)[2])', device $(& $fmt ($o | Select-Object -First 4) 'A' 'B') ...; $((& $hap) -replace '^.*?(game_hits=\d+ game_fails=\d+).*$', '$1')")
    } finally {
        try { Disable-FLDevPads } catch {}
        try { $null = Disconnect-FLSdlPad } catch {}
        $null = Send-FL 'spawn clear'; $null = Send-FL 'spawn clearat off'
        $null = Send-FL 'set pad_backend 0'; $null = Send-FL 'set input_mode 0'; $null = Send-FL 'set pause_on_disconnect 1'
        $null = Send-FL 'ap stop'
        if ((Send-FL 'state') -match 'paused=1') { $null = Send-FL 'click PauseButtonBox0' }
    }
    $out -join '; '
} -budget 300

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

Section 'target-churn' {
    # Ship controls and autopilots stay robust to capricious targeting (the owner, 2 Nov 2026, after the M01 formation
    # stall at LockManeuvers; the maneuver watchdog, core\game\maneuver_watch.h): from free roam (SaveC705)
    # a game formation on a nearby friendly, a game goto to a station and the Advanced Autopilot's dock each get 6 s of
    # target churn (next enemy, clear, double clear, a station, a planet, the sun, the farthest object, back) with small
    # pad stick nudges (0.15 for 0.3 s). PASS per leg: the maneuver kept running (or ended at its goal), no stall
    # (< 5 m/s for > 3 s with the goal > 150 m away), the distance to the leader / goal not growing (formation: held
    # within 300 m), FLvibed answering.
    Boot SaveC705.fl
    $null = Step-FLPopups -Now
    $start = $script:FLLogSeq
    $cs = @(Get-FLContacts -All)
    $friend = $cs | Where-Object { $_.Ship -and $_.Attitude -gt 0.2 -and $_.Distance -lt 6000 } | Sort-Object Distance | Select-Object -First 1
    $stations = @($cs | Where-Object { $_.Dock -and -not $_.Ship -and $_.Type -notmatch 'lane|ring|gate|hole' } | Sort-Object Distance)
    $planet = $cs | Where-Object { $_.Type -match 'planet' } | Select-Object -First 1
    $sun = $cs | Where-Object { $_.Type -match 'sun|star' } | Select-Object -First 1
    $far = $cs | Sort-Object Distance -Descending | Select-Object -First 1
    $gotoTo = $stations | Where-Object { $_.Distance -gt 4000 } | Select-Object -First 1
    if (-not $gotoTo) { $gotoTo = $stations | Select-Object -Last 1 }
    $dockTo = $stations | Select-Object -First 1
    if (-not $friend -or -not $gotoTo -or -not $dockTo) { throw "fixture: friend=$($friend.Id) station=$($gotoTo.Id) dock=$($dockTo.Id) in $($cs.Count) contacts" }
    $churn = @({ Send-FL 'tap USER_NEXT_ENEMY' }, { Send-FL 'tap USER_CLEAR_TARGET' },
               { Send-FL 'tap USER_CLEAR_TARGET'; Send-FL 'tap USER_CLEAR_TARGET' },
               { Send-FL "settarget $($stations[-1].Id)" }, { if ($planet) { Send-FL "settarget $($planet.Id)" } },
               { if ($sun) { Send-FL "settarget $($sun.Id)" } }, { Send-FL "settarget $($far.Id)" },
               { Set-FLPad -RX 0.15; $null = Wait-FLFramesPassed 18 1; Set-FLPad },
               { Set-FLPad -LX 0.15; $null = Wait-FLFramesPassed 18 1; Set-FLPad }, { Send-FL "settarget $($gotoTo.Id)" })
    $dist = { param($id) $c = @(Get-FLContacts -All) | Where-Object Id -eq $id | Select-Object -First 1; if ($c) { [double]$c.Distance } else { -1 } }
    $leg = {
        param($name, $goalOf, $running, [double]$hold)
        $p0 = Get-FLPilot; $g = & $goalOf $p0; $d0 = & $dist $g
        $t0 = Get-Date; $last = $p0; $lastT = $t0; $slowSince = $null; $stall = 0.0; $maxD = $d0; $minV = 999.0; $i = 0; $diag = ''
        $seq0 = $script:FLLogSeq; $trace = [Collections.Generic.List[string]]::new(); $cruiseSeen = $false
        while (((Get-Date) - $t0).TotalSeconds -lt 6) {
            $null = & $churn[$i % $churn.Count]; $i++
            $null = Wait-FLFramesPassed 20 2
            $p = Get-FLPilot; $now = Get-Date
            if (-not $p) { throw "$name`: not in flight after churn $i" }
            $dt = ($now - $lastT).TotalSeconds
            $v = if ($dt -gt 0) { [Math]::Sqrt((0..2 | ForEach-Object { [Math]::Pow($p.Pos[$_] - $last.Pos[$_], 2) } | Measure-Object -Sum).Sum) / $dt } else { 0 }
            $g = & $goalOf $p; if (-not $g) { $g = & $goalOf $p0 }
            $d = & $dist $g
            $minV = [Math]::Min($minV, $v); $maxD = [Math]::Max($maxD, $d); if ($p.Cruise) { $cruiseSeen = $true }
            $gc = @(Get-FLContacts -All) | Where-Object Id -eq $g | Select-Object -First 1
            $trace.Add(("churn {0} op {1} sel {2} v {3:N1} d {4:N0} goal {5} {6} pos {7} throttle {8} | {9} | {10}" -f $i, $p.Op, $p.Target, $v, $d, $g, $gc.Name, ($p.Pos -join ','), (([regex]::Match((Send-FL 'state'), 'throttle=(\S+)')).Groups[1].Value), (Send-FL 'ai state'), (Send-FL 'ai dockport')))
            if ($v -lt 5 -and $d -gt 150) {
                if (-not $slowSince) { $slowSince = $lastT; $diag += " [slow at churn $i, op $($p.Op), sel $($p.Target): $(Send-FL 'ap status')]" }
                $stall = [Math]::Max($stall, ($now - $slowSince).TotalSeconds)
            } else { $slowSince = $null }
            $last = $p; $lastT = $now
            if (-not (& $running $p $d)) { break }
        }
        $null = Send-FL 'tap USER_CLEAR_TARGET'
        $p1 = Get-FLPilot; $d1 = & $dist $g
        $ok = (& $running $p1 $d1); $alive = [bool](Send-FL 'state')
        $line = "{0}: op {1}->{2} dist {3:N0}->{4:N0} (max {5:N0}) min speed {6:N1} stall {7:N1}s churns {8}" -f $name, $p0.Op, $p1.Op, $d0, $d1, $maxD, $minV, $stall, $i
        $why = @()
        if (-not $ok) { $why += 'maneuver lost' }
        # The game's dock queue is no stall of ours (2 Nov 2026, churn_stall_2026-11-02_032149_ap-dock.txt: the reused
        # game's failures were the server's "dock in use - entering queue" - kind 0 response 3, an NPC docking at the same
        # station - and the game's own dock holding the ship at rest in the queue, op 7 and the autopilot's goal unchanged
        # through every churn). The client gets only its own replies; queued = the leg's last dock reply is 3.
        $replies = @(Get-FLEventLog $seq0 | Where-Object { "$_" -match 'request-reply ship=\d+ kind=0 response=(\d+)' } | ForEach-Object { [int]$Matches[1] })
        if ($stall -gt 3 -and $replies.Count -and $replies[-1] -eq 3) { $line += ' (held in the game''s dock queue)'; $stall = 0 }
        if ($stall -gt 3) {
            # the evidence for a stall (2 Nov 2026: ap-dock stalled at 0 m/s in a reused game): the autopilot's state
            # when it slowed and its own events in the leg
            $ev = @(Get-FLEventLog $seq0 | Select-Object -Last 40)
            $file = Join-Path $PSScriptRoot ("..\..\research\experience\runs\churn_stall_{0:yyyy-MM-dd_HHmmss}_{1}.txt" -f (Get-Date), $name)
            Set-Content -Path $file -Value (@($line, "diag:$diag", '--- trace') + $trace + @('--- events') + $ev)
            $why += "stalled (trace $file)"
        }
        # The wing's cruise is no broken formation (tests\formation_control.ps1, 5 Nov 2026, research\experience\runs\formation_control): cruise is
        # shared with the wing, the follower's charge starts after the leader's, so the distance to the wing leader opens by up to 350 m and closes again
        # with the op at formation throughout; a patrol starting its cruise inside the 6 s was every "left the formation" of 5 Nov
        if ($hold -gt 0 -and $maxD -gt $hold) { if ($cruiseSeen) { $line += " (the wing's cruise opened it to $([int]$maxD) m)" } else { $why += "left the formation (> $hold m)" } }
        if ($hold -le 0 -and $d1 -gt $d0 + 100) { $why += 'goal receding' }
        if (-not $alive) { $why += 'FLvibed silent' }
        # any failure keeps its trace, not only a stall: 'left the formation' twice on 5 Nov left nothing to read
        if ($why -and -not ($why -match 'stalled')) {
            $ev = @(Get-FLEventLog $seq0 | Select-Object -Last 40)
            $file = Join-Path $PSScriptRoot ("..\..\research\experience\runs\churn_fail_{0:yyyy-MM-dd_HHmmss}_{1}.txt" -f (Get-Date), $name)
            Set-Content -Path $file -Value (@($line, "why: $($why -join ', ')", "goal $g", (Send-FL 'ai state'), '--- trace') + $trace + @('--- events') + $ev)
        }
        [pscustomobject]@{ Line = $line; Fail = ($why -join ', ') }
    }
    $results = @()
    $null = Start-FLFormation $friend.Id
    # churn once formed (the join flies in from the friend's distance: 863 m on 2 Nov 2026)
    $null = Wait-FLUntil { $null = Step-FLPopups; $p = Get-FLPilot; if (-not $p -or -not $p.Leader) { return $false }; $d = & $dist $p.Leader; $d -ge 0 -and $d -le 250 } 'formed (leader within 250 m)' 40
    $results += & $leg 'formation' { param($p) $p.Leader } { param($p, $d) $p.Op -eq 'formation' } 300
    $null = Stop-FLManeuver
    $null = Set-FLTarget $gotoTo.Id
    $null = Start-FLManeuver USER_MANEUVER_GOTO goto
    $results += & $leg 'goto' { param($p) $gotoTo.Id } { param($p, $d) $p.Op -eq 'goto' -or ($d -ge 0 -and $d -lt 600) } 0
    $null = Stop-FLManeuver
    $null = Start-FLAutopilot dock $dockTo.Id
    # (the autopilot holds its own goal through the churn: a selection change never becomes its dock target)
    $results += & $leg 'ap-dock' { param($p) $dockTo.Id } { param($p, $d) $a = Get-FLAutopilot; if ($a.Mode -notin 'off', '') { $a.Target -eq $dockTo.Id } else { $p.Op -eq 'dock' } } 0
    $null = Send-FL 'ap stop'
    $null = Stop-FLManeuver
    $wd = @(Get-FLEventLog $start | Where-Object { "$_" -match 'maneuver (stalled|watchdog)' }).Count
    $sum = ($results.Line -join ' | ') + " | watchdog lines $wd"
    $bad = @($results | Where-Object Fail | ForEach-Object { "$($_.Line.Split(':')[0]): $($_.Fail)" })
    if ($bad) { throw "$($bad -join '; ') :: $sum" }
    $sum
} -budget 150

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
    $o = & (Join-Path $fl 'tools\harness\missions\M01a.ps1') *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M01a $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M01a: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
}

Section 'mission-M01b' {
    Boot (Join-Path $fl 'research\saves\bc46-all-missions\Save5f61.fl') -FreshOnly
    $o = & (Join-Path $fl 'tools\harness\missions\M01b.ps1') *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M01b $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M01b: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
}

Section 'mission-M02-scenes' {
    $s = Start-FLScene M02 8
    if (-not $s.Valid) { throw "scene 8 stale: $($s.Why)" }
    $o = & (Join-Path $fl 'tools\harness\missions\M02.ps1') -From 8 *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M02 $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M02 from 8: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
}

Section 'objective-M03' {
    # the autopilot's objective mode flies every space part of M03; the harness only does the rooms
    Boot (Join-Path $fl 'research\saves\bc46-all-missions\Save1e7d.fl') -FreshOnly
    $o = & (Join-Path $fl 'tools\harness\missions\M03objective.ps1') *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M03o $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M03 objective mode: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
} -budget 1400

Section 'objective-M04' {
    # M04 "Escape from Liberty" by the objective mode (space); the harness only does the rooms
    Boot (Join-Path $fl 'research\saves\bc46-all-missions\Save6380.fl') -FreshOnly
    $o = & (Join-Path $fl 'tools\harness\missions\M04objective.ps1') *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M04o $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M04 objective mode: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
} -budget 1800

Section 'objective-M05' {
    # M05 (Cambridge -> Omega-3 -> Leeds) by the objective mode (space); the harness only does the rooms
    # (Complete-FLBaseStory at Sprague and Baxter, the Tobias scene at Leeds)
    Boot (Join-Path $fl 'research\saves\bc46-all-missions\Save1d87.fl') -FreshOnly
    $o = & (Join-Path $fl 'tools\harness\missions\M05objective.ps1') *>&1 | ForEach-Object { "$_" }
    Add-Content $res ($o | ForEach-Object { "    M05o $_" })
    if (-not ($o -match 'SUCCESS')) { throw "M05 objective mode: $(($o | Select-Object -Last 3) -join ' | ')" }
    ($o | Select-String 'total') -join ''
} -budget 1900

Section 'fight-matrix' {
    # The free-roam retreat must not abandon fights the autopilot wins (user, 19 Oct 2026): tools\harness\
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
        $rows += @(& (Join-Path $fl 'tools\harness\flfights.ps1') -Save $s.save -Tag $s.tag) |
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
            # HUD 2 (modules\hud\docs\UI_HUD2.md): with the pad the device and hud_style 1 / 2 it replaces the game's panels
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

Section 'modules-matrix' {
    # The cut modules' switches (docs\MODULES_PLAN.md P5): each one off with the rest on, each one on with the controller off, all on; core
    # only and `safe` by tests\modules_nothing.ps1, which needs no game. A module that is off leaves no hook and no verb: every row's hook
    # table is judged against the all-on game's. The release build, one fresh game per row. SetAppActive stays in every row: a background
    # test start hooks it for the focus guard, whatever the display row says. `coreVerbs`: verbs of core's page host, through its `core` verb, with no channel of the
    # controller's. ChatFlush (the server's word on the autopilot) is core's and in every row; so is the watchdog's verb.
    $inst = if ($Instance -gt 0) { $Instance } else { 9 }
    $savedInstance = $env:FLVIBED_INSTANCE
    $env:FLVIBED_INSTANCE = "$inst"
    $null = New-FLInstance $inst
    $both = 'subtitles', 'controller'
    $display = 'SetupScreen', 'WindowStyle'
    $ap = 'GunAimPoint', 'JumpInComplete', 'ManeuverFrameSize', 'ManeuversMessage'
    $movie = 'import movie.dll!?PlayMovie@MediaPlayer@@YAJPAUHWND__@@_NPBD@Z', 'import ole32.dll!CoCreateInstance'
    $configs = @(
        @{ name = 'all on'; modules = $both; verbs = 'stalls', 'livesplit', 'timer', 'winmode', 'hud2', 'uiskin' },
        @{ name = 'npc off'; modules = $both + 'npc=0'; gone = 'stalls'; verbs = 'livesplit', 'timer', 'winmode' },
        @{ name = 'speedrun off'; modules = $both + 'speedrun=0'; gone = 'livesplit', 'timer'; verbs = 'stalls', 'winmode' },
        @{ name = 'display off'; modules = $both + 'display=0'; gone = @('winmode'); verbs = 'stalls', 'livesplit', 'timer'; noHooks = $display },
        @{ name = 'controller off'; modules = @('subtitles'); noHooks = $display; hasHooks = @('ChatFlush'); alone = $true; verbs = @('watchdog') },
        @{ name = 'npc on alone'; modules = 'subtitles', 'npc=1'; noHooks = $display; alone = $true },
        @{ name = 'speedrun on alone'; modules = 'subtitles', 'speedrun=1'; noHooks = $display + 'IsCmdHeld'; hasHooks = @('ChatFlush'); alone = $true; coreVerbs = 'timer', 'livesplit', 'startprompt'; verbs = 'mp', 'fault status', 'watchdog' },
        @{ name = 'keyboard and mouse, all'; modules = 'subtitles', 'hud=1', 'autopilot=1', 'speedrun=1', 'display=1', 'npc=1'; hasHooks = $display + $ap + 'ChatFlush'; alone = $true; coreVerbs = 'timer', 'livesplit', 'startprompt', 'stalls', 'hud2'; verbs = 'mp', 'fault status', 'watchdog', 'ap status', 'winmode' },
        @{ name = 'display on alone'; modules = 'subtitles', 'display=1'; hasHooks = $display; alone = $true; verbs = @('winmode') },
        @{ name = 'movies off'; modules = $both + 'movies=0'; hasHooks = $movie; alone = $true },   # (lone: the subtitles' channel, whose rows these are)
        @{ name = 'subtitles off'; modules = @('controller'); noSubs = $true; noHooks = $movie },
        @{ name = 'movies alone'; modules = 'controller', 'movies=1'; noSubs = $true; hasHooks = $movie },
        @{ name = 'autopilot off'; modules = $both + 'autopilot=0'; noHooks = $ap; verbs = 'stalls', 'livesplit', 'timer', 'winmode', 'watchdog' },
        @{ name = 'autopilot alone'; modules = 'subtitles', 'autopilot=1'; alone = $true; hasHooks = $ap; noHooks = $display },
        @{ name = 'hud off'; modules = $both + 'hud=0'; gone = 'hud2', 'uiskin', 'uistrip', 'gametext', 'cue'; verbs = 'stalls', 'livesplit', 'timer', 'winmode' },
        @{ name = 'hud alone'; modules = 'subtitles', 'hud=1'; alone = $true; hasHooks = 'HandleKey', 'UiControlAdd'; noHooks = $display + 'IsCmdHeld'; verbs = 'hud2', 'uiskin' }
    )
    function Hook-Names([string]$reply) { @($reply -split "`n" | Where-Object { $_ -match '^\S+ ' } | ForEach-Object { $w = $_ -split ' '; if ($w[0] -eq 'import') { "import $($w[1])" } else { $w[0] } } | Sort-Object -Unique) }
    function Frames-Now { [int](((Send-FLS 'state') -split "`n" | Where-Object { $_ -match '^frames' }) -replace '\D', '') }
    $rows = [Collections.Generic.List[string]]::new()
    $bad = [Collections.Generic.List[string]]::new()
    $base = $null
    try {
        foreach ($c in $configs) {
            Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
            $null = & $deploy -Instance $inst -Background -Restart -NoBuild -Release -Modules $c.modules -Save SaveC702.fl 2>&1
            $f1 = -1
            if ($c.noSubs) { if ((Send-FL 'ping') -ne 'pong') { $bad.Add("$($c.name): the controller does not answer") } } else {
                $f0 = Frames-Now
                $until = (Get-Date).AddSeconds(20)
                do { $f1 = Frames-Now } while ($f1 -lt $f0 + 120 -and (Get-Date) -lt $until)
                if ($f1 -lt $f0 + 120) { $bad.Add("$($c.name): the game stopped at frame $f1") }
            }
            $send = if ($c.alone) { { param($v) Send-FLS $v } } else { { param($v) Send-FL $v } }
            $hooks = Hook-Names (& $send 'hooks')
            if (-not $base -and -not $c.alone) { $base = $hooks }
            $unknown = (& $send 'flvibed_no_such_verb') -replace 'flvibed_no_such_verb', 'X'
            $missing = @(); $still = @()
            foreach ($v in @($c.verbs)) { if ($v -and ((& $send $v) -replace [regex]::Escape($v), 'X') -eq $unknown) { $missing += $v } }
            foreach ($v in @($c.gone)) { if ($v -and ((& $send $v) -replace [regex]::Escape($v), 'X') -ne $unknown) { $still += $v } }
            foreach ($v in @($c.coreVerbs)) { if ($v -and ((Send-FLS "core $v") -replace [regex]::Escape(($v -split ' ')[0]), 'X') -eq ((Send-FLS 'core flvibed_no_such_verb') -replace 'flvibed_no_such_verb', 'X')) { $missing += "core $v" } }
            if ($missing) { $bad.Add("$($c.name): no answer to $($missing -join ', ')") }
            if ($still) { $bad.Add("$($c.name): still answers $($still -join ', ')") }
            foreach ($h in @($c.noHooks)) { if ($h -and $hooks -contains $h) { $bad.Add("$($c.name): hook $h installed") } }
            foreach ($h in @($c.hasHooks)) { if ($h -and $hooks -notcontains $h) { $bad.Add("$($c.name): hook $h missing") } }
            if (-not $c.alone -and -not $c.noSubs) {
                $extra = @($hooks | Where-Object { $base -notcontains $_ })
                $lost = @($base | Where-Object { $hooks -notcontains $_ -and @($c.noHooks) -notcontains $_ })
                if ($extra -or $lost) { $bad.Add("$($c.name): hooks differ from all on: +$($extra -join ',') -$($lost -join ',')") }
            }
            $rows.Add("$($c.name): modules $($c.modules -join ','), $($hooks.Count) hooks, frames $f1")
        }
        $nothing = & (Join-Path $fl 'tests\modules_nothing.ps1') 2>&1
        if ($LASTEXITCODE -ne 0) { $bad.Add("core only / safe: $($nothing | Select-Object -Last 1)") }
        $rows.Add("core only, safe: $($nothing | Select-Object -Last 1)")
    } finally {
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
        $null = & $deploy -Instance $inst -NoBuild -NoStart -Modules $both 2>&1
        $env:FLVIBED_INSTANCE = $savedInstance
    }
    Add-Content $res ($rows | ForEach-Object { "    modules $_" })
    if ($bad.Count) { throw ($bad -join '; ') }
    "$($rows.Count) rows OK: " + (($rows | ForEach-Object { ($_ -split ':')[0] }) -join '; ')
} -budget 480

Section 'display-subtitles' {
    # Real dialogue in the two extreme aspect ratios (user, 21 Oct 2026: "on-screen subtitle analysis - in space and on
    # a planet; are cutscenes working as intended, is spacing reasonable"): from Save2b9b (M01, Manhattan bar) Juni's
    # room scene (letterboxed), the offer accepted, the launch with its animations, the M01a opening cutscene in space
    # (cutscene camera, letterboxed), then King's flight comms. Every shown line is judged by the module's own layout
    # report (debug `layout`: the frame's geometry, no pixel reading) against the design rules (modules\subtitles\docs\
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
        # (tab-noroom since fix-text 5, 25 Oct 2026: without it optional here the match failed and every tab read 0)
        $q = [regex]::Match($t, 'tab=([\d.-]+) body=([\d.-]+)(?: tab-noroom=\d+)? cruise=([\d.-]+),([\d.-]+)')
        if ($p.Success -and -not $q.Success) { $bad.Add("layout: the panel line's tab / body / cruise fields not found (format changed?)") }
        $panel = if ($p.Success) {
            @{ x = [double]$p.Groups[1].Value; w = [double]$p.Groups[2].Value; h = [double]$p.Groups[3].Value; yoff = [double]$p.Groups[4].Value
               s = [double]$p.Groups[5].Value; moved = $p.Groups[6].Value -eq '1'; barRight = [double]$p.Groups[7].Value; nnLeft = [double]$p.Groups[8].Value
               top = if ($p.Groups[9].Success) { [double]$p.Groups[9].Value } else { 0 }
               tab = if ($q.Success) { [double]$q.Groups[1].Value } else { 0 }; body = if ($q.Success) { [double]$q.Groups[2].Value } else { 0 }
               cruiseRight = if ($q.Success) { [double]$q.Groups[3].Value } else { 0 }; cruiseBottom = if ($q.Success) { [double]$q.Groups[4].Value } else { 0 } }
        }
        $bn = [regex]::Match($t, 'banner x=([\d.-]+) y=([\d.-]+)')
        $banner = if ($bn.Success) { @{ y = [double]$bn.Groups[2].Value } }
        # HUD 2 (hud_style 1 / 2, the default since 0.2.0, on with K&M too): it hides the game's bar and NeuroNet box and
        # gives the dialogue a free band at the top (hud2-place band=L..R px)
        $h2 = $t -match 'hud2 bus=1 on=1'
        $hb = [regex]::Match($t, 'hud2-place band=([\d.-]+)\.\.([\d.-]+)')
        $band = if ($hb.Success) { @{ l = [double]$hb.Groups[1].Value; r = [double]$hb.Groups[2].Value } }
        @{ sw = $sw; sh = $sh; px = [int]$g.Groups[3].Value; context = $g.Groups[5].Value; lb = [int]$g.Groups[6].Value; anchor = [double]$g.Groups[7].Value / $sh; fromTop = $g.Groups[8].Value -eq 'top'; blocks = $blocks; panel = $panel; banner = $banner; hud2 = $h2; band = $band }
    }
    function Judge([string]$tag, $lay) {
        # the flight top panel draws 10 % under the size table since 0.2.1 (display.cpp DrawImpl fontScale 0.9)
        $pct = if ($lay.context -eq 'flight' -and $lay.panel) { 0.036 * 0.9 } else { 0.036 }
        $want = [Math]::Round($lay.sh * $pct)
        if ([Math]::Abs($lay.px - $want) -gt 1) { $bad.Add("${tag}: font $($lay.px) px, expected $want ($($pct * 100) % of $($lay.sh))") }
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
            if ($lay.hud2) {
                # HUD 2: no name tab (narrow or not), the panel inside HUD 2's free band (the game's bar and NeuroNet box
                # are hidden, their readings stale)
                if ($pn.s -ge 1 -and $pn.tab -gt 0) { $bad.Add("${tag}: the name-tab layout under HUD 2") }
                if ($pn.s -ge 1 -and $lay.band) {
                    $left = $pn.x - $pn.w / 2; $right = $pn.x + $pn.w / 2
                    if ($pn.w -le $lay.band.r - $lay.band.l + 1 -and ($left -lt $lay.band.l - 1 -or $right -gt $lay.band.r + 1)) { $bad.Add("${tag}: HUD 2 panel ($([int]$left)..$([int]$right) px) leaves its band ($([int]$lay.band.l)..$([int]$lay.band.r))") }
                }
                $script:hud2Seen++
            } elseif ($pn.s -ge 1 -and $pn.tab -gt 0) {
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
            # the classic HUD's expectations (the bar moved, the name tab on narrow screens) pinned to hud_style 0: the
            # default since 0.2.0 is HUD 2 (on with K&M too), whose placement has no tab (hotfix 2 Nov 2026)
            $null = Send-FL 'set hud_style 0'
            $script:panelSeen = 0; $script:tabSeen = 0; $script:hud2Seen = 0
            $summary.Add((Capture-Lines "$($m.name) flight" 90 4 "subs_$($m.name)_flight"))
            # space_position 2 is the default: in flight the lines go to the top panel; narrow screens (4:3: aspect
            # < ~1.42) use the name-tab layout (space_narrow 5, the default), wide ones the plain panel
            if ($script:panelSeen -eq 0) { $bad.Add("$($m.name): no flight line in the top panel (space_position 2)") }
            if ($script:hud2Seen -gt 0) { $bad.Add("$($m.name): HUD 2 on with hud_style 0") }
            $wh = [double[]]($m.size -split ',\s*'); $narrow = $wh[0] / $wh[1] -lt 1.42
            if ($narrow -and $script:tabSeen -eq 0) { $bad.Add("$($m.name): narrow screen without the name-tab layout (space_narrow 5)") }
            if (-not $narrow -and $script:tabSeen -gt 0) { $bad.Add("$($m.name): the name-tab layout on a wide screen") }
            # HUD 2 (hud_style 2, the default): a two-speaker stack (King + Newark Station, long lines) in its band, no tab
            $null = Send-FL 'set hud_style 2'
            $null = Send-FLS 'clear'
            $null = Wait-FLUntil { (Send-FLS 'layout') -match 'hud2 bus=1 on=1' } 'HUD 2 on' 10
            $script:hud2Seen = 0
            $null = Send-FLS 'say DX_M01_0120_KING king'; $null = Send-FLS 'say DX_M01_0190_STATION_NEWARK m01a_newark'
            $null = Wait-FLUntil { (Send-FLS 'layout') -match 'panel x=\S+ w=\S+ h=\S+ yoff=\S+ s=1 ' } 'the HUD 2 panel' 10
            $lay = Read-Layout
            if ($lay -and $lay.blocks.Count -ge 2) { Judge "$($m.name) hud2 stack" $lay; $null = Save-FLShot "subs_$($m.name)_hud2_stack" -Dir $shotDir }
            else { $bad.Add("$($m.name): HUD 2 two-speaker stack not shown") }
            if ($script:hud2Seen -eq 0) { $bad.Add("$($m.name): HUD 2 did not place the stack (hud_style 2)") }
            $summary.Add("$($m.name) hud2 stack $(if ($lay) { $lay.blocks.Count } else { 0 }) blocks")
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
