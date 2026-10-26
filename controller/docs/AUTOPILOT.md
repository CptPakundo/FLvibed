# Advanced Autopilot: state, rules, settings, numbers

The autopilot as built: what it does, the user's rules behind each behaviour, every setting, the measurements and the
audits. Moved here from `..\CLAUDE.md` on 21 Oct 2026 (the audit's CLAUDE.md diet) and kept current from here on:
**a new behaviour, setting or verified number goes into this file in the same commit**, not into CLAUDE.md. The
product design (goal model, engagement policy, UI, phases) is `AUTOPILOT_UX.md`; the HUD cues' design is `HUD_CUES.md`;
the user's combat rules are in memory player-combat-priorities; the game facts in `..\..\docs\kb\` and
`..\..\docs\FREELANCER_RE.md`. Code (one file per section since 21 Oct 2026; shared types, globals and prototypes in
`..\src\impl\autopilot_impl.h`): `autopilot.cpp` (core: modes, settings and personality, the state, start / stop),
`autopilot_objective.cpp`, `autopilot_behaviours.cpp`, `autopilot_target.cpp` (target choice, situation, odds, missile
economy and geometry), `autopilot_combat.cpp`, `autopilot_dock.cpp` (fast dock, queue shortcut), `autopilot_travel.cpp`,
`autopilot_frame.cpp` (Update), `autopilot_settings.cpp` (ini, status, the `ap` command); `..\..\core\game\ai.*`,
`space.*`, `route.*`, `avoid.*`, `flight.*`.

Priority (user, 21 Oct 2026): capability first (speedrun-grade); the player UI of toggles and presets comes later, so
every behaviour gets its own `ap set` setting with a sensible default and stays separable (memory autopilot-ui-later).

## Speedrun flight, sessions 7-9 (22-24 Oct 2026; M12-driven, shared code - verdicts `..\..\docs\LEG_DOSSIER.md` M12)
As built (commit in brackets; game facts in `..\..\docs\missions\M12.md` and `FREELANCER_RE.md`). **Most of these have no
`ap set` switch yet** - a breach of the one-setting-per-behaviour rule above, listed as pre-testing work in the handoff.
- **Cruise lock** (338b093; user's hard rule: no cruise drop from the M12 core entrance to the Omicron Minor hole): from
  an airlock transit to the next jump / dock the cruise is kept, steering only (`CruiseLock`, after the guard). It
  gives way for one airlock run that passed beside its door (59a015d) and during the charge fallback below.
- **Charge fallbacks:** a charge not done in 10 s -> 12 s afterburner (1ef7ea1); a charge reset under fire (elapsed falls
  back, attackers > 0) -> 8 s afterburner at once (4e6e6ef); the charge starts at once after a jump-in (5dddece).
  Measured: the charge always runs at engine speed (80 m/s); engine kill does not carry speed into it (FREELANCER_RE).
- **Airlock runs** (`autopilot_dock.cpp`): no waypoint-list wait under the lock; the guard leaves a run lined up on the
  door's axis (e1c7016); the door's line read from its dock bay when the trip's sphere lies beside an airlock and the
  trip staged 50 m out, level with the mouth (`AirlockStage`, 60e4f90, 3524a65); level with the mouth the run curves in
  from 130 m off the axis, as the user's try 3 (f3106c8); charge 0.5 s before the transit (4c133b7).
- **Passages** (`PassageVia`, d0df954, 3b6417a, 79ead46): a line clear for a ray but not for the ship + 20 m (the M12
  core's static plate with a ~20 m hole) -> a passage 50-90 m to the side, tested 80 m past the obstacle; aimed at
  until 120 m before it, then a line parallel to the way.
- **Wheels** (`WheelTiming` in ArriveAt; `avoid::WheelScan`): a rotating part's gap timed; the crossing ring = the
  smallest ring at least half free (0a40dba).
- **Way planning:** a failed plan retried from 60 m off the nearest solid (d1a0705); ARC planner round a structure
  (6ad18fa); the guard leaves a planned way while the motion is clear beyond the stop + 0.3 s (4852aeb, untested).
- **Holds / pursuit:** the static hold's gap >= the launch rule's blast need + 6 m, pushed back with 8 m overshoot
  (223e12d, b011598); a close static hull counts as on-line for launches (756ea21); the pursuit brakes for a big target
  by the flight model's stop x 1.5 + 40 m (05f6219).
- **Docks under fire:** a lined-up run closing > 100 m/s keeps its speed to the normal request point (bf7dc5d).
- **Session 10 (24 Oct 2026; every switch off by default until the leg gate; verdicts `..\..\docs\LEG_DOSSIER.md` M12):**
  kept - `ring_request` (a planet ring requested at p2 at full speed; a pass beyond p2 requests at once - probe: the
  landing 0.03 s after the request, `..\..\docs\kb\navigation-and-hazards.md`), `airlock_charge_at` (the share of the charge
  done at an airlock's transit; the user: 0.5), `wait_flies` (a trip waiting for the waypoint list flies on, burner within
  45 deg), `airlock_burn` (the way in only: slow only as the turn left needs, the burner through the turn-in; never under
  the cruise lock), `tractor_exit_run` (loot tractored in passing under the cruise lock: on to the trip's airlock at once),
  `hop_cruise_hold` (a hop's charge timed by its own clock - in some hops the engine's cruise flag read 0 4 s into a
  charge at 79 m/s, elapsed -5; in others engaged=1 with the elapsed time: not verified which is the game's rule - and kept
  unless the burner wins by > 2 s: the flip-flop fixed), `hop_reset_burn` (a hop's charge reset under fire burns 8 s),
  `passage_window` (a passage seeks the opening's centre first), `tractor_exit_range` (600 m: the early exit run only that
  close to the door); set B (ring_request 10, airlock_charge_at 0.5, rejoin_nearest 1) is the best M12 config so far,
  set K (all kept) 6 / 6 but slower; experimental, off - `hug_route` (v2: hoop posts, windows - not winning yet),
  `hop_narrow`, `arc_bulge`, `wheel_parallel`, `travel_passage`, `noway_climb` (untested where it applies). Verdicts:
  `..\..\docs\LEG_DOSSIER.md` M12 "session 10".
- **Session 11 (24 Oct 2026; all off; verdicts `..\..\docs\LEG_DOSSIER.md` M12 "session 11"):** `leave_on_kill` (Doomed: a
  static target our guided munitions in flight within 400 m carry >= 1.2x its hull left is dead in effect - the hold
  breaks off, the combat's next pick or the objective's next step; under it the finishing torpedo goes too; DoomedWatch
  logs the left target's kill as `target-destroyed ... (doomed ...)` - right 30 of 30, kill ~0.9 s later; no measurable
  segment gain), `static_hold_hullgap` (the slide keeps the hull gap by a ray each step: worse at M12 gen 3),
  `static_hold_mine_gap` (mines only within this hull gap, default 40 = before; at 60 mines drop, no gain), `charge_cm`
  (CMs every 0.8 s while the cruise charges when a ship seen firing a cruise disruptor at us - learned, 2 min - is within
  3.2 km), `jumpin_burn` (the first 8 s after a jump-in or a trip's start with an attacker within 3 km on the afterburner,
  never once a charge runs; v3: any system entry not made through an airlock counts as the jump-in, hostile ships
  count, a charge in its first 1.5 s is cut), `passage_axis` (a passage on a trip into an airlock door takes the clear
  opening nearest the door's axis; the last door line read is kept across trips), **`ref_line`** (kept: a reference
  flight's line through a structure followed by pure pursuit, 1.2 s of speed ahead, under the cruise lock within 250 m
  of the line - `RefLineAim`, speedrun knowledge, source: the user's M12 try 3 through the core; its part before the
  hub rotated about the exit door's axis to this run's turbine crossing, ray-tested and stepped back toward the
  reference's azimuth when blocked; past its end on along the door's axis), **`stale_destroy_wait`** (kept: a destroy
  goal only the last objective stands in for, or one that would replace a step's own trip, none of whose objects the
  mission marks, is a later step's - no trip; never one that ends the mission (leads to null / SUCCEED: M13's city);
  M09 4 / 5 PASS, PB 724, was 0 in five gates), `towards_nag` (a fight objective after a towards-nag objective is
  fought on the way: M09 failed "fled the battle" - off), `closed_lane_ahead` (a live flying script goal instead of
  waiting at a lane closed both ways: M07 worse, a later step's goal - off), `dock_regain` (a granted fast dock whose
  distance keeps growing 4 s re-approaches: M06 2 / 2 without firing; v2 also from the travel's dock step, sampled each
  second; session 12 below), `leash_follow` (every script goal beyond a leash and nothing else to do: follow the
  leash's ship instead of resting - M10's stall inside Von Claussen's leash; off - session 12 found that stall was the
  Holstein lock, not the leash). Speedrun preset (`ap preset speedrun`, applied by flleg.ps1 at every leg):
  ring_request 10, airlock_charge_at 0.5, rejoin_nearest 1, ref_line 1. Events: `hold-mine`, `doomed`, `cm-charge`,
  `travel jump-in burn`, `ref-line`; salvo-check logs hold-gap / torp-need / to-point.
- **Session 12 (24 Oct 2026; verdicts `..\..\docs\LEG_DOSSIER.md` M10 / M09 / M04 "session 12"):**
  **`blast_flank`** (on, general safety: no missile / torpedo launch while another solid lies within the round's blast
  reach - blast + our radius + 10 m - of the ship: rays right / left / up / down and 45 deg forward of them, the target
  skipped; event `launch-held ... beside us`; M09 s11o9-i2 died at full hull to its own torpedo bursting on the
  generator's rock ~15 m beside it, frames `end_sheet.jpg`; M09 s12a 2 / 2 PASS, it never held a launch; NOT yet A/B'd on
  M12 / M13, where it may hold salvos in the trenches - set s12e, planned: M12 arms on / off x3, M13 x2).
  **`pre_aggro`** (off = 0, a range in m, 2500 tested; speedrun knowledge, source: the mission script - IDEAS A6): the
  mission ships the step's script is about to turn hostile to the player - `mission::PendingHostileShips`:
  Act_SetVibeLbltoShip / Act_SetVibe ... REP_HOSTILE_* up to 4 hops (breadth first) from the step's setter and the
  trigger that armed it, not past the next objective, another system's trigger passing the chain on - found among the
  game's spawns by their spawn position (host API v17 `SpawnedShips`; the harness's Get-FLMissionShips rule: [MsnShip] /
  [MsnFormation] / spawn-action position within 2 m, a formation's next spawns within 1 km in order; exact ids, never a
  whole faction); neutral within the range they read attitude -0.65 (`space::SetPreHostile`, `Contact::preHostile`);
  the nearest is fought at once whatever the step (ObjectiveDecide "pre-aggro", not on dock / lane steps; it interrupts
  a travel objective, every 3 s at most); event `pre-aggro text= spawns= ships=`. M04 s12c / s12d: all 8 bounty hunters
  flagged, fought ~18 s before the Destroy banner, 9 / 9 PASS; mactan 135-139 s (was 140-171) - a few seconds: the
  rest is the trip to Mactan the runners make before the hunters come (dossier M04). `dock_regain` (on in sets
  s12b-s12d, still off by default): losing ground counts only at a crawl < 40 m/s (a queued gate's grant was dropped
  while the game's dock flew to the entry point), and a grant followed by 4 s of crawling hands over to the game's own
  dock maneuver at once, once per grant, event `grant not followed` (M04 Benford: granted at 24 m / 285 m/s, bump,
  drift; s12b / s12c rescued Benford stalls, one at 59.6 s, over the 58 s limit; the handover not met yet in s12d).
  Candidates for the speedrun preset after the gate: pre_aggro 2500, dock_regain 1.
  Review fixes (fresh-eyes pass, session 12 end; UNTESTED in game): the host's spawn list spans the process - pre_aggro
  maps only spawns after the newest one at the last menu (`NoteMenu`); a placeholder objective flags nothing; flagged
  ships no longer count in ObjectiveThreat (their own 3 s interrupt); no pre-aggro on escort steps, and with an armed
  ship leash only ships within 75 % of it from the anchor; blast_flank's rays run only for a round about to go (missile
  wanted and refired, torpedo branch) - solar targets only (a ship target keeps the centre-distance rule); the grant
  handover once per fast request; the "no way" dock retry at most 15 times a step; event throttles survive the mode
  restarts' g.time reset (minefield-damage included); the lethal guard never acts against the game's own dock / goto /
  lane / jump and overrides the cruise lock near a death zone (only planets and suns are lethal spheres; closing-speed
  test, not the closest approach - a skirting path may trigger it). Open doubts: the formation rule of MapSpawns takes
  consecutive spawns (other spawns in between are mis-assigned, as in the harness); PendingHostileShips reads only
  Act_SetVibe / Act_SetVibeLbltoShip with Player second; whether the game reuses space ids after a load is unverified.
- **24 Oct 2026 evening (the checkup session; sets s13a / s13b, dossier M04):** the swallowed-step fix (cc88517 / 2b3fabd)
  verified: a placeholder step with hostile marks is their fight (M04.hole 70.7 -> 50 s). Its Combat chase cost
  M04.mactan +17 s -> **while an escort is active (or the survival stance has a lead) the placeholder fight keeps the
  escort and fights the marks as its threats** (34ba88f; s13b 5/6 PASS, PB 974, mactan 133). Open from s13b: Mactan's dock
  stays script-locked until Juni is within 3 km; the dock-retry rest at the base let the mission's nag fail the run once
  in 6 - wait with the ship whose arrival unlocks the dock. The code itself was restructured the same day without a
  behaviour change (`..\..\..\CHECKUP_2026-10-24.md` 9.2: the registry, `autopilot_util.h`, `_geo` / `_hold`, the enums,
  `State g` in groups, `autopilot::Contacts()` with the pre-aggro overlay) - verified by `tools\apdiff.py` (9.3).
- **Lethal guard** (session 12, every mode, no switch): closing on a planet / sun death zone (`space::LethalZones`) so
  that the flight reaches its edge within 3 s (+200 m) -> steer out along the tangent, no cruise / afterburner; skipped
  while the mode's own aim lies outside the sphere and short of its edge (planet-ring fast docks). Event `lethal-guard`.
  s12b M04 i3: the survive mode's evasion flew into Manhattan's death sphere at 200 m/s, dead at full hull.
- **Diagnostics:** `autopilot bump` (speed lost 40 m/s in 0.2 s, not braking: the nearest objects); `autopilot
  minefield-damage` (hull lost in a mine-danger field, a mine within 150 m, the shield down or falling - a leg FAIL,
  `docs\TESTING.md`; hull lost at a steady shield is a zone's own damage, M10's `Zone_Rh04_minefield` 200/s; a one-shot
  mine death never logs it); a dock trip to the step's base that finds "no way" (a script-locked base: M10 Holstein
  until comm 1370) rests and asks again every 4 s, like a refused dock; the motion trace's core-line tool `research\tools\core_line.py`.
- Reverted (do not rebuild): early dock requests while cruising (the game's dock drops the cruise to 80); the fly-through
  centre over an airlock (no cruise to the lair); the rejoin's speed margin (hub circling); "far spokes" wheel timing.
- **Removed 24 Oct 2026 (the checkup, `..\..\..\CHECKUP_2026-10-24.md`; the user: a switch that lost is deleted, its
  verdict stays here and in the dossier; the defaults' behaviour is unchanged):** `hug_route`, `arc_bulge`, `hop_narrow`,
  `wheel_parallel`, `travel_passage`, `passage_axis` (the session-10/11 planner and passage variants, none winning),
  `towards_nag` (M09 failed), `closed_lane_ahead` (M07 worse), `leash_follow` (the stall was the Holstein lock),
  `static_hold_hullgap` (worse at M12 gen 3), `leave_on_kill` with `Doomed` / `DoomedWatch` and the `doomed` events (no
  measurable gain), `charge_cm` and `jumpin_burn` (no gain at Omicron Minor), `cruise_burn` (measured: no gain),
  `fire_all` (an A/B arm), `ring_lineup` (the straight-in request is the only one left; `ring_request` tunes it). The
  narrative above keeps their history. Still in code, off, awaiting a verdict: `hop_cruise_hold`, `hop_reset_burn`,
  `passage_window`, `airlock_burn`, `wait_flies`, `tractor_exit_run` / `_range` (session 10's set K: 6 / 6 but slower),
  `noway_climb` (planned: gen 3 -> core entry), `dock_regain`, `pre_aggro` (preset candidates), `extend` + 4 knobs
  (the 17 Oct benchmark loser; its stats feed flbench / flfights).

## Advanced Autopilot (in progress since 17-18 Oct 2026)
User goal: full harness abilities + the player ship with full NPC-AI abilities, the player deciding the specifics.
- `..\..\core\game\ai.*`: the game's AI on the player ship (maneuver dispatcher, server / client directives, pilot
  personalities from the game's [Pilot] files with every field settable, health, mission-lock check). Debug
  `ai state|man|dir|pilots|pilot|copy|field|fields|set|brain|cbrain|release|target|free|layout`. Facts:
  `..\..\docs\FREELANCER_RE.md` "AI on the player ship" (NPC brain does not apply; AI never fires; Evade needs the host's guard: host API v6
  `GuardEvade` patches the Evade behaviour's update so a missing threat finishes it as for NPCs instead of crashing
  Common.dll; DrasticEvade = timed slide / reverse, `ai dir drasticevade off=x,y,0 time= mode=0|1 client=1`).
  For any ship (spawned NPCs): `ai::GiveBrain`, `SubmitFor`, `LoadPilotOnto` (pilot fields over a live NPC's
  personality), `ApplyBlock` (a job block); `ai rawdiff <npc> <pilot>` diffs personalities word by word. [Pilot]
  angles are degrees, the game's Personality radians: LoadPilot converts since 18 Oct 2026 (was wrong before).
- User principles for it (17 Oct 2026): the harness is agentic, the autopilot must be **predictive**: as capable as
  the best behaviour the harness has shown, with no agent in the loop; fast, responsive, effective, good for short-
  and long-term player goals; robust and general (ship, loadout, system level, factions from the game's data); no
  hard-coded key presses where the game has native logic; everything works with the game out of focus.
- `..\src\impl\autopilot.*`: modes goto, dock, follow, formation, face, escort, combat, hold, travel, objective. Combat:
  - **Target choice** (PickTarget, twice a second, from the game's data): hostile ships (attitude), attackers of us
    or the escorted ship first, fighters, damaged hulls, distance, a bonus for the current one; big ships last unless
    `big_ships`; a target the player chose is kept until destroyed; chase limit (out of range 8 s: ignored 30 s; a
    flank run to a big ship's side resets that clock - 21 Oct 2026, gunboats were given up 8 s into the run).
  - **Steering:** lead pursuit (controller `Pursue`, shared with the bot: nose and reticle on the lead cross, binary
    throttle, reverse when close or turning hard, afterburner to close in) or the game's Trail (`steer = 1`); between
    targets it flies towards the next one (never coasts). **Ship-adaptive duel** (21 Oct 2026, `adaptive`, `duel`):
    close turn / wide turn / slash by the turn-rate ratio (`space::ShipMotionOf`) and the fight's measured exchange
    (duel A/B: close 3.35 / wide 2.52 / slash 1.38 exchange -> priors; `..\..\docs\kb\combat-targets.md` "Duel tactics").
    The turn onto a target far off the nose while attacked is a dodge (`turn_evade`: slide bursts every 0.7 s,
    afterburner, reverse when the target is behind within gun range; from 60 deg until 25, at most 4 s).
  - **Evasion, reactive and blended** (user: no hits = no evasion; more damage = more evasion; never at the expense of
    tracking): pressure = shield + hull points lost in 2 s / total (6 % = full), fades over ~2 s; the pursuit weaves
    (corkscrew around the lead cross, reticle and gimbaled guns stay on it, radius up to `weave_radius` 0.75 of the
    fire cone), slide
    bursts above 0.5 (all four directions; HDE's keymap has SLIDE_EVADE_UP / DOWN, unbound), break-away goto +
    afterburner on heavy damage (personality trigger). `dodge = 2`: the game's Evade instead (NPC style; it turned the
    ship away for 2-3 s each time: the "lost tracking" the user saw). Flee below the hull threshold or outmatched.
  - **Weapons:** one fire gate (`fire_cone` 0.5, a 0.15 s hold), continuous unless `bursts = 1`; staggered fire
    (below); missiles by the launch planner (the "Missiles" item below); the gimbal aim while the game flies (Evade,
    break, Trail).
  - **Missile dodge** (no CM possible: none fitted - `space::PlayerCountermeasures` -1 on the test Starflier -, no
    ammo, or 3 drops failed): `dodge_window` (0.6 s) before impact slide hard to one side + afterburner, the missile
    overshoots. b5 (16 runs vs 747-damage police missiles): 0.6 s 0 deaths / 3.78 kills/min / 0.20 lost per kill; no
    dodge 1 death / 3.01 / 0.23. Outcome statistics incoming-missile-hits / -misses for every homing munition aimed
    at us (verdict 0.3 s after it vanishes).
  - **Scenario caveat (user, 18 Oct 2026):** every number above is one scenario - the starter Starflier vs Liberty
    police swarms at Manhattan, stacked against the player. Extend off / stagger on / weave 0.75 / dodge 0.6 s are
    tuned for it only; judge with the odds of success in mind and re-test across a scenario matrix (ships /
    loadouts, enemy types and counts, area levels) - the experience store learns per context for this reason.
  - **Benchmarks:** random police encounters vary a lot (1-9 kills); compare variants with repeats or per-kill A/B
    inside each fight (settings alternate at every kill: paired samples on the same enemies; `..\tools\flbench.ps1`:
    Enter-FLStream, Invoke-FLBenchRun, Invoke-FLABRun / Invoke-FLABBench; results in `..\..\research\bench`). Ways to shorten them (not built): stop a comparison once the difference is clearly beyond the noise; cap runs at 60-90 s (per-minute metrics); fixed spawned scenarios. Enemy supply is the limit (~2 kills/min: one patrol per reload, sparse waves after; a
    Rogue zone 6 km above SaveC702 and staying at Manhattan were both ~20 % engaged). **Fixed scenarios since 18 Oct
    2026:** spawned enemies fight (`..\..\core\game\spawn.*`, debug `spawn <npcshiparch> <faction|player> [count] [dist]
    [job= pilot= order= base= force= level=] | list | hunt | clear | clearat <hull>`; the job's scene toughness was the block,
    `..\..\docs\FREELANCER_RE.md` "Spawning ships"); `..\tools\flbench.ps1` `Enter-FLScenario <name>` (SaveC702 fresh, police
    friendly, the scenario's spawns: rogues-2, rogues-3, police-4, elite-1, mixed-3), `Invoke-FLBenchRun
    -UntilSpawnsDead` (ends when our spawns are gone; natural encounters that wander in count as Extra),
    `Invoke-FLScenarioAB` (whole fights A / B alternating, paired; stops once Lost or Secs differ with |t| >= 2.5
    after 3 pairs, else at 8). Other player ships: the campaign saves (`..\..\docs\missions\SAVES.md`). **Fight matrix**
    (`..\tools\flfights.ps1`, sweep section `fight-matrix`, since 19-20 Oct 2026): 15 enemy kinds on the starter ship
    (SaveC705) and a strong one (SaveC706); FAIL on a retreat from a winnable kind, a hopeless kind not left, or a spawn
    failure; 30 / 32 won on 21 Oct 2026 (heavy_d12_x4 / _far are hopeless for the starter ship; heavy_d12_x2 varies).
  - **Countermeasures** (`countermeasures` 1, every mode; only with a dropper and ammo -
    `space::PlayerCountermeasures`, droppers counted by their ammo since 20 Oct 2026 (the equip "active" flag reads 0
    for droppers and launchers: before, a dropper of 28 read as none and every missile was only dodged); max 3 per
    munition): only against homing munitions aimed at us (missiles,
    torpedoes, cruise disruptors; mines not verified; `space::Missiles`, debug `missiles`: CGuided objects, target,
    owner, cruise_disruptor, homing, damage incl. the explosion, blast radius, closing speed), once inside
    `cm_distance` (700 m; a CM diverts within 1000 m at 70 %), again every 0.35 s while one still homes on us; cruise
    disruptors outside a fight (two drops: 1 - 0.3^2 = ~91 % diverted); in a fight only when the predicted damage of what homes on us reaches
    `cm_shield_fraction` (0.5) of the shield left, or 5 % of the hull with the shield down (user: a hit is not
    certain, small ones are not worth a CM); 2 = every homing munition. Was: on a timer after any damage.
  - **Repairs** (`repair` 1, damage model since 19 Oct 2026: measured loss rate vs the attackers' gun potential x the
    hit rate learned in the fight, homing munitions due, the gun rounds in flight with the game's shield formula, over
    2.5 s and a 0.75 s reaction window): **hull is sacred** (user, 20 Oct 2026: the player hates hull damage, shields are
    the expendable pool) - a battery when the expected damage would take the shield below its offline threshold
    (before it fails: a failed shield reboots and absorbs nothing), or when it is failed / near depletion and >= 2 % of
    the hull would get through; a failed / failing shield is never held back by the whole-unit rounding (it waited for
    death-soon at hull 0.29). Nanobots when the model's hull loss over the horizon >= 15 %, the hull would end below
    20 %, or the hull is below 40 %. `repair` 2 = the personality's thresholds. **In every mode** (DamageAndRepairs:
    travel, hold, follow, dock, a rejoining escort - 20 Oct 2026: M04 died in `hold` beside a hostile cruiser with a
    nanobot and a battery aboard; the repairs ran only inside a fight). Batteries at the last useful moment (the
    reaction window, not the 2.5 s horizon; 20 Oct 2026). **Units by count** (21 Oct 2026, user: "one battery per press,
    scale it by incoming fire, predicted hull damage"): the game's `IObjAffect::use_item` with our amount
    (`exe::UseItem`, `ai::UseConsumable`, event repair-units), not the fill-the-bar button.
  - **Extend** (`extend`, default 0 - b4 at Manhattan, 16 runs: kills/min 2.47 vs 4.10, on target 58 vs 80 %, no less
    damage per kill; needs an odds-aware trigger; user + playlist study: under fire players afterburn away and take the attacker off screen,
    ~6 s, resume ~3 s after the hits stop): on evasion >= 0.6 for 0.8 s or shield < 35 % while hit, afterburner away
    from the threat centre with a jink (`extend_jink` 40 deg, new every 0.7 s) and slide bursts, guns firing when the
    target crosses the cone; resume after `extend_quiet` 1.5 s without hits (min 2 s, max `extend_max` 8 s); futile
    (still hit after `extend_patience` 3 s: surrounded) = fight instead, 6 s cooldown. Under the survival stance extend
    runs only while the race is lost.
  - **Staggered fire** (`stagger` 1, user's trick, any loadout): each burst starts the N active non-homing guns one by
    one at k*r/N (r = slowest refire), by each gun's own FIRE_WEAPON slot - the game's HUD weapon list
    (`Weapon::fireSlot`; the list index tapped a Javelin's slot until 18 Oct 2026) - then holds every gun's slot (not
    fire-all, which launches missiles too); verified 0.060 s phase on 2 x 0.12 s guns. Default on (user: benefit of the
    doubt - may raise amortized DPS by spreading projectiles). Theory: sustained DPS is refire-limited (2 x
    li_gun01_mark01 use 84.8 energy/s < ge_fighter_power01's 95/s), power-limited loadouts are set by the charge rate.
    Measured so far inconclusive: b4 dealt per firing second 85 vs 82; per-kill A/B 12 kills 41 vs 56 dealt/s.
  - **Run statistics** (status / stop event): hit-time, evade-time / -nohit / -on-target, lost (ships' worth), dealt
    (hull points taken off targets), dps (dealt per firing second), extends, extend-time, incoming-missile-hits /
    -misses, gun-shots / gun-duty (shots / the shots possible while firing was wanted) / gun-accuracy, own-missile-hits
    / -misses (event missile-outcome per launch: planned or not, flight, closest approach; missile kills were counted
    as misses until 21 Oct 2026 - the real hit rate is ~84-100 %), turns / turn-time.
    Events: target / target-destroyed / given-up, repair, cm, dodge-missile, missile-hit / -missed, extend / -end,
    flee, retreat phase=..., loot, tactic=...; `autopilot tactic-loop` = more than 8 tactic changes in 1 s (a bug: the per-change events
    are muted until it calms down; the seek/loot flip-flop of 17-18 Oct was such a bug, fixed 0ada145). A destroyed
    player ship stops the run first (`why=player died`; before 18 Oct the death frame counted a phantom kill and
    reported `fled`). The fight is assessed twice a second whatever the target rules and damage is tracked every frame
    (`lost` includes damage taken between targets and while fleeing since 18 Oct: older `lost` numbers read low).
    Big-hit evidence event (a loss > 20 % of shield + hull in one frame names the nearest hostiles). Per-second `self`
    events (hull, shield, nanobots, batteries, CMs) are the runs' track for `research\tools\section_cost.py`.
  - **Audit 18 Oct 2026 (bugs fixed, verified out of focus, dev + release):** stale fight reading with a chosen target
    (flee never ended), flee goto issued once (sat among hostiles after 8 km), stale "being hit" between targets
    (repairs), escort's stale no-enemy time (back to Rejoin at once), engage 0 ended an escort, CMs / dodge timed on
    the first listed munition instead of the nearest, follow never noticed a dead leader while the game's follow ran,
    held commands (slide up / down) left held after a hand-back to pad flight, flbench A/B dropping the last segment.
    Core audit the same day (`..\..\core\game\space|ai|spawn`): missile closing speed timed by QueryPerformanceCounter
    and kept between samples (GetTickCount's ~16 ms steps read 0 for several frames in every 50 ms: dodge dropouts);
    the harness waypoint save belongs to its flight (a failed goto left it armed for the next flight / save); spawns
    are tracked by id + their own reputation (ids outlived a load and are reused: `spawn clear` could destroy another
    ship); guarded GetRep / SetAttitude / GetLocation in spawn and the `ai layout` personality read.
    Open: the flee stops 3 km from hostiles, but escort missions fail at 3-5 km from the escorted ship
    (`..\..\docs\missions` M01a/M01b/M03/M06 nags) - the engagement policy must keep a mission's distance rules.
  - **Tuning (17 Oct 2026, 43 runs, no godmode, police hostile, SaveC702):** lost per kill / kills per min: weave
    0.75 0.13 / 3.38 (default now), 0.6 0.20 / 3.31, evasion off 0.33 / 3.22 (3 deaths in 7), dodge 2 0.82 / 2.12
    (2 deaths in 3); evade_scale 6 % beats 4 % and 10 %; slides 0.5 s kept. Throttle profile: autopilot afterburner
    42 % / full fwd 37 % / reverse 9 %; the user's playlist 29 / 42 / 4 %.
  - **Combat study of the user's play** (playlist "Freelancer: A 2020 playthrough in 4K", downloaded video-only to
    `..\..\research\videos`; `..\..\tools\vidhud.js` bracket / hostile red / shield-hull-power bars, `vidspeed.js` speed
    readout by trained digit templates (0 wrong in 240 checked frames), `vidstudy.js` fights + throttle / aim / damage
    phases, `vidsheet.ps1` contact sheets; M13 excluded: uniquely scripted). The playlist and many PvP clips have no commentary, so transcripts cannot find the fights: the HUD readers do. CPU decoding ran ~20x real time on 17 Oct; since 20 Oct the streaming OCR scan (`..\..\tools\ocr\flocr.py`, `vidmissiles.py`, NVDEC crop) runs 67x at 2 fps. Pooled, 15 missions, 7103 s of fights,
    198 hot spells: full fwd 42 %, afterburner 29 %, cruise 20 %, reverse 4 % (12-23 % only in M01 vs fighters, mostly
    while the target is off the nose), target on screen ~53 %, centred ~10 %. Under fire (hp falling) vs cool:
    afterburner 39 vs 22 %, reverse 7 vs 3 %, target on screen 48 vs 57 %; hot spell median 6.1 s, the attack resumes
    3.1 s after the hits stop, the attacker goes off screen ~always (the extend pattern). Per mission: M02 afterburner
    33 % (bursts ~1.3 s); M08 weapon platforms fwd + afterburner, no reverse; M13 afterburner approach, then braking
    to ~30 m/s to hold a firing position on big targets. After a kill the next target is locked within ~2 s.
    PvP tutorials (Discovery, Chills' "Snub PvP Basics"): engine kill + reverse for anchor turns, strafing opposite
    the turn, early turning vs jousting - PvP techniques; jousting does not apply to NPCs (user). Consumables and
    ordnance across the playlist: `..\..\docs\kb\consumables.md`; HUD list scans: `footage_scans.md`.
  - **Predictive layer** (`ai::ReadSelf`, `Assess`): own hull / shield / power / gun range / nanobots / batteries
    from the game's objects; the threat in fighter units (class weight x attacker x hull); repairs (above);
    leaves the fight below the flee threshold or when outmatched and losing
    (`caution` 0..2: afterburner, then cruise away from the threat's centre); free roam only (never in a mission):
    `FightOdds` races their time to destroy us against ours (loadouts' gun potential, hit points, then the fight's
    measured rates) - a hopeless race (`retreat_odds`, default 0.05; the loadout prior alone below 0.02) is left at
    first contact, the outmatched rule uses the same race (the head count only without loadouts); calibrated and
    guarded by the sweep section `fight-matrix` (`..\tools\flfights.ps1`, `..\..\docs\kb\consumables.md`); threat weights: class x real toughness
    (max hit points vs ours, square root) x area danger (+25 % per zone encounter level above the player's rank,
    space::ReadArea); pursuit band capped at half the gun range. **Retreat phases** (20 Oct 2026, D): break
    (afterburner jinks) / charge (lull, the cruise charge fits) / cruise (speed > 0.75 cruise); a cruise disruptor hit or
    re-engagement drops back to break; events `autopilot retreat phase=...`.
  - **Loot, the player's way:** tractor all (USER_COLLECT_LOOT) as a reflex whenever worthwhile loot (fits the hold,
    worth >= `loot_min_value` at the goods.ini base price) is within the tractor's reach ([Tractor] max_length), in
    the fight too; after the fight fly within reach and hold still; single items (the game's Loot behaviour, most
    valuable first) only with the hold nearly full (< 15 % free).
  - Start / stop (since 25 Oct 2026: View held half a second on the pad, K / the bar's fifth button / HUD 2's
    autopilot icon on keyboard + mouse); stick > `autopilot_cancel` hands back; triggers still fire.
  - **One start for every device** (the owner, 26 Oct 2026: "Autopilot functionality needs to be identical across K&M
    and Gamepad"; `StartSmart`, autopilot.cpp): with `start_smart` 1 (default) the plain start reads the game's live
    state - an objective now (the game's objective text id) -> the objective mode; else a waypoint list (the nav map's:
    the player's plotted route or the mission's) -> travel along it, across systems; else the target-based start
    (`StartForTarget`: none -> combat on the closest enemy, hostile -> combat, ship -> escort, dock / lane / gate / hole
    -> dock, else goto). `start_smart` 0 = the target alone (the start before). Event `autopilot smart-start
    objective= waypoints= -> objective|waypoints|target`; debug `ap start smart`. The stop by the key / the bar click is
    the player's stop (StopReason::PlayerInput): it ends the objective mode as the pad's View hold always did (before,
    `Stop("bar")` left the supervisor on, which re-decided into Hold - found by the parity test). The goal panel
    (AUTOPILOT_UX.md 5.3) has a keyboard + mouse form: Shift + the autopilot key or Shift + click on the bar button /
    HUD 2's icon opens it (the pad: X while it flies, the wheel's AUTOPILOT GOALS). Verified 26 Oct 2026 (release, HDE
    4K, `research\experience\runs\autopilot_parity_2026-10-26\`): M03 SaveS010 K and View held -> objective; SaveS023
    (no objective) with a waypoint -> waypoint travel, without -> target, both devices.
  - **Missiles** (18 Oct 2026): wanted on need (outmatched by measured time-to-kill vs time-to-die, big ship, slow kill)
    or the economy (price <= 1 % of net worth and stock + the hostiles' expected loot drops >= 10); launched only when
    the predicted flight (munition muzzle / motor / lifetime from the game, our range rate) is <= 1.5 s, the nose is
    on, the starting velocity is within the seeker's cone, nothing friendly / neutral is in the corridor, and the guns
    + missiles in flight will not kill first; otherwise a run (afterburner, nose on, up to 6 s; the run-in per target
    type since 21 Oct 2026). **Missile burst** (21 Oct 2026, user: missiles are the textbook case once the shield is
    down): a failed target shield is a reason to launch, timed within the generator's offline window (shields fail at
    offline_threshold and stay down exactly 12.00 s), several missiles landing together. Guns fire by their own
    fire slots (the game's HUD list), never fire-all (it launches missiles); hold fire on friendly / neutral ships in
    the line of fire (stations too: the game's hull ray test, `space::SightBlocker`). The ship's own behaviours never
    launch consumables (personality launch fields zeroed). Events: missile-run, missile-launch, missile-round-left,
    line-of-fire. Fixture: SaveC705.
- **Objective mode** (`ap start objective`, 19 Oct 2026; host API v11): a supervisor above the modes doing what the
  current objective asks from the player's information - fight (mission-pointed hostile within 6 km or an attacker
  within 3 km), a friendly target ship (escort words: formation, refused -> follow; else follow), a hostile target
  (combat), a planet's ring / a station (travel, dock only when the words say so), a lane ring with a dock verb (ride it
  toward the neighbour `space::LaneOpen` allows: the mission's own permission; far: arrive near it first; nothing is
  decided while riding / jumping), the navmarker (travel), else rest (Hold). Re-decides on objective change, the sub-mode's end, a fight over, a threat; keeps a running mode with the same
  target; decide-loop guard (6 in 10 s: 5 s pause). An escort objective whose target is a place (a buoy) escorts the
  ship an objective pointed at earlier (`escortShip`: M03 transport1), else the nearest friendly marked ship. Verbs are
  English keywords (other languages: kind only). A travel objective (fly to / dock with a place or a navmarker)
  outranks the fights around it (user: adhere to the mission's script); once arrived, threats are fought as before.
  Verified: M03 S010 -> SUCCESS alone; **M03 from step 5 (campaign
  Save1e7d) -> "Mission Result: Success" with the objective mode flying every space part** (lane 42 -> 43, convoy
  escort, ambush 8 kills, Willard, Valkyries, CA Minor, Walker, Battle of Willard 11 kills; 401 + 136 + 295 s) and the
  harness only for the rooms (launch x2, the CA Minor bar scene) - 19 Oct 2026; **M04 "Escape
  from Liberty" from Save6380 -> SUCCESS 1173 s** the same way (`..\tools\missions\M04objective.ps1`, sweep `objective-M04`;
  3 / 3 in 1002-1059 s on 21 Oct 2026); **M05 -> SUCCESS 1225-1304 s** (`M05objective.ps1`, sweep `objective-M05`).
  Rules learned in M04 (general): a way out the objective names - a lane ride the mission allows, a jump gate / hole -
  outranks the fight around it (the threat check leaves those runs alone); a lane objective the mission still keeps
  closed = wait at the ring and take it the moment it opens (watched every 0.5 s); no flee while the objective mode
  runs (mission battles have tethers: "You fled the battle"); lane entries only at pairs the game grants. Hostiles
  attacking our side count by the target's reputation (stations, platforms too - Contact::attitude is ships only); a
  target hidden behind an obstacle stays a candidate beyond 1.5x gun range; interception cuts cruise half a second
  short of gun range. Not yet: rooms (the mission scripts do the player's base stop - repair, restock - before every
  launch: M03 / M04 / M05objective.ps1).
  **Survival intent** (20 Oct 2026; user + the 2020 footage: M04 to Benford is "fast-and-cheap survival", `..\..\docs\kb\
  missions\M04.md`): a Survive objective ("defend yourself", "survive", "stay alive", "escape") or the mission's title
  ("escape" / "flee" / "survive", host v13) sets a stance for the whole mission: only attackers on us within 1200 m
  are fought (not the battle around - its friendlies may die by script; the script may answer every kill with a
  backup wing), a hostile objective target is fought only on a Fight verb, and the rest is escorting the mission's
  friendly ship (Juni), never a hold in place. Targets at the game's invincibility floor (Act_Invulnerable,
  `space::AtInvincibleFloor`) are never targets. M04: 4 of 6 deaths before -> the stance's batch 2 of 2 escapes.
  A rest under the stance that takes hits moves out of the fire (`under-fire` event: from the toughest hostile within
  2.5 km to the far side of the lead, 1 km farther than now) - a capital ship's turrets fire on us while the ship itself
  targets another, so "being hit" is the signal, not `targetsPlayer`; fire from a fighter is answered by fighting it
  (Mactan's Rheinland wing: was held out 35-60 s). Under the stance a big ship the mission marks is scenery in the target
  ranking (an objective's own ship comes as the chosen target), extend runs only while the race is lost, and combat
  keeps out of a big target's bow arc (its main guns: inside 35 deg of its nose within 3 km -> a flank run, clear at
  50 deg; user, 21 Oct 2026). A hold near a dockable base rests off its lanes (45 deg off the exit line, 2 base radii out).
  **Speedrun-session rules (22 Oct 2026, generic leg probes M06-M09; `..\..\docs\LEGS.md`):** objective words match at
  word starts ("Rheinland" is no "land " dock order); an objective re-issued unchanged (same text, target, navmarker) is
  no new step (M07: re-issued every second, each restarted the travel); no target and no navmarker = the mission's
  newest non-hostile mark (M06 "Fly to Quintaine"); a formation the MISSION gives the player during a step
  (`mission-form`, read while `ai::MissionHolds`) is kept for that step's travel (M09: the arch counts when Ozu arrives);
  under a Fight verb ("Destroy ...") the marked objects outrank everything and are targets whatever their attitude unless
  friendly (M09's neutral Shield Generators; speedrun note A6). **Race rings** (`pass`): a satellite-class solar, no lane
  ring, physical radius >= 30 m (track ring 88 m) is flown through, not arrived at - cruise held, a crossing point in the
  ring's plane biased toward the next ring (racing line), look-ahead to the ring the exit axis points at, a miss turns
  back, the turning circle drops cruise only when the aim is unreachable, ships ahead within 150 m pushed off the line
  (M06: bumping Hovis = `Cnd_ProjHit` fail). M06 race 1 WON 22 Oct 2026; practice lap 74.1 s on the free-roam fixture
  SaveC707 (`..\tools\flrace.ps1`, history `research\experience\legs\race.tsv`), = the user's 2020 lap
  (`..\..\docs\kb\missions\M06_race_footage.md`). `cruise_burn` (thrusters over cruise) measured no gain: off. Debug
  `ap start pass <ring>`; events `pass ring / pass next / pass-missed / pass-trace / pass-brake`.
  **Endgame rules (22 Oct 2026, M12 / M13 legs):** a mission's Idle park (Act_PobjIdle: op freeflight at a priority,
  unlocked) is no hold - the objective mode flies at once (M13's launch parks the player ~26 s among Nomads); the leg
  driver arms the objective mode in the base (`ap start arm`) so it decides on the first frame in space. A dock port
  held without our request (a mission's Act_DockRequest, M12's core exit) is waited out, never overridden
  (GameDocksUs). A travel arrival "close" (no progress 8 s within 2 km) counts only where a solid keeps the ship off the
  point; elsewhere it re-plans (M12's lair waypoint wants 500 m). **Turret blind spots** (`blind_spot`, default on; user:
  "Rheinland battleships have blind-side gaps"; the 2020 M13 footage parks inside the Nomad battleship's hull and kills
  it in ~45 s at full hull): a big target with >= 4 guns is scored at 26 directions x 0.2-1.1 radii (+150 / +300 m) in
  its own frame by how many of its guns reach each spot - the game's own CEGun::CanPointAt + munition range
  (space::GunsCovering, debug `cover <id>`) - then by our way there; the best is flown to on the afterburner and held by
  station keeping (Out::station: throttle / reverse along the nose, slides across, the pursuit keeps nose and guns on
  the target). The test is not the whole truth (M13: the 466 and 333 m aft spots, "uncovered", were hit): a spot where
  > 2 % of our hit points go per second, two readings in a row, is marked bad and the next taken (M13: 133 m from the
  centre on the long axis, inside the hull envelope - zero loss). The test runs with the joint limits widened by 0.2 rad
  (marginal spots count as covered); bad spots are kept per target across fight restarts. **Shelter** (`blind_shelter`,
  default on; user, 22 Oct 2026: hugging the inside of the Rheinland battleship's bow trench, its hull blocked most shots
  from the other ships): among uncovered spots, the one fewer hostile ships (within 2.5 km, up to 12) have a clear
  real-hull line to wins (150 per exposed line against 1000 per covering turret). More (same day): a fight that
  moves < 30 m and deals nothing in 6 s is wedged (M12: inside the lair's structure) and breaks out 3 s away from the
  nearest solid; a pursuit with a costly field here or 3 s ahead takes the avoidance's line (M11: a mine at the Osiris
  mouth); an escort's lead is the marked ship whose game name shares a word with the objective ("Sinclair and
  Quintaine"; M13: the nearest was Juni, who follows the player - nobody moved); a follow that stands in for a refused
  formation asks again within 800 m; an "arrived" step whose navmarker in this system is still > 600 m off travels again
  after 10 s (4x per step); an arrival point inside a solid's sphere is approached via a staging point straight out from
  the solid's centre beyond it (going round counts as progress); a fast-dock approach without 100 m of progress in 20 s
  hands over to the game's dock (M09 Tekagi's Base). Round 3 (same day, M12 / M13 full legs): **torpedoes** have their
  own launch (USER_LAUNCH_TORPEDOS, launchers on HpTorpedo..; before, the torpedo was taken as "the" missile launcher and
  none ever fired) at big or static targets with hit points for two or more, at the launcher's own refire, never from
  inside the target's radius + 150 m (M13: a torpedo from inside the battleship's hull envelope struck the hull beside
  us); missiles likewise outside radius + 100 m. The pursuit brakes by the closing speed (big targets 1.2 s of it beyond
  the band, fighters 0.5 s above 80 m/s: rammed a shield generator / a Nomad fighter); a blind spot's last 800 m are
  flown by station keeping. Race rings are 30-300 m and not hostile (the Nomad City read as one). A non-fight step with
  a navmarker becomes a trip after 2 min (endless waves); a retry short of the marker goes by a via point 500 m off the
  line; a followed mission lead at rest 15 s -> up to 2 km from the gate / hole nearest it (M02 King). The docking
  cutscene skip only for docks into a base. "bar" (whole word) and "enter" (not "enter formation") are dock verbs.
  Lethal zones are never exempt from the pursuit's avoidance. A travel that wants a dock at an object that does not
  read dockable docks there (M13 sphere airlock); a mission's dock grant we did not request is handed over without
  cancel while the ship's own Dock op runs. **Airlocks are flown through, never docked** (`airlock_fly_through`, default
  on; session 4, 22 Oct 2026, the user's report "stuck in letterbox mode inside the Dyson sphere"): the game's Dock
  maneuver at an airlock (dock point type 8: the Dyson sphere door, the Nomad lair / core) sets the ship's control
  exclusion 4 at its start - letterbox and no steering - and the airlock's system switch never clears it
  (`..\..\docs\FREELANCER_RE.md` "Control exclusion"); every M13 run since the airlock dock went in had flown the
  sphere letterboxed (the 1194 s PASS too: combat inside on-target 1.6 s of 262), and the M12 full legs failed in
  the core the same way. The fast approach lines up as at a gate, then runs through p2 and the mount 200 m on at full
  speed with no request; the system switch is the transit (`travel airlock transit`), as the 2020 footage flies it.
  Fallback: 50 m past the mount with no transit for 3 s -> the request as before. Verified M13 from St03: transit 21.9 s
  after the approach began, exclusion 0 throughout. The exclusion is logged on every change (`control-exclusion a -> b`).
  **Close-quarter flight (session 4, user, 22 Oct 2026: "a lot of crashing into geometry"; "the player is required to
  make close quarter flight decisions that are highly aware of nearby geometry"; no LLM in the loop - the mod alone):**
  `geometry_guard` (default on) runs over every mode's steering: the way the ship moves is ray-tested against the real
  hulls (`avoid::RayFan` over `space::RayHitDistance` = PhySys::FindRayCollisions; three parallel rays for the ship's
  width) out to its stopping distance + 0.8 s + 40 m; blocked = the clear direction nearest the goal from rings at
  12-85 deg (brake when none), no afterburner / cruise; rays across the motion (80 m) slide the ship away from a wall
  closer than 35 m (the 2020 M12 lair trench at 200 m/s: straight down the middle, small sideways corrections). Left
  alone: our dock request out, a control exclusion set, < 20 m/s, the object being entered / attacked. The bounding-sphere
  avoidance (`avoid::Steer`) stays for fields and planets. Dense study clips of the 2020 tight-quarter / blind-side
  footage (10 fps): `research\videos\catalogue\clips\` (`research\tools\clips\dense_clips.ps1`).
  **Attack passes** (`static_pass`, default on since 22 Oct 2026 ~14:00; the orbit below is the fallback): in on the
  pursuit (band radius + 80 m), a firing window within radius + 200 m (<= 4 s or until the shield fails), out to an
  exit 600 m beyond the hull with a clear path and the most cover (8 directions; real-hull rays), again - from the 2020
  M13 St03 generators (catalogue c0011: four in ~2 min, nanobots untouched). M13 from St03 PASS 767 s with it (hull
  96 %, 17 nanobots; the orbit runs died with 0) but slow: St03 generators 5:24.
  **Hold-and-salvo** (`static_hold`, default on since session 5, 22 Oct 2026; Source::Skill; tested in game up to
  commit 8df84fb, the items marked *untested* came after): every generator, satellite and platform is fought the way
  the top-3 runs do it (`..\..\docs\research\speedrun.md` V4; the user's own reading of run 1's M12 / M13 and the
  calibration ledger `research\evidence\speedrun\reference\calib\CALIBRATION.md`): **once targeted, fired upon without
  pause until destroyed**, from a hold `static_hold_gap` (60 m) off the real hull - the runners' HUD 58-100 m is a
  surface distance (0-M against a cruiser's hull), drifting out until the kill. The hold point (the game's ray test
  from just outside the target's sphere to its centre, on the best of 9 sides by a clear path, the fewest attackers
  with a line to it, the way there; sides lost on before avoided) is approached by the pursuit and held by station
  keeping (entered at 2.5 s of the speed / >= 250 m, left 150 m beyond). The hull ahead decides (tested session 6,
  23 Oct 2026) - closer to the **target's own** hull ahead (the ray's hit inside its sphere) than the gap + 60 m
  re-anchors the point onto our own line at the gap, closer than 0.7x the gap pushes it back (a 666 m M13 structure:
  the centre-based band let the ship drift to 150 m from its centre, guns mostly silent; before the own-hull test the
  ray met the M12 lair's lattice 3 km short of a generator and the ship held there 8 min without a shot); the
  **slide** `static_hold_slide` (5 deg/s, one direction per hold, no reversals - the runners'
  "light orbit", measured from the far background) moves the point round the target at its distance; mines every
  1.5 s (`static_hold_mines`; the controller taps USER_LAUNCH_MINES, ignored without a dropper); **exposed** (attackers
  within 700 m, shield < 25 %, no batteries) = the slide three times as fast, still firing; **hull lost at the hold**
  (> 4 % in 3 s) = that side marked bad, another side, the guns kept on (the attack passes' exits stopped firing). A
  held static target is kept until it dies unless the hull is under 35 % (targeting below); it survives detours to
  other targets. No missile run from a hold. Events `static-hold`, `static-hold at point`, `static-hold re-anchored`,
  `static-hold exposed`, `static-hold bad`, `salvo-check` (what holds a launch back, every 2 s at a hold). Measured
  (8df84fb): M13 from St03 470 / 500 s (was 767), the barrier generators 124 s for four (was 5:24), M09 from the arch
  597 / 611 (767), full M09 834 (852), the three M12 generators 92 s (runners 72-101). Session 6 (c7f51cf and after,
  with the script destroy goals): M13 from St03 **422** s, from the city **171** s (was 255; 174 in a second pass), M12 667 / 629, M08 696 /
  702, M09 from the arch 696. Open: consumables - at the barrier all 28 nanobots and 29 batteries went by repairs one
  at a time (run 1: batteries 9 at once on a failing shield, nanobots kept for the city; IDEAS A31); the city's big
  generators (radius 915 m) were held 42-119 m off the hull among 12 attackers - death in 0.4 s from full hull (run 1
  holds under each generator's cone at HUD ~200 m, cruising ~2 km above the towers).
  **No-progress rule** (target-invulnerable after 6 s on target with the hull unchanged): only time on target within
  gun reach **and firing** counts (M08 23 Oct 2026: a blind-spot hold 400-950 m off Tekagi's platforms gave both up;
  M12: a shield generator given up at hull 0.03 while Juni crossed the line of fire and the guns held - 9e10904,
  untested in game). The
  neutral-mark rule of a "Destroy ..." step (marked neutrals are targets, A6) is for solars only: a neutral marked ship
  is someone the mission is about (M08: the transport to capture was shot).
  **Launches at static targets close by** (a solar whose hull the ray test sees within 300 m): the blast rule reads the
  real hull gap on the line and along the nose - the blast radius + 5 m, 20 m at least (*untested*; was 2x, 30 m;
  HDE: torpedo 25 m, missiles 16 m); the friendly corridor is the blast + 30 m and ends at the target (the wide one -
  allies, the sphere's shell "behind" each generator - held 90 % of the salvos); no flight-time gate (a static target
  never dodges). Ship targets keep the centre-distance rule (radius + 150 / + 100 m: the M13 battleship torpedo).
  **Targeting dwell** (`target_dwell` 6 s, *untested*): a live target is kept at least that long before the ranking may
  switch (M09, the user: "indecisiveness while targeting the shield generators" - generator <-> attacking fighter
  every ~3 s under the pressed rule's shield hysteresis).
  **Script goals** (`script_goals`, default on, Source::Hidden - knowledge scope `all` only; tested in game session 6;
  offline mirror `tools\resolver_check.py`, cross-checked by `tests\decision_test.cpp`; live `ap goals`): `core\game\mission.*` reads the current mission's script from the install and
  returns the flying conditions the step's armed triggers wait for (armed = what the objective's setter chain and the
  system-entry triggers activated, the chain itself and switched-off triggers excluded, a system inherited from the
  arming trigger; a single Cnd_DistShip / Cnd_DistVec on the player or a ship that follows the player; leading within 4
  hops to a fresh objective or SUCCEED, never FAIL). For a fight step, a step with no objective, or a trip whose
  condition a follower drives, the objective mode travels there instead (the mission's success first, then the
  nearest; event `script-goal`): M12 Omicron Minor "Destroy all of the Nomad Ships" -> within 8 km of Toledo (runners
  63 s vs our 224); M08 "Fly to the Waypoint" -> Ozu within 9 km of the Ku01 -> Ku04 hole (all three runs cruise at the
  hole, the banner flips 5.0-5.5 km out; verified 23 Oct 2026). Leashes (Act_NagDistLeaving) are not checked yet.
  A distance goal the ship is already inside is skipped (it has fired or fires by itself - M12 core: "within 700 m"
  at 678 m won over the power cell, the trip found no way, the ship rested); a script-goal trip the game cannot route
  ("no way") drops that goal for the step (event `script-goal ... no way there`; 9e10904, untested in game).
  **Destroy goals** (session 6, 23 Oct 2026; user, M13 from St03: "failed as soon as the player ship targeted a nomad
  ship rather than fly straight at the generators in the trenches"): a single Cnd_Destroyed of the mission's
  [MsnSolar]s by label or nickname (count <= the objects) whose chain leads on; a FAIL counts there only through
  triggers that fire at once (Cnd_True / Cnd_Timer) - one the chain merely arms (M13 fail_range3 "outside 18 km") is a
  watcher. On a step that is no fight, escort or dock and names no ship of its own (a place, or only the mark
  fallback), the live objects (contacts, or beyond the contact list the solars the game still places: the city's
  generators 20-60 km off) become the mission's marks (`MissionObjectIds`) and the step a fight: the ranking goes for
  them first, attackers only when the race is lost; the nearest beyond 5 km is flown to 1.5 km short first as a trip
  (event `script-destroy`, actions `script-kill` / `to-marks`). Steps it changes (offline sweep): M13 "Head to the
  Barrier" (the four generators: the runners kill one before "Destroy the generators" shows), "Fly to the Waypoint"
  in the sphere (2 of the 5 city generators end the mission), M09 "Go to Tekagi's Arch". A threat fight re-decides every
  3 s while such a condition has an object up (the city's generators spawn after the first decision).
  **What names the step:** a placeholder objective ("null", "nn_blank": "Awaiting Mission Objective" 21660) or a text
  more than one mission's script uses names none - the last real objective stands in (`from-last`), and with none
  known (a save loaded mid-mission) the script that sets the mission's title (Act_SetTitle) with destroy goals only
  (from its system entry alone the order of the flying ones is unknown). A target that is only the mark fallback (the
  newest friendly mark) does not block script goals (M12 Omicron Minor: "Destroy all of the Nomad Ships" shows for 0 ms,
  then "Awaiting"; the ship escorted Juni 165 s instead of flying to Toledo).
  **Knowledge sources** (`AUTOPILOT_UX.md` 2a): each behaviour names its `Source` (Seen / Skill / Hidden / Meta);
  `knowledge` = `all` (default: the speedrun preset) | `exposed` (Hidden and Meta behaviours off - the future default
  preset that knows only what the player was exposed to). Tagged so far: blind spots Hidden (turret reach from the
  guns' joint limits), hold-and-salvo Skill. Presets `ap preset speedrun | competent`.
  **A mission's dock grant at an airlock is the way in**: within 20 s of a grant we did not request, no dock running,
  the nearest airlock within 3 km is flown through (M12's lair entry came under "Fly to the Waypoint"; the run then
  fought the Nomads until the jump hole collapsed). Tested in game; the airlock run is exempt from the threat check
  (user, 23 Oct 2026: M13 "did not move with intent into the airlock" - a Nomad nearby restarted the run every 0.5 s,
  15 times; now objective to transit 15 s).
  **Motion trace** (`motion_trace`, default on; session 6): `FLMotion.tsv` beside the log, 10 rows / s while the
  autopilot flies - mode, action, speed, the mode's steering (pursue / station / steer / brake), throttle, afterburner,
  cruise wish, slide, the geometry guard (blocked, clear distance along the motion), wall slide and its distance,
  target and its distance, the nose's alignment with the motion, position. flleg keeps it with the leg's evidence;
  `research\tools\motion_summary.py` cuts it into segments (speed, afterburner / cruise use, guard share and flips).
  First reading (M12, 23 Oct 2026): inside the lair the cruise is wanted 88-92 % of the time at 119-198 m/s mean, p10
  78-80 m/s - the charge keeps breaking on the turns and the ship mostly flies at engine speed, afterburner 0-1 %
  (runners 237-299 m/s); the guard blocks < 5 % of the time. Since 7466b45 the trace also records the **player's own
  flight** (pad or mouse / keyboard: mode -1, action `player`, the game's throttle) plus the engine state (cruise
  charging / cruising), the nose vector and the system - the user's reference runs (M12 23 Oct 2026,
  `research\experience\reference\M12_user_2026-10-23_try3_*`).
  **Speed carry (session 6, 7466b45 / ed37ba9; built, untested as a full leg - the user: the runners "fly ahead to the
  waypoint" and "tractor the power cell without slowing down"):** a trip that arrives while the objective mode runs keeps
  the cruise (the next step decides); **fly-through**: a navmarker step whose script condition is "the player within R of
  a point" holding the marker is aimed beyond the marker along the approach - no braking (event `objective fly-through`);
  a loot destination is flown through (the beam takes it from its reach), and loot the mission marks is tractored in
  every mode within the beam's reach (event `tractor marked loot`); no new cruise charge before the nose is within ~25
  deg of the goal (the M12 core checkpoint faced the wall: the charge, the guard's brake and release flipped every 0.1 s,
  the ship crawled backwards 70 s); the geometry guard ends the cruise only when it must brake and does not reverse a ship
  under 50 m/s (it turns it). The ray test now counts a solar's **moving parts** (class 0x1: the M12 core's reactor wheel,
  `..\..\docs\FREELANCER_RE.md` "Ray test") - the guard and the line-of-sight checks see the turbine's spokes; the timed
  crossing of such a moving obstacle is not built (HANDOFF NEXT 1).
  **Session 7 (23 Oct 2026, M12 with the user watching; 441c74c ... b6a6039; LEG_DOSSIER M12). State at the end: M12
  from M12_ck_launch PASS 3 of 3 (499, 482, 477 s; best 463), control -> 500 m of the hole 284-298 s (best 273.1; the
  user's try 3 209.4). Judged by 3 parallel runs per change (TESTING.md) - single runs misled several times.**
  - *Line of fire*: a solar hostile to us never blocks it (the lair's hull behind a generator held the guns 60 s); the
    event `line-of-fire solar <id> before|behind the target att=.. -> fire|hold` names the blocker.
  - *Pre-stage* (`mission::GoalsAfter`): a step whose own condition has fired while the script talks (a placeholder
    shows, or a destroy condition is met - `objective <trigger> met ... deciding again`) flies to the next step's
    targets: its destroy objects (a hold 300 m off the nearest, nose on it with pitch, no threat fights) or its distance
    condition (the sphere's near side, flown through). `mission::GoalsArmedBy`: a fired condition's chain arms the next
    one - that one is the trip (Omicron Minor 8 km -> 3 km of Toledo's ring; verified, 3 of 3 land).
  - *Fly-through to a condition's near side* when the marker sits near its sphere's edge (0.7 R, aim at the centre); a
    trip that ends outside its condition goes again (4x), never into a threat fight.
  - *Real geometry first*: `avoid::RayPath` (per berth 80 / 30 / ship-width m every plane - level, vertical, solid-centre
    - and a 3D lattice (`RayPathGrid`, lazy A* over 120 m cells, the start reaching two cells round it), the shortest
    kept; a node line through the destination and the solid's centre; 80 000 rays at most (12 000 measured 28 ms); event
    `path ... rays .. ms (mode, berth)`; `planesFirst` = the first plane wins, no lattice - the hops: the lattice's chords
    stalled 2 of 4 hop runs in the trench) and `GeoPathAim` (a pure-pursuit carrot ~1.5 s ahead, on to the next leg past
    its end or within 150 m of it, centred between walls above / below and either side, at most 120 m, smoothed; **planned
    again when more than 250 m off the leg flown or 8 s without 50 m of the way gained** - the z3-b core trap). Used by
    the hops between static targets (`static_hop`: cruise by the flight model's plan, cut at the hold's reach; a charge
    not done after 8 s at engine speed -> 10 s of afterburner; under 10 m/s for 2 s -> 2 s back off with a slide, then a
    new way; no dodges / turn-evades while hopping), ArriveAt when the real line ahead is blocked, and the airlock run
    (only beyond 400 m of the door's outer point p2, judged by the line to p2 - the line to its aim inside the airlock is
    never clear, and the run orbited p2: 2 of 3 runs stalled). On a ray-clear line no solid's bounding sphere is avoided
    and no staging is flown; inside a sphere it is not avoided at all (the M12 core's radiation: 8 m/s); a damage zone
    the ship is in is no strict field. The geometry guard trusts a planned leg that is clear for the ship (it brakes only
    for a hit inside the stopping distance), the wall slide stays off on it, and a wedged fight's break-out (the ray
    fan's clear way) is left alone.
  - *Hold*: gap `static_hold_gap` 20 m - but a loaded launcher's blast radius + 10 m (torpedoes: 35 m) - and the lateral
    run `static_hold_lateral` 35 m/s along the hull (reverses at geometry); mines only within 40 m of the hull while
    sliding; station keeping from the braking distance + 60 m (was 2.5 s of speed: 14 s crawls).
  - *Countermeasures*: a cruise disruptor in flight at us always matters while the engine charges or cruises (5 hits in
    30 s reset a hop's charge with no countermeasure dropped).
  - *Airlocks*: a mission's grant at an airlock is flown through at once (no 4 s wait); the airlock run drops cruise while
    its planned way is flown. (Tried and reverted: "straight in from beside the door" - it circled the door.)
  - *Fast dock*: never at rest under fire while a dock waits (the game's dock parked the ship 440 m short of the Omicron
    Minor hole among 10 Nomads): 4 s on the afterburner round the entrance, then the request again.
  - *Rotating parts* (`wheel_timing`, avoid::WheelScan / WheelAim): two 33 x 33 ray scans 0.25 s apart across the line
    of flight -> pivot, spin, the freest ring; the crossing point re-aimed each frame for the predicted arrival; the guard
    leaves that part to the timing. Readings (M12 core, 6+ runs): spin -47/-48 deg/s (measured 36), ring 380-410 m, maxR
    406-412 - the scan takes in more than the wheel (the chamber?); crossings worked, but check against `ap rayraw`.
  **Session 8 (23 Oct 2026, AFK; a061c04 ... 57f855c; per-change verdicts LEG_DOSSIER M12 "Key notes", gate results
  HANDOFF). M12 FULL leg from Save5e67: 3 of 3 PASS at 84bd318 (498 / 454 / 516 s) -> best 393; par 348.**
  - *Trips*: a new trip to within 150 m of the running one's destination, or to the object it goes to, keeps it (no
    brake / re-plot / cancelled game dock); a script goal that arms the next one inside its own sphere is flown through
    to that one (M12 8 km -> 3 km of Toledo's ring, cruise kept); marked loot already in beam reach keeps the running trip.
  - *Script goals*: never beyond 85 % of an armed leash (`mission::Leashes`: Cnd_DistShip outside Player -> FAIL; anchor =
    the object, else the nearest friendly mission ship; only leashes the player is inside now); a goal reached with the
    step unchanged is dead for the step; a fight step is replaced only by a goal its own objective chain arms
    (`Goal::byEntry` = armed only from a system entry).
  - *Ways*: an elbow first (one via point over the straight line at the destination's height +-150 m, both legs
    ray-clear), else out first (8 headings x 3 climbs incl. level, 150 / 250 / 450 m) then the elbow, at most 1.6x the
    straight way; while no way is known the elbows alone each second (the lattice keeps its 6 s pace). A fly-through
    pushed off its line of approach rejoins it 3 s of speed ahead where both legs are clear (held through one failed test).
  - *Airlocks*: within 1.2 km of the door and not lined up - or lined up with the nose 45 deg+ off - engine speed (at most
    120 m/s), no cruise / burner; the line-up lapses outside its cone; a mission's grant skips the airlock the ship came
    in by (`SystemEntry`).
  - *Missiles*: at a hold on a static target they refire at the launcher's rate (the pilot's missile_launch_interval_time,
    20-45 s, held one salvo per generator); *fast dock*: no queue-shortcut rush under fire.
  - Harness: `tools\flgate.ps1` (the leg gate); Leg.ps1's stall watchdog counts comm lines / mission messages.
  **KNOWN PROBLEMS (user, 22 Oct 2026 ~14:10, watching M12 / M13): "inefficient gameplay, weird jitters, indecisive
  non-firing"; "M12 is ignoring the objective and throwing itself at Nomads"; "M13 is not simply attacking the
  generators as it should".** Causes in the logs: the geometry guard flips blocked / clear every 0.1-0.2 s and replaces
  the steering each time (jitter; needs hysteresis and a blend); the guard override and the passes' out phase turn the
  lead pursuit into a steer run - the guns do not track (non-firing); the pressed race rule hopped across 7 fighters in
  44 s at the city before returning to the generator, and the passes broke off a 915 m city generator after 15 s (not
  simply attacking); 4 s windows + long exits whose cover score never differs (208 every time) - slow; structure
  flights at 80-140 m/s under the guard vs the 2020 trench at 200 m/s. Since (session 5): the speedrun research is
  done; slow passes / not simply attacking / target hopping are addressed by hold-and-salvo, the dwell and the held-
  target rule (below); jitter and structure-flight speed remain open.
  **Static targets on the move** (`static_orbit`, default on; user: "M12 had the player ship HOLD STILL as it was
  shooting target generators"): a target that never moves (generators, platforms too weak for a blind spot) is fought
  from an orbit round it - radius + 150 m, ~45 m/s by station keeping (nose and guns on it), the direction flipping every
  3-6 s, bobbing +-60 m, turning back where a leg meets a solid - instead of hovering at the pursuit band. Baseline: the
  2020 M12 generators (in at ~200 m/s, firing window 15-80 m/s close in, away and back, never at rest).
  **Pressed: attackers before marks - only when the race is lost**: an attacker within 1.2 km ranks as high as a mission
  mark while our shield is down (until it is back above half: hysteresis - a "3+ attackers" variant flipped the target
  every 1-2 s in M09 and held us on its endless reinforcements, generators 280 s instead of 148) AND the nearest mark
  (hit points / our damage rate) would take more than 60 % of our time to die (hull + half the shield + nanobots over
  the fight's loss rate; event `pressed`). User: "inside the Dyson Sphere you can bumrush the generators and ignore the
  fighters completely" - the race says so there; it said otherwise at M13's St03 generators (M13: a generator worked under four Nomad fighters - nanobots 12 -> 5 in 10 s, dead; speedrun M13
  "Nomads: fighters first"). The user's reading of St03 (23 Oct 2026) outranks that note: "fly straight at the
  generators in the trenches" - run 1 never selects a fighter on the barrier approach (reference ref.py M13); the race
  rule still decides when the shield is down under close attackers. Nomad fighter guns: 1630 per bolt, 1 s refire, 600 m reach - the "instant deaths at high
  hull" of M12 / M13 were 3-4 bolts at 40-115 m.
  **No launch into a solid**: a torpedo or missile goes only when the straight line to the target is clear of solars and
  field rocks (space::SightBlocker, real hulls; event `launch-held`) - M12: a torpedo at a lair generator struck the
  lattice beside us and killed the ship at 93 % hull.
  Route: a stand-off point whose costly field reaches the ship stays at the base's radius (M07 Kress' base: it had
  "arrived" at the ship's own position).
  **Idle facing** (user, 20 Oct 2026): with nothing to do the nose goes to the step's point of interest (objective
  target > base / station / big ship > next route leg > companion last; MissionSubject).
  Batteries follow "hull is sacred" (user): pressed before the shield fails, see Repairs.
- **Travel and docking** (`Start-FLTravel`, travel mode; `..\..\core\game\route.*`, `avoid.*`): the fastest route from
  the game's best path (lanes, gates, holes), hazard-free (`..\tools\flroute.ps1` zones; planets' lethal zones not
  strict - a berth + look-ahead from the ship's flight model, `..\..\docs\kb\navigation-and-hazards.md`), own obstacle
  avoidance (`avoid`; debris ignored, rocks detoured - verified Li01 Detroit, 20 Oct 2026); a jump gate / hole as the
  destination is jumped through and ends the travel; a ride that ends early (lane disrupted) closes the ring where it
  stopped and the re-plan enters past the hackers (`route [ring ...]` debug; 20-21 Oct 2026); holds begun in a jump
  rest at the exit (no cruise while jumping); free-flight legs are planned the way the avoidance flies them, round a
  planet's lethal zone in the way (`avoid::PathLength`, 21 Oct 2026: Leeds' ring behind its planet was 4.8 s over
  every plan). **Fast dock** (`fast_dock`, user's player-style approach: the bay's p2,
  request just before; `dock_request_distance`, `dock_arrive_speed`, `dock_stage`): rings straight in (A/B 20 Oct 2026:
  31.2 s vs line-up 36.4 vs the game's dock 37.9, Manhattan 5 km); at bases the request goes in at the normal point and
  the server's reply decides (`DockReply`, host API v14: 3 = queued; user decision 21 Oct 2026: no hold short on our own
  busy reading); the single-bay wait no longer flickers. **Queue shortcut** (user's technique, 20 Oct 2026; gates
  only - lanes never queued in 40+ grants): only when the dock is queued, predict the NPC first at the gate, rush inside
  the entrance, formation on the ship locked in its docking animation = instant jump; a race, never a wait.
  `skip_dock_scene`: the autopilot's own docks skip their docking / landing cutscene (0.4 s). Lane speed learned per
  lane; `eta`, `worth`.
- **Settings** `[Autopilot]` in FLController.ini (`ap set <name> <v>` live; `config save`): pilot, engage, auto_fire,
  missiles, countermeasures, evade, fire_cone, follow_distance, follow_offset, escort_radius, hold_radius (hold mode:
  run to the point, the game's Idle stop, the nose to the hold's target or along the next leg of the game's route, by
  steering at zero throttle (never the selection), fly back past this leash; yields to any other maneuver), consumables
  (needed | never: missiles, repair, countermeasures), flee, steer, bursts, engage_distance, engage_radius, big_ships,
  caution, retreat_odds, repair, repair_shield_at, repair_hull_at, battery_horizon, cm_distance, cm_shield_fraction,
  dodge_window, loot, loot_min_value, dodge, evade_scale, weave_radius, slide_time, extend, extend_quiet, extend_max,
  extend_jink, extend_patience, stagger, turn_evade, adaptive, duel (force one: 0 / 1 / 2, -1 adaptive),
  skip_dock_scene, `p.<field>` (personality overrides); travel / dock: fast_dock, dock_request_distance,
  dock_arrive_speed, dock_stage, ring_lineup (A/B only), ring_request (m before a ring's p2 at full speed; 0 = ~1.4 s /
  400 m out), lane_request_distance, lane_toward, fast_thrust (0 planned, 1 burner, 2 cruise: verification runs), avoid,
  airlock_fly_through, airlock_charge_at (the charge's share done at an airlock's transit), airlock_burn, wait_flies,
  tractor_exit_run + tractor_exit_range (m), passage_window, travel_passage, rejoin_nearest, wheel_parallel (m),
  noway_climb, hug_route, arc_bulge, hop_narrow (fraction), hop_cruise_hold, hop_reset_burn (session 10: all off by
  default - kept / experimental in "Speedrun flight" above), geometry_guard; combat: disable_at (end the fight
  at this hull fraction of the chosen target: "disable" objectives), fire_all (A/B only: the game's fire-all, which also
  launches missiles), blind_spot, blind_shelter, static_hold, static_hold_gap, static_hold_slide, static_hold_mines,
  static_pass, static_orbit, target_dwell; objective: script_goals; knowledge (all | exposed); run_in_background,
  focus_only. Debug
  `ap start <mode> [id|x y z] | stop | status | pilot | set | clear | field | save | load | preset
  standard|no-consumables|speedrun|competent | lof <id>`; `ap status` and the stop event carry engaged / on-target / firing / avg-dist.
  The definitive list with defaults and comments: `..\src\impl\autopilot_impl.h` struct Settings (ini keys in
  `autopilot_settings.cpp`).
- **Out of focus** (option `run_in_background`, `deploy.ps1 -Background`): host API v7 SetKeepRunning (frame gate,
  watchdog, focus guard + start-phase hand-back, virtual cursor written into the game's cursor state); `focus_only`
  drops only the physical pad. The focus guard's backup round (a system-wide thread snapshot, ~39 ms of CPU) ran every
  100 ms = 39 % of a core per background game; now 1 s for the first 30 s, then 10 s (new threads are hooked in DllMain
  anyway; the round logs any it had to catch): 0.7 %, parallel sweep 3 games navigation 132 -> 174 fps, M01a 149 ->
  183 (21 Oct 2026). Native movie skip: subtitles host `movieskip`.
- Verified 17 Oct 2026, all with the game out of focus: combat 90 s = 4 kills (2.7/min, the bot's rate; dev) and
  release 4 kills in 81 s until no enemies were left; dock on Fort Bush (24 s); escort (holds ~270 m); formation
  (joins, ~130 m); every op crash-free on target death. Not yet: real pad, solar targets in combat, Turret Mode.
- Verified (predictive layer, 17 Oct 2026, out of focus): godmode combat 7 kills in 107 s; loot phase collected 6
  items; without godmode vs 8 police: batteries used as the shield fell, left at hull 0.59 (threat 7.9 vs strength
  2.0), got away with 0.55. Background test starts never take focus or sound (FLVIBED_BACKGROUND, CBT hooks on every
  thread, z-order bottom, render client silent flag; user confirmed: not even the mouse moved). 19 Oct 2026: the
  intro's DirectShow renderer (quartz.dll, loaded with the first movie) raised the game over the user's fullscreen
  browser for a moment: modules are now patched as they load (LdrRegisterDllNotification) and an in-context WinEvent
  backstop sends any raised game window back down inside the raising call (z-order watcher: 0 of 1002 checks above).
- **Sweep state (21 Oct 2026 04:23):** full parallel sweep 17 / 17 PASS, 0 deaths; M01a ~515 s, M01b ~650 s, M02 ~940 s,
  M03 ~950 s, M04 ~1030 s, M05 ~1230 s; fight matrix 30 / 32. Open: an unexplained 4 m 40 s game freeze (M05 flee after
  two missile hits, resumed by itself; the check-ins now catch a freeze and save where the threads run:
  `..\..\tools\flfreeze.ps1`); fight-matrix variance (heavy_d12_x2, ku_gunboat under load). Leeds ring dock ~5 s over the
  prediction: fixed 21 Oct 2026 (the planet's zone in the way, above). Own missiles after the c039118 run-in: 15 / 18 hits
  in the 06:17 M04 sweep (misses: a second missile on a dying target, a 175 m/s crossing target, a head-on pass).
  Current work and queue: `..\..\HANDOFF.txt`.
- Direction (user, details in `AUTOPILOT_UX.md` and memory player_combat_priorities): success is contextual -
  mission kill objectives = targets destroyed with least damage taken; vs overwhelming forces still win, scaling
  defense with the odds (threat vs strength); an ambush during another goal may be evaded or ignored (engagement
  policy per goal); consumables by difficulty and the player's resources; algorithmic baseline + experience / RL
  (per-context experience store, no forgetting). Long term: harness + autopilot finish the game with zero input
  (the campaign-run benchmark, memory campaign-run-benchmark) and beat the 2020 playthrough's in-space times per
  mission phase without trading hull or success (memory playthrough-time-benchmark).
- Next: the P5 remainder (between missions: net-worth story gates +3500 / 5200 / 7000 / 10000, jobs, purchases; rooms in
  the autopilot), then P6 (New Game -> story end unattended, AFK work); experience store; Turret Mode (user
  idea: AI flies, the player aims with the right stick in turret view = USER_AUTO_TURRET, "TURRET VIEW", key H; left
  stick cancels); more prediction (system level, faction strength, mission objectives by script label); scan / new
  game natively; harness waits whose safety timeouts can become game signals (user: every timer should get a native
  game-data replacement).

## HUD cues (19 Oct 2026; design + status `HUD_CUES.md`)
- `..\src\impl\shieldcue.cpp`: screen-edge sprites from the fitted shield's own hit effect (`..\..\core\game\hitlook`: shield /
  munition hit looks from the game data, C++ .ale reader), tinted by the attacker's weapon impact, strength by the damage
  burst, a flash when the shield fails; the edge glow is brighter toward the hit (direction from the point the game
  feeds its own HUD damage indicator: host API v12 PlayerHit); hull hits in the ship's own hull hit look and, below the
  ship's damage-fuse thresholds, a soft edge of its damage effects (sparks / fire / smoke; `..\..\docs\kb\hull_damage.md`);
  hull damage carries its own pattern (bold hexagons by default / plates) and reddens with the hull lost, breathing below the game's
  hull-critical point; options `shield_cue`, `shield_cue_intensity`, `hull_cue`, `hull_cue_pattern`; debug `cue` (`cue dir <deg>|off` pins the direction, `cue
  hullpts <hp>|off` fakes the hull for the damage edge), `hitlook` (`hull`, `stages`).
- `..\src\impl\reticlecue.cpp`: ring pulses at the crosshair (autopilot engage / hand-back, nanobots / batteries used); option
  `reticle_cue`; debug `cue ring 0..3`. The shield cue draws in `OnRenderUnderHud` (under the HUD, user's choice; the
  overlay as fallback), the rings in `OnRenderOverlay` (over the HUD), both through `..\..\core\game\render`
  (`Sprite`, `BeginPlain`).
- Test aids: `drainshield <id>` / `shieldof <id>` (the game's DrainShields / GetShieldHealth), `shot <cmd> <px> <py>`
  (fire, then park the cursor the frame after the power draw: clean research captures), `laneopen <ring> [toward]`
  (the mission's lane permission, `space::LaneOpen`; the travel planner skips lanes it denies).
