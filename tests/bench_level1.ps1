# The design bench's level 1 (audit 71b section 5, B5a): every recorded frame of the fixture runs is replayed by the bench
# with no game (`flbench replay`, tools\hudbench\replay.*: Step and Draw from the record, the host's answers from its
# tape) and the bench's display list is compared with the game's list beside the record: vertex counts per layer and
# mark, texture names, colours, positions within 0.01 px, the layers' fingerprints, the font sets' prints, the hotspots
# and Step's state. No game is started. Exit 0: every record equal (the holes named); otherwise flbench's code
# (3 a font differs, 4 a list differs, 5 a replay failed) or 1 (no build, no fixture).
#   tests\bench_level1.ps1 [-NoBuild] [-Runs b4b_run1,...] [-Install <game folder>] [-Out <folder>]
# The fixtures: research\experience\bench\record\<run> (tests\bench_record.ps1 makes them, one game start per size: the
# 25 simulated states, pad and K&M, 1280x800 and 3840x2160, with no 3D view, so no under pass: B4b's finding F2); the two
# verify_b4b_reload_* runs are live flight with a station targeted, 12 of their 15 frames with the under pass. A
# fixture shows what the game drew with the HUD 2 code of its build: after a change to what the flight HUD draws, record
# again (bench_record.ps1) before trusting a difference.
param(
    [switch]$NoBuild,
    # (rc047_run1: recorded 14 Nov 2026 on 0.4.7's candidate, after the rule that faded-out prompts hold no room changed
    # Step's state in the older runs b4b_run1 / b4b_run2 / b4b_sync_ef2f0afc; their drawn passes stayed equal)
    [string[]]$Runs = @('rc047_run1', 'verify_b4b_reload_184340', 'verify_b4b_reload_191825'),
    [string]$Install = '',
    [string]$Out = ''
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
if (-not $Install) { $Install = Join-Path $root 'Install\Freelancer HD Edition' }
if (-not $Out) { $Out = Join-Path $root ('research\experience\bench\level1\' + (Get-Date -Format 'yyyyMMdd_HHmmss')) }
if (-not $NoBuild) {
    & (Join-Path $root 'tools\buildlock.ps1') bench | Out-Null
    if ($LASTEXITCODE -ne 0) { Write-Output "bench_level1: the bench did not build (exit $LASTEXITCODE)"; exit 1 }
}
$exe = Join-Path $root 'build\bench\flbench.exe'
if (-not (Test-Path $exe)) { Write-Output "bench_level1: no $exe (build.cmd bench)"; exit 1 }
$worst = 0
foreach ($run in $Runs) {
    $folder = Join-Path $root "research\experience\bench\record\$run"
    if (-not (Test-Path $folder)) { Write-Output "bench_level1: no fixture run $folder"; exit 1 }
    $o = Join-Path $Out $run
    New-Item -ItemType Directory -Force $o | Out-Null
    $lines = & $exe replay --install $Install --out $o --brief $folder 2>&1
    $code = $LASTEXITCODE
    $lines | Where-Object { $_ -notmatch '^EQUAL ' } | ForEach-Object { "  $_" }
    Write-Output "bench_level1: $run exit $code (report $o\level1.txt)"
    if ($code -ne 0 -and ($worst -eq 0 -or $code -gt $worst)) { $worst = $code }
}
Write-Output $(if ($worst -eq 0) { 'bench_level1: PASS' } else { "bench_level1: FAIL ($worst)" })
exit $worst
