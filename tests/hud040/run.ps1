# Phase 2 of 0.4.0: the checks only a running game can answer (tests\hud040\README.md says what each one passes on, and the budget). One background game per
# size on a numbered instance, muted, driven by game state and debug verbs; frames and logs in research\experience\runs\hud040_<Label>\ (git-ignored).
# Usage (PowerShell tool, from the worktree, only when the driver says the games are free):
#   tests\hud040\run.ps1 -Label <name> -Only sizes,first,alerts,form,motion,flash,cue,menus,captures [-Sizes '3840, 2160','1280, 800'] [-Instance 7]
# Each section reports PASS / FAIL / LOOK (a frame a person or run-judge must read). Exit 1 = a FAIL. No section reruns itself: a failure stops at its evidence.
param([Parameter(Mandatory)][string]$Label,
      [string[]]$Only = @('sizes', 'first', 'alerts', 'form', 'motion', 'flash', 'cue', 'lock', 'menus', 'dialogs', 'padmissile', 'dock', 'captures'),
      [string[]]$Sizes = @('3840, 2160', '1280, 800'), [int]$Instance = 7)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
$dir = Join-Path $root "research\experience\runs\hud040_$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$bad = [Collections.Generic.List[string]]::new()
$home_ = Get-FLDisplaySize
function Result([string]$kind, [string]$what) { "{0,-5} {1}" -f $kind, $what; if ($kind -eq 'FAIL') { $bad.Add($what) } }
function Check([bool]$ok, [string]$what) { Result $(if ($ok) { 'PASS' } else { 'FAIL' }) $what }
function Shot([string]$name) { $f = Save-FLShot -Name $name -Dir $dir; Move-Item -LiteralPath $f (Join-Path $dir "$name.png") -Force; Result 'LOOK' "$name.png" }
function Start-Game([string]$size, [string]$save, [string[]]$modules = @('subtitles', 'controller')) {
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Set-FLInstanceDisplay $Instance 'borderless' $size
    & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Modules $modules -Save $save | Select-Object -Last 1
    $null = Wait-FLFramesPassed 200
    $old = Join-Path (Get-FLInstallDir) 'EXE\grab'
    if (Test-Path $old) { [IO.Directory]::Delete($old, $true) }
}
function Stop-Game { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue }
function NoFault([string]$where) {
    $log = Join-Path (Get-FLInstallDir) 'EXE\FLController.log'
    Check (@(Select-String -Path $log -Pattern 'FATAL').Count -eq 0) "${where}: no fault in the log"
}
function Lint([string]$tag) {
    $sim = (Send-FL 'hud2 state') -split "`n" | Select-Object -First 1 # (for the record: a refused sim keeps the last one, which silently proved nothing in p2s)
    $null = Send-FL 'hud2 lint'; $null = Wait-FLFramesPassed 20
    $r = Send-FL 'hud2 lint'
    Set-Content (Join-Path $dir "lint_$tag.txt") $r
    Check ($r -notmatch 'verdict FAIL') "lint ${tag}: $(($r -split "`n" | Where-Object { $_ -match '^verdict' }) -join ' ')"
}
# the grab's frames once the game has finished writing them: its times.txt comes last, and stands locked until then
function Take-Grab([string]$to) {
    $from = Join-Path (Get-FLInstallDir) 'EXE\grab'; $times = Join-Path $from 'times.txt'
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 60) {
        try { if (Test-Path $times) { [IO.File]::Open($times, 'Open', 'Read', 'None').Dispose(); break } } catch { }
        Start-Sleep -Milliseconds 500
    }
    Copy-Item $from $to -Recurse -Force
    [IO.Directory]::Delete($from, $true) # (the next take must wait for its own times.txt)
}
function Tag([string]$size) { $size -replace '[ ,]+', 'x' }
$sims = 'alertlist', 'alertlistpad', 'combat', 'travel', 'cruise', 'dock', 'lane', 'ap', 'formation', 'torpedo', 'critical', 'hazard', 'groups', 'notarget', 'offscreen'

# the sizes: hud_size at each screen, every sim through the lint, pad and keyboard; the machine's answer in the log
function Section-sizes {
    foreach ($size in $Sizes) {
        Start-Game $size 'SaveC702.fl'
        $log = Join-Path (Get-FLInstallDir) 'EXE\FLController.log'
        Result 'LOOK' "$(Tag $size) $((Select-String -Path $log -Pattern 'hud_size:' | Select-Object -Last 1).Line)"
        foreach ($s in 1, 2, 3) {
            $null = Send-FL "set hud_size $s"; $null = Wait-FLFramesPassed 20
            foreach ($sim in $sims) {
                $reply = Send-FL "hud2 sim $sim"
                if ($reply -match 'unknown') { Result 'FAIL' "the sim $sim was refused: $reply"; continue }
                $null = Wait-FLFramesPassed 30
                Lint "$(Tag $size)_size${s}_$sim"
            }
            foreach ($sim in 'combat', 'alertlist', 'alertlistpad', 'critical', 'cruise') {
                $null = Send-FL "hud2 sim $sim"; $null = Wait-FLFramesPassed 30
                Shot "$(Tag $size)_size${s}_$sim"
            }
        }
        $null = Send-FL 'hud2 sim off'
        NoFault "sizes $(Tag $size)"
        Stop-Game
    }
}

# the first frame: from the menu into space, the first frames grabbed; flash_frames.py finds the native HUD in them
function Section-first {
    $size = $Sizes[0]
    Start-Game $size 'SaveC701.fl' # (docked: an in-game load into space flashed as a load from the menu did: 12a row 10)
    foreach ($n in 1..3) {
        $null = Send-FLS 'grab 240 0 0 1920 200 1' # (a region: a full frame during a load killed one game)
        $null = Load-FLSave 'SaveC702.fl'
        $null = Wait-FLFramesPassed 120
        Take-Grab (Join-Path $dir "first_$n")
        $null = Load-FLSave 'SaveC701.fl'
    }
    Result 'LOOK' 'first_*: tests\hud040\frames.py native <folder> names any frame with the native HUD'
    NoFault 'first'
    Stop-Game
}

# the alerts: the zone that shows them; A1 the alert stays seen through its blink while hidden, A2 the art steady at the line and gone under windows
function Section-alerts {
    foreach ($size in $Sizes) {
        Start-Game $size (Join-Path $root 'Install\instances\3\Freelancer HD Edition\SAVE\Accts\SinglePlayer\SaveG004.fl') # (the radiation zone: made by the gate's probes, kept in that instance)
        # (this save's fight kills the ship in about 40 s: the frames and A3 need it alive. The flag is read at a load: p2i died with it set)
        $null = Send-FL 'godmode on'
        $null = Load-FLSave 'SaveG004.fl'; $null = Wait-FLFramesPassed 200
        $seen = 0; $n = 0; $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt 8) { $n++; if ((Send-FL 'hud2 state') -match 'radiation=1') { $seen++ }; Start-Sleep -Milliseconds 100 }
        Check ($n -gt 0 -and $seen / $n -ge 0.9) "A1 radiation held through its blink: $seen of $n samples"
        $st = Send-FL 'hud2 state'
        Check ($st -match "art='shown'") "A2 the game's art stands at the line: $(($st -split "`n" | Where-Object { $_ -match '^alerts' }) -join ' ')"
        Shot "$(Tag $size)_radiation_flight"
        # (what each tap really opened goes to a file: USER_CANCEL and USER_INVENTORY left plain flight in the frames of p2d)
        $log = Join-Path $dir "windows_$(Tag $size).txt"
        foreach ($w in 'USER_PAUSE', 'USER_NAV_MAP', 'USER_INVENTORY') {
            # (a tap during the last window's closing is lost: p2j's inventory. The tries are printed: more than one is worth a look)
            $open = $false; $tries = 0
            while (-not $open -and $tries -lt 3) {
                $tries++
                $reply = Send-FL "tap $w"
                $sw = [Diagnostics.Stopwatch]::StartNew()
                while (-not $open -and $sw.Elapsed.TotalSeconds -lt 3) { $open = (Send-FL 'state') -match 'mode=space-ui'; Start-Sleep -Milliseconds 100 }
            }
            Check $open "$w opened, tap $tries of 3"
            if (-not $open) { continue }
            $null = Wait-FLFramesPassed 120
            Add-Content $log "--- $w`: $reply`n$(Send-FL 'state')`n$((Send-FL 'hud2 state') -split "`n" | Where-Object { $_ -match '^alerts' })"
            Shot "$(Tag $size)_radiation_$w"
            $null = Send-FL "tap $w"
            $null = Wait-FLUntil { if ((Send-FL 'state') -match 'mode=space ') { 1 } } "$w closed" 10
            $null = Wait-FLFramesPassed 60
        }
        # A3: the line and the art end with the zone (the review's latch: the art's forced show kept the alert's word true)
        $null = Move-FLShipNear 'Planet Manhattan' 2000
        # (the game samples the zones on its own timer, so the flag can trail the 10 Hz place line: p2h's 4K frame)
        $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt 6 -and (Send-FL 'hud2 state') -notmatch 'radiation=0') { Start-Sleep -Milliseconds 100 }
        "A3 the line ended {0:0.0} s after the ship arrived" -f $sw.Elapsed.TotalSeconds
        $null = Wait-FLFramesPassed 30
        $st = Send-FL 'hud2 state'
        Check ($st -match 'radiation=0' -and $st -match "art='not asked'") "A3 out of the zone, the line and the art are gone: $(($st -split "`n" | Where-Object { $_ -match '^alerts' }) -join ' ')"
        Shot "$(Tag $size)_radiation_left"
        Set-Content (Join-Path $dir "roots_$(Tag $size).txt") (Send-FL 'hud2 tree roots')
        Result 'LOOK' "roots_$(Tag $size).txt: each line reading unknown wants a row in hud2_roots.h"
        NoFault "alerts $(Tag $size)"
        Stop-Game
    }
}

# T1, T2, T4, T5: the form against the game's bracket on the same frame; the records against the projection
function Section-form {
    foreach ($size in $Sizes) {
        Start-Game $size 'SaveC702.fl'
        $null = Send-FL 'spawn fc_lr_pi_fighter_d1 player 3 900'; $null = Wait-FLFramesPassed 200
        $null = Send-FL 'tap USER_NEXT_ENEMY'; $null = Wait-FLFramesPassed 60
        # (0x4EF030 draws an ON-SCREEN target only, so the form starts at the first frame one is on screen: p2f's stood off screen)
        $null = Send-FL 'ap start smart'
        $null = Wait-FLUntil { if ((Send-FL 'hud2 form') -match 'standing') { 1 } } 'the target on screen' 30
        $null = Send-FL 'ap stop'
        $state = Send-FL 'hud2 form'
        Check ($state -match 'standing') "T1 the form stands: $state"
        Set-Content (Join-Path $dir "tree_radiation_$(Tag $size).txt") (Send-FL 'hud2 tree radiation') # (the alert's fields: A2)
        Shot "$(Tag $size)_form_card"
        $null = Wait-FLFramesPassed 200
        Shot "$(Tag $size)_form_settled"
        $null = Send-FL 'hud2 form off'; $null = Wait-FLFramesPassed 30
        Shot "$(Tag $size)_form_native"
        $null = Send-FL 'hud2 form on'; $null = Wait-FLFramesPassed 30
        $null = Send-FL 'ap start smart'
        $null = Wait-FLUntil { if ((Send-FL 'hud2 marks') -match 'target proj=\(\d{1,4},\d{1,4}\) rec=\([1-9]') { 1 } } 'the target on screen for T4' 30
        $marks = Send-FL 'hud2 marks'
        $null = Send-FL 'ap stop'
        Set-Content (Join-Path $dir "marks_$(Tag $size).txt") $marks
        $m = [regex]::Match($marks, 'target proj=\((-?\d+),(-?\d+)\) rec=\((-?\d+),(-?\d+)\)')
        Result 'LOOK' "one sample under the autopilot's turn (T4 itself: motion): $($m.Value)"
        # T2's return: the bracket's art back after a load and a launch
        $null = Send-FL 'hud2 form off'; $null = Wait-FLFramesPassed 30
        $null = Load-FLSave 'SaveC702.fl'; $null = Wait-FLFramesPassed 200
        $null = Send-FL 'spawn fc_lr_pi_fighter_d1 player 1 900'; $null = Wait-FLFramesPassed 200
        $null = Send-FL 'tap USER_NEXT_ENEMY'; $null = Wait-FLFramesPassed 60
        Shot "$(Tag $size)_form_native_after_load"
        NoFault "form $(Tag $size)"
        Stop-Game
    }
}

# the clip on real motion, T3 and T8: a fight flown by the autopilot, frames at the game's rate, for the side-change and edge rules
function Section-motion {
    # (the radiation save: its fight has missiles, so an alert card meets the form; godmode is read at a load)
    Start-Game $Sizes[0] (Join-Path $root 'Install\instances\3\Freelancer HD Edition\SAVE\Accts\SinglePlayer\SaveG004.fl')
    $null = Send-FL 'godmode on'; $null = Load-FLSave 'SaveG004.fl'; $null = Wait-FLFramesPassed 200
    $null = Send-FL 'tap USER_NEXT_ENEMY'; $null = Wait-FLFramesPassed 30
    $null = Send-FL 'ap start smart'
    $null = Wait-FLFramesPassed 120
    # (step 6 = every 6th PIXEL, consecutive game frames: 640x360, all 600 under the grab's 400 MB, about 10 s, long enough for a side flip. A full frame during a load killed one game; never during a load)
    $null = Send-FLS 'grab 600 0 0 3840 2160 6'
    $null = Wait-FLFramesPassed 700
    Take-Grab (Join-Path $dir 'motion')
    # T8: the seat's changes over a longer fight, from the log: no side flip inside kSideHoldSec, no flicker at the seat's edge
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 60) { Start-Sleep -Milliseconds 500 }
    $log = Join-Path (Get-FLInstallDir) 'EXE\FLController.log'
    $seats = @(Select-String -Path $log -Pattern 'hud2 form: seat' | ForEach-Object { $_.Line })
    Set-Content (Join-Path $dir 'seat_changes.txt') $seats
    $times = @($seats | ForEach-Object { if ($_ -match '^\[(\d+):(\d+):([\d.]+)\]') { 3600 * [int]$Matches[1] + 60 * [int]$Matches[2] + [double]$Matches[3] } })
    $close = 0; for ($i = 1; $i -lt $times.Count; $i++) { if ($times[$i] - $times[$i - 1] -lt 0.28) { $close++ } }
    Check ($times.Count -gt 0 -and $close -eq 0) "T8 the seat: $($times.Count) changes, $close of them within 0.3 s of the last"
    $null = Send-FL 'ap stop'
    # T4 on a ship that has stopped turning: one sample under a turn read 18 px at 4K (p2k). The median of ten, against 0.2 % of the width
    $null = Wait-FLFramesPassed 120
    $d = foreach ($i in 1..10) {
        $m = [regex]::Match((Send-FL 'hud2 marks'), 'target proj=\((\d{1,4}),(\d{1,4})\) rec=\(([1-9]\d*),(\d+)\)')
        if ($m.Success) { [Math]::Max([Math]::Abs([int]$m.Groups[1].Value - [int]$m.Groups[3].Value), [Math]::Abs([int]$m.Groups[2].Value - [int]$m.Groups[4].Value)) }
        $null = Wait-FLFramesPassed 10
    }
    $d = @($d | Sort-Object)
    $limit = 0.002 * [int](((Get-FLDisplaySize) -split '[ ,]+')[0])
    if ($d.Count -ge 5) { Check ($d[[int]($d.Count / 2)] -le $limit) "T4 records against the projection: median $($d[[int]($d.Count / 2)]) px of $($d.Count) samples, limit $limit, all: $($d -join ' ')" }
    else { Result 'LOOK' "T4 not judged: the target on screen in $($d.Count) of 10 samples" }
    Result 'LOOK' 'motion\: run-judge reads the seat, the side change, the edge; T8 from the log of seat changes'
    NoFault 'motion'
    Stop-Game
}

# the flash test: the hull at its worst, frames at the game's rate, the rate measured from the pixels
function Section-flash {
    Start-Game $Sizes[0] 'SaveC702.fl'
    $null = Send-FL 'hud2 sim critical'; $null = Wait-FLFramesPassed 60
    $s = (Get-FLDisplaySize) -split '[ ,]+'
    $w = [int]$s[0]; $h = [int]$s[1]
    $null = Send-FLS ("grab 480 {0} {1} {2} {3} 1" -f [int]($w / 2 + 0.05 * $h), [int](0.42 * $h), [int](0.08 * $h), [int](0.16 * $h))
    $null = Wait-FLFramesPassed 560
    $g = Join-Path $dir 'flash'
    Take-Grab $g
    $py = Join-Path $root '..\FLvibed\research\tools\ocr\.venv\Scripts\python.exe'
    $r = & $py -I (Join-Path $PSScriptRoot 'frames.py') rate $g
    $r
    Check ($LASTEXITCODE -eq 0) 'the flash test: every frequency above 3 Hz under the limit'
    $null = Send-FL 'set skin_motion 2'; $null = Wait-FLFramesPassed 30; Shot 'flash_motion_none'; $null = Send-FL 'set skin_motion 0'
    $null = Send-FL 'set hull_cue 0'; $null = Send-FL 'set shield_cue 0'; $null = Wait-FLFramesPassed 30; Shot 'flash_cues_off'
    $null = Send-FL 'hud2 sim off'
    NoFault 'flash'
    Stop-Game
}

# the cue: states on the row at both sizes; a real lock is sought with a seeker in a fight
function Section-cue {
    foreach ($size in $Sizes) {
        Start-Game $size 'SaveC702.fl'
        $null = Send-FL 'hud2 sim groups'; $null = Wait-FLFramesPassed 30
        Shot "$(Tag $size)_cue_states"
        Lint "$(Tag $size)_cue"
        $null = Send-FL 'hud2 sim off'
        NoFault "cue $(Tag $size)"
        Stop-Game
    }
}

# a real lock: SaveC702's ship has no launcher, so no lock could come (p2l: 0 samples). The radiation save's Barracuda mounts missiles and starts among hostiles
function Section-lock {
    Start-Game $Sizes[0] (Join-Path $root 'Install\instances\3\Freelancer HD Edition\SAVE\Accts\SinglePlayer\SaveG004.fl')
    $null = Send-FL 'godmode on'; $null = Load-FLSave 'SaveG004.fl'; $null = Wait-FLFramesPassed 200
    $null = Send-FL 'tap USER_NEXT_ENEMY'; $null = Wait-FLFramesPassed 30
    $null = Send-FL 'ap start smart'
    $locks = 0; $n = 0; $shot = $false; $best = -9; $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 40) {
        $n++
        $st = Send-FL 'hud2 state'
        $v = [regex]::Match($st, ' lock=(-?[\d.]+)'); if ($v.Success) { $best = [Math]::Max($best, [double]$v.Groups[1].Value) }
        if ($n % 16 -eq 1) { Add-Content (Join-Path $dir 'lock_samples.txt') "--- $n`n$(($st -split "`n" | Where-Object { $_ -match 'locked=' }) -join ' ')`n$(Send-FL 'weapons')" }
        if ($st -match 'locked=1') { $locks++; if (-not $shot) { Shot 'lock_held'; $shot = $true } }
        Start-Sleep -Milliseconds 250
    }
    Result 'LOOK' "the lock clock's best: $best"
    $null = Send-FL 'ap stop'
    Result $(if ($locks) { 'LOOK' } else { 'FAIL' }) "a real lock: $locks of $n samples"
    NoFault 'lock'
    Stop-Game
}

# the review's two dialog fixes: CONTINUE under a dialog takes no click, Enter answers the dialog, and with no dialog the chip still works (the half that catches a broken gate)
function Section-dialogs {
    foreach ($size in $Sizes) {
        Start-Game $size ''
        $null = Wait-FLUntil { if ((Send-FL 'uiskin state') -match 'chip=1') { 1 } } 'the CONTINUE chip' 30
        $st = Send-FL 'uiskin state'
        $m = [regex]::Match($st, 'rect=(-?[\d.]+),(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)')
        if (-not $m.Success) { Result 'FAIL' "no chip rectangle: $st"; Stop-Game; continue }
        $cx = ([double]$m.Groups[1].Value + [double]$m.Groups[3].Value) / 2; $cy = ([double]$m.Groups[2].Value + [double]$m.Groups[4].Value) / 2
        Shot "$(Tag $size)_menu"
        $null = Send-FL 'menu exit'; $null = Wait-FLFramesPassed 120
        Shot "$(Tag $size)_dialog_exit"
        $null = Send-FL "mouse $cx $cy"; $null = Wait-FLFramesPassed 20
        $null = Send-FL 'mbtn 0 1'; $null = Wait-FLFramesPassed 6; $null = Send-FL 'mbtn 0 0'; $null = Wait-FLFramesPassed 60
        $st = Send-FL 'uiskin state'
        Check ($st -match 'wait=-1') "D1 CONTINUE takes no click under the dialog: $([regex]::Match($st, 'skinmenu[^\n]*').Value)"
        Shot "$(Tag $size)_dialog_after_click"
        $null = Send-FL 'post 27'; $null = Wait-FLFramesPassed 90 # (Esc: the safe answer)
        Shot "$(Tag $size)_dialog_after_esc"
        Check ($null -ne (Get-FLGameProcess)) 'D2 Esc took the safe answer'
        # D3 Enter: this routine confirm focuses its action (SKIN 6.2), so Enter exits the game: last, and it is the pass
        $null = Send-FL 'mouse release'
        $null = Send-FL 'menu exit'; $null = Wait-FLFramesPassed 120
        try { $null = Send-FL 'post 13' } catch { }
        $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt 10 -and (Get-FLGameProcess)) { Start-Sleep -Milliseconds 200 }
        Check ($null -eq (Get-FLGameProcess)) 'D3 Enter takes the focused answer, here EXIT GAME'
        # D4 the gate's other half: a fresh game, no dialog, the same click on the chip loads
        Start-Game $size ''
        $null = Wait-FLUntil { if ((Send-FL 'uiskin state') -match 'chip=1') { 1 } } 'the CONTINUE chip' 30
        $null = Send-FL "mouse $cx $cy"; $null = Wait-FLFramesPassed 20
        $null = Send-FL 'mbtn 0 1'; $null = Wait-FLFramesPassed 6; $null = Send-FL 'mbtn 0 0'
        $moved = $false; $sw = [Diagnostics.Stopwatch]::StartNew()
        while (-not $moved -and $sw.Elapsed.TotalSeconds -lt 30) { $moved = (Send-FL 'state') -notmatch 'mode=menu'; Start-Sleep -Milliseconds 200 }
        Check $moved "D4 with no dialog the same click loads: $(((Send-FL 'state') -split "`n")[0])"
        $null = Send-FL 'mouse release'
        NoFault "dialogs $(Tag $size)"
        Stop-Game
    }
}

# issue 35 for real: a pad, 1280x800, the list open, a missile from the fight; the FLARE row read from the frame
function Section-padmissile {
    Start-Game '1280, 800' (Join-Path $root 'Install\instances\3\Freelancer HD Edition\SAVE\Accts\SinglePlayer\SaveG004.fl')
    $null = Send-FL 'godmode on'; $null = Load-FLSave 'SaveG004.fl'; $null = Wait-FLFramesPassed 200
    # (a background game: the pad is the device in use only by `input_mode 1`, HARNESS.md)
    $null = Send-FL 'set input_mode 1'; Enable-FLDevPads -Foreground; Set-FLDevPad -Slot 0 -Vendor xbox; $null = Wait-FLFramesPassed 60
    $st = Send-FL 'hud2 state'
    Check ($st -match 'kbm=0') "the pad's form: $([regex]::Match($st, 'kbm=\d').Value)"
    if ($st -notmatch 'kbm=0') { Result 'FAIL' 'not on the pad: the frames below do not count'; Stop-Game; return }
    $null = Send-FL 'tap USER_NEXT_ENEMY'; $null = Wait-FLFramesPassed 30
    $null = Send-FL 'hud2 list on'; $null = Wait-FLFramesPassed 30
    $got = @{}; $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt 90 -and $got.Count -lt 3) {
        $st = Send-FL 'hud2 state'
        foreach ($w in 'MISSILE', 'SHIELD', 'HULL') {
            if ($st -match "alert='$w" -and -not $got[$w]) {
                $got[$w] = 1; Shot "padmissile_$w"; Lint "padmissile_$w"
                Add-Content (Join-Path $dir 'padmissile_state.txt') "--- $w`n$st"
            }
        }
        Start-Sleep -Milliseconds 100
    }
    Check ($got['MISSILE']) "a real missile's card on the pad: seen $(@($got.Keys) -join ', ')"
    $null = Send-FL 'hud2 sim critical'; $null = Wait-FLFramesPassed 30
    Shot 'padmissile_critical_sim'; Lint 'padmissile_critical_sim'
    $null = Send-FL 'hud2 sim off'
    NoFault 'padmissile'
    Stop-Game
}

# the screens: front end, windows, the options page, at both sizes
function Section-menus {
    foreach ($size in $Sizes) {
        Start-Game $size 'SaveC701.fl'
        $null = Send-FL 'ui options open'; $null = Wait-FLFramesPassed 150
        foreach ($t in 0..4) { $null = Send-FL "ui tab $t"; $null = Wait-FLFramesPassed 30; Shot "$(Tag $size)_options_tab$t" }
        $null = Send-FL 'ui options close'; $null = Wait-FLFramesPassed 60
        Set-Content (Join-Path $dir "seams_$(Tag $size).txt") (Send-FL 'uiskin seams')
        foreach ($w in 'USER_NAV_MAP', 'USER_INVENTORY', 'USER_PLAYER_STATS') {
            $null = Send-FL "tap $w"; $null = Wait-FLFramesPassed 120
            Shot "$(Tag $size)_$w"
            $s = Send-FL 'uiskin seams'
            Add-Content (Join-Path $dir "seams_$(Tag $size).txt") "--- $w`n$s"
            $null = Send-FL "tap $w"; $null = Wait-FLFramesPassed 120
        }
        Result 'LOOK' "seams_$(Tag $size).txt: every line is a native control left unclaimed"
        NoFault "menus $(Tag $size)"
        Stop-Game
    }
}

# the two things the driver saw at a skipped dock (Baltimore, 4K): is the green place line typing out or cut, and does a space comm outlive the dock in the base
function Section-dock {
    Start-Game $Sizes[0] 'SaveC702.fl'
    $null = Set-FLTarget 'Baltimore Shipyard'
    $null = Send-FL 'set skip_dock_scene 1'
    $null = Start-FLAutopilot -Mode dock
    # the voice across the step into the base: the comm player's own flag (`commplaying`, +0x460), sampled through the step and 8 s after it
    $vl = Join-Path $dir 'dock_voice.txt'; $sw = [Diagnostics.Stopwatch]::StartNew(); $seen = $null; $last = ''
    while ($sw.Elapsed.TotalSeconds -lt 180) {
        $line = "$(Send-FL 'commplaying') | $(((Send-FL 'state') -split "`n")[0] -replace ' frame=.*','')"
        if ($line -ne $last) { Add-Content $vl ("{0:0.00} {1}" -f $sw.Elapsed.TotalSeconds, $line); $last = $line }
        if ($line -match 'mode=docked' -and $null -eq $seen) { $seen = $sw.Elapsed.TotalSeconds; Shot 'dock_step' }
        if ($null -ne $seen -and $sw.Elapsed.TotalSeconds - $seen -gt 8) { break }
        Start-Sleep -Milliseconds 40
    }
    if ($null -eq $seen) { throw 'docked not reached after 180 s' }
    $null = Wait-FLUntil { if ((Send-FL 'state') -match 'mode=docked') { 1 } } 'docked' 5
    foreach ($s in 1..8) { Shot "dock_after_$s"; Start-Sleep -Seconds 1 }
    Set-Content (Join-Path $dir 'dock_subtitles.txt') (Send-FLS 'shown 0')
    Result 'LOOK' 'dock_after_*: the green line grows to its whole text and goes, or stands cut; the comm subtitle: how long it stands in the base'
    NoFault 'dock'
    Stop-Game
}

# the captures for the Feature Tour and the owner's sheets
function Section-captures {
    foreach ($size in $Sizes) {
        Start-Game $size 'SaveC702.fl'
        foreach ($sim in 'combat', 'travel', 'cruise', 'hazard', 'groups', 'critical') {
            $null = Send-FL "hud2 sim $sim"; $null = Wait-FLFramesPassed 40
            Shot "$(Tag $size)_$sim"
        }
        $null = Send-FL 'hud2 sim off'
        Stop-Game
    }
}

# ---- the frames phase 2 still owed (driver, 6 Nov): each a LOOK, read by a person. The target's place by `hud2 marks`; no verb places a projection
function Mark-Xy { $m = [regex]::Match("$(Send-FL 'hud2 marks')", 'target proj=\((-?\d+),(-?\d+)\)'); if ($m.Success) { [pscustomobject]@{ X = [int]$m.Groups[1].Value; Y = [int]$m.Groups[2].Value } } }
function Power { $m = [regex]::Match("$(Send-FL 'hud2 state')", ' power=([\d.e-]+)'); if ($m.Success) { [double]$m.Groups[1].Value } else { -1 } }
# the rest of the owed frames, each by a route of its own: any object under a panel, every contact's place written down; the meter by held guns; the name by reloads, as a wing's
# names are drawn at each load and the prefix "Escort n" takes the room
function Section-owed2 {
    foreach ($size in $Sizes) {
        $tag = Tag $size
        $w = [int]($size -split ',')[0]; $h = [int]($size -split ',')[1]
        Start-Game $size 'SaveC702.fl'
        # (a real fight stands the ship panel at the bottom left: under a sim the marks have no place to read)
        $null = Send-FL 'godmode on'; $null = Load-FLSave 'SaveC702.fl'; $null = Wait-FLFramesPassed 200
        $null = Send-FL 'spawn fc_lr_pi_fighter_d1 player 3 2500'; $null = Wait-FLFramesPassed 200
        $hit = $null; $free = $null; $seen = @()
        foreach ($c in @(Get-FLContacts -All | Sort-Object Distance | Select-Object -First 80)) {
            $null = Send-FL "settarget $($c.Id)"; $null = Wait-FLFramesPassed 10
            $p = Mark-Xy
            $seen += "$($c.Id) $($c.Type) $($c.Name) $(if ($p) { "$($p.X),$($p.Y)" } else { 'none' })"
            if (-not $p -or $p.X -le 0 -or $p.Y -le 0 -or $p.X -ge $w -or $p.Y -ge $h) { continue }
            if ($p.X -lt 0.2 * $w -and $p.Y -gt 0.72 * $h -or $p.X -gt 0.7 * $w -and ($p.Y -gt 0.72 * $h -or $p.Y -lt 0.27 * $h)) { if (-not $hit) { $hit = $c } }
            elseif (-not $free) { $free = $c }
            if ($hit -and $free) { break }
        }
        Set-Content (Join-Path $dir "${tag}_contacts_seen.txt") $seen
        if ($hit -and $free) {
            Result 'LOOK' "under a panel: $($hit.Name)"
            $null = Send-FL "settarget $($free.Id)"; $null = Wait-FLFramesPassed 60
            $null = Send-FLS "grab 40 0 0 $w $h 4"
            $null = Send-FL "settarget $($hit.Id)"
            $null = Wait-FLFramesPassed 80
            Take-Grab (Join-Path $dir "${tag}_block_open")
            Shot "${tag}_block_open_after"
        } else { Result 'LOOK' "NOT SHOWN: nothing under a panel ($tag): ${tag}_contacts_seen.txt" }
        Stop-Game
        Start-Game $size (Join-Path $root 'Install\instances\3\Freelancer HD Edition\SAVE\Accts\SinglePlayer\SaveG004.fl')
        $null = Send-FL 'godmode on'; $null = Load-FLSave 'SaveG004.fl'; $null = Wait-FLFramesPassed 200
        Set-Content (Join-Path $dir "${tag}_repnames.txt") (Send-FL 'repnames')
        $found = $false; $longest = ''
        foreach ($round in 1..25) {
            $null = Load-FLSave 'SaveG004.fl'; $null = Wait-FLFramesPassed 120
            Add-Content (Join-Path $dir "${tag}_repnames.txt") (Send-FL 'repnames')
            $names = @(Get-FLContacts -All | Where-Object { $_.Ship } | Sort-Object { $_.Name.Length } -Descending)
            if ($names.Count -and $names[0].Name.Length -gt $longest.Length) { $longest = $names[0].Name }
            if ($names.Count -and $names[0].Name.Length -ge 24) {
                $null = Send-FL "settarget $($names[0].Id)"; $null = Wait-FLFramesPassed 90
                Result 'LOOK' "a name of $($names[0].Name.Length): '$($names[0].Name)' (round $round)"
                Shot "${tag}_long_name"
                Set-Content (Join-Path $dir "${tag}_long_name.txt") (Send-FL 'hud2 state')
                $found = $true; break
            }
        }
        if (-not $found) { Result 'LOOK' "NOT SHOWN: no name past the cut in 25 loads ($tag); the longest '$longest'" }
        NoFault "owed2 $tag"
        Stop-Game
    }
}

# the meter at three levels by `hud2 sim`: held guns did not move the game's energy in a background game
function Section-owedmeter {
    foreach ($size in $Sizes) {
        Start-Game $size 'SaveC702.fl'
        foreach ($s in 'travel', 'combat', 'lowenergy') {
            $reply = Send-FL "hud2 sim $s"; $null = Wait-FLFramesPassed 40
            Check ("$reply" -notmatch 'unknown|refus') "the sim $s stands: $reply"
            Shot "$(Tag $size)_meter_$s"
        }
        $null = Send-FL 'hud2 sim off'
        Stop-Game
    }
}

# the Controller tab with a pad whose triggers can be driven: `trigger_effects` shows only then. A virtual pad through SDL: no real pad is opened
function Section-owedpad {
    foreach ($size in $Sizes) {
        Start-Game $size 'SaveC702.fl'
        $null = Send-FL 'set pad_backend 1'
        $null = Wait-FLUntil { if ((Send-FL 'state') -match 'sdl_status=2') { 1 } } 'SDL running' 15
        Write-Host "        $(Connect-FLSdlPad -Type ps5 -Foreground)"
        $null = Wait-FLFramesPassed 60
        $null = Send-FL 'ui options open'; $null = Wait-FLFramesPassed 150
        $null = Send-FL 'ui tab 1'; $null = Wait-FLFramesPassed 40
        Shot "$(Tag $size)_options_pad_tab1"
        $rows = "$(Send-FL 'ui rows')"
        Set-Content (Join-Path $dir "$(Tag $size)_options_pad_rows.txt") $rows
        Check ($rows -match 'trigger_effects') "the row stands with the pad: $(($rows -split "`n" | Where-Object { $_ -match 'trigger_effects' }) -join ' ')"
        $null = Send-FL 'ui focus trigger_effects'; $null = Wait-FLFramesPassed 30
        Shot "$(Tag $size)_options_pad_focus"
        $null = Send-FL 'ui options close'
        try { $null = Disconnect-FLSdlPad } catch { Write-Host "        (detach: $($_.Exception.Message))" }
        NoFault "owedpad $(Tag $size)"
        Stop-Game
    }
}

# two alert cards: the second comes in while the first stands: consecutive frames at full motion, stills at the other two
function Section-owedcards {
    foreach ($size in $Sizes) {
        $tag = Tag $size
        $w = [int]($size -split ',')[0]; $h = [int]($size -split ',')[1]
        Start-Game $size 'SaveC702.fl'
        $null = Send-FL 'godmode on'; $null = Load-FLSave 'SaveC702.fl'; $null = Wait-FLFramesPassed 200
        $null = Send-FL 'hud2 alert missile'; $null = Wait-FLFramesPassed 40
        $null = Send-FLS "grab 30 0 0 $w $h 3"
        $null = Send-FL 'hud2 alert hull'
        $null = Wait-FLFramesPassed 60
        Take-Grab (Join-Path $dir "${tag}_cards_full")
        Shot "${tag}_cards_full_after"
        $null = Wait-FLFramesPassed 400
        foreach ($m in 1, 2) {
            $null = Send-FL "set skin_motion $m"; $null = Wait-FLFramesPassed 20
            $null = Send-FL 'hud2 alert missile'; $null = Wait-FLFramesPassed 40
            $null = Send-FL 'hud2 alert hull'; $null = Wait-FLFramesPassed 3
            Shot "${tag}_cards_motion$m"
            $null = Wait-FLFramesPassed 400
        }
        $null = Send-FL 'set skin_motion 0'
        NoFault "owedcards $tag"
        Stop-Game
    }
}

# the seat with a real ship near the centre, a pirate and a nomad (the owner's peek, 6 Nov: at the reticle almost nothing showed): the form's numbers and a frame near the middle
function Section-owedseat {
    foreach ($size in $Sizes) {
        $tag = Tag $size
        $w = [int]($size -split ',')[0]; $h = [int]($size -split ',')[1]
        Start-Game $size 'SaveC702.fl'
        $null = Send-FL 'godmode on'; $null = Load-FLSave 'SaveC702.fl'; $null = Wait-FLFramesPassed 200
        foreach ($arch in 'MSN13_Nomad_Fighter_Populator', 'MSN13_Nomad_Fighter_Populator') {
            # (the second pass as the loot test flies: `hud2 on` forces the HUD, state force=1: the frames the owner saw)
            if ($forced) { $null = Send-FL 'hud2 on'; $null = Wait-FLFramesPassed 30 }
            $forced = $true
            $null = Send-FL 'spawn clear'; $null = Wait-FLFramesPassed 30
            Result 'LOOK' "$arch`: $(Send-FL "spawn $arch player 1 600")"
            $null = Wait-FLFramesPassed 120
            $null = Send-FL 'tap USER_NEXT_ENEMY'; $null = Wait-FLFramesPassed 30
            $null = Send-FL 'hud2 alert tractor 5'   # (a wide card, gone by the time the frames are taken: it must leave nothing behind)
            $null = Send-FL 'ap start smart'
            $n = 0; $sw = [Diagnostics.Stopwatch]::StartNew()
            while ($sw.Elapsed.TotalSeconds -lt 30 -and $n -lt 3) {
                $p = Mark-Xy
                if ($p -and [Math]::Abs($p.X - $w / 2) -lt 0.1 * $h -and [Math]::Abs($p.Y - $h / 2) -lt 0.1 * $h) {
                    $n++
                    $f = "$(Send-FL 'hud2 form')"
                    Result 'LOOK' "$arch $n at $($p.X),$($p.Y): $f"
                    Shot "${tag}_seat_$(if ($forced2) { 'forced' } else { 'plain' })_$n"
                    Start-Sleep -Milliseconds 800
                } else { Start-Sleep -Milliseconds 100 }
            }
            $null = Send-FL 'ap stop'
            if (-not $n) { Result 'LOOK' "NOT SHOWN: $arch never near the middle: $(Send-FL 'hud2 form')" }
            $forced2 = $true
        }
        NoFault "owedseat $tag"
        Stop-Game
    }
}

function Section-owedform {
    foreach ($size in $Sizes) {
        $tag = Tag $size
        $w = [int]($size -split ',')[0]; $h = [int]($size -split ',')[1]
        Start-Game $size 'SaveC702.fl'
        $null = Send-FL 'godmode on'; $null = Load-FLSave 'SaveC702.fl'; $null = Wait-FLFramesPassed 200
        # a block opening on a panel: a still object under one (the sim stands the ship panel at the bottom left), reached by a target change
        $null = Send-FL 'hud2 sim combat'; $null = Wait-FLFramesPassed 60
        $hit = $null; $free = $null
        foreach ($c in @(Get-FLContacts -All | Where-Object { -not $_.Ship } | Sort-Object Distance | Select-Object -First 60)) {
            $null = Send-FL "settarget $($c.Id)"; $null = Wait-FLFramesPassed 12
            $p = Mark-Xy
            if (-not $p -or $p.X -le 0 -or $p.Y -le 0 -or $p.X -ge $w -or $p.Y -ge $h) { continue }
            if (($p.X -lt 0.17 * $w -and $p.Y -gt 0.8 * $h) -or ($p.X -gt 0.72 * $w -and ($p.Y -gt 0.8 * $h -or $p.Y -lt 0.25 * $h))) { if (-not $hit) { $hit = $c; Result 'LOOK' "under a panel: $($c.Name) at $($p.X),$($p.Y)" } }
            elseif (-not $free) { $free = $c }
            if ($hit -and $free) { break }
        }
        if ($hit -and $free) {
            $null = Send-FL "settarget $($free.Id)"; $null = Wait-FLFramesPassed 60
            $null = Send-FLS "grab 40 0 0 $w $h 4"
            $null = Send-FL "settarget $($hit.Id)"
            $null = Wait-FLFramesPassed 80
            Take-Grab (Join-Path $dir "${tag}_block_open")
            Set-Content (Join-Path $dir "${tag}_block_open.txt") (Send-FL 'hud2 form')
            Shot "${tag}_block_open_after"
        } else { Result 'LOOK' "NOT SHOWN: no still object under a panel with another free ($tag)" }
        $null = Send-FL 'hud2 sim off'; $null = Wait-FLFramesPassed 30
        # the mini card, a target change, FADES, NONE
        $null = Send-FL 'spawn fc_lr_pi_fighter_d1 player 3 900'; $null = Wait-FLFramesPassed 200
        $null = Send-FL 'ap start smart'   # (it turns the nose on the fight; `settarget` then picks a ship on screen, none in the card's way)
        $null = Wait-FLFramesPassed 300
        $null = Send-FL 'ap stop'
        # a target change onto a ship on screen, the card still to run: the mark must lie inside the screen. Its id, so a change that did nothing shows
        function Target-OnScreen([uint32]$not) {
            foreach ($try in 1..30) {
                foreach ($c in @(Get-FLContacts -All | Where-Object { $_.Ship -and $_.Attitude -lt 0 -and $_.Id -ne $not } | Sort-Object Distance)) {
                    $null = Send-FL "settarget $($c.Id)"
                    $null = Wait-FLFramesPassed 3
                    $p = Mark-Xy
                    if ($p -and $p.X -gt 0.15 * $w -and $p.X -lt 0.68 * $w -and $p.Y -gt 0.1 * $h -and $p.Y -lt 0.72 * $h) { return $c }
                }
                $null = Wait-FLFramesPassed 20
            }
            $null
        }
        $cur = Target-OnScreen 0
        if ($cur) {
            $null = Wait-FLFramesPassed 12
            Shot "${tag}_mini_card"
            $p = Mark-Xy
            Result 'LOOK' "the mark after the frame: $(if ($p) { "$($p.X),$($p.Y)" } else { 'none' }) of ${w}x${h}"
            Result 'LOOK' "mini card on $($cur.Name) ($(Send-FL 'hud2 form'))"
            $null = Wait-FLFramesPassed 400
            Shot "${tag}_settled"
        } else { Result 'LOOK' "NOT SHOWN: no hostile on screen for the mini card ($tag)" }
        foreach ($m in @(@(0, 'full'), @(1, 'fades'), @(2, 'none'))) {
            $null = Send-FL "set skin_motion $($m[0])"; $null = Wait-FLFramesPassed 20
            $was = if ($cur) { $cur.Id } else { 0 }
            $next = Target-OnScreen $was
            if (-not $next) { Result 'LOOK' "NOT SHOWN: no other hostile on screen ($($m[1]), $tag)"; continue }
            $null = Wait-FLFramesPassed 5
            Shot "${tag}_change_$($m[1])"
            Result 'LOOK' "change at $($m[1]): $was -> $($next.Id) $($next.Name)"
            $cur = $next
            $null = Wait-FLFramesPassed 600
            Shot "${tag}_change_$($m[1])_settled"
        }
        $null = Send-FL 'set skin_motion 0'
        # the bars at each motion: two stills, 0.3 s apart: at NONE and FADES they must not breathe
        $null = Send-FL 'hud2 sim critical'; $null = Wait-FLFramesPassed 60
        foreach ($m in 1, 2) {
            $null = Send-FL "set skin_motion $m"; $null = Wait-FLFramesPassed 40
            Shot "${tag}_bars_motion${m}_a"
            $null = Wait-FLFramesPassed 60
            Shot "${tag}_bars_motion${m}_b"
        }
        $null = Send-FL 'set skin_motion 0'; $null = Send-FL 'hud2 sim off'
        NoFault "owedform $tag"
        Stop-Game
    }
}

function Section-owedfight {
    foreach ($size in $Sizes) {
        $tag = Tag $size
        $w = [int]($size -split ',')[0]; $h = [int]($size -split ',')[1]
        Start-Game $size (Join-Path $root 'Install\instances\3\Freelancer HD Edition\SAVE\Accts\SinglePlayer\SaveG004.fl')
        $null = Send-FL 'godmode on'; $null = Load-FLSave 'SaveG004.fl'; $null = Wait-FLFramesPassed 200
        $null = Send-FL 'tap USER_NEXT_ENEMY'; $null = Wait-FLFramesPassed 30
        $null = Send-FL 'ap start smart'
        # a lock, then the target off the nose: the autopilot stops and the fight carries the ship past
        $locks = 0; $off = 0; $offLocked = 0; $shot = $false; $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt 70) {
            $st = "$(Send-FL 'hud2 state')"
            $locked = $st -match 'locked=1'
            if ($locked) { if (++$locks -eq 6) { $null = Send-FL 'ap stop' } }
            $p = Mark-Xy
            $isOff = $locks -ge 6 -and (-not $p -or $p.X -lt 0 -or $p.Y -lt 0 -or $p.X -gt $w -or $p.Y -gt $h)
            if ($isOff) {
                $off++
                if ($locked) { $offLocked++ }
                elseif (-not $shot) { Shot "${tag}_lock_off_nose"; Set-Content (Join-Path $dir "${tag}_lock_off_nose.txt") "$st`n$(Send-FL 'hudinfo lock')`n$(Send-FL 'targetinfo')"; $shot = $true }
            }
            Start-Sleep -Milliseconds 250
        }
        $null = Send-FL 'ap stop'
        Result $(if ($offLocked) { 'FAIL' } else { 'LOOK' }) "lock: $locks locked samples; off the screen $off samples, of them locked $offLocked; frame: $shot"
        # the meter near empty: guns held
        $null = Send-FL 'fire USER_FIRE_WEAPON1 1'
        $low = 9; $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt 40) {
            $v = Power
            if ($v -ge 0 -and $v -lt $low) { $low = $v }
            if ($v -ge 0 -and $v -lt 0.2) { break }
            Start-Sleep -Milliseconds 200
        }
        Shot "${tag}_meter_low"
        $null = Send-FL 'fire USER_FIRE_WEAPON1 0'
        Result 'LOOK' "the meter's lowest: $low"
        # a long pilot name: spawned wings until one is past the cut
        $found = $false
        foreach ($round in 1..14) {
            $null = Send-FL 'spawn fc_lr_pi_fighter_d1 player 3 900'; $null = Wait-FLFramesPassed 120
            $long = @(Get-FLContacts -All | Where-Object { $_.Ship -and $_.Name.Length -ge 24 } | Select-Object -First 1)
            if ($long.Count) {
                $null = Send-FL "settarget $($long[0].Id)"; $null = Wait-FLFramesPassed 90
                Result 'LOOK' "a name of $($long[0].Name.Length): '$($long[0].Name)'"
                Shot "${tag}_long_name"
                $found = $true; break
            }
            $null = Send-FL 'spawn clear'; $null = Wait-FLFramesPassed 30
        }
        if (-not $found) { Result 'LOOK' "NOT SHOWN: no name past the cut in 14 wings ($tag)" }
        NoFault "owedfight $tag"
        Stop-Game
    }
}

# issue 37's capitals: docked at a base whose name holds a letter past ASCII, in a copy of SaveC701: the docked title is the shared uppercase (uistrip)
function Section-owedcaps {
    $enc = [Text.Encoding]::GetEncoding(28591)
    $saves = Get-FLSaveDir
    $src = $enc.GetString([IO.File]::ReadAllBytes((Join-Path $saves 'SaveC701.fl')))
    $i = 0
    foreach ($b in @(@('Rh02', 'Rh02_06_Base', 'LUEBECK'), @('Iw04', 'Iw04_01_Base', 'CURACAO'))) {
        $to = Join-Path $dir "SaveB7E$i.fl"
        [IO.File]::WriteAllBytes($to, $enc.GetBytes($src.Replace('system = Li01', "system = $($b[0])").Replace('base = Li01_01_Base', "base = $($b[1])")))
        $size = $Sizes[$i % $Sizes.Count]
        Start-Game $size $to
        $null = Send-FL 'set ui_style 1'; $null = Wait-FLFramesPassed 120
        Result 'LOOK' "$($b[2]): $(("$(Send-FL 'uistrip data')" -split "`n")[0])"
        Shot "$(Tag $size)_caps_$($b[2])"
        NoFault "owedcaps $($b[2])"
        Stop-Game
        $i++
    }
}

try {
    foreach ($s in $Only) { "=== $s"; & "Section-$s" }
} finally {
    Stop-Game
    Set-FLInstanceDisplay $Instance 'borderless' $home_
}
if ($bad.Count) { "hud040: $($bad.Count) FAILED"; $bad | ForEach-Object { "  $_" }; exit 1 }
"hud040: no FAIL; frames in $dir"
