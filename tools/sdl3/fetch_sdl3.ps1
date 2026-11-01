# SDL3's pinned source for build.cmd (WORK_QUEUE Q5: the opt-in gamepad backend; tools\sdl3\build_sdl3.cmd builds it):
# the official release zip of github.com/libsdl-org/SDL, tag release-<version>, its SHA-256 checked against the pin below,
# unpacked into research\repos\SDL3-<version>\SDL3-<version>\ (git-ignored, beside the zip). Nothing is installed.
# Present already: only checked (the version in SDL_version.h; the zip's hash when the zip is there). A new SDL version =
# a new pin here (the asset's SHA-256 as GitHub lists it) and SDL3_VER in build.cmd. SDL is zlib-licensed
# (LICENSES\SDL3-zlib.txt; README "Credits and licences").
# Usage (PowerShell): tools\sdl3\fetch_sdl3.ps1 [-Version 3.4.18]   exit 1 = missing / mismatch
param([string]$Version = '3.4.18')
$root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$pins = @{
    # 2 Oct 2026, the release asset SDL3-3.4.18.zip (17,094,462 B; first downloaded 29 Oct 2026 for the U29 probe)
    '3.4.18' = '9cd42377704398796071b8597cd7e21da254a43bad98c4199739647adc13fa6f'
}
if (-not $pins.ContainsKey($Version)) { "fetch_sdl3: no pin for SDL3 $Version (add its SHA-256 to `$pins)"; exit 1 }
$dir = Join-Path $root "research\repos\SDL3-$Version"
$zip = Join-Path $dir "SDL3-$Version.zip"
$src = Join-Path $dir "SDL3-$Version"
$ver = Join-Path $src 'include\SDL3\SDL_version.h'

function Test-Version {
    if (-not (Test-Path $ver)) { return $false }
    $t = Get-Content $ver -Raw
    $want = $Version -split '\.'
    foreach ($i in 0..2) {
        $name = @('MAJOR', 'MINOR', 'MICRO')[$i]
        if ($t -notmatch "#define SDL_${name}_VERSION\s+$($want[$i])\b") { return $false }
    }
    return $true
}
function Test-Zip {
    if (-not (Test-Path $zip)) { return $null }
    return (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower() -eq $pins[$Version]
}

if (Test-Path $src) {
    if (-not (Test-Version)) { "fetch_sdl3: $src is not SDL $Version (SDL_version.h) - move it away and run again"; exit 1 }
    $z = Test-Zip
    if ($z -eq $false) { "fetch_sdl3: $zip does not match the pinned SHA-256 - the unpacked source is not trusted"; exit 1 }
    "fetch_sdl3: SDL3 $Version source present ($src; zip $(if ($null -eq $z) { 'not kept' } else { 'SHA-256 OK' }))"
    exit 0
}

New-Item -ItemType Directory -Force $dir | Out-Null
if (-not (Test-Zip)) {
    $url = "https://github.com/libsdl-org/SDL/releases/download/release-$Version/SDL3-$Version.zip"
    "fetch_sdl3: downloading $url"
    $ProgressPreference = 'SilentlyContinue'
    Invoke-WebRequest -Uri $url -OutFile $zip -UseBasicParsing
    if (-not (Test-Zip)) {
        [System.IO.File]::Delete($zip)
        "fetch_sdl3: the download does not match the pinned SHA-256 - removed, nothing unpacked"
        exit 1
    }
}
Expand-Archive -Path $zip -DestinationPath $dir -Force
if (-not (Test-Version)) { "fetch_sdl3: the zip did not unpack to $src as SDL $Version"; exit 1 }
"fetch_sdl3: SDL3 $Version unpacked into $src ($((Get-Item $zip).Length) bytes zip, SHA-256 OK)"
exit 0
