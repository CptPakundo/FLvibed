# HUD 2 layout presets: design (revision 2, decided by the owner; not built)

**Revision 2 (29 Oct 2026)** applies the owner's answers in the body: (1) reveal / peek §1.6; (2) the cruise prompt per
preset (§2); (3) sub-target = a wheel action (§1.4); (4) no duplication §1.7; (5) Guided = `ap_hints` 1 + the first-run
guided setup (§2.4); (6) par anonymised (§2.3). Plus two owner rules of the day: **standard pad first** (every input
exists on a plain Xbox pad, the DualSense and K&M; paddles only as optional remaps) and **HUD presets adapt to the
controller preset** (§4).

Status: the design is decided (the owner, 29-30 Oct 2026: §7); **Focus's layout is built (31 Oct 2026, §2.2 "Built"; run in game 31 Oct 2026; the owner judged its frames: §7, `UI_HUD2.md` 5.19-5.23)**, no other preset is built or measured in game yet (§6 step 4's plumbing, `hud_preset`, is built 31 Oct 2026: Speedrun and Guided still draw as Standard; §6 step 5's layout core, `hud2_layout.h`, is built the same day with Standard = today's HUD bit for bit; §6 step 3's
lint is built, 31 Oct 2026; its live baseline on today's HUD: `docs\hud2_lint_baseline.txt` - first FAIL in every context, after the 5.16 fixes 0 FAIL / WARN only (re-run 31 Oct ~15:45):
the loadout plate in zone S1, centre-stack gaps 0.004-0.007 H in combat / cruise, the destination twice in dock / lane; each answered the same day, layout or lint, in `UI_HUD2.md` 5.16, the in-game re-baseline pending) - the build order is §6 and the private work queue. Tags: **[O]** an owner rule or decision; **[R]**
outside evidence - with a URL = fetched in round 3 (29 Oct 2026), "[R, memory]" = an earlier round's claim not
re-fetched, plain [R] = `research\evidence\hud_presets\hud_web.md` / `ADAPTIVE_UI_RESEARCH.md`; **[C]** a code or repo
fact; **[D]** a design choice that needs your eye and in-game frames. Sizes are fractions of screen height H at 16:9,
k = 1, from the top-left; the screen is 1.778 H wide (area 1.778 H²), centre (0.889, 0.50). "Boxed" = plates only.
Today: 9.7 % boxed in calm, 18.6 % in combat [C].

## 1 The shared foundation

### 1.1 What every preset keeps
- HUD 2's art: navy glass plates with the chamfered chrome frame, cyan / active yellow / hostile red / friendly green, Agency FB, HDE's flat icon sheets. One plate style; each preset's identity comes from layout, reticle and density [O, hud_design_principles 3].
- Corners by job: top-left where you are going, top-right what you look at, bottom-left your ship, bottom-right what you fire [C, UI_HUD2 1.4]. Margins 3 % W at the sides, 0.03 H top and bottom [C] (5 % / 0.05 H until 0.2.0: the TV title-safe area; 3 % = PC practice, the owner 1 Nov 2026).
- The game's own reticle, lead pip, target brackets **with their name / distance label and bars**, missile lock, damage arcs and off-screen arrows stay native and untouched (`VtHudIndicators`, UI_HUD2.md 5.9: hiding the label needs an indicator hook, decided against) [C]. Gimbal aim never steers [O].
- LB hints show whenever no target is selected: 45 % in calm, 100 % in combat; they never decay [O, 63dd152].
- **Every preset has its own combat layout.** No preset "turns everything on" [O, 29 Oct]; UI_HUD2.md 87's global rule is retired.

### 1.2 Shared rules
- **Glyphs** [O, hud_glyph_badges]. A prompt glyph is its pill's height minus padding (0.032-0.036); a badge on a panel about half the element's height, never below 0.032 next to 0.030 numbers. Xbox names: RS / LS / View / Menu [O]. Every glyph and keycap resolves from the live bindings (§4).
- **HOLD** is reserved for the hold input, written before the glyph [O]. Cargo reads "CARGO 12/40", the maneuver "FORMATION".
- **Cues**: one flash = one meaning, under 3 Hz, never colour alone, each with an off switch [O]. Churning readouts update at 4 Hz at most.
- **Continuous motion** [O]. Opacity keeps today's single ease (tau 0.25 s panels, 0.12 s card, 0.15 s alert) [C]. A shape change is a continuous rectangle morph: the plate lerps with the same ease, inner rows lerp to their places, extra rows fade in once the rectangle reaches 80 %. Never a crossfade between layouts [D].
- **Hysteresis** [D]: opacity follows combat at once and lingers 2 s; shapes (the fight forms) linger 6 s with distance hysteresis (enter 2 / 4 km, leave 2.4 / 4.5 km). A farther hostile target grows only the card.
- **Combat entry also on damage** [D]: a real hull or shield hit (the hit-flash test) or an inbound missile, so capital guns, mines and long-range missiles cannot leave a quiet preset in calm.
- **Text**: floor 0.024 H / 14 px [C]; unplated text ≥ 0.026 with the navy outline [D]. Row height = max(k × design row, clamped text + 0.008), so plates at k < 1 grow instead of overflowing (5.14) [C]. Arc thickness = max(0.0065, 0.007 × k) [R, Armored Core VI].

### 1.3 Sightlines and the centre stack
- **Zones are data** in UI_SPEC.json, derived from rules; a plate inside a zone fails the lint.
- **Exemptions by name**: the subtitles' dialogue panel and name-tab body; the alert card (≤ 3 s); a peek; the K&M click-pinned contact list (S2 only). The same for a list the player opened on the pad, by input parity: S2 only, at most 10 rows; the automatic combat list keeps the rule (the driver's decision of 6 Nov 2026, for the owner to review).
- **Native S1 occupants**: the bracket label and the objective banner. They are no longer debt: §1.7 makes them the only home of what they show (the bracket label's part replaced 6 Nov 2026: §1.7 pair 1).
- **Centre stack, every preset** [D]:
  - Inside r 0.045 only native markers. The speed stood at (+0.058, 0.030) [C] until 0.4.0, which moves it under the meter (R + 0.0285, built on `build-040`), and the lower centre's start with it (`UI_HUD2_INPUTS.md` 7b).
  - The 6 o'clock sector (±55°) is the ship's; the ship starts at 0.667-0.685 H in the release captures [C]. Where chevrons cross the reticle lane or the throttle word, the chevrons draw on top and the text dims to 30 %. The hit flash draws above the throttle arc.
  - **Ship keep-out** for new text: x 0.40-0.60 W, y 0.63-0.92 (WARN; frames of a light fighter, a Defender, a freighter).
    Since the owner's 31 Oct 2026 rule (§7, `UI_HUD2.md` 5.23) the stricter **lower centre** holds: in those columns, from
    the ring's lowest drawn edge + 0.01 H to the bottom, nothing HUD 2 draws (lint zone `bottom`, FAIL).
  - **RS-click pick radius = the radius the preset draws** (`RingRadius()`, also TARGET AHEAD).

### 1.4 The functional floor (every preset, pad and K&M)

| Gap | Shared fix [D] |
|---|---|
| **Capital fight: no sub-target** (blocker) | A card row "PART Heavy Turret 2 · bar · 40 %" while a part is selected (+0x534 [C, FREELANCER_RE 979]), else "5 PARTS". **Pad: a wheel action, no default button** [O, answer 3]: a contextual **TARGET PART** entry, present only while the target has parts (else the wheel keeps its 8 entries [C, wheel.cpp]); each pick sends the game's USER_NEXT_SUBTARGET (wraps). K&M: NEXT / PREVIOUS SUB-TARGET (Ctrl+S / Shift+S, keymap.ini [C]), a click on the row. Capital targets always show their wireframe; a fitted torpedo gets its own loadout row. |
| Protected ship tiny, no warning | First row of the escort list (yellow diamond, hull %); alert "PROTECT <name> 50 % / 25 %"; threat rows attacking it carry a diamond. |
| Mission timer lost in merged lines | Its own field on the headline line, "TIME 1:42" (amber under 30 s plus the word); never merged or paused. |
| Objective change missed mid-fight | The **native banner** announces it (§1.7 pair 2); the headline takes the new text when the banner ends, plus a pill TARGET OBJECTIVE for 6 s (first MissionObjectIds entry [C]). No NEW OBJECTIVE alert (it repeated the banner). |
| Autopilot hand-back unnoticed | `hud_ap_cue` (on in every preset): reticle-lane state word "YOU HAVE CONTROL · <reason>" 2 s, "AUTOPILOT" 1 s on engage. |
| Turret / rear view | TURRET VIEW chip and EXIT TURRET (an answer); "REAR VIEW" state word while latched. |
| Pad combat with no target | The threat block stands alone in the card slot, LB hints merged in, 2 rows + "+n", sorted attacking-you first, then distance; a 0.020 bearing arrow only on rows the native off-screen arrow does not already point at (§1.7 pair 3). |
| Abort lost under fire | The combat context line by priority: hand-back / EXIT TURRET > the running maneuver's abort > REVERSE > TURRET. |
| Looting: will it fit? | Reticle lane "[X] TRACTOR 3 · ROOM 2" (ROOM amber when short); "CARGO FULL · 3 ITEMS" amber, no glyph. |
| Lane / cruise disruption | "LANE DISRUPTED" from the lane's result code, answer CRUISE with its bound glyph. **Unverified** until tested. |

**Fight-form completeness rule**: everything a fight needs lives in the fight form (hold room when loot is in reach, all launcher and torpedo ammo, CM / battery / nanobot counts, the sub-target row). The peek works in combat (§1.6) but is never required there.

### 1.5 Alerts
Built differently (0.3.0, the owner, GitHub issue 21; UI_HUD2.md 5.40): the dangers stand beside the ring in a stack per side, every danger showing, so the queue below is not built for them and no second line shares a plate.

- A level-checked **priority queue** at 10 Hz [D]: HULL CRITICAL > MISSILE INCOMING > SHIELD FAILING > PROTECT > CRUISE / LANE DISRUPTED > CARGO FULL. A lower alert never replaces a higher one; a second active alert is one 0.034 line in the same plate. Fixes today's last-wins slot (hud2.cpp 359-365) [C].
- 3 s; a lit slot stays lit while its condition holds [C]. While MISSILE INCOMING holds, its FLARE answer also shows in the reticle lane [R, GAG]. This is the one deliberate redundancy: a second **channel** for a critical state (§1.7).
- **Letterbox**: only HULL CRITICAL and MISSILE INCOMING show, in the lower bar; up to 2 others queue and are re-validated after (new code).
- **Wake**: an alert wakes the panel that holds its answer. **Budget**: the alert counts its full 0.50 × 0.05 = 0.025 H².

### 1.6 Prompts and reveal

**Three kinds of prompt text** [D on R: contextual prompts are the modern norm - the Bonn 2026 study of 30 GOTY nominees, [R, memory]: the page sits behind a bot wall in round 3; remaps must update hints, https://devdocs.xbox.com/gaming/accessibility/xbox-accessibility-guidelines/107]:

| Kind | What | Examples | Decays? | Where |
|---|---|---|---|---|
| **State word** | what is happening now | CRUISE 3.2, AUTOPILOT, REAR VIEW, TURRET VIEW, KILL, a latched AFTERBURNER, LOOT n | never; no glyph | at the instrument it describes |
| **Answer** | the input that resolves a live situation | FLARE on MISSILE INCOMING, TAKE CONTROL, EXIT TURRET, a maneuver's ABORT while combat holds, CRUISE after LANE DISRUPTED, the LB hints [O] | never, while the situation lasts | at the eye: the reticle lane or the alert plate |
| **Teaching glyph** | "this button does that here" | DOCK, LANE, JUMP, TRACTOR, CRUISE in calm, FORMATION, GOAL PANEL, TARGET PART | the only kind that retires | the context line, or a badge on the element it acts on |

- "Permanent" no longer means a fixed spot: it means "never retires". Only Guided keeps teaching glyphs for good (Lost Judgment's standing legend = Guided's peek [O, pad_hud_informative]).
- **Five lanes**, nothing else carries prompts: the reticle lane (answers, cruise, tractor); the context line (**since the owner's decision 4, 31 Oct 2026 (§7): two lanes hugging the bottom-left and bottom-right beside the ship panel and the loadout, the bottom centre x 0.40-0.60 W clear - the ship's movement and control left, what you fire / use / target right; the lint's zone `bottom`; `UI_HUD2.md` 5.20**; teaching); the target lane (LB hints with no target, "RS NEXT · ×2 CLEAR" with one); alert answers; the peek legend (Guided). Prompts not yet learned outrank learned ones.
- **Retiring** [R, Hypertrack, low; Black Flag Resynced graduated removal, high]: after n real uses while showing, per **label @ context @ device family** ("DOCK@calm@pad"), never on a timer. **Returns** on a real stall (per prompt, e.g. a dock target in range, < 20 m/s for 10 s, no dock started; lanes, queues, formation, jump, autopilot, cruise, letterbox, dialogue never count), a device switch (first in a session or after ≥ 60 s on the other device, held 1 s), a rebind of that action, 14 days without play, a peek, "Show all hints again". Data prompts keep their number (LOOT n). Counts in a small Install\ file, written on dock, undock and exit only [O, "keep it light"]. Numbers are [D] settings; returns are logged for tuning.

**Reveal: how a quiet preset shows what it holds back.** Shipped patterns [R]: HZD's Dynamic HUD wakes elements on progression or interaction, and a touchpad tap shows the HUD "at any time" (https://accessible.games/accessible-player-experiences/access-patterns/personal-interface/); HFW: "Show HUD: swipe up on touchpad" (https://outsidergaming.com/horizon-forbidden-west-controls-guide-for-ps4-ps5-and-gameplay-tips/), Dynamic / Always On / Custom [R, memory]; GoW Ragnarök sets **each** element On (gameplay decides) / Swipe / Off, "HUD Toggle" being one of four swipe shortcuts (https://www.playstation.com/en-us/games/god-of-war-ragnarok/accessibility/); Everspace 2: "H - Toggle HUD visibility" on K&M (https://everspace.fandom.com/wiki/Controls_(ES2)); Destiny 2: only HUD Opacity Full / High / Low / Off (https://help.bungie.net/hc/en-us/articles/360049200911), its idle fade [R, memory]. Counter-lesson: Far Cry 2's fading outside the player's control was criticised; given the choice, players want more information (https://www.gamedeveloper.com/design/game-ui-discoveries-what-players-want). Sony's PC ports map touchpad actions to "other buttons or button combinations" per control preset (https://support.nixxes.com/hc/en-us/articles/49254608715027-How-do-touchpad-controls-work-with-controllers-that-do-not-have-a-touchpad); the exact Xbox / keyboard binds were not found (PCGamingWiki and the guides with tables refused the fetch).

Three ways back, from automatic to manual, in every preset [D]:
1. **Context auto-reveal of the changed element only** (never the whole HUD): it wakes for `hud_wake_s` (4 s), then fades with the panel ease. Triggers: a hit → the arcs (and the hold-below-nominal rules); credits or cargo change → the hold strip; an ammo / count change or a consumable press → that slot; a weapon-group switch → the group row; a new target → the card; a maneuver starts or ends → the chip; new objective → the banner, then the headline; a device switch → the teaching glyphs of the context; space entry (letterbox over, HUD at full alpha) → one auto-peek, never in combat.
2. **Manual peek**: one input shows everything the preset holds back, in place, for 3 s (no new panel shape, never a plate in S1); a second peek inside the window pins it until the next (a toggle, also for reaching K&M click rows). Inputs, after checking the live tables (`ui_meta.h` kBind_*) [C]:
   - **Pad: View double-tap**, every controller preset. FREELANCER's tap and hold slots are all taken (LS click engine kill, RS click target / hold rear / double clear, View hub / hold autopilot); ACE uses LS click too; only ASSISTED leaves LS click and LB free, so neither can be shared. Double slots are free on every button but RS click, and View has the same jobs in all three presets. Cost: a button with a double delivers its tap when the 0.3 s window closes (presets.cpp 214-283), so **the hub opens 0.3 s later** (Q1). DualSense: Create (FLvibed's PlayStation View glyph [C]); Sony's touchpad has no Xbox equivalent.
   - **Pad, no timing needed**: a SHOW HUD wheel entry (accessibility; also opens with a 0.3 s delay when the wheel opens in calm, Standard only).
   - **K&M: the game's own MINIMIZE HUD command** (USER_MINIMIZE_HUD, "Minimizes the HUD.", Backspace by default, rebindable in the game's controls [C, keymap.ini, resources ids 1440 / 3235]); its native effect on the panels HUD 2 hides must be checked. DISPLAY CURRENT OBJECTIVE (N) re-shows the native banner and wakes the headline. Hover-peek with the free cursor only (mouse flight off), 0.35 s dwell, never while throttle or fire input is active.
   - Paddles (Elite / Edge): optional, by mapping one to View in the Accessories app; never assumed [O, standard pad first].
3. **Always-full off-ramp**: `hud_reveal` 0 hides nothing by context (the compact calm forms and opacities stay); 1 context + peek (default); 2 peek only (nothing wakes by itself except alerts and answers). GoW's per-element On / Swipe / Off is the later Custom extension.

### 1.7 No duplication

The rule [O, answer 4]: **one value, one place at a time.** A value may move (handover), never show in two visible places at once; the only exception is a second channel for a critical state (the FLARE mirror, the hull flash beside HULL CRITICAL) [R, memory: NASA alerting review]. **Two more, written by the owner's rulings of 6 Nov 2026 [O]:** the target's name and distance stand on the card and at the bracket, the distance one surface value in both (`UI_HUD2_CRITIQUE.md` R13; the bracket's place is HUD 2's form at the ship, pair 1, and the owner kept both on the card); the selected group stands on the weapon cue and in the loadout, as the cue's requester asked (R3, `UI_HUD2_GROUP_CUE.md`). The lint declares both. Placement by information type [R]: an in-world number beat HUD bars and icons for ammo (35 % fewer shots after empty, preferred by 70 %: https://www.yorku.ca/mack/ieee_gem2015.html); a world-space navigation line beat the compass, while **own health read best as a stable HUD number**; the information type matters more than the display category (https://www.yorku.ca/mack/ec2018.html). Diegetic precedents: The Division labels streets and objects in the world (https://www.hudsandguis.com/2013/06/26/tom-clancys-the-division-interface-design/; GDC 2017: https://gdcvault.com/play/1024026/Lessons-Learned-Creating-UI-for); Dead Space's RIG carries health on the suit (https://gamedeveloper.com/design/video-designing-i-dead-space-i-s-immersive-user-interface), but its holographic map failed at navigation (gamedeveloper, above). So: **identity and distance at the thing, exact own-ship values at a fixed place, events where they happen.**

| Pair | Today / draft 1 | Decision [D] | Risk and check |
|---|---|---|---|
| 1 Target card vs the native bracket label + bars | the card repeated name, distance and bars next to the bracket's own | **REPLACED 6 Nov 2026 (the owner): HUD 2's targeting display takes the native bracket, label and bars' place (`UI_HUD2_SKIN.md` 6.10). Name and distance stand on the card and at the ship, the distance in the game's surface value (R13).** Before: **A (recommended)**: the bracket is the only home of name, distance and bars **while the target is on screen**; the card keeps what the bracket lacks (class, faction and reputation, scan, lock, PART, threats). Off screen (native arrow only), name and distance hand over to the card's top line by a morph, and back. | No hook. Check the native label's text height at 4K and 720p against the 0.024 floor (full-res crops). If it fails, **B**: hide it and draw HUD 2's own at the bracket - a hook in `Targetable_Objects`' draw (vt 0x5D9454 [C]). B's checks: a signed symbol, a chaining detour, fail closed (label stays native), frames hook on / off on 0.7.1 and upstream, the FLSharp sweep and the MOD_COMPATIBILITY.md plugins that draw indicators, its own off switch. **C** (hide native, keep the card): rejected, worst eye travel. |
| 2 Nav headline vs the native objective banner | the headline expanded 6 s and a NEW OBJECTIVE alert fired beside the banner's same text | The banner (placed by the subtitles [C]) owns the **event**, the headline is the **standing** home: it keeps the old text at 30 % while the banner shows, then morphs to the new one. It adds only what the banner lacks (distance / ETA, TIME, lane state). NEW OBJECTIVE alert dropped; the TARGET OBJECTIVE pill (an answer) stays. | Check the banner's duration, and that it shows in combat and after a letterbox under HUD 2; if it can be missed, the 6 s expansion returns. |
| 3 Off-screen direction | draft 1 added threat-row bearing arrows; an ellipse was planned | The native off-screen arrows stay the only direction cue for the target and the objective; **no ellipse**. A threat row gets a bearing arrow only when no native arrow points at it (a non-selected threat). A world-space route line (the York navigation result) is a later experiment, not in a preset. | None (no native change). |
| 4 Own ship | ring arcs and the ship panel | Exact values only in the ring / arcs (fixed place, numbers below 40 %); the ship panel never shows own bars. Hit **direction** is an event drawn ship-mounted: an arc segment at the hit's bearing, outside the ship keep-out (Dead Space as precedent; benefit [D]). | None. |
| 5 Ammo | loadout rows and reticle cues | One home per count. Focus moves the active launcher's count to the aim point ("MSL 12", the in-world-number result); the others keep it in the loadout. The FLARE mirror carries no count. | None. |
| 6 Speedrun's flank | flank and bracket both showed TGT distance | TGT distance only while the target is off screen; on screen the closing rate. | None. |

### 1.8 Shared K&M and aspect rules
- **K&M** keeps the click-pinned contact list (header, 5 filters, ≤ 10 rows, inside S2) [C]; a contact-list key mirrors the wheel action; the clickable maneuver row stays reachable (a row, or a dim "MANEUVERS" tag). Keycaps from the game's keymap replace glyphs; retire counts per device family; peek = MINIMIZE HUD (§1.6).
- **4:3 / 5:4**: k = 0.8, text clamped at the floor, rows per 1.2, the built HOLD cascade [C]; S1's half-width = the gap from the centre to the card's left edge - 0.01. 16:10: k = 0.9.
- **21:9 / 32:9**: `hud_frame` 1 anchors the corners to a centred 16:9 frame at cx ± 0.80 H [O, principle 1]; the frame and `chatFree` go on the HUD bus (no HostApi bump), `chatFree.y1` pinned per preset, the comm feed growing upward from the bottom.

## 2 The four presets

### 2.1 Standard: "the data ring" (the default)
**Identity.** Today's HUD 2, refined. A segmented ring carries the ship's state: shield left, hull right, power and throttle below, speed inside. The corners have a compact calm form and a designed fight form: a cockpit that breathes [R, HZD dynamic HUD; Black Flag Resynced Default].

**Reticle**: ring r 0.10, arcs 0.007, numbers below 40 % [C, 5d52919]. An offline shield's dim track fills back as it rebuilds (red = low, filling = rebuilding) [D]. The ring holds 100 % in calm while the shield recharges or hull < 35 % [D, HUD_CUES]. During a cruise charge, speed and charge seconds show (today).

**Cruise prompt [D, answer 2]** (superseded 31 Oct 2026, section 7 "Yes, move it": the prompt is the left lane's, the reticle lane keeps only the glyph-less state word; `UI_HUD2.md` 5.21): stays in the **reticle lane** (y 0.692), Standard's learned spot right under the eye. The state word (CRUISE CHARGING 3.2 / CRUISE) always shows; the glyph is a teaching glyph (retires after 3) and returns as an answer after LANE / CRUISE DISRUPTED.

**Calm**:

| Element | Position | Size H |
|---|---|---|
| Nav (free text) | 0.089, 0.05 | w ≤ 0.38, h ≤ 0.30 (K&M 0.32); headline 0.032 |
| Card, compact | x 1.329-1.689, y 0.05- | 0.36 × 0.085 with the target on screen (class · faction · rep, PART); 0.36 × 0.12 off screen (+ name · distance) |
| LB hints | the card's top line, right-aligned | glyph 0.032, 45 % |
| Ring / speed | centre | r 0.10, 40 % / 60 % |
| Reticle lane | y 0.692 | cruise row, [X] TRACTOR n · ROOM |
| Comm feed | left rail x ≤ 0.469, bottom 0.76 | lines 0.034 |
| Context line | y 0.915 | ≤ 3 pills, 0.044 tall |
| Ship, compact | x 0.089-0.389, y 0.83-0.95 | 0.30 × 0.12, thumbnail 0.10 (formation 0.16) |
| Loadout, compact | x 1.329-1.689, y 0.82-0.95 | 0.36 × 0.13, cross 0.11 |

Zones: S1 x 0.469-1.309, y 0.20-0.80 (0.20 allows the dialogue panel). S2, the right rail, x 1.309-1.689, y 0.40-0.78: empty.

**Combat** (morph, §1.2):
- **Card**: full form 0.36 × 0.165 on screen / 0.215 off screen, same left edge, growing down only.
- **Threat block** under the card, 0.36 × 0.11: a 0.044 header row (target lane + filter), 2 rows at 0.032 + "+n". K&M 3 rows.
- **Ship**: fight form 0.36 × 0.15 (view, escorts with the protected ship first). **Loadout**: 0.48 × 0.15 (cross, RT groups, RB launcher, torpedo row).
- **Nav**: 1 line + the timer field. **Context line**: ≤ 2 pills by priority.

**Other contexts**: lane: chip + bar + ETA, card at 75 %, ABORT LANE. Dock: chip + queue / bay, ABORT DOCK. Formation: member list. Jump: countdown on the chip (letterbox keeps it). Autopilot: travel counts as calm, TAKE CONTROL pinned. Comms: the event part of the nav line yields to a comm line; TIME and FORMATION stay.

**Prompts and reveal**: ≤ 3 pills calm, ≤ 2 combat; cap 6 pairs combat, 5 otherwise, plus answers. Retire after 3. `hud_reveal` 1; peek on; the wheel's opening (0.3 s) also peeks in calm and travel.

**Budgets** (recomputed for §1.7 pair 1):
- Calm: 5.9 % no target; with one 7.6 % on screen, 8.3 % off screen. K&M (header folded into the card): 6.2 / 7.9 / 8.6 %.
- Combat: steady 0.240 H² = 13.5 % (target on screen); peak 15.9 % (off screen + alert; ≤ 16). K&M peak 16.9 % (≤ 17).
- Sub-target row: +0.6 % while a part is shown.

### 2.2 Focus: "glass and edges"
**Identity.** In calm the screen belongs to space: no ring, two thin split arcs, a bare centre. Instruments sit at the rim, where peripheral vision reads them [R, Squadrons Instruments Only; Ghost of Tsushima; Black Flag Minimal]. Nothing nags; one input brings everything back.

**Reticle**:
- **Arcs**: shield left, hull right, 50° each at r 0.12 [D]; the radius from data (log the lead pip's screen radius in `hud2 sim combat` fights; keep the arcs off where it dwells). Pick radius 0.12.
- **Under-arc band** at +0.14 (0.63-0.65 H, clear of the ship): the throttle word (BURN / KILL / REV) and the speed; the cruise state; "LOOT n"; in combat the active launcher's count "MSL 12" (§1.7 pair 5).
- **Hit direction**: a segment on the arc circle at the hit bearing. **Chevrons** at 0.15 / 0.17 (§1.3).
- **Visibility**: the hull arc stays at 40 % whenever hull < 100 % (hull does not regenerate); the shield arc holds while recharging; the power arc steady through combat, in calm only during a cruise charge.
- **Speed shows** continuously during protect, formation, dock and goto, and while the throttle lever rests between 1 and 99 % (K&M, ACE: §4); otherwise on throttle-state changes or a > 20 % change held 0.5 s, then 3 s; with a 3-segment throttle tick.

**Cruise prompt [D, answer 2]**: moves into the **under-arc band**, 45 % in calm. Focus has no reticle lane; the band is its only centre text, so the cruise state lives where the speed lives. The glyph retires after 2 uses; the state word stays.

**Calm**:

| Element | Position | Size H |
|---|---|---|
| Nav: headline 1 line + timer | 0.089, 0.05 | w ≤ 0.48 (4:3: 0.36), text 0.030; h about 0.08 |
| LB hints / compact card | top-right | 0.026 text / card 0.34 × 0.065 on screen, 0.095 off screen |
| Hold strip (wakes 4 s) | 0.089, 0.93 | 0.026 |
| Fire strip | bottom-right | hidden; wakes on a change and on any lit alert slot |
| Context line | y 0.915 | ≤ 1 pill |
| Comm feed | left, `chatFree` right edge 0.30 W | — |

Zones: the clear field x 0.30-0.70 W, y from the published top strip's bottom (`topFree`) to 0.82. K&M: a dim "MANEUVERS" tag.

**Combat ("edge combat")**: arcs at full, numbers below 40 %. Top-right: card + 2 threat rows, plate 0.34 × 0.127 on screen / 0.167 off screen, target lane on the first row. Bottom-right: fire strip 0.36 × 0.14 (cross 0.12, slots 0.04, icons 0.02; the active group as numeral + short name; launcher name and lock, torpedo). Bottom-left: escorts as free text, 0.042 rows with inline bars; the hold line appended 4 s on a pickup. Context line ≤ 2 pills.

**Other contexts**: lane: chip + bar + ETA, the card hides unless hostile or a mission object. Dock, formation, jump: chip lines as Standard. Autopilot: TAKE CONTROL until learned, then 3 s at each engage. Letterbox: the chip, arcs only below nominal; only what changed wakes.

**Prompts and reveal**: ≤ 3 pairs plus answers. Retire after 2. `hud_reveal` 1, peek on.

**Budgets**: calm 1.6 % with a target on screen, 2.2 % off screen (≤ 3). Combat steady 0.106 H² = 6.0 %, peak 8.2 % (≤ 7 / 9). K&M (4 rows, 0.40 strip): 7.6 / 9.8 % (≤ 8.5 / 10).

**Built (31 Oct 2026; run in game, the owner's verdicts on its frames below)** - `hud_preset 1`, still the hidden advanced row; as built, the tests and the reasons: `UI_HUD2.md` 5.18. Every number above that the spec carries comes from `UI_SPEC.json` (the layout core's `FocusCorners`, the reveal `hud2layout::Reveal`); what this section left open was decided by the build as the stated default or the least change - **the owner's decisions, pending the frames**: (1) arcs r 0.12 until the lead pip's dwell radius is measured; (2) the clear field's top = topFree's bottom 0.30 H, checked by the unit test, not a live lint zone; (3) no new scrim (the nav's soft scrim, the kit's outline elsewhere) until the contrast frames; (4) the card always in its off-screen form (name + distance; no bars) until step 6's on / off-screen state - the budget case is "peak"; (5) a flat icon in the card, no 3D views in Focus; (6) the target lane on the card's third row; (7) K&M keeps Standard's contact-list block and a click row: 4.2 % calm, 11.2 % combat + alert (over 3 / 10: WARN); (8) no budget-drop rule (wakes and the peek are transient); (9) the bottom-left 0.32 × 0.17 ("CARGO 12/40 · CREDITS 4 250", escort rows above it); (10) the recharging shield arc at the hull's 40 %; (11) the power / charge arc inside the circle (r − 0.012) to keep the band 0.01 H clear; (12) the band on fixed anchors, BOOST (not BURN), the tick + % whenever the lever sits partial (the lint's analog rule); (13) the cruise glyph until the retire store, `CRUISE` at 45 % in calm only; (14) the lean nav keeps the dock / lane / jump event with its chip and the 5 s arrival line, the cruise-charge chip moves to the band; (15) Focus applies its bundle's reveal values itself (not `hud_fade`) until step 4 writes the bundles; (16) the fire strip shows the active group only, no torpedo row, the launcher's count there only out of a fight. The pad / K&M peek inputs, the retire counts and the on / off-screen hand-over stay steps 6-7; debug `hud2 peek` until then. **The owner's verdicts on the frames (31 Oct 2026, §7; built: `UI_HUD2.md` 5.19):** the calm arcs per meter (below full 40 %, full hidden, no hit wake); no overlap inside the card (the pad card + 0.012 for its lane row: calm 0.107 / fight 0.179; the K&M click row 0.040); no hold strip (the bottom-left = the escort rows, 0.32 x 0.12); the cruise prompt in the context line (the band keeps CRUISE 3.2 / CRUISE ON); the fire strip shows every group, the guns in no group and the launcher at 0.024 text; the other decisions kept. **The second round (§7, `UI_HUD2.md` 5.20), Standard too:** long names scroll (a marquee); the fire strip's base 0.22 with the cross stacked on its right end (0.16 x 0.14); the prompts in the bottom-corner lanes.

### 2.3 Speedrun: "telemetry"
**Identity.** A flat numeric dash read like a racing car's [D; LiveSplit / racing deltas as inspiration]: tabular digits, flat icons (capital targets keep their wireframe), hardly any prose. Its signature: **two flanks beside the ring - how far on the left, how long and how late on the right.**

**Reticle**:
- **Ring**: r 0.10, 60 % in calm; numbers "when not full" (hud_numbers 2). **Speed** at +0.058, tabular.
- **Cruise prompt [D, answer 2]**: **inside the ring** at +0.083 as state (flat cruise icon + "3.2"). Speedrun starts learned, so its glyph appears only on a stall or as the answer after a disruption: the runner's eye stays at the reticle and the ring interior is otherwise empty.
- **Flanks** at x ±0.20-0.32, free text. **Left**: OBJ / DOCK / LANE distance; TGT distance off screen, else the closing rate (§1.7); "ESC 42 %" while a protected ship is under 50 %. **Right**: ETA over the delta. Over a bracket or the lead pip a flank fades to 25 % (0.5 s hold), never moves; 4 Hz; ETA from a 1 s EMA closing speed, whole seconds above 10 s, "--" when > 90° off. Both collapse in a jump and in formation without a destination; with no hostile target they keep OBJ even in combat.
- No 12 o'clock pip. Ink "-" = ahead, amber "+" = behind; green and red only inside the timer strip.

**Calm**:

| Element | Position | Size H |
|---|---|---|
| Nav: headline + standing place line | 0.089, 0.05 | w ≤ 0.45; 0.028 / 0.024 |
| Timer strip (gradient scrim, not boxed) | top-right | run time 0.034; split name; line 3 the delta to PB or **PAR** - par anonymised, no runner names [O, answer 6] |
| LB hints | under the strip | §1.1 |
| Loadout, compact | x 1.429-1.689, y 0.76-0.95 | 0.26 × 0.19 |
| Bottom-left | empty; escort rows / 4 s hold line | free text |
| Context line | one stall pill | 0.026 |

`chatFree.y1` pinned at 0.82. K&M: an always-visible list-toggle icon (0.035, not boxed) and the contact-list key.

**Combat**: card flat 0.36 × 0.115 on screen / 0.15 off screen, bars beside the name only off screen, the target lane at the row's right end. Threat rows as free text under the card (2 pad, 4 K&M). Loadout 0.40 × 0.17 (launcher ammo here). No standing prompts. The mission timer only on the strip.

**Other contexts**: lane, dock, autopilot: the flanks carry the goal ("QUEUE 2" while queued); the DOCK stall is suppressed while a request is pending, queued or refused. Letterbox: run time + delta in the top bar.

**Prompts and reveal**: ≤ 2 (the tractor slot + 1 stall pill) plus answers and LB hints. Starts learned; stall time 3 s; TAKE CONTROL once per session. `hud_reveal` 1, peek on, no auto-peek on space entry after the first in a session.

**Budgets**: calm 2.8 % no target; with one 5.1 % on screen, 5.8 % off screen (≤ 3 / 6). Combat 0.109 H² = 6.1 % on screen, peak 8.3 % (≤ 10). K&M 6.6 %.

### 2.4 Guided: "the annotated cockpit"
**Identity.** A flight instructor in the co-pilot seat [R, Black Flag Resynced Default; Lost Judgment legend, your reference]: every instrument labelled, every value with a unit, two state rows saying what is happening and why, prompts for as long as their action is available, text about 17 % larger, the ring's upper half bare. Opt-in for the default player; **the first-run "guided setup" choice** and the suggestion for the ASSISTED controller preset (§4) [O, answer 5]. `ap_hints` 1 [O].

**Reticle**:
- Ring r 0.10, arcs 0.0075. **Arc labels** "SHIELD 82" / "HULL 100" at 125° / 55°, r 0.12, baseline at dy ≥ +0.10; numbers full, words 70 %; ≤ 25 % turns red + LOW.
- "82 m/s" at +0.058; the throttle word spelled out (AFTERBURNER / ENGINE KILLED / REVERSE) at +0.146; "THROTTLE 64 %" beside it while the lever rests between 1 and 99 %. "POWER LOW" below 25 % (2 s hold).
- **Cruise prompt [D, answer 2]**: the **reticle lane** (y +0.19) with words: "[Y] CRUISE" whenever cruise is possible, "[Y] CANCEL CRUISE · CHARGING" while charging; never retires (Guided's rule). Also "[X] TRACTOR 3 ITEMS · ROOM 2", and in combat "HOLD [RS] LOOK BACK" while a threat is behind.

**Calm** (rows sized from the 0.028 text):

| Element | Position | Size H |
|---|---|---|
| Nav: chip; headline + TIME; maneuver row; reason row "FREE FLIGHT · New York · Badlands · 3 HOSTILES 6.2 km"; K&M maneuver buttons | 0.089, 0.05 | w ≤ 0.50, h ≤ 0.24 (K&M 0.30); sentences ≤ 40 characters |
| Card, labelled | top-right | 0.42 × 0.15 on screen (CLASS, FACTION · REPUTATION in words, PART, "HOLD [LB] HAIL · SCAN · MORE"); 0.42 × 0.21 off screen (+ NAME, DISTANCE). CLEAR as a corner badge |
| LB hints | 2 right-aligned lines inside 0.42 | 0.028 / 0.036 |
| Ship strip | 0.089, bottom 0.95 | 0.34 × 0.12: ship, CARGO, CREDITS |
| Loadout, 2×2 grid (glyph + word + count, the K&M parity shape) | bottom-right | 0.40 × 0.15 |
| Context line | y 0.915 | ≤ 4 pills, measured widths |
| Comm feed | left, y 0.60-0.78 | — |

Zones: Z1 core r 0.085; Z2 horizon band |dy| ≤ 0.08 for 0.15 ≤ |dx| ≤ 0.62; Z3 forward-upper band; Z4 dialogue ≥ 0.42 wide. Calm opacity: ring 65 %, panels 70 %.

**Combat**: card merged 0.42 × 0.24 on screen / 0.30 off screen, header "THREATS 3 · [RS] NEXT", 2 reserved row slots (the edge never jumps), rows "ATTACKING YOU". Loadout grid + weapons 0.48 × 0.17. Wingmen 0.34 × 0.15, protected ship first. Reason row "COMBAT · hostile targeting you, 3.2 km"; the maneuver row and its exit pill stay. Pills ≤ 2 (REVERSE, HOLD [X] TURRET; an abort outranks TURRET).

**Other contexts**: lane "TRADE LANE → <ring>" + bar + ETA. Arrival "ARRIVED: <system · zone>" 3 s. Autopilot "AUTOPILOT FLYING · <mode> · waiting: <reason>", after a hand-back "YOU HAVE CONTROL · <reason>" 4 s. Turret "TURRET VIEW · RS aims · the ship holds course". A device switch changes only the glyphs.

**Prompts and reveal**: up to 8 pairs plus answers; nothing retires (context guards against nagging [R, gamedev.net; GAG]). `hud_reveal` 0 (nothing hidden by context). Peek "ALL CONTROLS NOW": a right column 0.42 wide from the card's bottom to the loadout's top, ≤ 8 rows, the classic button_legend renderer; on View double-tap, MINIMIZE HUD and a wheel entry.

**Budgets**: calm panels 5.7 % no target; 9.2 % with one on screen, 10.6 % off screen (≤ 6 / 11); pills up to +1.4 %. Combat 0.234 H² = 13.2 % on screen, worst 17.7 % (≤ 15 / 18). K&M (3 rows) 14.3 % (≤ 16.5). Text floor 0.028 (0.026 at 4:3), following `hud_text_scale`; text_size × hud_text_scale clamped at the fit ceiling (~1.18) [C].

## 3 Comparison

| | Standard | Focus | Speedrun | Guided |
|---|---|---|---|---|
| Identity | data ring, breathing corners | bare centre, edge instruments | numeric flanks, flat telemetry | labelled instruments, sentences |
| Centre data | speed + arcs, numbers when low | state band under the arcs (speed, cruise, MSL count) | speed, cruise in ring; flanks distance / time + delta | labelled arcs, m/s, spelled states |
| Cruise prompt | reticle lane; glyph retires (3) | under-arc band; retires (2) | in-ring state; starts learned | reticle lane with words; never retires |
| Teaching glyphs | ≤ 3 calm / 2 combat, retire after 3 | ≤ 1 / 2, after 2 | ~0, start learned | ≤ 4 / 2, never |
| Reveal (`hud_reveal`) | context + peek | context + peek | context + peek | always full |
| Peek shows | the held-back lines | the held-back lines | lines + K&M rows | all-controls legend |
| Density | medium | low | medium, all numeric | high |
| Boxed calm (no target / on / off screen) | 5.9 / 7.6 / 8.3 % | ~0 / 1.6 / 2.2 % | 2.8 / 5.1 / 5.8 % | 5.7 / 9.2 / 10.6 % |
| Boxed combat (steady / peak) | 13.5 / 15.9 % | 6.0 / 8.2 % | 6.1 / 8.3 % | 13.2 / 17.7 % |
| Pick radius | 0.10 | 0.12 | 0.10 | 0.10 |

Today: 9.7 % calm, 18.6 % combat. Steady = target on screen; peak = target off screen + an alert.

## 4 HUD preset × controller preset

Controller presets [C, ui_meta.h kBind_*, presets.h]: **FREELANCER** (default; LS throttle lever, LT afterburner, LS click engine kill, LB picker / hold wheel), **ACE** (LS steers, RT / LT throttle levers with afterburner / reverse at full pull, RS strafes, A fires, cruise D-pad left, LB countermeasure; **no wheel bound**), **ASSISTED** (the autopilot flies, RS aims, B hands back, X goal panel, Y hold wheel), **CUSTOM** (= FREELANCER until the Bindings editor); options `weapon_groups` 0 / 1 / 2, `holds_as_toggles`; planned: Southpaw and per-action hold / toggle (`docs\research\controller_ui.md`). The device switches live.

**Rules** [O, 29 Oct; D for the details]:
1. **Never block a combination** (player choice, accessibility; XAG 107: "always provide alternate inputs with equivalent functionality"). The HUD preset decides where and how; the controller setup decides **what must be there**.
2. **Glyphs from the live table**, per frame (preset + weapon_groups + toggles + stick layout) [R, XAG 107: "remapped bindings update hints, tips, tutorials, and controller diagrams"]. A moved action moves its glyph; layouts never mirror.
3. **Standard pad first**: every required input exists on a plain Xbox pad, the DualSense (Create = View) and K&M [O].
4. **Adaptation**: a needed reading or prompt never disappears behind a preset's quietness. Each setup requires:

| Controller setup | Requires in every HUD preset |
|---|---|
| FREELANCER / CUSTOM | the throttle word on change (the lever is used binary: BINDINGS 2), KILL while engine kill holds |
| ACE | a continuous throttle reading (tick + %) while the lever rests between 1 and 99 % (the trigger levers keep their value); CM's answer shows LB, cruise's glyph D-pad left |
| ASSISTED | the autopilot's line (mode · goal · waiting reason) as an answer-kind line for as long as it flies; TAKE CONTROL (B) never retires; the reticle always shown (RS aims) |
| weapon_groups 2 | the two group rows with RT / LT badges in the fight form; the afterburner glyph on LS click |
| holds_as_toggles | a state word for every latched action (AFTERBURNER, REVERSE, REAR VIEW) while latched: a toggle without one is invisible state |
| K&M | keycaps from the keymap, the click-pinned contact list, the maneuver row or its tag, MINIMIZE HUD as the peek |

**Compatibility** (fine / adapts: what it adds / discouraged: why; nothing is blocked):

| HUD preset | FREELANCER / CUSTOM | ACE | ASSISTED | K&M |
|---|---|---|---|---|
| Standard | fine (suggested) | fine: the ring's throttle arc is the reading (suggested) | adapts: the AP line pinned in the nav | fine (suggested) |
| Focus | fine | adapts: throttle tick + % in the under-arc band while partial | **discouraged**: the AP line is pinned whenever the autopilot flies, so Focus's bare calm never happens; Guided or Standard offered | adapts: the throttle tick while partial, MANEUVERS tag |
| Speedrun | fine | adapts: throttle % beside the speed (tabular) while partial | adapts: the flanks carry the autopilot's goal, the AP line in the nav (watching an autopilot run is a real use) | adapts: the list-toggle icon |
| Guided | fine | fine: "THROTTLE 64 %" is already a label | fine (suggested) | fine |

- **Suggested pairing, never forced**: FREELANCER / CUSTOM / ACE → Standard, ASSISTED → Guided, K&M → Standard. When the controller preset changes and the HUD preset equals the old pairing's suggestion, the options page offers one row "Suggested HUD layout: Guided · [A] use · [B] keep"; a declined pair is not offered again. The first-run guided setup preselects Guided for ASSISTED.
- **Budgets**: every adaptation is unplated free text (≤ 0.026, in a band or nav the preset already has): no boxed budget changes; the lint checks zones and the ship keep-out.
- **A gap this found**: ACE binds no action wheel, so today the wheel's rare actions (scan, formation, comms, filter, goals, rear) and this design's TARGET PART / SHOW HUD are unreachable on ACE (Q2).

## 5 Settings

- `hud_preset`: 0 Standard (default), 1 Focus, 2 Speedrun, 3 Guided, 4 Custom (shown, never picked). A preset writes its bundle; editing a bundled key relabels it Custom [R, TLOU II / MSFS].
- **Never written by a preset**: text_size, hud_scale, hud_opacity, hud_transition; glyph_style / glyph_look, hud_style, ui_style; aim_*, fluf_windows, speedrun_timer; the classic layer's button_legend / world_prompts / prompt_bar / hud_glyphs / ap_status_line; every subtitles key; every controller binding (§4). Reason: about 60 % of GoW players changed text size [R].

**Existing keys in the bundles**:

| Key | Std | Focus | Speedrun | Guided |
|---|---|---|---|---|
| hud_fade | 1 | 2 | 1 | 1 |
| hud_numbers (0 low, 1 always, **2 not full**) | 0 | 0 | 2 | 1 |
| hud_threats / hud_context / hud_alerts / hud_info | 1/0/1/1 | same | same | same |
| hud_wireframes (capital targets exempt) | 1 | 1 | 0 | 0 |
| ap_hints | 0 | 0 | 0 | 1 [O] |
| shield_cue / hull_cue_pattern / reticle_cue | 0/2/0 | 0/2/0 | 0/2/0 | 1/2/1 |

**New keys** (default = Standard; each through core\settings.h → SETTINGS.md → UI_SPEC.json / ui_meta.h → the Feature Tour, plus a drift_check line for bundle coverage):

| Key | Range | Std | Focus | Speedrun | Guided |
|---|---|---|---|---|---|
| hud_reticle | 0 data ring, 1 split arcs, 2 telemetry, 3 labelled | 0 | 1 | 2 | 3 |
| hud_calm_ring / hud_calm_panel | 0-100 | 40/60 | 0/60 | 60/60 | 65/70 |
| hud_card | 0 full, 1 compact calm, 2 compact always | 1 | 2 | 2 | 0 |
| hud_ship | 0 panel, 1 compact, 2 strip on change | 1 | 2 | 2 | 1 |
| hud_loadout | 0 full, 1 compact, 2 hidden calm + fire strip, 3 grid | 1 | 2 | 1 | 3 |
| hud_fight_forms | 0 today's sizes, 1 fight forms | 1 | 1 | 1 | 1 |
| hud_threat_rows (K&M; pad clamps to 2 until hud2.cpp 592) | 1-10 | 3 | 4 | 4 | 3 |
| hud_nav | 0 full, 1 lean | 0 | 1 | 1 | 0 |
| hud_place | 0 event, 1 standing, 2 off | 0 | 0 | 1 | 1 |
| hud_cruise_row (the cruise state's place) | 0 reticle lane, 1 under arcs, 2 in ring | 0 | 1 | 2 | 0 |
| hud_context_len / hud_context_combat | 0-4 | 3/2 | 1/2 | 1/1 | 4/2 |
| hud_prompt_learn (teaching glyphs) | 0 never retire, 1-5 uses, 6 start learned | 3 | 2 | 6 | 0 |
| hud_prompt_recall_s / hud_prompt_return_days | | 10/14 | 10/14 | 3/14 | –/– |
| hud_reveal | 0 always full, 1 context + peek, 2 peek only | 1 | 1 | 1 | 0 |
| hud_peek / hud_auto_peek | 0/1 | 1/1 | 1/1 | 1/1 (first per session) | 1/0 |
| hud_wake_s, hud_hold_nominal, hud_combat_linger_ms, hud_form_linger_s | | 4, 1, 2000, 6 | same | same | same |
| hud_frame, hud_ap_cue | 0/1 | 1, 1 | 1, 1 | 1, 1 | 1, 1 |
| hud_labels, hud_explain | 0/1 | 0 | 0 | 0 | 1 |
| hud_text_scale | 100-130 | 100 | 100 | 100 | 117 |
| speedrun_compare (par anonymised) | 0 PB, 1 par, 2 both | 0 | 0 | 2 | 0 |

Binds, not settings: the peek is the `dbl` slot of VIEW in UI_SPEC.json's three flight tables (`hud_peek`) and the game's USER_MINIMIZE_HUD on K&M; TARGET PART and SHOW HUD are wheel entries (wheel.cpp). The options page gains "Show all hints again".

## 6 Implementation plan

The code [C]: hud2.cpp (contexts 587-589, threats 592, state chain 1162-1167, context line 1170-1213, alerts 1226-1259, sim 1279-1350, layout 1417-1434, alert placement 1482-1516, chatFree 1566, fades 1610, wake 1697-1715); hud2_parts.cpp (ring, cruise row, card, loadout); hud2_click.cpp (K&M hotspots); hud2_kit (drawing); hudinfo / hudvis, wheel.cpp, presets.cpp, runtimer.cpp, display.cpp (subtitles, via the bus).

**Order of work**:
1. **Data first.** UI_SPEC.json gets the preset table: zones as rules, element rectangles per context × preset × device × on / off-screen target, budgets, prompt caps, each drawn value's **value id** (for the duplication check), each controller setup's requirements (§4). **Done 31 Oct 2026** (uncommitted at writing): `UI_SPEC.json` `hud2_presets` (every number with its section here; what this doc leaves open listed per preset under `open`), from `tools\uimock\patches\2026-10-31_hud2_presets.json`; `tools\ui_meta.py` → `core\ui_meta.h` `uimeta::hud2p`; the lint reads Standard's zones / budgets / caps / value ids from there.
2. **Extend `hud2 sim`**: ap, turret, formation, jump, loot, letterbox, combat_notarget, protect, capital, lane+comm, ap+comm, target off screen, plus a mock wireframe rectangle (sim suppresses wireframes and the letterbox today [C]). **Built 31 Oct 2026** (not yet run in game; names and parse `modules\hud\src\hud2_sim.h` + tests\hud2_sim_test.cpp, the states hud2.cpp Simulate, `hud2 sim list`): the mock-up's five unchanged, and each new state fills only what Slow fills in that real situation - `ap` the autopilot travelling (AUTOPILOT chip + "TRAVEL TO MANHATTAN", ModeVerb; cruising; TAKE CONTROL, GOALS), `apwait` its objective run waiting ("MISSION" + "WAITING FOR THE LEADER", WaitReason; no target), `turret` (TURRET VIEW chip, EXIT TURRET, a fight), `formation` (FORMATION WALKER chip and bar, the leader as the escort row, LEAVE FORMATION), `jump` (JUMP NEW YORK + the countdown, no bar; the gate targeted; MAP), `letterbox` (the jump's tunnel: `cine` and the letterbox fades - only the chip and the ring; the game's bars are not drawn), `loot` (TRACTOR 3 under the ring, a LOOT target, TRACTOR ACTIVE in the info line, TRACTOR pill), `capital` (a BATTLESHIP target with bars in a fight; no sub-target row - HUD 2 draws none until step 6), `notarget` (combat_notarget: a fight with no target - the LB hints at full alpha, no pad threat list), `offscreen` (the destination behind the ship: drawn as on screen - HUD 2 keeps no on / off-screen state until step 5; the direction is the game's native arrow), `comm` (a dock approach with DOCKING GRANTED . BAY 3 under TIME 1:42 while the comm surface shows: the event yields, TIME stays), `+comm` on any (lane+comm, ap+comm; the subtitles' panel itself comes from their `test` command). protect = `combat` (its protect bar). Not built: the mock wireframe rectangle (RequestViews still refuses the views under a sim). The sim's fight states set the fight rule (g_combat, the threat list and the fades) themselves; the lint needed no change.
3. **The lint** on `hud2 state`. FAIL: a plate in a zone; text under the floor; centre-stack overlaps; **one value id visible twice** (except declared second channels); **a bound action with no reachable prompt, keycap or wheel entry, or a wheel action with no wheel bind**; **an analog control in use without its readout**; **a latched toggle without a state word**. WARN: budgets, prompt counts, the ship keep-out. Over HUD preset × controller preset × weapon_groups × toggles × device; first on today's HUD (baseline). **Built 31 Oct 2026; the live baseline ran the same day: FAIL in every context, `docs\hud2_lint_baseline.txt`**: the pure checker `modules\hud\src\hud2_lint.h` (every rule above, plus title-safe 90 % as WARN; each number cited to its section here / U35; tests\hud2_lint_test.cpp) and debug `hud2 lint` (`hud2_lint_live.cpp`: the kit records the last drawn pass's items, tagged by part and ring element). Live inputs: Standard's zones S1 / S2 / the r 0.045 core, budgets and prompt caps from step 1's data (`ui_meta.h` `uimeta::hud2p`; the combat budget on K&M is 2.1's 17 %), value ids = the model's text fields matched to the drawn strings, the throttle lever's readout, the toggles' words (BOOST, REV, KILL, TURRET VIEW, REAR VIEW for the wheel's latch). **n/a live**: the action-reach rule (no prompt catalogue; the wheel's entries are wheel.cpp's), steer / aim (the native reticle), a button-latched rear view; contrast (needs frames: a named hook only).
4. **Preset plumbing**: the keys, bundles, Custom on edit, drift_check, the suggested-pairing row. **The plumbing built 31 Oct 2026** (not yet run in game; no layout changes): the key `hud_preset` 0 Standard (default) / 1 Focus / 2 Speedrun / 3 Guided in the [Controller] registry (`controller.cpp` kOptions, `docs\SETTINGS.md`; not in UI_SPEC.json's rows, so the options page lists it only with `show_advanced`, raw, on the CONTROLLER tab, and the Feature Tour does not change until step 5); `modules\hud\src\hud2_preset.h` (pure, tests\hud2_preset_test.cpp): the clamp (out of range = Standard), which layouts are built (only Standard: every other preset draws as Standard, labelled "Focus (layout not built: Standard drawn)"), the preset's data and its value ids' homes (`Home`, the off-screen hand-over), and §4's adaptation read - the controller setup's column, what it needs (the table's rows), the compatibility cell and the suggested pairing, from `UI_SPEC.json` `controller_setups` generated into `ui_meta.h` (`kCompat`, `kSuggestedPad` / `Kbm`, `kLeverPartialLo` / `Hi`; tools\ui_meta.py). HUD 2's model carries the preset, the drawn layout, its data and the adaptation every frame (`hud2.cpp` Update; nothing acts on them yet); `hud2 lint` takes the active preset's zones / budgets / caps and names it in its header; `hud2 state` prints a `preset=` line. **Open for step 4**: the bundles (writing §5's existing keys would change the HUD before the layouts exist), the new §5 keys, Custom on edit, the drift_check bundle-coverage line, the suggested-pairing row on the options page, the spec row (with step 5).
5. **Layout core**: one per-frame layout from (preset, context, device, controller setup, aspect, target on screen); a rectangle ease; form hysteresis, combat-on-damage, `RingRadius()` per preset; chatFree / frame on the bus. **The core built 31 Oct 2026** (not yet run in game; `UI_HUD2.md` 5.17): `modules\hud\src\hud2_layout.h` (pure, tests\hud2_layout_test.cpp) computes every element's rectangle - nav, card, threat list, ship, loadout, ring (`RingRadius()` = the drawn preset's `reticleR`), context line, alert, the bus's ring / topFree / chatFree - from the drawn preset's data (`ui_meta.h` `uimeta::hud2p`: the margins, the ring radius, each element's numbers per context via `Num()`, the alert plate, the budget caps; `tools\ui_meta.py` now also writes the elements' numbers and the shared margins / aspect factors) for the frame's context × device × W / H / k, plus what the preset's rules suppress (the letterbox's collapse; a budget rule that drops elements in its order while the plates are over the cap - Standard has none: today's HUD is over the budget in nearly every context, its compact forms are step 8's). `hud2.cpp` ComputeLayout is its impure half (the text measures, the dialogue rectangle from the bus); Render, Publish and the fades take the core's result. Standard is today's HUD bit for bit (the test compares 576 frames against the old ComputeLayout over 9 screens × pad / K&M × 4 scales × 8 states: every value equal); the spec's numbers it reads are today's (card 0.215 = the combat full form off screen, loadout 0.15 = the fight form, context y 0.915, ring 0.10, margins 0.05, aspect k 0.9 / 0.8); **11 overrides** keep today's literals where the spec's (refined) Standard differs - card width 0.44 (spec 0.36), the K&M card 0.22, the nav's width rule / box / height / edge, the threat list's geometry, the ship 0.40 × 0.17 (spec 0.30 × 0.12 calm, 0.36 × 0.15 fight), the loadout 0.62 / 0.66 wide (spec 0.48), the ring's bus rectangle, topFree / chatFree (`hud2_layout.h` `std_ov`, each naming its gap). **Open for step 5** (with the first preset that has a second form): the rectangle ease, form hysteresis, combat-on-damage, the 21:9 frame (`hud_frame`), `chatFree.y1` per preset, the controller-setup adaptation (`Adapt` is read, not laid out), the on / off-screen target state. Found: the pad's threat list's 2nd row ends 0.004 H inside S2 at 16:9 and wider, and the context line's fallback above the panels (a wide first prompt on 4:3 / 5:4) crosses S1's bottom - today's layout, kept; the lint baseline ran on K&M only.
6. **The shared floor (§1.4, 1.5, 1.7)**: the alert queue; the sub-target reader (+0x534), the card row and the contextual TARGET PART wheel entry (USER_NEXT_SUBTARGET); the card handover on / off screen; the headline-after-banner rule; protect alert, hand-back cue, timer field; threat sorting and bearing arrows only where no native arrow points; ROOM on the tractor row. These help Standard before the other presets exist.
7. **Prompts and reveal**: the three kinds, lanes, priority, per-prompt stall preconditions, the retire store, wake-on-change triggers, the View `dbl` slot in the spec tables, MINIMIZE HUD and N on K&M, the SHOW HUD wheel entry, hover gating.
8. **Presets in order**, each with your frames before the next: Standard (re-baseline its frames once; the harness pins `hud_preset 0`), Focus (first measure the lead pip's dwell radius; **its layout built 31 Oct 2026 with r 0.12 - §2.2 "Built", `UI_HUD2.md` 5.18; the driver's lint baseline `tools\hud2_lint_baseline.ps1 -Preset 1` and the frames next**), Speedrun (flanks, tabular digits, ETA smoothing, the strip), Guided (state rows; strings from docs\kb-verified facts).

**Verification**: the lint over sim contexts × 4 presets × the controller matrix × the display matrix (1080p, 4K, 1920×1200, 1600×1200, 1280×1024, 2560×1080, 3840×1600, 5120×1440; scale × text up to 1.18); contrast ≥ 4.5:1 for every text rectangle on full-res frames of the M03 nebula, a bright planet and sun glare (decides Focus's scrim); motion frame by frame (combat entry / exit at 2 km, a device switch, a card handover); cost under 0.3 ms (0.12 ms today [C]); frames for you per preset (calm, combat, lane, dock, capital) as 4K crops beside the reference games.

**Unverified, to check in game**:
- the native bracket label's text height at 4K and 720p (decides §1.7 pair 1 A vs B);
- the objective banner's duration, and that it shows in combat and after a letterbox under HUD 2;
- what USER_MINIMIZE_HUD does natively while HUD 2 hides those panels (hudvis's state byte), and whether HUD 2 can read it as the peek;
- USER_NEXT_SUBTARGET with the target panel hidden, and whether the native sub-target bracket stays visible;
- that the DualSense Create button reaches FLvibed's XInput reader as View under Steam Input;
- the lane-disrupted result code; the order of `space::Contacts`; the dialogue panel's real height; the compact forms' scale headroom.

## 7 Questions still open for you
**The owner, 31 Oct 2026, on the lower centre** ("The CRUISE ON text still hides the ship. We should really make sure the
lower center of the screen is devoid of HUD elements (unless they're diegetic like an on-screen ship/solar being a target,
etc)."; binding; what changed: `UI_HUD2.md` 5.23): nothing HUD 2 draws in x 0.40-0.60 W from the ring's lowest drawn edge
+ 0.01 H to the screen's bottom, every preset, pad and K&M, every context. Built: Standard's reticle lane under the ring is
gone - CRUISE ON reads inside the ring in the charge seconds' slot (+0.083), [X] TRACTOR n is a right-lane context
prompt, the throttle word stands beside its arc; Focus's band moved inside the arcs' circle (+0.078); the context line's
fallback is two lanes above the panels instead of a centred line; the lint zone `bottom` runs from the layout's
`clearY0` (Standard 0.6344 H, Focus 0.6333 H at 16:9; was 0.85 H) and FAILs any recorded item there but the transient
overlays (hit flash, missile chevrons). The 1.6 "reticle lane" (answers, cruise, tractor) is retired: its words live in the
ring, the context lanes and the alert. Open: the power arc hidden while cruising; TRACTOR n yields to combat's X-hold
TURRET; the alert card's below-the-ring fallback (4:3 with a list / dialogue; 1.3 exempts the alert) and the alert queue's
FLARE mirror "in the reticle lane" need a decision.
**The owner, 31 Oct 2026, on the ship panel** ("have a look at the lower left HUD panel ... there appears to be some
wasted space in there. Reduce it."; binding; what changed: `UI_HUD2.md` 5.22): Standard's ship panel 0.40 -> **0.31 H**
wide (height 0.15 kept, equal to the loadout's): the icon column sized to its content, tighter gaps, the values
right-aligned to the nearer edge; with the context pills' tight K&M keycap the left lane (0.276 H at 16:9) holds `[key]
CANCEL CRUISE` whole at 16:9 / 16:10 / 21:9, pad and K&M (closes the open item below). Focus's bottom-left block (free text, no icon column) kept at 0.32.
**The owner's decision, 31 Oct 2026, "Yes, move it"** (5.20's open item; binding; what changed: `UI_HUD2.md` 5.21):
Standard's ring cruise prompt with its keycap above the ship moves to the bottom-left lane exactly as Focus's did (B13) -
calm `[key] CRUISE` in the left lane, while charging `[key] CANCEL CRUISE` first; the ring keeps only the plain state word
(CRUISE while charging - its seconds are in the ring already -, CRUISE ON) with no keycap; the ship area stays clear; pad
and K&M alike. Open: CANCEL CRUISE (0.204 H) is wider than Standard's 16:9 left lane (0.186 H) and scrolls there.
**The owner's decisions, second round, 31 Oct 2026** (on the 5.19 frames `focus_frames_2026-10-31\after_A_standard.jpg` /
`after_B_focus.jpg`; binding; what changed: `UI_HUD2.md` 5.20):
1. **Long names: a marquee**, the world-standard public-display pattern, instead of a permanent "..." cut - a text that
   does not fit its room shows its start with a trailing "...", holds a short pause, scrolls slowly and continuously
   (sub-pixel, frame-rate independent) until its end is visible, holds, resets to the start and repeats; clipped to its
   room; a text that fits does not move. Everywhere a text was fitted / cut. Built: 1.5 s holds, 0.05 H/s, a 0.25 s fade
   for the reset (`core\game\marquee.h`, `hud2kit::TextMarquee`). HUD 2 has no reduced-motion option yet (open).
2. **Launchers hidden while LOCKED**: keep (fine).
3. **The loadout too wide because of the consumables**: the consumables' cross (D-pad / key cross: nanobots, batteries,
   countermeasures, mines) sits on top of the loadout base panel's right end, stacked above it and right-aligned; the
   base narrows to the weapons list, freeing the bottom of the screen. Both Standard and Focus (its fire strip likewise);
   A5 kept (the ship panel and the loadout's base share one height); S1's 0.80 H edge and the title-safe area respected,
   the lint 0 FAIL. Built: Standard base 0.40 (K&M 0.44) + cross 0.20 x 0.15; Focus strip 0.22 + cross 0.16 x 0.14.
4. **Prompts tight to the bottom-left and bottom-right, the very centre bottom clear "for aesthetic purposes"** (the
   cruise / dock / context pills hid the ship): the context line's pills go into two lanes hugging the bottom-left and
   bottom-right (beside the ship panel and the loadout), never in a central bottom band; both presets; a lint zone FAILs
   a prompt / text in the bottom-centre band. Built: the rule is the corners' jobs (1.1) - the ship's movement and control
   left, what you fire / use / target right (1.6 "Five lanes"); the zone `bottom` x 0.40-0.60 W, y >= 0.85 H.

**The owner's verdicts on the Standard and Focus frames, 31 Oct 2026** (`research\experience\runs\focus_frames_2026-10-31\
decisions_A_standard.jpg` / `decisions_B_focus.jpg`; binding; what changed: `UI_HUD2.md` 5.19, the items marked "Owner"
in 5.16 / 5.18):
- **A1** (Standard's loadout, now 0.15 H): "mostly fine, but it's not fine for the glyphs to overlap. Consider tightening
  the glyphs themselves so that everything fits nicely." - no glyph / keycap overlaps anywhere in the loadout (the NUM 1 /
  NUM 2 / NUM 3 keycaps touched each other and the weapon names); the glyphs / keycaps sized to fit, the 0.15 H panel kept.
- **A2, A3, A4**: fine (kept).
- **A5**: "keep these panels at identical height" - the ship panel and the loadout both 0.15 H, the ship panel's content
  (up to 3 escorts) fitted by tighter rows, no overlap.
- **B1 / B10 / B11 / B12** (Focus's calm meters): like recent AAA games - in calm the hull meter only while the hull is not
  full and the shield meter only while the shield is not full, at reduced opacity; a full meter hidden completely; each
  meter on its own. Combat as built (full arcs).
- **B4-B7** (Focus's card / list): "It is important for HUD elements inside cards to not overlap, neither glyphs nor
  contextual data" - every overlap inside the Focus card and the K&M list fixed (the faction line over the keycap row, the
  CTRL+R / CTRL+T keycaps), and the same in Standard's card wherever it happens.
- **B9**: cargo hold / credits are not needed in flight - dropped from Focus's bottom-left block (it now holds the escort
  rows alone).
- **B13**: "the cruise prompt itself can live at the edge of the screen rather than at the center. It's crowded." - Focus's
  cruise prompt moved out of the centre band to the context line, the teaching lane at the bottom edge (§1.6).
- **B14**: fine.
- **B16** (Focus's fire strip): show ALL weapons including launchers (launchers are often in no active group; the player
  wants armament status even when focused), with a smaller font and adapted spacing so all fit; the active group highlighted.
- The known fault: ABORT DOCK showed an empty keycap on K&M - fixed (the free-flight key's name "`" was a near-invisible
  mark: now "~").
- Not mentioned, kept as built: B2, B3, B5, B6, B8, B15.
- The check that guards the rule: the lint's `FAIL panel.overlap` (two items of one panel overlapping or under 0.01 H
  apart; `UI_HUD2.md` 5.19).

**Answered by the owner, 29 Oct 2026: yes to all five recommendations** - (1) peek on View double-tap in every
controller preset AND the SHOW HUD wheel entry, the hub opening 0.3 s later accepted; (2) the wheel on ACE's RB hold;
(3) option A (the native bracket label is the target's only on-screen home, no hook; B only if frames show it too small; **replaced 6 Nov 2026, §1.7 pair 1**);
(4) the cruise glyph retires in Standard / Focus / Speedrun, never in Guided; (5) Focus + ASSISTED "discouraged"
(Guided offered, never blocked). Also: SDL3 linked statically into FLvibed.dll (MinSizeRel, ~1.3 MB; IDEAS U29). The
questions as asked:
1. **Pad peek on View double-tap** in every controller preset. The price: the NeuroNet hub opens 0.3 s after a View tap, because the dispatcher waits to see whether a second tap follows. Acceptable, or SHOW HUD in the wheel only (no delay, but slow mid-flight)?
2. **ACE has no action wheel.** That makes TARGET PART, SHOW HUD and the existing rare actions unreachable on ACE. My proposal: put the wheel on ACE's **RB hold**. RB's tap (next enemy) stays and waits only until the button is released; LB's countermeasure stays instant. Agreed, or another hold?
3. **Target card vs the native label (§1.7 pair 1).** A: the native bracket keeps name, distance and bars while the target is on screen, with no hook. The card takes them over only while the target is off screen. B: a hook in the native draw, only if frames show the native label is too small. Go with A?
4. **Cruise glyph.** It becomes a teaching glyph that retires in Standard, Focus and Speedrun (the state word stays; the glyph returns after a disruption). It never retires in Guided. Agreed, given that your 26 Oct permanent spot is now free?
5. **Focus + ASSISTED marked "discouraged"** (offered Guided, never blocked). Fine, or treat it as an ordinary adaptation?
