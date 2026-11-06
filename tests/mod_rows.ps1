# What a mod's install must also do, in one game each (docs\MODULES_PLAN.md "The per-module flight plan"): -Case rooms: a New Game's room scene, its subtitle and frame.
# fight: HUD 2's ring with enemies on the player, its state and a frame. lane: the autopilot rides a trade lane: it enters, rides at the lane's speed, leaves in flight.
# Usage (PowerShell tool): tests\mod_rows.ps1 -Case rooms|fight|lane [-Instance 1] [-Label x]   build\release. Exit 1 = a failure. Frames: runs\mod_rows\<label>.
param([ValidateSet('rooms', 'fight', 'lane')][string]$Case = 'lane', [string]$Instance = '1', [string]$Label = 'run')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = $Instance
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$frames = Join-Path $root "research\experience\runs\mod_rows\$Label"
New-Item -ItemType Directory -Force $frames | Out-Null
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
$deploy = Join-Path $root 'tools\deploy.ps1'
try {
    switch ($Case) {
        'rooms' {
            & $deploy -Background -NoBuild -Release -Instance $Instance | Select-Object -Last 1
            $s0 = Get-Date
            "menu new: $(Send-FL 'menu new')"
            $null = Wait-FLUntil { $null = Close-FLBlocker; $null = Send-FLS 'movieskip'; (Get-FLMode) -eq 'docked' } 'docked' 120
            # a New Game lands on the launch pad, where nobody speaks: the story's first scene waits in the bar
            # the whole history (`shown 0`), so no window between two polls can miss a line
            $shown = $null; $t = [Diagnostics.Stopwatch]::StartNew()
            while ($t.Elapsed.TotalSeconds -lt 70 -and -not $shown) {
                $null = Close-FLBlocker
                $shown = ("$(Send-FLS 'shown 0' 500)" -split "`n" | Select-Object -Skip 1 | Where-Object { $_ -match '\S' } | Select-Object -First 1)
                if ($t.Elapsed.TotalSeconds -gt 30 -and -not $script:bar) { $script:bar = $true; "bar: $(Goto-FLRoom 'Bar' -Watch)"; "talk: $(Send-FL 'talk story')" }
                Start-Sleep -Milliseconds 120
            }
            "lines so far: $("$(Send-FLS 'shown 0' 500)" -split "`n" | Select-Object -First 1)"
            $null = Save-FLShot -Name "$Instance-room" -Dir $frames
            Check ([bool]$shown) "a line shows in the opening scene: $("$shown".Substring(0, [Math]::Min(90, "$shown".Length)))"
            "mode: $(Get-FLMode)"
        }
        'fight' {
            & $deploy -Background -NoBuild -Release -Instance $Instance -Save SaveC701.fl | Select-Object -Last 1
            "launch: $(Start-FLLaunch)"
            "style: $(Send-FL 'set hud_style 1'); $(Send-FL 'set input_mode 1')"   # (HUD 2 on the pad: an earlier test's saved options must not decide this)
            Set-FLPad -LX 0.02; $null = Wait-FLFramesPassed 20; Clear-FLPad; $null = Wait-FLFramesPassed 20
            "spawn: $(Send-FL "spawn $(Resolve-FLNpc 'fc_lr_pi_fighter_d1') player 3 700")"
            $hit = $false; $t = [Diagnostics.Stopwatch]::StartNew()
            while ($t.Elapsed.TotalSeconds -lt 25 -and -not $hit) { Start-Sleep -Milliseconds 150; $hit = "$(Send-FL 'hud2 state')" -match 'combat=1' }
            $state = "$(Send-FL 'hud2 state')" -replace "`r?`n", ' | '
            Check $hit "HUD 2 knows the fight: $($state.Substring(0, [Math]::Min(100, $state.Length)))"
            $null = Save-FLShot -Name "$Instance-fight" -Dir $frames
        }
        'lane' {
            & $deploy -Background -NoBuild -Release -Instance $Instance -Save SaveC701.fl | Select-Object -Last 1
            "launch: $(Start-FLLaunch)"
            # The nearest ring by the system's own file (the contact lists leave rings out, and a mod may name them otherwise); a solar's id is its nickname's hash
            . (Join-Path $root 'tools\flhash.ps1')
            $ini = Get-ChildItem -LiteralPath (Join-Path (Get-FLInstallDir) 'DATA\UNIVERSE\SYSTEMS\LI01') -Filter 'Li01.ini' | Select-Object -First 1
            $at = [regex]::Match("$(Send-FL 'ship')", 'pos=\((-?\d+),(-?\d+),(-?\d+)\)')
            $near = 1e12; $nick = ''
            foreach ($b in ((Get-Content -LiteralPath $ini.FullName -Raw) -split '(?m)^\[') | Where-Object { $_ -match '(?im)^archetype\s*=\s*Trade_Lane_Ring' }) {
                $n = [regex]::Match($b, '(?im)^nickname\s*=\s*(\S+)').Groups[1].Value
                $q = [regex]::Match($b, '(?im)^pos\s*=\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)')
                $d = [Math]::Sqrt(([double]$q.Groups[1].Value - [double]$at.Groups[1].Value) * ([double]$q.Groups[1].Value - [double]$at.Groups[1].Value) + ([double]$q.Groups[3].Value - [double]$at.Groups[3].Value) * ([double]$q.Groups[3].Value - [double]$at.Groups[3].Value))
                if ($d -lt $near) { $near = $d; $nick = $n }
            }
            if (-not $nick) { throw 'no Trade_Lane_Ring in the system file' }
            $id = Get-FLHash $nick
            "ring $nick ($id) at $([int]$near) m: $("$(Send-FL "ap start dock $id")".Substring(0, 50))"
            $top = 0.0; $rode = $false; $t = [Diagnostics.Stopwatch]::StartNew()
            while ($t.Elapsed.TotalSeconds -lt 150) {
                $p = "$(Send-FL 'pilot')"
                $v = [double][regex]::Match("$(Send-FL 'hud2 state')", 'speed=(\d+)').Groups[1].Value
                if ($p -match ' lane=1') { if (-not $rode) { "in the lane at $([int]$t.Elapsed.TotalSeconds) s" }; $rode = $true; $null = Save-FLShot -Name "$Instance-lane" -Dir $frames }
                elseif ($rode) { break }
                Start-Sleep -Milliseconds 400
            }
            Check $rode 'the autopilot entered the lane'
            Check ((Get-FLMode) -eq 'space') "it left the lane in flight (mode $(Get-FLMode), $([int]$t.Elapsed.TotalSeconds) s)"
            $hp = "$(Send-FL 'ship')" -replace "`r?`n", ' '
            "after: $($hp.Substring(0, [Math]::Min(70, $hp.Length)))"
        }
    }
} finally {
    Clear-FLPad
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
if ($bad.Count) { "mod rows ${Case}: $($bad.Count) FAILED"; exit 1 }
"mod rows ${Case}: OK"
