# The maneuver watchdog protects every player, whichever modules are on (core\game\maneuver_watchdog.*). The verb `watchdog test` starts a real game goto and makes
# it read as stopped; the live part must then log one stall and re-issue the goto, free flight first. Rows: the autopilot off with the pad's entry, core's page
# host with no module at all, the HUD alone. A real stall is not reproduced here (docs\FREELANCER_RE.md "Formation"): the rule itself has tests\maneuver_watch_test.cpp.
# Usage (PowerShell tool): tests\watchdog_modules.ps1 [-Row pad,core,hud] [-Instance 1]    build\release. Exit 1 on a failure.
param([string[]]$Row = @('pad', 'core', 'hud'), [string]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$deploy = Join-Path $root 'tools\deploy.ps1'
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Stop-Game { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 20 -ErrorAction SilentlyContinue }
$rows = @{ pad = 'subtitles,controller,autopilot=0'; core = 'subtitles'; hud = 'subtitles,hud=1' }

try {
    foreach ($r in $Row) {
        "--- $r"
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules $rows[$r] -Save SaveC705.fl | Select-Object -Last 1
        $pad = $r -eq 'pad'
        $send = { param($c) if ($pad) { Send-FL $c } else { Send-FLS $c } }
        $log = Join-Path (Get-FLInstallDir) $(if ($pad) { 'EXE\FLController.log' } else { 'EXE\FLSubtitles.log' })
        Start-Sleep -Seconds 3
        $state = "$(& $send 'watchdog')"
        Check ($state -match 'watchdog on') "the row: $state"
        $go = "$(& $send 'watchdog test')"
        Check ($go -match 'accepted') "the goto: $go"
        $until = (Get-Date).AddSeconds(12)
        do { Start-Sleep -Milliseconds 500; $lines = @(Get-Content $log | Where-Object { $_ -match 'maneuver stalled|maneuver watchdog: re-issue' }) } while ($lines.Count -lt 2 -and (Get-Date) -lt $until)
        $null = & $send 'watchdog off'
        Check (@($lines | Where-Object { $_ -match 'maneuver stalled: op=goto' }).Count -ge 1) 'one stall logged'
        $again = @($lines | Where-Object { $_ -match 're-issue goto on \d+ try 1/3: accepted' })
        Check ($again.Count -ge 1) "re-issued: $(($again | Select-Object -First 1) -replace '^.*maneuver watchdog: ', '')"
        Stop-Game
    }
} finally { Stop-Game }
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'watchdog with the modules off: PASS'
