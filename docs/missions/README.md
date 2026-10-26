# Mission runbooks

One runbook per story mission: the mission's script (`DATA\MISSIONS\Mxx\*.ini`) translated into what a test needs.
For each step: how the harness **knows** the mission is there (events, mode, room), what **blocks** the flow
(pausing popups, briefings, offers, scenes), what the player **must do** next and with which harness call, and what
**fails** the mission (timers, nags that escalate, forbidden kills). Tests read the runbook before acting in a mission
and check the state against it at every step (`..\..\CLAUDE.md`, "Mission flow awareness first"). They also guide
building campaign saves per mission.

**Resolved across all runbooks (19 Oct 2026):** `Act_PlayerCanTradelane = false, A, B` closes every lane except the
ride from A toward B (one direction); the bare `false` closes all. Read from the game's own lane check and verified in
M03 at step 5 (`..\FREELANCER_RE.md` "Lane permission"). The runbooks' older "Uncertain" notes on its argument meaning
are answered by this; live query: debug `laneopen <ring> [toward]`. The autopilot's planner skips lanes it denies.

| Mission | Runbook | Script |
|---|---|---|
| M01a Li01 | [M01a.md](M01a.md) | `M01a\m01a.ini` |
| M01b Li01 | [M01b.md](M01b.md) | `M01b\m01b.ini` |
| M02 Li01 | [M02.md](M02.md) | `M02\m02.ini` |
| M03 Li02 | [M03.md](M03.md) | `M03\m03.ini` |
| M04 Li01 / Br04 | [M04.md](M04.md) | `M04\m04.ini` |
| M05 Br03 / Bw01 | [M05.md](M05.md) | `M05\m05.ini` |
| M06 Br05 | [M06.md](M06.md) | `M06\m06.ini` |
| M07 Br04 / Bw08 / Ku03 | [M07.md](M07.md) | `M07\m07.ini` |
| M08 Ku01 / Ku06 | [M08.md](M08.md) | `M08\m08.ini` |
| M09 Ku06 / Ku07 | [M09.md](M09.md) | `M09\m09.ini` |
| M10 Ku06 / Rh | [M10.md](M10.md) | `M10\m10.ini` |
| M11 Iw02 / Li | [M11.md](M11.md) | `M11\m11.ini` |
| M12 St02 | [M12.md](M12.md) | `M12\m12.ini` |
| M13 St01 / Li01 | [M13.md](M13.md) | `M13\m13.ini`, `m13_opensp_*.ini` |

**Two layers.** The script facts (triggers, line numbers, ids, conditions, timers, outcomes) are complete and never
change; the harness layer (the **Harness signal** and **harness call** lines, "harness gap" notes, the summary
table's harness column) follows `controller\tools\flctl.ps1` and the event formats below. **When the harness changes
(a function renamed or added, an event format changed, a gap closed), update these runbooks in the same commit**:
search `docs\missions\` for the old name or `harness gap`.

**Complete.** Every trigger of the script is described or listed as dead / orphan, and every FAIL, save point,
objective and popup / nag / objective string id appears: `tools\runbook-coverage.ps1 -Mission Mxx` lists what is
missing (`tools\runbook-coverage.ps1` alone: a summary of all runbooks).

**Version.** The runbooks describe the scripts of the test install, **HD Edition 0.7.1** (what tests run against).
HDE commits after 0.7.1 fixed some mission scripting (user, 17 Oct 2026), so a dead trigger, a `NagOff` of a
nag that does not exist or a similar script bug noted here may be fixed upstream: compare with the HDE repository's
`DATA\MISSIONS` before reporting one as a game fact, and note "fixed after 0.7.1 in <commit>" where it applies.

## Relevance audit (19 Oct 2026, after the harness and autopilot advances)
Measured over the 14 runbooks (676 KB):
- **Script layer: still accurate and still needed.** 2,219 `trigger (line)` references, 94 % point at the named
  trigger in the installed script (most misses are string ids written like line numbers). The fail tables, nag
  chains, timers, races (M01a TL146) and blocking UI are knowledge nothing else infers: plan B and the stall layer
  read the objective text, not timers. For M03-M13 the runbooks are the only distilled reading of the scripts
  (~20 "Uncertain" notes each, 1-2 "verified").
- **Harness layer: stale.** Every function they name still exists, but the prescriptions predate the autopilot:
  93 `Start-FLGoto` calls (the native goto: slow, adds player waypoints, raw path - now superseded by
  `Start-FLTravel` with no destination = the mission's own markers) and 16 `Start-FLCombat` (no mission reach, may
  flee - superseded by `Invoke-FLMissionFight`); only M01b names the current calls. Scripted missions (M01a, M01b,
  M02) follow their `.ps1`, not the runbook's call lines.
- **Overlap with the knowledge base.** The raw script inventory (spawned objects, marks, invulnerability, kill and
  fail triggers, objectives) is now generated (`tools\kbgen.ps1` -> `docs\kb\generated\msn_*.tsv`); what a pilot must
  understand (set pieces) lives in `docs\kb\missions\`.

**Decision:** keep the runbooks as the mission-flow reference (signals, blockers, fails, timings, races) and the
coverage check; do not hand-maintain inventories the generator produces. When a mission gets its script, rewrite its
harness lines to the current primitives (`Start-FLTravel`, `Invoke-FLMissionFight`, `Start-FLTradeLane`,
`Start-FLJump`, `Start-FLFormation`, `Invoke-FLScanShips`, scene saves), mark steps verified, and write its kb page.
Until then a runbook's harness lines are hints, not instructions.

## Sources
- Scripts: `Install\Freelancer HD Edition\DATA\MISSIONS\Mxx\` (`mxx.ini` = triggers; `npcships.ini` = mission ships;
  `m0xx_s0yy_<base>_<room>_offer.ini` / `_reoffer.ini` / `_nrml.ini` = the base scenes (RTCs) of the mission).
- Text of string ids (objectives, popups, failures, NN messages): `research\strings\resources.tsv` (git-ignored game
  content; `tools\dump-resources.ps1` makes it), columns `<ids>\t<kind>\t<text>`. Quote titles only (a few words); do
  not copy long texts (game content).
- Spoken lines (`DX_Mxx_nnnn_SPEAKER`): `subtitles\data\subtitles.tsv` (id, voice, kind, source, sec, speaker, text);
  `sec` is the line's duration, which turns a `+lines` estimate into seconds.
- Engine behaviour already verified: `..\FREELANCER_RE.md`; the M01a runbook's verified notes (nags escalate to FAIL).

## What the harness observes (controller\tools\flctl.ps1)
- Events (`Get-FLEvents`, `Wait-FLEvent <regex>`; Content.dll hooks of the server API):
  - `objective count=<n> title=<ids> "<text>" text=<ids> "<text>" obj=<ids> "<text>"` =
    `pub::Player::SetMissionObjectives` (the NN objective, `Act_SetNNObj`): `title` / `text` are the mission's (the NN
    log header, e.g. 22000 "MISSION 1"), **`obj` is the step objective** (the runbooks' objective ids; the first
    MissionObjective entry +8, found in game 18 Oct 2026 - before that the event had no step id and a wait on
    `title=<step id>` never matched). Wait with `Wait-FLEvent 'objective.*obj=22010'`.
  - `objective-state id=<n> state=<n>`
  - `popup caption=<ids> "<text>" text=<ids> buttons=0x..` = `Act_PopUpDialog` (pauses the game until closed:
    `MMOkButton`, closed by `Close-FLBlocker`)
  - `mission-message type=FAILURE|SUCCESS|<n> text=<ids>` = `Act_ChangeState` FAIL / SUCCEED (type 0 = failure,
    1 = success: 1231 "Mission Result: Success" - verified in M01b 18 Oct 2026; until then the event wrongly named
    type 1 "dialog" and no SUCCESS was ever reported) and other mission texts by number ("GAME SAVED" 1890, personal
    log entries)
  - `nn-message <ids>` = Neural Net messages
  - `comm from=<space id> to=<space id> lines=<n> first=0x<hash>` = `Act_SendComm` / ether comms (first = hash of the
    first line's id: `tools\flhash.ps1` `Get-FLHash 'DX_M01_0100_KING'` -> 0xBA4E5B0D, verified in game)
  - `mode <a> -> <b>` (menu, docked, space, space-ui, cutscene), `player-death`
- `Get-FLStoryState`: mode, room and its busy reason, open dialogs (MMOkButton, NavMapContinueButton,
  OfferDialog*, LoadSavegameButton, pause), character offers in the room (`talk_story`, `talk_mission`, ...), last
  story event.
- Space: `Get-FLContacts` (objects of the system by name), `Set-FLTarget <name|nickname>`, `Start-FLDock
  <name|nickname>` (a script's solar nickname such as `Li01_02_dock_ring` works: a solar's space id is the hash of its
  lowercase nickname; base nicknames such as `Li01_01_Base` are not space objects),
  `Wait-FLSpace` (waits out story cutscenes), `targetinfo`, `destroytarget` (test aid), `selfdestruct`.
- Flight (the game's own maneuvers, `controller\tools\flctl.ps1`; every approach follows a hazard-free route planned by
  `flroute.ps1` from the system's files and the game's zone test: minefields, danger fields, damage zones):
  - `Start-FLGoto <name|nickname> [-Within m]` / `Start-FLGoto -Position x,y,z [-Within m]` / `Start-FLGoto
    -Objective`: the Goto autopilot (a position becomes the player's waypoint, as the nav map sets it, and the
    mission's navmarkers are put back afterwards; `-Objective` flies to the objective's navmarker as the script set
    it); waits for the arrival (or `-Within`). Ship ids are not nickname hashes (only solars' are): mission ships by
    id from `Get-FLContacts -All`.
  - `Start-FLTradeLane <ring> [-Toward <next ring>]`: dock maneuver on the ring, waits for the ride and its end
    (`-NoWait`: once riding). The direction is the side the ship docks from: `-Toward` approaches from the far side.
  - `Start-FLJump <gate|hole nickname>`: dock maneuver on the gate / hole, waits until flying in the new system.
  - `Start-FLFormation <ship>`, `Stop-FLManeuver` (free flight); `Get-FLPilot` (running maneuver `Op`, lane ride
    `Lane` / `Entry` / `Exit`, `Jumping`, `Locked` = Act_LockManeuvers, formation `Leader`, `System`, `Pos`).
  - Docking also uses the route and the game's dock range (50 km): `Start-FLDock` from anywhere in the system.
  - The waits stop with the game's reason when it refuses (`info_access_denied` at a locked gate, ...), on death and
    on a mission failure.
  - Test aids, never in a mission-flow test: `Set-FLCruiseSpeed <speed>` (boost far from the goal, normal cruise near
    it), `Move-FLShipNear <object>` (pinpoint tests: relocates the ship beside the object). Free-roam fixture with all
    gates open: `SaveC703` (`controller\tools\make-testsaves.ps1`).
- Base: `Goto-FLRoom <room>`, `talk story|mission|...`, `click OfferDialogAcceptButton`, `Start-FLLaunch`,
  `Wait-FLRoomReady`; skips room entries and room scenes unless `-Watch`.
- Combat (the Advanced Autopilot, game-driven, real kills): `Start-FLCombat [-Target <id|name|nickname>] [-Kills n]
  [-TimeoutSec s] [-Settings @{...}]` fights until no hostile ship is in reach (or the chosen target, ship or solar,
  is destroyed; it is kept until then; verified on ships, solar targets not yet), returns Why / Kills / Time / Firing
  / AvgDist; `Start-FLAutopilot <mode>`,
  `Wait-FLAutopilot`, `Stop-FLAutopilot`, `Get-FLAutopilot` for escort / follow / formation / goto / dock / face.
  `Get-FLContacts -Hostile` lists hostile ships; `Attacker` marks ships that target the player. Mission ships the
  script must not lose (escorts, King, Donau) are never picked: the autopilot only picks hostile ships, and a chosen
  target is the test's own decision. `destroytarget` stays a test aid for skipping a fight that is not the subject.
- The game can run out of focus (`deploy.ps1 -Background`, FLController.ini `run_in_background = 1`): every harness
  call works while the user uses other windows.
- Loot: `Get-FLContacts -All` lists loot (`Type loot`, the item's name); `Start-FLCombat` collects the loot within
  3 km after the fight (the game's Loot behaviour, `loot` setting); a single item: `Send-FL 'ai dir loot <id>
  client=1'`. Kills by `destroytarget` drop no loot.
- Mission ships by script name: `Get-FLMissionShips <Mxx>` maps the session's spawn events (`spawn-ship id= arch=
  pos=`) to the mission ini's [MsnShip] / formation ships and their labels by exact spawn position (verified M01a:
  King = `escort`, the Donau formation, the Order `badboys` with `lbl_order`); Id, Nickname, Labels, Formation, Alive.
  This closes "killing groups by label": `Start-FLCombat -Target <id>` per labelled ship.
- Scene saves (start a re-test right before its scene; every mission, docs\missions\SAVES.md): mission scripts take
  `-SceneSaves` (native saves at their `Scene <step> '<label>'` points + the mission's own checkpoints, catalogued in
  `scene_saves.tsv`) and `-From <step>`; `Start-FLScene <mission> <step> [label]` loads one and checks it (objective,
  game build). Events `mission-save ids=` (Act_Save checkpoints); debug `save [ids]` (native save now).
- Dialogue: `Wait-FLDialogue` waits while the radio plays a mission line (the comm player's own playing flag, debug
  `commplaying`; chatter does not count); only actions that end the space scene call it (docks, jumps, docking
  travels) - never the objective waits: the player acts while the radio talks.
- Regression sweep: `controller\tools\flsweep.ps1 [-Only <sections>]` (base, navigation, pad, combat, hold, subtitles,
  M01a, M01b, M02 scenes, release; PASS / FAIL into research\experience\runs). Screenshot sampler
  `tools\flshots.ps1`: every `deploy.ps1 -Background` start runs one per game session (a JPEG every 6 s of the whole
  test + a 5 fps video in one-minute segments `video_*.mkv` for the dense evidence around a moment, into
  research\experience\shots\<time>; the newest 15 sessions kept, none older than 3 days). Check-ins every
  ~2 min: `tools\flsheet.ps1` tiles every frame since the previous check-in and the window before it into one sheet,
  each tile captioned with the session log's key event and the event timeline printed beside it (the live log while
  the session runs; a session's frames and archived logs share its start stamp) - frames and log cross-checked;
  unexpected behaviour = stop, read EXE\logs (previous sessions kept), fix.
- Waypoints: never a second, player waypoint on top of the mission's (the native HUD shows the mission's alone). Prefer
  `Start-FLTravel` with no destination (the mission's list); `Start-FLGoto -Position` at a point the mission's list
  already marks (first entry within 1.5 km) flies to that waypoint itself (`-Objective`), so runbook calls stay valid.
- Resuming at a scene (`Start-FLScene` + `-From`): the script reads the session's history from the load (the scene's
  objective arrives before the script starts).
- Scanning: `Invoke-FLScanShips` (hold at a point, scan every ship; marked ships first).

## Script semantics across missions (to verify in game; update here, then the runbooks)
- **`Act_PlayerCanDock = false, <objects>` / `Act_PlayerCanTradelane = false, <rings>`** most likely mean "off
  everywhere **except** the listed objects": every listed object is one the mission needs right then (M08 line 506
  lists `Ku01_to_Ku04_hole` just before the player must take it; M01a lists the Pittsburgh dock ring just before
  docking there). The form without a list turns it off everywhere. Inference from the scripts, not yet tested.
- **Nags escalate to FAIL** with the nag's text (verified in M01a); the scripts give no nag timing.
- **`Act_PobjIdle` stops the player's current maneuver / autopilot and the ship (full stop in ~1 s, then free
  flight; verified 17 Oct 2026 with the Idle directive)**; it does not freeze the player, which is why no script ever
  "un-idles" (HDE PR #270 uses it as a "full stop"). Runbooks that call it a freeze mean this.
  Observation (17 Oct 2026, not yet seen in a mission): the player's maneuvers are AI directives on the ship's
  behaviour manager, and `pub::AI::OpType` has an **Idle** op (6) next to Goto, Dock, Formation and FreeFlight, so
  an "idle" for the player most likely replaces the running directive (goto, dock, formation) with Idle: the ship
  stops maneuvering, and the player's next maneuver or input replaces it again. `Get-FLPilot` shows it as `Op = idle`
  (and `Act_LockManeuvers` as `Locked`), so a test can check this the first time a mission idles the player.
- Changes HDE made to the scripts after 0.7.1 are listed at the end of each affected runbook (M06, M08, M10, M11,
  M12, M13) and in `..\HDE_UPSTREAM.md`.
- **Solar space ids** = hash of the lowercase nickname (verified 17 Oct 2026), so scripts' object nicknames work
  with `Set-FLTarget` / `Start-FLDock`.

## Runbook template
Keep the M01a conventions (line numbers **1-based** as an editor shows them, `->` = Act_ActTrig, real seconds for
lines, uncertainties marked **Uncertain**). Plain English, short sentences. Sections:

1. Title, one-line summary, source files, **Start**: where the mission begins (base, room, the character that offers
   it, how the previous mission hands over), **End**: what SUCCEED needs and where the player is afterwards.
2. **Outcomes** table: FAIL / SUCCEED, ids, the text (title only), trigger (line), meaning.
3. **Save points** table: `Act_Save` name / ids, where written, the resume trigger, where it resumes.
4. **Steps**, each with:
   - **Starts**: trigger (line) and condition.
   - **Harness signal**: the event(s) that prove this step started (e.g. `objective ... title=21830 "Destroy the
     Hostile Ships"`, `popup caption=21895`, a comm line, mode change), plus what `Get-FLStoryState` shows.
   - **Blocking UI / scenes**: popups (pause the game), briefing (nav map), offers, room scenes, space cutscenes
     (`Act_StartCutscene` / camera actions: skippable in a base, not in space), player freezes (`Act_PobjIdle`),
     scripted relocations; say which `Close-FLBlocker` handles and which the test must decide.
   - **Required action** and the **harness call** for it (or "harness gap: <what is missing>"). A "launch" in a
     runbook is the mission's requirement; the scripts (`controller\tools\missions\M03objective.ps1` / `M04objective` /
     `M05objective`, since 20 Oct 2026) make the player's base stop first (`Complete-FLBaseStory`: repair + restock).
   - **Fail paths**: chains with their total times (timers + line durations in seconds), nags (they escalate: check
     whether a nag text equals a FAIL text), forbidden kills / lanes / docks.
   - **Safe window**: how long and where free testing is safe.
5. **Dead or orphan triggers**, **Uncertainties**, **Summary table** (step, objective ids + title, harness signal,
   required action, fail timers, safe for free testing).
