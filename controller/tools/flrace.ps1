# Race practice and lap-time benchmark (22 Oct 2026; user: racing is a general capability, M06's course its benchmark -
# memory racing-general, docs\LEGS.md): the free-roam fixture SaveC707 (Br05 at the M06 start point, the M06 br_elite) and
# `ap start pass <first ring>` - the autopilot flies through the ring and on along the course by its own look-ahead. The
# official order is M06's objective chain (read from a real race's log, 22 Oct 2026): a pass out of order is reported
# (the mission would not count it). Prints the splits and the lap (start -> the second crossing of the first ring).
# The game must run on the fixture (deploy.ps1 -Background -Restart -NoBuild -Instance n -Save <research\saves\legs\
# SaveC707.fl>); -Reload loads it again first (several laps in one game).
param([int]$Instance = 0, [switch]$Reload, [int]$TimeoutSec = 240, [string]$Label = 'm06-course')
if ($Instance -gt 0) { $env:FLVIBED_INSTANCE = "$Instance" }
$fl = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
. (Join-Path $PSScriptRoot 'flctl.ps1')
$course = [uint32[]]@(2949212877, 2949223117, 2299934144, 2412349133, 2949249741, 2412375757, 2949128909, 2412263117,
                      2836857280, 2299991488, 2843410890, 2836883904, 2300018112, 2836894144, 2306545098, 2300020160,
                      2843437514, 2836920768, 2306571722, 2300046784, 2836799936, 2949212877)
if ($Reload) { $null = Load-FLSave 'SaveC707.fl'; $null = Wait-FLSpace 30 }
$since = Get-FLEventSeq
$r = Send-FL "ap start pass $($course[0])"
if ($r -match '^pass:') { throw $r }
$t0 = Get-Date
# (state shared with the wait's condition block, which runs in its own scope)
$st = @{ next = 0; splits = [Collections.Generic.List[string]]::new(); wrong = 0; missed = 0; last = $t0; seen = 0 }
$null = Wait-FLUntil {
    $lines = @(Get-FLEventLog $since)
    for ($i = $st.seen; $i -lt $lines.Count; $i++) {
        $l = "$($lines[$i])"
        if ($l -match 'travel pass ring=(\d+) lateral=(\d+) speed=(\d+)') {
            if ([uint32]$Matches[1] -eq $course[$st.next]) {
                $now = Get-Date
                $st.splits.Add(('{0}:{1:N1}s/{2}m/{3}ms' -f $st.next, ($now - $st.last).TotalSeconds, $Matches[2], $Matches[3]))
                $st.last = $now; $st.next++
            } else { $st.wrong++ }
        }
        if ($l -match 'pass-missed') { $st.missed++ }
        # the mission's objective names the next gate; here the tool does: a wrong look-ahead is corrected at once
        if ($l -match 'travel pass next=(\d+)' -and $st.next -lt $course.Count -and [uint32]$Matches[1] -ne $course[$st.next]) {
            $null = Send-FL "ap start pass $($course[$st.next])"
        }
    }
    $st.seen = $lines.Count
    $st.next -ge $course.Count
} 'the lap' $TimeoutSec
$null = Send-FL 'ap stop'
$lap = ($st.last - $t0).TotalSeconds
$next = $st.next; $missed = $st.missed; $wrong = $st.wrong; $splits = $st.splits
"LAP {0:N1} s ({1} rings, {2} misses, {3} out of order) splits: {4}" -f $lap, $next, $missed, $wrong, ($splits -join ' ')
$hist = Join-Path $fl 'research\experience\legs\race.tsv'
if (-not (Test-Path $hist)) { Set-Content $hist "date`tlabel`tlap`trings`tmisses`twrong`tcommit" }
$commit = (git -C $fl rev-parse --short HEAD) + $(if (git -C $fl status --porcelain) { '+dirty' } else { '' })
Add-Content $hist ("{0:yyyy-MM-dd HH:mm}`t{1}`t{2:N1}`t{3}`t{4}`t{5}`t{6}" -f (Get-Date), $Label, $lap, $next, $missed, $wrong, $commit)
