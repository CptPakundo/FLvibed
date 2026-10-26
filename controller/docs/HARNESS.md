# The test harness: flctl.ps1, flsub.ps1, debug channels

Reference of the harness functions and debug commands. Moved here from `..\..\CLAUDE.md` and `..\CLAUDE.md` on
21 Oct 2026 (the audit's CLAUDE.md diet). The rules for using them are in `..\..\docs\TESTING.md`. The functions
themselves: `..\tools\flctl.ps1` (104 functions), `..\..\subtitles\tools\flsub.ps1`; debug commands:
`..\src\impl\debug.cpp`, the shared ones in `..\..\core\game\room.cpp`.

**Load `.ps1` files only in the PowerShell tool, never from Bash** (each call is a new process; dot-source the harness
in every call). `$env:FLVIBED_INSTANCE = '<n>'` points a PowerShell session's harness at parallel game instance n.

**Parts (since 21 Oct 2026):** `flctl.ps1` holds the shared variables and dot-sources, in order, `flctl.core.ps1` (debug
channel, pad, frames, log, deadlines, waits), `flctl.rooms.ps1` (story state, blockers, rooms, launch, base repair /
restock, base stories, space waits), `flctl.space.ps1` (contacts, targets, docks, the game's maneuvers, cruise and
teleport aids), `flctl.missions.ps1` (event log, objectives, mission fights, dialogue, plan B, stalls, jobs),
`flctl.travel.ps1` (travel, jumps, joining, formation, stop), `flctl.ui.ps1` (Goto-FLRoom, Select-FLItem, events),
`flctl.saves.ps1` (scene saves, Load-FLSave, frames, peeks, console, godmode), `flctl.autopilot.ps1` (combat test setup,
mission ships, the autopilot, focus). The parts share one scope; never load a part alone. New functions go into the
part of their topic; `deploy.ps1` parse-checks every `*.ps1` in the tool folders.

## controller\tools\flctl.ps1
- Core: `Send-FL <debug cmd>`, `Wait-FLUntil` (poll a state condition; every timeout prints `Get-FLStoryState`),
  `Get-FLMode` / `Get-FLFrame`, `Get-FLLog`, `Get-FLEvents` / `Wait-FLEvent` (game events), `Set-FLDeadline` (live
  watchdog: every wait throws OVER BUDGET), `Focus-FLGame` (a no-op in background mode).
- Virtual pad: `Set-FLPad` / `Press-FLPad` / `Clear-FLPad`. Never use screen coordinates in tests: named controls,
  D-pad navigation, values from `state`.
- Rooms and story: `Load-FLSave <file>` (native, by file name, from any state; refuses a stale slot - saves are read at
  startup, `deploy.ps1 -Save` prepares one), `Goto-FLRoom <label> [-NoWait]` (room buttons by function, waits on game
  state), `Start-FLLaunch`, `Skip-FLAnimation`, `Wait-FLRoomReady`, `Close-FLBlocker` (story popups, mission briefing),
  `Complete-FLBaseStory` (a base's story talks and room scenes played out, then the player's base stop -
  `Invoke-FLBaseRepair` (the game's repair window) and `Invoke-FLBaseRestock` (nanobots, batteries and the ammo of the
  mounted launchers - debug `equipment` + the launchers' projectile_archetype - at the equipment dealer, any base:
  filters searched, lists scrolled, the game's own buy; keeps the repair just paid in cash; purchases are net-worth
  neutral) - then the launch: no per-mission room lists), `Get-FLStoryState` (mode, room, open dialogs, offers, last
  story event), `Find-FLTalker`, `Select-FLItem <control name>`, `Wait-FLSpace` (waits out story cutscenes in space),
  `Send-FLConsole <cmd>`.
- Space: `Get-FLContacts` (objects of the system, any HUD filter; `-Hostile`: Attitude, TargetId, Attacker),
  `Set-FLTarget <name|id>`, `Start-FLDock [name]` (target, dock request, the base's reply, docking cutscene skipped),
  the game's own maneuvers `Start-FLGoto <object> | -Position x,y,z | -Objective`, `Start-FLTradeLane <ring> [-Toward
  <ring>]` (enters by the autopilot's fast entry; `-Native`: the game's dock maneuver), `Start-FLJump <gate|hole>`,
  `Start-FLFormation <ship id>`, `Join-FLShip` (formation where the game allows it, else the autopilot's follow),
  `Stop-FLManeuver`, `Get-FLPilot` (autopilot state); every approach follows a hazard-free route
  (`..\tools\flroute.ps1`: minefields, danger fields, damage zones from the system files, the game's zone test).
- Autopilot: `Start-FLTravel <object> | -Position x,y,z [-System s [-Object o]]` (the autopilot's fastest route: lanes,
  gates, fast docks, obstacle avoidance; a travel with no destination = the mission's own markers), `Start-FLAutopilot`
  / `Wait-FLAutopilot` / `Stop-FLAutopilot` / `Get-FLAutopilot`, `Start-FLCombat` (real kills), `Start-FLMissionCombat`
  / `Invoke-FLMissionFight` (reach to the nearest hostile, no fleeing in mission battles), `Start-FLCombatTest`,
  `Update-FLIdlePark` (the harness parks the ship with the autopilot's hold mode while it waits), `Resolve-FLObjectiveTarget`.
- Speedrun legs (22 Oct 2026, `..\..\docs\LEGS.md`): `missions\Leg.ps1` (one leg of any mission: base story by
  `Complete-FLBaseStory -AcceptStory`, space by the objective mode; -BudgetSec stops an off-pace leg; -StallSec 60, the
  stall watchdog: no movement of 300 m and no change of the autopilot's kills / damage dealt / target / action for that
  long ends the leg STALL - user, 23 Oct 2026: stuck tests are a time waster; segment practice `-SaveAt <cps>` /
  `-StartCp <cp> [-StartSave <file>]` / `-StopAt <cp>` with the save-fit check: the ship's state at a save in its
  `scene_saves.tsv` row, the first run from it writes `fit` / `fit_note` and logs `SAVE-FIT` - `..\..\docs\TESTING.md`
  "The test loop" 4), `..\tools\flleg.ps1 [-ApSet 'k v','k v']`
  (autopilot switches sent after the boot - a set's A/B in one build; `..\..\tools\flset.ps1` runs one set side) `..\tools\flleg.ps1
  -Instance n -Save s -Label l` (a leg on its own instance with evidence, history `research\experience\legs\legs.tsv`,
  a leg-end save catalogued as scene `<label> end`; a leg that does not pass keeps `end_tail.mkv` + `end_sheet.jpg`,
  the last 15 s of the recording; the motion trace `FLMotion.tsv` is kept with every leg; `-Save` by name looks in
  `research\saves\bc46-all-missions\`, then the leg checkpoints `research\saves\legs\`). `Complete-FLBaseStory` also walks to the room the objective names
  (`Get-FLObjectiveRoom`) and tours the rooms while launching is not allowed (`Step-FLRoomTour`). Commodities:
  `Invoke-FLTraderBuy @{ commodity_x = n }` (0 = what fits). Market data: `..\..\tools\flmarket.js`.
- **One client at a time per game channel:** `Send-FL` / `Send-FLS` hold a named mutex per channel (22 Oct 2026: a leg
  runner and a check-in on one game overwrote each other's command slot - lost / crossed answers).
- Missions: plan B `Update-FLPlanB` (the next action inferred from the objective text + spoken lines, run alongside
  every mission wait; `Get-FLMissionHint`), stalls `Update-FLStall` (`research\experience\stalls.jsonl`), jobs
  `Get-FLJobOffers` / `Start-FLJob` / `Invoke-FLJob` / `Get-FLJobBases` (expected pay; offers, objectives' targets and
  navmarkers are host events; `research\experience\jobs.jsonl`), `Invoke-FLScanShips`; **reading events for a test step**
  (26 Oct 2026, the parity follow-up): mark the host's sequence first (`events 999999999` -> `last N`) and read
  `Get-FLEventLog N` after `Start-FLEventLog` ran once in that script (a first `Get-FLEventLog` without it starts the
  log at "now" and loses what came between the mark and the call); never judge a step by `Get-FLHistory` (the host's
  whole ring: t3 reported a 21 s "why=player input" stop in a background job that was a manual K test's stop from two
  minutes earlier). A start pressed in the launch's tail is refused (`autopilot start refused reason=key (no ship or a
  cutscene)`): wait for the answer event and press again (`research\experience\runs\autopilot_parity_2026-10-26\tools\t5.ps1` jobs, `t6.ps1` parity); mission scripts
  `..\tools\missions\` (Campaign.ps1: New Game to M02; M01a, M01b, M02; M03objective / M04objective / M05objective:
  rooms by the harness, space by the objective mode).
- Test aids: `godmode on|off` (debug), `Set-FLCruiseSpeed`, `Move-FLShipNear` (pinpoint tests only; size-aware), `stall <ms>`
  (debug: the game thread spins, at most 60 s - a frozen game for the freeze watchdog `..\..\tools\flfreeze.ps1`).
- Benchmarks: `..\tools\flbench.ps1` (Enter-FLStream, Invoke-FLBenchRun, Invoke-FLABRun / Invoke-FLABBench,
  Enter-FLScenario <name>, Invoke-FLBenchRun -UntilSpawnsDead, Invoke-FLScenarioAB; results in `research\bench`),
  `..\tools\flfights.ps1` (fight matrix), `..\tools\flsweep.ps1` (regression sweep), `..\..\tools\flbench.ps1`
  (timing history across sweeps), `..\..\tools\fltest.ps1` (one test start with run folder + manifest).

## subtitles\tools\flsub.ps1
`Send-FLS`, `Watch-FLGame` (`-LinePattern`, `-ShotMs`), `Wait-FLSShown` + `Get-FLSLast` (for `-After`; works during
movies and across restarts), `Close-FLModal` (native clicks), `Set-FLSIntro` (switch test intros via freelancer.ini,
restorable), `Save-FLShot` (game window only), `Get-FLSLog`.

## Debug channels and logs
Each module has its own debug channel (shared memory `Local\FLControllerShm[_n]` / `Local\FLSubtitlesShm[_n]`) and log
(`EXE\FLController.log`, `EXE\FLSubtitles.log`; `EXE\logs` keeps previous sessions). Freelancer reads the mouse only
while it is the foreground window; pad commands and every debug command work in the background.

- **Shared commands in both channels** (`core\game\`): `click <Control>` (text, icon and front-end buttons; the reply
  adds `(nn-window busy ...)` when a docked NeuroNet window was sliding - the game drops a close then, retry after the
  slide; `hotspots` / `goto` say `busy: nn-window` meanwhile), `visible
  <Control>`, `hotspots`, `goto <label>`, `talkers`, `talk <n|offer>`, `menu <new|load|options|exit>`, `pause`,
  `pauseload`, `saves`, `loadsave <file>`, `pendingload` (the CHARACTER_ID the in-game load / death dialog last loaded),
  `ctlinfo <Control>` (addresses for RE), `selfdestruct` (test aid: death dialog). `equipment` / `equipall` also print
  `mines=` (the mines counted by ammo, as the countermeasures).
- **Controller host** (`src\host\main.cpp`): `uigen` - the UI generation counter of HostApi v23 ("uigen N adds A removes
  R": the game's control list adds / removes; Extended HUD 2 finds windows by it; still in a calm room or calm flight).
- **Controller debug** (`src\impl\debug.cpp`): `state`, `ship`, `uiitems`, `set <name> <v>` (live only; `config save`
  writes `FLController.ini`), `config` (`config settings`: every [Controller] and [Autopilot] setting with its value and
  help, from the registry `core\settings.h`; `ap settings` the autopilot's alone), `bot`, `events`, `peek` / `poke`, `snap` / `diff`, `findvt <value> [max]`
  (objects by vtable: a scan of committed private memory), `watch <addr> [len] [w|rw] | report | stop` (a hardware data
  breakpoint on the game thread: which instructions write / read it, with two callers; removed before a hot reload),
  `tap`, `mouse`, `targetinfo`, `destroytarget`,
  `contacts`, `settarget`, `perf` (flight model), `perf cpu` (the logic's CPU time per frame per entry point -
  OnFrame, OnRenderOverlay, OnRenderUnderHud, space::Contacts and its fresh scans - since the last `perf cpu`, the frame
  rate, the worst frame; resets; `core\log.h` perf::Scope), `rec start [file] | stop` (flight recorder: CSV per frame beside the
  exe), `bestpath` / `routes` (the game's best path into the waypoint list), `route [ring ...]` (the autopilot's plan
  from here along that list, the rings given closed as a disrupted ride closes them), `equipment` / `equipall` (the
  server's equipment list; every item with its active flag and ammo), `missiles`, `rounds`, `obstacles`, `laneopen <ring>
  [toward]`, `objobj <ref>`, `worth`, `eta`, `drainshield <id>` / `shieldof <id>`, `shot <cmd> <px> <py>`, `uihide`,
  `cue ...`, `hitlook ...`, `nnsev`, `spawn <npcshiparch> <faction|player> [count] [dist] [job= pilot= order= base=
  force= level=] | list | hunt | clear | clearat <hull>`, `ai state|man|dir|pilots|pilot|copy|field|fields|set|brain|
  cbrain|release|target|free|layout|rawdiff|self` (`ai self`: hull / shield fractions and maxima, power, gun range,
  nanobots and batteries n/max, tractor range, hold - `core\game\ai.cpp`), `ap start <mode> [id|x y z] | stop | status | pilot | set | clear | field
  | save | load | preset standard|no-consumables | lof <id> | goals | ray | scan | rayraw` (`ap goals`: the script
  conditions that advance the current step, live - text, system, title, from-last and each goal's kind, distance /
  count, whether the game places its object; `ap ray x0 y0 z0 x1 y1 z1`: the ray test's first hit; `ap scan x0 y0 z0 x1
  y1 z1 half step`: a grid of parallel rays as a map ('.' clear, digits = the hit's tenth of the length); `ap rayraw
  ...`: every raw hit with object class, id, crc - a solar's moving part is class 0x1; session 6).
- **The UI build's commands** (25 Oct 2026; each module's `Command()` in `src\impl`, the exact syntax from its usage
  line): `ui options [open|close|cancel] | tab <n|name> | rows | press <up|down|left|right|a|b|lb|rb|pgup|pgdn ...> |
  focus <key> | default | state` (the FLVIBED options page: open standalone, switch tabs, the rows with their values,
  queued presses for the next frame, the focus onto a row, this tab's defaults, the hosting state); `uibtn` (host-side:
  the native buttons' state, controls and placement, the raw-key counters), `uibtn click <1 bar|2 options page|3 pause>`
  (a click through the game's own release path), `uibtn mesh [<path relative to DATA>]` (the bar button's mesh for the
  next HUD build); `device` (the input-device service: pad / keyboard-and-mouse and why); `fluf` (FLUF awareness,
  `..\..\docs\MOD_COMPATIBILITY.md` FLUF: present 0 / 1 co-residency / 2 aware, the build, the status line, `fluf_windows`,
  text input, the cursor over a FLUF window, whether the pad drives FLUF, `palette` / `holdsKeys` (FLvibed's keys held),
  every open keep-out with its kind and rectangle and (bus v5) its ImGui name, input rule, focus and close-button point);
  `d3dreset` (host-side, background test starts only: the device Reset in place with its last parameters through the
  vtable, every module's Reset hook running; replies TestCooperativeLevel before / after and Reset's result);
  `winmode` (host-side: borderless windowed - setting, provider, the game's windowed style, DPI awareness, setups /
  rewrites / toggles, the window / client / monitor rectangles, render size); `winmode altenter` (host-side, background
  test starts only: one display setup as FLUF's Alt+Enter makes it, refused unless FLvibed's rewrite applies - never
  exclusive fullscreen);
  `hooks` (host-side, both modules: every detour's mode, place and where the function's own jump leads); `padhud on|off|state` (the pad
  HUD layer: badges, legend, status line; the Classic HUD); `hud2 on|off|auto|state|sim <combat|travel|cruise|dock|lane|off>|tree
  <root>|hit <x> <y>` (HUD 2, `..\..\docs\UI_HUD2.md`: forced on / off or back to `hud_style`, its state and the published
  layout (the nav line also `event` = the dock / lane / jump answer, `comm` = the subtitles' dialogue panel shows a line,
  `cine` = the letterbox, `fight` = the threat list's rule; HUD 2 FIX2 25 Oct 2026), a simulated state, a native HUD root's control tree, the game's hit test at a pixel; `hud2 viewback on|off`
  the dark backing under the game's 3D views with the target panel's pulses and the view's object, `hud2 viewzoom <f>`
  the views' rectangles over their windows, 1.5 by default; `hud2 tree roots` every top-level control of the game's
control list with its vtable and state, `hud2 tree <control name>` any named control's tree); `hudinfo [wg|engine|lock|
  tractor|obj <id>|sys [id]|equip <arch>]` (HUD 2's readers, `core\game\hudinfo.h`, and their research dumps); `picker open|close|a|b|up|down|next|prev|cat <name>|state` (the target
  picker); `awheel open|pick <n>|commit|cancel|rear|state` (the action wheel; `wheel` is the mouse wheel); `goals
  open|close|toggle|tab <0-2>|up|down|a|x|y|left|right|rebuild|click <slot>|state` (the autopilot goal panel; `state`
  says form=pad|kbm, clickbox= and the keyboard + mouse form's hotspots; `click` = a click of its click box as the host
  routes it: 0-2 tabs, 3 the ESC mark, 4 / 5 scroll, 6.. the visible rows); `postkey <vk> <0|1>` (one real
  WM_KEYDOWN / WM_KEYUP to the game window - a Shift held across a `post <vk>`: the keyboard + mouse tests' Shift+K);
  `ap start smart` (the plain start of every device: objective, else waypoints, else the target); `bases [rooms|chars]`
  (the pad in bases: the screen, the room ring, the bar's characters, the prompt bar's state); `map [dest]` (the nav map:
  state, the destination list); `menus [row|prefill]` (the menu screens on the pad: state, the list row under the cursor,
  the save description prefill); `osk open [save|chat]|close|type <text>|shift|state` (FLvibed's on-screen keyboard);
  `newgame open|close|pick <n>|mode <n>|records|rescan|state` (the New Game prompt and the per-save mode records);
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
(locked?)`, `minefield-damage` (shield down or falling only)). Formats: `..\..\docs\missions\README.md` "What the harness observes".
Events come from IAT hooks in Content.dll, which the game reloads (e.g. after a death): `events::Update` re-applies them.
