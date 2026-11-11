# The Claude Code harness: outside skill / agent repositories and what transfers
Status: current
Checked against: FLvibed 3c9c1390 (6 Nov 2026, short form: 22 watched-path commits: the module split P0-P11e (moves, comments, map rows); 5251ff09 guards and routing see modules\; guards fs-root-search, heredoc-script-backslash, the HDE = vanilla edit check (ea92edc3); T16 by hand again, T20 in part)
Dirty paths at check: another session's uncommitted the private work queue, `tools/uimock/options.py` (not judged); 950b25cc, 4a270d9e and 3c9c1390 landed during the round, read
Watch: tools/claude/**, tools/drift_check.ps1, tools/docrefs.py, tools/docindex.py
Evidence: none saved (the repositories read through the GitHub API only - nothing cloned, installed or run). The round's
summary for the owner: `..\SKILLS_RESEARCH_2026-10-29.md` (environment folder, not versioned)
Sources: five research agents (two repositories each) and the coordinating session's checks of their claims against the
repositories and the private Claude Code harness. Tags: [repo] a file in the outside repository, [file] FLvibed, [assumed]

## What it means for FLvibed

*6 Nov 2026 (3c9c1390, short form):* the pattern holds: three more repeats became checks (45344164, faed4bba, ca7fd127 from seven launches lost in P11c), and dead-path checks arrived for scripts and Watch lines (d0e79b14, 64ad2dd5: T20 in part). A refactor can silently blind a guard: `docs-writeback` keyed on the old folders and passed every commit in `modules\` until 5251ff09. The miner (T16), T15, T17 and T22 are still not built.

*31 Oct 2026 ~04:00 (fc379a3, short form):* the audit's pattern continues: a recommendation that stayed prose is turned into a guard with a fixture, and a guard that fired wrongly is narrowed (ac601f2 the fire log keeps 2,000 characters of the command; eb9e1191 deploy-front ignores a statement that only reads deploy.ps1; fe771ef two leaks of the own-launch rule). The 'prose does not hold, hooks do' conclusion is the one these commits confirm; no repository finding of this note is contradicted. the private Claude Code harness notes is held under its size ratchet (22,355 B) by moving the replay column to the section ff89026 added here.

*29 Oct 2026 (04b4c05, new):* ten popular repositories were read - obra/superpowers, mattpocock/skills,
multica-ai/andrej-karpathy-skills, anthropics/skills, DietrichGebert/ponytail, nextlevelbuilder/ui-ux-pro-max-skill,
safishamsi/graphify (now Graphify-Labs), JuliusBrussee/caveman, addyosmani/agent-skills, Leonxlnx/taste-skill (105k-308k
stars each by the API; popularity is no evidence). **None is worth installing**: prose-first, web-first, or a context
cost on every turn. About fifteen patterns transfer, nearly all as hooks, checks or agent-criteria edits - consistent
with the harness's own finding that hooks hold and prose rules do not (the private Claude Code harness notes). Verified gaps in the
harness: no destructive-git rule in `guard.js`; no SubagentStart hook (SessionStart context never reaches subagents);
`edit-check.js` checks only HANDOFF.txt's size, no skill size and no mojibake; `game-facts.md` has no "grep, then read by
offset" line though `FREELANCER_RE.md` is 237 KB. The ideas: T14 (cheap gates), T15-T22 (below). The 29 Oct verdict on
knowledge-graph MCPs stands (Graphify).
*30 Oct 2026 (12e7942, short form):* of the verified gaps above only the SubagentStart hook is still open (T14);
8f32140 built the rest and T18 / T19; 09fe148 added the routing hooks (`route-agent` / `route-prompt` / `route-workflow`).
*30 Oct 2026 (aafe41d, short form):* no pattern here changed status. Repeats keep being turned into rules by hand
(T16's job): 351cd96 the -File / -Command array pitfall into the PowerShell rule (two dead detached launches),
0d278f1 a guard overridden 7x in 24 h now checks its own safe case. The afk-run skill gained the Rollover (a successor
scheduled task; `afk-autonomy.md`). T14's SubagentStart hook is still open *(done 30 Oct 2026, next line)*.
*30 Oct 2026 ~11:50 (16df8bd, short form):* **T14 complete** (fa8fc40): `hooks\subagent-start.js` gives every subagent
but FLvibed's own game-facts / run-judge / docs-auditor a rules block as additionalContext (727 bytes, 849 after
322ce9f; never blocks: 1 s stdin fallback, errors = no output; 7 cases). **T16's retro ran once by hand with the
miner** (322ce9f, WORK_QUEUE H6): `GUARD_MINE` over 120 transcripts found 18 refused `update_plan` calls by subagents ->
one line in those rules (a subagent reports by its hand-back); the extension to re-reads, no-change retries and
reverted edits is not built. Doc size budgets for the always-loaded maps (a125547, T13); `docrefs.py` knows a plan file
names files not built yet (d411231). The afk-run skill's Rollover rewritten for the autonomous handover (9a1d545).
T15, T17, T20-T22 unchanged.

## Findings

### Cheap gates (T14)
- Destructive git - refuse or ask on `reset --hard`, `clean -f`, `checkout .`, `restore .`, `branch -D`, `stash
  drop / clear`, the safe alternative in the reason: patterns from mattpocock
  `skills/misc/git-guardrails-claude-code/scripts/block-dangerous-git.sh` [repo] (its plain substring grep is easy to
  dodge: port the patterns, not the script). `guard.js` has none [file].
- Skill / agent size: refuse a SKILL.md or agent file at 20,000 characters - after a compaction Claude Code re-attaches an
  invoked skill cut to 20K and the tail is lost silently (superpowers `tests/skill-size/test-skill-size.sh`, citing the
  Claude Code issue) [repo]. FLvibed's are 2-4 KB: a guard for later. Optionally a warning for CLAUDE.md over the ~10 KB
  guideline (the root file is 21 KB, controller's 15 KB; the docs diet is the owner's call).
- Mojibake (UTF-8 saved back as Latin-1 / 1252) for .md / .txt / .tsv / .json - PowerShell 5 round trips cause it and the
  file still parses; protects the subtitle TSVs (taste-skill `tests/text-encoding.mjs`) [repo].
- Tagged debug probes `[DEBUG-xxxx]` and a commit refusal while a staged diff holds one (mattpocock
  `skills/engineering/diagnosing-bugs/SKILL.md`) [repo].
- `game-facts.md`: one line - FREELANCER_RE.md is beyond one Read; grep, then read by offset (graphify `graphify/skill.md`
  step B3, its truncated-read coverage check) [repo].
- A SubagentStart hook injecting a block under 1 KB (the principles, the evidence / citation rule, no audio, tools by full
  path) into general-purpose / Explore agents, skipping FLvibed's own three; never block on stdin, a 1 s fallback (ponytail
  `hooks/ponytail-subagent.js`, its Windows issues #443 / #790) [repo].

### Bigger patterns
- **T15 bar-guard** (agent-skills `skills/constraint-driven-development/references/floor-guard.md`) [repo]: on commit,
  scan the diff for quiet bar-lowering - loosened run-judge criteria, TESTING.md tolerances or splits limits; deleted /
  skipped unit tests; removed guard rules; new `#pragma warning(disable`, empty `catch {}`, `-ErrorAction
  SilentlyContinue`, TODO stubs; exit 2 ("could not run") never counts as clean; an exceptions file with owner and date;
  ratchets on per-leg PB / hull.
- **T16 stumble miner + retro** (superpowers `skills/diagnosing-superpowers/prompts/repeated-work.md`, `stumbles.md`;
  mattpocock `skills/engineering/retro/SKILL.md`) [repo]: extend `test_hooks.js` GUARD_MINE (failed shell calls today) with
  repeated reads (3+), edits (2+), "failed, then retried with nothing changed", reverted edits; classify each: mechanical ->
  a guard / edit-check rule, judgement -> a run-judge / docs-auditor criterion, noise -> delete.
- **T17 skill / agent trigger measurement**: cheap first - a replay counting misses (a build with no build-and-test
  call before it, a flexe.h symbol without new-exe-symbol); the "applies even when ..." description lever, measured at
  7/27 -> 21/27 fires with 0 false fires (agent-skills `evals/README.md`, `evals/plugin/*`) [repo]; live evals only when a
  description changes, run outside the project folder so the hooks do not fire (anthropics
  `skills/skill-creator/scripts/run_eval.py` is not Windows-ready: `select` on pipes) [repo].
- **T18 run-judge criteria** in one edit: extract and check the claims in the session's summary, flag non-discriminating
  criteria, the burden of proof on PASS, a hard failure caps the verdict, never pass the judge the expected verdict,
  findings need a concrete case, end with "Not checked:" (anthropics `skills/skill-creator/agents/grader.md`;
  agent-skills `skills/doubt-driven-development/SKILL.md`; taste-skill `references/critique.md`; ponytail
  `skills/ponytail-review/SKILL.md`) [repo].
- **T19 subagent return format**: game-facts / docs-auditor rows `claim — path:line — standing`, a totals line, a row
  cap, "not answered" stated - subagent reports stay in the main context every turn, so that is where shrinking pays
  (caveman `agents/cavecrew-investigator.md`) [repo].
- **T20 docs checks**: `docrefs.py` also resolves backticked `X::Y` / `name()` symbols, unique matches only (graphify
  `graphify/extractors/markdown.py`); a dead-path / dead-tool check over CLAUDE.md, skills, agents, rules (anthropics
  `skills/claude-api/shared/prompt-audit.md`); a trim validator for always-loaded files - headings, code, paths, numbers,
  negations kept (caveman `skills/caveman-compress/scripts/validate.py`) [repo].
- **T21 no-progress escalation**: after N sets on one leg without a better judged verdict, hand the attempt ledger to a
  fresh agent or the owner; require "the one command that goes red on this, already run" (superpowers
  `skills/subagent-driven-development/references/fix-loop.md`; mattpocock `diagnosing-bugs` phase 1) [repo]. The
  `set-no-progress` guard already refuses a set without a new hypothesis [file]; this adds the escalation.
- **T22 a `/release` command**: docs-auditor, the setup test and the Windows 7 / package checks in parallel, merged into
  GO / NO-GO with the rollback named; never a push without the owner's word (agent-skills `.claude/commands/ship.md`) [repo].
- Into UI (U35): a frame-based contrast check - WCAG relative luminance (ui-ux-pro-max `scripts/design_system.py`) and
  the thresholds of its `references/pro-rules.md`; an in-game layout inspector after taste-skill `scripts/inspect.mjs`.
- Into testing (T1 / T5): ponytail `benchmarks/agentic/compare.py` (paired sign test + bootstrap) for cross-leg gates;
  anthropics `skills/claude-api/shared/evals/eval-audit.md` (separate harness failures from real ones, noise floors,
  ablation) - see `test-efficiency.md`.

### Skipped, and why
- Caveman's voice: its own `docs/HONEST-NUMBERS.md` - output 0.3 % of tokens in long sessions, net bill within ±0.2 %;
  terse output costs the owner clear reports. Its proxy / cloud / LLM-compress: a third-party binary, an account, an LLM
  rewrite of rule files.
- Graphify: docs (where FLvibed's facts live) go only through a non-deterministic LLM pass; ~25 Python packages; a nudge
  hook on every Read / Grep. `docindex.py`, `docrefs.py`, game-facts and ghidra cover navigation.
- Karpathy skills: one prose prompt, covered by the four principles and the pre-change checklist.
- UI UX Pro Max's database, taste-skill's library: web / mobile styling; their "HUD / Sci-Fi" style (glow, blinking,
  tickers) runs against the game-consistent look and the < 3 Hz cue rule.
- Ponytail's persona: "smallest change" clashes with Efficacy = widen capability; its codebase map parses no C++ / PS.
- Superpowers' bootstrap (a "use skills" meta-prompt every session), TDD, worktrees, its brainstorming server;
  agent-skills' web checklists; anthropics' webapp-testing (port polling).

## Harness history (moved from the private Claude Code harness notes on 31 Oct 2026, the doc-size ratchet; verbatim)
The afternoon replay (29 Oct 2026, 42,306 shell calls, subagents included; fired / failed then): ps-colon-var 5 / 5,
ps-pipe-statement 16 / 16, cmd-build 1,592 / 175, rm-spaced-path 128 / 84, ps1-from-bash 37 / 0, bare-python 192 / 24,
shell-patch-code 402 / 0, bash-backtick-dq 1 / 0, git-commit-all 455 / 5 (pattern only), git-push 11 / 0, deploy-front
89 / 10, set-no-progress 43 / 0. Fixed the same day from the live fire log: the set rules and build-during-set no
longer fire on a script that is only read (grep / sed / Get-Content, quoted patterns ignored), nor on release / setup
builds (the legs deploy build\dev only).
Pruned after the replay: `start-process-args` (99 fires, 1 real failure).

Benchmark: `game-facts` on 29 Oct 2026 (Sonnet, 44 s, 13 tool calls): the HUD distance basis and M08's real trigger -
both misread in past sessions (CALIBRATION.md #3, #4) - answered right with `path:line` citations; a third question no
source covers came back "not answered" with two probes, no guess.

Not adopted (29 Oct 2026; for the owner):
- clangd / an LSP plugin (needs LLVM and a `compile_commands.json` generator for `build.cmd`): download, owner's OK.
- Crash minidumps + cdb / mcp-windbg (Debugging Tools for Windows: a Microsoft download, owner's OK).
- Image-diff / OpenCV MCPs, knowledge-graph MCPs: the Python frame pipeline and the docs indexes already do more; a
  graph tool would be a second source of truth.

## Guard rules: what the replay showed (moved from tools\claude\README.md 31 Oct 2026, audit R3)
Would have fired / of those failed then, per rule, as the README carried it (29 Oct 2026 replay and later notes).

| Rule | Fired / failed |
|---|---|
| `ps-colon-var` | 3 / 3 |
| `ps-pipe-statement` | 16 / 16 (from the mining, 29 Oct afternoon) |
| `cmd-build` | 1,472 / 145 (49 / 10 since 25 Oct) |
| `rm-spaced-path` | 73 / 57 |
| `ps1-from-bash` | 12 / 0 (harm, not failure) |
| `bash-backtick-dq` | 1 / 0 (bit twice on 29 Oct) |
| `bash-inline-write-backslash` | 18 / 1 since 29 Oct (silent harm) |
| `bare-python` | 104 / 14 |
| `shell-patch-code` | 238 / 0 (rule: Edit / Write) |
| `git-commit-all` | 455 unscoped sweeps in the history (the replay cannot know their trees: an upper bound); two known harms, both multi-area (29 Oct) |
| `destructive-git` | 39 / 0 (27 checkout path discards, 2 clean -f, 4 force pushes in the published clone) |
| `debug-tag` | 0 in the 1,571-commit history (.md only) |
| `git-push` | 12 / 0 |
| `deploy-front` | 2 / 0 |
| `review-due` | replay 29 Oct over 7 AFK sessions: 270 raw over 302 launches / builds, 23 with compliance |
| `stop-the-line` | replay: 1 (24 Oct, replay ATTENTION) |
| `build-during-set` | new |
| `set-dirty-build` | new (79 % of past leg rows were +dirty) |
| `set-no-progress` | the history's biggest sink: 39 h on one leg; replay 184 / 1 with flleg (upper bound) |
| `speedrun-ref` | the owner, 29 Oct (a 12-run M13 probe before the runners' data); replay: 219 fires in 236 transcripts; an override passes its own rule only |
| `no-budget` | new 30 Oct 2026 evening (the compliance audit: budgets written after launches, runs with no `docs\test_budget.tsv` row, overruns Q2b 1 -> 9, Q6s7 2 -> 7) |
| `runner-study` | new 30 Oct 2026 evening (the owner, after saying it several times: analyse the runners thoroughly FIRST, only THEN plan; the one-line `# ref:` did not make it happen - LOOK L2 was planned and probed before the runners' start geometry was read) |
| `no-reach` | new 30 Oct 2026 evening (the Q4 retro: 75 legs before the reach was read; the JRS start past every event the change acts on - both prose until the audit) |
| `reach-short` | new 31 Oct 2026 (audit D1: no-reach never computed the prose; Q4s3's +8 s at ~27 s detectable would pass it) |
| `probe-first` | audit B1 |
| `spec-with-changelog` | 30 Oct (spec 9.5 h behind) |
| `docs-writeback` | audit B4, 31 Oct |
| `retro-row` | audit B5 |
| `retro-docs-auditor` | new 30 Oct 2026 evening (the auditor ran as batches after the commits; 16 nudges passed by) |
| `speedrun-ref-read` | replay: 47 fires in 236 transcripts |

## The documentation re-read after the 0.4.6 sweep (11 Nov 2026 evening; proposals, nothing applied)

The owner asked for it while the sweep ran. Two agents read the pages and compared them with the harness; the lead
added what the sweep itself showed (one lead, about 40 subagent runs, six worktrees: `docs\RELEASE_0.4.6.md`).
Read: Claude Code's sub-agents, hooks guide and reference (first 100K characters), worktrees, context-window, workflows,
agent-teams, best-practices, memory, checkpointing, model-config (in part); the Fable 5.2 prompting page, the prompting
best practices, the migration guide. NOT read: the Claude Code overview page itself, settings / permissions, skills,
plugins, desktop, Remote Control, scheduled tasks, output styles, MCP, headless, costs, the changelog; the effort and
compaction pages of the platform docs. Nothing read contradicts the instruction audit of the same morning except item 3.

| # | Proposal | Rests on | What the sweep showed |
|---|---|---|---|
| 1 | Agent definitions for the spawns that recur: `fix-worker` (the workers' brief as its body, model and effort in the frontmatter, `skills:` preloading build-and-test and new-exe-symbol), `release-verifier` (no Edit / Write), `spec-publisher`. `route-agent.js` fills a missing model or effort and logs an off-policy choice, but no longer refuses one | sub-agents: the model's order is the call's parameter, then the definition's frontmatter, then `CLAUDE_CODE_SUBAGENT_MODEL`; a definition's skills are injected whole | The keyword classifier put a C++ debugging lane on the cheaper model (stopped and respawned); 35 lane prompts written by hand around a scratch file; four spawns refused for naming the owner's own model and effort |
| 2 | A verifier of its own for each lane as soon as its fix is committed, not two passes at the end | best-practices: a fresh reviewer has not seen the reasoning; workflows: a verify stage, fix until a check passes | Three "FIXED + VERIFIED" did not hold in the release build (58, 71, the notice at a load), found hours later |
| 3 | The ownership grant ("this codebase is yours ... fix what you find nearby") leaves the always-loaded CLAUDE.md and goes into the briefs of feature and change packages; the three limits stay | the Fable 5.2 page: a rule that limits scope does not cancel the grant; leave the sentence out where the code is to change as little as possible | Workers of a bug sweep reported unasked changes (the cargo pips, placement side effects). The owner's own rule of 11 Nov: his decision |
| 4 | Worktrees for workers made by one mechanism: a `WorktreeCreate` hook (git worktree add, the `Install` junction, the SDL3 cache, a git-ignored scratch folder inside the worktree) with its `WorktreeRemove`; the guard refuses `git stash` in a linked worktree | worktrees: the hook replaces the default logic; `.worktreeinclude` cannot make a junction | Made by hand; workers overwrote each other's scratch helpers and frames; one used the shared stash, which the power cut then caught mid-write |
| 5 | A `PreCompact` hook naming what the summary must keep (the open lanes, the release doc, the running agents); HANDOFF.txt's "CARRIED" and "OLDER OPEN ITEMS" halves moved to the docs they belong to | hooks reference: PreCompact's stdout becomes instructions for the summary; the Fable page: remove notes no longer needed | The 8,000-character cap refused a commit seven times in one day; the lead trimmed by a few characters each time |
| 6 | `spec-with-changelog` reminds instead of refusing while a release doc lists the spec as open; `handoff-size` stays a refusal | best-practices: a hook is for a rule with zero exceptions; the README's own rule that a guard whose fires are all overrides goes | About twelve refusals, each passed with the escape comment for the same true reason. Check `guard_fires.tsv` first |
| 7 | `CLAUDE_CODE_SUBAGENT_MODEL` as the default model for spawns, so the guard no longer has to enforce it | sub-agents: a default that a definition or the call still overrides | The guard exists to stop spawns inheriting the session's model |
| 8 | A long orchestration keeps the lead's context for decisions: verifiers judge the frames and return verdicts, a fresh lead per round reads the release doc | sub-agents and context-window on many detailed results; the best-practices page on a new context over compaction | The lead passed 400K tokens, much of it frames and reports |
| 9 | One owner for the AFK "never idle" text (the afk-run skill), the hooks pointing at it; `context-warn.js` line 55 lacks the owner's-word exception | the Fable page: "never" is read as absolute; rewrite absolutes with scope and reason | Two commits on 11 Nov edited the same rule in six files |
| 10 | Owner-facing reports name things by what they are, not by the session's codenames; the Fable page's block on writing for a reader without the context, in CLAUDE.md; HANDOFF.txt exempt | the Fable page: no term made up during the work; key points first | The lead's own messages of the sweep ("r4flight", "verifier B") |
| 11 | Measure before changing: the routing table's efforts re-swept on Fable; the guard's rules replayed on Fable transcripts and those that catch nothing pruned; `health.js`'s delegation counts for Fable leads | the Fable page: rerun the effort sweep; narrow stopping conditions that are not needed; review standing delegation lines | The table and about 35 guard rules were tuned on Opus 5.5 |
| 12 | A line on recovery after a crash: resume the lead, then the workers by id; work in progress is committed (now in the workers' brief) | checkpointing: not a replacement for version control, subagents' and shell edits are not tracked | The power cut of 15:03 |

Left alone because the pages recommend what the project does: hooks only for what must always hold and CLAUDE.md as a
short map; the SessionStart re-injection after a compaction; path-scoped rules for C++ and PowerShell; a rewrite
instead of a refusal for a bare `python`; read-only agents with tool limits and the neutral run-judge brief; workers
never editing shared files; word caps on reports; rules that carry the owner's reason and date; the explicit list of
what needs the owner's word; "verified means" and "say what was not checked".
Checked and not a fit: agent teams (experimental, no restore after a resume, changes how named subagents start);
checkpoints as a backup; `.worktreeinclude` alone; `memory:` on agents (the knowledge base does that job); ultracode by
default; `CLAUDE_CODE_SUBAGENT_MODEL_FORCE` (it would override the definitions); importing HANDOFF.txt into CLAUDE.md
(it would ride in every spawn).
