# Records how a test game ended: started by deploy.ps1 for every game it starts (detached, hidden, broken away from the
# caller's job where Windows allows it, so it outlives a stopped Claude task), it holds a handle on the game, waits for
# it to end and appends ONE line to the instance's EXE\logs\game_exits.log, then ends itself. Nothing else: no polling,
# no window, no file but that line. Why: Freelancer's own crash filter ends a crashed game with no Windows event and no
# dump, so a "silent exit" was unexplained until its exit code was known (audit 95_silent_exits.md, 13 Nov 2026).
# The line (tab-separated): <local exit time, ms> instance <n|main> pid <pid> build <FLvibed.build.txt> exit 0x<code>
#   lived <s> s session <deploy stamp>
# Codes seen on this PC (docs\TESTING.md "A silent exit"): 0xFFFFFFFF Stop-Process (every test's clean-up), 0x00000001
# a Claude task's process tree stopped, 0xC0000409 fail-fast, other 0xC00000xx a crash, 0x00000000 the game's own quit.
param([Parameter(Mandatory)][int]$GamePid, [Parameter(Mandatory)][long]$StartUtcTicks, [Parameter(Mandatory)][string]$Log,
      [string]$Instance = 'main', [string]$Build = '?', [string]$Session = '')
$ErrorActionPreference = 'Stop'
function Write-ExitLine([string]$when, [string]$rest) {
    $line = "$when`tinstance $Instance`tpid $GamePid`tbuild $Build`t$rest`tsession $Session"
    $null = [IO.Directory]::CreateDirectory((Split-Path $Log -Parent))
    # (another game's watcher may append at the same moment: retry a shared-write collision for up to 5 s)
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        try {
            $s = [IO.File]::Open($Log, 'Append', 'Write', 'Read')
            try { $b = [Text.Encoding]::UTF8.GetBytes($line + "`r`n"); $s.Write($b, 0, $b.Length) } finally { $s.Dispose() }
            return
        } catch [IO.IOException] { if ($sw.Elapsed.TotalSeconds -gt 5) { throw }; Start-Sleep -Milliseconds 50 }
    }
}
$fmt = 'yyyy-MM-dd HH:mm:ss.fff'
try {
    $p = [Diagnostics.Process]::GetProcessById($GamePid)
    $null = $p.Handle   # (the handle is what keeps the exit code readable after the process is gone)
} catch {
    Write-ExitLine (Get-Date -Format $fmt) 'exit unknown (the game was gone before its watcher opened it)'
    exit 0
}
# the same process, not a later one that got its pid
if ([Math]::Abs($p.StartTime.ToUniversalTime().Ticks - $StartUtcTicks) -gt [TimeSpan]::TicksPerSecond) {
    Write-ExitLine (Get-Date -Format $fmt) "exit unknown (pid $GamePid now belongs to a process started $($p.StartTime.ToString($fmt)))"
    exit 0
}
$p.WaitForExit()
$lived = ($p.ExitTime - $p.StartTime).TotalSeconds
Write-ExitLine $p.ExitTime.ToString($fmt) ("exit 0x{0:X8}`tlived {1:F1} s" -f $p.ExitCode, $lived)
exit 0
