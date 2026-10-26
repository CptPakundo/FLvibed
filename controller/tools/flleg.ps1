# Runs one speedrun leg (missions\Leg.ps1) on its own game instance from a save, unattended, with evidence: a fresh
# background game (deploy.ps1 -Background -Restart -NoBuild -Instance <n> -Save <save>), the leg, the phase table and
# the transcript and the game's logs in research\experience\legs\<time>_<label>_i<n>\, one line in research\experience\legs\legs.tsv
# (the leg history: label, save, outcome, seconds, fps, commit). Several at once = one pwsh per instance (-Parallel
# runs in docs\LEGS.md). Build once before (deploy.ps1 -NoStart); never build while legs run.
param([Parameter(Mandatory)][int]$Instance, [Parameter(Mandatory)][string]$Save, [Parameter(Mandatory)][string]$Label,
      [int]$TimeoutSec = 3600, [double]$BudgetSec = 0, [switch]$KeepGame, [switch]$Chain,
      # autopilot switches for this leg, sent after the boot ('ring_request 10'): a change set's A/B in one build
      # (docs\TESTING.md "The test loop" 3); the transcript records them
      [string[]]$ApSet = @(),
      # segment practice (Leg.ps1): saves at these checkpoints / a run from such a save / stop at a checkpoint
      [string[]]$SaveAt = @(), [string]$StartCp = '', [string]$StopAt = '',
      # -NoTables: no docs\LEGS.md (legtable.js) and no splits.py update after this leg (its splits.txt is still written);
      # the set drivers (tools\flset / flarms / flgate.ps1) pass it and regenerate both once after their loop
      [switch]$NoTables)
$fl = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $fl 'controller\tools\flctl.ps1')
# the catalogue's save-fit columns kept by the leg-end save below (missions\Leg.ps1 writes them; Set-FLScenes writes
# $script:FLSceneHead)
$script:FLSceneHead = @(@($script:FLSceneHead) + @('speed', 'cruise', 'hull', 'shield', 'nanobots', 'batteries', 'objective_text',
                                                   'fit', 'fit_note', 'fit_date') | Select-Object -Unique)
$dir = Join-Path $fl ("research\experience\legs\{0:yyyy-MM-dd_HHmm}_{1}_i{2}" -f (Get-Date), $Label, $Instance)
$null = New-Item -ItemType Directory -Force $dir
Start-Transcript -LiteralPath (Join-Path $dir 'transcript.txt') | Out-Null
$commit = (git -C $fl rev-parse --short HEAD) + $(if (git -C $fl status --porcelain) { '+dirty' } else { '' })
"leg $Label instance $Instance save $Save commit $commit"
$tBoot = Get-Date
if ($Save -eq 'newgame') {
    # the campaign run from the main menu (the golden run's start, LEGS.md L00): New Game, its intro movie skipped
    # natively, the Freeport 7 prologue (a 68.5 s space cutscene, unskippable) waited out, the landing at Manhattan
    $path = 'newgame'
    $null = & (Join-Path $fl 'tools\deploy.ps1') -Background -Restart -NoBuild -Instance $Instance 2>&1
    . (Join-Path $fl 'subtitles\tools\flsub.ps1')   # Send-FLS movieskip
    $r = Send-FL 'menu new'; "menu new: $r"
    $null = Wait-FLUntil { $null = Close-FLBlocker; $null = Send-FLS 'movieskip'; (Get-FLMode) -eq 'docked' } 'docked at Manhattan' 600
} else {
    # a path, else a BC46 start save, else a leg checkpoint (research\saves\legs\: M13_ck_st03.fl ... - by name only it
    # resolved while the instance still had a copy of its own, 23 Oct 2026)
    $path = if (Test-Path -LiteralPath $Save) { $Save } else {
        @('bc46-all-missions', 'legs') | ForEach-Object { Join-Path $fl "research\saves\$_\$Save" } |
            Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    }
    if (-not $path) { $path = Join-Path $fl "research\saves\bc46-all-missions\$Save" } # (deploy names it in its error)
    # (-ReplaceSave: an instance's saves are test data - a game-written checkpoint may share a BC46 file name)
    # A boot whose game never answers is retried once (session 11, twice in a day: the game did not come up, the next
    # harness call threw "shared memory not found" and ended the whole set / gate queue)
    for ($try = 1; $try -le 2; $try++) {
        # (deploy throws when the game does not come up - its own Stop preference: caught here, else the retry never ran)
        $dep = try { & (Join-Path $fl 'tools\deploy.ps1') -Background -Restart -NoBuild -Instance $Instance -Save $path -ReplaceSave 2>&1 }
               catch { "deploy threw: $_" }
        $up = $false
        $tUp = Get-Date
        while (-not $up -and ((Get-Date) - $tUp).TotalSeconds -lt 60) {
            try { $null = Send-FL 'state'; $up = $true } catch { Start-Sleep -Milliseconds 500 }
        }
        if ($up) { break }
        "boot try $try failed: the game does not answer ($(($dep | Select-Object -Last 2) -join ' / '))"
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    }
}
$boot = [int]((Get-Date) - $tBoot).TotalSeconds
"boot $boot s: $(Get-FLStoryState)"
# The legs judge the speedrun preset (docs\TESTING.md "The test loop" 7): applied first, a set's switches after it
$r = Send-FL 'ap preset speedrun'
"ap preset speedrun -> $r"
if ("$r" -match '^unknown|failed') { throw "ap preset speedrun: $r" }
foreach ($s in $ApSet) {
    $r = Send-FL "ap set $s"
    if ($r -match '^unknown|failed') { throw "ap set ${s}: $r" }
    "ap set $s -> $r"
}
# A full leg (from the leg's start save, docs\legs_reference.tsv) is judged against the cumulative checkpoint limits too
# (docs\TESTING.md "Speedrun splits"); a part run (a checkpoint save) by its segments only.
$legName = if ($Label -match '^(M\d\d[ab]?)') { $Matches[1] } else { '' }
$full = [bool](Import-Csv (Join-Path $fl 'docs\legs_reference.tsv') -Delimiter "`t" |
               Where-Object { $_.leg -eq $legName -and $_.start_save -eq (Split-Path $path -Leaf) })
"leg $legName $(if ($full) { 'full (from its start save)' } else { 'part run' })"
$f0 = Get-FLFrame; $sw = [Diagnostics.Stopwatch]::StartNew()
$res = & (Join-Path $fl 'controller\tools\missions\Leg.ps1') -Label $Label -TimeoutSec $TimeoutSec -BudgetSec $BudgetSec `
         -Out (Join-Path $dir 'phases.tsv') -Chain:$Chain -Full:$full -SaveAt $SaveAt -StartCp $StartCp -StopAt $StopAt `
         -StartSave $(if ($StartCp) { Split-Path $path -Leaf } else { '' }) *>&1 |
       ForEach-Object { "$_" }
$res
$fps = try { [int]((Get-FLFrame - $f0) / $sw.Elapsed.TotalSeconds) } catch { 0 }
$last = "$($res | Select-Object -Last 1)"
$outcome = if ($last -match '^(SUCCESS|UNTIL)') { 'PASS' } else { 'FAIL' }
$secs = [int][regex]::Match($last, 'total (\d+) s').Groups[1].Value
# Hull lost to a minefield fails the leg (the user, 23 Oct 2026: "consider any run through Zone 21 that received hull
# damage from the minefield as a fail state"): the logic's "autopilot minefield-damage" events (autopilot_frame.cpp)
if ($outcome -eq 'PASS') {
    $logNow = Join-Path (Get-FLInstallDir) 'EXE\FLController.log'
    $mine = if (Test-Path -LiteralPath $logNow) { Select-String -LiteralPath $logNow -Pattern 'autopilot minefield-damage' | Select-Object -First 1 }
    if ($mine) {
        $outcome = 'FAIL'
        $last = "FAILED (minefield hull damage: $($mine.Line.Trim())) total $secs s"
        "minefield damage -> FAIL: $($mine.Line.Trim())"
    }
}
# The leg's end is the next leg's start: a dev save there (user, 22 Oct 2026: mid-run saves at leg boundaries), catalogued
# as a scene save (docs\missions\scene_saves.tsv: mission = the leg label, step 'end'; Start-FLScene <label> end).
if ($outcome -eq 'PASS' -and -not $StopAt) {
    try { $null = Wait-FLUntil { (Get-FLMode) -in 'docked', 'space' } 'a savable state' 30; $null = Save-FLScene $Label 'end' 'leg-end' 'leg' }
    catch { "leg-end save failed: $_" }
}
$hist = Join-Path $fl 'research\experience\legs\legs.tsv'
if (-not (Test-Path $hist)) { Set-Content $hist "date`tlabel`tsave`toutcome`tsecs`tboot`tfps`tcommit`tinstance`tdetail" }
Add-Content $hist ("{0:yyyy-MM-dd HH:mm}`t{1}`t{2}`t{3}`t{4}`t{5}`t{6}`t{7}`t{8}`t{9}" -f (Get-Date), $Label, (Split-Path $path -Leaf),
                   $outcome, $secs, $boot, $fps, $commit, $Instance, ($last -replace '\t', ' ').Substring(0, [Math]::Min(300, $last.Length)))
"RESULT $outcome $Label $secs s fps=$fps"
# the speedrun comparison table in docs\LEGS.md follows the history (user, 22 Oct 2026: ever-updating, for training);
# under a set driver (-NoTables) once per set instead of per leg
if (-not $NoTables) { try { & node (Join-Path $fl 'tools\legtable.js') } catch { "legtable: $_" } }
Stop-Transcript | Out-Null
# The game's own logs are rewritten by the instance's next start: keep this leg's copies with its evidence.
foreach ($log in 'FLController.log', 'FLSubtitles.log', 'FLMotion.tsv') {
    $src = Join-Path (Get-FLInstallDir) "EXE\$log"
    if (Test-Path -LiteralPath $src) { Copy-Item -LiteralPath $src -Destination $dir -ErrorAction SilentlyContinue }
}
# Splits (docs\TESTING.md "Speedrun splits"): this leg against par / PB / gold into splits.txt beside its evidence, then
# the split history, PBs, golds, limits, docs\SPLITS.md and the LiveSplit files brought up to date (the next leg runs
# against the new bests) - under a set driver (-NoTables) the driver does that once after its loop.
try {
    $py = Join-Path $fl 'research\tools\ocr\.venv\Scripts\python.exe'
    $sp = Join-Path $fl 'tools\splits.py'
    & $py -I $sp run $dir *>&1 | ForEach-Object { "$_" } | Tee-Object -FilePath (Join-Path $dir 'splits.txt') | Out-Host
    if (-not $NoTables) { $null = & $py -I $sp update 2>&1 }
} catch { "splits: $_" }
if (-not $KeepGame) { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue }
# A failed leg (death, mission failure, stall / time-out) keeps its last 15 s of the sampler's recording beside the logs
# (user, 22 Oct 2026: a dense sequence of frames at every death / failure / stall): end_tail.mkv (the last two one-minute
# segments joined) and end_sheet.jpg (every frame of the last 15 s at the recording's 5 fps, time-stamped, with the
# HUD's speed / thrust strip). The game's 0.2 s deaths need the in-game ring buffer (handoff) for more than 5 fps.
if ($outcome -ne 'PASS') {
    try {
        $shots = Get-ChildItem (Join-Path $fl 'research\experience\shots') -Directory -Filter "*_i$Instance" |
            Where-Object { $_.LastWriteTime -ge $tBoot } | Sort-Object Name | Select-Object -Last 1
        $segs = if ($shots) { @(Get-ChildItem $shots.FullName -Filter 'video_*.mkv' | Sort-Object Name) } else { @() }
        if ($segs.Count -gt 0) {
            # the encoder finishes its last segment when the game (and the sampler) stop: wait until it stops growing
            $lastSeg = $segs[-1].FullName
            $size = -1
            for ($i = 0; $i -lt 20 -and (Get-Item -LiteralPath $lastSeg).Length -ne $size; $i++) {
                $size = (Get-Item -LiteralPath $lastSeg).Length; Start-Sleep -Milliseconds 500
            }
            $list = Join-Path $dir 'end_tail.txt'
            Set-Content $list (($segs | Select-Object -Last 2 | ForEach-Object { "file '$($_.FullName -replace "'", "'\''")'" }) -join "`n")
            $tail = Join-Path $dir 'end_tail.mkv'
            & ffmpeg -v error -y -f concat -safe 0 -i $list -c copy $tail
            Remove-Item -LiteralPath $list -ErrorAction SilentlyContinue
            $d = [double](& ffprobe -v error -show_entries format=duration -of csv=p=0 $tail)
            $sheet = & (Join-Path $fl 'tools\vidsheet.ps1') $tail -From ([Math]::Max(0, $d - 15)) -Fps 5 -Cols 5 -Rows 15 `
                       -TileWidth 384 -Out (Join-Path $dir 'end_sheet.jpg')
            "end frames: $sheet ($('{0:0.0}' -f $d) s of video in end_tail.mkv)"
        } else { "end frames: no recording found for instance $Instance" }
    } catch { "end frames failed: $_" }
}
