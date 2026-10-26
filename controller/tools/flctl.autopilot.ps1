# FLController harness - combat test setup, mission ships, the autopilot, focus. Part of flctl.ps1 (dot-sourced by it; never load a part alone).

# Free-roam combat test setup: godmode + test save in space + Liberty police hostile.
# $Rep: console command that makes enemies, e.g. 'rep l H' (Liberty police) or 'rep = H' (everyone).
# Hostility does not spawn ships: the save must be near traffic (bases, lanes).
function Start-FLCombatTest([string]$Save = 'SaveC702.fl', [string]$Rep = 'rep l H') {
    # godmode is typed in chat and applies on the next load: an extra load is only needed when it
    # still has to be typed and we are not in the game yet (main menu or death dialog).
    $p = Get-FLGameProcess
    $needGod = -not (Test-Path -LiteralPath (Join-Path $env:TEMP "flctl_godmode_$($p.Id)"))
    if ($needGod -and (Send-FL 'state') -match 'mode=menu') { $null = Load-FLSave $Save }
    Enable-FLGodmode
    $m = Load-FLSave $Save
    $null = Wait-FLSpace 10
    Send-FLConsole $Rep
    $m
}

# The mission script's ships in this session: the `spawn-ship` events (every NPC Content.dll spawns, with its exact
# spawn position) matched to the mission ini's [MsnShip] position, and [MsnFormation] position for a formation's first
# ship (its other ships: the next spawns within 1 km, in the formation's `ship =` order). Returns Id, Nickname,
# Labels, Formation, Alive (the object still exists), Spawn (position). $Mission: the mission folder (M01a, M02...).
function Get-FLMissionShips([Parameter(Mandatory)][string]$Mission) {
    $folder = Join-Path (Join-Path $script:FLRoot 'Install\Freelancer HD Edition\DATA\MISSIONS') $Mission
    $ini = Get-ChildItem -LiteralPath $folder -Filter "$Mission.ini" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $ini) { throw "no mission script $Mission.ini in $folder" }
    $ships = @{}; $formations = @(); $sec = $null; $cur = $null; $overrides = @()
    $dist = { param($a, $b) [Math]::Sqrt([Math]::Pow($a[0] - $b[0], 2) + [Math]::Pow($a[1] - $b[1], 2) + [Math]::Pow($a[2] - $b[2], 2)) }
    foreach ($line in Get-Content -LiteralPath $ini.FullName) {
        $l = ($line -replace ';.*$', '').Trim()
        if ($l -match '^\[(.+)\]$') {
            $sec = $matches[1]; $cur = [ordered]@{ Nickname = $null; Pos = $null; Labels = @(); Ships = @() }
            if ($sec -eq 'MsnFormation') { $formations += $cur }
            continue
        }
        if (-not $cur -or $l -notmatch '^(\w+)\s*=\s*(.+)$') { continue }
        $k = $matches[1].ToLower(); $v = $matches[2].Trim()
        switch ($k) {
            'nickname' { $cur.Nickname = $v; if ($sec -eq 'MsnShip') { $ships[$v] = $cur } }
            'position' { $cur.Pos = @($v -split '\s*,\s*' | ForEach-Object { [double]$_ }) }
            'label' { $cur.Labels += $v }
            'ship' { $cur.Ships += $v }
            # A spawn action may place the ship / formation elsewhere: `Act_SpawnFormation = name, x, y, z`,
            # `Act_SpawnShip = name, orders, x, y, z[, orientation]`.
            'act_spawnformation' { $p = $v -split '\s*,\s*'; if ($p.Count -ge 4) { $overrides += ,@($p[0], @([double]$p[1], [double]$p[2], [double]$p[3])) } }
            'act_spawnship' { $p = $v -split '\s*,\s*'; if ($p.Count -ge 5) { $overrides += ,@($p[0], @([double]$p[2], [double]$p[3], [double]$p[4])) } }
        }
    }
    $alt = @{}; foreach ($o in $overrides) { if (-not $alt[$o[0]]) { $alt[$o[0]] = @() }; $alt[$o[0]] += ,$o[1] }
    $near = { param($obj, $pos) foreach ($q in @(,$obj.Pos) + @($alt[$obj.Nickname])) { if ($q -and (& $dist $q $pos) -lt 2) { return $true } }; $false }
    # Every event of the session, a page at a time (one reply is capped: early spawns such as King's at the launch fell
    # out of a single read in long missions, M02 18 Oct 2026).
    $allEvents = [System.Collections.Generic.List[string]]::new(); $cursor = 0
    for ($page = 0; $page -lt 200; $page++) {
        $lines = @((Send-FL "events $cursor") -split "`n" | Where-Object { $_ } | Select-Object -Skip 1)
        if (-not $lines.Count) { break }
        foreach ($l in $lines) { $allEvents.Add($l) }
        $cursor = [int](($lines[-1] -split ' ')[0])
    }
    $spawns = @($allEvents | ForEach-Object {
        if ($_ -match 'spawn-ship id=(\d+) arch=\S+ pos=(-?\d+),(-?\d+),(-?\d+)') {
            [pscustomobject]@{ Id = [uint32]$matches[1]; Pos = @([double]$matches[2], [double]$matches[3], [double]$matches[4]) }
        } })
    $alive = @{}; foreach ($c in Get-FLContacts -All) { $alive[$c.Id] = $true }
    $out = [System.Collections.Generic.List[object]]::new()
    $add = { param($s, $nick, $frm)
        $msn = $ships[$nick]
        $out.Add([pscustomobject]@{ Id = $s.Id; Nickname = $nick; Labels = $(if ($msn) { $msn.Labels } else { @() })
                                    Formation = $frm; Alive = [bool]$alive[$s.Id]; Spawn = ($s.Pos -join ',') }) }
    for ($i = 0; $i -lt $spawns.Count; $i++) {
        $s = $spawns[$i]
        $ship = $ships.Values | Where-Object { & $near $_ $s.Pos } | Select-Object -First 1
        if ($ship) { & $add $s $ship.Nickname $null; continue }
        $frm = $formations | Where-Object { & $near $_ $s.Pos } | Select-Object -First 1
        if (-not $frm) { continue }
        & $add $s $frm.Ships[0] $frm.Nickname
        for ($j = 1; $j -lt $frm.Ships.Count -and $i + 1 -lt $spawns.Count; $j++) {
            if ((& $dist $s.Pos $spawns[$i + 1].Pos) -gt 1000) { break }
            $i++; & $add $spawns[$i] $frm.Ships[$j] $frm.Nickname
        }
    }
    $out
}

# ---- Advanced Autopilot (controller\src\impl\autopilot.cpp): the game-driven pilot flies and fights on its own; the
# harness only starts it, reads it and waits on its events (`autopilot start|stop|tactic|target|target-destroyed|
# target-given-up`). Its aim (pursuit, gimbal) moves the virtual mouse: the game must have focus (Focus-FLGame).

# The autopilot's state: Mode, Tactic, Target, Kills, Time, Engaged / OnTarget / Firing (s), AvgDist, Op, Hull and
# the settings (Settings hashtable).
function Get-FLAutopilot {
    $s = Send-FL 'ap status'
    $kv = @{}; foreach ($m in [regex]::Matches($s, '(\S+)=(\S+)')) { $kv[$m.Groups[1].Value] = $m.Groups[2].Value }
    [pscustomobject]@{ Mode = $kv['mode']; Tactic = $kv['tactic']; Target = [uint32]$kv['target']; Kills = [int]$kv['kills']
                       Time = [double]$kv['time']; Engaged = [double]$kv['engaged']; OnTarget = [double]$kv['on-target']
                       Firing = [double]$kv['firing']; AvgDist = [double]$kv['avg-dist']; Op = $kv['op']
                       Hull = [double]$kv['hull']; Settings = $kv }
}

# Starts the autopilot. $Mode: combat, escort, follow, formation, goto, dock, face, or auto (picked from the current
# target like the pad's LB + L3). $Target: id, name or nickname (Resolve-FLContact); $Position x,y,z for goto.
# $Settings: [Autopilot] settings for this run (e.g. @{ steer = 1; engage_radius = 2000; 'p.trail_distance' = 150 }).
function Start-FLAutopilot([ValidateSet('auto', 'combat', 'escort', 'follow', 'formation', 'goto', 'dock', 'face')]
                           [string]$Mode = 'auto', $Target, [double[]]$Position, [hashtable]$Settings = @{}) {
    if ((Get-FLMode) -notin 'space', 'space-ui') { throw "not in flight: $(Get-FLStoryState)" }
    foreach ($k in $Settings.Keys) {
        $r = Send-FL "ap set $k $($Settings[$k])"; if ($r -match '^unknown|failed') { throw "ap set ${k}: $r" }
    }
    $arg = if ($Position) { $Position -join ' ' } elseif ($null -ne $Target) { (Resolve-FLContact $Target).Id } else { '' }
    $r = Send-FL "ap start $Mode $arg"
    if ($r -match '^refused|unknown mode') { throw "autopilot: $r | $(Get-FLStoryState)" }
    Get-FLAutopilot
}

function Stop-FLAutopilot { $null = Send-FL 'ap stop'; Get-FLAutopilot }

# Waits until the autopilot stops by itself (its `autopilot stop ... why=` event: no enemies left, maneuver ended,
# leader gone, docked, ...), or -Kills are reached, the player dies or the mission fails; stops it on timeout.
# Returns the run: Why, Kills, Time, combat statistics.
function Wait-FLAutopilot([int]$Kills = 0, [int]$TimeoutSec = 300) {
    $seq = Get-FLEventSeq
    $sw = [Diagnostics.Stopwatch]::StartNew(); $why = 'timeout'; $stop = $null
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        foreach ($l in (Get-FLEvents -After $seq)) {
            if ($l -match 'autopilot stop .*why=(.+?) kills=') { $why = $matches[1]; $stop = $l }
            elseif ($l -match 'player-death') { $why = 'player-death' }
            elseif ($l -match 'mission-message type=FAILURE') { $why = 'mission failed' }
        }
        $seq = $script:FLEventSeq
        if ($why -ne 'timeout') { break }
        if ($Kills -gt 0 -and (Get-FLAutopilot).Kills -ge $Kills) { $why = 'kills reached'; break }
        Start-Sleep -Milliseconds 250
    }
    $a = Get-FLAutopilot
    $hull = $a.Hull
    if ($a.Mode -ne 'off') {
        $null = Send-FL 'ap stop'
        $seq2 = $script:FLEventSeq
        $stop = Get-FLEvents -After $seq2 | Where-Object { $_ -match 'autopilot stop' } | Select-Object -Last 1
    }
    # The run's numbers come from its stop event (the state resets when it stops).
    $kv = @{}; if ($stop) { foreach ($m in [regex]::Matches($stop, '(\S+)=(\S+)')) { $kv[$m.Groups[1].Value] = $m.Groups[2].Value } }
    [pscustomobject]@{ Why = $why; Kills = [int]$kv['kills']; Time = [double]$kv['time']; Engaged = [double]$kv['engaged']
                       OnTarget = [double]$kv['on-target']; Firing = [double]$kv['firing']; AvgDist = [double]$kv['avg-dist']
                       Hull = $hull; StopEvent = $stop }
}

# Fights with the autopilot until no hostiles are left in reach (or -Kills, -TimeoutSec): real kills by the game's
# weapons, no test aid. $Target: start on this ship (kept until destroyed); default: the autopilot picks.
function Start-FLCombat($Target, [int]$Kills = 0, [int]$TimeoutSec = 300, [hashtable]$Settings = @{}) {
    Focus-FLGame
    $null = Start-FLAutopilot combat -Target $Target -Settings $Settings
    Wait-FLAutopilot -Kills $Kills -TimeoutSec $TimeoutSec
}

# Freelancer only reads the mouse (DirectInput) while it is the foreground window: the virtual
# cursor needs focus, pad commands do not. Brings the game to the front if the mouse is not
# captured (this takes keyboard focus away from whatever the user is doing).
function Focus-FLGame {
    $s = Send-FL 'state'
    if ($s -match 'flags=0x[0-9A-F]*[13579BDF] ') { return } # bit0 = captured
    # run_in_background: the game, the virtual mouse and the harness work without focus; never take it from the user.
    if ($s -match 'background_run=1' -and $s -match 'foreground=0') { return }
    $p = Get-FLGameProcess
    if ($p) { [void](New-Object -ComObject WScript.Shell).AppActivate($p.Id) }
    $null = Wait-FLUntil { (Send-FL 'state') -match 'foreground=1' } 'game in the foreground' 5
    Set-FLPad -LX 0.01 # pad input re-captures the mouse
    try { $null = Wait-FLUntil { (Send-FL 'state') -match 'flags=0x[0-9A-F]*[13579BDF] ' } 'mouse captured' 3 } finally { Clear-FLPad }
}
