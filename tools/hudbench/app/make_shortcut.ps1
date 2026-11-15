# Makes (or remakes) the shortcut `HUD 2 Bench.lnk` that starts the design bench with a double-click (bench_app.ps1):
# by default in the folder that holds this FLvibed checkout (the environment folder), or in -Folder. Re-run it after
# moving the checkout. The shortcut runs PowerShell 7 inside a console host with no window (conhost --headless), so no
# console is ever shown; its icon is the default browser's (the window opens there: browser.ps1). The shortcut itself is not tracked.
#   tools\hudbench\app\make_shortcut.ps1 [-Folder <folder>]
param([string]$Folder = '')
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) -Parent
if (-not $Folder) { $Folder = Split-Path $root -Parent }
$pwsh = (Get-Command pwsh -ErrorAction SilentlyContinue).Source
if (-not $pwsh) { Write-Output 'make_shortcut: PowerShell 7 (pwsh) is needed (the build lock uses it)'; exit 1 }
$conhost = Join-Path $env:SystemRoot 'System32\conhost.exe'
$launcher = Join-Path $PSScriptRoot 'bench_app.ps1'
. (Join-Path $PSScriptRoot 'browser.ps1')
$browser = Find-BenchBrowser
$lnk = Join-Path $Folder 'HUD 2 Bench.lnk'
$shell = New-Object -ComObject WScript.Shell
$s = $shell.CreateShortcut($lnk)
$s.TargetPath = $conhost
$s.Arguments = "--headless `"$pwsh`" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File `"$launcher`""
$s.WorkingDirectory = $root
$s.Description = 'HUD 2 Bench: the design bench of FLvibed (closes with its window)'
if ($browser) { $s.IconLocation = "$($browser.Exe),0" }
$s.WindowStyle = 7 # (minimized: conhost --headless shows nothing; this is a second guard)
$s.Save()
Write-Output "make_shortcut: $lnk -> $launcher"
