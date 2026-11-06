# NeuroNet windows with the controller off, and the Autopilot rows on the bus (the P11b audit). Row one: `controller = 0`, the autopilot flying, the goal panel
# open, the nav map by F6: the mode goes to space-ui, the panel closes, the autopilot flies on (only the pause holds it). Row two: all on: the options page's Autopilot tab has
# its values (163 rows against the bus's old cap of 160 greyed all of it). Exit 1 on a failure.
# Usage (PowerShell tool): tests\window_modules.ps1 [-Instance 1]    build\release
param([string]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$deploy = Join-Path $root 'tools\deploy.ps1'
if (-not ('FLKeyWin' -as [type])) { Add-Type -Name FLKeyWin -Namespace '' -MemberDefinition '[DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);' }
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Stop-Game { try { Clear-FLPad } catch { }; Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 20 -ErrorAction SilentlyContinue }
function ApTime { [double][regex]::Match("$(Send-FLS 'ap status')", ' time=([\d.]+)').Groups[1].Value }

try {
    '--- the controller off: a window under the autopilot'
    & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,hud=1,autopilot=1' -Save SaveC702.fl | Select-Object -Last 1
    Start-Sleep -Seconds 3
    Check ("$(Send-FLS 'mode')" -eq 'space') 'in flight'
    $null = Send-FLS 'ap start hold'
    Start-Sleep -Seconds 1
    Check ("$(Send-FLS 'goals open')" -match 'open=1') 'the goal panel is open'
    $game = (Get-FLGameProcess).MainWindowHandle
    [void][FLKeyWin]::PostMessage($game, 0x100, [IntPtr]0x75, [IntPtr]0x00400001) # F6, the nav map
    Start-Sleep -Milliseconds 120
    [void][FLKeyWin]::PostMessage($game, 0x101, [IntPtr]0x75, [IntPtr]0xC0400001)
    Start-Sleep -Seconds 2
    Check ("$(Send-FLS 'mode')" -eq 'space-ui') "the window is seen: mode $(Send-FLS 'mode')"
    Check ("$(Send-FLS 'goals')" -match 'open=0') 'the goal panel closed with the mode'
    $t0 = ApTime; Start-Sleep -Seconds 1; $t1 = ApTime
    Check ($t1 -gt $t0) "the autopilot flies on under the window: $t0 -> $t1"
    Stop-Game
    '--- all on: the Autopilot tab'
    & $deploy -Background -NoBuild -Release -Instance $Instance -Save SaveC702.fl | Select-Object -Last 1
    Start-Sleep -Seconds 2
    Check (@(Get-Content (Join-Path (Get-FLInstallDir) 'EXE\FLController.log') | Where-Object { $_ -match 'settings bus: .* Autopilot [1-9]' }).Count -eq 1) 'the Autopilot section is on the bus'
    $null = Send-FL 'ui options open'
    $null = Send-FL 'ui tab 4'; $null = Send-FL 'ui focus show_advanced'; $null = Send-FL 'ui press a' # (the style's rows stand behind Show all settings)
    $null = Send-FL 'ui tab 3'
    $rows = "$(Send-FL 'ui rows')"
    Check ($rows -match 'AUTOPILOT' -and $rows -match 'preset = \((?!CUSTOM)' -and $rows -match 'knowledge = \d') 'its rows have their values, the preset is named'
    $null = Send-FL 'ui options close'
} finally { Stop-Game }
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'window modules: PASS'
