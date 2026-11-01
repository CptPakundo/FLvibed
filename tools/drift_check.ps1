# The periodic drift / regression check (docs\TESTING.md "AFK sessions" 5; the user, 24 Oct 2026: "add a periodic check for
# drifts and regressions, so that your continuous testing and changes doesn't just loop infinitely without meaningful
# improvements"). One line per check, OK or ATTENTION, then the progress review of the target leg(s):
#   decisions   tools\decision_check.ps1 - the objective mode's script-goal decisions over every mission unchanged
#   kb          tools\kbgen.ps1 -Check - the generated kb tables match the install
#   replay      tools\splits.py replay - no good archived run fails the checkpoint limits (GOOD RUNS FAILED: 0)
#   settings    every `ap set` key the code accepts is documented in controller\docs\AUTOPILOT.md
#   settings-doc docs\SETTINGS.md matches a fresh generation from the registries (tools\settings_table.ps1 -Check)
#   ui-meta     core\ui_meta.h matches a fresh generation from docs\UI_SPEC.json (tools\ui_meta.py --check)
#   spec-copy   docs\spec\ (the static read-only living spec) matches a fresh export of tools\uimock (export.ps1 -Check)
#   checkpoints controller\src\impl\checkpoints_gen.h matches docs\checkpoints.tsv (tools\checkpoints_gen.py --check)
#   subtitle-review the review tool's unit tests (subtitles\review\FLSubtitles\asr\tests\run_tests.py, ~1 s: fit rules,
#               sentence joining vs the C++ JoinPieces fixtures, takes, saves, the publish copy rule)
#   tools       every script in tools\ is listed in tools\README.md
#   doc-refs    every backticked repository path in the tracked .md files exists (tools\docrefs.py)
#   doc-graph   every tracked .md is referred to by another doc (tools\docindex.py --orphans; the maps excepted)
#   handoff     HANDOFF.txt under 8,000 characters
#   claude-env  the Claude Code harness is installed as versioned (tools\claude\install.ps1 -Check) and its hook tests
#               pass (tools\claude\hooks\test_hooks.js)
#   ghidra      the Ghidra project's seed is today's core\flexe.h (tools\ghidra\ghidra.ps1 -Check; OK when not set up)
#   win7        the built release DLL, setup and server plugin import nothing Windows 7 lacks (tools\win7_check.ps1)
#   git         nothing uncommitted (else: a change not committed after it was finished)
#   backup      the newest bundle in the backup folder (tools\backup.ps1 -ShowDest) is under 4 h old
#   research    docs\research notes with watched changes since their last check-up (tools\research_check.ps1; a count), and headers grown a '; before:' chain
#   saves       no new run from an unfit checkpoint save after its verdict (docs\missions\scene_saves.tsv fit / fit_date,
#               the runs by the save's file name in research\experience\legs\legs.tsv; docs\TESTING.md "The test loop" 4):
#               ATTENTION for runs started since the previous review (else the last 24 h) - the slip is seen once and
#               the next review clears it, so it never stops the line for good; older ones and the count of unverified
#               leg-checkpoint saves are information
#   health      the working method's own trend (tools\claude\health\health.js --md, ~6 s; T35): the latest complete ISO week's
#               rates marked worse than the week before (failed calls, guard refusals / overrides per 100 calls, tokens
#               and $ per commit, $ per active hour, mean context); ERROR (not counted) when health.js fails
#   overrides   guard-ok overrides per rule in ..\.claude\guard_fires.tsv over the last 24 h, or since the previous review
#               when that is longer ago (a re-run never shrinks the window), the hook tests' sessions left out (ids
#               'test...', 't<digit>...', empty); 3+ of one rule = ATTENTION (an eroding rule: fix the cause or prune the
#               rule - tools\claude\README.md)
#   leg-bests   docs\LEGS.md's best full legs match legs.tsv by splits.py full_pass_row (splits.py bestcheck)
#   reader-fixtures  the legs.tsv readers classify tools\fixtures\reader_rows.tsv as expected (tools\reader_fixtures.py)
#   progress    tools\splits.py progress <leg>: NO PROGRESS / UNRELIABLE flags
# Each check runs isolated: one that throws (a missing tool, a failed child script) reports "<check> ERROR <message>"
# (counted) and the review goes on, so a broken check never skips the stop-the-line ones.
# After the checks it writes ..\.claude\drift_last.txt (environment folder; local) - the one machine-written record of the
# last review, read by the command guard while AFK (tools\claude\hooks\guard.js review-due: a set or build needs a
# finished review under 2 h old; stop-the-line: no set while decisions / replay / saves show ATTENTION or ERROR).
# Tab-separated lines: "time <UTC ISO>", one "<check> OK|ATTENTION|ERROR <detail>" per check ("progress:<leg>" per
# target leg), "legs <list>", "result <summary>", "secs <run time>". A review stopped by an error outside the checks
# writes "result INCOMPLETE: <error>", which the guard treats as no review.
# Usage (PowerShell): tools\drift_check.ps1 [-Leg M12[,M04]] [-Since 'YYYY-MM-DD HH:MM']   exit 1 when any check needs attention
param([string[]]$Leg = @(), [string]$Since = '')
$root = Split-Path $PSScriptRoot -Parent
$here = $PSScriptRoot
$py = Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe'
$envClaude = Join-Path (Split-Path $root -Parent) '.claude'
$driftLast = if ($env:FLV_DRIFT_LAST) { $env:FLV_DRIFT_LAST } else { Join-Path $envClaude 'drift_last.txt' }   # (tests)
$t0 = Get-Date
$bad = 0
$rows = [System.Collections.Generic.List[string]]::new()
# $state: a word other than OK / ATTENTION (ERROR: the check could not run; counted only when -not $ok)
function Report([string]$name, [bool]$ok, [string]$detail, [string]$state = '', [string]$key = '') {
    $script:bad += [int](-not $ok)
    $s = if ($state) { $state } elseif ($ok) { 'OK' } else { 'ATTENTION' }
    $script:rows.Add("$(if ($key) { $key } else { $name })`t$s`t$($detail -replace '[\t\r\n]+', ' ')")
    "{0,-10} {1,-9} {2}" -f $name, $s, $detail
}
# one isolated check: an error inside it is that check's ERROR row (counted), never the end of the review
function Invoke-Check([string]$name, [scriptblock]$body, [string]$key = '') {
    try { & $body } catch { Report $name $false "check failed: $($_.Exception.Message)" 'ERROR' $key }
}
# the previous review's time (the overrides and saves windows)
$prevReview = $null
if (Test-Path $driftLast) {
    $tl = Get-Content $driftLast -TotalCount 3 | Where-Object { $_ -match "^time`t(.+)$" } | Select-Object -First 1
    $pv = [DateTimeOffset]::MinValue
    if ($tl -and [DateTimeOffset]::TryParse(($tl -split "`t")[1], [Globalization.CultureInfo]::InvariantCulture,
            [Globalization.DateTimeStyles]::AssumeUniversal, [ref]$pv)) { $prevReview = $pv }
}
$legs = @($Leg | ForEach-Object { $_ -split ',' } | Where-Object { $_ })
$done = $false
$abort = ''
Push-Location $root
try {
    Invoke-Check 'decisions' {
        $d = @(& (Join-Path $here 'decision_check.ps1') 2>&1 | ForEach-Object { "$_" })
        Report 'decisions' ($LASTEXITCODE -eq 0 -and ($d -join ' ') -match 'unchanged') ($d | Select-Object -First 1)
    }
    Invoke-Check 'kb' {
        $k = @(& (Join-Path $here 'kbgen.ps1') -Check 2>&1 | ForEach-Object { "$_" })
        Report 'kb' (($k -join ' ') -match 'current') ($k | Select-Object -Last 1)
    }
    Invoke-Check 'replay' {
        $r = "$(& $py -I (Join-Path $here 'splits.py') replay 2>&1 | Select-String 'GOOD RUNS FAILED')"
        Report 'replay' ($r -match 'GOOD RUNS FAILED: 0') $r.Trim()
    }
    Invoke-Check 'settings' {
        # the registry rows `{"name", Type, ...` (core\settings.h) plus the two hand-written word keys `k == "..."`
        $keys = Select-String -Path (Join-Path $root 'controller\src\impl\autopilot_settings.cpp') -Pattern '(?:\{"|k == ")([a-z_0-9.]+)"(?:, (?:Bool|Int|Float|Hex|String|Vec3)\b|\))' -AllMatches |
                ForEach-Object { $_.Matches } | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
        $doc = Get-Content (Join-Path $root 'controller\docs\AUTOPILOT.md') -Raw
        $miss = @($keys | Where-Object { $doc -notmatch "\b$([regex]::Escape($_))\b" })
        Report 'settings' ($miss.Count -eq 0) "$($keys.Count) keys$(if ($miss) { '; undocumented: ' + ($miss -join ', ') })"
    }
    Invoke-Check 'settings-doc' {
        $sd = @(& (Join-Path $here 'settings_table.ps1') -Check 2>&1 | ForEach-Object { "$_" })
        Report 'settings-doc' ($LASTEXITCODE -eq 0) ($sd | Select-Object -Last 1)
    }
    Invoke-Check 'ui-meta' {
        $um = "$(& $py -I (Join-Path $here 'ui_meta.py') --check 2>&1 | Select-Object -Last 1)"
        Report 'ui-meta' ($LASTEXITCODE -eq 0) $um.Trim()
    }
    Invoke-Check 'signatures' {
        # the signature table (core\flexe.h, as last built into sigtool): every symbol found by signature alone in the test
        # install's exe, none with padding bytes as literals (FREELANCER_RE.md "Signatures"; 28 had them until 29 Oct 2026),
        # none over a detour's first 10 bytes (sigtool HOOK-OVERLAP: the logic resolves after the host's detours, 30 Oct 2026)
        $sig = Join-Path $root 'build\sigtool\sigtool.exe'
        $tExe = Join-Path $root 'Install\Freelancer HD Edition\EXE\Freelancer.exe'
        if ((Test-Path $sig) -and (Test-Path $tExe)) {
            $so = @(& $sig check $tExe --scan 2>&1 | ForEach-Object { "$_" })
            $sx = (Get-Item $sig).LastWriteTime -lt (Get-Item (Join-Path $root 'core\flexe.h')).LastWriteTime
            Report 'signatures' ($LASTEXITCODE -eq 0 -and -not $sx) ("$(($so | Select-String 'expected addresses') -replace 'expected addresses: ', '')" +
                "; $(($so | Select-String 'across padding')); $(($so | Select-String 'detour''s first bytes'))$(if ($sx) { '; sigtool older than flexe.h: build.cmd sigtool' })")
        } else { Report 'signatures' $false 'sigtool or the test install missing (build.cmd sigtool)' }
    }
    Invoke-Check 'spec-copy' {
        # collect the whole output first: Select-Object -First 1 in the pipe would stop export.ps1 before its `exit 1`
        # (and $LASTEXITCODE would keep the previous check's 0 - a stale copy reported OK, 26 Oct 2026)
        $sco = @(& (Join-Path $here 'uimock\export.ps1') -Check 2>&1 | ForEach-Object { "$_" })
        $sc = "$($sco | Select-Object -First 1)"
        Report 'spec-copy' ($LASTEXITCODE -eq 0) $sc.Trim()
    }
    Invoke-Check 'checkpoints' {
        $cg = "$(& $py -I (Join-Path $here 'checkpoints_gen.py') --check 2>&1 | Select-Object -Last 1)"
        Report 'checkpoints' ($LASTEXITCODE -eq 0) $cg.Trim()
    }
    Invoke-Check 'subtitle-review' {
        $srt = @(& $py -I (Join-Path $root 'subtitles\review\FLSubtitles\asr\tests\run_tests.py') 2>&1 | ForEach-Object { "$_" })
        $srtLast = "$($srt | Where-Object { $_ -match '^(Ran |OK|FAILED)' } | Select-Object -Last 2)"
        Report 'subtitle-review' ($LASTEXITCODE -eq 0) $srtLast.Trim()
    }
    Invoke-Check 'doc-refs' {
        $dr = "$(& $py -I (Join-Path $here 'docrefs.py') 2>&1 | Select-Object -Last 1)"
        Report 'doc-refs' ($LASTEXITCODE -eq 0) $dr.Trim()
    }
    Invoke-Check 'doc-graph' {
        $dg = "$(& $py -I (Join-Path $here 'docindex.py') --orphans 2>&1 | Select-Object -Last 1)"
        Report 'doc-graph' ($LASTEXITCODE -eq 0) $dg.Trim()
    }
    Invoke-Check 'tools' {
        $readme = Get-Content (Join-Path $here 'README.md') -Raw
        $tools = Get-ChildItem $here -File | Where-Object { $_.Extension -in '.ps1', '.py', '.js' } | ForEach-Object Name
        $unlisted = @($tools | Where-Object { $readme -notmatch [regex]::Escape($_) })
        Report 'tools' ($unlisted.Count -eq 0) "$($tools.Count) scripts$(if ($unlisted) { '; not in README: ' + ($unlisted -join ', ') })"
    }
    Invoke-Check 'handoff' {
        $hs = (Get-Item (Join-Path $root 'HANDOFF.txt') -ErrorAction Stop).Length
        Report 'handoff' ($hs -lt 8000) "$hs bytes"
    }
    Invoke-Check 'claude-env' {
        # the Claude Code harness (tools\claude\README.md): installed as versioned, and its hooks' own tests pass
        $ce = "$(& (Join-Path $here 'claude\install.ps1') -Check 2>&1 | Select-Object -Last 1)"
        $ceOk = $LASTEXITCODE -eq 0
        $ht = @(& node (Join-Path $here 'claude\hooks\test_hooks.js') 2>&1 | ForEach-Object { "$_" })
        Report 'claude-env' ($ceOk -and $LASTEXITCODE -eq 0) ("$($ce.Trim()); " + (($ht | Where-Object { $_ -match 'cases:' }) -join ', '))
    }
    Invoke-Check 'ghidra' {
        # the Ghidra project (tools\ghidra\README.md; optional): seeded with today's core\flexe.h
        $gh = "$(& (Join-Path $here 'ghidra\ghidra.ps1') -Check 2>&1 | Select-Object -Last 1)"
        Report 'ghidra' ($LASTEXITCODE -eq 0) $gh.Trim()
    }
    Invoke-Check 'win7' {
        # Windows 7: the built binaries import nothing the system lacks (tools\win7_check.ps1; OK when nothing is built)
        $w7 = @(& (Join-Path $here 'win7_check.ps1') 2>&1 | ForEach-Object { "$_" })
        Report 'win7' ($LASTEXITCODE -eq 0) (($w7 | Select-Object -First 2) -join ' ')
    }
    Invoke-Check 'git' {
        $dirty = @(git status --porcelain)
        if ($LASTEXITCODE -ne 0) { throw "git status exit $LASTEXITCODE" }
        Report 'git' ($dirty.Count -eq 0) "$($dirty.Count) uncommitted path(s)$(if ($dirty) { ': ' + (($dirty | Select-Object -First 4) -join '; ') })"
    }
    Invoke-Check 'backup' {
        $bkDir = & (Join-Path $here 'backup.ps1') -ShowDest
        $bk = if ($bkDir) { Get-ChildItem $bkDir -Filter 'FLvibed_*.bundle' -ErrorAction SilentlyContinue |
              Sort-Object LastWriteTime -Descending | Select-Object -First 1 }
        $age = if ($bk) { [math]::Round(((Get-Date) - $bk.LastWriteTime).TotalHours, 1) } else { 999 }
        Report 'backup' ($age -lt 4) "newest $(if ($bk) { $bk.Name } else { 'none' }), $age h old"
    }
    Invoke-Check 'research' {
        $rc = @(& (Join-Path $here 'research_check.ps1') *>&1 | ForEach-Object { "$_" }) # (it writes with Write-Host)
        $stale = @($rc | Where-Object { $_ -match 'watched paths changed in' }).Count
        $chain = @($rc | Where-Object { $_ -match '^\s+HEADER CHAIN:' }).Count # (the flag line itself, not a listed commit subject)
        Report 'research' ($stale -eq 0 -and $chain -eq 0) "$stale research note(s) with watched changes since their check-up, $chain header(s) with a '; before:' chain (tools\research_check.ps1)"
    }
    Invoke-Check 'saves' {
        # a run counts when it started (its legs.tsv end time - secs - boot) over 2 min after the verdict: not the run that
        # judged it; ATTENTION only for the ones started since the previous review (else the last 24 h), so the line clears
        $win = if ($prevReview) { $prevReview.LocalDateTime } else { (Get-Date).AddHours(-24) }
        $sv = @(Import-Csv (Join-Path $root 'docs\missions\scene_saves.tsv') -Delimiter "`t" | Where-Object { $_.source -eq 'leg-checkpoint' })
        $lh = Join-Path $root 'research\experience\legs\legs.tsv'
        $runs = if (Test-Path $lh) { @(Import-Csv $lh -Delimiter "`t") } else { @() }
        $unfit = @($sv | Where-Object { $_.fit -eq 'unfit' })
        $since = @(foreach ($u in $unfit) {
            $v = [datetime]::MinValue
            $starts = @(if ([datetime]::TryParse("$($u.fit_date)", [ref]$v)) {
                foreach ($x in @($runs | Where-Object { $_.save -eq $u.name -and $_.date })) {
                    $end = [datetime]::MinValue; $rs = 0; $rb = 0
                    if (-not [datetime]::TryParse("$($x.date)", [ref]$end)) { continue }
                    [void][int]::TryParse("$($x.secs)", [ref]$rs); [void][int]::TryParse("$($x.boot)", [ref]$rb)
                    $st = $end.AddSeconds(-($rs + $rb))
                    if ($st -gt $v.AddMinutes(2)) { $st }
                } })
            [pscustomobject]@{ save = "$($u.mission) $($u.step) $($u.name)"; n = $starts.Count
                new = @($starts | Where-Object { $_ -gt $win }).Count }
        })
        $new = @($since | Where-Object { $_.new -gt 0 })
        $old = @($since | Where-Object { $_.n -gt $_.new })
        $unv = @($sv | Where-Object { $_.fit -notin 'fit', 'unfit' }).Count
        Report 'saves' ($new.Count -eq 0) ("$($unfit.Count) unfit checkpoint save(s)" +
            $(if ($new) { "; run from after the verdict since $($win.ToString('yyyy-MM-dd HH:mm')): " +
                (($new | ForEach-Object { "$($_.save) $($_.new)x" }) -join ', ') + ' (their verdicts do not count)' } else { '' }) +
            $(if ($old) { '; earlier (seen): ' + (($old | ForEach-Object { "$($_.save) $($_.n - $_.new)x" }) -join ', ') } else { '' }) +
            "; $unv unverified (their first run judges them)")
    }
    # the working method's trend (health.js --md; its TSV goes to a temp file: this check writes nothing else). The ISO
    # week of today is incomplete: compare the latest complete week with the one before it, the table's (w) marks
    $hT = Get-Date
    $hTmp = Join-Path ([IO.Path]::GetTempPath()) "flv-drift-health-$PID.tsv"
    try {
        $ho = @(& node (Join-Path $here 'claude\health\health.js') --md --out $hTmp 2>$null | ForEach-Object { "$_" })
        $hx = $LASTEXITCODE
        $now = Get-Date
        $thu = $now.Date.AddDays(3 - (([int]$now.DayOfWeek + 6) % 7))                 # the ISO week's Thursday decides it
        $thisWeek = '{0}-W{1:00}' -f $thu.Year, ([math]::Floor(($thu.DayOfYear - 1) / 7) + 1)
        $head = $ho | Where-Object { $_ -match '^\| week \|' } | Select-Object -First 1
        $weeks = @($ho | Where-Object { $_ -match '^\| \d{4}-W\d\d \|' } | Where-Object { ($_ -split '\|')[1].Trim() -lt $thisWeek })
        $hSecs = [math]::Round(((Get-Date) - $hT).TotalSeconds, 1)
        if (-not $head -or $hx -notin 0, 2) {
            Report 'health' $true "not measured: health.js exit $hx$(if (-not $head) { ', no week table' }) ($hSecs s)" 'ERROR'
        } elseif ($weeks.Count -lt 2) {
            Report 'health' $true "fewer than two complete ISO weeks before $thisWeek ($hSecs s)"
        } else {
            $cols = @(($head -split '\|') | ForEach-Object { $_.Trim() })
            $cur = @(($weeks[-1] -split '\|') | ForEach-Object { $_.Trim() })
            $pre = @(($weeks[-2] -split '\|') | ForEach-Object { $_.Trim() })
            $worse = @(for ($i = 1; $i -lt $cols.Count; $i++) {
                if ($cur[$i] -match '^(\S+) \(w\)$') { "$($cols[$i]) $(($pre[$i] -split ' ')[0]) -> $($Matches[1])" } })
            Report 'health' ($worse.Count -eq 0) ("$($cur[1]) vs $($pre[1]): " + $(if ($worse) { 'worse ' + ($worse -join ', ') } else { 'no rate worse' }) +
                " (tools\claude\health\health.js --md; $hSecs s)")
        }
    } catch { Report 'health' $true "not measured: $($_.Exception.Message)" 'ERROR' }
    finally { if (Test-Path $hTmp) { [System.IO.File]::Delete($hTmp) } }
    Invoke-Check 'leg-bests' {
        # docs\LEGS.md's best full legs = legs.tsv's by the one definition (splits.py full_pass_row; WORK_QUEUE H10 #13:
        # a stop-at run was taken for a leg's best twice, 78752d8 / d9f445a)
        $o = @(& $py -I (Join-Path $root 'tools\splits.py') bestcheck 2>&1 | ForEach-Object { "$_" })
        Report 'leg-bests' ($LASTEXITCODE -eq 0) ($o -join ' ')
    }
    Invoke-Check 'reader-fixtures' {
        # the legs.tsv readers against tools\fixtures\reader_rows.tsv (audit D5, 31 Oct 2026: ~9 harness bugs from
        # readers misreading the data): splits.py's row classes + legtable.js / flgate.ps1 still holding its three tests
        $o = @(& $py -I (Join-Path $root 'tools\reader_fixtures.py') 2>&1 | ForEach-Object { "$_" })
        Report 'reader-fixtures' ($LASTEXITCODE -eq 0) ($o -join ' ')
    }
    Invoke-Check 'ratchets' {
        # numbers that may only get better unless a commit raises them with a reason (WORK_QUEUE H4, IDEAS T13; 30 Oct
        # 2026): docs\ratchets.tsv. ATTENTION when a value grew more than its max_growth over the row, a note when it
        # shrank 5 %+ (tighten the row). release_dll_bytes = build\release\FLvibed.dll as last built; doc_bytes:<path> =
        # that file's size (path from FLvibed\; IDEAS T13's doc size budgets: the always-loaded maps, 30 Oct 2026).
        $rt = Join-Path $root 'docs\ratchets.tsv'
        $msgs = @(); $okAll = $true
        foreach ($r in @(Import-Csv $rt -Delimiter "`t")) {
            $now = switch -Regex ($r.name) {
                '^release_dll_bytes$' { $f = Join-Path $root 'build\release\FLvibed.dll'; if (Test-Path $f) { (Get-Item $f).Length } else { $null } }
                '^doc_bytes:(.+)$' { $f = Join-Path $root $Matches[1]; if (Test-Path $f) { (Get-Item $f).Length } else { $null } }
                default { $null }
            }
            if ($null -eq $now) { $msgs += "$($r.name): not measured"; continue }
            $lim = [double]$r.value * (1 + [double]$r.max_growth)
            $pct = 100.0 * ($now - [double]$r.value) / [double]$r.value
            if ($now -gt $lim) { $okAll = $false; $msgs += ("{0} {1:N0} = {2:+0.0;-0.0} % over the ratchet {3:N0} (limit +{4:P0}): bring it back or raise the row with a reason" -f $r.name, $now, $pct, [double]$r.value, [double]$r.max_growth) }
            elseif ($pct -le -5) { $msgs += ("{0} {1:N0} ({2:0.0} %): tighten the row" -f $r.name, $now, $pct) }
            else { $msgs += ("{0} {1:N0} ({2:+0.0;-0.0} %)" -f $r.name, $now, $pct) }
        }
        Report 'ratchets' $okAll (($msgs -join '; ') + ' (docs\ratchets.tsv)')
    }
    Invoke-Check 'testing' {
        # the in-game testing method's own trend (WORK_QUEUE H15, 30 Oct 2026): docs\test_budget.tsv's latest ISO week
        # through health.js's testingTrend - ATTENTION when the runs used exceed 1.5x the stated budgets or more than one
        # harness bug per package was met (Working method 23: as much testing as needed, no more)
        $hj = Join-Path $root 'tools\claude\health\health.js'
        $bt = Join-Path $root 'docs\test_budget.tsv'
        $js = "const H=require(process.argv[1]);const r=H.testingRows(require('fs').readFileSync(process.argv[2],'utf8'));" +
              "console.log(H.testingTrend(r).filter(l=>/^\| \d{4}-W/.test(l)).pop()||'')"
        $w = "$(& node -e $js $hj $bt 2>&1)".Trim()
        if ($w -notmatch '^\|') { Report 'testing' $true "no package rows in docs\test_budget.tsv" ; return }
        $c = @(($w -split '\|') | ForEach-Object { $_.Trim() })
        $ratio = [double](($c[5] -split ' ')[0] -replace '^-$', '0'); $bugs = [double](($c[6] -split ' ')[0])
        Report 'testing' ($ratio -le 1.5 -and $bugs -le 1) ("$($c[1]): $($c[2]) package(s), used / budget $($c[5]), harness bugs / package " +
            "$($c[6]), wasted runs / package $($c[8]) (docs\test_budget.tsv, H15)")
    }
    Invoke-Check 'overrides' {
        # guard-ok overrides per rule over the last 24 h, or since the previous review when that is longer ago (a re-run
        # never shrinks the window and wipes the signal), the hook tests' sessions left out
        $gf = Join-Path $envClaude 'guard_fires.tsv'
        $day = [DateTimeOffset]::UtcNow.AddHours(-24)
        $from = if ($prevReview -and $prevReview -lt $day) { $prevReview } else { $day }
        $ov = @(if (Test-Path $gf) { Get-Content $gf | ForEach-Object {
            $f = $_ -split "`t"
            $ft = [DateTimeOffset]::MinValue
            if ($f.Count -ge 4 -and $f[3] -eq 'override' -and $f[1] -and $f[1] -notmatch '^(test|t\d)' -and
                [DateTimeOffset]::TryParse($f[0], [Globalization.CultureInfo]::InvariantCulture,
                    [Globalization.DateTimeStyles]::AssumeUniversal, [ref]$ft) -and $ft -gt $from) {
                [pscustomobject]@{ rule = $f[2]; recent = [bool]($prevReview -and $ft -gt $prevReview) } } } })
        $og = @($ov | Group-Object rule | Sort-Object Count -Descending)
        $eroded = @($og | Where-Object { $_.Count -ge 3 })
        Report 'overrides' ($eroded.Count -eq 0) ("since $($from.ToUniversalTime().ToString('yyyy-MM-dd HH:mm')) UTC " +
            "($(if ($from -eq $day) { 'the last 24 h' } else { 'the previous review' })): " +
            $(if ($og) { ($og | ForEach-Object { $nr = @($_.Group | Where-Object recent).Count
                "$($_.Name) $($_.Count)$(if ($prevReview) { " ($nr since the previous review)" })" }) -join ', ' } else { 'none' }) +
            $(if ($eroded) { '; 3+ of one rule: fix the cause or prune the rule (tools\claude\README.md)' } else { '' }))
    }
    foreach ($l in $legs) {
        Invoke-Check 'progress' {
            $pa = @('progress', $l) + $(if ($Since) { @('--since', $Since) } else { @() })
            $p = @(& $py -I (Join-Path $here 'splits.py') @pa 2>&1 | ForEach-Object { "$_" })
            $last = "$($p | Select-Object -Last 1)".Trim()
            $unrel = @($p | Select-Object -Last 4 | Where-Object { $_ -match 'UNRELIABLE' }).Count
            Report "progress" ($last -notmatch 'NO PROGRESS' -and $unrel -eq 0) "${l}: $last$(if ($unrel) { "; $unrel of the last 3 sets UNRELIABLE" })" '' "progress:$l"
        } "progress:$l"
    }
    $done = $true
} catch { $abort = "$($_.Exception.Message)" }
finally { Pop-Location }
if (-not $done) { $bad++ }
$summary = if (-not $done) { "INCOMPLETE: $abort" } elseif ($bad) { "$bad check(s) need attention" } else { 'all checks OK' }
$summary
# the review's record for the guards (review-due, stop-the-line); an INCOMPLETE result counts there as no review
$secs = [math]::Round(((Get-Date) - $t0).TotalSeconds)
$text = (@("time`t$([DateTimeOffset]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ'))") + $rows +
    @("legs`t$($legs -join ',')", "result`t$($summary -replace '[\t\r\n]+', ' ')", "secs`t$secs")) -join "`n"
try { [System.IO.File]::WriteAllText($driftLast, $text + "`n") } catch { "drift_last.txt not written: $($_.Exception.Message)" }
exit [int]($bad -gt 0)
