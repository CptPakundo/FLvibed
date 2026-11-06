# The game's target keys under HUD 2 (UI_HUD2_INPUTS.md 4 row 3): USER_NEXT_TARGET / PREV_TARGET and NEXT_ENEMY walk the contact list, whose rows the
# game builds in the list's draw, which stops with the hidden root. One background game a row: hud2 with the pad's RS click, kbm (HUD 2, the controller
# off), classic as the control. Judged: the keys step through distinct objects, the same on each. -Range: a target left past 10 km, the panel's drop rule.
# Usage (PowerShell tool): tests\target_keys.ps1 [-Row hud2,kbm,classic] [-Range] [-Instance 1]   build\release. Exit 1 = a failure.
param([string[]]$Row = @('hud2', 'kbm', 'classic'), [switch]$Range, [int]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
$script:kbm = $false
function Say([string]$c) { if ($script:kbm) { Send-FLS "core $c" } else { Send-FL $c } } # (no controller: core's verbs on the subtitles' channel)
function Pass([int]$n) {
    if (-not $script:kbm) { $null = Wait-FLFramesPassed $n; return }
    $count = { [int](((Send-FLS 'state') -split "`n" | Where-Object { $_ -match '^frames' }) -replace '\D', '') }
    $f = & $count; $sw = [Diagnostics.Stopwatch]::StartNew()
    while ((& $count) -lt $f + $n -and $sw.Elapsed.TotalSeconds -lt 15) { Start-Sleep -Milliseconds 20 }
}
function Target { [regex]::Match((Say 'pilot'), 'target=(\d+)').Groups[1].Value }
function Walk([string]$cmd, [int]$n) {
    $ids = foreach ($i in 1..$n) { $null = Say "tap $cmd"; Pass 12; Target }
    "  ${cmd}: $($ids -join ' ')"
    @($ids | Where-Object { $_ -ne '0' } | Sort-Object -Unique).Count
}
foreach ($r in $Row) {
    "--- $r"
    $script:kbm = $r -eq 'kbm'
    $mods = if ($r -eq 'classic') { 'subtitles,controller,hud=0' } elseif ($script:kbm) { 'subtitles,hud=1,autopilot=1' } else { 'subtitles,controller' }
    & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Modules $mods -Save SaveC702.fl | Select-Object -Last 1
    try {
        Pass 400 # (the list's first build: 1.5 s)
        # (the keys walk from the current target: with none they do nothing, on the classic HUD too)
        "  $(Say 'spawn fc_lr_pi_fighter_d1 player 3 1500')"; Pass 300
        if ($r -eq 'hud2') {
            # the pad's RS click: the smart target, then the next enemy
            Clear-FLPad; Set-FLPad -LX 0.01; Pass 20; Clear-FLPad; Pass 20
            Press-FLPad @('RS'); Pass 150 # (a tap is delivered when the double tap's window closes)
            Check ((Target) -ne '0') "the pad's RS click finds a target ($(Target))"
        }
        $en = Walk 'USER_NEXT_ENEMY' 4; $en | Select-Object -First 1
        Check (@($en)[-1] -ge 1) "$r`: USER_NEXT_ENEMY finds a ship (it may stay on one, classic too)"
        $next = Walk 'USER_NEXT_TARGET' 6; $next | Select-Object -First 1
        Check (@($next)[-1] -ge 3) "$r`: USER_NEXT_TARGET steps through $(@($next)[-1]) objects"
        $prev = Walk 'USER_PREV_TARGET' 5; $prev | Select-Object -First 1
        Check (@($prev)[-1] -ge 3) "$r`: USER_PREV_TARGET steps through $(@($prev)[-1]) objects"
        $log = Join-Path (Get-FLInstallDir) $(if ($script:kbm) { 'EXE\FLSubtitles.log' } else { 'EXE\FLController.log' })
        Check (@(Select-String -Path $log -Pattern 'FATAL').Count -eq 0) "$r`: no fault in the log"
        if ($Range) {
            # a base left behind past 10 km: the target panel's drop rule, as a line to compare between the two HUDs
            $null = Send-FL 'spawn clear'
            $null = Set-FLTarget 'Fort Bush'; $null = Wait-FLFramesPassed 20
            $t = Target
            $here = [regex]::Match((Send-FL 'ship'), 'pos=\((-?\d+),(-?\d+),(-?\d+)\)')
            $null = Start-FLAutopilot -Mode goto -Position ([double]$here.Groups[1].Value + 60000), ([double]$here.Groups[2].Value), ([double]$here.Groups[3].Value)
            $sw = [Diagnostics.Stopwatch]::StartNew()
            while ($sw.Elapsed.TotalSeconds -lt 75 -and (Target) -eq $t) { Start-Sleep -Milliseconds 500 }
            "  $r target $t after $([int]$sw.Elapsed.TotalSeconds) s: $(Target), from $($here.Value) to $(([regex]::Match((Send-FL 'ship'), 'pos=\S+')).Value)"
        }
    } finally {
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
    }
}
if ($bad.Count) { "target keys: $($bad.Count) FAILED"; exit 1 }
'target keys: OK'
