# Testing FLvibed: build, deploy, test install, rules

The testing rules the user set, and how the test install, deploy and sweeps work. Moved here from `..\CLAUDE.md` on
21 Oct 2026 (the audit's CLAUDE.md diet); the rules are unchanged. The harness functions themselves are described in
`..\docs\HARNESS.md`; the mission runbooks in `missions\`; the game knowledge base in `kb\`.

## Build and deploy
- Build through the lock: `tools\buildlock.ps1 <target>` (one build at a time across sessions; it also stamps
  `build\<config>\build_info.txt`, what the build was built from, which the legs record). The command guard refuses a
  direct `build.cmd` call. The targets:
- `build.cmd dev` (default): `build\dev\FLvibed.dll` (every module's host part) + `FLvibedLogic.dll` (every module's logic,
  hot-reloadable as one). `build.cmd release`: `build\release\FLvibed.dll` alone.
  `build.cmd impl`: the logic DLL only. `build.cmd sigtool`: `build\sigtool\sigtool.exe` (signatures for `core\flexe.h`). `build.cmd test`: the host-free unit
  tests (`tests\`, seconds, no game; `tools\README.md`). `build.cmd setup`: the players' installer (`tools\setup\`).
  `build.cmd bench`: flbench.exe in the build folder's bench folder (generated), the design bench (HUD 2's draw kit with no game; `tools\README.md` `hudbench\`).
- `tools\deploy.ps1` (PowerShell tool, full path): builds, writes `dacom.ini` (FLvibed.dll) and `FLvibed.ini`,
  copies `modules\subtitles\data\*.tsv` to `EXE\FLSubtitles\`, then hot-reloads (game running, DLL unchanged) or starts the
  game, skipping each startup movie natively (`movieskip`: the game's movie loop sees it finished) while its video
  window exists. Switches: `-Impl` (logic only, hot reload, about 18 s of which 17 the build - never a restart for the ini files, 7 Nov 2026: iterate a change this way, the release build only for the last check; `reload` or `unload` on either module's channel is one act for all the logic, `core\loader.h`), `-Release` (the player build), `-Restart`, `-NoStart`,
  `-NoBuild`, `-Intro` (let movies play), `-Save <file>` (start, then load that save by name), `-Modules
  subtitles,controller` (which modules run; each alone runs in the background too; both off or `safe = 1` is a game with no
  guard at all, so never started: `tests\modules_nothing.ps1` proves that state with no game), **`-Background`** (start without taking focus, game runs out of focus: use
  it for automated tests so the user keeps the screen; every harness call works out of focus; it also starts the
  screenshot sampler `tools\flshots.ps1` for the session - a standing order: look at `research\experience\shots\<time>`
  while waiting), `-Instance <n>` (also a prepared folder's name, e.g. `-Instance vanilla11c` for the vanilla 1.1
  install, since 29 Oct 2026 - deploy and flleg; the set drivers stay numbered) (parallel test games, n >= 1: `Install\instances\<n>`, a hard-linked mirror of the
  test install made on first use, with its own saves, logs, screenshots `shots\<time>_i<n>` and debug channels
  `Local\FLControllerShm_<n>` / `Local\FLSubtitlesShm_<n>`; `$env:FLVIBED_INSTANCE = '<n>'` points the harness of a
  PowerShell session at that game; `tools\fldata.ps1` Get-FLInstallDir / Get-FLGameProcess / New-FLInstance).
  `deploy.ps1` refuses to start the game when it would write to the shared `Documents\My Games\Freelancer`
  (`Assert-FLDataSeparate`) and warns when the test evidence under `research\` passes 20 GB (the user decides about
  pruning; nothing is pruned by itself; on the user's word `tools\prune_evidence.ps1`, which keeps the 3 fastest runs per leg).
- **Controller input without a device** (30 Oct 2026): the virtual pad (`Set-FLPad`, focus-free) and the device-layer
  test pads (`Set-FLDevPad`: XInput slots served from shared memory - slot scan, hot-plug, the focus gate, glyph family,
  rumble; sweep section `pad-device`) and, with `pad_backend` 1, the SDL virtual test pads (`Connect-FLSdlPad -Type
  xbox|ps5|switchpro`: SDL's own virtual devices with the real pads' USB ids - type, glyph family, buttons, axes,
  hot-plug, rumble / trigger rumble / effects read back; sweep section `pad-sdl`); the haptics' effects on both
  backends' streams (debug `haptics`, sweep section `pad-haptics`: the pad must be the device in use - `set input_mode
  1` - and the gate open) - `docs\HARNESS.md`.
- **Sweeps:** `tools\harness\flsweep.ps1 -Parallel <N>` builds once, splits the sections over N games by their median
  times and runs them at once (measured 20 Oct 2026: the full sweep 2688 s on 3 games vs ~7900 s serial, every mission
  SUCCESS; 1 game 191 fps, 3 games 154-163 fps each). Section `target-churn` (0.2.1): formation / goto / autopilot
  dock under target churn and stick nudges, ~2 min (`docs\HARNESS.md`). Under load the harness polls slower, so a test must never race
  the game: a threshold the test depends on is enforced game-side (base-repair: `spawn clearat <hull>` despawns the
  attackers - and their missiles in flight, counted as damage already done - the frame the hull reaches it; harness
  polling missed it and the ship died). A sweep ends after the game has exited (a dying game counted as running for
  deploy: exit 1 after PASS runs). Never build during a sweep. **Reused games** (the sweep loads the next fixture
  natively instead of restarting, ~30-40 s saved per section; user, 20 Oct 2026): fresh for the first section, after
  any failure, for fresh-only sections (missions, story, fight matrix, release) and after 5 sections / 20 min; a failure
  in a reused game is re-run fresh and a fresh pass is reported STATE-LEAK (a bug of its own); `-Fresh` for full sweeps;
  results record fresh vs reused. Results: `research\experience\runs\sweep_<date>.txt`;
  timing history `tools\flbenchhist.ps1`; one test start with evidence `tools\fltest.ps1`; check-ins `tools\flcheckin.ps1`
  (text digest per instance every ~2 min, ATTENTION on death / stall / repairs / a frozen game - no frame in 3 s, with
  per-thread CPU, the busy threads' code locations and our logs saved by `tools\flfreeze.ps1`).
- **Speedrun legs** (22 Oct 2026, `LEGS.md`): `tools\harness\flleg.ps1` runs one leg per game instance from a save,
  several at once (one pwsh each, build once before); each instance has its own copies of the FL*.dll (not hard links),
  so a new build can be deployed to an idle instance while others run. Prefer a leg (minutes, from the nearest leg
  save) over a long chain; a chain run only for what crosses leg boundaries. A failed leg keeps the last 15 s of its
  recording (`end_tail.mkv`, `end_sheet.jpg`: every 5 fps frame, time-stamped) beside its logs. A leg whose log has
  `autopilot minefield-damage` (hull lost to a mine in a mine-danger field; not a zone's steady damage) is a FAIL whatever the mission says (the
  user, 23 Oct 2026, for Zone 21; the rule holds for every minefield). **Fail states** (the user, 23 Oct 2026: fail a
  run as soon as it cannot be a good one - shorter test cycles): the checkpoint limits ("Speedrun splits" below) plus
  the event rules of `tools\harness\missions\leg_failstates.tsv`, rows per leg (`M12`) or for all (`*`): `event` = a
  log event matching the pattern ends the leg `FAILED (FAIL STATE ...)` at once; `by` = the pattern must appear within
  `secs` of the anchor's first event (no anchor: of the leg start). `Leg.ps1` checks them live; `-NoFailStates` turns
  them off. Each row says why (its data). (The first per-leg `by` rows - 1.3x the leg's best, M12's lair / core
  deadlines - were replaced on 24 Oct 2026 by the data-derived checkpoint limits.)
- **Evidence pruning** (the user, 24 Oct 2026): `tools\prune_evidence.ps1` (dry run by default, `-Apply`; `-All` also
  failed legs and sessions tied to no leg) deletes session videos (`.mkv`) only - screenshots, logs, motion traces, leg
  folders and failure end clips stay - and **always keeps the full data of the 3 fastest passing runs of each leg**
  (mission + start save). First run 24 Oct: 28.7 GB of video from 775 sessions, evidence 40 -> ~15 GB. Second, 29 Oct
  (the owner): `-All -OlderThanDays 3` (15.3 GB), then the UI captures in `research\experience\runs\` thinned on the
  owner's word (29.4 -> 15.3 GB; every action in `runs\_thinned_2026-10-29.tsv`): folders no doc or tool cites lost
  their images (text kept); doc-cited folders kept every file a doc / tool / commit / their own notes names, bursts
  (>= 200 frames in a folder) every 10th frame as PNG, the rest as full-resolution JPEG q92; the spec's image sources
  (`tools\uimock\spec_images.py`), folders a tool reads and recent runs untouched. Left: `hud2x_verify_2026-10-25`
  (2 GB, its generic file names all count as named). Evidence 48 GB after.
- **Windows 7** (a player's 0.1.0 report, a tester's on 0.3.0; no Windows 7 VM, the owner): `tools\win7_check.ps1` (run by
  `package.ps1`, which fails a release on it, and by `drift_check`) looks every import of the release DLL, the setup and
  the server plugin up in `tools\win7_exports.tsv`, the real export tables of Windows 7 SP1 x86 (6.1.7601.23403, from
  KB3125574; `-Reference` rewrites it). `tools\win7_imports.tsv` holds only what that update lacks. It also fails on a name
  in the DLL's text that a current system DLL exports, Windows 7 does not, and `core\host\win7sim.cpp` does not list, and on a
  binary whose Rich header has a part from a toolset past build.cmd's pin (MSVC 14.44: VS 2026's no longer targets Windows 7), a stale library included.
  `FLVIBED_SIM_WIN7=1` in the deploying shell's environment (like `FLVIBED_BACKGROUND`, never stored) makes the game
  take its Windows 7 paths here: XInput from `xinput9_1_0` only, `DwmFlush` failing (Aero off), and every newer API and DLL
  unfound at run time, FLvibed's, SDL's and the C runtime's look-ups alike (the log's `win7 sim: refused` lines; a release
  build: a dev build's logic DLL is not patched). Proved
  by these: the loader finds every import; each fallback runs. Not proved: a real Windows 7 machine, and a real pad read
  through xinput9_1_0.
  `tests\win7_speed.ps1`: the keyboard's throttle reaches top speed and comes back, on HUD 2, the simulation, a pad, the controller off and classic.
  `tests\strafe_release.ps1`: a pad strafe stops on the release, whether or not the key map lists the command. `tests\dock_far.ps1`: the autopilot's dock past the game's range. `tests\mod_rows.ps1 -Case rooms|fight|lane`: a mod's room subtitles, HUD 2's ring in a fight, the autopilot's lane ride.
  `tests\throttle_after_launch.ps1`: straight after a launch the throttle reads the ship's own, and a short press of the throttle key leaves the speed alone (passes on stock HDE, 6 Nov. Not run on the build before `fl::GetThrottle` called the game's getter; that build's drop was read from probes on the mod, FrontierSpace launches 2, 4 and 9).
- **The autopilot under windows** (GitHub issue 25): `tests\ap_under_window.ps1`, flying, and with `-Fight` fighting on the pad and on keyboard and mouse, with a no-window control.
- **The pad's toggles and the native target keys:** `tests\pad_toggles.ps1` (GitHub issues 29, 30), `tests\target_keys.ps1` (HUD 2 against classic).
- **PlayStation and Switch pads with no setting:** `tests\pad_fallback.ps1`. A test game leaves the machine's own pad alone (`FLVIBED_BACKGROUND`): debug `padfallback hid|0|1` gives the fallback its answer, `padfallback rumble` a pulse on the pad in the hands. A device-layer pad stands in for a mapper's mirror and for a real Xbox pad beside it; row `injected` sends real injected input (F24, a mouse move of one pixel and back) through the low-level hooks with the game behind. A real Steam Input run and `dimouse`'s drop at the reads need the game in front: the owner's pad check.
- **Before a game is spent on a test script:** `tools\test_lint.ps1` (in `drift_check`) resolves its harness calls and their parameters.
- **A game started in front runs no frames while another window has focus,** so `Send-FL` times out with the game healthy (`Responding` true, the log still): read what a player's run needs while it is in front, or from its log afterwards. One client on the debug channel at a time: a polling watcher starves other calls.
- **Module matrix** (sweep section `modules-matrix`, release build, one fresh game per row): all on, each cut module off, the controller off, each cut
  module on with the controller off; hooks judged against the all-on game's table, verbs against an unknown verb; core only and `safe` by
  `tests\modules_nothing.ps1`. `deploy.ps1 -Modules subtitles,controller,speedrun=0` writes a row only when it is named. The startup movies are skipped there:
  `tests\movie_modules.ps1` lets them play: all on, Subtitles alone, Movies alone (frames), a reload during a movie (dev build).
- **A background start never takes the screen** (the owner, 6 Nov 2026: a test of FrontierSpace changed the whole screen's resolution): that mod's `patches.ini` sets byte 0x1B16CC to 1, which is exclusive fullscreen whatever its comment says, and `display = 0` left no borderless window, so the game took the monitor at 1024x768. `deploy.ps1 -Background` now refuses such a start (`Assert-FLNoFullscreen`, `tools\fldata.ps1`: the patches.ini row when FLPatch is listed, else the exe's byte; another build's exe is refused too). A prepared instance from outside is made windowed before its first start. Only `flsweep -Attended`'s fullscreen row passes, through `FLVIBED_ATTENDED`.
- **Display matrix** (sweep section `display-matrix`, 21 Oct 2026): one game instance (the worker's, or instance 9 in a
  serial sweep; never the main install) is started fresh per configuration - borderless 3840x2160, bordered windowed
  1600x1200 (4:3) and 2560x1080 (21:9), borderless 1280x720, a DPI-unaware process - and both modules are checked (back
  buffer size, window border, DPI awareness, HUD projection, pad throttle, HUD 2 coming on with the pad, a subtitle, a
  screenshot per mode under `research\experience\runs\display_<time>`). **HUD 2 in tests** (25 Oct 2026,
  `modules\hud\docs\UI_HUD2.md`): with `hud_style` 2 (default) HUD 2 replaces the game's panels on every device - the
  virtual pad included - so a test that reads or clicks the game's own HUD controls (`uiitems`, `visible`, `click` on a
  maneuver / contact / status button) runs with `hud_style 0`; the autopilot
  legs and sets drive no pad and see the game's HUD. HUD 2's own state: debug `hud2 state`, `hudinfo`. Exclusive fullscreen only with `-Attended` (it takes the screen). How the
  modes are switched: `docs\kb\display.md`, `tools\fldata.ps1 Set-FLInstanceDisplay`. The `display-subtitles` section
  judges real dialogue at 4K and 720p borderless, 4:3 and 21:9 windowed (Juni's room scene, the M01a opening cutscene,
  King's flight comms) by the subtitles module's `layout` debug report against the design rules - in flight the top
  panel: the maneuver bar moved; at 16:9 / 21:9 the plain panel inside the room between the bar and the NeuroNet box;
  at 4:3 the name-tab layout (the tab in that room, the body below the bar's row, clear of the cruise box, on screen);
  above 0.44 H, the objective banner's text below it (~11 min).
- **Verify a change in game, in the release build too** before calling a stage done (`deploy.ps1 -Release`; the sweep's
  `release-build` section).
- **A release candidate's checks** (the owner, 11 Nov 2026, after 0.4.6's four rounds): each fix is verified by an
  agent other than its fixer as soon as it is committed, not at the end (three fixes reported as verified did not hold
  in the release build); one full item-by-item pass per release candidate, after the branches merge; a fix that lands
  after that pass is checked by its reach (what its files drive) and the automated nets (unit tests, lint,
  `tests\pixels.ps1` against the last release), not by another full pass.
- **The states to check are written at triage, before the fix, and checked by someone other than the fixer** (audit,
  12 Nov 2026, report 12_: the three 0.4.6 fixes whose "verified" did not hold each failed on a state or a form the
  fixer never set up - a scroll and its cost per frame, the hint of the control a word stands for rather than of the
  hidden button under it, a load from space into a richer save - not on dev against release). Each bug or issue gets,
  when it is triaged, its list of states: the display sizes, pad and mouse, docked and in space, the transitions and
  loads into and out of it, moving or scrolling and the cost per frame where it draws, and the form the result must
  take (what exactly the player sees or reads). The fix is checked against that list by an agent or person who did not
  write it, in the release build; a fault found while checking that is older than the release goes to the next one.
  This holds for a fix landing after the full pass too: it gets its own states checked, just no new full pass.
- **The two interfaces' nets** (the HUD 2 rebuild, 12 Nov 2026): every HUD 2 restructure package, and any change the
  lint names for a shared file (`tools\module_lint.py --changed <paths>`, `docs\ARCHITECTURE.md` "The two interfaces"),
  runs them on the release build before and after the change, each on its own instance:
  - **The NeuroNet pages:** `tests\nn_pages.ps1 -Instance <n>` walks LOG, MAP, INVENTORY, STATUS and INFO in the pad
    and keyboard-and-mouse forms, at 3840x2160 and 1280x800, docked and in space, asserting on live values. About 3.5
    minutes; `-Sizes` / `-Forms` / `-Places` narrow it. A KNOWN line marks a fault already in the release.
  - **The classic HUD:** `tests\classic_frames.ps1 [-Instance <n>]` compares 8 frames of the classic HUD (`hud_style 0`)
    at both sizes with a pinned set: flight in both forms, FLvibed's page hosted in the game's Options screen, the bar
    with the pad's prompt bar, the game's equipment dealer walk. About 4 minutes. The flight frames are taken in a still,
    empty sky (SaveC706 raised 25 km) and compared in every cell, no mask (12 Nov 2026). A FAIL names the frame and the
    changed cells (read its `_diff.png`); an INCONCLUSIVE (exit 3: only cells that moved in that run's own window changed)
    is run again, and one that comes back on the same cells is a FAIL; an ERROR (exit 2) is a state the run could not
    reach or hold (the sky not still, a ship within 10 km) and proves nothing. Re-pin (`-Pin`, about 12 minutes; the
    steps: `docs\HARNESS.md`) only on purpose, when a change to the classic HUD was asked for.
  - **The INFO page against the game's window** (audit R3): `tests\info_compare.ps1 [-Instance <n>]` after any change to
    the INFO page or its composers (`core\game\screens\nnhelp_*`, `modules\hud\src\pages\neuronet\info_*`, `pages\pagerun.*`, INFO's shell in `nnpage.cpp`). About 2.5 minutes.
  - **The base pages' recorded baseline** (audit 72_, the page framework's T0): `tests\base_pages.ps1 -Mode check
    [-Instance <n>]` after every change to a docked page (`basepage.*`, the dealers' and boards' parts of `padbases.cpp`,
    `nnpage.cpp`'s INVENTORY / STATUS / LOG and jettison, and every F2-F5 package of `70b_`), on the release build. It walks
    every base page in both forms and both sizes and compares every recorded field with the baseline (taken on a457e082, re-recorded on 13 Nov 2026 after the owner's prompt rule with the pages' drawn texts: audit 101_)
    (before the framework moved anything); a FAIL names the size, form, page, state and field. Masked, by name in the
    script: the job board's offers, the accepted job and the bar's characters (random at each start). Re-record (`-Mode
    record`, then the new sha256 into the script) only when a change to what a page shows or does was asked for, never to
    make a FAIL pass; a record with a walk error writes nothing. About 25 minutes (one game per size); `-Sizes` / `-Forms` /
    `-Pages` narrow it (a narrowed check compares only what it walked).

## Parallel lanes (the owner, 12 Nov 2026)
C++ work runs in several **lanes** at once (at most four with games): each a git worktree `..\FLvibed-lane-<name>` on its own branch,
with its own build folder, lock and game test instance; made and removed only with `tools\lane.ps1`. The rules (when, worktrees,
the game limit, the shared baselines a lane never re-pins, the hooks' view of main): `docs\TESTING_DETAIL.md` "Parallel lanes". Read it before
working in or starting a lane.

## Speedrun splits: checkpoints, par, fail limits, the test loop (the user, 24 Oct 2026)
The method (checkpoints, splits, par, fail limits, evidence layout) moved to `docs\TESTING_DETAIL.md` "Speedrun splits"; the test loop
below stays here. The runners' times are the minimum goal for every leg and segment (the owner, 29 Oct 2026).

### The test loop (replaces "judge a change by three runs")
0. **The runners' times are the minimum goal for every leg and segment** (the owner, 29 Oct 2026: "their times are your
   minimum goal" - for the whole speedrun part of the project): a segment is done when our gold and PB reach par. Per
   segment, before a change: WHAT the runners do, HOW (inputs, speed, distances, consumables, timing), the ADVANTAGE (where
   their seconds come from) and the RISK or what makes it safe for them - they play vanilla 1.1, we play HD Edition
   (e.g. full-damage explosions: FREELANCER_RE "Explosion damage").
1. **Target**: the segment with the largest gap that the runners' data explains - `docs\SPLITS.md` (PB - par, gold - par),
   then `research\evidence\speedrun\reference\ref.py <leg> <section>` (all three runs side by side, frames on demand), the
   leg's dossier entry (`docs\LEG_DOSSIER.md`) and our runs of that segment (`splits.tsv`, the leg folders). Never a
   segment the runners' data does not cover without saying so. **This holds for every leg test, probes and failure
   reads included** (the owner, 29 Oct 2026: a 12-run M13 battleship probe and a Ghidra read went by without one look at
   the runners - interceptors first, the battleship at 0 m, torpedoes from 0 m). The enforcement below is ARCHIVED
   since 12 Nov 2026 with the other leg-run guards (off until the leg work resumes and the private harness's archive
   is switched on; the rule itself stands): the command guard's
   `speedrun-ref` (the first `flleg` / `flset` on a leg in a session is refused with the leg's runner data until the
   command states "# ref: <their approach vs ours>"; `speedrun-ref-read` does the same once for the first read of a
   leg's run evidence), the verdict nudge, which puts the same data beside every leg's RESULT line
   (the private reference hook), and the judge gate (a Stop hook: a turn does not end with a leg verdict that no
   `run-judge` call followed; run-judge's report has a required RUNNERS section). the private Claude Code harness notes has the why
   and the replay numbers.
2. **Probe before a fix** (session 9 lesson: the M12 plate read as "rotating spokes" from video for four sets): read the
   mechanism in game (ray scan, motion trace, dense frames, the log) and write the finding down; a fix built on an
   unverified hypothesis is not tested.
3. **One change per set**, behind its own `ap set` switch where it is a behaviour (A/B in the same build).
4. **Runs per side** = `Runs` in `docs\SPLITS.md` (from the leg's spread over its last 10 full passes: enough runs that
   a 2 % / 5 s gain stands out of the noise; 3..8). **Or decide a side early** (IDEAS T1, 29 Oct 2026;
   `docs\research\test-efficiency.md` 1): A = the finished previous set, `tools\splits.py sprt <set A> <set B> --seg
   <checkpoint>` after every batch of 3 on B - its first line is the verdict: KEEP / REVERT stop, CONTINUE = one more
   batch on B (no verdict before B's first full batch; `tools\flarms.ps1 -Sprt [-SprtSeg <checkpoint>]` runs this check
   after every third round itself, A = `-SprtA`, else arm a, else the first arm of -Order - the same A on every instance
whatever its rotated order (30 Oct 2026: the first of each instance's own -Order inverted A on the rotated one), and ends the set when every arm has its verdict;
   the two-instance form, one `tools\flset.ps1` per variant: `-Sprt -SprtA <reference variant> [-SprtB <other side>]
   [-SprtSeg <checkpoint>]` judges the pair after every third run of its own and stops on any final verdict (both
   sides read the shared history, so both stop) - opt-in
   since 30 Oct 2026; the default is the owner's D1), INCONCLUSIVE (12 runs on B, still between the bounds) reverts,
   NODATA stops (fail closed: a side with no value - an unanchored or mistyped label regex -, A with one value, a run in
   both sets, an option it does not understand). A DNF counts as the segment's fail limit; B failing 2 runs more than A,
   or a leg total slower by its 2 % gain, forces REVERT; exit code 0 always (advice: the caller decides). Its `reach`
   line says when A is too small for B's difference ever to reach a bound - fly A again (`flarms`) or expect
   INCONCLUSIVE. **Which runs' sigma counts** (30 Oct 2026, M13-MIMIC: mm3's sprt could never call - the history's 12 s
   came from M13.sinclair runs before `timer_destroy` changed the segment, the sets' own runs spread 2.0 s): sigma =
   max(history, pooled, floor 1), the history = the last 10 full passes' segment spread, a guard against small sets
   that under-read the noise - **but from 4 degrees of freedom on (nA + nB - 2) the history counts only up to the sets'
   own pooled sd's upper 90 % confidence bound** (sqrt(df s^2 / chi2_0.10(df)); `splits.py` sigma_rule, the line "the
   history's ... capped"): a history far above what the sets' runs can spread is another base's. mm2 -> mm3: sigma 12 ->
   3.2, LLR 0.12 -> 2.45 (CONTINUE at g 2). **A segment's failure is its own DNF** (0ba9c25): a run that timed the target
   segment and failed later is a timed sample there, not a failure in the fails guard (mm4-b_i1 passed M13.bsdown, stalled
   at the Osiris, and had counted among A's bsdown failures; `splits.py selftest` "later failure no fail"). The sprt is
   time-only: a survival change shows in its fails guard and needs its own count (Fisher) beside it. **Before an sprt
   set, its smallest detectable effect** (30 Oct 2026: three M13 sets of 24 legs
ended without a verdict): about 2.5 x sigma x sqrt(2 / cap) - M13.bsdown's sigma 26.6 s at a cap of 12 gives ~27 s; a
change the runners' data does not put above that is not worth the set (cut the variance first: fixed dt, seeds, a
checkpoint save closer to the change). `splits.py selftest` checks its math and arguments. Computed by `tools\reach.py`; 31 Oct - 12 Nov 2026 (archived since with the leg-run guards) the guard `reach-short` refused a set / arms launch whose `reach=<checkpoint>:<runs per arm>:<expected s>` lies below it (`reach=probe` for a run that judges nothing). A segment-local change
   may be judged on that segment's runs from the nearest checkpoint save (cheaper), the full leg after. All runs of a
   set on one commit, at once
   (`flleg.ps1 -Instance 1/2/3`), check-ins on all; no code change while they run. A set side = `tools\flset.ps1 -Instance
   n -Leg L -Set s -Variant v -Runs r -ApSet 'k v,k v'` (one pwsh per instance; switches A/B in one build); several
   arms interleaved on one instance: `tools\flarms.ps1` (rotate the arm order per instance). The set drivers (`flset`,
   `flarms`, `flgate`) run every leg with `flleg.ps1 -NoTables` and regenerate the tables (`legtable.js` -> `docs\LEGS.md`
   and `docs\LEG_DOSSIER.md`, `splits.py update`) once per set after their loop; each leg still writes its own `splits.txt`.
   **Before a segment A/B** (30 Oct 2026, JRS: a pair started 314 m from the hole, past every event the change acts on):
   grep a seed run's log for the change's own events (its switch's event names) and start from a save BEFORE the first
   of them, the `-SprtSeg` the segment that contains them; no such event in the window = the test cannot cover it.
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
   the last id other than 0 and 21660 "Awaiting") - with `fit` = `unverified` (`unfit` at once when taken above ~50 m/s or in cruise, 30 Oct 2026). **Since 30 Oct 2026 (evening) a
checkpoint passed moving defers its save** to the first moment at rest (<= 30 m/s, no cruise) on the same story step,
90 s at most (`SAVE <cp> deferred` / `at rest N s after the checkpoint` / `dropped`); the run is not slowed for it, and
the save's first segment then starts N s past the checkpoint (a `seg` run: compared between arms, never a gold). The first run from it (`-StartCp`) judges
   it on its first frame in space, before the objective mode moves the ship, and once the load's objective settled (a step
   objective seen, or 10 s): **unfit** when it was saved above ~50 m/s or in cruise and loads at rest or below half that
   speed ("saved at N m/s (cruise), loads at M m/s"), or when the objective after the load differs from the recorded one
   or reads 21660 ("objective <saved> -> <loaded> after the load"); else **fit-load** with the load state, which becomes
   **fit** once a run from it reached its first checkpoint, and **unfit** when that run ends STALL before it (since 30 Oct
   2026 evening: SaveS409 loaded right, was called fit, then hung in the sphere entry's letterbox). Two more story
   checks the same evening: no save is taken while the objective reads 21660 "Awaiting" (S417 deferred into it; a
   deferred save waits past it), and a save records the mission marks up (`marks`): when none come back within 5 s of
   the load it is unfit and the run ends HARNESS. And a save made while a **marked mission solar the save will not keep**
   is up is unfit at once (`SAVE <cp> unfit: saved with N marked mission solar(s) a save does not keep (...)`, Leg.ps1
   LostSolars, 30 Oct 2026 late evening): the game re-sends the saved marks of every object that exists after the load,
   but keeps a spawned [MsnSolar] only when its `system =` is the player's system (`docs\FREELANCER_RE.md` "Saves") -
   S416 lost M09's 4 generators that way (the autopilot had no target for 160 s; the step cannot finish from it). The
   verdict goes
   into `fit`, `fit_note` (+ the first segment against the last 10 full PASS runs of it - a note, never the verdict) and
   `fit_date`, the transcript logs `SAVE-FIT <fit>: <note>`; a judged save is not judged again (hand verdicts stay).
   A story-unfit load (the objective changed / "Awaiting") ends that run at once as `HARNESS unfit save: <note>` - the
   autopilot has no step and would only drift to the stall limit (30 Oct 2026: M07's Cali base saves S404 / S405 load
   "Awaiting"; the owner saw two instances idle); a physically unfit one (moving -> at rest) flies on.
   `flset.ps1` / `flarms.ps1 -StartCp` refuse an unfit save (the throw names its note; `-Force` runs it anyway), warn on an
   unverified one, and stop a set when its first run judged the save unfit; `flleg.ps1 -Save` directly is not checked.
   `drift_check.ps1` (`saves`) flags runs from an unfit save after its verdict. A gap the check cannot see (M12.gen3: the
   same objective, stuck on a trigger that already fired) gets a hand verdict in the row. **No build
   while a set runs** (session 10: each leg copies the DLLs at its boot - a rebuild between a set's legs mixed builds and
   voided runs); a stopped set's aborted runs leave no history row - note them in its log.
5. **Verdict by segments** (`tools\splits.py compare <set A> <set B>`, label regexes): the changed segment's median
   against the previous set's (same N), and no other segment of the leg WORSE (beyond max(2 s, its spread, 5 %)); keep
   or revert, then write the verdict (dossier note, commit). With `sprt` (step 4) its verdict on the target segment is
   the keep / revert, `compare` still shows the other segments. **What the sequential rule would have done**:
   `splits.py sprt-replay` replays the history's consecutive sets (3+ full legs each, stop-at runs left out) as step 4
   runs it - A whole, B in batches of 3 - 29 Oct 2026: on the leg total 3 of 53 pairs reached a verdict, none before
   B's last batch (M12's leg spread 20 s against a 7 s gain: leg totals are not affordable - target segments); per
   segment 196 of 576 (15 KEEP, 181 REVERT, 29 of them by a guard), 10 % of B's runs saved; the past 3-run sets were
   mostly too small for any verdict. (Replayed with A in batches too, the rule had looked better - 8 %, a KEEP on M04
   after 3 + 3 runs that A's other 3 runs undo.) Its pairing line (IDEAS T6): the sd of same-instance A-B differences
   is ~the unpaired one (median ratio 0.99) - pairing by instance alone buys nothing. **And the routes, every set, every segment** (the user, 24
   Oct 2026: "I'm concerned you weren't able to figure the pathing issue from comparing to research data" - times and
   speed profiles hid an M12 hop flown 0.2-0.4 km off the user's line into the lair's inner wall): `tools\routecmp.py
   <leg folders>` against the reference flight (the user's own trace where one exists), and for the segments it does not
   cover ref.py's speed / phase profile side by side with ours (time at engine speed outside the one charge, braking,
   stops). Every LONGER / OFF-ROUTE / SLOW / GUARD flag is read in the trace before the set is called done - a flagged
   segment is a finding, also when its time looks fine.
6. **Shared code** (travel, combat, dock, guard, objective logic) changed -> a leg gate is DUE, and **the gate runs only
   when the owner names it** (the owner, 5 and 11 Nov 2026: "do not do a full gate check!"; then, asked which rule holds
   everywhere, "only when I name it" - attended and AFK alike; the full sweep `flsweep` the same). So: write "gate due:
   <what changed>" into the handoff, say it to the owner once, and go on; never ask again and never start it. Until it
   runs, cover the change by the smallest test by reach (a leg from the nearest fit checkpoint save, a probe, a set).
   The gate (`tools\flgate.ps1`: every leg
   once from its start save in `docs\legs_reference.tsv`; `-ApSet` carries a switch to every leg, e.g. the measurement
   gate `situation_shadow 1` read by `tools\shadow_digest.py --gate <tag>`) is compared checkpoint by checkpoint with the last gate (`splits.py compare
   'gate-<last>$' 'gate-<new>$'`); a WORSE segment of one run is a lead to re-run (one run is noise), a leg whose
   segments stay worse is a regression to fix first. **Since 30 Oct 2026 evening every gate harvests** (fresh
   checkpoint saves by default, `-NoFreshSaves` off; after the queue `sets\gate_<tag>_A.tsv` from `tools\gate_harvest.py`
   and `sets\gate_<tag>_par.txt` from `tools\gapreport.py gate`: its result is said as BEAT PAR / AT PAR / CLOSER / SAME
   / FURTHER), and `flset.ps1 -Sprt` refuses to fly an A arm a gate on the same build already harvested (`-FlyA` to fly
   it anyway): the B arm alone against `splits.py sprt --a-samples`.
   **Every launch** (leg, set, gate) carries `# budget: <pkg> n/N` stated before the first run, every set `# reach:`
   (what its runs can detect vs the effect expected, the start save before the change's events), and the live state of
   each new run is read 60 s+ after its start (`tools\launchcheck.ps1`; the Stop hook refuses the turn end otherwise) -
   the guard rules and hooks of the private Claude Code harness notes.
7. **Speedrun knowledge vs general play**: behaviour that only pays because of a mission's script (a trigger distance, a
   bespoke route) is speedrun knowledge and names its source (`modules\autopilot\docs\AUTOPILOT_UX.md` 2a); general behaviour
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
  only (not built yet). Reasons, differences and the plan: `docs\research\hde-upstream.md`.
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
- Campaign saves for every mission (a third-party set, local in `research\saves\`): the scene-save catalogue (private;
  how to use them safely; `tools\flsave.js` decodes and catalogues saves - use it, do not guess counts or loadouts). Never
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
  FLvibed test instance <n> (background, muted)" or "... test, main install ..." (the host's watcher thread, `core\host\focus.cpp`: any module alone gets all of this,
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
  re-tested and catalogue it (the private scene-save catalogue, `docs\missions\scene_saves.tsv`); re-tests start seconds before the subject
  (mission scripts: `-From <step>`).
- **Mission scripting must never stall a test**: read the mission's script (`DATA\MISSIONS\<mission>\*.ini`, the
  runbooks) for what comes next (the next room, offer, popup, briefing), use `Get-FLStoryState`, and let the waits
  close story blockers (`Close-FLBlocker`).
- **Tests judge quality, not completion** (user, 20 Oct 2026): a PASS needs good behaviour (time against the plan and
  the history, sane choices, the product doing the work rather than the helpers), judged against the 2020 playthrough
  where it exists (the reference footage: the 2020 playlist and the top-3 speedruns, `research\videos\`); a "pass" that only completed - a
  section that passed for days while flying past a lane - is a failure. For a verdict that decides a change, hand the
  run to the `run-judge` agent (the private Claude Code harness notes).
- **Every unattended test samples screenshots and runs check-ins** (`deploy.ps1 -Background` starts the sampler;
  `tools\flcheckin.ps1` every ~2 min in the background; unexpected behaviour = stop, read the logs, fix).
- **Reference runs by the user** (23 Oct 2026, M12): the user flies a set piece on the main install, in front with
  sound (build and commit first); the motion trace records the player's flight
  (`FLMotion.tsv`, mode -1) and a sampler runs at 10 fps / 1920 px (`tools\flshots.ps1 -Fps 10 -Width 1920`). Tell the
  user which save and where it places them; they need a few seconds to settle - the opening is no reference. Note the
  tick of every reload (each try separate), keep the good try under `research\experience\reference\` (trace rows, log
  excerpt) and read the user's own notes on what was not ideal before deriving a benchmark from it.
- **Watchdog every running test - high priority** (user, 23 Oct 2026: "allowing tests to go on and stay stuck is
  counter-productive and a time waster"; an M12 leg held 3 km off a generator for 8 min until its clock failed it):
  check-ins every ~60 s while legs run, each read as it lands; a stuck or known-wrong test is stopped at once (stop its
  task, close its instance), diagnosed and rerun - never left to its time-out. Legs stop themselves on no progress
  (`Leg.ps1 -StallSec`, `docs\HARNESS.md`).
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
- the private handoff = the current state only (mode, now / next, plan status, open issues), **under 8,000 characters** -
  Claude Code caps hook output at 10,000 and a longer handoff arrived as a 2 KB preview (20 Oct 2026); a finished item
  is deleted from it (git keeps every version). The private handoff history is frozen since 12 Nov 2026 (the owner,
  audit decision 18: kept and searchable, its pointers still valid, nothing appended). Both are versioned since 21 Oct 2026.
- The environment's hooks (versioned in the private session hooks since 29 Oct 2026, installed by the private harness installer;
  all of the harness: the private Claude Code harness notes): `compact-handoff.js` injects the handoff after every compaction and
  into every new or resumed session (matchers `compact|startup|resume` since 21 Oct 2026; after a compaction or resume
  with the live git / game / set-driver state), `context-warn.js` warns near the context limit and notices a compaction
  by the context drop, `afk-guard.js` (Stop) blocks the run's driver's idle turn ends while `..\..\.claude\afk.flag` exists
  (the user is AFK on a plan; delete it when they are back; the flag's `driver=` line names the session it holds), `guard.js` refuses the repeat-offender shell commands, `edit-check.js`
  checks each written file.
- Keep the handoff and the docs consistent both ways (user, 20 Oct 2026): a newer handoff fact goes into the docs, a
  doc fact that matters to the current work goes into the handoff; recency usually wins, correctness and the guiding
  principles always do. Update the handoff at every milestone and before long test runs, not only near the limit.
- A fresh session per work package is preferred over compaction (audit, 21 Oct 2026): end at a clean point (commit,
  handoff current, game closed), start the next session; compaction is the fallback when a package overruns.

## AFK sessions: unattended runs (user, 23 Oct 2026; rebuilt 12 Nov 2026)
An unattended run is a list of packages the owner approved before leaving: a run file per date (private, like the Claude Code harness). It
has `main` in order, then a ranked `reserve`. Each package has 2-5 done checks, a judge that is not its worker, and a
budget (game boots, spawns, hours). The run's token and hours caps are the owner's, set per run, with no default. One
driver session (the owner's model pick, Remote Control on) works one package at a time in a fresh background subagent,
has it judged by another agent, reads only the verdict file and marks the package. When main and the reserve are
worked, or a cap is spent, it writes a report of at most 300 words (`..\..\AFK_SUMMARY_<date>.md`) and stops: no
rollover, no successor session. Budgets are counted by hooks and at every game boot, from the start commit's copy of
the run file; the budget hooks, the run file and the settings are hashed and locked during the run. What needs the
owner's word (a push, tag, release or GitHub post; the leg gate or full sweep; deleting what the run did not make; a
system-wide install; a game in front or with sound) is neither done nor asked about: it is recorded for him and listed
in the report. A leg package runs only on a leg whose runners' study the owner signed off; its bar is the leg's par,
and two sets without a better judged verdict (or its budget spent) end it. How a run is started, kept up and ended:
the afk-run skill (a private session skill). The tools and hooks: the private Claude Code harness notes, UNATTENDED_RUNS.md (not exported). The lane
and subagent lessons: the AFK playbook (a private process document). In-game tests during a run follow this page as always: background,
muted, the given instance, judged from frames and the live log. The one unattended run ever started (13 Nov 2026) was aborted after two minutes, because its hooks are not lane-aware (a game start from a lane ran the lane's copy of the run tool and tripped the tamper stop); that is not fixed, so no run starts without the owner.

**The improvement loop and its periodic review** (attended leg work - in an unattended run a leg package ends by its own
rule above; the user, 24 Oct 2026: keep improving the speedrun autopilot
   without stopping, to meet par; "a periodic check for drifts and regressions, so that your continuous testing and
   changes doesn't just loop infinitely without meaningful improvements"). The loop is "The test loop" above, one
   target at a time from the handoff's ordered list. **Every 3 judged sets or every 2 hours**, whichever comes first:
   `tools\drift_check.ps1 -Leg <target leg(s)>` (decisions, kb, replay, settings documented, the generated files
   current, the static spec copy, doc references, tools listed, handoff size, git clean, backup age, research staleness,
   saves, and `splits.py progress`; the full list in its header). Its verdicts are binding:
   - **NO PROGRESS** on the target leg (3 sets without a new best set median, by max(2 s, 1 %); a set = one label's full
     legs on one build, 2+ runs - `flset`'s default `-Runs 2` counts - a reused label cut where the build stamp changes
     or 6 h lie between runs, stop-at runs (`-StopAt`) never counted; since 29 Oct 2026, which made M09 and M11 NO
     PROGRESS): stop changing that
     target. Write down why the last attempts failed (dossier), then either change the approach (probe / research the
     mechanism first - "Probe before a fix") or move to the next target in the list. Never a fourth variation of the
     same idea. Enforced 29 Oct - 12 Nov 2026 (archived since with the other leg-run guards; the rule stands): the command guard (the private command guard `set-no-progress`) refuses
     `flset.ps1` on such a leg unless the command names the new approach (`# guard-ok:set-no-progress <hypothesis>`,
     logged in `..\.claude\guard_fires.tsv`).
   - **UNRELIABLE** (PASS under 5 / 6 in a recent set): fail states first - read every failed run before any speed work.
   - any other ATTENTION: fix it before the next set (a regression in decisions / replay / saves is a stop-the-line;
     `health` is the weekly trend, for the end-of-run report).
   - after a shared change is kept: a leg gate is due - "gate due: <what changed>" into the handoff; the gate itself
     runs only when the owner names it, AFK too (test loop 6; no gate by the clock). After one: `splits.py compare
     'gate-<last>$' 'gate-<new>$'`; a leg that got WORSE is the next target.
   - a research note's re-check only where the run's work touched its area (docs\research\README.md; the scheduled
     round was lifted on 12 Nov 2026: stale notes in the review are information).
   The review's line goes into the handoff ("REVIEW <time>: ..."). `drift_check` (each check isolated: a broken one
   is an ERROR row, not a lost review) also writes `..\..\.claude\drift_last.txt`, the one machine-written record of the
   last review (its `health` and `overrides` lines too: the method's own trend, rules overridden 3+ times in 24 h);
   while AFK the command guard read it (29 Oct - 12 Nov 2026, T31; archived since with the leg-run guards): `review-due` refuses a set launch or a dev build when
   it is missing, over 2 h old or unfinished, `stop-the-line` refuses a set while it shows ATTENTION / ERROR on
   decisions / replay / saves (and a flleg run from an unfit save) - each refusal names the step that clears it (the
   `afk-run` skill, the private Claude Code harness notes). A kept change is turned on by default only after
   a gate (run on the owner's word) shows no regression on the other legs; until then it stays behind its switch (speedrun-only knowledge stays behind its preset / switch and names
   its source - `modules\autopilot\docs\AUTOPILOT_UX.md` 2a). Long runs (the leg gate, several legs in a row) go
   as detached pwsh (`Start-Process`), not as tool background tasks: those end at the tool's 10 min timeout. Quote the
   script path inside one `-ArgumentList` string (`"-NoProfile -File `"$g`" ..."`): an array splits the path at its
   spaces and pwsh only prints its help. A direct `flleg.ps1` with an array argument (`-ApSet 'a 1','b 2'`) cannot go
through `-File` (its values split at their spaces, the leg dies at once on a parameter error): pass
`@('-NoProfile','-Command',"& '<path>' ... -ApSet 'a 1','b 2'")` (30 Oct 2026). `deploy.ps1 -Impl -Instance <n>` STARTS the game when that instance has none
   (in front, with sound - session 8: the gate had already closed it); for a test instance always add `-Background`,
   and build-only work uses `build.cmd impl` / `deploy.ps1 -NoStart` (the instances are hard-linked mirrors: deploy only
   while no instance game runs).
