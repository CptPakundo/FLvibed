# Extended HUD 2: menus, rooms, dialog boxes (queued task brief, 25 Oct 2026)

Status: **queued** - starts only after HUD 2 (`docs\UI_HUD2.md`) is complete (`HANDOFF.txt` line `=== HUD 2: COMPLETE`),
in fresh context (after a compaction), AFK. Optional and switchable: the owner judges it later and keeps or discards it.

## The owner's request (25 Oct 2026, verbatim in substance)
"Right now HUD 2 only touches on space mode, but it doesn't have to exclusively be only that. You might consider
researching and possibly iterating HUD 2 alternatives for menus, rooms, dialog boxes, etc. It's not super critical, but
it would be a nice to have. If you could implement that and then show me later how it looks like, I'll make a choice
about whether or not to keep it. This extended HUD 2 should be built with a very similar process to what you've shown me
in this session, as I thought it was exceptionally well done and positively iterated upon. This is heavy work, so
consider dividing workloads to agents ... started specifically after compaction ... with fresh context AFTER the current
HUD 2 task is complete." Also binding from the same day: keep things light and efficient; support the whole game and all
FLvibed features; never get in the way of the autopilot work and the project's workflows.

## The process (as HUD 2 was made)
1. Read: `FLvibed\CLAUDE.md`, `controller\CLAUDE.md`, `docs\UI_HUD2.md`, `docs\UI_BUILD.md`, `docs\UI_PLAN.md`,
   `docs\UI_SPEC.json`, `docs\research\controller_ui.md` (rounds 1-3), `research\evidence\controller_ui\hud_*.md`,
   `hud2_critiques.md`, `tools\uimock\README.md` + `hud2.py` / `hud2_states.py`, `git log --grep "HUD 2"`.
2. Research with agents (web; no audio / video pages, no downloads): modern AAA menus, base / hub rooms, conversation
   screens, vendors / dealers, job boards, inventories, pause / load / save, confirmation dialogs - pad and keyboard-mouse,
   with numbers (XAG sizes); how remasters modernised such screens and kept their identity. Inventory every non-flight
   screen and what FLvibed already does there (`padbases`, `padmenus`, `padmap`, `osk`, `newgame`, the options page, the
   prompt bar; captures in `research\experience\ui_2026-10-25\screens\`; the living mock-up's Screens view, artifact
   4v4dXbB5GTVt1URowFtSvN).
3. Design an extended HUD 2 language for those screens (navy glass, light-blue chrome, Agency FB, the game's own art,
   glyphs in the HUD tint, text >= 0.024 H), replacing or restyling the game's chrome where it helps, always over the
   game's own logic (native actions only, fails closed).
4. Mock it up on clean captures from a BACKGROUND instance (extend `tools\uimock\` like `hud2.py`), every screen state;
   at least two independent critique passes (an agent on the frames vs the research, the principles and the native
   screens), iterating after each; record the critiques.
5. Build with parallel agent tracks on disjoint files (UI_BUILD.md 2 / UI_HUD2.md 5.5), behind ONE new setting that keeps
   today's look as the default until the owner chooses (e.g. `ui_style` CLASSIC / HUD 2), in the spec (specpatch.py
   add_rows, the artifact's `ui/spec` pinned, `docs\UI_SPEC.json`, `core\ui_meta.h`).
6. Verify on background instances only: every screen at 4K, 1280x720, 21:9, 4:3, pad and keyboard-mouse, release build;
   measured cost; every harness path that reads / clicks the game's own controls on these screens
   (`controller\tools\flctl.rooms.ps1`, `flctl.missions.ps1`, the sweep) still works.
7. Show the owner: a page (Artifact) with before / after per screen, what changed and why, the critiques, how to switch
   it on / off; docs, handoff, research notes, commits, `tools\backup.ps1`.

## 8. Research (25 Oct 2026, three agents; evidence git-ignored)
- **Web** (`research\evidence\controller_ui\hud2x_research_web.md`, 15 principles with sources): the remasters players
  liked kept layout and logic and changed the skin (SkyUI, Vanilla UI Plus, Diablo II Resurrected, HDE's own Flat /
  Dark skins), most with an instant global classic toggle (C&C Remastered, StarCraft F5, Halo CEA, AoE III DE);
  Homeworld Remastered was criticised for scattering panels. Freelancer's list text is ~0.013-0.016 H (under the
  0.024 H console floor), price quality is colour only, credits / cargo appear only inside the dealer windows.
- **Inventory** (`hud2x_inventory.md`, `hud2x_trees.md`; 113 captures pad / native in
  `research\experience\runs\hud2x_captures_2026-10-25\`): every non-flight screen, its control tree, what FLvibed
  draws there, the harness-critical names (its section 2). Two families: chrome as named static-image children (menus,
  pause, save / load, confirm, offer, job board, news, the room bar, the NeuroNet box) and chrome drawn by the window
  class itself (dealers, ship dealer, repair, mission log / death, help, player info, the nav map's frame).
- **Feasibility** (`hud2x_feasibility.md`, frames `research\experience\runs\hud2x_probe_2026-10-25\`, instance 2):
  hiding only the art and drawing glass under the UI pass works on every family reached, and every native control
  still clicks by name (rooms, job rows, news rows, dealer rows + the buy panel and slider, ship rows, the save
  screen's buttons). Art children: clear state bit 1 only (`FindVisible` tests bit 0: the harness still sees them);
  self-drawn art: the base draw 0x5A0760 keeps the mesh handle at +0x84, -1 hides just that art. The NavBar's frame and
  tab are re-shown by the game every frame while a window is open over the room (re-clear per frame, as HUD 2 does for
  HUD_Maneuvers). The under-UI draw point exists in every state (the host's UiRender detour, ~2 us). A text control's
  colour is +0x348 (R G B A), kept by the game. Rejected: recolouring the art (no effect; the tint is a shared global);
  swapping a window's vtable (it broke the harness's NavBar lookup).

## 9. Design (draft 1, 25 Oct 2026 ~13:45; the mock-ups and critiques follow)
One setting **`ui_style`** (`[Controller]`, int 0..1, default **0 CLASSIC** = today's screens exactly; 1 = **HUD 2**),
independent of `hud_style` (flight). Both devices. Off = every write restored (the game's own later writes win).
1. **The skin, not the screens** (research 1-2, 15): the game's windows, their positions, rows, buttons, texts and
   flows stay; FLvibed removes only their art and draws HUD 2 glass under them. Nothing native is re-implemented, so
   every harness path and plugin keeps working; an unknown window stays native (fails closed).
2. **Glass** (HUD 2's plate): navy `GLASS` at 85 % over rooms (research: 75-85 % for 4.5:1 over the brightest rooms),
   the light-blue hairline rim (`CHROME` 45 %), chamfers top-left / bottom-right 0.012 H, one plate per native panel
   (the art's extents, taken from the window's own controls at open, as fractions of H: every aspect works). Modal
   windows (pause, save / load, confirm, options) add a full-screen navy scrim (55 %, replacing the game's black
   plane); windows over a room add none (the room stays the identity).
3. **Headers**: the native title text recoloured `CHROME` (+0x348) with a 0.0015 H hairline under it across the plate;
   panel captions (INVENTORY / DEALER / PLAYER SHIP ...) the same.
4. **Focus and selection**: under the focused / selected row a filled bar (active yellow at 14 %) with a 0.004 H
   yellow bar on its left - one focus style everywhere (research 6); the native yellow outline stays.
5. **The room bar**: its frame and tab art hidden; one slim glass bar under the icons (0.088 H), the current room's
   icon on a yellow underline and its NAME under the bar (Agency FB 0.026 H, yellow, e.g. TRADER) - labels beside
   icons (research 10); in pad mode LB / RB glyphs at the bar's ends. The NeuroNet box the same (its background hidden).
6. **The context strip** (docked, any room, hidden under modal menus): top-left at the safe margin on HUD 2's soft
   scrim (no box): the base as the headline (0.034 H) with the system dim; under it CREDITS, CARGO used / capacity,
   the ship's name - one value, one place (research 7). While a transfer is pending in a dealer and the readers allow,
   the after-values follow an arrow ("$500,000 -> $497,520").
7. **The character card** (pad hover in rooms): the game's grey Arial tooltip replaced by a HUD 2 card (faction dim
   small caps over the name in INK, an accent bar in the attitude colour) where the name is readable natively; else
   the native tooltip stays.
8. **The prompt bar** (FLvibed's): HUD 2's pills (dark glass, glyph + verb in Agency FB 0.030 H), same place.
9. **Dealers**: four plates (player list, transfer, dealer list, infocard); the price-quality square gets a word
   (GOOD / FAIR / HIGH, colour + word: research 8) when the game's own deal state is readable, else the square alone.
10. **Left native (decided)**: the main menu (it rebuilds its frames on each appearance with a slide-in: risk for no
    gain), the loading screen (< 1 s in game), the nav map's own drawing (labels, grid), the 3D rooms, every text
    size (the game draws its texts; enlarging them is not a skin change).
11. **Light**: windows are found on events (a counter the host raises when the game adds or removes a control), not by
    polling; art hidden once per window open (the NavBar's two re-clears per frame); plates = a few quads in the
    existing render pass; budget 0.1 ms per frame.
12. **Also fixed on the way** (the inventory's section 3): `NN_Repair` and the docked nav map read as the room by the
    pad layer (prompts over the window); `NN_Confirm` read as the pause menu (B = RESUME); the pad's character hover
    left after a switch to keyboard and mouse; the arrow cursor on pad menu screens.

## 10. Design as iterated (iteration 3, 25 Oct 2026 ~15:40) and the build rules
Mock-ups `tools\uimock\hud2x.py` (layered captures `research\experience\runs\hud2x_layers_2026-10-25\`; renders
`research\evidence\controller_ui\hud_redesign\hud2x\`); critiques `research\evidence\controller_ui\hud2x_critique1.md`
and `hud2x_critique2.md` (round 1: the mock pasted moving room pixels over the glass, prompt pills collided with content,
no single focus style, the strip's contrast, header rules through titles, empty plates, missing decision numbers; round 2:
focus placement by rule, a hierarchy step for added text, the game's own verdict first, aspect rules, one glyph one
meaning, the options tabs' art, scroll bars). What changed from draft 1 (section 9):
- **Prompt lanes**: docked screens put FLvibed's prompt pills top right under the NeuroNet box (right edges equal; the
  windows reach the bottom edge there); modal menus a bottom lane >= 0.015 H under their lowest plate. Pills: glass,
  glyph 0.032 H + verb Agency FB 0.028 H; the verb names its object ("ENTER BAR").
- **Focus**: one style - a fill of active yellow at 28 % on the DRAWN element's full extent (a dealer row = icon cell +
  view; the pause buttons' drawn frame sits ~0.027 H below their hit box) and a 0.005 H yellow bar outside its left
  edge; never on a disabled or current-page button; the native rollover outline suppressed in pad mode.
- **Strip** (overlay): line 1 the base (0.036 H, INK), the room (ACTIVE small caps), the system (CHROME); line 2 CREDITS /
  CARGO / SHIP (labels CHROME 0.024 H, values INK 0.030 H); a pending purchase the game ALLOWS shows yellow deltas
  ("-$66", "+1"); a soft navy scrim (blurred edges, ~65 %). Width budget = the room bar's left edge - the 0.05 W margin -
  0.03 H: what does not fit wraps (room + system onto their own line) or drops (the ship first, then the system).
- **Glass**: plates 85 % with a firmer rim (CHROME 70 %, 0.0013 H); header rules from the title control's extent (none
  where the panel has its own divider); empty plates collapse (an empty player list: header + "NOTHING TO SELL").
- **Modal menus**: 70 % scrim; page tabs (Options) and the save / load side buttons lose their art and get HUD 2 tab
  plates (the current page: yellow caption + underline).
- **Scroll bars**: the native art hidden, a 0.005 H track with the thumb from the bar's own range / position; the control
  stays for clicks.
- **Decision numbers** (overlay, CHROME one step dimmer than the content, 0.024 H floor; only from data the game holds,
  else nothing - fails closed): the price-quality word left of the game's square (GOOD / FAIR / HIGH by the side's own
  sense), the equipment compare line under the dealer list ("VS MOUNTED <item>: DAMAGE +18 % . REFIRE = . RANGE -50 M"),
  the ship dealer's TRADE-IN / NET with the game's own refusal first ("NEEDS LEVEL 1", amber; then no strip delta), the
  job's type + difficulty beside the detail's reward, the offer's giver under its title and its reward line.
- **Glyphs**: LB / RB belong to the room bar on docked screens (the equipment filter moves to Y); tabs in Options.
- **Left native (decided)**: the main menu, the loading screen, the nav map's drawing, the welcome texts (the game
  toggles them itself), every native text size, the confirm dialog's own dimming box (it is what dims the pause menu
  behind it; HUD 2 adds the scrim only).

### 10.1 Build rules (pinned from critique 2)
- All: plates, rules, focus and anchors from the window's native control rects at open (fractions of H only for
  padding); art hidden by state bit 1 (art children) or the base draw's mesh handle +0x84 = -1 (self-drawn art), the
  draw slot checked against the signed base draw first; nothing the game toggles itself in a hide list unless re-cleared
  per frame (the NavBar's frame / tab); restore only bytes still holding our value; text recolour only via +0x348;
  glass, scrim and focus fill in `OnRenderUnderHud`, the strip / pills / card / words in `OnRenderOverlay`; windows found
  on the host's UI generation counter (HostApi v23), never by polling.
- Docked: the room bar's frame / tab re-cleared each frame; the strip within its budget; the pill lane under the
  NeuroNet box.
- Dealers: four plates, empty ones collapsed; focus = the list's selected index; words / deltas only when the game's own
  deal / verdict is readable.
- Lists (job board, news, NeuroNet windows): header + rule from the title rect; a per-row line only from the row's data.
- Modal menus: 70 % scrim; one plate per panel; the bottom lane; the native rollover suppressed in pad mode.
- `ui_style` 0 (CLASSIC, the default) changes nothing anywhere; switching restores every byte at once.

### 10.2 Build tracks (parallel agents, disjoint files; docs\UI_BUILD.md 2 rules)
| Track | Owns | Delivers |
|---|---|---|
| PADFIX | `padbases.*`, `padmenus.*`, `padmap.*` | the five pad-layer bugs (9.12) |
| HOST-X | `core\flexe.h` (block "Extended HUD 2"), `controller\src\host\*`, `controller\src\api.h`, `docs\FREELANCER_RE.md` (its section) | HostApi v23 `UiGeneration`, the signed sites / vtables, the facts |
| SKIN | `controller\src\impl\uiskin.{h,cpp}` (new), `controller.cpp` (the `ui_style` row + the calls), `promptbar.*` (the HUD 2 pills and lanes), `options_page.cpp` (its HUD 2 look under `ui_style`) | windows, art hide / restore, glass, scrim, focus, scroll bars, tabs, recolour, the room bar |
| STRIP | `controller\src\impl\uistrip.{h,cpp}` (new), `core\game\baseinfo.{h,cpp}` (new readers) | the strip, the character card, the decision numbers |
| SPEC (orchestrator) | `tools\uimock\*`, `docs\UI_SPEC.json`, `core\ui_meta.h`, `docs\SETTINGS.md`, this file | the `ui_style` row in the living spec |

## 11. As built (25 Oct 2026, the owner away; the tracks' reports, evidence `research\experience\runs\hud2x_*_2026-10-25\`)
| Track | Commits | What landed |
|---|---|---|
| PADFIX | 7bc4b4b, aaa6532, 219bdb4, 73993e4 | the repair window, the docked nav map and NN_Confirm as pad screens of their own (confirm: focus on NO, B = No); the room hover released and the arrow back on a switch to keyboard and mouse; the arrow hidden on the pad's menu screens (SetCursorHidden) |
| HOST-X | d1d5bf4 | HostApi v23 `UiGeneration` (chained detours on the control list's add 0x59DBF0 / remove 0x59DC00, one increment each; debug `uigen`); flexe.h block "Extended HUD 2" (the window vtables, WindowBaseDraw 0x5A0760); 175 / 175 symbols in 0.7.1 and upstream HDE |
| SKIN | 32b091c, 66ac871 | `controller\src\impl\uiskin.*`: windows on the counter (500 ms fallback on an older host), art hidden by bit 1 (re-cleared each frame) or +0x84 after the draw-slot check, titles recoloured (+0x348), glass / scrim / focus / tab plates / the room bar under the UI pass, exact restore; the `ui_style` row; the prompt pills in their lanes (promptbar); the HUD 2 options page; 35-40 us per frame (0.2 us at ui_style 0); every harness path passed with ui_style 1 |
| MODAL | 1d76bf9 | under ui_style 1 a modal menu in space (pause, save / load, Options, confirm) hides the whole flight HUD (hudvis::SetMenuOver, HUD 2's pass, the pad layer, the cues) until it closes |
| STRIP | 84492e9, 0c313d9 | `controller\src\impl\uistrip.cpp` + `core\game\baseinfo.*`: the strip (base, the room by its own button hint, system; credits, cargo, ship; a purchase's deltas = the game's Total line), the character card from the game's own name-card text (any hover, pad or mouse) with the attitude accent, the price words from the game's own deal verdict (IDS 1047-1052), the equipment compare column, the ship dealer's level refusal + trade-in / net (trade-in = hull + equipment, the game's own), the job type from the icon's hint, the offer giver; flexe.h block "Extended HUD 2 readers"; 4:3 and 720p checked |
| SPEC | adf77f1, bc06323 | the `ui_style` row in the living spec (ui/spec v114), `UI_SPEC.json`, `ui_meta.h`, `SETTINGS.md` |
Deviations from section 10, each with its reason: the pause focus sits on the hit box (in game the drawn frames coincide with the hit boxes: the mock-up's 0.027 H shift came from a capture artefact); the focus source is the game's hit test at the cursor (the hovered control goes stale out of focus); **left native**: the nav map's frame (its draw never reaches the base draw, so +0x84 is not trusted there), the repair window's art, the scroll bars (a bar draws itself; clearing its bit 1 would remove its clicks), the native rollover outline in pad mode (no safe switch found); the docked lane's five pills can reach over the room's sub-tab plate; the confirm dialog's own box (kept) dims the focus fill; the compare line is a column inside the transfer panel (under the list it collided with the panel edges); the offer giver sits top right of the body; the deal words on the row's name line (the price covers the spot left of the square); difficulty and reward are not repeated (the game writes both into the job / offer text and keeps no field for them); at 4:3 with a window open the strip keeps two lines (room and cargo drop).

## 12. Verification, critique round 3 and the fix round (25 Oct 2026; evidence `research\experience\runs\hud2x_verify_2026-10-25\` (REPORT.md), `hud2x_fix_2026-10-25\`, `perf_pad_2026-10-25\`, `hud2x_final_2026-10-25\`)
- **VERIFY** (dec4dc7, f410c26, 8d2f8ec; release build, instance 8): every screen at 4K (pad and keyboard-mouse), 1280x720,
  2560x1080 and 1600x1200 (pad) opened and closed by name; ui_style 0 = the earlier captures; ui_style 1 survives
  `config save` + restart; the harness with ui_style 1 all PASS (every room, TraderBuy, Restock, Repair, the job board
  accept, save from pause + Load-FLSave, Start-FLLaunch, M01a steps 0-1); 182 / 182 symbols; Extended HUD 2 67-84 us per
  frame at 4K. Fixed on the way: the character card at the game's own card position (the ToolTip's +0x60), the native
  ToolTip hidden only under the card, plates off during the letterbox, the repair family.
- **Critique round 3** (`research\evidence\controller_ui\hud2x_critique3.md`, on the built screens): keep it as the
  switchable option; the strip passes contrast over the brightest rooms (5.7:1 worst); every decision number matched the
  game's own on the same frames; the seams to fix: focus on the hover instead of the selection, the docked pill lane,
  FLvibed overlays over the modals, LB / RB with two meanings, plates missing native children, the per-shot compare,
  the olive focus fill.
- **FIX** (5a7e565, 39fe840, 9f3a3bf, 35b2b52, a1bc3e6, 1a91d79, e13fbe1): the pill lane's left limit (wraps to a second row,
  else drops pills; A and BACK stay); the room strip off under Options; HUD 2's context line off while chat / the OSK is
  open; the subtitles step aside under pause, load / save, Options, confirm, the front-end load and the main menu (both
  ui_style values: a pre-existing overlap); the front-end load screen in the load / save family; focus = the selected row
  (the row's flag +0x35C bit 0x10), one focus mark per screen, yellow edges on the fill; the confirm's focus above its
  dimming box; the room bar's bumper glyphs hidden while a window binds the bumpers; plates around the cargo bar and the
  QUANTITY label; the equipment compare per second (DPS, energy, speed, range). Left: the faction names' colour on Player
  Status (not from +0x348: its source not found), the confirm's ornate frame, the death dialog's focus, the job board's
  native tooltips.
- **PERF** (bc886f3, + a1bc3e6): a pre-existing cost of the pad's menu navigation for every pad player, whatever ui_style -
  `UpdateUiNav` re-collected the clickable controls (~2,800 hit tests) every frame until the first D-pad step: 2.5 ms in a
  room, 7.4 ms per frame on a dealer at 4K (100 fps vs 189 with keyboard-mouse). Now a cache refreshed when the control
  tree's clickable set changes: 37 us in the bar, 65-80 us on the dealer (185-200 fps), flat over 10 minutes.
- **The owner's page**: artifact `HvVSFUvvpa6aGicw8SWyfH` (https://claude.ai/artifact/HvVSFUvvpa6aGicw8SWyfH): a drag
  before / after per screen (29 screens, release build 4K pad, `research\experience\runs\hud2x_final_2026-10-25\`), grouped
  by family, how to switch it on / off, what stayed native; generated by `tools\uimock\hud2x_page.py` (republish the same
  file to keep the link). The final captures' README lists what they found; FIX2 follows (the ship dealer's level refusal
  restored, the job board's focus at 21:9 / by entry path, the wrapped pill row, the Options screens' pad focus, the OSK
  over the chat field).

## 13. The bottom lane (25 Oct 2026, the LANE track; facts `research\evidence\controller_ui\hud2x_lane_probe.md`, evidence `research\experience\runs\hud2x_lane_2026-10-25\`)
The owner: the prompt pills at the BOTTOM of every screen, uniform, never over subtitles or other elements, prompts on
every menu. Built in `promptbar.cpp` (placement, both `ui_style` values - only the look differs) over uiskin's geometry pass
(`uiskin.cpp` Geometry: the keep-outs under ui_style 0 too, from the same plate rectangles, with no drawing and no write):
1. **One lane**: one row right-aligned at the 0.05 W margin in the band 0.910-0.950 H; HUD 2 pills 0.040 H tall (glyph
   0.032 H, verb 0.026 H; a hold action puts the small dim word HOLD BEFORE its glyph at the 0.024 H floor, chrome at
   85 %, 0.35 g from the glyph's broken ring - the owner, 26 Oct 2026, 7c2b73a / 90d6069; the pill's width includes it, so
   rules 3-4 act on the real pill; where the word would drop a pill or change the lane's mode against the lane without any
   word, the cascade of `UI_HUD2.md` 5.9 (the context-line row) picks the first step that keeps it: the word above the
   glyph - only where its place above the pill is clear of the keep-outs and the other pills, and then part of the lane's
   area for the subtitles -, over the label, one shared word, the outline alone; the plan is cached per layout, status
   `hold=<step> holdRuns=n`); the classic look keeps its glyph + label + HOLD pairs on the gradient, in the same places.
   The placement's geometry is `core\game\promptfit.h` LaneLayout / LaneHold (host-free `tests\promptfit_test.cpp` across
   the display matrix, `research\experience\runs\hold_word_2026-10-26\layout.md`: every hold screen keeps its place with
   the word before the glyph, except the ship dealer at 21:9, which the word before would turn into a column - (a) keeps it
   a row; nothing drops; verified in game at 4K and 4:3 on the job board's HOLD A ACCEPT). The docked
   top lane is gone except as rule 4's last resort.
2. **Keep-outs in 2D** (rectangles): every open window's plates (modal ones too) or, plate-less, its rect; the tabs; the
   room bar, its sub-row and LB / RB, the NeuroNet box; uistrip's drawings (`uistrip::Drawn`: strip, card, words); the
   QUANTITY line (`padbases::QuantityRect`); the OSK; HUD 2's ship / loadout / context panels under a menu in flight; the
   chat keeps FIX2's rule (the row off its Console field).
3. **Trim before moving**: while the lane is in use (`promptbar::LaneWanted`: prompts set, pad, prompt_bar not OFF) the
   plates that end in the band end at their content + 0.006 H (`PlateDef::trim`): news 0.909, Options in game 0.908, front
   end 0.929. The front end's trimmed plate still reaches the band under the row: rule 4.
4. **Column, then last resort**: where the row collides, the pills stack bottom-up right of the window (bottom 0.950 H,
   0.008 H apart) when the free column at the bottom slot is as wide as the widest pill, dropping by the bar's order (A and
   B stay); else, docked, the top-right lane under the NeuroNet box (two rows, then drops), elsewhere the row just under
   the lowest obstacle - logged once per screen and size (`promptbar: ... last resort`).
5. **Subtitles**: on a docked non-modal screen the lane in use is published on the HUD bus (version 2: `HudLane`,
   `PublishLane` / `ReadLane` / `LaneFrame`; an older bus or controller = no lane), whether the pills show or have faded;
   `display.cpp` Place keeps the lowest row 0.010 H above it (a row always, a column only where the block reaches it; a
   block inside the letterbox bar that the lane cuts into rests above it). Keyboard and mouse: unchanged.
6. **A and B on every menu**: added A SELECT to news / NeuroNet log / Player Status / Current Information and A OK + B
   CLOSE to the rumour window; the main menu and the death dialog keep A only (B does nothing there by design).

**Where rule 4 applies (measured, instance 3):** 4K: HUD 2 dealers (commodity / equipment) -> top (the widest pill, LB RB
YOUR CARGO / DEALER, 0.325 H, wider than the 0.237 H column right of the infocard plate); classic dealers -> column; ship
dealer -> column (both looks); front-end Options -> column. 4:3: dealers and ship dealer -> top (column 0.04 / 0.08 H),
front-end Options -> below. 21:9: dealers -> column, everything else a row. 16:10 (1280x800): dealers -> top (column
0.156 H), ship dealer -> column, front-end Options -> below (HUD 2) / column (classic). Every other screen: the row.
**Dealers' bumper pill "SIDE"** (the owner, 25 Oct 2026; was YOUR CARGO / DEALER, YOURS / DEALER): the HUD 2 dealers at 4K
take the column (commodity widest 443 px, equipment 355, column 512) and stay there under 2560x1080 (commodity / equipment
column, ship dealer row); 1280x800 keeps the top lane (QUANTITY 160 px > the 125 px column, expected).

**Not fixed - the ui_style 1 -> 0 dealer art** (probe 5): the panels stay blank when the window was skinned within its
first ~0.5 s (reproduced: opened under ui_style 1, or switched within 0.3 s of the open; a window open 0.7 s first
restores fine; a native row click does not bring it back). Likely cause (inferred from the timing, not found in the code): the panels' open animation only runs while +0x84 holds the mesh handle (NN_Inventory's
draw 0x479700 -> WindowBaseDraw 0x5A0760 checks it at 0x420D60); hidden from the start, the restored mesh stays at its
start pose. Delaying the hide would show the native art for ~0.5 s at every open: the owner decides.
**Fixed (25 Oct 2026, uiskin `Repose`):** the open slide moves / scales the window through its pose setters (vtable +0x14
0x5A0880 position at +0x60, +0x1C 0x5A0950 orientation at +0x3C), which pass the pose to the engine's mesh only while
+0x84 is a handle; after the restore uiskin calls both with the stored pose (children follow), so the art is back without a reopen.

Commits 1b3df77 (rooms: the player's own character is the pad's default focus - the owner's addition), 014f39a (the bus +
the subtitles), 7910aaf (the lane), da73c32 (the release build links). Verified on instance 3: the dev build at 3840x2160
(both styles), 1600x1200, 2560x1080, 1280x800 (both styles) - `research\experience\runs\hud2x_lane_2026-10-25\`
(`<size>_<screen>.png`, `<size>c_` = ui_style 0, `tour.log` the lane per shot, `harness.log`: Goto-FLRoom,
Invoke-FLTraderBuy, Start-FLJob, a save from pause + Load-FLSave all pass); the release build's 29 pairs at 4K pad in
`research\experience\runs\hud2x_final_2026-10-25\` (README "LANE recapture"). Owner's open calls: the HUD 2 dealers at 16:9
/ 16:10 take the top lane (a narrower dealer pill, e.g. "LB RB CARGO / DEALER", or glyph-only bumper pills would fit the
16:9 column; at 16:10 the column is 0.156 H, so only glyph-only pills or the infocard's empty lower-right corner - not
reliable: the equipment Stats column reaches 0.961 H - would keep them at the bottom); the dealer art bug above.

**Critique 4 fixes (25 Oct 2026; `research\evidence\controller_ui\hud2x_critique4.md`, evidence
`research\experience\runs\hud2x_c4fix_2026-10-25\`, the release recapture in `hud2x_final_2026-10-25\` README "CRITIQUE 4"):**
- Rule 4, docked last resort: the top lane under the NeuroNet box takes as many right-aligned rows as the room holds,
  filled from the bar's end, each as wide as its free band; it drops only when that runs out (4:3 dealers: MAX CLOSE /
  SIDE QUANTITY BUY, ship dealer BUY CLOSE / SHIP STATS; 16:10 HUD 2 dealers the same two rows).
- Rule 4, elsewhere: the nearest legal band - under the lowest obstacle while the row still ends by 0.950 H, else the first
  free band above the lane (front-end Options at 4:3 / 16:10: SELECT / BACK above the Options plate, 0.075-0.115 H).
  Nothing is placed below the 0.95 H safe margin.
- **The fit rule in the lane (FIX-TEXT, 25 Oct 2026; `docs\UI_HUD2.md` 5.13):** the order is now the row; the row with the
  labels at the legibility floor (0.024 H, >= 14 px); the column; **two rows at the bottom** (labels at the floor: the
  band's row takes the bar's end, the row 0.008 H above it the rest, each clear of the keep-outs in its band); the column
  at the floor; only then the last resorts (status `lane=rows`, `atFloor=1`). Measured on HDE 1600x1200 / 1280x720 and
  vanilla 1280x960: every screen of the tour keeps a bottom row or column (room, job board, pause docked / in space, the
  options page, the 720p dealers' column) except the **equipment dealer at 4:3**, whose infocard plate covers the band to
  0.975 H and to 0.91 W (a 44-58 px column): the top lane under the NeuroNet box stays its place (geometry, not text).
  The vanilla run's "no pills" there was the top lane after the contextual 3 s fade, plus pause's transient below-lane
  while HUD 2's panels fade under the menu.
- Rule 2: the chat's Console field is a keep-out like the rest (FIX2's own row is gone): its pills take rule 4's column
  right of the field; the OSK's keep-out is its own panel (centred, from 0.50 H), not the whole width.
- Rule 5, the subtitles (HUD bus v3: `HudFloor`, `HudDockedSubs`): the block's top also stays 0.010 H under the open
  windows' floor in its x range (the lowest plate edge per 1/32 of the width, published by the prompt bar). **Decided:**
  where the block does not fit between the window and the lane, the lane counts only where it reaches the block's x
  range (21:9 / 4K offer: the block at its base bottom beside the pills); else **the lane yields** - the block hangs
  right under the window, is published on the bus and the pills keep clear of it (they move to rule 4's column). Where
  even that would leave the safe area (a window reaching the screen's foot), the block rests at its base bottom and the
  lane yields. Not "above the window": the offer / job board fill the room above them and the line would leave its
  speaker's place at the bottom. The pills are never under the subtitles; the subtitles never over a window's buttons.
- Confirm: A YES / NO (the focused answer; SELECT off them) · D-pad CHOOSE · B NO. Rooms: no A pill while Trent holds
  the focus (his click starts nothing, FREELANCER_RE "Base rooms"). Y names what the press does, with or without a
  character strip: LAUNCH while the room has the game's launch hotspot (`IDS_HOTSPOT_LAUNCH`), else JOB BOARD (the
  strip's prompts had only the job board, so the launch pad with Trent on it offered no Y LAUNCH; fixed 25 Oct 2026).
- The card keeps off the other characters (uistrip PlaceBy): of the places the hard rules allow (above centred, slid off
  a neighbour, lifted up to 0.08 H; beside; under) the least covering of the others (heads 20x bodies), then the least
  moved; both styles.
Commits 619dd22, c0e89fe, 33ffa96, e3fcb5d, 85fb446.

## 14. Beside FLUF (26 Oct 2026, the A2 work package; verified in game with the nightly, A3 + step 2 - `MOD_COMPATIBILITY.md` FLUF)
- The docked overlay's parts (`uistrip.cpp` Draw) each ease to 0.15 alpha on their own while one of FLUF's windows covers
  that part's last drawn rectangles - the strip, the character card and the decision words separately (step 2, 26 Oct
  2026: A3 had faded the whole strip and the card at the bottom centre when the Debug Window covered only the strip's
  left end). FLUF draws over them and takes the mouse there.
- **The owner's decision (26 Oct 2026):** FLUF's windows are always on top; nothing of Extended HUD 2 is moved for them -
  the fade under a covering window is the behaviour, and it counts every FLUF window (`fluf_windows` default 2, the
  Debug Window included).
- The bottom lane (rule 5's keep-outs, `promptbar.cpp` CollectExtra) takes FLUF's windows as keep-outs like any window.
- While a FLUF input window is open (palette, options / Player Status, map, a module window) the pad drives it, not the
  pad screens (the cursor on the left stick, A / X mouse buttons, B Escape - a palette command's or module's window by
  its title-bar close button since 0e41948, and the B that closes never reaches flight since 0eca73d; the palette's list
  on the D-pad, A runs) and
  the bar shows CLICK / CURSOR / CLOSE or RUN / SELECT / CLEAR-CLOSE (screens `fluf.window`, `fluf.palette`).
- The skin's plates (the game's own windows) are not moved: FLUF's windows lie over them as over the game's.

## 15. Closing the NeuroNet windows (26 Oct 2026, the dealer-close bug; evidence `research\experience\runs\dealer_close_2026-10-26\`)
- **The report:** the equipment dealer would not close on the pad's B (the bar said B CLOSE) nor on the harness's
  `click Inventory_Close`, with or without FLUF; once B seemed to open the BUY transfer panel instead.
- **Root cause (the game's, not a regression of 26 Oct):** the docked NeuroNet window manager drops a close while its
  open / close slide runs (~0.5 s right after the window's controls show) - the game's own Escape too, in both
  `ui_style` values, on HDE 0.7.1 and vanilla 1.1 (`FREELANCER_RE.md` "The NeuroNet window manager's slides drop a
  close"). The step-2 runs pressed B (or clicked) once, 60 frames after the list showed, inside the slide, and then only
  polled. The "BUY panel" was the pad's entry focus selecting the first row while the lost B did nothing. Not bisected:
  the drop is the game's own close function returning (disassembly) and reproduces with the skin off and with Escape.
- **Fix:** the pad's B on a NeuroNet window (`padbases.cpp`) is held while the window slides (`room::NNWindowBusy`, the
  manager's transition bytes through signed code) and clicked natively the first frame after (at most 2 s, then dropped
  with an event: fails closed); the entry focus and the row reselect wait while a close is held (no BUY under a close).
  `goto` answers `busy: nn-window` during a slide (the harness's loops retry) and `click` notes it. Escape stays the
  game's (dropped in a slide, as in vanilla).
- **The close frame (the owner's "slides off-screen / flickers"):** dense back-buffer frames (`grab`, every frame) show
  the dealer windows gone in one frame, no native art at any point - but for one frame the skin's room-bar and NeuroNet
  box plates stood empty (the game stops drawing their art in the close frame while the controls read shown). Fixed:
  `uiskin.cpp` draws no docked frame while a close runs (`room::NNWindowClosing`: a slide with no window open).
- Verified (release build): HDE 4K (instance 1) and 4:3 1600x1200 (instance 9), vanilla 1.1 without FLUF and with the
  FLUF nightly in both `[Libraries]` orders - B at 0 / 15 / 40 frames after the list showed (held, then closed), B with
  the transfer panel up, Escape, the native close click (what the mouse's release runs), a goto right after a close
  (`busy: nn-window`), the reopen: all pass; the close frames clean in all five (`grab\*_close*`, `shots\*_sheet.png`).
