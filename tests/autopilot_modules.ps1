# The Autopilot with the controller off, and the controller with the autopilot off (docs\MODULES_PLAN.md P8): background games, frames in
# research\experience\runs\autopilot_modules. Prints what it read; exit 1 on a failure.
# Usage (PowerShell tool): tests\autopilot_modules.ps1 [-Row alone|off] [-Instance 1]    build\release
param([string[]]$Row = @('alone', 'off'), [string]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$deploy = Join-Path $root 'tools\deploy.ps1'
$dir = Join-Path $root 'research\experience\runs\autopilot_modules'
$bad = [Collections.Generic.List[string]]::new()
if (-not ('FLKeyWin2' -as [type])) { Add-Type -Name FLKeyWin2 -Namespace '' -MemberDefinition '[DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);' }
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Stop-Game { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue }
function Key([int]$vk) {
    $game = (Get-FLGameProcess).MainWindowHandle
    [void][FLKeyWin2]::PostMessage($game, 0x100, [IntPtr]$vk, [IntPtr]0x00250001)
    [void][FLKeyWin2]::PostMessage($game, 0x101, [IntPtr]$vk, [IntPtr]0xC0250001)
}
function Ap-Field([string]$s, [string]$k) { [regex]::Match($s, "(?:^|\s)$k=(\S+)").Groups[1].Value }
function Wait-Sec([double]$s) { Start-Sleep -Milliseconds ([int]($s * 1000)) }

try {
    if ('alone' -in $Row) {
        '--- the autopilot with the controller off, on keyboard and mouse'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,autopilot=1' -Save SaveC702.fl | Select-Object -Last 1
        Wait-Sec 2
        $s0 = Send-FLS 'ap status'
        "  $("$s0".Substring(0, [Math]::Min(140, "$s0".Length)))"
        Check ("$(Send-FLS 'mode')" -eq 'space') "mode: $(Send-FLS 'mode')"
        Check ((Ap-Field $s0 'mode') -eq 'off') 'it starts off'
        $hooks = @((Send-FLS 'hooks') -split "`n" | ForEach-Object { ($_ -split ' ')[0] })
        Check (($hooks -contains 'GunAimPoint') -and ($hooks -contains 'JumpInComplete') -and ($hooks -contains 'IsCmdHeld')) 'its hooks are in'
        Save-FLShot -Name 'alone_bar' -Dir $dir
        Key 0x4B # K: the autopilot key, through the raw key chain
        Wait-Sec 2
        $s1 = Send-FLS 'ap status'
        "  $("$s1".Substring(0, [Math]::Min(140, "$s1".Length)))"
        Check ((Ap-Field $s1 'mode') -ne 'off') "the K key starts it: mode $(Ap-Field $s1 'mode')"
        $t1 = [double](Ap-Field $s1 'time')
        Wait-Sec 3
        $s2 = Send-FLS 'ap status'
        Check ([double](Ap-Field $s2 'time') -gt $t1) "it flies: time $t1 -> $(Ap-Field $s2 'time')"
        "  owners: $(Send-FLS 'owners')"
        Save-FLShot -Name 'alone_flying' -Dir $dir
        $null = Send-FLS 'ap stop'
        Wait-Sec 1
        Check ((Ap-Field (Send-FLS 'ap status') 'mode') -eq 'off') 'ap stop stops it'
        $p0 = Send-FLS 'goals'
        "  goals: $("$p0".Substring(0, [Math]::Min(120, "$p0".Length)))"
        $null = Send-FLS 'goals open'
        Wait-Sec 1
        $p1 = Send-FLS 'goals'
        "  goals open: $("$p1".Substring(0, [Math]::Min(120, "$p1".Length)))"
        Check ("$p1" -ne "$p0") 'the goal panel opens'
        Save-FLShot -Name 'alone_goals' -Dir $dir
        $null = Send-FLS 'goals close'
        $null = Send-FLS 'ap start hold'
        Wait-Sec 2
        Check ((Ap-Field (Send-FLS 'ap status') 'mode') -eq 'hold') 'ap start hold'
        Check ((Send-FLS 'ping') -eq 'pong') 'the game answers'
        $log = Get-Content (Join-Path (Get-FLInstallDir) 'EXE\FLSubtitles.log') -Raw
        Check ($log -notmatch 'FATAL') 'no fault in the log'
        Stop-Game
    }
    if ('off' -in $Row) {
        '--- the controller with the autopilot off'
        & $deploy -Background -NoBuild -Release -Instance $Instance -Modules 'subtitles,controller,autopilot=0' -Save SaveC702.fl | Select-Object -Last 1
        Set-FLPad -LX 0.01; $null = Wait-FLFramesPassed 20; Clear-FLPad; $null = Wait-FLFramesPassed 20
        $hooks = @((Send-FL 'hooks') -split "`n" | ForEach-Object { ($_ -split ' ')[0] })
        Check (($hooks -notcontains 'GunAimPoint') -and ($hooks -notcontains 'JumpInComplete')) 'none of its hooks'
        $r = Send-FL 'ap status'
        Check ("$r" -match 'module is off') "ap: $r"
        Set-FLPad -Buttons BACK; Wait-Sec 1; Clear-FLPad; $null = Wait-FLFramesPassed 20 # View held: the autopilot's button
        Check ("$(Send-FL 'ap status')" -match 'module is off') 'View held starts nothing'
        Set-FLPad -LY 1; $null = Wait-FLFramesPassed 60; Clear-FLPad
        $hud = Send-FL 'hud2 state'
        "  hud2: $("$hud".Substring(0, [Math]::Min(120, "$hud".Length)))"
        Save-FLShot -Name 'off_flight' -Dir $dir
        $log = Get-Content (Join-Path (Get-FLInstallDir) 'EXE\FLController.log') -Raw
        Check ($log -notmatch 'FATAL') 'no fault in the log'
        Stop-Game
    }
} finally {
    Stop-Game
    & $deploy -Instance $Instance -NoBuild -NoStart -Modules subtitles,controller | Out-Null
}
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'autopilot modules: PASS'
