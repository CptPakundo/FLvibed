# Backup: git bundles of FLvibed (which holds the subtitle review in subtitles\review since 25 Oct 2026) and, while it
# is still on disk, the archived pre-merge review repository ..\FLvibed-subtitle-review, to another drive.
# Local-only git with no remote is one disk failure away from losing everything (audit, 21 Oct 2026); a bundle is a
# complete copy of the repository (all branches, full history), not a push. Restore: git clone <bundle> <folder>.
# The audit folder (..\audit_2026-11-12, the driver's working documents: in no repository) goes along as one zip, audit_<stamp>.zip, without its
# history_rewrite_scratch (a rewrite scratch of 24 MB and more) and the runtime tools\slots; -IncludeScratch keeps the scratch. -DryRun lists what
# would be written (destination, files, size) and writes nothing.
# Keeps the newest -Keep bundles per repository and the newest -Keep audit zips. Run from anywhere (paths resolve from this script's location).
# Destination (no machine path in the repository): -Dest, else $env:FLVIBED_BACKUP_DIR, else the first line of the
# local, git-ignored file .backup_dir in the repository root. -ShowDest prints the resolved folder and stops
# (drift_check.ps1 reads it).
param(
    [string]$Dest = '',
    [int]$Keep = 10,
    [switch]$ShowDest,
    [switch]$DryRun,
    [switch]$IncludeScratch
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
if (-not $DryRun -and -not (Test-Path $Dest)) { New-Item -ItemType Directory -Path $Dest | Out-Null }
$stamp = Get-Date -Format 'yyyy-MM-dd_HHmm'
foreach ($r in $repos) {
    if (-not (Test-Path (Join-Path $r.Path '.git'))) { if (-not $r.Optional) { Write-Warning "no repository at $($r.Path)" }; continue }
    if ($DryRun) { Write-Output "$($r.Name): DRY RUN, would write $(Join-Path $Dest "$($r.Name)_$stamp.bundle") from $($r.Path)"; continue }
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

# the audit folder (in no repository): one zip
$audit = Join-Path $env_ 'audit_2026-11-12'
if (Test-Path -LiteralPath $audit) {
    $skip = @('\tools\slots\*'); if (-not $IncludeScratch) { $skip += '\history_rewrite_scratch\*' }
    $files = @(Get-ChildItem -LiteralPath $audit -Recurse -File | Where-Object { $rel = $_.FullName.Substring($audit.Length); -not ($skip | Where-Object { $rel -like $_ }) })
    $mb = [math]::Round(($files | Measure-Object Length -Sum).Sum / 1MB, 1)
    $zip = Join-Path $Dest "audit_$stamp.zip"
    if ($DryRun) { Write-Output "audit: DRY RUN, would write $zip from $($files.Count) files, $mb MB uncompressed (scratch $(if ($IncludeScratch) { 'included' } else { 'left out' }))"; return }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $z = [IO.Compression.ZipFile]::Open($zip, 'Create')
    try { foreach ($f in $files) { $null = [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($z, $f.FullName, ('audit_2026-11-12' + $f.FullName.Substring($audit.Length).Replace('\', '/')), 'Optimal') } }
    finally { $z.Dispose() }
    Write-Output "audit: $zip ($([math]::Round((Get-Item $zip).Length / 1MB, 1)) MB, $($files.Count) files)"
    Get-ChildItem $Dest -Filter 'audit_*.zip' | Sort-Object LastWriteTime -Descending | Select-Object -Skip $Keep | ForEach-Object { [IO.File]::Delete($_.FullName) }
}
