# Who owns the input in flight (docs\MODULES_PLAN.md P7's table): one background game walks the table's rows and prints each answer as a
# line with no time in it, so two builds are compared with a plain diff. Rows: the pad's holds while the autopilot flies, the hand-back by
# a stick, a keyboard start handed back by the pad, the options page's gate and the `enabled` gate.
# Usage (PowerShell tool): tests\input_owner.ps1 -Label <name> [-Instance 1] [-Against <name>]   build\release; lines in runs\input_owner\<name>.txt
param([Parameter(Mandatory)][string]$Label, [string]$Instance = 1, [string]$Against = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
$dir = Join-Path $root 'research\experience\runs\input_owner'
New-Item -ItemType Directory -Force $dir | Out-Null
$out = [Collections.Generic.List[string]]::new()
if (-not ('FLKeyWin' -as [type])) { Add-Type -Name FLKeyWin -Namespace '' -MemberDefinition '[DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);' }
function Note([string]$line) { $out.Add($line); $line }
function Held { (((Send-FL 'state') -split "`n" | Where-Object { $_ -match '^held:' }) -replace '^held:\s*', '' -split ' ' | Where-Object { $_ } | Sort-Object) -join ' ' }
function Mode { (Get-FLAutopilot).Mode }
function ApTime { (Get-FLAutopilot).Time }
function Settle { $null = Wait-FLFramesPassed 20 }
function Frames-Counted { [int]([regex]::Match((Send-FL 'state'), 'frame=(\d+)').Groups[1].Value) }

& (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save SaveC702.fl | Select-Object -Last 1
try {
    $dll = Join-Path (Get-FLInstallDir) 'EXE\FLvibed.dll'
    $build = (Get-FileHash $dll -Algorithm SHA256).Hash.Substring(0, 16)
    $build | Set-Content (Join-Path $dir "$Label.build")
    "build: $build (not in the compare)"
    Clear-FLPad
    Set-FLPad -LX 0.01; Settle; Clear-FLPad; Settle   # the pad is the device
    Note "start: mode $(Mode), held [$(Held)]"
    # --- the pad's holds while the autopilot flies
    $null = Send-FL 'ap start hold'; Settle
    Note "ap hold: mode $(Mode), held [$(Held)]"
    Set-FLPad -RT 1; Settle
    Note "ap + RT: held [$(Held)]"
    Set-FLPad -LT 1; Settle
    Note "ap + LT: held [$(Held)]"
    "  $(Send-FL 'owners') (not in the compare)"
    Set-FLPad -Buttons RB; Start-Sleep -Milliseconds 1500; Settle   # past the dispatcher's hold time, whatever the frame rate
    Note "ap + RB held: held [$(Held)]"
    Set-FLPad; Settle
    Note "ap, pad let go: held [$(Held)]"
    # --- a stick under autopilot_cancel, then past it
    Set-FLPad -LX 0.1; Settle
    Note "ap, small stick: mode $(Mode)"
    Set-FLPad -LX 1; Settle
    Note "hand-back by the left stick: mode $(Mode), held [$(Held)]"
    Clear-FLPad; Settle
    $null = Send-FL 'ap start hold'; Settle
    Set-FLPad -RX 1; Settle
    Note "hand-back by the right stick: mode $(Mode), held [$(Held)]"
    Clear-FLPad; Settle
    # --- a keyboard start, handed back by the pad
    # (a posted key: the game's own key path, where the bar button's `autopilot_key` listens; debug `key` goes past it)
    $game = (Get-FLGameProcess).MainWindowHandle
    [void][FLKeyWin]::PostMessage($game, 0x100, [IntPtr]0x4B, [IntPtr]0x00250001)
    [void][FLKeyWin]::PostMessage($game, 0x101, [IntPtr]0x4B, [IntPtr]0xC0250001)
    Settle
    $m = Mode
    Note "key K: mode $(if ($m -eq 'off') { 'off' } else { 'on' })"
    if ($m -ne 'off') {
        Set-FLPad -LY 1; Settle
        Note "the pad takes a keyboard start back: mode $(Mode), held [$(Held)]"
        Clear-FLPad; Settle
    }
    # --- the options page's gate: the autopilot flies on while the page takes input (only the pause holds it)
    $null = Send-FL 'ap start hold'; Settle
    $t0 = ApTime
    $null = Wait-FLFramesPassed 30
    Note "gate check, ap time moves: $((ApTime) -gt $t0)"
    $null = Send-FL 'ui options open'; Settle
    $t1 = ApTime; $null = Wait-FLFramesPassed 40
    Note "the page open: ap time moves: $((ApTime) -gt $t1), mode $(Mode)"
    $null = Send-FL 'ui options close'; Settle; Settle
    $t2 = ApTime; $null = Wait-FLFramesPassed 30
    Note "the page closed: ap time moves: $((ApTime) -gt $t2)"
    # --- the enabled gate
    $null = Send-FL 'set enabled 0'; Settle
    $t3 = ApTime; $null = Wait-FLFramesPassed 40
    Note "enabled 0: ap time moves: $((ApTime) -gt $t3), mode $(Mode)"
    $null = Send-FL 'set enabled 1'; Settle
    $t4 = ApTime; $null = Wait-FLFramesPassed 30
    Note "enabled 1: ap time moves: $((ApTime) -gt $t4)"
    $null = Send-FL 'ap stop'; Settle
    Note "ap stop: mode $(Mode), held [$(Held)]"
} finally {
    Clear-FLPad
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
$file = Join-Path $dir "$Label.txt"
$out | Set-Content $file
if ($Against) {
    $d = Compare-Object (Get-Content (Join-Path $dir "$Against.txt")) $out
    if ($d) { $d | ForEach-Object { "$($_.SideIndicator) $($_.InputObject)" }; 'input owner: DIFFERS'; exit 1 }
    "input owner: same as $Against"
}
