# Screenshot sampler for unattended tests (user, 19 Oct 2026): tests can go wrong in ways no event or timeout catches
# (the ship idling while the mission moves on, flying into a planet); frames of the game window, sampled while a test
# runs, let the agent see that and pause, analyse the logs and fix. Runs as its own hidden process beside the test:
# it only captures the window (PrintWindow - never the game's debug channel, never focus or input).
# deploy.ps1 -Background starts one per game session (a standing order). By hand:
#   Start-Process pwsh -WindowStyle Hidden -ArgumentList '-NoProfile','-File','"<path>\flshots.ps1"','-Dir','"<dir>"' (quote: spaces)
# Two outputs in -Dir:
# - every -IntervalSec a JPEG (-Width px wide, ~80 KB) as <yyyyMMdd_HHmmss>.jpg: the check-in sheets (tools\flsheet.ps1);
#   the newest -Keep stay (600 x 6 s = a whole test of up to an hour);
# - with -Fps > 0 (default 5) a video of every frame captured, piped into ffmpeg (H.264, -Width px) as one-minute
#   segments video_<yyyyMMdd_HHmmss>_<nnn>.mkv (readable while the test runs): the dense evidence around anything the
#   sheets show (user, 19 Oct 2026: 5 fps; measured 19 Oct: capture 42 ms + scale/encode 14 ms per frame at 3840x2160).
# Sessions are pruned by deploy.ps1 (the newest 15, none older than 3 days). Ends when the game has been gone for 60 s,
# or -MaxMinutes.
param([Parameter(Mandatory)][string]$Dir, [double]$IntervalSec = 6, [int]$Keep = 600, [int]$Width = 1280,
      [double]$Fps = 5, [double]$MaxMinutes = 180, [int]$Instance = 0)
$ErrorActionPreference = 'Continue'
# parallel test games: capture only this instance's game (tools\fldata.ps1 Get-FLGameProcess)
if ($Instance -gt 0) { $env:FLVIBED_INSTANCE = "$Instance" } else { Remove-Item Env:FLVIBED_INSTANCE -ErrorAction SilentlyContinue }
. (Join-Path $PSScriptRoot 'fldata.ps1')
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System; using System.Runtime.InteropServices;
public static class FLShots {
  [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern IntPtr SetThreadDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] public static extern IntPtr GetWindowDpiAwarenessContext(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  public struct RECT { public int L, T, R, B; }
}
'@
[void][FLShots]::SetProcessDpiAwarenessContext([IntPtr](-4))
New-Item -ItemType Directory -Force $Dir | Out-Null
$codec = [Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -eq 'image/jpeg'
$ep = New-Object Drawing.Imaging.EncoderParameters 1
$ep.Param[0] = New-Object Drawing.Imaging.EncoderParameter ([Drawing.Imaging.Encoder]::Quality), 70L
# The video encoder: JPEG frames in on stdin (image2pipe), constant frame rate out, a new file every minute.
$ff = $null
$ffmpeg = if ($Fps -gt 0) { (Get-Command ffmpeg -ErrorAction SilentlyContinue).Source }
function Start-Encoder {
    if (-not $ffmpeg) { return $null }
    $si = New-Object Diagnostics.ProcessStartInfo $ffmpeg
    $pattern = Join-Path $Dir ('video_' + (Get-Date -Format 'yyyyMMdd_HHmmss') + '_%03d.mkv')
    $rate = ('{0:0.###}' -f $Fps).Replace(',', '.')
    $si.Arguments = "-v error -y -f image2pipe -framerate $rate -c:v mjpeg -i - -c:v libx264 -preset veryfast -crf 30 " +
                    "-pix_fmt yuv420p -g $([int]($Fps * 10)) -f segment -segment_time 60 -reset_timestamps 1 `"$pattern`""
    $si.UseShellExecute = $false; $si.RedirectStandardInput = $true; $si.CreateNoWindow = $true
    [Diagnostics.Process]::Start($si)
}
$start = Get-Date; $gone = $null; $lastJpeg = [datetime]::MinValue
$frameMs = if ($Fps -gt 0) { 1000 / $Fps } else { $IntervalSec * 1000 }
$ms = New-Object IO.MemoryStream
$clock = [Diagnostics.Stopwatch]::StartNew()
# A stop request (deploy.ps1 before a new session): <shots>\sampler.stop - end cleanly, the encoder finishes its file
# (a killed ffmpeg loses its buffered last seconds: the frames that matter after a crash).
$stopFile = Join-Path (Split-Path $Dir -Parent) $(if ($Instance -gt 0) { "sampler$Instance.stop" } else { 'sampler.stop' })
while (((Get-Date) - $start).TotalMinutes -lt $MaxMinutes) {
    if ([IO.File]::Exists($stopFile)) { break }
    $p = Get-FLGameProcess | Where-Object MainWindowHandle -ne 0 | Select-Object -First 1
    if (-not $p) {
        if ($ff) { try { $ff.StandardInput.Close(); [void]$ff.WaitForExit(10000) } catch { }; $ff = $null }
        if (-not $gone) { $gone = Get-Date } elseif (((Get-Date) - $gone).TotalSeconds -gt 60) { break }
        Start-Sleep -Seconds 2; continue
    }
    $gone = $null
    $t0 = $clock.Elapsed.TotalMilliseconds
    $old = [IntPtr]::Zero
    try {
        # the client size in the window's own DPI context: a DPI-unaware game is drawn by PrintWindow at its real
        # (logical) size, which a per-monitor-aware caller would read scaled (a 2x frame, picture in the top-left quarter)
        $old = [FLShots]::SetThreadDpiAwarenessContext([FLShots]::GetWindowDpiAwarenessContext($p.MainWindowHandle))
        $r = New-Object FLShots+RECT
        [void][FLShots]::GetClientRect($p.MainWindowHandle, [ref]$r)
        $w = $r.R - $r.L; $h = $r.B - $r.T
        if ($w -gt 0 -and $h -gt 0) {
            $bmp = New-Object Drawing.Bitmap $w, $h
            $g = [Drawing.Graphics]::FromImage($bmp); $hdc = $g.GetHdc()
            [void][FLShots]::PrintWindow($p.MainWindowHandle, $hdc, 3) # client area + full content
            $g.ReleaseHdc($hdc); $g.Dispose()
            if ($old -ne [IntPtr]::Zero) { [void][FLShots]::SetThreadDpiAwarenessContext($old); $old = [IntPtr]::Zero }
            $sh = [int]($h * $Width / $w); $sh -= $sh % 2              # even height: H.264 4:2:0
            $small = New-Object Drawing.Bitmap $bmp, $Width, $sh
            $bmp.Dispose()
            $ms.SetLength(0); $small.Save($ms, $codec, $ep)
            if ($ffmpeg) {
                if (-not $ff -or $ff.HasExited) { $ff = Start-Encoder }
                try { $ms.WriteTo($ff.StandardInput.BaseStream) } catch { $ff = $null }
            }
            if (((Get-Date) - $lastJpeg).TotalSeconds -ge $IntervalSec) {
                $lastJpeg = Get-Date
                [IO.File]::WriteAllBytes((Join-Path $Dir ((Get-Date -Format 'yyyyMMdd_HHmmss') + '.jpg')), $ms.ToArray())
                Get-ChildItem $Dir -Filter '2*.jpg' | Sort-Object Name -Descending | Select-Object -Skip $Keep |
                    ForEach-Object { [IO.File]::Delete($_.FullName) }
            }
            $small.Dispose()
        }
    } catch { } finally { if ($old -ne [IntPtr]::Zero) { [void][FLShots]::SetThreadDpiAwarenessContext($old) } }
    $wait = $frameMs - ($clock.Elapsed.TotalMilliseconds - $t0)
    if ($wait -gt 1) { Start-Sleep -Milliseconds ([int]$wait) }
}
if ($ff) { try { $ff.StandardInput.Close(); [void]$ff.WaitForExit(10000) } catch { } }
