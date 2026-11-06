# A Windows 7 tester's report on 0.3.0: "flight speed locked to 10 m/s, can still enter tradelanes". One background game a row, the same
# flight each time: the keyboard's throttle up (USER_INC_THROTTLE's key-down: a background game reads no DirectInput keys) held for 8 s,
# then the top speed, the throttle, the game's pilot state and frame timing. Rows: plain, the Windows 7 simulation, each with the controller off, and with a pad
# in use. Judged: full throttle reaches the ship's top speed (the Starflier's 80 m/s). Lines: runs\win7_speed.
# Usage (PowerShell tool): tests\win7_speed.ps1 [-Row plain,sim,sim-c0,sim-pad,plain-c0] [-Instance 1]   build\release. Exit 1 = a failure.
param([string[]]$Row = @('plain', 'sim', 'sim-pad', 'sim-c0', 'plain-c0'), [int]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$dir = Join-Path $root 'research\experience\runs\win7_speed'
New-Item -ItemType Directory -Force $dir | Out-Null
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
foreach ($r in $Row) {
    "--- $r"
    $off = $r -match 'c0'
    if ($r -match 'sim') { $env:FLVIBED_SIM_WIN7 = '1' } else { Remove-Item Env:FLVIBED_SIM_WIN7 -ErrorAction SilentlyContinue }
    # the controller off, the autopilot's row on: the debug verbs of core's frame answer on the subtitles' channel
    $mods = if ($r -match 'classic') { 'subtitles,controller,hud=0' } elseif ($off) { 'subtitles,hud=1,autopilot=1' } else { 'subtitles,controller' }
    & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Modules $mods -Save SaveC702.fl | Select-Object -Last 1
    $send = if ($off) { { param($c) Send-FLS "core $c" } } else { { param($c) Send-FL $c } }
    try {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt 30) { $s = & $send 'ship'; if ("$s" -match 'speed=') { break }; Start-Sleep -Milliseconds 200 }
        if ("$s" -notmatch 'speed=') { Check $false "$r`: no ship read: $s"; continue }
        if ($r -match 'pad') { Set-FLPad -LX 0.01; $null = Wait-FLFramesPassed 20; Clear-FLPad; $null = Wait-FLFramesPassed 20 }
        "  $(& $send 'fire USER_INC_THROTTLE 1')"
        $last = -1; $top = 0.0; $t = [Diagnostics.Stopwatch]::StartNew()
        while ($t.Elapsed.TotalSeconds -lt 8) {
            Start-Sleep -Milliseconds 250
            $s = & $send 'ship'
            $v = [double][regex]::Match($s, 'speed=(\S+)').Groups[1].Value
            $top = [Math]::Max($top, $v)
        }
        $null = & $send 'fire USER_INC_THROTTLE 0'
        $null = & $send 'fire USER_DEC_THROTTLE 1'
        Start-Sleep -Milliseconds 1500
        $null = & $send 'fire USER_DEC_THROTTLE 0'
        $down = [regex]::Match((& $send 'ship'), 'throttle=(\S+)').Groups[1].Value
        $thr = [regex]::Match($s, 'throttle=(\S+)').Groups[1].Value
        "  top $top m/s, throttle $thr; $(& $send 'timing')"
        "  $(& $send 'pilot')".Substring(0, 90)
        $log = Join-Path (Get-FLInstallDir) $(if ($off) { 'EXE\FLSubtitles.log' } else { 'EXE\FLController.log' })
        (Select-String -Path $log -Pattern 'win7 sim|XInput:|winmode: DPI|FATAL' | Select-Object -First 12 | ForEach-Object { '  log: ' + $_.Line.Substring(0, [Math]::Min(110, $_.Line.Length)) })
        $null = Save-FLShot -Name $r -Dir $dir
        Check ($top -gt 70) "$r`: full throttle reaches top speed ($top m/s)"
        Check ([double]$down -lt 0.05) "$r`: the throttle comes back down ($down)"
    } finally {
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
    }
}
Remove-Item Env:FLVIBED_SIM_WIN7 -ErrorAction SilentlyContinue
if ($bad.Count) { "win7 speed: $($bad.Count) FAILED"; exit 1 }
'win7 speed: OK'
