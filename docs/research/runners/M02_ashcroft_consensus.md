# M02 Ashcroft (M02.ashcroft -> M02.pod -> M02.nygate -> M02.nygatein): escalation step 1 - the autopilot's view

Escalation step 1 of `docs\AFK_PLAYBOOK.md` 13a: the autopilot side, written 31 Oct 2026 by a read-only analyst (no
build, no game). It reads the runners' side from `M02.md`, the ASH-START code (chase_kill / spawn_stage) and the B-arm
probes as2-as4. [M] = measured (log / motion trace / code), [I] = inferred. Times are seconds; "+n" = after the "Follow
Ashcroft" banner (22262) unless said otherwise. Why escalated: the section ends 50-70 s off par (pod + nygate + nygatein:
runners 174 s, our PB 243.2), and ASH-START (as2-as4) was NET WORSE (`M02.md` top paragraph).

Runs read (all under `research\experience\legs\`): B arm (chase_kill 1 + spawn_stage 1) `2026-10-31_0611_M02-as2-b_i5`,
`2026-10-31_0614_M02-as3-b_i4`, `2026-10-31_0632_M02-as4-b_i2`, `2026-10-31_0632_M02-as4-b_i5` (as4 = after e83af45b,
"no dodges while chasing"); A arm (both off) `2026-10-31_0540_M02-as0-a_i5` and the PB `2026-10-30_1610_M02-gate-s7arb3_i3`.
Each folder: `FLController.log` (events), `FLMotion.tsv` (ms, speed, cruise / engine state, target, tgt_m, x, z, shield),
`splits.txt`.

## The runners' view (summary of `M02.md` "## M02.ashcroft" / "## M02.pod" / "## M02.nygatein")
- They bring the last escort low (0.09) and leave it to King / the police, and are **already cruising at ~300 m/s near
  Ashcroft's spawn (-28500, 0, 51100)** when `escorts_destroyed` fires: r3 843 m from him at the banner, r1 <= 2.1 km.
  R1 even sits 14 s, then cruise-charges while the 4th escort still lives. M02.ashcroft costs them 46-60 s [M].
- They catch him **during his own ~8 s cruise charge** (75 m/s at 0.8 s, 294 m/s at 9 s: LOOK_PLAN 138-146) and sit
  45-230 m behind him (medians 262 / 119 / 173 m), guns online by **engine-kill coasts at cruise speed** (R2 297 -> 283
  m/s in 4 s, firing at ~120-150 m: `d10_r2_ashcroft_ek.jpg`), Javelins r1 2 / r3 1, ~6 s of fire in total; the pod at
  +18..+29 s, tractored at once [M fights.tsv 2-4, ref events].
- The price: the pod lies 40-44 km from the NY gate (ours ~30.5 km from his run point), so nygatein costs them 118-125 s
  vs our 84-106 [M]. Ashcroft + pod + nygate + nygatein: r1 220, r2 227 s [M splits_reference 30-33].

## The autopilot's view
### What the code does, from the 3rd escort's death to the pod (chase_kill 1, spawn_stage 1)
1. **The stage** (`autopilot_objective.cpp` 2089-2148, `lookahead.h` 278-298): with one escort alive it plans P = 200 m
   behind the spawn on his line + 250 m beside it, clamped into 0.8 x King's 5 km `nag_battle3` - and nothing else (the
   Cnd_DistShip leashes are not in the clamp: AUTOPILOT.md 484) [M code]. Beyond 1.5 km of P: a Travel trip; within
   1.5 km: a **Hold at rest** at P with the nose along his run (`DecHoldAt`, `StageNose`, autopilot_frame.cpp 1309-1326);
   the last escort within 1.5 km of P: a normal fight on it, wherever it leads [M code].
2. **The chase** (`ChaseCandidate` 884-899, the branch 2034-2067): at the Follow banner Ashcroft is fought (Mode::Combat).
   Per frame `lookahead::ChaseStep` (lookahead.cpp 577-597): **Charge (cruise after him) only when his speed > our engine
   top (80 m/s)**; until then the plain fight (no cruise: he is inside 3 km). Coast (engine kill, `ChaseCoast`,
   autopilot_frame.cpp 1215-1306) only with us in cruise, >= 150 m/s, him within 30 deg and **110-250 m** away; ends past
   0.9 x gun range, shield < 0.15, gone or 30 s; then engine on and a new cruise charge [M code].
3. Game facts that bound it [M `docs\FREELANCER_RE.md` 467-476, 1045]: a cruise charge runs ~5 s at the engine's 80 m/s,
   then ~3 s of ramp: 300 m/s ~8.5 s after the press, **weapons offline while charging**; engine kill ends cruise and holds
   the speed (no drag). Ours from the press to 300 m/s in these runs: 7.5-8.5 s [M FLMotion, below].

### Measured per run (B arm)
| | as2-b_i5 | as3-b_i4 | as4-b_i2 | as4-b_i5 |
|---|---|---|---|---|
| stage trip / hold at P / last-escort fight (s) | 12.1 / 15.1 / 8.4 | 44.6 / 10.2 / 7.2 | 44.9 / 7.1 / 4.6 | 44.0 / 11.1 / 2.2 |
| leash detour on the trip ("leash: ~2.4-2.5 km from 13 (leash 3000 m) - back toward it") | none | 06:25:53.442 | 06:43:38.383 | 06:43:51.249 |
| our speed at his spawn (the `objective chase` line) | 105 (afterburner) | 186 (afterburner) | 48 | 0 |
| distance to him at the banner (`d_spawn` = `dist`) | 740 m | 810 m | 1,227 m | 1,147 m |
| our cruise press (FLMotion engine 1) / at 300 m/s | +5.4 / +13.9 | +5.4 / +14.0 | +2.0 / +10.1 | +2.1 / +9.6 |
| gap when both cruise (FLMotion tgt_m) | 1,495 m | 1,515 m | **1,425 m for 80 s** | **1,423 m for 80 s** |
| cruise drops in the chase | +15 (`stop why=no enemies left`), +32, +52 (evade / bumps); gap 3,090 -> 4,878 -> 5,535 | +15 (same stop), +34, +52; gap 3,045 -> 4,745 -> 7,586 | none | none |
| `autopilot coast start` lines | 0 | 0 | 0 | 0 |
| Javelins at him | 2 at the run point (one abandoned at 5,290 m) | 1 at the spawn (845 m), 2 at the run point | 1 at the spawn (1,217 m, missed: closest 1,377 m), 4 at the run point | 2 at the spawn (1,147 / 1,161 m), 3 at the run point |
| gun range entered (`head-on ... dist=659`) / shield down | +91.8 / +93.1 | +118.8 / +120.6 | +97.6 / +102.0 | +97.3 / +101.2 |
| kill (= pod segment) | 123.0 | 131.3 | 111.2 | 111.3 |
| pod tractor delay (`spawn-loot` -> `tractor marked loot`) | - | 0.5 s (78 m) | 0.07 s (132 m) | 0.4 s (148 m) |
| M02.ashcroft / pod / nygate / nygatein (splits.txt) | 62.6 / 123 / 21.5 / 88.5 | 74.8 / 131.2 / 22.2 / 85 | 66.6 / 111.2 / 21.5 / 91 | 72.7 / 111.3 / 23.1 / 90 |
| the four together | 295.6 | 313.2 | 290.3 | 297.1 |

A arm for comparison [M]: as0-a_i5 kills the 4th escort itself at the banner (05:51:19.906), `travel plan arrive -22099 0
75896 eta=107.6` (05:51:21.442: the navmarker, never Ashcroft), kill 05:53:19.942 = +120.0, tractor 0.02 s later; the PB
(`M02.md` 142) at rest 4.69 km off, cruising from +13 s, kill +115.2. Ashcroft + pod + nygate + nygatein: as0-a_i5 274.7,
PB 268.1.

### What the numbers say
1. **The chase is a tail chase at equal speed, whatever the stage did** [M]: the gap freezes at 1,423-1,515 m in all
   four runs, from starting gaps of 740-1,227 m, and stays (as4: 1,423-1,427 m from +10 to +90 s, both at 300 m/s). Its
   size is set by **when our cruise charge starts relative to his**: ChaseStep charges only once he is faster than our
   80 m/s engine, i.e. after his own charge has ended (as2 / as3: his gap starts growing at +5.4..+6.4, our press at
   +5.4) - so we always reach 300 m/s ~4-5 s after him, and he gains ~1-1.1 km in that time [M FLMotion; the
   mechanism I from lookahead.cpp 591-596]. In as2 the gap fell 745 -> 419 m in the first 5 s (we at 80 m/s with the
   afterburner, he ~75-80 m/s charging) and then opened to 1,495 m [M].
2. **He is caught only at his run point** (25,609 m run: `flees 25609 m` in the stage line), where he drops cruise at
   ~+91 s (as4-b_i2 FLMotion +90 -> +92: 300 -> 81 m/s); then an 18-20 s fight beside his thugs (`odds ... measured
   ships=5`, 06:45:56.410) [M]. So **pod >= ~110 s for any chase that does not kill him en route** (as4: 111.2 / 111.3 =
   that floor; the A arm 114-120 is 3-9 s above it) [I from M]. The pod segment barely moved; nygatein's 85-91 vs A's
   105-106 is a difference this analysis did not explain (not the chase: the pod is tractored at the same run point).
3. **The stage costs the 35-45 s, and two thirds of it is a leash fight** [M]: in 3 of 4 runs the trip to P (clamped to
   4.0 km from King, inside `nag_battle3`) crossed 80 % of the mission's 3 km ship leash on King (`objective leash Li01_03
   3000 m: ship (kept)`; the supervisor's trip rule, autopilot_objective.cpp 3060-3070), turned back to King and re-charged
   cruise: as4-b_i2 FLMotion -46 s -> -34 s cruising back 2.5 km, then a second 8.5 s charge; trip 44-45 s vs as2's 12 s
   without the detour. Then a 7-15 s hold at rest, and the last escort fought where it was: **at his spawn we were
   742-1,238 m off it, not at P's 320 m**, at 0-186 m/s (the hold is at rest by design; StageNose turns at rest).
4. **The chase did drop out of cruise** before e83af45b [M]: as2 / as3 at +15 s with `stop mode=combat why=no enemies
   left` (06:23:36.819, 06:26:52.747; the target lost at a 1,527-1,563 m gap, `haveEnemy` false at
   autopilot_combat.cpp 2712 - which input failed is unread [I]), then evade / bump drops (fixed by e83af45b). as4: none.
5. **The engine-kill coast at cruise speed has never run** [M: 0 `coast start` lines in the five B logs]: the 110-250 m
   window was never reached. Whether our engine kill at 300 m/s holds ~285-297 m/s like the runners' (vanilla 1.1) is
   **unmeasured in HDE** (WORK_QUEUE 96, A36); `FREELANCER_RE.md` 468 records only "106 m/s from cruise: 104-108" - its
   context is not clear and may mean the kill does not hold cruise speed [doubt].
6. **Kill time once in reach** [M]: at the run point gun range -> kill 13.6-18 s, shield down -> kill 9-10 s (821 shield,
   ~3.6k dealt: `stop mode=combat ... dealt=3600 dps=176-181`), two forward guns. A single coast of ~12-14 s must
   therefore kill him; a re-cruise costs a full charge (~8.5 s) during which he gains ~1.4 km [I from FREELANCER_RE 471-473].
7. Missiles: 1-2 Javelins wasted at 1.1-1.2 km at his spawn in as3 / as4 (launched at 80 m/s at a target leaving at 300)
   [M]; none during the chase (weapons offline in cruise) [M].

## Agreements / differences
- **Agree:** the gap is the start geometry and timing, not the chase logic or the fight (M02.md 142, LOOK_PLAN 8); the
  runners' "already cruising when he spawns" is the decisive move; at equal cruise speed nothing closes a 1.4 km gap [M].
- **Agree:** the coast is the only way to fire at cruise speed (weapons offline while charging / cruising) [M game text].
- **Differ (refines the runner view):** "cruising at his spawn" is not needed to reach the coast window: **both cruise
  charges take ~8.5-9 s** (his 294 m/s at 9 s, ours 7.5-8.5 s), so **if our charge starts at his spawn from <= ~300 m
  behind him on his line, the gap at the lock stays ~150-300 m** [I from the measured charges]. Our charge started 2-5.4 s
  late (the `hisSpeed > engineTop` gate) from 740-1,240 m - both halves of the 1.4 km.
- **Differ:** our weapons go offline the moment we press cruise, so we cannot charge before the last escort dies while
  killing it ourselves; the runners leave it to King / the police (non-deterministic: unverified who killed it in each
  run, M02.md 131). Charging at the kill is the deterministic version; its cost is ~0.3 s of reaction, not 5 s.
- **Unique to us:** the leash detour (3 km King leash vs P clamped at 4 km) costs ~25-30 s in 3 of 4 staged runs; the
  runners stay near King.

## The ONE design: `spawn_charge` (bool, default 0; Source::Hidden; requires chase_kill + spawn_stage)
**Built 31 Oct 2026, not flown** (`controller\docs\AUTOPILOT.md` "spawn_charge"; pure half `lookahead.h` StagePoint over
a leash list / StillFleeing / ChargeGap / ChargeKeep / ChaseFacts::fleeing, `tests\spawn_charge_test.cpp`). As built:
side 70 m; the ship leashes = the supervisor's held one (`g_obj.leashShip`) + the live table's armed script leashes;
the fight from P keeps station when > 400 m off P; the coast against a fleeing ship starts at 270 m/s (not 150: the
window is reached in our charge's ramp from this gap). Two findings while building [M code / script]: (a) the predictor
on the profiles below gives a lock gap of ~58 m for a press WITH his from 260 m (we gain ~200 m: our ramp leads his by
0.5 s), +150 m per 0.5 s of our lag - so the 150-300 m below holds for a ~0.3-0.8 s reaction, and a lag-0 press locks
under the coast's 110 m (no coast); (b) the "3 km King leash" is `need_to_dock` (m02.ini 2207-2211: Cnd_DistShip
outside Player, Li01_03, 3000 -> FAIL 22365), armed only by comm_1330_king (DX_m02_1310_KING, Li01) at the mission's end;
`mission::LeashesOf` reads it in every step (no arming; only a trigger's own `system` filters, and it has none) and
DecideBody anchors it on the fallback ship (the step's friendly target / the survival lead: King [I from the log's
"ship (kept)", not "(named)"]) - so clamping to it at 0.75 (2,250 m) with King ~3.7-4.6 km from the spawn leaves no point
within 600 m: the stage is refused (`spawn-charge none (far ...)`, tests\spawn_charge_test.cpp M02 case). The fix that
frees the point is the decision's leash arming (the live table), not this switch.

Charge at his spawn from the stage point, inside every leash:
1. **The stage point on his line, inside the ship leashes:** P = spawn - 250 m x u + 60-80 m x n (a crossing margin, not
   250 m: at a 200 m gap 250 m aside is 51 deg off the nose, outside the coast's 30 deg) and the clamp takes the minimum
   of 0.8 x every leaving nag **and 0.75 x every armed ship leash** (`mission::Leashes`, here Li01_03 3,000 m on King) -
   the supervisor's own 80 % trip rule then never fires. If no point within the leashes lies <= 600 m from the spawn: no
   stage (fail closed to today's fight).
2. **The last escort fought from P:** in the stage's fight verdict the combat keeps within 400 m of P (station at P, guns
   on the escort as it attacks; the 25 s timeout stays the fallback), so at the kill we are <= ~400 m behind the spawn
   with the nose along u.
3. **The charge at the spawn:** ChaseStep gains the fact `fleeing` (his first orders are the FleeingSpawn goto, already
   read by spawn_stage) - with it, Charge at once whatever his speed (no `hisSpeed > engineTop` wait), steering onto his
   line during the 80 m/s phase; no missile at a fleeing chase target beyond 600 m (the wasted spawn Javelins).
4. The coast as built (110-250 m, guns, the 0.5-hull missile hold), now reachable.

**Pure half (unit-tested in `tests\`):** `lookahead::ChaseStep` with `fleeing` (fleeing + not cruising + gap > 110 m ->
Charge even at his 0-80 m/s; not fleeing -> today's verdicts); `lookahead::StagePoint` with the side / back constants and
a list of leashes (each anchor + radius x fraction) - the point inside all or a refusal "no-point-in-leashes"; a predictor
`ChargeGap(gap0, our_lag_s)` = gap0 + the speed-profile difference (his: 75 m/s for ~5.5 s, ramp to 300 m/s at ~9.5 s;
ours: 80 m/s 5 s, 300 m/s at 8.5 s) to log the expected lock gap beside the measured one.

**Expected [I]:** lock gap ~150-300 m at ~+9 s; coast from ~+10 s; kill in one ~12-14 s coast if the kill holds speed
-> pod ~25-35 s (-80..-90); ashcroft ~40-50 s (no leash detour: as2's 12 s trip; +15..+25 vs A); nygatein +35..+45
(the pod 10-13 km farther from the gate). Net vs the A arm -10..-40 s on the four segments (A 268-275 -> ~235-260;
runners 220-227).

**Probe (no S418: `docs\missions\scene_saves.tsv` 57 UNFIT - a stall at rest after the Rogues' fight):**
- **P0 ANSWERED from existing data (the driver, 31 Oct 2026 ~15:20) [M]: the speed holds** - M13-ab12-b_i1's
  `big_coast_own` coast (an engine kill in cruise): `coast start ... speed=300` 05:02:39.181, `coast end ... speed=287
  after 6.8 s` 05:02:46.021 (HDE; FREELANCER_RE.md updated). Gun fire during such a coast: still to see in P1.
- (the original P0, kept:) **P0, the A36 fact, before any build (~1 min, any fit open-space save):** cruise to 300 m/s, engine kill, read FLMotion
  speed for 10 s and fire the guns. Pass: >= 270 m/s after 4 s and the guns fire. **If the speed falls to ~100 m/s, the
  coast at cruise speed does not exist in HDE: stop, and the design changes to the runners' other half (be ahead of his
  charge; see Risks).**
- **P1, the mechanism (n=1, S419 = M02.ashcroft fit, the Follow step, ~2 min):** the console `pos` test aid (as the L2
  probe, LOOK_PLAN 7) puts the ship 250 m behind his spawn on his line at load; chase_kill 1, spawn_charge 1. Judge from
  the log + FLMotion: the cruise press within 1 s of `objective chase`; the gap at the lock (both 300 m/s) <= 300 m; a
  `coast start` by +15 s; speed >= 270 m/s through the coast; `target-shield down` during the coast; kill <= +40 s.
- **P2, the leg (n=1 then a set, `Save05c7.fl` full leg, `-StopAt M02.nygatein`):** the `spawn-stage ... P=` line inside
  the 3 km leash and no `leash ... back toward it` during PreStage; `objective chase ... d_spawn <= 400`; ashcroft <= 50,
  pod <= 40, the four segments <= 250 (A 268-275). Verdict by `tools\splits.py sprt` against the A arm, stated as distance
  to par (220) and to the PB (268.1 on the four).

## Risks
- **The engine kill at cruise speed (top):** unmeasured in HDE; the runners' 297 -> 283 m/s is vanilla 1.1; FREELANCER_RE
  468's "106 m/s from cruise" may say it does not hold [doubt] - P0 decides before anything is built.
- **Fighting the last escort from P:** the escort may chase King, not us; then the stage times out (25 s) to today's fight
  (fail closed, but slow: the as2-as4 cost again). A missile finisher (a Javelin at the low escort, then the cruise
  press: the hit 1.1-1.9 s later, `missile-outcome` flights) would also start our charge ~1.5 s before his - a later variant.
- **One coast must kill:** 2 forward guns need ~10-14 s on him; a short coast (shield < 0.15, gap past 540 m) means a
  re-charge and the 1.4 km gap again. Our hull / shield are full here (the runners' safety too) [I].
- **Ram / overtake:** charging from 250 m behind as he accelerates from rest: we never pass him (our charge is not faster
  than his until ~8.5 s) [I]; the 60-80 m side margin keeps the nose clear.
- **The "no enemies left" stop at ~1.5 km** (as2 / as3) is unexplained [I]; the design keeps the gap far below it, but a
  failed charge would meet it again.
- **HDE vs vanilla:** a Javelin at 110-230 m from the coast is expected outside its blast, untested [I]; 22355 (a round
  on the pod) is guarded by the 0.5-hull missile hold.
