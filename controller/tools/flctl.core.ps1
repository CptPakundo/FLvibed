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
# Harness dry-runs (WORK_QUEUE H16, 30 Oct 2026; Q2b's retro: each of the new crawler's six harness bugs cost a game
# boot): $env:FLV_RECORD = <file> writes every command and its reply (and every pad state) of a run; $env:FLV_REPLAY =
# <file> answers from it with no game. A recording is cut into epochs at each ACTION (a pad state, or a command that
# is not a read); within an epoch a read returns its last recorded reply up to there - so a driver's polling may differ
# in count - and `state` counts its frame up by one per call (the waits see frames pass). An action that differs from
# the recording's next one ends the replay: "REPLAY DIVERGED at action <n>". Lines: kind (R / A) TAB epoch TAB command
# TAB reply (base64).
$script:FLReadRx = '^(state|uiitems|menus|padhud|hit|ship|pilot|ap status|ap situation|timing|gameclock|hotspots|popups|dialogs|focus|visible|events|talkers|sdlpad status|haptics status|haptics table)\b'
function Add-FLRecord([string]$Cmd, [string]$Reply) {
    if (-not (Test-Path variable:script:FLRecEpoch)) { $script:FLRecEpoch = 0 }
    $read = $Cmd -match $script:FLReadRx
    $line = "{0}`t{1}`t{2}`t{3}" -f $(if ($read) { 'R' } else { 'A' }), $script:FLRecEpoch, ($Cmd -replace '[\t\r\n]', ' '),
        [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($Reply))
    Add-Content -LiteralPath $env:FLV_RECORD -Value $line
    if (-not $read) { $script:FLRecEpoch++ }
}
function Invoke-FLReplay([string]$Cmd) {
    if (-not $script:FLRep -or $script:FLRep.file -ne $env:FLV_REPLAY) {
        $rows = foreach ($l in Get-Content -LiteralPath $env:FLV_REPLAY) {
            $p = $l -split "`t", 4
            [pscustomobject]@{ kind = $p[0]; epoch = [int]$p[1]; cmd = $p[2]; reply = [Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($p[3])) }
        }
        $byCmd = @{}
        foreach ($x in $rows) { if ($x.kind -eq 'R') { if (-not $byCmd.ContainsKey($x.cmd)) { $byCmd[$x.cmd] = [Collections.Generic.List[object]]::new() }; $byCmd[$x.cmd].Add($x) } }
        $script:FLRep = @{ file = $env:FLV_REPLAY; reads = $byCmd; acts = @($rows | Where-Object kind -eq 'A'); epoch = 0; frame = 1000 }
    }
    $r = $script:FLRep
    $c = $Cmd -replace '[\t\r\n]', ' '
    if ($c -notmatch $script:FLReadRx) {
        $want = if ($r.epoch -lt $r.acts.Count) { $r.acts[$r.epoch] } else { $null }
        if (-not $want -or $want.cmd -ne $c) {
            throw "REPLAY DIVERGED at action $($r.epoch): recorded '$(if ($want) { $want.cmd } else { '<end of recording>' })', the driver sent '$c'"
        }
        $r.epoch++
        return $want.reply
    }
    $hit = $null
    if ($r.reads.ContainsKey($c)) { $l = $r.reads[$c]; for ($k = $l.Count - 1; $k -ge 0; $k--) { if ($l[$k].epoch -le $r.epoch) { $hit = $l[$k]; break } } }
    if (-not $hit) { throw "REPLAY: no recorded reply to '$c' up to action $($r.epoch)" }
    if ($c -eq 'state') { $r.frame++; return ($hit.reply -replace 'frame=\d+', "frame=$($r.frame)") }
    $hit.reply
}

function Send-FL([string]$Command, [int]$TimeoutMs = 5000) {
    if ($env:FLV_REPLAY) { return (Invoke-FLReplay $Command) }
    if ($env:FLV_RECORD) { $rep = Send-FLLocked $Command $TimeoutMs; Add-FLRecord $Command $rep; return $rep }
    Send-FLLocked $Command $TimeoutMs
}
function Send-FLLocked([string]$Command, [int]$TimeoutMs = 5000) {
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
    # (a pad state is an action of the dry-run recording: H16 above)
    $padAct = "pad $mask $LX $LY $RX $RY $LT $RT"
    if ($env:FLV_REPLAY) { $null = Invoke-FLReplay $padAct; return }
    if ($env:FLV_RECORD) { Add-FLRecord $padAct '' }
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

# ---- Device-layer test pads (shm v2, 30 Oct 2026, IDEAS T36 slice 1; controller\src\shm.h). While enabled, the game's
# XInput slots 0-3 are served from here instead of the system's XInput: a test pad takes a real pad's whole path (the
# slot scan with its 1.5 s rescan, unplug / replug, the focus gate, the glyph family from -Vendor, rumble read back by
# Get-FLDevRumble). The virtual pad above (Set-FLPad) stays the focus-free harness hand. A real pad is ignored while on.
# Offsets: shm.h's static_asserts (devActive 62512, devSlot 62520 + 36 per slot, devRumbleSeq 62664, devRumble 62668).
$script:FLDevVendors = @{ xbox = 0x045E; playstation = 0x054C; nintendo = 0x057E }
function Use-FLDevView([scriptblock]$Body) {
    $h = Get-FLView
    try {
        if ($h.View.ReadUInt32(4) -lt 2) { throw "the game's shared memory is v$($h.View.ReadUInt32(4)): device-layer test pads need a build with shm v2 (30 Oct 2026)" }
        & $Body $h.View
    } finally { $h.View.Dispose(); $h.Map.Dispose() }
}
# -Foreground: the focus gate sees the game as in front (the physical pad's gate open in a background game).
function Enable-FLDevPads([switch]$Foreground) {
    Use-FLDevView { param($v) $v.Write(62516, [uint32]$(if ($Foreground) { 1 } else { 0 })); $v.Write(62512, [uint32]1) }
}
function Disable-FLDevPads {
    Use-FLDevView { param($v)
        $v.Write(62512, [uint32]0); $v.Write(62516, [uint32]0)
        for ($o = 62520; $o -lt 62664; $o += 4) { $v.Write($o, [uint32]0) }
    }
}
# One slot's state: -Connected:$false unplugs it; the stick / trigger / button values replace the slot's whole state.
function Set-FLDevPad([ValidateRange(0, 3)][int]$Slot = 0, [bool]$Connected = $true, [string[]]$Buttons = @(),
                      [double]$LX = 0, [double]$LY = 0, [double]$RX = 0, [double]$RY = 0, [double]$LT = 0, [double]$RT = 0,
                      [ValidateSet('xbox', 'playstation', 'nintendo')][string]$Vendor = 'xbox') {
    $mask = 0
    foreach ($b in $Buttons) {
        if (-not $b) { continue }
        if (-not $script:FLButtons.ContainsKey($b)) { throw "unknown pad button '$b' (valid: $($script:FLButtons.Keys -join ', '))" }
        $mask = $mask -bor $script:FLButtons[$b]
    }
    Use-FLDevView { param($v)
        $o = 62520 + 36 * $Slot
        $v.Write($o + 4, [uint32]$mask)
        $trig = ([int]([Math]::Round($LT * 255)) -band 0xFF) -bor (([int]([Math]::Round($RT * 255)) -band 0xFF) -shl 8)
        $v.Write($o + 8, [uint32]$trig)
        $v.Write($o + 12, [int32]([Math]::Round($LX * 32767))); $v.Write($o + 16, [int32]([Math]::Round($LY * 32767)))
        $v.Write($o + 20, [int32]([Math]::Round($RX * 32767))); $v.Write($o + 24, [int32]([Math]::Round($RY * 32767)))
        $v.Write($o + 28, [uint32]$script:FLDevVendors[$Vendor])
        $v.Write($o + 32, [uint32]($v.ReadUInt32($o + 32) + 1))
        $v.Write($o, [uint32]$(if ($Connected) { 1 } else { 0 }))
    }
}
# A press on a test pad by game state (as Press-FLPad): held until read down in -HoldFrames frames, then released.
function Press-FLDevPad([string[]]$Buttons, [int]$Slot = 0, [string]$Vendor = 'xbox', [int]$HoldFrames = 2) {
    Set-FLDevPad -Slot $Slot -Buttons $Buttons -Vendor $Vendor
    $null = Wait-FLFramesPassed $HoldFrames
    Set-FLDevPad -Slot $Slot -Vendor $Vendor
    $null = Wait-FLFramesPassed 2
}
# The rumble commands the game sent to the test pads since -Since (a previous call's last Seq): Seq, Slot, Low, High
# (0..1), Frame (the game's heartbeat). The ring holds the last 16.
function Get-FLDevRumble([int]$Since = 0) {
    Use-FLDevView { param($v)
        $n = $v.ReadUInt32(62664)
        for ($s = [Math]::Max($Since + 1, $n - 15); $s -le $n; $s++) {
            $o = 62668 + 20 * (($s - 1) % 16)
            if ($v.ReadUInt32($o) -ne $s) { continue } # overwritten while read
            [pscustomobject]@{ Seq = $s; Slot = $v.ReadUInt32($o + 4); Low = $v.ReadUInt32($o + 8) / 65535.0
                               High = $v.ReadUInt32($o + 12) / 65535.0; Frame = $v.ReadUInt32($o + 16) }
        }
    }
}

# ---- SDL virtual test pads (shm v3, 30 Oct 2026, WORK_QUEUE Q5 slice 2 / IDEAS T36 slice 2; controller\src\shm.h, the
# host's debug `sdlpad`, controller\src\host\sdlpad.cpp). With pad_backend 1 (SDL running: `Send-FL 'set pad_backend 1'`)
# a test pad is attached through SDL's own virtual joystick driver with a real pad's USB ids (xbox 045E:0B12, ps5
# 054C:0CE6, switchpro 057E:2009), so SDL names its type as for the real pad and the pad takes the whole SDL path: the
# type -> the glyph family, buttons, sticks, triggers, hot-plug; what SDL sends to it (rumble, trigger rumble, effects)
# is recorded (Get-FLSdlPadOut). While one is attached the newest is the pad in use (a real pad is ignored). TEST ONLY:
# refused in multiplayer and while SDL is not running. Offsets: shm.h's static_asserts (sdlFlags 62988, sdlAttached
# 62992, sdlSlot 62996 + 32 per slot, sdlOutSeq 63124, sdlOut 63128 + 24 per entry).
function Use-FLSdlView([scriptblock]$Body) {
    $h = Get-FLView
    try {
        if ($h.View.ReadUInt32(4) -lt 3) { throw "the game's shared memory is v$($h.View.ReadUInt32(4)): SDL test pads need a build with shm v3 (30 Oct 2026)" }
        & $Body $h.View
    } finally { $h.View.Dispose(); $h.Map.Dispose() }
}
# One slot's state (XInput form: the same buttons as Set-FLPad, sticks -1..1 with +y up, triggers 0..1); replaces it whole.
function Set-FLSdlPad([ValidateRange(0, 3)][int]$Slot = 0, [string[]]$Buttons = @(),
                      [double]$LX = 0, [double]$LY = 0, [double]$RX = 0, [double]$RY = 0, [double]$LT = 0, [double]$RT = 0) {
    $mask = 0
    foreach ($b in $Buttons) {
        if (-not $b) { continue }
        if (-not $script:FLButtons.ContainsKey($b)) { throw "unknown pad button '$b' (valid: $($script:FLButtons.Keys -join ', '))" }
        $mask = $mask -bor $script:FLButtons[$b]
    }
    Use-FLSdlView { param($v)
        $o = 62996 + 32 * $Slot
        $v.Write($o, [uint32]$mask)
        $trig = ([int]([Math]::Round($LT * 255)) -band 0xFF) -bor (([int]([Math]::Round($RT * 255)) -band 0xFF) -shl 8)
        $v.Write($o + 4, [uint32]$trig)
        $v.Write($o + 8, [int32]([Math]::Round($LX * 32767))); $v.Write($o + 12, [int32]([Math]::Round($LY * 32767)))
        $v.Write($o + 16, [int32]([Math]::Round($RX * 32767))); $v.Write($o + 20, [int32]([Math]::Round($RY * 32767)))
    }
}
# Attach a test pad (its slot's state neutral first). -Foreground: the focus gate sees the game as in front while a test
# pad is attached (the pad's gate open in a background game, as Enable-FLDevPads -Foreground). Returns the host's reply
# ("attached slot=<n> kind=<k> id=<id> vendor= product= type=<SDL's type> (<n>)"); throws on a refusal.
function Connect-FLSdlPad([ValidateSet('xbox', 'ps5', 'switchpro')][string]$Type = 'xbox', [ValidateRange(0, 3)][int]$Slot = 0,
                          [switch]$Foreground) {
    Set-FLSdlPad -Slot $Slot
    Use-FLSdlView { param($v) $v.Write(62988, [uint32]$(if ($Foreground) { 1 } else { 0 })) }
    $r = Send-FL "sdlpad attach $Type $Slot"
    if ($r -notmatch '^attached ') { throw "Connect-FLSdlPad ${Type}: $r" }
    $r
}
# Detach one slot's test pad, or every one (-Slot -1); the foreground flag cleared when none is left.
function Disconnect-FLSdlPad([ValidateRange(-1, 3)][int]$Slot = -1) {
    $r = Send-FL "sdlpad detach $(if ($Slot -lt 0) { 'all' } else { $Slot })"
    if ($r -notmatch '^detached ') { throw "Disconnect-FLSdlPad: $r" }
    foreach ($s in $(if ($Slot -ge 0) { @($Slot) } else { 0..3 })) { Set-FLSdlPad -Slot $s }   # neutral for the next attach
    Use-FLSdlView { param($v) if (-not $v.ReadUInt32(62992)) { $v.Write(62988, [uint32]0) } }
    $r
}
# The focus gate's foreground flag for the attached SDL test pads, on or off while they stay attached (Connect-FLSdlPad
# -Foreground sets it at the attach): off = the game counts as in the background again (a background test of the closed
# gate with the pad attached - the haptics' silence, Q5 slice 3).
function Set-FLSdlForeground([bool]$On = $true) {
    Use-FLSdlView { param($v) $v.Write(62988, [uint32]$(if ($On) { 1 } else { 0 })) }
}
# The host's view: "sdlpad status=<backend: -1 failed, 0 off, 1 starting, 2 running> in_use=<slot|-1> attached=0x<mask>
# out_seq=<n> slot<n>=<kind>/id<id> ..."
function Get-FLSdlPad { Send-FL 'sdlpad status' }
# A press on a test pad by game state (as Press-FLPad): held until read down in -HoldFrames frames, then released.
function Press-FLSdlPad([string[]]$Buttons, [int]$Slot = 0, [int]$HoldFrames = 2) {
    Set-FLSdlPad -Slot $Slot -Buttons $Buttons
    $null = Wait-FLFramesPassed $HoldFrames
    Set-FLSdlPad -Slot $Slot
    $null = Wait-FLFramesPassed 2
}
# What SDL sent to the test pads since -Since (a previous call's last Seq): Seq, Slot, Kind (rumble | triggers |
# effect), A / B (rumble: low / high motor 0..1; triggers: left / right 0..1; effect: A = its bytes), Frame (the game's
# heartbeat). The ring holds the last 32. SDL resends a running rumble every 2 s and sends the stop itself when its
# duration ends (both appear here).
function Get-FLSdlPadOut([int]$Since = 0) {
    Use-FLSdlView { param($v)
        $n = $v.ReadUInt32(63124)
        for ($s = [Math]::Max($Since + 1, $n - 31); $s -le $n; $s++) {
            $o = 63128 + 24 * (($s - 1) % 32)
            if ($v.ReadUInt32($o) -ne $s) { continue } # overwritten while read
            $kind = $v.ReadUInt32($o + 8)
            $scale = if ($kind -eq 3) { 1.0 } else { 65535.0 }
            [pscustomobject]@{ Seq = $s; Slot = $v.ReadUInt32($o + 4); Kind = @('?', 'rumble', 'triggers', 'effect')[[Math]::Min($kind, 3)]
                               A = $v.ReadUInt32($o + 12) / $scale; B = $v.ReadUInt32($o + 16) / $scale; Frame = $v.ReadUInt32($o + 20) }
        }
    }
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

# The game's own boxes over the flow - M01's tutorial boxes (Act_PopupDialog: "Basics of combat" mid-fight), mission
# popups, the launch briefing - are dismissed as a player would (Close-FLBlocker, native clicks) from every harness
# wait, at most every 0.4 s (the user, 28 Oct 2026: tests in a tutorial area kept stalling on them - a box pauses the
# game, then blocks the pause menu a save load needs). On by default; a test whose subject is such a box sets
# $global:FLKeepPopups = $true. Load-FLSave dismisses a box that blocks its pause menu in any case.
if (-not (Test-Path variable:global:FLKeepPopups)) { $global:FLKeepPopups = $false }
$script:FLPopupCheck = [Diagnostics.Stopwatch]::StartNew()
function Step-FLPopups([switch]$Now) {
    if ($global:FLKeepPopups -or (-not $Now -and $script:FLPopupCheck.ElapsedMilliseconds -lt 400)) { return '' }
    $script:FLPopupCheck.Restart()
    if (-not (Get-Command Close-FLBlocker -ErrorAction SilentlyContinue)) { return '' }
    $c = ''
    try { $c = Close-FLBlocker } catch [System.TimeoutException] { } catch { Write-Host "popup close: $($_.Exception.Message)" }
    if ($c) { Write-Host ("{0:HH:mm:ss} closed the game's box ({1})" -f (Get-Date), $c) }
    $c
}

# Waits until the game has processed $Frames more frames (state, not time; safety limit $MaxSec). $false on the limit.
function Wait-FLFramesPassed([int]$Frames, [double]$MaxSec = 10) {
    $f0 = Get-FLFrame
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $MaxSec) {
        $null = Step-FLPopups
        try { $f = Get-FLFrame } catch [System.TimeoutException] { continue }
        if ($f -lt $f0) { $f0 = $f }   # the frame count starts over after a logic hot reload
        if ($f -ge $f0 + $Frames) { return $true }
        Start-Sleep -Milliseconds 5   # polling interval
    }
    $false
}

function Get-FLLog([int]$Tail = 40) {
    # this instance's log ($env:FLVIBED_INSTANCE, tools\fldata.ps1; until 29 Oct 2026 always the main install's)
    Get-Content (Join-Path (Get-FLInstallDir) 'EXE\FLController.log') -Tail $Tail
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
        $null = Step-FLPopups
        # A command the game did not answer in time (loading, a stall) means "not yet"; the game exiting ends the wait.
        try { $r = & $Condition; $silent = $null } catch [System.TimeoutException] { $r = $null; $silent = $_.Exception.Message }
        if ($r) { return $r }
        Start-Sleep -Milliseconds 50
    } while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec)
    $state = if ($silent) { "game not answering ($silent)" } else { try { Get-FLStoryState } catch { 'game not answering' } }
    throw "$What not reached after $TimeoutSec s. Game state: $state"
}
