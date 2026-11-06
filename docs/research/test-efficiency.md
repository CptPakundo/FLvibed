# Test efficiency: sequential verdicts, tuning under noise, game-time acceleration, offline routes
Status: current
Checked against: FLvibed 3c9c1390 (6 Nov 2026, short form: 22 watched-path commits: the module split P0-P11e (moves, comments, map rows); 2 lines each in splits.py / apdiff.py (paths); T1-T6 and the reach rule unchanged; no effect)
Dirty paths at check: another session's uncommitted the private work queue, `tools/uimock/options.py` (not judged); 950b25cc, 4a270d9e and 3c9c1390 landed during the round, read
Watch: tools/splits.py, tools/flset.ps1, tools/flarms.ps1, tools/flgate.ps1, tools/apdiff.py, docs/TESTING.md, modules/controller/src/host/main.cpp, core/host/input.cpp, core/game/flight.*
Evidence: none saved (web pages read 29 Oct 2026; the disassembly reads below cite `research\disasm\` lines)
Sources: a research agent's report (web + repo greps), the coordinating session's checks against the repo. Tags: [page] a web
source, [file] the repo or the disassembly, [calc] arithmetic in this note, [assumed] not verified

## What it means for FLvibed

*31 Oct 2026 ~04:00 (fc379a3, short form):* T1 matured by M13-MIMIC's use: a segment's failure is its own DNF - a run that timed the segment and failed later is a timed sample (0ba9c25, after fd58d51 dropped runs that never reached it); the history's sigma is capped at the sets' pooled sd's upper 90 % bound from df 4 on (5790994: M13.sinclair mm2 -> mm3 LLR 0.12 -> 2.45, was CONTINUE for the wrong reason); and the lesson of the Q4 sets (a set's smallest detectable effect first) is now a guard: `tools\reach.py` (2.5 x sd x sqrt(2 / cap)) behind `reach-short` (ed05c01) - the Q4 sets (sd 26.6 s, 12 a side ~27 s vs +8 s expected) would be refused. Nothing in T2-T6 changed.

*29 Oct 2026 (04b4c05, new; the owner's priority: the best leg times for the least testing time and effort):* the test
loop (`docs\TESTING.md` "The test loop") already does segment runs from checkpoint saves, runs per side from the spread
and a noise-aware `splits.py compare`. Four things would cut the time it costs most:
- **Fixed-dt time acceleration (T2)** - the game has one variable frame dt that FLvibed's `FrameUpdate` detour already
  sees; feeding it a fixed 1/60 s with the fps cap raised runs the game at fps / 60 of real time (~2.4x at the measured
  ~147 fps) with steps identical to the `fpscap 60` runs. Its runs are the 60 fps group (PBs and golds were set at
  ~147 fps, and the physics is step-dependent), so it decides A/B and the final word stays a real-speed run. A probe
  first (below).
- **Common random numbers (T3)** - with fixed dt and fixed seeds (`srand(timeGetTime())` at 0x5B1B66, FLvibed's own RNG
  seeds), A and B may see the same encounter; if σ falls k-fold, runs needed fall k². Probe: 5 same-seed runs vs 5 normal.
- **A batched sequential verdict (T1)** replaces the fixed runs per side: clear wins and losses stop after 1-2 batches
  of 3, close calls run to a cap. The current rule is underpowered [calc]: n = (2σ/g)² per side gives a standard error
  of the difference of 0.71·g, i.e. ~29 % power to see a true gain g at two-sided α 0.05.
- **Offline route ranking (T4)** from the already-fitted flight model (FREELANCER_RE "Flight model": first-order drag,
  tau 0.17 s, turn cap, piecewise cruise; `flight::PlanRun` predicted the fast dock within 0.1 s) - fly only the top 1-2
  candidate routes, then `routecmp.py` against the runners.
Tuning (T5): no global Bayesian optimisation at 20-60 runs an hour; booleans and presets by racing with the T1 test,
1-3 continuous values per segment by SPSA-style paired perturbations. Pairing by instance and batch, and a regression
adjustment on the start state (speed, position) cut the variance further (T6).
*30 Oct 2026 (12e7942, short form):* T1 built (1da113b, `splits.py sprt`; TESTING.md test loop 4-5): replayed, leg totals
decide 3 of 53 past pairs (none early - target segments), segments 196 of 576 with 10 % of B's runs saved; not yet
looped by `flset` / `flarms`. **Contradicted in part (T6):** pairing by instance alone buys nothing (sd ratio 0.99 over
47 pairs); the start-state adjustment is untested.
*30 Oct 2026 (4ca36df, short form):* T2 slice 1 built and probed (4ca36df, WORK_QUEUE Q3): debug `fixeddt <fps>`
detours the frame timer 0x42D770 itself (not FrameUpdate's argument, as the T2 bullet above put it) and overwrites the
clamped / raw / summed dt; measured **x4.40 game time at 264 fps** uncapped (the ~2.4x at ~147 fps above was an
estimate), x1.00 with `fpscap 60`; at 1x on M13 3 + 3 legs show no detectable difference (2 passes in the fixed-dt arm:
not proof of neutrality). Above 1x the runs are not comparable yet (the server clock, its damage timestamps, audio,
comm lengths, FLvibed's ~40 wall timers: slice 2). T3's fixed-dt half now exists; its seeds are still open. The six new symbols
(217 in all) match statically on four exes (`discovery.md`).
*30 Oct 2026 (aafe41d, short form):* **T1 is in the set driver** (1cf549b: `flarms.ps1 -Sprt [-SprtSeg]`, opt-in; 351cd96 `-SprtA` after a rotated order inverted arm A) and ran three live Q4 sets - q4s1 INCONCLUSIVE, q4s2 REVERT
(the fails guard), q4s3 INCONCLUSIVE, each to the 12-a-side cap (no early stop seen yet). Their lesson (afc495d,
TESTING.md test loop 4): compute a set's smallest detectable effect before running it - M13.bsdown's sigma 26.6 s
hides any effect under ~20-27 s at 12 a side, which is why the variance cut (T2 / T3) comes first. **T2 slice 2 built**
(ee25e00: FLvibed's decision timers on one game clock, `game_ms` in FLMotion, Server.dll's clock follows fixed dt);
**probe 4 FAILS for comm-gated segments** (19c034c: 3 uncapped M13 legs hit the game-time fail state before the
battleship objective - the story's comms stay on wall time): T2 is parked there; a fight-only segment from a save past
the comms is the open use (the speed-up only). **T3 probed, negative** (e0e291e / 10e69bd: `seed <n>` at the harness's
moment, sd 12.0 vs 14.9 s, same-seed runs 70-103 s apart - no common random numbers); next would be a seed at a fixed
game event plus a determinism check of the motion trace. So the "runs fall k²" saving above is unrealised; T6's
start-state adjustment is the variance lever not yet tried. A leg's best is now the best FULL leg everywhere
(78752d8, d9f445a, 8278963 `splits.py full_pass_row`).
*30 Oct 2026 ~11:50 (16df8bd, short form):* **T1 is in both set drivers now**: `flset.ps1 -Sprt -SprtA <ref> [-SprtB]
[-SprtSeg]` (bd80303, the two-instance form: after every third run the pair is judged from the shared history; a final
verdict or ERROR ends the set, `-Runs` stays the cap) - argument checks and the verdict parse verified, a live early
stop still not seen. `flgate.ps1 -ApSet` (0ce1b2b) sends switches after each boot, so a whole gate can run a
measurement-only setting (the A37 shadow gate s5shadow); `tools\shadow_digest.py` reads its agree / differ per rule and
fails closed. The variance levers (T2's fight-only segment, T3's seed at a game event, T6's start-state adjustment) are
unchanged.

## Findings

### 1. Sequential A/B for noisy runs
- **SPRT / GSPRT (fishtest)** [page]: sum the log-likelihood ratio of H1 (gain = elo1) vs H0 (elo0); stop at
  ln((1-β)/α) and ln(β/(1-α)) (±2.94 at α = β = 0.05: https://talkchess.com/viewtopic.php?p=682938). Stockfish's bounds,
  e.g. gainers [0, 2], non-regression [-1.75, 0.25] (https://www.chessprogramming.org/Sequential_Probability_Ratio_Test).
  Typical saving 40-60 % of a fixed-n test with the same errors (https://metricgate.com/docs/power-sprt-sequential/); a
  proven 36-75 % reduction for symmetric error bounds (https://arxiv.org/abs/2603.00216). Fishtest's pentanomial model
  pairs games to cut variance (https://github.com/vdbergh/pentanomial) - the analogue here is pairing A and B by instance
  and batch.
- **mSPRT / always-valid p-values** (https://arxiv.org/abs/1512.04922): free peeking, less power per sample than an SPRT
  tuned to a known gain - FLvibed knows its gain (max(5 s, 2 % PB) today), so SPRT fits better.
- **Bayesian expected loss** (https://www.chrisstucchio.com/blog/2014/bayesian_ab_decision_rule.html): optional
  stopping still inflates errors (2.5 % -> 11.8 % wrong picks in one simulation:
  https://www.r-bloggers.com/2015/08/is-bayesian-ab-testing-immune-to-peeking-not-exactly/amp/).
- **Racing / bandits** (F-Race, irace: https://iridia.ulb.ac.be/IridiaTrSeries/link/IridiaTr2016-004.pdf): drop an arm
  once significantly worse than the best; helps only with 3+ arms. Thompson sampling minimises regret, not
  best-arm identification - the wrong objective here.
- **Expected runs per side** in units of (σ/g)² [calc]: equal-error fixed test at α = β = 0.05 ~21.6; SPRT at a true gain
  of 0 or g ~10.6, at g/2 ~17.3, at 2g ~3.9. The noise ratio matters more than the rule: at σ/g ~6.7 (M12's full leg) no
  test is affordable - test segments.
- **Recommended rule (T1)** - a batched two-sample GSPRT per target segment:
  - value y = the segment time on a PASS; a DNF counts as the checkpoint's fail limit from `docs\SPLITS.md` [refined 31 Oct 2026: a segment's failure is its own DNF, 0ba9c25]
    (pessimistic censoring); winsorise at median + 3·MAD;
  - σ = max(the history sd of the last 10 passes, the pooled sd within the set);
  - d = ȳA − ȳB, V = σ²(1/nA + 1/nB), **LLR = g·(d − g/2) / V** (paired by instance and batch: V = s_d² / n);
  - KEEP at LLR ≥ ln((1−β)/α) = 2.77 (α 0.05, β 0.2), REVERT at ≤ ln(β/(1−α)) = −1.56, else one more batch of 3; at 12
    per side stop "inconclusive" and revert;
  - g = max(2 s, 2 % of the segment PB) (elo1; elo0 = 0);
  - guards: a reversed SPRT on the leg total (H1 "worse by g", at α/k for k segments) and a revert when B has 2+ more
    fails than A;
  - batch-wise checks overshoot the bounds a little: slightly conservative, acceptable.
  - Build: `splits.py sprt <A> <B> --seg CP --gain g --alpha --beta` printing LLR, bounds and CONTINUE / KEEP / REVERT;
    `flset` / `flarms` loop on it. Measure first by replaying past sets in `splits.tsv` offline (runs used, verdicts
    flipped). Cost ~1 day; risk low; confidence high.

### 2. Tuning with expensive noisy evaluations
- Optuna / BO documentation warns against noisy objectives (https://arxiv.org/pdf/2111.14756); TPE needs retuning under
  noise (https://arxiv.org/pdf/2304.11127). SMAC's intensification is aggressive racing on shared seeds / instances
  (https://arxiv.org/pdf/2109.09831). Stockfish tunes dozens of numbers with SPSA - perturb all at once, play θ+ vs θ−
  (https://chessprogramming.org/SPSA). CMA-ES needs adaptive re-evaluation under noise (https://arxiv.org/abs/2409.16757).
- For FLvibed (T5, confidence medium): racing for switches and presets, SPSA pairs for 1-3 continuous numbers per
  segment; SMAC3 over Optuna if a library is ever wanted.

### 3. Game-time acceleration
- Web [page]: Cheat Engine's speedhack hooks timeGetTime / GetTickCount / RtlQueryPerformanceCounter. Unity ML-Agents
  scales fixedDeltaTime with timeScale (default 20); its staff warn of tunnelling and check trained policies at scale 1
  (https://forum.unity.com/threads/can-the-timescale-be-used-for-an-training-acceleration.919295/). No Freelancer
  community knowledge on timing or speedhacks was found.
- The game's frame timing [file; disassembly read by the agent, not verified in game - *verified in game 30 Oct 2026
  (4ca36df, `FREELANCER_RE.md` the 0x42D770 row): cap 200.0, the clamp 0x610A08 = 1.0 (dt at most 1 s), dt 0.005 at
  the cap*]: the frame timer at 0x42D770
  (`Freelancer.ann.asm` 64650-64727) spins in Common.dll `Timing::Delta::calc_dt` until dt ≥ 1/[0x610A0C] (the fps cap,
  set from `RtcSlider::get_default_fps_cap`; HDE's `flplusplus.ini` `max_fps = 200`), clamps dt ≤ 1/[0x610A08] (value
  unknown), stores raw dt at 0x667D48, clamped at 0x667D40, accumulates 0x667D38. The main loop passes 0x667D40 to
  `FrameUpdate(double dt)` 0x5B2890 (`FREELANCER_RE.md` line 87, checked), which calls `Timing::UpdateGlobalTime(dt)`,
  0x59E080(dt) and the game state's vtable +0x30 with a float dt: **one variable step per frame from one source.**
  FLvibed detours FrameUpdate already (`modules\controller\src\host\main.cpp` 506-517, checked).
- Real-time clocks that ignore dt: `timeGetTime` at 0x566544 / 0x566A98 (a 1000 ms timer) and 0x5AC2DD-0x5ACE46;
  `srand(timeGetTime())` 0x5B1B66; audio and voice lines; ~106 GetTickCount / QPC uses in `modules\controller\src\impl` (e.g.
  `autopilot.cpp` 312 / 327, `autopilot_combat.cpp` 342 / 1274 / 1845 an RNG seed). `livesplit.cpp` 521 sums game dt.
- The physics is step-dependent: the 60 fps runs are their own group (`AUTOPILOT.md` "60 fps runs", 5234017 `fpscap`).
- **The probe (T2)** (*built 30 Oct 2026, WORK_QUEUE Q3 slice 1: a detour of the frame timer 0x42D770 itself, not
  FrameUpdate's argument - `docs\HARNESS.md` "Fixed dt"; probe steps 1 and 3 done, 4 needs slice 2*): read
  0x610A08 / 0x610A0C live; a debug `fixeddt 60` passing 1/60 into the original FrameUpdate and
  writing 0x667D40 / 0x667D48, the cap raised; 3 baseline runs of 2-3 short segments at `fpscap 60`, the same at 2x and
  3x, `apdiff.py` against the baseline; comms and objectives in the same order (audio stays real time: comm-gated
  triggers may shift); FLvibed's own timers and the harness timeouts on game time. Cost 2-3 days, risk medium.
- **Determinism (T3)**: fix srand and FLvibed's seeds, fly 5 identical runs; coinciding traces = common random numbers.
  Open: whether the SP server's `IServerImpl::ElapseTime(float)` gets the client's dt (*answered 30 Oct 2026, 4ca36df /
  FREELANCER_RE: it takes 0x667D40, so `fixeddt` reaches it; the server's own clock stays on wall time*); Server.dll /
  NPC randomness.
- Measure CPU vs GPU limits per instance first (memory rule "measure the bottleneck first"); if GPU-bound, lower the
  background instances' resolution.

### 4. Offline trajectory optimisation
- Foehn et al.'s time-optimal planning beats human pilots but takes up to an hour per plan
  (https://rpg.ifi.uzh.ch/docs/ScienceRobotics21_Foehn.pdf, https://arxiv.org/abs/2108.04537); car racing uses
  minimum-curvature QPs or minimum-time optimal control (https://github.com/TUMFTM/global_racetrajectory_optimization).
- Freelancer's flight is simpler and already fitted: a point-mass model with speed modes (engine, afterburner, cruise
  charge then 60 m/s² to 300 m/s) and turn arcs, plus dynamic programming over mode switches, ranks candidate routes per
  leg (T4). It cannot model combat or scripts. Cost 3-5 days; risk low; confidence medium.

### Ranked by expected saving
| # | Change | Expected saving | How to measure |
|---|---|---|---|
| 1 | Fixed-dt acceleration (T2) | 50-65 % wall clock | game time / wall time per leg; apdiff clean |
| 2 | Seeded RNG + fixed dt (T3) | runs fall k² if σ falls k-fold (2-10x possible) | sd of 5 same-seed vs 5 normal runs |
| 3 | Batched GSPRT with censored DNFs (T1) | 40-60 % fewer runs than an equal-power fixed test | replay past sets offline |
| 4 | Pairing + start-state adjustment (T6) | variance cut, size unknown | paired vs unpaired sd |
| 5 | Offline route ranking (T4) | fewer route sets | route experiments per leg |
