# Backup: git bundles of FLvibed (which holds the subtitle review in subtitles\review since 25 Oct 2026) and, while it
# is still on disk, the archived pre-merge review repository ..\FLvibed-subtitle-review, to another drive.
# Local-only git with no remote is one disk failure away from losing everything (audit, 21 Oct 2026); a bundle is a
# complete copy of the repository (all branches, full history), not a push. Restore: git clone <bundle> <folder>.
# Keeps the newest -Keep bundles per repository. Run from anywhere (paths resolve from this script's location).
# Destination (no machine path in the repository): -Dest, else $env:FLVIBED_BACKUP_DIR, else the first line of the
# local, git-ignored file .backup_dir in the repository root. -ShowDest prints the resolved folder and stops
# (drift_check.ps1 reads it).
param(
    [string]$Dest = '',
    [int]$Keep = 10,
    [switch]$ShowDest
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot             # FLvibed
$env_ = Split-Path -Parent $root                     # the environment folder
if (-not $Dest) { $Dest = $env:FLVIBED_BACKUP_DIR }
$local = Join-Path $root '.backup_dir'
if (-not $Dest -and (Test-Path $local)) { $Dest = (Get-Content $local -TotalCount 1).Trim() }
if ($ShowDest) { return $Dest }
if (-not $Dest) { throw "no backup folder: pass -Dest, set FLVIBED_BACKUP_DIR or write it into $local" }
$repos = @(
    @{ Name = 'FLvibed'; Path = $root },
    @{ Name = 'FLvibed-subtitle-review'; Path = (Join-Path $env_ 'FLvibed-subtitle-review'); Optional = $true }
)
if (-not (Test-Path $Dest)) { New-Item -ItemType Directory -Path $Dest | Out-Null }
$stamp = Get-Date -Format 'yyyy-MM-dd_HHmm'
foreach ($r in $repos) {
    if (-not (Test-Path (Join-Path $r.Path '.git'))) { if (-not $r.Optional) { Write-Warning "no repository at $($r.Path)" }; continue }
    $file = Join-Path $Dest "$($r.Name)_$stamp.bundle"
    & git -C $r.Path bundle create $file --all 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "git bundle failed for $($r.Name)" }
    & git -C $r.Path bundle verify $file 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "bundle verify failed: $file" }
    $size = [math]::Round((Get-Item $file).Length / 1MB, 1)
    $head = (& git -C $r.Path log -1 --format='%h %s').Trim()
    Write-Output "$($r.Name): $file ($size MB) at $head"
    Get-ChildItem $Dest -Filter "$($r.Name)_*.bundle" | Sort-Object LastWriteTime -Descending |
        Select-Object -Skip $Keep | Remove-Item -Force
}
