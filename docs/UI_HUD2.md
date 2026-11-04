# HUD 2: the ground-up pad HUD (approved and BUILT 25 Oct 2026; as built: 5.8-5.11)

The owner (25 Oct 2026 ~08:30), after the pad HUD round 2: "a smart balance between B and C ... be daring, and
completely change the way the space HUD looks ... from the ground up ... a modern looking and feeling HUD that has the
Freelancer flavor, that makes use of Freelancer assets where applicable, but is otherwise simple, intuitive yet
completely and utterly fully functional as well as what we've designed so far." This document is that design: what
HUD 2 shows, where, in which state, from which data, and what is missing to build it. Status: **built** (approved by the owner 25 Oct 2026 ~08:30, section 4; built and
verified the same day while the owner was away, 5.5-5.11; the as-built page is artifact `D6XnaBvWLKHARfdvTQuj3Y` v2 with
the release captures of `research\experience\runs\hud2_fix2_2026-10-25\`). History of the design: the
mock-ups (`tools\uimock\hud2*.py`, renders in `research\evidence\controller_ui\hud_redesign\hud2\`; three
iterations, each judged by an independent critique: `research\evidence\controller_ui\hud2_critiques.md`) and the
proposal page (artifact `D6XnaBvWLKHARfdvTQuj3Y`) carry it until the owner decides; then it goes into the living mock-up and `UI_SPEC.json` like any UI change
(`tools\uimock\README.md`), and this file becomes the build brief.

Research: `docs\research\controller_ui.md` (rounds 2 and 3; evidence `research\evidence\controller_ui\hud_*.md`).

## 1. Principles (each from the research and the two critiques; the build uses fractions of H)
1. **One HUD drawn by FLvibed in pad mode, the game's HUD hidden whole** (the HUDless switch the game already has);
   the same game state, both devices live: a mouse move or key brings the native HUD back in one frame (decision 13).
2. **Status where the eye is**: shield, hull and power as thick segmented arcs on a small ring around the reticle
   (radius 0.10 H; Starfield's third-person ring, Destiny's "one glance down", the York ammo study); detail in the
   corners. Arcs >= 0.0065 H thick (Armored Core VI's thin meters were the top complaint); nothing crosses the speed.
3. **Subtraction**: each value in one place; plates only where text must hold over bright scenery (the target card,
   the ship and loadout panels, the alert, the context pills); the top-left is free text on a soft scrim; boxed area
   about 10 % of the screen (the native HUD ~22 %).
4. **Corners by job**: top-left where you are going (the objective as the headline, the maneuver as a chip), top-right
   what you are looking at, bottom-left your ship, bottom-right what you fire and use; the comm feed left-middle (where
   FLvibed's subtitles already sit); the context line under the player's ship; one transient alert top-centre.
5. **Freelancer's own art and language**: HDE's flat icon sheets (`buttontextures_flaticons.txm`: maneuvers, contact
   types, the starburst and the spiked sphere for flares and mines), the bracket / arrow art (`HUD_targetingelement`),
   the wireframe views of the target and of the player's ship, the segmented gauge blocks, the palette (navy glass,
   light-blue chrome, cyan, active yellow, hostile red, friendly green), Agency FB.
6. **Symbols with one meaning**: diamonds = objectives; the lead pip = a ring with a dot (filled = in gun range);
   every off-screen direction on ONE ellipse (0.62 x 0.40 H) as the game's arrow in the stance colour with the
   distance; missile bearing = red chevrons on the ring; a hit's side = a transient amber flash at 0.13 H.
7. **Shape is the label; glyphs only where they teach**: the D-pad cross, the ring's sides; one glyph per cluster
   (RT / RB, LB / R3); the context line (at most three, never one a panel shows, holds as a broken ring + the small dim word HOLD BEFORE the
   glyph - the owner, 26 Oct 2026; where that would cost a prompt: above the glyph, over the label, one shared word, the
   outline alone - the prompt pills the same); alerts name
   their answer as glyphs and the slot they name lights up (and stays lit after the 3 s card).
8. **State shapes the HUD**: calm dims the ring to 40 % (full arcs show no numbers) and the side panels to 60-65 %;
   combat brings everything to full with the threat list and escorts; cruise charging fills the bottom arc in cyan
   with the seconds; docking / lane / jump / autopilot show the maneuver chip with progress and ETA.
9. **Sizes**: no text under 0.024 H (XAG 101 console 26 px at 1080p), glyphs 0.032-0.036 H; free text with a navy
   outline; `text_size` scales all.

## 2. Layout (16:9; one safe margin: 5 % of W at the sides, 5 % of H top and bottom; tops on one line)
| Zone | Element | Contents |
|---|---|---|
| top-left | NAV (no box) | the objective as the headline (yellow diamond) with the protected ship's hull bar; the maneuver chip while one runs (its flat icon, name, object; AUTOPILOT chip when FLvibed flies) with progress and ETA; system and zone for 5 s after a jump; NeuroNet (View, a dot for news) |
| top-centre | ALERT | one transient card (3 s), 0.05 H: the event in its colour + the answering buttons as glyphs (SHIELD FAILING · D-pad down BATTERY · D-pad left FLARE); skipped when a comm line says it |
| top-right | TARGET card | the game's wireframe of the target (else its flat type icon), attitude chip, class, level, name, faction, distance, shield and hull bars with numbers (only for what can be damaged); in combat the threat list under it (LB TARGETS, R3 NEXT, the filter, the next two hostiles) |
| centre | RETICLE RING | radius 0.10 H: shield left (red at <= 25 %), hull right, power below (cyan while cruise charges, with the seconds); the speed inside; a number at an arc's end only below 40 %; missile chevrons and the hit flash outside; the game's own reticle in the middle |
| around | DIRECTIONS | one ellipse (0.62 x 0.40 H): the target and every hostile off the screen, the game's arrow in the stance colour, distance under it |
| world | target tag | the game's corner brackets in the attitude colour, the distance under them, the lead pip, the missile lock closing around them |
| left-middle | COMMS | the subtitles module's lines: speaker in the attitude colour, faction, the line wrapped at 0.30 W |
| bottom-left | SHIP | the game's wireframe of the player's ship (damage shows on it); in combat the escorts by their comm call signs with hull bars, otherwise the hold and credits |
| bottom-centre | CONTEXT line | under the player's ship: at most three actions on dark pills |
| bottom-right | LOADOUT | the D-pad cross (nanobots up, batteries down, countermeasures left, mines right, counts in the slot corner; a slot lights in its alert's colour) and the weapons: RT gun groups with ">" and yellow on the active one (Y held cycles), RB the launcher, ammo and LOCKED |

## 3. Data: what FLvibed reads today (the code map of 25 Oct 2026) and what is missing
| Element | Source today | Gap |
|---|---|---|
| hull, shield, power, nanobots, batteries, hold | `ai::ReadSelf` (`core\game\ai.cpp`), `space::ShieldOf` | shield "rebuilding" is derived (down + the rebuild time; `shieldcue.cpp` has the edges) |
| speed, throttle, cruise off / charging / on + charge time | `fl::PlayerKinematics`, `fl::GetThrottle`, `flight::ReadModel` | **afterburner firing** and **engine kill**: no reader |
| maneuver, lane, jump, dock, formation | `space::ReadPilot`, `space::PlayerControlExclusion`, `ai::DockInProgress`, host `DockReply` | lane disrupted only as a result code |
| objective, waypoint | host `ObjectiveNow` / `ObjectiveSteps`, `space::Waypoints` | - |
| system / zone name | system id (`exe::PlayerSystem`) | **display names**: universe.ini `strid_name`, the zone's `ids_name` |
| target name, attitude, distance, hull, shield | `fl::TargetId`, `space::SpeakerName`, `space::Contacts`, `space::ShieldOf` | **faction text** lives in the subtitles module (`names::Lookup`): move to core; class string from the ship class bits |
| target wireframe | the game's target view (3D) | FLvibed draws no meshes: **keep the game's own view** inside the card (its flag 0x679C18 and the target panel's view mode are known from DynamicTargetScreen, unverified) or fall back to the flat type icon |
| own-ship wireframe | the game's status view (3D) | the same question as the target's |
| threats | `space::Contacts` (attitude, targetsPlayer, distance) | - |
| weapons, ammo, missiles, CM, mines | `space::PlayerWeapons`, `PlayerCountermeasures`, `PlayerMines` | **weapon display names** (equipment `ids_name`), **group membership** and the **active group** (the game keeps them; not read yet) |
| world brackets, arrow, lead | `fl::WorldToScreen`, `fl::TargetLeadPos` | does the HUDless switch hide the game's brackets too? (HUDless's notes say yes) - then FLvibed draws them with the game's art |
| comms | the subtitles module (`display.cpp`) | its HUD 2 slot (left-middle; a `space_position` value) |
| notifications | `nn-message` events, shield edges, dock replies, cargo | a queue |
| credits | `space::PlayerWorth` internals | a getter (small) |
| hide / show the native HUD | HUDless's bytes 0x679C0C / 0x679C10 / 0x679C40 and the comm / chat sites; `exe::UiRenderSkip` | **sign them** in `flexe.h` and verify in game what each hides (brackets, reticle, damage arcs, comm window, mission banner) |

## 4. The owner's go (25 Oct 2026 ~11:00) and notes
"I can go with your recommendations for the four decisions" (HUD 2 the pad default with CLASSIC kept; the target card
top right; the ring shield left / hull right / power below; fading when calm) plus:
- **A quick transition** between the keyboard-and-mouse HUD and the controller HUD: a few frames at most, for visual
  continuity.
- **The character portraits** and everything the subtitles module does (the dialogue panel, the comm portrait, the
  chat column, the Neural Net lines, every placement and style option of its menu) keep working, adapted to HUD 2.
- **Fade what is not needed** (calm travel minimal), combat turns it all back on.
- **HUD 2 for keyboard and mouse too**, as a fresh alternative to the game's HUD.
- **More contextual information and visualisation** (as the trade lane's progress), where it does not clutter.
- "Be careful of costly actions like re-hiding HUD elements every frame ... keep things light and efficient."
- "HUD 2 must support the entire game AND all new features!"

## 5. Build: contracts, rules, ownership, coverage

### 5.1 Contracts
- **The HUD bus** (`core\hud_bus.h`, one instance in FLvibed.dll; controller HostApi v21 / subtitles HostApi v8
  `GetHudBus`): HUD 2 publishes `HudLayout` every flight frame (active, kbm, alpha, scale, the rectangles of its parts in
  fractions of the screen, `topFree` for the dialogue panel, `chatFree` for the chat column); the subtitles logic reads
  it and publishes `HudSubtitles` back (where its dialogue panel and chat column landed) so HUD 2's alert goes below the
  panel. Stale (no publish for > 2 frames) = HUD 2 absent: both fall back to today's layouts.
- **The native HUD switch** (`controller\src\impl\hudvis.*`, logic): HUD 2 hides the game's own panels - the maneuver bar
  (with the cruise box and FLvibed's autopilot button), the NeuroNet box, the target panel, the contact list, the status
  / weapon panel, the gauges (HUD_Core) - by their root controls' state byte (+0x6C bits 0|1; `docs\FREELANCER_RE.md` UI).
  What stays native: the comm window (the portrait the subtitles panel hosts), the objective banner (the subtitles
  move it), the target indicators (brackets, off-screen arrows, the lead marker, the target's bars over its bracket:
  `VtHudIndicators`, not a named control), the damage arcs, the cursor / reticle. Measured 25 Oct 2026 (instance 8,
  SaveC705): hiding a root by its state byte holds for the status, target, NeuroNet and gauge roots; the game re-shows
  the maneuver bar's and the contact list's roots itself every frame.
- **The transition**: the native panels go (or return) on the switch frame; HUD 2 fades in (out) over `hud_transition`
  ms (default 120, ~7 frames at 60 fps) with a short slide (0.01 H) toward its corner: the continuity the owner asked
  for without drawing both HUDs together.

### 5.2 Efficiency rules (the owner: "keep things light and efficient")
- **No per-frame searches.** The root controls are found once (by a named child -> parent chain, `ui::Collect` or
  FindControl) and cached; revalidated only when the HUD is rebuilt (a game load, a launch, entering space; a cheap
  check of the cached root's name / vtable at most every 2 s). The per-frame work for the switch is a compare and a byte
  write on about six cached roots, done in `OnRenderUnderHud` (before the UI pass draws). Restore = put back the bits.
- **Data at its own rate**: speed, throttle, shield, hull, power, cruise charge per frame (cheap reads); contacts /
  threats, names, weapons, cargo, escorts at 10 Hz (cached); system / zone / faction / equipment names cached by id.
- **Drawing**: one render pass for HUD 2 (Begin .. End), fonts built once per size (rebuilt on `hud_scale` / screen
  height change), text widths cached per string, geometry as boxes / quads (arcs as quad strips, ~4 quads per segment);
  nothing drawn at alpha 0; the fades are cheap per-element multipliers.
- **Budget**: HUD 2 Update + Render under 0.3 ms per frame at 4K on the dev machine (`perf cpu` reports OnFrame /
  OnRenderOverlay; record it in the verification).

### 5.3 Settings (`[Controller]`, the options page's CONTROLLER tab, section HUD; spec keys = registry keys)
| Key | Type | Default | Meaning |
|---|---|---|---|
| `hud_style` | int 0..2 | 1 | 0 CLASSIC (the game's HUD; in pad mode the badges and legend as built), 1 HUD 2 ON THE PAD (keyboard and mouse keep the game's HUD), 2 HUD 2 ALWAYS |
| `hud_fade` | int 0..2 | 1 | 0 ALWAYS FULL, 1 CALM DIMS (ring 40 %, panels 60 %), 2 MINIMAL (calm flight hides the side panels and the ring; anything that changes brings its part back for a few seconds) |
| `hud_opacity` | int 40..100 | 100 | HUD 2's overall opacity, % |
| `hud_scale` | int 80..130 | 100 | HUD 2's size, % (text never below the 26 px floor at 1080p at 100 %) |
| `hud_numbers` | int 0..2 | 0 | the ring's numbers: 0 when low, 1 always, 2 never |
| `hud_threats` | bool | 1 | the threat list under the target card in combat |
| `hud_context` | int 0..1 | 0 | the context line under the ship: 0 contextual, 1 off |
| `hud_alerts` | bool | 1 | the transient alert cards (shield failing, missile, cargo full, ...) |
| `hud_info` | bool | 1 | the contextual information (lane progress, jump and dock approach, loot in reach, scans, formation, mission timers) |
| `hud_wireframes` | bool | 1 | the game's wireframe views in the target card and the ship panel (where the game's view can be kept; else the flat icons) |
| `hud_transition` | int 0..400 | 120 | advanced: the switch's fade in ms |
CLASSIC keeps today's rows (`hud_glyphs`, `button_legend`, `ap_status_line`, `world_prompts`) meaning what they mean.

### 5.4 Coverage: everything the game's HUD and FLvibed do in flight, and where HUD 2 has it
| Function | Today | HUD 2 |
|---|---|---|
| Maneuvers free flight / goto / dock / formation (+ lane, jump) | maneuver bar, keys, pad A / B / hold A | the maneuver chip (top-left) with progress / ETA; pad as today; K&M: the chip row is clickable (the game's own commands) + keycaps |
| Cruise (charge, engaged, disrupted) | cruise box, Y | the ring's bottom arc charges cyan; under the ring always the cruise row ([Y] CRUISE dim / CHARGING / CRUISE ON, 26 Oct 2026); alert on disruption |
| FLvibed autopilot (start / stop, mode, wait reason, goals) | bar button, key K, View hold, status line, goal panel (X) | AUTOPILOT chip + mode + wait reason in the nav; the goal panel, picker and wheel draw over HUD 2 as today; K&M: the chip clicks |
| NeuroNet (map, inventory, log, player info, help) + news flash | NeuroNet box | NeuroNet chip (View / key; K&M click opens the map; the log / inventory keys as the game's) with the news dot |
| Target: name, faction, attitude, class, level, distance, hull, shield, 3D view | target panel | target card (+ the game's wireframe where it can be kept, else the flat icon) |
| Target cycling, next enemy, nearest, clear, under the reticle | RS click / picker / keys / contact list | RS click = the nearest object inside the ring (26 Oct 2026), LB picker, the threat list; [RS] x2 CLEAR on the target card; LB TARGETS / WHEEL hints in the card's corner while no target (faded outside combat); K&M: list rows click to target |
| Contact list with filters | contact list panel | the picker (pad), the threat list; K&M: a contact list mode of the threat list (a key / click toggles it, the filters as the picker's categories) |
| Hail / scan cargo / tractor / formation request | target panel buttons, keys, wheel | the wheel (pad), keys (K&M); scan results and loot in reach as contextual info |
| Own hull / shield / power | gauges | the ring (+ numbers when low), the ship panel's wireframe |
| Speed / throttle / afterburner / engine kill / reverse | the gauges' boxes | the speed inside the ring, throttle / burner / kill at the ring's bottom centre (26 Oct 2026; was 5 o'clock) |
| Weapons, groups, ammo, toggling | weapon list panel, group buttons | the loadout (groups with ">", the launcher and ammo); K&M: groups click to select, the keys as the game's |
| Nanobots, batteries, countermeasures, mines | status panel buttons, D-pad | the D-pad cross (pad) / keycaps (K&M), counts, lit by alerts |
| Damage direction | the game's arcs | the game's arcs (kept) |
| Off-screen target / objective arrows, brackets, lead | the game's indicators | the game's (kept), their keep-outs = HUD 2's rectangles |
| Objective banner, objectives | the game's banner | the banner (kept, placed by the subtitles) + the objective line in the nav |
| Comms with portraits, chat, Neural Net lines | the comm window, the subtitles panel and columns | the subtitles module's panel with the portrait (top-centre, between nav and target) and columns, through the bus |
| Turret view, rear view | X hold toggles (26 Oct 2026), RS hold | the context line and a TURRET chip in the nav; in turret view the reticle stays centred and RS turns the camera |
| Formation members, escorts | - | the ship panel's escort list in combat / formation |
| Cargo hold, credits | inventory | the ship panel (calm) |
| Trade lane, jump gate / hole, docking approach, dock queue | - (the game's lines) | contextual info in the nav: rings left, ETA, queue position, jump countdown |
| Mission timers / countdowns | the game's text | contextual info (when the game exposes one) |
| Speedrun timer (speedrun mode) | under the NeuroNet box | under the target card (top-right) |
| Picker, action wheel, goal panel, prompt bar | over the HUD | over HUD 2 (HUD 2's context line steps aside while a modal shows its bar) |
| Shield / hull / reticle cues | FLvibed cues | as today |
| Device switch, glyph families, presets, holds-as-toggles | built | HUD 2 follows them (glyphs per family; K&M keycaps from the keymap) |

### 5.5 Tracks (parallel agents in this checkout: disjoint files; `docs\UI_BUILD.md` 2's rules apply)
| Track | Owns | Delivers |
|---|---|---|
| **BUS** (done, orchestrator) | `core\hud_bus.*`, `build.cmd` :link, both hosts' `GetHudBus`, both `api.h` | the bus |
| **READERS** | `core\game\hudinfo.{h,cpp}` (new), `core\flexe.h` (one block "HUD 2"), `docs\FREELANCER_RE.md` / `docs\kb` facts | the missing readers (5.6), each verified in game |
| **HUD2** | `controller\src\impl\hud2*.{h,cpp}`, `hudvis.{h,cpp}`, `controller.cpp` (integration + the 5.3 rows in kOptions, `uiopt`), `padhud.cpp` (gating only), `runtimer.cpp` (placement only), `core\game\flicons.{h,cpp}` (the flat icon atlas from the game's txm) | HUD 2 itself |
| **SUBS** | `subtitles\src\impl\*` | the subtitles module on the bus (5.1) with every existing option still working |
| **SPEC** (orchestrator) | `docs\UI_SPEC.json` through the mock-up, `tools\uimock\*`, `core\ui_meta.h`, `docs\SETTINGS.md`, this file | the options page rows, the living spec, the docs |

### 5.6 Readers HUD 2 needs (READERS; `core\game\hudinfo.h`, cheap, cached, fail closed)
The active weapon group and each weapon's group membership; weapon / equipment display names (ids_name); the system's
and the zone's display names; the target's faction (the reputation group's short name), its class string and level;
afterburner firing; engine kill on; credits; tractor beam active and loot in reach (count); the trade lane run (rings
left, the destination ring / object, ETA); jump gate / hole sequence state; the docking state (approach, queue position,
bay); cruise disrupted; missile lock progress on the target (if the game keeps it); formation leader and members; a
mission timer the game shows (if any). Each: the API, the address / structure (signed in `flexe.h`), the in-game
verification, the fact in `FREELANCER_RE.md`.

### 5.7 Verification (every track; `docs\TESTING.md`)
Background instances only (READERS 5, HUD2 6, SUBS 7), the virtual pad, frames AND the live log; 4K first, then 1280x720,
2560x1080, 1600x1200; every state of `hud2_states.py` reproduced in game (calm travel, cruise charging, combat with the
alerts, autopilot docking, trade lane) plus turret view, a jump, a mission comm with a portrait, the picker / wheel /
goal panel over HUD 2, speedrun mode, the device switch both ways (the transition), K&M HUD 2; full-resolution crops
against the mock-ups; `perf cpu` within budget; release build; close every game instance.

### 5.8 Integration test (25 Oct 2026, the INTEGRATE track; instance 5, SaveC705 / SaveC703; evidence
`research\experience\runs\hud2_integrate_2026-10-25\`, the showcase set `final_*.png`)
Built in this pass: names as the game shows them (`hudinfo::DisplayName`, "Waypoint" for a route waypoint); the lane bar
over the whole ride; the place line = the game's arrival line (the house by the game's table or HDE's Territory.dll,
Zone.dll's zone edges; the native line hidden while HUD 2 shows); a jump as JUMP from the gate approach to the jump-in,
never DOCK at the arrival; the game's 3D views in the target card's and the ship panel's left column (`hud_wireframes`:
a dark backing under them, the target panel run one frame shown per new target; else the flat icons); the subtitles leave
out the Neural Net lines HUD 2 shows as alerts (`HudLayout.alerts`); the list steps back under the flight modals; the
missile chevrons outside the hit flash's band. Coverage (5.4) checked in game, pad form at 4K unless noted:
| Item | Result |
|---|---|
| Combat: alerts (SHIELD FAILING + BATTERY, MISSILE INCOMING, HULL CRITICAL), chevrons, threat list, the target view | pass |
| Calm travel (ring 40 %, panels 60-65 %), cruise charging (cyan arc + seconds, no native line) | pass |
| Dock with the FLvibed autopilot (`ap start dock`: AUTOPILOT + DOCK FORT BUSH, REQUESTED -> GRANTED) | pass |
| Trade lane (6 and 3 rings, progress 0.01 -> 1, ETA, PLANET MOJAVE) | pass |
| Jump gate both ways (JUMP CALIFORNIA, QUEUE / GRANTED, 0:12 countdown, "CALIFORNIA SYSTEM, LIBERTY SPACE.") | pass |
| Turret view (state turret, EXIT TURRET), rear view (HUD 2 stays, no chip: none specified) | pass |
| Comms with a portrait in topFree (Fort Bush's dock answer, a police officer in the K&M form) | pass |
| Picker, action wheel (CONTACT FILTER -> IMPORTANT / SHIPS on HUD 2's list), goal panel above the bottom panels | pass |
| Speedrun timer (livesplit start) under the list | pass |
| Device switch both ways (t 0 -> 1 / 1 -> 0 in ~120 ms; the roots hidden / restored) | pass |
| K&M form (hud_style 2, input_mode 2; `uibtn click 106` list header, `112` a row), keycaps | pass |
| CLASSIC (hud_style 0: every root, the arrival line and the views' rectangles restored, padhud back) | pass |
| 1280x720 borderless, 2560x1080 and 1600x1200 windowed, 4K borderless - all in the release build | pass |
| `perf cpu` at 4K in a fight: hud2.Update 22 us + hud2.Render 68-97 us; OnRenderUnderHud (switch, backing, the game's view draws) 40-65 us | pass |
Open: the picker's NAVIGATION rows name unvisited gates / holes ("NEW YORK -> COLORADO", picker.cpp); a trade lane ring
reads "Trade Lane Ring" where the classic panel names its lane ("Planet Manhattan->Fort Bush"); the game's missile
warning icon (red, over the target card's top-left corner in a fight) is native and not kept out; the K&M card's
NEXT ENEMY keycap sits in the view's column; one 10-15 ms UnderHud frame was seen at 2560x1080 / 1600x1200 (not at 4K,
not repeated: likely the game loading a view's mesh). (All five fixed in 5.9.)

### 5.9 Fix round after the in-game critique (25 Oct 2026, the FIX track; instance 5; commits 15f5ffc, fe1383c, 95dea02 and the docs; evidence `research\experience\runs\hud2_fix_2026-10-25\`)
The critique of the built HUD (`research\evidence\controller_ui\hud2_critiques.md` "Round 3") item by item:
| Item | What changed |
|---|---|
| 1 NAV | the objective headline re-read whenever its text changes (a mission save loaded after another kept the old sequence: empty headline); the protected ship's hull from the objective's refs, else the weakest friendly ship among the mission's objects (`autopilot::MissionObjectIds`); a standing dim line under it: system + zone, else system + house ("CALIFORNIA · TAHOE ICE CLOUD", "NEW YORK · LIBERTY SPACE", parsed from the game's own arrival line), replaced 5 s by an arrival / zone event, left out in a fight. **News dot: left out** - the NeuroNet's flash state was not found this round (the box's owner keeps its buttons at +0x370..+0x3A4, `NNMissionLogButton` +0x378 / `...Root` +0x394; no blink field identified); `Model.news` and its drawing are in place for when it is |
| 2 comms | the ambient chatter (chatter 0) drawn by the subtitles module in HUD 2's chat band while HUD 2 shows (`display.cpp` DrawHud2Feed): at the 0.05 W margin, Agency FB name / faction / line at HUD 2's sizes (line 0.034 H, name 0.030 H), a navy scrim and a 0.003 H accent bar in the speaker's colour, newest lowest, three lines; **one comm surface at a time**: the feed hides while the top panel shows a line and its lines wait (time starts when shown; dropped after 6 s unseen); the game's chat window (`MsgWin`) hidden under HUD 2, kept while the chat input (`CTID`) is open; the top panel's text in Agency FB too while HUD 2 shows. **Faction: decided against a change** - it already is the live reputation (server.dll GetRep -> GetAffiliation -> GetShortGroupName and GetAttitude): "Escort 1 Rookie Emerson · Rogues" in red was a hostile Rogue wing whose formation role is "Escort" (seen again: "Off. Gordon Lampkin · Police" red once Liberty was hostile) |
| 3 alerts | every card carries its answer glyph or an amber none state: NO COUNTERMEASURES · EVADE (no launcher) / NO FLARES · EVADE (empty), NO BATTERIES / NO NANOBOTS · BREAK OFF, CRUISE DISRUPTED + the cruise button RECHARGE, CARGO HOLD FULL · NO ROOM FOR LOOT; an empty slot lights amber |
| 4 escorts | the ship panel in a fight / formation: the formation's members, then the friendly ships among the mission's objects (M03: "Transport Leader", "Transport 2", "Juni"), then friendly ships fighting within 3 km; names by `hudinfo::DisplayName` |
| 5 weapons | a row per weapon group that has guns, numbered as the game numbers it (no "2 -" / "3 -" rows; the active group kept when more than three have guns, two with a torpedo row: 5.30; a click selects that group), the guns in no group dimmed in a row of their own without a label; the launcher row in three columns: the name (its last words dropped first: "JAVELIN", "SLINGSHOT"), the ammo always (amber at 0), LOCKED in its own column at the right edge; the loadout 0.62 H wide (0.66 H in the keyboard form) |
| 6 throttle | a segmented arc outside the ring at 5 o'clock (24-62 degrees) filling with the throttle; full yellow + BOOST while the afterburner fires (`hudinfo::AfterburnerOn`), empty amber + KILL (`EngineKilled`), amber REV in reverse |
| 7 cruise | the charge fills the whole bottom arc (48-132 degrees, between the shield's and the hull's), the seconds centred under the speed inside the ring |
| 8 calm | the ring 40 %, the speed and the throttle 60 %, the side panels 60 / 65 % (plates and text): visible in the travel / cruise captures |
| 9 target view | the game's 3D view only for ships, stations, planets and gates (a lane ring's view drew stray silhouettes; loot / waypoints have nothing to show), else the flat type icon at 0.07 H; a long name steps down to 0.026 H before it is cut |
| 10 slots | a wrench for the nanobots (distinct from the D-pad cross), the counts at 0.030 H |
| centre: mouse-over | the game's mouse-over box (top-level `ToolTip`: "Police Base / Fort Bush" on the speed during an autopilot dock) and `Rollover` hidden in the pad form (kept for keyboard and mouse) |
| centre: damage arcs | the native damage arcs (`HUD_DamageIndicator`) hidden under HUD 2: HUD 2's amber hit flash at 0.13 H shows the side (the arcs were "kept" in 5.1; the critique found they compete with the ring) |
| centre: bracket bars / labels | **decided against**: the target bracket's own name / distance label ("Fort Bush 1341-M") and its bars are drawn by `Targetable_Objects`' own draw (vt 0x5D9454), not by child controls (its only children: `ReticleRangeText`, `MissileAlert`, `TrailAlert`, `RadiationAlert`, `LagAlert`); hiding them would need a hook in the indicators' draw - not robust enough for this round |
| centre: off-screen arrows | **kept native** (the game's arrows in the stance colour; the subtitles' `hud_arrows` keeps them clear of HUD 2's rectangles): a redraw on one ellipse needs the same indicator hook, no robust improvement this round |
| centre: context line | one baseline (0.915 H): the last prompts give way when the line would run into the panels (only a first prompt that does not fit goes above them); one prompt per button (LOOT n replaces X TURRET: the hold ring and the tap never at once). **HOLD word** (the owner, 26 Oct 2026; 0df5d10 after the label, then 7c2b73a / 90d6069 before the glyph with a cascade): a hold prompt writes HOLD **before** its glyph (0.024 H x k, clamped to the floor where k < 1, chrome at 85 %, track 0.06; 0.35 g to the glyph so the broken ring stays clear of the word). Where that would cost a prompt or move the line above the panels against the line without any word, the first step of the cascade that keeps everything (`core\game\promptfit.h` ChooseHold / PlaceContextHold, host-free `tests\promptfit_test.cpp`): (a) the word above the glyph, centred, just over the plate (only its excess over the glyph counts); (b) in the ring's top gap - never at the floor (a ~0.015 H gap against a ~0.04 H word), so skipped; (b2) over the label's start (this build's addition: no width where the label is wider than the word); (c) one word before a run of hold prompts; (d) the word dropped from the last hold prompts, at worst the outline alone. A raised word counts in the line's top (the alert rule, the HUD bus rect, FLUF's dimming); `hud2 state` prints `hold=<step>`. Measured (`research\experience\runs\hold_word_2026-10-26\layout.md`): 16:9 / 16:10 combat keeps REVERSE + TURRET with (a) (0.338 H of 0.360 H; the word before both would need 0.435 H), 4:3 EXIT TURRET (a) and TAKE CONTROL (b2) stay between the panels, every other case "before"; no prompt lost anywhere, the outline alone never needed; in game at 4K (combat (a), turret / autopilot before) and 4:3 (turret / autopilot (a)) |
| centre: cinematics | while the game's letterbox is up (`exe::LetterboxAmount` > 0: a jump's tunnel, dock / launch scenes) HUD 2 collapses to the nav chip (in the upper bar) and the ring; the side panels and the context line go |
| keycaps | never under the 0.024 H text floor (the cap grows around its name); the keyboard form's consumables as a 2 x 2 grid of key + slot; the card's NEXT ENEMY / CLEAR row stays right of the view's column (short labels, else icons and keys) |
| flat icons | drawn in the HUD tint: the icon layers' colour is the vertex colour, the shape the texture's alpha (D3DTOP_SELECTARG2) |
| speaker strings | the names come from `hudinfo::DisplayName` everywhere HUD 2 shows a ship (threats, escorts, picker, goal panel); "Armored Transport OSC N" was not met again this round (open: check in a convoy mission) |
The known bugs: the picker's (and the goal panel's) rows named by `hudinfo::DisplayName` (`flightmodal.h` ContactName: no unvisited gate names); a trade lane ring named as the game's panel names it (`hudinfo::LaneName`, "Colorado Jump Gate->Fort Bush" - verified against the classic contact list / target panel); the red missile warning hidden (`Targetable_Objects` > `MissileAlert`, re-hidden every frame it blinks on); the keyboard form's NEXT ENEMY keycap kept out of the view; a dock with a lane ring reads LANE <far end> (LANE REQUESTED / GRANTED, ABORT LANE). Perf (dev, 4K, 190 fps, calm with contacts): hud2.Update 16 us + hud2.Render 57 us + OnRenderUnderHud 33 us per frame; the subtitles module 5.5 us (its feed included).
Verification: every fix on instance 5 in the dev build at 4K with full-resolution crops (SaveC705 combat with Rogues and
with Liberty hostile for incoming missiles, SaveS008 M03, SaveC703 lanes and the New York -> California gate), then the
release build at 4K and at 1280x720 borderless; the showcase re-captured in the release build as
`research\experience\runs\hud2_fix_2026-10-25\final_{combat,travel,cruise,dock,lane,jump,kbm,720p_combat,turret}.png`
(same situations as the integrate set; the combat frame without an alert card - no missile came within the capture's
window - the alert with NO COUNTERMEASURES is `dev_combat_missile_alert.png`, the hidden missile warning
`crop_missilealert_hidden.png`, M03's objective / portrait / escorts `dev_m03_*.png`, the lane approach
`dev_lane_approach.png`). A fourth commit (c2a27eb) after the first release captures: in turret view X stays EXIT TURRET
(loot no longer takes its place) and the comm feed waits while the letterbox is up (it straddled the bar in the jump).
Open after this round: the NeuroNet news dot (the flash state); the NeuroNet chip sits on the letterbox bar's edge in a
jump; the lane ride's destination is empty in the ride's first second (the reader's last ring not named yet); the bracket's own label / bars and the arrows (an indicator hook); the protected-ship bar was not seen in game (M03's convoy out of reach in the test).

### 5.10 Fix round 2 (25 Oct 2026, the FIX2 track; instance 5; commits 1d58c9b, 8f07928, 2a19427, 12bc233, da6d611, 1f1ad28; evidence `research\experience\runs\hud2_fix2_2026-10-25\`)
The critique of the fix round's captures (`research\evidence\controller_ui\hud2_critiques.md` "Round 4") item by item:
| Item | What changed |
|---|---|
| 1 empty slot | the D-pad slot an alert names (MISSILE INCOMING: countermeasures; SHIELD FAILING: batteries, + countermeasures with missiles; HULL CRITICAL: nanobots) lights while the card shows and while its condition holds - border and icon in the lit colour, amber `#E8A33A` (`kWarn`) when the answer is used up or not fitted; a used-up slot (0) stays lit amber while empty; an empty / missing count reads "0" at 0.030 H (amber when used up or named, dim when simply not fitted; was a ~0.004 H dash); the card's NO COUNTERMEASURES words in the same amber |
| 2 NAV | NEURONET pinned on the nav's top line in every state (the keyboard form's maneuver row pinned under it); the objective, place, chip, progress, wait and info lines stack below on their own (the nav rectangle 0.29 H, 0.338 H keyboard). The dock / lane / jump answer (DOCKING GRANTED, QUEUE n, BAY n; `Model.event`) is left out while the comm surface shows a line (the subtitles' dialogue panel on the HUD bus): one value, one place; it returns when the line ends. **Found on the way**: the controller judged the subtitles' bus record stale by comparing the record's `frame` (the subtitles' own counter) with the bus's counter - every record read stale, so the alert never moved under the dialogue panel either; now fresh = the record changed within our last 2 publishes |
| 3 hit flash | one 0.008 H amber arc fading over 300 ms (the 0.018 H glow band under it is gone). The trigger: the damage indicator's inputs are not all damage (`docs\FREELANCER_RE.md` HUD_DamageIndicator: source reports came without any hull / shield change), so a report flashes only when the hull or the shield fell within 0.5 s of it, and never while the letterbox is up. The tunnel flash itself was not reproduced in two jumps this round (no report came in the tunnel); both guards close it (rejected reports are logged, 5 at most) |
| 4 prompt, weapons | the autopilot's context prompt is a verb: TAKE CONTROL (was AUTOPILOT OFF; the mock-up's dock state too; the CLASSIC legend's AUTOPILOT OFF row in `UI_SPEC.json` is unchanged). Weapon rows wrap one gun per line onto a spare line (at most two lines a row, three lines above the launcher) before a name is cut: M03's "Rowlett's Revenge · Azrael" / "Lavablade Mk II · Drail". The group numeral in a cell of its own (a small rounded box) with a drawn "1" (stem, flag, foot: Agency FB's 1 is a bare stroke, ">1" read ">\|"); the active group's marker a yellow triangle left of the cell |
| 5 polish | the throttle arc at 50..66 degrees (8 degrees clear of the hull arc's end) with its empty blocks a third as thick (`SegArc` emptyWidth) - BOOST / KILL / REV beyond it; one fight rule: the standing place line hides by the threat list's rule (hostiles near; it showed while the autopilot fought); the target card's empty faction row collapses (the distance moves onto the name's row); NEURONET (with the headline, place and info) left out during the letterbox - only the maneuver chip stays; K&M CTRL+R twice was **one binding** (USER_NEXT_ENEMY) labelled on the card's NEXT ENEMY and on the list header's NEXT: the header's NEXT is dropped in the keyboard form while the card shows; the nav scrim's edge in two ramps (0.30 -> 0.12 over 0.03 H, -> 0 over 0.05 H) |
| also | the alerts arm 1.5 s after entering space and only on a live hull (a load read the hull as 0 for a moment: HULL CRITICAL · NO NANOBOTS on a whole ship in the first release capture); two middle dots typed as literal characters went in as cp1252 "Â·" (the wrap never found its separator) - escapes only, as the files say (quoted on purpose: mojibake-ok) |
| decided against | the bracket's own label / bars and the off-screen arrows redraw (5.9: an indicator hook, not robust enough) - the biggest centre gap stays native; a bar under the jump chip (the jump's total is not known before the sequence; the countdown stays) |
Verification: dev build on instance 5 at 4K (SaveC705 vs Rogues: the missile alert with the missing CM slot amber, the hit-report rejections in the log; SaveC703: the Baltimore Shipyard dock - DOCKING GRANTED gone from the nav while the comm panel showed it - and the New York -> Colorado jump tunnel with the chip alone), then the release build at 4K and 1280x720 borderless. The showcase re-captured in the release build as `final_{combat,travel,cruise,dock,lane,jump,kbm,720p_combat,turret}.png` - the combat frame moving at 200 m/s with BOOST and LOCKED - plus `final_missile_alert.png` (the amber slot, the alert below the dialogue panel), `final_m03_protect.png` (M03 SaveS008: "Defend the Transport Convoy" with the convoy's hull bar beside it - **the protected-ship bar seen in game**; crops `crop_m03_protect_bar.png`, `crop_weapon_wrap.png`), the dev frames `dev_*.png`. The captures before 1f1ad28 had no wrapped row to show (SaveC705's names fit). Perf (release, 4K, a fight, 185 fps): hud2.Update 15 us + hud2.Render 63 us + OnRenderUnderHud 44 us = 0.12 ms per frame (budget 0.3 ms); the new work is per-frame arithmetic only (the slot light at 10 Hz, the bus read once per layout).
Open after this round: the bracket's label / bars and the arrows (the indicator hook); an arrival's 5 s place line still shows in a fight (the event outranks the fight rule); the gate approach's own letterbox collapses HUD 2 a few seconds before the tunnel (as designed; `final_jump.png` is in the tunnel: the chip alone, no flash, release build); `padmap.cpp` / `glyphs.cpp` carry literal non-ASCII strings outside HUD 2 (to check).
(The non-ASCII item: fixed in 557d60e, `\u` escapes.)

### 5.11 As built: the guiding principles review and the decisions log (25 Oct 2026, the owner away)
Critique round 4 (`research\evidence\controller_ui\hud2_critiques.md`) judged the built HUD "modern AND Freelancer ...
ready to show" after five changes, all made in 5.10.
- **Robustness:** the game's HUD is hidden per root through its own state byte, looked up by name and revalidated, so
  a missing root leaves that panel native. Readers fail closed: an unknown value shows nothing, never a guess. The HUD
  bus treats a stale or absent partner as absent, so either module also works alone. A false alert on load is guarded.
  HUD 2's 8 new signatures match HDE 0.7.1 and upstream. Three of them include inter-function padding, as some older
  ones do (IDEAS C1; they miss only on Discovery's exe). Open: the real Elite pad has not been tried; everything was
  checked on the virtual pad.
- **Generalization:** one HUD for every state and mission, reading the game's own data: arrival lines from HDE's
  Territory / Zone plugins, names through the game's own unknown-object test, its 3D views and flat icon sheets, its
  dock replies, lane ring chains and jump sequence. There is nothing per mission. The same HUD serves keyboard and
  mouse, with keycaps and the game's own buttons as hotspots.
- **Efficiency:** 0.12 ms per frame in a release fight at 4K, against a 0.3 ms budget. Hiding costs nothing per frame
  once a root is hidden; data is read at its own rate; drawing is one batched pass. The autopilot's legs and sets drive
  no pad, so HUD 2 stays off there, and the sweep's display matrix checks that it switches on.
- **Efficacy:** the coverage test (5.8) passed in dev and release, at 4K, 720p, 21:9 and 4:3. Four critique rounds were
  run, two of them on the built HUD. The as-built page shows every state from release captures.
Decisions made while the owner was away (each with its reason in 5.8-5.10):
- the brackets, lead indicator and off-screen arrows stay native (the indicator hook is not robust enough);
- wireframes reuse the game's own views, and only for ships, stations, planets and gates;
- unvisited objects read "Unknown ..." everywhere;
- one value, one place: alerts against the comm column, and the nav's event line against the comm panel;
- the native damage arcs and the missile icon are hidden;
- the faction shown is the live reputation;
- the living mock-up takes the 11 `hud_*` rows through ui/spec;
- the alerts wait 1.5 s after entering space.
Open: the brackets / arrows; the NeuroNet news dot (state not found); the 5 s arrival line in a fight; the real pad.

### 5.12 The views on every build (25 Oct 2026, the FIX-VIEWS track; evidence `research\experience\runs\fix_views_2026-10-25\views\`)
On vanilla 1.1 the ship panel showed a disc and the target card's window stayed empty. Now one mechanism on HDE 0.7.1,
upstream HDE and vanilla 1.1 (`controller\src\impl\hudvis.cpp`, `docs\FREELANCER_RE.md` "UI"):
- **The ship:** the status panel's own 3D view, drawn by the panel base's routines in the order HDE's HudStatus.dll calls
  them (six new signed symbols, `core\flexe.h`: 193 / 193 on HDE 0.7.1, the upstream exe and vanilla 1.1). HudStatus.dll is
  no longer needed; on HDE the view is the same as before (4K capture beside `hud2_fix2` `final_travel.png`).
- **The target:** the target panel's own draw (`exe::TargetViewDraw`) with its view mode +0x648 set to 1 for the draw and
  put back after it: vanilla left it 0 (the game sets 1 only on a contact-list pick), so the draw returned at once.
- **The flat icons** (the fallback where no view draws: loot, waypoints, lane rings): `core\game\flicons.cpp` reads any
  square sheet of the same grid (vanilla's 256 px `ButtonTextures.txm`: the cells scale with the sheet, resampled up).
- Verified in the release build: vanilla 1280x960 windowed, HDE 3840x2160 borderless and 1600x1200 windowed, SaveC705 with
  and without a target (`hud2 status`: both views draw, 0 refused).
- **Hover help only when intended** (the owner saw the NeuroNet tooltip flicker in a taskbar preview; refined: tooltips for
  a real hover only): `hudvis::SetHoverHelp`, decided each frame by `hud2::RenderUnderHud` - in flight (Mode::Space) the
  game's ToolTip box and Rollover label show only after a real mouse move (the host's physical-mouse tick: motion, wheel,
  buttons) within the last 3 s; the NeuroNet windows, bases, menus, a paused game and Extended HUD 2's modal menus keep the
  game's own. Otherwise each frame at the UI pass's entry (after the game's update, before any draw) a shown one is hidden
  the game's own way (state bit 1 off + the ToolTip's showing flag +0x159 = 0, as its hide 0x4BE390): its hover timer
  starts over, so it never reaches the screen - in both HUD styles (was: hidden only by HUD 2's switch in the pad form).
  Verified: the decision (suppressed in flight with HUD 2 on and off, allowed docked, no hide through the load menus);
  the flicker itself was not reproduced here (the game starts a tooltip only on real mouse motion, which the tests do not
  make): the owner's check - a mouse move onto the NeuroNet box shows it, a resting cursor in flight never does.

### 5.13 Text on every build and at every size (25 Oct 2026, the FIX-TEXT track; evidence `research\experience\runs\fix_text_2026-10-25\`)
- **One fit rule** (`core\game\textfit.h`, host-free tests `tests\textfit_test.cpp`): a text that does not fit its box
  steps down - solved, only as far as needed - to the floor, only then it is cut with an ellipsis (or wraps, where the box
  takes lines: `hud2kit::FitLines`, the nav's objective onto two lines). Results cached per string / size / box / font
  set (`hud2 state` kit `fits` / `fitMisses`); `hud2kit::FitText` / `TextFit` (HUD 2, uistrip, the pills),
  `uidraw::FitText` / `TextShadowFit` (the pad screens' row kit: labels, pickers, keys, buttons, tabs).
- **The legibility floor** (the owner): no text under `textfit::MinSize` = 0.024 H and never under 14 px (0.024 H at
  800x600, the game's smallest mode), whatever `text_size` / `hud_scale` say - the kit clamps every text and builds its
  atlas no smaller (no magnified glyphs); the row kit's label fonts likewise, its body face (0.0155 H) only to 14 px.
  HUD 2's scale never under 100 % (its smallest texts already sit at the floor there) and never over what its panels
  fit (bottom row, right column, the centre column with the alert: max ~1.18); the alert never over the ring, the
  context line never under the alert. Glyph marks are exempt (sized by their shape).
- **The font on every build** (`core\game\gamefont.*`, both modules): the faces' names from `DATA\FONTS\fonts.ini`
  `[TrueType]` (HudSmall / ListText; text or BINI - vanilla's is binary), then each face from the game's `[FontFiles]`
  (HDE), else Windows' installed fonts (vanilla's installer puts Agency FB there), else Segoe UI; one log line names the
  source. Vanilla 1.1 now draws Agency FB (was Segoe UI: "Destroy the...", "PLANET MANHAT...", "FIGHTE...").
- **Weapon rows**: identical guns once with a count ("Justice Mk I ×2"; the old substring de-duplication also merged a
  "Mk I" into a "Mk II" row). The kit keeps two font sets (callers ask text_size alone vs x hud_scale in one frame: one
  set rebuilt its atlas every pass, 1877 builds in 40 s at hud_scale 130).
- Verified (release build, background): before / after on HDE instance 3 at 3840x2160, 1600x1200 windowed, 1280x720 and
  on vanilla 1.1 (`Install\instances\vanilla11`, 1280x960 windowed) - `<res>_<before|after>_<screen>.png`; the matrix
  text_size / hud_scale 80/80, 100/100, 150/130 on the strip, a dealer with its deal words and pills, the options page,
  a fight and HUD 2's combat state - `m_<res>_<min|def|max>_<screen>.png`. Perf at 4K in a fight: hud2.Update 27 us +
  Render 35 us + OnRenderUnderHud 22 us (was 0.12 ms).
- **The three left open, closed (FIX-TEXT 2, 25 Oct 2026; evidence `research\experience\runs\fix_text2_2026-10-25\`,
  HDE instance 3 at 3840x2160 and 1600x1200 windowed, text_size / hud_scale 80/80 and 150/130, release build):**
  - the strip hides under FLvibed's own modals too (the FLVIBED page standalone or hosted, the on-screen keyboard - the
    set padbases treats as modal), checked every frame in Draw (`uistrip state` why=modal menu);
  - the alert keeps clear of the subtitles through the bus, one owner each way so nothing oscillates: HUD 2 moves its
    alert off the subtitles' published dialogue wherever it landed (below it, above it, below the ring - the first place
    clear of every HUD 2 part); the comm feed, the Neural Net column read the alert's rectangle and yield (the feed
    narrows to end left of it, or ends above it when that leaves under 0.25 H of width; the column starts below it).
    The case seen: the alert under the ring at 4:3 met the feed's lowest card (also at 4K with hud_scale 130);
  - the options page follows text_size: its fonts at the player's size, rows / tabs / buttons as tall as the text, the
    widths the Options screen's (labels and captions take the fit rule; more rows scroll; buttons grow upward, the
    bottom edge inside the screen's frame); below 100 % nothing shrinks (the label faces stand at the floor).

### 5.14 The objective line as the game formats it, and the aspect factor (25 Oct 2026, the HUD2-43 track; evidence `research\experience\runs\hud2_43_2026-10-25\`)
- **Objective text:** the owner's vanilla 1.1 Donau fight showed "Ships%M" (vanilla's mlog strings carry a `%M` the
  game's formatter drops; HDE's do not). The host formats entry 0 of every SetMissionObjectives through the game's own
  formatter (exe::FmtStrFormat on the entry's FmtStr, inside the hook while its values live) and serves it as HostApi v24
  `ObjectiveText`; HUD 2's headline takes it (values filled in, unknown codes dropped as the game drops them); with an
  older host the raw string loses every `%` code (letter, digits, `v` digit). Vanilla 1.1 and HDE: "Destroy the Hostile
  Ships". `hud2 state` gains a `layout` line (k, aspect, the five zones, overlaps, the top / bottom gaps).
- **Aspect factor:** `k = max(hud_scale x text_size, 1) x aspect`, aspect = the usable width (0.9 W) against 16:9's
  (1.6 H), clamped 0.8..1: 16:9 and wider 1.0 (2560x1080, 4K unchanged), 16:10 0.9, 4:3 / 5:4 0.8; then the old caps
  (what fits). Not under 0.8: texts keep the floor, panel rows are spaced for floor text (0.64 stacked the card's
  numbers). Verified (release, HDE instance 4, SaveC705 + `hud2 sim combat`, Save17f7 = M01a Donau fight): 1600x1200,
  1280x1024, 1280x800, 2560x1080, 3840x2160 - overlaps none, the ring readable; vanilla 1.1 (`Install\instances\
  vanilla11c`, 1280x960) the same. ~~Open: a mission save loaded after a free-roam start (SaveC705 -> Save17f7) brings no
  objective event at all - the headline stays empty there.~~ **Fixed (25 Oct 2026):** every save load reloads
  Content.dll and the objective is restored before the host's hooks are back; the host now re-reads the game's own copy
  (exe::ClientObjective, HostApi v25 ObjectiveReread) once when it re-installs the hooks after a load (and the logic once
  per mode change out of the menus; an older host: unchanged). Verified, release build: HDE instance 3 (4K borderless)
  SaveC705 -> Save17f7 "Destroy the Hostile Ships", -> Save1e7d (M03, 22425), -> SaveC705 (cleared), -> Save17f7, ->
  M09_ck_arch "Fly to Tekagi's Arch", a fresh start on Save17f7 (the hooked event, unchanged); vanilla 1.1
  (`vanilla11c`) SaveC705 -> Save17f7 -> SaveC705 the same. `research\experience\runs\hud2_obj_2026-10-25\`.

### 5.15 Beside FLUF (26 Oct 2026, the A2 work package; built, in game pending A3 - `MOD_COMPATIBILITY.md` FLUF)
FLUF's ImGui windows (nightly-2026-10-16: the command palette top centre, the options / Player Status window, its nav
map, toasts top left, its Debug Window) draw over HUD 2 and take the mouse over themselves. `hud2.cpp` Render: each part
(nav, target card + list, ship, loadout, ring, context line, alert) eases to 0.15 alpha while a FLUF window covers its
rectangle (`flufaware::Dim`, 0.15 s each way, once per frame), back to full when it closes; the layout does not move
(no jump of the HUD when a window opens). Which windows count: `fluf_windows` (2 = every FLUF window incl. the Debug
Window, the default; 1 = FLUF's player windows; 0 = none). Without FLUF every factor is 1. **The owner's decision (26 Oct
2026):** FLUF's windows are always on top; HUD 2's parts fade under the one that covers them and are never moved for them
(no "move" feature), and the fade counts every FLUF window (default `fluf_windows` 2). Verified in game with the nightly
(A3, `research\experience\runs\fluf_nightly_2026-10-25\TABLE.md` 4a). **Input (the owner, 26 Oct 2026):** while FLUF's
command palette is open (its search box focused or not) or a FLUF text box has the keys, FLvibed holds all its own
keyboard input, like a chat box; every FLUF window that takes input (by ImGui's state, not its name) gets the pad, B
closes the focused one (`MOD_COMPATIBILITY.md` FLUF "Input", `core\fluf_rules.h`).

### 5.16 The layout lint's baseline FAILs (31 Oct 2026; `docs\hud2_lint_baseline.txt`, `UI_HUD2_PRESETS.md` 6 item 3; the lint re-run in game: 0 FAIL over all 19 sim contexts, frames for the owner still due)
- Loadout 0.15 H tall (was 0.17): its top stays at zone S1's bottom edge 0.80 H (PRESETS 2.1); rows 0.028 + 0.030 pitch, launcher at 0.122, the cross's vertical arm 0.88 d. **Owner (31 Oct 2026, A1): "mostly fine, but it's not fine for the glyphs to overlap. Consider tightening the glyphs themselves so that everything fits nicely."** - the 0.15 H panel kept, its glyphs / keycaps tightened: 5.19.
- Cruise charging: the speed at +0.045 H (was +0.050), 0.01 H clear of the seconds at +0.083 (the centre stack's gap, PRESETS 1.3). **Owner (A2): fine, kept.**
- The maneuver chip's object (DOCK / GOTO <target>, a lane's far end) fades out while the target card names the same object, back with the card's fade (letterbox): one value, one place (PRESETS 1.7). The same while an escort row of the ship panel names it (a formation's leader: `hud2 sim formation`). **Owner (A3): fine, kept.**
- Lint, not layout: arcs on one centre measured with their radial ends (the ring's arcs 6-7.7 degrees apart pass), the recorder takes `SegArc`'s drawn extent, the hit flash / chevrons are overlays drawn on top (PRESETS 1.3) outside the gap rule. **Owner (A4): fine, kept.**
- (The frames' A5, not a 5.16 item: the ship panel 0.17 H beside the 0.15 loadout, tops 0.02 H apart.) **Owner (A5): "keep these panels at identical height"** - the ship panel 0.15 H: 5.19.

### 5.17 The layout core (31 Oct 2026, Q7 step 5; `UI_HUD2_PRESETS.md` 6 step 5; built; in game since the 5.18 frames, Standard bit-identical)
- **What:** `controller\src\impl\hud2_layout.h` (pure; tests\hud2_layout_test.cpp) is the one place the frame's element
  rectangles are computed: `Compute(Frame, preset data) -> Out`. In: the screen (W, H), `hud_scale` / `text_size` (the
  scale k = `promptfit::Hud2Scale`, which carries the aspect factor), the device (pad / K&M), the context (combat,
  letterbox, an alert), which elements are present (target, list and its rows, context prompts), the impure measures
  (the context line's prompt widths and HOLD word, the alert's width) and the subtitles' dialogue rectangle. Out: the nav
  (with its text limit), the card, the threat list's top / bottom, the ship panel, the loadout, the ring centre and
  radius, the context line (prompts kept, baseline, hold plan, widths), the alert, the bus's ring / topFree / chatFree,
  the elements the preset suppresses and the boxed area against the preset's cap. `hud2.cpp` ComputeLayout is its
  impure half (measures, the HUD bus read) and copies `Out` into `Layout` for the parts; Publish takes the bus
  rectangles from it and Update the suppression (the fades). The drawn preset is `hud2preset::Drawn` (Standard until
  steps 6-8); `hud2 state` prints a `core` line (`suppressed=`, `boxed=`, `cap=`, `overrides=`).
- **From the data** (`ui_meta.h` `uimeta::hud2p`, generated from `UI_SPEC.json` `hud2_presets`; `tools\ui_meta.py` now
  writes every element's numbers per context, `Num(preset, context, element, key, fallback)`, and the shared margins,
  alert plate and aspect factors): the margins (5 % W, 0.05 H), the ring radius (`reticleR` 0.10: `RingRadius()`), the
  card's height (the combat full form off screen, 0.215: today's card always carries the name and distance), the
  loadout's height (the fight form, 0.15: 5.16), the context line's baseline (0.915, now a `promptfit::PlaceContext`
  parameter), the alert plate (0.05). A missing number falls back to today's literal (fail closed).
- **Standard's overrides** (`hud2_layout.h` `std_ov`; the spec's Standard is the refined design, PRESETS 2.1, so these
  stay until step 8 re-baselines Standard with the owner's frames): (1) card width 0.44 (spec 0.36); (2) the K&M card
  0.22 (spec one height per form; +0.005 for its click footer); (3) the nav's width rule 0.25-0.62 H × k keeping 0.42 H
  for the dialogue panel (spec w_max 0.38); (4) the nav's box (margin - 0.02 / - 0.01 H); (5) its height 0.29 / K&M 0.338
  (spec 0.30 / 0.32); (6) its right edge = the drawn text + 0.01 H; (7) the threat list's geometry (header 0.034 + 0.044,
  rows 0.042, "+n" 0.03, the K&M header 0.06; spec a 0.044 header and 0.032 rows); (8) the ship 0.40 × 0.17 (spec calm
  0.30 × 0.12, fight 0.36 × 0.15); (9) the loadout 0.62 / K&M 0.66 wide (spec 0.48 / 0.36); (10) the ring's bus
  rectangle R × 1.6; (11) topFree (0.30 H tall) and chatFree (0.30 W; the spec's comm feed x ≤ 0.469, bottom 0.76 agree
  only at 16:9, k 1). The parts' inner geometry (rows, glyph sizes, the reticle lane's 0.692) is unchanged in the parts.
- **Suppression** (the preset's rules beside its data): the letterbox keeps the nav's chip, the ring and the alert (the
  card, list, ship, loadout and context line fade out, as before); a budget rule drops elements in its order while the
  plates are over the preset's cap for the frame's case (calm no target / target, combat steady / peak = an alert or the
  target off screen). **Standard has no budget rule**: today's HUD is over the cap in nearly every context
  (`docs\hud2_lint_baseline.txt`: 15-19 %), dropping a part would move it; the lint keeps its WARN.
- **Tests:** Standard against the old ComputeLayout (frozen in the test) on 576 frames (3840x2160, 1920x1080, 1280x720,
  1600x1200, 1280x1024, 1920x1200, 2560x1080, 3840x1600, 5120x1440 × pad / K&M × 4 scale pairs × 8 states: calm, combat
  with a hold, typing, an alert over the line, an alert under a comm panel, the alert's clear-place search, the contact
  list, a line that loses prompts): every value bit-identical (1 px was the bound); the 4K anchors by hand; the zones
  (the lint's own ZonesOf / InZone); the letterbox; the budget rule (order, prefix, never past its list, nothing under
  the cap); no screen = an empty layout.
- **Found, today's layout kept (for the driver's lint and step 8):** the pad's threat list's 2nd row plate ends at
  0.404 H at 16:9 and wider, 0.004 H inside S2 (the live baseline ran on K&M, where the list is exempt there: `hud2
  lint` on the pad will FAIL `zone` in combat); the context line's fallback above the panels (a first prompt that alone
  does not fit, 4:3 / 5:4 with a wide label) sits at 0.78 H across S1's bottom edge.
- **Open** (PRESETS 6 step 5, with the first preset that has a second form): the rectangle ease and form hysteresis,
  combat-on-damage, the 21:9 frame, `chatFree.y1` per preset, laying out the controller-setup adaptation, the target's
  on / off-screen state. (Focus, 5.18, brought the card's form ease, the fight lingers and its own adaptation reads.)

### 5.18 Focus, "glass and edges" (31 Oct 2026, Q7 step 8's first preset after Standard; `UI_HUD2_PRESETS.md` 2.2; built; run in game 31 Oct 2026 (driver): lint 0 FAIL, frames sent)
- **Where:** `hud_preset 1` only (the hidden advanced row: the options page shows it with `show_advanced`; no spec page,
  no CHANGELOG until the owner's verdict). `hud2_preset.h` kBuilt[Focus]; the layout core's `RulesOf(1)` (corners 1,
  the pad's threat rows in the card, the bottom-left unplated, the card's off-screen form, the pill cap) and
  `FocusCorners` (every number the spec gives from `ui_meta.h`, now also the reticle's, the cruise row's and the
  bundle's numbers: `tools\ui_meta.py`; the open ones in `focus_ns`); its reveal `hud2layout::Reveal` (pure); the parts
  `DrawSplitArcs`, `DrawTargetCompact`, `DrawShipStrip`, `DrawFireStrip` and the lean `DrawNav` (new drawing: Standard's
  parts cannot express split arcs, a band, a 0.095 card, free-text escorts or a 0.36 strip); the context line, the alert
  and the keyboard form's contact list are Standard's. Debug `hud2 peek [pin|off]`, `hud2 state`'s `focus` line.
- **Per context (vs Standard):**
  - *Calm:* no ring - the shield (left) and hull (right) arcs, 50 degrees each at r 0.12, hidden at full, 40 % below full
    (hull) / while recharging (shield), numbers below 40 % outside each arc; under them the band at + 0.14 H: the speed
    (by the speed rule), BOOST / KILL / REV, the throttle tick (+ % while the lever sits partial), the cruise state
    (`[Y] CRUISE` 45 %, `CRUISE 3.2` / `CRUISE ON` cyan), `[X] LOOT n`. Top-left: the headline on one line (0.030) with
    `TIME 1:42` at its end, the protect bar; chip lines in a maneuver (dock / lane / jump / formation / goto + bar + ETA,
    the dock queue / bay; the AP chip, mode and wait line); the arrival line 5 s; K&M a dim `MANEUVERS` tag. Top-right:
    the compact card 0.34 x 0.095 (name + distance, attitude chip + class, faction, the pad's `[RS] x2 CLEAR`), else the
    LB hints (0.026). Bottom: the context line <= 1 pill. Held back: the fire strip, the bottom-left text, the NeuroNet
    chip, the standing place line, the info line, the K&M maneuver row. Boxed (4K pad): 2.4 % with a target, 0 without.
  - *Combat:* all three arcs full (the power arc steady); the band adds `MSL n` (the count's one home, 1.7 pair 5); the
    card morphs to 0.34 x 0.167 (eased 0.25 s, its 2 threat rows fade in from 80 %, the form lingers 6 s after the
    fight), the target lane `[RS] NEXT` (+ CLEAR when the faction keeps 0.06 H); the fire strip 0.36 x 0.14 (K&M 0.40:
    the D-pad cross / the 2 x 2 key grid, the active group's numeral + first gun, the launcher + LOCKED; opacity lingers
    2 s); escorts as free text rows with inline bars above the hold line; <= 2 pills. Boxed (4K pad): 7.1 %, 8.6 % with
    an alert (cap 9 %).
  - *Lane:* the card hidden unless hostile or an objective's object. *Dock / formation / jump:* the chip lines as
    Standard. *Autopilot:* the AP line; TAKE CONTROL is the 1 calm pill. *Letterbox:* the nav's chip, the arcs only below
    nominal; the band, card, corners and line go.
  - *Reveal:* a change wakes only its element for 4 s (cargo / credits -> the hold strip, a count / the group -> the fire
    strip, an escort's hull -> its rows, a hit -> the arcs; a lit alert slot holds the strip); the peek shows everything
    held back for 3 s, a second pins, the next ends it; one auto-peek per space entry (never in combat); `hud_reveal`
    0 / 2 in the pure rules (Focus = 1).
- **Tests:** `tests\hud2_focus_test.cpp` (921 checks): the 4K anchors, the morph, K&M, the 4:3 / 16:10 nav caps; 9
  screens x pad / K&M x calm / combat x alert: the margins, no plate in the clear field (x 0.30-0.70 W, 0.30-0.82 H) or
  the core, the pill caps, the budgets; the reveal's rules (calm, wakes, the speed, the lingers and the form ease, the
  peek and auto-peek, levels 0 / 2, the letterbox). Standard: `hud2_layout_test` still bit-identical (576 frames).
- **Decisions for the owner** (the spec's `open` list and what the doc leaves to the build; each the doc's default or
  the least change). **The owner's verdicts, 31 Oct 2026 (frames `research\experience\runs\focus_frames_2026-10-31\
  decisions_B_focus.jpg`; PRESETS 7; what changed: 5.19)** after each:
  1. The arcs' radius 0.12 (the doc's provisional value; the lead pip's dwell radius is still to be measured).
     **Owner (B1 with B10-B12): the calm meters like recent AAA games - each meter on its own: below full at reduced
     opacity, full hidden completely; combat as built.** The radius kept.
  2. The clear field's top = topFree's bottom, 0.30 H (Standard's strip, so the dialogue panel keeps its place); the
     field stays a unit-test check (the spec's `lint: false`), not a live lint zone. **Owner: not mentioned - kept.**
  3. No new scrim: the nav keeps its soft scrim, the band / hold / escort text the kit's navy outline - the contrast
     frames decide. **Owner: not mentioned - kept.**
  4. The card always draws its off-screen form (name + distance) and no bars until step 6's on / off-screen state and
     handover; the budget case is then "peak". **Owner (B4-B7): "It is important for HUD elements inside cards to not
     overlap, neither glyphs nor contextual data"** - the form kept, its overlaps fixed (5.19).
  5. The compact card shows the flat icon, no 3D view; Focus requests no views at all (capital wireframes: step 6).
     **Owner: not mentioned - kept** (inside B4-B7's no-overlap rule).
  6. The target lane on the card's third row (`x2 CLEAR`; in a fight `NEXT` first). **Owner: not mentioned - kept**
     (its glyph's place fixed by B4-B7).
  7. K&M keeps Standard's contact-list block under the card (its header is the click-pinned list's toggle) plus a
     0.034 click row for CLEAR / NEXT ENEMY: 4.2 % calm (cap 3) and 11.2 % in combat with an alert (cap 10) - the
     lint's WARN; the spec's 4 K&M rows inside the card would need that toggle a new home. **Owner (B4-B7): no overlap
     inside the card / list** - the faction line no longer runs over the CTRL+R / CTRL+T keycaps (the click row 0.040,
     tight keycaps: 5.19); the block and its WARN kept.
  8. No budget-drop rule: the pad fits; a wake or a peek is transient, like the alert. **Owner: not mentioned - kept.**
  9. The bottom-left block 0.32 x 0.17 (3 escort rows at 0.042 over the hold line at 0.93); its line "CARGO 12/40 .
     CREDITS 4 250". **Owner (B9): cargo hold / credits are not needed in flight** - the hold strip is gone; the block
     (0.32 x 0.12) holds the escort rows alone (5.19).
  10. The shield arc's "holds while recharging" = 40 % (the hull's level) while below full. **Owner (B10): per meter,
      exactly** - built so; the hit's wake of both arcs to full removed (5.19).
  11. The power / charge arc inside the circle (r - 0.012 H, 70-110 / 66-114 degrees): at r it would sit 0.004 H from
      the band (the centre stack's gap is 0.01); the arcs' numbers beside their middles. **Owner (B11, with the calm
      meters' rule): kept.**
  12. The band on fixed anchors (centre +- 0.045 H); BOOST, not the doc's BURN (the owner's word, 26 Oct); the tick and %
      whenever the lever sits partial in any setup (the lint's analog rule), the speed steady only for ACE / K&M; the
      20 % change measured against at least 10 m/s; cruise counts as a throttle state. **Owner (B12, with the calm
      meters' rule): kept.**
  13. The cruise glyph shows until step 7's retire store; `CRUISE` off at 45 % in calm only; the charge / ON words at
      full (state words never decay); the band hides in the letterbox. **Owner (B13): "the cruise prompt itself can live
      at the edge of the screen rather than at the center. It's crowded."** - the prompt moved to the context line (the
      teaching lane, bottom edge); the band keeps the state words (5.19).
  14. The lean nav: the dock / lane / jump event (queue, bay) stays with its chip ("chip lines as Standard", though the
      spec's value table gives `event` no Focus home); the arrival line stays 5 s (an event; HUD 2 hides the game's own);
      the cruise-charge chip moves to the band; the 4:3 width below W / H 1.5 (16:10 keeps 0.48). **Owner (B14): fine,
      kept.**
  15. Until step 4 writes the bundles Focus applies its own (`hud_reveal` 1, `hud_auto_peek` 1, the 4 s wake, the 2 s /
      6 s lingers, from the data) and does not read `hud_fade`; `hud_calm_panel` 60 has nothing to dim (the card stays
      at 100 % as Standard's). **Owner: not mentioned - kept.**
  16. The fire strip's group row is the active group (its first gun + "+"); no torpedo row (no reader yet); the
      launcher's count in the strip only out of a fight. **Owner (B16): show ALL weapons incl. launchers, a smaller font
      and adapted spacing so all fit, the active group highlighted** - built (5.19); the torpedo row since 5.30.
  - **The known fault:** ABORT DOCK's keycap read empty on K&M - fixed (5.19).
- **Not verified:** nothing ran in game (the driver builds, runs `tools\hud2_lint_baseline.ps1 -Preset 1` and captures
  the frames); the band sits in the ship keep-out's top edge (0.627-0.653 H vs 0.63: the lint's WARN, the doc's own
  place); the peek's real inputs, the retire store and the on / off-screen state are steps 6-7.

### 5.19 The owner's verdicts on the Standard / Focus frames (31 Oct 2026; PRESETS 7; built; run in game (driver): lint 0 FAIL, frames sent)
The frames `research\experience\runs\focus_frames_2026-10-31\decisions_A_standard.jpg` / `decisions_B_focus.jpg` (full-res
`std_*.png`, `focus_*.png`). Per decision, what changed (fractions of H x k):
- **A1** (Standard's loadout; "not fine for the glyphs to overlap ... tightening the glyphs themselves"): the 0.15 H panel
  kept. A **tight keycap** (`hud2kit::Keycap(..., tight)`, `DrawPrompt(..., tight)`): the name at the 0.024 floor, the cap
  1.2 x the name (0.0288; the floor-grown cap is 0.033-0.039) - used in the loadout, Focus's fire strip, the K&M maneuver
  row and Focus's card click row; the context line, the reticle lane, the nav chip and Standard's card keep the old cap.
  K&M with group keys: rows from 0.022 at a 0.039 pitch (0.0288 + 0.01; was 0.028 / 0.030 - the NUM 1 / 2 / 3 caps
  overlapped each other), the launcher row 0.128, names end 0.012 before their key (was 0.008), the launcher row's texts
  end left of the key column. The pad's rows are unchanged (0.028 / 0.030 / 0.122). Every slot (both presets) is one
  helper, `Slot`: the count at the text floor (was 0.030 hanging over the slot's lower right edge and over the icon) at
  the slot's right, the icon beside it (two digits' room kept: 0.0199 in Standard's 0.046 slot, was 0.0285), stacked
  diagonally only for three digits; the up slot's count 0.018 H above the D-pad glyph (was 0.0012).
- **A2-A4**: kept. (The K&M maneuver row's keys are tight now - their floor-grown caps sat 0.0094 H from the headline's
  box, a panel.overlap FAIL; the row's place is unchanged.)
- **A5** (identical heights): the ship panel 0.40 x 0.15 (`std_ov::kShipH`, was 0.17), its top on the loadout's 0.80;
  rows: name 0.030, HOLD 0.068 (bar + 0.020), CREDITS 0.120; up to 3 escorts from 0.026 at a 0.042 pitch (was 0.05),
  bars 0.018 under their names; the view window 0.12 (0.80 x h). The layout core's test: only the ship's 0.17 -> 0.15
  in its frozen reference (bit-identical otherwise, 576 frames).
- **B1 / B10 / B11 / B12** (calm meters): each meter on its own in calm - below full 40 %, full 0; a hit no longer wakes
  both arcs to full for 4 s (`Reveal`: `wakeArcs` and `RevealIn::hit` removed); the fight's full arcs, a peek and
  `hud_reveal` 0 unchanged (the player asked for everything there).
- **B4-B7** (no overlap inside cards): Focus's pad card + 0.012 (`focus_ns::kCardLaneRow`: calm 0.107, fight 0.179) -
  row 3 at 0.089 with a 0.0264 lane glyph (was 0.078 / 0.0288: its top met row 2's texts); its threat rows' icons
  0.021 (0.024 stacked at the 0.032 pitch left 0.008). K&M: row 3 stays 0.078, the click row 0.040 (was 0.034) with
  tight keycaps centred at y1 - 0.020 (`DrawCardIcons(..., compact)`) - the faction line ran over CTRL+R / CTRL+T.
  K&M combat without an alert 9.9 % (cap 10). Standard's card passed the new rule unchanged (pad and K&M).
- **B9**: the hold strip is gone (`DrawShipStrip` without it; `Reveal` without `holdSig` / `holdLine`); the bottom-left
  block (`focus_ns::kShipH` 0.12, was 0.17) holds the escort rows alone - the last on 0.93 - in a fight, on an escort's
  hull change, on a peek; held back otherwise.
- **B13**: Focus's cruise prompt is a context-line pill (the teaching lane at the bottom edge, PRESETS 1.6: "CRUISE in
  calm" is a teaching glyph): calm `[Y] CRUISE` after an actionable target's DOCK / TRACTOR, before GO TO / TARGET AHEAD
  (with the 1-pill calm cap it shows unless DOCK / TRACTOR / DROP CRUISE leads); `[Y] CANCEL CRUISE` first while it
  charges; `hud2 sim travel` / `cruise` likewise. The band keeps the state words (`CRUISE 3.2`, `CRUISE ON`, no glyph).
  Standard's reticle lane unchanged.
- **B16**: the fire strip shows every group (its numeral cell, its guns; the active lit, the others dim), the guns in no
  group (dim) and the launcher (name, count out of a fight, LOCKED); text 0.024 (was 0.026); the rows share the 0.12
  inner height at up to 0.040, centred (5 rows: 0.024); RT centred on the group rows, RB on the launcher's. The cross:
  slots 0.042 at 0.044 (was 0.038 at 0.040) so each count fits beside its icon (0.016).
- **ABORT DOCK's empty keycap**: not empty - free flight (`USER_MANEUVER_FREE_FLIGHT`) is key 192 = VK_OEM_3 in
  `SAVE\UserKeyMap.ini`, which `GetKeyNameTextW` names "`" on a US layout: a ~0.004 H tick at the cap's top. `VkName`
  now shows that key as "~" (its larger mark, the same 1-character cap - "TILDE" widened LEAVE FORMATION's pill to
  0.28 H at 4K K&M, past the loadout's edge, and the context line fell back above the panels into zone S1: the
  driver's in-game lint) and words the rarer lone marks (´ ACUTE, ^ CARET, ' QUOTE, , COMMA, . PERIOD, - MINUS, ¨
  DIAERESIS); letter names of other layouts stay. The same key on STOP / LEAVE FORMATION / the maneuver row's first key.
- **The driver's in-game lint (4K K&M), fixed:** the fire strip's launcher count sits in the numeral cells' column
  (right-aligned to its edge: one column with the group digits, 0.010 H from the names; it was 0.006 H diagonally from
  the last digit), its name left out where only a stub would fit before LOCKED; the cards' texts that end just short of
  a neighbour row's column (Focus: the name / faction vs the class, the class vs the distance; Standard: the name vs
  the faction row's distance) are fitted to end 0.011 H before it (`ClearRoom`; real widths "Manhattan" / "PLANET",
  "POLICE BASE" / "Liberty Police" were 0.0088 H apart). The lint tests carry those real rectangles.
- **The lint: `FAIL panel.overlap`** (`hud2_lint.h` PartOf / PanelItem / PanelGapPx; the gap U35 / controller_ui "HUD
  rules" 8, "no overlap between HUD boxes, a minimum gap (proposed 0.01 H)"): two drawn texts, glyphs, keycaps or icons
  of one part (the tag's first segment; `list` counts as the card's; each `ring.*` component its own) closer than 0.01
  H - or overlapping at all when they are one designed unit (`hud2kit::RecUnit`, stamped by the recorder: the part
  draws a prompt and its own label, a slot's icon + count + key, a chip's icon + word inside one scope - that is how a
  designed pair is told) or two lines of one text column (x ranges overlapping: the line pitch spaces them). Plates,
  bars, lines and arcs are not weighed; items under 10 % alpha neither. Before these fixes it caught, in the owner's
  frames' cases (tests\hud2_lint_test.cpp, the old geometry): the NUM keycaps overlapping each other and 0.008 H from
  the names (A1), a slot's icon under its count and the up slot's count 0.0012 H over the D-pad glyph (A1's pad form),
  the K&M maneuver key 0.0094 H from the headline, the faction line under the CTRL+R keycap (B4 K&M), row 2's class
  text under the lane glyph (B4 pad); the fixed geometry passes. Every prompt + label site was wrapped in a unit
  (reticle lane, band, card lanes, hints, list header, context pills, alert answers, NEURONET, the maneuver chip, the
  card's click row, slots).
- **Tests:** `hud2_lint` 804 checks (+ the panel.overlap cases and semantics), `hud2_focus` 922 (the card's new heights,
  per-meter arcs, no hold strip), `hud2_layout` 29142 (ship 0.15); 51 tests, 0 failures.
- **Not verified:** nothing ran in game; the impl sources (hud2_parts / hud2 / hud2_click / hud2_kit / lint_live) are
  not in the test build - the driver's `dev` build is their first compile. The living spec still draws the old HUD 2
  mock (`tools\uimock\hud2_states.py`: the loadout 0.56 x 0.17, the ship 0.40 x 0.17): it must show the 0.15 ship panel
  beside the 0.15 loadout, the slot counts at the floor beside their icons, the K&M group keys as tight caps at the
  0.039 pitch (not published here).

### 5.20 The owner's decisions, second round (31 Oct 2026; PRESETS 7; built; run in game (driver): lint 0 FAIL, frames sent)
The owner's second verdicts on the 5.19 frames (`research\experience\runs\focus_frames_2026-10-31\after_A_standard.jpg` /
`after_B_focus.jpg`). Per decision, what changed (fractions of H x k):
- **1 Long names: a marquee** (the public-display pattern) instead of a permanent "...". The pure timing
  `core\game\marquee.h` (`tests\marquee_test.cpp`): a text the fit rule would cut (it still shrinks to the floor first)
  shows its cut form (start + ellipsis, exactly), holds **1.5 s**, scrolls at **0.05 H/s** (at the floor size; as many
  characters a second when larger) until its end is right-aligned in the room, holds **1.5 s**, fades out **0.25 s** and
  back in on its start, repeats. The clip opens from the cut form's prefix to the room and the ellipsis fades over the
  last ellipsis' width, so the picture is continuous (the only jump, the reset, happens at alpha 0) and a function of
  the time since the text first showed (frame-rate independent; sub-pixel: no x snap while it scrolls). Why: Android's
  TextView marquee waits 1.2 s before starting and restarting and moves 30 dp/s (~0.04 H/s on a phone) [R, memory]; a
  HUD is read in glances, hence the owner's ~1.5 s; 0.05 H/s is ~5 Agency FB capitals a second, well under reading
  speed; the reset fades with the panels' 0.25 s ease (PRESETS 1.2 continuous motion). The kit's `hud2kit::TextMarquee`
  (TextFit's signature) draws the full text clipped to its room by **per-glyph clipping** (each glyph quad and its UVs
  cut to the room: D3D8 has no scissor, a viewport would split the batch), its clock keyed by string + size + room
  (restarts after 30 unseen passes), and records ONE text whose box is the room - `panel.overlap` sees the same box at
  every moment. Applied wherever HUD 2 fitted / cut a text: the nav (Standard's 2-line headline: its last line scrolls
  the rest of the headline; Focus's headline; every fitted nav line), both cards' name / class / faction, the pad's
  threat rows and the K&M list rows, the ship panel's and Focus's escort names, the loadout's and the fire strip's group
  / ungrouped / launcher names (the launcher's word drop "JAVELIN MISSILE" -> "JAVELIN" is gone: the full name scrolls),
  and a context pill's label where a lane squeezes it. `ClearRoom` takes a cut text's end as its room's. A text that
  fits does not move. **Reduced motion: HUD 2 has no such option yet** - `marquee::Static` is the cut form a switch would
  draw (WCAG 2.2.2 asks for one where motion lasts over 5 s; for the owner).
- **2 Launchers hidden while LOCKED**: kept.
- **3 The loadout narrowed, the consumables stacked above it**: the cross (the D-pad cross; K&M its 2 x 2 key grid) has
  its own plate on the base panel's right end, right-aligned, 0.012 H above it; the base holds the weapons only.
  Standard: base **0.40** (K&M **0.44**; was 0.62 / 0.66) x 0.15 (= the ship panel's height, A5), the names' room ~0.23 H
  (was 0.25); the cross **0.20 x 0.15** (K&M 0.16 + 2 key widths x 0.13), its geometry unchanged. Focus: the fire strip
  **0.22** (pad and K&M; the spec's 0.36 / 0.40 held the cross) x 0.14, the cross **0.16 x 0.14** (K&M 0.134 + 2 keys x
  0.11, so K&M combat stays under its 10 % cap). `hud2_layout.h` `Out::cons` (`std_ov::kCons*`, `focus_ns::kStripW` /
  `kCons*`; `Measures::slotKeyW` = the K&M keys' widest tight keycap, `SlotKeyWidth`), part of the Loadout element (one
  fade, one budget entry); the base's top stays S1's 0.80 H edge; the cross lies right of S1 in the right rail's lower
  end - the lint's S2 is split there (left of the cross as before, above it down to its top). Side effects: the K&M
  contact list and the goal panel (`BottomPanelsTop`) stop above the cross (at 4K the list holds ~5 rows instead of ~9);
  the bus publishes the loadout with the cross as one rectangle; `Hud2Scale`'s bottom-row cap (1.02 / 1.06 H) is
  unchanged (conservative: Standard's k does not move).
- **4 Prompts in the bottom corners, the centre bottom clear**: the context prompts go into two **lanes** beside the
  bottom panels (`promptfit.h` `ContextLanes` / `PlaceLanes` / `PlaceLanesHold`; `hud2_layout.h` `PlaceContextLanes`):
  the left from the ship panel's right edge + 0.012 gap + 0.012 pad to 0.40 W - pad, the right from 0.60 W + pad to the
  loadout's left edge - 0.024; two rows each (the preset's baseline 0.915 and one 0.06 above it). **The rule: the
  corners' jobs (PRESETS 1.1)** - the ship's movement and control left beside the ship panel (DOCK, GO TO, CRUISE /
  CANCEL CRUISE, DROP CRUISE, STOP / ABORT DOCK / ABORT LANE / LEAVE FORMATION, REVERSE, TAKE CONTROL, GOALS, MAP), what
  you fire / use / target right beside the loadout (TURRET / EXIT TURRET, TRACTOR, TARGET AHEAD, TARGETS) - `hud2.cpp`
  `CtxSide`; within a lane the priority order, the first prompt hugging its panel. A prompt takes the first row it fits
  whole, else an empty row squeezed (down to its glyph + 2 floor ems of label; the label then scrolls), else the other
  lane; one with no place ends the line (a prefix). The HOLD cascade as before; a raised word only where no prompt sits
  above it, never squeezing a prompt more than the word-less layout did. Both presets (Focus after its pill cap). Where
  no lane has room for even the first prompt (4:3 / 5:4 with hud_scale x text_size over ~1.08: the panels reach the
  band) the old centred line above the panels is the fallback (`ctxFallback`: across S1's bottom edge there, as 5.17
  found; never in the band). The alert no longer pushes the line down: the prompts step aside while it meets one (the
  lanes and the alert do not meet at any aspect). **The lint zone `bottom`** (every preset; `hud2_lint.h` `kBottom*`,
  `Zone::texts`): FAIL `zone` for any plate, text, glyph, keycap or icon in x 0.40-0.60 W (the ship keep-out's columns,
  PRESETS 1.3) from 0.85 H down (the owner's ~0.85; the old line's pills reached 0.889); the subtitles' panel exempt as
  in every zone.
- **Tests** (`build.cmd test`: 52 tests, 0 failures): `marquee` (the values, a fit stays still, the cycle, 3 cycles at
  1 ms with no visible step and nothing past the room, 30 / 144 fps alike, sub-pixel); `promptfit` the lanes (geometry,
  both lanes, the row above, the squeeze and its minimum, the hand-over, the prefix, typing, the cascade's raised word);
  `hud2_layout` the frozen reference changed only for decisions 3 / 4 (576 frames otherwise bit-identical), the 4K
  anchors of the base / cross / lanes, every pill and the cross outside S1 / S2 / core / bottom; `hud2_focus` the strip
  and its cross, the pills outside the clear field and the band, no fallback on the matrix; `hud2_lint` the band (a
  centred pill and its label FAIL, the lanes pass, the subtitles exempt) and the split rail.
- **Not verified**: nothing ran in game; the impl sources are not in the test build (the driver's `dev` build is their
  first compile). To capture: a marquee sequence (a long target name / faction on Focus's 0.34 card, a long launcher
  name on K&M beside LOCKED; frames at 0, ~1.6, ~2.5 s and in the end hold), both presets pad + K&M in calm / combat /
  dock / formation / ap (the lanes: DOCK left, TARGET AHEAD right; LEAVE FORMATION squeezed at 4:3), `hud2 lint` 0 FAIL
  with the new zone.
- **Open for the owner**: no reduced-motion switch; the launcher's full name scrolls (the word drop removed); TARGET
  AHEAD (~0.19 H at 4K) is squeezed in the 4K right lane (~0.17 H) and scrolls - the band, the loadout's 0.40 or the
  lanes' gaps decide that; the K&M contact list's fewer rows above the cross. The living spec mock
  (`tools\uimock\hud2_states.py`, not edited here) must show the narrow loadout with the stacked cross, the lanes beside
  the panels with nothing in the bottom-centre band, a long name in its cut (start) form, and (5.21) Standard's cruise
  prompt in the left lane ([Y] CRUISE calm, [Y] CANCEL CRUISE first while charging) with only the glyph-less state word
  under the ring, and (5.22) the ship panel 0.31 x 0.15 (icon column 0.085, texts from 0.107, values 0.014 before the
  edge), the context pills' tight K&M keycaps, and CANCEL CRUISE whole in the wider left lane.

### 5.21 Standard's cruise prompt to the left lane (31 Oct 2026; PRESETS 7; built; run in game (driver): lint 0 FAIL, frames sent)
The owner on 5.20's open item ("Yes, move it"): Standard's ring row no longer carries the cruise prompt with its keycap
just above the ship's nose. As Focus's did (B13), the prompt is a context-line prompt in every preset (`hud2.cpp`: the
`focusLine` gate is gone; `CtxSide` puts `Cruise` in the left lane, the ship's movement): calm `[Y] CRUISE` after an
actionable target's DOCK / TRACTOR, while charging `[Y] CANCEL CRUISE` first; pad and K&M alike (K&M its keycap). The
ring row (`hud2_parts.cpp` "ring.lane") keeps only the state word without a glyph - cyan `CRUISE` while charging (the
seconds already stand inside the ring above it, PRESETS 1.7: so not "CRUISE 3.2" as Focus's band reads, which has no
seconds of its own), `CRUISE ON`, nothing while off - beside `[X] TRACTOR n` as before (`Model::cruiseBtn` removed).
Standard has no pill cap (<= 3 prompts, 2 rows a lane): at 16:9 / 21:9 / 16:10 every Standard line with the cruise
prompt places in full (DOCK + CRUISE stack in the left lane's two rows at 16:9; at 21:9 one row); at 4:3 K&M the right
lane is closed and `TARGETS`, the line's last, gives way to CRUISE (the prefix rule); at 5:4 K&M the old centred fallback
line as before. Measured with the game's font (`tests\promptfit_test.cpp` StandardCruiseLane, `fltest -v`): `CANCEL
CRUISE` is 0.204 H (pad) / 0.214 H (K&M) against the 16:9 left lane's 0.186 H - it is squeezed to the lane and its label
scrolls (the marquee, decision 1); open for the owner (a shorter label, or the lane's width). Tests: 52, 0 failures.
Not verified: nothing ran in game; `hud2.cpp` / `hud2_parts.cpp` are not in the test build (the driver's `dev` build is
their first compile).

### 5.22 Standard's ship panel narrowed (31 Oct 2026; PRESETS 7; built; run in game (driver): the frame shows CANCEL CRUISE unscrolled after 8a3a4866)
The owner on the frame `focus_frames_2026-10-313_standard_cruise.jpg` ("some wasted space in there. Reduce it"): the
panel is **0.31 x 0.15** (`std_ov::kShipW`, was 0.40; the height stays the loadout's, A5). Removed (fractions of H x k):
the icon column 0.12 (0.80 x h) -> **0.085** (`std_ov::kShipViewW`: the icon at half the panel's height, 0.075 - the
owner's glyph rule, it was 0.0744 already - with 0.005 each side instead of 0.035 / 0.041 of air), the texts start 0.010
after it (x 0.107, was 0.150; 0.015 from the icon), the values are right-aligned 0.014 before the edge (was 0.018); the
text column is 0.189 (was 0.232), every row and datum unchanged (name / class, HOLD + bar, CREDITS, up to 3 escort rows
at the 0.042 pitch, a long name's marquee). The game's wireframe window (`hud2.cpp`, `hud_wireframes`) uses the same 0.085
column, so the wireframe draws narrower. HOLD / CREDITS labels take only the room their value leaves less 0.012
(`TextMarquee`): an 8-9 digit credit count scrolls the label instead of overlapping it (panel.overlap); 7 digits fit.
**The K&M keycap** (the driver's 4K K&M frame `std_cruise-213223.png` after a first 0.35 cut still scrolled ":ANCEL
CRU..."): the test had modelled the K&M prompt's key as a 1.3 h bumper glyph (0.047 H), but the game's regular SHIFT+W
keycap is 0.101 H (`promptfit::KeycapWidth`, now the kit's own measure; the frame ~218 px), so the real prompt was 0.268 H
against a 0.236 lane. The context prompts now draw the **tight keycap** (`DrawContext` / `ContextWidths`, as the loadout
and the maneuver row since A1; `promptfit::KeycapTightWidth`): `[SHIFT+W] CANCEL CRUISE` is 0.254 H (pad 0.204). The left
lane: **0.276 H** at 16:9 (4K, 1080p, 720p), 0.249 at 16:10 (prompt 0.241), 0.484 at 21:9 - placed whole, closing 5.21's
open item; 4:3 (0.190) / 5:4 (0.161) still squeeze K&M's. Focus's bottom-left block (0.32, free escort text with no icon
column) has not this slack and is unchanged (its K&M pills take the tight keycap too). Tests: `promptfit`
StandardCruiseLane measures K&M prompts with the real keycaps (SHIFT+W, F3) through the kit's formula, checks the regular
SHIFT+W cap against the frame (0.101 H +- 0.006 at 4K) and asserts the lane holds CANCEL CRUISE with no left-lane prompt
squeezed at 16:9 / 16:10 / 21:9 (it FAILS on the 0.35 panel with the regular cap: 11 failures); `hud2_layout` the frozen
reference's ship width 0.31 (the 4K anchor 861.6, was 1056); 52 tests, 0 failures. Not verified: nothing ran in game;
`hud2_parts.cpp` / `hud2_kit.cpp` / `hud2.cpp` are not in the test build (the next `dev` build is their first compile);
the narrower wireframe window and the tight context keycaps want a frame.

### 5.23 The lower centre devoid of HUD 2 (31 Oct 2026; PRESETS 7; built; run in game (driver): lint 0 FAIL, the frame shows the ship clear)
The owner on the frame `focus_frames_2026-10-31\r5_standard_cruise.jpg` (CRUISE ON in cyan just above the ship): "The
CRUISE ON text still hides the ship. We should really make sure the lower center of the screen is devoid of HUD elements
(unless they're diegetic like an on-screen ship/solar being a target, etc)." The rule: **nothing HUD 2 draws lies in x
0.40-0.60 W from the ring's lowest drawn edge + the centre stack's 0.01 H down to the screen's bottom** - both presets
(and Speedrun / Guided, which draw as Standard), pad and K&M, every context. What moved (fractions of H x k):
- **Standard's reticle lane is gone** (it stood at the centre + R + 0.092 = 0.692 H on the ship's nose). The cruise state
  reads in one place, inside the ring under the speed, in the charge seconds' slot (+0.083, tag `ring.cruise`; Speedrun's
  spec put its cruise state there already): `3.2 s` while the engine charges (the arc filling), cyan **CRUISE ON** while it
  runs (the speed rises to +0.045 as while charging), nothing while off; the charge word CRUISE went (the arc + seconds,
  the nav's CRUISE CHARGING and the left lane's CANCEL CRUISE say it). The power arc (68-112 degrees) yields that slot
  while cruise charges or runs (the game takes the weapons offline while charging, FREELANCER_RE "The cruise line"; while
  running it is this package's choice, open for the owner). The prompt stays the left lane's (5.21).
- **[X] TRACTOR n** is a context prompt in the **right lane** (`hud2.cpp` `TractorCtx`, `CtxSide` Tractor -> right): in calm
  after DOCK / the targeted loot's TRACTOR (whose label then carries the count: "TRACTOR 3", one value one place), in the
  other states after their own prompts; within the line's 3 prompts and its one-prompt-per-button rule - in combat the
  X-hold TURRET keeps the X, so TRACTOR n does not show there (it did under the ring; open for the owner). Not in Focus,
  whose band keeps LOOT n with the tractor's glyph.
- **Standard's throttle word** (BOOST / KILL / REV) left its place centred under the throttle arc (to R + 0.058, in the
  lower centre): it stands right of the arc, from cx + 0.068, its box's bottom on the arc's outer edge - > 0.01 H from the
  power / charge arcs and from CRUISE ON (centre.overlap). The throttle arc itself stays at the ring's bottom centre (the
  owner, 26 Oct 2026) and is the ring's lowest drawn edge.
- **Focus's band** moved from the centre + 0.14 (y 0.63-0.65, over the ship) inside the arcs' circle to **+ 0.078**
  (`UI_SPEC.json` focus `band_dy` / `band_y` 0.565-0.591, `core\ui_meta.h` regenerated); its rows pass through the gaps
  between the split arcs (+-25 degrees) and the bottom arc, > 0.01 H from each, the speed's top 0.018 H outside the core
  (r 0.045). The bottom (power / charge) arc moved from R - 0.012 onto the circle R.
- **The context line's fallback** (no lane beside the panels has room: 4:3 / 5:4 at large scales) is no longer the centred
  line above the panels (it crossed the lower centre) but the same two lanes above the panels (`promptfit.h` `LanesAbove`:
  margin .. 0.40 W and 0.60 W .. margin, baseline 0.04 H x k above the panels, two rows); where even they have no room,
  no prompt.
- **The lint zone `bottom`** (`hud2_lint.h` `ZonesOf(..., clearY0)`): x 0.40-0.60 W, from **`Out::clearY0` = cy +
  `RingBottom` + 0.01 H** (`hud2_layout.h`: Standard's family R + 0.022 + 0.75 x 0.0065 / 2 = the throttle arc's outer
  edge, **0.6344 H** at 16:9 k 1; Focus R + 0.0065 / 2, **0.6333 H**; it scales with k) to the screen's bottom (was 0.85
  H). It now FAILs **any** recorded item there - plates, texts, glyphs, keycaps, icons, arcs, shapes - except the transient
  overlays (`Item::overlay`: the 0.3 s hit flash and the missile chevrons at the threat's bearing, 1.3). Without a layout
  the preset's circle at k 1 + 0.01 H (0.61 H, stricter). `hud2 lint` passes the drawn layout's `clearY0`. The keep-out's
  WARN is now inside this FAIL zone's columns.
- **Diegetic exceptions**: the game's own brackets on targets, its reticle and the 3D scene are drawn by the game, not by
  the kit, so the recorder (`hud2kit::Record`) never sees them and the zone cannot flag them; everything HUD 2 draws goes
  through the kit and is recorded with its component tag.
- **Tests** (`build.cmd test`: 0 failures): `hud2_lint` - the frame's CRUISE ON as drawn (0.692 H, ~0.109 H wide: 115 px
  of the frame's 1057 px H) FAILs `zone bottom`, with the TRACTOR row and the old centred throttle word 4 FAILs (all four
  passed at the old 0.85 H top); the moved layout (CRUISE ON at +0.083, the speed +0.045, the word beside its arc, the
  charge arc while charging) has no `zone` and no `centre.overlap`; an arc and a shape in the zone FAIL, a missile
  chevron (overlay) passes; the zone's top from `clearY0` and the fallback per preset. `hud2_layout` - `clearY0` at 4K
  and at a larger scale, the frozen reference's fallback = the lanes above the panels, no pill in the 0.40-0.60 W columns
  across the 576-frame matrix. `hud2_focus` - `band_dy` 0.078, `clearY0`, the old band (+0.14) FAILs 3 items in the zone
  and the new one none with no `centre.overlap` against the hull and bottom arcs; the band inside the circle and above
  `clearY0` across the matrix.
- **Not verified / open**: nothing ran in game; `hud2_parts.cpp` / `hud2.cpp` are not in the test build (the next `dev`
  build is their first compile); the real CRUISE ON width inside the ring wants a frame (the geometry allows up to ~0.12
  H). Open for the owner: the power arc hidden while cruising; TRACTOR n absent in combat (the X button's TURRET); the
  alert card keeps its 1.3 exemption in every zone and can still fall back below the ring (`hud2_layout.h`: only when it
  would meet the ring, 4:3 with a list or a dialogue) - not moved here; the unbuilt alert-queue design's FLARE mirror "in
  the reticle lane" (`UI_SPEC.json` value_ids alert `second`) needs a new home. The living spec mock
  (`tools\uimock\hud2_states.py`, not edited) must show CRUISE ON inside the ring, TRACTOR n in the right lane, the
  throttle word beside its arc and Focus's band inside the circle.

### 5.24 The 0.2.1 hotfix: prompts wrap, show on change, the maneuver lock answers (2 Nov 2026; built; run in game: lint 0 FAIL)
- **Prompts never marquee** (the owner): a context prompt's label too long for its lane wraps onto two lines
  (`core\game\promptfit.h` FitLabel: one line; one line shrunk down to 0.85 x nominal; two lines at the best word split,
  0.85 x nominal, shrunk to the floor where needed - never under `textfit::MinSize`; the ellipsis at the floor only as the
  last resort). A lane squeezes a prompt no narrower than its label's two-line width at the floor (LabelMinWidth); a
  squeezed prompt (`wrap` in `hud2 state`) grows its plate 0.008 H x k upward (0.060 tall, bottoms aligned) and the row
  above in its lane moves up by the same (PlaceLanes `tall`), so two-line prompts never meet each other or the panels;
  the lines' centres 1.0 em apart (0.95 overlapped by 2.6 px: the lint's panel.overlap). Panel names keep the marquee.
- **`hud_prompts`** (Int 0..1, default 1; `core\game\promptshow.h`, the Focus preset's "gone when not needed"): 1 = the
  lanes show on a change of the prompt set (its signature as the model built it, before the layout's trim) or after a
  player action (`hud2::NoteAction`: the pad's maneuvers, cruise, targeting, the wheel's actions through Apply; the
  keyboard's through ImplApi v4 OnKeyCmd; a target switch / clear by any input), 4 s, then the context fade (tau 0.12 s);
  0 = always (0.2.0). The modal prompt bar (game boxes, pickers) is not affected.
- **The maneuver lock's answer** (a player report, M01 after joining Brandt's formation): while `space::Pilot::locked`
  (IBehaviorManager +0xBC, Act_LockManeuvers) a pad or key free flight / formation / dock / goto logs `event maneuvers
  locked: <action> refused` and shows the alert card LOCKED BY THE MISSION (warning amber, 2.5 s, not gated by
  `hud_alerts`); the request still reaches the game. The classic HUD: log only (the game greys its maneuver bar).
- Evidence: `research\experience\runs\hotfix_021_2026-11-02\` (run.txt; `j_` the K&M CTRL+R TARGET AHEAD on two lines at
  4K, `b_` the lanes hidden in calm, `c_` shown after a target switch, `e_` / `l_` the lock card after pad B in formation).
  Lint at 4K calm: 0 FAIL at 100 % and at hud_scale 130 x text_size 150; in formation one pre-existing FAIL (value.twice:
  the leader's name on the card and in the ship panel), not the lanes.

### 5.25 The 0.2.1 hotfix, second round: the contextual HUD, the wheel's look (2 Nov 2026; built; run in game: lint 0 FAIL)
- **`hud_fade` = 0 FULL / 1 CONTEXTUAL** (the default; the owner's decisions of 2 Nov 2026). CALM DIMS and MINIMAL retired
  (the registry clamps a stored 2 to 1; the code reads anything but 0 as 1). Rule: `core\game\hudwake.h` (test
  `tests\hudwake_test.cpp`): per-element wake timers (ring + speed, ship panel, loadout, target card + list, nav), 4 s
  after the last trigger, then the existing ease. Triggers: the pad's steering past 0.15 (Context::steer), boost (the
  model's burner), fire / launcher (Context::firing) and any change of the loadout's counts, a target switch / clear (any
  input), the maneuver chip's change (goto, dock, formation, free flight, any input) and the pad / key actions
  (NoteAction's wake bits), cruise on / off / charging and engine kill, a hit, an objective change. Always up: a fight
  (the state not calm), a modal / NeuroNet window (everything), low shield (< 25 %) / hull (< 50 %) (ring + ship),
  incoming missiles (ring); the alert card is never faded; the prompts follow `hud_prompts`, the card corner's LB hints
  with them, the K&M list without a target with the target element. Under CONTEXTUAL the ring's speed / cruise readout
  go with the ring (DrawRing kept them at 60 % of a dimmed ring). Focus keeps its own reveal. The nav now has a fade
  (`g_f.nav`). Gap: K&M mouse steering is not a steer trigger (the pad's only); keys / clicks wake through their actions.
  The spec's `hud2_presets` bundles still name Focus's hud_fade 2 (read as 1).
- **The action wheel's look** (`wheel.cpp`): under HUD 2 drawn by HUD 2's kit - each command on a glass plate (chamfer
  TL / BR, hairline rim; the pick: yellow rim + accent edge + yellow text), outlined HUD 2 text, a 22 % glass disc with a
  faint rim behind (the scene shows through), the stick's path ring, the pick's name below the reticle ring; under the
  classic HUD the game's space-HUD panel (`uidraw::HudPanel`, 55 % / 80 % picked) over a ~15 % navy disc.
- Evidence (`research\experience\runs\hotfix_021_2026-11-02\`, free-roam SaveC705, popups stepped): `k_ctx_idle_hud_gone-012517`,
  `k_ctx_steering_ring_speed-012519`, `k_ctx_boost-012526`, `k_full_mode-012536`, `w_wheel_hud2-012541`,
  `w_wheel_classic-012545`; the card waking on a target switch `k_ctx_target_switch_card-012314` (M01 save: SaveC705 had
  nothing in the ring's cone). Lint: contextual idle PASS, target card PASS / WARN, full WARN - 0 FAIL throughout.

### 5.26 The 0.2.1 hotfix, third round: panels sized to content, the compact wheel, the mouse wakes the ring (2 Nov 2026; built; partly verified)
- **Panels sized to their content** (the owner: "they only need to be as big as the content they display"): Standard's
  target card (with the threat / contact list under it: the wider of the two), ship panel and loadout base panel take
  the width their texts, icons and paddings need (`hud2_parts.cpp` / `hud2_click.cpp` `*ContentWidth`, mirroring the
  Draw functions' columns; the loadout counts a gun row's two-line wrap while a spare line is left), floored at
  0.30 / 0.22 / 0.26 H x k (`hud2_layout.h` kCardMinW / kShipMinW / kLoadMinW), eased in `hud2.cpp` (grows at 14 / s,
  shrinks at 5 / s and only past 0.012 H x k: kContentHyst - a ticking value never breathes the panel; a new scale or
  screen snaps), anchored to their corners, margins unchanged; the prompt lanes follow the panels' edges as before.
  **Maxima, not fixed widths now:** `std_ov::kCardW` 0.44, `kShipW` 0.31, `kLoadW` 0.40 / `kLoadWKbm` 0.44 (and the
  spec's card / ship / loadout widths they stand in for). Fixed as before: every height, the stacked consumables' cross
  (`kConsW` / `kConsWKbm`), Focus's corners (its compact card, free-text bottom-left and fire strip keep their widths).
  The layout core takes the eased widths as `Measures::cardW / shipW / loadW` (0 = unmeasured = the maxima: the pinned
  geometry tests unchanged; `hud2_layout_test` pins the fit, the cap and the easing).
- **The compact wheel** (both looks, the owner's addition): a label of two or more words on two lines
  (`promptfit::SplitLabel`, the prompts' two-line rule: AUTOPILOT / GOALS, CRUISE / DISRUPTOR, CONTACT / FILTER ...) at
  the same size; the radius the smallest at which every plate clears the reticle ring + 0.035 H x k (its arcs and
  chevrons; the classic: 0.09 H) and every other plate by 0.010 H x k (`wheel.cpp` CompactRadius), at most the old
  0.2 H; the disc fits the plates' outer corners. The picked name repeated under the ring is gone in both looks (it
  crossed CONTACT FILTER / FORMATION; the picked plate is highlighted).
- **The mouse wakes the ring** (hud_fade 1): `hudwake::Steer` also fires while the pad does not own the input in space
  and the game's mouse flight is engaged (`exe::MouseFlightEngaged`) with the cursor 0.03 H off centre, or the game's
  cursor moved over 0.004 H in a frame.
- **The mouse cursor hides too** (hud_fade 1; the owner: "100% full contextual hiding and re-appearance"): in flight
  with the keyboard and mouse the game's cursor hides after 3 s without mouse movement or a button
  (`hudwake::CursorStep`, `kCursorIdleS`; test `tests\hudwake_test.cpp`) and shows the frame either comes. Shown in
  every other context: not Mode::Space, the pad owning the input (its virtual cursor), a flight modal, HUD 2 not
  active or a menu over it, a cinematic, any non-HUD window (`OnlyHudOpen`: the pause menu, popups, NeuroNet windows),
  the options page, a FLUF input window (`flufaware::InputWindowOpen`), mouse flight engaged, an unresolved mouse
  symbol. Mechanism: the game's own cursor-shown flag through HostApi v20 `SetCursorHidden` (the pad screens' path,
  `FREELANCER_RE.md` "Hiding the arrow"), the two sources OR-ed in `impl\host.h` `HostCursorHide`. `hud2 state` prints
  `cursor=shown|hidden`.
- `docs\UI_SPEC.json` bundles: Focus's hud_fade 2 -> 1 (2 is retired), `core\ui_meta.h` regenerated.
- Evidence (release, main install, SaveC705): `r3_panels_calm-013621`, `r3_panels_combat-013627` (the ship panel
  narrower: 0.31 -> its content; the loadout stays at its 0.40 maximum: the launcher row "JAVELIN MISSILE 36" + LOCKED's
  column need more). Lint calm / combat / wheel: 0 FAIL (1-2 WARN). **Not verified:** the wheel frames
  (`r3_wheel_hud2-013628`, `r3_wheel_classic-013631`) caught no open wheel (the debug `wheel open` did not show it this
  run - not diagnosed); the mouse wake - the harness's virtual mouse (`mouse x y`, `mbtn`) did not reach the game's cursor
  in the background run (`state` read mouse=(1920,1080) buttons=0x0 throughout), so the trigger never saw motion.

### 5.27 The 0.2.1 hotfix, fourth round: the nav and the picker sized to content, real free room (2 Nov 2026; built; partly verified)
- **The nav** (Standard and Focus): its box's right edge and bottom and its soft scrim follow the drawn lines
  (`hud2.cpp` ComputeLayout: at once when they grow, `EaseContent` when they shrink - gently, only past kContentHyst);
  Standard's box height = the drawn bottom + 0.01 H, floored at `kNavMinH` 0.10 H x k, today's 0.29 / 0.338 the maximum
  (`hud2_layout.h`; 0 = unmeasured = the maximum). The formation / waypoint bar, its ETA and the AUTOPILOT / TURRET VIEW
  chips now count in the nav's extent (they were outside it). The text cap `navMaxX` is unchanged.
- **The target picker** (`picker.cpp`): width = header / category strip / the widest row over EVERY category (switching
  categories never resizes it), rows = the fullest category's (floor 3, at most 7); the old 0.42 H x ts / 0.45 W and 7
  rows the maxima; eased the same way, right edge and screen middle anchored; columns from the measured widths.
- **Real free room** (the owner: "make full use of horizontal and vertical space that is allotted and won't be scaled
  down and removed"; a label scrolls / wraps / shrinks only past the room its element keeps anyway): the rule for every
  HUD 2 marquee. Applied: the loadout's launcher row (`hud2layout::RowRoom`, unit-tested) takes LOCKED's column only
  while LOCKED shows. **Not yet applied** (open): a second line in a panel's spare height before a marquee, and an audit
  of the other marquees (ship names, card name, threat list) - they already take their row's width to the panel edge.
- Tests 0 failures (hud2_layout: nav height fit / floor / cap, RowRoom). In game (release, main install, SaveC705):
  `hud2 lint` K&M calm ran (first finding WARN prompts.pairs); cursor: idle 4.5 s -> hidden; nav map
  (`tap USER_NAV_MAP`) -> shown, and still shown after it closed. **Not verified:** the pause menu - `key 0x1B` did
  not open it in the background game (mode stayed space, cursor stayed hidden), so the menu re-appearance is untested;
  the r4_ frames - the capture helper failed (no PNGs written), so there are no before / after frames.

### 5.28 The 0.2.1 hotfix, fifth round: real room everywhere, a second line before a marquee, the cursor's fail-safe (2 Nov 2026; built; verified in game except the long-launcher wrap)
- **The rule** (closes 5.27's open items): a HUD 2 label scrolls only past the room its element keeps anyway. Panels
  size to their one-line content up to their maximum (`LoadoutContentWidth`'s gun rows now measure one line: round 4
  measured the narrowest two-line split, so a row wrapped inside a panel that could still grow); where the panel keeps
  spare height a too-long row takes a second line before it scrolls: `hud2layout::TwoLineSplit` (pure, unit-tested:
  the most whole parts that fit line one, at least one; no break = marquee) for the gun rows (`drawNames`, split at the
  separator) and the launcher row (`DrawLoadout`: its first words on the free gun line above it, the whole width to the
  right edge; only when the rows above left that line unused). The other marquees audited: ship name, card name /
  faction (`ClearRoom`), threat list, nav lines already take their row's real room; the ship name and the threat list
  rows now go through `hud2layout::RowRoom` (the threat row reserves its distance gap only when a distance shows).
- **The target picker** (`picker.cpp`): the category strip neither scrolls nor wraps, so it is the width's floor up to
  0.45 W (at the 0.42 H x ts cap it ran past both edges at 4K, frame r5_picker_full); the rows use that width too
  (the lane names no longer cut).
- **The cursor's fail-safe** (`controller.cpp`): eligible for the idle hide only when the debounced AND this frame's
  detected mode are space and the game is not paused (`fl::GamePaused`), so every non-flight screen shows it the same
  frame; a disabled logic releases the idle source before its early return.
- Verified (release, main install, SaveC705, 4K, `run hotfix_021_2026-11-02`): tests 0 failures (59 tests); cursor
  idle 4.5 s -> hidden; `Send-FL pause` -> paused=1, shown (still shown 4 s later); resumed (`click PauseButtonBox0`) ->
  shown at +0 and +1.5 s, hidden at +4.5 s. `hud2 lint` 0 FAIL: K&M calm WARN prompts.pairs (13 prompts, cap 5); pad
  with the picker open WARN budget (boxed 11.5 %, budget 10 %: the wider picker counts). Frames r5_flight_full,
  r5_nav, r5_ship, r5_loadout (live "JAVELIN MISSILE" fits one line), r5_picker_full (before the picker fix),
  r5_picker2_full / r5_picker (after). **Not verified in game:** the launcher's second line (no live or sim launcher
  name is long enough; unit-tested only).

### 5.29 The 0.2.1 hotfix, sixth round: the pad's cursor, heights to content, turret view, fire wakes all (2 Nov 2026; built; verified in game except the K&M fire keys)
The owner's test findings: (1) the game's cursor now hides with the pad in control too (`core\game\hudwake.h`
CursorStep: 3 s, the same rule; the pad's own aim / steering are not activity, a real mouse hands the device back and
shows it; menus / map / modals stay the fail-safe; controller.cpp). (2) The ship panel and the loadout base panel fit
their HEIGHT to their rows (`hud2_parts.cpp` ShipContentHeight / LoadoutContentHeight; floors `hud2layout::kShipMinH`
0.09 / `kLoadMinH` 0.075, the old 0.15 the maximum, eased with the widths' hysteresis, anchored to the bottom margin;
Standard's launcher row keeps its pad over the bottom edge); A5's equal-height rule (5.19) is dropped, the
consumables' cross stacks on the loadout's new top. (3) Turret view out of a fight is calm flight for CONTEXTUAL (it
forced the whole HUD before); entering / leaving it wakes nav, ship and ring (the chip), then the HUD fades. (4) Any
fire input's PRESS - guns, launcher, CM / mine / torpedo / disruptor, fire-all, a group trigger, locked or empty -
wakes every element (`hudwake::FireInput`; the pad's in controller.cpp, the K&M keymap's keys in hud2.cpp, only with
the game's window in front). Run hotfix_021_2026-11-02 (release, SaveC705, pad): idle 4.5 s cursor=hidden kbm=0; an
RT tap from shown=0 -> shown=31, back to 0 after the linger; turret entry shown=19, idle 6 s shown=0; frame
r6_loadout (Starflier: one group + an ungrouped gun + the launcher: loadout 264 px = 0.122 H, the cross on its top;
ship panel full 0.15 with CREDITS). **Not verified:** the K&M fire keys in game (the test game runs in the background:
the foreground gate holds them off), the live `hud2 lint` (only armed; unit tests 0 failures).

### 5.30 Reported issues: weapon groups on the pad, the torpedo row, the hull's colour (4 Nov 2026; GitHub issues 1, 4, 5; built; run in game: dev build, SaveC706, background)
**Weapon groups on the pad** (`wgroups.*`): the action wheel's EDIT GROUPS opens a list of the ship's guns for the active
group, over the HUD in both HUD styles: A switches the focused gun on / off (USER_TOGGLE_WEAPONn), D-pad up / down the
gun, LB / RB the group 1..3 (USER_WEAPON_GROUPn: the group edited is the active group, the HUD's rows follow), B done;
the sticks keep flying, RT keeps firing. The game stores the groups, no state of ours. Guns only: launchers fire on their
own buttons in every mode (UI_PLAN 6.4). It replaces UI_PLAN's ticks in the inventory, which were never built. Y held
(`weapon_groups` 1) and the wheel's WEAPON GROUP go to the next of the first three groups with guns, from the game's
active group (`controller\src\impl\wgroups_rules.h` NextGroup: an empty group fired nothing). The game's number for a gun is its entry in
HUD_Status's list, read through HUD 2's hiding since this round (FREELANCER_RE "HUD 2 readers"); without the list the
rows grey out and A does nothing, never a guess.
**The torpedo row:** the torpedo hardpoints' launcher left the launcher row (its rounds were added to the missiles' count
under the missile's name) for a row of its own on the third gun line, in the gun names' column: `HOLD · name` on the pad,
`key · name` in the keyboard form, the count at the right end (amber at 0). So a torpedo ship shows two group rows, the
active group kept, the guns in no group dropped with two rows full. One RB glyph between the torpedo and launcher rows,
as RT beside the gun rows: two 0.036 glyphs overlap at the 0.030 pitch, and a keycap in the row broke the lint in the
keyboard form. A cruise disruptor on that hardpoint takes the row when no torpedo does (no prompt: it fires from the
wheel). Focus's strip has the same row, its count always (the band's MSL n is the launcher's). Debug state `hud2 sim torpedo`.
**The hull's colour:** the ring's arc and number and the target card's bar and number are the game's red (`kHull`), white
gone (the owner, 4 Nov 2026: no option, no need to keep it). The escorts' bars stay friendly green. The low states (the
owner, same day; `hud2_parts.cpp` LowPulse, 1.5 Hz): at 25 % and below the shield keeps its blue and pulses to hostile red,
the hull pulses to bright red (`kHullHot`) with a bloom, two wider faint layers of the lit arc under it. Arc and number, in
Standard and Focus; `shield_cue` / `hull_cue` 0 switch them off (the SHIELD FAILING / HULL CRITICAL cards stay). Debug
state `hud2 sim critical`.
Run: `hud2 lint` 0 FAIL over pad / keyboard x Standard / Focus x `torpedo` / `combat` / `notarget` / `travel`; frames of
the row, the editor and the nine-slot wheel at 4K; every button of the editor on the virtual pad; two groups kept apart
over switches; Y held 1 -> 2 -> 1, and staying on 1 with no other group armed. **Not verified:** a real pad, a save and
reload of edited groups, a ship with more than ten weapons, a cruise disruptor on the row.

### 5.31 The owner's play notes: the tractor's prompts, the right stick, the enemies cue, the NeuroNet box (4 Nov 2026; built; run in game: dev build, SaveC705, background, virtual pad)
The owner, after playing the 4 Nov build: "I'd like the tractor prompts at the bottom of the screen to note the object's
name and range. Also, it's not prompted anywhere that the A button also tractors something. Prompting should clarify which
prompt does what (tractor specific, tractor all, etc)." / "Even when the target panel is hidden, when there are enemies
nearby the top right corner of the screen should mention there are enemies nearby, and prompt the player to press the
right stick to target nearest enemy. In general, right stick should default to target nearest enemy for a first press, and
only consecutive presses should be 'next target'." / the classic NeuroNet box beside HUD 2 after loading the convoy save.
- **A tractors one item, X all** (`controller.cpp` ContextAction): A on targeted loot in reach = USER_TRACTOR_BEAM (the
  game's "Tractor Beam (Selected)"); with no target A targets the nearest loot in reach and tractors it; targeted loot out of
  reach = GO TO. X = USER_COLLECT_LOOT (all). Until now A and X both sent COLLECT_LOOT. "In reach" = within the tractor's
  max_length and `CShip::get_space_for_cargo_type` > 0 (loot the hold cannot take is not offered).
- **The prompts** (`hud2.cpp` TractorCtx, right lane): `[A] TRACTOR <ITEM> · <range>` (the target, or the nearest with
  no target; not in ASSISTED, where A starts the autopilot; the keyboard's one-item key only on a target), `[X] TRACTOR
  ALL n` (one item that A does not name: its name and range). Focus keeps its band's LOOT n and gains A's item prompt.
  The keyboard form: A's form shows USER_TRACTOR_BEAM's key, X's USER_COLLECT_LOOT's (it showed the one-item key for
  "all"). The classic HUD (`padhud.cpp`): the same rows in the legend, the world prompt names the item and its range.
- **RS: enemies first** (`target_cycle.h`, unit test `tests\target_cycle_test.cpp`; `controller.cpp` SmartTarget): with
  a hostile ship within 10 km (`ctl::NearbyEnemy`), a cold press takes the nearest; a press within 3 s with the target
  the last press left = the nearest enemy this run has not visited (all of them once, then again); with no enemy near,
  TARGET AHEAD as before (the ring, then the game's nearest / next enemy). Set natively (space::SetTarget). Labels:
  NEAREST ENEMY when enemies are near, else TARGET AHEAD (HUD 2's calm lane, the classic legend; the fight legend NEAREST /
  NEXT ENEMY).
- **The enemies cue** (`hud2_parts.cpp` DrawEnemyCue): the pad, nothing targeted, enemies near, not the autopilot or
  the turret: under the LB hints in the card's corner, `n ENEMIES NEARBY` (hostile red) and `[RS] TARGET NEAREST`, eased
  0.12 s, not under the contextual fade; the HUD bus's keep-out grows to 0.135 H below the card's top.
- **The NeuroNet box** (`hudvis.cpp`): not reproduced in 44 harness reloads (space to space and docked, the convoy save
  SaveB001). The one gap found: the switch buttons cached under the box's root were never re-checked, and a load that
  put the new root at the old address (the allocator reused root addresses in the runs) would leave them pointing into
  freed blocks whose bit 1 kept the box shown. Now a cached button counts only while its parent is still the root, the
  buttons are found again every 2 s, and the box kept up logs `hudvis: the NeuroNet box stays shown: <button> is open`.
- Run (`research\experience\runs\hud_notes_2026-11-04\`): RS on three pirates: cold 25 (nearest), 27, 26 in a run, cold
  after 4 s 25 again; the cue at 4K (frames a_, e_ / g_sim_notarget); with no target `[A] TRACTOR DRAIL · 1.1 km`,
  `[X] TRACTOR ALL 12`; K&M `[SHIFT+B] TRACTOR ZAPHIEL TURRET · 721 m`, `[B] TRACTOR ALL 3`. Six trials with the ship
  stopped beside 18 items: a target set first, A pulled exactly it twice; the third (the farthest) had not arrived after
  4 s; with no target A pulled the nearest three times, once with a second item. The nav map open keeps the box (logged),
  closed hides it the next frame. `hud2 lint`: the cue's rows first 0.0061 H apart (FAIL) -> 0.01 H; after, 0 new FAIL
  (the known `loadout '3' / 'JAVELIN'` FAIL stays). **Not verified:** a real pad, the classic HUD's rows on screen, Focus.
