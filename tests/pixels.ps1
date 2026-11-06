# A moved draw kit can drift unseen (docs\MODULES_PLAN.md P4): the same scenes of a build, as frames, to compare with another
# build's by tests\pixels_compare.py. One background game per size on a numbered instance, from a fixed save: HUD 2, the pad's
# legend and prompts, the options page on two tabs. Frames in research\experience\runs\pixels_<label>\ (git-ignored).
# Docked: the prompts, and a dealer under Extended HUD 2. -Dll runs another build's release DLL (build\ref_*), -Modules another row, -Classic hud_style 0.
# Usage (PowerShell tool): tests\pixels.ps1 -Label <name> [-Sizes '3840, 2160','1280, 800'] [-Instance 3] [-Dll <path>] [-Modules a,b=0] [-Classic]   uses build\release
param([Parameter(Mandatory)][string]$Label, [string[]]$Sizes = @('3840, 2160', '1280, 800'), [int]$Instance = 3, [string]$Dll = '',
      [string[]]$Modules = @('subtitles', 'controller'), [switch]$Classic, [switch]$DockedOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
$env:FLVIBED_INSTANCE = "$Instance"
$dir = Join-Path $root "research\experience\runs\pixels_$Label"
New-Item -ItemType Directory -Force $dir | Out-Null
$home_ = Get-FLDisplaySize
$release = Join-Path $root 'build\release\FLvibed.dll'
$kept = "$release.kept"
if ($Dll) { [IO.File]::Copy($release, $kept, $true); [IO.File]::Copy((Resolve-Path $Dll).Path, $release, $true) }
function Style { if ($Classic) { $null = Send-FL 'set hud_style 0'; $null = Wait-FLFramesPassed 30 } }
function Shot([string]$name) {
    $f = Save-FLShot -Name $name -Dir $dir
    Move-Item -LiteralPath $f (Join-Path $dir "$name.png") -Force
}
try {
    foreach ($size in $Sizes) {
        $tag = ($size -replace '[ ,]+', 'x')
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Set-FLInstanceDisplay $Instance 'borderless' $size
        if (-not $DockedOnly) {
        & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Modules $Modules -Save SaveC702.fl | Out-Null
        $null = Wait-FLFramesPassed 200
        Style
        Shot "${tag}_hud2"
        Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30
        Shot "${tag}_pad"
        $null = Send-FL 'ui options open'; $null = Wait-FLFramesPassed 150 # (the game's window behind it slides in)
        Shot "${tag}_options_general"
        $null = Send-FL 'ui tab 1'; $null = Wait-FLFramesPassed 30
        Shot "${tag}_options_controller"
        $null = Send-FL 'ui options close'
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        }
        & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Modules $Modules -Save SaveC701.fl | Out-Null
        $null = Wait-FLFramesPassed 200
        Style
        Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30
        Shot "${tag}_docked"
        $null = Goto-FLRoom 'Equipment'
        $null = Wait-FLRoomReady
        $r = Send-FL 'goto IDS_HOTSPOT_EQUIPMENT' # the room's sub-button: the dealer's window
        "  dealer: $r"
        $null = Wait-FLUntil { if ((Send-FL 'uiitems') -match '(?m)^\s*ItemButton1\s') { 1 } } 'the dealer window' 15
        $null = Wait-FLFramesPassed 120
        Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30
        Shot "${tag}_dealer"
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    }
} finally {
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Set-FLInstanceDisplay $Instance 'borderless' $home_
    if ($Dll) { [IO.File]::Copy($kept, $release, $true); [IO.File]::Delete($kept) }
}
Get-ChildItem $dir -Filter '*.png' | ForEach-Object { '{0,-36} {1,10} bytes' -f $_.Name, $_.Length }
