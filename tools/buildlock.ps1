# One build at a time: build.cmd cleans and rewrites build\<config>\, so two builds started together break each other
# (parallel agents or sessions, 24 Oct 2026). This wrapper takes a machine-wide mutex, waits for the running build if
# there is one, then runs build.cmd with the given target and prints its result and time.
# Usage (PowerShell): tools\buildlock.ps1 [dev|impl|release|sigtool|test]   exit code = build.cmd's
param([string]$Target = 'dev')
$root = Split-Path $PSScriptRoot -Parent
$mutex = New-Object System.Threading.Mutex($false, 'Global\FLvibedBuild')
$waited = [Diagnostics.Stopwatch]::StartNew()
if (-not $mutex.WaitOne(0)) {
    "waiting for the running build..."
    $null = $mutex.WaitOne()
}
try {
    if ($waited.Elapsed.TotalSeconds -gt 1) { "waited $([int]$waited.Elapsed.TotalSeconds) s for the lock" }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $out = & cmd /c "`"$root\build.cmd`" $Target" 2>&1 | Out-String -Stream
    $code = $LASTEXITCODE
    $sw.Stop()
    $out | Select-String -Pattern 'warning|error' | ForEach-Object { "$_" }
    "$Target build: exit $code, $([int]$sw.Elapsed.TotalSeconds) s"
    exit $code
} finally {
    $mutex.ReleaseMutex()
    $mutex.Dispose()
}
