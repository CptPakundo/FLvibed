# A window keeps its button prompts for as long as it is open (the owner's pad card, 6 Nov 2026: at the job board they left after 3 s and never returned). prompt_bar's default,
# contextual, now fades only outside windows: a base room. Reads the bar's own state past the 3 s timer, on first and later visits, at a window with a decision (the job board), one
# with none (the news board) and a dealer with nothing selected; then a room, whose prompts still go. One background game, build\release. Exit 1 = a failure.
# Usage (PowerShell tool): tests\window_prompts.ps1 [-Instance 5] [-Root <a checkout>]   -Root: another build's tree, for a control run
param([string]$Instance = 5, [string]$Root = (Split-Path $PSScriptRoot -Parent))
$ErrorActionPreference = 'Stop'
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $Root 'tools\harness\flctl.ps1')
. (Join-Path $Root 'tools\harness\flsub.ps1')
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Bar { "$((Send-FL 'map state') -split "`n" | Where-Object { $_ -match '^promptbar ' })" }
function Field([string]$name) { [regex]::Match((Bar), "\b$name=(\S+)").Groups[1].Value }
function Open-Spot([string]$room, [string]$spot, [string]$ready) {
    if ((Send-FL 'hotspots') -notmatch "(?m)^$spot\s") { $null = Goto-FLRoom $room }
    $null = Wait-FLRoomReady
    $null = Wait-FLUntil { $x = Send-FL "goto $spot"; if ($x -notmatch 'busy') { $x } } 'click' 15
    $null = Wait-FLUntil { if ((Send-FL "visible $ready") -match '^visible') { 1 } } 'open' 25
}
function Close-Spot([string]$button) { $null = Wait-FLUntil { $null = Send-FL "click $button"; if ((Send-FL 'gametext window') -match 'window=none') { 1 } } 'close' 15; $null = Wait-FLFramesPassed 30 }
function Stays([string]$name, [int]$seconds) {
    Start-Sleep -Seconds 1
    Check ((Field 'visible') -eq '1') "${name}: the prompts show ($(Field 'screen'))"
    Start-Sleep -Seconds ($seconds - 1)
    Check ((Field 'visible') -eq '1') "${name}, $seconds s, past the 3 s timer: they stay"
}

& (Join-Path $Root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save SaveC701.fl -Modules @('subtitles', 'controller') | Select-Object -Last 1
try {
    "build: $(Get-Content (Join-Path $Root 'build\release\build_info.txt'))"
    $null = Wait-FLFramesPassed 200
    Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30
    foreach ($visit in 'first visit', 'later visit') {
        Open-Spot 'Bar' 'IDS_HOTSPOT_MISSIONVENDOR' 'MissionVendorAcceptButton'
        Stays "job board, $visit" 12
        Close-Spot 'MissionVendorCloseButton'
        Open-Spot 'Bar' 'IDS_HOTSPOT_NEWSVENDOR' 'NewsCloseButton'
        Stays "news board, $visit" 12
        Close-Spot 'NewsCloseButton'
    }
    Start-Sleep -Seconds 5
    Check ((Field 'screen') -eq 'room' -and (Field 'visible') -eq '0') "a base room, no decision: they go ($(Field 'screen'), visible=$(Field 'visible'))"
    Open-Spot 'ShipDealer' 'IDS_HOTSPOT_SHIPDEALER' 'ShipTraderShipName0'
    Stays 'ship dealer, nothing bought' 8
    Close-Spot 'ShipTraderCancelButton'
} finally {
    Clear-FLPad
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
}
if ($bad.Count) { "window prompts: $($bad.Count) FAILED"; exit 1 }
'window prompts: OK'
