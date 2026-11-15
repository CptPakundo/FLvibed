# The pad's moves on HUD 2's pages that no other net drives (audit 97_ "Not checked": the F5 hand-off in padbases.cpp PageBindsNow / UpdatePage;
# audit 101_ step 4): RS and a held D-pad move a docked text (LOG: read through `page`'s first= = the journal bar's position, see Row-Log), RS on the news / job board steps the page's own list, B in the gap between two
# NeuroNet pages closes the NeuroNet, B at the ship dealer's final stage steps back to the offers and a second B closes, and the jettison panel over
# INVENTORY is the pad's (RT / LT and the D-pad its quantity, Y all, B cancel). LT / RT on a dealer's quantity and A on the panel are base_pages.ps1's.
# Judged on the mod's own state: `page texts` (what the pages drew: a text that moved shows other lines), `nnpage`, `nnstrip`, `basepage`, the game's
# JettisonQuantitySlider (`scroll <name> 0`) and its hold (`equipment`). One background game, 3840x2160 borderless (the instance's size), pad form.
# Usage (PowerShell tool): tests\pad_page_moves.ps1 -Instance <n> [-Rows log,news,gap,final,jettison]    build\release (it needs `page texts`), no build.
# Output research\experience\runs\pad_page_moves\<time>\ (steps.txt, a frame per row). Exit 0 PASS, 1 a FAIL, 2 no FAIL but a row NOT RUN.
# What it cannot drive: whether the GAME's own text bar under the job board / news stays still (no reader of a rich text's bar); a real pad's stick
# curve (the virtual pad's -RY 1 is full deflection); the job board's text side (its offers are random: base_pages.ps1 masks them).
param([string]$Instance = '', [ValidateSet('log', 'news', 'gap', 'final', 'jettison')][string[]]$Rows = @('log', 'news', 'gap', 'final', 'jettison'))
$ErrorActionPreference = 'Stop'
if (-not $Instance) { throw 'name the instance: -Instance <n> (docs\TESTING.md "Parallel lanes")' }
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
$dir = Join-Path $root ("research\experience\runs\pad_page_moves\{0:yyyyMMdd_HHmmss}" -f (Get-Date))
New-Item -ItemType Directory -Force $dir | Out-Null
$steps = Join-Path $dir 'steps.txt'
$bad = [Collections.Generic.List[string]]::new(); $notRun = [Collections.Generic.List[string]]::new()
$crashSince = Get-Date   # (a crash recorder block written after this is the run's: Get-FLCrashBlocks at the end)
function Say([string]$l) { Write-Host $l; Add-Content -LiteralPath $steps $l }
function Check([bool]$ok, [string]$what) { Say ("{0,-5} {1}" -f $(if ($ok) { 'PASS' } else { 'FAIL' }), $what); if (-not $ok) { $bad.Add($what) } }
function NotRun([string]$what) { Say "NOT RUN $what"; $notRun.Add($what) }
function KV([string]$s) { $h = @{}; foreach ($m in [regex]::Matches($s, '(\w+)=(\S*)')) { if (-not $h.ContainsKey($m.Groups[1].Value)) { $h[$m.Groups[1].Value] = $m.Groups[2].Value } }; $h }
function NN { KV (Send-FL 'nnpage') }
function Strip { KV (Send-FL 'nnstrip') }
function Kind { $s = "$(Send-FL 'basepage')"; if ($s -match '^basepage (ships|repair|prices|board) ') { $Matches[1] } elseif ($s -match '^basepage final ') { 'final' } elseif ($s -match '^basepage kind=(\d+)') { @{ '0' = 'none'; '1' = 'commodity'; '2' = 'equipment' }[$Matches[1]] } else { 'unknown' } }
function Sel { [int][regex]::Match("$(Send-FL 'basepage')", ' sel=(-?\d+)').Groups[1].Value }
# the pages' drawn texts without their boxes' header line (`page texts`, pagerun.cpp)
function Texts { $t = "$(Send-FL 'page texts')"; ((($t -split "`n") | Select-Object -Skip 1) -join "`n") }
function Shot([string]$n) { try { $null = Save-FLShot -Name $n -Dir $dir } catch { Say "  (no frame ${n}: $($_.Exception.Message))" } }
# (true once the condition holds within the time, false when it did not: `until` itself is a PowerShell keyword)
function Holds([scriptblock]$c, [string]$what, [double]$sec = 8) { try { $null = Wait-FLUntil $c $what $sec; $true } catch { if ($_.Exception.Message -match 'not reached') { $false } else { throw } } }
function Gear { (@([regex]::Matches("$(Send-FL 'equipment')", 'arch=(\d+) id=\d+ count=(\d+)') | ForEach-Object { "$($_.Groups[1].Value)x$($_.Groups[2].Value)" } | Sort-Object)) -join ' ' }
# the page's text scroll (the page runtime's state, `page`: first = its first line shown; pagerun.cpp Pad scrolls it by the D-pad and RS)
function LogPos { $m = [regex]::Match("$(Send-FL 'page')", '^page id=\S+ .*? first=(-?\d+)'); if ($m.Success) { [int]$m.Groups[1].Value } else { $null } }
function Slider { $r = "$(Send-FL 'scroll JettisonQuantitySlider 0')"; [regex]::Match($r, 'pos=(\S+) range=(\S+)\.\.(\S+)') }
function Load([string]$save, [string]$mode) {
    Clear-FLPad
    Say "  $(Load-FLSave $save)"
    $null = Wait-FLUntil { (Get-FLMode) -eq $mode } "$mode after $save" 60
    if ($mode -eq 'docked') { Wait-FLRoomReady } else { $null = Wait-FLFramesPassed 30 }
    $null = Send-FL 'set input_mode 1'; $null = Wait-FLUntil { (Send-FL 'device') -match '^device=pad ' } 'the pad form' 10
}
function Close-NN { for ($i = 0; $i -lt 3 -and (Strip).standing -eq '1'; $i++) { Press-FLPad @('B'); $null = Holds { (Strip).standing -ne '1' } 'the NeuroNet closed' 6 } }
function Open-Spot([string]$room, [string]$spot, [string]$kind) {
    Wait-FLRoomReady
    if ((Send-FL 'hotspots') -notmatch "(?m)^$spot\s") { $null = Goto-FLRoom $room; Wait-FLRoomReady }
    $null = Wait-FLUntil { $x = Send-FL "goto $spot"; if ($x -match '^clicked') { $x } } "the room's $spot" 15
    $null = Wait-FLUntil { (Kind) -eq $kind } "the $kind page" 20
    $null = Wait-FLFramesPassed 60
}

# ---- rows
function Row-Log {
    $null = Send-FL 'nnstrip tab 0'
    if (-not (Holds { (NN).page -eq '1' } 'the LOG page' 15)) { NotRun 'log: the LOG page did not open'; return }
    $null = Wait-FLFramesPassed 60
    # The reader (main a745b1bd): LOG runs on the page runtime and `page` prints first= for it = the journal bar's position (the bar's own
    # unit: a second of RS moved it 0 -> 1170). `page texts` cannot judge it (the journal's text is drawn clipped, which it does not record).
    # A build whose `page` prints no first= for LOG is a NOT RUN (fails closed), never a pass.
    $t0 = LogPos
    if ($null -eq $t0) { Set-FLPad -RY -1; $null = Wait-FLFramesPassed 60; Clear-FLPad; Shot 'log_rs_down'; NotRun "log: this build's `page` prints no first= for LOG (frame log_rs_down)"; Close-NN; return }
    # RS held down for about a second (kPagePadRsDown: ry < -0.3): the bar must go on from the top; then RS up: it must come back
    Set-FLPad -RY -1; $null = Wait-FLFramesPassed 60; Clear-FLPad; $null = Wait-FLFramesPassed 20
    $t1 = LogPos
    Shot 'log_rs_down'
    Check ($null -ne $t1 -and $t1 -gt $t0) "log: RS held down moves the journal's text on (bar $t0 -> $t1)"
    if (-not ($null -ne $t1 -and $t1 -gt $t0)) { Close-NN; return }
    Set-FLPad -RY 1; $null = Wait-FLFramesPassed 60; Clear-FLPad; $null = Wait-FLFramesPassed 20
    $t2 = LogPos
    Shot 'log_rs_up'
    # (held as long as the way down, so it must come back at least half of the way: any small decrease is no pass)
    Check ($null -ne $t2 -and ($t1 - $t2) -ge ($t1 - $t0) / 2) "log: RS held up moves it back at least half of the way (bar $t0 -> $t1 -> $t2)"
    # the held D-pad: one step a press, then on while held (0.3 s, then about 14 steps a second: padbases.cpp kTextBarDelay / kTextBarStep)
    Set-FLPad -RY 1; $null = Wait-FLFramesPassed 120; Clear-FLPad; $null = Wait-FLFramesPassed 20   # (back to the top)
    $top = LogPos
    Set-FLPad -Buttons @('Down')
    $seen = [Collections.Generic.List[string]]::new(); $seen.Add("$top"); $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 1.5) { $t = "$(LogPos)"; if ($t -ne $seen[$seen.Count - 1]) { $seen.Add($t) }; $null = Wait-FLFramesPassed 2 }
    Clear-FLPad; $null = Wait-FLFramesPassed 20
    $moves = $seen.Count - 1
    Shot 'log_dpad_held'
    Check ($moves -ge 4) "log: a held D-pad keeps moving the text (bar positions seen in 1.5 s held: $moves; one press would be 1)"
    Close-NN
}
function Row-News {
    Open-Spot 'Bar' 'IDS_HOTSPOT_NEWSVENDOR' 'board'
    $s0 = Sel
    Set-FLPad -RY -1; $null = Wait-FLFramesPassed 40; Clear-FLPad; $null = Wait-FLFramesPassed 20
    $s1 = Sel
    Shot 'news_rs'
    Check ($s1 -ne $s0) "news: RS steps the page's own list (sel $s0 -> $s1; the game's text bar under it is not read: no reader)"
    Press-FLPad @('B'); $null = Holds { (Kind) -in 'none', 'unknown' } 'the news closed' 10
}
function Row-Gap {
    for ($try = 1; $try -le 3; $try++) {
        $null = Send-FL 'tap USER_INVENTORY'
        if (-not (Holds { (NN).page -eq '2' } 'the inventory page' 6)) { $null = Send-FL 'tap USER_INVENTORY'; $null = Holds { (NN).page -eq '2' } 'the inventory page' 10 }
        $null = Wait-FLFramesPassed 30
        $null = Send-FL 'nnstrip tab 0'   # INVENTORY -> LOG: the old window closes, the new one opens about a second later
        $inGap = Holds { $n = NN; $s = Strip; $n.page -eq '0' -and $s.standing -eq '1' } 'the gap between the two pages' 3
        if (-not $inGap) { Say "  gap try ${try}: the gap was not seen (page=$((NN).page))"; Close-NN; continue }
        Press-FLPad @('B')
        $closed = Holds { (Strip).standing -ne '1' -and (NN).page -eq '0' } 'the NeuroNet closed after B in the gap' 6
        $log = (NN).page -eq '1'
        Shot 'gap_b'
        Check ($closed -and -not $log) "gap: B between INVENTORY and LOG closes the NeuroNet (closed=$closed, LOG opened=$log, try $try)"
        Close-NN
        return
    }
    NotRun 'gap: the gap between two pages was not caught in 3 tries'
}
function Row-Final {
    # SaveC7PM.fl: rank 10 so the dealer sells (as base_pages.ps1's SaveC7BP), written before the game started (Make-PM below:
    # the game lists the saves at its start - 13 Nov 2026 a copy written here was not in the load screen)
    try {
        Load 'SaveC7PM.fl' 'docked'
        Open-Spot 'ShipDealer' 'IDS_HOTSPOT_SHIPDEALER' 'ships'
        Set-FLPad -Buttons @('A'); try { $null = Wait-FLUntil { (Kind) -eq 'final' } 'A held: the final stage' 8 } finally { Clear-FLPad; $null = Wait-FLFramesPassed 10 }
        $null = Wait-FLFramesPassed 60
        Press-FLPad @('B')
        $back = Holds { (Kind) -eq 'ships' } 'B: back to the offers' 8
        Shot 'final_b1'
        Check $back "final: B at the final stage steps back to the ship offers (now $(Kind))"
        if ($back) {
            $null = Wait-FLFramesPassed 30
            Press-FLPad @('B')
            $gone = Holds { (Kind) -in 'none', 'unknown' } 'a second B: closed' 8
            Check $gone "final: a second B closes the ship dealer (now $(Kind))"
        }
    } finally { }   # (SaveC7PM.fl is deleted at the run's end)
}
$pmSave = $null
function Make-PM {
    $src = Join-Path (Get-FLSaveDir) 'SaveC701.fl'; $script:pmSave = Join-Path (Get-FLSaveDir) 'SaveC7PM.fl'
    [IO.File]::WriteAllText($script:pmSave, [regex]::Replace([IO.File]::ReadAllText($src), '(?m)^rank = \d+', 'rank = 10'), [Text.Encoding]::ASCII)
}
function Row-Jettison {
    Load 'SaveC706.fl' 'space'
    $null = Send-FL 'tap USER_INVENTORY'
    if (-not (Holds { (NN).page -eq '2' } 'the inventory page' 6)) { $null = Send-FL 'tap USER_INVENTORY'; $null = Holds { (NN).page -eq '2' } 'the inventory page' 10 }
    for ($i = 0; $i -lt 6 -and (NN).filter -ne '1'; $i++) { $f = (NN).filter; Press-FLPad @('Right'); $null = Holds { (NN).filter -ne $f } 'the next filter' 5 }
    $null = Holds { (Send-FL 'visible JettisonButton') -match '^visible' } 'the game''s jettison button' 5
    if ((Send-FL 'visible JettisonButton') -notmatch '^visible') { NotRun 'jettison: no ammunition to jettison in SaveC706'; Close-NN; return }
    $g0 = Gear
    Press-FLPad @('Y')
    if (-not (Holds { (Send-FL 'visible JettisonAcceptButton') -match '^visible' } 'the jettison panel' 8)) { Check $false 'jettison: Y opens the panel'; Close-NN; return }
    $null = Wait-FLFramesPassed 30
    $m = Slider
    if (-not $m.Success) { NotRun "jettison: the panel's quantity bar was not read ($(Send-FL 'scroll JettisonQuantitySlider 0'))" }
    else {
        $p0 = [double]$m.Groups[1].Value; $hi = [double]$m.Groups[3].Value
        Say "  panel: quantity pos $p0, range $($m.Groups[2].Value)..$hi"
        if ($hi -le [double]$m.Groups[2].Value) { NotRun "jettison: one unit only (range $($m.Groups[2].Value)..$hi): the quantity moves cannot show" }
        else {
            Set-FLPad -RT 1; $null = Holds { [double](Slider).Groups[1].Value -ne $p0 } 'RT: the quantity moved' 4; Clear-FLPad; $null = Wait-FLFramesPassed 10
            $p1 = [double](Slider).Groups[1].Value
            Check ($p1 -ne $p0) "jettison: RT moves the panel's quantity ($p0 -> $p1)"
            Set-FLPad -LT 1; $null = Holds { [double](Slider).Groups[1].Value -ne $p1 } 'LT: the quantity moved' 4; Clear-FLPad; $null = Wait-FLFramesPassed 10
            $p2 = [double](Slider).Groups[1].Value
            Check ($p2 -ne $p1) "jettison: LT moves it back ($p1 -> $p2)"
            Press-FLPad @('Right'); $null = Holds { [double](Slider).Groups[1].Value -ne $p2 } 'D-pad right: the quantity moved' 3
            $p3 = [double](Slider).Groups[1].Value
            Check ($p3 -ne $p2) "jettison: the D-pad moves the quantity ($p2 -> $p3)"
            # all = the bar's last position, which RT held cannot pass (the game's bar reads range 1..count+1 with its last
            # position at count: 13 Nov 2026, 20 torpedoes read range 1.0..21 and the page drew 20 of 1..20)
            Press-FLPad @('Y'); $null = Holds { [double](Slider).Groups[1].Value -gt $p3 } 'Y: all' 3
            $p4 = [double](Slider).Groups[1].Value
            Set-FLPad -RT 1; $null = Wait-FLFramesPassed 40; Clear-FLPad; $null = Wait-FLFramesPassed 10
            $p5 = [double](Slider).Groups[1].Value
            Check ($p4 -gt $p3 -and $p5 -eq $p4) "jettison: Y sets all ($p3 -> $p4; RT held after it: $p5, so $p4 is the last position)"
        }
    }
    Shot 'jettison_panel'
    Press-FLPad @('B')
    $shut = Holds { (Send-FL 'visible JettisonAcceptButton') -notmatch '^visible' } 'B: the panel closed' 6
    $null = Wait-FLFramesPassed 30
    $still = (NN).page -eq '2'
    Close-NN
    $null = Wait-FLFramesPassed 60   # (the world runs again after the close: a jettison would show in the hold now)
    # (waits on the state, not on the frames: the equipment list read right after the close can still be mid-update on a loaded machine - a hold that
    # stays changed for 6 s is a jettison that happened)
    $null = Holds { (Gear) -eq $g0 } 'the hold as it was before the panel' 6
    $g1 = Gear
    # (a changed hold names what changed: arch x count before and after; the gate of 15 Nov 2026 failed here with no way to tell a jettison from a misread)
    $diff = if ($g1 -ne $g0) { $a = @($g0 -split ' '); $b = @($g1 -split ' '); "; before only: $(@($a | Where-Object { $_ -notin $b }) -join ' '); after only: $(@($b | Where-Object { $_ -notin $a }) -join ' ')" } else { '' }
    Check ($shut -and $still -and $g1 -eq $g0) "jettison: B cancels the panel (closed=$shut, INVENTORY still open=$still, hold unchanged=$($g1 -eq $g0)$diff)"
}

# ---- run
$build = try { Get-Content (Join-Path $root 'build\release\build_info.txt') -TotalCount 1 } catch { 'unknown' }
Say "pad_page_moves on instance $Instance, build $build, rows $($Rows -join ',')"
try {
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
    if ('final' -in $Rows) { Make-PM }
    & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save 'SaveC701.fl' | Select-Object -Last 1 | ForEach-Object { Say "  $_" }
    $null = Wait-FLUntil { (Get-FLMode) -eq 'docked' } 'docked at Manhattan' 90
    $null = Send-FL 'fpscap 60'; $null = Send-FL 'set hud_fade 0'
    # HUD 2 whatever the main install's settings file says (its skin and its pages come with it: ui_style and base_pages were retired 13 Nov 2026)
    $null = Send-FL 'set hud_style 2'
    $arm = Send-FL 'page texts'
    if ($arm -notmatch 'armed|^page texts n=') { throw "the build has no 'page texts' verb ($arm)" }
    Wait-FLRoomReady
    $null = Send-FL 'set input_mode 1'; $null = Wait-FLUntil { (Send-FL 'device') -match '^device=pad ' } 'the pad form' 10
    # (news first: the board's own rows exist in the game's first load)
    foreach ($r in @('news', 'log', 'gap', 'final', 'jettison')) {
        if ($r -notin $Rows) { continue }
        Say "--- $r"
        try { & "Row-$((Get-Culture).TextInfo.ToTitleCase($r))" } catch { Check $false "${r}: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))"; try { Clear-FLPad; Close-NN } catch { } }
    }
} finally {
    try { if (Get-FLGameProcess) { Clear-FLPad; $null = Send-FL 'set input_mode 0' } } catch { }
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    # (the game log goes with the frames: the next net's game start overwrites the install's copy)
    try { Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue; Copy-Item -LiteralPath (Join-Path (Get-FLInstallDir) 'EXE\FLController.log') -Destination $dir -ErrorAction Stop } catch { Say "  (game log not kept: $($_.Exception.Message))" }
    if ($pmSave -and (Test-Path -LiteralPath $pmSave)) { [IO.File]::Delete($pmSave) }
}
foreach ($c in @(Get-FLCrashBlocks $crashSince)) { Check $false "a game crashed during the run (EXE\FLvibed_crash.log):`n$c" }
if ($bad.Count) { Say "pad_page_moves: $($bad.Count) FAIL ($dir)"; exit 1 }
if ($notRun.Count) { Say "pad_page_moves: no FAIL, $($notRun.Count) NOT RUN ($dir)"; exit 2 }
Say "pad_page_moves: PASS ($dir)"
exit 0
