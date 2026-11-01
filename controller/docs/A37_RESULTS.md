# A37 slice results (moved from A37_PLAN.md 31 Oct 2026, audit C4)

The build and test record of each slice of `A37_PLAN.md` (the plan: design, slices, risks), headings unchanged - a reference to `A37_PLAN.md "S<n> results"` lands here.

## S1 results (30 Oct 2026)
**Built** (behaviour-neutral: no decision path reads it; `ObjectiveDecide` / `ObjectiveSupervise` unchanged - diff
review: the only touch is a read-only `MenuSpawn` getter beside `NoteMenu`; `mission::AdvanceGoals` keeps its exact path,
the snapshot reads through `AdvanceGoalsPeek`, which writes none of mission.cpp's memory): `situation.{h,cpp}`,
`situation_io.h`, `objective_kinds.h`, `situation_snap.cpp`, `core\game\jobs.{h,cpp}`, `ap situation [snap|hash]`,
`build.cmd` test line, tests `situation` / `situation_io` / `objective_kinds` / `jobs` + `tests\fixtures\situation\*.snap`,
flctl `Save-FLSituation`, `ap situation` in FLReadRx, `fltest.exe -snaps <dir>`.
**Fixtures** (boot 2, `research\experience\runs\a37s1_2026-10-30_0838_fresh`, a fresh game per save on instance 2): all six
named saves existed and loaded (SaveS208's catalogue "unfit" is its cruise speed for a leg start - irrelevant to a
snapshot, kept): `m07_tau31_s208` (trade_lane / script / ref), `m12_hole_s182` (awaiting, stand-in title, attack by the
mark fight, subject none), `m13_start_3989` (docked; fly_to from system-entry goals - an S1 limitation, the live table arms
exit_base Cnd_BaseExit), `m03_willard_s010` (fly_to / verb / ref), `m03_end_free_s023` and `free_docked_c701`
(no_objective; docked).
**Done-check.** (a) in game `ap situation hash`: roundtrip=ok (live hash = re-parsed hash, text identical) on all 6 saves in
boot 1 and boot 2 and on the 4 "10 s later" snapshots; host-free `Assess(Parse(snap)).hash` = the `#game-hash` for the 6
fixtures + the 4 later snapshots (`fltest.exe -snaps <run>\later`: 10 of 10). (b) coverage green: the install's [Trigger]s
use 40 Cnd_ (2,655 lines), the table has 44 rows (the live table's 43 vtable names + Cnd_TetherBroke; unused in HDE:
cmptoplane, jumpgateact, npcsystemexit, rumorheard), 0 missing, every count equal; 16 job kinds in 33 objective_text rows,
0 missing. Corrections to this plan: 16 kinds (not 13), 327757 is shared by 6 kinds / 6 rows (not 8) - and 327745 is
ambiguous too (pb_kill_ships / mb_kill_ships). (c) unit tests 35 tests / 21,775 checks / 0 failures (the runner's own count,
incl. -snaps), release build exit 0, `win7_check` OK (326 imports, all on the allowlist).
**Probes.**
1. A40 textId = objective_text id: **yes for the job's own step, no for its route.** Boots 1-2 reached no job (boot 1 loaded
   SaveC701 sixth in one game: no offer event; boot 2: the deploy's load created Manhattan's 8 offers, all `offer-gone`
   0.94 s later as the driver loaded the same save again). Loaded once (runs `..._final4e1c8e7`, `..._job4e1c8e7`): 9 / 8
   offers at once; a Kill Ships job (2,150 cr) accepted -> docked: one entry 13081 "Launch into Space"; launched -> 6
   entries: 13067 "Dock with the marked Trade Lane to reach the enemy ships." (step), four route entries text 1 " " (lane
   rings, navmarkers) and the battle entry **327739 "Kill all %F0v2 ships near the mission waypoint." = vignetteparams.ini
   node 423's objective_text** (guide_to_main_battle and guide_to_pre_battle: 2 rows -> ambiguous, both fly_to). So the
   job kinds name the battle step; the route steps are the game's generic 13xxx / " " texts (their kind: the words + the
   subject, as `job_killships_c701.snap` shows: trade_lane by the verb). Follow-up (not S1): an id whose kinds share one
   goal type could give that type instead of "ambiguous".
2. LiveTriggers: **cost** 1.88-1.97 ms for the first read in a game (the signatures resolved once per Content.dll load; 6
   fresh games), 0.05 ms after (boot 1). **Agreement** (the live table = the save's mission in all 4 story saves): the ini
   derivation's goals were live in 0 of 3 - M07: its one goal (by_entry tobias_at_active_tl_to_tau23_gate) is not in the
   live list, which arms tl_enter_to_holman (Cnd_TLEntered); M13 docked: 2 by_entry distship goals not live, live arms
   exit_base (Cnd_BaseExit), spawnships, wait_in_equipment_room; M03 S010: no ini goals, live arms 10 incl.
   player_at_lab_before_walker / dst_player_at_battle (Cnd_DistShip); S182: by title, no goals, live arms 10. HealthDec,
   TLEntered and BaseExit rows read no Cnd_ name (not in the verified vtable map): S2 falls back to the ini's key there.
3. Space ids right after a load vs 10 game seconds later: **identical** - M07 49/49, M12 32/32 (boot 1: 32 -> 31, a Nomad
   killed), M03 S010 48/48, S023 48/48. **But the host's objective title and mission marks survive a load** in one process
   (boot 1): S023 (free roam, no objective) still listed S010's marks 126 / 120 and M13's title 24400 (a fresh game: no
   marks, title 0); S182 after S208 read title 23200 "MISSION 7", so the by-title stand-in named M07's script. A
   multi-load process misleads the objective mode the same way (MissionObjectIds, the title stand-in): S2 must drop what is
   older than the load (a host change), and fixtures need a fresh game each. **Fixed 30 Oct 2026** (the "load reset"
   commit; `..\..\docs\FREELANCER_RE.md` "The client's copy of the objectives and save loads"): the host forgets title,
   marks and comms at Content.dll's load notification and hooks it there; HostApi v34 GameLoads -> `mission::ForgetGame`.
   In one game S208 -> S182 now reads title 24200 + the by-title stand-in M12 (the fresh-game fixture's), S010 -> S023 no
   title, no marks, no script (runs `loadreset_2026-10-30_0910` and the commit's re-check).
**Cost**: a snapshot 0.04-0.95 ms (the first of a game 40-43 ms: one-time reads), 0.9-18 KB of text.
**On the commit's build** (4e1c8e7 src:clean, run `research\experience\runs\a37s1_2026-10-30_0847_final4e1c8e7`): (a)
again roundtrip=ok on the 6 saves + the docked job, host-free = game 7 / 7 + the 4 later snapshots 4 / 4; the job
snapshot in space became a 7th fixture (`job_killships_c701.snap`, the A40 path). The re-run on the docs commit's build
is cited in the WORK_QUEUE Q6 note.
**Retro.** Boot 1 found three assessor errors the synthetic tests did not (a placeholder's 15 [NNObjective]s counted as
"known"; the mark fight decided after the stand-in, where ObjectiveDecide decides it first; TowardsNagOf's first-script
nag for a shared text) and the stale title / marks; the fixes and a fresh game per fixture cost boot 2. Harness bugs: 2
(the driver's event log before the first start, a second load before the job probe). Lesson -> the fixture recorder starts
a fresh game per save (HARNESS.md); a probe that needs a docked board loads once.

## S2 results (30 Oct 2026)
**Built** (behaviour-neutral: `ObjectiveDecide` / `ObjectiveSupervise` / the autopilot's `mission::AdvanceGoals` and
`Leashes` untouched - mission.cpp's GoalsOf / Leashes bodies were factored into `GoalOfTrigger` / `LeashesOf` with the
same rules, `tools\decision_baseline.txt` unchanged, decision_forget_game green): `mission::GoalsOfLive(LiveTable, system,
textId)` (each ARMED row - state 0 - with one condition AdvanceGoals takes, parsed from its [Trigger] line by the same
rules; the step's own objective excluded; a trigger's system or its arming trigger's), `ArmedLeashes(LiveTable, system)`
(a leash while its trigger is armed), `LeadsOfLive(LiveTable, textId)` (each row's lead + hop); no memory (no last script,
no shown objectives). The adapter takes them when the table is ok with a script, else the ini view (the reason printed);
`snap v2` keeps both views (`source_live`, `inigoal` / `inileash`, `livetrig leads= hops=`; v1 files still parse and keep
their hashes: the S1 job fixture). Assess: goals `source=live`, leashes `armed=1`, a placeholder's stand-in `live`, with no
flying / destroy goal armed the kind by the armed row that sets the next objective (own action first, then fewest hops:
its Cnd_ through the A41 table), `story` + `diff` lines (live vs ini by script + trigger / object + distance), and
`ap situation` ends with a `timing` line (not hashed; logged `situation:` in FLController.log).
**Done-check** (instance 2, background, muted, a fresh game per save; boot 1 `research\experience\runs\
a37s2_2026-10-30_0935` on the pre-commit build `c7bfc1a+dirty src:b32152d0`):
1. SaveS182 after the load: `goal source=live trigger=range_for_sendcomm_dx_m12_0500_vonclaussen kind=1 cnd=cnd_distship
   leads=mlog_land_on_the_base live=0 dist=16095` (within 8 km of Toledo -> dock_with_toledo_trigger -> land on the base),
   `standin=live`, `subject=goal`; the kind stays the mark fight (it comes first, as in ObjectiveDecide); `story
   source=live both=0 only_live=1 only_ini=0` (the ini view by title had no goal).
2. Live vs ini: M13 Save3989 docked: no live goal, kind `base_action` (`ids+live:cnd_baseexit`: exit_base sets defend_base
   at once), `diff goal only=ini` wait_for_trent_again + in_osiris_range (the ini view's two system-entry distance goals
   are not armed); M07 SaveS208: kind `trade_lane` unchanged (the ref is Holman's ring), `diff goal only=ini
   tobias_at_active_tl_to_tau23_gate` (not armed; the game arms tl_enter_to_holman, Cnd_TLEntered). SaveS010: neither view
   gives a goal (dst_player_at_battle sets `mlog_awaiting_objective, OBJECTIVE_HISTORY` and arms comms only;
   player_at_lab_before_walker only watches triggers) - `both=0`. SaveS023: `story source=ini reason=no story mission
   (story id 0 ...)` (the fallback).
3. LiveTriggers' timing: 0.036-0.089 ms per read once the scripts are loaded (`timing live_ms=` on 5 games), 1.74 ms the
   first read in a free-roam game; boot 1's first in-game read measured 37-39 ms because it now ran before the ini view
   and so paid mission.cpp's one-time script read (~36 ms; S1 measured 1.9 ms after it) - the adapter reads the ini view
   first since (the timing measures the table again).
4. In-game round trip `roundtrip=ok` on all 5 games; host-free hash = the game's for the 5 new snapshots (`fltest.exe -snaps
   <run>`: 12 of 12 with the 7 fixtures). One driver bug (SaveC701 is the instance's own save, not under research\saves:
   the sixth game did not start) and one wrong expectation of mine (S010 `source=script`: the rule says verb).
**On the commit's build** (63291e3 src:clean, run `research\experience\runs\a37s2_2026-10-30_0940_commit63291e3`, all 6
saves, a fresh game each): the same answers - S182 `goal source=live trigger=range_for_sendcomm_dx_m12_0500_vonclaussen
... leads=mlog_land_on_the_base live=0`, `story source=live only_live=1`; M13 `base_action` / `ids+live:cnd_baseexit`,
`only_ini=2`; M07 `only_ini=1` (tobias_at_active_tl_to_tau23_gate); S010 `both=0`; S023 / C701 `story source=ini` (no
story mission); LiveTriggers' first read in each game 1.59-1.70 ms (the scripts loaded first), 0.020-0.052 ms after
(`timing live_ms=`, FLController.log `situation:`); `roundtrip=ok` 6 / 6; host-free = game hash 13 / 13 (`fltest.exe
-snaps`, with the 7 fixtures). These six snapshots are the fixtures now (`snap v2`; the job fixture stays v1: the v1
path). Unit tests 37 / 0 failures (21,837 checks), release build + `win7_check` OK (3 binaries, 532 imports).
**Retro.** The plan's done-check held on the first boot; the cost was the driver (a save path, Save-FLShot without flsub.ps1)
and one prediction I wrote before reading the S010 triggers. Lesson: derive a fixture's `-Expect` host-free first (the unit
test over the install's scripts already had the answer) - the driver's expectations now come from that run.

## S3 results (30 Oct 2026)
**Built** (behaviour-neutral: every decision is still ObjectiveDecide's - its covered exits call `ObjectiveStart` with the
same arguments in the same order as before, then a shadow call that tests the switch; `tools\decision_check.ps1`:
"decisions unchanged (95 lines)"): `situation_rules.{h,cpp}` - pure `Rule_*(const Situation&, const Queue&) ->
std::optional<Proposal>` (mode / target / action by their ModeName / ActionName strings, `keep` = no new start), one per
branch of ObjectiveDecide before the pre-stage (re-located by content; at 58f9c70 `:353-386` leash, `:430-441` marked loot,
`:469-510` tractor exit, `:525-526` jump, `:531-551` airlock, `:563-640` script-destroy, `:656-677` the dock entry,
`:683-924` the script goal and the fired goal's armed trip) + `Rule_Held` (ObjectiveSupervise's holds `:1685-1740`:
mission holds, the game docks us, lane, jump); `DecideS3` = the first that holds in that order, else `none`. A `Ctx`
computes what ObjectiveDecide derives before its branches the same way (MissionObjectIds = marks + resolved refs +
script marks, the mark fight, the verb, the ref / mark-fallback target). The rules read the **ini view** (AdvanceGoals /
Leashes, `goalsLive ? iniGoals : goals`): today's path; the live view stays the assessor's. Setting `situation_shadow`
(Bool, default 0; SETTINGS.md); `ap situation last`.
**What the snapshot needed (`snap v3`):** the objective's refs resolved (`refs_obj`, MissionObjectIds' second half), the
goals each ini goal arms (`armed`: GoalsArmedBy - the fly-through and the fired-condition trips), the destroy goals'
object positions (beyond the contact list: M13's generators), per leash the comm that disarms it (`off`), the ship its
pilot's name names (`ship`) and the object's position, the system entry (the airlock rule's "not the way we came in"),
the tractor's reach and tractor_exit_*; the contacts add every jump gate / hole / airlock, the friends hostile ships
attack within 15 km and the memory's ships. The executor memory is the `queue` record (`Queue`: mode / target, the
objective run's sub / subTarget / action, the travel's destination and airlock lock, cruise lock, leash ship / distance,
last dock, escort ship, bad / dead goals, script marks, and DestroyFired's two statics s_seenAlive / s_seenListed - the
"hidden statics" S5 classes). `decided` = the old decision (from g_obj; `ap situation last`: the shadowed exit's branch).
v1 / v2 texts parse and hash as before (Describe unchanged).
**The shadow's timing:** the memory is taken as ObjectiveDecide begins (after its step-memory upkeep: the seq reset, a
dead goal noted, scriptMarkN = 0), the snapshot at the exit after the old start - so mission.cpp's memory (lastScript /
lastText / the shown objectives, which AdvanceGoals updates during the decision) is what the decision itself saw, and the
start's own effects (mode, target, the travel) come from the memory, not the snapshot. GoalsArmedBy in the snapshot adds
an empty `g_shown` entry per script it reads (as the old path's own calls do; no answer changes).
**Not ported faithfully (and why):** nothing of the S3 range is left out; three reads are approximations, all reported by
the shadow if they matter: (1) the tractor rule's distance to the door uses the pilot's position (the old path reads
PlayerKinematics' - the same point in every read seen, not proven); (2) the snapshot's contacts are the 48 nearest +
every one the rules name: a contact the old path finds beyond those (e.g. a 49th friendly ship within 6 km for the leash's
survival-lead anchor) is missing; (3) the leash anchors' comm test uses wall time read in the adapter (`off`), as the old
path. ObjectiveSupervise's own leash trip (`:1743-1754`, a supervisor exit, not ObjectiveDecide's) has no shadow.
**Tests:** `situation_rules` (95 checks): each rule both ways over synthetic situations (holds / one condition fails),
the order (held > leash > ... > script goal), the ini view vs the live view, the leash anchors (named ship, solar outside,
disarmed), DestroyFired over the memory, the fly-through and fired-armed trips, the mission's success first; the seven
recorded fixtures (v1 / v2, autopilot off) all decide `none` - each is a later slice's step (M07's lane ride, M03's
Willard target: S5; M12's mark fight: S4; docked / no objective) -; a v3 fixture with `decided rule=` must get exactly
that decision. `situation_io`: the v3 records round-trip, v3 under a v2 / v1 header fails, queue / decided missing fails.
Unit tests 38 / 0 failures (21,947 checks: fltest.exe's own count). The in-game check and the decided fixtures: below.
**In game, after the commit** (c79fc35 src:clean, instance 2, background, muted, a fresh game per save, `ap preset
speedrun` + `ap set situation_shadow 1` + `ap start objective`; run `research\experience\runs\a37s3_2026-10-30_1015`,
driver.log + `<save>.events.txt` + 6 shots): **4 shadow events, 4 agree, 0 differ.** SaveS207 (M07.gate, 74 s): the
first decision `situation shadow agree jump old=travel,2330007305,jump,keep=0 new=jump mode=travel target=2330007305
action=jump keep=0 text=23245` (the Tau-31 jump gate; 40.4 ms - the game's first snapshot pays its one-time reads), then
`agree held:jump` (hold / rest kept, 1.4 ms) after the jump and `agree held:mission_holds` at the lane ring (dock / lane
kept, 1.2 ms); SaveS208 (M07.tau31, 132 s): `agree held:mission_holds` at the lane entry; its other decisions were S4 /
S5 branches (the lane, the pre-stage toward the mark): no shadow there, as designed. The shot at 60 s shows the ship
entering the Tau-31 lane (the mission-holds verdict). The jump's `ap situation last` became the fixture
`tests\fixtures\situation\m07_gate_jump_s207.snap` (`snap v3`, `decided rule=jump`): host-free hash = the game's, the
rules give exactly the recorded decision (`situation_rules`: "8 recorded snapshots decided; 1 carry the old path's
decision, 1 of them agree"). Unit tests 38 / 0 failures (21,956 checks); release build (c79fc35 src:clean) +
`win7_check` OK (3 binaries, 532 imports).
**Not verified (exact):** of the 9 rules, 2 met in game (Jump: 1 exit; Held: 3 holds - mission_holds 2, jump 1; lane and
game_dock 0 - the lane rides read as mission holds first); Leash, MarkedLoot, TractorExit (2 exits), Airlock,
ScriptDestroy (2 exits), DockEntry and ScriptGoal (3 exits) 0 in game - synthetic tests only; 1 of the 14 covered
ObjectiveDecide exits seen (leash 2, marked loot 2, tractor exit 2, jump 1, airlock 1, script-destroy 2, dock entry 1,
script goal 3). S5's gate (every leg with the switch on) is where the rest is measured.
**Retro.** One failed build (a host helper qualified with the autopilot namespace) and one failed test (v3 `armed`
records written into a v1 text) before the commit - both caught by the cheap steps; the synthetic expectations I first
wrote for the script goal were wrong twice (a real step only takes a goal another ship drives; "land" in the next step
counts as the mission's success) - read the branch's filters before writing a case. The in-game run: 1 run, 2 games, 0
harness bugs; its stop rule counted held verdicts as branches, so game 1 stopped at 74 s before the Tau-31 script-goal
exit the q3s2 gate's sequence promised -> the next shadow driver stops on decide-exit branches only (S4 / S5 driver note).

## S4 results (30 Oct 2026)
**Built** (behaviour-neutral: ObjectiveDecide's S4 exits call `ObjectiveStart` with the same arguments as before - now
through `ShadowExit`, which then tests the switch -; the one other touch: the function static `lastReal` became the file
static `s_lastReal` behind a reference of the same name, read by `ObjectiveLastReal()`; `tools\decision_check.ps1`:
"decisions unchanged (95 lines)"): nine pure rules for ObjectiveDecide from the pre-stage to a ship target (re-located by
content after S3; at c79fc35 + this change `:954-1338`): `Rule_PreStage` (the fired condition's next step: its distance
goal, else its destroy condition's nearest placed object - trip / hold within 700 m), `Rule_PreStageNext` (prestage_next:
NextStepGoals of the last real objective, else the declared next place beyond 1 km; never across a leash),
`Rule_MarkFight` (a fight step's nearest live non-friendly mark: big_coast's huge static -> its fight, beyond 5 km -> a
trip), `Rule_PreAggro`, `Rule_SurvivalStage`, `Rule_Survival`, `Rule_ThreatFight` (the escort's ship / the mark fight with
the escort / survival's own hostile target / the fight), `Rule_Escort` (formation, or the running follow, with the memory's
ship, the named mark, the nearest friendly mark), `Rule_ShipTarget`; `Decide` = S3 + S4 in ObjectiveDecide's order (the
shadow's decider; `DecideS3` stays the S3 subset). The script block (S3's `Rule_ScriptGoal`) is one shared `ScriptBlock`
now - its goals, anchors, best trip and first fired condition feed the script goal, both pre-stages and the pre-aggro's
leash.
**ObjectiveDecide's writes on the way, derived per rule from the memory as the decision began (no rule depends on another
having run):** `leashShip` / `leashDist` (the leash's clears by a dock step / jump ref / navmarker at a gate, then the
script block's anchors: disarmed -> 0, a ship anchor we are inside -> it), `lastReal` (the step's text unless 21660 / 1),
the survival stance (`g_survivalTitle / Mission / Ended / System` + `g_obj.survival`: the title's id 22602 or escape words,
the first system, survival_until_jump, a Survive step), `escortShip` (unchanged inside the escort branch: a ship target
skips it). S3's rules now read the stance as the decision began (`queue survival=`, v4; a v3 snapshot's own) - the snapshot
is taken at the exit, after S4's update.
**`snap v4`** (the format changed: v1-v3 texts parse, write back and hash as before - Describe unchanged): `after` (the
goals of the step each ini goal leads to - GoalsAfter, once per leads + script), `prestage` + `realnext` (NextStepGoals /
NextObjectivePlace from lastReal after the update; the survival stage's place with two objectives skipped), `obj ...
title_escape=`, `pilot ... system=`, `settings ... big_coast= survival_*=`, `contact ... radius= named=` (pointed contacts:
a non-ship's obstacle radius, a ship's speaker-name word in the step's text), `queue ... last_real= arrived= step_age=
escorted= surv_*=`. Pre-stage-next and the survival stage fail closed when the snapshot's next-step facts were read from
another text than the rule's lastReal.
**Not ported faithfully (and why), beyond S3's three approximations:** (4) the lane-escape test reads the snapshot's
`lane` records, written for trade-lane-ring contacts only (the old path asks `space::LaneLinks` of any target; a
non-ring has no links); (5) PreHostileNear / the escort's ties: the old path takes the first of the game's contact list
(nearest first), the rules the nearest of the snapshot's (sorted by distance, then id) - equal unless two contacts tie;
(6) the step age (`longStep`) is taken as the decision begins (the same frame: the game clock does not move within it).
**Tests** (`situation_rules`, 176 checks - fltest's own count): each S4 rule both ways over synthetic situations (holds / one
condition fails) and the order (mark-fight before the threat fight, pre-aggro before an attacker's fight, the survival
stage before the attacked stance, the escort's marks vs the mark fallback's ship target); the survival memory (the title,
its escape words, a Survive step, survival_until_jump, an ended stance), the leash's 75 % limit and its clearing;
M12's S182 fixture (v2, autopilot off) now decides `threat-fight combat 0 fight` (the mark fight; DecideS3 still `none`);
`situation_io` 104 (v4 records, v4 under v3 / v2 / v1 headers fails, a v3 rewrite round-trips). Unit tests 38 / 0
failures (22,050 checks).
**In-game budget (up front):** 1 background run on instance 2 after the commit (clean build), 4-5 fresh games, <= 12 min
of game; the driver stops a game on S4 decide-exit verdicts only (S3's retro). Saves (early decisions in S4 branches):
SaveS182 M12.hole (21660 with the Nomads marked: the mark fight -> threat-fight; the S2 fixture decides it), SaveS208
M07.tau31 (the S3 run's status after the lane ride: objective `pre-stage`), SaveS008 M03 step 9 (22455 "Defend the
Transport Convoy": an escort verb under a pirate attack -> threat-fight escort / escort / ship-target), SaveS350
M13.battleship (24425 "Destroy the Nomad Battleship": a fight step -> mark-fight / threat-fight), SaveS009 M03 step 13
(22520 "Destroy the Rheinland Valkyries": a fight step). Not reachable from a space save today: the survival rules (M04
starts docked at Manhattan) - synthetic only unless a game meets them.
**In game, after the commit** (16a4b63 src:clean, instance 2, background, muted, a fresh game per save, `ap preset
speedrun` + `ap set situation_shadow 1` + `ap start objective`; run `research\experience\runs\a37s4_2026-10-30_1052`:
driver.log, `<save>.events.txt`, one `ap situation last` per branch and game, 15 shots; 5 games, 6.5 min): **10 shadow
events, 10 agree, 0 differ - 9 at S4 exits** (threat-fight 5, escort 2, mark-fight 1, pre-stage-next 1) + 1 hold
(held:mission_holds, S208's lane entry). Per game: S182 M12.hole - `threat-fight combat 0 fight` x2 (the mark fight, the
first decision and the one after the first kill; 2.95 / 0.42 ms); S208 M07.tau31 - after the lane ride
`pre-stage-next travel 0 pre-stage` ("23250 -> mlog_take_the_tradelane_to_the_tau29_gate: its juni_cautious, within 4500
m, 7032 m to go"; the S3 run's `objective=pre-stage` status was this branch, not the fired-condition pre-stage: both
start action PreStage); S008 M03 step 9 - `threat-fight escort 15 escort` (22455 "Defend the Transport Convoy": the
escort's ship under the pirates' attack), then `escort formation 15` (22510) and `escort formation 14` (22465), both the
named ship of the step's text; S350 M13.battleship - `mark-fight combat 2212950529 fight` (big_coast: the battleship's
obstacle radius 666 m) then `threat-fight combat 0 fight` (the step's hostile target); S009 M03 step 13 - `threat-fight
combat 0 fight` (22520, the Valkyries; the game then ran to its 100 s limit on S5 branches - the dock). The seven S4
`ap situation last` snapshots are fixtures (`tests\fixtures\situation\m12_hole_threatfight_s182`,
`m07_tau31_prestagenext_s208`, `m03_convoy_threatfight_s008`, `m03_convoy_escort_s008`, `m03_valkyries_threatfight_s009`,
`m13_battleship_markfight_s350`, `m13_battleship_threatfight_s350`; `snap v4`, `decided rule=`): host-free hash = the
game's 15 / 15 (`fltest.exe -snaps <run>` and the fixture folder), the rules give exactly the recorded decision 8 / 8 (with
S3's jump). Shadow cost: 0.4-4.4 ms a decision (40 ms the first of a game). Unit tests 38 / 0 failures (22,094 checks);
release build (c2b0469 src:clean - 16a4b63's sources + a handoff commit) + `win7_check` OK (3 binaries, 532 imports);
decision_check "decisions unchanged (95 lines)". **No differ: nothing to classify** (no porting bug, no hidden-static
difference met).
**Not verified (exact):** of the 9 S4 rules, 4 met in game (threat-fight, escort, mark-fight, pre-stage-next); 5 not
(pre-stage, pre-aggro, survival-stage, survival, ship-target) - synthetic tests only. Of the 21 S4 exits, 5 met
(threat-fight's escort-ship and plain-combat exits, escort's formation, mark-fight's huge static, pre-stage-next's
next-step goal); 16 not (pre-stage 3, pre-stage-next's declared place, mark-fight's far trip, pre-aggro, survival-stage 3,
survival 2, threat-fight's mark-fight escort and survival-target exits, escort's running follow, ship-target 2). The
survival rules need an M04 space save (none in `docs\missions\scene_saves.tsv`: M04's legs start docked); S5's gate
measures the rest.
**Retro.** Budget 1 run / <= 12 min, used 1 run (5 games, 6.5 min), 0 harness bugs, 0 wasted runs; 0 failed builds, 0
failed tests before the commit (S3's retro held: the branch filters read before each case). One wrong prediction: S208's
early S4 decision was pre-stage-next, not the pre-stage (I read the S3 run's `objective=pre-stage` status, an action
two branches share) -> name the expected branch from the events' text (`autopilot objective pre-stage-next ...`), not the
action. Coverage gap -> an M04 scene save in space (after the launch, "Defend Yourself!") would put the survival rules in
reach of one background game (H-lane item).

## S5 results (30 Oct 2026)
**Built** (behaviour-neutral: ObjectiveDecide's remaining exits call `ObjectiveStart` with the same arguments as before -
through `ShadowExit`, which then tests the switch; the survival lead's `stayWithLead` takes the branch's name; no other
touch; `tools\decision_check.ps1`: "decisions unchanged (95 lines)"): twelve pure rules for ObjectiveDecide after the ship
target to its end (at 6833b42 `:1342-1627`), in its order: `Rule_Lane` (a Dock step at a lane ring: closed both ways ->
`lane-wait` - a trip beyond 1.5 km unless arrived, else a hold -; else the ride - a trip beyond 3 km unless arrived, else
the dock toward the open neighbour), `Rule_MissionForm` (a trip objective with the mission's named leader alive and
friendly), `Rule_Pass` (PassRingRadius: a satellite-class solar, no lane ring / dock / links, 30-300 m, not hostile; the
running pass kept), `Rule_RouteLane` (a route whose target is a lane ring: toward the neighbour nearer the route's step 1),
`Rule_Arrived` (rest - beside the survival lead within follow_distance, else with it), `Rule_Route` (to the last entry's
object / navmarker), `Rule_Waypoints` (a target on / within 1.5 km of a non-last waypoint), `Rule_Arrive` (a planet's
docking ring; dock for a Dock step or an airlock target), `Rule_NavWay` (the nearest airlock / gate / hole within 200 m of
the marker, unless hostile), `Rule_Via`, `Rule_Navmarker`, `Rule_Rest` (the arrived rest's lead rule, else a hold); `Decide`
= S3 + S4 + S5 (it never answers "none" now: the rest always holds). **Every ObjectiveDecide exit is shadowed: S5 adds 21
exits** (19 `ShadowExit` call sites - the survival lead's two serve the arrived and the final rest: lane 4, mission-form 1,
pass 2, route-lane 2, arrived 3, route 1, waypoints 1, arrive 1, nav-way 1, via 1, navmarker 1, rest 3); ObjectiveSupervise's
own starts (dock-wait, the formation / follow upkeep, lead-stall, the survival upkeep) are not ObjectiveDecide's.
**`snap v5`** (the format changed: v1-v4 texts parse, write back and hash as before - Describe unchanged): `step` / `entry
... ref_placed= ref_pos=` (ObjectPos of the first resolved ref - the route's step 1), `contact ... dock_ring=`
(PlanetDockRing of a pointed or entry-referenced solar; `radius=` now read for the entries' refs too - the pass ring),
`lane ... next_placed= next_pos= prev_placed= prev_pos=` (the neighbours' positions), `settings ... follow_distance=`,
`queue ... mission_leader= via_next= arrive_retries= tr_pass=`; the contacts add the memory's mission leader.
**The hidden statics, classed:** DestroyFired's `s_seenAlive` / `s_seenListed` - read only by the S3 / S4 script block,
from the memory as the decision began (`queue seen_*`, S3); no S5 exit reads them. The PreAggro statics
(`PreAggroUpdate`'s `next` / `lastText` / `lastSystem` / `lastSpawn` / `lastRange` / `lastIds`) decide only when the
pre-hostile overlay is re-read; the overlay itself is each contact's `pre_hostile`, a fact of the same frame (the snapshot
at the exit) - nothing to derive. mission.cpp's memory (`g_shown`, `lastScript`, `lastText` behind fromLast): S5 reads it
only in the navmarker's fly-through (AdvanceGoals), which shapes the trip (flyDir / flyGoal), not the decision's mode /
target / action - not ported (S7's executor carries the trip). ObjectiveDecide's writes on the way (`laneRing`,
`missionLeader = 0` for a dead leader) are read by no later branch of the same decision: no rule needs them.
**Not ported faithfully (and why), beyond S3's and S4's six:** (7) the pass ring's radius is the snapshot's `radius` (the
largest obstacle with the object's id within 50 m; the old path takes the first) and its lane-links exclusion reads the
`lane` records, written for trade-lane-ring contacts only (as (4)) - equal unless an object has two obstacles or a non-ring
has lane links; (8) a shadowed decision's waypoint list is taken as the decision began (ShadowEntry), the rest of the
snapshot at the exit - the old path reads the list at its branch with no start before it, so the same list, but a start's
stop of a plotted travel restores the player's list before the exit (StopImpl -> RestoreWaypoints): read at the exit it
would differ; (9) v4 and older texts carry no dock ring, ref / neighbour positions or S5 memory: their S5 decisions are the
rules' over defaults (a planet target arrives at the planet, not its ring; no route-lane ride) - the recorded fixtures
decide, only v5 ones are compared; (10) the fly-through's aim (above) is no part of a proposal.
**Tests** (`situation_rules`, 265 checks - fltest's own count): each S5 rule both ways over synthetic situations (the lane:
far / arrived / near, closed both ways - trip, hold -, unknown, one neighbour; the mission leader dead / hostile / arrived;
the pass ring's six exclusions, Survive, the kept pass; the route-lane by step 1's object or navmarker, unplaced, no
neighbour placed, not the last text, one entry; the route by object / navmarker / none; the waypoints by position / object
/ only the last entry; the arrival's dock ring, Dock verb, airlock; the navmarker's way - 300 m off, the nearer hostile, a
ship -, the via point - retries 0, another system, no pilot -; the rest and the arrived rest beside / with / without the
survival lead, follow_distance) and the order (lane before the arrival, the route's ride before the arrived rest, the
arrived rest before the route, the waypoints before the arrival, the marker's way before its via point); every recorded
fixture decides something other than "none"; the S4 solar-target case decides `arrive` now. `situation_io` 133 (v5
records, v5 under v4 / v3 / v2 / v1 headers fails, a v4 rewrite round-trips without the v5 fields). Unit tests 38 / 0
failures (22,191 checks). Release build (d0715f7 src:clean) 4,548,096 B (+11,264 B, +0.25 % over the ratchet's 4,536,832)
+ `win7_check` OK (3 binaries, 532 imports).
**In-game budget (up front):** 1 background run on instance 2 on the clean build of the S5 commit, <= 6 fresh games, <= 15
min of game; the driver (`runs\a37s4_2026-10-30_1052\driver.ps1` with S5's rule list) stops a game on S5 decide-exit
verdicts only; the branches predicted from the earlier runs' event texts (S3 / S4 runs), not from actions.
**In game, after the commit** (d0715f7 src:clean, instance 2, background, muted, a fresh game per save, `ap preset
speedrun` + `ap set situation_shadow 1` + `ap start objective`; run `research\experience\runs\a37s5_2026-10-30_1125`:
driver.ps1 / driver.log, `<save>.events.txt`, one `ap situation last` per branch and game, shots; 5 games, 3 min 47 s of
wall time): **14 shadow events, 14 agree, 0 differ - 9 at S5 exits** (the driver's `total` line and
`tools\shadow_digest.py` over the folder; the results commit first wrote 15 - a miscount, corrected by the driver) (arrive 3, rest 2, lane 1, arrived 1, navmarker 1,
nav-way 1) + S3's jump, tractor-exit (keep) and airlock (the first tractor-exit and airlock verdicts in game), S4's
threat-fight and one hold (held:jump). Per game, every prediction met: S207 M07.gate - `jump`, then `rest hold 0 rest`
(21660 after the jump, nothing to do: the final rest), `held:jump`, then `lane dock 2538460490 lane` (23250, "toward
3075320138 at 2722 m"); S009 M03 step 13 - `threat-fight` (the Valkyries), `arrive travel 2660539341 arrive` (22525 "Fly to
Planet California Minor"), `arrived hold 0 rest` (its trip ended), `arrive travel 2660539341 dock` (22415, the Dock step);
S010 M03 step 16 - `arrive travel 3196110401 arrive` (22490 Willard; 4.0 ms); S180 M12.core - `navmarker travel 0
navmarker` (24230 "Fly to the Core"), `tractor-exit` keep (21660, the power cell 830 m off within the beam), `airlock travel
2281279501 airlock` (24240, the granted exit door 1,038 m off), `nav-way travel 2460445762 dock` (24245, the marker on the
core's exit dock); S023 M03 end - `rest hold 0 rest` (no objective; 39.3 ms - the game's first snapshot). Ten `ap situation
last` snapshots are fixtures (`tests\fixtures\situation\m07_gate_lane_s207`, `m07_gate_rest_s207`,
`m03_california_arrive_s009`, `m03_california_arrived_s009`, `m03_willard_arrive_s010`, `m12_core_navmarker_s180`,
`m12_core_navway_s180`, `m12_core_airlock_s180`, `m12_core_tractorexit_s180`, `m03_end_free_rest_s023`; `snap v5`, `decided
rule=`): host-free hash = the game's 12 / 12 (`fltest.exe -snaps <run>`) and 25 / 25 over the fixture folder, the rules
give exactly the recorded decision 18 / 18 (with S3's and S4's). Shadow cost 0.9-4.0 ms a decision (43.7 / 39.3 ms for the first one of
the S207 / S023 games: the one-time reads). Unit tests 38 / 0 failures (22,265 checks). **No differ: nothing to classify** (no porting bug, no hidden-static
difference met).
**Not verified (exact):** of the 12 S5 rules, 6 met in game (lane, arrived, arrive, nav-way, navmarker, rest); 6 not
(mission-form, pass, route-lane, route, waypoints, via) - synthetic tests only. Of the 21 S5 exits, 6 met (lane's dock ride,
the arrived hold-rest, arrive - both its actions, arrive and dock -, nav-way, navmarker, the final hold-rest); 15 not (lane's
lane-wait trip / hold and its far trip, mission-form, pass 2, route-lane 2, the survival lead's hold / escort in both rests
4, route, waypoints, via). Reach for the rest: M06's race rings (pass), M07 "Fly to Planet Kyushu" (route-lane / route, 5
entries), M09 Ozu (mission-form), M04 West Point (lane-wait) and the M04 survival lead - the shadow gate (every leg with
the switch on, the driver's) measures them; the navmarker's via point needs a trip that ended short of its marker.
**Retro.** Budget 1 run / <= 6 games / <= 15 min, used 1 run (5 games, 3.8 min), 0 harness bugs, 0 wasted runs; 0 failed
builds, 1 failed test build before the commit (a captureless lambda in the test used a function-local constexpr: moved to
file scope - the cheap step caught it); every branch predicted from the earlier runs' event texts held (S4's retro applied:
S009's post-fight decisions read off the S4 run's `objective decide ... text=22525 ... nav=1` + `start mode=travel
target=...` lines). One miss of my own outside the game: my wait loops' end pattern (`done`) matched the rule's detail "this
step's travel is done" in driver.log - three early returns, no effect on the run -> a driver's end marker must be a line of
its own (match `^\S+ done$`). Bonus coverage: the M12 core save met two S3 rules no run had met (tractor-exit, airlock).

## S5's shadow gate (30 Oct 2026, the driver)
`tools\flgate.ps1 -Tag s5shadow -ApSet 'situation_shadow 1'` on d0715f7 src:clean, instances 1-3 (queues a / b / c), 09:28-10:38Z:
**487 shadow decisions, 487 agree, 0 differ, 23 rules over 14 legs** (`tools\shadow_digest.py --gate s5shadow`; threat-fight 61,
arrive 57, ship-target 45, jump 42, held:mission_holds 33, lane 32, pre-stage-next 28, navmarker 28, pass 22, held:jump 22, rest 19,
survival-stage 18, arrived 16, script-goal 11, escort 6, mark-fight 5, route-lane 5, held:lane 4, pre-stage 4, nav-way 2, airlock 1,
tractor-exit 1, and one more) - nothing to classify. First met in game here: pass, survival-stage, script-goal, escort, route-lane,
pre-stage. Still never met in game (synthetic only): mission-form, route, waypoints, via, pre-aggro, survival, leash, marked loot,
script-destroy, dock entry. Gate verdicts (`tools\digest.py --gate s5shadow`): 11 OK (no SLOWER), 3 FAIL, each judged by `run-judge`:
M13 a death at 162 s (one >9,000 HP hit 1.3 s after targeting the newly uncloaked 120k solar 3364 m off, shadow agreed, 1.5 ms;
the same decision in runs without the shadow survived - a known one-hit class at a new site, the runners head for Toledo there);
M11 a death at 975 s in the second order-ship defence (big-hit 2,988 at hull 0.07, 5 attackers at ~344 m; the last shadow decision
60 s earlier) - known damage class, new site; **M05 a HARNESS false fail**: checkpoint M05.cambholein waits for objective 22867,
which the script posts during the jump tunnel - this run never got it (0 lines) though the ship was in the new system and moving
on (FLController.log 1682-1707): the fail limit fired on a skipped objective. Shadow cost: 0.4-4 ms a decision, 42 ms the first of
a game. Not proven by one gate: behaviour neutrality in the strict sense (paired runs off / on would; the judge's note). Budget 1
gate, used 1; 1 harness bug (M05's checkpoint, fixed in the harness after the gate).
## S6 results (30 Oct 2026)
**Built** (pure, behaviour-neutral: nothing in the game calls it - `goalqueue.cpp` is compiled into the logic by the
`controller\src\impl\*.cpp` glob and into fltest; ObjectiveDecide / ObjectiveSupervise and every game path untouched;
`tools\decision_check.ps1` "decisions unchanged (95 lines)"; no snapshot change - snap stays v5): `goalqueue.{h,cpp}`:
- **A38 `GoalRecord`** (`goal v1`): id, version, `Type` (closed: mission_step, fly_to, dock, trade_lane, jump, formation,
  follow, escort, attack, clear_area, defend, loot, hold, repair, wait, base_action), `State`, `Origin` (story / player /
  job), parent / step, depends_on, subject (`Ref`: object / pos / system + system), typed `Params` (within_m, radius_m,
  count, toward, the step's A41 kind), `when` / `done` / `fail`, `Policy` (knowledge 0 / 1, engage auto / avoid /
  protect / fight, side effects), confirm, repeat (-1 forever), source_text, display (title / detail string ids),
  step_key, `EntryMemory`; one text line per record in a fixed field order, `goalqueue v1` header + `end`; parsing fails
  closed (unknown / missing / repeated / out-of-order field, unknown enum name, bad value).
- **The condition language** (closed, 11 kinds, each Yes / No / Unknown over the Situation - Unknown is never done or
  failed): true, dist_obj (cnd_distship), dist_pos (distvec / distcircle / locenter), destroyed (one object), docked
  (baseenter), in_system (systementer), step_left (the game's step key moved on: the story's own done), lane_ride
  (tlentered), leader (playermaneuver formation), hull_below, script_goal (a trigger achieved in the Situation or complete
  in the live table); `!` negates; done / fail = any of, when = all of.
- **`Validate(rec, sit, env)`**, the first refusal with its reason code: bad_version, unknown_type, unknown_field,
  bad_param, no_subject, unresolved (subject no contact / destroyed, an unknown depends_on / parent, references without a
  queue), bad_subject (dock on no base, formation / follow / escort with no friendly ship, jump on no gate, a lane on no
  ring), knowledge_scope, game_lock (mission holds / the game's dock / no ship; a lane closed both ways), no_route
  (another system and no route oracle: fail closed - `Env::routeKnown`, null today), broken_constraint (a never-attack
  step; a subject outside an armed - or unknown-armed - leash). A MissionStep's subject is informational (never refused).
- **The re-proof** (`Prove` / `ProveAll`): schema refused -> failed; done -> achieved; fail -> failed; game_lock ->
  blocked, any other refusal -> failed; `when` not Yes / a dependency not done -> blocked; else valid.
  **Achieve semantics** (`PopAchieved`): the achieved head(s) popped before anything acts, its steps (children) with it,
  its id out of every depends_on, a repeat re-armed at the back with fresh memory, at most one pass (a forever-repeat
  cannot loop); the new head Active.
- **`Rebuild(sit, prev)`**: the story entries from the situation alone (`StoryEntries`: the step as a MissionStep - origin
  job for a job's step, its subject / A41 kind / text / step key, done = step_left - and the story source's script goals
  as its children: the live table's armed goals, the ini view as the fallback; fly_to / clear_area / wait, done =
  script_goal, knowledge 1), `prev`'s player entries kept while not failed (their dependencies on rebuilt entries
  dropped), a carried "Dock X" entry kept while valid, `prev`'s story / mission memory and step key. No `prev` = cold.
- **The executor memory classed** (`MemoryFields`: all 35 fields of `situation::Queue`, a test checks the count):
  derived each time - seq, leash_ship / leash_dist (`DerivedLeashShip`); entry memory dying with the step (the story step
  entry's `EntryMemory`) - cruise_lock, escort, bad_goal, dead_goals, script_marks, arrived,
  step_age (as the step's start clock), mission_leader, via_next, arrive_retries; story memory - last_real and (since S7
  round 2, record / queue v2) seen_alive / seen_listed (the queue's `StoryMemory`: game scope like the old path's
  statics - in the step's entry they were lost at a step with none), last_dock (a carried story "Dock X" entry, A37_PLAN 1); mission scope - surv_title / mission / ended /
  system, survival; executor-only (`ExecMemory`, not the queue's) - mode, target, sub, sub_target, action, tr_dest /
  tr_object / tr_pos, lock / lock_have / lock_p2, tr_pass, escorted. `ToRulesQueue(queue, exec, sit)` builds the rules'
  Queue, `FromRulesQueue` classes a recorded one (lossless: 25 / 25 fixtures decide the same through it).
- **The step key** is written only by `Advance(queue, sit, proposal)`, after Decide: a decision (not held / none) for a
  key the queue does not hold replaces the story entries (fresh entry memory, the step's start clock) and only then writes
  the key; every decision keeps last_real and the carried Dock entry by ObjectiveDecide's lastDock rules (a Dock step's
  base / planet sets it, a transit Dock step clears it, a placeholder the script does not know keeps it while its object is
  a contact and no leash is in the system). Assess / ProveAll / PopAchieved / Rebuild write no key (tested, incl. the
  4c4fb6f sequence: step B shows, a held verdict keeps key A, B's decision writes B).
- **D10** (`Resume(paused, sit, pause, policy)`): rebuilt as above, every achieved entry popped (not only the head), the
  player's no longer valid ones dropped, then `ResumeChoiceOf`: nothing (empty), silent (head valid, pause <=
  `ResumePolicy::silentMaxMs`, no system change, no dock, no job taken, moved <= `movedMaxM`), else ask (RESUME / CLEAR).
  **The thresholds are placeholders S7 tunes: kResumeSilentMs = 30 s, kResumeMovedM = 5 km.** A silent resume of the same
  step keeps that step's entry memory; an ask re-plans it fresh.
**Tests** (`tests\goalqueue_test.cpp`, 164 checks - fltest's own count): a full record round-trips and 15 parse refusals;
every validator reason code (incl. the route oracle both ways, a leash, a never-attack job step, knowledge "all" vs
"exposed"); the four proof states + Unknown never done; achieve pops (two heads, the children, depends_on, a repeat, a
forever-repeat); the step key; the carried dock / last_real through a placeholder; D10 - silent (memory kept, a dead
escort dropped), ask for long / another system / docked / a job / far, the story moved on (the player's achieved entry
popped), nothing, a blocked head asks; S182's Toledo goal as a knowledge-1 child refused under "exposed". Unit tests 39 /
0 failures (22,429 checks). Release build (503dede+dirty src:4538a852) 4,549,632 B (+12,800 B, +0.28 % over the ratchet's
4,536,832; +1,536 B over S5) + `win7_check` OK (3 binaries, 532 imports). In-game budget 0 (host-free; a leg gate ran on
instances 1-3).
**Cold = warm** (`Decide(Assess(s), warm)` vs `Decide(Assess(s), ToRulesQueue(Rebuild(s), exec, s))`, the executor's
own state the same in both; each difference attributed by putting the differing fields back one at a time) over the 25
fixtures: **23 agree, 1 declared-memory exception, 1 finding.** The cold queue lacks recorded memory in 16 of them
(step_age 16, last_real 8, surv_title / surv_system 8, arrived, cruise_lock, seen_alive / seen_listed 1 each); 7 are v1 /
v2 fixtures with no memory record (agree trivially).

| Fixture | Verdict | Warm / cold |
|---|---|---|
| free_docked_c701, m03_end_free_s023, m03_end_free_rest_s023, m07_gate_rest_s207, m13_start_3989 | agree | rest |
| job_killships_c701 | agree | lane (travel 2793534401) |
| m03_california_arrive_s009, m03_willard_s010, m03_willard_arrive_s010 | agree | arrive |
| m03_convoy_escort_s008 / _threatfight_s008 | agree | escort formation 15 / threat-fight escort 15 |
| m03_valkyries_threatfight_s009, m12_hole_s182, m12_hole_threatfight_s182, m13_battleship_threatfight_s350 | agree | threat-fight combat |
| m07_gate_jump_s207 / m07_gate_lane_s207, m07_tau31_s208 | agree | jump / lane dock |
| m12_core_airlock / _navmarker / _navway / _tractorexit_s180 | agree | airlock / navmarker / nav-way / tractor-exit (keep) |
| m13_battleship_markfight_s350 | agree | mark-fight (huge static) |
| **m03_california_arrived_s009** | **exception: arrived** (entry memory) | warm `arrived hold rest`; cold `arrive travel 2660539341` - the step's trip ended 503 m from the ring, the cold queue flies it again |
| **m07_tau31_prestagenext_s208** | **finding: last_real** (story memory) | warm `pre-stage-next travel` (lastReal 23250 -> juni_cautious); cold `ship-target follow 16` - at "Awaiting" after the Tau-31 lane ride nothing in the snapshot names the last real objective (the live stand-in names the script, not the text), so the mark fallback follows Juni |

"From off" (the executor's state dropped too - an activation) differs in 3: the two above and m12_core_tractorexit (its
tractor-exit keep needs the running travel: cold-from-off arrives at the loot 13) - executor state S7's executor must
carry through a pause or re-derive.
**Exceptions, why:** `arrived` is entry memory by A37_PLAN 1 (it dies with the step; the queue keeps it while the step
lasts, D10's silent resume keeps it). It is derivable: the step entry's own done test (within the arrival reach of its
subject) would make it a fact - S7's choice.
**Finding (the hidden memory A37 removes):** ObjectiveDecide's `s_lastReal` survives toggles and loads today; after a
load at an "Awaiting" placeholder the rebuild cannot know the last real step. Warm (the queue kept through D10's pause)
decides as today; cold (after a load / a fresh process) follows Juni instead of pre-staging. Options for S7: (a) accept
it - the queue's story memory carries it through every pause, and only a load loses it (then the cold decision, gated);
(b) a snap v6 fact from the live table - the completed row whose Act_SetNNObj set the last real objective (the objective
history) - so the rebuild derives it; (c) the ini view's fromLast stand-in (mission.cpp's lastText) - itself memory, no.
Not added to the snapshot here (prefer none).
**Not ported / open for S7:** (1) `DerivedLeashShip` takes ScriptBlock's named-ship anchors only (not the target-ship /
survival-lead anchors, not today's "kept while the block does not apply") - no fixture has a leash, so cold = warm does
not test it; (2) the adapter must add every queue entry's subject to the snapshot's contacts (else a far player goal is
`unresolved` -> failed); (3) the route oracle (`Env::routeKnown`, core\game\route.*) - null today, so every
other-system goal is no_route; (4) the condition language has no projhit / healthdec / lootacquired / cargoscanned /
commcomplete / timer kinds (the story step waits on step_left); the story children are S2's distance / destroy goals
only (baseenter / tlentered rows are no entries yet); (5) the executor (side effects, the running travel across a
pause), the switch `situation_arbiter`, the resume thresholds' tuning, the goal panel (S9).
**Retro.** Budget 0 in-game runs (host-free slice; the gate held the instances), used 0; 1 failed test build before the
first run (two compile errors: `situation::Parse` is not in `situation::io`; an ambiguous template overload for the
`float[3]` members - fixed by a second macro, not an overload); 0 failed tests beyond the expected-verdict table, which
was filled from the first run as designed; both differences were predicted from reading the rules before the run (the
arrived rest and the pre-stage-next lastReal). Guard fires: 2 of my own (a `sed` patch to main.cpp / build.cmd -
shell-patch-code; a stray `python3` in the same command) - both mechanical guards already exist and held -> no new check;
lesson: code files with Edit from the first try, never a batch sed.

## S7 results (built 30 Oct 2026; in-game pending)
**Built behind `situation_arbiter`** (Bool, default 0; SETTINGS.md; with it 0 nothing changes - every helper below starts
at once exactly as before, `tools\decision_check.ps1` "decisions unchanged (95 lines)", the shadow untouched):
- **Plan mode + the executor** (`autopilot_objective.cpp`): with the switch on, ObjectiveDecide's body (`DecideBody`, the
  old path kept intact for S10) runs in plan mode - it still decides (the oracle) and still plans (the trip's geometry, the
  fly-through, the memory the supervisor and the travel read: travelObjective, goalTrigger, flyGoal, preStageTarget,
  laneRing, leashShip, the survival stance, ...), but every start it would make and its ObjectiveStart are recorded
  (`Pending`; StartTravel / StartOrKeep / the new `Dec*` helpers for the hold-at-point, the lane ride's dock, the pass,
  the shot, the waypoint list, the fly-through). Then `ArbiterExecute`: the arbiter decides (`situation::Arbitrate`,
  `situation_snap.cpp`: the snapshot at the exit with the memory as the decision began, the executor's memory classed into
  the goal queue - `FromRulesQueue`, lossless -, `Decide` over `ToRulesQueue`, then `Advance` writes the step key), and the
  executor starts **the arbiter's decision**: the body's own plan when the arbiter agrees with it (its geometry is
  today's), else the rules' plan - every rule now carries its exit's start (`situation::Plan` in `Proposal`: the kind,
  object / point / system, noDock, the lane's `toward`, the pass radius, the fly-through, and the exit's writes, applied
  only on a differ). Then the decision's ObjectiveStart with the real start's result.
- **Side effects check the game first** (`situation_exec.{h,cpp}` `CheckSideEffect`, pure; `GateFactsNow` reads the game):
  nothing while not in space, riding a lane or jumping (refused); a dock request (a Dock plan or a trip that docks at its
  end) never while the game docks us (refused), never to the object the game's dock already holds (keep while our mode
  runs, else refused), never a second while our Dock mode to it runs (keep), never another while our own request awaits
  its answer (refused; "awaits" = our Dock mode requested and the game holds no port yet - a proxy); no formation request
  while the game already has us under that leader (adopt: `StartAdopt` - Start without the restart's free flight, the
  maneuver marked started); cruise untouched (as today: StopImpl keeps it across a restart). Every start or refusal emits
  `side-effect <kind> target= step=0x... rule= [keep|refused|adopt why=]` (kinds travel / dock-trip / dock / pass / shoot /
  hold / waypoints / the mode); every decision `situation arbiter agree|differ <proposal> old=<rule,mode,target,action,
  keep> plan=agree|<diff> ok= step= ms=` - S10's measurement of the rules' plans against the old ones (`PlanDiff`: points
  within 1 m, the fly within 0.01 / 1 m).
- **D10, the pause** (`situation_snap.cpp`): the player's stop (ap stop, the pad, player input: StopImpl) keeps the queue
  with the working memory as it stands (`ArbiterPause` + `goalqueue::CarryMemory`) and a pause mark (game clock, system,
  position; docked / left space meanwhile from Stop); a death or a new game (host v34 GameLoads, controller.cpp) forgets
  it. The next activation (ap start objective / arm / the smart start: `ArbiterActivation`, a running objective mode
  paused first) resumes at its first decision (`ArbiterResume`, before the step upkeep): `Resume` re-proves the paused
  queue; **silent** (the head valid, the pause <= `resume_silent_s` 30 s, no system change, no dock, no job, moved <=
  `resume_moved_m` 5 km) of the same step restores all of that step's memory (arrived, escort, bad / dead goals, retries,
  via, cruise lock, mission leader, the step's age) and marks the step as seen; **ask** re-plans it fresh with the story
  memory (last real text, the carried dock) kept - S9 draws RESUME / CLEAR, until then the start is the player's yes;
  with no paused queue (the first start in this game) the queue is rebuilt cold. Event `situation resume <choice> cold=
  same_step= paused_ms= moved= system_changed= docked= job= arrived= ... why=`; `situation pause ...`; debug `ap situation
  queue` (the queue, the pause mark, the paused queue).
- **The decisions taken for D10** (the driver's, AFK): `last_real` is story memory the queue carries through a pause; a
  load loses it (no snap v6) - before each decision the old body's `s_lastReal` is set from the queue
  (`SetObjectiveLastReal`), so the oracle and the arbiter see the same. S6's `arrived` exception: a rebuilt step entry
  starts arrived by the step's own arrival test (`goalqueue::WithinArrivalReach`: within 1 km of the arrival object - a
  planet's docking ring, else the target - for a step that neither docks nor names a ship / ring / gate / hole; within
  600 m of a navmarker with no airlock / gate / hole on it - the supervisor's own "there"). The resume thresholds are
  settings rows (cheap: two kDefs rows), named placeholders for S9.
- **The supervisor stays on the old path** (its holds, the leash trip back, the dock-wait holds, the formation refusal /
  retry, the lead stall, under-fire, the survival upkeep, the re-decision triggers): they are the executor's upkeep of the
  running sub-mode, reacting to its own state (lastStopReason, the running travel, the formation's answer) - executor-only
  memory by A37_PLAN 1 - and Rule_Held already agrees with its holds (S3-S5: 0 differ). Their starts do not pass the gate
  yet (S10 with the old body, or earlier if the gate shows a duplicate there). The leash anchor the arbiter reads is the
  executor's own (S6 open (1): `DerivedLeashShip` covers named anchors only).
**Tests** (fltest's own count): 40 tests, 22,716 checks, 0 failures. `situation_exec` (287 checks): every gate refusal
(not in space, lane, jump, the game docks us, the game's dock to it - keep / refused -, our dock to it, a reply pending,
already led - adopt -, and what is not refused), the kinds and the event line; `PlanDiff`; `StartArgsOf` (held / the
leash's running trip: no ObjectiveStart); the pause facts (a clock gone back, another system, docked, a job); the rules'
own geometry (the via point, the far mark's trip 1.5 km short, the leash trip, the navmarker); the arrival reach. Over the
25 fixtures: **Decide -> executor = the old ObjectiveStart arguments for every decided fixture (18 / 18)**, every plan
fits its mode (25 / 25); **pause -> resume, cold = warm with the queue's carried memory:** a short pause (silent; 21 the
same step - the other 4: three with no objective, "nothing" to resume, and M13's docked start, which asks: docked) decides
as the recorded memory 25 / 25 (the
S6 finding agrees: the queue carries last_real 23250 -> pre-stage-next); a long pause (ask: re-planned, the story memory
kept) 25 / 25; **a load (no paused queue, cold) 24 / 25** - the one difference is the decided finding
(m07_tau31_prestagenext: follows Juni, ship-target follow 16), and S6's `arrived` exception now agrees by the arrival
reach (goalqueue's cold = warm: 24 agree, 0 exceptions, 1 finding). decision_check unchanged (95 lines). **Release**
(src: this commit's tree, built during the gate - test / release only) **4,653,568 B (+116,736 B, +2.57 % over the
ratchet's 4,536,832; +103,936 over S6)**: most of it goalqueue.cpp, compiled into the logic since S6 but dropped by the
linker until something called it, plus the executor; `win7_check` OK (3 binaries, 532 imports).
**In game (the driver's, after this commit; budget 1 gate, 1 fight-matrix, 6 runs):** (a) flgate with `-ApSet
'situation_arbiter 1'` - no FAIL / SLOWER; digest the `situation arbiter differ` and `plan=` non-agree lines (S10's
evidence) and every `side-effect ... refused` (a refusal on a leg is a finding); (b) SaveS182 M12.hole x3 with the switch:
PASS - but note: S7 is parity, its first decision there is the mark fight as today (S4: `threat-fight combat 0 fight`),
not a trip toward Toledo (the rules read the ini view; the live view's Toledo goal is S2's assessor fact, no rule takes
it) - that clause needs a rule change of its own (a later slice, with its A/B); (c) SaveS208 M07.tau31 x3: PASS, the
lane ride then `pre-stage-next` at "Awaiting"; in one run an `ap stop` + `ap start objective` within 30 s after the ride ->
`situation resume silent ... same_step=1 last_real=23250` and pre-stage-next again (D10 silent), one after > 30 s ->
`resume ask` (re-planned, last_real kept), and a reload of S208 there -> `resume ... cold=1` (the decided finding); (d)
sweep `fight-matrix` with the switch (the threat / pre-aggro / survival starts through the executor). Then default 1 +
CHANGELOG + sprt on the differing segments (S7's done-check, the driver's).
**In game, round 1 (30 Oct 2026, build d8c498f = 08346a9's code; NOT kept):** (a) gate s7arb: 12 OK, M12 SLOWER +68 s
(run-judged: the end segment's Nomad chase, an existing class, decisions as s5shadow - not S7), M11 a death; the arbiter
491 agree / 1 differ (M13 13:06:40 `pre-stage-next` vs the old `threat-fight` after a 2-kill fight, the 879 s PB run), 0
refused of 44 side effects. **But run-judge found the parity broken behind "agree":** `goalqueue::Advance` writes
`lastReal` at every decision while the old body writes it only past the exits before its script-goal block (the jump exit
returns first), and ObjectiveDecide fed the queue's value back into the body - so both sides read the same changed memory
("agree" is circular) and `pre-stage-next` fired after jump steps (M11: 24125 -> 24050 where s5shadow followed ship 62;
24080 -> 24085; 24110 -> 24115 from the jump tunnel, a lane trip 35.6 km into Zone 21, the game's dock flying straight,
a 3,143 one-shot mine death - the class mpb23 met on the direct route). **Fixed** in `Arbitrate`: the queue carries the
body's own `lastReal` after its decision while the body is the oracle (S7-S9; the rules own the memory from S10). Lesson
(-> a check): an "agree" between two sides that share a memory proves nothing - the shadow gate (S5) compared against an
untouched body, the S7 gate must compare decision sequences against the switch-off gate at the same story points
(`shadow_digest.py` over s5shadow vs s7arb2, not the arbiter's own agree count). The check (30 Oct 2026):
`shadow_digest.py --gate s7arb2 --baseline s5shadow` (executed rules per objective step, exit 1 on a differing step, 2
on missing data); on s7arb it flags M11 `24125>21660` / `24080>21660` ship-target -> pre-stage-next (24110: s5shadow
never reached it, one-sided). (b) SaveS182 x3: first decision
`threat-fight` agree, 3 / 3. (c) SaveS208: silent (`same_step=1 last_real=23250`, then pre-stage-next), ask (35 s:
`last_real` kept, pre-stage-next), cold after a reload (`cold=1`; S208 reloads before the lane ride, so it decided `lane`:
the Awaiting "follows Juni" finding needs a save at Awaiting) - all with the bug in place, re-run on the fix
(`runs\a37s7_2026-10-30_1402`). Round 2: the gate s7arb2 on the fix.
**In game, round 2 (30 Oct 2026, gate s7arb2, b63c1ad src:clean; NOT kept):** 14 legs - 11 OK, M12 SLOWER +56 s (round 1
+68 s: the same class, a confirmation due), M11 and M13 deaths. run-judge (both): **M11 = a hazard that already existed** - the
arbiter agreed to the end (`threat-fight`); the static hold pinned at 245-250 m from the Navy satellite behind a solid,
every launch "held: a solid in the way", 0 kills in 40 s, batteries 26 -> 0, 1 nanobot used of 26; the same pattern died
on 28 Oct (mpb2, oddsab-a): 3 deaths in 25 satellite fights (runners: salvos at 0 m or the fighters first, nobody parks at
250 m) -> an M11 fail-state item, not S7. **M13 = unclear, the arbiter a candidate:** the one arbiter differ of the gate
(14:31:15, `pre-stage-next` "24425 -> 24420 (declared next)" vs the old `threat-fight`) flies 1210 m to (6350,0,-5080) =
NomadCap2's spawn point (`docs\missions\M13.md` 128: `cap_dies` spawns it cloaked, `cap_timer` +25 s makes it hostile);
the ship waited there at hull 0.84 / 0 batteries, Cap2 uncloaked at 97-139 m and killed it in 7 s. Round 1 made the same
differ (13:06:40, 1188 m) and lived (hull 1.0, 15 nanobots); s5shadow (no differ) died at the same uncloak 3.4 km away -
n = 1 each. The runners fly to Planet Toledo here, never to Cap2. `shadow_digest.py` vs s5shadow: 10 steps differ, all
but M13 the end-of-fight `rest` / `arrive` noise or agreed by the body too (M10 `23865>21660` pre-stage-next "23870 ->
23870" in both sides: open). **Why the two sides split at M13:** same rule order, same `lastReal` (24425); the candidate
is the script block's `fired` - the body's `DestroyFired` asks the live object (`space::ObjectPos`) and its memory as it
reads, the rule `o.placed` and the memory as the decision began (`s_entry`) - so Cap1's destroy condition may count as
fired on one side only. Not provable from the run: no snapshot was kept -> **the check**: every arbiter / shadow differ now
logs its snapshot (`snap| ` lines, the first 16 per process; `situation_snap.cpp` LogDiffer), `tools\snap_extract.py
<run> <dir>` writes them as fixtures for `fltest.exe -snaps <dir>` (the host-free replay). **Found and fixed (30 Oct
2026):** probe M13-s7snap-2 (SaveS349, the battleship checkpoint, `situation_arbiter 1`, PASS 121 s) reproduced the differ
at once (15:24:18, 1134 m) with its snapshot; replayed host-free on its own memory it decides the body's `threat-fight`.
The cause: `FromRulesQueue` kept seen_alive / seen_listed in the step's entry, and at this exit the objective is blank
(text 0) - no step entry - so the round trip dropped them, Cap1's destroy condition (`cap_dies`, seen alive, gone from
the contacts) no longer read as fired, and `pre-stage-next` opened. S6's "lossless" check covered only steps with an
entry. Fix: the seen lists are story memory (`StoryMemory`, record / queue text v2); the snapshot is the fixture
`tests\fixtures\situation\m13_bsdown_blank_s349.snap` (old code: "classing lost memory: threat-fight vs pre-stage-next",
lossless 25 / 26; fixed: 26 / 26, 40 tests 0 failures). Release 4,652,544 bytes. Probe M13-s7snap-3 on the fix: the
blank step agrees (`threat-fight`), 0 differ, PASS. The S7 driver on 4eef991 (`runs\a37s7_2026-10-30_1533`): (b) M12
first decision `threat-fight` agree 3 / 3; (c) silent `same_step=1 last_real=23250` then pre-stage-next, ask after 35 s
(`last_real` kept), cold after a reload `cold=1` (then `lane`, the reload precedes the lane ride) - 6 games, 0 differ,
0 refused. **Round 3 = gate s7arb3 on 4eef991: parity verified** - 481 arbiter decisions over 14 legs, 481 agree / 0
differ; vs s5shadow 10 steps differ, all end-of-fight `rest` / `arrived` order noise; 13 PASS; M11 died in the Zone 21
mines (the gate -> Osiris-mouth line sank to y -1049; 0 differ in the leg; 3 earlier deaths there, all below -625 m:
pre-existing, run-judged); M12 SLOWER vs an outlier best (414 s vs 354; switch-off gates 386-424 s; its 17 decisions and
plans all agree, 0 side-effect holds, ~1 ms each - not S7). **Retro (S7):** budget 1 gate + 1 driver run, used 3 gates +
2 driver runs + 2 probes (`docs\test_budget.tsv`); both misses were parity checks that looked at the wrong thing (an agree
over a shared memory; "lossless" only over steps with an entry) and each cost a gate -> the checks now in place: decisions
vs a switch-off gate per story step, a snapshot with every differ, the blank-step fixture. `situation_arbiter` stays 0 by
default: it changes no decision by design, so turning it on buys nothing until a slice gives it its own (S8+); the old body
goes at S10. Separately, for the owner's standards: pre-staging toward a "Destroy" objective's point (a hostile spawn) is
a risk the runners do not take (the body does it too wherever it fires).
**Not verified:** everything in game; the rules' own plans run only on a differ (on agreement the body's plan runs), so
their geometry is checked against the old plans by the `plan=` field, not yet by flight; the gate's "reply pending" is a
proxy (no port held yet); the supervisor's starts bypass the gate; Ask has no panel yet (S9).
**Retro.** Budget 0 in-game runs (the gate held instances 1-3: test / release builds only), used 0; 0 failed builds, 0
failed tests (first build green). 2 guard fires of mine - a bare `python3` in a Bash line and a `sed` edit of a .cpp
(`situation_snap.cpp`'s usage string) - **the same two S6's retro named**: the guards held both (no harm), but the class
repeats across sessions -> the spawn brief for code slices should carry "Bash: no python3, no sed on sources" (the driver's
call; this subagent may not edit the harness). Size: +2.6 % - the next slice that adds code should look at the
ratchet first (S10 takes the old body out).

