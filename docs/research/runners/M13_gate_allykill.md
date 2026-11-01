# M13 gate battleship: `ally_kill` - the design (31 Oct 2026 ~05:47, a design agent; to build after M02's spawn_stage)

Why: the Nomad gate battleship (NomadCap3) is M13's main fail state - 3 deaths in ~16 legs on 31 Oct, ~20 batteries
spent there with attackers=0 (docs\research\runners\M13.md "## M13.barrier"). Runner R2 does not fight it: from v14420
it sits at the hole (0 m) / 8-31 m from the battleship, launchers / shield / hull unchanged; the allies kill it ~45 s
after the ambush; it jumps ~v14431 (run2 raw rows). [Claims below from the agent's read - verify each line before build.]

## The script (DATA\MISSIONS\M13\m13.ini)
- 24460 "Take out the Nomad Ships": an `ids` objective with no place (200-203), set by `in_hole_range` (1919-1940);
  NomadCap3 marked 2 s later (1942-1945).
- The hole is locked since `wait_for_trent` (1843); unlocked only by `check_cap` (1953-1964) on
  `Cnd_Destroyed = NomadCap3, 1, ALL` - NO 4th field = any killer (elsewhere e.g. `dexter, 1, EXPLODE, Player`); the same
  trigger sets `lead_through_gate` (24465, dock with the hole).
- Allies are hostile to it (`SetVibeLbl us,NomadCap3` 1929-1930) and invulnerable to 0.9 (1870-1873). No timer / FAIL
  while it lives: `nag_gate` off at 1938; `nag_nomad_gate` (23175) only after the unlock.

## Why we fight it
ObjectiveVerb has no "take out" (autopilot_objective.cpp 149-187) -> verb None; markFight (913-926) makes the marked
hostile within 6 km a Fight; timer_destroy acts only on "timer" (StepAdvanceOf says "kill"); kill_ahead finds no kind-3
goal ahead; the fight starts at big_coast's huge-static branch (1760-1766) or the threat fight (1945); battery_budget
counts it as the big fight now (autopilot_combat.cpp 616-618).

## The rule: `ally_kill` (Bool, default 0, Source::Hidden; speedrun knowledge)
Help: "a step the script ends on a mission solar's death by anyone, whose kill unlocks the next step's dock, is not
fought - wait at that dock, answer only attackers, dock on the unlock; fight as before while no friendly wears it down.
0 = today's fight". Row beside timer_destroy (autopilot_settings.cpp 237), g_set.allyKill (autopilot_impl.h ~392).
- Data (core\game\mission.*): Goal::killer = f[3] (mission.cpp 843-849; "" any); StepAdvance gets `anyKiller` and
  `unlocks` (CreateID of the object in `Act_LockDock = Player,<obj>,unlock` on the kill path or its at-once hops - no
  LockDock parsing today). Qualifies: kind "kill", target a mission solar (kind 3, not ships), anyKiller, unlocks ==
  nextObject, next step's verb Dock. Add the fields to the StepAdvanceSweep line (lists every qualifying step).
- Pure half: `lookahead::AllyKill(AllyKillFacts)` -> the TdVerdict values. Facts: qualifies, speedrun, me, dockPos,
  target alive, its hull, allies targeting it (friendly ships whose targetId is it), seconds waited, hull drop over the
  wait, attacked, our hull. Not qualifying / not speedrun / target dead -> Today; no ally on it, >= 20 s waited and <
  0.05 hull drop -> Fight (fails closed); > 300 m from the dock -> Stage (Defend if attacked within 1500 m); at the dock
  -> Rest (Defend if attacked). dockPos = space::ObjectPos(nextObject) (the hole 46983,0,-10694 st01.ini 98 - not the
  rep_inst point). Stage = a point trip, never Action::Dock (a locked dock answers dock_denied_locked: FREELANCER_RE 638-640).
- Adapter: AllyKillFactsNow like TimerDestroyFactsNow (697-764, keep the target's hull + the time on arrival); the
  decision right after timer_destroy (after ~1007) using its exits (991-1006: Travel/PreStage, Hold, Combat defendOnly -
  autopilot_target.cpp 206); a 1 s recheck (like 3013-3028); battery_budget leaves the target out of bigIds (616-618)
  and its group out of wAhead (631-646). It must run before the mark fight / coast starts (CoastActive, 827).
- Tests (tests\look_test.cpp): M13 24460 -> kill, anyKiller 1, unlocks = st01_to_st03_hole = nextObject; M06 dexter ->
  anyKiller 0; a verdict table for every branch.
- Logs: once per step `autopilot objective ally-kill <text> data: watcher=check_cap any-killer=1 unlocks=<id>
  next=lead_through_gate(24465)`; per change `... ally-kill <text> -> <verdict> why= target= hp= allies= dist= waited=
  hull= batt=`.

## Probe
From SaveS415 (FIT, M13.battleship), `-StopAt M13.gens`, 2-3 legs, ally_kill 1 + the M13 switches; keep the
M13.orillion checkpoint save (docked at the Osiris, 24450) as a new fixture ~110 s from the gate. Judge: no combat / no
launch at NomadCap3; 24460 -> 24465 time (R2 ~45 s); batteries at 24470 (aim >= 25 vs td1's 9); hull / shield at the
hole; deaths 0; dock within ~3 s of 24465; M13.barrier vs par 89; whether the fallback fired.
Risks: the allies alone may be slower than R2 (the gain is batteries + no deaths, not seconds); turrets reach the hole
(784 m from the battleship's centre) -> next variant a stand-off on the hole's far side; juni_dies / king_dies may be
armed; never acts under `exposed`.
