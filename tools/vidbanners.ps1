# Objective banner timeline of reference footage (user, 20 Oct 2026: judge the autopilot against the 2020 playthrough,
# per objective): the HUD's centred banner ("FLY TO RESEARCH STATION WILLARD", "MISSION FAILED", ...) read once per
# second by Windows OCR (the banners' large font reads well; the HUD's small digits do not - tools\vidspeed.js). Output
# per video: research\videos\catalogue\tracks\<id>.banners.tsv - from, to, text (one row per banner shown; seconds of
# the video, like the other 1 fps tracks). No playback: ffmpeg decodes the crop band only.
# Runs at idle priority (never slows a game test running beside it).
# Usage: tools\vidbanners.ps1 [-Ids id,id] [-Missions] [-Between] [-Force]
param([string[]]$Ids, [switch]$Missions, [switch]$Between, [switch]$Force)
$root = Split-Path $PSScriptRoot -Parent
$cat = Join-Path $root 'research\videos\catalogue'
try { (Get-Process -Id $PID).PriorityClass = 'Idle' } catch {}
$ocr = Join-Path $PSScriptRoot 'winocr.ps1'
$videos = Import-Csv (Join-Path $cat 'videos.tsv') -Delimiter "`t" | Where-Object {
    ($Ids -and $_.id -in $Ids) -or ($Missions -and $_.mission -match '^M\d+(-\d+)?$') -or ($Between -and $_.mission -match '\.5$') }
foreach ($v in $videos) {
    $out = Join-Path $cat "tracks\$($v.id).banners.tsv"
    if ((Test-Path $out) -and -not $Force) { "skip $($v.id) (done)"; continue }
    $tmp = Join-Path $env:TEMP "vidbanners_$($v.id)"
    $null = New-Item -ItemType Directory -Force $tmp
    Get-ChildItem $tmp -Filter '*.png' | Remove-Item -Force
    # the banner band: centre 60 % of the width, 13-25 % of the height (HDE HUD, 16:9); 1 frame per second, gray
    $p = Start-Process ffmpeg -ArgumentList '-v', 'error', '-i', ('"' + (Join-Path $root "research\videos\$($v.path)") + '"'),
        '-vf', 'fps=1,crop=iw*0.6:ih*0.12:iw*0.2:ih*0.13,format=gray', ('"' + (Join-Path $tmp '%05d.png') + '"') -NoNewWindow -PassThru
    try { $p.PriorityClass = 'Idle' } catch {}
    $p.WaitForExit()
    $files = @(Get-ChildItem $tmp -Filter '*.png' | Sort-Object Name | ForEach-Object FullName)
    $lines = @()
    for ($i = 0; $i -lt $files.Count; $i += 200) {   # Windows PowerShell 5.1 for WinRT; batches keep the command short
        $batch = $files[$i..([Math]::Min($i + 199, $files.Count - 1))]
        $lines += @(& powershell -NoProfile -File $ocr @batch)
    }
    # rows: second (file N = second N-1) and text; banners = runs of the same text (letters only, upper case)
    $rows = foreach ($l in $lines) {
        $f, $t = $l -split "`t", 2
        $txt = (($t -replace '[^A-Za-z0-9 |:\-]', '') -replace '\s+', ' ').Trim()
        if ($txt -cmatch '[A-Z]{3,}') { [pscustomobject]@{ s = [int]($f -replace '\D', '') - 1; text = $txt } }
    }
    $runs = @(); $cur = $null
    foreach ($r in $rows) {
        if ($cur -and $cur.text -eq $r.text -and $r.s -le $cur.to + 2) { $cur.to = $r.s }
        else { $cur = [pscustomobject]@{ from = $r.s; to = $r.s; text = $r.text }; $runs += $cur }
    }
    "from`tto`ttext" | Set-Content $out -Encoding utf8
    $runs | Where-Object { $_.to -gt $_.from } | ForEach-Object { "$($_.from)`t$($_.to)`t$($_.text)" } | Add-Content $out -Encoding utf8
    Remove-Item -LiteralPath $tmp -Recurse -Force
    "$($v.id) $($v.mission): $(@($runs | Where-Object { $_.to -gt $_.from }).Count) banners -> $out"
}
