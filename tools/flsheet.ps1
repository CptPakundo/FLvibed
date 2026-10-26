# Check-in contact sheet of the screenshot sampler's frames (tools\flshots.ps1; user, 19 Oct 2026): each check-in during
# an unattended test looks at every frame since the previous check-in AND the window before it (the bigger picture:
# a drift, a stall, a slow fight), tiled small with their times, in one image - one read instead of dozens.
#   tools\flsheet.ps1 [-Dir <session folder>] [-Since <HH:mm:ss | yyyyMMdd_HHmmss>] [-Last 40] [-Out <jpg>]
# -Dir: default the newest session in research\experience\shots. -Since: frames from then on (default: the newest
# -Last). The sheet keeps at most -Last frames (evenly thinned beyond that). Prints the sheet's path and the frame span;
# remembers the check-in in <Dir>\checkins.txt so the next call's default window = previous check-in's start onward.
param([string]$Dir, [string]$Since, [int]$Last = 40, [int]$TileWidth = 384, [int]$Cols = 5, [string]$Out)
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent
if (-not $Dir) { $Dir = (Get-ChildItem (Join-Path $root 'research\experience\shots') -Directory | Sort-Object LastWriteTime | Select-Object -Last 1).FullName }
$frames = @(Get-ChildItem $Dir -Filter '2*.jpg' | Sort-Object Name)
if (-not $frames.Count) { throw "no frames in $Dir" }
$log = Join-Path $Dir 'checkins.txt'
$prev = @(if (Test-Path $log) { Get-Content $log })
if (-not $Since -and $prev.Count -ge 2) { $Since = $prev[-2] }   # the previous check-in's window too
if ($Since) {
    if ($Since -match '^\d{1,2}:\d{2}(:\d{2})?$') { $Since = $frames[-1].BaseName.Substring(0, 9) + ($Since -replace ':', '').PadRight(6, '0') }
    $frames = @($frames | Where-Object { $_.BaseName -ge $Since })
}
if ($frames.Count -gt $Last) {   # evenly thinned, the newest frame always last
    $step = ($frames.Count - 1) / ($Last - 1)
    $frames = @(0..($Last - 1) | ForEach-Object { $frames[[int][Math]::Round($_ * $step)] })
}
Add-Content $log (Get-ChildItem $Dir -Filter '2*.jpg' | Sort-Object Name | Select-Object -Last 1).BaseName
# The session's log beside its frames (user, 19 Oct 2026: cross-check what the frames show against what the log says):
# the live EXE\FLController.log while the session is current (EXE\logs\current-session.txt = this folder's name), else
# its archive EXE\logs\<session>_FLController.log. Key events only (objectives, autopilot starts / stops / holds /
# missiles / evasion, deaths, mission results), each with its time.
$exe = Join-Path $root 'Install\Freelancer HD Edition\EXE'
$session = Split-Path $Dir -Leaf
$cur = Join-Path $exe 'logs\current-session.txt'
$logFile = if ((Test-Path $cur) -and (Get-Content $cur -TotalCount 1).Trim() -eq $session) { Join-Path $exe 'FLController.log' }
           else { Join-Path $exe "logs\${session}_FLController.log" }
if (-not (Test-Path -LiteralPath $logFile)) { $logFile = Join-Path $exe 'FLController.log' } # not archived yet: live
# A log belongs to the session only if it starts at or after the session's start (archives written by the old deploy
# were named by the NEXT start: they hold the previous session).
$startHms = ($session -replace '^\d{4}-\d\d-\d\d_', '')
$firstHms = ((Get-Content -LiteralPath $logFile -TotalCount 1 -ErrorAction SilentlyContinue) -replace '^\[(\d\d):(\d\d):(\d\d).*', '$1$2$3')
if ($firstHms -match '^\d{6}$' -and $firstHms -lt $startHms) { $logFile = Join-Path $exe 'FLController.log' }
$events = @()
if (Test-Path -LiteralPath $logFile) {
    $day = $frames[0].BaseName.Substring(0, 8)
    $from = $frames[0].BaseName.Substring(9); $to = $frames[-1].BaseName.Substring(9)
    # shared read: the game holds the live log open for writing
    $fs = [IO.FileStream]::new($logFile, 'Open', 'Read', 'ReadWrite'); $sr = [IO.StreamReader]::new($fs)
    $lines = $sr.ReadToEnd() -split "`r?`n"; $sr.Dispose()
    foreach ($l in $lines) {
        if ($l -notmatch '^\[(\d\d):(\d\d):(\d\d)\.\d+\] event (.*)$') { continue }
        $hms = $Matches[1] + $Matches[2] + $Matches[3]; $e = $Matches[4]
        if ($hms -lt $from -or $hms -gt $to) { continue }
        $txt = switch -regex ($e) {
            '^objective count=\d+ .*obj=(\d+) "([^"]*)"' { "objective $($Matches[1]) $($Matches[2])"; break }
            '^autopilot (start|stop) mode=(\w+)(?: target=(\d+))?(?:.*?why=(.+?) kills=(\d+))?' {
                "ap $($Matches[1]) $($Matches[2])" + $(if ($Matches[4]) { " ($($Matches[4]), kills $($Matches[5]))" }); break }
            '^autopilot (hold scenic|hold look|missile-launch|missile-outcome|evade-guns|travel plan|leave-guns)(.{0,70})' { "ap $($Matches[1])$($Matches[2])"; break }
            '^autopilot avoid strict=1' { 'ap avoid: inside a strict zone'; break }
            '^(player-death|mission-message type=\w+.{0,60})' { $Matches[1]; break }
            default { $null }
        }
        if ($txt) { $events += [pscustomobject]@{ T = $hms; Text = $txt } }
    }
}
$first = [Drawing.Image]::FromFile($frames[0].FullName)
$th = [int]($first.Height * $TileWidth / $first.Width); $first.Dispose()
$rows = [Math]::Ceiling($frames.Count / $Cols)
$sheet = New-Object Drawing.Bitmap ($Cols * ($TileWidth + 4)), ($rows * ($th + 4))
$g = [Drawing.Graphics]::FromImage($sheet); $g.Clear([Drawing.Color]::Black)
$g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBilinear
$font = New-Object Drawing.Font 'Arial', 13, ([Drawing.FontStyle]::Bold)
for ($i = 0; $i -lt $frames.Count; $i++) {
    $x = ($i % $Cols) * ($TileWidth + 4); $y = [Math]::Floor($i / $Cols) * ($th + 4)
    $img = [Drawing.Image]::FromFile($frames[$i].FullName)
    $g.DrawImage($img, $x, $y, $TileWidth, $th); $img.Dispose()
    $t = $frames[$i].BaseName.Substring(9) -replace '(\d{2})(\d{2})(\d{2})', '$1:$2:$3'
    $g.FillRectangle([Drawing.Brushes]::Black, $x, $y, 70, 20); $g.DrawString($t, $font, [Drawing.Brushes]::White, $x + 2, $y + 1)
    # Caption: the log's latest key event since the previous tile (what the log says this frame should show)
    $prevT = if ($i -gt 0) { $frames[$i - 1].BaseName.Substring(9) } else { '000000' }
    $nowT = $frames[$i].BaseName.Substring(9)
    $cap = $events | Where-Object { $_.T -gt $prevT -and $_.T -le $nowT } | Select-Object -Last 1
    if ($cap) {
        $small = New-Object Drawing.Font 'Arial', 9
        $c = if ($cap.Text.Length -gt 60) { $cap.Text.Substring(0, 60) } else { $cap.Text }
        $g.FillRectangle([Drawing.Brushes]::Black, $x, $y + $th - 16, $TileWidth, 16)
        $g.DrawString($c, $small, [Drawing.Brushes]::Yellow, $x + 2, $y + $th - 15)
        $small.Dispose()
    }
}
$g.Dispose()
if (-not $Out) { $Out = Join-Path $Dir ('sheet_' + $frames[-1].BaseName + '.jpg') }
# JPEG (was PNG: 12-30 MB of sheets per test session, 19 Oct 2026)
$codec = [Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -eq 'image/jpeg'
$ep = New-Object Drawing.Imaging.EncoderParameters 1
$ep.Param[0] = New-Object Drawing.Imaging.EncoderParameter ([Drawing.Imaging.Encoder]::Quality), 85L
$sheet.Save($Out, $codec, $ep); $sheet.Dispose()
"{0} ({1} frames {2} - {3})" -f $Out, $frames.Count, $frames[0].BaseName.Substring(9), $frames[-1].BaseName.Substring(9)
"log: $(if (Test-Path -LiteralPath $logFile) { Split-Path $logFile -Leaf } else { 'none found' }), $($events.Count) key events in the window"
$events | ForEach-Object { "  {0}:{1}:{2} {3}" -f $_.T.Substring(0, 2), $_.T.Substring(2, 2), $_.T.Substring(4, 2), $_.Text }
