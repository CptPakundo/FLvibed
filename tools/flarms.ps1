# A multi-arm change set on one game instance (docs\TESTING.md "The test loop" 3-4): -Rounds rounds of the arms in
# -Order, one leg each (flset.ps1 -Runs 1), every arm = -Base plus its own switches from -Extras; arm a (or any arm not
# named in -Extras) flies the base alone. Run one pwsh per instance with the order rotated (spreads instance / time bias);
# labels <leg>-<set>-<arm> for splits.py compare. Segment practice through -StartCp / -StopAt (flset.ps1: an unfit
# checkpoint save is refused before the first arm unless -Force, passed through). The tables
# (docs\LEGS.md, splits.py update) are regenerated once after all rounds (every leg runs with -NoTables).
# Usage: tools\flarms.ps1 -Instance 1 -Leg M12 -Set s11x -Order a,b,c -Rounds 3 -Base 'ref_line 1' `
#          -Extras 'b=dock_regain 1;c=pre_aggro 2500,blast_flank 0' [-StartCp M12.banner] [-StopAt M12.gen3]
param([Parameter(Mandatory)][int]$Instance, [Parameter(Mandatory)][string]$Leg, [Parameter(Mandatory)][string]$Set,
      [Parameter(Mandatory)][string]$Order, [int]$Rounds = 1, [string]$Base = '', [string]$Extras = '',
      [string]$StartCp = '', [string]$StopAt = '', [switch]$Force)
$root = Split-Path $PSScriptRoot -Parent
$g = Join-Path $PSScriptRoot 'flset.ps1'
# (an unfit save throws in the first flset call: the set stops there, before any arm ran)
$extra = @{}
foreach ($e in ($Extras -split ';')) { if ($e -match '^\s*(\w+)=(.*)$') { $extra[$Matches[1]] = $Matches[2] } }
for ($r = 0; $r -lt $Rounds; $r++) {
    foreach ($v in ($Order -split ',')) {
        $ap = @($Base, $extra[$v]) | Where-Object { $_ }
        & $g -Instance $Instance -Leg $Leg -Set $Set -Variant $v -Runs 1 -ApSet ($ap -join ',') -StartCp $StartCp -StopAt $StopAt -NoTables -Force:$Force
    }
}
# the tables once per set: the same two commands flleg.ps1 runs after a single leg (docs\LEGS.md from the history; the
# split history, PBs, golds, limits, docs\SPLITS.md and the LiveSplit files)
try { & node (Join-Path $root 'tools\legtable.js') } catch { "legtable: $_" }
try { $null = & (Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe') -I (Join-Path $root 'tools\splits.py') update 2>&1 }
catch { "splits: $_" }
"tables regenerated (LEGS.md, splits update)"
