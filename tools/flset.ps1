# One side of a change set (docs\TESTING.md "The test loop"): -Runs legs of one leg in a row on one game instance, all
# labelled <leg>-<set>-<variant> for tools\splits.py compare, with the variant's autopilot switches (-ApSet, sent after
# each boot by flleg.ps1). Run one pwsh per instance (background); the log goes to research\experience\legs\sets\.
# Usage: tools\flset.ps1 -Instance 1 -Leg M12 -Set s10 -Variant ring -Runs 2 -ApSet 'ring_request 10'
# Segment practice ("The test loop" 4): -StartCp <checkpoint> starts each run from the newest checkpoint save of that
# checkpoint (made by -SaveAt in an earlier leg: docs\missions\scene_saves.tsv, source leg-checkpoint) or -Save <path>;
# -StopAt <checkpoint> ends it there. -SaveAt <checkpoints> makes such saves during the runs.
# Save fit ("The test loop" 4): a checkpoint save whose catalogue row says fit=unfit (the first run from it found the
# load unrepresentative: saved moving and loads at rest, or another objective after the load) is refused - the throw
# names its fit_note - unless -Force; fit=unverified (or none recorded) runs with a warning: its first run judges it.
# The tables (docs\LEGS.md by legtable.js, splits.py update) are regenerated once after the loop, not after every leg
# (flleg.ps1 -NoTables; "The test loop" 4); -NoTables skips them here too (flarms.ps1 regenerates after its own rounds).
param([Parameter(Mandatory)][int]$Instance, [Parameter(Mandatory)][string]$Leg, [Parameter(Mandatory)][string]$Set,
      [Parameter(Mandatory)][string]$Variant, [int]$Runs = 2, [string[]]$ApSet = @(),
      [string]$Save = '', [string]$StartCp = '', [string]$StopAt = '', [string[]]$SaveAt = @(), [switch]$NoTables,
      [switch]$Force)
$root = Split-Path $PSScriptRoot -Parent
if ($StartCp) {
    # the newest leg-checkpoint save of that checkpoint, or the catalogue row of the -Save given
    $rows = @(Import-Csv (Join-Path $root 'docs\missions\scene_saves.tsv') -Delimiter "`t" |
              Where-Object { $_.mission -eq $Leg -and $_.step -eq $StartCp -and $_.source -eq 'leg-checkpoint' })
    $row = if ($Save) { $rows | Where-Object { $_.name -eq (Split-Path $Save -Leaf) } | Select-Object -Last 1 }
           else { $rows | Sort-Object date | Select-Object -Last 1 }
    if (-not $Save -and -not $row) { throw "flset: no checkpoint save of $Leg $StartCp (make one with -SaveAt $StartCp)" }
    if ($row) {
        if (-not $Save) { $Save = Join-Path $root "research\saves\scenes\$($row.name)" }
        $fit = "$($row.fit)"
        if ($fit -eq 'unfit') {
            $msg = "flset: checkpoint save $($row.name) ($Leg $StartCp) is UNFIT for segment practice ($($row.fit_date)): $($row.fit_note)"
            if (-not $Force) { throw "$msg - make a save where the story waits at rest (-SaveAt), or -Force" }
            Write-Warning "$msg (-Force: running it anyway)"
        } elseif ($fit -ne 'fit') {
            Write-Warning "flset: checkpoint save $($row.name) ($Leg $StartCp) is unverified: its first run judges it (SAVE-FIT in the transcript, fit in docs\missions\scene_saves.tsv)"
        }
    }
} elseif (-not $Save) {
    # the leg's start save (docs\legs_reference.tsv: a full leg, judged against the cumulative limits too)
    $Save = (Import-Csv (Join-Path $root 'docs\legs_reference.tsv') -Delimiter "`t" | Where-Object { $_.leg -eq $Leg }).start_save
    if (-not $Save) { throw "flset: no start save for leg '$Leg' in docs\legs_reference.tsv" }
}
# (-ApSet 'a 1','b 2' through pwsh -File arrives as one string: split on commas)
$split = { param($a) @($a | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ }) }
$sets = & $split $ApSet
$saveAt = & $split $SaveAt
$label = "$Leg-$Set-$Variant"
$log = Join-Path $root "research\experience\legs\sets\$label`_i$Instance.log"
for ($i = 1; $i -le $Runs; $i++) {
    if ($row -and $i -gt 1 -and -not $Force -and "$($row.fit)" -ne 'fit') {
        # an unverified save judged by this set's first run: an unfit verdict ends the set here (tables still regenerated)
        $now = Import-Csv (Join-Path $root 'docs\missions\scene_saves.tsv') -Delimiter "`t" |
               Where-Object { $_.name -eq $row.name -and $_.mission -eq $Leg -and $_.source -eq 'leg-checkpoint' } | Select-Object -Last 1
        if ("$($now.fit)" -eq 'unfit') {
            "$(Get-Date -Format 'HH:mm:ss') stopped before run $i/${Runs}: $($row.name) judged UNFIT: $($now.fit_note)" | Add-Content $log
            Write-Warning "flset: $($row.name) judged unfit by this set's first run - stopped ($($now.fit_note))"
            break
        }
    }
    "$(Get-Date -Format 'HH:mm:ss') run $i/$Runs $label i$Instance save $Save$(if ($StartCp) { " from $StartCp" })$(if ($StopAt) { " to $StopAt" }) ap set: $($sets -join '; ')" | Add-Content $log
    $res = & (Join-Path $root 'controller\tools\flleg.ps1') -Instance $Instance -Save $Save -Label $label -ApSet $sets `
             -SaveAt $saveAt -StartCp $StartCp -StopAt $StopAt -NoTables 2>&1 | ForEach-Object { "$_" }
    "$(Get-Date -Format 'HH:mm:ss') $($res | Where-Object { $_ -match '^RESULT ' } | Select-Object -Last 1)" | Add-Content $log
}
# the tables once per set: the same two commands flleg.ps1 runs after a single leg (docs\LEGS.md from the history; the
# split history, PBs, golds, limits, docs\SPLITS.md and the LiveSplit files)
if (-not $NoTables) {
    try { & node (Join-Path $root 'tools\legtable.js') } catch { "legtable: $_" }
    try { $null = & (Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe') -I (Join-Path $root 'tools\splits.py') update 2>&1 }
    catch { "splits: $_" }
    "$(Get-Date -Format 'HH:mm:ss') tables regenerated (LEGS.md, splits update)" | Add-Content $log
}
