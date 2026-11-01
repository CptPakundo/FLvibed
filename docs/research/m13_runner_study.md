# M13 runner study: what the top-3 any% runs do, segment by segment, and one "runner shape" for us
Status: history - the 30 Oct study; the living M13 runner study is `runners\M13.md` (with its four notes) - header added 31 Oct 2026
Checked against: FLvibed e4064c9a (31 Oct 2026 ~15:35, header only: research_check listed this note every round for want of one)
Watch: docs/research/runners/M13.md

Study of 30 Oct 2026 (late), after the owner watched td1full: "a deeper analysis and then mimicking of speedrunners is
required to get M13 up to par". The runners' segment times are our **minimum** per segment. Analysis only, no code.

**Sources and tags.** [M] = measured (source given), [I] = inferred. Runner data: `research\evidence\speedrun\reference\`
(`ref.py M13 "<section>"`, `events_dense_run<N>.tsv`; "v" = video seconds), `research\evidence\speedrun\topics\combat\fights.tsv`
(M13 rows 55-66). Ours: `docs\SPLITS.md` 303-319, `docs\checkpoints.tsv` 184-194, `docs\LEG_DOSSIER.md` 1243-1315,
`docs\missions\M13.md` (script), `controller\docs\AUTOPILOT.md`, and the latest full run
`research\experience\legs\2026-10-30_2136_M13-td1full-b_i1\` (td1: `splits.txt`, `FLController.log` at wall-clock
times, `FLMotion.tsv`). Frames looked at: `reference\motion\frames\B2_r1_m13_nomadbs.jpg` (R1 battleship),
`reference\frames\r2_14068.0_M13_24s.jpg` (R2 battleship approach, cut for this study), `reference\motion\frames\B5_r1_m13_city_2s.jpg`
(R1 city), `reference\frames\r1_14398_M13_barrier.jpg` (described in `sections_dense.tsv`).

**The game difference that decides the fights.** The runners play vanilla 1.1. There an explosion that only grazes a ship
deals 1/3 of its damage. In HD Edition it deals all of it (`docs\FREELANCER_RE.md` "Explosion damage"; Server.dll patch 0x960C).
Our own rounds have hurt us at the Nomad battleship: a torpedo whose impact was 80-95 m from us cost 2,424 (one full
torpedo), and missile pairs launched 81-86 m from the centre burst 6-8 m from us, costing 3,434 and 3,836. Our ship's hull
plus shield is 7,344 [M: AUTOPILOT.md 879-890, 972-976, 955]. So **"0 m with every launcher" transfers only where no round
can burst on us**. Approach timing, routes, cruise, ignoring fighters and the consumable rhythm transfer fully.

## 1. Segment table (seconds)

Par, the three runners and our gold come from SPLITS.md 309-319 [M]. td1 comes from `splits.txt` [M]. "Gap" = td1 - par.
Checkpoint names are those of checkpoints.tsv. Note: **M13.sphere is the four barrier generators** (the run-judge's
"barrier +34"); M13.barrier is the Nomad-gate fight.

| # | Checkpoint (segment) | Par | R1 / R2 / R3 | Gold | td1 | Gap |
|---|---|---|---|---|---|---|
| 1 | control (Toledo: launch) | 13 | 28 / 1 / 13 | 10 | 18.0 | +5 |
| 2 | battleship (interceptors until the 24425 banner) | 56 | 49 / 74 / - | 48 | 49.4 | -7 |
| 3 | bsdown (24425 -> 24420: kill + the 25 s cap_timer) | 45 | - / 59 / - | 66 | 95.9 | **+51** |
| 4 | sinclair (24420 -> relaunch / 24440) | 33 | - / 24 / 33 | 31.5 (td1) | 31.5 | -2 (like for like ~+15 vs R2, below) |
| 5 | osiris (escort to the Osiris, 1 km) | 104 | 136 / - / 104 | 119 | 136.9 | **+33** |
| 6 | orillion (Osiris dock, refill, launch) | 29 | 32 / - / 29 | 34 | 35.7 | +7 |
| 7 | gatefight (follow Orillion to the gate) | 78 | 78 / 77 / 78 | 78 | 78.9 | +1 |
| 8 | barrier (gate fight, through the hole) | 89 | 111 / 83 / - | 73 | 84.2 | -5 |
| 9 | gens (to the barrier, until 24485) | 45 | 48 / 48 / - | 47 | 47.5 | +3 (but see 9: the runners use it) |
| 10 | sphere (4 barrier generators) | 65 | 58 / 76 / 65 | 95 | 99.4 | **+34** |
| 11 | end (sphere, the city generators, cutscene) | 129 | 124 / 120 / 129 | 152 | **died** at +157 s | death |

Two kinds of segment matter. **Script-timed** segments (2, 7, 9, and most of 3 and 4) end on a timer or a comm chain whatever
we do. There the runners use the dead time to **get into position, or even start the next fight, before its banner**.
**Action-timed** segments (3's kill, 5, 10, 11) are where seconds and deaths are.

## 2. Per segment

### 1 control: Toledo, launch (par 13, td1 18.0, +5)
- Runners: R3 spends 13 s in the base [M: ref.py "accept -> DEFEND"]. R2 buys its munitions to 50 at the launch [M: count_up v14031.5].
- Ours: restock 21:36:59 -> 21:37:08 and launch at 21:37:43 [M: transcript]. Gold 10.5 proves the harness can be quicker. Not an autopilot item.

### 2 battleship: interceptors until "Destroy the Nomad Battleship" (par 56, td1 49.4, -7)
- Script [M: M13.md 108-114]: `nomadcap` **spawns 36.9 s after T0** (`nomad_timer`). The 24425 banner comes **12.4 s
  later** (49.3 s), after Juni's 0190.
- **R1** [M: ref.py section 2; B2 sheet]: fights interceptors for 20 s, charges cruise at v13875 and cruises at 300 m/s
  for 13 s. It is **at the battleship's hull ("0-M") by v13895, 10 s before the banner** (v13905). It fires Slingshot +
  Catapult at v13896.6, drops mines and **bursts 9 batteries at v13898.6 (27 -> 18)**. In the frames the ship sits
  inside the battleship's blue model.
- **R2** [M: events v14070-14081; r2_14068 sheet]: afterburns at it at ~199 m/s and fires a torpedo + Slingshot +
  Catapult every ~2 s **from 1,358 m, starting at v14070, 12 s before its banner** (v14082). The sheet shows a pass
  along/through the hull, Toledo in view at ~v14081, a turn back, then the hull again from v14083 [I: small tiles].
- **R3** targets it at +39 s at 383-2,200 m [M].
- Ours: fighting interceptors 1-1.4 km from Toledo, **4,923 m from the battleship at its banner**. We spent 3 batteries (29 -> 26)
  on fighters before it [M: FLMotion 21:38:06.9; log 21:37:53].
- Advantage of theirs: 10-12 s of fire before the banner, and the ~25 s our approach takes after it.
- Risk: none for them. HDE: the approach volley from range transfers. R1's 0 m does not (below).

### 3 bsdown: the Nomad battleship's kill (par 45, td1 95.9, +51)
- Segment = kill time after the banner + 25-29 s of script (`cap_timer`) [M: M13.md 128-129; td1 kill 21:39:13.2,
  split 21:39:42]. **Kill after the banner: R1 ~20 s** (v13905 -> ~13925, fights.tsv:55), **R2 ~33 s** (v14082 ->
  ~14115, fights.tsv:56, dossier 1274), **td1 66.3 s** [M].
- **What and how.**
  - **R1:** at 0 m the whole fight. Speed pulses of 50-185 m/s with afterburner bursts. Every launcher every ~2-3 s.
    Mines 10 at once at v13906.6. **Batteries 9 / 10 / 8 at v13898.6 / 13912.6 / 13919.4** (27 -> 0 in 21 s).
    Nanobots 2 [M: section 3 events].
  - **R2:** moving at 90-199 m/s at the hull, torpedoes 11, batteries 10 at v14089.3 and 9 at v14103.6, hull >= 0.86
    [M: fights.tsv:56; events].
- **Ours (td1)** [M: FLMotion + salvo-check lines]:
  1. +0-20 s: 4.9 km at 200 m/s, no cruise.
  2. +20-41 s: a station crawl from 1,060 m to 168 m at 40-50 m/s.
  3. +42 s: an afterburner jab to 95 m.
  4. +48-66 s: still at 124 m (speed 0).
  - Battleship hp: 119,475 at +21 s (the allies had done almost nothing). **3,170 hp/s while we launched from 966 -> 193 m**
    (21:38:28-46: 119k -> 62k). **1,780 hp/s once `self_burst` held every launch "inside 300 m"** (21:38:50 -> 21:39:12:
    40k -> 1.2k; 11 `launch-held` lines). So **half our DPS was given away for the last 22 s**.
  - Batteries 26 -> 0 in 20 s in 7 doses of n 2-6 sized to the current gap; two "repair-effect" readings were negative
    (the shield still falling). Then nanobots at hull 0.83 / 0.59 [M: log 21:38:33-21:38:57].
- **Advantage:** (a) fire before the banner. (b) No crawl: the cover is crossed in ~7 s (AUTOPILOT.md 1050-1053).
  (c) Full launch rate to the kill. (d) Batteries in 9-10 bursts that refill the shield to full on each failure.
- **Risk / why safe for them:** vanilla graze = 1/3 damage. In HDE, launching at 0-150 m is what killed us (mpb22, mpb24,
  probe4-1; AUTOPILOT.md 865-901). The death predictor is **batteries left on reaching 300 m**: >= 7 meant 15 / 15 lived
  (AUTOPILOT.md 1052). Exposure, not battery timing, is the lever (q4s2 REVERT, 1045-1051).
- **Transfers:** the pre-banner approach, the approach volley from range (R2's 1,358 m), crossing the cover fast, and battery
  bursts. **Does not transfer:** launching from 0-150 m. The owner's rule: full DPS, splash avoided by where we fire from
  (AUTOPILOT.md 988-1003).
- **Gap: ~46 s on the kill** (66 vs 20). About 20 s of it is position at the banner, about 10 s the held launches, and the rest the crawl [I].

### 4 sinclair: Toledo dock and relaunch (par 33, td1 31.5)
- Script [M: M13.md 129-143]: `trent_go_back` unlocks the ring **40 s after the kill**. 24430 follows ~5 s later.
  24420 itself is timer-ended, so Cap2 never needs to die.
- **R2** [M: dossier 1274; events v14141-14165]: after the kill it heads for Toledo at 193-199 m/s, docks at kill +40
  (the unlock) and relaunches 24 s after the 24420 banner. **R3:** 0 launches, 0 hull lost, docked +20 s after its banner.
- **Ours:**
  - After the kill we fought 3 fighters, then **NomadCap2 from 1.35 km at a 45 m/s station crawl** for 15 s (shield 0.12).
  - `timer_destroy` staged only at the 24420 banner (21:39:42, 5,287 m out, batt 0). Cruise to Toledo, docked 21:40:13 = **kill
    +60 s** [M: log timer-destroy lines; FLMotion].
  - Our split (24440 0.5 s after the dock) hides that we are ~15-20 s behind R2 like for like [I: R2 docks at kill +40, we at +60].
- **Risk:** none for them (Cap2 never fought; the respawns stop at trent_go_back). Transfers fully. **Gap ~-20 s available.**

### 5 osiris: escort Sinclair (par 104, td1 136.9, +33)
- **All three runners**: 5 s cruise charge, then **95 s of cruise at 300 m/s straight to the Osiris** (29.7 km each).
  0-2 launches, 0 batteries, Sinclair targeted only in passing (R3 at 268-434 m at +24 s, R1 at 1.2 km at +84 s)
  [M: ref.py section 4]. The step ends when the **player** is within 1,000 m of the Osiris (`in_osiris_range`,
  M13.md 167). Sinclair's position does not matter.
- **Ours** [M: FLMotion 21:40:18-21:42:43; log]:
  - 17 s fighting 4 interceptors at the launch (kills 21:40:29-33).
  - `formation` on Sinclair from 2.85 km: 75-96 m/s for 5 s, a catch-up at 360 m/s.
  - Then **22 s at 80 m/s** at 100 m from Sinclair while the shield fell (1.0 -> 0.15; the Pika-wing stretch, `near_pika` M13.md 157 [I]).
  - Then 300 m/s. Mean 236 m/s.
- **Advantage:** no fight, no escort pace. **Risk:** the runbook marks Sinclair's death FAIL and `sinclair_nag` as
  uncertain after 1497 (M13.md 50-56, 158). All three runners leave him and pass [M]. Transfers. **Gap -25..-33 s.**

### 6 orillion: Osiris stop (par 29, td1 35.7, +7)
- Runners cruise into the dock: R3 299 -> 194 -> 13 m/s, docked at the 24445 banner (v14345) [M: section 4 profile].
- Ours: 12 s from the 24445 banner (21:42:30.8) to the dock (21:42:42); afterburner 164 m/s, then 78 m/s. Base 24 s [M].
  This is the `bay_arrive` / fast-dock area (AUTOPILOT.md 859-864). **Gap ~-6 s.**
- Restock here [M: transcript]: 29 batteries, 7 nanobots (to 29 / 29). **This is the second and last consumable budget.**

### 7 gatefight: follow Orillion (par 78, td1 78.9)
- Script-timed. Every run is 89 % cruise, 21-22 km [M: section 6]. At par. Nothing to take.

### 8 barrier: the Nomad-gate fight, through the hole (par 89, td1 84.2, -5)
- **R1:** interceptors and passes over NomadCap3 at 0 m. **Batteries 29 -> 22**, nanobots 1 [M: fights.tsv:58].
- **R2:** docks the hole while the battleship lives, 0 batteries [M: fights.tsv:59; event v14425].
- **R3:** nanobots 22 -> 14, hull dip 0.47 [M: fights.tsv:60].
- **Ours:**
  - Faster than par.
  - But **20 batteries (29 -> 9, by 21:45:00) spent at the gate battleship (2801119619) with `attackers=0`**. Doses n 2-8 at
    21:44:50-59, against the static's turrets [M: log].
  - A 21 s station crawl from 1,321 to 277 m at 42-50 m/s at that battleship [M: FLMotion 21:44:40-59].
- Transfer: R2's "no fight" or R1's "spend 7". **Risk to our leg: the batteries are needed later (sections 10-11).**

### 9 gens: to the barrier (par 45, td1 47.5)
- Script-timed by Juni (`juni_near_center` + 16.9 s of dialogue -> 24485, M13.md 213).
- **R1 uses it:** cruise 34 s straight to the barrier, a full stop, then on a Nomads Satellite / generator from +21 s at
  93-958 m with salvos from +32 s, mines from +35 s. **The first generator dies before the 24485 banner**
  (kill v14382 in the run labels, ~14392 in fights.tsv:61; banner v14397.5) [M: section 8].
- R2 also launches 20 torpedoes before its banner [M: v14494.9].
- **Ours:**
  - Cruise 16 s, then **"fight" on 5 Nomad fighters for 20 s at 3-31 m/s** (kills 21:46:13-34).
  - **Batteries 9 -> 0, hull 1.0 -> 0.83**.
  - Then 24485 found us with no generator engaged [M: log, FLMotion 21:46:05-37].
  - Fighters were ignored by all runners (fights.tsv:61; dossier 1300-1306).
- Runbook caution: killing generator 1 before Juni reaches the centre may skip the objective / fail_range2 swap ("Uncertain",
  M13.md 214). R1 did it and finished [M]. **Gain: one generator (~15-25 s) moved into script time.**

### 10 sphere: the four barrier generators (par 65, td1 99.4, +34)
- **R1** [M: section 9, frame-checked description, fights.tsv:61]:
  - Afterburner ~199 m/s for 6-9 s along the wall to each generator. Full stop ~60-85 m off its hull (HUD surface distance).
  - Every launcher every 2.0 s plus mines while stopped. Generators 2-4 die at v14400 / 14427 / 14452. Fighters ignored.
  - **Batteries 9 + 9 (22 -> 4) on shield failures, nanobots 28 kept** (fights.tsv:61 corrects the older note).
- **R2:** hold 35-83 m with a right slide, batteries 29 -> 20 -> 1, hull 0.96 -> 0.83.
- **R3:** 1-113 m, no batteries aboard, nanobots 14 -> 1, 63 s for four.
- **Ours** [M: log, FLMotion 21:46:37-21:48:16]:
  - **28 s of travel after the banner** ("to-marks" cruise to the first generator).
  - Then per generator: afterburner 200 m/s to ~450 m, a 15-35 m/s station crawl to 143-160 m from the centre (radius 111:
    a 32-54 m hull gap), then a still hold. Kills at +46 / +59 / +80 / +99 s: ~13-21 s per generator once there,
    which is **R3's pace**.
  - The loss is the travel (28 s) plus generator 1 not started in segment 9.
  - Repairs: **nanobots 29 -> 10 in doses of 1-3**, batteries 0 (spent in 8-9), hull dips to 0.54. "static-hold bad ...
    another side" twice [M].
- Risk: torpedoes at 60-100 m in HDE. Our holds at a 32-54 m hull gap did not burst on us in td1 (no big-hit lines before the
  city) [M], and the M12 / M13 holds are tested (AUTOPILOT.md 624-647). The batteries-at-failure rhythm transfers.
- **Gap: -25..-30 s** from no fighter detour plus the generator-1 start, and the nanobots kept for the city.

### 11 end: the Dyson city (par 129, td1 died)
- **R1** [M: section 10 description; B5 sheet; fights.tsv:64]:
  - Cruise ~45 s straight to the first satellite. Cruise cut ~3 s out.
  - **A dead stop inside the satellite's cone, 0-70 m from the generator sphere.** Every launcher + mines every ~2 s.
    It blows at ~v14532.
  - Cruise charge at once, ~20 s to the second, the same. Last generator v14575.5.
  - ~20 fighters ignored. 4 batteries + nanobots 28 -> 18.
  - In the frames the ship sits in the dark underside of the cone, the orange generator filling the view (v14517-14531, v14559-14575).
- **R2:** both satellites from **inside the model (0 m)**, batteries 11 -> 0 [M: fights.tsv:65].
- **R3:** ~50 s of cruise across, then 0 m [M: fights.tsv:66].
- Two final generators end the step (`watch_endgame_object`, M13.md 239).
- **Ours** [M: FLMotion 21:48:16-21:50:53; log]:
  - Entered with **hull 0.81, 7 nanobots, 0 batteries**.
  - The airlock run at 200 m/s, then a cruise charge at 80 m/s under fire (hull 0.95 -> 0.72).
  - Cruise 300 m/s to generator 1. **Held 406-409 m from its centre** (radius 915, hull gap 48-113 m; "guns 3 of 5"),
    attackers 5-12, `big-hit lost=2386`, hull 0.72 -> 0.43. Killed 21:50:01.
  - Cruise to generator 2, **held 549-562 m from its centre with guns 1-2 of 5**, died 21:50:52.9 (`player died`, 2 kills).
- The city fixture shows the fight itself is solvable: `big_coast` + close hold 4/4 PASS 126-136 s **from 0 batteries**
  (AUTOPILOT.md 1089-1090). **What killed td1 is the entry state and the far hold** [I]: no batteries, 7 nanobots, hull
  0.72 after the airlock charge, and a hold where 1-3 of 5 guns bear.
- Risk: R1 / R2 at 0 m inside the satellite launch torpedoes: in HDE a burst there is full damage. The mpb21 death was our
  own rounds bursting 52-60 m off a city generator's hull, the generator centre ~430 m behind it (AUTOPILOT.md 843-849, 1088).
  Transfers: the route (cruise straight, ignore fighters), the position under the cone with every gun on the target, and
  arriving with a budget. Does not transfer: ordnance from 0 m.

## 3. The consumable budget (one plan across the leg)

Budget windows [M: transcript restocks]: **A** = Toledo -> Osiris, 29 batteries + 29 nanobots. **B** = Osiris -> end,
29 + 29. No refill in between (the Toledo stop restocks nothing in our harness; the runners only pick up loot).

| Window / phase | R1 batteries (nanobots) | R2 | R3 | td1 |
|---|---|---|---|---|
| A: interceptors before the battleship | 0 (3) | 0 | 0 | 3 |
| A: Nomad battleship | 27 in 3 bursts (2) | 19 in 2 (2) | 24 in 3 (2) | 26 in 7 doses, then nanobots |
| B: Nomad gate | 7 (1) | 0 | 0 (8) | **15** |
| B: to the barrier (fighters) | 0 | 0 | 0 | **9 (+ hull 0.83)** |
| B: 4 barrier generators | 18 in 2 bursts (0) | 28 | 0 (13) | 0 (19 nanobots, 1-3 at a time) |
| B: city | 4 (10) | 11 (5) | 0 (~22) | 0 (7 left, died) |

[M: events per section; fights.tsv 55-66; log repair lines.] **Rule this sets** [I]:
- Window A goes to the battleship alone.
- Window B: <= ~7 at the gate, 0 on fighters, the barrier in 9-bursts on a failing shield, and **>= ~10 batteries + >= ~15
  nanobots entering the city**. That is R1's shape: it entered with 4 + 28.

## 4. Ranked mimic changes (one "runner shape" design)

Expected seconds are against td1 [I], built from the gaps above. Fixtures: `docs\missions\scene_saves.tsv` has **one fit
M13 save, SaveS415 (M13.battleship, 24425, 29 / 29)**. S349 / S406 / S409 are unfit, S348 / S350 unverified. Later
segments need the full leg from S415 or Save3989.

| # | Change (what the runners do) | Evidence | Expected effect | Touches | Cheapest test |
|---|---|---|---|---|---|
| 1 | **Battleship: be there at its spawn, not at its banner.** Cruise toward `nomadcap` when `nomad_timer` spawns it (36.9 s after T0); the approach volley from <= 1.4 km (R2) during the 12 s before 24425; cross the turret cover at cruise / afterburner, never a station crawl | R1 0 m 10 s before the banner, R2 firing 12 s before; td1 4.9 km out at the banner, a 21 s crawl | bsdown -15..-25 s; fewer deaths (time inside the cover drains the batteries: >= 7 left at 300 m = 15 / 15 lived) | `prestage_next` / `mission::NextStepGoals` (a spawn the script times), objective mode; `big_coast` (cruise in), `blind_rush` (OFF; its launch-hold side effects, AUTOPILOT.md 921-934) | S415 has the banner already up, so a fixture saved ~30 s after T0 is needed. Until then: full leg `-StopAt M13.bsdown` from Save3989 |
| 2 | **Full DPS to the kill from a site where rounds cannot reach us** (the owner's rule): keep launching at the refire rate from a ring >= the measured safe gap (we had 3.2k hp/s from 966-193 m and launched nothing inside 300 m); guns at the contact / blind hold between salvos | td1 hp series (half DPS for 22 s); muzzle bursts at 81-86 m; q4s1 site stood 14-63 m, not 150 | bsdown -8..-12 s; risk neutral or better if the site is truly >= the burst distance | Q4 slice 1: `ordnance_site`, `ordnance_standoff`, `launch_site_check` (fails open on long flights), `self_burst` / `self_burst_damage` (its "inside 300 m" lesson held 11 checks) | S415 `-StopAt M13.bsdown`, 12 a side (sigma 26.6: only effects > ~20 s show, so combine 1+2+3 as one set) |
| 3 | **Batteries in runner bursts on shield failure, with a leg budget**: A all to the battleship; B <= 7 at the gate, none on fighters, 9-bursts at the barrier, >= 10 + 15 nanobots kept for the city | Section 3 table; td1 spent 3 + 15 + 9 off-target and entered the city with 0 / 7 | Removes the city death mode (td1) and the barrier nanobot drain; 0 s alone, enables 4-6 | `battery_reserve` (works since a7d3c31, keeps 70 % only near a marked big static), `repair` / `battery_horizon`, `static_volley` (REVERT alone: bursts sooner with the same exposure) | Full leg from S415 (log repair lines + entry state at 24475); judge by the city-entry consumables |
| 4 | **Osiris: cruise straight to the Osiris, no formation, no launch fight** (the step ends on the player within 1 km of the Osiris) | All 3 runners: 95 s at 300 m/s, 29.7 km; td1 17 s fight + 22 s at 80 m/s | osiris -25..-33 s; risk: Sinclair FAIL / `sinclair_nag` uncertain (M13.md 50-56) but the runners pass | objective mode escort words -> formation (AUTOPILOT.md 500-508, 573), `Rule_Escort`, `follow_own`, `escort_radius`; a new switch (e.g. escort-to-place when the step's end is a place trigger) | S415 `-StopAt M13.osiris` (~4 min) |
| 5 | **Barrier: no fighter detour; generator 1 started in the script time before 24485** (R1 killed it before the banner) | R1 v14382-14392 vs banner 14397.5; td1 20 s on 5 fighters, 28 s travel after 24485 | sphere -25..-30 s; the batteries kept (item 3) | the objective mode's threat recheck at 24470 (`reason=threat`/`recheck` every 3 s), `prestage_next` (the generators spawn at `in_story`), `static_hold` | Full leg `-StopAt M13.sphere` from S415 (no fit St03 save; S406 unfit) |
| 6 | **Cap2 window: stage at Toledo's ring from the first kill and dock at the unlock** (trent_go_back = kill +40), not at 24420 | R2 docked at kill +40; td1 docked at kill +60 after crawling at NomadCap2 | sinclair -15..-20 s; less hull / no Cap2 shots | `timer_destroy` (extend its stage from the kill: 24425's next step is the timer step 24420), `mission::StepAdvanceOf` | S415 `-StopAt M13.sinclair` |
| 7 | **City: hold where all guns bear** (under the satellite's cone, close to the generator sphere), ordnance only from a launch-safe site; cruise straight from the airlock and do not charge cruise under fire at the door | R1 / R2 inside the cone at 0-70 m; td1 holds 406-562 m from the centre with guns 1-3 of 5 | Survival plus city ~130 s (fixture 126-136) | `big_coast`, `static_hold_guns`, `static_hold` re-anchor, the airlock run (`airlock_fly_through`) | The city fixture of AUTOPILOT.md 1089 (M13_ck_city, 0 batteries; not in scene_saves.tsv as fit); else the full leg |
| 8 | Osiris dock at cruise into the bay | R3 docked at the banner; td1 12 s from 1 km | orillion -6 s | `bay_arrive`, fast dock (AUTOPILOT.md 859-864) | S415 `-StopAt M13.orillion` |

**Sum if all land** [I]: bsdown ~-25..-35, sinclair ~-15..-20, osiris ~-25..-33, sphere ~-25..-30, city alive at
~130 (vs PB 152), orillion ~-6. That puts the leg near 730-760 s, against par 686 and PB 862. Par needs every item.

**Order** [I]:
- (a) 4 and 6 first: pure flight, no HDE risk, ~45 s, S415 fixtures.
- (b) 3 + 5 together: the budget only shows over the whole of window B.
- (c) 1 + 2 as one switch set at the battleship. It is the biggest and riskiest; q4 showed that single switches drown in sigma 26.6 s.
- (d) 7 last, on the entry state that 3 + 5 produce.

## 5. Open questions

**For a game run / game-facts:**
1. Does damage before the 24425 banner count on `nomadcap`? `freighter_dies` makes it `Act_Invulnerable ... true, true, 0.9`
   (m13.ini 1417, M13.md 110). The runners fire before the banner; whether the 0.9 floor binds them or only non-player
   damage is not read.
2. Does killing barrier generator 1 before `juni_near_center` leave the 24485 / `fail_range2` swap unfired (M13.md 214)?
   R1 finished, but our objective reader must cope.
3. Leaving Sinclair: are Sinclair's death FAIL and `sinclair_nag` live after `trent_go_back` (M13.md 50-56)? All runners leave him.
4. What is the launch-safe distance at the Nomad battleship and the city generators in HDE? Our bursts were seen 6-95 m from
   us, and one round's centre was unexplained (AUTOPILOT.md 887-898). Item 2's ring needs a number from logged
   `own-end` / `big-hit` lines, not the runners.
5. Our loadout (Anubis; `missile01_mark03`, `torpedo01_mark01`, mines, CM) vs theirs (Slingshot + Catapult + Starkiller
   torpedo): is our launcher DPS equal? Their kill times assume their DPS.

**For the owner:**
1. Is the battery / nanobot budget of section 3 (city entry >= 10 batteries + 15 nanobots) the right trade, given R3 does
   the barrier with nanobots only?
2. Leaving Sinclair and fighters behind (items 4, 5) is speedrun knowledge. Should it go only into the speedrun preset
   (AUTOPILOT_UX 2a)?
3. The city: is "inside the cone, guns only, ordnance from outside" acceptable as the HDE version of R1 / R2's 0 m?
