# The save lists and the pad's DELETE (audit 139_ FIX NOW A and B). One background game per display size and HUD style:
#   front   main menu -> LOAD GAME: `menuprobe rows` (rows held / drawn, box and line height in pixels, the bar); frame front
#   game    in a docked save, pause -> LOAD GAME: the same; frame game; scrolled to the end: the last save drawn; frame game_end
#   delete  (load list) the cursor onto row 1 (the list's own hovered row), the pad's X: DELETE's enabled byte (+0x5D6) and the
#           game's confirm box (Confirm_delBtnNo); frame delete; NO answers it (nothing is deleted)
# Checks: the rows drawn fit the panel (rows x line <= the bar's height in game, the box on the front list, + a quarter line) on
# both columns of the front, load and save lists; no write while a list stands still (HUD 2); save mode: pause -> SAVE GAME; every save reachable by the bar;
# X opens the confirm. Usage (PowerShell tool): tests\menu_rows.ps1 -Instance 11 [-Sizes 1280x800,3840x2160] [-Style 2]
#   [-Tag before]    build\release. Frames: research\experience\runs\menu_rows\<stamp>_<tag>. Exit 1 = a check failed.
param([string]$Instance = '', [string[]]$Sizes = @('1280x800', '3840x2160'), [int]$Style = 2, [string]$Tag = 'run')
$ErrorActionPreference = 'Stop'
if (-not $Instance) { throw '-Instance <n>' }
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
$dir = Join-Path $root "research\experience\runs\menu_rows\$(Get-Date -Format yyyyMMdd_HHmmss)_$Tag"
New-Item -ItemType Directory -Force $dir | Out-Null
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { $l = "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; $l; Add-Content (Join-Path $dir 'log.txt') $l; if (-not $ok) { $bad.Add($what) } }
function Note([string]$s) { $s; Add-Content (Join-Path $dir 'log.txt') $s }
function Settle([int]$n = 20) { $null = Wait-FLFramesPassed $n }
# `menuprobe rows` as numbers
function Rows {
    $t = "$(Send-FL 'menuprobe rows')"
    $cols = foreach ($m in [regex]::Matches($t, '(?m)^(names|dates) count (\d+) rows (\d+) first (\d+) box [\d.]+ room ([\d.]+) line (\d+)')) {
        [pscustomobject]@{ Col = $m.Groups[1].Value; Count = [int]$m.Groups[2].Value; Rows = [int]$m.Groups[3].Value; First = [int]$m.Groups[4].Value
                           Box = [double]$m.Groups[5].Value; Line = [int]$m.Groups[6].Value }
    }
    [pscustomobject]@{ Text = $t; Cols = @($cols) }
}
function Judge([string]$where) {
    $r = Rows
    Note "  $where`: $($r.Text -replace "`r?`n", ' | ') || $(Send-FL 'uiskin lists')"
    Check ($r.Cols.Count -eq 2) "$where`: both columns read"
    foreach ($c in $r.Cols) {
        # (a quarter line of slack: the game's own save list at its own font runs 3 px past its bar, 16 x 23 px vs 365 px at 1280x800)
        Check ([Math]::Min($c.Rows, $c.Count) * $c.Line -le $c.Box + $c.Line / 4) "$where $($c.Col): the rows drawn fit the panel ($([Math]::Min($c.Rows, $c.Count)) x $($c.Line) px vs $([Math]::Round($c.Box)) px)"
    }
    $r
}
function Shot([string]$name) { $null = Save-FLShot -Name "${size}_s${Style}_$name" -Dir $dir }

$initial = Get-FLDisplaySize (Get-FLInstallDir)
try {
    foreach ($size in $Sizes) {
        Note "--- $size hud_style $Style"
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
        Set-FLInstanceDisplay $Instance 'borderless' (($size -split 'x') -join ', ')
        & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Set "hud_style=$Style" | Select-Object -Last 1
        $null = Wait-FLUntil { (Send-FL 'menu load') -match '^clicked' } 'main menu LOAD GAME' 60
        $null = Wait-FLUntil { (Send-FL 'menuprobe rows') -match '^front' } 'the front list' 20
        Settle 60
        Note "  $(Send-FL 'winmode')"
        $null = Judge 'front'; Shot 'front'
        $null = Load-FLSave 'SaveC701.fl'   # (docked at Manhattan: the pause menu offers SAVE GAME there)
        $null = Send-FL 'set input_mode 1'
        $null = Wait-FLUntil { $r = Send-FL 'pause'; $r -match 'open|requested' } 'pause' 10
        $null = Wait-FLUntil { (Send-FL 'pausecodes') -notmatch 'not open' } 'pause menu' 10
        $null = Send-FL 'pauseload'
        $null = Wait-FLUntil { (Send-FL 'menuprobe rows') -match '^game' } 'the in-game load list' 15
        Settle 60
        $g = Judge 'game'; Shot 'game'
        # every save reachable: the bar to its end, then the last save among the rows drawn
        $s = Send-FL 'scroll LoadSave_Scroll 999'
        Settle 10
        $e = Rows
        $n = $e.Cols[0]
        Note "  end: $s | $($e.Text -replace "`r?`n", ' | ')"
        Check ($n.First + [Math]::Min($n.Rows, $n.Count) -ge $n.Count) "game: the bar reaches the last save (first $($n.First) + $($n.Rows) rows of $($n.Count))"
        Shot 'game_end'
        $null = Send-FL 'scroll LoadSave_Scroll -999'
        Settle 10
        # the pad's X on row 1: the cursor onto it by the list's own hovered row (+0x7C)
        $list = [regex]::Matches("$(Send-FL 'uiitems')", '(?m)^(\S*)\s+ctl=(\w+) vt=005E5F9C root=\w+ center=\((\d+),(\d+)\) half=\((\d+),(\d+)\)') | Select-Object -First 1
        if (-not $list) { Check $false 'delete: the list control found'; continue }
        $cx = [int]$list.Groups[3].Value; $cy = [int]$list.Groups[4].Value; $hh = [int]$list.Groups[6].Value
        $hit = $false
        for ($y = $cy - $hh; $y -le $cy + $hh -and -not $hit; $y += 3) {
            $null = Send-FL "mouse $cx $y"; Settle 2
            if ((Send-FL 'menus row') -match 'hovered\(\+0x7C\)=1 ') { $hit = $true }
        }
        Check $hit 'delete: the cursor on row 1'
        Note "  before X: $((Send-FL 'menuprobe window') -split "`n" | Select-Object -First 1)"
        Clear-FLPad; Set-FLPad -Buttons X; Settle 6; Clear-FLPad; Settle 30
        $w = (Send-FL 'menuprobe window') -split "`n" | Select-Object -First 1
        Note "  after X: $w"
        $confirm = (Send-FL 'visible Confirm_delBtnNo') -match '^visible'
        Check ($w -match 'delete_on 1') "X: DELETE enabled ($w)"
        Check $confirm 'X: the game''s confirm box stands'
        Shot 'delete'
        if ($confirm) { $null = Send-FL 'click Confirm_delBtnNo'; Settle 30 }
        $after = Rows
        Check ($after.Cols[0].Count -eq $g.Cols[0].Count) "nothing deleted ($($g.Cols[0].Count) -> $($after.Cols[0].Count) saves)"
        # no write while nothing changes: the writes counted across 60 idle frames
        $w0 = Send-FL 'uiskin lists'; Settle 60; $w1 = Send-FL 'uiskin lists'
        Note "  idle: $w0 -> $w1"
        if ($Style -ne 0) { Check ([regex]::Match($w0, 'writes (\d+)').Groups[1].Value -eq [regex]::Match($w1, 'writes (\d+)').Groups[1].Value) 'no writes while the list stands still' }
        # the save mode (16 rows, its own bar): pause -> SAVE GAME
        $null = Send-FL 'click LoadSave_Close'; Settle 20
        $null = Wait-FLUntil { $r = Send-FL 'pause'; $r -match 'open|requested' } 'pause' 10
        $box = Wait-FLUntil { [regex]::Match("$(Send-FL 'pausecodes')", '(?m)^(PauseButtonBox\d+) ctl \S+ code 2\s*$').Groups[1].Value } 'the pause menu''s Save' 10
        $null = Send-FL "click $box"
        $null = Wait-FLUntil { (Send-FL 'menuprobe rows') -match '^game gameRows 16' } 'the save list' 15
        Settle 60
        $null = Judge 'save'; Shot 'save'
        $null = Send-FL 'click LoadSave_Close'; Settle 20
    }
} catch { Check $false "the run stopped: $($_.Exception.Message) (line $($_.InvocationInfo.ScriptLineNumber))" }
finally {
    Clear-FLPad
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
    if ($initial) { Set-FLInstanceDisplay $Instance 'borderless' $initial }
}
Note "frames: $dir"
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'all checks passed'
