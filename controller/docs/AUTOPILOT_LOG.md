# Advanced Autopilot: session and set logs (moved out of AUTOPILOT.md)

Dated per-session / per-set logs - run verdicts, set results, probe narratives, measurements - **moved from
`AUTOPILOT.md` on 31 Oct 2026, verbatim** (the doc-size ratchet, audit C5; nothing rewritten, nothing deleted). The
behaviours as built, the owner's rules, every setting's description and the current state stay in
[AUTOPILOT.md](AUTOPILOT.md), which points here with `(history: AUTOPILOT_LOG.md "<section>")` where a block left.
Order: the order the blocks sat in AUTOPILOT.md (oldest topics of the combat section first, then the M13 sets).
Each section opens with where its text came from; the text itself is untouched, so its
indentation and any `..\` paths are as they were in AUTOPILOT.md (paths are relative to `controller\docs\`).

## Audit 18 Oct 2026

*Moved from: the "Audit 18 Oct 2026" bullet (combat section); its closing "Open:" item stays in AUTOPILOT.md.*

stale fight reading with a chosen target
    (flee never ended), flee goto issued once (sat among hostiles after 8 km), stale "being hit" between targets
    (repairs), escort's stale no-enemy time (back to Rejoin at once), engage 0 ended an escort, CMs / dodge timed on
    the first listed munition instead of the nearest, follow never noticed a dead leader while the game's follow ran,
    held commands (slide up / down) left held after a hand-back to pad flight, flbench A/B dropping the last segment.
    Core audit the same day (`..\..\core\game\space|ai|spawn`): missile closing speed timed by QueryPerformanceCounter
    and kept between samples (GetTickCount's ~16 ms steps read 0 for several frames in every 50 ms: dodge dropouts);
    the harness waypoint save belongs to its flight (a failed goto left it armed for the next flight / save); spawns
    are tracked by id + their own reputation (ids outlived a load and are reused: `spawn clear` could destroy another
    ship); guarded GetRep / SetAttitude / GetLocation in spawn and the `ai layout` personality read.


## Tuning 17 Oct 2026

*Moved from: the "Tuning (17 Oct 2026, 43 runs ...)" bullet (combat section).*

  - **Tuning (17 Oct 2026, 43 runs, no godmode, police hostile, SaveC702):** lost per kill / kills per min: weave
    0.75 0.13 / 3.38 (default now), 0.6 0.20 / 3.31, evasion off 0.33 / 3.22 (3 deaths in 7), dodge 2 0.82 / 2.12
    (2 deaths in 3); evade_scale 6 % beats 4 % and 10 %; slides 0.5 s kept. Throttle profile: autopilot afterburner
    42 % / full fwd 37 % / reverse 9 %; the user's playlist 29 / 42 / 4 %.


## Sets ab1-ab4

*Moved from: the "Bundles A + B" paragraph (objective-mode section), set ab1 .. ab4.*

Set ab1: 5 full legs from Save3989,
  stopped 2 min in - `kill_ahead`'s fight-step form never fired from the start save: "Defend the Order Base" (24414) reads
  as an Escort step with no object (fixed 131f881: such a step we fight in counts), and its setters lead nowhere, so the
  next step was unreadable (95a9a4a: the armed rows' destroy goals, `mission::GoalsOfLive` - nomadcap's `cap_dies` is
  armed at its spawn). **Set ab2 (bafeb64 + 131f881, all of bundles A + B; 5 legs) [M]: `bs_approach` BAD as built** -
  the dash flew (200 m/s, `blind-rush` 799 -> 205 m in 3 s) and launches went from 4.6 km in, but nothing beyond ~1.6 km
  hit (hp 119.4k at 1.6 km); at the blind spot (0,94,-94, cover 1, the spot A uses) every launch was held: the round would
  burst 65-73 m ahead vs the need 83 / 92 at margin 55 - the spot sits in a notch, the hull ~50 m from the centre; the gap
  penalty (<= 600) lost to the cover score. Guns alone ~1.9k hp/s: kill 83-119 s after the banner vs A's ~48 s; 3 of 5
  FAILED the bsdown limit (191 s). Dropped from the next set (ab3). ab2's two other legs died with batteries held by the
  budget's hull-0.6 floor (i1 at the Nomad gate: share spent, 24 held on a failed shield at hull 0.89, dead 2 s later; i3
  at the barrier: 9 held on a shield at 0.16, dead from hull 0.96) - cd9fcd9: a failing shield under fire at a big target
  is always refilled, the share binds the top-ups only. Set ab3 (95a9a4a): the defend step runs Mode::Escort, so the
  fight-step check never ran before the banner, and after it the armed-rows fallback named nomadcap itself - a decide
  every second (29-53 a run, kills 54-95 s after the banner); 24b43a5 fixes both (Escort mode, the fallback only for the
  defend step, never the target fought already). Set ab4 = cd9fcd9. **Set ab3-b_i4: the new PB 782.6 s** (was 861.7;
  sphere 76.5 and end 150.5 golds; `dead-first=1` in 0 of 55 salvo checks). ab4 [M]: kill_ahead now fires 1 s after
  nomadcap's spawn from 5.0-5.5 km (12.6 s before the banner) - bsdown 70.8 / 80.0, legs 799.6 / 800.2; the city entered
  with 0 batteries: at the Nomad gate the always-released failure bursts went 10 / 10 / 9 in 6 s at hull 1.00. Then:
  


## Sets ab4-ab8

*Moved from: the "Bundles A + B" paragraph, ab4 open levers and measurements (a fragment between two kept sentences).*

Open levers seen [M ab4-b_i1]: the city
  airlock run from behind the door's plane (way-in 108 deg, a 1.9 km path, 28.8 s vs mm9's 14) - the barrier's kill order
  should end at the generator nearest the door's approach; the city trip's first ~13 s at 200 m/s before the cruise charge
  (`travel cruise charge reset at 2.0 s (attackers=4)`: hits reset the charge - no lever). Measured since [M]: the 3-unit
  cap (62c0730) - ab7-b_i2 killed the gate battleship with 0 batteries spent (29 kept for the barrier); `bay_request_lined`
  - ab7-b_i2 requested 12 m from p2 at 65 m/s 6.3 s after 24445 (port 0.5 s later), ab6-b_i4 braked to rest beside the bay
  and asked from 622 m (orillion 49.2): 

*Moved from: the "Bundles A + B" paragraph, PB 781.4 s and set ab8 (up to the `blind_site_check` description, which stays).*

**PB 781.4 s (n=1, no verdict;
  ab6-b_i4; par 686: +95; sprt vs mm9 CONTINUE on every segment).** Gate m13afk1 stopped at ~01:55 (the owner: par first).
  Run-judged ab3-ab7 BAD (3 of 11 died; bs_approach raised the spot hits to 2-4 a run - dropped). **Set ab8 (ae78f55: no
  bs_approach, the city floor of 4 batteries a kill ahead, 32410f6) [M]: PB 754.6 s (ab8-b_i1, n=1, no verdict)**, legs
  761.6 / 799.2; bsdown 57.6 / 58.5 / 61.9 / 74.6 (sprt vs mm9 CONTINUE: one failure counted at the limit), orillion 34-38;
  2 of 5 died at the battleships. **Run-judged ab8 BAD (3 of 5 passed; the harness caused no death):** the battleship kill
  28.6 / 29.7 / 32.9 / 45.7 s after the banner (ab4-ab7 median 47.8, mm9 42.8-47.5 - the runners' ~21-35 s), but the
  lowest hull there 0.37 / 0.50 / dead (ab4-ab7 >= 0.78): no `launch-site` lines any more, nor their margin. i5: our own
  torpedo (launched at a 51 m line gap with the hull 11 m beside, no fresh salvo check; dead 33 ms later, a burst cloud on
  the end sheet). i2 (cause not proven; my "own missiles" reading was wrong - they burst 220 m off): the new floor held all
  24 batteries (4 x 6 kills ahead) on a shield at 0.06-0.11 to the death -> **189469a: the floor capped at 8**
  (unverified). i4 survived its own torpedo at the barrier at hull 0.09 (`big-hit lost=2423`, `blast-near to-us=29`). City
  entry batteries 2 / 5 / 0, 0 city deaths in 3; orillion median 36.1 (ab4-ab7 41.2). 


## Sets ab9-ab11

*Moved from: the "Package AFK2" paragraph: probe tr0 (the `torp_reach` description before it and the `missile_reach` description after it stay).*

Probe tr0 (S415 -> M13.bsdown, torp_reach 100) [M, n=1]: torpedoes
  only at nose gaps 128-361 m (none inside 100; 3 held at 60-94 m, no event of its own), the kill 54 s after the banner
  (mm8 median 50.4), bsdown 83.0 (mm8 79). Run-judged INCONCLUSIVE: the lowest hull 0.308 (not 0.645 - my misread), our
  own missiles 97 / 98 launched 148 m out burst 14 / 23 m from us (`big-hit lost=2828 own-end=none`, 1.000 -> 0.327) -
  torp_reach covers torpedoes only; 29 batteries + 11 nanobots spent. Set ab9 (5 full legs, reach M13:5:45) 02:35-.
  

*Moved from: set ab9 (its closing rule, "battery_budget's rhythm now holds at every big fight", stays).*

**Set ab9 (cfe09fd: + torp_reach 100, blind_site_check, the floor cap of 8) run-judged BAD [M]:** 4 of 5 passed
  (759.4 / 778.9 / 803.5 / 805.4, median 791.2 vs ab8 761.6; sprt leg CONTINUE); torp_reach at the barrier: no torpedo
  inside 100 m, 0 own-round hits there (ab8 1-3), sphere median 84.4 (no time gained); i2 died at the battleship from a
  turret volley on a shieldless still hold - its batteries went 29 -> 19 on top-ups by 300 m and to 0 at 150 m out (window
  A had no rhythm); city own blasts 13-20 m off at a 40 m hold (i4: 0.96 -> 0.32); Toledo queued once (sinclair 55.1).
  

*Moved from: probe pr1 and set ab11 (up to the `city_hold_side` description, which stays).*

**Probe pr1 (missile_reach 100 + the three above;
  776.9 s, no death) run-judged BAD [M, n=1]:** the battleship 36 s after the banner, 0 own-round big-hits; missile_reach
  held 48 (29 at the barrier, whose holds sit at a 46-53 m gap: salvos 4 vs ab9's 15-27, sphere 96.2 vs ab9 median 84.4 -
  ab9 had 0 own-round hits there without it) -> the next set runs **missile_reach 40** (above tr0's 14-23 m bursts, below
  the barrier's gaps); static_site_check's one hold was a torpedo torp_reach held anyway; arrive_walled_fast acted once
  (-3.7 s; its line still prints "8 s" - a text slip); self_burst_near unproven (a blocked learn logs nothing); the city
  entered with 0 batteries / 21 nanobots, hull 1.00 -> 0.47 at the barrier with no round of ours in flight.
  **Set ab11 (pr1's switches, missile_reach 40; 5 legs from Save3989, build 3f40b195) run-judged BAD [M]:** 3 of 5 failed (2 deaths + i5 below)
  at the city's first generator (final_generator5; ab8 2 of 5) - i5 did NOT die (corrected 31 Oct 2026: its
  hull stayed in (0, 0.0005) - the `self hull=` event prints only above 0 -, shields refilled to 1.00, the ship flew on,
  no death camera; the autopilot's hull <= 0.0005 test stopped "player died" on every restart, 36 times, until the
  M13.end limit; fixed: exact 0 + the death camera, and Leg.ps1's AUTOPILOT LOOP stop); passes 787.1 / 758.8 (+73 vs par, PB SAME), the city
  segment ~5 s CLOSER, sphere FURTHER (97.8 / 89.9); sprt vs ab8 CONTINUE (LLR -0.26; fails excess 1.0). Every death
  held 31-67 m off the OUTWARD face, 100-163 deg from the city centre, in front of the weapons platforms, with 0-2
  batteries: the gate battleship still spends 20-23 although the budget logs keep=24 (the 3f40b195 rhythm changed
  nothing there); i4 lost 0.93 -> 0.26 to its own torpedo (129 m gap, another hull 72 m - torp_reach / self_burst_near
  on). [I, small n] ab8 / ab9 / pr1 held 250-500 m below the generator's height and lost little. Next: the city-hold rule
  of `..\..\docs\research\runners\M13_city_hold.md` (city-centre side, gap >= 100 m, deep y) and the gate battery
  spend, probe first.


## Probe ch1 and set ab12

*Moved from: the `city_hold_side` description: probe ch1 verdict (the transit description after it stays).*

**Probe ch1** (ab11's switches + this, n=1, Save3989, build fc379a32) run-judged **BAD** [M]: PASS 777.6 s (+91.8 vs
  par, FURTHER than the PB's +23; the city segment 167.3 s = the PB's, +38 vs par). At the first generator
  (final_generator5) the side was picked (45 deg) but **never flown to**: off its point the hold hands the steering to
  the pursuit, which held its own band on the arrival side at bearing -35 deg, 131 deg off the side (FLMotion vs m13.ini
  platforms 13-15; the run-judge's 130-135), 7 torpedoes held by the site check, 38.4 s vs the PB's 28.5. 

*Moved from: the `missile_keep_big` description: set ab12 (probe kb1 and the fixes after it stay).*

**Set ab12** (5 legs, 136ef81e, ab11's switches + city_hold_side + missile_keep_big) run-judged INCONCLUSIVE [M]: 5 / 5
  alive (ab11 3 / 5 failed), 743.9 (PB, -10.7) / 771.0 / 774.8 / 803.2 / 813.0, sprt vs ab11 INCONCLUSIVE on time (+59 s
  with censored fails). The second city generator on the side 5 / 5 (gap 111-125, 20-28 s); the FIRST not (0-51 % of the
  frames on the side: the round transit at station-keeping speed outlasts the kill, 34-39 s vs ab11's 17-27) -
  city_hold_side UNPROVEN (kept); missile_keep_big no measurable effect (the Catapults go to the sphere's static
  generators, which keep free use; 37-40 left at the city as in ab11). 


## 29 Oct 2026: M13 burst probe and blind_rush

*Moved from: the 29 Oct 2026 list in the combat section (after gate mpb22); the "shape switches, as registered" bullet stays.*

    - *Open: gate mpb24's M13 death (read 29 Oct 2026 ~17:40, log only, not probed):* the same Nomad battleship
      (2212950529, ship=0 to the fight marks: a static, so the launch rule takes the ray-test gap, not the centre-distance
      fallback of radius 666 + 150 m). A blind-spot hold 45-100 m from its spots fired torpedo + missile salvos from
      1,550 down to 147 m from its centre (it at 30 % hp; launches at 155-223 m hit). At 14:58:05.418 a torpedo + 4 missiles
      left at 147 m; hull 0.977 at .715, dead by 06.002 - the salvo's full damage (2,424 + 4 x 1,212 = 7,272) over our
      ~6.4k hull, no missile-outcome logged (dead first). `self_burst` learns only from a burst we survive, so the first
      full salvo that bursts is fatal (mpb22 and mpb24 alike); the gap rays (line, nose, flank at blast + radius + 10 m)
      saw nothing.
    - *The M13 burst probe (29 Oct 2026 ~17:40-17:55; 6 runs Save3989 -> M13.bsdown, `self_burst 0`; legs\
      *_M13-burstprobe*, *_M13-burstprobe2*; 79ff8e7 `to-us` in missile-outcome, 5237831 the launch's hull gaps):*
      2 of 6 died at the battleship (106-114 s), 4 passed. (1) `godmode on` sent mid-session does NOT protect: hull fell
      and run 1 died with it on (the game's flag needs a load, flctl.saves.ps1's note). (2) 120+ rounds at the battleship
      flew and hit, none of the logged ones gone at the launch, the nearest end 41 m from us (`to-us`) (probe-1 died 8 ms
      after a torpedo + missile launch at 151 m: those rounds were never logged - a muzzle burst there not ruled out). (3)
      **Our torpedo hurts us from beyond its blast radius:** probe2-3 launched one at 131 m (line gap 86, nose gap 106,
      nearest hull of six 100 m axis rays 55 m) and lost exactly 2,424 - the torpedo's hull_damage, no other weapon in
      weapon_equip.ini deals it - 0.92 s later, at its impact; probe2-1 lost 2,423 0.17 s after a torpedo's impact
      (launched at 197 m, gap 95); mpb22 read the same (2,424, 0.9 s). The impact was ~80-95 m from us, blast 25 m: the
      gap / flank rule cannot see it, and `self_burst`'s `to-us` test (blast + radius) does not count it. Mechanism not
      read (the game's explosion over a big target's parts?). **Ghidra read (29 Oct, FREELANCER_RE "Explosion damage"):**
      an explosion reaches every object whose collision geometry meets its sphere (radius 25 m for a torpedo); HD
      Edition's Server.dll patch (0x960C) removes the falloff, so a ship it reaches takes the FULL 2,424 - in vanilla a
      graze is 1/3. Still open: why a torpedo whose last live position was 80-95 m from us reached us - the explosion
      centre is not where `to-us` assumes; next, log the centre from the HUD's explosion feed (0x4D6F00) at each
      2,424 loss. Consequence for the rules either way: in HDE any explosion that touches us is full damage, so the
      blast margin buys no partial safety.
      **Centre logging (3b951f3, 6 more runs, burstprobe3 / burstprobe4; 1 died):** `blast-near` (a kind-1 point within
      300 m from the HUD's damage indicator) fired 0 times in all 6, the feed live (round reports ~800 km out). That
      feed takes an explosion only within its radius of the player, so the explosions that cost us were NOT within
      25 m of us - the damage reaches beyond the blast sphere (how the server's FindSphereCollisions finds us is not
      read). The death (probe4-1): hull 0.62 (~4,570 with the shield), a torpedo at 154 m (gap 59, nearest hull 38)
      and a missile pair 0.13 s later = 4,848; dead 0.55 s after the torpedo. Over 30 gap-logged torpedoes the line gap
      does not separate the cases (deaths at 59 / 70 m; launches at 48-65 m with no hit seen - a coarse detector: hull
      read once a second, repairs mask). **So no geometric rule is supported.** The rule the data does support is a
      budget: near a huge target, never have more own explosive damage in flight (torpedo + missiles launched within
      the last ~2 s) than our hull + shield points can take - every death so far was a salvo worth more than what was
      left (mpb24 7,272 over ~6.2k; probe-1 at 0.67 hull; probe4-1 4,848 over ~4,570). Gate after.
      **Against the runners (read late - the owner, 29 Oct 2026; fights.tsv M13 "Order base", dossier M13):** all three
      kill the interceptors FIRST (20-36 s), then cruise / afterburn to the battleship and hold at **0 m** for 21-35 s;
      run 2 fired 11 torpedoes from 0 m with hull >= 0.86; batteries 27-29 -> 0 in bursts.
      **Corrected by run-judge the same evening (the first judge-gate call; verified):** "deaths 0 interceptor kills,
      passes 3-8" was a misread - the fight's kill #1 in every pass IS the battleship (`target-destroyed id=2212950529
      kills=1`, 42.9-67.7 s in; the deaths came at 37.8-45.2 s, before it could fall), the rest are interceptors after
      it. Interceptors are already cleared for ~50 s in the escort phase (6-13 kills; the deaths killed MORE there).
      The steady hull loss is the battleship's turrets: steps of 697 = `nomad_turret01_mark01_ammo` (15 turrets on
      MSN13_Nomad_Battleship); the fighters' 232 steps are the small part. Ours crawls 600 -> 150 m at 40-50 m/s for
      13-31 s (FLMotion) under 4-9 turrets (`here=`), then holds 50-100 m off a blind spot one turret still covers
      (`cover=1`); every death was in that crawl or hold; the fastest pass (probe4-3, bsdown 72.0, gold) never came
      within 434 m. The fatal seconds stay mixed (probe-1: ~2,815 in <= 0.71 s with no own round in flight = 4 turret
      hits or a muzzle burst; probe2-1: a 2,423 hit 0.17 s after its own torpedo struck). Also: every probe ran a
      `+dirty` build; 3 of the 4 probe deaths were on instance 1 (0 of 8 on 2 / 3, unexplained); `blast-near`'s zero is
      not proof (only round reports were logged). **Best-supported change: close to 0 m FAST (cruise / afterburner)
      inside the turrets' reach, the batteries kept for that phase** - the runners' approach; interceptors-first is
      done already, the salvo budget does not explain probe-1. Cheapest proof: one log-only run (shooter + munition of
      every >= 1,000 hp loss), then ~10 runs of the fast 0 m approach on a clean build, instance 1 in the rotation
      (0 deaths in 10 vs 4 in 13: p ~ 0.03).
    - **`blind_rush` (deleted 31 Oct 2026, AUD-A1-5) (97dabbc, OFF - tested, not kept; 29 Oct 2026 evening, 12 runs `M13-rush-*`, clean build,
      `self_burst` at its default ON; run-judge: BAD):** the rush works as a flight (800 -> 200 m in ~3 s at 200 m/s
      vs 13-31 s of crawl) and no run died (0 / 12 vs 4 / 13; Fisher one-sided p 0.057 - not significant, and
      confounded: `self_burst` was off in 12 of the 13 baseline runs), but 1 run FAILED the segment limit (rush-1-4,
      the battleship 108 s in, +29 s scripted -> over 130 s) and rush-3-3 nearly died: an own torpedo launched at 139 m
      ended 32 m from us, `big-hit lost=2423 hull=0.07`, nanobots saved it (the 1 s hull samples showed 0.645 at the
      lowest - near-deaths hide there). The kill got slower: first blind spot -> kill median ~65 s vs ~36 s, torpedoes
      2-12 vs 10-24 - `self_burst` held launches 7-33 times a fight: rounds launched during the rush end at the point we
      fly into (14 m from us at ~1,100 m: a 1,251 m lesson in one run, 267-563 m in the others), and spot-hopping (1-4
      "blind-spot hit ... avoided" a fight) restarts the rush (8-28 rush events a run). **Next, the runners' shape
      whole (run-judge; fights.tsv "Order base"):** one hold at hull contact (0 m), no spot-hopping, batteries kept for
      that hold (we arrive with 3-29), no launches during the rush, `self_burst` learning only from a round end that
      cost hit points (a loss within 0.3 s of its impact), not from distance; first the confound split: 12 runs
      `blind_rush 1` + `self_burst 0`; log hull / shield at 10 Hz (near-deaths).


## 29 Oct 2026: the shape switches and the owner's decision lead-up

*Moved from: the 29 Oct 2026 list in the combat section; the owner's DECISION that follows stays.*

    - **The shape switches (eb30dcb, all OFF) - an interrupted batch, 29 Oct 2026 ~19:10-19:35 (the owner stopped testing;
      12 runs finished: split = `blind_rush 1` + `self_burst 0`, 3 at ~147 fps + 6 at 60 fps; shape = all five switches,
      3 at 60 fps; run-judge INCONCLUSIVE, verified):**
      - The **60 fps runs** are a group of their own (debug `fpscap`, 5234017; PBs / golds were set at ~147 fps; the
        runners' recordings are 60 fps): hull damage 3 / 3 uncapped vs 1 / 6 capped (p ~ 0.05), deaths 1 / 3 vs 0 / 6.
      - **shape60** held ~20-29 m from a hull-contact spot (target distance median 62-118 m vs 166-413 m for split) with
        hull 1.000 all fight at 10 Hz in 3 / 3, but launched almost nothing (5 torpedoes in one run, 1 from ~1.4 km in
        the other two): hold -> kill 41-68 s on guns. Why the launches were refused is NOT logged (launch-held 0;
        salvo-check only under g.hold) - "the blast rule blocks them at contact" is a guess until logged. Contact = safe
        is not proven either: two things changed at once (closer, no torpedoes).
      - **`battery_reserve` never engaged - a bug:** `static float bigNext` outlives `g = State()` at Start (g.time back to
        0), so after the battleship mark's restart its next check came ~50 s late, past the hold. Even fixed it would come
        late: the escort phase spent 4-12 batteries before the mark (17-25 left < the 21 it keeps) - the reserve has to
        start before the battleship's objective, or key on the mission's plan.
      - **The runners play vanilla 1.1** (docs\research\speedrun.md "Material"): there a graze does 1/3, in HD Edition an
        explosion that reaches us does it all (FREELANCER_RE "Explosion damage") - their 0 m torpedoes do not prove ours
        safe. Our ship is the Anubis: hull 4,200 + shield 3,144 = the 7,344 the big-hit lines print (not hull alone - the
        earlier "~6.4k hull" figures in these bullets read totals). split-1-1's death: an own torpedo gone 0.10 s after
        launch 9 m from us (gap 74, nose gap 78: the rays clear) cost ~2,400, then a torpedo + missile at hull 0.81 / shield
        0.06 (~3,600 left) - no repeated damage needed. split-2-1: a 2,423 hit at hull 0.15, 3,231 at 0.20 (10 Hz min 0.129,
        1 Hz 0.72).
      - **Next (on resume):** fix `bigNext` (reset with g.time) and start the reserve earlier; log every launch refusal
        with its reason at the blind hold; replace "no launch at contact" by a budget - launch only while hull + shield
        exceed the explosive damage in flight plus one muzzle burst; then 6 shape runs at the same frame rate as their
        comparison.
    - **After the owner's "their times are the minimum goal" (29 Oct 2026 evening; LEG_DOSSIER M13 "Per segment"): a7d3c31
      (battery_reserve fixed + `salvo-check blind`), d7807f1 (`blind_contact_gap` 55), 12 runs (3 `refusal`, 9 `gap55`,
      ~140 fps, clean builds); run-judge INCONCLUSIVE leaning BAD, verified:**
      - battery_reserve now works: 25-29 batteries at the battleship's start (was 3-29), spent at the hold.
      - Refusals: at the radius+15 hold the line gap read 22-44 m, `blast-torp=0` every time. At 55 m the ship still sits
        7-35 m short of the spot, so blast-torp=0 in 81 of 143 checks; the two runs that launched freely (16 / 14
        torpedoes) took 82 / 83 s, the rest 91-106 s. gap55 bsdown median 93 s - no faster than the crawl batch.
      - The 67.3 s "gold" (gap55-2-1) is luck: the allies had the battleship at 68,356 of 120,000 before our first torpedo.
      - **Own missiles burst at the muzzle at the hold:** gap55-1-2, pairs launched 81-86 m from the centre, gone 0.01-0.18
        s later 6-8 m from us, `big-hit lost=3434 hull=0.33` and `3836 hull=0.23`; nanobots + batteries fired in the same
        frame, so the 10 Hz trace stayed >= 0.668 and `self_burst_damage` read "cost=0" (it reads the total after a
        same-frame repair - it must add the repair back). In HD Edition close-range ordnance at a big hull is the hazard
        however the gap rays read.
      - gap55-3-1 died in the rush, 811 m out, from an explosion centred 9 m from us (kind-1 = an explosion, not a
        collision) at hull 1.0 / shield 0.43 - not ours (nothing launched), not a fighter's gun (232) or a turret (697);
        the allies' missile01_mark05 pairs (4,890 each, radius 16; King / the Order wings) at the interceptors chasing us
        fit (end_tail.mkv 100.4-100.8: missile trails, a blast beside us). Not proven: log the shooter and munition.
      - "Interceptors first" was refuted earlier (kill #1 = the battleship); but we start the battleship with 3 fresh
        interceptors on us, and the runners have cleared theirs - here it matters for the allies' fire into our path.
      - Next options (owner's call, AUTOPILOT.md): A. ordnance from range only - R2's approach volley (every 2 s from
        1,358 m), a stand-off ring >= ~300 m off the hull for the salvos, the contact hold for guns and safety; B. the
        contact hold with every launcher + full shield and battery bursts (the runners' consumables) - the muzzle bursts
        say no; plus in both: no rush with a hostile fighter within ~300 m, self_burst_damage counting the repair, the
        shooter / munition logged at every blast-near and big-hit.


## Q4 slice 1 and 2, judged 30 Oct 2026

*Moved from: Q4 slice 1 (the switch table and the logging stay).*

        **Judged 30 Oct 2026:** probe 3 legs + set q4s1 (24 legs, a = battery_reserve 1, b = + the four switches):
        SPRT INCONCLUSIVE on M13.bsdown (B +5.3 s median, sigma 26.6; deaths A 5 / B 2) - the switches stay OFF.
        run-judge: launch_site_check fails open (a round still flying at the sim horizon passes: 42 of 304 launches);
        the fire site stood 14-63 m off the hull, not 150; own torpedoes do burst beside us (muzzle 16.7 m/s + 4.4 m/s2
        and hit_pts 2: we can outrun our own torpedo, and something sets it off 15-21 m away) but rarely (23 of 2,284 own
        rounds in two days of M13 runs, one death).

*Moved from: Q4 slice 2 and the "(4)" lines (which continue the M13 burst probe item above).*

every one of q4s1's 7 deaths was a
        single loss of 3.9-5.7k in one tick (shield + hull 7,344) at shield 0.08-0.47, four with 9-22 batteries aboard;
        the survival model's `potential` counted hostile SHIPS only and M13's Nomad battleship is a static (15 turrets,
        697 a round, refire 1 s), so `worst` read 3-8x low and neither aboutToFail nor deathSoon fired. With the
        switch on, one volley of each hostile static within 3 km - its guns that can point at us and reach us now
        (`space::VolleyAt`: CEGun::CanPointAt + GetMunitionRange, the munition's hull damage; homing launchers left out) -
        is the largest hit to expect (`max(maxHit, volley)`) in worst / worstSoon, and "under guns" counts it like an
        attacker: a battery before the volley lands, the whole gap when death is near, and battery_reserve's hold gives
        way (it never applies at deathSoon). The repair events log `volley=`. The runners keep the shield full the
        same way (batteries 9-10 at once on each failure, fights.tsv "Order base").
        **Judged 30 Oct 2026, set q4s2 (24 legs, a = battery_reserve 1, b = + static_volley 1): SPRT REVERT** by the
        fails guard (B failed 4 of 12, A 2 of 12; B's bsdown median 5.5 s faster) - the switch stays OFF. The reader
        works (volley= 1.4-4.2k at the battleship) and b bursts batteries as the runners do (21 -> 13 -> 5 in 11 s), but
        in BOTH arms nearly every run ends with 0 batteries: 29 x 300 shield does not outlast our exposure, so spending
        sooner only moves the empty point earlier. Two slices at the battery / launch side failed: the lever left is
        exposure - how long and where we sit inside the turrets' cover (the runners reach 0 m from 1.4 km in ~7 s
        and kill it in 30-46 s; ours crawl 600 -> 250 m at 48-80 m/s for ~22 s first).
        run-judge on q4s2: batteries left on reaching 300 m separate the outcomes (>= 7: 15 / 15 lived; <= 4: 4 / 8
        died); the 1200 -> 200 m crawl (16-27 s, median 47 m/s, station mode at throttle -1 from ~840 m) drains them;
        raw seconds inside 300 m do not separate them. **Set q4s3 (a = battery_reserve 1, b = + blind_rush 1): SPRT
        INCONCLUSIVE** (bsdown B +8.0 s median, fails A 2 / B 3 of 12) - blind_rush stays OFF. **Q4 parked after three
        slices without a verdict (30 Oct 2026):** at sigma 26.6 s (the last 10 full passes) 12 runs a side cannot resolve
        an effect under ~20 s on bsdown; the next Q4 work comes after Q3's variance cut (fixed dt, T3 seeds) or with a
        change whose effect the runners' data puts far above that (the whole runners' shape: cruise in, 0 m, batteries
        on each failure - one switch set, not one switch at a time).
      (4) The deaths all had kills=0 in the battleship fight (survivors killed 3-8 fighters) and batteries 0 in every run:
      Nomad fighters on us + own torpedo damage + no batteries. Rule candidates (gate after): learn from the signature (a
      hull loss = a torpedo's damage within 0.3 s of our torpedo's impact -> `self_burst` keep for that target), or no
      torpedo inside a huge target's radius; the fighters-first question (dossier M13) beside it.


## Verified 17 Oct and sweep state 21 Oct 2026

*Moved from: the end of the Objective-mode section (after "Timers").*

- Verified 17 Oct 2026, all with the game out of focus: combat 90 s = 4 kills (2.7/min, the bot's rate; dev) and
  release 4 kills in 81 s until no enemies were left; dock on Fort Bush (24 s); escort (holds ~270 m); formation
  (joins, ~130 m); every op crash-free on target death. Not yet: real pad, solar targets in combat, Turret Mode.
- Verified (predictive layer, 17 Oct 2026, out of focus): godmode combat 7 kills in 107 s; loot phase collected 6
  items; without godmode vs 8 police: batteries used as the shield fell, left at hull 0.59 (threat 7.9 vs strength
  2.0), got away with 0.55. Background test starts never take focus or sound (FLVIBED_BACKGROUND, CBT hooks on every
  thread, z-order bottom, render client silent flag; user confirmed: not even the mouse moved). 19 Oct 2026: the
  intro's DirectShow renderer (quartz.dll, loaded with the first movie) raised the game over the user's fullscreen
  browser for a moment: modules are now patched as they load (LdrRegisterDllNotification) and an in-context WinEvent
  backstop sends any raised game window back down inside the raising call (z-order watcher: 0 of 1002 checks above).
- **Sweep state (21 Oct 2026 04:23; history - the current per-leg state is `docs\SPLITS.md`):** full parallel sweep 17 / 17 PASS, 0 deaths; M01a ~515 s, M01b ~650 s, M02 ~940 s,
  M03 ~950 s, M04 ~1030 s, M05 ~1230 s; fight matrix 30 / 32. Open: an unexplained 4 m 40 s game freeze (M05 flee after
  two missile hits, resumed by itself; the check-ins now catch a freeze and save where the threads run:
  `..\..\tools\flfreeze.ps1`); fight-matrix variance (heavy_d12_x2, ku_gunboat under load). Leeds ring dock ~5 s over the
  prediction: fixed 21 Oct 2026 (the planet's zone in the way, above). Own missiles after the c039118 run-in: 15 / 18 hits
  in the 06:17 M04 sweep (misses: a second missile on a dying target, a 175 m/s crossing target, a head-on pass).
  Current work and queue: `..\..\HANDOFF.txt`.
