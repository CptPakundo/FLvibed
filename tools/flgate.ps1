# The leg gate (HANDOFF P2b; user, 23 Oct 2026: "no regressions elsewhere, or else it'll be a loop of improvements and
# regressions"): every mission leg once, against its best full-leg time in research\experience\legs\legs.tsv. One
# pwsh per instance, each with its own queue of legs (flleg.ps1 one after another); three at once, balanced by the
# legs' lengths (-Queue a / b / c). Each leg's line goes to research\experience\legs\sets\gate_<tag>.tsv: leg, save,
# outcome, seconds, best before, the difference, verdict (OK / SLOWER = more than 15 % over the best / FAIL); the
# checkpoint-by-checkpoint comparison with the last gate: tools\splits.py compare 'gate-<last>$' 'gate-<tag>$'. The
# tables (docs\LEGS.md, splits.py update) are regenerated once after the queue (every leg runs with -NoTables).
# Usage (three background pwsh): tools\flgate.ps1 -Instance 1 -Queue a -Tag s8   (2 b, 3 c)
# -ApSet 'k v','k2 v2': autopilot switches sent after each boot (flleg.ps1 -ApSet), e.g. a measurement gate with
#   'situation_shadow 1' (WORK_QUEUE Q6 S5: every leg with the A37 shadow on; its agree / differ lines in each leg's
#   FLController.log, tools\shadow_digest.py). The tag should say so: the gate's rows are judged like any other gate.
# -Queue p<k>of<n> (WORK_QUEUE H18, the owner 30 Oct 2026: gates should cost less of the PC's time): the 14 legs split
#   into n queues balanced by their best full-leg times (longest first, each to the shortest queue), this driver runs queue
#   k - measured 30 Oct: three games at ~190 fps used 23 % CPU of 32 threads, ~25 % GPU each, 66 GB RAM free, so six
#   instances fit (tools\flgate_all.ps1 starts them). a / b / c stay the fixed three-queue form.
# -FreshSaves: the leg also saves at each of its checkpoints that has no fit or unverified save in
#   docs\missions\scene_saves.tsv (Leg.ps1 -SaveAt: ~0.35 s of harness time, the game keeps drawing) - the gate refreshes the
#   segment fixtures (an unfit save such as SaveS349 gets a successor) without saving where a good one exists.
# Since 30 Oct 2026 evening (the compliance audit: no gate had harvested anything since H18 was built) the harvest is the
# default: fresh saves on (-NoFreshSaves turns them off; -FreshSaves is kept for old command lines), and after the queue
# the driver writes the gate's A samples (gate_harvest.py --out sets\gate_<tag>_A.tsv) and its distance to par
# (gapreport.py gate <tag> -> sets\gate_<tag>_par.txt, printed): a gate result is reported as BEAT PAR / AT PAR /
# CLOSER / SAME / FURTHER, never "OK" alone (the owner, 30 Oct 2026).
param([Parameter(Mandatory)][int]$Instance, [Parameter(Mandatory)][ValidatePattern('^(a|b|c|all|p\d+of\d+)$')][string]$Queue,
      [Parameter(Mandatory)][string]$Tag, [string[]]$Only = @(), [string[]]$ApSet = @(), [switch]$FreshSaves, [switch]$NoFreshSaves,
      # -ListOnly: print this driver's legs (and with -FreshSaves the checkpoints it would save at), start nothing
      [switch]$ListOnly)
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'fllegrun.ps1')   # Invoke-FLLegGuarded
$FreshSaves = -not $NoFreshSaves
# the legs' start saves: docs\legs_reference.tsv, as flset.ps1 reads them (a copy here until 29 Oct 2026)
$saves = [ordered]@{}
foreach ($r in Import-Csv (Join-Path $root 'docs\legs_reference.tsv') -Delimiter "`t") { if ($r.start_save) { $saves[$r.leg] = $r.start_save } }
if ($saves.Count -lt 14) { throw "flgate: docs\legs_reference.tsv lists $($saves.Count) legs with a start save (14 expected)" }
# (balanced by the bests of 23 Oct 2026: ~4,600-5,000 s each)
$queues = @{
    a = 'M07', 'M05', 'M03', 'M01a'
    b = 'M10', 'M13', 'M04', 'M08', 'M12'
    c = 'M06', 'M11', 'M02', 'M09', 'M01b'
    all = @($saves.Keys)
}
# (-Only M04,M01b through pwsh -File arrives as one string: split it - session 8, a "M04,M01b" leg failed at 0 s)
$hist = Join-Path $root 'research\experience\legs\legs.tsv'
if ($Queue -match '^p(\d+)of(\d+)$') {
    $k = [int]$Matches[1]; $n = [int]$Matches[2]
    if ($k -lt 1 -or $k -gt $n -or $n -gt 14) { throw "flgate: -Queue $Queue (k in 1..n, n up to 14)" }
    $rows = Import-Csv $hist -Delimiter "`t"
    $est = [ordered]@{}
    foreach ($l in $saves.Keys) {   # (the leg's best full PASS = splits.py full_pass_row, as below; none: 900 s)
        $b = ($rows | Where-Object { $_.save -eq $saves[$l] -and $_.outcome -eq 'PASS' -and $_.detail -like 'SUCCESS*' -and $_.instance -match '^\d*$' } |
              ForEach-Object { [int]$_.secs } | Measure-Object -Minimum).Minimum
        $est[$l] = if ($b) { $b } else { 900 }
    }
    $load = @(0) * $n; $part = @(1..$n | ForEach-Object { , @() })
    foreach ($l in @($est.Keys | Sort-Object { - $est[$_] }, { $_ })) {   # (longest first, ties by name: every driver computes the same split)
        $j = 0; for ($i = 1; $i -lt $n; $i++) { if ($load[$i] -lt $load[$j]) { $j = $i } }
        $part[$j] += $l; $load[$j] += $est[$l]
    }
    $queues[$Queue] = $part[$k - 1]
    "flgate queue $Queue : $($part[$k - 1] -join ', ') (~$($load[$k - 1]) s)"
}
$legs = if ($Only.Count) { @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ }) } else { $queues[$Queue] }
foreach ($l in $legs) { if (-not $saves.Contains($l)) { throw "flgate: unknown leg '$l' (known: $($saves.Keys -join ', '))" } }
# (-ApSet 'a 1','b 2' through pwsh -File arrives as one string: split on commas, as flset.ps1 does)
$sets = @($ApSet | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })
$hist = Join-Path $root 'research\experience\legs\legs.tsv'
$out = Join-Path $root "research\experience\legs\sets\gate_$Tag.tsv"
$null = New-Item -ItemType Directory -Force (Split-Path $out)
if (-not $ListOnly -and -not (Test-Path $out)) { Set-Content $out "time`tleg`tsave`toutcome`tsecs`tbest`tdiff`tverdict`tinstance" }
foreach ($leg in $legs) {
    $save = $saves[$leg]
    # the best full leg before this run (PASS rows from the leg's start save that finished the leg: "SUCCESS total" -
    # a stop-at segment run, "UNTIL total", is no full leg: gate q3s2 took M13's 141 s segment runs for its best, 30 Oct 2026)
    $best = (Import-Csv $hist -Delimiter "`t" | Where-Object { $_.save -eq $save -and $_.outcome -eq 'PASS' -and $_.detail -like 'SUCCESS*' -and $_.instance -match '^\d*$' } |   # (= splits.py full_pass_row)
             ForEach-Object { [int]$_.secs } | Measure-Object -Minimum).Minimum
    # under the wall-clock watchdog (tools\fllegrun.ps1): a hung leg no longer stalls the queue
    $p = @{ Instance = $Instance; Save = $save; Label = "$leg-gate-$Tag"; NoTables = $true }
    if ($sets.Count) { $p.ApSet = $sets }   # (only with a value: fllegrun.ps1's note on empty arrays)
    if ($FreshSaves) {
        $cps = @(Import-Csv (Join-Path $root 'docs\checkpoints.tsv') -Delimiter "`t" |
                 Where-Object { $_.leg -eq $leg -and $_.cp -like "$leg.*" -and $_.cp -notmatch '\.(control|end)$' } | ForEach-Object { $_.cp })
        $have = @(Import-Csv (Join-Path $root 'docs\missions\scene_saves.tsv') -Delimiter "`t" |
                  Where-Object { $_.mission -eq $leg -and $_.source -eq 'leg-checkpoint' -and $_.fit -in 'fit', 'fit-load', 'unverified' } | ForEach-Object { $_.step })
        # (at most 3 per leg: the dry run of 30 Oct found 4-17 missing per leg; the segments with the biggest gap to the
        # runners first when tools\gapreport.py ranks them, else the leg's order)
        $want = @($cps | Where-Object { $_ -notin $have })
        $rank = Join-Path $root 'tools\gapreport.py'
        if ($want.Count -gt 3 -and (Test-Path $rank)) {
            $order = @(& (Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe') -I $rank rank $leg 2>$null)
            if ($order.Count) { $want = @($order | Where-Object { $_ -in $want }) + @($want | Where-Object { $_ -notin $order }) }
        }
        $want = @($want | Select-Object -First 3)
        if ($want.Count) { $p.SaveAt = $want; "GATE $leg fresh saves at: $($want -join ', ')" }
    }
    if ($ListOnly) { "GATE $leg (list only)"; continue }
    $res = Invoke-FLLegGuarded $p
    $line = "$($res | Where-Object { $_ -match '^RESULT ' } | Select-Object -Last 1)"
    $ok = $line -match '^RESULT PASS'
    $secs = [int][regex]::Match($line, ' (\d+) s ').Groups[1].Value
    $diff = if ($best) { $secs - $best } else { '' }
    $verdict = if (-not $ok) { 'FAIL' } elseif ($best -and $secs -gt 1.15 * $best) { 'SLOWER' } else { 'OK' }
    Add-Content $out ("{0:HH:mm}`t{1}`t{2}`t{3}`t{4}`t{5}`t{6}`t{7}`t{8}" -f (Get-Date), $leg, $save, $(if ($ok) { 'PASS' } else { 'FAIL' }),
                      $secs, $best, $diff, $verdict, $Instance)
    "GATE $leg $verdict $secs s (best $best)"
}
if ($ListOnly) { return }
# the tables once per queue: the same two commands flleg.ps1 runs after a single leg (docs\LEGS.md from the history; the
# split history, PBs, golds, limits, docs\SPLITS.md and the LiveSplit files)
try { & node (Join-Path $root 'tools\legtable.js') } catch { "legtable: $_" }
try { $null = & (Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe') -I (Join-Path $root 'tools\splits.py') update 2>&1 }
catch { "splits: $_" }
"tables regenerated (LEGS.md, splits update)"
# the harvest (A samples of this gate's legs, every driver of the gate rewrites it with what has finished) and the
# distance to par (the owner's verdict words)
$py = Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe'
$sets = Join-Path $root 'research\experience\legs\sets'
$hv = @('-I', (Join-Path $root 'tools\gate_harvest.py'), '--gate', $Tag, '--out', (Join-Path $sets "gate_${Tag}_A.tsv"), '--quiet')
if (@($ApSet).Count) { $hv += @('--apset', (@($ApSet | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ }) -join ',')) }
try { & $py @hv 2>&1 | Select-Object -Last 3 } catch { "harvest: $_" }
try {
    $par = @(& $py -I (Join-Path $root 'tools\gapreport.py') gate $Tag 2>&1)
    Set-Content -LiteralPath (Join-Path $sets "gate_${Tag}_par.txt") -Value $par
    $par
} catch { "gapreport: $_" }
"GATE DONE queue $Queue"
