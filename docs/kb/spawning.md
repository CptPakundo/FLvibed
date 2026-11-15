# Encounter spawning: who appears where (the game's own rules)

How the game populates space around the player with ambient, patrol and trade traffic, written to be implemented as
an offline spawn simulator (GitHub #61, owner-approved 15 Nov 2026) that reads only the data the game loads. Mission
ships (`Act_SpawnShip` and the like) are not covered here: they spawn where their script says
(`..\FREELANCER_RE.md` "Spawns"), but they do count against the density limit (rule D2).

**Where the code is.** All of it is in **Content.dll** (the SpacePop controller and the encounter classes), not in
Freelancer.exe. Addresses below are Content.dll VAs at its preferred base, as the Ghidra project
(`tools\ghidra\`, `node tools\ghidra\gq.js content asm <va>`) and the decomp number them.

**Marks.** `[exe]` read in Content.dll's disassembly in this pass (15 Nov 2026), address given; `[decomp]` from
`research\repos\Freelancer-Decomp\src\Content\` only (an AI-made rebuild: a lead, not checked instruction by instruction
here); `[data]` read from the HDE 0.7.1 install; `[inferred]` reasoned, not read. Librelancer
(`research\repos\Librelancer`) only parses and edits these keys (its editor, `IniSerializer.cs:332`); it has no spawning
logic to compare with. Nothing here is verified in game yet; the #61 census runs (15 Nov 2026, kept in the driver's notes
outside the repository) are the only live data so far.

## 1. Inputs (all loaded data; nothing is a vanilla table)
| Input | Read from | What the spawner takes |
|---|---|---|
| Zones | system ini `[zone]` (`universe.ini` -> system file) | shape / pos / size / rotate (through the engine's zone object: `pub::System::InZones`, `pub::Zone::GetRadius / GetDistance / ClassifyPoint`), and the population keys of section 2 |
| Encounter files | system ini `[EncounterParameters] nickname, filename` | `[EncounterFormation]` and `[Creation]` (section 4); each zone `encounter =` line gets its **own copy** of the parameters, with its own reroll state and created-ship list (`Encounters.cpp:226-264` `[decomp]`) |
| Ship classes | `DATA\MISSIONS\shipclasses.ini` `[ShipClass] nickname, member` | resolves `sc_fighters` -> `class_fighter`, `diff5` -> `d5` (`[data]`; lookup `Cargo.cpp:198-235` `[decomp]`) |
| NPC ships | `npcships.ini` `[NPCShipArch]` `npc_class`, `level`, loadout, ship | which ship a faction flies for a formation line |
| Faction props | `faction_prop.ini` `npc_ship` lists, `formation = <class>, <formation>` | which factions can fly a line at all; the formation shape by class |
| Formations | `formations.ini` | slot offsets only (no effect on counts) |
| Reputation | the player's standing with each group (live; `initialworld.ini` + story at start) | refuses hostile factions when a mission set `hostile_clamp` off (G1) |
| Trade lanes | system ini trade-lane rings (`prev_ring` / `next_ring`) | lane-end fill, lane attacks, lane arrivals |
| Docks | system objects with docking (`dock_with`, archetype docking fixtures) | single-ship launches (section 8) |

## 2. Which zones can spawn
| Key | Effect | Source / mark |
|---|---|---|
| any population key (`encounter`, `density_restriction`, `population_additive`, `tradelane_attack`, `max_battle_size`, `relief_time`) or `path_label` | the zone is **populated**; a zone with none of them is skipped everywhere | `Zones.cpp:264-265`, `Encounters.cpp:300` `[decomp]` |
| `density = n` (int) | the ship limit (D1); **`density = 0` at the player blocks every spawn** for that player (path zones and the chosen zone alike) | `Zones.cpp:220`, `Populators.cpp:715-724` `[decomp]` |
| `repop_time = t` (s, float; default 10; <= 0 becomes 1) | the spawn chance divisor (D3) and the arrival-fill share (A2) | default `[exe]` 06ef8f9d; clamp `Zones.cpp:224-230` `[decomp]` |
| `encounter = name, level, chance` | one encounter: `name` = an `[EncounterParameters]` nickname, `level` = the `diff<level>` class added to every class line (E5), `chance` = a **relative weight** among the zone's encounters (not a probability) | `Encounters.cpp:223-264`, pick `:374-396` `[decomp]`; "diff%d" `[exe]` 06fb7838 |
| `faction = group, weight` (lines right after an `encounter`) | that encounter's faction weights; a `faction` line anywhere else is ignored | `Encounters.cpp:237-260`, `Zones.cpp:216-219` `[decomp]` |
| `population_additive = bool` (**default true**) | an additive zone may be tried from the near list (2000 m) and the view cone (4000 m) (L2, L3) | default `[exe]` 06ef8fe3 (`byte [esi+0x34] = 1`); vanilla writes only `= false` (123 zones, all systems) `[data]` |
| `max_battle_size = n` (default **0**) | while the player is in combat, a formation is refused once the combat's hostile count reached n (G2) | default `[exe]` 06ef8fd9; compare `[exe]` 06ef561d-06ef5631 |
| `relief_time = t` (default 15) | after combat ends, no spawn from this zone for t seconds (G2) | compare `[exe]` 06ef569d-06ef56ad; default 15 `[decomp]` (`SystemInfo.h:64`) |
| `density_restriction = n, type` | refuses a formation when ships of `type` within 3750 m + the formation's lines of `type` exceed n (G4); **a no-op on vanilla data**, see G4 | `Encounters.cpp:266-274`, `EncounterParams.cpp:913-945` `[decomp]` |
| `tradelane_attack = p` (percent) | weight of this path zone for lane ambushes (section 7) | `Encounters.cpp:279-283` `[decomp]` |
| `path_label`, `usage = patrol / trade` | marks a patrol / trade path zone: always tried when the player is in it (L1); its leader spawns on the path (P2) | `Zones.cpp:185-210` `[decomp]` |
| nickname containing `Tradelane` and a box 10x longer than wide and high | a **trade-lane zone**: counts as radius 0 in the smallest-zone pick (L1), trade-lane arrivals keep to its own lane | `Zones.cpp:162-176` `[decomp]` |
| `pop_type`, `toughness`, `sort`, `spacedust`, `lane_id`, `attack_ids` | **not read by the spawner**. `pop_type` is in no string table of Content.dll, Common.dll, Server.dll or Freelancer.exe; `spacedust` is read by Common.dll / Freelancer.exe (the visual) | string search `[exe]`; `toughness` / `attack_ids` absent from Content.dll `[exe]` (who reads them: open) |
| `mission_type`, `vignette_type`, `mission_eligible` | random-mission placement, not ambient spawning | `Zones.cpp:212-256` `[decomp]` |

Vanilla values (all systems, `[data]`): density 3 (2767 zones), 5 (525), 9, 6, 0 (119); repop_time 90 (2720), 10, 30,
20; max_battle_size 4 (3656); relief_time 30 (2788), 20, 15.

## 3. When: one heartbeat every 3 s per player
- **H1** The SpacePop controller ticks every **3.0 s** (`SetHeartbeatInterval(id, 3.0f)` `[exe]` 06f5a579). On each tick,
  for every player ship in space: collect the zone lists at the ship (L1-L3), then **at most one formation** spawns
  (`PopulateZones`, 06f5b690; `Populators.cpp:342-418` `[decomp]`). Time base (game vs real seconds, pause): `[inferred]`
  game time.
- **H2** A player ship in a trade lane is skipped (no heartbeat spawns while in a lane): `Ship->HasField120()`
  (`Populators.cpp:319` `[decomp]`); that this flag means "in a lane" (it holds two object ids the code treats as a
  segment, `SystemInfo.cpp:738-760`) is `[inferred]`.
- **H3** A mission can switch all of it off (`random_pop` / `Act_RandomPop`) and add **exclusion spheres**
  (`Act_RandomPopSphere x, y, z, r, on|off`): no spawn when the player, or the spawn point, is inside one
  (`Populators.cpp:366-371`, `MissionAct.cpp:225-262` `[decomp]`). Free play: on, no spheres.

## 4. Lists of candidate zones (`CollectSpacePopZones`, 06f5b000)
- **L1 At the ship** (`InZones(pos, 10 m)` `[exe]` 06f5b060): every **path zone** the player is in, plus **one** non-path
  zone: the one with the **smallest radius** (a trade-lane zone counts as 0), inserted at a random position of the list
  (`rand() * (n+1) / 32768`) (`Populators.cpp:675-713` `[decomp]`). If any zone of this list has density 0, stop (H-block).
- **L2 Near** (`InZones(pos, 2000 m)` `[exe]` 06f5b1d9): additive zones within 2000 m that were not *kept* in L1. The
  bigger non-path zones the player is inside were dropped from L1 (their ids zeroed), so an additive one of them comes
  back here (`Populators.cpp:726-746` `[decomp]`).
- **L3 Ahead** (single player only): additive zones in the view frustum 70 x 55.4 degrees, 4000 m deep
  (`InZones(transform, 1.2217, 0.9671, 4000)` `[exe]` 06f5b271-06f5b282), whose surface is within 4000 m, and in neither
  list above (`Populators.cpp:748-790` `[decomp]`).
- Order of the path zones inside L1 = the engine's `InZones` order: `[inferred]` system-file order (open, O7).

## 5. Density limit and the spawn roll
- **D1 Limit** = the largest `density` in L1; a zone of L2 replaces it with its own density when the limit is below 0.6 x
  that density (0.6f `[exe]` 06fbbb38; `Populators.cpp:348-363` `[decomp]`). L3 zones each use floor(0.33 x own density)
  (0.33f `[exe]` 06fbbb3c).
- **D2 N** = Content-owned NPC ships of **any faction** (encounter and mission ships) within **3750 m** of the request
  point (SP; MP 9200 m) (`[exe]` 06ef8f40 -> [06fcf300] = 3750; `CountNpcShipsNear` 06f67440, `DockQueues.cpp:1614-1665`
  `[decomp]`). The 35-degree, 3750 m cone the function also tests adds nothing inside the 3750 m sphere. Ships with a
  flag at +0x5a8 are not counted (meaning open, O1). The request point is the player for L1 / L2, the point **5000 m
  ahead** of the ship for L3 (-2 x 2500 `[exe]` 06fbbb64 / 06fbbb68).
- **D3 Roll per zone**: zones are tried in order L1, L2, L3; the first that spawns ends the tick. For each:
  `N >= Limit` -> no; else **p = clamp((Limit - N) / repop_time, 0, 1)**, success when `rand()/32768 < p`
  (`RollSpawnChance` `[exe]` 06ef9940-06ef9a47: `((Limit*1 - N)/3) / ((repop/1)/3)`). Failing the roll or failing to
  spawn (E-rules, G-rules, placement) moves on to the next zone.
- **D4** Before the roll, every encounter of the zone is refreshed (E2) and any **starred** formation (`*` in a
  permutation) spawns regardless of the roll (`Encounters.cpp:338-357, 403` `[decomp]`). Vanilla has no star `[data]`.
- Example (vanilla ambient zone: density 3, repop 90, nothing near): p = 3/90 per 3 s, about one formation every 90 s.

## 6. What spawns: encounter, formation, faction, ships
- **E1 Encounter**: weighted by `chance` over the zone's encounters; an encounter that spawns nothing is removed from the
  draw and another is drawn, so weights renormalise over the encounters that can spawn (`Encounters.cpp:409-422`
  `[decomp]`).
- **E2 Refresh** (every time the zone is tried): forget created ships that no longer exist; pick each formation's faction
  anew (`group_reroll_frequency` default "every"); pick a permutation by weight when the reroll interval (3rd value of
  `permutation`, default 0 = every time) has passed (`EncounterParams.cpp:628-743` `[decomp]`).
- **E3 Permutation** = `permutation = i [+ j ...][*], weight[, reroll_s]`: the formations spawned together; with no
  `[Creation]`, one permutation of all formations, weight 1. An unstarred pass stops at the first formation that spawns
  (`EncounterParams.cpp:417-471, 905-907` `[decomp]`). Vanilla: every permutation names one formation `[data]`, so an
  encounter = one formation picked by permutation weight.
- **E4 Faction** of a formation: weighted by the encounter's `faction` weights, **restricted to the groups whose
  `faction_prop` `npc_ship` list holds an NPC ship that fits every ship line** of the formation (`BuildFormationFactions`
  `EncounterParams.cpp:502-577`; fit test `Cargo.cpp:183-235` `[decomp]`). A listed faction with no fitting ship never
  appears; a formation with no allowed faction never spawns. `feeling_to_formation` ties a formation's faction to
  another's (friendly / hostile / same).
- **E5 Ship line fit**: the line's classes = its `ship_by_class` class plus `diff<level + offset>` (offset = 4th value,
  floor 1). A class that is a `[ShipClass]` needs one of its `member`s in the NPC ship's `npc_class`; another id must be in
  `npc_class` itself (`Cargo.cpp:205-235` `[decomp]`; tag `[exe]` 06ef39a0 / 06fb7838). So `level` 5 + `diff5` member
  `d5` picks `npc_class ... d5` ships. **The player's rank plays no part**: the arch is drawn uniformly among the fitting
  ones with no level filter (`push -1` `[exe]` 06ef5734 -> `PickFactionShipLineArch` 06f35890).
- **E6 Count** per line: uniform integer in `[min, max]` (`rand()*(max-min+1)/32768 + min`, `EncounterParams.cpp:1072`
  `[decomp]`). Expected ships of a formation = sum of (min+max)/2; the first block is the leader.

## 7. Gates that refuse a formation (`Spawn`, 06ef54e0)
- **G1 Standing**: in single player a faction the player stands at <= -0.6 with spawns only while `hostile_clamp` is on
  (default on; a mission may switch it) (`[exe]` 06ef5577-06ef5599). Player-state flags bit 0 / bit 1 refuse hostile /
  non-hostile factions (`[exe]` 06ef55c4-06ef55f4; who sets them: open, O6).
- **G2 Combat**: the player is "in combat" while an NPC with attitude <= -0.6 towards him is within **2500 m**; the count
  is every such NPC met since the combat began (`UpdateHostileContacts` `PlayerState.cpp:985-1060` `[decomp]`). In combat:
  refused once that count >= the zone's `max_battle_size`. Out of combat: refused until `relief_time` seconds after it
  ended (`[exe]` 06ef55fa-06ef56c8).
- **G3 Availability**: `allow_simultaneous_creation = no` -> only while none of its ships live; `times_to_create` cap
  (vanilla: yes / infinite `[data]`); every line needs a ship arch (E4/E5) (`EncounterParams.cpp:827-856` `[decomp]`).
- **G4 density_restriction** compares the restriction's id with the line's **raw** class ids and the NPCs' raw class ids
  (no `[ShipClass]` resolution: neither the inlined finds in 06ef5a20 nor `HasClassId` 06f575a0 call anything `[exe]`).
  Vanilla restrictions name ship classes (`patroller`, `lawfuls`) while lines carry `class_patroller` / `sc_fighters`, so
  the count of matching lines is 0 and the restriction never applies on vanilla data `[inferred]` (to confirm: O5). A mod
  that names raw ids makes it bite: implement it as written.
- **G5 Placement** (section 8) can fail: the formation is dropped and the next encounter / zone is tried.

## 8. Where the ships appear (`PlaceSpawnFormation`, 06f65f10)
The formation's `arrival` flags (`all`, `tradelane`, `object_all`, `object_docking_ring`, `object_jump_gate`,
`object_station`, `object_capital`, `cruise`, `buzz`; `-x` clears) are tried from a random start until one places
(`SpawnPlacement.cpp:1314-1376`, `EncounterParams.cpp:269-345` `[decomp]`):
- **cruise** (vanilla patrol and trade paths: `arrival = cruise`): a path leader spawns on a random segment of the
  zone's path, walking 200 m steps from the point nearest the player, outside 2500 m of him; others on a sphere of
  2500-2550 m around the player's position + 5 s of his velocity, in the zone (`zone_creation_distance` = how far
  outside the zone the point may be; vanilla 0 = inside; default 1500 `[exe]` 06ef5be5), within 250 m of height of the
  point 7 s ahead, clear of objects by their radius + 300 m (`SpawnPlacement.cpp:1004-1100` `[decomp]`).
- **buzz**: a point 2500-2550 m from the player (stopped) or 2500 m + 2.5 s of travel ahead, 150 m above the path, the
  leader flying at him (`SpawnPlacement.cpp:1140-1310` `[decomp]`).
- **tradelane** (vanilla `tradelane_*` encounters on lane zones): out of a lane within 5000 m, 3750 m beyond the entry
  ring (0 for L3), refused when a ship travelling that way is within 15 km or a lane end is seen by the player
  (4000 m / 70 degrees) (`SpawnPlacement.cpp:668-890` `[decomp]`).
- **object_*** (dock launch): **single-ship formations only**; ships with moorings from a dock 3500-4000 m away; berth
  ships and the rest from a ring / gate / station within 2700 m (2700 `[exe]` 06ef8f80; 3500^2 `[exe]` 06fbbc98;
  `SpawnPlacement.cpp:892-1002` `[decomp]`).
- The 2500 m sphere around the player keeps every heartbeat spawn just outside 2500 m `[decomp]` (consistent with the
  #61 census's first sightings at about 2.5 km).

## 9. Arrival fill: launch, jump-in, lane entry
- **A1** On **launch from a base** (SpaceEnter), on **entering a system** (SystemEnter) and at the exit of a lane the
  player enters (1200 m beyond the end ring, 1200 `[exe]` 06fbbb58), the game fills the area at once (`PopulateArrival`
  06f5beb0; `Populators.cpp:247-307, 420-506, 520-600` `[decomp]`). A console teleport (`pos`) triggers none of these:
  a census after a teleport starts empty and fills only by heartbeats `[inferred]`.
- **A2** Budget = the largest density in L1, or floor(0.6 x density) of an L2 zone if larger (no N subtracted in SP).
  Each zone gets Budget x share, share = (Budget / repop_time x mean repop_time) normalised, i.e. **proportional to
  1 / repop_time**; at least 1; filled by repeated draws **without the chance roll** until it is reached or a draw
  creates nothing (`PopulateUpTo` 06ef9a60).
- **A3** Arrival placement: cruise / buzz on a sphere of 100-1875 m around the point (100 / 1775 `[exe]` 06fbbc78 /
  06fbbc80); no lane arrivals except path leaders; dock launches only for mooring ships within 3500 m. Arrival leaders
  start with life 76 instead of 2 x longevity (`EncounterParams.cpp:955-957` `[decomp]`).

## 10. Trade lanes and patrol paths
- **T1 Lane ambush**: when the player enters a lane, if that lane had no ambush in the last **180 s** (`[exe]` 06fbbf90),
  roll `rand() < lane weight`, where a ring's weight is the largest `tradelane_attack`/100 of the **path zones** at it
  that hold a faction hostile to the ring's owner; a ring at least two ahead with no player within 5000 m (`[exe]`
  06fbbf04) is drawn by weight, then an encounter of that zone with a hostile faction (`SystemInfo.cpp:1427-1600`
  `[decomp]`). Mission switch `tradelane_attacks` (`MissionScript.cpp:1837-1840` `[decomp]`).
- **T2 Lane traffic**: traders on lanes are `tradelane_*` encounters of the lane zones (L1 smallest zone = radius 0, so a
  lane zone wins over any ambient zone the player is also in) arriving out of a ring (section 8), plus the lane-end fill
  (A1). Trade-path encounters fly the path and look for dock targets within 20 km (`SpawnPlacement.cpp:1481-1617`
  `[decomp]`). Their flight after spawning is not part of this page.
- **P1 Patrol paths**: every path zone the player is in is in L1 every tick (no smallest-zone filter), so overlapping
  paths each roll; **P2** their leaders spawn on the path (section 8, cruise) and fly it (`behavior = patrol_path`).

## 11. Bases
No base launches traffic on its own: the only dock launches are encounter formations with an `object_*` arrival
(section 8, single ships), and `mbases.ini` feeds the bar characters (`FBasePop`, `Populators.cpp:793-830` `[decomp]`).
Ships also leave the census by **docking** (trade guides) `[decomp]`.

## 12. Despawning: the life counter (`UpdateNpcShipLives`, 06f73fa0, every 1 s)
- **X1** An encounter ship has a life of `longevity` (default **24** `[exe]` 06ef5c02; vanilla sets none `[data]`), starting
  at 2 x longevity (48; arrival leaders 76). Every 1 s (`[exe]` 06fce770 = 1.0) it loses `rate x 1` (`SystemInfo.cpp:953-1046`
  `[decomp]`), with (`GetNpcShipLifeRate` `[exe]` 06f73bf0-06f73d4a):
  - **refill to longevity** (rate -1) when a player is within **2500 m**, or (SP) sees it (4000 m, 70 degrees), or is in a
    lane with it within 2500 m of the lane segment / 4000 m of its end;
  - else **rate = min(15, 1 + (d - 2500) / 100)** per second, d = distance to the nearest player.
- **X2** At life < 0 it is destroyed, with its **whole formation**, unless (SP) the player targets one of them
  (`SystemInfo.cpp:1006-1040` `[decomp]`). Formation members never decay on their own.
- Consequence: unseen ships beyond about 4 km live a few seconds (24 / 15 = 1.6 s from a full refill); a fresh spawn at
  2.5 km that stays unseen and does not close in is gone in 30-50 s. Steady state = spawn rate x lifetime inside a
  bubble of about 2.5-4 km, not a per-zone population.

## 12a. Flight after spawn (O10, pinned 15 Nov 2026)
The formation's `behavior` makes the leader's guide (`ParseGuideBehavior`: 2 trade, 3 patrol_path, 4 wander; `Guides.cpp:91-97`
`[decomp]`); followers keep formation. A ship's life (X1) is then set by how fast it leaves the 2500 m refill sphere:
- **F1 wander** (`FWanderGuide::Start`, 06f0b410 `[decomp]`): a `GotoVec` with **no cruise** (`Field_5A` cruise 0, `Field_5B`
  no-cruise 1), so at the **engine's top speed** = engine `max_force` / (engine `linear_drag` + ship `linear_drag`)
  (`..\FREELANCER_RE.md` "top speed F / d"; vanilla NPCs 80-120 m/s `[data]`); destination: 16 directions 22.5 degrees apart,
  each at a random range in [1500, 3000] m (`PickPointsAround` 06f61430), tried from a random start, the first **inside
  the zone it spawned from** (`ClassifyPoint < 1`) with a clear segment; arrival range 500 m, then the next leg. None found:
  a 0.5 s wait and both ranges halved while the minimum > 100 m, then up to 1000 m towards the zone's centre (range 250).
  An obstacle within 150 m: 200 m away from it first.
- **F2 patrol_path** (`FPatrolRoute::Update` 06f0bfe0 `[decomp]`): `GotoVec` to the path's points in turn (range 500), **no
  cruise flag either way**, so the AI cruises on long legs: measured at **300 m/s = `EngineEquipConsts::CRUISING_SPEED`**
  (constants.ini; Common.dll default 300) from the first sample (census C3, Magellan path1, 10 ships: median 298-300 m/s
  `[measured]`); at the path's end it turns back (AdvancePoint).
- **F2b cargo scan** (`FCargoScanRoute`, patrol guide only; `FindScanTarget` 06f0c6b0 `[decomp]`): each ship within 2500 m of
  a patrol gets **one roll** at the faction's `scan_chance` (faction_prop; a pilot personality's `scan_chance` overrides;
  vanilla 0.3 for 25 of 55 factions, none for bounty hunters / traders `[data]`) when the patrol's attitude to it lies in
  [-0.6, 0.2] (unlawful faction) or [-0.6, 1.0] (lawful, for a player); a hit trails it at 500 m (ScanRange), scans,
  then resumes the path. Measured (census C4, New York path1, god mode effective): Liberty patrols closed to 70-270 m
  at engine speed and stayed 45-85 s within 2.5 km before cruising off at 300 m/s and despawning at 3.4-4.1 km; Magellan
  path1's bounty hunters (no `scan_chance`) never stopped `[measured]`. The scan's own duration (its comm chain, the
  answer timer) is not read: the simulator holds it as `Assume::scanHold` = 60 s.
- **F3 trade** (`FTradeGuide::Start` 06f07240 `[decomp]`): `BuildPathDirectives` legs (lanes + free `GotoVec`, no cruise
  flag) to a dock target picked at spawn within **20 km** (0x469c4000, `SpawnPlacement.cpp:1481-1617`), docks, waits,
  launches, swaps targets. Lane arrivals (section 8) ride the lane at 2500 m/s: census C3 Hudson lane1, 6 ships first
  seen at 3.3-3.8 km, gone from the 5 km disc within one 3 s sample, next seen 19 km away `[measured]`.
- **F4 hostile to the player**: the pilot's attack overrides the guide; it closes at engine speed with thruster bursts
  (C3: 80-120 m/s median, 200 m/s peaks) and, against a god-mode player, stays (life refilled within 2500 m).
- **Despawn checks X1** (C3, Magellan path1): path ships flying away at 300 m/s vanished as whole formations at 3.2-3.9
  km from the player, 24-30 s after their first sighting; X1 predicts the first unseen second at 2500 m plus
  t + 1.5 t^2 = 24, i.e. t = 3.7 s, d = 3.6 km.
Consequence for the census: path / trade ships are in a 5 km disc for ~25 s, lane traders for under 3 s, wanderers for
their legs' time (1.5-3 km at ~80-120 m/s), god-mode attackers for good. Not modelled by the simulator: cruise charge (5 s
at engine speed, then 60 m/s^2), the obstacle and segment-clear tests, followers' own flight, the trade route's lanes.

## 13. What the census set-up does to the numbers (god mode, player as target)
- Hostiles that reach 2500 m refill their life every second and stay; a god-mode player does not kill or lose them, so
  they pile up (G2 counts them, D2 counts them) and **suppress further spawns** (N grows; in combat the zone's
  `max_battle_size` = 4 stops spawns after the 4th hostile met; after it, `relief_time` 30 s more).
- A ship the player targets never expires, nor does its formation (X2).
- The heartbeat's spawns land at about 2.5 km, so counts in a 5 km disc mix fresh spawns with converging hostiles;
  first sightings (the #61 census's bias fix) measure the spawn side, which is what D3 models.
- Teleported census points miss the arrival fill (A1); points reached by launch / jump / lane start pre-filled.

## 14. Rules not pinned down (and the short live measurement that settles each)
| Open | Measurement |
|---|---|
| O1 the +0x5a8 flag that hides an NPC from N (dying? docking?) | log N as the game counts it (debug read of `CountNpcShipsNear` inputs) beside `track` while ships dock / die |
| O2 heartbeat and life clocks: game seconds? paused in menus? | time 20 consecutive spawn events at a quiet point (one zone, known density / repop): spacing in multiples of 3 s |
| O3 HasField120 = "player in a lane" (H2, X1) | spawn count and NPC lifetimes during a long lane ride vs free flight |
| O4 placement failure rate (zone_creation_distance 0 at zone edges, object clearance) | hold the player 1 km inside vs 1 km outside an ambient zone's edge, count spawns per 100 heartbeats |
| O5 density_restriction is a no-op on vanilla data (G4) | two patrol paths overlapping at one point: expect two patrols within 3750 m (the restriction would allow one) |
| O6 player-state flags bits 0-3 (G1, T1): who sets them | read the flags at a free-play point and inside a mission (debug read) |
| O7 the order of path zones in L1 (affects which path rolls first) | `InZones` result order at a multi-path point vs the system file order |
| O8 `toughness`, `attack_ids` readers (not Content.dll) | string search of the other game DLLs; nothing spawning-related expected |
| O9 relief_time default 15 (register value, not an immediate) | read 06ef8ff3's source register |
| ~~O10 flight after spawn~~ | **pinned**: section 12a (guides read, path / lane / hostile flight measured in census C3) |

## 15. The simulator (outline)
```
inputs (from whatever the game loaded; nothing vanilla-specific):
  system   = parse(universe.ini -> system ini): zones (shape, pos, size, rotate R = Rz*Ry*Rx, keys of section 2),
             [EncounterParameters] -> encounter files, trade-lane ring chains, dockable objects
  classes  = shipclasses.ini; archs = npcships.ini; props = faction_prop.ini (npc_ship, formation)
  standing = player's group reputations (save file or live); flags: hostile_clamp=on, random_pop=on, no spheres
per zone z with population keys: copy each encounter, apply diff<level>; per formation f:
  allowed(f) = encounter factions with an npc_ship fitting every line (E4/E5)
  E[ships|f] = sum over lines of (min+max)/2

state: ships = [] (pos, formation id, life, faction, hostile?), t = 0
loop every 1 s (life, X1-X2) and every 3 s (heartbeat):
  heartbeat at player p (skip if in lane; skip if inside an exclusion sphere):
    L1, L2, L3 = zones by section 4 geometry at p (and its facing for L3)
    if any density==0 in L1: continue
    Limit = D1(L1, L2)
    for z in L1 + L2 + L3:                      # first success ends the tick
      req = p (L3: p + 5000 ahead); N = ships within 3750 m of req
      lim = Limit (L3: floor(0.33*density_z)); if N >= lim: next
      if rand() >= min(1, (lim - N) / repop_z): next
      draw encounters of z by chance (without replacement):
        f = permutation pick; faction = weighted pick over allowed(f)
        apply G1 (standing), G2 (combat state from ships within 2500 m), G3, G4
        pos = placement(arrival flags of f, z, p, velocity)   # section 8; may fail
        if spawned: add ships (count by E6, life = 2*longevity), stop tick
  arrival fill (A1-A3) on launch / jump-in / lane entry
  each ship: move by its behavior (section 12a: wander legs at engine speed, patrol at cruise + cargo scan, trade to a dock,
             lane arrivals at lane speed, hostile -> player at engine speed)
             life update X1, expire formation at life < 0 unless targeted
outputs: for a player held at point p (or flying a route): over T seconds, the time-mean count of ships per
  faction per volume cell (and first-sighting counts per cell, comparable with the census), Monte Carlo over seeds;
  a fast analytic mode: per tick P(z spawns) = prod_{j before z}(1-q_j) * q_j with q_j = p_j * P(placement ok),
  expected ships = sum_z P(z) * sum_enc w_enc * sum_f w_f * E[ships|f] split by faction share, N from the steady state
  (fixed point of spawn rate x lifetime).
```
**Mods with no change.** Every number above that is data (densities, repop times, weights, factions, ship lines,
levels, formations, arrival flags, longevity, lanes, docks) is read from the files the game loaded, in the game's order
and with its defaults (repop 10, additive true, max_battle_size 0, relief 15, longevity 24, zone_creation_distance 1500,
reroll every time). A mod that adds systems, zones, encounters, factions or ships changes only the inputs; the
constants that stay are the engine's own (3 s heartbeat, 3750 m count radius, 2000 / 4000 m lists, 0.6 / 0.33, 2500 m
spawn ring and life rule), which a mod can change only by patching Content.dll. Inside FLvibed the same code can read
the loaded data from the game (the parsed zones and encounters) instead of the files.

## 16. The simulator as built, and its check against the censuses
Built (15 Nov 2026, lane mapzones): `core\game\spawnsim.*` (host-free core: sections 3-8 and 12, Monte Carlo, the
game's 15-bit `rand()`, fixed seed), `core\game\spawnsim_ini.*` (the offline loader), `build.cmd spawnsim` ->
`tools\spawnsim\` (`flspawnsim.exe`, `validate.py`), unit tests `tests\spawnsim_test.cpp`. Not modelled: arrival fill
(A1-A3; the censuses teleported), lane ambush (T1), G4, the lane-arrival refusals (`Assume::laneAccept`). Its guesses
for open rules are `spawnsim::Assume` (O4 placement tries, O10 flight: hostiles close to 1000 m at 100 m/s, path ships
100 m/s along their axis, wanderers still); the player's facing is unknown, so runs cycle through 8 yaws (L3, X1 "seen").
Scored like the v4 model (first sightings in the 5 km disc; count = time-mean ships first seen inside, t >= 30 s), with
**no fitted factor and no calibration**, on all 23 census points (New York x2, Hudson Iw02, Magellan Iw03):
lead faction 59 %, lead relation 77 %, cell lead faction / relation 60 / 77 %, count error 1.15 log2 (v4, calibrated on
New York, held out: 57 / 79 %, 43 / 61 %, 1.34). Base clusters (6 points) are within 0.18 log2 (13 %); the error is an
over-count of 2-3x at open-space, lane and path points of the first New York census (the run whose `godmode` did not
take effect) and at two census points that saw nothing after 30 s. Per system and the reading: the driver's notes
(`145_issue61.md` "Simulator"). What the residual points to: O10 (how long a spawned ship stays within 5 km of the
player: wander, attack runs, path speed), the lane-arrival refusals (section 8 tradelane: setting them off lifts the
relation lead to 86 %), and the combat gate's state in a census whose player was really fighting (G2).

**v2 (15 Nov 2026, O10 folded in, section 12a):** flight from the data (`behavior` per formation; engine speed per NPC ship
from loadouts.ini -> engine_equip `max_force` / (`linear_drag` + shiparch `linear_drag`); cruise speed from constants.ini
`CRUISING_SPEED`, else Common.dll's 300; `scan_chance` / `legality` per faction), still no fitted factor. Results with the
new god-mode New York census C4 (amb_nw, lane22, path1): `145_issue61.md` "Simulator v2". The finding that matters most: at
the same New York points, C4 (god mode effective) counted 4.8 / 4.5 ships where census run 1 (god mode not in effect)
counted 1.1 / 1.4, so most of v1's "2x over-count" was the census, not the rules; the simulator now under-counts C4 by 0.47
log2.

**In game: HUD 2 map's influence cells (15 Nov 2026, lane mapzones; `core\game\influence.*`, `modules\hud\src\mapcells.*`).** The
political layer's ALL on HUD 2's map draws the simulator's field as hex cells (spacing = the populated zones' span / 100, at least
1 km; a cell = expected ships near a player held at its centre, the colour by the first sightings' relation mix, the size and
brightness by the time-mean count, log scale to 12; a lane or patrol path crossing a cell adds its nearest point as a sample, the
cell takes its strongest; cells with the same lists share one simulation of 16 runs x 90 s). Inputs: the shown system's id (map
+0x7E0), the player's vibe (the zone build's, 0x49CB9C) and feelings (Common.dll `GetGroupFeelingsTowards`, groups by Server.dll
`GetReputationGroup`), and the system's zones / encounters / formations / factions / ships from the files EXE\freelancer.ini names,
read with `spawnsim_ini` from the running install: **memory lacks them** (the zone object gives ids, factions and weights only, no
`density`, encounter `chance`, formation or ship list). A cell shows only where the map draws one of its zones (the zone ids the
build drew: the game's discovery rule). Measured (release, instance 11, SaveC706): New York 5382 cells, 5017 simulations, 4.5-4.9 s
on the worker thread (offline tool 4.4 s; load 47 ms); Hudson 4842 cells, 2316 simulations, 1.1 s; draw 0.51-0.56 ms a frame
(New York, 2961 cells drawn), 0.37 ms (Hudson). Again when a faction's relation class flips (feelings read once a second).
