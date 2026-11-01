# Test automation: UI crawlers, layout checks, real-input tiers, attended check cards, token-lean agent loops, ratchets
Status: current
Checked against: FLvibed 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 3 watched-path commits: 32c80fce (T8's layout lint built in part: `hud2_lint.h` + debug `hud2 lint`, 79 unit checks, the first approved dump `docs\hud2_lint_baseline.txt`; no pseudo-localisation, no SSIM goldens, contrast n/a), dff3fac2 (guard, 141/141), 5dcf6195 (routing test, 195/195); T7 / T9-T13 and the real-pad path gaps unchanged)
Dirty paths at check: another session's uncommitted `controller/src/impl/autopilot_impl.h`, `controller/src/impl/autopilot_objective.cpp`, `controller/src/impl/autopilot_settings.cpp`, `controller/src/impl/lookahead.{cpp,h}`, `core/fluf.cpp`, `tests/fluf_probe_test.cpp`, `tools/setwatch.sh` (not judged) (31 Oct 2026 ~15:25)
Watch: controller/src/impl/gamepad.*, controller/src/impl/uikit.cpp, controller/src/impl/controller.cpp, controller/src/impl/debug.cpp, controller/docs/HARNESS.md, docs/TESTING.md, tools/drift_check.ps1, tools/flsweep.ps1, tools/claude/**
Evidence: none saved (web pages read 29 Oct 2026)
Sources: a research agent's report (web + the repo); the coordinating session checked the repo findings
(`gamepad.cpp` 90-128, `controller.cpp` 2283-2295, `uikit.cpp` 178-192). Tags: [page], [file], [assumed]

## What it means for FLvibed

*31 Oct 2026 ~15:20 (32c80fce, short form):* **T8 is built in part** (32c80fce): a pure layout checker over HUD 2's drawn rectangles (`controller\src\impl\hud2_lint.h`, adapter `hud2_lint_live.cpp`, debug `hud2 lint`; 79 unit checks) with U35's measurable rules (zones, text floor, centre gaps, duplicates, prompts, budgets, title-safe) and a live baseline as the approved text dump to diff against (`docs\hud2_lint_baseline.txt`, FAIL in every context on today's HUD). Not yet: the dump per commit as a ratchet, pseudo-localisation, SSIM golden screens, contrast (needs frames), screens beyond the flight HUD. The T7 crawler and T10 cards are untouched.

*31 Oct 2026 ~04:00 (fc379a3, short form):* the golden / fixture pattern this note recommends reached the harness's own readers: `tools\fixtures\reader_rows.tsv` (8 `legs.tsv` rows with their expected class) is checked against `splits.py full_pass_row` / `stopat_row` (a planted 78752d8-style bug fails 3 rows), and `test_hooks` counts a hook's own refusal as covered, not a stumble (c6005a2 / c8c24ee). The UI side (T7 crawler, T8 layout lint, T10 cards) is untouched; the two real-pad path gaps stand.

*29 Oct 2026 (04b4c05, new; the owner: automate as many tests as possible, reach as many human use-case checks as
possible, at the lowest token cost and the fewest mistakes; a sweeping round of virtual and real inputs; no regression,
drift or bloat):* the harness has most of the parts already - `uiitems` (`debug.cpp` ~905, every control's name,
rectangle, depth, the hovered one), `ui options rows|press|state|focus`, the prompt bar's status, `uigen`;
`drift_check.ps1`'s ~20 fitness checks; the gate, `splits.py replay`, `fltest.ps1` run folders, the sweep's `pad-input` /
`display-matrix` sections and `-Attended`. What is missing:
- **A focus-graph crawler (T7)** over every screen with the virtual pad: reachability, no dead ends, B backs out,
  reversibility, prompts match actions.
- **A layout lint with golden dumps (T8)** from FLvibed's own draw rectangles (the rules in `controller_ui.md` "HUD
  rules"), pseudo-localisation at +30 %, then a small SSIM set; only flagged regions go to an image read.
- **Real-input tiers (T9)** - the virtual pad does **not** exercise the real-pad path (below); record / replay of the
  owner's pad is free, a PC-commanded Pico pad covers the USB stack.
- **Attended check cards (T10)** - the owner's checks at zero tokens during the session.
- **Token practices, test impact, ratchets (T11-T13).**
Two real-pad findings in the code (checked; U38; *both overtaken 30 Oct 2026, e38ca61 / 08a99a7 - see the dated line
below*):
1. The virtual pad is merged **after** the real XInput read (`gamepad.cpp` 104-124), so slot scanning, unplug /
   reconnect, the out-of-focus gate and glyph-family detection (`glyphs.cpp` 47-85) never run in a test.
2. The two pad readers gate focus differently: flight ignores the real pad out of focus only with `focus_only` on
   (`controller.cpp` 2289), the menus always (`uikit.cpp` 185). With `focus_only` off and the game in the background, the
   pad can fly while the menus ignore it - intended or not, it is written nowhere. Whether the game's own DirectInput
   also sees the Elite (double input) is unknown - only a real device shows it.
*30 Oct 2026 (12e7942, short form):* the owner decided the focus rule (c5900da): a background game ignores the pad, one
gate for flight and menus (U38, not built). T11's `run-judge` runs on opus now (8f32140), spawns get a model from the
routing hooks (09fe148). T7-T13 still open.
*30 Oct 2026 (4ca36df, short form):* **both real-pad findings above are overtaken.** 1: the device-layer test pads
(e38ca61, T36 slice 1) serve XInput slots 0-3 from shared memory, so slot scan (`pad_slots.h`, unit test), unplug /
replug, the focus gate, the glyph family (from the test pad's vendor id) and rumble now run in a test (sweep
`pad-device` PASS); the old virtual pad stays the focus-free harness hand. 2: one focus gate for both readers
(08a99a7, `focus_gate.h`, U38 built): `focus_only` the opt-out for flight and menus alike. Left from U38: the real pad
(J7) and the game's DirectInput double input. **T7 slice 1 built** (b376a21, `flcrawl.ps1`): baseline CRAWL PASS 5
screens / 29 controls / 116 edges, a planted dead end caught, and it found a real bug at once (the LAN / INTERNET
prompt let the focus walk onto the main menu behind it, fixed); slice 2 = bases, A / LB-RB / LT-RT, K&M. T9's
pad-slot pin exists in part: while test pads are active the system's XInput is not read (a real pad cannot feed that
instance). T13's "crawler coverage never falls" now has a baseline to ratchet. T8, T10-T13 still open.
*30 Oct 2026 (aafe41d, short form):* **T36 slice 2 built** (8e19482, WORK_QUEUE Q5): with `pad_backend` 1 SDL3's
virtual Xbox Series / DualSense / Switch Pro pads run in the game by their real USB ids (host debug `sdlpad`, shm v3,
flctl `*-FLSdlPad*`), so type, glyph family, buttons / sticks / LT, the options page, hot-plug and the rumble / trigger
rumble / effect read-back are tested with no device (sweep `pad-sdl` PASS). It found a real gap at once: the options
page's own pad reader stayed on XInput - "every pad path" means every `Gamepad` reader (the slice's retro). **T7:**
harness dry-runs (8e758a7, H16: `FLV_RECORD` / `FLV_REPLAY`) replay a recorded crawl's verdict in 33 s with no game and
stop DIVERGED on a changed path - the crawler's harness bugs no longer each cost a game boot; slice 2 not started.
**T13 done in part** (0977002: `docs\ratchets.tsv`, the release DLL size, raised with its reason for SDL3). The
pad-slot pin (T9) holds for the SDL pads too (the newest test pad is the pad in use); the real-input tier stays open.
T8, T10-T12 still open.
*30 Oct 2026 ~11:50 (16df8bd, short form):* **recorded game state as host-free fixtures** (Q6 S1-S5, 4e1c8e7 ..
d0715f7; fafb9dd): seven `tests\fixtures\situation\*.snap` snapshots recorded in game, their host-free hash equal to the
game's (13 / 13 after the v2 re-record), the arbiter's rules unit-tested over them (38 tests, 22,265 checks at S5) and checked
in game in shadow (28 / 28 agree) - the golden-dump pattern T8 proposes for layouts, applied to decisions; a changed
rule meets its fixtures before a game. The haptics rules are pure and unit-tested the same way (1893428, `haptics` 130
checks; sweep `pad-haptics` both backends; the real popup source and the feel on a real pad not verified, J8). **T13:**
doc size budgets (a125547, `doc_bytes:<path>` rows for the four always-loaded maps + WORK_QUEUE.md). T14 complete
(fa8fc40, the SubagentStart hook; `claude-harness.md`). T7 slice 2, T8, T10-T12 still open.

## Findings

### 1. Automated UI testing in the industry
- Unity's Game Crawler: screen-to-screen randomised interaction, errors / warnings recorded, stuck detection, soak runs,
  fed by a scripted run to its start (https://docs.unity3d.com/Packages/com.unity.automated-testing@0.7/manual/GameCrawler.html).
- stb-tester "Smart Navigation": learns a menu by exploring with only a reader that names the focused item; relearns on
  change (https://stb-tester.com/blog/2024/02/29/smart-navigation).
- Bevy's focus-graph properties: fully connected, reversible, intuitive (moves follow the screen direction), forgiving
  (octant tolerance) (https://hackmd.io/@bevy/BJeSeclbSkg).
- Unreal: the Automation Driver (UI by element id / path), Gauntlet sessions, a Screenshot Comparison browser
  (https://dev.epicgames.com/documentation/unreal-engine/screenshot-comparison-tool-in-unreal-engine; details from a
  summary).
- Rare, Sea of Thieves: gameplay tests built in from the start, every check-in passing them
  (https://gdcvault.com/play/1026366/Automated-Testing-of-Gameplay-Features).
- EA SEED: curiosity-driven RL agents for coverage are costly (days); imitation learning far cheaper (20 min vs 5 h)
  (https://www.ea.com/seed/news/cog2021-curiosity-driven-rl-agents) - for menus a graph search is the right tool.
- Visual regression in practice: SSIM with a threshold (~0.98), ignore masks for dynamic regions, a baseline per
  platform, a human approves every baseline change (https://bugnet.io/blog/how-to-build-a-screenshot-comparison-tool-for-qa).
- Pseudo-localisation (doubled vowels, +30 % length) exposes overflow; Godot ships it
  (https://docs.godotengine.org/en/stable/tutorials/i18n/pseudolocalization.html).

### 2. Real-input options
- **ViGEmBus is retired** (archived 2 Nov 2023; its updater contacts a domain the project no longer owns - a security
  risk; the Legacinator tool fixes the config; the successor VirtualPad announced, no release found):
  https://docs.nefarius.at/projects/ViGEm/End-of-Life/.
- GP2040-CE (RP2040) presents as XInput / PS4 / PS5 / Xbox One / Switch, buttons from wired pins
  (https://github.com/OpenStickCommunity/GP2040-CE); a PC-commanded Pico pad from TinyUSB's community XInput driver plus a
  serial command parser like pts-pico-firmware (https://github.com/calebccff/pts-pico-firmware), ~$5-10 in parts.
- Windows' `InputInjector.InitializeGamepadInjection` needs a restricted UWP capability; whether desktop XInput games
  see it is unverified (https://learn.microsoft.com/en-us/uwp/api/windows.ui.input.preview.injection.inputinjector.injectgamepadinput).
- vJoy makes DirectInput devices only [assumed]; Titan Two record / replay unverified.
- Tiers (T9): **T0** the shared-memory virtual pad (exists, logic only); **T1** record / replay - a record mode in the real
  read writes the Elite's states to TSV in attended play, replay through the shared-memory pad (human timing and stick
  noise; free, ~half a day); **T2** a virtual XUSB device (ViGEmBus: a kernel driver, UAC, the updater fix - the owner's
  call - or Windows' injection if it works); **T3** a Pico with custom TinyUSB XInput firmware over serial (no driver;
  the real USB stack, hot-plug, a PS5 vendor id for glyphs; 1-2 days of firmware) - preferred over a retired driver.
  T2 / T3 need a test-only pad-slot pin from the environment so the owner's own pad never feeds a test instance; menu runs
  with real devices are attended slots unless a test-only flag relaxes the menus' focus gate. Human only: feel, rumble,
  paddles and Xbox Accessories profiles, Bluetooth, ergonomics.

### 3. Cheap human checks (T10)
- In-game reporters: one hotkey captures the frame, the log before the press, the build and the state; some attach the
  last seconds of video (https://docs.accelbyte.io/development-toolkit/developer-guides/blackbox-error-reporter/Issue%20Reporter/,
  https://bugnet.io/blog/how-to-build-an-in-game-bug-report-button).
- Design: `tools\flcheck.ps1` + a DLL overlay. A card file, one row per step: id, fixture, setup commands, instruction,
  expected result, an optional automatic assert (an event or a `uiitems` match), what to capture. The game in front and
  unmuted (the standing rule); the automatic assert runs first, the human judges only what machines cannot; holding
  LS + RS: A pass, B fail, Y note (marks the moment), X redo; each verdict writes a TSV row (build, device, state), an
  in-process frame, a log mark and the last 10 s of `flshots`. No tokens during the session; afterwards only the failed
  rows and their crops are read.

### 4. Token-lean agent loops (T11, T12)
- Subagents take a `model:` field and return only a summary; the docs advise routing tasks to Haiku for cost and warn
  that detailed reports from many subagents fill the main context (https://code.claude.com/docs/en/sub-agents).
  `run-judge` is `model: inherit` today (`tools\claude\agents\run-judge.md`; game-facts and docs-auditor are `sonnet`).
- Tools should return token-efficient output (https://www.anthropic.com/engineering/writing-tools-for-agents). Prompt
  caching hits only an exact prefix: editing CLAUDE.md, adding an MCP server or tool, or injecting timestamps mid-session
  invalidates it [page, third-party].
- Test selection: Meta's predictive selection halved test cost while reporting > 95 % of failures and > 99.9 % of faulty
  changes (https://ar5iv.arxiv.org/html/1810.05286); Google TAP selects by reverse dependencies of the changed files
  (https://huang.isis.vanderbilt.edu/cs8395-f23/paper/google-testing-icse-seip-17.pdf). For FLvibed a dependency map is
  enough: `docs\test_map.tsv` (path glob -> sweep sections, crawler screens, unit tests, apdiff segments) and
  `tools\flimpact.ps1 <rev range>` printing the smallest set (the memory rule "test scope by reach" made a tool).
- Practices: every tool ends with one verdict line + a path and writes `summary.json`; agents read summaries, never raw
  logs; deterministic checks (log greps, crawler, lint) before any image read; images only on failure, cropped; judges
  and check-in monitors on `sonnet` / `haiku` with a fixed short report (verdict, evidence paths, at most 10 lines);
  waits in the background.

### 5. Ratchets and fitness functions (T13)
- Fitness functions = automated checks of architectural characteristics, run continuously
  (https://thoughtworks.com/radar/techniques/architectural-fitness-function); `drift_check.ps1` is FLvibed's.
- Google: ~1.5 % of test runs flaky, ~16 % of tests with some flakiness, larger tests flakier; rerun and track, never
  ignore (https://testing.googleblog.com/2016/05/).
- New rows for drift_check: the release DLL's size budget (+3 % needs a note); `perf cpu` ms per frame on each pad screen;
  crawler coverage never falls; the layout golden files; a size budget per doc beside the handoff's; a flaky-test
  quarantine list with an owner and an expiry (flake rates logged, never silently retried; the hover-A gap held there).
