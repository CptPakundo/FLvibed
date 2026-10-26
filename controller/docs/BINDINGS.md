# Pad bindings: research and presets (18 Oct 2026)

The **binding pass** of `TASK_GLYPHS_HUD.md` (principle 9, phase 1b). User brief: research space and air combat games
across the spectrum, reason pros and cons, and propose a small number of presets that each make sense in their own
way; the current flight binds are placeholders and may change fundamentally. Rules from the user: ergonomic,
context-aware, industry-standard, good flow in the hand; **core functions one button**, two-button chords only for
secondary functions; consumables need better, more intuitive buttons; opening the target picker by holding D-pad up
is doubtful. Menus keep the one grammar (A accept, B back, LB/RB tabs, View = NeuroNet hub, Start = pause).

Status (25 Oct 2026): **superseded by the decided presets** FREELANCER / ACE / ASSISTED of `..\..\docs\UI_SPEC.json`
(`bindings`; design `..\..\docs\UI_PLAN.md` 6.4), built in `..\src\impl\presets.*` (the tables and the tap / hold /
double-tap dispatcher) and run by `controller.cpp` UpdateSpace; the summary is in `..\CLAUDE.md` ("Controls summary").
Where this document differs from UI_SPEC, UI_SPEC wins. The research below (what other games do, the frequency
table, the reasoning) stays as the record behind the decisions.

## 1. What comparable games do (default gamepad layouts)
Sources at the end. "Finger" = triggers / bumpers, usable while both thumbs stay on the sticks.

| Game (type) | Steer / aim | Throttle, strafe | Triggers | Bumpers | Face buttons | D-pad | Stick clicks | Wheels / holds |
|---|---|---|---|---|---|---|---|---|
| **Star Wars: Squadrons** (space dogfight) | RS pitch / yaw | LS Y throttle, LS X roll | LT tap target ahead, **hold = targeting wheel**; RT fire | LB / RB left / right auxiliary (missiles, abilities) | A cycle targets (double = my attacker), **B countermeasures**, X shield focus / power, Y ping (hold = comms wheel) | power: up weapons, left engines, right shields, down balance | **L3 boost (hold = drift)**, R3 free look | wheels on holds; Aviator / Southpaw stick variants |
| **Everspace 2** (space shooter / looter) | RS aim (hold left / right = roll) | **LS strafe + hover**, **LT forward thrust, LB reverse** | LT thrust, RT primary | LB reverse, RB secondary | **B consumable, X device**, Y camera, A confirm | tap = toggle, **hold = wheel** (devices, consumables, weapons) | **L3 boost**, R3 lock target | D-pad wheels; players complain that holding B / X + direction for consumables is fiddly |
| **Rebel Galaxy Outlaw** (Privateer-like, closest in spirit to Freelancer) | LS steer + pitch | throttle on **A up / B down** | LT auto-track target, RT fire | LB lock target, RB MFD / hold = warp | X secondary, Y command menu | weapons / missiles / link fire, power in combat | **L3 boost**, R3 ECM | map: LS pan, A waypoint, triggers zoom |
| **Starfield** (space RPG) | sticks | — | RT / LT weapons | RB flight mode, LB scanner | A select target, B cancel, X ship action / targeting mode, Y weapon 2 | power up / down, systems left / right | L3 boost | — |
| **No Man's Sky** | sticks | triggers speed up / down | — | **LB + RB pulse engine** | B boost, X land | — | L3 scan | many holds (criticised on PC) |
| **EVE: Valkyrie** (VR dogfight) | LS / head | — | **LT hold = missile lock, release = salvo** | roll | A boost, B brake, X anti-missile | — | R3 cancel lock | — |
| **Elite Dangerous** (sim, console default) | LS pitch / yaw (or roll) | RS roll / vertical thrust; throttle on modifiers | — | — | — | — | — | heavy **modifier combos** (e.g. Y+RB heat sink, Y+LB chaff): players report the face button's own action firing too; bumper / D-pad combos work better |
| **Ace Combat 7** (arcade air) | **LS pitch / turn / roll** (Standard), LS roll (Expert) | **RT accelerate, LT decelerate** | throttle | LB / RB yaw | **A gun, B missile, Y change target, X change weapon** | — | **L3 + R3 flare**, R3 view | — |
| **Project Wingman** (arcade air) | LS | triggers (default: turning on triggers, criticised) | — | — | face buttons fire (criticised as counterintuitive) | — | — | flare button shared with the AoA limiter |
| **War Thunder** (sim air, community pad setups) | RS pitch / roll | LS throttle / yaw | RT guns | LB+LT missiles, RB bombs | — | flaps | — | arcade "mouse aim" on the right stick |
| **X4 Foundations** | community: RS turn, LS strafe | boost on LS click; default criticised, since 7.0 radial menus | — | community: roll on bumpers | — | — | L3 boost (community) | radials |
| **Star Citizen / Battlefront II** | community setups only (defaults widely called inadequate); BF2: weapons on triggers, B evade, A land, boost on stick click | | | | | | | |

Patterns across the spectrum:
- **Fire on RT** everywhere (except arcade air, where guns are on A because the right stick is the camera).
- **Boost on L3** in most space games (Squadrons, Everspace 2, Rebel Galaxy Outlaw, Starfield, X4 community); Squadrons'
  **hold boost = drift** is the space equivalent of Freelancer's engine kill.
- **Targeting on a finger** is what the best-received dogfighter does (Squadrons LT: tap target ahead, double tap my
  attacker, **hold for a targeting wheel**). EVE uses LT hold for missile lock. Targeting on face buttons (AC7 Y,
  Squadrons A for cycling) is for secondary cycling.
- **Countermeasures**: Squadrons B, Rebel Galaxy Outlaw R3, AC7 L3 + R3 (a deliberate chord, rare use).
- **Consumables**: Everspace 2 B / X with D-pad wheels (criticised); arcade games avoid them. No standard: room to do
  better with direct one-button access.
- **Throttle**: three families: left stick Y (Squadrons, space dogfighters), triggers (Everspace 2 thrust / reverse,
  AC7, No Man's Sky), face buttons (Rebel Galaxy Outlaw). Strafe on the left stick in shooters (Everspace 2).
- **Power / mode switching on the D-pad** (Squadrons, Rebel Galaxy Outlaw, Starfield): the D-pad is the "quick tap"
  cluster in space games.
- **Modifier combos are the weak point** (Elite: face-button modifiers misfire). Wheels on holds work when the tap
  action stays useful.

## 2. Freelancer's controls and how often they matter
Freelancer flies by **pointing** (the ship turns towards the cursor: the controller's gimbal aim does this with a
stick), has **no roll**, a persistent **throttle lever** plus a **hold-to-reverse**, **afterburner** (hold),
**cruise** (toggle, charges, breaks on combat), **strafe** left / right / up / down (HDE leaves up / down unbound),
**engine kill** (drift) and many one-shot commands. Frequency from the user's 2020 playlist study (15 videos, 7103 s
of fights; HANDOFF.txt): in fights afterburner 29 % of the time (bursts ~1.3 s, 39 % when under fire), reverse 4 %
(12 % in M01; bursts 1.5-3 s; 5-7 forward / reverse switches per minute), cruise 20 % overall; the throttle is used
binary (full forward or full reverse), never partial.

| Tier | Actions | Implication |
|---|---|---|
| Continuous | steer / aim, fire guns | right stick + RT, never interrupted |
| Combat, frequent, needs instant hold | afterburner, reverse, strafe dodge, missiles, targeting (nearest / next enemy) | fingers or the left stick: no thumb lift off the aim stick |
| Combat, situational, timed tap | countermeasures (vs incoming missile), nanobots, shield battery, mines, engine kill (drift) | one button each; a tap may briefly lift the left thumb (the lever throttle keeps its value), not the aim thumb |
| Flight, frequent, calm | cruise, dock / lane / jump with target, tractor loot, target cycling, nav map | face buttons (the aim thumb is free outside fights) |
| Rare | goto, formation, free flight (cancel), scan cargo, torpedo, cruise disruptor, rear view, turret view, weapon groups, contact-list filter, comms, inventory, autopilot goal panel | wheel or chord; also reachable from the View hub |

**Key ergonomic fact for Freelancer:** because the throttle lever keeps its value, lifting the **left** thumb (to the
D-pad) costs only a moment of strafe / reverse, while lifting the **right** thumb (to the face buttons) takes the guns
off the target. So in combat the **D-pad is cheaper than the face buttons** — the opposite of games whose left stick
is a spring throttle. Consumables belong on the D-pad, as one-button taps.

## 3. Presets
Three presets plus two global variants. Each preset keeps the menu grammar and A = context in flight (dock, land,
lane, jump, tractor the loot in reach: whatever applies to the target / situation, shown by the contextual world
prompt).

### Preset 1 — **Dogfighter** (recommended default; Squadrons + Everspace 2, adapted to Freelancer)
| Input | Tap | Hold / variant |
|---|---|---|
| Right stick | aim / steer (gimbal) | — |
| Left stick Y | throttle lever up / down | **hard pull = reverse** while held (lever kept) |
| Left stick X | strafe left / right | — |
| RT | fire guns | (hold) |
| LT | **target**: nearest hostile ahead | double tap = **target my attacker**; **hold = target picker** (wheel / list, release on a row to select) |
| RB | **afterburner** (hold) | — |
| LB | fire **missile** (selected secondary) | — |
| A | **context**: dock / land / lane / jump with target, else tractor loot in reach | hold = **goto** target (autopilot-free game maneuver) |
| B | **cancel** maneuver (free flight) | hold = rear view |
| X | tractor all loot | hold = **formation** with target |
| Y | **cruise** toggle | hold = **action wheel** (sticky selection): scan cargo, turret view, torpedo, cruise disruptor, weapon group, contact filter, comms, autopilot goals |
| D-pad ↑ / ↓ | **nanobots / shield battery** | — |
| D-pad ← | **countermeasures** | — |
| D-pad → | **mine** | — |
| L3 | **engine kill = drift** while held (option: toggle) | — |
| R3 | next hostile (cycle) | hold = previous |
| View | NeuroNet hub (map first) | — |
| Menu | pause | — |
| L3 + R3 | autopilot on / off (deliberate chord, rare) | also in the action wheel |

Why: everything combat-critical is on fingers or the left stick (reverse and strafe on the same thumb that holds the
throttle, targeting and missiles on the left fingers, afterburner on the right finger), consumables one tap each on
the cheap D-pad, calm actions on the face buttons, rare ones in one wheel. Squadrons' praised LT targeting, Everspace
2's strafe stick, the space-standard L3 boost slot used for Freelancer's drift. **Cons:** targeting leaves the D-pad (muscle memory for current testers); "hard pull
= reverse" needs tuning (threshold, hysteresis) to avoid accidental reverse; holds on A / B / X are B-tier inputs
(they have tap meanings that stay useful, so the hold is optional).

### Preset 2 — **Ace** (arcade flight feel: Ace Combat / Rebel Galaxy Outlaw)
| Input | Tap | Hold / variant |
|---|---|---|
| Left stick | aim / steer (gimbal) | — |
| Right stick | strafe (left / right / up / down) | R3 = rear view (hold) |
| RT | throttle: analog up to 100 %; **full press = afterburner** | — |
| LT | brake; **full press = reverse** | — |
| A | **fire guns** (hold) | — |
| B | **fire missile** | — |
| Y | target nearest hostile | double = my attacker; **hold = target picker** |
| X | context (dock / lane / jump / loot) | hold = goto |
| RB | next hostile | hold = previous |
| LB | **countermeasures** | — |
| D-pad ↑ / ↓ | nanobots / shield battery | — |
| D-pad ← / → | cruise toggle / mine | — |
| L3 | engine kill (drift, hold) | — |
| View / Menu | hub / pause | — |
| L3 + R3 | autopilot on / off | action wheel: hold D-pad → |

Why: for players who think "one stick flies, triggers are the throttle, thumb buttons shoot" (AC7's Standard layout,
Rebel Galaxy Outlaw's stick-steer). Throttle and afterburner / reverse become one analog motion per trigger.
**Cons:** guns on A means the right thumb fires instead of aiming — works because aim moves to the left stick, but
strafe (right stick) and firing compete for the same thumb; consumables on the D-pad now cost steering (left thumb),
so they are slower in a fight than in Dogfighter.

### Preset 3 — **Assisted** (one hand, accessibility, casual)
The Advanced Autopilot flies; the player aims and decides (the "Turret Mode with the autopilot" idea in
`..\CLAUDE.md`, the Mario Kart smart-steer / auto-accelerate pattern from the accessibility research).
| Input | Action |
|---|---|
| Right stick | aim (the autopilot handles range, afterburner, reverse, evasion, countermeasures, repairs, loot) |
| RT | fire guns |
| RB | fire missile |
| A | context / accept the autopilot's suggestion |
| B | cancel / hand back manual flight |
| Y | target nearest hostile (hold = picker) |
| X | goal panel (autopilot actions / queue) |
| Left side | unused by default (Southpaw mirrors everything to the left hand) |

Why: the whole game is playable with one hand and a single stick; matches SpecialEffect's top input tier (single
presses, one stick). **Cons:** depends on the autopilot's combat quality and the Turret Mode work; less control by
design.

### Global variants (any preset)
- **Southpaw**: swap the sticks (and for Assisted, mirror all buttons) — as Squadrons offers.
- **Holds ↔ toggles**: afterburner, reverse, drift, rear view, target picker (accessibility).
- **Xbox Elite / paddles** (the user's pad): XInput does not report paddles separately, so paddles are mapped in the
  Xbox Accessories app to existing buttons. Suggested mapping for Dogfighter: P1 → D-pad ← (countermeasures), P2 → D-pad
  ↑ (nanobots), P3 → D-pad ↓ (battery), P4 → Y (cruise): consumables then need no thumb lift at all.

## 4. Comparison with the placeholder
| Placeholder (today) | Dogfighter change | Reason |
|---|---|---|
| D-pad: nearest / clear / cycle targets | → LT (tap nearest, hold picker), R3 cycle | targeting is frequent in combat: on a finger; frees the cheap D-pad |
| LB layer D-pad: nanobots / battery / CM / mines | → one-button D-pad | user rule: core functions one button |
| LT missiles | → LB | LT becomes targeting (Squadrons model) |
| B hold reverse | → left stick hard pull | reverse belongs to the throttle thumb; B = cancel (grammar) |
| A dock, X loot, Y cruise | A context (dock or loot), X loot, Y cruise (+ hold wheel) | context A matches the world prompt |
| L3 engine kill toggle | L3 hold = drift | Squadrons' drift, usable while turning |
| LB + L3 autopilot | L3 + R3 (and the wheel) | chord kept rare and deliberate; LB is now a weapon |
| LB layer (8 actions) | Y-hold action wheel for rare actions | one secondary layer, sticky selection, no misfires |

## 5. How to decide and verify
1. The user picks a preset (or a mix) from this table and the diagram page (`research\mockups\bindings.html`).
2. Implement presets as data (`[Bindings] preset = dogfighter | ace | assisted`, every slot overridable), the HUD
   tray / prompts read from it (TASK_GLYPHS_HUD principle 5).
3. Verify: virtual-pad tests per preset (each command fires once, holds / taps / double taps / hard pull recognised,
   no misfire between tap and hold), then **(user)** feel tests on the Elite pad in a free-roam fight (SaveC702) and a
   dock / lane / map round trip; log button usage (counts and timings per action) during the user's sessions to see
   which presses are slow or missed.
4. Optional footage pass: controller gameplay clips (Squadrons, Everspace 2, Rebel Galaxy Outlaw, AC7) downloaded by
   the user, studied silently like the UI videos, to measure how often players press what in comparable games.

## 6. Open questions
1. Which preset should be the default (proposed: Dogfighter)?
2. Reverse on the left stick's hard pull (Dogfighter) — acceptable, or keep a held button?
3. LT tap / double tap / hold for targeting — does it feel right, or should the picker get its own button?
4. Should the action wheel (rare actions) be on Y-hold, or on another hold?
5. Paddle suggestions for the Elite pad — useful to document for players, or keep paddles out of the defaults?

## Sources
- Star Wars: Squadrons controls (Xbox): https://outsidergaming.com/star-wars-squadrons-complete-controls-guide-for-ps4-and-xbox-one/ ;
  EA accessibility (hold / toggle wheels): https://ea.com/able/resources/star-wars/star-wars-squadrons/xbox-one/controller-settings
- Everspace 2 Xbox controls: https://www.magicgameworld.com/?p=47828 ; player feedback on consumables:
  https://steamcommunity.com/app/1128920/discussions/0/3814034217016730186
- Rebel Galaxy Outlaw Xbox controls: https://www.magicgameworld.com/?p=37242
- Starfield ship controls: https://twinfinite.net/starfield/starfield-controls-guide-for-xbox-pc/ ,
  https://sportskeeda.com/esports/starfield-spaceship-guide-best-control-power-allocation-combat
- No Man's Sky flight controls: https://carlsguides.com/nomanssky/controls.php
- EVE: Valkyrie controls coverage: https://www.roadtovr.com/eve-valkyrie-now-supports-oculus-touch-gamepad-alternative/
- Elite Dangerous pad bindings (community): https://forums.frontier.co.uk/threads/elite-dangerous-default-context-controller-bindings-helpcard.215704 ,
  https://forums.frontier.co.uk/threads/xbox-controllers-and-chaff.466669
- Ace Combat 7 controls: https://shacknews.com/article/109486/ace-combat-7-controls-playstation-4-and-xbox-one ;
  Standard vs Expert: https://www.bandainamcoent.com/pt_br/news/8-steps-to-fly-towards-victory-in-ace-combat-7
- Project Wingman control impressions: https://scientificgamer.com/thoughts-project-wingman/
- War Thunder pad setups (community): https://steamcommunity.com/app/236390/discussions/0/3773490849739893262
- X4 gamepad (community): https://steamcommunity.com/app/392160/discussions/0/4626981776940679441
- Star Wars Battlefront II starfighter controls: https://www.gamerevolution.com/?p=36658
- Accessibility input tiers and assists: `research\ADAPTIVE_UI_RESEARCH.md` section 2.4
