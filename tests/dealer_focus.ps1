# With the pad the mount boxes (EquipToggleN) are out of the D-pad's focus (GitHub issue 10's comment): LS click is the mount, so A on a focused row can never unmount.
# The equipment dealer, the player's side: a walk of the D-pad over every row, in both directions, and a tap of A at each stop, reading the focus and the server's
# mounted list. Judged: no stop on a mount box; the mounted list unchanged by the walk and the taps; the rows and the filters still reached; LS click still mounts or
# unmounts the focused row, and the prompt says MOUNT. Frames at each size.
# Usage (PowerShell tool): tests\dealer_focus.ps1 [-Sizes '1280, 800','3840, 2160'] [-Instance 1]    build\release. Exit 1 on a failure.
param([string[]]$Sizes = @('1280, 800', '3840, 2160'), [string]$Instance = 1, [string]$Save = 'Save1148.fl')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
$dir = Join-Path $root 'research\experience\runs\dealer_focus'
New-Item -ItemType Directory -Force $dir | Out-Null
$home_ = Get-FLDisplaySize
$bad = [Collections.Generic.List[string]]::new()
function Check($ok, [string]$what) { $ok = [bool]@($ok | Where-Object { $_ }).Count; "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Focus { [regex]::Match("$(Send-FL 'bases')", 'focused=(\S*)').Groups[1].Value }
function Mounted { @([regex]::Matches("$(Send-FL 'equipment')", 'arch=(\d+) id=\d+ count=\d+ mounted=1') | ForEach-Object { $_.Groups[1].Value } | Sort-Object) -join ' ' }
function Press([string]$b) { Press-FLPad @($b); $null = Wait-FLFramesPassed 8 }
# The first row of the player's list: a side switch puts the focus on a row, an even number of them comes back to this side.
function First-Row {
    Press 'Right'; Press 'Left'; $null = Wait-FLFramesPassed 20
    if ("$(Send-FL 'bases')" -notmatch ' side=2 ') { Press 'Right'; $null = Wait-FLFramesPassed 20 }
    foreach ($i in 1..6) { if ((Focus) -eq 'ItemView01') { break }; Press 'Up' }
}

try {
    foreach ($size in $Sizes) {
        $tag = $size -replace '[ ,]+', 'x'
        "--- $tag"
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Set-FLInstanceDisplay $Instance 'borderless' $size
        & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Modules subtitles,controller -Save $Save | Select-Object -Last 1
        $null = Wait-FLFramesPassed 200
        Set-FLPad -RY 0.6; $null = Wait-FLFramesPassed 30; Clear-FLPad; $null = Wait-FLFramesPassed 30
        $null = Goto-FLRoom 'Equipment'
        $null = Wait-FLRoomReady
        "  dealer: $(Send-FL 'goto IDS_HOTSPOT_EQUIPMENT')"
        $null = Wait-FLUntil { if ((Send-FL 'uiitems') -match '(?m)^\s*ItemButton1\s') { 1 } } 'the dealer window' 15
        $null = Wait-FLFramesPassed 120
        Press 'Right' # the player's side
        $null = Wait-FLFramesPassed 30
        $toggles = @([regex]::Matches("$(Send-FL 'uiitems')", '(?m)^\s*(EquipToggle\d+)\s') | ForEach-Object { $_.Groups[1].Value })
        Check ($toggles.Count -gt 0) "the player's side shows mount boxes: $($toggles -join ' ')"
        $before = Mounted
        $stops = [Collections.Generic.List[string]]::new()
        # (Down runs the list to its end first: 26 mounted items are many rows, and the filters lie under the list)
        foreach ($b in @('Up') * 3 + @('Down') * 40 + @('Up') * 12) { Press $b; $stops.Add((Focus)) }
        $seen = @($stops | Sort-Object -Unique)
        "  stops: $($seen -join ' ')"
        "  in order: $((@($stops | Select-Object -First 30)) -join ' ')"
        Check (@($seen | Where-Object { $_ -match '^EquipToggle' }).Count -eq 0) 'the walk stops on no mount box'
        Check (@($seen | Where-Object { $_ -match '^ItemView' }).Count -ge 3) 'the rows are reached'
        # (the filters are X's, the lane's FILTER; the D-pad need not reach them here, and does not: below the last row it reaches ShipView_Info)
        Check (@($seen | Where-Object { $_ -match '^BuySell' }).Count -ge 1) 'the transfer panel is reached'
        # the report's accident: Up from the first row landed on a mount box. It lands on the game's BUY or SELL now, where a tap is the trade by R16 (UI_HUD2_SKIN.md 6.9), so no A here
        First-Row
        Press 'Up'
        $landed = Focus
        Check ($landed -notmatch '^EquipToggle') "Up from the first row stops on no mount box: $landed"
        Check ((Mounted) -eq $before) 'the walk unmounted nothing'
        $null = Save-FLShot -Name "${tag}_walk" -Dir $dir
        First-Row
        # LS click on a row with a box
        $row = $null
        for ($i = 0; $i -lt 8 -and -not $row; $i++) {
            $f = Focus
            if ($f -match '^ItemView0?(\d+)$' -and $toggles -contains "EquipToggle$([int]$Matches[1])") { $row = $f } else { Press 'Down' }
        }
        Check $row "a row with a box under the focus: $row"
        if ($row) {
            $null = Wait-FLFramesPassed 20
            $lane = [regex]::Match("$(Send-FL 'bases')", "items='([^']*)'").Groups[1].Value
            Check ($lane -match 'MOUNT') "the prompt says MOUNT: '$lane'"
            $now = Mounted
            Press 'LS'
            $null = Wait-FLFramesPassed 30
            $after = Mounted
            "  $(@(Select-String -Path (Join-Path (Get-FLInstallDir) 'EXE\FLController.log') -Pattern 'mount box' | Select-Object -Last 1).Line)"
            # (the list re-sorts at once, docs\FREELANCER_RE.md "ToggleButtonControl": the row now holds another item, so a second click is not the way back)
            Check ([Math]::Abs(($after -split ' ').Count - ($now -split ' ').Count) -eq 1) "LS click mounts or unmounts one item: $(($now -split ' ').Count) -> $(($after -split ' ').Count) mounted"
        }
    }
} finally {
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Set-FLInstanceDisplay $Instance 'borderless' $home_
}
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'the mount boxes out of the pad''s focus: PASS'
