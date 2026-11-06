# FLController harness - the autopilot's travel, jumps, joining ships, formation, stopping maneuvers. Part of flctl.ps1 (dot-sourced by it; never load a part alone).

# Travels the fastest way the autopilot finds (autopilot `travel`, core\game\route: the game's best path, lanes ridden
# from the best ring with free-flight exits, gates, fast docks, obstacle avoidance; re-planned after jumps and
# disruptions). Destination: $Target (an object of this system: name, id or nickname; docked with if dockable), or
# -Position x,y,z [-System <system id or nickname> [-Object <object nickname / id there, docked with>]], or nothing (the
# waypoint list as it is: a mission objective's route or the player's own). Waits (story popups closed) until the
# autopilot stops - docked, arrived, refused - unless -NoWait. Returns @{ Why; Events } (the autopilot's travel events).
function Start-FLTravel($Target, [double[]]$Position, $System, $Object, [switch]$NoWait, [switch]$NoDock, [int]$TimeoutSec = 900) {
    if (-not (Get-FLPilot)) { throw "not in flight: $(Get-FLStoryState)" }
    $toId = { param($v) if ($v -is [string] -and $v -notmatch '^\d+$') { Get-FLHash $v.ToLower() } else { [uint32]$v } }
    $arg = ''
    # No destination: the waypoint list as it is - or, when the objective's marker is a navmarker the mission never puts
    # into the list (M01b "Fly to the Waypoint", 19 Oct 2026: the travel found no waypoints and the mission failed), its
    # position from the newest objective (the "Mission Waypoint" the HUD shows: fair information). The list may fill a
    # moment after the objective: checked over ~1.5 s first.
    if (-not $Position -and $null -eq $Target) {
        $wl = @{ any = $false }
        try { $null = Wait-FLUntil { $wl.any = (Send-FL 'routes') -match '(?m)^ 0 pos '; $wl.any } 'the waypoint list' 1.5 } catch { }
        if (-not $wl.any) {
            $sys = (Get-FLPilot).System
            $obj = Get-FLEventLog 0 | Where-Object { "$_" -match ' objective count=' } | Select-Object -Last 1
            if ("$obj" -match ' nav=(-?\d+),(-?\d+),(-?\d+) system=(\d+)' -and [uint32]$Matches[4] -eq [uint32]$sys) {
                $Position = @([double]$Matches[1], [double]$Matches[2], [double]$Matches[3])
            }
        }
    }
    if ($Position) {
        $arg = $Position -join ' '
        if ($System) { $arg += " $(& $toId $System)"; if ($Object) { $arg += " $(& $toId $Object)" } }
    } elseif ($null -ne $Target) { $arg = "$((Resolve-FLContact $Target).Id)" }
    if ($NoDock) { $arg = "$arg nodock".Trim() }   # arrive beside the object, no dock (the autopilot's `nodock`)
    $seq = Get-FLEventSeq
    $r = Send-FL "ap start travel $arg"
    if ($r -match '^refused|^travel:') { throw "travel: $r | $(Get-FLStoryState)" }
    if ($NoWait) { return $r }
    $st = @{ why = $null; heard = $false } # (a hashtable: the wait's script block runs in its own scope)
    $dockId = if ($NoDock) { 0 } elseif ($null -ne $Target -and -not $Position) { [uint32]$arg } elseif ($Object) { [uint32](& $toId $Object) } else { 0 }
    $null = Wait-FLUntil {
        $null = Close-FLBlocker
        foreach ($l in (Get-FLEvents -After $seq)) {
            if ("$l" -match 'autopilot stop mode=travel why=(.+?) kills=' -and "$l" -notmatch 'why=restart') { $st.why = $Matches[1] }
        }
        # Dialogue buffer before the dock (user, 18 Oct 2026: the Missouri fast dock cut the mission's lines - docking
        # ends the space scene): within 3.5 km of a dock target while mission lines still play, park and listen, then
        # travel on. Once per travel (Wait-FLDialogue caps it and never waits under threat).
        if ($dockId -and -not $st.heard -and -not $st.why) {
            $d = Get-FLContacts -All | Where-Object { $_.Id -eq $dockId } | Select-Object -First 1
            if ($d -and $d.Distance -lt 3500 -and (Get-FLDialogueBusy ([Math]::Max(0, $script:FLLogSeq - 400)))) {
                $st.heard = $true
                $null = Send-FL 'ap start hold'
                $null = Wait-FLDialogue ([Math]::Max(0, $script:FLLogSeq - 400))
                $null = Send-FL "ap start travel $arg"
            }
        }
        # why=docking: the game's docking sequence took the ship after the autopilot's own dock request - wait for docked
        ($st.why -and $st.why -ne 'docking') -or (Get-FLMode) -eq 'docked'
    } "travel to $arg" $TimeoutSec
    if ($st.why -eq 'docking') { $st.why = $null }
    $ev = @(Get-FLEvents -After $seq | Where-Object { "$_" -match 'autopilot (travel|avoid)' } |
            ForEach-Object { "$_" -replace '^\d+ \d+ autopilot ', '' })
    [pscustomobject]@{ Why = $(if ($st.why) { $st.why } else { 'docked' }); Events = $ev }
}

# Takes a jump gate or jump hole the way a player does: target it, the dock maneuver (the autopilot flies to it, the
# jump tunnel follows), then waits until the player is flying in the new system (system id changed, not jumping,
# mode space; story cutscenes on arrival are waited out, popups closed). $Gate: name, id or script nickname
# (`Li01_to_Li02`, `Bw01_to_Br03_hole`). Returns the pilot state in the new system.
function Start-FLJump($Gate, [int]$TimeoutSec = 600) {
    $from = Get-FLPilot
    if (-not $from) { throw "not in flight: $(Get-FLStoryState)" }
    # the jump cuts the system's comms: let the mission's lines play out first (dialogue buffer)
    $null = Wait-FLDialogue ([Math]::Max(0, $script:FLLogSeq - 400))
    # this action replaces whatever the autopilot was doing (M02 18 Oct 2026: plan B's travel to the gate kept selecting
    # its lane ring, so the gate's selection never held)
    if ((Get-FLAutopilot).Mode -ne 'off') { $null = Send-FL 'ap stop' }
    $c = Move-FLToDock $Gate
    $at = Get-FLTargetPos
    $ev = Get-FLEventSeq
    $null = Start-FLManeuver USER_MANEUVER_DOCK dock
    $st = @{ jumped = $false }
    $null = Wait-FLUntil {
        $null = Close-FLBlocker
        Assert-FLFlying "jump through $($c.Name)" $ev
        $x = Get-FLPilot
        if (-not $x) { return $null }   # tunnel / cutscene: no flight state
        if ($x.System -ne $from.System) { return -not $x.Jumping }
        if ($x.Jumping) { $st.jumped = $true; return $null }
        if (-not $st.jumped -and $x.Op -ne 'dock') {
            Assert-FLFlying "jump through $($c.Name)" $ev   # the refusal line may come with the end of the op
            throw "the dock maneuver on $($c.Name) ended without a jump (op=$($x.Op)): $(Get-FLStoryState)"
        }
        if (-not $st.jumped) { Update-FLCruiseBoost (Get-FLDistance $x.Pos $at) }
        $null
    } "jump through $($c.Name)" $TimeoutSec
    if (-not (Wait-FLSpace 60)) { throw "no flight after the jump through $($c.Name): $(Get-FLStoryState)" }
    Get-FLPilot
}

# Joins a ship to fly with it ("Fly to Walker", "Escort the convoy"): the game's formation where the game allows it,
# else the autopilot's follow (flies to it and stays with it) - the game refuses formation with a cruiser (M03 Walker,
# 19 Oct 2026). Waits up to $WaitSec for the ship to appear in the contacts (an objective can name it a moment before it
# is listed). Returns 'formation' or 'follow'.
# $Ship may be an objective's target= value (a reputation instance for mission ships: M03 "Fly to Walker" target=8):
# resolved by Resolve-FLObjectiveTarget. $Text: the objective's words - the cross-check when that finds no listed ship:
# the marked friendly the words name.
function Join-FLShip($Ship, [int]$WaitSec = 10, [string]$Text) {
    if (-not (Get-FLContacts -All | Where-Object { $_.Id -eq [uint32]$Ship -and $_.Ship })) {
        $r = Resolve-FLObjectiveTarget $Ship; if ($r) { $Ship = $r }
    }
    if ($Text -and -not (Get-FLContacts -All | Where-Object { $_.Id -eq [uint32]$Ship -and $_.Ship })) {
        $m = Get-FLMarkedFriendly 0 $Text
        if ($m) { $Ship = $m }
    }
    $box = @{ id = $null } # (a hashtable: the wait's script block runs in its own scope)
    try { $null = Wait-FLUntil { $c = Get-FLContacts -All | Where-Object { $_.Id -eq [uint32]$Ship -and $_.Ship } | Select-Object -First 1; if ($c) { $box.id = $c.Id }; [bool]$c } 'the ship to join' $WaitSec } catch { }
    $id = $box.id
    if (-not $id) { throw "join: ship $Ship not in this system" }
    try { $null = Start-FLFormation $id; return 'formation' } catch { }
    $null = Stop-FLManeuver
    $null = Start-FLAutopilot follow -Target $id
    'follow'
}

# Joins a ship's formation the way a player does: target the ship, the formation maneuver
# (USER_MANEUVER_FORMATION). Returns the pilot state once the game has the player in formation with it (Leader).
# Leaving: Stop-FLManeuver.
function Start-FLFormation($Ship) {
    $c = Set-FLTarget $Ship
    $ev = Get-FLEventSeq
    $null = Start-FLManeuver USER_MANEUVER_FORMATION formation
    Wait-FLUntil {
        Assert-FLFlying "formation with $($c.Name)" $ev
        $x = Get-FLPilot
        if ($x -and $x.Op -eq 'formation' -and $x.Leader) { return $x }
        if ($x -and $x.Op -ne 'formation') { throw "formation with $($c.Name) ended (op=$($x.Op)): $(Get-FLStoryState)" }
        $null
    } "formation with $($c.Name)" 30
}

# Ends the running maneuver (goto, dock, formation) the way the player's Free Flight button does.
function Stop-FLManeuver {
    $r = Start-FLManeuver USER_MANEUVER_FREE_FLIGHT freeflight
    # Free flight keeps a running cruise: the ship then cruises straight on (M01a, 18 Oct 2026: a goto stopped at
    # TL146 cruised 50 km out of the mission's 2 km sphere - "You failed to fly to the waypoint"). End it as a player
    # does (the cruise toggle; the engine's state is in `pilot`).
    $p = Get-FLPilot
    if ($p -and $p.Cruise) {
        $null = Send-FL 'tap USER_CRUISE'
        $null = Wait-FLUntil { $x = Get-FLPilot; -not $x -or -not $x.Cruise } 'cruise off' 5
    }
    $r
}
