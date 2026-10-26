# Benchmark history (user, 20 Oct 2026: time every repeated test's sub-sections in context - ship, loadout, settings,
# build - so the data shows regressions and chances to improve). One append-only table,
# research\experience\bench\bench.tsv, one row per timed sub-section:
#   time      when it ended (yyyy-MM-dd HH:mm:ss)
#   commit    FLvibed HEAD (short hash, "+" = uncommitted changes in the build)
#   suite     what ran: sweep, fights, mission M05, ...
#   case      the sub-section: a sweep section, a fight scenario, a mission objective (its ids: "21660>22920")
#   context   what it ran with: the fixture / ship tag and loadout ("c705"), settings changed from the defaults
#   outcome   PASS / FAIL, won / died / retreated, SUCCESS / FAILED
#   seconds   time to completion
#   stats     key=value extras (kills, lost, dps, turns, odds ...)
# Usage (dot-source):  . tools\flbench.ps1
#   Add-FLBench -Suite fights -Case heavy_d12_x2 -Context c705 -Outcome won -Seconds 95 -Stats 'kills=2'
#   Import-FLBenchHistory          # backfill from research\experience\runs (fights_*.tsv, sweep_*.txt); idempotent
#   Show-FLBench [-Suite fights] [-Last 1] [-Threshold 0.15]
#                                  # each case's latest run vs the median of its earlier runs in the same context:
#                                  # SLOWER / FASTER beyond the threshold, outcome changes (won -> died) as REGRESSION
$script:FLBenchRoot = Split-Path $PSScriptRoot -Parent
$script:FLBenchFile = Join-Path $script:FLBenchRoot 'research\experience\bench\bench.tsv'
$script:FLBenchCols = 'time', 'commit', 'suite', 'case', 'context', 'outcome', 'seconds', 'stats'

function Get-FLBenchCommit([datetime]$At) {
    # The commit the build had: HEAD now (plus "+" when dirty); for a past time the last commit before it.
    Push-Location $script:FLBenchRoot
    try {
        if ($null -ne $At -and $At -ne [datetime]::MinValue) { $b = '--before=' + $At.ToString('s'); return (git log -1 $b --format=%h 2>$null) }
        $h = git rev-parse --short HEAD 2>$null
        if (git status --porcelain --untracked-files=no 2>$null) { $h += '+' }
        $h
    } finally { Pop-Location }
}

function Add-FLBench([Parameter(Mandatory)][string]$Suite, [Parameter(Mandatory)][string]$Case, [string]$Context = '',
                     [string]$Outcome = '', [Parameter(Mandatory)][double]$Seconds, [string]$Stats = '',
                     [datetime]$Time = (Get-Date), [string]$Commit) {
    if (-not (Test-Path $script:FLBenchFile)) {
        $null = New-Item -ItemType Directory -Force (Split-Path $script:FLBenchFile)
        ($script:FLBenchCols -join "`t") | Set-Content $script:FLBenchFile -Encoding utf8
    }
    if (-not $Commit) { $Commit = Get-FLBenchCommit }
    $clean = { param($s) "$s" -replace '[\t\r\n]+', ' ' }
    (@($Time.ToString('yyyy-MM-dd HH:mm:ss'), $Commit, $Suite, $Case, $Context, $Outcome, [Math]::Round($Seconds, 1),
       $Stats) | ForEach-Object { & $clean $_ }) -join "`t" | Add-Content $script:FLBenchFile -Encoding utf8
}

# A mission's time per objective, from the game's own objective events (no hand-made step markers, any mission): the
# event lines ("<seq> <tick ms> <text>") of one run; each objective (obj=<ids> "<text>") lasts until the next different
# one, the last until the run's last event. Case = "<ids> <text>" so the same objective lines up across runs.
function Add-FLBenchObjectives([Parameter(Mandatory)][string]$Suite, [string]$Context = '', [string[]]$Lines) {
    $marks = @()
    foreach ($l in $Lines) {
        $m = [regex]::Match("$l", '^\d+ (\d+) objective .*?obj=(\d+) "([^"]*)"')
        if ($m.Success -and (-not $marks.Count -or $marks[-1].id -ne $m.Groups[2].Value) -and $m.Groups[2].Value -ne '1') {
            $marks += [pscustomobject]@{ tick = [double]$m.Groups[1].Value; id = $m.Groups[2].Value; text = $m.Groups[3].Value }
        }
    }
    if (-not $marks.Count) { return }
    $lastTick = [double]([regex]::Match("$($Lines[-1])", '^\d+ (\d+)').Groups[1].Value)
    for ($i = 0; $i -lt $marks.Count; $i++) {
        $end = if ($i + 1 -lt $marks.Count) { $marks[$i + 1].tick } else { $lastTick }
        $text = $marks[$i].text; if ($text.Length -gt 50) { $text = $text.Substring(0, 50) }
        Add-FLBench -Suite $Suite -Case "$($marks[$i].id) $text" -Context $Context -Seconds (($end - $marks[$i].tick) / 1000) `
            -Outcome $(if ($i + 1 -lt $marks.Count) { 'done' } else { 'last' })
    }
}

function Get-FLBench { if (Test-Path $script:FLBenchFile) { Import-Csv $script:FLBenchFile -Delimiter "`t" } }

# One-time backfill of the runs before the live rows (fights / sweep write their own rows since 20 Oct 2026 04:30).
function Import-FLBenchHistory([datetime]$Before = '2026-10-20 04:30') {
    $runs = Join-Path $script:FLBenchRoot 'research\experience\runs'
    $have = @{}; Get-FLBench | ForEach-Object { $have["$($_.time)|$($_.suite)|$($_.case)|$($_.context)"] = 1 }
    $script:n = 0
    $add = { param($t, $suite, $case, $ctx, $out, $sec, $stats)
        $k = "$($t.ToString('yyyy-MM-dd HH:mm:ss'))|$suite|$case|$ctx"
        if (-not $have[$k]) { Add-FLBench -Suite $suite -Case $case -Context $ctx -Outcome $out -Seconds $sec -Stats $stats `
                                           -Time $t -Commit (Get-FLBenchCommit $t); $have[$k] = 1; $script:n++ } }
    foreach ($f in Get-ChildItem $runs -Filter 'fights_*.tsv' | Where-Object LastWriteTime -lt $Before) {
        $m = [regex]::Match($f.Name, '_(\d{4}-\d\d-\d\d)_(\d\d)(\d\d)\.tsv$')
        $t0 = [datetime]::ParseExact("$($m.Groups[1].Value) $($m.Groups[2].Value):$($m.Groups[3].Value)", 'yyyy-MM-dd HH:mm', $null)
        $i = 0
        foreach ($r in Import-Csv $f.FullName -Delimiter "`t") {
            $out = if ($r.retreated -eq 'True' -and $r.outcome -ne 'won') { "$($r.outcome)+retreat" } else { $r.outcome }
            & $add $t0.AddSeconds(++$i) 'fights' $r.scenario "$($r.ship) caution=$($r.caution)" $out ([double]$r.seconds) `
                "kills=$($r.kills) odds=$($r.odds_prior)/$($r.odds_measured)"
        }
    }
    foreach ($f in Get-ChildItem $runs -Filter 'sweep_*.txt' | Where-Object LastWriteTime -lt $Before) {
        $m = [regex]::Match($f.Name, '_(\d{4}-\d\d-\d\d)_\d{4}\.txt$'); $day = $m.Groups[1].Value
        foreach ($l in Get-Content $f.FullName) {
            $s = [regex]::Match($l, '^(\d\d:\d\d:\d\d) (PASS|FAIL) (\S+) \(([\d,]+) s(?:, (\w+))?\)')
            if (-not $s.Success -or $l -match 'build failed') { continue }
            & $add ([datetime]::ParseExact("$day $($s.Groups[1].Value)", 'yyyy-MM-dd HH:mm:ss', $null)) 'sweep' `
                $s.Groups[3].Value $s.Groups[5].Value $s.Groups[2].Value ([double]($s.Groups[4].Value -replace ',', '')) ''
        }
    }
    "imported $script:n rows into $script:FLBenchFile"
}

function Show-FLBench([string]$Suite, [int]$Last = 1, [double]$Threshold = 0.15) {
    $rows = @(Get-FLBench | Where-Object { -not $Suite -or $_.suite -eq $Suite })
    $good = 'PASS', 'won', 'SUCCESS'
    foreach ($g in $rows | Group-Object suite, case, context) {
        $all = @($g.Group | Sort-Object time)
        foreach ($cur in $all | Select-Object -Last $Last) {
            $prev = @($all | Where-Object { $_.time -lt $cur.time })
            $ok = @($prev | Where-Object { $_.outcome -in $good } | ForEach-Object { [double]$_.seconds } | Sort-Object)
            $med = if ($ok.Count) { $ok[[int][Math]::Floor(($ok.Count - 1) / 2)] } else { $null }
            $delta = if ($med -and $cur.outcome -in $good) { ([double]$cur.seconds - $med) / $med } else { $null }
            $flag = if ($prev.Count -and $cur.outcome -notin $good -and ($prev | Select-Object -Last 3).outcome -contains ($good | Where-Object { $_ -in $prev.outcome } | Select-Object -First 1)) { 'REGRESSION' }
                    elseif ($null -ne $delta -and $delta -gt $Threshold) { 'SLOWER' }
                    elseif ($null -ne $delta -and $delta -lt -$Threshold) { 'FASTER' } else { '' }
            [pscustomobject]@{ suite = $cur.suite; case = $cur.case; context = $cur.context; runs = $all.Count
                               latest = $cur.seconds; median = $med; delta = if ($null -ne $delta) { '{0:+0%;-0%}' -f $delta } else { '' }
                               outcome = $cur.outcome; commit = $cur.commit; flag = $flag }
        }
    }
}
