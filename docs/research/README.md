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
   write only `docs\research\` and `research\evidence\`; they never touch `HANDOFF.txt`, code, `CLAUDE.md` (beyond
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
| `freelancer-decomp.md` | FreelancerDecomp/Freelancer-Decomp: full C++ rebuild of the 1.1 client; symbol cross-reference | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 4 watched-path commits: bd5d7af6 + 44659916 (locked-transit back-off, Action::Jump), 4a4bd7b1 strafe_pass (strafe.h pure), f8e1dd31 (FREELANCER_RE: engine kill at full cruise holds 300 -> 287 m/s, HDE, measured from a motion trace); no symbol, flexe.h or decomp cross-reference touched; no effect) | current |
| `flsharp.md` | BC46/FLSharp: the HDE plugin that moved from 0.8 (test install) to 0.9.2 (upstream HDE) | 4338909 (31 Oct 2026 ~05:10, short form: 1 watched-path commit: the audit robustness bundle 55d42263 (PatchVtable / PatchImport fail closed on a non-writable slot, the D3D Reset / Present / CreateDevice re-patch keeps the previous pointer on a refusal, room-character hook order, subtitle loaders parse-then-swap); `core/flexe.h` unchanged since 5eb7bcb; no clash; no effect) | current |
| `discovery.md` | Discovery Freelancer 5.3: patched 1.1 exe, FLUF, DSAce; signature regression | 4338909 (31 Oct 2026 ~05:10, short form: 1 watched-path commit: the audit robustness bundle 55d42263 (PatchVtable / PatchImport fail closed on a non-writable slot, the D3D Reset / Present / CreateDevice re-patch keeps the previous pointer on a refusal, room-character hook order, subtitle loaders parse-then-swap); `core/flexe.h` unchanged since 5eb7bcb; no new hook site; no effect) | current |
| `crossfire.md` | Crossfire 2.0.1: launcher-run `EXE2`, anticheat, one-script campaign | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 3 watched-path commits (autopilot_*.cpp): bd5d7af6 + 44659916 the locked-transit back-off (keyed on the game's own route answer, no mission literal), 4a4bd7b1 strafe_pass (hull geometry by ray tests, default 0, parked BAD); no Crossfire-specific content; no effect) | current |
| `fluf.md` | FLUF (Freelancer Universal Framework, The Starport): status (active, AGPL-3.0, refuses HDE's plugins), every hook it shares with FLvibed, "FLUF in play" for the target build nightly-2026-10-16 (its UI, hook sites, FLvibed's awareness layer, the in-game result in both orders, known issues) | 4338909 (31 Oct 2026 ~05:10, short form: 1 watched-path commit: the audit robustness bundle 55d42263 (PatchVtable / PatchImport fail closed on a non-writable slot, the D3D Reset / Present / CreateDevice re-patch keeps the previous pointer on a refusal, room-character hook order, subtitle loaders parse-then-swap); `core/flexe.h` unchanged since 5eb7bcb; the D3D8 Reset / CreateDevice row now fails closed on a refused patch too; overlap table and verdict unchanged) | current |
| `starport.md` | The Starport, Codeberg/GitHub orgs, KnowledgeBase, adoxa, other decomps: sources not on hand; **section 10: facts mined from the HDE Discord (30 Oct 2026)** | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 1 watched-path commit (f8e1dd31: FREELANCER_RE's engine-kill-at-cruise measurement, HDE); nothing on the HDE Discord facts of section 10, the sources or the decomp states; no effect) | current |
| `speedrun.md` | speedrun.com records, routes, money gates, skips vs the autopilot; **the top-3 recordings analysed frame by frame** (legs, sections vs our legs, fights, docks, money, loadouts) | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 11 watched-path commits (14:27-15:18): strafe_pass flown (sp1) BAD and parked; ab13 / ally_kill and chase_kill + spawn_stage run-judged BAD; the locked-gate loop fixes bd5d7af6 / 44659916 (fx2 i3 still STALLs, cause inferred); AUTOPILOT.md logs split to AUTOPILOT_LOG.md; the HUD 2 lint; SPLITS 15:10: M13 PB 744 (+58), sum of best 692 (+6, was 694); this note's own tables not re-based) | current |
| `hde-upstream.md` | Switching the test install to the upstream HDE + FLSharp 0.9.2: what differs, what it costs, when (recommendation: second install now, primary at v0.8; 30 Oct 2026: the owner deferred the second install) | fc379a3 (31 Oct 2026 ~04:00, short form: 6 watched-path commits (all `docs/TESTING.md`: the AFK_PLAYBOOK pointer, the segment-DNF rule, `tools\reach.py` / reach-short); no HDE, FLSharp, install or signature content (grep of the diff); decision unchanged) | current |
| `controller_ui.md` | The controller UI round (25 Oct 2026): flight presets round 2 and cross-genre reflexes, device switching the Ubisoft way, glyph families and pad detection, pad cursors and dense screens, assist presentation, speedrun modes, LiveSplit; feeds `docs\UI_PLAN.md` section 6; **29 Oct 2026: presets, rebinding, measurable HUD rules** (U33-U37) | fc379a3 (31 Oct 2026 ~04:00, short form: 2 watched-path commits: a4571892 `controller.cpp` +3 lines (autopilot::Stop on a player death), 0b04d363 AUTOPILOT_UX.md names `repair_hull_at` design-only (the unread key was deleted, 1d6f6da); no pad / UI / HUD change; no effect) | current |
| `options_menus.md` | How well-regarded games do options menus (presets, explainers, previews, assists; GAG / XAG 104) and Freelancer's own Options screen measured; feeds `docs\UI_PLAN.md` | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 1 watched-path commit: 4a4bd7b1 (docs/SETTINGS.md gains strafe_pass, Hidden + Meta; 269 keys); hidden keys are not on the options page; no effect) | current |
| `npc-ai.md` | Hostile NPCs: how they decide, the stall hunt (GitHub issue 3, the Donau fight), the scene toughness test, levers and rules for a dynamic difficulty (scope open) | f2cede02 (4 Nov 2026) | current |
| `llm-npcs.md` | LLM NPCs and AI assistants in games (SkyrimNet, Mantella, CHIM, ImmersiveAI, COVAS:NEXT, ATC products...), and what the same would take in Freelancer (far later) | 4338909 (31 Oct 2026 ~05:10, short form: 1 watched-path commit (55d42263, hook / subtitle-loader robustness); no comm / dialog / NPC-speech content; no effect) | current |
| `ghidra.md` | Ghidra 12.1.4 + the GhidraMCP 7.0.0 bridge: the 18 game binaries decompiled on demand (set-up, the vtable gap, the benchmark on three open questions) | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 1 watched-path commit: f8e1dd31 (FREELANCER_RE: the engine-kill-at-cruise coast, measured from a motion trace, not decompiled); `core/flexe.h` unchanged since 5eb7bcb; the benchmark claims stand) | current |
| `test-efficiency.md` | Testing the speedrun autopilot for the least time: a batched sequential verdict (GSPRT), tuning under noise, fixed-dt game-time acceleration and common random numbers, offline route ranking (T1-T6) | 4338909 (31 Oct 2026 ~05:10, short form: 1 watched-path commit (55d42263, hook / loader robustness and two stage-1 log lines); T1-T6 and the reach rule unchanged; no effect) | current |
| `controller-input-haptics.md` | Beyond XInput: SDL3 statically linked, Windows.Gaming.Input / GameInput, DualSense reports, trigger effects and waveform haptics, AAA rumble design and a haptics engine (U29-U32) | fc379a3 (31 Oct 2026 ~04:00, short form: 1 watched-path commit: a4571892 `controller.cpp` +3 lines (autopilot::Stop on a player death); no gamepad / glyph / haptics change; no effect) | current |
| `autopilot-planning.md` | The general autopilot: HTN / GOAP / utility / teleo-reactive in shipped games, re-orientation at every activation, natural-language orders to a validated goal schema, the game's goal vocabulary, toggle fuzz (A37-A43) | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 4 watched-path commits: bd5d7af6 + 44659916 the locked-transit back-off (a Dock / Jump trip the game cannot route rests 4 s x 15, then 10 s; fx2: the back-off fires, i3 still STALLs - cause inferred, not verified: m13.ini `wait_for_trent` locks the gate after check_cap's unlock), a live case for A37 / A38's 'the game's locks' and A39's live trigger table; 4a4bd7b1 strafe_pass (flown sp1 BAD, parked, 4ca9eb64); f8e1dd31 FREELANCER_RE: an engine kill at full cruise holds the speed (A36 answered); A37 / A40-A42 unchanged) | current |
| `test-automation.md` | Automating tests at the lowest token cost: a focus-graph crawler, layout lint and golden dumps, real-input tiers, attended check cards, token practices, test impact, ratchets; two real-pad path gaps in the code (T7-T13, U38) | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 3 watched-path commits: 32c80fce (T8's layout lint built in part: `hud2_lint.h` + debug `hud2 lint`, 79 unit checks, the first approved dump `docs\hud2_lint_baseline.txt`; no pseudo-localisation, no SSIM goldens, contrast n/a), dff3fac2 (guard, 141/141), 5dcf6195 (routing test, 195/195); T7 / T9-T13 and the real-pad path gaps unchanged) | current |
| `hud-presets.md` | HUD 2 overhaul and layout presets: what shows in what context, at what size, in what flow; the element x context x preset matrix, budgets, dynamic rules, owner questions (U39-U41) | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 1 watched-path commit: 32c80fce (PRESETS 6 item 3, the layout lint, built; live baseline `docs\hud2_lint_baseline.txt` FAIL in every context on today's HUD; zones from the doc's rules - step 1's preset data not in UI_SPEC.json yet; no preset built); U39-U41 unchanged) | current |
| `model-policy.md` | Which model, effort and orchestration per task, measured from this project's transcripts (context costs most; fast mode; inherited models) and Anthropic's guidance; the routing table and triggers (T23-T27, T35) | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 2 watched-path commits: 5dcf6195 (routing: a docs move / split / trim - verbatim, session logs, a size ratchet - routes to bulk-doc-writing, Sonnet medium; the AUTOPILOT.md ratchet brief had been classed cpp-design-debug by its 'autopilot' / 'build' words; 195/195) - a matcher gap found in use, T25 extended; dff3fac2 (guard, not routing); `../.claude/settings.local.json` outside the repository, not checked) | current |
| `afk-autonomy.md` | AFK runs: failure modes from the history and the mechanisms against drift, regression and stalls; the progress verdict's blind spots, the queue, review-due, judge-immutable (T28-T35) | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 2 watched-path commits: dff3fac2 (guard set-no-progress: a flset part run (-StopAt) or a set from a named checkpoint save is no attempt the verdict counts - 25 overrides of the rule in 24 h, drift_check "overrides"; a save in a variable stays counted; 141/141) - the T28 override cause fixed at the guard, the override cap itself still open; 5dcf6195 (routing: a docs move / split / trim -> bulk-doc-writing; handoff RESUME block); no conclusion changed) | current |
| `claude-harness.md` | Ten outside Claude Code skill / agent repositories (superpowers, anthropics/skills, agent-skills, ...): none installed, about fifteen patterns as hooks, checks and agent criteria (T14-T22) | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 2 watched-path commits: dff3fac2 (the set-no-progress guard narrowed after 25 overrides in 24 h: part runs and named-save probes exempt, fail closed on a save in a variable; 3 cases, 141/141) - the note's 'a guard that fired wrongly is narrowed' pattern; 5dcf6195 (routing rule + test, 195/195); T16 / T18 / T21 unchanged; no effect) | current |
| `gap_decomposition.md` | The gap to the runners by kind of segment (`tools\gapreport.py decompose`): fights 48 % of +2,518 s, jumps 25 %, lanes 12 %; per kind the runners' way, ours (logs of gate s7arb3) and one mechanism: the objective kill cycle (fight phases measured: kill speed ~35 %, placement / exits ~37 %), enter at speed (tested and parked: the hole's occupant decides, the "91 s a gate" counted occupancy), trigger-route planning (the LOOK plan) | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 1 watched-path commit (06c44292: AUTOPILOT.md logs moved to AUTOPILOT_LOG.md, LEG_DOSSIER.md pointer repointed; probe sp1's legs.tsv rows); no figure changed; the kind decomposition still awaits `gapreport.py decompose`; the stale M13 row unchanged (SPLITS 15:10: M13 PB 744, sum of best 692)) | current |
| `subtitles_overlap.md` | Two or more comm speakers at once: guidelines (XAG 104, Netflix, BBC, GAG, Hamilton, Hattner), Unreal's subtitle priority, games, Freelancer's own comm priorities; the default "Focused" (one speaker, priority classes) vs the "Expanded" stack (S10) | 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 1 watched-path commit (32c80fce: UI_HUD2_PRESETS.md status line + 6 item 3, the HUD 2 layout lint; no subtitle, speaker or overlap rule); no effect) | current |

## Runner studies (`runners\`, no template header: working studies written by the speedrun-autopilot sessions)

| File | What |
|---|---|
| `m13_runner_study.md` | M13 (Into the Dyson Sphere): what the top-3 runs do segment by segment, one runner shape for us (30 Oct 2026); no template header |
| `runners\M13.md` | M13 runner study per checkpoint segment; links the four notes below |
| `runners\M02.md` | M02 (The Unity, Cosmo, Ashcroft): the runners vs the pod (+93 vs par 22); ASH-START (chase_kill / spawn_stage) result |
| `runners\M02_ashcroft_consensus.md` | M02 Ashcroft, escalation step 1 of AFK_PLAYBOOK 13a: the autopilot's view (spawn geometry, the frozen 1.4-1.5 km cruise gap, King's 3 km leash) and the proposed `spawn_charge` (31 Oct 2026, f8e1dd31) |
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
