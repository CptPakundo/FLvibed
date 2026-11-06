# [Modules] `safe = 1` and both switches at 0 install nothing (docs\MODULES_PLAN.md P2). A game started that way has no focus
# guard and no silence, so this needs no game: the release DLL is loaded into a 32-bit PowerShell from a folder of its own with
# each ini. Nothing started = no log file, no debug channel; a module started would open its log first thing.
# Usage (PowerShell tool): tests\modules_nothing.ps1    exit 1 on a failure. Run after a release build.
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$dll = Join-Path $root 'build\release\FLvibed.dll'
$ps32 = Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
$probe = @'
param([string]$dll)
Add-Type -Name N -Namespace W -MemberDefinition '[DllImport("kernel32", SetLastError = true, CharSet = CharSet.Unicode)] public static extern IntPtr LoadLibraryW(string path);'
$h = [W.N]::LoadLibraryW($dll)
if ($h -eq [IntPtr]::Zero) { "LOADFAIL $([Runtime.InteropServices.Marshal]::GetLastWin32Error())" } else { 'LOADED' }
'@
$failed = 0
$dir = Join-Path ([IO.Path]::GetTempPath()) "flvibed_modules_$PID"
New-Item -ItemType Directory -Force $dir | Out-Null
$script = Join-Path $dir 'probe.ps1'
[IO.File]::WriteAllText($script, $probe)
Copy-Item $dll $dir
foreach ($case in @(
        @{ name = 'safe = 1 over both on'; rows = "subtitles = 1`r`ncontroller = 1`r`nsafe = 1" },
        @{ name = 'both at 0'; rows = "subtitles = 0`r`ncontroller = 0" },
        @{ name = 'both at 0, a new row on'; rows = "subtitles = 0`r`ncontroller = 0`r`nhud = 1" },
        # the control: a module that is on opens its log, so an empty folder above means something (outside the game its hooks
        # are refused, or the probe dies: only the log counts here)
        @{ name = 'control, subtitles on'; rows = "subtitles = 1`r`ncontroller = 0"; started = $true })) {
    Get-ChildItem $dir -Filter '*.log' | ForEach-Object { [IO.File]::Delete($_.FullName) }
    [IO.File]::WriteAllText((Join-Path $dir 'FLvibed.ini'), "[Modules]`r`n$($case.rows)`r`n[FLvibed]`r`nsettings_version = 2`r`n")
    $ErrorActionPreference = 'Continue'
    $out = & $ps32 -NoProfile -ExecutionPolicy Bypass -File $script (Join-Path $dir 'FLvibed.dll') 2>&1
    $ErrorActionPreference = 'Stop'
    $logs = @(Get-ChildItem $dir -Filter '*.log' | ForEach-Object Name)
    $ok = if ($case.started) { $logs -contains 'FLSubtitles.log' } else { "$out" -match 'LOADED' -and $logs.Count -eq 0 }
    "{0,-4} {1}: {2}; logs: {3}" -f $(if ($ok) { 'ok' } else { 'FAIL' }), $case.name, "$out", ($logs -join ', ')
    if (-not $ok) { $failed = 1 }
}
[IO.Directory]::Delete($dir, $true)
exit $failed
