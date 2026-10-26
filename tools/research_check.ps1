# research_check.ps1 - the staleness check for docs\research (docs\research\README.md "The check-up round").
# Prints HEAD and the dirty paths, then for every research note the commits since its "Checked against" commit
# that touch the paths in its "Watch:" line, and the commits since the backlog's last check-up.
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

    Get-ChildItem $notes -Filter '*.md' | Where-Object { $_.Name -notin @('README.md', 'IDEAS.md') } | ForEach-Object {
        $text = Get-Content $_.FullName -Raw
        $hash = [regex]::Match($text, 'Checked against:\s*FLvibed\s+(?<h>[0-9a-f]{7,})').Groups['h'].Value
        $watch = [regex]::Match($text, '(?m)^Watch:\s*(?<w>.+)$').Groups['w'].Value
        Write-Host "`n== $($_.Name)  checked against ${hash}"
        if (-not $hash) { Write-Host "   no 'Checked against' header"; return }
        git cat-file -e "${hash}^{commit}" 2>$null
        if ($LASTEXITCODE -ne 0) { Write-Host "   commit ${hash} not in this repository"; return }
        $paths = @()
        if (-not $All -and $watch) { $paths = $watch -split '\s*,\s*' | Where-Object { $_ } }
        $args = @('log', '--oneline', '--date=short', '--format=%h %ad %s', "${hash}..HEAD")
        if ($paths.Count) { $args += '--'; $args += ($paths | ForEach-Object { $_ -replace '\\', '/' }) }
        $log = git @args 2>$null
        if ($log) { Write-Host "   watched paths changed in $(@($log).Count) commit(s):"; $log | ForEach-Object { Write-Host "   $_" } }
        else { Write-Host "   nothing in the watched paths since ${hash}" }
        if ($dirty -and $paths.Count) {
            $hit = $dirty | Where-Object { $line = $_.Substring(3); $paths | Where-Object { $line -like ($_ -replace '\\', '/').TrimEnd('*') + '*' } }
            if ($hit) { Write-Host "   dirty now in watched paths:"; $hit | ForEach-Object { Write-Host "   $_" } }
        }
    }
}
finally { Pop-Location }
