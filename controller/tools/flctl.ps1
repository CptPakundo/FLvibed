# FLController test/debug harness. Dot-source this file (it loads its parts flctl.*.ps1 in order), then use:
#   Send-FL "state"                          -> sends a debug command, returns the reply
#   Set-FLPad -Buttons A,RB -RX 0.5 -RT 1     -> drives the virtual gamepad
#   Clear-FLPad                               -> releases the virtual gamepad
#   Press-FLPad A [-HoldFrames 2]             -> tap a button (held until the game has read it)
# Layout must match src\shm.h.

$script:FLShmName = "Local\FLControllerShm"
# FLvibed root = two levels above this script (controller\tools).
$script:FLRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
. (Join-Path $script:FLRoot 'tools\flhash.ps1')   # Get-FLHash: nickname -> id (Set-FLTarget by nickname)
. (Join-Path $script:FLRoot 'tools\fldata.ps1')   # Get-FLSaveDir / Get-FLUserDataDir: the test install's own data folder
. (Join-Path $PSScriptRoot 'flroute.ps1')         # hazard-free routes for the flight helpers (Get-FLRoute, Move-FLToward)
$script:FLButtons = @{
    Up = 0x0001; Down = 0x0002; Left = 0x0004; Right = 0x0008; Start = 0x0010; Back = 0x0020;
    LS = 0x0040; RS = 0x0080; LB = 0x0100; RB = 0x0200; A = 0x1000; B = 0x2000; X = 0x4000; Y = 0x8000
}


# The harness in parts (split on 21 Oct 2026; one topic each, same scope: dot-sourced here in this order):
. (Join-Path $PSScriptRoot 'flctl.core.ps1')   # the debug channel, virtual pad, frames, log, deadlines and waits
. (Join-Path $PSScriptRoot 'flctl.rooms.ps1')   # story state, blockers, rooms, launch, base repair / restock, base stories, space waits
. (Join-Path $PSScriptRoot 'flctl.space.ps1')   # contacts, targets, docks, the game's maneuvers (goto, lanes), cruise and teleport test aids
. (Join-Path $PSScriptRoot 'flctl.missions.ps1')   # the mission event log, objectives, mission fights, dialogue, plan B, stalls, jobs
. (Join-Path $PSScriptRoot 'flctl.travel.ps1')   # the autopilot's travel, jumps, joining ships, formation, stopping maneuvers
. (Join-Path $PSScriptRoot 'flctl.ui.ps1')   # room navigation by function, UI items, events
. (Join-Path $PSScriptRoot 'flctl.saves.ps1')   # scene saves, loading saves natively, frames, memory peeks, the console, godmode
. (Join-Path $PSScriptRoot 'flctl.autopilot.ps1')   # combat test setup, mission ships, the autopilot, focus
