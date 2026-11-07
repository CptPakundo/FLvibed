# Controller UI: presets, device switching, glyphs, pad menus, assists, speedrun mode and LiveSplit
Status: current (the UI build of 25 Oct 2026 realised U15-U23; where the build departed from the research the bullet says so in place)
Checked against: FLvibed 3c9c1390 (6 Nov 2026, short form: 43 watched-path commits: the module split P0-P11e (moves, comments, map rows) and the controller.cpp split; the default binds changed (Y tap next group / Y hold cruise, A one item / X all, RS nearest enemy first), HUD 2 the default everywhere since 0.2.0, the Steam glyph family; marked in place)
Dirty paths at check: another session's uncommitted the private work queue, `tools/uimock/options.py` (not judged); 950b25cc, 4a270d9e and 3c9c1390 landed during the round, read
Watch: docs/UI_PLAN.md, docs/UI_SPEC.json, modules/controller/docs/BINDINGS.md, modules/controller/docs/TASK_GLYPHS_HUD.md, modules/autopilot/docs/AUTOPILOT_UX.md, modules/controller/src/impl/controller*.cpp, modules/controller/src/impl/gamepad.*, core/game/render.*
Evidence: `research\evidence\controller_ui\` - `hud_conventions_aaa.md` and `hud_space_games.md` (round 2, 25 Oct 2026 ~08:00, every claim with its URL; `hud_redesign\` the frames, renders and the proposal page); the three research files of 25 Oct 2026 in full (`flight_presets.md` 73 sources, `pad_ui_switching_glyphs_menus.md` 68 sources, `assist_speedrun_livesplit.md` 59 sources; every claim cited, forum opinions marked anecdotal) and the interactive mock-up page (`mockup_v2_index.html`; live at artifact `4v4dXbB5GTVt1URowFtSvN`); the game screens captured for it in `research\experience\ui_2026-10-25\screens\`
Sources: see the evidence files (web pages read 25 Oct 2026; nothing downloaded). Earlier rounds this builds on: `modules\controller\docs\BINDINGS.md` (12 games' pad layouts, 18 Oct) and `modules\controller\docs\research\ADAPTIVE_UI_RESEARCH.md` (device switching, pad menus, accessibility, 17 Oct)

## What it means for FLvibed (the design is in `docs\UI_PLAN.md` section 6; the owner decides in the mock-up)

*6 Nov 2026 (3c9c1390, short form):* the device rules hold and the defaults moved; every change came from the owner or a GitHub issue. **Binds:** Y tap = next group and Y held = cruise (91aee7bb, issue 15; `y_swap` the way back), A tractors one item and X all with named prompts, a cold RS press takes the nearest enemy (6365c9ac). **HUD:** `hud_style` 2 (HUD 2 always) and `ui_style` HUD 2 are the defaults since 0.2.0. **Glyphs:** a fifth `glyph_style`, Steam, from a real Valve device or `SteamDeck=1` (`glyphs.cpp`; Steam Input's type still not read, U16). **Device rules:** c4652165 - a stick takes the device back only by moving, so a worn stick no longer takes it from the mouse (U15), and the switch ends a pad snap and a virtual button. **Holds:** `holds_as_toggles` stays global, and a latch now follows the game (dd235cb5, U34). Still open: the Smart-Steering tick (U20), the named pipe (U22), the Bindings screen (U33; U60 proposes a read-only card), the real pad.

*Research round 29 Oct 2026 ~22:30 (HEAD 04b4c05; the owner: sizing and spacing that make sense, full-featured but
compact defaults, accessible options with depth, better presets, full rebinding, a visual / flow pass on every element):*
FLvibed's default pad layout already matches the industry core (Findings, "Presets, rebinding, HUD rules"); the gaps are
**customisation** and **checking**. (1) No Bindings screen - CUSTOM equals FREELANCER (`presets.h`); its must-haves are
U33. (2) Hold / toggle is one global switch (`holds_as_toggles`) and the tap / hold / double timings are constants
(`kHoldSec` / `kDblSec`, `presets.h` 92); no stick-layout (Southpaw) swap, only `invert_y` (U34). (3) No automatic HUD
layout or contrast check - the measurable rules below become a lint over the draw rectangles (U35, with
`test-automation.md` T8). (4) `text_size` stops at 150 %, `hud_scale` at 130 % (`ui_meta.h`); XAG's 200 % and the TV
text floor are out of reach, and the 0.024 H floor should be re-measured as glyph body height (U36). (5) No accessibility
presets or first-launch screen; no search over ~140 advanced rows (U37). Correction to the agent's list: "sensitivity
cannot reach +50 %" is not a gap - `steer_scale`'s default 0.77 is the ship's measured maximum turn rate, so a sensitivity
option works through the response curve (`steer_expo`), not a higher scale.

*Check-up 30 Oct 2026 ~11:50 (short form, HEAD 16df8bd; 2 watched-path commits aafe41d..16df8bd):* 1893428 (U30's
skeleton): the `rumble` help now names everything that vibrates - hits by their weight, the shield failing, the
autopilot's hand-back pulse, the game's boxes waiting for an answer - so the 01:50 line's "its one sender" is overtaken
(marked there). The hand-back pulse feels the same but plays only with the pad as the device in use and only when the
autopilot ends in flight: U15's device switching (the last valid input picks the device) applied to rumble, and never in
cutscenes or menus. d488fbd counts game loads in `controller.cpp`'s frame (the logic's mission memory starts over) - no
UI rule. No row changes.
*Check-up 30 Oct 2026 ~07:25 (short form, HEAD aafe41d; 3 watched-path commits 4ca36df..aafe41d):* with the opt-in
`pad_backend` 1 the glyph family comes from SDL's gamepad type (baae629, `pad_backend.h` VendorForType ->
`glyphs::Update`), and the SDL test pads show it per type - xbox / playstation / nintendo, a two-pad hand-over switching
the family (8e19482). No HUD or menu rule here changed; the setting is an advanced developer key, its Feature Tour line
deferred to the slice that ships it to players (WORK_QUEUE Q5).
*Check-up 30 Oct 2026 ~01:50 (short form, HEAD 4ca36df; 3 watched-path commits 04b4c05..4ca36df):* the device rules
met new ground, none is contradicted. 08a99a7: one focus gate for the physical pad in flight and menus (U38; the
industry-standard "a background game ignores the pad"; a press held across the focus return stays ignored until
released - the press-ownership rule of 0eca73d applied to focus; `focus_only` the opt-out); the `rumble` help now names
its one sender (U30) *(overtaken 30 Oct 2026 by 1893428: it names every effect of the haptics table - check-up ~11:50)*. e38ca61: the glyph family follows the pad's vendor id through `glyphs::Update` (proven on a test
pad, sweep `pad-device`: a PlayStation vendor id switched the glyphs xbox -> playstation), the input to U16's per-pad detection; still one active pad slot (first that
answers). b376a21: the LAN / INTERNET prompt now holds the D-pad focus (it walked onto the main menu behind it) - U19's
dense-screen rules hold on that prompt. U16 / U19 / U20 / U22 statuses unchanged.

*Check-up 29 Oct 2026 ~07:15 (short form, HEAD 067bf32; 2 watched-path commits 7518008..067bf32):* no effect. 373634a
brings `UI_PLAN.md` section 2 and `AUTOPILOT_UX.md` in line with what U17's line below already records (the toggle is
the View hold, the LB + L3 chord gone since 28 Oct); 6970588 makes the options page's own pad reader apply the player's
`stick_deadzone` (it kept the default) - one setting now means the same on every screen, as the device rules want.
*Check-up 29 Oct 2026 (whole set, HEAD 7518008; 20 watched-path commits 77d8655..7518008):* three of this note's rules
met new ground, none is contradicted. (1) **Assist presentation (U20):** the turret view now runs under the autopilot
(ef56c49 / 4857584: X held = the turret camera; the right stick turns the camera and does **not** take the ship back) -
a deliberate exception to "hand-back by the player's own flight input", limited to a look control; the hand-back rule
stands for the flight inputs. In multiplayer a start the server refuses shows nothing on screen yet
(`docs\MULTIPLAYER.md` "Decisions for the owner" 3): the one-state-line rule says where it belongs (IDEAS U28). (2)
**One deliberate autopilot control:** `autopilot_pad` and its L3 + R3 / hold-Menu alternatives are gone (393bea9) - the
pad starts the autopilot only by the half-second View hold this note recommended (U17). (3) **Dense screens (U19):** the
multiplayer server and character lists take the pad (29bbada: D-pad picks, A connects / loads, Y new character) and the
main menu, the MULTIPLAYER button's LAN / INTERNET prompt and the multiplayer death dialog press the game's own buttons
directly, also out of focus (c69dad6; *30 Oct 2026, 08a99a7: the physical pad is ignored with the game in the
background, menus and flight alike - "out of focus" holds for the harness's virtual pad only*). HUD 2's owner rounds 2-6 (TARGET AHEAD inside the ring, the LB hints, the cruise
/ tractor spots; 1975cb5..5d52919) are layout inside HUD 2 (U27). Still open from this note: the Smart-Steering tick
(U20), Steam Input's type (U16), the named-pipe LiveSplit fallback (U22), the Bindings screen; the handoff's "one pad
slot only" (`gamepad.cpp`) bears on U16's per-pad detection.
*Check-up 26 Oct 2026 (step 3 short form, HEAD e5c02cc; 12 watched-path commits 855745b..e5c02cc):* the pad cursor and
the dense-screen rules (U18 / U19) now reach FLUF's ImGui windows and the game's tutorial / mission popups (0e41948: the
stick moves the game's cursor FLUF reads, A / B as mouse button / close); a press belongs to the consumer that had the pad
when it went down (`core\press_gate.h`, 0eca73d - the B that closes a window never reaches flight); the dealer's B is held
through the NeuroNet window's slide (f52815b); two new options rows (`fluf_windows` af2adb1, `borderless` 90a8b7f). All
extend built ideas; nothing here is contradicted. Not verified: the tutorial boxes on the pad with the owner (attended).
**Extended HUD 2 and the fix tracks (check-up 25 Oct 2026, HEAD 855745b; `modules\hud\docs\UI_HUD2_EXTENDED.md` 10-13,
`UI_HUD2.md` 5.10-5.11):** HUD 2 was declared complete (057b2d8; FIX2 landed: lit D-pad slots, NEURONET pinned, the
thinner hit flash, wrapped and counted weapon rows 5ea171f) and its menus / rooms / dialogs followed the same day:
`ui_style` (CLASSIC default / HUD 2) skins the game's own windows (uiskin, found on the host's UI generation counter,
d1d5bf4 / 32b091c), a docked context strip and character card (84492e9; the bar's character strip removed by the owner,
becc738 - D-pad walks the room's characters, Trent the default focus), modal menus over a hidden flight HUD (1d76bf9), and
**the prompt pills in one bottom lane on every screen** (7910aaf, through the HUD bus v2, 014f39a) - a departure from
U19's "bottom right, at most five": one right-aligned row at the bottom on every screen, a top-lane fallback over the
dealers. More pad screens of their own (repair, docked nav map, the confirm dialog: PADFIX 1-3), and the game's arrow
hides on the menu screens too (U18, 73993e4). Glyphs: **`glyph_look`** (c5d0191: AUTOMATIC / COLOURED / HUD 2 GLYPHS,
the owner's mix of both kits) beside `glyph_style` (U16). Every FLvibed text through one fit rule and a 0.024 H / 14 px
floor (4e9e1f6, U26), HUD 2's scale following the aspect (389e37d), hover help only on an intentional mouse hover
(4a1a380). Performance: the pad's per-frame UI probe was 2.5-7 ms and is now cached (bc886f3) - the "cheap at run time"
rule. HUD 2 runs on vanilla 1.1 too (65cd6dd, 48c712a, c6158a4). Still open: the news dot's data (U7), the Smart-Steering
tick (U20), Steam Input's type (U16), the named-pipe LiveSplit fallback (U22), the Bindings screen, and the owner's
Elite-pad pass.
**HUD 2 built (check-up 25 Oct 2026, HEAD 073a665; `modules\hud\docs\UI_HUD2.md` 4-5):** the owner gave the go (~11:00) and HUD 2 was
built the same day - the HUD bus, the readers (`core\game\hudinfo.*`), HUD 2 (`hud2*.cpp`, `hudvis.*`), the subtitles on
the bus, keyboard-and-mouse clicks, polish, integration (5.8: every coverage row passed, the display matrix in the release
build, 0.13-0.18 ms a frame at 4K in a fight) and a fix round after an in-game critique (5.9); `hud_style` 1 (HUD 2 on the
pad) is the default *[since 0.2.0: `hud_style` 2, HUD 2 always, `docs\SETTINGS.md`]*, CLASSIC (the U23 badges and legend) stays at 0, HUD 2 for keyboard and mouse at 2 (IDEAS U27 built;
U24 / U25 superseded with their D-pad cross, loadout strip and NeuroNet chip carried into HUD 2; U26's 0.024 H floor
built). **Where the build departed from round 3:** the game's HUD is not hidden by HDE's HUDless switch (the paragraph
below) but per root control by the state byte +0x6C (found once, cached, re-hidden only where the game re-shows them -
the owner's "no costly re-hiding"); the damage arcs were hidden too (they competed with the ring); the brackets' own
label / bars and the off-screen arrows stay native (a hook in the indicators' draw was judged not robust yet); the
NeuroNet news dot is drawn without data (the flash state not found: IDEAS U7). Open: 5.9's open list, the Elite pad's
pass; the round-4 critique is done and its fix round (FIX2) was landing during this check (72a6d4e..12bc233, `hud2*`
only, read: nothing here changes). Coexistence with FLSharp 0.9.2 / HUDless: no clash on paper, four
second-install sweep items (`flsharp.md`, IDEAS U4).
**Pad HUD round 3, HUD 2 (25 Oct 2026 ~09:00-10:30; the owner: "a smart balance between B and C ... be daring ...
from the ground up ... Freelancer flavor ... fully functional"):** a ground-up pad HUD drawn by FLvibed with the
game's HUD hidden whole - design brief `modules\hud\docs\UI_HUD2.md` (U27, proposed). Research added: whole-HUD layouts of
modern space games measured on screenshots (`hud_space_visual.md`: Starfield's third-person ring with arcs at the
reticle, Everspace 2's world target tag and corner bars, Ace Combat 7's one-colour line HUD, Elite's one-hue rule,
Halo's light-blue glow palette, Armored Core VI's "thin meters" complaints, the York ammo-near-reticle study), the code
map of what FLvibed reads for every HUD value (`hud_data_map.md`: nearly all readable; gaps afterburner / engine
kill / active weapon group / display names / faction text outside the subtitles / the native wireframe views), and two
independent design critiques of the mock-ups (`hud2_critiques.md`) that drove iterations 2 and 3; proposal page artifact `D6XnaBvWLKHARfdvTQuj3Y`.
Freelancer assets for it: HDE's flat icon sheets and targeting element textures (`buttontextures_flaticons.txm`,
extracted to `hud_redesign\fl_assets\`, never committed). The game's HUD hides whole by HDE's HUDless switch (bytes
0x679C0C / 0x679C10 / 0x679C40, the target view at 0x4E3CE0, the comm / chat sites; poked by debug for the captures,
not yet signed or verified for what each piece hides). *[Departed in the build (073a665): HUD 2 does not use HUDless's
bytes; it hides the panel roots by their state byte (`hudvis.*`), and the HUDless bytes stay unsigned.]*
**Pad HUD round 2 (25 Oct 2026 ~08:00, HEAD 815c9c1; the owner: the built badges are "tiny controller glyphs on top of
the K&M HUD" that do not look natural, the legend aside; proposal page artifact `Mh1UW7KC8QtSEGFNKa23h6`):** both new
research passes (`hud_conventions_aaa.md`, `hud_space_games.md`) say the built U23 badge layer is the anti-pattern -
none of ten space / flight games stamps a glyph on every HUD control; the well-received PC-first pad ports change the
interaction instead (BG3, Civ VII, FFXIV); the badges' 0.022 H with ~14 px letters at 1080p is XAG 101's "don't".
What works: **pad-native clusters whose shape is the mapping** (Everspace 2 swaps its keyboard slot row for two
D-pad-shaped crosses with one glyph per cluster; Elden Ring; FFXIV), **contextual prompts at line height** (Squadrons
above the reticle, Fenyx at the object), **glyphs in the HUD's own colour** (Everspace 2, Starfield, Squadrons; Steam's
NeutralColorABXY), **>= 26 px at 1080p** (~0.024 H; the proposals use 0.040 H glyphs, 0.031 H labels). Three directions
were rendered on 4K frames (`tools\uimock\hudmock*.py`, `research\evidence\controller_ui\hud_redesign\`): A clean (no
badges, everything in the legend - overflows in combat), **B pad clusters (recommended, U24)**, C pad cockpit (U25).
The owner decides; nothing is built or in the spec yet.
**Built (check-up 25 Oct 2026, HEAD abf84ac; `docs\UI_BUILD.md` 5):** the UI build of 25 Oct 2026 realised every idea of
this note - device switching (U15, 3a1dcec), the glyph families (U16, 442654f; drawn by FLvibed itself, not Kenney), the
FREELANCER / ACE / ASSISTED presets (U17, 442654f), the pad cursor everywhere with magnetism (U18, c476760 / a6f8e2f), the
pad screens, OSK and prompt bar (U19, 13c41f0 / c476760 / d01c755), the status line and hand-back (U20, 442654f), the New
Game prompt with the mode per save (U21, 13c41f0), the LiveSplit client with an in-game split detector (U22, 13c41f0 /
7383675) and the informative pad HUD with badges and legend (U23, 442654f, rebuilt 3318282 after the owner's review).
Verified in the release build across the display matrix on the virtual pad; the real Elite pad has not felt phase D
(`UI_BUILD.md` 5 "Known gaps": no Bindings screen, native row controls are replicas, no goal queue). What the build did
differently from the research is marked in each bullet below; the next check reads the user's Elite-pad pass.
- **Device switching, the Ubisoft way, with the rules nobody publishes written down**: no studio publishes thresholds; the engineering rules that exist are qualitative (Burke's "5 golden rules of input": a timestamp engine, last *valid* input wins, a mouse minimum distance, an analog noise floor; Unreal CommonUI's thrashing window). Ubisoft's own auto-detect is the cautionary tale: prompt and cursor flicker with a drifting DualShock stick, L2 / R2 re-arming a switched-off auto-detect, extra joysticks flickering the menus, L3 popping the Windows touch keyboard. So: pad = any button, D-pad, trigger > 0.5 or a stick past ~0.25 (above XInput's default 0.24 dead zone); mouse = ~8-10 px at 1080p in one frame or a wheel notch; keyboard = any key; a 300-500 ms window in which the other device's axis-only signals are ignored (buttons switch at once); the HUD swaps in one frame; a sticky **Input device: Automatic / Controller / Keyboard and mouse** setting; a pad unplugged in flight pauses. (U15)
  *[Built 3a1dcec (25 Oct 2026): `device.*` - pad buttons switch at once, dead-zoned sticks and triggers past 0.5 after a
  0.4 s axis grace (`kAxisGrace`; the research's 300-500 ms), mouse = physical motion over consecutive frames through the
  host's real-mouse reading (28dd009), keyboard = the raw `WinKey` hook (injected keys never count); `input_mode`
  Automatic / Controller / Keyboard and mouse and `pause_on_disconnect` are registry rows (001d858); the pad HUD layer
  follows the device the same frame.]*
- **Glyph families need the pad's identity, which XInput hides**: detect by HID vendor / product id (SDL's table: Sony 054C, Nintendo 057E, Microsoft 045E, 8BitDo 2DC8), default to Xbox for unknown pads, prefer Steam Input's type when the game runs under Steam; the setting is **Button prompts: Automatic / Xbox / PlayStation / Nintendo** plus **Confirm with: bottom button / right button (Nintendo)** - confirm stays positional (south) by default, which Sakurai calls near-universal now and Switch the one exception. Art: Kenney Input Prompts 1.5a (CC0) covers all three families, Switch 2 and Steam Deck in one style, with fonts; nobody ships Elite paddles (draw them). (U16)
  *[Built 442654f (25 Oct 2026), **differently on the art**: `glyphs.*` draws the Xbox / PlayStation / Nintendo glyphs
  itself (GDI shapes and letters, 4x supersampled into one atlas; no art file, sharp at any size) - Kenney's CC0 set
  was not adopted; `UI_SPEC.json` keeps it as "a possible art swap". Detection as proposed: RawInput HID vendor ids
  (Sony 054C, Nintendo 057E, else Xbox); Steam Input's type is not read. Settings `glyph_style` (Automatic / Xbox /
  PlayStation / Nintendo) and `confirm_button` (001d858).]*
- **The default flight preset borrows the strongest cross-genre reflexes** (round 2 added 13 flight games and the action / mech / racing norms): fire on RT; afterburner on LT (a trigger is the best hold: Chorus, Halo, Destiny boost there; Everspace players dislike stick-click holds); lock-on on R3 (Elden Ring, Armored Core VI, BotW); consumables one tap each on the D-pad (works in Freelancer because the throttle lever persists); A = context, B = cancel / brake (held = reverse, Rebel Galaxy Outlaw's throttle-down); rare actions in one hold-wheel picked with the D-pad (analog selection in a hold-wheel misfires: the Everspace 2 complaint); the autopilot on a half-second hold of View with a ring (deliberate, one-handed, no double-tap: the Star Citizen anti-pattern). Freelancer-specific material is thin: not on Steam (no shared layouts), the one forum trace is the cursor-recentring problem. (U17)
  *[Built 442654f (25 Oct 2026): `presets.*` - FREELANCER (the owner's 25 Oct binds: RT fire, LT afterburner, R3 tap
  smart target / hold rear, LB tap picker / hold wheel, X tap tractor / hold turret, Y cruise *[changed 4-5 Nov 2026: Y tap next group, Y held cruise, 91aee7bb; A one item, X all, 6365c9ac]*, D-pad consumables, View
  hold autopilot) plus ACE and ASSISTED with tap / hold / double; every FREELANCER binding verified by the key trace;
  `pad_preset` with CUSTOM on the options page; the module's private map "Controls summary" rewritten and BINDINGS.md marked
  superseded by the spec (e0d2963). Not built: the Bindings screen (`edit_bindings` shows a note; `UI_BUILD.md` 5).]*
- **Pad cursor as an option, focus by default**: Destiny's cursor slows over items and pulls toward them, Destiny 2 aborts holds only when the cursor leaves the item; players ask for snap-to on the D-pad (Grim Dawn) and for the virtual cursor back where it was removed (MSFS). So: focus navigation everywhere, **Pad cursor: Off / Rooms and maps / Everywhere** (default the middle), ~1200 px/s with a ramp, slow to ~40 % over a target with a gentle pull, D-pad hides it. (U18)
  *[Overtaken by the owner (f1804a6) and built c476760 / a6f8e2f (25 Oct 2026): the pad cursor is **on everywhere by
  default** (`pad_cursor` registry default 2 = EVERYWHERE, 2c9e5d4), coexisting with focus on every screen; the
  magnetism is in `padbases.cpp` (`cursorMagnet`: the cursor slows over the element it is pulled to, never below a third
  of the stick's speed so it can always leave; panels and lists give no pull); the game's own arrow is hidden while the
  pad is the device (HostApi v20 `SetCursorHidden`: the cursor-shown flag 0x6107DC cleared around `CursorUpdate`,
  28dd009). Verified with the virtual pad only.]*
- **Dense screens follow Starfield's slot set** (the best citable baseline): LB / RB tabs, LT / RT zoom or page, X / Y contextual, View = map, R3 = reset view; the nav map as NMS does it (one stick moves the cursor, hold to commit); trade screens with the quantity on the triggers, buy / sell on A, all on Y; an own on-screen keyboard so Windows' never appears. Prompt bar bottom right, confirm first, at most five glyphs, hidden when a modal has its own; destructive actions on a ~1 s hold with a sweep. (U19)
  *[Built 25 Oct 2026: the menu screens, the own on-screen keyboard (`osk.*`) and the New Game prompt (13c41f0,
  D-MENUS); bases, dealers, NeuroNet windows and the nav map on the pad (`padbases.*`, c476760, D-BASES; the dealers'
  ToggleButtonControl mount boxes through a signed native click path, a6f8e2f); the flight modals (f1e0ed9); one shared
  prompt bar (`promptbar.*`: bottom right, confirm first, at most five, HOLD marked, `prompt_bar` CONTEXTUAL / ALWAYS /
  OFF; the flight modals' own bar folded onto it, d01c755). Verified with the virtual pad at 4K and 720p; the Elite pad's
  pass is pending.]*
- **Assist presentation**: one state line in a fixed HUD spot (Elite "TO DISENGAGE", X4 "Autopilot engaged", Starfield "Grav Jump Pending"), engaged and left by the same control, hand-back by the player's own flight input with **no re-arm on release** (Elite's throttle rule), interruption by the world (Rebel Galaxy, FreeSpace); Mario Kart's Smart Steering is the model for "AI steers, player aims": a tick that lights only while the assist corrects, toggled at the start and in the pause menu. (U20)
  *[Built 442654f (25 Oct 2026): `padhud.*` draws the one autopilot status line under the maneuver bar
  (`ap_status_line`) and lists HAND BACK on the stick (B does it too) in the legend; the goal panel (f1e0ed9) is the
  modal form. Not found in the code: the Smart-Steering tick on the reticle while the assist corrects - open, small.]*
- **Speedrun mode at New Game is a known pattern** (Celeste's speedrun clock, Hollow Knight's Steel Soul and the "one life" entries beside the default; Neon White's always-on timer praised): a short list with the default highlighted and one description line, the choice tagged on the save slot and **locked per save** (Mario Kart's "Smart Steering OK" flag) so recordings stay comparable. (U21)
  *[Built 13c41f0 (25 Oct 2026): `newgame.*` - A on NEW GAME in pad mode opens NORMAL GAME / ASSISTED / SPEEDRUN MODE
  with one description line; the mode is written beside every save the game writes (a sidecar record with the speedrun
  clock and checkpoint index), a loaded save's mode becomes the session's, the load screens tag ASSISTED / SPEEDRUN over
  the row; with keyboard and mouse the game's own New Game runs (NORMAL).]*
- **LiveSplit integration is cheap and well defined**: LiveSplit Server is built in since 1.8.29 (TCP 16834, plain text, `\n`-terminated: `starttimer`, `split`, `reset`, `pausegametime` / `unpausegametime`, `setgametime`, `switchto gametime`; WebSocket since 1.8.30; the named pipe `\\.\pipe\LiveSplit` always open). speedrun.com Freelancer: real time, video required, "timing starts on pressing New Game and ends when the final cutscene starts after destroying the last generator"; no autosplitter or load remover exists. So the mod sends `starttimer` on the New Game click, `split` per story checkpoint and at the final cutscene, `reset` on the main menu, pauses game time during loads, and exposes a stable memory struct for community ASL scripts; it never calls its own timer official. (U22)
  *[Built 13c41f0 + 7383675 (25 Oct 2026): `livesplit.*` - TCP 127.0.0.1:16834 with `starttimer` on the New Game click,
  `split` per story checkpoint (an in-game split detector over `checkpoints_gen.h`, generated from `docs\checkpoints.tsv`
  and mirroring `tools\splits.py`), `reset` on the main menu, `pausegametime` / `unpausegametime` around loads; the
  exported `FLvibedSplitsV1` struct (magic FLVSPLIT, `docs\livesplit\README.md`) for autosplitters; the HUD timer
  (`runtimer.*`, best segments in `EXE\FLvibed\splits_best.ini`); the client connects only while a speedrun session runs
  (7383675). Not built: the named-pipe fallback (TCP only). The timer is game time with loads excluded - not the board's
  real time, as the note demands.]*

## Findings (condensed; the evidence files hold the tables and every source)
### Flight presets, round 2 (`flight_presets.md`)
13 more flight-model-alike games tabulated (Chorus: LT boost, LB drift, L3 cruise, weapons on the D-pad; Rogue Leader: triggers accelerate / brake; Star Fox: double-tap shoulder = roll; the joystick era: Wing Commander's double-tap-and-hold afterburner, both buttons = missile; Everspace 1's dev: strafe "incredibly important", roll least; Darkstar One: never put the throttle on a self-centring axis). Cross-genre norms: R3 lock-on, B dodge / cancel, D-pad consumables (tap use, hold wheel), one weapon per trigger / bumper (AC6, MechWarrior 5), LT boost in Halo vehicles / Destiny sparrow, View = map, Menu = pause. Feedback: praised = one dedicated input per frequent action and the D-pad for 4-way state (Squadrons, AC6); criticised = hold-chords with release ambiguity (Elite), hold-wheels that read the stick (Everspace 2), stick-click holds (ES1 / ES2), tap / double / hold stacking (Star Citizen). The proposal's thin slots to playtest: afterburner LB vs LT vs L3, cruise on L3 vs a face button, engine kill on a hold, D-pad reach while steering, the wheel's selection input.

### Switching, glyphs, menus (`pad_ui_switching_glyphs_menus.md`)
No numeric thresholds anywhere public; Unity switches only on a "non-noisy, non-synthetic" control; Ubisoft's "Input Auto-Detect" toggle and its failure modes. Button-name table for the three families; the Japan / Switch confirm convention and the swap precedents (Switch 10.0 OS remap, Steam Deck's "Nintendo layout"); VID / PID list; Steam Input's glyph API and type enum; Hades auto-detects four families; Kenney 1.5a as the one consistent CC0 set. Pad cursors: Destiny's GDC talk (slow, pop, gravity), the forced-cursor friction in ~14 % of 235 titles, D4's grid complaints. Maps and trade on a pad: Starfield's full table, NMS one-stick map, Elite's galaxy-map search still needing a keyboard; Daisywheel retired. Prompt bars and grammar consolidated from Starfield / Valhalla / NMS / Elite; GAG basics for size and contrast; hold-to-confirm 1-2 s with a sweep.

### Assists, speedrun modes, LiveSplit (`assist_speedrun_livesplit.md`)
State lines and lamps across Elite, X4, Star Citizen, Starfield, Wing Commander; Elite's docking computer rule (any throttle input disengages); FreeSpace's drop-out rules; MSFS's silent preset overwrite as the anti-pattern; Mario Kart Smart Steering; Celeste / Neon White / Dead Cells / Metroid Dread / Steel Soul for speedrun and timer presentation; the LiveSplit Server command set and versions; speedrun.com Freelancer's rules; new-game prompt conventions (default pre-selected, plain names, one line each, disclose gating, tag the slot, lock when it changes leaderboard meaning).

### Pad HUD round 2 (`hud_conventions_aaa.md`, `hud_space_games.md`; 25 Oct 2026 ~08:00)
Platform numbers: XAG 101 console text >= 26 px at 1080p / 52 at 4K, the letter inside a glyph too, glyph ~ line
height, scalable to 200 %; XAG 102 contrast 4.5:1 (3:1 large); title-safe 90 %; Steam Deck: no K&M glyphs while the pad
is active, glyphs >= 9 px at 800p; Steam Input's NeutralColorABXY. Patterns: slots with the glyph built in (Diablo IV's
corner bar for couch distance, Ragnarok's one group and one message queue); spatial isomorphism (Elden Ring's cross,
FFXIV's cross hotbar); radials (BG3, RGO with slow-mo, X4 7.0, Star Citizen); world prompts (Fenyx, Squadrons above the
reticle); legend strips belong to menus and overlays. Space games: Everspace 2 (two D-pad crosses of four tiles, one
monochrome glyph per cluster ~2.4 % SH, the PC keyboard HUD a row with keycaps - the layout swaps with the device),
Squadrons (no glyphs on the instruments, the button map on a Loadout overlay), Starfield (HUD-cyan key chips on the
reticle ring's labels), Ace Combat 7 (no glyphs; a text weapon list with ">"; the GUI lead: one meaning per line and
colour, few boxes), Elite (hold a modifier + the D-pad direction where the panel sits), X4 (the mouse HUD with an
emulated cursor - the closest analogue to the built state, called clunky). Anti-patterns: badges on mouse-sized icons,
glyph salad (the built combat legend repeats six badges), glow failing contrast (Halo Infinite), small flight icons
(Chorus).

### Presets, rebinding, HUD rules (29 Oct 2026 ~22:30; a research agent's web round, not saved as evidence; confidence H / M / L)
**Pad schemes.** Squadrons (H): remapping in three contexts (menus, hangar / briefing, flight); each wheel Hold or Toggle,
drift Hold or Double-tap, throttle Continuous or Steps with centre friction; deadzone and sensitivity per axis; invert
separately for flight and look (https://ea.com/able/resources/star-wars/star-wars-squadrons/xbox-one/controller-settings).
Everspace 2 (M): Scheme A (shooter) / B (racing, Everspace 1), each right- and left-handed; D-pad tap toggle, hold wheel
(https://everspace.fandom.com/wiki/Controls_(ES2)). Ace Combat 7 (H): Standard (LS X yaws) vs Expert (LS X rolls); Type
A / B swaps throttle and yaw between triggers and bumpers; beginners start on Standard
(https://shacknews.com/article/109486/ace-combat-7-controls-playstation-4-and-xbox-one). X4 7.00 (H): customisable radial
menus, on X / Y in the default profile, custom profiles untouched (https://www.egosoft.com/news/archive/2024May_en.php).
Elite Dangerous (M): bindings per vehicle context; any edit silently becomes Custom.3.0.binds, bindings lost when a
device id changes; the galaxy map turns analog inputs digital - lesson: the map is its own context and a profile never
vanishes. Halo Infinite (H; the best preset pattern): button layouts chosen **separately** from stick layouts (Default,
Legacy, Southpaw, Legacy Southpaw); any edit relabels CUSTOM
(https://support.halowaypoint.com/hc/en-us/articles/4407649252116-Guide-to-Halo-Infinite-Game-Settings). Rebel Galaxy
Outlaw (L): a hidden context key also drove its autopilot, so remapping broke it - every action named and visible. No
reliable pad defaults found for No Man's Sky, Star Citizen, Chorus, FreeSpace Open (preset files), Starfield (M, sources
disagree). Industry core: RT primary fire; RS aim (dogfighters) or LS steer (arcade); LS click boost / drift; D-pad quick
slots; hold = wheel; A context / accept; B back; View map; Menu pause; the stick swap a separate axis - FREELANCER fits.
**Presets**: keep the three; add two independent axes - stick layout (Default / Southpaw) and hold-vs-toggle per action;
an ACE "Type B" (AC7's trigger-throttle / bumper swap) and an ASSISTED left-hand mirror; no fourth preset (a trigger-thrust
layout fights Freelancer's persistent throttle; playtest first).
**Rebinding must-haves** (XAG 107: https://devdocs.xbox.com/build/game-principles/accessibility/xag-deep-dives/xag-107-input.md):
any action on any button incl. the stick clicks and D-pad slots, prompts / diagrams following the mapping (FLvibed's
prompts read the tables already); contexts Flight, FlightAp, Menus, Bases, Dealers, Nav map, Text entry; tap / hold / double
per button with Steam Input's activator rules (an interruptible tap held back while a hold or double can still fire:
https://partner.steamgames.com/doc/features/steam_controller/activators - the dispatcher does this); per-action
Hold / Toggle; the timings as settings; on capture SWAP / REPLACE / CANCEL, never a silent double bind or an "already
bound" dead end, a warning for an unbound essential (fire, accept / back, pause, map), Menu and B never unbindable; a live
pad diagram with changed binds marked by a dot, not only colour; reset per action / context / all; named custom slots
(The Last of Us Part II has three), export / import as a text block; a profile survives a device-id change; any edit flips
the preset to CUSTOM (built).
**Options**: presets -> player rows -> Show advanced (built); accessibility presets Vision / Hearing / Motion / Motor, each
Some / Full (God of War Ragnarök: https://www.playstation.com/en-us/games/god-of-war-ragnarok/accessibility/); a short
first-launch screen of grouped shortcuts (Far Cry 6: https://caniplaythat.com/2021/10/06/far-cry-6-accessibility-menu-deep-dive/)
- the New Game prompt could carry it; XAG 112: accessibility reachable from launch, LB / RB tabs, LT / RT pages, B back,
lists wrap (with an off switch), grids do not, search or filters for long content
(https://devdocs.xbox.com/build/game-principles/accessibility/xag-deep-dives/xag-112-ui-navigation.md).
**HUD rules an automatic check can enforce** (H = screen height; XAG 101 / 102:
https://devdocs.xbox.com/build/game-principles/accessibility/xag-deep-dives/xag-101-text-display.md, xag-102-contrast.md;
https://devdocs.xbox.com/build/core-features/graphics/overviews/screen-areas):
1. every HUD rectangle inside the inner 90 % (title-safe); only clip-tolerant art in the 90-95 % band;
2. ultrawide: edge elements anchor to a centred 16:9 rectangle (or a user width), never stretched; sizes in H units;
3. text measured as **body height** (descender to ascender, not the font size): >= 18 px at 1080p (0.0167 H) on PC,
   >= 26 px (0.024 H) at TV distance;
4. scalable to 200 % of the minimum without losing content, glyphs with the text, scrolling in one direction at most;
5. contrast 4.5:1 for text and symbols, 3:1 for large text (>= ~0.033 H), 7:1 in a high-contrast mode, against the
   **worst** background behind the text over several frames; outline pixels count only above 2:1; a backplate option;
6. flashes < 3 Hz, every cue with an off switch (the owner's rule, WCAG 2.3.1);
7. a glyph ~0.5 x its element's height and never smaller than the text beside it;
8. no overlap between HUD boxes, a minimum gap (proposed 0.01 H), nothing inside the reticle ring but ring parts, boxed
   area <= ~10 % of the screen (HUD 2's own budget);
9. text blocks <= 80 characters a line, line spacing 1.5x;
10. per-axis sensitivity and invert per stick and axis.
Overlap detection from screenshots is hard in games (https://arxiv.org/pdf/2504.20261); check the element boxes from the
HUD bus / draw data per screen across the display matrix (PASS / WARN / FAIL per screen, as Unreal's "UI Resolution Guard"
does), screenshot baselines for visual drift.

