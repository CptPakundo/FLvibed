# M02 Ashcroft (M02.ashcroft -> M02.pod): escalation step 2 - the second data pass on the recordings

> **P0 ANSWERED (the driver, 31 Oct 2026 ~17:10, HDE) [M]:** an engine kill at full cruise ends cruise at once
> (speed held ~300 m/s); the engine back on does NOT resume it - 300 -> ~90 m/s within 0.5-0.7 s, a new ~8.5 s charge
> needed (SaveS419, `tap USER_MANEUVER_ENGINEKILL` twice; FREELANCER_RE.md). **CORRECTION (the owner, 31 Oct 2026):**
> the "R3 taps keep cruise" reading below is WITHDRAWN - an inference from OCR'd speed numbers; in vanilla too an engine
> kill ends cruise and the velocity cannot be redirected while it lasts (the ship coasts straight on its vector, the nose
> only aims). For ash_ride: no taps; a coast is a straight line with him dead ahead; the loop turns under cruise only.

## Verification (31 Oct 2026, adversarial frame pass)
Every [I] claim and every OCR-only [M] claim a design builds on was re-read from 5 fps full-res frames (ffmpeg,
video stream only; frame n of a window = t0 + (n-1) x 0.2 s, alignment re-checked at R1 v1877.0: 216-M, 295, Javelin
16). Sheets: `research\evidence\speedrun\topics\combat\frames\ash_verify_*.jpg` (18). Tags in the text below:
[verified frames ...] / [corrected: ...] / [withdrawn: ...] / [undecided: ... probe].
- **Verified:** the escort left at shield 0 + hull ~0.35-0.6 with CRUISE ENGINE CHARGING - WEAPONS OFF-LINE on screen;
  the escort's hull keeps falling while the runner is km away (R3's info panel: Health 15 at v1930.0, 13 at v1933.0);
  R1's head-on pass and U-turn under cruise (flare on, 288-296 m/s, no charge text); the stable gap at ~299 m/s before
  every coast (R1 213-214 m 2.8 s, R2 112-116 m 6.6 s, R3 87-91 m >= 2 s); the coast = flare off + guns on from the same
  frame; the decay ~3 m/s per s; the gap opening; Javelins (R1 at ~216 / ~231 m, R3 at ~90 m); shield-then-hull bar
  times; the TRACTOR banner right after the eject. **New, frame-read, vanilla:** after R1's coast the speed falls
  281 -> 202 -> 88 m/s in 0.8 s (v1881.6-1882.4) = engine back on after an engine kill drops cruise speed, exactly the
  HDE P0 result: the owner's correction holds in the recordings too.
- **Corrected:** the escorts die later than the note said - R1's Dowling is still listed (3.7 K) at v1860.0 and gone at
  v1860.2 (~24.8 s after the press, not v1858.3 / 22.9 s: v1858.2 was a deselection, the row turned red); R3's Harrison
  is listed to v1933.6 (~38.3 s after the press, not v1925.7 / 30.3 s). Ashcroft's first appearance = R1 v1860.2 at
  2.2 km (banner v1860.2; not the v1861.7 selection at 2.1 km), R2 banner v1822.4, R3 v1933.6 at 843 m (banner v1933.8).
  Follow -> eject: R1 +21.4, R2 +27.4 (bracket yellow v1849.8; pod explosion v1851.4 = +29.0), R3 +20.2 (not +18.2).
  **R3 had no engine-kill coasts or holds before the fight:** the 299 -> 242 (v1919-1921) and 285 -> 247 (v1930-1933)
  dips recover to 281-297 within ~2 s with no CRUISE ENGINE CHARGING text and no 300 -> 90 drop - by the corrected
  mechanic they are cruise turns (forward speed falls while the velocity swings; the ship is off-centre, the camera
  lagging), not engine kills. His ramp: R1's closure 1538 -> 404 m in 3.0 s (label, v1863.4-1866.4) at our 296-300 gives
  him **>= 60-80 m/s toward us** (a lower bound, geometry unknown), R2 (behind him) 362 -> 96 m in 1.4 s gives **<= ~105
  m/s** (an upper bound) at +4.6-6.0 s; ~230 / 275 / 292 / 299 at +11.4 / 12.0 / 12.6 / 13.2 s (R1 label gaps); ~300 is
  reached ~13 s after his appearance, not 10-12.
- **Withdrawn:** "R3 engine-kill coast at 247-261 m/s toward the spawn / held 3 s waiting" (both cruise turns, above);
  ash_ride step 1's "engine-kill coast ... re-cruise / tap" hold.
- **Undecided:** who kills the 4th escort (King / police / other): the frames show only list ranges (escort 3.7 K, King
  4.1 K from R1); probe in HDE: leave Escort 4 at shield 0 / hull ~0.5, fly 4 km off, log its damage events with the
  inflictor (or watch it from a free camera) until `escorts_destroyed`, n=3. His exact speed in the first ~11 s (only
  bounds above); probe: log Ashcroft's speed + cruise state at 10 Hz from his spawn in HDE (one run). The damage rates
  stay [I] (not frame-decidable).
- **For the design (ash_ride):** step 1 stands for the escort (shield 0, hull <= 0.6, cruise beside it) but its wait is
  **cruise loitering (turns / circles at 250-300 m/s near the spawn, R3 even read the escort's health in the info
  panel), no engine kill**; the 35 s fallback is too short (R3 waited 38.3 s and still won): make it >= 45 s or tie it
  to the escort's hull read. Step 2 stands (he is slow for ~6 s, ~300 only after ~13 s). Step 3 stands as written
  (stable gap >= 1.2 s, both ~299, guns from the first frame, Javelins <= 250 m); its re-cruise is a **charge** (no
  tap: vanilla frames show the 300 -> 90 drop too). The probe's escort-wait budget should expect 25-38 s.

Escalation step 2 of `docs\AFK_PLAYBOOK.md` 13a (step 1 = `M02_ashcroft_consensus.md`; its `spawn_charge`, flown as
probes sc1 / sc3 / sc4, brought 0 chase kills in 6 runs: `modules\autopilot\docs\AUTOPILOT.md` "Flown 31 Oct 2026 ~15:35",
"Probe sc3"; HANDOFF sc4). Research only, 31 Oct 2026, no game, no build; checked against HEAD 3e5f1385 (dirty paths
not read). [M] measured, [I] inferred. Times are video seconds (v) of each recording; "+n" = after the FOLLOW
ASHCROFT banner. Window: the 3rd escort's death -> the pod (R1 v1830-1882, R2 v1796-1852, R3 v1878-1955).

## New datapoints and how they were taken
- **10 Hz HUD trace** (`research\evidence\speedrun\reference\trace_run<N>.tsv`, the 22 Oct pipeline: speed, cruise /
  charge %, target name / distance / hull / shield bars, ammo; field accuracy in its README) cut to the windows:
  `research\evidence\speedrun\topics\combat\data\ashpass\trace_r<N>.tsv`. Target bars alternate with misreads
  (0.02-0.13 rows between 0.9 rows): values below are the runs of consistent reads, checked on frames.
- **5 fps full-res frames** (ffmpeg, video stream only) of the three windows, read by eye: engine flare on / off (the
  cruise flare is a large white bloom behind the ship; it vanishes at an engine kill), gun bolts / muzzle glows,
  Javelin smoke, the target bracket's distance and bars, the CRUISE ENGINE CHARGING text. Sheets (git-ignored):
  `research\evidence\speedrun\topics\combat\frames\ash_r1_coast_0.6s.jpg`, `ash_r2_coast_1hz.jpg`,
  `ash_r3_end_0.6s.jpg`, single frames `ash_r1_v1835.4_escort_left.jpg`, `ash_r1_v1877.0_coast_fire.jpg`,
  `ash_r2_v1802.8_escort_left.jpg`, `ash_r2_v1823.6_at_spawn.jpg`, `ash_r3_v1933.8_ek_wait.jpg`; the sheet script
  `topics\combat\scripts\ash_sheet.py`. Trace-to-frame alignment checked at R1 v1877.0 (216 m, 295 m/s, Javelin 16 on
  both) [M]. OCR = the trace's flocr pass; "frame" = my reading.
- **The script facts behind the numbers** [M m02.ini, HDE install]: `escorts_destroyed` (1541-1547: Cnd_Destroyed
  Escort 4 by anyone -> Act_SpawnShip Ashcroft at (-28500, 0, 51100) (MsnShip 273-277), `start_stage4_attack`);
  `start_run1` gives him `ashcroft_stage2` at once (goto_cruise to (-22099, 0, 75896), 1589-1591); **`ashcroft_health_35`
  (1734-1737: Cnd_HealthDec 0.35 -> `destroy_him` -> Act_Destroy = the eject)** - the kill needs his shield + 65 % of
  his hull, not 100 %; "Destroy Ashcroft's Ship" (22263) is set only by `disable_ash_nag` (1689-1693: King within 5 km of
  the run point, then the player within 1 km of him) - **no runner ever sees that banner** (banners.tsv: FOLLOW ->
  TRACTOR) [M]. pi_elite hull 3,600 (shiparch 4243), our measured kill total dealt=3600 (sc1 / sc3 / sc4 logs) [M].
- Loadouts [M `runs\<run>\loadouts.md`, frames]: R1 Patriot 3x Justice Mk III + Javelin (frame v1877.0 weapon list);
  R2 Starflier Justice Mk II + Mk I (no missile fired); R3 Rhino Javelin + 2 Justice Mk III + Stunpulse turret + 4
  Justice Turret Mk II (weapon list in `ash_r3_end_0.6s.jpg`). Ours: `guns=2` (sc3-c_i2 log 16:16:30) [M].

## Per runner
| | R1 TM4S13 | R2 OldOak | R3 SydMontague |
|---|---|---|---|
| (a) last escort | Andrew Dowling | Ivan Buhr | Kevin Harrison |
| its bars when the runner leaves (= cruise press) [M OCR, frame] | shield 0, hull 0.36 at 93 m (v1835.4) | shield 0, hull 0.59 at 80 m (v1802.8-1803.0) | shield 0, hull 0.56-0.58 at 242 m (v1895.4-1896.2) [verified frames `ash_verify_r1_press_*`, `_r2_press_*`, `_r3_press_*`: shield bar empty, hull bar ~1/3 / ~0.6 / ~0.55, R3 226-M at v1895.4; R1 + R3 show CRUISE ENGINE CHARGING - WEAPONS OFF-LINE] |
| how the runner brought it there [M] | shield 0.87 -> 0 in 3 s (v1828.4-1831.4), hull 0.96 -> 0.36 in 4 s | shield 0.46 -> 0 (v1796.4-1798.6), hull 0.92 -> 0.59 in 4.2 s | shield 0.60 -> 0 (v1882.0-1884.4), hull 0.98 -> 0.56 in ~11 s, two aborted charges (v1885.5, v1886.6) |
| its hull afterwards, nobody of ours firing (charging / cruise = weapons offline) [M OCR] | 0.34-0.36 while charging beside it (89-96 m, v1835.9-1836.9); 0.09-0.22 at 1.96-2.9 km (v1846-1849); 0.20 at 3.1-3.5 km (v1852-1855); 0.07-0.09 (v1857.6-1858.2) | not selected after v1803.0 (no reads) | 0.44 at 191 m (v1898.6, charging); 0.27-0.30 at 4.4 km (v1922); 0.08-0.11 at 3.6 km (v1923.8-1925.6) [R3 verified frames `ash_verify_r3_wait_spawn_*`: info panel "Health: 15 / Shield: 0" at v1930.0, "Health: 13" at v1933.0 - it keeps losing hull at 3.5-3.8 km; R1's values OCR-only] |
| who damages it [I by elimination] | King / police (the runner's weapons are offline from v1835.3) | unknown (no reads) | King / police (same reason) [undecided: frames show only list ranges (R1: escort 3.7 K, King 4.1 K); someone else damages it (verified, row above), who is not visible; probe: HDE, Escort 4 left at shield 0 / hull ~0.5, log its damage inflictors until `escorts_destroyed`, n=3] |
| where / when it dies [M last read -> target lost] | v1858.3, 3,400 m from the runner (HUD list), 22.9 s after the runner's cruise press | unknown; before v1822.8 (Ashcroft up) = within 19.8 s of the press | v1925.7, 3,600 m from the runner, 30.3 s after the press [corrected: R1 still listed "3.7K Rogues - Andrew Dowling" at v1860.0, gone v1860.2 when Ashcroft is listed = **~v1860.1, ~3.7 km, ~24.8 s after the press** (v1858.2 was a deselection: the row turns red); R2 unknown; R3 listed to v1933.6 (3.2-3.5 K) = **~v1933.6, ~38.3 s after the press**; frames `ash_verify_r1_list_*`, `_r1_escort_spawn_*`, `_r3_list_*`] |
| (b) cruise press -> 300 m/s [M] | v1835.3 -> charge done ~v1840.3 (92 % at v1839.9) -> 299 at v1844.4 | v1802.9 -> 99 % v1807.8 -> 270 v1811.2 -> 299 v1812.0 | v1895.3 -> done v1900.3 -> 271 v1904.2 -> 299 v1906.8 |
| the runner when the escort dies [M] | in cruise at 273-299 m/s (a turn dip 300 -> 273, v1857-1859) | in cruise 296-300 m/s since v1811.2 (assumed: death time unknown) | engine-kill coast at 261 m/s toward the spawn (v1919.0-1924.4 coast, cruise again v1924.6) [withdrawn: no EK - 299 -> 242 -> 261 -> 297 with no charge text and no 300 -> 90 drop = a cruise turn (`ash_verify_r3_cruiseturn_*`); and the escort died at v1933.6 (row above), when R3 was in cruise at ~251-259 m/s] |
| the runner when Ashcroft appears [M] | cruise 298 m/s; Ashcroft selected v1861.7 at 2,100 m (list) | cruise 292-296 m/s; first label v1822.8 at 457 m (screen edge), v1823.6 **212 m** beside him (frame; OCR 165) | **engine-kill coast at 247-249 m/s, held 3 s waiting** (v1930.6-1933.6); selected v1933.8 at 843 m (list) [corrected: R1 cruise 295-299, Ashcroft **listed v1860.2 at 2.2 km**, banner v1860.2 (`ash_verify_r1_list_*`, `_r1_banner_*`); R2 verified (293 m/s, flare on, 212-M beside him v1823.6; banner v1822.4); R3 **in cruise, turning** (285 -> 247 -> 281 with no charge text; info panel open on Harrison), Ashcroft listed v1933.6 at 843 m, banner v1933.8 (`ash_verify_r3_wait_spawn_*`, `_r3_banner_*`)] |
| distance to the spawn at his appearance [M = his first range, he has not moved yet] | ~2.1 km, closing head-on | ~0.2-0.46 km, passing beside | ~0.84 km [verified frames, R1 corrected to 2.2 km at v1860.2] |
| time from cruise reached to his appearance [M] | 17.3 s | 11.0 s | 27.1 s (with two EK holds) [corrected: 15.8 s (v1844.4 -> v1860.2) / 10.4 s (v1812.0 -> banner v1822.4) / 26.8 s, R3's dips being cruise turns, not EK holds] |
| (c) his speed ramp [M gap + own speed; his = ours - d(gap)/dt] | head-on: closure 370-380 m/s at our 293-300 -> **him ~75-80 m/s** (v1863.5-1866.5, gap 1,524 -> 384); the runner passes him (~v1866.5) and U-turns in cruise (frames v1866.8-1871.2, speed 288-299); behind him: gap 284 (v1871.5), 247, 224, 217, 214 (v1873.5), 213 to v1875.5 -> him **~222 / 252 / 284 / 293 / 299 m/s** at 0.5 s steps (v1871.5-1873.5) | gap 290 (v1827.0) -> 103 (v1828.4) at our 285-297: him **~160 m/s** at ~+4.5 s; 97 -> 119 (v1831.6-1832.6, opening 22 m/s: him ~315 overshoot of our 292-297); **114 m steady at equal 299 m/s v1833.6-1840.4** | list ranges only (1 s lag): 843 (v1933.8), 362 (v1936.8), 223 (v1939.8), 173 (v1942.8), 45 (v1945.8: alongside), 96 (v1949.0), 89 (v1951.8); five EK taps of 0.8-4 s (speed 235-262), each back to 279-295 within 0.2-0.8 s **with no CRUISE ENGINE CHARGING text** (frames v1934-1948) [corrected R1: labels 1538-M (v1863.4), 1250 (v1864.2), 951 (v1865.0), 723 (v1865.6), 588 (v1866.0), 404 (v1866.4) at our 296-300 = closure 340-460 -> him **>= 60-80 m/s toward us (a lower bound; the geometry is not exactly head-on-known)** at +3.2..+6.2 s after his appearance; head-on pass (target swings behind, ~v1867.3 by extrapolation; list "45" at v1869.8 lags) and U-turn **under cruise verified** (flare on throughout, 288-296 m/s, no charge text; `ash_verify_r1_headon_uturn_*`); behind: 274 (v1871.6), 233, 218, 214 (v1873.4), 213 to v1876.2 -> him ~230 / 275 / 292 / 299 (`ash_verify_r1_behind_*`). R2 corrected: R2 behind him, turning: 362-M (v1827.0) -> 240 (v1827.6) -> 96 (v1828.4) at our 285-297 -> him **<= ~105 m/s (upper bound)** at +4.6..+6.0 s after the banner; the 97 -> 119 opening (v1831.6-1832.6) is undecided (R2 turning: speed 290); 112-116 m steady at 299 v1833.6-1840.0 verified (`ash_verify_r2_spawn_close_*`, `_r2_coast_*`). R3: the "EK taps" are withdrawn (box at the top; by the frames they behave like the cruise turns of v1919-1925 / v1930-1933)] |
| his speed ~300 reached [M] | ~v1873.5 = 11.8 s after first selectable | ~v1832.5-1833.6 = ~10-11 s after first seen | not resolvable (list) [corrected: R1 ~v1873.4 = **~13.2 s after his first listing** (v1860.2); R2 ~v1833.6 = ~11.2 s after the banner (v1822.4); exact ramp undecided: probe = log his speed + cruise state at 10 Hz from spawn in HDE] |
| (d) the coast: engine kill [M frames: flare off, bolts on] | ~v1875.5 (flare at v1875.2, gone v1875.8) after **2 s at a stable 213 m** | ~v1840.6 (flare to v1840.0, gone v1841.0) after **7 s at a stable 114 m** | ~v1951.5 (flare to v1951.2, gone v1951.8) at **87-89 m**, 3 s at ~89-96 m before [verified frames, refined: R1 flare v1875.4, gone v1875.8 + muzzle glows, gap 213-214 from v1873.4 (2.2-2.8 s); R2 flare v1840.0, gone + bolts v1840.4 (coast ~v1840.2), gap 112-116 for 6.6 s; R3 flare v1951.2, gone + bolts v1951.6, gap 87-91 m from v1949.0, both at 298-299] |
| speed through the coast [M OCR, frames] | 299 -> 296 (v1876.4) -> 292 (v1878.0) -> 288 (v1879.4) -> 284 (v1880.6) -> 281 (v1881.8) | 299 -> 297 -> 294 -> 289 -> 286 -> 283 -> 280 -> 277 -> 274 -> 272 -> 270 -> 267 (1 Hz, v1840-1851) | 299 -> 297 -> 292 -> 288 -> 285 (v1951.4-1954.0) [verified frames: R1 299 / 296 (v1876.4) / 293 (v1877.6) / 290 (v1878.8) / 287 / 285 (v1880.4) / 282 (v1881.2); R2 294 (v1842) / 286 (v1844) / 280 (v1846) / 274 (v1848); R3 297 (v1952.4) / 291 (v1953.2) / 284 (v1954.0): ~3 m/s per s] |
| gap through the coast [M] | 213 -> 216 (v1877.0) -> 222 (v1878.0) -> 231 (v1879.0) -> 243 (v1880.0) -> 258 (v1881.0) -> 263 (v1881.3) | 114 -> 115 -> 120 -> 127 -> 137 -> 150 -> 166 -> 185 -> 206 -> 230 -> 258 -> 287 (1 Hz, frames) | 89 (list; frames 87-95 m) [verified frames: R1 213 -> 222 (v1878.0) -> 238 (v1879.6) -> 249 (v1880.4) -> 262 (v1881.2); R2 114 -> 120 (v1842) -> 137 -> 166 -> 206 (v1848) -> 241 (v1849.4) -> 287 (v1851.0); R3 88 -> 95 (v1953.6)] |
| guns [M frames] | muzzle glows from v1875.8 to the eject | bolts from v1841.0 to the eject (`ash_r2_coast_1hz.jpg`) | bolts + turrets from v1951.8 [verified frames: from the first flare-off frame in all three (R1 v1875.8, R2 v1840.4, R3 v1951.6)] |
| missiles [M ammo count, frames] | Javelin v1877.0 (16) and v1879.0 (15), at 216 / 231 m; smoke at the target from v1878.2 | none | Javelin v1952.3 (34) at ~89 m [verified frames: R1 17 -> 16 between v1876.8 and v1877.2 (215-217 m), 16 -> 15 between v1878.8 and v1879.2 (229-233 m); R3 35 -> 34 between v1952.0 and v1952.4 (89-90 m)] |
| his shield / hull [M OCR + frames] | shield 0.97 (v1876.1) -> 0.50 (v1877.3) -> 0.11 (v1878.6) -> 0 (v1878.9): **2.8 s**; hull 0.94 (v1879.0) -> 0.80 (v1880.0) -> 0.46 (v1880.4) -> 0.34 (v1881.2): **2.2 s** | shield 1.00 (v1840.4) -> 0.62 (v1842.0) -> 0.24 (v1843.4) -> 0 (v1844.6): **4.2 s**; hull 0.99 (v1844.4) -> 0.84 (v1846.0) -> 0.66 (v1847.6) -> 0.53 (v1849.0) -> 0.33 (v1851.2): **6.8 s** | shield 0.33 (v1952.2) -> 0 (v1952.6); hull 0.98 (v1952.8) -> 0.68 (v1953.8) -> eject (frame v1954.2): **~2.7 s** in all [verified frames (bar lengths, R1 `ash_verify_r1_coast_*`): shield bar full to v1876.4, empty v1878.8; hull bar full to ~v1879.6, ~half v1880.4, ~1/3 v1881.2, explosion v1881.6; R2 shield bar gone by v1844.0; R3 explosion + TRACTOR banner v1954.0 (`ash_verify_r3_coast_*`)] |
| coast length to the eject [M] | ~6.0 s (eject = explosion frame v1881.8) | ~10.0 s (bracket turns yellow v1850.0; TRACTOR banner v1851.0) | ~2.7 s (explosion v1954.2) [verified / refined frames: R1 ~6.0 s (v1875.6 -> v1881.6); R2 ~9.6 s to the yellow bracket (v1849.8), pod explosion + TRACTOR v1851.4 (`ash_verify_r2_eject_*`); R3 ~2.4 s (v1951.6 -> v1954.0)] |
| damage rate in the coast [I from the bars: shield ~821 (our log), 65 % hull = 2,340 of HDE 3,600] | ~290 shield/s; ~1,060 hull/s with 2 Javelins | ~195 shield/s; ~345 hull/s, guns only, gap 137 -> 287 m | ~1,150/s (8 weapons + Javelin, gap ~90 m) [stays I: not frame-decidable (the bar times are verified; the HP totals are HDE / our log)] |
| (e) eject vs banners [M] | eject v1881.4-1881.8 -> TRACTOR v1882.0; **no "Destroy Ashcroft's Ship"** | v1850-1851 -> TRACTOR v1851.0; none | v1954.2 -> TRACTOR v1954.0 (OCR 1 fps); none |
| Follow banner -> eject (pod split) [M] | +21.6 | +27.5 | +18.2 [corrected from the banners' first frames (v1860.2 / v1822.4 / v1933.8): **+21.4 / +27.4 (yellow bracket; +29.0 to the pod explosion) / +20.2**] |
| after: re-cruise [M] | cruise press v1882.0 at 246 m/s (charge from the coast) [verified frames + new: speed 281 (v1881.6) -> 202 (v1882.0) -> 88 (v1882.4) = engine on after the EK coast drops to engine speed, as in the HDE P0] | press v1851.6 at 214 m/s | press v1954.5 at 271 m/s |

## What the numbers say
1. **They leave the last escort with its shield gone and 36-59 % hull, right beside it, and press cruise there**
   [M a]. NPCs finish it 23-30 s later while the runner is 3.4-3.6 km away in cruise (R1, R3) [M distance, I who]. The
   price of that non-determinism: R3 waits 27 s at cruise / engine-kill coast near the spawn [M]; R1 17 s, R2 11 s.
   [first sentence verified frames; corrected: NPCs finish it ~24.8 s (R1, ~3.7 km) / ~38.3 s (R3, ~3.2-3.5 km) after
   the press; R3 waits ~27 s **in cruise, turning** (no engine kill), R1 ~16 s, R2 ~10 s; who kills: undecided]
2. **At his spawn every runner is at 247-299 m/s** - two in cruise (R1, R2), one in an engine-kill coast held 3 s at
   ~248 m/s 0.84 km off (R3) [M b]. Their distance: R2 0.2-0.46 km passing beside, R3 0.84 km, R1 2.1 km head-on.
   [corrected: all three **in cruise** at 251-299 m/s (R3 turning at ~251-259, not an engine-kill coast); R1 2.2 km]
3. **His cruise charge is slow and long in the recordings: ~75-80 m/s for >= 4.8 s, ~160 m/s at ~+4.5 s (R2), ~300
   m/s only 10-12 s after he is first selectable** [M c]. The runners use that window to get behind him (R1: a head-on
   pass and a U-turn at cruise; R2: already beside him, turns in) and **sit at a stable gap at equal 299 m/s before
   the coast**: R1 213 m for 2 s, R2 114 m for 7 s, R3 ~89-96 m for 3 s [M].
   [corrected: his speed is bounded, not measured - >= 60-80 m/s (R1, +3.2..+6.2 s) and <= ~105 m/s (R2, +4.6..+6.0
   s), ~230-299 at +11.4..+13.2 s; ~300 ~13 s after his appearance. Verified frames: R1's head-on pass + U-turn under
   cruise; the stable gaps R1 213-214 m 2.2-2.8 s, R2 112-116 m 6.6 s, R3 87-91 m >= 2 s, all at 298-299]
4. **The engine kill comes only after both are at full cruise and the gap is stable at 87-213 m** [M d]. Then the speed
   decays 299 -> 281-267 over 6-10 s (2-3 m/s per s, vanilla) and the gap opens 213 -> 263 (R1) / 114 -> 287 (R2).
   Guns fire from the first coast frame; R1 / R3 add Javelins at 89-231 m in the first 1-3.5 s.
   [verified frames: flare off + guns on in the same frame; ~3 m/s per s; Javelins R1 at ~216 / ~231 m (+1.4 / +3.4 s),
   R3 at ~90 m (+0.6 s). After R1's coast the engine-on drop 281 -> 88 m/s in 0.8 s (v1882.0-1882.4)]
5. **One coast kills**: eject after 2.7 / 6.0 / 10.0 s [M], because (a) the kill is shield + 65 % hull (m02.ini
   1734) [M], (b) their weapons out-damage ours: 3 Justice Mk III + 2 Javelins (R1, ~1,060 hull/s in the coast), 8
   weapons (R3) [I from bars]; R2's two Justice guns needed the full 10 s at a widening gap.
   [eject times verified frames: ~6.0 / ~9.6 (yellow bracket; explosion +1.6 s) / ~2.4 s; the damage rates stay I]
6. [WITHDRAWN 31 Oct - the owner: an engine kill ends cruise in vanilla too; see the box at the top] **R3's short engine-kill taps return to 279-295 m/s within 0.2-0.8 s with no charging text** (v1934-1948) [M], i.e.
   in vanilla 1.1 an engine kill during cruise does not cost a new charge when the engine comes back [I]. Our
   `FREELANCER_RE.md` 470-474 says "Cruise ends (cruise=False)" in HDE after an engine kill - a direct conflict,
   **unmeasured in HDE for an engine-back-on at cruise speed**.
7. Online: no M02 speedrun notes beyond the speedrun.com route summary already in `docs\research\speedrun.md` 533
   ("weaken the last Rogue (shield down) before cruising to intercept Ashcroft; engine kill (EK) to catch Ashcroft"),
   which these numbers confirm [M]. Two WebSearch queries (standard) found nothing else
   (https://strategywiki.org/wiki/Freelancer/Mission_2 describes only the normal mission).

## Differences from ours (sc1 / sc3 / sc4, 6 runs; logs under `research\experience\legs\2026-10-31_1534..1630_M02-sc*`)
- **Speed at his spawn: ours 0-96 m/s, cruise=0 in all six** (`objective chase ... speed=58 / 23 / 0 / 96 / 14 / 0
  cruise=0`) vs the runners' 247-299 [M]. We kill the 4th escort ourselves and are at rest or slow beside it; d_spawn
  212-608 m (sc1, sc3, sc4-i2) or 1,938-2,859 m (sc4-i1 / i3) [M].
- **Coast timing: ours started during his ramp** (sc3-c_i2: `coast start gap=111 v=270 his=268`, 8.3 s after the
  chase line; he reached 300, `coast end (gap) gap=540 v=244 his=300 after 10.2 s`) [M]. The runners start the coast
  only after 2-7 s at equal 299 m/s with a stable gap; a coast at 270 against a ship still ramping is outrun at once.
  [verified frames: 2.2-6.6 s stable at 298-299 before each coast]
- **Damage in the coast: ours ~87 HP/s (sc3: shield down at 9.4 s), the runners' 195-1,150 /s** [M ours, I theirs].
  With our 2 guns (combat dps 125-270 in these logs) one 10 s coast deals ~0.9-2.7k of the ~3.2k needed (821 shield +
  2,340 hull) [I] - even a perfect coast needs Javelins (2 at <= 250 m, the R1 pattern) or a second coast.
- [WITHDRAWN 31 Oct - the owner: an engine kill ends cruise in vanilla too; see the box at the top] **Re-cruise cost:** ours plans a full new charge (~8.5 s) after a coast (AUTOPILOT.md ChaseCoast); R3 shows taps that
  resume cruise speed in < 1 s in vanilla [M] - in HDE unknown (above).
- The last escort: we fight it to death (sc3: `kills=3` then the 4th; sc4 an extra 3-5 s fight) [M log]; the runners
  stop at hull 0.36-0.59 and leave [M].

## The ONE proposal: `ash_ride` - be at cruise speed when he spawns, ride his ramp, coast only at equal speed
> **ash_ride built 31 Oct 2026, not flown** (`modules\autopilot\docs\AUTOPILOT.md` "ash_ride"; `modules\autopilot\src\ash_ride.h`,
> `autopilot_ashride.cpp`, `tests\ash_ride_test.cpp`), adapted to the P0 above and the owner's correction of 31 Oct 2026
> (an engine kill out of cruise coasts straight on the velocity and loses cruise; finding 6's "taps keep cruise in vanilla"
> was an inference from OCR'd speeds, not a fact): no taps - a cruise loop near the spawn, a timed cruise intercept, the
> coast only with him dead ahead on our velocity line. The numbers below are named constants there, open to the frame
> re-check.
1. **Leave the 4th escort at shield 0 and hull <= 0.6** (read from its bars; not a kill), press cruise beside it
   (inside `nag_battle3`'s 5 km of King), and fly to a point **400-800 m behind his spawn on his run line** (spawn
   -> (-22099, 0, 75896)). There, engine-kill coast at cruise speed toward the spawn (R3's hold) and **re-cruise /
   tap** to hold 250-300 m/s until `escorts_destroyed` fires (the NPCs' kill). Fallback (fail closed): if the escort
   still lives 35 s after our press, or we drift past 5 km of King, go back and kill it (today's fight).
   [step 1 verified for the escort (shield 0, hull <= 0.6, cruise beside it); **withdrawn: the engine-kill coast /
   re-cruise / tap hold** (R3 held in cruise turns; an EK costs a charge, vanilla frames too) - the hold is cruise
   loitering (turns / circles at 250-300 m/s near the spawn); **corrected: the 35 s fallback is too short** (R3's escort
   died 38.3 s after the press): >= 45 s or tied to the escort's hull read (R3 watched it in the info panel)]
2. **At his appearance: no charge, already fast.** Close to 90-220 m behind him while he ramps (75-160 m/s for the
   first ~5 s), never past him (R1's U-turn is the fallback, not the plan).
   [stands; corrected numbers: he is >= 60-80 and <= ~105 m/s at +3..+6 s, ~300 only at ~+13 s]
3. **Coast only when he is at >= 295 m/s and the gap has been stable (+-10 m) for >= 1 s at 90-220 m** (R1 / R2 / R3
   all did this); fire every gun from the first frame and Javelins at once while the gap is <= 250 m (R1: 2 in 2 s;
   the 0.5-hull missile hold for the pod stays). End the coast at 300 m or his shield regen; then re-cruise (tap if HDE
   keeps cruise, else a charge) and repeat.
   [stands, verified frames (stable >= 2 s at 87-214 m, both 298-299; guns from the first frame; Javelins <= 233 m);
   re-cruise **= a charge** (no tap: P0 in HDE and R1's 281 -> 88 m/s frames in vanilla)]
- [P0 answered: see the box at the top; Fail branch applies]
- **P0 first (~2 min, no build, any open-space save, the HDE fact that decides step 1 and 3's re-cruise):** cruise
  to 300 m/s, engine kill for 2 s, engine back on; read FLMotion speed + the cruise / charge state at 10 Hz. Pass
  (tap exists): >= 280 m/s within 1 s without a charge. Fail: step 1's hold becomes repeated charges (time-boxed) and
  step 3 plans one coast with Javelins.
- **Probe (S419-type Follow-step start is too late for step 1: use `Save05c7.fl`, `-StopAt M02.pod`, n=2 then a set):**
  judge from the log + FLMotion: the 4th escort's last hull read <= 0.6 at our press; our speed >= 250 m/s and
  distance 0.4-1.0 km at the `objective chase` line; `coast start` with his >= 295 and our >= 295, gap 90-220 m;
  **kill (pod) <= +30 s after the Follow banner** (runners +18..+28 [corrected: +20.2..+27.4, explosion up to +29.0]; ours 105.8-129.3); ashcroft + pod <= 90 s (runners
  64-81, PB 146.8); no `nag_battle3` / 13093 FAIL; the escort-wait share (press -> `escorts_destroyed`) logged.

## Confidence and gaps
- High [M]: speeds, the cruise press / charge times, the engine-kill moments (frames), the coast speeds and gaps
  (frames at 1 Hz / 0.6 s), Javelin times, the 35 % eject (script), no "Destroy" banner.
- Medium: target bars (OCR alternates with misreads; the quoted runs are consistent and checked on frames); R3's ranges
  are list values with ~1 s lag; R1's spawn time (selection time, not spawn).
- Not measured: who exactly killed each last escort (King vs police; R2's escort not visible after v1803); the
  runners' distance to the spawn point per second before he appears (no HUD anchor there: only his first range and
  the escort / King ranges); turret / NPC fire on Ashcroft during the coast; vanilla damage values (the damage rates
  use HDE hull / our shield and are [I]). HDE engine-kill-then-engine-on at cruise speed (P0).
