# Game knowledge base generator (docs\kb\generated\): tables built straight from the test install's game data, so the
# knowledge base matches the install the project tests against (user, 19 Oct 2026). Re-run after any install change;
# docs\kb\generated\SOURCE.txt records which install the tables came from (Freelancer.exe + DATA fingerprint) and
# `-Check` reports whether they are stale.
#   tools\kbgen.ps1 [-Check]
# Tables (TSV, one row per item):
#   hazards.tsv  every zone with damage (lethal = damage >= 1000/s: planet / sun cores - obstacles; else crossable
#                strict zones) and every field whose file flags danger (mines, gas, density, DAMAGING), per system
#   solars.tsv   every system object: nickname, archetype, position, base it docks to, reputation, ids_name
param([switch]$Check)
$root = Split-Path $PSScriptRoot -Parent
$data = Join-Path $root 'Install\Freelancer HD Edition\DATA'
$exe = Join-Path $root 'Install\Freelancer HD Edition\EXE\Freelancer.exe'
$out = Join-Path $root 'docs\kb\generated'
$null = New-Item -ItemType Directory -Force $out

# Fingerprint: the exe's hash + every DATA file's size and time (cheap, changes with any install update)
$exeHash = (Get-FileHash $exe -Algorithm SHA256).Hash.Substring(0, 16)
$sb = [Text.StringBuilder]::new()
Get-ChildItem $data -Recurse -File | Sort-Object FullName | ForEach-Object { [void]$sb.Append("$($_.FullName.Length)$($_.Length)$($_.LastWriteTimeUtc.Ticks)") }
$dataHash = [BitConverter]::ToString([Security.Cryptography.SHA256]::Create().ComputeHash([Text.Encoding]::UTF8.GetBytes($sb.ToString()))).Replace('-', '').Substring(0, 16)
$stamp = "exe=$exeHash data=$dataHash"
$srcFile = Join-Path $out 'SOURCE.txt'
if ($Check) {
    $was = if (Test-Path $srcFile) { (Get-Content $srcFile | Select-String '^fingerprint ').Line -replace '^fingerprint ', '' } else { '' }
    if ($was -eq $stamp) { "kb generated tables: current ($stamp)" } else { "kb generated tables: STALE (tables $was, install $stamp) - run tools\kbgen.ps1" }
    return
}

# A text ini as a list of sections { name; keys = list of (key, value) } (keys lower-case; BINI files are skipped)
function Read-Ini([string]$path) {
    $secs = [Collections.Generic.List[object]]::new(); $cur = $null
    foreach ($raw in [IO.File]::ReadLines($path)) {
        $l = ($raw -replace ';.*$', '').Trim()
        if (-not $l) { continue }
        if ($l.StartsWith('BINI')) { break }
        if ($l -match '^\[(.+)\]$') { $cur = [pscustomobject]@{ Name = $Matches[1].ToLower(); Keys = [Collections.Generic.List[object]]::new() }; $secs.Add($cur); continue }
        if ($cur -and $l -match '^([^=]+?)\s*=\s*(.*)$') { $cur.Keys.Add(@($Matches[1].ToLower(), $Matches[2])) }
    }
    $secs
}
function Get-Key($sec, [string]$k) { foreach ($p in $sec.Keys) { if ($p[0] -eq $k) { return $p[1] } }; $null }

$systems = Read-Ini (Join-Path $data 'UNIVERSE\universe.ini') | Where-Object Name -eq 'system' |
    ForEach-Object { [pscustomobject]@{ Nick = Get-Key $_ 'nickname'; File = Get-Key $_ 'file' } } | Where-Object File

$hz = [Collections.Generic.List[string]]::new(); $hz.Add("system`tzone`tkind`tshape`tpos`tsize`tdamage`tlethal`tflags")
$so = [Collections.Generic.List[string]]::new(); $so.Add("system`tnickname`tarchetype`tpos`tbase`tdock_with`treputation`tids_name")
foreach ($s in $systems) {
    $path = Join-Path $data ("UNIVERSE\" + $s.File)
    if (-not (Test-Path $path)) { continue }
    $secs = Read-Ini $path
    $zones = @{}
    foreach ($z in $secs | Where-Object Name -eq 'zone') {
        $n = Get-Key $z 'nickname'; if (-not $n) { continue }
        $zones[$n.ToLower()] = $z
        $dmg = Get-Key $z 'damage'
        if ($dmg -and [double]$dmg -gt 0) {
            $hz.Add(("{0}`t{1}`tdamage`t{2}`t{3}`t{4}`t{5}`t{6}`t" -f $s.Nick, $n, (Get-Key $z 'shape'), ((Get-Key $z 'pos') -replace '\s', ''),
                ((Get-Key $z 'size') -replace '\s', ''), $dmg, $(if ([double]$dmg -ge 1000) { 'yes' } else { 'no' })))
        }
    }
    foreach ($f in $secs | Where-Object { $_.Name -in 'asteroids', 'nebula' }) {
        $file = Get-Key $f 'file'; $zn = Get-Key $f 'zone'
        if (-not $file -or -not $zn) { continue }
        $fp = Join-Path $data $file
        if (-not (Test-Path $fp)) { continue }
        $flags = @(Read-Ini $fp | Where-Object Name -eq 'properties' | ForEach-Object { $_.Keys } | Where-Object { $_[0] -eq 'flag' } |
                   ForEach-Object { $_[1].ToLower() } | Where-Object { $_ -in 'mine_danger_objects', 'gas_danger_objects', 'object_density_high', 'damaging' })
        if (-not $flags.Count) { continue }
        $z = $zones[$zn.ToLower()]
        $hz.Add(("{0}`t{1}`t{2}`t{3}`t{4}`t{5}`t`tno`t{6}" -f $s.Nick, $zn, $f.Name, $(if ($z) { Get-Key $z 'shape' }),
            $(if ($z) { (Get-Key $z 'pos') -replace '\s', '' }), $(if ($z) { (Get-Key $z 'size') -replace '\s', '' }), ($flags -join ',')))
    }
    foreach ($o in $secs | Where-Object Name -eq 'object') {
        $so.Add(("{0}`t{1}`t{2}`t{3}`t{4}`t{5}`t{6}`t{7}" -f $s.Nick, (Get-Key $o 'nickname'), (Get-Key $o 'archetype'),
            ((Get-Key $o 'pos') -replace '\s', ''), (Get-Key $o 'base'), ((Get-Key $o 'dock_with')), (Get-Key $o 'reputation'), (Get-Key $o 'ids_name')))
    }
}
[IO.File]::WriteAllLines((Join-Path $out 'hazards.tsv'), $hz)
[IO.File]::WriteAllLines((Join-Path $out 'solars.tsv'), $so)

# ---- equipment, ships, loot (DATA\EQUIPMENT, DATA\SHIPS, DATA\MISSIONS\lootprops.ini) ----
function Index-By([object[]]$secs, [string]$name) {
    $h = @{}
    foreach ($s in $secs | Where-Object Name -eq $name) { $n = Get-Key $s 'nickname'; if ($n) { $h[$n.ToLower()] = $s } }
    $h
}
function Num($v) { if ($null -eq $v -or $v -eq '') { return $null }; $x = 0.0; if ([double]::TryParse(($v -split ',')[0].Trim(), [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$x)) { $x } else { $null } }
$inv = [Globalization.CultureInfo]::InvariantCulture
function F($v, [int]$dp = 1) { if ($null -eq $v) { '' } else { ([double]$v).ToString("0.$('#' * $dp)", $inv) } }
$eq = @(); foreach ($f in Get-ChildItem (Join-Path $data 'EQUIPMENT') -Filter '*.ini') { $eq += Read-Ini $f.FullName }
$prices = @{}
foreach ($g in $eq | Where-Object Name -eq 'good') {
    $e = Get-Key $g 'equipment'; $p = Get-Key $g 'price'
    if (-not $e) { $e = Get-Key $g 'nickname' }
    if ($e -and $p -and -not $prices.ContainsKey($e.ToLower())) { $prices[$e.ToLower()] = $p }
}
$mun = Index-By $eq 'munition'; $mot = Index-By $eq 'motor'; $exp = Index-By $eq 'explosion'
$gt = [Collections.Generic.List[string]]::new()
$gt.Add("gun`tids_name`tkind`thull_dmg`tenergy_dmg`tblast_radius`tmuzzle_speed`tlifetime`trange`trefire`thull_dps`tpower`tturn_rate`tseeker_range`tseeker_fov`tmax_turn`tmotor_accel`tmotor_time`tammo`tammo_price`tgun_price")
foreach ($g in $eq | Where-Object Name -eq 'gun') {
    $n = Get-Key $g 'nickname'; $m = $mun[([string](Get-Key $g 'projectile_archetype')).ToLower()]
    if (-not $m) { continue }
    $x = $exp[([string](Get-Key $m 'explosion_arch')).ToLower()]
    $hull = (Num (Get-Key $m 'hull_damage')) + $(if ($x) { Num (Get-Key $x 'hull_damage') } else { 0 })
    $en = (Num (Get-Key $m 'energy_damage')) + $(if ($x) { Num (Get-Key $x 'energy_damage') } else { 0 })
    $v = Num (Get-Key $g 'muzzle_velocity'); $life = Num (Get-Key $m 'lifetime'); $ref = Num (Get-Key $g 'refire_delay')
    $mo = $mot[([string](Get-Key $m 'motor')).ToLower()]
    $homing = [bool](Get-Key $m 'seeker')
    $kind = if ($homing) { 'homing' } elseif ((Get-Key $m 'requires_ammo') -eq 'true') { 'ammo' } else { 'energy' }
    $range = if ($mo) { $null } elseif ($v -and $life) { $v * $life } else { $null }
    $an = ([string](Get-Key $g 'projectile_archetype')).ToLower()
    $gt.Add(($n, (Get-Key $g 'ids_name'), $kind, (F $hull), (F $en), $(if ($x) { F (Num (Get-Key $x 'radius')) }), (F $v), (F $life 3), (F $range 0),
        (F $ref 3), $(if ($ref -gt 0) { F ($hull / $ref) }), (F (Num (Get-Key $g 'power_usage')) 2), (Get-Key $g 'turn_rate'),
        (Get-Key $m 'seeker_range'), (Get-Key $m 'seeker_fov_deg'), (Get-Key $m 'max_angular_velocity'),
        $(if ($mo) { Get-Key $mo 'accel' }), $(if ($mo) { Get-Key $mo 'lifetime' }), $(if ($kind -ne 'energy') { $an }), $prices[$an], $prices[$n.ToLower()]) -join "`t")
}
$sh = [Collections.Generic.List[string]]::new(); $sh.Add("shield`tids_name`tcapacity`tregen`toffline_threshold`trebuild_time`tshield_type`tprice")
foreach ($s in $eq | Where-Object Name -eq 'shieldgenerator') {
    $n = Get-Key $s 'nickname'
    $sh.Add(($n, (Get-Key $s 'ids_name'), (Get-Key $s 'max_capacity'), (Get-Key $s 'regeneration_rate'), (Get-Key $s 'offline_threshold'),
        (Get-Key $s 'offline_rebuild_time'), (Get-Key $s 'shield_type'), $prices[$n.ToLower()]) -join "`t")
}
$en = [Collections.Generic.List[string]]::new(); $en.Add("engine`tids_name`tmax_force`tlinear_drag`ttop_speed_ship_drag1`treverse_fraction`tcruise_charge`tprice")
foreach ($e in $eq | Where-Object Name -eq 'engine') {
    $n = Get-Key $e 'nickname'; $f = Num (Get-Key $e 'max_force'); $dr = Num (Get-Key $e 'linear_drag')
    $en.Add(($n, (Get-Key $e 'ids_name'), (F $f 0), (F $dr), $(if ($f -and $dr) { F ($f / ($dr + 1)) }), (Get-Key $e 'reverse_fraction'),
        (Get-Key $e 'cruise_charge_time'), $prices[$n.ToLower()]) -join "`t")
}
$shipSecs = Read-Ini (Join-Path $data 'SHIPS\shiparch.ini') | Where-Object Name -eq 'ship'
$sp = [Collections.Generic.List[string]]::new(); $sp.Add("ship`tids_name`ttype`tship_class`tmass`thit_pts`tstrafe_force`tstrafe_accel`tlinear_drag`tsteering_torque`tangular_drag`tnanobots`tbatteries`thold`tmission_property")
foreach ($s in $shipSecs) {
    $m = Num (Get-Key $s 'mass'); $st = Num (Get-Key $s 'strafe_force')
    $sp.Add(((Get-Key $s 'nickname'), (Get-Key $s 'ids_name'), (Get-Key $s 'type'), (Get-Key $s 'ship_class'), (F $m 0), (Get-Key $s 'hit_pts'),
        (F $st 0), $(if ($m -and $st) { F ($st / $m) }), (Get-Key $s 'linear_drag'), ((Get-Key $s 'steering_torque') -replace '\s', ''),
        ((Get-Key $s 'angular_drag') -replace '\s', ''), (Get-Key $s 'nanobot_limit'), (Get-Key $s 'shield_battery_limit'), (Get-Key $s 'hold_size'),
        (Get-Key $s 'mission_property')) -join "`t")
}
# Loot: drop_properties = chance %, min_worth, worth_mult, min, max1, max2 (the game's rule: FREELANCER_RE.md "loot")
$lt = [Collections.Generic.List[string]]::new(); $lt.Add("item`tchance_pct`tmin_worth`tworth_mult`tmin`tmax1`tmax2`tprice")
$lp = Join-Path $data 'MISSIONS\lootprops.ini'
if (Test-Path $lp) {
    foreach ($s in Read-Ini $lp | Where-Object Name -eq 'mlootprops') {
        $n = Get-Key $s 'nickname'; $v = @((Get-Key $s 'drop_properties') -split ',' | ForEach-Object { $_.Trim() })
        $lt.Add((@($n) + $v + @($prices[$n.ToLower()])) -join "`t")
    }
}
# Solar archetypes: type (WEAPONS_PLATFORM, DESTROYABLE_DEPOT, STATION, PLANET...), hit points (1e36 = indestructible),
# radius, loadout (a platform's guns), dock kinds
$sa = [Collections.Generic.List[string]]::new(); $sa.Add("archetype`ttype`thit_pts`tdestructible`tsolar_radius`tloadout`tdocking")
foreach ($s in Read-Ini (Join-Path $data 'SOLAR\solararch.ini') | Where-Object Name -eq 'solar') {
    $hp = Num (Get-Key $s 'hit_pts')
    $docks = @($s.Keys | Where-Object { $_[0] -eq 'docking_sphere' } | ForEach-Object { ($_[1] -split ',')[0].Trim() } | Sort-Object -Unique) -join ','
    $sa.Add(((Get-Key $s 'nickname'), (Get-Key $s 'type'), $(if ($null -ne $hp) { if ($hp -ge 1e30) { 'inf' } else { F $hp 0 } }),
        $(if ($null -ne $hp -and $hp -lt 1e30) { 'yes' } else { 'no' }), (Get-Key $s 'solar_radius'), (Get-Key $s 'loadout'), $docks) -join "`t")
}
[IO.File]::WriteAllLines((Join-Path $out 'solararch.tsv'), $sa)

# ---- story missions (DATA\MISSIONS\Mxx\*.ini): what each mission spawns, its objectives, its triggers ----
# msn_objects.tsv     spawned solars and ships: kind, nickname, archetype (ships: npcships.ini npc_ship_arch ->
#                     ship_archetype), faction, labels, position, name id, and what the script does to it (marked,
#                     invulnerable, destroyed by script, a kill objective / fail condition)
# msn_objectives.tsv  [NNObjective]s: type (ids / navmarker / rep_inst), text ids, system, position or object
# msn_triggers.tsv    every trigger: conditions + the actions that matter to a pilot (MarkObj, SetNNObj, ChangeState,
#                     Invulnerable, Destroy, LightFuse, SpawnShip/Solar, NagDist*, GiveObjList on the player)
$mo = [Collections.Generic.List[string]]::new(); $mo.Add("mission`tkind`tnickname`tarchetype`tfaction`tlabels`tpos`tname_ids`tmarked`tinvulnerable`tscript_destroys`tkill_trigger")
$mj = [Collections.Generic.List[string]]::new(); $mj.Add("mission`tobjective`tstate`ttype`targs")
$mt = [Collections.Generic.List[string]]::new(); $mt.Add("mission`ttrigger`tinit_state`tsystem`tconditions`tactions")
$keep = '^act_(markobj|setnnobj|changestate|invulnerable|destroy|lightfuse|spawnship|spawnsolar|nagdist\w*|setnnstate|playercandock|playercanlaunch|setvibe\w*|adjhealth|givenobjlist|giveobjlist|popupdialog|save|relocate\w*)$'
foreach ($md in Get-ChildItem (Join-Path $data 'MISSIONS') -Directory | Where-Object Name -match '^M\d') {
    $mission = $md.Name
    $npcShips = @{}
    $np = Join-Path $md.FullName 'npcships.ini'
    if (Test-Path $np) { foreach ($s in Read-Ini $np | Where-Object Name -eq 'npcshiparch') { $npcShips[([string](Get-Key $s 'nickname')).ToLower()] = Get-Key $s 'ship_archetype' } }
    $secs = @(); foreach ($f in Get-ChildItem $md.FullName -Filter '*.ini' | Where-Object Name -ne 'npcships.ini') { $secs += Read-Ini $f.FullName }
    $npcs = Index-By $secs 'npc'
    # what the triggers do to each object / label
    $marked = @{}; $invul = @{}; $destroys = @{}; $kills = @{}
    foreach ($tr in $secs | Where-Object Name -eq 'trigger') {
        $tn = Get-Key $tr 'nickname'
        $conds = @(); $acts = @()
        foreach ($p in $tr.Keys) {
            $k = $p[0]; $v = $p[1]
            $first = (($v -split ',')[0]).Trim().ToLower()
            if ($k -like 'cnd_*') {
                $conds += "$k=$v"
                if ($k -eq 'cnd_destroyed') { if (-not $kills[$first]) { $kills[$first] = $tn } }
            } elseif ($k -match $keep) {
                $acts += "$k=$v"
                switch ($k) {
                    'act_markobj' { if (($v -split ',')[1].Trim() -ne '0') { $marked[$first] = $true } }
                    'act_invulnerable' { if (($v -split ',')[1].Trim() -eq 'true') { $invul[$first] = $true } }
                    'act_destroy' { $destroys[$first] = $true }
                    'act_lightfuse' { $destroys[$first] = $true }
                }
            }
        }
        $mt.Add(($mission, $tn, (Get-Key $tr 'initstate'), (Get-Key $tr 'system'), ($conds -join ' ; '), ($acts -join ' ; ')) -join "`t")
    }
    $flags = { param($n, $labels)
        $ids = @($n.ToLower()) + @($labels | ForEach-Object { $_.ToLower() })
        $m = [bool]($ids | Where-Object { $marked[$_] }); $i = [bool]($ids | Where-Object { $invul[$_] })
        $d = [bool]($ids | Where-Object { $destroys[$_] }); $k = ($ids | ForEach-Object { $kills[$_] } | Where-Object { $_ } | Select-Object -First 1)
        $yn = { param($b) if ($b) { 'yes' } else { '' } }
        , @((& $yn $m), (& $yn $i), (& $yn $d), $(if ($k) { [string]$k } else { '' }))
    }
    foreach ($s in $secs | Where-Object Name -eq 'msnsolar') {
        $n = Get-Key $s 'nickname'; $labels = @($s.Keys | Where-Object { $_[0] -eq 'label' } | ForEach-Object { $_[1] })
        $mo.Add((@($mission, 'solar', $n, (Get-Key $s 'archetype'), (Get-Key $s 'faction'), ($labels -join ','), ((Get-Key $s 'position') -replace '\s', ''), (Get-Key $s 'string_id')) + (& $flags $n $labels)) -join "`t")
    }
    foreach ($s in $secs | Where-Object Name -eq 'msnship') {
        $n = Get-Key $s 'nickname'; $labels = @($s.Keys | Where-Object { $_[0] -eq 'label' } | ForEach-Object { $_[1] })
        $npc = $npcs[([string](Get-Key $s 'npc')).ToLower()]
        $arch = if ($npc) { $npcShips[([string](Get-Key $npc 'npc_ship_arch')).ToLower()] }
        $mo.Add((@($mission, 'ship', $n, $arch, $(if ($npc) { Get-Key $npc 'affiliation' }), ($labels -join ','), ((Get-Key $s 'position') -replace '\s', ''),
            $(if ($npc) { Get-Key $npc 'individual_name' })) + (& $flags $n $labels)) -join "`t")
    }
    foreach ($o in $secs | Where-Object Name -eq 'nnobjective') {
        $ty = @((Get-Key $o 'type') -split ',' | ForEach-Object { $_.Trim() })
        $mj.Add(($mission, (Get-Key $o 'nickname'), (Get-Key $o 'state'), $ty[0], (($ty | Select-Object -Skip 1) -join ',')) -join "`t")
    }
}
[IO.File]::WriteAllLines((Join-Path $out 'msn_objects.tsv'), $mo)
[IO.File]::WriteAllLines((Join-Path $out 'msn_objectives.tsv'), $mj)
[IO.File]::WriteAllLines((Join-Path $out 'msn_triggers.tsv'), $mt)
[IO.File]::WriteAllLines((Join-Path $out 'guns.tsv'), $gt)
[IO.File]::WriteAllLines((Join-Path $out 'shields.tsv'), $sh)
[IO.File]::WriteAllLines((Join-Path $out 'engines.tsv'), $en)
[IO.File]::WriteAllLines((Join-Path $out 'ships.tsv'), $sp)
[IO.File]::WriteAllLines((Join-Path $out 'loot.tsv'), $lt)
[IO.File]::WriteAllText($srcFile, "Generated by tools\kbgen.ps1 from the test install (Install\Freelancer HD Edition) on $(Get-Date -Format 'yyyy-MM-dd HH:mm').`r`nfingerprint $stamp`r`nsystems $($systems.Count), hazards $($hz.Count - 1), objects $($so.Count - 1)`r`n")
"kb: $($systems.Count) systems, $($hz.Count - 1) hazards, $($so.Count - 1) objects, $($gt.Count - 1) guns/launchers, $($sh.Count - 1) shields, $($en.Count - 1) engines, $($sp.Count - 1) ships, $($lt.Count - 1) loot rules, $($mo.Count - 1) mission objects, $($mj.Count - 1) objectives, $($mt.Count - 1) triggers ($stamp)"
