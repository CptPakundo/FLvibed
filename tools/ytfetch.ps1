# Downloads research videos (combat study) fast and silently: video only (no audio track), <= $MaxHeight, into
# $Dest (keep it under research\, git-ignored; one folder per source, no scripts in it). Downloads need the user's OK.
# Speed (measured 17 Oct 2026): yt-dlp's default fetches fragments one at a time (~3 MiB/s); -N 16 (concurrent
# fragments) gave 18-35 MiB/s per process, and several processes in parallel add up. formats=dashy splits the https
# DASH formats into fragments too, so -N applies to them as well (yt-dlp issues #14229 / #15996 advice). YouTube
# needs a JS runtime for its challenge (Deno, found on PATH: `yt-dlp -v` lists "JS runtimes").
# -Sections '*120-300' fetches only that part (seconds or hh:mm:ss): scan a 360p copy for the fights first.
# Use: tools\ytfetch.ps1 -Dest research\videos\pvp -Ids id1,id2 [-Parallel 3] [-MaxHeight 1080] [-Sections '*60-180']
param([Parameter(Mandatory)][string]$Dest, [Parameter(Mandatory)][string[]]$Ids, [int]$Parallel = 3,
      [int]$MaxHeight = 1080, [int]$Fragments = 16, [string]$Sections)
$ytdlp = (Get-Command yt-dlp -ErrorAction Stop).Source
New-Item -ItemType Directory -Force $Dest | Out-Null
$common = @('-N', $Fragments, '--extractor-args', 'youtube:formats=dashy',
            '-f', "bv*[height<=$MaxHeight][ext=mp4]/bv*[height<=$MaxHeight]", '-o', '%(id)s.%(ext)s', '--no-progress')
if ($Sections) { $common += @('--download-sections', $Sections) }
$groups = @{}; for ($i = 0; $i -lt $Ids.Count; $i++) { $groups[$i % $Parallel] += @($Ids[$i]) }
$sw = [Diagnostics.Stopwatch]::StartNew()
$procs = foreach ($g in $groups.Values) {
    $log = Join-Path $Dest ("ytfetch-{0}.log" -f $g[0].TrimStart('-'))
    Start-Process -FilePath $ytdlp -ArgumentList ($common + @('--') + $g) -WorkingDirectory $Dest -NoNewWindow -PassThru `
        -RedirectStandardOutput $log -RedirectStandardError "$log.err"
}
$procs | Wait-Process
$mb = ($Ids | ForEach-Object { Get-ChildItem $Dest -Filter "$_.*" -File | Where-Object Extension -in '.mp4', '.webm' } |
    Measure-Object Length -Sum).Sum / 1MB
'{0} videos, {1:N0} MiB in {2:N0} s ({3:N1} MiB/s)' -f $Ids.Count, $mb, $sw.Elapsed.TotalSeconds, ($mb / [math]::Max(1, $sw.Elapsed.TotalSeconds))
