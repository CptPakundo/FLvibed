# FLController harness - the debug channel, virtual pad, frames, log, deadlines and waits. Part of flctl.ps1 (dot-sourced by it; never load a part alone).

function Get-FLView {
    # parallel test games: $env:FLVIBED_INSTANCE (set when deploy.ps1 -Instance <n> started the game) picks its channel
    $name = if ($env:FLVIBED_INSTANCE) { "$($script:FLShmName)_$($env:FLVIBED_INSTANCE)" } else { $script:FLShmName }
    try {
        $mmf = [System.IO.MemoryMappedFiles.MemoryMappedFile]::OpenExisting($name)
    } catch {
        throw "FLController shared memory not found (is the game running with FLvibed.dll and the controller module on? FLvibed.ini [Modules])"
    }
    return @{ Map = $mmf; View = $mmf.CreateViewAccessor() }
}

# No answer to a command: the game exited (an error that ends every wait), or it is alive but did not process a frame
# in time (a TimeoutException: loading, a stall; Wait-FLUntil treats it as "not yet"). Was: the text "<timeout: ...>"
# returned as if it were the answer - callers parsed it as data (frame 0, "not in space", ...), 18 Oct 2026.
function Throw-FLNoAnswer([string]$Command, [int]$TimeoutMs) {
    if (-not (Get-FLGameProcess)) { throw "Freelancer exited (no answer to '$Command')" }
    throw [System.TimeoutException]::new("no answer to '$Command' within $TimeoutMs ms (game loading, stalled or hung)")
}

# One command slot per game: a named mutex per channel serializes every client (22 Oct 2026: a leg runner, the check-ins
# and an interactive session on one game overwrote each other's command - lost and crossed answers, "no answer to
# 'reload' within 5000 ms").
function Send-FL([string]$Command, [int]$TimeoutMs = 5000) {
    # (the channel name already starts with "Local\": one backslash only in a mutex name)
    $chan = if ($env:FLVIBED_INSTANCE) { "$($script:FLShmName)_$($env:FLVIBED_INSTANCE)" } else { $script:FLShmName }
    $mname = 'Local\FLvibedCmd_' + ($chan -replace '^.*\\', '')
    $mx = [System.Threading.Mutex]::new($false, $mname)
    $owned = $false
    try { $owned = $mx.WaitOne($TimeoutMs + 1000) } catch [System.Threading.AbandonedMutexException] { $owned = $true }
    try { Send-FLUnlocked $Command $TimeoutMs } finally { if ($owned) { $mx.ReleaseMutex() }; $mx.Dispose() }
}
function Send-FLUnlocked([string]$Command, [int]$TimeoutMs = 5000) {
    $h = Get-FLView
    try {
        $v = $h.View
        if ($v.ReadUInt32(0) -ne 0x54434C46) { throw "bad magic" }
        $bytes = [Text.Encoding]::ASCII.GetBytes($Command + [char]0)
        $v.WriteArray(48, $bytes, 0, [Math]::Min($bytes.Length, 1023))
        $seq = $v.ReadUInt32(40) + 1
        $v.Write(40, [uint32]$seq)
        $sw = [Diagnostics.Stopwatch]::StartNew()
        while ($v.ReadUInt32(44) -ne $seq) {
            if ($sw.ElapsedMilliseconds -gt $TimeoutMs) { Throw-FLNoAnswer $Command $TimeoutMs }
            Start-Sleep -Milliseconds 5
        }
        $buf = New-Object byte[] 61440
        [void]$v.ReadArray(1072, $buf, 0, $buf.Length)
        $len = [Array]::IndexOf($buf, [byte]0); if ($len -lt 0) { $len = $buf.Length }
        return [Text.Encoding]::UTF8.GetString($buf, 0, $len)
    } finally { $h.View.Dispose(); $h.Map.Dispose() }
}

function Set-FLPad([string[]]$Buttons = @(), [double]$LX = 0, [double]$LY = 0, [double]$RX = 0, [double]$RY = 0,
                   [double]$LT = 0, [double]$RT = 0) {
    $mask = 0
    foreach ($b in $Buttons) {
        if (-not $b) { continue }
        if (-not $script:FLButtons.ContainsKey($b)) { throw "unknown pad button '$b' (valid: $($script:FLButtons.Keys -join ', '))" }
        $mask = $mask -bor $script:FLButtons[$b]
    }
    $h = Get-FLView
    try {
        $v = $h.View
        $v.Write(16, [uint32]$mask)
        $trig = ([int]([Math]::Round($LT * 255)) -band 0xFF) -bor (([int]([Math]::Round($RT * 255)) -band 0xFF) -shl 8)
        $v.Write(20, [uint32]$trig)
        $v.Write(24, [int32]([Math]::Round($LX * 32767)))
        $v.Write(28, [int32]([Math]::Round($LY * 32767)))
        $v.Write(32, [int32]([Math]::Round($RX * 32767)))
        $v.Write(36, [int32]([Math]::Round($RY * 32767)))
        $v.Write(12, [uint32]1)
    } finally { $h.View.Dispose(); $h.Map.Dispose() }
}

function Clear-FLPad {
    $h = Get-FLView
    try {
        $v = $h.View
        foreach ($o in 16, 20, 24, 28, 32, 36) { $v.Write($o, [uint32]0) }
        $v.Write(12, [uint32]0)
    } finally { $h.View.Dispose(); $h.Map.Dispose() }
}

# A button press by game state: held until the controller logic has read it down in -HoldFrames frames (it reads the
# pad once per frame and acts on edges), then released until it has read it up in 2 frames. Was: 120 ms + 80 ms of
# wall clock - a slow frame (loading, out of focus) could miss the press, 18 Oct 2026. -HoldFrames > 2 for long holds.
function Press-FLPad([string[]]$Buttons, [int]$HoldFrames = 2) {
    Set-FLPad -Buttons $Buttons
    $null = Wait-FLFramesPassed $HoldFrames
    Set-FLPad -Buttons @()
    $null = Wait-FLFramesPassed 2
}

# Waits until the game has processed $Frames more frames (state, not time; safety limit $MaxSec). $false on the limit.
function Wait-FLFramesPassed([int]$Frames, [double]$MaxSec = 10) {
    $f0 = Get-FLFrame
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $MaxSec) {
        try { $f = Get-FLFrame } catch [System.TimeoutException] { continue }
        if ($f -lt $f0) { $f0 = $f }   # the frame count starts over after a logic hot reload
        if ($f -ge $f0 + $Frames) { return $true }
        Start-Sleep -Milliseconds 5   # polling interval
    }
    $false
}

function Get-FLLog([int]$Tail = 40) {
    Get-Content (Join-Path $script:FLRoot 'Install\Freelancer HD Edition\EXE\FLController.log') -Tail $Tail
}

# Polls a game-state condition (scriptblock returning a truthy value) every 50 ms; returns that value, or throws
# "<What> not reached" after -TimeoutSec, which is only a safety limit (tests wait on state, not on time).
# Test watchdog (user, 20 Oct 2026: a section going badly must be flagged at once, not finish quietly): while a deadline
# is set (Set-FLDeadline, the sweep sets one per section from its budget), every wait throws once it has passed.
if (-not (Test-Path variable:global:FLDeadline)) { $global:FLDeadline = $null } # global: mission scripts load this file in their own scope
function Set-FLDeadline([double]$Seconds, [string]$Why = 'budget') {
    $global:FLDeadline = if ($Seconds -gt 0) { @{ at = (Get-Date).AddSeconds($Seconds); why = $Why; sec = $Seconds } } else { $null }
}
function Wait-FLUntil([scriptblock]$Condition, [string]$What, [double]$TimeoutSec = 30) {
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $silent = $null
    do {
        if ($global:FLDeadline -and (Get-Date) -gt $global:FLDeadline.at) {
            throw "OVER BUDGET ($($global:FLDeadline.why) $([int]$global:FLDeadline.sec) s) while waiting for $What"
        }
        # A command the game did not answer in time (loading, a stall) means "not yet"; the game exiting ends the wait.
        try { $r = & $Condition; $silent = $null } catch [System.TimeoutException] { $r = $null; $silent = $_.Exception.Message }
        if ($r) { return $r }
        Start-Sleep -Milliseconds 50
    } while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec)
    $state = if ($silent) { "game not answering ($silent)" } else { try { Get-FLStoryState } catch { 'game not answering' } }
    throw "$What not reached after $TimeoutSec s. Game state: $state"
}
