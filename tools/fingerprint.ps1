# The refactor's freeze (docs\MODULES_PLAN.md section 3): what must not change while the code is split into modules,
# written to tests\fingerprints\<label>\ as plain text, so a difference reads as a diff.
#   settings.txt   docs\SETTINGS.md regenerated from the registries: every key, default and help
#   ui_meta.txt    the hash of core\ui_meta.h: the options page's rows
#   core_once.txt  core\game is one instance: tools\core_once.py over the release link's map
#   dll_size.info  the release DLL's size: reported by -Compare, never a difference; a label of another size than build\release is STALE
#   hooks_*.txt    each module's hook table from a running game: name, mode, chain target; DLL offsets removed
#   perf_*.txt     CPU per entry point over $PerfSpans spans of flight in the one game, the median per row, so one hitch (a
#                  63 ms call in P1's first sample moved a mean by 70 %) stays out of the reference; `worst call` keeps the
#                  largest of the spans, so it still shows; `order:` = the entry points' order in one frame. The spans
#                  together are the first 3000 frames after the load: later the fixture's work changes (about 16 s in a
#                  line comes on screen: hud2.Render halves, the subtitles' draw goes from 0.1 to 37 us)
#   spans.info     every span's raw reply
# Usage (PowerShell tool):
#   tools\fingerprint.ps1 -Label <name>                    static parts only, seconds, no game
#   tools\fingerprint.ps1 -Label <name> -Live              also one background game on instance 1 (no build)
#   tools\fingerprint.ps1 -Label <name> -PerfOnly          only the perf files, one game: another build's cost, launch for
#                                                          launch (costs vary about 6 % between launches of one build)
#   tools\fingerprint.ps1 -Compare <name> [-Against <name>]  diff two labels (default: against `baseline`); exit 1 on a
#                                                          difference; perf_*.txt compare the order line and report cost,
#                                                          a row 15 % up marked (not a difference: measure it first)
#   tools\fingerprint.ps1 -Label <name> -CommandsOnly      only commands_*.txt, one game: the debug channels' replies, numbers
#                                                          as #, each verb in order, ending with `reload` and `unload`
# The perf files are stored only from a quiet machine. Each span measures what the rest of the machine does (every process but the
# game and this script, in cores: spans.info `machine:` lines); a span above -QuietCores throws the launch away, because a retried
# span would be later in the fixture, and the launch is tried again once the machine is calm, for at most -QuietWaitMin minutes.
# Then it says so and stores no perf file: exit 2. The frame order is not a cost, so it is kept in any case (orders.info).
param([string]$Label = '', [switch]$Live, [switch]$PerfOnly, [switch]$CommandsOnly, [string]$Compare = '', [string]$Against = 'baseline',
      [int]$PerfFrames = 1000, [int]$PerfSpans = 3, [string]$Fixture = 'SaveC702.fl', [int]$Instance = 1,
      [double]$QuietCores = 3.0, [int]$QuietWaitMin = 10) # 3.0: a quiet sample, 5 Nov, read 1.8-2.7 cores; a busy one 4.5-11
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$store = Join-Path $root 'tests\fingerprints'

if (-not ('FLVMachine' -as [type])) {
    Add-Type @'
using System.Runtime.InteropServices;
public static class FLVMachine {
    [DllImport("kernel32.dll")] static extern bool GetSystemTimes(out long idle, out long kernel, out long user);
    public static double BusySeconds() { long i, k, u; GetSystemTimes(out i, out k, out u); return (k + u - i) / 1e7; }
}
'@
}

# CPU seconds of the game and of this script: what a span's own work is.
function Get-OwnCpu {
    $game = @(Get-FLGameProcess | ForEach-Object { $_.TotalProcessorTime.TotalSeconds } | Measure-Object -Sum)[0].Sum
    [double]$game + [Diagnostics.Process]::GetCurrentProcess().TotalProcessorTime.TotalSeconds
}

function Start-MachineSample {
    $procs = @{}
    foreach ($p in Get-Process) { if ($null -ne $p.CPU) { $procs[$p.Id] = [double]$p.CPU } }
    [pscustomobject]@{ clock = [Diagnostics.Stopwatch]::StartNew(); busy = [FLVMachine]::BusySeconds(); own = Get-OwnCpu; procs = $procs }
}

# The rest of the machine since the sample started, in cores, and its busiest process (by name, to read, never to decide on).
function Stop-MachineSample($s) {
    $sec = $s.clock.Elapsed.TotalSeconds
    $cores = ([FLVMachine]::BusySeconds() - $s.busy - ((Get-OwnCpu) - $s.own)) / $sec
    $skip = @($PID) + @(Get-FLGameProcess | ForEach-Object Id)
    $top = Get-Process | Where-Object { $null -ne $_.CPU -and $skip -notcontains $_.Id } |
        Sort-Object { $_.CPU - [double]$s.procs[$_.Id] } -Descending | Select-Object -First 1
    [pscustomobject]@{ cores = [Math]::Max([double]0, [double]$cores); seconds = $sec
        top = '{0} {1:N2}' -f $top.ProcessName, (($top.CPU - [double]$s.procs[$top.Id]) / $sec) }
}

# True once the rest of the machine stayed under the limit for two samples in a row; false at `$until`.
function Wait-MachineQuiet([datetime]$until) {
    $calm = 0
    while ((Get-Date) -lt $until) {
        $s = Start-MachineSample
        Start-Sleep -Seconds 5
        $m = Stop-MachineSample $s
        if ($m.cores -le $QuietCores) { if (++$calm -ge 2) { return $true } }
        else { $calm = 0; $script:lastBusy = '{0:N2} cores (top: {1})' -f $m.cores, $m.top }
    }
    $false
}

# The frame's forms a label holds: orders.info, or the spans of a label older than it.
function Get-Orders([string]$dir) {
    @(foreach ($n in 'orders.info', 'spans.info') {
            $p = Join-Path $dir $n
            if (Test-Path $p) { Get-Content $p | Where-Object { $_ -like 'order:*' } }
        }) | Sort-Object -Unique
}

function Get-PerfRows([string]$path) {
    $rows = @{}
    foreach ($m in [regex]::Matches((Get-Content -Raw $path), '(?m)^\s+(\S+)\s+([\d.]+) us/frame')) {
        $rows[$m.Groups[1].Value] = [double]$m.Groups[2].Value
    }
    $t = [regex]::Match((Get-Content -Raw $path), 'total ([\d.]+);') # the subtitles' one line
    if ($t.Success) { $rows['total'] = [double]$t.Groups[1].Value }
    $rows
}

function Get-Median([double[]]$v) {
    $s = @($v | Sort-Object)
    if ($s.Count % 2) { $s[[int](($s.Count - 1) / 2)] } else { ($s[$s.Count / 2 - 1] + $s[$s.Count / 2]) / 2 }
}

# The controller's spans as one reply of the same form: the median per row, the largest worst call, the commonest order.
function Merge-PerfSpans([string[]]$spans) {
    $names = [Collections.Generic.List[string]]::new()
    $us = @{}; $worst = @{}; $calls = @{}; $orders = @()
    foreach ($s in $spans) {
        foreach ($m in [regex]::Matches($s, '(?m)^\s+(\S+)\s+([\d.]+) us/frame\s+(\d+) calls\s+worst call\s+([\d.]+) us')) {
            $k = $m.Groups[1].Value
            if (-not $us.ContainsKey($k)) { $names.Add($k); $us[$k] = @(); $worst[$k] = 0.0; $calls[$k] = @() }
            $us[$k] += [double]$m.Groups[2].Value
            $calls[$k] += [double]$m.Groups[3].Value
            $worst[$k] = [Math]::Max($worst[$k], [double]$m.Groups[4].Value)
        }
        $orders += ($s -split "`n" | Where-Object { $_ -like 'order:*' } | ForEach-Object { $_.Trim() })
    }
    "median of $($spans.Count) spans of $PerfFrames frames; " + (($spans[0] -split "`n")[0]).Trim()
    foreach ($k in $names) {
        '  {0,-18} {1,8:F1} us/frame  {2,6:F0} calls  worst call {3,8:F1} us' -f $k, (Get-Median $us[$k]), (Get-Median $calls[$k]), $worst[$k]
    }
    ($orders | Group-Object | Sort-Object Count -Descending | Select-Object -First 1).Name
}

# The subtitles' reply is one line of numbers: the span with the median total stands for them.
function Select-MedianSpan([string[]]$spans) {
    $sorted = @($spans | Sort-Object { [double][regex]::Match($_, 'total ([\d.]+);').Groups[1].Value })
    "median of $($spans.Count) spans of $PerfFrames frames"
    $sorted[[int][Math]::Floor(($sorted.Count - 1) / 2)]
}

if ($Compare) {
    $a = Join-Path $store $Against
    $b = Join-Path $store $Compare
    # a label of a build the audit's fixes have since replaced says nothing about what ships (P5 and P6 each did this)
    $size = Join-Path $b 'dll_size.info'; $built = Join-Path $root 'build\release\FLvibed.dll'
    if ((Test-Path $size) -and (Test-Path $built) -and "$(Get-Content $size)" -notmatch ": $((Get-Item $built).Length)$") { "STALE   $Compare was not taken from build\release as it is now" }
    $failed = $false
    foreach ($f in Get-ChildItem $a -File) {
        $other = Join-Path $b $f.Name
        if (-not (Test-Path $other)) { "ABSENT  $($f.Name) (not taken for $Compare)"; continue }
        if ($f.Extension -eq '.info') {
            if ($f.Name -notin 'spans.info', 'orders.info') { "info    $(Get-Content $f.FullName) -> $(Get-Content $other)" }
            continue
        }
        if ($f.Name -like 'perf_*') {
            $rowsA = Get-PerfRows $f.FullName
            $rowsB = Get-PerfRows $other
            foreach ($k in ($rowsA.Keys | Sort-Object)) {
                if ($rowsB.ContainsKey($k)) {
                    $mark = if ($rowsA[$k] -ge 1 -and $rowsB[$k] -gt $rowsA[$k] * 1.15) { '  <- 15 % up: measure it launch for launch' } else { '' }
                    '        {0,-18} {1,8:N1} -> {2,8:N1} us/frame{3}' -f $k, $rowsA[$k], $rowsB[$k], $mark
                }
            }
            continue
        }
        $d = Compare-Object (Get-Content $f.FullName) (Get-Content $other)
        if ($d) {
            "DIFFERS $($f.Name): $(@($d).Count) lines"
            $d | Select-Object -First 20 | ForEach-Object { "  $($_.SideIndicator) $($_.InputObject)" }
            $failed = $true
        } else { "same    $($f.Name)" }
    }
    # `order:` is the fullest frame of a span, and the fixture has two forms of it (a contact scan before or inside hud2.Update),
    # both in every label, and three spans may show only one: a form the reference does not have is a difference, a form that
    # did not come up is not
    $orderA = Get-Orders $a
    $orderB = Get-Orders $b
    $new = @($orderB | Where-Object { $orderA -notcontains $_ })
    if (-not $orderB) { "ABSENT  frame order (not taken for $Compare)" }
    elseif ($new) { "DIFFERS frame order: new`n  $($new -join "`n  ")"; $failed = $true }
    else { 'same    frame order' }
    if ($failed) { exit 1 }
    return
}

if (-not $Label) { throw 'name a -Label or a -Compare' }
$out = Join-Path $store $Label
New-Item -ItemType Directory -Force $out | Out-Null

# Every verb a host answers, the verbs that collide between the two channels, and what a stopped logic answers: the reply's
# shape, numbers as #. `unload` last: everything after it is the host's own answer.
$channelVerbs = 'ping', 'hooks', 'state', 'config', 'set flvibed_no_such_key 1', 'trace 0', 'perf', 'flvibed_no_such_verb'
$hostVerbs = @{
    controller = 'events 999999', 'uigen', 'winmode', 'uibtn', 'sdlpad', 'd3dreset'
    subtitles  = @('movieskip')
}
function Save-Commands([string]$dir) {
    $reply = {
        param([string]$ch, [string]$verb)
        "> $verb"
        $text = if ($ch -eq 'controller') { Send-FL $verb } else { Send-FLS $verb }
        # the state's values move with the game: what must hold is each line's name and form
        "$text" -split "`n" | ForEach-Object {
            (($_ -replace '0x[0-9A-Fa-f]+', '<addr>') -replace '\b[0-9A-Fa-f]{8}\b', '<addr>') -replace '[-+]?\d+(\.\d+)?', '#'
        } | Where-Object { $_.Trim() }
    }
    # `reload` and `unload` are one act for all the logic: every channel is read before either
    $files = @{}
    foreach ($ch in 'controller', 'subtitles') { $files[$ch] = @($channelVerbs + $hostVerbs[$ch] | ForEach-Object { & $reply $ch $_ }) }
    foreach ($ch in 'controller', 'subtitles') { $files[$ch] += @(& $reply $ch 'reload') }
    $files['controller'] += @(& $reply 'controller' 'unload')
    foreach ($ch in 'controller', 'subtitles') { $files[$ch] += @('ping', 'state' | ForEach-Object { & $reply $ch $_ }) }
    foreach ($ch in 'controller', 'subtitles') { $files[$ch] | Set-Content (Join-Path $dir "commands_$ch.txt") }
}

if (-not $PerfOnly -and -not $CommandsOnly) {
    & (Join-Path $root 'tools\settings_table.ps1') -Out (Join-Path $out 'settings.txt') | Out-Null
    "core\ui_meta.h $((Get-FileHash (Join-Path $root 'core\ui_meta.h') -Algorithm SHA256).Hash)" |
        Set-Content (Join-Path $out 'ui_meta.txt')
}

$dll = Join-Path $root 'build\release\FLvibed.dll'
if ((Test-Path $dll) -and -not $PerfOnly -and -not $CommandsOnly) {
    $py = Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe'
    & $py -I (Join-Path $root 'tools\core_once.py') | Set-Content (Join-Path $out 'core_once.txt')
    if ($LASTEXITCODE -ne 0) { throw "core_once: $(Get-Content (Join-Path $out 'core_once.txt') -Raw)" }
    # the size is reported, not compared: packages P1 on change it on purpose (docs\ratchets.tsv release_dll_bytes)
    "release dll bytes (informational): $((Get-Item $dll).Length)" | Set-Content (Join-Path $out 'dll_size.info')
}

if ($Live -or $PerfOnly -or $CommandsOnly) {
    . (Join-Path $root 'tools\harness\flctl.ps1')
    . (Join-Path $root 'tools\harness\flsub.ps1')
    $env:FLVIBED_INSTANCE = "$Instance"
    $normal = {
        param([string]$reply)
        # build-dependent numbers go: DLL offsets and raw addresses; what stays is what is hooked, how and through what
        ($reply -split "`n" | ForEach-Object { ($_ -replace '\+0x[0-9A-Fa-f]+', '') -replace '\b0x[0-9A-Fa-f]{6,8}\b', '<addr>' } |
            Where-Object { $_.Trim() }) -join "`n"
    }
    $start = { & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Release -Instance $Instance -Save $Fixture | Out-Null }
    if ($CommandsOnly) {
        & $start
        try { Save-Commands $out } finally { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue }
        return
    }
    $script:lastBusy = ''
    $giveUp = (Get-Date).AddMinutes($QuietWaitMin)
    $first = $true
    $taken = $false
    while ($true) {
        # -Live's first game has hooks and replies to read, which no load can change: only the perf files wait for quiet
        if (($PerfOnly -or -not $first) -and -not (Wait-MachineQuiet $giveUp)) { break }
        & $start
        try {
            if ($first -and $Live) {
                & $normal (Send-FL 'hooks') | Set-Content (Join-Path $out 'hooks_controller.txt')
                & $normal (Send-FLS 'hooks') | Set-Content (Join-Path $out 'hooks_subtitles.txt')
            }
            $null = Send-FL 'perf cpu'
            $null = Send-FLS 'perf'
            $ctl = @(); $sub = @(); $machine = @()
            foreach ($span in 1..$PerfSpans) {
                # a span of the game's own frames, so a slower build measures the same work; the deadline only catches a hang
                $until = (Get-FLFrame) + $PerfFrames
                $deadline = (Get-Date).AddSeconds(120)
                $sample = Start-MachineSample
                while ((Get-FLFrame) -lt $until) {
                    Step-FLPopups
                    if ((Get-Date) -gt $deadline) { throw "the game did not reach $PerfFrames frames in 120 s" }
                }
                $machine += Stop-MachineSample $sample
                $ctl += "$(Send-FL 'perf cpu')"
                $sub += "$(Send-FLS 'perf')"
            }
            @(Get-Orders $out) + @($ctl -split "`n" | Where-Object { $_ -like 'order:*' } | ForEach-Object { $_.Trim() }) |
                Sort-Object -Unique | Set-Content (Join-Path $out 'orders.info')
            $lines = @($machine | ForEach-Object { 'machine: {0:N2} cores over {1:N1} s, top: {2}' -f $_.cores, $_.seconds, $_.top })
            $lines | ForEach-Object { "  $_" } | Write-Host
            $busy = @($machine | Where-Object { $_.cores -gt $QuietCores })
            if ($busy) { $script:lastBusy = '{0:N2} cores (top: {1})' -f $busy[0].cores, $busy[0].top }
            else {
                Merge-PerfSpans $ctl | Set-Content (Join-Path $out 'perf_controller.txt')
                Select-MedianSpan $sub | Set-Content (Join-Path $out 'perf_subtitles.txt')
                ($ctl + $sub + $lines) | Set-Content (Join-Path $out 'spans.info')
                $taken = $true
            }
            if ($first -and $Live) { Save-Commands $out }
        } finally {
            Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        }
        $first = $false
        if ($taken -or (Get-Date) -gt $giveUp) { break }
    }
    if (-not $taken) {
        Get-ChildItem $out -File | ForEach-Object { '{0,-24} {1,8} bytes' -f $_.Name, $_.Length }
        "PERF NOT TAKEN: the machine did not go quiet in $QuietWaitMin min (limit $QuietCores cores; last $script:lastBusy); no perf file stored"
        exit 2
    }
}
Get-ChildItem $out -File | ForEach-Object { '{0,-24} {1,8} bytes' -f $_.Name, $_.Length }
