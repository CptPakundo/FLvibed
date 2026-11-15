# HUD 2's NPC icons (GitHub 84 + 85; audit 147_): each NPC ship's own top-down outline in the contact list and the target
# card, its rank mark, the rank word giving way. One background game per size (1280x800, 3840x2160), SaveC706 (free flight
# near Manhattan), the keyboard-and-mouse form (input_mode 2: the contact list mode), a mixed spawn (fighters light / heavy,
# a transport, a train, a gunboat, a cruiser, a repair ship, a lifter; hostile, neutral and friendly), frames of the list
# and of a ship target's card, `npcicons` (outlines built, bakes, their cost) and `perf cpu` (hud.npcicons per frame);
# then the verb's fixtures (tier 2 / 3 rank words, an over-long name, one model forced to its class icon): the list again
# and each fixture ship's card.
# Judged here only that the outlines were built and baked and nothing faulted; the look is the frames' (a run-judge / the owner).
# Usage (PowerShell tool): tests\npc_icons.ps1 [-Instance 5] [-Sizes 1280x800,3840x2160]   build\release. Exit 1 = a failure.
param([string]$Instance = '', [string[]]$Sizes = @('1280x800', '3840x2160'))
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if ($Instance) { $env:FLVIBED_INSTANCE = "$Instance" }
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
$dir = Join-Path $root ("research\experience\runs\npc_icons\{0:yyyyMMdd_HHmmss}" -f (Get-Date))
New-Item -ItemType Directory -Force $dir | Out-Null
$logFile = Join-Path $dir 'log.txt'
function Say([string]$s) { $s; Add-Content -LiteralPath $logFile $s }
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { Say ("{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what); if (-not $ok) { $bad.Add($what) } }
function Shot([string]$name) { $p = Save-FLShot -Name $name -Dir $dir; Say "  frame $p" }
# (npc family + level, faction, distance, extra): fighters light / heavy / very heavy, transport, train, gunboat, cruiser, repair, lifter
$spawns = @(
    @('fc_lr_pi_fighter_d2', 'player', 2600, 'level=2'), @('fc_lr_pi_fighter_d11', 'player', 2800, 'level=12'),
    @('li_n_li_elite_d5', 'li_n_grp', 900, 'level=8'), @('li_p_li_fighter_d4', 'li_p_grp', 1000, 'level=3'),
    @('co_vr_ge_transport_d5', 'co_vr_grp', 1200, ''), @('co_hsp_ge_train_d5', 'co_hsp_grp', 1500, ''),
    @('br_n_br_gunboat_d10', 'br_n_grp', 1700, 'level=10'), @('li_n_cruiser_d22', 'li_n_grp', 1900, 'level=22'),
    @('co_kt_ge_repair_d5', 'co_kt_grp', 1300, ''), @('co_ti_ge_lifter_d5', 'co_ti_grp', 1100, ''))
# (ship archetype, the name its NPC ships are shown under): Liberty Navy tier 2 / 3, the pirates' tier 3, Liberty Police tier 2
$fixtures = @(
    @('li_elite', 'Capt. Dana Whitfield'), @('li_cruiser', 'Col. Jonathan Blackwood-Harrington'),
    @('pi_fighter', 'Ace Mara Kovac'), @('li_fighter', 'Sgt. Rhea Fallon'))
foreach ($size in $Sizes) {
    $wh = $size -split 'x'
    Say "--- $size"
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
    Set-FLInstanceDisplay $env:FLVIBED_INSTANCE 'borderless' "$($wh[0]), $($wh[1])"
    & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $env:FLVIBED_INSTANCE -Save 'SaveC706.fl' | Select-Object -Last 1 | ForEach-Object { Say "  $_" }
    try {
        $null = Wait-FLUntil { (Get-FLMode) -eq 'space' } 'in space' 90
        $null = Send-FL 'fpscap 60'
        $null = Send-FL 'set hud_style 2'
        $null = Send-FL 'set hud_fade 0'
        $null = Send-FL 'set input_mode 2'
        $null = Wait-FLFramesPassed 200
        foreach ($s in $spawns) {
            $npc = Resolve-FLNpc $s[0]
            Say "  spawn $npc $($s[1]): $(Send-FL "spawn $npc $($s[1]) 1 $($s[2]) $($s[3])")"
        }
        $null = Wait-FLFramesPassed 240
        Say "  $((Send-FL 'npcicons') -join ' ')"
        $null = Send-FL 'hud2 click 6' # the list header: the contact list mode (every contact, nearest first)
        $null = Wait-FLFramesPassed 20
        $null = Send-FL 'hud2 click 8' # its SHIPS filter
        $null = Wait-FLFramesPassed 240 # (the 10 Hz refresh asks; the worker builds; the draw bakes)
        Shot "${size}_list"
        $null = Send-FL 'perf cpu'; $null = Wait-FLFramesPassed 300
        $perf = Send-FL 'perf cpu'
        $line = ($perf -split "`n" | Where-Object { $_ -match 'npcicons|hud2\.Render|frames' }) -join ' | '
        Say "  perf: $line"
        # ship targets: the nearest enemy (a pirate: red outline, rank), then the nearest contacts (the card's rank mark and outline)
        $null = Send-FL 'tap USER_NEXT_ENEMY'; $null = Wait-FLFramesPassed 60
        Shot "${size}_target_enemy"
        $null = Send-FL 'tap USER_NEXT_TARGET'; $null = Wait-FLFramesPassed 60
        Shot "${size}_target_next"
        $null = Send-FL 'tap USER_NEXT_TARGET'; $null = Wait-FLFramesPassed 60
        Shot "${size}_target_next2"
        # the name fixtures (npcicons name: the traffic brings tier 1 words and names of at most 23 characters only): tier 2
        # and 3 words, an over-long name (its rank word gives way first, the mark stays), and one model forced to draw its
        # class icon (npcicons fail: the fallback of a model with no outline; the Patriot, li_p_li_fighter)
        foreach ($f in $fixtures) { Say "  $(Send-FL "npcicons name $($f[0]) $($f[1])")" }
        Say "  $(Send-FL 'npcicons fail li_fighter')"
        $null = Send-FL 'tap USER_CLEAR_TARGET'; $null = Wait-FLFramesPassed 60
        if ("$(Send-FL 'hud2 state')" -notmatch ' list=1 ') { # (the list mode again if a target left it: the header toggles)
            $null = Send-FL 'hud2 click 6'; $null = Wait-FLFramesPassed 20
            $null = Send-FL 'hud2 click 8'
        }
        $null = Wait-FLFramesPassed 120
        Shot "${size}_list_fixtures"
        foreach ($f in $fixtures) {
            # target the ships nearest first until the card names the fixture's ship (the game's next-target key cycled
            # without reaching the Liberty ships in the first run; the contacts carry the game's name, not the fixture's)
            $found = $false
            foreach ($c in @(Get-FLContacts | Where-Object { $_.Ship } | Sort-Object Distance)) {
                $null = Send-FL "settarget $($c.Id)"; $null = Wait-FLFramesPassed 12
                if ("$(Send-FL 'hud2 state')" -match ("target=1 '" + [regex]::Escape($f[1]) + "'")) { $found = $true; break }
            }
            Check $found "$size`: targeted the ship shown as '$($f[1])'"
            if ($found) { $null = Wait-FLFramesPassed 60; Shot "${size}_target_$($f[0])" }
        }
        Say "  $(Send-FL 'npcicons clear')"
        $n = (Send-FL 'npcicons') -join ' '
        Say "  $n"
        $outl = [int]([regex]::Match($n, 'outlines=(\d+)').Groups[1].Value)
        $bakes = [int]([regex]::Match($n, 'bakes=(\d+)').Groups[1].Value)
        Check ($outl -ge 6) "$size`: $outl ship outlines built from their models"
        Check ($bakes -ge 6) "$size`: $bakes cells baked"
        $log = Join-Path (Get-FLInstallDir) 'EXE\FLController.log'
        Copy-Item -LiteralPath $log -Destination (Join-Path $dir "FLController_$size.log") -ErrorAction SilentlyContinue
        Check (@(Select-String -Path $log -Pattern 'FATAL').Count -eq 0) "$size`: no fault in the log"
        Say "  $((Select-String -Path $log -Pattern 'npcicons:' | Select-Object -Last 3 | ForEach-Object { $_.Line }) -join ' / ')"
    } finally {
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
    }
}
Say "evidence: $dir"
if ($bad.Count) { "npc icons: $($bad.Count) FAILED"; exit 1 }
'npc icons: OK'
