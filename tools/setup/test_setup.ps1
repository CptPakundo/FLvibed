# Tests FLvibed Setup (build\setup\FLvibed-Setup.exe) on throwaway game folders: copies of the test installs' real
# dacom.ini files (HD Edition and vanilla, FLvibed's line taken out first) beside a stand-in Freelancer.exe. Runs the
# setup's command line (/quiet): install, update, uninstall keeping / removing the settings, the uninstaller removing
# itself, a running game, a wrong folder, a dacom.ini without [Libraries]. Never touches a real install; the per-user
# "installed apps" entries it creates are removed by the uninstalls it runs (and checked). PowerShell tool:
#   tools\setup\test_setup.ps1 [-Work <folder>]   (default: build\setup\test)
param([string]$Work)
$ErrorActionPreference = 'Stop'
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$setup = Join-Path $root 'build\setup\FLvibed-Setup.exe'
if (-not (Test-Path $setup)) { throw 'build\setup\FLvibed-Setup.exe missing: build.cmd release, then build.cmd setup' }
if (-not $Work) { $Work = Join-Path $root 'build\setup\test' }
if (Test-Path $Work) { Remove-Item $Work -Recurse -Force }
New-Item -ItemType Directory -Force $Work | Out-Null

$script:fails = 0
function Check([bool]$ok, [string]$what) {
    if ($ok) { "  ok   $what" } else { "  FAIL $what"; $script:fails++ }
}
function Run([string[]]$argv) {
    $p = Start-Process $setup -ArgumentList $argv -PassThru -WindowStyle Hidden; $p.WaitForExit()
    return $p.ExitCode
}
function EntryKeys { @(Get-ChildItem 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall' -ErrorAction SilentlyContinue | Where-Object { $_.PSChildName -like 'FLvibed-*' }) }
function NewGame([string]$name, [string]$dacomFrom) {
    $g = Join-Path $Work $name
    New-Item -ItemType Directory -Force (Join-Path $g 'EXE') | Out-Null
    [IO.File]::WriteAllBytes((Join-Path $g 'EXE\Freelancer.exe'), [byte[]](0x4D, 0x5A))
    $lines = [IO.File]::ReadAllText($dacomFrom) -split "(?<=`n)" | Where-Object { $_ -notmatch '^\s*FLvibed\.dll\s*\r?\n?$' }
    [IO.File]::WriteAllText((Join-Path $g 'EXE\dacom.ini'), ($lines -join ''))
    return $g
}
$payload = 'FLvibed.dll', 'FLvibed.ini', 'FLSubtitles\subtitles.tsv', 'FLSubtitles\movies.tsv', 'FLSubtitles\movie_frames.tsv',
    'FLSubtitles\speaker_colors.tsv', 'FLSubtitles\word_times.tsv', 'FLvibed\autopilot.3db', 'FLvibed\Uninstall FLvibed.exe'
$entriesBefore = (EntryKeys).Count

$sources = @{
    'hde' = Join-Path $root 'Install\Freelancer HD Edition\EXE\dacom.ini'
    'vanilla' = Join-Path $root 'Install\instances\vanilla11\Freelancer\EXE\dacom.ini'
}
foreach ($kind in 'hde', 'vanilla') {
    if (-not (Test-Path $sources[$kind])) { "skip $kind (no $($sources[$kind]))"; continue }
    "== $kind"
    $g = NewGame "game_$kind" $sources[$kind]
    $exe = Join-Path $g 'EXE'
    $orig = [IO.File]::ReadAllBytes("$exe\dacom.ini")

    Check ((Run '/install', "`"$g`"", '/quiet', '/log', "`"$Work\$kind-install.log`"") -eq 0) 'install exits 0'
    foreach ($f in $payload) { Check (Test-Path "$exe\$f") "installed $f" }
    $dll = Get-Item "$exe\FLvibed.dll"
    Check ($dll.VersionInfo.ProductVersion -eq (Get-Item (Join-Path $root 'build\release\FLvibed.dll')).VersionInfo.ProductVersion) 'FLvibed.dll is the release build''s version'
    Check ((Get-FileHash "$exe\FLvibed.dll").Hash -eq (Get-FileHash (Join-Path $root 'build\release\FLvibed.dll')).Hash) 'FLvibed.dll is byte-identical to the release build'
    $d = [IO.File]::ReadAllLines("$exe\dacom.ini")
    Check (@($d | Where-Object { $_.Trim() -ieq 'FLvibed.dll' }).Count -eq 1) 'dacom.ini lists FLvibed.dll once'
    $lib = [Array]::FindIndex($d, [Predicate[string]]{ param($l) $l.Trim() -ieq '[Libraries]' })
    $ours = [Array]::FindIndex($d, [Predicate[string]]{ param($l) $l.Trim() -ieq 'FLvibed.dll' })
    $next = $lib + 1; while ($next -lt $d.Count -and -not $d[$next].Trim().StartsWith('[')) { $next++ }
    Check ($ours -gt $lib -and $ours -lt $next) 'the line is inside [Libraries]'
    $after = @($d[($ours + 1)..($next - 1)] | Where-Object { $_.Trim() -ne '' -and -not $_.Trim().StartsWith(';') })
    Check ($after.Count -eq 0) 'the line comes after the plugins already listed'
    Check ((Get-Item "$exe\dacom.ini").Length -eq $orig.Length + 'FLvibed.dll'.Length + 2) 'dacom.ini grew by exactly one line'
    Check ([IO.File]::ReadAllBytes("$exe\FLvibed\dacom.ini.before-flvibed").Length -eq $orig.Length) 'dacom.ini backed up'
    Check ((EntryKeys).Count -eq $entriesBefore + 1) 'installed-apps entry created'

    # update: the player's settings stay, the line is not doubled
    Add-Content "$exe\FLvibed.ini" '; my setting'
    'x' | Set-Content "$exe\FLController.ini"
    Check ((Run '/install', "`"$g`"", '/quiet') -eq 0) 'update exits 0'
    Check ((Get-Content "$exe\FLvibed.ini" -Raw) -match 'my setting') 'update keeps FLvibed.ini'
    Check (@([IO.File]::ReadAllLines("$exe\dacom.ini") | Where-Object { $_.Trim() -ieq 'FLvibed.dll' }).Count -eq 1) 'update does not double the line'
    Check ((EntryKeys).Count -eq $entriesBefore + 1) 'update keeps one entry'

    # uninstall, keeping the settings
    'saves' | Set-Content "$exe\FLvibed\saves.ini"
    'log' | Set-Content "$exe\FLController.log"
    Check ((Run '/uninstall', "`"$g`"", '/keepsettings', '/quiet', '/log', "`"$Work\$kind-uninstall.log`"") -eq 0) 'uninstall (keep settings) exits 0'
    Check ([Linq.Enumerable]::SequenceEqual([IO.File]::ReadAllBytes("$exe\dacom.ini"), $orig)) 'dacom.ini byte-identical to before'
    foreach ($f in 'FLvibed.dll', 'FLSubtitles', 'FLvibed\autopilot.3db', 'FLvibed\Uninstall FLvibed.exe', 'FLController.log') { Check (-not (Test-Path "$exe\$f")) "removed $f" }
    foreach ($f in 'FLvibed.ini', 'FLController.ini', 'FLvibed\saves.ini') { Check (Test-Path "$exe\$f") "kept $f" }
    Check ((EntryKeys).Count -eq $entriesBefore) 'installed-apps entry removed'

    # install again, then uninstall from the installed uninstaller (removes itself), settings removed
    Check ((Run '/install', "`"$g`"", '/quiet') -eq 0) 'reinstall exits 0'
    $p = Start-Process "$exe\FLvibed\Uninstall FLvibed.exe" -ArgumentList '/uninstall', "`"$g`"", '/quiet' -PassThru -WindowStyle Hidden; $p.WaitForExit()
    Check ($p.ExitCode -eq 0) 'the installed uninstaller exits 0'
    $deadline = (Get-Date).AddSeconds(15)
    while ((Test-Path "$exe\FLvibed") -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 250 }
    Check (-not (Test-Path "$exe\FLvibed")) 'the uninstaller and its folder are gone'
    $left = @(Get-ChildItem $exe -Recurse -File | ForEach-Object { $_.FullName.Substring($exe.Length + 1) } | Sort-Object)
    Check (($left -join ',') -eq 'dacom.ini,Freelancer.exe') "only the game's files remain ($($left -join ', '))"
    Check ([Linq.Enumerable]::SequenceEqual([IO.File]::ReadAllBytes("$exe\dacom.ini"), $orig)) 'dacom.ini byte-identical to before (again)'
    Check ((EntryKeys).Count -eq $entriesBefore) 'no entry left'
}

"== edge cases"
$g = NewGame 'game_running' $sources['hde']
Copy-Item "$env:SystemRoot\System32\timeout.exe" "$g\EXE\Freelancer.exe" -Force
$run = Start-Process "$g\EXE\Freelancer.exe" -ArgumentList '/t', '30', '/nobreak' -PassThru -WindowStyle Hidden
Start-Sleep -Milliseconds 500
Check ((Run '/install', "`"$g`"", '/quiet') -eq 3) 'a running game refuses the install (exit 3)'
Check (-not (Test-Path "$g\EXE\FLvibed.dll")) 'nothing installed while the game runs'
Stop-Process -Id $run.Id -Force -ErrorAction SilentlyContinue
Check ((Run '/install', "`"$Work`"", '/quiet') -eq 4) 'a folder without the game is refused (exit 4)'
# one folder, two paths (player install test 26 Oct 2026): installed by the real path, a game started through a
# junction, uninstalled through the junction - the running game is seen, the installed-apps entry goes
$g = NewGame 'game_twopaths' $sources['hde']
$j = Join-Path $Work 'game_twopaths_link'
New-Item -ItemType Junction -Path $j -Target $g | Out-Null
Check ((Run '/install', "`"$g`"", '/quiet') -eq 0) 'install by the real path'
Check ((EntryKeys).Count -eq $entriesBefore + 1) 'one entry'
Copy-Item "$env:SystemRoot\System32\timeout.exe" "$g\EXE\Freelancer.exe" -Force
$run = Start-Process "$j\EXE\Freelancer.exe" -ArgumentList '/t', '30', '/nobreak' -PassThru -WindowStyle Hidden
Start-Sleep -Milliseconds 500
Check ((Run '/uninstall', "`"$g`"", '/quiet') -eq 3) 'a game started through the junction is seen by the real path (exit 3)'
Stop-Process -Id $run.Id -Force -ErrorAction SilentlyContinue; Start-Sleep -Milliseconds 300
Check ((Run '/uninstall', "`"$j`"", '/quiet') -eq 0) 'uninstall through the junction'
Check ((EntryKeys).Count -eq $entriesBefore) 'the entry is gone though installed by the other path'
Check (-not (Test-Path "$g\EXE\FLvibed.dll")) 'the files are gone'
[IO.Directory]::Delete($j)   # the junction only, not its target

# a DLL still locked for a moment after the game closed: the uninstall waits for it (another process holds the file)
$g = NewGame 'game_locked' $sources['hde']
Check ((Run '/install', "`"$g`"", '/quiet') -eq 0) 'install (lock test)'
$dllPath = "$g\EXE\FLvibed.dll"
$holder = Start-Process powershell -ArgumentList '-NoProfile', '-Command', "`$f = [IO.File]::Open('$dllPath', 'Open', 'Read', 'Read'); Start-Sleep -Seconds 2; `$f.Close()" -PassThru -WindowStyle Hidden
Start-Sleep -Milliseconds 700
Check ((Run '/uninstall', "`"$g`"", '/quiet') -eq 0) 'uninstall while the DLL is held for 2 s more (waits, exit 0)'
Check (-not (Test-Path $dllPath)) 'FLvibed.dll deleted once released'
$holder.WaitForExit()

# Program Files: the game's settings and logs land in the player's VirtualStore copy of EXE\ (disc install test,
# 26 Oct 2026); the uninstall cleans them there too (settings kept on request)
foreach ($keep in $false, $true) {
    $g = NewGame "game_vstore_$keep" $sources['hde']
    Check ((Run '/install', "`"$g`"", '/quiet') -eq 0) "install (VirtualStore, keep settings $keep)"
    $vs = Join-Path $env:LOCALAPPDATA ('VirtualStore\' + (Join-Path (Resolve-Path $g).Path 'EXE').Substring(3))
    New-Item -ItemType Directory -Force (Join-Path $vs 'FLvibed') | Out-Null
    foreach ($f in 'FLController.ini', 'FLSubtitles.ini', 'FLController.log', 'FLSubtitles.log', 'FLvibed\saves.ini') { 'x' | Set-Content (Join-Path $vs $f) }
    $a = @('/uninstall', "`"$g`"", '/quiet'); if ($keep) { $a += '/keepsettings' }
    Check ((Run $a) -eq 0) "uninstall (VirtualStore, keep settings $keep)"
    Check (-not (Test-Path (Join-Path $vs 'FLController.log')) -and -not (Test-Path (Join-Path $vs 'FLSubtitles.log'))) 'VirtualStore logs removed'
    $kept = (Test-Path (Join-Path $vs 'FLController.ini')) -and (Test-Path (Join-Path $vs 'FLvibed\saves.ini'))
    if ($keep) { Check $kept 'VirtualStore settings kept on request' } else { Check (-not $kept -and -not (Test-Path (Join-Path $vs 'FLSubtitles.ini'))) 'VirtualStore settings removed' }
    $root = Join-Path $env:LOCALAPPDATA ('VirtualStore\' + (Resolve-Path $Work).Path.Substring(3))
    if (Test-Path $root) { [IO.Directory]::Delete($root, $true) }   # the test's own VirtualStore files only
}

$g = NewGame 'game_nolib' $sources['hde']
Set-Content "$g\EXE\dacom.ini" "[DACOM]`r`nIgnoreDACOMEnv = true`r`n"
Check ((Run '/install', "`"$g`"", '/quiet') -eq 1) 'a dacom.ini without [Libraries] fails (exit 1)'
Check ((Run '/uninstall', "`"$g`"", '/quiet') -eq 0) 'and uninstalls cleanly after that'
Check ((EntryKeys).Count -eq $entriesBefore) 'no entry left at the end'

if ($script:fails) { "$($script:fails) FAILED"; exit 1 } else { 'all passed' }
