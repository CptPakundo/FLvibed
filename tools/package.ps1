# Builds the player package: build\package\FLvibed-<version>.zip (the release FLvibed.dll, the shipped data, a default
# FLvibed.ini, the install notes, CHANGELOG.md and the licences), laid out as the game folder expects (EXE\...), and
# prints its SHA-256. The version comes from core\version.h. Run in the PowerShell tool: tools\package.ps1 [-NoBuild]
param([switch]$NoBuild)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent

$version = ([regex]::Match((Get-Content (Join-Path $root 'core\version.h') -Raw), '#define FLV_VERSION "([^"]+)"')).Groups[1].Value
if (-not $version) { throw 'FLV_VERSION not found in core\version.h' }
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
foreach ($f in 'subtitles.tsv', 'movies.tsv', 'movie_frames.tsv', 'speaker_colors.tsv', 'word_times.tsv') {
    Copy-Item (Join-Path $root "subtitles\data\$f") (Join-Path $stage 'EXE\FLSubtitles')
}
Copy-Item (Join-Path $root 'controller\data\autopilot.3db') (Join-Path $stage 'EXE\FLvibed')
[IO.File]::WriteAllText((Join-Path $stage 'EXE\FLvibed.ini'), "[Modules]`r`nsubtitles = 1`r`ncontroller = 1`r`n")
Copy-Item (Join-Path $root 'CHANGELOG.md') $stage
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
   it merge: FLvibed.dll, FLvibed.ini, FLSubtitles\ and FLvibed\ land in EXE\.
2. Open EXE\dacom.ini in a text editor. In the [Libraries] section, add this line after the plugins already listed
   there (on the original game after .\flmaterials.dll):
       FLvibed.dll
3. Start the game. Settings: Options > FLVIBED (also the pause menu's sixth button).

UNINSTALL
Remove the FLvibed.dll line from EXE\dacom.ini, then delete EXE\FLvibed.dll, FLvibed.ini, FLController.ini,
FLSubtitles.ini, FLController.log, FLSubtitles.log and the folders EXE\FLSubtitles\ and EXE\FLvibed\. Saves stay valid.

NEEDS
Windows 10 or 11 and your own copy of the game. Controllers: XInput (Xbox) pads; other pads through Steam Input,
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
FLvibed's code: no licence has been chosen yet; until one is, no licence is granted for the code.
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
$setupHash = (Get-FileHash $setupExe -Algorithm SHA256).Hash.ToLower()
"$name-Setup.exe  $([math]::Round((Get-Item $setupExe).Length / 1MB, 2)) MB  sha256 $setupHash"
Get-ChildItem $stage -Recurse -File | ForEach-Object { $_.FullName.Substring($stage.Length + 1) }
