# Testing FLvibed: build, deploy, test install, rules

The testing rules the user set, and how the test install, deploy and sweeps work. Moved here from `..\CLAUDE.md` on
21 Oct 2026 (the audit's CLAUDE.md diet); the rules are unchanged. The harness functions themselves are described in
`..\controller\docs\HARNESS.md`; the mission runbooks in `missions\`; the game knowledge base in `kb\`.

## Build and deploy
- `build.cmd dev` (default): `build\dev\FLvibed.dll` (both modules' hosts) + `FLSubtitlesImpl.dll` +
  `FLControllerImpl.dll` (the logic, hot-reloadable). `build.cmd release`: `build\release\FLvibed.dll` alone.
  `build.cmd impl`: the logic DLLs only. `build.cmd sigtool`: `build\sigtool\sigtool.exe` (signatures for `core\flexe.h`). `build.cmd test`: the host-free unit
  tests (`tests\`, seconds, no game; `tools\README.md`).
- `tools\deploy.ps1` (PowerShell tool, full path): builds, writes `dacom.ini` (FLvibed.dll) and `FLvibed.ini`,
  copies `subtitles\data\*.tsv` to `EXE\FLSubtitles\`, then hot-reloads (game running, DLL unchanged) or starts the
  game, skipping each startup movie natively (`movieskip`: the game's movie loop sees it finished) while its video
  window exists. Switches: `-Impl` (logic only, hot reload), `-Release` (the player build), `-Restart`, `-NoStart`,
  `-NoBuild`, `-Intro` (let movies play), `-Save <file>` (start, then load that save by name), `-Modules
  subtitles,controller` (which modules run), **`-Background`** (start without taking focus, game runs out of focus: use
  it for automated tests so the user keeps the screen; every harness call works out of focus; it also starts the
  screenshot sampler `tools\flshots.ps1` for the session - a standing order: look at `research\experience\shots\<time>`
  while waiting), `-Instance <n>` (parallel test games, n >= 1: `Install\instances\<n>`, a hard-linked mirror of the
  test install made on first use, with its own saves, logs, screenshots `shots\<time>_i<n>` and debug channels
  `Local\FLControllerShm_<n>` / `Local\FLSubtitlesShm_<n>`; `$env:FLVIBED_INSTANCE = '<n>'` points the harness of a
  PowerShell session at that game; `tools\fldata.ps1` Get-FLInstallDir / Get-FLGameProcess / New-FLInstance).
  `deploy.ps1` refuses to start the game when it would write to the shared `Documents\My Games\Freelancer`
  (`Assert-FLDataSeparate`) and warns when the test evidence under `research\` passes 20 GB (the user decides about
  pruning; nothing is pruned by itself - memory keep-test-evidence).
- **Sweeps:** `controller\tools\flsweep.ps1 -Parallel <N>` builds once, splits the sections over N games by their median
  times and runs them at once (measured 20 Oct 2026: the full sweep 2688 s on 3 games vs ~7900 s serial, every mission
  SUCCESS; 1 game 191 fps, 3 games 154-163 fps each). Under load the harness polls slower, so a test must never race
  the game: a threshold the test depends on is enforced game-side (base-repair: `spawn clearat <hull>` despawns the
  attackers - and their missiles in flight, counted as damage already done - the frame the hull reaches it; harness
  polling missed it and the ship died). A sweep ends after the game has exited (a dying game counted as running for
  deploy: exit 1 after PASS runs). Never build during a sweep. **Reused games** (the sweep loads the next fixture
  natively instead of restarting, ~30-40 s saved per section; user, 20 Oct 2026): fresh for the first section, after
  any failure, for fresh-only sections (missions, story, fight matrix, release) and after 5 sections / 20 min; a failure
  in a reused game is re-run fresh and a fresh pass is reported STATE-LEAK (a bug of its own); `-Fresh` for full sweeps;
  results record fresh vs reused. Results: `research\experience\runs\sweep_<date>.txt`;
  timing history `tools\flbench.ps1`; one test start with evidence `tools\fltest.ps1`; check-ins `tools\flcheckin.ps1`
  (text digest per instance every ~2 min, ATTENTION on death / stall / repairs / a frozen game - no frame in 3 s, with
  per-thread CPU, the busy threads' code locations and our logs saved by `tools\flfreeze.ps1`; memory screenshot-sampling).
- **Speedrun legs** (22 Oct 2026, `LEGS.md`): `controller\tools\flleg.ps1` runs one leg per game instance from a save,
  several at once (one pwsh each, build once before); each instance has its own copies of the FL*.dll (not hard links),
  so a new build can be deployed to an idle instance while others run. Prefer a leg (minutes, from the nearest leg
  save) over a long chain; a chain run only for what crosses leg boundaries. A failed leg keeps the last 15 s of its
  recording (`end_tail.mkv`, `end_sheet.jpg`: every 5 fps frame, time-stamped) beside its logs. A leg whose log has
  `autopilot minefield-damage` (hull lost to a mine in a mine-danger field; not a zone's steady damage) is a FAIL whatever the mission says (the
  user, 23 Oct 2026, for Zone 21; the rule holds for every minefield). **Fail states** (the user, 23 Oct 2026: fail a
  run as soon as it cannot be a good one - shorter test cycles): the checkpoint limits ("Speedrun splits" below) plus
  the event rules of `controller\tools\missions\leg_failstates.tsv`, rows per leg (`M12`) or for all (`*`): `event` = a
  log event matching the pattern ends the leg `FAILED (FAIL STATE ...)` at once; `by` = the pattern must appear within
  `secs` of the anchor's first event (no anchor: of the leg start). `Leg.ps1` checks them live; `-NoFailStates` turns
  them off. Each row says why (its data). (The first per-leg `by` rows - 1.3x the leg's best, M12's lair / core
  deadlines - were replaced on 24 Oct 2026 by the data-derived checkpoint limits.)
- **Evidence pruning** (the user, 24 Oct 2026): `tools\prune_evidence.ps1` (dry run by default, `-Apply`; `-All` also
  failed legs and sessions tied to no leg) deletes session videos (`.mkv`) only - screenshots, logs, motion traces, leg
  folders and failure end clips stay - and **always keeps the full data of the 3 fastest passing runs of each leg**
  (mission + start save). First run 24 Oct: 28.7 GB of video from 775 sessions, evidence 40 -> ~15 GB.
- **Display matrix** (sweep section `display-matrix`, 21 Oct 2026): one game instance (the worker's, or instance 9 in a
  serial sweep; never the main install) is started fresh per configuration - borderless 3840x2160, bordered windowed
  1600x1200 (4:3) and 2560x1080 (21:9), borderless 1280x720, a DPI-unaware process - and both modules are checked (back
  buffer size, window border, DPI awareness, HUD projection, pad throttle, HUD 2 coming on with the pad, a subtitle, a
  screenshot per mode under `research\experience\runs\display_<time>`). **HUD 2 in tests** (25 Oct 2026,
  `docs\UI_HUD2.md`): with `hud_style` 1 (default) HUD 2 replaces the game's panels while the pad is the device - the
  virtual pad included - so a test that reads or clicks the game's own HUD controls (`uiitems`, `visible`, `click` on a
  maneuver / contact / status button) runs before any pad input, with `input_mode 2`, or with `hud_style 0`; the autopilot
  legs and sets drive no pad and see the game's HUD. HUD 2's own state: debug `hud2 state`, `hudinfo`. Exclusive fullscreen only with `-Attended` (it takes the screen). How the
  modes are switched: `docs\kb\display.md`, `tools\fldata.ps1 Set-FLInstanceDisplay`. The `display-subtitles` section
  judges real dialogue at 4K and 720p borderless, 4:3 and 21:9 windowed (Juni's room scene, the M01a opening cutscene,
  King's flight comms) by the subtitles module's `layout` debug report against the design rules - in flight the top
  panel: the maneuver bar moved; at 16:9 / 21:9 the plain panel inside the room between the bar and the NeuroNet box;
  at 4:3 the name-tab layout (the tab in that room, the body below the bar's row, clear of the cruise box, on screen);
  above 0.44 H, the objective banner's text below it (~11 min).
- **Verify a change in game, in the release build too** before calling a stage done (`deploy.ps1 -Release`; the sweep's
  `release-build` section).

## Speedrun splits: checkpoints, par, fail limits, the test loop (the user, 24 Oct 2026)
The user's overhaul of the testing methodology: "a set list of checkpoints throughout the game's story ... right before
or right after a notable event ... where player interactivity is involved"; every section between checkpoints is a split
to beat; "meet or beat the fastest times for each story mission in its whole, and ... not fall significantly behind at
each separate checkpoint"; a catastrophic mistake or delay stops the test as a failure, judged "by both analyzing prior
test data and also by analyzing what the speedrunners did in that segment"; the speedrun data and the test data kept
current, tidy and regression-free; the research data referred to at every step. The model is LiveSplit's (splits,
personal best, best segments / gold, sum of best, comparisons).
- **The checkpoints**: `docs\checkpoints.tsv` (hand-curated; 193 over the 14 mission legs, made from the three recordings'
  banners / events / curated section times and our logs' objective ids). Between missions a free-time leg is one
  segment (mission end -> the next accept; not built yet). Each row: how our log shows it (`ours`: `obj:<id>` the
  objective first shown after the previous checkpoint, or up to 5 s before it; `control` the HUD back in space after a
  launch / its space scene; `land`, `launch`, `cutscene`, `success`; `kill:<n>` the n-th destroyed marked target), how the
  recordings show it (`runner`: a banner, an event, `hud`, `end`; hand-read `r1..r3` win, `src` says from where), the room
  scenes the runners watched inside the segment (`scene`: HDE's Esc skips them, so they come out of the runners' times)
  and what happens in the segment. Add or move a checkpoint only with both sides' data; then `tools\splits.py build`.
- **Par** (`docs\splits_reference.tsv`, generated by `splits.py build`): per leg the golden run = the fastest of the three
  recordings scene-free (the user: "the golden benchmark would be the fastest speedrun"); a checkpoint its footage does
  not show is placed by the other runs' share of the segment. The **mission target**: the whole leg at or under par.
  **Pace** per checkpoint: AHEAD (at or under par), ok, BEHIND = over par by more than max(10 s, 5 %).
- **Our splits** come from every leg's `FLController.log` (+ the transcript's leg start): `splits.py history` ->
  `research\experience\legs\splits.tsv` (derived, regenerable); **PB** = the fastest full PASS leg's splits, **gold** = the
  best time of each segment in any run (a part run's first segment is never timed: the load re-sends its objective).
  `docs\SPLITS.md` (generated, committed with the leg results) = par, runners, gold, PB, spread, limits per checkpoint,
  the sum of best per leg; `docs\livesplit\<leg>.lss` = LiveSplit files with those comparisons (open them in LiveSplit to
  see a leg as the runners do; nothing of ours depends on LiveSplit).
- **Fail limits - a catastrophic mistake or delay** (`controller\tools\missions\leg_checkpoints.tsv`, generated by
  `splits.py limits`): a segment fails when it takes longer than max(2x the runners' slowest time for it, +45 s;
  1.5x our gold + 20 s) - far outside both the runners' worst and our own best; a full leg also fails when a checkpoint
  is not reached by 1.15x the PB's split + 30 s. The own-history bounds tighten by themselves as golds and PBs improve.
  `kill:` checkpoints are soft (our log names only the autopilot's own target's kills): unseen at their limit, it carries
  over to the next. `Leg.ps1` checks the limits live (SPLIT lines, `cp` rows in `phases.tsv`, `FAILED (FAIL STATE
  (CHECKPOINT ...))`); `Leg.ps1 -Label <leg> -Full -ReplayLog <leg folder>` runs the same code over an archived log.
  **Validation** (re-run after any change to the limit constants, the checkpoints or the matching): `splits.py replay`
  must show `GOOD RUNS FAILED: 0` (good = a full PASS within 10 % of the PB; a good run older than the gold that now
  bounds the segment is "superseded"), and the live code must stop the same runs at the same checkpoints (24 Oct 2026:
  262 archived full legs, all agree; the replay stops 29 failed runs early, ~2 h of test time).
- **After every leg** `flleg.ps1` writes `splits.txt` (this run against par / PB / gold) beside the evidence and runs
  `splits.py update` (history, limits, SPLITS.md, LiveSplit files): the next leg runs against the new bests.
- **Where the evidence lives** (tidied 24 Oct 2026): `research\experience\legs\` = one folder per leg run
  (`<time>_<label>_i<n>`: transcript, phases.tsv, splits.txt, the game's logs, end clip on failure) + `legs.tsv` (the
  history, the source of truth), `splits.tsv` (derived), `race.tsv`; `legs\sets\` = the gate / set drivers' tables and
  logs (`gate_<tag>.tsv`, detached-pwsh logs: write new ones there); `research\experience\misc\` = loose probe logs of
  18-23 Oct; `stalls.jsonl` / `jobs.jsonl` stay at the top (the harness appends to them); videos by
  `tools\prune_evidence.ps1`. Name every set's runs `<leg>-<set>-<variant>` so `splits.py compare` can select them.

### The test loop (replaces "judge a change by three runs")
1. **Target**: the segment with the largest gap that the runners' data explains - `docs\SPLITS.md` (PB - par, gold - par),
   then `research\evidence\speedrun\reference\ref.py <leg> <section>` (all three runs side by side, frames on demand), the
   leg's dossier entry (`docs\LEG_DOSSIER.md`) and our runs of that segment (`splits.tsv`, the leg folders). Never a
   segment the runners' data does not cover without saying so.
2. **Probe before a fix** (session 9 lesson: the M12 plate read as "rotating spokes" from video for four sets): read the
   mechanism in game (ray scan, motion trace, dense frames, the log) and write the finding down; a fix built on an
   unverified hypothesis is not tested.
3. **One change per set**, behind its own `ap set` switch where it is a behaviour (A/B in the same build).
4. **Runs per side** = `Runs` in `docs\SPLITS.md` (from the leg's spread over its last 10 full passes: enough runs that
   a 2 % / 5 s gain stands out of the noise; 3..8). A segment-local change may be judged on that segment's runs from the
   nearest checkpoint save (cheaper), the full leg after. All runs of a set on one commit, at once
   (`flleg.ps1 -Instance 1/2/3`), check-ins on all; no code change while they run. A set side = `tools\flset.ps1 -Instance
   n -Leg L -Set s -Variant v -Runs r -ApSet 'k v,k v'` (one pwsh per instance; switches A/B in one build); several
   arms interleaved on one instance: `tools\flarms.ps1` (rotate the arm order per instance). The set drivers (`flset`,
   `flarms`, `flgate`) run every leg with `flleg.ps1 -NoTables` and regenerate the tables (`legtable.js` -> `docs\LEGS.md`,
   `splits.py update`) once per set after their loop; each leg still writes its own `splits.txt`.
   **Segment practice** (session 11, 24 Oct 2026): `-SaveAt 'M12.gen1,M12.core'` writes a native save the moment each
   checkpoint is reached (scene `<leg>` step `<checkpoint>`, source `leg-checkpoint`; the SAVE line logs its cost - 126-218
   ms, the game kept drawing ~150-190 fps: no stall); `-StartCp M12.gen1 -StopAt M12.gen3` then runs from the newest such
   save to that checkpoint (`Leg.ps1`: timed from the leg start, kills counted after the save's). `splits.py` marks these
   runs `seg`: their first segment starts at rest after a load, so it is compared between sets (`compare`) but never a
   gold. A save restores position and story state (FREELANCER_RE "Saves") but not the speed: M12.cell's save was made at
   300 m/s in cruise and loads at rest inside the core, so every run from it starts the exit airlock run from a standstill
   in the worst place - the same bad exit in 6 of 6 runs on two builds (24 Oct 2026 checkup, sets apb563 / aprefac: the
   run hits the structure, "no transit in 3 s", a refused dock, ~16 s lost; the user, watching: "absolutely terrible"
   against the PB). Such a save is fit for build-against-build comparison, not for a time that means anything against
   par; a practice save must be made where the story waits at rest (M12.banner runs true). **Which saves are usable** (session 11): one made while the
   story waits (M12.banner: at rest before the banner) runs true; one made **at** a trigger's moment does not - M12.gen3
   (written at the kill) loads stuck on "destroy the generators" with the generator gone; and after any mid-mission load
   the objective reads "Awaiting", so the script goals offer only destroy goals (`mission.cpp` byTitle) - from M12.hole
   the autopilot fought the Omicron Minor Nomads instead of flying to Toledo. **The save-fit check** (the user, 24 Oct
   2026: "a sanity check on if specific saves created for the purposes of testing checkpoints within a speedrun are
   actually representative of what the autopilot is actually supposed to do"): `-SaveAt` records the ship's state at the
   save in its `scene_saves.tsv` row - `speed` (debug `ship`), `cruise` (`pilot`), `hull` / `shield` (fractions),
   `nanobots`, `batteries` (debug `ai self`, else the last `self` event), `objective_text` (the story's step objective:
   the last id other than 0 and 21660 "Awaiting") - with `fit` = `unverified`. The first run from it (`-StartCp`) judges
   it on its first frame in space, before the objective mode moves the ship, and once the load's objective settled (a step
   objective seen, or 10 s): **unfit** when it was saved above ~50 m/s or in cruise and loads at rest or below half that
   speed ("saved at N m/s (cruise), loads at M m/s"), or when the objective after the load differs from the recorded one
   or reads 21660 ("objective <saved> -> <loaded> after the load"); else **fit** with the load state. The verdict goes
   into `fit`, `fit_note` (+ the first segment against the last 10 full PASS runs of it - a note, never the verdict) and
   `fit_date`, the transcript logs `SAVE-FIT <fit>: <note>`; a judged save is not judged again (hand verdicts stay).
   `flset.ps1` / `flarms.ps1 -StartCp` refuse an unfit save (the throw names its note; `-Force` runs it anyway), warn on an
   unverified one, and stop a set when its first run judged the save unfit; `flleg.ps1 -Save` directly is not checked.
   `drift_check.ps1` (`saves`) flags runs from an unfit save after its verdict. A gap the check cannot see (M12.gen3: the
   same objective, stuck on a trigger that already fired) gets a hand verdict in the row. **No build
   while a set runs** (session 10: each leg copies the DLLs at its boot - a rebuild between a set's legs mixed builds and
   voided runs); a stopped set's aborted runs leave no history row - note them in its log.
5. **Verdict by segments** (`tools\splits.py compare <set A> <set B>`, label regexes): the changed segment's median
   against the previous set's (same N), and no other segment of the leg WORSE (beyond max(2 s, its spread, 5 %)); keep
   or revert, then write the verdict (dossier note, commit). **And the routes, every set, every segment** (the user, 24
   Oct 2026: "I'm concerned you weren't able to figure the pathing issue from comparing to research data" - times and
   speed profiles hid an M12 hop flown 0.2-0.4 km off the user's line into the lair's inner wall): `tools\routecmp.py
   <leg folders>` against the reference flight (the user's own trace where one exists), and for the segments it does not
   cover ref.py's speed / phase profile side by side with ours (time at engine speed outside the one charge, braking,
   stops). Every LONGER / OFF-ROUTE / SLOW / GUARD flag is read in the trace before the set is called done - a flagged
   segment is a finding, also when its time looks fine.
6. **Shared code** (travel, combat, dock, guard, objective logic) changed -> the leg gate (`tools\flgate.ps1`: every leg
   once) before the next change set, compared checkpoint by checkpoint with the last gate (`splits.py compare
   'gate-<last>$' 'gate-<new>$'`); a WORSE segment of one run is a lead to re-run (one run is noise), a leg whose
   segments stay worse is a regression to fix first.
7. **Speedrun knowledge vs general play**: behaviour that only pays because of a mission's script (a trigger distance, a
   bespoke route) is speedrun knowledge and names its source (`controller\docs\AUTOPILOT_UX.md` 2a); general behaviour
   must not key on mission ids. The legs judge the speedrun preset; the sweeps / fight matrix judge the general play.
   Since session 11 `flleg.ps1` applies `ap preset speedrun` at every leg's boot (before a set's `-ApSet`); the preset
   holds the kept speedrun switches (`autopilot_settings.cpp` presets; AUTOPILOT.md "Speedrun flight").
8. **Behaviour-identical refactors** are checked with `tools\apdiff.py`, not the gate (checkup 24 Oct 2026, audit
   step 0): before the refactor fly 2-3 short segments (`flset.ps1 -StartCp/-StopAt`, at least 3 runs, the same save and
   instances) on the pre-refactor build - that is the baseline; after it fly the same segments and run
   `apdiff.py -Baseline <label>@<commit> -Run <label>@<new commit>`. It compares the normalised `autopilot` event
   sequence and kinds and the motion trace per segment against the baseline's envelope (2 % + floors) and exits 1 on a
   flag. Calibrated on the archive (its header): a flag on the same segment in every judged run is a finding to read in
   the log, a flag in one run of three is noise, fights are never judged by the motion - and the full leg stays the gate's.

## The test install and its data
- Test install made with the official installer and the user's options: `tools\install-hde.ps1 -Dest
  '<FLvibed>\Install\Freelancer HD Edition'` (Borderless Windowed, DxWrapper + d3d8to9, recommended options,
  vanilla 10 s jump tunnels, startup intros kept; side effects in the script header). Ask the user before changing
  these options.
- **Install policy (user, 21 Oct 2026):** HDE 0.7.1 stays the primary test install and the benchmark baseline until
  HDE publishes v0.8; the upstream HDE + FLSharp 0.9.2 becomes a second, separate install for compatibility sweeps
  only (not built yet). Reasons, differences and the plan: `research\hde-upstream.md`.
- **Nothing of this project's game outside the project (user, 18 Oct 2026):** the user may play a personal Freelancer
  install on this PC; everything the test install writes stays in it unless the user says otherwise. Its
  `EXE\flplusplus.ini` has `save_in_directory = true` + `screenshots_in_directory = true` (HDE's flplusplus redirects
  Common.dll `GetUserDataPath`): saves, `PerfOptions.ini`, screenshots in `Install\Freelancer HD Edition\SAVE` /
  `SCREENSHOTS`. Tools resolve the folder with `tools\fldata.ps1` (`Get-FLSaveDir`, `Get-FLUserDataDir`);
  `install-hde.ps1` applies the setting and undoes the installer's side effects (Documents PerfOptions.ini it created,
  the Windows uninstall entry - one AppId for every HDE install). New FLvibed files the game should keep per user go
  through the game's `GetUserDataPath`, never Documents directly. Not redirectable: the game's own log
  `AppData\Local\Freelancer\FLSpew.txt` (rewritten each run).
- `deploy.ps1` keeps the save folder under the load screen's 256-save limit (oldest `SaveXXXX` -> `SAVE\overflow`,
  never deleted).

## Fixtures and campaign saves
Saves: `Load-FLSave <file>` loads any save by file name through the game's own load screens (well under a second in
game); Freelancer lists the saves it found at startup, so a new save file needs a restart (`deploy.ps1 -Save` copies
one in first). Fixtures (`Install\Freelancer HD Edition\SAVE\Accts\SinglePlayer`, `Get-FLSaveDir`):
- `SaveC701` free roam docked at Manhattan (500k credits, launch allowed); `SaveC700` is a former test slot (currently
  a copy of SaveC701).
- `SaveC702` free roam in space near Manhattan.
- `SaveC703` free roam in space near Manhattan with every gate and hole open (the others keep the story's
  `locked_gate`s: gates answer `info_access_denied`).
- `SaveC705` = SaveC702 with the M02 ship's loadout (gun HpWeapon01, Javelin launcher HpWeapon02 + 36 Javelins, gun
  HpWeapon03): missile / combat tests straight into space.
- `SaveC706` = SaveC705 with the M09 ship (bh_elite: hull 3600, shield 2297, 4 guns ~1630 hull/s, missiles, torpedo,
  mines; built 19 Oct 2026 from campaign Save4686 by `tools\mkfixture.js`: the ship block swapped into a free-roam
  fixture; the sweep's fight-matrix section builds it when missing): fights with a strong ship.
- `Save2b9b` M01 Manhattan bar (Juni = talk_story), `Save4559` / `Save49c2` M01 Manhattan (story start; check the
  objective before use).
- Campaign saves for every mission (a third-party set, local in `research\saves\`): `missions\SAVES.md` (catalogue, how
  to use them safely; `tools\flsave.js` decodes and catalogues saves - use it, do not guess counts or loadouts). Never
  `AutoSave.fl` / `Restart.fl`. Scene saves (saved natively right before re-testable scenes): `missions\scene_saves.tsv`.

**Pick (or build) the fixture that reaches the test's subject with the least unrelated time** (user, 18 Oct 2026: a
story save made every test sit through M02's intro scene); new fixtures are edited copies of the plain-text free-roam
saves.

## Rules for driving tests (from the user)
- **Test in the background** (user, 17 Oct 2026): start the game with `deploy.ps1 -Background` so it never takes the
  user's screen; everything (loads, menus, flight, combat, docking) must work out of focus. Never steal focus
  (`Focus-FLGame` is a no-op then). No hard-coded key presses where the game has native logic (movies, menus). Never
  let anything play sound. The user's design rules for it: 100 % seamless (not even a split-second focus or mouse
  grab), never change Windows or game settings (a mixer mute was rejected: silence is in-process, the render client's
  silent flag), all game systems keep working as in a normal start, test flags only in the game process's environment
  (FLVIBED_BACKGROUND), never stored. A background start's window title names it (user, 21 Oct 2026): "Freelancer -
  FLvibed test instance <n> (background, muted)" or "... test, main install ..." (controller host's watcher thread,
  re-applied every second). If new code may still take focus, ask first and close the game right after.
  **The silence and the inactive window are FLvibed's own**: a game without FLvibed loaded comes up in front with its
  sound. Every start outside `deploy.ps1` (which installs FLvibed itself) calls `Assert-FLWillLoad <EXE dir>` first
  (`tools\fldata.ps1`: FLvibed.dll present and listed in dacom.ini, else no start) - 26 Oct 2026, a test copy uninstalled
  by the setup got only the DLL back, started loud in front of the owner.
- **Drive tests by game state and data, never by fixed delays, guessed menu positions or screen coordinates:**
  `Wait-FLUntil` on `state` / `frame` / `ship` / `hotspots ... ready` / `visible`, events (`Wait-FLEvent`),
  `Wait-FLSShown -After`, `Watch-FLGame`. Timeouts are safety limits only; fixed durations only as measurement
  windows. Interact natively (the game's own logic: `click`, `goto`, `talk`, `menu`, `pause`, `loadsave`), not by
  moving the cursor or pressing keys; native interaction must never degrade or alter game logic. When a needed
  state or action has no native access yet, find it in the game (`FREELANCER_RE.md`, `research\disasm`) instead
  of adding a delay.
- **Skip animations that are not the subject of the test**, as a player's Escape does (user, 17 Oct 2026):
  `Load-FLSave`, `Goto-FLRoom` and `Start-FLLaunch` skip room entries, room story scenes and the launch sequence
  by default (`Skip-FLAnimation`: USER_CANCEL only while the game reports one running: `hotspots ... busy:
  navbar|no navbar|scene`); pass `-Watch` when the animation (or a scene's dialogue: subtitle tests) is the subject.
  `deploy.ps1` skips the startup movies unless `-Intro`. In space only the docking / landing cutscene (docking camera)
  is skippable; story cutscenes in space are waited out (HDE excludes them from its Esc skip). Never send USER_CANCEL
  blindly in a loop: inside a mission scene it can cut the mission's own sequence short (17 Oct 2026: the M01 launch
  then did nothing).
- **Mission runbooks** (`missions\`, one per story mission, README = purpose + template): read the mission's runbook
  before testing in it. Changing the harness (function names, event formats, a closed gap) means updating the runbooks'
  harness lines in the same commit.
- **Mission flow awareness first (user, 17 Oct 2026):** before and during every step know the mission state and what
  is on screen (`Get-FLStoryState`, the mission runbook, events, a screenshot when the state does not explain what
  happens). Blocking UI and mission scripting are the most common test failures; when something does not react,
  read the state before retrying, and turn every new failure pattern into a harness check.
- **Know the save's story state first** (mission, objective, what is locked): only try interactions the story
  allows (e.g. M01 blocks launching until Juni's job is accepted; the Launch button `IDS_HOTSPOT_LAUNCH` exists only
  when launching is allowed). Read a mission's script / runbook before testing in it (timers and nags fail missions;
  `Act_NagDistTowards / Leaving` escalate to FAIL): no code reloads, rebuilds or long experiments during a timed phase;
  select targets explicitly (a stale test target once shot the friendly USV Brandt).
- **Scene saves** (user, 18 Oct 2026): in long missions save natively right before each scene or fight likely to be
  re-tested and catalogue it (`missions\SAVES.md`, `scene_saves.tsv`); re-tests start seconds before the subject
  (mission scripts: `-From <step>`).
- **Mission scripting must never stall a test**: read the mission's script (`DATA\MISSIONS\<mission>\*.ini`, the
  runbooks) for what comes next (the next room, offer, popup, briefing), use `Get-FLStoryState`, and let the waits
  close story blockers (`Close-FLBlocker`).
- **Tests judge quality, not completion** (user, 20 Oct 2026): a PASS needs good behaviour (time against the plan and
  the history, sane choices, the product doing the work rather than the helpers), judged against the 2020 playthrough
  where it exists (memory tests-judge-quality, crossref-reference-sources).
- **Every unattended test samples screenshots and runs check-ins** (`deploy.ps1 -Background` starts the sampler;
  `tools\flcheckin.ps1` every ~2 min in the background; unexpected behaviour = stop, read the logs, fix).
- **Reference runs by the user** (23 Oct 2026, M12): the user flies a set piece on the main install, in front with
  sound (memory player-starts-with-sound: build and commit first); the motion trace records the player's flight
  (`FLMotion.tsv`, mode -1) and a sampler runs at 10 fps / 1920 px (`tools\flshots.ps1 -Fps 10 -Width 1920`). Tell the
  user which save and where it places them; they need a few seconds to settle - the opening is no reference. Note the
  tick of every reload (each try separate), keep the good try under `research\experience\reference\` (trace rows, log
  excerpt) and read the user's own notes on what was not ideal before deriving a benchmark from it.
- **Watchdog every running test - high priority** (user, 23 Oct 2026: "allowing tests to go on and stay stuck is
  counter-productive and a time waster"; an M12 leg held 3 km off a generator for 8 min until its clock failed it):
  check-ins every ~60 s while legs run, each read as it lands; a stuck or known-wrong test is stopped at once (stop its
  task, close its instance), diagnosed and rerun - never left to its time-out. Legs stop themselves on no progress
  (`Leg.ps1 -StallSec`, `controller\docs\HARNESS.md`).
- **Judge a change by several runs** (user, 23 Oct 2026: "run the current build 3 times ... and judge it by all three
  before changing anything else"): leg results vary a lot run to run (M12 session 7: hops 31-86 s, the same build 1/3
  and 3/3 by chance of cruise disruptors and fighters). Since 24 Oct 2026 the number of runs comes from the leg's
  measured spread and the verdict is by checkpoint segments: "Speedrun splits" -> "The test loop" above. A single run
  is for diagnosing, not for judging.
- **Cheap test first, sized to the change** (user, 19 and 21 Oct 2026): a quick low-cost probe before any long or
  high-quality run; the shortest test that exercises the changed path (a fixture + one maneuver: SaveC703 -> Manhattan
  ring dock is 75 s; a repro script; `route` with arguments), the mission sections left to the next full sweep; bundle
  several small changes per test run (compile-check locally, then one sweep). Teleports (`Move-FLShipNear`) and cruise
  boost (`Set-FLCruiseSpeed`, brake early) shorten pinpoint tests, never tests that follow a mission's flow.
- **Test what the change can affect, nothing it cannot** (user, 21 Oct 2026): before choosing a test, trace the
  change's reach (which code paths, contexts, modes and data it touches) and test those dimensions only. A dimension
  the change cannot influence is not multiplied in: e.g. the speaker name colours (computed from the data, drawn the
  same in every mode) needed one display mode through the paths that colour names (room scene, comm-path cutscene,
  flight), not the four-mode `display-subtitles` section; the display matrix is for changes to placement, scaling or
  the HUD layout. Say in one line why the chosen test covers the change and what it leaves out.
- **Close the game when your testing ends or is aborted** (end of turn, interruption, handoff), unless the user is
  about to test (user, 17 Oct 2026). Skip the intros unless working on movies (`deploy.ps1` does). Screenshots of the
  game window when the other tools are not enough (`Save-FLShot`).

## Session state and the environment hooks
- `HANDOFF.txt` = the current state only (mode, now / next, plan status, open issues), **under 8,000 characters** -
  Claude Code caps hook output at 10,000 and a longer handoff arrived as a 2 KB preview (20 Oct 2026); finished items
  move to `HANDOFF_history.txt` (append, never delete). Both are versioned since 21 Oct 2026 (they were git-ignored).
- The environment's hooks (`..\..\.claude\hooks\`): `compact-handoff.js` injects the handoff after every compaction and
  into every new or resumed session (matchers `compact|startup|resume` since 21 Oct 2026), `context-warn.js` warns near
  the context limit and notices a compaction by the context drop, `afk-guard.js` (Stop) blocks idle turn ends while
  `..\..\.claude\afk.flag` exists (the user is AFK on a plan; delete it when they are back).
- Keep the handoff and the docs consistent both ways (user, 20 Oct 2026): a newer handoff fact goes into the docs, a
  doc fact that matters to the current work goes into the handoff; recency usually wins, correctness and the guiding
  principles always do. Update the handoff at every milestone and before long test runs, not only near the limit.
- A fresh session per work package is preferred over compaction (audit, 21 Oct 2026): end at a clean point (commit,
  handoff current, game closed), start the next session; compaction is the fallback when a package overruns.

## AFK sessions: continuous work (user, 23 Oct 2026)
The user's AFK go-ahead covers working, iterating, fixing, testing, self-reflection and course corrections, without
asking, **continuously until the user says otherwise** (it also allowed a child session near a full context, which an
AFK turn cannot start: 3.). The protocol:
1. **Start**: create `..\..\.claude\afk.flag` (one line: "AFK continuous - plan in FLvibed\HANDOFF.txt", the time). The
   Stop hook (`afk-guard.js`) then blocks idle turn ends. Create a **session cron keep-alive** (CronCreate, every ~20 min,
   prompt: "AFK continuation: continue the next unfinished item in FLvibed\HANDOFF.txt; never end idle") - a turn that
   ends while a background task or cron is pending is allowed by the hook, and the cron (not the task's notification
   alone) guarantees the session is re-prompted. Every turn ends with a background task running (a test, a check-in).
2. **Work**: the handoff's AFK plan in order; record each decision taken for the user in the handoff ("AFK decisions",
   one line each) and each result in the dossier / docs; commit after every finished change; `tools\backup.ps1` every few
   hours and at package ends. Course-correct when stuck (memory reconsider-when-stuck): after 2-3 failed attempts at one
   thing, step back, write down why, change approach or move to the next item.
3. **Rollover near a full context**: not possible from an AFK turn - `start_session` is refused in a turn the cron
   keep-alive or a task notification started ("only on the user's own words"), and the user is not there to start one
   (session 8, 23 Oct 2026). Long runs are carried by compaction with the handoff current at every milestone (4.); only
   the user's return ends AFK (delete the flag and the cron).
4. **Compaction contingency** (compaction carried an AFK session for the first time on 23 Oct 2026): the handoff is the
   only memory that survives, so it is brought current **at every set boundary** (a set started / judged, a change
   committed) - its "RESUME HERE" block names what is running (set label, instances, start time, the switch list), what
   was decided and the next step. After a compaction (or on any doubt): the post-compaction audit before anything else -
   read the injected handoff; `git status` / `git log -5` in both repositories; which games run (`Get-CimInstance
   Win32_Process -Filter "Name='Freelancer.exe'"`) and which set drivers (`pwsh` with `flset` / `flleg` in the command
   line); the set logs in `research\experience\legs\sets\`; the cron keep-alive exists (CronList - recreate it if not);
   re-arm the result Monitor (it expires after 30 min). Then judge a finished set or wait for a running one - never start
   a second driver on an instance that has one, never build while a set runs (the legs copy the DLLs at their boot) - once every leg of the set has booted (the
   set log's last `run n/n` line on each instance), a build is safe and goes to the next set (session 12).
5. **The improvement loop and its periodic review** (the user, 24 Oct 2026: keep improving the speedrun autopilot
   without stopping, to meet par; "a periodic check for drifts and regressions, so that your continuous testing and
   changes doesn't just loop infinitely without meaningful improvements"). The loop is "The test loop" above, one
   target at a time from the handoff's ordered list. **Every 3 judged sets or every 2 hours**, whichever comes first:
   `tools\drift_check.ps1 -Leg <target leg(s)>` (decisions, kb, replay, settings documented, the generated files
   current, the static spec copy, doc references, tools listed, handoff size, git clean, backup age, research staleness,
   saves, and `splits.py progress`; the full list in its header). Its verdicts are binding:
   - **NO PROGRESS** on the target leg (3 sets without a new best set median, by max(2 s, 1 %)): stop changing that
     target. Write down why the last attempts failed (dossier), then either change the approach (probe / research the
     mechanism first - "Probe before a fix") or move to the next target in the list. Never a fourth variation of the
     same idea.
   - **UNRELIABLE** (PASS under 5 / 6 in a recent set): fail states first - read every failed run before any speed work.
   - any other ATTENTION: fix it before the next set (a regression in decisions / replay is a stop-the-line).
   - every 6 hours, or after a shared change is kept: the leg gate (`tools\flgate.ps1`) and `splits.py compare
     'gate-<last>$' 'gate-<new>$'`; a leg that got WORSE is the next target.
   - the research check-up round (docs\research\README.md) when the review shows stale notes and nothing is running.
   The review's line goes into the handoff ("REVIEW <time>: ..."). A kept change is turned on by default only after
   the gate shows no regression on the other legs (speedrun-only knowledge stays behind its preset / switch and names
   its source - `controller\docs\AUTOPILOT_UX.md` 2a). Long runs (the leg gate, several legs in a row) go
   as detached pwsh (`Start-Process`), not as tool background tasks: those end at the tool's 10 min timeout. Quote the
   script path inside one `-ArgumentList` string (`"-NoProfile -File `"$g`" ..."`): an array splits the path at its
   spaces and pwsh only prints its help. `deploy.ps1 -Impl -Instance <n>` STARTS the game when that instance has none
   (in front, with sound - session 8: the gate had already closed it); for a test instance always add `-Background`,
   and build-only work uses `build.cmd impl` / `deploy.ps1 -NoStart` (the instances are hard-linked mirrors: deploy only
   while no instance game runs).
