# The autopilot keeps working if the game isn't paused (GitHub issue 25, the owner's rule): one background game flies a goto off the nose
# and opens each window over the flight. Judged per window: the autopilot's clock moves, its mode stays, the ship turns to the goal and
# moves, the cursor stays where the window has it; the pad's stick in the window does not hand back or steer. Row `none` is the control:
# the same flight with no window, where the stick does hand back. -Fight: the autopilot fights spawned pirates under each window, on the pad
# and on keyboard and mouse: its guns fire and hit, the cursor stays. Frames: runs\ap_under_window.
# Usage (PowerShell tool): tests\ap_under_window.ps1 [-Row page,map,inventory,story,stats,turret,none] [-Fight] [-Instance 1] [-Save SaveC702.fl]
# build\release. Exit 1 = a check failed.
param([string[]]$Row = @('page', 'map', 'inventory', 'story', 'stats', 'turret', 'none'), [switch]$Fight, [string[]]$FightRow = @('none', 'map', 'inventory', 'page'),
      [string]$Instance = 1, [string]$Save = 'SaveC702.fl')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$dir = Join-Path $root 'research\experience\runs\ap_under_window'
New-Item -ItemType Directory -Force $dir | Out-Null
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Settle([int]$n = 20) { $null = Wait-FLFramesPassed $n }
function Ship {
    $s = Send-FL 'ship'
    $p = [regex]::Match($s, 'speed=(\S+) pos=\((-?\d+),(-?\d+),(-?\d+)\)')
    $f = [regex]::Match($s, 'forward=\((\S+?),(\S+?),(\S+?)\) throttle=(\S+)')
    [pscustomobject]@{ Speed = [double]$p.Groups[1].Value; Pos = [double[]]($p.Groups[2].Value, $p.Groups[3].Value, $p.Groups[4].Value)
                       Fwd = [double[]]($f.Groups[1].Value, $f.Groups[2].Value, $f.Groups[3].Value); Throttle = [double]$f.Groups[4].Value }
}
function Dist([double[]]$a, [double[]]$b) { [Math]::Sqrt(($a[0] - $b[0]) * ($a[0] - $b[0]) + ($a[1] - $b[1]) * ($a[1] - $b[1]) + ($a[2] - $b[2]) * ($a[2] - $b[2])) }
function Deg([double[]]$a, [double[]]$b) { [Math]::Acos([Math]::Max(-1, [Math]::Min(1, $a[0] * $b[0] + $a[1] * $b[1] + $a[2] * $b[2]))) * 57.29578 }
function Mouse { [regex]::Match((Send-FL 'state'), 'mouse=\((-?\d+),(-?\d+)\)').Value }
# A goto over a goto just stopped does not turn at once, window or not: the rows with no mode change (page, none) go first and last.
function Stop-Row { $null = Send-FL 'ap stop'; Settle 200 }
$opens = @{ none = 'version'; turret = 'tap USER_NAV_MAP'; map = 'tap USER_NAV_MAP'; inventory = 'tap USER_INVENTORY'; story = 'tap USER_STORY_STAR'; stats = 'tap USER_PLAYER_STATS'; page = 'ui options open' }
$closes = @{ none = 'version'; turret = 'tap USER_NAV_MAP'; map = 'tap USER_NAV_MAP'; inventory = 'tap USER_INVENTORY'; story = 'tap USER_STORY_STAR'; stats = 'tap USER_PLAYER_STATS'; page = 'ui options close' }

if ($Fight -and $Save -eq 'SaveC702.fl') { $Save = 'SaveC705.fl' } # (armed for the fight matrix)
& (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save $Save | Select-Object -Last 1
try {
    Clear-FLPad
    Set-FLPad -LX 0.01; Settle; Clear-FLPad; Settle
    foreach ($r in $(if ($Fight) { @() } else { $Row })) {
        "--- $r"
        # `turret`: the nav map over the turret view, whose look keeps the cursor at the centre in flight
        if ($r -eq 'turret') { "turret: $(Send-FL 'tap USER_AUTO_TURRET')"; Settle 10 }
        # the game opens a window about a second after its command, so the flight starts under the open window
        "open: $(Send-FL $opens[$r])"
        if ($r -notin 'page', 'none') { $null = Wait-FLUntil { (Get-FLMode) -eq 'space-ui' } "$r open" 15 }
        Settle 10
        $mode = Get-FLMode
        $a = Ship
        # a goal sideways of the nose: the flight has to turn under the window
        $side = [double[]]($a.Fwd[2], 0, -$a.Fwd[0])
        $goal = 0..2 | ForEach-Object { [Math]::Round($a.Pos[$_] + 40000 * $side[$_]) }
        $null = Start-FLAutopilot -Mode goto -Position $goal
        $m0 = Mouse
        $b = Ship; $t = (Get-FLAutopilot).Time
        $null = Save-FLShot -Name "$r-open" -Dir $dir
        Settle 400
        $c = Ship; $ap = Get-FLAutopilot
        "  mode $mode, ap $($ap.Mode) op $($ap.Op), speed $($b.Speed) -> $($c.Speed), throttle $($c.Throttle), turned $([Math]::Round((Deg $b.Fwd $c.Fwd), 1)) deg, moved $([Math]::Round((Dist $b.Pos $c.Pos))) m, cursor $m0 -> $(Mouse)"
        Check ($r -in 'page', 'none' -or $mode -eq 'space-ui') "$r open: the game says a window has the cursor ($mode)"
        Check ($ap.Time -gt $t) "$r open: the autopilot's clock moves"
        Check ($ap.Mode -eq 'goto') "$r open: the mode stays ($($ap.Mode))"
        Check ((Dist $b.Pos $c.Pos) -gt 30) "$r open: the ship moves"
        if ($r -ne 'none') { Check ((Deg $c.Fwd $side) -lt 12) "$r open: the ship has turned to the goal ($([Math]::Round((Deg $c.Fwd $side))) deg off)" }
        Check ((Mouse) -eq $m0) "$r open: the cursor stays the window's"
        # the pad's stick goes to the window: no hand-back, no steering of the ship
        $m1 = Mouse
        Set-FLPad -LX 1; Settle 30; Clear-FLPad; Settle 5
        $ap = Get-FLAutopilot
        if ($r -eq 'turret') {
            Set-FLPad -RX 0.8 -RY 0.5; Settle 40; Clear-FLPad; Settle 30
            Check ((Mouse) -ne $m1) "turret under the map: the pad's cursor stays where it was moved ($m1 -> $(Mouse))"
            $null = Send-FL 'tap USER_AUTO_TURRET'
        }
        if ($r -eq 'none') {
            Check ($ap.Mode -eq 'off') "control, no window: the pad's stick hands back ($($ap.Mode))"
            Stop-Row
            continue
        }
        Check ($ap.Mode -eq 'goto') "$r open: the pad's stick does not hand back ($($ap.Mode))"
        "close: $(Send-FL $closes[$r])"
        if ($r -notin 'page', 'none') { $null = Wait-FLUntil { (Get-FLMode) -eq 'space' } "$r closed" 15 }
        Settle 20
        $null = Save-FLShot -Name "$r-closed" -Dir $dir
        "  mode $(Get-FLMode), ap $((Get-FLAutopilot).Mode)"
        Check ((Get-FLAutopilot).Mode -eq 'goto') "$r closed: the autopilot still flies"
        Stop-Row
        Clear-FLPad
    }
    foreach ($r in $(if ($Fight) { $FightRow } else { @() })) {
        foreach ($dev in 'pad', 'kbm') {
            "--- fight under $r, $dev"
            if ($dev -eq 'pad') { Set-FLPad -LX 0.01; Settle; Clear-FLPad; Settle } else { $null = Send-FL 'post 126'; Settle }
            "  $(Send-FL 'spawn clear')"
            "  $(Send-FL "spawn $(Resolve-FLNpc 'fc_lr_pi_fighter_d1') player 2 900")"
            Settle 30
            "open: $(Send-FL $opens[$r])"
            if ($r -notin 'page', 'none') { $null = Wait-FLUntil { (Get-FLMode) -eq 'space-ui' } "$r open" 15 }
            Settle 10
            $mode = Get-FLMode
            $null = Start-FLAutopilot -Mode combat
            $m0 = Mouse
            $sw = [Diagnostics.Stopwatch]::StartNew(); $seenHeld = ''
            while ($sw.Elapsed.TotalSeconds -lt 40) {
                $null = Wait-FLFramesPassed 60
                $h = (Send-FL 'owners'); if ($h.Length -gt $seenHeld.Length) { $seenHeld = $h }
                $ap = Get-FLAutopilot
                if ($ap.Mode -ne 'combat' -or $ap.Kills -ge 1) { break }
            }
            $null = Save-FLShot -Name "fight-$r-$dev" -Dir $dir
            "  $([int]$sw.Elapsed.TotalSeconds) s, mode $mode, ap $($ap.Mode) tactic $($ap.Tactic) on-target $($ap.OnTarget) firing $($ap.Firing) kills $($ap.Kills) dealt $($ap.Settings['dealt']) hull $($ap.Hull); $seenHeld; cursor $m0 -> $(Mouse)"
            Check ($r -in 'page', 'none' -or $mode -eq 'space-ui') "fight under ${r}, ${dev}: a window has the cursor ($mode)"
            Check ($ap.Firing -gt 0.5) "fight under ${r}, ${dev}: the guns fire ($($ap.Firing) s)"
            Check ($ap.Kills -gt 0) "fight under ${r}, ${dev}: a kill within 40 s"
            if ($r -ne 'none') { Check ((Mouse) -eq $m0) "fight under ${r}, ${dev}: the cursor stays the window's" } # (with no window the aim is the cursor)
            if ($dev -eq 'pad' -and $r -ne 'none') {
                Set-FLPad -RX 1; Settle 30; Clear-FLPad; Settle 5
                Check ((Get-FLAutopilot).Mode -eq 'combat') "fight under ${r}, pad: the stick does not hand back"
            }
            if ($r -ne 'none') {
                # a stop under the window lets go of everything the flight held
                $null = Send-FL 'ap stop'; Settle 10
                $o = "$(Send-FL 'owners')"
                Check ($o -notmatch 'autopilot') "fight under ${r}, ${dev}: a stop under the window leaves no hold ($o)"
            }
            "close: $(Send-FL $closes[$r])"
            if ($r -notin 'page', 'none') { $null = Wait-FLUntil { (Get-FLMode) -eq 'space' } "$r closed" 15 }
            $null = Send-FL 'ap stop'; $null = Send-FL 'spawn clear'; Settle 30
        }
    }
} finally {
    Clear-FLPad
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
if ($bad.Count) { "ap under window: $($bad.Count) FAILED"; exit 1 }
'ap under window: OK'
