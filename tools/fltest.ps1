# One way to start any test, the cheapest probe included (user, 20 Oct 2026: logging and screenshot sampling in every
# test): the game in the background on a fixture, the event log, the screenshot sampler checked (a test without frames
# stops at once), and at the end one run folder research\experience\runs\<session>_<name>\ with the test's own notes,
# the full event stream and a manifest naming the session's archived logs (EXE\logs\<session>_*.log) and frames
# (research\experience\shots\<session>). Optional benchmark rows (tools\flbench.ps1).
# Usage (dot-source in the PowerShell tool):
#   . tools\fltest.ps1
#   $t = Start-FLTest repair-probe -Save SaveC707.fl [-NoBuild]
#   Write-FLTest $t "note"                 # timestamped line in the run's notes.txt (and the console)
#   Stop-FLTest $t [-KeepGame]             # saves events + manifest, closes the game (the standing rule)
$script:FLTestRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $script:FLTestRoot 'controller\tools\flctl.ps1')
. (Join-Path $script:FLTestRoot 'subtitles\tools\flsub.ps1')
. (Join-Path $script:FLTestRoot 'tools\flbench.ps1')

function Get-FLSession {
    $f = Join-Path $script:FLTestRoot 'Install\Freelancer HD Edition\EXE\logs\current-session.txt'
    if (Test-Path $f) { (Get-Content $f -TotalCount 1).Trim() }
}

# The sampler of this session writes frames into shots\<session>: wait (state, not a fixed delay) for its first one.
function Assert-FLShots([string]$Session, [int]$TimeoutSec = 30) {
    $dir = Join-Path $script:FLTestRoot "research\experience\shots\$Session"
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
        if (Test-Path $dir) { if (Get-ChildItem $dir -Filter '*.jpg' -ErrorAction SilentlyContinue | Select-Object -First 1) { return $dir } }
        if (-not @(Get-CimInstance Win32_Process -Filter "Name='pwsh.exe'" | Where-Object { $_.CommandLine -match 'flshots\.ps1' }).Count) {
            throw "screenshot sampler is not running (session $Session)"
        }
        Start-Sleep -Milliseconds 250
    }
    throw "no screenshot frame within $TimeoutSec s ($dir)"
}

function Start-FLTest([Parameter(Mandatory)][string]$Name, [string]$Save, [switch]$NoBuild, [switch]$Release) {
    $a = @{ Background = $true; Restart = $true }
    if ($Save) { $a.Save = $Save }
    if ($NoBuild) { $a.NoBuild = $true }
    if ($Release) { $a.Release = $true }
    $deployOut = & (Join-Path $script:FLTestRoot 'tools\deploy.ps1') @a 2>&1
    $session = Get-FLSession
    $run = Join-Path $script:FLTestRoot "research\experience\runs\${session}_$Name"
    $null = New-Item -ItemType Directory -Force $run
    $t = [pscustomobject]@{ Name = $Name; Session = $session; Run = $run; Save = $Save; Start = Get-Date
                            Commit = Get-FLBenchCommit; Shots = $null; EventSeq = 0 }
    $deployOut | ForEach-Object { "$_" } | Set-Content (Join-Path $run 'deploy.txt')
    $t.Shots = Assert-FLShots $session
    $t.EventSeq = Start-FLEventLog
    Write-FLTest $t "start $Name save=$Save commit=$($t.Commit) mode=$(Get-FLMode)"
    $t
}

function Write-FLTest($T, [string]$Line) {
    $l = "{0:HH:mm:ss} {1}" -f (Get-Date), $Line
    Add-Content (Join-Path $T.Run 'notes.txt') $l
    Write-Host $l
}

function Stop-FLTest($T, [switch]$KeepGame) {
    try { Get-FLEventLog $T.EventSeq | ForEach-Object { "$_" } | Set-Content (Join-Path $T.Run 'events.txt') }
    catch { Write-FLTest $T "events not read: $_" }
    $logs = Join-Path $script:FLTestRoot 'Install\Freelancer HD Edition\EXE'
    @("name     $($T.Name)", "session  $($T.Session)", "commit   $($T.Commit)", "save     $($T.Save)",
      "start    $($T.Start.ToString('s'))", "end      $((Get-Date).ToString('s'))",
      "shots    $($T.Shots)",
      "log      $logs\FLController.log (archived as EXE\logs\$($T.Session)_FLController.log at the next start)") |
        Set-Content (Join-Path $T.Run 'manifest.txt')
    if (-not $KeepGame) { Get-FLGameProcess | Stop-Process -Force }
    $T.Run
}
