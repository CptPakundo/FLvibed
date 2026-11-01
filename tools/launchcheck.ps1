# The live-state read of every leg run started in the last 30 min (the owner, 30 Oct 2026: the two stuck instances of
# that evening were found by the owner, not the driver; made mechanical the same evening - tools\claude\hooks\launch-check.js
# refuses a turn end until this ran 60 s or more after each such start). Per run: its age, the transcript's last line
# and how long ago it was written, the save / fit / harness / stall lines, the result if any, and a flag:
#   SILENT  - no transcript line for over 90 s while the run has no result (a hang the watchdog may not see)
#   HARNESS / STALL / FAILED / unfit - read the run, classify it (a harness bug is fixed, never counted as a result)
# Writes ..\..\.claude\launchcheck_last.txt (the hook's stamp). Usage: tools\launchcheck.ps1 [-WindowMin 30]
param([int]$WindowMin = 30)
$root = Split-Path $PSScriptRoot -Parent
$legs = Join-Path $root 'research\experience\legs'
$now = Get-Date
$runs = @(Get-ChildItem -LiteralPath $legs -Directory | Where-Object { $_.Name -match '^\d{4}-\d\d-\d\d_\d{4}_' } |
          ForEach-Object { $t = Join-Path $_.FullName 'transcript.txt'; if (Test-Path -LiteralPath $t) { Get-Item -LiteralPath $t } } |
          Where-Object { ($now - $_.CreationTime).TotalMinutes -lt $WindowMin } | Sort-Object CreationTime)
if (-not $runs.Count) { "launchcheck: no leg run started in the last $WindowMin min" }
# Boot = the transcript's start to its first step line (before it the harness only waits for the game's shared memory).
# A run still booting is judged against the p99 of the last 40 boots (audit D7, 31 Oct 2026: a 113 s boot vs p99 18 s
# was read as a hang): "slow, progressing" while the instance's game log still grows, "no progress" when it does not.
function Get-BootSec($tr) {
    $first = Get-Content -LiteralPath $tr.FullName -TotalCount 400 -ErrorAction SilentlyContinue |
             Where-Object { $_ -match '^(\d\d):(\d\d):(\d\d) ' } | Select-Object -First 1
    if (-not $first) { return $null }
    $c = $tr.CreationTime; $at = $c.Date.Add([TimeSpan]::Parse($first.Substring(0, 8)))
    if ($at -lt $c.AddMinutes(-1)) { $at = $at.AddDays(1) }
    [Math]::Max(0, ($at - $c).TotalSeconds)
}
$bootP99 = $null
if ($runs.Count) {
    $hist = @(Get-ChildItem -LiteralPath $legs -Directory | Where-Object { $_.Name -match '^\d{4}-\d\d-\d\d_\d{4}_' } |
              Sort-Object Name | Select-Object -Last 60 | ForEach-Object { $t = Join-Path $_.FullName 'transcript.txt'
                  if (Test-Path -LiteralPath $t) { Get-BootSec (Get-Item -LiteralPath $t) } } |
              Where-Object { $_ -ne $null } | Select-Object -Last 40 | Sort-Object)
    if ($hist.Count -ge 5) { $bootP99 = $hist[[Math]::Min($hist.Count - 1, [Math]::Ceiling(0.99 * $hist.Count) - 1)] }
}
$flags = 0
foreach ($t in $runs) {
    $name = $t.Directory.Name
    $lines = @(Get-Content -LiteralPath $t.FullName -ErrorAction SilentlyContinue | Where-Object { $_ -match '\S' })
    $steps = @($lines | Where-Object { $_ -match '^\d\d:\d\d:\d\d ' })
    # (flleg prints its RESULT line after Stop-Transcript: an ended run shows the transcript footer and its "space -> <end>")
    $ended = [bool]($lines | Where-Object { $_ -eq 'PowerShell transcript end' })
    $result = @($steps | Where-Object { $_ -match ' (space|base) -> ' }) | Select-Object -Last 1
    $quiet = ($now - $t.LastWriteTime).TotalSeconds
    # (a long cruise writes few transcript lines: the instance's live FLController.log counts as a sign of life too)
    $liveQuiet = $null
    if ($name -match '_i(\w+)$') {
        $live = Join-Path $root "Install\instances\$($Matches[1])\Freelancer HD Edition\EXE\FLController.log"
        if (Test-Path -LiteralPath $live) { $liveQuiet = ($now - (Get-Item -LiteralPath $live).LastWriteTime).TotalSeconds; $quiet = [Math]::Min($quiet, $liveQuiet) }
    }
    $booting = -not $steps.Count -and -not ($lines | Where-Object { $_ -eq 'PowerShell transcript end' })
    # (case-sensitive: "stalled" in the harness's own timeout text is no STALL)
    $notes = @($steps | Where-Object { $_ -cmatch 'SAVE-FIT|SAVE \S+ |HARNESS|STALL|unfit|FAILED|player-death' } | Select-Object -Last 6)
    $flag = if ($ended) { if ("$result" -match '-> (SUCCESS|until)\b') { 'done' } else { 'RESULT' } }
            elseif ($quiet -gt 90) { 'SILENT' }
            elseif ($notes | Where-Object { $_ -cmatch 'HARNESS|STALL|unfit|FAILED' }) { 'LOOK' } else { 'ok' }
    if (-not $booting -and $flag -in 'SILENT', 'RESULT', 'LOOK') { $flags++ }
    $age = ($now - $t.CreationTime).TotalSeconds
    if ($booting) {
        # (the game log is written from the DLL's start: growing = the game is loading, not hung)
        $prog = $liveQuiet -ne $null -and $liveQuiet -le 30
        $p99 = if ($bootP99 -ne $null) { '{0:N0}' -f $bootP99 } else { '?' }
        $flag = if ($bootP99 -eq $null -or $age -le $bootP99) { 'booting' } elseif ($prog) { 'BOOT-SLOW' } else { 'BOOT-STUCK' }
        if ($flag -eq 'BOOT-STUCK') { $flags++ }
        "{0} [{1}] boot {2:N0} s (p99 {3} s){4}" -f $name, $flag, $age, $p99,
            $(if ($flag -eq 'booting') { '' } elseif ($prog) { ': slow, progressing (game log written {0:N0} s ago)' -f $liveQuiet } else { ': no progress (game log ' + $(if ($liveQuiet -ne $null) { '{0:N0} s quiet' -f $liveQuiet } else { 'missing' }) + ')' })
        continue
    }
    "{0} [{1}] started {2:N0} s ago, last line {3:N0} s ago" -f $name, $flag, $age, $quiet
    if ($steps.Count) { "  last: $($steps[-1])" }
    foreach ($n in $notes) { "  note: $n" }
    if ($ended -and $result) { "  end: $result" }
}
$stamp = Join-Path (Split-Path $root -Parent) '.claude\launchcheck_last.txt'
Set-Content -LiteralPath $stamp -Value ("{0} runs={1} flagged={2}" -f $now.ToUniversalTime().ToString('o'), $runs.Count, $flags)
