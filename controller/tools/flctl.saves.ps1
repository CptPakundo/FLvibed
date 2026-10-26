# FLController harness - scene saves, loading saves natively, frames, memory peeks, the console, godmode. Part of flctl.ps1 (dot-sourced by it; never load a part alone).

# ---------------------------------------------------------------- scene saves (test start points, every mission)
# User, 18 Oct 2026: players build "hyper-specific, well-intended save sets"; re-tests start seconds before the scene
# instead of replaying a mission. A native save (debug `save` = pub::Save, what missions' Act_Save calls) restores a
# story mission exactly at the step it was made (verified 18 Oct 2026, M02 step 4: position, objective and target,
# King respawned beside the ship) - so a scene save can be made anywhere; the mission's own checkpoints (Act_Save,
# event `mission-save ids=`) are kept too. Each save is archived once (research\saves\scenes\SaveSnnn.fl, git-ignored;
# the S makes the name impossible for the game's own hex names) and catalogued in docs\missions\scene_saves.tsv:
# mission, step (the mission script's -From step), label, the objective id at save time (checked after loading: a
# stale save is reported, not used blindly), place, the game build (Freelancer.exe + Content.dll hashes) and the
# FLvibed commit it was made with.
$script:FLSceneDir = Join-Path $script:FLRoot 'research\saves\scenes'
$script:FLSceneCatalogue = Join-Path $script:FLRoot 'docs\missions\scene_saves.tsv'
# (the fitness columns since 24 Oct 2026: the ship's state at the save and the first run's verdict - Leg.ps1 CpSave /
# SAVE-FIT; every writer of the catalogue must keep them, so they are part of the header here)
$script:FLSceneHead = 'name', 'mission', 'step', 'label', 'source', 'objective', 'place', 'build', 'commit', 'date',
                      'speed', 'cruise', 'hull', 'shield', 'nanobots', 'batteries', 'objective_text', 'fit', 'fit_note', 'fit_date'
function Get-FLGameBuild {
    $i = Join-Path $script:FLRoot 'Install\Freelancer HD Edition'
    (Get-FileHash (Join-Path $i 'EXE\Freelancer.exe')).Hash.Substring(0, 8) + '-' +
        (Get-FileHash (Join-Path $i 'DLLS\BIN\Content.dll')).Hash.Substring(0, 8)
}
function Get-FLScenes {
    if (-not (Test-Path $script:FLSceneCatalogue)) { return @() }
    @(Import-Csv $script:FLSceneCatalogue -Delimiter "`t")
}
function Set-FLScenes($Rows) {
    $lines = @($script:FLSceneHead -join "`t") + @($Rows | Sort-Object mission, step, label | ForEach-Object {
        $r = $_; ($script:FLSceneHead | ForEach-Object { "$($r.$_)" -replace "[`t`r`n]", ' ' }) -join "`t" })
    Set-Content $script:FLSceneCatalogue $lines -Encoding UTF8
}

# Archives a save as the scene (Mission, Step, Label) and catalogues it: $File = a save the game already wrote (a
# checkpoint), else a native save is made now. Re-saving a scene replaces its archive (same SaveSnnn name).
function Save-FLScene([string]$Mission, [string]$Step, [string]$Label, [string]$Source = 'manual', [string]$File,
                      [string]$Objective, [string]$Place) {
    $dir = Get-FLSaveDir
    if (-not $File) {
        $before = (Get-Date).AddSeconds(-1)
        $r = Send-FL 'save'
        if ($r -notmatch 'result=0') { throw "save failed: $r" }
        $st = @{ f = $null }
        $null = Wait-FLUntil {
            $st.f = Get-ChildItem $dir -Filter 'Save*.fl' | Where-Object { $_.LastWriteTime -ge $before -and $_.Name -notmatch '^SaveS' } |
                    Sort-Object LastWriteTime -Descending | Select-Object -First 1
            [bool]$st.f
        } 'the save file' 15
        $File = $st.f.FullName
    }
    # the objective now: the host's whole event history (this session's log may have started after it was set)
    $objId = if ($Objective -eq '-') { '' } else { $Objective } # '-': none now (filled by the first verified load)
    if (-not $Objective) {
        $obj = Get-FLHistory ' objective count=\d+ .* obj=\d+ ' | Select-Object -Last 1
        $objId = if ("$obj" -match ' obj=(\d+) ') { $Matches[1] } else { '' }
    }
    $place = $Place
    if (-not $place) {
        $p = Get-FLPilot
        $place = if ($p) { "space $($p.System) $([int]$p.Pos[0]),$([int]$p.Pos[1]),$([int]$p.Pos[2])" } else { "docked" }
    }
    $rows = @(Get-FLScenes)
    $old = $rows | Where-Object { $_.mission -eq $Mission -and $_.step -eq $Step -and $_.label -eq $Label } | Select-Object -First 1
    $name = if ($old) { $old.name } else {
        $n = [int](1 + (@($rows | ForEach-Object { [int]($_.name -replace '\D', '') }) + 0 | Measure-Object -Maximum).Maximum)
        'SaveS{0:D3}.fl' -f $n
    }
    $null = New-Item -ItemType Directory -Force $script:FLSceneDir
    Copy-Item -LiteralPath $File (Join-Path $script:FLSceneDir $name) -Force
    $commit = (& git -C $script:FLRoot rev-parse --short HEAD 2>$null)
    $row = [pscustomobject]@{ name = $name; mission = $Mission; step = $Step; label = $Label; source = $Source
                              objective = $objId; place = $place; build = (Get-FLGameBuild); commit = "$commit"
                              date = (Get-Date -Format 'yyyy-MM-dd HH:mm') }
    Set-FLScenes (@($rows | Where-Object { $_.name -ne $name }) + $row)
    Write-Host ("{0:HH:mm:ss} scene save {1} {2} step {3} {4} ({5}) objective {6}" -f (Get-Date), $name, $Mission, $Step, $Label, $Source, $objId)
    $row
}

# Where a save puts the player: "space <system> <x,y,z>" or "base <base>", read from the file (game saves are
# encoded: tools\flsave.js decodes them).
function Get-FLSavePlace([string]$File) {
    $tmp = Join-Path $env:TEMP ("flv_saveplace_" + [guid]::NewGuid().ToString('N'))
    $in = New-Item -ItemType Directory (Join-Path $tmp 'in'); Copy-Item -LiteralPath $File $in
    $null = & node (Join-Path $script:FLRoot 'tools\flsave.js') $in (Join-Path $tmp 'out') 2>$null
    $ini = Get-ChildItem (Join-Path $tmp 'out') -Filter '*.ini' -ErrorAction SilentlyContinue | Select-Object -First 1
    $place = ''
    if ($ini) {
        $t = Get-Content $ini.FullName
        $val = { param($k) $m = $t | Select-String "^$k = (.+)$" | Select-Object -First 1; if ($m) { $m.Matches[0].Groups[1].Value.Trim() } else { '' } }
        $base = & $val 'base'; $sys = & $val 'system'; $pos = & $val 'pos'
        $place = if ($base) { "base $base" } else { "space $sys $($pos -replace '\s', '')" }
    }
    Remove-Item -LiteralPath $tmp -Recurse -Force -ErrorAction SilentlyContinue
    $place
}

# The mission's own checkpoint saves (Act_Save) written during a run (events after $After): each archived as a scene
# (step = the objective at the time, label "checkpoint-<ids>"), matched to the file the game wrote at that moment.
function Save-FLCheckpoints([string]$Mission, [int]$After) {
    $dir = Get-FLSaveDir
    $nowTick = [Environment]::TickCount64 -band 0xFFFFFFFFL; $now = Get-Date
    foreach ($l in (Get-FLHistory 'mission-save ids=\d+ result=0' | Where-Object { [int]("$_" -split ' ')[0] -gt $After })) {
        $null = "$l" -match '^\d+ (\d+) mission-save ids=(\d+)'
        $at = $now.AddMilliseconds(-($nowTick - [long]$Matches[1])); $ids = $Matches[2]
        $f = Get-ChildItem $dir -Filter 'Save*.fl' | Where-Object { $_.Name -notmatch '^SaveS' -and [Math]::Abs(($_.LastWriteTime - $at).TotalSeconds) -le 3 } |
             Select-Object -First 1
        if (-not $f) { Write-Host "checkpoint $ids`: no save file found near $at"; continue }
        # The objective is left to the first verified load (Start-FLScene): what the mission's resume trigger shows
        # then (around the save itself the mission often shows "Awaiting Mission Objective").
        $null = Save-FLScene $Mission "ckpt-$ids" "checkpoint-$ids" 'checkpoint' $f.FullName '-' (Get-FLSavePlace $f.FullName)
    }
}

# Starts the game at a catalogued scene: restarts it with the save placed in the save folder and loaded, then checks
# the mission's objective against the one recorded (and the game build). Returns the catalogue row with Valid / Why.
# The mission script then runs with -From <step>.
function Start-FLScene([string]$Mission, [string]$Step, [string]$Label) {
    $row = Get-FLScenes | Where-Object { $_.mission -eq $Mission -and $_.step -eq $Step -and (-not $Label -or $_.label -eq $Label) } |
           Sort-Object date -Descending | Select-Object -First 1
    if (-not $row) { throw "no scene save for $Mission step $Step $Label (Get-FLScenes)" }
    $src = Join-Path $script:FLSceneDir $row.name
    if (-not (Test-Path $src)) { throw "scene save missing from the archive: $src" }
    $why = @()
    if ($row.build -ne (Get-FLGameBuild)) { $why += "made with game build $($row.build), now $(Get-FLGameBuild)" }
    $null = & (Join-Path $script:FLRoot 'tools\deploy.ps1') -Background -Restart -NoBuild -Save $src -ReplaceSave
    $null = Start-FLEventLog
    # The objective the mission shows once loaded: the last non-zero one in this (fresh) game session's whole history
    # (a load first clears it - obj=0 - and can be quick enough to set it before this log starts); settled when one
    # is seen and 2 s passed, or 10 s without one (docked scenes, missions without objectives).
    $st = @{ obj = $null; sw = [Diagnostics.Stopwatch]::StartNew() }
    $null = Wait-FLUntil {
        foreach ($l in (Get-FLHistory ' objective count=')) { if ("$l" -match ' obj=([1-9]\d*) ') { $st.obj = $Matches[1] } }
        ($st.obj -and $st.sw.Elapsed.TotalSeconds -ge 2) -or $st.sw.Elapsed.TotalSeconds -ge 10
    } "the scene's objective" 30
    if ($row.objective -and $st.obj -ne $row.objective) { $why += "objective $($st.obj), recorded $($row.objective)" }
    if (-not $row.objective -and $st.obj -and -not $why.Count) {
        # recorded without its objective (made by an older harness): the first verified load fills it in
        $rows = @(Get-FLScenes); ($rows | Where-Object { $_.name -eq $row.name }).objective = $st.obj; Set-FLScenes $rows
        $row.objective = $st.obj
    }
    $row | Add-Member Valid ($why.Count -eq 0) -Force
    $row | Add-Member Why ($why -join '; ') -Force
    Write-Host ("{0:HH:mm:ss} scene {1} {2} step {3} {4}: {5}" -f (Get-Date), $row.name, $Mission, $Step, $row.label, $(if ($row.Valid) { 'ok' } else { "STALE: $($row.Why)" }))
    $row
}

# Loads a single-player save by its file name, natively, from wherever the game is (core\game\menu.cpp: the game's
# own load logic, no cursor, no pad navigation, no row positions): main menu -> its Load Game button and the
# character list; in game -> the pause menu (as Escape opens it) and its Load button; a death / mission failure
# dialog -> its Load button. Done when the load screen has closed and the game is docked (room built: hotspots
# listed) or in space. Freelancer lists the saves it found when it started: a save file added later needs a
# game restart (tools\deploy.ps1 -Save <file> copies it into the save folder before starting).
# $Save: file name in the save folder ('SaveC702.fl', 'SaveC702') or a path to a save there.
function Load-FLSave([string]$Save, [int]$TimeoutSec = 60, [switch]$Watch) {
    $name = Split-Path $Save -Leaf
    Clear-FLPad
    $where = Get-FLMode
    if ((Send-FL 'saves') -match 'no load screen open') {
        if ($where -eq 'menu') {
            # Right after a death the game is in menu mode before its death dialog shows (death camera): wait for the
            # dialog or the main menu, whichever the game puts up.
            $null = Wait-FLUntil {
                if ((Send-FL 'uiitems') -match '(?m)^LoadSavegameButton\s') {
                    $null = Send-FL 'click LoadSavegameButton'   # death / mission failure dialog
                    return $true
                }
                (Send-FL 'menu load') -match '^clicked'
            } 'death dialog or main menu' 20
        } elseif ((Send-FL 'uiitems') -match '(?m)^LoadSavegameButton\s') {
            $null = Send-FL 'click LoadSavegameButton'   # death / mission failure dialog
        } else {
            $null = Wait-FLUntil { $r = Send-FL 'pause'; if ($r -match 'open') { return $true }; $false } 'pause menu' 10
            $null = Wait-FLUntil { (Send-FL 'pausecodes') -notmatch 'not open' } 'pause menu' 10
            $r = Send-FL 'pauseload'; if ($r -notmatch '^clicked') { throw "pause menu: $r" }
        }
        $null = Wait-FLUntil { (Send-FL 'saves') -notmatch 'no load screen open' } 'load screen' 15
    }
    $f0 = Get-FLFrame
    $r = Send-FL "loadsave $name"
    if ($r -notmatch '^loading') { throw $r }
    # The request ([0x67ECC4] = 7, set by 0x5B2C70) is processed by the game's next frames: wait until frames have
    # advanced past it and nothing is pending, then for the loaded state (in a base: the room's buttons ready).
    $mode = Wait-FLUntil {
        if ((Get-FLFrame) -lt $f0 + 3 -or (Get-FLPeek32 0x67ECC4) -eq 7) { return $null }
        if ((Send-FL 'saves') -notmatch 'no load screen open') { return $null }
        $m = Get-FLMode
        if ($m -eq 'space') { return 'space' }
        if ($m -eq 'docked' -and (Send-FL 'hotspots') -match 'hotspots [1-9]') { return 'docked' }
        $null
    } "loaded $name" $TimeoutSec
    if ($mode -eq 'docked') { Wait-FLRoomReady -Watch:$Watch }   # the room's entry (skipped unless -Watch)
    $mode
}

function Get-FLFrame { [int]([regex]::Match((Send-FL 'state'), 'frame=(\d+)').Groups[1].Value) }

# $true as soon as $Until holds, $false once $Frames game frames passed without it. Also ends after $MaxSec of wall
# clock (was: frame counts alone - a game that stopped answering reads frame 0, and the frame counter starts over
# after a logic hot reload: the wait never ended, 18 Oct 2026).
function Wait-FLFrames([int]$Frames, [scriptblock]$Until, [double]$MaxSec = 5) {
    $f0 = Get-FLFrame
    $sw = [Diagnostics.Stopwatch]::StartNew()
    do {
        if (& $Until) { return $true }
        $f = Get-FLFrame
    } while ($f -ge $f0 -and $f -lt $f0 + $Frames -and $sw.Elapsed.TotalSeconds -lt $MaxSec)
    $false
}
function Get-FLPeek32([uint32]$Address) {
    $x = ((Send-FL ('peek 0x{0:X} 4' -f $Address)) -split ':\s*')[1].Trim() -split '\s+'
    [Convert]::ToUInt32($x[3] + $x[2] + $x[1] + $x[0], 16)
}

# Runs an HDE console command (single player): opens the chat input, types it, sends it with Enter. Waits on the
# chat input control (CTID) appearing and closing, not on time.
# Examples: 'rep l H' (Liberty police hostile), 'godmode' (takes effect on the next load), 'cash 100000'.
function Send-FLConsole([string]$Command) {
    $chatOpen = { (Send-FL 'visible CTID') -match '^visible' }
    $null = Send-FL 'tap USER_CHAT'
    $null = Wait-FLUntil $chatOpen 'chat input' 5
    $null = Send-FL "type $Command"
    $null = Send-FL 'post 13'
    $null = Wait-FLUntil { -not (& $chatOpen) } 'chat input closed' 5
}

# The console's "godmode" is a toggle and its state cannot be read back, so remember it per game
# process (it resets when the game restarts). Takes effect on the next load.
function Enable-FLGodmode {
    $p = Get-FLGameProcess
    if (-not $p) { throw "game not running" }
    $flag = Join-Path $env:TEMP "flctl_godmode_$($p.Id)"
    if (Test-Path -LiteralPath $flag) { return }
    if ((Send-FL 'state') -match 'mode=menu') { throw "godmode needs to be typed in game (chat), not in the menus" }
    Send-FLConsole "godmode"
    New-Item -ItemType File -Path $flag -Force | Out-Null
}
