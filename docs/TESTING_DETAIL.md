# Testing FLvibed: sections moved out of TESTING.md

Documents diet part 2 (15 Nov 2026): `TESTING.md` was over its size limit. These two sections moved here word for word;
`TESTING.md` keeps their headings with a short pointer, so every reference to them still leads here. The rules stand.

## Parallel lanes (the owner, 12 Nov 2026)
C++ work runs in several **lanes** at once (at most four with games: below): each a git worktree `..\FLvibed-lane-<name>` beside the main checkout,
on its own branch `lane/<name>`, with its own `build\` folder, build lock and game test instance, merged into main one at
a time and removed after its merge. The tool and every command: `tools\lane.ps1` (`tools\README.md` "Parallel lanes").
- **When:** a C++ package (a fix, a feature, a restructure step) that builds and tests while other C++ work goes on in
  main or another lane. Docs-only and tools-only changes stay in main. One package per lane; a lane lives until its merge.
- **Worktrees are made and removed only with `tools\lane.ps1`** (`new` / `remove`), never by hand and never with
  `git worktree add / remove` directly: a lane holds junctions into main's folders, and a recursive delete or a
  `git worktree remove` that met one could go through it into the shared install or the evidence. `remove` first proves
  nothing holds a file or a current folder inside the lane (a rename of the folder there and back), then takes every
  reparse point out as a link only and compares each target's files before and after; a removal that stopped half-way
  is finished by running `remove` again. The registry `..\.claude\lanes.json` is the tool's: never edit it or put back
  a copy of it by hand (other sessions' lanes come and go in it); a lost entry comes back with `lane.ps1 adopt`.
- **The cycle:** `lane.ps1 new <name> -Instance <n>` prints the lane's commands; work and commit IN the lane
  (`git -C <lane> commit -m ... -- <paths>`); build with the lane's own `tools\buildlock.ps1`; deploy with the lane's
  `tools\deploy.ps1 -Background -Release -NoBuild -Instance <n>` (deploy refuses any other instance from a lane, and
  refuses a lane's instance from any other checkout); `lane.ps1 sync <name>` (rebase on main); `lane.ps1 verify <name>`
  (runs the unit tests, a release build and the module lint in the lane on its tip; `-Passed 'nn_pages=<run
  folder>,classic_frames=<run folder>'` records the nets' runs); `lane.ps1 merge <name>` (fast-forward only: a
  rebased, clean, verified tip; nothing is pushed); `lane.ps1 remove <name>`. A sync moves the tip, so verify again
  after it: merge only takes a marker for the exact tip.
- **Nets as one command, never polled** (14 Nov 2026): a lane runs its nets through `tools\nets.ps1` in the background - `pwsh -File <FLvibed>\tools\nets.ps1 -Lane <lane> -Run "tests\a.ps1 -Instance <n> ;; tests\b.ps1 -Instance <n>"` - which waits for a free game slot (at most four games at once, all lanes) and runs the nets in turn; one notification at the end, the log in `<lane>\scratch\nets_<stamp>.log`. Its test: `tools\nets_selftest.ps1`.
- **The two nets and the lint from a lane:** run the lane's own copies, `<lane>\tests\nn_pages.ps1 -Instance <n>` and
  `<lane>\tests\classic_frames.ps1 -Instance <n>` (the lane's instance, always named: since 13 Nov 2026 nn_pages, info_compare, base_pages, classic_frames and the pad nets have no default and refuse to start without it; the others' defaults are main's
  instances), and `<lane>\research\tools\ocr\.venv\Scripts\python.exe -I <lane>\tools\module_lint.py --enforce`. They
  read the lane's `build\release` and write their run folders through the lane's `research\experience\runs` junction into
  main's evidence. **The classic net's reference frames are the one shared pinned set** (`runs\classic_frames\pinned\`,
  reached through the junction): a lane compares against it and never re-pins it (`-Pin` refuses from a lane); a change
  that needs a re-pin is re-pinned from main after its merge, on purpose, as the net's rules say above.
- **Instances:** numbered ones only, never `v11nightly`; the registry (`..\.claude\lanes.json`) holds one lane per
  instance. A game started from a lane reports the lane's path (`<lane>\Install\instances\<n>\...`), which main's
  `Get-FLGameProcess` does not match: that is why an instance belongs to one checkout at a time.
- **At most three workers with games at once**, on files that do not overlap (the owner's lean method of 13 Nov 2026; 12 Nov 2026: with up to five, a walk test failed falsely under load,
  on a machine shared with the owner's other work): count every worker that starts Freelancer.exe, in a registered
  lane or from an outside copy, and every such worker registers its instance with `tools\lane.ps1` (an unregistered
  worker's instance gave unexplained results the same day: a run drew a newer build than it had installed). A
  timing failure of a walk test while other games run is re-run once alone before it is believed.
- **Who merges:** an additive package (new files, symbols, tests, documents) may merge itself; a lane that moves or
  changes existing code stops at "ready to merge" and the lead orders the merge. One merge at a time.
- **A worker ends its own log followers** (`tail -f`, `Get-Content -Wait` and the like) before it finishes: a
  leftover follower holds its file open, outlives the session and blocks `lane.ps1 remove`. Stop only the ones it
  started, identified by their command line.
- **A game that exits silently** (no dump, no log line) is recorded as its own finding: the date, the instance, the
  build id and what ran. Until its cause is known it is evidence neither for nor against the package under test.
  **A silent exit: read its code here** (since 13 Nov 2026): the instance's `EXE\logs\game_exits.log`, one line per game deploy
  started (`tools\flexitwatch.ps1`, `docs\HARNESS.md` "How a game ended"; the harness's "Freelancer exited" / "shared memory not
  found" errors carry it as `; game exit: ...`). Codes measured on this PC: `0xFFFFFFFF` Stop-Process (every test's own clean-up),
  `0x00000001` a Claude task's process tree stopped, `0xC0000409` fail-fast (/GS), another `0xC00000xx` a crash through
  Freelancer's filter (no event, no dump), `0x00000000` the game's own quit. Record the line with the finding. A crash's place:
  the block in `EXE\FLvibed_crash.log` with the same pid (and on a test start `EXE\FLvibed_crash.dmp`; `docs\HARNESS.md` "How a game ended").
- **What stays private to a lane:** `build\` (`build\sdl3` copied at `new`), `scratch\` (the worker's scratch files:
  the lanes no longer share one scratch folder), `.lane\` (the markers and verify logs, kept in main's
  research\experience\lanes at removal), and anything a tool writes inside the lane outside the junctions: leg folders
  and rows (`research\experience\legs\` holds the tracked `legs.tsv`, so it is not linked: legs, sets and gates run from
  main), `research\experience\*.jsonl`. `remove` refuses while such files exist, until `-KeepScratch <folder>` copies
  them out or `-DiscardScratch` drops them.
- **Builds side by side** (measured 12 Nov 2026, 32 threads, the machine already about 30-40 % busy with other programs):
  one release build alone 24 s, about 430 CPU-seconds (the machine at 85 % on average while it runs); two release builds
  in two lanes at once 40 s each, both done in 42 s (48 s one after the other), the machine at 92 %. So each lane locks
  only its own folder: lanes never wait for each other or for main, at the price of a saturated CPU for that half
  minute, which slows the other programs on the machine a little; three at once would take about a minute.
- **The Claude Code hooks** read the main checkout's state in some rules, whatever folder a session works in: a session
  in a lane is shown main's handoff at its start, and the commit-sweep rule judges main's working tree. Stage explicit
  paths in a lane as everywhere.

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
  launch / its space scene; `land`, `launch`, `cutscene`, `success`; `kill:<n>` the n-th destroyed marked target;
  `save:<ids>` the mission's checkpoint save with that description id, the log's `mission-save ids=<ids>`, 8 ms after
  the game's GAME SAVED message 1890 - M01b.end = 32120, where the runners' split ends: CP-M01b, 30 Oct 2026; in game
  HostApi v35 MissionSave), how the
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
- **Fail limits - a catastrophic mistake or delay** (`tools\harness\missions\leg_checkpoints.tsv`, generated by
  `splits.py limits`): a segment fails when it takes longer than max(2x the runners' slowest time for it, +45 s;
  1.5x our gold + 20 s) - far outside both the runners' worst and our own best; a full leg also fails when a checkpoint
  is not reached by 1.15x the PB's split + 30 s. The own-history bounds tighten by themselves as golds and PBs improve;
  a segment limit is never below the PB run's own segment + 20 s (29 Oct 2026: the new M13 PB was over an 82 s runner
  bound at M13.sinclair), and a good run older than the PB counts as superseded for any limit (the floor that let it
  pass was its own while it was the PB: 30 Oct 2026, mpb23 after the gate s7arb PB); a stop-at run is no good run.
  `kill:` checkpoints are soft (our log names only the autopilot's own target's kills): unseen at their limit, it carries
  over to the next. So is an `obj:` checkpoint the script skipped (30 Oct 2026, M05 in gate s5shadow: 22867 posted in the
  jump tunnel, 22870 shown): since the previous checkpoint a later checkpoint's objective showed, or one whose text id
  lies between this checkpoint's and the next obj checkpoint's (ids are not in story order across a leg) - `Leg.ps1`
  logs "checkpoint <cp> skipped", `splits.tsv` its `skip` column, the judge carries the limit over. `Leg.ps1` checks the limits live (SPLIT lines, `cp` rows in `phases.tsv`, `FAILED (FAIL STATE
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

