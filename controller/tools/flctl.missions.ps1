# FLController harness - the mission event log, objectives, mission fights, dialogue, plan B, stalls, jobs. Part of flctl.ps1 (dot-sourced by it; never load a part alone).

# Mission event log: every event from Start-FLEventLog on, read incrementally with its own cursor (a page at a time
# until caught up; the debug channel caps one reply, so asking "everything since the mission start" lost the newest
# events in long missions - M02 step 12, 18 Oct 2026). Waits search the accumulated log, so an event that came while
# the script was doing something else (an objective during a lane ride) is still seen. Returns the start sequence.
$script:FLLog = [System.Collections.Generic.List[string]]::new()
$script:FLLogSeq = -1
function Start-FLEventLog {
    $script:FLLog.Clear()
    $script:FLLogSeq = [int]([regex]::Match((Send-FL 'events 999999999'), '^last (\d+)').Groups[1].Value)
    $script:FLLogSeq
}
function Update-FLEventLog {
    if ($script:FLLogSeq -lt 0) { $null = Start-FLEventLog }
    for ($page = 0; $page -lt 100; $page++) {
        $lines = @((Send-FL "events $($script:FLLogSeq)") -split "`n" | Where-Object { $_ } | Select-Object -Skip 1)
        if (-not $lines.Count) { break }
        foreach ($l in $lines) { $script:FLLog.Add($l) }
        $script:FLLogSeq = [int](($lines[-1] -split ' ')[0])
    }
}
# The logged events after sequence $After.
function Get-FLEventLog([int]$After = 0) {
    Update-FLEventLog
    $script:FLLog | Where-Object { [int](($_ -split ' ')[0]) -gt $After }
}

# Waits for a mission step objective (the objective event's obj=<ids>; $Ids a regex alternation, e.g. '22025|22045')
# logged after event $After (the mission event log: Start-FLEventLog), closing story popups meanwhile. Throws on a
# mission failure or the player's death. Returns the id seen.
function Wait-FLObjective([string]$Ids, [int]$After, [int]$TimeoutSec = 300, [switch]$NoPlanB) {
    $st = @{ hit = $null }
    $null = Wait-FLUntil {
        $null = Close-FLBlocker
        foreach ($l in (Get-FLEventLog $After)) {
            if ("$l" -match 'mission-message type=FAILURE|player-death') { throw "mission failed: $l" }
            if ("$l" -match "objective .*obj=($Ids) ") { $st.hit = $Matches[1] }
        }
        if (-not $st.hit -and -not $NoPlanB) {
            Update-FLPlanB $After   # plan B alongside (acts on idle / disagreement)
            Update-FLStall $After   # no progress for a while: analyse and act
        }
        if (-not $st.hit) { Update-FLIdlePark }   # nothing flies the ship: park it, nose on something
        $st.hit
    } "objective $Ids" $TimeoutSec
    # No dialogue buffer here (19 Oct 2026): players act while the radio talks - fight, scan, follow. Waiting after
    # every objective held the ship 46 s at M02's scan while the mission moved on (the scan then waited for an
    # objective that had already come, Cosmo attacking) and very likely cost M01b's prison-ship fight. The buffer
    # belongs only before actions that end the space scene and its comms: Start-FLDock, Start-FLJump, docking travels.
    # A popup set in the objective's own tick (M01a 21875 + popup 21855, 18 Oct 2026) pauses the game: close it before
    # the caller acts (targeting failed under it). The popup can land a frame after the objective event.
    $null = Wait-FLFramesPassed 3
    for ($i = 0; $i -lt 5 -and (Close-FLBlocker); $i++) { $null = Wait-FLFramesPassed 2 }
    $st.hit
}

# A mission fight until the step objective changes to one of $UntilIds (logged after event $After): the combat
# autopilot, started once the mission's maneuver lock is off (a formation the mission flies is locked), and again when
# it has stopped ("no enemies in reach") and the hostiles around have changed since (a new wave) - a state condition,
# not a retry loop (was: restarted every 170 ms while far hostiles were listed, M02). Throws on failure / death.
# Verified in M01b (18 Oct 2026). Returns the objective id reached.
# Combat for a fight the mission asks for ("defend", "destroy"): the whole battle, not the autopilot's default 3 km -
# the reach covers the nearest hostile + 1.5 km (3-15 km; 15 km = the battle range the autopilot uses for hostiles
# fighting our side), and the fight entry cruises in (user, 18 Oct 2026: at Pueblo "Help defend" left the ship idle -
# the attackers were beyond 3 km). Returns the reach set, 0 when no hostile is in the battle range.
function Start-FLMissionCombat([double]$MaxReach = 15000) {
    $near = Get-FLContacts -Hostile | Sort-Object Distance | Select-Object -First 1
    if (-not $near -or $near.Distance -gt $MaxReach) { return 0 }
    $reach = [Math]::Min($MaxReach, [Math]::Max(3000, $near.Distance + 1500))
    $null = Send-FL "ap set engage_radius $reach"
    # A mission battle is not left: fleeing fails it ("You ran from the battle", M02 Ashcroft 18 Oct 2026) - the
    # defence is repairs, batteries and evasion (Invoke-FLMissionFight restores the defaults).
    $null = Send-FL 'ap set flee 0'; $null = Send-FL 'ap set caution 0'
    $null = Send-FL 'ap start combat'
    $reach
}

function Invoke-FLMissionFight([string]$UntilIds, [int]$After, [int]$TimeoutSec = 600, [hashtable]$Settings = @{}) {
    foreach ($k in $Settings.Keys) { $null = Send-FL "ap set $k $($Settings[$k])" }
    $st = @{ hit = $null; seen = $null }
    try {
        $null = Wait-FLUntil {
            $null = Close-FLBlocker
            foreach ($l in (Get-FLEventLog $After)) {
                if ("$l" -match 'mission-message type=FAILURE|player-death') { throw "mission failed: $l" }
                if ("$l" -match "objective .*obj=($UntilIds) ") { $st.hit = $Matches[1] }
            }
            if ($st.hit) { return $true }
            Update-FLPlanB $After
            Update-FLStall $After
            $p = Get-FLPilot
            if ($p -and -not $p.Locked -and (Get-FLAutopilot).Mode -in 'off', 'hold') {
                $now = (@(Get-FLContacts -Hostile | Where-Object { $_.Distance -le 15000 } | ForEach-Object { $_.Id }) | Sort-Object) -join ','
                if ($now -and $now -ne $st.seen) {
                    $st.seen = $now
                    $null = Start-FLMissionCombat
                }
            }
            $false
        } "fight until objective $UntilIds" $TimeoutSec
    } finally {
        $null = Send-FL 'ap stop'; $null = Send-FL 'ap set engage_radius 3000'
        $null = Send-FL 'ap set flee -1'; $null = Send-FL 'ap set caution 1'
    }
    $st.hit
}

# ---- Plan B: what to do next, inferred from the mission's own words (user idea, 18 Oct 2026) ----
# When a scripted step's wait runs out (the deterministic handling missed something), the next action is read from
# what the game tells the player: the current objective's text and the spoken lines since (comm events carry the
# first line's hash; subtitles\data\subtitles.tsv has every line's text and speaker), plus the objective's own target
# (`target=`, a marked object) or navmarker (`nav=`). Ordered rules turn that into one harness action.
function Get-FLLineIndex {
    if (-not $script:FLLines) {
        $script:FLLines = @{}
        foreach ($r in (Import-Csv (Join-Path $script:FLRoot 'subtitles\data\subtitles.tsv') -Delimiter "`t")) {
            $script:FLLines[[uint32](Get-FLHash $r.id)] = $r
        }
    }
    $script:FLLines
}

# The spoken lines after $After: Seq, From (the speaking ship), Speaker, Id, Text.
function Get-FLDialogue([int]$After, [int]$Last = 12) {
    $ix = Get-FLLineIndex
    @(Get-FLEventLog $After | Where-Object { "$_" -match ' comm from=(\d+) to=\d+ lines=\d+ first=0x([0-9A-F]+)' } | ForEach-Object {
        $null = "$_" -match '^(\d+) .* comm from=(\d+) to=(\d+) lines=\d+ first=0x([0-9A-F]+)'
        $r = $ix[[Convert]::ToUInt32($Matches[4], 16)]
        [pscustomobject]@{ Seq = [int]$Matches[1]; From = [uint32]$Matches[2]; To = [uint32]$Matches[3]; Speaker = $r.speaker
                           Id = $r.id; Text = $r.text }
    }) | Select-Object -Last $Last
}

# Seconds of mission dialogue still to play (0 = quiet). The game queues comm lines and plays them one after another,
# each for its clip length (subtitles.tsv sec, from the comm event's ids and send delay). Only the mission's own lines
# count (kind story / unique; generic chatter never holds the player), and a line already heard in this log is a nag
# repeat (nags repeat until the player acts: waiting on them is wrong). Timed against the event's GetTickCount.
function Get-FLDialogueLeft([int]$After) {
    $ix = Get-FLLineIndex
    $seen = @{}; $until = 0L
    foreach ($l in (Get-FLEventLog $After)) {
        if ("$l" -notmatch '^\d+ (\d+) comm from=\d+ to=\d+ lines=\d+ first=0x([0-9A-F]+)(?: ids=([0-9A-F,]*) delay=([\d.]+))?') { continue }
        $t = [long]$Matches[1]
        $ids = if ($Matches[3]) { $Matches[3] -split ',' } else { @($Matches[2]) }
        $delay = if ($Matches[4]) { [double]$Matches[4] } else { 0 }
        $key = $ids -join ','
        $repeat = $seen[$key]; $seen[$key] = $true
        $dur = 0.0; $story = $false
        foreach ($id in $ids) {
            $r = $ix[[Convert]::ToUInt32($id, 16)]
            if ($r) { $dur += [double]$r.sec; if ($r.kind -in 'story', 'unique') { $story = $true } }
        }
        if (-not $story -or $repeat) { continue }
        $until = [Math]::Max([long]($t + $delay * 1000), $until) + [long]($dur * 1000)
    }
    $now = [Environment]::TickCount64 -band 0xFFFFFFFFL
    [Math]::Max(0, ($until - $now) / 1000.0)
}

# Idle park (user note 1, 18 Oct 2026): while the harness waits and nothing flies the ship (free flight, autopilot off,
# no lane / lock / jump), park it the way the game parks its NPCs - the autopilot's hold: the game's Idle stop where it
# comes to rest, the nose along the next leg of the game's route (else it stays). The hold yields by itself to any maneuver or mission
# directive that takes the ship, and every harness action replaces it. $script:FLParkOff = $true turns it off.
function Update-FLIdlePark {
    if ($script:FLParkOff) { return }
    $p = Get-FLPilot
    $idle = $p -and $p.Op -eq 'freeflight' -and -not ($p.Lane -or $p.Locked -or $p.Jumping) -and
            (Get-FLAutopilot).Mode -eq 'off'
    # Idle for 2 s on end: a maneuver just requested (by plan B, the stall layer) shows in the op a frame or two later,
    # and the hold's Idle stop would cancel it.
    if (-not $idle) { $script:FLParkIdle = $null; return }
    if (-not $script:FLParkIdle) { $script:FLParkIdle = [Diagnostics.Stopwatch]::StartNew(); return }
    if ($script:FLParkIdle.Elapsed.TotalSeconds -lt 2) { return }
    $script:FLParkIdle = $null
    $null = Send-FL 'ap start hold'
}

# Is the mission talking right now? From the game's state (user, 18 Oct 2026: "more attuned to game logic and state
# data", nearby dialogue can be NPC chatter): the radio's own playing flag (comm player +0x460, debug `commplaying`)
# and the comm that started last is a mission line (kind story / unique - chatter is not - and not a nag repeat).
# Falls back to the clip-length estimate (Get-FLDialogueLeft) when the flag is unknown (no comm yet, older DLL).
function Get-FLDialogueBusy([int]$After) {
    $r = Send-FL 'commplaying'
    if ($r -notmatch 'comm-playing (-?\d)') { return (Get-FLDialogueLeft $After) -gt 0 }
    if ($Matches[1] -eq '-1') { return (Get-FLDialogueLeft $After) -gt 0 }
    if ($Matches[1] -ne '1') { return $false }
    $ix = Get-FLLineIndex
    $comms = @(Get-FLEventLog $After | Where-Object { "$_" -match ' comm from=' })
    if (-not $comms.Count) { return $false } # playing something from before the window (or not ours to judge)
    $keyOf = { param($l) if ("$l" -match 'ids=([0-9A-F,]+)') { $Matches[1] } elseif ("$l" -match 'first=0x([0-9A-F]+)') { $Matches[1] } }
    $last = $comms[-1]; $key = & $keyOf $last
    if (@($comms | Select-Object -SkipLast 1 | Where-Object { (& $keyOf $_) -eq $key }).Count) { return $false } # a nag repeat
    foreach ($id in ($key -split ',')) {
        $row = $ix[[Convert]::ToUInt32($id, 16)]
        if ($row -and $row.kind -in 'story', 'unique') { return $true }
    }
    $false
}

# Dialogue buffer (user note 1, 18 Oct 2026: the Colorado gate was docked while Battleship Unity still talked): let the
# mission's dialogue play out before the next action, parked meanwhile - as a player listens before moving on. Done when
# the radio has been quiet of mission lines for 1.5 s (the next line of a conversation follows within ~1 s). Never under
# threat (a hostile within 5 km: act now); $MaxSec is only a hang guard. Returns the seconds waited.
function Wait-FLDialogue([int]$After, [double]$MaxSec = 180) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $quiet = $null
    while ($sw.Elapsed.TotalSeconds -lt $MaxSec) {
        if (Get-FLDialogueBusy $After) { $quiet = $null }
        elseif (-not $quiet) { $quiet = [Diagnostics.Stopwatch]::StartNew() }
        if ($quiet -and $quiet.Elapsed.TotalSeconds -ge 1.5) { break }
        if (@(Get-FLContacts -Hostile | Where-Object { $_.Distance -lt 5000 }).Count) { break }
        foreach ($l in (Get-FLEventLog $After)) {
            if ("$l" -match 'mission-message type=FAILURE|player-death') { throw "mission failed: $l" }
        }
        $null = Close-FLBlocker
        Update-FLIdlePark
        $null = Wait-FLFramesPassed 15
    }
    $w = $sw.Elapsed.TotalSeconds - $(if ($quiet) { $quiet.Elapsed.TotalSeconds } else { 0 })
    if ($w -gt 1) { "{0:HH:mm:ss} dialogue buffer {1:N1} s" -f (Get-Date), $w | Write-Host }
    $w
}

# The rules, first match wins; the objective text is checked before the dialogue (it is the game's own instruction),
# the newest line before older ones. Action: formation | tradelane | dock | travel | fight | follow | escort.
# O: the objective text (the game's instruction, terse); D: a spoken line - only instructions to the player (imperative
# forms; narration such as "convoys have come under attack" or "the Trade Lane is back up" must not match: both
# misfired in M01a, 18 Oct 2026 - the second docked a lane, M01a's premature-TL34 trap).
$script:FLHintRules = @(
    # "Escort the Transport Convoy to ..." = formation with the escorted ship, the game's own escort (user, 19 Oct 2026,
    # M03: plan B read the escort objectives as travel and flew ahead of the convoy - its nag fails at 1200 m)
    @{ A = 'formation'; O = 'formation|\bescort\b'
       D = '\b(form up|(re-?)?enter formation|get (back )?in(to)? formation|use the formation maneuver)\b' }
    @{ A = 'tradelane'; O = 'trade ?lane'
       D = '\b(use|take|dock with|enter|get on) the (marked )?trade ?lane\b' }
    @{ A = 'dock';      O = '\b(dock|land) (with|at|on)\b'
       D = '\b((you|go ahead and) (dock|land)|request (docking|landing)|dock with (planet|station|the base))\b' }
    @{ A = 'fight';     O = '\b(destroy|kill|eliminate|defend|protect|attack)\b'
       D = '\b(engage (the )?(enemy|them|hostiles?|those)|engage enemy ships|take (them|him|her|it|those \w+) out|open fire|weapons free|destroy (them|those|the \w+)|attack (them|those|the \w+))\b' }
    @{ A = 'follow';    O = '\bfollow\b'
       D = '\b(follow me|stay (close|with me)|stick with me|on my wing)\b' }
    @{ A = 'scan';      O = '\bscan'
       D = '\b(scan (every|each|all|the|those|that|nearby|any)\b|use your (cargo )?scanner|start scanning)' }
    @{ A = 'wait';      O = '\bwait\b'
       D = '\b(wait (here|for (me|us|my))|hold (your )?position|stand by)\b' }
    @{ A = 'travel';    O = '\b(waypoint|fly to|go to|meet|rendezvous|proceed)\b'
       D = '\b(fly (toward|to) (it|the waypoint)|head (to|for) the waypoint|meet (me|us) (at|near|by))\b' }
)

# The friendly ship the mission marks most recently (mark events: the HUD's mission marker; the player's own
# information): who an "escort" objective means when it names no target. 0 / $null if none is present.
# Cross-checked with the objective's own words: a marked friendly whose displayed name shares a word with the text
# ("Escort the Transport Convoy" -> a marked "Transport ..."; Juni and the Gamma wing are marked too in M03) wins.
function Get-FLMarkedFriendly([int]$After = 0, [string]$Text) {
    $marked = [ordered]@{}
    foreach ($l in (Get-FLEventLog $After)) { if ("$l" -match ' mark id=(\d+) state=(\d+)') { $marked.Remove($Matches[1]); if ($Matches[2] -ne '0') { $marked[$Matches[1]] = $true } } }
    if (-not $marked.Count) { return $null }
    $friends = @(Get-FLContacts -All | Where-Object { $_.Ship -and $_.Attitude -gt 0 -and $marked.Contains("$($_.Id)") })
    if (-not $friends.Count) { return $null }
    if ($Text) {
        $words = @([regex]::Matches($Text.ToLower(), '[a-z]{4,}') | ForEach-Object Value | Where-Object { $_ -notin 'escort', 'with', 'from', 'into', 'fly' })
        $named = $friends | Where-Object { $n = "$($_.Name)".ToLower(); @($words | Where-Object { $n.Contains($_) }).Count } |
                 Sort-Object Distance | Select-Object -First 1
        if ($named) { return $named.Id }
    }
    foreach ($id in @($marked.Keys)[($marked.Count - 1)..0]) {
        $c = $friends | Where-Object { "$($_.Id)" -eq $id } | Select-Object -First 1
        if ($c) { return $c.Id }
    }
    $null
}

# The space object an objective's target= ('R') value names: a reputation instance (FmtStr::append_rep_instance; M03
# Walker = 8) or a space id (M03's dock ring) - the DLL's `objobj`. $null when nothing in the system matches yet.
function Resolve-FLObjectiveTarget($Ref) {
    if (-not $Ref) { return $null }
    $m = [regex]::Match((Send-FL "objobj $Ref"), 'id=(\d+)')
    if ($m.Success -and [uint32]$m.Groups[1].Value) { return [uint32]$m.Groups[1].Value }
    $null
}

function Get-FLMissionHint([int]$After) {
    $obj = $null; $target = $null; $nav = $null; $objSeq = 0
    foreach ($l in (Get-FLEventLog $After)) {
        if ("$l" -match '^(\d+) .* objective count=\d+ .* obj=\d+ "([^"]*)"(.*)$') {
            $objSeq = [int]$Matches[1]; $obj = $Matches[2]; $rest = $Matches[3]
            $target = if ($rest -match 'target=(\d+)') { [uint32]$Matches[1] } else { $null }
            if ($target) { $r = Resolve-FLObjectiveTarget $target; if ($r) { $target = $r } }
            $nav = if ($rest -match 'nav=(-?\d+),(-?\d+),(-?\d+)') { [double[]]@($Matches[1], $Matches[2], $Matches[3]) } else { $null }
        }
    }
    # Spoken lines count only when addressed to the player (the comm's receiver is the player ship) or to everyone.
    $me = [uint32][regex]::Match((Send-FL 'contacts'), 'player=(\d+)').Groups[1].Value   # changes with every load
    $lines = @(Get-FLDialogue $After | Where-Object { $_.To -eq $me -or $_.To -eq 0 })
    $sources = @()
    if ($obj -and $obj.Trim()) { $sources += [pscustomobject]@{ Text = $obj; From = 0; Who = 'objective'; Seq = $objSeq; Kind = 'O' } }
    for ($i = $lines.Count - 1; $i -ge 0; $i--) {
        if ($lines[$i].Seq -lt $objSeq) { break }   # lines before the current objective are history
        $sources += [pscustomobject]@{ Text = $lines[$i].Text; From = $lines[$i].From; Who = $lines[$i].Speaker; Seq = $lines[$i].Seq; Kind = 'D' }
    }
    foreach ($s in $sources) {
        foreach ($rule in $script:FLHintRules) {
            if ($s.Text -match $rule[$s.Kind]) {
                return [pscustomobject]@{ Action = $rule.A; Target = $target; Nav = $nav; Speaker = $s.From; Seq = $s.Seq
                                          Kind = $s.Kind; Evidence = "$($s.Who): $($s.Text)"; Objective = $obj }
            }
        }
    }
    $null
}

# What plan A is doing now, in plan B's terms (from the game: autopilot mode, maneuver, lane): fight | formation |
# move (travel, goto, dock, lane: one class - travel rides lanes and docks itself) | follow | idle.
function Get-FLPlanAState {
    $a = Get-FLAutopilot; $p = Get-FLPilot
    if (-not $p) { return 'busy' }   # cutscene / docked / loading: nothing to compare
    # Escort fights what threatens the escorted ship, anchored to it: it IS a fight (M02 step 14, 18 Oct 2026: counted
    # as "follow", plan B replaced it with an unanchored fight and the mission failed "You ran from the battle").
    if ($a.Mode -in 'combat', 'escort') { return 'fight' }
    if ($a.Mode -eq 'follow') { return 'follow' }
    if ($p.Lane) { return 'busy' }   # a lane ride is never interrupted
    if ($a.Mode -in 'travel', 'goto', 'dock' -or $p.Op -in 'goto', 'dock') { return 'move' }
    if ($p.Op -eq 'formation') { return 'formation' }
    if ($p.Locked) { return 'busy' }
    'idle'
}

# Plan B alongside plan A (user, 18 Oct 2026): computed at every new objective / line (no waiting for A to give up),
# compared with what A is doing; B acts when A is idle (at once) or keeps disagreeing for $PatienceSec. Each piece
# of evidence acts once. Call it from every wait loop; $script:FLPlanBOff = $true turns it off (a script that holds
# on purpose, e.g. a trap such as M01a's premature TL34). Log: $script:FLPlanBLog.
$script:FLPlanB = @{ seq = -1; hint = $null; since = $null; acted = @{}; logged = @{} }
$script:FLPlanBLog = [System.Collections.Generic.List[string]]::new()
function Update-FLPlanB([int]$After, [double]$PatienceSec = 20) {
    if ($script:FLPlanBOff) { return }
    Update-FLEventLog
    if ($script:FLLogSeq -ne $script:FLPlanB.seq) {
        $script:FLPlanB.seq = $script:FLLogSeq
        $h = Get-FLMissionHint $After
        if ($h -and (-not $script:FLPlanB.hint -or $h.Seq -ne $script:FLPlanB.hint.Seq -or $h.Action -ne $script:FLPlanB.hint.Action)) {
            $script:FLPlanB.hint = $h; $script:FLPlanB.since = [Diagnostics.Stopwatch]::StartNew()
        }
    }
    $h = $script:FLPlanB.hint
    if (-not $h -or $script:FLPlanB.acted["$($h.Seq)"]) { return }
    $a = Get-FLPlanAState
    $want = if ($h.Action -in 'travel', 'dock', 'tradelane') { 'move' } elseif ($h.Action -eq 'wait') { 'idle' } else { $h.Action }
    if ($want -eq 'idle' -and $a -ne 'idle') { return }   # a wait never stops what A does
    if ($a -eq 'busy') { return }
    if ($a -eq $want) {
        $script:FLPlanB.acted["$($h.Seq)"] = $true
        $script:FLPlanBLog.Add("agree $want ($($h.Evidence))")
        return
    }
    # A busy plan A yields only to the game's own instruction (the objective), and only after lasting disagreement; a
    # spoken line acts only on an idle A (logged once otherwise).
    if ($a -ne 'idle' -and $h.Kind -ne 'O') {
        if (-not $script:FLPlanB.logged["$($h.Seq)"]) {
            $script:FLPlanB.logged["$($h.Seq)"] = $true
            $script:FLPlanBLog.Add("disagree A=$a B=$want (a line: acts only once A is idle; $($h.Evidence))")
        }
        return
    }
    # Hostile fighters close by come before a trip (21 Oct 2026, M01a: the Donau scene ended a frame before "Destroy the
    # Hostile Ships", and B flew the stale "Meet King" navmarker - at Manhattan's death zone - with five Order fighters
    # 3 km off; the 2020 playthrough fought them on the spot). The hint waits; the fight is the caller's.
    if ($want -eq 'move' -and @(Get-FLContacts -Hostile | Where-Object { $_.Ship -and $_.Type -eq 'fighter' -and $_.Distance -le 5000 }).Count) {
        if (-not $script:FLPlanB.logged["hostile$($h.Seq)"]) {
            $script:FLPlanB.logged["hostile$($h.Seq)"] = $true
            $script:FLPlanBLog.Add("hold ${want}: hostile fighters within 5 km ($($h.Evidence))")
        }
        return
    }
    if ($a -eq 'idle' -or $script:FLPlanB.since.Elapsed.TotalSeconds -ge $PatienceSec) {
        $script:FLPlanB.acted["$($h.Seq)"] = $true
        $r = try { Invoke-FLMissionHint $h } catch { "plan B $($h.Action) failed: $_" }
        $script:FLPlanBLog.Add("override A=$a -> $r")
        Write-Host "  [plan B] A=$a -> $r"
    }
}

# Scans the ships around, the player's way (M02 "cut your engines at the waypoint and scan every ship"; user, 18 Oct
# 2026: King spotting Cosmo for us was a bug): stop (autopilot / maneuver off, engines idle), then target each ship
# in scanner reach, nearest first, and use the scanner (USER_SCAN_CARGO), each once; ships that come closer later
# are scanned as they arrive. Ends when a new objective arrives after $After, or after $TimeoutSec. Returns the count.
function Invoke-FLScanShips([int]$After, [double]$Reach = 2000, [int]$TimeoutSec = 120, [double[]]$At) {
    # Hold still while scanning (user, 18 Oct 2026: free flight drifted on): the autopilot keeps station at $At (the
    # waypoint) or where the ship is.
    $null = Send-FL 'ap stop'
    $r = if ($At) { Send-FL ("ap start hold {0} {1} {2}" -f $At[0], $At[1], $At[2]) } else { Send-FL 'ap start hold' }
    if ($r -match '^refused') { $null = Stop-FLManeuver }
    $st = @{ done = @{}; n = 0; chasing = $false }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        $null = Close-FLBlocker
        if (@(Get-FLEventLog $After | Where-Object { "$_" -match ' objective count=' -and [int](("$_" -split ' ')[0]) -gt $After }).Count) { Write-Host '  [scan] ends: new objective'; break }
        if (@(Get-FLEventLog $After | Where-Object { "$_" -match 'mission-message type=FAILURE|player-death' }).Count) { Write-Host '  [scan] ends: failure'; break }
        # Ships the mission marks come first (M02: Cosmo is marked as the convoy leaves the lane; 18 Oct 2026 he never
        # came within reach of the hold): a marked ship out of reach is approached, then the hold resumes.
        $marked = @{}
        foreach ($l in (Get-FLEventLog $After)) { if ("$l" -match ' mark id=(\d+) state=(\d+)') { $marked[$Matches[1]] = $Matches[2] -ne '0' } }
        $ships = @(Get-FLContacts -All | Where-Object { $_.Ship -and -not $st.done["$($_.Id)"] })
        $m = $ships | Where-Object { $marked["$($_.Id)"] } | Sort-Object Distance | Select-Object -First 1
        if ($m -and $m.Distance -ge $Reach) {
            if (-not $st.chasing) { $st.chasing = $true; $null = Send-FL 'ap stop'; $null = Send-FL "ap start goto $($m.Id)" }
            Start-Sleep -Milliseconds 300; continue
        }
        if ($st.chasing -and -not $m) {   # done with the marked ship: back to holding
            $st.chasing = $false; $null = Send-FL 'ap stop'
            $null = if ($At) { Send-FL ("ap start hold {0} {1} {2}" -f $At[0], $At[1], $At[2]) } else { Send-FL 'ap start hold' }
        }
        $c = if ($m) { $m } else { $ships | Where-Object { $_.Distance -lt $Reach } | Sort-Object Distance | Select-Object -First 1 }
        if (-not $c) { Start-Sleep -Milliseconds 300; continue }
        $st.done["$($c.Id)"] = $true
        Write-Host ("  [scan] {0} {1} at {2:N0} m" -f $c.Id, $c.Name, $c.Distance)
        try { $null = Set-FLTarget $c.Id } catch { Write-Host "  [scan] target failed: $_"; continue }
        $null = Send-FL 'tap USER_SCAN_CARGO'
        $st.n++
        $null = Wait-FLFramesPassed 30
    }
    $null = Send-FL 'ap stop'   # the hold ends with the scan; the mission's next step takes over
    $st.n
}

# Carries out a hint with the harness (one step; the caller waits for the mission's next signal again).
function Invoke-FLMissionHint($Hint) {
    $ring = { (Get-FLContacts -All | Where-Object { $_.Type -eq 'tradelane' } | Sort-Object Distance | Select-Object -First 1).Id }
    switch ($Hint.Action) {
        'formation' { $who = if ($Hint.Target) { $Hint.Target } elseif ($m = Get-FLMarkedFriendly 0 $Hint.Objective) { $m } else { $Hint.Speaker }
                      $null = Start-FLFormation $who }
        'tradelane' { $r = if ($Hint.Target) { $Hint.Target } else { & $ring }; $null = Start-FLTradeLane $r -NoWait }
        'dock'      { if ($Hint.Target) { $null = Start-FLTravel $Hint.Target -NoWait } }
        'fight'     { if (-not (Start-FLMissionCombat)) { return "plan B: fight - no hostile within the battle range ('$($Hint.Evidence)')" } }
        'scan'      {
            $at = if ($Hint.Nav -and ($p = Get-FLPilot) -and (Get-FLDistance $p.Pos $Hint.Nav) -lt 3000) { $Hint.Nav } else { $null }
            $n = Invoke-FLScanShips -After $Hint.Seq -At $at
            return "plan B: scan ($n scanned; from '$($Hint.Evidence)')"
        }
        'follow'    { if ($Hint.Speaker) { $null = Send-FL "ap start follow $($Hint.Speaker)" } }
        # No navmarker / target in the objective: the waypoint the speaker "uploaded" is in the waypoint list
        # (`ap start travel` with no destination follows it; M01a after the Order fight, 18 Oct 2026).
        # The objective's own target first (user, 19 Oct 2026, M03: "Fly to Planet California Minor" names the docking
        # ring, "Fly to Walker" names Walker - flying the nav coordinate went at the planet / held at Walker's spawn
        # point and failed "meet up"): a ship is joined in formation (meets it and stays with it), an object is arrived
        # at without docking (the dock is the mission's next instruction); the coordinate only without a target.
        'travel'    {
            $tc = if ($Hint.Target) { Get-FLContacts -All | Where-Object { $_.Id -eq $Hint.Target } | Select-Object -First 1 }
            if ($tc -and $tc.Ship) { $null = Join-FLShip $tc.Id }
            elseif ($tc) { $null = Start-FLTravel $tc.Id -NoDock -NoWait }
            elseif ($Hint.Target -and ($mf = Get-FLMarkedFriendly 0 $Hint.Objective)) { $null = Join-FLShip $mf }
            elseif ($Hint.Nav) { $null = Start-FLTravel -Position $Hint.Nav -NoWait }
            else { $null = Send-FL 'ap start travel' }
        }
    }
    "plan B: $($Hint.Action) (from '$($Hint.Evidence)')"
}

# ---- Stalls: spot them, analyse them, act (user, 18 Oct 2026) ----
# A stall = no mission progress for $StallSec while the ship gets nowhere. Progress: a new objective / step, mission
# message or spoken line, a kill, or the ship moving / riding. Each check builds one situation snapshot
# (Get-FLSituation); the response is the first rule that applies, each rule once per stall: close a blocker -> fight
# hostiles in the battle range -> go for the objective's target / navmarker by its kind -> plan B's reading of the
# words -> logged as unresolved. Every stall, its snapshot and the action are appended to
# research\experience\stalls.jsonl (find recurring ones, fix them at the source). $script:FLStallOff turns it off.
function Get-FLSituation([int]$After) {
    $obj = $null
    foreach ($l in (Get-FLEventLog $After)) {
        if ("$l" -match '^(\d+) (\d+) objective count=\d+ .* obj=(\d+) "([^"]*)"(.*)$') {
            $rest = $Matches[5]
            $obj = [pscustomobject]@{ Seq = [int]$Matches[1]; Id = [int]$Matches[3]; Text = $Matches[4]
                                      Target = $(if ($rest -match 'target=(\d+)') { [uint32]$Matches[1] } else { 0 })
                                      Nav = $(if ($rest -match 'nav=(-?\d+),(-?\d+),(-?\d+)') { [double[]]@($Matches[1], $Matches[2], $Matches[3]) } else { $null }) }
        }
    }
    if ($obj -and $obj.Target) { $obj.Target = [uint32](Resolve-FLObjectiveTarget $obj.Target) } # rep instance -> space id (0: not listed)
    $p = Get-FLPilot; $a = Get-FLAutopilot
    $stop = (Get-FLEventLog $After | Where-Object { "$_" -match 'autopilot stop .*why=(.+?) kills=' } | Select-Object -Last 1)
    $why = if ("$stop" -match 'why=(.+?) kills=') { $Matches[1] } else { '' }
    $host_ = @(Get-FLContacts -Hostile | Sort-Object Distance)
    $navD = if ($obj -and $obj.Nav -and $p) { [int](Get-FLDistance $p.Pos $obj.Nav) } else { -1 }
    $tgt = if ($obj -and $obj.Target) { Get-FLContacts -All | Where-Object { $_.Id -eq $obj.Target } | Select-Object -First 1 } else { $null }
    [pscustomobject]@{
        Objective = $obj; Pilot = $p; Mode = $a.Mode; LastStop = $why; PlanA = $(if ($p) { Get-FLPlanAState } else { 'busy' })
        Hostiles = $host_.Count; NearestHostile = $(if ($host_.Count) { $host_[0].Distance } else { -1 })
        NavDistance = $navD; Target = $tgt; Blocked = ((Send-FL 'visible MMOkButton') -match '^visible' -or (Send-FL 'visible NavMapContinueButton') -match '^visible')
        Story = Get-FLStoryState
    }
}

$script:FLStall = @{ seq = -1; since = $null; pos = $null; tried = @{}; episode = 0 }
$script:FLStallLog = [System.Collections.Generic.List[string]]::new()
function Update-FLStall([int]$After, [double]$StallSec = 25) {
    if ($script:FLStallOff) { return }
    Update-FLEventLog
    $st = $script:FLStall
    $p = Get-FLPilot
    # progress since the last check: mission events, kills, or the ship covering ground / riding
    $news = @(Get-FLEventLog ([Math]::Max($After, $st.seq)) | Where-Object { "$_" -match ' (objective|mission-message|comm from|autopilot target-destroyed|nn-message)' })
    # Ground covered counts only under control (a maneuver / the autopilot / a ride): drifting in free flight is aimless,
    # not progress (user, 18 Oct 2026: "aimless manual free-flight is usually a conscious decision by the player").
    $moved = $p -and ($p.Lane -or ($st.pos -and (Get-FLDistance $p.Pos $st.pos) -gt 150 -and (Get-FLPlanAState) -ne 'idle'))
    if ($news.Count -or $moved -or -not $st.since) {
        $st.seq = $script:FLLogSeq; $st.since = [Diagnostics.Stopwatch]::StartNew(); $st.pos = $(if ($p) { $p.Pos } else { $null })
        if ($news.Count) { $st.tried = @{} }   # the mission moved: a new episode
        return
    }
    # (the reference position moves only with progress, above: was updated every poll - ~50 ms - so a steady travel never
    # covered 150 m "since the last check" and was restarted every 26 s, M03 19 Oct 2026)
    if ($st.since.Elapsed.TotalSeconds -lt $StallSec) { return }
    # Stalled: analyse, act (each rule once per episode), record.
    $s = Get-FLSituation $After
    $act = $null
    $try = { param($k, $do) if (-not $st.tried[$k]) { $st.tried[$k] = $true; $script:act = $k; & $do; return $true }; $false }
    $script:act = $null
    if ($s.Blocked) { $null = & $try 'close-blocker' { $null = Close-FLBlocker } }
    # The autopilot stopped on its own (fled, target lost) out of an anchored mode: resume that first (M02 18 Oct 2026:
    # the escort of King fled, a new unanchored fight then left King - "You ran from the battle").
    $last = Get-FLEventLog $After | Where-Object { "$_" -match 'autopilot start mode=(escort|follow|formation|hold) target=(\d+)' } | Select-Object -Last 1
    if (-not $script:act -and $last -and $s.Mode -eq 'off' -and $s.LastStop -notmatch '^(command|yield)') {
        $null = "$last" -match 'mode=(\w+) target=(\d+)'
        $resume = "ap start $($Matches[1]) $($Matches[2])"
        $null = & $try "resume-$($Matches[1])" { $null = Send-FL 'ap set flee 0'; $null = Send-FL 'ap set caution 0'; $null = Send-FL $resume }
    }
    if (-not $script:act -and $s.Hostiles -and $s.NearestHostile -le 15000 -and $s.Mode -ne 'combat') {
        $null = & $try 'fight' { $null = Start-FLMissionCombat } }
    if (-not $script:act -and $s.Objective -and $s.PlanA -eq 'idle') {
        $o = $s.Objective
        if ($o.Text -match 'formation|\bescort\b' -and $s.Target -and $s.Target.Ship) { $null = & $try 'formation' { $null = Start-FLFormation $s.Target.Id } }
        elseif ($o.Text -match '\bescort\b' -and ($esc = Get-FLMarkedFriendly 0 $o.Text)) { $null = & $try 'formation' { $null = Start-FLFormation $esc } }
        elseif ($s.Target -and $s.Target.Type -eq 'tradelane') { $null = & $try 'tradelane' { $null = Start-FLTradeLane $s.Target.Id -NoWait } }
        elseif ($s.Target) { $null = & $try 'go-target' { $null = Start-FLTravel $s.Target.Id -NoWait } }
        elseif ($o.Nav -and $s.NavDistance -gt 1500) { $null = & $try 'go-nav' { $null = Start-FLTravel -Position $o.Nav -NoWait } }
        elseif ($o.Nav) { $null = & $try 'hold-nav' { $null = Send-FL ("ap start hold {0} {1} {2}" -f $o.Nav[0], $o.Nav[1], $o.Nav[2]) } }
    }
    if (-not $script:act) {
        $h = Get-FLMissionHint $After
        if ($h) { $null = & $try "planB-$($h.Action)" { $null = Invoke-FLMissionHint $h } }
    }
    $line = [ordered]@{ time = (Get-Date).ToString('s'); stalledSec = [int]$st.since.Elapsed.TotalSeconds
                        action = $(if ($script:act) { $script:act } else { 'unresolved' })
                        objective = $(if ($s.Objective) { "$($s.Objective.Id) $($s.Objective.Text)" } else { '' })
                        planA = $s.PlanA; mode = $s.Mode; lastStop = $s.LastStop; hostiles = $s.Hostiles
                        nearestHostile = $s.NearestHostile; navDistance = $s.NavDistance; story = $s.Story }
    $json = $line | ConvertTo-Json -Compress
    $script:FLStallLog.Add($json)
    Write-Host "  [stall] $($line.action) after $($line.stalledSec) s: $($line.objective) (A=$($s.PlanA) mode=$($s.Mode) stop=$($s.LastStop) hostiles=$($s.Hostiles)@$($s.NearestHostile) nav=$($s.NavDistance))"
    $dir = Join-Path $script:FLRoot 'research\experience'
    if (-not (Test-Path -LiteralPath $dir)) { $null = New-Item -ItemType Directory -Force $dir }
    Add-Content -LiteralPath (Join-Path $dir 'stalls.jsonl') -Value $json
    $st.since = [Diagnostics.Stopwatch]::StartNew()   # give the action time before the next judgement
}

# ---- Job board (random missions; the player's safe way to earn money) ----
# The game creates a base's job offers when the base loads (host event `offer key= base= type= reward=`, from
# pub::GF::MissionVendorOfferCreate); the board lists them in key order, five rows visible (MissionVendorSummaryBox0..4).
# Types: 1 Kill Ships; 3 / 6 other kinds (portrait / cargo icons, not yet flown). Returns the offers of the latest
# batch with their Row.
# Every event the host still holds (paged: one reply is capped), whatever this script's own log saw.
function Get-FLHistory([string]$Match = '') {
    $out = [System.Collections.Generic.List[string]]::new(); $cursor = 0
    for ($page = 0; $page -lt 200; $page++) {
        $lines = @((Send-FL "events $cursor") -split "`n" | Where-Object { $_ } | Select-Object -Skip 1)
        if (-not $lines.Count) { break }
        foreach ($l in $lines) { if (-not $Match -or $l -match $Match) { $out.Add($l) } }
        $cursor = [int](($lines[-1] -split ' ')[0])
    }
    $out
}

# The offers on a base's board now: created (offers are made once per base, kept across loads) and not gone
# (accepted / renewed), in key order = board order. $Base: base hash (default: the base of the newest offer).
function Get-FLJobOffers([uint32]$Base = 0) {
    $live = [ordered]@{}
    foreach ($l in (Get-FLHistory ' offer')) {
        if ($l -match 'offer key=(\d+) base=(\d+) type=(\d+) reward=(\d+)') {
            $live["$($Matches[1])"] = [pscustomobject]@{ Key = [int]$Matches[1]; Base = [uint32]$Matches[2]; Type = [int]$Matches[3]
                                                       Reward = [int]$Matches[4]; Row = -1 }
            if (-not $PSBoundParameters.ContainsKey('Base')) { $Base = [uint32]$Matches[2] }
        } elseif ($l -match 'offer-gone key=(\d+)') { $live.Remove("$($Matches[1])") }
    }
    $row = 0
    foreach ($o in ($live.Values | Where-Object { $_.Base -eq $Base } | Sort-Object Key)) {
        $o.Row = $row++
        $o | Add-Member -NotePropertyName Difficulty -NotePropertyValue (Get-FLJobDifficulty $o.Reward) -PassThru
    }
}

# A job's difficulty from its reward: the game pays diff2money(difficulty) (DATA\RandomMissions\diff2money.ini, a
# rising table, linear between its points), so the inverse is exact. What it sends: one npcranktodiff.ini row per wave,
# the ship count drawn by weight (KnowledgeBase, adoxa) at the column matching the difficulty. Any mod's tables (read
# from the install).
function Get-FLJobDifficulty([double]$Reward) {
    if (-not $script:FLMoneyTable) {
        $f = Join-Path $script:FLRoot 'Install\Freelancer HD Edition\DATA\RandomMissions\diff2money.ini'
        $script:FLMoneyTable = @(Get-Content -LiteralPath $f | Where-Object { $_ -match '^\s*Diff2Money\s*=\s*([\d.]+),\s*([\d.]+)' } |
            ForEach-Object { $null = $_ -match '=\s*([\d.]+),\s*([\d.]+)'; , @([double]$Matches[1], [double]$Matches[2]) })
    }
    $t = $script:FLMoneyTable
    for ($i = 1; $i -lt $t.Count; $i++) {
        if ($Reward -le $t[$i][1]) {
            $f = ($Reward - $t[$i - 1][1]) / [Math]::Max(1e-9, $t[$i][1] - $t[$i - 1][1])
            return [Math]::Round($t[$i - 1][0] + $f * ($t[$i][0] - $t[$i - 1][0]), 4)
        }
    }
    $t[-1][0]
}

# What jobs take, measured (the autopilot's experience, persisted across runs; research\experience\jobs.jsonl,
# git-ignored): one line per finished job. Get-FLJobSeconds: the median time of the recorded jobs closest in
# difficulty (at least 3), else $Default.
function Add-FLJobRecord($Offer, $Result) {
    $dir = Join-Path $script:FLRoot 'research\experience'
    if (-not (Test-Path -LiteralPath $dir)) { $null = New-Item -ItemType Directory -Force $dir }
    $rec = [ordered]@{ time = (Get-Date).ToString('s'); base = $Offer.Base; type = $Offer.Type; reward = $Offer.Reward
                       difficulty = $Offer.Difficulty; result = $Result.Result; secs = $Result.Secs }
    Add-Content -LiteralPath (Join-Path $dir 'jobs.jsonl') -Value ($rec | ConvertTo-Json -Compress)
}
function Get-FLJobSeconds([double]$Difficulty, [double]$Default = 240) {
    $f = Join-Path $script:FLRoot 'research\experience\jobs.jsonl'
    if (-not (Test-Path -LiteralPath $f)) { return $Default }
    $ok = @(Get-Content -LiteralPath $f | ForEach-Object { $_ | ConvertFrom-Json } | Where-Object { $_.result -eq 'SUCCESS' })
    if ($ok.Count -lt 3) { return $Default }
    $near = @($ok | Sort-Object { [Math]::Abs($_.difficulty - $Difficulty) } | Select-Object -First 5)
    $s = @($near | ForEach-Object { [double]$_.secs } | Sort-Object)
    $s[[int][Math]::Floor($s.Count / 2)]
}

# The job's goal: the last navmarker of the latest objective list (objective line + its objective-step lines) in the
# event log after $After - where a Kill Ships job's enemies wait.
function Get-FLJobGoal([int]$After) {
    $navs = @()
    foreach ($l in (Get-FLEventLog $After)) {
        if ("$l" -match ' objective count=') { $navs = @() }
        if ("$l" -match ' objective(-step)? .* nav=(-?\d+),(-?\d+),(-?\d+)') { $navs += , [double[]]@($Matches[2], $Matches[3], $Matches[4]) }
    }
    if ($navs.Count) { $navs[-1] } else { $null }
}

# The player's net worth as the story code counts it (debug `worth`: server GetAssetValue + InspectCash; equipment
# counts 0.3 x price, the hull 0.8, cargo in full - FREELANCER_RE "Net worth").
function Get-FLWorth {
    $r = Send-FL 'worth'
    if ($r -match 'cash=(-?\d+) assets=(-?[\d.]+) total=(-?[\d.]+) rc=0,0') {
        [pscustomobject]@{ Cash = [int]$Matches[1]; Assets = [double]$Matches[2]; Total = [double]$Matches[3] }
    }
}

# The commodity table (tools\flmarket.js --sells: base, good, price, local price, net-worth gain per unit bought there),
# read once per session.
function Get-FLMarket {
    if (-not $script:FLMarket) {
        $js = Join-Path $script:FLDataRoot 'tools\flmarket.js'
        $script:FLMarket = @(& node $js --sells 2>$null | Select-Object -Skip 1 | ForEach-Object {
            $c = "$_" -split "`t"
            [pscustomobject]@{ Base = $c[0]; Good = $c[1]; Price = [double]$c[2]; Local = [double]$c[4]; Gain = [double]$c[6] }
        })
    }
    $script:FLMarket
}

# The money layer's first rule (docs\LEGS.md "Free-time legs"; FT1 measured 22 Oct 2026: 15 Light Arms bought at
# Trenton Outpost cleared the M01b -> M02 gate, docking alone did not): while the story's net-worth gate is open, buy
# the commodity this base sells furthest below its net-worth price (cargo counts at the goods.ini price), else fly to the
# base of this system that does and buy there. Docked only. Returns what it did.
function Invoke-FLMoneyPlan([int]$MaxStops = 2) {
    $did = [Collections.Generic.List[string]]::new()
    for ($stop = 0; $stop -lt $MaxStops; $stop++) {
        $need = Get-FLWorthNeeded
        if ($null -eq $need -or $need -le 0) { break }
        $mode = Get-FLMode
        if ($mode -notin 'docked', 'space', 'space-ui') { $did.Add("mode $mode"); break }
        $st = Send-FL 'state'
        $base = [Convert]::ToUInt32([regex]::Match($st, 'base=0x([0-9a-fA-F]+)').Groups[1].Value, 16)
        $sys = [Convert]::ToUInt32([regex]::Match($st, 'system=0x([0-9a-fA-F]+)').Groups[1].Value, 16)
        $rows = @(Get-FLMarket | Where-Object { $_.Gain -gt 0 -and (Get-FLHash ($_.Base -replace '_\d+_base$', '')) -eq $sys } |
                  Sort-Object Gain -Descending)
        if (-not $rows.Count) { $did.Add('no below-price commodity in this system'); break }
        $here = if ($mode -eq 'docked') { $rows | Where-Object { (Get-FLHash $_.Base) -eq $base } | Select-Object -First 1 }
        if (-not $here) {
            # the nearest of the bases within 80 % of the best gain (chain 22 Oct 2026: Detroit Munitions +140 was a
            # 4 min flight west, Trenton +126 is beside Manhattan; FT1 cleared with 15 units at Trenton)
            $best = $rows[0].Gain
            $all = @(Get-FLContacts -All)
            $near = @($rows | Where-Object { $_.Gain -ge 0.8 * $best } | ForEach-Object {
                $id = Get-FLHash ($_.Base -replace '_base$', '')
                $c = $all | Where-Object { $_.Id -eq $id } | Select-Object -First 1
                [pscustomobject]@{ Row = $_; Dist = if ($c) { [double]$c.Distance } else { 1e9 } }
            } | Sort-Object Dist)
            $to = if ($near.Count) { $near[0].Row } else { $rows[0] }
            $did.Add("to $($to.Base) for $($to.Good) (+$($to.Gain)/unit)")
            if ($mode -eq 'docked') { $null = Start-FLLaunch }
            $r = Start-FLTravel ($to.Base -replace '_base$', '')
            if ((Get-FLMode) -ne 'docked') { $did.Add("travel: $($r.Why)"); break }
            continue
        }
        $did.Add("buy $($here.Good) at $($here.Base): $(Invoke-FLTraderBuy @{ $here.Good = 0 })")
        $after = Get-FLWorthNeeded
        $did.Add("needed $need -> $after")
        break   # one purchase per plan: the hold is full or the gate is met; the story decides on its next check
    }
    $did -join '; '
}

# Credits still needed before the story moves on: the game's latest target (host event `worth-target`, set by the
# story code at a mission's end) minus the net worth now; $null without a target in the host's events (e.g. a game
# loaded mid free time: the target is set at the mission's end only).
function Get-FLWorthNeeded {
    $t = Get-FLHistory ' worth-target ' | Select-Object -Last 1
    if (-not $t -or "$t" -notmatch 'worth-target (-?\d+)') { return $null }
    $target = [double]$Matches[1]
    $w = Get-FLWorth
    if (-not $w) { return $null }
    [Math]::Max(0, $target - $w.Total)
}

# Can the player's ship dock with this object? The game's own rule, from its data (user, 18 Oct 2026: use a dock only
# when docking is explicitly possible): the ship's `mission_property` (SHIPS\shiparch.ini: can_use_berths,
# can_use_med_moors, can_use_large_moors) against the object's docking spheres (its system file [Object] archetype ->
# DATA\SOLAR\solararch.ini docking_sphere = berth | ring | moor_medium | moor_large | jump | airlock). Berth ships use
# berths and docking rings; mooring fixtures take moor ships only (a mod such as adoxa's Moors changes the ships'
# mission_property, and this follows). $Ship: the ship's archetype id (default: the player's, from `pilot`; no
# reading = berths). Unknown object = no. Cached per object: the set of its sphere types.
function Test-FLSmallShipDock([uint32]$Id, [uint32]$Ship = 0) {
    $data = Join-Path $script:FLRoot 'Install\Freelancer HD Edition\DATA'
    if (-not $script:FLShipDockKinds) {
        $kinds = @{}; $cur = $null
        foreach ($l in Get-Content (Join-Path $data 'SHIPS\shiparch.ini')) {
            if ($l -match '^\s*\[') { $cur = $null }
            elseif ($l -match '^\s*nickname\s*=\s*(\S+)') { $cur = [uint32](Get-FLHash $Matches[1].ToLower()) }
            elseif ($cur -and $l -match '^\s*mission_property\s*=\s*can_use_(\w+)') {
                $kinds[$cur] = switch ($Matches[1].ToLower()) { 'berths' { @('berth', 'ring') } 'med_moors' { @('moor_medium') }
                                                                  'large_moors' { @('moor_large') } default { @() } }
            }
        }
        $script:FLShipDockKinds = $kinds
    }
    if (-not $Ship) { $p = Get-FLPilot; if ($p) { $Ship = $p.Arch } }
    $can = if ($Ship -and $script:FLShipDockKinds.ContainsKey($Ship)) { $script:FLShipDockKinds[$Ship] } else { @('berth', 'ring') }
    if (-not $script:FLSmallDock) {
        $arch = @{}; $cur = $null
        foreach ($l in Get-Content (Join-Path $data 'SOLAR\solararch.ini')) {
            if ($l -match '^\s*nickname\s*=\s*(\S+)') { $cur = $Matches[1].ToLower() }
            elseif ($cur -and $l -match '^\s*docking_sphere\s*=\s*(\w+)') {
                if (-not $arch[$cur]) { $arch[$cur] = @() }
                $arch[$cur] += $Matches[1].ToLower()
            }
        }
        $map = @{}
        foreach ($l in Get-Content (Join-Path $data 'UNIVERSE\universe.ini')) {
            # system files: `file = Systems\Li01\Li01.ini` (relative to UNIVERSE; base files `Universe\Systems\..\Bases\..`
            # are relative to DATA and skipped)
            if ($l -notmatch '^\s*file\s*=\s*(systems\\[^\\]+\\[^\\]+\.ini)\s*$') { continue }
            $f = Join-Path (Join-Path $data 'UNIVERSE') $Matches[1]
            if (-not (Test-Path -LiteralPath $f)) { continue }
            $nick = $null
            foreach ($o in Get-Content -LiteralPath $f) {
                if ($o -match '^\s*\[') { $nick = $null }
                elseif ($o -match '^\s*nickname\s*=\s*(\S+)') { $nick = $Matches[1].ToLower() }
                elseif ($nick -and $o -match '^\s*archetype\s*=\s*(\S+)') { $map[[uint32](Get-FLHash $nick)] = @($arch[$Matches[1].ToLower()]) }
            }
        }
        $script:FLSmallDock = $map
    }
    $types = $script:FLSmallDock[$Id]
    [bool]($types -and @($types | Where-Object { $_ -in $can }).Count)
}

# Where to look for work (game data, user 18 Oct 2026: the player goes to the base with the best jobs they can do
# when the trip is worth it): the bases of this system with Kill Ships jobs (DATA\MISSIONS\mbases.ini DestroyMission),
# their best pay (DATA\RandomMissions\diff2money.ini at the faction's top difficulty), docked through a friendly
# object (attitude > 0; only docks a small ship can use: Test-FLSmallShipDock - mooring fixtures are for large ships),
# ranked by the time to the goal (user, 18 Oct 2026): the planner's route time to the
# board (debug `eta`: lanes, fast docks - not the straight distance) + the jobs needed (ceil($Need / pay)) x $JobSec
# (one job: to its site, the fight, back to the board). A farther board that pays more wins when it saves jobs.
# $Need: credits still to earn (0: one job). $Skip: dock ids refused before. Best first.
function Get-FLJobBases([double]$Need = 0, [double]$JobSec = 240, [uint32[]]$Skip = @(), [string]$Stage = 'FreeTime_01_02') {
    # Expected pay, not the maximum: single-player offers are drawn from a window set by the story stage (adoxa's RE,
    # Starport KnowledgeBase "random missions editing": rank_diff = rankdiff.ini[stage], f = 1.3689^3; hi = maxDiff, lo =
    # max(minDiff, rank_diff / f) - or max(minDiff, maxDiff / f) when rank_diff x f >= maxDiff; none when rank_diff x f
    # <= minDiff; difficulty = hi - cos(rnd x pi/2) x (hi - lo)), weighted by the board's faction and type weights.
    # Checked against the boards seen after M01b: Manhattan 2013-2655 (offers 2000-2550), Navy up to 3219 (the user).
    if ($script:FLJobStage -ne $Stage) { $script:FLJobData = $null; $script:FLJobStage = $Stage }
    if (-not $script:FLJobData) {
        $data = Join-Path $script:FLRoot 'Install\Freelancer HD Edition\DATA'
        $money = @(Get-Content (Join-Path $data 'RandomMissions\diff2money.ini') | Where-Object { $_ -match '^Diff2Money = ([\d.]+), (\d+)' } |
                   ForEach-Object { $null = $_ -match '^Diff2Money = ([\d.]+), (\d+)'; , @([double]$Matches[1], [double]$Matches[2]) })
        $pay = {
            param($d)
            for ($i = 1; $i -lt $money.Count; $i++) {
                if ($d -le $money[$i][0]) { $f = ($d - $money[$i - 1][0]) / ($money[$i][0] - $money[$i - 1][0]); return $money[$i - 1][1] + $f * ($money[$i][1] - $money[$i - 1][1]) }
            }
            $money[-1][1]
        }
        $rank = $null
        foreach ($l in Get-Content (Join-Path $data 'MISSIONS\rankdiff.ini')) {
            if ($l -match "^\s*rank_diff\s*=\s*$Stage\s*,\s*([\d.]+)") { $rank = [double]$Matches[1] }
        }
        $f = [Math]::Pow(1.3689, 3)
        # expected pay of one DestroyMission range (16-point average over rnd)
        $expect = {
            param($minD, $maxD)
            if ($null -eq $rank) { return & $pay $maxD }   # stage unknown: the board's top
            $d2 = [Math]::Min(100, $rank * $f)
            if ($d2 -le $minD) { return 0 }
            $lo = if ($d2 -ge $maxD) { [Math]::Max($minD, $maxD / $f) } else { [Math]::Max($minD, [Math]::Min(100, $rank / $f)) }
            $s = 0
            for ($i = 0; $i -lt 16; $i++) { $s += & $pay ($maxD - [Math]::Cos(($i + 0.5) / 16 * [Math]::PI / 2) * ($maxD - $lo)) }
            $s / 16
        }
        $bases = @{}; $base = $null; $fw = 0; $acc = @{}
        foreach ($l in Get-Content (Join-Path $data 'MISSIONS\mbases.ini')) {
            if ($l -match '^nickname = (\S+)') { $base = [uint32](Get-FLHash $Matches[1].ToLower()) }
            elseif ($l -match '^weight = ([\d.]+)') { $fw = [double]$Matches[1] }
            elseif ($l -match '^mission_type = DestroyMission, ([\d.]+), ([\d.]+), ([\d.]+)' -and $base) {
                $e = & $expect ([double]$Matches[1]) ([double]$Matches[2])
                if ($e -gt 0) {
                    $w = $fw * [double]$Matches[3]
                    if (-not $acc[$base]) { $acc[$base] = @(0.0, 0.0) }
                    $acc[$base] = @(($acc[$base][0] + $e * $w), ($acc[$base][1] + $w))
                }
            }
        }
        foreach ($k in $acc.Keys) { if ($acc[$k][1] -gt 0) { $bases[$k] = $acc[$k][0] / $acc[$k][1] } }
        $script:FLJobData = $bases
    }
    $docks = Get-FLContacts -Dock | Where-Object { $script:FLJobData[$_.Base] -and $_.Id -notin $Skip -and (Test-FLSmallShipDock $_.Id) }
    $picked = @($docks | Group-Object Base | ForEach-Object {
        $_.Group | Sort-Object @{ e = { $_.Type -ne 'dockring' } }, Distance | Where-Object {
            [double]([regex]::Match((Send-FL "attitude $($_.Id)"), ' (-?[\d.]+)$').Groups[1].Value) -gt 0 } | Select-Object -First 1
    })
    if (-not $picked.Count) { return }
    $eta = @{}
    foreach ($l in ((Send-FL ("eta " + (($picked | ForEach-Object { $_.Id }) -join ' '))) -split "`n")) {
        if ($l -match '^eta (\d+) (-?[\d.]+)') { $eta[[uint32]$Matches[1]] = [double]$Matches[2] }
    }
    $ranked = foreach ($d in $picked) {
        $p = $script:FLJobData[$d.Base]
        $t = if ($eta[$d.Id] -gt 0) { $eta[$d.Id] } else { $d.Distance / 300.0 }   # no plan: straight at cruise
        $jobs = [Math]::Max(1, [Math]::Ceiling($Need / $p))
        # a job's time: measured (research\experience\jobs.jsonl, nearest difficulty) unless -JobSec is given
        $js = if ($PSBoundParameters.ContainsKey('JobSec')) { $JobSec } else { Get-FLJobSeconds (Get-FLJobDifficulty $p) $JobSec }
        [pscustomobject]@{ Dock = $d.Id; Name = $d.Name; Base = $d.Base; Distance = $d.Distance; Pay = [int]$p
                           Travel = [int]$t; Jobs = $jobs; JobSec = [int]$js; Total = [int]($t + $jobs * $js) }
    }
    $ranked | Sort-Object Total
}

# Opens the job board of the base you are docked at (Bar), takes the best paid visible offer of the given types and
# accepts it. Returns the offer. The offers must have been created in this event log (dock / load after
# Start-FLEventLog); a board of another base throws.
function Start-FLJob([int[]]$Types = @(1)) {
    $offers = @(Get-FLJobOffers)
    if (-not $offers.Count) { throw 'no job offers in the event log (dock or load after Start-FLEventLog)' }
    $pick = $offers | Where-Object { $_.Row -lt 5 -and $_.Type -in $Types } | Sort-Object Reward -Descending | Select-Object -First 1
    if (-not $pick) { throw "no visible job of type $($Types -join ',') ($(($offers | ForEach-Object { "$($_.Type):$($_.Reward)" }) -join ' '))" }
    if ((Send-FL 'hotspots') -notmatch 'IDS_HOTSPOT_MISSIONVENDOR') { $null = Goto-FLRoom Bar }
    $r = Send-FL 'goto IDS_HOTSPOT_MISSIONVENDOR'; if ($r -notmatch '^clicked') { throw "job board: $r" }
    $null = Wait-FLUntil { (Send-FL 'visible MissionVendorAcceptButton') -match '^visible' } 'job board open' 10
    $r = Send-FL "click MissionVendorSummaryBox$($pick.Row)"; if ($r -notmatch '^clicked') { throw "job row: $r" }
    Update-FLEventLog
    $mark = $script:FLLogSeq
    $r = Send-FL 'click MissionVendorAcceptButton'; if ($r -notmatch '^clicked') { throw "accept: $r" }
    $null = Wait-FLUntil { @(Get-FLEventLog $mark | Where-Object { "$_" -match 'objective count=\d+ title=\d+ "[^"]+" .*obj=13081' }).Count } 'job accepted' 20
    $pick | Add-Member -NotePropertyName Since -NotePropertyValue $mark -PassThru
}

# Flies an accepted Kill Ships job: launch, the fastest route to where its enemies wait (the job's last navmarker:
# objective `nav=` / `objective-step ... nav=`), combat there kept within $Area of that point (a job fails "You ran
# from the battle" once you leave: the autopilot's flee is off for it), until the job's result. Then docks at $DockAt
# (a base object / dock ring; default: none). $Since: the event log position at the accept (Start-FLJob's .Since).
# Returns @{ Result; Cash; Secs }.
function Invoke-FLJob([int]$Since, $DockAt, [double]$Area = 3500, [int]$TimeoutSec = 900, $Offer) {
    $t0 = Get-Date
    if ((Get-FLMode) -eq 'docked') { $null = Start-FLLaunch }
    $st = @{ result = $null; cash = 0; seen = $null }
    $null = Send-FL 'ap set flee 0'; $null = Send-FL 'ap set caution 0'
    $null = Send-FL "ap set engage_radius $($Area + 500)"   # the autopilot's reach covers what the loop calls a fight
    try {
        $null = Wait-FLUntil {
            $null = Close-FLBlocker
            foreach ($l in (Get-FLEventLog $Since)) {
                if ("$l" -match 'player-death') { throw "job: player died" }
                if ("$l" -match 'mission-message type=(FAILURE|SUCCESS)') { $st.result = $Matches[1] }
                if ("$l" -match ' cash (-?\d+)') { $st.cash = [int]$Matches[1] }
            }
            if ($st.result) { return $true }
            $p = Get-FLPilot
            if (-not $p) { return $false }
            $goal = Get-FLJobGoal $Since
            if (-not $goal) { return $false }
            $d = Get-FLDistance $p.Pos $goal
            $mode = (Get-FLAutopilot).Mode
            $near = @(Get-FLContacts -Hostile | Where-Object { $_.Distance -lt $Area })
            if ($near.Count) {
                # A fight: combat (never the travel's cruise among hostiles - user, 18 Oct 2026), restarted only when
                # the hostiles have changed since it last stopped ("no enemies in reach" otherwise restarts every poll).
                $now = ($near | ForEach-Object { $_.Id } | Sort-Object) -join ','
                if ($mode -eq 'travel') { $null = Send-FL 'ap stop'; $mode = 'off'; $st.seen = $null }
                if ($mode -eq 'off' -and $now -ne $st.seen) { $st.seen = $now; $null = Send-FL 'ap start combat' }
            } elseif ($d -gt 1500 -and $mode -eq 'off') {
                $st.seen = $null
                $null = Send-FL ("ap start travel {0} {1} {2}" -f $goal[0], $goal[1], $goal[2])
            }
            $false
        } 'job result' $TimeoutSec
    } finally {
        $null = Send-FL 'ap stop'
        $null = Send-FL 'ap set flee -1'; $null = Send-FL 'ap set caution 1'; $null = Send-FL 'ap set engage_radius 3000'
    }
    if ($DockAt -and $st.result -eq 'SUCCESS') { $null = Start-FLTravel $DockAt }
    $res = [pscustomobject]@{ Result = $st.result; Cash = $st.cash; Secs = [int]((Get-Date) - $t0).TotalSeconds }
    if ($Offer) { Add-FLJobRecord $Offer $res }
    $res
}
