# Prunes the test evidence's videos (the user, 24 Oct 2026: 40 GB, ~34 GB of it 5 fps session videos; "the prune option,
# with one notable exception: always keep full data for the fastest 3 runs of a specific leg"). Deletes only the .mkv
# video segments of research\experience\shots\<session> folders that belong to a PASSING leg run older than -OlderThanDays.
# Keeps: every screenshot (.jpg), every leg folder (research\experience\legs: logs, traces, a failure's end clip), every
# session of a failed leg, every session tied to no leg run (manual tests, probes, sweeps), and the full data of the 3
# fastest PASS runs of each leg (a leg = its mission id + start save, from legs.tsv). A session belongs to a leg run when
# it is the same instance's session started within 3 minutes after the leg's start (the leg folder's time stamp).
# -All (the user's option 3, 24 Oct 2026: "as long as you have the data on changes, autopilot behavior, the rest of the
# logged and sampled data"): also the videos of failed legs (their last 15 s stay in the leg folder) and of sessions tied
# to no leg run older than -OlderThanDays - still never a screenshot, a log or the top 3 per leg.
# Dry run by default (a report); -Apply deletes. Usage: tools\prune_evidence.ps1 [-OlderThanDays 3] [-All] [-Apply]
param([double]$OlderThanDays = 3, [switch]$All, [switch]$Apply)
$root = Split-Path $PSScriptRoot -Parent
$legsDir = Join-Path $root 'research\experience\legs'
$shotsDir = Join-Path $root 'research\experience\shots'
$cut = (Get-Date).AddDays(-$OlderThanDays)
# leg runs: folder yyyy-MM-dd_HHmm_<label>_i<n>, outcome / seconds / save from legs.tsv (date + label + instance)
$hist = @(Import-Csv (Join-Path $legsDir 'legs.tsv') -Delimiter "`t")
$runs = foreach ($d in Get-ChildItem $legsDir -Directory) {
    if ($d.Name -notmatch '^(\d{4}-\d\d-\d\d)_(\d\d)(\d\d)_(.+)_i(\d+)$') { continue }
    $start = [datetime]::ParseExact("$($Matches[1]) $($Matches[2]):$($Matches[3])", 'yyyy-MM-dd HH:mm', $null)
    $label = $Matches[4]; $inst = $Matches[5]
    # its legs.tsv row: same label and instance, the first row written after the start
    $row = $hist | Where-Object { $_.label -eq $label -and $_.instance -eq $inst -and [datetime]$_.date -ge $start } |
        Sort-Object { [datetime]$_.date } | Select-Object -First 1
    if (-not $row) { continue }
    $leg = if ($label -match '^(M\d\d[ab]?|L\d\d|FT\d+)') { $Matches[1] } else { $label }
    [pscustomobject]@{ Start = $start; Inst = $inst; Label = $label; Key = "$leg|$($row.save)"; Outcome = $row.outcome
                       Secs = [int]$row.secs; Folder = $d.Name; Session = $null }
}
# each run's screenshot session
$sessions = @(Get-ChildItem $shotsDir -Directory | Where-Object Name -match '^(\d{4}-\d\d-\d\d)_(\d{6})_i(\d+)$' | ForEach-Object {
    $null = $_.Name -match '^(\d{4}-\d\d-\d\d)_(\d{6})_i(\d+)$'
    [pscustomobject]@{ Dir = $_; Start = [datetime]::ParseExact("$($Matches[1]) $($Matches[2])", 'yyyy-MM-dd HHmmss', $null); Inst = $Matches[3] }
})
foreach ($r in $runs) {
    $s = $sessions | Where-Object { $_.Inst -eq $r.Inst -and $_.Start -ge $r.Start -and $_.Start -lt $r.Start.AddMinutes(3) } |
        Sort-Object Start | Select-Object -First 1
    if ($s) { $r.Session = $s.Dir }
}
# the 3 fastest PASS runs per leg: kept whole
$keep = @{}
foreach ($g in ($runs | Where-Object { $_.Outcome -eq 'PASS' -and $_.Session } | Group-Object Key)) {
    foreach ($r in ($g.Group | Sort-Object Secs | Select-Object -First 3)) { $keep[$r.Session.FullName] = "$($g.Name) $($r.Secs) s" }
}
$del = @(); $bytes = 0
$pruneDirs = @($runs | Where-Object { ($_.Outcome -eq 'PASS' -or $All) -and $_.Session -and $_.Start -lt $cut } |
    ForEach-Object { $_.Session.FullName })
if ($All) {
    $legSessions = @{}; foreach ($r in $runs) { if ($r.Session) { $legSessions[$r.Session.FullName] = 1 } }
    $pruneDirs += @($sessions | Where-Object { -not $legSessions.ContainsKey($_.Dir.FullName) -and $_.Start -lt $cut } |
        ForEach-Object { $_.Dir.FullName })
}
foreach ($dir in ($pruneDirs | Sort-Object -Unique)) {
    if ($keep.ContainsKey($dir)) { continue }
    foreach ($v in Get-ChildItem $dir -Filter '*.mkv' -File) { $del += $v; $bytes += $v.Length }
}
"runs: $(@($runs).Count) leg runs, $(@($runs | Where-Object Session).Count) with a session; legs kept whole (3 fastest PASS each): $($keep.Count) sessions"
$keep.GetEnumerator() | Sort-Object Value | ForEach-Object { "  keep  $($_.Value)  $(Split-Path $_.Key -Leaf)" }
"videos to delete ($(if ($All) { 'all sessions' } else { 'PASS runs' }) older than $OlderThanDays days, not top 3): $($del.Count) files, {0:N1} GB in {1} sessions" -f ($bytes / 1GB), @($del | ForEach-Object { $_.DirectoryName } | Sort-Object -Unique).Count
if ($Apply) {
    $del | Remove-Item -Force
    "deleted."
} else { "dry run - nothing deleted (-Apply deletes)" }
