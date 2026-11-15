# The docked base pages walked and recorded (modules\hud\src\basepage.*, docs\BASE_ROOMS_PLAN.md): the net the page framework's restructures (audit 70_,
# F3: best prices, repair, commodity, equipment, final stage, ship dealer, job board, news out of basepage.cpp) are checked with. Beside them the two
# pages that are not basepage's but stand on the same bases: the bar's rumour (the game's dialog in the skin) and the inventory's jettison (nnpage, F4).
# Two modes: -Mode record writes every recorded state and action result of the walk as text (a baseline, for the build it ran on); -Mode check runs the
# same walk and compares it with the baseline field by field: any difference is a FAIL naming the size, form, page, state and field. What legitimately
# varies between starts is masked by name in $Masks below, each with its reason, and listed as MASKED in the verdict; nothing else is skipped.
# One background game per display size (3840x2160, 1280x800), each page in the pad form (input_mode 1, the virtual pad's presses through padbases, holds
# for what spends) and the keyboard-and-mouse form (input_mode 2, `uibtn click <100+slot>` on HUD 2's hotspots, found by their place on the page).
# Fixtures: SaveC701 (docked at Manhattan, 500k credits: news, job board, commodity trader, best prices through the inventory, equipment dealer, the bar);
# SaveC7BP, written by this script from SaveC701 (rank 10 so the ship dealer sells, shield / thruster / both guns damaged so repair has rows: repair,
# ship dealer, final stage, a purchase); SaveC706 (in space: jettison). Every action that spends is undone (bought and sold back, mounted and unmounted)
# or done in the game only and thrown away by the next save load (the fixture files are never written): a ship purchase, a repair, a jettison, a job.
# Recorded per state: the page's own verb (`basepage`; `nnpage` for the inventory), the game's texts the page shows (ctlrect + peek of the game's text
# controls: the price, the total, the refusal, the repair total, the ship price), the pad's prompts drawn (`bases` promptbar) and wanted (the page's
# rules, basepage.cpp Prompts, as far as the verb tells them), the game's windows parked off the screen (`ctlrect`), HUD 2's hotspots (`uibtn`), and for
# every action the game's own values before and after (credits and assets `worth`, the equipment list `equipment`, the mission / offers state).
# And what the pages drew (field `texts`, the test verb `page texts`, pagerun.cpp, since 13 Nov 2026, audit 101_): every text and keycap of the pages'
# pass with its box and em in pixels - the words HUD 2 draws itself on the rows (MOUNTED / IN HOLD / a deal word, the slot word), the cards' text, the
# ship card's numbers. Not recorded: the game's text a page draws clipped (the news's text side, TextClipped: the kit does not record it), colours.
# Usage (PowerShell tool): tests\base_pages.ps1 -Mode check|record [-Instance 4] [-Sizes 3840x2160,1280x800] [-Forms pad,kbm]
#   [-Pages news,jobs,commodity,prices,equipment,bar,accept,repair,ships,jettison,nn] [-Baseline <file>]   build\release, no build.
# Output research\experience\runs\base_pages\<time>\ (record.txt = this run's records, steps.txt, verdict.txt, <n>_<size>_<form>_<page>_<state>.jpg).
#   -Mode slice [-Sizes ..] [-Forms ..] -Pages <some> [-From <a run's record.txt>]: re-records a slice into the baseline (13 Nov 2026, for a change that
#   asks for a page's new look): walks those (or takes an earlier run's record.txt, no game) and replaces exactly the baseline's fields of those sizes,
#   forms and pages, every other line byte-equal; prints the states replaced and each field changed / added / dropped, and the new sha256 for
#   $BaselineSha256 below. The baseline is shared by every lane: a slice is recorded on main, on purpose, like a record. A page whose states depend on
#   the pages walked before it (the news / jobs visit maps) is re-recorded with those pages in -Pages, or checked by a full run after.
#   The job board after a refused job (states refused / rechosen of page accept, Walk-Refused): walked with -Pages accept, in both forms at both sizes; a check
#   reports its fields NEW while $PendingStates names them and the baseline lacks them (below).
# Exit 0 = PASS (check) / recorded (record, slice), 1 = a FAIL (check) or the walk could not go on, 2 = no usable baseline.
param([ValidateSet('record', 'check', 'slice')][string]$Mode = 'check', [string]$Instance = '', [string[]]$Sizes = @('3840x2160', '1280x800'),
      [ValidateSet('pad', 'kbm')][string[]]$Forms = @('pad', 'kbm'),
      [ValidateSet('news', 'jobs', 'commodity', 'prices', 'equipment', 'bar', 'accept', 'repair', 'ships', 'jettison', 'nn')]
      [string[]]$Pages = @('news', 'jobs', 'commodity', 'prices', 'equipment', 'bar', 'accept', 'repair', 'ships', 'jettison', 'nn'),
      [string]$Baseline = '', [string]$From = '')
$ErrorActionPreference = 'Stop'
if ($From -and $Mode -ne 'slice') { throw '-From is for -Mode slice only' }
$root = Split-Path $PSScriptRoot -Parent
if (-not $Instance) { throw 'name the game instance: -Instance <n> (no default: a default instance belongs to no checkout - docs\TESTING.md "Parallel lanes")' }
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
Add-Type -AssemblyName System.Drawing

# The baseline: git-ignored evidence (it holds item, ship and base names: game text, which the repository never ships - CLAUDE.md "Copyright"); this
# script holds its hash, so a check never runs against a baseline that changed under it. Recorded on a457e082 (audit 72_), again on 13 Nov 2026 after the owner's prompt rule (audit 101_).
$BaselineSha256 = '540ED00D7C4D1383AA68BCF1822CD62B89663AD5221A9FDE5ED2FF38FAAD1C72'   # (recorded 13 Nov 2026 on 0015f1b4 = main 69370d41 + `page texts`, after the owner's prompt rule fe0c3622: run research\experience\runs\base_pages\20261113_075813; its 12 clock items then written `<box>` by PageTexts' own rule, nothing else touched; audit 101_; slice of `accept` recorded 13 Nov 2026 on main ef2f0afc for the refused-job states: run research\experience\runs\base_pages\20261113_190615; audit 112_)
# States the walk has gained that the shared baseline does not hold yet (the job board after a refused job, 13 Nov 2026): a check reports their fields NEW,
# not FAIL, until a slice record on main takes them in (audit 112_); empty the list then.
$PendingStates = @()
$defaultBaseline = Join-Path $root 'research\experience\base_pages\baseline.txt'
if (-not $Baseline) { $Baseline = $defaultBaseline }
# The shared baseline's folder, as this checkout spells it and as main does (a lane reaches main's through a junction): a record or a slice
# from a lane must not write it (it replaces the baseline for every lane and main: docs\TESTING.md "Parallel lanes"), and the hash guard
# below holds for every spelling of it.
$gd = "$(git -C $root rev-parse --path-format=absolute --git-dir 2>$null)"; $cd = "$(git -C $root rev-parse --path-format=absolute --git-common-dir 2>$null)"
$inLane = $gd -and $cd -and [IO.Path]::GetFullPath($gd) -ine [IO.Path]::GetFullPath($cd)
# (every folder on the way resolved through its junction or link, so any lane's junction spelling meets main's folder; the verifier, 13 Nov 2026)
function Resolve-RealPath([string]$p) {
    $full = [IO.Path]::GetFullPath($p); $drive = [IO.Path]::GetPathRoot($full); $cur = $drive
    foreach ($part in $full.Substring($drive.Length).Split([char]'\', [StringSplitOptions]::RemoveEmptyEntries)) {
        $cur = Join-Path $cur $part
        $i = Get-Item -LiteralPath $cur -Force -ErrorAction SilentlyContinue
        if ($i -and $i.LinkTarget) { $t = $i.ResolveLinkTarget($true); if ($t) { $cur = $t.FullName } }
    }
    $cur.TrimEnd('\')
}
$sharedDirs = @((Join-Path $root 'research\experience\base_pages')) + @(if ($cd) { Join-Path (Split-Path ([IO.Path]::GetFullPath($cd)) -Parent) 'research\experience\base_pages' })
$realBase = Resolve-RealPath $Baseline
$isShared = [bool]@($sharedDirs | Where-Object { $realBase.StartsWith((Resolve-RealPath $_) + '\', [StringComparison]::OrdinalIgnoreCase) })
if ($inLane -and $Mode -in 'record', 'slice' -and $isShared) { throw "-Mode $Mode from a lane ($root) would write the shared baseline ($Baseline): record from the main checkout only, or name a -Baseline outside research\experience\base_pages" }

# What legitimately varies between starts of the same build, masked by name (a regex on "page|state|field"), each with its reason. A masked field is still
# recorded; check lists it as MASKED and does not compare it.
$Masks = @(
    @{ Key = '^jobs\|(?!open\|(v\.kind|lane\.screen|lane\.visible|reached|hot\.strip|parked\.\w+|park\.\w+)$|walk\|end$|classic_open\|(classic\.page|lane\.screen|parked\.\w+|game\.buttons)$)'
       Why = 'the job board''s offers are drawn at random at each start: their number (rows, the walk, the press map, hotspots), their text (READ and the text side only while it overflows, the scroll), what a selection reveals (the visit map); compared: the page opens, its lane''s screen, the strip, the park' }
    @{ Key = '^news\|visit\|count$'; Why = 'the visit map''s size includes what the job board''s random offers revealed in the walks before (news, jobs, then news in the other form); the news rows'' own reveals (select_rowN) are compared' }
    @{ Key = '^ships\|classic_open\|(game\.buttons|lane\.pending)$|^final\|classic_open\|'
       Why = 'the game''s own ship dealer window under the pad (hud_style 0): the pad''s selection of the first ship, and with it the game''s BUY and the classic final stage, comes within 10 s in some runs and not in others (13 Nov 2026: 4K shown in four runs, not in 084044; 1280x800 not in 060148) - a timing of the classic window, not of the pages; the classic opening itself is still compared (a finding in audit 101_)' }
    @{ Key = '^accept\|[^|]*\|(?!(end|credits_unchanged|worth|selection_cleared|caption_drawn|job_chosen|accept_prompt)$)'; Why = 'the accepted job is one of the random offers (its kind, pay, target and the story state it leaves; the refused board''s rows are random offers too); compared: the walk ends, no cash moves at the accept, and after a refused job the game''s caption, the page''s drawn "Mission Rejected" and its ACCEPT prompt (refused / rechosen)' }
    @{ Key = '^bar\|[^|]*\|(?!end$)'; Why = 'the bar''s characters and what each offers are drawn at each start (a rumour, an info, a bribe or a mission offer, at another place, or none); compared: the walk ends' }
)

$stamp = '{0:yyyyMMdd_HHmmss}' -f (Get-Date)
$dir = Join-Path $root "research\experience\runs\base_pages\$stamp"
New-Item -ItemType Directory -Force $dir | Out-Null
$recFile = Join-Path $dir 'record.txt'; $stepFile = Join-Path $dir 'steps.txt'; $verdictFile = Join-Path $dir 'verdict.txt'
$script:size = ''; $script:form = ''; $script:W = 0; $script:H = 0; $script:shot = 0
$script:recKeys = @{}; $script:errors = [Collections.Generic.List[string]]::new()
$total = [Diagnostics.Stopwatch]::StartNew()
$crashSince = Get-Date   # (a crash recorder block written after this is the run's: Get-FLCrashBlocks at the end)
$build = try { (Get-Content (Join-Path $root 'build\release\build_info.txt') -TotalCount 1) } catch { 'unknown' }
# (nothing here writes to the pipeline by accident: a function's output is its return value - tools\claude\rules\powershell.md)
function Say([string]$line) { Write-Host $line; Add-Content -LiteralPath $stepFile $line }
function Note([string]$line) { Add-Content -LiteralPath $stepFile $line }

# ---- one record: "<size>|<form>|<page>|<state>|<field> = <value>"; a key is never written twice (a repeat gets #2, #3: the walk's order is part of it)
function Rec([string]$page, [string]$state, [string]$field, $value) {
    $v = ("$value" -replace '[\r\n]+', ' / ').Trim()
    $k = "$($script:size)|$($script:form)|$page|$state|$field"
    if ($script:recKeys.ContainsKey($k)) { $n = 2; while ($script:recKeys.ContainsKey("$k#$n")) { $n++ }; $k = "$k#$n" }
    $script:recKeys[$k] = $v
    Add-Content -LiteralPath $recFile "$k = $v"
}
function RecAll([string]$page, [string]$state, $h) { foreach ($e in $h.GetEnumerator()) { Rec $page $state $e.Key $e.Value } }

# ---- readings
function KV([string]$s) { $h = [ordered]@{}; foreach ($m in [regex]::Matches($s, '(\w+)=(\S*)')) { if (-not $h.Contains($m.Groups[1].Value)) { $h[$m.Groups[1].Value] = $m.Groups[2].Value } }; $h }
# The page's verb (basepage.cpp Command), parsed by kind. Names carry spaces: they are cut at the next known field.
function BP {
    $s = (Send-FL 'basepage') -replace "[\r\n]+", ' '
    $o = [ordered]@{ raw = $s }
    if ($s -match '^basepage (ships|repair|prices|board) ') {
        $o.kind = $Matches[1]
        $head = ($s -split ' \[', 2)[0]
        foreach ($e in (KV $head).GetEnumerator()) { if ($e.Key -ne 'bar') { $o[$e.Key] = $e.Value } }
        if ($o.kind -eq 'board') { $o.bar = if ($s -match ' bar=(\d+)' -and $Matches[1] -ne '0') { 'set' } else { 'none' } }
        if ($o.kind -eq 'repair') { $o.total = [regex]::Match($head, 'total=(.*)$').Groups[1].Value.Trim() }
        $o.list = @([regex]::Matches($s, '\[([^\]]*)\]') | ForEach-Object { $_.Groups[1].Value })
        return $o
    }
    if ($s -match '^basepage final (.*?) basepage (kind=.*)$') {
        $f = $Matches[1]; $s = "basepage $($Matches[2])"; $o.kind = 'final'
        $o.stop = [regex]::Match($f, 'stop=(\S+)').Groups[1].Value; $o.can = [regex]::Match($f, 'can=(\S+)').Groups[1].Value
        $o.back = [regex]::Match($f, 'back=(\S+)').Groups[1].Value; $o.why = [regex]::Match($f, 'why=(.*?) tradein=').Groups[1].Value
        $o.tradein = [regex]::Match($f, 'tradein=(\S+)').Groups[1].Value
    }
    $m = [regex]::Match($s, '^basepage kind=(\d+) side=(\d+) yours=(\d+) theirs=(\d+) sel=(-?\d+)(?: filter=(\S+) rowY=(\S+) rowT=(\S+) canTrade=(\S+) qty=(\S+) credits=(\S+))?(?: item=(.*?) card=(\S+) mounted=(\S+) tagged=(\S+))?(?: vs=(.*?) vsTagged=(\S+) free=(\S+))?$')
    if (-not $m.Success) { $o.kind = 'unknown'; return $o }
    if (-not $o.Contains('kind')) { $o.kind = @{ '0' = 'none'; '1' = 'commodity'; '2' = 'equipment'; '7' = 'final' }[$m.Groups[1].Value]; if (-not $o.kind) { $o.kind = "k$($m.Groups[1].Value)" } }
    $names = 'k', 'side', 'yours', 'theirs', 'sel', 'filter', 'rowY', 'rowT', 'canTrade', 'qty', 'credits', 'item', 'card', 'mounted', 'tagged', 'vs', 'vsTagged', 'free'
    for ($i = 2; $i -le 18; $i++) { if ($m.Groups[$i].Success) { $o[$names[$i - 1]] = $m.Groups[$i].Value } }
    $o
}
function Kind { (BP).kind }
# The pad's lane as padbases drew it (promptbar::Status): its items are what stands after the cut at promptfit::kLaneMax
function Lane {
    $b = Send-FL 'bases'
    $p = ($b -split 'promptbar ', 2)[1]
    $h = [ordered]@{}
    $h.screen = [regex]::Match("$p", '^screen=(\S+)').Groups[1].Value
    foreach ($k in 'count', 'pending', 'visible', 'dropped', 'lane') { $h[$k] = [regex]::Match("$p", "\b$k=(\S+)").Groups[1].Value }
    $h.items = [regex]::Match("$p", "items='([^']*)'").Groups[1].Value
    $h.area = [regex]::Match("$p", "items='[^']*' area=(\S+)").Groups[1].Value
    $h
}
# HUD 2's hotspots this frame (uibtn): rectangles in pixels, in the order the page registered them
function Hots {
    @(foreach ($l in ((Send-FL 'uibtn') -split "`n")) {
        $m = [regex]::Match($l, '^\s*(\d+) [0-9A-F]+ want \((-?\d+),(-?\d+)\)-\((-?\d+),(-?\d+)\)')
        if ($m.Success) { [pscustomobject]@{ Slot = [int]$m.Groups[1].Value; X0 = [int]$m.Groups[2].Value; Y0 = [int]$m.Groups[3].Value; X1 = [int]$m.Groups[4].Value; Y1 = [int]$m.Groups[5].Value } }
    })
}
function RectsText($hs) { ((@($hs) | Sort-Object Y0, X0 | ForEach-Object { "$($_.X0),$($_.Y0)-$($_.X1),$($_.Y1)" }) -join ' ') }
# The page's hotspots by place (the strip of the base and rooms stands above 0.165 H: basepage.cpp PageTop)
function PageHots { @(Hots | Where-Object { $_.Y0 -ge 0.16 * $script:H }) }
function LaneHots { @(PageHots | Where-Object { $_.Y0 -ge 0.9 * $script:H } | Sort-Object X0) }
# A list's rows: hotspots of one column (same left and right edge), taller than a chip, top to bottom. Side: left / right of the screen's middle
function RowHots([string]$side) {
    $c = @(PageHots | Where-Object { $_.Y0 -lt 0.9 * $script:H -and ($_.Y1 - $_.Y0) -ge 0.06 * $script:H -and ($_.Y1 - $_.Y0) -le 0.17 * $script:H -and ($_.X1 - $_.X0) -lt 0.5 * $script:W })
    $c = @($c | Where-Object { if ($side -eq 'left') { $_.X0 -lt 0.34 * $script:W } else { $_.X0 -ge 0.6 * $script:W } })
    $g = @($c | Group-Object { "$($_.X0),$($_.X1)" } | Sort-Object Count -Descending | Select-Object -First 1)
    if (-not $g) { return @() }
    @($g[0].Group | Sort-Object Y0)
}
# The filter tabs (the equipment dealer's four, the final stage's five): one row of short hotspots at the head of the left list
function TabHots { @(PageHots | Where-Object { ($_.Y1 - $_.Y0) -lt 0.06 * $script:H -and $_.Y0 -lt 0.32 * $script:H -and $_.X1 -le 0.36 * $script:W } | Sort-Object X0) }
# The chips between the lists (the trade's word, - + MAX, MOUNT, COMPARE; the ship's BUY; repair's TICK ALL / REPAIR): short hotspots outside the lane
# and the tabs, in lines top to bottom, each line left to right
function ChipLines {
    $tabs = @(TabHots | ForEach-Object Slot)
    # (a card squeezed to a sliver at the page's foot is no chip: at least 0.02 H tall)
    $c = @(PageHots | Where-Object { $_.Y0 -lt 0.9 * $script:H -and ($_.Y1 - $_.Y0) -lt 0.06 * $script:H -and ($_.Y1 - $_.Y0) -ge 0.02 * $script:H -and $_.Slot -notin $tabs })
    $lines = @($c | Group-Object { [int][Math]::Round($_.Y0 / 4) } | Sort-Object { $_.Group[0].Y0 } | ForEach-Object { , @($_.Group | Sort-Object X0) })
    , $lines   # (one object: the caller indexes lines, (ChipLines) | ... walks them)
}
# The inventory's five filter tabs (nnpage): the lower of the two rows of five short hotspots at the top (the strip's five words stand above them)
function NNTabs {
    $g = @(Hots | Where-Object { ($_.Y1 - $_.Y0) -lt 0.07 * $script:H -and $_.Y0 -lt 0.3 * $script:H } | Group-Object { [int][Math]::Round($_.Y0 / 4) } | Where-Object Count -eq 5 | Sort-Object { $_.Group[0].Y0 } | Select-Object -Last 1)
    if (-not $g) { return @() }
    @($g[0].Group | Sort-Object X0)
}
# The page's rows read until two readings 30 frames apart agree (after a filter or side change the rows are re-read a moment later: basepage g_tradeWait)
function Settle-BP { $a = (BP).raw; for ($i = 0; $i -lt 10; $i++) { $null = Wait-FLFramesPassed 30; $b = (BP).raw; if ($a -eq $b) { return }; $a = $b } }
function Click($hot, [string]$what) {
    if (-not $hot) { throw "no hotspot for $what" }
    $r = Send-FL "uibtn click $(100 + $hot.Slot)"
    if ($r -ne 'clicked') { throw "the click on $what (hotspot $($hot.Slot)): $r" }
    Note "  click $what (hotspot $($hot.Slot) at $($hot.X0),$($hot.Y0))"
}
# A game text control's own text (winplates::TextOf's read: the control +0x334 -> its text object -> a UTF-16 string), '' when not shown
function U32At([uint32]$a) {
    $r = Send-FL ('peek 0x{0:x} 4' -f $a)
    if ($r -notmatch ':') { return [uint32]0 }
    $b = @((($r -split ':', 2)[1].Trim() -split '\s+') | ForEach-Object { [Convert]::ToByte($_, 16) })
    if ($b.Count -lt 4) { return [uint32]0 }
    [BitConverter]::ToUInt32([byte[]]$b, 0)
}
# The string is read a memory page at a time, up to its NUL, as TextOf reads it a character at a time: until 15 Nov 2026 one `peek` of 256 bytes
# read it, and `peek` answers "read failed" for the whole range when any byte of it cannot be read, so a short string allocated within 256 bytes of the
# end of the heap's committed memory read '' while the game drew it (the price-text flake of audit 137_: TransferPriceText / QuantityTotalText '' in one
# state for 8 s and every row, the page's own record showing '$150'; heap layout differs per start, so 2 runs of 3 failed). Such a string is noted in
# steps.txt ("the old read") with the byte count left in its page, so the cause stays measured.
$script:gtNoted = @{}
function GameText([string]$name) {
    $r = Send-FL "ctlrect $name"
    $m = [regex]::Match($r, ' ctl=([0-9A-F]{8}) ')
    if (-not $m.Success) { return '' }
    $obj = U32At ([Convert]::ToUInt32($m.Groups[1].Value, 16) + 0x334)
    if (-not $obj) { return '' }
    $txt = U32At $obj
    if (-not $txt) { return '' }
    $bytes = [Collections.Generic.List[byte]]::new()
    [long]$a = $txt
    for ([int]$left = 256; $left -gt 0; ) {
        [int]$n = [Math]::Min($left, [int](4096 - ($a % 4096)))
        $hex = Send-FL ('peek 0x{0:x} {1}' -f $a, $n)
        if ($hex -notmatch ':') { break }   # (the page after the string's cannot be read: the string ended before it, or nothing was read)
        $nul = $false
        foreach ($l in ($hex -split "`n")) { if ($l -match ':') { foreach ($t in (($l -split ':', 2)[1].Trim() -split '\s+')) { if ($t) { $bytes.Add([Convert]::ToByte($t, 16)) } } } }
        for ($i = $bytes.Count - $n; $i + 1 -lt $bytes.Count; $i += 2) { if ($i -ge 0 -and $bytes[$i] -eq 0 -and $bytes[$i + 1] -eq 0) { $nul = $true; break } }
        if ($nul) { break }
        $left -= $n; $a += $n
    }
    $s = [Text.Encoding]::Unicode.GetString($bytes.ToArray())
    $z = $s.IndexOf([char]0); if ($z -ge 0) { $s = $s.Substring(0, $z) }
    [long]$inPage = 4096 - ([long]$txt % 4096)
    if ($inPage -lt 256 -and -not $script:gtNoted.ContainsKey("$name@$txt") -and (Send-FL ('peek 0x{0:x} 1' -f ($txt + $inPage))) -notmatch ':') {
        $script:gtNoted["$name@$txt"] = 1
        Note ("  GAMETEXT the old read: $name's string at 0x{0:X8} has {1} bytes left in its page and the next page cannot be read: one 256-byte peek read '' (now '{2}')" -f $txt, $inPage, $s)
    }
    $s
}
# A game window parked by the page: its rectangle's left past the screen's right edge
function Parked([string]$name) {
    $r = Send-FL "ctlrect $name"
    $m = [regex]::Match($r, ' left=(-?[\d.]+) top=\S+ right=(-?[\d.]+)')
    if (-not $m.Success) { return 'not shown' }
    if ([double]$m.Groups[1].Value -ge $script:W -or [double]$m.Groups[2].Value -le 0) { 'off the screen' } else { 'on the screen' }
}
function Vis([string]$name) { (Send-FL "visible $name") -match '^visible' }
# The game's own values an action changes: cash and assets (`worth`), the equipment list (`equipment`: arch, count, mounted, hardpoint, health)
function Worth { $w = Send-FL 'worth'; "cash=$([regex]::Match($w, 'cash=(\S+)').Groups[1].Value) assets=$([regex]::Match($w, 'assets=(\S+)').Groups[1].Value)" }
function Gear {
    (@(((Send-FL 'equipment') -split "`n") | Where-Object { $_ -match '^arch=' } | ForEach-Object {
        $m = [regex]::Match($_, 'arch=(\d+) id=\d+ count=(\d+) mounted=(\d) hp=(\S+) health=(\S+)')
        "$($m.Groups[1].Value):$($m.Groups[2].Value):$($m.Groups[3].Value):$($m.Groups[4].Value):$($m.Groups[5].Value)" }) | Sort-Object) -join ' '
}
# The player's ship as the game's lists tell it: its equipment and cash together (a ship purchase changes both at once)
function Ship { "$(Worth) | $(Gear)" }

# ---- the prompts the page wants (basepage.cpp Prompts, padbases' B CLOSE when the page has no B of its own), from what the verb tells. Two of its
# conditions are not printed: Mountable (a hardpoint item: taken as the weapons and external filters' items) and CanTag (a mounted one with numbers:
# taken as the same, mounted). A card longer than its room (RS SCROLL CARD) is not printed either: written "(RS SCROLL CARD if the card overflows)".
function Wanted($b) {
    $w = [Collections.Generic.List[string]]::new()
    switch ($b.kind) {
        'board' {
            $jobs = (Lane).screen -eq 'jobboard'
            if ($b.side -ne '1' -or $b.bar -eq 'none') { $w.Add("DU+DD $(if ($jobs) { 'JOB' } else { 'STORY' })"); if ($b.bar -eq 'set') { $w.Add('DR READ') } }
            else { $w.Add("DL $(if ($jobs) { 'JOBS' } else { 'STORIES' })"); $w.Add('RS SCROLL') }
            if ($jobs -and (Vis 'MissionVendorAcceptButton')) { $w.Add('A ACCEPT') }
        }
        'ships' {
            if ($b.side -eq '0') { $w.Add('DU+DD SHIP'); if ([int]$b.sel -ge 0) { $w.Add('DR VIEW SHIP') } }
            else { $w.Add('DL SHIP LIST'); $w.Add('RS TURN'); $w.Add('(DU+DD SCROLL CARD if the card overflows)') }
            if ($b.zoomable -eq '1') { $w.Add('LT+RT ZOOM') }
            if ($b.can -eq '1') { $w.Add('A BUY THIS SHIP') }
        }
        'repair' {
            if ($b.list.Count -gt 1) { $w.Add('DU+DD ITEM') }
            if ($b.list.Count -gt 0) { $t = [regex]::Match($b.list[[Math]::Min([int]$b.sel, $b.list.Count - 1)], 'tick=(\d)').Groups[1].Value; $w.Add($(if ($t -eq '1') { 'A UNTICK' } else { 'A TICK' })); $w.Add('X TICK ALL / NONE') }
            if ($b.can -eq '1') { $w.Add('Y REPAIR') }
            $w.Add('(RS SCROLL CARD if the card overflows)')
        }
        'prices' { if ([int]$b.n -gt 1) { $w.Add('DU+DD BASE') } }
        default {
            $final = $b.kind -eq 'final'; $yours = $b.side -eq '0'
            if ($final -and $b.stop -eq '1') { $w.Add('DL+DR YOUR ITEMS'); $w.Add('A BUY SHIP') }
            else {
                $w.Add('DU+DD ITEM')
                $word = if ($final) { if ($b.can -eq '1') { 'BUY SHIP' } elseif ($yours) { 'DEALER' } else { 'NEW SHIP' } } elseif ($yours) { 'DEALER' } else { 'YOUR SHIP' }
                $w.Add("$(if ($yours) { 'DR' } else { 'DL' }) $word")
                if ($b.canTrade -eq '1') {
                    $w.Add("A $(if ($final) { if ($yours) { 'SELL TO DEALER' } else { 'TAKE BACK' } } elseif ($yours) { 'SELL' } else { 'BUY' })")
                    if (Vis 'QuantitySlider') { $w.Add('LT+RT QUANTITY'); $w.Add('Y MAX') }
                }
            }
            if ($b.kind -eq 'equipment' -or $final) { $w.Add('X FILTER') }
            if ($b.kind -eq 'commodity' -and (Vis 'TradeRouteButton')) { $w.Add('X BEST PRICES') }
            $hard = $b.kind -eq 'equipment' -and $yours -and $b.item -and $b.item -ne 'EMPTY' -and $b.filter -in '0', '2'
            if ($hard) { $w.Add("LS $(if ($b.mounted -eq '1') { 'UNMOUNT' } else { 'MOUNT' })") }
            if ($hard -and $b.mounted -eq '1') { $w.Add("R3 $(if ($b.tagged -ne '0' -and $b.tagged -ne '') { 'CLICK UNTAG / CLICK COMPARE WITH' } else { 'CLICK COMPARE WITH' })") }
            if ($final -and $b.back -eq '1') { $w.Add('B BACK TO SHIPS') }
        }
    }
    if (-not ($w | Where-Object { $_ -like 'B *' })) { $w.Add('B CLOSE') }
    @($w)
}

# ---- one state: the page's whole reading, read until it holds (two readings 30 frames apart equal: the rows re-read every 0.75 s at most and the
# lane settles within promptfit's 400 ms; at the test's 60 fps cap 30 frames are 0.5 s), recorded, and a frame for the run-judge
$script:pageTexts = @{
    commodity = 'TransferPriceText', 'QuantityTotalText', 'ErrorHelp'
    equipment = 'TransferPriceText', 'QuantityTotalText', 'ErrorHelp'
    final     = 'TransferPriceText', 'QuantityTotalText', 'ErrorHelp', 'ShipDealerShipPrice', 'ShipBuyProblem'
    ships     = 'ShipDealerShipPrice', 'ShipBuyProblem'
    repair    = 'RepairTotalCost'
}
$script:pageWindows = @{ commodity = 'NN_Dealer', 'NN_Inventory'; equipment = 'NN_Dealer', 'NN_Inventory'; final = 'NN_Dealer', 'NN_Inventory'; ships = 'NN_ShipTrader'
                         repair = 'NN_Repair'; prices = 'NN_CommodityByBase'; board = 'NN_MissionVendor', 'NN_News' }
$script:buttons = 'BuySellTradeButton', 'QuantitySlider', 'TradeRouteButton', 'RepairOkButton', 'ShipTraderBuyButton', 'ContinueButton', 'BackToShipDealer', 'MissionVendorAcceptButton'
function Slider([string]$name) {
    if (-not (Vis $name)) { return 'not shown' }
    $r = Send-FL "scroll $name 0"   # (a move of 0: the bar's position and range as the game holds them)
    $m = [regex]::Match($r, 'pos=(\S+) range=(\S+)')
    if ($m.Success) { "pos=$($m.Groups[1].Value) range=$($m.Groups[2].Value)" } else { $r }
}
function Read-State {
    $b = BP
    $o = [ordered]@{}
    # the verb's line verbatim (70b W1 (2)), the board's text-bar address written as <bar> (a heap address: it differs at every start)
    $o['v.line'] = $b.raw -replace ' bar=\d+', ' bar=<bar>'
    foreach ($e in $b.GetEnumerator()) { if ($e.Key -notin 'raw', 'list', 'k') { $o["v.$($e.Key)"] = $e.Value } }
    if ($b.Contains('list')) { $o['v.list'] = ($b.list -join ' | ') }
    # the game window's own state (70b W1 (3)): its buttons shown, its quantity bar
    $o['game.buttons'] = (@($script:buttons | Where-Object { Vis $_ }) -join ',')
    if ($b.kind -in 'commodity', 'equipment', 'final') { $o['game.QuantitySlider'] = Slider 'QuantitySlider' }
    if ($script:form -eq 'pad') {
        $l = Lane
        foreach ($k in 'screen', 'items', 'count', 'dropped', 'visible', 'pending', 'area') { $o["lane.$k"] = $l[$k] }
        $want = Wanted $b
        $o.wanted = $want -join ', '
        $drawn = @($l.items -split ', ' | Where-Object { $_ })
        $o.cut = (@($want | Where-Object { $_ -notin $drawn -and $_ -notlike '(*' }) -join ', ')
    } else {
        $l = Lane
        $o['lane.visible'] = $l.visible; $o['lane.items'] = $l.items
    }
    foreach ($t in @($script:pageTexts[$b.kind])) {
        if ($t) { $o["game.$t"] = GameTextFilled $t $(if ($b.canTrade -eq '1') { 8 } else { 3 }) }   # (a state's texts can show the other side's selection, canTrade=0 with a price: always 3 s; a state that can trade: 8 s)
    }
    foreach ($w in @($script:pageWindows[$b.kind])) { if ($w) { $o["parked.$w"] = Parked $w } }
    $o['hot.page'] = RectsText (PageHots)
    $o['hot.strip'] = RectsText @(Hots | Where-Object { $_.Y0 -lt 0.16 * $script:H })
    $o.texts = PageTexts
    $o
}
# What the pages drew this frame (`page texts`, pagerun.cpp: the kit's records of the pages' pass): every text and keycap in drawn order with its box
# and em in pixels - the rows' own words (MOUNTED, a deal word, the slot), the cards' text, the ship card's numbers. Armed once per game (Run-Size).
function PageTexts {
    $t = "$(Send-FL 'page texts')"
    if ($t -notmatch '^page texts n=\d+ age_ms=(\d+)') { return "($($t.Trim()))" }
    # (the record is the last pages' pass: one that did not run for 2 s is an old frame's - the skin's pass not drawn - never this state's)
    if ([int]$Matches[1] -gt 2000) { return "(stale: no pages' pass for $($Matches[1]) ms)" }
    # (a clock the game shows - STATUS's time played, 00:36:45 - is written <clock> with its box <box>: it differs at every run, and its box
    # with it (right-aligned digits of different widths); its em is still compared)
    ((($t -split "`n") | Select-Object -Skip 1 | ForEach-Object { $_.Trim() -replace "^(\w) \S+ (em=\d+) '\d+:\d\d:\d\d'$", "`$1 <box> `$2 '<clock>'" }) -join ' / ')
}
function Same($a, $b) { if ($a.Count -ne $b.Count) { return $false }; foreach ($k in $a.Keys) { if ("$($a[$k])" -ne "$($b[$k])") { return $false } }; $true }
function Stable {
    $a = Read-State
    for ($i = 0; $i -lt 12; $i++) {
        $null = Wait-FLFramesPassed 60   # (1 s at the test's cap: the page re-selects the game's row every 0.75 s at most, its texts follow)
        $b = Read-State
        if ((Same $a $b) -and ($script:form -ne 'pad' -or $b['lane.pending'] -eq '0')) { return $b }
        $a = $b
    }
    $a['unsteady'] = 'the reading changed in every window of 0.5 s for 6 s'
    $a
}
function Shot([string]$name) {
    $script:shot++
    $base = '{0:d3}_{1}_{2}_{3}' -f $script:shot, $script:size, $script:form, ($name -replace '[^\w]+', '_')
    $png = Save-FLShot -Name $base -Dir $dir
    $img = [Drawing.Image]::FromFile($png)
    try {
        $enc = [Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -eq 'image/jpeg'
        $ps = New-Object Drawing.Imaging.EncoderParameters 1
        $ps.Param[0] = New-Object Drawing.Imaging.EncoderParameter ([Drawing.Imaging.Encoder]::Quality), 88L
        $img.Save((Join-Path $dir "$base.jpg"), $enc, $ps)
    } finally { $img.Dispose() }
    [IO.File]::Delete($png)
    $base
}
# The park after the slide (70b W1 (4)): the window's root and every control under it (the game's tree as `dumpui` writes it, as info_compare reads
# it), each one's left edge against the screen's width; on the screen are named (a kept child, e.g. the ship dealer's 3D view, with its rectangle)
function Park-Tree([string]$rootName) {
    $o = [ordered]@{}
    $r = Send-FL "ctlrect $rootName"
    $m = [regex]::Match($r, ' root=([0-9A-F]+).* left=(-?[\d.]+)')
    if (-not $m.Success) { $o["park.$rootName"] = "($r)" -replace '[0-9A-F]{8}', '<addr>'; return $o }
    $rootId = $m.Groups[1].Value
    $path = (Send-FL 'dumpui').Trim()
    $names = [Collections.Generic.List[string]]::new()
    if (Test-Path -LiteralPath $path) {
        $depth = -1
        foreach ($l in (Get-Content -LiteralPath $path)) {
            $mm = [regex]::Match($l, '^( *)(\S+)\s+ctl=')
            if (-not $mm.Success) { if ($l -match '^== root') { $depth = -1 }; continue }
            $d = $mm.Groups[1].Value.Length
            if ($depth -lt 0) { if ($mm.Groups[2].Value -eq $rootName) { $depth = $d }; continue }
            if ($d -le $depth) { $depth = -1; if ($mm.Groups[2].Value -eq $rootName) { $depth = $d }; continue }
            if ($names -notcontains $mm.Groups[2].Value) { $names.Add($mm.Groups[2].Value) }
        }
    }
    $off = 0; $on = [Collections.Generic.List[string]]::new()
    foreach ($n in $names) {
        $c = [regex]::Match((Send-FL "ctlrect $n"), ' root=([0-9A-F]+).* left=(-?[\d.]+) top=(-?[\d.]+) right=(-?[\d.]+) bottom=(-?[\d.]+)')
        if (-not $c.Success -or $c.Groups[1].Value -ne $rootId) { continue }   # (hidden, or the same name in another window)
        if ([double]$c.Groups[2].Value -ge $script:W -or [double]$c.Groups[4].Value -le 0) { $off++ } else { $on.Add(('{0} ({1:0},{2:0})-({3:0},{4:0})' -f $n, [double]$c.Groups[2].Value, [double]$c.Groups[3].Value, [double]$c.Groups[4].Value, [double]$c.Groups[5].Value)) }
    }
    $o["park.$rootName"] = "root $(if ([double]$m.Groups[2].Value -ge $script:W) { 'off the screen' } else { 'on the screen' }); children off $off, on: $(if ($on.Count) { $on -join ', ' } else { 'none' })"
    $o
}
$script:parkRoots = @{ commodity = 'NN_Dealer', 'NN_Inventory'; equipment = 'NN_Dealer', 'NN_Inventory'; final = 'NN_Dealer', 'NN_Inventory'; ships = @('NN_ShipTrader')
                       repair = @('NN_Repair'); prices = @('NN_CommodityByBase'); jobs = @('NN_MissionVendor'); news = @('NN_News') }
# Records the page's state under <page>|<state>, with a frame; at a page's opening also how it was reached and the park
function State([string]$page, [string]$state) {
    $s = Stable
    if ($state -eq 'open') {
        if ($script:reached) { $s['reached'] = $script:reached; $script:reached = '' }
        foreach ($r in @($script:parkRoots[$page])) { if ($r) { foreach ($e in (Park-Tree $r).GetEnumerator()) { $s[$e.Key] = $e.Value } } }
    }
    RecAll $page $state $s
    $f = Shot "${page}_$state"
    Note "STATE $($script:size) $($script:form) $page $state ($f.jpg): $($s['v.kind']) sel=$($s['v.sel']) item=$($s['v.item']) lane='$($s['lane.items'])'"
    $s
}

# ---- input in each form
function Set-Form([string]$form) {
    $null = Send-FL "set input_mode $(if ($form -eq 'pad') { 1 } else { 2 })"
    $want = if ($form -eq 'pad') { 'device=pad' } else { 'device=kbm' }
    $null = Wait-FLUntil { (Send-FL 'device') -match "^$want " } "input form $form" 10
    $null = Wait-FLFramesPassed 4
}
function Press([string[]]$b) { Press-FLPad $b }
# One row by the pad's D-pad. A press is held two frames (Press-FLPad): when the game runs slowly (a 4K background game on a loaded machine) two frames
# can pass the lists' repeat delay (0.3 s, basepage.cpp Pad) and the focus moves two rows; that step is walked back by the verb's own one-row press
# and noted in steps.txt, so the record holds where each press should land. $false when the focus did not move (the list's end).
function Pad-Step([string]$dir, [scriptblock]$selOf, [string]$verb = 'basepage') {
    $was = [int](& $selOf)
    Press @($dir)
    $moved = try { Await { [int](& $selOf) -ne $was } 'a D-pad step' 2; $true } catch { $false }
    if (-not $moved) { return $false }
    $want = $was + $(if ($dir -eq 'Down') { 1 } else { -1 })
    $now = [int](& $selOf)
    if ($now -ne $want) {
        Note "  note: one $dir press moved the focus $was -> $now (the list's repeat on a slow frame): walked back to $want"
        for ($g = 0; $g -lt 20 -and [int](& $selOf) -ne $want; $g++) { $null = Send-FL "$verb press $(if ([int](& $selOf) -gt $want) { 'up' } else { 'down' })"; $null = Wait-FLFramesPassed 3 }
    }
    $true
}
# A held press (the page's hold-to-spend, dealer_rules.h kHoldSec 0.6 s): held until the condition holds, then released
function Hold([string]$button, [scriptblock]$done, [string]$what, [double]$sec = 8) {
    Set-FLPad -Buttons @($button)
    try { $null = Wait-FLUntil $done $what $sec } finally { Clear-FLPad; $null = Wait-FLFramesPassed 3 }
}
# A trigger held until the condition holds (LT / RT: quantity, zoom)
function Trigger([string]$which, [scriptblock]$done, [string]$what, [double]$sec = 6) {
    if ($which -eq 'LT') { Set-FLPad -LT 1 } else { Set-FLPad -RT 1 }
    try { $null = Wait-FLUntil $done $what $sec } finally { Clear-FLPad; $null = Wait-FLFramesPassed 3 }
}
# Waits for the verb to show a field's new value after an input (state, not time)
function Await([scriptblock]$cond, [string]$what, [double]$sec = 8) { $null = Wait-FLUntil $cond $what $sec }

# ---- rooms and pages
function Open-Spot([string]$room, [string]$spot, [string]$kind) {
    Wait-FLRoomReady
    if ((Send-FL 'hotspots') -notmatch "(?m)^$spot\s") { $null = Goto-FLRoom $room; Wait-FLRoomReady }
    $null = Wait-FLUntil { $x = Send-FL "goto $spot"; if ($x -match '^clicked') { $x } } "the room's $spot" 15
    Await { (Kind) -eq $kind } "the $kind page" 20
    $script:reached = "save $($script:save), room $room, hotspot $spot"
}
function Close-Page([string]$what) {
    if ($script:form -eq 'pad') { Press @('B') } else { Click (LaneHots)[0] "CLOSE ($what)" }
    Await { (Kind) -in 'none', 'unknown' } "$what closed" 15
    Wait-FLRoomReady
}

# ---- a list's walk: the focus to every row in turn, each row's reading (the verb's item and the game's selection and texts), then once more past the end
function Row-Reading($texts) {
    # (read until two readings 20 frames apart agree: the game's texts follow its selection a moment after the page's focus)
    $a = Row-Read $texts
    for ($i = 0; $i -lt 10; $i++) { $null = Wait-FLFramesPassed 20; $b = Row-Read $texts; if ($a -eq $b) { return $b }; $a = $b }
    $b
}
function Row-Read($texts) {
    $b = BP
    $r = "$($b.item)|row=$(if ($b.side -eq '0') { $b.rowY } else { $b.rowT })|trade=$($b.canTrade)|qty=$($b.qty)|mounted=$($b.mounted)|card=$($b.card)"
    foreach ($t in $texts) { $r += "|$t=$(GameTextFilled $t $(if ($b.canTrade -eq '1') { 8 } else { 1 }))" }
    $r
}
# The game's price and repair total are never empty while their control stands: an empty read is the game refilling it after a selection or a tick
# (13 Nov 2026 beside other games: the repair total at 4K, a commodity row's price at 1280x800): read again, 10 frames apart, 1 s at most.
# A row or state that CAN trade (canTrade=1) waits up to 8 s: run 20261113_235610 read the 4K pad commodity sell_row's price empty after 3 s (baseline $155), passing on a re-run.
# $limit: seconds to wait (1 default). The price and total MUST come: wait up to 3 s (13 Nov 2026, instance 11 beside other games, runs\base_pages\20261113_110233: three 4K
# equipment rows and two states read empty past the 1 s, passing on a re-run alone). A row is due when it can be traded (the baseline's 210 empty
# row prices are all canTrade=0 rows: a refusal); a state always (yours_f1 shows the dealer's $25 with canTrade=0; its 32 empty fields cost 3 s).
function GameTextFilled([string]$t, [int]$limit = 1) {
    $v = GameText $t
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($t -in 'RepairTotalCost', 'TransferPriceText', 'QuantityTotalText' -and $v -eq '' -and $sw.Elapsed.TotalSeconds -lt $limit -and (Vis $t)) { $null = Wait-FLFramesPassed 10; $v = GameText $t }
    # (a price that can trade still empty after the whole wait: what the control answers, so a cause other than the read's is seen)
    if ($v -eq '' -and $limit -ge 8 -and $t -in 'TransferPriceText', 'QuantityTotalText') { Note "  GAMETEXT $t empty after $limit s: $(Send-FL "ctlrect $t")" }
    $v
}
# The model of nnrules::List.Fit (tests\nnpage_test.cpp): the first shown row after the focus moved to sel, five rows, a row of lead when more than two
function Fit-First([int]$sel, [int]$first, [int]$count, [int]$rows = 5) {
    $lead = if ($rows -gt 2) { 1 } else { 0 }
    if ($sel - $lead -lt $first) { $first = $sel - $lead }
    if ($sel + $lead -ge $first + $rows) { $first = $sel + $lead - $rows + 1 }
    [Math]::Max(0, [Math]::Min($first, [Math]::Max(0, $count - $rows)))
}
# Focus row $to of the dealer list on the current side (pad: D-pad up / down a row a press; mouse: the row's hotspot, the list window by Fit-First)
$script:first = @{ '0' = 0; '1' = 0 }
function Focus-Row([int]$to) {
    $b = BP; $side = $b.side; $count = [int]$(if ($side -eq '0') { $b.yours } else { $b.theirs })
    for ($guard = 0; [int](BP).sel -ne $to -and $guard -lt 40; $guard++) {
        $sel = [int](BP).sel
        if ($script:form -eq 'pad') {
            if (-not (Pad-Step $(if ($to -gt $sel) { 'Down' } else { 'Up' }) { (BP).sel })) { throw "the focus did not move off row $sel toward $to" }
            $script:first[$side] = Fit-First ([int](BP).sel) $script:first[$side] $count
            continue
        }
        else {
            $rows = RowHots $(if ($side -eq '0') { 'left' } else { 'right' })
            $want = $sel + [Math]::Sign($to - $sel)
            $idx = $want - $script:first[$side]
            if ($idx -lt 0 -or $idx -ge $rows.Count) { $idx = [Math]::Max(0, [Math]::Min($rows.Count - 1, $idx)); $want = $script:first[$side] + $idx }
            Click $rows[$idx] "row $want"
        }
        Await { [int](BP).sel -eq $want } "the focus on row $want"
        $script:first[$side] = Fit-First $want $script:first[$side] $count
    }
}
function Walk-List([string]$page, [string]$state, $texts) {
    Settle-BP
    $b = BP; $count = [int]$(if ($b.side -eq '0') { $b.yours } else { $b.theirs })
    Focus-Row 0
    $names = [Collections.Generic.List[string]]::new()
    for ($i = 0; $i -lt $count; $i++) {
        Focus-Row $i
        $null = Wait-FLFramesPassed 2
        $r = Row-Reading $texts
        Rec $page $state "row$i" $r
        $names.Add(($r -split '\|')[0])
    }
    Rec $page $state 'rows' $count
    if ($count -gt 0 -and $script:form -eq 'pad') {
        Press @('Down'); $null = Wait-FLFramesPassed 6
        Rec $page $state 'sel_after_last' "$((BP).sel) (of $count)"
    }
    Focus-Row 0
    @($names)
}

# ---- the verb's press map (70b W1 (6), basepage.cpp Command: up, down, left, right, x, y; ls, trade and a act and are the walks' own actions): the
# page's line after each press, in that order, from the state the walk leaves
function Press-Map([string]$page, [string]$state, [string[]]$buttons = @('up', 'down', 'left', 'right', 'x', 'y')) {
    if ($script:form -ne 'pad') { return }
    foreach ($b in $buttons) {
        $r = Send-FL "basepage press $b"
        $null = Wait-FLFramesPassed 12   # (the press runs at once; the rows' re-read and the game's selection follow within the frames after)
        Rec $page $state "press.$b" "$r -> $(Steady { (BP).raw -replace ' bar=\d+', ' bar=<bar>' })"
    }
}
# A reading once two readings 20 frames apart agree (10 tries): the game's selection follows a press a moment later, later under load (13 Nov 2026:
# the inventory's press map read gameRow -1 once in three runs)
function Steady([scriptblock]$read) {
    $a = "$(& $read)"
    for ($i = 0; $i -lt 10; $i++) { $null = Wait-FLFramesPassed 20; $b = "$(& $read)"; if ($a -eq $b) { return $b }; $a = $b }
    $a
}
# The visit map (70b W1 (8); the client's st6::map<uint, char> at 0x673320, FREELANCER_RE "Visit record": +4 the head node, +0x10 the size; a node
# is left +0, parent +4, right +8, key +0xC, flags +0x10, nil byte +0x15): every id and its flag byte, in key order
function VisitMap {
    $head = U32At 0x673324; $size = U32At 0x673330
    $out = [Collections.Generic.SortedDictionary[uint32, int]]::new()
    if (-not $head -or $size -gt 5000) { return $out }
    function Node([uint32]$a) {
        $r = Send-FL ('peek 0x{0:x} 24' -f $a)
        $bytes = [Collections.Generic.List[byte]]::new()
        foreach ($l in ($r -split "`n")) { if ($l -match ':') { foreach ($t in (($l -split ':', 2)[1].Trim() -split '\s+')) { if ($t) { $bytes.Add([Convert]::ToByte($t, 16)) } } } }
        if ($bytes.Count -lt 24) { return $null }
        $a2 = $bytes.ToArray()
        [pscustomobject]@{ L = [BitConverter]::ToUInt32($a2, 0); R = [BitConverter]::ToUInt32($a2, 8); K = [BitConverter]::ToUInt32($a2, 12); V = [int]$a2[16]; Nil = $a2[21] -ne 0 }
    }
    $nil = U32At 0x673328
    $stack = [Collections.Generic.Stack[uint32]]::new()
    $root = [uint32](U32At ($head + 4))
    if ($root -and $root -ne $nil -and $root -ne $head) { $stack.Push($root) }
    $guard = 0
    while ($stack.Count -and $guard -lt 6000) {
        $guard++
        $n = Node $stack.Pop()
        if (-not $n -or $n.Nil) { continue }
        $out[$n.K] = $n.V
        foreach ($c in $n.L, $n.R) { if ($c -and $c -ne $head -and $c -ne $nil) { $stack.Push($c) } }
    }
    if ($out.Count -ne $size) { $out[[uint32]0] = -1 }   # (a read that did not walk the whole map: shows as id 0 = -1 in every diff and record)
    $out
}
function VisitDiff($a, $b) {
    $d = [Collections.Generic.List[string]]::new()
    foreach ($k in $b.Keys) { if (-not $a.ContainsKey($k)) { $d.Add("+$k=$($b[$k])") } elseif ($a[$k] -ne $b[$k]) { $d.Add("$k $($a[$k])->$($b[$k])") } }
    foreach ($k in $a.Keys) { if (-not $b.ContainsKey($k)) { $d.Add("-$k") } }
    if ($d.Count) { $d -join ' ' } else { 'none' }
}

# ---- the pages
function Walk-Board([string]$page) {
    $spot = if ($page -eq 'jobs') { 'IDS_HOTSPOT_MISSIONVENDOR' } else { 'IDS_HOTSPOT_NEWSVENDOR' }
    Open-Spot 'Bar' $spot 'board'
    Settle-BP   # (the first row is taken a moment after the opening: basepage Update BoardPick(0))
    $null = State $page 'open'
    # the rows: the focus walks down until it stops (the page offers as many as the game's window holds); the visit map before and after each
    # selection (the game reveals what a selected job or story names: 31_ section 4-5)
    $sw = [Diagnostics.Stopwatch]::StartNew(); $vm = VisitMap
    Rec $page 'visit' 'count' $vm.Count
    Note ("  visit map: {0} ids read in {1:n1} s" -f $vm.Count, $sw.Elapsed.TotalSeconds)
    $seq = @([int](BP).sel)
    for ($i = 0; $i -lt 12; $i++) {
        $was = [int](BP).sel
        if ($script:form -eq 'pad') { $moved = Pad-Step 'Down' { (BP).sel } }
        else {
            $r = @(RowHots 'left'); if ($was + 1 -ge $r.Count) { break }; Click $r[$was + 1] "row $($was + 1)"
            $moved = try { Await { [int](BP).sel -ne $was } "the next $page row" 2; $true } catch { $false }
        }
        $seq += [int](BP).sel
        if (-not $moved) { break }
        $null = Wait-FLFramesPassed 10
        $vm2 = VisitMap
        Rec $page 'visit' "select_row$((BP).sel)" (VisitDiff $vm $vm2)
        $vm = $vm2
    }
    Rec $page 'walk' 'sel_seq' ($seq -join ' ')
    Rec $page 'walk' 'rows' ([int]($seq | Measure-Object -Maximum).Maximum + 1)
    $null = State $page 'walked'
    if ($script:form -eq 'pad') {
        for ($i = 0; $i -lt 12 -and [int](BP).sel -gt 0; $i++) { $was = [int](BP).sel; Press @('Up'); Await { [int](BP).sel -lt $was } "the $page row up" 4 }
        if ((BP).bar -eq 'set') {
            Press @('Right'); Await { (BP).side -eq '1' } "the $page text side" 4
            $null = State $page 'text'
            $t0 = [int](BP).textAt
            $null = Send-FL 'basepage press down'; $null = Wait-FLFramesPassed 10   # (the text's scroll step by the page's own press: one step, whatever the frame rate)
            Rec $page 'text' 'textAt_after_down' "$((BP).textAt) (from $t0)"
            $null = Send-FL 'basepage press up'; $null = Wait-FLFramesPassed 10
            Rec $page 'text' 'textAt_after_up' "$((BP).textAt)"
            Press @('Left'); Await { (BP).side -eq '0' } "the $page list side" 4
        } else { Rec $page 'text' 'side' 'no text side: the text fits (READ not offered)' }
    } else {
        $r = @(RowHots 'left'); if ($r.Count) { Click $r[0] 'row 0'; Await { [int](BP).sel -eq 0 } "the first $page row" 4 }
    }
    Press-Map $page 'end'
    Close-Page $page
    if ($page -eq 'jobs') { Classic-Open 'jobs' 'Bar' $spot 'NN_MissionVendor' 'MissionVendorCloseButton' } else { Classic-Open 'news' 'Bar' $spot 'NN_News' 'NewsCloseButton' }
}

function Walk-Commodity {
    Open-Spot 'Trader' 'IDS_HOTSPOT_COMMODITYTRADER' 'commodity'
    $script:first = @{ '0' = 0; '1' = 0 }
    $start = Worth; $gear0 = Gear
    Rec 'commodity' 'open' 'worth' $start; Rec 'commodity' 'open' 'gear' $gear0
    $null = State 'commodity' 'open'
    $texts = 'TransferPriceText', 'QuantityTotalText', 'ErrorHelp'
    $names = Walk-List 'commodity' 'dealer' $texts
    # the good to trade: the first the dealer sells to this player
    $pick = -1
    for ($i = 0; $i -lt $names.Count -and $pick -lt 0; $i++) { Focus-Row $i; if ((BP).canTrade -eq '1' -and (Vis 'QuantitySlider')) { $pick = $i } }
    if ($pick -lt 0) { Rec 'commodity' 'trade' 'pick' 'none: no good this player can buy'; Close-Page 'commodity'; return }
    Focus-Row $pick
    $good = (BP).item
    Rec 'commodity' 'trade' 'pick' "$pick $good"
    $null = State 'commodity' 'dealer_row'
    # the quantity: one more, one less, the most
    $q0 = (BP).qty
    if ($script:form -eq 'pad') { Trigger 'RT' { (BP).qty -ne $q0 } 'RT: one more' } else { Click (ChipLines)[0][2] '+ chip'; Await { (BP).qty -ne $q0 } 'one more' }
    $null = State 'commodity' 'qty_more'
    $q1 = (BP).qty
    if ($script:form -eq 'pad') { Trigger 'LT' { (BP).qty -ne $q1 } 'LT: one less' } else { Click (ChipLines)[0][1] '- chip'; Await { (BP).qty -ne $q1 } 'one less' }
    Rec 'commodity' 'qty_less' 'qty' (BP).qty
    if ($script:form -eq 'pad') { Press @('Y') } else { Click (ChipLines)[0][3] 'MAX chip' }
    Await { $q = (BP).qty -split '/'; $q[0] -eq $q[1] } 'the most'
    $null = State 'commodity' 'qty_max'
    # back to one (70b W1 (7): buy 1): LT held on the pad, the - chip on the mouse
    if ($script:form -eq 'pad') { Trigger 'LT' { (BP).qty -match '^1/' } 'LT: down to one' 15 }
    else { for ($i = 0; $i -lt 200 -and (BP).qty -notmatch '^1/'; $i++) { $q = (BP).qty; Click (ChipLines)[0][1] '- chip'; Await { (BP).qty -ne $q } 'one less' } }
    Rec 'commodity' 'qty_one' 'qty' (BP).qty
    # BUY one: a hold on the pad (a tap never spends), the chip on the mouse
    $c0 = (BP).credits
    if ($script:form -eq 'pad') {
        Press @('A'); $null = Wait-FLFramesPassed 20
        Rec 'commodity' 'tap_a' 'credits' "$((BP).credits) (a tap spends nothing: was $c0)"
        Hold 'A' { (BP).credits -ne $c0 } 'A held: bought'
    } else { Click (ChipLines)[0][0] 'BUY chip'; Await { (BP).credits -ne $c0 } 'bought' }
    Await { [int](BP).yours -ge 1 } 'the good on your side'
    Rec 'commodity' 'bought' 'worth' (Worth); Rec 'commodity' 'bought' 'gear' (Gear)
    $null = State 'commodity' 'bought'
    # your side
    if ($script:form -eq 'pad') { Press @('Left') } else { $r = RowHots 'left'; Click $r[0] 'your first row' }
    Await { (BP).side -eq '0' } 'your side'
    $null = Walk-List 'commodity' 'yours' $texts
    $null = State 'commodity' 'yours'
    Close-Page 'commodity'
    # BEST PRICES: the inventory's (the good in the hold, filter 4), then the page
    if ('prices' -in $Pages) { Walk-Prices }
    # sell it back: the trader again, your side, the most, SELL
    Open-Spot 'Trader' 'IDS_HOTSPOT_COMMODITYTRADER' 'commodity'
    $script:first = @{ '0' = 0; '1' = 0 }
    if ($script:form -eq 'pad') { Press @('Left') } else { $r = RowHots 'left'; Click $r[0] 'your first row' }
    Await { (BP).side -eq '0' -and (BP).item -eq $good } 'your side on the good'
    $null = State 'commodity' 'sell_row'
    $c1 = (BP).credits
    if ($script:form -eq 'pad') { Hold 'A' { (BP).credits -ne $c1 } 'A held: sold' } else { Click (ChipLines)[0][0] 'SELL chip'; Await { (BP).credits -ne $c1 } 'sold' }
    Await { [int](BP).yours -eq 0 } 'the hold empty of it'
    $end = Worth
    Rec 'commodity' 'sold' 'worth' $end; Rec 'commodity' 'sold' 'gear' (Gear)
    Rec 'commodity' 'sold' 'restored' "$(if ($end -eq $start -and (Gear) -eq $gear0) { 'yes' } else { "no: $start -> $end" })"
    $null = State 'commodity' 'sold'
    Press-Map 'commodity' 'end'
    Close-Page 'commodity'
    Classic-Open 'commodity' 'Trader' 'IDS_HOTSPOT_COMMODITYTRADER' 'NN_Dealer' 'Inventory_Close'
}

# BEST PRICES (NN_CommodityByBase), from the inventory with the good in the hold: X on the pad, the BEST PRICES chip on the mouse
function Walk-Prices {
    Wait-FLRoomReady
    $null = Send-FL 'tap USER_INVENTORY'
    $null = Wait-FLUntil { (Send-FL 'nnpage') -match '^nnpage page=2 ' } 'the inventory page' 10
    $null = Wait-FLFramesPassed 10
    for ($i = 0; $i -lt 6 -and [regex]::Match((Send-FL 'nnpage'), ' filter=(\d+)').Groups[1].Value -ne '4'; $i++) {
        $f = [regex]::Match((Send-FL 'nnpage'), ' filter=(\d+)').Groups[1].Value
        if ($script:form -eq 'pad') { Press @('Right') }
        else {
            # the inventory's five filter tabs: the row of five short hotspots under the strip (nn_pages' Layout)
            $tabs = (NNTabs)
            Click $tabs[4] 'the inventory''s goods tab'
        }
        Await { [regex]::Match((Send-FL 'nnpage'), ' filter=(\d+)').Groups[1].Value -ne $f } 'the next inventory filter' 5
    }
    $null = Wait-FLUntil { (Send-FL 'nnpage') -match ' items=[1-9]' } 'the good in the inventory' 5
    # (the game drops a NeuroNet window's commands while one slides: docked the room says so, `hotspots` busy: nn-window - nn_pages)
    $null = Wait-FLUntil { (((Send-FL 'hotspots') -split "`n")[-1]) -match 'ready$' } 'no NeuroNet window sliding' 15
    $null = Wait-FLFramesPassed 20
    # (the inventory's action chips stand from 0.425 W in the lane, its CLOSE at the lane's right end: nnpage.cpp)
    if ($script:form -eq 'pad') { Press @('X') } else { Click (LaneHots | Where-Object { $_.X0 -ge 0.42 * $script:W -and $_.X1 -le 0.9 * $script:W } | Select-Object -Last 1) 'the BEST PRICES chip' }
    # FINDING (audit 72_): in the mouse form at 1280x800 the first click on BEST PRICES right after the goods tab's click opened nothing in 2 of 3
    # full runs (taken in the game, `clicks` counted; the game's row stays unselected, gameRow=-1, in both the failing and the working case). Clicked once
    # more after 4 s, and said so in steps.txt (not a record field: it would make the check flaky; the opening itself is compared)
    $opened = try { Await { (Kind) -eq 'prices' } 'the best prices page' 4; $true } catch { $false }
    if (-not $opened) {
        Note '  FINDING: the first BEST PRICES press opened nothing: pressed again'
        if ($script:form -eq 'pad') { Press @('X') } else { Click (LaneHots | Where-Object { $_.X0 -ge 0.42 * $script:W -and $_.X1 -le 0.9 * $script:W } | Select-Object -Last 1) 'the BEST PRICES chip, again' }
        Await { (Kind) -eq 'prices' } 'the best prices page' 10
    }
    $script:reached = "save $($script:save) with one good bought, the inventory page on its goods filter, $(if ($script:form -eq 'pad') { 'X' } else { 'the BEST PRICES chip' })"
    $null = State 'prices' 'open'
    $b = BP
    $n = [int]$b.n
    if ($script:form -eq 'pad' -and $n -gt 0) {
        $seq = @([int]$b.sel)
        for ($i = 0; $i -le $n; $i++) { Press @('Down'); $null = Wait-FLFramesPassed 4; $seq += [int](BP).sel }
        Rec 'prices' 'walk' 'sel_seq' ($seq -join ' ')
        for ($i = 0; $i -le $n; $i++) { Press @('Up'); $null = Wait-FLFramesPassed 4 }
        Rec 'prices' 'walk' 'sel_back' (BP).sel
    }
    Press-Map 'prices' 'open' @('up', 'down', 'x', 'y')
    if ($script:form -eq 'pad') { Press @('B') } else { Click (LaneHots)[0] 'CLOSE (best prices)' }
    Await { (Kind) -ne 'prices' } 'best prices closed' 10
    $null = Wait-FLUntil { (Send-FL 'nnpage') -match '^nnpage page=2 ' } 'the inventory back' 10
    Rec 'prices' 'closed' 'back_to' 'the inventory page'
    $null = Wait-FLFramesPassed 10
    if ($script:form -eq 'pad') { Press @('B') } else { $null = Send-FL 'nnstrip close' }
    $null = Wait-FLUntil { (Send-FL 'nnpage') -match '^nnpage page=0 ' -and (Send-FL 'nnstrip') -match 'standing=0' } 'the inventory closed' 15
    Wait-FLRoomReady
}

function Walk-Equipment {
    Open-Spot 'Equipment' 'IDS_HOTSPOT_EQUIPMENTDEALER' 'equipment'
    $script:first = @{ '0' = 0; '1' = 0 }
    $start = Worth; $gear0 = Gear
    Rec 'equipment' 'open' 'worth' $start; Rec 'equipment' 'open' 'gear' $gear0
    $null = State 'equipment' 'open'
    $texts = 'TransferPriceText', 'ErrorHelp'
    # every filter: the dealer's rows and yours
    $f0 = (BP).filter
    for ($k = 0; $k -lt 4; $k++) {
        $f = (BP).filter
        $null = Walk-List 'equipment' "dealer_f$f" $texts
        if ($script:form -eq 'pad') { Press @('Left') } else { $r = RowHots 'left'; if ($r.Count) { Click $r[0] 'your first row' } }
        if ([int](BP).yours -gt 0) { Await { (BP).side -eq '0' } 'your side' }
        $script:first['0'] = 0
        $null = Walk-List 'equipment' "yours_f$f" $texts
        $null = State 'equipment' "yours_f$f"
        if ($script:form -eq 'pad') { Press @('Right') } else { $r = RowHots 'right'; Click $r[0] 'the dealer''s first row' }
        Await { (BP).side -eq '1' } 'the dealer''s side'
        $script:first['1'] = 0
        $next = ([int]$f + 1) % 4
        if ($script:form -eq 'pad') { Press @('X') } else { Click (TabHots)[$next] "filter tab $next" }
        Await { (BP).filter -eq "$next" } "filter $next"
        $null = State 'equipment' "filter$next"
    }
    Rec 'equipment' 'filters' 'round' "$(if ((BP).filter -eq $f0) { 'four steps come round' } else { "ended on $((BP).filter), began on $f0" })"
    # weapons, your side, the first mounted gun: compare tag, unmount and mount
    if ((BP).filter -ne '0') { if ($script:form -eq 'pad') { for ($i = 0; $i -lt 4 -and (BP).filter -ne '0'; $i++) { Press @('X'); $null = Wait-FLFramesPassed 6 } } else { Click (TabHots)[0] 'filter tab 0' } }
    Await { (BP).filter -eq '0' } 'the weapons filter'
    if ($script:form -eq 'pad') { Press @('Left') } else { Click (RowHots 'left')[0] 'your first row' }
    Await { (BP).side -eq '0' } 'your side'
    $script:first['0'] = 0
    $gun = -1
    for ($i = 0; $i -lt [int](BP).yours -and $gun -lt 0; $i++) { Focus-Row $i; if ((BP).mounted -eq '1') { $gun = $i } }
    Rec 'equipment' 'gun' 'row' $gun
    if ($gun -ge 0) {
        $null = State 'equipment' 'gun'
        $t0 = (BP).tagged
        if ($script:form -eq 'pad') { Press @('RS') } else { Click ((ChipLines) | Select-Object -Last 1)[0] 'the COMPARE chip' }
        Await { (BP).tagged -ne $t0 } 'the compare tag'
        $null = State 'equipment' 'gun_tagged'
        if ($script:form -eq 'pad') { Press @('RS') } else { Click ((ChipLines) | Select-Object -Last 1)[0] 'the UNTAG chip' }
        Await { (BP).tagged -eq $t0 } 'the compare tag back'
        Rec 'equipment' 'gun_untagged' 'tagged' (BP).tagged
        $guns0 = @((Gear) -split ' ' | Where-Object { $_ -match ':1:HpWeapon' }).Count
        if ($script:form -eq 'pad') { Press @('LS') } else { $l = (ChipLines)[0]; Click $l[$l.Count - 1] 'the UNMOUNT chip' }
        Await { @((Gear) -split ' ' | Where-Object { $_ -match ':1:HpWeapon' }).Count -eq $guns0 - 1 } 'a gun unmounted'
        Await { (BP).mounted -eq '0' } 'the focus on the unmounted gun'
        Rec 'equipment' 'unmounted' 'gear' (Gear)
        $null = State 'equipment' 'unmounted'
        if ($script:form -eq 'pad') { Press @('LS') } else { $l = (ChipLines)[0]; Click $l[$l.Count - 1] 'the MOUNT chip' }
        Await { @((Gear) -split ' ' | Where-Object { $_ -match ':1:HpWeapon' }).Count -eq $guns0 } 'the gun mounted again'
        Await { (BP).mounted -eq '1' } 'the focus on the mounted gun'
        Rec 'equipment' 'mounted' 'gear' (Gear)
        Rec 'equipment' 'mounted' 'restored' "$(if ((Gear) -eq $gear0) { 'yes' } else { 'no' })"
        $null = State 'equipment' 'mounted'
    }
    # buy the dealer's first gun this player may buy, then sell it back
    if ($script:form -eq 'pad') { Press @('Right') } else { Click (RowHots 'right')[0] 'the dealer''s first row' }
    Await { (BP).side -eq '1' } 'the dealer''s side'
    $script:first['1'] = 0
    $pick = -1
    for ($i = 0; $i -lt [int](BP).theirs -and $pick -lt 0; $i++) { Focus-Row $i; if ((BP).canTrade -eq '1') { $pick = $i } }
    Rec 'equipment' 'trade' 'pick' "$pick $(if ($pick -ge 0) { (BP).item })"
    if ($pick -ge 0) {
        $item = (BP).item; $c0 = (BP).credits
        $null = State 'equipment' 'dealer_row'
        if ($script:form -eq 'pad') { Hold 'A' { (BP).credits -ne $c0 } 'A held: bought' } else { Click (ChipLines)[0][0] 'BUY chip'; Await { (BP).credits -ne $c0 } 'bought' }
        $null = Wait-FLUntil { (Gear) -ne $gear0 } 'the equipment list after the buy' 5
        Rec 'equipment' 'bought' 'worth' (Worth); Rec 'equipment' 'bought' 'gear' (Gear)
        $null = State 'equipment' 'bought'
        if ($script:form -eq 'pad') { Press @('Left') } else { Click (RowHots 'left')[0] 'your first row' }
        Await { (BP).side -eq '0' } 'your side'
        $script:first['0'] = 0
        $mine = -1
        $n = [int](BP).yours
        for ($i = $n - 1; $i -ge 0 -and $mine -lt 0; $i--) { Focus-Row $i; if ((BP).item -eq $item -and (BP).canTrade -eq '1') { $mine = $i } }
        Rec 'equipment' 'sell' 'row' $mine
        if ($mine -ge 0) {
            $null = State 'equipment' 'sell_row'
            $c1 = (BP).credits
            if ($script:form -eq 'pad') { Hold 'A' { (BP).credits -ne $c1 } 'A held: sold' } else { Click (ChipLines)[0][0] 'SELL chip'; Await { (BP).credits -ne $c1 } 'sold' }
            $null = Wait-FLUntil { (Gear) -eq $gear0 } 'the equipment list as it began' 5
            $end = Worth
            Rec 'equipment' 'sold' 'worth' $end; Rec 'equipment' 'sold' 'gear' (Gear)
            Rec 'equipment' 'sold' 'restored' "gear $(if ((Gear) -eq $gear0) { 'as it began' } else { 'changed' }); $start -> $end"
            $null = State 'equipment' 'sold'
        }
    }
    # sell one of a stack owned before the visit (70b W1 (7): the old item's price base differs from a just-bought one's, 32_ check 4): the internal
    # filter's first stack on your side (nanobots in SaveC701)
    if ((BP).filter -ne '3') {
        if ($script:form -eq 'pad') { for ($i = 0; $i -lt 4 -and (BP).filter -ne '3'; $i++) { $f = (BP).filter; Press @('X'); Await { (BP).filter -ne $f } 'the next filter' } }
        else { Click (TabHots)[3] 'filter tab 3' }
    }
    Await { (BP).filter -eq '3' } 'the internal filter'
    Settle-BP
    if ((BP).side -ne '0') { if ($script:form -eq 'pad') { Press @('Left') } else { Click (RowHots 'left')[0] 'your first row' }; Await { (BP).side -eq '0' } 'your side' }
    $script:first['0'] = 0
    Focus-Row 0
    Settle-BP
    Rec 'equipment' 'old' 'item' "$((BP).item) qty=$((BP).qty) trade=$((BP).canTrade)"
    if ((BP).canTrade -eq '1' -and (BP).qty -notmatch '^1/' -and (Vis 'QuantitySlider')) {
        # (the sale's quantity opens at the whole stack: down to one, LT held on the pad, the - chip on the mouse)
        if ($script:form -eq 'pad') { Trigger 'LT' { (BP).qty -match '^1/' } 'LT: down to one' 15 }
        else { for ($i = 0; $i -lt 200 -and (BP).qty -notmatch '^1/'; $i++) { $q = (BP).qty; Click (ChipLines)[0][1] '- chip'; Await { (BP).qty -ne $q } 'one less' } }
    }
    if ((BP).canTrade -eq '1') {
        $g1 = Gear; $c2 = (BP).credits
        $null = State 'equipment' 'old_row'
        if ($script:form -eq 'pad') { Hold 'A' { (BP).credits -ne $c2 } 'A held: one old unit sold' } else { Click (ChipLines)[0][0] 'SELL chip'; Await { (BP).credits -ne $c2 } 'one old unit sold' }
        $null = Wait-FLUntil { (Gear) -ne $g1 } 'the equipment list after the sale' 5
        Rec 'equipment' 'old_sold' 'worth' (Worth); Rec 'equipment' 'old_sold' 'gear' (Gear)
        $null = State 'equipment' 'old_sold'
    }
    Press-Map 'equipment' 'end'
    Close-Page 'equipment'
    Classic-Open 'equipment' 'Equipment' 'IDS_HOTSPOT_EQUIPMENTDEALER' 'NN_Dealer' 'Inventory_Close'
}

# The bar: a rumour (the game's offer dialog in the skin); the characters and their offers vary at each start (masked)
function Walk-Bar {
    $global:FLKeepPopups = $true
    try {
        Wait-FLRoomReady
        if ((((Send-FL 'hotspots') -split "`n") | Where-Object { $_ -match 'IDS_HOTSPOT_MISSIONVENDOR' }).Count -eq 0) { $null = Goto-FLRoom 'Bar'; Wait-FLRoomReady }
        $t = Send-FL 'talkers'
        Rec 'bar' 'room' 'talkers' (($t -split "`n" | Select-Object -Skip 1 | ForEach-Object { ($_ -split '\s+')[-1] } | Where-Object { $_ } | Sort-Object) -join ',')
        # a rumour when one stands, else an info, a bribe or a mission offer (each the same dialog; opened and closed, never accepted)
        $kind = @('talk_rumor', 'talk_info', 'talk_bribe', 'talk_mission') | Where-Object { $t -match $_ } | Select-Object -First 1
        if (-not $kind) { $kind = '' }
        Rec 'bar' 'room' 'kind' $(if ($kind) { $kind } else { 'none this start' })
        if (-not $kind) { return }
        $r = Send-FL "talk $kind"
        Rec 'bar' 'room' 'givers' $r
        # (the character's talk is a room scene first, skipped as a player's Escape does, only while the game reports it: Skip-FLAnimation)
        $null = Wait-FLUntil { if ((Vis 'OfferDialogClose2Button') -or (Vis 'OfferDialogCloseButton')) { $true } else { $null = Skip-FLAnimation; $false } } 'the offer''s dialog' 60
        $s = Stable
        $o = [ordered]@{}
        $o.dialog = (@('OfferDialogClose2Button', 'OfferDialogCloseButton', 'OfferDialogAcceptButton') | Where-Object { Vis $_ }) -join ','
        foreach ($k in 'lane.items', 'lane.screen', 'lane.count', 'lane.visible', 'hot.page', 'hot.strip') { if ($s.Contains($k)) { $o[$k] = $s[$k] } }
        RecAll 'bar' 'rumour' $o
        $null = Shot 'bar_rumour'
        if ($script:form -eq 'pad') { Press @('B') } else { $null = Send-FL "click $(if (Vis 'OfferDialogClose2Button') { 'OfferDialogClose2Button' } else { 'OfferDialogCloseButton' })" }
        $null = Wait-FLUntil { -not (Vis 'OfferDialogClose2Button') -and -not (Vis 'OfferDialogCloseButton') } 'the rumour closed' 20
        Wait-FLRoomReady
        Rec 'bar' 'closed' 'why' 'closed'
    } finally { $global:FLKeepPopups = $false }
}

# A job accepted (one per game: after a save load the board offers nothing new, HARNESS.md "Missions"); the board's own ACCEPT: held A or the chip
function Walk-Accept {
    Open-Spot 'Bar' 'IDS_HOTSPOT_MISSIONVENDOR' 'board'
    $null = Wait-FLUntil { (Vis 'MissionVendorAcceptButton') -or (Vis 'MissionVendorSummaryBox0') } 'the job rows' 10
    $w0 = Worth
    if (-not (Vis 'MissionVendorAcceptButton')) { Rec 'accept' 'board' 'accept' 'not offered'; Close-Page 'jobs'; return }
    $null = State 'accept' 'before'
    if ($script:form -eq 'pad') {
        Press @('A'); $null = Wait-FLFramesPassed 20
        Rec 'accept' 'tap_a' 'board' "$(if ((Kind) -eq 'board') { 'still open (a tap accepts nothing)' } else { 'closed' })"
        Hold 'A' { (Kind) -ne 'board' -or -not (Vis 'MissionVendorAcceptButton') } 'A held: accepted'
    } else { Click (LaneHots)[1] 'ACCEPT JOB chip'; Await { (Kind) -ne 'board' -or -not (Vis 'MissionVendorAcceptButton') } 'accepted' }
    $null = Wait-FLFramesPassed 30
    Rec 'accept' 'after' 'page' (Kind)
    Rec 'accept' 'after' 'worth' (Worth)
    Rec 'accept' 'after' 'credits_unchanged' "$(if ((Worth) -eq $w0) { 'yes' } else { 'no' })"
    Rec 'accept' 'after' 'story' (Get-FLStoryState)
    $obj = (Get-FLEvents | Where-Object { $_ -match 'objective|mission' } | Select-Object -Last 1)
    Rec 'accept' 'after' 'mission_event' $(if ($obj) { 'an objective / mission event came' } else { 'none' })
    $null = Shot 'accept_after'
    $script:accepted = $true
    if ((Kind) -eq 'board') { Close-Page 'jobs' }
}
# The job board AFTER A REFUSED JOB (fixed in main ec6c2d1c; audit 109_ 2d): a job is active (Walk-Accept took one), the board is opened again and
# ACCEPT pressed: the game refuses, and the page must show the game's "Mission Rejected" caption and offer no ACCEPT until a job is chosen. Recorded under
# page 'accept': states refused (right after the refusal) and rechosen (a row taken again: ACCEPT back). Walked in each form the run has, so both
# forms stand at both sizes though the accept itself is one per game. Steps as research\experience\lanes\fixes4_2026-11-13\scratch\jobs\refuse.ps1.
function Walk-Refused {
    Open-Spot 'Bar' 'IDS_HOTSPOT_MISSIONVENDOR' 'board'
    $null = Wait-FLUntil { (Vis 'MissionVendorAcceptButton') -or (Vis 'MissionVendorSummaryBox0') } 'the job rows' 10
    if (-not (Vis 'MissionVendorAcceptButton')) { Rec 'accept' 'refused' 'board' 'accept not offered'; Close-Page 'jobs'; return }
    # (the game's caption control; its ACCEPT control stays "visible" in the parked window: the page's prompts and texts, recorded below, say what is offered)
    # (a job chosen first, as a player's ACCEPT needs one: the page takes the first row once per opening, a moment after it, and would take it AFTER a refusal
    # that came sooner - basepage.cpp g_boardPicked; the first walk's State 'before' gives it the same time)
    Settle-BP; Await { [int](BP).sel -ge 0 } 'the first job chosen' 10
    $gone = { (Kind) -ne 'board' -or [int](BP).sel -lt 0 }   # (the game drops its selection to -1 at a refusal)
    if ($script:form -eq 'pad') { Hold 'A' $gone 'A held: the accept refused' } else { Click (LaneHots)[1] 'ACCEPT JOB chip'; Await $gone 'the accept refused' }
    $null = Wait-FLFramesPassed 30
    Rec 'accept' 'refused' 'selection_cleared' "$([int](BP).sel -lt 0)"
    if ((Kind) -ne 'board') { Rec 'accept' 'refused' 'page' "closed: $(Kind)"; return }
    $s = State 'accept' 'refused'
    $rejectedDrawn = "$($s['texts'])"; $refusedLane = "$($s['lane.items'])"
    Rec 'accept' 'refused' 'caption_drawn' "$([bool]($rejectedDrawn -match 'Mission Rejected'))"   # (the page's own drawn text, `page texts`)
    Rec 'accept' 'refused' 'accept_prompt' "$([bool]($s['lane.items'] -match 'ACCEPT'))"       # (the pad's lane / the mouse's chips: no ACCEPT offered)
    # until a job is chosen: a row taken again brings ACCEPT back
    if ($script:form -eq 'pad') { Press @('Down') } else { $r = @(RowHots 'left'); if ($r.Count) { Click $r[0] 'row 0' } }
    $chosen = try { Await { [int](BP).sel -ge 0 } 'a job chosen again' 6; $true } catch { $false }
    Rec 'accept' 'rechosen' 'job_chosen' "$chosen"
    $s = State 'accept' 'rechosen'
    Rec 'accept' 'rechosen' 'accept_prompt' "$([bool]($s['lane.items'] -match 'ACCEPT'))"
    Close-Page 'jobs'
    # what the fix (ec6c2d1c) promises, judged here as well as recorded: a walk error, i.e. a FAIL, until the baseline holds the states too
    $bad = @()
    if ($rejectedDrawn -notmatch 'Mission Rejected') { $bad += 'the page does not draw the game''s "Mission Rejected" caption' }
    if ($script:form -eq 'pad' -and $refusedLane -match 'ACCEPT') { $bad += "the pad's lane offers ACCEPT after the refusal ($refusedLane)" }
    if ($script:form -eq 'pad' -and $s['lane.items'] -notmatch 'ACCEPT') { $bad += "the pad's lane does not offer ACCEPT again once a job is chosen ($($s['lane.items']))" }
    if (-not $chosen) { $bad += 'a row taken again did not choose a job' }
    if ($bad) { throw "the job board after a refused job: $($bad -join '; ')" }
}

function Walk-Repair {
    Classic-Open 'repair' 'Equipment' 'IDS_NN_REPAIR_YOUR_SHIP' 'NN_Repair' 'RepairCancelButton'   # (before the repair: its rows)
    Open-Spot 'Equipment' 'IDS_NN_REPAIR_YOUR_SHIP' 'repair'
    $null = Wait-FLUntil { (BP).list.Count -gt 0 } 'the repair rows' 10
    $w0 = Worth; $g0 = Gear
    Rec 'repair' 'open' 'worth' $w0; Rec 'repair' 'open' 'gear' $g0
    $null = State 'repair' 'open'
    $n = (BP).list.Count
    # the rows: pad D-pad down to the last and up; the mouse's click takes a row and ticks it (MendPick): its walk is the tick test below
    if ($script:form -eq 'pad') {
        $seq = @([int](BP).sel)
        for ($i = 0; $i -lt $n -and (Pad-Step 'Down' { (BP).sel }); $i++) { $seq += [int](BP).sel }
        Rec 'repair' 'walk' 'sel_seq' ($seq -join ' ')
        for ($i = 0; $i -lt $n -and [int](BP).sel -gt 0; $i++) { $null = Pad-Step 'Up' { (BP).sel } }
        Rec 'repair' 'walk' 'sel_back' (BP).sel
        $t0 = (BP).total
        Press @('A'); Await { (BP).total -ne $t0 } 'A: the first row unticked'
        $null = State 'repair' 'untick'
        Press @('A'); Await { (BP).total -eq $t0 } 'A: ticked again'
        Press @('X'); Await { (BP).total -ne $t0 } 'X: none ticked'
        $null = State 'repair' 'none'
        Press @('X'); Await { (BP).total -eq $t0 } 'X: all ticked again'
        Rec 'repair' 'all' 'total' (BP).total
        Hold 'Y' { (Worth) -ne $w0 } 'Y held: repaired'
    } else {
        $t0 = (BP).total
        $rows = RowHots 'right'; if (-not $rows.Count) { $rows = RowHots 'left' }
        Rec 'repair' 'rows' 'hotspots' $rows.Count
        if ($rows.Count -ge 2) { Click $rows[1] 'row 1'; Await { [int](BP).sel -eq 1 -and (BP).total -ne $t0 } 'row 1 taken and unticked'; $null = State 'repair' 'untick'; Click $rows[1] 'row 1 again'; Await { (BP).total -eq $t0 } 'row 1 ticked again' }
        $chips = (ChipLines) | Where-Object { $_.Count -eq 2 } | Select-Object -First 1
        Click $chips[0] 'TICK ALL / NONE chip'; Await { (BP).total -ne $t0 } 'none ticked'
        $null = State 'repair' 'none'
        Click $chips[0] 'TICK ALL / NONE chip'; Await { (BP).total -eq $t0 } 'all ticked again'
        Rec 'repair' 'all' 'total' (BP).total
        Click $chips[1] 'REPAIR chip'; Await { (Worth) -ne $w0 } 'repaired'
    }
    $null = Wait-FLUntil { (Gear) -ne $g0 } 'the equipment list after the repair' 5
    Rec 'repair' 'repaired' 'worth' (Worth); Rec 'repair' 'repaired' 'gear' (Gear)
    $null = State 'repair' 'repaired'
    Press-Map 'repair' 'end'
    Close-Page 'repair'
}

function Walk-Ships {
    Classic-Open 'ships' 'ShipDealer' 'IDS_HOTSPOT_SHIPDEALER' 'NN_ShipTrader' 'ShipTraderCancelButton'   # (before the purchase; its final stage too)
    Open-Spot 'ShipDealer' 'IDS_HOTSPOT_SHIPDEALER' 'ships'
    $null = Wait-FLUntil { (BP).list.Count -gt 0 } 'the ships' 10
    $w0 = Worth; $g0 = Gear; $ship0 = Ship
    Rec 'ships' 'open' 'worth' $w0; Rec 'ships' 'open' 'gear' $g0; Rec 'ships' 'open' 'ship' $ship0
    $null = State 'ships' 'open'
    $n = (BP).list.Count
    for ($i = 0; $i -lt $n; $i++) {
        if ([int](BP).sel -ne $i) { $was = (BP).sel; if ($script:form -eq 'pad') { $null = Pad-Step 'Down' { (BP).sel } } else { Click (RowHots 'left')[$i] "ship $i" }; Await { [int](BP).sel -eq $i } "ship $i" }
        $b = BP
        Rec 'ships' 'walk' "ship$i" "sel=$($b.sel) can=$($b.can) price=$(GameText 'ShipDealerShipPrice') why=$(GameText 'ShipBuyProblem')"
    }
    if ($script:form -eq 'pad') {
        Press @('Down'); $null = Wait-FLFramesPassed 6; Rec 'ships' 'walk' 'sel_after_last' (BP).sel
        Press @('Right'); Await { (BP).side -eq '1' } 'the ship side'
        $null = State 'ships' 'card'
        $y0 = (BP).yaw
        Set-FLPad -RX 1; try { Await { (BP).yaw -ne $y0 } 'RS turns the ship' 5 } finally { Clear-FLPad }
        Rec 'ships' 'card' 'turned' 'yes'
        $z0 = (BP).zoom
        Trigger 'RT' { (BP).zoom -ne $z0 } 'RT zooms'
        Rec 'ships' 'card' 'zoomed' 'yes'
        Press @('Left'); Await { (BP).side -eq '0' } 'the list side'
    }
    # the first ship this player can buy: its final stage
    $pick = -1
    for ($i = 0; $i -lt $n -and $pick -lt 0; $i++) { if (((BP).list[$i]) -match 'ref=0 ') { $pick = $i } }
    Rec 'ships' 'buy' 'pick' $pick
    if ($pick -lt 0) { Close-Page 'ships'; return }
    for ($i = 0; $i -lt $n -and [int](BP).sel -ne $pick; $i++) { $was = [int](BP).sel; if ($script:form -eq 'pad') { Press @($(if ($pick -gt $was) { 'Down' } else { 'Up' })) } else { Click (RowHots 'left')[$pick] "ship $pick" }; Await { [int](BP).sel -ne $was } 'the ship to buy' }
    $null = State 'ships' 'pick'
    if ($script:form -eq 'pad') {
        Press @('A'); $null = Wait-FLFramesPassed 20; Rec 'ships' 'tap_a' 'page' "$(Kind) (a tap buys nothing)"
        Hold 'A' { (Kind) -eq 'final' } 'A held: the final stage'
    } else { Click ((ChipLines) | Select-Object -Last 1)[0] 'BUY THIS SHIP chip'; Await { (Kind) -eq 'final' } 'the final stage' }
    $script:first = @{ '0' = 0; '1' = 0 }
    $null = State 'final' 'open'
    $texts = 'TransferPriceText', 'ErrorHelp'
    if ($script:form -eq 'pad') { Press @('Left'); Await { (BP).stop -eq '0' -and (BP).side -eq '0' } 'the new ship''s side' }
    else { $r = RowHots 'left'; if ($r.Count) { Click $r[0] 'the new ship''s first row'; Await { (BP).side -eq '0' } 'the new ship''s side' } }
    $null = State 'final' 'yours'
    # the five filters, each one's rows
    for ($k = 0; $k -lt 5; $k++) {
        Settle-BP
        $b = BP; $f = $b.filter; $sd = if ($b.side -eq '0') { 'yours' } else { 'theirs' }
        $script:first = @{ '0' = 0; '1' = 0 }
        if ([int]$(if ($b.side -eq '0') { $b.yours } else { $b.theirs }) -gt 0) { $null = Walk-List 'final' "${sd}_f$f" $texts } else { Rec 'final' "${sd}_f$f" 'rows' 0 }
        $next = ([int]$f + 1) % 5
        if ($script:form -eq 'pad') { Press @('X') } else { Click (TabHots)[$next] "filter tab $next" }
        Await { (BP).filter -eq "$next" } "filter $next"
    }
    Settle-BP
    $null = State 'final' 'filters_round'
    # sell the first sellable item to the dealer and take it back
    if ((BP).filter -ne '0') { if ($script:form -eq 'pad') { for ($i = 0; $i -lt 5 -and (BP).filter -ne '0'; $i++) { Press @('X'); $null = Wait-FLFramesPassed 6 } } else { Click (TabHots)[0] 'filter tab 0' } }
    Await { (BP).filter -eq '0' } 'the weapons filter'
    Settle-BP
    if ($script:form -eq 'kbm' -and (BP).side -ne '0') { $r = RowHots 'left'; if ($r.Count) { Click $r[0] 'the first row'; Await { (BP).side -eq '0' } 'the new ship''s side' } }
    $script:first['0'] = 0
    Focus-Row 0
    if ((BP).canTrade -eq '1') {
        $t0 = [int](BP).theirs; $item = (BP).item
        if ($script:form -eq 'pad') { Hold 'A' { [int](BP).theirs -gt $t0 } 'A held: sold to the dealer' } else { Click ((ChipLines) | Where-Object { $_.Count -le 4 } | Select-Object -Last 1)[0] 'SELL TO DEALER chip'; Await { [int](BP).theirs -gt $t0 } 'sold to the dealer' }
        $null = State 'final' 'sold_to_dealer'
        if ($script:form -eq 'pad') { Press @('Right'); $null = Wait-FLFramesPassed 4; if ((BP).stop -eq '1') { Press @('Right') } } else { Click (RowHots 'right')[0] 'the dealer''s first row' }
        Await { (BP).side -eq '1' -and ($script:form -ne 'pad' -or (BP).stop -ne '1') } 'the dealer''s side'   # (the BUY SHIP stop is the pad's only)
        $script:first['1'] = 0
        Focus-Row 0
        $t1 = [int](BP).theirs
        if ($script:form -eq 'pad') { Hold 'A' { [int](BP).theirs -lt $t1 } 'A held: taken back' } else { Click ((ChipLines) | Where-Object { $_.Count -le 4 } | Select-Object -Last 1)[0] 'TAKE BACK chip'; Await { [int](BP).theirs -lt $t1 } 'taken back' }
        $null = State 'final' 'taken_back'
    } else { Rec 'final' 'trade' 'sell' "nothing sellable on filter 0 ($((BP).item))" }
    # BACK TO SHIPS and in again, then the purchase
    if ($script:form -eq 'pad') { Press @('B') } else { Click (LaneHots)[1] 'BACK TO SHIPS chip' }
    Await { (Kind) -eq 'ships' } 'back to the ships'
    $null = State 'ships' 'back'
    Press-Map 'ships' 'back'
    if ($script:form -eq 'pad') {
        $null = Send-FL 'basepage press left'
        for ($i = 0; $i -lt $n -and [int](BP).sel -gt 0; $i++) { $null = Send-FL 'basepage press up'; $null = Wait-FLFramesPassed 4 }
        Await { (BP).side -eq '0' -and [int](BP).sel -eq 0 } 'the list''s first ship again'
    }
    for ($i = 0; $i -lt $n -and [int](BP).sel -ne $pick; $i++) { $was = [int](BP).sel; if ($script:form -eq 'pad') { Press @($(if ($pick -gt $was) { 'Down' } else { 'Up' })) } else { Click (RowHots 'left')[$pick] "ship $pick" }; Await { [int](BP).sel -ne $was } 'the ship to buy' }
    if ($script:form -eq 'pad') { Hold 'A' { (Kind) -eq 'final' } 'A held: the final stage' } else { Click ((ChipLines) | Select-Object -Last 1)[0] 'BUY THIS SHIP chip'; Await { (Kind) -eq 'final' } 'the final stage' }
    $null = Wait-FLFramesPassed 10
    $null = State 'final' 'again'
    Press-Map 'final' 'again'
    if ($script:form -eq 'pad') {
        if ((BP).stop -ne '1') { Press @('Right'); Await { (BP).stop -eq '1' } 'the BUY SHIP stop' }
        Hold 'A' { (Ship) -ne $ship0 } 'A held on BUY SHIP: bought' 12
    } else { Click ((ChipLines)[0])[0] 'BUY SHIP chip'; Await { (Ship) -ne $ship0 } 'bought' 12 }
    $null = Wait-FLUntil { (Kind) -ne 'final' } 'the final stage gone' 15
    $null = Wait-FLFramesPassed 30
    Rec 'ships' 'bought' 'ship' (Ship); Rec 'ships' 'bought' 'worth' (Worth); Rec 'ships' 'bought' 'gear' (Gear); Rec 'ships' 'bought' 'page' (Kind)
    $null = Shot 'ships_bought'
    if ((Kind) -ne 'none') { Close-Page 'ships' }
}

# Jettison (the inventory page in space, nnpage; F4's): one unit of the first ammunition stack that is not a mine, through the game's panel, from SaveC706
function Walk-Jettison {
    # (in space the game may drop the first command after a load or close: sent once more when the window did not come, as nn_pages does)
    $null = Send-FL 'tap USER_INVENTORY'
    $came = try { Await { (Send-FL 'nnpage') -match '^nnpage page=2 ' } 'the inventory page' 4; $true } catch { $false }
    if (-not $came) { Note '  the game dropped the inventory command: sent again'; $null = Send-FL 'tap USER_INVENTORY'; Await { (Send-FL 'nnpage') -match '^nnpage page=2 ' } 'the inventory page' 10 }
    $null = Wait-FLFramesPassed 10
    $g0 = Gear
    Rec 'jettison' 'open' 'gear' $g0
    # the ammunition filter (1)
    for ($i = 0; $i -lt 6 -and [regex]::Match((Send-FL 'nnpage'), ' filter=(\d+)').Groups[1].Value -ne '1'; $i++) {
        $f = [regex]::Match((Send-FL 'nnpage'), ' filter=(\d+)').Groups[1].Value
        if ($script:form -eq 'pad') { Press @('Right') } else {
            $tabs = (NNTabs)
            Click $tabs[1] 'the inventory''s ammunition tab'
        }
        Await { [regex]::Match((Send-FL 'nnpage'), ' filter=(\d+)').Groups[1].Value -ne $f } 'the next inventory filter' 5
    }
    $null = Wait-FLFramesPassed 20
    # (the game's JettisonButton stands once its own selection reached the row: after a tab's click that came 20 frames too late once at 3840x2160,
    # check run 20261112_210945; waited for, 5 s at most)
    $null = try { Wait-FLUntil { (Vis 'JettisonButton') -or (KV (Send-FL 'nnpage')).items -eq '0' } 'the game''s jettison button' 5 } catch { $null }
    $nn = KV (Send-FL 'nnpage')
    Rec 'jettison' 'ammo' 'items' $nn.items
    Rec 'jettison' 'ammo' 'button' "$(if (Vis 'JettisonButton') { 'shown' } else { 'not shown' })"
    if ($script:form -eq 'pad') { $l = Lane; Rec 'jettison' 'ammo' 'lane.items' $l.items }
    $null = Shot 'jettison_ammo'
    if (-not (Vis 'JettisonButton')) { Rec 'jettison' 'ammo' 'result' 'no jettison offered'; return }
    if ($script:form -eq 'pad') { Press @('Y') } else {
        $chips = @(LaneHots | Where-Object { $_.X0 -ge 0.42 * $script:W })
        Rec 'jettison' 'ammo' 'chips' $chips.Count
        Click $chips[$(if ($chips.Count -ge 2 -and ((PadPrompts) -match '^A (UN)?MOUNT$')) { 1 } else { 0 })] 'the JETTISON chip'
    }
    $null = Wait-FLUntil { Vis 'JettisonAcceptButton' } 'the jettison panel' 8
    $null = Wait-FLFramesPassed 20
    $p = [ordered]@{}
    if ($script:form -eq 'pad') { $l = Lane; $p['lane.items'] = $l.items; $p['lane.screen'] = $l.screen }
    $p['parked.JettisonAcceptButton'] = Parked 'JettisonAcceptButton'
    $p['hot.page'] = RectsText (PageHots)
    RecAll 'jettison' 'panel' $p
    $null = Shot 'jettison_panel'
    if ($script:form -eq 'pad') { Press @('A') } else {
        $row = @(PageHots | Group-Object { [int][Math]::Round($_.Y0 / 4) } | Where-Object Count -eq 5 | ForEach-Object { , @($_.Group | Sort-Object X0) } | Where-Object { ($_[3].X1 - $_[3].X0) -gt ($_[0].X1 - $_[0].X0) } | Select-Object -Last 1)
        Click $row[0][3] 'the panel''s JETTISON chip'
    }
    $null = Wait-FLUntil { -not (Vis 'JettisonAcceptButton') } 'the panel closed by its JETTISON' 8
    $null = Wait-FLFramesPassed 20
    # (the world stands paused under the page in space: the server takes the unit out once it runs again, after the close)
    $nn = KV (Send-FL 'nnpage')
    Rec 'jettison' 'accepted' 'nnpage' "page=$($nn.page) paused=$($nn.paused) filter=$($nn.filter) items=$($nn.items) itemSel=$($nn.itemSel)"
    $null = Shot 'jettison_accepted'
    if ($script:form -eq 'pad') { Press @('B') } else { $null = Send-FL 'nnstrip close' }
    $null = Wait-FLUntil { (Send-FL 'nnpage') -match '^nnpage page=0 ' } 'the inventory closed' 15
    $null = Wait-FLUntil { (Gear) -ne $g0 } 'the unit gone from the hold' 10
    Rec 'jettison' 'done' 'gear' (Gear)
}
# ---- the NeuroNet pages docked (70b W1 lists INVENTORY, STATUS and LOG beside the dealers; tests\nn_pages.ps1 asserts on their walks, this records
# their tables): each one's line (nnpage, its addresses and icon counters left out), lane, hotspots, a frame; the inventory's five filters; the press map
# (its addresses, icon counters and the count of blanked trade labels - tradeBlanked counts the frames it blanked the game's label - left out)
function NNLine { ((Send-FL 'nnpage') -replace ' root=0x[0-9a-f]+', '' -replace ' icon(Sends|Loads|CatchUps|Hidden)=\d+', '' -replace ' lent=\d+', '' -replace ' tradeBlanked=\d+', '').Trim() }
function NN-Read {
    $o = [ordered]@{}
    $o['nn.line'] = NNLine
    $l = Lane
    if ($script:form -eq 'pad') { foreach ($k in 'screen', 'items', 'count', 'dropped', 'visible', 'pending') { $o["lane.$k"] = $l[$k] } } else { $o['lane.visible'] = $l.visible }
    $o['hot.page'] = RectsText (PageHots)
    $o['hot.strip'] = RectsText @(Hots | Where-Object { $_.Y0 -lt 0.16 * $script:H })
    $o.texts = PageTexts
    $o
}
function NN-State([string]$page, [string]$state) {
    $a = NN-Read
    for ($i = 0; $i -lt 12; $i++) { $null = Wait-FLFramesPassed 30; $b = NN-Read; if ((Same $a $b) -and ($script:form -ne 'pad' -or $b['lane.pending'] -eq '0')) { break }; $a = $b }
    RecAll $page $state $b
    $null = Shot "${page}_$state"
}
function NN-Tab([int]$i, [string]$word) {
    $r = Send-FL "nnstrip tab $i"   # (the strip word's own action, what its hotspot and the bumpers call: nnstrip.cpp)
    Await { (Send-FL 'nnpage') -match "^nnpage page=$(@{ LOG = 1; INVENTORY = 2; STATUS = 3 }[$word]) " -and (Send-FL 'nnstrip') -match 'held=0x0' } "the $word page ($r)" 20
    $null = Wait-FLUntil { (((Send-FL 'hotspots') -split "`n")[-1]) -match 'ready$' } 'no NeuroNet window sliding' 15
}
function Walk-NN {
    Wait-FLRoomReady
    NN-Tab 2 'INVENTORY'
    # (its rows' hotspots come with the rows' first draw, a frame or more after the page: the first reading waits for them)
    $null = Wait-FLUntil { $n = KV (Send-FL 'nnpage'); [int]$n.items -eq 0 -or @(Hots | Where-Object { ($_.Y1 - $_.Y0) -ge 0.1 * $script:H -and $_.X0 -lt 0.1 * $script:W }).Count -gt 0 } 'the inventory''s rows drawn' 10
    $script:reached = ''
    Rec 'inventory' 'open' 'reached' "save $($script:save), docked, the strip's INVENTORY"
    Rec 'inventory' 'open' 'parked.NN_Inventory' (Parked 'NN_Inventory')
    NN-State 'inventory' 'open'
    for ($k = 0; $k -lt 5; $k++) {
        $f = [regex]::Match((Send-FL 'nnpage'), ' filter=(\d+)').Groups[1].Value
        if ($script:form -eq 'pad') { Press @('Right') } else {
            $tabs = (NNTabs)
            Click $tabs[(([int]$f) + 1) % 5] 'the next inventory tab'
        }
        Await { [regex]::Match((Send-FL 'nnpage'), ' filter=(\d+)').Groups[1].Value -ne $f } 'the next inventory filter' 5
        NN-State 'inventory' "filter$([regex]::Match((Send-FL 'nnpage'), ' filter=(\d+)').Groups[1].Value)"
    }
    if ($script:form -eq 'pad') { foreach ($b in 'down', 'up', 'left', 'right', 'x') { $r = Send-FL "nnpage press $b"; $null = Wait-FLFramesPassed 12; Rec 'inventory' 'end' "press.$b" "$r -> $(Steady { NNLine })" } }
    NN-Tab 3 'STATUS'
    $null = Wait-FLUntil { $n = KV (Send-FL 'nnpage'); [int]$n.reps -gt 0 } 'the factions' 10
    NN-State 'status' 'open'
    if ($script:form -eq 'pad') {
        foreach ($k in 1..4) { Press @('X'); $null = Wait-FLFramesPassed 6; NN-State 'status' "filter_step$k" }
        foreach ($b in 'down', 'up', 'right', 'left', 'y') { $r = Send-FL "nnpage press $b"; $null = Wait-FLFramesPassed 12; Rec 'status' 'end' "press.$b" "$r -> $(Steady { NNLine })" }
    }
    NN-Tab 0 'LOG'
    NN-State 'log' 'open'
    if ($script:form -eq 'pad') {
        foreach ($b in 'Left', 'Right') { Press @($b); $null = Wait-FLFramesPassed 10; NN-State 'log' "tab_$($b.ToLower())" }
        foreach ($b in 'down', 'up', 'left', 'right') { $r = Send-FL "nnpage press $b"; $null = Wait-FLFramesPassed 12; Rec 'log' 'end' "press.$b" "$r -> $(Steady { NNLine })" }
    }
    if ($script:form -eq 'pad') { Press @('B') } else { $null = Send-FL 'nnstrip close' }
    $null = Wait-FLUntil { (Send-FL 'nnpage') -match '^nnpage page=0 ' -and (Send-FL 'nnstrip') -match 'standing=0' } 'the NeuroNet closed' 15
    Wait-FLRoomReady
}

# ---- the same pages on the classic HUD (70b W1 (9): hud_style 0, the game's own windows): opened natively, the game's buttons and texts, the pad's
# lane on the classic HUD, a frame each; closed by the game's own close button
function Classic-Open([string]$page, [string]$room, [string]$spot, [string]$window, [string]$close) {
    $null = Send-FL 'set hud_style 0'
    try {
        Wait-FLRoomReady
        if ((Send-FL 'hotspots') -notmatch "(?m)^$spot\s") { $null = Goto-FLRoom $room; Wait-FLRoomReady }
        $null = Wait-FLUntil { $x = Send-FL "goto $spot"; if ($x -match '^clicked') { $x } } "the room's $spot (classic)" 15
        $null = Wait-FLUntil { Vis $window } "the game's $window" 15
        $null = Wait-FLFramesPassed 45   # (the window's slide in: the game's own animation, its controls placed after it)
        # the pad form selects the first ship a moment later, and the game shows its BUY then (pad: always in the runs of 13 Nov; the mouse form
        # selects nothing): waited for, 10 s at most, so the reading never races it (the 1280x800 baseline of 060148 read it before it came)
        if ($page -eq 'ships' -and $script:form -eq 'pad') { $null = try { Wait-FLUntil { Vis 'ShipTraderBuyButton' } 'the game''s BUY under the pad''s selection' 10 } catch { $null } }
        Classic-State $page 'open' $window
        if ($page -eq 'ships' -and (Vis 'ShipTraderBuyButton')) {
            $null = Send-FL 'click ShipTraderBuyButton'
            $came = try { Await { Vis 'ContinueButton' } 'the game''s final stage' 10; $true } catch { $false }
            Rec 'final' 'classic_open' 'reached' "$(if ($came) { 'by the game''s BUY button' } else { 'no: the game''s buy did not open it' })"
            if ($came) { $null = Wait-FLFramesPassed 45; Classic-State 'final' 'open' 'NN_Dealer'; $null = Send-FL 'click BackToShipDealer'; $null = Wait-FLUntil { Vis 'NN_ShipTrader' } 'the ships again' 10; $null = Wait-FLFramesPassed 45 }
        }
        $null = Send-FL "click $close"
        $null = Wait-FLUntil { -not (Vis $window) } "the game's $window closed" 15
        Wait-FLRoomReady
    } finally { $null = Send-FL 'set hud_style 2' }   # (back to HUD 2 always, the default: until 13 Nov 2026 this set 1, the mixed style the owner retired - decision 77 - so every walk after the first classic opening ran on it)
}
# (read until two readings 30 frames apart agree, as the HUD 2 states: the game fills its texts a moment after the window's selection)
function Classic-Read([string]$window) {
    $o = [ordered]@{}
    $o['classic.page'] = (BP).kind   # (no HUD 2 page stands on the classic HUD: none)
    $o['game.buttons'] = (@($script:buttons | Where-Object { Vis $_ }) -join ',')
    foreach ($t in 'TransferPriceText', 'QuantityTotalText', 'ErrorHelp', 'RepairTotalCost', 'ShipBuyProblem') { $o["game.$t"] = GameText $t }
    $o["parked.$window"] = Parked $window
    if ($script:form -eq 'pad') { $l = Lane; $o['lane.screen'] = $l.screen; $o['lane.items'] = $l.items; $o['lane.pending'] = $l.pending }
    $o
}
function Classic-State([string]$page, [string]$state, [string]$window) {
    $a = Classic-Read $window
    for ($i = 0; $i -lt 12; $i++) { $null = Wait-FLFramesPassed 60; $o = Classic-Read $window; if ((Same $a $o) -and ($script:form -ne 'pad' -or $o['lane.pending'] -eq '0')) { break }; $a = $o }
    RecAll $page "classic_$state" $o
    $null = Shot "${page}_classic_$state"
}

# The pad's prompts now (the form switched for the reading, as nn_pages' PadView)
function PadPrompts {
    $back = $script:form
    Set-Form 'pad'; $null = Wait-FLFramesPassed 30
    $it = @((Lane).items -split ', ')
    if ($back -ne 'pad') { Set-Form $back; $null = Wait-FLFramesPassed 10 }
    $it
}

# ---- the derived fixture: SaveC701 with rank 10 (the ship dealer sells) and the shield, thruster and both guns damaged (repair has rows)
function Write-DerivedSave {
    $src = Join-Path (Get-FLSaveDir) 'SaveC701.fl'
    $t = [IO.File]::ReadAllText($src)
    $t = [regex]::Replace($t, '(?m)^rank = \d+', 'rank = 10')
    $health = @{ 'HpShield01' = '0.5'; 'HpThruster01' = '0.4'; 'HpWeapon01' = '0.3'; 'HpWeapon02' = '0.6' }
    foreach ($hp in $health.Keys) { $t = [regex]::Replace($t, "(?m)^(equip = \d+, $hp, )1(?=\r?$)", "`${1}$($health[$hp])") }
    $out = Join-Path (Get-FLSaveDir) 'SaveC7BP.fl'
    [IO.File]::WriteAllText($out, $t, [Text.Encoding]::ASCII)
    $out
}

# ---- one walk per size and form
function Run-Size([string]$size) {
    $wh = $size -split 'x'; $script:W = [int]$wh[0]; $script:H = [int]$wh[1]; $script:size = $size
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
    Set-FLInstanceDisplay $Instance 'borderless' "$($script:W), $($script:H)"
    $null = Write-DerivedSave
    $sw = [Diagnostics.Stopwatch]::StartNew()
    & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save 'SaveC701.fl' | Select-Object -Last 1 | ForEach-Object { Say "  $_" }
    $script:save = 'SaveC701'
    $null = Wait-FLUntil { (Get-FLMode) -eq 'docked' } 'docked at Manhattan' 90
    $null = Send-FL 'fpscap 60'   # (test games at no more than 60 fps; the readings' windows count frames)
    $null = Send-FL 'set hud_style 2'   # (HUD 2 always, the default, whatever the main install's settings file says: no mixed style, decision 77)
    $arm = Send-FL 'page texts'   # (arms the pages' text record for this game: PageTexts)
    if ($arm -notmatch 'armed|^page texts n=') { throw "the build has no 'page texts' verb ($arm): this net needs a build with it (pagerun.cpp)" }
    Wait-FLRoomReady
    $render = [regex]::Match((Send-FL 'winmode'), 'render (\d+)x(\d+)')
    if ($render.Groups[1].Value -ne "$($script:W)" -or $render.Groups[2].Value -ne "$($script:H)") { throw "the game renders at $($render.Value), not $size" }
    Say ("  {0} ready in {1:n1} s" -f $size, $sw.Elapsed.TotalSeconds)
    # the board, the news and the bar first, in the game's first load: after a save load the board offers nothing new (HARNESS.md "Missions")
    $first = @('news', 'jobs', 'bar') | Where-Object { $_ -in $Pages }
    foreach ($form in $Forms) {
        $script:form = $form; Set-Form $form
        foreach ($p in $first) { Run-Page $p { switch ($p) { 'news' { Walk-Board 'news' } 'jobs' { Walk-Board 'jobs' } 'bar' { Walk-Bar } } } }
    }
    if ('accept' -in $Pages) {
        # one job a game: the pad form's at 3840x2160, the mouse form's at 1280x800
        $af = if ($size -eq '3840x2160') { 'pad' } else { 'kbm' }
        $script:accepted = $false
        # (the refused-job states follow in the same page record, in the accept's form first, then the other: the job stays active for the game)
        if ($af -in $Forms) { $script:form = $af; Set-Form $af; Run-Page 'accept' { Walk-Accept; if ($script:accepted) { foreach ($f in @($af) + @($Forms | Where-Object { $_ -ne $af })) { $script:form = $f; Set-Form $f; Walk-Refused }; $script:form = $af; Set-Form $af } } }
    }
    if ('prices' -in $Pages -and 'commodity' -notin $Pages) { Say '  (prices walks inside commodity: add -Pages commodity)' }
    # every other page from its own fresh load of its fixture (70b W1 (7): each action from the same save)
    $rest = @(@('nn', 'SaveC701.fl', 'docked'), @('commodity', 'SaveC701.fl', 'docked'), @('equipment', 'SaveC701.fl', 'docked'), @('repair', 'SaveC7BP.fl', 'docked'),
              @('ships', 'SaveC7BP.fl', 'docked'), @('jettison', 'SaveC706.fl', 'space'))
    foreach ($form in $Forms) {
        $script:form = $form
        foreach ($r in $rest) {
            if ($r[0] -notin $Pages) { continue }
            Run-Page $r[0] {
                Clear-FLPad
                Note "  $(Load-FLSave $r[1])"
                $script:save = @{ 'SaveC701.fl' = 'SaveC701'; 'SaveC7BP.fl' = 'SaveC7BP (SaveC701, rank 10, damaged)'; 'SaveC706.fl' = 'SaveC706' }[$r[1]]
                $null = Wait-FLUntil { (Get-FLMode) -eq $r[2] } "$($r[2]) after $($r[1])" 60
                if ($r[2] -eq 'docked') { Wait-FLRoomReady } else { $null = Wait-FLFramesPassed 30 }
                Set-Form $form
                switch ($r[0]) { 'nn' { Walk-NN } 'commodity' { Walk-Commodity } 'equipment' { Walk-Equipment } 'repair' { Walk-Repair } 'ships' { Walk-Ships } 'jettison' { Walk-Jettison } }
            }
        }
    }
}
# One page's walk: a failure is recorded as the page's ERROR record (so check sees a walk that no longer goes through) and the page is left
function Run-Page([string]$page, [scriptblock]$walk) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    try { & $walk; Rec $page 'walk' 'end' 'completed' }
    catch {
        $msg = "$($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))"
        Rec $page 'walk' 'end' "ERROR: $msg"
        $script:errors.Add("[$($script:size) $($script:form) $page] $msg"); Say "ERROR [$($script:size) $($script:form) $page] $msg"
        try { $null = Shot "${page}_error" } catch { }
        try {
            Clear-FLPad; $global:FLKeepPopups = $false
            foreach ($c in 'OfferDialogClose2Button', 'OfferDialogCloseButton', 'JettisonCancelButton') { if (Vis $c) { $null = Send-FL "click $c"; $null = Wait-FLFramesPassed 30 } }
            $null = Send-FL 'set hud_style 2'   # (a classic opening the error cut short: back to the default, HUD 2 always)
            for ($i = 0; $i -lt 3 -and (Kind) -notin 'none', 'unknown'; $i++) { $k = Kind; Press @('B'); $null = Wait-FLUntil { (Kind) -ne $k } 'the page left after the error' 6 }
            if ((Send-FL 'nnstrip') -match 'standing=1') { $null = Send-FL 'nnstrip close'; $null = Wait-FLFramesPassed 30 }
        } catch { }
    }
    Say ("  {0} {1} {2}: {3:n1} s" -f $script:size, $script:form, $page, $sw.Elapsed.TotalSeconds)
}

# ---- the comparison
function Read-Records([string]$file) {
    $h = [ordered]@{}
    foreach ($l in [IO.File]::ReadAllLines($file)) {
        if ($l -match '^#' -or -not $l.Trim()) { continue }
        $i = $l.IndexOf(' = '); if ($i -lt 0) { continue }
        $h[$l.Substring(0, $i)] = $l.Substring($i + 3)
    }
    $h
}
function Mask([string]$key) {
    $p = $key -split '\|', 3   # size, form, page|state|field
    $rest = ($p[2] -replace '#\d+$', '')
    foreach ($m in $Masks) { if ($rest -match $m.Key) { return $m.Why } }
    $null
}
# A record key the sizes, forms and pages of this run walk (the final stage is walked inside the ship dealer's walk, best prices inside the commodity trader's)
function InRun([string]$key) {
    $p = $key -split '\|'
    # (the NeuroNet pages are walked as -Pages nn and recorded under their own names: until 13 Nov 2026 they were never compared)
    $walked = switch ($p[2]) { 'final' { 'ships' -in $Pages } 'prices' { 'prices' -in $Pages -and 'commodity' -in $Pages } { $_ -in 'inventory', 'log', 'status' } { 'nn' -in $Pages } default { $p[2] -in $Pages } }
    $p[0] -in $Sizes -and $p[1] -in $Forms -and $walked
}
# -Mode slice: the baseline's fields of this run's sizes, forms and pages (InRun) replaced by $src's, every other line kept byte for byte (its order,
# its line ends, its encoding); the slice's lines stand where the first replaced line stood. A summary: states replaced, fields changed / added / dropped.
function Merge-Slice([string]$src) {
    $bytes = [IO.File]::ReadAllBytes($Baseline)
    $bom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
    $text = [Text.UTF8Encoding]::new($false).GetString($bytes, $(if ($bom) { 3 } else { 0 }), $bytes.Length - $(if ($bom) { 3 } else { 0 }))
    $nl = if ($text.Contains("`r`n")) { "`r`n" } else { "`n" }
    $old = $text -split [regex]::Escape($nl)   # (a final line end leaves one empty last item: kept, so the file ends as it did)
    $new = [Collections.Generic.List[string]]::new()
    foreach ($l in [IO.File]::ReadAllLines($src)) { if ($l -notmatch '^#' -and $l.IndexOf(' = ') -ge 0 -and (InRun $l.Substring(0, $l.IndexOf(' = ')))) { $new.Add($l) } }
    if (-not $new.Count) { throw "the slice holds no field of sizes $($Sizes -join ',') forms $($Forms -join ',') pages $($Pages -join ',') in $src" }
    $out = [Collections.Generic.List[string]]::new(); $was = [ordered]@{}; $placed = $false
    foreach ($l in $old) {
        $i = $l.IndexOf(' = ')
        if ($l -notmatch '^#' -and $i -ge 0 -and (InRun $l.Substring(0, $i))) {
            $was[$l.Substring(0, $i)] = $l.Substring($i + 3)
            if (-not $placed) { $out.AddRange($new); $placed = $true }
            continue
        }
        $out.Add($l)
    }
    if (-not $placed) {   # (no field of the slice in the baseline yet: before the final empty item, if the file ended with a line end)
        if ($out.Count -and $out[$out.Count - 1] -eq '') { $out.InsertRange($out.Count - 1, $new) } else { $out.AddRange($new) }
    }
    # (one comment line after the first header line names the slice; no ' = ' in it, so Read-Records skips it either way)
    $recBuild = [regex]::Match("$(Get-Content -LiteralPath $src -TotalCount 1)", '^# base_pages record, build (.+?), \d{4}-\d\d-\d\dT').Groups[1].Value   # (the record's own build: -From names an earlier run's)
    if (-not $recBuild) { $recBuild = "unknown (no record header in $src)" }
    $note = "# slice re-recorded $(Get-Date -Format s): sizes $($Sizes -join ','), forms $($Forms -join ','), pages $($Pages -join ','), build $recBuild, from $src"
    $at = 0; while ($at -lt $out.Count -and $out[$at] -match '^#') { $at++ }
    $out.Insert($at, $note)
    $now = [ordered]@{}; foreach ($l in $new) { $i = $l.IndexOf(' = '); $now[$l.Substring(0, $i)] = $l.Substring($i + 3) }
    $changed = @($now.Keys | Where-Object { $was.Contains($_) -and $was[$_] -cne $now[$_] })
    $added = @($now.Keys | Where-Object { -not $was.Contains($_) }); $dropped = @($was.Keys | Where-Object { -not $now.Contains($_) })
    $states = @(@($was.Keys) + @($now.Keys) | ForEach-Object { ($_ -split '\|')[0..3] -join '|' } | Sort-Object -Unique)
    $bdir = Split-Path $Baseline -Parent
    Copy-Item -LiteralPath $Baseline (Join-Path $bdir ('baseline_{0}_old.txt' -f $stamp))
    $body = [Text.UTF8Encoding]::new($false).GetBytes(($out -join $nl))
    [IO.File]::WriteAllBytes($Baseline, [byte[]]$(if ($bom) { [byte[]](0xEF, 0xBB, 0xBF) + $body } else { $body }))
    $sha = (Get-FileHash -LiteralPath $Baseline -Algorithm SHA256).Hash
    $sum = @("SLICE $($states.Count) states replaced ($($was.Count) fields before, $($now.Count) now): $($changed.Count) changed, $($added.Count) added, $($dropped.Count) dropped; every other line kept")
    foreach ($k in $changed) { $sum += "CHANGED $k`n    was '$($was[$k])'`n    now '$($now[$k])'" }
    foreach ($k in $added) { $sum += "ADDED $k = $($now[$k])" }
    foreach ($k in $dropped) { $sum += "DROPPED $k (was '$($was[$k])')" }
    $sum += "-> $Baseline (sha256 $sha; the old one: baseline_${stamp}_old.txt). Set `$BaselineSha256 in tests\base_pages.ps1 to it in the commit that names why."
    foreach ($l in $sum) { Say $l }
    [IO.File]::WriteAllLines($verdictFile, [string[]]$sum)
}

# ---- run
$initial = Get-FLDisplaySize (Get-FLInstallDir)
Add-Content -LiteralPath $recFile "# base_pages record, build $build, $(Get-Date -Format s), sizes $($Sizes -join ','), forms $($Forms -join ','), pages $($Pages -join ',')"
Say "base_pages $Mode on instance $Instance, build: $build"
if ($Mode -in 'check', 'slice') {   # (a slice goes only into the baseline this script knows)
    if (-not (Test-Path -LiteralPath $Baseline)) { Say "NO BASELINE: $Baseline (record one with -Mode record)"; exit 2 }
    $have = (Get-FileHash -LiteralPath $Baseline -Algorithm SHA256).Hash
    if ($BaselineSha256 -and $isShared -and [IO.Path]::GetFileName($Baseline) -ieq 'baseline.txt' -and $have -ne $BaselineSha256) { Say "BASELINE CHANGED: $Baseline has sha256 $have, the script records $BaselineSha256"; exit 2 }
    Say "baseline: $Baseline (sha256 $have)"
}
if ($Mode -eq 'slice' -and $From) {   # (an earlier run's records: no game)
    if (-not (Test-Path -LiteralPath $From)) { Say "NO RECORD: $From"; exit 2 }
    Merge-Slice $From
    exit 0
}
try {
    foreach ($size in $Sizes) { Run-Size $size }
} catch { $script:errors.Add("[$($script:size)] the walk stopped: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))"); Say "ERROR $($script:errors[-1])" }
finally {
    try { if (Get-FLGameProcess) { Clear-FLPad; $null = Send-FL 'set input_mode 0' } } catch { }
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
    Get-CimInstance Win32_Process -Filter "Name='pwsh.exe'" | Where-Object { $_.CommandLine -match 'flshots\.ps1' -and $_.CommandLine -match "-Instance $([regex]::Escape($Instance))(\s|$)" } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    $derived = Join-Path (Get-FLSaveDir) 'SaveC7BP.fl'
    if (Test-Path -LiteralPath $derived) { [IO.File]::Delete($derived) }
    if ($initial) { Set-FLInstanceDisplay $Instance 'borderless' $initial }
}
# a game of this run that crashed left a block in the crash recorder's log: a walk error quoting it (a record is then not kept)
foreach ($c in @(Get-FLCrashBlocks $crashSince)) { $script:errors.Add("a game crashed during the run (EXE\FLvibed_crash.log):`n$c"); Say "ERROR $($script:errors[-1])" }
$secs = [int]$total.Elapsed.TotalSeconds
$count = $script:recKeys.Count
if ($Mode -in 'record', 'slice' -and $script:errors.Count) {
    # (a walk that did not go through is no baseline: the file stays as it was)
    Say ''
    Say "NOT RECORDED: $($script:errors.Count) walk errors ($count fields, $secs s); $Baseline left as it was. This run's records: $recFile"
    foreach ($e in $script:errors) { Say "  ERROR $e" }
    Add-Content -LiteralPath $verdictFile "NOT RECORDED: $($script:errors.Count) walk errors"
    exit 1
}
if ($Mode -eq 'slice') { Merge-Slice $recFile; exit 0 }
if ($Mode -eq 'record') {
    $bdir = Split-Path $Baseline -Parent
    New-Item -ItemType Directory -Force $bdir | Out-Null
    if (Test-Path -LiteralPath $Baseline) { Copy-Item -LiteralPath $Baseline (Join-Path $bdir ('baseline_{0}_old.txt' -f $stamp)) }
    Copy-Item -LiteralPath $recFile $Baseline -Force
    $sha = (Get-FileHash -LiteralPath $Baseline -Algorithm SHA256).Hash
    $states = @($script:recKeys.Keys | ForEach-Object { ($_ -split '\|')[0..3] -join '|' } | Sort-Object -Unique).Count
    Say ''
    Say "RECORDED $count fields in $states states, $($script:errors.Count) walk errors, $($script:shot) frames, $secs s -> $Baseline (sha256 $sha)"
    foreach ($e in $script:errors) { Say "  ERROR $e" }
    Add-Content -LiteralPath $verdictFile "RECORDED $count fields, $states states, $($script:errors.Count) walk errors, $secs s, sha256 $sha"
    if ($script:errors.Count) { exit 1 }
    exit 0
}
# check: every baseline field of the sizes, forms and pages this run walked, against this run's
$base = Read-Records $Baseline
$now = Read-Records $recFile
$fails = [Collections.Generic.List[string]]::new(); $masked = [Collections.Generic.List[string]]::new(); $same = 0
foreach ($k in $base.Keys) {
    if (-not (InRun $k)) { continue }
    $p = $k -split '\|'
    $why = Mask $k
    if ($why) { $masked.Add("MASKED $k ($why)"); continue }
    if (-not $now.Contains($k)) { $fails.Add("FAIL $($p[0]) $($p[1]) page=$($p[2]) state=$($p[3]) field=$($p[4]): missing now (baseline '$($base[$k])')"); continue }
    if ($now[$k] -cne $base[$k]) { $fails.Add("FAIL $($p[0]) $($p[1]) page=$($p[2]) state=$($p[3]) field=$($p[4]): baseline '$($base[$k])' now '$($now[$k])'") } else { $same++ }
}
$newStates = [Collections.Generic.List[string]]::new()
foreach ($k in $now.Keys) {
    if ($base.Contains($k) -or (Mask $k)) { continue }
    $p = $k -split '\|'
    # a state the walk has gained and the baseline does not hold yet (named in $PendingStates until a slice record on main takes it in): reported
    # NEW, not a FAIL; a new field in a state the baseline has, or any other new state, still fails
    if ($p[3] -in $PendingStates -and -not @($base.Keys | Where-Object { $_.StartsWith("$($p[0])|$($p[1])|$($p[2])|$($p[3])|") }).Count) { $newStates.Add("NEW $($p[0]) $($p[1]) page=$($p[2]) state=$($p[3]) field=$($p[4]): '$($now[$k])' (state not in the baseline yet)"); continue }
    $fails.Add("FAIL $($p[0]) $($p[1]) page=$($p[2]) state=$($p[3]) field=$($p[4]): new now ('$($now[$k])'), not in the baseline")
}
$lines = @($fails) + @($newStates) + @($masked | Sort-Object -Unique)
[IO.File]::WriteAllLines($verdictFile, [string[]]$lines)
Say ''
foreach ($f in $fails | Select-Object -First 60) { Say $f }
if ($fails.Count -gt 60) { Say "... $($fails.Count - 60) more in $verdictFile" }
if ($newStates.Count) {
    Say "NEW (not a FAIL): $($newStates.Count) fields in states the baseline does not hold yet: $((@($newStates | ForEach-Object { $q = $_ -split ' '; "$($q[1]) $($q[2]) $($q[3]) $($q[4])" } | Sort-Object -Unique)) -join '; ')"
    foreach ($n in $newStates | Select-Object -First 60) { Say $n }
}
Say ("base pages check: {0} fields equal, {1} FAIL, {2} NEW, {3} masked, {4} walk errors, {5} frames, {6} s - {7}" -f $same, $fails.Count, $newStates.Count, $masked.Count, $script:errors.Count, $script:shot, $secs, $dir)
Add-Content -LiteralPath $verdictFile ("SUMMARY {0} equal, {1} FAIL, {2} NEW, {3} masked, {4} walk errors, {5} s" -f $same, $fails.Count, $newStates.Count, $masked.Count, $script:errors.Count, $secs)
if ($fails.Count -or $script:errors.Count) { Say 'BASE PAGES: FAIL'; exit 1 }
Say 'BASE PAGES: PASS'
exit 0
