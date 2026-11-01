# M13.bsdown: escalation step 2 - the second data pass on the recordings

Escalation step 2 (the owner, 31 Oct 2026: step 1, `M13_bsdown_consensus.md`, brought no significant gain). Research
only, no game, no build; checked against HEAD f65cc2d4 (dirty: docs\LEGS.md, LEG_DOSSIER.md, SPLITS.md, legs.tsv -
not read here). [M] measured, [I] inferred. Evidence, scripts and outputs: `research\evidence\speedrun\bsdown_pass2\`
(below "pass2\"). Windows: R1 v13890-13930, R2 v14066-14116, R3 v14146-14188 (banners 13905.0 / 14082.0 / 14158.0,
events_dense_run*.tsv; run folders' events.tsv agree: R1 fight 13895, R2 "battleship at 0 m with allies" 14080).

## New datapoints and how they were taken
- **The model** [M]: `d_n_battleship` (Solar\solararch.ini 4280, hit_pts 120,000), mesh `no_battleship.3db` LOD0
  (2,081 verts, 2,868 tris) read by `pass2\bsgeom.py`: 1,254 m long (z -642 bow tip .. +613 stern, -Z = bow:
  HpHeadlight z -471, HpEngine01 +606), 277 x 244 m at its widest (the bulb, z -350..-50); a 300 m cone ahead of the
  bulb, a thin 650 m tail behind it (`pass2\geom_profile.txt`). Mesh bounding radius 628; our autopilot reads 666.
- **15 turrets** [M]: `nomad_turret01_mark01` (697 per shot, refire 1 s, reach 1,200 m: weapon_equip.ini 19582-19615),
  rings at z -528 (1, bow), -353 (3), -125 (3), +45 (3), +378 (3), +564 (2). Joint limits: yaw +-170 deg on the ship
  hardpoint, pitch 0-90 deg (no_gunship_turret.cmp Cons\Rev): each covers the half-space above its mount. No shield.
- **Cover map** (`pass2\geom_map.txt`, `geom_clear.txt`): my joint-limit count at our kb3 site -233,0,0 = 4, the same
  as the game's `cover=4` (instance 1 log 2026-10-31_053211: validates the model). By joint limits (= the game's
  GunsCovering, no hull occlusion) **no point 100-400 m off the hull has fewer than 4 guns** [M model]; with the hull's
  occlusion counted, 1-3 only at 60-110 m gaps (under the body, beside the bow cone) [I: whether the AI fires through
  its own hull is not verified].
- **HUD "0 m"** [M]: the HUD distance is to the bounding sphere: R2 reads 98 -> 7 -> 0 at 14076.0-14076.5 and keeps
  0 m while flying 180-198 m/s along the ship, out past its end and back (sheet1hz_r2_v14077.jpg); "0 m" = within
  ~630-666 m of the centre, i.e. anywhere up to ~500 m off the hull.
- **Hull bar at 5 Hz** (`pass2\hullbar2.py` -> `bar2_r*.tsv`; the filled red-magenta part / the bar's 106-108 px;
  `hullbar.py` / `bar_r*.tsv` = a failed first reader, kept). Reads 64-112 of 200-250 frames; values < 0.15 outside
  the end are noise.
- **Hull apparent size** (`pass2\hullsize.py` -> `hull_r*.tsv`): saturates at close range (the blob touches the
  crop), usable only as cover fraction; the distances below are hand measurements on full-res frames.

## Per runner
| | R1 | R2 | R3 |
|---|---|---|---|
| Hull at banner [M] | ~0.73 (0.85 @13900, 0.65 @13908.2) | ~0.79 (0.87 @14078.8, 0.75 @14083.8) | 1.00 (@14158-14160) |
| Arrival -> kill [M/I kill] | 13898 -> ~13925 (+20) | 14077.4 -> ~14113 (+31) | 14160.2 -> ~14182.5 (+24.5) |
| hp/s by phase [M] | 3.6k, 2.9k, 3.5k, then ~4.9k [I] | 8.7k arrival volley, 4.7k, 6.3k, tail 1.4k [I] | 7.6k arrival volley, 4.5k, 7.2k |
| Mean speed at the ship, share < 20 / 20-100 / >= 100 m/s [M trace] | 128; 12 / 41 / 48 % | 127; 19 / 13 / 68 % | 89; 19 / 37 / 44 % |
| Still spells (< 20 m/s) [M] | 13910.5-13912.2: shield 0, hull 0.99 -> 0.83, batteries 10 @13912.6 | 4 of 0.6-2.1 s, each a battery dose or a reverse | 4 of 0.6-1.3 s; batteries 9 @14181.4 (shield 0.15 -> 0.99) |
| Launch cadence [M events] | torpedo + Slingshot + Catapult every 2-5 s | torpedo every ~2 s, missile pair every 2-3 s | Catapult pair + missile pair every 1.5-3.5 s |
| Hull gap / face [I, frames] | ~200-250 m, off the bulb's forward quarter (r1_v13910.png: bulb 790 px = 42 deg -> ~360 m from the axis) | ~200 m broadside of the bow cone / bulb (r2_v14088.jpg: 750 px tall = 40 deg); lengthwise passes, turns beyond the ends (14080-14083, 14102-14109) | close along the aft body and the bow face alternately (sheet1hz_r3_v14166.jpg); not measured |

Turret fire per position (task item 5): **not measured** - muzzle flashes and tracers are not separable from our own
ordnance at 1080p (motion README drops firing too); the red hit arcs show in r1 13909-13912, r2 14096-14098,
r3 14166 / 14173.

## Answer
- **There is no safe still spot 100-400 m off this hull** [M model]: every point there has >= 4 guns by the game's own
  joint test. The runners are not safer by position - their "0 m" is ~150-350 m off the hull, the band our kb3 site
  (gap 183, cover 4) sat in [I]. They are safer by **motion and time**: >= 100 m/s for 44-68 % of the fight, the
  speed changing every 1-2 s (afterburner pulses 70 <-> 200), still only <= 2 s for a battery dose; R1's one longer
  still spell took a hull volley like our deaths [M]. A turret round flies 0.2 s to 200 m: a 100 m/s velocity change
  moves the ship ~20 m off the lead [I].
- **Fast** = every launcher at refire from arrival (zero held launches: at 150-250 m the line gap is always >= 100 m),
  arrival right after the banner (R3: +2.2 s, kill +24.5 with 0 pre-banner damage).

**Proposal (one):** replace the hold with **lengthwise strafing passes**: rush in to a 150-250 m hull gap beside the
bulb / cone, then fly along the axis at 130-200 m/s (afterburner pulses, speed swing >= 80 m/s every 1-2 s, never
< 20 m/s except a <= 2 s battery stop at a shield failure), nose and gimbals on the hull, torpedo + missiles at refire;
at each end turn 1-2 km out and come back (R2's cycle ~12 s). Judge: kill <= +30 after the banner (our 4.5k/s ceiling:
120k in ~27 s from arrival), 0 held launches, no death.

**Confidence:** high that "0 m" is the bounding sphere and that no stand-off point is uncovered; high on speeds,
cadence and the damage curve's shape (R3 alone: 5.4k/s average); medium-low on the runners' exact gap (+-40 %, one or
two hand-measured frames each) and face; R2 / R1 kill times inferred from the bar's last read and the target switch.
