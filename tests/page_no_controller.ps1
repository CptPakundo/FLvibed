# The options page with the controller off (docs\MODULES_PLAN.md P4): a background game with the subtitles alone, the page driven
# through the subtitles' channel (`ui ...`: the controller's channel does not exist then). Prints what it read; exit 1 on a failure.
# Usage (PowerShell tool): tests\page_no_controller.ps1 [-Instance 1]   uses build\release
param([string]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$env:FLVIBED_INSTANCE = "$Instance"
$dir = Join-Path $root 'research\experience\runs\page_no_controller'
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Frames([int]$n) { $f = [int](((Send-FLS 'state') -split "`n" | Where-Object { $_ -match '^frames' }) -replace '\D', ''); $until = (Get-Date).AddSeconds(20); do { $g = [int](((Send-FLS 'state') -split "`n" | Where-Object { $_ -match '^frames' }) -replace '\D', '') } while ($g -lt $f + $n -and (Get-Date) -lt $until) }
& (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Modules subtitles -Save SaveC702.fl | Select-Object -Last 1
try {
    Frames 60
    $open = Send-FLS 'ui options open'
    Frames 30
    Check ("$open" -match 'open') "opened: $open"
    $tabs = Send-FLS 'ui tab'
    Check ("$tabs" -match 'tabs:') "tabs: $tabs"
    $seen = @{}
    foreach ($t in 0, 2, 4) {
        $null = Send-FLS "ui tab $t"
        Frames 15
        $rows = @((Send-FLS 'ui rows') -split "`n" | Where-Object { $_.Trim() })
        $seen[$t] = $rows.Count
        "  tab $t`: $($rows.Count) rows; first: $(($rows | Select-Object -First 2) -join ' | ')".Substring(0, [Math]::Min(150, "  tab $t`: $($rows.Count) rows; first: $(($rows | Select-Object -First 2) -join ' | ')".Length))
        $f = Save-FLShot -Name "tab$t" -Dir $dir
    }
    Check ($seen[0] -gt 0) 'HUD has rows'
    Check ($seen[2] -gt 3) 'SUBTITLES has its rows'
    Check ($seen[4] -gt 0) 'MORE has rows (the small features switches)'
    $null = Send-FLS 'ui tab 2'; Frames 10
    $before = Send-FLS 'ui state'
    $null = Send-FLS 'ui press down'; Frames 10
    $after = Send-FLS 'ui state'
    Check ("$before" -ne "$after") "input reaches the page: $("$after".Substring(0, [Math]::Min(90, "$after".Length)))"
    $close = Send-FLS 'ui options close'; Frames 20
    Check ("$close" -match 'closed') "closed: $close"
    Check ((Send-FLS 'ping') -eq 'pong') 'the subtitles still answer'
    $hooks = (Send-FLS 'hooks') -split "`n" | Where-Object { $_ -match '^(WinKey|UiRender|HitTest|PrefsInitElements|PrefsMessage|PauseInitElements|PauseMessage|IsCmdHeld|MouseSteer|ManeuverFrameSize)\b' } | ForEach-Object { ($_ -split ' ')[0] }
    "hooks of the page: $($hooks -join ' ')"
    Check (($hooks -contains 'PrefsMessage') -and ($hooks -contains 'WinKey') -and ($hooks -notcontains 'IsCmdHeld') -and ($hooks -notcontains 'ManeuverFrameSize')) 'the page''s hooks in, the pad''s and the bar''s out'
} finally {
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
}
Select-String -Path (Join-Path (Get-FLInstallDir) 'EXE\FLSubtitles.log') -Pattern 'FATAL|core logic|page:' | Select-Object -First 5 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(130, $_.Line.Length)) }
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'page without the controller: PASS'
