# FLvibed tools

Every tool resolves paths from its own location (`$PSScriptRoot`); run them by full path. PowerShell scripts only in
the PowerShell tool, never from Bash. Python tools run with the project's own interpreter
(`research\tools\ocr\.venv\Scripts\python.exe -I`; there is no system Python); JavaScript tools with Node.

## Build, install, deploy, test
| Tool | What |
|---|---|
| `..\build.cmd` | The only build: `dev` (default), `impl`, `release`, `sigtool`, `test` |
| `buildlock.ps1` | `build.cmd <target>` under a machine-wide mutex: one build at a time across agents / sessions (two builds together break each other's `build\` folder); prints the warnings and the time |
| `format.ps1` | clang-format (the VS LLVM toolset, x64) over `core`, `controller\src`, `subtitles\src`, `tests`, `tools\sigtool` by `..\.clang-format` (the existing style written down); `-Check` lists what would change |
| `ui_meta.py` | Generates `..\core\ui_meta.h` (the options page's tabs / sections / rows, the preset tables, the bindings presets as constexpr data) from `..\docs\UI_SPEC.json`; its alias table maps spec keys to the bus sections / registry keys / value codings; `--check` = stale? (`drift_check` runs it; the Python venv, `-I`) |
| `checkpoints_gen.py` | Generates `..\controller\src\impl\checkpoints_gen.h` (the in-game split detector's checkpoint list: the same conditions `splits.py` matches in the logs) from `..\docs\checkpoints.tsv`; `--check` = stale? (`drift_check` runs it; the Python venv, `-I`) |
| `mk3db.py` | Builds FLvibed's own HUD icon meshes (`..\controller\data\autopilot.3db`, deployed to `EXE\FLvibed\`): a self-contained `.3db` with its VMeshLibrary, material and TGA texture, the game's own CRC-32 for the ids (`mk3db.py crc <name>`); `..\docs\FREELANCER_RE.md` "Icon meshes" |
| `settings_table.ps1` | Generates `..\docs\SETTINGS.md` (every `[Controller]` / `[Autopilot]` / `[Subtitles]` key: type, range, default, help) from the `core\settings.h` registries in the code; `-Check` = stale? (`drift_check` runs it) |
| `..\build.cmd test` | The host-free unit tests (`..\tests\*.cpp`): compiles the pure core pieces (mission, ini, utf, avoid, route, flight, the text-fit rule, the in-tree CreateID `core\game\flhash.cpp`, the hook decoder and FLUF co-residency of `core\hook.cpp` against a simulated FLUF) plain with `tests\space_stub.cpp` for the game into `build\test\fltest.exe` and runs it; seconds, no game (10 tests, 311 checks at da1363f). Exit code = failures. The data tests (the decision sweep vs `decision_baseline.txt`) take the install from argument 2 / `FLVIBED_INSTALL` / `Install\Freelancer HD Edition`, skipped when absent. `buildlock.ps1 test` serialises it with other builds |
| `deploy.ps1` | Build + install + start / hot-reload; switches in `..\docs\TESTING.md` |
| `package.ps1` | The release's files in `build\package\`: `FLvibed-<version>.zip` (release FLvibed.dll, the shipped data, a default FLvibed.ini, README.txt with install / uninstall, CHANGELOG.md, LICENSES) and `FLvibed-<version>-Setup.exe` (`build.cmd setup`) from a fresh release build (`-NoBuild`: the last one, e.g. a published DLL put back); the version from `..\core\version.h`, checked against the DLL and the setup; prints the SHA-256s |
| `setup\` | The players' one-click installer / uninstaller (`setup.cpp`, `setup.rc` with the payload, `setup.manifest`; `build.cmd setup`): finds the game (HD Edition's installer entry, the original game's key, the default folders, its own folder, a folder picker), installs / updates / uninstalls, a Windows Apps entry per game folder; command line `/install` / `/uninstall <folder> [/keepsettings] [/quiet] [/log <file>]`. `setup\test_setup.ps1` tests it on throwaway copies of the test installs' dacom.ini files (84 checks) |
| `logo\make_logo.py` | FLvibed's logo (concept A, the owner's pick 26 Oct 2026: HUD 2's reticle + the chamfered FL VIBED wordmark, drawn as shapes - no font): `..\docs\images\logo\` (logo / mark for dark and light backgrounds, the icon, GitHub's social preview 1280 x 640) and `setup\flvibed.ico`; `--png` renders the PNGs and the icon with headless Edge (Python venv, `-I`) |
| `flset.ps1` | One side of a change set: `-Runs` legs of `-Leg` in a row on one instance, labelled `<leg>-<set>-<variant>`, with `-ApSet` switches (flleg.ps1 sends them after each boot); one pwsh per instance; verdict by `splits.py compare`; segment practice: `-SaveAt` checkpoint saves, `-StartCp` / `-StopAt` runs from them (`..\docs\TESTING.md` "The test loop" 4); a checkpoint save whose `fit` in `..\docs\missions\scene_saves.tsv` is `unfit` is refused with its note unless `-Force`, an `unverified` one warns (its first run judges it, `Leg.ps1` SAVE-FIT) and a set stops when that run judged it unfit; every leg runs with `flleg.ps1 -NoTables`, the tables (`legtable.js`, `splits.py update`) once after the loop (`-NoTables` skips them here too, for flarms.ps1) |
| `flgate.ps1` | The leg gate: every mission leg once, 3 instances at a time (`-Queue a/b/c`, `-Only M12`), vs the best in `research\experience\legs\legs.tsv` -> `gate_<tag>.tsv`; legs with `-NoTables`, the tables once after the queue |
| `decision_check.ps1` + `resolver_check.py` | The offline decision check: `resolver_check.py` (the versioned Python port of `core\game\mission.cpp` AdvanceGoals; `tests\decision_test.cpp` checks the two against each other) over every objective, diffed with `decision_baseline.txt`; `-Update` rewrites it |
| `install-hde.ps1` | The test install from the official installer with the user's options (side effects undone; header) |
| `fldata.ps1` | Where the test install keeps its data: `Get-FLInstallDir`, `Get-FLSaveDir`, `Get-FLUserDataDir`, `Get-FLGameProcess`, `New-FLInstance` |
| `fltest.ps1` | One test start with evidence: background, sampler checked, run folder + manifest |
| `flshots.ps1` | Screenshot sampler (started by `deploy.ps1 -Background`): a JPEG every 6 s + 5 fps H.264 segments |
| `flcheckin.ps1` | Check-in digests per game instance every ~2 min (ATTENTION on death / stall / repairs / game gone / a frozen game) |
| `flfreeze.ps1` | Freeze watchdog (dot-sourced by the check-ins): `Test-FLFrozen` (frame heartbeat unchanged over 3 s, read from the shared memory), `Save-FLFreezeEvidence` (per-thread CPU, where the busy threads and the main thread run - EIP + stack samples by module - and our logs, to `research\experience\freeze\`) |
| `flsheet.ps1` | Contact sheet of the frames since the last check-in, captioned with log events |
| `flbench.ps1` | Benchmark history across sweeps: time per sub-section, regressions vs the median |
| `backup.ps1` | Git bundles of FLvibed (and, while it is on disk, the archived pre-merge `..\FLvibed-subtitle-review`) to the backup folder: `-Dest`, else `$env:FLVIBED_BACKUP_DIR`, else the first line of the local, git-ignored `.backup_dir` in the repository root (this machine: a folder on the D: drive); keeps the newest 10; `-ShowDest` prints the folder |
| `..\controller\tools\flctl.ps1` | The harness loader; its parts `flctl.core / rooms / space / missions / travel / ui / saves / autopilot.ps1` (`..\controller\docs\HARNESS.md`) |
| `..\controller\tools\flsweep.ps1` | The regression sweep (`-Parallel N`, `-Fresh`) |
| `..\controller\tools\flfights.ps1` | The fight matrix (sweep section `fight-matrix`) |
| `..\controller\tools\flbench.ps1` | Combat benchmarks and A/B runs |
| `..\controller\tools\flroute.ps1` | Hazard-free routes from the system files (minefields, danger fields, damage zones) |
| `..\controller\tools\flleg.ps1` | One speedrun leg (`missions\Leg.ps1`) on its own instance from a save, unattended, with evidence; one row in `research\experience\legs\legs.tsv` (`-NoTables` for the set drivers) |
| `..\controller\tools\flrace.ps1` | Race practice and lap-time benchmark on the M06 course (fixture SaveC707) |
| `..\controller\tools\make-testsaves.ps1` | The free-roam fixture saves |
| `..\controller\tools\missions\` | Mission scripts: `Leg.ps1` (any story leg by one set of rules), `Campaign.ps1`, the older M01a / M01b / M02 / M03 / M04 / M05objective scripts; `leg_checkpoints.tsv`, `leg_failstates.tsv` |
| `..\subtitles\tools\flsub.ps1` | The subtitles harness |

## Game data and research
| Tool | What |
|---|---|
| `kbgen.ps1` | The knowledge base's generated tables from the install (`-Check`: still matches the install?) |
| `dis.ps1` | Disassembly ranges from `research\disasm\Freelancer.ann.asm` (`-From 0x.. -To 0x..`) |
| `sigtool\` | Signatures for `core\flexe.h` (`build.cmd sigtool`; `sigtool.exe gen <exe> code|data <va>`) |
| `dump-resources.ps1` | Strings / infocards of the resource DLLs -> TSV (`research\strings\resources.tsv`) |
| `utf-list.ps1` | UTF tree listing / extraction |
| `flhash.ps1` | CreateID hashes of game names |
| `flsave.js` | Decodes and catalogues saves (counts, loadouts) - use it, never guess |
| `mkfixture.js` | A free-roam fixture save with another save's ship + loadout |
| `flmeta.js` | Ship / equipment meta from game data (`..\docs\SHIP_EQUIPMENT_*.md`) |
| `splits.py` | Speedrun splits (OCR venv python, `-I`): the runners' splits per checkpoint of `..\docs\checkpoints.tsv` and par (`build`), ours from every archived leg (`history`), PB / gold / spread, the fail limits for `Leg.ps1` (`limits`), their validation on the archive (`replay`), `..\docs\SPLITS.md` (`table`), LiveSplit files (`lss`), one run's report (`run <leg folder>`), the progress review per leg (`progress [<leg>] [--since]`: every set in time order, NEW BEST / UNRELIABLE / NO PROGRESS); `update` after every leg (`flleg.ps1`). Method: `..\docs\TESTING.md` "Speedrun splits" |
| `routecmp.py` | Route check (OCR venv python, `-I`): every checkpoint segment of leg runs, our flown route against a reference flight (M12: the user's try 3) - path, deviation, height, slow / guard seconds, flags LONGER / OFF-ROUTE / SLOW / GUARD; a step of every set (`..\docs\TESTING.md` "The test loop" 5) |
| `flarms.ps1` | A multi-arm change set on one instance: `-Rounds` rounds of the arms in `-Order` (one leg each via flset.ps1 `-NoTables`), each arm = `-Base` + its `-Extras` switches (`'b=x 1;c=y 1,z 1'`); one pwsh per instance, order rotated; segment practice via `-StartCp` / `-StopAt` (`-Force` passed to flset.ps1 for an unfit save); the tables once after all rounds |
| `apdiff.py` | The refactor check (OCR venv python, `-I`): archived leg runs compared as structure, not equality - `-Baseline <folders \| label[@commit]> -Run <folders \| label>`: the normalised `autopilot` event sequence (first divergence, per-kind counts against the baseline envelope, kinds in one side only), the motion trace per checkpoint segment (duration, distance, mean / max speed, cruise and slow seconds, `--tol` 2 % + floors; `~` where the baseline itself spreads wider), outcome / time / build; exit 1 on a flag; `--segments <re>`, `--quiet`. Formats and calibration in its header; when to run it: `..\docs\TESTING.md` "The test loop" 8 |
| `holdstats.py` | Ordnance at static-target holds (OCR venv python, `-I`; leg folders or `--label <regex>`): per target the hold's seconds, torpedoes launched against the 2 s refire, the longest torpedo gap, salvo checks not ready, mines (`hold-mine`), missiles, the predicted kill's lead (`doomed`) |
| `docrefs.py` | The doc reference check (OCR venv python, `-I`): every backticked repository path in the tracked `.md` files must exist (rooted paths against the root / the doc / a module folder, relative ones against the tracked files and the local material on disk; placeholders, game-data paths and `docs\research` skipped - its header); exit 1 on a broken one (`drift_check` runs it) |
| `drift_check.ps1` | The periodic drift / regression check: decisions, kb tables, splits replay, `ap set` keys documented, the generated files current (`SETTINGS.md`, `ui_meta.h`, `checkpoints_gen.h`, the static spec copy `docs\spec\` by `uimock\export.ps1 -Check`), tools listed, doc references resolve, handoff size, git clean, backup age, research staleness, saves (runs from an unfit checkpoint save after its verdict; the count of unverified ones), and the progress review (`splits.py progress`: NO PROGRESS / UNRELIABLE) per target leg; exit 1 when anything needs attention (`..\docs\TESTING.md` "AFK sessions" 5) |
| `prune_evidence.ps1` | Prunes the test evidence's session videos (`.mkv`) of passing legs (dry run by default, `-Apply`, `-All`); always keeps the full data of the 3 fastest runs per leg; screenshots, logs, traces and leg folders stay (`..\docs\TESTING.md` "Evidence pruning") |
| `research_check.ps1` | The staleness check for `..\docs\research`: per research note the commits since its check-up that touch its watched paths (`..\docs\research\README.md` "The check-up round") |
| `flmarket.js` | The commodity market table from the install's data: local prices, who sells what, the net-worth effect of each purchase (`node tools\flmarket.js [install] [--sells] [--good] [--base] [--routes <hold>]`) |
| `legtable.js` | The speedrun comparison table in `..\docs\LEGS.md` (leg + whole-run times, the top-3 runs vs our bests, checkpoint bests) from `legs.tsv` and `..\docs\legs_reference.tsv`; run by `flleg.ps1` after a leg, by the set drivers once per set (`flleg.ps1 -NoTables`) |
| `hitfx.js`, `aledump.js` | Hit-effect table from the .ale files (`..\docs\kb\hit_effects.md`) |
| `storydump.ps1` | Per-mission story transcripts from the game data (written outside the repo: game content) |
| `runbook-coverage.ps1` | Runbook completeness against the mission scripts |

## Gameplay-video study (no playback, ever; downloads need the user's OK)
| Tool | What |
|---|---|
| `ytfetch.ps1` | Fast video-only downloads into `research\videos` |
| `vidcat.js` | The video catalogue (`research\videos\catalogue`): videos, segments, notes |
| `vidhud.js` | HUD per frame: target bracket, hostile red, shield / hull / power bars |
| `vidspeed.js` + `speedglyphs.json` | Speed readout by digit templates |
| `vidstudy.js`, `vidpool.js` | Fights and throttle / aim / damage phases; pooled over many videos |
| `vidsheet.ps1` | Contact sheets of successive frames |
| `vidbanners.ps1`, `winocr.ps1` | Objective-banner timeline per video (Windows OCR) |
| `ocr\flocr.py` | The Freelancer text reader (CRNN-CTC trained on the game's fonts and strings); `vidmissiles.py` scans the weapon / contact panels of the playlist (NVDEC crop, 67x real time at 2 fps) |

One-off research scripts live in `research\tools\` (git-ignored): `section_cost.py` (a mission section's time / hull /
consumables vs the 2020 footage), `ringdock_ab.ps1`, `cutscene_float_probe.ps1`, `hold_after_jump.ps1`, `ocr\ratebench.py`.
