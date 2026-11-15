# The NeuroNet INFO page's text against the game's own (modules\hud\src\nnpage.cpp Page::Info, infobody, inforules): under HUD 2 the page composes the
# game's INFO text itself and draws it, the game's NN_Help window parked off the screen after its open slide. `nnpage info` prints what HUD 2 would
# compose now ([HUD2]), the game's window's text read from its list while it is open ([GAME]) and what the page shows ([PAGE] case lines first room).
# Situations (SaveC706 in space, SaveC701 docked at Manhattan):
#   S1          space, no target: [PAGE] == [GAME] (the window built at open is the ground truth) and == [HUD2]
#   S2-<what>   space, each target selected BEFORE opening: Planet Manhattan, the unknown New York jump gate, Fort Bush, Baltimore Shipyard, a trade
#               lane ring, the sun, the nearest NPC ship, the Missouri, West Point, the Manhattan Docking Ring, the player's waypoint: [PAGE] == [GAME]
#   S-docked    docked at Manhattan: [PAGE] == [GAME]
#   S-refresh   open with no target, Fort Bush selected while open: [PAGE] becomes the Fort Bush card (== a fresh [HUD2]); the target cleared while
#               open: [PAGE] stays the Fort Bush card (the classic HUD's window's rule; under HUD 2 the parked window does not refresh: [GAME] not compared)
#   S-park-*    after the open slide NN_Help and every control under it stand right of the screen (ctlrect left > width), parked=1 (space and docked)
#   S-scroll    docked (the long market text): when lines > room, `nnpage press down` raises first by 1, `up` lowers it, first stays in 0..lines-room
#   LINES       with every [PAGE] comparison: the page's laid-out lines ([LINES]) carry each paragraph's words in order, the market's items at
#               one indent with their prices right-aligned in one column (HUD 2's grid, the owner, 12 Nov 2026): Check-Lines
#   STYLES      (the owner, 12 Nov 2026, audit decision 56: the card's italic, underline and colour drawn) every block prints each styled
#               paragraph's runs after it ("~ <characters>:<TRA attributes>[h]", inforules::StyleLine), so [PAGE] == [GAME] compares the
#               styles too; and each [LINES] piece names the attributes it is drawn with: every visible character of a paragraph drawn
#               with its run's attributes (Check-Lines). The styled cards: S2-ManhattanDockingRing (a red bold warning); S2-Missouri and
#               S2-WestPoint are UNKNOWN STATION in SaveC706 (never near), so S2-Missouri-found puts the ship beside the Missouri
#               (Move-FLShipNear), lets the game find it and compares its card (red CLASSIFIED required; its italic second card needs a
#               dock there, visit bit 0x02); S1-Badlands puts the ship inside the Badlands nebula, no target, and compares the zone's
#               card (65996: red bold warning, the italic "Missouri": italic and colour required); the PASS lines say what each covered.
#   S-underline in space, each size, the first form: New York's system card 66106 put on the page by the test verb `nnpage info card`
#               (underline never reaches the game's INFO): its underlined runs drawn with their attributes, a frame, the next open normal
#   frames      <size>_<form>_<case>.jpg for the owner: space_notarget, space_fortbush, space_missouri (red: the ship beside the
#               Missouri, found), space_badlands (red bold, italic: no target in the nebula), docked_manhattan, each size and form;
#               card66106_underline each size, the first form
# The comparisons run at the first size (the text does not depend on it); the frames at every size. Lines are compared with their trailing blanks
# trimmed and trailing empty lines dropped (invisible on the screen; the channel may trim the reply's end).
# Run folder: research\experience\runs\info_compare\<time> (steps.txt every verb sent and its reply, verdict.txt, the frames).
# Usage (PowerShell tool): tests\info_compare.ps1 [-Instance 5] [-Sizes 3840x2160,1280x800] [-Forms pad,kbm] [-Places space,docked]   build\release, no build.
# Exit 0 = every reachable situation passed, 1 = a FAIL or MISSING (or the run could not go on).
param([string]$Instance = '', [string[]]$Sizes = @('3840x2160', '1280x800'), [ValidateSet('pad', 'kbm')][string[]]$Forms = @('pad', 'kbm'),
      [ValidateSet('docked', 'space')][string[]]$Places = @('space', 'docked'), [string]$DockedSave = 'SaveC701.fl', [string]$SpaceSave = 'SaveC706.fl')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $Instance) { throw 'name the game instance: -Instance <n> (no default: a default instance belongs to no checkout - docs\TESTING.md "Parallel lanes")' }
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
Add-Type -AssemblyName System.Drawing
$dir = Join-Path $root ("research\experience\runs\info_compare\{0:yyyyMMdd_HHmmss}" -f (Get-Date))
New-Item -ItemType Directory -Force $dir | Out-Null
$steps = Join-Path $dir 'steps.txt'; $verdict = Join-Path $dir 'verdict.txt'
$script:res = [Collections.Generic.List[object]]::new(); $script:W = 0; $script:H = 0; $script:ctx = ''; $script:shots = 0
$total = [Diagnostics.Stopwatch]::StartNew()
$crashSince = Get-Date   # (a crash recorder block written after this is the run's: Get-FLCrashBlocks at the end)
# (nothing here writes to the pipeline by accident: a function's output is its return value)
function Say([string]$line) { Write-Host $line; Add-Content -LiteralPath $verdict $line }
function Note([string]$line) { Add-Content -LiteralPath $steps ("---- [{0:HH:mm:ss}] [{1}] {2}" -f (Get-Date), $script:ctx, $line) }
# Every verb this script sends, with its reply, in steps.txt
function V([string]$cmd) {
    $r = Send-FL $cmd
    Add-Content -LiteralPath $steps ("===== [{0:HH:mm:ss}] [{1}] > {2}`n{3}`n" -f (Get-Date), $script:ctx, $cmd, $r)
    $r
}
function Result([string]$name, [string]$status, [string[]]$lines = @()) {
    $script:res.Add([pscustomobject]@{ Name = $name; Status = $status; Lines = @($lines) })
    Say "$status $name"
    foreach ($l in $lines) { Say "    $l" }
}
function Until([scriptblock]$cond, [string]$what, [double]$sec = 5) { try { $null = Wait-FLUntil $cond $what $sec; $true } catch { if ($_.Exception.Message -match 'not reached') { $false } else { throw } } }
function KV([string]$s) { $h = @{}; foreach ($m in [regex]::Matches($s, '(\w+)=(\S*)')) { if (-not $h.ContainsKey($m.Groups[1].Value)) { $h[$m.Groups[1].Value] = $m.Groups[2].Value } }; $h }
function NN { KV (Send-FL 'nnpage') }
function Strip { KV (Send-FL 'nnstrip') }

# ---- `nnpage info` parsed: Head (the first line's fields), HUD2 / GAME / PAGE (the blocks' lines, $null when absent), PageHead (the [PAGE] line's fields)
function Info([switch]$Quiet) {
    $raw = if ($Quiet) { Send-FL 'nnpage info' } else { V 'nnpage info' }
    $o = @{ Raw = $raw; Head = @{}; HUD2 = $null; GAME = $null; PAGE = $null; PageHead = @{} }
    $cur = $null; $first = $true
    foreach ($l in ("$raw" -split "`n")) {
        $l = $l.TrimEnd("`r")
        if ($first) { $o.Head = KV $l; $first = $false; continue }
        if ($l -eq '[HUD2]') { $cur = 'HUD2'; $o.HUD2 = [Collections.Generic.List[string]]::new(); continue }
        if ($l -eq '[GAME]') { $cur = 'GAME'; $o.GAME = [Collections.Generic.List[string]]::new(); continue }
        if ($l -match '^\[PAGE\]') { $cur = 'PAGE'; $o.PAGE = [Collections.Generic.List[string]]::new(); $o.PageHead = KV $l; continue }
        if ($l -match '^\[LINES\]') { $cur = 'LINES'; $o.LINES = [Collections.Generic.List[string]]::new(); $o.LinesWrap = [double](KV $l).wrap; continue }
        if ($cur) { $o[$cur].Add($l) }
    }
    foreach ($k in 'HUD2', 'GAME', 'PAGE') { if ($null -ne $o[$k]) { $o[$k] = Norm $o[$k] } }
    $o
}
# A block's paragraphs: the "|" lines' text, each with its style line's runs ("~ <n>:<attrs>[h] ...": $null = all plain)
function Paras($block) {
    $out = [Collections.Generic.List[object]]::new()
    foreach ($l in @($block | Select-Object -Skip 1)) {
        if ($l -like '|*') { $out.Add([pscustomobject]@{ Text = $l.Substring(1); Runs = $null }) }
        elseif ($l -like '~*' -and $out.Count) { $out[$out.Count - 1].Runs = @([regex]::Matches($l, ' (\d+):([0-9A-F]{8}h?)') | ForEach-Object { [pscustomobject]@{ N = [int]$_.Groups[1].Value; A = $_.Groups[2].Value } }) }
    }
    , $out.ToArray()
}
# The attributes each visible (non-space) character is drawn with, in order: from a paragraph's runs, or from a line's pieces
function Styles-OfPara($p) {
    $s = [Collections.Generic.List[string]]::new()
    if ($null -eq $p.Runs) { foreach ($c in $p.Text.ToCharArray()) { if ($c -ne ' ') { $s.Add('00000000') } }; return , $s.ToArray() }
    $at = 0
    foreach ($r in $p.Runs) {
        if ($at -ge $p.Text.Length) { break } # (the line's trailing blanks are trimmed: nothing visible there)
        foreach ($c in $p.Text.Substring($at, [Math]::Min($r.N, $p.Text.Length - $at)).ToCharArray()) { if ($c -ne ' ') { $s.Add($r.A) } }
        $at += $r.N
    }
    , $s.ToArray()
}
# The page's laid-out lines ([LINES]: paragraph, market item, then left / right / attributes / text per piece) against its paragraphs
# ([PAGE]), since HUD 2 lays the market out in its own grid (the owner, 12 Nov 2026: names aligned, prices in a right-hand column): every
# paragraph drawn, in order, with exactly its words, each visible character with its run's attributes (decision 56: italic, underline,
# colour); every market item's name at one indent; every price right-aligned in one column, clear of its name and inside the column's
# width; every indented priced line of the game's text drawn as such an item. The problems, none = good.
function Check-Lines($info) {
    $bad = [Collections.Generic.List[string]]::new()
    if ($null -eq $info.LINES -or $null -eq $info.PAGE) { $bad.Add('no [LINES] / [PAGE] block'); return , $bad.ToArray() }
    $wrap = [double]$info.LinesWrap; $rows = @()
    foreach ($l in $info.LINES) {
        if ($l -match '^\s*$') { continue }
        if ($l -notmatch '^-?\d+\t[01](\t|$)') { $bad.Add("an unreadable [LINES] line: '$l'"); continue }
        $f = $l -split "`t"
        $pieces = @(for ($i = 2; $i + 3 -lt $f.Count; $i += 4) { [pscustomobject]@{ L = [double]$f[$i]; R = [double]$f[$i + 1]; A = $f[$i + 2]; T = $f[$i + 3] } })
        $rows += [pscustomobject]@{ Para = [int]$f[0]; Item = $f[1] -eq '1'; Pieces = $pieces }
    }
    $pp = Paras $info.PAGE
    $paras = @($pp | ForEach-Object { $_.Text })
    $words = { param($s) ("$s" -replace '\s+', ' ').Trim() }
    $last = -1
    foreach ($r in $rows) { if ($r.Para -lt $last) { $bad.Add("line order: paragraph $($r.Para) after $last") }; $last = $r.Para }
    for ($p = 0; $p -lt $paras.Count; $p++) {
        $mine = @($rows | Where-Object Para -eq $p)
        if (-not $mine) { $bad.Add("paragraph $p not drawn: '$($paras[$p])'"); continue }
        # (a line's pieces read as one where they touch - a superscript after its digits - and apart where a gap stands - a price after its name)
        $drawn = & $words (($mine | ForEach-Object { $s = ''; $prev = $null; foreach ($pc in $_.Pieces) { $s += $(if ($prev -and $pc.L -gt $prev.R + 0.5) { ' ' } else { '' }) + $pc.T; $prev = $pc }; $s }) -join ' ')
        if ($drawn -cne (& $words $paras[$p])) { $bad.Add("paragraph $p drawn as '$drawn', is '$(& $words $paras[$p])'") }
        # the styles: each visible character drawn with its run's attributes (only where the words already agree)
        else {
            $want = Styles-OfPara $pp[$p]
            $got = @(foreach ($r in $mine) { foreach ($pc in $r.Pieces) { foreach ($c in $pc.T.ToCharArray()) { if ($c -ne ' ') { $pc.A } } } })
            for ($k = 0; $k -lt [Math]::Max($want.Count, $got.Count); $k++) {
                if ($k -ge $want.Count -or $k -ge $got.Count -or $want[$k] -cne $got[$k]) {
                    $bad.Add("paragraph $p's styles: visible character $k drawn as $(if ($k -lt $got.Count) { $got[$k] } else { 'nothing' }), is $(if ($k -lt $want.Count) { $want[$k] } else { 'nothing' })"); break
                }
            }
        }
        if ($paras[$p] -match '^ {4,}\S.* \$[\d,]+\s*$' -and -not ($mine | Where-Object { $_.Item -and ($_.Pieces | Where-Object { $_.T -like '$*' }) })) { $bad.Add("paragraph $p, a priced item, not in the grid: '$($paras[$p])'") }
    }
    $items = @($rows | Where-Object Item)
    $indents = @($items | ForEach-Object { $_.Pieces[0].L } | Sort-Object -Unique)
    if ($indents.Count -gt 1) { $bad.Add("the items' names at $($indents.Count) indents: $($indents -join ', ')") }
    $prices = @($items | ForEach-Object { $_.Pieces | Select-Object -Skip 1 | Where-Object { $_.T -like '$*' } })
    $edges = @($prices | ForEach-Object { [Math]::Round($_.R, 0) } | Sort-Object -Unique)
    if ($edges.Count -gt 1 -and ($edges[-1] - $edges[0]) -gt 1) { $bad.Add("the prices end at $($edges.Count) edges: $($edges -join ', ')") }
    if ($prices | Where-Object { $_.R -gt $wrap + 0.5 }) { $bad.Add("a price past the column's width $wrap") }
    foreach ($r in $items) { if ($r.Pieces.Count -ge 2 -and $r.Pieces[1].L -lt $r.Pieces[0].R) { $bad.Add("a price over its name: '$($r.Pieces[0].T)' / '$($r.Pieces[1].T)'") } }
    , $bad.ToArray()
}
function Norm($lines) {
    $a = @($lines | ForEach-Object { $_.TrimEnd() })
    $n = $a.Count; while ($n -gt 0 -and $a[$n - 1] -eq '') { $n-- }
    , @(if ($n -gt 0) { $a[0..($n - 1)] } else { @() })
}
# The lines that differ, as "L<n> <A>: '...' | <B>: '...'"
function Diff-Lines($a, [string]$an, $b, [string]$bn) { # (not "Diff": PowerShell's alias of Compare-Object wins)
    $out = [Collections.Generic.List[string]]::new()
    $n = [Math]::Max($a.Count, $b.Count)
    for ($i = 0; $i -lt $n; $i++) {
        $x = if ($i -lt $a.Count) { $a[$i] } else { '<none>' }; $y = if ($i -lt $b.Count) { $b[$i] } else { '<none>' }
        if ($x -cne $y) { $out.Add(("L{0} {1}: '{2}' | {3}: '{4}'" -f ($i + 1), $an, $x, $bn, $y)) }
    }
    , $out.ToArray()
}
# PASS / FAIL / MISSING for a list of pairs to be equal: @(@('PAGE','GAME'), ...)
function Compare-Blocks([string]$name, $info, $pairs) {
    $missing = @(foreach ($p in $pairs) { foreach ($k in $p) { if ($null -eq $info[$k]) { $k } } }) | Select-Object -Unique
    if ($missing) { Result $name 'MISSING' @("no [$($missing -join '], [')] block (window=$($info.Head.window) page case=$($info.PageHead.case))"); return }
    $lines = @(foreach ($p in $pairs) { $d = Diff-Lines $info[$p[0]] $p[0] $info[$p[1]] $p[1]; foreach ($x in $d) { "$($p[0]) vs $($p[1]): $x" } })
    # what the page shows is also what it draws: its laid-out lines against its paragraphs (Check-Lines)
    if (@($pairs | ForEach-Object { $_ }) -contains 'PAGE') { $problems = Check-Lines $info; $lines += @(foreach ($x in $problems) { "LINES: $x" }) }
    # (what the styles covered: styled paragraphs, and the italic / underlined / coloured characters among them)
    $ps = Paras $info[$pairs[0][0]] # (assigned first: Paras returns its array whole, one pipeline object)
    $runs = @($ps | Where-Object { $_.Runs } | ForEach-Object { $_.Runs })
    $cover = "styled paragraphs $(@($ps | Where-Object { $_.Runs }).Count): italic $(($runs | Where-Object { [Convert]::ToUInt32($_.A.Substring(0, 8), 16) -band 2 } | Measure-Object N -Sum).Sum + 0)" +
             " underline $(($runs | Where-Object { [Convert]::ToUInt32($_.A.Substring(0, 8), 16) -band 4 } | Measure-Object N -Sum).Sum + 0) coloured $(($runs | Where-Object { [Convert]::ToUInt32($_.A.Substring(0, 8), 16) -shr 8 } | Measure-Object N -Sum).Sum + 0) characters"
    if ($lines.Count) { Result $name 'FAIL' $lines } else { Result $name 'PASS' @("$($info[$pairs[0][0]].Count) lines equal; title '$(($info[$pairs[0][0]] | Select-Object -First 1))' (case=$($info.Head.case) target=$($info.Head.target) window=$($info.Head.window) parked=$($info.Head.parked)); $cover") }
}

# ---- the INFO window opened and closed the game's way (USER_HELP / the strip's close), waited on by state
function Room-Ready { if ((Get-FLMode) -eq 'docked') { $null = Wait-FLUntil { (((Send-FL 'hotspots') -split "`n")[-1]) -match 'ready$' } 'the room ready (no window sliding)' 15 } }
function Open-Info {
    Room-Ready
    $null = V 'tap USER_HELP'
    # (the game drops a window command while the last close's slide runs: no window after the wait = sent once more)
    if (-not (Until { (Strip).open -eq '0x6a' } 'the INFO window' 4)) { Note 'the game dropped USER_HELP (a slide): sent again'; $null = V 'tap USER_HELP' }
    $null = Wait-FLUntil { $s = Strip; $s.open -eq '0x6a' -and $s.held -eq '0x0' -and $s.standing -eq '1' -and (NN).page -eq '4' } 'INFO up' 20
    # the open slide ends, then the page parks the window: parked=1 and the window's text readable
    $ok = Until { $i = Info -Quiet; $i.Head.parked -eq '1' -and $i.Head.window -eq '1' -and $null -ne $i.PAGE } 'INFO parked, its text in' 10
    if (-not $ok) { Note 'INFO: parked=1 / window=1 / [PAGE] not all seen within 10 s' }
    $null = Wait-FLFramesPassed 4
}
function Close-Info {
    if ((Strip).standing -ne '1' -and (NN).page -eq '0') { return }
    $null = V 'nnstrip close'
    $null = Wait-FLUntil { $s = Strip; $s.standing -eq '0' -and $s.open -eq '0x0' -and (NN).page -eq '0' } 'the NeuroNet closed' 15
    Room-Ready
}
function Clear-Target {
    if ((Send-FL 'targetinfo') -match '^no target') { return }
    $null = V 'tap USER_CLEAR_TARGET'
    $null = Wait-FLUntil { (Send-FL 'targetinfo') -match '^no target' } 'the target cleared' 5
}
# A target by its object id (`settarget`, the game's own target action): $true when the game took it
function Select-Target([uint32]$id) {
    $r = V "settarget $id"
    if ($r -notmatch '^target set') { return $false }
    Until { (Send-FL 'targetinfo') -match "id=$id " } "target $id" 5
}

# ---- the parked window: NN_Help and every control under it (dumpui's tree) right of the screen
function Check-Park([string]$name) {
    $i = Info
    $help = V 'ctlrect NN_Help'
    $bad = [Collections.Generic.List[string]]::new(); $seen = [Collections.Generic.List[string]]::new()
    $m = [regex]::Match($help, 'ctl=([0-9A-F]+) root=([0-9A-F]+).* left=(-?[\d.]+)')
    if (-not $m.Success) { Result $name 'FAIL' @("no rectangle for NN_Help: $help"); return }
    $helpRoot = $m.Groups[2].Value
    if ([double]$m.Groups[3].Value -le $script:W) { $bad.Add("NN_Help left=$($m.Groups[3].Value) <= $($script:W)") }
    $seen.Add("NN_Help left=$($m.Groups[3].Value)")
    # the controls under NN_Help by name (the game's tree as dumpui writes it), each read where it stands now
    $names = [Collections.Generic.List[string]]::new()
    $path = (V 'dumpui').Trim()
    if (Test-Path -LiteralPath $path) {
        $tree = Get-Content -LiteralPath $path
        $depth = -1
        foreach ($l in $tree) {
            $mm = [regex]::Match($l, '^( *)(\S+)\s+ctl=')
            if (-not $mm.Success) { if ($l -match '^== root') { $depth = -1 }; continue }
            $d = $mm.Groups[1].Value.Length
            if ($depth -lt 0) { if ($mm.Groups[2].Value -eq 'NN_Help') { $depth = $d } ; continue }
            if ($d -le $depth) { $depth = -1; if ($mm.Groups[2].Value -eq 'NN_Help') { $depth = $d }; continue }
            if ($names -notcontains $mm.Groups[2].Value) { $names.Add($mm.Groups[2].Value) }
        }
        Copy-Item -LiteralPath $path (Join-Path $dir ("dumpui_{0}.txt" -f ($name -replace '[^\w]+', '_'))) -Force
    } else { $bad.Add("dumpui gave no file ($path)") }
    if ($names -notcontains 'PlayerInfoText') { $names.Insert(0, 'PlayerInfoText') }
    foreach ($n in $names) {
        $r = V "ctlrect $n"
        $c = [regex]::Match($r, 'root=([0-9A-F]+).* left=(-?[\d.]+) top=(-?[\d.]+) right=(-?[\d.]+)')
        if (-not $c.Success) { if ($n -eq 'PlayerInfoText') { $bad.Add("PlayerInfoText: $r") }; continue } # (a hidden child has no visible control)
        if ($c.Groups[1].Value -ne $helpRoot) { continue } # (the same name in another window)
        $seen.Add("$n left=$($c.Groups[2].Value)")
        if ([double]$c.Groups[2].Value -le $script:W) { $bad.Add("$n left=$($c.Groups[2].Value) right=$($c.Groups[4].Value) <= width $($script:W)") }
    }
    if ($i.Head.parked -ne '1') { $bad.Add("parked=$($i.Head.parked)") }
    if ($bad.Count) { Result $name 'FAIL' (@($bad) + "read: $($seen -join ', ')") }
    else { Result $name 'PASS' @("parked=1; $($seen.Count) controls right of x=$($script:W): $($seen -join ', ')") }
}

# ---- frames for the owner: <size>_<form>_<case>.jpg
function Shot([string]$name) {
    $script:shots++
    Note "frame $name"
    $null = V 'nnpage'; $null = V 'nnstrip'
    $png = Save-FLShot -Name $name -Dir $dir
    $img = [Drawing.Image]::FromFile($png)
    try {
        $enc = [Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -eq 'image/jpeg'
        $ps = New-Object Drawing.Imaging.EncoderParameters 1
        $ps.Param[0] = New-Object Drawing.Imaging.EncoderParameter ([Drawing.Imaging.Encoder]::Quality), 90L
        $img.Save((Join-Path $dir "$name.jpg"), $enc, $ps)
    } finally { $img.Dispose() }
    [IO.File]::Delete($png)
}
function Set-Form([string]$form) {
    $null = V "set input_mode $(if ($form -eq 'pad') { 1 } else { 2 })"
    $want = if ($form -eq 'pad') { 'device=pad' } else { 'device=kbm' }
    $null = Wait-FLUntil { (Send-FL 'device') -match "^$want " } "input form $form" 10
    $null = Wait-FLFramesPassed 4
}
# The Missouri found: SaveC706 has never been near it (its card is UNKNOWN STATION there), so the ship is put beside it
# (Move-FLShipNear: the harness's `pos`) and the game finds it itself, as flying close does; $true once its visit byte is set.
function Near-Missouri {
    try { $null = Move-FLShipNear 'Li01_03' 1500 } catch { Note "Near-Missouri: $($_.Exception.Message)"; return $false }
    $null = Select-Target $kMissouri
    Until { $v = (Info -Quiet).Head.visit; $v -and $v -ne '-1' -and $v -ne '0' } 'the Missouri found (its visit byte set)' 20
}
# S2-Missouri-found: its card as the game shows it once found (65747 red CLASSIFIED, 65748 its italic name): PAGE == GAME with styles
function Missouri-Found {
    $name = 'S2-Missouri-found'
    try {
        Clear-Target
        if (-not (Near-Missouri)) { Result $name 'FAIL' @('the ship was not put near the Missouri, or the game did not find it within 20 s'); return }
        Open-Info
        $i = Info
        Compare-Blocks "$name (target $($i.Head.target), visit $($i.Head.visit)): PAGE == GAME" $i @(, @('PAGE', 'GAME'))
        $last = $script:res[$script:res.Count - 1]
        # (found = visit 1: the card's first page, its red CLASSIFIED; the italic second card needs visit bit 0x02, a dock there)
        if ($last.Status -eq 'PASS' -and ($last.Lines -join ' ') -notmatch 'coloured [1-9]') {
            $last.Status = 'FAIL'; $last.Lines += 'the found card carries no colour: the styles not exercised'; Say '    -> FAIL: the found card carries no colour'
        }
    } catch { Result $name 'FAIL' @("the step stopped: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))") }
    finally { try { Close-Info } catch { } }
}
# The Badlands nebula (Zone_Li01_Badlands_Nebula, card 65996: a red bold warning and the italic "Missouri"): INFO with no target shows
# the named zone the ship is in (ZoneAtPoint with a name required: the asteroid field inside it has none). The ship is put in the
# 1000 m shell above the field and inside the nebula (the field's half-height 19000, the nebula's 20000), where no asteroid stands;
# $true once the page's composition shows the zone's card (the game finds the zone on entry).
function In-Badlands {
    Send-FLConsole 'pos -7460, 19500, 63000'
    if (-not (Until { $x = Get-FLPilot; $x -and (Get-FLDistance $x.Pos @(-7460, 19500, 63000)) -lt 300 } 'the ship in the Badlands' 10)) { Note 'In-Badlands: the ship did not arrive'; return $false }
    Clear-Target
    Until { (Info -Quiet).HUD2 -match 'NAVIGATIONAL HAZARD' } "the Badlands' card in the composition" 20
}
# S1-Badlands: no target inside the nebula: PAGE == GAME == HUD2, its italic and colour required
function Badlands-Zone {
    $name = 'S1-Badlands'
    try {
        if (-not (In-Badlands)) { Result $name 'FAIL' @('the ship was not put in the Badlands, or its card never came into the composition within 20 s'); return }
        Open-Info
        $i = Info
        Compare-Blocks "$name (no target, in the nebula): PAGE == GAME == HUD2" $i @(@('PAGE', 'GAME'), @('PAGE', 'HUD2'))
        $last = $script:res[$script:res.Count - 1]
        if ($last.Status -eq 'PASS' -and ($last.Lines -join ' ') -notmatch 'italic [1-9].* coloured [1-9]') {
            $last.Status = 'FAIL'; $last.Lines += "the zone's card carries no italic or no colour: the styles not exercised"; Say "    -> FAIL: the zone's card carries no italic or no colour"
        }
    } catch { Result $name 'FAIL' @("the step stopped: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))") }
    finally { try { Close-Info } catch { } }
}
# S-underline: the game's INFO never shows an underline (only system cards carry it), so `nnpage info card 66106` (New York's
# system card: "Settled Planets" underlined, "Bases" bold and underlined) puts it on the page for one open: its [PAGE] is that card,
# its lines carry the underline's attributes (Check-Lines), the frame shows the rule; the open after it is the game's text again.
function Underline-Card([string]$size, [string]$form) {
    $name = "S-underline $size $form (card 66106)"
    try {
        $r = V 'nnpage info card 66106'
        if ($r -notmatch 'next INFO open') { Result $name 'FAIL' @("the verb refused: $r"); return }
        Open-Info
        $i = Info
        $problems = Check-Lines $i # (its array as returned: @() around it would add an empty element)
        $ps = Paras $i.PAGE # (assigned first: Paras returns its array whole)
        $under = @($ps | Where-Object { $_.Runs } | ForEach-Object { $_.Runs } | Where-Object { [Convert]::ToUInt32($_.A.Substring(0, 8), 16) -band 4 })
        $bad = @()
        if (($i.PAGE | Select-Object -First 1) -ne 'title=INFOCARD 66106') { $bad += "the page shows '$($i.PAGE | Select-Object -First 1)', not the card" }
        if (-not $under) { $bad += 'no underlined run in the card' }
        $bad += @(foreach ($x in $problems) { "LINES: $x" })
        Shot "${size}_${form}_card66106_underline"
        Close-Info
        Open-Info
        $j = Info -Quiet
        if (($j.PAGE | Select-Object -First 1) -eq 'title=INFOCARD 66106') { $bad += 'the next open still shows the test card' }
        if ($bad) { Result $name 'FAIL' $bad } else { Result $name 'PASS' @("underlined runs $($under.Count) ($(($under | Measure-Object N -Sum).Sum) characters), drawn with their attributes; the next open the game's text ('$($j.PAGE | Select-Object -First 1)')") }
    } catch { Result $name 'FAIL' @("the step stopped: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))") }
    finally { try { Close-Info } catch { } }
}
function Frame([string]$size, [string]$form, [string]$case) {
    try { Open-Info; Shot "${size}_${form}_${case}" }
    catch { Result "frame ${size}_${form}_${case}" 'FAIL' @("could not take it: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))") }
    finally { try { Close-Info } catch { } }
}

# ---- the space situations
$kTargets = [ordered]@{ # (the object ids are the hashes of the system file's nicknames, as the probe of 12 Nov 2026 read them: research\experience\runs\r3_probe\20261112_160819)
    'Manhattan' = '9e984242'; 'NewYorkGate-unknown' = '88b63901'; 'FortBush' = '9e9c4248'; 'BaltimoreShipyard' = 'be9cc648'; 'TradeLaneRing' = 'a6ec2bcb'; 'Sun' = 'a5071a8a'
    # the styled cards (decision 56; audit 84_ "The game check's cards"; ids = tools\flhash.ps1 Get-FLHash of the nicknames): the battleship
    # Missouri (Li01_03: 65747 red CLASSIFIED, 65748 its italic name), West Point (Li01_09: 65771 red CLASSIFIED; UNKNOWN STATION while not
    # found), the Manhattan Docking Ring (Dock_Ring_Li01_01: 66141 a red bold warning)
    'Missouri' = '9e982a42'; 'WestPoint' = '9e996242'; 'ManhattanDockingRing' = 'a444920a' }
$kFortBush = [Convert]::ToUInt32($kTargets['FortBush'], 16)
$kMissouri = [Convert]::ToUInt32($kTargets['Missouri'], 16)
function Space-Situations {
    # S1 and the park check
    Clear-Target
    Open-Info
    $i = Info
    Compare-Blocks 'S1 space, no target: PAGE == GAME == HUD2' $i @(@('PAGE', 'GAME'), @('PAGE', 'HUD2'))
    Check-Park 'S-park space'
    Close-Info
    # S2: each target selected before INFO opens
    $list = [Collections.Generic.List[object]]::new()
    foreach ($k in $kTargets.Keys) { $list.Add([pscustomobject]@{ Name = $k; Id = [Convert]::ToUInt32($kTargets[$k], 16); Kind = 'object' }) }
    $ship = Get-FLContacts -All | Where-Object { $_.Ship -and $_.Distance -gt 0 } | Sort-Object Distance | Select-Object -First 1
    if ($ship) { $list.Add([pscustomobject]@{ Name = "NPCship-$($ship.Name -replace '[^\w]+', '')-$($ship.Id)"; Id = $ship.Id; Kind = 'object' }) }
    else { Result 'S2-NPCship' 'SKIP' @('no NPC ship in the contact list (contacts all)') }
    $list.Add([pscustomobject]@{ Name = 'PlayerWaypoint'; Id = 0; Kind = 'waypoint' })
    foreach ($t in $list) {
        $name = "S2-$($t.Name)"
        try {
            Clear-Target
            if ($t.Kind -eq 'waypoint') {
                $r = V 'waypoint 6173 0 -45000'
                $wm = [regex]::Match($r, '^waypoint (\d+)')
                if (-not $wm.Success -or -not (Until { (Send-FL 'targetinfo') -match "id=$($wm.Groups[1].Value) " } 'the waypoint targeted' 5)) { Result $name 'SKIP' @("the waypoint was not set / targeted: $r"); continue }
            } elseif (-not (Select-Target $t.Id)) { Result $name 'SKIP' @("the game did not take target $($t.Id) (not in this system or not targetable)"); continue }
            Open-Info
            $i = Info
            if ($i.Head.target -eq '0x0') { Result $name 'SKIP' @('the target was gone when INFO opened') }
            else {
                Compare-Blocks "$name (target $($i.Head.target)): PAGE == GAME" $i @(, @('PAGE', 'GAME'))
                # KNOWN (audit R3, 12 Nov 2026): the player's waypoint. The game's window shows "WAYPOINT" / "Player Waypoint 1";
                # the page's composition reads the same object's visit value 0 (0x4D4D00, as the window's builder calls it) and shows
                # "UNKNOWN OBJECT" / 55023 on every open (run research\experience\runs\r3_probe\20261112_162911_waypoint). Unsettled:
                # shown as KNOWN only while the difference is exactly that one; anything else stays a FAIL.
                $last = $script:res[$script:res.Count - 1]
                if ($t.Kind -eq 'waypoint' -and $last.Status -eq 'FAIL' -and ($i.GAME | Select-Object -First 1) -eq 'title=WAYPOINT' -and
                    ($i.PAGE | Select-Object -First 1) -eq 'title=UNKNOWN OBJECT' -and $i.PAGE.Count -eq 2) { $last.Status = 'KNOWN'; Say "    -> KNOWN: the player's waypoint (the script's note above)" }
            }
        } catch { Result $name 'FAIL' @("the step stopped: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))") }
        finally {
            try { Close-Info } catch { }
            if ($t.Kind -eq 'waypoint') { try { $null = V 'waypoint clear' } catch { } }
        }
    }
    # S-refresh: a new target while open re-composes; a cleared one keeps the card
    $name = 'S-refresh'
    try {
        Clear-Target
        Open-Info
        $i0 = Info
        if ($null -eq $i0.PAGE) { Result $name 'MISSING' @('no [PAGE] with INFO open, no target'); return }
        if (-not (Select-Target $kFortBush)) { Result $name 'SKIP' @('the game did not take Fort Bush as the target'); return }
        $came = Until { $x = Info -Quiet; $null -ne $x.PAGE -and $x.PageHead.case -eq $x.Head.case -and (($x.PAGE -join "`n") -ceq ($x.HUD2 -join "`n")) -and $x.Head.target -ne '0x0' } 'the page re-composed for Fort Bush' 5
        $i1 = Info
        $card = $i1.HUD2
        $lines = @()
        # (Diff-Lines returns its array as one object: foreach, not the pipeline, walks its lines)
        if (-not $came) { $lines += @(foreach ($x in (Diff-Lines $i1.PAGE 'PAGE' $i1.HUD2 'HUD2')) { "Fort Bush selected while open: $x" }) }
        if (($i1.PAGE -join "`n") -ceq ($i0.PAGE -join "`n")) { $lines += 'Fort Bush selected while open: [PAGE] still the no-target text' }
        Clear-Target
        $null = Wait-FLFramesPassed 20 # (a measurement window: the page has 20 frames to (wrongly) re-compose)
        $i2 = Info
        $lines += @(foreach ($x in (Diff-Lines $i2.PAGE 'PAGE' $card 'FortBushCard')) { "target cleared while open: $x" })
        if ($lines.Count) { Result $name 'FAIL' $lines }
        else { Result $name 'PASS' @("selected while open: [PAGE] became '$($card[0])' ($($card.Count) lines, == fresh HUD2); cleared while open: [PAGE] kept it (HUD2 now '$($i2.HUD2[0])')") }
    } catch { Result $name 'FAIL' @("the step stopped: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))") }
    finally { try { Close-Info } catch { }; try { Clear-Target } catch { } }
    # last (they move the ship): the Missouri found, its card's red against the window's; the Badlands' red and italic
    Missouri-Found
    Badlands-Zone
}

# ---- the docked situations
function Docked-Situations {
    Open-Info
    $i = Info
    Compare-Blocks 'S-docked Manhattan: PAGE == GAME' $i @(, @('PAGE', 'GAME'))
    Check-Park 'S-park docked'
    # S-scroll: the market text
    $name = 'S-scroll docked'
    try {
        $lines = [int]$i.PageHead.lines; $room = [int]$i.PageHead.room; $max = $lines - $room
        if ($null -eq $i.PAGE) { Result $name 'MISSING' @('no [PAGE]') }
        elseif ($lines -le $room) { Result $name 'SKIP' @("the text fits: lines=$lines room=$room") }
        else {
            $bad = [Collections.Generic.List[string]]::new()
            $f0 = [int]$i.PageHead.first
            if ($f0 -ne 0) { $bad.Add("first=$f0 at open, want 0") }
            $null = V 'nnpage press up'; $null = Wait-FLFramesPassed 6 # (a measurement window: up at the top must not move)
            $f = [int](Info).PageHead.first; if ($f -ne 0) { $bad.Add("up at the top: first=$f, want 0") }
            $null = V 'nnpage press down'
            $ok = Until { [int](Info -Quiet).PageHead.first -eq 1 } 'first=1 after down' 3
            $f = [int](Info).PageHead.first; if (-not $ok) { $bad.Add("down: first=$f, want 1") }
            $null = V 'nnpage press up'
            $ok = Until { [int](Info -Quiet).PageHead.first -eq 0 } 'first=0 after up' 3
            $f = [int](Info).PageHead.first; if (-not $ok) { $bad.Add("up: first=$f, want 0") }
            # down to the end and past it: first stops at lines-room
            $seq = [Collections.Generic.List[int]]::new()
            for ($k = 0; $k -lt $max + 3; $k++) {
                $was = [int](Info -Quiet).PageHead.first
                $null = Send-FL 'nnpage press down'
                $null = Until { [int](Info -Quiet).PageHead.first -ne $was } 'first moved' 1 # (at the end it does not: the 1 s is the limit)
                $seq.Add([int](Info -Quiet).PageHead.first)
            }
            Note "down x$($max + 3): first $($seq -join ' ')"
            if ($seq[-1] -ne $max) { $bad.Add("after $($max + 3) downs first=$($seq[-1]), want lines-room=$max ($($seq -join ' '))") }
            if (@($seq | Where-Object { $_ -gt $max -or $_ -lt 0 }).Count) { $bad.Add("first left 0..$max ($($seq -join ' '))") }
            $null = Info
            if ($bad.Count) { Result $name 'FAIL' $bad } else { Result $name 'PASS' @("lines=$lines room=${room}: down 0->1, up 1->0, up at 0 stays, down stops at $max ($($seq -join ' '))") }
        }
    } catch { Result $name 'FAIL' @("the step stopped: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))") }
    finally { try { Close-Info } catch { } }
}

$initial = Get-FLDisplaySize (Get-FLInstallDir)
$firstSize = $true
try {
    foreach ($size in $Sizes) {
        $wh = $size -split 'x'; $script:W = [int]$wh[0]; $script:H = [int]$wh[1]
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
        Set-FLInstanceDisplay $Instance 'borderless' "$($script:W), $($script:H)"
        $firstPlace = $true
        foreach ($place in $Places) {
            $save = if ($place -eq 'docked') { $DockedSave } else { $SpaceSave }
            $script:ctx = "$size $place"
            $sw = [Diagnostics.Stopwatch]::StartNew()
            if ($firstPlace) { & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save $save | Select-Object -Last 1 | ForEach-Object { Note "deploy: $_" } }
            else { Note "load: $(Load-FLSave $save)" }
            $null = Wait-FLUntil { (Get-FLMode) -eq $place } "$place after $save" 90
            $null = Wait-FLFramesPassed 30
            Say ("  {0} ready in {1:n1} s" -f $script:ctx, $sw.Elapsed.TotalSeconds)
            if ($firstPlace) { $null = V 'winmode' }
            $firstPlace = $false
            if ($firstSize) {
                Set-Form $Forms[0]
                try { if ($place -eq 'space') { Space-Situations } else { Docked-Situations } }
                catch { Result "$place situations" 'FAIL' @("stopped: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))"); try { Close-Info } catch { } }
            }
            foreach ($form in $Forms) {
                $script:ctx = "$size $place $form"
                Set-Form $form
                if ($place -eq 'space') {
                    Clear-Target; Frame $size $form 'space_notarget'
                    if (Select-Target $kFortBush) { Frame $size $form 'space_fortbush' } else { Result "frame ${size}_${form}_space_fortbush" 'SKIP' @('Fort Bush not taken as the target') }
                    Clear-Target
                } else { Frame $size $form 'docked_manhattan' }
            }
            if ($place -eq 'space') {
                # the styled card (decision 56): the Missouri found (the ship moved beside it), red CLASSIFIED and its italic name,
                # in every form; then the underline through the test card, in the first form
                $found = Near-Missouri
                foreach ($form in $Forms) {
                    $script:ctx = "$size $place $form"
                    Set-Form $form
                    if ($found -and (Select-Target $kMissouri)) { Frame $size $form 'space_missouri' } else { Result "frame ${size}_${form}_space_missouri" 'FAIL' @('the Missouri not found / not taken as the target') }
                    Clear-Target
                }
                # the Badlands' card (no target, in the nebula): the red bold warning and the italic name, in every form
                $inside = In-Badlands
                foreach ($form in $Forms) {
                    $script:ctx = "$size $place $form"
                    Set-Form $form
                    if ($inside) { Frame $size $form 'space_badlands' } else { Result "frame ${size}_${form}_space_badlands" 'FAIL' @('the ship not in the Badlands / its card not composed') }
                }
                Set-Form $Forms[0]
                Underline-Card $size $Forms[0]
            }
        }
        $firstSize = $false
    }
    foreach ($p in @('space', 'docked')) { if ($Places -notcontains $p) { Result "$p situations" 'SKIP' @("'$p' not in -Places") } }
} catch {
    Result 'run' 'FAIL' @("the run stopped: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))")
} finally {
    try { if (Get-FLGameProcess) { Clear-FLPad; $null = Send-FL 'set input_mode 0' } } catch { }
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
    # this instance's screenshot sampler (deploy starts one per game): stopped with the game
    Get-CimInstance Win32_Process -Filter "Name='pwsh.exe'" | Where-Object { $_.CommandLine -match 'flshots\.ps1' -and $_.CommandLine -match "-Instance $([regex]::Escape($Instance))(\s|$)" } |
        ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    if ($initial) { Set-FLInstanceDisplay $Instance 'borderless' $initial }
}
foreach ($c in @(Get-FLCrashBlocks $crashSince)) { Result 'run' 'FAIL' @("a game crashed during the run (EXE\FLvibed_crash.log):`n$c") }
$by = $script:res | Group-Object Status | ForEach-Object { "$($_.Name) $($_.Count)" }
Say ''
Say ("info compare: {0}; {1} frames; {2:n0} s - {3}" -f ($by -join ', '), $script:shots, $total.Elapsed.TotalSeconds, $dir)
$bad = @($script:res | Where-Object { $_.Status -in 'FAIL', 'MISSING' })
if ($bad.Count) { Say 'INFO COMPARE: FAIL'; exit 1 }
Say 'INFO COMPARE: PASS'
exit 0
