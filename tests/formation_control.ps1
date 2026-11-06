# Is `target-churn`'s "left the formation" caused by the churn? The section's formation leg, in one background game, with and without
# the churn, round for round: each round loads SaveC705, joins the nearest friendly ship's formation and watches the distance to the
# wing leader for 6 s; odd rounds touch nothing, even rounds churn as the section does. One line a round in
# runs\formation_control\<label>.txt: leader, maximum distance, the speed range, cruise seen, whether the maneuver stayed formation.
# Usage (PowerShell tool): tests\formation_control.ps1 -Label <name> [-Rounds 30] [-Instance 1]   uses build\release
param([Parameter(Mandatory)][string]$Label, [int]$Rounds = 30, [int]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
$dir = Join-Path $root 'research\experience\runs\formation_control'
New-Item -ItemType Directory -Force $dir | Out-Null
$file = Join-Path $dir "$Label.txt"
function Note([string]$line) { Add-Content -LiteralPath $file -Value $line; Write-Host $line }

$dist = { param($id) $c = @(Get-FLContacts -All) | Where-Object Id -eq $id | Select-Object -First 1; if ($c) { [double]$c.Distance } else { -1 } }
& (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save SaveC705.fl | Select-Object -Last 1
try {
    for ($r = 1; $r -le $Rounds; $r++) {
        $churned = ($r % 2) -eq 0
        try {
            Clear-FLPad
            $null = Load-FLSave SaveC705.fl
            $null = Step-FLPopups -Now
            $cs = @(Get-FLContacts -All)
            $friend = $cs | Where-Object { $_.Ship -and $_.Attitude -gt 0.2 -and $_.Distance -lt 6000 } | Sort-Object Distance | Select-Object -First 1
            if (-not $friend) { Note "round $r no friend"; continue }
            $stations = @($cs | Where-Object { $_.Dock -and -not $_.Ship -and $_.Type -notmatch 'lane|ring|gate|hole' } | Sort-Object Distance)
            $planet = $cs | Where-Object { $_.Type -match 'planet' } | Select-Object -First 1
            $far = $cs | Sort-Object Distance -Descending | Select-Object -First 1
            $churn = @({ Send-FL 'tap USER_NEXT_ENEMY' }, { Send-FL 'tap USER_CLEAR_TARGET' },
                       { Send-FL 'tap USER_CLEAR_TARGET'; Send-FL 'tap USER_CLEAR_TARGET' },
                       { Send-FL "settarget $($stations[-1].Id)" }, { if ($planet) { Send-FL "settarget $($planet.Id)" } },
                       { Send-FL "settarget $($far.Id)" },
                       { Set-FLPad -RX 0.15; $null = Wait-FLFramesPassed 18 1; Set-FLPad },
                       { Set-FLPad -LX 0.15; $null = Wait-FLFramesPassed 18 1; Set-FLPad })
            $null = Start-FLFormation $friend.Id
            $null = Wait-FLUntil { $null = Step-FLPopups; $p = Get-FLPilot; if (-not $p -or -not $p.Leader) { return $false }; $d = & $dist $p.Leader; $d -ge 0 -and $d -le 250 } 'formed' 45
            $t0 = Get-Date; $last = Get-FLPilot; $lastT = $t0; $i = 0
            $maxD = 0.0; $minV = 9999.0; $maxV = 0.0; $cruise = 0; $lost = ''; $leader = $last.Leader; $series = @()
            while (((Get-Date) - $t0).TotalSeconds -lt 6) {
                if ($churned) { $null = & $churn[$i % $churn.Count] }
                $i++
                $null = Wait-FLFramesPassed 20 2
                $p = Get-FLPilot; $now = Get-Date
                if (-not $p) { $lost = 'no pilot'; break }
                $dt = ($now - $lastT).TotalSeconds
                $v = if ($dt -gt 0) { [Math]::Sqrt((0..2 | ForEach-Object { [Math]::Pow($p.Pos[$_] - $last.Pos[$_], 2) } | Measure-Object -Sum).Sum) / $dt } else { 0 }
                $d = & $dist $leader
                $maxD = [Math]::Max($maxD, $d); $minV = [Math]::Min($minV, $v); $maxV = [Math]::Max($maxV, $v)
                if ($p.Cruise) { $cruise++ }
                $series += "$([int]$v)/$([int]$d)"
                $last = $p; $lastT = $now
                if ($p.Op -ne 'formation') { $lost = "op $($p.Op)"; break }
            }
            Note ("round {0} {1} friend {2} {3} leader {4} max {5:N0} v {6:N0}-{7:N0} cruise {8}/{9} {10} | {11}" -f $r, $(if ($churned) { 'churn  ' } else { 'control' }), $friend.Id, $friend.Name, $leader, $maxD, $minV, $maxV, $cruise, $i, $(if ($lost) { "LOST $lost" } elseif ($maxD -gt 300) { 'OVER' } else { 'ok' }), ($series -join ' '))
            $null = Stop-FLManeuver
        } catch { Note "round $r error: $_" }
    }
} finally {
    Clear-FLPad
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
