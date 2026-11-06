# Advanced Autopilot: state, rules, settings, numbers

The autopilot as built: what it does, the user's rules behind each behaviour, every setting, the measurements and the
audits. Moved here from `..\CLAUDE.md` on 21 Oct 2026 (the audit's CLAUDE.md diet) and kept current from here on:
**a new behaviour, setting or verified number goes into this file in the same commit**, not into CLAUDE.md. The
product design (goal model, engagement policy, UI, phases) is `AUTOPILOT_UX.md`; the HUD cues' design is `HUD_CUES.md`;
the user's combat rules are in "Direction" below and `AUTOPILOT_UX.md`; the game facts in `..\..\docs\kb\` and
`..\..\docs\FREELANCER_RE.md`. Code (one file per section since 21 Oct 2026; shared types, globals and prototypes in
`..\..\modules\autopilot\src\autopilot_impl.h`): `autopilot.cpp` (core: modes, settings and personality, the state, start / stop),
`autopilot_objective.cpp`, `autopilot_behaviours.cpp`, `autopilot_target.cpp` (target choice, situation, odds, missile
economy and geometry), `autopilot_combat.cpp`, `autopilot_dock.cpp` (fast dock, queue shortcut), `autopilot_travel.cpp`,
`autopilot_frame.cpp` (Update), `autopilot_settings.cpp` (ini, status, the `ap` command); `..\..\core\game\ai.*`,
`space.*`, `route.*`, `avoid.*`, `flight.*`.

Priority (user, 21 Oct 2026): capability first (speedrun-grade); every behaviour gets its own `ap set` setting with a
sensible default and stays separable, so the player UI stays cheap (the owner's standing direction). That UI was built on 25
Oct 2026 (the options page's AUTOPILOT tab and its presets: `docs\UI_BUILD.md`); new behaviours still come as settings.

**Windows (the owner, 5 Nov 2026, GitHub issue 25): the autopilot keeps working if the game isn't paused.** No window, menu or page stops it, the
options page included; each gate is checked against the pause, never against what is on screen (`..\..\..\docs\ARCHITECTURE.md`). The pad's input
to a window neither steers nor hands back. Proof: `..\..\..\tests\ap_under_window.ps1`, flying and fighting, pad and keyboard and mouse.

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
- **Dock / goal under target churn (2 Nov 2026):** the dock holds its own goal - the selection changes (next enemy,
  clear, a station / planet / sun / far object) and 0.15 stick nudges left the game's Dock (op 7) and the autopilot's
  target untouched (trace `research\experience\runs\churn_stall_2026-11-02_032149_ap-dock.txt`). The sweep's
  intermittent `target-churn` "ap-dock stalled" (2 in 7 reused games, none fresh) was the server's "dock in use -
  entering queue" (request-reply kind 0 response 3: an NPC docking at the same station) and the game's own Dock holding
  the ship at rest in its queue - no autopilot code changed; the section now reads a queue hold as such and checks that
  the autopilot's target stays the dock target. Open: with `fast_dock` 0 (default) a base queue is waited out at rest.
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
  ring_request 10, airlock_charge_at 0.5, rejoin_nearest 1, ref_line 1 (since 31 Oct 2026, audit A1-3, every preset is a
  full snapshot: standard / no-consumables / competent set these four back to their defaults - `competent` after
  `speedrun` kept them before - and `ref_line` aims only with `knowledge all`). Events: `hold-mine`, `doomed`, `cm-charge`,
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
  `noway_climb` (planned: gen 3 -> core entry), `pre_aggro` (a preset candidate; `dock_regain` is on by default since
  gate mpb18), `extend` + 4 knobs (deleted 31 Oct 2026, AUD-A1-5; the 17 Oct benchmark loser; the survival stance's
  extend keeps the knobs' values as constants and still feeds the extends stats flbench / flfights read).

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
- `..\..\modules\autopilot\src\autopilot.*`: modes goto, dock, follow, formation, face, escort, combat, hold, travel, objective. Combat:
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
    inside each fight (settings alternate at every kill: paired samples on the same enemies; `tools\harness\flbench.ps1`:
    Enter-FLStream, Invoke-FLBenchRun, Invoke-FLABRun / Invoke-FLABBench; results in `..\..\research\bench`). Ways to shorten them (not built): stop a comparison once the difference is clearly beyond the noise; cap runs at 60-90 s (per-minute metrics); fixed spawned scenarios. Enemy supply is the limit (~2 kills/min: one patrol per reload, sparse waves after; a
    Rogue zone 6 km above SaveC702 and staying at Manhattan were both ~20 % engaged). **Fixed scenarios since 18 Oct
    2026:** spawned enemies fight (`..\..\core\game\spawn.*`, debug `spawn <npcshiparch> <faction|player> [count] [dist]
    [job= pilot= order= base= force= level=] | list | hunt | clear | clearat <hull>`; the job's scene toughness was the block,
    `..\..\docs\FREELANCER_RE.md` "Spawning ships"); `tools\harness\flbench.ps1` `Enter-FLScenario <name>` (SaveC702 fresh, police
    friendly, the scenario's spawns: rogues-2, rogues-3, police-4, elite-1, mixed-3), `Invoke-FLBenchRun
    -UntilSpawnsDead` (ends when our spawns are gone; natural encounters that wander in count as Extra),
    `Invoke-FLScenarioAB` (whole fights A / B alternating, paired; stops once Lost or Secs differ with |t| >= 2.5
    after 3 pairs, else at 8). Other player ships: the campaign saves (`..\..\docs\missions\SAVES.md`). **Fight matrix**
    (`tools\harness\flfights.ps1`, sweep section `fight-matrix`, since 19-20 Oct 2026): 15 enemy kinds on the starter ship
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
  - **Extend** (`extend` (deleted 31 Oct 2026, AUD-A1-5, with its four knobs: their values are constants now), default 0 - b4 at Manhattan, 16 runs: kills/min 2.47 vs 4.10, on target 58 vs 80 %, no less
    damage per kill; needs an odds-aware trigger; user + playlist study: under fire players afterburn away and take the attacker off screen,
    ~6 s, resume ~3 s after the hits stop): on evasion >= 0.6 for 0.8 s or shield < 35 % while hit, afterburner away
    from the threat centre with a jink (40 deg, new every 0.7 s) and slide bursts, guns firing when the
    target crosses the cone; resume after 1.5 s without hits (min 2 s, max 8 s); futile
    (still hit after 3 s: surrounded) = fight instead, 6 s cooldown (the four values are constants since the `extend`
    switch and its knobs were deleted 31 Oct 2026, AUD-A1-5: kExtendJink / Quiet / Max / Patience in autopilot_combat.cpp). Under the survival stance extend
    runs only while the race is lost.
  - **Staggered fire** (`stagger` 1, user's trick, any loadout): each burst starts the N active non-homing guns one by
    one at k*r/N (r = slowest refire), by each gun's own FIRE_WEAPON slot - the game's HUD weapon list
    (`Weapon::fireSlot`; the list index tapped a Javelin's slot until 18 Oct 2026; until 4 Nov 2026 HUD 2's hiding of
    HUD_Status sent the read to the hardpoint-name fallback, wrong on SaveC706: now read hidden too, the fallback only
    when the list is unreadable; a fight there: all four guns inside 0.25 s, no launcher fired) - then holds every gun's slot (not
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
    reported `fled`): relative hull exactly 0, else the death camera (31 Oct 2026; the old test, hull <= 0.0005, called
    a ship alive at a sliver of hull dead - set ab11 i5). The fight is assessed twice a second whatever the target rules and damage is tracked every frame
    (`lost` includes damage taken between targets and while fleeing since 18 Oct: older `lost` numbers read low).
    Big-hit evidence event (a loss > 20 % of shield + hull in one frame names the nearest hostiles). Per-second `self`
    events (hull, shield, nanobots, batteries, CMs) are the runs' track for `research\tools\section_cost.py`.
  - **Audit 18 Oct 2026 (bugs fixed, verified out of focus, dev + release):** (history: AUTOPILOT_LOG.md "Audit 18 Oct 2026")
    Open: the flee stops 3 km from hostiles, but escort missions fail at 3-5 km from the escorted ship
    (`..\..\docs\missions` M01a/M01b/M03/M06 nags) - the engagement policy must keep a mission's distance rules.
  - Tuning, 17 Oct 2026: 43 runs of weave / evasion / dodge / evade_scale and the throttle profile (history: AUTOPILOT_LOG.md "Tuning 17 Oct 2026")
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
    guarded by the sweep section `fight-matrix` (`tools\harness\flfights.ps1`, `..\..\docs\kb\consumables.md`); threat weights: class x real toughness
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
  - **Multiplayer: the server's word** (the owner, 28 Oct 2026: the autopilot technically able at full capacity, its
    permission the server's; `mprules.*`, `..\..\docs\MULTIPLAYER.md` "Server permission"): a server chat line
    `[flvibed autopilot=on|travel|off]` (server senders only, the tag alone on its line: FLHook relays players' chat as
    server lines "Name: text") decides; a silent server gets `mp_autopilot` (0 off, 1
    travel only - the default, the owner 28 Oct 2026, 2 everything). Off refuses every start; travel refuses a combat start and keeps
    guns and missiles silent in every mode (evasion, repairs, countermeasures stay). Single player: always everything.
    The space readers fall back to the client's objects in multiplayer (positions, attitudes, reps, shields, lane
    links). The objective mode works there from the client's own objective (Freelancer.exe's client object, slot 72 - one
    path for both modes since 28 Oct 2026; marks slot 75, dock replies 83, results 70): a job from the server's board, the plain
    start -> objective, lanes to its waypoint, its marked targets killed. In multiplayer a pod (CLoot) is no CEqObj: the
    attitude / rep / shield fallbacks ask ships and solars only (a job's marked pod read hostile, 28 Oct 2026).
    Verified on the local server: docks at Manhattan (under fire), Newark, Fort Bush (49 km), Hold from View held.
    **Other players** (two clients, 28 Oct 2026): their ships read friendly (0.9) whatever the server's PvP rule, so
    Assess counts a player's ship as hostile when it is the chosen combat target or has us targeted while we lose hull /
    shield (3 s) - before, a duel added no threat (`flee hull=0.10 threat=0.0`) and the odds-aware exit never fired.
    Autopilot PvP: `..\..\docs\MULTIPLAYER.md` "Several clients", `..\..\tools\mptest\pvp.ps1`. On a server the fight
    leans to survival (the owner, 28 Oct 2026: no reload, a lost ship is lost): `mp_caution` (default 2) and `mp_flee`
    (default 0.4 hull) replace `caution` and the flee floor (`autopilot_combat.cpp`, the flee block). `pvp_duel`
    (default 0, experimental; `PvpDuel`, a layer over the fight's output against a player's ship: defend = random slide
    jinks + afterburner bursts while their guns are on us, trade = slide while both are on, punish = steady, break =
    slide off inside `pvp_min_dist`, stand-off `pvp_range`) lost its first 2 A/B rounds - the fight still ran at
    12-70 m (the opponent closes); next: separation control and an evasive escape (MULTIPLAYER.md). Gun accuracy
    counts the target's shield too (`dealt-shield`); since 28 Oct 2026 every exchange estimate does (the owner: "shield
    damage dished out needs to be counted"): `OurDps` (hull + shield points per firing second), `PointsLeft` (a target's
    hull + shield while up) in the odds race, the pressed race, the missile planner's kill times and the duel's tally;
    the wedge check counts shield hits as progress. Switch `odds_shield` (on; off = hull points only). Verdict (gate
    mpb2 vs mpb0, then set oddsab: arm b `odds_shield 0`, 2 runs per arm on M09 / M11 / M13): no regression - M09 769 vs
    776 s, M13 906 vs 936 s (one arm-a death at the battleship), M11 failed in both arms (satellite / zone21 / nygate,
    as in mpb0); the M11 satellite deaths come with the static-hold point 248 m from the centre (chosen by attacker lines
    and geometry, not by the odds). Gate mpb2's slower legs: a 152 s game stall in M02's fight and a 27 s one in M10
    (game time vs wall time), the rest single-run noise. CombatStats: `pvp-maneuver / defend / trade / punish / break`
    seconds.
  - **Hold defends itself** (`hold_defend`, default on; 28 Oct 2026: two multiplayer clients holding near Manhattan sat
    still under Rogue fire for 1.5-5 min, spent every nanobot and battery and died - Hold had no threat branch): a Hold
    the player started (never the objective mode's own holds) answers attackers (Assess: hostile ships within 3 km with
    us targeted) with a Combat run where fighting is allowed, then starts the Hold again at its point when the fight
    ends with nothing left to fight (`g_holdResume`; any other end - a command, a death, another start - forgets it).
    Events `hold-defend attackers=<n>`, `hold-defend over`. Under `travel` permission the Hold does not fight (open:
    leave instead).
  - **One start for every device** (the owner, 26 Oct 2026: "Autopilot functionality needs to be identical across K&M
    and Gamepad"; `StartSmart`, autopilot.cpp): with `start_smart` 1 (default) the plain start reads the game's live
    state - an objective now (the game's objective text id) -> the objective mode; else a waypoint list (the nav map's:
    the player's plotted route or the mission's) -> travel along it, across systems; else the target-based start
    (`StartForTarget`: none -> combat on the closest enemy, hostile -> combat, ship -> escort, dock / lane / gate / hole
    -> dock, else goto; since 28 Oct 2026 a dockable BASE, a GATE or a HOLE -> the travel there - the game's best path, its lanes when
    faster, then the dock - instead of the game's straight-line Dock: the owner saw a multiplayer test cruise past the
    lane beside its route; Manhattan -> Fort Bush 73.6 s by lane vs 183 s straight; a gate 60 km out: the game's Dock ended at once, the travel jumped in 175 s). `start_smart` 0 = the target alone (the start before). Event `autopilot smart-start
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
- **Situation assessor** (30 Oct 2026, `A37_PLAN.md`; IDEAS A37 / A40 / A41): **nothing
  behavioural - no decision reads it yet** (the arbiter is S3-S7, behind a switch). A pure `situation::Assess(Snapshot)`
  (`..\src\impl\situation.{h,cpp}`) over one snapshot of the facts the objective mode reads (`situation_snap.cpp`, the
  only impure part: reads only - mission.cpp's goals through `mission::AdvanceGoalsPeek`, which leaves its memory of the
  last script / text and the shown objectives untouched; the dock grant through `SinceGameGrant`, never `GameDocksUs`);
  the snapshot as text `snap v1` (`situation_io.h`, exact round trip). The Situation: the step key from the snapshot
  alone (ObjectiveSupervise's key + the sorted marks: 4c4fb6f's class cannot recur there), placeholder (none /
  no_objective / awaiting 21660 / blank) and its stand-in (live / ini_last / title), the kind (A41 table
  `..\..\modules\autopilot\src\objective_kinds.h`) with its source - the job's objective_text id (A40, `..\..\core\game\jobs.*`) > mark
  fight > the script's NNObjective type + the subject's class + the step's Cnd_ > the words -, subject (ref / nav / mark /
  goal), held (docked / no_ship / mission_holds / game_dock / lane / jump), threats (ObjectiveThreat's rules + pre-aggro),
  constraints (leashes - armed known from the live table, S2 -, the step's nag, a job's never-kill), the script goals with their live
  trigger row and achieved, affordances, a hash. Debug `ap situation [snap|hash]` (HARNESS.md); unit tests situation /
  situation_io / objective_kinds / jobs over a synthetic snapshot and six recorded fixtures (`..\..\tests\fixtures\
  situation\`). Cost in game: the snapshot 0.1-1 ms, the live table 0.05-2 ms (S1 results in `A37_PLAN.md`).
  **The assessor's story source (S2, 30 Oct 2026; still nothing behavioural - `ObjectiveDecide` and the autopilot's
  `mission::AdvanceGoals` / `Leashes` path unchanged, `tools\decision_baseline.txt` unchanged):** when the live trigger
  table answers with a script (`mission::LiveTriggers`, single player) the snapshot's goals are `mission::GoalsOfLive`
  (each ARMED trigger with one flying / destroy condition, parsed by AdvanceGoals' own rules; inactive / complete rows
  never fire), its leashes `mission::ArmedLeashes` (a leash only while its trigger is armed: `armed=1`), a placeholder's
  stand-in the live script (`standin=live`), and with no such goal armed the kind comes from the armed row that sets the
  next objective (`mission::LeadsOfLive`; M13 docked: exit_base, Cnd_BaseExit -> base_action); else (multiplayer,
  another build, no story mission) the ini derivation as before, the reason printed. `ap situation` prints both views'
  difference (`story` / `diff` lines) and the live table's read time (`timing`); `snap v2` keeps both views.
  **The arbiter's first rules in shadow (S3, 30 Oct 2026; nothing behavioural - every decision is ObjectiveDecide's,
  `tools\decision_baseline.txt` unchanged):** `..\src\impl\situation_rules.{h,cpp}` ports ObjectiveDecide's branches up
  to the pre-stage as pure rules over the situation + the executor's memory (`Rule_Held` - ObjectiveSupervise's holds:
  mission holds, the game docks us, lane, jump; docked / no ship hold nothing, as today -, `Rule_Leash`,
  `Rule_MarkedLoot`, `Rule_TractorExit`, `Rule_Jump`, `Rule_Airlock`, `Rule_ScriptDestroy`, `Rule_DockEntry`,
  `Rule_ScriptGoal`; `DecideS3` = the first that holds, in ObjectiveDecide's order; each reads the ini view, as the
  objective mode does). **`situation_shadow`** (bool, default 0 - a measurement switch: with it off nothing runs and no
  event is written) runs them at every exit they cover and logs `situation shadow agree|differ <branch> old=... new=...`
  (HARNESS.md); `ap situation last` keeps that decision's snapshot (`snap v3`: the rules' extra facts, the memory as
  `queue`, the old decision as `decided`). Cost with it on: one snapshot + assess per covered decision (and per second
  of a hold). **S4 (30 Oct 2026, still nothing behavioural):** the branches from the pre-stage to a ship target -
  `Rule_PreStage`, `Rule_PreStageNext`, `Rule_MarkFight` (big_coast's huge static + the far-mark trip),
  `Rule_PreAggro`, `Rule_SurvivalStage`, `Rule_Survival`, `Rule_ThreatFight`, `Rule_Escort`, `Rule_ShipTarget`; `Decide`
  = every ported rule in ObjectiveDecide's order (the shadow's decider). What ObjectiveDecide writes on the way and reads
  later (the leash's clears and the script block's anchors -> `leashShip`, `lastReal`, the survival stance's mission
  memory) each rule derives from the memory as the decision began; `lastReal` (a function static until S4) is
  `ObjectiveLastReal()`. `snap v4` adds the facts and memory they read (`after`, `prestage`, `realnext`, the title's
  escape words, the pilot's system, a pointed contact's obstacle radius / named-in-text, `queue ... last_real= arrived=
  step_age= escorted= surv_*=`). **S5 (30 Oct 2026, still nothing behavioural):** the rest - `Rule_Lane` (wait / ride),
  `Rule_MissionForm`, `Rule_Pass`, `Rule_RouteLane`, `Rule_Arrived`, `Rule_Route`, `Rule_Waypoints`, `Rule_Arrive`,
  `Rule_NavWay`, `Rule_Via`, `Rule_Navmarker`, `Rule_Rest` (the survival lead's rest inside the arrived / final rest):
  every ObjectiveDecide exit is shadowed, `Decide` always proposes something. `snap v5` adds what they read (the route's
  step-1 position, lane neighbours' positions, a planet's docking ring, follow_distance, `queue ... mission_leader=
  via_next= arrive_retries= tr_pass=`); a shadowed decision reads the waypoint list as the decision began. The shadow gate
  (every leg with the switch on) measures the differences (`A37_PLAN.md`). **The switch (S7, 30 Oct 2026; built, in-game
  pending): `situation_arbiter`** (bool, **default 0 = today's decisions exactly**; experimental, not player-visible).
  With it on the objective mode's decision is the arbiter's: ObjectiveDecide's body runs in *plan mode* (it still decides
  - the oracle - and plans the trip's geometry and its memory, but starts nothing), then `Decide` over the goal queue
  (`goalqueue.*`, S6: the executor's memory classed into the queue at every decision) decides, and the executor
  (`..\..\modules\autopilot\src\situation_exec.*` + `autopilot_objective.cpp` ArbiterExecute) starts it - the body's own plan when the
  arbiter agrees with it, else the rules' plan (`situation::Plan`: each rule carries its exit's start) - through the
  **game-first gate**: nothing while not in space, on a lane or in a jump; no dock request while the game docks us, to the
  object the game's dock already holds, while our dock to it runs, or while our own request awaits its answer; no
  formation request under the leader the game already has us with (our mode adopts it: `StartAdopt`); cruise as before.
  Events: `side-effect <kind> target= step= rule= [keep|refused|adopt why=]` per start, `situation arbiter agree|differ
  <proposal> old=... plan=agree|<diff>` per decision. **The player's stop pauses** (D10): the queue is kept with the
  memory as it stands; the next start re-proves it (`situation resume silent|ask|nothing ...`): silent (a pause up to
  `resume_silent_s` 30 s, nothing unrelated done - no system change, no dock, no job, moved under `resume_moved_m` 5 km)
  restores that step's memory (arrived, escort, dead goals, retries, cruise lock, ...); ask (S9 draws RESUME / CLEAR;
  until then the start re-plans the step fresh, the story memory kept); a load / a new game / a death forgets the queue
  (the last real objective with it: after a load at "Awaiting" the cold decision - A37 S6's finding, decided). A rebuilt
  step entry starts `arrived` when the ship is within its arrival reach (1 km of the target's ring / object for a step
  that does not dock, 600 m of a navmarker not on a way through). ObjectiveSupervise (its holds, the leash trip back, the
  dock waits, the formation retries) stays on the old path. `ap situation queue` prints the queue and its pause.
- **LOOK L0 - the look-ahead, logged only (30 Oct 2026; plan and results `LOOK_PLAN.md`):** **`look_log`** (bool,
  default 0; a measurement switch like `situation_shadow`: with it off nothing runs, and with it on the decision path
  is the same code - the adapter starts nothing and writes no `g_obj` field). At each objective decision (DecideBody, after
  the `decide` event; a decision that returns before it - the leash trip, marked loot, the tractor exit - logs none) the
  pure verdict `..\src\impl\lookahead.{h,cpp}` `LookTarget` reads the facts the adapter (`autopilot_objective.cpp`
  LookFactsNow) fills: the step's armed goals (`mission::AdvanceGoalsPeek`, `look`), its chain's (`ChainGoals`: armed by
  the setter or about to be, 4 hops, `hops`) and the next step's (`NextStepGoals`, `look`) - Goal kind 4 (the mission's
  ships destroyed by label / nickname, `ShipIdsOf` by spawn position, else the pilot's name), the contacts, the leashes
  (`Leashes`), the step's towards nag (`TowardsNagOf`, 4000 m when the script gives none), the tethers (`Tethers`, reported
  only) and the declared next place (`NextObjectivePlace`, PSN). **(a) kill**: the nearest kind 4 ship that is spawned,
  alive, hostile by the game (not pre_aggro's overlay), targetable, not friendly, with no failing health gate, inside every
  leash (85 %) and not farther from the nag's target than the ship is; **(b) fly**: else the least `PathCost` among the
  placed kind 1 / 2 player goals not inside, leashed, nag-breaking or PSN-refused (straight flight at the cruise speed
  to the sphere, then on to the furthest-advancing candidate's sphere - by the goal's lane ride when its chain waits for
  one: `route::LaneBetween` / `RideTime`); else none. Events (once per change of the verdict - the key: step, kind, target,
  trigger, the blockers' codes; distances never): `autopilot look kill <ship>=<id> trigger= -> <leads> src=armed|chain|next
  dist= hull= gate= inv= ...`, `autopilot look fly trigger= -> <leads> src= cost=<s> to_go=<m> nearest=<trigger>/<s> ...`,
  `autopilot look none ...`, each with `text= verb= kills= flies= [nag=<name>:<m>/<radius>] leashes= tethers=
  [psn_next=<text>:ok|refused] blocked=<n>: <trigger>[/<ship>]:<code>(<detail>) ...` (codes: unspawned dead friendly
  pre-hostile not-hostile untargetable gate-fails leash nag psn unplaced inside); and per kill-goal ship under the
  script's Act_Invulnerable `autopilot look invincible <ship>=<id> trigger= script=<trigger>:true,<b>,<floor>
  game=<inv>,<b>,<floor>|unread|no-contact hull= gate=` - the game's own `space::Invincible` (GetInvincible) against the
  script (the M02 fact, answered 30 Oct 2026: no 0.5 floor against the player). `ap look` prints the verdict and every
  fact. Not acting: L1, L3, L4 (`LOOK_PLAN.md` 3). L2 (`look_ships`, acting on the kill verdict) was not built
  (`LOOK_PLAN.md` 8): no case reaches a detectable effect (M02's follow is a chase at equal cruise speed), and a
  `src=chain` kill (M02.rogues' escort2) must never be acted on before its trigger is armed.
- **ASH-START - the chase and the spawn stage (31 Oct 2026, not flown; `..\..\docs\research\runners\M02.md` "## Plan" 1):**
  two switches, both bool, default 0 until their verdict. **`chase_kill`** (Source::Hidden + Meta) acts on LOOK's kill
  verdict where L2 would not: in a step that asks no fight, dock or escort and points at no solar but has a navmarker or
  a target (M02 "Follow Ashcroft" 22262, verb None), LookTarget's `src=armed` kill (alive, hostile, targetable, inside the
  leashes and the towards nag) is fought now (`autopilot_objective.cpp` ChaseCandidate; the branch sits before the travel
  objective; the supervisor asks once per ship and step when his contact or hostility lags the step). The fight on him
  (g_obj.chaseShip) runs `lookahead::ChaseStep` per frame: a target no faster than our engine's top speed is a plain
  fight; a faster one is charged in cruise at once (the combat's approach, no 3 km floor, no drop at gun range); with him
  ahead (30 deg) 110-250 m off at >= 150 m/s in cruise the coast guard kills the engine (`autopilot_frame.cpp`
  ChaseCoast: Out::coast, the guns by their own rules) until the gap passes 0.9 x gun range, our shield < 0.15 or he is
  gone (30 s cap), then the engine back on and the cruise again (no brake). No missile / torpedo at him under 0.5 hull
  (HDE splash; 22355 if a round hits the pod). Events: `autopilot objective chase <ship>=<id> d_spawn= dist= speed=
  cruise= trigger=` (once per ship), `autopilot approach cruise ... chase target=`, `autopilot coast start ship= gap= v=
  his= bearing=`, `autopilot coast end (gap|shield|gone|30 s) ship= gap= v= his= after`, `autopilot chase recruise gap=
  speed= engine=`. **`spawn_stage`** (Source::Hidden; requires `chase_kill` - without the chase the stage gains ~0-3 s)
  stages in a Fight step whose armed kind 4 goal (armed in the live table when it reads) has one ship alive and the rest
  down (by spawn, `ShipIdsOf`) when its trigger spawns a later step's kill ship (`mission::SpawnAfterKill`: the trigger's
  own and at-once chain, 4 hops; the ship an armed Cnd_Destroyed names) that FLEES (`lookahead::FleeingSpawn`, PSN's
  exception: its first orders a GotoVec / GotoSpline >= kPsnRadius away, nothing aimed at the player, no other
  non-friendly spawn of the chain unplaced or within kPsnRadius). The point: `StagePoint` - 200 m behind the spawn on
  his line, 250 m beside it on the anchor's side, pulled into 0.8 x the fight's leash (`mission::LeavingNags`: the
  Act_NagDistLeaving rows this kill switches off by Act_NagOff - M02's nag_battle3, King, 5 km; an anchor not found
  refuses). Then (Action::PreStage, so the threat check leaves it alone): a trip there beyond 1.5 km, the last ship
  fought when within 1.5 km of the point, else a hold there with the nose along his run (StageNose); 25 s at the point
  without the kill = today's fight for the step (fail closed); the supervisor re-decides on a verdict change (1 s, with
  hysteresis). Events: `autopilot objective spawn-stage <trigger> <alive>/<need> alive -> <ship> spawn d=<P from the
  spawn> P=(x,z) anchor=<object> <d>/<leash> m free|clamped to_go= -> travel|fight|hold (flees <m>)`, `... spawn-stage
  <trigger>: none (<why>)`, `... spawn-stage <trigger> timeout -> fight`. Not covered: Cnd_DistShip leashes
  (`mission::Leashes`) are not part of the stage's clamp; a give by label (Act_GiveObjList = <label>, ...) is not read as
  a ship's first orders (refused as no-orders).
  **`leash_live`** (built 31 Oct 2026, **not flown**; bool, default 0, until a gate): the supervisor's distance leashes
  (the 80 % trip rule, the leash ship it keeps) only from the rows the game's live trigger table has armed
  (`mission::ArmedLeashes`); without the table, every listed leash as before. Why: M02's `need_to_dock` (3 km from King,
  m02.ini 2207-2211) is armed only by comm_1330_king at the mission's end but was kept through the Ashcroft step - as3 /
  as4's stage trips turned back at 2.4-2.5 km (06:43:38), and spawn_charge's point clamps to it ("far 2395 m").
  **Flown 31 Oct 2026 ~15:35, probe sc1 (n=1, f12b1dca+dirty with leash_live; chase_kill + spawn_stage + spawn_charge +
  leash_live) - run-judged BAD / FURTHER [M]:** ashcroft + pod 197.1 s (as4 177.8 / 184.0, PB 146.8, par 68). leash_live
  PASSED (the point free: clamp=free, anchor_d 2622; as3 / as4 turned back at 2.4 km). The decisive cause: not in cruise
  at his spawn - the stage trip switched to hold with 1305 m to go (15:45:33), the keep crawled at 24-33 m/s 24 s and
  never got within 400 m of P, the last escort (530-780 m off) died only 35 s after the third; cruise pressed +1.5 s
  from 26 m/s at 493 m (expect_lock 290 - outside the coast window); no coast; the gap then froze at equal 300 m/s
  until his run point (~t+91). Extra: a self-made "no enemies left" stop at 2,012 m (target-given-up), four 300 -> 97
  m/s drops with shield hits and no attackers (wing 80-83 nearby, unconfirmed), and two logging faults - the `bump`
  lines there were our own cruise-charge slowdowns, turn-evades with attackers=0. Fixed after sc1 (built, not flown): a
  charge stage's trip ends 150 m from P (`kChargeArrive`; spawn_stage's 1500 made the "arrival" 1305 m short), and the
  keep flies to its station at full throttle / burner beyond 300 m (`kChargeKeepFly`) instead of the crawl. Open: the
  target-given-up at 2 km in a chase (fixed 090bb269), the impacts' source. **Probe sc3 (2 runs, 8ed2e45c clean)
  run-judged BAD for the kill [M]:** 785.0 (= PB 784.4; ashcroft + pod 161.2 - CLOSER than sc1 197.1 / as4 177.8, PB
  146.8) and 817.6 (189.2). The stage ended 116 / 134 m from P, no give-up; i2 = the first coast ever: 111 m at 270 m/s
  vs his 268, he reached 300 and opened 34-48 m/s, the coast ended at 540 m after 10.2 s - his shield down at 9.4 s (~87
  HP/s; a kill needs ~4,421 HP: guns alone ~50 s), no Javelin (the missile run abandoned at 354 m); i1: the press 1.16 s
  late while fighting the escort at P, gap 265 at our 259 m/s - no coast. Kill still at his run point in both. Changed
  5e4e5078: the fleeing coast starts at 295 m/s (`kChargeCoastSpeed`, was 270). Open: missiles inside the coast (the
  1.5 s flight gate vs a target at 300 m/s), the escort left at full hull (the runners leave it at ~0.09 for the NPCs),
  "no way" for a 250 m point (i1), an unlogged cruise cut. **Probe sc4 (3 runs, c0f77a43: the coast at 295) run-judged
  BAD [M]:** no coast in any run, the kill at his run point in all 3 (+94..124 s after his spawn; runners 18-21 s). i3's
  771.7 is a new M02 leg PB (was 784.4) from the rogues (-25.4) and pod segments - spawn_charge contributed nothing (at
  his spawn 2.9 km off at rest). At his spawn: i1 1935 m at 96 m/s (the stage timed out, the last escort dragged us
  1.4 km + 1.1 km up), i2 255 m at 14 m/s (a head-on delayed the press to +1.75 s; at 295 m/s the gap was 464 > the
  window's 250), i3 2858 m at rest. **The one cause across sc1 / sc3 / sc4 (6 runs, 0 chase kills): we are never at
  cruise speed when he spawns** - the runners leave the last escort at ~0.09 hull to the NPCs and are already cruising
  near his spawn; 295 only made the coast start later. **spawn_charge PARKED (default 0)**; escalation step 2 (a data
  pass on the recordings: `..\..\docs\research\runners\M02_ashcroft_datapass.md`); the judge's next idea: the cruise
  charge on the "1/4 alive" verdict and the last escort left to the NPCs below ~0.1 hull.
  **Probe ar1 (ash_ride's first flight, 2 runs, 06f9f68a; driver read, not yet run-judged) [M]:** 763.5 (NEW M02 PB,
  was 771.7) and 783 s. ar1-i1: the escort left at shield 0 / hull 0.58, the NPCs killed it 9 s later; at Ashcroft's
  spawn we were IN CRUISE at 300 m/s 1.86 km off (the first time in any probe - the runners' state); the intercept
  closed to 189 m by +6 s (go / overrun "beside" while he was at 77-112 m/s), overran him, and the gap then held at
  702 m at equal 300 m/s for 80 s - outside the coast window (90-220), no coast, the kill at his run point. Next: once
  he is at cruise behind him at equal speed no gap closes - the station must be taken while he still ramps (arrive
  behind, not beside), or the overrun's turn must end inside the window. **Run-judged BAD [M]** (no coast in either run,
  both kills after his run point): i1 - the "overrun (beside)" fired at +1.2 s with lat 1468 m and path 1809 < need 2483
  (674 m early), we flew at him and crossed his course at 112-137 deg while he was at ~112 m/s (gap 1454 -> 141 m), an S
  cruise turn ended 703 m behind him at equal 300; i2 - the loop worked (463 m from its centre at 300 m/s), but the
  escort died 1.77 s before his spawn and the restart into Hold dropped cruise (me=0 at his spawn), gap 569 m held 80 s.
  i1's PB 763.5 is not from ash_ride (scan / cosmodone / marked faster; Ashcroft +17.4). Runners: R1 the same head-on
  start but its U-turn ended behind him while he still ramped (284 -> 213 m), R2 from behind. Next (the judge): gate
  overrun / beside on his line (lat < ~200-300 m) and wait / loop while path < need - 150; the Hold restart on the last
  escort's death must keep cruise. Proof: 2-3 runs, pass = a coast line + a kill < 30 s after the Follow banner.
  **`spawn_charge`** (built 31 Oct 2026, **not flown** before sc1; bool, default 0; Source::Hidden; acts only with `chase_kill` 1 and
  `spawn_stage` 1; the design: `..\..\docs\research\runners\M02_ashcroft_consensus.md` "The ONE design") changes the
  stage three ways. (1) The point (`lookahead.h` StagePoint over a leash list): 250 m behind the spawn on his line and
  70 m aside, on the side that needs the least pull, inside 0.8 x every leaving nag this kill switches off and 0.75 x
  every armed ship leash - the supervisor's own (`g_obj.leashShip`, the 3 km one that turned the as3 / as4 trips back)
  and the script's (`mission::ArmedLeashes` from the live table, else every `mission::Leashes` row) not disarmed by a
  heard comm. A ship leash's anchor not found: spawn_stage's point as before (fail closed); no point inside them all
  within 600 m of the spawn: no stage (the fight as without spawn_stage). (2) The last ship's fight at the point
  (`ChargeKeep`, autopilot_frame.cpp ChargeKeepStation): beyond 400 m of P the pursuit keeps station 200 m from P toward
  the escort (nose and guns on it) until back within 250 m. (3) The chase on the staged ship (his contact matched by its
  [MsnShip] nickname, the fact kept across the step change) while he is > 1.5 km from his run point (`ChargeFleeing`):
  ChaseStep's `fleeing` charges at once whatever his speed (from beyond 110 m, or on with a charge running), the coast
  starts only at 270 m/s (not 150: from the stage's gap the window is reached in our charge's ramp, where a 150 m/s
  coast is outrun at once), no missile at him beyond 600 m, and the supervisor looks for the chase every 200 ms while he
  is expected. The coast itself is unchanged. Events: `autopilot spawn-charge stage P=(x,z) d_spawn= leashes=<n>
  clamp=free|clamped bound=<leashes that pulled it> anchor_d= expect_lock=<lag 0>/<0.5>/<1 s> min= -> travel|fight|hold
  ship=`, `autopilot spawn-charge none (<why>) leashes= bound= -> fight`, `autopilot spawn-charge fallback (<why>) -> the
  spawn_stage point`, `autopilot spawn-charge keep on|off d_P= target= target_d_P=`, `autopilot spawn-charge cruise at
  spawn d= his= me= engine= from_spawn= bearing= expect_lock= min= target=`. The predictor (`ChargeGap`, the note's
  profiles) puts the lock at ~58 m for a press with his from 260 m, +150 m per 0.5 s of lag (tests\spawn_charge_test.cpp).
  **`ash_ride`** (built 31 Oct 2026; flown once, probe ar1, run-judged BAD: no coast - above; bool, default 0; Source::Hidden; acts only with `spawn_charge`,
  `spawn_stage` and `chase_kill` 1; the data pass's proposal `..\..\docs\research\runners\M02_ashcroft_datapass.md` adapted
  to its P0 (HDE: engine back on after an engine kill at cruise -> ~90 m/s, a new 8.5 s charge) and the owner's correction
  of 31 Oct 2026 (an engine kill out of cruise coasts straight on the velocity, cruise lost: no taps, a coast is a straight
  line, every turn under the cruise engine); pure half `..\..\modules\autopilot\src\ash_ride.h`, adapter `autopilot_ashride.cpp`,
  `tests\ash_ride_test.cpp`) changes only what happens around the fleeing ship's spawn. Armed when spawn_charge's stage
  point holds and a loop fits every leash; then the stage's verdict is the fight on the last ship wherever it flies (no
  trip / hold / keep station / 25 s timeout). (1) **Leave:** its shield 0 and hull <= 0.6 (the runners 0.36-0.59) -> the
  ride; back to the fight when it still lives 35 s later or we pass (frac + 0.04) of a leash moving outward. (2) **The
  loop:** at cruise round a centre on his line r + 200 m behind the spawn (400-800 m band, pulled into every leash shrunk
  by r, its farthest point <= 2 km from the spawn); r = 2 x the cruise turn radius = 2 x 300 / (0.74 x the model's turn
  rate): 0.74 = the measured top velocity turn at >= 280 m/s (1.23 rad/s, 120k two-second FLMotion windows of 30-31 Oct)
  over the Starflier's 1.667 -> 486 m, a ~10 s lap; M02 with King's 5 km nag: centre 1,132 m behind, farthest 1,619 m (the
  3 km King leash at 0.75 refuses it, as spawn_charge's point). The combat on the escort runs underneath (no dodge, no
  give-up, no "no enemies left" stop / hold / loot, a stop keeps the cruise). (3) **Intercept** (his contact chased):
  wait on the loop while our way to the station 160 m behind him where he reaches cruise (his profile + his live speed)
  is shorter than what we fly in his time to cruise (our charge counted) - 150 m; then at cruise to it by a ~45 deg join
  (full-rate turns beyond 0.12 rad); beside / ahead of him: pass and turn at cruise; he at cruise and we behind within
  600 m / 200 m off his line: the nose on him, cruise kept. Closed loop on M02's geometry: 31 / 32 start phases reach the
  coast window at the measured 1.23 rad/s turn, 25 / 32 at 1.1 (a slower turn than counted arrives late). (4) **Coast**
  (ChaseCoast, the start under this gate): he >= 295 m/s, we in cruise >= 290, the gap 90-220 m within +-10 m for 1 s, he
  within 6 deg of our velocity line and his course within 6 deg of ours; between coasts the intercept steers at cruise.
  Every rule's number is a named constant in `ash_ride.h` with its source. Events: `autopilot ash-ride armed <trigger>
  escort= loop center= r= d_spawn= farthest=`, `... ash-ride none (<why>) -> spawn_charge`, `... ash-ride leave-escort
  hull= shield= escort= d_spawn= me= cruise=`, `... ash-ride loop center=(x,z) r= d_spawn= farthest= dir= d_center=`,
  `... ash-ride spawn his= d= d_spawn= me= cruise= since_leave=`, `... ash-ride intercept wait|go|overrun|track (<why>)
  t= his= me= cruise= charge_left= gap= along= lat= path= need= t_ready= station=`, `... ash-ride coast gap= his= me=
  vel_off= parallel= stable=` (then `autopilot coast start`), `... ash-ride fallback (35 s | leash <name> | no spawn seen)
  -> fight | spawn_charge`, `... ash-ride paused mode=`, `... ash-ride end (gone | not fleeing | objective off)`.
- **TIMER-DESTROY - a "Destroy ..." step the script ends by itself is not fought (30 Oct 2026, da3e794;
  the private work queue TIMER-DESTROY):** **`timer_destroy`** (bool, default 0 until its verdict). The script reader
  `mission::StepAdvanceOf` (`core\game\mission.h`) names how a step advances: forward from its objective's setters and the
  triggers armed beside them (6 hops of Act_ActTrig, never past a new objective or a FAIL) - **kill** when a path to the
  next objective runs through a Cnd_Destroyed / Cnd_HealthDec, **timer** when none does and one runs on Cnd_Timer /
  Cnd_CommComplete / Cnd_True alone, **other** otherwise. Over the install (`StepAdvanceSweep`, tests\look_test.cpp `-v`):
  34 timer steps, two of them "Destroy ..." (the verb gate): **M13 24420 "Destroy the Nomads"** (cap_timer ->
  trent_go_back 15 s -> ether_one, Orillion's 0460 -> go_home; watch_cap2 only hides Cap2) and **M01b 22080 "Destroy the
  Hostile Fighters"** (the scouts flee on dst_scouts_run_safeguard's 8 s timer); M13 24425 and M09 23635 read kill. The
  verdict is pure (`lookahead.h` TimerDestroy): with speedrun knowledge (`knowledge 1`) the ship flies to 300 m short of
  the advancing path's next objective's point (PSN: never a destroy objective, never within 3 km of the step's own
  navmarker), rests there (Action PreStage: the threat check leaves the trip and the rest alone) and answers only a
  hostile ship shooting at it within 1200 m at the place (combat under `g_obj.defendOnly`: the target filter in
  `autopilot_target.cpp` takes nothing else - the marked battleship stays scenery); without it (`knowledge 0`) the fight
  goes on while hull >= 0.6 and batteries >= 7 (Q4's 300 m predictor), else rest / defend. The supervisor re-reads the
  verdict every second (not while defending: that combat ends by itself). Event `autopilot objective timer-destroy <text>
  -> stage|rest|defend|fight why= advance= next=<objective>(<path>) place= hull= batt= attacked= speedrun=`, once per
  change. Not read yet: Toledo's dock unlock at trent_go_back (~5 s before 24430; the runners dock on it) - the next
  lever; the arbiter (`situation_arbiter`) has no rule for it (with `situation_shadow` on, its steps differ while the
  switch is on).
  **In game (30 Oct 2026 late evening, set td1, run-judged GOOD):** S415 -> M13.sinclair, speedrun preset: A (switch off)
  57.6 / 57.5 / 56.9 s - each fought NomadCap2 ~20 s (8 torpedoes, ~20 missiles, 3-5 batteries; Cap2 never dies), flew the
  4.9 km only at 24430, one lost hull 0.96 -> 0.77 on the way (the look0 pattern); B 27.4 / 28.0 / 26.8 s - staged within
  0.4 s of 24420, no shot, 0 hull lost (one entered at hull 0.59 and 0 batteries). A fourth B died in the first
  battleship (24425, A's code path: StepAdvanceOf reads kill there) - `splits.py sprt` books such a run as a DNF of the
  later target segment (CONTINUE, LLR 0.12, sigma 21): excluded by the judge, B 27.4 +- 0.6 vs A 57.3 +- 0.4 s. PAR: about
  at par like-for-like (our 24440 fires in the base 0.5 s after the dock, the runners' boundary is the relaunch: B ~32 s vs
  par 33.2, R2 24). Full leg td1full from Save3989: 24420 at 0 batteries -> stage, one defend near the ring (attacked),
  M13.sinclair 31.5 s by the split, 36.3 s to the relaunch like-for-like (par 33.2; hull 1.00 -> 0.79 leaving Cap2 from
  631 m with 0 batteries, likely its turrets - not proven); run-judged BAD as a leg (it died in the Dyson city, 833 s, not
  caused by the switch: batteries spent on fighters at the gate and the barrier). **Found: the 25 s cap_timer gap before
  24420 (no objective shown) still chases NomadCap2 - 6 torpedoes from 1,318 to 706 m; the runners head for Toledo in it.**
  Left: that gap (its last real step 24425 reads kill, the hidden step's own setter is cap_timer); dock on the unlock; the rest stops up to
  kTdArrive (700 m) short of the stand-off point (td1-b 2124: 872 m from the ring); the cruise charge at 80 m/s for 6 s
  where the runners afterburn; `defend` / staging through live interceptors only seen once; M01b 22080 not run.
- **ALLY-KILL - a kill anyone may make that unlocks the next dock is not fought (31 Oct 2026; design
  `..\..\docs\research\runners\M13_gate_allykill.md`):** **`ally_kill`** (bool, default 0 until its verdict;
  Source::Hidden - never under `knowledge` exposed). `mission::StepAdvanceOf` now also gives, for a **kill** step, the
  kill path's destroy trigger as a lone Cnd_Destroyed: its mission solars (`killObjects`), `anyKiller` (no 4th field -
  `Goal::killer` holds that field, "" any; M06 d_dexter = ..., Player) and `unlocks` (the first `Act_LockDock = Player,
  <obj>, unlock` on the kill path from that trigger on or in what it fires at once, 4 hops). The rule holds when the kill
  is on mission solars, counts any killer, unlocks the next objective's object and the next step's verb is Dock - over the
  install only **M13 24460 "Take out the Nomad Ships"** (check_cap = NomadCap3, 1, ALL unlocks St01_to_St03_hole ->
  lead_through_gate 24465; the sweep line's `ally-kill=1`). The verdict is pure (`lookahead.h` AllyKill, TdResult):
  more than 300 m from the dock (`space::ObjectPos` of the next objective's object, the hole itself) a point trip to 150 m
  short of it (Action PreStage, no dock: the locked hole answers dock_denied_locked; defend when attacked within 1500 m),
  at the dock rest (defend when attacked: combat under `g_obj.defendOnly`); after 20 s at the dock with no friendly ship
  targeting the solar and < 0.05 of its hull lost: the fight as before (kept for the step); the dock itself is the next
  step's own decision on the unlock. Decided right after `timer_destroy`, before the mark fight / the big coast; the
  supervisor re-reads it every second (from today's verdict once per step: a decision the coast guard swallowed);
  `battery_budget` leaves the target out of the big fight now and its group out of the fights ahead while it is not
  fought. Events: once per kill step `autopilot objective ally-kill <text> data: watcher= any-killer= unlocks= next=<obj>
  (<text>) solars= next-object= shape= next-dock=`; per change `... ally-kill <text> -> stage|rest|defend|fight why=
  target= hp= allies= dist= waited= drop= hull= batt= attacked=`. Not flown; the arbiter has no rule for it.
- **M13-MIMIC switches (30 Oct 2026 late; plan `..\..\docs\research\runners\M13.md`, WORK_QUEUE M13-MIMIC), all default
  0:** **`escort_by_place`** (45a7653) - an Escort step whose live table holds a player distance goal (`mission::
  EscortByPlace`: the armed rows' goals with the player as subject; refused while an armed Cnd_Destroyed row fails at once
  or a leash is armed; fails closed without a table) is flown straight to 70 % of that goal's distance (Action ScriptGoal);
  set mm2: M13.osiris 108.8 s x3 vs A 122-141, run-judged GOOD. **`td_window`** (a72d86b, with `timer_destroy`) -
  TimerDestroy from the kill before the step (a placeholder after a "kill" step whose watcher is no longer armed and whose
  next objective is a timer "Destroy") and a **Dock** verdict when the script takes the next objective's dock already
  (`mission::ArmedBaseEnters`: an armed lone Cnd_BaseEnter row leading on; the dock target = StepAdvance `nextObject`, a
  rep_inst's object); set mm3: M13.sinclair 20.5-21.1 s vs A 25.3-32.1, run-judged GOOD. **`big_coast_own`** (a72d86b) -
  the CoastGuard (`big_coast`) coasts only at the fight's own target, a "Destroy" step's marks or the trip's destination
  object (set mm1: from the formation / a trip it opened 78-99 s fights on NomadCap2 / NomadCap4 and the escort nag failed
  3 of 3 runs). Event lines: `autopilot objective escort-by-place <text> -> <trigger> (<leads>), <m> to go | none: <why>`,
  `timer-destroy 0 -> stage` (the gap), `timer-destroy <text> -> dock why=unlocked`.
  **Items 3-6 (30 Oct 2026 late night; all default 0; verdicts below as they come):** **`kill_ahead`** (bb34216) - a step
  with no place of its own whose script destroy goal is a later step's (stale_destroy_wait skips it: M13 24470 "Head to
  the Barrier", the generators unmarked) flies to the nearest object as a PreStage trip (no threat detour) and fights it
  the moment `mission::KillAheadSafe` allows the kill: every Cnd_Destroyed trigger the kill fires armed, none failing,
  none switching off an armed trigger that sets a new objective (generator1_down deacts juni_near_center: before Juni is
  within 3.5 km of the centre the kill would leave the step without "Destroy the Generators"); its fight-step form makes
  the NEXT step's destroy target the fight's mark once it is alive and safe (M13's nomadcap from its spawn, 36.9 s after
  the launch, not the 49.3 s banner). Events `objective kill-ahead <trigger>: <id> at <m> (kill <n> of <label>) -> fight |
  wait: <why>`, `kill-ahead ... (next step's, in fight <text>)`. **`battery_budget`** (bb34216) - `mission::FightsAhead`
  (the Cnd_Destroyed goals on mission solars between the step's first setter and the next base dock's, in trigger order;
  timer steps out): at a big hostile static (our target, or a marked one within 3 km) the batteries the window began with
  are spent up to its share 1 / (k + 1), k = the other groups ahead; on fighters none unless death is near or the hull is
  under 0.5; nanobots keep k / (k + 1) above half hull. Event `battery-budget text= groups= big= k= window-start= keep
  batteries= nanobots= now=`. **`standoff_hold`** (+ `standoff_slide` 60 m, `standoff_slide_speed` 20 m/s) - the firing
  site (ordnance_site's spot >= ordnance_standoff off the hull) is the one hold for guns and launchers, reached at rush
  speed, held with a circle round it across the line of fire, the nose on (event `standoff-hold`); measured first: the
  exposure at the Nomad battleship is 361 hit points a second crawling within 700 m of its centre, 169 moving, 143-229
  moving at 760-900 m (248 traces; `..\..\docs\research\runners\M13.md` Plan 4). **`hold_guns_hull`** - static_hold_guns
  counts the guns at the hull point they fire at, not the centre (td1's city hold stepped out to 549-562 m from a city
  generator's centre, ~430 m behind its hull, "guns 1-2 of 5"). **`place_dock`** (with escort_by_place) - the place that
  ends the escort step is the next step's dock with the same object (`mission::ObjectiveObjectOf`: in_osiris_range ->
  dock_with_osiris, osiris; Act_PlayerCanDock = false, osiris leaves it the one dock allowed) - the trip is that dock.
  **`hold_shelter`** (adcea3f; item 3b, the owner's barrier directive) - the static hold's candidate points also pay 40
  per open direction (26 rays to 400 m without a solid: fighters move, the shelter is the structure); `static-hold
  target ... open=`. **Verdicts so far (31 Oct 2026 ~00:10; all still default 0):** `kill_ahead` run-judged GOOD on the
  sphere (mm6 B 83.2 / 76.7 s vs mm4-a 98.9-111.9, CLOSER by ~27 s; generator 1 dead 16-21 s after 24485 vs 39-48 s;
  no 24470 fighter detour) - the fight starts 7.3-7.7 km out ~18 s before the banner, its first torpedo 1.2 s after it.
  `battery_budget` run-judged BAD as built: at the gate it held 20 batteries with the shield at 0 until death-soon (hull
  1.00 -> 0.40), the city reserve bought no time - rework: release past the share on a failed shield under fire.
  `standoff_hold` (mm5 / mm7, 15 runs) run-judged BAD on time (bsdown 86.8-103.3 vs A survivors 74-90: the rush fires 3
  rounds where A's crawl fires 8 and arrives with the battleship at ~94k vs ~56k; the site's rounds would burst 46-66 m
  ahead, half held), SAFER (0 deaths). **`blind_no_pass` alone** (mm8, 9 runs): bsdown 77.1-95.2 (median 79.3, A's
  ~80), 0 deaths; with mm5 / mm7 0 of 24 vs A 4 of 17 (one-sided Fisher ~0.02) - every A death followed `static-pass
  out` (the owner's pull-away reading); sprt CONTINUE (time-only). Not flown alone yet: `place_dock`, `hold_guns_hull`,
  `hold_shelter` (set mm9, combined, running at hand-over).
  **Bundles A + B (AFK stage 1, 31 Oct 2026, bafeb64; all default 0):** **`battery_budget` reworked** - the share of a big
  fight is weighted by the kills the window's script waits on (`mission::FightGroup.kills`, the label's highest
  Cnd_Destroyed count: M13 window B NomadCap3 1, the barrier's generator 4, the city's final_generator 2 -> ~5 / ~16 / the
  rest, R1 7 / 18 / 4); while fights lie ahead a big fight refills only on a failing shield (no top-ups above the offline
  threshold: mm9 without the budget spent 29 -> 5 at the gate in presses from shield 0.70 at hull 1.00); a failed shield
  under fire below hull 0.6 releases the share (the first build held 20 with the shield at 0 to hull 0.40). Event
  `battery-budget ... kills= ahead=`. **`static_full_dps`** - at a static target the overkill rule holds a missile only
  when the rounds already homing will kill it (OurDps holds the launchers already: over a 9-16 s flight mm9's city missiles
  stopped at 51k of a generator's 75k; `dead-first=1` in ~40 % of the barrier / city salvo checks). **`bs_approach`**
  (+ `bs_approach_gap` 100 m; 4c, the owner's battleship approach) - the run to a blind spot at afterburner speed down to
  the braking distance (the rush, launches kept - the set runs `launch_site_check 1`, margin 55: our torpedo cost us its
  2,424 bursting 80-95 m away), and BlindSpot's 8 best spots pay 6 a metre their line to the target's centre falls short of
  the gap (a round holds the whole target and bursts at the first hull it meets). (history: AUTOPILOT_LOG.md "Sets ab1-ab4")
  **62c0730** - past the share a failing shield gets 3 units (back online, not a full bar); **f85b9d8** - the big fight in
  window A counts only within 2 km of our target (run-judge on ab2: 29 spent on interceptors 2-5 km out);
  **`big_cruise_in`** (f85b9d8, default 0) - the cruise in to a huge static (radius >= 500 m) goes on under fire (ab4-b_i1
  dropped its charge on the first evade at 4.5 km; ab2-b_i2 / i4 crawled 4.9 -> 1.3 km at 80 m/s): ab6-b_i4 at the spot 6
  s after the banner, yet the kill at +43.7 - the hold's launch gating sets the kill (11 torpedoes / 13 missile taps in
  44 s vs a refire ceiling of 22 / 22; ceiling ~4.5k hp/s, measured ~3.2k); **`bay_request_lined`** (8d07573, default 0) -
  a bay's request only within 40 m of its axis (mm9-b_i2 requested 300 m off it at the Osiris, the game's dock flew the
  rest in 10 s; ab4-b_i3 sat 3 s at rest 460 m from p2 side-on, orillion 50.3). (history: AUTOPILOT_LOG.md "Sets ab4-ab8") **32410f6** - off the axis the brake keys on the way to p2. (history: AUTOPILOT_LOG.md "Sets ab4-ab8") **`blind_site_check`** (0982383,
  default 0): its probe bsc0 died at +38 s - a torpedo that passed the simulated check (need 57 m) burst 94 m from us, inside
  our measured 80-95 m reach; 4ab7c12 keeps only the torpedo hull-beside hold (unverified). The next package's first test
  (the judge): ~8 runs S415 -> M13.bsdown with no torpedo whose burst can come within ~100 m of us, vs mm9.
  **Package AFK2 (02:30-):** **`torp_reach`** (d91c2cb, default 0; m) - no torpedo at a big or static target unless the hull
  along the line and the nose is at least this far. (history: AUTOPILOT_LOG.md "Sets ab9-ab11")
  **`missile_reach`** (default 0; m) - the same for missiles; both now log `launch-held target=<id>: torpedo|missile reach
  <m> m, hull gap <g> / nose <n>` when a refired round is held. From the driver's M13 log analysis (ab3-ab8, 31 Oct;
  ..\..\..\AUDIT_2026-10-31.md "M13 analysis"; all default 0): **`self_burst_near`** - self_burst learns only from rounds
  launched within 400 m (ab8-b_i3 learned "inside 1201 m" from one at 1,051 m during a coast, then 19 holds, bsdown 74.6);
  **`arrive_walled_fast`** - the "close, no progress" arrival after 2 s at cruise speed (every city trip orbited the arrive
  point ~7.4 s at 290 m/s); **`static_site_check`** - the hull-beside torpedo hold at static holds (both city deaths: own
  torpedoes with a hull 31 / 51 m off). Checked and not built: `kill_ahead_stage` (the wait branch already travels to 600
  m short of generator 1; it arms 30 s in with 7.7 km to go), `dock_alt_bay` (whether a re-request near Toledo's other dock
  point gets it is unverified: probe first), the airlock kill order (the analysis: no effect; two slow runs bumped one solid).
  (history: AUTOPILOT_LOG.md "Sets ab9-ab11") So **battery_budget's rhythm now holds at every big fight** (no top-ups above the offline threshold, bursts on failure),
  not only while fights lie ahead - the runners' 9 / 10 / 8 at the hull. (history: AUTOPILOT_LOG.md "Sets ab9-ab11")
  **`city_hold_side`** (31 Oct 2026, default 0, **probed ch1, set pending**; pure half `..\..\modules\autopilot\src\city_hold.h`, test
  `..\..\tests\city_hold_test.cpp`) - why: ab11's deaths above (31-67 m off the outward face, 100-163 deg from the city
  centre, in front of the platforms; the runners stay inside the model or ~200 m out). What: at a static hold the guards
  are found from the live objects (`GuardPlatforms`: a hostile solar - attitude <= -0.6 - whose guns the game's turret
  test `space::GunsCovering` says can point at and reach the target's centre, or a WEAPONS_PLATFORM (type bit 1 << 12)
  within 3 km of its hull; `..\..\docs\kb\combat-targets.md` Weapon platforms); the hold's candidates lie within +/-45 deg
  of the horizontal direction away from their centroid (0 / +-22.5 / +-45 at the target's height, two tilted 15 / 30 deg
  below), at a 120-130 m hull gap (HoldGap clamped; static_hold_guns' steps skipped), scored as today plus 30 per platform
  gun that can point at, reach and see the point (GunsCovering + SightBlocker: the platform-cover score); a deeper point
  only when that score is clearly lower (>= 2 guns and a quarter fewer). No re-anchor onto our own line unless that line is
  on the side (the gap it sets >= 100 m); the slide stops / the lateral run reverses at the side's edge; missiles,
  torpedoes and mines only with the predicted burst (the launch site's RoundImpact, shared `impactOf`) and the nearest
  hull (line, nose, six 100 m axis rays) >= 100 m off. Events: `city-hold side=<deg> gap= platforms= target= cover=
  band-cover= deep= dy= deep-cover=a/b`, `city-hold off target= platforms=0`, `city-hold re-anchor refused ...`,
  `city-hold launch-held target= kind=torpedo|missile|mine impact= hull= need=100`. [I] the cover score's real reach
  (CanPointAt ignores the turrets' current aim; SightBlocker sees real hulls only) is unmeasured at the city.
  (history: AUTOPILOT_LOG.md "Probe ch1 and set ab12") So **the
  transit** (31 Oct 2026, under the same switch): near the target (centre distance < the point's + 400 m) and off its
  point, the station keeping carries the ship to the point with the nose on the target - straight when the line is
  clear, else round the centre in 35 deg steps at the point's distance (`cityhold::TransitStep`, unit-tested on this
  geometry); event `city-hold transit target= left=<deg> to-point= centre= round|straight`. **Probe ch2** (3 legs, build
  19cf6932) run-judged BAD for it [M]: all PASS (780.8 / 788.0 / 776.0), M13.end 161.1 / 158.1 / 149.7 (gold; ch1 167.3)
  - but the side was reached in 1 of 3: the transit's station output sat above the off-point branch, which turned it off
  again (296 of 374 frames at final_generator5 `pursue`, throttle -1, the ship backed against the hull); moved into
  that branch after the hop (31 Oct 2026, not flown yet). The clearance did not tell the sides apart (`clear=72-82
  held=5/7` at both generators: every side point there has a city hull within 100 m). The second
  generator's hold was on the side (22-37 deg, gap 113-124) but its point sat 75-87 m from another city structure: 7
  torpedoes held over 18 s. So the candidates now also score their
  **ordnance clearance** (the launch rule's six 100 m rays from the point; a hull inside them +2000, above any cover
  difference, below the path / structure penalties; the side line logs `clear= held=n/7`) - not flown yet.
  **`missile_keep_big`** (31 Oct 2026, default 0, not flown; needs battery_budget's look-ahead): probe ch1 fired 46 of
  its 50 Catapults by the sphere fight (`free-use` at fighters 150 m out), none was left for the city's generators [M];
  the runners fire every launcher at the generators and ignore the city's ~20 fighters (`runners\M13.md`). While the
  look-ahead (`mission::FightsAhead`, the kills of the big fights still ahead incl. the current one, published 1 Hz as
  `g.combat.fightsAheadW`) holds any, a small ship gets no free-use missile - only the need path (the odds) fires at it;
  big ships and static targets keep free use. Event `missile-keep target= w= stock=` (5 s).
  (history: AUTOPILOT_LOG.md "Probe ch1 and set ab12") **Probe kb1** (8 runs from S415, standoff_hold at
  120, arms still / sliding) BAD [M]: kills +47..+96 (goal +32), the site never held - every run picked local 0,0,233,
  "gap 214", a point inside the battleship's envelope that the centre ray read as clear; the ship stalled 7-50 m off the
  hull. Fixed (31 Oct 2026): a fire site needs a ray from well outside to reach it and its six stand-off rays clear (event
  `fire-site rejected`); probe kb2 (2 runs, 75 / 78 s) then picked NO site - none of the 8 best was both >= 120 m and
  clear - so the pool is the 24 best since. And the city transit's round steps are flown at full throttle on the
  afterburner (the nose off the target meanwhile), only the straight last part by station keeping. Both not flown yet.
  **Run-judged 31 Oct 2026 ~14:40 (the batch 06:11-07:12):** set ab13 (ab12 + `ally_kill`) **BAD** on time - median 817.6 vs ab12's 774.8 (three builds in the set, f1ae6d75's transit in it too: bsdown / end confounded); ally_kill itself worked as designed (stage -> defend -> rest, the allies kill the gate battleship in 8-14 s, 29 batteries kept to the gens) but its barrier segment median 90.2 vs ab12's 84.9: **ally_kill BAD** (kept 0); its stall fallback never fired: UNPROVEN. Correction: ak2-i4 was NOT a 3-min rest - after 24465 (dock with the jump gate) the travel failed 154 times 'the game found no way (locked gate?)', a dock-refusal LOOP (a bug to fix: retry backoff / the gate's unlock), and the heartbeat did run at Rest on 14afc899 (3e1e27ef's move is harmless but was not the cause). ab13's 53 s death: 29 batteries held through a shield failure at Toledo (the budget), a game result. Runner 2 may dock the gate while the battleship lives - check its frames. **chase_kill + spawn_stage BAD** (M02 B median 1152.8 vs A 1112.6; never inside gun range: no engine-kill coast, 0-186 m/s at the start).
  The gate battleship's 20-23 batteries (ab11) are not waste [M]: each 3-press revived the failed shield (0.08 -> 0.39
  at hull 1.00; Common.dll CEShield::ShieldBatteryBoost clears the offline timer, disasm) - the lever there is the
  exposure, not the battery rule.
  **`strafe_pass`** (31 Oct 2026, built, NOT flown yet; escalation step 2, `..\..\docs\research\runners\M13_bsdown_datapass.md`
  "Proposal"; Source Hidden + Meta; pure half `..\..\modules\autopilot\src\strafe.h` + `tests\strafe_test.cpp`, adapter `StrafeFly` in
  `autopilot_combat.cpp`) - at a big armed hull (4+ guns by `space::GunsCovering`, not dockable, still, and by the game's
  ray test >= 600 m along its longest local axis and >= 1.8 x its width; the M13 battleship is a solar, so "ship" means
  this class, and a CShip's hull - no solid to the ray test - falls back) no hold: lengthwise passes along that axis, the
  line 200 m off the hull's width profile (9 stations a side), the gap = the smaller of a live ray from us to our
  station's axis point and the profile distance, held in 150-250 by the slide; full throttle with afterburner pulses 1.0
  on / 0.6 off (~200 <-> ~80 m/s); the nose on the hull 18 deg ahead (a point the ray confirms; guns `SetGunAim` on it,
  the launchers aligned by it - every launch rule unchanged: at >= 150 m the line gap, the nose ray and the hull-beside
  rays pass torp_reach 100); off the hull 175 m back on; at max(1 km, end + 400 m) a turn onto the far side's line (the
  same / the two across if its rays meet a solid). Fails closed (`strafe fallback why=`: axis / guns / dockable / moving /
  geometry - all lines blocked or a live gap < 80 m - / join 25 s / turn 10 s / pass 30 s) to today's fight for that
  target. No battery stop (doses are tapped while moving). Events `strafe start target= axis= len= gap= side=`, `strafe
  at` (1 s), `strafe pass n= gap= speed= launches=` (each pass's means, min gap, speed min / max, aim share), `strafe
  turn at=`, `strafe end`. The unit test's crude flight (an ellipsoid of its size, 80 s): 4 turns, min gap 137, the nose
  on the hull 32 % of pass time, 81-200 m/s.
  **Flown 31 Oct 2026 ~14:52, probe sp1 (bd5d7af6; ab12's switches + strafe_pass 1, standoff_hold 0) - run-judged BAD
  [M], PARKED (default 0):** 4 runs S415 -> M13.bsdown 87.7 / 104.8 / 101.7 / 105.0 (kb3 76.6 / 70.7 / fail / 75.4; seg
  par 45.0), banner -> kill 59.8-77.0 s (kb3 42-48, runners 20-31); the full leg (+ ally_kill) 96.5 s there -> FAIL STATE
  at bsdown (171 s). It flies as designed (no fallback, pass means 187-242 m, min 114-147) and is SAFE (hull 1.00 in all
  5, kb3 0.48 / 0.17 / 0.004 + 1 death) - but the guns are on the hull 9-17 % of the time (on-target 17 % vs kb3 48-61 %),
  14-34 launches held (aligned=0), a 20 s join from 4 km, ~40 % of each cycle beyond the ends and in turns. sprt vs kb3 /
  kb4 INCONCLUSIVE. Not proven: why the nose ray loses the hull mid-pass at a 200 m gap. If it returns: the nose on the
  hull through the whole pass, turns at the end + ~150 m, judged on aim >= 40 % and <= 3 held launches.
  **Two bug fixes (31 Oct 2026 ~15:00, the AFK run's NOW 1; built, decision_check unchanged, not flown yet):** (a) a
  Dock step's trip to a gate / hole / ring the script keeps locked ("the game found no way") now rests and asks again -
  every 4 s for 15 tries, then every 10 s (event `objective dock <id>: no way (locked gate?) - again in N s (try n)`);
  before, transit objects had no `lastDock`, so the base back-off missed them and the trip was re-decided every ~1 s
  (ak2-b_i4: 154 tries at M13's "Dock with the Nomad Jump Gate" to the leg's limit). (b) `battery_budget` at a small-ship
  fight with big ones ahead (the keep is the whole window there) gives a failing shield under fire (the shield's own
  offline threshold, hull >= 0.5) a dose of 3, at most 6 of the window's batteries for small fights (ab13-b_i6 held
  29 through a failing shield among Toledo's interceptors and died in one frame at 53 s).
  (a) flown: probe fx1 (S450 -> M13.barrier, ab12 + ally_kill 1, bd5d7af6) re-ran the 1 s loop in 2 of 3 - a gate /
  hole trip carries Action::Jump, not Dock -> 44659916 widened the branch to both. Probe fx2 (44659916; build_info
  "+dirty" = only the lint writer's untracked hud2_lint.h, not compiled in): the back-off fires (i3: tries every ~5 s), 2
  of 3 PASS, **i3 still STALLs** (harness 60 s, dock-wait): the game's best path to the gate stayed empty 60+ s after
  check_cap's `Act_LockDock ... unlock`. **Open, inferred (game-facts, m13.ini 1838-1843 / 1957):** `wait_for_trent`
  (Cnd_DistShip inside Player Orillion 320 -> Act_LockDock lock) fires after check_cap when our "Follow Orillion" trip
  flies to the gate and meets him only there - the gate then stays locked to the mission's end. (b) not reached yet.
  (That inference is refuted - see lock_trap below: S450 holds the lock with wait_for_trent already fired.) **Direct
  fallback (31 Oct 2026 ~16:00, built, not flown):** a travel whose best-path request comes back empty for a dock / gate /
  hole / ring of this system within 8 km (`travel no way plotted to <id> (<d> m): straight to it, the dock request
  decides`) plans a one-entry list to that object instead of stopping "no way" - the game's dock answer decides, a
  refusal is retried by the objective; independent of why the planner returned nothing. **Flown: probe fx3 (S450 ->
  barrier, ab12 + ally_kill 1, 4e962485) 3 / 3 PASS (167 / 169 / 182 s)** - i2 got the empty path 256 m from the gate and
  the fallback took it through (`no way plotted ... (256 m)` then `plan jump`); fix 1a verified [M]. Likely cause [I]:
  the planner's same-system search takes a 500 m range (Content.dll FUN_06f5e910, game-facts 31 Oct) - resting at the
  dock within it, the request returns nothing (fx2-g_i3 rested there too).
  **That inference does not hold for fx2 (checked 31 Oct 2026 ~15:45):** SaveS450 (M13.orillion, the probe's start, 24455
  shown) holds `locked_gate = 2926433351` and the [TriggerSave] rows `in_hole_range`, `near_nomad_gate`, `pause1` (CreateID
  via `tools\flhash.ps1`) - `wait_for_trent` fired before the save and is armed by nothing else (only
  `wait_until_after_launch` arms it), so i3's empty path after `check_cap` has another cause (open; i1 / i2 decided the same
  trip 0.34 / 0.25 s after 24465 and planned `jump` at once, i3 0.16 s after and got "no way" in 2.4 s).
- **`lock_trap`** (Bool, default 0, Source::Hidden; 31 Oct 2026; built, unit-tested, not flown): the order fact above as a
  general rule. A lock trap (`mission::LockTrapsOfLive`, core\game\mission.cpp) = an armed live row in the player's system
  with a lone `Cnd_DistShip = inside, Player, <obj>, R` or `Cnd_DistVec = inside, Player, x, y, z, R` that runs
  `Act_LockDock = Player, <dock>, lock`, while the dock's unlock is still to come: an armed row other than the trap, or a
  trigger such a row arms within 4 `Act_ActTrig` hops (never through the trap, never into a complete row), holds
  `Act_LockDock = Player, <dock>, unlock` (M13: `in_hole_range` -> `check_cap`). A lock with no unlock to come is no trap -
  met first it would close the dock for good (M12 `lock_lair_when_player_exits`: its unlock fired before; M11
  `sye_Li01again`: the unlocks only through its own chain; M12's reversed `Act_LockDock = <obj>, Player, lock` is not the
  player's form). The objective layer (`LockTrapDecide`, before every other rule of the decision after the leash) flies to
  0.7 R on our side of the nearest trap's ship / point when we are beyond 0.8 R (the driver's ~0.8 R; 0.7 R = the script
  goals' arrival factor), a PreStage trip (the threat check leaves it alone); the ship found by id, else by its pilot's
  name (the leash's way), a moved ship re-decided at the trip's end. Inside 0.8 R it lets the trigger fire and never flies
  to that trap again for this story controller (a row still armed while we were inside is not what we think). Fails closed:
  no live table, no trap, no anchor (event `... not found - today's decision`). Events: `autopilot objective lock-trap
  <trigger>: within <R> m of <ship> (locks <dock>, unlock by <trigger>) - <d> m now`, then `... satisfied (fired or
  disarmed)`. Unit test `mission_lock_trap` (synthetic M13 / M12 / M11 shapes + the install's m13 / m12 / m11 rows).
  Also new (the retro's check): every objective start of its own logs `autopilot objective start action=<Action>
  mode=<mode> target=<id>` right after `autopilot start mode=...` (Start prints that line before the action is known; the
  new prefix matches none of the `autopilot start mode=` parsers - apdiff.py, flsweep.ps1, flctl.missions.ps1;
  decision_check.ps1 reads no log).
- **Objective mode** (`ap start objective`, 19 Oct 2026; host API v11): a supervisor above the modes doing what the
  current objective asks from the player's information - fight (mission-pointed hostile within 6 km or an attacker
  within 3 km), a friendly target ship (escort words: formation, refused -> follow; else follow), a hostile target
  (combat), a planet's ring / a station (travel, dock only when the words say so), a lane ring with a dock verb (ride it
  toward the neighbour `space::LaneOpen` allows: the mission's own permission; far: arrive near it first; nothing is
  decided while riding / jumping), the navmarker (travel), else rest (Hold). Re-decides on objective change, the sub-mode's end, a fight over, a threat; keeps a running mode with the same
  target; decide-loop guard (6 in 10 s: 5 s pause; a decision the guard swallows leaves its step undecided, so it
  forgets the step's key and the next look decides the step - 30 Oct 2026: before, a new objective arriving as the 7th
  decision was taken for "the same objective set again" after the pause and never decided, gate q3s2 M07). An escort objective whose target is a place (a buoy) escorts the
  ship an objective pointed at earlier (`escortShip`: M03 transport1), else the nearest friendly marked ship. Verbs are
  English keywords (other languages: kind only). A travel objective (fly to / dock with a place or a navmarker)
  outranks the fights around it (user: adhere to the mission's script); once arrived, threats are fought as before.
  Verified: M03 S010 -> SUCCESS alone; **M03 from step 5 (campaign
  Save1e7d) -> "Mission Result: Success" with the objective mode flying every space part** (lane 42 -> 43, convoy
  escort, ambush 8 kills, Willard, Valkyries, CA Minor, Walker, Battle of Willard 11 kills; 401 + 136 + 295 s) and the
  harness only for the rooms (launch x2, the CA Minor bar scene) - 19 Oct 2026; **M04 "Escape
  from Liberty" from Save6380 -> SUCCESS 1173 s** the same way (`tools\harness\missions\M04objective.ps1`, sweep `objective-M04`;
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
  SaveC707 (`tools\harness\flrace.ps1`, history `research\experience\legs\race.tsv`), = the user's 2020 lap
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
  **Level-off (`level_off`, default on; the owner, 28 Oct 2026: on a long flight to a distant waypoint the ship stayed
  banked and the camera skewed):** the player has no roll control and FLvibed's steering (the game's own mouse flight)
  never levels - measured: a -56 deg bank stays (0.3 deg/s back), the game's auto-level toggle changes nothing in free
  flight; the game's Goto levels a ship in ~2 s. On a travel's straight run (an Arrive step, the nose within 2 deg of its
  aim, the aim over 2 km away, over 60 m/s, no lethal zone ahead, the geometry guard clear, the game in free flight) a
  bank over 15 deg held 1.5 s hands the run to a Goto directive toward the same aim (the cruise as it is) until the
  bank is under 3 deg (at most 4 s; then 8 s quiet), events `level-off roll=` / `level-off end (level | 4 s | ...)`.
  Verified: a 49 deg bank from the autopilot's own turn levelled to 1.2 deg in ~3 s. Refused (multiplayer: no
  server-side AI for the player) = off for 30 s. The motion trace has a `roll` column since (debug `ship`: roll, up).
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
  (from its system entry alone the order of the flying ones is unknown). **What it reads is the loaded game's own** (30 Oct
  2026, the A37 S1 finding): at each game load (a fresh Content.dll: a save, New Game, the load after a death) the host
  forgets the title, the marks (MissionMarks / MissionObjectIds) and the comms heard and hooks the new Content.dll at
  once, so the load's own objective and title are seen; the logic forgets mission.cpp's last script / text and shown
  objectives when HostApi v34 GameLoads moves (`mission::ForgetGame`, controller.cpp OnFrame). Before, SaveS182 loaded
  after SaveS208 kept M07's title and the by-title stand-in named M07's script. The marks are not lost by this (MARKS-LOAD
  closed 30 Oct 2026 late evening, no code change): the game re-sends the saved marks of every object that exists after
  the load, after the reset (SaveS418: King + 3 rogues); what S416 lacked were the generators themselves - a mission solar
  without a matching `system =` is not saved, so after such a load the step has no target at all (`..\..\docs\FREELANCER_RE.md`
  "Saves"; the harness refuses such saves as fixtures). A target that is only the mark fallback (the
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
  **Motion trace** (`motion_log`, was `motion_trace`; session 6; since 31 Oct 2026 off for players - 0.1.0 wrote it for
  everyone, audit A1-1 - and always on in a test start, `FLVIBED_BACKGROUND=1` from `deploy.ps1 -Background`; the
  0.1.0 ini's `motion_trace = 1` line is ignored): `FLMotion.tsv` beside the log, 10 rows / s while the
  autopilot flies - mode, action, speed, the mode's steering (pursue / station / steer / brake), throttle, afterburner,
  cruise wish, slide, the geometry guard (blocked, clear distance along the motion), wall slide and its distance,
  target and its distance, the nose's alignment with the motion, position; `ms` wall time, `game_ms` game time (since
  30 Oct 2026, the game clock; restarts at a logic hot reload). flleg keeps it with the leg's evidence;
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
  - *Every gun on a static target* (28 Oct 2026, the owner watching M13's generators: "not firing on the generator",
    "too close to the generator and not firing everything it has", "too much of an over-emphasis on avoiding friendly
    fire"; ca3e715, b6df5ec). Probe (debug `gunpoint`, i2 at a generator): every gun and the turret CanPointAt / CanFire
    ok, all idle 7 s in the hold's wide orbit (the lead off the screen centre = not "aligned"); run M13-fcov-a: the guns
    fired 11.4 s of a 113 s generator fight, "line-of-fire blocked" up to 45 s (a friendly within 400 m behind the
    generator). Now: `fire_covering` (on) - while any gun can point at the lead (CEGun::CanPointAt, every 0.2 s) the guns
    aim at it through host v28 SetGunAim and fire, nose on it or not; the guns' line-of-fire check ends at a solar target
    (its hull takes the misses); `static_hold_guns` (on) - at the hold, fewer guns pointing at the target (5 deg, the
    tolerance CanFire accepts) than fitted = the gap +20 m every 1.5 s, until a step brings no gun more (then one step
    back; a gun mounted facing away kept the first runs stepping out to ~700 m); `lof_caution` (0.35; 1 = before) scales
    every friendly-fire corridor (guns 21 m, 31 m behind for 140 m; missiles 70 m + blast, 140 m). Measured from
    M13_ck_st03 to the sphere: 214 / 235 s (before) -> 179 / 167 s; the four generators 74 s with the guns firing 48-49
    s (was 113-133 s, 11-23 s). Judged by gate mpb3 + the fight matrix (HANDOFF).
  - *Open (the owner, 28 Oct 2026, M09 and M13's St03 entry fights "suboptimal")*: half the fight with the nose off the
    target - turn-evades every 4-5 s swing it 110-172 deg toward another attacker; fights at or past gun reach (average
    518-599 m, i3 1754 m chasing targets to 9.8 km before giving them up); the guns held for friendlies (now tightened).
  - **Time on target, the evening of 28 Oct 2026 (the owner at the gate; f8eb920 .. d950a58; judged by gate mpb12 + the
    fight matrix - mpb3..mpb11 were stopped in their first minutes for these changes or a defect each showed):**
    - `finish_target` (on): a live hostile ship within 1 km stays the target until it dies (the attacker bonus -800 beat
      the current target's -400); hull under 35 %, a player's choice or a run beyond 1 km lets go.
    - `missile_lead` (on): at a moving ship the seeker-cone line runs to its impact point (position + velocity x the
      predicted flight) and the launch comes out of >= 0.4 s of afterburner run (planned rounds at moving ships hit
      0.82-0.92, faster closing better: 0.84 at 50-150 m/s, 0.93 above). Missile outcomes: torpedo taps mark their
      rounds as planned, shield loss counts, at a solar the closest approach is to its hull (361 of 400 "unplanned"
      misses at 0.28 were torpedoes at generators). Missiles at every solar and big ship go at the launcher's refire.
    - `headon_reverse` (on): a ship 150-1000 m off with its nose on us (25 deg) closing > 40 m/s - reverse thrust, the
      pursuit steered at its position + velocity x half the time to the pass (<= 1.5 s), the guns on the real lead
      (SetGunAim), a slide against the predicted miss point while it is under the clearance; held to closing <= 0, 45
      deg or 100 m (it flapped every 40 ms around the entry at first). Under fire it holds - no extend / break / evade -
      while the shield is above `headon_shield` (0.25) and the hull above half.
    - `standoff` (on): measured on 43,677 combat samples - inside 80 m the nose was on target 50 % of the time and lost
      it 14 x a minute (250-350 m: 87 %, 0.14); per fight the hits per second were highest close (0-150 m 10.7, 150-250
      m 9.2, 250-350 m 7.2). The band against a ship: the prior max(80 m, 1.3 x its speed / our turn rate) up to 60 % of
      gun reach (our turn 1.05-1.2 rad/s, fighters 72-200 m/s: ~95-240 m), then per pair (our archetype, its archetype)
      the 50 m bin with the most gun points per second (>= 60 s of the pair, a neighbour bin every third fight under 10
      min) - kept in EXE\FLvibed\ranges.tsv; event `standoff pair=.. band=.. learned|explore|prior`.
    - Roll (the player has none; pitch and yaw turn alike - shiparch steering torque and angular drag equal on both for
      68 of 69 entries, roll torque 2-4x): the roll engine (space::PlayerSpin / AddPlayerSpin = PhySys angular
      velocity, at most steering_torque z / angular_drag z; verified +-1 rad/s linear, no drift; `roll_test`);
      `roll_guns` (on) rolls toward the angle where more guns can point at the lead (CanPointAt at 5 deg, 8 angles every
      0.25 s - the guns' swivel limits sit in their mounts' frames); `roll_flair` (on) a barrel roll with a sideways slide
      when no angle loses a gun; `roll_bank` (off; banking into turns spun the ship on through +-180 deg - solved 29 Oct by
      `steer_level`, below; the frame measured right-handed, + = roll right); no roll in a
      big turn (level-off waits for the nose within 10 deg, the fight's rolls for the lead within 25 deg - M07, mpb19);
      `level_off` now levels by roll (rate -2 x bank, at most half the top rate; 56.5 -> 0.6
      deg in 3 s, smooth) in travel / dock / goto / follow / face / hold - the Goto hand-over only as the fallback.
    - Host `pump_no_block` (on): the game's message pump (0x5B0B60) peeks, then takes with the blocking GetMessageA - a
      peeked message GetMessage cannot take left the main thread waiting with no frame (gate mpb6, M07: frozen, main
      thread in win32u under Freelancer.exe+0x1B0BA4; the 4 m 40 s M05 freeze and mpb2's 152 s M02 stall resumed by
      themselves). The import never waits: nothing within 2 ms = WM_NULL; the log counts such takes.
  - **The steering's frame, committed rolls, the follow, self-bursts (29 Oct 2026; 0332ec1; judged by gate mpb22):**
    - `steer_level` (2): the game's mouse flight does not turn the nose in the ship's own frame. Measured on instance 5
      (SaveC705, travel to Fort Bush with `roll_bank`, event `autopilot bank ... phi= turn=`): with the ship banked b
      against the system plane the nose turned b off the body-frame steering (b -25 -> 8 deg off, -73 -> 90, -113 ->
      118), so the bank chased an aim that kept moving round the nose - the spin. Restated into the level frame (1) or
      the camera's (2) the turn follows the aim (aim -68 / turn -70, -53 / -44 deg), the 90 deg turn closes in 0.9 s,
      no spin. In fights (debug `steer_check`, the lead round the nose vs the measured turn every 0.25 s, c706, 4
      scenarios): Pursue steers by the chase camera's screen offsets; unrestated its turn ran +12..+20 deg off (mean,
      banks under 90 deg), restated into the camera's frame the mean is -5..+8 deg at every bank, medians alike
      (16-22 deg: the lead moves) - hence 2. aim::LevelSteer(sx, sy, from, into), fl::FlightScreenAxes. Fight times equal
      within noise. `roll_bank` now stable (banks 31-46 deg, levelled after) but off: Fort Bush docks 11.5-11.6 s with
      it, 11.3 without (2 runs each); Baltimore equal.
    - `roll_commit` (on): gate mpb21 had 1182 `roll guns` choices - median 0.51 s, 477 ended at the first 0.25 s
      re-check, of 464 followed by another choice only 55 began at the guns the first went for (a 45 deg roll needs
      ~1.3 s). Now a choice is an angle (the roll measured from the spin), slowed at the end (4 x the angle left),
      arrived once the guns reach (`roll guns reached now= from= gain= in=`), dropped after two checks without gain or
      2.5 s, 0.5 s before the next choice. Fight A/B (c706, 4 scenarios x 2): 0 of 80 choices reached their angle
      before, 49 of 74 after (each at its gain).
    - `follow_own` (on): the game ends a re-issued follow after 0.15-0.19 s (mpb21 probe: 7-15 flips per instance; also
      Escort's rejoin, which had no back-off); a follow behind a moving transport at 791 m held 40 s (SaveC705) - the
      cause is not found yet: each end is logged (`follow ended after= dist= leader-speed= our-speed=`). After one end
      within 1 s our own flight holds the follow point (the leader's offset in its level frame, a second ahead; nose on
      it beyond 80 m, else parallel; throttle for its speed + half the gap) while it stays within 1.25 x follow_distance
      at engine speed (`follow own on|off`); else the c9732aa back-off.
    - `self_burst` (on): mpb21's M13 death at the sphere's last generator (radius 915): rounds launched 557-569 m from
      its centre, 52-60 m off its hull, were gone 0.01 s after their launch and burst on us - 2,424 (a torpedo's full
      damage) earlier in the leg, 2,071, then the salvo that killed the ship at 94 % hull 12 ms after its launch;
      launched from 782 m they flew 2 s and hit. The blast / flank rules saw nothing (the hull 52 m off, blast 25 / 16
      m); the Nomads named by `big-hit` were 881-938 m off, beyond their 600 m guns. Learned per target: a round gone
      within 0.12 s of its launch -> no launch from closer than that + 150 m (`self-burst`, `launch-held ... burst at
      the launch`). Why the game bursts them there is not read yet (the ship sits inside the target's 915 m radius).
    - `dock_regain` on by default: gate mpb18's M06 Hood - granted at 7 m / 193 m/s, a bump, then the game's dock (op 7)
      ran 64 s without docking (the probe); validated before on M04 (s12b-s12d). Its "grant not followed" hand-over
      now waits for a crawl that gains nothing (mpb22 M04: it handed a closing crawl over, the approach restarted, +9 s).
    - After gate mpb22 (13 / 14 PASS - M11 passes, M13 died at the Nomad battleship; fight matrix FAIL 28 / 32 on the
      old c705/pirelite_d9_x2 wrong retreat, re-run 3 / 3 won both ways; follow flips 20 -> 14, escort 11 -> 2):
      `self_burst` also counts a round that ends within its blast + our radius + 10 m of us (M13: a torpedo at the
      battleship from its blind spot, 242 m from its centre, burst on us 0.9 s later - 2,424, dead); `follow_own` stays
      on for the leader once on, at any distance (cruise beyond 2.5 km, off inside 1 km) but lane / jump speed - the
      game ends a follow at once beyond follow_distance (27 km, 9.9 km: re-issued every 0.2 s) and the own flight
      flapped at a leader just over our engine speed; `bay_arrive` (on; the owner, 29 Oct 2026: "fast-dock was a
      little too aggressive with the station"): at a bay the game's dock flies the last metres from p2, so a cruise
      arrival rammed Benford every gate (requested 15-25 m from p2 at 282-299 m/s, the hull hit 0.15 s later, 299 ->
      136-146 m/s, then a crawl; mpb22b failed the 58 s checkpoint) - lined up on a bay the arrival is braked to 2 s of
      the hull ahead of p2 along its axis (ray test, 1 km), at least 1.25 x the engine's top speed (event
      `fast-dock bay-arrive cap=`). Verified mpb22c: Benford landing 11.1 s (gold; par 12.5, the last runs 23-58 s), arrival 130 m/s.
    - Gate mpb24's M13 death, the M13 burst probe, the runners' approach and the `blind_rush` test, 29 Oct 2026 (history: AUTOPILOT_LOG.md "29 Oct 2026: M13 burst probe and blind_rush")
    - **The shape switches, as registered** (eb30dcb; `autopilot_settings.cpp`; all OFF until gated): `rush_hold_fire`
      (no missile or torpedo launch while `blind_rush` closes on the spot), `self_burst_damage` (`self_burst` learns
      only from a round end that cost us >= 70 % of its damage), `blind_contact` (blind spots at hull contact too, 0 m;
      a held spot kept unless it costs 5 % a second), `blind_contact_gap` (55 m since d7807f1), `battery_reserve`.
    - The shape switches: an interrupted batch, and the runs after "their times are the minimum goal", 29 Oct 2026 (history: AUTOPILOT_LOG.md "29 Oct 2026: the shape switches and the owner's decision lead-up")
      - **DECISION (the owner, 29 Oct 2026 ~21:45): combine A and B - "You must achieve full DPS, and if that means that
        when splash damage radius is a problem - avoid that radius. You MUST maintain full DPS while surviving as
        efficiently as possible."** The design rule this sets, for every big target (not only M13's battleship):
        1. Full DPS is the requirement, not a trade-off: every gun and every launcher at its refire rate, the whole fight
           (the runners' rate: a torpedo + two missiles every ~2-3 s, guns throughout).
        2. Splash is avoided by WHERE we fire from, never by holding fire: each launch happens from a position whose
           round's impact point is outside its blast radius + our radius + margin of us (HD Edition: full damage at any
           reach - FREELANCER_RE "Explosion damage"); the muzzle bursts at the 55 m hold (0.01-0.18 s, 6-8 m) say the
           point must be read along the round's own path (the nose, the launcher), not only the line to the centre.
        3. So the ship moves between the two ranges as the weapons need: the contact / blind-spot hold (guns, safety from
           the turrets) and a firing position at the stand-off the ordnance needs (A's approach volley from range, R2's
           every 2 s from 1,358 m, and a ring off the hull) - timed to the launchers' refire, so no salvo waits.
        4. Survive efficiently (B's consumables): arrive with full batteries (battery_reserve, working), burst them on a
           failing shield as the runners do, nanobots by need; no rush with a hostile fighter within ~300 m (the allies'
           fire into our path); self_burst counting the damage a same-frame repair hides; log the shooter and munition at
           every blast-near and big-hit.
        Measure it against the runners: bsdown par 45 s, their ~40 s from arrival to the kill; DPS logged per fight
        (ours vs the launchers' and guns' theoretical rate).
        **Status: decided, not built** - the owner paused this work for another session's project-wide work (29 Oct
        ~21:45); resume from here.
      - **Q4 slice 1, built 30 Oct 2026 (not gated: every switch registered OFF until an sprt KEEP):**
        rules 2 and 3 of the decision, on `core\game\ordnance.h` (host-free, unit test `ordnance`: FlightTime - the old
        MissileFlightTime's maths, now its wrapper -, RoundImpact, PredictPos, LaunchSafe, KeepOut, ShouldGoToSite,
        PickFireSite). The switches (`autopilot_settings.cpp` kDefs; docs\SETTINGS.md):

        | Switch | Default | What |
        |---|---|---|
        | `launch_site_check` | off | at a static or big target, after the existing blast rules (they stay): the round flown ahead from the launcher (our velocity + the muzzle along the nose, motor, seeker turn, the game's ray test per ~0.2 s stretched by the fuse `detonation_dist`) to its burst, we to where this frame's station / steer goal and our velocity take us by then (the worse of that and our velocity alone); a launch only with the two >= blast + max(5, our radius) + `launch_site_margin` apart. Log `autopilot launch-site ok|held weapon=torp|missile impact-dist t sep need cause sep-goal sep-coast goal det dist speed` (2 s per verdict). While on, `self_burst`'s learned distance and `rush_hold_fire` are skipped at these targets (proxies of it) |
        | `launch_site_margin` | 20 m (0-200) | the margin above |
        | `ordnance_keepout` | off | our live rounds' burst points (TrackOwnMissiles: the ray from the round to its target's centre meets the hull, 10 Hz; solars only - a ship's hull is no solid to the ray test) bend the station / steer goal out of a sphere of blast + our radius + margin (from inside it: straight out); log `autopilot keepout` (1 s) |
        | `ordnance_site` | off | the blind-spot scan also picks a firing position: the best-scored candidate with a hull gap >= `ordnance_standoff` (rays for the 3 best candidates that far out); the station goes there while a launcher with ammo has its refire due by the station law's travel time to it, else to the guns spot; logs `autopilot fire-site spot ...` (a new site) and `fire-site on|off ...` (a change) |
        | `ordnance_standoff` | 150 m (0-600) | the firing position's hull gap |
        | `blind_no_pass` | off | StaticPass refuses a target whose blind-spot scan found 4+ turrets, as StaticHold does (a held spot dropped mid-fight started the pass's afterburner exit run: the death of `research\experience\legs\2026-10-30_0135_M13-dt60b-1-1_i1`); a pass left in its coast goes back to In (big_coast then restores a killed engine) |

        Logging beside them (always on): the combat stop line's `ord-launches` (homing rounds that left) and
        `ord-possible` (seconds a launcher had ammo and its refire ready, summed per launcher, while the planner had
        a target); `big-hit` names our last round to end within 0.4 s (`own-end=<id> munition=torp|missile to-us ...`).
        The fuse (FREELANCER_RE "Proximity fuse"): a physics sphere of detonation_dist (missiles 4 m, torpedoes 6.25 m)
        round the round; it does not explain the 55 m hold's muzzle bursts - the launch-site log's `cause` and `det`
        and big-hit's `own-end` are the probes. To arm all six: `ap set launch_site_check 1`, `ap set ordnance_keepout 1`,
        `ap set ordnance_site 1`, `ap set blind_no_pass 1` (margin 20 / standoff 150 are the defaults).
        (history: AUTOPILOT_LOG.md "Q4 slice 1 and 2, judged 30 Oct 2026")
      - **Q4 slice 2, built 30 Oct 2026 (`static_volley` (deleted 31 Oct 2026, AUD-A1-5), off until an sprt KEEP):** (history: AUTOPILOT_LOG.md "Q4 slice 1 and 2, judged 30 Oct 2026")
    - *Open (the owner, 29 Oct 2026)*: M04's opening does not need the fight at Manhattan - the script waits for
      Walker's wing to kill the Unity (`Cnd_Destroyed battleship_unity`) and its comms (M04.unity at par, 56.7 vs 56.6),
      so the escort's fight there only risks hull; M04.lane3 is +25 s over par from lane-ring entries bumping Juni
      (57-87 m). Next: no chase in "Defend yourself" / "Survive" steps (attackers of us / Juni only).
    - **The owner live, 29 Oct 2026 late (4c67fee; not gated - verification runs only):**
      `survival_calm` (on): under an escape title's stance only ships shooting at us are targets and no destroy step /
      mark is pursued (the memory - title, ended, system - and the way into the system start over at each load, NoteMenu:
      audit A1-2 / A1-8, 31 Oct 2026); `survival_until_jump` (on): the stance ends at the mission's first system change (the owner: "the
      entire mission up to and after Benford's destruction does not benefit from ... pursuing time on target. After the
      player jumps ... speedrunning kills has meaning"; runners' Unity section: 0-2 kills, hull lost 0.0-0.04). M04
      calm run: hull 1.000 through Manhattan, batteries 14 -> 7, the stance ended at the jump, hunters -13 s vs PB, PASS
      1004 s. `prestage_next` (on, speedrun knowledge): during "Awaiting" after a step the reader saw no goal fire (a
      lane ride), fly to the next step's goal (mission::NextStepGoals, dialogue chains) or else to the point of the
      [NNObjective] declared next (mission::NextObjectivePlace; M04 "Dock with the Trade Lane" -> depot_wp1): the
      20 s follow of Juni at 78 m/s gone, M04.benford 8.5 s (gold; par 12, was 27.9), -22 s vs PB at Benford.
      `survival_stage` (on): the calm stance waits 300 m short of the next declared place with a point (skipping 0,0,0
      markers: Ring 147); lane2 unchanged (Walker's wing and comms gate it); lane3 +25 s stays (Ring 134 after the ride).
      `big_coast` (on): CoastGuard (autopilot_frame.cpp), run last over every mode - the objective mode cruising at a
      huge destructible static target ahead (radius >= 500 m, finite hit points: a solar's attitude reads 0) within
      gun reach + `big_coast_lead` (1500 m) of its hull: the fight started on it, the engine killed a frame later still
      in cruise, then Out::coast (the controller sends only steering - the owner: "Killing the engine when in cruise
      engine only works when NO additional engine-related input is sent"), the objective mode not deciding; ends at
      weapons range + the braking distance (or the shield under 15 %, not on a hit), engine on (pressed again only after
      1.5 s: a second blind press turned it off), a brake to 60 m/s, then the fight. aim::Current at a huge static
      target aims at its hull point (ray) and calls it in range by that point; the launchers measure their seeker reach
      and the nose to it (the city generators' centre lies ~430 m behind the hull: "guns 1 of 5", no launch in the coast,
      the guns silent). City fixture (M13_ck_city, 0 batteries): coast + close hold 4/4 PASS 126-136 s (the hold alone
      3/4, 132-140 s); coast + standoff 0.7 x gun reach + sideways run died 3/3 (strafes top out ~25-30 m/s with the
      nose on). Off after tests: `big_pass` (307 / 236 s), `big_standoff`, `big_jink`, `hold_big_gap` (died 2/2),
      `hold_jink` (all five deleted 31 Oct 2026, AUD-A1-5). `follow_defend` (on): a player's follow under attack escorts its leader, a leader gone under attack
      fights on (instance 6 died idle). Open: the owner's lateral / vertical moves under fire; a full M13 leg with the coast.
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
  the game's best path (lanes, gates, holes), hazard-free (`tools\harness\flroute.ps1` zones; planets' lethal zones not
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
  `shortcut_rush` (30 Oct 2026; 1 = as above, 0 = the request at the fast-dock point at speed and the game's queue
  answer decides, no rush and no queued take-over): the legs never logged a formation success (0 of 125 tries 28-30
  Oct) and the rush cost +91 s over its own ETA on gate s7arb3 (17 rushes, 13 at holes; `..\..\docs\research\
  gap_decomposition.md` 4) - A/B pending, the default unchanged until it is judged.
  `skip_dock_scene`: the autopilot's own docks skip their docking / landing cutscene (0.4 s). Lane speed learned per
  lane; `eta`, `worth`.
- **Settings** `[Autopilot]` in FLController.ini (`ap set <name> <v>` live; `config save`): pilot, engage, auto_fire,
  missiles, countermeasures, evade, fire_cone, follow_distance, follow_offset, escort_radius, hold_radius (hold mode:
  run to the point, the game's Idle stop, the nose to the hold's target or along the next leg of the game's route, by
  steering at zero throttle (never the selection), fly back past this leash; yields to any other maneuver), consumables
  (needed | never: missiles, repair, countermeasures), flee, steer, bursts, engage_distance, engage_radius, big_ships,
  caution, retreat_odds, hold_defend, mp_caution, mp_flee, pvp_duel, pvp_min_dist, pvp_range, repair, battery_horizon, cm_distance, cm_shield_fraction,
  dodge_window, loot, loot_min_value, dodge, evade_scale, weave_radius, slide_time, stagger, turn_evade, adaptive, duel (force one: 0 / 1 / 2, -1 adaptive),
  skip_dock_scene, `p.<field>` (personality overrides); travel / dock: fast_dock, dock_request_distance,
  dock_arrive_speed, dock_stage, ring_request (m before a ring's p2 at full speed; 0 = ~1.4 s /
  400 m out), lane_request_distance, lane_toward, fast_thrust (0 planned, 1 burner, 2 cruise: verification runs), avoid,
  airlock_fly_through, airlock_charge_at (the charge's share done at an airlock's transit), airlock_burn, wait_flies,
  tractor_exit_run + tractor_exit_range (m), passage_window, rejoin_nearest,
  noway_climb, hop_cruise_hold, hop_reset_burn (session 10: off by default - kept / experimental in "Speedrun flight"
  above; ring_lineup, travel_passage, wheel_parallel, hug_route, arc_bulge, hop_narrow and fire_all were removed on
  24 Oct 2026), geometry_guard; combat: disable_at (end the fight
  at this hull fraction of the chosen target: "disable" objectives), blind_spot, blind_shelter, static_hold, static_hold_gap, static_hold_slide, static_hold_mines,
  static_pass, static_orbit, target_dwell; objective: script_goals; knowledge (all | exposed); run_in_background,
  focus_only. Debug
  `ap start <mode> [id|x y z] | stop | status | pilot | set | clear | field | save | load | preset
  standard|no-consumables|speedrun|competent | lof <id>`; `ap status` and the stop event carry engaged / on-target / firing / avg-dist.
  The definitive list with defaults and comments: `..\..\modules\autopilot\src\autopilot_impl.h` struct Settings (ini keys in
  `autopilot_settings.cpp`).
- **Out of focus** (option `run_in_background`, `deploy.ps1 -Background`): host API v7 SetKeepRunning (frame gate,
  watchdog, focus guard + start-phase hand-back, virtual cursor written into the game's cursor state); `focus_only`
  drops only the physical pad (flight and menus alike, one gate since 30 Oct 2026: `..\src\impl\focus_gate.h`). The focus guard's backup round (a system-wide thread snapshot, ~39 ms of CPU) ran every
  100 ms = 39 % of a core per background game; now 1 s for the first 30 s, then 10 s (new threads are hooked in DllMain
  anyway; the round logs any it had to catch): 0.7 %, parallel sweep 3 games navigation 132 -> 174 fps, M01a 149 ->
  183 (21 Oct 2026). Native movie skip: subtitles host `movieskip`.
- **Timers:** the decision timers run on game time since 30 Oct 2026 (`..\..\core\game\gameclock.h`: the sum of the
  frame dt, advanced in `OnFrame`; still while paused, 1/fps per frame under the test-only `fixeddt`), and so do the
  rate estimates (missile closing speed, the player's velocity, the rounds' hit times in `space_rounds.cpp`); UI / HUD
  animation, profiling, `fpscap`, the logs' stamps and the host's stamps (comm heard, player hit) stay on wall time.
- Verified 17-19 Oct 2026 (combat, dock, escort, formation, the predictive layer, background starts) and the sweep state of 21 Oct 2026 (the current per-leg state is `docs\SPLITS.md`) (history: AUTOPILOT_LOG.md "Verified 17 Oct and sweep state 21 Oct 2026")
- Direction (user, details in `AUTOPILOT_UX.md`): success is contextual -
  mission kill objectives = targets destroyed with least damage taken; vs overwhelming forces still win, scaling
  defense with the odds (threat vs strength); an ambush during another goal may be evaded or ignored (engagement
  policy per goal); consumables by difficulty and the player's resources; algorithmic baseline + experience / RL
  (per-context experience store, no forgetting). Long term: harness + autopilot finish the game with zero input
  (the campaign-run benchmark: New Game -> story end unattended) and beat the 2020 playthrough's in-space times per
  mission phase without trading hull or success (the owner's benchmark).
- Next: the P5 remainder (between missions: net-worth story gates +3500 / 5200 / 7000 / 10000, jobs, purchases; rooms in
  the autopilot), then P6 (New Game -> story end unattended, AFK work); experience store; Turret Mode (user
  idea: AI flies, the player aims with the right stick in turret view = USER_AUTO_TURRET, "TURRET VIEW", key H; left
  stick cancels); more prediction (system level, faction strength, mission objectives by script label); scan / new
  game natively; harness waits whose safety timeouts can become game signals (user: every timer should get a native
  game-data replacement).

## HUD cues (19 Oct 2026; design + status `HUD_CUES.md`)
- `..\..\modules\hud\src\shieldcue.cpp`: screen-edge sprites from the fitted shield's own hit effect (`..\..\core\game\hitlook`: shield /
  munition hit looks from the game data, C++ .ale reader), tinted by the attacker's weapon impact, strength by the damage
  burst, a flash when the shield fails; the edge glow is brighter toward the hit (direction from the point the game
  feeds its own HUD damage indicator: host API v12 PlayerHit); hull hits in the ship's own hull hit look and, below the
  ship's damage-fuse thresholds, a soft edge of its damage effects (sparks / fire / smoke; `..\..\docs\kb\hull_damage.md`);
  hull damage carries its own pattern (bold hexagons by default / plates) and reddens with the hull lost, breathing below the game's
  hull-critical point; options `shield_cue`, `shield_cue_intensity`, `hull_cue`, `hull_cue_pattern`; debug `cue` (`cue dir <deg>|off` pins the direction, `cue
  hullpts <hp>|off` fakes the hull for the damage edge), `hitlook` (`hull`, `stages`).
- `..\..\modules\hud\src\reticlecue.cpp`: ring pulses at the crosshair (autopilot engage / hand-back, nanobots / batteries used); option
  `reticle_cue`; debug `cue ring 0..3`. The shield cue draws in `OnRenderUnderHud` (under the HUD, user's choice; the
  overlay as fallback), the rings in `OnRenderOverlay` (over the HUD), both through `..\..\core\game\render`
  (`Sprite`, `BeginPlain`).
- Test aids: `drainshield <id>` / `shieldof <id>` (the game's DrainShields / GetShieldHealth), `shot <cmd> <px> <py>`
  (fire, then park the cursor the frame after the power draw: clean research captures), `laneopen <ring> [toward]`
  (the mission's lane permission, `space::LaneOpen`; the travel planner skips lanes it denies).
