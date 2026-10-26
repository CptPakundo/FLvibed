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
#   handoff     HANDOFF.txt under 8,000 characters
#   git         nothing uncommitted (else: a change not committed after it was finished)
#   backup      the newest bundle in the backup folder (tools\backup.ps1 -ShowDest) is under 4 h old
#   research    docs\research notes with watched changes since their last check-up (tools\research_check.ps1; a count)
#   saves       no run from an unfit checkpoint save after its verdict (docs\missions\scene_saves.tsv fit / fit_date, the
#               runs by the save's file name in research\experience\legs\legs.tsv; docs\TESTING.md "The test loop" 4);
#               plus the count of unverified leg-checkpoint saves
#   progress    tools\splits.py progress <leg>: NO PROGRESS / UNRELIABLE flags
# Usage (PowerShell): tools\drift_check.ps1 [-Leg M12[,M04]] [-Since 'YYYY-MM-DD HH:MM']   exit 1 when any check needs attention
param([string[]]$Leg = @(), [string]$Since = '')
$root = Split-Path $PSScriptRoot -Parent
$py = Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe'
$bad = 0
function Report([string]$name, [bool]$ok, [string]$detail) {
    $script:bad += [int](-not $ok)
    "{0,-10} {1,-9} {2}" -f $name, $(if ($ok) { 'OK' } else { 'ATTENTION' }), $detail
}
Push-Location $root
try {
    $d = @(& (Join-Path $PSScriptRoot 'decision_check.ps1') 2>&1 | ForEach-Object { "$_" })
    Report 'decisions' ($LASTEXITCODE -eq 0 -and ($d -join ' ') -match 'unchanged') ($d | Select-Object -First 1)
    $k = @(& (Join-Path $PSScriptRoot 'kbgen.ps1') -Check 2>&1 | ForEach-Object { "$_" })
    Report 'kb' (($k -join ' ') -match 'current') ($k | Select-Object -Last 1)
    $r = "$(& $py -I (Join-Path $PSScriptRoot 'splits.py') replay 2>&1 | Select-String 'GOOD RUNS FAILED')"
    Report 'replay' ($r -match 'GOOD RUNS FAILED: 0') $r.Trim()
    # the registry rows `{"name", Type, ...` (core\settings.h) plus the two hand-written word keys `k == "..."`
    $keys = Select-String -Path (Join-Path $root 'controller\src\impl\autopilot_settings.cpp') -Pattern '(?:\{"|k == ")([a-z_0-9.]+)"(?:, (?:Bool|Int|Float|Hex|String|Vec3)\b|\))' -AllMatches |
            ForEach-Object { $_.Matches } | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
    $doc = Get-Content (Join-Path $root 'controller\docs\AUTOPILOT.md') -Raw
    $miss = @($keys | Where-Object { $doc -notmatch "\b$([regex]::Escape($_))\b" })
    Report 'settings' ($miss.Count -eq 0) "$($keys.Count) keys$(if ($miss) { '; undocumented: ' + ($miss -join ', ') })"
    $sd = @(& (Join-Path $PSScriptRoot 'settings_table.ps1') -Check 2>&1 | ForEach-Object { "$_" })
    Report 'settings-doc' ($LASTEXITCODE -eq 0) ($sd | Select-Object -Last 1)
    $um = "$(& $py -I (Join-Path $PSScriptRoot 'ui_meta.py') --check 2>&1 | Select-Object -Last 1)"
    Report 'ui-meta' ($LASTEXITCODE -eq 0) $um.Trim()
    # collect the whole output first: Select-Object -First 1 in the pipe would stop export.ps1 before its `exit 1`
    # (and $LASTEXITCODE would keep the previous check's 0 - a stale copy reported OK, 26 Oct 2026)
    $sco = @(& (Join-Path $PSScriptRoot 'uimock\export.ps1') -Check 2>&1 | ForEach-Object { "$_" })
    $sc = "$($sco | Select-Object -First 1)"
    Report 'spec-copy' ($LASTEXITCODE -eq 0) $sc.Trim()
    $cg = "$(& $py -I (Join-Path $PSScriptRoot 'checkpoints_gen.py') --check 2>&1 | Select-Object -Last 1)"
    Report 'checkpoints' ($LASTEXITCODE -eq 0) $cg.Trim()
    $srt = @(& $py -I (Join-Path $root 'subtitles\review\FLSubtitles\asr\tests\run_tests.py') 2>&1 | ForEach-Object { "$_" })
    $srtLast = "$($srt | Where-Object { $_ -match '^(Ran |OK|FAILED)' } | Select-Object -Last 2)"
    Report 'subtitle-review' ($LASTEXITCODE -eq 0) $srtLast.Trim()
    $readme = Get-Content (Join-Path $PSScriptRoot 'README.md') -Raw
    $tools = Get-ChildItem $PSScriptRoot -File | Where-Object { $_.Extension -in '.ps1', '.py', '.js' } | ForEach-Object Name
    $unlisted = @($tools | Where-Object { $readme -notmatch [regex]::Escape($_) })
    $dr = "$(& $py -I (Join-Path $PSScriptRoot 'docrefs.py') 2>&1 | Select-Object -Last 1)"
    Report 'doc-refs' ($LASTEXITCODE -eq 0) $dr.Trim()
    Report 'tools' ($unlisted.Count -eq 0) "$($tools.Count) scripts$(if ($unlisted) { '; not in README: ' + ($unlisted -join ', ') })"
    $hs = (Get-Item (Join-Path $root 'HANDOFF.txt')).Length
    Report 'handoff' ($hs -lt 8000) "$hs bytes"
    $dirty = @(git status --porcelain)
    Report 'git' ($dirty.Count -eq 0) "$($dirty.Count) uncommitted path(s)$(if ($dirty) { ': ' + (($dirty | Select-Object -First 4) -join '; ') })"
    $bkDir = & (Join-Path $PSScriptRoot 'backup.ps1') -ShowDest
    $bk = if ($bkDir) { Get-ChildItem $bkDir -Filter 'FLvibed_*.bundle' -ErrorAction SilentlyContinue |
          Sort-Object LastWriteTime -Descending | Select-Object -First 1 }
    $age = if ($bk) { [math]::Round(((Get-Date) - $bk.LastWriteTime).TotalHours, 1) } else { 999 }
    Report 'backup' ($age -lt 4) "newest $(if ($bk) { $bk.Name } else { 'none' }), $age h old"
    $rc = @(& (Join-Path $PSScriptRoot 'research_check.ps1') *>&1 | ForEach-Object { "$_" }) # (it writes with Write-Host)
    $stale = @($rc | Where-Object { $_ -match 'watched paths changed in' }).Count
    Report 'research' ($stale -eq 0) "$stale research note(s) with watched changes since their check-up (tools\research_check.ps1)"
    # a run counts when it started (its legs.tsv end time - secs - boot) over 2 min after the verdict: not the run that judged it
    $sv = @(Import-Csv (Join-Path $root 'docs\missions\scene_saves.tsv') -Delimiter "`t" | Where-Object { $_.source -eq 'leg-checkpoint' })
    $lh = Join-Path $root 'research\experience\legs\legs.tsv'
    $runs = if (Test-Path $lh) { @(Import-Csv $lh -Delimiter "`t") } else { @() }
    $unfit = @($sv | Where-Object { $_.fit -eq 'unfit' })
    $since = @(foreach ($u in $unfit) {
        $v = [datetime]::MinValue
        $n = if ([datetime]::TryParse("$($u.fit_date)", [ref]$v)) {
            @($runs | Where-Object { $_.save -eq $u.name -and $_.date } | Where-Object {
                ([datetime]$_.date).AddSeconds(-([int]$_.secs + [int]$_.boot)) -gt $v.AddMinutes(2) }).Count
        } else { 0 }
        [pscustomobject]@{ save = "$($u.mission) $($u.step) $($u.name)"; n = $n }
    })
    $used = @($since | Where-Object { $_.n -gt 0 })
    $unv = @($sv | Where-Object { $_.fit -notin 'fit', 'unfit' }).Count
    Report 'saves' ($used.Count -eq 0) ("$($unfit.Count) unfit checkpoint save(s)" +
        $(if ($used) { '; run from after the verdict: ' + (($used | ForEach-Object { "$($_.save) $($_.n)x" }) -join ', ') } else { '' }) +
        "; $unv unverified (their first run judges them)")
    foreach ($l in @($Leg | ForEach-Object { $_ -split ',' } | Where-Object { $_ })) {
        $pa = @('progress', $l) + $(if ($Since) { @('--since', $Since) } else { @() })
        $p = @(& $py -I (Join-Path $PSScriptRoot 'splits.py') @pa 2>&1 | ForEach-Object { "$_" })
        $last = "$($p | Select-Object -Last 1)".Trim()
        $unrel = @($p | Select-Object -Last 4 | Where-Object { $_ -match 'UNRELIABLE' }).Count
        Report "progress" ($last -notmatch 'NO PROGRESS' -and $unrel -eq 0) "${l}: $last$(if ($unrel) { "; $unrel of the last 3 sets UNRELIABLE" })"
    }
} finally { Pop-Location }
"$(if ($bad) { "$bad check(s) need attention" } else { 'all checks OK' })"
exit [int]($bad -gt 0)
