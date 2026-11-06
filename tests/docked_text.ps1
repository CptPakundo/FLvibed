# GitHub issue 22, "fonts randomly change in size and boldness in the Job Board, Commodity Trader, Equipment Dealer and Ship Dealer".
# Walks those four windows on a numbered instance (background game, SaveC701 at Manhattan, HUD 2, deck_mode 2) the way a player does:
# the equipment dealer's filters, one of them twice, the lists scrolled, rows selected. After each step it reads every shown text's
# font (debug `gametext texts`) and the rich size (`gametext window`). Frames and the raw tables: research\experience\runs\issue22\<Label>\.
# It JUDGES, at each -Sizes size, with text_size and hud_scale 100:
#   ROLE   rows of one role keep one font for one opening of a window (a list's names, prices, the transfer row, job rewards)
#   BACK   a filter left and come back to shows its rows in the font they had
#   RICH   one rich text size for one opening of a window, whichever item is selected
#   LABELS at that size every stat stands beside its value (the game's `labelsOff`)
# Any of these failing is the bug: exit 1. Three more are reported beside them, each a control that must hold for that reading to stand:
#   VISIT  the same steps in a second opening give the same fonts (the sizes are not random: they follow what the window holds)
#   SLIDER text_size / hud_scale 80 give the same fonts as 100 (no slider reaches the game's own text)
#   OFF    with window_text 0 ROLE, BACK and RICH all hold (the variation is windowtext's)
# Usage (PowerShell tool): tests\docked_text.ps1 [-Instance 3] [-Sizes '1280, 800','3840, 2160'] [-Label run] [-NoPad]   uses build\release
param([int]$Instance = 3, [string[]]$Sizes = @('1280, 800', '3840, 2160'), [string]$Label = 'docked', [switch]$NoPad)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
$env:FLVIBED_INSTANCE = "$Instance"
$top = Join-Path $root "research\experience\runs\issue22\$Label"
New-Item -ItemType Directory -Force $top | Out-Null

$script:dir = $top
$script:rows = [Collections.Generic.List[object]]::new()
$script:rich = [Collections.Generic.List[object]]::new()
$script:opening = 0
$script:phase = ''
$script:win = ''
$script:shots = $false
$script:labelsOff = 0

$roles = [ordered]@{
    'list names' = '^ItemName\d+$'; 'list prices' = '^ItemPrice\d+$'
    'transfer name' = '^TransferItemName$'; 'transfer price' = '^TransferItemPrice$'
    'ship names' = '^ShipTraderShipName\d$'; 'ship prices' = '^ShipTraderShipPrice\d$'
    'job rewards' = '^MissionVendorSummaryReward\d$'; 'chosen job reward' = '^MissionVendorMissionReward$'
}
function Say([string]$s) { Add-Content -LiteralPath (Join-Path $script:dir 'walk.log') "$(Get-Date -Format HH:mm:ss) $s"; Write-Host $s }

# One step: every text's font, the rich size; a frame when asked
function Snap([string]$step, [switch]$Frame) {
    $null = Wait-FLFramesPassed 30
    $win = Send-FL 'gametext window'
    $m = [regex]::Match($win, 'window=(\S+).*columns(.*?) singles=\d+ rich=(\d+)')
    if ($win -match 'labelsOff=1') { $script:labelsOff++; Say "  labels apart: $($script:win) $step" }
    $script:rich.Add([pscustomobject]@{ phase = $script:phase; opening = $script:opening; win = $script:win; step = $step; rich = [int]$m.Groups[3].Value
                                        columns = $m.Groups[2].Value.Trim() })
    foreach ($l in (Send-FL 'gametext texts') -split "`n") {
        $t = [regex]::Match($l, '^(\S+) < (\S*) x=(\S+) y=(\S+) font (\S*) ext (\S+) x (\S+) lines (\d+) wrap (\d+)')
        if (-not $t.Success -or [int]$t.Groups[8].Value -lt 1) { continue }
        $script:rows.Add([pscustomobject]@{ phase = $script:phase; opening = $script:opening; win = $script:win; step = $step
                                            ctl = $t.Groups[1].Value; parent = $t.Groups[2].Value; font = $t.Groups[5].Value
                                            w = [double]$t.Groups[6].Value; h = [double]$t.Groups[7].Value })
    }
    if ($Frame -and $script:shots) {
        $name = "$($script:phase)_$($script:win)_$step"
        $f = Save-FLShot -Name $name -Dir $script:dir
        Move-Item -LiteralPath $f (Join-Path $script:dir "$name.png") -Force
    }
}
function Open-Win([string]$room, [string]$spot, [string]$ready, [string]$win) {
    if ((Send-FL 'hotspots') -notmatch "(?m)^$spot\s") { $null = Goto-FLRoom $room }
    $null = Wait-FLRoomReady
    $r = Wait-FLUntil { $x = Send-FL "goto $spot"; if ($x -notmatch 'busy') { $x } } "$spot click" 15
    if ($r -notmatch '^clicked') { throw "${spot}: $r" }
    try { $null = Wait-FLUntil { if ((Send-FL "visible $ready") -match '^visible') { 1 } } "$win open" 25 }
    catch {
        # what stands there instead: the frame, the pad's view and the controls
        $null = Save-FLShot -Name "not_open_$win" -Dir $script:dir
        Say "not open: $win; $(((Send-FL 'bases') -split "`n")[0])"
        Say (((Send-FL 'uiitems') -split "`n" | ForEach-Object { ($_ -split '\s+')[0] }) -join ' ')
        throw
    }
    $null = Wait-FLFramesPassed 300 # the slide-in; the rows reach their size during it
    $script:opening++
    $script:win = $win
}
function Close-Win([string]$btn) {
    $null = Wait-FLUntil { $null = Send-FL "click $btn"; if ((Send-FL 'gametext window') -match 'window=none') { 1 } } "close $btn" 15
    $null = Wait-FLFramesPassed 30
}
function Pick([string]$ctl, [string]$step, [switch]$Frame) {
    if ((Send-FL "visible $ctl") -notmatch '^visible') { return }
    $null = Send-FL "click $ctl"
    Snap $step -Frame:$Frame
}
# Down a list to its end, a step at a time
function Scroll-Down([string]$bar, [string]$step) {
    $null = Send-FL "scroll $bar -1000"
    for ($i = 0; $i -lt 30; $i++) {
        $p = Send-FL "scroll $bar 2"
        if ($p -notmatch 'pos=([\d.]+)' -or [int][double]$Matches[1] -le $at) { break }
        $at = [int][double]$Matches[1]
        Snap "${step}_at$at"
    }
    $null = Send-FL "scroll $bar -1000"
}
function Walk-Equipment {
    Open-Win 'Equipment' 'IDS_HOTSPOT_EQUIPMENTDEALER' 'Filter_Weapons' 'equipment'
    Snap 'open'
    $null = Send-FL 'click Filter_Weapons'; Snap 'weapons' -Frame
    Pick 'ItemButton1' 'weapons_row1' -Frame
    Pick 'ItemButton3' 'weapons_row3'
    $script:at = 0; Scroll-Down 'Inventory_Scroll' 'weapons_scroll'
    foreach ($f in 'Ammo', 'Equipment', 'CargoBay') {
        $null = Send-FL "click Filter_$f"; Snap $f.ToLower()
        Pick 'ItemButton1' "$($f.ToLower())_row1" -Frame
        $script:at = 0; Scroll-Down 'Inventory_Scroll' "$($f.ToLower())_scroll"
    }
    $null = Send-FL 'click Filter_Weapons'; Snap 'weapons_back' -Frame
    Pick 'ItemButton1' 'weapons_back_row1' -Frame
    Close-Win 'Inventory_Close'
}
function Walk-Trader {
    Open-Win 'Trader' 'IDS_HOTSPOT_COMMODITYTRADER' 'ItemButton1' 'trader'
    Snap 'open'
    foreach ($r in 1..5) { Pick "ItemButton$r" "row$r" -Frame:($r -le 3) }
    $script:at = 0; Scroll-Down 'Inventory_Scroll' 'scroll'
    Pick 'ItemButton1' 'row1_back'
    Close-Win 'Inventory_Close'
}
function Walk-Ship {
    Open-Win 'ShipDealer' 'IDS_HOTSPOT_SHIPDEALER' 'ShipTraderShipName0' 'ship'
    Snap 'open'
    foreach ($i in 0..2) { Pick "ShipTraderShipNameIconFrame$i" "row$i" -Frame:($i -eq 0) }
    Close-Win 'ShipTraderCancelButton'
}
function Walk-Job {
    Open-Win 'Bar' 'IDS_HOTSPOT_MISSIONVENDOR' 'MissionVendorAcceptButton' 'job'
    Snap 'open' -Frame
    foreach ($i in 0..4) { Pick "MissionVendorSummaryBox$i" "row$i" }
    for ($p = 1; $p -le 8; $p++) {
        $s = Send-FL 'scroll MissionVendorSummaryScroll 1'
        if ($s -notmatch 'pos=([\d.]+)' -or [int][double]$Matches[1] -lt $p) { break }
        Pick 'MissionVendorSummaryBox4' "scrolled$p" -Frame:($p -eq 1)
    }
    Close-Win 'MissionVendorCloseButton'
}
function Walk([string]$phase, [bool]$frames) {
    $script:phase = $phase; $script:shots = $frames
    Walk-Equipment; Walk-Trader; Walk-Ship; Walk-Job
}

# ---- the judgement: lines of one phase
function Judge-Role([string]$phase) {
    $out = @()
    foreach ($k in $roles.Keys) {
        $r = @($script:rows | Where-Object { $_.phase -eq $phase -and $_.ctl -match $roles[$k] })
        foreach ($g in ($r | Group-Object opening)) {
            $fonts = @($g.Group | Group-Object font | ForEach-Object { "$($_.Name)x$($_.Count)" })
            if ($fonts.Count -gt 1) { $out += "ROLE $($g.Group[0].win) ${k}: $($fonts -join ', ')" }
        }
    }
    $out
}
function Judge-Back([string]$phase) {
    $out = @()
    foreach ($o in @($script:rows | Where-Object { $_.phase -eq $phase -and $_.step -eq 'weapons_back' } | Select-Object -ExpandProperty opening -Unique)) {
        $a = @($script:rows | Where-Object { $_.phase -eq $phase -and $_.opening -eq $o -and $_.step -eq 'weapons' -and $_.ctl -match '^Item(Name|Price)\d+$' })
        foreach ($x in $a) {
            $y = $script:rows | Where-Object { $_.phase -eq $phase -and $_.opening -eq $o -and $_.step -eq 'weapons_back' -and $_.ctl -eq $x.ctl -and $_.parent -eq $x.parent } | Select-Object -First 1
            if ($y -and $y.font -ne $x.font) { $out += "BACK $($x.parent) $($x.ctl): $($x.font) $($x.w)x$($x.h) -> $($y.font) $($y.w)x$($y.h)" }
        }
    }
    $out
}
function Judge-Rich([string]$phase) {
    $out = @()
    foreach ($g in (@($script:rich | Where-Object { $_.phase -eq $phase }) | Group-Object opening)) {
        $s = @($g.Group | Group-Object rich | ForEach-Object { "$($_.Name)%x$($_.Count)" })
        if ($s.Count -gt 1) { $out += "RICH $($g.Group[0].win): $($s -join ', ')" }
    }
    $out
}
function Fonts-Of([string]$phase) {
    $h = @{}
    foreach ($r in $script:rows | Where-Object { $_.phase -eq $phase }) { $h["$($r.win)|$($r.step)|$($r.parent)|$($r.ctl)"] = "$($r.font)" }
    $h
}
function Judge-Same([string]$a, [string]$b, [string]$tag) {
    $x = Fonts-Of $a; $y = Fonts-Of $b; $out = @()
    foreach ($k in $x.Keys) { if ($y.ContainsKey($k) -and $y[$k] -ne $x[$k]) { $out += "$tag ${k}: $($x[$k]) -> $($y[$k])" } }
    Say "  $tag compared $(@($x.Keys | Where-Object { $y.ContainsKey($_) }).Count) texts of $a with $b"
    $out
}

$home_ = Get-FLDisplaySize
$failed = $false
try {
    foreach ($size in $Sizes) {
        $tag = ($size -replace '[ ,]+', 'x')
        $script:dir = Join-Path $top $tag
        New-Item -ItemType Directory -Force $script:dir | Out-Null
        $script:rows.Clear(); $script:rich.Clear(); $script:opening = 0
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Set-FLInstanceDisplay $Instance 'borderless' $size
        & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Modules @('subtitles', 'controller') -Save SaveC701.fl | Out-Null
        $null = Wait-FLFramesPassed 200
        foreach ($c in 'set deck_mode 2', 'set text_size 100', 'set hud_scale 100', 'set window_text 1') { $null = Send-FL $c }
        if (-not $NoPad) { Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30 }
        Say "== $size, device: $([regex]::Match((Send-FL 'state'), '(?m)^device=\S+').Value)"
        Walk 's100' $true
        Walk 's100v2' $false
        foreach ($c in 'set text_size 80', 'set hud_scale 80') { $null = Send-FL $c }
        Walk 's80' $true
        foreach ($c in 'set text_size 100', 'set hud_scale 100', 'set window_text 0') { $null = Send-FL $c }
        $null = Wait-FLFramesPassed 30
        Walk 'off' $true
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        $script:rows | Export-Csv -LiteralPath (Join-Path $script:dir 'texts.csv') -NoTypeInformation
        $script:rich | Export-Csv -LiteralPath (Join-Path $script:dir 'rich.csv') -NoTypeInformation

        $bug = @(Judge-Role 's100') + @(Judge-Back 's100') + @(Judge-Rich 's100')
        if ($script:labelsOff) { $bug += "LABELS $($script:labelsOff) step(s) with stats and values apart at the window's size" }
        $off = @(Judge-Role 'off') + @(Judge-Back 'off') + @(Judge-Rich 'off')
        $visit = @(Judge-Same 's100' 's100v2' 'VISIT')
        $slider = @(Judge-Same 's100' 's80' 'SLIDER')
        Say "== $size judged: $($bug.Count) finding(s) ROLE / BACK / RICH; controls: VISIT $($visit.Count), SLIDER $($slider.Count), OFF $($off.Count)"
        $bug + $visit + $slider + @($off | ForEach-Object { "OFF $_" }) | ForEach-Object { Say "  $_" }
        if ($bug.Count) { $failed = $true }
    }
} finally {
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Set-FLInstanceDisplay $Instance 'borderless' $home_
}
if ($failed) { Write-Host 'FAIL: text in the docked windows changes font or size with what the window holds (issue 22)'; exit 1 }
Write-Host 'PASS'
