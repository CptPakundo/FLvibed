# A multi-arm change set on one game instance (docs\TESTING.md "The test loop" 3-4): -Rounds rounds of the arms in
# -Order, one leg each (flset.ps1 -Runs 1), every arm = -Base plus its own switches from -Extras; arm a (or any arm not
# named in -Extras) flies the base alone. Run one pwsh per instance with the order rotated (spreads instance / time bias);
# labels <leg>-<set>-<arm> for splits.py compare. Segment practice through -StartCp / -StopAt (flset.ps1: an unfit
# checkpoint save is refused before the first arm unless -Force, passed through). The tables
# (docs\LEGS.md, splits.py update) are regenerated once after all rounds (every leg runs with -NoTables).
# Usage: tools\flarms.ps1 -Instance 1 -Leg M12 -Set s11x -Order a,b,c -Rounds 3 -Base 'ref_line 1' `
#          -Extras 'b=dock_regain 1;c=pre_aggro 2500,blast_flank 0' [-StartCp M12.banner] [-StopAt M12.gen3]
#   -Sprt (opt-in; WORK_QUEUE H2, IDEAS T1 - whether it becomes the default is the owner's D1): after every third round
#   the split history is refreshed and tools\splits.py sprt judges each arm against A (-SprtA, else arm a, else the
#   first arm of -Order: the same A on every instance whatever its rotated order) - on
#   -SprtSeg <checkpoint> when given, else the leg total; an arm with a final verdict (KEEP / REVERT / INCONCLUSIVE /
#   NODATA) runs no more, the set ends when every arm has one; -Rounds stays the cap. One pwsh per instance as before:
#   each instance judges the history of all instances' runs so far (labels are shared).
param([Parameter(Mandatory)][int]$Instance, [Parameter(Mandatory)][string]$Leg, [Parameter(Mandatory)][string]$Set,
      [Parameter(Mandatory)][string]$Order, [int]$Rounds = 1, [string]$Base = '', [string]$Extras = '',
      [string]$StartCp = '', [string]$StopAt = '', [switch]$Force, [switch]$Sprt, [string]$SprtSeg = '', [string]$SprtA = '')
$root = Split-Path $PSScriptRoot -Parent
$g = Join-Path $PSScriptRoot 'flset.ps1'
$py = Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe'
$sp = Join-Path $root 'tools\splits.py'
# (an unfit save throws in the first flset call: the set stops there, before any arm ran)
$extra = @{}
foreach ($e in ($Extras -split ';')) { if ($e -match '^\s*(\w+)=(.*)$') { $extra[$Matches[1]] = $Matches[2] } }
$arms = @($Order -split ',' | Where-Object { $_ })
# the sprt's reference arm (A): -SprtA, else arm a (the base alone) when it flies, else the first of -Order - the same
# arm on every instance whatever its rotated order (it was the first of -Order: instance 2's "b,a" judged a against b,
# set q4s1, 30 Oct 2026)
$ref = if ($SprtA) { $SprtA } elseif ($arms -contains 'a') { 'a' } else { $arms[0] }
if ($Sprt -and $arms -notcontains $ref) { throw "-SprtA ${ref}: not an arm of -Order $Order" }
$done = @{}   # arm -> its final sprt verdict
for ($r = 0; $r -lt $Rounds; $r++) {
    foreach ($v in $arms) {
        if ($done.ContainsKey($v)) { continue }
        $ap = @($Base, $extra[$v]) | Where-Object { $_ }
        & $g -Instance $Instance -Leg $Leg -Set $Set -Variant $v -Runs 1 -ApSet ($ap -join ',') -StartCp $StartCp -StopAt $StopAt -NoTables -Force:$Force
    }
    if ($Sprt -and ($r + 1) % 3 -eq 0 -and $arms.Count -gt 1) {
        $null = & $py -I $sp update 2>&1
        $a = "^$([regex]::Escape("$Leg-$Set-$ref"))$"
        foreach ($v in $arms) {
            if ($v -eq $ref -or $done.ContainsKey($v)) { continue }
            $args2 = @('-I', $sp, 'sprt', $a, "^$([regex]::Escape("$Leg-$Set-$v"))$") + $(if ($SprtSeg) { @('--seg', $SprtSeg) } else { @() })
            $out = @(& $py @args2 2>&1 | ForEach-Object { "$_" })
            $line = $out | Where-Object { $_ -match '^SPRT (\w+)' } | Select-Object -First 1
            $verdict = if ($line -match '^SPRT (\w+)') { $Matches[1] } else { 'ERROR' }
            "sprt after round $($r + 1): arm $v vs $ref -> $line"
            if ($verdict -ne 'CONTINUE') { $done[$v] = $verdict }
        }
        if ($done.Count -ge $arms.Count - 1) {
            "sprt: every arm has its verdict after round $($r + 1) ($(($done.GetEnumerator() | ForEach-Object { "$($_.Key) $($_.Value)" }) -join ', ')) - the set ends"
            break
        }
    }
}
# the tables once per set: the same two commands flleg.ps1 runs after a single leg (docs\LEGS.md from the history; the
# split history, PBs, golds, limits, docs\SPLITS.md and the LiveSplit files)
try { & node (Join-Path $root 'tools\legtable.js') } catch { "legtable: $_" }
try { $null = & (Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe') -I (Join-Path $root 'tools\splits.py') update 2>&1 }
catch { "splits: $_" }
"tables regenerated (LEGS.md, splits update)"
