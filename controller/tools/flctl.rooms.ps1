# FLController harness - story state, blockers, rooms, launch, base repair / restock, base stories, space waits. Part of flctl.ps1 (dot-sourced by it; never load a part alone).

# Where the game and the story stand, from game state: mode, the room (busy reason), open dialogs that decide the
# flow, what the room's characters offer, the latest objective / mission message event. For deciding the next step
# (with the mission script / runbook) and for error messages.
function Get-FLStoryState {
    $dialogs = 'MMOkButton', 'NavMapContinueButton', 'OfferDialogAcceptButton', 'OfferDialogCloseButton', 'BtnAccept',
               'LoadSavegameButton', 'ExitButtonBox1', 'PauseButtonBox0', 'CTID'
    $open = @($dialogs | Where-Object { (Send-FL "visible $_") -match '^visible' })
    $room = ((Send-FL 'hotspots') -split "`n")[-1]
    $offers = @((Send-FL 'talkers') -split "`n" | Select-Object -Skip 1 | ForEach-Object { ($_ -split '\s+')[-1] } |
                Where-Object { $_ -and $_ -ne 'talk_blowoff' } | Sort-Object -Unique)
    $last = @(Get-FLEvents | Where-Object { $_ -match 'objective|mission-message|player-death' } | Select-Object -Last 1)
    "mode=$(Get-FLMode); room: $room; dialogs: $(if ($open) { $open -join ',' } else { 'none' }); offers: " +
        "$(if ($offers) { $offers -join ',' } else { 'none' }); last story event: $(if ($last) { $last[0] } else { 'none' })"
}

function Get-FLMode { [regex]::Match((Send-FL 'state'), 'mode=(\S+)').Groups[1].Value }

# Dialogs the story puts in the way of the flow, closed the way a player dismisses them (native clicks): tutorial
# popups and mission popups (`Act_PopupDialog`, MMOkButton) and the mission briefing shown on launching with a
# mission (nav map: NavMapContinueButton). Called by the harness waits so mission scripting never stalls a test;
# returns the closed control's name or ''. Offers, rumours and decisions are not closed here (the test decides).
function Close-FLBlocker {
    foreach ($b in 'MMOkButton', 'NavMapContinueButton') {
        if ((Send-FL "visible $b") -match '^visible') {
            $r = Send-FL "click $b"
            if ($r -notmatch '^clicked') { throw "Close-FLBlocker: $r" }
            return $b
        }
    }
    ''
}

# Skips the base animation that is running, as Escape does (USER_CANCEL through the game's own command system),
# only while the game reports one: in a base, the NavBar busy with a room's entry transition (`hotspots ... busy:
# navbar+..`), gone during the launch / landing sequence (`busy: no navbar`), or a room scene (story / bar
# cutscene) hiding the room buttons (`busy: scene`). Never otherwise: with nothing to skip, USER_CANCEL opens the
# pause menu. Returns $true if it skipped. Tests whose subject is the animation or scene (e.g. its subtitles) pass
# -Watch to the functions that use this (Goto-FLRoom, Load-FLSave, Start-FLLaunch, Wait-FLRoomReady).
function Skip-FLAnimation {
    $st = Send-FL 'state'
    # In space only the docking / landing cutscene (docking camera 0x679634) is skippable. Story cutscenes in space
    # (cutscene camera 0x6792F8) are not (HD Edition skips story cinematics "excluding space cutscenes"): never
    # cancel those, the cancel would open the pause menu once they end.
    if ($st -match 'mode=cutscene' -and $st -match 'camera=0x679634\b') { $null = Send-FL 'tap USER_CANCEL'; return $true }
    if ($st -notmatch 'mode=docked') { return $false }
    if ((((Send-FL 'hotspots') -split "`n")[-1]) -notmatch 'busy: (navbar|no navbar|scene)') { return $false }
    $null = Send-FL 'tap USER_CANCEL'
    $true
}

# Waits until the base room is ready for input (NavBar idle), skipping its entry transition unless -Watch.
function Wait-FLRoomReady([switch]$Watch, [int]$TimeoutSec = 30) {
    $st = @{ skipped = [bool]$Watch }   # state shared with the condition block (it runs in its own scope)
    $null = Wait-FLUntil {
        if (Close-FLBlocker) { return $false }   # mission popups on entering a room (Act_PopupDialog)
        if ((((Send-FL 'hotspots') -split "`n")[-1]) -match 'hotspots [1-9]\d* ready$') { return $true }
        # A mission offer opened on arrival (e.g. M01a's in the Equipment room): the room waits for the test's decision.
        if ((Send-FL 'visible OfferDialogAcceptButton') -match '^visible') { return $true }
        if (-not $st.skipped) { $st.skipped = Skip-FLAnimation }
        $false
    } 'room ready' $TimeoutSec
}

# Launches from the base the player is in (the Launch button, natively) and waits until in space. The base's launch
# sequence is skipped unless -Watch. Throws if the story does not allow launching (no Launch button).
function Start-FLLaunch([switch]$Watch, [int]$TimeoutSec = 60) {
    # A room scene may still run (Pittsburgh after M01a's landing, 18 Oct 2026: room buttons refused, "busy: scene").
    Wait-FLRoomReady -Watch:$Watch
    $hs = Send-FL 'hotspots'
    if ($hs -notmatch 'IDS_HOTSPOT_LAUNCH') {
        # The launch area: Cityscape (Manhattan), Planetscape (Pittsburgh), Deck (stations) - whichever the base has.
        $area = @('Cityscape', 'Planetscape', 'Deck') | Where-Object { $hs -match "(?m)^$_\s" } | Select-Object -First 1
        if (-not $area) { throw "no launch area among this room's buttons: $hs" }
        $null = Goto-FLRoom $area -Watch:$Watch
        if ((Send-FL 'hotspots') -notmatch 'IDS_HOTSPOT_LAUNCH') { throw 'no Launch button: launching is not allowed here now (story state?)' }
    }
    Wait-FLRoomReady -Watch:$Watch
    $r = Send-FL 'goto launch'; if ($r -notmatch '^clicked') { throw "launch: $r" }
    $st = @{ skipped = [bool]$Watch }
    # Out of the base (the briefing a mission shows on launch is closed natively), then in flight: the launch's docking
    # camera is skipped unless -Watch; a mission's story cutscene right after the launch is waited out (Wait-FLSpace,
    # its time does not count against -TimeoutSec).
    $null = Wait-FLUntil {
        if (Close-FLBlocker) { return $null }
        $m = Get-FLMode
        if ($m -ne 'docked') { return $m }
        if (-not $st.skipped) { $st.skipped = Skip-FLAnimation }
        $null
    } 'left the base' $TimeoutSec
    if (-not $Watch) { $null = Skip-FLAnimation }
    if (-not (Wait-FLSpace $TimeoutSec)) { throw "launched, but not in flight: $(Get-FLStoryState)" }
    'space'
}

# Plays a base's story out the way a player does, then launches: story talks (`talk_story`) are taken, room scenes and
# animations skipped (watched with -Watch), popups closed natively; once the base offers no story talk and launching is
# allowed (Start-FLLaunch finds the launch area), it launches. -Until (e.g. the mission's result) ends it early. An open
# job offer is the caller's decision: it throws. Returns 'space' or 'until'. Mission bases whose scenes chain through
# rooms (M05 Sprague: Planetscape -> Digsite -> Planetscape2 -> Spaceflight) need no per-mission room list.
# Repair at a base the way the player does (user, 20 Oct 2026: M04's ship flew on damaged - nothing repaired at base
# stops; the 2020 playthrough repairs at bases, "DAMAGED ITEMS" window): the room hotspot "Repair your ship"
# (IDS_NN_REPAIR_YOUR_SHIP, behaviour 12; in the ship dealer and equipment rooms) opens the game's repair window, every
# damaged item already ticked; RepairOkButton repairs them (the game charges: ReqChangeCash, ReqHullStatus,
# ReqEquipment - verified 20 Oct 2026 on a fight-damaged ship: -385 credits, assets back to full). No damaged item = the
# window is closed untouched. Returns "repaired <credits>", "nothing to repair", or why not. Native clicks only.
function Invoke-FLBaseRepair([int]$TimeoutSec = 15) {
    if ((Get-FLMode) -ne 'docked') { return 'not docked' }
    $has = { (Send-FL 'hotspots') -match 'IDS_NN_REPAIR_YOUR_SHIP' }
    if (-not (& $has)) {
        foreach ($room in 'ShipDealer', 'Equipment') {
            if ((Send-FL 'hotspots') -match "(?m)^$room\s") { $null = Goto-FLRoom $room; $null = Wait-FLRoomReady; if (& $has) { break } }
        }
    }
    if (-not (& $has)) { return 'no repair here' }
    # the room's NavBar ignores clicks while busy (right after a window closed: a second repair visit answered "busy:
    # navbar+34C", 21 Oct 2026) - wait for it as every room click does
    $null = Wait-FLRoomReady -TimeoutSec $TimeoutSec
    $cash0 = [int][regex]::Match((Send-FL 'worth'), 'cash=(-?\d+)').Groups[1].Value
    $r = Send-FL 'goto REPAIR'
    if ($r -notmatch '^clicked') { return "repair button: $r" }
    # the window: its OK button with item rows, or the toggle alone (nothing damaged)
    $state = Wait-FLUntil {
        $ui = Send-FL 'uiitems'
        if ($ui -match '(?m)^\s*RepairOkButton\s' -and $ui -match '(?m)^\s*RepairCheckBox0\s') { return 'items' }
        if ($ui -match '(?m)^\s*RepairToggleChecks\s' -and $ui -notmatch '(?m)^\s*RepairCheckBox0\s') { return 'none' }
        $null
    } 'the repair window' $TimeoutSec
    if ($state -eq 'none') { $null = Send-FL 'click RepairCancelButton'; return 'nothing to repair' }
    $null = Send-FL 'click RepairOkButton'
    $paid = Wait-FLUntil { $c = [int][regex]::Match((Send-FL 'worth'), 'cash=(-?\d+)').Groups[1].Value; if ($c -ne $cash0) { $cash0 - $c } } 'the repair charge' 5
    if ((Send-FL 'uiitems') -match '(?m)^\s*RepairCancelButton\s') { $null = Send-FL 'click RepairCancelButton' }
    "repaired $paid"
}

# Restock nanobots and shield batteries at a base the way the player does (user, 20 Oct 2026: M04 entered Magellan with
# all 3 nanobots spent): the Equipment room's dealer (IDS_HOTSPOT_EQUIPMENTDEALER), filter Cargo Bay, each row's good
# read through the game's own lookup (debug `dealer`: the dealer list's selected good, exe::DealerSelectedGood), the
# row's QuantitySlider set natively (debug `scroll`: the slider's range is 1 .. what fits; its position = the
# quantity) and BuySellTradeButton clicked - the game then sends GFGoodBuy, ReqAddItem, ReqChangeCash itself (verified
# 20 Oct 2026, Manhattan: 3 + 3 for 600 + 600 credits). Any base: the filters are searched Cargo Bay first, then the
# others, each list scrolled to its end (debug `scroll Inventory_Scroll`), so no dealer layout is assumed.
# Budget (user: consumables by the player's current resources): a purchase moves cash into ship assets 1:1 (verified:
# cash -2800, assets +2800), so net worth - the story gates - is unchanged; the cost is liquidity only. It fills what
# fits but keeps -Reserve credits in cash (Complete-FLBaseStory passes the repair just paid: enough for another one).
# Offensive / defensive consumables too (user, 20 Oct 2026: ammo restock affects every later test): the ammo of the
# MOUNTED launchers only (user: "ammo that's being used by mounted equipment") - the ship's equipment list (debug
# `equipment`: the server's own list, docked too) gives the mounted launchers, the game data gives each one's munition
# (projectile_archetype of its [Gun] / [MineDropper] / [CounterMeasureDropper]); those Ammo-list rows are filled the
# same way (the dealer's slider = what fits). Ammo carried for a launcher that is not mounted is left alone.
# The user's play (kb consumables.md "Restock at stops vs loot"): at stops they filled the ammo they used to full.
# -NoAmmo skips it.
$script:FLLauncherAmmo = $null
function Get-FLMountedAmmo {   # ammo nicknames of the mounted launchers (lower case), from `equipment` + the game data
    if (-not $script:FLLauncherAmmo) {
        $script:FLLauncherAmmo = @{}
        foreach ($f in Get-ChildItem (Join-Path (Get-FLInstallDir) 'DATA\EQUIPMENT') -Filter '*equip*.ini') {
            $cur = $null
            foreach ($l in Get-Content -LiteralPath $f.FullName) {
                if ($l -match '^\s*\[') { $cur = $null }
                elseif ($l -match '^\s*nickname\s*=\s*(\S+)') { $cur = $Matches[1].ToLower() }
                elseif ($cur -and $l -match '^\s*projectile_archetype\s*=\s*(\S+)') {
                    $script:FLLauncherAmmo[[uint32](Get-FLHash $cur)] = $Matches[1].ToLower()
                }
            }
        }
    }
    $ammo = @()
    foreach ($l in (Send-FL 'equipment') -split "`n") {
        if ($l -match 'arch=(\d+) .*mounted=1' -and $script:FLLauncherAmmo.ContainsKey([uint32]$Matches[1])) {
            # energy guns name a projectile too (their bolt, not a good): only munitions sold as goods are ammo
            $a = $script:FLLauncherAmmo[[uint32]$Matches[1]]
            if ((Get-FLGoodPrice $a) -gt 0) { $ammo += $a }
        }
    }
    $ammo | Sort-Object -Unique
}
# Returns "restocked <good> <n> (<cr>) ..." with the credits spent, or why not.
$script:FLGoodPrices = $null
function Get-FLGoodPrice([string]$Nickname) {
    if (-not $script:FLGoodPrices) {
        $script:FLGoodPrices = @{}
        foreach ($f in Get-ChildItem (Join-Path (Get-FLInstallDir) 'DATA\EQUIPMENT') -Filter '*good*.ini') {
            $cur = $null
            foreach ($l in Get-Content -LiteralPath $f.FullName) {
                if ($l -match '^\s*\[') { $cur = $null }
                elseif ($l -match '^\s*nickname\s*=\s*(\S+)') { $cur = $Matches[1].ToLower() }
                elseif ($cur -and $l -match '^\s*price\s*=\s*([\d.]+)') { $script:FLGoodPrices[$cur] = [double]$Matches[1] }
            }
        }
    }
    [double]$script:FLGoodPrices[$Nickname.ToLower()]
}
function Invoke-FLBaseRestock([string[]]$Goods = @('ge_s_repair_01', 'ge_s_battery_01'), [int]$Reserve = 0,
                              [switch]$NoAmmo, [int]$TimeoutSec = 20) {
    if ((Get-FLMode) -ne 'docked') { return 'not docked' }
    # the exact hotspot: every room's NavBar lists the room button IDS_HOTSPOT_EQUIPMENTDEALER_ROOM too (a substring
    # match took that for the dealer and never changed rooms - base-restock FAIL 20 Oct 2026)
    $has = { (Send-FL 'hotspots') -match '(?m)^IDS_HOTSPOT_EQUIPMENTDEALER\s' }
    if (-not (& $has)) {
        if ((Send-FL 'hotspots') -match '(?m)^Equipment\s') { $null = Goto-FLRoom Equipment; $null = Wait-FLRoomReady }
        if (-not (& $has)) { return 'no equipment dealer here' }
    }
    $cash = { [int][regex]::Match((Send-FL 'worth'), 'cash=(-?\d+)').Groups[1].Value }
    $cash0 = & $cash
    # the room must be ready: right after the repair window the NavBar is still in its transition and ignores the
    # click ("dealer: busy: navbar+34C" at Mactan, 20 Oct 2026) - wait for the state, then retry while it says busy
    $null = Wait-FLRoomReady
    $r = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_EQUIPMENTDEALER'; if ($x -notmatch 'busy') { $x } } 'the dealer click (room not busy)' 10
    if ($r -notmatch '^clicked') { return "dealer: $r" }
    $null = Wait-FLUntil { (Send-FL 'visible Filter_CargoBay') -match '^visible' } 'the dealer window' $TimeoutSec
    $want = @{}
    $ammoGoods = if ($NoAmmo) { @() } else { @(Get-FLMountedAmmo) }
    foreach ($g in @($Goods) + $ammoGoods) { $want[[uint32](Get-FLHash $g.ToLower())] = @{ name = $g; price = Get-FLGoodPrice $g } }
    $done = @()
    # one list row: select it (state, not timing: the dealer list's selection is this row - the visible row plus the
    # scroll offset), and buy if it is a wanted good. After a purchase the dealer clears the selection a moment later:
    # a row clicked before that was deselected again and the buy did nothing (20 Oct 2026) -> wait for the clear.
    $visit = {
        param([int]$row, [int]$off)
        $null = Send-FL "click ItemButton$row"
        $idx = $row - 1 + $off
        $sel = { if ((Send-FL "dealer ItemButton$row") -match "(?m)^d1 .*sel $idx good (\d+)") { [uint32]$Matches[1] } }
        # a click can go unanswered (M02 chain 22 Oct 2026: row 2 right after a full row 1 - the whole Cargo Bay filter
        # was dropped): once more after 1.5 s
        $g = $null
        try { $g = [uint32](Wait-FLUntil $sel "row $idx selected" 1.5) } catch { $null = Send-FL "click ItemButton$row"; $g = [uint32](Wait-FLUntil $sel "row $idx selected" 3) }
        if (-not $want.ContainsKey($g)) { return }
        $w = $want[$g]; $want.Remove($g)
        # a good the ship cannot take more of (hold / capacity full) gets no QuantitySlider and no BuySellTradeButton: the
        # slider comes with the selection, so 3 game frames after it a missing slider means full (was: a 2 s wait per
        # full item - 12 s of every M10 base stop, 22 Oct 2026)
        $f0 = Get-FLFrame
        $up = $false
        try { $up = [bool](Wait-FLUntil { (Send-FL 'visible QuantitySlider') -match '^visible' -or (Get-FLFrame) -ge $f0 + 3 } "the $($w.name) quantity slider" 2) } catch { }
        $up = (Send-FL 'visible QuantitySlider') -match '^visible'
        if (-not $up) { $script:restockDone += "$($w.name) 0 (full)"; return }
        # the slider's position is the quantity; its top position (range end - one page) = what fits
        $s = Send-FL 'scroll QuantitySlider 1000'
        if ($s -notmatch 'pos=([\d.]+)') { $script:restockDone += "$($w.name) none ($s)"; return }
        $fits = [int][double]$Matches[1]
        $n = $fits
        if ($w.price -gt 0) { $n = [math]::Min($n, [math]::Floor(((& $cash) - $Reserve) / $w.price)) }
        if ($n -lt 1) { $script:restockDone += "$($w.name) 0 (fits $fits, cash reserve $Reserve)"; return }
        $null = Send-FL "scroll QuantitySlider $($n - $fits)"
        $c0 = & $cash
        $null = Send-FL 'click BuySellTradeButton'
        $paid = Wait-FLUntil { $c = & $cash; if ($c -ne $c0) { $c0 - $c } } "the $($w.name) charge" 5
        $null = Wait-FLUntil { (Send-FL 'visible QuantitySlider') -notmatch '^visible' } 'the dealer clearing its selection' 5
        $script:restockDone += "$($w.name) $n ($paid cr)"
    }
    $script:restockDone = @()
    foreach ($filter in 'Filter_CargoBay', 'Filter_Ammo', 'Filter_Equipment', 'Filter_Weapons') {
        if (-not $want.Count) { break }
        $null = Send-FL "click $filter"
        # an empty list shows no rows; a filled one shows ItemButton1 at once
        $rows = $false
        try { $rows = [bool](Wait-FLUntil { (Send-FL 'visible ItemButton1') -match '^visible' } "the $filter list" 2) } catch { }
        if (-not $rows) { continue }
        $null = Send-FL 'scroll Inventory_Scroll -1000'   # top (lists without a scroll bar fit on one page)
        try {
            for ($row = 1; $row -le 5 -and $want.Count; $row++) {
                if ((Send-FL "visible ItemButton$row") -notmatch '^visible') { break }
                & $visit $row 0
            }
            # further rows: one row down at a time, the new one appears as row 5; ends where the bar stops moving
            $off = 0
            while ($want.Count) {
                $s = Send-FL 'scroll Inventory_Scroll 1'
                if ($s -notmatch 'pos=([\d.]+)' -or [int][double]$Matches[1] -le $off) { break }
                $off = [int][double]$Matches[1]
                & $visit 5 $off
                # a purchase can rebuild the list: re-read where the bar is
                $p = Send-FL 'scroll Inventory_Scroll 0'
                if ($p -match 'pos=([\d.]+)') { $off = [int][double]$Matches[1] }
            }
        } catch { $script:restockDone += "(${filter}: $_)" }   # a row that never selects (empty / changing list): next filter
    }
    foreach ($w in $want.Values) { $script:restockDone += "$($w.name) not sold here" }
    $done = $script:restockDone
    if ((Send-FL 'visible Inventory_Close') -match '^visible') { $null = Send-FL 'click Inventory_Close' }
    if (-not $done.Count) { return 'nothing to restock here' }
    "restocked $($done -join ', '); spent $($cash0 - (& $cash))"
}

# The room the current objective names, from the player's information (the objective text: "Meet Tobias in the Leeds
# Equipment Shop", "Meet Juni in the Manhattan Bar"): Bar / Equipment / Trader / ShipDealer, or '' (22 Oct 2026, the
# generic leg driver: M04 / M05 end on entering Leeds Equipment, which a launch-first base stop never reaches).
function Get-FLObjectiveRoom {
    $o = @(Get-FLEvents | Where-Object { $_ -match 'objective .*obj=' } | Select-Object -Last 1)
    if (-not $o.Count) { return '' }
    $t = "$($o[0])"
    if ($t -match '(?i)\bbar\b') { return 'Bar' }
    if ($t -match '(?i)equipment') { return 'Equipment' }
    if ($t -match '(?i)commodit|trader') { return 'Trader' }
    if ($t -match '(?i)ship ?dealer') { return 'ShipDealer' }
    ''
}
# One step of a room tour when the story keeps the player in the base (no Launch button, no story talk): the next room
# of this base not visited yet (a room scene or talk may wait there), as a player looks around. Returns the room or ''.
function Step-FLRoomTour([hashtable]$Visited) {
    $labels = @((Send-FL 'hotspots') -split "`n" | ForEach-Object { ($_ -split '\s+')[0] } |
                Where-Object { $_ -in 'Bar', 'Equipment', 'Trader', 'ShipDealer', 'Cityscape', 'Planetscape', 'Deck' })
    $next = $labels | Where-Object { -not $Visited.ContainsKey($_) } | Select-Object -First 1
    if (-not $next) { $Visited.Clear(); return '' }
    $Visited[$next] = $true
    try { $null = Goto-FLRoom $next; return $next } catch { return '' }
}

# Buys commodities at this base's commodity trader the way the player does (22 Oct 2026, the money layer of the speedrun
# plan, docs\LEGS.md): the Trader room's hotspot IDS_HOTSPOT_COMMODITYTRADER opens the same dealer list as the equipment
# dealer (no filters; verified Manhattan: 8 rows), each row's good read natively (debug `dealer`), the QuantitySlider set,
# BuySellTradeButton. -Want: nickname -> units (0 = as many as fit and the cash allows: the slider's range). Net worth:
# cargo counts at its goods.ini price (kb economy.md), so a buy below that price raises it at once. Returns what it did.
function Invoke-FLTraderBuy([hashtable]$Want, [int]$TimeoutSec = 20) {
    if ((Get-FLMode) -ne 'docked') { return 'not docked' }
    $has = { (Send-FL 'hotspots') -match '(?m)^IDS_HOTSPOT_COMMODITYTRADER\s' }
    if (-not (& $has)) {
        if ((Send-FL 'hotspots') -match '(?m)^Trader\s') { $null = Goto-FLRoom Trader; $null = Wait-FLRoomReady }
        if (-not (& $has)) { return 'no commodity trader here' }
    }
    $cash = { [int][regex]::Match((Send-FL 'worth'), 'cash=(-?\d+)').Groups[1].Value }
    $null = Wait-FLRoomReady
    $r = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_COMMODITYTRADER'; if ($x -notmatch 'busy') { $x } } 'the trader click (room not busy)' 10
    if ($r -notmatch '^clicked') { return "trader: $r" }
    $null = Wait-FLUntil { (Send-FL 'visible ItemButton1') -match '^visible' } 'the trader list' $TimeoutSec
    # (not $want: PowerShell names ignore case - `$want = @{}` emptied the -Want parameter and nothing was ever bought)
    $todo = @{}
    foreach ($k in $Want.Keys) { $todo[[uint32](Get-FLHash $k.ToLower())] = @{ name = $k; units = [int]$Want[$k]; price = 0 } }
    $done = [Collections.Generic.List[string]]::new()
    $visit = {
        param([int]$row, [int]$off)
        $null = Send-FL "click ItemButton$row"
        $idx = $row - 1 + $off
        $g = [uint32](Wait-FLUntil { if ((Send-FL "dealer ItemButton$row") -match "(?m)^d1 .*sel $idx good (\d+)") { [uint32]$Matches[1] } } "row $idx selected" 5)
        if (-not $todo.ContainsKey($g)) { return }
        $w = $todo[$g]; $todo.Remove($g)
        $up = $false
        try { $up = [bool](Wait-FLUntil { (Send-FL 'visible QuantitySlider') -match '^visible' } "the $($w.name) quantity slider" 2) } catch { }
        if (-not $up) { $done.Add("$($w.name) 0 (hold full / not sold)"); return }
        $s = Send-FL 'scroll QuantitySlider 1000'
        if ($s -notmatch 'pos=([\d.]+)') { $done.Add("$($w.name) none ($s)"); return }
        $fits = [int][double]$Matches[1]   # the slider's top = what fits and the cash allows
        $n = if ($w.units -gt 0) { [math]::Min($w.units, $fits) } else { $fits }
        if ($n -lt 1) { $done.Add("$($w.name) 0"); return }
        $null = Send-FL "scroll QuantitySlider $($n - $fits)"
        $c0 = & $cash
        $null = Send-FL 'click BuySellTradeButton'
        $paid = Wait-FLUntil { $c = & $cash; if ($c -ne $c0) { $c0 - $c } } "the $($w.name) charge" 5
        # the equipment dealer clears its selection ~0.5 s after a purchase; the commodity trader keeps it (22 Oct 2026)
        try { $null = Wait-FLUntil { (Send-FL 'visible QuantitySlider') -notmatch '^visible' } 'the trader clearing its selection' 2 } catch { }
        $done.Add("$($w.name) $n ($paid cr)")
    }
    $null = Send-FL 'scroll Inventory_Scroll -1000'
    try {
        for ($row = 1; $row -le 5 -and $todo.Count; $row++) {
            if ((Send-FL "visible ItemButton$row") -notmatch '^visible') { break }
            & $visit $row 0
        }
        $off = 0
        while ($todo.Count) {
            $s = Send-FL 'scroll Inventory_Scroll 1'
            if ($s -notmatch 'pos=([\d.]+)' -or [int][double]$Matches[1] -le $off) { break }
            $off = [int][double]$Matches[1]
            & $visit 5 $off
            $p = Send-FL 'scroll Inventory_Scroll 0'
            if ($p -match 'pos=([\d.]+)') { $off = [int][double]$Matches[1] }
        }
    } catch { $done.Add("(trader: $_)") }
    foreach ($w in $todo.Values) { $done.Add("$($w.name) not sold here") }
    if ((Send-FL 'visible Inventory_Close') -match '^visible') { $null = Send-FL 'click Inventory_Close' }
    "bought $($done -join ', ')"
}

# Sells the ship's commodities at this base's commodity trader, natively (verified 22 Oct 2026, Manhattan): the window
# has two lists with the same row names - the dealer's (NN_Dealer, ItemButton1..) and the ship's cargo (NN_Inventory,
# ItemButton1..#2: the second control of that name, core\game\room.cpp FindVisible "#n"). A cargo row clicked shows
# QuantitySlider (top = all units) and BuySellTradeButton. Every cargo row is sold whole, one after another, until the
# list is empty or -MaxRows. Returns "sold <rows> rows for <credits>".
function Invoke-FLTraderSell([int]$MaxRows = 20, [int]$TimeoutSec = 20) {
    if ((Get-FLMode) -ne 'docked') { return 'not docked' }
    $has = { (Send-FL 'hotspots') -match '(?m)^IDS_HOTSPOT_COMMODITYTRADER\s' }
    if (-not (& $has)) {
        if ((Send-FL 'hotspots') -match '(?m)^Trader\s') { $null = Goto-FLRoom Trader; $null = Wait-FLRoomReady }
        if (-not (& $has)) { return 'no commodity trader here' }
    }
    $cash = { [int][regex]::Match((Send-FL 'worth'), 'cash=(-?\d+)').Groups[1].Value }
    $null = Wait-FLRoomReady
    if ((Send-FL 'visible ItemButton1') -notmatch '^visible') {
        $r = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_COMMODITYTRADER'; if ($x -notmatch 'busy') { $x } } 'the trader click (room not busy)' 10
        if ($r -notmatch '^clicked') { return "trader: $r" }
        $null = Wait-FLUntil { (Send-FL 'visible ItemButton1') -match '^visible' } 'the trader list' $TimeoutSec
    }
    $c0 = & $cash; $rows = 0
    # Measured 22 Oct 2026: a sale pays and clears the selection at once; the cargo list is rebuilt ~0.7 s later (a
    # click before that lands on the sold row's stale control) - the visible cargo row count dropping is the sign. An
    # empty list keeps one (empty) row: a row that selects nothing ends the selling.
    $count = { $n = 0; foreach ($i in 1..5) { if ((Send-FL "visible ItemButton$i#2") -match '^visible') { $n++ } }; $n }
    try {
        while ($rows -lt $MaxRows -and (Send-FL 'visible ItemButton1#2') -match '^visible') {
            $n0 = & $count
            $null = Send-FL 'click ItemButton1#2'
            $sel = $false
            try { $sel = [bool](Wait-FLUntil { (Send-FL 'visible BuySellTradeButton') -match '^visible' } 'the cargo row selected' 2) } catch { }
            if (-not $sel) { break }
            $null = Send-FL 'scroll QuantitySlider 1000'
            $before = & $cash
            $null = Send-FL 'click BuySellTradeButton'
            $null = Wait-FLUntil { (& $cash) -ne $before } 'the sale paid' 5
            $rows++
            if ($n0 -le 1) { break }   # that was the last row
            try { $null = Wait-FLUntil { (& $count) -lt $n0 } 'the cargo list rebuilt' 3 } catch { }
        }
    } catch { "(trader sell: $_)" }
    if ((Send-FL 'visible Inventory_Close') -match '^visible') { $null = Send-FL 'click Inventory_Close' }
    "sold $rows rows for $((& $cash) - $c0)"
}

function Complete-FLBaseStory([scriptblock]$Until = { $false }, [switch]$Watch, [int]$TimeoutSec = 600, [switch]$NoRepair,
                              [switch]$NoRestock, [switch]$AcceptStory) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $last = ''
    $repaired = $NoRepair.IsPresent
    $toured = @{}
    $objRoom = ''   # the objective room already visited (once per objective room)
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        if (& $Until) { return 'until' }
        if (Close-FLBlocker) { continue }
        $m = Get-FLMode
        if ($m -in 'space', 'space-ui', 'cutscene') { if (Wait-FLSpace 60) { return 'space' }; continue }
        if ($m -ne 'docked') { Start-Sleep -Milliseconds 200; continue }
        # an open job offer keeps the NavBar busy (`busy: ui-flag`): judged before the busy branch, which otherwise
        # loops for the whole timeout (21 Oct 2026: Juni's M01 offer, 600 s)
        if ((Send-FL 'visible OfferDialogAcceptButton') -match '^visible') {
            # -AcceptStory (the campaign run): a story character's offer (talk_story / an autoplay offer scene; the job
            # board is never opened here) is the story going on - accept at once
            if ($AcceptStory) {
                Write-Host ("{0:HH:mm:ss} base: story offer accepted" -f (Get-Date))
                $null = Send-FL 'click OfferDialogAcceptButton'; Start-Sleep -Milliseconds 300; continue
            }
            throw "a job offer is open: the caller decides ($(Get-FLStoryState))"
        }
        if ((((Send-FL 'hotspots') -split "`n")[-1]) -match 'busy') {
            if (-not $Watch) { $null = Skip-FLAnimation }
            Start-Sleep -Milliseconds 200; continue
        }
        if ((Send-FL 'talkers') -match 'talk_story') { $null = Send-FL 'talk story'; Start-Sleep -Milliseconds 500; continue }
        $want = Get-FLObjectiveRoom
        if ($want -and $want -ne $objRoom -and (Send-FL 'hotspots') -match "(?m)^$want\s") {
            $objRoom = $want
            Write-Host ("{0:HH:mm:ss} base: the objective's room {1}" -f (Get-Date), $want)
            try { $null = Goto-FLRoom $want -Watch:$Watch } catch { Write-Host "objective room ${want}: $_" }
            continue
        }
        if (-not $repaired) {   # once per stop, after the story talks, before the launch (the player's order)
            $repaired = $true
            $rep = ''
            try { $rep = Invoke-FLBaseRepair; Write-Host ("{0:HH:mm:ss} base repair: {1}" -f (Get-Date), $rep) } catch { Write-Host "base repair failed: $_" }
            if (-not $NoRestock) {   # then the consumables, as the player does; cash for another repair like this one stays
                $keep = if ("$rep" -match '^repaired (\d+)') { [int]$Matches[1] } else { 0 }
                try { $rs = Invoke-FLBaseRestock -Reserve $keep; Write-Host ("{0:HH:mm:ss} base restock: {1}" -f (Get-Date), $rs) } catch { Write-Host "base restock failed: $_" }
            }
            continue
        }
        try { return Start-FLLaunch -Watch:$Watch } catch {
            $last = "$_"
            # launching not allowed: the story wants something in this base first - look around the rooms
            $r = if ($last -match 'no Launch button|no launch area') { Step-FLRoomTour $toured } else { '' }
            if ($r) { Write-Host ("{0:HH:mm:ss} base: no launch yet, room tour -> {1}" -f (Get-Date), $r) } else { Start-Sleep 1 }
        }
    }
    throw "the base story did not end in a launch after $TimeoutSec s: $(Get-FLStoryState); last launch try: $last"
}

# Waits until the game is in flight, closing tutorial popups natively (their OK button MMOkButton, the game's own
# click logic, no cursor). A story cutscene in space (mission scripting, mode `cutscene`) is waited out: it cannot be
# skipped, and its time does not count against -TimeoutSec (-MaxCutsceneSec caps it). Returns $true in space, $false
# on timeout.
function Wait-FLSpace([int]$TimeoutSec = 30, [int]$MaxCutsceneSec = 300) {
    $st = @{ cut = [Diagnostics.Stopwatch]::new() }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        if (-not (Close-FLBlocker)) {
            $m = Get-FLMode
            if ($m -in 'space', 'space-ui') { return $true }
            if ($m -eq 'cutscene') { $st.cut.Start() } else { $st.cut.Stop() }
        }
        if ($st.cut.Elapsed.TotalSeconds -gt $MaxCutsceneSec) { return $false }
        if ($sw.Elapsed.TotalSeconds - $st.cut.Elapsed.TotalSeconds -gt $TimeoutSec) { return $false }
        Start-Sleep -Milliseconds 50
    }
}
