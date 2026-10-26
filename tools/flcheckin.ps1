# Test check-in (user, 20 Oct 2026: "this should've been picked up by periodical log sampling and screenshot
# checking"): waits -IntervalSec, then prints a digest per game instance - running or not, the objective, the
# autopilot's last decision / mode / tactic, the last fight assessment (odds, our hit points), deaths, repairs, CM
# drops since the start, and the newest sampler screenshot (look at it). Run it in the background during long tests and
# again after each report: every report is a check-in. Unexpected behaviour = stop the run, read the logs, fix.
# Cheap by design (user: check-ins can be expensive, optimize them): the digest is text; each instance's previous
# digest is kept (research\experience\checkin\i<n>.json) and ATTENTION lines name what changed for the worse - a new
# death, no objective progress since the last check-in (stall), 3+ repairs since the last one, the game gone, a FROZEN
# game (the process alive, no frame in 3 s: tools\flfreeze.ps1 saves per-thread CPU, where the busy threads run and our
# logs to research\experience\freeze\ on every check-in while it lasts; the next one says when it resumed). Open the
# shot only on ATTENTION, plus every 4th check-in as a spot check (SPOT).
# Usage: & tools\flcheckin.ps1 [-Instances 1,2,3] [-IntervalSec 120]   (0 = the main install)
param([int[]]$Instances = @(0), [int]$IntervalSec = 120)
$root = Split-Path $PSScriptRoot -Parent
$stateDir = Join-Path $root 'research\experience\checkin'
New-Item -ItemType Directory -Force $stateDir | Out-Null
. (Join-Path $root 'tools\flfreeze.ps1') # Get-FLHeartbeat, Save-FLFreezeEvidence
Start-Sleep -Seconds $IntervalSec
# Frozen = the process alive, its frame heartbeat unchanged over 3 s (all instances in one window; tools\flfreeze.ps1)
$beat0 = @{}
foreach ($i in $Instances) { $env:FLVIBED_INSTANCE = if ($i) { "$i" } else { $null }; $beat0[$i] = Get-FLHeartbeat }
Start-Sleep -Seconds 3
foreach ($i in $Instances) {
    $env:FLVIBED_INSTANCE = if ($i) { "$i" } else { $null }
    . (Join-Path $root 'tools\fldata.ps1')
    $exe = Join-Path (Get-FLInstallDir) 'EXE'
    $log = Join-Path $exe 'FLController.log'
    $proc = Get-FLGameProcess
    $run = [bool]$proc
    $beat = Get-FLHeartbeat
    $frozen = $run -and $null -ne $beat0[$i] -and $beat -eq $beat0[$i]
    $l = if (Test-Path $log) { @(Get-Content -LiteralPath $log -Tail 4000) } else { @() }
    $last = { param($p) ($l | Where-Object { $_ -match $p } | Select-Object -Last 1) -replace '^\[([\d:.]+)\] event ', '$1 ' }
    $count = { param($p) @($l | Where-Object { $_ -match $p }).Count }
    $obj = & $last 'event objective count=.*text=\d+'
    $obj = if ($obj -match '\] event objective .*obj=\d+ "([^"]*)"' -or $obj -match 'obj=\d+ "([^"]*)"') { $Matches[1] } else { '-' }
    $shots = Get-ChildItem (Join-Path $root 'research\experience\shots') -Directory -Filter "*_i$i" -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime | Select-Object -Last 1
    if (-not $i) { $shots = Get-ChildItem (Join-Path $root 'research\experience\shots') -Directory | Where-Object Name -notmatch '_i\d+$' | Sort-Object LastWriteTime | Select-Object -Last 1 }
    $shot = if ($shots) { Get-ChildItem $shots.FullName -Filter '*.jpg' | Sort-Object LastWriteTime | Select-Object -Last 1 } else { $null }
    $deaths = & $count 'event player-death'; $reps = & $count 'autopilot repair '
    $objSeq = & $count 'event objective count='
    $sf = Join-Path $stateDir "i$i.json"
    $prev = if (Test-Path $sf) { Get-Content -Raw $sf | ConvertFrom-Json } else { $null }
    $n = if ($prev) { [int]$prev.n + 1 } else { 1 }
    $att = @()
    if ($prev) {
        $result = & $last 'mission-message type=(SUCCESS|FAILURE)'
        if ($prev.running -and -not $run -and $result -notmatch 'SUCCESS') { $att += 'game gone' }   # closed after a result: done
        if ($deaths -gt $prev.deaths) { $att += "death (+$($deaths - $prev.deaths))" }
        # a docked scene (Benford's Van Pelt talk ~3.5 min) is no stall; docked / in a cutscene longer than 6 min is
        $modeLine = & $last 'event mode \w+ -> '
        $modeNow = [regex]::Match($modeLine, '-> (\w+)').Groups[1].Value
        $modeAt = [regex]::Match($modeLine, '^([\d:.]+)').Groups[1].Value
        $modeAge = if ($modeAt) { ((Get-Date) - [datetime]::ParseExact($modeAt.Substring(0, 8), 'HH:mm:ss', $null)).TotalSeconds } else { 0 }
        $quietStop = $modeNow -in 'docked', 'cutscene' -and $modeAge -lt 360
        if ($run -and $objSeq -eq $prev.objSeq -and $obj -eq $prev.obj -and -not $quietStop) { $att += "no objective progress since the last check-in ($obj)" }
        if ($reps - $prev.reps -ge 3) { $att += "$($reps - $prev.reps) repairs since the last check-in" }
        if (-not $frozen -and $prev.frozenSince) { $att += "resumed after a freeze (seen since $($prev.frozenSince); evidence $($prev.freezeDir))" }
    }
    $frozenSince = if ($frozen) { if ($prev -and $prev.frozenSince) { $prev.frozenSince } else { Get-Date -Format 'HH:mm:ss' } } else { $null }
    $freezeDir = if ($prev -and $prev.freezeDir -and $frozen) { $prev.freezeDir } else { $null }
    if ($frozen) {
        # evidence on every check-in while it lasts (where the threads are now: a loop or a wait)
        $resp = try { $proc.Responding } catch { '?' }
        $freezeDir = Save-FLFreezeEvidence ([pscustomobject]@{ Heartbeat = $beat; WindowSec = 3; Responding = $resp; Since = $frozenSince })
        $att += "FROZEN: no frame in 3 s (since $frozenSince, window responding=$resp); evidence $freezeDir (threads.txt first)"
    }
    @{ n = $n; running = $run; deaths = $deaths; reps = $reps; objSeq = $objSeq; obj = $obj; frozenSince = $frozenSince
       freezeDir = $freezeDir } | ConvertTo-Json | Set-Content $sf
    "== i$i $(if ($run) { 'RUNNING' } else { 'not running' }) | objective: $obj"
    if ($att) { "   ATTENTION i${i}: $($att -join '; ') -> look at the shot, read the log" }
    elseif ($n % 4 -eq 0) { "   SPOT i${i}: routine look at the shot" }
    "   decide: $(& $last 'objective decide reason')"
    "   mode:   $(& $last 'autopilot start mode')"
    "   tactic: $(& $last 'autopilot tactic=') | odds: $([regex]::Match("$(& $last 'autopilot odds=')", 'odds=\S+ .*?our-hp=\d+').Value)"
    "   deaths $(& $count 'event player-death') | repairs $(& $count 'autopilot repair ') | cm $(& $count 'autopilot cm ') | result: $(& $last 'mission-message type=(SUCCESS|FAILURE)')"
    "   shot: $(if ($shot) { $shot.FullName } else { '-' })"
}
$env:FLVIBED_INSTANCE = $null
