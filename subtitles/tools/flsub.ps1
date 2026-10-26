# FLSubtitles test/debug harness. Dot-source this file (in the PowerShell tool), then:
#   Send-FLS "state"            -> sends a debug command to the running game, returns the reply
#   Get-FLSLog [-Tail 40]        -> the end of EXE\FLSubtitles.log
#   Save-FLShot <name>           -> screenshot of the game window into the scratch folder given by $env:FLS_SHOTS
# Debug commands: see src\impl\impl.cpp (state, config, set, trace, test, say, chat, clear, reloaddata).
# Shared-memory layout must match src\shm.h.

$script:FLSShmName = "Local\FLSubtitlesShm"
# FLvibed root = two levels above this script (subtitles\tools).
$script:FLSRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
. (Join-Path $script:FLSRoot 'tools\fldata.ps1')   # the install of this game instance (FLVIBED_INSTANCE), Get-FLGameProcess
$script:FLSExe = Join-Path (Get-FLInstallDir) 'EXE'

# One command slot per game: serialized by a named mutex per channel (22 Oct 2026, as Send-FL: concurrent clients
# overwrote each other's command).
function Send-FLS([string]$Command, [int]$TimeoutMs = 5000) {
    $name = if ($env:FLVIBED_INSTANCE) { "$($script:FLSShmName)_$($env:FLVIBED_INSTANCE)" } else { $script:FLSShmName }
    $mx = [System.Threading.Mutex]::new($false, 'Local\FLvibedCmd_' + ($name -replace '^.*\\', ''))
    $owned = $false
    try { $owned = $mx.WaitOne($TimeoutMs + 1000) } catch [System.Threading.AbandonedMutexException] { $owned = $true }
    try { Send-FLSUnlocked $Command $TimeoutMs $name } finally { if ($owned) { $mx.ReleaseMutex() }; $mx.Dispose() }
}
function Send-FLSUnlocked([string]$Command, [int]$TimeoutMs, [string]$name) {
    try { $mmf = [System.IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting($name) }
    catch { throw "FLSubtitles shared memory not found (is the game running with FLvibed.dll and the subtitles module on? FLvibed.ini [Modules])" }
    $v = $mmf.CreateViewAccessor()
    try {
        if ($v.ReadUInt32(0) -ne 0x42534C46) { throw "bad magic" }
        $bytes = [Text.Encoding]::UTF8.GetBytes($Command + [char]0)
        $v.WriteArray(20, $bytes, 0, [Math]::Min($bytes.Length, 1023))
        $seq = $v.ReadUInt32(12) + 1
        $v.Write(12, [uint32]$seq)
        $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($v.ReadUInt32(16) -ne $seq) {
            if ($sw.ElapsedMilliseconds -gt $TimeoutMs) {
                # The game exited (ends every wait), or did not process a frame in time (TimeoutException: loading, a
                # stall). Was: "<timeout: ...>" returned as if it were the answer, 18 Oct 2026.
                if (-not (Get-FLGameProcess)) { throw "Freelancer exited (no answer to '$Command')" }
                throw [System.TimeoutException]::new("no answer to '$Command' within $TimeoutMs ms (game loading, stalled or hung)")
            }
            Start-Sleep -Milliseconds 5
        }
        $buf = New-Object byte[] 61440
        [void]$v.ReadArray(1044, $buf, 0, $buf.Length)
        $len = [Array]::IndexOf($buf, [byte]0); if ($len -lt 0) { $len = $buf.Length }
        return [Text.Encoding]::UTF8.GetString($buf, 0, $len)
    } finally { $v.Dispose(); $mmf.Dispose() }
}

# Near-real-time game watcher (needs FLController in the same install for state, UI and events).
# Polls every 250 ms and returns as soon as something needs attention, instead of sleeping on a timer:
#   [pscustomobject]@{ Reason; Detail; Mode; Seconds }
# Reasons: 'until' (the -Until scriptblock returned true; it gets the state line), 'modal' (a tutorial
# popup / dialog appeared: its buttons are named in Detail), 'mode' (mode changed, e.g. cutscene -> docked;
# only with -StopOnMode), 'event' (an event matching -EventPattern: popup, FAIL, SUCCESS, death by default),
# 'timeout'.
# -LinePattern: also return ('line') when FLSubtitles shows a subtitle / chat line matching it (regex on
# "<seq> <ms> <sub|chat> <category> | <speaker> | <text>"); -ShotMs takes a screenshot that long after the
# line appeared (Detail then ends with " => <png>"). -Modal:$false ignores dialogs (when one is expected).
function Watch-FLGame([scriptblock]$Until = $null, [int]$TimeoutSec = 120, [switch]$StopOnMode,
                      [string]$EventPattern = 'popup|FAILURE|SUCCESS|death|mission-message type=FAILURE',
                      [string]$LinePattern = '', [int]$ShotMs = -1, [bool]$Modal = $true, [string]$ShotName = 'line') {
    $ctl = Join-Path $script:FLSRoot 'controller\tools\flctl.ps1'
    . $ctl
    $after = [int](((Send-FL 'events 999999999') -split "`n")[0] -replace '^last ', '')
    $shownAfter = [int](((Send-FLS 'shown 999999999') -split "`n")[0] -replace '^last ', '')
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $mode0 = $null
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
      # A command the game did not answer in time (loading, a stall) = nothing to report yet; the game exiting throws.
      try {
        if ($LinePattern) {
            $sh = (Send-FLS "shown $shownAfter") -split "`n"
            $shownAfter = [int]($sh[0] -replace '^last ', '')
            $hitLine = $sh | Select-Object -Skip 1 | Where-Object { $_ -and $_ -match $LinePattern } | Select-Object -First 1
            if ($hitLine) {
                if ($ShotMs -ge 0) { Start-Sleep -Milliseconds $ShotMs; $hitLine += ' => ' + (Save-FLShot $ShotName) }
                return [pscustomobject]@{ Reason = 'line'; Detail = $hitLine; Mode = ''; Seconds = [math]::Round($sw.Elapsed.TotalSeconds, 1) }
            }
        }
        $state = ((Send-FL 'state') -split "`n")[0]
        $mode = ($state -replace '^mode=(\S+).*', '$1')
        if (-not $mode0) { $mode0 = $mode }
        $t = [math]::Round($sw.Elapsed.TotalSeconds, 1)
        if ($Until -and (& $Until $state)) { return [pscustomobject]@{ Reason = 'until'; Detail = $state; Mode = $mode; Seconds = $t } }
        # modal windows: a tutorial popup's OK button or a dialog's buttons among the hit-testable controls
        $ui = (Send-FL 'uiitems') -split "`n" | Where-Object { $_ -match '^(MMOkButton|NN_\w+|\w*Popup\w*|\w*Dialog\w*|Offer\w+|OkButton|YesButton|NoButton|ExitButtonBox\d|PauseButtonBox\d|LoadSave_\w+|NavMapContinueButton)\s' }
        if ($ui -and $Modal) { return [pscustomobject]@{ Reason = 'modal'; Detail = (($ui | ForEach-Object { ($_ -split '\s+')[0] }) -join ','); Mode = $mode; Seconds = $t } }
        $ev = (Send-FL "events $after") -split "`n"
        $after = [int]($ev[0] -replace '^last ', '')
        $hit = $ev | Select-Object -Skip 1 | Where-Object { $_ -match $EventPattern }
        if ($hit) { return [pscustomobject]@{ Reason = 'event'; Detail = ($hit -join ' | '); Mode = $mode; Seconds = $t } }
        if ($StopOnMode -and $mode -ne $mode0) { return [pscustomobject]@{ Reason = 'mode'; Detail = "$mode0 -> $mode"; Mode = $mode; Seconds = $t } }
      } catch [System.TimeoutException] {}
        Start-Sleep -Milliseconds 250
    }
    [pscustomobject]@{ Reason = 'timeout'; Detail = ''; Mode = $mode; Seconds = $TimeoutSec }
}

# Closes the modal window the watcher reported with the button a player would use to dismiss it, clicked natively
# (the game's own button logic, no cursor, works in flight): tutorial popups (MMOkButton), the mission briefing
# shown on launch (nav map: Continue), rumour / offer windows (close), the Exit dialog (No). Returns its name, ''
# if none is open; throws if the button is not natively clickable (never presses keys blindly).
function Close-FLModal {
    foreach ($b in 'MMOkButton', 'NavMapContinueButton', 'OfferDialogClose2Button', 'OfferDialogCloseButton', 'ExitButtonBox1') {
        if ((Send-FLS "visible $b") -match '^visible') {
            $r = Send-FLS "click $b"
            if ($r -notmatch '^clicked') { throw "Close-FLModal: $r" }
            return $b
        }
    }
    ''
}

# The newest line number of the shown-lines feed: take it BEFORE triggering a line, pass it to Wait-FLSShown -After.
function Get-FLSLast { [int](((Send-FLS 'shown 999999999') -split "`n")[0] -replace '^last ', '') }

# Waits for the next subtitle / chat line matching $Pattern that FLSubtitles shows: lines after -After (from
# Get-FLSLast, taken before triggering the line, so a line that appears at once is not missed), else only lines
# shown after the call starts. Works during movies, where the controller's channel is idle, and across a game
# restart, where the sequence starts again at 1. -ShotMs takes a screenshot that long after it appears. Returns the
# line (+ " => <png>") or 'timeout'.
function Wait-FLSShown([string]$Pattern, [int]$TimeoutSec = 60, [int]$ShotMs = -1, [string]$Shot = 'line', [int]$After = -1) {
    $after = $After
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        try {
            $r = (Send-FLS "shown $([Math]::Max($after, 0))" 300) -split "`n"
            if ($r[0] -match '^last (\d+)') {
                $last = [int]$Matches[1]
                if ($after -lt 0 -or $last -lt $after) { $after = $last; continue } # first look / game restarted
                $after = $last
                $hit = $r | Select-Object -Skip 1 | Where-Object { $_ -and $_ -match $Pattern } | Select-Object -First 1
                if ($hit) {
                    if ($ShotMs -ge 0) { Start-Sleep -Milliseconds $ShotMs; $hit += ' => ' + (Save-FLShot $Shot) }
                    return $hit
                }
            }
        } catch {}
        Start-Sleep -Milliseconds 150
    }
    'timeout'
}

# Chooses the startup movies of this project's test install (EXE\freelancer.ini [Startup] movie_file) for
# testing intro subtitles without replacing any video: -Intro <file in DATA\MOVIES> (e.g. fl_intro_vanilla.wmv,
# fl_intro_full.wmv), -NoLogos skips the two logo movies, -Restore puts the install's original lines back.
function Set-FLSIntro([string]$Intro = '', [switch]$NoLogos, [switch]$Restore) {
    $ini = Join-Path $script:FLSExe 'freelancer.ini'
    $backup = "$ini.flsub-backup"
    if ($Restore) {
        if (Test-Path $backup) { Copy-Item $backup $ini -Force; Remove-Item $backup; 'restored' } else { 'nothing to restore' }
        return
    }
    if (-not (Test-Path $backup)) { Copy-Item $ini $backup }
    $lines = Get-Content $backup | ForEach-Object {
        if ($_ -match '^\s*movie_file\s*=\s*movies\\(MGS|DA)_Logo' -and $NoLogos) { ";$_" }
        elseif ($_ -match '^\s*movie_file\s*=\s*movies\\FL_Intro\.wmv' -and $Intro) { "movie_file = movies\$Intro" }
        else { $_ }
    }
    Set-Content $ini $lines -Encoding ascii
    (Select-String -Path $ini -Pattern 'movie_file').Line
}

function Get-FLSLog([int]$Tail = 40) { Get-Content (Join-Path $script:FLSExe 'FLSubtitles.log') -Tail $Tail }

# Captures the game window (client area, physical pixels) to a PNG; works while it is not in front
# only if it is not covered. Returns the file path.
function Save-FLShot([string]$Name = 'shot', [string]$Dir = $env:FLS_SHOTS) {
    if (-not $Dir) { $Dir = Join-Path $env:TEMP 'flsub-shots' }
    New-Item -ItemType Directory -Force $Dir | Out-Null
    Add-Type -AssemblyName System.Drawing
    if (-not ('FLSWin' -as [type])) {
        Add-Type @'
using System; using System.Runtime.InteropServices;
public static class FLSWin {
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  public struct RECT { public int L, T, R, B; }
}
'@
    }
    [void][FLSWin]::SetProcessDpiAwarenessContext([IntPtr](-4))
    $p = Get-FLGameProcess; if (-not $p) { throw 'Freelancer is not running' }
    $h = $p.MainWindowHandle
    # The game window's own content (PrintWindow, PW_CLIENTONLY | PW_RENDERFULLCONTENT: DirectX content through the
    # compositor), never the screen area: the game may be behind the user's windows (background tests), and a screen
    # copy would capture the user's desktop. No capture rather than the wrong one.
    # The client size in the window's own DPI context: PrintWindow draws a DPI-unaware game (vanilla 1.1) at its real
    # (logical) size, while a per-monitor-aware caller reads its client rect scaled (200 %: a 2x bitmap with the picture
    # in its top-left quarter, 26 Oct 2026). A DPI-aware game (HDE) reads the same either way.
    $old = [FLSWin]::SetThreadDpiAwarenessContext([FLSWin]::GetWindowDpiAwarenessContext($h))
    try {
        $r = New-Object FLSWin+RECT
        if (-not [FLSWin]::GetClientRect($h, [ref]$r) -or $r.R -le $r.L -or $r.B -le $r.T) { throw 'game window has no client area' }
        $bmp = New-Object Drawing.Bitmap ($r.R - $r.L), ($r.B - $r.T)
        $g = [Drawing.Graphics]::FromImage($bmp)
        $hdc = $g.GetHdc()
        $ok = [FLSWin]::PrintWindow($h, $hdc, 3)
        $g.ReleaseHdc($hdc)
    } finally { if ($old -ne [IntPtr]::Zero) { [void][FLSWin]::SetThreadDpiAwarenessContext($old) } }
    if (-not $ok) { $g.Dispose(); $bmp.Dispose(); throw 'game window capture failed (PrintWindow)' }
    $file = Join-Path $Dir ("{0}-{1:HHmmss}.png" -f $Name, (Get-Date))
    $bmp.Save($file, [Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    return $file
}
