# AFK playbook - how an autonomous FLvibed run works (binding)

Written 31 Oct 2026 at the end of the AFK run of 00:45-04:10 from everything that run and the ones before it taught
(`..\AFK_SUMMARY_2026-10-31.md`, `..\AUDIT_2026-10-31.md`, `docs\test_budget.tsv`). The mechanics (flag, cron, review,
compaction audit) are the afk-run skill's; the rules behind them are the private work queue "Working method" 1-24 and
`docs\TESTING.md`. This page is the order of work and the lessons. Read it at the start and after every compaction.

## 1. The session
1. **One session drives the whole run:** started by the owner on the Desktop app, on **the model and effort the owner
   picks** - or, when they ask for a run without naming one, the setup Claude recommends for the run's plan before it
   starts (the owner, 11 Nov 2026; the routing table's "afk" row: Fable 5.2 high for design- and
   orchestration-heavy plans, Opus 5.5 high for leg iteration, tests, builds and chores; the afk-run skill "The driver's
   model and effort") - with **Remote Control on** (the owner, 31 Oct 2026, non-negotiable: AFK means away from the
   desktop). The app's
   "Connect new sessions to Remote Control" covers a session the owner starts, not a scheduled run, and a scheduled run
   cannot switch it on itself (measured 31 Oct) - so **no rollover**: `..\.claude\afk.norollover` stays,
   `afk_handover.ps1 -Request` refuses, the successor task stays paused. The session compacts in place; after each
   compaction do the afk-run skill's post-compaction audit, then re-read this page and the handoff.
2. **Keep the main context small** - every turn pays for all of it (W43: mean 524K, $ / active hour 20.0 -> 28.7):
   - every bulky read goes to a subagent with a precise brief and a size cap on its report (< 4 KB): logs, frames,
     transcripts, docs over ~20 KB, code exploration, judging;
   - never Read a big file whole; Grep with a head limit; no re-reading what is already known; short turns.
3. Start (afk-run skill "Start"): the flag (`seq=1`), the cron keep-alive, the handoff's RESUME block with the cron id.
   Do not resume the successor task. End only on the owner's word (the skill's "End": summary, backup, flag, cron).

## 2. Lanes and helpers
4. **The driver alone owns the C lane** (builds, deploys, games, sets). Side sessions only for read-only research;
   never two writers of the C lane (31 Oct: a set launched from another session's superseded plan, 5 runs wasted).
   Before a launch from a plan someone else wrote: `git log` since you read it.
5. **Subagents** run in the background; one writer per area; their report is a claim - **verify the key claims against
   the evidence before you commit or build on them** (31 Oct: the run-judge called a ship dead that was alive; the
   switch-deletion agent's constants were checked against the old defaults). Commit their files by explicit paths.
6. **Routing:** the router misclassed three briefs on 31 Oct. Check the class the hook names; when it is wrong, stop
   and respawn with the model / effort named and a `route-ok: <why>` line: C++ design / debug Opus high; a scoped C++
   change Opus medium; a harness script fix Opus medium; docs Sonnet medium; judging = the run-judge agent (Opus high).
   Turn each miss into a matcher + a case in the private routing policy.

## 3. Legs and in-game tests (the project's biggest pain point)
7. **Study first, plan second:** the leg's runner study (`docs\research\runners\<leg>.md`, `tools\runner_study.py`)
   before any plan; every change says what the runners do and why ours differs (WHAT / HOW / ADVANTAGE / RISK).
8. **Probe first:** a new switch flies one leg (n=1) and its decision log line is seen before any set (guard
   probe-first). A set states its budget, its reach (`reach=<cp>:<runs>:<expected s>`, guard reach-short) and the
   runners' approach (`# ref:`).
9. **Within ~2 min of every launch** read its live state yourself: `tools\launchcheck.ps1` (boot phase, SILENT, LOOK).
   **At the launch, arm `tools\setwatch.sh`** on the Monitor tool (one event per instance that ends, every terminal
   state): no hand-made wait loops - the owner, 31 Oct 2026: "You waited entire minutes while no instance was running.
   Tighten up those watchers" (a loop grepped set.log for RESULT, which is in `<label>.log`: 6 idle minutes).
10. **Verdicts:** the run-judge with a neutral brief (no hypothesis) and the PAR line, plus `tools\splits.py sprt`;
    state results as distance to par and to the PB (BEAT PAR / AT PAR / CLOSER / SAME / FURTHER). **Stop at the
    verdict** - no "one more run to be sure".
11. **Harness failures are harness bugs** (stuck, popup, crash, a loop, a missed event): classify, fix at the source,
    never count as a result (31 Oct: the AUTOPILOT LOOP stop in Leg.ps1).
12. **Bundle tests:** build several changes, then one probe / set that covers them by reach; the gate (`flgate`) only
    when the owner names it (11 Nov 2026): at the end of a leg's iteration write "gate due: <the switches being kept>"
    into the handoff and go on (`docs\TESTING.md` test loop 6).
13. **The review** (`tools\drift_check.ps1 -Leg <target>`) every 2 h or 3 judged sets; its verdicts are binding:
    UNRELIABLE = fail states first, NO PROGRESS = change the approach or the target, never a fourth variation.
13a. **Escalate a section that does not improve** (the owner, 31 Oct 2026 05:03): 1. two or three agents analyse it, one
    from the runners' side and one from the autopilot's, and reach a consensus on the approach (a third critiques); 2.
    if that brings no significant gain, a new data pass on the speedrun recordings (new kinds of datapoints, other frame
    rates, online research); 3. then reconsider the fundamentals - how the autopilot maneuvers in space and next to
    geometry. Each step only after the one before failed; say which step a package is on. Enforced: guard `escalation-step` (a set past NO PROGRESS names `escalation=<step>:<its note under docs\research\>`).
    **Step 2 reads, it does not infer** (the owner, 31 Oct 2026, after a data pass claimed "vanilla keeps cruise through
    engine-kill taps" from OCR'd speed numbers): HDE keeps vanilla's behaviour (bug fixes and accessibility only; splash
    damage the rare near-exception), so the runners' footage shows the game's real mechanics. A mechanic is stated only
    from the frame sequence that shows it (charge text, cruise flare, travel direction), never from a number trace alone;
    an apparent HDE / vanilla difference is a misread until an HDE probe proves it; every data-pass brief says so.

## 4. Commits, docs, getting better
14. Commit every finished change with explicit paths; docs write-back in the same commit (kb / FREELANCER_RE /
    AUTOPILOT.md / runbook harness lines / SETTINGS.md); the handoff under 8,000 characters (guard handoff-size);
    `tools\backup.ps1` at each package end.
15. A package ends with the docs-auditor, its retro row in `docs\test_budget.tsv` and **one repeat turned into a check**
    (a guard, a criterion, a skill line). The development itself must get measurably better (memory
    work_improves_over_time; `node tools\claude\health\health.js --md` at each review).
16. Name work in plain words, not only codenames. Ask nothing while the owner is away. What needs the
    owner's word (a push, tag, release or GitHub post; the leg gate or the full sweep; deleting what this run did not
    make; a driver or system-wide install; a game in front or with sound) is neither done nor asked about: it goes
    under "WAITS ON THE OWNER" in the RESUME block with what is prepared, and the run goes on. A push notification only
    for what stops the run itself. If everything planned waits on the owner, find more work that is not owner-only
    (the owner, 11 Nov 2026; where to look: the afk-run skill "Never end it yourself").

## 5. What to work on (the owner's order of 31 Oct ~00:40, still binding)
1. **M13 to par** by mimicking the runners - bold changes, no constant small-change testing (then every mission).
2. **Audit and sweep, enforced:** largely done 31 Oct; the open rows of `..\AUDIT_2026-10-31.md`.
3. **The queue** (the private work queue): build first, build a lot, test in bundles at key points.
4. **Feature completeness, weighted to what players use:** HUD classic + HUD 2, controller features, UI / UX / flow.
The concrete NOW / NEXT is the handoff's top block.
