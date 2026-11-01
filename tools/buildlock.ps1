# One build at a time: build.cmd cleans and rewrites build\<config>\, so two builds started together break each other
# (parallel agents or sessions, 24 Oct 2026). This wrapper takes a machine-wide mutex, waits for the running build if
# there is one, then runs build.cmd with the given target and prints its result and time.
# Usage (PowerShell): tools\buildlock.ps1 [dev|impl|release|sigtool|test|setup] [extra build.cmd argument, e.g. test's
# install folder]   exit code = build.cmd's
param([string]$Target = 'dev', [string]$Arg = '')
$root = Split-Path $PSScriptRoot -Parent

# What a build was built from (29 Oct 2026; the leg history recorded HEAD + "+dirty" from a plain git status, which the
# leg bookkeeping itself dirties - 699 of 881 rows): build\<config>\build_info.txt = "<HEAD>[+dirty] src:<hash8|clean>
# <target> <time>", where dirty / the hash cover only the build's inputs (core, the modules' src, build.cmd: uncommitted
# changes and new files). deploy.ps1 copies it next to the DLLs as FLvibed.build.txt; flleg.ps1 records it.
function Write-FLBuildInfo([string]$Root, [string]$Target) {
    $paths = 'core', 'controller/src', 'subtitles/src', 'build.cmd'
    $sha = "$(git -C $Root rev-parse --short HEAD)"
    $changes = "$(git -C $Root diff HEAD -- @paths | Out-String)$((git -C $Root ls-files -o --exclude-standard -- @paths) -join "`n")"
    $src = 'clean'
    if ($changes.Trim()) {
        $hash = [Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($changes))
        $src = ([BitConverter]::ToString($hash) -replace '-', '').Substring(0, 8).ToLower()
        $sha += '+dirty'
    }
    $cfg = if ($Target -ieq 'release') { 'release' } else { 'dev' }
    Set-Content (Join-Path $Root "build\$cfg\build_info.txt") "$sha src:$src $Target $(Get-Date -Format s)"
}
$mutex = New-Object System.Threading.Mutex($false, 'Global\FLvibedBuild')
$waited = [Diagnostics.Stopwatch]::StartNew()
try {
    if (-not $mutex.WaitOne(0)) {
        "waiting for the running build..."
        $null = $mutex.WaitOne()
    }
} catch [System.Threading.AbandonedMutexException] {
    "the previous build's process died holding the lock: taken over"   # WaitOne owns the mutex now
}
try {
    if ($waited.Elapsed.TotalSeconds -gt 1) { "waited $([int]$waited.Elapsed.TotalSeconds) s for the lock" }
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $extra = if ($Arg) { " `"$Arg`"" } else { '' }
    $out = & cmd /c "`"$root\build.cmd`" $Target$extra" 2>&1 | Out-String -Stream
    $code = $LASTEXITCODE
    $sw.Stop()
    # the stamp before any output: a caller's `| Select-Object -First n` stops this script at its n-th line, and the stamp
    # written after the output was then never written (29 Oct 2026: four dev builds kept the 12:34 stamp of an older HEAD)
    if ($code -eq 0 -and $Target -in 'dev', 'impl', 'release') { Write-FLBuildInfo $root $Target }
    # the unit tests' failures are their own lines (no "error" in them): show the whole run when they fail
    if ($Target -ieq 'test' -and $code -ne 0) { $out } else { $out | Select-String -Pattern 'warning|error' | ForEach-Object { "$_" } }
    "$Target build: exit $code, $([int]$sw.Elapsed.TotalSeconds) s"
    exit $code
} finally {
    $mutex.ReleaseMutex()
    $mutex.Dispose()
}
