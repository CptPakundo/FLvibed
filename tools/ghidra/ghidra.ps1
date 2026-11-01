# Ghidra for FLvibed, headless (tools\ghidra\README.md): the project research\ghidra\project\FLvibed.gpr holds
# read-only copies of the test install's Freelancer.exe and game DLLs (research\ghidra\bin_hde071), analysed, the
# exe seeded with core\flexe.h's symbols (FLvibedSeed.java). Everything Ghidra is in research\ghidra\ (git-ignored).
#   -Import [-Files a,b]  copy the binaries from the test install (read-only), import + analyse what is not in the
#                         project yet (all, or -Files), seed Freelancer.exe. Stops the MCP server first (one owner of
#                         the project at a time) and leaves it stopped.
#   -Seed                 re-run the flexe.h seed and FREELANCER_RE.md's notes (FLvibedNotes.java) on Freelancer.exe
#                         (after new symbols / doc facts); stops the server first.
#   -Vtables [-Files]     create the functions only vtables point at (FLvibedVtables.java; analysis misses them), all
#                         imported programs or -Files; -Import runs it on what it imported. Stops the server first.
#   -Serve                start the headless MCP server (loopback 8089; mcp_launch.js --server-only)
#   -Stop                 stop it
#   -Status               what is installed, imported and running
#   -Check                the project's seed is today's core\flexe.h (drift_check's `ghidra` line); exit 1 when stale
# Usage (PowerShell): tools\ghidra\ghidra.ps1 -Import | -Seed | -Serve | -Stop | -Status
param([switch]$Import, [string[]]$Files = @(), [switch]$Seed, [switch]$Vtables, [switch]$Serve, [switch]$Stop, [switch]$Status,
      [switch]$Check)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$g = Join-Path $root 'research\ghidra'
$bin = Join-Path $g 'bin_hde071'
$logs = Join-Path $g 'logs'
$exeDir = Join-Path $root 'Install\Freelancer HD Edition\EXE'
$dllDir = Join-Path $root 'Install\Freelancer HD Edition\DLLS\bin'
$games = @('Freelancer.exe', 'Common.dll', 'Content.dll', 'Server.dll', 'DACOM.dll', 'DALib.dll', 'System.dll',
    'rendcomp.dll', 'RemoteClient.dll', 'RemoteServer.dll', 'EngBase.dll', 'SoundManager.dll', 'Alchemy.dll',
    'THORN.dll', 'Deformable2.dll', 'FLMaterials.dll', 'x86math.dll', 'ximage.dll')

function Get-GhidraHome { (Get-ChildItem (Join-Path $g 'ghidra_12.1.4') -Directory)[0].FullName }
function Get-ServerProcess {
    # by its command line, or - when Windows does not give it (empty for the server mcp_launch.js starts detached, so
    # -Seed / -Stop missed it and the seed failed "Unable to lock project", 30 Oct 2026, WORK_QUEUE H17) - as the owner
    # of the server's loopback port 8089 whose java.exe lives under research\ghidra
    $byCmd = @(Get-CimInstance Win32_Process -Filter "Name='java.exe'" |
               Where-Object { $_.CommandLine -like '*GhidraMCPHeadlessServer*' -and $_.CommandLine -like "*$g*" })
    $byPort = @(Get-NetTCPConnection -LocalPort 8089 -State Listen -ErrorAction SilentlyContinue | ForEach-Object {
                   Get-CimInstance Win32_Process -Filter "ProcessId=$($_.OwningProcess)" } |
                Where-Object { $_.Name -eq 'java.exe' -and "$($_.ExecutablePath)" -like "$g\*" })
    @($byCmd + $byPort) | Sort-Object ProcessId -Unique
}
function Stop-Server {
    foreach ($p in @(Get-ServerProcess)) { Stop-Process -Id $p.ProcessId -Force; "stopped the MCP server (pid $($p.ProcessId))" }
    # a project lock no process holds (a server that died without closing the project; Ghidra never clears it: every
    # headless run failed "Unable to lock project", 30 Oct 2026, H17) is moved aside - the move fails while a process
    # still has it open, which leaves a live lock alone
    $proj = Join-Path $g 'project'
    if (-not @(Get-ServerProcess).Count) {
        $stamp = Get-Date -Format 'yyyyMMdd_HHmmss'
        foreach ($f in 'FLvibed.lock', 'FLvibed.lock~') {
            $p = Join-Path $proj $f
            if (Test-Path -LiteralPath $p) {
                try { [IO.File]::Move($p, "$p.stale_$stamp"); "moved a stale project lock aside: $f" } catch { "project lock $f is held: left alone" }
            }
        }
    }
}
function Invoke-Headless([string[]]$HeadlessArgs, [string]$Name) {
    $env:JAVA_HOME = (Get-ChildItem (Join-Path $g 'jdk21') -Directory)[0].FullName
    $ah = Join-Path (Get-GhidraHome) 'support\analyzeHeadless.bat'
    $null = New-Item -ItemType Directory -Force $logs
    $out = Join-Path $logs "$Name.out"
    $t = Measure-Command { & $ah (Join-Path $g 'project') FLvibed @HeadlessArgs -log (Join-Path $logs "$Name.log") -scriptlog (Join-Path $logs "$Name.script.log") *> $out }
    if ($LASTEXITCODE -ne 0) { throw "analyzeHeadless $Name failed (exit $LASTEXITCODE): $out" }
    "$Name done in $([int]$t.TotalSeconds) s"
}
function Update-SeedTsv {
    $tsv = Join-Path $g 'flexe.tsv'
    node (Join-Path $PSScriptRoot 'flexe_tsv.js') $tsv
    if ($LASTEXITCODE -ne 0) { throw 'flexe_tsv.js failed' }
    $tsv
}
function Update-NotesTsv { # docs\FREELANCER_RE.md's addresses (FLvibedNotes.java)
    $tsv = Join-Path $g 're_notes.tsv'
    node (Join-Path $PSScriptRoot 're_notes.js') $tsv
    if ($LASTEXITCODE -ne 0) { throw 're_notes.js failed' }
    $tsv
}
function Get-Imported {
    $idx = Join-Path $g 'project\FLvibed.rep\idata\~index.dat'
    if (-not (Test-Path $idx)) { return @() }
    # the index lists each file as "  <id>:<name>:<file id>"
    @(Get-Content $idx | ForEach-Object { if ($_ -match '^\s+[0-9a-f]+:([^:]+):') { $Matches[1] } })
}

if ($Import) {
    Stop-Server
    $null = New-Item -ItemType Directory -Force $bin
    foreach ($f in $games) {
        $dst = Join-Path $bin $f
        if (Test-Path $dst) { continue }
        $src = @((Join-Path $exeDir $f), (Join-Path $dllDir $f)) | Where-Object { Test-Path $_ } | Select-Object -First 1
        if (-not $src) { Write-Warning "$f not in the test install"; continue }
        Copy-Item -LiteralPath $src $dst
        (Get-Item $dst).IsReadOnly = $true
    }
    $want = if ($Files) { $Files } else { $games }
    $have = Get-Imported
    $todo = @($want | Where-Object { $have -notcontains $_ -and (Test-Path (Join-Path $bin $_)) })
    if (-not $todo) { 'everything imported already'; return }
    $tsv = Update-SeedTsv
    $notes = Update-NotesTsv
    $paths = $todo | ForEach-Object { Join-Path $bin $_ }
    Invoke-Headless (@('-import') + $paths + @('-postScript', 'FLvibedSeed.java', $tsv, '-postScript', 'FLvibedNotes.java', $notes,
        '-scriptPath', $PSScriptRoot, '-max-cpu', '8')) "import_$(Get-Date -Format yyyyMMdd_HHmm)"
    $Vtables = $true; $Files = $todo
}
if ($Seed) {
    Stop-Server
    $tsv = Update-SeedTsv
    $notes = Update-NotesTsv
    Invoke-Headless @('-process', 'Freelancer.exe', '-noanalysis', '-postScript', 'FLvibedSeed.java', $tsv,
        '-postScript', 'FLvibedNotes.java', $notes, '-scriptPath', $PSScriptRoot) "seed_$(Get-Date -Format yyyyMMdd_HHmm)"
}
if ($Vtables) {
    Stop-Server
    $want = if ($Files) { $Files } else { Get-Imported }
    foreach ($f in $want) {
        Invoke-Headless @('-process', $f, '-noanalysis', '-postScript', 'FLvibedVtables.java', '-scriptPath', $PSScriptRoot) "vtables_$($f -replace '\W', '_')"
    }
    foreach ($f in $want) {
        $l = Join-Path $logs "vtables_$($f -replace '\W', '_').script.log"
        if (Test-Path $l) { (Select-String -LiteralPath $l -Pattern 'FLvibedVtables: ' | Select-Object -Last 1).Line -replace '^.*FLvibedVtables: ', '' }
    }
}
if ($Stop) { Stop-Server }
if ($Serve) {
    node (Join-Path $PSScriptRoot 'mcp_launch.js') --server-only
    if ($LASTEXITCODE -ne 0) { throw 'the MCP server did not start (research\ghidra\logs\server.log)' }
}
if ($Check) {
    $tsv = Join-Path $g 'flexe.tsv' # written by the last seed (Update-SeedTsv)
    if (-not (Test-Path (Join-Path $g 'project\FLvibed.gpr'))) { 'ghidra: not set up (optional; tools\ghidra\README.md)'; exit 0 }
    $now = (node (Join-Path $PSScriptRoot 'flexe_tsv.js') 2>$null) -join "`n"
    $seeded = if (Test-Path $tsv) { (Get-Content $tsv) -join "`n" } else { '' }
    $n = @($now -split "`n" | Where-Object { $_ }).Count
    if ($now -ne $seeded) { "ghidra: the project's seed is older than core\flexe.h ($n symbols now): tools\ghidra\ghidra.ps1 -Seed"; exit 1 }
    $notesTsv = Join-Path $g 're_notes.tsv'
    $notesNow = (node (Join-Path $PSScriptRoot 're_notes.js') 2>$null) -join "`n"
    $notesThen = if (Test-Path $notesTsv) { (Get-Content $notesTsv -Encoding utf8) -join "`n" } else { '' }
    if ($notesNow -ne $notesThen) { "ghidra: FREELANCER_RE.md changed since its notes went into the project: tools\ghidra\ghidra.ps1 -Seed"; exit 1 }
    "ghidra: seeded with today's flexe.h ($n symbols) and FREELANCER_RE.md, $(@(Get-Imported).Count) programs"
    exit 0
}
if ($Status) {
    "ghidra:   $(if (Test-Path (Join-Path $g 'ghidra_12.1.4')) { Get-GhidraHome } else { 'not installed' })"
    "imported: $((Get-Imported) -join ', ')"
    $p = @(Get-ServerProcess)
    "server:   $(if ($p) { "running (pid $($p.ProcessId -join ', '))" } else { 'not running' })"
}
