# Model, effort and orchestration policy for the Claude Code work (measured from this project's transcripts + Anthropic's guidance)
Status: current
Checked against: FLvibed 3c9c1390 (6 Nov 2026, short form: 17 watched-path commits: the module split P0-P11e (moves, comments, map rows); 5251ff09 policy.json takes modules\ (T25); the table unchanged; T23, T26 open; `../.claude/settings.local.json` outside the repository, not checked)
Dirty paths at check: another session's uncommitted the private work queue, `tools/uimock/options.py` (not judged); 950b25cc, 4a270d9e and 3c9c1390 landed during the round, read
Watch: tools/claude/**, ../.claude/settings.local.json
Evidence: `research\evidence\model_policy\` - `models_repo.md` (the transcript mining: sessions, tokens, costs, subagents), `models_web.md` (official docs, benchmarks, practitioners; URLs), `models_proposal.md`, `models_verify.json` (the sceptic recomputed the numbers from `agg.json`), `mining\` (the streaming scripts `mine.js` etc. and `agg.json`)
Sources: a research workflow (29 Oct 2026, the owner's request); the numbers are list-price equivalents at Anthropic's per-token prices ("$" below); the verifier's recomputation confirmed them. Tags: [M] measured here, [C] cited, [J] judgement

## What it means for FLvibed

*31 Oct 2026 ~04:00 (fc379a3, short form):* **an AFK driver is Opus 5.5 High, set by the owner's rule** (b195314, 31 Oct, 'MUST start at Opus 5.5 High'; the table's 'Opus 5.5, medium' rows are for attended sessions). Measured that day: `set_session_effort` on another session works; `set_session_model` there was blocked by the auto-mode classifier and `set_remote_control` is refused in a scheduled run (both need the owner's allow rules, which are in since fec472b, and the second still fails unattended), so the handover that depended on them is suspended (4fb61dc; `afk-autonomy.md`). The routing hooks tightened (79a963e, 6d6d1b2) without changing the table. T23-T27 unchanged.

*30 Oct 2026 (aafe41d, check-up):* **T27 is built** (65ccf70, WORK_QUEUE H3): `tools\digest.py` reads legs / sets /
gates into one line per leg with a class (death / failstate / harness / unknown; fc3238e: a mission's own 'Mission
Failed' is `missionfail`), leg folders without a row are UNJUDGED (fails closed) and `--gate` adds the gate's verdicts -
the deterministic form this note recommended over a Haiku log reader. T35 gained the testing method's own trend
(8fbdf5b, `docs\test_budget.tsv`). T26's effort / model sweep is still open (H9), so the [J] rows stand.
*29 Oct 2026 (989d138, new; the owner: a better definition, and reactivity, on which model, which effort and whether
ultracode is used, from the project's history and the best-regarded guidance):*
- **The context costs more than the model** [M]: over 49 main sessions and their subagents ~$6,460 list-equivalent;
  cache reads 65 %, cache writes 21 %, output 14 %; the mean context per main call ~505K tokens, the same before and
  after the 21 Oct audit (fresh starts 62-84K). Keeping the main thread small (subagents for bulky reads, hand-offs at
  package boundaries) saves more than any model switch.
- **The billing is very likely a subscription** [M, medium]: main sessions write the 1-hour cache almost exclusively,
  which Claude Code requests only within plan usage. So the binding budget is the usage limits, "$" is a proxy - except
  **fast mode, which bills usage credits: $1,440 of it, 55 subagents fast because their parent was** [M]. Fast mode off
  unless the owner is watching live (T23).
- **Name a model on every spawn** [M]: 32 spawns without `model` inherited Fable ($377). `run-judge` is `model:
  inherit` (all 4 judge transcripts so far ran on Opus 5.5 at high effort, so no bad case yet); a Fable or fast parent
  would make it ~2x [verifier] (T24).
- **Effort is per spawn too**: the Agent tool takes a per-call `effort` (Claude Code v2.1.292+; precedence
  `CLAUDE_CODE_EFFORT_LEVEL` > per-call > frontmatter > session) [C, verifier]; an effort change keeps the cache on all
  four current models [C]; a model switch or a fast-mode toggle rebuilds it [C].
- **Cache lifetimes**: the main thread holds 1 hour (an AFK tick under an hour costs ~$0.10 of reads at 500K; a gap over
  an hour ~$4 of rewrites) [M + C]; **subagents and workflow agents hold 5 minutes** - a judge or lookup that waits on a
  game for more than 5 minutes rewrites its context (`subagentPromptCacheTtl` / `CLAUDE_CODE_SUBAGENT_PROMPT_CACHE_TTL`
  can raise it) [C].
- **Multi-agent pays on breadth, wastes on chains** [C]: Anthropic's research system ~15x the tokens of chat, token use
  explaining most of the quality; dependent chains (an autopilot leg's set -> judge -> change) do better on one strong
  orchestrator. FLvibed's UI build fan-out cost 2.8x its main sessions [M]. Default: workflows small; medium for research
  and audit rounds; large / ultracode only on the owner's word for a named round (ultracode's keyword does not work from
  cron / scheduled prompts [C]).
- **Not measured yet: quality per model or effort here** - no A/B runs exist; the routing rows below marked [J] become
  [M] only after an effort sweep on FLvibed's own cases (T26). A Haiku log reader is **not** recommended: a missed FAIL
  would feed the judge a false PASS (fails open); a deterministic digest script is the robust form (T27).

## Findings

### The routing table (Claude applies it per task; [J] rows pending T26)
| Task class | Model / effort | Where | Basis |
|---|---|---|---|
| A test run's log / check-in / gate output | a script digest (verdict lines, errors, timings), no model | tool output | [J]: fail closed |
| Judging a leg or run (frames + log, RUNNERS) | Opus 5.5, high | `run-judge` subagent | [M] 4 calls ~$2; misreads cost most |
| Game-fact lookup (kb, FREELANCER_RE, ini) | Sonnet 5.5, medium | `game-facts` | [M] 44 s / 13 calls, both past misreads right (n = 2) |
| Ghidra lookup / one function | Sonnet 5.5, high | `game-facts` via `gq.js` | [M] one run $2.19, answers unverified |
| Ghidra structs / hook-site decisions | Opus 5.5, high; the decision inline | subagent + main | [C] share the design context |
| C++ hook / autopilot design, root-cause debugging | Opus 5.5, high inline; Fable 5.1 high only after the escalation below | main | [C] high +2.5 points for +38 % cost on SWE-bench Pro (saturated for Opus 5.5); Fable medium below Opus medium at 3.2x (one run) |
| Routine scoped C++ fix, a settings key, a test | Opus 5.5, medium | main | [C] |
| Autopilot leg iteration | Opus 5.5, medium + the judge row | main + `run-judge` | [C] dependent chain; the 39 h sink was strategy, not effort [M] |
| UI build tracks | Opus 5.5, medium inline; spec / mock patches Sonnet | main + ≤ 2-3 subagents | [M] fan-out 2.8x |
| Docs audit | Sonnet 5.5, medium | `docs-auditor` | [M] $0.21 / 78 s (n = 1) |
| Research rounds | Opus lead at medium, Sonnet readers at medium | a small / medium workflow | [C] research quality flat across effort (DeepResearch Bench II: low 66 % vs high 65 %) |
| Bulk doc writing (lore, runbooks) | Sonnet 5.5, medium | subagents | [M] quality never measured |
| Subtitle line review | Opus 5.5, medium | main | [J] |
| Commit / handoff / backup chores | the current model, low effort | main | [C] no model switch (cache) |
| AFK keep-alive tick | no model work: check state, end the turn | main | [J] |
| Environment check-ups / sweep audits | Opus high lead, Sonnet readers | a medium workflow; large on the owner's word | [J] |

Machine-readable form and source of truth: the private routing policy (29 Oct 2026, applied by the route hooks,
the private Claude Code harness notes "Routing"). It adds [J] classes this table does not list: status question, release, code review,
subtitle pipeline, HUD design, design critique, in-game test, codebase lookup, synthesis, plus the system classes report
and general. Its docTable maps each class to its row here.

### Triggers (react, do not wait to be told)
- Two failed attempts at the same fix or leg (memory `reconsider_when_stuck`) -> diagnose first [C: Claude Code team,
  7 Jul 2026]: a skipped file / test / source -> raise effort one level; full context and still wrong -> the next model
  up, one attempt, then report and change the route; otherwise fix the prompt, the kb page or the scoping.
- NO PROGRESS -> change the hypothesis or the target, never only the effort.
- Signatures, hooks, hot-reload compatibility touched -> high effort for that turn [C].
- The third mechanical repeat of one shape -> a script, or a Sonnet subagent [J].
- A task that would read > ~10 frames or > ~50K of log -> a subagent (images also push older images out of the
  conversation, rebuilding the cache from there [C]).
- The main context past ~400K -> update the handoff and compact at the next package boundary (a fresh session cannot be
  started from a cron turn, `CHECKUP_2026-10-24.md` ~222) [J + M].
- Research / synthesis never at xhigh / max [C, medium].

### Where each choice is set
Session model: `/model` or the session tools (the owner by default; a switch rebuilds the cache). Session effort:
`/effort` by the owner only - a session never changes its own effort or model [31 Oct 2026: an AFK driver is Opus 5.5 High by the owner's rule, first paragraph above]; Claude tells the owner in one sentence
which level fits (an effort change keeps the cache, a model change rebuilds it). Fast mode: `/fast`; persists across sessions -
`fastModePerSessionOptIn: true` or `CLAUDE_CODE_DISABLE_FAST_MODE=1` make "off" stick [C]. Subagents: frontmatter
`model` / `effort` (versioned in the private Claude Code harness), the per-call `model` / `effort`, then
`CLAUDE_CODE_SUBAGENT_MODEL` (which needs `CLAUDE_CODE_SUBAGENT_MODEL_FORCE=1` to reach the built-in Explore / Plan; it
does not override `model: inherit`) [C]. Workflows: per-agent `model` / `effort` in the script, the size guideline.
Ultracode: a setting, `/effort ultracode`, or the keyword in a typed prompt.

### Live data points (feed the T26 sweep)
- 29 Oct 2026, HUD research round 3: three web readers routed per the table (`research-small`: Sonnet, medium) came
  back thin - each fetched 1-6 pages (several 403s) and filled the rest from memory - against the same day's Opus readers
  (inherit, high) that cited 10-30 fetched sources each. One escalation (Opus, high) filled the gaps. n = 3 vs ~10:
  suggestive, not proof. **Acted on (29 Oct 2026, the owner asked whether it was followed up):** `policy.json`
  `research-small` / `research-broad` readers now Opus at medium (effort is flat for research quality, so the model is
  the lever), and their briefs ask for 10+ fetched pages with memory claims marked; the research table row below is
  superseded for readers. Re-check after the next two research rounds (fetched-source counts per reader).

### Corrections the verifier made to the first proposal
The AFK tick cost (1-hour main TTL, not 5 minutes: ~$0.10, not $2.50); "a fresh session per leg" in AFK is impossible
from cron turns; a per-call effort parameter exists; run-judge's penalty ~2x not 2.5x and never incurred; the README
line references (the private Claude Code harness notes is 86 lines: the game-facts benchmark :69, the 39 h sink :48, run-judge :19);
"two failed attempts" is the owner's memory rule, not the cited article; effort changes keep the cache on all four
current models (not an Opus-only beta); budgets rest on n = 1-4 samples (anecdotes, not baselines).
