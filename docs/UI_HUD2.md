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
| 5 weapons | a row per weapon group that has guns, numbered as the game numbers it (no "2 -" / "3 -" rows; the active group kept when more than three have guns; a click selects that group), the guns in no group dimmed in a row of their own without a label; the launcher row in three columns: the name (its last words dropped first: "JAVELIN", "SLINGSHOT"), the ammo always (amber at 0), LOCKED in its own column at the right edge; the loadout 0.62 H wide (0.66 H in the keyboard form) |
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
| also | the alerts arm 1.5 s after entering space and only on a live hull (a load read the hull as 0 for a moment: HULL CRITICAL · NO NANOBOTS on a whole ship in the first release capture); two middle dots typed as literal characters went in as cp1252 "Â·" (the wrap never found its separator) - escapes only, as the files say |
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
