# The design bench's fixtures (audit 71b B4b): HUD 2's 25 simulated states in its pad and keyboard-and-mouse forms,
# recorded by `hud2 bench record` from the release build, one display size per game start, so the bench (B5a) can run
# each state again with no game and its lists can be held against the game's (B6a, level 1).
#   1. per size: the instance's display set borderless, a background start on the still sky of tests\classic_frames.ps1
#      (SaveC706 raised 25 km), checked still; glyph_style 1 (Xbox), the NPCs' chatter off
#   2. per form (pad: the virtual pad, hud_style 1; K&M: input_mode 2, `hud2 on`) and per `hud2 sim list` name: the
#      state checked and settled (two round trips round -SettleSec), then
#        <name>_off.txt   `hud2 kitdump` of the next -Passes kit passes, the recorder off
#        <name>_on.txt    the same, started together with `hud2 bench record -Frames <dir> <name>` (the recorder on)
#        <name>_<W>x<H>_f<k>.flstate.json / .fldl.json   the recorded frames (hud2_record.h)
#      and the state checked again; a frame shot <name>.png (PrintWindow) for the eye
#   3. `hud2 fontdump fonts.txt` (every font set's fingerprint block), the game closed
#   4. tools\hudbench\record_check.py check <dir>: every record read whole, its list beside it with the hash the record
#      names, the list's inputs fingerprint the record's, the record's over pass among the passes drawn with the
#      recorder on AND among those drawn with it off (what is drawn does not change with the recorder; a state whose
#      draw moves from pass to pass is named, not failed)
#   5. <dir>\manifest.txt
# FAILS LOUDLY (exit 1) on a state not reached, a record missing or short (fewer frames than asked, a record under 16 KB
# or a list under 1 KB, a record without its list), a check that does not hold (a record cut short breaks its hashes); exit 2 when the run stopped (no game, wrong DLL, no still sky).
# The records carry game names and the player's bindings: they stay in the git-ignored evidence (research\experience\
# bench\record\), carry the private marker, and never leave this machine (tools\private_check.py).
# Usage (PowerShell tool):
#   tests\bench_record.ps1 -Instance <n> [-Sizes 1280x800,3840x2160] [-Forms pad,kbm] [-States cruise,combat] [-Frames 2]
#                          [-Out <dir>] [-Passes 6] [-SettleSec 1.5] [-NoShots]
param([Parameter(Mandatory)][string]$Instance, [string[]]$Sizes = @('1280x800', '3840x2160'), [string[]]$Forms = @('pad', 'kbm'),
      [string[]]$States = @(), [ValidateRange(1, 50)][int]$Frames = 2, [string]$Out = '', [int]$Passes = 6,
      [double]$SettleSec = 1.5, [switch]$NoShots)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
$deploy = Join-Path $root 'tools\deploy.ps1'
$python = Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe'
$checker = Join-Path $root 'tools\hudbench\record_check.py'
if (-not $Out) { $Out = Join-Path $root ("research\experience\bench\record\{0:yyyyMMdd_HHmmss}" -f (Get-Date)) }
$Out = [IO.Path]::GetFullPath($Out)
New-Item -ItemType Directory -Force $Out | Out-Null
$runLog = Join-Path $Out 'log.txt'
function Say([string]$l) { $l = '{0:HH:mm:ss} {1}' -f (Get-Date), $l; Write-Host $l; Add-Content -LiteralPath $runLog $l }
$dll = Join-Path $root 'build\release\FLvibed.dll'
if (-not (Test-Path -LiteralPath $dll)) { Say "no release build ($dll)"; exit 2 }
$buildInfo = (Get-Content (Join-Path $root 'build\release\build_info.txt') -TotalCount 1)
$dllHash = (Get-FileHash -LiteralPath $dll -Algorithm SHA256).Hash
$minRecord = 16384; $minList = 1024   # a record holds two whole states (~60 KB); a list with nothing drawn is ~1.3 KB
$results = [Collections.Generic.List[string]]::new()
$failures = [Collections.Generic.List[string]]::new()
$stopped = $false

# ---- the still sky (tests\classic_frames.ps1 Get-SkySave / Sky-Check, the same numbers)
$skyRise = 25000; $skyClear = 10000
function Get-SkySave {
    $src = Join-Path (Get-FLSaveDir) 'SaveC706.fl'
    $text = [IO.File]::ReadAllText($src, [Text.Encoding]::Latin1)
    $m = [regex]::Match($text, '(?m)^pos = ([^\r\n]*)')
    $p = @($m.Groups[1].Value -split ',' | ForEach-Object { [double]$_.Trim() })
    if (-not $m.Success -or $p.Count -ne 3) { throw "${src}: no 'pos = x, y, z' line" }
    $text = $text.Substring(0, $m.Index) + ('pos = {0:0}, {1:0}, {2:0}' -f $p[0], ($p[1] + $skyRise), $p[2]) + $text.Substring($m.Index + $m.Length)
    $o = Join-Path $Out 'SaveC706sky.fl'
    [IO.File]::WriteAllText($o, $text, [Text.Encoding]::Latin1)
    $script:skyY = $p[1] + $skyRise
    $o
}
function Get-NearestShip {
    $d = @([regex]::Matches("$(Send-FL 'contacts')", '(?m)^\d+ \S+ dist=(\d+) ship ') | ForEach-Object { [int]$_.Groups[1].Value })
    if ($d.Count) { ($d | Measure-Object -Minimum).Minimum } else { [int]::MaxValue }
}
function Sky-Check {
    $p = Get-FLPilot; $near = Get-NearestShip
    $arrival = [regex]::Match("$(Send-FL 'hud2 state')", '(?m)^\s*arrival\s+want=\d now=([0-9A-F]{8})(?: hidden=([0-9A-F]{8}))?[^\r\n]*').Groups
    $arrOk = $arrival[0].Success -and $arrival[0].Value -notmatch 'unresolved' -and ($arrival[1].Value -eq '00000000' -or $arrival[2].Value -eq $arrival[1].Value)
    $thr = (Send-FL 'state') -match 'throttle=0\b'
    $yOk = $p -and [Math]::Abs($p.Pos[1] - $script:skyY) -lt 50
    $script:skyWhy = "y=$(if ($p) { [int]$p.Pos[1] } else { '?' }) want $([int]$script:skyY), throttle0=$thr, nearest=$near, arrival='$($arrival[0].Value.Trim())'"
    if ($yOk -and $thr -and $near -ge $skyClear -and $arrOk) {
        "still sky, arrival line $(if ($arrival[1].Value -eq '00000000') { 'gone' } else { 'hidden by HUD 2' })"
    }
}

# ---- one state: settled, kitdump off, kitdump + record on, checked again
function Test-Closed([string]$p) { try { $s = [IO.File]::Open($p, 'Open', 'Read', 'None'); $s.Dispose(); $true } catch { $false } }
function Hud2Line { ("$(Send-FL 'hud2 state')" -split "`n")[0] }
function Device { [regex]::Match("$(Send-FL 'device')", 'device=(\S+)').Groups[1].Value }
function Hud2-Check([string]$Form, [string]$Sim) {
    $h = Hud2Line; $d = Device
    $s = [regex]::Match($h, '\bsim=(\S+)').Groups[1].Value
    if ($h -match '^hud2 active=1 visible=1' -and $d -eq $Form -and $s -eq $Sim -and (Send-FL 'state') -match 'paused=0') {
        "device=$d $([regex]::Match($h, 'sim=\S+ .*').Value)$(if ($sky = Sky-Check) { "; $sky" } else { '; SKY NOT STILL' })"
    }
}
function Dump-Start([string]$file, [int]$n = $Passes) {
    foreach ($f in $file, "$file.bin") { if (Test-Path -LiteralPath $f) { Remove-Item -LiteralPath $f -Force } }
    $r = Send-FL "hud2 kitdump $n $file"
    if ($r -notmatch '^kitdump: the next') { throw "kitdump refused: $r" }
    $script:dumpAsked = $n
}
function Dump-Wait([string]$file) {
    $null = Wait-FLUntil { (Test-Closed $file) -and (Test-Closed "$file.bin") } "kitdump $(Split-Path $file -Leaf) closed" 20
    $n = @(Get-Content -LiteralPath $file | Where-Object { $_ -match '^pass ' }).Count
    if ($n -ne $script:dumpAsked) { throw "$(Split-Path $file -Leaf): $n passes, $script:dumpAsked asked" }
}
function Record-State([string]$Dir, [string]$Name, [scriptblock]$Check) {
    try {
        $before = Wait-FLUntil $Check "state $Name" 20
        $null = Send-FL 'state'; Start-Sleep -Milliseconds ([int]($SettleSec * 1000)); $null = Send-FL 'state'   # the fades settle
        $off = Join-Path $Dir "${Name}_off.txt"; $on = Join-Path $Dir "${Name}_on.txt"
        Dump-Start $off; Dump-Wait $off
        foreach ($f in @(Get-ChildItem -LiteralPath $Dir -Filter "${Name}_*x*_f*.fl*.json" -ErrorAction SilentlyContinue)) { [IO.File]::Delete($f.FullName) }
        Dump-Start $on ($Passes * ($Frames + 2)) # (longer: the record's frames inside it, though armed one call later)
        $r = Send-FL "hud2 bench record $Frames `"$Dir`" $Name"
        if ($r -notmatch '^bench record: the next') { throw "bench record refused: $r" }
        Dump-Wait $on
        $state = Wait-FLUntil { $s = "$(Send-FL 'hud2 bench record')"; if ($s -match '\bleft=0\b') { $s } } "$Name recorded" 20
        $after = & $Check
        if (-not $after) { throw "the state no longer held after the record" }
        # the record whole: every frame asked, each with its list, none short
        $recs = @(Get-ChildItem -LiteralPath $Dir -Filter "${Name}_*x*_f*.flstate.json")
        $lists = @(Get-ChildItem -LiteralPath $Dir -Filter "${Name}_*x*_f*.fldl.json")
        $short = @(@($recs | Where-Object { $_.Length -lt $minRecord }) + @($lists | Where-Object { $_.Length -lt $minList }) | ForEach-Object { "$($_.Name) $($_.Length) bytes" })
        if ($state -notmatch "written=$Frames\b" -or $state -notmatch '\bfailed=0\b' -or $recs.Count -ne $Frames -or $lists.Count -ne $Frames -or $short.Count) {
            throw "record short: $state; $($recs.Count) records, $($lists.Count) lists of $Frames$(if ($short.Count) { "; short: $($short -join ', ')" })"
        }
        $shot = ''
        if (-not $NoShots) {
            try { $f = Save-FLShot -Name $Name -Dir $Dir; Move-Item -LiteralPath $f (Join-Path $Dir "$Name.png") -Force; $shot = " shot=$Name.png" }
            catch { $shot = " shot=NONE ($($_.Exception.Message))" }
        }
        $results.Add("$Name | $before | $state$shot")
        Say "recorded $Name | $state"
    } catch {
        $failures.Add("$Name | $($_.Exception.Message)")
        Say "FAILED $Name : $($_.Exception.Message)"
        try { $null = Send-FL 'hud2 bench record 0'; $null = Send-FL 'hud2 kitdump 0' } catch { }
    }
}

$homeSize = $null
$started = Get-Date
try {
    if (Get-FLGameProcess) { throw "instance $Instance already has a game running (not started here): not touched" }
    $homeSize = Get-FLDisplaySize -InstallDir (Get-FLInstallDir)
    Say "bench_record -> $Out; build $buildInfo; dll sha256 $dllHash; instance $Instance; sizes $($Sizes -join ','); forms $($Forms -join ','); frames $Frames"
    foreach ($size in $Sizes) {
        if ($size -notmatch '^(\d{3,4})x(\d{3,4})$') { throw "size '$size': not <W>x<H>" }
        $W = [int]$Matches[1]; $H = [int]$Matches[2]
        $dir = Join-Path $Out $size
        New-Item -ItemType Directory -Force $dir | Out-Null
        # the machine-wide game limit (docs\TESTING.md "Parallel lanes": at most four games): wait for a slot
        while ($true) {
            $g = @(& (Join-Path $root 'tools\lane.ps1') status *>&1 | Out-String -Stream | Select-String 'game pid').Count
            if ($g -lt 4) { break }
            Say "gate: $g games running, waiting"
            Start-Sleep -Seconds 130
        }
        try {
            $null = Set-FLInstanceDisplay $Instance 'borderless' "$W, $H"
            $r = & $deploy -Release -NoBuild -Background -Instance $Instance -Save (Get-SkySave) 2>&1 | Select-Object -Last 1
            Say "start $size : $r"
            $deployed = Join-Path (Get-FLInstallDir) 'EXE\FLvibed.dll'
            $depHash = if (Test-Path -LiteralPath $deployed) { (Get-FileHash -LiteralPath $deployed -Algorithm SHA256).Hash } else { 'missing' }
            if ($depHash -ne $dllHash) { throw "the deployed DLL ($depHash) is not build\release's ($dllHash)" }
            $render = [regex]::Match((Send-FL 'winmode'), 'render (\d+)x(\d+)').Value
            if ($render -ne "render ${W}x$H") { throw "the game renders '$render', not $size" }
            $null = Send-FL 'set glyph_style 1'   # Xbox glyphs, as classic_frames (a real pad would switch the family)
            $null = Send-FLS 'set chatter 2'      # the NPCs' radio chatter off (random lines)
            $null = Send-FL 'set hud_style 0'
            $null = Send-FL 'set input_mode 1'
            Set-FLPad
            try { $sky = Wait-FLUntil { $s = Sky-Check; if ($s -match 'arrival line gone') { $s } } 'the still sky' 60 } catch { throw "$($_.Exception.Message) (last: $script:skyWhy)" }
            Say "sky: $sky"
            $null = Send-FL 'set hud_style 1'
            $list = "$(Send-FL 'hud2 sim list')"
            if ($list -notmatch '^hud2 sim (\S+)') { throw "hud2 sim list: $list" }
            $sims = @($Matches[1] -split '\|' | Where-Object { $_ })
            if ($States.Count) { $sims = @($sims | Where-Object { $States -contains $_ }) }
            Say "sims ($($sims.Count)): $($sims -join ' ')"
            foreach ($form in $Forms) {
                if ($form -eq 'pad') { $null = Send-FL 'hud2 auto'; $null = Send-FL 'set input_mode 1'; Set-FLPad }
                else { Clear-FLPad; $null = Send-FL 'set input_mode 2'; $null = Send-FL 'hud2 on' }
                foreach ($st in $sims) {
                    $null = Send-FL "hud2 sim $st"
                    Record-State $dir "${form}_$st" ([scriptblock]::Create("Hud2-Check '$form' '$st'"))
                }
                $null = Send-FL 'hud2 sim off'
            }
            $f = Join-Path $dir 'fonts.txt'
            $r = Send-FL "hud2 fontdump $f"
            Say "fontdump: $r"
            if ($r -notmatch '^fontdump: \d+ fonts') { $failures.Add("$size fonts | $r") }
        } catch {
            Say "STOPPED $size : $($_.Exception.Message)"
            $failures.Add("$size (run stopped) | $($_.Exception.Message)")
            $stopped = $true
        } finally {
            try { if (Get-FLGameProcess) { Clear-FLPad } } catch { }
            Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
            Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
        }
        # the checks of this size's folder
        $c = & $python -I $checker check $dir 2>&1 | Out-String
        Set-Content -LiteralPath (Join-Path $dir 'check.txt') $c
        Say "check $size : $(($c -split "`n" | Where-Object { $_ -match '^RESULT' }) -join ' ')"
        if ($LASTEXITCODE -ne 0) { $failures.Add("$size check | exit $LASTEXITCODE (see $dir\check.txt)") }
    }
} catch {
    Say "ERROR: $($_.Exception.Message)"
    $failures.Add("(run stopped) | $($_.Exception.Message)")
    $stopped = $true
} finally {
    # this instance's screenshot sampler (deploy -Background starts one per game), the ones started since this script began
    $stop = Join-Path $root "research\experience\shots\sampler$Instance.stop"
    try { Set-Content -LiteralPath $stop 'bench_record' } catch { }
    $mine = { @(Get-CimInstance Win32_Process -Filter "Name='pwsh.exe'" | Where-Object { $_.CommandLine -match 'flshots\.ps1' -and $_.CommandLine -match "-Instance $Instance(\s|$)" -and $_.CreationDate -ge $started }) }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ((& $mine).Count -and $sw.Elapsed.TotalSeconds -lt 20) { Start-Sleep -Milliseconds 250 }
    & $mine | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    if ($homeSize) { $null = Set-FLInstanceDisplay $Instance 'borderless' $homeSize }
}

Set-Content -LiteralPath (Join-Path $Out 'manifest.txt') (@(
    "bench_record $(Get-Date -Format s) ($('{0:n0}' -f ((Get-Date) - $started).TotalSeconds) s), instance $Instance, sizes $($Sizes -join ','), forms $($Forms -join ','), frames $Frames, passes $Passes, settle $SettleSec s",
    "build_info: $buildInfo", "dll sha256: $dllHash", "", "recorded ($($results.Count)):") + $results + @("", "FAILED ($($failures.Count)):") + $failures)
Say "manifest: $($results.Count) recorded, $($failures.Count) failed -> $(Join-Path $Out 'manifest.txt')"
if ($stopped) { exit 2 }
if ($failures.Count) { exit 1 }
exit 0
