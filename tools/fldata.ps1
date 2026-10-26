# Where the test install keeps the game's user data (saves, PerfOptions.ini) - dot-source it.
# User, 18 Oct 2026: everything this project's game writes stays out of the places a personal Freelancer install uses
# (Documents\My Games\Freelancer, Pictures, the HDE uninstall entry) unless the user says otherwise. The test install's
# EXE\flplusplus.ini sets save_in_directory = true (HDE's flplusplus redirects Common.dll GetUserDataPath): the data
# lives in <install>\SAVE. The resolver reads that file, so the tools follow whatever it says.

$script:FLDataRoot = Split-Path $PSScriptRoot -Parent
# Parallel test games (20 Oct 2026, user: "faster and more efficient testing is an important goal"): game instance <n>
# runs from Install\instances\<n>\Freelancer HD Edition, a hard-linked mirror of the test install (no disk cost) with
# its own SAVE folder, logs and FLvibed files; $env:FLVIBED_INSTANCE selects it for the tools AND reaches the game
# (inherited at start): its debug channels are Local\FLControllerShm_<n> / Local\FLSubtitlesShm_<n>. Unset = the
# classic single install. Freelancer's single-instance mutex (WinMain 0x5B2F8E) has an empty name in HDE's exe.
function Get-FLInstallDir {
    if ($env:FLVIBED_INSTANCE) { Join-Path $script:FLDataRoot "Install\instances\$($env:FLVIBED_INSTANCE)\Freelancer HD Edition" }
    else { Join-Path $script:FLDataRoot 'Install\Freelancer HD Edition' }
}
$script:FLInstallDir = Get-FLInstallDir

# The game process of this instance (by its exe path), or $null.
function Get-FLGameProcess {
    $exe = Join-Path (Get-FLInstallDir) 'EXE\Freelancer.exe'
    Get-Process Freelancer -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path -ieq $exe } | Select-Object -First 1
}

# Before every background test start: FLvibed must load, because the background start's silence and inactive window are
# FLvibed's own (FLVIBED_BACKGROUND is read inside it). Without FLvibed.dll in EXE\ and its line in dacom.ini the game
# comes up in front with its sound - it did on 26 Oct 2026 (a test copy uninstalled by the setup, then only the DLL
# copied back: the owner heard and closed it). Throws instead of starting.
function Assert-FLWillLoad([string]$ExeDir) {
    $dll = Join-Path $ExeDir 'FLvibed.dll'
    $dacom = Join-Path $ExeDir 'dacom.ini'
    if (-not (Test-Path -LiteralPath $dll)) { throw "no FLvibed.dll in $ExeDir - a start now would be loud and in front: not started" }
    if (-not (Test-Path -LiteralPath $dacom) -or -not (Get-Content -LiteralPath $dacom | Where-Object { $_.Trim() -ieq 'FLvibed.dll' })) {
        throw "dacom.ini in $ExeDir does not list FLvibed.dll - a start now would be loud and in front: not started"
    }
}

# Creates (or completes) instance <n>'s mirror of the test install: every file hard-linked, except what a game or a
# deploy writes per instance - the SAVE folder (copied: own saves, PerfOptions), logs, FLvibed's own files in EXE
# (deploy copies them; a linked DLL loaded by another instance could not be replaced) and the .ini files deploy and
# the modules rewrite. Returns the instance's install folder.
function New-FLInstance([int]$N) {
    $src = Join-Path $script:FLDataRoot 'Install\Freelancer HD Edition'
    $dst = Join-Path $script:FLDataRoot "Install\instances\$N\Freelancer HD Edition"
    $own = '^(SAVE\\|EXE\\logs\\|EXE\\FL[^\\]*\.(dll|ini|log)$|EXE\\FLSubtitles\\|EXE\\dacom\.ini$|EXE\\[^\\]*\.log$)'
    foreach ($f in Get-ChildItem -LiteralPath $src -Recurse -File) {
        $rel = $f.FullName.Substring($src.Length + 1)
        $to = Join-Path $dst $rel
        if (Test-Path -LiteralPath $to) { continue }
        $null = New-Item -ItemType Directory -Force (Split-Path $to -Parent)
        if ($rel -match $own) { Copy-Item -LiteralPath $f.FullName $to }
        else { $null = New-Item -ItemType HardLink -Path $to -Target $f.FullName }
    }
    $dst
}

# Display mode and resolution of a game INSTANCE (never the main install: its options are the user's). How HD Edition
# sets them (its installer's Process_DisplayMode, read from github.com/FLHDE/freelancer-hd-edition-installer, 21 Oct 2026):
# the resolution is PerfOptions.ini [Display] size= W, H (per instance: SAVE is copied, not linked); the mode is three
# byte patches in Freelancer.exe - file 0x1B16CC = the immediate of `mov byte ptr [esp+79h], 0|1` at 0x5B16C8 (1 =
# fullscreen default, 0 = windowed), and the two high bytes of the window style pushed at 0x424777 and 0x42490A
# (`push 02CA0000h`: WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX; 0000 = borderless). The instance's
# Freelancer.exe is a hard link of the install's: the first mode change replaces it with a copy (the install keeps its
# bytes), and only an exe whose bytes at those offsets are the expected ones is patched (another build: refused).
# Fullscreen is exclusive fullscreen: it takes the screen - never for a background test while the user works.
function Set-FLInstanceDisplay([int]$N, [ValidateSet('borderless', 'windowed', 'fullscreen')][string]$Mode = 'borderless',
                               [string]$Size) {
    if ($N -lt 1) { throw 'Set-FLInstanceDisplay: instances only (n >= 1); the main install keeps the user''s options' }
    $inst = Join-Path $script:FLDataRoot "Install\instances\$N\Freelancer HD Edition"
    $exe = Join-Path $inst 'EXE\Freelancer.exe'
    if (-not (Test-Path -LiteralPath $exe)) { throw "no instance $N at $inst (deploy.ps1 -Instance $N makes it)" }
    $item = Get-Item -LiteralPath $exe
    if ($item.LinkType -eq 'HardLink' -and @($item.Target).Count -gt 0) {   # break the link: a private copy
        $tmp = "$exe.copy"
        Copy-Item -LiteralPath $exe $tmp
        [IO.File]::Delete($exe)
        Move-Item -LiteralPath $tmp $exe
    }
    # a fresh exe copy is held briefly by the real-time scanner ("being used by another process", 21 Oct 2026): retry
    $fs = $null
    for ($try = 0; -not $fs; $try++) {
        try { $fs = [IO.File]::Open($exe, 'Open', 'ReadWrite') }
        catch { if ($try -ge 40) { throw }; Start-Sleep -Milliseconds 250 }
    }
    try {
        $rd = { param($off, $n) $fs.Position = $off; $b = New-Object byte[] $n; [void]$fs.Read($b, 0, $n); ($b | ForEach-Object { $_.ToString('X2') }) -join '' }
        $wr = { param($off, [byte[]]$b) $fs.Position = $off; $fs.Write($b, 0, $b.Length) }
        # the sites must be what the HDE 0.7.1 exe has (either state): the opcode bytes before them, then the value
        if ((& $rd 0x1B16C8 4) -ne 'C6442479' -or (& $rd 0x1B16CC 1) -notin '00', '01') { throw "Freelancer.exe: unexpected bytes at 0x1B16C8 (not the HDE 0.7.1 build): mode not changed" }
        foreach ($o in 0x2477A, 0x2490D) {
            if ((& $rd ($o - 3) 3) -ne '680000' -or (& $rd $o 2) -notin '0000', 'CA02') { throw "Freelancer.exe: unexpected bytes at 0x$($o.ToString('X')) (not the HDE 0.7.1 build): mode not changed" }
        }
        $style = if ($Mode -eq 'windowed') { [byte[]](0xCA, 0x02) } else { [byte[]](0x00, 0x00) }
        & $wr 0x2477A $style; & $wr 0x2490D $style
        & $wr 0x1B16CC ([byte[]]@($(if ($Mode -eq 'fullscreen') { 1 } else { 0 })))
    } finally { $fs.Dispose() }
    if ($Size) {
        if ($Size -notmatch '^\s*(\d{3,4})\s*,\s*(\d{3,4})\s*$') { throw "Set-FLInstanceDisplay: -Size 'W, H' expected, got '$Size'" }
        $w = $Matches[1]; $h = $Matches[2]   # taken before the next -match overwrites $Matches (it wrote "size= ," once)
        $perf = Join-Path (Get-FLUserDataDir $inst) 'PerfOptions.ini'
        $text = Get-Content -LiteralPath $perf -Raw
        if ($text -notmatch '(?m)^size\s*=.*$') { throw "no [Display] size in $perf" }
        [IO.File]::WriteAllText($perf, [regex]::Replace($text, '(?m)^size\s*=.*$', "size= $w, $h"))
    }
    "instance ${N}: $Mode$(if ($Size) { ", $Size" })"
}

# The main install's display size (PerfOptions.ini [Display] size= W, H), the default for restoring an instance.
function Get-FLDisplaySize([string]$InstallDir = (Join-Path $script:FLDataRoot 'Install\Freelancer HD Edition')) {
    $perf = Join-Path (Get-FLUserDataDir $InstallDir) 'PerfOptions.ini'
    if ((Get-Content -LiteralPath $perf -Raw) -match '(?m)^size\s*=\s*(\d+)\s*,\s*(\d+)') { "$($Matches[1]), $($Matches[2])" } else { $null }
}

# The game's user-data folder for an install: <install>\SAVE when flplusplus saves in the game directory, else
# Documents\My Games\<save_folder_name> (the HDE default, shared with any other Freelancer).
function Get-FLUserDataDir([string]$InstallDir = (Get-FLInstallDir)) {
    $ini = Join-Path $InstallDir 'EXE\flplusplus.ini'
    $inDir = $false; $name = 'Freelancer'
    if (Test-Path -LiteralPath $ini) {
        foreach ($l in (Get-Content -LiteralPath $ini)) {
            if ($l -match '^\s*save_in_directory\s*=\s*(\S+)') { $inDir = $Matches[1] -match '^(true|1|yes)$' }
            elseif ($l -match '^\s*save_folder_name\s*=\s*(.+?)\s*$') { $name = $Matches[1] }
        }
    }
    if ($inDir) { return (Join-Path $InstallDir 'SAVE') }
    Join-Path ([Environment]::GetFolderPath('MyDocuments')) "My Games\$name"
}

# The single-player save folder (Accts\SinglePlayer under the user-data folder).
function Get-FLSaveDir([string]$InstallDir = (Get-FLInstallDir)) { Join-Path (Get-FLUserDataDir $InstallDir) 'Accts\SinglePlayer' }

# Throws when the test install would write to the shared default folder (the separation is off).
function Assert-FLDataSeparate([string]$InstallDir = (Get-FLInstallDir)) {
    $d = Get-FLUserDataDir $InstallDir
    if (-not $d.StartsWith($InstallDir, [StringComparison]::OrdinalIgnoreCase)) {
        throw "the test install's game data would go to $d (shared with personal installs): set save_in_directory = true in $InstallDir\EXE\flplusplus.ini"
    }
}
