# The player-facing UI: the autopilot button and the FLvibed options page (design, 25 Oct 2026)

Status: **design for the owner's decisions (section 5); nothing built.** The brief (user, 25 Oct 2026): (1) a new
maneuver-bar button that toggles the autopilot, with a default key the game does not bind; (2) options in both the main
menu and the in-game menu for what the mod adds or changes (subtitles and how they show, autopilot configurations,
sensible defaults, anything else the mod changes), after a look at the whole project and at how well-regarded games do
options, fitting the game's own design and flow.

Grounding: the game's screens measured in the test install (`FREELANCER_RE.md` "Options screen", "Main menu, load
screens, pause menu"; screenshots `research\experience\ui_2026-10-25\`), the 24 Oct checkup's UI brief
(`..\checkup_2026-10-24\ui_grounding.md`), the research round `docs\research\options_menus.md`, the settings
registry (`core\settings.h`, `docs\SETTINGS.md`). Mock-ups over the real 4K screenshots:
`research\experience\ui_2026-10-25\mock_options_mainmenu_autopilot.png`, `mock_options_ingame_subtitles.png`,
`mock_maneuver_bar_autopilot.png` (built by the session's script; not pixel-exact, the game's own fonts).

## 1. Principles
1. **Fit the game, do not decorate it.** The mod's options live inside Freelancer's own Options screen, as one more
   page button in its column, with the game's row grammar (label left, control right, ENABLED / DISABLED beside a
   checkbox, a picker with arrows, a slider with a value caption, section headers, a rollover help line at the top,
   DEFAULT and CANCEL at the bottom, instant apply). The bar button is a fifth maneuver button, lit like the others.
2. **One registry drives everything** (`core\settings.h`): the page reads the rows, ranges and help from it; `set`,
   `ap set`, the ini files and `docs\SETTINGS.md` stay what they are. Nothing in the page is a second copy of a value.
3. **Short player tier, "Show advanced" for the rest** (GoW's 70 options "needed a break"): about 60 player rows across
   five tabs; the ~95 developer keys appear only with the switch on, raw (label = key, help from the registry).
4. **Presets first, rows second, CUSTOM when a row deviates** (MSFS, Forza, TLOU2, GoW); a preset never silently undoes
   a change made after it (the preset row is at the top; changing it re-applies, the rows below show the result).
5. **Every row explains itself** in the rollover line (the registry's help, written for players), marks a changed value
   with a dot (not colour alone), says "after restart" or "at the next load" where that is true, and greys out when a
   parent row disables it.
6. **Subtitles are previewed live** on their tab (XAG 104; Forza, Spider-Man): a sample line through the display's own
   code, so what the player sees is what they get.
7. **Pad-ready from the start**: the same page is walked by the D-pad (rows), LB / RB (tabs), A (toggle / open), B (back),
   left / right (values); it is the first screen of the controller-first UI (`controller\docs\TASK_GLYPHS_HUD.md`),
   whose glyph and prompt layer land on it later. Native game logic for the entry (the game's own button), own drawing
   for the page (the proven renderer); resolution, aspect and DPI by the UI camera, as the subtitles do.
8. **Fails closed** (CLAUDE.md): an unresolved symbol leaves the game's screen as it is; the settings stay reachable
   through the ini files and the debug commands.

## 2. The autopilot button on the maneuver bar
**What the player sees.** A fifth button after Formation (`mock_maneuver_bar_autopilot.png`). A click toggles the
Advanced Autopilot exactly as LB + L3 does: with a target, the smart start (hostile -> fight, friendly ship -> escort,
dockable -> dock, else fly to); without one, the objective / auto mode; a click while it flies hands the ship back.
While the autopilot flies the button is lit (the active maneuver's yellow); unlit otherwise; grey when the autopilot
cannot start (the maneuvers are locked, a cutscene, the ship is docking). The game's "Auto hide maneuver bar" option
hides it with the bar. The bar's frame grows by one button pitch; the cruise box and the tab keep their place under
the bar's left end. In pad mode the later pad HUD replaces the bar (TASK_GLYPHS_HUD section 5); the button is the mouse
twin of LB + L3.

**Mechanism** (facts: `FREELANCER_RE.md` "Options screen", the maneuver bar paragraph). The game builds its four
buttons from `hud.ini [Maneuvers]` through the icon-button factory 0x5A0C90 into four fixed slots and lays them out
every frame (`exe::ManeuverLayout`, already observed by the subtitles host). FLvibed creates **its own native icon
button** with the same factory, parented to `HUD_Maneuvers`, and places it in a chained `ManeuverLayout` observer of
the controller host after the game's pass (x = the fourth button + one pitch; the frame part scaled in x by the same
code path the subtitles use for narrow screens). Native button = the game's hit test, hover, click sound and the
"click on UI vs fire" arbitration for free; the click is routed by the signed `IconButtonClick` / `IconButtonActivate`
detours to the controller logic (`autopilot::StartForTarget` / `Stop("bar")`); its lit state is set by the game's own
messages (vt[0xA8] 0xC) from `autopilot::Active()` each frame. The icon: first an **overlay-drawn glyph** (render.*
sprite at the control's projected rectangle, the bar's light blue, yellow when lit); later an own `.3db` mesh shipped
in `EXE\FLvibed\` for the exact shaded look, once loading a mesh from outside `DATA\` is probed (`core\game\utf.*` can
write UTF). Tooltip: "Autopilot" through the hint ids (+0xCC) only if a private string id can be served by the text
hook; otherwise none (the rollover is a nicety). Setting `[Controller] maneuver_button` (default 1). Fails closed: any
unresolved symbol = no button; the key and the pad chord remain.

**The key** (`[Controller] autopilot_key`, shown and rebindable on the Autopilot tab; the game's Controls page cannot
list it - the command table is fixed). Checked at start against the live keymap (0x67C258): a key the game binds is
refused with a log line and the row says so. Ignored while the chat field (`CTID`) or a text field has focus.
Candidates, all unbound in HDE's `keymap.ini` (every bound key is listed in `FREELANCER_RE.md` "Keyboard" / the
session's extraction):

| Key | For | Against |
|---|---|---|
| **K** (recommended) | the only plain letter the game leaves free; one press, no modifier; nothing bound around it that a slip would hit | no mnemonic; right hand (the autopilot is a calm action: the hand is free) |
| F10 | continues the bar's F-key row (F2 Goto, F3 Dock, F4 Formation; F5-F9 are the NeuroNet windows) | Windows delivers F10 as a system key (WM_SYSKEYDOWN); the game handles SYS messages, but it needs a probe; Alt+F10 is a debug command |
| Shift + A (X4's autopilot key) | the game's own idiom for variants (Shift + W cruise) | A is strafe left: a chord on a held movement key misfires |
| F11 / F12, Insert, [ ] ; , . / | free | no mnemonic; F12 is Steam's screenshot default |
| Caps Lock | a toggle key for a toggle | toggles the OS caps state: no |

**Verification**: the button appears / disappears with the setting; a click starts and stops the autopilot (events
`autopilot start reason=bar` / `stop reason=bar`); lit follows `Active()`; frame width, tab and cruise placement in
the display matrix (4K, 1080p window, 4:3, 21:9; HudShift's bar group shifted; the bar auto-hide on); the game's four
buttons unchanged; release build. No autopilot behaviour changes, so no apdiff.

**Effort**: S-M (the unknown is the click routing of a button the game did not create: the factory's action slot).

## 3. The FLvibed options page
### 3.1 Entry: a page button in the game's Options screen, in both menus
The Options screen (`NN_Preferences`) is one class for the main menu and for F1 -> OPTIONS, and it has a free slot in
its column in both contexts (under CREDITS in the main menu, under AUDIO in game; `FREELANCER_RE.md`). FLvibed adds a
page button **FLVIBED** there with the game's icon-button factory and the siblings' mesh (`mock_options_*.png`). Its
click hides the game's current page controls (the state byte the game itself uses) and shows FLvibed's page in the same
panel; any other page button, RETURN TO GAME / MAIN MENU or the X restores the game's page. So the main menu and the
in-game menu both get the options through the flow players already know (Options -> FLvibed), with no sixth pause-menu
button (the checkup's earlier idea) and no game file touched. The caption: the game's buttons take an IDS string; ours
is served through the text hook or drawn by FLvibed over an uncaptioned button (decided at implementation, both are
known paths).

### 3.2 The page
Title **FLVIBED OPTIONS**; a row of sub-tab buttons as the Controls page has (SHIP CONTROLS / USER INTERFACE /
MULTIPLAYER): **GENERAL / CONTROLLER / AUTOPILOT / SUBTITLES / MOVIES**; under them the rows in the game's style with
section headers where a tab is long, scrolling like the Controls page's table when needed; the rollover help line at
the top of the screen; **DEFAULT** (this tab) and **CANCEL** (every change since the page opened) at the bottom; leaving
the page keeps the changes and writes the ini files through the registry's writers. Changes apply as they are made
(the game's own semantics; the subtitles and the autopilot already apply `set` live); a row whose effect comes later
says so ("at the next load" for the in-flight subtitle position, which moves the bar; "after restart" for a module).

**Drawing and input.** The rows are FLvibed-drawn replicas of the game's controls through `core\game\render.*` (the
game's faces from `DATA\FONTS`, sizes from `fonts.ini`, the colours measured from the screenshots), placed by the UI
camera so every resolution, aspect and DPI works as the subtitles' panels do. Mouse: hover = focus, click acts; the
page area is claimed through the host's `HitTest` detour (no guns in this screen anyway; the game's own buttons around
the page stay native). Keyboard: up / down rows, left / right values, Enter toggles, Escape = the game's own close.
Pad: D-pad rows, left / right values, A toggles / opens, LB / RB tabs, B back - the focus kit the controller-first UI
needs anyway. Native row controls through the game's factories (ToggleButtonControl, ScrollControl) stay a later
refinement: they give the exact look but couple to PerfOptions and need the RE the checkup priced as heavy.

**The row kit** (one control per registry type, plus the UI metadata of 3.4): toggle (bool) = checkbox + ENABLED /
DISABLED; picker (enum / word settings) = the box with arrows, the value as a word; slider (int / float ranges) = the
game's slider with a value caption, words where the registry names steps (Caution: NEVER LEAVE / WHEN OUTMATCHED /
EARLY); key = a box showing the key, click or A = "press a key"; action = a button (Reset all, Test). A dot before a
label that differs from its default; a greyed row when its parent is off; the help line from the registry.

### 3.3 Tabs and rows (player tier; defaults = today's; the ini key in brackets; wording to confirm)
**GENERAL** - Subtitles module ENABLED (after restart) [`[Modules] subtitles`]; Controller module ENABLED (after
restart) [`controller`]; Show advanced settings DISABLED (adds the developer keys to every tab, raw); Reset all FLvibed
settings (button); the version and a one-line About (credits and licences, filled at publishing).

**CONTROLLER** (sections) - AIM: Aim mode GIMBAL / RETICLE [`aim_mode`], Lock-on cone [`aim_cone`], Invert Y
[`invert_y`], Steering response [`steer_expo`]; STICKS: Stick deadzone [`stick_deadzone`], Trigger deadzone
[`trigger_threshold`], Throttle rate [`throttle_rate`]; TARGETING: Auto-target OFF / CLOSEST ENEMY / ... [`auto_target`],
Firing selects the target [`fire_target`]; MENUS: Cursor speed [`cursor_speed`], Scroll speed [`wheel_rate`]; HUD CUES:
Shield hit cue [`shield_cue`] + Intensity [`shield_cue_intensity`], Hull hit cue [`hull_cue`] + Pattern
[`hull_cue_pattern`], Reticle cue [`reticle_cue`]; SYSTEM: Controller only while the game is in focus [`focus_only`],
Keep the game running in the background [`run_in_background`]. Later, as they are built: Input device AUTO /
CONTROLLER / KEYBOARD, Pad layout preset (BINDINGS.md), Rumble, Text size, Pad cursor.

**AUTOPILOT** - Preset COMPETENT / SPEEDRUN / NO CONSUMABLES / CUSTOM [`ap preset`]; Knowledge WHAT YOU HAVE SEEN /
EVERYTHING [`knowledge`]; Combat THE TARGET ONLY / TARGET, THEN NEAREST [`engage`]; Fire the guns [`auto_fire`];
Missiles [`missiles`]; Countermeasures NEVER / WHEN IT MATTERS / EVERY MISSILE [`countermeasures`]; Nanobots and
batteries NEVER / WHEN NEEDED / AS THE PILOT WOULD [`repair`]; Loot after a fight [`loot`]; Caution NEVER LEAVE / WHEN
OUTMATCHED / EARLY [`caution`]; Avoid hazards [`avoid`]; Fast docking [`fast_dock`]; Skip docking cutscenes
[`skip_dock_scene`]; Hand back on stick input (0-100 %) [`autopilot_cancel`]; Autopilot key [`autopilot_key`, new];
Autopilot button on the maneuver bar [`maneuver_button`, new]. A status block (what it is doing and why, the last
events) joins when the status strip is built (`AUTOPILOT_UX.md` 5.4). The ~80 flight-geometry and speedrun keys stay
advanced. Open: whether `pilot` (the game's `[Pilot]` personalities) is a player row.

**SUBTITLES** (sections, the preview line always visible) - TEXT: Subtitles [`enabled`], Text size SMALL .. HUGE
[`size`], Font GAME / SEGOE UI [`font`], Text colour WHITE / YELLOW [`text_color`], Shadow [`shadow`]; BACKGROUND:
Background HUD PANEL / PLAIN [`background_style`], Opacity [`background_opacity`]; SPEAKERS: Speaker names OWN ROW /
INLINE / OFF [`speaker_names`], Speaker colours [`speaker_colors`, **new key**], Hostile marker [`hostile_marker`];
CHANNELS: Story [`story`], Comms [`comms`], Bar [`bar`], Neural Net [`neural_net`], Radio chatter CHAT WINDOW /
SUBTITLES / OFF [`chatter`], Dialogue also in the chat window [`chat_all_dialogue`]; PLACEMENT: In flight TOP PANEL /
TOP THIRD / BOTTOM [`space_position`, "at the next load"], Comm portrait in the panel [`comm_portrait`], Keep the HUD
arrows clear [`hud_arrows`], Cutscenes IN THE BLACK BAR / ON THE PICTURE [`letterbox_mode`], One speaker at a time in
cutscenes [`cinematic_single_speaker`]; MOTION: Motion [`motion`], Spoken-word pop [`word_pop`], Neural Net severity
colours [`neural_net_severity`]. Advanced: the offsets, lingers, narrow-screen variants, chat layouts.

**MOVIES** (new keys: the module's movie handling has none today) - Fit movies to the screen [`movie_fit`, default on];
Startup movies PLAY / SKIP [`movie_skip`, default play; the native skip exists]; Intro AS INSTALLED / HD / ORIGINAL
2003 / FULL (BONUS DVD) [`movie_intro`; only the files present are offered]; Movie subtitles [`movie_subtitles`].

### 3.4 What the registry needs (no change to `set` / the ini writers)
A UI table per module beside `kDefs`: key -> tab, section, order, player label, enum labels (the picker words), tier
(player / advanced), what a change needs (live / next load / restart), preset membership, and for a few rows a parent
(greys out). The help strings exist; some get a player rewrite. New keys: `speaker_colors`, `maneuver_button`,
`autopilot_key`, `movie_fit`, `movie_skip`, `movie_intro`, `movie_subtitles`. The subtitle and binding presets become
data (the autopilot's exist: `ap preset`).

### 3.5 Build order and effort (S <= 1 session, M 2-3)
1. **The bar button** (section 2) - S-M; independent of the page, the user's first ask, visible at once.
2. Registry UI metadata + the new keys - S.
3. The row kit over `render.*` with mouse, keyboard and pad focus - M (the controller-first UI's renderer spike).
4. The Options-screen page button and page hosting (hide / restore the game's page) - M (RE of the page switch).
5. The five tabs - S each; the Subtitles preview S.
6. Display-matrix pass (4K, 1080p window, 4:3, 21:9, DPI-unaware), release build, docs (SETTINGS.md regenerated).

## 4. Out of scope here (kept where they are)
The autopilot goal panel and status strip (`AUTOPILOT_UX.md` 5), the pad HUD, pickers, glyphs and prompt bar
(`TASK_GLYPHS_HUD.md`), the binding pass (`BINDINGS.md`). This page is their foundation (the row kit, focus and the
device switch), not a replacement.

## 5. Decisions (the owner, 25 Oct 2026, through the interactive mock-up; the decided spec: `docs\UI_SPEC.json`)
The mock-up (artifact `4v4dXbB5GTVt1URowFtSvN`: the two Options contexts, the bar, every row live and editable) was
accepted as proposed: tabs, sections, rows, words, defaults and presets unchanged ("mostly pleased, a good place to
start; changes may come later, easier with the expanded UI in place").
1. **The key**: **K**.
2. **The icon**: **a shipped mesh from the start** (an own `.3db` in `EXE\FLvibed\`, the bar's line-art style; the
   glyph design still open - the mock-up's ring-and-chevron is a placeholder).
3. **The entry**: the FLVIBED page button in the game's Options screen in both menus **and a sixth button in the F1
   pause menu** (the direct route in space and bases; it opens the same page).
4. **The tabs**: GENERAL / CONTROLLER / AUTOPILOT / SUBTITLES / MOVIES as mocked.
5. **Semantics**: the game's own (instant apply, CANCEL reverts, DEFAULT per tab, leaving keeps) plus "Show advanced".
6. **The autopilot rows and their words**: as mocked. **6b open**: whether `pilot` (the game's `[Pilot]` personality the
   autopilot flies by, `DATA\MISSIONS\pilots_*.ini`; default `pilot_military_hard`) becomes a player row - the owner
   asked for an explanation first; the recommendation is the advanced tier only (the presets cover the player's need).
7. **Subtitles**: the new `speaker_colors` key **and** the always-visible preview line.
8. **The MOVIES tab** with its four new keys: **now**.
9. **Module switches** on GENERAL: **shown** (after restart).
10. **Order**: **the bar button and the page in parallel** (disjoint files: the bar in the controller module; the page's
    foundation - registry UI metadata, the row kit - in core; the Options-screen and pause-menu entries last).

## 5b. Decisions 11-20 and the standing order (25 Oct 2026, end of the design round)
All 21 decisions are answered in the mock-up's Decisions view (`docs\UI_SPEC.json` carries them): the owner's
calls through the session (LB picker / wheel, the R3 scheme, X hold turret view, Y hold weapon-group cycle, no "my
attacker", the pad cursor everywhere, the HUD kept informative with badges and the button legend, ASSISTED = turret
mode) and the suggested defaults for the rest (6b `pilot` advanced only; 12 hold View; 13 automatic switching with a
lock; 14 the three glyph families, confirm = bottom with a Nintendo swap; 17 the New Game prompt locked per save; 18
the LiveSplit client now; 19 the prompt bar contextual; 20 foundation -> flight HUD -> bases -> dealers -> map).
**Standing order (the user, 25 Oct 2026): the mock-up is the living spec and stays current through development** -
every UI change lands in it (`tools\uimock\README.md`: the page's source, build, publish, how the owner's edits in the
artifact's `ui/spec` document are patched, `docs\UI_SPEC.json` regenerated) in the same commit as the code.

## 6. The controller UI (round 2, 25 Oct 2026; the owner's brief: "nothing must only work with keyboard and mouse")
Research: `docs\research\controller_ui.md` (three rounds of sources in `research\evidence\controller_ui\`). The whole
design is in the interactive mock-up (artifact `4v4dXbB5GTVt1URowFtSvN`, version 3 then - now the living spec, v22 published, v23 in source; its page kept as
`research\evidence\controller_ui\mockup_v2_index.html`): the Input buttons switch the whole page between keyboard
and mouse and the three glyph families, the Screens view shows every game screen (21 screens, 31 states, from the
captures in `research\experience\ui_2026-10-25\screens\`) with its pad layout, the Bindings view the diagram and
presets per situation, the Options views the extended CONTROLLER and AUTOPILOT tabs, the Maneuver bar view the
autopilot button, Decisions 11-20 the questions of this round. Everything is editable there; the decided spec is read
back into `docs\UI_SPEC.json` afterwards.

### 6.1 Principles added by the brief
1. **Seamless replacement**: the controller UI replaces the game's UI the moment pad input registers (one frame), and
   the game's UI returns on a mouse move or a key; both devices always work; a sticky Input device setting locks it.
2. **Everything interactable works on the pad**: every screen in the screen audit has a pad flow that ends in the game's
   own logic (native clicks, the hotspot records, the action dispatchers); nothing is cursor emulation.
3. **Intuitive and ergonomic first**: the default preset borrows the strongest reflexes (RT fire, LT boost, R3 lock-on,
   D-pad consumables, A context, B cancel), the most functions with the fewest presses, chords only for rare actions.
4. **Glyphs for Xbox, PlayStation and Switch**, chosen by the connected pad (HID ids) and overridable; confirm positional
   with a Nintendo swap.
5. **With and without the autopilot**: the FREELANCER and ACE presets fly manually; ASSISTED hands the flying to the
   autopilot and keeps aiming and decisions; every preset has the autopilot one hold away and the hand-back on any stick.

### 6.2 Device switching (decision 13)
Last valid input wins on unpaused game time: pad = any button, D-pad, a trigger past 0.5, a stick past ~0.25 (above the
XInput dead zone, so drift never counts); keyboard and mouse = any key, ~8-10 px of mouse motion at 1080p in one frame
(scaled by DPI) or a wheel notch. After a switch the other device's axis-only signals are ignored for 300-500 ms;
buttons and keys switch at once. The HUD and prompts swap in one frame; the cursor is hidden and hover disabled in pad
mode. Settings: Input device Automatic / Controller / Keyboard and mouse; a pad unplugged in flight pauses the game.

### 6.3 Glyphs (decision 14)
Families Xbox / PlayStation / Switch over one positional layout (south = confirm). Detection: XInput slot matched to
the HID vendor / product id (SDL's table; Sony 054C, Nintendo 057E, Microsoft 045E, 8BitDo 2DC8), Steam Input's type
when present, Xbox for unknown pads. Settings: Button prompts Automatic / Xbox / PlayStation / Nintendo; Confirm with
bottom / right button. Art: Kenney Input Prompts 1.5a (CC0) in one style for all families; Elite paddles drawn by us.

### 6.4 The presets (decisions 11, 12; the whole table in the mock-up's Bindings view)
**FREELANCER (default)**: RS steer (gimbal lock on the target), R3 tap = the object under the reticle (the screen
centre with the reticle off), else the nearest enemy / tap again = the next of that kind / double tap = clear / hold =
rear view (the owner's scheme, 25 Oct; the double tap costs the cycle a double-tap window - B when nothing is running is
the fallback home for "clear");
LS throttle lever + strafe; L3 engine kill; RT fire; LT afterburner (hold); LB tap the target picker / hold the
action wheel (scan cargo, weapon group, cruise disruptor, formation, comms, contact filter, autopilot goals, rear
view; picked with the right stick, dead-zoned, the D-pad works too, the left stick keeps flying - the owner's calls of
25 Oct: wheels belong on a bumper, scan cargo is contextual and fits the wheel, and Freelancer needs no "my attacker"
bind, nearest enemy plus firing-selects-the-target cover it); RB missile / hold torpedo or cruise disruptor; A context
(dock, lane, jump, fly to; loot in reach) / hold formation; B cancel (free flight, drop cruise, cancel dock) / hold
reverse; X tractor loot / **hold turret view**; Y cruise; D-pad nanobot / battery / countermeasure / mine; View
NeuroNet hub (map first) / hold
half a second = Advanced Autopilot on / off; Menu pause. **ACE** (arcade flight, for Ace Combat / Rogue Squadron / Star Fox hands; tidied 25 Oct to the same rules): LS
steers (gimbal lock on the target), RS strafes, RT throttle up with afterburner at full pull, LT throttle down / brake
with reverse at full pull (the lever persists, as Ace Combat's accelerate / decelerate), A guns, B missile / hold
torpedo, Y the smart target (under the reticle else nearest, again next, double clear - Ace Combat's change-target
button), X context / hold turret view, RB next enemy / hold previous, LB tap picker / hold the action wheel, R3
cruise / hold rear view, L3 engine kill, D-pad consumables as above, View hub / hold autopilot, Menu pause. Favours one
thumb flying and aiming with the other free for the weapons; sets aside the finger throttle lever and the D-pad's
cheapness (the left thumb steers, so a nanobot mid-fight costs steering). **ASSISTED**: the autopilot flies; RS aims, RT guns, RB missile,
A context / accept, B hand back, Y target / hold picker, X goal panel, D-pad consumables. Shared situations: menus
(D-pad / LS focus, A accept, B back / hold close all, X context, Y secondary, LB / RB tabs, LT / RT pages, RS scroll, R3
reset view, L3 pad cursor), bases (LB / RB rooms, D-pad left / right characters, A talk, Y launch or job board, X
news, LS cursor), dealers (LB / RB side, D-pad rows, LT / RT quantity, A buy / sell / mount, Y all, X filter), nav map
(LS cursor, D-pad snap, A select / again waypoint, X remove / hold clear, Y travel, LT / RT zoom, LB / RB system /
universe, View destination list), text entry (an own on-screen keyboard; A type, X space, B backspace, Menu done).
Playtest before freezing: afterburner on LT vs LB vs L3, cruise on Y vs L3, engine kill as a toggle, the D-pad reach
while steering, the wheel's selection input.
**Weapon groups** (the owner's question, 25 Oct: some want it simple, some granular): a tiered setting
`weapon_groups`. **SIMPLE (default)**: RT fires the active group (the game's default group), group switching in the
action wheel, the HUD's group buttons badge the current one; **CYCLE ON Y HOLD**: Y held flips to the next group (Y tap
stays cruise), the badge and the combat legend show it; **TWO GROUPS ON THE TRIGGERS**: RT fires group 1, LT group 2
(the Armored Core / MechWarrior pattern; afterburner moves to L3, engine kill into the wheel; ACE: A / B). In every
mode launchers fire only on their own buttons; groups are assigned in the inventory (D-pad left / right ticks a focused
weapon's group boxes, the game's own checkboxes).
**Turret view** (the game's H; the owner's question, 25 Oct): in FREELANCER **X held** (the owner's call), in ACE a
toggle in the action wheel (RS aims, RT fires, LS keeps the throttle and strafes, the ship flies straight on, B exits);
**ASSISTED is built on it** - the autopilot flies and fights, the player aims and fires from the
turret camera (this answers AUTOPILOT_UX.md open question 11: turret mode = the Assisted preset, not a separate
toggle). Hand-back exception: with the autopilot on, the right stick is the aim in turret view and does not hand the
ship back; the left stick or B does.

### 6.5 The pad HUD and the screens (decisions 15, 16, 19)
**The flight HUD stays informative in pad mode** (the owner, 25 Oct: as with keyboard and mouse): nothing of the
game's HUD is hidden. The pad adds **glyph badges on the HUD's own elements** that a button acts on - the maneuver
bar's four buttons and the autopilot button (B free flight, A goto / dock, A held formation, View held autopilot), the
cruise box (Y), the NeuroNet box (View), the contact-list expander and filters (LB = the picker), repair and battery
(D-pad up / down), the weapon groups and next enemy - lit with their element; a **contextual button legend** in the
empty right-middle of the screen (the Lost Judgment / RGG pattern; setting `button_legend`, default on) listing only
the actions that apply now with their glyphs and counts (a dockable target: DOCK, CRUISE, TARGETS, MAP; a fight: FIRE,
AFTERBURNER, MISSILE 24, NEXT ENEMY, NANOBOT 3, BATTERY 3, COUNTERMEASURE 12, MINE 8, REVERSE held; the autopilot:
HAND BACK, GOALS, AUTOPILOT OFF held), stepping aside while a panel has its own prompt bar; an **autopilot status
line** under the maneuver bar while the autopilot flies, cruise charges or turret view is on; **world prompts** only
when something unusual is possible now; the **target picker** on LB tap, the **action wheel** on LB hold, the **goal
panel** on X while the autopilot flies, the **speedrun timer** top right under the NeuroNet box in speedrun mode. The prompt bar is contextual by default
(new screens and pending decisions), bottom right, confirm first, at most five glyphs. The **pad cursor is on
everywhere by default** (the owner, 25 Oct: rooms, the bar's characters, menus, dealers, the map), coexisting with
focus navigation on every screen: the left stick shows it and the game reacts as to the mouse (a character lights up
with its talk cursor, a button highlights, a row focuses), it slows over and pulls toward targets, a D-pad or bumper
press hides it and focus carries on from there; A = click either way; Off = focus only. Every other screen (load, save,
pause, death, NeuroNet windows, bar, job board, news, the three dealers) has its focus, prompts and hidden elements in
the mock-up's Screens view and its notes.

### 6.6 New Game and speedrun mode (decisions 17, 18)
NEW GAME opens a short list: NORMAL GAME (default) / ASSISTED (the autopilot ready from the first mission, competent
preset) / SPEEDRUN MODE (the in-game timer and a split per story checkpoint on the HUD, the autopilot's speedrun
preset, LiveSplit fed over TCP); one description line for the highlighted entry, A starts, B backs out; the choice is
written into the save, tagged on the save slot and locked per save. LiveSplit: a tiny TCP client to 127.0.0.1:16834
(`starttimer` on the New Game click, `split` per checkpoint and at the final cutscene, `reset` on the main menu,
`pausegametime` / `unpausegametime` around loads; the named pipe as fallback), plus a stable memory struct for
community autosplitters; rows under AUTOPILOT > SPEEDRUN MODE. The mod never calls its timer official.

### 6.7 Build order (decision 20; proposed)
Foundation (the input-device service, glyph atlas, prompt bar, focus kit, the pad cursor component) -> the flight HUD
(strip, tray, hidden elements, picker, wheel) -> bases (rooms, the bar strip, job board, news) -> dealers -> the nav map
-> the New Game prompt and LiveSplit. Each phase verified in the background game at 4K and 1080p with the virtual pad,
then felt by the owner on the Elite pad and with a real mouse (`docs\TESTING.md`).
