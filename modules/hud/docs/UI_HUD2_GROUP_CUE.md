# The weapon group cue (design, 5 Nov 2026; P11f's last feature, on the owner's word)

**Status:** design, ruled by the owner on 6 Nov 2026 (section 0). Not built. Game facts: `research\evidence\hud2_skin\09_weapon_cue_facts.md`; the source games: `08_mechwarrior_cue_reader.md`. Frames: `research\evidence\hud2_skin\mock\group_cue_4k.png`, `group_cue_1280x800.png`, `group_cue_states.png` (`tools\uimock\hud2s.py`). Build lane: after P11e, the 12 probes and the most-seen screens.

**The request, a player's, through the owner:** "Would it be possible to show weapon group icons above the crosshair, showing currently-selected group, and lighting said individual icons up once your target is in the center of the screen and reaches the furthest-maximum range of any of the three weapon groups, like MechWarrior 4 and 5 do?" The owner: "a good balance of meeting his request exactly while also fitting in with how HUD 2 should work, with its constraints and standards."

**The player, on the slim frames (5 Nov 2026):** "Needs to be a smidge smaller, plus M for missile(s) and T for torpedo(es); otherwise look good so far!"

## 0. The owner's rulings (6 Nov 2026): these win over sections 3, 4 and 7 below
**Seat:** above the ring, each mark on a small dark cell, so it stays readable over the bracket's name (D5; a real bracket puts the name where the row goes: `decisions\D5_cue_seat.jpg`).

**One meaning per channel** (D4; sources `research\evidence\hud2_gate\d4_channels_research.md`; frames `decisions\D4_final.jpg`, motion `D4_final_motion.gif`):
| State | Mark | Channel |
|---|---|---|
| A group | digit in chrome at 70 % (#5B8CAA) on the dark cell | |
| Selected | yellow digit, yellow hairline | colour, and a hairline, so not colour alone (3.1:1 against the dim chrome) |
| In reach | solid fill, digit dark; all or nothing | fill |
| Locked (M, T) | **a steady glow and an inverted tick above the mark, point down, the two breathing together** | glow and form |
| Reloading (M, T) | a thin bar under the mark, 0.0026 H, for the refire's 2 s only | form |
| No rounds, cannot fire | amber, a slash, never filled | colour and form |

The glow means lock and nothing else: the "one pulse at any change" of the sheet the owner read is dropped, one meaning per cue.

**Confirmed by the owner, 6 Nov 2026, "good for now":** the 1 Hz breath with the tick in phase (so the table's "steady glow" reads as that breath), the tick as the inverted triangle, the glow meaning lock only.

**Against the rules, checked:**
- **Flash:** 1 Hz, soft (a sine, no hard edge), under the 3 Hz ceiling, over a mark far under WCAG's flash area. It holds while locked, so it is a standing motion beside the crosshair: the off switch is below.
- **Reduced motion:** `skin_motion` FADES or NONE: the tick stops at rest and the glow holds steady. The state stays readable with no motion at all. `group_cue` 0 turns the row off.
- **The Deck size:** a cell of 16 x 21 px. The tick is 5 px tall and travels 2.2 px: about the least a move can be without shimmering, so it needs pixel-snapped steps or a smooth subpixel draw: judge in game. The bar is 2 px.
- **What research advised against, and the owner chose knowingly:** steady glow with fill, because at this size both read as luminance; a light breath at the crosshair where the research said "motion: none". It said no motion on digits: the digit stands still; the tick moves. The refire bar on M and T was the research's own exception; guns keep none (2-8 Hz would flicker).
- **Unknown:** the game's lock has not been seen once in 48 samples, so glow and tick are unproven in play: probe "a real lock" before building them.

**The triangle:** the ruling reads "invert the triangle (pointy side down)". The only triangle on that sheet is the lock's tick, so that is what turned. The loadout's yellow selection arrow is not on the cue and is untouched.

**Sizes:** the text follows `hud_size`: 0.024 H on Normal and Large, 0.021 H on Small, Normal's text at 1280x800 (`UI_HUD2_INPUTS.md` 7).

## 1. Verdict
Yes, as asked, and small. A row in the ring's open top, at the text floor: up to three group digits, then M and T. The selected group is boxed; each mark fills when the target is in the ring and inside that weapon's reach. The ring's top is free in every preset, it is 0.09 H from the eyes, the game gives the range and the fire state natively, and the cost is a few microseconds.

**Where it differs from the player's memory of the source, not from the request:** MechWarrior 4's boxes stand beside the crosshair, and MechWarrior 5 does not do this without a mod (section 2). The request's "above" is met as written.

## 2. What the source games do
| | Where | Rule | Source |
|---|---|---|---|
| MW4 Vengeance | **A 2 x 3 block of small boxes right of the crosshair**; the range readout above it | Boxes lit by group | Frames: [Osiris](https://img.gameswelt.de/public/images/200102/74ad56c2804f66346445e1ea58667842.jpg), [night](https://img.gameswelt.de/public/images/200102/60638eaf14a3a52259ccad55037f45a2.jpg), read here at 640 x 480 |
| MW4 Mercenaries | A separate "Weapon in range indicator" in the manual's HUD diagram | Not stated | manual callout, excerpt only |
| BattleTech: Firestorm (MW4's engine) | Three boxes beside the crosshairs, one per fire group | **Green if the target is in range of one or more weapons of the group; yellow if out of range, not recharged or out of ammo.** The clearest written rule | pod training manual (`08_...` S3) |
| MW5 | Reticle red on a hit; weapon groups in the lower-right readout; optional cooldown pips | Range colours only through a mod, on the group's *shortest* weapon | MW5 manual; mod author (S9-S12) |
| MWO | Squares show cooldown, the list shows range | Split in two | `08_...` |

Three players recall MW4 lighting by group in range, one naming the longest weapon (forum excerpts, one voice each). So: per group, each cell on its own, the group's longest reach: the player's "furthest-maximum" reads as that, and Firestorm agrees. Hard evidence for home MW4's exact rule is thin: the frames show the place, not the rule.

## 3. What it shows
**Size follows importance** (the owner, 5 Nov 2026: "only as big as they need to be, and only as big as their importance"). The first mock's solid 0.034 H boxes were too much for a secondary cue beside the ring. The ring's shield and hull come first, so the cue stands below them: digits at the text floor, bare by default. A box is drawn only for the state that matters.

**Digits.** The game's own group number, as on the loadout's rows. Up to three, the groups the loadout panel shows; the active group is always among them.

**M and T.** After the digits, a small gap between. M while the ship carries a missile launcher, T while it carries torpedoes; a ship without one has no letter. Letters are never selected: launchers have their own buttons, so no yellow. A cruise disruptor on the torpedo hardpoint gets no letter: T says torpedo, and it does no damage to aim for.

| State | Look | Why |
|---|---|---|
| A group | the digit alone, `CHROME` | no box: most of the time it is just a number |
| Selected | yellow hairline, yellow digit | yellow = active, as the loadout's arrow and number |
| In reach | **filled**, digit navy | the moment that matters; fill against bare reads without colour |
| Selected, in reach | filled yellow | |
| Cannot fire, no rounds | the mark alone, amber, never fills | amber as the loadout's empty count |

**Selected:** `weapon_groups` 0 and 1: the game's current group (`hudinfo::ActiveWeaponGroup`). 2: groups 1 and 2, both firing on their triggers. No group saved: the live guns as the one active group; no guns: nothing.

**No flash.** A steady state, eased 80 ms. Lit and unlit hold at least 170 ms and the test has hysteresis, so it cannot cross 3 Hz. Firestorm counts recharging as unlit; here refire is ignored: guns recharge in 0.12-0.5 s and a cell would blink with every shot.

## 4. The test, each frame while shown
For each group, lit when all hold:
1. **A target:** any selected target, friend or foe: the cue says the guns reach it, not that it should be shot. `aim::Current()`, already read.
2. **In the centre:** the lead point within the ring of the aim point, or any part of the target within it (a battleship fills the ring long before its centre does). `aim.screenX / Y`, `aim.radius` projected. The ring is the test: what the player sees is what counts, and RS's pick radius already equals it. Radius by preset: 0.10 H, Focus 0.12 H.
   - **The aim point:** gimbal: the screen centre. With `aim_cone` 90 the reticle would lock anywhere on screen, which is why the test uses the ring and not the reticle: the request says the centre. Cursor flight (K&M): the cursor. Turret view, ASSISTED: the turret's aim (probe W3).
3. **In reach:** distance to the target's surface (`aim.dist` minus `aim.radius`, as the autopilot's) at or under the reach = the longest `CEGun::GetMunitionRange` of the mark's weapons. A group: its guns. M: the missile launchers with rounds. T: the torpedo launchers with rounds. For a LOCK seeker the call gives the seeker range: 1000 m for missile01, 1500 m for torpedo01 (data, HDE). Sorted as the loadout does: homing, on `HpTorpedo` or not, not a cruise disruptor (`hud2_data.cpp:876`). Reach is archetype data, cached on a loadout or group change.
4. **Can fire:** at least one weapon of the mark: `CEGun::CanFire` at the lead point, ignoring 2 (refire). 5 (angle) keeps it unlit. 0, 1, 3, 4 make the mark amber when every weapon of it is so; for M and T, rounds are counted by ammo (the active flag reads 0 for launchers). 6, 7 (cruise, lane) hide the row.

M and T do not wait for a lock: the loadout's LOCKED chip keeps that one meaning, and the seeker's range is where a lock can hold. Whether the two agree in a fight is probe W6.

A group with mixed ranges lights at its longest gun, as asked: some guns may not yet reach. The loadout's rows stay where each gun's name is.

## 5. Where
**Standard, Speedrun, Guided:** ring r 0.10, centred on x, cell centres at `cy - 0.093 k` H, cell 0.020 wide x 0.026 tall, gap 0.003, the letters 0.008 from the digits, text 0.024. Five marks: half-width 0.059; the ring's low numbers start at 0.072 k. **The text stays at 0.024 H, the lint's floor (`text.floor` FAILs under it); this round took the box down round it.** The owner, 5 Nov 2026: the floor stays, no exception; 0.022 H would have saved about 1 px at 1280x800 (digit height 14.7 to 13.5 px, measured). Inside the ring's top gap, between the shield's and hull's arcs (they end at 42 degrees off the side).
**Focus:** on its arcs' circle, `cy - 0.112 k`, in the open top between the split arcs.

**What it covers:** at most five cells, 0.15 % of a 16:9 screen, and only while all five are filled; the three groups alone 0.09 % (0.13 % before this round); bare digits cover next to nothing. It adds no plate, so nothing to the boxed budget (`UI_HUD2_PRESETS.md`, 13.5-15.9 % in a fight).

**Keep-outs:** r 0.045 stays native (the row's lower edge is at r 0.079 k). Clear of the dialogue panel (S1 starts at 0.20 H; the ring's top is 0.40), the side alert cards (they begin at 0.205 k), the top-centre alert, the lower centre. The game's lead cross may pass under the row: nothing moves.

**Where the ring's low numbers would come within 0.01 H:** at 4:3, k 0.8, they start at 0.058 and a full row ends at 0.059, so it applies there with both letters: the row rises above the ring's circle, `cy - R - 0.020`. One rule in `hud2_layout.h`, unit-tested over the display matrix.

## 6. When
Shown with a target, or in a fight (`g_combat`). Never in calm travel: the ring stays bare, and "information the player does not need" stays off. Hidden in cruise, lanes, docking, cutscenes, with a modal or NeuroNet window, when the ring is hidden.

## 7. Per preset and mode
`hud_preset` offers Standard and Focus today; the Speedrun and Guided rows are for when those layouts are built.

| | Rule |
|---|---|
| Standard | as above; the ring's calm dimming applies |
| Focus | shown only with a hostile target or in a fight, its bare centre kept; 45 % until a cell lights |
| Speedrun | as Standard. Its spec's "No 12 o'clock pip" concerns a marker, not this; the row shows only in a fight. Telemetry's flanks untouched |
| Guided | as Standard, and the word IN RANGE under the row when the selected group is lit, at the floor (Guided's rule: words). Guided's arc labels spell M and T nowhere: the loadout names them |
| FREELANCER, ACE, ASSISTED | no difference in the row; the digit follows `weapon_groups` |
| K&M | the same row at the ring. The aim is the cursor, so the eye may be elsewhere: the row stays with the ring and not the cursor: a moving row would cross every keep-out |
| Turret view | the row stays while the ring is up; if the ring goes in that view, the row goes (probe W3) |
| Classic HUD | not there: HUD 2's feature |
| Multiplayer | `GetMunitionRange`, `CanFire` are client-side Common.dll; a missing read hides the row |

## 8. Lint
New component `ring.groups`, in the centre stack (`centre.overlap`: 0.01 H from every other centre item, the ring's numbers included). Digits pass `text.floor`. Cells are recorded as shapes, not plates: a plate inside S1 fails `zone`. `hud2 sim groups` states: none lit, one, all, an empty group, a two-trigger mode, 4:3. `hud2 lint` 0 FAIL over pad and keyboard, four presets, at 4K, 1280x800, 4:3, 21:9, with `hud_numbers` always and low. `UI_HUD2_PRESETS.md` 1.3's centre stack gains the row.

## 9. Setting, cost, fail closed
- **`group_cue`** (`[Controller]`, HUD section), 0 off, 1 on. **Default 1:** MW4 has it fixed on, MW5's nearest (pips) is an option, and a fight is where HUD 2 already shows more. With 0 no read runs.
- **Cost:** reused each frame: `aim::Current()`, the contacts. New: reach cached on change; `CanFire` per gun at 10 Hz, ten guns at most; five marks and at most five shapes. Budget 5 us a frame in a fight, 0 in calm. Read at a quiet window, like every number.
- **Fail closed:** any read failing hides the row. An unread reach (0) shows the cell, never lit.

## 10. Probes (with the design's 12; W for weapon)
| # | Probe | Decides |
|---|---|---|
| W1 | `hudinfo wg` on a fresh ship, one saved group, three | section 3's group list |
| W2 | `GetMunitionRange` against lifetime x velocity in HDE; `gunpoint` for FireResult 3, 4, 6, 7 and a destroyed gun | section 4 |
| W3 | Turret view and ASSISTED: the ring, the aim point | sections 4, 7 |
| W4 | `is_control_excluded` (mission weapon locks): does `CanFire` show it | cell state |
| W5 | Turrets' arcs: `CanPointAt` on `auto_turret` guns | section 4 |
| W6 | M and T against the game's lock (`hudinfo::MissileLock`) in a fight; `CanFire` on a launcher | whether a lit M can be unlocked |

**Not read:** any game "out of range" text or auto-aim cone, not found so far; the game shows no in-range flag on its HUD beyond a lead point computed (`aim.inRange`). The lit moment is untested in a fight: the hysteresis numbers and the ring as "centre" are for the frames to judge. The mock's lit states are illustrative.
