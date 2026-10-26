<#
Story assembler (user, 20 Oct 2026: read and catalogue the whole story - dialogue, scripts, journal): one ordered text
file per story mission, from the game's own data, for reading and analysis (game content: written OUTSIDE the
repository, research\story\ by default).

Per mission, in this order:
  1. TEXT   - every string id the mission script (DATA\MISSIONS\<m>\m<nn>.ini) references, in script order, resolved
              from the string dump (tools\dump-resources.ps1): titles, offers, objectives, journal entries ("PERSONAL
              ENTRY"), popups. Kind tags: JOURNAL / OBJECTIVE / TEXT.
  2. SCENES - base-room scenes of the mission (DATA\MISSIONS\<m>\m0nn_sNNN*.ini): their dx_sNNN lines from the subtitle
              database (speaker: text).
  3. SPACE  - the mission's space dialogue: DX_Mnn_* lines of the subtitle database, in id order (the ids are numbered
              in story order), plus the cutscenes (.thn) that reference them.
Usage: tools\storydump.ps1 -Strings <strings.tsv> [-Out <dir>]
#>
param([Parameter(Mandatory)][string]$Strings, [string]$Out)
$root = Split-Path $PSScriptRoot -Parent
if (-not $Out) { $Out = Join-Path $root 'research\story' }
$null = New-Item -ItemType Directory -Force $Out
$missions = Join-Path $root 'Install\Freelancer HD Edition\DATA\MISSIONS'

$str = @{}
foreach ($l in [IO.File]::ReadLines((Resolve-Path $Strings).Path)) {
    $c = $l -split "`t", 3
    if ($c.Count -eq 3) { $str[[int]$c[0]] = ($c[2] -replace '\\n', ' ') }
}
$subs = @{}
foreach ($l in [IO.File]::ReadLines((Join-Path $root 'subtitles\data\subtitles.tsv'))) {
    if ($l.StartsWith('#') -or $l.StartsWith("id`t")) { continue }
    $c = $l -split "`t"
    if ($c.Count -lt 7 -or $c[2] -eq 'chunk') { continue }
    $key = $c[0].ToLower()
    if (-not $subs.ContainsKey($key)) { $subs[$key] = "$(if ($c[5]) { $c[5] } else { $c[1] }): $($c[6])" }
}
$summary = @()
foreach ($dir in Get-ChildItem $missions -Directory | Where-Object Name -match '^M\d') {
    $m = $dir.Name
    $num = [int]($m -replace '\D', '')
    $o = [Collections.Generic.List[string]]::new()
    $o.Add("# $m - story transcript (assembled from the game data by tools\storydump.ps1)")
    # 1. text ids in script order
    $o.Add(''); $o.Add('## TEXT (script order)')
    $seen = @{}
    $nText = 0; $nJournal = 0
    foreach ($ini in Get-ChildItem $dir.FullName -Filter '*.ini' | Sort-Object { $_.Name -notmatch '^m\d\d' }, Name) {
        $ln = 0
        foreach ($line in [IO.File]::ReadLines($ini.FullName)) {
            $ln++
            if ($line -match '^\s*;') { continue }
            foreach ($mt in [regex]::Matches($line, '\b(\d{4,6})\b')) {
                $id = [int]$mt.Groups[1].Value
                if (-not $str.ContainsKey($id) -or $seen[$id]) { continue }
                $t = $str[$id].Trim()
                if ($t.Length -lt 3) { continue }
                $seen[$id] = 1
                $kind = if ($t -match 'PERSONAL ENTRY') { $nJournal++; 'JOURNAL' } elseif ($t.Length -le 90 -and $line -match '(?i)objective|obj|SetTitle|nav') { 'OBJECTIVE' } else { 'TEXT' }
                $nText++
                $o.Add("[$kind $id $($ini.Name):$ln] $t")
            }
        }
    }
    # 2. base scenes
    $o.Add(''); $o.Add('## SCENES (base rooms)')
    $nScene = 0
    foreach ($sc in Get-ChildItem $dir.FullName -Filter 'm0*_s*.ini' | Sort-Object Name) {
        $sn = [regex]::Match($sc.Name, '_s(\d{3})').Groups[1].Value
        if (-not $sn) { continue } # (m013_endgame_*: no scene number - matched every scene)
        $o.Add("### $($sc.Name)")
        foreach ($k in ($subs.Keys | Where-Object { $_ -like "dx_s$sn*" } | Sort-Object)) { $o.Add("$k | $($subs[$k])"); $nScene++ }
    }
    # 3. space dialogue + cutscenes referencing it
    $o.Add(''); $o.Add('## SPACE dialogue (id order)')
    # the lines this mission's scripts reference (M01a / M01b share the DX_M01 ids), in id (story) order
    $refs = @{}
    foreach ($f in Get-ChildItem $dir.FullName -File) {
        foreach ($mt in [regex]::Matches([IO.File]::ReadAllText($f.FullName), '(?i)\bdx_m\d+\w*')) { $refs[$mt.Value.ToLower()] = 1 }
    }
    $keys = @($subs.Keys | Where-Object { $refs.ContainsKey($_) } | Sort-Object)
    foreach ($k in $keys) { $o.Add("$k | $($subs[$k])") }
    $thn = @(Get-ChildItem $dir.FullName -Filter '*.thn' | ForEach-Object { $_.Name })
    $o.Add(''); $o.Add("## CUTSCENES: $($thn -join ', ')")
    $file = Join-Path $Out "$m.txt"
    [IO.File]::WriteAllLines($file, $o)
    $summary += "{0,-5} text {1,4} (journal {2,2}), scene lines {3,4}, space lines {4,4}, cutscenes {5}" -f $m, $nText, $nJournal, $nScene, $keys.Count, $thn.Count
}
# The world between the missions (user, 20 Oct 2026: news, rumours too): DATA\MISSIONS\news.ini (headline + text per
# story stage and base) and mbases.ini (bar characters' rumours / knowledge per story stage), deduplicated, in story
# stage order (the rank values: mission_01a_loaded .. mission_end).
$stageOrder = @{}
$i = 0
foreach ($sname in 'base_0_rank', 'mission_01a_loaded', 'mission_01a_accepted', 'mission_01b_loaded', 'mission_01b_accepted',
                   'mission_02_loaded', 'mission_02_accepted', 'mission_03_loaded', 'mission_03_accepted', 'mission_04_loaded',
                   'mission_04_accepted', 'mission_05_loaded', 'mission_05_accepted', 'mission_06_loaded', 'mission_06_accepted',
                   'mission_07_loaded', 'mission_07_accepted', 'mission_08_loaded', 'mission_08_accepted', 'mission_09_loaded',
                   'mission_09_accepted', 'mission_10_loaded', 'mission_10_accepted', 'mission_11_loaded', 'mission_11_accepted',
                   'mission_12_loaded', 'mission_12_accepted', 'mission_13_loaded', 'mission_13_accepted', 'mission_end') { $stageOrder[$sname] = $i++ }
$w = [Collections.Generic.List[string]]::new()
$w.Add('# WORLD - news, rumours, knowledge by story stage (tools\storydump.ps1)')
$news = @(); $cur = $null
foreach ($l in [IO.File]::ReadLines((Join-Path $missions 'news.ini'))) {
    if ($l -match '^\[NewsItem\]') { if ($cur) { $news += $cur }; $cur = @{ bases = @() } }
    elseif ($cur -and $l -match '^\s*(\w+)\s*=\s*(.+)$') { if ($Matches[1] -eq 'base') { $cur.bases += $Matches[2].Trim() } else { $cur[$Matches[1]] = $Matches[2].Trim() } }
}
if ($cur) { $news += $cur }
$w.Add(''); $w.Add("## NEWS ($($news.Count) items; stage from-to, bases)")
$byHead = $news | Group-Object { "$($_.rank)|$($_.headline)" }
foreach ($g in $byHead | Sort-Object { $s = (($_.Group[0].rank -split ',')[0]).Trim(); if ($stageOrder.ContainsKey($s)) { $stageOrder[$s] } else { 99 } }) {
    $n = $g.Group[0]; $h = [int]$n.headline; $tx = [int]$n.text
    $bases = ($g.Group | ForEach-Object { $_.bases }) | Sort-Object -Unique
    $w.Add("[$($n.rank)] ($(@($bases).Count) bases: $(($bases | Select-Object -First 6) -join ' '))")
    $w.Add("  HEADLINE: $($str[$h])"); $w.Add("  TEXT: $($str[$tx])")
}
$rum = @{}; $know = @{}
foreach ($l in [IO.File]::ReadLines((Join-Path $missions 'mbases.ini'))) {
    if ($l -match '^\s*rumor(?:_type2)?\s*=\s*([\w]+)\s*,\s*([\w]+)\s*,\s*\d+\s*,\s*(\d+)') {
        $id = [int]$Matches[3]; if (-not $rum.ContainsKey($id)) { $rum[$id] = @{ from = $Matches[1]; to = $Matches[2]; n = 0 } }; $rum[$id].n++
    } elseif ($l -match '^\s*know\s*=\s*(\d+)\s*,\s*(\d+)') {
        $id = [int]$Matches[2]; if (-not $know.ContainsKey($id)) { $know[$id] = 0 }; $know[$id]++
    }
}
$w.Add(''); $w.Add("## RUMOURS ($($rum.Count) unique; stage from-to, characters)")
foreach ($k in $rum.Keys | Sort-Object { $s = $rum[$_].from; if ($stageOrder.ContainsKey($s)) { $stageOrder[$s] } else { 99 } }, { $_ }) {
    if ($str.ContainsKey($k)) { $w.Add("[$($rum[$k].from)..$($rum[$k].to) x$($rum[$k].n) #$k] $($str[$k])") }
}
$w.Add(''); $w.Add("## KNOWLEDGE ($($know.Count) unique)")
foreach ($k in $know.Keys | Sort-Object) { if ($str.ContainsKey($k)) { $w.Add("[#$k x$($know[$k])] $($str[$k])") } }
[IO.File]::WriteAllLines((Join-Path $Out 'WORLD.txt'), $w)
$summary += "WORLD news $($news.Count), rumours $($rum.Count) unique, knowledge $($know.Count) unique"
$summary
"written: $Out"
