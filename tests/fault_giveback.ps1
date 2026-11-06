# A fault gives the game back (coreframe::OnFault): the verb `fault` breaks the logic on purpose in a background game; the frames and the log say what the player
# keeps. Rows: `frame` and `draw`, the controller's host, with the stick held over the fault; `alone`, core's page host (controller off, HUD and autopilot on).
# Judged: one FATAL and one give-back in the log; the subtitles still answer; the game's own panels on screen where HUD 2 had none; the ship no longer turning
# (two frames a second apart, the backdrop still); a device Reset still passes after the fault. Frames: research\experience\runs\fault_giveback.
# A fault in one module stops that module alone (modslots::Guard): rows `pad-frame` and `pad-draw` break the NPC, Speedrun, Autopilot and HUD modules one after the
# other in one game, `alone-frame` the same on core's page host. Judged after each: one FATAL naming the module and no entry's fault; its give-back; its verbs gone,
# the next modules' still there; the pad still steers the ship, the options page still opens; the HUD's fault brings the game's status bar back and a device Reset passes.
# Usage (PowerShell tool): tests\fault_giveback.ps1 [-Row frame,draw,alone,pad-frame,pad-draw,alone-frame] [-Instance 1]    build\release. Exit 1 on a failure.
param([string[]]$Row = @('frame', 'draw', 'alone', 'pad-frame', 'pad-draw', 'alone-frame'), [int]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$deploy = Join-Path $root 'tools\deploy.ps1'
$dir = Join-Path $root 'research\experience\runs\fault_giveback'
New-Item -ItemType Directory -Force $dir | Out-Null
Add-Type -AssemblyName System.Drawing
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Stop-Game { try { Clear-FLPad } catch { }; Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 20 -ErrorAction SilentlyContinue }
# The share of bright pixels in the screen's bottom band, centre: the game's status bar lives there, HUD 2 draws nothing there in flight.
function Band([string]$png) {
    $b = [Drawing.Bitmap]::FromFile($png)
    try {
        $n = 0; $hit = 0
        for ($y = [int]($b.Height * 0.955); $y -lt [int]($b.Height * 0.995); $y += 4) {
            for ($x = [int]($b.Width * 0.44); $x -lt [int]($b.Width * 0.57); $x += 4) {
                $c = $b.GetPixel($x, $y); $n++
                if ($c.R + $c.G + $c.B -gt 240) { $hit++ }
            }
        }
        $hit / [double]$n
    } finally { $b.Dispose() }
}
# Mean difference of two frames over the top third of the screen's centre: the backdrop, away from the HUD and the ships near the player.
function Drift([string]$a, [string]$c) {
    $p = [Drawing.Bitmap]::FromFile($a); $q = [Drawing.Bitmap]::FromFile($c)
    try {
        $sum = 0.0; $n = 0
        for ($y = [int]($p.Height * 0.12); $y -lt [int]($p.Height * 0.45); $y += 8) {
            for ($x = [int]($p.Width * 0.30); $x -lt [int]($p.Width * 0.75); $x += 8) {
                $u = $p.GetPixel($x, $y); $v = $q.GetPixel($x, $y)
                $sum += [Math]::Abs($u.R - $v.R) + [Math]::Abs($u.G - $v.G) + [Math]::Abs($u.B - $v.B); $n++
            }
        }
        $sum / $n
    } finally { $p.Dispose(); $q.Dispose() }
}
function Shot([string]$name) { "$(Save-FLShot -Name $name -Dir $dir)" | Select-Object -Last 1 }

# One game, its modules broken one after the other. `alone`: core's page host, whose debug verbs go through `core`.
function Test-Modules([string]$name) {
    $alone = $name -like 'alone*'
    $where = ($name -split '-')[1]
    $mods = if ($alone) { 'subtitles,hud=1,autopilot=1,speedrun=1,npc=1' } else { 'subtitles,controller' }
    & $deploy -Background -NoBuild -Release -Instance $Instance -Modules $mods -Save SaveC702.fl | Select-Object -Last 1
    $log = Join-Path (Get-FLInstallDir) $(if ($alone) { 'EXE\FLSubtitles.log' } else { 'EXE\FLController.log' })
    $send = { param($c) if ($alone) { Send-FLS "core $c" } else { Send-FL $c } }
    $direct = { param($c) if ($alone) { Send-FLS $c } else { Send-FL $c } } # (the verbs with their own place on the subtitles' channel)
    $verbs = [ordered]@{ npc = 'stalls'; speedrun = 'timer'; autopilot = 'ap status'; hud = 'hud2 state' }
    $unknown = "$(& $send 'flvibed_no_such_verb')" -replace 'flvibed_no_such_verb', 'X'
    function Gone([string]$v) { $r = if ($v -like 'ap *') { "$(& $direct $v)" } else { "$(& $send $v)" }; ($r -replace [regex]::Escape(($v -split ' ')[0]), 'X') -eq $unknown -or $r -match 'module is off' }
    if (-not $alone) { Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad }
    Start-Sleep -Seconds 3
    $b0 = Band (Shot "${name}_before")
    $healthy = if ($alone) { '' } else { "$(Send-FL 'd3dreset')" } # a background game's Reset answers the same codes healthy or not: the check is that they stay
    Start-Sleep -Milliseconds 800
    $order = @('npc', 'speedrun', 'autopilot', 'hud') | Where-Object { $where -eq 'frame' -or $_ -ne 'npc' }
    foreach ($m in $order) {
        "  -- $m"
        if ($m -eq 'autopilot') {
            $null = & $direct 'ap start hold'
            Start-Sleep -Seconds 1
        }
        $armed = "$(& $direct "fault $m $where")"
        Check ($armed -match 'fault armed') "the verb: $armed"
        $until = (Get-Date).AddSeconds(6)
        do { Start-Sleep -Milliseconds 200; $st = "$(& $direct 'fault status')" } while ($st -notmatch "\b$m\b" -and (Get-Date) -lt $until)
        Check ($st -match "\b$m\b") "stopped: $st"
        Start-Sleep -Milliseconds 400
        $lines = Get-Content $log
        Check (@($lines | Where-Object { $_ -match "FATAL.*in the $m module" }).Count -eq 1) 'one fault in the log, naming the module'
        Check (@($lines | Where-Object { $_ -match 'FATAL' -and $_ -notmatch ' module: it is stopped' }).Count -eq 0) "no entry's fault"
        Check (@($lines | Where-Object { $_ -match "the $m module's changes to the game given back" }).Count -eq 1) 'its give-back'
        Check (Gone $verbs[$m]) "its verb is gone: $($verbs[$m])"
        foreach ($k in $order[($order.IndexOf($m) + 1)..($order.Count)] | Where-Object { $_ }) { Check (-not (Gone $verbs[$k])) "$k plays on" }
        Check ((Send-FLS 'ping') -eq 'pong') 'the subtitles answer'
        if (-not $alone) {
            Set-FLPad -RX 1; $null = Wait-FLFramesPassed 30
            $turn = "$(Send-FL 'ship')"; Clear-FLPad
            $yaw = [Math]::Abs([double][regex]::Match($turn, 'y\(yaw\)=(-?[\d.]+)').Groups[1].Value)
            Check ($yaw -gt 3) "the pad steers the ship: yaw $yaw deg/s"
            $null = Wait-FLFramesPassed 30
        }
        $open = "$(& $direct 'ui options open')"; Start-Sleep -Milliseconds 300
        $close = "$(& $direct 'ui options close')"; Start-Sleep -Milliseconds 300
        Check ($open -match 'open' -and $close -match 'closed') "the page opens and closes"
        if ($m -eq 'hud') {
            Start-Sleep -Seconds 1
            $b1 = Band (Shot "${name}_hud_after")
            Check ($b1 -gt 0.3 -and $b1 -gt $b0 * 3) ("the game's status bar is back: {0:P0} of the band, {1:P0} before" -f $b1, $b0)
            if (-not $alone) {
                $reset = "$(Send-FL 'd3dreset')"
                Check ($reset -eq $healthy) "a device Reset answers as a healthy game's: $($reset.Substring(0, [Math]::Min(100, $reset.Length)))"
                Start-Sleep -Milliseconds 800
                Check ((Send-FLS 'ping') -eq 'pong') 'a device Reset after the fault: the game lives'
            }
        }
    }
    Stop-Game
}

try {
    foreach ($r in $Row) {
        "--- $r"
        if ($r -like '*-*') { Test-Modules $r; continue }
        $alone = $r -eq 'alone'
        $mods = if ($alone) { 'subtitles,hud=1,autopilot=1' } else { 'subtitles,controller' }
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules $mods -Save SaveC702.fl | Select-Object -Last 1
        $log = Join-Path (Get-FLInstallDir) $(if ($alone) { 'EXE\FLSubtitles.log' } else { 'EXE\FLController.log' })
        $send = { param($c) if ($alone) { Send-FLS $c } else { Send-FL $c } }
        if (-not $alone) { Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad }
        Start-Sleep -Seconds 3
        $before = Shot "${r}_before"
        if (-not $alone) { Set-FLPad -LX 1 -RX 1; $null = Wait-FLFramesPassed 30 }
        $armed = & $send "fault $(if ($r -eq 'draw') { 'draw' } else { 'frame' })"
        Check ("$armed" -match 'armed') "the verb: $armed"
        Start-Sleep -Milliseconds 700
        if (-not $alone) { Clear-FLPad }
        Start-Sleep -Milliseconds 800
        $after = Shot "${r}_after"
        Start-Sleep -Seconds 1
        $after2 = Shot "${r}_after2"
        $lines = Get-Content $log
        Check (@($lines | Where-Object { $_ -match 'FATAL' }).Count -eq 1) 'one fault in the log'
        Check (@($lines | Where-Object { $_ -match 'given back' }).Count -eq 1) 'one give-back in the log'
        Check (@($lines | Where-Object { $_ -match 'give-back faulted' }).Count -eq 0) 'the give-back did not fault'
        Check ((Send-FLS 'ping') -eq 'pong') 'the subtitles answer'
        $b0 = Band $before; $b1 = Band $after
        Check ($b1 -gt 0.3 -and $b1 -gt $b0 * 3) ("the game's status bar is back: {0:P0} of the band, {1:P0} before" -f $b1, $b0)
        $d = Drift $after $after2
        Check ($d -lt 12) ("the ship is not turning: backdrop drift {0:N1}" -f $d)
        if (-not $alone) {
            $reset = "$(Send-FL 'd3dreset')"
            "  d3dreset: $($reset.Substring(0, [Math]::Min(120, $reset.Length)))"
            Start-Sleep -Milliseconds 800
            Check ((Send-FLS 'ping') -eq 'pong') 'a device Reset after the fault: the game lives'
        }
        Stop-Game
    }
} finally { Stop-Game }
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'fault give-back: PASS'
