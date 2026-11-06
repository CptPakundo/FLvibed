# Autopilot planning: goal architecture, re-orientation at every activation, natural-language commands, the game's goal vocabulary
Status: current
Checked against: FLvibed 3c9c1390 (6 Nov 2026, short form: 49 watched-path commits: the module split P0-P11e (moves, comments, map rows) and the autopilot_objective / combat splits; lock_trap d3814fd1 (default 0, not flown), the direct fallback (fx3 3 / 3), 37639266 stop / new-game fixes; the 31 Oct wait_for_trent inference refuted (marked in place); situation_arbiter still 0)
Dirty paths at check: another session's uncommitted the private work queue, `tools/uimock/options.py` (not judged); 950b25cc, 4a270d9e and 3c9c1390 landed during the round, read
Watch: modules/autopilot/docs/AUTOPILOT_UX.md, modules/autopilot/src/autopilot*.cpp, core/game/mission.*, docs/FREELANCER_RE.md, docs/UI_BUILD.md
Evidence: none saved (web pages read 29 Oct 2026; the Game AI Pro PDFs could not be fetched - those claims rest on
abstracts, secondary pages and the agent's prior knowledge, marked [assumed] where it matters)
Sources: a research agent's report (web + the repo + the local KnowledgeBase clone); the coordinating session checked the
repo references. Tags: [page], [file], [assumed]

## What it means for FLvibed

*6 Nov 2026 (3c9c1390, short form):* the switch-over has not happened: `situation_arbiter` is still default 0 (`docs\SETTINGS.md`) and the module split moved the code without changing it (`autopilot_objective.cpp` is now look / decide / supervise, 739734bd). Three things touch this note. (1) **The 31 Oct cause is refuted:** SaveS450 holds the lock with `wait_for_trent` already fired (`AUTOPILOT.md`, checked 31 Oct ~15:45), so fx2 i3's empty path has another cause; the direct fallback (a best path that comes back empty within 8 km: straight to the object, the dock answer decides) flew fx3 3 / 3 PASS, the likely cause being the planner's 500 m same-system range [inferred]. (2) `lock_trap` (d3814fd1; default 0, unit-tested, **not flown**) is the general rule the validator row asks for, read from the live table (A39), but it sits in `ObjectiveDecide` beside the arbiter (A37, A40). (3) 37639266: a load from the pause menu never met Mode::Menu, so running modes flew on into the new game; `ForgetGame` stops them - another remembered-state case for A37 and A42's load edge. A38 / A41 / A43 unchanged.

*31 Oct 2026 ~15:20 (32c80fce, short form):* the locked-gate loop (bd5d7af6, 44659916; `AUTOPILOT.md` 'Objective mode' fixes) is the case this note's validator row names - 'the game's locks allow ... locked gates, a best path exists': the executor asked the game for a route every ~1 s to a gate the script kept locked (ak2-b_i4: 154 'found no way'). The back-off now rests and re-asks; probe fx2 still STALLs in 1 of 3, and the cause the driver inferred (game-facts, not verified in game) is an armed trigger: `wait_for_trent` (Cnd_DistShip 320 m to Orillion -> Act_LockDock lock) fires after check_cap's unlock when our trip goes to the gate rather than to Orillion *[refuted 31 Oct 2026 ~15:45, `AUTOPILOT.md`: SaveS450 holds the lock with `wait_for_trent` already fired; the cause of fx2 i3 is another one]*. The general fix is the assessor reading armed triggers whose action locks the step's target (A39's `mission::LiveTriggers` exists) and satisfying them while the step lasts (A37, A38) - recorded on A37. strafe_pass (4a4bd7b1) was flown and parked BAD (`speedrun.md`).

*31 Oct 2026 ~04:00 (fc379a3, short form):* the commits since 5eb7bcb are M13 shape work in the existing handlers behind default-0 switches (`AUTOPILOT.md` has each with its verdict) and the audit's cleanup (1d6f6da: `extend` + 4 knobs, `big_pass`, `big_standoff`, `big_jink`, `hold_big_gap`, `hold_jink`, `blind_rush`, `static_volley`, the never-read `repair_*_at` deleted; the survival stance keeps `extend` as constants). None adds a goal type, a queue or an arbiter input: A37 / A40 / A41 stand as they were. Two touch this note's ideas: 29defbc - the autopilot's own escape-mission memory (`g_survival*`) and the system-entry point now start over at each load (NoteMenu), consistent with 're-orient at every activation' and A42's load edge; a4571892 - the dead test is the game's own kill value (relative hull exactly 0, FLHook kills with SetRelativeHealth 0) with the death camera as the backstop, and a PlayerDied stop no longer cancels a dead ship's behaviour (the commit says: not verified in game). `FREELANCER_RE.md` (f9657cb, ad11e02) gained a bay-request row and the finding that a guided round holds the whole target object (no part can be aimed at).

*30 Oct 2026 ~23:30 (5eb7bcb, short form):* the script readers grow toward A41's done / fail sources: `StepAdvanceOf`
names how a step advances (kill / timer / other; a "Destroy" step can end on a timer - M13 24420, verified in game),
`EscortByPlace` the player's distance goals, `ArmedBaseEnters` the base the script waits for, LOOK L0's `ChainGoals` the
triggers about to arm. They decide in `ObjectiveDecide` behind default-0 switches and gate on the English verb, outside
the arbiter (A37's switch-over must take them in; A40). MARKS-LOAD (09891f9): a load re-sends the marks of existing
objects, but unsaved mission solars leave a step targetless - a re-proof after a load needs a fit save (A42).

*30 Oct 2026 ~19:00 (bf4d1d1, short form):* **A37 and A38 are built, behind a switch.** S6 (ae5ba16) the goal queue as
this note's schema proposed: A38's record (typed subject, when / done / fail in an 11-kind `Cnd_`-like language with
Unknown never done or failed, policy, source text, display ids), a validator with 14 fail-closed reason codes, re-proof /
achieve semantics / rebuild from the live table (A39) first; cold = warm over 25 fixtures (23 agree, `arrived` settled in
S7, `last_real` a decided finding). S7 (08346a9) the executor: the arbiter's decision over the queue started through a
game-first side-effect gate, D10 pause / resume (silent / ask / cold; a load forgets the queue) - **in game parity 481
agree / 0 differ** (gate s7arb3, 4f6900d, after the fixes b63c1ad / 4eef991, both remembered-state bugs of the kind A37
removes); `situation_arbiter` stays 0 (4ae968b) - the switch-over and the goal panel / planner (`UI_BUILD.md` 5) remain.
A42's cold / warm and pause / resume oracles now exist host-free (`tests\goalqueue_test.cpp`, `tests\situation_exec_test.cpp`);
the toggle fuzz itself is not built. A41 gains decompiled `Cnd_` semantics (FREELANCER_RE: nag 20 s samples / 900 m
closing / 4 strikes, tether time cumulative, Cnd_Destroyed counts; not verified in game) that the LOOK plan
(`modules\autopilot\docs\LOOK_PLAN.md`) builds on. A43 unchanged.

*30 Oct 2026 ~11:50 (16df8bd, short form):* **A37 is being built as this note proposed, behaviour-neutral so far**
(WORK_QUEUE Q6, `modules\autopilot\docs\A37_PLAN.md`): S1 the pure assessor `situation::Assess` over a recorded snapshot
(4e1c8e7; host-free hash = the game's), S2 the live trigger table as its story source with the ini view as the
fail-closed fallback (63291e3, A39), S3-S5 every `ObjectiveDecide` exit as a pure `Rule_*` in shadow behind
`situation_shadow` (default 0; c79fc35, 16a4b63, d0715f7): in game **28 / 28 shadow events agree, 0 differ** (S3 4, S4 10,
S5 14; a1a2d57 corrected S5's count), decisions unchanged (decision_check 95 lines); the shadow gate s5shadow runs. Nothing
decides by the rules yet - the switch-over and the queue (`UI_BUILD.md` 5) are A37_PLAN's later slices. S1 found
exactly the kind of remembered state A37 removes: the host's objective title and marks survived a save load in one
process (the by-title stand-in then named another mission's script); fixed at the source (d488fbd: everything kept per
game starts over at Content.dll's loader notification, before the new mission runs; FREELANCER_RE "The load's own
moment"). A41 done (`objective_kinds.h`: 44 Cnd_ rows, 40 used by the HDE install, 16 job kinds), A40 verified in game
(a job's battle entry carries its objective_text id; route entries keep the words). A38, A42 (the load reset gives its
"after loads" edge a defined start), A43 unchanged.
*30 Oct 2026 (aafe41d, short form):* a live case for the stateless assessor (A37): gate q3s2's M07 failed because
`ObjectiveUpdate` stored the new step's key before deciding it, the decide-loop guard swallowed the decision, and the
key check later took the step for "the same objective again" - the ship fought on until the nag failed the mission
(4c4fb6f, latent since 22 Oct; fixed by forgetting the key). Remembered state that is not re-proven against the world
is exactly what A37's design removes. The decision timers now run on one game clock (ee25e00), so pauses no longer age
them. A37-A43 unchanged (the queue not built, WORK_QUEUE Q6).
*29 Oct 2026 (04b4c05, new; the owner: a general autopilot for every task the game sets, re-orienting itself at all
times - also when toggled off and on at any moment - multi-stage and multi-system on the fly, and later natural-language
orders):* the goal model is designed (`modules\autopilot\docs\AUTOPILOT_UX.md` sections 2-3: states, goal records with
completion and failure tests, the queue, reflexes, interrupts, the auto planner) but **the queue is not built**
(`docs\UI_BUILD.md` 5: QUEUE / DO NEXT greyed). The research says the design is the right one and adds what makes it
resume-proof:
- **A stateless situation assessor (A37)**: a pure function from a game snapshot to the situation (story step, marks,
  waypoints, system, dock state, contacts, threats, affordances), run on every activation and on events (objective,
  mark, system, dock, death, load) - never polling. The queue is the only memory and every entry is re-proven against
  the world. Goals have *achieve* semantics (a goal already true on activation is done), side-effecting steps (dock
  request, lane entry, a purchase) check the game's state first - no duplicate side effects.
- **A teleo-reactive arbiter** (Nilsson's ordered condition -> action list, which is `AUTOPILOT_UX.md` 3.3's
  arbitration already): player input, Held, survival, reflexes, interrupts (utility-scored with hysteresis), the active
  goal, Auto - resumes from any state because only the world decides.
- **HTN-style decomposition with expected states (Goal-Driven Autonomy)**: each composite goal expands into steps that
  carry the game state they expect; a discrepancy re-decomposes from the current state. No GOAP at the top level.
- **One plan format and one validator (A38)**: the goal panel, the auto planner and a later NL layer all emit the same
  goal records, accepted by one validator that fails closed (references resolve, knowledge scope allows, the game's
  locks allow - `Act_LockDock`, `PlayerCanDock`, `PlayerCanTradelane`, `LockManeuvers`, locked gates - a best path
  exists, no constraint is broken). NL later (A43): grammar first (proven in shipped play: VoiceAttack + EDDI for Elite
  Dangerous), an LLM optional and out of process; constrained decoding guarantees syntax, never meaning.
- **The top probe (A39)**: the live trigger table. A save lists the live triggers (`[TriggerSave] trigger = <CreateID>` +
  their condition state, `FREELANCER_RE.md` 1443, checked), so Content.dll holds it at run time; reading it would end the
  "Awaiting" (21660) gap after a mid-mission load (`docs\TESTING.md` "Which saves are usable") properly. **Built and
  verified in game 30 Oct 2026**: `mission::LiveTriggers` (`core\game\mission_live.h`), debug `mission live`
  (`FREELANCER_RE.md` "The live trigger table").
- **Language-free job steps (A40)**: a job step's `objective_text` id (e.g. 327734 `guide_to_main_battle`, 327744
  `kill_other_solars`) names the vignette step in any language - it can replace the objective mode's English keywords
  (`AUTOPILOT.md` 378, checked) for jobs; for story steps the NNObjective type plus the step's `Cnd_*`.
- **Coverage (A41)** - every objective kind the game generates mapped to a goal type and its done / fail source (table
  below), the measure of "handles everything". **Toggle fuzz (A42)** - the test for "re-orients at all times".

## Findings

### 1. Planning architectures in shipped games
- **GOAP** (F.E.A.R., Orkin GDC 2006: https://pages.cs.wisc.edu/~dyer/cs540/handouts/gdc2006_orkin_jeff_fear.pdf): a
  three-state FSM, A* over actions to the most relevant goal, replanning when a plan becomes invalid; plans a few actions
  long in practice - a poor fit for open multi-system missions.
- **HTN** (Killzone 3 bots, Game AI Pro 1 ch. 29; Transformers, Humphreys GAP1 ch. 12; Horizon Zero Dawn's HTN + utility,
  https://www.guerrilla-games.com/read/the-ai-of-horizon-zero-dawn, https://guerrilla-games.com/read/htn-planning-in-decima):
  shipped plans "rarely exceed a few actions" (https://www.lamsade.dauphine.fr/~cazenave/papers/MenifCGW2014.pdf). Fluid
  HTN (https://github.com/ptrefall/fluid-hierarchical-task-network) replans only on completion, failure or a world change,
  rejects a replan that cannot beat the current plan, supports partial plans and re-checks executing conditions each
  tick. HTN methods = `AUTOPILOT_UX.md` 3.2's composite goals.
- **Utility AI** (Guild Wars 2, https://www.gdcvault.com/play/1021848/; Dragon Age Inquisition, GAP3 ch. 31): stateless by
  re-scoring each tick; needs a commitment bonus / hysteresis.
- **Behaviour trees**: reactive by re-evaluation, but a "running" node holds hidden state - the resume problem.
- **Teleo-reactive programs** (Nilsson 1994: https://www.jair.org/index.php/jair/article/view/10112): the first true
  condition wins, scanned continuously - resumes from any state.
- Fit: the hybrid shipped games use - goals as data in the queue, a teleo-reactive arbiter and runners, HTN-style
  decomposition at activation and on discrepancy, utility with hysteresis for the auto planner's generators.

### 2. Re-orientation and resume
- **Goal-Driven Autonomy** (Weber, Mateas, Jhala; EISBot won 73 % against StarCraft's built-in AI:
  https://ojs.aaai.org/index.php/AIIDE/article/download/12401/12260; the model:
  https://ojs.aaai.org/index.php/AIIDE/article/download/12605/12453): expectations per plan step, discrepancy detection,
  explanation, goal formulation and management.
- Voyager (https://voyager.minedojo.org/) judges success with an LLM critic; FLvibed's tests from game state are strictly
  better - never replace them with a model's judgement.
- The assessor's sources on hand [file]: the client objective list (`exe::ClientObjective`, host API v25 `ObjectiveReread`,
  checked in `modules\controller\src\api.h`), the marks (re-patched after a load), the waypoint list, the system, the dock state,
  contacts, the visit record.

### 3. Natural language to commands
- Proven: **VoiceAttack + EDDI** for Elite Dangerous - phrases map to macros over a keybind layer, game events fire
  commands without speech; deterministic, local, grammar-based
  (https://forums.frontier.co.uk/goto/post?id=4864958, https://github.com/ArNeo-VR/EDVA).
- LLM agents: Mindcraft's typed `!command(args)`, its code-writing mode off by default as "vulnerable to injection"
  (https://github.com/mindcraft-bots/mindcraft); Convai's typed actions with enum parameters; every path must signal
  completion or the NPC stalls (https://convai.com/blog/how-to-make-ai-npcs-act-on-your-commands-in-unreal-engine-5-with-convai)
  - middleware and demos, no shipped game found. Voyager needs GPT-4 and code execution - unsuitable.
- SayCan (https://proceedings.mlr.press/v205/ichter23a.html): the model proposes, an affordance score filters - errors
  nearly halved; FLvibed's filter is the validator over game state.
- Constrained decoding (llama.cpp GBNF and its JSON-Schema converter, Outlines, API strict tool use) guarantees syntax,
  not semantics. Out of process, never in multiplayer chat (as `llm-npcs.md`).
- **Minimum goal schema now** (A38): `id`, `version`, `type` (closed enum), `state`, `origin`, `parent` / `step`,
  `depends_on`; `subject` = a reference `{kind: base|solar|ship|label|system|zone|pos|waypoint|objective, nickname /
  CreateID, system, pos}` plus its resolution status; typed `params` with units; `when` / `done` / `fail` as condition
  expressions (not code) in one small language mirroring the game's `Cnd_*` (distance, base / system entered, destroyed
  count, health, lane, cargo); `policy` (engagement, route, knowledge scope, allowed side effects); `confirm`, `repeat`;
  `source_text` + span (NL); `display` rebuilt from the game's own strings (strid) for the read-back confirmation.

### 4. The game's goal vocabulary (local HDE scripts: 42 `Cnd_` types, ~90 `Act_` types; `Cnd_CommComplete` 607 uses,
`Cnd_Timer` 381; meanings in `research\repos\KnowledgeBase\docs\ini-editing\hardcoded-inis\data\missions\missions.ini.md`,
https://codeberg.org/TheStarport/KnowledgeBase; the job tree in `RANDOMMISSIONS\vignetteparams.ini`)
| Game source | Goal type | Done / fail source |
|---|---|---|
| `Cnd_DistVec` / `DistCircle` / `LocEnter`, `DistShip` inside; NNObjective `navmarker` | Fly to | the script's distance; else 150 m |
| `Cnd_BaseEnter`; NNObjective `rep_inst` on a base | Dock / travel to base | base id; `nnv_dock_denied_*` |
| `Cnd_TLEntered` / `TLExited` / `InTradelane` | Trade lane | lane state; `LaneOpen` |
| `Cnd_SystemEnter` / `SpaceEnter` / `JumpInComplete` | Jump / travel to system | system id |
| `Cnd_Destroyed` (label, count) | Attack / clear area (label set) | kill count |
| `Cnd_HealthDec`, `Cnd_ProjHit*` | Attack until hit / health, or a defend constraint | health / hit |
| `Cnd_PlayerManeuver` GOTO / DOCK / FORMATION | Fly to / dock / formation | maneuver state |
| `Cnd_LootAcquired`; jobs `Tractor_in_loot` / `return_loot` | Loot (+ travel back) | cargo; job success type 3 |
| `Cnd_CargoScanned` (one use) | Scan (new) | scan |
| `Cnd_CommComplete` / `Timer` / `True` / `WatchTrigger` / `RTCDone` | Wait (Held-like, face the scene) | the next objective |
| `Cnd_MsnResponse` / `CharSelect` / `PopUpDialog` / `LaunchComplete` | Base action, player-gated | room / launch |
| `Cnd_DistShip` outside -> FAIL, `TetherBroke`, `Act_NagDistTowards` / `Leaving` | constraints, not goals | `mission::Leashes` |
| Jobs (`DestroyMission` only): `mb_kill_ships`, `kill_big_solar` / `kill_other_solars`, `Assassinate_ship` / `_solar`, `Destroy_loot`, `guide_to_*`, `*_dont_kill_ships` | Attack / clear / fly to / constraint | objective R / N values; message types 3 / 2 |
| Free roam: trade, repair, explore | Trade run / repair / fly to | cargo, credits, hull |

### 5. The toggle fuzz test (A42)
- The harness toggles through the native `ap` command (no input emulation) at seeded times (the seed logged); about
  half land on edges: lane entry, mid-jump, the dock queue, cutscenes / Held, ±0.5 s around an objective change, just
  after a load, mid-fight. Off periods 0, 1 frame, 0.5 s, 5 s, 30 s, and "perturbed" (the game's own goto moves the ship
  first).
- Oracles: the mission succeeds; time ≤ baseline + a bounded cost per toggle; **cold / warm equivalence** (the decision
  after a toggle equals a fresh assessor's on the same snapshot); no forbidden act (a leashed label attacked, a leash
  broken, a story action invented); no duplicate side effects; no decide-loop guard hits. Failures replayed and shrunk
  by delta-debugging the toggle list. Plus mid-mission save loads (the 21660 case).
- A host-free tier in `tests\`: recorded snapshots through the assessor as a pure function, property tests for
  idempotence and equivalence.

### Risks and open questions (probes)
1. The live trigger table in Content.dll (A39) - the top probe. 2. A running job's state (vignette node, critical loot)
at run time. 3. Space-id reuse after a load (unverified, `FREELANCER_RE.md` ~1862) and the `N` values of re-read client
objectives (~1909). 4. Multiplayer: scripts run on the server, the assessor degrades to what the client sees. 5. LLM
command parsing is demo-grade; plan grammar-first, LLM optional, the validator as the safety.
