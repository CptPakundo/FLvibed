# The gap to the runners, by kind of segment
Status: current
Checked against: FLvibed 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 1 watched-path commit (06c44292: AUTOPILOT.md logs moved to AUTOPILOT_LOG.md, LEG_DOSSIER.md pointer repointed; probe sp1's legs.tsv rows); no figure changed; the kind decomposition still awaits `gapreport.py decompose`; the stale M13 row unchanged (SPLITS 15:10: M13 PB 744, sum of best 692))
Dirty paths at check: another session's uncommitted `controller/src/impl/autopilot_impl.h`, `controller/src/impl/autopilot_objective.cpp`, `controller/src/impl/autopilot_settings.cpp`, `controller/src/impl/lookahead.{cpp,h}`, `core/fluf.cpp`, `tests/fluf_probe_test.cpp`, `tools/setwatch.sh` (not judged) (31 Oct 2026 ~15:25)
Watch: docs/checkpoints.tsv, docs/splits_reference.tsv, docs/LEG_DOSSIER.md, tools/gapreport.py,
controller/src/impl/autopilot_dock.cpp, controller/src/impl/autopilot_objective.cpp, controller/src/impl/autopilot_combat.cpp
Evidence: `research\experience\legs\splits.tsv` (our segment history), `docs\splits_reference.tsv` (par), the gate
s7arb3 legs' `FLController.log`s (`research\experience\legs\2026-10-30_1[56]*-gate-s7arb3_i*`), the runners' material
(`docs\LEG_DOSSIER.md`, `research\evidence\speedrun\topics\combat\`, `navigation\`, `reference\sections_dense.tsv`).
Log readings were made with throwaway scripts (not kept); each number below says what it counts so it can be redone.
Sources: our own leg history and the runners' recordings' tables [file]; no outside pages

**Question (the owner, 30 Oct 2026):** success = a leg significantly closer to par, at par or beating it; gates and
failed speed attempts are the top time sinks. Where does the leg time go against the runners, and which ONE general
mechanism per kind closes the most of it?

## What it means for FLvibed (first screen)

*31 Oct 2026 ~04:00 (fc379a3, short form):* `docs\SPLITS.md` (03:24) now reads M13 par 686, **PB 755 (+69; ab8-b_i1, 754.6 s, n=1; was 891 / +205 in the table below)**, sum of best 703 (+17), all legs PBs 12,046 (+1,998), golds 11,314 (+1,266). The table row for M13 and the M13 figures quoted from it below are the 30 Oct values; the kind decomposition (fights 48 %, jumps 25 %, lanes 12 %) was not recomputed - M13's gain is a fight gain (the barrier / battleship / city), so fights' share of the remaining gap falls, the order of the kinds is not known to change. Re-run `tools\gapreport.py decompose` at the next whole-set round.

*30 Oct 2026 ~23:30 (5eb7bcb, short form):* M01b.end now ends at GAME SAVED (9e4ec5b: the 16 s false gap of section 7
removed). Mechanism 1's "required set" can be empty: a "Destroy ..." step the script ends on a timer (M13 24420; 9fed936
`timer_destroy`, default 0) is staged, not fought - set td1 M13.sinclair 27.4 vs 57.3 s; with `td_window` /
`escort_by_place` (5eb7bcb, run-judged GOOD) M13.sinclair + osiris 134-141 s vs par 137, AT PAR. Section 7's largest rule
is overtaken for M02 (LOOK L2 closed unbuilt, 1e71507: the start geometry, not the follow) - marked in place.

The owner (30 Oct 2026): a gate result is good only when a leg came significantly closer to par, reached it or beat
it. Over the 14 legs the recent medians are **+2,518 s** over par (sum of per-segment medians; the per-leg best set
medians the owner quoted are lower). By kind of segment: **fight +1,214 s (48 %), jump +633 s (25 %), lane +292 s
(12 %)**; base, dock, cruise, escort and scripted waits together +379 s (15 %). Three general mechanisms, one per top
kind:

1. **Fight - the objective kill cycle** (runners' R1-R4 + R8): cruise in, cut ~100 m short, hold at 0-60 m of the hull
   *surface* with every gun and launcher, never break off for attackers the step does not need (consumables pay), leave
   on the kill. Expected **~300-450 s per full run** (the measured kill-phase excess over the runners' same fights, +
   the approach leads; inference). Cheapest test: checkpoint saves at `M09.gens` / `M11.satellite` / `M13.gens`, then
   segment arms with `splits.py sprt --seg`.
2. **Jump - enter at speed**: no shortcut-rush / formation shortcut at holes and gates in legs; the request goes in at
   the fast-dock point at speed. Measured: in leg logs the formation shortcut took us in **0 times** (122 "no effect"
   on 28-30 Oct); the rush costs **91 s per gate** over its own ETA (s7arb3, 17 rushes). Closes ~16-25 % of the jump
   kind; the rest is post-jump routing (mechanism 3). Cheapest test: a switch for it (none exists) + the M07
   `M07.tau29` save -> `M07.end` (three holes, five rushes in s7arb3).
   **[Contradicted 30 Oct 2026, bb9163a - section 4 "Tested ... PARKED":** the switch was built (`shortcut_rush`,
   51fd767) and one M07.tau29 pair showed no gain (M07.cali 104.1 vs 102.3 s, kyushuhole 84.8 vs 84.6): the at-speed
   request is refused while an NPC occupies the hole, so the "91 s per gate" counted the occupant's time, not the rush's.
   `shortcut_rush` stays 1; the jump kind's remaining lever is mechanism 3.]
3. **Lane (and the jump kind's remainder) - trigger-route planning with look-ahead**: choose among the step's armed
   triggers the path with the least time to the step *after* next, act on the script's lock / permission changes
   rather than on banners, and pre-stage during dialogue inside every armed nag and leash. Evidence: M05's lane is
   optional (`player_near_br04_01`), M06's Leeds hole unlocks ~6.8 s before its banner, M07 Tau-31 runner 3 reaches
   "Defend" 44 s after the closed-lane objective. Expected ~100-200 s (inference). Cheapest test: offline first
   (`ap goals` / the decision sweep), then saves at `M05.leedshole`, `M07.gate`.

## 1. The decomposition (`gapreport.py decompose`)

Method: per segment, the median of the leg's last 10 full passes (its start save, PASS, end checkpoint reached) minus
par (the golden run, scene-free); gold gap = our best segment ever minus par. Kinds come from `checkpoints.tsv`'s
`segment` text and `ours` column (`control` -> base, `kill:<n>` -> fight); the first activity the text names decides
(the texts name a segment's parts in order). `--list` prints each segment's kind and the keyword that decided it.

| Kind | Segments | Par s | Ours s (median) | Gap s | Share | Gap of segments over par | Gold gap s |
|---|---|---|---|---|---|---|---|
| fight | 48 | 2992 | 4206 | +1214 | 48 % | +1281 | +593 |
| jump | 34 | 1822 | 2455 | +633 | 25 % | +643 | +444 |
| lane | 19 | 1086 | 1378 | +292 | 12 % | +314 | +201 |
| dock | 8 | 169 | 265 | +96 | 4 % | +98 | +61 |
| base | 29 | 1292 | 1388 | +96 | 4 % | +181 | +47 |
| cruise | 27 | 1188 | 1269 | +81 | 3 % | +171 | -33 |
| escort | 22 | 1320 | 1386 | +67 | 3 % | +138 | -4 |
| scripted | 6 | 179 | 217 | +39 | 2 % | +49 | +35 |
| **all** | 193 | 10047 | 12565 | **+2518** | 100 % | +2874 | +1344 |

The kinds: **base** a base visit (room, restock, Esc, launch, space-enter scene); **fight** killing / defending;
**escort** pace set by a companion (meet, formation, follow, shed); **lane** trade-lane rides; **jump** gates and holes
including "X hole -> Y" (transit and the flight on in the next system); **dock** approach and docking; **cruise**
open-space travel to a point; **scripted** spawn / banner / scan / tractor / scene waits. Gate s7arb3 alone: fight
+1,204, jump +566, lane +276, base +89, dock +86, escort +72, cruise +67, scripted +36.

Top 15 single segments (recent median - par): M04.mactan +127 (fight), M07.sinclair +101 (lane), M02.pod +98 (fight),
M03.end +80, M13.sphere +80, M09.tekagi +71, M07.fleetdone +69, M01b.end +61, M13.bsdown +53 (all fight), M06.end +49
(jump), M11.osiris +49 (jump), M07.gate +48 (fight), M07.cali +47 (jump), M02.cosmo +44 (scripted), M09.patroldown +43
(fight).

**Caveats.** (a) Segment boundaries are objective events, and ours and the runners' do not always fall at the same
moment, so a gap moves between neighbours: M04.hunters -43 / M04.mactan +127 (the runners fly to Mactan inside their
hunters segment, `LEG_DOSSIER.md` M04 "All three"), M02.cosmodone -19 / M02.lane2 +24 (the Cosmo scene falls on
different sides), M08.enemies -53 / M08.escort +22 / M08.kyoto +41. Kind sums partly cancel this; single segments do
not. (b) The runners' "TAKE THE JUMP HOLE" banner can come after their tunnel: M06 runs 1-3 spend 42-48 % of
"DESTROY ALL HOSTILE SHIPS -> TAKE THE JUMP HOLE" off the space HUD (`reference\sections_dense.tsv` rows M06 run 1
line 142, run 2 line 469, run 3 line 783), i.e. they jump before the objective shows (section 5). (c) A mixed segment
("Sprague fight, dock") counts for its first part. (d) The recent window mixes builds for legs flown rarely (M13: 7
passes).

## 2. Gate s7arb3 vs s5shadow (`gapreport.py gate s7arb3 --prev s5shadow`)

Totals are the end checkpoint's time (scene-free, from the accept; M01b ends at GAME SAVED, so 536 s, not the 635 s
leg total of `gate_s7arb3.tsv`). *[Corrected 30 Oct 2026, 9e4ec5b: the 536 s was objective 22055's time, 15.8-18.1 s
after GAME SAVED (section 7); M01b.end is now `save:32120` (GAME SAVED itself): s7arb3 520.6 s, +64.]*

| Leg | Total | Par | Gap | Reference (gap) | Verdict |
|---|---|---|---|---|---|
| M01a | 502 | 409 | +93 (+22.8 %) | s5shadow 508 (+99) | SAME |
| M01b | 536 | 457 | +79 | s5shadow 568 (+111) | CLOSER (-32) |
| M02 | 1099 | 876 | +223 | s5shadow 1109 (+234) | SAME |
| M03 | 876 | 763 | +114 | s5shadow 870 (+108) | SAME |
| M04 | 939 | 812 | +126 | s5shadow 999 (+186) | CLOSER (-60) |
| M05 | 1144 | 952 | +192 | s5shadow FAILED; best set median M05-s11p5-o 1244 (+292) | CLOSER (-100) |
| M06 | 697 | 489 | +208 | s5shadow 684 (+196) | SAME (+12) |
| M07 | 1673 | 1250 | +422 | s5shadow 1668 (+418) | SAME |
| M08 | 655 | 582 | +73 | s5shadow 676 (+94) | CLOSER (-21) |
| M09 | 758 | 530 | +228 | s5shadow 731 (+201) | FURTHER (+27) |
| M10 | 1078 | 914 | +164 | s5shadow 1092 (+177) | SAME |
| M11 | - | 978 | - | - | FAIL at M11.nygate (player death, 1173 s) |
| M12 | 414 | 348 | +65 | s5shadow 394 (+45) | FURTHER (+20) |
| M13 | 891 | 686 | +205 | s5shadow FAILED; best set median M13-oddsab-b 936 (+250) | CLOSER (-45) |

Summary: 13 finished legs, sum of gaps +2,194 s vs +2,410 s for the references = -216 s; CLOSER 5, SAME 6, FURTHER 2,
FAIL 1. No leg at or beat par. Against the best set medians instead of s5shadow (`gate s7arb3`): CLOSER 8, SAME 3,
FURTHER 2, FAIL 1, -327 s.

## 3. Fight (+1,214 s, 48 %)

**What the runners do / how** (`topics\combat\notes.md` section 2): static solar targets - cruise in, brake ~100 m
short (290 -> 2 m/s in 1.2 s), hold at ~0 m/s 0-100 m from the hull, every gun and launcher, leave the moment it dies
(R1, lines 133-139); afterburner below ~1 km between targets, cruise above (R2, 140-143); fighters ignored while
working a static objective, their fire paid with batteries then nanobots (R3, 144-148); capital ships the objective
needs at 0 m or inside (R4, 149-154), not the Rheinland cruiser (153-154); leave on the banner (R8, 164-166). Per fight
(`fights.tsv`): M09 four generators 36-40 s, M11 satellite + fighters 42-72 s (satellite dead in ~6 s at 0 m, run 2),
M13 order base 69-81 s, M10 Sigma-13 42-48 s, M05 Baxter 57-63 s.
**Advantage:** the kill phase is short because damage is maximal (point blank: every round lands, salvos) and
nothing else is shot. **Risk / safety:** paid with consumables (M13 battleship: 27-29 batteries -> 0-3 in all runs,
`notes.md` 81-86). On **HD Edition** explosions deal full damage regardless of distance to big ships and bases
(`docs\FREELANCER_RE.md` 398-411: Server.dll patch, BinaryEdits line 135; our own torpedo cost us 2,424 at 80-95 m) -
point-blank torpedo / missile use carries a self-damage risk the vanilla runners did not have; one M09 run died at full
hull to its own torpedo on the generator's rock (`controller\docs\AUTOPILOT.md` 84-88).

**What our autopilot does** (`controller\docs\AUTOPILOT.md` 563-583 `static_hold`, 701-719): a hold point
`static_hold_gap` off the hull with a lateral slide, launches gated by `blast_flank` / line-of-fire checks, a held
static target "kept until it dies unless the hull is under 35 %" - yet it "survives detours to other targets"
(581-582). The logs of gate s7arb3 show where the fight time goes (throwaway scan of `autopilot start mode=combat`,
`target id=`, `target-destroyed`, `tactic=`, `launch-held` events per fight segment):
- **Kill phase over the runners' same fights** (first combat start -> last kill vs `fights.tsv` durations): M09
  generators 85.9 s (+46..50), M11 satellite 112.2 s (+40..70), M07 fleet 174.3 s (+57..66), M13 order base ~134.5 s
  (bsdown 85.1 + the battleship segment 49.4; +54..66), M05 Baxter 95.3 s (+32..38), M05 Leeds hole 78.1 s
  (+12..36), M08 platforms 79.7 s (+14..29), M07 blockade 72.7 s (+10..25), M10 Sigma-13 61.1 s (+13..19), M04
  hunters 44.4 s (+8..17), M05 Sprague 43.0 s (+4..13), M13 barrier 84.1 s (-3..+21), M03 Valkyries 23.9 s (~0).
  **Sum ~+295..+425 s.**
- **Shots at what the step does not need:** M09.tekagi 17.4 s on two fighters between generators (targets 26 and 28,
  16:33-16:35 in the M09 s7arb3 log); M11.rendezvous held 250 m from the satellite's centre (`static-hold ... point=248
  m from the centre`, radius 195) with six `launch-held ... a solid (class 0x1) in the way` in 20 s, then 56 s on
  fighters; M13.end 76 s on ships among the city's generators (runners ignore ~20 fighters, R3); M01b.end 59 s on
  scrambled fighters after the two platforms (runners: platforms only, `LEG_DOSSIER.md` M01b run 1-3).
- **Evade instead of fire:** 531 s of `tactic=evade` against 2,748 s of `engage` over the gate (15 %); 151
  `launch-held` events against 290 launches.
- **Not kill-phase:** M04.mactan's 121 s after the last kill is the trip to Mactan (routing: mechanism 3 / dossier M04
  "the step's meeting comes to us"); M02.pod's 103 s before any kill is the Ashcroft chase (runners: chase at cruise,
  engine kill to fire, 18-21 s; `notes.md` 116-117, NAV P4) - a chase rule, not this mechanism.

**Mechanism - the objective kill cycle (one rule set for every required kill):** the step's required set (the mission's
marks / `mission::AdvanceGoals` kind-3 destroy goals, `core\game\mission.h` 24-31, 40-45) is fought to the end in one
cycle - cruise in and cut at stopping distance + hold gap; hold at 0-60 m of the hull **surface** where the most guns
bear (the `static_hold_guns` probe already exists, AUTOPILOT.md 711-713); every launcher at refire unless the round's
blast would reach us (`blast_flank`'s geometry, not a stand-off); no target switch and no evade while shield +
batteries + nanobots cover the measured incoming DPS (the `odds=` model already logs `their-dps` / `our-hp`); unneeded
attackers only below a hull floor; leave on the kill at cruise. Capital ships by class (cruiser 150-450 m, R4). It is
the owner's "full DPS, avoid splash by where we fire from, survive by consumables" and the runners' R1-R4 + R8 as one
behaviour, general over generators, platforms, satellites, capitals and the step's marked ships.
**Expected:** most of the ~300-425 s kill-phase excess plus the static approaches (M13.sphere 36 s and M09.tekagi 28 s
of travel before the first combat) - **~300-450 s per run (inference)**; the HDE blast rule may cap the torpedo part.
**Cheapest test:** checkpoint saves at `M09.gens`, `M11.satellite`, `M13.gens` (none exist: `docs\missions\
scene_saves.tsv` has leg-checkpoint saves only for M07, M12 and M13.battleship; `flgate.ps1 -FreshSaves` now orders
the missing ones by `gapreport.py rank`), then `flset.ps1 -StartCp M09.gens -StopAt M09.tekagi` arms with the cycle
on / off and `splits.py sprt <A> <B> --seg M09.tekagi`; the fight matrix (`flfights.ps1`) for general play. Saves made
in flight load at rest (TESTING.md 205-210): fit for A/B, not for par.

## 4. Jump (+633 s, 25 %)

**What the runners do / how** (`topics\navigation\notes.md`): cruise at once after every launch and jump exit (median
9 s to >= 290 m/s, rule 1, lines 50-53); jump holes at cruise without stopping - 26 of 44 hole entries kept >= 250 m/s
with no wait; the stops are scripted waits all three share (Tau-29 26-60 s, Tau-23 / Kyushu 3-18 s, Chugoku 16-17 s;
rule 4, 64-68); gates straight in at cruise when free, queued when busy (Omega-3 12-13 s in all runs; rule 5, 69-73).
The formation trick appears once in 47 gate entries (run 3, the California gate in FT2; section 2, 100-122).
**Advantage:** no seconds at the mouth. **Risk:** none seen for holes (no queue outside scripted waits, notes P1 line
219); at a busy gate the queue costs the same 12-24 s either way.

**What our autopilot does:** the fast dock's shortcut-rush starts when an NPC going in would reach the entrance first
- full speed to the entrance, no request, formation on a locked-in ship, called off after 5-10 s at the entrance or
30 s (`controller\src\impl\autopilot_dock.cpp` 915-947); `ShortcutPlace` admits **jump holes** as well as gates
(126-130), although the kb rule says "jump gates only" (`docs\kb\navigation-and-hazards.md` 113) and the user's trick
was "jump gates and trade lanes for sure, jump holes maybe" (94-95). Measured in the leg logs (throwaway scan pairing
each `shortcut-rush ahead` with the next `fast-dock request`):
- gate s7arb3: 17 rushes (13 at holes, bay-len 302), rush -> request 3.4-15.2 s, **excess over the rush's own
  `our-eta` 91 s** (5.3 per rush), 7 requests at speed 0; all 30 Oct gates: 78 rushes, 360 s; 28-30 Oct legs: 131
  rushes, 780 s.
- `formation-shortcut taken in`: **0 lines in every archived leg log**; on 28-30 Oct 125 tries (`busy-by ...
  locked in`) and 122 `no effect after 6 s: requesting, the game queues`. The kb's "4 of 4 taken in" came from the
  20 Oct navigation sweep at the Li01->Li02 gate (`navigation-and-hazards.md` 124-126), not from legs.
- Examples: M05.end - rush 8.7 s at the Leeds hole, request at speed 0 (+28.6 s); M07.cali - "time up at the
  entrance" after 8.3 s; the M07 Kyushu hole - four rushes in a row, ~50 s (15:49:44-15:50:34).

**Tested 30 Oct 2026 ~17:40 - PARKED, the measure was wrong:** one pair from SaveS210 (M07.tau29), `shortcut_rush`
1 vs 0: M07.cali 104.1 vs 102.3 s, M07.kyushuhole 84.8 vs 84.6. The no-rush arm's at-speed request at the Tau-23 and
Kyushu holes was answered `response=1` while an NPC was in the hole and granted on the re-request 8 s later
(M07-jrs-b log 17:26:05 / 17:29:08); the rush arm parked and requested at 0 m/s. Both wait for the same occupant, so
"the rush +91 s over its own ETA" counted the hole's occupancy, not a cost of the rush. `shortcut_rush` stays 1 (no
change). Open, small: the formation trick's 0 / 125 in legs - not a time cost while the occupant decides.

**Mechanism - enter at speed (as drafted):** the request goes in at the fast-dock point at speed for every hole and gate; the
game's own queue answer (reply 3) decides a wait. The formation shortcut stays a manual-play trick (its success in
legs is 0 of 125). Its switch: `shortcut_rush` (51fd767, default 1 = the rush; the A/B M07-jrs runs since 30 Oct 17:16). **Expected ~90 s per
full run** (measured excess, a lower bound: it leaves out the re-acceleration after a stop) = ~16 % of the jump kind
(s7arb3: +566 s). The rest of the jump kind is what happens after the jump: M05.end flies the optional lane
(section 5), M06.end jumps only after the banner and waits under "Awaiting" (15 s hold in Leeds), M10.hamburg flies the
waypoint chain as five 6-21 s legs (60 s vs the runners' 16-30 s, NAV notes line 190 / sections.tsv) - mechanism 3.
**Cheapest test:** the switch, then segment arms from the M07.tau29 save (unverified, 77 m/s) to `M07.end` (Tau-29,
Tau-23 and Kyushu holes; five rushes in s7arb3), `splits.py sprt --seg M07.cali`; the offline excess count above re-run
on each new gate is the free check.

## 5. Lane (+292 s, 12 %)

**What the runners do / how:** lane entries at cruise or afterburner, the ring takes them; lane sections equal ours
(`navigation\notes.md` rule 3, 59-63: "lanes are not where we lose time"). Where a lane is optional they skip it: M05
after the Leeds hole run 3 flies "FLY TO THE TRADE LANE" 33 s and docks 11.8 s later, run 1 shows "DOCK WITH THE TRADE
LANE" for 2 s then "DOCK WITH PLANET LEEDS" (`reference\sections_dense.tsv`, M05 sub-sections). M07 Tau-31: run 3
cruises on toward the mission waypoint and reaches "Defend" 44 s after the closed-lane objective (`LEG_DOSSIER.md` M07
note 5, 559-572). **Advantage:** whole lane rides and dialogue waits disappear. **Risk:** the script's nags - M07's
v2 pre-stage failed the mission ("You failed to dock with the Trade Lane": the objective's own `NagDistTowards` toward
the closed ring, dossier 569-571); same rules in vanilla and HDE (mission scripts; not an HDE difference as far as
read).

**Where the lane kind's gap is** (s7arb3 logs): mostly not riding. M07.sinclair +101 is the dialogue-gated Tau-31 chain
- 26 s under "Awaiting" before the script goal, a 30 s wait at the lane "closed both ways (the mission's lane
permission): waiting at it", 13 s more under "Awaiting" later (M07 s7arb3 log 15:44:44-15:47:58). M02.lane2 (+24) is the
Cosmo scene (27 s) plus a follow, M05.follow (+25) holds a 53 s space scene - boundary shifts (section 1a). M10.holstein
(+39) is the Fischer shed and the dock; M01a.end (+36) the Pittsburgh ring.

**What our autopilot does:** among the step's flying goals it takes "the mission's success first, then the nearest"
(`controller\src\impl\autopilot_objective.cpp` 957-1008). At M05 "Fly to the Trade Lane" that picked
`dst_to_br04_tradelane` (the log's `travel plan arrive 7544.5 -21.4 55522.5`, the point 7890, 25, 56125 within 3,000
m) over `player_near_br04_01` (within 4,000 m of Leeds' ring, which skips the lane: `docs\missions\M05.md` 286), so
the leg flies 58 s to the lane point, rides, and docks 109 s after the objective (M05 s7arb3 log, +66.2 -> +175.5).
At M06 the Leeds hole is unlocked by `tm_endholers` two seconds after the last kill, while the objective comes after
two more comms (`docs\missions\M06.md` 240-241, ~8.8 s); our ship held 8.6 s and then flew 1.6 km to the hole.

**Mechanism - trigger-route planning with look-ahead:** cost each armed trigger's path by the time to the step after
next (via `GoalsArmedBy` / `GoalsAfter` / `NextStepGoals`, `core\game\mission.h` 76-84, 153-157), not by the nearest
trigger; treat the script's lock and permission actions (LockDock unlock, PlayerCanTradelane, PlayerCanDock) as the
moment to act; while the script talks, pre-stage toward the next step's goal inside every armed towards-nag distance and
leash (`TowardsNagOf`, `Leashes`, mission.h 85-98, 140-152) - the constraint the M07 v2 probe broke. General (script
data, no mission ids); speedrun knowledge where it uses a trigger the player was not told about (AUTOPILOT_UX 2a).
**Expected:** M05.end ~50-60 s (run 3: 45 s from "Fly to the Trade Lane" to docked vs our 109 s), M07.sinclair up to
~50-85 s (if the nag-bounded line exists), M06.end ~10-15 s, the jump kind's post-jump remainder (M10.hamburg ~30 s
if its waypoint chain has a shortcut - not read) - **~100-200 s per run (inference)**.
**Cheapest test:** offline first - `ap goals` at the M05 step (debug command, `autopilot_settings.cpp` 627) and the
decision sweep (`tools\decision_check.ps1`) show which triggers the planner would pick, no game needed; then a save at
`M05.leedshole` (`-SaveAt`) and `-StartCp M05.leedshole -StopAt M05.end` arms, `sprt --seg M05.end`; M07 from the
`M07.gate` save (unverified, 0 m/s) to `M07.sinclair`.

## 6. Verified and not

- **Verified (data read this session):** the decomposition and the gate table (`gapreport.py`, selftest OK); the
  rush counts and the 0 formation successes (leg logs); the M05 / M06 trigger facts (runbooks); the s7arb3 timelines
  quoted (M05.end, M06.end/holein, M07.cali/end/tau31/sinclair, M09.tekagi, M11.rendezvous, M13.sphere, M01b.end,
  M10.hamburg, M02.missouri/lane2); the code paths cited.
- **Not verified:** every "expected" number (inferences from logs and runner tables, no run); whether the formation
  shortcut can work in legs at all (it never logged a success); whether M01b's base explosion scene waits for the
  scrambled fighters (if so, those 59 s are required); whether M10's Hamburg waypoint chain can be skipped; whether the
  runners' "TAKE THE JUMP HOLE" banner shows at tunnel exit in general (seen for M06 only); the kind of a mixed segment
  (first-named rule); the HDE explosion rule's cost at point blank (FREELANCER_RE marks it "not verified in game").
- **Ideas for `IDEAS.md`** (for the working session): the three mechanisms above; a `shortcut_rush` switch; the
  boundary-shift caveat for per-segment verdicts (`gapreport.py gate` lists the top-3 segments, read neighbours too).

## 7. Fight phases, measured (30 Oct 2026 ~17:30, `research\experience\legs\fight_phases_2026-10-30.md`)

The 12 biggest fight segments (+775 s on gate-run medians) by phase: **kill speed ~+271 s (35 %)**, **placement and
exits ~+287 s (37 %**: before the first shot +124, after the last kill +93, not separable +70), script pacing /
checkpoint definition ~+102 s, no runner phase split ~+108 s. Proxies: weapon fire is not logged (first shot = the
first trace row in gun range in a fight mode; M13 the first missile); M13 has 3 passes.
- **Placement / exits own the two biggest:** M04.mactan 98.7 of 141 s after the last kill (the trip, the dock
  objective wait, a queued dock); M02.pod 105.9 s before the first shot (to the "Follow Ashcroft" point before the
  destroy step shows) - the runners fight both on the way. Net of the neighbours' under-par boundary shifts (M04.hunters
  -42.8, M02.ashcroft -20.1): ~+84 / +77.5 s.
- **Kill speed owns six:** M03.end (100.7 s of kills vs ~22), M09.tekagi (71 vs 37), M13.bsdown (65 vs ~42), M07.gate,
  M06.hole, M09.patroldown - different causes (wave spawns, static holds, the capital approach, stand-off).
- **Script-paced:** M11.nygate 133.2-133.4 s in all 5 runs whatever our last kill (67-133 s): only the Phoenix health
  trigger moves it (`docs\missions\M11.md` 238-243). **Checkpoint definition:** M01b.end's objective 22055 comes 15.8-18.1
  s after GAME SAVED (checked: s7arb3 i3 16:50:32.6 -> 16:50:48.4), the runners' end IS GAME SAVED - a 16 s false gap. *[Fixed 30 Oct 2026, 9e4ec5b (CP-M01b): a checkpoint kind
  `save:<ids>`, M01b.end = save:32120; over every archived run only the 15 full M01b runs' M01b.end rows move (-15.7..-18.1
  s): PB 536.4 -> 520.6, gold 94.9 -> 79.2 (gold - par +41 -> +25).]*
- **Largest single rule:** act on the next step's goal while the current step only waits (attack the followed ship a
  later step names as the destroy target; fight on the way to the next destination) - ~155-220 s over M02 / M04, more
  than any kill-speed rule; companion tethers are the known blocker (dossier M04). Runner-up: reach firing distance at
  cruise and brake there (M13.bsdown / .sphere, M09.tekagi: 24-39 s to the first shot).
  *[Contradicted in part 30 Oct 2026, 5d21d21 / 1e71507 (LOOK L2 closed unbuilt, `controller\docs\LOOK_PLAN.md` 8): for
  M02 the follow is a tail chase at equal cruise speed from 4.6-6.2 km behind, so attacking Ashcroft during "Follow
  Ashcroft" wins ~0-3 s; M02.pod's ~90 s is the start geometry (the runners are cruising near his spawn when the 4th
  escort dies) - re-targeted as ASH-START (IDEAS A22). The M04 half not tried.]*

## Use
`gapreport.py gate <tag> [--prev <tag>]` after every gate (the owner's verdict words: BEAT PAR / AT PAR / CLOSER /
SAME / FURTHER), `rank <leg>` for `flgate.ps1 -FreshSaves`, `decompose [--matrix|--list]` for the target choice.
