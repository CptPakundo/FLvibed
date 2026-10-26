# Route planning for the flight helpers (dot-sourced by flctl.ps1): the game's autopilots (goto, dock) fly straight
# at their target and through anything on the way (17 Oct 2026: the dock autopilot flew into the Zone 21 minefield
# at the New York -> Alaska gate and the ship died). Which zones are hazards comes from the game files of the
# player's system; whether a flight segment crosses one is the game's own zone test (debug `zonesalong`:
# server.dll pub::System::InZones along the segment).
#   Hazard = a field zone ([Asteroids] / [Nebula]) whose field file has a danger flag (`danger_density_*`,
#   `*_danger_objects`, `DAMAGING`), minus the field's [Exclusion Zones] (gate and lane corridors through it),
#   or any zone with `damage` > 0 (radiation, planet and sun cores).

$script:FLData = Join-Path $script:FLRoot 'Install\Freelancer HD Edition\DATA'
$script:FLSystemCache = @{}

# Sections of a Freelancer ini file: list of @{ Type = 'zone'; Keys = @{ key = @(values...) } } (keys lowercase).
function Read-FLIni([string]$Path) {
    $sections = [System.Collections.Generic.List[object]]::new()
    $cur = $null
    foreach ($line in [IO.File]::ReadLines($Path)) {
        $l = ($line -replace ';.*$', '').Trim()
        if (-not $l) { continue }
        if ($l -match '^\[(.+)\]$') { $cur = @{ Type = $Matches[1].ToLower(); Keys = @{} }; $sections.Add($cur); continue }
        if ($cur -and $l -match '^([^=]+?)\s*=\s*(.*)$') {
            $k = $Matches[1].ToLower()
            if (-not $cur.Keys.ContainsKey($k)) { $cur.Keys[$k] = [System.Collections.Generic.List[string]]::new() }
            $cur.Keys[$k].Add($Matches[2])
        }
    }
    $sections
}

function ConvertTo-FLVector([string]$s) { [double[]]@($s -split '\s*,\s*' | ForEach-Object { [double]$_ }) }

# Hazard zones of a system (by system id, as Get-FLPilot reports it): hashtable zone id -> @{ Name; Reason;
# Exclusions = zone ids; Pos; Radius (bounding radius); Rotate }, plus the exclusion zones' geometry (.Zones).
function Get-FLSystemHazards([uint32]$SystemId) {
    if ($script:FLSystemCache.ContainsKey($SystemId)) { return $script:FLSystemCache[$SystemId] }
    $uni = Read-FLIni (Join-Path $script:FLData 'UNIVERSE\universe.ini')
    $sys = $uni | Where-Object { $_.Type -eq 'system' -and $_.Keys.nickname -and
                                 (Get-FLHash $_.Keys.nickname[0].ToLower()) -eq $SystemId } | Select-Object -First 1
    if (-not $sys) { throw "system $SystemId not found in universe.ini" }
    $file = Join-Path $script:FLData ('UNIVERSE\' + $sys.Keys.file[0])
    $ini = Read-FLIni $file
    $zones = @{}
    foreach ($z in $ini | Where-Object Type -eq 'zone') {
        $nick = $z.Keys.nickname[0]
        $size = if ($z.Keys.size) { ConvertTo-FLVector $z.Keys.size[0] } else { [double[]]@(0) }
        $shape = if ($z.Keys.shape) { $z.Keys.shape[0].ToUpper() } else { 'SPHERE' }
        $radius = switch ($shape) {
            'BOX' { [Math]::Sqrt(($size | ForEach-Object { $_ * $_ } | Measure-Object -Sum).Sum) / 2 }
            'CYLINDER' { [Math]::Max($size[0], $(if ($size.Count -gt 1) { $size[1] / 2 } else { 0 })) }
            default { ($size | Measure-Object -Maximum).Maximum }
        }
        $zones[[uint32](Get-FLHash $nick.ToLower())] = @{
            Name = $nick; Shape = $shape; Size = $size; Radius = $radius
            Pos = ConvertTo-FLVector $z.Keys.pos[0]
            Rotate = if ($z.Keys.rotate) { ConvertTo-FLVector $z.Keys.rotate[0] } else { [double[]]@(0, 0, 0) }
            Damage = if ($z.Keys.damage) { [double]$z.Keys.damage[0] } else { 0 }
        }
    }
    $hazards = @{}
    foreach ($id in $zones.Keys) {
        if ($zones[$id].Damage -gt 0) { $hazards[$id] = @{ Zone = $zones[$id]; Reason = "damage=$($zones[$id].Damage)"; Exclusions = @() } }
    }
    foreach ($f in $ini | Where-Object { $_.Type -in 'asteroids', 'nebula' }) {
        if (-not $f.Keys.file -or -not $f.Keys.zone) { continue }
        $path = Join-Path $script:FLData $f.Keys.file[0]
        if (-not (Test-Path -LiteralPath $path)) { continue }
        $field = Read-FLIni $path
        $flags = @($field | Where-Object Type -eq 'properties' | ForEach-Object { $_.Keys.flag } | Where-Object { $_ })
        $danger = @($flags | Where-Object { $_ -match '(?i)danger|damaging' })
        if (-not $danger) { continue }
        $id = [uint32](Get-FLHash $f.Keys.zone[0].ToLower())
        if (-not $zones.ContainsKey($id)) { continue }
        $excl = @($field | Where-Object Type -eq 'exclusion zones' | ForEach-Object { $_.Keys.exclusion } |
                  Where-Object { $_ } | ForEach-Object { [uint32](Get-FLHash $_.ToLower()) })
        $hazards[$id] = @{ Zone = $zones[$id]; Reason = ($danger -join ','); Exclusions = $excl }
    }
    $r = @{ Hazards = $hazards; Zones = $zones; File = $file }
    $script:FLSystemCache[$SystemId] = $r
    $r
}

# The first point of the straight flight A -> B inside a hazard (game zone test every $Step m), or $null if the
# segment is clear. A hazard the segment starts in (the ship is inside it now) is ignored while the segment leaves
# it, unless -IncludeStart. Returns @{ Zone = id; Name; Reason; At = point; T = 0..1 }.
function Test-FLSegment([double[]]$A, [double[]]$B, [hashtable]$Sys, [double]$Step = 250, [switch]$IncludeStart) {
    $r = Send-FL ('zonesalong {0} {1} {2} {3} {4} {5} {6}' -f $A[0], $A[1], $A[2], $B[0], $B[1], $B[2], $Step)
    $lines = $r -split "`n" | Where-Object { $_ }
    $m = [regex]::Match($lines[0], '^zones samples=(\d+)')
    if (-not $m.Success) { throw "zonesalong: $r" }
    $n = [int]$m.Groups[1].Value
    $spans = @{}
    foreach ($l in $lines | Select-Object -Skip 1) {
        $parts = $l -split ' '
        $spans[[uint32]$parts[0]] = @($parts | Select-Object -Skip 1 | ForEach-Object { $x = $_ -split '-'; , @([int]$x[0], [int]$x[1]) })
    }
    $in = { param($zone, $i) foreach ($rg in $spans[$zone]) { if ($i -ge $rg[0] -and $i -le $rg[1]) { return $true } }; $false }
    $best = $null
    foreach ($id in $spans.Keys) {
        if (-not $Sys.Hazards.ContainsKey($id)) { continue }
        $h = $Sys.Hazards[$id]
        # Samples inside the hazard and outside all of its exclusions are hazardous. When the segment starts on a
        # hazardous sample (the ship is in it now), the leading hazardous stretch is the way out and is ignored.
        $prev = -1   # last sample of the leading hazardous stretch (-1: none)
        $found = $false
        foreach ($rg in $spans[$id]) {
            for ($i = $rg[0]; $i -le $rg[1]; $i++) {
                $excluded = $false
                foreach ($e in $h.Exclusions) { if ($spans.ContainsKey($e) -and (& $in $e $i)) { $excluded = $true; break } }
                if ($excluded) { continue }
                if (-not $IncludeStart -and $prev -eq $i - 1) { $prev = $i; continue }   # still on the way out
                if (-not $best -or $i -lt $best.Index) { $best = @{ Index = $i; Zone = $id } }
                $found = $true; break
            }
            if ($found) { break }
        }
    }
    if (-not $best) { return $null }
    $t = if ($n) { $best.Index / $n } else { 0 }
    $h = $Sys.Hazards[$best.Zone]
    @{ Zone = $best.Zone; Name = $h.Zone.Name; Reason = $h.Reason; T = $t
       At = [double[]](0..2 | ForEach-Object { $A[$_] + ($B[$_] - $A[$_]) * $t }) }
}

# Unit vector of a zone's local axis k (0 x, 1 y, 2 z) in system space: rotate = x, y, z degrees (Freelancer
# applies them as Rx * Ry * Rz; verified for yaw only: Zone_Li01_zone21_lane_access ends at the Alaska gate).
function Get-FLZoneAxis([hashtable]$Zone, [int]$K) {
    $r = $Zone.Rotate | ForEach-Object { $_ * [Math]::PI / 180 }
    $cx = [Math]::Cos($r[0]); $sx = [Math]::Sin($r[0]); $cy = [Math]::Cos($r[1]); $sy = [Math]::Sin($r[1])
    $cz = [Math]::Cos($r[2]); $sz = [Math]::Sin($r[2])
    # columns of Rx * Ry * Rz (each element in parentheses: the comma binds tighter than arithmetic)
    switch ($K) {
        0 { [double[]]@(($cy * $cz), ($sx * $sy * $cz + $cx * $sz), (- $cx * $sy * $cz + $sx * $sz)) }
        1 { [double[]]@((- $cy * $sz), (- $sx * $sy * $sz + $cx * $cz), ($cx * $sy * $sz + $sx * $cz)) }
        2 { [double[]]@($sy, (- $sx * $cy), ($cx * $cy)) }
    }
}

# Detour points for a hazard: a ring around it (bounding radius + margin, in the plane of the system) and, for each
# of its exclusion zones, the zone's centre and the ends of its longest axis just outside it (the corridor ends).
function Get-FLDetourPoints([hashtable]$Hazard, [hashtable]$Sys, [double]$Margin = 2500) {
    $z = $Hazard.Zone
    $pts = [System.Collections.Generic.List[double[]]]::new()
    $rr = $z.Radius + $Margin
    for ($k = 0; $k -lt 12; $k++) {
        $a = $k * [Math]::PI / 6
        $pts.Add([double[]]@(($z.Pos[0] + $rr * [Math]::Cos($a)), $z.Pos[1], ($z.Pos[2] + $rr * [Math]::Sin($a))))
    }
    foreach ($e in $Hazard.Exclusions) {
        if (-not $Sys.Zones.ContainsKey($e)) { continue }
        $x = $Sys.Zones[$e]
        $pts.Add($x.Pos)
        $half = @(0, 1, 2 | ForEach-Object {
            $s = if ($x.Size.Count -gt $_) { $x.Size[$_] } else { $x.Size[0] }
            if ($x.Shape -eq 'BOX') { $s / 2 } else { $s } })
        if ($x.Shape -eq 'CYLINDER') { $half = @($x.Size[0], $(if ($x.Size.Count -gt 1) { $x.Size[1] / 2 } else { $x.Size[0] }), $x.Size[0]) }
        $k = [Array]::IndexOf($half, ($half | Measure-Object -Maximum).Maximum)
        $ax = Get-FLZoneAxis $x $k
        foreach ($sgn in 1, -1) {
            $d = $half[$k] + 1500
            $pts.Add([double[]](0..2 | ForEach-Object { $x.Pos[$_] + $sgn * $ax[$_] * $d }))
            $d = $half[$k] * 0.5   # inside the corridor too: the leg in can turn along it
            $pts.Add([double[]](0..2 | ForEach-Object { $x.Pos[$_] + $sgn * $ax[$_] * $d }))
        }
    }
    $pts
}

# A hazard-free route from the ship to $Goal (system coordinates) as a list of via points (not including the start;
# the last one is $Goal). Lazy shortest path: legs are assumed clear until the game's zone test says otherwise; each
# blocking hazard adds its detour points. Throws when the goal lies inside a hazard (outside its exclusions) or no
# route is found.
function Get-FLRoute([double[]]$Goal, [double[]]$From) {
    $p = Get-FLPilot
    if (-not $p) { throw "not in flight: $(Get-FLStoryState)" }
    if (-not $From) { $From = $p.Pos }
    $sys = Get-FLSystemHazards $p.System
    $at = Test-FLSegment $Goal $Goal $sys -IncludeStart
    if ($at -and $at.Reason -match '^damage' -and $sys.Hazards[$at.Zone].Zone.Shape -eq 'SPHERE') {
        # A planet's or sun's centre lies in its own death zone: route to the zone's surface facing the ship (the
        # game's maneuver flies the rest: dock rings sit outside it).
        $z = $sys.Hazards[$at.Zone].Zone
        $d = Get-FLDistance $From $z.Pos
        $Goal = [double[]](0..2 | ForEach-Object { $z.Pos[$_] + ($From[$_] - $z.Pos[$_]) / $d * ($z.Radius + 1500) })
        $at = Test-FLSegment $Goal $Goal $sys -IncludeStart
    }
    if ($at) { throw "the goal $($Goal -join ',') lies inside $($at.Name) ($($at.Reason))" }
    $nodes = [System.Collections.Generic.List[double[]]]::new(); $nodes.Add($From); $nodes.Add($Goal)
    $blocked = @{}; $clear = @{}; $added = @{}
    for ($iter = 0; $iter -lt 40; $iter++) {
        # Dijkstra over the current nodes (0 = start, 1 = goal), skipping legs known to be blocked
        $n = $nodes.Count
        $dist = @([double]::MaxValue) * $n; $prev = @(-1) * $n; $done = @($false) * $n; $dist[0] = 0
        for ($c = 0; $c -lt $n; $c++) {
            $u = -1; for ($i = 0; $i -lt $n; $i++) { if (-not $done[$i] -and ($u -lt 0 -or $dist[$i] -lt $dist[$u])) { $u = $i } }
            if ($u -lt 0 -or $dist[$u] -eq [double]::MaxValue) { break }
            $done[$u] = $true
            if ($u -eq 1) { break }
            for ($v = 0; $v -lt $n; $v++) {
                if ($done[$v] -or $blocked.ContainsKey("$u,$v")) { continue }
                $d = $dist[$u] + (Get-FLDistance $nodes[$u] $nodes[$v])
                if ($d -lt $dist[$v]) { $dist[$v] = $d; $prev[$v] = $u }
            }
        }
        if ($dist[1] -eq [double]::MaxValue) { break }
        $path = @(); for ($v = 1; $v -ge 0; $v = $prev[$v]) { $path = , $v + $path }
        $ok = $true
        for ($i = 0; $i -lt $path.Count - 1; $i++) {
            $u = $path[$i]; $v = $path[$i + 1]
            if ($clear.ContainsKey("$u,$v")) { continue }
            $hit = Test-FLSegment $nodes[$u] $nodes[$v] $sys
            if (-not $hit) { $clear["$u,$v"] = $true; $clear["$v,$u"] = $true; continue }
            $ok = $false
            $blocked["$u,$v"] = $true; $blocked["$v,$u"] = $true
            if (-not $added.ContainsKey($hit.Zone)) {
                $added[$hit.Zone] = $true
                foreach ($pt in Get-FLDetourPoints $sys.Hazards[$hit.Zone] $sys) { $nodes.Add($pt) }
            }
            break
        }
        if ($ok) {
            $route = [System.Collections.Generic.List[object]]::new()
            foreach ($v in $path | Select-Object -Skip 1) { $route.Add($nodes[$v]) }
            return , $route
        }
    }
    throw "no hazard-free route to $($Goal -join ',') found (system file $($sys.File))"
}

# Brings the ship safely to where the final maneuver on $Goal can start: plans a hazard-free route, flies its via
# points with the game's Goto autopilot (each until within $Within m, live distance, so the next leg starts without
# a full stop), and with -Range (a dock request's range) also flies along the last leg until $Goal is within
# 90 % of it. Returns the route (via points and goal).
function Move-FLToward([double[]]$Goal, [double]$Range = 0, [double]$Within = 600) {
    $route = Get-FLRoute $Goal
    for ($i = 0; $i -lt $route.Count - 1; $i++) { $null = Start-FLGoto -Position $route[$i] -Within $Within -Direct }
    if ($Range -gt 0) {
        $here = (Get-FLPilot).Pos
        $d = Get-FLDistance $here $Goal
        if ($d -gt $Range * 0.9) {   # a point of the (checked) last leg, inside the range
            $f = 1 - ($Range * 0.8) / $d
            $null = Start-FLGoto -Position ([double[]](0..2 | ForEach-Object { $here[$_] + ($Goal[$_] - $here[$_]) * $f })) -Within $Within -Direct
        }
    }
    # The via points replaced the player's waypoint list: put the mission's navmarkers back (unless it changed them).
    $null = Send-FL 'waypoint restore'
    $route
}
