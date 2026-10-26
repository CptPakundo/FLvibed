# The offline decision check (HANDOFF P2a; the user, 23 Oct 2026: "no regressions elsewhere, or else it'll be a loop of
# improvements and regressions"): tools\resolver_check.py (the offline port of core\game\mission.cpp AdvanceGoals) sweeps
# every objective of every mission script and lists the script goals the objective mode would pick (subject, condition,
# the step it leads to). The result is compared with tools\decision_baseline.txt (versioned): any line that changed is a
# changed decision somewhere in the campaign - review it before trusting a change to mission.cpp / the objective mode.
# Seconds per run. Keep the port in step with mission.cpp (a change there that the port lacks shows here as no change).
# Usage: tools\decision_check.ps1            compare with the baseline (exit 1 on a difference)
#        tools\decision_check.ps1 -Update    write the current sweep as the new baseline (commit it with the change)
param([switch]$Update)
$root = Split-Path $PSScriptRoot -Parent
$py = Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe'
$missions = Join-Path $root 'Install\Freelancer HD Edition\DATA\MISSIONS'
$base = Join-Path $PSScriptRoot 'decision_baseline.txt'
$now = @(& $py -I (Join-Path $PSScriptRoot 'resolver_check.py') $missions 2>&1 | ForEach-Object { "$_" })
if ($Update -or -not (Test-Path $base)) {
    Set-Content -Path $base -Value $now -Encoding utf8
    "decision baseline written: $($now.Count) lines"
    return
}
$old = @(Get-Content $base -Encoding utf8)
$diff = Compare-Object $old $now
if (-not $diff) { "decisions unchanged ($($now.Count) lines)"; return }
"DECISIONS CHANGED ($(@($diff).Count) lines; <= baseline only, => now only):"
$diff | ForEach-Object { "$($_.SideIndicator) $($_.InputObject)" }
exit 1
