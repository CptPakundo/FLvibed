# Speedrun without the Controller (core\game\session.*, modules\speedrun\src\startprompt.*). Row `start`: `subtitles = 1, controller = 0, speedrun = 1` at the main menu:
# NEW GAME is the module's, the list offers NORMAL GAME and SPEEDRUN MODE, the pick starts the game's own New Game, the session reads speedrun and the clock runs.
# Row `split`: a real checkpoint on keyboard and mouse, with the Autopilot to get there: a clock started, a real dock and launch, the detector's `control` split from
# the frame's mode. Row `pad`: with the Controller on nothing of this runs. Frames: research\experience\runs\speedrun_modules.
# The clicks are the game's own virtual mouse (`mouse`, `mbtn`): NEW GAME's click opens the list and starts nothing; Escape closes it and no Exit question comes;
# a click on SPEEDRUN MODE starts the game's New Game.
# Usage (PowerShell tool): tests\speedrun_modules.ps1 [-Row start,split,pad] [-Instance 1]    build\release. Exit 1 on a failure.
param([string[]]$Row = @('start', 'split', 'pad'), [int]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$deploy = Join-Path $root 'tools\deploy.ps1'
$dir = Join-Path $root 'research\experience\runs\speedrun_modules'
New-Item -ItemType Directory -Force $dir | Out-Null
$bad = [Collections.Generic.List[string]]::new()
function Check($ok, [string]$what) { $ok = [bool]@($ok | Where-Object { $_ }).Count; "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Stop-Game { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 20 -ErrorAction SilentlyContinue }
$padSend = ${function:Send-FL}
$script:alone = $true
# the harness drives through Send-FL: with the controller off the same verbs go through `core` on the subtitles' channel
function Send-FL([string]$Command, [int]$TimeoutMs = 5000) {
    if (-not $script:alone) { return & $padSend $Command $TimeoutMs }
    if ($Command -eq 'state') { return "mode=$(Send-FLS 'mode' $TimeoutMs)" } # (`state` is the pad's verb)
    Send-FLS "core $Command" $TimeoutMs
}

try {
    foreach ($r in $Row) {
        "--- $r"
        $script:alone = $r -ne 'pad'
        if ($r -eq 'start') {
            & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,speedrun=1' | Select-Object -Last 1
            $null = Wait-FLUntil { "$(Send-FL 'uiitems')" -match '(?m)^FrontButton' } 'the main menu' 60
            $st = "$(Send-FL 'startprompt')"
            $until = (Get-Date).AddSeconds(10)
            while ($st -notmatch 'taken=1' -and (Get-Date) -lt $until) { Start-Sleep -Milliseconds 300; $st = "$(Send-FL 'startprompt')" }
            Check ($st -match 'wanted=1' -and $st -match 'taken=1') "NEW GAME is the module's: $($st.Substring(0, [Math]::Min(60, $st.Length)))"
            function Click([string]$xy) {
                $x, $y = $xy -split ','
                $null = Send-FL "mouse $x $y"; Start-Sleep -Milliseconds 400
                $null = Send-FL 'mbtn 0 1'; Start-Sleep -Milliseconds 200
                $null = Send-FL 'mbtn 0 0'; Start-Sleep -Milliseconds 300
                $null = Send-FL 'mouse release'
            }
            # the menu's buttons slide in: the button at rest
            $button = ''; $was = 'x'; $until = (Get-Date).AddSeconds(15)
            while ((Get-Date) -lt $until -and (-not $button -or $button -ne $was)) {
                $was = $button; Start-Sleep -Milliseconds 400
                $button = [regex]::Match("$(Send-FL 'startprompt')", 'button=(\d+,\d+)').Groups[1].Value
            }
            Check $button "NEW GAME at $button"
            Click $button
            $st = "$(Send-FL 'startprompt')"
            Check ($st -match 'open=1') 'a real click on NEW GAME opens the list'
            Check ("$(Send-FLS 'mode')" -eq 'menu') 'and the game starts nothing'
            $null = Save-FLShot -Name 'list' -Dir $dir
            Check ("$(Send-FL 'startprompt pick 1')" -match 'not offered') 'ASSISTED is not offered without the autopilot'
            $null = Send-FL 'postkey 27 1'; Start-Sleep -Milliseconds 100; $null = Send-FL 'postkey 27 0'; Start-Sleep -Milliseconds 500
            Check ("$(Send-FL 'startprompt')" -match 'open=0') 'Escape closes the list'
            Check ("$(Send-FL 'visible ExitButtonBox0')" -notmatch '^visible') 'and the game asks no Exit question'
            Click $button
            $st = "$(Send-FL 'startprompt')"
            Start-Sleep -Milliseconds 300
            $st = "$(Send-FL 'startprompt')"
            $rows = [regex]::Match($st, 'rows=(\S+) (\S+) (\S+)').Groups
            Check ($rows[3].Value) "the rows: $($rows[1].Value) $($rows[3].Value)"
            Click $rows[3].Value # SPEEDRUN MODE: the third of NORMAL, ASSISTED, SPEEDRUN (ASSISTED's box unset here)
            $picked = "$(Send-FL 'startprompt')"
            Check ($picked -match 'open=0') "the click on SPEEDRUN MODE: $($picked.Substring(0, [Math]::Min(90, $picked.Length)))"
            $until = (Get-Date).AddSeconds(60)
            do { Start-Sleep -Milliseconds 500; $null = Send-FLS 'movieskip'; $m = "$(Send-FLS 'mode')" } while ($m -eq 'menu' -and (Get-Date) -lt $until)
            Check ($m -ne 'menu') "the game's New Game ran: mode $m"
            Start-Sleep -Seconds 2
            $st = "$(Send-FL 'startprompt')"
            Check ($st -match 'session=speedrun') 'the session reads speedrun'
            Check ($st -match 'taken=0') 'the menu has its hit test back'
            $t1 = [int][regex]::Match("$(Send-FL 'timer')", ' t=(\d+)ms').Groups[1].Value
            Start-Sleep -Seconds 2
            $timer = "$(Send-FL 'timer')"
            $t2 = [int][regex]::Match($timer, ' t=(\d+)ms').Groups[1].Value
            Check ($timer -match 'running=1' -and $t2 -gt $t1) "the clock runs: $t1 -> $t2 ms"
            $null = Save-FLShot -Name 'running' -Dir $dir
        } elseif ($r -eq 'split') {
            & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,speedrun=1,autopilot=1' -Save SaveC705.fl | Select-Object -Last 1
            Start-Sleep -Seconds 3
            $null = Send-FL 'livesplit start'
            $before = "$(Send-FL 'timer')"
            Check ($before -match 'next=M01a\.control') "the first checkpoint is next: $([regex]::Match($before, 'next=\S+').Value)"
            $station = @(Get-FLContacts -Dock -All) | Where-Object { -not $_.Ship -and $_.Type -notmatch 'lane|ring|gate|hole' } | Sort-Object Distance | Select-Object -First 1
            $dock = "$(Send-FLS "ap start dock $($station.Id)")"
            "  dock at $($station.Id), $([int]$station.Distance) m: $($dock.Substring(0, [Math]::Min(70, $dock.Length)))"
            $null = Wait-FLUntil { $null = Close-FLBlocker; "$(Send-FLS 'mode')" -eq 'docked' } 'docked' 180
            Start-FLLaunch | Select-Object -Last 1
            Start-Sleep -Seconds 2
            $after = "$(Send-FL 'timer')"
            Check ($after -match 'next=M01a\.wing') "the real launch split: $([regex]::Match($after, 'next=\S+').Value)"
        } else {
            & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller' | Select-Object -Last 1
            $null = Wait-FLUntil { "$(Send-FL 'uiitems')" -match '(?m)^FrontButton' } 'the main menu' 60
            Start-Sleep -Seconds 2
            $st = "$(Send-FL 'startprompt')"
            Check ($st -match 'wanted=0' -and $st -match 'taken=0') "the Controller's prompt is the one: $($st.Substring(0, [Math]::Min(50, $st.Length)))"
            $ng = "$(Send-FL 'newgame pick 2')"
            Check ($ng -match 'clicked main menu') "the pad's prompt still starts it: $($ng.Substring(0, [Math]::Min(60, $ng.Length)))"
            $until = (Get-Date).AddSeconds(60)
            do { Start-Sleep -Milliseconds 500; $null = Send-FLS 'movieskip'; $m = "$(Send-FLS 'mode')" } while ($m -eq 'menu' -and (Get-Date) -lt $until)
            Start-Sleep -Seconds 2
            Check ("$(Send-FL 'newgame state')" -match 'session=speedrun') 'the session reads speedrun'
        }
        Stop-Game
    }
} finally { Stop-Game }
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'speedrun without the controller: PASS'
