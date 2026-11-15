# Self-test of tools\flexitwatch.ps1 without a game (13 Nov 2026): a copy of ping.exe stands in for Freelancer.exe.
# Three cases: it ends by itself (exit 0x00000000), it is stopped with Stop-Process (0xFFFFFFFF, as every test's clean-up),
# and a pid that does not exist (exit unknown). Each must leave exactly one line in the log, and every watcher must end.
# Usage (PowerShell tool): tools\flexitwatch_selftest.ps1     exit 0 = PASS, 1 = FAIL.
$ErrorActionPreference = 'Stop'
$watch = Join-Path $PSScriptRoot 'flexitwatch.ps1'
$pwshExe = (Get-Process -Id $PID).Path
$base = Join-Path ([IO.Path]::GetTempPath()) ("flexitwatch_selftest_" + [Guid]::NewGuid().ToString('N').Substring(0, 8))
$null = [IO.Directory]::CreateDirectory($base)
$log = Join-Path $base 'logs\game_exits.log'
$fails = 0
function Check([string]$what, [bool]$ok, [string]$seen = '') { Write-Host ("  {0}  {1}{2}" -f $(if ($ok) { 'ok  ' } else { 'FAIL' }), $what, $(if ($seen) { "   [$seen]" })); if (-not $ok) { $script:fails++ } }
function Start-Watch([int]$gamePid, [long]$ticks, [string]$tag) {
    Start-Process -FilePath $pwshExe -WindowStyle Hidden -PassThru -ArgumentList @('-NoProfile', '-NonInteractive', '-File', "`"$watch`"",
        '-GamePid', "$gamePid", '-StartUtcTicks', "$ticks", '-Log', "`"$log`"", '-Instance', $tag, '-Build', '"selftest build"', '-Session', 'st')
}
function Line-Of([string]$tag) { @(if (Test-Path -LiteralPath $log) { Get-Content -LiteralPath $log | Where-Object { $_ -match "`tinstance $tag`t" } }) }
try {
    Write-Host 'flexitwatch.ps1:'
    $exe = Join-Path $base 'ping.exe'
    [IO.File]::Copy((Join-Path $env:SystemRoot 'System32\PING.EXE'), $exe)
    # 1. ends by itself
    $g1 = Start-Process -FilePath $exe -ArgumentList '-n 3 127.0.0.1' -WindowStyle Hidden -PassThru
    $w1 = Start-Watch $g1.Id $g1.StartTime.ToUniversalTime().Ticks 'own'
    # 2. stopped
    $g2 = Start-Process -FilePath $exe -ArgumentList '-n 60 127.0.0.1' -WindowStyle Hidden -PassThru
    $w2 = Start-Watch $g2.Id $g2.StartTime.ToUniversalTime().Ticks 'stopped'
    Start-Sleep -Seconds 2   # (the watcher's pwsh needs a moment to open its handle; a stop before that is case 3's line)
    Stop-Process -Id $g2.Id -Force
    # 3. no such process
    $w3 = Start-Watch 999999 0 'gone'
    foreach ($w in $w1, $w2, $w3) { if (-not $w.WaitForExit(30000)) { Check "watcher pid $($w.Id) ends by itself" $false; Stop-Process -Id $w.Id -Force } }
    $l1 = @(Line-Of 'own'); $l2 = @(Line-Of 'stopped'); $l3 = @(Line-Of 'gone')   # (@(): one line would unroll to a string)
    Check 'own end: one line, exit 0x00000000' ($l1.Count -eq 1 -and $l1[0] -match "`texit 0x00000000`tlived \d+\.\d s`t") ($l1 -join ' | ')
    Check 'Stop-Process: one line, exit 0xFFFFFFFF' ($l2.Count -eq 1 -and $l2[0] -match "`texit 0xFFFFFFFF`t") ($l2 -join ' | ')
    Check 'no such pid: one line, exit unknown' ($l3.Count -eq 1 -and $l3[0] -match 'exit unknown') ($l3 -join ' | ')
    Check 'time to the millisecond, build and session in the line' ($l1.Count -eq 1 -and $l1[0] -match '^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d\.\d{3}\t' -and $l1[0] -match "`tbuild selftest build`t" -and $l1[0] -match "`tsession st$")
} finally {
    foreach ($g in $g1, $g2) { if ($g -and -not $g.HasExited) { Stop-Process -Id $g.Id -Force } }
    Start-Sleep -Milliseconds 300
    [IO.Directory]::Delete($base, $true)
}
if ($fails) { Write-Host "FAIL: $fails check(s)"; exit 1 }
Write-Host 'PASS'
exit 0
