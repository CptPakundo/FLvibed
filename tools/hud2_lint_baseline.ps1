# The HUD 2 layout lint's live baseline (docs\UI_HUD2_PRESETS.md 6 item 3, IDEAS T8): one background game on an
# instance, a save in space, `hud2 lint` over every `hud2 sim` context, written as the approved dump (docs\hud2_lint_baseline.txt
# by default) or to -Out for a diff (-Compare prints the lines that differ from the approved file, per context).
# Each context is sampled after its fades settle (1.5 s, two round trips): sampled at once, a context still showed the
# previous one's plates fading out (31 Oct 2026: jump / lane read 13 plates 16.7 % mid-fade, 12 plates 15.2 % settled).
# The "live" context is a real moving frame: its pixel positions vary by a few px run to run.
# Usage: tools\hud2_lint_baseline.ps1 [-Instance 3] [-Out <file>] [-Compare] [-Preset 0]
param([string]$Instance = '3', [string]$Out = '', [switch]$Compare, [int]$Preset = 0,
      [string]$Save = '')
$ErrorActionPreference = 'Stop'
$fl = Split-Path $PSScriptRoot -Parent
$approved = Join-Path $fl 'docs\hud2_lint_baseline.txt'
if (-not $Out) { $Out = if ($Compare) { Join-Path $env:TEMP "hud2_lint_$Instance.txt" } else { $approved } }
# (a calm save: the free roam after the story, docked at Manhattan - launched first. SaveS415, M13's battleship fight,
# killed the idle ship within ~2 min, and every context after it read the dead HUD: 31 Oct 2026, Focus's lint)
if (-not $Save) { $Save = 'Save0e2b.fl' }
$contexts = 'live', 'combat', 'travel', 'cruise', 'dock', 'lane', 'ap', 'apwait', 'turret', 'formation', 'jump',
            'letterbox', 'loot', 'capital', 'notarget', 'offscreen', 'comm', 'lane+comm', 'ap+comm'
$raw = [System.Collections.Generic.List[string]]::new()
& (Join-Path $fl 'tools\deploy.ps1') -NoBuild -Background -Instance $Instance -Save $Save *> $null
$env:FLVIBED_INSTANCE = $Instance
. (Join-Path $fl 'controller\tools\flctl.ps1')
if ((Send-FL 'pilot') -match 'not in space') { $null = Start-FLLaunch -TimeoutSec 90 }
$null = Wait-FLSpace -TimeoutSec 90
try {
    $null = Send-FL 'hud2 on'
    $null = Send-FL "set hud_preset $Preset"
    foreach ($ctx in $contexts) {
        if ($ctx -ne 'live') { $null = Send-FL "hud2 sim $ctx" }
        $null = Send-FL 'hud2 lint' # arms the recorder
        $null = Send-FL 'state'
        Start-Sleep -Milliseconds 1500 # the fades settle (see the header)
        $null = Send-FL 'state'
        $raw.Add("=== context $ctx")
        foreach ($line in ((Send-FL 'hud2 lint' 8000) -split "`r?`n")) {
            if ($line -match '^(hud2 lint|FAIL|WARN|n/a|verdict)') { $raw.Add(($line -replace ' frame_age_ms=\d+', '')) }
        }
    }
    $null = Send-FL 'hud2 sim off'
    $null = Send-FL 'set hud_preset 0'
}
finally {
    Get-Process freelancer -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "*instances\$Instance\*" } |
        Stop-Process -Force
}
$build = (Get-Content (Join-Path $fl 'build\dev\build_info.txt') -TotalCount 1)
$hdr = @("# HUD 2 layout lint - the live baseline (docs\UI_HUD2_PRESETS.md 6 item 3), by tools\hud2_lint_baseline.ps1",
         "# $(Get-Date -Format 'yyyy-MM-dd HH:mm'), dev $build, instance $Instance, $(Split-Path $Save -Leaf), hud_preset $Preset, K&M (a background game), 3840x2160 expected; every hud2 sim context, sampled settled",
         '# Diff a later run against this file (-Compare): a new FAIL / WARN is a regression or a decision; "live" varies by a few px.')
Set-Content $Out ($hdr + $raw)
$fails = ($raw | Where-Object { $_ -match '^FAIL' }).Count
"hud2 lint baseline: $($contexts.Count) contexts, $fails FAIL line(s) -> $Out"
if ($Compare) {
    $blocks = {
        param($lines) $ctx = ''; $h = @{}
        foreach ($l in $lines) {
            if ($l -match '^=== context (.+)$') { $ctx = $Matches[1]; $h[$ctx] = @() }
            elseif ($ctx -and $l -match '^(FAIL|WARN|verdict)') { $h[$ctx] += $l }
        }
        $h
    }
    $a = & $blocks (Get-Content $approved); $b = & $blocks $raw
    foreach ($k in $contexts) {
        $d = Compare-Object @($a[$k]) @($b[$k])
        if ($d) { "-- $k"; $d | ForEach-Object { "  $($_.SideIndicator) $($_.InputObject)" } }
    }
}
