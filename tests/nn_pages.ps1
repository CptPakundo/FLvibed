# The NeuroNet pages walked (docs\NEURONET_PLAN.md, modules\hud\src\nnpage.* and nnstrip.*): the net the HUD 2 restructure packages are checked with.
# One background game per display size, the docked save and the space save loaded in it, each walked in the pad form (input_mode 1, the virtual
# pad's real presses through padbases) and the keyboard-and-mouse form (input_mode 2, clicks on HUD 2's own hotspots through the game's button path:
# `uibtn click`). Every tab the strip offers (LOG, MAP, INVENTORY, STATUS, INFO; CHAT and GROUP are multiplayer only) and the sub-tabs and filters:
# INVENTORY's five filters, a row walk to the list's end and (docked) a mount and back, STATUS's four reputation filters, its sort and a walk, INFO's
# SHOW ON MAP, LOG's MISSION / JOURNAL, the strip's wrap and a double step (the pad's held press), the close.
# Asserted at each step on what the mod reports (nnpage, nnstrip, nnstrip rooms, bases' promptbar, uibtn's hotspots, the game's own fields by peek):
# the page and the strip's tab are the ones asked for, the focus stays inside the rows and stops at the ends, the game's filter / log tab / card follow
# the page, the pad's prompts are the page's for that state, the mouse's chips offer what the pad's prompts offer, the world pauses in space only, the
# strip stands centred, every hotspot stands on screen and none overlaps another. Frames for the run-judge and every reading in
# research\experience\runs\nn_pages\<time> (checks.txt, steps.txt, <n>_<size>_<place>_<form>_<step>.jpg).
# Usage (PowerShell tool): tests\nn_pages.ps1 [-Instance 5] [-Sizes 3840x2160,1280x800] [-Forms pad,kbm] [-Places docked,space]   build\release, no build.
# Exit 0 = every check passed, 1 = a check failed (or the walk could not go on).
param([string]$Instance = '', [string[]]$Sizes = @('3840x2160', '1280x800'), [ValidateSet('pad', 'kbm')][string[]]$Forms = @('pad', 'kbm'),
      [ValidateSet('docked', 'space')][string[]]$Places = @('docked', 'space'), [string]$DockedSave = 'SaveC701.fl', [string]$SpaceSave = 'SaveC706.fl')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $Instance) { throw 'name the game instance: -Instance <n> (no default: a default instance belongs to no checkout - docs\TESTING.md "Parallel lanes")' }
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
Add-Type -AssemblyName System.Drawing
$dir = Join-Path $root ("research\experience\runs\nn_pages\{0:yyyyMMdd_HHmmss}" -f (Get-Date))
New-Item -ItemType Directory -Force $dir | Out-Null
$checks = Join-Path $dir 'checks.txt'; $steps = Join-Path $dir 'steps.txt'
$script:pass = 0; $script:failed = [Collections.Generic.List[string]]::new(); $script:ctx = ''; $script:shot = 0; $script:W = 0; $script:H = 0
$total = [Diagnostics.Stopwatch]::StartNew()
$crashSince = Get-Date   # (a crash recorder block written after this is the run's: Get-FLCrashBlocks at the end)
# (nothing here writes to the pipeline: a function's output is its return value - tools\claude\rules\powershell.md)
function Say([string]$line) { Write-Host $line; Add-Content -LiteralPath $checks $line }
function Check([bool]$ok, [string]$what) {
    $line = '{0} [{1}] {2}' -f $(if ($ok) { 'PASS' } else { 'FAIL' }), $script:ctx, $what
    if ($ok) { $script:pass++ } else { $script:failed.Add($line) }
    Say $line
}
# A fault the release at HEAD has, written down (not hidden): reported KNOWN, not failed; a pass there says it was fixed - then make it a plain Check.
$script:known = [Collections.Generic.List[string]]::new()
function Known([bool]$ok, [string]$what, [string]$why) {
    if ($ok) { Check $true $what; return }
    $line = 'KNOWN [{0}] {1} - {2}' -f $script:ctx, $what, $why
    $script:known.Add($line); Say $line
}
# A wait whose outcome is a check: $true when the condition came, $false at the safety limit (the walk goes on to its next check)
function Until([scriptblock]$cond, [string]$what, [double]$sec = 5) { try { $null = Wait-FLUntil $cond $what $sec; $true } catch { if ($_.Exception.Message -match 'not reached') { $false } else { throw } } }

# ---- what the mod reports, parsed
function KV([string]$s) { $h = @{}; foreach ($m in [regex]::Matches($s, '(\w+)=(\S*)')) { if (-not $h.ContainsKey($m.Groups[1].Value)) { $h[$m.Groups[1].Value] = $m.Groups[2].Value } }; $h }
function NN { KV (Send-FL 'nnpage') }
function Strip { KV (Send-FL 'nnstrip') }
function Bar {
    $b = Send-FL 'bases'
    $first = KV (($b -split "`n")[0]); $p = ($b -split 'promptbar ', 2)[1]
    $h = KV $p
    $h.items = @(([regex]::Match("$p", "items='([^']*)'").Groups[1].Value -split ', ') | Where-Object { $_ })
    $h.where = $first.where; $h.gameFilter = $first.gameFilter; $h.padScreen = $first.screen
    $h
}
function Hots {
    @(foreach ($l in ((Send-FL 'uibtn') -split "`n")) {
        $m = [regex]::Match($l, '^\s*(\d+) [0-9A-F]+ want \((-?\d+),(-?\d+)\)-\((-?\d+),(-?\d+)\)')
        if ($m.Success) { [pscustomobject]@{ Slot = [int]$m.Groups[1].Value; X0 = [int]$m.Groups[2].Value; Y0 = [int]$m.Groups[3].Value; X1 = [int]$m.Groups[4].Value; Y1 = [int]$m.Groups[5].Value } }
    })
}
# The strip's words and plates as nnstrip draws them (`nnstrip rooms`): the tab words are nntabs.h kTabs' (single player: five)
$kTabWords = @('LOG', 'MAP', 'INVENTORY', 'STATUS', 'INFO', 'CHAT', 'GROUP')
function Spots {
    $words = [Collections.Generic.List[object]]::new(); $plates = [Collections.Generic.List[object]]::new()
    foreach ($l in ((Send-FL 'nnstrip rooms') -split "`n")) {
        $m = [regex]::Match($l, '^(word|plate) (-?\d+),(-?\d+)-(-?\d+),(-?\d+) ctl=\S+(?: "([^"]*)")?')
        if (-not $m.Success) { continue }
        $o = [pscustomobject]@{ Word = $m.Groups[6].Value; X0 = [int]$m.Groups[2].Value; Y0 = [int]$m.Groups[3].Value; X1 = [int]$m.Groups[4].Value; Y1 = [int]$m.Groups[5].Value }
        if ($m.Groups[1].Value -eq 'word') { if ($o.Word -in $kTabWords) { $words.Add($o) } } else { $plates.Add($o) }
    }
    [pscustomobject]@{ Words = @($words); Plates = @($plates) }
}
function Near($a, $b) { [Math]::Abs($a.X0 - $b.X0) -le 3 -and [Math]::Abs($a.Y0 - $b.Y0) -le 3 -and [Math]::Abs($a.X1 - $b.X1) -le 3 -and [Math]::Abs($a.Y1 - $b.Y1) -le 3 }
# HUD 2's hotspots sorted by what the mod says they are: a strip word's (its rectangle is the word's), CLOSE (the strip's other plate, in the lane),
# the lane's chips (CLOSE's row), the page's tabs (a row of two or more, the topmost), its rows (the rest, top to bottom)
function Layout {
    $h = Hots; $s = Spots
    $strip = @{}; $used = @{}
    foreach ($w in $s.Words) { $hit = $h | Where-Object { Near $_ $w } | Select-Object -First 1; if ($hit) { $strip[$w.Word] = $hit.Slot; $used[$hit.Slot] = 1 } }
    $box = $s.Plates | Where-Object { $s.Words.Count -and [Math]::Abs($_.Y0 - $s.Words[0].Y0) -le 3 } | Select-Object -First 1
    $close = $null
    foreach ($p in ($s.Plates | Where-Object { $_ -ne $box })) { $hit = $h | Where-Object { Near $_ $p } | Select-Object -First 1; if ($hit) { $close = $hit; $used[$hit.Slot] = 1 } }
    $lane = @(if ($close) { $h | Where-Object { -not $used.ContainsKey($_.Slot) -and [Math]::Abs($_.Y0 - $close.Y0) -le 3 } | Sort-Object X0 })
    foreach ($c in $lane) { $used[$c.Slot] = 1 }
    $rest = @($h | Where-Object { -not $used.ContainsKey($_.Slot) })
    $tabs = @($rest | Group-Object { [int][Math]::Round($_.Y0 / 4) } | Where-Object Count -ge 2 | Sort-Object { $_.Group[0].Y0 } | Select-Object -First 1 | ForEach-Object { $_.Group } | Sort-Object X0)
    $tabSlots = @($tabs | ForEach-Object Slot)
    $rows = @($rest | Where-Object { $_.Slot -notin $tabSlots } | Sort-Object Y0)
    [pscustomobject]@{ Hots = $h; Spots = $s; Strip = $strip; Box = $box; Close = $close; Lane = $lane; Tabs = $tabs; Rows = $rows }
}

# ---- the game's own values the page acts on, and the tab table (nntabs.h: the game's command ids; nnpage.cpp's Page enum; padbases' screen names)
$kCmd = @{ LOG = '0x67'; MAP = '0x63'; INVENTORY = '0x66'; STATUS = '0x65'; INFO = '0x6a' }
$kPage = @{ LOG = '1'; MAP = '0'; INVENTORY = '2'; STATUS = '3'; INFO = '4' }
$kBarScreen = @{ LOG = 'missionlog'; INVENTORY = 'inventory'; STATUS = 'playerinfo'; INFO = 'help' }
$kFilterWords = @('ALL', 'ENEMIES', 'INDIFFERENT', 'FRIENDS') # (nnpage.cpp FilterWord, the X prompt's cycle)
# One byte of a game window's field (`peek`; the log's tab at +0x37C is a byte: the bytes above it are another field's)
function Peek8([string]$root, [int]$off) {
    $a = '0x{0:x}' -f ([Convert]::ToUInt32($root.Substring(2), 16) + $off)
    [Convert]::ToInt32((((Send-FL "peek $a 1") -split ':', 2)[1].Trim() -split '\s+')[0], 16)
}
# nnrules::List.Fit's first shown row after the focus moved to `sel` (a row of lead when more than two rows stand; tests\nnpage_test.cpp holds the rule)
function Fit-First([int]$sel, [int]$first, [int]$count, [int]$rows) {
    $lead = if ($rows -gt 2) { 1 } else { 0 }
    if ($sel - $lead -lt $first) { $first = $sel - $lead }
    if ($sel + $lead -ge $first + $rows) { $first = $sel + $lead - $rows + 1 }
    [Math]::Max(0, [Math]::Min($first, [Math]::Max(0, $count - $rows)))
}

# ---- frames and readings for the run-judge
function Shot([string]$name) {
    $script:shot++
    $base = '{0:d3}_{1}_{2}' -f $script:shot, ($script:ctx -replace '[^\w]+', '_'), $name
    Add-Content -LiteralPath $steps ("=== $base`n" + (Send-FL 'nnpage') + "`n" + (Send-FL 'nnstrip') + "`n" + ((Send-FL 'bases') -split "`n" | Select-Object -First 2 | Out-String) +
                                    (((Send-FL 'uibtn') -split "`n" | Where-Object { $_ -match 'hud2 hotspots| want ' }) -join "`n"))
    $png = Save-FLShot -Name $base -Dir $dir
    $img = [Drawing.Image]::FromFile($png)
    try {
        $enc = [Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -eq 'image/jpeg'
        $ps = New-Object Drawing.Imaging.EncoderParameters 1
        $ps.Param[0] = New-Object Drawing.Imaging.EncoderParameter ([Drawing.Imaging.Encoder]::Quality), 90L
        $img.Save((Join-Path $dir "$base.jpg"), $enc, $ps)
    } finally { $img.Dispose() }
    [IO.File]::Delete($png)
}

# ---- waits (state only; each calls Step-FLPopups through Wait-FLUntil / Wait-FLFramesPassed)
function Settle { $null = Wait-FLFramesPassed 4 } # (the hotspots and the lane are the last drawn frame's: one draw after the state)
function Wait-Tab([string]$word, [string]$why) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $null = Wait-FLUntil {
        $s = Strip; $n = NN
        $s.open -eq $kCmd[$word] -and $s.held -eq '0x0' -and $s.standing -eq '1' -and $n.page -eq $kPage[$word] -and
        ($word -ne 'STATUS' -or ([int]$n.reps -gt 0 -and $n.s0)) -and # (STATUS: the server's answer is in)
        ($word -ne 'MAP' -or (Send-FL 'navnew state') -match ' on=1 grown=1 transform=1') # (MAP: the map has no page; it grows to its own ground as the slides end, ~1.2 s)
    } "$word ($why)" 20
    Settle
    $sw.ElapsedMilliseconds
}
function Set-Form([string]$form) {
    $null = Send-FL "set input_mode $(if ($form -eq 'pad') { 1 } else { 2 })"
    $want = if ($form -eq 'pad') { 'device=pad' } else { 'device=kbm' }
    $null = Wait-FLUntil { (Send-FL 'device') -match "^$want " } "input form $form" 10
    Settle
}
function Press([string[]]$b) { Press-FLPad $b }
# A press as Press-FLPad, and how long the button stood down by the wall clock (an upper bound of what the game saw): at or past $kHoldLimit
# the lists' repeat (0.3 s, pageinput.h kListRepeat) may have stepped on, as for a player's hold
# (released as soon as $moved holds - the page took the press - or after two frames as Press-FLPad: a press stands down about one frame
# then, far from the repeat; the verifier measured 0.20-0.30 s for two frames plus the release on a loaded machine)
$kHoldLimit = 0.25
function TimedPress([string]$b, [scriptblock]$moved = $null) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    Set-FLPad -Buttons @($b)
    for ($i = 0; $i -lt 2; $i++) { $null = Wait-FLFramesPassed 1; if ($moved -and (& $moved)) { break } }
    Set-FLPad -Buttons @()
    $t = $sw.Elapsed.TotalSeconds
    $null = Wait-FLFramesPassed 2
    $t
}
function Click([int]$slot, [string]$what) {
    $r = Send-FL "uibtn click $(100 + $slot)"
    Check ($r -eq 'clicked') "the mouse's click on $what (hotspot $slot): $r"
}

# ---- what every step asserts
function Check-Common([string]$word, [string]$form, [string]$place) {
    # (the pad's lane follows padbases' screen read and the lane's settle - promptfit kLaneSettleMs / kLaneMaxWaitMs: it may stand a few frames behind the page)
    $laneCame = if ($form -eq 'pad' -and $kBarScreen[$word]) { Until { $bb = Bar; $bb.visible -eq '1' -and $bb.screen -eq $kBarScreen[$word] } "the lane for $word" 2 } else { $true }
    $s = Strip; $n = NN; $b = Bar; $L = Layout
    $order = @($L.Spots.Words | Sort-Object X0 | ForEach-Object Word)
    Check (($order -join ',') -eq 'LOG,MAP,INVENTORY,STATUS,INFO') "the strip offers the single player's tabs in the game's order ($($order -join ','))"
    $idx = [Array]::IndexOf($order, $word)
    Check ($s.tab -eq "$idx" -and $s.open -eq $kCmd[$word] -and $s.held -eq '0x0') "the strip's tab is $word (tab=$($s.tab) want $idx, open=$($s.open) want $($kCmd[$word]), held=$($s.held))"
    Check ($n.page -eq $kPage[$word]) "the page up is $word's (page=$($n.page) want $($kPage[$word]))"
    $padFlag = if ($form -eq 'pad') { '1' } else { '0' }
    Check ($s.pad -eq $padFlag -and $n.pad -eq $padFlag) "the page and the strip are in the $form form (strip pad=$($s.pad), page pad=$($n.pad))"
    $r = $s.rect -split ',' | ForEach-Object { [double]$_ }
    Check ($r.Count -eq 4 -and $r[0] -ge 0 -and $r[2] -le $script:W -and $r[1] -ge 0 -and [Math]::Abs(($r[0] + $r[2]) / 2 - $script:W / 2) -le 2) "the strip stands on screen, centred ($($s.rect) on $($script:W)x$($script:H))"
    $missing = @($order | Where-Object { -not $L.Strip.ContainsKey($_) })
    Check ($missing.Count -eq 0) "every strip word is clickable: a hotspot on each word ($(if ($missing) { 'missing ' + ($missing -join ',') } else { 'all 5' }))"
    $off = @($L.Hots | Where-Object { $_.X0 -lt 0 -or $_.Y0 -lt 0 -or $_.X1 -gt $script:W -or $_.Y1 -gt $script:H })
    $over = @(for ($i = 0; $i -lt $L.Hots.Count; $i++) { for ($j = $i + 1; $j -lt $L.Hots.Count; $j++) {
        $a = $L.Hots[$i]; $c = $L.Hots[$j]
        $ox = [Math]::Min($a.X1, $c.X1) - [Math]::Max($a.X0, $c.X0); $oy = [Math]::Min($a.Y1, $c.Y1) - [Math]::Max($a.Y0, $c.Y0)
        if ($ox -gt 2 -and $oy -gt 2) { "$($a.Slot)/$($c.Slot)" } } })
    Check ($off.Count -eq 0 -and $over.Count -eq 0) "$($L.Hots.Count) hotspots, all on screen, none over another$(if ($off) { '; off screen: ' + (($off | ForEach-Object Slot) -join ',') })$(if ($over) { '; overlapping: ' + ($over -join ' ') })"
    if ($form -eq 'pad') {
        Check (-not $L.Close) 'the pad form has no CLOSE chip (B closes)'
        if ($word -ne 'MAP') {
            Check ($laneCame -and $b.visible -eq '1' -and $b.screen -eq $kBarScreen[$word]) "the prompt lane stands for $word within 2 s (screen=$($b.screen) visible=$($b.visible))"
            # (a page's lane takes every prompt it sets, by the page rule - the width first, a second row, a prompt giving way last and never B:
            # promptfit PageRule, the owner, 12 Nov 2026; at 16:9 and 16:10 every page's list fits one row)
            Check ($b.items -contains 'B CLOSE') "the lane shows B CLOSE: '$($b.items -join ', ')'"
            Check ($b.page -eq '1' -and $b.dropped -eq '0') "the lane takes the page's rule and drops no prompt (promptbar page=$($b.page) dropped=$($b.dropped) lane=$($b.lane))"
        }
    } else {
        Check ($b.visible -ne '1') "the keyboard-and-mouse form shows no pad lane (visible=$($b.visible))"
        Check ([bool]$L.Close) "the CLOSE chip stands in the lane and is clickable$(if ($L.Close) { " (hotspot $($L.Close.Slot) at $($L.Close.X0),$($L.Close.Y0))" })"
    }
    $paused = if ($word -eq 'MAP') { $null } elseif ($place -eq 'space') { '1' } else { '0' }
    if ($paused) { Check ($n.paused -eq $paused) "the world is $(if ($paused -eq '1') { 'paused in space' } else { 'running docked' }) under the page (paused=$($n.paused))" }
    [pscustomobject]@{ Strip = $s; NN = $n; Bar = $b; L = $L }
}
# The pad's prompts this state should show (nnpage.cpp Prompts, padbases Prompts' B CLOSE), from the page's own state and the game's
function Check-PadPrompts([string]$word, $st, [string]$place) {
    $it = @($st.Bar.items); $n = $st.NN
    switch ($word) {
        'LOG' {
            $extra = @($it | Where-Object { $_ -notin 'DL+DR MISSION / JOURNAL', 'DU+DD SCROLL', 'B CLOSE', 'Y SHOW ON MAP' })
            Check (($it -contains 'DL+DR MISSION / JOURNAL') -and ($it -contains 'DU+DD SCROLL') -and $extra.Count -eq 0) "LOG's prompts: the two tabs, scroll, (show on map), close: '$($it -join ', ')'"
        }
        'INFO' {
            # SCROLL only when the text is longer than the page (the owner, 12 Nov 2026): the page's own lines and room (`nnpage info` [PAGE])
            $pg = [regex]::Match((Send-FL 'nnpage info'), 'lines=(\d+) first=\d+ room=(\d+)')
            $scrolls = $pg.Success -and [int]$pg.Groups[1].Value -gt [int]$pg.Groups[2].Value
            $want = if ($scrolls) { 'DU+DD SCROLL, Y SHOW ON MAP, B CLOSE' } else { 'Y SHOW ON MAP, B CLOSE' }
            Check ($pg.Success -and ($it -join ', ') -eq $want) "INFO's prompts: scroll only with more lines than room ($(if ($pg.Success) { "lines=$($pg.Groups[1].Value) room=$($pg.Groups[2].Value)" } else { 'no [PAGE] line' })), show on map, close: '$($it -join ', ')'"
        }
        'STATUS' {
            $ok = $it.Count -eq 4 -and $it[0] -eq 'DU+DD FACTION' -and $it[1] -match '^X SHOW (ALL|ENEMIES|INDIFFERENT|FRIENDS)$' -and $it[2] -match '^Y SORT BY (NAME|STANDING)$' -and $it[3] -eq 'B CLOSE'
            Check $ok "STATUS's prompts: faction, the next filter, the sort, close: '$($it -join ', ')'"
        }
        'INVENTORY' {
            $f = [int]$n.filter; $items = [int]$n.items
            $jet = (Send-FL 'visible JettisonButton') -match '^visible'
            $bad = @()
            if ($it -notcontains 'DL+DR FILTER') { $bad += 'no DL+DR FILTER' }
            if ($it -notcontains 'RS SCROLL CARD') { $bad += 'no RS SCROLL CARD' }
            if ($it -notcontains 'B CLOSE') { $bad += 'no B CLOSE' }
            if (($items -gt 1) -ne [bool]($it -contains 'DU+DD ITEM')) { $bad += "ITEM with $items rows" }
            $a = @($it | Where-Object { $_ -match '^A (UN)?MOUNT$' })
            if ($a -and ($place -eq 'space' -or $f -notin 0, 2, 3 -or $items -eq 0)) { $bad += "A MOUNT where nothing mounts (filter $f, $place)" }
            if (($it | Where-Object { $_ -match '^Y ' }) -and -not $jet) { $bad += 'Y JETTISON with no jettison button' }
            if (($it -contains 'X BEST PRICES') -and $f -ne 4) { $bad += "BEST PRICES on filter $f" }
            $known = @($it | Where-Object { $_ -notin 'DL+DR FILTER', 'DU+DD ITEM', 'RS SCROLL CARD', 'B CLOSE', 'X BEST PRICES' -and $_ -notmatch '^A (UN)?MOUNT$|^Y (AGAIN TO )?JETTISON$|^R3 CLICK (UNTAG|COMPARE WITH)$' })
            if ($known) { $bad += "unknown: $($known -join ' / ')" }
            Check ($bad.Count -eq 0) "INVENTORY's prompts for filter $f, $items rows, $place$(if ($jet) { ', jettison button' }): '$($it -join ', ')'$(if ($bad) { ' - ' + ($bad -join '; ') })"
        }
    }
}
# The mouse's chips against the pad's prompts in the same state (NEURONET_PLAN "the actions are chips in the lane"): the pad's view read with the form switched
# for a moment. INVENTORY's A / X / Y / R3, STATUS's X (its sort has no chip), LOG's and INFO's Y.
function Check-Parity([string]$word, $st) {
    $it = @(PadView $kBarScreen[$word])
    $L = Layout
    $keys = switch ($word) { 'INVENTORY' { '^(A|X|Y|R3) ' } 'STATUS' { '^X ' } default { '^Y ' } }
    $want = @($it | Where-Object { $_ -match $keys })
    # (the pad's lane takes every prompt the page sets: no compare click lost to a cap since the page rule, 12 Nov 2026)
    Check ($L.Lane.Count -eq $want.Count) "the mouse's lane has a chip for each of the pad's actions on $word ($($L.Lane.Count) chips; pad: '$($want -join ', ')')"
    $L
}

# ---- the pages
function Walk-Inventory([string]$form, [string]$place) {
    $st = Check-Common 'INVENTORY' $form $place
    if ($form -eq 'pad') { Check-PadPrompts 'INVENTORY' $st $place } else { $null = Check-Parity 'INVENTORY' $st }
    Shot 'inventory'
    $start = [int]$st.NN.filter
    $counts = @{}
    foreach ($k in 1..5) {
        $want = ($start + $k) % 5
        if ($form -eq 'pad') { Press @('Right') } else { $L = Layout; Check ($L.Tabs.Count -eq 5) "INVENTORY's five filter tabs are clickable ($($L.Tabs.Count))"; if ($L.Tabs.Count -eq 5) { Click $L.Tabs[$want].Slot "filter tab $want" } }
        $null = Wait-FLUntil { $n = NN; $n.filter -eq "$want" -and (Bar).gameFilter -eq "$want" } "filter $want" 10
        Settle
        $n = NN; $L = Layout; $items = [int]$n.items; $sel = [int]$n.itemSel
        $counts[$want] = $items
        $shown = @(($n.shownRows -split ',') | Where-Object { $_ -ne '' })
        Check ($n.filter -eq (Bar).gameFilter) "filter $want is the game's own (page $($n.filter), game $((Bar).gameFilter))"
        Check (($items -eq 0 -and $sel -eq 0) -or ($sel -ge 0 -and $sel -lt $items)) "filter ${want}: the focus is inside the rows (itemSel=$sel of $items)"
        Check ($L.Rows.Count -eq [Math]::Min(5, $items) -and $shown.Count -eq [Math]::Min(5, $items)) "filter ${want}: five rows at most stand and are clickable ($($L.Rows.Count) hotspots, $($shown.Count) icon rows, $items items)"
        if ($form -eq 'pad') { $st = [pscustomobject]@{ NN = $n; Bar = (Bar) }; Check-PadPrompts 'INVENTORY' $st $place }
        Shot "inventory_filter$want"
    }
    Check ((NN).filter -eq "$start") "five filter steps come round to where they started ($start)"
    if ($form -eq 'pad') {
        Press @('Left'); $back = ($start + 4) % 5
        Check (Until { (NN).filter -eq "$back" -and (Bar).gameFilter -eq "$back" } "filter $back by D-pad left") "D-pad left steps back to filter $back"
        Press @('Right'); $null = Wait-FLUntil { (NN).filter -eq "$start" } "filter $start again" 10
    }
    # the row walk on the fullest filter
    $full = ($counts.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 1).Key
    Go-Filter $form $full
    $null = Wait-FLUntil { (NN).filter -eq "$full" -and (Bar).gameFilter -eq "$full" } "filter $full for the walk" 10
    Settle
    $items = [int](NN).items
    if ($form -eq 'pad') {
        # (each press's hold is timed: a press this script released past the lists' repeat delay - PowerShell held up on a loaded machine while the
        # game ran on - is a hold, which may step on by the repeat as a player's hold does; such a press is not judged as one press, the walk's end is.
        # 13 Nov 2026: "0 6 7 8 ..", "0 1 4 6 .." beside other games, passing alone; pageinput.h kListRepeat 0.3 s)
        $null = Until { (NN).syncPending -eq '0' } 'the selection settled before the walk'
        $seq = @([int](NN).itemSel); $held = @()
        for ($k = 0; $k -le $items; $k++) { $p = $seq[-1]; $held += TimedPress 'Down' { [int](NN).itemSel -ne $p }; $seq += [int](NN).itemSel }
        $long = @(for ($k = 0; $k -lt $held.Count; $k++) { if ($held[$k] -ge $kHoldLimit) { $k } })
        $steps1 = @(for ($k = 1; $k -lt $seq.Count; $k++) { if (($k - 1) -notin $long) { $seq[$k] - $seq[$k - 1] } })
        $bigOk = -not @(for ($k = 1; $k -lt $seq.Count; $k++) { if (($k - 1) -in $long -and $seq[$k] -lt $seq[$k - 1]) { 1 } })   # (a hold moves on, never back)
        $ok = $seq[-1] -eq $items - 1 -and $seq[-2] -eq $items - 1 -and -not ($steps1 | Where-Object { $_ -notin 0, 1 }) -and $bigOk -and ($long.Count -or @($steps1 | Where-Object { $_ -eq 1 }).Count -eq $items - 1 - $seq[0])
        $heldNote = if ($long.Count) { "; presses $($long -join ',') held past $kHoldLimit s by this script ($((@($long | ForEach-Object { '{0:N2}' -f $held[$_] })) -join ' ') s): not judged as one press" } else { '' }
        Check $ok "D-pad down walks filter $full one row a press and stops at the last ($($seq -join ' '); $items rows$heldNote)"
        # (a walk mostly of long presses judges little but its end: not a pass - re-run it, beside fewer games)
        Check ($long.Count * 2 -le $held.Count) "the walk's presses were judged one by one: $($held.Count - $long.Count) of $($held.Count) under $kHoldLimit s"
        Shot 'inventory_walk_end'
        if ($items -ge 2) {
            $p = [int](NN).itemSel; $h = TimedPress 'Up' { [int](NN).itemSel -ne $p }; $at = [int](NN).itemSel
            if ($h -ge $kHoldLimit) { Check ($at -le $items - 2) "D-pad up held $('{0:N2}' -f $h) s by this script: it moves back ($at)" }
            else { Check ($at -eq $items - 2) "D-pad up steps back one ($at)" }
        }
        Check (Until { (NN).syncPending -eq '0' } 'the game selection caught up') "the game's selection caught up with the walk (syncPending=$((NN).syncPending))"
    } else {
        $L = Layout
        $last = $L.Rows.Count - 1
        if ($last -ge 1) {
            Click $L.Rows[$last].Slot "row $last"
            Check (Until { [int](NN).itemSel -eq $last } "row $last focused") "a click on row $last focuses it (itemSel=$((NN).itemSel))"
            Shot 'inventory_row_click'
            # the window moved to keep a row of lead under the focus (nnrules::List.Fit, five rows): the top row shown is now row `first`
            $first = Fit-First $last 0 $items 5
            Click (Layout).Rows[0].Slot 'the top row shown'
            Check (Until { [int](NN).itemSel -eq $first } "row $first focused") "a click on the top row shown focuses row $first, the window kept its lead (itemSel=$((NN).itemSel), $items rows)"
        }
    }
    # a mount and back (docked) / nothing mounts (space): the game's own InvMount, the loadout's gun count
    Go-Filter $form 0
    for ($k = 0; $k -le $items -and [int](NN).itemSel -ne 0; $k++) { if ($form -eq 'pad') { Press @('Up') } else { Click (Layout).Rows[0].Slot 'the top row shown'; $null = Wait-FLFramesPassed 2 } }
    $null = Wait-FLUntil { [int](NN).itemSel -eq 0 } 'weapons, first row' 10
    Settle
    $guns0 = [int](NN).guns
    $a = @(PadView 'inventory' | Where-Object { $_ -match '^A (UN)?MOUNT$' }) # (the mouse's first chip is MOUNT / UNMOUNT where the pad shows A: Check-Parity)
    if ($place -eq 'space') {
        Check ($a.Count -eq 0) "in space nothing mounts: no A prompt ('$($a -join '')')"
        if ($form -eq 'pad') { Press @('A'); $null = Wait-FLFramesPassed 30; Check ([int](NN).guns -eq $guns0) "A in space leaves the loadout ($guns0 guns -> $((NN).guns))" } # (30 frames: a measurement window)
    } elseif ($a -contains 'A UNMOUNT') {
        if ($form -eq 'pad') { Press @('A') } else { Click (Layout).Lane[0].Slot 'UNMOUNT chip' }
        Check (Until { [int](NN).guns -eq $guns0 - 1 } 'a gun unmounted' 10) "docked: UNMOUNT takes a gun off ($guns0 -> $((NN).guns))"
        Shot 'inventory_unmounted'
        # (the list moves a moment later, ToggleMount's re-read; the selection follows the item: the pad says MOUNT on it)
        $ready = Until { (PadView 'inventory') -contains 'A MOUNT' } 'A MOUNT on the unmounted gun' 10
        Check $ready "the focus follows the unmounted gun: the pad offers A MOUNT ('$((PadView 'inventory') -join ', ')')"
        if ($ready) {
            if ($form -eq 'pad') { Press @('A') } else { Click (Layout).Lane[0].Slot 'MOUNT chip' }
            Check (Until { [int](NN).guns -eq $guns0 } 'the gun mounted again' 10) "docked: MOUNT puts it back ($((NN).guns) of $guns0 guns)"
        }
    } else { Check $false "docked weapons, first row: the pad shows UNMOUNT on a mounted gun ('$($a -join '')', $guns0 guns)" }
}
# A filter by the form's own way: the pad's D-pad right, the mouse's tab
function Go-Filter([string]$form, [int]$f) {
    if ($form -eq 'pad') { for ($k = 0; $k -lt 5 -and (NN).filter -ne "$f"; $k++) { Press @('Right'); $null = Wait-FLUntil { (NN).filter -eq (Bar).gameFilter } 'the filter step' 5 } }
    else { $L = Layout; if ($L.Tabs.Count -eq 5) { Click $L.Tabs[$f].Slot "filter tab $f" } }
    $null = Wait-FLUntil { (NN).filter -eq "$f" -and (Bar).gameFilter -eq "$f" } "filter $f" 10
    Settle
}
# The pad's prompts in this state: the form switched for the reading (the mouse form back after), the lane's screen asked for
function PadView([string]$screen) {
    $form = if ((NN).pad -eq '1') { 'pad' } else { 'kbm' }
    if ($form -ne 'pad') { Set-Form 'pad' }
    $null = Wait-FLUntil { $b = Bar; $b.visible -eq '1' -and $b.screen -eq $screen } "the pad's lane on $screen" 10
    $it = @((Bar).items)
    if ($form -ne 'pad') { Set-Form 'kbm'; $null = Wait-FLUntil { (NN).pad -eq '0' -and (Layout).Close } 'the mouse form back' 10; Settle }
    $it
}
function Walk-Status([string]$form, [string]$place) {
    $st = Check-Common 'STATUS' $form $place
    $n = $st.NN
    Check ([int]$n.shown -le [int]$n.reps -and [int]$n.shown -ge 0) "STATUS lists the player's factions ($($n.shown) shown of $($n.reps)); the numbers read (s0..s12: $(@($n.Keys | Where-Object { $_ -match '^s\d+$' }).Count))"
    Check (([int]$n.shown -eq 0 -and $n.sel -eq '0') -or ([int]$n.sel -lt [int]$n.shown)) "STATUS: the focus is inside the rows (sel=$($n.sel) of $($n.shown))"
    if ($form -eq 'pad') { Check-PadPrompts 'STATUS' $st $place } else { $null = Check-Parity 'STATUS' $st }
    Shot 'status'
    # the filters: the X word names the next; four steps come round; enemies + indifferent + friends = all (nnrules::Passes: the game's five bands)
    $xw = [regex]::Match(((PadView 'playerinfo') -join '|'), 'X SHOW (\w+)').Groups[1].Value
    $cur = ([Array]::IndexOf($kFilterWords, $xw) + 3) % 4
    $per = @{}
    foreach ($k in 1..4) {
        $was = $cur; $cur = ($cur + 1) % 4
        if ($form -eq 'pad') { Press @('X') } else { $L = Layout; Check ($L.Lane.Count -eq 1) "STATUS's filter chip stands ($($L.Lane.Count))"; Click $L.Lane[0].Slot "SHOW $($kFilterWords[$cur]) chip" }
        $null = Wait-FLFramesPassed 3
        $n = NN; $per[$kFilterWords[$cur]] = [int]$n.shown
        if ($form -eq 'pad') { $xn = [regex]::Match(((Bar).items -join '|'), 'X SHOW (\w+)').Groups[1].Value; Check ($xn -eq $kFilterWords[($cur + 1) % 4]) "X: $($kFilterWords[$cur]) shown, the prompt names $xn next" }
        Check ([int]$n.sel -eq 0 -and ([int]$n.shown -le [int]$n.reps)) "filter $($kFilterWords[$cur]): $($n.shown) of $($n.reps), the focus back on the first (sel=$($n.sel))"
        if ($kFilterWords[$cur] -eq 'ENEMIES') { Shot 'status_enemies' }
    }
    Check ($per['ALL'] -eq [int]$n.reps -and ($per['ENEMIES'] + $per['INDIFFERENT'] + $per['FRIENDS']) -eq $per['ALL']) "the filters part the factions: $($per['ENEMIES']) + $($per['INDIFFERENT']) + $($per['FRIENDS']) = $($per['ALL']) of $($n.reps)"
    # (all shown now, as it began: four steps)
    if ($form -eq 'pad') {
        $y0 = ((Bar).items | Where-Object { $_ -like 'Y *' })
        Press @('Y'); $null = Wait-FLFramesPassed 3
        $y1 = ((Bar).items | Where-Object { $_ -like 'Y *' })
        Check ($y0 -ne $y1 -and [int](NN).shown -eq [int]$n.shown) "Y sorts the other way ('$y0' -> '$y1'), the same factions"
        Press @('Y'); $null = Wait-FLFramesPassed 3
        $shown = [int](NN).shown
        $seq = @([int](NN).sel)
        for ($k = 0; $k -le [Math]::Ceiling($shown / 2) + 1; $k++) { Press @('Right'); $seq += [int](NN).sel }
        Check ($seq[-1] -eq $shown - 1 -and $seq[-2] -eq $shown - 1) "D-pad right pages to the last faction and stops ($($seq -join ' '); $shown shown)"
        if ($shown -ge 2) {
            $p = [int](NN).sel; $h = TimedPress 'Up' { [int](NN).sel -ne $p }; $at = [int](NN).sel
            if ($h -ge $kHoldLimit) { Check ($at -le $shown - 2) "D-pad up held $('{0:N2}' -f $h) s by this script: it moves back ($at)" }
            else { Check ($at -eq $shown - 2) "D-pad up steps back one ($at)" }
        }
    } else {
        $L = Layout
        Check ($L.Rows.Count -ge [Math]::Min(1, [int]$n.shown) -and $L.Rows.Count -le [int]$n.shown) "STATUS's faction rows are clickable ($($L.Rows.Count) of $($n.shown))"
        if ($L.Rows.Count -ge 2) { $k = $L.Rows.Count - 1; Click $L.Rows[$k].Slot "faction row $k"; Check (Until { [int](NN).sel -eq $k } "faction $k focused") "a click on faction row $k focuses it (sel=$((NN).sel))" }
    }
    Check (Until { (NN).cardFor -eq '1' } 'the card follows the focus') "the faction card is the focused faction's (cardFor=$((NN).cardFor), sel=$((NN).sel))"
    Shot 'status_walked'
}
function Walk-Info([string]$form, [string]$place) {
    $st = Check-Common 'INFO' $form $place
    if ($form -eq 'pad') { Check-PadPrompts 'INFO' $st $place } else { $null = Check-Parity 'INFO' $st }
    if ($place -eq 'space') { Check ([int]$st.NN.places -ge 1) "INFO in space lists what the player has found here ($($st.NN.places) places)" }
    Shot 'info'
    if ($form -eq 'pad') { Press @('Y') } else { Click (Layout).Lane[0].Slot 'SHOW ON MAP chip' }
    $ms = Wait-Tab 'MAP' 'SHOW ON MAP'
    $null = Check-Common 'MAP' $form $place
    Check ((Send-FL 'navnew state') -match ' on=1 grown=1 transform=1') "SHOW ON MAP opens the full-screen map, grown, on the game's own transform in $ms ms ($((Send-FL 'navnew state') -replace '^navnew ', '' -replace ' scale=.*', ''))"
    Shot 'map'
}
function Walk-Log([string]$form, [string]$place) {
    $st = Check-Common 'LOG' $form $place
    if ($form -eq 'pad') { Check-PadPrompts 'LOG' $st $place } else { $null = Check-Parity 'LOG' $st }
    $rootA = $st.NN.root
    Shot 'log'
    # MISSION / JOURNAL: the game's own tab (+0x37C: 0 the log, 1 mission; docs\research\neuronet-windows-facts.md)
    foreach ($t in 1, 0) {
        if ($form -eq 'pad') { Press @($(if ($t -eq 1) { 'Left' } else { 'Right' })) } else { $L = Layout; Check ($L.Tabs.Count -eq 2) "LOG's two tabs are clickable ($($L.Tabs.Count))"; Click $L.Tabs[1 - $t].Slot "log tab $(if ($t -eq 1) { 'MISSION' } else { 'JOURNAL' })" }
        $came = Until { (Peek8 (NN).root 0x37C) -eq $t } "log tab $t"
        $n = NN
        Check ($came -and $n.page -eq '1' -and $n.root -eq $rootA) "$(if ($t -eq 1) { 'MISSION' } else { 'JOURNAL' }): the game's own tab follows (+0x37C=$(Peek8 $n.root 0x37C) want $t), the page stays (root $($n.root))"
        Shot "log_tab$t"
    }
}

function Walk([string]$form, [string]$place) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    Set-Form $form
    $mode0 = Get-FLMode
    # the game drops its window commands while a NeuroNet window slides (the last close's): docked the room says so (`hotspots` busy: nn-window); in space
    # there is no such reading, so a command the game dropped is seen by its window not coming, and sent once more
    if ($place -eq 'docked') { $null = Wait-FLUntil { (((Send-FL 'hotspots') -split "`n")[-1]) -match 'ready$' } 'the room ready (no window sliding)' 15 }
    $null = Send-FL 'tap USER_INVENTORY'
    if (-not (Until { (Strip).open -eq $kCmd['INVENTORY'] } 'the inventory window' 4)) { Say "  the game dropped the inventory command (a slide): sent again"; $null = Send-FL 'tap USER_INVENTORY' }
    $ms = Wait-Tab 'INVENTORY' 'the game''s inventory command'
    Say "  INVENTORY up in $ms ms"
    Walk-Inventory $form $place
    $L = Layout
    if ($form -eq 'pad') { Press @('RB') } else { Click $L.Strip['STATUS'] 'the strip word STATUS' }
    $ms = Wait-Tab 'STATUS' 'RB / click'; Say "  STATUS up in $ms ms"
    Walk-Status $form $place
    if ($form -eq 'pad') { Press @('RB') } else { Click (Layout).Strip['INFO'] 'the strip word INFO' }
    $ms = Wait-Tab 'INFO' 'RB / click'; Say "  INFO up in $ms ms"
    Walk-Info $form $place
    if ($form -eq 'pad') { Press @('LB') } else { Click (Layout).Strip['LOG'] 'the strip word LOG' }
    $ms = Wait-Tab 'LOG' 'LB / click'; Say "  LOG up in $ms ms"
    Walk-Log $form $place
    # two presses in a row land on the second (the pad: the bumpers pick, the window opens once they stop; the mouse: the last press wins)
    $opens = @{}
    if ($form -eq 'pad') { Press @('LB'); Press @('LB'); $want = 'STATUS' } # (LOG -1 wraps to INFO, -1 again STATUS)
    else {
        # the second press while INFO's window comes in: the hotspots are renumbered as the pages change (a slot is a frame's, uibtn click goes by slot), so
        # the second goes through the strip word's own action (`nnstrip tab`, what that hotspot calls) once the first is seen taken
        Click (Layout).Strip['INFO'] 'INFO'
        $null = Wait-FLUntil { $s = Strip; $s.open -ne $kCmd['LOG'] -or $s.held -ne '0x0' } 'the INFO press taken' 5
        $r = Send-FL "nnstrip tab $([Array]::IndexOf(@('LOG', 'MAP', 'INVENTORY', 'STATUS', 'INFO'), 'MAP'))"
        Check ($r -match 'clicked') "MAP pressed during INFO's slide ($r)"
        $want = 'MAP'
    }
    $null = Wait-FLUntil { $s = Strip; $opens[$s.open] = 1; $s.open -eq $kCmd[$want] -and $s.held -eq '0x0' -and (NN).page -eq $kPage[$want] } "$want after two presses" 20
    $null = Wait-Tab $want 'the second press' # (STATUS: its numbers in, the world paused after)
    $between = @($opens.Keys | Where-Object { $_ -notin $kCmd['LOG'], $kCmd[$want], '0x0' })
    if ($form -eq 'pad') { Check ($between.Count -eq 0) "two bumper presses open STATUS only, nothing between (seen: $($opens.Keys -join ' '))" }
    $null = Check-Common $want $form $place
    Shot 'two_presses'
    if ($form -eq 'pad') { Press @('B') } else { Click (Layout).Close.Slot 'CLOSE' }
    $null = Wait-FLUntil { $s = Strip; $s.standing -eq '0' -and $s.open -eq '0x0' -and (NN).page -eq '0' } 'the NeuroNet closed' 15
    $null = Wait-FLUntil { (Get-FLMode) -eq $mode0 } "back to $mode0" 15
    Settle
    Check ((NN).paused -eq '0') "closed: the strip and the page are gone, the game back in $mode0, the world running"
    Shot 'closed'
    Say ("  {0} walked in {1:n1} s" -f $script:ctx, $sw.Elapsed.TotalSeconds)
}

$initial = Get-FLDisplaySize (Get-FLInstallDir)
try {
    foreach ($size in $Sizes) {
        $wh = $size -split 'x'; $script:W = [int]$wh[0]; $script:H = [int]$wh[1]
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
        Set-FLInstanceDisplay $Instance 'borderless' "$($script:W), $($script:H)"
        $first = $true
        foreach ($place in $Places) {
            $save = if ($place -eq 'docked') { $DockedSave } else { $SpaceSave }
            $script:ctx = "$size $place"
            $sw = [Diagnostics.Stopwatch]::StartNew()
            if ($first) { & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save $save | Select-Object -Last 1 | ForEach-Object { Say "  $_" } }
            else { Say "  $(Load-FLSave $save)" }
            $want = if ($place -eq 'docked') { 'docked' } else { 'space' }
            $null = Wait-FLUntil { (Get-FLMode) -eq $want } "$place after $save" 60
            $null = Wait-FLFramesPassed 30
            Say ("  {0} ready in {1:n1} s" -f $script:ctx, $sw.Elapsed.TotalSeconds)
            if ($first) {
                $render = [regex]::Match((Send-FL 'winmode'), 'render (\d+)x(\d+)')
                Check ($render.Groups[1].Value -eq "$($script:W)" -and $render.Groups[2].Value -eq "$($script:H)") "the game renders at $size ($($render.Value))"
            }
            $first = $false
            foreach ($form in $Forms) {
                $script:ctx = "$size $place $form"
                try { Walk $form $place }
                catch {
                    $script:failed.Add("FAIL [$($script:ctx)] the walk stopped: $($_.Exception.Message)"); Say "FAIL [$($script:ctx)] the walk stopped: $($_.Exception.Message)"
                    try { Shot 'stopped' } catch { }
                    try { Clear-FLPad; if ((Strip).standing -eq '1') { $null = Send-FL 'nnstrip close'; $null = Wait-FLUntil { (Strip).standing -eq '0' } 'closed after a failure' 15 } } catch { }
                }
            }
        }
    }
} finally {
    try { if (Get-FLGameProcess) { Clear-FLPad; $null = Send-FL 'set input_mode 0' } } catch { }
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
    # this instance's screenshot sampler (deploy starts one per game): stopped with the game
    Get-CimInstance Win32_Process -Filter "Name='pwsh.exe'" | Where-Object { $_.CommandLine -match 'flshots\.ps1' -and $_.CommandLine -match "-Instance $([regex]::Escape($Instance))(\s|$)" } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    if ($initial) { Set-FLInstanceDisplay $Instance 'borderless' $initial }
}
$script:ctx = 'run'
foreach ($c in @(Get-FLCrashBlocks $crashSince)) { Check $false "a game crashed during the run (EXE\FLvibed_crash.log):`n$c" }
Say ''
Say ("nn pages: {0} checks passed, {1} failed, {2} frames, {3:n0} s - {4}" -f $script:pass, $script:failed.Count, $script:shot, $total.Elapsed.TotalSeconds, $dir)
if ($script:failed.Count) { $script:failed | ForEach-Object { Say $_ }; Say 'NN PAGES: FAIL'; exit 1 }
Say 'NN PAGES: PASS'
exit 0
