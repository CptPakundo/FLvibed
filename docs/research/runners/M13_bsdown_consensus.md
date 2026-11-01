# M13 battleship (M13.battleship -> M13.bsdown): escalation step 1 - the consensus

Escalation step 1 of `docs\AFK_PLAYBOOK.md` 13a (the owner, 31 Oct 2026 05:03): two analysts, one from the runners'
side, one from the autopilot's, then this synthesis by the driver (31 Oct 2026 ~05:10, HEAD 566d8cd0). [M] measured,
[I] inferred. Why escalated: NO PROGRESS on M13 over 4 sets; bsdown +25 vs par 45; two deaths in four legs at Nomad
battleships' close holds (ch3-i1, ch4-i1).

## What each side found
**Runners** (reference\trace_run1-3.tsv at 0.2 s, fights.tsv 55-57): R1 / R2 / R3 kill +20 / +31 / +25 after the 24425
banner. R2 / R3 afterburn in at 190-200 m/s from 1.4-2.2 km and reach "0 m" (the HUD surface distance) without
braking; R2 fires torpedo + Slingshot + Catapult every 1-2 s on the way in. At the hull they keep moving - afterburner
pulses 106-198 m/s, 66-88 m/s between - and every one of their 5 shield failures lines up with a slow-down [M, n=5];
batteries 8-10 at once only at a failure [M]. Their 0 m ordnance relies on vanilla's 1/3 graze [I]: not in HDE.
**Ours** (5 legs, code + logs): coast 300 m/s to ~730 m, then station keeping (0.4 x distance, <= 60 m/s, no
afterburner: controller.cpp 1462-1479) to ~160 m by +24 s, then the blind spot `spot=0,94,94` (0.2 x radius, the
smallest step) at a 36-64 m hull gap, where `torp_reach` 100 (16 windows), the 47 m hull-beside rule (11) and
`self_burst` "inside 331 m" (9) hold the launches: 3.5-4.2k hp/s before, 2.4-3.0k/s after (ceiling ~4.5k) [M]. The
blind-spot score never looks at the hull gap - our own rules contradict each other [M code]. ch2-i2 (0.6 s under
150 m) killed at +30 vs +44..+59 [M].
**Tried before** (AUTOPILOT.md verdicts): standoff_hold (safer, slower: the slide cost aim, launch_site_check held half),
ordnance_site (its site stood 14-63 m, not 150 - never really flown), blind_contact (no launches at contact),
bs_approach (the dash worked, 799 -> 205 m in 3 s; the spot sat in a notch), big_cruise_in (early, but the hold's
gating set the kill time).

**The two deaths** (run-judged ~05:08) [M logs, cause I medium]: both 4-5 s after a blind-spot re-anchor ("spot hit:
avoided from now"), crawling 8-49 m/s in transit to the next spot with 3-4 turrets bearing on a 0.13-0.27 shield, one
volley of ~3.9k / ~5.0k (697 a turret round) - ch4 with 8 batteries unused (the shield never read "failed"). The runners
never crawl or re-anchor under the turrets. So the fail state and the time sink are the same place: the close hold.

## Consensus
1. **Arrive fast:** the rush branch (`rushOn`, BrakeDistance + 60 m) for every blind-spot fight - bs_approach proved the
   dash; launch the ranged volley from 1.4 km in (both).
2. **Hold outside our own reach:** a point whose line and nose gaps are >= 100 m (the torp_reach rule's own number,
   above the measured 80-95 m self-damage reach), the turret cover deciding among those - the standoff site, not the
   0.2 x radius spot (both). `self_burst` off (a distance proxy of the direct reach rules; autopilot view).
3. **Batteries** 8-10 at once on a failure, none on the way in; blind_no_pass on (both; existing).
4. **Open - motion at the hold:** the runners never sit still (n=5 failures at slow-downs) vs mm5 / mm7 where the slide
   cost aim. Not settled by reasoning: **the probe flies both**.

## Result (31 Oct 2026 ~05:38): step 1 FAILED -> step 2
kb1 (8 runs): the site never held (inside the hull envelope) -> outside test; kb2 picked none -> pool 24; kb3 (4): the
site held (gap 183, 0 launches held, 13 torpedoes + 12 missiles), 70-77 s, one DEATH held still under its 4 covering
turrets (1.00 -> dead in 7 s, 0 batteries); kb4 (moving, 120 m slide at 60 m/s, 3 runs): 2 DEATHS, on target 8 of 82 s,
the pass 113 s - stopped at the verdict. The clear stand-off sites are the covered ones; the runners' 0 m is not ours
in HDE. **The judge's measurement (kb2 / kb3) [M]:** once in place the damage rate is at the loadout ceiling, 4.1-5.7k
hp/s - the time goes into the ARRIVAL (first damage +16..22 s after the banner); bsdown = the kill + ~28.5 s, so par
45 needs the kill by ~+16.5 - i.e. firing before the banner, as R1 does. And no mine has been dropped in any run (R1: 10
at the battleship). Next for this section: step 2, a new data pass (the runners' real hull distance and face from full-res
frames, the battleship's damage curve and their launch cadence at 5 Hz) - the target meanwhile M02 (the review).

## The probe (no build)
From the FIT fixture `research\saves\scenes\SaveS415.fl`, `-StopAt M13.bsdown`, the set's switches plus
`standoff_hold 1, ordnance_standoff 120, launch_site_check 0, self_burst 0, rush_hold_fire 0, blind_no_pass 1`:
arm S (still) `standoff_slide 0`; arm M (moving, the runners' >= 60 m/s) `standoff_slide 120, standoff_slide_speed 60`. Judged on the kill time after the banner (goal
<= +32, par needs ~+20), the held-launch lines (<= 3), the `standoff-hold` gap / cover lines, the lowest hull, deaths.
If the site's cover is >= 2 guns in both arms: build a BlindSpot candidate filter (line gap >= torp_reach) instead.
Expected [I]: 3.5-4.0k hp/s through the hold, kill ~+28..32, bsdown ~53-57 (still ~10 s over par: then step 2, a new
data pass - the runners' real hull distance and the battleship's damage curve at 5 Hz).
