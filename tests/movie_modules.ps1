# Movies with Subtitles off and the reverse, and a reload during a movie (docs\MODULES_PLAN.md P6): background games that let the startup
# movies play. Screenshots in research\experience\runs\movie_modules. Prints what it read; exit 1 on a failure.
# Usage (PowerShell tool): tests\movie_modules.ps1 [-Row all|subtitles|movies|reload] [-Instance 1]
# all, subtitles, movies: build\release. reload: build\dev (a release never frees the logic).
param([string[]]$Row = @('all', 'subtitles', 'movies', 'reload'), [string]$Instance = 1)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$env:FLVIBED_INSTANCE = "$Instance"
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
$deploy = Join-Path $root 'tools\deploy.ps1'
$dir = Join-Path $root 'research\experience\runs\movie_modules'
$bad = [Collections.Generic.List[string]]::new()
function Check([bool]$ok, [string]$what) { "{0,-4} {1}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $what; if (-not $ok) { $bad.Add($what) } }
function Log-Of([string]$name) { $p = Join-Path (Get-FLInstallDir) "EXE\$name"; if (Test-Path $p) { Get-Content $p -Raw } else { '' } }
function Stop-Game { Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue; Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue }
function Movies-Started { ([regex]::Matches((Log-Of 'FLSubtitles.log'), 'PlayMovie\(.*\) -> 0')).Count }
function Frames-Now { [int]([regex]::Match((Send-FLS 'state'), '(?m)^frames (\d+)').Groups[1].Value) }
function Wait-Until([scriptblock]$cond, [int]$sec) { $t = (Get-Date).AddSeconds($sec); while ((Get-Date) -lt $t) { if (& $cond) { return $true }; Start-Sleep -Milliseconds 200 }; $false }

try {
    if ('all' -in $Row) {
        '--- all on: lines over a fitted picture'
        & $deploy -Background -NoBuild -Release -Intro -Instance $Instance -Modules subtitles,controller | Select-Object -Last 1
        $shown = Wait-FLSShown '.' -TimeoutSec 90
        Check ($shown -and $shown -ne 'timeout') "a line shows: $shown"
        Check ((Log-Of 'FLSubtitles.log') -match 'movie: lines in a child window') 'the lines are in the child window'
        Check ((Log-Of 'FLSubtitles.log') -match 'movie: fit in') 'the picture is fitted'
        Save-FLShot -Name 'all_on' -Dir $dir
        Stop-Game
    }
    if ('subtitles' -in $Row) {
        '--- Subtitles without Movies: lines, the picture as Movie.dll draws it'
        & $deploy -Background -NoBuild -Release -Intro -Instance $Instance -Modules subtitles,controller,movies=0 | Select-Object -Last 1
        $shown = Wait-FLSShown '.' -TimeoutSec 90
        Check ($shown -and $shown -ne 'timeout') "a line shows: $shown"
        Check ((Log-Of 'FLSubtitles.log') -match 'movie: lines in a child window') 'the lines are in the child window'
        $log = Log-Of 'FLSubtitles.log'
        Check ($log -notmatch 'movie: fit in') 'no fit'
        Check ($log -notmatch 'intro override') 'no intro override'
        Save-FLShot -Name 'subtitles_alone' -Dir $dir
        Stop-Game
    }
    if ('movies' -in $Row) {
        '--- Movies without Subtitles: a fitted picture, no lines'
        $job = Start-Job -ScriptBlock { param($d, $i) & $d -Background -NoBuild -Release -Intro -Instance $i -Modules 'controller,movies=1' 2>&1 | Out-Null } -ArgumentList $deploy, $Instance
        $fitted = Wait-Until { (Log-Of 'FLController.log') -match 'movie: fit in' } 60
        Check $fitted 'the picture is fitted'
        if ($fitted) { $null = Wait-Until { (Log-Of 'FLController.log') -match 'movie: fl_intro' } 45; Start-Sleep -Seconds 12; Save-FLShot -Name 'movies_alone' -Dir $dir }
        $log = Log-Of 'FLController.log'
        Check ($log -match 'Movie hooks: play=') 'the movie hooks are in'
        ($log -split "`n" | Where-Object { $_ -match 'movie: |intro' } | Select-Object -First 6) | ForEach-Object { '  ' + $_.Trim().Substring(0, [Math]::Min(120, $_.Trim().Length)) }
        Stop-Game
        Stop-Job $job -ErrorAction SilentlyContinue; Remove-Job $job -Force -ErrorAction SilentlyContinue
    }
    if ('reload' -in $Row) {
        '--- a reload during a movie (dev build)'
        & $deploy -Background -NoBuild -Intro -Instance $Instance -Modules subtitles,controller | Select-Object -Last 1
        Check (Wait-Until { (Movies-Started) -ge 1 } 60) 'a movie plays'
        Check ((Frames-Now) -eq 0) 'before the first frame'
        $started = Movies-Started
        $fits = ([regex]::Matches((Log-Of 'FLSubtitles.log'), 'movie: fit in')).Count
        "reload: $(Send-FLS 'reload' 15000)"
        Check ((Send-FLS 'ping') -eq 'pong') 'the game answers after the reload'
        Start-Sleep -Seconds 3
        Check ((Get-FLGameProcess) -and (Send-FLS 'ping') -eq 'pong') 'and goes on playing'
        "reload: $(Send-FLS 'reload' 15000)"
        Check ((Send-FLS 'ping') -eq 'pong') 'a second reload'
        Check ((Frames-Now) -eq 0 -and (Movies-Started) -eq $started) 'the same movie still plays'
        $null = Send-FLS 'movieskip'
        Check (Wait-Until { (Movies-Started) -gt $started } 20) 'the next movie starts on the new logic'
        Check (Wait-Until { ([regex]::Matches((Log-Of 'FLSubtitles.log'), 'movie: fit in')).Count -gt $fits } 20) 'and is fitted'
        Check ((Log-Of 'FLSubtitles.log') -notmatch 'FATAL') 'no fault in the log'
        Start-Sleep -Seconds 4
        Check ((Get-FLGameProcess) -and (Send-FLS 'ping') -eq 'pong') 'it plays on'
        $until = (Get-Date).AddSeconds(60)
        while ((Get-Date) -lt $until -and (Frames-Now) -le 0) { $null = Send-FLS 'movieskip'; Start-Sleep -Milliseconds 500 }
        Check ((Frames-Now) -gt 0) 'the main menu comes up'
        Stop-Game
    }
} finally {
    Stop-Game
    & $deploy -Instance $Instance -NoBuild -NoStart -Modules subtitles,controller | Out-Null
}
if ($bad.Count) { "FAILED: $($bad -join '; ')"; exit 1 }
'movie modules: PASS'
