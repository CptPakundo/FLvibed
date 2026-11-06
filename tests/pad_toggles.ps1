# The pad's latches follow the game, and X switches the inventory's categories (GitHub issues 29, 30). One background game on the virtual pad:
#   boost      holds_as_toggles, weapon groups on triggers: LS latches the afterburner, the thrust energy runs out, the latch goes with it,
#              and one press starts it again
#   rear       RS held latches the rear view, turret view on and off ends it, the latch goes with it, one hold starts it again
#   inventory  USER_INVENTORY in flight: X steps the filters as at the dealer, FILTER is on the prompt bar, nothing of the flight moves
# Frames: runs\pad_toggles. Usage (PowerShell tool): tests\pad_toggles.ps1 [-Row boost,rear,rs,inventory,docked] [-Instance 1]   build\release. Exit 1 = a failure.
param([string[]]$Row = @('boost', 'rear', 'rs', 'inventory', 'docked'), [string]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$dir = Join-Path $root 'research\experience\runs\pad_toggles'
New-Item -ItemType Directory -Force $dir | Out-Null
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Burner { [regex]::Match((Send-FL 'hudinfo'), 'afterburner (\d)').Groups[1].Value -eq '1' }
function Held { (((Send-FL 'state') -split "`n" | Where-Object { $_ -match '^held:' }) -replace '^held:\s*', '') }
function Camera { [regex]::Match((Send-FL 'state'), 'camera=(\S+)').Groups[1].Value }
function Get-Filter { [regex]::Match((Send-FL 'bases'), 'gameFilter=(-?\d+)').Groups[1].Value } # (the game's own index, not the pad's count)

& (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save SaveC702.fl | Select-Object -Last 1
try {
    "$(Send-FL 'set holds_as_toggles 1')"
    Clear-FLPad; Set-FLPad -LX 0.01; $null = Wait-FLFramesPassed 20; Clear-FLPad; $null = Wait-FLFramesPassed 20
    if ('boost' -in $Row) {
        "--- boost: $(Send-FL 'set weapon_groups 2')"
        Press-FLPad @('LS'); $null = Wait-FLFramesPassed 10
        Check (Burner) 'LS starts the afterburner'
        $null = Wait-FLUntil { -not (Burner) } 'the thrust energy out' 30
        $null = Wait-FLFramesPassed 60
        Check ((Held) -notmatch 'USER_AFTERBURN') "the latch went with it: held [$(Held)]"
        Start-Sleep -Seconds 6 # (the energy back in part: a press with none is refused)
        Press-FLPad @('LS'); $null = Wait-FLFramesPassed 20
        Check (Burner) 'one press starts it again'
        Press-FLPad @('LS'); $null = Wait-FLFramesPassed 20
        Check (-not (Burner) -and (Held) -notmatch 'USER_AFTERBURN') 'the next press stops it, as a toggle'
        # engine kill under a latched boost, for the record: the latch follows whatever the game does
        Press-FLPad @('LS'); $null = Wait-FLFramesPassed 20
        $null = Send-FL 'tap USER_MANEUVER_ENGINEKILL'; $null = Wait-FLFramesPassed 120
        "  engine kill: burner $(Burner), held [$(Held)]"
        $null = Send-FL 'tap USER_MANEUVER_ENGINEKILL'; $null = Wait-FLFramesPassed 120
        "  engine on: burner $(Burner), held [$(Held)]"
        if (Burner) { Press-FLPad @('LS'); $null = Wait-FLFramesPassed 20 }
    }
    if ('rear' -in $Row) {
        "--- rear: $(Send-FL 'set weapon_groups 0')"
        $chase = Camera
        Set-FLPad -Buttons RS; Start-Sleep -Milliseconds 700; Clear-FLPad; $null = Wait-FLFramesPassed 20
        $rear = Camera
        Check ($rear -ne $chase) "RS held starts the rear view ($chase -> $rear)"
        $null = Wait-FLFramesPassed 90
        Check ((Held) -match 'REARVIEW') "the latch stands while the game's view does: held [$(Held)]"
        $null = Send-FL 'tap USER_AUTO_TURRET'; $null = Wait-FLFramesPassed 30
        $null = Send-FL 'tap USER_AUTO_TURRET'; $null = Wait-FLFramesPassed 90
        Check ((Camera) -eq $chase) 'turret view on and off ended the rear view'
        Check ((Held) -notmatch 'REARVIEW') "the latch went with it: held [$(Held)]"
        Set-FLPad -Buttons RS; Start-Sleep -Milliseconds 700; Clear-FLPad; $null = Wait-FLFramesPassed 20
        Check ((Camera) -eq $rear) 'one hold starts it again'
        Set-FLPad -Buttons RS; Start-Sleep -Milliseconds 700; Clear-FLPad; $null = Wait-FLFramesPassed 20
    }
    if ('rs' -in $Row) {
        "--- rs: $(Send-FL "spawn $(Resolve-FLNpc 'fc_lr_pi_fighter_d1') player 3 1500")"
        $null = Wait-FLFramesPassed 200
        Clear-FLPad; Set-FLPad -LX 0.01; $null = Wait-FLFramesPassed 20; Clear-FLPad; $null = Wait-FLFramesPassed 20
        Press-FLPad @('RS'); $null = Wait-FLFramesPassed 150 # (a tap is delivered when the double tap's window closes)
        $t = [regex]::Match((Send-FL 'pilot'), 'target=(\d+)').Groups[1].Value
        Check ($t -ne '0') "the pad's RS click finds a target ($t)"
        "  $(Send-FL 'spawn clear')"
    }
    if ('inventory' -in $Row) {
        '--- inventory'
        "open: $(Send-FL 'tap USER_INVENTORY')"
        $null = Wait-FLUntil { (Get-FLMode) -eq 'space-ui' } 'the inventory' 15
        $null = Wait-FLFramesPassed 60
        $names = @((Send-FL 'uiitems') -split "`n" | Where-Object { $_ -match '^\s*Filter_' } | ForEach-Object { ($_.Trim() -split '\s+')[0] })
        "filters: $($names -join ' ')"
        Check ($names.Count -ge 2) 'the window has its filters in flight'
        $null = Save-FLShot -Name 'inventory' -Dir $dir
        $seen = @{}
        $before = Held
        $first = Get-Filter
        "  the game's filter at the opening: $first"
        foreach ($i in 1..$names.Count) { Press-FLPad @('X'); $null = Wait-FLFramesPassed 20; $seen[(Get-Filter)] = 1; "  X: filter $(Get-Filter)" }
        Check ($seen.Count -eq $names.Count) "X reaches every filter of the game's ($($seen.Keys -join ' '))"
        Check ((Get-Filter) -eq $first) 'and comes round to where it started, no press dead'
        Check ((Held) -eq $before -and (Get-FLMode) -eq 'space-ui') 'X does nothing else: the window stays, no command held'
        $bar = "$(Send-FL 'bases')"
        Check ($bar -match 'FILTER') 'the prompt bar says so'
        $null = Save-FLShot -Name 'inventory_filtered' -Dir $dir
        "close: $(Send-FL 'tap USER_INVENTORY')"
        $null = Wait-FLUntil { (Get-FLMode) -eq 'space' } 'the window closed' 15
        $null = Wait-FLFramesPassed 200 # (it slides away after the mode: a command before that is lost)
        $null = Send-FL 'tap USER_INVENTORY'
        $null = Wait-FLUntil { (Get-FLMode) -eq 'space-ui' } 'the inventory again' 15
        $null = Wait-FLFramesPassed 60
        $was = Get-Filter
        Press-FLPad @('X'); $null = Wait-FLFramesPassed 20
        Check ((Get-Filter) -ne $was) "a reopened window: the first X moves it ($was -> $(Get-Filter))"
        $null = Send-FL 'tap USER_INVENTORY'; $null = Wait-FLFramesPassed 90
    }
    if ('docked' -in $Row) {
        '--- docked'
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
        & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save SaveC701.fl | Select-Object -Last 1
        $null = Wait-FLFramesPassed 200
        Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30
        "open: $(Send-FL 'tap USER_INVENTORY')"
        $null = Wait-FLUntil { (Get-Filter) -ne '-1' } 'the docked inventory' 15
        $null = Wait-FLFramesPassed 60
        $seen = @{}
        foreach ($i in 1..5) { Press-FLPad @('X'); $null = Wait-FLFramesPassed 20; $seen[(Get-Filter)] = 1 }
        Check ($seen.Count -ge 4) "docked: X steps the game's filters ($($seen.Keys -join ' '))"
        Check ("$(Send-FL 'bases')" -match 'FILTER') 'docked: the prompt bar says so'
        $null = Save-FLShot -Name 'inventory_docked' -Dir $dir
    }
} finally {
    Clear-FLPad
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
if ($bad.Count) { "pad toggles: $($bad.Count) FAILED"; exit 1 }
'pad toggles: OK'
