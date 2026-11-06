# Target information in the scene: brackets, a tag at the target, leader lines
Status: current
Checked against: FLvibed 34474db9 (6 Nov 2026); HDE 0.7.1 test install; upstream not read
Dirty paths at check: `tests/fingerprints/state/` (untracked, not judged)
Watch: modules/hud/docs/UI_HUD2_SKIN.md, modules/hud/docs/UI_HUD2_PRESETS.md, modules/hud/docs/UI_HUD2_GROUP_CUE.md, modules/hud/docs/UI_HUD2_INPUTS.md, modules/hud/src/hudvis.cpp, core/flexe.h#HudDrawPart|VtHudIndicators, core/game/fl.cpp
Evidence: none saved. Decompiles reproduce with `tools\ghidra\gq.js exe dec <addr>`; prior art `research\stash_patches\stash0_target_label_hook_WIP.patch`; the real bracket frame `research\evidence\hud2_gate\decisions\D5_cue_seat.jpg`
Sources: listed at the end

Tags: **[measured]** project frames or numbers. **[read]** code or decompile read this round (decompiled: not verified in game). **[text]** a page read. **[shot]** an image seen. **[snippet]** a search result, page unread. **[memory]** recollection. **[inference]** this note's reasoning.

## What it means for FLvibed

**Both ideas can be built. The game separates the pieces well enough:** the bracket corners go by nulling one pointer with no hook, and the label and bars go by a hook already drafted in a stash. Targeting, clicks, arrows, waypoint diamonds, the lead cross and the target sound all stay. No probe has run yet (section 3).

**Recommendation: Form A, a tag at the target, no plate, no standing line.** HUD 2 draws small corner ticks sized to the object, with name, distance and two thin bars in outlined text beside them. When the target enters the ring zone, the tag parks in one fixed seat above the weapon cue. That is the fight's normal case. **Form D, a hairline from a threat row to its ship, follows as a second step**, only while the player picks a target. That is where the owner's multi-line idea earns its place: on-screen ships other than the target carry no mark at all in the game [read], so the rows say who but not where.

**Against the two ideas as stated:**
- "Compress next to the target" holds, with one change: next to the target fails exactly when the player aims, because the target then sits inside the ring. Hence the parked seat.
- "A multi-line thingie" as a standing display is not recommended. No confirmed game does it in live combat (section 1), and the research says labels should stay still and lines short (section 2). The galleries that would settle this were blocked, so "not confirmed" is not "does not exist".

**What this reopens,** the four items of `UI_HUD2_SKIN.md` 16: section 5 below. **Decisions for the owner:** section 6. **For `IDEAS.md`:** section 7.

**Since this note (6 Nov 2026):** the owner asked for a comparison of different concepts, not of these forms alone. Four concepts, eleven versions, are drawn on real frames: `research\evidence\hud2_gate\decisions\Target_*.jpg` (`tools\uimock\hud2t.py`). Forms A, C and D stand among them as candidates. Section 6's frames for bright scenes, capitals and off screen wait until a concept is chosen. A frame of Mass Effect's galaxy map the owner showed was a generic reference only.

**Round 2 (6 Nov 2026, not final):** the owner dropped the two concepts anchored to the reticle or linked by a line, and any anchor at the target card: form C and form D's line go with them. Two go on: worn by the ship (caret, straight bars, numbers after a hit; the circle stays the player's) and a mini card that turns into form A. Sheets `Target2_*` (`tools\uimock\hud2u.py`), with the target inside the ring and close ships.

**Ruled (the owner, 6 Nov 2026; `UI_HUD2_SKIN.md` 6.10, not built):** with a soft bed behind the words, and name and distance kept on the card. The candidate, a combination: a new target opens as the mini card, which settles into the form worn by the ship, bars at the bottom. The owner is good with round 2's other picks: numbers for 3 s after a hit, the shoulder seat inside the ring, the close ships' steps. Drawn: `Target3_candidate.jpg`, `.gif` (`tools\uimock\hud2v.py`). The cases section 6 held back, with carets by attitude: `Target3_cases.jpg` (`tools\uimock\hud2w.py`); capitals and waypoints stand on the playthrough (vanilla, 1080p), no HDE frame holding them. The shoulder seat moved out to 0.10 k to clear the ring's low-shield and hull numbers.

**A waypoint target by its type (the owner, 6 Nov 2026):** a ship stays a ship and a solar a solar; being the waypoint is an addition. Compared on `Target3_waypoint.jpg` (`tools\uimock\hud2y.py`): the game's diamond kept, or a small mark before the name (this sheet's pick). Hiding the diamond's art (+0x7B4) is unprobed. The owner then asked for the small diamond round the attitude caret, lightly glowing, the name line plain: `Target3_waypoint_caret.jpg` (`tools\uimock\hud2z.py`). Both glows breathe (the owner): the lock relatively fast, 1 Hz as D4, the waypoint somewhat slower, 0.5 Hz, both speeds kept by the owner; steady under `skin_motion` FADES and NONE: `Target3_breathing.gif` (`tools\uimock\hud2b.py`).

## 1. How games show a target

The evidence is thin: Fandom, the Hard-Light wiki and gameuidatabase refused access, and only three game images opened.

| Game | At the target | Elsewhere | Crowd | Off screen | Stands on |
|---|---|---|---|---|---|
| **Freespace 2** | Corner brackets **sized from the model's on-screen bounds**, minimum 20-30 px; distance; name and class by flag; up to 4 attacker icons | Target monitor, escort list [memory] | Full brackets on the selected only [memory] | Edge triangle, and a triangle **orbiting the reticle**; missile triangles with a distance tail | [text] FS2_Open source |
| **Elite Dangerous** | Ring; beside it, **no plate**, name, distance, ETA; **a hairline from the ring under the name** (a supercruise destination, not a combat target) | Wireframe and data lower left; subsystems on the left panel; 3D scanner | Scanner, contacts [memory] | Compass dot [memory] | [shot], [text] |
| **Star Citizen** (3.10) | "Best" target from the view cone; locked ones add distance, ribbon, direction | | Crew-shared pins, three slots [snippet] | Locked targets keep selection | [text] comm-link |
| **EVE Online** | Small bracket per object, health near it | Overview list, the real interface [memory]; a row of locked targets | Filtered per tab; **players switch brackets off in big fights** | | [snippet] |
| **Everspace 2** | Bars, lead [memory] | | Triangles hard to read in crowds, one player says; another disputes | Edge triangles, scaled and faded by distance | [text] Steam thread |
| **Ace Combat 7** | Coloured target boxes, UNKNOWN yellow until lock; short tags beside them, no lines [memory] | Radar | Boxes on every contact [memory] | A green vector to the target, red on lock | [text] |

Not confirmed in the scene: X4 (three bottom monitors, target at right [snippet]), Squadrons (a white box on the selected enemy [snippet]), Rebel Galaxy Outlaw, Chorus, Project Wingman.

**Leader lines in or near live action.** Checked: Cyberpunk 2077, Hardspace: Shipbreaker, Armored Core VI, Horizon, Battlefield 2042, Metroid Prime, Xenoblade X, Star Citizen 4.x, The Division, Ghost Recon, Andromeda, Homeworld 3, Warframe, Zone of the Enders.
- **Not one was confirmed** with an elbowed line to a label during combat. For most the evidence is missing, not negative. A person on gameuidatabase could settle Armored Core VI, Hardspace and Ace Combat 7 in minutes.
- **Cyberpunk 2077's scanner** [shot, two frames]: scene dimmed, target outlined, a name above it, data docked at the side with **nothing joining them to the target**. Time slows [snippet].
- **Scan modes** (Hardspace, Metroid, Horizon): the richest annotation lives in a mode that disables tools or weapons, or slows time [snippet].
- **Elite's hairline** is the only live line confirmed: a stub.
- **The real precedent is air traffic control:** every label joined to its aircraft by a leader line (Eurocontrol 21/00 [text]): a plan view of slow objects, read by a trained controller.

**Pattern [inference from the above]:** a mark on the object; text beside it or docked; detail docked; a list or filter once the sky is crowded; a separate edge or reticle cue off screen. The owner's memory may well be right for a game this round could not open: one named frame would outweigh this section.

## 2. What the research says

**Eye travel and proximity.** Wickens' proximity compatibility principle: information used together belongs close together in perceptual space [snippet, the 1995 paper's record]. Fadden, Wickens and Ververs: conformal head-up imagery beats non-conformal [snippet]. Ververs and Wickens: head-up clutter hurt detection of instrument changes and traffic [snippet]. So [inference]: identity and distance belong at the thing (`UI_HUD2_PRESETS.md` 1.7 says so from the York studies), but "next to the target" gains only if it replaces more than it adds.

**Labels attached to moving things.**
- Azuma and Furmanski, ISMAR 2003 [snippet, abstract only; the PDF refused]: of four placement algorithms, the cluster-based one placed best, yet **people read labels fastest with the algorithms that removed overlap soonest, even when placement was not ideal.**
- Madsen et al., TVCG 2016 [snippet, abstract; the full text refused]: people locate annotations significantly better when they are **in object space and the layout's update rate is limited**, and they prefer a limited rate to a continuous one. Numbers not read.
- Eurocontrol EEC Note 21/00 [text]: no radical jumps (R-MOV.1); **a selected label does not move** (R-MOV.2); a rest position it returns to (R-POS.1, 2); a threshold against small oscillations (R-POS.3); line length bounded (R-LIN.1); lines cross neither lines nor labels (R-OVE.4, 5); attention stays on the task (R-HMI.4).
- US patent 6,047,233 [snippet]: keep the tag fixed, stretch the line as the icon moves, move the tag only at a set length.
- AR labels in clutter, arXiv 2507.00198 [text; 15 people]: accuracy equal across placements; per-item leader lines grow long in dense scenes.

Together [inference]: a stable place beats a clever one; solve the layout rarely; a line is a price paid when the label must stand elsewhere, so keep it short and do not let it cross.

**Backing plates against outlined text.**
- Gabbard, Swan, Hix et al., IEEE VR 2007 [text; 24 participants, outdoor AR]: **billboard was worst, by a wide margin: F(3,69) = 152 on error, a 0.711 error difference and 797 ms; drop shadow, outline and none were equivalent.** The authors trace this to their colouring, which matched plate to background and ignored text against plate: it condemns a badly coloured plate, not a dark one.
- Gattullo 2015, a 2026 review [snippet]: billboard and outline, light text on dark, are the reliable forms.
- No winner: a plate buys certainty and pays with area; outline costs almost nothing. **Here [measured]:** HUD 2's text outline already holds over the ship's white fin (`UI_HUD2_INPUTS.md` 7b); the native label has 1 px strokes at 1280x800, HUD 2's text a stem of 0.0033 H, 2.6 px (critique R1, R13).

## 3. Feasibility

### 3.1 What the game does [read, Freelancer.exe 1.1 / HDE 0.7.1]

- **`Targetable_Objects` has exactly five children**, from the constructor 0x4E8FE0: `ReticleRangeText` (a text control, font HudSmall, field +0x40C) and the four alerts (+0x410..+0x41C). So name and distance **are** `ReticleRangeText`, moved and drawn again per label (vt+0xA8 position, vt+0x34 draw).
- **The rest of the per-frame calls**: 0x4ED180 the records and mouse test; 0x4EE260 all indicators; 0x4F2070 the lead marks, from the list 0x4EF030 fills (*inference* from its callers); 0x4F2220 the mouse's fire state; 0x4EBF70 and 0x4F1DD0 not worked out.
- **0x4EF030 draws one on-screen target:** the four corners (`+0x7A0`), hats (`+0x7B0`), the diamond when the target is a waypoint (`+0x7B4`), bars as immediate-mode quads (rectangles at +0x780..+0x79C, set only in the constructor), distance through 0x4EEF50, name (cached in +0x854 / +0x888, wrapped at 0.171), multiplayer level (+0x8C0). **It also does things that are not drawing:** plays the target sound (`PlayUiSound(0x24)`), runs the 0.3 s zoom-in, and **fills the lead list**. The original must run.
- **The bracket is not quite fixed.** Scale = 1 - (1 - s/100) x 0.25, with s from 0x4ECFD0 clamped to 10..100: about 300 x radius / distance. So 0.775 for a fighter past a few hundred metres, up to 1.0 for something large and near. That agrees with the 12 measured boxes: **it never follows the object's real size.**
- **Clicks do not depend on the drawing.** 0x4ED180 tests the mouse against position ± s pixels from each record, and 0x4EC450's cursor names (hostile, friendly, fire) come from that.
- **Other on-screen ships get no mark.** In 0x4EE260, an on-screen non-target gets only a name for a group member under 3,000 m.
- **Waypoints stand apart:** the two waypoint records (+0x380, +0x3B4) draw through 0x4EBDE0 and 0x4F0620. Only a target that is itself a waypoint uses the diamond inside 0x4EF030, its distance through the same 0x4EEF50.

### 3.2 Routes

| Piece | Route | State |
|---|---|---|
| **Corners** | Write 0 to indicators +0x7A0; put it back on leaving. One writer (constructor 0x4E9F09), four readers, all in 0x4EF030, behind `if (+0x7A0 != 0)` at 0x4EF3E0. **No hook.** Re-apply when the HUD is rebuilt, as `hudvis` does. The object is `Targetable_Objects` - 0x38 (RE doc "Target indicators") | read; unprobed |
| **Label and bars** | The stash: detour 0x4EF030 (`HudIndicatorDraw`), set a mute flag, call the original; gate 0x421CC0 (`HudVertex`) and the text class's vt+0x34 by the flag; `hk::PatchVtable` exists (`core\hook.h:220`). All or nothing, fails closed | Written for the old tree, never landed: needs a re-cut to core, and flexe.h has neither symbol |
| Keep | **Must not mute a waypoint target's distance:** test the record's 0x200 and 0x4D4220, which the stash does not | new |
| Stays by itself | Off-screen arrows and their bars (0x4F0900, outside the mute), waypoint marks, lead, group names, alerts, click tests | read |

Not the existing `HudDrawPart` hook for the corners: it is the subtitles host's (`modules\subtitles\src\host\main.cpp:134-176`) and only moves parts. A deny on `Targetable_Objects` as a root stays wrong: it would blank all of the last row.

### 3.3 Position and size

| Source | Gives | Rate | Note |
|---|---|---|---|
| `fl::WorldToScreen` (`core\game\fl.cpp:238`), `WorldToFlightScreen` | pixels and depth | on call | Live; used by the controller and `aim`. **Nothing in `modules\hud\src` projects today** |
| `space::TargetBody` (`space.h:269`) | target's centre and physical radius | on call | Gives a true on-screen extent: radius x scale / depth |
| `space::Contacts` (`hud2_data.cpp:662`) | every contact's pos, no radius | HUD 2 reads it in `Slow`, 10 Hz | **Too slow to anchor a tag to** a moving ship |
| **The game's records** (+0x374, stride 0x34; count at +0x36C +0x800) | per contact: flags +0, screen x, y, z +4..+C, world +0x10, id +0x1C, s +0x20, distance +0x28, on-screen +0x2D | the game's own, per frame | Several targets for a guarded read, at the game's own point. A new reader in core, with a layout check |

**One target: yes** (projection, `TargetBody` for size). **Several: yes**, through the records. HUD 2 does neither yet. A line primitive exists (`core\ui\hud2_kit.h:66`). `aim::Current` gives the aim point, not the centre.

### 3.4 Probes (one background instance, muted; each a debug verb, no code kept)

| # | Question | Pass |
|---|---|---|
| T1 | +0x7A0 null: corners gone; targeting, click, RS-click pick, sound, lead, arrows, diamonds as before; what `CurrentTargetHat` (+0x7B0, not nulled) still marks | frames and log, K&M and pad forms |
| T2 | The re-cut stash: label and bars gone on screen; waypoint target keeps its distance; arrow's bars stay; group names stay; **+0x7A0 returns after load, launch, jump** | frames |
| T3 | A tag on the object or a frame behind? Native against HUD 2 pixels, turning hard, at 30, 60, 120 fps | under 1 px, or a measured lag |
| T4 | Records against `WorldToScreen`, also in turret and rear view | agree, or name who wins |
| T5 | Capital ships, planets filling the screen: centre and radius usable? | frames |
| T6 | FLSharp, HDE's `HudTarget`, FLUF, upstream: anyone hooking 0x4EF030 or 0x421CC0 (0x421CC0 has 100 callers: the gate must cost one branch) | a sweep, on the owner's word |
| T7 | Real width at 0.024 H of the longest names, faction prefix and lane rings included | numbers for the seat |
| T8 | A real fight's frames: how long is the target inside the ring zone? Sets the park thresholds | footage or autopilot logs |

## 4. The forms

Numbers: 0.024 H text is 52 px at 4K, 19 px at 800. A two-line tag with bars is about 0.066 H tall and 0.13-0.17 H wide for a 16-letter name (*estimate*, T7): 0.5-0.65 % of a 16:9 screen. Ring R 0.10 k; cue cells at `cy - 0.093 k`, top at `cy - 0.106 k`, half-width 0.059.

### Form A: the tag (recommended)
**What:** four short ticks, HUD 2's own, sized from the object's on-screen radius with a floor (the Freespace rule), in attitude colour. Beside them, outlined text: name, surface distance, two thin bars. Outside the ring zone it rides the target, changing side only past a threshold, by a slide. **Inside the ring zone (enter at R + 0.03, leave at R + 0.06, to be set by T8) it slides to one seat, centred, directly above the cue**, and stays there. No line: inside the ring there is one target and it wears the ticks. Off screen: the native arrow and bars, with name and distance back on the card.

| Check | |
|---|---|
| 4K, 800 | Floor text, 2.6 px stem at 800 against the native 1 px. The bars at 0.005 H are 4 px at 800: confirm in a frame |
| The cue | **Solves the clash that R3 measured** (29-42 % of ring positions): the label no longer falls on the row. Seat above it, nothing shares its cells. D5's seat stands |
| Multi-target | One tag. Threat rows stay as they are. Other ships stay unmarked as in the game, until Form D |
| Pad, K&M | Identical. Draw only: no hotspot, clicks go to the game's records. RS-click radius unchanged |
| Robust | Corners and mute both in, or neither. A refused symbol, or the module off: the native bracket, no tag |
| General, efficient | One rule for every object, size from the game's radius; one projection, one text block a frame |
| Effective | Smaller than the 0.24 x 0.16 H bracket on a fighter; reads better at 800 |

**Against it:** the parked seat is not at the target, so the fight's commonest moment reads as a fixed line: 0.15-0.2 H of eye travel, not zero (though far under the card's). Two states mean a target skimming the ring's edge shows movement; hysteresis limits it, frames must prove it. And the tag covers scene where the player looks for the next ship.

### Form B: the tag on a plate
Form A with a dark plate at the skin's opacity. **Certain over the sun and bright planets; the rest as A.** The lint fails a plate in S1 (`UI_HUD2_PRESETS.md` 1.3), so it needs a third exemption. Adds 0.5-0.65 % to a boxed budget of 13.5-15.9 % in a fight, and a moving dark box near the aim point hides where a player looks for the next target. **Worth one frame, not a form of its own:** if outline fails over the sun, the fallback is a soft dark halo behind the text, a wider outline, not a plate.

### Form C: standing callouts
Elbowed hairlines from the target and up to two threats to tags in fixed slots either side of the ring, solved at a limited rate, sticky, no crossings. **The best answer to "where are they", and the owner's word.** Text as A, pad and K&M alike.
- **Legibility:** a 1 px hairline over a nebula at 800 is near invisible; 2 px is a scratch across the sightline.
- **Cue, ring:** lines to a target near the centre cross the ring and may cross the cue.
- **Multi-target:** its strength, but when the ship turns, the far ends sweep and the lines swing through the aim point.
- **Principles:** needs a solver and rules for when no crossing-free layout exists (robust, general); needs the records (efficient); works against R-HMI.4 and every clutter finding at the moment of highest load, and three moving lines are clutter of the kind the owner set out to remove (effective). No confirmed game does it live.

**Against rejecting it:** this round could not open the galleries; a game may do it well. If the owner names one, look at it first.

### Form D: row tethers (recommended second step)
The rows stay where they are. **While the player is choosing** (picker or wheel open, K&M hover on a row, cycling targets, a few seconds at most), a hairline joins the focused row to its ship, with a small tick on the ship. Nothing moves but the line's far end: the patent's rule and R-MOV.2 at once. One line at a time.

- **Legibility:** 2 px at 800 with a dark edge; a frame must show it over a nebula. **Cue:** drawn under the ring, dimmed inside it.
- **Multi-target:** answers what the rows lack, for the focused row; rows at 2 (pad), 2-4 (K&M). **Pad, K&M:** the same trigger, focus on a row.
- **Principles:** a row not on screen shows no line, the arrow rule stands; needs the records; runs only while choosing; small, with an off setting.

**Against it:** a line across the screen for 1-2 s is a strong event for a small job, and fast cycling flicks lines across the scene. It could be cut to the tick alone.

### Comparison

| | Today | A | B | C | D (with A) |
|---|---|---|---|---|---|
| Name legible at 800 | No, 1 px | Yes | Yes | Yes | Yes |
| Clear of the cue | No, 29-42 % | Yes | Yes | Partly | Yes |
| Over the sun | Weak | To prove | Certain | To prove | To prove |
| Says where the threats are | No | No | No | Yes | On request |
| In a shipped game, live | Yes | Yes (Elite) | Partly | Not confirmed | Not confirmed |

## 5. What the design must re-read (`UI_HUD2_SKIN.md` 16)

1. **Kept roots (3.1, `UI_HUD2_INPUTS.md` 3).** `Targetable_Objects` stays a kept root, whatever form wins. What changes is below root level: three part rows, "corners: hide, home the tag", "label: hide, home the tag, not on a waypoint", "bars: hide on screen, home the tag"; `ReticleRangeText` keeps drawing the rest. **INPUTS 2's note:** the child list is answered (five, no more); the per-frame calls are answered but for 0x4EBF70 and 0x4F1DD0 (3.1).
2. **The cue's dark cells and D5.** The stated reason for the cells goes: they answered the bracket's label under the row, and no label stands there in A, B or D, but for a waypoint target's native diamond text (R3), which the cells still answer. **D5's seat, above the ring, gets stronger:** the column alternative existed to clear the label. The cells still serve over bright scenes and the owner chose them, so keep them unless a bare frame reads as well; they do not decide the form.
3. **R13's exception and the 0.36 card.** One surface distance holds in all forms: the tag reads the value the game shows. With the tag on screen, **"one value, one place" can return:** name and distance on the tag; the card keeps wireframe, class, faction, scan, lock; off screen both go to the card's top line, by the morph `UI_HUD2_PRESETS.md` 1.7 once drew. Hold it about 0.5 s past the edge (*inference*), so a target at the edge does not swap. That undoes R13's exception and is the actual decluttering; R13's objection, a 1 px label, falls as HUD 2 draws it. The owner decides (section 6); until then name and distance stand on both.
4. **Section 14, the target label over the card.** Moot under A, B and D: the native label is gone, and the tag keeps out of HUD 2's panels, as the arrows do (`display.cpp:2633`, `Hud2KeepOuts`). The stash's rectangle test is dropped, its mute kept. `UI_HUD2_INPUTS.md` 9 item 8 closes with it.

`UI_HUD2_PRESETS.md` 1.3 loses the bracket label as a native S1 occupant (the tag: text only in S1, no plate); `core\ui_meta.h:1174-1176` follows the ruling.

## 6. Decisions for the owner

| # | Decision | Pick | Strongest case against |
|---|---|---|---|
| 1 | Form | **A, then D** | C is what was asked for by word; the park puts the text away from the target when it matters most |
| 2 | Background | **None; a halo if the sun frame fails** | Only a plate is certain on every background, and the evidence does not rule it out |
| 3 | Name and distance on the card while the tag shows | **Off the card, one place** | A fixed home costs no search, and handover at the edge is one more behaviour |
| 4 | Ticks or nothing on the object | **Ticks.** Every game read keeps a mark on the object; in a crowd the name alone does not say which ship | "Remove the targeting brackets" was the word: ticks are still small brackets |
| 5 | The game behind the idea | **If it can be named, a frame of it before C is dropped** | |

**Unproven:** nothing here was tried in game: every route, alignment (T3), outline over the sun, widths (T7), ring time (T8), plugins (T6). Leader lines in games not opened. Azuma and Madsen rest on abstracts.

**Frames to mock**, each at 4K and 1280x800, over real captures (`research\evidence\hud2_gate\frames\`) where the scene exists:
1. Fighter at about 500 m off the ring, nebula: today against A.
2. Target inside the ring, M and T lit: today, A parked, A not parked.
3. Sun and bright planet: A, A with halo, B.
4. Capital ship, close and far: ticks from the radius against the native bracket.
5. Fight with three hostiles: A; C; A + D with a row in focus.
6. Off screen: arrow with the name on the card, and the handover.
7. Target beside the card, top right: tag against panel keep-outs.
8. Mission waypoint target: diamond and distance kept.
9. The 0.36 card with and without name and distance (decision 3).
10. Motion: a short silent clip of A parking, leaving, and a side flip.

## 7. For `IDEAS.md` (written there 6 Nov 2026: U64-U68)

- Form A with decisions 1-4; Form D; a core reader for the indicator records; probes T1-T8 into `UI_HUD2_SKIN.md` 12.
- FREELANCER_RE: section 3.1's facts. `UI_HUD2_CRITIQUE.md` R3's "fixed" to read "0.775-1.0 of nominal, never the object's size".

## Sources

Games: github.com/scp-fs2open/fs2open.github.com `code/hud/hudbrackets.cpp`, `hudtarget.cpp`; interfaceingame.com/wp-content/uploads/elite-dangerous/elite-dangerous-chasing-players.jpg; gamepressure.com/elitedangerous/left-target-panel/z47391; api.star-citizen.wiki/comm-links/17711; steamcommunity.com/app/1128920/discussions/5/3801651661439694254; uploadvr.com/star-wars-squadrons-hud; acecombat.wiki.gg/wiki/HUD; gamepressure.com/ace-combat-7/aerial-combat/zcbccd; zhengdong-uga.github.io/InterfaceFire (scanning frames). Read and tagged by a research agent; [shot] is its reading.

Research: Eurocontrol EEC Note 21/00 (eurocontrol.int); Gabbard et al. 2007 (ed-swan.github.io/publications/papers/2007_Gabbard-etal_Text-Drawing-Styles_IEEE-VR.pdf); Azuma and Furmanski, DOI 10.1109/ISMAR.2003.1240689; Madsen et al., DOI 10.1109/TVCG.2016.2518318; arxiv.org/html/2507.00198v1; Wickens and Carswell 1995 (trid.trb.org/View/452882); Fadden et al. (saemobilus.sae.org/content/2000-01-5542); US patent 6,047,233.

Code: cited in place. Decompiles 0x4E8FE0, 0x4EC450, 0x4ECFD0, 0x4ED180, 0x4EE260, 0x4EEF50, 0x4EF030, 0x4F0620, 0x4F0900, 0x4F2070; `research\disasm\Freelancer.asm` for the field readers.
