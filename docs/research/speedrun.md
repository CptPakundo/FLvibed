# Speedrunning Freelancer: records, routes, money gates and skips versus the autopilot
Status: current (re-based 25 Oct 2026 at 855745b on SPLITS.md 24 Oct 23:55 and dossier M04; the older dated lines below are history) (before: partly stale (25 Oct 2026: the PBs moved - M04 974, M10 1124, M09 724 - and sets s12-s13 landed, `docs\SPLITS.md` was regenerated 24 Oct 23:55; "What it means" needs a re-base on `docs\SPLITS.md` and the M04 dossier before the next autopilot session - **pending**, not attempted in the 25 Oct ~07:00 UI-build check-up nor in the ~12:15 HUD 2 one) (the "What it means" section re-based on `docs\SPLITS.md` on 24 Oct 2026; V2 / V3's "ours" numbers are dated and marked))
Checked against: FLvibed 144e362 (26 Oct 2026, coordinator short form before the export; 144e362 itself only the handoff + README text: the watched commits since - d89f88c / 79e2bcc smart start + K&M goal panel, 7632ced job objective text, 77d8655 objective-named loot shot, 9ff7fe7 handoff - are autopilot / UI internals; no statement of this note changes; not re-read in depth); before: FLvibed 01fcfa9 (26 Oct 2026, the step 3 short form re-confirmed: the only watched commits since, 9327f86 / 01fcfa9, are doc and handoff edits recording what this note already says); before: FLvibed e5c02cc (26 Oct 2026, step 3 short form: no autopilot / splits change - first "What it means" line); before: FLvibed 855745b (25 Oct 2026 ~23:05, whole-set check-up: 30 watched-path commits 12bc233..855745b, all handoffs plus PADFIX 1 / 4-5 (docs\kb, pad screens) - no autopilot code, SPLITS.md, LEGS.md, checkpoints or dossier change since 9f45cd8; **What it means re-based** on SPLITS.md 24 Oct 23:55 and dossier M04 (s12d-s13b): new first bullet, the 24 Oct one marked superseded); before: 12bc233 (25 Oct 2026 ~12:15, short form after the HUD 2 build: 15 watched-path commits abf84ac..12bc233 (the 15th, 72a6d4e, a handoff that landed during the round), only `HANDOFF.txt` (the HUD 2 build's handoffs, the queued extended HUD 2 task, the owner's rule that HUD 2 never gets in the way of the autopilot work) and `docs/kb/hud-controls.md` (+10 lines, HUD roots, 1ae14b0); no autopilot code, SPLITS.md, LEGS.md, checkpoints or dossier changed - no finding touched; HUD 2 moves the speedrun timer under the target card (placement only, A10 / U22 unchanged); the re-base stays pending); before: abf84ac (25 Oct 2026 ~07:00, short form after the UI build: 33 watched-path commits 25a8c6b..abf84ac, none changing a finding - 24 Oct: the refactors 8989640..e3bbd96 (one settings registry, 16 abandoned switches deleted 244a4b7, file splits, enums, clang-format; behaviour unchanged by their messages), the practice-save fitness check f5d8d76, sets s13a 946d274 (M04 6/6 PASS) and s13b 771c2c5 (5/6, PB 974, mactan 133 s; the FAIL = Mactan's script-locked dock waiting for Juni), 34ba88f (a placeholder step's marked hostiles while an escort is active are the escort's threats); 25 Oct: the UI build's in-game split detector / LiveSplit client / HUD timer / SPEEDRUN save mode (13c41f0, 7383675: A10, U21, U22) and the handoff rewrites; the re-base stays pending); before: 25a8c6b (24 Oct 2026 ~18:50, short form: the 6 later commits - review fixes, the M04 swallowed-objective fix, FREELANCER_RE facts - change no finding here; before that 648d6bd ~18:15, dirty: none: 16 watched-path commits since 1defa3f - session 12: pre_aggro (A6, built, in test on M04), blast_flank (A17), the Holstein lock retry and M10 PB 1125, dock_regain rescuing Benford stalls (A24), lethal guard, sets s12a-s12c; IDEAS A6 / A17 / A24 / A34 updated, "What it means" given a dated line); before: 1defa3f (24 Oct 2026, short-form check-up: 303 commits since f944652, 189 in watched paths - sessions 6-11 of the speedrun autopilot: story checkpoints / splits / par (6d4f5b0), segment practice, the speedrun preset (19482da), M12 at par, stale_destroy_wait, leave_on_kill / charge_cm / jumpin_burn / dock_regain / leash_follow (off); IDEAS A3-A33 re-checked, A34-A36 added); before: f944652 (23 Oct 2026, V12 the M12 lair corrected by the user's play-by-play and reference run); before: 4c7d3fe (22 Oct 2026, V11 the dense reference added; before: 78147c1, the video round: the top-3 recordings analysed against the autopilot's leg bests of LEGS.md at this commit, incl. session 4's static_pass / pressed / blind-spot / torpedo-rule commits f001197..3fa5999); before: 52047ed, 2883bd8 (21 Oct 2026); speedrun.com board as of 21 Oct 2026
Dirty paths at check: none at the start (25 Oct 2026 ~23:05; during the round another session's uncommitted `controller/src/impl/padbases.cpp`, `controller/src/impl/promptbar.cpp`, `docs/UI_HUD2_EXTENDED.md` appeared - not judged); none (25 Oct 2026 ~12:15); none (25 Oct 2026 ~07:00); before: none at the start (24 Oct 2026, 1defa3f); at the end `controller/src/impl/autopilot_frame.cpp`, `controller/src/impl/autopilot_objective.cpp` (another session's uncommitted edits, appeared during the check; not judged here)
Watch: controller/docs/AUTOPILOT.md, controller/src/impl/autopilot_*.cpp, docs/kb, docs/missions, docs/LEGS.md, docs/SPLITS.md, docs/checkpoints.tsv, docs/LEG_DOSSIER.md, HANDOFF.txt
Evidence: research\evidence\speedrun\ (commodity_prices.tsv; runs_all.json; **runs\** = the three recordings analysed: per run notes.md, legs.tsv, events.tsv, banners.tsv, scenes.tsv, economy.tsv, loadouts.md, splits.tsv, frames, scripts; METHOD.md = the shared brief; **topics\combat\** = 22 fights x 3 runs (fights.tsv, HUD readings); **topics\navigation\** = 619 entries (docks.tsv), sections.tsv = every mission leg in sections vs our autopilot runs)
Sources: [video] the top-3 any% recordings (research\videos\speedruns\, provided by the user 22 Oct 2026, decoded frame by frame with ffmpeg, never played, no audio) with the runners' / commenters' notes beside them; [page] speedrun.com API and the two written route guides; [data] HDE goods.ini x market_commodities.ini; [page] StrategyWiki / forum notes; [repo] LEGS.md, research\experience\legs\ (our leg runs' phases / logs)

## What it means for FLvibed

*Check-up 26 Oct 2026 (step 3 short form, HEAD e5c02cc; 7 watched-path commits 855745b..e5c02cc):* handoffs, the docs
sweeps and the kb's display / Reset pages only - no autopilot code, `SPLITS.md`, `LEGS.md`, checkpoints or dossier change;
the re-based "Where it stands" below holds.
- **Where it stands (re-based 25 Oct 2026, HEAD 855745b, on `docs\SPLITS.md` generated 24 Oct 23:55 and dossier M04
  through set s13b; no autopilot code has changed since 34ba88f, 24 Oct 23:20):** par **10,047 s**; **our PBs 12,867 s
  (+2,820), our golds 12,141 (+2,094)**. By PB the legs to win back are M13 +507 (one full pass only), M07 +419, M02 +258,
  M05 +251, M10 +210, M09 +195, M06 +193, M11 +191, M04 +158, M03 +123, M01b +110, M01a +106, M08 +94; M12 is at par (+6,
  gold sum -10). **Largest gold-vs-par segments and the idea behind each (A34):** M13.end +114 (city generators: A16, A31),
  M04.mactan +104 (below), M07.sinclair +96 (Tau-31, dialogue-gated: dossier M07), M02.pod +93 (Ashcroft: A22 after the
  A36 probe), **M03.end +74** (Willard pirates, one kept alive until the bombers: A7 - not in the 24 Oct list), M10.expdone
  +60 (A18), M07.fleetdone +58 (A18), M01b.end +52 (platforms only), M13.sphere +48 (A16, A31), M07.cali +47, M02.cosmo
  +46 (A21), M13.bsdown +43 (A18, A35), M06.end +43 (Leeds hole -> Leeds), M06.hole +42 (A18), M09.tekagi +42 (A16),
  M11.nygate +42, M11.osiris +41. **M04, read with the dossier:** M04.hunters is 47 s *under* par and M04.mactan 104
  over - the runners fly at Mactan from the Magellan gate and meet the hunters ~2 km from it, so their trip is inside
  their hunters segment; the pair together is **+57 s** (ours 26 + 126 vs 73 + 22), the real gap. `pre_aggro` (A6,
  built, off by default) already fights the hunters ~18 s before the banner; what remains is the trip the runners make
  first, blocked by Juni's tethers (`bounty_hunter_nag_*`: Cnd_TetherBroke juni 3000 m, not parsed yet) and by Mactan's
  dock, locked until Juni is within 3 km (set s13b's FAIL: 13 "no way (locked)" retries resting at the base until the
  mission's nag failed it - wait for a locked dock *with* the ship whose arrival unlocks it; A21, A24). The swallowed
  "Rheinland Assault Wing" step is fixed (34ba88f, the escort kept: M04.hole gold 43 = +6, was +31; M04.wing -11). The
  owner's order for the next autopilot session (handoff, 25 Oct 00:00): first a comprehensive analysis of how the
  runners ran M04, segment by segment with routes (`tools\routecmp.py` needs an M04 reference trace), then fixes by
  priority; then set s12e (blast_flank A/B), the leg gate and `pre_aggro 2500` / `dock_regain 1` into the speedrun preset;
  then the legs by gap. Switches removed since this note's 24 Oct lines (244a4b7: `leave_on_kill`, `charge_cm`,
  `jumpin_burn`, `towards_nag`, `closed_lane_ahead`, `leash_follow`, `static_hold_hullgap` and nine others, all off and
  judged losers; verdicts in AUTOPILOT.md "Removed 24 Oct 2026"). New tools from the UI build that bear on this note:
  the leg boundaries fire live in game (split detector, LiveSplit, HUD timer; A10, U22).
- **Where it stood (check-up 24 Oct 2026, HEAD 1defa3f; `docs\SPLITS.md` generated 15:44; superseded by the bullet above):** the research's leg
  boundaries became the project's story checkpoints and splits (`docs\checkpoints.tsv`, `tools\splits.py`, LiveSplit
  files; par = per leg the fastest recording, scene-free = **10,047 s**, the 10,051 below minus rounding). **Our PBs sum to
  12,933 s (+2,886 over par), our golds to 12,204 (+2,157)** - was 14,659 (+4,600) at the video round. M12 is at par
  (PB 354 vs 348, gold sum 339 < par); M01b and M07 are split now. **Largest gold-vs-par segment gaps, with the idea that
  addresses each (A34):** M04.mactan +119 (bounty hunters fought for 140 s vs 22: A6 / A21), M13.end +114 (city: A16, A31),
  M07.sinclair +96 (dialogue-gated Tau-31 objectives; closed_lane_ahead was a dead end), M02.pod +93 (Ashcroft: A22),
  M10.expdone +67 (experimental ship from inside: A18), M07.fleetdone +58 (A18), M01b.end +52 (platforms only), M13.sphere
  +48 (A16, A31), M07.cali +47, M02.cosmo +46 (scan at once: A21), M13.bsdown +43 (Nomad battleship at 0 m: A18, A35),
  M06.hole +42 (gunboat at 0-60 m: A18), M09.tekagi +42 (generators: A16), M09.patroldown +36. Combat is still the
  bulk; objective logic (A21) has landed for distance / destroy goals and the M12 / M09 nags; the dock items moved from
  the Osiris stall to moving-ship docks (A24).
  *[648d6bd (24 Oct 2026, session 12): the runners' pre-aggro of the M04 bounty hunters is built as `pre_aggro` (off,
  in test on M04; A6); M10 PB 1125 after the Holstein script-lock retry (A24); `dock_regain` rescues M04 Benford stalls
  by handing a grant not followed to the game's own dock (A24); the M09 one-shot death was our own torpedo on the
  generator's rock -> `blast_flank` (A17). M04.mactan gold 136 (+115) is still the largest gap.]*
  *[25 Oct 2026 (abf84ac, the UI-build check-up; the re-base is pending): `docs\SPLITS.md` regenerated 24 Oct 23:55 -
  PBs 12,867 (+2,820), golds 12,141 (+2,094); M04 PB 974 (s13b 771c2c5: 5/6 PASS, mactan 133 s, the FAIL a script-locked
  Mactan dock waiting for Juni), M09 724, M10 1124; the user saw "numerous more problems" in s13a / s13b and asked for a
  comprehensive analysis of the runners' M04 before more M04 work (handoff 9f45cd8). New from the UI build: the leg
  boundaries this research defined now fire **live in game** - the split detector (`livesplit.cpp` over
  `checkpoints_gen.h`, generated from `docs\checkpoints.tsv`, mirroring `tools\splits.py`), the LiveSplit client (TCP
  16834), the HUD timer and the SPEEDRUN mode written on every save (13c41f0, 7383675; A10, U21, U22). Its clock is game
  time with loads excluded - the board's real time (A10) is still not it.]*
- **The benchmark, now with splits:** WR any% 4:02:48 = 14,568 s (TM4S13; reproduced from the video to 0.07 s),
  2nd 14,707 s (OldOak), 3rd 14,771 s (SydMontague). **The three runs' best legs sum to 14,002 s (3:53:22).** Leg
  boundaries are reproducible from frames (accept = the offer popup closes; land = the docking animation's last frame;
  the runners' own LiveSplit splits match them within +-2.4 s) - the same definitions apply to our leg logs (A10).
- **The gap is ~4,600 s of mission time, and it is mostly combat.** With the room scenes the runners must watch
  removed (we skip them with Esc), the runners' best mission legs sum to **10,051 s against our 14,659 s**: combat
  ~2,300 s (A16-A20), objective logic ~600-700 s (A21, A22), dock / entry handling ~350-400 s (A3, A23, A24), M07 + M01b
  unsplit ~760 s. We are equal or faster on lanes, formation phases, the long M02 cruise, the Nomad gate and base stops.
  *[Overtaken 24 Oct 2026: +2,886 s by PBs, +2,157 by golds (first bullet); M07 / M01b split; the M12 Toledo item
  fixed (A21).]*
- **One combat rule carries most of it: hold and salvo** (A16). At every static or capital target all three runners
  cruise in, brake ~100 m short, hold still at 0-100 m (inside the turrets' dead zone, or inside the model), fire
  everything, and cruise out on the banner. Never orbits, passes or stand-offs. Our static_pass / orbit, the torpedo
  "never inside radius + 150 m" ban and target hopping at static objectives are the opposite (A17, A19); capital
  ships by class, the experimental ship from inside its model (A18); leave on the banner (A20).
  *[24 Oct 2026: hold-and-salvo is built and the static_pass / orbit and torpedo ban are replaced (A16, A17); the M12
  generators are at par, the M09 / M13 ones not (+42 / +48 gold). Two limits found in our build: the hold is refused at
  an armed target since afa1754 (the M13 Nomad battleship killed a hold 42 m off its hull - the runners sit at 0 m:
  A35), and "leave on the kill" (`leave_on_kill`) gained < 1 s per generator at M12 (A20).]*
- **Objectives are triggers, not orders** (A21): M12 straight to Toledo (8 km trigger), M04 straight to Mactan, Cosmo
  scanned at once, Ashcroft chased at cruise with engine kill to fire (A22).
  *[24 Oct 2026: M12 Toledo and the M09 Tekagi nag solved (A21: script goals, stale_destroy_wait on by default); the
  limits are nags (`Act_NagDistTowards`) that fail a run leaving a step's own way (M09 towards_nag, M07 closed_lane_ahead).
  M02 Cosmo / Ashcroft untouched. Engine kill from cruise is not measured in our build (A36).]*
- **The money layer is settled by evidence** (A1, A2, A5): no jobs in any run; early gates by micro-trades on the
  story path or depot pods, FT3 by the Balboa wreck / M03 pods, later by one trade per hop, the late game by the M10 wreck
  guns. **Any dealer transaction or cargo pickup fires the level-up** (no reload needed), so the gate never costs a dock.
- **Smaller items:** companions shed by holding cruise or by engine kill at cruise 3-4.5 km out (A3, revised); no
  waits at hole entrances (A23); the M10 Osiris dock stall and Holstein refusals (A24); cruise to stopping distance
  (A25); afterburner through cruise-disruptor waves (A26); the M11 Phoenix kill (A27); shop in the accept visit (A28).
  The instant gate jump by formation is rare (1 of 47 gate entries) and our gate shortcut already covers it.
  *[24 Oct 2026: planet rings now requested at p2 at full speed (`ring_request`, speedrun preset: Toledo -6 s), the M12
  Omicron Minor hole entered at speed; the Osiris stall is not in the M10 PB (end segment 30 s vs par 19), the open dock
  item is the moving-ship stall (M04 Benford, M06 Hood: A24). Disruptor waves: afterburner fallbacks built, the
  charge_cm / jumpin_burn variants no gain (A26). The money layer (A1, A2, A5, A28) has had no work since 22 Oct: the FT
  legs have no autopilot leg yet.]*
- **Legitimacy line:** only save/load abuse and the vanilla autosave cutscene skip are outside the project's rules;
  everything else is game-native. The runners lose ~1,900-2,170 s per run to room scenes vanilla cannot skip; HDE's Esc
  does (FREELANCER_RE: Benford docked in 2.5 s instead of ~200 s), so a board-comparable golden run on HDE starts
  ~30 min ahead on that alone - **a board comparison must say so** (A10).

## Findings: the top-3 recordings, frame by frame (video round of 22 Oct 2026)

Material: the three any% recordings the user downloaded (1920x1080 60 fps, vanilla 1.1 HUD, not HD Edition), each
with a notes file (runner's comment, YouTube questions and answers). Three agents analysed one run each (skeleton at
1 fps: objective-banner OCR, per-second scene class; leg boundaries; dealer screens; then a dense pass on the gap legs),
then two agents compared the runs by topic (22 fights x 3 runs from the HUD read every 0.5 s with the project's
game-font OCR, 10 fps strips at decisive moments; 619 dock / gate / hole / lane entries from a 2 fps speed read; every
mission leg in sections against our autopilot's best leg runs). Everything ran CPU-only at idle priority for the last
hours (the user was gaming). All numbers below are seconds; v = video time, r = run time.

### V1 Start, end, method (how to count legs reproducibly)
| Run | Official | Start (New Game click) | End (final cutscene's first frame) | Measured | Overlay | False starts |
|---|---|---|---|---|---|---|
| 1 TM4S13 (18 Sep 2026) | 4:02:48 | v=11.30 (+-0.05) | v=14,579.23 (last generator v=14,575.5) | 14,567.93 | none | none (v=4.5-10 shows the emptied Load list) |
| 2 OldOak (31 Oct 2019) | 4:05:06 | v=2.45 (timer 0.05 at v=2.50) | v=14,709.2 (timer 4:05:06.77) | 14,706.8 | LiveSplit left 480 px, game 4:3 | none |
| 3 SydMontague (2 Feb 2019) | 4:06:11 | v=10.90 (timer 0.00 -> 1.00 at v=12.0) | v=14,781.97 (timer frozen 4:06:11.35, split 0.2 s late) | 14,771.07 | LiveSplit top right, 20 splits | none (v=0-9.5 deletes old saves with the timer at 0) |
Boundary definitions (used for every run; apply the same to our logs): **accepted** = the mission-offer popup closes
after Accept (the next action is the runner's load-skip of the bar scene); **land on X** = the last frame of the docking
animation (our log: the base-enter event); **M01 end** = the first "GAME SAVED" after the pirate base; **M03 end** =
"MISSION RESULT: SUCCESS" (Juni pays in space); **M13 end** = the final cutscene's first black frame. Located from the
1 fps banner OCR + scene class (`runs\run1_TM4S13\notes.md` (a): HUD / letterbox / UI-frame templates), refined on
10 fps frames: +-1 s at 1 fps, +-0.1 s at 10 fps. Validation: run 2's 13 splits are all mission accepts (= the ends of
our FT legs) and match the event times within +-0.5 s (split 1 = 1258 s = L00 + M01 + FT1 = 1258.1); run 3's 20 splits
agree within -2.4..+2.3 s where they mean the same event (his "Mission 1" = the Pittsburgh landing, 99.7 s after
GAME SAVED; no FT splits after M06 / M09-M12; his M04 / M05 / M07 splits 7-8 s after landing). Caveat for comparisons:
runners often shop **after** the accept (CA Minor, Cambridge, Hood, Osiris), so purchases sit in mission legs.

### V2 Leg table: three runs, the best of them, and ours
| Leg | Run 1 | Run 2 | Run 3 | Best | Room scenes in best (Esc-skippable) | Best without scenes | Ours (LEGS.md) |
|---|---|---|---|---|---|---|---|
| L00 | 229 | 140 | 143 | 140 | intro, Juni bar | - | - |
| M01 (a / b) | 874 (417 / 457) | 899 (423 / 476) | 880 (409 / 472) | 874 | - | 409 / 457 | 517 / 648 |
| FT1 | 216 | 220 | 358 | 216 | - | - | probe only |
| M02 | 876 | 928 | 896 | 876 | - | 876 | 1134 |
| FT2 | 432 | 482 | 476 | 432 | - | - | - |
| M03 | 780 | 766 | 763 | 763 (IL record 836) | - | 763 | 919 |
| FT3 | 473 | 468 | 339 | 339 | - | - | - |
| M04 | 1036 | 1014 | 1019 | 1014 | Benford ~200 | 813 | 1043 |
| FT4 | 378 | 564 | 620 | 378 | - | - | - |
| M05 | 1204 | 1170 | 1198 | 1170 | Sprague 199-246 | 952 | 1346 |
| FT5 | 530 | 464 | 461 | 461 | - | - | - |
| M06 | 544 | 534 | 489 | 489 | - | 489 | 1193 |
| FT6 | 166 | 168 | 168 | 166 | Leeds M07 offer | ~5 on HDE | - |
| M07 | 1262 | 1266 | 1250 | 1250 | - | 1250 | 1816 |
| FT7 | 318 | 282 | 292 | 282 | - | - | - |
| M08 | 601 | 582 | 600 | 582 | - | 582 | 676 |
| FT8 | 42 | 44 | 50 | 42 | - | - | - |
| M09 | 796 | 830 | 882 | 796 | arch 250-266 | 530 | 852 |
| FT9 | 26 | 38 | 37 | 26 | - | - | - |
| M10 | 980 | 1040 | 1026 | 980 | Holstein 53-65 | 915 | 1498 |
| FT10 | 160 | 160 | 162 | 160 | Osiris | ~5 on HDE | - |
| M11 | 1158 | 1224 | 1252 | 1158 | prison 157-180 | 979 | 1196 |
| FT11 | 126 | 126 | 128 | 126 | Osiris | ~5 on HDE | - |
| M12 | 362 | 350 | 348 | 348 | - | 348 | 627 |
| FT12 | 246 | 248 | 249 | 246 | Toledo | ~5 on HDE | - |
| M13 | 752 | 702 | 686 | 686 | - | 686 | 1194 |
| **Total** | **14,568** | **14,707** | **14,771** | **14,002** | | mission legs **10,051** | mission legs **14,659** |
*[Dated 24 Oct 2026: the "Ours" column is LEGS.md as of 22 Oct. Current PBs (`docs\SPLITS.md`, par in brackets where it
differs from "best without scenes" by rounding): M01a 515, M01b 567, M02 1134, M03 886, M04 990 (812), M05 1203, M06 682,
M07 1669, M08 677, M09 724, M10 1171 (914), M11 1169 (978), M12 354, M13 1192 - sum 12,933.]*
Run totals (scene class, run 1): 10,283 space (2,686 combat), 3,518 letterboxed scenes, 267 base UI, 159 loading, 78
menus. Room scenes watched (Esc-skippable on HDE, checked frame by frame in run 2: all 20 are base interiors, none a
space cutscene): ~1,950 (run 1), 1,947 (run 2), 2,174 (run 3); space cutscenes ~845-1,008 per run (nobody skips them).
The 2020 playthrough (the user's casual 4K play, per-mission videos 700-7,043 s incl. exploration) is no longer the
time reference; it stays the reference for style and hull (memory playthrough-time-benchmark).

### V3 Where the autopilot loses the ~4,600 s (`topics\navigation\sections.tsv`)
| Cause | ~s | Largest sections (gap vs the best runner) |
|---|---|---|
| Combat | 2,300 | M06 Glorious defence +403, M13 Dyson city +293, M10 experimental battleships +255 (run 2: Bruchsal -> Osiris 280 vs our 638), M05 fight after Baxter +179, M06 gunboat +148, M13 Nomad battleship +120 |
| Objective logic | 600-700 | M12 "destroy all Nomad ships" obeyed (runners fly to Toledo: the trigger is within 8 km) +161; M04 Freeport-4 detour (runners go straight to Mactan; the bounty hunters meet them there) +123; M02 Ashcroft (runners chase at cruise + engine kill; pod in 18-30 s) +102; M02 scan (Cosmo scanned at once vs our 69 s hold) +48 |
| Dock / entry handling | 350-400 | M10 Osiris: granted at 5 m, then 91 s before the bay took us (runners 15-18 s) +126; waits at hole entrances for an NPC to follow (Sigma-13 / Tohoku 10-13, Leeds hole 22, Chugoku 46, Colorado gate 11); Holstein refused twice ~18; queued behind Juni at CA Minor 14, Magellan ~9 |
| Unsplit | 760 | M07 566 and M01b 191: their best runs have no FLController.log (rerun with logs to split) *[resolved 24 Oct 2026: both split by checkpoints in `docs\SPLITS.md`; M07 PB +419 over par, its biggest segments Tau-31 +96 / fleet +58 / Cali +47 (gold); M01b +110, the platforms segment +52]* |
*[Check-up 24 Oct 2026: this table is the 22 Oct picture. Overtaken rows: M12 "destroy all Nomad ships" (fixed, M12 at
par), M06 Glorious +403 (now +25 gold, M06 PB 682), M10 experimental +255 (now +67 gold), M10 Osiris dock +126 (not in
the PB; the end segment +9 gold). The current ranking is the first bullet of "What it means" (IDEAS A34).]*
Equal or faster: lane rides; formation phases (M08 escort 116 vs 166: the runners form up early and wait); the 30 km
M02 cruise to the NY gate (102 vs 118-126); the M13 Nomad gate; base stops once the runners' scenes are removed. The M06
race is level (runners 68-73 s at cruise from GO; our flrace best 74.1): **the M06 gap is entirely outside the race**
(run 2: Hood -> Glorious 121 vs 495, Glorious -> Leeds 181 vs 429).

### V4 Combat (`topics\combat\notes.md`, fights.tsv: 22 fights x 3 runs)
Rules seen in all three runs, each with timestamps there:
1. **Static targets (generators, satellites, platforms): cruise in, brake ~100 m short (1.2-1.3 s, ~90 m from cruise:
   10 fps strips), hold still at 0-100 m, fire everything, leave on the kill.** M09 arch ~9 s per generator (36-40 for
   four; run 2 also from 185-730 m with guns along the arch), M13 barrier 63-87 for four, M12 72-101 for three, M08
   platforms 2-6 each. No run orbits, makes passes or stands off at 600 m.
2. **Between targets: afterburner under ~1 km, cruise above**, also inside the lair and the Dyson sphere; the M13 city
   crossed at 300 m/s with its ~20 fighters never fought, satellites killed from 0-250 m (run 2 from inside each model).
3. **Ignore fighters at a static objective**; pay their fire with batteries (run 1 at the barrier 22 -> 4, all 28
   nanobots kept) or nanobots when no batteries are carried (run 3 14 -> 1).
4. **Needed capital ships: 0 m or inside the model.** Experimental ship (M10): all three park inside the hull and take no
   damage there (30-36 s; run 2 kills the engine at cruise and coasts into the gantry at ~265 m/s, v=11716-11764); the
   damage comes in the last cruise kilometre outside. Nomad battleship: 0 m, 21-35 s, batteries 27-29 -> 0-3. Rheinland
   gunboats 0-60 m. **Exception, the Rheinland cruiser (Nestor):** at 0-60 m runs 1 / 2 lost 28 % / 43 % hull; run 3 at
   165-455 m lost nothing.
5. **Capital ships the objective does not need are cover:** M07 Schiller is hugged while the fighters and gunboats die;
   run 1 leaves it alive in the same ~108 s the others take to kill it (whether our build's script needs Schiller dead
   is open). Run 2's M07 fleet: two fighters, Bietigheim at 51 m, Burghausen at 197 m, Nestor at 663 m, Schiller at 0 m,
   thruster held (180-200 m/s), every bot and battery spent.
6. **Ashcroft (M02): engine kill at cruise 110-230 m behind him, fire while coasting at cruise speed; he ejects within
   6-10 s** (all runs).
7. **Leave on the banner:** cruise out the moment the objective changes (M10 Sigma-13: 8 heavy fighters in 57 s,
   nearest first at 80-200 m, the battleships Fenrir / Jormung ignored, out on BREAK OFF).
Consumables: batteries go to capital fights, to holding the shield against the M10 heavy fighters and to soaking fighter
fire at the M13 generators; nanobots only after a real hull drop (run 2 carries none until M10); CMs only against
Rheinland missiles (never Nomads); missiles / torpedoes in salvos from 0-100 m at static / capital targets (never from
range: the commenter's "> 1000 m after cruising in" idea is used by nobody, A29). Correction: the HUD counter order is
nanobots then batteries (run 1's first notes had them swapped).

### V5 Navigation, docks, companions (`topics\navigation\notes.md`, docks.tsv)
1. Cruise at once: median ~9 s from control to 290+ m/s after every launch and jump exit (5 s charge + ramp).
2. Afterburner (~199 m/s) for short hops, cruise for anything longer; lanes entered at cruise or afterburner (the
   ring takes over in the last 1-2 s; ours matches).
3. Holes at cruise with no wait in 26 of 44 entries; the stops are scripted waits shared by all three runs (Tau-29,
   Tau-23 / Kyushu, Chugoku 16-17). Free gates at cruise; busy gates queued (Omega-3 12-13 in all runs, Alaska / New
   York 15-24, Nomad gate 15). Scripted waits all runs share: Pittsburgh ring 16-24 (run 3 ~50 there), Willard 18-28,
   Kyoto after M09 18-26.
4. Planet rings straight in at 299 m/s; stations / battleships at cruise until ~2 s out.
5. **Instant gate jump by formation** (run 3, YouTube 43:20 = v=2596-2600.5, the California gate): targets "Navy -
   Escort 1 Lt. Larsen" already in the gate mouth, closes 1,356 -> 925 m at cruise, rear view from v=2598, enters the
   tunnel at 2600.5 at 299 m/s with no dock request or queue. **The only case in 47 gate entries**; pays 10-20 s at a busy
   gate only (3-5 per run: ~30-75 s per campaign). The project's formation queue shortcut is the same mechanism.
6. **Companion shedding before single-entry docks / gates** (resolves A3): either hold cruise all the way in (run 1 at
   CA Minor: Juni falls from 168-345 m to 1.3-1.7 km; Magellan) or engine kill at cruise 3-4.5 km out and coast in at
   270-299 m/s (run 2 CA Minor v=3182 ~3.5 km, Mitchell v=12778; run 3 Magellan v=4497 at 3.1 km, YouTube 1:15:00): the
   companion falls 1.5-2.6 km back and the player docks first. At Mitchell the bay still made run 2 wait ~10 s 100 m
   out. Fischer (M10) is left 50-54 km behind just by taking the Frankfurt lanes. Run 2 at Mactan (M04): no shed needed,
   the bounty-hunter fight separates Juni.
7. **Cruise disruptors:** run 3's M09 Chugoku-hole wave (v=10594-10790; the runner: a bug spawning extra fighters with
   cruise disruptors) cost ~3 min: a 126 s fight, then 70 s on the afterburner at 198 m/s to the hole.

### V6 Money route and the level-up trigger
| Gate | Run 1 (TM4S13) | Run 2 (OldOak) | Run 3 (SydMontague) |
|---|---|---|---|
| FT1 +3500 | M01b reward + 1 depot pod (16 H-Fuel) sold in the Patriot trade | **Water Manhattan 15 -> Pittsburgh 30 x20, Boron Pittsburgh 120 -> Manhattan 155 x20** on M01's own path; LEVEL 3 on the Boron sale (v=1245) | Rhino at Pittsburgh, Fort Bush depot: LEVEL 3 in space at the depot (v=1293) |
| FT2 +5200 | depot again (25 H-Fuel) sold at CA Minor 425 (+10,625) | **buy 20 H-Fuel at Missouri -> LEVEL 5 -> sell back** (v=2196-2208); Defender | LEVEL 5 in space after Missouri (v=2390) |
| FT3 +7000 | **4 M03 artifacts x1,282 + Balboa wreck 20 Cardamine x1,500** (= 35,488) | **Balboa wreck** (20 Cardamine + 2 Pyros Type 2 + Pyros turret) tractored in the Willard ice field: LEVEL 7 in space (v=3512); 12 artifacts tractored (2 sold at Willard 270, 10 at Manhattan 1,282) | **M03 transport 2's pods**: +46.6k at Manhattan (YouTube 50:29 = v=2990-3030) |
| (M04) | **80 H-Fuel Manhattan 360 -> Mactan 516 = +12,480** | Pyros sold at Mactan | - |
| FT4 +10000 | Optronics Leeds 980 -> Cambridge 1,330 **x80 = +28,000** | Optronics x30; Crusader at Norfolk | Crusader at Norfolk |
| FT5 +16000 | Diamonds Cambridge 879 -> Leeds 1,275 x30, Mining Machinery x30 | Diamonds x35, Mining Machinery Leeds 350 -> Hood 465 x40; LEVEL 11 at Leeds | - |
| FT7 +36000 | Gold Hood 322 -> New Tokyo 1,104 x35 (+27,370) | Gold x35 + M07 4,000 | - |
| FT8 +30000 | Light Arms 630 -> 770 x45; LEVEL 16 on the sale | Light Arms x35; **sell + buy back torpedoes** (v=9966-9968) | - |
| late | 2 Diamondback guns (Sigma-13 wrecks, 34,698 each) pay the Anubis | wreck loot sold at Bruchsal +138,792 | Onyx x2 + Diamondback sold at the Osiris: 15,050 -> 186,402 |
**The level-up fires on the transaction or the pickup itself** (LEVEL banners at sales, buys and in space after
tractoring), not on a later dock: the guides' "undock + load autosave" was never needed (run 1); run 2's two loads
(v=2388 Manhattan: a story step followed at once; v=2523 after the California jump: no visible effect) are the only
load-as-trigger cases. No job board in any run. Run 3's Cosmo kill for artifacts (M02, v=1665-1690) was tried and
abandoned (the commenter's method: wait for the objective update, kill with minimal damage, up to 7 of 20 drop, scaled
by net worth).

### V7 Ships and loadouts (`runs\*\loadouts.md`)
| Run | Ship path (where) | Notes |
|---|---|---|
| 1 | Starflier -> **Patriot** (Manhattan, 7,600, FT1) -> **Rhino** (Manhattan, 9,170, FT3, hold 80) -> **Cavalier** (Cambridge, 25,660, FT4) -> **Crusader** (**Battleship Suffolk**, 35,350, FT5) -> **Barracuda** (**New Tokyo**, 71,060, FT7) -> **Anubis** (Osiris, 1,100 after the trade-in) | Javelin at Pittsburgh; Slingshot + Adv. Stunpulse for M03; Starkiller torpedo launcher (Hood) + Imp. CM dropper from FT5; Debilitator for M11, Reaper turret + Catapult + Driller mines for M12-M13 |
| 2 | Starflier -> **Defender** (Missouri, FT2) -> **Crusader** (**Battleship Norfolk**, FT4) -> **Anubis** (Osiris, FT10) | batteries / bots sold at the start; Slingshot (Manhattan), CA Minor kit; torpedo launcher + CM dropper at Leeds FT5; **old kit sold in the Cargo Transfer screen to cut the ship price** (and anything left in the dealer column is sold on leaving) |
| 3 | Starflier -> **Rhino** (Pittsburgh, FT1) -> **Crusader** (Battleship Norfolk, FT4) -> **Anubis** (Osiris) | 11 Javelins right after the M01 accept; Kyoto refill to 50 of every ammo; Debilitator sold back before M12 |
Answers to section 9's open questions: the Crusader is sold at Battleship Norfolk (runs 2, 3) and Battleship Suffolk
(run 1) in vanilla, and the Barracuda at New Tokyo (run 1) - vanilla markets differ from the HDE data, so the route's
purchase plan must come from the **test install's** markets, not the guides. SydMontague on the Rhino: "the larger
cargo space ... I'd prefer a Defender for the fighting part", and with the M03 transport loot "a future route may
actually use a Defender in Liberty" - run 2 did exactly that.

### V8 Skips and save / load
- Load operations: run 1 46 pause / menu visits (~140 s incl. loads; none used as a gate trigger or after a death),
  run 2 ~40, run 3 26 (103 s). Uses: cut the bar scene after the Accept, cut landing / room scenes, once the launch
  scene, the M13 softlock recovery, two story triggers in run 2. Outside the project's rules; on HDE the same scenes end
  with Esc (~0.5-1 s each).
- What vanilla cannot skip (no autosave behind the scene) and the runners watch in full: offer scenes up to the accept,
  Benford / Van Pelt (~200), Sprague (199-258), Leeds Tobias (165), the Tekagi arch interior (250-266), Holstein pad
  (53-65; "no cutscene skip" there so Fischer docks after), Osiris x2 (124-161), the prison break (157-180), Toledo
  (244-250). Our autopilot already Esc-skips room scenes, so our leg bests do not contain them; the remaining check is
  that every one of these is cut and the story still advances (A10 note; FREELANCER_RE "room story scenes").
- Space cutscenes nobody skips: M01 enter 77, M02 enter 55, Cosmo 27, M05 chase 53 + torpedo 57, race intros 75, M07
  fleet 68, M12 Osiris 41, Benford attack, Sprague explosion, Bruchsal escape, Walker's death (~845-1,008 per run).

### V9 Softlocks, hiccups, bugs
- **M13 Osiris softlock (run 1, v=14123-14159, 36 s):** after the load-skip of the Osiris landing and a restock, "FOLLOW
  ORILLION TO THE JUMP GATE" is wrong; the loaded save sits before the dock; re-dock, load, restock, relaunch fixes it.
  Inference: the load-skip restores a save taken before the story step is armed - a save/load artefact our no-load rule
  should be immune to (still log the M13 Osiris dock state, A7). Run 2 and run 3: no Osiris softlock in M10 or M13.
- **Balboa hiccup (run 1, v=3436-3450, ~14 s):** the Patriot's hold full, 3 H-Fuel jettisoned to take the last Cardamine
  (the runner's note "not dropping H-fuel on the way").
- **M09 Chugoku-hole fighter wave** (run 3, ~3 min; V5.7). **Pittsburgh ring wait** (run 3, ~50 s).
- **M11:** run 2 parked ~32 s in the Alaska gate waiting for Walker's scene (v=13088-13120) - the commenter's LNS-Phoenix
  kill (form up after the Mitchell escape, engine kill at cruise after the first Nomad wave, kill it: Walker's scene
  starts at once, the gate is still reached) would save that (A27); no run used it.
- No deaths and no mission failures in any of the three runs.

### V10 Corrections to the 21 Oct findings below
- Section 1 / 4 "FT1 = Rhino + 64 H-Fuel from the depot": one route of three; run 2 did FT1 with micro-trades and no new
  ship, run 1 with one pod; all clear +3500 in 216-358 s.
- Section 4 "the M10 wrecks": they carry class-10 guns (Onyx, Diamondback, 34,698 resale each), the late game's money.
- Section 5 "load autosave as a state trigger" / section 7 item 2: the transaction or pickup fires the level-up in all
  three runs; the load was not needed (run 1) or used once without visible effect (run 2 v=2523).
- Section 7 item 3 "engine kill at ~4.5-5.5 km": measured 3-4.5 km at cruise, or no engine kill at all (holding cruise
  sheds Juni too); Fischer is shed by the lanes, not at Holstein.
- Section 7 item 11 / A8 "M06 race": settled - cruise from GO, 68-73 s in all runs, our race 74.1 s; the M06 gap is
  outside the race.
- Section 9 "Battleship Norfolk sells the Crusader" / "Barracuda at New Tokyo": confirmed for vanilla (V7).

### V11 The dense reference: HUD traces, motion traces, calibration (22 Oct 2026, evening)
Built by two agents (`research\evidence\speedrun\reference\`; README there = fields, accuracies, regeneration):
- **HUD traces** `trace_run1..3.tsv` (10 samples / s over every leg, ~148k rows each; speed, thruster energy - the
  vanilla HUD's left box, not throttle -, cruise state / charge, own bars, nanobots / batteries, every launcher's ammo,
  red contacts, target name / distance / bars, banners), `events_dense_run*.tsv` (launches, CMs, consumables, cruise,
  afterburner, target changes, kills, entries), `sections_dense.tsv` (970 sections). Hand-checked 52-54 frames per
  field group per run: runs 1 / 2 mostly 94-100 %; run 3's big-font numbers weaker (Catapult 73 %, mines 81 %, speed
  88 %; a rework is running); in-view target labels weak in runs 2 / 3 (the contact-list readings are reliable).
- **Motion traces** `motion\motion_run1..3.tsv` (view yaw / pitch / roll from RAFT optical flow of the background,
  calibrated on the game's steering law to 0.92-1.02; cursor and target bracket; the maneuver bar's autopilot state;
  30 fps around decisive moments) and `motion\behaviours.md` (rules with frame evidence).
- **Lookup:** `python ref.py <leg> [<section>]` (the OCR venv) prints the three runs side by side in < 1 s;
  `--frames <run> <video_s> [<s>]` cuts a 10 fps sheet.
- **Calibration ledger** `reference\calib\CALIBRATION.md`: the user's readings of the footage outrank automated reads
  (memory footage-reading-calibration). Key corrections: the runners hold generators near contact with a slow one-way
  slide, never ram; the HUD target distance is a surface distance (0-M against a cruiser's hull); "MOUSE FLIGHT" is a
  steering toggle, not an autopilot state.
Rules found (frame-backed; the autopilot side: `controller\docs\AUTOPILOT.md` hold-and-salvo, IDEAS A16-A30):
1. **Static targets:** fired upon without pause from the moment they are targeted (launchers start ~1 km out); the hold
   at HUD 60-100 m (run 1) / 35-60 m (run 2) off the surface, the cursor on the target 91-96 % of the time; still (M12,
   all runs) or a slow one-way slide 3-11 m/s (~5 deg/s, always to the ship's right at the M13 barrier, never reversed);
   every launcher on cooldown (~2 s, synchronised salvos), mines every 1-2 s only while holding, a failing shield
   refilled with ~9 batteries at once; calm holds beat weaving (run 1 barrier 74 s vs run 2's 87 s). Cycle 18-22 s per
   generator: cruise line from 2-5 km, cruise cut 150-200 m short, ~1 s stop, turn 50-60 deg/s for 0.5-1.5 s on the
   kill overlapping the afterburner, afterburner under ~1 km.
2. **M10 experimental ship:** aimed broadside at the hull's middle, through the open lattice at 290-300 m/s, contact
   stops the ship inside the core, held 0-11 m off the hull for ~40 s firing everything.
3. **Nomad battleship:** 0 m, speed pulses 0 <-> 150-200 m/s every 3-8 s, 7-10 batteries at once per shield failure.
4. **They leave on the kill, not the banner** (M05: cruise charging the instant the last gunboat dies, 19 s before
   the objective changes; M10 ~5 s before "Break off"). *[Our test, 24 Oct 2026 (`leave_on_kill`, M12 generators, sets
   s11o / s11p): leaving a static target our munitions in flight will kill saves only 0.3-0.9 s there - the runners' gain
   is against the banner, not the kill; the M05 / M10 cases are untested (A20).]*
5. **Cruise charging is a hidden cost:** 117-142 charges per run, 550-680 s; flight modes per run ~4,000-4,300 s
   cruise, 1,100-1,600 s afterburner, engine kill only 30-110 s.
6. **The game's own autopilot** (Goto / Dock / Formation) flies ~27 / 18 / 21 % of the non-lane space time - formation
   behind leading NPCs, dock approaches, long free legs; everything that decides time is hand-flown.
7. **Damage is taken when the clock matters** (M06 Glorious: run 1 ends at 0.70 hull, run 2 at 0.37, nanobots used);
   **consumables refill mid-fight** from tractored loot; menus opened while cruising overlap screen time with travel.
8. **Docks / gates / holes:** lined up 5-15 s before entry (0.1-1.3 deg/s), the last turn > 10 deg/s 1-3 s before
   docks / gates, none in the last 15 s at holes. **Dogfights:** flat snaps without banking, a lost target re-acquired
   in a median 0.8-2.6 s, attackers ignored while on an objective; the M10 heavy fighters ~1 missile / torpedo per
   kill, ~6 s per kill at a median 220 m, CMs in pairs only when a lock appears.
9. **M08 "Fly to the Waypoint":** all three cruise at the Honshu hole, the navmarker 30-40 deg off to the left; the
   banner flips 40-44 s in, 5.0-5.5 km from the hole (the script: Ozu within 9 km of it).

### V12 M12 lair: corrected and given a benchmark (23 Oct 2026, session 6)
1. **The hops between the lair's generators are cruise, not afterburner** (CAL #10): the automated phase codes read
   "charge dropped after 3-5 s, then afterburner 198 m/s"; the frames (run 1 v13372-13388) show the charge running to
   completion, and the user's play-by-play: charge on the kill, cruise most of the way in a mostly straight line,
   smoothly round the geometry, countermeasures pre-emptively against the Nomads' cruise disruptors.
2. **The core** (run 1 frames v13447-13463 at 4 fps + the user's runs): the charge is started just before the entrance
   and survives the airlock; a rotating six-spoked wheel ("turbine", 36 deg/s, at z ~400 - the model's Rev joint and
   keyframes, `docs\missions\M12.md`) is crossed through a gap at full speed; a ~90 m hole on the axis is flown dead
   centre; the power cell is tractored in passing at ~300 m/s; out of the exit straight at the hole.
3. **The user's own reference run** = the M12 opening's time to beat, **~202 s** from control to 500 m of the Omicron
   Minor hole (`docs\LEG_DOSSIER.md` M12); its generators (74.2 s) beat all three runners (80-82).
4. Countermeasures: the user ran out (50 -> 0 by the hole) against persistent cruise disruptors - budget them.
5. *Check-up 24 Oct 2026:* the M12 leg is at par (PB 354 vs 348; golds -10 below par). Kept from this: cruise hops
   (`hop_cruise_hold`), the core by the user's try-3 line (`ref_line`, speedrun preset; 12/12 vs 9/12 PASS), the charge at
   50 % at the core entry (`airlock_charge_at 0.5`), the CM budget (17fd2b2). Omicron Minor -> Toledo: 65.2 s without a
   charge reset (par 63.3), ~5 s per reset by the Nomad disruptors; CMs while charging (`charge_cm`) did not prove out.
   The user's ~202 s (control -> 500 m of the hole) is still unbeaten (the PB: 231 s from the M12.control checkpoint to the hole checkpoint, par 217 - boundaries differ
   slightly from the user's 500 m mark).

## Findings (agent report of 21 Oct 2026)

### 1 Summary
- One board: speedrun.com/freelancer (6 players, 28 verified runs, moderator SydMontague). Full-game `any%` WR
  **4:02:48 by TM4S13, 18 Sep 2026** (real time, New Game -> last generator's cutscene); second 4:05:06 OldOak 2019,
  third 4:06:11 SydMontague 2019. Save/load is allowed; no rule mentions mods or HD Edition; "adoxa's HUDShift" is the
  one listed patch. 13 per-mission IL boards exist but hold a single run (Mission 3, 13:56). No splits are published.
- The whole community route is two public guides (SydMontague's raw notes, HotSliceOfJesus's annotated version). Its
  money engine is **free loot, not jobs**: the DSE H-Fuel storage depot near Fort Bush (4 pods x 16 H-Fuel, 360 cr
  each at Manhattan) is emptied twice, the M03 convoy's artifact pods are shot open (1282 cr each at Manhattan), the
  M10 wrecks are looted; plus 3-4 single-hop trades carried along the story's own path (Optronics Leeds -> Cambridge,
  Diamonds Cambridge -> Leeds, Mining Machinery Leeds -> Hood, Gold Hood -> New Tokyo, Light Arms New Tokyo -> Kyoto).
  Job-board missions are never used.
- Time is saved by: never waiting for wingmen (take lanes / gates the instant the script allows), **engine kill at
  ~4.5-5.5 km to shed Juni before docking** (so the companion does not take the single bay first), attacking neutral
  ambushers before they turn hostile, skipping every base cinematic (vanilla: Esc -> load the latest autosave; HDE:
  Esc skips directly), and not shopping mid-mission. Space cutscenes cannot be skipped by anyone.
- Known softlocks runners route around: M03 Willard (killing all Rheinlanders before the bombers spawn), M10 Osiris
  (dock only after the purple waypoint appears), M13 Osiris (unexplained, cost the WR ~2 min), M04 "Van Pelt" save.
- Against FLvibed: the one comparable number (M03 IL 836 s vs the objective mode's 881-950 s from the same point)
  says the autopilot is already within ~10 % of a casual IL; the WR pace (~14,570 s for 13 missions + 8 free times +
  all base time) is roughly 1.5-2x faster than the current sweep pace extrapolated. The biggest gaps are the
  between-missions layer (not built), companion-shedding before docks, and fighting only what the script requires.

### 2 Boards, categories, rules, records
Source: speedrun.com REST API (`downloads\runs_all.json`, `leaderboard_any.json`, `lb_level_*.json`,
`lb_sectioned_*.json`) `[api]`; game page and guides read in the browser `[page]`.

| Board / category | Timing rule (paraphrased from the rules text) | Best verified runs |
|---|---|---|
| **any%** (full game, `w20wgrjd`) | Starts at New Game; stops when the final cutscene begins after the last generator is destroyed. Save/load allowed; the savegame folder must be emptied before each run except `restart.fl`. Real time, video required, no ms, PC only, no emulators. | 1. **TM4S13 4:02:48 (2026-09-18)**, 2. OldOak 4:05:06 (2019-10-31), 3. SydMontague 4:06:11 (2019-02-02), 4. HotSliceOfJesus 4:13:00 (2019-10-10), 5. fennelwhyth 4:41:35 (2020-05-22), 6. EmeraldStar82 5:05:11 (2018-03-15) |
| **Sectioned** 1-4 | New Game -> landing on Leeds after M04 | no runs |
| **Sectioned** 5-9 | undock from Leeds before M05 -> landing on Kyoto after M09 (prepared save must be reachable by normal play) | no runs |
| **Sectioned** 10-13 | accepting M10 from Hakkera -> final cutscene | no runs |
| **Levels** Mission 1..13 (per-level any%) | starts when the mission-accept autosave loads; ends M1 "Game Saved" after the Rogue base, M2 land Missouri, M3 Juni's payment, M4/5/6 land Leeds, M7 land Kyushu, M8/9 land Kyoto, M10/11 land Osiris, M12 land Toledo, M13 final cutscene. Prepared saves allowed if reachable by normal play; ships / equipment must respect rank and access; saves from glitch-aborted missions may NOT be used for access or funds. | only **Mission 3: HotSliceOfJesus 13:56 (836 s), 2019-04-19** ("practice/IL run, not really optimal ship") |

Other facts from the 28 run comments `[api]`: every WR-pace run reports deaths or softlocks (TM4S13 WR: "hiccup on
Balboa from not dropping H-fuel on the way, and still softlocking on Osiris mission 13"; the 4:05:29 run: "end
softlock took about 2 minutes to fix"; SydMontague 2018: "Willard softlock, Hood problems, a cutscene skip didn't
work"; HotSlice: "full screen and higher framerate improved speed", "IMG glitch led to being wrecked entering the Nomad
homeworld", "money was the main problem: 32 H-fuel, 4 artifacts, low missiles and torpedoes"). All runs: platform PC,
`splits: null`, no in-game time. Resources page: patches "adoxa's HUDShift" only; no splits, tools or saves. Forum: two
8-year-old threads (Discord link, "Interested in speedrunning"). No HD Edition ruling anywhere; runs are on the
original game by the runners' comments (`[assumed]`: the guides mention vanilla-only tricks such as the autosave
cutscene skip). Not found anywhere else: no GameFAQs / Reddit / The Starport speedrun threads; the only other
"speedrun"-tagged material is unrelated games.

### 3 Per-mission route table (M01a..M13)
"Route" = what SydMontague's notes `[page oeyn3]` and HotSliceOfJesus's guide `[page cq15x]` do (paraphrased).
"Class" for the autopilot: **N** = game-native and automatable with what FLvibed already has (goto / dock / lane /
formation / objective mode), **H** = relies on human aim, reflex or risk judgement (the fight matrix / duel logic
must cover it), **G** = glitch, save abuse or outside the project's rules. Times: only the M03 IL exists; everything
else is the project's own measurement (`[repo]` AUTOPILOT.md sweep state, 21 Oct 2026) or unknown.

| Mission | Best known time | Runner route and tactics | Dangers / softlocks | Class |
|---|---|---|---|---|
| Pre-M01 (New Game) | WR total 4:02:48 | Quit to main menu after the first loading, load the autosave, create a manual save (for cutscene skips). Cutscene-skip the bartender, Juni + diplomat; accept M01; **sell batteries & nanobots, buy a Javelin launcher + ammo** (launcher 500, ammo 25 each `[data]`). | - | N (shop), G (vanilla save/load skip; HDE has Esc) |
| M01a | sweep ~515 s from Juni's offer `[repo]` | Kill the Order wing, go to the Fort Bush lane the moment they are dead, dock asap; form up with USV Brandt; kill the Rogues; take the lane as soon as Brandt is within 2000 m of it (HotSlice: wait until the distance readout changes from ~1800 to ~1600 m, then it is safe). Dock at Pittsburgh. | Lane too early = fail (the runbook's TL146 race) | N: the runbook already encodes the race; the "Brandt within 2000 m" distance test is a direct native trigger candidate |
| M01b | sweep ~650 s `[repo]` | Load the savegame on landing (cutscene skip), undock asap, form up with the LSF fighters, defend XT-19; fly toward Manhattan staying within 2.9 km of the waypoint until the Beta-4 comm; at the comm turn ~120 deg left toward the dark particle clouds (the base spawns there); rescue Beta-4; **shoot only the two weapon platforms**, then head for Pittsburgh. | None noted | N (the kb page already says platforms only) |
| Free time 1 (+3500) | - | Dock Pittsburgh: **buy Rhino (hull 7900 `[data]`, hold 80) + a Stunpulse turret**, undock. Loot the DSE H-Fuel depot near Fort Bush (`Li01_14_depot_Hfuel`, 6416,100,-53784; 4 cargo pods x 16 H-Fuel `[data]` loadouts.ini). Fly to Manhattan, sell H-Fuel (360 each `[data]`) + "crap", buy ammo, Justice guns / turrets, accept M02. | Shooting the pods of a lawful faction's depot: the notes say pods are shot freely only while an enemy is within 2.4 km, "otherwise only 2" (see M02) - a reputation rule `[assumed]` | N if pod-shooting is allowed by the project's rules (it is in-game loot, not a cheat); H for the rep judgement |
| M02 | sweep ~940 s from scene 8 `[repo]` | Cutscene-skip Lonnigan. Fly to the Colorado gate and use it - **make sure King follows through the lanes (he is required for the Unity trigger)**. Scan the DSE fighter the moment he is visible (select + scan-cargo key works before normal range). Kosmo: ~3 shots drop most of his shield without failing; start firing after King says "engines"; take the lane as soon as "Game Saved" appears (King must come). Rogues at Pueblo -> fly toward Ouray asap. At the asteroid field weaken the last Rogue (shield down) before cruising to intercept Ashcroft; **engine kill (EK) to catch Ashcroft**. Back in NY make sure King comes to Fort Bush (required for the Rogues trigger); you may attack the Rogues before they turn hostile (one free kill). SydMontague: aggro the Rogues and kite them to the H-Fuel depot, shoot all pods while an enemy is within 2.4 km, otherwise only 2. Dock Missouri, cutscene skip. | Cosmo destroyed = fail; Ashcroft's pod lost = fail (runbook) | N (scan early, lane at the save, formation), H (kiting hostiles to the depot) |
| Free time 2 (+5200) | - | Sell 16 of 48 H-Fuel at Missouri (336 `[data]`); Fort Bush depot again; Manhattan (sell 360), cutscene skips; fly to California Minor, load the autosave after jumping; **buy Slingshot (1830 `[data]`), class-3 shield, Heavy Thruster**; accept M03. | - | N |
| M03 | **IL 13:56 = 836 s** (accept autosave -> Juni's payment) `[api]`; objective mode 881-950 s from step 5 `[repo]` | Undock, fly to the Magellan gate and to the transports' spawn (left of the Rheinland nebula); stay close until the 3rd buoy; Outcasts: kill ~4-5 of the first wave (they spawn to the left), then **shoot the transports' cargo pods to steal the artifacts (transport 2 first; transport 1 destroyed = fail)**; go to Willard the moment the last Outcast dies; dock, sell H-Fuel (444 `[data]`), undock; straight to the CA Minor ring; Valkyries; **EK on the approach to shed Juni, dock**; bar, skip; undock, Walker, Willard as soon as the waypoint is acknowledged; kill all Rheinlanders. | **Softlock: killing every Rheinlander before the bombers spawn.** HotSlice: kill most, then cruise toward the bomber spawn and EK-intercept them before they reach Willard. | N (pods = loot targets; the kb's "gunboats count" rule), H (the bomber intercept timing) |
| Free time 3 (+7000) | - | Manhattan: sell artifacts (1282 `[data]`) + crap, buy H-Fuel (360) and ammo; **undock, load the autosave** (forces the story's level check), accept M04. | - | G (load) -> N alternative: find the native event that re-evaluates the gate (section 7) |
| M04 | objective mode 1002-1059 s; user 2020 1444 (1359 scene-accounted) `[repo]` | Stay close to the West Point lane, dock with it asap, **make sure King and Juni come**; thrust through West Point, lane to Ithaca (King + Juni required for the Benford trigger); Benford: "do whatever you want except dying - your position is reset after the cutscene"; **Magellan gate asap, EK at ~5-5.5 km to shed Juni**; attack the bounty hunters while still neutral; dock Mactan (do not repair if going for the Tobias skip); Rheinlanders; Leeds hole; dock Leeds; **repair, press F1 until the dealer menu opens = Tobias cutscene skip** (or the normal skip, but never load the Van Pelt save). | The Van Pelt autosave resets progress; Mactan turns hostile with Outcasts within 5 km `[page StrategyWiki M5]` | N (the survival stance already fights nothing it need not), H (EK timing) |
| Free time 4 (+10000; the mission pays +25000 at Leeds' Equipment room `[repo]` M04.md) | - | Leeds: **buy 80 Optronics, sell 45** (keeps 35; Leeds sells at 980 `[data]`), buy 35 H-Fuel (300), 2 Adv. Stunpulse, Imp. CM. Fly to **Battleship Norfolk (Br03 Cambridge system `[data]`)**: sell H-Fuel (396), **buy Crusader (hull 22000 `[data]`; HDE data also lists it at Planet Leeds and the Hood)**, class-4 shield, another Slingshot, 2 Rippers / class-4 guns + ammo. Cambridge: sell Optronics (1330: +350/unit), buy Diamonds (880), accept M05. | - | N |
| M05 | objective mode 1225-1304 s `[repo]` | Follow the waypoints to Freeport 1; after the lane fly directly to Sprague; kill the 4 Rheinlanders, dock; undock and **fly directly to Baxter** (do not fight the endless wings - the kb says the same); skip; resupply missiles if low; undock, kill the Rheinlanders, straight to the Cambridge hole; fly to the waypoint until the 2 Rheinlanders appear; Leeds hole without stopping; kill the Rheinlanders, dock the hole; **the marked lane ring only (one ring works)**; dock the planet; skip. | - | N (all already in the kb page) |
| Free time 5 (+16000) | - | Leeds: sell Diamonds (1276: +396/unit), buy Mining Machinery (400) + sell garbage. Hood: sell Mining Machinery (520: +120), **buy Gold (323; sells 1105 at New Tokyo)**, buy Torpedo launcher, 1 Catapult, ammo (HotSlice: buy the torpedo / missiles at Leeds instead to dodge the "IMG neutrality bug" that blocks buying at the Hood). Accept M06. | IMG reputation bug at the Hood | N |
| M06 | - | **Win the race**, dock the Hood, skip; undock, fly directly to Glorious; kill the Rheinlanders, dock Glorious, load the save (skip); waypoint 1, kill, head for the Leeds hole killing the Rheinlanders on the way; hole, Leeds, dock, accept M07. StrategyWiki: fly the course at cruise, miss no gate, hit nothing; Hovis errs near the end `[page]`. Forums: F2/goto drops cruise near obstacles; overtaking Hovis before his weapons activate failed a run `[page neoseeker]`. | Race 2 lost = FAIL with no third race (runbook); 30 s gate timer | H (manual ring flying at cruise; the runbook notes goto works in 0.7.1 but not later HDE) |
| M07 | - | Kill one Agent, lane asap; kill the 2 Rheinlanders until they cloak, lane to the gate; kill, gate asap; Holman, then directly to the Tau-29 lane; the under-construction Tau-23 ring as soon as the battleship uncloaks; lane, ignore the corvettes; hole, straight to Shinkaku; dock + undock; straight to the Schiller battle; kill the fleet, Tau-23 hole when the last dies. HotSlice: safest = hide in the large opening in front of Battleship Schiller and kill it with missiles, thrust out, rejoin; faster but riskier = fight straight away; **gunboat missiles one-shot, cruiser turrets hurt**. Cali: dock, skip, bar, skip; Kyushu hole; land, skip. | Gunboat missiles | H (the Schiller "hide in the opening" trick is geometry the autopilot could learn: the bow-arc rule) |
| Free time 7 (0 gate) | - | New Tokyo: sell Gold (1105: +782/unit), **buy Light Arms (`commodity_sidearms`, 630 `[data]`)**, ammo; SydMontague also buys a Barracuda (hull 15300; not sold in Kusari per HDE data - Bretonia only, so `[assumed]` vanilla difference or bought earlier); Shinagawa, accept M08. | - | N |
| M08 | - | Honshu gate, Honshu hole, Yukawa shipyard: **CM dropper on auto** - the platforms' missiles one-shot when the shield is low; formation with the transport ("often a long wait for the transport to reach cruise, no way to speed it up"); Marine fighters; Chugoku hole; Kyoto, skip. | Platform missiles | N (CM logic exists), the transport wait is script-bound |
| Free time 8 (+36000) | - | Kyoto: sell Light Arms (770: +140/unit), garbage, refill ammo. **"Often it requires to buy, and sell, and then buy back your torpedoes to trigger the level up."** Accept M09. | - | N (a transaction as the gate's trigger: section 7) |
| M09 | - | Tohoku hole; up to 5 km from Outpost Ryuku; the arch, kill the patrol; generators with the **CM dropper active (battleship missiles one-shot at low shield)**; "do the rest fast"; after the arch cutscene use CMs against the gunship missiles, fighters first. Land Kyoto, skip; accept M10. | Missiles | N/H |
| Free time 9 (+30000) | - | Nothing bought in the notes (the Kyoto sales after M08 and the +10000 rewards of M08/M09 cover it `[assumed]`). | - | - |
| M10 | - | Sigma-13: kill all 8 fighters (or 4 cruisers or the battleship: "fastest 8 fighters if quick, battleship safest hiding in its front"); **loot the two Old Imperial Navy wrecks (Onyx grid 4C, Diamondback 5C)**; New Berlin (skip on landing; HotSlice sells the level-10 weapons there and restocks); Holstein: **EK to shed Fischer and dock before him (no cutscene skip here)**; Bruchsal: defend, land, skip (do not load the last mission save); sell wreck loot, buy Driller mine / 4 Lugers, ammo; passage, destroy **one** experimental ship; wait until Botzler dies, then straight to the Hamburg hole staying within 6 km of von Claussen; **dock the Osiris only once the purple waypoint has spawned on it (else softlock)**. | Osiris softlock; 6 km tether | N (wrecks = loot; one ship) |
| M11 | - | Buy an Anubis (hull 1000 `[data]`), Debilitator turret, mines, sell garbage; NY hole; Liberty ships; Buffalo, skip; NY hole; satellite + fighter; Walker, **form up with Walker (needed)**; fighters until Juni enters the code, gate asap; **Mitchell: EK at ~4.5 km, dock**; NY gate, dock asap; Omaha, wait for the Osiris to uncloak, dock. | - | N |
| M12 | - | Swap the Debilitator for a Reaper / Death's Hand turret (hull damage only from here); first generator: wait until damageable; 3 generators; **CM dropper on after the 3rd (Nomads sometimes carry cruise disruptors)**; nest, energy cell; hole asap; Toledo ignoring the Nomads; accept M13. | - | N |
| M13 | - | Skip; destroy the first battleship (head left); wait for the docking order, dock, skip, undock; Osiris immediately, dock, skip, **refill ammo fully**; hyper gate immediately (if the waypoint resets to the Osiris, follow Orillion on radar); Nomads: fighters first, allies' missiles finish the battleship; gate asap; entrance, 4 generators, through, first generator immediately, destroy the generators (the runbook: 2 of the 5 final generators end it `[repo]`). | **Osiris softlock (cause unknown, cost the WR ~2 min)** | N |

### 4 Money gates: the fastest legitimate ways
Gate amounts `[repo]` FREELANCER_RE.md "Story progression by net worth": at a mission's end `delta_worth` = net worth
then + FreeTime 01-02 **3500**, 02-03 **5200**, 03-04 **7000**, 04-05 **10000**, 05-06 **16000**, 06-07 **0**, 07-08
**36000**, 08-09 **30000**, later 0; net worth = `GetAssetValue` (ship, equipment, cargo) + cash, evaluated on the
story object's events 0 and 0x100B. Mission cash (`Act_AdjAcct`, runbooks `[repo]`): M01a bounty 750 (bad branch
only), M01b +3500, M02 +5000, M03 +3000 (Willard), M04 +25000 (Leeds Equipment room), M06 +7000, M07 +4000, M08-M12
+10000 each, M13 +20000; M05: none found in the runbook.

Prices below: HDE repository `goods.ini` base price x `market_commodities.ini` multiplier `[data]`
(`downloads\commodity_prices.tsv`, 799 rows, 18 commodities, every base). They agree with the StrategyWiki figures
(H-Fuel 360 Manhattan / 425 CA Minor / 444 Willard `[page]`), so vanilla and HDE match here.

| Gate | What the runners do | Numbers | Source |
|---|---|---|---|
| +3500 (after M01b) | Rhino (hold 80) at Pittsburgh, then the **Fort Bush DSE H-Fuel depot**: 64 H-Fuel for free, sold at Manhattan | 64 x 360 = **23,040** cr of loot (minus pods kept for later); Rhino hull 7900, Starflier hull 6000 trade-in | notes `[page]`, depot loadout + prices `[data]` |
| +5200 (after M02) | the same depot again (pods respawn with the system `[assumed]`), part sold at Missouri (336) and Manhattan (360); M02 +5000 | 48 H-Fuel on hand per the notes ("sell 16/48") = ~17,000 | notes `[page]` |
| +7000 (after M03) | M03 artifacts from the convoy pods, sold at Manhattan | artifacts **1282** at Manhattan, 1215 at Rochester (buys), 1012 New Berlin, 810 New Tokyo; HotSlice's bad run had "4 artifacts" and money trouble, so ~10+ are the target | notes, run comments `[page/api]`; prices `[data]` |
| +10000 (after M04) | M04's own +25000 at Leeds, plus **Optronics Leeds 980 -> Cambridge 1330 (+350 x 35 = 12,250)** and **H-Fuel Leeds 300 -> Norfolk 396 (+96 x 35 = 3,360)** on the way; Crusader bought at Norfolk | the Rhino's 80 hold carries 35 + 35 (+10 free) | notes `[page]`, prices `[data]` |
| +16000 (after M05) | **Diamonds Cambridge 880 -> Leeds 1276 (+396; x30 in the Crusader = 11,880)**, Mining Machinery Leeds 400 -> Hood 520 (+120 x 30 = 3,600), kills / loot | needs ~16,000: the two trades give ~15,500, the rest from loot and M05 | notes `[page]`, prices `[data]` |
| 0 (after M06), M06 +7000 | nothing needed; Gold bought at the Hood (323) is carried through M06/M07 | - | - |
| +36000 (after M07) | **Gold Hood 323 -> New Tokyo 1105 (+782 x 30 = 23,460)**, M07 +4000, then **Light Arms New Tokyo 630 -> Kyoto 770 (+140 x 30 = 4,200)** after M08 | the sum is short of 36,000 on paper: the rest is loot sold along the way and the ship / equipment value bought (purchases are 1:1 cash -> value `[repo]` economy.md) | notes `[page]`, prices `[data]` |
| +30000 (after M08) | M08 +10000, Light Arms sold at Kyoto, the Barracuda / torpedoes bought; "buy, sell, buy back the torpedoes to trigger the level up" | - | notes `[page]` |
| later (0) | M09-M12 each +10000; StrategyWiki: have ~100 k before M10, no more trading after it `[page]` | - | - |

Alternatives the walkthroughs name (not used by runners, for the job-less fallback): Cardamine from the Flint wreck in
the Badlands sells 1500 at Manhattan `[page StrategyWiki M1]`; Silver (Colorado) 630 at Manhattan; Engine Components
Houston 600 -> Mactan 1425 `[data]` (StrategyWiki says to carry them into M04 `[page]`); the Lorenzo wreck in Leeds
(2 Pyros 2, Dragoon turret, 20 Cardamine) and San Vicente (2 Dragoon 2); Henry Jones / Caraz wrecks in Cambridge
(6 Starbeam turrets + 10 Gold; 2 Angelito + 20 Artifacts) `[page StrategyWiki M5]`; GameSpot: mining runs
Copperton -> Pueblo early, Silverton -> Los Angeles (700) `[page search snippet]`. Job-board missions: none of the
speedrun material uses them; StrategyWiki only says reward grows with fewer allies / more enemies.

Equipment sale value: StrategyWiki M1 says selling equipment returns 30 % of its price `[page]` - FLvibed measured
purchases as 1:1 into net worth `[repo]`; the resale ratio is not verified by the project (section 9).

### 5 Cutscene / skip rules the runners rely on
- **Base cinematics (RTC room scenes):** vanilla has no skip key; the runners' "cutscene skip" = Esc during the scene,
  Load Game, load the latest autosave (the one written at the mission accept / landing), which resumes after the
  scene. Pitfalls in the notes: at Leeds after M04 the latest autosave is the "Van Pelt" one and must not be loaded
  (it rewinds); at Bruchsal "don't load the last mission save"; at Holstein "NO cutscene skip" (Fischer must dock
  after you). The **Tobias skip** at Leeds is different: repair the ship and press F1 until the dealer menu opens - the
  scene is pre-empted by the room UI `[page cq15x, oeyn3]`. **HD Edition** makes all of this unnecessary: CHANGELOG
  "Skip story cinematics (excluding space cutscenes) with the Esc key, like in the Freelancer Beta version"
  (`[data]` HDE CHANGELOG.md line 129) - FLvibed already sends USER_CANCEL for room scenes and docking cameras `[repo]`
  FREELANCER_RE.md, kb navigation-and-hazards.md "The docking / landing cutscene is skipped by the autopilot itself".
- **Space cutscenes (`Act_StartCutscene` / Thorn cameras):** not skippable by anyone; upper bounds per scene are in
  `docs\missions\CUTSCENES.md` `[repo]`. The runners' only lever is to be in the right place when they end.
- **Startup movies:** commented out in freelancer.ini `[page the-starport]`; HDE "Made intro skips optional" `[data]`.
- **"Load autosave" as a state trigger (not a skip):** "Undock, load Autosave, accept Mission 4" and "load Autosave
  after jumping" use a load to make the story object re-evaluate the net-worth gate / spawn state without flying back
  in `[page oeyn3]`. Allowed by the board, but save abuse under the project's rules; the native equivalent is to fire
  the same evaluation event (section 7, item 2).
- **Frame rate:** "Full screen and higher framerate improved speed" `[api]` (HotSlice, 4:36:42 comment). The runners
  treat fps as a speed factor.

### 6 Comparison with FLvibed's numbers and runbooks
| Item | FLvibed today `[repo]` | Speedrun knowledge | Verdict |
|---|---|---|---|
| M03 whole mission from the accept | objective mode 881-950 s from Save1e7d (step 5, docked at CA Minor after the accept) | IL **836 s** from the accept autosave to Juni's payment, on a "not optimal ship", with a Willard shop stop inside the time | the autopilot is 5-14 % behind a casual human IL; the runner also steals the artifact pods on the way |
| M04 | 1002-1059 s (survival stance) vs the user's 1359 s scene-accounted | no split; route = lanes the instant they open, EK to shed Juni at the Magellan gate, no Mactan shopping, Tobias skip | the stance matches the route ("do whatever you want except dying"); the EK shed is missing |
| M05 | 1225-1304 s | route: never fight the endless wings; "only this one ring works" at Leeds | matches the kb page; nothing new except "resupply missiles at Baxter if low" |
| Full campaign | sweep M01a..M05 ~5,315 s of mission time, between-missions layer not built, P6 not started | WR 14,568 s for everything incl. 8 free times and all base / load time | extrapolated the current pace is ~1.5-2x the WR pace (estimate; the sweep starts from fixtures, not New Game) |
| Money gates (`AUTOPILOT.md` "P5 F: jobs, purchases") | plan: job board + purchases | runners: **free loot (depot pods, convoy artifact pods, wrecks) + one trade per hop**, never jobs | contradicts the plan's default; the loot route is both faster and game-native (tractor, sell) |
| Docking with a companion (kb: "mission companions dock at the same moment as the player (M03 Willard: Juni first, ... docked after ~35 s)") | "a companion could be let in first" | runners **engine-kill at 4.5-5.5 km so the companion drops out of formation / falls behind and the player gets the bay first** | contradicts "let in first": shed, do not yield |
| Neutral ambushers (M04 bounty hunters, M02 Fort Bush Rogues) | engage by attitude | runners attack them **before they turn hostile** (no penalty, one free kill) | beats the attitude-only rule when the script marks them |
| M01b | platforms only (kb) | same, plus "turn ~120 deg left at Beta-4" to face the base spawn | same |
| M10 wrecks, M03 artifact pods, M02 depot | not in any runbook or kb page | the main income of the route | gap |
| Queue shortcut (formation on a docking NPC at gates) | measured 4 of 4 with a lock | not in any runner note | the project is ahead here |
| Cutscenes | Esc skips rooms + docking cameras; space scenes played | same in effect (HDE) | equal |
| M06 race | runbook: goto per ring works in 0.7.1 | StrategyWiki: cruise the whole course, miss nothing, Hovis fails near the end; forum: goto drops cruise near obstacles | the runbook's goto plan risks losing cruise; a cruise-held manual ring path is safer |
| Softlocks | M03 "bombers 60 s after arrival", M10 Osiris waypoint not noted, M13 Osiris not noted | Willard softlock (kill all before the bombers spawn), Osiris softlocks M10 (wait for the purple waypoint) and M13 (unknown) | add to the kb pages as fail paths |

### 7 Actionable items for the autopilot (prioritised)
*[Check-up 24 Oct 2026: the live statuses are IDEAS.md A1-A36. Of these twelve: 11 done (A8), 6 in part (M12 Toledo
and the M09 nag by script goals, A21), 12 in part (leg / checkpoint boundaries and par built, A10; the board-comparable
campaign number is not), 2 answered by the videos; the rest open.]*
1. **Build the between-missions layer on loot first, trades second, jobs last.** Evidence: every runner reaches each
   gate with depot / convoy / wreck loot plus one trade per hop (section 4); prices verified in the game data. Concrete
   plan per gate: FT1 Rhino + Fort Bush depot (64 H-Fuel = 23,040 at Manhattan); FT2 depot again; FT3 artifacts;
   FT4 Optronics Leeds -> Cambridge, H-Fuel Leeds -> Norfolk; FT5 Diamonds Cambridge -> Leeds, Mining Machinery Leeds
   -> Hood, Gold Hood -> (New Tokyo); FT7 Gold, Light Arms New Tokyo -> Kyoto. Generalise it as "a trade planner from
   `market_commodities.ini` along the story's own path with the ship's hold", not a per-gate script (Generalization).
2. **[Answered 22 Oct by the videos, V6: a dealer transaction or a cargo pickup in space fires the level-up; verify in
   our build with `worth`]** **Find the native trigger of the gate evaluation.** The runners need a load ("undock, load autosave") or a
   buy-sell-buy-back of torpedoes to make the level-up fire `[page]`; FLvibed knows the story object evaluates on
   events 0 and 0x100B and already logs `worth-target` `[repo]`. Test with debug `worth` which base actions
   (dealer transaction, room change, launch, land) fire 0x100B; the cheapest one replaces the runners' reload. Also
   verify with `worth` how cargo is valued (local price vs `goods.ini` price) - it decides whether buying trade goods
   moves net worth (the "buy 80 Optronics, sell 45" step hints the runners manage that too).
3. **[Revised 22 Oct by the videos, V5.6: engine kill at cruise 3-4.5 km, or just holding cruise; Fischer by the
   lanes]** **Companion shedding before a dock: engine kill at ~4.5-5.5 km** (M03 CA Minor, M04 Magellan gate, M10 Holstein
   vs Fischer, M11 Mitchell) `[page]` so Juni / Fischer do not take the single bay first; the kb measured a 35 s loss
   at Willard `[repo]`. Game-native (USER_ENGINE_KILL, then the fast dock). Verify whether the companion simply drops
   formation or whether the distance matters (the notes give 4.5 and 5-5.5 km).
4. **Lane / gate entry on the script's distance condition, not on the comm.** M01a "Brandt within 2000 m of the lane,
   readout 1800 -> 1600" `[page]` and M02 "lane as soon as Game Saved appears" are the exact triggers the runbooks
   describe as races; the objective mode already watches `laneopen` every 0.5 s - add the companion-distance rule for
   missions whose lane is open but whose trigger needs the companion (King for the Unity and Rogues triggers in M02,
   King + Juni for Benford in M04).
5. **Loot targets as mission targets:** cargo pods of depots and of the M03 transports (transport 2 only; transport 1
   = fail), the M10 wrecks (Onyx 4C, Diamondback 5C). The kb's `DESTROYABLE_DEPOT` note ("[assumed] destroyed through
   parts") is confirmed by the route: the pods are shot open (`HpCargo01..04`, 16 H-Fuel each `[data]`). Respect the
   project's hull rule; the notes' "only 2 pods unless an enemy is within 2.4 km" is a reputation guard to verify.
6. **Fight only what the script needs** (already the survival stance's idea): M05 Sprague wings, M04 pursuers, M10
   "one experimental ship", M07 "ignore the corvettes", M12 "ignore the Nomads to Toledo" `[page]`. Make "leave when
   the objective is a place" the default for every mission, not only Survive verbs.
7. **Pre-aggro marked neutrals** (M04 bounty hunters, M02 Fort Bush Rogues, M11 satellite) `[page]`: when the mission
   marks a ship as a target the autopilot may open fire before its attitude flips (the kb's rule 1 "mission-pointed
   hostiles first" already ranks marks first; extend it to marked neutrals).
8. **Softlock guards:** M03 keep at least one Rheinlander alive until the bombers have spawned (or intercept the
   bombers before Willard); M10 dock the Osiris only after its navmarker exists; M13 Osiris (unknown cause - log the
   objective / dock state there). Add to `docs\kb\missions` M03 and the M10 / M13 pages when written.
9. **CM dropper policy by mission:** M08 platforms, M09 battleship / gunships, M12 after the 3rd generator (cruise
   disruptors) `[page]`; the autopilot's CM logic already keys on predicted damage - make sure the one-shot cases
   (shield low + capital missile) are inside `cm_shield_fraction`.
10. **Shopping plan per hop** (what and where): Javelin at the start (sell bats / bots), Rhino + Stunpulse turret at
    Pittsburgh, Justice guns Manhattan, Slingshot + class-3 shield + Heavy Thruster at CA Minor, Crusader + class-4
    shield + Rippers at Norfolk, torpedo + Catapult at Leeds (not the Hood: IMG bug), Barracuda / Light Arms at New
    Tokyo, Lugers + Driller mine at Bruchsal, Anubis + Debilitator at the Osiris, Reaper / Death's Hand turret for
    M12-M13 (hull damage only) `[page]`. The user's 2020 loadouts remain the compatibility reference; the runners'
    list is the minimal-cost one.
11. **M06 race:** fly the ring path at cruise without goto (goto drops cruise near obstacles `[page neoseeker]`; the
    runbook's goto-per-ring plan is a risk); losing race 2 has no retry.
12. **Benchmark hygiene:** the board times real time from New Game incl. loads and base time; the project's per-phase
    in-space comparison is finer, but a campaign-run number comparable to the board needs the same start / end
    (New Game click -> the M13 final cutscene start) and an uncapped frame rate (runners report fps as a speed factor).

### 8 Sources
- speedrun.com game page / leaderboard (browser): https://www.speedrun.com/freelancer
- API (saved under `downloads\`): https://www.speedrun.com/api/v1/games?name=freelancer ;
  .../games/268eekk6/categories?embed=variables ; .../games/268eekk6/levels ;
  .../runs?game=268eekk6&status=verified&max=200&embed=players,category,level ;
  .../leaderboards/268eekk6/category/w20wgrjd ; .../leaderboards/268eekk6/level/<id>/824106w2 ;
  https://www.speedrun.com/api/v2/GetGuideList?gameId=268eekk6
- Guides (browser): HotSliceOfJesus "Basic Route and Strategy Guide" https://www.speedrun.com/freelancer/guides/cq15x ;
  SydMontague "4:17:54 notes (raw)" https://www.speedrun.com/freelancer/guides/oeyn3 ;
  WR run page https://www.speedrun.com/freelancer/runs/me5w0l8m ; forums https://www.speedrun.com/freelancer/forums ;
  resources https://www.speedrun.com/freelancer/resources
- StrategyWiki (MediaWiki API wikitext): https://strategywiki.org/wiki/Freelancer/Mission_1 ... /Mission_12
  (Mission_13 does not exist)
- The Starport, cutscene skipping: https://the-starport.com/forums/topic/3313/how-to-skip-cutscenes
- Search snippets only (pages blocked to WebFetch: 402/403): GameSpot walkthrough
  https://www.gamespot.com/articles/freelancer-walkthrough/1100-6025600/ ; GameFAQs "Mactan Notes"
  https://gamefaqs.gamespot.com/boards/913966-freelancer/52877777 ; Neoseeker Hovis threads
  https://www.neoseeker.com/forums/1935/t202174-how-beat-hovis-in-race/ ; fandom Trade Routes
  https://freelancer.fandom.com/wiki/Trade_Routes ; SWAT Portal engine kill
  https://swat-portal.com/forum/cms/wiki/freelancer/enginekill/
- Game data (read only): `FLvibed\research\repos\freelancer-hd-edition\DATA\EQUIPMENT\goods.ini`,
  `market_commodities.ini`, `market_ships.ini`, `weapon_good.ini`, `SOLAR\loadouts.ini`,
  `UNIVERSE\universe.ini`, `UNIVERSE\SYSTEMS\Li01\Li01.ini`, `CHANGELOG.md`; names from
  `FLvibed\research\strings\resources.tsv`; FLvibed `docs\kb\generated\ships.tsv`, `solararch.tsv`.
- FLvibed docs: `CLAUDE.md`, `controller\docs\AUTOPILOT.md`, `docs\kb\README.md`, `economy.md`,
  `navigation-and-hazards.md`, `combat-targets.md`, `missions\M01a..M05.md`, `docs\missions\README.md`,
  `CUTSCENES.md`, `M01b/M02..M13.md` (grep), `docs\FREELANCER_RE.md` (story gates), `HANDOFF.txt`.

### 9 What could not be verified
- [Answered 22 Oct: the recordings give every leg of three runs, V1-V2] **Per-mission splits of any full run**: none published (`splits: null` on all 28 runs; no splits.io; the YouTube /
  Twitch pages return no description text to WebFetch and were not opened in the browser by the project's rule).
  The only per-mission number is the M03 IL (836 s).
- **Game version of the runs** (vanilla 1.1 vs HDE): no rule, no comment says it; the autosave cutscene skips in the
  notes only make sense in vanilla.
- **Net worth valuation of cargo** (local vs base price) and the exact event that re-evaluates the gate - the
  runners' reload / buy-sell-buy-back steps imply it is not continuous; the project can measure both with `worth`.
- **The "only 2 pods unless an enemy within 2.4 km" rule** (reputation mechanics of shooting a lawful depot's pods).
- **Depot pod respawn** between Free time 1 and 2 (the notes loot the same depot twice).
- [Answered 22 Oct, V7: yes in vanilla - Norfolk (runs 2, 3) and Suffolk (run 1); the Barracuda at New Tokyo (run 1)]
  **Battleship Norfolk** sells the Crusader in vanilla: HDE data lists `bf_package` at Planet Leeds, New London,
  Cambridge and the Hood, not at Norfolk (Br03); the runners buy it at Norfolk, so vanilla markets may differ or they
  mean another dealer. SydMontague's Barracuda "at New Tokyo" likewise (HDE sells `bhf_package` in Bretonia only).
- **M13 Osiris softlock** cause (the WR holder does not know either).
- **Frame rate as a speed factor** (one runner's observation; not measured).
- fandom "Single Player Campaign" / "Trade Routes" and GameSpot pages (402/403 to WebFetch, not opened in the browser
  to limit scope); nothing in them contradicts the game data used here.
