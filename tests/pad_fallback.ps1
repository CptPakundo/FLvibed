# A PlayStation or Switch pad with no setting changed (docs\UI_OPTIONS.md O5): pad_backend stays 0, and SDL3 reads the pad natively, whether or not XInput answers.
# One background game on the virtual pads: `real` the pad really plugged in here, if any (detection, type, glyph family: no press), `ps5` and `switchpro`
# SDL's virtual pads through the whole path, `mapper` a device-layer XInput pad as a mapper's mirror (read once, no flip, deadzones) and as a real Xbox pad beside
# the physical one (its own input takes over, prompts and rumble with the hands), `pad_sdl_fallback` 0, `none` no such pad in the HID list (SDL never starts).
# `page` the options page's reader follows the hands, `injected` real injected input through the hooks (F24 and a mouse move of one pixel and back).
# Usage (PowerShell tool): tests\pad_fallback.ps1 [-Row real,ps5,switchpro,mapper,page,injected,none] [-Instance 5]   build\release. Exit 1 = a failure.
param([string[]]$Row = @('real', 'ps5', 'switchpro', 'mapper', 'page', 'injected', 'none'), [int]$Instance = 5)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Backend { "$((Send-FL 'state') -split "`n" | Where-Object { $_ -match 'pad_backend=' })" }
function Field([string]$name) { [regex]::Match((Backend), "$name=(\S+)").Groups[1].Value }
function Family { [regex]::Match((Send-FL 'padhud'), 'glyphs family=(\w+)').Groups[1].Value }
function Pad { "$((Send-FL 'state') -split "`n" | Where-Object { $_ -match '^pad connected' })" }
function Focus { [regex]::Match((Send-FL 'ui rows'), 'focus=(-?\d+)').Groups[1].Value }
function Wait-Source([string]$src, [double]$sec = 8) { $null = Wait-FLUntil { (Field 'source') -eq $src } "pad source $src" $sec }
function Wait-Off([double]$sec = 8) { $null = Wait-FLUntil { (Backend) -eq '' } 'SDL stopped' $sec }

& (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save SaveC702.fl | Select-Object -Last 1
try {
    $null = Wait-FLFramesPassed 60
    Check ((Backend) -eq '') 'a test game leaves the machine''s pad alone: SDL does not run, pad_backend is the default'
    if ('real' -in $Row) {
        "--- real: $(Send-FL 'padfallback hid')"
        Start-Sleep -Seconds 4 # (2 s of waiting for a mapper, the HID list's look, SDL's start)
        "  $(Backend)".Substring(0, [Math]::Min(190, "  $(Backend)".Length))
        "  family $(Family); $((Send-FL 'state') -split "`n" | Where-Object { $_ -match '^pad connected' } | ForEach-Object { $_.Substring(0, [Math]::Min(40, $_.Length)) })"
    }
    if ($Row | Where-Object { $_ -in 'ps5', 'switchpro', 'mapper', 'page', 'injected' }) {
        "$(Send-FL 'padfallback 1')"
        Wait-Source 'sdl'
    }
    foreach ($kind in 'ps5', 'switchpro') {
        if ($kind -notin $Row) { continue }
        "--- $kind"
        "  $(Connect-FLSdlPad -Type $kind -Slot 0 -Foreground)"
        $null = Wait-FLFramesPassed 30
        $want = @{ ps5 = 'playstation'; switchpro = 'nintendo' }[$kind]
        Check ((Family) -eq $want) "$kind`: the prompts' family ($(Family))"
        Check ((Field 'sdl_type') -eq @{ ps5 = '6'; switchpro = '7' }[$kind]) "$kind`: the pad's real type ($(Field 'sdl_type'))"
        Set-FLSdlPad -Slot 0 -Buttons A -LX 0.5 -RT 1; $null = Wait-FLFramesPassed 20
        $p = "$((Send-FL 'state') -split "`n" | Where-Object { $_ -match '^pad connected' })"
        "  $($p.Substring(0, [Math]::Min(130, $p.Length)))"
        Check ($p -match 'buttons=0x2?1000 ' -and $p -match 'L=\(0\.[1-9]' -and $p -match 'RT=[1-9]|RT=0\.[1-9]') "$kind`: button, stick and trigger reach the pad's state"
        Set-FLSdlPad -Slot 0; $null = Wait-FLFramesPassed 20
        "  $(Disconnect-FLSdlPad -Slot 0)"
        $null = Wait-FLFramesPassed 30
    }
    if ('mapper' -in $Row) {
        '--- mapper: a mirror of the physical pad is never read'
        "  $(Connect-FLSdlPad -Type ps5 -Slot 0 -Foreground)"
        Wait-Source 'sdl'
        Enable-FLDevPads -Foreground
        Set-FLDevPad -Slot 0 -Connected $true
        $null = Wait-FLUntil { (Send-FL 'state') -match 'slot=0' -or (Field 'source') -eq 'sdl' } 'the XInput scan' 8
        $null = Wait-FLFramesPassed 120
        Check ((Field 'source') -eq 'sdl') 'an XInput pad at rest beside it: the physical pad stays the pad'
        foreach ($press in 1..3) {
            Set-FLSdlPad -Slot 0 -Buttons B; Set-FLDevPad -Slot 0 -Connected $true -Buttons B; $null = Wait-FLFramesPassed 12
            Check ((Pad) -match 'buttons=0x2000') "press $press and its mirror: read as one pad"
            Set-FLSdlPad -Slot 0; $null = Wait-FLFramesPassed 3; Set-FLDevPad -Slot 0 -Connected $true; $null = Wait-FLFramesPassed 30 # (the mirror's release lags)
            Check ((Field 'source') -eq 'sdl' -and (Family) -eq 'playstation') "press ${press}: the lagging release moved nothing ($(Field 'source'), $(Family))"
        }
        Set-FLDevPad -Slot 0 -Connected $true -Buttons A; Set-FLSdlPad -Slot 0 -Buttons A; $null = Wait-FLFramesPassed 12 # a mirror that leads
        Set-FLSdlPad -Slot 0; Set-FLDevPad -Slot 0 -Connected $true; $null = Wait-FLFramesPassed 30
        Check ((Field 'source') -eq 'sdl') 'a mirror that leads its pad: still the physical pad'
        Set-FLDevPad -Slot 0 -Connected $true -LX 0.2; $null = Wait-FLFramesPassed 40
        Check ((Field 'source') -eq 'sdl') 'XInput drifting inside the deadzone is at rest'
        Set-FLDevPad -Slot 0 -Connected $true; $null = Wait-FLFramesPassed 10
        '--- beside: a real Xbox pad, moving alone'
        Set-FLDevPad -Slot 0 -Connected $true -Buttons B
        Wait-Source 'xinput' 5
        $null = Wait-FLFramesPassed 10
        Check ((Pad) -match 'buttons=0x2000' -and (Family) -eq 'xbox') "its own input takes over: $(Family)"
        Set-FLDevPad -Slot 0 -Connected $true; $null = Wait-FLFramesPassed 120
        Check ((Field 'source') -eq 'xinput' -and (Family) -eq 'xbox') 'both at rest: the hands stay, the prompts do not flip'
        $d0 = @(Get-FLDevRumble | Select-Object -Last 1).Seq; if (-not $d0) { $d0 = 0 }
        $s0 = @(Get-FLSdlPadOut | Select-Object -Last 1).Seq; if (-not $s0) { $s0 = 0 }
        "  $(Send-FL 'padfallback rumble')"; $null = Wait-FLFramesPassed 60
        Check (@(Get-FLDevRumble -Since $d0 | Where-Object { $_.Low -gt 0 }).Count -gt 0 -and @(Get-FLSdlPadOut -Since $s0 | Where-Object { $_.Kind -eq 'rumble' -and $_.A -gt 0 }).Count -eq 0) 'the rumble goes to the pad in the hands, not to both'
        Set-FLSdlPad -Slot 0 -LX 0.2; $null = Wait-FLFramesPassed 40
        Check ((Field 'source') -eq 'xinput') 'the physical pad drifting inside the deadzone takes nothing'
        Set-FLSdlPad -Slot 0 -LX 0.3; $null = Wait-FLFramesPassed 12
        Check ((Field 'source') -eq 'sdl' -and (Family) -eq 'playstation') "past the deadzone it takes the pad back at once ($(Family))"
        Set-FLSdlPad -Slot 0; $null = Wait-FLFramesPassed 20
        $d0 = @(Get-FLDevRumble | Select-Object -Last 1).Seq; if (-not $d0) { $d0 = 0 }
        $s0 = @(Get-FLSdlPadOut | Select-Object -Last 1).Seq; if (-not $s0) { $s0 = 0 }
        "  $(Send-FL 'padfallback rumble')"; $null = Wait-FLFramesPassed 60
        Check (@(Get-FLSdlPadOut -Since $s0 | Where-Object { $_.Kind -eq 'rumble' -and $_.A -gt 0 }).Count -gt 0 -and @(Get-FLDevRumble -Since $d0 | Where-Object { $_.Low -gt 0 }).Count -eq 0) 'and the rumble follows it'
        '--- the options row: pad_sdl_fallback 0 asks for the mapper'
        "  $(Send-FL 'set pad_sdl_fallback 0')"
        Wait-Off 8
        Set-FLDevPad -Slot 0 -Connected $true -Buttons B; $null = Wait-FLFramesPassed 20
        Check ((Backend) -eq '' -and (Pad) -match 'buttons=0x2000') 'SDL gone, XInput alone is read'
        Set-FLDevPad -Slot 0 -Connected $true
        "  $(Send-FL 'set pad_sdl_fallback 1')"
        Wait-Source 'sdl' 10
        Set-FLDevPad -Slot 0 -Connected $false
        Disable-FLDevPads
        $null = Wait-FLFramesPassed 30
        Check ((Field 'source') -eq 'sdl') 'the XInput pad gone: the physical pad'
        "  $(Disconnect-FLSdlPad -Slot 0)"
    }
    if ('page' -in $Row) {
        '--- page: the options page reads the pad in the hands'
        "  $(Connect-FLSdlPad -Type ps5 -Slot 0 -Foreground)"
        Wait-Source 'sdl'
        Enable-FLDevPads -Foreground
        Set-FLDevPad -Slot 0 -Connected $true -Buttons B
        Wait-Source 'xinput' 8
        Set-FLDevPad -Slot 0 -Connected $true; $null = Wait-FLFramesPassed 20
        $null = Send-FL 'ui options open'; $null = Wait-FLFramesPassed 10
        $f0 = Focus
        foreach ($i in 1..2) { Set-FLDevPad -Slot 0 -Connected $true -Buttons Down; $null = Wait-FLFramesPassed 8; Set-FLDevPad -Slot 0 -Connected $true; $null = Wait-FLFramesPassed 8 }
        $f1 = Focus
        $null = Send-FL 'ui options close'; $null = Wait-FLFramesPassed 10
        Check ($f1 -and $f0 -ne $f1) "hands on XInput's pad: its D-pad moves the page's focus ($f0 -> $f1)"
        Set-FLDevPad -Slot 0 -Connected $false
        Disable-FLDevPads
        Set-FLSdlPad -Slot 0 -Buttons B; $null = Wait-FLFramesPassed 20; Set-FLSdlPad -Slot 0
        Wait-Source 'sdl' 5
        $null = Wait-FLFramesPassed 20
        $null = Send-FL 'ui options open'; $null = Wait-FLFramesPassed 10
        $f0 = Focus
        Press-FLSdlPad Down -Slot 0; Press-FLSdlPad Down -Slot 0
        $f1 = Focus
        $null = Send-FL 'ui options close'; $null = Wait-FLFramesPassed 10
        Check ($f1 -and $f0 -ne $f1) "hands on the physical pad: its D-pad does ($f0 -> $f1)"
        "  $(Disconnect-FLSdlPad -Slot 0)"
    }
    if ('injected' -in $Row) {
        # The real hooks, with the game behind: the test pad's foreground flag turns the filter on. F24 and a mouse move of nothing touch nothing on the owner's screen.
        '--- injected: a mapper''s keys and mouse, through the system'
        Add-Type -Namespace FlvT -Name In -MemberDefinition '[DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, System.UIntPtr extra); [DllImport("user32.dll")] public static extern void mouse_event(uint flags, int dx, int dy, uint data, System.UIntPtr extra);'
        $key = { [FlvT.In]::keybd_event(0x87, 0, 0, [UIntPtr]::Zero); [FlvT.In]::keybd_event(0x87, 0, 2, [UIntPtr]::Zero) }
        $mouse = { [FlvT.In]::mouse_event(1, 1, 0, 0, [UIntPtr]::Zero); [FlvT.In]::mouse_event(1, -1, 0, 0, [UIntPtr]::Zero) } # (a move of 0 makes no event: one pixel and back)
        "  $(Connect-FLSdlPad -Type ps5 -Slot 0 -Foreground)"
        Wait-Source 'sdl'
        $null = Wait-FLFramesPassed 60
        Start-Sleep -Milliseconds 400
        $d0 = [int](Field 'dropped')
        & $key; & $mouse; $null = Wait-FLFramesPassed 20
        $d1 = [int](Field 'dropped')
        Check ($d1 -eq $d0) "the pad at rest: another tool's key and mouse pass ($d0 -> $d1)"
        Set-FLSdlPad -Slot 0 -Buttons B; $null = Wait-FLFramesPassed 20
        & $key; $null = Wait-FLFramesPassed 20
        $d2 = [int](Field 'dropped')
        Check ($d2 -ge $d1 + 2) "the pad pressed: an injected key, down and up, is dropped ($d1 -> $d2)"
        & $mouse; $null = Wait-FLFramesPassed 20
        $d3 = [int](Field 'dropped')
        Check ($d3 -gt $d2) "and an injected mouse move ($d2 -> $d3)"
        Set-FLSdlPad -Slot 0; $null = Wait-FLFramesPassed 30
        Start-Sleep -Milliseconds 400
        & $key; $null = Wait-FLFramesPassed 20
        Check ([int](Field 'dropped') -eq $d3) 'released: it passes again'
        "  $(Disconnect-FLSdlPad -Slot 0)"
    }
    if ('none' -in $Row) {
        '--- none'
        "$(Send-FL 'padfallback 0')"
        Wait-Off
        Check ((Backend) -eq '') 'no such pad in the HID list: SDL does not run'
        Start-Sleep -Seconds 3
        Check ((Backend) -eq '') 'and stays off'
    }
    $log = Join-Path (Get-FLInstallDir) 'EXE\FLController.log'
    Select-String -Path $log -Pattern 'pad backend:|FATAL' | Select-Object -Last 12 | ForEach-Object { '  log: ' + $_.Line.Substring(0, [Math]::Min(120, $_.Line.Length)) }
    Check (@(Select-String -Path $log -Pattern 'FATAL').Count -eq 0) 'no fault in the log'
} finally {
    Clear-FLPad
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
if ($bad.Count) { "pad fallback: $($bad.Count) FAILED"; exit 1 }
'pad fallback: OK'
