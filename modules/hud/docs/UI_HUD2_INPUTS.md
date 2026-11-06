# HUD 2 overhaul: every input in one place (P11f gate, step 1, 5 Nov 2026)

**What this is:** everything the overhaul of `UI_HUD2_SKIN.md` must answer, gathered before any code, so nothing is lost before the build. It records; it decides nothing. Decisions wanted from the owner are in section 9. Probes and frames: `UI_HUD2_SKIN.md` 12, `research\evidence\hud2_gate\` (git-ignored; `probes\` scripts and results, `frames\` real captures, `mock\` composites, `sheet_NN.jpg` the contact sheets).

**Standing of a claim:** *game* = seen in a running game on 5 Nov 2026 (HDE 0.7.1, build 559e28dc plus probe verbs); *code* = decompiled or read, not run; *doc* = an earlier document, not re-read.

## 1. GitHub issues touching the UI (read 5 Nov 2026, with comments)
| # | Title | State | What remains for the overhaul |
|---|---|---|---|
| 30 | Can't switch inventory categories with X in flight | open, in P11e2's track | the strip's inventory keeps the window's own pad map: after P11e2 lands, the mock's prompts follow it |
| 29 | Left stick double click to resume boost | open, in P11e2's track | not UI |
| 28 | Energy meter drain is horizontally flipped | open, no comment. **The owner, 5 Nov 2026: draining inwards from both sides "sounds nice": centred, symmetrical, from both ends to the middle** | Section 7a |
| 27 | Radiation Damage symbol clips through the entire UI | open, no reply | **Confirmed in game:** drawn over the target card's corner, the nav map, the pause and the death dialog. Section 2 |
| 26 | Weapon group / in-range indicators above the crosshair | open, no comment on GitHub | `UI_HUD2_GROUP_CUE.md`. **The "player's later comments" are not on the issue:** "a smidge smaller, plus M ... T" reached the design through the owner. The reply must not cite a comment that is not there |
| 25 | Autopilot stops under a NeuroNet window | open, fixed in P11e, unreleased | none; the reply waits for the release |
| 24 | Digits near Quantity flicker when buying or selling | open, not reproduced | The `QUANTITY n / m` line now stands still. **The quantity dialog has no row in `UI_HUD2_SKIN.md` 5:** add it. Only the reporter can close this |
| 23 | Squadron display's pips wider than HUD 2's | open, fixed in P11e, unreleased | the size audit's slim plate keeps the 20-pip bar |
| 22 | Fonts change size and boldness in the docked menus | open, fixed in P11c, unreleased | `windowtext`'s rules bind every new row (`UI_HUD2_SKIN.md` 14) |
| 10 | Equipment dealer | open. "Working fine now" (the Deck, 5 Nov 03:28); then the D-pad reaches the mount icon | Fixed in P11c (`tests\dealer_focus.ps1`), unreleased. The left-stick click as the mount's only route needs a prompt on the screen: check at the dealer's pad frames |
| 12 | Chat reverts to the vanilla look in cutscenes | closed | **A mission's story cutscene with chat stays unrun** (docking and launch only). Belongs to the gate's "cutscene start and end" moment |
| 3 | NPC aggression | open | not UI |

Closed and built on (14-21, 1, 2, 4-9, 11, 13): no action, but issue 2 (stats and inventory out of the pad's reach) is what the NeuroNet strip finally answers in full, and issue 21's danger stack is the home section 2 proposes.

## 2. Native alerts (a gap: `UI_HUD2_SKIN.md` names none)
The indicator object's constructor 0x4E8FE0 makes five children of the top-level `Targetable_Objects`: `ReticleRangeText`, `MissileAlert`, `TrailAlert`, `RadiationAlert`, `LagAlert`. Update 0x4EC450, reached only through the vtable, hides them all when GameOption 6 or 10 is 0, without the player's ship, or in a cutscene (0x4EC400). Nothing is data-driven (`hud.ini` has an empty section). Art `HUD_*alert*.3db`, `MULTI_lag.3db`. *code*

| Alert | Warns of | Shown when | Motion | hudvis today | HUD 2's home today | If hidden, the player loses |
|---|---|---|---|---|---|---|
| `MissileAlert` | a missile on the player | target-list entry flagged 0x1000000 aiming at the player (0x4F18D0); sound 0x19, Neural Net line | blinks, period = distance in km, 0.1 s at least | hidden, a "self" root | MISSILE INCOMING card + ring chevrons (`hud2_data.cpp:1404`) | only the distance-coded blink rate |
| `TrailAlert` | a ship on the player's tail | `get_closest_trailing_ship` under 500 m (0x4F1800) | 1 Hz | **never hidden** | **none** | the tail warning: nothing else says it |
| `RadiationAlert` | a damaging zone | zone word bit 0x40000 and ship +0x218 = 0 (0x4F4F50); queues `radiation_damage_detected` | toggles every 0.7 s | **never hidden** | **none** (the Neural Net line stays) | the standing warning; the line and the hull loss remain |
| `LagAlert` | multiplayer ping | 600 ms and up, flashing over 1200 ms (0x4F1B40, 1 s tick) | flash | **never hidden** | **none** | the ping warning |

**Seen in game** (`SaveG004`, in `Zone_li01_badlands_radiation1`, 4K and 1280x800): the icon stands at about 0.77 W, 0.11 H, on the target card's lower left corner. It draws over the nav map, the pause menu and the death dialog, and keeps toggling under the pause (`visible` flips): its update runs there. In the death frame a second, red icon stands above it, not identified. Frames: `radiation__kbm`, `radiation_pause__pad`, `radiation_navmap__pad`.

**The doc's positions disagree with the constructor** (x 0.224 against 0.126, 16:9 apart), so measure from frames, as above.

**Bit 0x40000 meaning "a damaging zone" is an inference:** `SaveG004`'s zone has `damage = 50` and the alert came up, which fits; other damage kinds not tried.

**Needed:** all three go under the deny rule (`hudvis` "self" roots, as `MissileAlert`) and each gets a home. The radiation and tail cards belong in 5.40's danger stack beside the ring; the owner's call: section 9 item 3. Mock: `alerts_3840x2160`, `alerts_1280x800`.

**Not answered:** what the other per-frame calls of 0x4EC450 draw (0x4ED180, 0x4EBF70, 0x4EE260, 0x4F1DD0, 0x4F2070, 0x4F2220); the full child list of `Targetable_Objects` beyond those five (the seam audit will name them).

## 3. Every native flight element and what HUD 2 does with it
| Native | hudvis | HUD 2's home | Gap |
|---|---|---|---|
| `HUD_Maneuvers`, `HUD_Target`, `HUD_ContactList`, `HUD_Status`, `HUD_Core`, `HUD_CruiseProgress`, `HUD_DamageIndicator`, `MsgWin` | hidden | chip, ring, cards, picker, loadout, comm feed (`UI_HUD2.md` 5.4) | damage arcs: hit side only in part |
| `NN_MainButton` | hidden, kept while a window is open | NEURONET prompt; the strip after the overhaul | the new-information flash: lost in flight today (section 5) |
| Cruise countdown "Cruise Disrupted - n" | kept while disrupted | CRUISE DISRUPTED card without the seconds | the skin's "deny" must keep this keep rule or draw the seconds |
| `Rollover`, `ToolTip` | kept on keyboard form | none in flight | row 55, 56 of the skin |
| `CommText`, comm portrait | kept, hidden under a modal | as built | none |
| `NN_ArrivalText` | hidden when the place line shows | place line | none |
| `Targetable_Objects` but `MissileAlert` (brackets, labels, bars, off-screen arrows, lead, reticle parts, `ReticleRangeText`) | **never touched, on purpose** | the game's own, in the world; in 0.4.0 the corners, label and bars go to HUD 2's targeting display (`UI_HUD2_SKIN.md` 6.10), the root stays kept | the deny rule must name the rest as kept, or it blanks them |
| `HUD_Waypoint`, `NN_Scanner`, `NN_GroupControl`, `BriefingParentWindow`, `LoadingProgressBar`, the objective banner (`MissionObjectiveWindow`) | never touched | native | each needs a row in the skin; the banner stands big and native in the mission frames (`mission__pad`) |

**The design's "deny by default" needs a keep list before the code:** the brackets, the banner, the waypoint marks and the comm surface are native and wanted.

## 4. Logic that lives in a panel's update (the bug class P11e found)
**The mechanism (*code*; the draw gate *game*):** the UI update 0x59E080 calls a top-level control's visible update (vt 0x2C) when state bit 0 is set, its hidden update (vt 0x30) when clear, and never reads bit 1. The UI pass draws only with bit 1 set. `hudvis` clears both (`st & ~3`), so a hidden root's visible update and draw both stop. `uiskin` clears bit 1 only, so **a window the skin hides loses no update**, and that must stay the rule: never bit 0 on a window.

| Row | Panel, address | Logic | Today | Result |
|---|---|---|---|---|
| 1 | Maneuver bar 0x4D85C0 | throttle keys applied | stepped by `hudvis` | fixed, P11e, *game* |
| 2 | NN box 0x45C8C0 | Neural Net queue | stepped by `hudvis` | *game* |
| 3 | **Contact list draw 0x4D24B0 calls 0x4D0490** | the list is built in the DRAW, every 1.5 s or when dirty; `USER_NEXT_TARGET` / `PREV_TARGET` (0x4D3E30) walk it | stepped by `hudvis`, P11e2: `tests\target_keys.ps1` cycles as classic | **CONFIRMED, game. Before P11e2, under HUD 2 the native keys did nothing: `down->0`, "no target", against a clean cycle of four ships in classic. `USER_NEXT_ENEMY` picks one ship and stays on it (27, 27, 27), where classic moves on. A released bug, in every 0.3.0 with `hud_style` 2 (`panelprobes.txt`)** |
| 4 | NN box consumer 0x4BC990 | objective banner queue (0x67232C) | not stepped | **Not a bug at a load: the banner shows and ends at 4 s under HUD 2 and classic alike** (`bannerprobe.txt`). A second banner while one stands: not tried |
| 5 | Target panel 0x4E1C20 | drop past 10 km | not stepped | **No difference: a ring at 15 km, targeted by the harness, stays in both, 600 frames.** Flying out of range from inside it: a base left 21 km behind stays in both too (P11e2, `tests\target_keys.ps1 -Range`). Filed item closed |
| 6 | Target panel helper 0x4E3BD0 | `CShip::VerifyTarget`, a dead target | pulse by accident | **No difference: after a kill "no target" in both** |
| 7 | Status panel 0x4DB340, flag +0x4C8 | weapon group apply after a launch | not stepped | **No difference: guns active alike after a launch** (one save, no stored groups changed) |
| 8 | MsgWin 0x46C1D0 | drains the affiliation map, two chat lines each | only while shown | not run; a burst when chat opens, cosmetic |
| 9 | Target panel 0x4E1C20 / 0x4DFC40 | sounds 0x23, 0x18; key event 0x26 | no | not run; low |
| 10 | Scanner tick 0x4E22B0 | closes the cargo scan out of range | no | not run; low to medium |
| 11 | Status tick | launcher vt+0xA0, purpose unknown | no | not run; low |

Display-only, or run anyway: gauges, cruise line, damage arcs, comm window (its hidden update calls the visible one), the contact list's hidden update. Under a modal the HUD roots do not update at all, classic included (0x59E080's short-circuit): vanilla.

**Hide by bit 1 alone? Tried, `gate bit1 1`:** the frame stays clean, no native art, hit tests unchanged, the map and pause fine. It does **not** bring the target keys back (the list is built in the draw), though `USER_NEXT_ENEMY` moved once (29, 30, 30). So it fixes the update-side class in one line and retires `StepThrottleKeys` and `StepNeuralNet`, which would otherwise double-step. Worth weighing in the gate's design; the draw-side list still needs its own step: call 0x4D0490, or route the keys to FLvibed's `target_cycle.h` (done in P11e2: section 4 row 3).

**Not traced:** how a key reaches a hidden control, the owner of flag +0x4C8, target fields +0x140 / +0xA8, key ids 0x26 / 0x66, child classes' updates, front-end pages. Evidence: `inputs_panel_logic.md` (29 rows).

## 5. Open HUD items (handoff, audit, queue)
| Item | Source | Goes where |
|---|---|---|
| A blank UI seen once | `UI_HUD2.md` 5.38 | not moot, not seen again in this gate's grabs; the seam audit and the moment grabs are the tools |
| The tester's "HUD shows at every screen load" | P11f row | **Caught:** one frame, the first of space, on a load (`UI_HUD2_SKIN.md` 12a row 10); plus the native box under every window in space and the death dialog |
| Game target label over HUD 2's card | handoff; `research\stash_patches\stash0_target_label_hook_WIP.patch` (a hook muting label bars and texts while over a panel; written against the old tree) | not moot; re-cut for the 0.36 card, or the card moves: section 9 item 8 |
| Arrows against chat keep-out | handoff, unverified | gate's frames: not read yet in a fight with chat at both sizes |
| Spec flight recapture in the new layout | handoff | after the build (living spec) |
| Lint FAIL `loadout '3'` / JAVELIN, 2.6-7.1 px | `UI_HUD2.md` 5.35 | **18 of this capture's reads at 1280x800, 2 at 4K**, K&M and pad. Accepted once; the size pass must settle it |
| Lint FAIL Lavablade / LOCKED | handoff | 7 reads at 1280x800, 1 at 4K: same pass. Also `RT` / `RB` (3 reads), `?` / `3` (2) |
| Lint `text.floor`, 71 reads | this capture | expected at k under 1 with the floor unmoved in the lint: `kTextFloor` must follow the size preset |
| Lint `zone`, 2 reads | this capture | read before the size pass |
| Audit filed rows 8-19 | `..\AUDIT_2026-11-05_modules.md` | none touches HUD 2's look; row 13 (a fault between Capture and Apply) stays accepted |
| Q7 open steps | the project's work queue (private) | bundles, Custom on edit, the pairing row; shared floor (sub-target row, card handover, threat sort); prompts and reveal; **Speedrun and Guided layouts: `hud_preset` offers Standard and Focus only.** The overhaul must not assume they exist; `UI_HUD2_GROUP_CUE.md` 7 says so |
| J9, J11 | queue | HUD 2 spacing report (feeds U40); frames per preset beside reference games: this gate's frames are Standard only |
| PRESETS 6 "Unverified" | `UI_HUD2_PRESETS.md` | banner under HUD 2: **answered, it shows and ends.** Others stand: native bracket label height, `USER_MINIMIZE_HUD`, `USER_NEXT_SUBTARGET`, lane-disrupted code |
| Second install (FLSharp) | handoff | M7 skipped twice; the map's writes need it (JFLP and FLSharp touch waypoints) |
| New-information flash on the log's button | skin probe 16 | not located |
| Issue 12's mission cutscene; a Neural Net line in a cutscene | handoff | the gate's moments |

## 6. What the probes corrected in the record
- `FREELANCER_RE`: "the inventory opens nothing docked" is wrong: it opens, `NN_Inventory` visible, skin family `inventory` (*game*, probe 15).
- A NeuroNet window is **rebuilt at each open**: a new map object each time (*game*). Every write goes to the new instance; `+0x4A8` is rewritten on a return from the universe view.
- The Political map's white blobs and the Patrol map's thick white lines are the game's own, in classic too: white is "neutral" in its legend (*game*). They lie under any label, and the skin's panels do not change them.

## 7. HUD size: the owner's rule, measured
**The rule (the owner, 5 Nov 2026):** three presets in the HUD / Display tab. Small mimics the classic HUD's size and scale closely. Normal, the default, a decent medium. Large for a Steam Deck, ROG Ally and the like, auto-selected when a handheld is detected; the player's choice wins; a failed detection falls to Normal.

**Today's four knobs, and what each does:**
| Knob | Reaches | Range |
|---|---|---|
| `hud_scale` | flight HUD's k | 80-130 |
| `text_size` | every FLvibed text and k | 80-150 |
| `deck_mode` | text 120, margins to a gutter; automatic at exactly 1280x800 | auto / on / off |
| `game_text_size` | the game's own fonts | 100-150 |

Two hard stops: `Hud2Scale` never lets k under 1 before the aspect factor, and the 0.024 H floor (`textfit::MinSize`, the lint's `kTextFloor`, and many literal `0.024f` sizes in `hud2_parts.cpp` and `hud2_click.cpp`, which scale with k and are not floors).

**Measured from real frames, 4K (`fight__*`):**
| | k | Cap height | Boxed in a fight, pad / K&M | Boxed in travel, pad |
|---|---|---|---|---|
| Classic's list text | about 0.88 by text | 31 px caps, 38 px with descenders (0.0176 H), pitch 50 px | about 21 % of the screen in panels, read by eye | same |
| k 0.75, floor 0.018 | 0.75 | smaller than classic | 9.9 / 10.4 % | 7.5 % |
| k 0.88, floor 0.021 | 0.88 | as classic (by the ratio 43 : 38 px) | 13.5 / 14.1 % | 10.3 % |
| k 1.0, floor 0.024 (today) | 1.0 | 43 px with descenders | 17.8 / 18.3 % | 13.6 % |
| k 1.18 (text 120 at 4K, Deck margins) | 1.18 | | 24.1 / 24.9 % | 18.8 % |

At 1280x800 the aspect factor takes 10 %: k 0.68 / 0.79 / 0.90 / 1.08 give 9.2 / 12.0 / 15.7 / 22.0 % against a 16 % budget. Today's Deck mode is the 22 %: the size audit's cuts then bring it to about 15-17 %.

**So "Small mimics classic" has two readings, and the frames separate them:** by text it is about k 0.88, not lower: today's default is about 13 % over classic's text. By occupied screen, HUD 2 is already far smaller than classic at every size. k 0.75 is smaller than classic, and at 1280x800 its lower case is near 6.6 px, under Valve's 9.

**The floor:** 0.024 H came from the Deck. Cited limits for a handheld: smallest character at least 0.0167 H recommended, 0.0130 H never less (Valve 9 / 12 px at 800, Xbox 9 / 12 at 720p and 14 / 18 at 1080p, all at 30 cm; `inputs_handheld_detection.md` 8). Agency FB's lower case is about half its font height (*memory*, to read from frames): 0.024 H gives about 0.012 H. Deck text at 120 about 0.0144 H, close to 12 px. So the floor does belong to handhelds: Large holds it at 120, and on a monitor at arm's length a smaller floor is defensible. **The owner kept it "with no exception" before this rule, so this is the owner's call: section 9 item 1.** What follows it: the size audit's numbers, the cue's 0.024 text and box, `UI_HUD2_SKIN.md` 4's table, `kTextFloor`, `textfit`'s tests.

**Decided by the owner, 6 Nov 2026 (D7, option A): floor 0.024 H on Normal, 0.021 H only on Small, with Small keeping Normal's text at 1280x800.** So the table below stands, and the first guess is retired. **The shape, as decided:**
| Preset | k | Text floor | Margins | When |
|---|---|---|---|---|
| Small | 0.875 | 0.021 H | safe margins | the classic's text size |
| Normal (default) | 1.0, today's look | 0.024 H on the flight HUD | safe margins | no change for anyone playing now |
| Large | 1.0 x text 120 | 0.024 H at text 120 | Deck's gutter | handhelds by detection, and exactly 1280x800 |

Why not the first guess (Small 0.75, Normal 0.875, Large 1.0): it makes every present player's HUD smaller by default and Small smaller than the HUD it should mimic. Both sets have real frames in `sheet_01.jpg`.

**One choice for players:** a single `hud_size` row: AUTOMATIC, SMALL, NORMAL, LARGE, on the HUD / Display tab. `deck_mode` retires into it with a migration: a saved 1 becomes LARGE, a saved 2 NORMAL, 0 AUTOMATIC. `hud_scale` and `text_size` stay as advanced trims on top, 100 meaning "the preset's own"; `game_text_size` stays apart (the game's fonts), though Large should raise it. Deck mode's margins become Large's.

**Handheld detection (`inputs_handheld_detection.md`, about 40 sources; nothing run on real hardware), ranked, cached at the first frame, never in DllMain, the reason logged:**
1. A preset the player chose: stop.
2. SMBIOS deny list (`GetSystemFirmwareTable('RSMB')`: Windows 7 on, and Wine builds it from `/sys/class/dmi/id`, so a Deck under Proton reads `Valve` / `Jupiter` or `Galileo` from any launcher): `Fremont` (Steam Machine), GPD non-handhelds: Normal.
3. SMBIOS model table: `Jupiter`, `Galileo`; boards `RC71L`, `RC72LA`, `RC73YA`, `RC73XA`, `RC74XA`; Lenovo's type 1 `83E1` and kin; MSI `MS-1T41/42/52/8K/91`; GPD `G1618-*`, `G1617-*`, `G1619-*`; AYANEO; `ONEXPLAYER*`: Large. A small data file, not code.
4. `RtlGetDeviceFamilyInfoEnum` form 0x2E with an integrated panel of 13 inches or less (Windows only: a stub in Wine, missing on 7; guides tell desktop owners to set it by hand, so the panel test guards it).
5. Integrated panel 9 inches or less.
6. Under Wine with SMBIOS unusable: `SteamDeck=1`: Large.
7. Exactly 1280x800 with no other evidence, Windows or Wine: **Large by default** (the owner, 6 Nov 2026: 1280x800 is "almost exclusively the Steam Deck"); the player's choice wins, at step 1.
8. Anything else, any failure: Normal.

Rejected: chassis type (Deck says Portable, the Ally Tablet, GPD Win 4 Notebook, none Hand Held), `SM_TABLETPC` and `SM_CONVERTIBLESLATEMODE` (both wrong on this desktop), `SteamDeck=1` alone (set on HTPCs, unset outside Steam), vendor-only matches, and resolution as the first signal: since June 2022 a Deck game on an external display defaults to 1280x800 or 720, and a Deck at 1280x720 misses today's rule. So 1280x800 stands last in the chain, after everything that can name the machine.

**Docked:** neither Valve nor Microsoft sets a rule, and no API tells. Large stays on a docked Deck unless H is over 800 (an inference), which matches Valve's own scaling on televisions; on Windows the game's monitor not being the internal one means Normal.

**Side effects:** a Windows window at 1280x800 with no other evidence stays Large, as today's `deck_mode` automatic does, so tests that lean on it keep working. Handheld Companion is CC BY-NC-SA: cross-check only; write the table from the kernel, InputPlumber and hhd. **Unverified without hardware:** RSMB under real Proton, `SteamDeck` in Desktop mode, what a docked Deck sees today, which handhelds report 0x2E, the strings of Xbox Ally, Legion Go S / 2, newer Claws, Zotac, GPD Win 5.

## 7a. The meters (GitHub issue 28)
**The report:** "Energy meter below the crosshair drains to the right, instead of to the left as a standard horizontal meter would. Suggest either having it drain to the left, or make the meter wider and have both sides drain towards the middle." Confirmed in the code: `SegArc(cx, cy, R, 68, 112, ..., m.power, kPower, 8, 2.2f, false)` lights from 68 degrees, the right end, so it empties towards the right (`hud2_parts.cpp:87`; Focus's 70-110 the same, `:1443`).

**The form, by the owner's size rule:** "wider" is the reporter's word, and it is not needed. Mocks `meter_forms`, `meter_sizes_*`, `sheet_11.jpg`, every size at full, half, low and empty. **Recommended: today's 44 degrees and 8 segments, the lit part trimmed continuously from both ends.** It reads at every size down to k 0.79 at 1280x800. 56 degrees and 10 segments reads a shade better at low power and costs nothing below the ring, because an arc's lowest point stays at 90 degrees; 70 degrees starts to read as a second ring and comes within 8 degrees of the hull and shield arcs' ends. A pair of whole segments at a time gives four steps: too coarse for a meter that moves with every shot.

| Size check | |
|---|---|
| Area | about 0.04 % of the screen, unchanged |
| Against the cue | the cue stands at 12 o'clock, the arc at 6: nothing shared, and both are now centred on the ring's axis |
| Against the ring | hull and shield end at 42 and 138 degrees; the arc ends at 68 and 112. The cruise charge arc (48-132) replaces it while charging, as today |
| Against the lower centre | the arc itself no lower than today; the stack's bottom drops by 7b's form C, with the speed under it |
| Over bright | the empty blocks stay dark, the lit yellow thins over cloud: `meter_sizes_3840x2160`. The arc has no plate, as every ring arc |

**Needed:** `SegArc` lights whole blocks from one end; a centred form with a trimmed edge is new, in the kit. Focus takes it too, with 6 segments.

**Every other meter, checked in the code:**
| Meter | Fills from | Empties | Verdict |
|---|---|---|---|
| Shield arc, hull arc | the bottom, both | down, both | right: issue 18, symmetrical |
| Power arc | the right | to the right | **issue 28** |
| Cruise charge arc (48-132; Focus 66-114) | the left | a charge, not a drain | standard |
| Throttle arc (82-98, under the power arc) | the left | to the left | **Changes (the owner, form C, 7b):** inside the ring, centred, filling from the middle out, so both bottom arcs follow one rule |
| Target's shield and hull, escorts' pips, hold, protect, progress (`SegBar`) | the left | to the left | standard; nobody passes `right` |
| Focus's throttle bar | the left | to the left | standard |

## 7b. The bottom of the reticle (6 Nov 2026)
**A player's request, the owner agreeing:** "Speedometer would be better off below the energy meter." The owner: "we can push it to the bottom of the reticle." Sheets: `decisions\Speed_below_the_meter.jpg`, over a bright planet and a nebula, at both sizes; `decisions\Reticle_bottom_forms.jpg`, throttle at 30 % and full.

**Ruled (the owner, 6 Nov 2026): form C.** The throttle inside, above the meter, centred, filling from the middle out; the meter drains to the middle; the speed under it; the bottom drops 0.019 H.

| Stack, top to bottom | The ring's bottom | Reads |
|---|---|---|
| **C (ruled): throttle arc inside the ring, centred; meter; speed** | R + 0.0435 H, 0.019 H lower than today | The shortest stack, and both bottom arcs follow one rule. The throttle takes the speed's old room and the speed's raised place in cruise (+0.045) retires. Meter and speed sit together, as asked |
| B: as C, the throttle filling left to right | the same | Two bottom arcs, two rules |
| A: meter, throttle arc, speed | R + 0.0644 H, 0.040 H lower | Nothing else moves, but the speed lies further over the ship |

**As drawn:** meter at R, 44 degrees, 8 segments (7a). Throttle at R - 0.020 k, 78-102 degrees, 6 blocks, wider than today's 82-98 and 5 blocks, with the width of today's arc; colours and states as built. Speed at R + 0.0285 k, 0.030 H.

**Not on the sheet, read from `hud2_parts.cpp`; no frame yet, so step 3 draws them first:**
- **The cruise readout must move up.** It stands at +0.083 (`3.2 s`, CRUISE ON), and the throttle's new place is +0.080: they would overlap. It takes part of the speed's old room: at about +0.055 a 0.024 H readout keeps 0.01 H from the arc's inner edge (the lint's `centre.overlap`). Still inside the ring, the place of the owner's 31 Oct rule.
- **KILL, BOOST and REV stay where they stand.** Their place is computed from the throttle's radius (`rt`), so moving the arc would carry the word in over the hull arc's end and the charge arc. It must be fixed to its place today, right of the speed.

**A rule moves with it:** the owner's lower centre (31 Oct: "devoid of HUD elements") starts at `RingBottom` + 0.01 H, so it starts lower by that much, and the speed stands over the ship's nose in all forms: this is the owner's word over the older rule, and `hud2layout::RingBottom`, `clearY0`, PRESETS 1.3 (+0.058) and the lint follow. **Size:** the same 0.030 H number, moved: no area added, only a different part of the picture covered. **Over bright:** it holds by its outline over the ship's white fin; the ring's keyline (R6) helps the arcs, not this. **Focus** already has the speed in its under-arc band at +0.14, so it needs nothing.

## 8. Rules that bind every line of the overhaul
Size by importance, flight only (`UI_HUD2_SIZE_AUDIT.md` 5). Legible everywhere, the game's own text included; panels grow first, abbreviation last; in a base the dealer takes centre stage. The lower centre stays clear of HUD. Nothing in a panel overlaps. Marquee, never a cut. No ambiguous symbols; one meaning per flash, under 3 Hz, never colour alone, with an off switch. Motion continuous. Glyph families by Xbox names. "HUD 2 is HUD 2 - all new". Input parity on pad and K&M. Move the window, not the subtitles. 1280x800 is a handheld: test there too. No flash at any screen load. With HUD 2 off, classic exactly. Every user-visible change updates the living spec.

## 9. For the owner (after the critique; the list the gate ends on)
1. **The floor and the three sizes:** the table of section 7, or the first guess, or other numbers. Does 0.024 H stay on Normal or move to Large alone?
2. **Default look:** Normal = today's, or smaller?
3. **Radiation, tail, lag:** homes in the danger stack, or a quieter form? Lag on the HUD at all?
4. **The target keys (section 4 row 3):** answered in P11e2: fixed in the bug track, the list stepped natively by `hudvis` (`exe::ContactListBuild`), which keeps the game's order and filter. Still open for P11f: the game's own filter, which HUD 2 neither shows nor drives.
5. **Hide by bit 1 for the flight roots** in the gate, retiring the two steppers?
6. **`deck_mode`:** retire into `hud_size`, with the migration?
7. **Large at Windows 1280x800** with no evidence: Normal, as the chain says, or Large as today? *Answered 6 Nov 2026: Large by default, the player's choice wins (section 7's chain).*
8. **The target label over the card:** mute it, as the stash does, or let the narrower card clear it? *Moot 6 Nov 2026: the targeting display replaces the native label (`UI_HUD2_SKIN.md` 6.10).*
9. **M and T on the cue:** lit by reach, as designed, though a lit letter may mean no lock (`UI_HUD2_SKIN.md` 12a W6), or lit only with the lock?
10. **The energy meter's form:** 44 degrees as today, or 56 for a shade more at low power? **The throttle arc under it:** centred too, or left? *Answered 6 Nov 2026: form C, drawn at 44 degrees, the throttle centred, inside the ring (7b).*
11. **The loading screen:** a load takes 1.1-1.2 s, so 6.4's 500 ms delay leaves a bar for about 0.65 s: the design's screen, or a plain fill with no bar? *Answered 6 Nov 2026: the place name at once, a bar only past 1.5 s, in 0.4.0 (`UI_HUD2_SKIN.md` 6.4).*
12. **Travel over budget:** every travel context reads 12-20 % against 10 %. Raise the budget for travel, or cut, and what?
