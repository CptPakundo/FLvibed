# nets.ps1 - run a lane's test nets one after another in ONE command, waiting for a game slot INSIDE the script.
# Why (the owner, 13 Nov 2026: less time and fewer tokens): an agent that polls for a free game slot or for a running
# test re-reads its whole context at every poll. Start this once in the background and wait for its one notification.
#   pwsh -File <FLvibed>\tools\nets.ps1 -Lane <lane folder> -Run "tests\nn_pages.ps1 -Instance 6 ;; tests\base_pages.ps1 -Mode check -Instance 6 -Pages repair,prices"
# A slot = one of four lock files held for the whole run; games of lanes that hold no slot (started the old way) count too.
# The lock files live in the repository's shared git folder (<FLvibed>\.git\flvibed_slots), which every lane (a git worktree) shares,
# so a script run from any lane sees the same slots. Moved here from audit_2026-11-12\tools\ on 14 Nov 2026; its test: nets_selftest.ps1.
# Output: <Lane>\scratch\nets_<stamp>.log (start / end / exit code / the last lines of each net) and <log>.full (everything).
# Exit: 0 = every net ran and exited 0; 1 = a net exited non-zero or its script is missing (the log says which).
# -SlotDir, -PollSec and -NoLooseCheck exist for nets_selftest.ps1 (a test must not wait on the machine's real games).
param([Parameter(Mandatory)][string]$Lane, [Parameter(Mandatory)][string]$Run, [string]$Out, [int]$MaxGames = 4, [int]$Tail = 8,
      [string]$SlotDir, [int]$PollSec = 20, [switch]$NoLooseCheck)
$ErrorActionPreference = 'Stop'
if (-not $SlotDir) {
    $common = & git -C $PSScriptRoot rev-parse --path-format=absolute --git-common-dir 2>$null
    $SlotDir = if ($LASTEXITCODE -eq 0 -and $common) { Join-Path $common 'flvibed_slots' } else { Join-Path ([IO.Path]::GetTempPath()) 'flvibed_slots' }
}
$slotDir = $SlotDir; New-Item -ItemType Directory -Force $slotDir | Out-Null
$Lane = (Resolve-Path -LiteralPath $Lane).Path.TrimEnd('\')
if (-not $Out) { $Out = Join-Path $Lane ("scratch\nets_{0}.log" -f (Get-Date -Format 'yyyyMMdd_HHmmss')) }
New-Item -ItemType Directory -Force (Split-Path $Out) | Out-Null
function Say($s) { $line = "[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $s; Add-Content -LiteralPath $Out -Value $line; Write-Output $line }
function HeldLanes {   # one entry per slot another run holds: its lane folder, or '?' when it cannot be read
    foreach ($i in 1..$MaxGames) {
        $p = Join-Path $slotDir "slot$i.lock"; if (-not (Test-Path -LiteralPath $p)) { continue }
        try { $h = [IO.File]::Open($p, 'Open', 'ReadWrite', 'Read'); $h.Close() }
        catch { try { $r = [IO.File]::Open($p, 'Open', 'Read', 'ReadWrite'); $sr = New-Object IO.StreamReader($r); $t = $sr.ReadToEnd(); $sr.Close(); if ($t.Trim()) { $t.Trim() } else { '?' } } catch { '?' } }
    }
}
# (the mutex name follows the slot folder, so a test with its own folder never queues behind the real one)
$mutexName = 'Global\FLvibedGameSlot' + $(if ($PSBoundParameters.ContainsKey('SlotDir')) { '_' + ($slotDir -replace '[^A-Za-z0-9]', '_') } else { '' })
$mutex = New-Object Threading.Mutex($false, $mutexName)
$slot = $null; $waited = 0
while (-not $slot) {
    try { [void]$mutex.WaitOne() } catch [Threading.AbandonedMutexException] { }
    try {
        $held = @(HeldLanes)
        $games = if ($NoLooseCheck) { @() } else { @(Get-Process Freelancer -ErrorAction SilentlyContinue | ForEach-Object { $_.Path } | Where-Object { $_ }) }
        $loose = @($games | Where-Object { $g = $_; -not ($held | Where-Object { $_ -ne '?' -and $g.StartsWith("$_\", [StringComparison]::OrdinalIgnoreCase) }) }).Count
        if ($held.Count + $loose -lt $MaxGames) {
            foreach ($i in 1..$MaxGames) { $p = Join-Path $slotDir "slot$i.lock"; try { $slot = [IO.File]::Open($p, 'OpenOrCreate', 'ReadWrite', 'Read'); break } catch { } }
            if ($slot) { $slot.SetLength(0); $b = [Text.Encoding]::UTF8.GetBytes($Lane); $slot.Write($b, 0, $b.Length); $slot.Flush() }
        }
    } finally { $mutex.ReleaseMutex() }
    if (-not $slot) { if ($waited % 300 -eq 0) { Say "waiting for a game slot ($($held.Count) slots held, $loose other games)" }; Start-Sleep $PollSec; $waited += $PollSec }
}
$ran = 0; $bad = 0
try {
    Say "slot taken after $waited s; lane $Lane"
    foreach ($c in ($Run -split '\s+;;\s+')) {
        $c = $c.Trim(); if (-not $c) { continue }
        $script, $rest = $c -split '\s+', 2
        $full = Join-Path $Lane $script
        if (-not (Test-Path -LiteralPath $full)) { Say "MISSING $full"; $bad++; continue }
        Say "START $c"; $t0 = Get-Date
        # (the child's exit code is passed on as it is: -Command alone would turn every non-zero exit into 1; a stale $LASTEXITCODE of a native call inside a script that ends well is ignored through $?)
        $o = & pwsh -NoProfile -Command "& '$full' $rest; if (`$?) { exit 0 } else { exit `$(if (`$LASTEXITCODE) { `$LASTEXITCODE } else { 1 }) }" *>&1 | Out-String -Stream
        $code = $LASTEXITCODE; $ran++; if ($code -ne 0) { $bad++ }
        Add-Content -LiteralPath ($Out + '.full') -Value (@("===== $c") + $o)
        Say ("END   {0}  exit={1}  {2:N1} min" -f $c, $code, ((Get-Date) - $t0).TotalMinutes)
        $o | Where-Object { $_ -and $_.Trim() } | Select-Object -Last $Tail | ForEach-Object { Add-Content -LiteralPath $Out -Value ("      " + $_); Write-Output ("      " + $_) }
    }
} finally { $slot.Close() }
Say "NETS DONE: $ran ran, $bad with a non-zero exit or missing; log $Out"
if ($bad) { exit 1 }
