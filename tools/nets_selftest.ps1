# Self-test of tools\nets.ps1 without a game (14 Nov 2026): a temporary "lane" holds tiny scripts, a temporary slot folder stands in for the shared one.
# Cases: (1) a net that passes, one that exits 3 and one that is missing: the log names each and the exit is 1; (2) all pass: exit 0, the slot is freed;
# (3) one slot only: a second run waits ("waiting for a game slot"), takes the slot after the first ends, and both exit 0.
# Usage (PowerShell tool): tools\nets_selftest.ps1     exit 0 = PASS, 1 = FAIL.
$ErrorActionPreference = 'Stop'
$nets = Join-Path $PSScriptRoot 'nets.ps1'
$pwshExe = (Get-Process -Id $PID).Path
$base = Join-Path ([IO.Path]::GetTempPath()) ("nets_selftest_" + [Guid]::NewGuid().ToString('N').Substring(0, 8))
$lane = Join-Path $base 'lane'; $slots = Join-Path $base 'slots'
$null = [IO.Directory]::CreateDirectory((Join-Path $lane 'tests')); $null = [IO.Directory]::CreateDirectory($slots)
$fails = 0
function Check([string]$what, [bool]$ok, [string]$seen = '') { Write-Host ("  {0}  {1}{2}" -f $(if ($ok) { 'ok  ' } else { 'FAIL' }), $what, $(if ($seen) { "   [$seen]" })); if (-not $ok) { $script:fails++ } }
function Put([string]$name, [string]$body) { [IO.File]::WriteAllText((Join-Path $lane "tests\$name"), $body) }
function Start-Nets([string]$run, [string]$log, [int]$max = 1) {
    Start-Process -FilePath $pwshExe -WindowStyle Hidden -PassThru -RedirectStandardOutput "$log.stdout" -ArgumentList @('-NoProfile', '-NonInteractive', '-File', "`"$nets`"",
        '-Lane', "`"$lane`"", '-Run', "`"$run`"", '-Out', "`"$log`"", '-SlotDir', "`"$slots`"", '-MaxGames', "$max", '-PollSec', '1', '-NoLooseCheck')
}
function Log-Of([string]$log) { if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log -Raw } else { '' } }
try {
    Put 'ok.ps1' "'fine'; exit 0"
    Put 'bad.ps1' "'a failure line'; exit 3"
    Put 'slow.ps1' "Start-Sleep 6; 'slow done'; exit 0"
    Write-Host 'nets.ps1:'
    # 1. pass, fail, missing
    $l1 = Join-Path $base 'one.log'
    $p = Start-Nets 'tests\ok.ps1 ;; tests\bad.ps1 ;; tests\absent.ps1' $l1
    if (-not $p.WaitForExit(120000)) { Stop-Process -Id $p.Id -Force; Check 'run 1 ends' $false }
    $t = Log-Of $l1
    Check 'a net that exits 3 makes the run exit 1' ($p.ExitCode -eq 1) "exit $($p.ExitCode)"
    Check 'the log has the passing net with exit=0' ($t -match 'END\s+tests\\ok\.ps1\s+exit=0')
    Check 'the log has the failing net with exit=3 and its last line' ($t -match 'END\s+tests\\bad\.ps1\s+exit=3' -and $t -match 'a failure line')
    Check 'a missing script is named' ($t -match 'MISSING .*absent\.ps1')
    Check 'the summary counts 2 ran, 2 bad' ($t -match 'NETS DONE: 2 ran, 2 with a non-zero exit or missing')
    # 2. all pass, slot freed
    $l2 = Join-Path $base 'two.log'
    $p = Start-Nets 'tests\ok.ps1 ;; tests\ok.ps1' $l2
    if (-not $p.WaitForExit(120000)) { Stop-Process -Id $p.Id -Force; Check 'run 2 ends' $false }
    Check 'all nets pass: exit 0' ($p.ExitCode -eq 0 -and (Log-Of $l2) -match 'NETS DONE: 2 ran, 0 with') "exit $($p.ExitCode)"
    $free = try { $h = [IO.File]::Open((Join-Path $slots 'slot1.lock'), 'Open', 'ReadWrite', 'Read'); $h.Close(); $true } catch { $false }
    Check 'the slot is freed when the run ends' $free
    # 3. one slot: the second run waits for the first
    $l3a = Join-Path $base 'three_a.log'; $l3b = Join-Path $base 'three_b.log'
    $a = Start-Nets 'tests\slow.ps1' $l3a
    $sw = [Diagnostics.Stopwatch]::StartNew(); while ((Log-Of $l3a) -notmatch 'START' -and $sw.Elapsed.TotalSeconds -lt 60) { Start-Sleep -Milliseconds 300 }
    $b = Start-Nets 'tests\ok.ps1' $l3b
    foreach ($q in $a, $b) { if (-not $q.WaitForExit(120000)) { Stop-Process -Id $q.Id -Force; Check "run 3 ends" $false } }
    $ta = Log-Of $l3a; $tb = Log-Of $l3b
    Check 'both runs exit 0' ($a.ExitCode -eq 0 -and $b.ExitCode -eq 0) "exits $($a.ExitCode), $($b.ExitCode)"
    Check 'the second run said it was waiting for a game slot' ($tb -match 'waiting for a game slot \(1 slots held')
    Check 'the second run took the slot only after waiting' ($tb -match 'slot taken after [1-9]\d* s')
    Check 'the first run took its slot at once' ($ta -match 'slot taken after 0 s')
    Check 'the second run started its net after the first ended' ($tb -match 'START tests\\ok\.ps1')
} finally {
    foreach ($q in $p, $a, $b) { if ($q -and -not $q.HasExited) { Stop-Process -Id $q.Id -Force } }
    Start-Sleep -Milliseconds 300
    try { [IO.Directory]::Delete($base, $true) } catch { Write-Host "  (temp folder $base not removed: $($_.Exception.Message))" }
}
if ($fails) { Write-Host "nets_selftest: $fails FAILED"; exit 1 }
Write-Host 'nets_selftest: PASS'
