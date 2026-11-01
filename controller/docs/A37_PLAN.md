# A37 situation assessor + teleo-reactive arbiter: the implementation plan (WORK_QUEUE Q6)

Drafted 30 Oct 2026 by a Plan agent (Opus high) at HEAD 48cb87c, read-only; the slices are the queue's order for Q6.
Ideas: `..\..\docs\research\IDEAS.md` A37-A42, `..\..\docs\research\autopilot-planning.md`; product design:
`AUTOPILOT_UX.md` 2-3.3. File:line citations are at 48cb87c.

## 0. Where the code stands
- No queue: `goals.h:8-9`, the QUEUE tab "ONE GOAL AT A TIME" (`goals.cpp:361-363`); DO NOW replaces the running goal
  (`goals.cpp:399-430`).
- Every activation is cold: `StartObjective` resets `g_obj = ObjectiveRun()` (`autopilot.cpp:482-491`), arm the same
  (`autopilot_settings.cpp:428-436`); a stop ends the objective mode (`autopilot.cpp:251-253`): the ~50 fields of
  `ObjectiveRun` (`autopilot_impl.h:924-981`) are lost on every toggle. Leg.ps1 runs every space phase cold (`ap start
  arm` / `ap start objective` / `ap stop`, `controller\tools\missions\Leg.ps1:441,450,504`).
- Memory never re-proven (outside g_obj, survives toggles and loads): `lastReal` (`autopilot_objective.cpp:680-682`),
  `s_seenAlive` / `s_seenListed` (`:240`), the PreAggro statics (`:1619-1621`, `:1663`); `core\game\mission.cpp`
  `g_shown` (`:297`), `lastScript` (`:301`), `lastText` (`:358`) behind `fromLast` (`:380-389`) and `Leashes()`
  (`:303-307`); `mission.h:69-71`: a leash's arming is not tracked.
- 4c4fb6f's class (memory written before the decision it stands for): `ObjectiveSupervise` wrote `stepKey`
  (`:1767-1796`) before `ObjectiveDecide`'s loop guard returned early (`:317-333`); fixed at `:331`, the class remains.
- `ObjectiveDecide` `:311`-~`:1597`, ~55 `return ObjectiveStart(...)` exits, reads and side effects mixed, English verbs
  (`:106-144`). Driven per frame from `UpdateModes` (`autopilot_frame.cpp:215-218`), a 500 ms look (`:1727-1730`); mode
  changes `controller.cpp:2431-2441` (NoteMenu, HostObjectiveReread).
- Readers exist (no HostApi bump): ObjectiveStep (`controller\src\api.h:11-19`), marks, refs, title, steps, spawns,
  dock reply, ObjectiveReread (v11-v25); `mission::LiveTriggers` (`core\game\mission_live.h:23-45`), unused by the
  autopilot yet (`mission.h:11`).

## 1. Design
**Assess (pure):** `Situation Assess(const Snapshot&, const AssessPolicy&)` in `controller\src\impl\situation.{h,cpp}`,
own plain structs. `Snapshot` = the facts of one adapter `SnapshotNow()` (`situation_snap.cpp`, the only impure part):
objective seq / step / steps / title / marks / refs; the story state and LiveTable rows; the script facts mission.cpp
derives (AdvanceGoals with fromLast / byTitle, Leashes, NextStepGoals, TowardsNag, PendingHostileShips); system, base,
mode; pilot (lane, jumping, op, leader); kinematics, hull, shield, cruise; MissionHolds, GameDocksUs, dock reply, mission
result; contacts (the 48 nearest + every marked / referenced one); waypoints; lane links / open state; spawns since the
last menu; knowledge scope and the settings the rules read. Fixtures: ids and numbers only, script paths relative (the
pre-commit hook refuses profile paths). `Situation` = the step (stepKey from `:1773` + sorted marks, placeholder 21660 /
null / nn_blank, the stand-in's source live / ini-last / title), its kind (A41) with `kindSource` (job objective_text
id, NNObjective type + Cnd_, verb, unknown), subject, held reason, threats (`:204-234` + pre-hostile), constraints
(leashes armed by live state 0, nags, never-attack labels), script goals tagged live / ini, affordances, per-candidate
achieved, a hash.
**When:** on events only - objective seq / marks, system, dock / launch, death / respawn, a load (`controller.cpp:
2439-2440`), a sub-mode's end, every activation; the 0.5 s tick keeps a cheap partial re-proof (contacts, distances,
threats); LiveTriggers only on the events (IDEAS A39).
**Arbiter:** `Proposal Decide(const Situation&, const Queue&)`, ordered rules (first that holds wins), the order of
`AUTOPILOT_UX.md:207-223` and today's ranking: Held (`:1679-1734`) > leash (`:349-386`) > transit > script-destroy
(`:563-640`) > dock entry (`:656-677`) > script goal / pre-stage > mark fights / pre-aggro / survival > escort / fight /
formation > lane / pass > route / arrive / dock / via / navmarker > rest. Resumes from any state.
**The queue is the only memory:** A38 records, each re-proven at every assess (valid / achieved / failed / blocked).
From ObjectiveRun: derived each time (seq, stepKey, lastMarks, the step's nature, leash / lane / leader / gameDock);
`lastDock` -> an entry "Dock X" (`:662-668`); entry memory dying with the step (escortShip, deadGoals, badGoal,
goalTrigger, arrived*, retries, formRefused, via*, cruiseLock, tractorExitPending, leadStillSince, stepAt, seen-alive);
executor-only (sub*, action, lastStop*, guard counters); mission scope (the survival stance, `autopilot.cpp:232-238`).
The step key is written only when the new entry replaces the old one, after Decide: 4c4fb6f's class cannot recur.
**Achieve semantics:** each goal's `done` over the Situation; a head already done is popped before anything acts.
**Side effects check the game first:** an executor generalising StartOrKeep (`:276-281`) - no dock request docked / on
dock op to the same target / reply pending / GameDocksUs; no lane / jump while in one; no formation request when
already led; cruise as `autopilot.cpp:277-286`; each emits `side-effect <kind> target= step=` (the fuzz oracle).
**A40:** `DATA\RANDOMMISSIONS\vignetteparams.ini` `objective_text = <kind>, <ids>, <arg>` (HDE 0.7.1: 33 rows, 13
kinds; 327757 shared by 8 kinds = ambiguous -> verb fallback); story steps by NNObjective type + Cnd_.
**A38:** `GoalRecord` {id, version, type (closed enum), state, origin, parent / step, depends_on, subject, typed params,
when / done / fail in a small Cnd_-like condition language, policy, confirm, repeat, source_text, display (string ids),
stepKey, entry memory} (`autopilot-planning.md:94-99`); `Validate(rec, situation)` refuses unresolved references, the
knowledge scope, game locks, no route, a broken constraint, unknown fields / kinds.
**A41:** the objective-kind coverage table as data (`objective_kinds.h`; a test fails on an install kind with no row).
The install uses 40 distinct Cnd_ (the planning note says 42: take the full list, mark the unused ones):

| Game source (HDE uses) | Goal type | Today |
|---|---|---|
| cnd_distship 199 / distvec 148 / distcircle 41 / locenter 35 / distveclbl 2 / inzone 2; navmarker | Fly to | distship / distvec parsed; navmarker `:1530-1591` |
| cnd_baseenter 46 / spaceexit 10; rep_inst on a base | Dock / travel to base | `:1442-1495` |
| cnd_tlentered 35 / tlexited 33 / intradelane 3 | Trade lane | `:1334-1346, 1414-1418` |
| cnd_systementer 18 / spaceenter 70 / jumpincomplete 2 / systemexit 28 / npcsystementer 24 | Jump / system; wait | Jump `:526`; npcsystementer none |
| cnd_destroyed 292 | Attack / clear by labels | `:563-640`; marks `:1081` |
| cnd_projhit 66 / healthdec 48 / projhitshiptolbl 6 | Attack until hit / defend | partial |
| cnd_playermaneuver 9 | Fly to / dock / formation | `:1298-1367` |
| cnd_lootacquired 2; tractor_loot / return_loot | Loot (+ return) | partial; return none |
| cnd_cargoscanned 1 | Scan | none |
| cnd_commcomplete 607 / timer 380 / true 337 / watchtrigger 100 / rtcdone 2 / watchvibe 1 / enclaunched 1 | Wait | Rest / PreStage `:924-1034` |
| cnd_msnresponse 27 / charselect 5 / popupdialog 4 / launchcomplete 19 / playerlaunch 18 / hasmsn 2 / baseexit 6 / inspace 15 / locexit 6 | Base action, player-gated | harness only |
| cnd_distship outside -> FAIL, tetherbroke 5, Act_Nag* | Constraint | leash `:349-386` |
| jobs (mb / pb_kill_ships, *_kill_big_*, kill_other_solars, kill_big_solar, Destroy_loot, jettison_loot, guide_to_*, return_to_battle, *_dont_kill_ships) | Attack / clear / fly to / constraint | verb only |
| free roam (trade, repair, explore) | Trade / repair / fly to | none |

## 2. Slices (each ends with a test_budget row + retro, docs in the same commit, release build + win7_check)
- **S1 - assessor, A41 table, A40 parser, snapshot recorder (pure, no behaviour change, no gate).** Files:
  `situation.{h,cpp}`, `situation_io.h` (`snap v1`, key value lines, round-trip), `objective_kinds.h`,
  `situation_snap.cpp`; `core\game\jobs.{h,cpp}`; debug `ap situation [snap|hash]` (`autopilot_settings.cpp` beside `ap
  goals`, `:615`); `build.cmd:126-128`; tests situation / situation_io / objective_kinds / jobs + fixtures
  `tests\fixtures\situation\*.snap`; flctl `Save-FLSituation`, `ap situation` in FLReadRx. Fixtures from SaveS182
  (M12.hole, 21660 after a load), M07.tau31 (4c4fb6f), M03 SaveS010, M13 Save3989, SaveC701 docked, SaveS023 free roam,
  recorded with FLV_RECORD. Tests: round-trip; kind / placeholder / stand-in source / subject / held per fixture;
  idempotence, contact-order invariance; the step key from the snapshot alone (a 3-snapshot 4c4fb6f sequence gives a new
  key for "Dock with the Trade Lane"); coverage over the install (every Cnd_ and objective_text kind has a row; unknown
  fails); jobs 327734 -> guide_to_main_battle, 327757 -> ambiguous. Done-check: (a) `ap situation hash` in game =
  `Assess(parse(snap)).hash` for the 6 fixtures; (b) coverage green with its counts; (c) tests + release + win7 (no new
  imports). Budget 1 background boot, ~15 min. Probes in that boot: A40's textId = objective_text id? (a job via
  padbases, or the local server's board); LiveTriggers' cost and agreement on S182 / M13 / M07; space ids right after a
  load vs 10 s later.
- **S2 - A39 as the story source (pure).** `mission::GoalsOfLive(LiveTable, system)`, `ArmedLeashes`, the stand-in
  from the live table; the adapter tags live / ini; `ap situation` prints the difference. Test
  `mission_live_goals_test.cpp` (armed / inactive / complete, a leash armed only at state 0, fallback, S182 -> Toledo
  `source=live`). Done-check: on S182 after the load the live table names the Toledo goal. 1 boot.
- **S3-S5 - the arbiter's rules ported in three parts (pure + a shadow).** `situation_rules.cpp`; in ObjectiveDecide
  one `ShadowCompare` per exit -> `situation shadow agree|differ <rule> <fields>`; setting `situation_shadow` (default
  0). S3 `:336-923`, S4 `:924-1310`, S5 `:1311-1597`. Per-rule tests from the fixtures + the old decision recorded with
  each. S5 ends with one gate `-ApSet 'situation_shadow 1'` (measurement): every differ classed porting bug / intended
  fix (the hidden statics).
- **S6 - queue, A38 schema, validator, re-proof, achieve (pure).** `goalqueue.{h,cpp}`; tests incl. cold = warm
  host-free (`Decide(Assess(s), warm) == Decide(Assess(s), Rebuild(s))`, declared-memory entries excepted).
- **S7 - the executor and the switch (behaviour, behind `situation_arbiter`, default 0).** Done-check: flgate with the
  switch (no FAIL / SLOWER); SaveS182 M12.hole 3 runs (the first decision toward Toledo, PASS); M07.tau31 3 runs; sweep
  fight-matrix; then default 1 + CHANGELOG + sprt on differing segments. Budget 1 gate, 1 fight-matrix, 6 runs.
  **Needs the owner's D10 first.**
- **S8 - toggle fuzz (A42).** `controller\tools\flfuzz.ps1` over flleg with a seeded toggler (logs its seed), debug `ap
  toggle <frames>` (test only), `situation_record` -> `FLSituation.tsv`; edges lane entry / mid-jump / dock queue /
  Held / +-0.5 s of an objective event / after a load / mid-fight; off periods 0 / 1 frame / 0.5 / 5 / 30 s /
  perturbed; oracles PASS, time <= best x 1.15 + 10 s per toggle, no duplicate side-effect, `situation coldwarm differ`
  0, decide-loop guard 0; shrinking by halving (confirmed over 3 runs: no common random numbers, T3). M03 / M07 / M12 x
  5 seeds (~1 h on 3 instances).
- **S9 - goal panel QUEUE / DO NEXT** (`goals.cpp:304-363`, `:399-430`; spec + ui_meta + the ui / spec artifact in one
  commit; CHANGELOG; `docs\UI_BUILD.md:149-150`). Done-check: flcrawl -Space over the panel, K&M = pad parity.
- **S10 - retire the old path** after one green gate cycle at 1 (the old ObjectiveDecide body, situation_shadow, the
  superseded mission.cpp statics; the ini fallback stays for multiplayer). 1 gate.

## Results (per slice; the full record: `A37_RESULTS.md`, same headings)
- S1-S6 (30 Oct 2026): built behaviour-neutral (`tools\decision_check.ps1` "decisions unchanged (95 lines)" at each); S5's shadow gate s5shadow: 487 shadow decisions, 487 agree, 0 differ, 23 rules over 14 legs.
- S7 (30 Oct 2026): the executor behind `situation_arbiter` (default 0); parity on gate s7arb3, 481 agree / 0 differ (WORK_QUEUE Q6); the default stays 0 until a slice gives the arbiter decisions of its own.
- S8-S10: open.

## 3. Risks
A40's ids may be formatted entry ids (`goals.cpp:178-186`) -> a text-id map, A40 waits; LiveTriggers' cost; space ids
after a load; shared code at every activation (shadow first, switch default 0, gate before keeping it, fuzz after);
fixtures going stale (an install-tier test re-derives); fuzz verdicts statistical; multiplayer (the live table fails
closed: label the ini fallback); the partial tick's frame cost.

## 4. The owner
D10 DECIDED 30 Oct 2026 (the owner: "your suggestion goes"; `..\..\docs\WORK_QUEUE.md`): the player's stop PAUSES
with the queue kept (`AUTOPILOT_UX.md:51`, not the end of today's `autopilot.cpp:251-253`); at the next start every entry
is re-proven - achieved ones popped, mission entries rebuilt from the live story state, the player's own goals kept
while valid and re-routed from where the ship is; it resumes silently when the head is still valid and the pause was
short with nothing unrelated done meanwhile, else the goal panel shows the queue with RESUME / CLEAR (the threshold - a
time, a system change, a dock or a job taken - is S7's to define and test; S9 draws it). An optional look at the QUEUE tab (S9).
