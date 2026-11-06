# The HUD's module rows in game (docs\MODULES_PLAN.md P9): Controller without the HUD, Subtitles without it, the HUD without the Controller. Background games,
# frames in research\experience\runs\hud_modules. Prints what it read; exit 1 on a failure.
# Usage (PowerShell tool): tests\hud_modules.ps1 [-Row pad,subs,hud] [-Instance 1]    build\release
param([string[]]$Row = @('pad', 'subs', 'hud'), [int]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$deploy = Join-Path $root 'tools\deploy.ps1'
$dir = Join-Path $root 'research\experience\runs\hud_modules'
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Stop-Game { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 20 -ErrorAction SilentlyContinue }
function Wait-Sec([double]$s) { Start-Sleep -Milliseconds ([int]($s * 1000)) }
function Head([string]$s, [int]$n = 150) { "$s".Substring(0, [Math]::Min($n, "$s".Length)) }
function No-Fault([string]$log) { Check ((Get-Content (Join-Path (Get-FLInstallDir) "EXE\$log") -Raw) -notmatch 'FATAL') "no fault in $log" }

try {
    if ('pad' -in $Row) {
        '--- the controller without the HUD: the classic HUD, the pad prompts'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=0' -Save SaveC701.fl | Select-Object -Last 1
        Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30
        Check ("$(Send-FL 'hud2 state')" -notmatch 'style') "hud2: no verb ($(Head (Send-FL 'hud2 state') 60))"
        $st = Send-FL 'state'
        "  $(Head $st 110)"
        Save-FLShot -Name 'pad_docked' -Dir $dir
        $prompt = Send-FL 'prompttrace'
        "  prompts: $(Head $prompt 200)"
        Check ((Send-FL 'ping') -eq 'pong') 'the game answers'
        No-Fault 'FLController.log'
        Stop-Game
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,hud=0' -Save SaveC702.fl | Select-Object -Last 1
        Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30
        Check ("$(Send-FL 'padhud')" -match 'active=1') 'the pad HUD layer is up'
        "  padhud: $(Head (Send-FL 'padhud') 200)"
        Save-FLShot -Name 'pad_flight' -Dir $dir
        No-Fault 'FLController.log'
        Stop-Game
    }
    if ('subs' -in $Row) {
        '--- the subtitles without the HUD: the classic HUD, keep-outs through hud_bus'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles' -Save SaveC702.fl | Select-Object -Last 1
        Wait-Sec 3
        Check ((Send-FLS 'ping') -eq 'pong') 'the subtitles answer'
        $hooks = @((Send-FLS 'hooks') -split "`n" | ForEach-Object { ($_ -split ' ')[0] })
        Check (($hooks -notcontains 'HandleKey') -and ($hooks -notcontains 'UiControlAdd')) 'no HUD or pad hooks'
        Save-FLShot -Name 'subs_flight' -Dir $dir
        No-Fault 'FLSubtitles.log'
        Stop-Game
    }
    if ('hud' -in $Row) {
        '--- the HUD without the controller: HUD 2 on keyboard and mouse'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,hud=1' -Save SaveC702.fl | Select-Object -Last 1
        Wait-Sec 3
        Check ("$(Send-FLS 'mode')" -eq 'space') "mode: $(Send-FLS 'mode')"
        $h = Send-FLS 'hud2 state'
        "  $(Head $h 220)"
        Check ("$h" -match 'on=1|active=1|shown=1|style=2') 'HUD 2 runs'
        $hooks = @((Send-FLS 'hooks') -split "`n" | ForEach-Object { ($_ -split ' ')[0] })
        Check (($hooks -contains 'HandleKey') -and ($hooks -contains 'UiControlAdd')) 'its shared hooks'
        Save-FLShot -Name 'hud_flight' -Dir $dir
        "  uiskin: $(Head (Send-FLS 'uiskin') 160)"
        "  cue: $(Head (Send-FLS 'cue') 120)"
        Check ((Send-FLS 'ping') -eq 'pong') 'the game answers'
        No-Fault 'FLSubtitles.log'
        Stop-Game
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,hud=1' -Save SaveC701.fl | Select-Object -Last 1
        Wait-Sec 3
        "  docked uiskin: $(Head (Send-FLS 'uiskin') 160)"
        Save-FLShot -Name 'hud_docked' -Dir $dir
        No-Fault 'FLSubtitles.log'
        Stop-Game
    }
} finally {
    Stop-Game
    & $deploy -Instance $Instance -NoBuild -NoStart -Modules subtitles,controller | Out-Null
}
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'hud modules: PASS'
