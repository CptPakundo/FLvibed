# A mod's own test fixtures, so the test scripts can run on a prepared instance of a mod (FrontierSpace, 6 Nov 2026). The test install's free-roam fixtures name
# ships and goods a mod may not have: each is rewritten from the mod's own data. An archetype the mod has stays; one it lacks goes; a fixture left without a ship or an
# engine takes the mod's EXE\newplayer.fl loadout. A system or base the mod lacks: no fixture, said plainly, and that name stays unmapped, so a test fails on it rather
# than running on the wrong thing. Writes Save<Prefix>7xx.fl and SAVE\fixtures.map (read by Resolve-FLFixture, tools\fldata.ps1). Nothing here runs a game.
# Usage (PowerShell tool): tools\mkmodfixtures.ps1 -Instance frontierspace [-OutDir <folder>]   -OutDir: write there to look first; the instance is not touched.
param([Parameter(Mandatory)][string]$Instance, [string]$OutDir = '', [string]$Prefix = 'F')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'flhash.ps1')
$inst = Join-Path $root "Install\instances\$Instance\Freelancer HD Edition"
if (-not (Test-Path -LiteralPath (Join-Path $inst 'EXE\Freelancer.exe'))) { throw "no install at $inst" }
$main = Join-Path $root 'Install\Freelancer HD Edition'
$fixtures = Join-Path $main 'SAVE\Accts\SinglePlayer'

# every nickname of the mod's ships, equipment and universe, by hash (text ini only: a binary ini is refused, not guessed)
function Get-Nicknames([string[]]$Files) {
    $set = @{}
    foreach ($f in $Files) {
        $raw = Get-Content -LiteralPath $f -Raw
        if ($raw -and $raw.StartsWith('BINI')) { throw "binary ini not read here: $f" }
        foreach ($m in [regex]::Matches($raw, '(?im)^\s*nickname\s*=\s*(\S+)')) { $set[[string](Get-FLHash $m.Groups[1].Value)] = $m.Groups[1].Value }
    }
    $set
}
$data = Join-Path $inst 'DATA'
$ships = Get-Nicknames @(Join-Path $data 'SHIPS\shiparch.ini')
$goods = Get-Nicknames @(Get-ChildItem -LiteralPath (Join-Path $data 'EQUIPMENT') -Filter '*.ini' -File | ForEach-Object FullName)
$world = Get-Content -LiteralPath (Join-Path $data 'UNIVERSE\universe.ini') -Raw
$engines = Get-Nicknames @(Join-Path $data 'EQUIPMENT\engine_equip.ini')

# the mod's starting loadout, by hash
$start = Get-Content -LiteralPath (Join-Path $inst 'EXE\newplayer.fl')
$startShip = ($start | Where-Object { $_ -match '^\s*ship_archetype\s*=' } | Select-Object -First 1) -replace '^.*=\s*', ''
$startLines = foreach ($l in $start) {
    if ($l -match '^\s*equip\s*=\s*([^,\s]+)\s*(?:,\s*(\S+))?') { "equip = $(Get-FLHash $Matches[1]), $($Matches[2]), 1" }
    elseif ($l -match '^\s*cargo\s*=\s*([^,\s]+)\s*,\s*(\d+)') { "cargo = $(Get-FLHash $Matches[1]), $($Matches[2]), , , 0" }
}

$out = if ($OutDir) { $OutDir } else { Join-Path $inst 'SAVE\Accts\SinglePlayer' }
$null = New-Item -ItemType Directory -Force $out
$map = [Collections.Generic.List[string]]::new()
foreach ($id in 'C701', 'C702', 'C703', 'C705', 'C706') {
    $src = Join-Path $fixtures "Save$id.fl"
    if (-not (Test-Path -LiteralPath $src)) { "Save$id`: no such fixture, skipped"; continue }
    $lines = Get-Content -LiteralPath $src
    $sys = ($lines | Where-Object { $_ -match '^\s*system\s*=' } | Select-Object -First 1) -replace '^.*=\s*', ''
    if ($world -notmatch "(?im)^\s*nickname\s*=\s*$([regex]::Escape($sys))\s*$") { "Save$id`: the mod has no system $sys, not made"; continue }
    $dropped = 0; $kept = [Collections.Generic.List[string]]::new()
    $shipOk = $false; $engine = $false
    foreach ($l in $lines) {
        if ($l -match '^\s*ship_archetype\s*=\s*(\d+)') {
            $shipOk = $ships.ContainsKey($Matches[1])
            if (-not $shipOk) { $l = "ship_archetype = $(Get-FLHash $startShip)" }
        } elseif ($l -match '^\s*(equip|cargo)\s*=\s*(\d+)') {
            if (-not $goods.ContainsKey($Matches[2])) { $dropped++; continue }
            if ($engines.ContainsKey($Matches[2])) { $engine = $true }
        }
        $kept.Add($l)
    }
    if (-not $shipOk -or -not $engine) {   # the mod's whole starting loadout instead
        $at = $kept.FindIndex([Predicate[string]] { param($x) $x -match '^\s*ship_archetype\s*=' })
        for ($k = $kept.Count - 1; $k -ge 0; $k--) { if ($kept[$k] -match '^\s*(equip|cargo)\s*=') { $kept.RemoveAt($k) } }
        $kept[$at] = "ship_archetype = $(Get-FLHash $startShip)"
        $kept.InsertRange($at + 1, [string[]]@($startLines))
    }
    $name = "Save$Prefix$($id.Substring(1)).fl"
    Set-Content -LiteralPath (Join-Path $out $name) $kept -Encoding ascii
    $map.Add("Save$id.fl`t$name")
    "$name from Save${id}: ship $(if ($shipOk) { $ships[($lines | Where-Object { $_ -match '^\s*ship_archetype' } | Select-Object -First 1) -replace '\D', ''] } else { $startShip }), $dropped goods dropped, $(if ($shipOk -and $engine) { 'its own loadout' } else { "the mod's starting loadout" })"
}
$mapFile = if ($OutDir) { Join-Path $OutDir 'fixtures.map' } else { Join-Path $inst 'SAVE\fixtures.map' }
Set-Content -LiteralPath $mapFile $map -Encoding ascii
"$($map.Count) fixtures, map: $mapFile"
