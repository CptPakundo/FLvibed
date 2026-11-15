# Front self-check of raw mouse input in the test copy (instance 4, already running in front with the test switch):
# program-sent relative mouse moves of n counts every 8 ms, with mouse_raw 0 and 1; the recorder beside it.
param([string]$OutDir = (Join-Path $PSScriptRoot 'out'))
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$fl = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$rec = Join-Path $PSScriptRoot 'mouse_gain_rec.ps1'
. "$fl\tools\harness\flctl.ps1"; $env:FLVIBED_INSTANCE = '4'
$ns = 'I' + [Guid]::NewGuid().ToString('N').Substring(0, 8)
Add-Type -TypeDefinition @"
using System; using System.Runtime.InteropServices; using System.Diagnostics; using System.Threading;
namespace $ns { public static class Inj {
  [DllImport("user32.dll")] static extern void mouse_event(uint f, int dx, int dy, uint d, UIntPtr e);
  // blocks of `block` events +n then `block` events -n, `blocks` times, one event every periodMs
  public static void Sweep(int n, int block, int blocks, double periodMs) {
    var sw = Stopwatch.StartNew(); int k = 0;
    for (int b = 0; b < blocks; ++b) for (int s = 0; s < 2; ++s) for (int i = 0; i < block; ++i) {
      double due = ++k * periodMs; while (sw.Elapsed.TotalMilliseconds < due) Thread.SpinWait(50);
      mouse_event(1, s == 0 ? n : -n, 0, 0, UIntPtr.Zero);
    }
  }
}}
"@
$inj = "$ns.Inj" -as [type]
foreach ($mode in 0, 1) {
    Send-FL "set mouse_raw $mode" | Out-Null
    Send-FL 'mouse 1920 1080' | Out-Null; Start-Sleep -Milliseconds 400; Send-FL 'mouse release' | Out-Null; Start-Sleep -Milliseconds 300
    $csv = Join-Path $OutDir "mouse_gain_7_selfcheck_raw$mode.csv"
    $p = Start-Process pwsh -WindowStyle Hidden -PassThru -ArgumentList @('-NoProfile', '-File', ('"' + $rec + '"'), '-PathLike', '"*\instances\4\*"', '-WaitSeconds', '10', '-MaxSeconds', '14', '-Out', ('"' + $csv + '"'))
    Start-Sleep -Seconds 4   # the recorder's compile and start
    foreach ($n in 2, 5, 10, 20, 40, 70) { $inj::Sweep($n, 8, 4, 8.0); Start-Sleep -Milliseconds 150 }
    $p.WaitForExit(20000) | Out-Null
    "mouse_raw $mode -> $csv"
    ((Send-FL 'state') -split "`n" | Select-String 'dimouse|foreground') | ForEach-Object { $_.Line.Substring(0, [Math]::Min(260, $_.Line.Length)) }
}
Send-FL 'set mouse_raw 1' | Out-Null
