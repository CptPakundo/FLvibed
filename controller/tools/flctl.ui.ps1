# FLController harness - room navigation by function, UI items, events. Part of flctl.ps1 (dot-sourced by it; never load a part alone).

# Moves the cursor to the first UI control whose name matches $Name (regex) and presses A.
# -Nth picks a later match (0-based). Returns the control line, or throws if it is not found.
function Goto-FLRoom([string]$Label, [int]$TimeoutSec = 15, [switch]$NoWait, [switch]$Watch) {
    # Base room button by function (debug `goto`): "Bar", "Trader", "Equipment", "ShipDealer", "Cityscape" (the
    # launch area, IDS_HOTSPOT_EXIT), or part of a hotspot name ("news", "mission", "repair"). RoomControlN /
    # ButtonControlN are numbered per room, so never click those by number.
    # Driven by game state, not delays: `goto` refuses while the NavBar would swallow the click (`busy: <gate>`,
    # room::NavBusy: room entry transition, UI flag, room busy) and while the room UI is not built yet; then the
    # room has changed when `hotspots` lists a different room. -TimeoutSec is only a safety limit.
    # Room actions that open a window (news, mission, repair) do not change the room: use -NoWait.
    # Room entry transitions are skipped (as Escape does) unless -Watch; returns once the new room is ready.
    # Right after a landing the landing sequence and the room's entry run first (M03 19 Oct 2026: "busy: navbar" for
    # longer than the click's 15 s after docking at California Minor): wait for the room to be ready (skipping the
    # animations as Escape does, unless -Watch), then click.
    $null = Wait-FLRoomReady -Watch:$Watch
    $sw = [Diagnostics.Stopwatch]::StartNew()
    if (-not $Watch) { $null = Skip-FLAnimation }   # a transition still running would swallow the click
    $before = Send-FL 'hotspots'
    do {
        $r = Send-FL "goto $Label"
        if ($r -match '^clicked') { break }
        if ($r -match '^not a supported') { throw $r }
        Start-Sleep -Milliseconds 100
    } while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec)
    if ($r -notmatch '^clicked') { throw "room button '$Label': $r`n$before" }
    if ($NoWait) { return $r }
    $rooms = { ((Send-FL 'hotspots') -split "`n" | Where-Object { $_ -notmatch '^hotspots ' }) -join "`n" }
    $was = (($before -split "`n") | Where-Object { $_ -notmatch '^hotspots ' }) -join "`n"
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        Start-Sleep -Milliseconds 50
        if ((& $rooms) -ne $was) {
            Wait-FLRoomReady -Watch:$Watch   # the new room's entry transition (skipped unless -Watch)
            return "$r, room ready after {0:n1} s" -f $sw.Elapsed.TotalSeconds
        }
    }
    throw "clicked '$Label' while the room was ready, but the room did not change (already there, or blocked by mission state)"
}

function Select-FLItem([string]$Name, [int]$Nth = 0, [switch]$NoPress, [switch]$Cursor) {
    # Native click first (debug `click`: the button's own release logic, no cursor, works without focus and
    # in flight); the cursor path only for controls it does not support (lists, front-end buttons, ...),
    # for -Nth > 0 (several controls share the name) or when -Cursor / -NoPress asks for the hover itself.
    if (-not $NoPress -and -not $Cursor -and $Nth -eq 0) {
        $wait = [Diagnostics.Stopwatch]::StartNew()
        do {
            $r = Send-FL "click $Name"
            if ($r -match '^clicked') { return $r }
            if ($r -match '^not a supported button') { break }
            Start-Sleep -Milliseconds 100
        } while ($wait.ElapsedMilliseconds -lt 3000)
    }
    Focus-FLGame
    # Controls appear a few frames after the window that owns them (e.g. a base's room buttons after a dialog
    # closes): wait for the item instead of failing on the first look.
    $wait = [Diagnostics.Stopwatch]::StartNew()
    do {
        $items = @((Send-FL 'uiitems') -split "`n" | Where-Object { $_ -match "^$Name\s" })
        if ($items.Count -gt $Nth) { break }
        Start-Sleep -Milliseconds 100
    } while ($wait.ElapsedMilliseconds -lt 3000)
    if ($items.Count -le $Nth) { throw "UI item '$Name' not found" }
    $line = @($items)[$Nth]
    $c = [regex]::Match($line, 'center=\((-?\d+),(-?\d+)\)')
    $ctl = [regex]::Match($line, 'ctl=([0-9A-F]+)').Groups[1].Value.ToLower().TrimStart('0')
    Send-FL "mouse $($c.Groups[1].Value) $($c.Groups[2].Value)" | Out-Null
    # Only press once Freelancer itself reports the control as hovered.
    $sw = [Diagnostics.Stopwatch]::StartNew(); $hovered = $false
    while ($sw.ElapsedMilliseconds -lt 3000) { # room transitions delay the hover
        Start-Sleep -Milliseconds 100
        if ([regex]::Match((Send-FL 'uiitems'), '^hovered=.*?\(([0-9a-f]+)\)').Groups[1].Value -eq $ctl) { $hovered = $true; break }
    }
    Send-FL 'mouse release' | Out-Null
    if (-not $hovered) { throw "UI item '$Name' did not become hovered" }
    if (-not $NoPress) { Press-FLPad A }
    $line
}

# Game events (mission objectives, popups, mission messages, comm lines, mode changes, death).
# Each line: "<seq> <tickMs> <text>". Get-FLEvents returns lines newer than the last call
# (or -After n); $script:FLEventSeq tracks the position. Note: each PowerShell call is a new
# process, so pass -After explicitly when continuing across calls.
$script:FLEventSeq = 0
function Get-FLEvents([int]$After = -1) {
    if ($After -ge 0) { $script:FLEventSeq = $After }
    $r = (Send-FL "events $($script:FLEventSeq)") -split "`n" | Where-Object { $_ }
    $last = [regex]::Match($r[0], '^last (\d+)')
    $lines = @($r | Select-Object -Skip 1)
    if ($lines.Count) { $script:FLEventSeq = [int](($lines[-1] -split ' ')[0]) }
    elseif ($last.Success -and [int]$last.Groups[1].Value -lt $script:FLEventSeq) { $script:FLEventSeq = 0 } # game restarted
    $lines
}

# Waits for an event matching $Pattern (regex) that happens after this call starts; returns the
# line, or $null on timeout. Every event seen while waiting is echoed when -Verbose is set.
function Wait-FLEvent([string]$Pattern, [int]$TimeoutSec = 60) {
    $start = [regex]::Match((Send-FL 'events 999999999'), '^last (\d+)').Groups[1].Value
    $script:FLEventSeq = [int]$start
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        foreach ($l in (Get-FLEvents)) {
            Write-Verbose $l
            if ($l -match $Pattern) { return $l }
        }
        Start-Sleep -Milliseconds 250
    }
    $null
}
