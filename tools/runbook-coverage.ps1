<#
Coverage check for a mission runbook (docs\missions\<Mission>.md): lists what the mission script defines that the
runbook never mentions: triggers, NN objectives, Act_ChangeState FAIL ids, Act_Save points and string ids used by
objectives, popups and failures. A runbook is complete when every trigger is either described or listed as dead /
orphan, and every FAIL / save / objective appears. Also lists steps without the template fields "**Harness signal**"
and "**Blocking UI / scenes**".

  runbook-coverage.ps1 -Mission M02            (one mission)
  runbook-coverage.ps1                         (every mission with a runbook; summary)
#>
param([string]$Mission)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$missions = Join-Path $root 'Install\Freelancer HD Edition\DATA\MISSIONS'
$books = Join-Path $root 'docs\missions'

function Get-Coverage([string]$name) {
    $book = Join-Path $books "$name.md"
    $dir = Join-Path $missions $name
    $ini = Get-ChildItem $dir -Filter "$($name.ToLower()).ini" | Select-Object -First 1
    if (-not (Test-Path $book) -or -not $ini) { return $null }
    $text = Get-Content $book -Raw
    $lines = Get-Content $ini.FullName
    $section = ''; $triggers = @(); $objectives = @(); $fails = @(); $saves = @(); $ids = @()
    $blocks = @{}; $blockStart = 0; $n = 0   # trigger nickname -> its line range (1-based)
    foreach ($l in $lines) {
        ++$n
        if ($l -match '^\s*\[(\w+)\]') { $section = $Matches[1]; $blockStart = $n; continue }
        if ($l -match '^\s*nickname\s*=\s*(\S+)') {
            if ($section -eq 'Trigger') {
                $nick = $Matches[1]   # the loop below overwrites $Matches
                $triggers += $nick
                $end = $n; while ($end -lt $lines.Count -and $lines[$end] -notmatch '^\s*\[') { ++$end }
                $blocks[$nick.ToLower()] = @($blockStart, $end)
                continue
            }
            elseif ($section -eq 'NNObjective') { $objectives += $Matches[1] }
        }
        if ($l -match 'Act_ChangeState\s*=\s*FAIL\s*,\s*(\d+)') { $fails += $Matches[1] }
        if ($l -match 'Act_Save\s*=\s*(\w+)\s*,\s*(\d+)') { $saves += $Matches[1] }
        if ($l -match '^\s*(Act_PopUpDialog|Act_SetNNObj|Act_NagDist\w*|Act_NNIds|Act_NagGreet)\s*=.*?\b(\d{5,6})\b') { $ids += $Matches[2] }
    }
    $miss = { param($list) @($list | Sort-Object -Unique | Where-Object { $text -notmatch "\b$([regex]::Escape($_))\b" }) }
    # Template fields every step must carry (docs\missions\README.md).
    $fields = 'Harness signal', 'Blocking UI'
    $stepsMissing = @(foreach ($s in ([regex]::Split($text, '(?m)^(?=#{2,3} Step \w)') | Where-Object { $_ -match '^#{2,3} Step \w' })) {
        $head = (($s -split "`n")[0] -replace '^#+\s*', '').Trim()
        $lack = @($fields | Where-Object { $s -notmatch "(?m)^\s*-\s*\**$([regex]::Escape($_))" })
        if ($lack) { "$head (no $($lack -join ', '))" }
    })
    # Citations "`trigger` (line)" (1-based) that point neither into that trigger's block nor at a line naming it
    # (the line that arms or deactivates it).
    $badCites = @(foreach ($m in [regex]::Matches($text, '`([\w\-]+)`\s*\((\d{2,5})')) {
        $nick = $m.Groups[1].Value
        $b = $blocks[$nick.ToLower()]
        $ln = [int]$m.Groups[2].Value
        if (-not $b -or ($ln -ge $b[0] -and $ln -le $b[1])) { continue }
        if ($ln -le $lines.Count -and $lines[$ln - 1] -match "\b$([regex]::Escape($nick))\b") { continue }
        "$nick ($ln): not in its block $($b[0])-$($b[1]) and line $ln does not name it"
    })
    [pscustomobject]@{
        Mission = $name
        BadCitations = $badCites
        Triggers = "{0}/{1}" -f ($triggers.Count - (& $miss $triggers).Count), $triggers.Count
        MissingTriggers = & $miss $triggers
        MissingObjectives = & $miss $objectives
        MissingFails = & $miss $fails
        MissingSaves = & $miss $saves
        MissingIds = & $miss $ids
        StepsMissingFields = $stepsMissing
    }
}

$names = if ($Mission) { @($Mission) } else { Get-ChildItem $books -Filter 'M*.md' | ForEach-Object BaseName }
foreach ($n in $names) {
    $c = Get-Coverage $n
    if (-not $c) { "${n}: no runbook or script"; continue }
    if ($Mission) { $c }
    else {
        "{0,-4} triggers {1,-9} missing: objectives {2}, fails {3}, saves {4}, ids {5}; steps lacking fields {6}; bad line citations {7}" -f
            $c.Mission, $c.Triggers, $c.MissingObjectives.Count, $c.MissingFails.Count, $c.MissingSaves.Count,
            $c.MissingIds.Count, $c.StepsMissingFields.Count, $c.BadCitations.Count
    }
}
