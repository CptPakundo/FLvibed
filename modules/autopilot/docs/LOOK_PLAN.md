# LOOK - act on the next step's goal while the current step only waits (plan, 30 Oct 2026)

Package LOOK of the private work queue "Speed first". Evidence: `..\..\docs\research\gap_decomposition.md` 5 and 7
(the largest single measured rule: ~155-220 s on M02.pod / M04.mactan). Drafted by a read-only Plan agent at 6557ecf;
its central facts re-read by the driver (ab7092b7): m02.ini `start_stage4_attack` (line 1594, not 1627 as drafted)
sets Ashcroft hostile to the player, marks him and arms `ashcroft_dead` / `ashcroft_health_35` during "Follow
Ashcroft", with `Act_NagDistTowards = POS, ash_nag, King, ...` and `Act_Invulnerable = Ashcroft, true, true, 0.5`;
m05.ini arms `player_near_br04_01` (2182) and `dst_to_br04_tradelane` (2136); the nearest-goal choice is
`autopilot_objective.cpp` 1007 (`far - sg.dist < bestDist`). Other line numbers below are the draft's: re-read before
editing (drift of ~10-30 lines seen). L0 is built (log only, section 6); L1-L4 are not.

## 1. The rule (script data only, no mission ids)
While the current step waits (a placeholder, a dialogue chain, a trip to a place whose arrival completes nothing), work
toward the step-after's armed condition:
- **(a) kill**: a `Cnd_Destroyed` on a mission ship already armed by the step's setter / chain, the ship hostile to the
  player and alive as a visible contact -> fight it now instead of flying to the step's marker (M02.pod);
- **(b) fly**: else toward the step-after's `Cnd_DistShip` / `Cnd_DistVec` goal, the least-cost armed goal rather than
  the nearest (M05.end: `player_near_br04_01` skips the lane; M06.end: the holers' kill chains to the hole goal ~6.8 s
  before its banner; M04.mactan: Mactan is the declared place two objectives ahead, opposite Freeport 4's marker).

Safety, each a pure predicate from script data: the current step's towards-nag (`TowardsNagOf`, mission.h) only inside
its grace; ship leashes (`Leashes`, `leashed()`) as today; tethers (`Cnd_TetherBroke`) as leashes beyond ~85 % of their
distance; PSN (never toward a destroy objective's spawn / an unspawned or cloaked hostile; one shared `StageablePlace`
predicate); never fire at a friendly contact or a ship the script has not turned hostile (pre_aggro's job); never pass
the script's own health gates (Ashcroft dies at 35 %); speedrun knowledge behind `Knows(Source::Hidden)` and its preset
(`AUTOPILOT_UX.md` 2a).

## 2. Where
- Pure: `core\game\mission.cpp` `GoalOfTrigger` gains kind 4 (ships by label via `MapSpawns`, exported as
  `ShipIdsOf`), gated so the decision sweep (`tools\decision_baseline.txt`) stays byte-identical; `Tethers` /
  `ArmedTethers` beside (not inside) `LeashesOf` (merging would change M04 for everyone). New
  `modules\controller\src\impl\lookahead.{h,cpp}`: `StageablePlace`, `LookTarget(facts)` -> kill / fly / none, `PathCost`.
- Old path (`autopilot_objective.cpp`): the kill branch before `travelObjective` (today a travel objective outranks the
  fight - why M02 flies the marker), after pre_aggro; the route choice in the goal loop via `PathCost` when
  `look_route` is on; the pre-stage branches call `StageablePlace` (PSN first or inside L1).
- Arbiter: `Rule_Look` after `Rule_PreStageNext` in `situation_rules.cpp`; `SnapLook` facts in the snapshot
  (`situation.h`, `situation_snap.cpp`, `situation_io.h` v6 + fixtures); `shadow_digest.py` classes LOOK differs.

## 3. Slices (each its own `ap set` switch, default = today; host-free test first)
| Slice | Switch | Case | Test | In game |
|---|---|---|---|---|
| L0 | `look_log` | all | a new host-free test in tests\ (look_test, to be written); `ap look` beside `ap goals` | decision_check unchanged |
| L1 | `look_route` | M05.end (M06) | synthetic m05 ini: picks the ring trigger; lane-faster case keeps the lane | save M05.leedshole (-SaveAt), `-StopAt M05.end`, sprt --seg M05.end |
| L2 | `look_ships` | M02.pod, M06.end | armed hostile live ship -> Combat; friendly / unspawned / cloaked / nag breach -> none; Rule_Look equivalence | **NOT BUILT (30 Oct 2026, section 8): no case reaches a detectable effect** |
| L3 | `look_tether` | M04 (enabling) | m04 chain gives 3 tether rows with FAIL; `juni_tether` excluded | offline `ap goals` after the Magellan gate |
| L4 | `look_next` | M04.mactan | decoration step + tether: allowed / refused distance, nag grace | save after the Magellan gate, `-StopAt M04.mactan` |
L0 first; L1, L2, L3 independent; L4 needs L3 and facts 1, 2, 5.

## 4a. Facts read from Content.dll (game-facts agent, 30 Oct 2026; decompiled, none verified in game)
- **Cnd_TetherBroke** = `shipA, shipB, dist, secs[, inner]` (parse 0x06ec11c0, evaluate 0x06ec1330, vtable 0x6FB5100):
  outside `dist` the mission clock accrues; the time is **cumulative, never reset on rejoin** (the gap spent inside is
  added at the next outside sample); the 5th field is an inner radius inside which time accrues only while not
  closing (M04 `juni, Player, 1500, 30, 2500`). Accrual needs a flag set by an unidentified event (+0x7c). Fires at
  accrued >= secs. Consequence for L3 / L4: any time Juni spends beyond 3 km counts for the rest of the step.
- **Act_NagDistTowards** = `POS, name, nagger, x, y, z, failText[, dist][, flag]` / `OBJ, name, nagger, object,
  failText[, dist][, flag]` (parse 0x06ecba90): default 4000 m and NAG_OUT_OF_COMBAT; a 20 s sample timer; "closing" =
  >= 900 m nearer than the last sample (~45 m/s average) or inside the radius, which resets the counter to 4; each
  missed sample plays a nag line, at 0 the mission fails (>= ~80 s; FREELANCER_RE's M09 ~63 s not reconciled). The
  combat flags matter only when the nag target is a ship (M04's Iw03_01 is a solar).
- **Act_Invulnerable** = `ship-or-label, a, [b], [f]` -> `pub::SpaceObj::SetInvincible(id, a, b, f)`; f is read by
  FLvibed as a hull floor (`core\game\space_ship.cpp` 143-165 `AtInvincibleFloor`), but M02's own `ashcroft_health_35`
  -> `Act_Destroy` contradicts a hard 0.5 floor: NOT answered (probe: `GetInvincible` + fire at him in "Follow Ashcroft").
- **Cnd_Destroyed** = `label|nick, count[, kind]`: count N = the N-th matching death, -1 = all; M06 `d_holers =
  holeboys, -1`. FLvibed's `MapSpawns` maps spawns within 2 m of the listed position (unverified for M06's spawns).

## 4. Facts to verify first (original list; 4a answers 1, 2, 4 from code, 3 open)
1. `Cnd_TetherBroke` fields (`ship, Player, dist, secs[, rejoin]`): consecutive time? reset? the 5th field (Ghidra
   `tools\ghidra\gq.js` on Content.dll, found by string; the `game-facts` agent).
2. `Act_NagDistTowards`: what counts as closing, the grace, NAG_OUT_OF_COMBAT in a fight (`docs\FREELANCER_RE.md`
   ~1480: ~63 s held -> FAIL, M09); why the runners survive flying away from Freeport 4 in M04.
3. `Act_Invulnerable = Ashcroft, true, true, 0.5`: probably no death below 50 % until released - then early fire is
   safe for the script; confirm.
4. Ship `Cnd_Destroyed` by label for ships spawned by action (M06 `holeboy1..3`).
5. Juni's lag when the hunters approach (our logs, runner footage).
6. M06 unlock vs banner timing; `PlayerCanDock false` on the hole in `tm_endholers`.
7. L1's lane cost: `route::LearnedLaneSpeed` may be empty on a fresh run (default 2500 m/s).

## 5. Risks
A health-gated / scripted ship engaged early (only engage, never finish; fact 3); nag and tether FAILs (L4 last, facts
first); kind 4 touches every `Goal.kind` consumer (parse behind a flag, L0 first); PSN overlap (one predicate, or the
M13 Cap2 death class returns); L1's cost model is a guess (sprt before any default change); shadow differs by design;
the expected gains are inferences until sprt.

## 6. L0 results (30 Oct 2026; host-free only - no game run yet)
Built, behaviour-neutral (switch `look_log`, default 0; `AUTOPILOT.md` "LOOK L0"):
- `core\game\mission.{h,cpp}`: Goal kind 4 (Cnd_Destroyed of [MsnShip]s by label, else nickname; count -1 = all) and the
  look extras (`ships` with pilot name ids and spawn positions, `healthGate` / `gateFails` from Cnd_HealthDec's immediate
  Cnd_True chain, `invulnerable` / `invSecond` / `invFloor` / `invTrigger` from the script's Act_Invulnerable,
  `laneFrom` / `laneToward` from a Cnd_TLEntered within 4 hops of the goal's chain, `leadsOrder`, `hops`) - only through
  the new `look` argument of `AdvanceGoalsPeek` / `NextStepGoals` / `GoalsAfter` / `GoalsArmedBy` (default false: every
  existing caller and `DecisionSweep` unchanged); `ChainGoals` (forward from the step's setters, 4 hops, any condition, not
  past a new objective); `ShipIdsOf` (MapSpawns); `Tether` / `Tethers` / `ArmedTethers` beside the leashes (not merged).
  ChainGoals resolves a trigger's system as the chain runs (its own `system`, else the system its Cnd_SystemEnter /
  Cnd_NPCSystemEnter / Cnd_SpaceEnter names, else its arming trigger's on the walk): M05's `dst_to_br04_tradelane` has no
  `system` line and `SystemOfTrigger`'s walk up the first arming trigger reaches `key_save_battleship` (Br03), which hid
  it in Br04 - a finding for the decision's own `systemOf` too (GoalsOf), not changed here.
- `modules\controller\src\impl\lookahead.{h,cpp}` (pure): `LookTarget`, `StageablePlace` (PSN: 3 km - an L0 choice, not
  measured - round an unspawned kill ship's spawn and round the declared next objective's point when it is a "Destroy"),
  `PathCost`, `Line` / `Describe` / `InvincibleLines`. The adapter `autopilot_objective.cpp` LookFactsNow / LookLog
  (`ap look`: LookCommand); `space::Invincible` (`core\game\space.h`, the GetInvincible read AtInvincibleFloor now uses).
- Tests: `tests\look_test.cpp` (71 checks, all pass): synthetic kind 4 by nickname / label, gate, invulnerability,
  ShipIdsOf, ChainGoals with hops / lane / order, tethers (`juni, Player, 1500, 30, 2500` and a failing chain); the verdict
  (kill; none for friendly / unspawned / untargetable / pre-hostile / not hostile / dead / failing gate / nag breach /
  leash; the M05-style ring cheaper -> ring, lane faster -> lane; inside / unplaced / leash / PSN for fly; the key; the
  invincible line); the install's m02.ini ("Follow Ashcroft" 22262: `ashcroft_dead` kind 4 -> `tractor`, Ashcroft's spawn
  -28500, 0, 51100, gate 0.35, Act_Invulnerable true, true, 0.5 by `start_stage4_attack`, the kill verdict; ash_nag POS
  radius 100), m05.ini (22890: `player_near_br04_01` hop 1 -> mlog_22894, `dst_to_br04_tradelane` hop 3 -> mlog_22893
  with lane Br04_Trade_Lane_Ring_3; the 40 s / 400 s ride cases), m04.ini (Li01 `juni_tether`, no FAIL of its own; Iw03 the
  three bounty-hunter rows, all failing). `build.cmd test`: 41 tests, 22,806 checks, 0 failures, 0 skipped;
  `decision_baseline` ok; `tools\decision_check.ps1`: decisions unchanged (95 lines); the release build compiles.
- **The next game run must read** (`look_log 1` + `ap preset speedrun`; M02 from a save before "Follow Ashcroft", M05
  through 22890; grep the event log for `autopilot look`):
  - `autopilot look kill ashcroft=<id> trigger=ashcroft_dead -> tractor src=... dist= hull= gate=0.35 inv=0.50 ...` at
    22262 (or `none ... blocked=...: ashcroft_dead/ashcroft:<code>` - which predicate holds him back: `unspawned` means
    neither the spawn position nor the pilot name found him, `pre-hostile` / `not-hostile` the attitude, `nag(ash_nag)`
    the 100 m nag's static test);
  - `autopilot look invincible ashcroft=<id> trigger=ashcroft_dead script=start_stage4_attack:true,1,0.50
    game=<inv>,<b>,<floor> hull=<h> gate=0.35` - **fact 3**: game `1,1,0.500` with the hull staying >= 0.5 under fire
    means the floor holds (then the 35 % gate cannot fire until something releases it); a hull below 0.5 or `0,...`
    answers it the other way. The line repeats only when the look line changes: `ap look` reads the hull on demand;
  - M05 at 22890: `autopilot look fly trigger=<player_near_br04_01|dst_to_br04_tradelane> ... cost= nearest=` - the costs
    with the real ring places and the game's lane ride (`lane=` in `ap look`); the decision itself is unchanged.

## 7. L0 in game (30 Oct 2026 evening; build 487c20c, `look_log 1`; rounds look0b from the new saves + look0c, a full M02 leg)
- **M02 (S419 x2, look0c live):** `look kill ashcroft ... src=armed` for the whole follow (~103-108 s: load 19:33:38 ->
  "Destroy Ashcroft's Ship" 19:35:26; live 19:46:13 -> 19:47:56), blocked only on the step after's own triggers
  (`dist_jump:nag(pos)`, `disable_ash_nag:unplaced`); the ship killed him ~10 s after the destroy banner (pod segment
  118 / 123 / 115.5 s vs par 22: the gain L2 targets). **Fact 3 not answered by the read:** `game=0,1,0.500` both after
  the load and live - the first flag reads 0 although the script passes `true, true, 0.5`; which flag is "invincible" is
  unproven. L2's first probe: fire at him during the follow, read his hull (`ap look` on demand) - below 0.5 = no floor;
  stays >= 0.5 = the floor holds (fight to it, then wait for the destroy step).
- **L2's probe (fact 3 answered, 30 Oct 2026 ~20:05; build 128b90d; `research\experience\runs\look2probe\`, 1 run of a
  budget of 2):** from S419 the ship was put ~350 m beside Ashcroft's path every ~9 s (console `pos`, a test aid) and
  fought him in combat mode. His hull fell under our fire during the follow (1.000 -> 0.986; sparse hits: he cruises
  away at 300 m/s) and, after the destroy banner (~83 s in: the placed ship was within 1 km of him), 0.967 -> 0.926 -> 0.609 -> 0.519 -> 0.433 -> dead, `GetInvincible` (0, 1,
  0.500) unchanged and no script line between: **no 0.5 floor against the player** (FREELANCER_RE "Saves"). No mission
  FAIL (`ash_nag`'s point lies on his path: fighting beside him kept us closing on it). The probe's other finding
  decides L2's reach: **the follow is a tail chase at equal speed** - Ashcroft charges ~8 s, cruises at ~300 m/s (ours:
  300) and stops at his run point ~88 s in; the A runs (look0b-a x2) cruise at 300 m/s straight there and arrive
  ~107 s, the destroy banner comes on arrival (we are within 1 km of him), the kill ~10 s later. He is never in gun reach
  earlier, so a kill branch during the follow can win at most the few seconds between our arrival and the banner
  (~0-3 s) - under the pod segment's detectable effect (spread ~6 s: ~5 s at 12 runs a side). The ~90 s to par lies
  in the start geometry: the runners are ~2.1 km off his spawn **and already cruising** when the 4th escort dies, catch
  him during his ~8 s charge and engine-kill-coast at 110-230 m (fights.tsv); we start 4.6 km off (full leg) at rest
  after the Rogues fight (our M02.ashcroft gold is 26 s under par - the runners spend that time placing themselves).
- **M05 (S422 x2):** at 22890 "Fly to the Trade Lane" `look fly trigger=dst_to_br04_tradelane cost=64.3 nearest=<same>`
  (`flies=2`), with a towards-nag to `Br04_Trade_Lane_Ring_3` (2,000 m); `player_near_br04_01` shows as armed only at
  22893 (`cost=59.6`). LOOK's pick at 22890 equals today's: gap note 5's M05 gain is unproven - re-read the runners' lane
  moment (run 1: "DOCK WITH THE TRADE LANE" 2 s then "DOCK WITH PLANET LEEDS") against the nag before L1.
- **M04:** no fixture - S417 (M04.hunters) was written while the objective read 21660 and loads "Awaiting"; L4 needs a
  rest save before the hunters' step (M04.fp4 dropped: no rest before M04.hunters).

## 8. L2 not built: no case reaches a detectable effect (30 Oct 2026 evening; reviewed by the driver session)
L2's first probe (section 7, fact 3 answered: no 0.5 floor against the player) also sized the slice. Reach = the
smallest effect a 12-runs-a-side sprt detects, 2.5 x sd x sqrt(2 / 12), with the sd over the segment's last 10 full PASS
runs (`splits.tsv`):

| Case | Segment | sd | Detectable | Expected from L2 | Why |
|---|---|---|---|---|---|
| Ashcroft during "Follow Ashcroft" | M02.pod | 5.6 s | 5.8 s | ~0-3 s | a chase at equal cruise speed (300 / 300 m/s) from 4.6-6.2 km behind at rest: he is in gun reach only at his run point, where the banner shows on arrival anyway |
| the holers during "Fly to the Leeds Jump Hole" | M06.holein | 7.9 s | 8.1 s | ~0 s | holeboy1 appears cloaked 5.2 s before "Destroy All Hostile Ships" (gate s7arb3: mark 15:47:32.1, objective 15:47:37.3); `d_holers` arms 6 s after him (`tm_lastholer`); the fight's length is set by holeboy2 / 3 spawning at +3 / +6 s |
| escort2 during "Fly to the Marked Ship" | M02.rogues | 3.0 s | 3.0 s | unsafe | `escorts_destroyed` (`Cnd_Destroyed Escort, 4`) is armed only by `convoy_caught` (inside 2,000 m of AshEscort1): kills before it may never count. L0's `src=chain` verdict at 22.7 km is exactly the case LOOK must refuse. The step's own lever is that fly goal, which L0 reads `convoy_caught:unplaced` (a mission ship by nickname: `ObjectPos` cannot place it) |

So there is no `look_ships` switch and no behaviour code, by the rule "no switch without a case that pays". The L0 verdict
stays log-only. Two findings for later slices: (1) a chain-source kill goal must never be acted on before its trigger
is armed (an L2 predicate if it is ever built); (2) a `Cnd_DistShip` to a mission ship should be placed by its spawn /
pilot name, as LookFactsNow does for kill ships (M02.rogues +28 s gold over par: the 65 s follow of the marked ship).

**The re-target: M02.ashcroft -> M02.pod start geometry** (runners' WHAT / HOW / ADVANTAGE / RISK:
`docs\LEG_DOSSIER.md` M02 "Per segment"). The runners are cruising near Ashcroft's spawn when the 4th escort dies and
catch him during his ~8 s charge. A rule from the script's data, not an M02 case:
- **Stage toward the next step's kill ship's spawn while the current step's last kill is pending**: the current step's
  `Cnd_Destroyed <label>, N` has N-1 down, and its trigger's `Act_SpawnShip` names the step-after's kill-goal ship at a known
  position (MapSpawns). Then bring the last one low and charge cruise toward the spawn, inside `nag_battle3`'s 5 km of
  King (the step's leaving nags).
- **PSN's exception from data** (PSN forbids staging at an unspawned hostile's spawn in general): the spawned ship's first
  directive on spawn is a flight away (`start_run1`: `Act_GiveObjList Ashcroft, ashcroft_stage2` = a goto, no attack
  ObjList), and no other hostile spawns within kPsnRadius of that place in the same chain (Thug1..4 spawn at the run
  point, 25 km away). A predicate `FleeingSpawn(goal)` in mission.cpp beside StageablePlace; anything else stays refused.
- **The chase**: CoastGuard (`big_coast`) generalised to a moving hostile ship. Match him at cruise behind him, kill
  the engine within gun reach + lead, coast firing with guns and missiles (Javelin blast vs 110-230 m: expected outside HDE's
  full splash - not tested), engine back on when the gap opens beyond gun reach.
- **Fixture**: SaveS418 (M02.rogues, 22269 at 5.9 m/s, unverified; its first load judges it; its marks come back -
  King + 3 rogues re-marked in the MARKS-LOAD probe, 30 Oct 2026 - and its [MarkObj] holds ships only). Else a fresh `-SaveAt M02.rogues` from Save05c7.
- **Reach**: M02.pod sd 5.6 s gives 5.8 s detectable, against an expected 60-90 s (runners 18-30 s vs our ~118 s). M02.ashcroft
  (sd ~6; gold 20 s, last-10 mean 26 s, vs par 46) may get slower by the placing: judge the pair of segments together (`--seg M02.pod` plus
  `compare` on M02.ashcroft).
