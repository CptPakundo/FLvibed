# Task: controller-first UI (pad HUD, pad screens, glyphs), switching live with keyboard/mouse

**Status (25 Oct 2026): built** - the design the owner decided in the mock-up (`..\..\docs\UI_PLAN.md` 6,
`..\..\docs\UI_SPEC.json`) is implemented (`..\..\docs\UI_BUILD.md`: architecture, what is built, the gaps); the open
questions of section 8 were answered by the spec's 21 decisions; section 5 holds the HUD badge rules as rebuilt after
the owner's review. The rest of this document is the brief and the research it was built from.

Read `..\CLAUDE.md` first (workflow, harness, rules). Research behind this brief:
`research\ADAPTIVE_UI_RESEARCH.md` (how modern games do it, accessibility first). The autopilot goal UI that builds on
it: `AUTOPILOT_UX.md` (section 5).

## Goal
Roadmap 1 and the controller module's primary goal (user): like modern AAA PC games, FLvibed has **two interaction
layers over one game**: Freelancer's own keyboard/mouse UI, and a **controller UI built for the pad**, switching live
with the last-used device, without disruption or loss of game logic or function, intuitive and fast in both.

**Not glyph swaps** (user, 17 Oct 2026): Freelancer's UI is mouse-led (a cursor everywhere, a nav map you click into,
small contact-list rows, maneuver buttons). The research confirms that strong pad UIs keep the game's logic and data
and replace the *interaction surface*: Diablo II: Resurrected swaps its whole HUD for a pad HUD in one frame, Baldur's
Gate 3 drops its hotbar for radials and world prompts, Crusader Kings III and Anno replace the cursor with a fixed
reticle, tabs and radials. So this task has three parts, in priority order:
1. **Pad HUD and pad screens:** new or restructured UI where the mouse UI does not fit (sections 4-5).
2. **Prompts and glyphs:** a context prompt bar on every pad screen, glyphs in text (tutorials, tooltips), focus
   always visible.
3. **Accessibility:** single-press alternatives for every chord, full pad remapping, holds ↔ toggles, deadzones,
   text scaling, presets (section 6).

Constraints (unchanged): native game logic only (every pad action ends in the game's own call; no cursor emulation for
UI); resolution / aspect / DPI agnostic (scale with the UI camera, not pixels); Borderless Windowed first; Xbox
glyphs (the user's Elite Series 2); casual, assistive feel; everything testable out of focus.

## 1. Design principles (from the research)
1. **Two layers, one state.** Keyboard/mouse keeps Freelancer's UI untouched plus FLvibed's additions (usable by
   mouse). The pad gets its own HUD layer and screens. Both read the same data and end in the same native calls;
   switching device changes the view, never the state (a selection, an open screen's context, the autopilot queue).
2. **Instant switch, no flicker.** Last input wins (per-device timestamps, noise floors), the swap is immediate, and
   a manual lock exists (Automatic / Controller / Keyboard-mouse) because flicker is the most reported failure.
3. **Two pad navigation modes, never the game's mouse** (user, 18 Oct 2026). Default: **auto-select focus** (D-pad /
   bumpers move a highlight between the screen's elements, lists, tabs, pickers). Option: a **Destiny-style pad
   cursor** on the left stick where pointing is natural (room clickables and characters, dealers, job board, dense
   menus, the nav map like AAA controller maps): FLvibed's own cursor, drawn by the overlay, with magnetism / slowdown
   over targets, acting through native hover and click (hit test at its position, then the control's or character's
   own handler), not by moving the game's mouse. Pressing one of the screen's direct buttons (D-pad, bumpers) returns
   to auto-select. Free flight steering is unaffected (the cursor exists only in UI screens).
4. **One grammar everywhere:** **A accept / select / yes** (the industry standard; Y may only be a shortcut from
   outside a screen, e.g. the job board from the bar), B back (always, from everywhere), X / Y contextual, LB / RB
   tabs, LT / RT pages or zoom, Start = game menu, **View = NeuroNet hub** (map first). A **bottom prompt bar** on every
   pad screen lists the valid actions; the focused element is always clearly highlighted; focus starts where most
   things are near.
5. **Show what is bound where the thumbs are** (D2R): the pad HUD labels its slots with the buttons that use them.
6. **Bring information to the player** (GoW): status, objective, autopilot reasons on the HUD rather than behind
   menus.
7. **Restructure dense screens** (step-by-step flows, two lists side by side, panels beside the world), don't port
   them.
8. **Accessibility is a design input from day one**, not a later options page.
9. **Default bindings are ergonomic** (user, 18 Oct 2026): they make sense in context, follow industry standards and
   flow well in the hand. **Core functions get one-button binds**; two-button chords (the LB layer, the pad's
   equivalent of other games' radial menus) only for secondary functions (e.g. switching the contact-list filter).
   The current defaults (controller CLAUDE.md, "Controls summary") are reviewed in a **binding pass** before the pad
   HUD shows them: consumables need better, more intuitive buttons (the user has a major note on this, pending), and
   opening the target picker by holding D-pad up is ergonomically doubtful (research item, section 8). Research and three proposed presets (Dogfighter, Ace, Assisted): `BINDINGS.md`.
10. **Contextual, never nagging:** prompts appear when they add information or a decision is pending; the autopilot is
   optional, so its hints appear only for players who use it (setting), never by default to everyone.

## 2. What already exists
- **Overlay hook:** `impl::OnRenderOverlay()` every frame inside Freelancer's UI rendering, after the HUD/UI and
  before the cursor (host detour on 0x41F150); empty today. `OnDeviceLost/OnDeviceReset` around
  `IDirect3DDevice8::Reset`.
- **D3D8 device:** captured by the host (`g_host->GetD3DDevice()`); HDE renders through a D3D8 wrapper. D3D8
  vtable indices in `..\..\docs\FREELANCER_RE.md` are from memory: verify. No d3d8.h in the SDK; `hook.h` has
  `VCall<>()`. Reference: FLUF's Dear ImGui D3D8 backend
  (`..\..\research\repos\FLUF\FLUF\Source\Client\Ui\ImGui\ImGuiD3D8.cpp`, `UiManager.cpp`).
- **Positions:** `ui::Project()` maps UI space to pixels via the UI camera (0x67DBF8); every control has a position
  and half-extent box (`ui::Collect/CollectAll`, debug `uiitems`). 3D: `fl::WorldToScreen`.
- **Device ownership today:** `controller.cpp` `padOwns` = pad tick vs physical mouse tick (`dimouse::LastRealTick`,
  which already excludes FLvibed's injected mouse); the keyboard is not counted yet. HUD transparency while the pad or
  an autopilot flies (`SetHudSuppression`, HitTest detour 0x59DD80).
- **Menus with the D-pad (works, virtual pad):** auto-focus, lists, modal windows; it moves the game's cursor onto the
  focused control (`SnapTo`) and clicks. This is cursor emulation: to be replaced screen by screen by native focus
  + native click (`click <Control>` exists for text, icon and front-end buttons).
- **Native game interaction already available** (`..\..\core\game\`): room characters (list, hover, talk:
  `room::`), room hotspots and NavBar (`room::Hotspots/Goto`, busy gates), front-end and pause menu buttons, load by
  index (0x4880F0), pause actions (0x4A7210), native target selection independent of the contact-list filter
  (action dispatcher 0x54BCB0, `space::`), maneuvers (dispatcher 0x4D8E20), waypoints (append 0x4C4780, select
  0x4CEEB0, clear 0x4C46C0), the player ship's AI and the Advanced Autopilot (`ai::`, `autopilot.*`).
- **Text:** popup caption / text ids in the event log; string ids via `events::IdsText`; key-name text from 0x577F50
  (FLUF: binding to display text). Multi-line popup texts are infocards.
- **HUD layout references:** HDE's `HudShift.dll` (moves HUD elements, `EXE\HudShift.ini`) and `HUDless.dll`, both
  loaded in this install: pad HUD anchors must follow HudShift's layout.

## 3. Architecture
- **Input-device service** (controller logic): active device `Pad | KbMouse` from pad input past deadzones
  (`g_pad.AnyInput`), physical mouse motion past a few pixels / buttons (`dimouse::LastRealTick`) and **keyboard**
  (new: the game's keyboard handler `WinKey` 0x577850 / `HandleKey` category 1, ignoring FLvibed's own injected keys
  and commands). Hysteresis only against noise; the switch itself is immediate. Setting `input_mode` = auto |
  controller | keyboard (manual lock). Every pad-UI piece subscribes; switching never releases a command the other
  device still holds, never closes a screen.
- **Renderer:** own D3D8 quads + text in `OnRenderOverlay` (state block saved/restored; survives Reset), or FLUF's
  ImGui D3D8 backend adapted. Scaled from the UI camera scale; anchored to named HUD controls so HudShift layouts work.
  Decide in phase 1 (open question 1).
- **Glyph atlas:** Xbox glyphs (Elite Series 2) + keyboard/mouse key caps; text with inline glyph tokens (GoW's icon
  font idea: writers / code embed `{A}` / `{LB}` in a string, the renderer draws the glyph for the active device and
  the current binding).
- **Prompt bar component:** a list of (action, label, enabled) per screen state; glyphs from the current bindings;
  the same component in pad and keyboard modes (keyboard shows key caps; for the game's own screens the keyboard
  prompts are hidden, as the game already shows its UI).
- **Pad screens** (FLvibed-drawn): each a small state machine over game data with a focus model, a prompt set and a
  native action per entry; in keyboard/mouse mode either a clickable twin (mouse hit-testing over an invisible native
  control, `CreateControl` 0x59DB20, so the game's click-vs-fire arbitration applies; or the HitTest detour) or a
  hand-over to the game's own screen with the selection carried over.
- **Pad HUD layer:** drawn over the game's HUD; hides the mouse-only HUD elements in pad mode (their controls become
  invisible / non-hit-testable while the pad is active; back instantly on a mouse move).
- **Focus without a cursor:** for game screens kept as they are (class a), D-pad focus selects a control and A calls
  its native click directly; the game cursor is parked and hidden in pad mode (no hover effects from a stale cursor).

## 4. Screen audit
Classes: **a** = already works with D-pad focus: prompts + native focus only; **b** = modified pad interaction on the
game's screen; **c** = new controller-first screen (FLvibed-drawn; the game's screen stays the keyboard/mouse view).
Effort: S / M / L (RE included).

| Screen / element | Class | Pad flow (sketch) | Native calls (known / RE needed) | KB/M view, switch | Effort |
|---|---|---|---|---|---|
| Main menu, New/Load game, character select | a | D-pad focus, A select, B back; Load: A loads (no double-tap) | front-end buttons (id handlers), load list (+0x8A4 rows) / known | game's screens; focus ↔ hover | S |
| Options (video, audio, controls) | b | tabs LB/RB, sliders with D-pad left/right, explanation panel per setting | option controls / RE | game's screen | M |
| Controls page (key bindings) | c | FLvibed **controller settings** screen: pad remap with a controller diagram, presets, layer latch, deadzones, prompt lock, text size | FLController.ini + live apply / known | game's key binding page stays for keys | M |
| Pause menu (`NN_Pause`) | a | focus, A, B = resume | 0x4A7210 action codes / known | game's | S |
| Death / mission failed dialog | a | focus on Load autosave, A | named text buttons / known | game's | S |
| Tutorial / mission popups (`NeuroNetMissionMessage`) | a + text | A = OK; key names in the text replaced with glyphs in pad mode | `MMOkButton`; text hook (infocards) / RE for text | game's text with key names | M |
| Mission offer (accept / decline) | a | A accept, B decline, prompts | `OfferDialogAcceptButton` etc. / known | game's | S |
| Save game (name entry) | c | auto-name default + on-screen keyboard (roadmap 3) | save dialog / RE | game's text field | M |
| Base: room navigation (NavBar) | b | **LB / RB cycle rooms**, Y = launch (when allowed), prompt bar names the rooms; optional pad cursor over the room's clickables (left stick), a direct button returns to auto-select | `room::Hotspots`, `room::Goto`, launch record / known | game's NavBar | S |
| Bar characters | c | character strip (portraits + offer type: story first, mission, rumour, bribe), D-pad cycles, focus = the game's own hover highlight, A talk; or point at a character with the pad cursor | `room::` characters, vt[10] hover, vt[12] click / known | 3D hotspots by mouse | M |
| Mission board (job board) | b | the game's list + details window adapted: D-pad focus on its list, **A accept job** (confirm), RS scroll, B close; optional pad cursor (mock-up 7) | mission vendor (hotspot behaviour 3) / RE | game's board | S-M |
| News vendor | a/b | list, RS scrolls the text | / RE | game's | S |
| Commodity trader | b | the game's window is already two lists + a transfer panel: LB/RB switch side, D-pad item, LT/RT quantity (hold = fast), A buy / sell, Y max, profit vs. known prices added; optional pad cursor | dealer window 0x449D10 / RE buy-sell | game's trader | M |
| Equipment dealer | b | same two-list window (mock-up 8): LB/RB side, LT/RT quantity, A buy / sell, X category filter, compare panel (equipped vs selected) added; optional pad cursor | / RE mount, buy, sell | game's dealer | M |
| Ship dealer | b | LB/RB cycle the offered ships, compare stats, A buy (confirm, trade-in shown); optional pad cursor | / RE | game's | M |
| Repair (planet repair, ship dealer) | a/b | A repair all, prompts | / RE | game's | S |
| Space HUD: maneuver bar (Goto / Dock / Formation / Free flight / Cruise) | c (hidden in pad mode) | actions already on pad buttons; the pad HUD shows a compact action cluster with glyphs (A dock, Y cruise, LB+A goto...) and the autopilot status line | maneuver dispatcher 0x4D8E20 / known | game's bar | S |
| Space HUD: contact list + filters | c | **pad target picker** (AUTOPILOT_UX 5.4a): category tabs Threats / Mission / Navigation / Ships / Loot, relevance-sorted large rows; quick D-pad targeting stays; the game list's **filter switched by a two-button bind** (binding pass); how the picker opens is a research item (hold D-pad up is doubtful) | action dispatcher 0x54BCB0, `space::` objects / known | game's list (filter buttons hidden in pad mode) | M |
| Space HUD: target view (scan, comm buttons) | b | X (LB layer) scan, comm on a prompt; target info stays | USER_SCAN_CARGO etc. / known | game's | S |
| Space HUD: weapon groups, consumables, loot | c | **pad tray** (D2R model): weapon group, nanobots / batteries / CM / mines with their D-pad glyphs and counts, tractor (X) | commands + `space::PlayerWeapons`, ship capacity readers / known | game's buttons | M |
| Space HUD: NeuroNet buttons (nav map, journal, inventory, reputation, info) | c (hidden in pad mode; the freed top-right spot may host a re-adapted HUD element: TBD) | **View opens the NeuroNet hub, map first**; LB/RB (or D-pad left/right) switch Map / Journal / Inventory / Reputation / Info (user, 18 Oct 2026); pad cursor option inside | NN button controls / known names | game's buttons | M |
| Nav map | c | **left-stick map cursor** like AAA controller maps (magnetism to objects, slowdown, the map pans at the edges; user, 18 Oct 2026) + the **destination list** (systems → bases / gates / points of interest, known + reachable, route length + danger) synced with it; LT/RT zoom; **A selects, A again on the selection plots it as a waypoint** (user, 18 Oct 2026); the game supports **multi-waypoint routes** (its waypoint list, up to 99): each further A-A appends the next waypoint, X removes the last one (hold X: clear the route); Y travels the route with the autopilot | waypoint API / known; map selection, zoom, best path / RE | game's nav map, opened centred on the picked object | L |
| Inventory | c | tabs (cargo, equipment, ship), list + detail, A use / mount, X jettison (hold to confirm) | / RE | game's | M |
| Mission log, rumours, info, reputation | b | LB/RB tabs, D-pad list, RS scroll | / RE | game's | S-M |
| Comm window, scan cargo window | b | list focus, A select | / RE | game's | S |
| Cutscene skip | b | hold B (with a fill ring) where the game allows skipping | USER_CANCEL under the busy gates / known | Escape | S |
| Text entry (chat MP, save names) | c | on-screen keyboard | CTID control / RE | keyboard | M |

## 5. Pad HUD (space) - as built (`src\impl\padhud.*`, `glyphs.*`; 25 Oct 2026, after the owner rejected the first badges
as "tiny and not placed in a way that looks natural to the HUD")
Pattern: D2R's labelled slots (the button glyph on the slot itself, scaled to it) + BG3's world prompts + the shared
prompt bar. The game's HUD stays; the pad layer badges its controls (`docs\kb\hud-controls.md` names them) and adds the
legend, the status line and the world prompts. Evidence: `research\experience\runs\ui_glyphs_2026-10-25\` (before /
after, 4K and 720p, crops per region).
- **Size: a badge is scaled to its control**, 0.45 of the control's height, at least 0.022 H, at most 0.036 H, all of it
  times `text_size` (4K: the maneuver bar's 156 px buttons get 70 px glyphs = 0.032 H, the 78 px header icons and the
  NeuroNet buttons 47 px = 0.022 H). Never a fixed pixel size.
- **Placement, one rule: where the HUD's own frames put their tabs.** A badge is centred on its control and sits half
  across the frame line of the control's panel that faces the open screen: the top panels' bottom line (the maneuver
  bar's B / A / A-hold / View-hold under their buttons, the NeuroNet box's View under the nav-map button, the cruise
  box's Y on its bottom edge), the bottom panels' header top line (the D-pad over each consumable with the direction
  highlighted, Y-hold over the active weapon group). In a header with room beside the control the badge sits inside
  the band beside it: LB left of the contact list's expander, R3 right of its next-enemy icon. Never on a corner.
  The frame lines are measured from the controls' rects at 4K (`padhud.cpp Badges`): the top panels' line 0.0088 H
  under their buttons' bottom edge, the cruise box's bottom 2 hh under its centre, the bottom headers' line 0.025 H
  above their icons' centres; HudShift moves panels whole, so the offsets hold; the controls come by name from
  `ui::Collect` (visible ones only), so a hidden control has no badge, and the layer vanishes the frame the device
  switches. The A badge sits on goto or dock, whichever applies to the target. (Deviation from the brief: the D-pad
  cannot sit *between* a consumable's icon and its count - the gap is 25 px at 4K - so it sits on the header line over
  the icon.)
- **Lit with the element:** a badge's chrome goes the game's active yellow (252, 243, 68) when its control is lit - the
  running maneuver's bar button (`space::Pilot` op), the cruise box while cruising / charging, the active weapon group,
  the autopilot button while the autopilot flies - else the HUD's chrome blue (130, 201, 243).
- **Style = the HUD's own chrome** (`glyphs.cpp`, two atlas layers per glyph): the body in the panel base (13, 11, 26)
  at 0.8 coverage (the glyph backs itself only within its silhouette: no plates, no grey discs), outlines and the
  letters on bumpers / triggers / sticks in the tint colour (chrome or yellow), the letters in the HUD's face (Agency FB
  through `core\game\gamefont`), the face buttons in their family colours (Xbox A green / B red / X blue / Y yellow,
  PlayStation shapes, Nintendo letters), the D-pad one cross with the pressed arm white, View / Menu their marks. Drawn
  at 4x supersampling per needed size (one atlas per family and size).
- **The legend** (right-middle, contextual by state; `button_legend`): glyphs at the HUD font's height (0.029 H, the
  labels' em), rows 1.3 of it, labels in the HUD font, a thin frame-coloured edge line at the left; a long legend (combat,
  nine rows) shifts up to keep clear of the bottom panels' header line. The prompt bar and the modals draw the same
  glyphs at their text height.
- **Status line** under the maneuver bar (autopilot / turret / cruise charging; `ap_status_line`); **world prompts**
  only when they add information now (a dockable target in range: "{A} DOCK", loot in tractor reach: "{A} / {X} LOOT";
  `world_prompts`); the autopilot's prompts only for players who use it (`ap_hints` / used once).
- Not built (the plan's earlier idea, superseded by the badges): hiding the HUD's mouse elements in pad mode and a
  separate "pad tray"; the HUD stays whole and the badges label it.

## 6. Accessibility layer (part of every phase)
- **Chords → single presses:** a latched LB layer option (tap LB = layer on until the next action / timeout), and
  every LB-layer action also reachable from the pad game menu; LB + L3 (autopilot) gets a single-button alternative.
- **Full pad remapping** in the FLvibed controller settings screen, every prompt and glyph following the bindings;
  a reset to defaults that cannot be remapped away; presets (default, left-handed, one-handed / reduced).
- **Holds ↔ toggles / two taps** (B reverse, rear view, LB layer), adjustable deadzones / stick curves / cursor speed,
  rumble on / off / strength (never the only channel).
- **Text scaling** for FLvibed's UI (and subtitles: already a module), body text never below a minimum at 1080p,
  icon size, HUD backgrounds; colour never the only cue.
- **Menus navigable with one stick and one button** (no bumper required: tabs also reachable as focusable headers).
- **Settings UX:** each setting with a one-line explanation, changed values highlighted, last-touched setting stays
  focused, presets first ("Quick start / Guided setup" on first run with the pad).
- **The autopilot as the assist mode** (navigation assist, combat assist; AUTOPILOT_UX), plus a "status" readout
  (hull, shield, cargo, objective) on a button.
- Playtest with players who use adaptive controllers when FLvibed goes public (outreach: SpecialEffect, AbleGamers).

## 7. Phased plan (in-game verification per phase)
Each phase: background test (`deploy.ps1 -Background`), screenshots (`Save-FLShot`), release build, docs, commit;
**(user)** = the user tests the feel on the Elite pad and with a real mouse/keyboard (mouse input needs focus).
1. **Foundation:** input-device service (pad / mouse / keyboard, manual lock), renderer spike (one quad + text at a
   UI-anchored position, state block, survives Reset), glyph atlas, prompt bar component. Verify: event-logged device
   switches with no flicker over 10 min idle with the pad connected and the mouse untouched; switch latency one frame;
   screenshots at 4K and a 1080p window; HudShift layout. **(user)** live switching feel.
1b. **Binding pass** (before any pad HUD shows bindings): review every default against principle 9 with the user
   (consumables, target picker access, filter switching, one-button core actions), research how comparable games bind
   these, update the controller defaults and the controls summary.
2. **Native focus for class a screens + the pad cursor component:** pause menu, dialogs, offers, death dialog, main /
   load menus by native focus + native click, game cursor hidden in pad mode, prompt bar on each; the optional
   Destiny-style pad cursor (overlay-drawn, magnetism, native hit test + click, a direct button returns to focus). Verify with the virtual pad on SaveC701 / M01 saves;
   no cursor movement (`mouse` debug).
3. **Pad HUD (space):** tray, hidden mouse-only elements, world prompts, status line; pad target picker (shared with
   AUTOPILOT_UX phase 4). Verify SaveC702: each picker category selects the game's real target (`targetinfo`); the
   game HUD returns on a mouse move. **(user)**
4. **Bases:** room cycling, bar character strip, mission board, offers. Verify on Save2b9b (M01 bar, Juni) and
   SaveC701: talk story natively, accept a job; runbook M01a for the mission flow.
5. **Dealers:** RE buy / sell / mount, then commodity trader, equipment dealer, ship dealer, repair screens. Verify:
   buy / sell / mount round trips on SaveC701, credits and cargo read back.
6. **NeuroNet:** the View hub (map first, LB/RB tabs), inventory, logs, nav map with the left-stick map cursor + destination list (shared with
   AUTOPILOT_UX phase 5). Verify: route set by picker matches the game's waypoints; nav map opens centred on the pick.
7. **Settings and text:** FLvibed controller settings screen (remap, presets, latch, deadzones, prompt lock, text size),
   tutorial / tooltip glyph replacement, on-screen keyboard / auto save names. Verify: remap changes every prompt;
   reset works; tutorial popups in M01 show pad glyphs. **(user)**

Order rationale: 1 is needed by everything; 3 before 4-6 because flight is where the player spends most time and the
autopilot UI depends on it; dealers last among screens because they need the most RE (now adapted windows, class b).

## 8. Open questions for the user
1. **Look:** pad UI in Freelancer's NeuroNet style (blends in) or a modern FLvibed style? Renderer: own quads or
   ImGui (FLUF's backend)?
2. **Glyph art:** generated at runtime or shipped images (license: e.g. CC0 sets)? Xbox only, or also PlayStation
   glyphs for other players?
3. **Pad HUD:** *answered (18 Oct 2026): hidden in pad mode; status strip where the maneuver bar was, tray above the
   gauges are good starting points.* Open: what re-adapted HUD element, if any, takes the freed top-right spot?
4. **Radials:** use radial menus (Anno / BG3 style, sticky selection) for anything — e.g. a consumables / weapon-group
   wheel on a D-pad hold (Everspace 2) — or keep lists and the LB layer?
5. **LB layer:** add the latched-layer option (tap LB, then the action) for single-press play?
6. **Switching:** Automatic by default with a manual lock in settings — agreed? Pause the game when the active pad
   disconnects in flight?
7. **Dealers:** *answered by the mock-ups (18 Oct 2026): adapt the game's two-list windows (class b) with an optional pad
   cursor.*
8. **Nav map:** *answered (18 Oct 2026): a left-stick map cursor like AAA controller maps, with the destination list.*
9. **Text size / couch distance:** is a text-scale option for FLvibed's UI wanted now (GoW: ~60 % of players changed
   it), and should the subtitle size follow it?
10. **Free cursor (LS today):** *answered (18 Oct 2026): replaced by the Destiny-style pad cursor option (FLvibed's own,
    native hit test + click; not the game's mouse) in rooms, menus and maps; a direct button returns to auto-select.*
    Open: on by default in rooms / maps, or opt-in?
11. **Which base screen first after the HUD:** bar / mission board (story progress) or dealers (economy)?
12. **Binding pass** (principle 9): your pending note on consumables first; then one-button core actions, the
    contact-list filter on a two-button bind, and how the target picker opens (research: how comparable games open
    a target / wheel menu without a held D-pad direction).
