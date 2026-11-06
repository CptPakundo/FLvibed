# In-game test, GitHub issues 34 and 36: buying a ship on the pad by an A hold on BUY SHIP, LB / RB reaching it, and the right stick moving only the list under the focus or
# cursor. The script form of tests\ship_buy_pad_game.md (steps 1-20; read it first), plus one section the md lacks: a held A counts for nothing
# on a new screen (dealer_rules.h NewScreen), which could eat a real hold.
# Background games on a numbered instance, muted (deploy.ps1 -Background), driven by game state and events; frames and logs in research\experience\runs\ship_buy_<Label>\
# (git-ignored). Never run while another session holds a test game: it starts its own.
# Usage (PowerShell tool, from the worktree, only when the driver says the games are free):
#   tests\ship_buy_pad.ps1 -Label <name> [-Only hold,buy,stick,kinds] [-Sizes '3840, 2160','1280, 800'] [-Skins 1,0] [-Instance 7]
# Sections, in the order they run in a game:
#   hold   a held A across a screen change acts on nothing, a fresh hold there acts: equipment dealer (window -> room -> window), first ship stage -> final stage (the game's own
#          change, under the hold that caused it), job board (room -> window). First size and skin only: the rule has no size or skin branch. It buys and takes a job.
#   buy    steps 1-13. Step 13 runs before 9: it wants the old ship still there. Step 9 buys with nothing transferred, so the game's sum can be checked exactly; step 12 buys with
#          items transferred, and shows what was sold and what moved.
#   stick  steps 14-20 at the commodity and equipment dealers (ids 14c, 14e ...). Step 18 runs first: its list must still fit.
#   kinds  step 1 and 2 again on a PlayStation test pad through SDL (the hold has no pad-kind branch: the md asks no more). First size and skin only.
#   kbm    the page's keyboard and mouse section by native clicks: the final stage, a transfer, BUY SHIP, with no pad prompt. The wheel has no harness verb: not driven.
# Each result is PASS / FAIL / NOT RUN / LOOK (a frame a person or run-judge reads). NOT RUN is a fixture that could not be made, never a pass.
# A step that throws is a FAIL and the run goes on. Exit 1 = a FAIL, 2 = no FAIL but a NOT RUN.
# Not here: the md's regression scripts (tests\dealer_focus.ps1, window_prompts.ps1, docked_text.ps1, state_fixes.ps1 -Row rs), each its own run.
# How full the A pill stands is no debug output: judge it from the frames.
param([string]$Label = 'run', [string[]]$Only = @('hold', 'buy', 'stick', 'kinds', 'kbm'),
      [string[]]$Sizes = @('3840, 2160', '1280, 800'), [int[]]$Skins = @(1, 0), [int]$Instance = 7, [string]$Save = 'SaveC701.fl',
      [int]$Rank = 4)   # written into the copies' [Player] rank: SaveC701 has 0, Manhattan's ships want 1, 4, 1 (FREELANCER_RE "The ship dealer"), so every row is refused without it
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
$dir = Join-Path $root "research\experience\runs\ship_buy_$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$bad = [Collections.Generic.List[string]]::new()
$notRun = [Collections.Generic.List[string]]::new()
$home_ = Get-FLDisplaySize
$script:Tag = ''
$script:Kind = 'dev'     # the pad in the hands: 'dev' the device-layer test pad, 'sdl' the SDL test pad
$script:Offer = $null    # the ship row in use
$script:Poor = $false    # steps 11, 12: the too-few-credits fixture stands
$script:Dealer0 = -1     # the dealer's rows at the final stage before any transfer
$script:Equip0 = ''

# Purchases must not touch the fixture: each section that buys loads its own copy, made before the game starts (the game lists saves at its start). A new write time keeps
# deploy's overflow rule off them.
$copies = @{ hold = 'SaveB7A1.fl'; buy = 'SaveB7A2.fl'; poor = 'SaveB7A3.fl'; stick = 'SaveB7A4.fl' }
$kHold = 0.6             # dealer_rules.h kHoldSec
$kCarry = 2 * $kHold     # a carried hold is watched this long: twice what would fire
$rxRow = '^Item(View|Button)\d'
$rxShipRow = '^ShipTraderShipName(Icon)?Frame\d'
$rxBuyShip = '^Continue' # (the button or its caption over it: IsBuyShip)

# (Write-Host: a helper's return value must never carry a result line with it)
function Result([string]$kind, [string]$what) { Write-Host ("{0,-7} {1}" -f $kind, $what); if ($kind -eq 'FAIL') { $bad.Add("$($script:Tag) $what") } }
function Check([bool]$ok, [string]$what) { Result $(if ($ok) { 'PASS' } else { 'FAIL' }) $what }
function NotRun([string]$step, [string]$why) { Write-Host ("{0,-7} step {1}: {2}" -f 'NOT RUN', $step, $why); $notRun.Add("$($script:Tag) step ${step}: $why") }
function Skip([string]$why) { throw "NOTRUN: $why" }
function Tag([string]$size) { $size -replace '[ ,]+', 'x' }
function Shot([string]$name) {
    $n = "$($script:Tag)_$name"
    try { $f = Save-FLShot -Name $n -Dir $dir; Move-Item -LiteralPath $f (Join-Path $dir "$n.png") -Force; Result 'LOOK' "$n.png" }
    catch { Result 'LOOK' "$n.png not taken: $($_.Exception.Message)" }
}
# One step: its deadline, a throw is its FAIL, the pad let go, its frame. Steps after a failed one find their own way or say NOT RUN.
function Step([string]$id, [string]$what, [scriptblock]$body, [int]$sec = 90) {
    if (-not (Get-FLGameProcess)) { NotRun $id 'the game is gone'; return }
    Write-Host "--- step ${id}: $what"
    Set-FLDeadline $sec "step $id"
    try { $null = & $body }
    catch {
        $m = $_.Exception.Message
        if ($m -like 'NOTRUN: *') { NotRun $id $m.Substring(8) } else { Result 'FAIL' "step ${id}: $m" }
    } finally {
        Set-FLDeadline 0
        try { Pad; $null = Wait-FLFramesPassed 4 } catch { }
    }
    if (Get-FLGameProcess) { Shot "s$id" }
}

# ---- the pad, at the device layer (the md's fixture): the whole state each call
function Pad([string[]]$Buttons = @(), [double]$LX = 0, [double]$RY = 0) {
    if ($script:Kind -eq 'sdl') { $null = Set-FLSdlPad -Slot 0 -Buttons $Buttons -LX $LX -RY $RY } else { $null = Set-FLDevPad -Slot 0 -Buttons $Buttons -LX $LX -RY $RY }
}
function Tap([string]$button) { Pad @($button); $null = Wait-FLFramesPassed 3; Pad; $null = Wait-FLFramesPassed 8 }
function Wake-Pad {
    Pad -RY 0.6; $null = Wait-FLFramesPassed 30; Pad; $null = Wait-FLFramesPassed 30
    $null = Wait-FLUntil { if ((Field 'pad') -eq '1') { 1 } } 'the pad as the device in use' 10
}
function Dev-Pads-On {
    Enable-FLDevPads -Foreground   # (the focus gate is shut in a background game)
    Set-FLDevPad -Slot 0
    $null = Wait-FLUntil { if ((Send-FL 'state') -match ' slot=0 ') { 1 } } 'the test pad in slot 0' 10
}

# ---- readers
function Vis([string]$name) { (Send-FL "visible $name") -match '^visible' }
function Field([string]$name, [string]$text = '') { if (-not $text) { $text = "$(Send-FL 'bases')" }; [regex]::Match($text, "(?:^|\s)$name=(\S*)").Groups[1].Value }
function Lane { [regex]::Match("$(Send-FL 'bases')", "items='([^']*)'").Groups[1].Value }
function AHeld { [double](Field 'aHeld') }
function Focus { Field 'focused' }
function Cash { [int][regex]::Match((Send-FL 'worth'), 'cash=(-?\d+)').Groups[1].Value }
function Ship-Name { [regex]::Match((Send-FL 'uistrip data'), 'ship "([^"]*)" credits').Groups[1].Value }
function Equip { (@([regex]::Matches("$(Send-FL 'equipment')", 'arch=(\d+) id=\d+ count=(\d+) mounted=(\d)') | ForEach-Object { "$($_.Groups[1].Value)x$($_.Groups[2].Value)m$($_.Groups[3].Value)" } | Sort-Object)) -join ' ' }
function Read-Offer {
    $m = [regex]::Match((Send-FL 'uistrip data'), 'ship sel=(-?\d+) refusal=(\d+) level=(-?\d+) price=(-?\d+) budget=(-?\d+) credits=(-?\d+)')
    if (-not $m.Success) { return $null }
    [pscustomobject]@{ Sel = [int]$m.Groups[1].Value; Refusal = [int]$m.Groups[2].Value; Level = [int]$m.Groups[3].Value; Price = [int]$m.Groups[4].Value
                       Budget = [int]$m.Groups[5].Value; Credits = [int]$m.Groups[6].Value }
}
function Mark { Update-FLEventLog; $script:FLLogSeq }
function Lines([int]$after, [string]$rx) { @(Get-FLEventLog $after | Where-Object { "$_" -match $rx }) }
function Log-Path { Join-Path (Get-FLInstallDir) 'EXE\FLController.log' }
function Log-Mark { @(Get-Content -LiteralPath (Log-Path)).Count }
# (`pad screen` is a log line, no event)
function Screen-Log([int]$from) { (@(Get-Content -LiteralPath (Log-Path) | Select-Object -Skip $from | Where-Object { $_ -match 'pad screen ' } | ForEach-Object { ($_ -replace '^.*?pad screen ', '') })) -join ' | ' }
function NoFault([string]$where) { Check (@(Select-String -Path (Log-Path) -Pattern 'FATAL').Count -eq 0) "${where}: no fault in the log" }
function Is-Final { (Vis 'ContinueButton') -or (Vis 'BackToShipDealer') }
function Wait-Screen([string]$rx, [double]$sec = 15) { $null = Wait-FLUntil { if ((Field 'screen') -match "^($rx)$") { 1 } } "screen $rx" $sec }
function Wait-Still {   # a window's slide over: the game drops what comes during it
    $null = Wait-FLUntil { if ((((Send-FL 'hotspots') -split "`n")[-1]) -notmatch 'nn-window') { 1 } } 'the window''s slide' 8
    $null = Wait-FLFramesPassed 10
}
function Wait-Held([double]$sec) { $null = Wait-FLUntil { if ((AHeld) -ge $sec) { 1 } } "A counted $sec s on this screen" ($sec + 10) }

# The side of a control by its parent chain, as padbases.cpp SideOf: 1 under NN_Dealer, else 2. `#2` counts shown bars only, so never read the number as the side.
function Ctl([string]$name) {
    $m = [regex]::Match((Send-FL "ctlinfo $name"), '^control ([0-9A-F]{8})')
    if ($m.Success) { [Convert]::ToUInt32($m.Groups[1].Value, 16) } else { [uint32]0 }
}
function Side-Of([uint32]$ctl) {
    $dealer = Ctl 'NN_Dealer'
    if (-not $dealer) { throw 'NN_Dealer is not visible' }
    $inv = Ctl 'NN_Inventory'
    $c = $ctl
    for ($i = 0; $i -lt 12 -and $c; $i++) {
        if ($c -eq $dealer) { return 1 }
        if ($inv -and $c -eq $inv) { return 2 }
        $c = Get-FLPeek32 ($c + 4)
    }
    2
}
# Row controls per side (an empty list keeps one empty row: compare counts, do not read them as items)
function Rows {
    $r = [pscustomobject]@{ Dealer = 0; Yours = 0 }
    foreach ($m in [regex]::Matches("$(Send-FL 'uiitems')", '(?m)^ItemView\d+\s+ctl=([0-9A-F]{8})')) {
        if ((Side-Of ([Convert]::ToUInt32($m.Groups[1].Value, 16))) -eq 1) { $r.Dealer++ } else { $r.Yours++ }
    }
    $r
}
# Both list bars by side; a bar is hidden while its list fits, so a missing key means that
function Bars {
    $b = @{}
    foreach ($t in '', '#2') {
        $n = "Inventory_Scroll$t"
        $m = [regex]::Match((Send-FL "scroll $n 0"), 'pos=(-?[\d.]+) range=(-?\d+\.\d)\.\.(-?\d+\.\d)')
        if (-not $m.Success) { continue }
        $key = if ((Side-Of (Ctl $n)) -eq 1) { 'dealer' } else { 'yours' }
        if ($b.ContainsKey($key)) { Skip "both shown bars read as the $key side: the map of bars to sides is wrong" }
        $b[$key] = [pscustomobject]@{ Name = $n; Pos = [double]$m.Groups[1].Value; Hi = [double]$m.Groups[3].Value }
    }
    $b
}
function Bars-Text($b) { (@('dealer', 'yours') | ForEach-Object { if ($b[$_]) { "$_ $($b[$_].Name)=$($b[$_].Pos) of $($b[$_].Hi)" } else { "$_ hidden" } }) -join ', ' }
function Bars-Top { $b = Bars; foreach ($k in $b.Keys) { $null = Send-FL "scroll $($b[$k].Name) -1000" }; $null = Wait-FLFramesPassed 10 }
# `bases`' stickBar: the bar the stick means now, pos(lo..hi,page=p,name)
function Stick-Bar {
    $m = [regex]::Match((Field 'stickBar'), '^(-?[\d.]+)\((-?[\d.]+)\.\.(-?[\d.]+),page=(-?[\d.]+),(.*)\)$')
    if (-not $m.Success) { return $null }
    [pscustomobject]@{ Pos = [double]$m.Groups[1].Value; Lo = [double]$m.Groups[2].Value; Hi = [double]$m.Groups[3].Value; Page = [double]$m.Groups[4].Value; Name = $m.Groups[5].Value }
}

# ---- getting around
function Open-Spot([string]$room, [string]$spot, [string]$ready) {
    if ((Send-FL 'hotspots') -notmatch "(?m)^$spot\s") { $null = Goto-FLRoom $room }
    $null = Wait-FLRoomReady
    $r = Wait-FLUntil { $x = Send-FL "goto $spot"; if ($x -notmatch 'busy') { $x } } "the $spot click" 15
    if ($r -notmatch '^clicked') { throw "${spot}: $r" }
    $null = Wait-FLUntil { if (Vis $ready) { 1 } } "$ready open" 25
    Wait-Still
}
# (a debug click dropped by the game during a slide is no pad press: the harness's Close-Spot retries it the same way)
function Close-Window([string]$button) { $null = Wait-FLUntil { $null = Send-FL "click $button"; if ((Field 'screen') -eq 'room') { 1 } } "$button closed" 15; Wait-Still }
# A NeuroNet window by its key, then its box's button (one path in the game, UI_HUD2_SKIN.md). A press during a slide is lost, so a miss is tried again; only after 3 s, as a second press on an open window closes it.
# The game's own hit test at the moved controls: a click by position must find them where they are drawn (fractions of H from the screen's middle, and from the top)
function Hits([string]$screen) {
    $null = Wait-FLFramesPassed 40
    $wh = $script:HitSize -split ',\s*'; $w = [double]$wh[0]; $h = [double]$wh[1]
    $points = @{ ship = @(,@('X', 0.611, 0.259, 'ShipTraderCancelButton')); missionlog = @(@('X', 0.4075, 0.225, 'StoryStarCloseButton'), @('MISSION', -0.204, 0.826, 'MissionButton'), @('LOG', 0.2025, 0.826, 'LogButton'));
                 help = @(,@('X', 0.40, 0.225, 'PlayerInfoCloseButton')); inventory = @(@('X', 0.404, 0.3025, 'Inventory_Close'), @('row', -0.1875, 0.48, 'Item')) }
    foreach ($p in $points[$screen]) {
        $r = "$(Send-FL ("hit {0:F0} {1:F0}" -f ($w / 2 + $p[1] * $h), ($p[2] * $h)))"
        Check ($r -match $p[3]) "hit ${screen} $($p[0]): wanted $($p[3]): $r"
    }
}
function Open-NN([string]$cmd, [string]$button, [string]$screen) {
    Wait-Still
    $said = @()
    foreach ($how in "tap $cmd", "tap $cmd", "click $button") {
        $said += "$how -> $(Send-FL $how)"
        try { Wait-Screen $screen 3; Hits $screen; return } catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw } }
    }
    Skip "the window did not open: screen=$(Field 'screen'); $($said -join ' | ')"
}
function Load-Copy([string]$file) { $null = Load-FLSave $file; Wake-Pad; $script:Poor = $false }
function Walk-Focus([string]$rx, [string[]]$keys) {
    foreach ($k in $keys) { if ((Focus) -match $rx) { return $true }; Tap $k }
    (Focus) -match $rx
}
# The list on that side (1 dealer, 2 the player's) under the focus, by the side keys. At the final stage the ring has BUY SHIP as a third stop.
function Side-To([int]$side) {
    foreach ($i in 1..4) {
        $s = "$(Send-FL 'bases')"
        if ((Field 'side' $s) -eq "$side" -and (Field 'focused' $s) -match $rxRow) { return }
        Tap 'RB'
    }
    Skip "no row on side $side by RB (side=$(Field 'side') focused=$(Focus))"
}
# A ship row the game will sell, reached by the D-pad; the md's +0x3FC: a refusal is closed and reopened, never reported as a pass.
function Find-Row {
    foreach ($k in 'Down', 'Down', 'Down', 'Up', 'Up', 'Up', 'Up', 'Up', 'Up') {
        if ((Focus) -match $rxShipRow) {
            $null = Wait-FLFramesPassed 10
            $o = Read-Offer
            if (-not $o) { Tap 'A'; $null = Wait-FLFramesPassed 10; $o = Read-Offer }   # (a tap on a row selects it, no more)
            # The rank in the copy is the way; +0x3FC read 1 or 0 across openings of one save, cause unread, so a level refusal left over is cleared as FREELANCER_RE's probe did, and said
            if ($o -and $o.Refusal -eq 1) {
                $at = (Ctl 'NN_ShipTrader') + 0x3FC + 4 * $o.Sel
                $r = Send-FL ('poke 0x{0:X} 00 00 00 00' -f $at)
                Result 'LOOK' "the row's level refusal (level $($o.Level)) stood with rank $Rank in the save: +0x3FC poked to 0: $r"
                Tap 'A'; $null = Wait-FLFramesPassed 10; $o = Read-Offer
            }
            if ($o -and $o.Refusal -eq 0 -and (Vis 'ShipTraderBuyButton')) { return $o }
            $script:LastOffer = $o
        }
        Tap $k
    }
    $null
}
function Pick-Ship {
    foreach ($try in 1..3) {
        if (-not (Vis 'ShipTraderShipName0')) { Open-Spot 'ShipDealer' 'IDS_HOTSPOT_SHIPDEALER' 'ShipTraderShipName0'; Wait-Screen 'shipdealer' }
        $o = Find-Row
        if ($o) { return $o }
        Close-Window 'ShipTraderCancelButton'
    }
    Skip "no ship row without a refusal in 3 openings, with rank $Rank in the save and +0x3FC poked (last read: $($script:LastOffer))"
}
# First stage to final stage; A let go the moment the window stands, so this never holds into BUY SHIP
function To-Final {
    Pad @('A')
    try { $null = Wait-FLUntil { if (Is-Final) { 1 } } 'the final stage' 12 } finally { Pad }
    Wait-Still
    try { $null = Wait-FLUntil { if ((Focus) -match "$rxBuyShip|$rxRow") { 1 } } 'the final stage''s entry focus' 5 }
    catch { Result 'LOOK' "the entry focus: focused=$(Focus) side=$(Field 'side')"; throw }
}
function Ensure-Final {
    if (Is-Final) { return }
    try { $script:Offer = Pick-Ship; To-Final }
    catch { if ($_.Exception.Message -like 'NOTRUN: *') { throw }; Skip "the final stage not reached: $($_.Exception.Message)" }
}
# A fresh hold: A down until the line comes or the time is out; how many lines came
function Fresh-Hold([string]$rx, [double]$sec = 4) {
    $m = Mark
    Pad @('A')
    try {
        try { $null = Wait-FLUntil { if ((Lines $m $rx).Count) { 1 } } "the line $rx" $sec }
        catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw } }   # (a timeout is a result: the hold did nothing)
    } finally { Pad }
    $null = Wait-FLFramesPassed 10
    (Lines $m $rx).Count
}
# The trade-in: NN_Dealer +0x9FC, an integer (FREELANCER_RE "The ship dealer's final stage"; +0x9F8 is no offer price: it read 7900 on an offer of 9170). The price is the offer's.
function Sum-Fields([int]$price) {
    $c = Ctl 'NN_Dealer'
    if (-not $c) { return $null }
    $p = Get-FLPeek32 ($c + 0x9F8); $t = Get-FLPeek32 ($c + 0x9FC)
    Result 'LOOK' "NN_Dealer +0x9F8 $p, +0x9FC $t; the ship costs $price"
    if ($t -le 0 -or $t -ge $price) { return $null }
    [pscustomobject]@{ Price = $price; Trade = [double]$t }
}

# ---- hold: a held A across a screen change, then a fresh one on that screen
function Judge-Carry([string]$id, [int]$m, [string]$rxAct, [int]$cash0, [int]$log0) {
    Wait-Held $kCarry
    Shot "s${id}_carried"
    Check ((Lines $m $rxAct).Count -eq 0) "step ${id}: A carried in, counted $(AHeld) s here: no action (screens: $(Screen-Log $log0))"
    Check ((Cash) -eq $cash0) "step ${id}: the credits stand at $cash0 ($(Cash))"
    Pad; $null = Wait-FLFramesPassed 10
    Check ((Lines $m 'pad \S+: (row \S+ clicked|\S+ clicked)').Count -eq 0) "step ${id}: its release is no tap"
}
function Section-hold {
    Step 'H1' 'equipment dealer: A held out of the dealer, through the room, into it again' {
        Open-Spot 'Equipment' 'IDS_HOTSPOT_EQUIPMENTDEALER' 'Filter_Weapons'
        Wait-Screen 'equipment'
        # (a change of filter clears the selection and the game keeps the filter: the hold that starts here has nothing to trade, wherever the focus stands, and the reopened
        # window has a row selected. No D-pad walk: from the rows Up leads to the NeuroNet's box, the filters lie under the list)
        $null = Send-FL 'click Filter_Ammo'
        $null = Wait-FLFramesPassed 20
        $null = Send-FL 'click Filter_Weapons'
        $null = Wait-FLUntil { if (Vis 'ItemButton1') { 1 } } 'the weapons list' 6
        try { $null = Wait-FLUntil { if (-not (Vis 'BuySellTradeButton')) { 1 } } 'the row let go' 3 } catch { Skip 'a row stays selected: the hold would trade here' }
        $null = Wait-FLFramesPassed 20
        if (Vis 'BuySellTradeButton') { Skip 'a row was selected again: the hold would trade here' }
        $m = Mark; $l = Log-Mark; $c = Cash
        Pad @('A')
        $null = Wait-FLFramesPassed 6
        $null = Wait-FLUntil { $null = Send-FL 'click Inventory_Close'; if ((Field 'screen') -eq 'room') { 1 } } 'the dealer closed with A down' 15
        $null = Wait-FLRoomReady
        $r = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_EQUIPMENTDEALER'; if ($x -notmatch 'busy') { $x } } 'the dealer click with A down' 15
        if ($r -notmatch '^clicked') { throw "dealer: $r" }
        Wait-Screen 'equipment'
        Wait-Still
        try { $null = Wait-FLUntil { if (Vis 'BuySellTradeButton') { 1 } } 'the entry row selected' 6 } catch { Skip "no row selected at the entry (focused=$(Focus)): a carried hold would have nothing to buy" }
        $null = Wait-FLUntil { if ((Field 'screen') -eq 'equipment' -and (Field 'modal') -eq '0') { 1 } } 'the window standing' 3
        Judge-Carry 'H1' $m 'pad buy: ' $c $l
        $m2 = Mark; $l2 = Log-Mark
        $n = Fresh-Hold 'pad buy: clicked BuySellTradeButton'
        Check ($n -eq 1) "step H1: a fresh hold on the same screen buys: $n lines (focused=$(Focus); screens during it: '$(Screen-Log $l2)')"
        if ($n) { $null = Wait-FLUntil { if ((Cash) -ne $c) { 1 } } 'the charge' 5 }
        Check ((Cash) -lt $c) "step H1: the credits fell $c -> $(Cash)"
        Close-Window 'Inventory_Close'
    } 150
    Step 'H2' 'first ship stage to final stage: the hold that buys runs on, then a fresh hold on BUY SHIP' {
        $o = Pick-Ship
        $ship0 = Ship-Name; $c = Cash; $m = Mark; $l = Log-Mark
        Pad @('A')
        $null = Wait-FLUntil { if ((Lines $m 'pad ship buy: clicked ShipTraderBuyButton').Count) { 1 } } 'the first stage''s hold' 6
        $null = Wait-FLUntil { if (Is-Final) { 1 } } 'the final stage under the hold' 12
        $null = Wait-FLUntil { if ((Field 'screen') -match '^(equipment|commodity)$') { 1 } } 'the final stage as a pad screen' 5
        Judge-Carry 'H2' $m 'pad ship trade: ' $c $l
        Check ((Ship-Name) -eq $ship0) "step H2: the ship stands: $ship0"
        Wait-Still
        if (-not ((Focus) -match $rxBuyShip)) { Skip "the focus is not on BUY SHIP: focused=$(Focus), so a hold here would not buy" }
        $l2 = Log-Mark
        $n = Fresh-Hold 'pad ship trade: clicked ContinueButton'
        Check ($n -eq 1) "step H2: a fresh hold on BUY SHIP buys: $n lines (screens: '$(Screen-Log $l2)')"
        if ($n) { $null = Wait-Screen 'room' 20 }
        Check ((Ship-Name) -ne $ship0) "step H2: the ship is now '$(Ship-Name)', was '$ship0' (price $($o.Price))"
    } 150
    Step 'H3' 'job board: A held in from the room' {
        $null = Goto-FLRoom 'Bar'
        $null = Wait-FLRoomReady
        $null = Wait-FLUntil { if ((Field 'screen') -eq 'room') { 1 } } 'the room as a pad screen' 5
        # (a press on anyone but the player's own character would talk: padbases UpdateRoom)
        $ok = $false
        foreach ($i in 1..8) {
            if ((Send-FL 'bases chars') -match '(?m)^\* .* player\s*$') { $ok = $true; break }
            Tap 'Right'; $null = Wait-FLFramesPassed 10
        }
        if (-not $ok) { Skip "the focus never stood on the player's own character: A would talk: $(Send-FL 'bases chars')" }
        $m = Mark; $l = Log-Mark; $c = Cash
        Pad @('A')
        $null = Wait-FLFramesPassed 6
        Check ((Lines $m 'pad bar: talk').Count -eq 0) 'step H3: A in the room talked to nobody'
        $r = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_MISSIONVENDOR'; if ($x -notmatch 'busy') { $x } } 'the job board click with A down' 15
        if ($r -notmatch '^clicked') { throw "job board: $r" }
        Wait-Screen 'jobboard'
        Wait-Still
        if (-not (Vis 'MissionVendorSummaryBox0')) { Skip 'no job on the board' }
        # (the row by a native click, as Start-FLJob: the hold accepts whatever the focus is on, and a D-pad walk here ended on the NeuroNet's box. A stays down through it)
        $r = Send-FL 'click MissionVendorSummaryBox0'
        if ($r -notmatch '^clicked') { Skip "the job row: $r" }
        $null = Wait-FLFramesPassed 20
        Result 'LOOK' "step H3: focused=$(Focus) with A down"
        Judge-Carry 'H3' $m 'pad job accept: ' $c $l
        Check ((Vis 'MissionVendorCloseButton')) 'step H3: the board stands open'
        $l2 = Log-Mark
        $n = Fresh-Hold 'pad job accept: clicked MissionVendorAcceptButton'
        Check ($n -eq 1) "step H3: a fresh hold accepts: $n lines (focused=$(Focus); screens: '$(Screen-Log $l2)')"
    } 120
}

# ---- buy: steps 1-13
function Section-buy {
    Load-Copy $copies.buy
    Step 1 'first stage: hold A, keep it down 2 s more' {
        $script:Offer = Pick-Ship
        Result 'LOOK' "the row: $($script:Offer)"
        $ship0 = Ship-Name; $c = Cash; $m = Mark; $l = Log-Mark
        Pad @('A')
        $null = Wait-FLUntil { if ((Lines $m 'pad ship buy: clicked ShipTraderBuyButton').Count) { 1 } } 'pad ship buy' 6
        Check $true 'step 1: pad ship buy: clicked ShipTraderBuyButton'
        $null = Wait-FLUntil { if ((Is-Final) -and (Field 'screen') -match '^(equipment|commodity)$' -and (AHeld) -ge 2) { 1 } } 'the final stage, A counted 2 s there' 16
        Shot 's1_held'
        Pad; $null = Wait-FLFramesPassed 10
        Check ((Lines $m 'pad ship trade').Count -eq 0) "step 1: no pad ship trade line (screens: $(Screen-Log $l))"
        Check ((Ship-Name) -eq $ship0 -and (Cash) -eq $c) "step 1: the ship '$(Ship-Name)' and credits $(Cash) unchanged"
        Wait-Still
    } 60
    Step 2 'release: the focus and the lane' {
        Ensure-Final
        $s = "$(Send-FL 'bases')"; $lane = [regex]::Match($s, "items='([^']*)'").Groups[1].Value
        Check ((Field 'focused' $s) -match $rxBuyShip) "step 2: focused=$(Field 'focused' $s) (the md: ContinueButton; IsBuyShip takes its caption too)"
        Check ($lane -match '\bSIDE(,| *$)' -and $lane -match 'FILTER' -and $lane -match 'BUY SHIP' -and $lane -match 'CLOSE') "step 2: the lane reads SIDE, FILTER, BUY SHIP, CLOSE: '$lane'"
        Check ((Field 'dropped' $s) -eq '0') "step 2: no pill dropped (dropped=$(Field 'dropped' $s) rows=$(Field 'rows' $s))"
        $st = "$(Send-FL 'nnstrip state')"
        Check ($st -match 'standing=0') "step 2: the strip does not stand here, LB / RB are the side keys: $(($st -split "`n")[0])"
        Result 'LOOK' 'step 2: the frame: the hold mark on A, no pill over BUY SHIP or a panel, nothing cut'
    }
    Step 3 'cancel: A down 0.3 s, release' {
        Ensure-Final
        if (-not ((Focus) -match $rxBuyShip)) { Skip "focused=$(Focus), not BUY SHIP" }
        $ship0 = Ship-Name; $c = Cash; $m = Mark
        Pad @('A'); Wait-Held 0.3
        Pad; $null = Wait-FLFramesPassed 10
        Check ((Lines $m 'pad ship trade').Count -eq 0) 'step 3: no click line'
        Check ((Ship-Name) -eq $ship0 -and (Cash) -eq $c) 'step 3: credits and ship unchanged'
        Check ((AHeld) -eq 0 -and (Is-Final)) "step 3: the count back at 0 ($(AHeld)), the window open"
    }
    Step 4 'a tap, then a second tap' {
        Ensure-Final
        if (-not ((Focus) -match $rxBuyShip)) { Skip "focused=$(Focus), not BUY SHIP" }
        $ship0 = Ship-Name; $c = Cash; $m = Mark
        Tap 'A'; Tap 'A'
        Check ((Lines $m 'pad ship trade|ContinueButton').Count -eq 0) 'step 4: BUY SHIP has no tap: no click line'
        Check ((Ship-Name) -eq $ship0 -and (Cash) -eq $c -and (Is-Final)) 'step 4: credits, ship and window unchanged'
    }
    Step 5 'RB, then LB' {
        Ensure-Final
        if (-not ((Focus) -match $rxBuyShip)) { Skip "focused=$(Focus), not BUY SHIP" }
        $script:Equip0 = Equip
        $m = Mark
        Tap 'RB'; $null = Wait-FLFramesPassed 20
        Check ((Lines $m 'pad dealer: side -> yours').Count -eq 1) "step 5: RB: side -> yours (focused=$(Focus))"
        $lane = Lane
        Check ($lane -match 'SIDE / BUY SHIP' -and $lane -match 'TRANSFER') "step 5: on a row: '$lane'"
        Shot 's5_row'
        $script:Dealer0 = (Rows).Dealer
        $m = Mark
        Tap 'LB'; $null = Wait-FLFramesPassed 20
        Check ((Lines $m 'pad dealer: side -> buy ship').Count -eq 1 -and (Focus) -match $rxBuyShip) "step 5: LB: side -> buy ship (focused=$(Focus))"
    }
    Step 6 'the trade-in: on a mounted row, A held' {
        Ensure-Final
        Side-To 2
        foreach ($i in 1..6) { if ((Lane) -match 'MOUNT') { break }; Tap 'Down'; $null = Wait-FLFramesPassed 10 }
        $mounted = (Lane) -match 'MOUNT'
        try { $null = Wait-FLUntil { if (Vis 'BuySellTradeButton') { 1 } } 'the transfer panel' 3 } catch { Skip "no transfer panel on focused=$(Focus)" }
        if (-not $mounted) { Result 'LOOK' "step 6: no row with a mount box found, the row used: $(Focus)" }
        $r0 = Rows; $ship0 = Ship-Name; $c = Cash; $m = Mark
        $n = Fresh-Hold 'pad transfer: clicked BuySellTradeButton'
        Check ($n -eq 1) "step 6: pad transfer: clicked BuySellTradeButton ($n lines)"
        # (the player's list shows five rows of more: only the dealer's side can show the move)
        try { $null = Wait-FLUntil { if ((Rows).Dealer -gt $r0.Dealer) { 1 } } 'the row on the dealer''s side' 5 } catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw } }
        $r1 = Rows
        Check ($r1.Dealer -gt $r0.Dealer) "step 6: the item stands on the dealer's side: rows dealer $($r0.Dealer) -> $($r1.Dealer), yours $($r0.Yours) -> $($r1.Yours)"
        Check ((Lines $m 'pad ship trade').Count -eq 0 -and (Ship-Name) -eq $ship0 -and (Cash) -eq $c) 'step 6: no purchase'
        $null = Wait-FLFramesPassed 90   # (the row is selected again within three tries, 0.3 s apart)
        $s = "$(Send-FL 'bases')"
        Check ((Field 'focused' $s) -match $rxRow -and (Field 'side' $s) -eq '2') "step 6: the focus keeps its row: focused=$(Field 'focused' $s) side=$(Field 'side' $s)"
        $null = Sum-Fields $script:Offer.Price
        Result 'LOOK' 'step 6: the frame: SHIP PRICE fell, the item on the dealer''s side (the text is no debug output)'
    }
    Step 7 'the three stops' {
        Ensure-Final
        if (-not (Vis 'ContinueButton')) { Skip 'BUY SHIP is not shown' }
        Side-To 1
        foreach ($p in @(@('RB', 'buy ship', $rxBuyShip), @('RB', 'yours', $rxRow), @('LB', 'buy ship', $rxBuyShip), @('LB', 'dealer', $rxRow))) {
            $m = Mark
            Tap $p[0]; $null = Wait-FLFramesPassed 20
            Check ((Lines $m "pad dealer: side -> $($p[1])").Count -eq 1 -and (Focus) -match $p[2]) "step 7: $($p[0]) -> $($p[1]) in one press (focused=$(Focus))"
        }
    }
    Step 8 'back: BackToShipDealer by the D-pad, tap A' {
        Ensure-Final
        if (-not (Walk-Focus '^BackToShipDealer' (@('Down') * 10 + @('Up') * 14))) { Skip "BackToShipDealer not reached by a D-pad walk (focused=$(Focus)); the stick's way is not driven here" }
        $m = Mark
        Tap 'A'
        Wait-Screen 'shipdealer'
        Wait-Still
        Check ((Lines $m 'pad dealer: BackToShipDealer clicked').Count -eq 1) 'step 8: the ship dealer again'
        # (nothing is sold until the purchase, so `equipment` never changed: the rows are what can show the return)
        Check ((Equip) -eq $script:Equip0) "step 8: the server's equipment as before: $(Equip)"
        Ensure-Final
        $d = (Rows).Dealer
        Check ($script:Dealer0 -ge 0 -and $d -eq $script:Dealer0) "step 8: the item returned: the dealer's rows $d, before the transfer $($script:Dealer0)"
    }
    Step 13 'close (B) from the final stage, one item transferred' {
        Ensure-Final
        Side-To 2
        $ship0 = Ship-Name; $c = Cash
        try { $null = Wait-FLUntil { if (Vis 'BuySellTradeButton') { 1 } } 'the transfer panel' 3 } catch { Skip "no transfer panel on focused=$(Focus)" }
        $null = Fresh-Hold 'pad transfer: clicked BuySellTradeButton'
        $null = Wait-FLFramesPassed 60
        Tap 'B'
        Wait-Screen 'room'
        $null = Wait-FLFramesPassed 30
        Check ((Ship-Name) -eq $ship0 -and (Cash) -eq $c) "step 13: the room, the old ship '$(Ship-Name)', credits $(Cash)"
        Check ((Equip) -eq $script:Equip0) "step 13: the equipment as before: $(Equip)"
    }
    Step 9 'a real purchase, nothing transferred' {
        Ensure-Final
        if (-not ((Focus) -match $rxBuyShip)) { Skip "focused=$(Focus), not BUY SHIP" }
        $ship0 = Ship-Name; $c = Cash
        $sum = Sum-Fields $script:Offer.Price
        $n = Fresh-Hold 'pad ship trade: clicked ContinueButton'
        Check ($n -eq 1) "step 9: pad ship trade: clicked ContinueButton ($n lines)"
        if (-not $n) { return }
        Wait-Screen 'room' 20
        $null = Wait-FLUntil { if ((Cash) -ne $c) { 1 } } 'the credits moved' 10
        Check ((Ship-Name) -ne $ship0) "step 9: uistrip data names the new ship '$(Ship-Name)', was '$ship0'"
        if ($sum) {
            $want = $c - $sum.Price + $sum.Trade
            Check ([Math]::Abs((Cash) - $want) -lt 1) "step 9: credits $c - price $($sum.Price) + trade-in $($sum.Trade) = $want, worth reads $(Cash)"
        } else { NotRun '9 sum' "NN_Dealer +0x9FC read no trade-in under the ship's price $($script:Offer.Price): the sum not judged; credits $c -> $(Cash)" }
        $null = Wait-FLFramesPassed 60
        $s = "$(Send-FL 'bases')"
        Check ((Field 'screen' $s) -eq 'room' -and (Field 'count' $s) -ne '0') "step 9: the room back, the pad's prompts back: screen=$(Field 'screen' $s) count=$(Field 'count' $s)"
    } 120
    Step 10 'too few credits: open the final stage' {
        Load-Copy $copies.poor
        $o = Pick-Ship
        Close-Window 'ShipTraderCancelButton'
        # the md's window: the first stage's budget still passes (it counts the equipment), the final stage's sum does not (hull only)
        $want = $o.Price - ($o.Budget - $o.Credits) + 1
        if ($want -lt 0) { Skip "the trade-in covers the ship: price $($o.Price), budget $($o.Budget), credits $($o.Credits)" }
        $b = [BitConverter]::GetBytes([int]$want)
        $r = Send-FL ('poke 0x673364 {0:X2} {1:X2} {2:X2} {3:X2}' -f $b[0], $b[1], $b[2], $b[3])
        if ($r -notmatch '^ok') { Skip "poke: $r" }
        $script:Offer = Pick-Ship
        Result 'LOOK' "step 10: client credits poked to ${want}: $($script:Offer)"
        To-Final
        if (Vis 'ContinueButton') { Skip "BUY SHIP stands at $want credits: the fixture is not made (the hull's trade-in covers the rest?)" }
        $script:Poor = $true
        $s = "$(Send-FL 'bases')"; $lane = [regex]::Match($s, "items='([^']*)'").Groups[1].Value
        Check $true 'step 10: no BUY SHIP (not visible ContinueButton)'
        Check (Vis 'ShipBuyProblem') 'step 10: ShipBuyProblem is shown; its words "You need $n more credits" in the frame'
        Check ((Field 'focused' $s) -match '^ItemView0?1$' -and (Field 'side' $s) -eq '2') "step 10: focused=$(Field 'focused' $s) side=$(Field 'side' $s), the player's first row"
        Check ($lane -match 'TRANSFER' -and $lane -match '\bSIDE(,| *$)' -and $lane -notmatch 'BUY SHIP') "step 10: A TRANSFER, the pill SIDE alone: '$lane'"
    } 150
    Step 11 'A held, the focus off every row (a filter)' {
        if (-not $script:Poor) { Skip 'the too-few-credits fixture does not stand' }
        # (the filters lie under the list: Down first, Up leads to the NeuroNet's box)
        if (-not (Walk-Focus '^Filter_' (@('Down') * 8 + @('Up') * 16))) { Skip "no filter under the focus by the D-pad ($(Focus))" }
        $panel = Vis 'BuySellTradeButton'
        $ship0 = Ship-Name; $c = Cash; $m = Mark
        Pad @('A'); Wait-Held $kCarry
        Pad; $null = Wait-FLFramesPassed 10
        # (the row may stay selected behind the focus: HoldAct leaves it alone off a row or the trade panel)
        Check ((Lines $m 'pad (ship trade|transfer): ').Count -eq 0) "step 11: no click line ($((Lines $m 'pad (ship trade|transfer): ') -join ' / ')); the transfer panel stood: $panel, focused=$(Focus)"
        Check ((Ship-Name) -eq $ship0 -and (Cash) -eq $c) 'step 11: nothing bought'
    }
    Step 12 'transfer rows until the text goes, then a hold buys' {
        if (-not $script:Poor) { Skip 'the too-few-credits fixture does not stand' }
        # (step 10's credits sit under the sum by all the equipment's worth: here one item's worth short, so a transfer or two brings BUY SHIP)
        $short = $script:Offer.Price - (Get-FLPeek32 ((Ctl 'NN_Dealer') + 0x9FC)) - 100
        $b = [BitConverter]::GetBytes([int]$short)
        $r = Send-FL ('poke 0x673364 {0:X2} {1:X2} {2:X2} {3:X2}' -f $b[0], $b[1], $b[2], $b[3])
        if ($r -notmatch '^ok') { Skip "poke: $r" }
        Result 'LOOK' "step 12: client credits poked to $short"
        $e0 = Equip; $moved = 0
        for ($i = 0; $i -lt 8 -and -not (Vis 'ContinueButton'); $i++) {
            Side-To 2
            try { $null = Wait-FLUntil { if (Vis 'BuySellTradeButton') { 1 } } 'the transfer panel' 3 }
            catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw }; Tap 'Down'; $null = Wait-FLFramesPassed 10; continue }
            $moved += Fresh-Hold 'pad transfer: clicked BuySellTradeButton'
            $null = Wait-FLFramesPassed 60
        }
        if (-not (Vis 'ContinueButton')) { Skip "BUY SHIP never appeared after $moved transfers: the items do not cover the ship" }
        Check $true "step 12: BUY SHIP appears after $moved transfers"
        Side-To 2
        $lane = Lane
        Check ($lane -match 'SIDE / BUY SHIP') "step 12: the pill: '$lane'"
        $m = Mark
        Tap 'LB'; $null = Wait-FLFramesPassed 20
        Check ((Lines $m 'pad dealer: side -> buy ship').Count -eq 1 -and (Focus) -match $rxBuyShip) "step 12: LB reaches it (focused=$(Focus))"
        $ship0 = Ship-Name
        $n = Fresh-Hold 'pad ship trade: clicked ContinueButton'
        Check ($n -eq 1) "step 12: a hold buys, on the copy $($copies.poor) ($n lines)"
        if ($n) { Wait-Screen 'room' 20 }
        Check ((Ship-Name) -ne $ship0) "step 12: the ship is '$(Ship-Name)', was '$ship0'"
        Result 'LOOK' "step 12: equipment before: $e0"
        Result 'LOOK' "step 12: after (the transferred sold, the rest moved): $(Equip)"
    } 150
    NoFault 'buy'
}

# ---- stick: steps 14-20
function Drive-Stick {
    $b0 = Bars
    Pad -RY -1
    try {
        try { $null = Wait-FLUntil { $n = Bars; foreach ($k in $b0.Keys) { if ($n[$k] -and $n[$k].Pos -ne $b0[$k].Pos) { return 1 } }; $null } 'a list bar moving' 3 }
        catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw } }   # (no move is a result)
        $null = Wait-FLFramesPassed 12   # (a second bar moving by the same stick shows here)
    } finally { Pad }
    $null = Wait-FLFramesPassed 10
    Bars
}
# Issue 36: "both sides" was only inferred. Both moving is reported with both readings: the cause would still be open.
function Judge-Stick([string]$id, [string]$want, $b0, $b1) {
    $moved = @('dealer', 'yours' | Where-Object { $b0[$_] -and $b1[$_] -and $b0[$_].Pos -ne $b1[$_].Pos })
    $read = "before: $(Bars-Text $b0); after: $(Bars-Text $b1)"
    if ($moved.Count -eq 2) { Result 'FAIL' "step ${id}: BOTH lists moved, the cause of issue 36 is still open: $read"; return }
    if ($want -eq 'none') { Check ($moved.Count -eq 0) "step ${id}: neither moves: $read"; return }
    Check ($moved.Count -eq 1 -and $moved[0] -eq $want) "step ${id}: only the $want list moves: $read"
}
function Need-Both { $b = Bars; if (-not ($b.dealer -and $b.yours)) { Skip "both lists must overflow: $(Bars-Text $b)" } }
# Buys one of each dealer row by native clicks (a fixture, as Invoke-FLTraderBuy) until $until holds; rows the ship cannot take are passed over
function Buy-Kinds([scriptblock]$until, [int]$max = 12) {
    $done = 0; $row = 0
    while ($done -lt $max -and -not (& $until)) {
        $row++
        if ($row -gt 5) {
            $b = Bars
            if (-not $b.dealer) { break }
            $p0 = $b.dealer.Pos
            $null = Send-FL "scroll $($b.dealer.Name) 1"
            $null = Wait-FLFramesPassed 6
            if ((Bars).dealer.Pos -le $p0) { break }
            $row = 5
        }
        if (-not (Vis "ItemButton$row")) { if ($row -le 5) { $row = 5 }; continue }
        $null = Send-FL "click ItemButton$row"
        # (no panel: "Your ship does not have a hardpoint capable of mounting this", seen on the class 5 rows)
        try { $null = Wait-FLUntil { if (Vis 'BuySellTradeButton') { 1 } } 'the row''s panel' 1 } catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw }; continue }
        $c = Cash
        $null = Send-FL 'click BuySellTradeButton'
        try { $null = Wait-FLUntil { if ((Cash) -ne $c) { 1 } } 'the charge' 4 } catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw }; continue }
        $done++
        # (the equipment dealer clears its selection about 0.5 s after a purchase, and a row clicked before that is let go: a frame count ran short of it at a high frame rate.
        # The commodity trader keeps its selection, so a timeout here is no fault)
        try { $null = Wait-FLUntil { if (-not (Vis 'BuySellTradeButton')) { 1 } } 'the selection cleared' 1.5 } catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw } }
        Wait-Still
    }
    $done
}
# The left stick's cursor to the other list's rows, no D-pad press. The dealer stands right of the player's list (the frames). Full deflection: the response is quadratic and the
# magnet slows the cursor over each control, so half a stick for 6 s, spent first the wrong way, ended still over the player's list.
function Cursor-To([int]$side) {
    $first = if ($side -eq 1) { 1 } else { -1 }
    foreach ($x in $first, (-$first)) {
        Pad -LX $x
        try {
            try { $null = Wait-FLUntil { $s = "$(Send-FL 'bases')"; if ((Field 'side' $s) -eq "$side" -and (Field 'focused' $s) -match $rxRow) { 1 } } "the cursor over side $side" 12; Pad; return $true }
            catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw } }
        } finally { Pad }
    }
    $false
}
function Stick-Dealer([string]$k, [string]$what, [string]$room, [string]$spot, [string]$ready, [string]$filter) {
    Step "18$k" "$what`: a focused list that fits, the other overflowing" {
        Open-Spot $room $spot $ready
        if ($filter) { $null = Send-FL "click $filter"; $null = Wait-FLFramesPassed 30 }
        $null = Wait-FLUntil { if (Vis 'ItemButton1') { 1 } } 'the dealer''s rows' 8
        if ((Rows).Yours -lt 1 -or -not (Vis 'ItemButton1#2')) { $null = Buy-Kinds { Vis 'ItemButton1#2' } 3 }
        $b = Bars
        if (-not $b.dealer -or $b.yours) { Skip "the dealer's list must overflow and the player's fit: $(Bars-Text $b)" }
        Side-To 2
        Judge-Stick "18$k" 'none' $b (Drive-Stick)
    } 120
    Step "fix$k" "$what`: both lists overflowing" {
        $n = Buy-Kinds { (Bars).yours } 12
        # (a filter holds the player's list to its kind: another may show more of it, and sell what lands there)
        foreach ($f in 'Filter_Equipment', 'Filter_Ammo', 'Filter_CargoBay', 'Filter_Commodity') {
            $b = Bars
            if ($b.dealer -and $b.yours) { break }
            if (-not $filter -or -not (Vis $f)) { continue }
            $null = Send-FL "click $f"; $null = Wait-FLFramesPassed 30
            $b = Bars
            if ($b.dealer -and -not $b.yours) { $n += Buy-Kinds { (Bars).yours } 8 }
            Result 'LOOK' "fixture ${k}: $f, $n kinds bought: $(Bars-Text (Bars))"
        }
        $b = Bars
        if (-not ($b.dealer -and $b.yours)) { Skip "$n kinds bought and not both overflow: $(Bars-Text $b)" }
        Check $true "fixture ${k}: $n kinds bought: $(Bars-Text $b)"
    } 180
    Step "14$k" "$what`: focus on a row of the dealer's list" {
        Need-Both; Side-To 1; Bars-Top
        $b0 = Bars; Judge-Stick "14$k" 'dealer' $b0 (Drive-Stick)
    }
    Step "15$k" "$what`: LB / RB to the player's rows" {
        Need-Both; Side-To 2; Bars-Top
        $b0 = Bars; $m = Mark
        Judge-Stick "15$k" 'yours' $b0 (Drive-Stick)
        $sel = Lines $m 'pad (commodity|equipment): selected'
        Check ($sel.Count -ge 1 -and (Field 'side') -eq '2') "step 15${k}: the focused list's row selected again: $($sel.Count) lines, side=$(Field 'side')"
    }
    Step "16$k" "$what`: the left stick's cursor over the other list" {
        Need-Both; Side-To 2; Bars-Top
        if (-not (Cursor-To 1)) { Skip "the cursor did not reach a dealer's row (side=$(Field 'side') focused=$(Focus))" }
        $s = "$(Send-FL 'bases')"
        Result 'LOOK' "step 16${k}: cursorShown=$(Field 'cursorShown' $s) focused=$(Field 'focused' $s) side=$(Field 'side' $s)"
        $b0 = Bars; Judge-Stick "16$k" 'dealer' $b0 (Drive-Stick)
    }
    Step "17$k" "$what`: focus on a filter or on BUY / SELL" {
        Need-Both; Side-To 2; Bars-Top
        if (-not (Walk-Focus '^(Filter_|BuySell)' (@('Up') * 8 + @('Down') * 12))) { Skip "no filter or BUY / SELL by the D-pad ($(Focus))" }
        $want = if ((Field 'side') -eq '2') { 'yours' } else { 'dealer' }
        Result 'LOOK' "step 17${k}: focused=$(Focus), the list last focused: $want"
        $b0 = Bars; Judge-Stick "17$k" $want $b0 (Drive-Stick)
    }
    Step "19$k" "$what`: an overflowing description" {
        Need-Both; Side-To 1; Bars-Top
        $t = $null
        foreach ($i in 1..30) {
            $x = Stick-Bar
            if ($x -and $x.Name -notmatch '^(Inventory_Scroll|-)?$') { $t = $x; break }
            Tap 'Down'; $null = Wait-FLFramesPassed 15
        }
        if (-not $t) { Skip 'no row with an overflowing description in 30 rows at this size' }
        Bars-Top
        $null = Wait-FLFramesPassed 15
        $b0 = Bars; $t = Stick-Bar
        Pad -RY -0.4 # (gently: the stick steps a whole line, and a text overflowing by less ends in one tick, the list moving before a read at full speed)
        try {
            $null = Wait-FLUntil { $x = Stick-Bar; if ($x -and $x.Pos -ne $t.Pos) { 1 } } 'the text moving' 4
            $b1 = Bars
            Check ($b1.dealer.Pos -eq $b0.dealer.Pos) "step 19${k}: the text first: text $($t.Pos) -> $((Stick-Bar).Pos) of $($t.Hi), the list stands: $(Bars-Text $b1)"
            $null = Wait-FLUntil { if ((Bars).dealer.Pos -ne $b0.dealer.Pos) { 1 } } 'the list moving at the text''s end' 25
        } finally { Pad }
        $b2 = Bars
        Check ($b2.yours.Pos -eq $b0.yours.Pos) "step 19${k}: then the focused list: $(Bars-Text $b2); text at $((Stick-Bar).Pos)"
    }
    Step "x$k" "$what`: closed" { Close-Window 'Inventory_Close' } 30
}
# Step 20, one single list: whatever bar the stick means there must move
function Single-List([string]$id, [string]$what, [scriptblock]$open, [string]$screen, [string]$close, [scriptblock]$prep = { }) {
    Step $id "single list: $what" {
        & $open
        Wait-Screen $screen 12
        Wait-Still
        & $prep
        $b = Stick-Bar
        if (-not $b -or $b.Name -match '^-?$' -or $b.Hi - $b.Page -le $b.Lo) { Skip "nothing here overflows: stickBar=$(Field 'stickBar')" }
        Pad -RY -1
        try { $moved = $false
            try { $null = Wait-FLUntil { $x = Stick-Bar; if ($x -and $x.Pos -ne $b.Pos) { 1 } } 'the bar moving' 3; $moved = $true } catch { if ($_.Exception.Message -match 'OVER BUDGET') { throw } }
        } finally { Pad }
        Check $moved "step ${id}: RS moves $($b.Name): $($b.Pos) -> $((Stick-Bar).Pos) of $($b.Hi)"
    } 60
    if ((Field 'screen') -ne 'room') { try { Close-Window $close } catch { Result 'FAIL' "step ${id}: its window did not close: $($_.Exception.Message)" } }
}
function Section-stick {
    Load-Copy $copies.stick
    Stick-Dealer 'c' 'commodity dealer' 'Trader' 'IDS_HOTSPOT_COMMODITYTRADER' 'ItemButton1' ''
    Stick-Dealer 'e' 'equipment dealer' 'Equipment' 'IDS_HOTSPOT_EQUIPMENTDEALER' 'Filter_Weapons' 'Filter_Weapons'
    Single-List '20inv' 'the inventory alone' { Open-NN 'USER_INVENTORY' 'NNInventoryButtonRoot' 'inventory' } 'inventory' 'Inventory_Close' {
        foreach ($i in 1..5) { $x = Stick-Bar; if ($x -and $x.Name -notmatch '^-?$') { break }; Tap 'X'; $null = Wait-FLFramesPassed 20 }
    }
    Single-List '20job' 'the job board' { Open-Spot 'Bar' 'IDS_HOTSPOT_MISSIONVENDOR' 'MissionVendorCloseButton' } 'jobboard' 'MissionVendorCloseButton'
    Single-List '20log' 'the log' { Open-NN 'USER_STORY_STAR' 'NNMissionLogButtonRoot' 'missionlog' } 'missionlog' 'StoryStarCloseButton'
    Single-List '20help' 'help' { Open-NN 'USER_HELP' 'NNHelpButtonRoot' 'help' } 'help' 'PlayerInfoCloseButton'
    Single-List '20stat' 'status' { Open-NN 'USER_PLAYER_STATS' 'NNPlayerInfoButtonRoot' 'playerinfo' } 'playerinfo' 'PlayerInfoCloseButton'
    NoFault 'stick'
}

# ---- kinds: the md's second kind, a PlayStation pad through SDL
function Section-kinds {
    Step 'K' 'step 1 and 2 on a PlayStation test pad' {
        Disable-FLDevPads      # (while they are on XInput reads them, even with pad_backend 1)
        $null = Send-FL 'set pad_backend 1'
        $null = Wait-FLUntil { if ((Send-FL 'state') -match 'sdl_status=2') { 1 } } 'SDL running' 15
        Write-Host "        $(Connect-FLSdlPad -Type ps5 -Foreground)"
        $script:Kind = 'sdl'
        try {
            Wake-Pad
            $null = Wait-FLUntil { if ((Send-FL 'padhud state') -match 'glyphs family=playstation') { 1 } } 'the PlayStation glyphs' 10
            Check $true 'step K: the glyph family follows the pad'
            $o = Pick-Ship
            $c = Cash; $m = Mark
            To-Final
            Check ((Lines $m 'pad ship buy: clicked ShipTraderBuyButton').Count -eq 1 -and (Lines $m 'pad ship trade').Count -eq 0 -and (Cash) -eq $c) "step K: the hold reached the final stage, nothing bought ($($o.Price))"
            $s = "$(Send-FL 'bases')"
            Check ((Field 'focused' $s) -match $rxBuyShip) "step K: focused=$(Field 'focused' $s)"
            Result 'LOOK' "step K: the lane: '$([regex]::Match($s, "items='([^']*)'").Groups[1].Value)'"
            Shot 'sK_lane'
            Tap 'B'
            Wait-Screen 'room'
        } finally {
            $script:Kind = 'dev'
            try { $null = Disconnect-FLSdlPad } catch { Write-Host "        (detach: $($_.Exception.Message))" }
            $null = Send-FL 'set pad_backend 0'
            Dev-Pads-On
        }
    } 120
}

# ---- kbm: the page's keyboard and mouse section, by native clicks
function Section-kbm {
    Disable-FLDevPads
    $null = Send-FL 'set input_mode 2'
    $null = Load-FLSave $copies.buy
    $script:Poor = $false
    Step 'M1' 'the final stage by clicks: no pad prompt' {
        Open-Spot 'ShipDealer' 'IDS_HOTSPOT_SHIPDEALER' 'ShipTraderShipName0'
        Hits 'ship'
        $null = Send-FL 'click ShipTraderShipNameFrame2'
        $null = Wait-FLUntil { if (Vis 'ShipTraderBuyButton') { 1 } } 'the row''s BUY' 6
        $null = Send-FL 'click ShipTraderBuyButton'
        $null = Wait-FLUntil { if (Is-Final) { 1 } } 'the final stage' 12
        Wait-Still
        $s = "$(Send-FL 'bases')"
        Check ((Field 'pad' $s) -ne '1') "step M1: the pad is not the device in use: pad=$(Field 'pad' $s)"
        Check ([regex]::Match($s, "items='([^']*)'").Groups[1].Value -eq '' -or (Field 'count' $s) -eq '0') "step M1: no pad prompt: count=$(Field 'count' $s) items='$([regex]::Match($s, "items='([^']*)'").Groups[1].Value)'"
        Check (Vis 'ContinueButton') 'step M1: BUY SHIP stands, as in the game'
    } 60
    Step 'M2' 'a row and TRANSFER by clicks' {
        if (-not (Is-Final)) { Skip 'the final stage does not stand' }
        $r0 = Rows; $c = Cash
        $null = Send-FL 'click ItemView01'
        $null = Wait-FLUntil { if (Vis 'BuySellTradeButton') { 1 } } 'the transfer panel' 6
        $null = Send-FL 'click BuySellTradeButton'
        $null = Wait-FLUntil { if ((Rows).Dealer -gt $r0.Dealer) { 1 } } 'the row on the dealer''s side' 6
        Check ((Rows).Dealer -gt $r0.Dealer) "step M2: the item stands on the dealer's side: rows $($r0.Dealer) -> $((Rows).Dealer)"
        Check ((Cash) -eq $c) 'step M2: nothing paid yet'
    } 60
    Step 'M3' 'BUY SHIP by a click' {
        if (-not (Vis 'ContinueButton')) { Skip 'BUY SHIP does not stand' }
        $ship0 = Ship-Name; $c = Cash
        $null = Send-FL 'click ContinueButton'
        $null = Wait-FLUntil { if ((Ship-Name) -ne $ship0) { 1 } } 'the new ship' 20
        Check ((Ship-Name) -ne $ship0) "step M3: the ship is '$(Ship-Name)', was '$ship0'; credits $c -> $(Cash)"
    } 60
    NoFault 'kbm'
    $null = Send-FL 'set input_mode 0'
}

# ---- one game: a size, a skin
function Stop-Game {
    if (Get-FLGameProcess) { try { Disable-FLDevPads } catch { } }
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
function Make-Copies {
    $d = Get-FLSaveDir
    $src = Join-Path $d $Save
    if (-not (Test-Path -LiteralPath $src)) { throw "no $Save in $d" }
    foreach ($n in $copies.Values) {
        $to = Join-Path $d $n
        if (Test-Path -LiteralPath $to) { Remove-Item -LiteralPath $to -Force }   # (this script's own copy; never written through a hard link)
        # (bytes through Latin-1: the save is plain text and only this line changes)
        $enc = [Text.Encoding]::GetEncoding(28591)
        $text = $enc.GetString([IO.File]::ReadAllBytes($src))
        if ($text -notmatch '(?m)^rank = \d+') { throw "no rank line in $src" }
        [IO.File]::WriteAllBytes($to, $enc.GetBytes(([regex]'(?m)^rank = \d+').Replace($text, "rank = $Rank", 1)))
    }
}
function Play([string]$size, [int]$skin, [string[]]$sections) {
    $script:Tag = "$(Tag $size)_skin$skin"
    Write-Host "=== game $($script:Tag): $($sections -join ', ')"
    Stop-Game
    Set-FLInstanceDisplay $Instance 'borderless' $size
    Make-Copies
    & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Modules subtitles,controller -Save $copies.hold | Select-Object -Last 1
    try {
        $null = Wait-FLFramesPassed 200
        Start-FLEventLog | Out-Null
        Dev-Pads-On
        Wake-Pad
        Write-Host "        $(Send-FL "set ui_style $skin")"
        $null = Wait-FLFramesPassed 30
        foreach ($s in $sections) {
            if (-not (Get-FLGameProcess)) { Result 'FAIL' "section ${s}: the game is gone"; break }
            try { & "Section-$s" } catch { Result 'FAIL' "section ${s}: $($_.Exception.Message)" }
        }
        NoFault $script:Tag
    } finally {
        Set-FLDeadline 0; Stop-Game
        try { Copy-Item -LiteralPath (Log-Path) (Join-Path $dir "$($script:Tag).log") -Force } catch { }
    }
}

try {
    $first = $true
    foreach ($size in $Sizes) {
    $script:HitSize = $size
        foreach ($skin in $Skins) {
            # (hold first: a board's offers go at a second load of a docked save, and this game's first load is its own)
            $sections = @('hold', 'buy', 'stick', 'kinds', 'kbm' | Where-Object { $Only -contains $_ -and ($first -or $_ -in 'buy', 'stick', 'kbm') })
            if ($sections.Count) {
                try { Play $size $skin $sections } catch { Result 'FAIL' "game $(Tag $size) skin ${skin}: $($_.Exception.Message)" }
            }
            $first = $false
        }
    }
} finally {
    Stop-Game
    Set-FLInstanceDisplay $Instance 'borderless' $home_
}
if ($notRun.Count) { "ship_buy_pad: NOT RUN ($($notRun.Count)):"; $notRun | ForEach-Object { "  $_" } }
if ($bad.Count) { "ship_buy_pad: $($bad.Count) FAILED"; $bad | ForEach-Object { "  $_" }; exit 1 }
if ($notRun.Count) { "ship_buy_pad: no FAIL, but steps did not run; frames in $dir"; exit 2 }
"ship_buy_pad: no FAIL; frames in $dir"
