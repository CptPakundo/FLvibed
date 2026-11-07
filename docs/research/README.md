# Research: index, structure and the rules that keep it current

Research = what the project learned from outside sources (other projects, mods, communities, decompilations,
speedruns) and what it means for FLvibed. It is kept **for use**: by topic, with one backlog of ideas, each idea
with a status and the project state it was last checked against. Timestamps stay, structure comes first.

## Standing orders (user, 21 Oct 2026)

1. **Structured for use.** One note per topic (not per session), the same sections in every note, one deduplicated
   backlog (`IDEAS.md`) that the notes feed. A reader must find "what does this mean for FLvibed now" in the first
   screen of every note.
2. **Never stale.** When the project changes, the ideas touching that area are re-checked and their statuses fixed.
   When new research is done, the whole set goes through the check-up round (below), not only the new part.
3. **Concurrency-safe.** Research runs alongside sessions that change the project. Every note and every idea
   records the FLvibed commit it was checked against; conclusions are checked against the *committed* state, and the
   uncommitted paths present at check time are listed so a later reader knows what was in flight. Research sessions
   write only `docs\research\` and `research\evidence\`; they never touch the private handoff, code, `CLAUDE.md` (beyond
   the pointer to this folder) or the working session's docs. Findings those docs should carry go into `IDEAS.md`
   as items for the working session.

## Where things are

| Path | What |
|---|---|
| `docs\research\README.md` | this index and the process |
| `docs\research\IDEAS.md` | **the backlog**: every actionable or theoretical idea from any research, deduplicated, with id, area, status, source, checked-against |
| `docs\research\<topic>.md` | one note per topic: header (status, checked against, evidence), "What it means for FLvibed" (links to idea ids), then the findings in full |
| `docs\research\CHECKUP_HISTORY.md` | every note's earlier check-ups (header and index entries), newest first per note |
| `research\evidence\<topic>\` | git-ignored raw material the notes cite: downloaded files, extracted inis, manifests, exes for signature regressions, data tables, scripts |
| `tools\research_check.ps1` | prints HEAD, the dirty paths, and for every note the commits since its checked-against commit that touch its watched paths |

## Notes

| Note | Topic | Checked against | Status |
|---|---|---|---|
| `freelancer-decomp.md` | FreelancerDecomp/Freelancer-Decomp: full C++ rebuild of the 1.1 client; symbol cross-reference | 3c9c1390 (6 Nov 2026, short form: 52 watched-path commits: the module split P0-P11e (moves, comments, map rows); flexe.h 217 -> 228 (K1 105 behind); FREELANCER_RE rows from the game, not the decomp; no cross-reference touched) | current |
| `flsharp.md` | BC46/FLSharp: the HDE plugin that moved from 0.8 (test install) to 0.9.2 (upstream HDE) | 3c9c1390 (6 Nov 2026, short form: 22 watched-path commits: the module split P0-P11e (moves, comments, map rows); 11 new symbols, none in FLSharp's source (grep); 228 / 228 on the upstream exe; no clash on paper) | current |
| `discovery.md` | Discovery Freelancer 5.3: patched 1.1 exe, FLUF, DSAce; signature regression | 3c9c1390 (6 Nov 2026, short form: 34 watched-path commits: the module split P0-P11e (moves, comments, map rows); flexe.h 217 -> 228; hook.cpp adds only report rows; 228 / 228 on all four exes, 0 padding spans, measured this round; no effect) | current |
| `crossfire.md` | Crossfire 2.0.1: launcher-run `EXE2`, anticheat, one-script campaign | 3c9c1390 (6 Nov 2026, short form: 16 watched-path commits: the module split P0-P11e (moves, comments, map rows); lock_trap, spawn_charge and ash_ride read the live table with no Crossfire-specific content; no effect) | current |
| `fluf.md` | FLUF (Freelancer Universal Framework, The Starport): status (active, AGPL-3.0, refuses HDE's plugins), every hook it shares with FLvibed, "FLUF in play" for the target build nightly-2026-10-16 (its UI, hook sites, FLvibed's awareness layer, the in-game result in both orders, known issues) | 3c9c1390 (6 Nov 2026, short form: 29 watched-path commits: the module split P0-P11e (moves, comments, map rows) (flufaware to core\ui, c1d8288c); 88b009c9 a FLUF window counts as a window for the frame; no new shared address; overlap table and verdict unchanged) | current |
| `starport.md` | The Starport, Codeberg/GitHub orgs, KnowledgeBase, adoxa, other decomps: sources not on hand; **section 10: HDE milestones, decomp states and patch tools from their public sources (30 Oct 2026)** | 3c9c1390 (6 Nov 2026, short form: 41 watched-path commits: the module split P0-P11e (moves, comments, map rows) and FREELANCER_RE / kb rows from the game; nothing on the public facts of section 10, the sources or the decomp states; no effect) | current |
| `speedrun.md` | speedrun.com records, routes, money gates, skips vs the autopilot; **the top-3 recordings analysed frame by frame** (legs, sections vs our legs, fights, docks, money, loadouts) | 3c9c1390 (6 Nov 2026, short form: 122 watched-path commits: the module split P0-P11e (moves, comments, map rows), M02 spawn_charge parked and ash_ride BAD, the engine-kill facts, lock_trap, the leg proof runs; SPLITS 5 Nov 21:23: PB total 12,034 (+1,987), sum of best 11,258 (+1,211); this note's tables not re-based) | current |
| `hde-upstream.md` | Switching the test install to the upstream HDE + FLSharp 0.9.2: what differs, what it costs, when (recommendation: second install now, primary at v0.8; 30 Oct 2026: the owner deferred the second install) | 3c9c1390 (6 Nov 2026, short form: 23 watched-path commits: the module split P0-P11e (moves, comments, map rows) and docs; 40fe028e MOD_COMPATIBILITY reads against the code; 228 / 228 on the upstream exe; second install still not built (M7); decision unchanged) | current |
| `controller_ui.md` | The controller UI round (25 Oct 2026): flight presets round 2 and cross-genre reflexes, device switching the Ubisoft way, glyph families and pad detection, pad cursors and dense screens, assist presentation, speedrun modes, LiveSplit; feeds `docs\UI_PLAN.md` section 6; **29 Oct 2026: presets, rebinding, measurable HUD rules** (U33-U37) | 3c9c1390 (6 Nov 2026, short form: 43 watched-path commits: the module split P0-P11e (moves, comments, map rows) and the controller.cpp split; the default binds changed (Y tap next group / Y hold cruise, A one item / X all, RS nearest enemy first), HUD 2 the default everywhere since 0.2.0, the Steam glyph family; marked in place) | current |
| `options_menus.md` | How well-regarded games do options menus (presets, explainers, previews, assists; GAG / XAG) and Freelancer's own Options screen measured; **the simple mode round of 6 Nov 2026** (three readers, three verifiers: settings screens, standards, what players change); feeds `docs\UI_PLAN.md` and `docs\UI_OPTIONS.md` (U57-U63) | 3c9c1390 (6 Nov 2026, short form: 3 watched-path commits: ccf824e6 the spec lists only binds that exist, ddb71c86 [Modules] reads "on" like every row, c4652165 pad fixes; U57-U63 as 4cab3e0f (the options rulings, landed during the round) left them) | current |
| `npc-ai.md` | Hostile NPCs: how they decide, the stall hunt (GitHub issue 3, the Donau fight), the scene toughness test, levers and rules for a dynamic difficulty (scope open) | 3c9c1390 (6 Nov 2026, short form: 34 watched-path commits: the module split P0-P11e (moves, comments, map rows) (the NPC module, 68dc8f32), 07ac7595 the recorder; no real stall recorded yet, issue 3 open; D2-D4 still the owner's) | current |
| `llm-npcs.md` | LLM NPCs and AI assistants in games (SkyrimNet, Mantella, CHIM, ImmersiveAI, COVAS:NEXT, ATC products...), and what the same would take in Freelancer (far later) | 3c9c1390 (6 Nov 2026, short form: 26 watched-path commits: the module split P0-P11e (moves, comments, map rows) and subtitle / comm fixes (57d6814a the Neural Net queue steps under HUD 2, a9235830 the comm feed through in-flight cutscenes, 5e37b14c a trace removed); N-rows stay later; no effect) | current |
| `ghidra.md` | Ghidra 12.1.4 + the GhidraMCP 7.0.0 bridge: the 18 game binaries decompiled on demand (set-up, the vtable gap, the benchmark on three open questions) | 3c9c1390 (6 Nov 2026, short form: 33 watched-path commits: the module split P0-P11e (moves, comments, map rows); flexe.h 228, FREELANCER_RE grew, so the seed is behind both (`ghidra.ps1 -Seed` not run: outside this round); tools/ghidra README paths only; the benchmark claims stand) | current |
| `test-efficiency.md` | Testing the speedrun autopilot for the least time: a batched sequential verdict (GSPRT), tuning under noise, fixed-dt game-time acceleration and common random numbers, offline route ranking (T1-T6) | 3c9c1390 (6 Nov 2026, short form: 22 watched-path commits: the module split P0-P11e (moves, comments, map rows); 2 lines each in splits.py / apdiff.py (paths); T1-T6 and the reach rule unchanged; no effect) | current |
| `controller-input-haptics.md` | Beyond XInput: SDL3 statically linked, Windows.Gaming.Input / GameInput, DualSense reports, trigger effects and waveform haptics, AAA rumble design and a haptics engine (U29-U32) | 3c9c1390 (6 Nov 2026, short form: 14 watched-path commits: the module split P0-P11e (moves, comments, map rows), the controller.cpp split; c4652165 device-switch fixes; `pad_backend` ships as a player row (README: not tested with real PlayStation or Switch pads); U30-U32 unchanged) | current |
| `haptics-best-in-class.md` | Haptics for 0.4.0: the best games effect by effect, what each pad and link can do on Windows, the craft, and Freelancer's effects by tier with one contextual engine; builds on `pad-native-features.md` and `ps-pad-bluetooth-mode.md` and corrects them (section 8); gaps at its end | f00671ca (6 Nov 2026, new) | current |
| `autopilot-planning.md` | The general autopilot: HTN / GOAP / utility / teleo-reactive in shipped games, re-orientation at every activation, natural-language orders to a validated goal schema, the game's goal vocabulary, toggle fuzz (A37-A43) | 3c9c1390 (6 Nov 2026, short form: 49 watched-path commits: the module split P0-P11e (moves, comments, map rows) and the autopilot_objective / combat splits; lock_trap d3814fd1 (default 0, not flown), the direct fallback (fx3 3 / 3), 37639266 stop / new-game fixes; the 31 Oct wait_for_trent inference refuted (marked in place); situation_arbiter still 0) | current |
| `test-automation.md` | Automating tests at the lowest token cost: a focus-graph crawler, layout lint and golden dumps, real-input tiers, attended check cards, token practices, test impact, ratchets; two real-pad path gaps in the code (T7-T13, U38) | 3c9c1390 (6 Nov 2026, short form: 47 watched-path commits: the module split P0-P11e (moves, comments, map rows); new: fingerprint.ps1, sweep modules-matrix, test_lint, path_check, win7_check against real export tables; the lint at 0 FAIL; T7, T9-T12 unchanged) | current |
| `hud-presets.md` | HUD 2 overhaul and layout presets: what shows in what context, at what size, in what flow; the element x context x preset matrix, budgets, dynamic rules, owner questions (U39-U41) | 3c9c1390 (6 Nov 2026, short form: 50 watched-path commits: the module split P0-P11e (moves, comments, map rows), the hud2.cpp split; Q7 steps 1-6 built: U39 built in part (plumbing, Focus; Speedrun and Guided not), lint 0 FAIL in 6 contexts, the layout rounds 5.16-5.23) | current |
| `model-policy.md` | Which model, effort and orchestration per task, measured from this project's transcripts (context costs most; fast mode; inherited models) and Anthropic's guidance; the routing table and triggers (T23-T27, T35) | 3c9c1390 (6 Nov 2026, short form: 17 watched-path commits: the module split P0-P11e (moves, comments, map rows); 5251ff09 policy.json takes modules\ (T25); the table unchanged; T23, T26 open; `../.claude/settings.local.json` outside the repository, not checked) | current |
| `afk-autonomy.md` | AFK runs: failure modes from the history and the mechanisms against drift, regression and stalls; the progress verdict's blind spots, the queue, review-due, judge-immutable (T28-T35) | 3c9c1390 (6 Nov 2026, short form: 29 watched-path commits: the module split P0-P11e (moves, comments, map rows); 64ad2dd5 launch-check counts own launches only; 45344164 fs-root-search, faed4bba heredoc-script-backslash, ca7fd127 test_lint, d0e79b14 path_check; rollover still suspended; T28 rest, T32-T34 open) | current |
| `claude-harness.md` | Ten outside Claude Code skill / agent repositories (superpowers, anthropics/skills, agent-skills, ...): none installed, about fifteen patterns as hooks, checks and agent criteria (T14-T22) | 3c9c1390 (6 Nov 2026, short form: 22 watched-path commits: the module split P0-P11e (moves, comments, map rows); 5251ff09 guards and routing see modules\; guards fs-root-search, heredoc-script-backslash, the HDE = vanilla edit check (ea92edc3); T16 by hand again, T20 in part) | current |
| `gap_decomposition.md` | The gap to the runners by kind of segment (`tools\gapreport.py decompose`): fights 48 % of +2,518 s, jumps 25 %, lanes 12 %; per kind the runners' way, ours (logs of gate s7arb3) and one mechanism: the objective kill cycle (fight phases measured: kill speed ~35 %, placement / exits ~37 %), enter at speed (tested and parked: the hole's occupant decides, the "91 s a gate" counted occupancy), trigger-route planning (the LOOK plan) | 3c9c1390 (6 Nov 2026, short form: 1 watched-path commit: 01ad1f4d (paths only); no figure changed; `gapreport.py decompose` still not re-run) | current |
| `hud-skin.md` | HUD 2 as a complete skin: total-conversion UI skins, map screens in space games, menu / dialog / loading / handheld standards, the same screens in Freelancer's mods; four readers, four verifiers; feeds `modules\hud\docs\UI_HUD2_SKIN.md` (U42-U46) | 3c9c1390 (6 Nov 2026, short form: 7 watched-path commits: b6f3be60 and 0fdfdc9f the design, 8d8243b3 and 5eac39f7 the probes in the design, 3c9c1390 the rulings and build order, 10998491 and 8292a7c5 fixes on the hidden HUD; U42-U46 stay adopted, nothing of the skin built) | current |
| `hud-critique.md` | The HUD 2 overhaul's critique round: Freelancer's way, modern AAA, FLvibed's flavour, attacks and verifiers; points to `modules\hud\docs\UI_HUD2_CRITIQUE.md` (U49-U56) | 3c9c1390 (6 Nov 2026, short form: 1 watched-path commit, landed during the round: 3c9c1390, the owner's rulings on D1, D2, D4-D7 (UI_HUD2_CRITIQUE.md 7); U47, U48, U50, U52 updated; no overhaul code yet) | current |
| `bar-critical-feedback.md` | Failing bars: a breathing glow that speeds up towards zero, and a bloom-out at zero: games, rate-coded urgency, flash limits, UI timing; drawn on `research\evidence\hud2_gate\decisions\Target3_bars.*` (U69) | 44a1b63b (6 Nov 2026, new) | current |
| `map-screens-aaa.md` | Map screens in AAA games: the controls of 15 games, patterns, the scheme of `docs\MAP_PLAN.md`; no frames or proportions | 3f750496 (7 Nov 2026, new) | current |
| `map-screens-rebuild.md` | The whole map: screen share, what reads modern or archaic, control gaps, prompts that teach, transitions, every function mapped to Freelancer, a four-phase rebuild proposal; frames `research\evidence\map_visual\` (U70-U75) | 3f750496 (7 Nov 2026, new) | current |
| `target-info-display.md` | Target information in the scene: how games show a target, labels on moving things, plates against outlines, what the game's indicator code lets go; forms A to D, the owner's decisions; the concept sheets `research\evidence\hud2_gate\decisions\Target_*.jpg` (U64-U68) | 34474db9 (6 Nov 2026, new) | current |
| `pad-face-colours.md` | The face buttons' shades: each family's latest coloured generation, measured from named photos, what was raised for the plate | 989e6174 (6 Nov 2026, new) | current |
| `subtitles_overlap.md` | Two or more comm speakers at once: guidelines (XAG 104, Netflix, BBC, GAG, Hamilton, Hattner), Unreal's subtitle priority, games, Freelancer's own comm priorities; the default "Focused" (one speaker, priority classes) vs the "Expanded" stack (S10) | 3c9c1390 (6 Nov 2026, short form: 8 watched-path commits: the module split P0-P11e (moves, comments, map rows); 72d95a04 `stack_fit` (stacks shrink together), a46bef03 `chat_size`; S10 decided by the owner 30 Oct (WORK_QUEUE D5), not built) | current |
| `neuronet-screens.md` | NeuroNet screens: how modern games build log, inventory, status, info, chat / group and the tab hub (37 pages, online) | 7 Nov 2026 | done; used by `docs\NEURONET_PLAN.md` |
| `neuronet-windows-facts.md` | NeuroNet windows: what each really is in the game (trees, calls, addresses; read-only research) | 7 Nov 2026 | done; used by `docs\NEURONET_PLAN.md` |

## Runner studies (`runners\`, no template header: working studies written by the speedrun-autopilot sessions)

| File | What |
|---|---|
3c9c1390 (6 Nov 2026, short form: 1 watched-path commit: 01ad1f4d (paths only in `runners\M13.md`); history, no effect) | M13 (Into the Dyson Sphere): what the top-3 runs do segment by segment, one runner shape for us (30 Oct 2026); no template header |
| `runners\M13.md` | M13 runner study per checkpoint segment; links the four notes below |
| `runners\M02.md` | M02 (The Unity, Cosmo, Ashcroft): the runners vs the pod (+93 vs par 22); ASH-START (chase_kill / spawn_stage) result |
| `runners\M02_ashcroft_consensus.md` | M02 Ashcroft, escalation step 1 of AFK_PLAYBOOK 13a: the autopilot's view (spawn geometry, the frozen 1.4-1.5 km cruise gap, King's 3 km leash) and the proposed `spawn_charge` (31 Oct 2026, f8e1dd31) |
| `runners\M02_ashcroft_datapass.md` | M02 Ashcroft, escalation step 2: the second data pass (14177992; two lines withdrawn by the owner, 7851745c); feeds `ash_ride` |
| `runners\M13_bsdown_consensus.md` | Battleship (battleship -> bsdown), escalation step 1 of AFK_PLAYBOOK 13a: the runner-view + autopilot-view consensus |
| `runners\M13_bsdown_datapass.md` | bsdown, escalation step 2: the second data pass on the recordings |
| `runners\M13_gate_allykill.md` | The gate battleship: the `ally_kill` design (runner R2 does not fight it) |
| `runners\M13_city_hold.md` | The city hold (generators, city deaths): the stage-1 study |

Status values: **current** (checked against the commit shown; nothing contradicts it), **partly stale** (some
conclusions overtaken; the note says which), **superseded** (kept for history; see the newer note), **retired**.

## The check-up round (every research session ends with it; every project update triggers the short form)

1. `tools\research_check.ps1`: HEAD, dirty paths, and per note the commits since its check that touch its watched
   paths (declared in the note header as `watch:`).
2. For each touched note: re-read the "What it means for FLvibed" section against the commits; fix statuses in
   `IDEAS.md` (done / superseded / still open), update the note's header (`checked against`, `dirty paths`), and
   mark contradicted findings in place rather than deleting them. The header and this index keep **only the latest
   check**: the entry it replaces goes, verbatim, to the top of the note's section in `CHECKUP_HISTORY.md` (never a
   "; before: ..." chain in the header - by 31 Oct 2026 those chains were ~100 KB, re-read at every check-up).
3. New research: add or extend the topic note (never a per-session file), merge its ideas into `IDEAS.md`
   (deduplicate: one idea, several sources), update this index.
4. Short form after a project update (a working session that lands a change in a watched area): steps 1-2 only, for
   the notes the tool lists. The working session may do it itself or leave a line in `IDEAS.md` for the next research
   session.
5. Record the round at the top of `IDEAS.md` ("Last check-up: <date>, HEAD <commit>, dirty: <paths or none>").

## Note header template

```
# <Topic>
Status: current | partly stale | superseded
Checked against: FLvibed <commit> (<date>); HDE 0.7.1 test install; HDE upstream <commit>
Dirty paths at check: <list or none>
Watch: <paths or globs whose change should trigger a re-check; `path#regex` counts a commit only when its diff of that path adds or removes a line matching the regex (extended, any case; no commas) - for shared files such as docs/SETTINGS.md>
Evidence: research\evidence\<topic>\ (<what>)
Sources: <where the findings come from; provenance tags [file] [page] [api] [assumed]>

## What it means for FLvibed
<the living part: a few paragraphs, each pointing at IDEAS.md ids>

## Findings
<the full material, dated, never rewritten to hide what was learned>
```
