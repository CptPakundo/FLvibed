# GitHub issue 31: with weapon_groups 2 the triggers fire their groups under the autopilot as in free flight, and HUD 2 shows both. A virtual pad, a background game, the autopilot
# flying: the held commands (debug `owners`), the HUD's two glyphs, LT no longer the afterburner, and the display at hand-back. Exit 1 = a failure. -Root: a control run on another build.
# Usage (PowerShell tool): tests\ap_groups.ps1 [-Instance 5] [-Root <a checkout>]   build\release
param([int]$Instance = 5, [string]$Root = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $Root 'tools\harness\flctl.ps1')
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Owners { "$(Send-FL 'owners')" }
function Glyphs { [regex]::Matches("$(Send-FL 'hud2 state')", '(?m)^.*(fire|Fire).*$') | ForEach-Object { $_.Value } | Select-Object -First 3 }
& (Join-Path $Root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save SaveC702.fl | Select-Object -Last 1
try {
    "build: $(Get-Content (Join-Path $Root 'build\release\build_info.txt'))"
    $null = Wait-FLFramesPassed 120
    foreach ($c in 'set weapon_groups 2') { "  $(Send-FL $c)" }
    Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 20; Clear-FLPad; $null = Wait-FLFramesPassed 20
    "  $(Send-FL 'ap start auto')"
    $null = Wait-FLFramesPassed 60
    "  owners idle: $((Owners).Substring(0, [Math]::Min(120, (Owners).Length)))"
    Set-FLPad -LT 1; $null = Wait-FLFramesPassed 20
    $o = Owners
    "  LT held: $($o.Substring(0, [Math]::Min(200, $o.Length)))"
    Check ($o -match 'GROUP2|GROUP_2|WEAPON_GROUP2') 'LT under the autopilot holds group 2'
    Check ($o -notmatch 'AFTERBURN') 'and not the afterburner'
    Clear-FLPad; $null = Wait-FLFramesPassed 10
    Set-FLPad -RT 1; $null = Wait-FLFramesPassed 20
    $o = Owners
    Check ($o -match 'GROUP1|GROUP_1|WEAPON_GROUP1') 'RT under the autopilot holds group 1'
    Clear-FLPad; $null = Wait-FLFramesPassed 10
    $h = "$(Send-FL 'padhud')"
    "  padhud: $($h.Substring(0, [Math]::Min(240, $h.Length)))"
    Glyphs
    "  $(Send-FL 'ap stop')"
    $null = Wait-FLFramesPassed 30
    Set-FLPad -LT 1; $null = Wait-FLFramesPassed 20
    Check ((Owners) -match 'GROUP2|GROUP_2|WEAPON_GROUP2') 'handed back: LT still group 2'
    Clear-FLPad
} finally {
    Clear-FLPad
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
if ($bad.Count) { "ap groups: $($bad.Count) FAILED"; exit 1 }
'ap groups: OK'
