# In-game test, GitHub issue 33: a tractor refused at a limit says why. The script form of tests\loot_full_game.md (read its table first).
# First run 6 Nov 2026 (run p2z): case 9 passed, the rest found nothing to tractor; this is the second version (what changed: tests\loot_full_game.md "Fixture").
# Background games on a numbered instance, muted (deploy.ps1 -Background), driven by game state, events and debug verbs; frames and logs in
# research\experience\runs\loot_full_<Label>\ (git-ignored). Never run while another session holds a test game: it starts its own.
# Usage (PowerShell tool, from the worktree, only when the driver says the games are free):
#   tests\loot_full.ps1 -Label <name> [-Only 1,2,3,4,5,6,7,8,9] [-Variants] [-Sizes '3840, 2160','1280, 800'] [-Instance 7]
# Cases (the md's table): 1 nanobots, 2 shield batteries, 3 ammo at the limit (gun mounted), 4 ammo for an unmounted gun, 5 normal loot, 6 hold full,
#   7 out of range, 8 tractor all; 9 = the md's layout check (hud2 alert tractor 3|4|5|6, a frame each, and the longest at the three HUD sizes);
#   10 (only with -Only 10, last: it ends the ship) = what a destroyed ship's game answers. A farm of 20-odd kills in one game ends that way: split the cases over games.
# Games: the first size runs every case but 3 (SaveC701: docked, a dealer, a Starflier with no launcher); case 3 runs in its own game from -AmmoSave
#   (a ship that mounts a launcher, docked at a base to top the stack up, then launched); other sizes run only cases 1, 2 and 9.
# What lies in space: the game rolls a drop per item the dying ship CARRIES (loadouts.ini cargo). Kits and batteries therefore come only from an NPC that carries
#   them, found from the game data (Get-Carriers / Find-Carrier), not from a pirate fighter (it carries missile and countermeasure ammo only). No NPC carries a
#   commodity, so "an item with room" is any item that is not a kit, battery or ammo (a commodity when one lies there).
# Kills: by default the autopilot shoots the spawned ship (-KillBy autopilot): the drop roll uses the worth of the players involved, which a `destroytarget`
#   may not credit (first run: no kit, battery or ammo in 110 kills). A census of what lies there is printed after each kill so the next run shows what dropped.
# -Variants adds the md's variants of cases 1 and 5: hud_alerts 0, classic HUD, neural_net_reason 0 (classic HUD, then HUD 2), neural_net 0, then
#   one game each with [Modules] hud=0 and subtitles=0.
# Each result is PASS / FAIL / NOT RUN / LOOK (a frame a person or run-judge reads). NOT RUN is a fixture that could not be made, never a pass.
# Exit 1 = a FAIL, 2 = no FAIL but a NOT RUN. Not covered: multiplayer (the md's last section; tools\mptest and tests\mp_modules.ps1 are its own run).
param([string]$Label = 'run', [int[]]$Only = 1..9, [switch]$Variants,
      [string[]]$Sizes = @('3840, 2160', '1280, 800'), [int]$Instance = 7, [string]$Save = 'SaveC701.fl',
      # a save whose ship mounts a launcher (a Barracuda with missiles; a fight and a radiation zone: godmode is switched on at once)
      [string]$AmmoSave = (Join-Path $PSScriptRoot '..\Install\instances\3\Freelancer HD Edition\SAVE\Accts\SinglePlayer\SaveG004.fl'),
      [string]$ShipArch = 'fc_lr_pi_fighter_d1',   # the pirate fighter: carries missile01_mark01_ammo x20 and countermeasure ammo, so it is the source of ammo for an unmounted gun (4a) and of loose equipment
      [ValidateSet('autopilot', 'destroytarget')][string]$KillBy = 'autopilot',
      [int]$MaxKills = 12,        # kills for the ammo drops (cases 3 and 4): the loot rule's expected drops per kill are printed first, so this is a cap, not a hope
      [int]$MaxItemKills = 8)     # kills for the others (kit, battery, a loose item)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
$dir = Join-Path $root "research\experience\runs\loot_full_$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$bad = [Collections.Generic.List[string]]::new()
$notRun = [Collections.Generic.List[string]]::new()
$home_ = Get-FLDisplaySize
$script:Tag = ''
$script:Kills = 0
$script:Foreign = $null
$script:Mounted = @()
$script:Range = 0
$script:Carriers = $null

# (Write-Host: a helper's return value must never carry a result line with it)
function Result([string]$kind, [string]$what) { Write-Host ("{0,-7} {1}" -f $kind, $what); if ($kind -eq 'FAIL') { $bad.Add($what) } }
function Check([bool]$ok, [string]$what) { Result $(if ($ok) { 'PASS' } else { 'FAIL' }) $what }
function NotRun([string]$case, [string]$why) { Write-Host ("{0,-7} case {1}: {2}" -f 'NOT RUN', $case, $why); $notRun.Add("case ${case}: $why") }
function Say([string]$text) { Write-Host "        $text" }
function Tag([string]$size) { $size -replace '[ ,]+', 'x' }
function Shot([string]$name) {
    $n = "$($script:Tag)_$name"
    $f = Save-FLShot -Name $n -Dir $dir; Move-Item -LiteralPath $f (Join-Path $dir "$n.png") -Force; Result 'LOOK' "$n.png"
    "$n.png"
}
function Start-Game([string]$size, [string[]]$modules, [string]$saveFile) {
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Set-FLInstanceDisplay $Instance 'borderless' $size
    & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Modules $modules -Save $saveFile | Select-Object -Last 1
    $null = Wait-FLFramesPassed 200
}
function Stop-Game { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue }
function NoFault([string]$where) {
    $log = Join-Path (Get-FLInstallDir) 'EXE\FLController.log'
    Check (@(Select-String -Path $log -Pattern 'FATAL').Count -eq 0) "${where}: no fault in the log"
}
# the md's "at start the log must not hold a refused hook for why the tractor refused an item" (the hook's label, tractorhook.cpp)
function HookCheck([string]$where) {
    $log = Join-Path (Get-FLInstallDir) 'EXE\FLController.log'
    $l = @(Select-String -Path $log -Pattern 'why the tractor refused an item' | ForEach-Object { $_.Line })
    Check (-not @($l | Where-Object { $_ -match '(?i)refus|fail|could not|cannot|not verif' }).Count) "${where}: no refused hook for the tractor's reason ($($l.Count) log lines name it)"
}

# ---- readers
function Get-Self {
    $s = Send-FL 'ai self'
    $m = [regex]::Match($s, 'nanobots=(\d+)/(\d+) batteries=(\d+)/(\d+) tractor=(\d+) hold=([\d.]+)/([\d.]+)')
    if (-not $m.Success) { throw "ai self: $s" }
    $script:LastHull = [double][regex]::Match($s, 'hull=([\d.]+)').Groups[1].Value
    [pscustomobject]@{ Hull = $script:LastHull; Nano = [int]$m.Groups[1].Value; NanoMax = [int]$m.Groups[2].Value; Bat = [int]$m.Groups[3].Value; BatMax = [int]$m.Groups[4].Value
                       Tractor = [double]$m.Groups[5].Value; HoldFree = [double]$m.Groups[6].Value; HoldSize = [double]$m.Groups[7].Value }
}
function Get-Events([int]$after) {
    $out = @()
    foreach ($l in ((Send-FL "events $after") -split "`n")) {
        if ($l -match 'tractor-fail reason=(\d+) msg=(\d+)') { $out += [pscustomobject]@{ Kind = 'fail'; Reason = [int]$Matches[1]; Msg = [uint32]$Matches[2]; Line = $l.Trim() } }
    }
    $out
}
function Get-Loot { @(Get-FLContacts -All | Where-Object { $_.Type -eq 'loot' }) }
# the subtitles' caption feed (Send-FLS 'shown'): the position now, and the text since a position; '' / -1 when the subtitles module is off.
# The client's own Neural Net lines (the tractor's, target_out_of_range) show up here: the `nn-message` event is only the server side's (events.cpp NNMessageHook).
function Shown-Seq { try { [int][regex]::Match((Send-FLS 'shown 999999999'), '^last (\d+)').Groups[1].Value } catch { -1 } }
function Shown-Text([int]$since) { if ($since -lt 0) { return '' }; try { Send-FLS "shown $since" } catch { '' } }
function Clock { [double]([regex]::Match((Send-FL 'gameclock'), 'gameclock (\S+)').Groups[1].Value) }
# every nickname of the equipment data, by hash: tells a commodity or an ammo from the loot contact's good id
function Get-NickMap {
    if ($script:Nicks) { return }
    $script:Nicks = @{}
    foreach ($f in Get-ChildItem (Join-Path (Get-FLInstallDir) 'DATA\EQUIPMENT') -Filter '*.ini') {
        foreach ($l in [IO.File]::ReadLines($f.FullName)) {
            if ($l -match '^\s*nickname\s*=\s*(\S+)') { $script:Nicks[[uint32](Get-FLHash $Matches[1])] = $Matches[1].ToLower() }
        }
    }
}
function Nick($loot) { $n = $script:Nicks[[uint32]$loot.Good]; if ($n) { $n } else { "good$($loot.Good)" } }
function IsCommodity($loot) { (Nick $loot) -like 'commodity_*' }
function IsAmmo($loot) { (Nick $loot) -like '*_ammo' }
function IsKit($loot) { $n = Nick $loot; $n -eq 'ge_s_repair_01' -or $n -eq 'ge_s_battery_01' }
function IsBulk($loot) { -not (IsKit $loot) -and -not (IsAmmo $loot) }   # an item that is none of the limited ones (a commodity when one lies there)
# which NPC ship carries what: loadouts.ini `cargo = <item>, <n>` joined to npcships.ini's [NPCShipArch] loadout. The loot roll is per carried item.
function Get-Carriers {
    if ($script:Carriers) { return }
    $data = Join-Path (Get-FLInstallDir) 'DATA'
    $lo = @{}; $cur = $null
    foreach ($f in Get-ChildItem (Join-Path $data 'Ships') -Filter '*loadout*.ini') {
        foreach ($l in [IO.File]::ReadLines($f.FullName)) {
            if ($l -match '^\s*\[') { $cur = $null }
            elseif ($l -match '^\s*nickname\s*=\s*(\S+)') { $cur = $Matches[1].ToLower(); if (-not $lo.ContainsKey($cur)) { $lo[$cur] = @{} } }
            elseif ($cur -and $l -match '^\s*cargo\s*=\s*(\w+)\s*,\s*(\d+)') { $lo[$cur][$Matches[1].ToLower()] = [int]$Matches[2] }
        }
    }
    $script:Carriers = [Collections.Generic.List[object]]::new()
    $inArch = $false; $nick = $null
    foreach ($l in [IO.File]::ReadLines((Join-Path $data 'Missions\npcships.ini'))) {
        if ($l -match '^\s*\[(.+?)\]') { $inArch = $Matches[1] -eq 'NPCShipArch'; $nick = $null }
        elseif ($inArch -and $l -match '^\s*nickname\s*=\s*(\S+)') { $nick = $Matches[1] }
        elseif ($inArch -and $nick -and $l -match '^\s*loadout\s*=\s*(\S+)') {
            $cargo = $lo[$Matches[1].ToLower()]
            if ($cargo -and $cargo.Count) { $script:Carriers.Add([pscustomobject]@{ Arch = $nick; Cargo = $cargo }) }
        }
    }
}
# the NPC ship arch carrying every named item, with the most of the scarcest one; $null when none does. Count = that carried number.
function Find-Carrier([string[]]$items) {
    Get-Carriers
    $best = $null
    foreach ($c in $script:Carriers) {
        $min = [int]::MaxValue
        foreach ($i in $items) { $n = $c.Cargo[$i.ToLower()]; if (-not $n) { $min = 0; break }; if ($n -lt $min) { $min = $n } }
        if ($min -gt 0 -and (-not $best -or $min -gt $best.Count)) { $best = [pscustomobject]@{ Arch = $c.Arch; Count = $min } }
    }
    $best
}
# the game's own expected drops per kill for an item (the `loot` verb: its roll with the player's current worth), -1 unknown
function Get-Drops([string]$nick, [int]$carried) { $m = [regex]::Match((Send-FL "loot $nick $carried"), 'drops=([\d.]+)'); if ($m.Success) { [double]$m.Groups[1].Value } else { -1 } }
# ammo stack limit: Common.dll's MAX_PLAYER_AMMO (docs\FREELANCER_RE.md "Tractor failure codes": RVA 0x18D1D4), read from the install's own file
function Get-AmmoLimit {
    $b = [IO.File]::ReadAllBytes((Join-Path (Get-FLInstallDir) 'EXE\Common.dll'))
    $rva = 0x18D1D4
    $pe = [BitConverter]::ToInt32($b, 0x3C); $n = [BitConverter]::ToUInt16($b, $pe + 6); $opt = [BitConverter]::ToUInt16($b, $pe + 20)
    for ($i = 0; $i -lt $n; $i++) {
        $o = $pe + 24 + $opt + 40 * $i
        $va = [BitConverter]::ToUInt32($b, $o + 12); $vs = [BitConverter]::ToUInt32($b, $o + 8); $raw = [BitConverter]::ToUInt32($b, $o + 20)
        if ($rva -ge $va -and $rva -lt $va + $vs) { $v = [BitConverter]::ToInt32($b, [int]($raw + $rva - $va)); if ($v -ge 1 -and $v -le 999) { return $v } }
    }
    0
}
# the stack of an ammo as the `weapons` verb shows it on the mounted launcher (-1 = no launcher shows it)
function Get-AmmoStack([string]$nick) {
    $h = Get-FLHash $nick
    foreach ($l in (Send-FL 'weapons') -split "`n") { if ($l -match "ammo=(-?\d+) ammo_arch=$h\b") { return [int]$Matches[1] } }
    -1
}

# ---- loot farming: spawn, kill, read what lies there; every wait has a deadline (Set-FLDeadline per case)
# What lies there, by item: after each kill, so a run that finds nothing says what it did find.
function Census([string]$when) {
    $l = Get-Loot
    $by = @($l | Group-Object { Nick $_ } | Sort-Object Count -Descending | Select-Object -First 8 | ForEach-Object { "$($_.Name)x$($_.Count)" })
    Say "loot ${when}: $($l.Count) lying, $(@($l | Where-Object { $_.Distance -le $script:Range }).Count) in reach: $($by -join ' '); hull $((Get-Self).Hull)"
}
function Kill-Round([string]$arch, [double]$dist = 700) {
    $r = Send-FL "spawn $arch player 1 $([int]$dist)"
    if ($r -notmatch '^spawned \d') { throw "spawn ${arch}: $r" }
    $id = [uint32](@(($r -split '\|')[0] -split ' ' | Where-Object { $_ -match '^\d+$' })[0])
    $null = Wait-FLUntil { if (Get-FLContacts -All | Where-Object Id -eq $id) { 1 } } "spawned $arch in the contacts" 15
    $null = Set-FLTarget $id
    $how = $KillBy
    if ($how -eq 'autopilot') {
        $null = Send-FL 'ap start smart'   # (the game's weapons kill it: the player is the killer, as in a real fight)
        try { $null = Wait-FLUntil { if (-not (Get-FLContacts -All | Where-Object Id -eq $id)) { 1 } } "the autopilot kills $arch" 90 }
        catch { if ($_.Exception.Message -match 'OVER BUDGET') { $null = Send-FL 'ap stop'; throw }; $how = 'destroytarget' }
        $null = Send-FL 'ap stop'
    }
    if ($how -eq 'destroytarget' -and (Get-FLContacts -All | Where-Object Id -eq $id)) {
        $null = Set-FLTarget $id
        $null = Send-FL 'destroytarget'
        $null = Wait-FLUntil { if (-not (Get-FLContacts -All | Where-Object Id -eq $id)) { 1 } } "$arch destroyed" 10
    }
    # (godmode sent after a load does not protect: HARNESS.md. The hull is never repaired here, so a long farm ends in the ship's death, and the contact "killed" above only left with it)
    if ((Send-FL 'ai self') -eq 'not flying') { throw "the player's ship was destroyed during this kill (hull $($script:LastHull) at the last reading, $($script:Kills) kills): a test state" }
    $script:Kills++
    $null = Wait-FLFramesPassed 60   # (the drops appear a few frames after the kill)
    Census "after kill $($script:Kills) of $arch by $how"
    $f = Get-Loot | Where-Object { (IsAmmo $_) -and ($script:Mounted -notcontains (Nick $_)) } | Select-Object -First 1
    if ($f) { $script:Foreign = $f }
}
# The nearest loot matching the filter; kills of $arch until one lies there, at most $max. $null (and LastKills) when none: the caller reports NOT RUN.
function Find-Loot([scriptblock]$Match, [int]$max, [string]$arch = $ShipArch, [double]$dist = 700) {
    $k0 = $script:Kills
    while ($true) {
        $hit = Get-Loot | Where-Object $Match | Sort-Object Distance | Select-Object -First 1
        if ($hit) { $script:LastKills = $script:Kills - $k0; return $hit }
        if (-not $arch -or $script:Kills - $k0 -ge $max) { $script:LastKills = $script:Kills - $k0; return $null }
        try { Kill-Round $arch $dist } catch { if ($_.Exception.Message -match 'OVER BUDGET') { $script:LastKills = $script:Kills - $k0; return $null }; throw }
    }
}
function Near { $script:Range }

# ---- one tractor attempt. $expect: 'fail' (wait for the refusal), 'arrive' (the loot leaves the contacts, a refusal also ends the wait), 'none' (just 180 frames).
# The frame is taken the moment the refusal is seen (a card lives 3 s); .Lag is the seconds from seeing it to the file.
function Pull([string]$name, $loot, [string]$cmd, [string]$expect, [double]$sec = 12) {
    $seq = Get-FLEventSeq
    $s0 = Shown-Seq
    $id = [uint32]0
    if ($loot) { $id = [uint32]$loot.Id; $null = Set-FLTarget $id }
    $hold0 = (Get-Self).HoldFree
    $null = Send-FL "tap $cmd"
    $r = [pscustomobject]@{ Name = $name; Seq = $seq; S0 = $s0; Id = $id; Fail = @(); Shown = ''; Arrived = $false; Shot = ''; Lag = -1; Hold0 = $hold0; HoldAtFail = $null }
    if ($expect -eq 'none') { $null = Wait-FLFramesPassed 180 }
    else {
        try {
            $null = Wait-FLUntil {
                if (@(Get-Events $seq | Where-Object Kind -eq 'fail').Count) { return 'fail' }
                if ($expect -eq 'arrive' -and $id -and -not (Get-FLContacts -All | Where-Object Id -eq $id)) { return 'arrive' }
                $null
            } "${name}: the tractor's answer" $sec
        } catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw } }   # (a timeout is a result: no event, the loot still lying)
    }
    if (@(Get-Events $seq | Where-Object Kind -eq 'fail').Count) {
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $r.HoldAtFail = (Get-Self).HoldFree
        $r.Shot = Shot "${name}_card"
        $r.Lag = $sw.Elapsed.TotalSeconds
    }
    $null = Wait-FLFramesPassed 30
    $r.Fail = @(Get-Events $seq | Where-Object Kind -eq 'fail')
    $r.Shown = Shown-Text $s0
    $r.Arrived = [bool]($id -and -not (Get-FLContacts -All | Where-Object Id -eq $id))
    $r
}
# the caption in the Neural Net column (subtitles `shown`): mode 'reason' = TRACTOR FAILURE plus $word; 'plain' = TRACTOR FAILURE without the reason;
# 'none' = no tractor caption; 'skip' = just print what it said (HUD 2's card carries the reason by default and the column leaves the line out)
function Check-Caption([string]$case, $p, [string]$mode, [string]$word) {
    if ($mode -eq 'none') {
        $null = Wait-FLFramesPassed 240
        $t = Shown-Text $p.S0
        Check ($t -notmatch '(?i)tractor') "case ${case}: no tractor caption"
        return
    }
    if ($mode -eq 'skip') { $t = Shown-Text $p.S0; Result 'LOOK' "case ${case}: caption feed since the event: $(($t -split "`n" | Where-Object { $_ -match '(?i)tractor' }) -join ' | ')"; return }
    $t = ''
    try { $t = Wait-FLUntil { $x = Shown-Text $p.S0; if ($x -match '(?i)tractor') { $x } } "case ${case}: the Tractor Failure caption" 10 }
    catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw } }
    if (-not $t) { Check $false "case ${case}: caption wanted ($mode) but none appeared"; return }
    $null = Shot "$($p.Name)_caption"
    $line = (($t -split "`n" | Where-Object { $_ -match '(?i)tractor' }) -join ' | ')
    if ($mode -eq 'reason') { Check ($line -match '(?i)tractor failure' -and $line -match "(?i)$word") "case ${case}: caption '$line' names the reason ($word)" }
    else { Check ($line -match '(?i)tractor failure' -and $line -notmatch "(?i)$word") "case ${case}: caption '$line' is TRACTOR FAILURE alone" }
}
function Judge-Refusal([string]$case, $p, [int]$reason, [string]$word, $loot, [string]$cap = 'skip') {
    $want = Get-FLHash 'nnv_tractor_failure'
    $f = $p.Fail | Select-Object -First 1
    if (-not $f) { Check $false "case ${case}: no tractor-fail event (expected reason=$reason); caption feed: $(($p.Shown -split "`n" | Where-Object { $_ -match '(?i)tractor|range' }) -join ' | ')"; return }
    Check ($f.Reason -eq $reason -and $f.Msg -eq $want -and $p.Fail.Count -eq 1) "case ${case}: observed '$($f.Line)' x$($p.Fail.Count); expected reason=$reason msg=$want once"
    Check ([bool](Get-FLContacts -All | Where-Object Id -eq $loot.Id)) "case ${case}: the refused item still lies there"
    if ($p.Lag -gt 1) { Result 'LOOK' "case ${case}: frame taken $([Math]::Round($p.Lag, 2)) s after the event (the md wants under 1 s): judge it with care" }
    else { Result 'LOOK' "case ${case}: frame $($p.Shot) taken $([Math]::Round($p.Lag, 2)) s after the event; expected amber line: $word" }
    Check-Caption $case $p $cap $word
}
function Judge-Arrival([string]$case, $p, [string]$what) {
    Check ($p.Fail.Count -eq 0) "case ${case}: no tractor-fail event (observed $($p.Fail.Count)) $what"
    Check $p.Arrived "case ${case}: the item arrived (left the contacts) $what"
    Result 'LOOK' "case ${case}: the game's own Neural Net lines meanwhile (caption feed): $(($p.Shown -split "`n" | Where-Object { $_ }) -join ' | ')"
}

# ---- the cases
function Case-1([string]$tag = '', [string]$cap = 'skip') {
    $c = "1$tag"
    $self = Get-Self
    if ($self.Nano -ne $self.NanoMax) { NotRun $c "nanobots $($self.Nano)/$($self.NanoMax), not at the maximum"; return }
    if (-not $script:KitArch) { NotRun $c 'no NPC ship in the game data carries repair kits'; return }
    $kit = Get-FLHash 'ge_s_repair_01'
    $loot = Find-Loot { $_.Good -eq $kit -and $_.Distance -le (Near) } $MaxItemKills $script:KitArch
    if (-not $loot) { NotRun $c "no repair kit dropped in $($script:LastKills) kills of $($script:KitArch)"; return }
    $self = Get-Self   # (re-read right before the tractor: a spent consumable breaks the case)
    if ($self.Nano -ne $self.NanoMax) { NotRun $c "nanobots $($self.Nano)/$($self.NanoMax) before the tractor"; return }
    $p = Pull "c$c" $loot 'USER_TRACTOR_BEAM' 'fail'
    Judge-Refusal $c $p 5 'STORAGE CAPACITY FOR NANOBOTS IS FULL' $loot $cap
    Check ((Get-Self).Nano -eq $self.NanoMax) "case ${c}: nanobots unchanged"
}
function Case-2 {
    $self = Get-Self
    if ($self.Bat -ne $self.BatMax) { NotRun 2 "batteries $($self.Bat)/$($self.BatMax), not at the maximum"; return }
    if (-not $script:KitArch) { NotRun 2 'no NPC ship in the game data carries shield batteries'; return }
    $bat = Get-FLHash 'ge_s_battery_01'
    $loot = Find-Loot { $_.Good -eq $bat -and $_.Distance -le (Near) } $MaxItemKills $script:KitArch
    if (-not $loot) { NotRun 2 "no shield battery dropped in $($script:LastKills) kills of $($script:KitArch)"; return }
    $self = Get-Self
    if ($self.Bat -ne $self.BatMax) { NotRun 2 "batteries $($self.Bat)/$($self.BatMax) before the tractor"; return }
    $p = Pull 'c2' $loot 'USER_TRACTOR_BEAM' 'fail'
    Judge-Refusal 2 $p 4 'STORAGE CAPACITY FOR SHIELD BATTERIES IS FULL' $loot
    Check ((Get-Self).Bat -eq $self.BatMax) 'case 2: batteries unchanged'
}
function Case-3 {
    if (-not $script:Case3Ammo) { NotRun 3 $script:Case3Why; return }
    $nick = $script:Case3Ammo; $hash = Get-FLHash $nick
    $limit = $script:AmmoLimit
    $stack = Get-AmmoStack $nick
    if ($stack -ne $limit) { NotRun 3 "the stack of $nick is $stack, the limit $limit (not topped up at the dealer: cash, or no base reached)"; return }
    $loot = Find-Loot { $_.Good -eq $hash -and $_.Distance -le (Near) } $MaxKills $script:Case3Arch
    if (-not $loot) { NotRun 3 "no drop of $nick in $($script:LastKills) kills of $($script:Case3Arch) (limit $MaxKills): the fixture is not made, this is not a pass"; return }
    Say "case 3: $nick lies there after $($script:LastKills) kills"
    $stack = Get-AmmoStack $nick
    if ($stack -ne $limit) { NotRun 3 "the stack of $nick fell to $stack before the tractor"; return }
    $p = Pull 'c3' $loot 'USER_TRACTOR_BEAM' 'fail'
    Judge-Refusal 3 $p 6 'STORAGE CAPACITY FOR THIS AMMO IS FULL' $loot
    Check ((Get-AmmoStack $nick) -eq $limit) "case 3: the stack stays at $limit"
}
function Case-4 {
    # (a) a stack below the limit, ammo for a gun the ship does not mount: the item arrives. (b) at the limit: needs a stack of unmounted-gun ammo at 50.
    $loot = $null
    if ($script:Foreign) { $loot = Get-Loot | Where-Object { $_.Id -eq $script:Foreign.Id -and $_.Distance -le (Near) } | Select-Object -First 1 }
    if (-not $loot) { $loot = Find-Loot { (IsAmmo $_) -and ($script:Mounted -notcontains (Nick $_)) -and $_.Distance -le (Near) } $MaxKills $ShipArch }
    if (-not $loot) { NotRun '4a' "no ammo for an unmounted gun dropped in $($script:LastKills) kills of $ShipArch (limit $MaxKills)" }
    else {
        Say "case 4a: $(Nick $loot) lies there; mounted: $($script:Mounted -join ', ')"
        $p = Pull 'c4a' $loot 'USER_TRACTOR_BEAM' 'arrive' 20
        Judge-Arrival '4a' $p "($(Nick $loot), no stack of it before, expected: none)"
    }
    NotRun '4b' 'needs a stack of an unmounted gun''s ammo at the limit: the dealer path buys only the mounted launchers'' ammo (Invoke-FLBaseRestock); not made'
}
function Case-5 {
    # what a nanobot does at a full hull (first run: the count stayed): printed, so the wait for a dented hull below rests on a reading
    $s0 = Get-Self
    if ($s0.Hull -ge 0.995 -and $s0.Nano -ge 1) {
        $r = Send-FL 'usenano 1'; $null = Wait-FLFramesPassed 120
        Result 'LOOK' "case 5b: at hull $($s0.Hull) ``usenano 1`` says '$r': nanobots $($s0.Nano) -> $((Get-Self).Nano)"
    }
    Case-5a ''
    # (b) a nanobot with room: spend one first (the game's own use_item), then take a kit. The game spends one only on a dented hull, and the autopilot may have spent one already.
    if (-not $script:KitArch) { NotRun '5b' 'no NPC ship in the game data carries repair kits'; return }
    $kit = Get-FLHash 'ge_s_repair_01'
    $k0 = $script:Kills
    while ($true) {
        $self = Get-Self
        $loot = Get-Loot | Where-Object { $_.Good -eq $kit -and $_.Distance -le (Near) } | Sort-Object Distance | Select-Object -First 1
        if ($loot -and ($self.Hull -lt 0.995 -or $self.Nano -lt $self.NanoMax)) { break }
        if ($script:Kills - $k0 -ge $MaxItemKills) { NotRun '5b' "no repair kit in reach with a dented hull after $($script:Kills - $k0) kills (hull $($self.Hull), kit: $([bool]$loot))"; return }
        Kill-Round $script:KitArch
    }
    if ($self.Nano -eq $self.NanoMax) {
        $r = Send-FL 'usenano 1'
        Say "case 5b: hull $($self.Hull): usenano 1 -> $r"
        $null = Wait-FLUntil { if ((Get-Self).Nano -eq $self.NanoMax - 1) { 1 } } 'one nanobot spent' 10
    }
    $n1 = (Get-Self).Nano
    $p = Pull 'c5b' $loot 'USER_TRACTOR_BEAM' 'arrive' 25
    Judge-Arrival '5b' $p '(a repair kit with room)'
    $null = Wait-FLUntil { if ((Get-Self).Nano -gt $n1) { 1 } } 'the nanobot count up by the kit' 10
    Check ((Get-Self).Nano -gt $n1) "case 5b: nanobots $n1 -> $((Get-Self).Nano)"
}
# the normal pull on its own (case 5, variants, module games): an item with room arrives, no refusal, no tractor caption. A commodity if one lies there, else loose equipment.
function Case-5a([string]$tag = '') {
    $loot = Find-Loot { (IsBulk $_) -and $_.Fits -ge 1 -and $_.Distance -le (Near) } $MaxItemKills $ShipArch
    if (-not $loot) { NotRun "5a$tag" "no item with room lies there after $($script:LastKills) kills of $ShipArch (hold free $((Get-Self).HoldFree))"; return }
    $before = (Get-Self).HoldFree
    $p = Pull "c5a$tag" $loot 'USER_TRACTOR_BEAM' 'arrive' 25
    Judge-Arrival "5a$tag" $p "($(Nick $loot) x$($loot.Units), fits $($loot.Fits))"
    if ($before -gt 0) { Say "case 5a${tag}: hold free $before -> $((Get-Self).HoldFree)" }
}
function Case-6 {
    # fills the hold by tractoring loose items (so it runs last) until one reads fits=0, then that one is refused
    $loot = $null
    for ($i = 0; $i -lt $MaxItemKills -and -not $loot; $i++) {
        $loot = Get-Loot | Where-Object { (IsBulk $_) -and $_.Fits -eq 0 -and $_.Distance -le (Near) } | Sort-Object Distance | Select-Object -First 1
        if ($loot) { break }
        $fit = Get-Loot | Where-Object { (IsBulk $_) -and $_.Fits -ge 1 -and $_.Distance -le (Near) } | Sort-Object Distance | Select-Object -First 1
        if ($fit) {
            $h0 = (Get-Self).HoldFree
            $null = Pull 'c6fill' $fit 'USER_TRACTOR_BEAM' 'arrive' 25
            Say "case 6: filling the hold with $(Nick $fit) x$($fit.Units): free $h0 -> $((Get-Self).HoldFree)"
        } else { Kill-Round $ShipArch }
    }
    if (-not $loot) { NotRun 6 "no item read fits=0 in $MaxItemKills steps (hold free $((Get-Self).HoldFree)); equipment of volume 0 never fills it"; return }
    $self = Get-Self
    Say "case 6: hold free $($self.HoldFree)/$($self.HoldSize), $(Nick $loot) x$($loot.Units) fits $($loot.Fits)"
    $p = Pull 'c6' $loot 'USER_TRACTOR_BEAM' 'fail'
    Judge-Refusal 6 $p 3 'YOU DON''T HAVE ENOUGH CARGO SPACE' $loot
}
function Case-7 {
    $range = (Get-Self).Tractor
    $loot = Find-Loot { $_.Distance -gt ((Near) + 150) -and $_.Distance -lt ((Near) + 1500) } $MaxItemKills $ShipArch ($range + 600)
    if (-not $loot) { NotRun 7 "no loot beyond the reach ($range m) after $($script:LastKills) kills"; return }
    Say "case 7: $(Nick $loot) at $($loot.Distance) m, reach $range m"
    $p = Pull 'c7' $loot 'USER_TRACTOR_BEAM' 'none'
    Check ($p.Fail.Count -eq 0) "case 7: no tractor-fail event (observed $($p.Fail.Count))"
    Check (-not $p.Arrived) 'case 7: the item stays where it was'
    # the game's target_out_of_range line is a client-side Neural Net line: no event carries it, the caption feed does (TARGET OUT OF RANGE, subtitles.tsv)
    if ($p.Shown -match '(?i)out of range') { Result 'PASS' "case 7: the game's TARGET OUT OF RANGE caption played ($(($p.Shown -split "`n" | Where-Object { $_ -match '(?i)out of range' }) -join ' | '))" }
    else { Result 'LOOK' "case 7: no TARGET OUT OF RANGE caption in the feed (the line may be hidden by the column's alerts rule, or not queued); read the frame. feed: $(($p.Shown -split "`n" | Where-Object { $_ }) -join ' | ')" }
    $null = Shot 'c7_after'
}
function Case-8 {
    $self = Get-Self
    if ($self.Nano -ne $self.NanoMax) { NotRun 8 "nanobots $($self.Nano)/$($self.NanoMax), not at the maximum"; return }
    if (-not $script:KitArch) { NotRun 8 'no NPC ship in the game data carries repair kits'; return }
    $kit = Get-FLHash 'ge_s_repair_01'
    # both within USER_COLLECT_LOOT's 1,000 m (FREELANCER_RE: a literal, not the tractor's range)
    $k0 = $script:Kills
    $kitLoot = $null; $com = $null
    while ($script:Kills - $k0 -le $MaxItemKills) {
        $kitLoot = Get-Loot | Where-Object { $_.Good -eq $kit -and $_.Distance -le 900 } | Sort-Object Distance | Select-Object -First 1
        $com = Get-Loot | Where-Object { (IsBulk $_) -and $_.Fits -ge 1 -and $_.Distance -le 900 } | Sort-Object Distance | Select-Object -First 1
        if ($kitLoot -and $com) { break }
        Kill-Round $(if (-not $kitLoot) { $script:KitArch } else { $ShipArch }) 500
    }
    if (-not ($kitLoot -and $com)) { NotRun 8 "kit and an item with room not both within 900 m after $($script:Kills - $k0) kills (kit: $([bool]$kitLoot), item: $([bool]$com))"; return }
    $self = Get-Self
    if ($self.Nano -ne $self.NanoMax) { NotRun 8 "nanobots $($self.Nano)/$($self.NanoMax) before the tractor"; return }
    $p = Pull 'c8' $null 'USER_COLLECT_LOOT' 'arrive' 30
    $null = Wait-FLUntil { if (-not (Get-FLContacts -All | Where-Object Id -eq $com.Id)) { 1 } } 'the item pulled in' 30
    $ev = @(Get-Events $p.Seq | Where-Object Kind -eq 'fail')
    $f5 = @($ev | Where-Object Reason -eq 5)
    # the game lists volume-0 items first, then by value, and a refusal after the first success says nothing (FREELANCER_RE): equipment worth more than the kit goes first
    if ($f5.Count -eq 0 -and $com.Fits -ge [int]::MaxValue) { NotRun 8 "no refusal, as the game orders it: $(Nick $com) (volume 0) went ahead of the kit; the md's row needs a commodity, and no NPC carries one" }
    else { Check ($f5.Count -ge 1) "case 8: reason=5 observed ($($ev.Count) refusals: $(($ev | ForEach-Object { $_.Line }) -join ' / '))" }
    Check (-not (Get-FLContacts -All | Where-Object Id -eq $com.Id)) "case 8: the item ($(Nick $com)) still arrived"
    if ($null -ne $p.HoldAtFail) { Result 'LOOK' "case 8: hold at the refusal $($p.HoldAtFail) (before pull $($p.Hold0)): $(if ($p.HoldAtFail -ge $p.Hold0) { 'refused before the pull, as the game orders it' } else { 'the pull had begun' })" }
    Check ((Get-Self).Nano -eq $self.NanoMax) 'case 8: nanobots unchanged'
}
# the signature of a destroyed ship, which ended cases 5, 6 and 8 of two runs: no autopilot, enemies close by, until `ai self` says so; then what spawn and contacts answer
function Case-10 {
    $arch = $(if ($script:KitArch) { $script:KitArch } else { $ShipArch })
    $r = Send-FL "spawn $arch player 4 300"
    if ($r -notmatch '^spawned \d') { throw "spawn ${arch}: $r" }
    $null = Wait-FLUntil { if ((Send-FL 'ai self') -eq 'not flying') { 1 } else { $null = Get-Self; $null } } 'the ship destroyed' 300
    Say "case 10: last hull reading $($script:LastHull)"
    $sp = Send-FL "spawn $arch player 1 700"
    $co = Send-FL 'contacts'
    Check ($sp -match 'player location unknown') "case 10: spawn on a destroyed ship says '$sp'"
    Check ($co -match '^no contacts') "case 10: contacts on a destroyed ship say '$co'"
    $null = Shot 'c10_destroyed'
}
# the md's layout check: each card at this size, the longest also at the three HUD size presets; frames only (a person or run-judge reads them for clipping)
function Case-9 {
    foreach ($r in 3, 4, 5, 6) {
        $a = Send-FL "hud2 alert tractor $r"
        Check ($a -match '^alert tractor') "case 9: hud2 alert tractor $r -> $a"
        $null = Wait-FLFramesPassed 20
        $null = Shot "c9_reason$r"
        $t0 = Clock
        $null = Wait-FLUntil { if ((Clock) -ge $t0 + 3.3) { 1 } } 'the card ending' 15
    }
    foreach ($s in 1, 2, 3) {
        $null = Send-FL "set hud_size $s"; $null = Wait-FLFramesPassed 20
        $null = Send-FL 'hud2 alert tractor 4'; $null = Wait-FLFramesPassed 20
        $null = Shot "c9_reason4_hudsize$s"
        $t0 = Clock
        $null = Wait-FLUntil { if ((Clock) -ge $t0 + 3.3) { 1 } } 'the card ending' 15
    }
}
# the variants of cases 1 and 5 (the md's list). Each sets its switches, runs 1 and the normal pull, resets.
function Reset-Switches { $null = Send-FL 'set hud_alerts 1'; $null = Send-FL 'hud2 on'; try { $null = Send-FLS 'set neural_net_reason 1'; $null = Send-FLS 'set neural_net 1' } catch { } }
function Case-V {
    $steps = @(
        @{ tag = 'alerts0'; cmds = @('set hud_alerts 0'); hud2 = 'on'; cap = 'reason'; note = 'no card; the column reads the reason' },
        @{ tag = 'classic'; cmds = @(); hud2 = 'off'; cap = 'reason'; note = 'classic HUD: the caption top right' },
        @{ tag = 'reason0classic'; sub = @('set neural_net_reason 0'); hud2 = 'off'; cap = 'plain'; note = 'TRACTOR FAILURE alone' },
        @{ tag = 'reason0hud2'; sub = @('set neural_net_reason 0'); hud2 = 'on'; cap = 'skip'; note = 'the card still shows, no caption doubled (read the frame and the printed feed)' },
        @{ tag = 'nn0classic'; sub = @('set neural_net 0'); hud2 = 'off'; cap = 'none'; note = 'no text at all, the voice as ever' })
    foreach ($s in $steps) {
        "--- variant $($s.tag): $($s.note)"
        Reset-Switches
        foreach ($c in $s.cmds) { $null = Send-FL $c }
        foreach ($c in $s.sub) { $null = Send-FLS $c }
        $null = Send-FL "hud2 $($s.hud2)"; $null = Wait-FLFramesPassed 30
        Case-1 "_$($s.tag)" $s.cap
        Case-5a "_$($s.tag)"
    }
    Reset-Switches
}

# ---- one game: start, restock at the base (docking first when the save starts in space), launch, godmode, the cases in the md's order but 6 last (it fills the hold), then close
function Play([string]$size, [int[]]$cases, [string[]]$modules = @('subtitles', 'controller'), [string]$tag = '', [switch]$WithVariants, [string]$saveFile = $Save) {
    $script:Tag = (Tag $size) + $tag
    "=== game $($script:Tag): cases $($cases -join ',')$(if ($WithVariants) { ' + variants' }) from $(Split-Path $saveFile -Leaf), kills by $KillBy"
    Start-Game $size $modules $saveFile
    try {
        Start-FLEventLog | Out-Null
        HookCheck $script:Tag
        Get-NickMap
        $script:AmmoLimit = Get-AmmoLimit
        $script:Case3Ammo = $null; $script:Case3Arch = $null; $script:Case3Why = ''
        $needStock = [bool](@($cases | Where-Object { $_ -ge 1 -and $_ -le 8 }).Count -or $WithVariants)
        $startMode = Get-FLMode
        if ($startMode -ne 'docked') {
            $null = Wait-FLSpace 60
            $null = Send-FL 'godmode on'   # (the save's fight: the ship would not live through the dealer trip)
        } else { $null = Wait-FLUntil { if ((Get-FLMode) -eq 'docked') { 1 } } 'docked at the save' 90 }
        Say "worth: $(Send-FL 'worth')"
        if ($needStock) {
            # the ammo to top up and to farm: mounted, carried by some NPC ship (the loot roll is per carried item), best expected drops by the game's own rule (`loot <ammo> <carried>`)
            $script:Mounted = @(Get-FLMountedAmmo)
            $best = $null; $bestDrops = 0; $bestCar = $null
            foreach ($n in $script:Mounted) {
                $car = Find-Carrier @($n)
                if (-not $car) { Say "ammo ${n}: no NPC ship carries it"; continue }
                $d = Get-Drops $n $car.Count
                Say "ammo ${n}: carried by $($car.Arch) x$($car.Count), expected drops $d per kill"
                if ($d -gt $bestDrops) { $best = $n; $bestDrops = $d; $bestCar = $car }
            }
            if ($script:AmmoLimit -le 0) { $script:Case3Why = 'the ammo limit was not read from Common.dll (RVA 0x18D1D4)' }
            elseif (-not $script:Mounted.Count) { $script:Case3Why = 'the ship mounts no launcher with ammo sold as a good (run case 3 in the -AmmoSave game)' }
            elseif (-not $best) { $script:Case3Why = "no mounted ammo is carried by an NPC ship with expected drops above 0 (loot verb: $($script:Mounted -join ', '))" }
            else { $script:Case3Ammo = $best; $script:Case3Arch = $bestCar.Arch }
            Say "stack limit (Common.dll MAX_PLAYER_AMMO): $($script:AmmoLimit); case 3 ammo: $(if ($best) { "$best from $($bestCar.Arch)" } else { 'none' })"
            $script:KitArch = $null
            $kc = Find-Carrier @('ge_s_repair_01', 'ge_s_battery_01')
            if ($kc) { $script:KitArch = $kc.Arch; Say "kits and batteries from $($kc.Arch) (carries at least $($kc.Count) of each): expected drops kit $(Get-Drops 'ge_s_repair_01' $kc.Count), battery $(Get-Drops 'ge_s_battery_01' $kc.Count)" }
            # at a dealer: a save that starts in space docks first (the nearest base the game offers)
            try {
                if ($startMode -ne 'docked') { $null = Start-FLDock $null -TimeoutSec 240; $null = Wait-FLUntil { if ((Get-FLMode) -eq 'docked') { 1 } } 'docked to restock' 60 }
                if ([int][regex]::Match((Send-FL 'worth'), 'cash=(-?\d+)').Groups[1].Value -lt 20000) {
                    try { Send-FLConsole 'cash 200000' } catch { Say "(cash top-up by the console failed: $($_.Exception.Message))" }
                }
                Say "restock: $(Invoke-FLBaseRestock)"
                $null = Start-FLLaunch
            } catch {
                Say "restock path failed: $($_.Exception.Message)"
                if ($script:Case3Ammo) { $script:Case3Why = "the dealer was not reached: $($_.Exception.Message)"; $script:Case3Ammo = $null }
                if ((Get-FLMode) -eq 'docked') { $null = Start-FLLaunch }
            }
        } elseif ($startMode -eq 'docked') { $null = Start-FLLaunch }
        $null = Send-FL 'godmode on'
        Reset-Switches
        $null = Wait-FLFramesPassed 60
        $self = Get-Self
        $script:Range = $self.Tractor
        Say "ship: nanobots $($self.Nano)/$($self.NanoMax) batteries $($self.Bat)/$($self.BatMax) tractor $($self.Tractor) m hold $($self.HoldFree)/$($self.HoldSize)"
        Census 'at the start'
        if ($needStock -and ($self.Nano -ne $self.NanoMax -or $self.Bat -ne $self.BatMax)) { Say 'WARNING: the restock left a consumable below its maximum: cases 1/2/5b/8 will say so' }
        $order = 9, 1, 2, 3, 4, 5, 7, 8, 'V', 6, 10
        foreach ($k in $order) {
            $isV = $k -is [string]   # ('V' is the variants' slot; the others are case numbers)
            if ($isV) { if (-not $WithVariants) { continue } } elseif ($cases -notcontains $k) { continue }
            if (-not (Get-FLGameProcess)) { Result 'FAIL' "case ${k}: the game is gone"; break }
            "--- case $k"
            Set-FLDeadline $(if ($isV -or $k -eq 3 -or $k -eq 4) { 1500 } else { 900 }) "case $k"
            try { & "Case-$k" } catch { Result 'FAIL' "case ${k}: $($_.Exception.Message)" }
            Set-FLDeadline 0
        }
        NoFault $script:Tag
    } finally { Set-FLDeadline 0; Stop-Game }
}

try {
    $first = $Sizes[0]
    $main = @($Only | Where-Object { $_ -ne 3 })
    if ($main.Count -or $Variants) { Play $first ([int[]]$main) -WithVariants:$Variants }
    foreach ($size in $Sizes | Select-Object -Skip 1) {
        $c = @($Only | Where-Object { $_ -eq 1 -or $_ -eq 2 -or $_ -eq 9 })
        if ($c.Count) { Play $size ([int[]]$c) }
    }
    if ($Only -contains 3) { Play $first @(3) @('subtitles', 'controller') '_ammo' -saveFile $AmmoSave }
    if ($Variants) {
        Play $first @(1, 5) @('subtitles', 'controller', 'hud=0') '_hud0'
        Play $first @(1, 5) @('subtitles=0', 'controller') '_sub0'
    }
} finally {
    Stop-Game
    Set-FLInstanceDisplay $Instance 'borderless' $home_
}
if ($notRun.Count) { "loot_full: NOT RUN ($($notRun.Count)):"; $notRun | ForEach-Object { "  $_" } }
if ($bad.Count) { "loot_full: $($bad.Count) FAILED"; $bad | ForEach-Object { "  $_" }; exit 1 }
if ($notRun.Count) { "loot_full: no FAIL, but cases did not run; frames in $dir"; exit 2 }
"loot_full: no FAIL; frames in $dir"
