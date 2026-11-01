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
# -Sprt (opt-in; WORK_QUEUE H2, IDEAS T1 - the default is the owner's D1): the two-instance form of a set (one flset
#   per variant, each on its own instance). After every third run of this flset the split history is refreshed and
#   tools\splits.py sprt judges -SprtA (the reference variant) against -SprtB (default: this -Variant; when this flset
#   IS the reference, -SprtB names the other side) on -SprtSeg <checkpoint> or the leg total; a final verdict (KEEP /
#   REVERT / INCONCLUSIVE / NODATA) ends the set, -Runs stays the cap. Both sides judge the same pair from the shared
#   history, so both stop at the same verdict (flarms.ps1 -Sprt is the one-instance multi-arm form).
param([Parameter(Mandatory)][int]$Instance, [Parameter(Mandatory)][string]$Leg, [Parameter(Mandatory)][string]$Set,
      [Parameter(Mandatory)][string]$Variant, [int]$Runs = 2, [string[]]$ApSet = @(),
      [string]$Save = '', [string]$StartCp = '', [string]$StopAt = '', [string[]]$SaveAt = @(), [switch]$NoTables,
      [switch]$Force, [switch]$Sprt, [string]$SprtA = '', [string]$SprtB = '', [string]$SprtSeg = '', [switch]$FlyA)
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'fllegrun.ps1')   # Invoke-FLLegGuarded
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
if ($Sprt) {
    if (-not $SprtA) { throw "flset -Sprt: name the reference variant with -SprtA" }
    if (-not $SprtB) { $SprtB = $Variant }
    if ($SprtA -eq $SprtB) { throw "flset -Sprt: -SprtA $SprtA is this variant - name the other side with -SprtB" }
    if ($Variant -ne $SprtA -and $Variant -ne $SprtB) { throw "flset -Sprt: -Variant $Variant is neither -SprtA $SprtA nor -SprtB $SprtB" }
    # the gate as a harvest (WORK_QUEUE H18 (1); enforced since the audit of 30 Oct 2026 evening - never used before):
    # the reference arm is not flown when a gate on this build already harvested A samples of this leg - the B arm alone
    # against them (splits.py sprt --a-samples <file>); -FlyA flies it anyway (say why in the command)
    if ($Variant -eq $SprtA -and -not $FlyA) {
        $stamp = "$(Get-Content -LiteralPath (Join-Path $root 'build\dev\build_info.txt') -ErrorAction SilentlyContinue)".Split(' ')[0]
        $have = @(Get-ChildItem -LiteralPath (Join-Path $root 'research\experience\legs\sets') -Filter 'gate_*_A.tsv' -ErrorAction SilentlyContinue |
                  Where-Object { @(Import-Csv -LiteralPath $_.FullName -Delimiter "`t" | Where-Object { $_.leg -eq $Leg -and $_.build -eq $stamp }).Count })
        if ($have.Count) { throw "flset -Sprt: the A arm ($SprtA) of $Leg on build $stamp is already harvested by a gate ($($have.Name -join ', ')): fly only -Variant $SprtB and judge with splits.py sprt --a-samples <that file> <set B>; -FlyA flies A anyway" }
    }
}
$py = Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe'
$sp = Join-Path $root 'tools\splits.py'
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
    # under the wall-clock watchdog (tools\fllegrun.ps1): a hung leg no longer stalls the set
    $res = Invoke-FLLegGuarded @{ Instance = $Instance; Save = $Save; Label = $label; ApSet = $sets; SaveAt = $saveAt
                                  StartCp = $StartCp; StopAt = $StopAt; NoTables = $true }
    "$(Get-Date -Format 'HH:mm:ss') $($res | Where-Object { $_ -match '^RESULT ' } | Select-Object -Last 1)" | Add-Content $log
    if ($Sprt -and $i % 3 -eq 0 -and $i -lt $Runs) {
        $null = & $py -I $sp update 2>&1
        $re = { param($v) "^$([regex]::Escape("$Leg-$Set-$v"))$" }
        $args2 = @('-I', $sp, 'sprt', (& $re $SprtA), (& $re $SprtB)) + $(if ($SprtSeg) { @('--seg', $SprtSeg) } else { @() })
        $out = @(& $py @args2 2>&1 | ForEach-Object { "$_" })
        $line = $out | Where-Object { $_ -match '^SPRT (\w+)' } | Select-Object -First 1
        $verdict = if ($line -match '^SPRT (\w+)') { $Matches[1] } else { 'ERROR' }
        "$(Get-Date -Format 'HH:mm:ss') sprt after run $i/${Runs}: $SprtB vs $SprtA -> $(if ($line) { $line } else { "ERROR: $($out -join ' | ')" })" | Add-Content $log
        if ($verdict -ne 'CONTINUE') {
            # ERROR too ends the set: a verdict step that cannot judge must not let the set run blind to its cap
            "$(Get-Date -Format 'HH:mm:ss') sprt: $verdict - the set ends after run $i/$Runs" | Add-Content $log
            break
        }
    }
}
# the tables once per set: the same two commands flleg.ps1 runs after a single leg (docs\LEGS.md from the history; the
# split history, PBs, golds, limits, docs\SPLITS.md and the LiveSplit files)
if (-not $NoTables) {
    try { & node (Join-Path $root 'tools\legtable.js') } catch { "legtable: $_" }
    try { $null = & (Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe') -I (Join-Path $root 'tools\splits.py') update 2>&1 }
    catch { "splits: $_" }
    "$(Get-Date -Format 'HH:mm:ss') tables regenerated (LEGS.md, splits update)" | Add-Content $log
}
