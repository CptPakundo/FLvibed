# What the audit of the project's state found and only the game can show. One background game:
#   pause    game time stands still under the pause (the game's own dt runs on)
#   rear     the autopilot turns with the rear view held on the pad (the steering update skips on +0x5BA)
#   load     a load from the pause menu stops the objective mode and any running mode, and reads the save's mode (no Mode::Menu on that way)
#   rs       the right stick moves the dealer's list, a pad player's only way down a long text as well
# Frames: runs\state_fixes. Usage (PowerShell tool): tests\state_fixes.ps1 [-Row pause,rear,load,rs] [-Instance 1]    build\release. Exit 1 = a check failed.
param([string[]]$Row = @('pause', 'rear', 'load', 'rs'), [string]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$dir = Join-Path $root 'research\experience\runs\state_fixes'
New-Item -ItemType Directory -Force $dir | Out-Null
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Settle([int]$n = 20) { $null = Wait-FLFramesPassed $n }
function Ship {
    $s = Send-FL 'ship'
    $p = [regex]::Match($s, 'speed=(\S+) pos=\((-?\d+),(-?\d+),(-?\d+)\)')
    $f = [regex]::Match($s, 'forward=\((\S+?),(\S+?),(\S+?)\) throttle=(\S+)')
    [pscustomobject]@{ Speed = [double]$p.Groups[1].Value; Pos = [double[]]($p.Groups[2].Value, $p.Groups[3].Value, $p.Groups[4].Value)
                       Fwd = [double[]]($f.Groups[1].Value, $f.Groups[2].Value, $f.Groups[3].Value) }
}
function Deg([double[]]$a, [double[]]$b) { [Math]::Acos([Math]::Max(-1, [Math]::Min(1, $a[0] * $b[0] + $a[1] * $b[1] + $a[2] * $b[2]))) * 57.29578 }
function Camera { [regex]::Match((Send-FL 'state'), 'camera=(\S+)').Groups[1].Value }
function Clock { [double]([regex]::Match((Send-FL 'gameclock'), 'gameclock (\S+)').Groups[1].Value) }
function Sideways { $a = Ship; $side = [double[]]($a.Fwd[2], 0, -$a.Fwd[0]); , (0..2 | ForEach-Object { [Math]::Round($a.Pos[$_] + 40000 * $side[$_]) }) }
function BarPos { [double]([regex]::Match((Send-FL 'scroll Inventory_Scroll 0'), 'pos=(-?[\d.]+)').Groups[1].Value) }
function BarText { [regex]::Match((Send-FL 'bases'), 'stickBar=(\S+)').Groups[1].Value } # (the description's bar when it overflows, else the list's)

& (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save SaveC702.fl | Select-Object -Last 1
try {
    Clear-FLPad
    Set-FLPad -LX 0.01; Settle; Clear-FLPad; Settle
    if ('pause' -in $Row) {
        '--- pause'
        $null = Send-FL 'tap USER_PAUSE'
        $null = Wait-FLUntil { if ((Send-FL 'state') -match 'paused=1') { 1 } } 'paused' 10
        $a = Clock; Start-Sleep -Milliseconds 1200; $b = Clock
        Check ([Math]::Abs($b - $a) -lt 0.05) "game time stands still under the pause ($a -> $b)"
        $null = Send-FL 'tap USER_PAUSE'
        $null = Wait-FLUntil { if ((Send-FL 'state') -match 'paused=0') { 1 } } 'resumed' 10
        $a = Clock; Start-Sleep -Milliseconds 500; $b = Clock
        Check ($b - $a -gt 0.2) "and moves again after it ($a -> $b)"
    }
    if ('rear' -in $Row) {
        '--- rear'
        $chase = Camera
        $g = Sideways; $b = Ship
        $null = Start-FLAutopilot -Mode goto -Position $g
        Set-FLPad -Buttons RS; Start-Sleep -Milliseconds 700
        $rear = Camera
        Settle 300
        $c = Ship
        Check ($rear -ne $chase) "RS held is the rear view ($chase -> $rear)"
        Check ((Deg $b.Fwd $c.Fwd) -gt 45) "the autopilot turned through it ($([Math]::Round((Deg $b.Fwd $c.Fwd))) deg)"
        Clear-FLPad; Settle; $null = Send-FL 'ap stop'; Settle 100
    }
    if ('load' -in $Row) {
        '--- load'
        $s = Send-FL 'ap start objective'
        Settle 60
        $ap = Send-FL 'ap status'
        "  $s | $(($ap -split "`n")[0])"
        Check ($ap -notmatch 'objective=off') 'the objective mode is on before the load'
        $null = Load-FLSave 'SaveC705.fl'
        Settle 60
        $after = Send-FL 'ap status'
        $mode = [regex]::Match($after, 'mode=(\S+)').Groups[1].Value
        Check ($mode -eq 'off') "a load stopped the running mode ($mode)"
        Check ($after -match 'objective=off') 'and the objective mode'
        Settle 200
        Check ([regex]::Match((Send-FL 'ap status'), 'mode=(\S+)').Groups[1].Value -eq 'off') 'nothing flies the new game unasked'
        $fixture = [IO.Path]::GetFileNameWithoutExtension((Resolve-FLFixture 'SaveC705.fl')).ToLower()   # (a mod's own fixture)
        $line = @(Get-FLLog 400 | Where-Object { $_ -match "session: .*$fixture" }) | Select-Object -Last 1
        Check ([bool]$line) "the session read the save with no menu on the way ($line)"
    }
    if ('rs' -in $Row) {
        '--- rs'
        "load: " + (Load-FLSave 'SaveC701.fl')
        Settle 60
        if ((Send-FL 'hotspots') -notmatch "(?m)^IDS_HOTSPOT_EQUIPMENTDEALER\s") { $null = Goto-FLRoom 'Equipment' }
        $null = Wait-FLRoomReady
        $null = Wait-FLUntil { $x = Send-FL 'goto IDS_HOTSPOT_EQUIPMENTDEALER'; if ($x -notmatch 'busy') { $x } } 'dealer click' 15
        $null = Wait-FLUntil { if ((Send-FL 'visible Filter_Weapons') -match '^visible') { 1 } } 'dealer open' 25
        Settle 300
        $null = Send-FL 'click Filter_Weapons'
        Settle 40
        Clear-FLPad; Set-FLPad -Buttons Down; Start-Sleep -Milliseconds 120; Clear-FLPad; Settle
        $a = BarPos
        Set-FLPad -RY -1; Start-Sleep -Milliseconds 300; Clear-FLPad; Settle
        $b = BarPos
        Check ($b -gt $a) "RS down moves the bar ($a -> $b of $(BarText))"
        $null = Save-FLShot -Name 'dealer_down' -Dir $dir
        Set-FLPad -RY 1; Start-Sleep -Milliseconds 500; Clear-FLPad; Settle
        $c = BarPos
        Check ($c -lt $b) "RS up brings it back ($b -> $c)"
        $null = Save-FLShot -Name 'dealer' -Dir $dir
    }
} finally {
    Clear-FLPad
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
}
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'all checks passed'
