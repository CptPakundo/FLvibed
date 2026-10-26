# Leg dossier: the top-3 any% runs vs our autopilot, leg by leg

What this is (user, 22 Oct 2026): one page per speedrun leg to steer the Advanced Autopilot's training towards the goal -
**match or beat every leg of the top any% run, per leg and run-wide**. Per leg: what happens, the times, how each
runner makes the time, what our autopilot does there, and the ranked things to copy or fix. Legs as in `docs\LEGS.md`
(speedrun.com IL boundaries: accept = the offer popup closes; land = the docking animation's last frame).

**Times blocks are generated - never type numbers between the `legtimes` markers.** `node tools\legtable.js` (run by
`controller\tools\flleg.ps1` after every leg) fills each mission leg's block from `docs\legs_reference.tsv` (the three
runs, best of 3, scene-free) and `research\experience\legs\legs.tsv` (our PASS runs from the leg's start save, with the
personal-best chain). L00 and the FT legs have no autopilot leg yet: their small tables are static (the runs' `legs.tsv`).

**Numbers in the prose are dated evidence**, not live state: section seconds come from
`research\evidence\speedrun\topics\navigation\sections.tsv` (our best leg runs as of 22 Oct 2026 vs the runs), the
autopilot's state from `controller\docs\AUTOPILOT.md` at commit 4c7d3fe (items marked *untested* there are untested here).

**Keeping it current:** when a training result, a new leg best or a user correction changes the picture of a leg, add a
dated one-line note under that leg's "Key notes for training" (newest first) and correct the bullets it outdates. The
user's reading of footage outranks every automated reading (`reference\calib\CALIBRATION.md`); a behaviour claim from the
traces counts only after its frames were looked at (cite the sheet).

Evidence shorthand (all under `research\evidence\speedrun\` unless a path says otherwise; `v=` = video seconds of that run):
| Tag | File |
|---|---|
| R1 / R2 / R3 | `runs\run1_TM4S13\` / `run2_OldOak\` / `run3_SydMontague\` (`notes.md`, `legs.tsv`, `economy.tsv`, `loadouts.md`) |
| NAV | `topics\navigation\sections.tsv` (section gaps), `notes.md`, `docks.tsv` |
| CMB | `topics\combat\notes.md`, `fights.tsv` (22 fights x 3 runs) |
| REF | `reference\` - `ref.py <leg> [<section>]` (`research\tools\ocr\.venv\Scripts\python.exe -I`), `sections_dense.tsv`, `data\verified_desc.tsv`, `frames\` |
| BEH / CAL | `reference\motion\behaviours.md` (frame-backed manoeuvre rules) / `reference\calib\CALIBRATION.md` (user corrections) |
| SR / IDEAS | `docs\research\speedrun.md` (V1-V11) / `docs\research\IDEAS.md` (A1-A33) |
| AP / LEGLOG | `controller\docs\AUTOPILOT.md` / `research\experience\legs\legs.tsv` and the run folders beside it |

Reading rules that apply everywhere (CAL): the HUD target distance is a **surface** distance (0-M against a hull); the
vanilla HUD's left strip box is **thruster energy**, not the throttle; "MOUSE FLIGHT" is a steering toggle, not an
autopilot state. Run 3's in-view target labels and its Catapult / mine counts are weak (REF README accuracy table):
runs 1 / 2 are used for those fields here.

## Index: the biggest lever per leg
| Leg | Title | Biggest lever (details in the leg) |
|---|---|---|
| L00 | The Freelancer Arrives | Esc every room scene; shop in the accept visit (chain not built) |
| M01a | Escort to Pittsburgh | the two fights (Order wing, Rogue lane ambush) and the last lane / ring |
| M01b | Defend XT-19, Raze the Rogue Base | unsplit (no log): rerun with logs; platforms only |
| FT1 | First Money Gate | micro-trades on M01's own path or one depot pod; level-up fires on the transaction |
| M02 | The Unity, Cosmo and Ashcroft | Ashcroft: cruise chase + engine kill to fire; scan Cosmo at once |
| FT2 | Ship and Kit for M03 | ship upgrade + level-up by a buy / sell; hold management |
| M03 | Artifact Smuggling | Willard fight, CA Minor companion shed, transport 2's artifact pods (money) |
| FT3 | Balboa Wreck and Manhattan | wreck / pod loot fires the gate in space |
| M04 | Escape from Liberty | fly straight to Mactan (hunters come to you); Leeds hole -> Leeds |
| FT4 | Optronics to Cambridge | one big trade, Crusader route choice |
| M05 | Sprague and Baxter | leave the post-Baxter fight on the kill; no hole / gate hold-shorts |
| FT5 | Diamonds, Gold and Torpedoes | trade chain + torpedo launcher / CM dropper |
| M06 | The Race and Glorious | Glorious defence and the gunboat with torpedoes, dock on the banner |
| FT6 | The Order Revealed (Leeds) | room scene only: Esc |
| M07 | Through Rheinland to Kusari | fleet fight in the battleship's lee; rerun with logs |
| FT7 | Gold to New Tokyo | one trade + Barracuda (run 1) |
| M08 | Tekagi's Transport | fly at the Honshu hole, not the waypoint (script goal) |
| FT8 | Light Arms to Kyoto | one sale fires the level-up |
| M09 | Tekagi's Arch | the Tekagi patrol and the generators (hold-and-salvo) |
| FT9 | Kyoto Restock | offer + ammo only |
| M10 | The Experimental Shipyard | experimental ship from inside its core; the Osiris dock stall |
| FT10 | Aboard the Osiris | room scene only: Esc |
| M11 | Prison Break | base stops, the satellite at 0 m, the Order ships after the prison |
| FT11 | Orillion's Briefing | room scene only: Esc |
| M12 | The Nomad Lair | ignore "destroy all Nomad ships" - fly to Toledo; the 600 s collapse clock |
| FT12 | Toledo | room scene only: Esc |
| M13 | Into the Dyson Sphere | Nomad battleship at 0 m, barrier / city generators by hold-and-salvo |

---------------------------------------------------------------------------------------------------------------------

## L00 - The Freelancer Arrives
New Game -> the Freeport 7 intro movie -> Manhattan arrival -> Juni's bar scene -> Accept "Mission 1" (Juni, Manhattan
bar). No flying. The runners' leg is mostly unskippable vanilla scenes; on HDE the intro skip and Esc on room scenes
remove almost all of it. The equipment dealer visit before the first launch belongs here or in M01a, depending on the run.

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 228.7 | 139.6 | 143.0 | 139.6 |

autopilot: not built yet (FT1 money step probe only). The one chain attempt from New Game (22 Oct 09:02) failed in M01a
("You never entered formation", LEGLOG).

**How each runner does it**
- Run 1: watches the intro + arrival (v=12-170), manual save + load to skip the walk-in (v=175-187), bar scene watched,
  accept v=240; sells 3 nanobots + 3 batteries (+1,200), buys a Justice Mk II (R1 notes (c)).
- Run 2: quits to the main menu after the first loading and loads the Manhattan autosave (v=72-90) = intro skipped,
  Esc + load at the offer (v=142): the fastest L00. Sells bots / bats, Justice Mk II **and 20 Water at 15 for
  Pittsburgh** (the first micro-trade, R2 notes (c)).
- Run 3: intro unskipped (v=13-80), menu juggling, Juni scene watched; **11 Javelins right after the accept** (v=157-162,
  R3 legs.tsv).

**Our autopilot**
- Not built as a leg. The pieces exist: Esc skips room scenes natively, `Complete-FLBaseStory -AcceptStory` talks /
  accepts, the dealer is driven natively. Intro movies are optional in HDE.

**Key notes for training**
1. On HDE this leg should take seconds, not 140: skip the intro, Esc the arrival and Juni scene, accept (SR V8).
2. Shop in the same visit: sell the starter nanobots / batteries, buy the guns / launcher of the plan, and **buy the
   first trade cargo here** (run 2's Water) so FT1 needs no detour (SR V6, A1, A28).
3. Fix the M01a formation failure seen in the newgame chain before timing L00 + M01 together (LEGLOG 09:02).

## M01a - Escort to Pittsburgh
Launch from Manhattan (the 77 s M01 space-enter scene, unskippable), kill the Order wing near Manhattan, fly to the TL146
lane, form up with the transport USV Brandt, ride the lanes, kill the Rogue lane ambush, re-enter formation, take the last
lane and land on Pittsburgh. Lane entry is a race with the script (Brandt must be within 2000 m of the lane).

<!-- legtimes:M01a:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 417 | 423 | 409 | 409 | 409 | 515 | 106 |

Our personal bests (full leg): 517 (10-22 08:03) -> 515 (10-23 15:27) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M01a:end -->

**How each runner does it**
- Run 1: Order wing v=339-400 with the Starflier's 3 Justice guns, Fort Bush lane at v=440 straight after the last kill,
  formation with Brandt, Rogues at the lane v=544-597, dock v=640-657 (R1 notes (c)).
- Run 2: Order wing in 57 s (v=243-300) near Manhattan without a trip, lane at once (v=343), Rogue ambush 445-495, dock
  v=565 (R2 notes (c)). Carries 20 Water for Pittsburgh (sold there at 30).
- Run 3: the fastest M01a (409 s): fights 78 s / 56 s, docks v=562.5 (R3 notes (c)).
- All three: wing kill 57-61 s, waypoint cruise 17-18 s straight, Rogue ambush 53-64 s (NAV rows M01a).

**Our autopilot**
- PASS from the Manhattan accept save Save7e1c. Section gaps (NAV, AP run 2026-10-22_0755_M01a_i3): base 28.5 s
  (Juni talk, restock, launch) vs ~10-15; fight 62 s then **34 s "Awaiting"** until the waypoint (+17); the waypoint
  leg detoured (378 blocked frames, +13.5); Rogue ambush 69 s + 15 s (+31); last lane + Pittsburgh ring 61 s vs 29 (+32;
  the runners' last split may hide a lane skip - to check).
- The Pittsburgh ring has a scripted wait all runs share (16-24 s inside the ring; NAV notes rule 6).

**Key notes for training**
1. Rogue lane ambush (+31 s) and the Order wing (+17 s): fighter fights at runner pace - nearest first, afterburner
   pursuit, no 34 s idle after the last kill (start travel on the kill, A20 / A30).
2. Last lane + ring (+32 s): check in a sheet whether the runners skip the last lane; then fly the ring straight in.
3. The TL146 waypoint (+13.5 s): one straight cruise line; the detour came from blocked frames (geometry guard).
4. Lane entry on the script's distance condition (Brandt within 2000 m), not on the comm (A4).

## M01b - Defend XT-19, Raze the Rogue Base
From Pittsburgh: launch, form up with the LSF fighters, defend station XT-19, cruise toward Manhattan along the waypoints
while scripted fighter waves come (Beta-4 rescue), then the Rogue base appears: shoot only its two weapon platforms; the
base explodes in a space scene; the leg ends on "GAME SAVED". The WR's lead here is flight time between the waves.

<!-- legtimes:M01b:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 457 | 476 | 472 | 457 | 457 | 648 | 191 |

Our personal bests (full leg): 648 (10-22 05:41) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M01b:end -->

**How each runner does it**
- Run 1: Pittsburgh shop 19 s (sells the M01a loot, **buys a Javelin launcher + 40 missiles**), launch 16 s, to XT-19 85
  s, defend 61 s, three waypoint legs of 62 / 62 / 10 s with fighter waves of 41 / 25 / 21 s, **platforms only** 32 s
  (v=1060-1092), explosion scene 18 s (R1 P2.1 table).
- Run 2: load on landing (King's scene), sells Water (+300), **buys 20 Boron at 120 for Manhattan**; XT-19 683-750,
  Beta-4 816-858, base: scrambled fighters then only the two platforms (v=977-1005) (R2 notes (c)).
- Run 3: XT-19 fighters v=672, waypoints 775-938, platforms only v=956-1011 (R3 notes (c)). Guide: at the Beta-4 comm turn
  ~120 deg left toward the dark clouds where the base spawns (SR section 3).

**Our autopilot**
- PASS from Save5f61, = the old script's time. **No FLController.log in the best run** (transcript: base 32 + space 616),
  so the gap is not split (NAV row M01b).

**Key notes for training**
1. Rerun M01b with logs to split the gap (HANDOFF NEXT 3); until then the likely causes are the same as M01a: idle after
   wave kills, slow fighter fights, waypoint legs not flown at cruise.
2. Platforms only at the base (kb and all runs): hold-and-salvo applies to weapon platforms (`static_hold`).
3. Buy the Javelin / missiles at Pittsburgh in the leg's base stop (all runs carry missiles from here).

## FT1 - First Money Gate
After "GAME SAVED": reach net worth +3,500 over the M01 end, then the M02 offer appears on the next dock (Manhattan bar,
Lonnigan / Juni). The level-up fires on the dealer transaction or the cargo pickup itself, never needing a reload (SR V6).

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 216.5 | 219.5 | 357.5 | 216.5 |

autopilot: not built yet (FT1 money step probe only).

**How each runner does it**
- Run 1: ignores "fly to Pittsburgh", lanes to Fort Bush, **shoots one pod of the DSE H-Fuel depot** (v=1240-1249, 16
  H-Fuel, LEVEL 3 at the pickup), lanes to Manhattan, buys the Patriot inside M02 (R1 notes (c), economy.tsv).
- Run 2: no depot, no ship: **the Boron bought at Pittsburgh sells at Manhattan (155) and LEVEL 3 appears at once
  (v=1245)** - two micro-trades on M01's own path (R2 notes (c)).
- Run 3: waits ~50 s in the Pittsburgh ring queue (v=1079-1134), buys a Rhino, kills a transport on the way, shoots the
  depot's pods with Rogues fighting nearby as cover (v=1262-1294, LEVEL 3 in space v=1293) (R3 notes (c), (f)).

**Our autopilot**
- Probe PASS (22 Oct 07:47, i6): Trenton Outpost, 15 Light Arms (all that fit) -> gate cleared, M02 offered; docking
  alone did not fire it, the buy did (`Invoke-FLMoneyPlan`, LEGS.md). ~170 s to Trenton.
- Depot probe (Fort Bush): the pods open, but the lawful depot's attitude fell 0.65 -> -1.00 in 7 s, police and pirates
  attacked, the Starflier died (LEGS.md "Free-time legs").

**Key notes for training**
1. Carry the trade from the previous base (run 2: Water out, Boron back): no detour, the gate fires on the sale (A1, A2).
2. Do not shoot lawful depot pods without cover (our probe); run 3 did it with Rogues fighting around (A5).
3. A buy can fire the level-up too (FT2 run 2, FT8): the cheapest dealer transaction that crosses the gate.

## M02 - The Unity, Cosmo and Ashcroft
Manhattan -> lanes -> Colorado gate (King must follow) -> scan nearby ships until Cosmo -> disable Cosmo's engines (scene)
-> lane at "GAME SAVED" -> help defend Pueblo -> the marked ship -> Rogues -> follow Ashcroft, tractor his life pod ->
the long open cruise to the New York gate -> Fort Bush Rogues -> land on Battleship Missouri.

<!-- legtimes:M02:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 876 | 928 | 896 | 876 | 876 | 1134 | 258 |

Our personal bests (full leg): 1134 (10-22 08:54) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M02:end -->

**How each runner does it**
- Run 1: Patriot + 3 Justice Mk III bought inside the leg, launch cinematic cut by a load (v=1352); Colorado gate v=1498
  at cruise; early scan v=1553; Ashcroft 1860-1886 (26 s incl. the pod); open cruise to the NY gate 152 s; **Fort Bush
  Rogues attacked at once** (v=2110-2150) (R1 P2.1 table).
- Run 2: **Ashcroft: chase at cruise 114-180 m behind, engine kill at v=1845 (283 -> 269 m/s coast), guns online at
  cruise speed, he ejects ~v=1850** (`R2\frames\key\q_ashcroft.jpg`, R2 P4); flies the whole mission at hull 0.34.
- Run 3: Cosmo kill for artifacts tried and abandoned (v=1665-1690); Ashcroft pod tractored v=1954 (R3 notes (c)).
- All three: Ashcroft dead in 18-21 s after an engine-kill coast (CMB fights.tsv, `CMB frames\d10_r2_ashcroft_ek.jpg`).

**Our autopilot**
- PASS from Save05c7 (lead-stall nudge at the Colorado gate). Gaps (NAV, 2026-10-22_0835_M02_i3): **Ashcroft +102 s**
  (travels to a point, eta 112 s, before fighting); **scan +48 s** (holds 69 s waiting for Cosmo); marked ship ->
  Rogues +43 s (follows the marked ship 67 s, then 4.7 km into the fight); Colorado gate +30 s (shortcut-rush stopped 11 s
  at the entrance); Cosmo scene -> lane +24 s.
- Faster than every runner on the 30 km NY-gate cruise (102 vs 118-126 s) and the Cosmo fight.

**Key notes for training**
1. Ashcroft (+102 s): chase at cruise, engine kill 110-230 m behind to fire while coasting at cruise speed (A22, CMB R6).
2. Scan Cosmo at once (+48 s): the script advances on the scan (NAV rule 7, A21).
3. Marked ship / Rogues (+43 s): go to the fight at cruise; pre-aggro the Fort Bush Rogues while neutral (A6).
4. No hold-short at the Colorado gate (+30 s): request at once when no NPC is locked in (A23); King must still follow.

## FT2 - Ship and Kit for M03
Missouri -> (money) -> Manhattan story step "meet Juni on CA Minor" -> California gate -> CA Minor bar -> accept M03.
Gate +5,200. The runners also buy their M03 kit here (ship, Slingshot, shield, thruster).

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 432.5 | 481.5 | 475.9 | 432.5 |

autopilot: not built yet (FT1 money step probe only). The CHAIN stalled at FT2: the hold was full of Light Arms (LEGS.md).

**How each runner does it**
- Run 1: Fort Bush depot again (25 H-Fuel fills the Patriot, LEVEL 5 v=2302), Manhattan touch with two quick loads,
  California gate, sells the H-Fuel at CA Minor x425 inside M03 (R1 notes (c)).
- Run 2: **buys 20 H-Fuel at Missouri -> LEVEL 5 -> sells them back** (v=2196-2208), buys the **Defender** (old kit sold in
  Cargo Transfer to cut the price), depot pods, Slingshot at Manhattan, CA Minor kit (R2 notes (c), P5).
- Run 3: level 5 in space after Missouri (v=2390); **the one instant gate jump by formation with a docking NPC in 47 gate
  entries** (California gate, v=2596-2600.5, `NAV frames\r3_2588_instant_gate.jpg`) (NAV notes (2)).

**Key notes for training**
1. A buy-and-sell-back at the first dock fires the gate with no travel (run 2, A2).
2. Plan the hold: sell / jettison cargo the plan no longer needs (our chain stall; run 1's Balboa hiccup, FT3).
3. The formation jump pays only at busy gates (30-75 s per campaign); our queue shortcut covers it (NAV notes (2)).

## M03 - Artifact Smuggling
CA Minor -> lane -> Magellan side: escort the transport convoy past the buoys, defend it from the Outcasts (shoot transport
2's cargo pods for artifacts - transport 1 destroyed = fail) -> Willard -> back: Valkyries, CA Minor (dock first, before
Juni) -> Walker to Willard -> the Rheinland pirates at Willard -> Juni pays in space ("MISSION RESULT: SUCCESS").

<!-- legtimes:M03:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 780 | 766 | 763 | 763 | 763 | 885 | 122 |

Our personal bests (full leg): 919 (10-22 08:20) -> 901 (10-23 08:37) -> 896 (10-24 10:34) -> 885 (10-24 14:16) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M03:end -->

**How each runner does it**
- Run 1: CA Minor shop inside the leg (Adv. Stunpulse + Slingshot); at the Outcast attack **parks at 7 m beside transport
  2 for 10 s and shoots 4 artifacts out** (CMB fights.tsv); Willard 8 s; CA Minor approached **holding cruise: Juni 168-345
  m -> 1.3-1.7 km behind, docks first** (v=3116-3140); then sets a waypoint on the Balboa wreck while the payment is due
  (R1 notes (c), P2.3).
- Run 2: pods shot at point blank v=2885-2914, 12 artifacts tractored; **engine kill at ~3.5 km from the CA Minor ring at
  cruise, Juni 100 m -> 1.7 km, docks first** (v=3182-3195, `R2\frames\key\ek_caminor.jpg`) (R2 notes (c), P4).
- Run 3: transport pods v=2990-3030 (+46.6k later at Manhattan; the runner's YouTube answer), Willard Rheinlanders to the
  payment in 52 s (NAV, R3 notes (c)).

**Our autopilot**
- PASS from Save1e7d (objective mode, the first mission it flew alone). Gaps (NAV, 2026-10-22_0804_M03_i5): **Willard
  Rheinlanders -> payment +96 s** (119 s fight + 30 s follow); Valkyries launch section +34 s; Willard approach +21 s;
  base stops Willard +18 s, CA Minor +16 s; **CA Minor dock +14 s (request at 354 m, reply 3 = queued, Juni in the bay
  first)**.
- No artifact pods (money layer not built).

**Key notes for training**
1. Willard fight (+96 s): fighters first, gunboats last at 0-300 m, nanobots only after real hull loss (CMB R9); keep a
   Rheinlander alive until the bombers spawn (softlock guard, A7).
2. Companion shed at CA Minor (+14 s): hold cruise to the ring or engine kill at cruise 3-4.5 km out (A3, NAV (3)).
3. Base stops (+34 s over two): launch the moment the story step allows; no room tour.
4. Run-wide: transport 2's pods (0-10 m beside it, 10 s) fund FT3 (A5); only when the money planner wants them.

## FT3 - Balboa Wreck and Manhattan
After Juni's payment: loot the Balboa wreck in the Willard ice field (20 Cardamine + Pyros guns) and/or sell the M03
artifacts at Manhattan; Manhattan's M04 offer scene (Juni, autoplay). Gate +7,000.

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 473.0 | 467.5 | 339.2 | 339.2 |

autopilot: not built yet (FT1 money step probe only).

**How each runner does it**
- Run 1: Balboa: Cardamine 17 + 3, 2 Pyros Type 2 + Pyros turret; **hold full: jettisons 3 H-Fuel (~14 s hiccup,
  v=3436-3450)**; watches the Manhattan arrival scene (78 s) (R1 notes (c), (f)).
- Run 2: jettisons Oxygen / Scrap, Balboa wreck v=3485-3523, **LEVEL 7 in space within seconds of the tractor**
  (v=3512); long lane route back; offer scene played 85 s (R2 notes (c)).
- Run 3: no wreck: straight to Manhattan, artifacts + loot sold 35,570 -> 82,169 (+46,599) (R3 notes (c)) - the fastest
  FT3 because the money came from the M03 pods.

**Key notes for training**
1. Cargo picked up in space fires the gate (A2): the wreck or pods clear +7,000 without a dock.
2. Run 3's route (pods in M03, straight to Manhattan) beats the wreck detour by ~130 s.
3. Free the hold before looting (run 1 lost 14 s jettisoning).

## M04 - Escape from Liberty
Manhattan -> West Point lane with King and Juni (required for the Benford trigger) -> the Navy turns hostile (Unity) ->
Ithaca -> Benford (room scene + station attack) -> Magellan gate -> bounty hunters -> Mactan -> Rheinland assault wing ->
"Fly to the Leeds jump hole alone" -> Leeds hole -> land on Leeds. The Benford room scene (~200 s) is Esc-skippable on HDE.

<!-- legtimes:M04:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 1036 | 1014 | 1019 | 1014 | 813 | 974 | 161 |

Our personal bests (full leg): 1043 (10-22 08:42) -> 993 (10-23 16:10) -> 974 (10-24 23:38) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M04:end -->
(docs\SPLITS.md counts the same PB run as 970 s: the splits run from the leg's accept, the 974 s here is the leg
script's total in `legs.tsv`.)

**How each runner does it**
- Run 1: shops in the leg (sells artifacts / Cardamine, Rhino, **80 H-Fuel x360 for Mactan**), Benford watched 200 s,
  Magellan gate with **cruise held - Juni (38-58 m) brakes at the gate, he goes through alone**; bounty hunters opened
  while neutral at 192 m; **Mactan: 80 H-Fuel x516 = +41,280 in 5 s of base time** (R1 notes (c), P2.3; CMB).
- Run 2: hunters opened on at 1.9 km; no companion shed needed at Mactan - the fight separates Juni (R2 P4).
- Run 3: **engine kill at 3.1 km from the Magellan gate at cruise, coasts at ~280 m/s, Juni falls back** (v=4497-4510,
  `R3\frames\k_4500_ek.jpg`; runner's YouTube answer) (R3 notes (c)).
- All three: after the Magellan gate "Fly to Freeport 4" - **they head for Mactan**; Mactan Base is the selected target
  and the hunters meet them ~2 km from Mactan (REF M04 section targets; `reference\frames\r2_4595.0_M04_60s.jpg`).

**Our autopilot**
- PASS from Save6380 (survival stance). Gaps (NAV, 2026-10-22_0824_M04_i5): **bounty hunters -> Mactan +123 s** (escort-mode
  fight 62 s, then 60 s back to Mactan with 59 detours, 11 s hold) - although the FP4 section itself is -38 s; Leeds hole
  -> Leeds ring +48 s; assault wing -> hole +38 s; Benford / FP4 faster than the runners.
- Magellan gate: queued (reply 3) after a rush, 42 s vs 33 (+9..16 s) - no shed.

**Key notes for training**
- 2026-10-24 23:55 (set s13b, M04 x2 on 3 instances at 34ba88f: the placeholder fight keeps the escort - the user's choice):
  **5 / 6 PASS, 974 / 990 / 1026 / 1028 / 1054 - PB 974 (was 989), two runs under the old PB.** M04.mactan 133.1 (s13a
  152.6, s12d 135.8: the chase is gone - the escort killed the 8 wing ships in 41 s carried along with Juni), wing 73.0
  and hole 50.0 keep the fix's gain (s12d 77.8 / 70.7; par 83.5 / 37); no segment worse than s12d. **The FAIL (784 s):**
  arrived at Mactan 55 s after the wing, "Dock with Mactan" refused (attitude 0.20), then "no way (locked)" 13 times, 4 s
  apart, resting at the base, until the mission's own nag failed it at +80 s ("You failed to fly to Mactan Base"). The
  kb: Mactan's dock unlocks once Juni is within 3 km - she never came within it in that run (the other 11 runs of s13a /
  s13b were never refused). Next: a locked dock whose unlock is the escorted ship's arrival is waited for WITH that ship
  (escort her in, the game's own rule), not resting at the base; and the dock-retry rest must stay inside the nag's
  distance. Progress tool: median 1026 (5 / 6), no new best set; the leg's new PB stands.
- 2026-10-24 23:12 (set s13a, M04 x2 on 3 instances at f5d8d76 = the checkup's refactors + the untested session-12 end code
  cc88517 / 2b3fabd, `pre_aggro 2500,dock_regain 1`): **6 / 6 PASS, median 1032 (s12d 1012), totals 1002-1057 vs 994-1051.**
  The swallowed step is decided now: `a placeholder step with hostile marks - their fight` 3 per run (0 in s12d), so the
  Rheinland Assault Wing is fought as its own objective - **M04.hole 51.6 s (was 70.7; par 37), M04.wing 73.0 (was 77.8)**.
  The price: **M04.mactan 152.6 (was 135.8, +16.8, noise 13: WORSE)** - the wing fight runs in Combat mode, a chase away
  from Juni (8 kills in 33-38 s against the escort's 43-44 s), then `fight over` -> hold -> follow Juni ~12 s until the
  "Fly to Mactan" objective, then a 55-60 s trip (s12d's escort fought the wing while carried along and flew 49-56 s).
  Net -2 s per leg; no new best. Next: at a placeholder fight while an escort is active, fight as the escort's threats
  (keep the escort), or re-decide at once after `fight over` instead of the hold / follow interlude. Also seen: dock_regain's
  grant handover 1 per run (Benford 29.0, unchanged), lethal-guard 1 per run, hunters 30.8 (pre-aggro 8 flags per run),
  the early steps fought nothing wrongly (the one early placeholder fight is Unity's own). The day's 26 refactoring
  commits show nowhere in the splits: every other segment within noise of s12d.
- 2026-10-24 (session 12, sets s12b-s12d): **pre_aggro** finds the 8 bounty hunters (spawn positions) and, since 77384d8,
  fights them ~14 s after "Fly to Freeport 4" shows, ~18 s before the Destroy banner: mactan 135.5 / 136.2 / 138.6 s
  (was 140-171) - only a few seconds. Where the 136 s go (s12d i1, from the banner): the last kills 45 s, dialogue 13 s,
  the trip to Mactan 52 s, the dock-unlock wait (Juni within 3 km) 11 s, a queued dock 15 s. The runners' 22 s = they
  fly at Mactan from the gate and the hunters meet them ~2 km from it, so the trip is inside their hunters segment.
  Blocker for doing the same: Juni flies her own list to Freeport 4 - `bounty_hunter_nag_30/60/90` (Cnd_TetherBroke
  juni 3000 m, 30 s each -> FAIL at 90 s) and `too_far_from_bounty_hunters` (6 km 10 s in the fight). Next: a general
  "the step's meeting comes to us" rule (its advance goal is a ship that follows the player - Script::followShips) that
  flies toward the step-after-next's destination within the companion tethers (Cnd_TetherBroke not parsed yet).
  Benford stalls: dock_regain rescues them (s12b, s12c); 5843e65 hands a grant not followed to the game's dock.
  s12d: 6 / 6 PASS 1003 / 1021 / 1051 / 1028 / 999 / 994 (pre_aggro 2500, dock_regain 1).
  **The user (24 Oct 2026, watching s12d): "The autopilot is making all the wrong choices when Rheinland ships are
  attacking Mactan."** Read from the logs: "Destroy the Rheinland Assault Wing" (22685) and "Awaiting" arrive in the same
  instant; the newest ("Awaiting") equalled the step before, so the objective mode never decided again - in 12 / 12 M04
  runs of s12c / s12d - and under the survival stance it escorted Juni ~110 s while the hackers / Juni killed the
  wing (M04.hole +31 s vs par). Fixed, UNTESTED: a re-issued step whose mission marks changed is a new step, and a
  placeholder step with live hostile marked ships within 6 km is their fight (the survival threat check counts a fight
  objective's marks). Tested in s13a / s13b (above): M04.hole 51.6 / 50.0 s.
1. After the Magellan gate fly straight to Mactan (+123 s): the hunters come to you; open on them while neutral (A21, A6).
2. Leeds hole -> Leeds (+48 s) and assault wing -> hole (+38 s): leave on the kill, cruise at once, ring straight in.
3. Magellan gate: shed Juni by holding cruise or an engine kill at ~3 km (A3).
4. Run-wide: run 1's H-Fuel Manhattan -> Mactan trade (+12,480) is in this leg; the planner decides (A1).

## FT4 - Optronics to Cambridge
Leeds (M04's +25,000 at the Equipment room) -> buy Optronics -> lanes through Bretonia -> Cambridge sale -> Cambridge
bar, accept M05. Gate +10,000. Runs 2 / 3 detour via Battleship Norfolk for the Crusader.

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 378.5 | 564.5 | 619.5 | 378.5 |

autopilot: not built yet (FT1 money step probe only).

**How each runner does it**
- Run 1: **80 Optronics x980 in 3 s** (all cash), straight to Cambridge; ship change later (Cavalier in M05, Crusader at
  Suffolk in FT5) (R1 notes (c)).
- Run 2: 30 Optronics, the Norfolk detour for the Crusader + class-4 shield (~370 s of lanes); "lost 4:12 against his PB:
  the Crusader switch" (R2 notes (c)).
- Run 3: Leeds dealers, Norfolk Crusader v=5375-5415, Cambridge (R3 notes (c)).

**Key notes for training**
1. The Crusader detour costs 186-241 s here: buy the next ship where the route already passes (A28; HDE markets differ
   from vanilla: plan from the test install's data).
2. Fill the hold with the one trade (run 1's x80 = +28,000).

## M05 - Sprague and Baxter
Cambridge -> lanes -> Omega-3 gate (busy: 12-13 s queue in all runs) -> Freeport 1 lanes -> Sprague fight -> Sprague
(room scenes, Esc on HDE; the dock explodes) -> Baxter -> the Rheinland wing after Baxter -> Cambridge hole -> follow the
Rheinlanders, chase and torpedo-run space scenes -> fight -> Leeds hole -> land on Leeds (only the marked lane ring works).

<!-- legtimes:M05:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 1204 | 1170 | 1198 | 1170 | 952 | 1203 | 251 |

Our personal bests (full leg): 1346 (10-22 09:05) -> 1249 (10-23 09:09) -> 1236 (10-23 15:51) -> 1203 (10-24 10:18) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M05:end -->

**How each runner does it**
- Run 1: shops after the accept (Cavalier, 30 Diamonds, 52 s); post-Baxter wing killed nose-on in ~65 s, nanobots 11 -> 1
  mid-fight; **charges cruise the instant the last gunboat dies (v=6035.7), at cruise 19 s before the "Fly to the
  Cambridge jump hole" banner** (v=6063), straight into the hole (`reference\frames\desc\r1_5971_M05_baxter_fight.jpg`,
  REF verified_desc).
- Run 2: Sprague fight 5710-5752, Baxter fight 6114-6175, flies at hull 0.69 never repaired (CMB fights.tsv).
- Run 3: fights 39 / 60+ / 66 s with nanobots instead of batteries (CMB fights.tsv).

**Our autopilot**
- PASS from Save1d87. Gaps (NAV, 2026-10-22_0842_M05_i5): **post-Baxter fight + Cambridge hole +179 s** (307 vs 128);
  Leeds hole -> Leeds +57 s (hole rush stopped 12 s at the entrance + a 22 s request, lanes); Sprague fight +37 s; Cambridge
  hole -> follow +36 s; Baxter approach +24 s; Omega-3 gate +15 s (rush stopped 12.6 s).

**Key notes for training**
1. Post-Baxter (+179 s): fighters first, gunboat last at point blank, and **leave on the kill** - the cruise charge starts
   on the last kill, not the banner (SR V11.4, A20, A30).
2. Holes / gates (+57, +15 s): no hold-short at hole entrances; at Omega-3 queue or formation-jump (A23).
3. Sprague fight (+37 s): nearest first on the afterburner; leave for the dock on the banner.

## FT5 - Diamonds, Gold and Torpedoes
Leeds -> sell Diamonds, buy Mining Machinery -> (ship) -> New London -> Dublin -> the Hood: sell MM, buy Gold, torpedo
launcher / CM dropper -> Hovis' offer at the Hood bar. Gate +16,000.

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 530.5 | 463.5 | 461.0 | 461.0 |

autopilot: not built yet (FT1 money step probe only).

**How each runner does it**
- Run 1: Diamonds x1,275 (+11,880, LEVEL 11), 30 MM; **Battleship Suffolk: Cavalier -> Crusader, Adv. Skyrail, Imp. CM
  dropper**; Hood: Starkiller + 27 torpedoes (no IMG bug) (R1 notes (c)).
- Run 2: Diamonds +13,860, 40 MM, Starkiller + Imp. CM dropper at Leeds (-31,000), ~100 s straight cruise in Dublin to
  the Hood (R2 notes (c)).
- Run 3: Leeds dealer 44,213 -> 18,714, Dublin gate, Hood (R3 notes (c)).

**Key notes for training**
1. The torpedo launcher + CM dropper bought here carry M06-M13's capital fights (SR V7): buy them, then spend them.
2. Run 1's Suffolk stop costs ~70 s over runs 2 / 3 but replaces run 2 / 3's Norfolk detour in FT4: one ship stop total.

## M06 - The Race and Glorious
Hood bar accept -> race intro scenes (75 s) -> the ring race against Hovis (bumping Hovis fails the mission; race 2
lost = fail) -> land on the Hood -> fly to Station Glorious, defend it, land -> meet near Glorious, Quintaine, destroy the
Rheinland gunboat -> Leeds hole (destroy all hostiles) -> land on Leeds. The race is level; the gap is outside it.

<!-- legtimes:M06:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 544 | 534 | 489 | 489 | 489 | 682 | 193 |

Our personal bests (full leg): 1193 (10-22 08:03) -> 719 (10-23 07:52) -> 714 (10-23 14:37) -> 710 (10-23 22:32) -> 682 (10-24 13:24) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M06:end -->

**How each runner does it**
- Run 1: race at cruise from GO (charging at 79 m/s, cruise 7132.5, 297-300 held, lap ~73 s; opens status screens
  mid-race) (`reference\frames\desc\r1_7127_M06_race.jpg`); **Glorious: ~44 s fight in the asteroid field with
  torpedoes every 2-3 s, docks 8 s after "LAND ON STATION GLORIOUS"** (v=7309-7361, `reference\frames\desc\
  r1_7309_M06_glorious.jpg`, looked at); ends the defence at hull 0.70 (REF M06).
- Run 2: Hood -> Glorious 121 s (40 s cruise, 53 s fight, torpedoes 50 -> 47, 19 s to the dock), Glorious -> Leeds 181 s
  (Quintaine 31 s, gunboat + escorts with torpedoes, 29 s cruise to the hole) - "cruises straight at every objective,
  fights at the objective with torpedoes, leaves the moment the banner changes" (R2 P2); takes the hull to 0.37 (REF).
- Run 3: fastest M06 (489): race 69 s, Glorious defence 47 s (REF M06; R3 notes (c)).

**Our autopilot**
- PASS full leg from Save33fa (race won, Glorious, Leeds); 1 of 8 runs passed (the others: "crossed Hovis" in the race
  before the push-off rule, a waypoint fail; LEGLOG). Race practice 74.1 s (= the user's 2020 lap).
- Gaps (NAV, 2026-10-22_0743_M06_i5): **Glorious defence 449 s vs 47 (+403)**, **gunboat 172 vs 24 (+148)**, Leeds hole ->
  Leeds +54, hole section +27; race +4.6.

**Key notes for training**
1. Glorious (+403 s): fight at the station with torpedoes, nearest first, and dock the second "Land on Station Glorious"
   appears; accept hull loss when the clock matters, within the hull-is-sacred floor (SR V11.7, A33).
2. Gunboat (+148 s): torpedoes / missiles at 0-60 m (CMB R4), leave on the banner.
3. Leeds hole -> Leeds (+54 s): straight in at cruise, lane ring straight in (A23).
4. Racing is general (user, 22 Oct): the race is the benchmark of time-optimal flight through any point sequence.

## FT6 - The Order Revealed (Leeds)
Only the Leeds M07 offer scene (Tobias / the Order, Equipment room, autoplay) and the Accept. No flying, no money gate.

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 165.5 | 167.5 | 168.5 | 165.5 |

autopilot: not built yet (FT1 money step probe only). Our M07 leg starts before the offer, so its base phase already
contains this scene skipped with Esc (~5 s on HDE; NAV row FT6).

**Key notes for training**
1. Esc the scene and accept; verify the offer still appears after the skip (R1 P2.4: base RTC `m007_s037a`).

## M07 - Through Rheinland to Kusari
Leeds -> kill one Agent, lanes -> Stokes fighters -> the Rheinland blockade (fighters, gunboats, a cruiser) at the Tau-31
gate -> Tau-31 lanes -> Sinclair defence in the asteroid field -> Tau-29 hole (scripted wait) -> Shinkaku in / out ->
fleet scenes (68 s) -> the Schiller fleet fight -> Tau-23 hole -> Cali (Kress' base) -> Kyushu hole -> land on Kyushu.

<!-- legtimes:M07:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 1262 | 1266 | 1250 | 1250 | 1250 | 1669 | 419 |

Our personal bests (full leg): 1816 (10-22 06:41) -> 1721 (10-23 08:11) -> 1669 (10-23 14:52) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M07:end -->

**How each runner does it**
- Run 1: blockade 72 s, **sits at 0-35 m on the cruiser and loses 28 % hull there** (v=7968-7977); fleet fight 111 s:
  fighters in **Schiller's lee** (its hull fills a third of the picture), gunboat Bietigheim at point blank, cruiser
  Nestor from range, Burghausen behind its stern, **leaves on the banner with Schiller alive** (v=8604)
  (`reference\motion\frames\B2_r1_m07_fleet_3s.jpg` per BEH 2c); spends batteries 20 -> 0, torpedoes 20 -> 1, CM 23 -> 6
  (CMB fights.tsv).
- Run 2: cruiser at 57 m: hull 0.96 -> 0.53; waits 16 s at the closed gate; fleet: Bietigheim at 51 m, Burghausen 197 m,
  Nestor 663 m, **Schiller killed at 0 m**, thruster held 180-200 m/s, every nanobot and battery spent (R2 P3).
- Run 3: **the cruiser from 165-455 m: no hull loss**; hugs Schiller at 1-45 m repeatedly (CMB fights.tsv).
- All three: Tau-29 hole is a shared scripted wait (52-60 s at 0 in runs 1 / 2), Kyushu / Tau-23 holes 3-18 s (NAV rule 4).

**Our autopilot**
- PASS full leg from Save766f (Leeds offer -> Kyushu); tail from Ku03 163 s. **The best run has no FLController.log**: split
  only at docks (NAV): Leeds -> Shinkaku 860 vs 587 (+273), Shinkaku -> Cali 597 vs 416 (+182), Cali -> Kyushu 303 vs 206
  (+97). Checkpoint runs failed on the Kyushu nag ("You failed to fly to Planet Kyushu", 4 of 5; LEGLOG).

**Key notes for training**
1. Rerun M07 with logs (HANDOFF NEXT 3): +552 s with no split is the largest unknown.
2. Fleet fight (part of +182 s): fighters first in the battleship's lee (`blind_shelter`), gunboats at 0-60 m, cruisers
   from 150-450 m (CMB R4 / R9, A18). Open: the runbook says every `lbl_rh_fleet` ship must die (`docs\missions\M07.md`
   step "Required action"), yet run 1 left Schiller alive and the mission went on - check in our build before skipping it.
3. Blockade: the cruiser from range (run 3: no loss) - not at 0 m (runs 1 / 2 lost 28-43 %).
4. The Kyushu nag fails checkpoint runs: read its distance / timer before the next M07 test.
5. 2026-10-24 (session 11, splits): the gold gaps are systematic - M07.sinclair 187-189 s in all three full passes
   (par 91, run 3), then fleetdone +58, gate +48, cali +47, end +39. **Tau-31 (M07.sinclair)**: the objectives there
   are dialogue-gated (Juni 0595 -> lane to Holman -> Tobias exits ring 7 -> Tobias 0650 -> "take the lane to the Tau-29
   gate" (a lane the mission keeps closed both ways) -> Tobias 0680 -> "Fly to the Trade Lane" -> ... "Defend Quintaine
   and Sinclair"). We flew to the closed ring and waited (129 s from that objective to "Defend"); run 3 cruised on
   toward the mission waypoint (25 s, 93 % cruise) and reached "Defend" 44 s after it. -> `closed_lane_ahead` (a live
   flying script goal instead of the wait; with none it still waits - M04 West Point's lane opens later). Probe s11n:
   WORSE, 242 s - the goal found ("within 1000 m of Construct_A_7") is a later step's, unarmed: 50 s there, back to the
   closed ring, then "Fly to the Trade Lane" (the broken lane ~(16.9k, 0, 17.9k)) - a zigzag. Off. Next idea: pre-stage
   toward the next objective's own nag point (the 0680 comm sets "Fly to the Trade Lane" with `NagDistTowards POS 16207,
   0, 18233`) - where run 3 was heading. **v2 probe (s11q): mission FAILED** - "You failed to dock with the Trade Lane":
   that objective carries its own `NagDistTowards ... Bw10_Trade_Lane_Ring_12, 23350` toward the closed ring, so leaving
   it fails. Dead end (off). In the gate baseline that span (closed-lane objective -> "Fly to the Trade Lane") already
   takes ~31 s vs run 3's 24: the Tau-31 loss lies in the other parts of the segment (23250 -> 23255, 23260 -> 23270).

## FT7 - Gold to New Tokyo
Kyushu -> New Tokyo (sell the Hood Gold, buy Light Arms, ship) -> Shinagawa bar, accept M08. Gate +36,000.

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 318.0 | 282.5 | 291.8 | 282.5 |

autopilot: not built yet (FT1 money step probe only).

**How each runner does it**
- Run 1: Gold x1,104 (+27,370, LEVEL 14), **Crusader -> Barracuda** (vanilla New Tokyo), 45 Light Arms, ammo (R1 notes (c)).
- Run 2: Gold (+27,370), 35 Light Arms; no ship (R2 notes (c)) - 35 s faster than run 1.
- Run 3: New Tokyo dealer 30,044 -> 13,333 (R3 notes (c)).

**Key notes for training**
1. One sale + one buy at New Tokyo; the gate fires on the Gold sale. A ship purchase costs ~35 s of dealer time here.

## M08 - Tekagi's Transport
Shinagawa -> lane -> "Fly to the Waypoint" (the trigger: Ozu within 9 km of the Honshu hole) -> Honshu hole -> Tekagi's
transport -> destroy the Yukawa weapon platforms -> formation with the slow transport (scripted pace) -> enemy ships ->
escort to the Chugoku hole -> land on Kyoto.

<!-- legtimes:M08:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 601 | 582 | 600 | 582 | 582 | 676 | 94 |

Our personal bests (full leg): 691 (10-22 04:52) -> 685 (10-22 07:54) -> 676 (10-22 14:03) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M08:end -->

**How each runner does it**
- All three: **cruise straight at the Honshu hole with the waypoint 30-40 deg off to the left; the banner flips 40-44 s in,
  5.0-5.5 km from the hole** (`reference\frames\desc\r1_9388_M08_waypoint.jpg`, BEH 6; CAL #3).
- Run 1: platforms at 19 m and 6 m, 2-4 s each (torpedoes 20 -> 10), then fighters; formation wait ~90 s (R1 (c), CMB).
- Run 2: platforms at 9-23 m, 3-6 s each; formation behind the transport ~155 s (R2 notes (c)).
- Run 3: platforms at 14-75 m; formation 168 s (R3 notes (c)).

**Our autopilot**
- PASS from Save3a48 (4 of 5 runs). Gaps (NAV, 2026-10-22_1351_M08_i1): after Chugoku -> Kyoto +45 s; platforms + Kusari
  forces +39 s; destroy enemy ships -> escort +27 s; waypoint -> Honshu hole +20 s. **Formation faster than the runners
  (116 vs 166 s)**: the trigger sets the pace.
- Script goal for the waypoint (Ozu within 9 km of the hole) built in 4c7d3fe, **verified in game 23 Oct 2026** (AP "Script goals").

**Key notes for training**
- 2026-10-23 (session 6): PASS 696 / 702 s on c7f51cf. Fixed: the no-progress rule had given up both Tekagi platforms from a blind-spot hold 400-950 m out (aimed out of gun reach) and the neutral-mark rule then shot the transport to capture (mission failed). The Ozu-within-9-km script goal verified. User: the ship still flies inefficiently close to the platforms - the motion trace shows 117 s of fight at 67 m/s mean, 63 % station keeping (the blind-spot hold); next: read the steering there.
1. Kyoto approach (+45 s): straight at cruise after the Chugoku hole; ring / dock without hold-shorts.
2. Platforms (+39 s): hold-and-salvo at 0-20 m, 2-6 s each, CMs on (one-shot missiles at low shield, A9).
3. Test the script goal (+20 s): fly at the hole, not the navmarker (A21).
4. Enemy ships after the formation (+27 s): fight them where they are and leave on the banner.

## FT8 - Light Arms to Kyoto
Kyoto: sell the Light Arms, restock, accept M09 at the Kyoto bar. Gate +30,000.

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 42.5 | 44.5 | 50.0 | 42.5 |

autopilot: not built yet (FT1 money step probe only).

**How each runner does it**
- Run 1: Light Arms x770 -> **LEVEL 16 on the sale** = gate met; restock (R1 notes (c)).
- Run 2: Light Arms + M08 reward; **sells the torpedoes and buys them back** (v=9966-9968) to fire the level-up (R2 (c)).
- Run 3: torpedo buy / sell per the guide (v=9969-9989) (R3 legs.tsv).

**Key notes for training**
1. A dealer transaction fires the gate; if worth is already enough, any buy / sell-back does it (A2).

## M09 - Tekagi's Arch
Kyoto -> Tohoku hole -> to within reach of the arch past Tekagi's patrol -> destroy the arch's 4 shield generators (the
battleship Musashi and fighters around) -> dock at Tekagi's base (room scenes, Esc on HDE) -> the Rheinland forces ->
Chugoku hole -> land on Kyoto.

<!-- legtimes:M09:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 796 | 830 | 882 | 796 | 530 | 724 | 194 |

Our personal bests (full leg): 899 (10-22 07:39) -> 852 (10-22 13:47) -> 834 (10-22 18:38) -> 731 (10-24 12:17) -> 724 (10-24 12:29) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M09:end -->

**How each runner does it**
- Run 1: cruises at 300 from 9.9 km and jettisons scrap in flight; **4 generators in ~37-39 s**: cruise to 140 m, 290 -> 9
  m/s in 1 s, holds at 28-95 m with a slide to the left, afterburner hops of 400-460 m, torpedoes from ~1 km, 10 CMs
  against Musashi's missiles; fighters and Musashi ignored; hull 0.98 -> 0.96 (`reference\frames\desc\
  r1_10200_M09_generators.jpg`, BEH 1a / 1b; CMB fights.tsv).
- Run 2: lost cruise 1.1 km out (~13 s slower), guns only along the arch from 185-730 m, generators in 36 s (R2 P3, BEH 1a).
- Run 3: cruises to 14 m of generator 1 and touches it; the **Chugoku-hole bug wave with cruise disruptors** cost ~3 min:
  126 s fight, then 70 s on the afterburner at 198 m/s to the hole (R3 (f), NAV rule 10).

**Our autopilot**
- PASS full leg from Save4686 (four of six runs); from the arch 597 s (hold-and-salvo, session 5). Gaps of the 852 s run
  (NAV, 2026-10-22_1333_M09_i3): **generators -> dock +93 s** (since then: 4 generators in 131 s, `docs\legs_notes.tsv`;
  runners 36-40); **Tekagi patrol +75 s** (104 vs 29-52); Chugoku -> Kyoto +55 s; Tohoku -> arch +28 s; Rheinland forces
  + hole +27 s (46 s of rushes / waits at the hole); Tohoku hole +16 s (rush stopped 13 s).
- The user saw "indecisiveness while targeting the shield generators" (generator <-> fighter every ~3 s): `target_dwell`
  6 s built, *untested* (AP).

**Key notes for training**
- 2026-10-23 (session 8 leg gate + bisect): **"Destroy the Tekagi Patrol" (23690) is a kill race**: Ozu's dialogue on
  entering Ku07 ends ~66-70 s later in `commcomplete_DX_M09_0210_OZU`, which sets "Fly to the Blood Dragon Outpost" and
  `Act_NagDistTowards ... Ozu, Ku07_01, 23760` - failing at once ("You failed to fly to the Blood Dragon Hideout") unless the
  patrol's 7 ships are already dead (all 7 dead advances to 23695 first). Passes 22 Oct: 7 kills in 65 s; the gate and its
  reruns: 3-5 kills in 70 s (gun accuracy 0.17-0.23 vs 0.33), FAIL x3 at HEAD; bisect runs of older builds scattered
  (4c7d3fe / 9e10904 / 7466b45 won the race, 7466b45 by 0.17 s; 9f8331f / 84bd318 lost; target_dwell 0 lost) - a marginal
  race, not one commit. Next: a faster patrol fight (focus fire, missiles on fresh targets, the first kill ~12 s in) or
  the fight drifting toward Ku07_01; 9e10904 also failed later by the same nag in 23640 (fighting a solar, not flying).
- 2026-10-24 (session 11): **why the patrol comes at all** - it is set by `at_nebula_attack` (the player within 14.75 km
  of Tekagi's base Ku07_02), and we enter that sphere only because at the Tohoku entry, under "Awaiting", the script
  goals take the generators (a later step's destroy goal, by the last objective, unmarked) and fly "to-marks" toward the
  base (every gate since 23 Oct; probe s11n: inside the sphere at 12.8 km). The story's way - entry -> the Blood Dragon
  outpost Ku07_01 - stays ~19 km from the base; the runners spend 5-13 s in that section, no kills. `towards_nag`
  (fly on toward the nag's target) fails the other way: leaving Ozu's battle (> 3 km) -> "You fled the battle" (23793).
  -> `stale_destroy_wait` (an unmarked destroy goal only the last objective stands in for is a later step's: no trip;
  v2: nor one replacing a step's own trip - "Fly to the Blood Dragon Outpost" took the same generators). Set s11o9 (6
  full legs, one lost to a boot crash): **4 PASS of 5** (731 / 739 / 760 / 724 s - new PB 724; one death at 456 s, later
  combat) against 0 PASS in
  five gates; golds at M09.tekagi (-47.5), forces, kyotogo. The patrol still comes on the way (par has it too) and is
  now cleared before the nag fails; the patrol fight (81.5 s vs par 22) is the next gap. M13 city check (v3: never a
  mission-ending goal): the switch never acts there -> on by default (gate s11preset: M09 PASS 772). Open: a one-shot
  death at full hull at the arch generators (s11o9-i2, 456 s: hull 1.00 / shield 0.69, dead 40 ms later, a torpedo
  launched at 106 m the same instant with the paths clear; 2 s before a 1,996-point hit under 3 attackers) - **read in
  session 12: our own torpedoes.** The generator sits on a big rock; the hold stood ~15 m off the rock face (end_tail
  1:11.0 / 1:11.6: generator 68 m, the rock filling the right half). The first torpedo (12:25:45.176, 256 m) "hit" after
  0.19 s and the 1,996-point hit came 0.19 s after launch; the second killed the ship 10 ms after its launch. The centre
  rays (line, nose) were clear; the rock was beside. -> `blast_flank` (rays across the nose within the blast reach).
1. Generators (131 s vs 36-40): hold-and-salvo at runner pace - cruise in, cut 150-200 m short, hold, every launcher on
   cooldown from ~1 km, afterburner hops, never off the target (A16, BEH A1-A3, CAL #2); test the dwell.
2. Tekagi patrol (+75 s): cruise past it toward the arch; fight only what blocks (CMB R3).
3. Chugoku approach and holes (+55, +27, +16 s): no hold-shorts at holes (shared scripted wait at Chugoku 16-17 s only);
   under cruise disruption fly the afterburner (A23, A26).
4. CMs against the battleship's missiles at low shield (A9).

## FT9 - Kyoto Restock
Kyoto: accept M10 at the Kyoto bar (Ozu / Hakkera), restock ammo. No money gate.

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 25.5 | 37.5 | 37.0 | 25.5 |

autopilot: not built yet (FT1 money step probe only).

**How each runner does it**
- Run 1: load, bar scene, accept (25.5 s); restock inside M10 (R1 notes (c)).
- Run 3: Kyoto dealer: 34 Slingshot, 34 Catapult, 34 torpedoes, 50 Imp. CM (42,188 -> 13,730) (R3 notes (c)).

**Key notes for training**
1. Fill every launcher here: M10's heavy fighters and experimental ship spend them (SR V4 consumables).

## M10 - The Experimental Shipyard
Kyoto -> Sigma-13 hole -> destroy the 8 Rheinland heavy fighters -> "Break off", New Berlin hole (the Old Imperial Navy
wrecks on the way) -> New Berlin -> Frankfurt gate, lanes -> Holstein (room scene; Fischer must dock after you) -> Bruchsal:
assault wing, dock, escape space scene (80 s) -> destroy one experimental battleship -> wait for Botzler's death -> Hamburg
hole within 6 km of Von Claussen -> Order forces -> land on the Osiris (only once its waypoint exists).

<!-- legtimes:M10:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 980 | 1040 | 1026 | 980 | 915 | 1125 | 210 |

Our personal bests (full leg): 1498 (10-22 08:44) -> 1379 (10-23 13:24) -> 1171 (10-23 17:07) -> 1143 (10-24 16:39) -> 1128 (10-24 16:58) -> 1125 (10-24 16:59) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M10:end -->

**How each runner does it**
- Run 1: **8 heavy fighters in 47-57 s**, nose-on at 80-500 m (median ~220 m), one Slingshot / Starkiller per target, ~6 s
  per kill, CM pairs, batteries hold the shield; **cruise charge ~5 s before BREAK OFF, right after the last kill**;
  wrecks looted on the way (2 Diamondbacks); **experimental ship: cruise straight to 0 m at 296-299 m/s, stopped by contact
  inside the hull, parked 30 s firing, 10 batteries at once when the shield failed once** (`reference\frames\desc\
  r1_10893_M10_heavyfighters.jpg`, `r1_11470_M10_bruchsal_expships.jpg`; BEH 2a). Holstein scene watched (64 s).
- Run 2: wrecks looted on the way (Kyoto -> New Berlin equal to ours: 313 vs 312); **Fischer left 50 km behind by the
  Frankfurt lanes**; experimental ship: **engine-kill coast 295 -> 264 m/s along the beam into the gantry**, sits inside at
  0 m, n/b 20/20 -> 19/20 only, dead in 38 s (`CMB frames\d10_r2_m10_gantry_entry.jpg`); Osiris docked at once (R2 P2 / P3).
- Run 3: no batteries aboard, heavy fighters in 42 s; navmap wreck search 110 s; inside the experimental ship at 0 m ~33 s
  (CMB fights.tsv; R3 notes (c)).
- All three: aim at the hull's **middle, broadside**, through the open lattice (BEH 2a); the damage comes in the last
  cruise kilometre outside (hull -12-16 %), none inside (CMB).

**Our autopilot**
- PASS full leg from Save7b39 (1 of 4 full runs; 3 deaths); from Holstein 806 s (blind spots reachable through free
  space; an earlier run flew into an experimental ship and thrashed 6 min). Gaps (NAV, 2026-10-22_0818_M10_i2):
  **Bruchsal -> after the experimental ships +255 s** (485 vs 230); **Osiris dock +126 s** (granted at 5 m, then 91 s before
  the bay took it); Hamburg-hole leg +60 s (5 short travel legs); Frankfurt -> Holstein +43 s (dock refused twice, reply 1,
  ~18 s); Sigma-13 hole -> fight +33 s; New Berlin +32 s.

**Key notes for training**
- 2026-10-24 (session 12, offline log reading, no runs): **the "three stall paths" are two.** s11preset and s11q-lf
  stalled the same way: Holstein answers `dock_denied_locked` (kind 0, response 1) - the script unlocks the ring only when
  Fischer's ether comm 1370 completes (24.8 s after the player is within 5 km of the ring, runbook step 7), the cruise
  arrives in ~17 s -, the retry 4 s later found "no way" (the game routes to no locked base), the step counted as
  arrived and the ship rested 60 s to a STALL. The leash note in s11preset (navpoint_12 beyond the leash) came before
  that and was not the stall. Fixed: a dock trip to the step's base that finds no way is retried every 4 s like a refusal.
  **The exit "minefield" fail (s11gate) was the zone's damage, not a mine:** `Zone_Rh04_minefield` has `damage = 200`
  (the only minefield zone with damage in the game); the entry leg after the Bruchsal launch drains the hull 200/s
  (~1.7 %/s, shield full) - every run, PASSes too (s9gate 1.00 -> 0.62) -, and a mine 80 m off made the detector call it
  a fail. Detector fixed (a mine's blast takes the shield first). Open: where the drain applies - the fight inside the
  shipyard's exclusion and the exit along tracks 5-14 lost no hull, the entry near tracks 1-4 lost 17-38 % (probe: which
  part of the entry, and whether keeping inside the 150 m track boxes avoids it); the exit crawls at the strict cap
  (~80 m/s for ~60 s).
  **Set s12a (b22ab14 + 694fd91): PASS 1143 s - new PB (was 1171)**; Holstein: refused (response 1) at the ring at
  300 m/s, "no way (locked?)", retried, granted 13.4 s after the refusal (gold 162.9); no minefield-damage event.
  Remaining Holstein slack: up to ~4 s of retry granularity (a retry on comm 1370's end would remove it).
- 2026-10-23 (session 8, Von Claussen's 6 km leash, `too_far_from_von_claussen_*`): **PASS 1379 s** (best was 1498;
  gate_s8o, c6ad02e). Before: "You fled the battle" x4 (the anchor was the wrong ship - mission ships have runtime ids;
  matched by RepName vs IdsText now), then a ship followed while the Hamburg hole dock nagged (the leash now ends on dock /
  jump steps and when the step's navmarker lies within 2 km of a hole / gate - the Hamburg step shows a blank text), then
  a death in the exit field: the leash's follow-back used the game's follow, which hit a big asteroid at ~380 m/s (hull
  0.98 -> 0 in one frame, s8n 1239 s) - now our trip toward the ship (sphere avoidance). Open: the leash trip restarts
  every ~12 s (+1.5 s plot) and alternates with the step's own trip at ~4.5 km; the script releases the leash at
  `botzler_dies` (step 13), which we do not track (all leashes are kept until a dock / jump step).
1. Experimental ship (+255 s): broadside at mid-hull through the lattice at cruise (or an engine-kill coast), stop inside
   at the core, hold and fire everything, batteries at once on shield failure; then leave the second ship (A18, BEH B1).
2. Osiris dock (+126 s): diagnose the 91 s bay stall; dock only after the purple waypoint (softlock guard, A24, A7).
3. Hamburg leg (+60 s): one direct trip, not five short legs (A21); Holstein refusals (+18 s, A24).
4. Heavy fighters: nearest first, shield held full with batteries (`battery_mode = shield_full` idea, CMB P8), leave on the
   last kill (A20); loot the wrecks only on the way (no detour cost in runs 1 / 2; the late-game money, A5).

## FT10 - Aboard the Osiris
Only the Osiris room scenes (Orillion, the M10 end and the M11 offer on Deck2) and the Accept. Run 3 buys the Anubis here
(inside the scene's room), runs 1 / 2 inside M11.

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 160.5 | 159.5 | 161.5 | 159.5 |

autopilot: not built yet (FT1 money step probe only). Our M11 leg starts from the Osiris after the offer; the scene is
Esc-skipped (~5 s on HDE; NAV row FT10).

**Key notes for training**
1. Esc the scene, accept; the Anubis purchase (wreck guns sold first) belongs to this visit (A28).

## M11 - Prison Break
Osiris (Anubis, kit) -> New York hole -> enemy ships -> Buffalo (room) -> in-system hole -> destroy the Navy satellite ->
**Rule (the user, 23 Oct 2026): any run through Zone 21 that takes hull damage from the minefield is a FAIL** - the
logic logs `autopilot minefield-damage` (hull falling in a mine-danger field, a mine within 150 m) and `flleg.ps1`
fails the leg on it, whatever the mission's outcome. **M11 is chronically unreliable: ~half its legs fail since 22 Oct
(FAIL 1237, 1208, 675, 1151, 690, 609; PASS 1196, 1214, 1169, 1229, 1242), most by one-shot mine deaths in Zone 21** -
pursuits drift into the field, and the travel to the in-system jump hole crawls through it on strict avoidance (s9m11b,
30 m/s, 20+ mines near). Needed: routes and fights kept out of mine fields by the zone's geometry (TODO).
Walker rendezvous, follow Walker into Zone 21 -> fights, defend the Order ships -> Alaska gate -> follow Walker to prison
station Mitchell (room scene, Esc on HDE) -> defend the Order ships (Walker's death scene) -> NY gate -> Omaha / Buffalo ->
the Osiris uncloaks, land on it.

<!-- legtimes:M11:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 1158 | 1224 | 1252 | 1158 | 979 | 1169 | 190 |

Our personal bests (full leg): 1196 (10-22 07:43) -> 1169 (10-23 14:57) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M11:end -->

**How each runner does it**
- Run 1: Osiris shop ~54 s inside the leg (Barracuda + 2 Diamondbacks -> Anubis, Debilitator); Navy satellite fight 42 s;
  **parks against Battleship Omaha's hull** until the Osiris scene (CMB fights.tsv); prison scene watched ~180 s (R1 (c)).
- Run 2: **satellite dead in ~6 s to torpedo + missile salvos at 0 m** (v=12447-12453); **engine kill ~4.5 km before
  Mitchell, Juni / King 200 m -> 2.6 km, then 10 s stopped 100 m off the bay** (`R2\frames\key\ek_mitchell.jpg`);
  **parks inside the Alaska -> NY gate ~32 s for Walker's scene** (v=13088-13120) (R2 notes (c), P4).
- Run 3: 1252 s, the slowest (Zone 21 and Alaska -> NY sections 68 / 211 s; NAV).
- Commenter on run 2: form up with the LNS-Phoenix after Mitchell, engine kill at cruise after the first Nomad wave and
  kill it - Walker's scene starts at once (`research\videos\speedruns\#2 ... OldOak.txt`); no run used it (A27).

**Our autopilot**
- PASS full leg from Save08f9 (2 of 4; 2 deaths); tail from Buffalo 101 s. Gaps (NAV, 2026-10-22_0723_M11_i5): **Order
  ships after the prison +89 s** (247 vs 158); NY hole -> Buffalo +52 s; Buffalo base stop +24 s; satellite +24 s and +15 s;
  in-system hole +20 s. Faster than the runners: accept / launch (no shop), NY hole (rush), Walker rendezvous; the prison
  scene is skipped.

**Key notes for training**
1. After the prison (+89 s): straight to the NY gate; try the Phoenix kill to start Walker's scene at once (~30 s, A27).
2. NY hole -> Buffalo (+52 s) and the Buffalo stop (+24 s): cruise at once, ring straight in, launch on the story step.
3. Navy satellite (+39 s over two sections): torpedo + missile salvo at 0 m, ~6 s (run 2); it is a static target for
   `static_hold`.
4. Mitchell: engine kill at ~4.5 km sheds the companions; the bay still holds ~10 s - arrive as it opens (NAV (3)).

## FT11 - Orillion's Briefing
Only the Osiris room scene after M11 (bar, Orillion: the M12 offer) and the Accept.

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 126.5 | 126.5 | 128.5 | 126.5 |

autopilot: not built yet (FT1 money step probe only). Our M12 leg starts from the Osiris; the scene is Esc-skipped.

**Key notes for training**
1. Esc the scene, accept; shop for M12 in the same visit (Reaper turret, Catapult, Driller mines - all runs; A28).

## M12 - The Nomad Lair
Osiris (St02) -> launch scene (41 s) -> formation with King and Juni to the lair -> enter the lair -> destroy the 3 shield
generators (generator 1 damageable only from the "Destroy all the shield generators" banner) -> core, tractor the power
cell, exit -> Omicron Minor hole -> "Destroy all of the Nomad ships" (the trigger is within 8 km of Toledo) -> land on
Toledo. **A 600 s countdown from launch collapses the Omicron Minor hole** (`docs\missions\M12.md` step 2).

<!-- legtimes:M12:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 362 | 350 | 348 | 348 | 348 | 354 | 6 |

Our personal bests (full leg): 627 (10-22 13:59) -> 454 (10-23 06:26) -> 407 (10-23 06:36) -> 404 (10-23 06:46) -> 394 (10-23 07:07) -> 393 (10-23 07:27) -> 392 (10-23 13:32) -> 389 (10-23 14:23) -> 388 (10-23 18:02) -> 375 (10-23 18:12) -> 372 (10-23 19:58) -> 366 (10-23 21:07) -> 364 (10-23 22:16) -> 362 (10-23 23:47) -> 359 (10-24 02:00) -> 355 (10-24 02:00) -> 354 (10-24 06:08) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M12:end -->

**How each runner does it**
- Run 1: shop 31 s after the accept (Debilitator sold, Catapult, 40 Driller mines); cruise in formation with King / Juni
  (35-180 m); **holds at HUD 60 m from v=13348 and fires the second the banner comes (v=13360)**; still holds (< 1 m/s),
  torpedoes / missiles every 2 s, mines every 1-2 s; cruise charge on the kill, then 12 s afterburner at 199 m/s through
  the tunnels, generator 3 by cruise; n/b 29/29 untouched; interceptors never fought (BEH 1b / 5, `reference\frames\desc\
  r1_13290_M12_generators.jpg`; CMB).
- Run 2: stops beside each generator at 50-100 m (speed -10..30), weaves without net motion, empties ordnance (Slingshot /
  Catapult / torpedoes ~18 each, mines 26) (R2 P3; BEH 1b).
- Run 3: fastest generators (~72 s): short afterburner hops, holds 0-55 m (CMB).
- All three: **"Destroy all of the Nomad ships" ignored - 85-87 % cruise straight to Toledo, no launches, 63-66 s**
  (REF M12 last section; `reference\frames\r1_13515.0_M12_60s.jpg`, looked at: a steady straight cruise, Toledo growing).

**Section-by-section playbook (study of 23 Oct 2026, session 6 - the user: "before any further testing of M12 ...
thoroughly research how speedrunners played it")** - REF `ref.py M12 <section>` for all three runs, frames cut from run
1's video (v13441-13463), our motion trace `research\experience\legs\2026-10-23_0014_M12-toledo-trace_i2\FLMotion.tsv`:

| Section | Runners (best of 3) | How (all three unless named) | Ours (23 Oct) | What to copy |
|---|---|---|---|---|
| Launch -> lair ("Fly to the Waypoint") | 68-70 s | cruise charge at the launch, 299 m/s for 28-31 s (run 1 in formation with King / Juni at 35-180 m), afterburner hops 198 m/s at the structure; straight onto generator 1 | 87 s; 30 s fighting 5 interceptors first | no fights on the way (the interceptors are never fought); arrive at generator 1 before the banner |
| Generators (3) | 80-82 s from the banner | at generator 1 before the banner (run 1 still 23 s at HUD 60 m), fire the second it shows; per generator ~28-32 s; between them (1.5-2.6 km): the cruise charge starts on the kill and **runs to completion** (run 1 v13372 13 % -> v13376 94 % -> v13378 no message, 123 m/s; frames looked at 23 Oct - the automated "dropped after 3-5 s" was wrong), then 197-199 m/s for 10-15 s while the left HUD box drains 99 -> 50 % (v13381-13388) - **cruise or afterburner here is OPEN** (the user sees cruise engines; CAL #1 reads the draining box as the afterburner; elsewhere in the lair cruise reaches 300 with the box at 100 %), hard stop, hold still; run 2 cruises the longest hop; countermeasures on the hops; nanobots / batteries untouched (run 1) | 149 s: 23 / 38 / 40 s per generator, hops at 142-176 m/s by the pursuit, afterburner 0 % | afterburner hops between static targets (A3), the banner's moment as the start |
| Generators down -> airlock -> core -> cell -> exit | 20-22 s | one straight line, no stop: afterburner dive at the airlock (191-199 m/s, v13445), cruise across the core (228 -> 299 m/s, the core waypoint 1.6 km dead ahead, v13455), **the power cell tractored in passing at 299 m/s** (v13460), out of the exit at 286 m/s (v13463); run 3 is in cruise at the section start and keeps it throughout | 55 s: each step change stops and restarts the travel (cruise lost, 5-7 s re-charge at 80 m/s), a 0.1 s guard blip cancels the cruise wish, the arrival brakes at the core point | carry speed and cruise across step changes; fly through a waypoint whose trigger radius (700 m) the line crosses; tractor in passing; no cruise cancel on a guard blip |
| Exit -> Omicron Minor hole | 47-54 s | cruise 299 m/s straight 35-40 s to the hole (run 1 by the game's own Goto 68 % of it); "Defend yourself" ignored | 109 s (the airlock out, then the hole) | as above; Goto where as fast (A32) |
| Omicron Minor -> Toledo | 63-66 s | "Destroy all of the Nomad ships" ignored: 85-87 % cruise straight to Toledo, no launches | 226 s (escorted Juni 165 s: the Toledo goal blocked - fixed 23 Oct, untested) | the script goal (within 8 km of Toledo) |

**The user's play-by-play of the lair (23 Oct 2026; outranks the table above where they differ - CAL #10):**
- The player reaches the lair and **immediately targets the generators**; each gets full attention and is destroyed
  with maximum DPS.
- On each kill: **charge the cruise**, often **countermeasures pre-emptively** (the Nomads are trigger-happy with
  cruise disruptors); **cruise most of the way to the nearest generator in a mostly straight line, smoothly round the
  geometry as needed - minimal jerky motion if any**; at the next generator maximum DPS again.
- After the last generator: **afterburner straight to the core entrance, the cruise charge engaged just before the
  entrance** (the user's guess: entering the core turns active cruise off but does not stop a running charge - to be
  verified in game).
- In the core: cruise engaged, straight ahead to the waypoint; two obstacles taken **without slowing even a bit** - a
  rotating "turbine" and beyond it a small hole flown **dead centre**; the **power cell tractored without slowing**;
  cruise on to the exit.
- Clear of the exit port: **turn at once to the waypoint and full speed to the jump hole** before it collapses.
- The user: these are the pain points now - "VERY bespoke decisions and highly reactive maneuvering that is also
  delicate to extreme levels".

Gaps this study leaves open: the runners' line *inside* the lair between generators (which tunnel) is read only for run 1
gen 1 -> 2 (BEH 5); whether the cruise survives the airlock transit in the game (run 3 suggests yes); the HUD distance
convention at the generators (CAL #4, #8).

**Our autopilot**
- PASS full leg from Save5e67 (2 of 12 full runs: 5 failed on the collapse clock, 4 deaths; LEGLOG); from the lair 322 s,
  the three generators 92 s (runner pace, session 5). Gaps of the 627 s run (NAV, 2026-10-22_1348_M12_i3): **Omicron Minor
  -> Toledo +161 s** (fought 158 s); generators +49 s (since closed); exit -> Omicron Minor +45 s; core + power cell + exit
  +33 s; formation to the lair +20 s.
- Script goal "Omicron Minor -> within 8 km of Toledo" built in 4c7d3fe; in game (23 Oct) blocked twice - by the mark
  fallback, then by "Awaiting" read as the script's placeholder objective - both fixed (c7f51cf, c162ec6), the retest
  still open (HANDOFF NEXT 1). The user (22 Oct): "M12 is
  ignoring the objective and throwing itself at Nomads" (AP KNOWN PROBLEMS).

**Key notes for training**
- 2026-10-23 (session 8, the user: "compare failure points to the research data"; sections_dense + trace_run1 vs ours,
  research\tools\leg_path.py): **the runners' whole leg** - hull 0.70-0.86 on arrival in Omicron Minor, then no hull
  lost and no consumables there (nanobots never in M12, run 1 9 batteries in the core), 85-100 % cruise; gens -> core
  exit 20-22 s (charge on the way to the airlock, the core at 89-100 % cruise). **The jump hole (trace run 1):** cruise
  299 m/s down to 131 m, the jump scene from there (the request from range - the game's dock autopilot keeps cruising),
  the exit at 0 m/s with the charge started within 0.5 s, 300 m/s 9 s later (docks.tsv "cruise_kept" = the approach,
  not the exit). **Ours:** the request at 400 m, the game's dock slowed to 80 then 3-6 m/s (~13-15 s to the jump), the
  charge ~5-6 s after the exit; three Omicron Minor deaths had the charge reset by disruptors for up to 41 s at 80 m/s
  among 9 attackers with nanobots / batteries at 0 (the CMs were spent one per missile as budgeted). Built: 1ef7ea1 (a
  charge not done in 10 s -> 12 s afterburner), af80aa5 (consumable reserve ~15 %), a52ecae (request a gate / hole ~1.5
  km out while cruising). Open: start the charge at once after a jump-in.
- 2026-10-23 (session 8, **30b8fb1: 3 PASS 389 (best) / 393 / 403**, gate_s8u): the planner before an "out first" elbow
  and a failed plan retried once the ship is 60 m off its start. Over 12 earlier runs gen 3 -> core entry was 21.0-21.5 s
  whenever the first plan found a way (3 of 3), 23 with an elbow, 28-45 with "out + elbow" (8 of 8: legs beside the trench
  walls at 80 m/s under the guard, planned again 1-2 times); now 22.0 / 28.4 / 22.1. Also in this set: no 1.5 s
  waypoint-list wait per trip (a5caabb / dd64b64 - every lair trip paused 1.5 s before planning), the last Dock objective
  kept under a placeholder (73e8732 - s8s-i3: "Dock with Planet Toledo" -> the `dont_run` nag -> "Awaiting", followed Juni
  5 min, FAIL 724). Sections (best run): banner 71.2, gens 81.6 from the banner (try 3 74.2), gen 3 -> core 22.1 (~9),
  core -> cell 12.8 (10.8), cell -> exit 8.8 (2.5), exit -> hole 47.4 (42.1); Omicron Minor: 4 s frozen at the jump
  exit, ~8 s cruise charge at 80 m/s, then 300 m/s straight to the ring (near the runners').
- 2026-10-23 (session 8, way following, ff430f5 - a corner skipped when the next is in plain sight): 2 PASS **392** (best)
  / 405, 1 FAIL (death in Omicron Minor, 376 s: 7 Nomads, every cruise charge disrupted, batteries 0, hull 0.95 -> 0.08 in
  1 s). Gen 3 -> core entry **21.3 / 21.5 s** (c6ad02e's pair: 45.0 / 21.3 - the slow one circled the "out" leg's end 11
  s at 78 m/s, inside the turning circle). Each skip was followed by a new plan ("off the way" judged from the skipped
  corner) - cb151aa starts the way at the ship. Next gap: cell -> core exit 14.5 / 17.5 vs 2.5: the airlock grant
  restarts the trip, the fast dock brakes for the turn (not lined up yet) and drops the cruise at 296 m/s; on the planned
  way (line to p2 not clear in the tube) the burner never lit - judged by the nose to the door, not the carrot (ece2973).
- 2026-10-23 (session 8, after the leg gate - shared fixes for other legs, none aimed at M12): 0a94207 / 38f66d5 5 PASS
  (436, 402, 399, 423, 403); 54ecc2b 1 FAIL - death by attrition (all 50 CMs, nanobots and batteries spent in the lair;
  in Omicron Minor every charge disrupted at 80 m/s); **17fd2b2 (CM budget: 1 drop per missile under 25, under 12 only vs
  a cruise disruptor while charging) 3 PASS 409 / 423 / 393 (38 CMs left)**. Gen 3 -> core entry still ~20-30 s: from the
  kill point no way plans for ~4 s (the 25 m start offset did not help - the candidates are blocked there).
- 2026-10-23 (session 8, **3 full legs of b06d0f7** - elbows each second, airlock line-up lapses, rejoin 3 s ahead,
  M12-s8-f11-a/b/c): **2 PASS (417, 407 s), 1 FAIL** (c: died at the Omicron Minor hole - the fast dock's queue shortcut
  rushed to the entrance behind a Nomad going in and parked there under 5 attackers, hull 0.56 -> dead in 4 s; now: no
  rush under fire, a running rush ends when fire comes). Core -> cell 13.6-17.9 in all 3, the lair door 6-8 s, gen 3 -> core
  entry 21.9-29.1 (the first plan at the kill point still none; elbows 2-4 s later as the ship drifts - first legs of
  150 m added: the user's slide out was 158 m). Core exit 14.6-17.6 (to read). Exit -> hole ~42 in all 3.
- 2026-10-23 (session 8, **3 full legs of 250c218** - level out-first leg, M12-s8-f10-a/b/c): **3 PASS (393 - the best -,
  491, 404 s)**. Gen 3 -> core entry 24.2 / 61.6 / 21.9: the first plan from the kill point still none, an elbow 6 s later
  at the planner's retry (now: the elbows alone each second while no way is known); b: arrived over the lair door at 300 m/s
  in cruise, "lined" 61 m above it, overshot the 90 deg turn and the sticky line-up cruised out to 1.2 km - 25 s (now: an
  airlock line-up lapses outside its cone, and lined with the nose 45 deg+ off = engine speed too). b core exit 63.9 s: at
  the cell 128 deg off the exit, a lattice way into a wall, ~20 s at 3-37 m/s in the radiation (hull 0.40) until an
  "out + elbow" replan. Core -> cell 13.8-15.3 in all 3 (the rejoin at 3 s of speed, held through one failed test - built).
- 2026-10-23 (session 8, **3 full legs of 278acc9** - out first, then the elbow, M12-s8-f9-a/b/c): **3 PASS (402, 402, 423
  s)** - the tightest set. Hops now take "out + elbow" / "elbow" ways (0.1-2.6 ms). Gen 3 -> core entry still 30-31 s: from
  the kill point no candidate cleared (only climbs of 27 deg+ were tried). Ray probe (ap ray, instance 3): our kill point
  (659, -20, -1566) -> the user's (759, -15, -1444) clear; the user's climb to (75, 398, -1410) clear; the level leg to the
  airlock top (-1058, 415, -1294) clear - so a level first leg was missing (added: 8 headings x 3 climbs incl. level).
  Core -> cell 13.9 / 21.4 / 34.1 (c: slow - to read).
- 2026-10-23 (session 8, **3 full legs of a144c01** - elbow ways + the line-of-approach rejoin, M12-s8-f8-a/b/c): **3 PASS
  (429, 415, 394 s - the best)**. Control -> 500 m of the hole 239.6-260.3 s (try 3 209.4); generators 79-82 s from the
  banner; core -> cell 14.1-16.9 (the rejoin fired past the wheel in all 3); core exit 5.6-13.4; exit -> hole 42-56; Omicron
  Minor -> landed 75-96 (a: heavy cruise-disruptor fire). Gen 3 -> core entry 22-32 s: no direct elbow cleared from beside
  generator 3 in the trench (657, -19, -1574) - the lattice way went back east first; the user's try 3 killed it from the
  far side (x ~760, sliding +z) and climbed out at ~45 deg (next, built: "out first, then the elbow" - 8 headings x 2
  climbs, 250 / 450 m, at most 1.6x the straight way). Omicron Minor: ~17.3 km in ~70 s - charge from rest after the jump
  ~8 s, cruise ~55, dock ~7; within ~10 s of the runners.
- 2026-10-23 (session 8, **3 full legs of 504d7b0** - same-object trip kept + missiles at refire at a static hold,
  M12-s8-f6-a/b/c): **3 PASS (407, 430, 411 s)**. Missile salvos 16-17 per leg (were 3); generator 1 down 12.5-14.5 s after
  the banner, banner -> gen 3 79-95 s (kill ~10 s from arrival; the hop ~19.5 s from the kill to the first torpedo at 1 km
  is now the bigger part). The hole restart is gone. Gen 3 -> core entry still 30-36 s: the straight line to the airlock
  top is blocked, the first plan finds none (77 000 rays), then a 3D lattice way of 2.6-3.3 km for 1.4-1.7 km straight;
  the user's try 3: climb ~45 deg to y ~410, level along x at 200 m/s, drop into the door at 80 (~14 s). Core -> cell
  14-24 s: past the wheel the way round the hub at y -395 met structure (295 -> 70 m/s). Next (built): an elbow way first
  (one via point at the destination's height +-150 m, both legs ray-clear) and a fly-through rejoins its line of approach.
- 2026-10-23 (session 8, **3 full legs of 1f9a39e** - fix 5, trip kept, M12-s8-f5-a/b/c): **2 PASS (404 - the best -, 421 s),
  1 FAIL** (b: died at the Omicron Minor hole - "Take the Jump Hole" came 2 s after our granted request, the objective
  restarted the trip to the same hole, the game's dock was cancelled, the ship stopped 147 m out among 10 Nomads, dead in
  4 s; EVERY run restarted there - fixed: a trip to the object the running one goes to is kept). Fix 5 fired in all 3
  ("trip kept" 40-47 m): gen 3 -> core entry 22.1 / 33.4 s (was 35), core exit 5.8 / 7.5 s (was 11-14). Generators: one
  4-missile salvo per generator - the pilot's missile_launch_interval_time (20-45 s) paced the missiles while torpedoes
  went every 2 s (fix 6: at a hold on a static target missiles refire at the launcher's 2 s; 4 x 1212 per salvo).
- 2026-10-23 (session 8, **3 full legs of a061c04** - the four fixes, M12-s8-f1-a/b/c): **3 PASS (451, 407, 429 s; baseline
  498 / 454 / 516)**. All four fired in game: the lair door entered in 6.6-6.8 s at 80 m/s (was 22-30 s of circling), the
  power cell kept the trip (no turn), the right exit airlock (out 9.5-11 s), Omicron Minor -> landed 75-92 s (was 120; a:
  cruise disruptors reset the charge again and again). Gen 3 -> core entry still 35 s (a, b): the pre-stage trip ran at
  200 m/s toward the airlock top, "Fly to the Waypoint" restarted it to a point 47 m off - braked to 80, 1.5 s plot, a 3.2
  km lattice way back (fix 5, 1f9a39e: a trip to within 150 m of the running one's destination keeps it). Core exit 11-14 s.
- 2026-10-23 (session 8, AFK, **baseline: 3 FULL legs of 84bd318 from Save5e67**, M12-s8-base-a/b/c): **3 PASS (498, 454,
  516 s; the full-leg best was 627)**. Base 58 s (accept + launch), control -> 500 m of the hole 261-305 s, Omicron Minor
  -> landed on Toledo **120 s both runs read** (runners 63-66). Sections: gen 3 -> core entry 21.5 / 53.2 s, core -> cell
  14.6-25.7, cell -> exit 8.7-14.7, exit -> hole 47-57. Read from the logs: (1) Omicron Minor: the trip to 0.7 x 8 km of
  Toledo *arrives* (52 m/s, cruise lost) and charges again for the 3 km ring trigger it armed (~20 s); the first cruise
  charge after the hole took 12.6 s under 9 attackers (cruise disruptors). (2) The lair top: the airlock run circled the
  door in cruise at 290-300 m/s, 400-740 m off its axis, 22-30 s, until it fell to 80 m/s and entered in ~7 s. (3) The
  power cell spawns ~845 m off: a trip "to" it turned the ship about at 297 m/s; the exit then 164 deg behind (r3-c: 36 s).
  (4) i3: the exit grant picked the ENTRY airlock (nearest within 3 km) - two refused requests. Fixes in a061c04.
- 2026-10-23 (session 7, **3 runs of 84bd318** - the path follow plans again when 250 m off its leg or 8 s without
  progress; M12-s7-r3-a/b/c): **3 PASS (499, 482, 477 s)** - the first fully reliable set. Generators 93-95 s from the
  banner, gen 3 -> core entry 55.4 / 45.6 / **22.0** s, core -> cell 14-17 s, cell -> exit 11.5 / 12.8 / 36.2 s, control
  -> 500 m of the hole 284-298 s (best 273.1, try 3 209.4). The core trap (z3-b) did not recur. Next gaps vs try 3:
  generators ~20 s, gen 3 -> core 13-46 s, the exit leg 9-34 s, Omicron Minor -> Toledo.
- 2026-10-23 (session 7, **3 runs of 75a2d8f** - the airlock's planned way only beyond 400 m of p2, M12-s7-z3-a/b/c):
  **2 PASS (463 s - the best from M12_ck_launch -, 485 s), 1 FAIL** (b: stuck inside the core after the wheel, the
  travel's unstick x4 at 0-2 m/s - stopped). The airlock is fixed: all three entered (gen 3 -> core entry 36.2 / 53.3 /
  33.5 s). Generators 92-96 s from the banner, hops 34-45 s, no hop stalls. Best control -> 500 m of the hole **273.1 s**
  (try 3 209.4). Core: a 14.1 + 12.5 s, c 34.5 + 9.5 s, b stuck - the core travel is the next unreliable part.
- 2026-10-23 (session 7, **3 runs of f718e4a** - the beside-the-door rule reverted, M12-s7-y3-a/b/c): 1 PASS (494 s),
  2 FAIL (b: airlock stall; c: airlock 105.6 s, then the core travel "arrived-close" 1.6 km short, a 3 km way for 890 m
  straight, the cell tractored from 600-860 m without coming in - the hole collapsed at 600 s). Generators 88-107 s from
  the banner. The circling was NOT the reverted rule: the airlock run's line check used the aim inside the airlock (never
  clear), so it always planned to p2 and, near p2, orbited it at engine speed (210-380 m from the mount = around p2, 302
  m above it). Fix built after these runs (untested): the airlock's planned way only beyond 400 m from p2, judged by the
  line to p2. Also open: the core travel (arrived-close against structure, long lattice ways).
- 2026-10-23 (session 7, **3 runs of 10bff5c at once**, M12-s7-x3-a/b/c): 1 PASS (521 s), 2 FAIL - both a 60 s STALL at
  the lair airlock. Banner -> gen 3 112.6 / 109.5 / 100.2 s (gen 1 16-19, gen 2 39-43, gen 3 39-55: the hops are
  steady now - no stalls, no wedges). **The airlock is the regression**: all three arrived at its top fast (b, c at 300
  m/s, the nose ~90 deg off the entry, 47-84 m from p2), the "clear line from beside the door - straight in" rule
  (9f8331f) took over, and the run circled the door at 77-80 m/s, 210-380 m from the mount - a got in after ~50 s
  (gen 3 -> core 65.5 s), b and c never. Core 14.3 + 13.7 s (a). Verdict: revert that rule (and its turn-speed
  companion), keep p2 as the plan target; then 3 runs again.
- 2026-10-23 (session 7, after the 480 s PASS - the gen 3 -> core work): 6 runs, 2 PASS (518, 554), 4 FAIL (hop stalls in
  the trench x2 with the 3D lattice, a 153 s generator 3 under 16 attackers with evade flips, a stall at the airlock dock).
  Gen 3 -> core entry 40.3 -> 34-35 s (airlock straight in from beside the door; the airlock plans to p2); the core 14.8 /
  13.5 s (the lattice found a way through). Hop times vary 31-86 s run to run (cruise disruptors reset the charges;
  countermeasures now drop while charging). Lesson: run-to-run variance is large - judge a change by 2-3 runs, and a
  path planner change touches every leg's travel (the decision sweep + leg gate are the guard). Uncommitted to a verdict:
  85cad5a (hops on planes again + stuck back-off) and the no-dodge-while-hopping rule, both untested.
- 2026-10-23 (session 7, last run M12-s7-holewait, 005a7bb): **PASS 480 s from M12_ck_launch** (that leg's best was 718)
  - control -> 500 m of the hole **284.1 s**: banner 71.2, generators 90.6 from the banner (18.7 / 40.7 / 31.2), gen 3 ->
  core entry 40.3, core -> cell 14.3, cell -> exit 20.9, exit -> hole 46.9; Omicron Minor -> landed on Toledo 121 s
  (was 226; the 8 km trigger, then the 3 km trigger it arms - GoalsArmedBy - then the fast dock; nothing fought). The
  run before died 440 m short of the hole: the game's dock parked the ship among 10 Nomads (now: circle the entrance).
- 2026-10-23 (session 7, the user watching every run; `research\tools\leg_sections.py M12 <log> <motion> --ref <try 3>`):
  control -> 500 m of the hole **298.6 s** (was ~386; try 3 209.4). Sections ours / try 3: banner 71.1 / 70.6 (script-
  paced), generator 1 **15.9** after the banner (pre-stage hold through the dialogue, torpedoes every 2 s), gen 1 -> 2
  46.1, gen 2 -> 3 40.5 (try 3 ~37 each), gen 3 -> core entry 41.2 / ~9, core -> cell 20.0 / 10.8, cell -> exit 16.5 /
  2.5, exit -> hole 47.3 / 42.1. Fixed this session: the lair's own hostile hull behind a generator held the guns (60 s
  STALL); fights during the 31 s dialogue and after generator 3 (pre-stage: the next step's targets); the core crawl at
  8 m/s (sphere avoidance inside the radiation zone); the staging arc over the lair (80 m/s); fake "arrived" + threat
  fights; the core exit's 4 s wait; Omicron Minor stood by Juni after the 8 km trigger (the armed 3 km trigger is the
  trip - untested). **The user's corrections:** mines add DPS but only from a lateral run along the hull at near-zero gap
  ("a pure lateral run along the hull is even better than what I did" - the reverse taps were the pad layout's
  convenience): hold gap 20 m, 35 m/s along the hull, the gap kept outside a loaded launcher's blast (+10 m - at 15 m no
  torpedo went); the pre-stage hold faces the generator (pitch too); between generators "a concave line ... within the
  trench along its inner perimeter" - try 3 dips from r 1900 (the generators, from the airlock axis) to r 1350-1500 (A
  -> B) / 1100 (B -> C), y within +-50, keeping the middle between the torus' upper and lower parts ("identical distance
  ... above and below the flight path"); the turbine: "flew too close to the turbine's center"; no fights after the
  core exit. Open: the hops still zigzag (planner: 80 m berth first + centring, untested), the airlock top overshoots
  at 300 m/s into a vertical entry, the core's planner finds no way (up to 24 000 rays - capped at 8 000), the wheel scan
  reads -48 deg/s and a 390 m ring (more than the wheel in the grid), the exit leg 16 s.
- 2026-10-23 (session 6) **TIME TO BEAT - the M12 opening, control after the launch scene -> 500 m from the Omicron
  Minor hole: ~202 s** (the user's try 3, "an excellent run": 209.4 s actual; corrected by the user's own reading:
  afterburner straight from the generator-3 kill instead of a cruise charge -2.7 s, no target selection on the two hops
  -2 s, turn to the hole at once on the core exit -2.7 s). Sections (actual / to beat): control -> generator banner 70.5
  (script-paced: the 2.7 km trigger + ~31 s dialogue; at generator 1 before the banner), banner -> core entry 83.3 / 78.6
  (generators 74.2 s from the banner - **beats all three runners' 80-82**), core 13.3 (run 1 ~13), exit -> 500 m of the
  hole 42.3 / 39.6. Our autopilot today ~386 s over the same span. Reference files `research\experience\reference\
  M12_user_2026-10-23_try3_*` (motion trace, log excerpt; video `research\experience\shots\2026-10-23_013408_user_M12_try2`).
  How: cruise hops between generators (charge on the kill - 8 s incl. the ramp - ~7-9 s of cruise over ~2.3 km, brake
  245 -> 110 m/s in 0.4 s), throttle 0 while holding at a generator, one break-off to the next generator while the last
  salvo was still in flight (the user: "the correct assumption the generator I'm leaving is going to be destroyed" -
  A30, launch / leave on the kill); afterburner to the core entrance, the charge started 1.9 s before the entry at 122
  m/s; in the core straight from the entry, out ~175-195 m left of the axis through a turbine gap at z ~400 at 240-301
  m/s (the wheel: 6 spokes, 36 deg/s - M12.md "The core"), back on the axis through the small hole at z ~-600 at 300
  m/s, 20-52 m off the axis to the exit; the active cruise survives the exit transit (59 -> 296 m/s in 1 s); out of the
  exit 3 s downward (-y, the exit's direction) before the turn, then a straight cruise to the hole. The user also: their
  shot timing at the generators was not 100 % (windows not fully used) - beatable there too.
  **Countermeasures (the user: "I spammed countermeasures so much that I actually used the last ones out of the core" -
  the Nomads persistent with cruise disruptors):** 50 at the launch -> 45 (~14 s after the banner) -> 33 (hop 1 -> 2) ->
  12 (hop 2 -> 3: 21 dropped in ~16 s) -> 7 (generator 3) -> 3 (core entry) -> 0 near the hole; batteries 29 -> 20 in one
  burst of 9, nanobots untouched. The incoming cruise disruptors are not in the log (player flight) - an autopilot run
  through the lair logs them (incoming missiles, cm outcomes): budget the 50 over the whole leg, drop only against a
  disruptor in flight toward us during a charge, keep a reserve for the exit -> hole cruise.
- 2026-10-23 (session 6, the user's own run as the reference - main install, motion trace player rows; the user: rusty
  approaches, a reversal at generator 1, "mediocre orbits", the hop to generator 2 broken by cruise disruptors - no
  countermeasures in time, **the hop 2 -> 3 "mostly on point"**, a crash into the turbine): generators **83 s** (runners
  80-82, ours 149); hop 2 -> 3 = charge 5 s + ramp ~3 s + cruise ~7 s over ~2.3 km, braked to 113 m/s at the generator;
  generators down -> out of the core 27 s **with the crash** (runners 20-22, ours 55); core entry already charging (16
  m/s), 311 m/s by z 390, the turbine hit at z ~365 ~150 m off the axis (M12.md "The core"), then the axis through the
  small hole at 260-299 m/s. Copy: cruise hops between generators with countermeasures before the charge; the turbine
  needs its real position / rotation (measured: 6 spokes, 36 deg/s, pivot on the axis at z ~400; the ray test sees it as class 0x1 hits - M12.md).
- 2026-10-23 (session 6): PASS 667 / 629 s. Fixed: the hold re-anchored on the lair's lattice 3 km short of a shield generator (8 min, the collapse clock failed it) - now only the target's own hull. Toledo: the goal was blocked twice (the mark fallback; then "Awaiting" = the script's placeholder objective) - both fixed, retest. User: travel inside the lair very sub-optimal, collisions near the base. Our sections vs best of 3: generators 149 vs 80-82, generators down -> exit 55 vs 20-22, exit -> Omicron Minor 109 vs 47-54, Omicron Minor -> Toledo 226 vs 66. Motion trace: inside the lair the cruise is wanted ~90 % at 119-198 m/s mean, p10 80 (the charge breaks on the turns), afterburner 0-1 %; runners 237-299 m/s - afterburner in tight geometry next.
- 2026-10-23 (session 9, AFK, sets s9a-s9y = 75 full legs from Save5e67; verdicts per change, git log for detail):
  **s9y 367 / 373 / 364 (best set, best 364)**; s9k-s9y means 368-431, 4 deaths in 45 legs (all in the breakout or
  Omicron Minor after a lost cruise). Kept: airlock run - no list wait under the cruise lock, the lock gives way after a
  pass beside the door (59a015d), the guard leaves a lined-up run (e1c7016); static hold - gap >= the launch need, push
  back with 8 m overshoot (223e12d, b011598), close static hull = on-line (756ea21); pursuit brakes by the model's stop
  (05f6219: was 3 brake-burn cycles per generator approach); a failed way plan retried 60 m off the nearest solid
  (d1a0705: gen 3 -> core 26-28 -> 19-20 s); lair top - the door's line read from its dock bay, staged 50 m out, the
  run curves in level with the mouth like try 3 (60e4f90, f3106c8, 3524a65); **the core exit "hit at z -1100" is a
  STATIC PLATE (probe; M12.md), not spokes** - PassageVia: the passage point 50 m to the side, then a line parallel
  through (d0df954, 3b6417a, 79ead46: 3 of 3 passed in s9x); the wheel's crossing ring = the smallest half free
  (0a40dba: the 410 m rings lost ~20 s at the hub); a charge reset under fire = 8 s afterburner at once (4e6e6ef);
  under fire a lined-up fast run keeps its speed to the hole's normal request point (bf7dc5d, untested). Reverted: the
  fly-through centre over an airlock (no cruise to the lair, +26 s), the rejoin's speed margin (hub circling), the
  "far spokes" wheel timing (wrong hypothesis: video misread - the probe settled it). Measured: the user's "burn,
  engine kill, charge" does not hold speed here (FREELANCER_RE "Engine kill"). Open: Omicron Minor deaths (followers
  through the hole + batteries spent before), the lair still ~10-15 s behind try 3, consumables drained by the exit.
  Lesson (the user, 23 Oct: "drifting?"): probe a mechanism before fixing it; 54 shared commits went untested on the
  other legs - the gate s9gate ran them: 11 PASS, no regression (M11 mine, M13 / M09 known). **s9z (bf7dc5d: the hole
  at speed) 372 / 395 / 370 / 362 (best 362), no deaths.** Remaining M12 gap (s9y-s9z vs try 3): the lair 92-101 s
  vs 83 (hops lose cruise to guard swings off their way in ~2 of 3 runs - 4852aeb untested); core, exit and hole within
  ~2 s of try 3; the base (58 s) and Omicron Minor -> Toledo not yet compared with the runners.
- 2026-10-24 (session 10, the improvement session; sets M12-s10-<v>, 6 legs a side, `splits.py compare`, routes
  `tools\routecmp.py`): **par's M12 split was a banner artefact** - run 3 (par) crosses door + core in cruise, the exit
  flash at v13730.2 (frames), its "Fly to the Core / Tractor / Exit / Defend" banners a ~4 s-each display queue after it;
  checkpoints.tsv now hand-reads r3 core 222.7 / cell 229.1 (estimate) / exit 231.4. Real gaps: inside the core (cell 11 vs
  ~6) and the hops, not exit -> hole (ours 54.5 = run 3's 53.8). PB 355 (s10-i), sum of golds 340 (par 348).
  **Kept (switches, off by default until the gate):** `ring_request 10` - Toledo landed at p2 at 300 m/s (probe: the scene
  0.03 s after the request, dock 25.0 vs 30.8 s; set B M12.end -6.0 s, 6 / 6); `airlock_charge_at 0.5` (the user: 50 % at
  the core entry); `wait_flies` + `airlock_burn` (the way in only: gen 3 -> core entry 17.8 -> 15.4 s, new gold 14.3);
  `tractor_exit_run` (the exit run at the cell's tractor - in its first run the core exit 5.3 s ahead of par).
  **Dropped (made it worse):** `rejoin_nearest`, `wheel_parallel` (a near-crash along the hub), `travel_passage` (dodged the
  core's ~90 m middle hole), `arc_bulge` (fired inside the core, 4.3 km arcs at the generators), `noway_climb` (never fired);
  `airlock_burn` at the core exit (7 fail states, sets C-G: the exit run circled the hub and missed the door).
  **Open:** gen 1 -> 2 route - the probe (research\experience\misc\m12_lair_wall_profile.tsv + m12_lair_scan_top.txt): the
  outer wall bulges from x -2080 (z 0) to -2666 (z -1250), pylons jut 250-450 m out at z -200 / -700 / -1100 / -2100,
  the user's try 3 runs 140-260 m outside the wall within ~50 m of the pylons. **The user chose the 3D map** - done
  (instance 4: 2,320 radial rays y -150..150 x z 600..-3000 + six along-trench scans, research\experience\misc\
  m12_lair_wall_3d.tsv / m12_lair_pylons_map.txt / m12_lair_fins_scan.txt): the "pylons" are **hoop frames** across the
  trench, 25-50 m thick, 180-240 m tall, outer posts 250-450 m off the wall, at z ~-210 / -690 / -1110 / -1575 / -2090 /
  -2260; try 3 threads every window (at z -1112 only ~60 m tall at the crossing, diagonal struts across). `hug_route` v2
  (the wall behind the posts, trench-middle heights, window search) found the first hoop's window but its legs clipped the
  struts (probes 3-4: fell back). `hop_narrow` (the 30 m berth when 12 % shorter) did not win at the first plan; the
  re-plans used it. Probes 1 / 5 (hop_cruise_hold, no hug_route): gen 1 -> 2 29.6 / 29.0 s kill to kill, on the user's
  line (mean 58 m, max 170) - the remaining hop gap is gen 2 -> 3 (30.6 s, 15 s under 100 m/s vs the user's 5). Probe 5
  also looped gen 3 -> core entry (1 km off route) and the core exit (6.1 km flown, 23 s) - open.
  **Sets J / K (~04:15):** the hop flip-flop's cause found and fixed - the game's cruise flag reads 0 while charging, the
  hop priced a fresh charge each frame (`hop_cruise_hold` now times its own charge: 2-3 toggles a leg, was 94-149);
  `passage_window` (the opening's centre: the middle hole centred, the plate's open side found); `tractor_exit_range` 600
  (the early exit run before the middle hole turned the ship about - set J fail); `hop_reset_burn`. **Set K = all kept
  switches: 6 / 6 PASS but median 379.4 vs set B's 364.2** - gen 3 -> core entry 24.4 vs 17.8 (the planner's "none" x3
  from the kill point, 2.5-3.6 km ways; `noway_climb` untested there), the core exit still looped in 2 of 6 (24 s, the
  cruise held), gen 1 -> 2 off the user's line in 4 of 6, Omicron Minor disruptors (80-88 s) in 2. **Best config so far =
  set B (ring_request 10, airlock_charge_at 0.5, rejoin_nearest 1): 6 / 6, 364.2.** M12 not at par (PB 355, +6); the
  remaining gaps need the core entry / exit flows reworked as one design (probe the core like the trench), not more
  one-switch fixes. The hop's cruise
  flip-flop (94-149 toggles in 7 of 15 legs) is intermittent; `hop_cruise_hold` did not stop it - the hop event now logs
  the plan. A death at generator 2's hold (batteries 0, the "exposed" orbit 0.3 s before the killing torpedo). Exit ->
  hole when early: a Nomad collision near the hole, the under-fire request at 80 m/s (+8 s).
- 2026-10-24 (session 11, AFK; segment practice from checkpoint saves - TESTING.md "The test loop" 4): **M12 checkpoint
  saves** banner..hole (label M12-s11-saves; a native save costs ~120 ms, no stall). Segment banner -> gen3 noise (set
  s11k, 18 runs, in effect A/A): medians gen1 15.4 / gen2 33.6 / gen3 28.7 s. **Generator ordnance** (the user, AFK: "some
  generator runs don't quite make full use of torpedoes and mines"; `tools\holdstats.py`): no mine ever dropped (the mine
  gap 40 m < the ~50 m hold the torpedoes' blast need sets); ~45 % of holds (mostly gen 3) had a 6-8 s torpedo gap -
  the hold settles at a 40-45 m hull gap there, under the launch need (~47); the 2-torpedo floor left the last ~5,000 hp
  to the guns. Set s11o (6 / arm): `leave_on_kill` v2 (finishing torpedo + the predicted kill) -0.3 / -0.8 / -0.9 s, the
  prediction right 13 of 13 (kill 0.0-2.0 s later); `static_hold_hullgap` gen 3 +1.1 s, `ready=0` 10 -> 19 (did not fix
  the settle - salvo-check now logs hold-gap / torp-need / to-point: probe before another try); `static_hold_mine_gap 60`
  ~100 mines, no gain (326 hull each: too weak to matter). Confirm set s11p (a vs b, 12 each). **Omicron Minor -> Toledo**
  (38 runs of 23-24 Oct): no charge reset - mean 65.2 s (par 63.3); with resets (13 runs) 77.7 s, ~5 s per reset: the
  Nomads' cruise disruptors (nomad_cruise_disruptor01: seeker 2.7 km, ~1,000 m/s, 3.1 s, 530 hull; refire 0.5 s) hit
  the charge right after the jump-in; the reactive CMs (one tracked missile, 192-429 m) come ~0.4 s before impact, with
  4 in flight. Set s11g (full legs, 6 / arm): `charge_cm` resets 1 / 9 Omicron runs vs 2 / 10, 8-34 CMs a leg - not
  proven, off; `jumpin_burn` never fired (the M12 hole is the objective's jump, not the travel mode's - fixed in v3).
  The M12.hole and M12.gen3 saves are unusable (TESTING.md "The test loop" 4). **Core fails** (the cruise dropped / the
  exit not reached): with `airlock_charge_at 0.5` 17 of 71 legs (24 %, rejoin_nearest on or off alike), without it 0 of
  6 here and 0 of ~76 gate legs before session 10. Mechanics (motion traces, s11g-b / s11h-a): after the turbine the
  rejoin leaves the ship ~200 m off the axis at the hub (the ~90 m hole at z -500) at 290 m/s - ~10 s of loops; at the
  plate the passage took the first clear opening (50 m to the horizontal side) and left the ship ~140 m off the exit
  door's axis ~150 m before its p2 (z -1235) - missed, the cruise lock dropped. Run 3 / try 3: turbine gap at r ~190,
  on the axis through the hole, over the plate at (4, 52), the door on the axis. `passage_axis` (the opening nearest the
  door's axis) did not apply in s11h (each trip resets its state; v2 keeps the last door read). s11h (27 runs from the
  banner save, all arms base in effect): 20 PASS, 5 core fails. Set s11i (9 / arm): without `airlock_charge_at` 2 / 9
  core fails as with it (the hypothesis not supported - the user's 0.5 kept); `passage_axis` (fired 6 / 9) and
  `jumpin_burn` v3 (fires at the Omicron Minor jump-in; +2 s there) no gain, off. **`ref_line` (kept, speedrun
  knowledge): the user's try-3 line through the core, pure pursuit, rotated about the exit door's axis to this run's
  turbine crossing - set s11s (banner save -> end, 12 / arm): 12 / 12 PASS vs 9 / 12 (all 3 base fails in the core),
  finishing median 238.0 vs 249.4 s; 10 of 12 crossings 13.3-14.6 s at 7-14 deg/s mean turn (try 3: 13 s).** One run
  rotated -81 deg and struck structure at z -228 (the core is not symmetric there; it looped 52 s, kept the cruise) -
  the rotation is now ray-tested and stepped back toward try 3's own azimuth until clear.
1. Toledo (+161 s): test the script goal - fly straight to Toledo, fight nothing (A21; user's report).
2. The 600 s collapse clock fails most full runs: budget the leg from launch; the runners need ~329 s launch -> Toledo
   (run 2, R2 P2).
3. Exit -> hole and core / power cell (+78 s): afterburner straight along the tunnel axes at ~200 m/s, cruise above
   ~1 km (BEH C1, A25); the geometry guard's 80-140 m/s and jitter are the known blockers (AP KNOWN PROBLEMS).
4. Generators: still at the hold (all runs: no slide here, CAL #5), fire on the banner, mines behind.

## FT12 - Toledo
Only the Toledo room scenes (landing flyover, Juni, Orillion's M13 offer) and the Accept.

| Run 1 (WR) | Run 2 | Run 3 | Best |
|---|---|---|---|
| 246.5 | 248.5 | 248.9 | 246.5 |

autopilot: not built yet (FT1 money step probe only). Our M13 leg starts at Toledo; the scenes are Esc-skipped.

**Key notes for training**
1. Esc and accept; refill batteries / nanobots full here (the Nomad battleship burns 27-29 batteries, CMB).

## M13 - Into the Dyson Sphere
Toledo -> defend the Order base: Nomad interceptors and the Nomad battleship -> Toledo docking order, dock and relaunch ->
escort Sinclair to the Osiris -> Osiris (refill) -> follow Orillion to the Nomad gate (fight at the gate) -> the barrier:
destroy the generators -> enter the Dyson sphere (airlock) -> the city generators -> the final cutscene.

<!-- legtimes:M13:begin -->
| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |
|---|---|---|---|---|---|---|
| 752 | 702 | 686 | 686 | 686 | 1194 | 508 |

Our personal bests (full leg): 1194 (10-22 11:35) - what changed: LEGS.md "Personal-best history".
<!-- legtimes:M13:end -->

**How each runner does it**
- Run 1: 20 s of interceptors, then **cruise straight at the Nomad battleship to 0 m**; at the hull speed pulses 50-185 m/s
  with afterburner bursts every 5-7 s, **batteries 9 / 10 / 8 at once on each shield failure** (`motion\frames\
  B2_r1_m13_nomadbs.jpg` per BEH 2b); Osiris softlock 36 s after a load-skip (R1 (f)); **barrier: 4 generators in 74 s** -
  cruise cut at 188 m, stop at HUD 93-100 m, holds 60-100 m with a **one-way slide to the ship's right** (3 of 4 holds),
  torpedoes / Catapult / Slingshot every 2.0 s, Driller mines every 1-2 s only while holding, batteries 9 at once twice,
  nanobots 28 kept, afterburner hops 4-8 s (BEH 1b / 1c, CAL #1 #5 #6, `reference\frames\r1_14398_M13_barrier.jpg`);
  **city: cruise ~2 km above the towers, holds under each satellite's cone at HUD ~200 m, ~20 fighters ignored, nanobots on
  the way out** (`reference\frames\r1_14514_M13_dyson_gen1.jpg`, BEH 5).
- Run 2: Nomad battleship 0 m, batteries 29 -> 0; docks the Nomad gate while the battleship lives; barrier holds 34-60 m
  with a right slide; **city satellites from inside the model (0 m)**; ends with no batteries (R2 P3; BEH 1b).
- Run 3: no Osiris softlock; barrier ~63 s with nanobots (no batteries aboard); city ~132 s (CMB fights.tsv).

**Our autopilot**
- PASS full leg from Save3989 once in 11 runs (8 deaths) - and that 1194 s pass flew the sphere letterboxed (the airlock
  lockout, fixed session 4: airlocks are flown through). Checkpoints: from St03 470 / 500 s with hold-and-salvo (barrier 124
  s for four; 4 of 16 runs passed), from the city 255 s (runners ~120 s) (LEGS.md, LEGLOG).
- Gaps of the 1194 s run (NAV, 2026-10-22_1115_M13_i4): **Dyson city +293 s**, **Nomad battleship + Toledo stop +120 s**,
  barrier generators +87 s, Sinclair escort +36 s. Faster than the runners: the Nomad gate (73 vs 84-112), the Osiris stop.
- 4c7d3fe tested 23 Oct 2026: the re-anchor follows the **target's own** hull only (it had met the sphere's towers 2 km
  short); slide 5 deg/s, mines every 1.5 s, exposed = faster slide in use (`static-hold exposed` events).

**Key notes for training**
- 2026-10-23 (session 6; user: "failed as soon as the player ship targeted a nomad ship rather than fly straight at the generators in the trenches"; "did not move with intent into the airlock"; "engaged some nomad fighters after it got in the dyson sphere rather than immediately leg it to the nearest generator"): script destroy goals - straight at the barrier generators (164-202 s from "Head to the Barrier" incl. the 11 km in; runners 106-124), the airlock run no longer restarted by threats (15 s to the transit), the city's generators rushed from the entry. From St03 PASS **422** s, from the city PASS **171** s (and 174); one city run died under 12 attackers 42-119 m off a 915 m generator with 0 batteries. Next: consumables (all 28 nanobots + 29 batteries went at the barrier one at a time; run 1 bursts 9 batteries on a failing shield, keeps nanobots for the city) and the city hold position (run 1: under each generator's cone at HUD ~200 m, ~2 km above the towers).
1. City (+293 s): cruise above the clutter straight to each generator, stop 0-200 m, salvo, leave on the kill; ignore the
   fighters (user: "inside the Dyson Sphere you can bumrush the generators and ignore the fighters completely", AP).
2. Nomad battleship (+120 s): 0 m with speed pulses every 3-8 s and **battery bursts of 7-10 per shield failure**, fire
   on every alignment (BEH B2, A31); cruise to it after the first interceptors instead of fighting them 36 s.
3. Barrier (124 s vs 63-87): slide to the right, batteries in bursts (~9) instead of letting the shield fail, launch from
   ~1 km on the approach, afterburner hops (A16 / A31, HANDOFF NEXT 2).
4. Survival: 8 deaths in 11 full runs (Nomad guns 1630 per bolt at 40-115 m, AP): the hull floor and battery budget come
   before speed here; test the 4c7d3fe hold rework on St03 + full M13 first (HANDOFF NEXT 1).
