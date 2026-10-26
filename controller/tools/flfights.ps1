# Fight matrix (user, 19 Oct 2026: "make sure the retreat logic does not ruin fights that are otherwise winnable - a
# lot of variables and circumstances"): free-roam fights against a table of enemy kinds (npcships.ini templates:
# faction, class, level, count, start distance), each flown at the given caution settings - `caution 0` never leaves
# (can the autopilot win it?), 1 the normal setting. The game must be in space on the fixture save, which is reloaded
# before every fight; the game is expected to have been started fresh (long spawn sessions once exhausted the spawner).
# Returns one object per fight (Ship, Scenario, Caution, Outcome won / died / alive / spawn-failed, Retreated, Kills,
# Seconds, OddsPrior, OddsMeasured) and appends rows to research\experience\runs\fights_<Tag>_<time>.tsv.
# Usage (PowerShell tool): & controller\tools\flfights.ps1 -Save SaveC705 -Tag c705 [-Only <scenario>...]
#   [-Cautions 0,1] [-MaxSec 150]
param([string]$Save = 'SaveC705', [string]$Tag = 'c705', [string[]]$Only, [int[]]$Cautions = @(1), [int]$MaxSec = 150)
$fl = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
. (Join-Path $PSScriptRoot 'flctl.ps1')
. (Join-Path $fl 'tools\flbench.ps1')
$log = Join-Path (Get-FLInstallDir) 'EXE\FLController.log'   # this game instance's log (parallel test games)
$out = Join-Path $fl ("research\experience\runs\fights_{0}_{1:yyyy-MM-dd_HHmm}.tsv" -f $Tag, (Get-Date))
# The enemy kinds: n name, npc template, c count, d spawn distance (m)
$script:FLFightKinds = @(
    @{ n = 'pir_d1_x1'; npc = 'fc_lr_pi_fighter_d1'; c = 1; d = 1500 },
    @{ n = 'pir_d1_x5'; npc = 'fc_lr_pi_fighter_d1'; c = 5; d = 1500 },
    @{ n = 'pir_d5_x2'; npc = 'fc_lr_pi_fighter_d5'; c = 2; d = 1500 },
    @{ n = 'pir_d6_x3_close'; npc = 'fc_lr_pi_fighter_d6'; c = 3; d = 600 },
    @{ n = 'pir_d3_x3_300m'; npc = 'fc_lr_pi_fighter_d3'; c = 3; d = 300 },
    @{ n = 'pirelite_d7_x1'; npc = 'fc_lr_pi_elite_d7'; c = 1; d = 1500 },
    @{ n = 'pirelite_d9_x2'; npc = 'fc_lr_pi_elite_d9'; c = 2; d = 1500 },
    @{ n = 'pirfreight_d3_x2'; npc = 'fc_lr_pi_freighter_d3'; c = 2; d = 1500 },
    @{ n = 'navy_d4_x2'; npc = 'li_n_li_elite_d4'; c = 2; d = 1500 },
    @{ n = 'bh_f2_d5_x2'; npc = 'gd_im_ge_fighter2_d5'; c = 2; d = 1500 },
    @{ n = 'bh_f4_d8_x1'; npc = 'gd_im_ge_fighter4_d8'; c = 1; d = 1500 },
    @{ n = 'ku_gunboat_x1'; npc = 'ku_n_ku_gunboat_d1-9'; c = 1; d = 2000 },
    @{ n = 'heavy_d12_x1'; npc = 'gd_im_ge_fighter5_d12'; c = 1; d = 900 },
    @{ n = 'heavy_d12_x2'; npc = 'gd_im_ge_fighter5_d12'; c = 2; d = 900 },
    @{ n = 'heavy_d12_x4'; npc = 'gd_im_ge_fighter5_d12'; c = 4; d = 900 },
    # the retreat's own case (20 Oct 2026): hopeless but seen early - a good retreat gets away (alive), not just "left"
    @{ n = 'heavy_d12_x4_far'; npc = 'gd_im_ge_fighter5_d12'; c = 4; d = 2500 })
$kinds = if ($Only) { $script:FLFightKinds | Where-Object { $_.n -in $Only } } else { $script:FLFightKinds }
"ship`tscenario`tcaution`toutcome`tretreated`tkills`tseconds`todds_prior`todds_measured" | Set-Content $out
foreach ($k in $kinds) {
    foreach ($caution in $Cautions) {
        Load-FLSave $Save | Out-Null
        $null = Wait-FLUntil { (Send-FL 'state') -match 'mode=space' } 'space' 60
        Start-Sleep 2
        $null = Send-FL 'godmode off'
        $null = Send-FL "ap set caution $caution"
        $mark = (Get-Content $log).Count
        $null = Send-FL 'spawn clear'
        $r = Send-FL "spawn $($k.npc) player $($k.c) $($k.d)"
        $row = [pscustomobject]@{ Ship = $Tag; Scenario = $k.n; Caution = $caution; Outcome = 'spawn-failed'; Retreated = $false
                                  Kills = 0; Seconds = 0; OddsPrior = ''; OddsMeasured = '' }
        # (a reply "spawned failed: pub::SpaceObj::Create failed (-4)" means nothing was created)
        if ($r -match '^spawned [1-9]') {
            $null = Send-FL 'ap start combat'
            $t0 = Get-Date; $row.Outcome = 'alive'
            while (((Get-Date) - $t0).TotalSeconds -lt $MaxSec) {
                Start-Sleep -Milliseconds 500
                if ((Send-FL 'state') -notmatch 'mode=space') { $row.Outcome = 'died'; break }
                if ((Send-FL 'spawn list') -match '^spawned 0') { $row.Outcome = 'won'; break }
                # the fight ended without a retreat (a lost target, a rest): back to it
                if ((Get-FLAutopilot).Mode -in 'off', 'hold' -and -not ((Get-Content $log | Select-Object -Skip $mark) -match 'autopilot flee')) {
                    $null = Send-FL 'ap start combat'
                }
            }
            $row.Seconds = [int]((Get-Date) - $t0).TotalSeconds
            $null = Send-FL 'ap stop'
            $ev = Get-Content $log | Select-Object -Skip $mark
            $row.Retreated = [bool]($ev -match 'autopilot flee')
            $row.Kills = @($ev | Where-Object { $_ -match 'target-destroyed' }).Count
            $p = $ev | Where-Object { $_ -match 'odds=([\d.]+) prior' } | Select-Object -First 1
            $m = $ev | Where-Object { $_ -match 'odds=([\d.]+) measured' } | Select-Object -First 1
            if ($p -match 'odds=([\d.]+) prior') { $row.OddsPrior = $Matches[1] }
            if ($m -match 'odds=([\d.]+) measured') { $row.OddsMeasured = $Matches[1] }
        }
        $null = Send-FL 'spawn clear'
        ($row.PSObject.Properties.Value -join "`t") | Add-Content $out
        # benchmark history: the fight's time in context, with the combat stats of the autopilot's stop line
        $st = ($ev | Where-Object { $_ -match 'autopilot stop ' } | Select-Object -Last 1)
        $stats = "kills=$($row.Kills) odds=$($row.OddsPrior)/$($row.OddsMeasured)"
        foreach ($k2 in 'lost', 'dps', 'on-target', 'turns', 'extends') { if ("$st" -match " $k2=([\d.]+)") { $stats += " $k2=$($Matches[1])" } }
        $outcome = if ($row.Retreated -and $row.Outcome -ne 'won') { "$($row.Outcome)+retreat" } else { $row.Outcome }
        try { Add-FLBench -Suite fights -Case $k.n -Context "$Tag caution=$caution" -Outcome $outcome -Seconds $row.Seconds -Stats $stats } catch {}
        $row
    }
}
$null = Send-FL 'ap set caution 1'
