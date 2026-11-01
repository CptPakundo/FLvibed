# AFK / autonomous runs: failure modes from this project's history and the mechanisms that prevent them
Status: current
Checked against: FLvibed 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 2 watched-path commits: dff3fac2 (guard set-no-progress: a flset part run (-StopAt) or a set from a named checkpoint save is no attempt the verdict counts - 25 overrides of the rule in 24 h, drift_check "overrides"; a save in a variable stays counted; 141/141) - the T28 override cause fixed at the guard, the override cap itself still open; 5dcf6195 (routing: a docs move / split / trim -> bulk-doc-writing; handoff RESUME block); no conclusion changed)
Dirty paths at check: another session's uncommitted `controller/src/impl/autopilot_impl.h`, `controller/src/impl/autopilot_objective.cpp`, `controller/src/impl/autopilot_settings.cpp`, `controller/src/impl/lookahead.{cpp,h}`, `core/fluf.cpp`, `tests/fluf_probe_test.cpp`, `tools/setwatch.sh` (not judged) (31 Oct 2026 ~15:25)
Watch: tools/claude/**, tools/drift_check.ps1, tools/flset.ps1, tools/flgate.ps1, tools/splits.py, controller/tools/fllegrun.ps1, docs/TESTING.md
Evidence: `research\evidence\afk_autonomy\` - `afk_repo.md` (the history: failure modes with dates and evidence paths), `afk_web.md` (Anthropic, METR, practitioners; URLs), `afk_proposal.md` (protocol v2, 12 changes ~17 h), `afk_verify.json` (the sceptic's 13 checks against the files and docs)
Sources: a research workflow (29 Oct 2026, the owner's request); corrections are the verifier's, the flset default re-checked by the coordinating session [checked]. Tags: [file], [page], [D]

## What it means for FLvibed

*31 Oct 2026 ~04:00 (fc379a3, short form):* **the AFK rollover is suspended** (4fb61dc; the owner, non-negotiable: Remote Control must be on for every new driver, and AFK means away from the desktop). Measured: `set_remote_control` in a scheduled run nobody opened is refused ("needs a person's approval") with and without the owner's allow rule, for itself and for another session; at the desktop it worked at once. So `..\.claude\afk.norollover`: `afk_handover -Request` refuses (exit 3), guard `context-launch` passes (the driver compacts in place), successor_prompt 1b creates the file when Remote Control is off. An AFK run is now ONE owner-started Desktop session at Opus 5.5 High with Remote Control (b195314; `docs\AFK_PLAYBOOK.md`, 1da4ed7); the 30 Oct paragraphs below describe the rollover as built and are history until it is re-enabled. Against the context exhaustion it answered: guard `context-launch` (a set launch past 400K tokens is refused unless a handover is pending, ff89026; 4 sessions had run ~1,860 calls over it) and compaction. The audit's enforcement line continues (e7d7c0a handoff-size, ed05c01 reach-short, 39cccda retro-row, e80985e docs-writeback, 4b9eb58 probe-first, e5083f8 / fe771ef judge-gate counts only the session's own launches' rows); 2d92292: a set from a checkpoint save (`-StartCp`) is no NO PROGRESS (T28's rest unchanged). Nothing in the conclusions is contradicted; the failure list gains "a successor that cannot be fully set up unattended".

*30 Oct 2026 ~23:30 (5eb7bcb, short form):* the owner's "little point to auditing if it is not enforced" turned the
evening audit into guards, as this note's T-rows argue (hooks hold, prose does not): an unread leg launch blocks the turn
end (84a0330), a launch states its budget, a set its reach (122ee60), a story leg needs its runner study first (9e58722),
a gate reports par distance, not "OK" (487c20c, 7af5f5f; T34's verdict words). T32 / T33 still open.

*30 Oct 2026 ~18:10 (the owner approved three changes after asking "was this handover process an improvement?"):*
two autonomous takeovers worked (13:04Z, 15:04Z; no run dropped; mean context 396K vs 497K the day before, $ / commit
1.9 vs 3.3 - confounded by the work mix), but the hourly period made 24-34 min waits (a scheduled run cannot fire
another task: `run_scheduled_task` is refused there too), and a predecessor handed over with a subagent still writing,
which then overwrote the successor's files. Now: the task every 15 min (`*/15 * * * *` + the app's fixed 210 s, `-AbortMin` 35);
`-Request` refuses while a subagent transcript of the session was written in the last 5 min (`-Force` names it); the
predecessor keeps its running work going until HANDED (no new package, no new subagent). Test:
`tools\claude\test_handover.ps1` (15 cases since). Later the same evening: the task is PAUSED outside AFK (enabled at
the afk-run Start, disabled at its End - only an owner-present turn can change it), and every run renames itself to
the TITLE afk_handover prints (`seq=` in the flag); runs stay under the routine (the app refuses to group or pin a routine run:
"Routine runs show under their routine"; Desktop 2.37337.1 hides the sidebar's Routines section for good -
anthropics/claude-code#67992), so `/afk-series` (tools\claude\skills\afk-series) lists them in a pane.

*30 Oct 2026 ~11:50 (16df8bd, short form):* **the rollover's first failure is fixed at the source** (9a1d545, the
owner: "make the handover work properly and autonomously"): the fixed task `flvibed-afk-successor` now recurs hourly (superseded the same evening: every
15 min, paused outside AFK, `-AbortMin` 35 - the paragraph above) and a run without a request releases itself, so a predecessor only `-Request`s (no tool call - `update_scheduled_task`
is refused inside a scheduled run) and keeps working; `-AbortMin` (default 80 = a period + 20) says ABORT NOW. It
worked once since: the successor e26b9bf0 took the run over at 09:04Z (6833b42). One more false NO PROGRESS removed: `set-no-progress` looks
up only a leg id (bd80303; a dry-run's `-Leg ZZ` read every leg as NO PROGRESS) - T28's open rest unchanged. The judges
got two fail-closed tools: `shadow_digest.py` (0ce1b2b, a gate's shadow agree / differ, unread folders and differs
fail) and docs-auditor criterion 8 (473ab12: cited evidence must come from a clean build of the commit; twice a
subagent cited its pre-commit dirty build). Doc size budgets in drift_check (a125547, T13). T32-T34 still open.
*30 Oct 2026 (aafe41d, short form):* a new mechanism against context exhaustion: **the AFK rollover** (61edf76, the
owner's standing OK) - `tools\claude\afk_handover.ps1`'s handshake (-Request / -TakeOver / -Status / -Complete, a
20-min -Abort that keeps the predecessor working) and one fixed scheduled task `flvibed-afk-successor` updated per
handover (7b0f84e: a task created in a cron / agent turn does not keep the permission mode). Its first use failed
safe: a session that is itself a scheduled run cannot update the task (it needs a person's approval there), so the
run carried on by compaction (3646262; an owner item) *(superseded 30 Oct 2026 by 9a1d545: the task recurs and the
predecessor only requests - line above)*. Fewer overrides at the source: `build-during-set` now reads
'every leg booted' itself (0d278f1; overridden 7x in 24 h for that safe case). **T35 extended** (8fbdf5b): the in-game
testing method's own trend (`docs\test_budget.tsv` -> health.js + drift_check `testing`); drift_check also gained
`ratchets` (0977002, T13) and `leg-bests` (8278963). T28's rest, T32-T34 still open; `docs\WORK_QUEUE.md` stays
hand-kept (T33).
*30 Oct 2026 (12e7942, short form):* the first four gaps below are closed - progress counts 2-run sets, cuts reused
labels, drops stop-at runs and `set-no-progress` covers `flleg` (1da113b, 8f32140: M09 and M11 became NO PROGRESS);
stale `afk.released` ids expire after 24 h, the keep-alive runs every 10 min, `review-due` and stop-the-line guards read
`drift_last.txt` (1ca71ac, T29-T31). Still open: T28's rest (non-`Mxx` labels, segment series), T32-T34.

*29 Oct 2026 (989d138, new; the owner: get the most out of AFK / autonomous modes with the fewest drifts, regressions or
unexpected stalls):* the harness already holds the big ones - `afk.flag` + `afk-guard.js` + the cron keep-alive (idle
stops), `compact-handoff.js` (context loss), the watchdogs and `flfreeze.ps1` (hung tests), `set-no-progress` (loops),
`set-dirty-build`, `deploy-front`, `git-commit-all`, `edit-check`, the judge gate. What the history and the verifier
show is still open:
- **The progress verdict misses the default path** [checked]: `flset.ps1` defaults to `-Runs 2` (line 14) while
  `splits.py progress` counts only sets of 3+ runs (`splits.py` 870) - a default single-instance set is invisible to NO
  PROGRESS; reusing a label merges attempts into one set; a non-`Mxx` label and segment-save runs drop out (T28).
- **`set-no-progress` fires only for `flset`** (`guard.js` 131-143), not `flleg` launches; 0 live fires so far (T28).
- **The stale `afk.released` ids** (3, one a live 29 Oct session) let a listed session end AFK turns unguarded; the
  rollover they served is dead for cron turns (owner-turn rollover would go with it) (T29).
- **afk-guard allows every stop while a cron or background task exists** (`afk-guard.js` 24-25): the measured ~5.4 h of
  idle gaps are by design - the cron, not the Stop hook, carries the run. A shorter cadence (`*/10`, or `*/5`, the one
  schedule without jitter) cuts the idle wait (T30).
- **No reviews recorded after 24 Oct** (the last REVIEW lines in `HANDOFF_history.txt`); whether skipped or unrecorded is
  unknown. A `review-due` guard needs one machine-written place for the last review (e.g. `drift_check` writing
  `.claude\drift_last.txt`) (T31).
- **Agents changing their own judges** (METR's reward-hacking reports; directional evidence): judge files (splits limits,
  `checkpoints.tsv`, par tables, `flgate.ps1`, `drift_check.ps1`, the hooks) change in their own commit with a reason -
  exempting generated outputs (`checkpoints_gen.h`), which must commit with their source (T32).
- **A queue with machine-checked done** (Anthropic's long-running-agents harness: a JSON feature list the agent may
  only flip by passing): `.claude\afk_queue.json` from HANDOFF "OPEN", each item with `done_check`, `reach_test`,
  `budget`, `status`; a script flips `done` only when its check passes; a PreToolUse rule refuses hand edits (a
  PostToolUse check reports too late) (T33).
- **Owner-presence signals fail open**: `UserPromptSubmit` also fires on cron prompts, subagent reports and session
  messages with no source field - a guard "the owner said so" cannot rest on it [page].
- **The gate's verdict words are OK / SLOWER / FAIL** (`flgate.ps1` 42): revert rules must key on SLOWER; the gate is a
  catastrophe detector (one run per leg, 1.15x) - a SLOWER leg gets a confirmation set before a verdict (T34).
- New guards need a replay against the 29 Oct transcripts before they go live, and an AFK-safe way out (a refusal with no
  owner present must name the next legal step), or one wedged rule stalls a whole run.

## Findings

### Failure modes -> mechanisms (the proposal, corrected)
| Failure mode | Mechanism | Status |
|---|---|---|
| Idle stop | flag + afk-guard + cron | existing |
| Context loss | compact-handoff live state; RESUME in a git-ignored `.claude\afk_state.json` injected by the same hook (removes the RESUME share of the 28 % handoff-only commits, not the cap-shrink share) | change |
| Hung test | `-StallSec`, deadlines, the `fllegrun.ps1` watchdog, `flfreeze.ps1` | existing |
| Alive but aimless run | a progress-rate watchdog in `fllegrun.ps1` (objective distance not falling 5 % in 90 s outside fights / scenes -> FAIL STALL-ALIVE); thresholds from the checkpoint table; a probe first | new |
| The agent's own unbounded waits | a guard against shell wait loops with no deadline term; checked against how waits really run (Monitor, background tasks) | new |
| Loop without progress | `set-no-progress` also on `flleg`; progress counts 2-run sets and segment series; after 2 overrides without a new best the target changes | change |
| Underpowered default-on | refuse flipping a default on without a set of the required N and a gate newer than the change | new |
| Waived stop-the-line | refuse sets while the last drift_check shows ATTENTION on `replay` / `decisions` / `saves` | new |
| Judges changed with the code | `judge-immutable` (generated outputs exempt) | new |
| Override erosion | 4 `build-during-set` overrides within ~7 min in one session on 29 Oct (6 that day, 2 sessions): a second override of a rule within 30 min refused | new |
| Unverified work presented as done | done flips through the queue script | new |
| Stalls outside the session (a dead session) | a heartbeat file from a PostToolUse hook + an outside check (a Desktop scheduled task or a Windows task - persistent config, the owner's OK) | new |
| No drift review for non-leg work | `drift_check` lines `queue`, `sets-under-N`, `switch-stale`, `overrides` (test rows filtered: 12 of 56 `guard_fires.tsv` rows are the hook tests'), `afk-infra` (cron age < 6 days, heartbeat, flag) | new |

### Work selection, regression net, context (the proposal's sections 2-5, kept)
- Next item: stop-the-line first, then the active item inside its budget, then the next todo; an empty queue falls back
  to verify-what-was-built, the sweep's open findings, the research check-up, triage. One item active, one thing per
  commit; park with a reason on budget / NO PROGRESS / two failed attempts / the override cap.
- Early set stops in `flset.ps1` (reliability: the 2nd FAIL of 6 already fails 5/6; futility: more than half the planned
  PASS times slower than best − max(2 s, 1 %)) - specified together with the progress verdict, so a stopped set is
  still counted (with `test-efficiency.md` T1).
- After a kept shared change: the gate, detached; SLOWER legs get 3-run confirmation sets; one fix attempt, then
  `git revert` and park.
- Runs are carried by compaction (no fresh session from a cron turn); subagents read, the main session alone builds,
  tests and commits; re-test the scaffolding when the model changes.
- End-of-run report (`..\AFK_SUMMARY_<date>.md`): hours / commits / items, verified results with evidence, speedrun
  bests vs par, regressions found / reverted, decisions taken for the owner, judge-file commits, overrides and guard
  fires, stalls with causes, waits on the owner, usage - plus the working method's own trend (memory
  `work_improves_over_time`: T35).
