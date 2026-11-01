# One speedrun leg (controller\tools\flleg.ps1) under a wall-clock watchdog, for the set drivers (flset / flgate /
# flarms; 29 Oct 2026, tools audit - SWEEP_2026-10-29.md #11). The drivers called flleg in their own process: a hang
# outside the harness waits (a stuck ffmpeg, a game that never answers at the boot) stalled the whole queue until
# someone noticed (memory watchdog_stuck_tests). Here the leg runs as a PowerShell job (a child process; its
# parameters travel as objects, arrays intact); past its -TimeoutSec (default 3600) + GraceSec (boot, the end clip, the
# tables) the job is stopped, the instance's game closed, and the history gets an honest row: FAIL, "WATCHDOG".
# Usage (dot-source): . tools\fllegrun.ps1; $lines = Invoke-FLLegGuarded @{ Instance = 1; Save = 'Save7e1c.fl';
#   Label = 'M01a-s1'; NoTables = $true; ApSet = @('k v') }      -> flleg's output lines (strings), RESULT last
function Get-FLHarnessHash {
    # the harness a leg loads (flleg, Leg.ps1, the flctl parts): one hash over their bytes
    $root = Split-Path $PSScriptRoot -Parent
    $files = @(Join-Path $root 'controller\tools\flleg.ps1') + @(Get-ChildItem (Join-Path $root 'controller\tools') -Filter 'flctl*.ps1' | ForEach-Object FullName) +
             @(Join-Path $root 'controller\tools\missions\Leg.ps1')
    $sha = [Security.Cryptography.SHA256]::Create()
    $all = [IO.MemoryStream]::new()
    foreach ($f in $files | Sort-Object) { $b = [IO.File]::ReadAllBytes($f); $all.Write($b, 0, $b.Length) }
    [BitConverter]::ToString($sha.ComputeHash($all.ToArray())).Replace('-', '').Substring(0, 16)
}
function Invoke-FLLegGuarded([Parameter(Mandatory)][hashtable]$Leg, [int]$GraceSec = 900) {
    $root = Split-Path $PSScriptRoot -Parent
    $flleg = Join-Path $root 'controller\tools\flleg.ps1'
    # one harness per set (30 Oct 2026 evening: Leg.ps1 was edited while two sets ran - a set's legs must all run the
    # same harness to be comparable; the "half-edited copy" first blamed for that round was a misread, aa85f7b): the
    # set's first leg pins the harness hash, a later leg on a changed harness is not started (the driver's loop ends on
    # the error; finish the set's change, start anew)
    $h = Get-FLHarnessHash
    if (-not $script:FLHarnessPin) { $script:FLHarnessPin = $h }
    elseif ($script:FLHarnessPin -ne $h) { throw "harness changed mid-set (flleg / Leg.ps1 / flctl*: $($script:FLHarnessPin) -> $h): this leg is not started - a set runs on one harness" }
    $limit = $(if ($Leg.TimeoutSec) { [int]$Leg.TimeoutSec } else { 3600 }) + $GraceSec
    # only parameters with a value: an empty array crosses into the job as one empty string (29 Oct 2026: -ApSet @()
    # arrived as '' and flleg sent "ap set " - "unknown setting" ended the leg at its boot)
    $pass = @{}
    foreach ($k in $Leg.Keys) {
        $v = $Leg[$k]
        if ($v -is [array]) { $v = @($v | Where-Object { "$_" -ne '' }); if (-not $v.Count) { continue } }
        elseif ($null -eq $v -or "$v" -eq '') { continue }
        $pass[$k] = $v
    }
    $job = Start-Job -ScriptBlock { param($s, $p) & $s @p *>&1 | ForEach-Object { "$_" } } -ArgumentList $flleg, $pass
    if (Wait-Job $job -Timeout $limit) {
        $out = @(Receive-Job $job)
        Remove-Job $job -Force
        return $out
    }
    $partial = @(Receive-Job $job -ErrorAction SilentlyContinue)
    Stop-Job $job
    Remove-Job $job -Force
    # the leg's own cleanup never ran: close the instance's game, write the row flleg would have written
    $was = $env:FLVIBED_INSTANCE
    try {
        $env:FLVIBED_INSTANCE = "$($Leg.Instance)"
        . (Join-Path $PSScriptRoot 'fldata.ps1')
        # a start the stopped job had already sent can still bring the game up: close it until none shows for 5 s
        # (the first probe, 29 Oct 2026, found no game at the stop and a running one a moment later)
        $quiet = [Diagnostics.Stopwatch]::StartNew()
        $cap = [Diagnostics.Stopwatch]::StartNew()
        while ($quiet.Elapsed.TotalSeconds -lt 5 -and $cap.Elapsed.TotalSeconds -lt 60) {
            $g = Get-FLGameProcess
            if ($g) { Stop-Process -Id $g.Id -Force; $quiet.Restart() }
            Start-Sleep -Milliseconds 250
        }
    } finally { $env:FLVIBED_INSTANCE = $was }
    $stamp = Join-Path $root 'build\dev\build_info.txt'
    $commit = if (Test-Path $stamp) { "$(Get-Content $stamp -TotalCount 1)".Split(' ')[0] } else { '?' }
    $hist = Join-Path $root 'research\experience\legs\legs.tsv'
    $why = "WATCHDOG: no result in $limit s (job stopped, game closed)"
    Add-Content $hist ("{0:yyyy-MM-dd HH:mm}`t{1}`t{2}`tFAIL`t{3}`t`t`t{4}`t{5}`t{6}" -f (Get-Date), $Leg.Label, $Leg.Save, $limit,
                       $commit, $Leg.Instance, $why)
    @($partial) + "RESULT FAIL $($Leg.Label) $limit s fps=0 ($why)"
}
