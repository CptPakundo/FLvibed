# HUD 2 overhaul: research and adversarial critique (P11f gate, 5 Nov 2026)

**What this is:** the owner's order before any overhaul code: "another crack at research + adversarial critiquing of the whole thing, with leanings both towards how Freelancer does things and how modern AAA games do things, and how to strike a balance of both while still maintaining that flavor that FLvibed is starting to develop over time." It judges `UI_HUD2_SKIN.md`, `UI_HUD2_INPUTS.md`, `UI_HUD2_GROUP_CUE.md`, `UI_HUD2_SIZE_AUDIT.md`, `UI_HUD2_PRESETS.md` and the frames of `research\evidence\hud2_gate\`. It changes none of them; nothing here is built.

**How it was made:** a clean session, not the design's authors. Three lens readers, given the frames before any design reasoning: Freelancer's way (26 sources), modern AAA (35 pages read, 11 excerpts), FLvibed's flavour (13 pages, the owner's rules, the history). Three attackers measuring real pixels and reading code: flight, every other screen, scope and failure. Two verifiers re-derived 49 claims: 18 held, 21 held with a change, 7 weakened, 2 wrong, 1 could not be checked. Corrections are applied below; section 10 lists what died. Evidence: `research\evidence\hud2_critique\` (`lens_*.md`, `attack_*.md`, `verify_*.md`, scripts, crops).

**Standing of a claim:** *measured* = read from real frame pixels; *frame* = seen; *code* / *doc* = read; *web* = page fetched (URLs in the lens files; the key ones in section 11); *memory* = unfetched knowledge; *inference*; *owner* = the owner's words. No game ran. Motion was read from code and docs, never seen.

## 1. Verdict

**Build it, but not in this order and not this exact list.** The direction survived every attack: corners by job, an empty lower centre, vitals at the crosshair, alerts that name their cure, near-opaque plates, verbs, one dialog form, the strip, the game's logic left in charge, off means classic. Five things should change before code:

1. **Effort follows the screen table, not the player.** Flight is step 9a of 11. The dealers and NeuroNet windows are marked done, yet their row text has about 40 % of the stroke of HUD 2's own text, and no build step reaches them.
2. **The weapon cue, as specified, prints on the target's name** at the moment it lights, and a lit M or T can mean no lock.
3. **"HUD 2's, not the game's" is right for chrome and wrong for identity.** The logo and the cursor have no shown gain. NEURONET is a word the game never shows a player.
4. **The pad map's inputs do not close, and the grown map is high risk, not medium.** Hidden toolbar controls stand on the map it grows over. Ship the frame, words and strip first.
5. **The sheets differ from what will be built** in the focus state and the glyphs, and they never show the logo, the cursor, a real bracket with the cue, or a comm portrait. Fix them before the owner rules on those.

**The balance in one line: Freelancer decides what the player recognises and does; AAA decides how text, menus and input behave; FLvibed's own grammar decides how it looks.**

## 2. The balance, area by area

| Area | Freelancer's way | AAA practice | FLvibed's flavour | Balance |
|---|---|---|---|---|
| Flight instruments | Empty centre, heavy corners, no radar, attitude colours, brackets, edge arrows | Gauges at the aimpoint; clarity beats element count; per-element visibility | The arcs, the clear lower centre, a HUD that goes away | **Keep FLvibed's.** Lean AAA only on contrast: a keyline on the arcs |
| Alerts | None to speak of: a blink, a Neural Net line | Directional, rate-coded; AC7's beep earns its noise | Event over answer: the mod's best idea, ahead of the genre (absence from search, not proof) | **Lean FLvibed.** Cards only for what has a cure; standing dangers get a quieter form |
| Target, contacts | Who is out there at a glance, no list work; wireframes | Range always on the readout; one-glance damage; nearest and cycle on a pad | Pips, the wireframe as the last home of the game's art | **Lean Freelancer** on K&M (rows with no target held), **AAA** on the pad |
| Weapons | Every gun named, green when on | Range per weapon; colour alone misleads when a mark has two jobs | Armament stays on, smaller font not fewer weapons | **Keep the loadout.** The cue must state only what is true |
| Verbs on K&M | Verbs on screen, one click, no manual: the most praised behaviour after mouse flight | Verbs that say what happens; prompts only for the live device | Two-word imperatives | **Lean Freelancer.** Today K&M gets pictogram-and-keycap pairs |
| Nav map | Sectors, the painted universe, click to plot | Flat one-press layers; nesting fails on a pad; no scale-jump gestures; focus and cursor apart | Selection card, words where icons are unclear | **Lean AAA on input,** Freelancer's map itself stays. Best screen of the overhaul |
| NeuroNet windows | Five windows, one key each | LB / RB between peers; windows own the screen; inner menus are where the anger lives (Starfield) | Strip of words | **Lean AAA.** One top line, one left edge, one scrim; bigger text where it fits |
| Dealers, bases | The room is the screen, navy against gold deco; infocards | More per screen, compare without equipping; mods fixed Starfield this way | VS MOUNTED, price deltas, the docked strip | **Lean AAA on text weight,** Freelancer on the rooms staying visible |
| Dialogs | Terse YES / NO, ornate frame nobody praised | Verbs, a consequence line, a safe default | Event, cost, answer | **FLvibed's card form.** A generic centred box is the drift |
| Front end, loading | Logo, floating buttons, planet | Resume straight in; the main menu hardly matters to players | Place name plus pips | **Leave most of it.** Spend on the wait and on Continue, not on the look |
| Words, identity | Neural Net, Freelancer, Sirius, sectors | Consistent nouns | Verbs | **Lean Freelancer.** The game's nouns, FLvibed's verbs |
| Options, access | Boxes can close; HDE's skins are mostly choices | Text scale apart from layout, solid backing, HUD colour, 7:1 modes | One master switch | **Lean AAA, cheaply.** Text apart from `k` now; colour later |

## 3. The flavour, stated plainly

**A pilot's instrument for a third-person ship. It frames the ship, says what to press in two words, shows only what the moment needs, and speaks in Freelancer's own materials.** It is mostly grammar and behaviour, so a static mock hides it.

**Established:** the arc parentheses (shield, hull, power, speed between); the empty lower centre; every alarm carrying its cure; the button on the thing, with the broken HOLD ring; pips for every quantity; a HUD that goes away; speech cards with a coloured edge; Freelancer's flesh (Agency FB, wireframes, emblems). **Growing:** verbs on every control; yellow = "this one". **An accident worth adopting:** the mustard focus block with its side tick.

**The colour is not the identity.** Blue is the commonest sci-fi interface colour (web, scifiinterfaces.com), so every frame must be saved by something else. The test: cover the logo. Fight, alert and the loading mock pass. The quit dialog, the main menu's column and the NeuroNet strip fail; the map's chip wall is weak.

**The design's drift:** `UI_HUD2_SKIN.md` 4 lists furniture (plate, chip, tab, scrim) and no grammar. That is what makes the new screens read as any design system. Adding the grammar is a page of text, no code (R12).

**Quirks kept from inertia, not choice:** NEURONET; `x2 CLEAR` (notation, not a verb, on a row of its own); the K&M maneuver row; two forms of the place line; the bare speed on the mast of the Barracuda. Arial should be decided, not inherited: measured, it is the better reader for prose, so keep it for prose.

## 4. Recommendations, ranked by what the player gains

### R1. CHANGE: the base screens are not done. Add a text step before dialogs
Rows 29-31, 41, 42 are where players sit. Measured at `game_text_size` 100 with `window_text` on, so already grown: dealer, inventory and status rows have cap 0.0175-0.0200 H and stem 0.0013 H (1 px at 800), against HUD 2's own text at cap 0.023 H, stem 0.0033 H on built pixels. Contrast is not the failure: 9-10:1, worst 6.6 over the gold room. **Weight is.** That is the owner's "tiny, and honestly? Not just in deck mode", with numbers on it. AAA agrees: Starfield's anger lives in inner menus (web, Kotaku, charlieintel).

**Fix:** relabel these rows "skinned, text open". One rung up doubles the stem at 800 (0.95 to 1.87 px), 1.35x at 4K, and one fixed rung per opening keeps issue 22 solved. It is not free: dealers fit, **ship rows (pitch 0.065) and job rows (18 px apart) do not**, about 69 of 382 goods overflow the row (estimate), and native text has no marquee. So: the rung by resolution, 800 first; ship price down a step; job rows by growing the window (R8). State the plan's table in rendered cap and stem: its "0.030 H" is a font size, `fixed_height` a larger unit. Section 4's Row rule ("the name steps down") contradicts issue 22 and goes.

### R2. CHANGE: flight first. Rewrite section 15 by exposure
Flight is what is seen for hours. Section 15 has no step for issues 27 and 28, the alerts, `hud_size` or the keep list (*doc*, verified). Order proposed in section 9.

### R3. CHANGE: the weapon cue
- **It lands on the name.** The native bracket is fixed at 0.24 x 0.16 H at any range (*measured*, 12 boxes, 206-810 m; gunboat footage alike). Its label stands -0.098 to -0.062 from bracket centre; the row -0.106 to -0.080 from screen centre. They meet in 29-42 % of ring positions, and a player aiming converges on the middle (*inference*). GROUP_CUE 5's keep-outs never name the bracket; sheet_10 has none drawn.
- **Moving it up does not fix it:** `cy - 0.125 k` only moves the clash to targets just above centre (32-39 %) and crosses the hit arc. A target that is also the mission waypoint gets a larger diamond whose distance label lands on the row (vanilla footage, by eye).
- **Pick:** stay above, as asked, on small dark cells, the form the owner chose for the consumables, so the cue stays readable and loses nothing when it covers a name the card also shows. Frame it over a real bracket before building. Decision D5.
- **M and T fill on the game's lock only.** W6 read `locked=0` in all 16 samples with M and T "ok"; at 1200 m the only lit mark would be an unlocked torpedo (*inference* from reach data: guns 550-800 m, M 1000, T 1500). This also brings LOCKED to the eye: today it sits 0.88 H from the reticle. Decision D4. EVE's lesson is one mark with two jobs (web), which is what a lit M would be.
- Selected group repeats the loadout, against "one value, one place": the requester asked for it, so write the exception. Cut Speedrun and Guided from the spec until they exist (`hud_preset` is 0..1).

### R4. CHANGE: K&M is the game's main form and the weakest flight HUD
K&M reads 10-19 prompts against a cap of 5-6 in nearly every frame; pad 6-9 (*measured*, logs; the lint warns and nothing acts). The maneuver row is tiny pictograms beside keycaps larger than they are; target actions are `CTRL+R` and a crosshair where the pad reads NEXT (*frame* `planet__kbm`). That breaks "no ambiguous symbols" and the behaviour reviewers praised: verbs on screen, one click (3 voices, none opposed). In the mission moment `mission__classic` names who is out there, `mission__kbm` shows nothing at top right.

**Fix:** one lit chip plus key, full worded row on peek; words on the target actions; two to four threat rows on K&M with no target held, at floor size. Check them first with `hud_fade` in mind.

### R5. CHANGE: alerts. Cards only for dangers with a cure
- Card and "every danger showing" do not fit standing dangers: `KeepCard` holds three and drops the oldest silently once four fire inside 3 s, which standing cards would make real.
- **Radiation is said in four places at once** at 1280x800 (nav, DANGER, the Neural Net line, the proposed card). ON YOUR TAIL has no cure, so it is a notification, the generic form of the mod's best idea.
- **Pick:** radiation: one standing amber nav line, a card once on entry. Tail: an unlabelled ring mark at its bearing, like the hit arc. Lag: one small word, multiplayer only. Neither takes a stack slot. Decision D6.
- Hold MISSILE INCOMING while `missiles > 0`, not 3 s: past that, the warning is the CM slot, sound, and chevrons at 1.1-2.0:1 on bright scenes (4.3-5.0 at peak on dark sky).
- Event word to `kInk`: real cards read 3.9-4.65, 2.8-3.0 in the first 0.4 s burst.
- **Order trap:** deny `TrailAlert` only after its home exists: it is the only tail warning.
- **Separate live bug:** FLvibed's own Neural Net column cuts "DETECTE" at the screen edge at 800 and, on `radiation__pad`, stands over the prompts. Classic wraps it. Denying the native alert will not touch it.

### R6. ADD: a navy keyline on the ring
The ring is rank 1 and the only element with no backing. Over a planet, shield and hull are under 3:1 on every degree, even on the better side; the power arc on the sun reads 1.02-1.43 (*measured*, both verifiers' methods agree). Nebula and fight mostly pass, and hue saves position for normal vision. The fill level over cloud is what fails. A 1-2 px keyline costs no area, the text outline proves it works, and `SegArc` is open for issue 28 anyway. Lift the place line to `kChrome` in the same pass.

### R7. CHANGE: the map. Close the pad's inputs, split the build
- **Inputs do not close:** A means SELECT and also presses the focused chip; the D-pad has three jobs; waypoint has three routes; the design's X WAYPOINT is X REMOVE as built; nothing replaces LB's return to the ship's system, and SYSTEM / SIRIUS has no prompt in the lane (*doc*, *code*, verified).
- **Fix:** two zones. Canvas: LS, triggers, A / X / Y, no chip focused. Panel: the D-pad enters and walks every chip, A presses, LS returns: the owner's own Destiny rule. One named press for SYSTEM / SIRIUS and one back to the ship's system. Starfield shows the cost of hidden scale jumps (web, Kotaku).
- **Risk H, not M.** The results add `+0x4A8 / +0x4AC` every frame, a new-window hook, 16 grid texts, titles, a base list, legend meshes with no route and a reader unwritten. In `_sheet_p2clicks` the toolbar, X and grid text stand inside the grown map; once hidden, whether they still take clicks is unprobed. **Ship 4a first** (frame off, strip, words, around the native map: 6.1's own fallback). Grow it after a hit-box sweep and the second install.
- **Labels:** cap 12, x 8-9 px, 1 px stem at 800, true to the game. "Reads better at both sizes" should say "declutters". Run hook 0x4177B0. Chip words stand 1.7x the place names: the frame outshouts the content.
- **Right panel:** selection card = the target card (attitude, class, distance, faction edge). Layers: tiles for K&M; on the pad one line. Twelve equal chips is the weakest mock.
- Fit: at 16:10 on Normal, 0.785 + 2 x 0.38 + margins = 1.641 H against 1.6 H. One solver, `min(0.785 H, W - panels - margins)`, replaces four aspect rows. The Deck mock draws 0.82 H: declare it.

### R8. CHANGE: one frame for all five windows
LB / RB from MAP to INVENTORY goes from 0.797 H to 0.551 H, left edge 0.05 W to 0.247 W, chips to native boxes, at 0.61 s a press, 1.2 s for two. Tabs that slide are not tabs as players know them. **Fix:** one top line, left edge and scrim; a hook on 0x55E120 goes with the strip's work; probe 18, grow `NN_Inventory` by the Layout tier (pitch and font, not rows; hit boxes unproven), Deck first. Help is one line in a 0.55 H plate against "never empty": plate to content. State the docked bumper hand-over (room bar, SIDE, strip) in one line.

### R9. CHANGE: focus. Recolour the word, keep the block
Focus lowers legibility: 3.68:1 on pause, 3.6-3.7 on the job board, against about 10 for a plain row and the skin's own 4.5:1 (*measured*, verified). The dealer passes because the game turns the text yellow. **Keep the mustard and the fill as built;** the word in ACTIVE or INK reads 5.0-5.7 on every screen. Do not lower the fill: commit 1a91d79f raised it from 0.28 to 0.32 because it "half-read disabled". `+0x348` reaches `PauseButtonText`, `ItemName`, the job rows; not box buttons, rich text or front-end labels. Chips the skin draws are free. Doc: the code says 0.32, the doc 28 %.

### R10. CHANGE: dialog = the alert card, calm
Left-aligned, side accent, event, cost, answer. Drops a shape from the kit and passes the logo test. A SELECT, B KEEP PLAYING, not KEEP PLAYING twice. Amber only where progress can be lost: the mock draws it on EXIT. Verbs stay (the owner decided; every lens agrees) but their route needs **a probe per dialog class**: on NN_Confirm the caption is a separate zero-box TitleText, a likely route; NN_Prompt's text is rich text and the colour write does not apply; Exit's class is unfound; alpha 0 is unproven. Key rows on string id or caller, not text.

### R11. CHANGE: the gate. One registry row per root
3.1's "only the skin going off restores" conflicts with section 8's fall to native and with 6.6a's "stays visible". INPUTS 3 lists what a true deny would blank: brackets, arrows, banner, waypoints, `NN_Scanner`, group control, briefing, cruise countdown. The tester's flash is an ordering fix on the first UI pass and needs no policy. **Fix:** a row per root: hide / keep / keep root hide art / home. Unknown stays visible and logged. Fall to native does not exist today (one `__except`, whole module): build it, retry at the next open since windows rebuild anyway, keep the strip independent of families, and tell the player in one lane line, or one ornate window reads as a new bug.

### R12. ADD: a grammar table in `UI_HUD2_SKIN.md` 4
Pips for any quantity; accent edge facing the eye; event, cost, answer; verbs; mustard focus; HOLD ring. Then pips in menus: reputation, cargo, route. This is what carries the flavour onto new screens, and it costs a page.

### R13. CHANGE: the card reads the game's distance
Card and bracket show two numbers for one target (505 / 495; card larger in 12 of 12 pairs, by 7-20 m). The card reads centre to centre, the label surface distance, so on a capital the gap is the object's radius and could be large (*inference*, no frame). The cue tests surface distance, so card and cue would disagree. **Fix:** card and threat rows use the surface value: the project's own rule, "the value the game itself uses". Strike PRESETS 1.7 pair 1: the label has 1 px strokes at 800 and cannot be the only home of the name. That leaves one more exception to the owner's "one value, one place", so write it down with the cue's. Keep the wireframe in the 0.36 card (the owner's answer) and redraw sheet_03, which lost it and the emblem.

### R14. CHANGE: sizes and the metric
- x-height at 800: Small (k .79) 8 px, Normal (.90) 10, Large (1.08) 11-12. Valve's 9 px does not say which height (web); on the strict reading **Small fails at 800** and Normal clears by 1. Small is honest at 4K: 38 px, classic's. **Fix:** at 800 Small keeps Normal's text; lint on rendered ink, not font size.
- Separate text from `k`: `text_size` moves `k` today. Settle it inside `hud_size`, or it is built twice.
- **Report steady and awake peak apart.** `hud_fade` 1 takes panels to zero after 4 s and `boxed` ignores fades, so `calm__pad` logs 8.3 % over a near-bare frame. The travel "overrun" (item 12) is a metric fault, not a design fault.
- The audit's headline fails at 800: 22.5 / 22.7 less its own savings is 17.9-18.5 against 16 / 17, not 15. That is k 1.08, the Large preset; Normal today reads 15.7. Apply 3.6, re-read, then state it. Escorts are the largest element for their rank; the slim plate is the owner's choice, so it stands.
- The gate's `__size_small / normal / large` names are k .75 / .875 / 1.18 (800: .68 / .79 / 1.08), not the recommended presets.

### R15. ADD: small access gains
A solid-plate row costs little, but plates already read 92-94 % opaque, so a high-contrast mode is mostly R6. Per-element visibility: designed (`hud_reveal`), absent from SETTINGS: say deferred. HUD colour: defer; hull on sea falls from dE 88 to 38 under simulated protanopia, and position still tells them apart. Audio for tail and radiation: defer.

### R16. CHANGE: HOLD BUY needs an alternative, not removal
XAG 115 names purchases and selling, and says no holds for confirming them, and adds "Provide alternate options" (web). Keep the hold from the row, allow a tap on a focused BUY. Work is real: `PanelFocus` only reads focus, `:1161` blocks that tap on purpose, and the re-select after a trade needs care. The NMS thread is weak here: 8 on topic, about routine gathering.

### R17. CUT or DEFER: loading, column plate, multiplayer's Shadow table
A bar for 0.65 s of a 1.1 s load is the flash 6.4 set out to avoid. Defer the screen; when it comes: place name at once, bar only past 1.5 s, since a Deck's load is unmeasured. **The real wait is the front end's 2.37 s with about 1 s empty, on every page:** make decoding it a probe. Cut the main menu's column plate: it adds nothing, the floating chips are cheaper, and a small arcs mark by the version line passes the logo test. Multiplayer: page rule only.

### R18. ADD: rows and frames that are missing
No row: `NN_Scanner`, the transfer panel (inline in `NN_Dealer`; "quantity dialog" misnames it), the log's inner tabs, banner, waypoints. In the table with no frame: briefing, repair, news, rumour, legend, base list. "62" is not the game's surfaces. Eight of the rows are stale from before the probes (`attack_scope.md` 4 lists 18 items; 3.1, 3.3, 6.4, 6.7 and section 15 would mislead a builder).

## 5. What the frames show that the documents do not

- **The base screens' weakness is stroke, not height or contrast** (R1): no document measured it.
- **The cue's seat is occupied:** the ring's top is free only with nothing near the centre, the one time the row does not matter.
- **Two numbers for one distance** in the same frame.
- **FLvibed's own Neural Net line breaks at 800** (R5): the design blames the native alert.
- **The focus block fails the plan's own rule,** on the pause menu of every session.
- **Plates work:** 92-94 % opaque, text holds over the sun and the gold rooms. The design's plate rule is better than its documents claim.
- **Calm is nearly bare:** better than every budget in the docs, because the metric ignores fades.
- **The "classic" baseline is HDE's Advanced Widescreen HUD plus FLvibed's legend,** ticked by HDE's installer by default at 16:9 and wider, not the 2003 layout. "Small mimics classic" means that.
- **Mock against build:** the mock's focus word is yellow at 6.9:1, the build's is blue at 3.7:1 (both stood on sheet_07); coloured Xbox face buttons where the build draws chrome glyphs, beside PlayStation lanes in the same frames; amber on EXIT; Deck map 0.82 H; "FLVIBED 0.4.0". A face button in yellow or red breaks one colour, one meaning on every sheet. **The logo, the cursor, a real capital fight, a comm portrait, K&M on new dialogs, Focus, 21:9 and 4:3 appear on no sheet.**
- **A capture artefact, not a defect:** the second highlight on `pause__pad` and `jobboard__pad` is the control under the screen's centre, in 4 of 4 frames. A real pad player sees it only with the free cursor parked off the row. The job board's native tooltip does stay on the pad.

## 6. Where the design is right (attacked and held)

Keep as designed:
- **Near-opaque glass:** the anti-fashion choice; Star Citizen and EVE players keep attacking translucency (web).
- **The alert naming its cure,** and its honest form, `NO NANOBOTS · BREAK OFF`.
- **Corners by job, lower centre clear,** in every frame read. **The ring's place.** Pulses at 1.5-1.6 Hz.
- **Contextual calm.** **Outlined text.** **Cue states** read at true pixels at 800, by shape as well as colour.
- **Growing the map** and rejecting a map of FLvibed's own. **The universe stretch:** no resolution knee at 1.6x.
- **The strip's place and order:** MAP in the middle answers the player's question.
- **Verb dialogs,** risk-based focus, B safe. Exit defaulting to its action stands.
- **The power arc** at 44 degrees draining to the middle.
- **VS MOUNTED, price deltas, GOOD / FAIR / HIGH:** the best text in a base, and what Starfield players modded in.
- **Arial for prose.** **No tips on a short load.** **No added motion on a measured path.**
- **Method:** probes before code, reported honestly; the seam audit; reversibility; off stays classic; section 11's list of what stays out; "a plate must earn its area".

## 7. Decisions for the owner

**The owner's rulings (6 Nov 2026).** The overhaul is release 0.4.0: "total, complete and global, a real makeover possibly"; 0.3.5 is the refactor and the bug fixes. Where a ruling differs from the critique's pick below, the ruling stands.

| # | Ruling | State |
|---|---|---|
| D1 | "All new, but do show me what you intend to replace the logo and the cursor with before going ahead with that." | **Decided in direction; the critique's pick is overruled.** Replacements designed and shown (`research\evidence\hud2_gate\decisions\D1_*`); nothing proceeds until the owner approves them |
| D2 | "Keep the game's name for it." | **Decided: NEURAL NET.** HELP or INFO was not said |
| D3 | 0.3.5 = the refactor and all fixes; 0.4.0 = the overhaul and the options menu | **Decided** |
| D4 | "I like to think that we can make the most of given space. We have M and T for torpedoes. We can change colors, we can fill the boxes up, we can make them glow... These are all possible avenues of signalling information to the player, as I'm sure UI/UX research can back up." | **Open, shown:** colour = selected, fill = in reach, form = locked (a tick) or out (amber, a slash), glow = one pulse at a change. `decisions\D4_cue_language.jpg`; sources `research\evidence\hud2_gate\d4_channels_research.md`. No real lock seen yet, 48 samples |
| D5 | A visual comparison | **Open:** the sheet is with the owner |
| D6 | A visual comparison | **Open:** the sheet is with the owner |
| D7 | "Previous design decisions may be subject to change, so give me a visual comparison." | **Open: the 0.024 H floor is open again** |
| D8 | HUD 2 by default, no first-run page | **Decided;** the critique's pick is overruled. The options menu is `options-research`'s, for 0.4.0 |
| D9 | CONTINUE on the main menu: yes | **Decided** |

**The owner's rulings on the comparison set (6 Nov 2026, second round; these close the rows above):**

| # | Ruling |
|---|---|
| D1 logo | **Keep the game's logo.** The drafts are retired |
| D1 cursor | **The new set, all 19 shapes, as drawn** (`decisions\D1b_cursor.jpg`) |
| D2 | **NEURAL NET; and INFO, not HELP** |
| D4 | "Glow, refire bar and also include the tick, and make it lightly bounce with the same frequency as the glow. Also, invert the triangle (pointy side down)." Drawn: `decisions\D4_final.jpg`, `D4_final_motion.gif`; `UI_HUD2_GROUP_CUE.md` 0 |
| D5 | **Above the ring** |
| D6 | "The critique's pick: an amber nav line, a ring mark and one small word; cards stay for dangers with a cure. Make sure the nav line doesn't overlap or gets overlapped, and if there's room for it, consider keeping the radiation image from the original game next to the amber nav line." Drawn: `decisions\D6_final.jpg` |
| D7 | **A: floor 0.024 H on Normal, 0.021 H only on Small, with Small keeping Normal's text at 1280x800** |

**Further rulings (6 Nov 2026):** R12 yes: the grammar is in `UI_HUD2_SKIN.md` 4. Item 12, the travel budget: fix the metric, report steady and awake apart.

**The owner's rulings on the before-and-after sheets (6 Nov 2026, third round; sheets `research\evidence\hud2_gate\decisions\R*.jpg`, `Reticle_bottom_forms.jpg`).** Two overrule this critique: R10, and R17's loading screen. Sections 2, 4, 8 and 9 keep their text as the record; the ruling wins. A flaw in a sheet's drawing is no part of the design.

| # | Ruling | Against the critique | The sheet's drawing flaw |
|---|---|---|---|
| R4 K&M flight | **All of B:** one lit chip with its key, the worded row on peek, NEXT / CLEAR words on the target buttons, 2-4 threat rows with no target held | As R4 | Keys over the card's lower edge |
| R6 ring | **Add the navy keyline on every arc;** the place line to `kChrome` in the same pass | As R6 | A ragged outline over the planet; the sun rows are the intent |
| R8 windows | **One common frame:** one top line, left edge, scrim; windows grow by pitch and font; probe 18, hook 0x55E120 | As R8 | None |
| R9 focus | **The word in ink on the focused row;** mustard block, bar and the 0.32 fill stay; box buttons and rich text keep today's | As R9, INK of its two colours | Top-left B panel not recoloured; a white sliver left of the block; the job icon whitened |
| R10 dialogs | **Keep the centred box** | **Overruled:** no alert card, and section 2's "a generic centred box is the drift" falls. Verbs, amber only where progress can be lost and a probe per dialog class stand | The focused word still blue: R9's ink applies |
| R13 distance | **Card and threat rows read the game's surface value;** card, bracket and cue agree. The exception to "one value, one place" is written with the cue's (`UI_HUD2_PRESETS.md` 1.7) | As R13 | None |
| R16 buy | **The hold on the row stays, a tap on the focused BUY is added;** the lane says so | As R16 | The row keeps a second focus block beside BUY; a soft patch behind the lane |
| R17 menu | **Floating chips, no column plate** | As R17 | None |
| R17 loading | **Form C, in 0.4.0:** the place name at once, a bar only past 1.5 s. Multiplayer's tables stay with the page rule | **The deferral is overruled** (item 11, section 9 step 11); the form is R17's own, and multiplayer's Shadow table is cut as it said | None |
| Reticle bottom | **Form C:** the throttle inside, above the meter, centred, filling from the middle out; the meter drains to the middle; the speed under it; the bottom drops 0.019 H. The speed stands over the ship's nose in all forms; the lower-centre rule moves with it | Answers item 10 | None |

**Three points those rulings left, ruled the same day (fourth round):**

| Point | Ruling |
|---|---|
| R16 | **The tap works on the focused BUY and SELL;** the hold on the row stays |
| Item 7 | **Windows at 1280x800 with no other evidence: Large by default;** the player's choice wins (`UI_HUD2_INPUTS.md` 7) |
| R13, the name | **The name on both card and bracket for now, a holding position.** The owner wants the targeting display itself researched: remove the brackets and compress the information next to the target, with or without a background; or contextual leader lines to the target with the information beside them. **So the bracket and label design is open, not settled, and "the native bracket label is the target's on-screen home" (29 Oct) is reopened.** Research: `docs\research\target-info-display.md`. What leans on it: `UI_HUD2_SKIN.md` 16 |

**The targeting display, ruled (6 Nov 2026, fifth round; this closes the row above):** the owner's combination stands: a mini card that settles into the form worn by the ship, bars at the bottom, with a soft bed behind the words. The card keeps name and distance, so R13's exception stands. A waypoint target follows its type, with a small diamond round the caret. The lock breathes at 1 Hz, the waypoint at 0.5 Hz. The rest: `UI_HUD2_SKIN.md` 6.10.

**The critique's picks, as written before the rulings:**

| # | Decision | Pick | Strongest case against |
|---|---|---|---|
| D1 | **How far "HUD 2's, not the game's" reaches.** It was said of a menu arrow, then made a rule | **Chrome only:** frames, plates, dialogs, lists. The game's where the thing is what players know: logo, nouns, attitude colours, room art. **Keep the FREELANCER logo. Cursor: keep the game's, or opt-in.** Neither fixes a legibility fault, and neither has been drawn. The cursor is cut on cost and mixed styles; the reviews praise the mouse scheme, not the cursor's art | The owner wants a complete skin and said so plainly; a real logo over a new UI is the seam the rule exists to remove. An opt-in leaves two looks to test |
| D2 | **NEURONET or NEURAL NET** | **NEURAL NET,** HELP to INFO. Vanilla and HDE hold 185 "Neural Net" strings and no player-visible "NeuroNet"; the window under the word reads NEURAL NET LOG | It is the owner's own word since 18 Oct, short, and FLvibed's voice; NEURAL NET is wider on the strip |
| D3 | **One 0.4.0, or a 0.3.1 now** | **0.3.1 now, then 0.4.0 = flight, dialogs, strip, base text; 0.5.0 = grown map.** Issues 10, 22, 23, 25 and more are fixed and waiting on 40 rows | The owner set "every fix and feature first, then P11d, P12". A release costs a gate, and a 0.3.1 ships the split unaudited |
| D4 | **M and T** (item 9) | **Fill on the game's lock only.** Needs a real lock in a probe first (W6) | A lock may be rare, so the letters almost never fill, and the requester asked about range. Reach is a true statement; the loadout's LOCKED chip already keeps that meaning |
| D5 | **The cue's seat** | **Above, on dark cells, over a real bracket frame first** | It covers the target's name when centred. A column beside the ring clears the label and is where MW4 really had it; the player's "above" came from memory |
| D6 | **Radiation, tail, lag** (item 3) | **R5:** amber nav line, ring mark, one small word, none in the stack | Standing cards are consistent with issue 21's "every danger showing"; a ring mark must be learned |
| D7 | **The floor and sizes** (item 1) | **INPUTS 7's table, 0.024 H on Normal, 0.021 H only on Small, with Small keeping Normal's text at 800** | The owner said "no exception", and one floor is simpler to lint and explain |
| D8 | **A first-run look choice** | **One HUD 2 / Classic page in setup.** One page, and it defuses the likeliest resentment | The norm is weaker than it first read: HDE ticks its own widescreen HUD by default and the README announces HUD 2. It adds a step, and the choice is one setting away |
| D9 | **Continue for the wordmark** (reverses 13.3) | **Yes:** one press to resume beats any front-end restyle, on the cost D1 frees | Continue is a feature, kept out of a skin on purpose, and "latest save" has its own failure cases |

## 8. Section 9's twelve items

| Item | Answer |
|---|---|
| 1 Floor, sizes | Owner: D7 |
| 2 Default look | Settled: Normal = today ("a decent medium", no regression) |
| 3 Radiation, tail, lag | Owner: D6 |
| 4 Target keys | Done: 10998491, unreleased |
| 5 Bit 1 for flight roots | One test run: `INPUTS` 4 rows 8-11, then retire the steppers |
| 6 `deck_mode` | Follows 1: retire into `hud_size` |
| 7 Windows 1280x800 | Large: "almost exclusively the Steam Deck" (owner) |
| 8 Label over the card | No hook (decided 29 Oct); read it on the 0.36 card. Moot: the native label goes (section 7) |
| 9 M and T | Owner: D4 |
| 10 Energy meter | 44 degrees, by "only as big"; show the throttle centred on a frame. Shown; owner: form C (section 7) |
| 11 Loading | R17: defer. Overruled; owner: form C, in 0.4.0 (section 7) |
| 12 Travel budget | Metric fault: R14, re-read steady and peak |

## 9. Build order proposed

1. Bug track, ahead of everything: the first-frame flash and the box under windows; the Neural Net column at 800.
2. Probes: dialog classes, a real lock, cue over a real bracket, hit boxes under hidden map controls, probe 18, 0x4177B0, 0x55E120, Alt-Tab, device Reset, mission failure, a story cutscene.
3. Redraw the sheets (section 5), then the owner's answers.
4. The root registry, then each alert's home, then the deny.
5. Flight: `hud_size`, audit's cuts, ring keyline, meter (issue 28), alerts, card distance, K&M rows.
6. Primitives with the grammar table; focus word.
7. Strip, map frame and words around the native map; pad zones; slides.
8. Base text; five windows on one frame.
9. Dialogs.
10. Cue.
11. Next release: grown map, Shadow panels, tooltips, page rule. Optional after: loading, front-end transition, multiplayer tables.

Close each release as `UI_HUD2_SKIN.md` 15's step 10. Second install and one Proton run are gates for the grown map; a test build for issue 10's reporter, on the owner's word, is the cheapest Deck evidence there is.

## 10. Claims that did not survive

| Claim | Fate |
|---|---|
| Ring under 3:1 "almost everywhere" (the lead's rough pass) | Planet only; the sun's casualty is the power arc |
| Docked text will likely miss 4.5:1 (AAA lens) | Wrong: 9-10:1; stroke is the failure |
| `cy - 0.125 k` clears the label (attack) | Wrong: moves the clash |
| SHIELD FAILING's word 2.74-3.16 (attack) | Sim frames freeze the burst; real 3.9-4.65 |
| Mock word 8.96:1, "the owner judged another state" | 6.9:1, and the built pause stood on the same sheet |
| Lower the fill to 28 % (attack) | Reverses a recorded decision; recolour the word instead |
| Kill the rollover in pad mode (attack) | "No safe switch" is on record |
| Shadow loses non-Latin text (scope) | The atlas grows; the open test is the face's coverage, 256 characters, `?` |
| "Four answers" for an unknown window | One real conflict, two side effects |
| Dialog verbs have no route (scope) | Dialog captions are another class; likely route, probe per class |
| Hold-to-buy resented by 12 NMS voices | 8 on topic, on gathering |
| "Lists want focus" from Destiny | The lens's inference, not the write-up's |
| HDE's skins are off unless taken | True of Dark and icons, not its widescreen HUD |
| HUD 2 "unannounced" | The README announces it |
| Ghost residue reads as dirt (lead) | A fade tail, targets go to 0 |
| Two highlights on pause (lead, three readers) | Capture artefact |
| 7 / 9 / 11 px x-heights (attack) | One pixel low: 8 / 10 / 11-12 |
| `'3' / 'JAVELIN'` overlap, K&M only | A 2.6 px gap, on the pad too |
| MW5 cue "only through a mod" (GROUP_CUE 2) | Overclaims: nothing settles vanilla; fix the doc's line |

## 11. Not verified, sources

**Not verified:** any motion; fight entry; how often the tail fires; a real lock; a capital ship under HDE's native bracket, or the card's gap on one; nav on bright; bloom's range; 21:9, 4:3, Focus; any Deck or Proton behaviour; real pad behaviour in the foreground. Overflow counts in R1 are estimates. Contrast on arcs ignores hue.

**Limits of the web evidence:** Metacritic, MobyGames, GameSpot, ModDB, Discovery, Frontier and Spectrum refused, and Reddit came through mirrors, so player counts are small and marked where used. "Sacred" rests strongly on reviews and thinly on today's players. Elite and Star Citizen stand on excerpts. Elite's nested-menu point could not be checked beyond excerpts.

**Key pages:** Xbox XAG 115 on learn.microsoft.com (read whole by the verifier; its exact address was not recorded), [XAG 101](https://devdocs.xbox.com/build/game-principles/accessibility/xag-deep-dives/xag-101-text-display.md), [XAG 102](https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/102), [Steam Deck compatibility](https://partner.steamgames.com/doc/steamdeck/compat), [EVE: Evolution, not revolution](https://www.eveonline.com/news/view/evolution-not-revolution), [EVE Photon feedback](https://forums.eveonline.com/t/photon-ui-general-feedback/380063), [Kotaku on Starfield's menus](https://kotaku.com/starfield-release-menu-ui-starmap-inventory-xbox-review-1850797661), [Freelancer manual](https://download.wcnews.com/files/manuals/Freelancer%20-%20Manual.pdf), [Game Over review](https://www.game-over.com/reviews/pc/Freelancer.html), [Filfre](https://www.filfre.net/2025/12/mr-roberts-goes-to-hollywood-part-2-the-producer), [HDE](https://github.com/FLHDE/freelancer-hd-edition). The full lists: section 6 of `lens_freelancer.md`, 5 of `lens_aaa.md`, 7 of `lens_flavour.md`.
