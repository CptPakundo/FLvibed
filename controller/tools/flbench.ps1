# Autopilot combat benchmarks (dot-source after flctl.ps1, PowerShell tool). Results go to research\bench (git-ignored).
# Every number is specific to its scenario (ship / loadout, enemies, area): the default scenario - the starter
# Starflier of SaveC702 vs Liberty police at Manhattan - is stacked against the player (user, 18 Oct 2026).
#   Enter-FLStream                     fresh SaveC702 load + `rep l H` (a police patrol ~800 m away attacks)
#   Invoke-FLBenchRun -Name n -Settings @{...}   one fight, status timeline + summary (kills, lost per kill, ...)
#   Invoke-FLABRun -A @{...} -B @{...}           A/B inside one fight: settings alternate at every kill (paired
#                                                 samples on the same enemies) or every -Segment seconds
#   Invoke-FLABBench -Tag t -A 'k=v;k=v' -B '...' -Fights 6   repeated A/B fights (the starting variant swaps)
#   Fixed scenarios (spawned enemies, the same every run - controlled A/B instead of random police encounters):
#   Enter-FLScenario <name>              fresh SaveC702 load (police friendly), then the scenario's spawns
#   Invoke-FLScenarioAB -Scenario s -A 'k=v;..' -B 'k=v;..'   alternating whole fights A / B on the same scenario,
#                                         paired; stops once a difference clears the noise (or -MaxPairs)
$script:FLBenchDir = Join-Path (Split-Path (Split-Path $PSScriptRoot)) 'research\bench'

# Scenario -> spawn specs (`spawn <npcshiparch> <faction|player> <count> <distance> [options]`). `player` = a private
# faction hostile to the player only: nothing else joins or takes sides. The jobs default to the game's solo job with
# scene toughness HARDEST (core\game\spawn.h): lone low-level spawns engage at once. Levels from npcships.ini.
$script:FLScenarios = [ordered]@{
    'rogues-2'  = @('fc_lr_pi_fighter_d1 player 2 1200')      # Liberty Rogue Patriots, level 1: the easiest
    'rogues-3'  = @('fc_lr_pi_fighter_d2 player 3 1200')      # level 2, three
    'police-4'  = @('li_p_li_fighter_d1-2 player 4 1200')     # Liberty police, level 2, four (the old benchmark's foes)
    'elite-1'   = @('li_n_li_elite_d4 player 1 1200')         # one Navy Elite, level 4: out of the starter ship's league
    'mixed-3'   = @('fc_lr_pi_fighter_d2 player 2 1200', 'li_n_li_elite_d4 player 1 1500')
}

# The player's ship of a scenario: a save with that ship, free roam (no mission), near Manhattan (police friendly).
# Docked saves launch first. or_elite: the campaign set's post-story save (..\docs\missions\SAVES.md; copied into the
# save folder once, the game lists saves at startup: deploy.ps1 -Save <path> does both).
$script:FLScenarioShips = [ordered]@{
    'starter'  = 'SaveC702.fl'   # ge_fighter (Starflier), rank 0, in space near Manhattan
    'or_elite' = 'SaveC704.fl'   # or_elite, rank 21, in space at the same spot (make-testsaves.ps1 from the campaign
                                 # set's post-story save Save0e2b; docked saves need a launch, which the game once
                                 # refused for a whole session: docs\FREELANCER_RE.md "Room hotspots")
}

function Enter-FLScenario([string]$Name, [string]$Ship = 'starter') {
    if (-not $script:FLScenarios.Contains($Name)) { throw "unknown scenario $Name (known: $($script:FLScenarios.Keys -join ', '))" }
    if (-not $script:FLScenarioShips.Contains($Ship)) { throw "unknown ship $Ship (known: $($script:FLScenarioShips.Keys -join ', '))" }
    $null = Send-FL 'ap stop'
    $null = Load-FLSave $script:FLScenarioShips[$Ship]
    $null = Wait-FLUntil { (Get-FLMode) -in 'space', 'docked' } 'space or base' 30
    if ((Get-FLMode) -eq 'docked') {
        # From a base: fly to the starter save's open-space spot (SaveC702: Li01 8000, 800, -50500) - every ship fights
        # in the same arena (was: spawning beside the launch point put ships inside Manhattan, dead at once, 18 Oct 2026).
        # Once (18 Oct 2026, not reproduced since) the game ignored the Launch click after a load from space, and once a
        # goto here ended ~5 km out as if arrived: the launch is retried after a fresh load, the arena checked by distance.
        try { $null = Start-FLLaunch -TimeoutSec 20 } catch {
            Write-Warning "scenario ${Name}: launch refused, loading again ($_)"
            $null = Load-FLSave $script:FLScenarioShips[$Ship]
            $null = Wait-FLUntil { (Get-FLMode) -eq 'docked' } 'docked' 30
            $null = Start-FLLaunch -TimeoutSec 20
        }
        $null = Wait-FLSpace
        $arena = [double[]](8000, 800, -50500)
        for ($try = 1; $try -le 3 -and (Get-FLDistance (Get-FLPilot).Pos $arena) -gt 600; $try++) {
            if ($try -gt 1) { Write-Warning "scenario ${Name}: goto ended $([math]::Round((Get-FLDistance (Get-FLPilot).Pos $arena))) m short, again" }
            $null = Start-FLGoto -Position $arena -Within 300
        }
        $null = Stop-FLManeuver
        if ((Get-FLDistance (Get-FLPilot).Pos $arena) -gt 600) { throw "scenario ${Name}: arena not reached: $(Send-FL 'pilot')" }
    }
    $null = Wait-FLUntil { (Get-FLMode) -eq 'space' } 'space' 30
    $n = 0
    foreach ($spec in $script:FLScenarios[$Name]) {
        $r = Send-FL "spawn $spec"
        if ($r -notmatch '^spawned( \d+)+$') { throw "scenario ${Name}: spawn $spec -> $r" }
        $n += ([regex]::Matches($r, ' \d+')).Count
    }
    # The fight starts once they are hostile contacts (a spawn is an object at once; the wait is a safety net).
    $null = Wait-FLUntil { @(Get-FLContacts -Hostile).Count -ge $n } "$n hostiles of $Name" 10
    $n
}

# Whole fights A / B alternating on the same scenario (paired: pair i = A and B on identical spawns; the first variant
# of a pair alternates). Per run: Secs (to clear, capped at -TimeoutSec), Lost (ships' worth), Kills, Died. After
# -MinPairs, stops once the paired difference of Lost or Secs reaches |t| >= -T (difference beyond the noise) or at
# -MaxPairs. Both variants must set the same keys (the autopilot keeps a setting until it is set again).
function Invoke-FLScenarioAB([string]$Scenario, [string]$Ship = 'starter', [string]$A, [string]$B, [int]$MinPairs = 3, [int]$MaxPairs = 8,
                             [double]$T = 2.5, [int]$TimeoutSec = 90, [string]$Tag) {
    $parse = { param($s) $h = @{}; foreach ($p in ($s -split ';')) { if ($p) { $k, $v = $p -split '=', 2; $h[$k] = $v } }; $h }
    $ha = & $parse $A; $hb = & $parse $B
    if (((@($ha.Keys) | Sort-Object) -join ',') -ne ((@($hb.Keys) | Sort-Object) -join ',')) { throw "A and B must set the same keys" }
    if (-not $Tag) { $Tag = "$Scenario-$Ship-" + (Get-Date -Format 'yyyyMMdd-HHmmss') }
    New-Item -ItemType Directory -Force $script:FLBenchDir | Out-Null
    $out = Join-Path $script:FLBenchDir "$Tag.scenario.csv"
    $runs = @(); $decision = 'max pairs'
    $tStat = { param($d) $n = $d.Count; if ($n -lt 2) { return 0 }; $m = ($d | Measure-Object -Average).Average
        $sd = [math]::Sqrt((($d | ForEach-Object { ($_ - $m) * ($_ - $m) }) | Measure-Object -Sum).Sum / ($n - 1))
        if ($sd -eq 0) { if ($m -eq 0) { 0 } else { [double]::PositiveInfinity * [math]::Sign($m) } } else { $m / ($sd / [math]::Sqrt($n)) } }
    for ($i = 1; $i -le $MaxPairs; $i++) {
        $order = if ($i % 2) { 'A', 'B' } else { 'B', 'A' }
        foreach ($v in $order) {
            $spawned = Enter-FLScenario $Scenario $Ship
            $res = Invoke-FLBenchRun -Name "$Tag-$i$v" -Settings $(if ($v -eq 'A') { $ha } else { $hb }) -TimeoutSec $TimeoutSec -UntilSpawnsDead
            # Secs = time to clear the scenario; a run that did not clear it (death, flight, timeout) counts the full cap
            # (was: its raw time - a death ends a run early and read as "faster", 18 Oct 2026).
            $cleared = $res.Why -in 'spawns cleared', 'no enemies left'
            $row = [pscustomobject]@{ Pair = $i; Variant = $v; Spawned = $spawned; Why = $res.Why; Cleared = [int]$cleared
                Secs = $(if ($cleared) { $res.Secs } else { $TimeoutSec }); RawSecs = $res.Secs
                Kills = $res.SpawnKills; Extra = $res.Extra; Lost = $res.Lost; Died = [int]($res.Why -eq 'player died' -or $res.Why -eq 'player-death')
                OnTargetPct = $res.OnTargetPct; Batteries = $res.Batteries; Nanobots = $res.Nanobots }
            $row | Export-Csv -NoTypeInformation -Append $out
            $runs += $row
        }
        # Paired differences B - A. A run that did not clear the scenario counts its full time.
        $pairs = 1..$i | ForEach-Object { $p = $_; $ra = $runs | Where-Object { $_.Pair -eq $p -and $_.Variant -eq 'A' }; $rb = $runs | Where-Object { $_.Pair -eq $p -and $_.Variant -eq 'B' }
            [pscustomobject]@{ Lost = $rb.Lost - $ra.Lost; Secs = $rb.Secs - $ra.Secs } }
        $tLost = & $tStat @($pairs.Lost); $tSecs = & $tStat @($pairs.Secs)
        if ($i -ge $MinPairs -and ([math]::Abs($tLost) -ge $T -or [math]::Abs($tSecs) -ge $T)) { $decision = "decided after $i pairs"; break }
    }
    $null = Send-FL 'ap stop'
    $sum = { param($v, $k) ($runs | Where-Object { $_.Variant -eq $v } | Measure-Object $k -Average).Average }
    [pscustomobject]@{ Scenario = $Scenario; Pairs = ($runs.Count / 2); Decision = $decision
        A_Secs = [math]::Round((& $sum 'A' 'Secs'), 1); B_Secs = [math]::Round((& $sum 'B' 'Secs'), 1); T_Secs = [math]::Round($tSecs, 2)
        A_Lost = [math]::Round((& $sum 'A' 'Lost'), 3); B_Lost = [math]::Round((& $sum 'B' 'Lost'), 3); T_Lost = [math]::Round($tLost, 2)
        A_Deaths = ($runs | Where-Object { $_.Variant -eq 'A' } | Measure-Object Died -Sum).Sum
        B_Deaths = ($runs | Where-Object { $_.Variant -eq 'B' } | Measure-Object Died -Sum).Sum
        A_Cleared = ($runs | Where-Object { $_.Variant -eq 'A' } | Measure-Object Cleared -Sum).Sum
        B_Cleared = ($runs | Where-Object { $_.Variant -eq 'B' } | Measure-Object Cleared -Sum).Sum; Csv = $out }
}

function Enter-FLStream {
    $null = Send-FL 'ap stop'
    $null = Load-FLSave SaveC702.fl
    $null = Wait-FLUntil { (Get-FLMode) -eq 'space' } 'space' 30
    $null = Send-FLConsole 'rep l H'
    $null = Wait-FLUntil { @(Get-FLContacts -Hostile).Count -gt 0 } 'hostiles' 20
}

function Invoke-FLABRun([hashtable]$A, [hashtable]$B, [hashtable]$Common = @{}, [double]$Segment = 30, [int]$TimeoutSec = 150) {
    $keys = 'kills', 'engaged', 'on-target', 'firing', 'dealt', 'lost', 'missile-hits', 'missile-misses', 'hit-time'
    $read = { $s = Send-FL 'ap status'; $kv = @{}; foreach ($m in [regex]::Matches($s, '(\S+)=(\S+)')) { $kv[$m.Groups[1].Value] = $m.Groups[2].Value }; $kv }
    $tot = @{ A = @{}; B = @{} }; foreach ($v in 'A', 'B') { foreach ($k in $keys + 'secs') { $tot[$v][$k] = 0.0 } }
    $set = { param($h) foreach ($k in $h.Keys) { $null = Send-FL "ap set $k $($h[$k])" } }
    & $set $Common; & $set $A
    $seq = Get-FLEventSeq
    $null = Start-FLAutopilot combat
    $cur = 'A'; $prev = & $read; $sw = [Diagnostics.Stopwatch]::StartNew(); $segStart = 0.0
    # The last segment counts too (was: only its seconds - the status is reset once the autopilot stops, so its
    # kills, damage and losses, a death's included, were dropped; a timeout dropped the whole segment, 18 Oct 2026).
    $account = { param($now) foreach ($k in $keys) { $tot[$cur][$k] += [double]$now[$k] - [double]$prev[$k] }
        $tot[$cur]['secs'] += $sw.Elapsed.TotalSeconds - $segStart }
    $done = $false
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        Start-Sleep -Milliseconds 250   # sampling interval of the status, not a wait for a state
        $now = & $read
        $done = $now['mode'] -eq 'off'
        if ($done) {
            # The run's final numbers: its stop event.
            $stop = Get-FLEvents -After $seq | Where-Object { $_ -match 'autopilot stop ' } | Select-Object -Last 1
            $now = @{}; foreach ($m in [regex]::Matches("$stop", '(\S+)=(\S+)')) { $now[$m.Groups[1].Value] = $m.Groups[2].Value }
            if ($stop) { & $account $now } else { $tot[$cur]['secs'] += $sw.Elapsed.TotalSeconds - $segStart }
            break
        }
        $killed = [double]$now['kills'] -gt [double]$prev['kills']
        if ($killed -or $sw.Elapsed.TotalSeconds - $segStart -ge $Segment) {
            & $account $now
            $cur = if ($cur -eq 'A') { 'B' } else { 'A' }
            & $set $(if ($cur -eq 'A') { $A } else { $B })
            $prev = & $read; $segStart = $sw.Elapsed.TotalSeconds
        }
    }
    if (-not $done) { & $account (& $read); $null = Send-FL 'ap stop' }
    foreach ($v in 'A', 'B') {
        $t = $tot[$v]
        [pscustomobject]@{ Variant = $v; Secs = [math]::Round($t['secs']); Kills = $t['kills']; Dealt = [math]::Round($t['dealt'])
            Lost = [math]::Round($t['lost'], 3); Firing = $t['firing']; OnTarget = $t['on-target']; Engaged = $t['engaged']
            MissileHits = $t['missile-hits']; MissileMisses = $t['missile-misses'] }
    }
}

function Invoke-FLABBench([string]$Tag, [string]$A, [string]$B, [int]$Fights = 6, [double]$Segment = 30, [int]$TimeoutSec = 150,
                          [hashtable]$Common = @{}) {
    New-Item -ItemType Directory -Force $script:FLBenchDir | Out-Null
    $parse = { param($s) $h = @{}; foreach ($p in ($s -split ';')) { if ($p) { $k, $v = $p -split '=', 2; $h[$k] = $v } }; $h }
    $ha = & $parse $A; $hb = & $parse $B
    $out = Join-Path $script:FLBenchDir "$Tag.ab.csv"
    for ($f = 1; $f -le $Fights; $f++) {
        Enter-FLStream
        $first, $second = if ($f % 2) { $ha, $hb } else { $hb, $ha }
        foreach ($r in (Invoke-FLABRun -A $first -B $second -Common $Common -Segment $Segment -TimeoutSec $TimeoutSec)) {
            $r.Variant = if (($r.Variant -eq 'A') -eq [bool]($f % 2)) { 'A' } else { 'B' }
            $r | Add-Member Fight $f -PassThru | Export-Csv -NoTypeInformation -Append $out
        }
    }
    $null = Send-FL 'ap stop'
    Import-Csv $out | Group-Object Variant | ForEach-Object {
        $g = $_.Group; $s = { param($k) ($g | Measure-Object { [double]$_.$k } -Sum).Sum }; $kills = [math]::Max(1, (& $s 'Kills'))
        [pscustomobject]@{ Variant = $_.Name; Secs = & $s 'Secs'; Kills = & $s 'Kills'; SecsPerKill = [math]::Round((& $s 'Secs') / $kills, 1)
            LostPerKill = [math]::Round((& $s 'Lost') / $kills, 3); DealtPerFiringSec = [math]::Round((& $s 'Dealt') / [math]::Max(1, (& $s 'Firing')), 1)
            OnTargetPct = [math]::Round(100 * (& $s 'OnTarget') / [math]::Max(1, (& $s 'Engaged'))); MissileHits = & $s 'MissileHits'; MissileMisses = & $s 'MissileMisses' }
    }
}

# -UntilSpawnsDead (fixed scenarios): the run ends once the ships we spawned are gone (natural encounters arriving
# later do not stretch it); SpawnKills = our spawns destroyed while it ran, Extra = other kills.
function Invoke-FLBenchRun([string]$Name, [hashtable]$Settings = @{}, [int]$TimeoutSec = 150, [switch]$UntilSpawnsDead) {
    New-Item -ItemType Directory -Force $script:FLBenchDir | Out-Null
    $seq = Get-FLEventSeq
    $alive = { @([regex]::Match((Send-FL 'spawn list'), '^spawned \d+((?: \d+)*)').Groups[1].Value -split ' ' | Where-Object { $_ }) }
    $spawns = if ($UntilSpawnsDead) { & $alive } else { @() }
    $null = Start-FLAutopilot combat -Settings $Settings
    $sw = [Diagnostics.Stopwatch]::StartNew(); $why = 'timeout'; $stop = $null; $evts = @()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        foreach ($l in (Get-FLEvents -After $seq)) {
            $evts += $l
            if ($l -match 'autopilot stop .*why=(.+?) kills=') { $why = $matches[1]; $stop = $l }
            elseif ($l -match 'player-death') { $why = 'player-death' }
        }
        $seq = $script:FLEventSeq
        if ($why -ne 'timeout') { break }
        if ($UntilSpawnsDead -and @(& $alive).Count -eq 0) { $why = 'spawns cleared'; break }
        Start-Sleep -Milliseconds 250   # event polling interval
    }
    if ((Get-FLAutopilot).Mode -ne 'off') { $null = Send-FL 'ap stop' }   # its stop event carries the run's numbers
    foreach ($l in (Get-FLEvents -After $seq)) {
        $evts += $l
        if ($l -match 'autopilot stop') { $stop = $l }
    }
    $spawnKills = @($spawns | Where-Object { $id = $_; $evts | Where-Object { $_ -match "target-destroyed id=$id " } }).Count
    $evts | Set-Content (Join-Path $script:FLBenchDir "$Name.events.txt")
    $kv = @{}; if ($stop) { foreach ($m in [regex]::Matches($stop, '(\S+)=(\S+)')) { $kv[$m.Groups[1].Value] = $m.Groups[2].Value } }
    $e = [double]$kv['engaged']; $k = [int]$kv['kills']
    [pscustomobject]@{ Name = $Name; Why = $why; Secs = [double]$kv['time']; Kills = $k; Lost = [double]$kv['lost']; Dealt = [double]$kv['dealt']
        LostPerKill = if ($k) { [math]::Round([double]$kv['lost'] / $k, 3) } else { 0 }
        KillsPerMin = if ($e) { [math]::Round(60 * $k / $e, 2) } else { 0 }
        OnTargetPct = if ($e) { [math]::Round(100 * [double]$kv['on-target'] / $e) } else { 0 }
        Dps = [double]$kv['dps']; Extends = [int]$kv['extends']; MissileHits = [int]$kv['missile-hits']; MissileMisses = [int]$kv['missile-misses']
        Cms = @($evts | Where-Object { $_ -match 'autopilot cm ' }).Count
        Batteries = @($evts | Where-Object { $_ -match 'autopilot repair shield' }).Count
        Nanobots = @($evts | Where-Object { $_ -match 'autopilot repair hull' }).Count
        SpawnKills = $spawnKills; Extra = [math]::Max(0, $k - $spawnKills); Stop = $stop }
}
