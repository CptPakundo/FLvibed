# The leg gate (HANDOFF P2b; user, 23 Oct 2026: "no regressions elsewhere, or else it'll be a loop of improvements and
# regressions"): every mission leg once, against its best full-leg time in research\experience\legs\legs.tsv. One
# pwsh per instance, each with its own queue of legs (flleg.ps1 one after another); three at once, balanced by the
# legs' lengths (-Queue a / b / c). Each leg's line goes to research\experience\legs\sets\gate_<tag>.tsv: leg, save,
# outcome, seconds, best before, the difference, verdict (OK / SLOWER = more than 15 % over the best / FAIL); the
# checkpoint-by-checkpoint comparison with the last gate: tools\splits.py compare 'gate-<last>$' 'gate-<tag>$'. The
# tables (docs\LEGS.md, splits.py update) are regenerated once after the queue (every leg runs with -NoTables).
# Usage (three background pwsh): tools\flgate.ps1 -Instance 1 -Queue a -Tag s8   (2 b, 3 c)
param([Parameter(Mandatory)][int]$Instance, [Parameter(Mandatory)][ValidateSet('a', 'b', 'c', 'all')][string]$Queue,
      [Parameter(Mandatory)][string]$Tag, [string[]]$Only = @())
$root = Split-Path $PSScriptRoot -Parent
$saves = [ordered]@{
    M01a = 'Save7e1c.fl'; M01b = 'Save5f61.fl'; M02 = 'Save05c7.fl'; M03 = 'Save1e7d.fl'; M04 = 'Save6380.fl'
    M05 = 'Save1d87.fl'; M06 = 'Save33fa.fl'; M07 = 'Save766f.fl'; M08 = 'Save3a48.fl'; M09 = 'Save4686.fl'
    M10 = 'Save7b39.fl'; M11 = 'Save08f9.fl'; M12 = 'Save5e67.fl'; M13 = 'Save3989.fl'
}
# (balanced by the bests of 23 Oct 2026: ~4,600-5,000 s each)
$queues = @{
    a = 'M07', 'M05', 'M03', 'M01a'
    b = 'M10', 'M13', 'M04', 'M08', 'M12'
    c = 'M06', 'M11', 'M02', 'M09', 'M01b'
    all = @($saves.Keys)
}
# (-Only M04,M01b through pwsh -File arrives as one string: split it - session 8, a "M04,M01b" leg failed at 0 s)
$legs = if ($Only.Count) { @($Only | ForEach-Object { $_ -split ',' } | Where-Object { $_ }) } else { $queues[$Queue] }
foreach ($l in $legs) { if (-not $saves.Contains($l)) { throw "flgate: unknown leg '$l' (known: $($saves.Keys -join ', '))" } }
$hist = Join-Path $root 'research\experience\legs\legs.tsv'
$out = Join-Path $root "research\experience\legs\sets\gate_$Tag.tsv"
$null = New-Item -ItemType Directory -Force (Split-Path $out)
if (-not (Test-Path $out)) { Set-Content $out "time`tleg`tsave`toutcome`tsecs`tbest`tdiff`tverdict`tinstance" }
foreach ($leg in $legs) {
    $save = $saves[$leg]
    # the best full leg before this run (PASS rows from the leg's start save)
    $best = (Import-Csv $hist -Delimiter "`t" | Where-Object { $_.save -eq $save -and $_.outcome -eq 'PASS' } |
             ForEach-Object { [int]$_.secs } | Measure-Object -Minimum).Minimum
    $res = & (Join-Path $root 'controller\tools\flleg.ps1') -Instance $Instance -Save $save -Label "$leg-gate-$Tag" -NoTables 2>&1 |
           ForEach-Object { "$_" }
    $line = "$($res | Where-Object { $_ -match '^RESULT ' } | Select-Object -Last 1)"
    $ok = $line -match '^RESULT PASS'
    $secs = [int][regex]::Match($line, ' (\d+) s ').Groups[1].Value
    $diff = if ($best) { $secs - $best } else { '' }
    $verdict = if (-not $ok) { 'FAIL' } elseif ($best -and $secs -gt 1.15 * $best) { 'SLOWER' } else { 'OK' }
    Add-Content $out ("{0:HH:mm}`t{1}`t{2}`t{3}`t{4}`t{5}`t{6}`t{7}`t{8}" -f (Get-Date), $leg, $save, $(if ($ok) { 'PASS' } else { 'FAIL' }),
                      $secs, $best, $diff, $verdict, $Instance)
    "GATE $leg $verdict $secs s (best $best)"
}
# the tables once per queue: the same two commands flleg.ps1 runs after a single leg (docs\LEGS.md from the history; the
# split history, PBs, golds, limits, docs\SPLITS.md and the LiveSplit files)
try { & node (Join-Path $root 'tools\legtable.js') } catch { "legtable: $_" }
try { $null = & (Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe') -I (Join-Path $root 'tools\splits.py') update 2>&1 }
catch { "splits: $_" }
"tables regenerated (LEGS.md, splits update)"
"GATE DONE queue $Queue"
