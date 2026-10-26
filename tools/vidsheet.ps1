# Contact sheet of successive gameplay frames for the combat study (no playback): $Count frames from $From at $Fps,
# each tile = the frame scaled down + an enlarged strip of the HUD's bottom bar (thrust %, power, speed), with the
# video time stamped on it. Use: tools\vidsheet.ps1 <video> -From 42 [-Fps 4] [-Cols 4] [-Rows 3] [-Out sheet.png]
param([Parameter(Mandatory)][string]$Video, [double]$From = 0, [double]$Fps = 4, [int]$Cols = 4, [int]$Rows = 3,
      [int]$TileWidth = 640, [string]$Out)
$ffmpeg = (Get-Command ffmpeg -ErrorAction Stop).Source
if (-not $Out) { $Out = Join-Path ([IO.Path]::GetTempPath()) ("sheet_{0}_{1}.png" -f [IO.Path]::GetFileNameWithoutExtension($Video), $From) }
$n = $Cols * $Rows
$dur = $n / $Fps + 0.5
# Bottom bar strip: x 37..63 %, y 95..99.5 % of the frame (thrust %, power bar, speed), scaled to the tile width.
$filter = "fps=$Fps,split[a][b];" +
    "[a]scale=${TileWidth}:-2[f];" +
    "[b]crop=iw*0.26:ih*0.045:iw*0.37:ih*0.95,scale=${TileWidth}:-2[s];" +
    "[f][s]vstack,drawtext=fontfile='C\:/Windows/Fonts/arial.ttf':text='%{pts\:hms}':x=8:y=8:fontsize=22:fontcolor=white:box=1:boxcolor=black@0.6," +
    "tile=${Cols}x${Rows}:padding=4"
$fromArg = ('{0:0.###}' -f ($From)).Replace(',', '.')
& $ffmpeg -v error -y -ss $fromArg -t $dur -copyts -i $Video -vf $filter -frames:v 1 $Out
if ($LASTEXITCODE -ne 0) { throw "ffmpeg failed ($LASTEXITCODE)" }
$Out
