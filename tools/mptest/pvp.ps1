# Autopilot-led PvP between test clients on one local server (tools\mptest\README.md "Several clients"; the owner, 28
# Oct 2026: "entirely autopilot-led PvP between different clients on that server"). PowerShell tool only.
#
#   & tools\mptest\pvp.ps1 -Clients 21,22 [-Rounds 3] [-RoundSec 180] [-StallSec 45] [-Label name]
#
# Every client must be connected and in space (Start-FLMpSession -PvP; Start-FLLaunch). A round: each client runs
# `ap start combat <the next client's ship id>` (its own channel), then the watchdog samples every client ~every 2 s
# (hull / shield, the autopilot's mode / tactic / kills / dealt / gun accuracy, the distance to its target) into
# <run>\samples.tsv and stops the round at once on: a death (the log's player-death), a stall (no hull or shield lost by
# anyone and no shot fired for -StallSec), an autopilot that stopped, a client gone, or -RoundSec. After a round the dead
# respawn (RESPAWN) and launch; the round's summary (who killed whom, hull, accuracy, dealt, the chat's death line) goes
# to <run>\rounds.tsv and the transcript. Evidence: research\experience\runs\mp_pvp_<time>_<label>\ (+ the logs' tails).
param([int[]]$Clients = @(21, 22), [int]$Rounds = 3, [int]$RoundSec = 180, [int]$StallSec = 45, [string]$Label = 'pvp')

$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
. (Join-Path $root 'controller\tools\flctl.ps1')
. (Join-Path $PSScriptRoot 'mptest.ps1')
$run = Join-Path $root "research\experience\runs\mp_pvp_$(Get-Date -Format yyyyMMdd_HHmmss)_$Label"
$null = New-Item -ItemType Directory -Force $run
$log = Join-Path $run 'transcript.txt'
function Say([string]$s) { $l = "$(Get-Date -Format HH:mm:ss) $s"; $l; Add-Content -LiteralPath $log $l }

function Get-Own([int]$i) { Invoke-FLMpAt $i { [int][regex]::Match(((Send-FL 'contacts') -split "`n")[0], 'player=(\d+)').Groups[1].Value } }
function Get-LogPath([int]$i) { Invoke-FLMpAt $i { Join-Path (Get-FLInstallDir) 'EXE\FLController.log' } }
function Read-Ap([int]$i) {
    Invoke-FLMpAt $i {
        $s = Send-FL 'ap status'
        $self = Send-FL 'ai self'
        $o = [ordered]@{}
        foreach ($k in 'mode', 'tactic', 'target', 'kills', 'dealt', 'lost', 'gun-shots', 'gun-accuracy', 'hull', 'own-missile-hits', 'incoming-missile-hits', 'dealt-shield') {
            $o[$k] = [regex]::Match($s, "(?:^| )$([regex]::Escape($k))=(\S+)").Groups[1].Value
        }
        $o['hullf'] = [regex]::Match($self, 'hull[= ]+([\d.]+)').Groups[1].Value
        $o['shieldf'] = [regex]::Match($self, 'shield[= ]+(-?[\d.]+)').Groups[1].Value
        $t = $o['target']
        $o['dist'] = if ($t) { [regex]::Match(((Send-FL 'contacts') -split "`n" | Where-Object { $_ -match "^$t " } | Select-Object -First 1) + '', 'dist=(\d+)').Groups[1].Value } else { '' }
        $o['space'] = [bool]((Send-FL 'state') -match 'mode=space')
        [pscustomobject]$o
    }
}

$samples = Join-Path $run 'samples.tsv'
"round`tt`tclient`tmode`ttactic`ttarget`tdist`tkills`tdealt`tlost`tshots`taccuracy`thull`tshield" | Set-Content -LiteralPath $samples
"round`tend`tsecs`tclient`tdied`tkills`tdealt`tlost`tshots`taccuracy`thull`tdeathline" | Set-Content -LiteralPath (Join-Path $run 'rounds.tsv')
Say "PvP run ${run}: clients $($Clients -join ', '), $Rounds rounds of up to $RoundSec s, stall $StallSec s"

for ($r = 1; $r -le $Rounds; $r++) {
    # everyone in space, the ship ids, the logs' starting lines
    foreach ($i in $Clients) {
        Invoke-FLMpAt $i {
            if ((Send-FL 'state') -notmatch 'mode=space') {
                # a death shows its dialog (RESPAWN / NEW PLAYER / NEW SERVER / MAIN MENU) a few seconds after the ship
                # is gone: wait for it or for a base, then respawn (docked at the last base)
                $null = Wait-FLUntil { (Send-FL 'visible RespawnButtonButton') -match '^visible' -or (Send-FL 'state') -match 'base=0x[1-9a-f]|mode=space' } 'the death dialog or a base' 30
                if ((Send-FL 'visible RespawnButtonButton') -match '^visible') { Send-FL 'click RespawnButtonButton' | Out-Null; $null = Wait-FLUntil { (Send-FL 'state') -match 'base=0x[1-9a-f]' } 'respawned' 60 }
                # multiplayer respawns the ship as the server saved it, damage included (28 Oct 2026: a client came back
                # at 0.32 hull round after round): repair while docked
                if ((Send-FL 'state') -match 'base=0x[1-9a-f]') { $null = Wait-FLRoomReady -TimeoutSec 30; Write-Output "repair at the respawn base: $(Invoke-FLBaseRepair)" }
                if ((Send-FL 'state') -notmatch 'mode=space') { Start-FLLaunch | Out-Null }
            }
            # a fair round: a damaged survivor docks at the nearest base by autopilot (a station or planet with a dock;
            # not a lane ring or gate), repairs the player's way (the base's repair window) and launches - the first runs
            # carried 5-30 % hull into the next round, which then ended in its first seconds by a flee
            $hull = [double][regex]::Match((Send-FL 'ai self'), 'hull=([\d.]+)').Groups[1].Value
            if ($hull -lt 0.98) {
                $base = (Send-FL 'contacts') -split "`n" | Where-Object { $_ -match ' (station|planet|dockring)\b.* dock ' } | Select-Object -First 1
                $id = ($base -split ' ')[0]
                Send-FL "ap start travel $id" | Out-Null
                $null = Wait-FLUntil { (Send-FL 'state') -match 'base=0x[1-9a-f]' } "docked at $id to repair" 240
                $null = Wait-FLRoomReady -TimeoutSec 30 # (the base state flips before the room is up: "not docked")
                $rep = Invoke-FLBaseRepair
                Start-FLLaunch | Out-Null
                Write-Output "repair: hull $hull -> $rep"
            }
        }
    }
    $ids = @{}; $logAt = @{}
    foreach ($i in $Clients) { $ids[$i] = Get-Own $i; $logAt[$i] = (Get-Content -LiteralPath (Get-LogPath $i)).Count }
    # the clients find each other first (another system / base: travel there, nodock) - the next client's position
    for ($k = 0; $k -lt $Clients.Count; $k++) {
        $a = $Clients[$k]; $b = $Clients[($k + 1) % $Clients.Count]
        $seen = Invoke-FLMpAt $a { (Send-FL 'contacts') -split "`n" | Where-Object { $_ -match "^$($ids[$b]) " } }
        if (-not $seen) {
            $p = Invoke-FLMpAt $b { [regex]::Match((Send-FL 'ship'), 'pos=\(([-\d]+),([-\d]+),([-\d]+)\)') }
            Say "round ${r}: client $a does not see $b's ship $($ids[$b]): travel to it"
            Invoke-FLMpAt $a { Send-FL "ap start travel $($p.Groups[1].Value) $($p.Groups[2].Value) $($p.Groups[3].Value) nodock" | Out-Null }
            # (the wait inside the client's channel: Wait-FLUntil's popup step acts on $env:FLVIBED_INSTANCE's game)
            Invoke-FLMpAt $a { $null = Wait-FLUntil { (Send-FL 'contacts') -split "`n" | Where-Object { $_ -match "^$($ids[$b]) " } } "client $a sees client $b" 240 }
        }
    }
    foreach ($k in 0..($Clients.Count - 1)) {
        $a = $Clients[$k]; $b = $Clients[($k + 1) % $Clients.Count]
        $reply = Invoke-FLMpAt $a { Send-FL "ap start combat $($ids[$b])" }
        Say "round ${r}: client $a (ship $($ids[$a])) -> combat on client $b's ship $($ids[$b]): $(($reply -replace ' tactic=.*', ''))"
    }
    $t0 = Get-Date; $lastProgress = $t0; $prev = @{}; $last = @{}; $end = ''
    while (-not $end) {
        Start-Sleep -Milliseconds 1500
        $now = ((Get-Date) - $t0).TotalSeconds
        foreach ($i in $Clients) {
            if (-not (Invoke-FLMpAt $i { Get-FLGameProcess })) { $end = "client $i gone"; break }
            $s = Read-Ap $i
            if ($s.mode -ne 'off') { $last[$i] = $s } # a death resets the autopilot's numbers: the summary keeps these
            "$r`t$([int]$now)`t$i`t$($s.mode)`t$($s.tactic)`t$($s.target)`t$($s.dist)`t$($s.kills)`t$($s.dealt)`t$($s.lost)`t$($s.'gun-shots')`t$($s.'gun-accuracy')`t$($s.hullf)`t$($s.shieldf)" | Add-Content -LiteralPath $samples
            $key = "$($s.dealt)|$($s.'gun-shots')|$($s.hullf)|$($s.shieldf)"
            if ($prev[$i] -ne $key) { $lastProgress = Get-Date; $prev[$i] = $key }
            $new = Get-Content -LiteralPath (Get-LogPath $i) | Select-Object -Skip $logAt[$i]
            if ($new -match 'event player-death') { $end = "client $i died" }
            elseif ($s.mode -eq 'off' -and $now -gt 3) { $end = "client ${i}'s autopilot stopped: $(($new | Select-String 'autopilot stop|refused' | Select-Object -Last 1).Line)" }
        }
        if (-not $end -and ((Get-Date) - $lastProgress).TotalSeconds -gt $StallSec) { $end = "STALL: no shot, no damage for $StallSec s" }
        if (-not $end -and $now -gt $RoundSec) { $end = "time ($RoundSec s)" }
    }
    $secs = [int]((Get-Date) - $t0).TotalSeconds
    Say "round $r ends after $secs s: $end"
    foreach ($i in $Clients) {
        $s = Read-Ap $i
        $new = Get-Content -LiteralPath (Get-LogPath $i) | Select-Object -Skip $logAt[$i]
        $died = [bool]($new -match 'event player-death')
        if (($died -or $s.mode -eq 'off') -and $last[$i]) { $s = $last[$i]; if ($died) { $s.hullf = '0' } }
        # the fight's own stop event holds its final numbers (a kill or a death resets the live status at once)
        $stop = $new | Where-Object { $_ -match 'event autopilot stop mode=combat' } | Select-Object -Last 1
        if ($stop) {
            foreach ($k in 'kills', 'dealt', 'gun-shots', 'gun-accuracy', 'dealt-shield') {
                $m = [regex]::Match($stop, "(?:^| )$([regex]::Escape($k))=(\S+)"); if ($m.Success) { $s.$k = $m.Groups[1].Value }
            }
            $s.lost = [regex]::Match($stop, ' lost=(\S+)').Groups[1].Value
            Say "  client ${i} fight: $(($stop -replace '^.*event autopilot stop ', '').Substring(0, [Math]::Min(260, ($stop -replace '^.*event autopilot stop ', '').Length)))"
        }
        $death = Invoke-FLMpAt $i { ((Send-FL 'mp') -split "`n" | Where-Object { $_ -match 'Death:' } | Select-Object -Last 1) -replace '^\s*sender \d+ \(\w+\): ', '' }
        # a Hold, not a stop: a stopped ship drifts on at the fight's speed (28 Oct 2026: one drifted into a planet's lethal
        # zone between rounds and died; the hold also keeps it clear of lethal zones)
        Invoke-FLMpAt $i { if ((Send-FL 'state') -match 'mode=space') { Send-FL 'ap start hold' | Out-Null } }
        Say "  client ${i}: died=$died kills=$($s.kills) dealt=$($s.dealt) lost=$($s.lost) shots=$($s.'gun-shots') accuracy=$($s.'gun-accuracy') hull=$($s.hullf) | $death"
        "$r`t$end`t$secs`t$i`t$died`t$($s.kills)`t$($s.dealt)`t$($s.lost)`t$($s.'gun-shots')`t$($s.'gun-accuracy')`t$($s.hullf)`t$death" | Add-Content -LiteralPath (Join-Path $run 'rounds.tsv')
        $new | Set-Content -LiteralPath (Join-Path $run "round${r}_i$i.log")
    }
    if ($end -match 'gone') { break }
}
Say "done: $run"
