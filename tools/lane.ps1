# Parallel lanes: one git worktree per line of C++ work, beside the main checkout, each with its own branch, build\
# folder, build lock and game test instance, merged into main one at a time and removed after its merge (the owner,
# 12 Nov 2026). Lanes are made and removed ONLY with this tool (docs\TESTING.md "Parallel lanes", tools\README.md).
#
#   lane.ps1 new <name> -Instance <n> [-From <commit or branch, default main>] [-Owner <who>] [-Force]
#   lane.ps1 status
#   lane.ps1 sync <name>                    rebase lane/<name> on main (refuses uncommitted changes; stops on conflicts);
#                                           a verified lane whose sync brought in no build input (documents, game-net scripts,
#                                           PowerShell tools, the Claude harness only: Test-BuildInput) keeps its marker on the new tip
#   lane.ps1 verify <name> [-Passed 'nn_pages=<run folder>,classic_frames=<run folder>'] [-Owner <who>]
#                                           runs the unit tests, a release build and the module lint IN the lane on its
#                                           tip and writes the marker merge needs (.lane\verified\<commit>.json)
#   lane.ps1 merge <name> [-WhatIf]         fast-forwards main to the lane's verified, rebased tip; never pushes
#   lane.ps1 remove <name> [-KeepBranch] [-Abandon] [-KeepScratch <folder> | -DiscardScratch] [-WhatIf]
#   lane.ps1 adopt <name> -Instance <n>     re-registers a lane worktree whose registry entry was lost
#
# A lane is the worktree ..\FLvibed-lane-<name> on the branch lane/<name>. It holds:
#   - junctions to main's git-ignored shared folders ($Shared below: the test install, the reference data, the saves, the
#     Python venv, the evidence folders), never to a tracked path (research\experience\legs\legs.tsv is tracked, so the
#     folders inside research\experience are linked one by one, legs\ excepted);
#   - its own build\ (build\sdl3 copied from main: no SDL3 rebuild) and build lock (tools\buildlock.ps1: a linked
#     worktree locks its own folder only, so lanes build beside main and each other);
#   - scratch\ (the worker's scratch files) and .lane\ (this tool's markers and logs), both git-ignored;
#   - a game instance reserved in ..\.claude\lanes.json (deploy.ps1 refuses a start there from any other checkout, and a
#     lane's start anywhere else).
# Safety: nothing is ever deleted recursively through a reparse point. remove takes every reparse point under the
# worktree out first, one by one and non-recursively ([IO.Directory]::Delete(path, $false) removes the link only), and
# compares each target's file list before and after; then git worktree remove. The main checkout and
# ..\FLvibed-0.1-export are refused everywhere; git worktree list and the registry must agree.
# Exit code: 0 done, 1 refused or failed (the line starting REFUSED / FAILED says why).
param([Parameter(Position = 0)][string]$Command = 'status', [Parameter(Position = 1)][string]$Name = '',
      [string]$Instance = '', [string]$From = 'main', [string]$Owner = '', [switch]$Force,
      [switch]$KeepBranch, [switch]$Abandon, [string]$KeepScratch = '', [switch]$DiscardScratch, [switch]$WhatIf,
      [string]$Passed = '')
$ErrorActionPreference = 'Stop'
trap { Write-Host "FAILED: $($_.Exception.Message) (lane.ps1 line $($_.InvocationInfo.ScriptLineNumber))"; exit 1 }

# ---------------------------------------------------------------- where things are
$Here = Split-Path $PSScriptRoot -Parent                 # the checkout this copy sits in (main or a lane)
$gitCommon = (& git.exe -C $Here rev-parse --path-format=absolute --git-common-dir 2>$null)
if ($LASTEXITCODE -ne 0 -or -not $gitCommon) { Write-Host "FAILED: $Here is not a git checkout"; exit 1 }
$Main = [IO.Path]::GetFullPath((Split-Path ([IO.Path]::GetFullPath("$gitCommon")) -Parent)).TrimEnd('\')
$EnvDir = Split-Path $Main -Parent
$Export = Join-Path $EnvDir 'FLvibed-0.1-export'
$Registry = Join-Path $EnvDir '.claude\lanes.json'
$MainBranch = 'main'
$NeverInstances = @('v11nightly')                         # the owner's: never used by a test
# The shared, git-ignored folders a lane links to main's (what the build, the unit tests, deploy.ps1, the harness,
# tests\nn_pages.ps1, tests\classic_frames.ps1 and tools\module_lint.py read; checked 12 Nov 2026). Plus every folder
# inside research\experience but legs\ (added in New-Lane).
$Shared = @(
    'Install',              # the test install and its instances: deploy, the harness, the unit tests' data tests
    'research\Vanilla',     # vanilla 1.1 data: tests\ini_test.cpp, universe_test.cpp
    'research\repos',       # reference repositories; the SDL3 source when build\sdl3's cache misses
    'research\saves',       # fixture and checkpoint saves (flleg, flsweep), the scene-save archive the harness writes
    'research\tools',       # the Python venv (module_lint, classic_frames, splits); deploy parses research\tools\*.ps1
    'research\evidence',    # research evidence tests and notes cite
    'research\disasm'       # tools\dis.ps1
)
# Build output a removal may delete without asking (recreatable); anything else ignored and not linked is the lane's own
$OutputRx = '^build\\(dev|release|test|sigtool|sdl3|mptest|flhook|bench)\\|^build\\setup\\(?!test\\)|(^|\\)__pycache__\\'

# ---------------------------------------------------------------- small helpers
function Say([string]$s) { Write-Host $s }
function Fail([string]$s) { Write-Host "REFUSED: $s"; exit 1 }
function Full([string]$p) { [IO.Path]::GetFullPath($p).TrimEnd('\') }
# A path is in a folder when it IS the folder or lies below it: a path boundary, never a bare prefix (..\FLvibed-lane-fixes3
# starts with ..\FLvibed-lane-fixes: `remove fixes` was refused for fixes3's game, 13 Nov 2026). Case-insensitive.
function Test-InFolder([string]$path, [string]$folder) {
    $f = $folder.TrimEnd('\')
    $path -ieq $f -or $path.StartsWith("$f\", [StringComparison]::OrdinalIgnoreCase)
}
# The same for a command line: the folder appears followed by a separator, a quote, a space or the end
function Test-CommandLineInFolder([string]$cmd, [string]$folder) {
    $cmd -match ('(?i)' + [regex]::Escape($folder.TrimEnd('\')) + '(?=\z|[\\/"''\s])')
}
# git in a folder; output as strings, exit code in $script:GitCode (a stderr line never throws here). Long paths on (a
# 327-character path stopped git worktree remove half-way, the breaker's test 12 Nov 2026) and paths unquoted (a quoted
# "\303\274" name was not found, so an untracked file missed the -Abandon stash)
function GitIn {
    $dir = $args[0]; $rest = @($args | Select-Object -Skip 1)
    $ErrorActionPreference = 'Continue'
    $out = @(& git.exe -c core.longpaths=true -c core.quotepath=false -C $dir @rest 2>&1 | ForEach-Object { "$_" })
    $script:GitCode = $LASTEXITCODE
    $out
}
function LanePath([string]$n) { Full (Join-Path $EnvDir "FLvibed-lane-$n") }
function Assert-Name([string]$n) {
    # (\z: $ also matches before a final newline - a name "brk`n" passed and left a ghost entry)
    if ($n -cnotmatch '^[a-z0-9][a-z0-9-]{0,23}\z') { Fail "lane name '$n': lower-case letters, digits and '-', at most 24 characters" }
}
function Assert-LanePath([string]$p) {
    $f = Full $p
    if ($f -ieq $Main) { Fail "$f is the main checkout" }
    if ($f -ieq (Full $Export)) { Fail "$f is the published clone" }
    if ((Split-Path $f -Parent) -ine (Full $EnvDir) -or (Split-Path $f -Leaf) -notmatch '^FLvibed-lane-[a-z0-9][a-z0-9-]*$') {
        Fail "$f is not a lane folder (..\FLvibed-lane-<name> beside the main checkout)"
    }
    if ((Test-Path -LiteralPath $f) -and ((Get-Item -LiteralPath $f -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        Fail "$f is itself a reparse point"
    }
}
function Assert-Instance([string]$n) {
    if ($NeverInstances -contains $n) { Fail "instance $n is never to be used" }
    if ($n -notmatch '^[0-9]{1,2}\z' -or [int]$n -lt 1 -or [int]$n -gt 99) { Fail "instance '$n': lanes use numbered instances 1-99 (the main install and the prepared named instances are not lane instances)" }
}
# every file under a folder, as full paths, never entering a reparse point
function New-WalkOptions { $o = [IO.EnumerationOptions]::new(); $o.RecurseSubdirectories = $true; $o.AttributesToSkip = [IO.FileAttributes]::ReparsePoint; $o.IgnoreInaccessible = $true; $o }
# every reparse point (folder or file) under a folder, found without entering any
function Get-ReparsePoints([string]$dir) {
    $found = [Collections.Generic.List[IO.FileSystemInfo]]::new()
    $stack = [Collections.Generic.Stack[string]]::new(); $stack.Push($dir)
    # (an unreadable folder throws: a link inside it would go unseen, and git worktree remove would meet it)
    $o = [IO.EnumerationOptions]::new(); $o.AttributesToSkip = 0; $o.IgnoreInaccessible = $false
    while ($stack.Count) {
        foreach ($e in [IO.DirectoryInfo]::new($stack.Pop()).EnumerateFileSystemInfos('*', $o)) {
            if ($e.Attributes -band [IO.FileAttributes]::ReparsePoint) { $found.Add($e) }
            elseif ($e -is [IO.DirectoryInfo]) { $stack.Push($e.FullName) }
        }
    }
    $found   # (unrolled: callers wrap the call in @())
}
function Get-Snapshot([string]$dir) {
    $set = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($f in [IO.Directory]::EnumerateFiles($dir, '*', (New-WalkOptions))) { $null = $set.Add($f.Substring($dir.Length)) }
    , $set
}
function Get-TreeBytes([string]$dir) {
    $b = [long]0
    foreach ($f in [IO.DirectoryInfo]::new($dir).EnumerateFiles('*', (New-WalkOptions))) { $b += $f.Length }
    $b
}
# copies a folder (no reparse point inside it allowed) and returns the number of files
function Copy-Tree([string]$from, [string]$to) {
    $rp = @(Get-ReparsePoints $from)
    if ($rp.Count) { Fail "will not copy ${from}: it holds a reparse point ($($rp[0].FullName))" }
    $n = 0
    foreach ($f in [IO.Directory]::EnumerateFiles($from, '*', (New-WalkOptions))) {
        $dst = $to + $f.Substring($from.Length)
        $null = [IO.Directory]::CreateDirectory((Split-Path $dst -Parent))
        [IO.File]::Copy($f, $dst, $false); $n++
    }
    $n
}
# The build lock's name for a checkout: tools\buildlock.ps1 Get-FLBuildMutexName says the same (keep the two equal)
function Get-BuildMutexName([string]$root) {
    $h = [Security.Cryptography.SHA256]::HashData([Text.Encoding]::UTF8.GetBytes((Full $root).ToLowerInvariant()))
    'Global\FLvibedBuild_' + ([BitConverter]::ToString($h, 0, 6) -replace '-', '')
}
function Test-BuildRunning([string]$root) {
    $m = [Threading.Mutex]::new($false, (Get-BuildMutexName $root))
    try {
        $free = $false
        try { $free = $m.WaitOne(0) } catch [Threading.AbandonedMutexException] { $free = $true }
        if ($free) { $m.ReleaseMutex() }
        -not $free
    } finally { $m.Dispose() }
}

# ---------------------------------------------------------------- processes and instances
function Get-Ancestors {
    $all = @{}; foreach ($p in Get-CimInstance Win32_Process) { $all[[int]$p.ProcessId] = [int]$p.ParentProcessId }
    $ids = [Collections.Generic.HashSet[int]]::new(); $id = $PID
    while ($id -and $all.ContainsKey($id) -and $ids.Add($id)) { $id = $all[$id] }
    , $ids
}
# Freelancer.exe processes of instance <n>, started from ANY checkout (a game started through a lane's Install junction
# reports the junction's path: tested 12 Nov 2026)
function Get-InstanceGames([string]$n) {
    $rx = '\\Install\\instances\\' + [regex]::Escape($n) + '\\Freelancer HD Edition\\EXE\\Freelancer\.exe$'
    @(Get-CimInstance Win32_Process -Filter "Name='Freelancer.exe'" | Where-Object { $_.ExecutablePath -match $rx })
}
# processes whose image or command line lies in the worktree (a game, a build's cmd, a test script), not this one's
function Get-LaneProcesses([string]$p) {
    $mine = Get-Ancestors
    @(Get-CimInstance Win32_Process | Where-Object {
        -not $mine.Contains([int]$_.ProcessId) -and (
            ($_.ExecutablePath -and (Test-InFolder $_.ExecutablePath $p)) -or
            ($_.CommandLine -and (Test-CommandLineInFolder $_.CommandLine $p))) })
}
function Get-InstanceLastUse([string]$n) {
    $exe = Join-Path $Main "Install\instances\$n\Freelancer HD Edition\EXE"
    if (-not (Test-Path -LiteralPath $exe)) { return $null }
    $t = @(Get-ChildItem -LiteralPath $exe -Filter '*.log' -File -ErrorAction SilentlyContinue) +
         @(Get-ChildItem -LiteralPath (Join-Path $exe 'logs') -File -ErrorAction SilentlyContinue) | Sort-Object LastWriteTime | Select-Object -Last 1
    if ($t) { $t.LastWriteTime } else { $null }
}

# ---------------------------------------------------------------- registry (..\.claude\lanes.json)
function Read-Registry {
    if (-not (Test-Path -LiteralPath $Registry)) { return }
    $j = Get-Content -LiteralPath $Registry -Raw | ConvertFrom-Json -DateKind String
    $j.lanes | Where-Object { $_ }   # (unrolled: callers wrap the call in @())
}
function Write-Registry($lanes) {
    $doc = [ordered]@{
        about = 'Parallel lanes (FLvibed\tools\lane.ps1): written by that tool only. One entry per lane: its worktree, branch, reserved game instance, owner.'
        lanes = @($lanes)
    }
    $tmp = "$Registry.tmp"
    [IO.File]::WriteAllText($tmp, ($doc | ConvertTo-Json -Depth 6))
    # a scanner or watcher holds the file a moment after each write: "access denied" 2 times in 3 (the breaker, 12 Nov
    # 2026); retried for up to 10 s
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        try { [IO.File]::Move($tmp, $Registry, $true); return }
        catch { if ($sw.Elapsed.TotalSeconds -gt 10) { throw }; Start-Sleep -Milliseconds 200 }
    }
}
# read-modify-write under a machine-wide mutex; $Change gets the list and returns the new list (or a string = refusal)
function Update-Registry([scriptblock]$Change) {
    $m = [Threading.Mutex]::new($false, 'Global\FLvibedLanes')
    try {
        try { $null = $m.WaitOne() } catch [Threading.AbandonedMutexException] { }
        $res = & $Change ([object[]]@(Read-Registry))
        if ($res -is [string]) { return $res }
        Write-Registry $res
        ''
    } finally { $m.ReleaseMutex(); $m.Dispose() }
}
function Set-LaneField([string]$n, [string]$field, $value) {
    $r = Update-Registry {
        param($lanes)
        foreach ($l in $lanes) { if ($l.name -eq $n) { $l | Add-Member -NotePropertyName $field -NotePropertyValue $value -Force } }
        , $lanes
    }
    if ($r) { Fail $r }
}

# ---------------------------------------------------------------- the lane as git and the registry see it
function Get-Worktrees {
    $list = [Collections.Generic.List[object]]::new(); $cur = $null
    foreach ($l in (GitIn $Main worktree list --porcelain)) {
        if ($l -like 'worktree *') { if ($cur) { $list.Add([pscustomobject]$cur) }; $cur = @{ Path = (Full $l.Substring(9)); Branch = ''; Head = ''; Prunable = $false } }
        elseif ($l -like 'branch *') { $cur.Branch = $l.Substring(7) -replace '^refs/heads/', '' }
        elseif ($l -like 'HEAD *') { $cur.Head = $l.Substring(5) }
        elseif ($l -like 'prunable*') { $cur.Prunable = $true }
    }
    if ($cur) { $list.Add([pscustomobject]$cur) }
    if ($script:GitCode) { Fail "git worktree list failed" }
    $list   # (unrolled: callers wrap the call in @())
}
function Get-Lane([string]$n) {
    $p = LanePath $n
    $L = [pscustomobject]@{
        Name = $n; Path = $p; Branch = "lane/$n"
        Reg = (@(Read-Registry) | Where-Object { $_.name -eq $n } | Select-Object -First 1)
        Wt = (@(Get-Worktrees) | Where-Object { $_.Path -ieq $p } | Select-Object -First 1)
        Exists = (Test-Path -LiteralPath $p)
        Prunable = $false
        HasBranch = $(GitIn $Main show-ref --verify --quiet "refs/heads/lane/$n" | Out-Null; $script:GitCode -eq 0)
    }
    # a worktree git still lists whose folder is gone (deleted by hand): no worktree; remove prunes it
    if ($L.Wt -and -not $L.Exists) { $L.Wt = $null; $L.Prunable = $true }
    $L
}
# the worktree and the registry agree (path, branch); unregistered lanes are refused unless allowed
function Assert-Agree($L, [switch]$AllowUnregistered) {
    if ($L.Reg -and (Full $L.Reg.path) -ine $L.Path) { Fail "the registry puts lane $($L.Name) at $($L.Reg.path), not $($L.Path)" }
    if ($L.Reg -and $L.Reg.branch -ne $L.Branch) { Fail "the registry names branch $($L.Reg.branch) for lane $($L.Name), not $($L.Branch)" }
    if (-not $L.Wt) {
        if ($L.Reg) { Fail "lane $($L.Name) is in the registry but git worktree list has no worktree at $($L.Path) (lane.ps1 remove $($L.Name) clears the entry)" }
        Fail "there is no lane $($L.Name) (lane.ps1 status)"
    }
    if ($L.Wt.Branch -ne $L.Branch) { Fail (Get-BranchAdvice $L) }
    if (-not $L.Reg -and -not $AllowUnregistered) { Fail "lane $($L.Name) has a worktree but no entry in $Registry (the two disagree): lane.ps1 adopt $($L.Name) -Instance <n> re-registers it" }
}
# uncommitted changes as "XY path" lines, the path as it is on disk (-z: never quoted; a rename's old path dropped)
function Get-Dirty([string]$wt) {
    $s = GitIn $wt status --porcelain=v1 -z --untracked-files=all
    if ($script:GitCode) { Fail "git status failed in ${wt}: $($s -join ' ')" }
    $e = @((($s -join "`n") -split "`0") | Where-Object { $_ })
    for ($i = 0; $i -lt $e.Count; $i++) {
        $e[$i]   # (unrolled: callers wrap the call in @())
        if ($e[$i].Length -ge 2 -and ($e[$i][0] -in 'R', 'C' -or $e[$i][1] -in 'R', 'C')) { $i++ }
    }
}
# git is stopped in a rebase: the advice for a detached HEAD there
function Get-BranchAdvice($L) {
    $busy = Test-GitBusy $L.Path
    if ($busy -like 'rebase*') { return "a rebase is stopped in the lane (HEAD detached): finish it (git -C `"$($L.Path)`" rebase --continue) or give it up (rebase --abort)" }
    "the worktree $($L.Path) is on '$($L.Wt.Branch)' (empty = detached HEAD), not $($L.Branch): check out $($L.Branch) there first"
}
function Test-GitBusy([string]$wt) {
    foreach ($x in 'rebase-merge', 'rebase-apply', 'MERGE_HEAD', 'CHERRY_PICK_HEAD', 'REVERT_HEAD') {
        $p = (GitIn $wt rev-parse --path-format=absolute --git-path $x | Select-Object -First 1)
        if ($p -and (Test-Path -LiteralPath $p)) { return $x }
    }
    ''
}
function Get-Owner { if ($Owner) { $Owner } elseif ($env:CLAUDE_CODE_SESSION_ID) { "session $($env:CLAUDE_CODE_SESSION_ID.Substring(0, 8))" } else { 'unnamed' } }
function Get-Marker([string]$wt, [string]$sha) { Join-Path $wt ".lane\verified\$sha.json" }

# ---------------------------------------------------------------- new / adopt
function Assert-InstanceFree([string]$n, [string]$exceptLane) {
    $taken = @(Read-Registry) | Where-Object { "$($_.instance)" -eq $n -and $_.name -ne $exceptLane } | Select-Object -First 1
    if ($taken) { Fail "instance $n is reserved by lane $($taken.name) ($($taken.path))" }
    $games = Get-InstanceGames $n
    if ($games.Count) { Fail "a game runs on instance $n (pid $($games[0].ProcessId), $($games[0].ExecutablePath))" }
    $last = Get-InstanceLastUse $n
    if ($last -and $last -gt (Get-Date).AddMinutes(-30) -and -not $Force) {
        Fail "instance $n's game logs were written at $($last.ToString('HH:mm')) (under 30 min ago): another session may be using it. Pick another, or -Force when you know it is free"
    }
}
function Write-Usage($L, [string]$inst) {
    $wt = $L.Path
    $py = "& `"$wt\research\tools\ocr\.venv\Scripts\python.exe`" -I"
    Say ''
    Say "Lane $($L.Name): $wt, branch $($L.Branch), game instance $inst. Work, build and test ONLY there:"
    Say "  edit + commit   git -C `"$wt`" add <paths>; git -C `"$wt`" commit -m '...' -- <paths>"
    Say "  build           & `"$wt\tools\buildlock.ps1`" dev | release | test      (the lane's own lock: builds beside main)"
    Say "  deploy          & `"$wt\tools\deploy.ps1`" -Background -Release -NoBuild -Instance $inst"
    Say "  harness         `$env:FLVIBED_INSTANCE = '$inst'; . `"$wt\tools\harness\flctl.ps1`""
    Say "  NeuroNet net    & `"$wt\tests\nn_pages.ps1`" -Instance $inst"
    Say "  classic net     & `"$wt\tests\classic_frames.ps1`" -Instance $inst      (the shared pinned set; never -Pin from a lane)"
    Say "  lint            $py `"$wt\tools\module_lint.py`" --enforce"
    Say "  scratch files   $wt\scratch\   (git-ignored, private to the lane)"
    Say "Then, in order:   & `"$Main\tools\lane.ps1`" sync $($L.Name)  ->  verify $($L.Name) [-Passed 'nn_pages=<run folder>,classic_frames=<run folder>']  ->  merge $($L.Name)  ->  remove $($L.Name)"
}
function New-Lane {
    Assert-Name $Name
    if (-not $Instance) { Fail 'new needs -Instance <n>' }
    Assert-Instance $Instance
    $L = Get-Lane $Name
    Assert-LanePath $L.Path
    if ($L.Exists) { Fail "$($L.Path) exists already" }
    if ($L.Wt) { Fail "git worktree list already has $($L.Path)" }
    if ($L.HasBranch) { Fail "branch $($L.Branch) exists already (an earlier lane's, kept? delete it or pick another name)" }
    $base = (GitIn $Main rev-parse --verify --quiet "$From^{commit}" | Select-Object -First 1)
    if ($script:GitCode -or -not $base) { Fail "-From '$From' is no commit" }
    if (-not (Test-Path -LiteralPath (Join-Path $Main 'Install'))) { Fail "the main checkout has no Install folder to share" }
    $sdl3 = Join-Path $Main 'build\sdl3'
    Assert-InstanceFree $Instance ''
    $entry = [ordered]@{ name = $Name; path = $L.Path; branch = $L.Branch; instance = "$Instance"; base = $base; from = $From
                         created = (Get-Date -Format s); owner = (Get-Owner); state = 'creating' }
    $r = Update-Registry {
        param($lanes)
        if ($lanes | Where-Object { $_.name -eq $Name }) { return "lane $Name is in the registry already" }
        if ($lanes | Where-Object { "$($_.instance)" -eq "$Instance" }) { return "instance $Instance is reserved already" }
        , (@($lanes) + [pscustomobject]$entry)
    }
    if ($r) { Fail $r }
    try {
        Say "git worktree add: $($L.Path) on $($L.Branch) from $From ($($base.Substring(0, 8)))"
        $o = GitIn $Main worktree add -b $L.Branch $L.Path $base
        if ($script:GitCode) { throw "git worktree add failed: $($o -join ' ')" }
        # junctions to main's shared folders; parents created as real folders
        $links = [Collections.Generic.List[string]]::new()
        foreach ($rel in $Shared) { $links.Add($rel) }
        $exp = Join-Path $Main 'research\experience'
        foreach ($d in Get-ChildItem -LiteralPath $exp -Directory -Force) {
            if ($d.Name -ne 'legs' -and -not ($d.Attributes -band [IO.FileAttributes]::ReparsePoint)) { $links.Add("research\experience\$($d.Name)") }
        }
        foreach ($rel in $links) {
            $target = Join-Path $Main $rel
            $link = Join-Path $L.Path $rel
            if (-not (Test-Path -LiteralPath $target -PathType Container)) { Say "  (no $rel in main: not linked)"; continue }
            if ((Get-Item -LiteralPath $target -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) { Say "  ($rel in main is itself a link: not linked)"; continue }
            if (Test-Path -LiteralPath $link) { throw "$link exists in the new worktree (a tracked path?): not linked" }
            $null = [IO.Directory]::CreateDirectory((Split-Path $link -Parent))
            $null = New-Item -ItemType Junction -Path $link -Target $target
            $it = Get-Item -LiteralPath $link -Force
            if ($it.LinkType -ne 'Junction' -or (Full $it.LinkTarget) -ine (Full $target)) { throw "junction $link -> $target did not come out right" }
        }
        Say "  $($links.Count) junctions into main's shared folders"
        # private: build\sdl3 (the SDL3 static library cache), scratch\, .lane\
        if (Test-Path -LiteralPath $sdl3) { $n = Copy-Tree $sdl3 (Join-Path $L.Path 'build\sdl3'); Say "  build\sdl3 copied ($n files)" }
        else { Say '  (main has no build\sdl3: the first build makes it from research\repos\SDL3-*)' }
        foreach ($d in 'scratch', '.lane\verified') { $null = [IO.Directory]::CreateDirectory((Join-Path $L.Path $d)) }
        foreach ($d in 'scratch\x', '.lane\x', 'Install\x', 'research\experience\runs\x') {
            $null = GitIn $L.Path check-ignore -q $d
            if ($script:GitCode) { throw "$d is not git-ignored in the lane" }
        }
        if (@(Get-Dirty $L.Path).Count) { throw "the new worktree is not clean: $((Get-Dirty $L.Path) -join '; ')" }
    } catch {
        Write-Host "FAILED: $($_.Exception.Message)"
        Write-Host "The lane is registered as 'creating'. Clear it with: & `"$PSCommandPath`" remove $Name -Abandon -DiscardScratch"
        exit 1
    }
    Set-LaneField $Name 'state' 'ready'
    Say "lane $Name made: build mutex $(Get-BuildMutexName $L.Path)"
    Write-Usage $L $Instance
}
function Add-Lane {   # adopt
    Assert-Name $Name
    if (-not $Instance) { Fail 'adopt needs -Instance <n>' }
    Assert-Instance $Instance
    $L = Get-Lane $Name
    Assert-LanePath $L.Path
    if ($L.Reg) { Fail "lane $Name is registered already" }
    Assert-Agree $L -AllowUnregistered
    Assert-InstanceFree $Instance ''
    $base = (GitIn $Main merge-base $MainBranch $L.Branch | Select-Object -First 1)
    $entry = [ordered]@{ name = $Name; path = $L.Path; branch = $L.Branch; instance = "$Instance"; base = $base; from = '(adopted)'
                         created = (Get-Date -Format s); owner = (Get-Owner); state = 'ready' }
    $r = Update-Registry { param($lanes) if ($lanes | Where-Object { $_.name -eq $Name -or "$($_.instance)" -eq "$Instance" }) { return 'taken meanwhile' }; , (@($lanes) + [pscustomobject]$entry) }
    if ($r) { Fail $r }
    foreach ($d in 'scratch', '.lane\verified') { $null = [IO.Directory]::CreateDirectory((Join-Path $L.Path $d)) }
    Say "lane $Name adopted on instance $Instance"
    Write-Usage $L $Instance
}

# ---------------------------------------------------------------- status
function Show-Status {
    $reg = @(Read-Registry)
    $wts = @(Get-Worktrees)
    $mainTip = (GitIn $Main rev-parse --short $MainBranch | Select-Object -First 1)
    Say "main $mainTip at $Main; registry $Registry ($($reg.Count) lanes)"
    $names = [Collections.Generic.List[string]]::new()
    foreach ($e in $reg) { $names.Add($e.name) }
    foreach ($w in $wts) {
        if ($w.Path -ieq $Main) { continue }
        $leaf = Split-Path $w.Path -Leaf
        if ($leaf -match '^FLvibed-lane-(.+)$' -and -not $names.Contains($Matches[1])) { $names.Add($Matches[1]) }
        elseif ($leaf -notmatch '^FLvibed-lane-') { Say "  NOT A LANE: worktree $($w.Path) on '$($w.Branch)' (made by hand? remove it by hand, junctions first)" }
    }
    if (-not $names.Count) { Say '  no lanes'; return }
    foreach ($n in $names) {
        $L = Get-Lane $n
        $flags = @()
        if (-not $L.Reg) { $flags += 'UNREGISTERED (lane.ps1 adopt)' }
        if (-not $L.Wt) { $flags += 'NO WORKTREE (lane.ps1 remove clears it)' }
        elseif ($L.Wt.Branch -ne $L.Branch) { $flags += "ON '$($L.Wt.Branch)'" }
        if ($L.Reg -and $L.Reg.state -ne 'ready') { $flags += "STATE $($L.Reg.state)" }
        $line = "  $n  $($L.Branch)"
        if ($L.HasBranch) {
            $c = (GitIn $Main rev-list --left-right --count "$MainBranch...$($L.Branch)" | Select-Object -First 1) -split '\s+'
            $tip = (GitIn $Main rev-parse $L.Branch | Select-Object -First 1)
            $line += "  +$($c[1]) ahead / -$($c[0]) behind main  tip $($tip.Substring(0, 8))"
            if ($L.Wt) { $line += $(if (Test-Path -LiteralPath (Get-Marker $L.Path $tip)) { ' verified' } else { ' unverified' }) }
        }
        if ($L.Wt) {
            $d = @(Get-Dirty $L.Path).Count
            $busy = Test-GitBusy $L.Path
            $line += "  $d uncommitted" + $(if ($busy) { " ($busy in progress)" } else { '' })
            $line += '  {0:N2} GB' -f ((Get-TreeBytes $L.Path) / 1GB)
            if (Test-BuildRunning $L.Path) { $line += '  BUILDING' }
        }
        if ($L.Reg) {
            $g = Get-InstanceGames "$($L.Reg.instance)"
            $line += "  instance $($L.Reg.instance): " + $(if ($g.Count) { "game pid $($g[0].ProcessId)" + $(if ($g[0].ExecutablePath.StartsWith("$($L.Path)\", [StringComparison]::OrdinalIgnoreCase)) { ' (from the lane)' } else { ' (NOT from the lane)' }) } else { 'no game' })
            $line += "  owner $($L.Reg.owner), since $($L.Reg.created)"
        }
        Say $line
        foreach ($f in $flags) { Say "      $f" }
    }
}

# ---------------------------------------------------------------- sync
# A path the verification (unit tests, release build, module lint) does not read: documents, the test scripts of the game nets, the
# PowerShell tools (except the two the verification itself runs), the Claude harness, the handoff. Everything else - sources, the data
# the build embeds, the lint's tables, the Python tools - is a build input. Unsure = an input (the marker is dropped, never wrongly kept).
function Test-BuildInput([string]$Path) {
    $p = ($Path -replace '\\', '/').TrimStart('/')
    if ($p -match '(?i)\.md$') { return $false }
    if ($p -match '(?i)^(HANDOFF(_history)?\.txt|tools/claude/.*|tests/[^/]+\.ps1)$') { return $false }
    if ($p -match '(?i)^tools/(?!buildlock\.ps1$|lane\.ps1$)[^/]+\.ps1$') { return $false }
    $true
}
# Writes the marker of $NewTip as a copy of $OldTip's (a sync that brought in no build input leaves the verified tree's inputs as they were).
# Returns a message, or '' when there was no marker to carry or main brought in a build input.
function Copy-MarkerToTip([string]$Wt, [string]$OldTip, [string]$NewTip, [string]$MainTip, [string[]]$Brought) {
    $old = Get-Marker $Wt $OldTip
    if ($OldTip -eq $NewTip -or -not (Test-Path -LiteralPath $old)) { return '' }
    try { $m = Get-Content -LiteralPath $old -Raw | ConvertFrom-Json -DateKind String } catch { return '' }
    if (-not $m.ok -or $m.commit -ne $OldTip) { return '' }
    $inputs = @($Brought | Where-Object { $_ -and (Test-BuildInput $_) })
    if ($inputs.Count) { return "" }
    $h = [ordered]@{}; foreach ($pr in $m.PSObject.Properties) { $h[$pr.Name] = $pr.Value }
    $h['commit'] = $NewTip; $h['main'] = $MainTip; $h['carried_from'] = $OldTip
    $new = Get-Marker $Wt $NewTip
    [IO.File]::WriteAllText($new, ($h | ConvertTo-Json -Depth 5))
    "the verified marker is kept for the new tip: main brought in $(@($Brought).Count) file(s), none a build input (carried from $($OldTip.Substring(0, 8)))"
}
function Sync-Lane {
    Assert-Name $Name
    $L = Get-Lane $Name; Assert-LanePath $L.Path; Assert-Agree $L
    $busy = Test-GitBusy $L.Path
    if ($busy) { Fail "a git operation is in progress in the lane ($busy): finish or abort it first" }
    $dirty = @(Get-Dirty $L.Path)
    if ($dirty.Count) { Fail "the lane has $($dirty.Count) uncommitted change(s): commit or discard them first`n  $($dirty[0..([Math]::Min(9, $dirty.Count - 1))] -join "`n  ")" }
    $before = (GitIn $L.Path rev-parse --short HEAD | Select-Object -First 1)
    $oldTip = @(GitIn $L.Path rev-parse HEAD)[0]
    $fork = @(GitIn $L.Path merge-base $oldTip $MainBranch)[0]
    $brought = if ($fork -match '^[0-9a-f]{40}$') { @(GitIn $L.Path diff --name-only $fork $MainBranch | Where-Object { $_ }) } else { $null }
    $o = GitIn $L.Path rebase $MainBranch
    if ($script:GitCode) {
        $u = @(GitIn $L.Path diff --name-only --diff-filter=U | Where-Object { $_ })
        Say "STOPPED: the rebase of $($L.Branch) on $MainBranch hit conflicts in $($u.Count) file(s):"
        foreach ($f in $u) { Say "  $f" }
        if (-not $u.Count) { Say ($o -join "`n") }
        Say "The rebase is left stopped in the lane. Resolve, then: git -C `"$($L.Path)`" add <files>; git -C `"$($L.Path)`" rebase --continue"
        Say "or give it up:  git -C `"$($L.Path)`" rebase --abort"
        exit 1
    }
    $after = (GitIn $L.Path rev-parse --short HEAD | Select-Object -First 1)
    $c = (GitIn $Main rev-list --left-right --count "$MainBranch...$($L.Branch)" | Select-Object -First 1) -split '\s+'
    $newTip = @(GitIn $L.Path rev-parse HEAD)[0]
    $kept = if ($null -ne $brought) { Copy-MarkerToTip $L.Path $oldTip $newTip @(GitIn $Main rev-parse $MainBranch)[0] $brought } else { '' }
    Say "lane $Name rebased on $MainBranch ($(@(GitIn $Main rev-parse --short $MainBranch)[0])): $before -> $after, +$($c[1]) ahead / -$($c[0]) behind. $(if ($kept) { $kept } else { 'Verify the new tip before merging.' })"
}

# ---------------------------------------------------------------- verify
# The marker: .lane\verified\<full commit id>.json = { lane, branch, commit, main (main's tip then), by, time, ok,
# checks: [ { name, ran_by = lane.ps1, exit, seconds, detail } | { name, ran_by = declared, evidence } ] }. merge reads it.
function Test-Lane {
    Assert-Name $Name
    $L = Get-Lane $Name; Assert-LanePath $L.Path; Assert-Agree $L
    if (Test-GitBusy $L.Path) { Fail 'a git operation is in progress in the lane' }
    $dirty = @(Get-Dirty $L.Path)
    if ($dirty.Count) { Fail "the lane has $($dirty.Count) uncommitted change(s): a verification must name a commit" }
    $sha = (GitIn $L.Path rev-parse HEAD | Select-Object -First 1)
    $commitTime = [DateTimeOffset]::FromUnixTimeSeconds([long]@(GitIn $L.Path log -1 --format=%ct HEAD)[0]).LocalDateTime
    $declared = @()
    foreach ($p in ($Passed -split ',' | Where-Object { $_.Trim() })) {
        $k, $v = $p.Trim() -split '=', 2
        if (-not $v) { Fail "-Passed '$p': write <check>=<evidence folder or file>" }
        $v = $v.Trim().Trim('"', "'")
        if (-not (Test-Path -LiteralPath $v)) { Fail "-Passed ${k}: $v does not exist" }
        $t = (Get-Item -LiteralPath $v -Force).LastWriteTime
        if ($t -lt $commitTime) { Fail "-Passed ${k}: $v was last written $t, before the tip commit ($commitTime): not evidence for it" }
        $declared += [ordered]@{ name = $k.Trim(); ran_by = 'declared'; evidence = (Full $v) }
    }
    $log = Join-Path $L.Path ".lane\verify_$($sha.Substring(0, 8))_$(Get-Date -Format yyyyMMdd_HHmmss).log"
    $null = [IO.Directory]::CreateDirectory((Split-Path $log -Parent))
    $checks = @(); $ok = $true
    $py = Join-Path $L.Path 'research\tools\ocr\.venv\Scripts\python.exe'
    $runs = @(
        @{ name = 'unit'; run = { & (Join-Path $L.Path 'tools\buildlock.ps1') test 2>&1 } },
        @{ name = 'release'; run = { & (Join-Path $L.Path 'tools\buildlock.ps1') release 2>&1 } },
        @{ name = 'module_lint'; run = { & $py -I (Join-Path $L.Path 'tools\module_lint.py') --enforce 2>&1 } }
    )
    foreach ($c in $runs) {
        Say "verify $Name ($($sha.Substring(0, 8))): $($c.name) ..."
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $ErrorActionPreference = 'Continue'
        $out = @(& $c.run | ForEach-Object { "$_" })
        $code = $LASTEXITCODE
        $ErrorActionPreference = 'Stop'
        Add-Content -LiteralPath $log -Value (@("==== $($c.name): exit $code, $([int]$sw.Elapsed.TotalSeconds) s") + $out)
        $detail = ($out | Select-Object -Last 1)
        if ($c.name -eq 'release' -and $code -eq 0) {
            $info = (Get-Content -LiteralPath (Join-Path $L.Path 'build\release\build_info.txt') -ErrorAction SilentlyContinue | Select-Object -First 1)
            if ($info -notmatch "^$($sha.Substring(0, 7))[0-9a-f]* src:clean ") { $code = 99; $detail = "build_info.txt does not name the clean tip: $info" } else { $detail = $info }
        }
        Say "  exit $code, $([int]$sw.Elapsed.TotalSeconds) s: $detail"
        $checks += [ordered]@{ name = $c.name; ran_by = 'lane.ps1'; exit = $code; seconds = [int]$sw.Elapsed.TotalSeconds; detail = "$detail" }
        if ($code -ne 0) { $ok = $false }
    }
    if (@(GitIn $L.Path rev-parse HEAD)[0] -ne $sha -or @(Get-Dirty $L.Path).Count) { Fail 'the lane changed while it was verified: run verify again' }
    if (-not $ok) { Say "FAILED: no marker written (log: $log)"; exit 1 }
    $marker = [ordered]@{ lane = $Name; branch = $L.Branch; commit = $sha; main = @(GitIn $Main rev-parse $MainBranch)[0]; by = (Get-Owner)
                          time = (Get-Date -Format s); ok = $true; checks = @($checks) + @($declared); log = $log }
    $mf = Get-Marker $L.Path $sha
    [IO.File]::WriteAllText($mf, ($marker | ConvertTo-Json -Depth 5))
    Say "verified: $mf"
}

# ---------------------------------------------------------------- merge
function Merge-Lane {
    Assert-Name $Name
    $L = Get-Lane $Name; Assert-LanePath $L.Path; Assert-Agree $L
    if (Test-GitBusy $L.Path) { Fail 'a git operation is in progress in the lane' }
    $dirty = @(Get-Dirty $L.Path)
    if ($dirty.Count) { Fail "the lane has $($dirty.Count) uncommitted change(s)" }
    $head = (GitIn $Main symbolic-ref --quiet HEAD | Select-Object -First 1)
    if ($head -ne "refs/heads/$MainBranch") { Fail "the main checkout is not on $MainBranch ($head)" }
    if (Test-GitBusy $Main) { Fail 'a git operation is in progress in the main checkout' }
    $tip = @(GitIn $L.Path rev-parse HEAD)[0]
    $mainTip = @(GitIn $Main rev-parse $MainBranch)[0]
    $null = GitIn $Main merge-base --is-ancestor $MainBranch $tip
    if ($script:GitCode) { Fail "lane $Name is not rebased on main's tip $($mainTip.Substring(0, 8)): lane.ps1 sync $Name, then verify again" }
    if ($tip -eq $mainTip) { Fail "lane $Name has nothing main lacks" }
    $mf = Get-Marker $L.Path $tip
    if (-not (Test-Path -LiteralPath $mf)) { Fail "no verification marker for the tip $($tip.Substring(0, 8)) ($mf): lane.ps1 verify $Name" }
    $m = Get-Content -LiteralPath $mf -Raw | ConvertFrom-Json -DateKind String
    $need = 'unit', 'release', 'module_lint'
    $bad = @($need | Where-Object { $n = $_; -not ($m.checks | Where-Object { $_.name -eq $n -and $_.ran_by -eq 'lane.ps1' -and $_.exit -eq 0 }) })
    if ($m.commit -ne $tip -or -not $m.ok -or $bad.Count) { Fail "the marker $mf does not pass the tip $($tip.Substring(0, 8)) (commit $($m.commit), ok $($m.ok), missing / failed: $($bad -join ', '))" }
    $changed = @(GitIn $Main diff --name-only "$mainTip..$tip" | Where-Object { $_ })
    $mainDirty = @(Get-Dirty $Main | ForEach-Object { $_.Substring(3) -replace '^.* -> ', '' -replace '^"|"$', '' })
    $clash = @($changed | Where-Object { $mainDirty -contains $_ })
    if ($clash.Count) { Fail "the main checkout has uncommitted changes in file(s) the lane changed: $($clash -join ', ')" }
    $log = @(GitIn $Main log --oneline "$mainTip..$tip")
    Say "merge $Name -> ${MainBranch}: fast-forward $($mainTip.Substring(0, 8))..$($tip.Substring(0, 8)), $($log.Count) commit(s), $($changed.Count) file(s); verified by $($m.by) at $($m.time) ($(($m.checks | ForEach-Object { $_.name }) -join ', '))"
    foreach ($l in $log) { Say "  $l" }
    if ($WhatIf) { Say 'WhatIf: nothing merged'; return }
    $o = GitIn $Main merge --ff-only --quiet $tip
    if ($script:GitCode) { Say "FAILED: git merge --ff-only: $($o -join ' ')"; exit 1 }
    if (@(GitIn $Main rev-parse $MainBranch)[0] -ne $tip) { Say 'FAILED: main is not at the lane tip after the merge'; exit 1 }
    $r = Update-Registry {
        param($lanes)
        foreach ($l in $lanes) { if ($l.name -eq $Name) { $l | Add-Member -NotePropertyName merged -NotePropertyValue (@($l.merged | Where-Object { $_ }) + "$tip $(Get-Date -Format s)") -Force } }
        , $lanes
    }
    if ($r) { Fail $r }
    Say "main is at $($tip.Substring(0, 8)) (fast-forward; nothing pushed). The lane may go: lane.ps1 remove $Name"
}

# ---------------------------------------------------------------- remove
function Remove-Lane {
    Assert-Name $Name
    $L = Get-Lane $Name
    Assert-LanePath $L.Path
    $wt = $L.Path
    $finish = $false
    $plan = [Collections.Generic.List[string]]::new()
    if (-not $L.Wt -and $L.Exists) {
        # an interrupted removal (git worktree remove deleted part of the tree, then stopped): the registry says removing,
        # no .git link is left and no reparse point is inside - what is left is plain files git was already deleting
        $rest = @(Get-ReparsePoints $wt)
        if ($L.Reg -and $L.Reg.state -eq 'removing' -and -not (Test-Path -LiteralPath (Join-Path $wt '.git')) -and -not $rest.Count) {
            $finish = $true
            $plan.Add("finish the interrupted removal: delete what is left of $wt ($(@([IO.Directory]::EnumerateFiles($wt, '*', (New-WalkOptions))).Count) plain files, no reparse point, no .git)")
        } else { Fail "$wt exists but git worktree list does not know it: not touched (look at it by hand)" }
    }
    if ($L.Prunable -and -not $Force) {
        Fail "git lists a worktree at $wt but the folder is gone. If it was MOVED, its junctions still point into main: move it back and run remove again. If it was deleted, -Force prunes git's record"
    }
    if (-not $L.Wt -and -not $L.Reg -and -not $L.HasBranch -and -not $L.Prunable -and -not $finish) { Fail "there is no lane $Name" }
    $abortRebase = $false
    if ($L.Wt) {
        if ($L.Reg -and (Full $L.Reg.path) -ine $wt) { Fail "the registry puts lane $Name at $($L.Reg.path), not $wt" }
        if ($L.Wt.Branch -ne $L.Branch) {
            if ($Abandon -and (Test-GitBusy $wt) -like 'rebase*') { $abortRebase = $true; $plan.Add('abort the stopped rebase (the branch keeps its commits)') }
            else { Fail (Get-BranchAdvice $L) }
        }
        if (-not $L.Reg) { Say "WARNING: lane $Name has no registry entry (its instance unknown): only processes in the worktree are checked" }
    }
    # 1. commits main lacks
    $ahead = @()
    if ($L.HasBranch) { $ahead = @(GitIn $Main log --oneline "$MainBranch..$($L.Branch)" | Where-Object { $_ }) }
    if ($ahead.Count -and -not $Abandon) { Fail "lane $Name has $($ahead.Count) commit(s) main lacks (merge first, or -Abandon writes them as patches):`n  $($ahead -join "`n  ")" }
    $dirty = @(); $untracked = @()
    if ($L.Wt) {
        $busy = Test-GitBusy $wt
        if ($busy -and -not $Abandon) { Fail "a git operation is in progress in the lane ($busy)" }
        $dirty = @(Get-Dirty $wt)
        if ($dirty.Count -and -not $Abandon) { Fail "the lane has $($dirty.Count) uncommitted change(s) (commit and merge, or -Abandon saves them as a patch):`n  $($dirty[0..([Math]::Min(9, $dirty.Count - 1))] -join "`n  ")" }
        $untracked = @($dirty | Where-Object { $_.StartsWith('?? ') } | ForEach-Object { $_.Substring(3).Trim('"') -replace '/', '\' })
    }
    # 2. nothing runs from it
    if ($L.Reg) {
        $g = Get-InstanceGames "$($L.Reg.instance)"
        if ($g.Count) { Fail "a game runs on the lane's instance $($L.Reg.instance) (pid $($g[0].ProcessId), $($g[0].ExecutablePath)): close it first" }
    }
    if ($L.Wt) {
        $procs = Get-LaneProcesses $wt
        if ($procs.Count) { Fail "process(es) run from the lane: $(($procs | ForEach-Object { "$($_.Name) pid $($_.ProcessId)" }) -join ', ')" }
        if (Test-BuildRunning $wt) { Fail 'a build runs in the lane (its build lock is held)' }
    }
    # 3. what is in the worktree: reparse points, private files, locks
    $points = @(); $private = @(); $laneFiles = @(); $locked = @()
    if ($L.Wt) {
        $points = @(Get-ReparsePoints $wt)
        $tracked = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        foreach ($t in ((GitIn $wt ls-files -z) -join '') -split "`0") { if ($t) { $null = $tracked.Add(($t -replace '/', '\')) } }
        $skipUntracked = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        if ($Abandon) { foreach ($u in $untracked) { $null = $skipUntracked.Add($u) } }
        foreach ($f in [IO.Directory]::EnumerateFiles($wt, '*', (New-WalkOptions))) {
            $rel = $f.Substring($wt.Length + 1)
            if ($rel -eq '.git') { continue }
            # an open handle or a loaded image anywhere in the worktree stops git worktree remove half-way: find it first
            try {
                $ro = ([IO.File]::GetAttributes($f) -band [IO.FileAttributes]::ReadOnly)
                [IO.File]::Open($f, 'Open', $(if ($ro) { 'Read' } else { 'ReadWrite' }), 'None').Dispose()
            } catch { $locked += $rel }
            if ($tracked.Contains($rel) -or $rel -match $OutputRx) { continue }
            if ($rel -match '^\.lane\\') { $laneFiles += $rel; continue }
            if ($skipUntracked.Contains($rel)) { continue }
            $private += $rel
        }
    }
    if ($locked.Count) { Fail "$($locked.Count) file(s) in the lane are open or loaded elsewhere: $(($locked | Select-Object -First 10) -join ', ')" }
    if ($private.Count -and -not $KeepScratch -and -not $DiscardScratch) {
        Fail ("the lane holds $($private.Count) file(s) of its own that are neither tracked nor build output (scratch, evidence written inside the lane, ...). " +
              "Copy them out with -KeepScratch <folder>, or -DiscardScratch to delete them:`n  " + (($private | Select-Object -First 20) -join "`n  "))
    }
    if ($KeepScratch) {
        $ks = Full $KeepScratch
        if ($ks.StartsWith("$wt\", [StringComparison]::OrdinalIgnoreCase) -or $ks -ieq $wt) { Fail '-KeepScratch must lie outside the lane' }
    }
    # 4. the plan
    $targets = [ordered]@{}
    foreach ($p in $points) {
        $t = $p.LinkTarget
        $plan.Add("unlink $($p.FullName.Substring($wt.Length + 1)) -> $t ($($p.LinkType))")
        if ($t) {
            $tf = Full $t
            if (-not $tf.StartsWith("$wt\", [StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $tf -PathType Container)) { $targets[$tf] = $true }
        }
    }
    $stash = ''
    if ($Abandon -and ($ahead.Count -or $dirty.Count)) {
        $stash = Join-Path $Main "research\stash_patches\lane-$Name\$(Get-Date -Format yyyyMMdd_HHmmss)"
        $plan.Insert(0, "write $($ahead.Count) commit(s) as patches" + $(if ($dirty.Count) { " + the uncommitted changes ($($dirty.Count))" } else { '' }) + " to $stash")
    }
    if ($private.Count) { $plan.Add($(if ($KeepScratch) { "copy $($private.Count) private file(s) to $(Full $KeepScratch)" } else { "discard $($private.Count) private file(s)" })) }
    if ($laneFiles.Count) { $plan.Add("keep .lane\ ($($laneFiles.Count) file(s): markers, verify logs) in $Main\research\experience\lanes\") }
    if ($L.Wt) { $plan.Add("git worktree remove $wt  ({0:N2} GB, junctions not followed)" -f ((Get-TreeBytes $wt) / 1GB)) }
    if ($L.HasBranch -and -not $KeepBranch) { $plan.Add("git branch $(if ($Abandon) { '-D' } else { '-d' }) $($L.Branch)") }
    if ($L.Reg) { $plan.Add("free instance $($L.Reg.instance) in the registry") }
    Say "remove ${Name}:"
    foreach ($p in $plan) { Say "  $p" }
    if ($WhatIf) { Say 'WhatIf: nothing done'; return }

    # 5. no handle left anywhere under the worktree (a shell's current folder inside it, an open folder): Windows refuses
    # to rename a folder while anything below it is open, so a rename there and back proves git worktree remove will not
    # stop half-way. First of all, so a refusal here leaves the lane exactly as it was.
    if ($L.Wt -or $finish) {
        $probe = "$wt.lane-remove-check"
        if (Test-Path -LiteralPath $probe) { Say "FAILED: $probe exists (an earlier check's? look at it)"; exit 1 }
        try { [IO.Directory]::Move($wt, $probe) } catch { Say "FAILED: something holds a file or folder open inside $wt (a shell's current folder, an editor, Explorer): close it and run remove again ($($_.Exception.InnerException.Message))"; exit 1 }
        try { [IO.Directory]::Move($probe, $wt) } catch { Say "FAILED: the lane folder is now $probe and could not be renamed back: rename it to $wt by hand, then run remove again"; exit 1 }
    }
    if ($L.Reg) { Set-LaneField $Name 'state' 'removing' }
    # 6. keep what must be kept
    if ($stash) {
        $null = [IO.Directory]::CreateDirectory($stash)
        if ($ahead.Count) {
            $o = GitIn $Main format-patch --quiet -o $stash "$MainBranch..$($L.Branch)"
            if ($script:GitCode) { Say "FAILED: git format-patch: $($o -join ' ')"; exit 1 }
            if (@(Get-ChildItem -LiteralPath $stash -Filter '*.patch').Count -ne $ahead.Count) { Say "FAILED: $stash does not hold $($ahead.Count) patches"; exit 1 }
        }
        if ($dirty.Count) {
            # (--output: git writes the bytes itself - no stderr warning lines in it, line ends as they are)
            $o = GitIn $wt diff HEAD --binary "--output=$(Join-Path $stash 'uncommitted.patch')"
            if ($script:GitCode) { Say "FAILED: git diff: $($o -join ' ')"; exit 1 }
            foreach ($u in $untracked) {
                $src = Join-Path $wt $u
                if (Test-Path -LiteralPath $src -PathType Container) { $null = Copy-Tree $src (Join-Path $stash "untracked\$u") }
                elseif (Test-Path -LiteralPath $src) { $dst = Join-Path $stash "untracked\$u"; $null = [IO.Directory]::CreateDirectory((Split-Path $dst -Parent)); [IO.File]::Copy($src, $dst) }
            }
            $miss = @($untracked | Where-Object { -not (Test-Path -LiteralPath (Join-Path $stash "untracked\$_")) })
            if ($miss.Count) { Say "FAILED: $($miss.Count) untracked file(s) did not reach ${stash}: $($miss -join ', ')"; exit 1 }
        }
        Say "kept: $stash"
    }
    if ($private.Count -and $KeepScratch) {
        $ks = Full $KeepScratch
        foreach ($rel in $private) { $dst = Join-Path $ks $rel; $null = [IO.Directory]::CreateDirectory((Split-Path $dst -Parent)); [IO.File]::Copy((Join-Path $wt $rel), $dst, $true) }
        $miss = @($private | Where-Object { -not (Test-Path -LiteralPath (Join-Path $ks $_)) })
        if ($miss.Count) { Say "FAILED: $($miss.Count) private file(s) did not reach $ks"; exit 1 }
        Say "kept: $($private.Count) private file(s) in $ks"
    }
    if ($laneFiles.Count) {
        $keep = Join-Path $Main "research\experience\lanes\${Name}_$(Get-Date -Format yyyyMMdd_HHmmss)"
        $null = Copy-Tree (Join-Path $wt '.lane') $keep
        Say "kept: .lane\ in $keep"
    }
    # 7. every reparse point out, the link only; each target's file list before and after
    if ($points.Count) {
        $before = [ordered]@{}
        foreach ($t in $targets.Keys) { $before[$t] = Get-Snapshot $t }
        foreach ($p in $points) {
            if ($p -is [IO.DirectoryInfo]) { [IO.Directory]::Delete($p.FullName, $false) } else { [IO.File]::Delete($p.FullName) }
            if (Test-Path -LiteralPath $p.FullName) { Say "FAILED: $($p.FullName) is still there"; exit 1 }
        }
        $left = @(Get-ReparsePoints $wt)
        if ($left.Count) { Say "FAILED: reparse point(s) left in the lane: $(($left | ForEach-Object FullName) -join ', ')"; exit 1 }
        $lost = 0
        foreach ($t in $targets.Keys) {
            $after = Get-Snapshot $t
            $missing = @($before[$t] | Where-Object { -not $after.Contains($_) })
            Say ("  target {0}: {1} files before, {2} after, {3} missing" -f $t, $before[$t].Count, $after.Count, $missing.Count)
            foreach ($x in ($missing | Select-Object -First 20)) { Say "    MISSING $t$x" }
            $lost += $missing.Count
        }
        # A junction's removal (a non-recursive delete of the link) deletes no file in its target, so a few files gone in the seconds between the two
        # lists are other sessions' own deletions (13 Nov 2026: one file, while other lanes wrote in the shared evidence; the second call went
        # through): named above and let pass. Many gone at once is what a delete THROUGH a link would look like: stopped.
        $total = 0; foreach ($t in $targets.Keys) { $total += $before[$t].Count }
        if ($lost -gt 20 -or ($total -gt 0 -and $lost -gt 0.01 * $total)) { Say "FAILED: $lost file(s) are gone from the link targets (more than other sessions' own deletes would explain): stopped before git worktree remove; look first. The junctions are already out; running remove again goes on from here."; exit 1 }
        if ($lost) { Say "  note: $lost file(s) named above went from the shared folders meanwhile (another session's deletes: a link's removal deletes nothing behind it)" }
        Say "  $($points.Count) reparse point(s) removed (links only), $(if ($lost) { "targets intact but for the $lost file(s) named" } else { 'every target intact' })"
    }
    # 8. the worktree, the branch, the registry
    if ($finish) {
        if (@(Get-ReparsePoints $wt).Count) { Say "FAILED: a reparse point appeared in ${wt}: not deleted"; exit 1 }
        foreach ($f in [IO.Directory]::EnumerateFiles($wt, '*', (New-WalkOptions))) {   # (a read-only file stops a delete)
            $a = [IO.File]::GetAttributes($f)
            if ($a -band [IO.FileAttributes]::ReadOnly) { [IO.File]::SetAttributes($f, $a -bxor [IO.FileAttributes]::ReadOnly) }
        }
        [IO.Directory]::Delete($wt, $true)   # (checked just now: no reparse point anywhere below, so nothing outside it can go)
        Say "  the rest of the interrupted removal deleted"
    }
    if ($L.Wt) {
        if ($abortRebase) { $null = GitIn $wt rebase --abort }
        if ($Abandon -and (Test-GitBusy $wt)) { $null = GitIn $wt merge --abort; $null = GitIn $wt cherry-pick --abort; $null = GitIn $wt revert --abort }
        $args2 = @('worktree', 'remove')
        if ($Abandon) { $args2 += '--force' }   # (the uncommitted and untracked files were saved above; ignored files never need it)
        $o = GitIn $Main @args2 $wt
        if ($script:GitCode) { Say "FAILED: git worktree remove: $($o -join ' ') (no junction is left; fix the cause and run remove again)"; exit 1 }
        if (Test-Path -LiteralPath $wt) { Say "FAILED: $wt is still there after git worktree remove"; exit 1 }
    }
    $null = GitIn $Main worktree prune
    if ($L.HasBranch -and -not $KeepBranch) {
        $o = GitIn $Main branch $(if ($Abandon) { '-D' } else { '-d' }) $L.Branch
        if ($script:GitCode) { Say "FAILED: git branch delete: $($o -join ' ')"; exit 1 }
        Say "branch $($L.Branch) deleted ($($o -join ' '))"
    }
    $r = Update-Registry { param($lanes) , @($lanes | Where-Object { $_.name -ne $Name }) }
    if ($r) { Fail $r }
    Say ("lane $Name removed" + $(if ($L.Reg) { "; instance $($L.Reg.instance) free" } else { '' }))
}

# -WhatIf only where it is implemented (a `new -WhatIf` once made the lane: the breaker, 12 Nov 2026)
if ($WhatIf -and $Command -notin 'merge', 'remove') { Fail "-WhatIf is for merge and remove only; '$Command' would really run" }
# a relative -KeepScratch is the caller's folder's, not the process's
if ($KeepScratch) { $KeepScratch = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($KeepScratch) }
switch ($Command) {
    'new' { New-Lane }
    'adopt' { Add-Lane }
    'status' { Show-Status }
    'sync' { Sync-Lane }
    'verify' { Test-Lane }
    'merge' { Merge-Lane }
    'remove' { Remove-Lane }
    default { Fail "unknown command '$Command': new, status, sync, verify, merge, remove, adopt (the header of $PSCommandPath)" }
}
exit 0
