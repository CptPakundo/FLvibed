# research_check.ps1 - the staleness check for docs\research (docs\research\README.md "The check-up round").
# Prints HEAD and the dirty paths, then for every research note the commits since its "Checked against" commit
# that touch the paths in its "Watch:" line (an entry `path#regex`: only commits whose diff of that path matches the
# regex), and the commits since the backlog's last check-up.
# Usage (PowerShell): tools\research_check.ps1 [-All]   (-All lists every commit since the check, not only watched paths)
param([switch]$All)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$notes = Join-Path $repo 'docs\research'
Push-Location $repo
try {
    $head = (git rev-parse --short HEAD).Trim()
    Write-Host "HEAD ${head}  ($(git log -1 --format=%cd --date=format:'%Y-%m-%d %H:%M'))"
    $dirty = git status --short
    if ($dirty) { Write-Host "Dirty paths (another session may be mid-change):"; $dirty | ForEach-Object { Write-Host "  $_" } }
    else { Write-Host "Working tree clean." }

    $ideas = Join-Path $notes 'IDEAS.md'
    if (Test-Path $ideas) {
        $m = [regex]::Match((Get-Content $ideas -Raw), 'Last check-up:\s*(?<date>[^,]+),\s*HEAD\s+(?<hash>[0-9a-f]{7,})')
        if ($m.Success) {
            $since = $m.Groups['hash'].Value
            $n = (git rev-list --count "${since}..HEAD" 2>$null)
            Write-Host "`nIDEAS.md last check-up $($m.Groups['date'].Value.Trim()) at ${since}: $n commit(s) since."
        }
    }

    Get-ChildItem $notes -Filter '*.md' | Where-Object { $_.Name -notin @('README.md', 'IDEAS.md', 'CHECKUP_HISTORY.md') } | ForEach-Object {
        $text = Get-Content $_.FullName -Raw
        $hash = [regex]::Match($text, 'Checked against:\s*FLvibed\s+(?<h>[0-9a-f]{7,})').Groups['h'].Value
        # Trim: in a CRLF note the line's \r stays in the capture and the last path never matched (T39)
        $watch = [regex]::Match($text, '(?m)^Watch:\s*(?<w>.+)$').Groups['w'].Value.Trim()
        Write-Host "`n== $($_.Name)  checked against ${hash}"
        # The header keeps only the latest check; earlier ones go to CHECKUP_HISTORY.md (README "The check-up round" 2)
        # [ \t], not \s: \s crosses the line end (a line ending ";" + one starting "before" read as a chain, 31 Oct)
        if ($text -match '(?m)^(Checked against|Dirty paths at check):[^\r\n]*;[ \t]*before:?[ \t]') {
            Write-Host "   HEADER CHAIN: a '; before:' chain in the header - move the earlier entries to docs\research\CHECKUP_HISTORY.md"
        }
        if (-not $hash) { Write-Host "   no 'Checked against' header"; return }
        git cat-file -e "${hash}^{commit}" 2>$null
        if ($LASTEXITCODE -ne 0) { Write-Host "   commit ${hash} not in this repository"; return }
        $paths = @()
        if (-not $All -and $watch) { $paths = @($watch -split '\s*,\s*' | ForEach-Object { ($_.Trim()) -replace '\\', '/' } | Where-Object { $_ }) }
        # A path outside the repository has no git history here; git refuses the whole log for it (T39)
        $outside = @($paths | Where-Object { $_ -match '^(\.\./|/|[A-Za-z]:)' })
        if ($outside.Count) {
            Write-Host "   outside the repository, not checked: $($outside -join ', ')"
            $paths = @($paths | Where-Object { $_ -notin $outside })
            if (-not $paths.Count) { return }
        }
        # A Watch entry `path#regex` counts a commit only when its diff of that path adds or removes a line matching
        # the (extended, case-insensitive) regex: a shared file such as docs/SETTINGS.md or FREELANCER_RE.md flagged
        # notes "due" on every unrelated change, 15 of 20 at once on 31 Oct 2026, nearly all "no effect" check-ups
        $plain = @($paths | Where-Object { $_ -notmatch '#' })
        $keyed = @($paths | Where-Object { $_ -match '#' })
        $paths = @($paths | ForEach-Object { ($_ -split '#', 2)[0] })
        $hashes = @()
        $failed = $false
        if ($plain.Count -or -not $keyed.Count) {
            $gitArgs = @('log', '--format=%H', "${hash}..HEAD")
            if ($plain.Count) { $gitArgs += '--'; $gitArgs += $plain }
            $hashes += @(git @gitArgs 2>$null); if ($LASTEXITCODE -ne 0) { $failed = $true }
        }
        foreach ($k in $keyed) {
            $p, $rx = $k -split '#', 2
            $hashes += @(git log --format=%H -E -i "-G$rx" "${hash}..HEAD" -- $p 2>$null); if ($LASTEXITCODE -ne 0) { $failed = $true }
        }
        if ($failed) { Write-Host "   git log failed (exit $LASTEXITCODE) for: $($paths -join ', ')"; return }
        $hashes = @($hashes | Where-Object { $_ } | Select-Object -Unique)
        $log = if ($hashes.Count) { git log --no-walk=sorted --date=short '--format=%h %ad %s' @hashes 2>$null } else { @() }
        if ($log) { Write-Host "   watched paths changed in $(@($log).Count) commit(s):"; $log | ForEach-Object { Write-Host "   $_" } }
        else { Write-Host "   nothing in the watched paths since ${hash}" }
        if ($dirty -and $paths.Count) {
            $hit = $dirty | Where-Object { $line = $_.Substring(3); $paths | Where-Object { $line -like $_.TrimEnd('*') + '*' } }
            if ($hit) { Write-Host "   dirty now in watched paths:"; $hit | ForEach-Object { Write-Host "   $_" } }
        }
    }
}
finally { Pop-Location }
