# Builds the player package: build\package\FLvibed-<version>.zip (the release FLvibed.dll, the shipped data, the install
# notes, CHANGELOG.md and the licences), laid out as the game folder expects (EXE\...), and
# prints its SHA-256. The version comes from core\version.h. Run in the PowerShell tool: tools\package.ps1 [-NoBuild] [-CheckSize]
# Before it packs anything (audit 12 Nov 2026, report 12_): the host-free unit tests (buildlock test, also with -NoBuild)
# and the release DLL's size against its row in docs\ratchets.tsv (release_dll_bytes, value x (1 + max_growth)). Either
# failing stops the package. Over the size limit: shrink the DLL or raise the row in a commit that says why - never
# silently. -CheckSize runs the size check alone (no build, no package) and exits 0 / 1.
param([switch]$NoBuild, [switch]$CheckSize)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

function Test-DllSize([string]$dll) {
    $row = @(Import-Csv (Join-Path $root 'docs\ratchets.tsv') -Delimiter "`t") | Where-Object { $_.name -eq 'release_dll_bytes' } | Select-Object -First 1
    if (-not $row) { throw 'docs\ratchets.tsv has no release_dll_bytes row: the size check cannot run' }
    $size = (Get-Item $dll).Length
    $limit = [math]::Floor([double]$row.value * (1 + [double]$row.max_growth))
    $msg = '{0}: {1:N0} bytes; recorded {2:N0} (docs\ratchets.tsv, {3}), limit {4:N0} (+{5:P0}); {6:+0.0;-0.0} % over the record' -f
        $dll, $size, [double]$row.value, $row.date, $limit, [double]$row.max_growth, (100.0 * ($size - [double]$row.value) / [double]$row.value)
    [pscustomobject]@{ Ok = ($size -le $limit); Text = $msg }
}
if ($CheckSize) {
    $s = Test-DllSize (Join-Path $root 'build\release\FLvibed.dll')
    $(if ($s.Ok) { 'size OK: ' } else { 'size OVER THE LIMIT: ' }) + $s.Text
    exit $(if ($s.Ok) { 0 } else { 1 })
}

$version = ([regex]::Match((Get-Content (Join-Path $root 'core\version.h') -Raw), '#define FLV_VERSION "([^"]+)"')).Groups[1].Value
if (-not $version) { throw 'FLV_VERSION not found in core\version.h' }
# the public repository has no export-ignored file: its docs must not point at one
$exportCheck = & (Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe') -I (Join-Path $PSScriptRoot 'docrefs.py') --export
if ($LASTEXITCODE) { $exportCheck | Write-Host; throw 'public docs point at export-ignored files (tools\docrefs.py --export)' }
# nothing of the design bench's private material in the export: its private folders, recordings, atlases, the marker
$privateCheck = & (Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe') -I (Join-Path $PSScriptRoot 'private_check.py') --head-only
if ($LASTEXITCODE) { $privateCheck | Write-Host; throw 'the export would publish private bench files (tools\private_check.py)' }
if (-not $NoBuild) {
    & (Join-Path $PSScriptRoot 'buildlock.ps1') release
    if ($LASTEXITCODE) { throw "release build failed ($LASTEXITCODE)" }
}
$dll = Join-Path $root 'build\release\FLvibed.dll'
$dllVersion = (Get-Item $dll).VersionInfo.ProductVersion
if ($dllVersion -ne $version) { throw "build\release\FLvibed.dll is version '$dllVersion', core\version.h says '$version': rebuild" }
$sz = Test-DllSize $dll
if (-not $sz.Ok) { throw "the release DLL is over its size limit: $($sz.Text). Shrink it, or raise docs\ratchets.tsv's row in a commit that says why" }
"size: $($sz.Text)"
# the unit tests (build\test\fltest.exe, built and run by build.cmd test; its exit code is the tests'; the data tests
# are skipped, not failed, when no test install is found)
& (Join-Path $PSScriptRoot 'buildlock.ps1') test
if ($LASTEXITCODE) { throw "the unit tests failed (buildlock test exit $LASTEXITCODE): no package" }

$name = "FLvibed-$version"
$out = Join-Path $root 'build\package'
$stage = Join-Path $out $name
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Force (Join-Path $stage 'EXE\FLSubtitles'), (Join-Path $stage 'EXE\FLvibed'), (Join-Path $stage 'LICENSES') | Out-Null

Copy-Item $dll (Join-Path $stage 'EXE')
foreach ($f in 'subtitles.tsv', 'movies.tsv', 'mixes.tsv', 'movie_frames.tsv', 'speaker_colors.tsv', 'word_times.tsv') {
    Copy-Item (Join-Path $root "modules\subtitles\data\$f") (Join-Path $stage 'EXE\FLSubtitles')
}
Copy-Item (Join-Path $root 'modules\autopilot\data\autopilot.3db') (Join-Path $stage 'EXE\FLvibed')
# No FLvibed.ini in the zip: merged by hand it would replace the player's, so its [Modules] rows and its settings stamp
# (core\settings_reset.h) would go, and the next start would reset every setting. The DLL writes the file at its first start.
Copy-Item (Join-Path $root 'CHANGELOG.md') $stage
# Linux (Wine / Proton): install, uninstall and the DualSense's Surround 4.0 profile (the owner, 7 Nov 2026: feature parity). LF line ends, whatever the checkout has
$sh = [IO.File]::ReadAllText((Join-Path $root 'tools\setup\linux\flvibed-linux.sh')) -replace "`r`n", "`n"
[IO.File]::WriteAllText((Join-Path $stage 'flvibed-linux.sh'), $sh, [Text.UTF8Encoding]::new($false))
Copy-Item (Join-Path $root 'LICENSES\*') (Join-Path $stage 'LICENSES')

$readme = @"
FLvibed $version - controller, subtitles and movies for Freelancer
=====================================================================

Native Xbox controller play with a controller-first interface, dialogue subtitles with speaker names, better movies
and the Advanced Autopilot (work in progress) for Freelancer: HD Edition 0.7.1 or the original Freelancer 1.1
(decrypted Freelancer.exe). One DLL; no game file is changed.

INSTALL
The easy way: run FLvibed-$version-Setup.exe from the same release page - it finds the game, installs, updates and
uninstalls (Windows Settings > Apps > FLvibed). By hand, with this zip:
1. Copy the EXE folder from this package into your Freelancer folder (the one that holds EXE\Freelancer.exe) and let
   it merge: FLvibed.dll, FLSubtitles\ and FLvibed\ land in EXE\. An update this way keeps your settings.
2. Open EXE\dacom.ini in a text editor. In the [Libraries] section, add this line after the plugins already listed
   there (on the original game after .\flmaterials.dll):
       FLvibed.dll
3. Start the game. Settings: Options > FLVIBED (also the pause menu's sixth button).

LINUX (Wine, Proton)
Unpack this zip and run, in a terminal, from its folder:
    bash flvibed-linux.sh install "<the game folder in your Wine or Proton prefix>"
It does what Setup does on Windows (uninstall: bash flvibed-linux.sh uninstall "<game folder>" [--keep-settings]). With a
DualSense plugged in by USB it also sets the pad's speaker to four channels (Surround 4.0), which the waveform haptics
need, as the Sound panel step does on Windows; again after a replug: bash flvibed-linux.sh haptics

UNINSTALL
Remove the FLvibed.dll line from EXE\dacom.ini, then delete EXE\FLvibed.dll, FLvibed.ini, FLController.ini,
FLSubtitles.ini, any *.before-0.2.0 backups of them, FLController.log, FLSubtitles.log, FLvibed_npc_stalls.log, FLvibed_crash.log and the
folders EXE\FLSubtitles\ and EXE\FLvibed\. Saves stay valid.

NEEDS
Windows 10 or 11 (tested; Windows 7 SP1, 8 and 8.1 should work - checked; a tester's Windows 7 needed VxKex for 0.3.0) and your own copy
of the game. Controllers: Xbox pads through XInput; PlayStation and Switch pads are read natively (an option hands
them to Steam Input or a mapper instead); a Steam Deck stays on Steam Input. Keyboard and mouse work too. No other
mod, loader or runtime is needed.

THIS IS A PREVIEW
Some features are incomplete and design choices may still change. What is new: CHANGELOG.md. Known limits, the
controls and every setting: the README on the project page and the Feature Tour
(https://claude.ai/artifact/4v4dXbB5GTVt1URowFtSvN).

LICENCES AND CREDITS
Freelancer is (c) Microsoft. FLvibed uses game content under the Microsoft Game Content Usage Rules
(https://www.xbox.com/en-US/developers/rules); it is free, non-commercial and not endorsed by or affiliated with
Microsoft. This package contains no game files and no audio.
The subtitle data (EXE\FLSubtitles\*.tsv) is licensed GPL-3.0 (LICENSES\GPL-3.0.txt). It uses the work of FLSubs by
Lorash, Freelancer Subtitles by L@Zar0 and the Russian edition's editor (GPL-3.0), and the wedge009.net mission
transcripts (CC BY-SA 4.0) and the transcript sites they cite. Thanks to everyone who contributed.
FLvibed's code is licensed GPL-3.0 (LICENSES\GPL-3.0.txt), like the subtitle data.
Made with AI ("vibe coded"): written by an AI (Claude, by Anthropic) under the owner's direction and tested in game.
No warranty.
"@
[IO.File]::WriteAllText((Join-Path $stage 'README.txt'), ($readme -replace "`r?`n", "`r`n"))

$zip = Join-Path $out "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
# (not Compress-Archive: it stores no Unix mode bits, so the shipped flvibed-linux.sh was not executable after an unzip
# on Linux or a Steam Deck - the owner's public set, 12 Nov 2026. tools\zip_package.py writes the same layout with Unix
# modes, *.sh 0755, and checks them back from the written zip)
$zipCheck = & (Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe') -I (Join-Path $PSScriptRoot 'zip_package.py') $stage $zip
if ($LASTEXITCODE) { $zipCheck | Write-Host; throw 'the zip was not written with an executable flvibed-linux.sh (tools\zip_package.py)' }
$zipCheck
$hash = (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower()
"$name.zip  $([math]::Round((Get-Item $zip).Length / 1MB, 2)) MB  sha256 $hash"

# the one-click installer / uninstaller (tools\setup), built from the same release DLL and data, beside the zip
& (Join-Path $PSScriptRoot 'buildlock.ps1') setup
if ($LASTEXITCODE) { throw "setup build failed ($LASTEXITCODE)" }
$setupExe = Join-Path $out "$name-Setup.exe"
Copy-Item (Join-Path $root 'build\setup\FLvibed-Setup.exe') $setupExe -Force
if ((Get-Item $setupExe).VersionInfo.ProductVersion -ne $version) { throw "the setup is not version $version" }
# Windows 7: no import the system lacks (an import of a newer function keeps the DLL from loading at all; 0.1.0's report)
& (Join-Path $PSScriptRoot 'win7_check.ps1') -Files $dll, $setupExe
if ($LASTEXITCODE) { throw 'win7_check: an import not verified for Windows 7 (above)' }
$setupHash = (Get-FileHash $setupExe -Algorithm SHA256).Hash.ToLower()
"$name-Setup.exe  $([math]::Round((Get-Item $setupExe).Length / 1MB, 2)) MB  sha256 $setupHash"
Get-ChildItem $stage -Recurse -File | ForEach-Object { $_.FullName.Substring($stage.Length + 1) }
