# Installs Freelancer: HD Edition 0.7.1 as FLvibed's test install with the official installer
# (Inno Setup), unattended, with the options the user chose (17 Oct 2026). Needs admin (UAC prompt).
#
#   install-hde.ps1 -Dest '<FLvibed>\Install\Freelancer HD Edition'
#
# Options = the installer's recommended (Express) set, except:
#   DisplayMode=2  Borderless Windowed (the installer default is Fullscreen; the user's primary mode)
#   GraphicsApi=1  DxWrapper + d3d8to9 (the installer's recommendation, what most players run)
#   JumpTunnel=0   vanilla 10 s jump tunnels (steadier mission timing for tests)
#   SkipIntros=0   keep the startup movies (FLSubtitles subtitles the intro)
# Parameter names: silent_options.iss of github.com/FLHDE/freelancer-hd-edition-installer.
#
# Kept separate from any personal Freelancer install (user, 18 Oct 2026): after installing, the test install's
# EXE\flplusplus.ini gets save_in_directory / screenshots_in_directory = true (saves, PerfOptions.ini and screenshots
# under <install>\SAVE and \SCREENSHOTS, tools\fldata.ps1); the installer's own side effects outside -Dest are undone:
# a Documents\My Games\Freelancer\PerfOptions.ini this install created ("Best video options") moves into <install>\SAVE
# (one that existed before is never touched), and the Windows uninstall entry (one AppId for every HDE install: a
# personal install would take it for an existing one) is exported to research\archive and removed (elevated).
# Remaining: Visual C++ redistributables may be installed (shared runtime, harmless).
param(
    [Parameter(Mandatory)] [string]$Dest,
    [string]$Vanilla = (Join-Path (Split-Path $PSScriptRoot -Parent) 'research\Vanilla\Freelancer'),
    [string]$Setup = (Join-Path (Split-Path $PSScriptRoot -Parent) 'research\Vanilla\FreelancerHDESetup_v0_7_1_1.exe'),
    [hashtable]$Options = @{}
)

# NewSaveFolder stays 0 (the installer's own choice of folder name); the data is moved into the install afterwards.
$opts = [ordered]@{ DisplayMode = 2; GraphicsApi = 1; JumpTunnel = 0; SkipIntros = 0; NewSaveFolder = 0 }
foreach ($k in $Options.Keys) { $opts[$k] = $Options[$k] }

if (Test-Path -LiteralPath (Join-Path $Dest 'EXE\Freelancer.exe')) { throw "already installed: $Dest" }
if (-not (Test-Path -LiteralPath (Join-Path $Vanilla 'EXE\Freelancer.exe'))) { throw "vanilla files not found: $Vanilla" }
New-Item -ItemType Directory -Force (Split-Path $Dest) | Out-Null
$log = Join-Path (Split-Path $Dest) 'hde-install.log'
# a PerfOptions.ini in the shared folder before installing is not ours (never moved afterwards)
$perfBefore = Test-Path -LiteralPath (Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Freelancer\PerfOptions.ini')

$argList = @('/SP-', '/SILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/DIR=`"$Dest`"", "/LOG=`"$log`"",
    "/VanillaFlDir=`"$Vanilla`"") + ($opts.Keys | ForEach-Object { "/$_=$($opts[$_])" })
"installer arguments: " + ($argList -join ' ')
$p = Start-Process -FilePath $Setup -ArgumentList $argList -Verb RunAs -PassThru -Wait
"installer exit code: $($p.ExitCode) (log: $log)"
if (-not (Test-Path -LiteralPath (Join-Path $Dest 'EXE\Freelancer.exe'))) { throw 'install failed: no EXE\Freelancer.exe' }

# ---- keep it separate from personal installs
$fpp = Join-Path $Dest 'EXE\flplusplus.ini'
$t = (Get-Content -LiteralPath $fpp) -replace '^(\s*save_in_directory\s*=).*$', '$1 true' -replace '^(\s*screenshots_in_directory\s*=).*$', '$1 true'
Set-Content -LiteralPath $fpp $t
"flplusplus: game data in $(Join-Path $Dest 'SAVE')"
$perf = Join-Path ([Environment]::GetFolderPath('MyDocuments')) 'My Games\Freelancer\PerfOptions.ini'
if ($perfBefore -eq $false -and (Test-Path -LiteralPath $perf)) {
    $save = Join-Path $Dest 'SAVE'; New-Item -ItemType Directory -Force $save | Out-Null
    Move-Item -LiteralPath $perf (Join-Path $save 'PerfOptions.ini') -Force
    foreach ($d in (Split-Path $perf), (Split-Path (Split-Path $perf))) {
        if ((Test-Path $d) -and -not (Get-ChildItem $d -Force)) { [IO.Directory]::Delete($d) }
    }
    "moved the installer's PerfOptions.ini into $save"
}
$key = 'HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\{F40FDCDA-3A45-4CC3-9FDA-167EE480A1E0}_is1'
if (Test-Path ('Registry::' + $key)) {
    $loc = (Get-ItemProperty ('Registry::' + $key)).InstallLocation
    if ($loc -and $loc.TrimEnd('\') -eq $Dest.TrimEnd('\')) { # only this install's entry
        $bak = Join-Path (Split-Path $PSScriptRoot -Parent) ("research\archive\hde-uninstall-entry-{0:yyyy-MM-dd-HHmm}.reg" -f (Get-Date))
        & reg.exe export $key $bak /y | Out-Null
        $r = Start-Process reg.exe -ArgumentList 'delete', "`"$key`"", '/f' -Verb RunAs -Wait -PassThru
        "uninstall entry removed (exit $($r.ExitCode), backup $bak)"
    }
}
