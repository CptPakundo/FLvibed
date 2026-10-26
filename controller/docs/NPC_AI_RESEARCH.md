# NPC AI on the player ship: research (17 Oct 2026)

> **In-game results (17 Oct 2026, later the same day):** see `..\..\docs\FREELANCER_RE.md` "AI on the player ship".
> Summary: maneuvers (Trail, Face, Goto, Dock) and server Goto / Follow / Idle work; the server refuses Trail / Strafe /
> Evade for the player, the client manager accepts them but **Evade / DrasticEvade crash Common.dll**; the **NPC brain
> (state graph + personality) does not drive the player ship** and the AI never fires for it. Built on that:
> `core\game\ai.*` (debug `ai ...`) and the **Advanced Autopilot** `controller\src\impl\autopilot.*` (debug `ap ...`),
> a FLvibed-side tactical loop in the role of the state graph. The desk research below is kept as written.

Question (user, 17 Oct 2026): can Freelancer's own NPC AI (role behaviours, combat maneuvers, scripted behaviour)
drive the player ship in ways richer than single-target autopilot? Desk research only: no game run, no mod code.
Sources: the annotated HDE Freelancer.exe disassembly (`tools\dis.ps1`), the export tables of the test install's
`server.dll` / `common.dll` / `content.dll`, FLSDK + FLHookSDK headers (`research\repos\FLSDK\include\FLCore\Common\AI\`),
ChaosMod (a single-player client mod that already puts the player ship under AI), FLUF `combat_ai`, HDE's mission
scripts and git history, `docs\missions\` runbooks.

Legend: **[disasm]** read in Freelancer.exe code, **[exports]** seen in the DLL export/import tables, **[SDK]** from the
community headers (layouts partly guessed by their authors), **[mod]** used by another mod that works in single
player, **[script]** used by the game's own mission scripts, **[untested]** needs the in-game test at the end.

## Short answer
- Yes. The player ship has the same `IBehaviorManager` as every NPC ship (Common.dll); the game itself puts it under
  AI directives all the time: the maneuver bar (goto, dock, formation, free flight), and mission scripts
  (`Act_GiveObjList = Player, ...`: Follow, GotoVec/GotoShip/GotoSpline, Idle, Avoidance) [disasm, script].
- The difference from an NPC is the **state graph**: the player's behaviour manager is created with state graph
  **`PLAYER`**, NPC fighters with `FIGHTER` (also `LEADER`, `ESCORT`, `TRANSPORT`, `GUNBOAT`, `CRUISER`, `NOTHING`,
  `DATA\AI\state_graph.db`) [disasm 0x544834]. The state graph is what turns a pilot personality (evade, buzz, trail,
  strafe, guns, missiles from `pilots_population.ini` / `pilots_story.ini`) into autonomous combat. Directives (one
  maneuver at a time, ordered from outside) work with the PLAYER graph; autonomous role behaviour needs a combat graph.
- Most promising for the controller: **one-shot native maneuvers through the game's own maneuver dispatcher**
  (0x4D8E20), including three the HUD never offers: **Trail** (stay behind the target), **Face** (turn to the target or
  a point) and **Strafe** (attack run). They report to the server like a maneuver-bar click, so mission conditions
  (`Cnd_PlayerManeuver`) still see them, and the player's own input takes over again (USER_CANCEL / free flight).
- Most promising for the test harness: a **game-AI combat bot**: `SubmitDirective` (server.dll) on the player ship for
  scripted flight (goto, follow, dock) right away, and a swap of the player's state graph to `FIGHTER` plus a pilot
  personality for a real NPC dogfighter [untested: the riskiest part, see Risks].

## 1. The AI layers and which apply to the player

| Layer | What | Where | Player ship in single player |
|---|---|---|---|
| **Behaviour manager** `IBehaviorManager` (0x1CC bytes [SDK]) | Per ship. Holds 21 behaviour objects (`behaviourArray` +0x174, one per `pub::AI::OpType`), the current behaviour index (+0xC0), the current directive (+0xC4), its priority (+0xC8), lock flag (+0xBC), enabled-maneuver mask (+0xB8), the steering inputs (direction, throttle, cruise, afterburner, strafe). Runs the active behaviour each frame. | Common.dll; created by `CEqObj::create_behavior_interface(IObjRW*, int stateGraph)`, fetched by `CEqObj::get_behavior_interface()` or the free function `GetBehaviorManager(IObjRW*)` | **Yes, client-side.** Freelancer.exe creates it at 0x544846 when the ship's owner (`[CShip+0xB4]`) is the local player (0x673344), with `pub::StateGraph::get_state_graph("Player", 0)` [disasm]. All player flight goes through it: mouse steering 0x4C7CB0 → `update_current_behavior_direction`; 0x4C7900–0x4C7BA0 wrappers for throttle, cruise, afterburner, strafe, auto level/avoidance (`docs\FREELANCER_RE.md` "Flight"). |
| **Behaviours** (OpType) | `-1` None, 0 NullOp, 1 Buzz, 2 Goto, 3 Trail, 4 Flee, 5 Evade, 6 Idle, 7 Dock, 8 Launch, 9 InstantTradeLane, 10 Formation, 11 LargeShipMove, 12 Cruise, 13 Strafe, 14 Guide, 15 Face, 16 Loot, 17 Follow, 18 DrasticEvade, 19 FreeFlight, 20 Delay [SDK `AIEnums.hpp`] | Common.dll behaviour classes (FLUF hooks Buzz, Evade, Face updates) | **FreeFlight (19) is the player's normal state**: user steering is just input to the FreeFlight behaviour. The client dispatcher uses Goto, Trail, Dock, Formation, Cruise, Strafe, Face, Loot, FreeFlight (table below). |
| **Directives** (`pub::AI::Directive*Op`, base `BaseOp {vtable, OpType op_type, int fireWeapons}`) | An order from outside: "do this behaviour with these parameters". `IBehaviorManager::set_current_directive(const IDirectiveInfo&, const BaseOp*)` applies one; `set_directive_priority`, `lock_maneuvers(bool)`, `enable_maneuver(int,bool)`, `cancel_behavior(CancelRequestType)`, `external_player_evaluate(int op)` (2 = maneuver not possible now, used to grey the maneuver bar) | Common.dll (exports); server-side wrapper `pub::AI::SubmitDirective(uint shipId, BaseOp*)` in server.dll, which itself imports `GetBehaviorManager` + `set_current_directive` [exports] | **Yes.** The client's maneuver dispatcher calls `set_current_directive` on the player's manager directly [disasm]; mission scripts and ChaosMod call `pub::AI::SubmitDirective(playerShipId, op)` [script, mod]. |
| **ObjLists** (mission scripts) | A queue of directives in `DATA\MISSIONS\Mxx\*.ini`: `GotoVec` (195 uses), `GotoShip` (145), `StayInRange`, `BreakFormation`, `SetPriority` (`ALWAYS_EXECUTE`), `Follow` (72), `MakeNewFormation`, `Avoidance`, `Delay`, `Dock`, `GotoSpline`, `FollowPlayer`, `StayOutOfRange`, `Idle`, `SetLifeTime` | Content.dll, `Act_GiveObjList = <ship>, <objlist>`; `Act_PobjIdle` = Idle on the player | **Yes, by design.** Examples: M02 `follow_king` (Follow King 1500 m, avoidance off), `stop_player` (Idle); M10 `follow_von_claussen_tight` (`SetPriority = ALWAYS_EXECUTE`, `Follow = Von_Claussen, 3000, 30, -5, 20`), M01b `King_and_Player_goto_gmg_patrol` (GotoVec). Mission actions `LockManeuvers true/false` lock the maneuver bar (e.g. during lane rides). |
| **Personality** (`pub::AI::Personality`, ~0x3F0 bytes of sub-structs) | Combat style: EvadeDodge, EvadeBreak, BuzzHeadToward, BuzzPassBy, Trail, Strafe, EngineKill, Repair, Gun, Mine, Missile, DamageReaction, MissileReaction, Countermeasure, Formation, Job (target preferences). Loaded per `[Pilot]` from `pilots_population.ini` (64) / `pilots_story.ini` (255) with `*_id` references to the style blocks | Set with `pub::AI::SubmitState(shipId, SetPersonalityParams*)` {BaseOp, int stateGraph, ContentCallback*, DirectiveCallback*, bool stateId, Personality} [SDK, mod: ChaosMod spawns NPCs this way]; `IBehaviorManager::get_personality` / `set_personality` | **Exists but is mostly idle on the player**: the PLAYER state graph does not choose combat behaviours by itself (inference: the player ship never evades on its own). |
| **State graph** | Table: situation → weighted choice of behaviours (switch ratios +0x10, `BehaviorSwitchRatios[21]`). Decides *when* to buzz, trail, evade, flee. | `DATA\AI\state_graph.db` (22.7 KB); `pub::StateGraph::get_state_graph(const char*, Type)` (Common.dll), `IBehaviorManager::set_state_graph(int, bool)`, `refresh_state_graph()`; `pub::AI::get_state_graph_id(shipId)` | **PLAYER graph** on the player [disasm]. Swapping it is the step from "directive" to "NPC brain" [untested]. |
| **Target / job selection** | Who to attack: `TargetManager::submit_forced_target`, `pub::AI::submit_forced_target(ship, target)` / `remove_forced_target`, `set_player_enemy_clamp` (missions' `PlayerEnemyClamp`), Job struct in the personality | server.dll / Common.dll | The player picks targets himself (`docs\FREELANCER_RE.md` "Targets"). An AI-driven player ship would need a forced target or the server's NPC scanner [untested]. |

Client vs server in single player: the game runs server.dll in-process. NPC AI is decided server-side (server.dll
calls the same Common.dll behaviour managers through `GetBehaviorManager`); the player ship is simulated by the
client, its behaviour manager is created by Freelancer.exe. `pub::AI::SubmitDirective(playerShipId, ...)` still
reaches it (mission scripts, ChaosMod "Assuming Direct Control" passes the client `CShip::id`) — how server.dll finds
the client's manager was not traced [mod, script]. Directives applied directly on the client manager
(`set_current_directive`) are **not** seen by the server unless the client also sends `IServer::SetManeuver`
(next section).

## 2. Native ways to give the player ship a behaviour

### 2a. The game's own player-maneuver dispatcher (best for the controller) [disasm]
`bool __thiscall PlayerManeuvers::Execute(int opType, bool flag)` at **0x4D8E20** (`ret 8`), `this = *(void**)0x674A58`
(0x463EE5 calls it that way). It reads the player ship (0x54BAF0 → CShip, class bits 0x503), its behaviour manager,
the ship's current target (`CShip::get_target`), builds the directive, calls `set_current_directive`, then tells the
server: `IServer vt[0x108] = SetManeuver(client 0x673344, XSetManeuver{shipFrom, targetId, opType})` (0x4D93AC; slot
matches FLHookSDK `IServerImpl::SetManeuver`), which is what mission conditions such as `Cnd_PlayerManeuver formation
transport1` react to.

| opType | Directive built | Needs a target | Notes |
|---|---|---|---|
| -1, 19 FreeFlight | none (IDirectiveInfo op 19, op NULL) | no | then 0x4C7950 = `reset_current_behavior_direction` (no turn left over). This is the "cancel autopilot" path. |
| 2 Goto | `DirectiveGotoOp`, gotoType 0 = ship | optional | target: range by target type (100 m for type 0x200, 750 m for trade-lane rings 0x80, else type-dependent constants); **no target: gotoType 1 = a point straight ahead** (ship position + forward × const), range 500 m |
| 3 Trail | `DirectiveTrailOp` on the target, distance from `this+0x3AC` | yes | USER_MANEUVER_TRAIL_CLOSER / _FARTHER (handled at 0x4D800F / 0x4D804E while Trail is active) change the distance via 0x4C78D0 |
| 7 Dock | `DirectiveDockOp` on the target | yes | the normal dock autopilot (verified path, `Start-FLDock`) |
| 10 Formation | flag 1: `DirectiveFormationOp`; flag 0: watch the target (`this+0x390`) and send a formation **request** | yes (flag 0) | the reply comes back through 0x4D9410 (arg 1 = reply, 0 = accepted → `Execute(10, true)`) |
| 12 Cruise, 16 Loot | none (op NULL) | no | |
| 13 Strafe | `DirectiveStrafeOp` on the target | yes | an NPC attack-run behaviour; not on any player key by default |
| 15 Face | `DirectiveFaceOp` on the target, or a point ahead if none | no | turn to face |
| 1 Buzz, 4 Flee, 5 Evade, 6 Idle, 8, 9, 11, 14, 17 Follow, 18 DrasticEvade, 20 | **no directive** (jump to the end: only the SetManeuver report) | — | e.g. `USER_MANEUVER_CORKSCREW_EVADE` calls `Execute(5)` (0x4D808D) and therefore does nothing locally: a cut feature |

How the keys reach it [disasm]: the player command handler (switch at 0x4D7FF5 on command − 4) maps
`USER_MANEUVER_TRAIL, _EVADE, _DOCK, _GOTO, _FACE, _DRASTIC_EVADE, _FORMATION, _STRAFE, _FREE_FLIGHT` to 0x4D83D1,
which looks the command up in the **four maneuver-bar slots** (records of 0x148 bytes, +0x134 command id, +0x138
op type) and calls 0x4D9470 with the slot: `external_player_evaluate(op)` → 2 = not possible, else `Execute(op, 0)`.
So a maneuver key only works if the HUD bar currently has that slot (vanilla: free flight, goto, dock, formation).
`USER_CANCEL` (0x4D81D9 → 0x4D8216) switches any running maneuver back to FreeFlight (19). Calling **0x4D8E20
directly** bypasses the bar lookup: Trail, Face and Strafe become usable although the HUD has no button for them
[untested]. The controller's LB+A (goto) / LB+B (formation) / A (dock) already use the bar commands.

### 2b. Server-side directive, any parameters [exports, mod, script]
`pub::AI::SubmitDirective(uint shipId, pub::AI::BaseOp*)` — server.dll
`?SubmitDirective@AI@pub@@YA?AW4OP_RTYPE@12@IPAVBaseOp@12@@Z`. Construct the op with its Common.dll constructor
(exported, e.g. `??0DirectiveGotoOp@AI@pub@@QAE@XZ`, which Freelancer.exe imports at 0x5C6520) so the vtable and
defaults are right, then fill the fields:
- **Goto** [SDK, ChaosMod]: `gotoType` (0 ship, 1 vector, 2 spline), `pos`, `targetId`, `spline[4]`, `range`,
  `thrust` (0–100, −1 = max), `shipMoves`/`shipMoves2` = true, `goToCruise` xor `goToNoCruise`, x60 200, x64 500.
  ChaosMod: `range 500, targetId = nearest dockable solar, shipMoves = shipMoves2 = true, gotoType 0, goToCruise`.
- **Follow** [SDK, ChaosMod on NPCs; M02/M10 on the player]: `followSpaceObj`, `maxDistance`, `offset` (formation
  offset, Vector), `dunno2` 400. Mission form: `Follow = <ship>, maxDistance, x, y, z[, ?]`.
- **Dock** [SDK, ChaosMod on the player]: `DockSpaceObj`, x14 0xbb83, x18 −1, x20 200, x24 500.
- **Idle** (`DirectiveIdleOp`, no fields) = `Act_PobjIdle`: stops the current maneuver. HDE commit c3c68fd (M08) adds
  `Act_PobjIdle` exactly to "break the player's flight" (waypoint + cruise) "and await the next orders" [script].
- **Cancel** (`DirectiveCancelOp`), **Delay** (`DelayTime`), **Trail**, **InstantTradelane** (`ringFrom`, `ringTo`,
  3750), **Launch**, **Evade / DrasticEvade / Strafe / Face / Formation / Guide / Ram / TractorBeam** (layouts unknown
  in the SDK: use the exported constructor and leave the defaults, or read the client dispatcher, which fills Trail,
  Strafe, Face from the target id at op+0xC).
- `pub::AI::lock_maneuvers(ship, bool)`, `set_directive_priority(ship, prio)`, `enable_maneuver(ship, op, bool)`,
  `submit_forced_target(ship, target)` / `remove_forced_target`, `update_formation_state`, `get_behavior_id(ship)`
  (current OpType) are server.dll exports too.

### 2c. NPC brain on the player ship (state graph + personality) [SDK, mod on NPCs, untested on the player]
`pub::AI::SetPersonalityParams p;` (exported ctor) → `p.personality` = a pilot's personality (read the `[Pilot]` and
style blocks as ChaosMod's `PersonalityHelper` does, or `IBehaviorManager::get_personality` from an NPC),
`p.stateGraph = pub::StateGraph::get_state_graph("FIGHTER", TYPE_STANDARD)`, `p.stateId = true`;
`pub::AI::SubmitState(playerShipId, &p)`. Alternative on the client manager: `set_personality` +
`set_state_graph(get_state_graph("FIGHTER", 0), true)` + `refresh_state_graph()`. Restore with the `"Player"` graph.
This is the only route to the NPC combat maneuvers as *autonomous* behaviour (evade dodge/break, buzz pass-by,
trail, strafe, engine kill, missile/mine/countermeasure use by the personality's timing). Whether the PLAYER-owned
ship then picks targets and fires on its own is unknown (target choice is server-side scanner/job logic;
`submit_forced_target` is the fallback).

### 2d. Not recommended
- Writing the behaviour manager's fields (ChaosMod "Sleepy Player" pokes the FreeFlight steering vector at
  manager+292): the controller already steers through 0x4C7CB0 natively.
- Calling `set_current_directive` directly on the client manager for maneuvers the dispatcher already builds: the
  server is not told (no `SetManeuver`), so mission conditions and the HUD maneuver state can disagree.

## 3. Risks and how to hand control back

| Risk | Detail | Mitigation |
|---|---|---|
| Fighting the player's input | While a directive runs, the steering override still calls `update_current_behavior_direction`; whether stick input is ignored, blended or cancels the directive is not known (`FLIGHT_AIM.md` "Open ideas": goto/formation keep weapons offline; "firm stick input could cancel autopilot" is still an idea) | Decide in the controller: any firm stick/throttle input → `Execute(19)` (FreeFlight), exactly what USER_CANCEL does. Never re-issue a maneuver the player cancelled. |
| Stuck states | `LockManeuvers true` (lane rides, convoys) and `SetPriority = ALWAYS_EXECUTE` ObjLists (M10 Bruchsal follow) are the mission's own locks; a mod directive could override a mission directive or be overridden; `Act_PobjIdle` without a visible un-idle (M01a/M01b/M02 runbooks: "Uncertain") | Read `get_current_directive()->op`, `bLockManeuvers` (+0xBC) and the priority (+0xC8) before acting; **never submit anything while a mission directive with priority or a lock is active**, or during cutscenes/lane rides/docking (`IsPlayerInCutscene`, `is_using_tradelane`, `is_launching`, `is_jumping`). Hand back with FreeFlight (19), not Cancel, so the player ends in the state the game expects. |
| Mission scripts | Conditions like `Cnd_PlayerManeuver` need the server report; scripts that `GiveObjList` the player expect their directive to stay | Use 0x4D8E20 (reports `SetManeuver`) for player-facing maneuvers; keep mod directives out of missions entirely unless the user asks (assist modes off while a mission ObjList is active). |
| State graph swap | The player ship as FIGHTER may keep NPC behaviour after a load/death/system change, may react to the HUD/target code differently, may not fire (player weapons are client-fired) | Harness only. Restore `"Player"` on every exit path (bot off, death event, mode change); test on the free-roam saves only; close the game after. |
| Struct layouts | Only Goto/Follow/Dock/Trail/InstantTradelane/Launch fields are known [SDK]; the rest are byte blobs | Always use the exported constructors; leave unknown fields at their defaults; prefer 0x4D8E20, which fills them itself. |
| Hot reload | Ops have vtables in Common.dll (fine), but a directive holding pointers into FLControllerImpl.dll (callbacks) would dangle after `reload` | No callbacks from the logic DLL; callbacks only in the host. |

## 4. What is feasible

### Controller module (assistive, opt-in, never against the stick)
Ranked by value for a casual pad player who loves gimbal aim:
1. **Trail target** (op 3 via 0x4D8E20): one button, the ship stays on the selected target's six at the trail
   distance; the player only fires (gimbal aim keeps working). D-pad up/down while trailing = closer/farther (the
   game's own TRAIL_CLOSER/FARTHER). Cancelled by any firm stick input or the same button. This is the most "NPC
   combat maneuver" the game offers the player without touching the state graph. Low risk: the game's own code path.
2. **Face target / face point** (op 15): snap-to-face for turrets, docking alignment, or "look at objective".
3. **Strafe run** (op 13): an NPC attack pass on the target (for larger targets), cancelled by input.
4. Goto with no target = "cruise straight ahead" (already reachable via the bar's goto with no target).
5. An "escort/follow" assist (Follow directive on a friendly, e.g. wingman in free roam) via `SubmitDirective`:
   only outside missions; useful for long cruises.
Not recommended for the controller: Evade/DrasticEvade/Buzz on the player (cut features, no client path), or the
FIGHTER state graph (that is auto-steer).

### Test harness
- **Now (low risk):** native goto/follow/dock/formation by `SubmitDirective` or 0x4D8E20 replace hand-steering in
  runbooks (M01a step "Harness gap: flying to a position or object"). The parallel session builds these in `flctl.ps1`;
  the dispatcher/direct-op details above are for that work.
- **Game-AI combat bot:** `bot npc <pilot>`: state graph FIGHTER + a pilot personality from `pilots_population.ini`
  (e.g. `light_fighter_default`) + `submit_forced_target` on the hostile target. If it works, it fights exactly like
  an NPC of that rank (evades, buzzes, uses missiles per the personality), which is a better "realistic opponent
  behaviour on our side" reference than the mod-side bot, and it frees the bot from aim code. The mod-side `bot
  fight` stays the reference for the pad pipeline (it drives the real input path; the NPC bot bypasses it).
- Middle ground if the swap fails: a mod-side state machine that issues native directives in a loop (Trail while
  far / Strafe when closing / Goto a point to break off), with the controller's own fire logic.

## 5. In-game test plan (run later, when the test install is free)
Prerequisites: debug commands in `src\impl\debug.cpp` (logic only, hot-reloadable) — `ai state` (current op
`get_current_directive()->op`, behaviour index +0xC0, lock +0xBC, priority +0xC8, state graph id), `ai man <op> [0|1]`
(calls 0x4D8E20 with `this = *(void**)0x674A58`), `ai dir <goto|follow|dock|idle|cancel> <id|x y z> [range]`
(server.dll `SubmitDirective` with exported constructors), `ai graph <name>` / `ai personality <pilot>` (2c),
`ai target <id>` (`submit_forced_target`). Save: `SaveC702` (free roam in space near Manhattan, traffic). Drive with
`Wait-FLUntil` on `ai state` / `ship`; no fixed delays; close the game at the end.

1. **Baseline:** `ai state` in free flight → op 19 (FreeFlight), state graph = PLAYER id. Press the bar's goto on a
   target (`Set-FLTarget`, USER_MANEUVER_GOTO) → op 2; USER_CANCEL → 19.
2. **Trail (op 3):** target a friendly police ship, `ai man 3` → op 3, distance to the target converges and holds
   (`Get-FLContacts`); TRAIL_CLOSER/FARTHER change it; stick input: does the directive survive, blend or cancel?
   (decides the controller's cancel rule); `ai man 19` returns control.
3. **Face (15) / Strafe (13):** same, with a station (face) and a hostile fighter after `Start-FLCombatTest` (strafe).
   Check that `Execute(5)` (corkscrew evade) really does nothing.
4. **Server directives:** `ai dir goto <Manhattan ring id> 500` (ChaosMod parameters) → op 2, arrives;
   `ai dir follow <npc id>` → op 17, follows with the offset; `ai dir idle` → stops (the HDE "full stop");
   after each, stick input and `ai man 19` hand back.
5. **Mission safety:** on an M02 save during `follow_king` (runbook M02) read `ai state` only: op 17, priority and lock
   values = the pattern the controller must leave alone.
6. **NPC brain (2c):** `Start-FLCombatTest`, `ai personality light_fighter_default`, `ai graph FIGHTER`,
   `ai target <hostile>`: does the ship maneuver on its own (evade on damage, buzz, trail)? does it fire? Then
   `ai graph Player` and verify normal flight, also after a death (`selfdestruct`) and a reload. Record kills/min for
   comparison with `bot fight` (3–4/min).
Add every verified fact to `docs\FREELANCER_RE.md` (a new "AI / behaviour manager" section) and update this file.
