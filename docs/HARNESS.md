# The test harness: flctl.ps1, flsub.ps1, debug channels

Reference of the harness functions and debug commands. Moved here from the private maps on
21 Oct 2026 (the audit's CLAUDE.md diet). The rules for using them are in `docs\TESTING.md`. The functions
themselves: `tools\harness\flctl.ps1` (133 functions, in its parts), `tools\harness\flsub.ps1`; debug commands:
`core\game\debugverbs.*` (the pad's: `modules\controller\src\impl\pad_verbs.cpp`), the shared ones in `core\game\room.cpp`.

**Load `.ps1` files only in the PowerShell tool, never from Bash** (each call is a new process; dot-source the harness
in every call). `$env:FLVIBED_INSTANCE = '<n>'` points a PowerShell session's harness at parallel game instance n.

**Parts (since 21 Oct 2026):** `flctl.ps1` holds the shared variables and dot-sources, in order, `flctl.core.ps1` (debug
channel, pad, frames, log, deadlines, waits), `flctl.rooms.ps1` (story state, blockers, rooms, launch, base repair /
restock, base stories, space waits), `flctl.space.ps1` (contacts, targets, docks, the game's maneuvers, cruise and
teleport aids), `flctl.missions.ps1` (event log, objectives, mission fights, dialogue, plan B, stalls, jobs),
`flctl.travel.ps1` (travel, jumps, joining, formation, stop), `flctl.ui.ps1` (Goto-FLRoom, Select-FLItem, events),
`flctl.saves.ps1` (scene saves - every read-modify-write of `scene_saves.tsv` under the machine-wide lock
`Enter-FLSceneLock`, parallel legs write it -, Load-FLSave, frames, peeks, console, godmode), `flctl.autopilot.ps1` (combat test setup,
mission ships, the autopilot, focus). The parts share one scope; never load a part alone. New functions go into the
part of their topic; `deploy.ps1` parse-checks every `*.ps1` in the tool folders.

## tools\harness\flctl.ps1
- The game's boxes (M01's tutorial boxes, mission popups, the launch briefing) are dismissed natively from every
  harness wait (`Step-FLPopups`, at most every 0.4 s; Load-FLSave clears one blocking its pause menu): tests in a
  tutorial area never stall on them (the user, 28 Oct 2026). A loop that sleeps instead of waiting calls
  `Step-FLPopups` itself; a test whose subject is a box sets `$global:FLKeepPopups = $true`.
- Core: `Send-FL <debug cmd>`, `Wait-FLUntil` (poll a state condition; every timeout prints `Get-FLStoryState`),
  `Get-FLMode` / `Get-FLFrame`, `Get-FLLog`, `Get-FLEvents` / `Wait-FLEvent` (game events), `Set-FLDeadline` (live
  watchdog: every wait throws OVER BUDGET), `Focus-FLGame` (a no-op in background mode).
- Virtual pad: `Set-FLPad` / `Press-FLPad` / `Clear-FLPad`. Never use screen coordinates in tests: named controls,
  D-pad navigation, values from `state`. It is merged after the pad's read: the focus-free harness hand, no slot.
`owners` (debug): who holds each command down, pad, autopilot or bot (`core\game\input_rules.h`); `tests\input_owner.ps1` reads it.
- Device-layer test pads (30 Oct 2026, IDEAS T36 slice 1; shared memory v2, `modules\controller\src\shm.h`): `Enable-FLDevPads
  [-Foreground]` / `Disable-FLDevPads`, `Set-FLDevPad -Slot 0-3 [-Connected $false] [-Buttons ..] [-LX ..] [-Vendor
  xbox|playstation|nintendo]`, `Press-FLDevPad`, `Get-FLDevRumble [-Since <seq>]`. While enabled the game's XInput
  slots 0-3 are served from shared memory instead of the system's XInput (`gamepad.cpp`; a real pad is ignored), so a
  test pad takes a real pad's whole path: the slot scan (`pad_slots.h`: the first slot that answers, a rescan every 1.5 s
  of game time while none is in use - wait for ` slot=<n> ` in `state`'s `pad` line), unplug / replug,
  `pause_on_disconnect`, the focus gate (closed in a background game; `-Foreground` opens it - an input held across the
  opening stays ignored until released), the glyph family from `-Vendor` (`padhud`'s `glyphs family=`), rumble as a
  command stream (slot, motor speeds, the game's heartbeat). Sweep section `pad-device` checks all of it.
- The pad backend (`pad_backend`, 30 Oct 2026): 0 XInput (default, everything above), with SDL3 beside it for a Sony or Nintendo pad in the HID list (`pad_sdl_fallback`,
  native first; a test game ignores the machine's pad, debug `padfallback hid|0|1` gives the answer, `padfallback rumble` a pulse: `tests\pad_fallback.ps1`), 1 SDL3 on
  the host's own thread (`modules\controller\src\host\sdlpad.*`, HostApi v33, its filter v38; the pure rules `modules\controller\src\pad_backend.h`, unit test
  `pad_backend`). `Send-FL 'set pad_backend 1'` starts it live (`set pad_backend 0` stops it); `state` then has a
  `pad_backend=1 source=sdl|xinput sdl_status=<-1 failed, 1 starting, 2 running> sdl_init_ms= sdl_pads= sdl_connected=
  sdl_type= sdl_vendor= dropped= sdl_name=` line under the `pad` line (absent with 0 while SDL never ran), the log
  `pad backend: SDL3 ...` / `SDL3 pad backend: ...` lines. The virtual pad merges after SDL's read as after XInput's;
  while the device-layer test pads are enabled XInput reads them even with `pad_backend` 1 (SDL keeps running, unread).
  Every pad reader follows `pad_backend` (since Q5 slice 2 also the options page's own reader, `uikit.cpp`: it stayed on
  XInput before). Out of the game the backend was probed with SDL virtual Xbox / DualSense pads
  (`research\evidence\sdl3\build_results.md` "Slice 1").
- SDL virtual test pads (30 Oct 2026, IDEAS T36 slice 2; shared memory v3, `modules\controller\src\shm.h`; TEST
  ONLY: refused in multiplayer and while SDL is not running): with `pad_backend` 1, `Connect-FLSdlPad -Type
  xbox|ps5|switchpro [-Slot 0-3] [-Foreground]` attaches a pad through SDL's own virtual joystick driver with the real
  pad's USB ids (045E:0B12 Xbox Series, 054C:0CE6 DualSense, 057E:2009 Switch Pro: `modules\controller\src\pad_backend.h` kTestPads), so
  SDL names its type as for the real pad and it takes the whole SDL path - the type -> `state`'s `sdl_type` / `sdl_vendor`
  and the glyph family, buttons, sticks, triggers, hot-plug; `Set-FLSdlPad -Slot n [-Buttons ..] [-LX ..] [-LT ..]` (the
  XInput form, as Set-FLPad), `Press-FLSdlPad`, `Set-FLSdlTouch [-Finger 0|1] [-X] [-Y] [-Up]` (a finger on a `ps5` test pad's touchpad, 0..1; the host's `sdlpad touch`), `Disconnect-FLSdlPad [-Slot n]` (none = all), `Get-FLSdlPad` (the host's
  `sdlpad status`: backend status, the slot in use, the attached mask, the read-back count), `Get-FLSdlPadOut [-Since
  <seq>]`: what SDL sent to the device - Kind rumble (A / B = low / high motor 0..1), triggers (left / right), effect (A =
  its bytes), with the game's heartbeat; SDL resends a running rumble every 2 s and sends the stop at its duration's end.
  While one is attached the newest is the pad in use (a real pad is ignored meanwhile; its detach hands back to the
  older one). `-Foreground` opens the focus gate in a background game while a test pad is attached (as
  `Enable-FLDevPads -Foreground`). The host's debug command behind them (the game thread posts, the SDL thread does every
  SDL call, at most 1 s): `sdlpad [status] | attach <xbox|ps5|switchpro> [slot] | detach [slot|all] | send rumble|triggers
  <0-65535> <0-65535> <ms> | send effect <1-64 bytes>` (`send` goes to the pad in use the way the game's own would: the
  game itself sends only rumble so far - trigger rumble and effects are IDEAS U30 / U31). `Set-FLSdlForeground
  $true|$false` flips the foreground flag while the test pads stay attached (the closed gate with a pad attached). The unit test `sdl_testpad` covers the
  pure pieces (kinds, the XInput form -> raw device values -> back, the pad in use). Not recorded by the dry-run recording
  (as the device-layer test pads). Sweep section `pad-sdl` checks all of it per type. A real pad may be plugged in: the
  section reads the machine's state with no test pad first and waits for that state after each detach, because SDL rightly
  falls back to the real pad.
- Haptics (30 Oct 2026, IDEAS U30; design `HUD_CUES.md` "Haptics"): debug `haptics [status] |
  table | play <effect> [level 0..1] | stop`. `status` (a read): `rumble=`, each category's gate (`world=` / `assist=` /
  `prompt=` `open` or why silent: `rumble 0`, `controller off`, `focus`, `device kbm`, `menu`, `cutscene`, `docked`,
  `paused`), the mix and the last values sent, `on=`, `sends=`, `playing=`, `duty=` (the fatigue average), the mixer's
  counts (`started merged silent fatigued full ignored`), the game's own events seen (`game_hits game_fails handbacks
  popups`) and `last=<effect>@<level> <result> src=game|debug`. `play` feeds an event into the same gates and mixer as the
  game's (TEST: the mixer path without a fight) and answers `haptics play <effect>: started|merged|silent (<why>)|...`.
  Events: `haptics started <effect> <level> src=game|debug` per start, `haptics silent ... why=` for the injected ones.
  The effects reach the test pads' device streams (`Get-FLSdlPadOut` with `pad_backend` 1, `Get-FLDevRumble` on the
  device-layer pads) only while the pad is the device in use - `set input_mode 1` in a background game - and the gate is
  open (`-Foreground`). Sweep section `pad-haptics`: XInput and SDL streams, the hand-back's pulse, a hull hit + a merged
  one, the confirm's two ticks, silence out of focus and paused (the prompt still plays paused), one real hit from two
  spawned pirates.
- Rooms and story: `Load-FLSave <file>` (native, by file name, from any state; refuses a stale slot - saves are read at
  startup, `deploy.ps1 -Save` prepares one), `Goto-FLRoom <label> [-NoWait]` (room buttons by function, waits on game
  state; "already in <room>" at once for a dealer room the player is in - FREELANCER_RE.md "Room hotspots"), `Start-FLLaunch`, `Skip-FLAnimation`, `Wait-FLRoomReady`, `Close-FLBlocker` (story popups, mission briefing),
  `Complete-FLBaseStory` (a base's story talks and room scenes played out, then the player's base stop -
  `Invoke-FLBaseRepair` (the game's repair window) and `Invoke-FLBaseRestock` (nanobots, batteries and the ammo of the
  mounted launchers - debug `equipment` + the launchers' projectile_archetype - at the equipment dealer, any base:
  filters searched, lists scrolled, the game's own buy; keeps the repair just paid in cash; purchases are net-worth
  neutral) - then the launch: no per-mission room lists), `Get-FLStoryState` (mode, room, open dialogs, offers, last
  story event), `Select-FLItem <control name>`, `Wait-FLSpace` (waits out story cutscenes in space),
  `Send-FLConsole <cmd>`.
- Space: `Get-FLContacts` (objects of the system, any HUD filter; `-Hostile`: Attitude, TargetId, Attacker),
  `Set-FLTarget <name|id>`, `Start-FLDock [name]` (target, dock request, the base's reply, docking cutscene skipped),
  the game's own maneuvers `Start-FLGoto <object> | -Position x,y,z | -Objective`, `Start-FLTradeLane <ring> [-Toward
  <ring>]` (enters by the autopilot's fast entry; `-Native`: the game's dock maneuver), `Start-FLJump <gate|hole>`,
  `Start-FLFormation <ship id>`, `Join-FLShip` (formation where the game allows it, else the autopilot's follow),
  `Stop-FLManeuver`, `Get-FLPilot` (autopilot state); every approach follows a hazard-free route
  (`tools\harness\flroute.ps1`: minefields, danger fields, damage zones from the system files, the game's zone test).
- Autopilot: `Start-FLTravel <object> | -Position x,y,z [-System s [-Object o]]` (the autopilot's fastest route: lanes,
  gates, fast docks, obstacle avoidance; a travel with no destination = the mission's own markers), `Start-FLAutopilot`
  / `Wait-FLAutopilot` / `Stop-FLAutopilot` / `Get-FLAutopilot`, `Start-FLCombat` (real kills), `Start-FLMissionCombat`
  / `Invoke-FLMissionFight` (reach to the nearest hostile, no fleeing in mission battles), `Start-FLCombatTest`,
  `Update-FLIdlePark` (the harness parks the ship with the autopilot's hold mode while it waits), `Resolve-FLObjectiveTarget`.
- **The situation snapshot** (30 Oct 2026, `A37_PLAN.md`): `Save-FLSituation -Path <file> [-Expect
  'placeholder=.. standin=.. kind=.. source=.. subject=.. held=.. verb=..'] [-Note <text>]` writes `ap situation snap`
  (reads only) to a file and returns Path, Hash, Step, SnapMs, Bytes and the snapshot's contact ids; it throws on a cut
  reply. The file is the `snap v2` text (`modules\autopilot\src\situation_io.h`; v2 since A37 S2: `script ... source_live=` - the
  goals / leashes are the live trigger table's -, the ini view beside them as `inigoal` / `inileash`, each `livetrig`'s
  `leads= hops=`; v1 files still parse): line 1 `snap v2`, then one record per line,
  `<record> <field>=<value> ...` with the fields in a fixed order - `obj` (seq, title, marks), `step` + one `entry` per
  objective entry (text, refs and each ref's resolved space id, navmarker, verb, the job kind of the text id), `script`
  (mission.cpp's stand-in: from_last, by_title, the stand-in text, the script, the next objective, the pending-hostile
  ships), `objdef` (the [NNObjective]s naming the text), `goal` / `nextgoal`, `leash`, `nag` / `nextnag`, `live` +
  `livetrig` (the live trigger table, its read time), `place`, `pilot`, `holds`, `settings`, `exec` (the autopilot's own
  state), `contact` (the 48 nearest + every one the rules name), `lane`, `waypoint`, `spawn`, then `end`; ids and numbers
  only, scripts relative to DATA\MISSIONS, strings percent-encoded, floats exact (`%.9g`). `#` lines are comments: the
  last is `#game-hash 0x........ step=0x........ snap_ms=..` (the hash the game's assessor gave this very snapshot),
  `-Expect` adds `#expect ...` for the unit test. `build\test\fltest.exe -snaps <dir>` checks a folder of them host-free
  (each hash = its `#game-hash`); the fixtures are `tests\fixtures\situation\*.snap`. A fresh game per fixture: the
  host's objective title and mission marks carried over from the previous save's game in one process (A37 S1; fixed 30
  Oct 2026 - they start over at each load - but the recorded fixtures stay fresh-game ones; the six named saves' fixtures
  are v2 since A37 S2, the job fixture v1). `ap situation` (plain) ends
  with a `timing snap_ms= live_ms= live_rows= story=live|ini` line (not part of the hash; also logged as `situation:` in
  FLController.log); its `story source=live|ini reason= both= only_live= only_ini=` and `diff goal|leash only=live|ini
  name= detail=` lines are the live table's view against the ini derivation's.
  **`snap v3` and the shadow (A37 S3, 30 Oct 2026):** v3 adds the facts the arbiter's rules read (`obj ... refs_obj=`,
  `armed by= by_file=` + a goal's fields - what each ini goal arms -, goal objects `id:placed:x:y:z`, `leash ... off= ship=
  placed= opos=`, `place ... entry= entry_pos=`, `pilot ... tractor=`, `settings ... tractor_exit_*`), `queue` (the
  executor memory the rules read: modes, the running travel, leash / last dock / dead goals, the destroy memory) and
  `decided rule= mode= target= action= keep=` (the old path's decision; `rule=` empty when read off the objective run).
  With `ap set situation_shadow 1` every ObjectiveDecide exit the S3 rules cover logs one event `situation shadow
  agree|differ <branch> old=<mode>,<target>,<action>,keep= new=<rule> mode= target= action= keep= text= step= ms= why=`, and
  ObjectiveSupervise's holds `situation shadow agree|differ held:<mission_holds|game_dock|lane|jump> ...` (once a second at
  most, an event only when the verdict changes); `ap situation last` = the last shadowed decision's snapshot (its `decided`
  record, a `#shadow` line, `#game-hash`) - a fixture whose decision `tests\situation_rules_test.cpp` checks.
  **The arbiter (A37 S7, `ap set situation_arbiter 1`; AUTOPILOT.md):** per decision `situation arbiter agree|differ
  <rule> mode= target= action= keep= old=<rule>,<mode>,<target>,<action>,keep= plan=agree|<diff> ok= step= ms= why=`; per
  start or refusal `side-effect <travel|dock-trip|dock|pass|shoot|hold|waypoints|mode> target= step=0x.. rule= [keep|refused|
  adopt why=]`; `situation pause step_key= ...` at the player's stop, `situation resume <silent|ask|nothing> cold= same_step=
  paused_ms= moved= ... why=` at the next start's first decision; `ap situation last` then ends with an `#arbiter` line, `ap
  situation queue` prints the goal queue (`goalqueue v1`) and its pause.
  **`snap v4` (A37 S4, 30 Oct 2026):** the S4 rules' facts - `after leads= file=` + a goal's fields (the goals of the step
  each ini goal leads to: the pre-stage's), `prestage last_real= next= next_ok= next_pos= next_text= stage_from= stage_ok=
  stage_pos= stage_text=` and `realnext` goals (NextStepGoals / NextObjectivePlace from the objective mode's last real
  objective), `obj ... title_escape=`, `pilot ... system=`, `settings ... big_coast= survival_calm= survival_stage=
  survival_until_jump=`, `contact ... radius= named=` (pointed contacts only), `queue ... last_real= arrived= step_age=
  escorted= surv_title= surv_mission= surv_ended= surv_system= survival=`; v1-v3 texts parse and hash as before. The
  shadow's branch names grow by `pre-stage`, `pre-stage-next`, `mark-fight`, `pre-aggro`, `survival-stage`, `survival`,
  `threat-fight`, `escort`, `ship-target` (ObjectiveDecide's exits from the pre-stage to a ship target).
  **`snap v5` (A37 S5, 30 Oct 2026):** the S5 rules' facts - `step` / `entry ... ref_placed= ref_pos=` (the first resolved
  ref's position: the route's step 1), `contact ... dock_ring=` (a pointed / referenced solar's planet docking ring; `radius=`
  now also for the entries' refs), `lane ... next_placed= next_pos= prev_placed= prev_pos=`, `settings ... follow_distance=`,
  `queue ... mission_leader= via_next= arrive_retries= tr_pass=`; a shadowed decision's `waypoint` records are the list as
  the decision began; v1-v4 texts parse and hash as before. The last branch names: `lane`, `mission-form`, `pass`,
  `route-lane`, `arrived`, `route`, `waypoints`, `arrive`, `nav-way`, `via`, `navmarker`, `rest` - every ObjectiveDecide
  exit is shadowed now (ObjectiveSupervise's own starts are not).
- Speedrun legs (22 Oct 2026, `docs\LEGS.md`): `missions\Leg.ps1` (one leg of any mission: base story by
  `Complete-FLBaseStory -AcceptStory`, space by the objective mode; -BudgetSec stops an off-pace leg; -StallSec 60, the
  stall watchdog: no movement of 300 m and no change of the autopilot's kills / damage dealt / target / action for that
  long ends the leg STALL - user, 23 Oct 2026: stuck tests are a time waster (comm lines count as progress, but a
  docking letterbox, control-exclusion 4, held over 120 s ends it regardless - 30 Oct 2026; a story-unfit checkpoint
  load ends it at once as HARNESS; 5 autopilot stops in a row at time 0 with the same why end it `AUTOPILOT LOOP` - the
  restart loop the watchdog missed while the ship flew on unflown, set ab11 i5, 31 Oct 2026; the leg's death is the
  `player-death` event (the death camera), never the autopilot's `why=player died`); segment practice `-SaveAt <cps>` /
  `-StartCp <cp> [-StartSave <file>]` / `-StopAt <cp>` with the save-fit check: the ship's state at a save in its
  `scene_saves.tsv` row (a checkpoint passed moving defers its save to the first rest on the same step, 90 s at most -
  `SAVE <cp> deferred / at rest / dropped`; never while the objective reads 21660 "Awaiting"; the mission marks up are
  recorded in `marks`; a save with a marked mission solar up that the save will not keep - its [MsnSolar] `system =`
  not the player's, the game still listing it (`objobj`) - is `unfit` at once, `SAVE <cp> unfit: ...` (LostSolars over
  `spawn-solar` events and flctl `Get-FLMissionSolars`; S416); 30 Oct 2026), the first run from it writes `fit-load` / `unfit` (+ `fit_note`) and logs
  `SAVE-FIT` (marks not back within 5 s of the load = unfit, the run ends HARNESS), `fit` once that run reached its first
  checkpoint, `unfit` when it ended STALL before it - `docs\TESTING.md`
  "The test loop" 4), `tools\harness\flleg.ps1 [-ApSet 'k v','k v'] [-Cmd 'godmode on'] [-NoFailStates]` (through `pwsh -File`: double-quote a value
  with a space - a single-quoted one splits)
  (autopilot switches sent after the boot - a set's A/B in one build; `tools\flset.ps1` runs one set side) `tools\harness\flleg.ps1
  -Instance n -Save s -Label l` (a leg on its own instance with evidence, history `research\experience\legs\legs.tsv`
  - its commit column is the build the leg deployed, from `build\dev\build_info.txt` that `tools\buildlock.ps1`
  writes: HEAD, "+dirty" only when the build's own inputs had uncommitted changes (29 Oct 2026; a plain git status
  before) -,
  a leg-end save catalogued as scene `<label> end`; a leg that does not pass keeps `end_tail.mkv` + `end_sheet.jpg`,
  the last 15 s of the recording; the motion trace `FLMotion.tsv` is kept with every leg; `-Save` by name looks in
  `research\saves\bc46-all-missions\`, then the leg checkpoints `research\saves\legs\`). `Complete-FLBaseStory` also walks to the room the objective names
  (`Get-FLObjectiveRoom`) and tours the rooms while launching is not allowed (`Step-FLRoomTour`). Commodities:
  `Invoke-FLTraderBuy @{ commodity_x = n }` (0 = what fits). Market data: `tools\flmarket.js`.
- **One client at a time per game channel:** `Send-FL` / `Send-FLS` hold a named mutex per channel (22 Oct 2026: a leg
  runner and a check-in on one game overwrote each other's command slot - lost / crossed answers).
- Missions: plan B `Update-FLPlanB` (the next action inferred from the objective text + spoken lines, run alongside
  every mission wait; `Get-FLMissionHint`), stalls `Update-FLStall` (`research\experience\stalls.jsonl`), jobs
  `Get-FLJobOffers` / `Start-FLJob` / `Invoke-FLJob` / `Get-FLJobBases` (load a docked save once before them: after
  `deploy.ps1 -Save SaveC701` a second `Load-FLSave` of it was followed by `offer-gone` for all 8 offers and none new,
  A37 S1; expected pay; offers, objectives' targets and
  navmarkers are host events; `research\experience\jobs.jsonl`), `Invoke-FLScanShips`; **reading events for a test step**
  (26 Oct 2026, the parity follow-up): mark the host's sequence first (`events 999999999` -> `last N`) and read
  `Get-FLEventLog N` after `Start-FLEventLog` ran once in that script (a first `Get-FLEventLog` without it starts the
  log at "now" and loses what came between the mark and the call); never judge a step by `Get-FLHistory` (the host's
  whole ring: t3 reported a 21 s "why=player input" stop in a background job that was a manual K test's stop from two
  minutes earlier). A start pressed in the launch's tail is refused (`autopilot start refused reason=key (no ship or a
  cutscene)`): wait for the answer event and press again (`research\experience\runs\autopilot_parity_2026-10-26\tools\t5.ps1` jobs, `t6.ps1` parity); mission scripts
  `tools\harness\missions\` (Campaign.ps1: New Game to M02; M01a, M01b, M02; M03objective / M04objective / M05objective:
  rooms by the harness, space by the objective mode).
- Test aids: `fault frame|draw` (the logic's next frame or overlay pass raises an access violation), `fault <module> frame|draw` (that module's own place: npc, speedrun, autopilot, hud), `fault status` (which stopped; `tests\fault_giveback.ps1`), `watchdog test|off` (a real goto that reads as stopped: `tests\watchdog_modules.ps1`), `core <verb ...>` (with the controller off, on the subtitles' channel: the verbs of core's `debug::Execute`; the mp, speedrun and fault tests shim `Send-FL` through it), `startprompt open|close|pick <mode>|state` (the keyboard's New Game list), `godmode on|off` (debug), `Set-FLCruiseSpeed`, `Move-FLShipNear` (pinpoint tests only; size-aware), `stall <ms>`
  (debug: the game thread spins, at most 60 s - a frozen game for the freeze watchdog `tools\flfreeze.ps1`),
  `fpscap <fps>|0` (debug: caps this game's frame rate, a high-resolution timer at the frame's start - the owner, 29 Oct
  2026: test games at no more than 60 fps; with flleg `-Cmd "fpscap 60"`; a leg's `fps=` then reads ~60, compare legs
  run at the same cap; `godmode on` sent mid-session does not protect - the flag needs a load).
- Benchmarks: `tools\harness\flbench.ps1` (Enter-FLStream, Invoke-FLBenchRun, Invoke-FLABRun / Invoke-FLABBench,
  Enter-FLScenario <name>, Invoke-FLBenchRun -UntilSpawnsDead, Invoke-FLScenarioAB; results in `research\bench`),
  `tools\harness\flfights.ps1` (fight matrix), `tools\harness\flsweep.ps1` (regression sweep), `tools\flbenchhist.ps1`
  (timing history across sweeps), `tools\fltest.ps1` (one test start with run folder + manifest).
- Sweep section `target-churn` (0.2.1, 2 Nov 2026): SaveC705, a game formation on the nearest friendly, a game goto to
  a station > 4 km and the Advanced Autopilot's dock, each with 6 s of target churn (next enemy, clear, double clear,
  station / planet / sun / farthest object, 0.15 stick nudges); PASS = maneuver kept (or ended at its goal), no stall
  (< 5 m/s > 3 s with the goal > 150 m), formation held within 300 m, goal not receding, FLvibed answering; reports the
  maneuver watchdog's lines (`event maneuver stalled` - since 4 Nov 2026 with the leader / goal's `docking= lane=
  jumping= speed=` -, `maneuver watchdog: re-issue ...` = free flight, then the maneuver again; setting `maneuver_watchdog`).

## tools\harness\flsub.ps1
`Send-FLS`, `Watch-FLGame` (`-LinePattern`, `-ShotMs`), `Wait-FLSShown` + `Get-FLSLast` (for `-After`; works during
movies and across restarts), `Close-FLModal` (native clicks), `Set-FLSIntro` (switch test intros via freelancer.ini,
restorable), `Save-FLShot` (game window only), `Get-FLSLog`.

## Debug channels and logs
There are two debug channels, the controller's and the subtitles' (shared memory `Local\FLControllerShm[_n]` / `Local\FLSubtitlesShm[_n]`), and two
logs (`EXE\FLController.log`, `EXE\FLSubtitles.log`; `EXE\logs` keeps previous sessions). Freelancer reads the mouse only
while it is the foreground window; pad commands and every debug command work in the background. The virtual pad is
never focus-gated; the physical pad is ignored in a background game, in flight and menus alike (`focus_only`, one gate:
`modules\controller\src\impl\focus_gate.h`, unit test `focus_gate`; 30 Oct 2026) - the device-layer test pads go through that gate as
a real pad does (`Enable-FLDevPads -Foreground` opens it in a background game).

- **Shared commands in both channels** (`core\game\`): `click <Control>` (text, icon and front-end buttons; the reply
  adds `(nn-window busy ...)` when a docked NeuroNet window was sliding - the game drops a close then, retry after the
  slide; `hotspots` / `goto` say `busy: nn-window` meanwhile), `visible
  <Control>`, `hotspots`, `goto <label>`, `talkers`, `talk <n|offer>`, `menu <new|load|options|exit>`, `pause`,
  `pauseload`, `saves`, `loadsave <file>`, `pendingload` (the CHARACTER_ID the in-game load / death dialog last loaded),
  `ctlinfo <Control>` (addresses for RE), `prompttrace [reset]` (controller only: every change of the pills' decision and the
  hover help, per frame, the last 256: a one-frame blink shows here, a poll misses it), `uiskin jobheight <px>` (controller
  only: the job text's height as the plate reads it, 0 = the game's), `uiskin seams` (every shown control still drawing art nobody hid or claimed: the 3.3 audit of `modules\hud\docs\UI_HUD2_SKIN.md`, walks the tree, rests on no counter), `uiskin loadplace <0|1>` (the loading screen tries the player's base and system: its probe), `gametext frame <0|1>` (NN_Inventory on the one frame of R8, off until probe 18), `selfdestruct` (test aid: death dialog). `equipment` / `equipall` also print
  only: the job text's height as the plate reads it, 0 = the game's), `nnstrip [state]` (the strip of words: standing, its tab, the manager's open and
  held commands, its rectangle, the click box), `navskin click` (the map's own click at the cursor, after `mouse x y`), `nnpage [state]` / `nnpage press <a|b|x|y|up|down|left|right>` (the full-screen NeuroNet page: its state; a pad press into it), `nnstrip go <tab>` / `step <dir>` / `close` (what a click and the bumpers press), `nnstrip slide <seconds>` (each NeuroNet
  slide, 0 the game's), `navskin [state]` (the map's frame and words: bound, failed, every chip with its rectangle, the layer, the view), `navskin native on|off` (the
  old map from its next open: what `skin_native = navmap` will set), `navskin press <id>` (a chip's action), `navnew [state]|on|off` (the full-screen map, on by default; off gives the game's size back for a comparison: grown, transform, records, labels shown and dropped), `map view [state]|zoom <z>|pan <du> <dt>|records|rings` (the map's transform; every record with its `vis`; every lane ring with the game's name and the route card's), `selfdestruct` (test aid: death dialog). `equipment` / `equipall` also print
  `mines=` (the mines counted by ammo, as the countermeasures).
- **The controller's host part** (`modules\controller\src\host\main.cpp`; `uigen` in `core\host\uihooks.cpp`): `uigen` - the UI generation counter of HostApi v23 ("uigen N adds A removes
  R": the game's control list adds / removes; Extended HUD 2 finds windows by it; still in a calm room or calm flight);
  `sdlpad ...` - the SDL virtual test pads (TEST ONLY; above, "SDL virtual test pads").
- **Fixed dt** (30 Oct 2026, IDEAS T2; TEST ONLY, single player): `fixeddt <fps> | off` - every frame
  advances 1/fps of game time and the game runs as fast as it draws (the host's detour of the frame timer, HostApi v32;
  `docs\FREELANCER_RE.md` "Frame / game state"); with `fpscap <fps>` too it runs at about 1x (the neutral setup).
  `timing`: the frame timer's cap, clamp, dt, raw dt, game-time sum, fixeddt and fpscap. Since slice 2 (30 Oct) the
  server's own clock follows the game dt too (around the game's frame, `modules\controller\src\host\main.cpp` BeforeGame / AfterGame; verified: +10.28 s server for +10.23 s game
  in 2.75 s wall) and FLvibed's decision timers and velocity estimates run on one game clock (`core\game\gameclock.h`;
  FLMotion.tsv's `game_ms` column beside the wall `ms`). Still on wall time: the server's damage timestamps (Server.dll's
  `Timing::read`, unread), audio / comm lengths, the host-stamped comm / hit ages (objective leash, shield cue).
  `gameclock`: that game clock in seconds (30 Oct 2026, Q3 probe 4) - `flleg.ps1 -GameClock` (Leg.ps1) times the
  checkpoints (each at the clock when the harness reads its event: < 1 s late), splits and the total by it; the
  fail-state windows, timeout and stall watchdog stay on wall time. Under uncapped fixeddt the comm-gated story stays
  on wall time (Q3 probe 4: a game-time fail state before M13's battleship objective) - uncapped is for fight-only
  segments; `fpscap 60` + `fixeddt 60` (1x) keeps the comms in step. `seed <n>` (TEST ONLY, T3 common random numbers):
  srand(n) in the game's MSVCRT (Freelancer.exe, server.dll, common.dll import it; the game seeds it once from
  timeGetTime at 0x5B1B66) and in FLvibed's own CRT (the autopilot's jinks / dodges / slides), on the game thread.
- **Dry-runs: record a run, replay it with no game** (30 Oct 2026): `$env:FLV_RECORD = <file>` writes
  every `Send-FL` command with its reply and every `Set-FLPad` state; `$env:FLV_REPLAY = <file>` answers from it
  (`flctl.core.ps1` Add-FLRecord / Invoke-FLReplay). Epochs are cut at each action (a pad state or a command outside
  `$script:FLReadRx`, the read commands); within one a read returns its last recorded reply, `state`'s frame counts up
  per call, so a driver's polling may differ; an action other than the recorded next one throws "REPLAY DIVERGED at
  action <n>". A new read command a driver polls belongs in FLReadRx (else its polls cut epochs: `events` / `talkers`
  were added after the first recording; `ap situation` with A37 S1). flcrawl skips its game starts under replay. Checked: the crawler's -PlantCheck
  run recorded (CRAWL PASS screens=4 controls=24 edges=96) and replayed in 33 s with the same line; without
  -PlantCheck the replay stops at action 2 (DIVERGED). Fixture: `research\experience\fixtures\
  dryrun_flcrawl_plantcheck_2026-10-30.tsv` (local). Use: a changed driver first replays its last recording; a
  DIVERGED action is either the change itself (record anew in game) or a bug.
- **The focus crawler** (30 Oct 2026, IDEAS T7): `tools\harness\flcrawl.ps1 [-Instance n] [-PlantCheck] [-Space]`
  walks every screen's D-pad focus graph on the virtual pad from the main menu (load, the LAN / INTERNET prompt, Options;
  `-Space`: the pause menu on SaveC702): unreachable controls, dead ends, a press onto no control (the game's `hit` test
  decides when `uiitems` missed a thin control), the focus leaving a modal window, prompts, B back within 3; never NEW
  GAME / EXIT / a load / a delete, and it stops if the game's exit confirmation appears. `-PlantCheck` first plants a dead
  end (`navplant`) and requires the catch. Output `research\experience\runs\crawl_<time>\` graph.tsv + verdicts.txt,
  last line `CRAWL PASS|FAIL ...`. Debug for it: `uiitems` prints a main menu button's id (`main=0x..`, menu.h) and
  `navplant`; `navplant <ctl hex> | off` (TEST ONLY: that control is never a D-pad target).
- **Controller debug** (`core\game\debugverbs.cpp`, the pad's in `modules\controller\src\impl\pad_verbs.cpp`): `state`, `ship`, `uiitems`, `set <name> <v>` (live only; `config save`
  writes `FLController.ini`), `config` (`config settings`: every [Controller] and [Autopilot] setting with its value and
  help, from the registry `core\settings.h`; `ap settings` the autopilot's alone), `bot`, `events`, `peek` / `poke`, `snap` / `diff`, `findvt <value> [max]`
  (objects by vtable: a scan of committed private memory), `watch <addr> [len] [w|rw] | report | stop` (a hardware data
  breakpoint on the game thread: which instructions write / read it, with two callers; removed before a hot reload),
  `tap`, `mouse`, `targetinfo`, `destroytarget`,
  `contacts`, `settarget`, `perf` (flight model), `perf cpu` (the logic's CPU time per frame per entry point -
  OnFrame, OnRenderOverlay, OnRenderUnderHud, space::Contacts and its fresh scans - since the last `perf cpu`, the frame
  rate, the worst frame; resets; `core\log.h` perf::Scope), `rec start [file] | stop` (flight recorder: CSV per frame beside the
  exe), `bestpath` / `routes` (the game's best path into the waypoint list), `route [ring ...]` (the autopilot's plan
  from here along that list, the rings given closed as a disrupted ride closes them), `mission live [raw]` (the story
  mission's live trigger table, `core\game\mission_live.*`: per trigger id, nickname, state, the condition's vtable name /
  the ini's condition, a timer's elapsed / duration, a kill count; `none - <reason>` without one - no story mission, a
  stale id after the mission ended, another build; `raw` adds the layout; `docs\FREELANCER_RE.md` "The live trigger
  table"), `equipment` / `equipall` (the
  server's equipment list; every item with its active flag and ammo), `missiles`, `rounds`, `obstacles`, `laneopen <ring>
  [toward]`, `objobj <ref>`, `worth`, `eta`, `drainshield <id>` / `shieldof <id>`, `shot <cmd> <px> <py>`, `uihide`,
  `cue ...`, `hitlook ...`, `nnsev`, `spawn <npcshiparch> <faction|player> [count] [dist] [job= pilot= order= base=
  force= level=] | list | hunt | clear | clearat <hull>`, `ai state|man|dir|pilots|pilot|copy|field|fields|set|brain|
  cbrain|release|target|free|layout|rawdiff|self` (`ai self`: hull / shield fractions and maxima, power, gun range,
  nanobots and batteries n/max, tractor range, hold - `core\game\ai.cpp`), `ap start <mode> [id|x y z] | stop | status | pilot | set | clear | field
  | save | load | preset standard|no-consumables|speedrun|competent | lof <id> | goals | look | situation [snap|hash] | ray | scan | rayraw` (`ap
  situation`: the situation assessor over a snapshot of now - the step and its key, placeholder / stand-in, kind and its
  source, subject, held, threats, constraints, script goals with their live state and achieved, affordances, the hash;
  `snap` the snapshot text, `hash` the hash of the live and of the re-parsed snapshot, `roundtrip=ok|DIFF`, `snap_ms`,
  `bytes` - reads only, `modules\autopilot\src\situation.h`, `AUTOPILOT.md` "Situation assessor"; `ap goals`: the script
  conditions that advance the current step, live - text, system, title, from-last and each goal's kind, distance /
  count, whether the game places its object; `ap look`: LOOK L0's verdict now (kill / fly / none) and every fact it read
  - kill goals and their ships, fly goals with cost and lane, nag, leashes, tethers, the declared next place, the
  blockers (`AUTOPILOT.md` "LOOK L0"; the switch `look_log` logs it per decision); `ap ray x0 y0 z0 x1 y1 z1`: the ray test's first hit; `ap scan x0 y0 z0 x1
  y1 z1 half step`: a grid of parallel rays as a map ('.' clear, digits = the hit's tenth of the length); `ap rayraw
  ...`: every raw hit with object class, id, crc - a solar's moving part is class 0x1; session 6).
- **The UI build's commands** (25 Oct 2026; each module's `Command()` in `modules\<name>\src`, the exact syntax from its usage
  line): `ui options [open|close|cancel] | tab <n|name> | rows | press <up|down|left|right|a|b|x|y|lb|rb|pgup|pgdn ...> |
  focus <key> | default | card | state` (the FLVIBED options page: open standalone, switch tabs 0 hud, 1 controller, 2 subtitles, 3 autopilot, 4 more, the rows with their
  values and head states, queued presses for the next frame, the focus onto a row, this tab's defaults, the first-start card, the hosting state); `tools\deploy.ps1` writes
  the card's stamp `first_card = 1`, so a test start never stalls on it; `uibtn` (host-side:
  the native buttons' state, controls and placement, the raw-key counters), `uibtn click <1 bar|2 options page|3 pause>`
  (a click through the game's own release path), `uibtn mesh [<path relative to DATA>]` (the bar button's mesh for the
  next HUD build); `device` (the input-device service: pad / keyboard-and-mouse and why); `fluf` (FLUF awareness,
  `docs\MOD_COMPATIBILITY.md` FLUF: present 0 / 1 co-residency / 2 aware, the build, the status line, `fluf_windows`,
  text input, the cursor over a FLUF window, whether the pad drives FLUF, `palette` / `holdsKeys` (FLvibed's keys held),
  every open keep-out with its kind and rectangle and (bus v5) its ImGui name, input rule, focus and close-button point);
  `d3dreset` (host-side, background test starts only: the device Reset in place with its last parameters through the
  vtable, every module's Reset hook running; replies TestCooperativeLevel before / after and Reset's result);
  `winmode` (host-side: borderless windowed - setting, provider, the game's windowed style, DPI awareness, setups /
  rewrites / toggles, the window / client / monitor rectangles, render size); `winmode altenter` (host-side, background
  test starts only: one display setup as FLUF's Alt+Enter makes it, refused unless FLvibed's rewrite applies - never
  exclusive fullscreen; `deploy.ps1 -Background` itself refuses a game that would open fullscreen, `Assert-FLNoFullscreen`, and only `FLVIBED_ATTENDED` from `flsweep -Attended` passes);
  `ui ...` with the controller off answers on the subtitles' channel (`core\host\pagehost.cpp`); `hooks` (host-side, both modules: every detour's mode, place and where the function's own jump leads; each channel lists its module's hooks and the ones the router shares, the core's own on the controller's, `core\host\commands.cpp`); `padhud on|off|state` (the pad
  HUD layer: badges, legend, status line; the Classic HUD); `hud2 alert shield|hull|missile|cargo|tractor <3-6>` (that alert's card on the live HUD, 3 s, for frames: UI_HUD2.md 5.40; event `tractor-fail reason=` when the game refuses loot: `core\game\tractorfail.h`); `hud2 on|off|auto|state|sim <name>[+comm]|sim list|sim off|tree
  <root>|hit <x> <y>` (HUD 2, `modules\hud\docs\UI_HUD2.md`: forced on / off or back to `hud_style`, its state and the published
  layout (the nav line also `event` = the dock / lane / jump answer, `comm` = the subtitles' dialogue panel shows a line,
  `cine` = the letterbox, `fight` = the threat list's rule; HUD 2 FIX2 25 Oct 2026; the `preset=` line, `modules\hud\docs\UI_HUD2_PRESETS.md` 6 step 4:
  the layout preset asked (`hud_preset`) and drawn - "Focus (layout not built: Standard drawn)" until step 5 - its data's zones / budgets /
  pick radius and the controller setup it adapts to: `setup=` column, `fit=` its compatibility cell, `suggested=`, `needs=`; the `core` line,
  6 step 5 / UI_HUD2.md 5.17: the layout core's drawn preset, `suppressed=` its elements (`(budget)` = by the budget rule), `boxed=` / `cap=`, `overrides=`), a simulated state (`modules\hud\src\hud2_sim.h`, `modules\hud\docs\UI_HUD2_PRESETS.md` 6 step 2: the mock-up's `combat`, `travel`, `cruise`, `dock`, `lane` and the presets' contexts
  `ap` (the autopilot travelling), `apwait` (its objective run waiting), `turret`, `formation`, `jump`, `letterbox` (the jump's tunnel: the
  bars' HUD without the bars), `loot`, `capital`, `notarget` (a fight, nothing targeted), `offscreen` (the destination behind: drawn as
  on screen, the native arrow is the game's), `comm` (a dock under a timer while a comm line shows); `+comm` forces the comm surface over
  any (`lane+comm`, `ap+comm`; the subtitles' panel itself: their `test` command); an unknown name is refused), a native HUD root's control tree, the game's hit test at a pixel; `hud2 lint [off]`
  the layout lint over the last drawn frame (`modules\hud\docs\UI_HUD2_PRESETS.md` 6 step 3: the first call arms the recorder, the next prints one FAIL / WARN / n/a line each and the verdict; zones, budgets and prompt caps
  are the active `hud_preset`'s, named in the header line `preset=`; `set hud_preset 1` draws Focus's layout since 31 Oct 2026, UI_HUD2.md 5.18;
  since 31 Oct 2026 also `FAIL panel.overlap`: two drawn texts / glyphs / keycaps / icons of one panel - nav, card (+ card.icons, list),
  ship, loadout / fire strip, context, alert, hints, each centre component such as ring.band / ring.cruise - that overlap or sit under
  0.01 H apart; a designed unit (`hud2kit::RecUnit`: a prompt and its label, a slot's icon + count + key) and two lines of one text
  column may touch, never overlap - hud2_lint.h PanelGapPx, UI_HUD2.md 5.19); and the zone `bottom` (every preset, the owner's decision 4
  and lower-centre rule, UI_HUD2.md 5.20 / 5.23): ANY recorded item (plate, text, glyph, keycap, icon, arc, shape; only the transient
  overlays ring.hit / ring.missiles apart) in the lower centre x 0.40-0.60 W from the drawn layout's `clearY0` (the ring's lowest drawn
  edge + 0.01 H: Standard 0.6344 H, Focus 0.6333 H at 16:9 k 1, scaling with k; was 0.85 H) to the bottom FAILs `zone`; Standard's
  S2 ends at the top of the consumables' cross stacked on the loadout (two S2 rectangles); a marquee text is recorded with its room as its box;
  `hud2 state`'s layout line names the cross (`cons=`) and the prompts' lanes (`ctx=lanes [L0 x+w] [R0 ...]`, row 1 = above the baseline;
  `fallback` = no lane beside the panels had room: the same two lanes above the panels, never the lower centre - UI_HUD2.md 5.23);
  `hud2 peek [pin|off]` Focus's manual peek (everything it holds back, 3 s; again inside the window pins it; the pad / K&M inputs are
  PRESETS 6 step 7's), `hud2 state`'s `focus` line its reveal (`held=`, `peek=`, `fight=`, `form=`, the arcs, `speed=`, `escorts=`; no `hold=` since the hold strip went, B9); `hud2 viewback on|off`
  the dark backing under the game's 3D views with the target panel's pulses and the view's object, `hud2 viewzoom <f>`
  the views' rectangles over their windows, 1.5 by default; `hud2 tree roots` every top-level control of the game's
control list with its vtable, state and registry row, `hud2 tree <control name>` any named control's tree; `hud2 form [on|off]` the targeting display or the game's bracket, `hud2 marks` the game's indicator records beside the target's projection; `hud2 sim hazard` radiation, tail and lag, `hud2 sim groups` the cue's states; `hud2 lint` reports `steady=` beside `boxed=`; `hud2 peek` also on Standard); `hudinfo [wg|engine|lock|
  tractor|obj <id>|sys [id]|equip <arch>]` (HUD 2's readers, `core\game\hudinfo.h`, and their research dumps); `stalls [hold <id|0>]` (the NPC stall recorder `stallrec.*`: its counts and file `EXE\FLvibed_npc_stalls.log`; `hold`, TEST ONLY, makes that ship's samples stand still: an NPC's brain overrides every directive, so a real ship cannot be stopped from here); `track [range]` (ships and loot near the player, one line each with position, target, health, attitude: a harness samples NPC motion from it); `wgroups open|close|up|down|a|next|prev|state` (the weapon group editor: the group, the groups with guns, each gun's toggle number and on / off); `picker open|close|a|b|up|down|next|prev|cat <name>|state` (the target
  picker); `awheel open|pick <n>|commit|cancel|rear|state` (the action wheel; `wheel` is the mouse wheel); `goals
  open|close|toggle|tab <0-2>|up|down|a|x|y|left|right|rebuild|click <slot>|state` (the autopilot goal panel; `state`
  says form=pad|kbm, clickbox= and the keyboard + mouse form's hotspots; `click` = a click of its click box as the host
  routes it: 0-2 tabs, 3 the ESC mark, 4 / 5 scroll, 6.. the visible rows); `postkey <vk> <0|1>` (one real
  WM_KEYDOWN / WM_KEYUP to the game window - a Shift held across a `post <vk>`: the keyboard + mouse tests' Shift+K);
  `ap start smart` (the plain start of every device: objective, else waypoints, else the target); `bases [rooms|chars]`
  (the pad in bases: the screen, the room ring, the bar's characters, the prompt bar's state); `map [dest]` (the nav map, `view=` 0 universe, 1 system, 2 sector, -1 closed:
  state, the destination list); `menus [row|prefill]` (the menu screens on the pad: state, the list row under the cursor,
  the save description prefill); `osk open [save|chat]|close|type <text>|shift|state` (FLvibed's on-screen keyboard);
  `newgame open|close|pick <n>|mode <n>|records|rescan|state` (the pad's New Game prompt; the per-save mode records are `core\game\session.*`);
  `livesplit state|test [cmd]|connect|reset|start|event <kind> [v]|rows|cp <index>` (the speedrun clock, the split
  detector and the LiveSplit client: a synthetic observation, the detector placed at a checkpoint); `timer
  state|bests|reset-best` (the HUD timer and its best segments).
- **Subtitles debug** (`Send-FLS`): `state`, `config`, `settings` (every setting with its help), `set <key> <v>`, `trace 0|1`, `test`, `say <id> [voice]`, `nn
  <id>`, `chat`, `clear`, `reloaddata`, `controls`, `shown [after]`, `grab` (back-buffer frames at the game's frame
  rate, 400 MB cap), `movieskip`, `layout` (also the top panel, the objective banner, the arrows moved and the
  keep-out rectangles), `perf` (the module's CPU time per frame per entry point since the last `perf`, the frame rate,
  the worst frame), `arrowtrace start|dump` (the current-target arrow per frame, game vs drawn, EXE\arrowtrace.tsv).

## Events the harness observes
Objectives, popups, comms, mission result, death, mode, dock replies (host API v14 DockReply: the server's answer,
3 = queued), autopilot events (target / target-destroyed / given-up, repair, cm, dodge-missile, missile-hit / -missed,
extend / -end, flee, loot, tactic=..., retreat phase=..., missile-run / -launch / -round-left, line-of-fire, under-fire,
avoid detour, big-hit evidence, per-second `self`; session 4, 22 Oct 2026: `control-exclusion a -> b` (the player
ship's lockout: 4 dock start / letterbox, 6 launch, 7 jump, 3 lane), `geometry-guard blocked|clear`, `static-pass`,
`static-orbit`, `pressed=`, `launch-held`, `travel airlock transit`, `fast-dock airlock run`, `objective granted
airlock`; session 12, 24 Oct 2026: `spawn-ship id= arch= pos=` (also kept by the host for the logic: host API v17
SpawnedShips), `pre-aggro text= spawns= ships=` (pre_aggro's flagged mission ships), `lethal-guard`, `fast-dock grant
not followed` / `losing ground` (dock_regain), `launch-held ... beside us` (blast_flank), `objective dock <id>: no way
(locked?)`, `minefield-damage` (shield down or falling only)). Formats: `docs\missions\README.md` "What the harness observes".
Events come from IAT hooks in Content.dll, which the game reloads (e.g. after a death): `events::Update` re-applies them.
