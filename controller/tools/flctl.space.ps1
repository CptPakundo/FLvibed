# FLController harness - contacts, targets, docks, the game's maneuvers (goto, lanes), cruise and teleport test aids. Part of flctl.ps1 (dot-sourced by it; never load a part alone).

# Space objects of the player's system from the game's own object registry (debug `contacts`, core\game\space.cpp),
# independent of the HUD contact list filter: objects with Id, Type, Distance, Dock, Ship, Base, Health (hull 0..1),
# Attitude (ships: towards the player, -1 hostile .. 1 friendly), TargetId (ships: what they have targeted),
# Attacker (ships that have the player targeted), Name, nearest first.
# -Dock: only objects the player can dock with (bases, docking rings). -Hostile: hostile ships only.
function Get-FLContacts([switch]$Dock, [switch]$All, [switch]$Hostile) {
    $arg = if ($Dock) { 'dock' } elseif ($All) { 'all' } else { '' }
    $lines = (Send-FL "contacts $arg") -split "`n"
    $player = if ($lines[0] -match 'player=(\d+)') { [uint32]$matches[1] } else { 0 }
    $list = $lines | Select-Object -Skip 1 | Where-Object { $_ } | ForEach-Object {
        $m = [regex]::Match($_, '^(\d+) (\S+) dist=(\d+)( dock)?( ship)? base=(\d+)(?: health=(-?[\d.]+))?(?: att=(-?[\d.]+) target=(\d+))?(?: maxhit=(\d+) good=0x([0-9A-F]+) units=(\d+) fits=(-?\d+) value=(\d+))? name=(.*)$')
        if ($m.Success) {
            [pscustomobject]@{ Id = [uint32]$m.Groups[1].Value; Type = $m.Groups[2].Value; Distance = [int]$m.Groups[3].Value
                               Dock = $m.Groups[4].Success; Ship = $m.Groups[5].Success; Base = [uint32]$m.Groups[6].Value
                               Health = $(if ($m.Groups[7].Success) { [double]$m.Groups[7].Value } else { -1 })
                               Attitude = $(if ($m.Groups[8].Success) { [double]$m.Groups[8].Value } else { 0 })
                               TargetId = $(if ($m.Groups[9].Success) { [uint32]$m.Groups[9].Value } else { 0 })
                               Attacker = $false
                               MaxHit = $(if ($m.Groups[10].Success) { [double]$m.Groups[10].Value } else { 0 })
                               Good = $(if ($m.Groups[11].Success) { [Convert]::ToUInt32($m.Groups[11].Value, 16) } else { 0 })
                               Units = $(if ($m.Groups[12].Success) { [int]$m.Groups[12].Value } else { 0 })
                               Fits = $(if ($m.Groups[13].Success) { [int]$m.Groups[13].Value } else { -1 })
                               Value = $(if ($m.Groups[14].Success) { [double]$m.Groups[14].Value } else { 0 })
                               Name = $m.Groups[15].Value }
        }
    }
    foreach ($c in $list) { if ($player -and $c.TargetId -eq $player) { $c.Attacker = $true } }
    if ($Hostile) { $list = $list | Where-Object { $_.Ship -and $_.Attitude -le -0.6 } }
    $list
}

# A contact of the current system by object id, displayed name or script / system nickname (solars: the hash of the
# lowercase nickname); throws when there is none.
function Resolve-FLContact($Target) {
    $all = Get-FLContacts -All
    $c = if ($Target -is [string] -and $Target -notmatch '^\d+$') {
        $byName = $all | Where-Object Name -eq $Target | Select-Object -First 1
        if ($byName) { $byName } else {
            $id = Get-FLHash $Target.ToLower()
            $all | Where-Object Id -eq $id | Select-Object -First 1
        }
    } else { $all | Where-Object Id -eq ([uint32]$Target) | Select-Object -First 1 }
    if (-not $c) { throw "no contact '$Target' in this system (Get-FLContacts -All)" }
    $c
}

# Makes a space object the player's target natively (the game's own target action, as a contact list click does;
# works whatever the contact list filter shows). $Target: an object id, a displayed name ('Fort Bush'), or a mission
# script / system file nickname ('Li01_02_dock_ring': a solar's space id is the hash of its lowercase nickname).
# Returns the contact.
function Set-FLTarget($Target) {
    $c = Resolve-FLContact $Target
    # A story popup pauses the game and the selection never lands (M01a 21855 / 21850 come with their objective, 18 Oct
    # 2026): close blockers first, and again while waiting.
    $null = Close-FLBlocker
    $r = Send-FL "settarget $($c.Id)"; if ($r -notmatch '^target set') { throw $r }
    $null = Wait-FLUntil {
        if (Close-FLBlocker) { $null = Send-FL "settarget $($c.Id)" }
        (Send-FL 'targetinfo') -match "id=$($c.Id) "
    } "target $($c.Name) ($($c.Id))" 5
    $c
}

# Docks with a base the way a player does: target it (natively), request docking (USER_MANEUVER_DOCK: the game's dock
# autopilot flies the approach), then the docking / landing cutscene, skipped as Escape does unless -Watch. $Target:
# a name or id (default: the nearest dockable object). Waits until docked with the room ready. The approach is flown
# by the game at its own speed (it ignores cruise): from 2 km about 24 s. From farther away (or with hazards in
# between) the ship first follows a hazard-free route into the dock request range (Move-FLToDock).
function Start-FLDock($Target, [switch]$Watch, [int]$TimeoutSec = 180) {
    if ((Get-FLMode) -notin 'space', 'space-ui') { throw "not in flight: $(Get-FLStoryState)" }
    # docking ends the space scene and its comms: let the mission's lines play out first (dialogue buffer)
    $null = Wait-FLDialogue ([Math]::Max(0, $script:FLLogSeq - 400))
    if ((Get-FLAutopilot).Mode -ne 'off') { $null = Send-FL 'ap stop' } # this action replaces the autopilot's
    $c = if ($Target) { Move-FLToDock $Target } else {
        $n = Get-FLContacts -Dock | Select-Object -First 1
        if (-not $n) { throw 'no dockable object in this system' }
        Move-FLToDock $n.Id
    }
    # The dock maneuver is a toggle (a second press cancels a running dock) and a handled key press does not mean the
    # request went out: the base's comm reply to the player ("request to dock is granted", comm event from the base)
    # is the game's answer. The key is refused (not handled) while the ship is still leaving a dock after a launch:
    # pressed again only then.
    $player = Get-FLPeek32 0x673378
    $ev = [int]([regex]::Match((Send-FL 'events 999999999'), '^last (\d+)').Groups[1].Value)
    $null = Wait-FLUntil { (Send-FL 'tap USER_MANEUVER_DOCK') -match 'down->1' } 'dock key accepted' 15
    $null = Wait-FLUntil {
        foreach ($e in (Get-FLEvents -After $ev)) { if ($e -match "comm from=$($c.Id) to=$player ") { return $true } }
        (Get-FLMode) -in 'cutscene', 'docked'
    } "docking reply from $($c.Name)" 30
    $st = @{ skipped = [bool]$Watch }
    $null = Wait-FLUntil {
        if (Close-FLBlocker) { return $false }
        $m = Get-FLMode
        if ($m -eq 'menu') { throw "docking with $($c.Name) ended in the menus (death?): $(Get-FLStoryState)" }
        if ($m -eq 'docked') { return (((Send-FL 'hotspots') -split "`n")[-1]) -match 'hotspots [1-9]' }
        if (-not $st.skipped -and $m -eq 'cutscene') { $st.skipped = Skip-FLAnimation }
        $false
    } "docked at $($c.Name)" $TimeoutSec
    Wait-FLRoomReady -Watch:$Watch
    "docked at $($c.Name)"
}

# The player ship's autopilot from the game's own state (debug `pilot`, core\game\space.cpp): Op = the running
# behaviour (freeflight, goto, dock (also during a trade lane ride and a gate / hole approach), formation, ...),
# Locked (Act_LockManeuvers), Lane / Entry / Exit (trade lane ride: the ring entered, the next ring), Jumping, Cruise,
# Leader (formation leader id), Target, System (id), Pos. $null when not in space.
function Get-FLPilot {
    $r = Send-FL 'pilot'
    $m = [regex]::Match($r, '^pilot op=(-?\d+) (\S+) locked=(\d) lane=(\d) entry=(\d+) exit=(\d+) jumping=(\d) ' +
                            'cruise=(\d) leader=(\d+) target=(\d+) system=(\d+) dockrange=(\d+) ' +
                            'cruisespeed=(\d+) pos=\((-?\d+),(-?\d+),(-?\d+)\)')
    if (-not $m.Success) { return $null }
    $arch = [regex]::Match($r, ' arch=(\d+)')   # the ship's archetype id (newer DLLs)
    $g = $m.Groups
    [pscustomobject]@{ Op = $g[2].Value; Locked = $g[3].Value -eq '1'; Lane = $g[4].Value -eq '1'
                       Entry = [uint32]$g[5].Value; Exit = [uint32]$g[6].Value; Jumping = $g[7].Value -eq '1'
                       Cruise = $g[8].Value -eq '1'; Leader = [uint32]$g[9].Value; Target = [uint32]$g[10].Value
                       System = [uint32]$g[11].Value; DockRange = [double]$g[12].Value
                       CruiseSpeed = [double]$g[13].Value
                       Pos = [double[]]@($g[14].Value, $g[15].Value, $g[16].Value)
                       Arch = $(if ($arch.Success) { [uint32]$arch.Groups[1].Value } else { 0 }) }
}

# Test aid (opt-in, never a default): a cruise speed boost for long flights, through HD Edition's own console
# command `cspd` (it also raises the physics speed cap); a game restart restores 300 (constants.ini). Faster cruise
# overshoots more (user, 17 Oct 2026): the flight helpers fly boosted only far from their current goal and switch
# back to the normal speed within $BrakeSec seconds of boosted flight from it (Update-FLCruiseBoost, live distance).
# The boost is remembered per game process. Mission timers do not scale with it. Set-FLCruiseSpeed 300 ends it.
$script:FLNormalCruise = 300
function Get-FLBoostFile { $p = Get-FLGameProcess
                           if ($p) { Join-Path $env:TEMP "flctl_cspd_$($p.Id)" } }
function Set-FLCruiseSpeed([double]$Speed = 300, [double]$BrakeSec = 15) {
    $f = Get-FLBoostFile
    if ($Speed -gt $script:FLNormalCruise) { Set-Content -LiteralPath $f "$Speed $BrakeSec" } else { Remove-Item -LiteralPath $f -ErrorAction SilentlyContinue }
    Set-FLCruise $Speed
}
function Set-FLCruise([double]$Speed) {   # the game's value now (Get-FLPilot CruiseSpeed)
    Send-FLConsole "cspd $Speed"
    $null = Wait-FLUntil { $p = Get-FLPilot; $p -and [Math]::Abs($p.CruiseSpeed - $Speed) -lt 1 } "cruise speed $Speed" 5
}

# Test aid for pinpoint tests only (user, 17 Oct 2026), never in tests that follow a mission's flow: relocates the
# player's ship next to an object of the current system with HD Edition's console command `pos x, y, z`. The spot
# accounts for the object's size: its radius as the game reports it (`targetinfo`) x 1.5 + $Clearance, on the side
# facing the ship, turned around the object until the game's zone test finds it outside every hazard
# (flroute.ps1). No teleport when the ship is already that close. (17 Oct 2026: after the console's own `jump` put
# the ship at a jump gate it died within seconds.) Waits until the game reports the ship there. Returns the pilot
# state.
function Move-FLShipNear($Target, [double]$Clearance = 800) {
    $c = Set-FLTarget $Target
    $info = Send-FL 'targetinfo'
    $at = Get-FLTargetPos
    $radius = [double][regex]::Match($info, 'radius=(\d+)').Groups[1].Value
    $p = Get-FLPilot
    $stand = $radius * 1.5 + $Clearance
    if ((Get-FLDistance $p.Pos $at) -le $stand * 1.5) { return $p }
    $sys = Get-FLSystemHazards $p.System
    $base =[Math]::Atan2($p.Pos[2] - $at[2], $p.Pos[0] - $at[0])
    $dest = $null
    foreach ($k in 0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5, 6) {   # 30 degree steps around the object
        $a = $base + $k * [Math]::PI / 6
        $cand = [double[]]@(($at[0] + $stand * [Math]::Cos($a)), $at[1], ($at[2] + $stand * [Math]::Sin($a)))
        if (-not (Test-FLSegment $cand $cand $sys -IncludeStart)) { $dest = $cand; break }
    }
    if (-not $dest) { throw "no hazard-free spot $([int]$stand) m around $($c.Name)" }
    Send-FLConsole ('pos {0:0}, {1:0}, {2:0}' -f $dest[0], $dest[1], $dest[2])
    Wait-FLUntil { $x = Get-FLPilot; if ($x -and (Get-FLDistance $x.Pos $dest) -lt 200) { $x } } "ship near $($c.Name)" 10
}

# Called by the flight helpers' waits with the live distance to their current goal: boosted cruise while farther
# than the braking distance (boost x BrakeSec), the normal speed inside it. Changes the game's value only on a
# transition. No boost set: does nothing.
function Update-FLCruiseBoost([double]$Distance) {
    $f = Get-FLBoostFile
    if (-not $f -or -not (Test-Path -LiteralPath $f)) { return }
    $boost, $brakeSec = (Get-Content -LiteralPath $f) -split ' ' | ForEach-Object { [double]$_ }
    $want = if ($Distance -gt $boost * $brakeSec) { $boost } else { $script:FLNormalCruise }
    $p = Get-FLPilot
    if ($p -and [Math]::Abs($p.CruiseSpeed - $want) -ge 1) { Set-FLCruise $want }
}

# Flies a hazard-free route to a dockable object until it is within the game's dock request range (DockRange, read
# from the game), then targets it. Returns the contact. Used before every dock maneuver (bases, lane rings, gates).
function Move-FLToDock($Target) {
    $null = Set-FLTarget $Target
    $null = Move-FLToward (Get-FLTargetPos) -Range (Get-FLPilot).DockRange
    Set-FLTarget $Target
}

# Position of the current target (debug `targetinfo`), $null without a target.
function Get-FLTargetPos {
    $m = [regex]::Match((Send-FL 'targetinfo'), 'pos=\((-?[\d.]+),(-?[\d.]+),(-?[\d.]+)\)')
    if ($m.Success) { [double[]]@($m.Groups[1].Value, $m.Groups[2].Value, $m.Groups[3].Value) }
}

function Get-FLDistance([double[]]$A, [double[]]$B) {
    [Math]::Sqrt([Math]::Pow($A[0] - $B[0], 2) + [Math]::Pow($A[1] - $B[1], 2) + [Math]::Pow($A[2] - $B[2], 2))
}

# Starts one of the player's maneuvers on the current target the way the player's key does (USER_MANEUVER_*), and
# returns the pilot state once the game's autopilot runs it ($Op as Get-FLPilot names it). The maneuver keys are
# toggles (a second press cancels the running maneuver): pressed only while $Op is not running, and again only
# while the game does not handle the press (e.g. still leaving a dock after a launch). "Handled" does not mean
# accepted: throws when the op does not start (maneuvers locked by the mission, no target, refused).
function Start-FLManeuver([string]$Command, [string]$Op, [int]$TimeoutSec = 15) {
    $p = Get-FLPilot
    if (-not $p) { throw "not in flight: $(Get-FLStoryState)" }
    $tm = [regex]::Match((Send-FL 'targetinfo'), ' id=(\d+) ')
    $tgt = if ($tm.Success) { [uint32]$tm.Groups[1].Value } else { 0 }
    if ($p.Op -eq $Op) {
        # Running on the current target: nothing to do. A leftover of an earlier maneuver (e.g. `dock` after a jump-in,
        # target cleared) would be toggled off by a press: end it first (free flight).
        if ($p.Target -eq $tgt -or $Op -eq 'freeflight') { return $p }
        $null = Wait-FLUntil { (Send-FL 'tap USER_MANEUVER_FREE_FLIGHT') -match 'down->1' } 'free flight' $TimeoutSec
        $null = Wait-FLFrames 5 { (Get-FLPilot).Op -ne $Op }
        if ($tgt) { $null = Send-FL "settarget $tgt" }   # free flight keeps the target; set again to be sure
    }
    $null = Wait-FLUntil { (Send-FL "tap $Command") -match 'down->1' } "$Command handled" $TimeoutSec
    # The op starts in the frame of the press (seen in game): a refusal shows once a few game frames have passed.
    if (Wait-FLFrames 5 { $x = Get-FLPilot; $x -and $x.Op -eq $Op }) { return Get-FLPilot }
    $p = Get-FLPilot
    throw "$Command handled, but the autopilot did not start $Op (op=$($p.Op) locked=$($p.Locked) target=$($p.Target)): $(Get-FLStoryState)"
}

# The Neural Net lines with which the game refuses a maneuver request (DATA\AUDIO voices: `nn-message <hash>`
# events; seen 17 Oct 2026: `info_access_denied` at the New York -> Alaska gate). `nnv_dock_denied_standing_by` is
# a queue, not a refusal.
$script:FLRefusals = @{}
foreach ($n in 'info_access_denied', 'nnv_dock_too_far', 'nnv_dock_denied_destination_hostile',
               'nnv_formation_request_denied', 'nnv_jump_gate_access_denied', 'nnv_trade_lane_access_denied',
               'nnv_trade_lane_end_docking_not_allowed') {
    $script:FLRefusals[[uint32](Get-FLHash $n)] = $n
}

# The event feed's current position (for Get-FLRefusal / Get-FLEvents -After).
function Get-FLEventSeq { [int]([regex]::Match((Send-FL 'events 999999999'), '^last (\d+)').Groups[1].Value) }

# The name of a refusal line (see FLRefusals) the game played after event $After, or $null.
function Get-FLRefusal([int]$After) {
    foreach ($l in (Send-FL "events $After") -split "`n") {
        $m = [regex]::Match($l, 'nn-message (\d+)')
        if ($m.Success -and $script:FLRefusals.ContainsKey([uint32]$m.Groups[1].Value)) { return $script:FLRefusals[[uint32]$m.Groups[1].Value] }
    }
    $null
}

# Throws when a flight helper's wait must stop: the game left space for the menus (death, mission failure), or
# (with -Since, an event position) it refused the request with a Neural Net line.
function Assert-FLFlying([string]$What, [int]$Since = -1) {
    if ((Get-FLMode) -eq 'menu') { throw "$What ended in the menus (death or mission failure?): $(Get-FLStoryState)" }
    if ($Since -ge 0) {
        $why = Get-FLRefusal $Since
        if ($why) { throw "$What refused by the game: $why" }
    }
}

# Flies to an object or a position with the game's own Goto autopilot (USER_MANEUVER_GOTO, as the player's Goto
# button; it uses cruise on its own). $Target: name, id or script nickname (Set-FLTarget). -Position x,y,z: a
# point of the current system, set as the player's waypoint the way the nav map does (this replaces the player's
# waypoint list: mission waypoints are a HUD aid only, mission triggers test distances). The autopilot flies straight
# through anything, so the flight first follows a hazard-free route (flroute.ps1: minefields, danger fields,
# damage zones from the system's files, the game's own zone test); -Direct skips that (the caller planned it).
# Waits until the autopilot ends the Goto by itself (arrival: about 100 m from a waypoint, which is then removed;
# ~360 m from a station's centre) or, with -Within, until the ship is that close to the target. -NoWait returns once
# the (last) Goto runs. Returns the pilot state.
# -Objective: the first waypoint of the player's list as the game has it (the mission objective's navmarker, set by
# the script), selected natively as its contact list row would be.
function Start-FLGoto($Target, [double[]]$Position, [switch]$Objective, [double]$Within = 0, [switch]$NoWait,
                      [switch]$Direct, [int]$TimeoutSec = 600) {
    if ($Position -and $Position.Count -ne 3) { throw '-Position needs x,y,z' }
    # A position the mission already marks (its waypoint list's first entry within 1.5 km) is flown to by that waypoint,
    # never by a second, player waypoint on top of it (user, 19 Oct 2026; the 2020 footage shows the mission's alone).
    # A new objective's navmarker reaches the list a few frames after it: an empty list is waited for briefly.
    if ($Position -and -not $Objective) {
        $wl = @{ first = $null } # (a hashtable: the wait's script block runs in its own scope)
        try {
            $null = Wait-FLUntil {
                $m = [regex]::Match((Send-FL 'routes'), '(?m)^ 0 pos (-?[\d.]+) (-?[\d.]+) (-?[\d.]+)')
                if ($m.Success) { $wl.first = @([double]$m.Groups[1].Value, [double]$m.Groups[2].Value, [double]$m.Groups[3].Value) }
                $m.Success
            } 'the mission waypoint list' 2
        } catch { }   # no list: a free position, the player's own waypoint as before
        if ($wl.first -and (Get-FLDistance $wl.first $Position) -lt 1500) { $Objective = $true; $Position = $null }
    }
    $selectObjective = {
        $r = Send-FL 'waypoint first'
        if ($r -notmatch '^waypoint \d+') { throw "objective waypoint: $r" }
    }
    if (-not $Direct) {
        $goal = if ($Position) { $Position } elseif ($Objective) { & $selectObjective; Get-FLTargetPos } else {
            $null = Set-FLTarget $Target; Get-FLTargetPos }
        $null = Move-FLToward $goal
    }
    # Selects the goto's target; returns its space id (checked: a waypoint must sit where it was asked for).
    $aim = {
        if ($Objective) {
            & $selectObjective
        } elseif ($Position) {
            $r = Send-FL ('waypoint {0} {1} {2}' -f $Position[0], $Position[1], $Position[2])
            if ($r -notmatch '^waypoint \d+') { throw "waypoint: $r" }
            $tp = Get-FLTargetPos
            if (-not $tp -or (Get-FLDistance $tp $Position) -gt 5) { throw "waypoint not selected as the target: $(Send-FL 'targetinfo')" }
        } else {
            $null = Set-FLTarget $Target
        }
        [uint32][regex]::Match((Send-FL 'targetinfo'), ' id=(\d+) ').Groups[1].Value
    }
    $what = if ($Objective) { 'the objective waypoint' } elseif ($Position) { "position $($Position -join ',')" } else { "$Target" }
    $st = @{ id = & $aim; again = 0 }
    $goal = if ($Position) { $Position } elseif ($Objective) { Get-FLTargetPos } else { $null }
    $null = Start-FLManeuver USER_MANEUVER_GOTO goto
    if ($NoWait) { return Get-FLPilot }
    $end = Wait-FLUntil {
        $null = Close-FLBlocker
        Assert-FLFlying "goto $what"
        $p = Get-FLPilot
        if (-not $p) { return $null }   # cutscene or space UI: keep waiting
        $at = if ($goal) { $goal } else { Get-FLTargetPos }
        $d = if ($at) { Get-FLDistance $p.Pos $at } else { [double]::MaxValue }
        if ($Within -gt 0 -and $d -le $Within) { return $p }
        if ($p.Op -ne 'goto') { return $p }
        if ($p.Target -ne $st.id) {
            # The goto lost its target (the HUD drops a waypoint within 150 m; the mission may change it): a goto
            # without a target flies straight ahead. Close enough = arrived, else aim again.
            if ($d -lt 1000) { return Stop-FLManeuver }
            if (++$st.again -gt 3) { throw "goto $what keeps losing its target (target=$($p.Target)): $(Get-FLStoryState)" }
            $st.id = & $aim
            $null = Start-FLManeuver USER_MANEUVER_GOTO goto
            return $null
        }
        Update-FLCruiseBoost $d
        $null
    } "goto $what" $TimeoutSec
    if ($Position -and -not $Direct) { $null = Send-FL 'waypoint restore' }   # the mission's navmarkers back
    $end
}

# Enters a trade lane at a ring and rides it, the way a player does, until the ship leaves the lane (end of the lane
# or a disruption). $Ring: name, id or script nickname (`Li01_Trade_Lane_Ring_178`). -Toward <ring>: the direction
# (the next ring of the lane); the ride is checked to head there. Default: the autopilot's fast entry (fast_dock on
# the ring: straight at the lane's start point at full speed, the request where the game's stop lands on it, from
# either side - through the ring from the far side; docs\FREELANCER_RE.md "Lane entry geometry and the fast entry";
# 15 s instead of 24.5 s from 2.7 km), after the hazard-free move into dock range. -Native: the game's own dock
# maneuver (USER_MANEUVER_DOCK; slow: it stops at the ring; for -Toward first a goto to the ring's far side).
# -NoWait returns once the ride has started. Returns the pilot state at the exit (Pos, and the Exit ring read during
# the ride as LastRing).
function Start-FLTradeLane($Ring, $Toward, [switch]$NoWait, [switch]$Native, [int]$TimeoutSec = 600) {
    $cur = Get-FLPilot
    if (-not $cur) { throw "not in flight: $(Get-FLStoryState)" }
    # Already under way (plan B started it from the mission's words, or the game / player did): adopt it and wait for
    # its result instead of starting a second entry (M02 step 10, 18 Oct 2026: the second request was refused).
    $adopt = $cur.Lane -or (Get-FLAutopilot).Mode -eq 'dock' -or $cur.Op -eq 'dock'
    $towardId = 0
    if ($adopt) {
        $c = Resolve-FLContact $Ring
        $at = $null
        $ev = Get-FLEventSeq
        if ($Toward) { $towardId = (Resolve-FLContact $Toward).Id }
    }
    if (-not $adopt -and $Toward) {
        $t = Set-FLTarget $Toward; $towardId = $t.Id
        if ($Native) {
            $tp = Get-FLTargetPos
            $null = Set-FLTarget $Ring; $rp = Get-FLTargetPos
            $d = Get-FLDistance $rp $tp
            $approach = 0..2 | ForEach-Object { $rp[$_] - ($tp[$_] - $rp[$_]) / $d * 1500 }
            $null = Start-FLGoto -Position $approach
        }
    }
    # The hazard-free move into dock range (flroute); when it finds no route (M02 step 16, 18 Oct 2026) the fast entry
    # flies itself - the autopilot's own avoidance (strict in minefields) - unless -Native.
    if (-not $adopt) {
        try { $c = Move-FLToDock $Ring }
        catch {
            if ($Native -or "$_" -notmatch 'no hazard-free route') { throw }
            $c = Set-FLTarget $Ring
        }
        $at = Get-FLTargetPos
        $ev = Get-FLEventSeq
        if ($Native) { $null = Start-FLManeuver USER_MANEUVER_DOCK dock }
        else { $null = Start-FLAutopilot dock -Target $c.Id -Settings @{ fast_dock = 1; lane_toward = $towardId } }
    }
    try {
        $p = Wait-FLUntil {
            $null = Close-FLBlocker
            Assert-FLFlying "trade lane at $($c.Name)" $ev
            $x = Get-FLPilot
            # In the first frames of the ride the next ring still reads as the entry ring itself.
            if ($x -and $x.Lane -and $x.Exit -and $x.Exit -ne $x.Entry) { return $x }
            # Native: the dock op ended without a ride. Fast: the autopilot flies in free flight until its request,
            # so the end is the autopilot stopping without a ride.
            # Over only when nothing runs any more: the fast entry's autopilot ends at its request while the game's
            # dock op goes on into the ride (M02 step 3, 18 Oct 2026: judged over in that gap).
            $over = if ($Native) { $x -and -not $x.Lane -and $x.Op -ne 'dock' }
                    else { $x -and -not $x.Lane -and $x.Op -ne 'dock' -and (Get-FLAutopilot).Mode -eq 'off' }
            if ($over) {
                Assert-FLFlying "trade lane at $($c.Name)" $ev   # the refusal line may come with the end of the op
                throw "the lane entry at $($c.Name) ended without a ride (op=$($x.Op)): $(Get-FLStoryState)"
            }
            if ($Native -and $at -and $x -and -not $x.Lane) { Update-FLCruiseBoost (Get-FLDistance $x.Pos $at) }
            $null
        } "lane entry at $($c.Name)" 180
    } finally {
        if (-not $Native) { $null = Send-FL 'ap set lane_toward 0' } # a later dock must not inherit the direction
    }
    if ($towardId -and $p.Exit -ne $towardId) { throw "lane entered at $($c.Name) heads to ring $($p.Exit), not $Toward" }
    if ($NoWait) { return $p }
    $st = @{ last = $p.Exit }
    $end = Wait-FLUntil {
        $null = Close-FLBlocker
        Assert-FLFlying "trade lane ride from $($c.Name)"
        $x = Get-FLPilot
        if (-not $x) { return $null }
        if ($x.Lane) { if ($x.Exit) { $st.last = $x.Exit }; return $null }
        $x
    } "end of the lane ride from $($c.Name)" $TimeoutSec
    $end | Add-Member LastRing $st.last -PassThru
}
