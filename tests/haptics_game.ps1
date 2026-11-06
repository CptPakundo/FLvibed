# The haptics' readers and gates in a running game (modules\controller\docs\HAPTICS.md 11). One background game, muted, on a virtual DualSense: a real pad is never opened (the host
# logs it, and this stops if the log does not say so). Prints the raw readings; judges only what a reading can show. Nothing here says how anything feels.
#   safety   the switch is off and SDL opens no real pad, before any effect fires
#   gauge    ThrustGauge against the afterburner's own run: 1 to 0 over the burn, the burner ending at 0 (tests\pad_toggles.ps1 measured 15.4 s on the Starflier)
#   cruise   the game's CanFire under cruise, the charge's readers
#   lane     the same on a lane; rings counted
#   field    space::FieldAt inside an asteroid field and a nebula of the ship's system
#   bump     the ship driven into a station: the hit report's kind
#   gates    Off on the virtual pad at the pause, a window, the autopilot, focus; the lease
# Usage (PowerShell tool): tests\haptics_game.ps1 [-Row safety,gauge,...] [-Instance 11] [-Save SaveC702.fl]
param([string[]]$Row = @('safety', 'gauge', 'cruise', 'lane', 'field', 'bump', 'gates'), [string]$Instance = 11, [string]$Save = 'SaveC702.fl', [switch]$NoStart,
      [string]$BumpInto = 'Fort Bush')
$script:BumpInto = $BumpInto
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$Row = @($Row | ForEach-Object { $_ -split ',' })
$dir = Join-Path $root 'research\experience\runs\haptics_game'
New-Item -ItemType Directory -Force $dir | Out-Null
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Sig { $h = @{}; foreach ($m in [regex]::Matches((Send-FL 'haptics signals'), '(\w+)=(\S+)')) { $h[$m.Groups[1].Value] = $m.Groups[2].Value }; $h }
function LogLines([string]$pattern) { Get-Content -LiteralPath (Join-Path (Split-Path (Get-FLGameProcess).Path -Parent) 'FLController.log') -ErrorAction SilentlyContinue | Where-Object { $_ -match $pattern } }
function Effects([int]$since) { @(Get-FLSdlPadOut -Since $since | Where-Object Kind -eq 'effect') }
function LastSeq { $o = @(Get-FLSdlPadOut | Select-Object -Last 1); if ($o.Count -and $o[0]) { $o[0].Seq } else { 0 } }
function Mode($e) { '{0:X2}/{1:X2}' -f ([uint32]$e.B -band 0xFF), (([uint32]$e.B -shr 8) -band 0xFF) } # right / left mode byte
function Shot([string]$name) { try { $null = Save-FLShot $name $dir } catch { "  (no frame: $_)" } }
# Each gun's own verdict on a point 3 km along the nose, as the engine asks it
function GunPoint { $s = Sig; $n = @($s.nose -split ',' | ForEach-Object { [double]$_ }); $p = @($s.pos -split ',' | ForEach-Object { [double]$_ })
    ((Send-FL ('gunpoint {0:0} {1:0} {2:0}' -f ($p[0] + $n[0] * 3000), ($p[1] + $n[1] * 3000), ($p[2] + $n[2] * 3000))) -replace "`r?`n", ' | ') }
# RT held 2 s: the shots the engine read, from each gun's refire clock, say whether the guns really fired
function Started { [int][regex]::Match((Send-FL 'haptics status'), ' started=(\d+)').Groups[1].Value }
function FireTest([string]$frame = '') {
    $a = Started; $low = 1.0; Set-FLSdlPad -Slot 0 -RT 1
    $t0 = Get-Date; $n = 0
    while (((Get-Date) - $t0).TotalSeconds -lt 3) { $p = [double](Sig).power; if ($p -lt $low) { $low = $p }; if ($frame -and ++$n -in 6, 7) { Shot "$($frame)_$n" } ; Start-Sleep -Milliseconds 150 }
    Set-FLSdlPad -Slot 0; Start-Sleep -Milliseconds 300
    "$((Started) - $a) effects started; lowest power $low; $(((Send-FL 'haptics status') -replace '.* last=', 'last=') -replace ' open=.*', '')"
}
$script:AsteroidAt = 6204, 0, 51761  # Li01's Zone_Li01_Badlands_low_density_asteroids, its centre (li01.ini)
$script:NebulaAt = 52628, 0, -49035   # Zone_Li01_Pittsburgh_Pirate_Base_Nebula

if (-not $NoStart) { & (Join-Path $root 'tools\deploy.ps1') -Background -NoBuild -Instance $Instance -Save $Save | Select-Object -Last 1 }
try {
    "$(Send-FL 'set haptics_device 0')"
    "$(Send-FL 'set input_mode 1')"
    "$(Send-FL 'set pause_on_disconnect 0')"
    "$(Send-FL 'set pad_backend 1')"
    $null = Wait-FLUntil { (Send-FL 'state') -match 'sdl_status=2' } 'SDL running' 10
    # ---- the safety line comes first: nothing below runs without it
    $sdlLine = @(LogLines 'SDL opens no real pad')
    if (-not $sdlLine.Count) { throw 'STOP: the log does not say that SDL opens no real pad' }
    $sdlLine[-1]
    $real = @(LogLines 'SDL3 pad backend: .*(in use|gamepad\(s\), in use)' | Where-Object { $_ -notmatch 'test pad' })
    if ($real.Count) { throw "STOP: SDL opened a pad that is no test pad: $($real[-1])" }
    "$(Connect-FLSdlPad -Type ps5 -Slot 0 -Foreground)"
    $null = Wait-FLUntil { $l = Send-FL 'state'; $l -match 'source=sdl' -and $l -match 'sdl_connected=1' } 'the virtual DualSense in use' 10
    $null = Wait-FLUntil { (Send-FL 'haptics status') -match 'world=open' } 'haptics open' 8
    $off = @(LogLines 'haptics: output to a real pad OFF')
    if (-not $off.Count) { throw 'STOP: the log does not say that output to a real pad is off' }
    $off[-1]
    (Send-FL 'haptics status') -replace '.* open=', 'open='
    Check ((Send-FL 'haptics status') -match '\+adaptive') 'the virtual DualSense has Adaptive Triggers'
    (Send-FL 'haptics signals')

    if ('gauge' -in $Row) {
        '--- gauge'
        Set-FLSdlPad -Slot 0 -LT 1
        $t0 = Get-Date; $rows = [Collections.Generic.List[object]]::new(); $lastOn = 0
        while (((Get-Date) - $t0).TotalSeconds -lt 26) {
            $s = Sig; $sec = ((Get-Date) - $t0).TotalSeconds
            $rows.Add([pscustomobject]@{ T = [Math]::Round($sec, 1); Gauge = [double]$s.gauge; Burner = $s.burner })
            if ($s.burner -eq '1') { $lastOn = $sec }
            if ($rows.Count -in 6, 30) { Shot "gauge_$($rows.Count)" }
            Start-Sleep -Milliseconds 400
        }
        Set-FLSdlPad -Slot 0
        ($rows | ForEach-Object { '{0}:{1:N3}/{2}' -f $_.T, $_.Gauge, $_.Burner }) -join ' '
        Check ($rows[1].Gauge -gt 0.8) 'the gauge starts near full'
        $mid = @($rows | Where-Object { $_.T -gt 5 -and $_.T -lt 10 })[0]
        Check ($mid.Gauge -gt 0.2 -and $mid.Gauge -lt $rows[1].Gauge) "it falls during the burn: $($mid.Gauge) at $($mid.T) s"
        $end = @($rows | Where-Object { $_.Burner -eq '0' -and $_.T -gt 2 })[0]
        Check ($end -and $end.Gauge -lt 0.05) "the burner ends at empty: $($end.Gauge) at $($end.T) s"
        Check ($rows[-1].Gauge -gt $end.Gauge + 0.3) "it refills at rest: $($rows[-1].Gauge)"
    }
    if ('cruise' -in $Row) {
        '--- cruise'
        "at rest: $(GunPoint)"
        "fire at rest: $(FireTest 'fire_rest')"
        $seqC = LastSeq
        $null = Send-FL 'tap USER_CRUISE'
        Start-Sleep -Seconds 1
        $wall = @(Effects $seqC)
        Check ($wall.Count -and (Mode $wall[-1]) -match '^21/') "the fire trigger stands as a wall from the charge on: $(($wall | ForEach-Object { Mode $_ }) -join ' ')"
        Start-Sleep -Seconds 1
        $s = Sig; "charging: engaged=$($s.engaged) charge=$($s.charge) fire=$($s.fire)"
        "charging: $(GunPoint)"
        Check ($s.engaged -eq '1') 'the charge is read'
        "fire while charging: $(FireTest)"
        Start-Sleep -Seconds 5
        $s = Sig; "cruising: engaged=$($s.engaged) charge=$($s.charge) fire=$($s.fire) excluded=$($s.excluded)"
        "in cruise: $(GunPoint)"
        "FireState in cruise (0 ready, 1 locked, 2 empty): $($s.fire)"
        $f = FireTest 'fire_cruise'; "fire in cruise: $f"
        Check ($f -match '^\d+ effects' -and [int]($f -split ' ')[0] -le 4) 'no grain for a held fire in cruise: no shot leaves the ship'
        $null = Send-FL 'tap USER_CRUISE'
        Start-Sleep -Seconds 1
    }
    if ('lane' -in $Row) {
        '--- lane'
        $ring = @(Get-FLContacts -All | Where-Object { $_.Name -match 'Trade Lane' } | Sort-Object Distance)[0]
        "ring: $($ring.Name) at $([int]$ring.Distance) m"
        $null = Start-FLTradeLane $ring.Id -NoWait
        $seen = 0; $rings = @(); $t0 = Get-Date
        while (((Get-Date) - $t0).TotalSeconds -lt 60) {
            $s = Sig
            if ($s.piece -eq '1') {
                if (-not $seen) { $seen = 1; "on the lane: $(GunPoint)"; "on the lane: fire=$($s.fire) excluded=$($s.excluded) rings=$($s.rings)"; "fire on the lane: $(FireTest 'fire_lane')" }
                if ($rings -notcontains $s.rings) { $rings += $s.rings }
            } elseif ($seen) { break }
            Start-Sleep -Milliseconds 300
        }
        Check ($seen -eq 1) 'a lane is read as the set piece'
        "rings seen: $($rings -join ' ')"
        Check ($rings.Count -ge 2) 'the rings count down'
        $null = Send-FL 'ap stop'
    }
    if ('field' -in $Row) {
        '--- field'
        foreach ($t in @(@{ Name = 'asteroid'; Want = '1'; Pos = $script:AsteroidAt }, @{ Name = 'nebula'; Want = '2'; Pos = $script:NebulaAt })) {
            if (-not $t.Pos) { "no $($t.Name) point given for this system"; continue }
            Send-FLConsole ('pos {0}, {1}, {2}' -f $t.Pos[0], $t.Pos[1], $t.Pos[2])
            Start-Sleep -Seconds 2
            $s = Sig
            Check ($s.field -eq $t.Want) "$($t.Name): field=$($s.field) at $($t.Pos -join ', ')"
        }
    }
    if ('bump' -in $Row) {
        '--- bump'
        # `pos` keeps the ship's heading: put it on the line through the station along its own nose, then burn straight in
        $st = Resolve-FLContact $script:BumpInto
        $null = Set-FLTarget $script:BumpInto
        $at = Get-FLTargetPos
        $radius = [double][regex]::Match((Send-FL 'targetinfo'), 'radius=(\d+)').Groups[1].Value
        $s = Sig; $nose = @($s.nose -split ',' | ForEach-Object { [double]$_ })
        $stand = $radius + 400
        "station: $($st.Name) radius $radius, standing $stand m off along the nose $($s.nose)"
        Send-FLConsole ('pos {0:0}, {1:0}, {2:0}' -f ($at[0] - $nose[0] * $stand), ($at[1] - $nose[1] * $stand), ($at[2] - $nose[2] * $stand))
        Start-Sleep -Milliseconds 500
        $seq0 = [uint32](Sig).hit_seq; $hull0 = (Sig).hull
        Set-FLSdlPad -Slot 0 -LT 1
        $t0 = Get-Date; $k = ''
        while (((Get-Date) - $t0).TotalSeconds -lt 25) {
            $s = Sig
            if ([uint32]$s.hit_seq -ne $seq0) { $k = "kind=$($s.hit_kind) pan=$($s.hit_pan) dist=$($s.hit_dist) hull=$($s.hull)"; break }
            Start-Sleep -Milliseconds 100
        }
        Set-FLSdlPad -Slot 0
        "first report: $k (hull was $hull0)"
        Check ($k -match 'kind=2') 'a bump reads as hit kind 2'
        (Send-FL 'haptics status') -replace '.* last=', 'last='
        Shot 'bump'
    }
    if ('gates' -in $Row) {
        '--- gates'
        function St([string]$at) { $x = Sig; "  [$at] open=$($x.open) ship=$($x.ship) excluded=$($x.excluded) piece=$($x.piece) mode=$([regex]::Match((Send-FL 'state'), 'mode=(\S+)').Groups[1].Value) $(((Send-FL 'haptics status') -replace '.* open=', 'open='))" }
        $null = Send-FL 'ap stop'; Start-Sleep -Milliseconds 500
        $null = Wait-FLUntil { (Sig).open -eq '1' } 'stances open' 8
        $null = Wait-FLFramesPassed 20
        $on = (Send-FL 'haptics status') -match 'stance=1'
        Check $on 'a stance stands in flight'
        # pause
        $seq = LastSeq
        $null = Send-FL 'pause'
        $null = Wait-FLUntil { (Send-FL 'state') -match 'paused=1' } 'the game paused' 5
        $null = Wait-FLUntil { @(Effects $seq).Count } 'the stance Off' 3
        $e = Effects $seq
        Check ($e.Count -ge 1 -and (Mode $e[-1]) -eq '05/05') "Off at the pause: $(($e | ForEach-Object { Mode $_ }) -join ' ')"
        $null = Wait-FLUntil { (Send-FL 'visible PauseButtonBox0') -match '^visible' } 'the pause menu' 5
        $null = Send-FL 'click PauseButtonBox0'
        $null = Wait-FLUntil { (Send-FL 'state') -match 'paused=0' } 'resumed' 10
        $null = Wait-FLFramesPassed 20
        Check ((Send-FL 'haptics status') -match 'stance=1') 'back after the pause'
        St 'after the pause'
        # a window
        $seq = LastSeq
        function ToMode([string]$want) { # (a tap can go unheard in the first frames after a resume: asked again, by the mode the game reports)
            foreach ($try in 1..4) {
                $null = Send-FL 'tap USER_NAV_MAP'
                foreach ($i in 1..12) { $null = Wait-FLFramesPassed 10; if ((Send-FL 'state') -match "mode=$want\b") { return } }
            }
            throw "the map did not reach mode $want"
        }
        ToMode 'space-ui'
        $null = Wait-FLFramesPassed 20
        St 'map open'
        $e = Effects $seq
        Check ($e.Count -ge 1 -and (Mode $e[-1]) -eq '05/05') "Off under a window: $(($e | ForEach-Object { Mode $_ }) -join ' ')"
        ToMode 'space'
        $null = Wait-FLUntil { (Send-FL 'haptics status') -match 'stance=1' } 'the stance back after the window' 5
        # the autopilot
        $seq = LastSeq
        $null = Send-FL 'ap start hold'; $null = Wait-FLFramesPassed 40
        St 'autopilot'
        $e = Effects $seq
        Check ($e.Count -ge 1 -and (Mode $e[-1]) -eq '05/05') "Off under the autopilot: $(($e | ForEach-Object { Mode $_ }) -join ' ')"
        $null = Send-FL 'ap stop'; $null = Wait-FLFramesPassed 40
        St 'autopilot stopped'
        # focus: the test pad's foreground flag off
        $seq = LastSeq
        Set-FLSdlForeground $false
        $null = Wait-FLFramesPassed 20
        $e = Effects $seq
        Check ($e.Count -ge 1 -and (Mode $e[-1]) -eq '05/05') "Off out of focus: $(($e | ForEach-Object { Mode $_ }) -join ' ')"
        Set-FLSdlForeground $true
        $null = Wait-FLFramesPassed 30
        # the lease: the game thread stops; the host must rest the pad by itself, and the logic must put the stance back after
        $seq = LastSeq
        Check ((Send-FL 'haptics status') -match 'stance=1') 'a stance stands before the stall'
        "$(Send-FL 'haptics stall 1800')"
        $e = Effects $seq
        Check ($e.Count -ge 1 -and (Mode $e[0]) -eq '05/05') "the lease: Off from the host in the stall: $(($e | ForEach-Object { Mode $_ }) -join ' ')"
        Check (@(LogLines 'nothing from the logic').Count -ge 1) 'the host logged it'
        $null = Wait-FLFramesPassed 40
        $e = Effects $seq
        Check ($e.Count -ge 2 -and (Mode $e[-1]) -ne '05/05') "the stance comes back after it: $(($e | ForEach-Object { Mode $_ }) -join ' ')"
        # the module's stop: a hot reload sends Off
        $seq = LastSeq
        "hot reload: $(Send-FL 'reload')"
        Start-Sleep -Seconds 3
        $e = Effects $seq
        Check ($e.Count -ge 1 -and (Mode $e[0]) -eq '05/05') "Off at a reload: $(($e | ForEach-Object { Mode $_ }) -join ' ')"
    }
} finally {
    try { Set-FLSdlPad -Slot 0; $null = Send-FL 'sdlpad detach all'; $null = Send-FL 'set pad_backend 0' } catch {}
    "real pad check: $(@(LogLines 'in use' | Where-Object { $_ -notmatch 'test pad' }).Count) lines of a real pad in use"
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
}
if ($bad.Count) { "FAILED: $($bad.Count)"; $bad; exit 1 }
'all checks passed'
