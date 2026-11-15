# One build at a time: build.cmd cleans and rewrites build\<config>\, so two builds started together break each other
# (parallel agents or sessions, 24 Oct 2026). This wrapper takes a machine-wide mutex, waits for the running build if
# there is one, then runs build.cmd with the given target and prints its result and time.
# Usage (PowerShell): tools\buildlock.ps1 [dev|impl|release|sigtool|test|setup] [extra build.cmd argument, e.g. test's
# install folder]   exit code = build.cmd's.   tools\buildlock.ps1 -MutexName: prints this checkout's lock name, builds nothing.
param([string]$Target = 'dev', [string]$Arg = '', [switch]$MutexName)
$root = Split-Path $PSScriptRoot -Parent

# The lock's scope (12 Nov 2026, parallel lanes: tools\lane.ps1). Two builds break each other only in the SAME build\
# folder, and each checkout has its own. The main checkout (and any copy outside git) keeps the one machine-wide name
# its sessions and agents share; a linked git worktree (a lane, ..\FLvibed-lane-<name>) locks its own folder only, so
# lanes build beside main and beside each other (the owner wants that when the machine allows it: docs\TESTING.md
# "Parallel lanes" has the measurement). tools\lane.ps1 Get-BuildMutexName computes the same lane name: keep them equal.
function Get-FLBuildMutexName([string]$Root) {
    $gd = "$(git -C $Root rev-parse --path-format=absolute --git-dir 2>$null)"
    $cd = "$(git -C $Root rev-parse --path-format=absolute --git-common-dir 2>$null)"
    if (-not $gd -or -not $cd -or [IO.Path]::GetFullPath($gd) -ieq [IO.Path]::GetFullPath($cd)) { return 'Global\FLvibedBuild' }
    $h = [Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes([IO.Path]::GetFullPath($Root).TrimEnd('\').ToLowerInvariant()))
    'Global\FLvibedBuild_' + ([BitConverter]::ToString($h, 0, 6) -replace '-', '')
}
$lockName = Get-FLBuildMutexName $root
if ($MutexName) { $lockName; exit 0 }

# What a build was built from (29 Oct 2026; the leg history recorded HEAD + "+dirty" from a plain git status, which the
# leg bookkeeping itself dirties - 699 of 881 rows): build\<config>\build_info.txt = "<HEAD>[+dirty] src:<hash8|clean>
# <target> <time>", where dirty / the hash cover only the build's inputs (core, the modules' src, build.cmd: uncommitted
# changes and new files). deploy.ps1 copies it next to the DLLs as FLvibed.build.txt; flleg.ps1 records it.
function Write-FLBuildInfo([string]$Root, [string]$Target) {
    $paths = 'core', 'modules/*/src/*', 'build.cmd'
    if ($Target -ieq 'bench') { $paths += 'tools/hudbench' } # (the design bench: its own sources too)
    $sha = "$(git -C $Root rev-parse --short HEAD)"
    $changes = "$(git -C $Root diff HEAD -- @paths | Out-String)$((git -C $Root ls-files -o --exclude-standard -- @paths) -join "`n")"
    $src = 'clean'
    if ($changes.Trim()) {
        $hash = [Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes($changes))
        $src = ([BitConverter]::ToString($hash) -replace '-', '').Substring(0, 8).ToLower()
        $sha += '+dirty'
    }
    $cfg = if ($Target -ieq 'release') { 'release' } elseif ($Target -ieq 'bench') { 'bench' } else { 'dev' }
    Set-Content (Join-Path $Root "build\$cfg\build_info.txt") "$sha src:$src $Target $(Get-Date -Format s)"
}
$mutex = New-Object System.Threading.Mutex($false, $lockName)
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
    if ($code -eq 0 -and $Target -in 'dev', 'impl', 'release', 'bench') { Write-FLBuildInfo $root $Target }
    # the unit tests' failures are their own lines (no "error" in them): show the whole run when they fail
    if ($Target -ieq 'test' -and $code -ne 0) { $out } else { $out | Select-String -Pattern 'warning|error' | ForEach-Object { "$_" } }
    "$Target build: exit $code, $([int]$sw.Elapsed.TotalSeconds) s"
    exit $code
} finally {
    $mutex.ReleaseMutex()
    $mutex.Dispose()
}
