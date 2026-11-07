# Builds the player package: build\package\FLvibed-<version>.zip (the release FLvibed.dll, the shipped data, the install
# notes, CHANGELOG.md and the licences), laid out as the game folder expects (EXE\...), and
# prints its SHA-256. The version comes from core\version.h. Run in the PowerShell tool: tools\package.ps1 [-NoBuild]
param([switch]$NoBuild)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

$version = ([regex]::Match((Get-Content (Join-Path $root 'core\version.h') -Raw), '#define FLV_VERSION "([^"]+)"')).Groups[1].Value
if (-not $version) { throw 'FLV_VERSION not found in core\version.h' }
# the public repository has no export-ignored file: its docs must not point at one
$exportCheck = & (Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe') -I (Join-Path $PSScriptRoot 'docrefs.py') --export
if ($LASTEXITCODE) { $exportCheck | Write-Host; throw 'public docs point at export-ignored files (tools\docrefs.py --export)' }
if (-not $NoBuild) {
    & (Join-Path $PSScriptRoot 'buildlock.ps1') release
    if ($LASTEXITCODE) { throw "release build failed ($LASTEXITCODE)" }
}
$dll = Join-Path $root 'build\release\FLvibed.dll'
$dllVersion = (Get-Item $dll).VersionInfo.ProductVersion
if ($dllVersion -ne $version) { throw "build\release\FLvibed.dll is version '$dllVersion', core\version.h says '$version': rebuild" }

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
    ./flvibed-linux.sh install "<the game folder in your Wine or Proton prefix>"
It does what Setup does on Windows (uninstall: ./flvibed-linux.sh uninstall "<game folder>" [--keep-settings]). With a
DualSense plugged in by USB it also sets the pad's speaker to four channels (Surround 4.0), which the waveform haptics
need, as the Sound panel step does on Windows; again after a replug: ./flvibed-linux.sh haptics

UNINSTALL
Remove the FLvibed.dll line from EXE\dacom.ini, then delete EXE\FLvibed.dll, FLvibed.ini, FLController.ini,
FLSubtitles.ini, any *.before-0.2.0 backups of them, FLController.log, FLSubtitles.log, FLvibed_npc_stalls.log and the
folders EXE\FLSubtitles\ and EXE\FLvibed\. Saves stay valid.

NEEDS
Windows 10 or 11 (tested; Windows 7 SP1, 8 and 8.1 should work - checked; a tester's Windows 7 needed VxKex for 0.3.0) and your own copy
of the game. Controllers: XInput (Xbox) pads; other pads through Steam Input,
DS4Windows or similar. No other mod, loader or runtime is needed.

THIS IS A PREVIEW
Some features are incomplete and design choices may still change. What is new: CHANGELOG.md. Known limits, the
controls and every setting: the README on the project page and the living spec
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
Compress-Archive -Path $stage -DestinationPath $zip
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
