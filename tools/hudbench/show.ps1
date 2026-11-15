# The owner's one command: one recorded state of HUD 2's flight HUD drawn by the design bench with no game, and checked
# against what the game drew; or drawn with a setting, a layout number or the screen size changed, beside the unchanged
# frame. Run in the PowerShell tool (or any PowerShell):
#   tools\hudbench\show.ps1 <state> [-Size WxH] [-Record 1280x800|3840x2160] [-Set name=value,...] [-Layout <file>]
#                           [-Name <word>] [-Frame 0|1] [-Run b4b_run1] [-List]
#   <state>   a recorded state's name, e.g. kbm_cruise, pad_combat (-List prints the names the run holds)
#   -Size     the screen size to draw at (default 1280x800). The recording of that size when there is one, else the
#             1280x800 recording redrawn at this size (-Record picks the recording)
#   -Set      settings for this run, e.g. -Set hud_scale=130 or -Set hud_preset=1,hud_size=large (README.md "Change a setting")
#   -Layout   a layout file (tools\hudbench\variants\*.layout.json: preset numbers by name; README.md "Change a layout number")
#   -Name     the variant's word in the file names (default: made from the changes)
# It builds the bench (always: it draws with the HUD code as it is now), replays the record (README.md "What the bench does"),
# and writes into build\bench\show\<folder>\: the lists, the atlas the replayer page draws them with, the report. With a
# change: the unchanged frame (<record>.unchanged@WxH) and the changed one (<record>.<name>@WxH) side by side.
# Exit: flbench's (0 drawn / equal; README.md lists the others).
param(
    [Parameter(Position = 0)][string]$State = '',
    [string]$Size = '1280x800',
    [string]$Record = '',
    [string[]]$Set = @(),
    [string]$Layout = '',
    [string]$Name = '',
    [int]$Frame = 0,
    [string]$Run = 'b4b_run1',
    [switch]$List
)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if ($Size -notmatch '^\d+x\d+$') { Write-Output "show: -Size WxH, e.g. 3840x2160"; exit 1 }
$base = Join-Path $root "research\experience\bench\record\$Run"
if (-not (Test-Path $base)) { Write-Output "show: no recording $base (runs: $((Get-ChildItem (Join-Path $root 'research\experience\bench\record') -Directory).Name -join ', '))"; exit 1 }
# the recording: -Record, else the one at the size drawn, else 1280x800's (redrawn at the size)
if (-not $Record) { $Record = if (Test-Path (Join-Path $base $Size)) { $Size } else { '1280x800' } }
$folder = Join-Path $base $Record
if (-not (Test-Path $folder)) { Write-Output "show: no recording at $Record in $Run (sizes: $((Get-ChildItem $base -Directory).Name -join ', '))"; exit 1 }
$names = Get-ChildItem $folder -Filter "*_$($Record)_f0.flstate.json" | ForEach-Object { $_.Name -replace "_$($Record)_f0\.flstate\.json$", '' }
if ($List -or -not $State) { Write-Output "states in $Run at $($Record):"; $names | ForEach-Object { "  $_" }; exit 0 }
$rec = Join-Path $folder "$($State)_$($Record)_f$Frame.flstate.json"
if (-not (Test-Path $rec)) { Write-Output "show: no record $rec (-List shows the states)"; exit 1 }
if ($Layout -and -not (Test-Path $Layout)) { Write-Output "show: no layout file $Layout"; exit 1 }
$Set = @($Set | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$changes = $Set.Count -gt 0 -or $Layout
$variant = $changes -or $Size -ne $Record
if ($changes -and -not $Name) {
    $parts = @($Set | ForEach-Object { $_ -replace '=', '-' })
    if ($Layout) { $parts += [IO.Path]::GetFileName($Layout) -replace '\.layout\.json$|\.json$', '' }
    $Name = ($parts -join '+') -replace '[^A-Za-z0-9_.+-]', '_'
    $Name = $Name -replace '\+', '_'
}
$exe = Join-Path $root 'build\bench\flbench.exe'
# (built every time, so it always draws with the HUD code as it is now: a few seconds when nothing changed)
& (Join-Path $root 'tools\buildlock.ps1') bench | Out-Null
if ($LASTEXITCODE -ne 0) { Write-Output 'show: the bench did not build'; exit 1 }
$sub = "$($State)_$Record" + $(if ($Size -ne $Record) { "@$Size" } else { '' }) + $(if ($changes) { ".$Name" } else { '' })
$out = Join-Path $root "build\bench\show\$sub"
New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -File | Remove-Item # (a folder holds one run: its lists and its atlas belong together)
$arg = @('replay', '--install', (Join-Path $root 'Install\Freelancer HD Edition'), '--out', $out)
if ($variant) {
    $arg += @('--size', $Size)
    foreach ($s in $Set) { $arg += @('--set', $s) }
    if ($Layout) { $arg += @('--layout', (Resolve-Path $Layout).Path) }
    if ($changes) { $arg += @('--name', $Name) }
}
& $exe @arg $rec
$code = $LASTEXITCODE
if ($Size -eq $Record) {
    $game = $rec -replace '\.flstate\.json$', '.fldl.json'
    Copy-Item $game (Join-Path $out ('game_' + (Split-Path $game -Leaf)))
}
Write-Output ''
Write-Output "The picture: open $(Join-Path $root 'tools\hudbench\web\index.html') in a browser, press 'Folder' and pick"
Write-Output "  $(Join-Path $root 'build\bench\show')"
Write-Output "then in the list under '$sub' press A on one file and B on another; 'Flip' (or the F key) switches between them."
Write-Output 'In this folder:'
Get-ChildItem $out -Filter '*.fldl.json' | ForEach-Object { "  $($_.Name)" }
if ($changes) { Write-Output "(A: the .unchanged@ list, B: the .$Name@ list; the report above says what changed and where)" }
elseif ($Size -eq $Record) { Write-Output "(the bench's list against the game's (game_...): the view 'Difference' shows where they differ)" }
exit $code
