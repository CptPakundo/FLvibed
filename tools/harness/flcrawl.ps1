# flcrawl.ps1 - the focus-graph crawler: the first automated UI-flow test (30 Oct 2026, WORK_QUEUE Q2b, IDEAS T7).
# On each screen it reaches, every control D-pad navigation sees (`uiitems`, the list the pad's own navigation uses)
# is visited: the cursor is put on it (`mouse`, a test fixture step), then each D-pad direction is pressed on the pad
# and the control the cursor lands on is the edge. The pad's focus in the game's menus IS the game cursor
# (controller.cpp UpdateUiNav: FindAt under the cursor, FindInDirection, SnapTo): read from `state`, never from the
# game's hover (`hovered=` is not recomputed in a background game - HARNESS.md).
# Per screen it judges (FAIL): a control the pad cannot reach from the screen's own first focus (BFS over the edges),
# a dead end (no direction leaves a control while others exist), a press that puts the cursor on no control, the
# focus leaving a modal window, a reached focus off the screen, a pad screen without
# prompts, B not returning to the parent screen within 3 presses; reported only: edges not reversed by the opposite
# direction. A transitions: only the safe ones listed in $Flow (never NEW GAME, EXIT, a load, a delete, quit).
# Output: research\experience\runs\crawl_<time>\graph.tsv (every edge) + verdicts.txt; the last line
# "CRAWL PASS|FAIL screens=.. controls=.. edges=.. issues=..".
#   -PlantCheck: first plants a dead end (debug `navplant`: the LOAD GAME button never a D-pad target) and requires the
#   crawler to report it unreachable - the crawler's own regression test (the done-check's "one planted dead end").
#   -Space: also the pause menu in flight (SaveC702).
param([int]$Instance = 1, [switch]$PlantCheck, [switch]$Space, [switch]$Keep)
$fl = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $fl 'tools\harness\flctl.ps1')
$deploy = Join-Path $fl 'tools\deploy.ps1'
$runDir = Join-Path $fl ("research\experience\runs\crawl_{0:yyyy-MM-dd_HHmm}" -f (Get-Date))
$null = New-Item -ItemType Directory -Force $runDir
$graphFile = Join-Path $runDir 'graph.tsv'
$verdictFile = Join-Path $runDir 'verdicts.txt'
"screen`tfrom_ctl`tfrom_name`tfrom_x`tfrom_y`tdir`tto_ctl`tto_name" | Set-Content $graphFile
$script:issues = [Collections.Generic.List[string]]::new()
$script:notes = [Collections.Generic.List[string]]::new()
$script:totals = @{ screens = 0; controls = 0; edges = 0 }
# (to the file and the host, never the pipeline: a function's return value must stay its own - a Tee-Object here once
# turned Enter-Main's $false into a truthy array)
function Say([string]$s) { $l = "$(Get-Date -Format HH:mm:ss) $s"; Add-Content -Path $verdictFile -Value $l; Write-Host $l }
$script:planting = $false
function Issue([string]$screen, [string]$s) {
    $script:issues.Add("${screen}: $s")
    Say "  $(if ($script:planting) { 'PLANTED (expected)' } else { 'ISSUE' }) ${screen}: $s"
}

# ---- reading the screen
$itemRx = '^(\S*)\s+ctl=([0-9A-F]+) vt=[0-9A-F]+ root=([0-9A-F]+) center=\((-?\d+),(-?\d+)\) half=\((-?\d+),(-?\d+)\) depth=\S+(?: main=0x([0-9a-f]+))?'
function Get-Items {
    @((Send-FL 'uiitems') -split "`n" | ForEach-Object {
        $m = [regex]::Match($_, $itemRx)
        if ($m.Success) {
            [pscustomobject]@{ name = $m.Groups[1].Value; ctl = $m.Groups[2].Value; root = $m.Groups[3].Value
                x = [int]$m.Groups[4].Value; y = [int]$m.Groups[5].Value; hw = [int]$m.Groups[6].Value; hh = [int]$m.Groups[7].Value
                main = $(if ($m.Groups[8].Success) { [Convert]::ToInt32($m.Groups[8].Value, 16) } else { -1 }) }
        }
    })
}
function Get-View {
    $st = Send-FL 'state'
    $mm = [regex]::Match($st, 'mouse=\((-?\d+),(-?\d+)\)'); $sc = [regex]::Match($st, 'screen=(\d+)x(\d+)')
    $menus = Send-FL 'menus'
    $items = Get-Items
    # safety: the game's EXIT / quit confirmation is never the crawler's to answer - stop before anything presses A
    if ($items | Where-Object { $_.name -like 'ExitButtonBox*' }) { throw 'UNSAFE: the game''s exit confirmation is open - the crawl stops' }
    $mx = [int]$mm.Groups[1].Value; $my = [int]$mm.Groups[2].Value
    # the focus: the control under the cursor, the smallest if they nest (ui::FindAt)
    $focus = $items | Where-Object { [Math]::Abs($_.x - $mx) -le $_.hw -and [Math]::Abs($_.y - $my) -le $_.hh } |
             Sort-Object { $_.hw * $_.hh } | Select-Object -First 1
    if (-not $focus) {
        # the list from `uiitems` can miss a thin control its hit-test grid stepped over (the Options page's sliders,
        # 54 px tall, run 6-7): the game's own hit test at the cursor decides then
        $h = [regex]::Match((Send-FL "hit $mx $my"), '= ([0-9A-F]{8}) (\S+)')
        if ($h.Success -and $h.Groups[1].Value -ne '00000000') {
            $known = $items | Where-Object { $_.ctl -eq $h.Groups[1].Value } | Select-Object -First 1
            $focus = if ($known) { $known } else {
                [pscustomobject]@{ name = $h.Groups[2].Value; ctl = $h.Groups[1].Value; root = ''; x = $mx; y = $my; hw = 0; hh = 0; main = -1; hit = $true } }
        }
    }
    [pscustomobject]@{ screen = [regex]::Match($menus, 'menus screen=(\S+)').Groups[1].Value
        prompts = [regex]::Match($menus, "items='([^']*)'").Groups[1].Value; device = [regex]::Match($menus, ' device=(\w+)').Groups[1].Value
        paused = $st -match 'paused=1'; w = [int]$sc.Groups[1].Value; h = [int]$sc.Groups[2].Value
        mx = $mx; my = $my; items = $items; focus = $focus }
}
function Key($it) { if ($it) { "$($it.name)@$($it.ctl)" } else { '-' } }
# the screen's controls where they rest (menus slide in, pages load their rows): two reads 60 frames apart agree, twice
function Wait-AtRest {
    $script:same = 0
    $null = Wait-FLUntil {
        $a = (Get-Items | ForEach-Object { "$($_.ctl)$($_.x),$($_.y)" }) -join ';'; $null = Wait-FLFramesPassed 60
        $b = (Get-Items | ForEach-Object { "$($_.ctl)$($_.x),$($_.y)" }) -join ';'
        if ($a -eq $b) { $script:same++ } else { $script:same = 0 }
        $script:same -ge 2 } 'the controls at rest' -TimeoutSec 15
}

# ---- one screen's focus graph
$dirs = 'Up', 'Down', 'Left', 'Right'
# screens whose window is modal: the focus must stay in the first focus's window (padmenus' screen names)
$modalScreens = 'mpprompt', 'pause', 'confirm', 'popup', 'death', 'save'
$opposite = @{ Up = 'Down'; Down = 'Up'; Left = 'Right'; Right = 'Left' }
function Crawl-Screen([string]$expect, [string[]]$Directions = $dirs) {
    # the screen settled: its own first focus placed (padmenus snaps after 250 ms) - a press moves the cursor onto
    # something when the screen starts without one
    $null = Wait-FLUntil { $script:v = Get-View; $script:v.screen -eq $expect -and $script:v.items.Count } "screen $expect with controls" -TimeoutSec 15
    # padmenus' own first focus placed (`menus focused=1`; the Options screen waits for its page and loads the page's
    # rows ~1.5-3 s after opening - run 6 crawled before the rows were there), else at most 5 s
    try { $null = Wait-FLUntil { (Send-FL 'menus') -match ' focused=1 ' } "the first focus placed on $expect" -TimeoutSec 5 } catch {}
    Wait-AtRest
    # the screen's own first focus (padmenus places it once the screen settles: the Options screen ~1.5-3 s); none
    # after 5 s -> one D-pad press (the generic navigation starts somewhere), else it is an issue
    $v0 = $null
    try { $null = Wait-FLUntil { $script:v0 = Get-View; $script:v0.focus } "the first focus on $expect" -TimeoutSec 5; $v0 = $script:v0 }
    catch { Press-FLPad Down; $null = Wait-FLFramesPassed 4; $v0 = Get-View }
    $start = $v0.focus
    $items = @($v0.items | Where-Object { $_.x -gt 0 -and $_.y -gt 0 -and $_.x -lt $v0.w -and $_.y -lt $v0.h })
    # a modal screen: only its own window's controls are the pad's (those behind are rightly out of reach)
    if ($modalScreens -contains $expect -and $start) {
        $behind = @($items | Where-Object { $_.root -ne $start.root }).Count
        $items = @($items | Where-Object { $_.root -eq $start.root })
        if ($behind) { Say "screen $expect`: modal - $behind control(s) of the windows behind left out" }
    }
    $script:totals.screens++; $script:totals.controls += $items.Count
    Say "screen $expect`: $($items.Count) controls, first focus $(Key $start), prompts '$($v0.prompts)', device $($v0.device)"
    if (-not $start) { Issue $expect 'no first focus (the D-pad placed the cursor on nothing)' }
    if ($v0.device -eq 'pad' -and -not $v0.prompts) { Issue $expect 'a pad screen without prompts' }
    $edges = @{}
    foreach ($it in $items) {
        $k = Key $it
        $edges[$k] = @{}
        foreach ($d in $Directions) {
            $null = Send-FL "mouse $($it.x) $($it.y)"; $null = Wait-FLFramesPassed 3
            Press-FLPad $d; $null = Wait-FLFramesPassed 4
            $v = Get-View
            if ($v.screen -ne $expect) { Issue $expect "D-pad $d on $k left the screen (-> $($v.screen))"; return $false }
            $to = Key $v.focus
            if ($v.focus -and $v.focus.PSObject.Properties['hit'] -and -not ($script:notes -contains "${expect}: hit test only: $to")) {
                $script:notes.Add("${expect}: hit test only: $to") # a control `uiitems` did not list (its edges not crawled)
            }
            $edges[$k][$d] = $to
            $script:totals.edges++
            "$expect`t$($it.ctl)`t$($it.name)`t$($it.x)`t$($it.y)`t$d`t$(if ($v.focus) { $v.focus.ctl } else { '-' })`t$(if ($v.focus) { $v.focus.name } else { '-' })" | Add-Content $graphFile
        }
    }
    if ($start) { $null = Send-FL "mouse $($start.x) $($start.y)" }
    # reachability from the first focus
    $seen = [Collections.Generic.HashSet[string]]::new(); $queue = [Collections.Generic.Queue[string]]::new()
    if ($start) { $null = $seen.Add((Key $start)); $queue.Enqueue((Key $start)) }
    while ($queue.Count) {
        $k = $queue.Dequeue()
        if (-not $edges.ContainsKey($k)) { continue }
        foreach ($to in $edges[$k].Values) { if ($to -ne '-' -and $seen.Add($to)) { $queue.Enqueue($to) } }
    }
    foreach ($it in $items) {
        $k = Key $it
        if (-not $seen.Contains($k)) { Issue $expect "unreachable by the D-pad from the first focus: $k at ($($it.x),$($it.y))" }
        # a modal screen keeps the focus in its own window (the first focus's root): an edge into a window behind is one
        if ($modalScreens -contains $expect -and $start -and $it.root -eq $start.root) {
            foreach ($d in $edges[$k].Keys) {
                $to = @($v0.items | Where-Object { (Key $_) -eq $edges[$k][$d] })[0]
                if ($to -and $to.root -ne $start.root) { Issue $expect "the focus left the modal window: $k -$d-> $(Key $to) (window $($to.root))" }
            }
        }
        foreach ($d in $edges[$k].Keys) {
            if ($edges[$k][$d] -eq '-') { Issue $expect "the focus landed on no control: $k -$d-> nothing (the cursor off every control)" }
        }
        $moves = @($edges[$k].Values | Where-Object { $_ -ne $k -and $_ -ne '-' })
        if ($items.Count -gt 1 -and -not $moves.Count) { Issue $expect "dead end: no direction leaves $k" }
        foreach ($d in $edges[$k].Keys) {
            $to = $edges[$k][$d]
            if ($to -ne $k -and $to -ne '-' -and $edges.ContainsKey($to) -and $edges[$to].ContainsKey($opposite[$d]) -and $edges[$to][$opposite[$d]] -ne $k) {
                $script:notes.Add("${expect}: not reversed: $k -$d-> $to, back $($opposite[$d]) -> $($edges[$to][$opposite[$d]])")
            }
        }
    }
    $off = @($v0.items | Where-Object { $seen.Contains((Key $_)) -and -not ($_.x -gt 0 -and $_.y -gt 0 -and $_.x -lt $v0.w -and $_.y -lt $v0.h) })
    foreach ($it in $off) { Issue $expect "a reachable focus off the screen: $(Key $it) at ($($it.x),$($it.y))" }
    $listed = @($items | Where-Object { $seen.Contains((Key $_)) }).Count
    Say "screen $expect`: $listed of $($items.Count) reachable$(if ($seen.Count -gt $listed) { " (+$($seen.Count - $listed) found by the hit test only)" })"
    $true
}
# B back to the parent screen within 3 presses
function Back-To([string]$from, [string]$parent) {
    for ($i = 1; $i -le 3; $i++) {
        Press-FLPad B
        # each press gets up to 4 s to reach the parent (the menus' slides; the Options screen handles B once settled);
        # another B only while the screen is still the same one: a B too many on the main menu opens the game's EXIT
        # confirmation (run 4, 30 Oct 2026)
        try { $null = Wait-FLUntil { (Get-View).screen -eq $parent } "B on $from" -TimeoutSec 4; Say "B: $from -> $parent in $i"; return $true } catch {}
        $s = (Get-View).screen
        if ($s -ne $from) { Issue $from "B went to $s, not $parent"; return $false }
    }
    Issue $from "B did not return to $parent within 3 presses (now $s)"; $false
}
# A on a main menu button by its id (menu.h MainButton) -> the expected screen
function Enter-Main([int]$id, [string]$expect) {
    $b = Get-Items | Where-Object { $_.main -eq $id } | Select-Object -First 1
    if (-not $b) { Issue 'mainmenu' "no main menu button 0x$('{0:x}' -f $id)"; return $false }
    $null = Send-FL "mouse $($b.x) $($b.y)"; $null = Wait-FLFramesPassed 3
    Press-FLPad A
    try { $null = Wait-FLUntil { (Get-View).screen -eq $expect } "A on main 0x$('{0:x}' -f $id) -> $expect" -TimeoutSec 10; $true }
    catch { Issue 'mainmenu' "A on main 0x$('{0:x}' -f $id) did not open $expect ($_)"; $false }
}
function Wait-MainMenu {
    $null = Wait-FLUntil { @(Get-Items | Where-Object { $_.main -ge 0 -and $_.x -gt $_.hw }).Count -ge 4 } 'the main menu slid in' -TimeoutSec 120
    # the pad's menu layer (padmenus: `menus screen=`) runs only while the pad is the input device: one press makes it so
    if ((Get-View).screen -ne 'mainmenu') { Press-FLPad Down }
    $null = Wait-FLUntil { (Get-View).screen -eq 'mainmenu' } 'the pad on the main menu' -TimeoutSec 10
    # (and the buttons where they rest: two reads 10 frames apart agree)
    $null = Wait-FLUntil { $a = (Get-Items | ForEach-Object { $_.x }) -join ','; $null = Wait-FLFramesPassed 10
                           $a -eq ((Get-Items | ForEach-Object { $_.x }) -join ',') } 'the main menu at rest' -TimeoutSec 10
    $null = Wait-FLFramesPassed 60
}

# ---- the flow
try {
    Say "crawl instance $Instance (dev build) -> $runDir"
    if (-not $env:FLV_REPLAY) { $null = & $deploy -Instance $Instance -Background -Restart -NoBuild 2>&1 }   # (a dry-run: no game)
    $null = Wait-FLUntil { (Send-FL 'state') -match 'mode=' } 'the game answering' -TimeoutSec 120
    Wait-MainMenu
    if ($PlantCheck) {
        $load = Get-Items | Where-Object { $_.main -eq 3 } | Select-Object -First 1
        Say "plant: navplant $($load.ctl) (LOAD GAME never a D-pad target)"
        $null = Send-FL "navplant $($load.ctl)"
        $before = $script:issues.Count; $script:planting = $true
        $null = Crawl-Screen 'mainmenu'
        $script:planting = $false
        $caught = @($script:issues | Select-Object -Skip $before | Where-Object { $_ -match "unreachable.*@$($load.ctl)" }).Count
        $null = Send-FL 'navplant off'
        # the planted issues and notes are the expected result, not the UI's: out of the lists, the verdict on the catch
        while ($script:issues.Count -gt $before) { $script:issues.RemoveAt($script:issues.Count - 1) }
        $script:notes.Clear()
        $script:totals = @{ screens = 0; controls = 0; edges = 0 }
        $null = Wait-FLFramesPassed 10
        if ($caught) { Say "plant: CAUGHT (reported unreachable)" } else { Issue 'plant' 'the planted dead end was NOT reported' }
    }
    $null = Crawl-Screen 'mainmenu'
    foreach ($f in @(@{ id = 3; screen = 'load' }, @{ id = 4; screen = 'mpprompt' }, @{ id = 0x0A; screen = 'options' })) {
        if (Enter-Main $f.id $f.screen) {
            # (every direction on the Options screen too: the D-pad only moves the cursor there - no code turns a slider
            # on Left / Right, and the game moves its sliders by drag; only A changes a setting)
            $null = Crawl-Screen $f.screen
            $null = Back-To $f.screen 'mainmenu'
        }
        Wait-MainMenu # (the menu slides its buttons in again after every screen it comes back from)
    }
    Say 'not followed (safety): NEW GAME (0x10), EXIT (0x0B), a save row''s load / delete'
    if ($Space) {
        if (-not $env:FLV_REPLAY) { $null = & $deploy -Instance $Instance -Background -Restart -NoBuild -Save SaveC702.fl 2>&1 }
        $null = Wait-FLUntil { (Send-FL 'state') -match 'mode=space' } 'in space (SaveC702)' -TimeoutSec 180
        $null = Wait-FLFramesPassed 60
        Press-FLPad Start
        if (Crawl-Screen 'pause') {
            Press-FLPad B; $null = Wait-FLFramesPassed 30
            if ((Get-View).paused) { Issue 'pause' 'B did not resume the game' } else { Say 'B: pause -> flight' }
        }
    }
} catch {
    Issue 'crawler' "stopped: $_"
} finally {
    try { $null = Send-FL 'navplant off' } catch {}
    if (-not $Keep -and -not $env:FLV_REPLAY) { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue }
}
foreach ($n in $script:notes) { Say "  note: $n" }
$verdict = if ($script:issues.Count) { 'FAIL' } else { 'PASS' }
Say "CRAWL $verdict screens=$($script:totals.screens) controls=$($script:totals.controls) edges=$($script:totals.edges) issues=$($script:issues.Count) not-reversed=$($script:notes.Count)"
