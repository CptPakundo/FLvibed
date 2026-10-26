# Defensive consumables (nanobots, shield batteries)

## Mechanics
- One press of the repair / battery command uses **as many units as the gap needs** (up to all held) to fill the bar
  completely `[user]` 19 Oct 2026; confirmed in game: a ~895 shield gap took 3 batteries (7 -> 4), refilled +837
  `[game]`. Restore per unit ~300 shield (misc_equip.ini ShieldBattery hit_pts 300) / ~600 hull (RepairKit 600)
  `[data]` + measured; the last unit rounds up `[game]` (exact rounding: to measure).
- A failed shield reboots offline: its points build up again while it absorbs nothing (`pub` shield `up` flag)
  `[game]` 19 Oct 2026 - must not count as protection.
- So frugality is about **when** to press: late (near depletion) refills more per press; a press on a small gap spends a
  whole unit for part of its value.

- **The autopilot sets the count itself (user decision 21 Oct 2026):** "one battery per press, pressing again only
  while still needed", scaled by "incoming fire, prediction of hull damage". The game's own `use_item` with our count
  (docs\FREELANCER_RE.md "Using N units"): batteries = the shield the next 2.5 s would take (rate x potential,
  missiles due, one more largest hit, rounds in flight) + the offline threshold - what is left; nanobots = what keeps
  the hull above 40 % after it; at least 1, at most what the gap holds with < 30 % of a unit spilled (a failed shield /
  death soon: the gap rounded up) and what is aboard; re-checked every post_delay (2 s). Event `repair-units`. Falls
  back to the button when use_item does not verify. SaveC705 vs one gd_im_ge_fighter5_d12: 3 presses x 1 unit 2 s
  apart (+221, +274 measured), won, hull 1.00 `[game]`.
- **A use_item press can use nothing** `[game]` 21 Oct 2026 (M04 bounty hunters, 1 of 7 presses: 1 battery at shield
  0.34, the count stayed 13; the server's answer, cause not known yet - the call's return value is not read). The
  effect check then locked batteries out for 10 s at once, the shield failed inside it, hull 1.00 -> 0.66. Now: a miss
  logs `repair-miss` and is retried at the next post_delay; only two misses in a row lock out for 10 s (nanobots alike).

## The autopilot's frugal rule (user decision 19 Oct 2026)
Battery "near depletion" = the fitted shield's own `offline_threshold` (st_equip.ini; 0.15 for most shields; read by
`space::ShieldOfflineThreshold`), or the shield failed, AND the damage model predicts >= 8 % of the hull getting
through within 2.5 s. (The footage showed presses up to 25 % and after a recent failure; the user chose the game's
value.) Until 19 Oct 2026 ~10:30 `ai::Self::shieldArch` was the ship's aggregate CEShield (select_equip.ini's built-in
shield), so the lookup missed and the 0.15 fallback applied; it now reads the fitted CEShieldGenerator.
**Timing (20 Oct 2026 ~23:40, b48780e):** "about to fail" is judged over the **reaction window** (0.75 s: the damage
rate, the largest recent hit, enemy rounds and munitions due), not the 2.5 s horizon - the press comes at the last
useful moment. M04 x3: batteries 4 / 11 / 8 (the user's 2020 run: 8), hull lost 0 (`docs\kb\missions\M04.md`).
Repairs run in **every** autopilot mode since 20 Oct 2026 (travel, hold, docking too; not only fights).

## Exercised in fights (19 Oct 2026, SaveC705: 3 batteries + 3 nanobots, `ap start combat`, spawned hostiles)
- 3 Liberty police fighters: won in 41 s, shield never below 37 %: no consumable (frugal, correct).
- 4 heavy Rheinland fighters (gd_im_ge_fighter5_d12, gun potential 17485 hp/s): a lost fight both runs. Batteries at
  shield failure only (correct). Run 1: 3 nanobots at hull 0.71 for +317 each (the 2.5 s "death risk" overrode the
  no-waste rule every moment: hit-rate floor 0.05 predicted 874 hp/s), dead 30 s later with none left. Fix: the waste
  override only for death within the reaction time (0.75 s, `worst-soon` / `death-soon` in the repair event), hit-rate
  floor 0.01 (learned 0.011). Run 2: nanobots at hull 0.61-0.62 for +420 / +424 / +319 (full value); still lost.
- Deterministic incoming damage (19 Oct 2026, ~11:30): the repair model adds the gun rounds already in flight that will
  hit (space::IncomingRounds, predicted / measured 1.03), simulated in arrival order through the shield (fails at its
  offline_threshold) into the hull; repair events carry `rounds` / `rounds-hull`. Run 3 vs the 4 heavy fighters:
  nanobots at hull 0.71 with `death-soon` from 3 rounds in flight worth 563 hull (real, not a rate guess); still lost.
- Hopeless odds (user, 19 Oct 2026: missions are do-or-die; free roam retreats when winning is useless). `[game]`
  SaveC705 ship vs 4 heavy Rheinland fighters (threat 16-21x strength), survival from the fight's start: fight, then
  run at hull 0.52: 50 s (x2); run at shield failure, straight 13 s / jinking 56 s; run to a base 2.5 km away 17 s
  (the travel flies straight, no consumables); fight on, no run: 14 s; **run at first contact (before their guns
  reach; jinks while hit, cruise after 1.5 s quiet): 72 s / 48 s, the shield holds ~16 s instead of ~4 s - kept**.
  None escapes: equal-speed attackers keep the cruise engine from charging. A base right beside the ship would be
  the only real refuge (docking) - not built (needs dock refusal by reputation handled).
- **Danger by firepower (user, 19 Oct 2026: "consider the enemy's damage capabilities, in the context of what they're
  targeting"): `FightOdds`** - the race: their time to destroy us (our hull + shield + half the consumables over
  their gun potential x a 2 % prior, measured from our losses after 8 s) against ours (their hull + shield over our
  damage rate, 35 % of our potential for the first 20 s of firing, then measured). Free roam: odds < 0.25 = hopeless
  (the loadout prior decides at once; a measured verdict must hold 3 s unless < 0.05); the fighter-unit head count
  only where no loadouts are read. `[game]` SaveC705 starter ship (guns 163 hull/s): 3 level-5 pirates odds 0.54
  prior / 0.76-1.07 measured, won 3 of 3 with no retreat (equal aim assumed at first: 0.03, ran and died - NPCs land
  1-15 % of their potential); 1 level-12 heavy fighter 0.20 (4371/s potential): retreat at contact, survived 120 s
  once, caught at 99 s once; 4 heavies 0.01: retreat at contact, 18-96 s. The decision is right; escaping faster
  attackers stays open.
- **Calibrated on a matrix (user, 19 Oct 2026: "make sure the retreat does not ruin winnable fights - many
  variables")** `[game]`: 2 ships (SaveC705 starter, guns 163/s; SaveC706 bh_elite, 1633/s) x 15 kinds (pirate
  fighters d1-d6 x1-x5 at 300-1500 m, pirate elites d7/d9, pirate freighters, Liberty Navy elites, bounty hunters
  d5/d8, a Kusari gunboat, 1/2/4 level-12 heavy fighters), each flown with `caution 0` (never leaves = can it win?)
  and the normal setting. Truth: every kind winnable for both ships except 4 heavies vs the starter ship. First
  calibration (prior decides at 0.25, measured at 8 s): 3 winnable fights left and lost (prior 0.18-0.25), later 2
  more (a burst measured 0.03 at 8 s; the head-count outmatched rule at hull 0.58). Wins measured as low as 0.08.
  **Now: the loadout prior decides alone below 0.02 (0.4 x retreat_odds), the measured race (after 15 s) below
  retreat_odds 0.05 held 5 s, the outmatched rule (hull < 60 %) uses the same race line; the head count only without
  loadouts.** Final runs: every winnable fight won with no retreat (incl. the strong ship vs 4 heavies twice, the
  starter vs 2 heavies); the starter vs 4 heavies left at contact (and died, as it does fighting).
  Harness caveat found on the way: after ~2 h of spawns in one session every spawn failed (-4, the spawner's flag
  probe capped at 4095 - fixed, docs\FREELANCER_RE.md "Spawning ships").

## The user's play (2020 playthrough, all missions) `[footage]` 19 Oct 2026
Method: HUD counters read per second (tools\vidspeed.js --box, templates research\videos\catalogue\countglyphs.json),
a press = a stable counter drop; shield / hull read as lit cells of the 16-cell status bar at 2 samples/s around each
press (scratchpad barcells.ps1, validated by eye). 29 drops, 11 misreads (counter blank while the bar did not move),
**18 real presses: 16 battery, 2 nanobot** (M02, M02.5, M03, M04, M04.5, M06-07, M07.5, M10, M11).
- Batteries (16): shield before the press (of 16 cells): **0 in 8** (failed), **1-4 in 7** (6-25 %), **10 in 1**.
  The shield was falling fast in the seconds before (e.g. 16 -> 0, 12 -> 0, 9 -> 3 within 6 s); the hull was full
  (16/16) in 14 and 14-15/16 in 2: batteries kept the hull untouched. 3-8 units per press (full refills).
- Nanobots (2): hull 14/16 with the shield failed (M06-07, 2 units) and hull 9/16 with the shield failed (M04.5,
  2 units -> 16/16). Rare: the shield did the work.
- Outlier: M07.5 2685 s - a battery at 10/16 shield (falling 14 -> 10 in 6 s, hull full): a pre-emptive press under
  heavy fire.
- Spec match (battery when depleted / near depletion and damage imminent; nanobot when hull damage is about to grow
  or the hull is badly damaged): 15 of 16 battery presses and both nanobot presses fit; "near depletion" in practice
  reached up to 25 % (4/16 cells).
- **Dense re-measure (user: 1-2 fps too sparse; counters 5 fps, bars 10 fps; 19 Oct 2026):** batteries: shield just
  before the press 0/16 in 9, 1-4/16 in 6, 9/16 in 1; falling 1-5 cells/s (6-31 %/s) before it; hull 14-16/16 in all.
  The 9/16 outlier (M07.5 2684 s): the shield had failed within the 8 s before, was rebuilding and falling again - a
  press against a second failure. Nanobots: both with the shield at 0, hull 12/16 and 9/16. Matches the game's own
  `offline_threshold` (0.15) region and the spec; the battery's "near depletion" in practice = <= 25 % or "failed
  recently and falling again".

## Ordnance across the 2020 playlist `[footage, OCR]` 20 Oct 2026
Every weapon-panel row of all 19 videos read at 2 fps (`tools\vidmissiles.py` on `tools\ocr\flocr.py`; per video
`research\videos\catalogue\tracks\<id>.weapons.tsv` / `.ordnance.csv` / `.launches.tsv` / `.ammo_gains.tsv`). A launch =
a count drop of 1-3 holding 2 samples (5-sample median; about +-1 s); a gain = a rise (tractored loot, purchases).
Counts are per munition type (two launchers of one missile share the count). The weapon panel grows upward with the
list (a 9-10 slot list reaches y ~1033 at 1080p): a box ending at y 1010 hid the last rows - and with them every
torpedo - until it was enlarged.

| Missions | Launched | Gained |
|---|---|---|
| M01-M03.5 | Javelin 15 / 17 / 20 / 4 / 17 / 6 per mission; Windstalker 6 (M03.5) | Javelins +4..16 (loot) |
| M04 | Windstalker 6 | Countermeasures +100 (bought), Drone Mine +1 |
| M04.5-M10 | Wasp cruise disruptors 1-3, Drone Mines 1-9, countermeasures 1-7 per video | Drone Mines +4..42, CDs +1..20 (loot) |
| M11 | Starkiller torpedoes 7, Adv. countermeasures 12 | - |
| M12 | Starkiller torpedoes 17 | - |
| M13 | Starkiller torpedoes 42 (one every ~2 s on the big ships) | Starkillers +17 |

Reading: missiles while the loadout has a launcher (to M04); the middle of the game runs on guns with mines /
disruptors / CMs as utilities, mostly collected rather than bought; torpedoes against the Nomads' capital ships at the
end. Loot pickup of ammunition is a regular behaviour (the loot reflex the autopilot has, `consumables` section).

**Hull damage in the user's play** (directive "hull is sacred", user 20 Oct 2026; `research\tools\hull_footage.py`:
the HUD bar scan masked to seconds with the weapon panel on screen - the bar reader alone also "read" a bar scene and a
cutscene as hull damage, checked by eye; docking / jump animations still read hull 0 with full shields and are
discarded): **the hull stayed at >= 95 % for 86-100 % of the time in space in every mission; 9 of 19 videos show no
hull damage at all** (M01, M03.5, M04, M05, M05.5, M06-07, M07.5, M08.5, M09). Where it dropped, it stayed shallow:
lowest hull M01.5 0.54, M02 0.54, M04.5 0.52, M11 0.55, M12 0.60 (89 % of the time full), M13 0.40 (the end battle);
M08 sat at ~0.90 for a while (74 % of the time at >= 0.95, unrepaired). The directive tracks the 2020 play closely.
Our autopilot in the same mission (M04, 20 Oct 2026): 3-8 repairs per run before the stance changes - the gap to close.

**Restock at stops vs loot** (`research\tools\ammo_purchases.py`, 20 Oct 2026): a count that rose across a gap of >= 30
s in the weapon panel (docked, a scene) = bought / rewarded at a stop; a rise while the panel stays up = loot. 153 rises:
6 at stops, 147 in flight. At stops the user **filled to full** what they actively used - Javelins 44 -> 50 and
44 -> 49 (M01.5, M02), countermeasures 0 -> 50 (M04) and 0 -> 10 (M08), Starkiller torpedoes 33 -> 50 (M13): 5 of 6
reached the highest count seen (50 = the munitions' carry limit). Mines (92 loot rises, 1 small stop rise 35 -> 36)
and cruise disruptors (33 loot, 0 stop) came from tractored loot, not purchases. Caveats: few stop events (the panel
shows only in space); a gap can be a scene and its rise a mission reward. Restock policy taken from it (harness
`Invoke-FLBaseRestock`): fill the ammo of the mounted launchers to what fits; open question to the user - were mines /
disruptors left to loot on purpose?
