# Builds FLvibed and deploys it into the test install (FLvibed\Install\Freelancer HD Edition).
#   deploy.ps1                 dev build: FLvibed.dll + the logic, FLvibedLogic.dll; if the game runs and FLvibed.dll is
#                              unchanged, the logic hot-reloads, otherwise the game restarts
#   deploy.ps1 -Impl           build only the logic and hot-reload it in the running game
#   deploy.ps1 -Release        the player build: FLvibed.dll alone (logic linked in, no hot reload); restarts
#   deploy.ps1 -Restart        stop the game, copy, start it
#   deploy.ps1 -NoStart        do not start the game
#   deploy.ps1 -Intro          let the startup movies play (otherwise each is skipped natively: `movieskip`)
#   deploy.ps1 -Save <file>    start, then load that save by name, natively (Load-FLSave); a save from outside the
#                              save folder is copied in first (Freelancer lists saves when it starts); never as
#                              AutoSave.fl / Restart.fl, never over a different file of that name unless -ReplaceSave
#   deploy.ps1 -Modules subtitles,controller
#                              which modules FLvibed.ini enables (default: both); takes effect at game start. A row of
#                              core\modules_rules.h is written only when named: -Modules subtitles,controller,speedrun=0
#   deploy.ps1 -Background     a test start that leaves the user alone: no focus taken, runs out of focus, sound
#                              muted (FLVIBED_BACKGROUND=1 for that process only); tests go on while the user works
#   deploy.ps1 -NoBuild        use the last build as it is (a sweep builds once, then starts each section with this)
#   deploy.ps1 -Instance <n>   parallel test games (n >= 1): Install\instances\<n> (a hard-linked mirror, made when
#                              missing), its own saves / logs / screenshots; the game and the tools of this process
#                              use channel <n> (FLVIBED_INSTANCE, tools\fldata.ps1). Build once first (-NoBuild here).
#                              Or a prepared folder's name (Install\instances\vanilla11c: the vanilla 1.1 install).
#   deploy.ps1 -GameArgs <s>   test-only: Freelancer.exe's command line, e.g. `-s127.0.0.1:2302` (the game's own direct
#                              connect to a server - the vanilla 1.1 client lists LAN servers by IPv6 link-local only);
#                              needs a fresh pwsh (the FLVWin type of an older deploy has no arguments overload)
param([switch]$Impl, [switch]$Release, [switch]$Restart, [switch]$NoStart, [switch]$Intro, [string]$Save = '',
      [string[]]$Modules = @('subtitles', 'controller'), [switch]$Background, [switch]$ReplaceSave, [switch]$NoBuild,
      [string]$Instance = '', [string]$GameArgs = '')
$ErrorActionPreference = 'Stop'
# -Instance: a number (a hard-linked mirror, made when missing) or the name of a prepared instance folder
# (Install\instances\vanilla11c: its own install, e.g. vanilla 1.1; 29 Oct 2026). '' / 0 = the main install.
$hasInst = $Instance -and $Instance -ne '0'

$root = Split-Path $PSScriptRoot -Parent                       # FLvibed
# Parse every harness script first: one syntax slip ("$name:" in a string, twice on 20 Oct 2026) broke the whole
# dot-sourced harness and surfaced only as "Get-FLGameProcess is not recognized" inside a test.
foreach ($ps in Get-ChildItem (Join-Path $root 'tools') -Filter *.ps1 -Recurse) {
    $perr = $null
    $null = [Management.Automation.Language.Parser]::ParseFile($ps.FullName, [ref]$null, [ref]$perr)
    if ($perr) { throw "harness script does not parse: $($ps.Name) line $($perr[0].Extent.StartLineNumber): $($perr[0].Message)" }
}
if ($hasInst) { $env:FLVIBED_INSTANCE = "$Instance" }
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\harness\flctl.ps1')
if ($hasInst -and -not (Test-Path (Join-Path (Get-FLInstallDir) 'EXE\Freelancer.exe'))) {
    if ($Instance -notmatch '^\d+$') { throw "instance ${Instance}: no install at $(Get-FLInstallDir) (named instances are prepared by hand)" }
    "instance ${Instance}: mirror -> " + (New-FLInstance ([int]$Instance))
}
$exeDir = Join-Path (Get-FLInstallDir) 'EXE'
# The test install's game data stays in the install (user, 18 Oct 2026: nothing of this project's game in the places a
# personal Freelancer install uses): refuse to run it while flplusplus would write to the shared Documents folder.
Assert-FLDataSeparate
# (flsweep's attended fullscreen row sets FLVIBED_ATTENDED: the owner watches and the screen may be taken)
if ($Background -and -not $env:FLVIBED_ATTENDED) { Assert-FLNoFullscreen $exeDir }
if ($Impl -and $Release) { throw '-Impl hot-reloads the dev build; the release build has no separate logic DLL' }

# ---------------------------------------------------------------- build
$config = if ($Release) { 'release' } else { 'dev' }
$target = if ($Impl) { 'impl' } else { $config }
$build = Join-Path $root "build\$config"
if ($NoBuild) {
    if (-not (Test-Path (Join-Path $build 'FLvibed.dll'))) { throw "-NoBuild: no $config build in $build" }
    # (P11g: another session's uncommitted edit reached a build that would have counted as a check on the clean code)
    $info = Join-Path $build 'build_info.txt'
    if ((Test-Path $info) -and (Get-Content $info -Raw) -match '\+dirty') { Write-Warning "-NoBuild: the $config build is +dirty, not any commit: do not count this run ($((Get-Content $info -Raw).Trim()))" }
} else {
    # through the machine-wide build lock: a deploy beside another session's build breaks both build\ folders
    & (Join-Path $PSScriptRoot 'buildlock.ps1') $target
    if ($LASTEXITCODE -ne 0) { throw "build failed" }
}

# ---------------------------------------------------------------- install configuration
# dacom.ini [Libraries]: FLvibed.dll once, after the game's own plugins; the old per-module host DLLs go.
$dacom = Join-Path $exeDir 'dacom.ini'
$lines = [Collections.Generic.List[string]](Get-Content $dacom)
$changed = $false
foreach ($old in 'FLSubtitles.dll', 'FLController.dll') {
    for ($k = $lines.Count - 1; $k -ge 0; $k--) { if ($lines[$k] -match "^\s*$([regex]::Escape($old))\s*$") { $lines.RemoveAt($k); $changed = $true } }
}
if (-not ($lines | Where-Object { $_ -match '^\s*FLvibed\.dll\b' })) {
    $i = $lines.FindIndex({ param($l) $l.Trim() -ieq '[Libraries]' })
    if ($i -lt 0) { throw "dacom.ini has no [Libraries] section: $dacom" }
    $j = $i + 1
    while ($j -lt $lines.Count -and $lines[$j] -notmatch '^\s*\[') { $j++ }
    while ($j -gt $i + 1 -and $lines[$j - 1].Trim() -eq '') { $j-- }
    $lines.Insert($j, 'FLvibed.dll')
    $changed = $true
}
if ($changed) { Set-Content $dacom $lines -Encoding ascii; 'dacom.ini: FLvibed.dll listed (old module DLLs removed)' }

# FLvibed.ini: enabled modules (read once at game start)
$known = 'subtitles', 'controller'
$rows = 'hud', 'autopilot', 'speedrun', 'display', 'npc', 'movies'   # absent = inherited (core\modules_rules.h)
$named = [ordered]@{}
foreach ($m in ($Modules -split ',')) {   # (one string through pwsh -File)
    $name, $value = $m -split '=', 2
    if ($known + $rows -notcontains $name) { throw "unknown module $name (known: $(($known + $rows) -join ', '))" }
    $named[$name] = if ($null -eq $value) { 1 } else { [int]$value }
}
# + the settings stamp (core\settings_reset.h kVersion): a test start never runs the release's settings reset
# + the first card's stamp (docs\UI_OPTIONS.md 5a): a test start never stalls on the first-start card; a test of the card clears it, or opens it with `ui card`
$lines = @($known | ForEach-Object { "$_ = $([int]($named[$_] -eq 1))" }) + @($rows | Where-Object { $named.Contains($_) } | ForEach-Object { "$_ = $($named[$_])" })
$ini = "[Modules]`r`n" + ($lines -join "`r`n") + "`r`n" +
    "[FLvibed]`r`nsettings_version = 2`r`nfirst_card = 1`r`n"
$iniPath = Join-Path $exeDir 'FLvibed.ini'
$iniChanged = -not (Test-Path $iniPath) -or ((Get-Content $iniPath -Raw) -ne $ini)
if ($iniChanged) { [IO.File]::WriteAllText($iniPath, $ini) }

# A numbered test instance plays the main install's settings: its FLController.ini / FLSubtitles.ini (written by the
# options page or `config save` in an instance game, not hard links) are made the main install's again before the
# start, the old one kept beside it as <name>.<time>.bak. Why: instance 3 flew M13 for days with its own 25 Oct
# [Autopilot] values (static_hold_gap 10, ring_request 10, ...) while 1 and 2 had none - a confound in every set that
# rotates arms over instances (run-judge, Q4 probe, 30 Oct 2026). A setting under test goes through flleg -ApSet.
$iniSynced = $false
if ($hasInst -and $Instance -match '^\d+$') {
    $mainExe = Join-Path $root 'Install\Freelancer HD Edition\EXE'
    foreach ($n in 'FLController.ini', 'FLSubtitles.ini') {
        $mine = Join-Path $exeDir $n; $theirs = Join-Path $mainExe $n
        $same = if (Test-Path $theirs) { (Test-Path $mine) -and (Get-FileHash $mine).Hash -eq (Get-FileHash $theirs).Hash } else { -not (Test-Path $mine) }
        if ($same) { continue }
        if (Test-Path $mine) { [IO.File]::Move($mine, "$mine.$(Get-Date -Format yyyyMMdd_HHmmss).bak") }
        if (Test-Path $theirs) { [IO.File]::Copy($theirs, $mine) }
        "instance ${Instance}: $n set to the main install's (the old one kept as .bak)"
        $iniSynced = $true
    }
}

# ---------------------------------------------------------------- restart decision
$running = Get-FLGameProcess | Where-Object { -not $_.HasExited }   # this instance's game only; a game being killed is not running
$hostChanged = $false
if (-not $Impl) {
    $deployed = Join-Path $exeDir 'FLvibed.dll'
    $hostChanged = -not (Test-Path $deployed) -or ((Get-FileHash $deployed).Hash -ne (Get-FileHash (Join-Path $build 'FLvibed.dll')).Hash)
}
if ($Restart -or (($hostChanged -or $iniChanged -or $iniSynced) -and $running)) {
    if ($running) {
        $running | Stop-Process -Force
        $running | Wait-Process -Timeout 10 -ErrorAction SilentlyContinue
    }
    $running = $null
}
if (-not $running) {
    # Windows releases the loaded DLLs a moment after the process ends: wait until every one of ours can be written
    # (was: FLvibed.dll only - a release deploy then failed on a hot-reload copy, FLvibedLogic.run0.dll, 19 Oct;
    # and only when deploy itself stopped the game - a game killed just before by a test script left FLvibed.dll
    # locked and the copy failed, 20 Oct 2026).
    $sw = [Diagnostics.Stopwatch]::StartNew()
    foreach ($dll in @(Get-ChildItem $exeDir -Filter 'FL*.dll' | ForEach-Object FullName)) {
        while ($sw.Elapsed.TotalSeconds -lt 15) {
            try { [IO.File]::Open($dll, 'Open', 'ReadWrite', 'None').Dispose(); break } catch { Start-Sleep -Milliseconds 100 }
        }
    }
}

# ---------------------------------------------------------------- copy
# A DLL can be locked again for a moment right after the unlock wait (seen 20 Oct 2026: the open test passed, the copy
# failed; the file was free seconds later - a scan of the just-released image is the likely holder): retry up to 15 s.
function Copy-Retry([string]$from, [string]$to) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        try { Copy-Item $from $to -Force -ErrorAction Stop; return }
        catch { if ($sw.Elapsed.TotalSeconds -ge 15) { throw }; Start-Sleep -Milliseconds 250 }
    }
}
New-Item -ItemType Directory -Force (Join-Path $exeDir 'FLSubtitles') | Out-Null
foreach ($f in 'subtitles.tsv', 'movies.tsv', 'mixes.tsv', 'movie_frames.tsv', 'speaker_colors.tsv', 'word_times.tsv') {
    $src = Join-Path $root "modules\subtitles\data\$f"
    if (-not (Test-Path $src)) { throw "shipped file missing: $src" }   # never skipped in silence: a moved file would just not arrive
    Copy-Item $src (Join-Path $exeDir "FLSubtitles\$f") -Force
}
# FLvibed's own shipped files (EXE\FLvibed\): the maneuver bar's autopilot icon mesh (modules\autopilot\data\autopilot.3db, built by
# tools\mk3db.py; the host falls back to the overlay-drawn glyph when it is missing)
New-Item -ItemType Directory -Force (Join-Path $exeDir 'FLvibed') | Out-Null
foreach ($f in 'autopilot.3db') {
    $src = Join-Path $root "modules\autopilot\data\$f"
    if (-not (Test-Path $src)) { throw "shipped file missing: $src" }
    Copy-Item $src (Join-Path $exeDir "FLvibed\$f") -Force
}
if (-not $Impl) { Copy-Retry (Join-Path $build 'FLvibed.dll') (Join-Path $exeDir 'FLvibed.dll') }
# what the build was built from (buildlock.ps1 Write-FLBuildInfo; flleg.ps1 records it per run)
$info = Join-Path $build 'build_info.txt'
if (Test-Path $info) { Copy-Item $info (Join-Path $exeDir 'FLvibed.build.txt') -Force }
$logic = 'FLvibedLogic.dll'
foreach ($f in @($logic) + 'FLSubtitlesImpl.dll', 'FLControllerImpl.dll', 'FLSubtitles.dll', 'FLController.dll') {
    $p = Join-Path $exeDir $f
    if ($Release -or $f -ne $logic) { if (Test-Path $p) { Remove-Item -LiteralPath $p -Force } }   # not used
    else { Copy-Retry (Join-Path $build $f) $p }
}
# the dev build's hot-reload copies: unused in a release; before P1 each module had its own
Get-ChildItem $exeDir -Filter 'FL*Impl.run*.dll' | Remove-Item -Force
if ($Release) { Get-ChildItem $exeDir -Filter 'FLvibedLogic.run*.dll' | Remove-Item -Force }

# ---------------------------------------------------------------- hot reload or start
$on = @{ subtitles = $named['subtitles'] -eq 1; controller = $named['controller'] -eq 1 }
if ($running) {   # one reload for all the logic, through either module's channel
    if ($on.controller) { 'logic -> ' + (Send-FL 'reload') } elseif ($on.subtitles) { 'logic -> ' + (Send-FLS 'reload') }
    return
}
if ($NoStart) { return }
$saveName = ''
if ($Save) {
    $Save = Resolve-FLFixture $Save   # (a mod's own fixture; no map, the name as given)
    # Freelancer lists the saves it finds when it starts: a save from elsewhere is copied into the save folder under
    # its own name first; it is loaded by name (Load-FLSave) once the main menu runs.
    $saveDir = Get-FLSaveDir # the test install's own folder (tools\fldata.ps1), never a personal install's
    $saveName = Split-Path $Save -Leaf
    if ($saveName -notmatch '\.fl$') { $saveName += '.fl' }
    if (Test-Path -LiteralPath $Save) {
        $dst = Join-Path $saveDir $saveName
        if ((Resolve-Path -LiteralPath $Save).Path -ne $dst) {
            # The save folder is the user's own (shared with their play): never AutoSave.fl / Restart.fl, never replace a
            # different file of the same name unless -ReplaceSave (was: Copy-Item -Force, 18 Oct 2026).
            if ($saveName -in 'AutoSave.fl', 'Restart.fl') { throw "refused: $saveName is the player's own file name; copy it under another name" }
            $same = (Test-Path -LiteralPath $dst) -and
                    (Get-FileHash -LiteralPath $dst).Hash -eq (Get-FileHash -LiteralPath $Save).Hash
            if ((Test-Path -LiteralPath $dst) -and -not $same -and -not $ReplaceSave) {
                throw "refused: $dst exists with other content (pass -ReplaceSave to replace it)"
            }
            if (-not $same) { Copy-Item -LiteralPath $Save $dst -Force }
        }
    } elseif (-not (Test-Path -LiteralPath (Join-Path $saveDir $saveName))) { throw "save not found: $Save" }
}
# Freelancer's load screen lists at most 256 saves (verified 20 Oct 2026: 261 files, 256 listed, the requested
# Save6380 missing - "no save in the load screen"); test runs add the missions' own key saves (Act_Save) every time.
# Above 240 the oldest hex-named saves the repository does not name (the missions' own Act_Save files) move to
# SAVE\overflow\ (kept, never deleted); the requested save, every save the repository names, and AutoSave / Restart
# stay.
$fullDir = Get-FLSaveDir
$all = @(Get-ChildItem -LiteralPath $fullDir -Filter '*.fl' -ErrorAction SilentlyContinue)
if ($all.Count -gt 240) {
    $over = Join-Path (Get-FLInstallDir) 'SAVE\overflow'
    New-Item -ItemType Directory -Force $over | Out-Null
    # Never a save the repository names (fixtures, campaign saves the tests use: the CLAUDE.md files, the harness and
    # research tools, the mission docs) - only the ones the missions write themselves (20 Oct 2026: the fixtures
    # SaveC700-C704 and Save2b9b went to overflow - "C700" is hex too - and their tests found no save).
    $named = @{}
    $docs = @(Get-ChildItem -LiteralPath $root -Filter 'CLAUDE.md' -Recurse -Depth 2 -ErrorAction SilentlyContinue) +
            @(Get-ChildItem -LiteralPath (Join-Path $root 'tools\harness') -Filter '*.ps1' -Recurse -ErrorAction SilentlyContinue) +
            @(Get-ChildItem -LiteralPath (Join-Path $root 'research\tools') -Filter '*.ps1' -ErrorAction SilentlyContinue) +
            @(Get-ChildItem -LiteralPath (Join-Path $root 'docs\missions') -Filter '*.md' -ErrorAction SilentlyContinue)
    foreach ($m in ($docs | Select-String -Pattern 'Save[0-9A-Za-z]{4}\b' -AllMatches)) {
        foreach ($x in $m.Matches) { $named["$($x.Value).fl".ToLower()] = $true }
    }
    $movable = @($all | Where-Object { $_.Name -match '^Save[0-9a-fA-F]{4}\.fl$' -and -not $named[$_.Name.ToLower()] -and
                                       $_.Name -ne $saveName } | Sort-Object LastWriteTime)
    $n = [Math]::Min($movable.Count, $all.Count - 200)
    foreach ($f in $movable | Select-Object -First $n) { Move-Item -LiteralPath $f.FullName (Join-Path $over $f.Name) -Force }
    "saves: $($all.Count) in the folder (load screen limit 256): moved the $n oldest SaveXXXX to $over"
}
if (-not ('FLVWin' -as [type])) {
    Add-Type @'
using System; using System.Runtime.InteropServices; using System.Text;
public static class FLVWin {
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint m, IntPtr w, IntPtr l);
  [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)] struct STARTUPINFO {
    public int cb; public string r, d, t; public int x, y, w, h, cx, cy, fill, flags; public short show, r2;
    public IntPtr r3, i, o, e; }
  [StructLayout(LayoutKind.Sequential)] struct PROCESS_INFORMATION { public IntPtr hp, ht; public int pid, tid; }
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)] static extern bool CreateProcess(
    string app, string cmd, IntPtr pa, IntPtr ta, bool inherit, int flags, IntPtr env, string dir,
    ref STARTUPINFO si, out PROCESS_INFORMATION pi);
  [DllImport("kernel32.dll")] static extern bool CloseHandle(IntPtr h);
  // Starts a program with its first window shown inactive (SW_SHOWNOACTIVATE): it does not take the focus.
  public static int StartInactive(string exe, string dir) { return StartInactive(exe, dir, null); }
  public static int StartInactive(string exe, string dir, string args) {
    var si = new STARTUPINFO(); si.cb = Marshal.SizeOf(si); si.flags = 1; si.show = 4;
    PROCESS_INFORMATION pi;
    if (!CreateProcess(exe, string.IsNullOrEmpty(args) ? null : "\"" + exe + "\" " + args, IntPtr.Zero, IntPtr.Zero, false, 0, IntPtr.Zero, dir, ref si, out pi))
      throw new System.ComponentModel.Win32Exception();
    CloseHandle(pi.hp); CloseHandle(pi.ht); return pi.pid;
  }
  public delegate bool EnumProc(IntPtr h, IntPtr l);
  [DllImport("user32.dll")] static extern bool EnumChildWindows(IntPtr p, EnumProc f, IntPtr l);
  [DllImport("user32.dll", CharSet = CharSet.Unicode)] static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  // A startup movie is playing: Movie.dll's DirectShow renderer window is a child of the game window.
  public static bool MoviePlaying(IntPtr game) {
    bool found = false;
    EnumChildWindows(game, (h, l) => { var s = new StringBuilder(64); GetClassName(h, s, 64);
      if (s.ToString() == "VideoRenderer") { found = true; return false; } return true; }, IntPtr.Zero);
    return found;
  }
}
'@
}
$flExe = Join-Path $exeDir 'Freelancer.exe'
# Keep the previous sessions' logs (each start rewrites them; 19 Oct 2026: a test death's evidence was lost to the next
# start): EXE\logs\<session>_<name>, the last 10 sessions. A session's stamp is its own start time, shared by its
# screenshot folder (research\experience\shots\<session>) - frames and log of one test pair up (user, 19 Oct 2026:
# read the ongoing session's log beside its frames; was: archived under the NEXT start's time). The current session's
# stamp: EXE\logs\current-session.txt; its log is the live EXE\FLController.log.
$logDir = Join-Path $exeDir 'logs'
$stamp = Get-Date -Format 'yyyy-MM-dd_HHmmss'
$null = New-Item -ItemType Directory -Force $logDir
$sessionFile = Join-Path $logDir 'current-session.txt'
$prev = if (Test-Path $sessionFile) { (Get-Content $sessionFile -TotalCount 1).Trim() } else { '' }
if (-not $running) {   # a hot reload keeps its session (and its live log)
    foreach ($n in 'FLController.log', 'FLSubtitles.log') {
        $l = Join-Path $exeDir $n
        if (Test-Path -LiteralPath $l) {
            $name = if ($prev) { $prev } else { (Get-Item -LiteralPath $l).LastWriteTime.ToString('yyyy-MM-dd_HHmmss') }
            Copy-Item -LiteralPath $l (Join-Path $logDir "${name}_$n") -Force
        }
    }
    if (-not $NoStart) { [IO.File]::WriteAllText($sessionFile, $stamp) }
}
# Test evidence is kept (user, 20 Oct 2026: no pruning for now - logs and screenshots are small and may explain a
# regression later; the passing M04 run's log was lost to the old 20-file limit, the one needed to diagnose a death):
# session logs (EXE\logs), run folders (research\experience\runs), screenshot sessions with their videos
# (research\experience\shots). Instead each start reports their size and warns above $warnGB, for the user to decide.
$warnGB = 20
$evidence = @($logDir, (Join-Path $root 'research\experience\runs'), (Join-Path $root 'research\experience\shots')) |
    Where-Object { Test-Path $_ }
$evidenceGB = (($evidence | ForEach-Object { Get-ChildItem $_ -Recurse -File -ErrorAction SilentlyContinue } |
                Measure-Object Length -Sum).Sum) / 1GB
if ($evidenceGB -gt $warnGB) { "WARNING: test evidence (logs, runs, shots) is {0:N1} GB (> {1} GB): ask the user about pruning" -f $evidenceGB, $warnGB }
if ($Background) {
    # A background test start: FLVIBED_BACKGROUND=1 in the game's environment only (never stored, the user's own starts
    # are unaffected): the host (core\host\focus.cpp, any module) keeps it running out of focus, keeps its window from taking the focus and
    # mutes its sound in the Windows mixer (inside the game all sound runs as always). Plus an inactive first window.
    $env:FLVIBED_BACKGROUND = '1'
    try { $proc = Get-Process -Id ($(if ($GameArgs) { [FLVWin]::StartInactive($flExe, $exeDir, $GameArgs) } else { [FLVWin]::StartInactive($flExe, $exeDir) })) } finally { Remove-Item Env:FLVIBED_BACKGROUND }
    # Standing order (user, 19 Oct 2026): every unattended test runs the screenshot sampler (tools\flshots.ps1), one per
    # game session (a sampler left from the last session is stopped: two captured the same window), into
    # research\experience\shots\<time>; kept, frames and videos (user, 20 Oct 2026: no pruning for now - size warning above).
    $shotRoot = Join-Path $root 'research\experience\shots'
    # this instance's sampler only (parallel test games: one sampler per game, -Instance in its command line)
    $mine = if ($hasInst) { "-Instance $([regex]::Escape($Instance))(\s|$)" } else { '^(?!.*-Instance [1-9a-zA-Z])' }
    $samplers = @(Get-CimInstance Win32_Process -Filter "Name='pwsh.exe'" | Where-Object { $_.CommandLine -match 'flshots\.ps1' -and $_.CommandLine -match $mine })
    if ($samplers.Count) {   # asked to stop (its video file finished), killed only if it has not ended within 10 s
        $null = New-Item -ItemType Directory -Force $shotRoot
        $stopFile = Join-Path $shotRoot $(if ($hasInst) { "sampler$Instance.stop" } else { 'sampler.stop' })
        [IO.File]::WriteAllText($stopFile, 'stop')
        $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($sw.Elapsed.TotalSeconds -lt 10 -and @($samplers | Where-Object { Get-Process -Id $_.ProcessId -ErrorAction SilentlyContinue }).Count) {
            Start-Sleep -Milliseconds 200
        }
        $samplers | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
        [IO.File]::Delete($stopFile)
    }
    # a stop request left from an interrupted deploy would end the new sampler at once (instance 3: none from 24 to 28
    # Oct 2026); the sampler also ignores one older than itself
    $leftover = Join-Path $shotRoot $(if ($hasInst) { "sampler$Instance.stop" } else { 'sampler.stop' })
    if ([IO.File]::Exists($leftover)) { [IO.File]::Delete($leftover) }
    $shotDir = Join-Path $shotRoot $(if ($hasInst) { "${stamp}_i$Instance" } else { $stamp })
    Start-Process pwsh -WindowStyle Hidden -ArgumentList (@('-NoProfile', '-File', ('"' + (Join-Path $PSScriptRoot 'flshots.ps1') + '"'),
        '-Dir', ('"' + $shotDir + '"')) + $(if ($hasInst) { @('-Instance', "$Instance") } else { @() })) # quoted: spaces
} else {
    $proc = if ($GameArgs) { Start-Process -FilePath $flExe -WorkingDirectory $exeDir -ArgumentList $GameArgs -PassThru }
            else { Start-Process -FilePath $flExe -WorkingDirectory $exeDir -PassThru }
}
# Skips the playing startup movie natively: the subtitles host reports it finished to the game's movie loop
# (`movieskip`; no key input, works out of focus). Without the subtitles module (no movie hooks): one Escape message.
function Skip-Movie {
    if ($on.subtitles) { try { if ((Send-FLS 'movieskip' 1000) -match '^skipping') { return } } catch {} }
    [void][FLVWin]::PostMessage($proc.MainWindowHandle, 0x100, [IntPtr]0x1B, [IntPtr]0x00010001)
    [void][FLVWin]::PostMessage($proc.MainWindowHandle, 0x101, [IntPtr]0x1B, [IntPtr]0xC0010001)
}
# The subtitles logic's frame count; a short stall (loading) is waited out, up to 60 s (Send-FLS throws a
# TimeoutException when the game does not answer in time, and a plain error when it exited).
function Get-Frames {
    $t = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        try { return [int]([regex]::Match((Send-FLS 'state' 300), '(?m)^frames (\d+)').Groups[1].Value) }
        catch [System.TimeoutException] { if ($t.Elapsed.TotalSeconds -gt 60) { throw } }
    }
}
# Ready = the game's main loop runs (the frame hook; startup movies run their own loop). The subtitles logic loads at
# the first movie, so its `state` reports `frames`; the controller logic loads at the first frame, so its answer means
# the same. -Intro returns as soon as subtitles answer (movie tests wait on subtitle lines while it plays). Without
# -Intro each movie is skipped (Skip-Movie), only while a movie window exists.
$sw = [Diagnostics.Stopwatch]::StartNew()
$escaped = 0
$ready = $false
while ($sw.Elapsed.TotalSeconds -lt 300) {
    try {
        if ($Intro -and $on.subtitles -and (Send-FLS 'ping' 300) -eq 'pong') { "game started, subtitles responding after {0:n1}s" -f $sw.Elapsed.TotalSeconds; $ready = $true; break }
        if ($on.subtitles -and (Get-Frames) -gt 0) { "main menu after {0:n1}s ($escaped movies skipped)" -f $sw.Elapsed.TotalSeconds; $ready = $true; break }
        if (-not $on.subtitles -and $on.controller -and (Send-FL 'ping' 300) -eq 'pong') { "main menu after {0:n1}s ($escaped movies skipped)" -f $sw.Elapsed.TotalSeconds; $ready = $true; break }
    } catch {}
    $proc.Refresh()
    if ($proc.HasExited) { throw "the game exited during its start (exit code $(try { $proc.ExitCode } catch { '?' })) - see EXE\FLSubtitles.log / FLController.log" }
    if (-not $Intro -and $proc.MainWindowHandle -ne [IntPtr]::Zero -and [FLVWin]::MoviePlaying($proc.MainWindowHandle)) {
        Skip-Movie; $escaped++
        while ([FLVWin]::MoviePlaying($proc.MainWindowHandle) -and $sw.Elapsed.TotalSeconds -lt 300) { Start-Sleep -Milliseconds 50 }
    }
    Start-Sleep -Milliseconds 50
}
# fail closed (29 Oct 2026: the loop ran out silently and the next step failed on a game that never got ready; no
# module on = nothing to ask, the old pass-through)
if (-not $ready -and ($on.subtitles -or $on.controller)) { throw "the game did not reach the main menu in 300 s (no module answered)" }
if (-not $Intro -and $on.subtitles) {
    # Safety net (state, not time): once the menu has run 30 frames, any input posted during the movies has been
    # handled; an Exit dialog then would be ours (ExitButtonBox1 = No, clicked natively).
    $f0 = Get-Frames
    while ((Get-Frames) -lt $f0 + 30) { Start-Sleep -Milliseconds 50 }
    if ((Send-FLS 'visible ExitButtonBox1') -match '^visible') { $null = Send-FLS 'click ExitButtonBox1'; 'closed an Exit dialog opened by a late Escape' }
}
if ($saveName) {
    if ($on.controller) {
        'loaded ' + $saveName + ': ' + (Load-FLSave $saveName)
    } else {
        # subtitles module alone: the same native steps through its channel
        $r = Send-FLS 'menu load'; if ($r -notmatch '^clicked') { throw "main menu: $r" }
        $t = [Diagnostics.Stopwatch]::StartNew()
        while ((Send-FLS 'saves') -match 'no load screen open') { if ($t.Elapsed.TotalSeconds -gt 15) { throw 'load screen did not open' }; Start-Sleep -Milliseconds 50 }
        $f0 = Get-Frames
        $r = Send-FLS "loadsave $saveName"; if ($r -notmatch '^loading') { throw $r }
        # loaded = frames past the request, load screen closed, and in a base with its room built or in space
        while ($true) {
            if ($t.Elapsed.TotalSeconds -gt 60) { throw 'load did not finish' }
            $st = Send-FLS 'state'
            $base = [regex]::Match($st, '(?m)^base (\d+)').Groups[1].Value; $ship = [regex]::Match($st, '(?m)^player_ship (\d+)').Groups[1].Value
            if ((Get-Frames) -ge $f0 + 3 -and (Send-FLS 'saves') -match 'no load screen open' -and
                (($base -ne '0' -and (Send-FLS 'hotspots') -match 'hotspots [1-9]') -or ($base -eq '0' -and $ship -ne '0'))) { break }
            Start-Sleep -Milliseconds 50
        }
        "loaded $saveName ($(if ($base -ne '0') { 'docked' } else { 'space' }))"
    }
}
