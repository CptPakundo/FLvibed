# One speedrun leg of any story mission by one set of rules (22 Oct 2026, the AFK speedrun plan; docs\LEGS.md): in
# space the autopilot's objective mode (`ap start objective`) flies everything; docked, the harness plays the base the
# way a player does (Complete-FLBaseStory: story talks, scenes skipped, the objective's room, a room tour while the
# story keeps the player in, repair + restock, launch). Replaces the per-mission M03 / M04 / M05objective scripts'
# pattern for every mission (their bodies were this loop with a room list).
# Start: the game in the leg's start state (docked after the accept, or in space mid-mission).
# Ends on the mission's result (SUCCESS / FAILURE / death), on -Until, on the time-out, or OFF PACE: with -BudgetSec the
# leg is stopped as soon as it is over budget (user, 22 Oct 2026: no long tests whose state is unknown), or STALL: no
# progress in space for -StallSec (default 60; 0 = off) - the watchdog in the space wait below.
# Prints one line per phase and "SUCCESS total <s>" / "FAILED (<why>) total <s>"; with -Out a phase table (TSV).
# -Chain (the campaign run, S4): a mission's SUCCESS is a split, not the end - the story goes on (base stories, the
# money step while a net-worth gate is open, the next offer); the run ends with the success of Mission 13 (its title
# 24400 seen in the run), a failure, -Until or the time-out.
param([int]$TimeoutSec = 3600, [double]$BudgetSec = 0, [int]$StallSec = 60, [string]$Label = 'leg', [scriptblock]$Until = { $false },
      [string]$Out = '', [switch]$Chain, [switch]$NoFailStates, [switch]$Full, [string]$ReplayLog = '', [double]$ReplayT0 = 0,
      # Segment practice (docs\TESTING.md "The test loop" 4): -SaveAt <checkpoints> writes a native save the moment each
      # is reached (catalogued as scene <leg> step <checkpoint>, source leg-checkpoint; its cost logged); -StartCp <cp>
      # = this run starts from such a save: timed from the leg start, the next checkpoint its first segment (kills
      # counted after the save's); -StopAt <cp> ends the leg UNTIL once that checkpoint is reached. -StartSave <file> =
      # the save -StartCp loaded (flleg.ps1 passes it; else the newest leg-checkpoint save of that checkpoint): the
      # save-fit check below judges that catalogue row.
      [string[]]$SaveAt = @(), [string]$StartCp = '', [string]$StopAt = '', [string]$StartSave = '',
      # -GameClock: the checkpoints, splits and the total in GAME seconds (debug `gameclock`, FLvibed's sum of the
      # game's frame dt) instead of wall seconds - for legs under the test aid `fixeddt` (WORK_QUEUE Q3 probe 4: the
      # game runs 2-4x faster than the wall clock). The fail-state windows, the timeout and the stall watchdog stay on
      # wall time (under fixeddt they only get more lenient in game time).
      [switch]$GameClock)
# -ReplayLog <leg folder>: no game - the folder's FLController.log is fed through the live checkpoint check (its clock
# from the transcript's leg start) and the splits / verdict printed: the live code checked against `splits.py replay`.
. (Join-Path $PSScriptRoot '..\flctl.ps1')
# Save fit (docs\TESTING.md "The test loop" 4; the user, 24 Oct 2026: "a sanity check on if specific saves created for the
# purposes of testing checkpoints within a speedrun are actually representative of what the autopilot is actually
# supposed to do"): a -SaveAt save records the ship's state at the save in its scene_saves.tsv row; the first run from it
# (-StartCp) compares the state after the load and writes the verdict fit / unfit (fit_note, fit_date); the set drivers
# refuse an unfit save (tools\flset.ps1 -Force). The catalogue keeps these columns through every harness write from this
# script (Set-FLScenes writes $script:FLSceneHead).
$script:FLSceneFit = 'speed', 'cruise', 'hull', 'shield', 'nanobots', 'batteries', 'objective_text', 'fit', 'fit_note', 'fit_date', 'marks'
$script:FLSceneHead = @(@($script:FLSceneHead) + $script:FLSceneFit | Select-Object -Unique)
$t0 = Get-Date
$script:legStart = if ($ReplayLog) { 0 } else { Get-FLEventSeq }
$script:chainFrom = $script:legStart
$script:moneyAt = [datetime]::MinValue   # the last money step (a gate it could not clear is tried again after 60 s)
$script:legUntil = $Until   # by script scope: inside Complete-FLBaseStory a bare $Until is its own parameter (call-depth overflow, 22 Oct 2026)
$script:phases = [Collections.Generic.List[string]]::new()
# (CpNow is defined further down: a Step before it - the judged SAVE-FIT line of an already fit save, line ~98 - reads the
# wall clock; it failed "CpNow is not recognized" on every such run since the fit catalogue, and its line was lost)
function Step($m) {
    $s = if (Get-Command CpNow -CommandType Function -ErrorAction SilentlyContinue) { CpNow } else { ((Get-Date) - $t0).TotalSeconds }
    Write-Host ("{0:HH:mm:ss} +{1,5:N0} {2}" -f (Get-Date), $s, $m)
}
# Fail states (the user, 23 Oct 2026: "additional fail states for each leg, where certain events happening or certain
# prerequisites for success failing to happen fail the run immediately" - shorter test cycles): leg_failstates.tsv,
# rows for this leg (the label's M12 / M01a prefix) and for every leg (*). 'event': an event matching the pattern ends
# the leg FAILED at once; 'by': the pattern must be seen within secs of the anchor's first event (no anchor: of the leg
# start), checked live - the leg ends FAILED when that time has passed without it. Off with -NoFailStates.
$script:failRules = @()
$leg = if ($Label -match '^(M\d\d[ab]?)') { $Matches[1] } else { '' }
if (-not $NoFailStates) {
    $rf = Join-Path $PSScriptRoot 'leg_failstates.tsv'
    if (Test-Path $rf) {
        $script:failRules = @(Import-Csv $rf -Delimiter "`t" | Where-Object { $_.leg -eq '*' -or ($leg -and $_.leg -eq $leg) })
    }
}
$script:seenAt = @{}   # pattern -> the time it was first seen
# Checkpoints and splits (docs\TESTING.md "Speedrun splits"; the user, 24 Oct 2026): the leg's checkpoints with par,
# our PB / gold and the fail limits, generated by tools\splits.py into leg_checkpoints.tsv. Each checkpoint reached
# prints a SPLIT line and a 'cp' phase row; a checkpoint not reached by the last one's time + its segment limit (and, in
# a full leg - -Full: started from the leg's start save -, by its cumulative limit) ends the leg FAILED (CHECKPOINT ...)
# - a catastrophic mistake or delay. The same rules as `splits.py replay` (judge), so the history validates them:
# kill:<n> checkpoints are soft (the log names only the autopilot's own target's kills: an unseen one carries its limit
# to the next); an objective up to 5 s before the previous checkpoint still counts. A part run (from a checkpoint save)
# starts at the checkpoint whose objective the load shows first; its first segment is not timed.
$script:cps = @()
if ($leg -and -not $Chain) {
    $cf = Join-Path $PSScriptRoot 'leg_checkpoints.tsv'
    if (Test-Path $cf) { $script:cps = @(Import-Csv $cf -Delimiter "`t" | Where-Object { $_.leg -eq $leg }) }
}
$script:cpk = @{ k = 0; prevT = $null; prevSeq = -1; lim = 0.0; started = [bool]$Full; firstSeen = $false; ms0 = $null
                 seen = @{}; ev = [Collections.Generic.List[object]]::new(); marks = @{}; split = @(); killOff = 0; stop = $false }
if ($StartCp -and $script:cps.Count) {
    $i = -1
    for ($j = 0; $j -lt $script:cps.Count; $j++) { if ($script:cps[$j].cp -eq $StartCp) { $i = $j; break } }
    if ($i -lt 0) { throw "Leg: -StartCp $StartCp is no checkpoint of $leg" }
    $c = $script:cpk
    $c.k = $i + 1; $c.started = $true; $c.prevT = 0.0; $c.firstSeen = $true
    # kills before the save = the n of the last kill:<n> checkpoint at or before it (not the rows: kill:3, kill:6 legs)
    $kn = @($script:cps[0..$i] | Where-Object { $_.ours -like 'kill:*' } | ForEach-Object { [int]($_.ours -replace '^kill:', '') })
    $c.killOff = if ($kn.Count) { ($kn | Measure-Object -Maximum).Maximum } else { 0 }
    # the save's targets were marked by its load, before this leg's events (a replay: from the archived log)
    $marked = if ($ReplayLog) { Select-String -LiteralPath (Join-Path $ReplayLog 'FLController.log') -Pattern ' mark id=\d+ state=1' | ForEach-Object Line }
              else { Get-FLHistory ' mark id=\d+ state=1' }
    foreach ($l in $marked) { if ("$l" -match ' mark id=(\d+) state=1') { $c.marks[$Matches[1]] = $true } }
}
# The save-fit check of a run from a checkpoint save: its catalogue row; judged when its fit is unverified (or not yet
# recorded), else its verdict is only logged. FitLoad reads the state on the first frame in space (before the objective
# mode moves the ship), FitStory the objective once the load settled, FitFirst notes the first segment.
$script:fit = $null
if ($StartCp -and -not $ReplayLog) {
    $fr = @(Get-FLScenes | Where-Object { $_.source -eq 'leg-checkpoint' -and $_.mission -eq $leg -and $_.step -eq $StartCp })
    $fr = if ($StartSave) { $fr | Where-Object { $_.name -eq $StartSave } | Select-Object -Last 1 } else { $fr | Sort-Object date | Select-Object -Last 1 }
    if ($fr) {
        $script:fit = @{ row = $fr; judge = ("$($fr.fit)" -in '', 'unverified'); load = $null; at = $null; done = $false; checked = [datetime]::MinValue; first = '' }
        if (-not $script:fit.judge) { Step "SAVE-FIT $($fr.fit) (judged $($fr.fit_date); $($fr.name)): $($fr.fit_note)" }
    } else { Step "SAVE-FIT ?: no catalogued leg-checkpoint save of $leg $StartCp$(if ($StartSave) { " named $StartSave" }) - not judged" }
}
function ObjIds([object[]]$Lines) {
    # objective ids in event order (the obj=<ids> field); the story's step = the last one other than 0 and 21660 "Awaiting"
    @($Lines | ForEach-Object { if ("$_" -match ' objective count=\d+ .* obj=(\d+) ') { $Matches[1] } })
}
function SaveState {
    # the ship now, from the harness: speed (debug `ship`, kinematics), cruise (`pilot`, Get-FLPilot), hull / shield as
    # fractions and the nanobots / batteries aboard (debug `ai self`, core\game\ai.cpp; else the latest per-second `self` event)
    $s = [ordered]@{ speed = ''; cruise = ''; hull = ''; shield = ''; nanobots = ''; batteries = '' }
    if ((Send-FL 'ship') -match 'speed=(-?[\d.]+)') { $s.speed = '{0:0.0}' -f [double]$Matches[1] }
    $p = Get-FLPilot
    if ($p) { $s.cruise = "$([int]$p.Cruise)" }
    $a = "$(Send-FL 'ai self')"
    if ($a -match 'hull=(-?[\d.]+) hull_max=\S+ shield=(-?[\d.]+) .*nanobots=(\d+)/\d+ batteries=(\d+)/') {
        $s.hull, $s.shield, $s.nanobots, $s.batteries = $Matches[1], $Matches[2], $Matches[3], $Matches[4]
    } elseif ("$(Get-FLHistory ' self hull=' | Select-Object -Last 1)" -match 'self hull=(\S+) shield=(\S+) nano=(\d+) batt=(\d+)') {
        $s.hull, $s.shield, $s.nanobots, $s.batteries = $Matches[1], $Matches[2], $Matches[3], $Matches[4]
    }
    $s
}
function Set-SaveFit([string]$Name, $Values) {
    # one catalogue row's fit columns, written atomically (a temp file, then a move: the set drivers read the catalogue
    # at every run's start); every other row and the column order kept, missing fit columns added at the end
    $f = $script:FLSceneCatalogue
    $lock = Enter-FLSceneLock   # (flctl.saves.ps1: parallel legs write the same catalogue)
    try {
    for ($try = 1; $try -le 5; $try++) {
        try {
            $lines = [IO.File]::ReadAllLines($f)
            $head = [Collections.Generic.List[string]]@($lines[0] -split "`t")
            foreach ($col in $script:FLSceneFit) { if (-not $head.Contains($col)) { $head.Add($col) } }
            $iName, $iMis, $iSrc = $head.IndexOf('name'), $head.IndexOf('mission'), $head.IndexOf('source')
            $out = [Collections.Generic.List[string]]::new(); $out.Add($head -join "`t")
            $hit = 0
            foreach ($l in ($lines | Select-Object -Skip 1)) {
                if (-not $l) { continue }
                $v = [Collections.Generic.List[string]]@($l -split "`t")
                if ($v[$iName] -eq $Name -and $v[$iMis] -eq $leg -and $v[$iSrc] -eq 'leg-checkpoint') {
                    while ($v.Count -lt $head.Count) { $v.Add('') }
                    foreach ($k in $Values.Keys) { $v[$head.IndexOf($k)] = "$($Values[$k])" -replace "[`t`r`n]", ' ' }
                    $l = $v -join "`t"; $hit++
                }
                $out.Add($l)
            }
            if (-not $hit) { Step "SAVE-FIT: no catalogue row $Name ($leg, leg-checkpoint) to write"; return }
            $tmp = "$f.$PID.tmp"
            [IO.File]::WriteAllLines($tmp, $out, [Text.UTF8Encoding]::new($false))
            Move-Item -LiteralPath $tmp -Destination $f -Force
            return
        } catch {
            if ($try -eq 5) { Step "SAVE-FIT: catalogue write failed: $_" } else { Start-Sleep -Milliseconds 200 }
        }
    }
    } finally { Exit-FLSceneLock $lock }
}
function FitLoad {
    $script:fit.load = SaveState; $script:fit.at = Get-Date
    $l = $script:fit.load
    Step "SAVE-FIT load state: speed $($l.speed) cruise $($l.cruise) hull $($l.hull) shield $($l.shield) nano $($l.nanobots) batt $($l.batteries)"
}
function FitStory {
    # the objective the load shows: settled once a step objective (not 0 / 21660) is seen, or 10 s after the first frame in space
    $x = $script:fit
    if ($x.done -or -not $x.load -or ((Get-Date) - $x.checked).TotalSeconds -lt 1) { return }
    $x.checked = Get-Date
    $ids = @(ObjIds (Get-FLHistory ' objective count=') | Where-Object { $_ -ne '0' })
    $loaded = if ($ids.Count) { $ids[-1] } else { '' }
    if (($loaded -in '', '21660') -and ((Get-Date) - $x.at).TotalSeconds -lt 10) { return }
    # (c) marks: a save made with mission marks up gets them back with its objective (S416, 30 Oct 2026 evening: 4
    # generator marks at the save, none after the load - the autopilot had no target for 160 s); 5 s for them to arrive
    $r = $x.row
    $wantMarks = if ("$($r.marks)" -match '^\d+$') { [int]$r.marks } else { 0 }
    $gotMarks = if ($wantMarks) { @(Get-FLHistory ' mark id=\d+ state=1').Count } else { 0 }
    if ($wantMarks -and -not $gotMarks -and ((Get-Date) - $x.at).TotalSeconds -lt 5) { return }
    $x.done = $true
    $l = $x.load
    $why = @()
    if ($wantMarks -and -not $gotMarks) {
        $why += "saved with $wantMarks mission mark(s) up, none after the load"
        $x.story = $true   # (no target for the step: Result ends the run as HARNESS)
    }
    # (a) physical: saved moving (above ~50 m/s or in cruise), loads at rest or below half that speed
    $sv = if ("$($r.speed)" -match '^-?[\d.]+$') { [double]$r.speed } else { $null }
    $lv = if ($l.speed -match '^-?[\d.]+$') { [double]$l.speed } else { $null }
    $sc = "$($r.cruise)" -eq '1'
    if ($null -ne $sv -and $null -ne $lv -and ($sv -gt 50 -or $sc) -and ($lv -lt 0.5 * $sv -or ($sc -and $l.cruise -eq '0'))) {
        $why += "saved at {0:0} m/s{1}, loads at {2:0} m/s" -f $sv, $(if ($sc) { ' (cruise)' } else { '' }), $lv
    }
    # (b) story: the step objective recorded at the save against the one shown after the load (21660 "Awaiting": the
    # script goals then offer only destroy goals - mission.cpp byTitle)
    $so = if ("$($r.objective_text)") { "$($r.objective_text)" } elseif ("$($r.objective)" -notin '', '0', '21660') { "$($r.objective)" } else { '' }
    if ($loaded -eq '21660' -or ($so -and $loaded -ne $so)) {
        $why += "objective $(if ($so) { $so } else { '?' }) -> $(if ($loaded) { $loaded } else { 'none' })$(if ($loaded -eq '21660') { ' (Awaiting)' }) after the load"
        $x.story = $true   # the load lost the story step: Result ends the run (a harness failure, never a result)
    }
    # (two stages since 30 Oct 2026 evening: a load that passes is 'fit-load' - SaveS409 loaded right and then hung in
    # the sphere entry's letterbox; 'fit' only once a run from it reached its first checkpoint, FitFirst)
    $fit = if ($why.Count) { 'unfit' } else { 'fit-load' }
    $note = if ($why.Count) { $why -join '; ' } else {
        "loads at $($l.speed) m/s$(if ($l.cruise -eq '1') { ' (cruise)' }), objective $loaded" +
            "$(if ($l.hull) { ", hull $($l.hull) shield $($l.shield)" })$(if ($null -ne $sv) { " (saved at $($r.speed) m/s)" })"
    }
    if ($x.first) { $note += "; $($x.first)" }
    $x.note = $note; $x.fit = $fit
    Set-SaveFit $r.name ([ordered]@{ fit = $fit; fit_note = "$note ($Label, $(Get-Date -Format 'dd MMM yyyy'))"; fit_date = (Get-Date -Format 'yyyy-MM-dd HH:mm') })
    Step "SAVE-FIT ${fit}: $note ($($r.name))"
}
function FitFirst([string]$Cp, [double]$Seg) {
    # the run's first segment against the spread of it in the last 10 full PASS legs (splits.tsv, in run order; a note
    # only - never the verdict)
    $x = $script:fit
    if (-not $x -or $x.first) { return }
    $xs = [Collections.Generic.List[double]]::new()
    $hf = Join-Path $script:FLRoot 'research\experience\legs\splits.tsv'
    if (Test-Path -LiteralPath $hf) {
        foreach ($ln in [IO.File]::ReadLines($hf)) {
            if (-not $ln.Contains("`t$Cp`t")) { continue }
            $v = $ln -split "`t"   # date label save start outcome secs commit dir cp n at seg
            if ($v.Count -ge 12 -and $v[3] -eq 'full' -and $v[4] -eq 'PASS' -and $v[8] -eq $Cp -and $v[11] -match '^[\d.]+$' -and $v[1] -like "$leg*") { $xs.Add([double]$v[11]) }
        }
    }
    $xs = @($xs | Select-Object -Last 10)
    $x.first = if ($xs.Count) { "first segment {0:N1} s vs full runs {1:N1}..{2:N1} (last {3})" -f $Seg, ($xs | Measure-Object -Minimum).Minimum, ($xs | Measure-Object -Maximum).Maximum, $xs.Count }
               else { "first segment {0:N1} s (no full PASS run times $Cp)" -f $Seg }
    Step "SAVE-FIT $($x.first)"
    if ($x.done -and $x.judge) {
        $x.note += "; $($x.first)"
        $vals = [ordered]@{ fit_note = "$($x.note) ($Label, $(Get-Date -Format 'dd MMM yyyy'))" }
        if ($x.fit -eq 'fit-load') { $x.fit = 'fit'; $vals.fit = 'fit'; $vals.fit_date = (Get-Date -Format 'yyyy-MM-dd HH:mm'); Step "SAVE-FIT fit: the first checkpoint ($Cp) passed after the load ($($x.row.name))" }
        Set-SaveFit $x.row.name $vals
    }
}
function CpTime([double]$ms) {
    # the event's own tick (ms) -> seconds since the leg start: anchored on the first event seen. -GameClock: the game
    # clock when the harness reads the event (the events carry wall ticks only: up to one poll late, < 1 s)
    if ($null -ne $script:g0) { return (CpNow) }
    if ($null -eq $script:cpk.ms0) { $script:cpk.ms0 = (CpNow) - $ms / 1000.0 }
    $ms / 1000.0 + $script:cpk.ms0
}
$script:replayNow = $null
$script:savePending = $null   # a -SaveAt save waiting for the ship at rest (CpSave)
$script:liveObj = ''          # the objective shown now, 21660 "Awaiting" included (a save there loads story-unfit: S404 / S405 / S417)
$script:liveMarks = @{}       # the mission marks up now (a save with marks up must get them back after its load: S416)
$script:liveSolars = @{}      # the mission solars spawned and not destroyed (a save keeps only those whose [MsnSolar] system = the player's: S416)
function GameSec { if ((Send-FL 'gameclock') -match '^gameclock (\d+(\.\d+)?)') { [double]$Matches[1] } else { $null } }
$script:g0 = if ($GameClock -and -not $ReplayLog) { GameSec } else { $null }
if ($GameClock -and -not $ReplayLog -and $null -eq $script:g0) { throw '-GameClock: the game does not answer `gameclock` (a build before it?)' }
$script:gLast = 0.0
function CpNow {
    if ($null -ne $script:replayNow) { return $script:replayNow }
    if ($null -ne $script:g0) {
        # (a missed answer keeps the last reading: the clock never runs backwards)
        $g = GameSec
        if ($null -ne $g -and $g - $script:g0 -ge $script:gLast) { $script:gLast = $g - $script:g0 }
        return $script:gLast
    }
    ((Get-Date) - $t0).TotalSeconds
}
# the splits' / phases' / total's clock: game seconds under -GameClock, else wall seconds as always (a replay too)
function LegNow { if ($null -ne $script:g0) { CpNow } else { ((Get-Date) - $t0).TotalSeconds } }
function CpEvent([string]$line) {
    if (-not $script:cps.Count) { return }
    $p = $line -split ' ', 3
    if ($p.Count -lt 3 -or $script:cpk.seen.ContainsKey($p[0])) { return }
    $script:cpk.seen[$p[0]] = $true
    $t = CpTime ([double]$p[1])
    $x = $p[2]
    $e = $null
    if ($x -match '^objective .* obj=(\d+) ') { $e = @{ t = $t; kind = 'obj'; val = $Matches[1] }; if ($Matches[1] -ne '0') { $script:liveObj = $Matches[1] } }
    elseif ($x -match '^mode (\S+) -> (\S+)') {
        $a, $b = $Matches[1], $Matches[2]
        if ($b -eq 'docked' -and $a -ne 'menu') { $e = @{ t = $t; kind = 'land' } }
        elseif ($a -eq 'docked') { $e = @{ t = $t; kind = 'launch' } }
        elseif ($a -eq 'space' -and $b -eq 'cutscene') { $e = @{ t = $t; kind = 'cutscene' } }
        if ($b -in 'space', 'space-ui' -and $a -in 'cutscene', 'docked') { $script:cpk.ev.Add(@{ t = $t; kind = 'control' }) }
    }
    elseif ($x -match '^mark id=(\d+) state=(\d)') {
        if ($Matches[2] -eq '1') { $script:cpk.marks[$Matches[1]] = $true; $script:liveMarks[$Matches[1]] = $true } else { $script:liveMarks.Remove($Matches[1]) }
    }
    elseif ($x -match 'target-destroyed id=(\d+)' -and $script:cpk.marks.ContainsKey($Matches[1]) -and [double]$Matches[1] -gt 100000) {
        $e = @{ t = $t; kind = 'kill' }
    }
    elseif ($x -match 'mission-message type=SUCCESS') { $e = @{ t = $t; kind = 'success' } }
    elseif ($x -match '^mission-save ids=(\d+) ') { $e = @{ t = $t; kind = 'save'; val = $Matches[1] } }   # save:<ids> (M01b.end at GAME SAVED)
    if ($x -match '^spawn-solar id=(\d+)') { $script:liveSolars[$Matches[1]] = $true }
    if ($x -match 'target-destroyed id=(\d+)') { $script:liveMarks.Remove($Matches[1]); $script:liveSolars.Remove($Matches[1]) }   # (a destroyed mark is down)
    if ($e) { $script:cpk.ev.Add($e) }
    CpAdvance
}
function CpAdvance {
    $c = $script:cpk
    while ($c.k -lt $script:cps.Count) {
        if (-not $c.started) {
            # a part run: the checkpoint of the first objective shown
            $o = $c.ev | Where-Object { $_.kind -eq 'obj' -and $_.val -notin '0', '21660' } | Select-Object -First 1
            if (-not $o) { return }
            $i = -1
            for ($j = 0; $j -lt $script:cps.Count; $j++) { if ($script:cps[$j].ours -eq "obj:$($o.val)") { $i = $j; break } }
            if ($i -lt 0) { $script:cps = @(); Step "checkpoints: objective $($o.val) is no checkpoint - no splits"; return }
            $c.k = $i; $c.started = $true
        }
        $cp = $script:cps[$c.k]
        $kind, $val = ($cp.ours -split ':', 2)
        $hit = -1
        if ($kind -eq 'kill') {
            $n = 0
            for ($j = 0; $j -lt $c.ev.Count; $j++) { if ($c.ev[$j].kind -eq 'kill' -and ++$n -eq [int]$val - $c.killOff) { $hit = $j; break } }
        } else {
            for ($j = 0; $j -lt $c.ev.Count; $j++) {
                if ($j -le $c.prevSeq -and -not ($null -ne $c.prevT -and $c.ev[$j].t -ge $c.prevT - 5)) { continue }
                if ($j -eq $c.prevSeq) { continue }
                if ($c.ev[$j].kind -eq $kind -and ($kind -notin 'obj', 'save' -or $c.ev[$j].val -eq $val)) { $hit = $j; break }
            }
        }
        if ($hit -lt 0) {
            # a hard obj checkpoint whose objective the script skipped (M05: 22867 posted in the jump tunnel, 22870
            # shown): since the previous checkpoint the story showed a later checkpoint's objective, or one whose text
            # id lies between this checkpoint's and the next obj checkpoint's (ids are not in story order across a
            # leg: M11.ishole 24125 comes before .satellite 24055) - soft, its limit carried over (splits.py
            # our_splits / judge: the same rule)
            if ($kind -eq 'obj') {
                $rest = @($script:cps | Select-Object -Skip ($c.k + 1) | Where-Object { $_.ours -like 'obj:*' } | ForEach-Object { $_.ours.Substring(4) })
                $later = $null
                for ($j = $c.prevSeq + 1; $j -lt $c.ev.Count; $j++) {
                    $e = $c.ev[$j]
                    if ($e.kind -ne 'obj' -or $e.val -in '0', '21660') { continue }
                    if ($e.val -in $rest -or ($rest.Count -and [long]$val -lt [long]$e.val -and [long]$e.val -lt [long]$rest[0])) { $later = $e; break }
                }
                if ($later) {
                    $c.lim += [double]$cp.seg_limit; $c.k++
                    Step "checkpoint $($cp.cp) skipped: objective $($later.val) shown after it (soft: carried over)"
                    continue
                }
            }
            return
        }
        $t = $c.ev[$hit].t
        if ($null -ne $c.prevT -and $t -lt $c.prevT) { $t = $c.prevT }
        $seg = if ($null -ne $c.prevT -and $c.firstSeen) { $t - $c.prevT } elseif ($Full -and $c.k -eq 0) { $t } else { $null }
        $par = [double]$cp.par
        $pace = if (-not $Full) { '' } elseif ($t -le $par) { 'AHEAD' } elseif ($t - $par -le [Math]::Max(10, 0.05 * $par)) { 'ok' } else { 'BEHIND' }
        $gold = if ($cp.gold) { [double]$cp.gold } else { $null }
        $msg = "SPLIT $($cp.cp) at {0:N1}" -f $t
        if ($null -ne $seg) { $msg += " seg {0:N1} (par {1:N1}" -f $seg, [double]$cp.par_seg; if ($null -ne $gold) { $msg += ", gold {0:N1}{1}" -f $gold, $(if ($seg -lt $gold) { ' NEW GOLD' } else { '' }) }; $msg += ')' }
        if ($Full) { $msg += " {0:+0.0;-0.0} vs par {1}" -f ($t - $par), $pace; if ($cp.pb) { $msg += " {0:+0.0;-0.0} vs PB" -f ($t - [double]$cp.pb) } }
        Step $msg
        if ($script:fit -and $null -ne $seg -and $null -ne $c.prevT -and $c.prevT -eq 0.0 -and $null -eq $script:replayNow) { FitFirst $cp.cp $seg }
        $script:phases.Add(("cp`t{0:N1}`t{1}`t{2}" -f $t, $(if ($null -ne $seg) { '{0:N1}' -f $seg } else { '' }), $cp.cp))
        $c.prevT = $t; $c.prevSeq = $hit; $c.lim = 0.0; $c.firstSeen = $true; $c.k++
        if ($cp.cp -in $SaveAt -and $null -eq $script:replayNow) { CpSave $cp.cp }
        if ($StopAt -and $cp.cp -eq $StopAt) { $c.stop = $true; return }
    }
}
function CpObjective {
    # the story's step now: this leg's last objective event other than 0 and 21660 "Awaiting" ('' none yet)
    $ob = @($script:cpk.ev | Where-Object { $_.kind -eq 'obj' -and $_.val -notin '0', '21660' } | ForEach-Object { $_.val })
    if ($ob.Count) { $ob[-1] } else { '' }
}
function CpSave([string]$name) {
    # A save taken moving is unfit (a load starts at rest; 30 Oct 2026: 9 of 10 kcs / jrs saves). Moving at the checkpoint
    # -> the save waits for the first moment at rest (<= 30 m/s, no cruise) on the same story step, 90 s at most
    # (CpSavePending, polled from Result); the run itself is not slowed or steered for it.
    if ($script:savePending) { Step "SAVE $($script:savePending.name) dropped: checkpoint $name came first"; $script:savePending = $null }
    $st = SaveState
    $sv = if ("$($st.speed)" -match '^-?[\d.]+$') { [double]$st.speed } else { 0 }
    # (nor while the objective reads 21660 "Awaiting": such a save loads story-unfit - S404 / S405, and S417 deferred into it)
    if ($sv -le 50 -and "$($st.cruise)" -ne '1' -and $script:liveObj -ne '21660') { CpSaveNow $name; return }
    $script:savePending = @{ name = $name; obj = (CpObjective); at = (Get-Date); next = (Get-Date) }
    Step ("SAVE $name deferred: {0:0} m/s{1}{3} at the checkpoint, waits for rest on objective {2} (90 s at most)" -f $sv, $(if ("$($st.cruise)" -eq '1') { ' cruise' } else { '' }), $script:savePending.obj, $(if ($script:liveObj -eq '21660') { ', Awaiting' } else { '' }))
}
function CpSavePending {
    $p = $script:savePending
    if (-not $p -or (Get-Date) -lt $p.next) { return }
    $p.next = (Get-Date).AddSeconds(1)
    $wait = ((Get-Date) - $p.at).TotalSeconds
    if ((CpObjective) -ne $p.obj) { Step ("SAVE $($p.name) dropped: the step moved on ({0} -> {1}) before the ship came to rest ({2:0} s)" -f $p.obj, (CpObjective), $wait); $script:savePending = $null; return }
    if ($wait -gt 90) { Step "SAVE $($p.name) dropped: no rest within 90 s on objective $($p.obj)"; $script:savePending = $null; return }
    if ($script:liveObj -eq '21660') { return }   # (the step talks: "Awaiting" - wait for its objective or the timeout)
    $s = if ((Send-FL 'ship') -match 'speed=(-?[\d.]+)') { [double]$Matches[1] } else { 999 }
    $p0 = Get-FLPilot
    if ($s -le 30 -and -not ($p0 -and $p0.Cruise)) {
        $script:savePending = $null
        Step ("SAVE $($p.name) at rest {0:0.0} s after the checkpoint" -f $wait)
        CpSaveNow $p.name
    }
}
function LostSolars {
    # the mission solars up now that a save made now would not keep: no [MsnSolar] definition of theirs has the player's
    # system as its `system =` (Get-FLMissionSolars; an id no script names is no mission solar), and the game still lists
    # it (`objobj <id>` by-id: a solar killed by a fuse or an NPC - M05's Wotan - leaves no target-destroyed event)
    $p = Get-FLPilot
    if (-not $p -or -not $p.System) { return }   # (the player's system unread: no verdict, never a false unfit)
    $sys = [uint32]$p.System
    $defs = Get-FLMissionSolars
    foreach ($id in @($script:liveSolars.Keys)) {
        $d = $defs[$id]
        if (-not $d -or $d.systems.Contains($sys)) { continue }
        if ("$(Send-FL "objobj $id")" -notmatch ' by-id=(\d+)' -or $Matches[1] -eq '0') { $script:liveSolars.Remove($id); continue }
        [pscustomobject]@{ id = $id; nick = $d.nick }
    }
}
function CpSaveNow([string]$name) {
    # a native save in flight (server.dll pub::Save, the missions' Act_Save) - its cost: the call's wall time and the
    # frames the game drew meanwhile, the ship's speed before and after (the game stalls while it writes). The ship's
    # state at the save (SaveState, read just before it) and the story's step objective go into the save's catalogue
    # row (fit unverified: the first run from it judges it - the save-fit check above).
    try {
        $speed = { if ((Send-FL 'ship') -match 'speed=(\S+)') { $Matches[1] } else { '?' } }
        $st = SaveState
        $f0 = Get-FLFrame; $v0 = if ($st.speed) { $st.speed } else { '?' }; $sw = [Diagnostics.Stopwatch]::StartNew()
        $row = Save-FLScene $leg $name $Label 'leg-checkpoint'
        $ms = $sw.ElapsedMilliseconds; $f1 = Get-FLFrame; $v1 = & $speed
        Step ("SAVE $name -> $($row.name) ({0} ms, {1} frames, speed $v0 -> $v1)" -f $ms, ($f1 - $f0))
        # the step objective: this leg's objective events up to the checkpoint, else the session's history
        $ob = @($script:cpk.ev | Where-Object { $_.kind -eq 'obj' -and $_.val -notin '0', '21660' } | ForEach-Object { $_.val })
        if (-not $ob.Count) { $ob = @(ObjIds (Get-FLHistory ' objective count=') | Where-Object { $_ -notin '0', '21660' }) }
        $vals = [ordered]@{}
        foreach ($k in $st.Keys) { $vals[$k] = $st[$k] }
        $vals.objective_text = if ($ob.Count) { $ob[-1] } else { '' }
        $vals.marks = "$($script:liveMarks.Count)"   # (the mission marks up at the save: FitStory wants them back after the load)
        $vals.fit = 'unverified'; $vals.fit_note = ''; $vals.fit_date = ''
        # a load always starts at rest (FREELANCER_RE "Saves"): a save made above ~50 m/s or in cruise is unfit at once,
        # the same rule FitStory applies on its first load (30 Oct 2026: 5 of the kcs / jrs saves, 3 runs spent on them)
        $sv = if ("$($st.speed)" -match '^-?[\d.]+$') { [double]$st.speed } else { 0 }
        if ($sv -gt 50 -or "$($st.cruise)" -eq '1') {
            $vals.fit = 'unfit'; $vals.fit_date = (Get-Date -Format 'yyyy-MM-dd HH:mm')
            $vals.fit_note = "saved at {0:0} m/s{1}, a load starts at rest (at the save, $Label)" -f $sv, $(if ("$($st.cruise)" -eq '1') { ' (cruise)' } else { '' })
        }
        # a marked mission solar the save does not keep ([MsnSolar] without `system =` or another system than the
        # player's: Content.dll's [MsnSolarSave] writer) is not re-created by the load - the step's target is gone (S416:
        # M09's generators; FREELANCER_RE "Saves")
        $lost = @(LostSolars | Where-Object { $script:liveMarks.ContainsKey($_.id) })
        if ($lost.Count) {
            $vals.fit = 'unfit'; $vals.fit_date = (Get-Date -Format 'yyyy-MM-dd HH:mm')
            $why = "saved with $($lost.Count) marked mission solar(s) a save does not keep ($(($lost | ForEach-Object { $_.nick }) -join ', ')): the load drops them (at the save, $Label)"
            $vals.fit_note = if ($vals.fit_note) { "$($vals.fit_note); $why" } else { $why }
            Step "SAVE $name unfit: $why"
        }
        Set-SaveFit $row.name $vals
        Step "SAVE $name state: speed $($st.speed) cruise $($st.cruise) hull $($st.hull) shield $($st.shield) nano $($st.nanobots) batt $($st.batteries) objective $($vals.objective_text)"
    } catch { Step "SAVE $name failed: $_" }
}
function CpDeadline {
    $c = $script:cpk
    if ($NoFailStates -or -not $script:cps.Count -or -not $c.started -or $c.k -ge $script:cps.Count) { return $null }
    $now = CpNow
    while ($c.k -lt $script:cps.Count) {
        $cp = $script:cps[$c.k]
        $soft = $cp.ours -like 'kill:*'
        $base = if ($null -ne $c.prevT) { $c.prevT } elseif ($Full) { 0.0 } else { return $null }
        $due = $base + $c.lim + [double]$cp.seg_limit
        if ($now -gt $due) {
            if ($soft) {
                $c.lim += [double]$cp.seg_limit; Step "checkpoint $($cp.cp) unseen by its limit (soft: carried over)"; $c.k++
                CpAdvance   # a later checkpoint may have come already (M12: the core entered while gen 3's kill went unlogged)
                if ($c.k -ge $script:cps.Count) { return $null }
                continue
            }
            return "FAIL STATE (CHECKPOINT $($cp.cp) not reached: segment over {0:N0} s, par {1:N1} s - $($cp.segment))" -f ($c.lim + [double]$cp.seg_limit), [double]$cp.par_seg
        }
        if ($Full -and -not $soft -and $now -gt [double]$cp.cum_limit) {
            return "FAIL STATE (CHECKPOINT $($cp.cp) not reached by {0:N0} s into the leg, par {1:N1} s)" -f [double]$cp.cum_limit, [double]$cp.par
        }
        return $null
    }
    $null
}
function FailState([string]$line) {
    foreach ($fr in $script:failRules) {
        foreach ($p in @($fr.pattern, $fr.anchor)) {
            if ($p -and $p.Trim() -and -not $script:seenAt.ContainsKey($p) -and $line.Contains($p)) { $script:seenAt[$p] = Get-Date }
        }
        if ($fr.kind -eq 'event' -and $line.Contains($fr.pattern)) { return "FAIL STATE ($($fr.why)): $line" }
    }
    return $null
}
function FailDeadline {
    foreach ($fr in $script:failRules) {
        if ($fr.kind -ne 'by' -or $script:seenAt.ContainsKey($fr.pattern)) { continue }
        $from = if ($fr.anchor -and $fr.anchor.Trim()) { $script:seenAt[$fr.anchor] } else { $t0 }
        if ($from -and ((Get-Date) - $from).TotalSeconds -gt [double]$fr.secs) { return "FAIL STATE ($($fr.why))" }
    }
    return $null
}
# The autopilot's restart loop (set ab11 i5, 31 Oct 2026, M13): every restart of the objective mode ended at once with
# the same reason - 36 times "player died" on a ship alive at a sliver of hull (the autopilot's old hull test) - while
# the ship flew on at full throttle, so the watchdog's 300 m kept seeing progress; the leg idled 2.5 min to the
# checkpoint limit. $script:apLoopN stops in a row at time 0 (no frame flown) with the same why end the leg AUTOPILOT
# LOOP (sticky: every later Result returns it; not 'STALL*', which would judge a fit-load save unfit - the loop is the
# autopilot's, not the save's). None of 1,100+ earlier leg logs has even 3 in a row; i5 ends 22 s after its false death
# instead of 150 s.
$script:apLoop = @{ seen = @{}; why = ''; n = 0; verdict = $null }
$script:apLoopN = 5
function ApLoop([string]$line) {
    if ($script:apLoop.verdict) { return $script:apLoop.verdict }
    $p = $line -split ' ', 3
    if ($p.Count -lt 3 -or $script:apLoop.seen.ContainsKey($p[0])) { return $null }
    $script:apLoop.seen[$p[0]] = $true
    if ($p[2] -notmatch '^autopilot stop mode=\S+ why=(.+?) kills=\S+ time=([\d.]+)') { return $null }
    $w, $secs = $Matches[1], [double]$Matches[2]
    if ($secs -ge 0.05) { $script:apLoop.why = ''; $script:apLoop.n = 0; return $null }
    if ($w -eq $script:apLoop.why) { $script:apLoop.n++ } else { $script:apLoop.why = $w; $script:apLoop.n = 1 }
    if ($script:apLoop.n -ge $script:apLoopN) {
        $script:apLoop.verdict = "AUTOPILOT LOOP ($($script:apLoop.n) stops in a row at start, why=$w)"
    }
    return $script:apLoop.verdict
}
function Result {
    if ($script:apLoop.verdict) { return $script:apLoop.verdict }
    if ($script:fit -and $script:fit.judge -and -not $script:fit.done) {
        try { FitStory } catch { Step "SAVE-FIT check failed: $_"; $script:fit.done = $true }
    }
    # A load that lost the story step (objective changed / "Awaiting") leaves the autopilot without its step: the run can
    # only drift to the stall limit (30 Oct 2026, M07-jrs2 from the Cali base save: 2 instances idle) - end it at once.
    # A save saved moving and loaded at rest still flies its story (its later checkpoints and saves stay usable).
    if ($script:fit -and $script:fit.done -and $script:fit.story) { return "HARNESS unfit save: $($script:fit.note)" }
    if ($script:savePending -and $null -eq $script:replayNow) { try { CpSavePending } catch { Step "SAVE pending check failed: $_"; $script:savePending = $null } }
    foreach ($l in (Get-FLEventLog $script:legStart)) {
        CpEvent "$l"
        if ($script:failRules.Count -and ($fs = FailState "$l")) { return $fs }
        if ("$l" -match 'mission-message type=FAILURE|player-death') { return "FAILED: $l" }
        if ($al = ApLoop "$l") { return $al }
        if ("$l" -match 'mission-message type=SUCCESS') {
            if (-not $Chain) { return 'SUCCESS' }
            if (@(Get-FLEventLog $script:chainFrom | Where-Object { "$_" -match 'title=24400 ' }).Count) { return 'SUCCESS' }
            # a split: the next mission's result counts from here
            $script:legStart = [long]("$l" -split ' ')[0] + 1
            $script:phases.Add(("split`t{0:N1}`t0`t{1}" -f (LegNow), "$l"))
            Step "SPLIT $l"
            return $null
        }
    }
    if ($script:failRules.Count -and ($fd = FailDeadline)) { return $fd }
    if ($cd = CpDeadline) { return $cd }
    if ($script:cpk.stop) { return 'UNTIL' }
    if (& $script:legUntil) { return 'UNTIL' }
}
function Phase([string]$kind, [datetime]$from, [string]$end) {
    $s = ((Get-Date) - $from).TotalSeconds
    $script:phases.Add(("{0}`t{1:N1}`t{2:N1}`t{3}" -f $kind, (LegNow), $s, $end))
}
if ($ReplayLog) {
    $tr = Get-Content -Raw -LiteralPath (Join-Path $ReplayLog 'transcript.txt')
    $m0 = [regex]::Match($tr, '(?m)^(\d\d):(\d\d):(\d\d) \+\s+0 ')
    $t0c = [int]$m0.Groups[1].Value * 3600 + [int]$m0.Groups[2].Value * 60 + [int]$m0.Groups[3].Value
    $script:cpk.ms0 = -$t0c   # the replayed events' "ms" are the log's clock in ms
    $n = 0
    $verdict = $null
    $ended = $null
    foreach ($ln in [IO.File]::ReadLines((Join-Path $ReplayLog 'FLController.log'))) {
        $mm = [regex]::Match($ln, '^\[(\d+):(\d+):(\d+)\.(\d+)\] event (.*)$')
        if (-not $mm.Success) { continue }
        $c = [int]$mm.Groups[1].Value * 3600 + [int]$mm.Groups[2].Value * 60 + [int]$mm.Groups[3].Value + [int]$mm.Groups[4].Value / 1000.0
        if ($c -lt $t0c - 43200) { $c += 86400 }
        if ($c -lt $t0c - 5) { continue }
        $script:replayNow = $c - $t0c
        if ($verdict = CpDeadline) { break }
        $ev = "{0} {1} {2}" -f (++$n), [long]($c * 1000), $mm.Groups[5].Value
        CpEvent $ev
        if ($verdict = ApLoop $ev) { break }   # the live Result's restart-loop check
        if ($mm.Groups[5].Value -match 'mission-message type=SUCCESS|mission-message type=FAILURE|player-death') { $ended = $mm.Groups[5].Value; break }
    }
    $at = '{0:N0}' -f $script:replayNow
    # (the game's result too, as the live Result reads it: FAILED: <event> / SUCCESS)
    if ($verdict) { "FAILED ($verdict) at $at s" }
    elseif ($ended -match 'type=SUCCESS') { "SUCCESS at $at s" }
    elseif ($ended) { "FAILED (FAILED: $ended) at $at s" }
    else { "no fail state (last event $at s)" }
    return
}
# every harness wait inside the leg throws OVER BUDGET at the budget, else at the time-out (29 Oct 2026: before, the
# time-out was checked only between phases, so a docked phase could overrun it by its own 600 s); a tighter deadline
# of the caller (a sweep section's) stays, and comes back after the leg
$outerDeadline = $global:FLDeadline
$limit = if ($BudgetSec -gt 0) { [Math]::Min($BudgetSec, $TimeoutSec) } else { $TimeoutSec }
if (-not $outerDeadline -or $outerDeadline.at -gt (Get-Date).AddSeconds($limit)) {
    Set-FLDeadline $limit $(if ($BudgetSec -gt 0 -and $BudgetSec -le $TimeoutSec) { "leg $Label budget" } else { "leg $Label time-out" })
}
$r = $null
$why = ''
try {
    while (-not $r) {
        if (((Get-Date) - $t0).TotalSeconds -gt $TimeoutSec) { $why = 'time-out'; break }
        $r = Result; if ($r) { break }
        $m = Get-FLMode
        $p0 = Get-Date
        if ($m -eq 'docked') {
            Step "base: $(((Send-FL 'hotspots') -split "`n")[-1])"
            # the objective mode armed in the base: it decides on the first frame in space (M13's launch parks the ship
            # among Nomads - 26 s of harness waits after it cost the hull, 22 Oct 2026)
            $null = Send-FL 'ap start arm'
            if ($Chain -and ($n = Get-FLWorthNeeded) -gt 0) { Step "money: needed $n - $(Invoke-FLMoneyPlan)" }
            $b = Complete-FLBaseStory -Until { [bool](Result) } -AcceptStory
            Phase 'base' $p0 $b
            Step "base -> $b"
        } elseif ($m -in 'space', 'space-ui') {
            Step 'space: objective mode'
            # a run from a checkpoint save: the state the load left, read before the objective mode moves the ship
            if ($script:fit -and $script:fit.judge -and -not $script:fit.load) { try { FitLoad } catch { Step "SAVE-FIT load state failed: $_"; $script:fit = $null } }
            if ((Send-FL 'ap status') -match ' objective=off\b') { $null = Send-FL 'ap start objective' }
            $box = @{ end = $null; off = $null; money = (Get-Date); wdAt = (Get-Date); wdSince = (Get-Date); wdPos = $null; wdKey = ''; evSeq = 0; talkUntil = $null }
            $null = Wait-FLUntil {
                $x = Result
                if ($x) { $box.end = $x; return $true }
                # Stall watchdog (user, 22 Oct 2026, high priority: "allowing tests to go on and stay stuck is
                # counter-productive and a time waster" - M12 held 3 km off a generator for 8 min until the clock failed
                # it). Every 5 s: progress = the ship moved 300 m, or the autopilot's kills / damage dealt / target /
                # objective action changed; none for $StallSec -> the leg ends STALL with its evidence.
                if ($StallSec -gt 0 -and ((Get-Date) - $box.wdAt).TotalSeconds -ge 5) {
                    $box.wdAt = Get-Date
                    $ap = Send-FL 'ap status'
                    $key = (@('kills', 'dealt', 'target', 'objective') | ForEach-Object { if ($ap -match " $_=(\S+)") { $Matches[1] } }) -join '|'
                    $pos = if ((Send-FL 'ship') -match 'pos=\((-?[\d.]+),(-?[\d.]+),(-?[\d.]+)\)') { @([double]$Matches[1], [double]$Matches[2], [double]$Matches[3]) }
                    $moved = $pos -and $box.wdPos -and ([Math]::Sqrt([Math]::Pow($pos[0] - $box.wdPos[0], 2) + [Math]::Pow($pos[1] - $box.wdPos[1], 2) + [Math]::Pow($pos[2] - $box.wdPos[2], 2)) -gt 300)
                    # ...or the script talks (session 8, 23 Oct 2026, M02: a 70 s scripted dialogue before the next objective,
                    # the ship rightly at rest - the watchdog ended a good leg STALL): a comm line or mission message since
                    $talk = $false
                    $ev = (Send-FL "events $([int]$box.evSeq)") -split "`n" | Where-Object { $_ }
                    $evLines = @($ev | Select-Object -Skip 1)
                    if ($evLines.Count) {
                        if ($box.evSeq) {
                            $talk = [bool]($evLines | Where-Object { $_ -match ' comm from=| mission-message ' })
                            # (a comm event of n lines plays ~6 s a line: M10's 7-line Bruchsal dock dialogue outlasted 60 s)
                            foreach ($l in $evLines) {
                                # the docking letterbox (exclusion 4): seconds when a dock / jump goes through, 78 s at most in
                                # a passing run (M12's scripted entry; 1,109 leg logs); held past 120 s it is a hang no comm
                                # line excuses (30 Oct 2026: M13 from SaveS409, the sphere entry
                                # stuck at 4 for 2+ min while Nomad chatter kept the watchdog quiet; the owner saw it)
                                if ($l -match ' control-exclusion \S+ -> (\S+)') {
                                    if ($Matches[1] -eq '4') { if (-not $box.dockExcl) { $box.dockExcl = Get-Date } } else { $box.dockExcl = $null }
                                }
                                if ($l -match ' comm from=.* lines=(\d+)') {
                                    $grace = (Get-Date).AddSeconds(6 * [int]$Matches[1])
                                    if (-not $box.talkUntil -or $grace -gt $box.talkUntil) { $box.talkUntil = $grace }
                                }
                            }
                        }
                        $box.evSeq = [int](($evLines[-1] -split ' ')[0])
                    }
                    if ($box.talkUntil -and (Get-Date) -lt $box.talkUntil) { $talk = $true }
                    if ($box.dockExcl -and ((Get-Date) - $box.dockExcl).TotalSeconds -gt 120) { $box.end = "STALL docking letterbox held 120 s (control-exclusion 4)"; return $true }
                    if (-not $box.wdPos -or $moved -or $talk -or $key -ne $box.wdKey) { $box.wdSince = Get-Date; $box.wdPos = $pos; $box.wdKey = $key }
                    elseif (((Get-Date) - $box.wdSince).TotalSeconds -gt $StallSec) { $box.end = "STALL no progress $StallSec s ($key)"; return $true }
                }
                # mission popups in space (M01b "Fly to the Waypoint" with OK: the game sat paused 12 min, 22 Oct 2026)
                $null = Close-FLBlocker
                $s = Send-FL 'state'
                if ($s -match 'mode=docked') { $box.end = 'docked'; return $true }
                # the chain between missions: a net-worth gate open in space (no mission runs then) -> the money step
                if ($Chain -and ((Get-Date) - $box.money).TotalSeconds -gt 5) {
                    $box.money = Get-Date
                    if (((Get-Date) - $script:moneyAt).TotalSeconds -gt 60 -and (Get-FLWorthNeeded) -gt 0) { $box.end = 'money'; return $true }
                }
                # the objective mode stopped by itself (an error, a hand-back): restart it. Its own state is the
                # status's objective= (its sub-mode reads mode=off between decisions and in its decide-loop pause: a
                # check on mode restarted it every 3-4 s in M01b); 3 s of grace still.
                if ($s -match 'mode=(space|space-ui)' -and (Send-FL 'ap status') -match ' objective=off\b') {
                    if (-not $box.off) { $box.off = Get-Date } elseif (((Get-Date) - $box.off).TotalSeconds -gt 3) { $box.end = 'autopilot off'; return $true }
                } else { $box.off = $null }
                $false
            } "leg $Label space" ($TimeoutSec - ((Get-Date) - $t0).TotalSeconds)
            $null = Send-FL 'ap stop'
            Phase 'space' $p0 $box.end
            Step "space -> $($box.end)"
            if ("$($box.end)" -like 'STALL*') { $why = $box.end; break }
            if ($box.end -eq 'autopilot off') { Start-Sleep 1 }
            if ($box.end -eq 'money') { $p1 = Get-Date; Step "money: $(Invoke-FLMoneyPlan)"; Phase 'money' $p1 'done'; $script:moneyAt = Get-Date }
        } else {
            # cutscene, loading, menus: wait it out (story cutscenes in space cannot be skipped)
            $null = Close-FLBlocker
            $null = Skip-FLAnimation
            Start-Sleep -Milliseconds 200
        }
    }
} catch { $why = "$_" -replace '\s+', ' ' } finally { $global:FLDeadline = $outerDeadline; $null = Send-FL 'ap stop' }
# a leg that ended before the load's objective settled: judged with what the load showed
if ($script:fit -and $script:fit.judge -and $script:fit.load -and -not $script:fit.done) {
    $script:fit.at = [datetime]::MinValue; $script:fit.checked = [datetime]::MinValue
    try { FitStory } catch { Step "SAVE-FIT check failed: $_" }
}
# a load judged 'fit-load' whose run stalled before its first checkpoint (no death, no story failure: the save's own
# state - SaveS409's letterbox hang) is unfit; a death leaves the verdict to the next run
if ($script:fit -and $script:fit.fit -eq 'fit-load' -and "$why" -like 'STALL*') {
    try {
        Set-SaveFit $script:fit.row.name ([ordered]@{ fit = 'unfit'; fit_note = "loads right, then $why before the first checkpoint ($Label, $(Get-Date -Format 'dd MMM yyyy'))"; fit_date = (Get-Date -Format 'yyyy-MM-dd HH:mm') })
        Step "SAVE-FIT unfit: $why before the first checkpoint after the load ($($script:fit.row.name))"
    } catch { Step "SAVE-FIT stall verdict failed: $_" }
}
$total = [int](LegNow)
if ($Out) { Set-Content -LiteralPath $Out -Value (@("phase`tat`tsecs`tend") + $script:phases) }
if ($r -eq 'SUCCESS' -or $r -eq 'UNTIL') { "$r total $total s" }
else { "FAILED ($(if ($r) { $r } else { $why })) total $total s; $(Get-FLStoryState)" }
