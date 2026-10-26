# Controller UI: presets, device switching, glyphs, pad menus, assists, speedrun mode and LiveSplit
Status: current (the UI build of 25 Oct 2026 realised U15-U23; where the build departed from the research the bullet says so in place)
Checked against: FLvibed 77d8655 (26 Oct 2026, coordinator short form before the export: the watched commits since - d89f88c / 79e2bcc smart start + K&M goal panel, 7632ced job objective text, 77d8655 objective-named loot shot, 9ff7fe7 handoff - are autopilot / UI internals; no statement of this note changes; not re-read in depth); before: FLvibed e5c02cc (26 Oct 2026, step 3 short form: FLUF windows / popups on the pad, press ownership, the dealer close, the fluf_windows / borderless rows - first "What it means" line); before: FLvibed 855745b (25 Oct 2026 ~23:05, whole-set check-up: 11 watched-path commits 073a665..855745b - Extended HUD 2 (ui_style, SKIN, MODAL, the spec rows), glyph_look, the pad probe perf fix, the unified spec page, HUD 2's objective after a load; read with UI_HUD2_EXTENDED.md 10-13 - new first paragraph); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: 3 watched-path commits 3ebd169..073a665 - d76299c (the HUD 2 rows in `UI_SPEC.json` through the living spec), 2541e58 / f5f07fe (`controller.cpp`: the HUD rows, the integration); the build read in `docs\UI_HUD2.md` 5.8 / 5.9, `hudvis.*`, `hud2*`, `uibtn.cpp`: U27 built, U24 / U25 superseded, U26 built in HUD 2, U23 CLASSIC only - first paragraph); before: 3ebd169 (25 Oct 2026 ~10:30, the pad HUD round 3: HUD 2, four new evidence files); before: 815c9c1 (round 2), abf84ac (25 Oct 2026 ~07:00, short form after the UI build: 24 watched-path commits ec018b6..abf84ac - the package's owner rounds 6469b0a..3cfa88c, the HUD track 3a1dcec / 442654f / e0d2963, the HOST / PAGE / D tracks, the integration a6f8e2f, VERIFY 2f98907, FIX e68bbb2..f1daccc, GLYPHS 3318282, the spec patch 2c9e5d4, the close e2ee6df; every U15-U23 claim read in the code: `controller\src\impl\device.*`, `glyphs.*`, `presets.*`, `padhud.*`, `padbases.*`, `promptbar.*`, `osk.*`, `newgame.*`, `livesplit.*`); before: ec018b6 (25 Oct 2026 ~02:30, the note's creation)
Dirty paths at check: none at the start (25 Oct 2026 ~23:05; during the round another session's uncommitted `controller/src/impl/padbases.cpp`, `controller/src/impl/promptbar.cpp`, `docs/UI_HUD2_EXTENDED.md` appeared - not judged); none (25 Oct 2026 ~12:15; also at 3ebd169)
Watch: docs/UI_PLAN.md, docs/UI_SPEC.json, controller/docs/BINDINGS.md, controller/docs/TASK_GLYPHS_HUD.md, controller/docs/AUTOPILOT_UX.md, controller/src/impl/controller.cpp, controller/src/impl/gamepad.*, core/game/render.*
Evidence: `research\evidence\controller_ui\` - `hud_conventions_aaa.md` and `hud_space_games.md` (round 2, 25 Oct 2026 ~08:00, every claim with its URL; `hud_redesign\` the frames, renders and the proposal page); the three research files of 25 Oct 2026 in full (`flight_presets.md` 73 sources, `pad_ui_switching_glyphs_menus.md` 68 sources, `assist_speedrun_livesplit.md` 59 sources; every claim cited, forum opinions marked anecdotal) and the interactive mock-up page (`mockup_v2_index.html`; live at artifact `4v4dXbB5GTVt1URowFtSvN`); the game screens captured for it in `research\experience\ui_2026-10-25\screens\`
Sources: see the evidence files (web pages read 25 Oct 2026; nothing downloaded). Earlier rounds this builds on: `controller\docs\BINDINGS.md` (12 games' pad layouts, 18 Oct) and `controller\docs\research\ADAPTIVE_UI_RESEARCH.md` (device switching, pad menus, accessibility, 17 Oct)

## What it means for FLvibed (the design is in `docs\UI_PLAN.md` section 6; the owner decides in the mock-up)

*Check-up 26 Oct 2026 (step 3 short form, HEAD e5c02cc; 12 watched-path commits 855745b..e5c02cc):* the pad cursor and
the dense-screen rules (U18 / U19) now reach FLUF's ImGui windows and the game's tutorial / mission popups (0e41948: the
stick moves the game's cursor FLUF reads, A / B as mouse button / close); a press belongs to the consumer that had the pad
when it went down (`core\press_gate.h`, 0eca73d - the B that closes a window never reaches flight); the dealer's B is held
through the NeuroNet window's slide (f52815b); two new options rows (`fluf_windows` af2adb1, `borderless` 90a8b7f). All
extend built ideas; nothing here is contradicted. Not verified: the tutorial boxes on the pad with the owner (attended).
**Extended HUD 2 and the fix tracks (check-up 25 Oct 2026, HEAD 855745b; `docs\UI_HUD2_EXTENDED.md` 10-13,
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
**HUD 2 built (check-up 25 Oct 2026, HEAD 073a665; `docs\UI_HUD2.md` 4-5):** the owner gave the go (~11:00) and HUD 2 was
built the same day - the HUD bus, the readers (`core\game\hudinfo.*`), HUD 2 (`hud2*.cpp`, `hudvis.*`), the subtitles on
the bus, keyboard-and-mouse clicks, polish, integration (5.8: every coverage row passed, the display matrix in the release
build, 0.13-0.18 ms a frame at 4K in a fight) and a fix round after an in-game critique (5.9); `hud_style` 1 (HUD 2 on the
pad) is the default, CLASSIC (the U23 badges and legend) stays at 0, HUD 2 for keyboard and mouse at 2 (IDEAS U27 built;
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
game's HUD hidden whole - design brief `docs\UI_HUD2.md` (U27, proposed). Research added: whole-HUD layouts of
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
  smart target / hold rear, LB tap picker / hold wheel, X tap tractor / hold turret, Y cruise, D-pad consumables, View
  hold autopilot) plus ACE and ASSISTED with tap / hold / double; every FREELANCER binding verified by the key trace;
  `pad_preset` with CUSTOM on the options page; `controller\CLAUDE.md` "Controls summary" rewritten and BINDINGS.md marked
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

