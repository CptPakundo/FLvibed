# HUD 2 in flight: the size audit (5 Nov 2026)

**Why:** the owner, 5 Nov 2026: "Take care not to occlude screen space too much in the flight UI. Try to balance our default 'visible, eligible and in keeping with modern standards' approach with also making sure HUD elements are only as big as they need to be, and only as big as their importance."

**Status:** audit and proposal. No game ran, nothing is built. Frames: `research\evidence\hud2_skin\mock\sheet_flight_audit.jpg` (today beside the proposal, 4K and 1280x800; `flight_lean_*`, `tools\uimock\hud2s.py`). The proposal's frames are mocks: old panels painted out, so faint rims of them remain, and the 4K frame's card has lost its K&M click row, which the numbers below count.

## 1. Verdict
**The fight's boxed area falls by about a fifth without losing one value, and 1280x800 comes under its budget.** 4K 15.4 % to about 12.5 %; 1280x800 19.4 % to about 15 %, against its 16 %. Nothing about the ring, the loadout or the target's data changes. One panel gets the width its spec always gave it, two lose what they did not need, and one thing that should already be gone leaves.

**The main cause:** columns kept for art that is not there, and plates larger than what they hold. The first proposal went further, about two fifths, by taking both plates off; the bright frame showed the consumables cannot go bare, and the owner chose the forms in 3.3 and 3.4.

## 2. Measured
A fight, Standard, the plates' rectangles read from real frames (`focus_frames_2026-10-31\standard_combat-205904`, K&M, 4K; `hud28_deckmode_2026-11-04\deck_combat-233052`, pad, 1280x800), against the lint's totals from nearby builds (4K: `hud2 sim torpedo`, 4 Nov; 1280x800: the same frame's run). Read by eye, so good to about 0.2 points each; the sums land within 0.6 points of the lint's, a check and not a proof.

| Element | Importance in a fight | 4K | 1280x800 | What it needs |
|---|---|---|---|---|
| Ring, shield and hull | 1 | no plate | no plate | as it is |
| Alert card | 1, for 3 s | 1.6 % | 1.9 % | as it is: transient, and the owner's own design |
| Target card | 2 | 5.4 % | 7.4 % | less: see 3.2 |
| Loadout | 2 ("armament status stays on") | 3.7 % | 4.2 % | as it is |
| Consumables | 3 | 1.5 % | 2.2 % | the marks, not the plate |
| Escorts | 4, mission by mission | 3.4 % | 3.1 % | the names and bars, not the plate or the icon |
| Context prompts | 5 | small | about 1 % | fewer, 3.5 |
| **Sum** | | **15.6 %** | **18.8 %** + prompts | |
| The lint | | 15.4 % | 19.4 % | |
| The budget | | 15.9 % peak | 16 % | |

## 3. What changes
### 3.1 Hold and credits leave Standard's flight HUD: already decided, not applied
The owner, 31 Oct 2026: "information the player does not need in flight (cargo, credits) stays off the flight HUD" (B9). Focus was changed then. **Standard's ship panel still shows HOLD 0 / 45 and CREDITS 500 000 in flight** (frame `0.3.0_issues_20261105\std_groups2_live`; `hud2_parts.cpp` DrawShip, `ShipContentHeight`). The strip shows both in a base. With them gone the calm panel holds a silhouette, the ship's name and its class, none of which change in flight: **the panel goes outside a fight or formation.** The cost of keeping it: about 2.6 % of the screen whenever it wakes.

### 3.2 The target card to its spec
`std_ov::kCardW` is 0.44; the spec says 0.36, and the code's comment says "today 0.44". Without a wireframe view, a 0.13 H column holds a 0.07 H icon: at 1280x800 about a third of the card is empty. Proposal:
- 0.36 wide, 0.17 tall (0.21 K&M, with its click row).
- The icon column at the icon when there is no view (0.085).
- The rows as they are: the name alone on its line, the distance beside the faction. The first mock put them on one line and they collided at 1280x800, so that stays.
- With a wireframe view the 0.13 column stays; a 0.186 H name room is enough for the frame's name, and a longer one takes the marquee.

Result: 5.4 % to 3.4 % (K&M 4.3 %); 7.4 % to 4.5 %.

### 3.3 Consumables: small dark cells, no outer plate (the owner, 5 Nov 2026)
The plate goes; each item keeps a small dark cell of its own, the low state's red or blue edge with it. **Bare marks failed:** over a bright scene the icons and the cross washed out and the red low cell vanished (`sheet_plates_bright.jpg`). The cells hold, at 0.9 % of the screen against 2.2 % at 1280x800, about 0.6 % at 4K.

### 3.4 Escorts: a slim plate (the owner, 5 Nov 2026)
A plate stays, sized to the rows: the names and bars, the silhouette column gone in a fight: it is the list's decoration, not its data. 2.3 % at 4K against 3.4 %; 2.7 % at 1280x800 against 3.1 %. Free text as Focus has it also held over bright; the owner kept the plate in Standard, so the bottom corners stay balanced. Focus keeps its free text.

### 3.5 Prompts: the lowest rank, so the strictest rule
Seven at 1280x800 against the cap of 6, and the glyph rings of HOLD REVERSE and HOLD TURRET are 0.05 H across, larger than the data they sit beside. They teach a binding: `UI_HUD2_PRESETS.md` retires them after 3 uses, and the rule should hold in a fight first. Glyph 0.05 to 0.036 H; a fight keeps its 2. Not in the numbers above.

### 3.6 Deck: text grows, plates do not
`deck_mode` reads text at 120 and `Hud2Scale` gives k 1.08, which scales plates with it. Panels that fit content (5.26-5.28) follow their text; those with fixed proportions grow for nothing. That is part of the card's 7.4 % against 5.4 %; the rest is the smaller screen. Rule: at 1280x800 a plate's size comes from its content, never from k alone.

### 3.7 Left as they are
The ring, the alert cards, the loadout, the nav text, the threat rows, the comm feed: each has no plate or has earned one. The weapon group cue adds at most 0.15 %, no plate.

## 4. Result
| | Today | Proposal | Budget |
|---|---|---|---|
| Fight, 4K, K&M, with an alert up | 15.4 % | 12.5 % | 15.9 % |
| Fight, 4K, pad, with an alert up | | 11.6 % | 15.9 % |
| Fight, 1280x800, pad, with an alert up | 19.4 % | 15.2 % (with prompts) | 16 % |
| Calm, ship panel awake | about 2.6 % more | none of that | 10 % |

**Nothing is lost:** every value in a fight stays, in the place it has. Hold and credits stay where the owner put them, in a base.

## 5. The rule, for what comes next
1. Rank the element against what stands near it.
2. Start at the text floor.
3. A plate must earn its area: if the text reads over the worst background without it, it goes.
4. A column exists for content that is there now.
5. State what it covers, in percent of the screen.
6. The lint's budgets stand; `hud2 lint`'s `boxed=` is the measure.

## 6. Not read
- **Calm and travel: read 5 Nov 2026 (`hud2 lint`, the gate's capture, today's build, Standard, pad / K&M, budget 10 % unless given):**
  | Context | 4K | 1280x800 |
  |---|---|---|
  | Calm | 8.3 / 10.1 % | 10.3 / 12.1 % |
  | Travel (sim) | 13.6 / 15.8 % | 17.1 / 20.1 % |
  | Cruise, real | 11.9 / 13.9 % | 15.6 / 17.7 % |
  | Lane (sim) | 12.6 / 15.2 % | 15.9 / 19.4 % |
  | Formation (sim) | 7.0 / 8.4 % | 9.1 / 10.8 % |
  | Fight, real (budget 16 / 17 %) | 17.8 / 18.3 % | 22.5 / 22.7 % |
  | Torpedo (sim) | 15.4 / 19.4 % | 19.4 / 24.9 % |

  **The owner, 6 Nov 2026: fix the metric, report steady and awake apart.** `boxed` ignores `hud_fade`, which takes panels to zero after 4 s, so these are awake peaks, not what stands on screen in travel: the "overrun" is the metric's fault (critique R14). Until the lint splits them, this table reads as peaks only. This audit's 3.1 and 3.2 reach them all the same. Section 1's 15.4 %, labelled K&M, matches `hud2 sim torpedo` on the pad; here K&M reads 19.4 %, and a real fight with threat rows 18 %: section 4's "today" column reads low by up to 4 points, so the percentages saved stand and the totals need re-reading from the same forms.
- **Focus:** not measured; it already has 3.1 and free-text escorts; 3.3's cells apply if its consumables stand bare. The card's compact form may need nothing.
- **Bright scenes:** the bright mock used the main menu's planet. The cells and the slim plate need frames over a real planet and a nebula, at both sizes. A value that fails 4.5:1 keeps a scrim.
- **21:9, 4:3:** not measured. 4:3's width is tight, so it gains most.

## 7. The owner's answers (5 Nov 2026)
1. **Standard's ship panel:** removed outside a fight or formation (your own ship's wireframe goes with it in calm flight).
2. **Consumables and escorts:** after the comparison (`research\evidence\hud2_skin\mock\sheet_plates_compare.jpg`, `sheet_plates_compare_4k.jpg`, `sheet_plates_bright.jpg`): "Slim plate for escorts, small dark cells for consumables." So 3.3, 3.4. The bottom panels' one-height rule stands for the escorts and the loadout, within the content heights of `UI_HUD2.md` 5.29.
3. **The wireframe in the target card:** kept, in the 0.36 card.

**Order:** after the probes, before the weapon group cue, so the cue goes into the final layout: it is `hud2_layout.h`, the presets' data, and the lint, in the HUD module; the living spec and the owner's frames follow.
