# Options menus: how well-regarded games do it, and what Freelancer's own screen looks like
Status: current (the UI build of 25 Oct 2026 realised U10-U14; two departures marked in place: the pause menu got a sixth button after all, the subtitles preview is a replica, not the display's own code)
Checked against: FLvibed 77d8655 (26 Oct 2026, coordinator short form before the export: the watched commits since - d89f88c / 79e2bcc smart start + K&M goal panel, 7632ced job objective text, 77d8655 objective-named loot shot, 9ff7fe7 handoff - are autopilot / UI internals; no statement of this note changes; not re-read in depth); before: FLvibed e5c02cc (26 Oct 2026, step 3 short form: the fluf_windows / borderless / chat_all_dialogue rows - first "What it means" line); before: FLvibed 3fa62f0 (25 Oct 2026 ~23:05, whole-set check-up at 855745b, plus 3fa62f0 that landed during the round (`docs\SETTINGS.md` regenerated: three cue rows' defaults read from their initialisers - a generator fix, nothing here): 18 watched-path commits 073a665..855745b - ui_style and glyph_look rows, the page opaque / text_size / fit rule, the subtitles' placement fixes; the U12 gap stands - first paragraph); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: 6 watched-path commits abf84ac..073a665 in `controller/src/impl/controller.cpp` (the 11 HUD rows in kOptions, 2541e58), `subtitles/src/impl/display.*` (the HUD bus, the comm feed, f8dc584 / f5f07fe / 15f5ffc / c2a27eb) and `docs/SETTINGS.md` (1ae14b0); nothing contradicted - the HUD section follows the tier rule, the preview replica's gap grew (first paragraph)); before: abf84ac (25 Oct 2026 ~07:00, short form after the UI build: 26 watched-path commits 4059e56..abf84ac - the design b2a8870 / ec018b6, the owner rounds, the PAGE track 001d858 / 7747ea0 / 22b4b0a, HOST dd5bce6 / 52b6871, INT-PAGE 6200900, INT-HOST 28dd009 / a6f8e2f, VERIFY, FIX 4+5 a83e06c, FIX 8 f1daccc, the spec patch 2c9e5d4; read in `controller\src\impl\options_page.cpp`, `presets.*`, `controller\src\host\uibtn.cpp`, `subtitles\src\impl\movie.cpp`); before: 4059e56 (25 Oct 2026 00:40, the note's creation)
Dirty paths at check: none at the start (25 Oct 2026 ~23:05; during the round another session's uncommitted `controller/src/impl/padbases.cpp`, `controller/src/impl/promptbar.cpp`, `docs/UI_HUD2_EXTENDED.md` appeared - not judged); none (25 Oct 2026 ~12:15; also at ~07:00)
Watch: docs/UI_PLAN.md, core/settings.h, core/game/menu.*, controller/src/impl/controller.cpp, subtitles/src/impl/display.*, docs/SETTINGS.md
Evidence: `research\experience\ui_2026-10-25\` (the test install's Options screen, pause menu and flight HUD at 4K, 25 Oct 2026, and the three mock-ups built over them); the sources below (read 25 Oct 2026)
Sources: [page] gameaccessibilityguidelines.com full list; [page] learn.microsoft.com XAG 104 (subtitles and captions); [page] caniplaythat.com God of War Ragnarok accessibility review, Forza Horizon 5 menu deep dive, Marvel's Spider-Man Remastered deep dive; [page] naughtydog.com TLOU2 accessibility features; [page] bugnet.io "how to design a good options menu"; [page] flyawaysimulation.com / forums.flightsimulator.com on MSFS Assistance presets; [page] pixelpoppers.com / gameaccessibilityguidelines.com on Celeste's Assist Mode; [page] Drexel 2023 thesis abstract on settings-menu UX; [page] gdevelop.io "Borrowing without copying" (settings screens)

## What it means for FLvibed

*Check-up 26 Oct 2026 (step 3 short form, HEAD e5c02cc; 12 watched-path commits 3fa62f0..e5c02cc):* three rows changed
by the same rules (registry row + spec row + `SETTINGS.md`): `fluf_windows` (af2adb1, default 2, with a status note on the
row: NO FLUF INSTALLED / FLUF DETECTED / ... CO-RESIDENCY ONLY - the "explain what it adapts" rule), `borderless`
(90a8b7f, 0 / 1 auto / 2, the row says BY FLVIBED or which provider; help cut to the tooltip's three lines), and
`chat_all_dialogue` reworded for the side chat (915be52). The tier / preset rules stand; U12's gap stands. Nothing
contradicted.
**Built (check-up 25 Oct 2026, HEAD abf84ac; `docs\UI_BUILD.md` 5):** the FLVIBED page button sits in `NN_Preferences`'
free slot in both menus with a host-side page switch (dd5bce6; `PrefsInitElements` / `PrefsMessage` / `PrefsShowPage` in
`flexe.h`), the page draws five tabs in the game's row grammar over the settings bus with instant apply, DEFAULT /
CANCEL, rollover help from the registry, presets with CUSTOM, Show advanced listing the unnamed bus rows raw, and the
MOVIES tab (U10-U14; 7747ea0, 22b4b0a, 6200900, 001d858). Two things went otherwise than this note said: the **pause menu
got a sixth button** (dd5bce6; the note judged F1 -> OPTIONS -> FLVIBED enough - the owner wanted the shortcut), and the
page button's **native caption never drew** (the game does not draw a TextControl added to NN_Preferences; the logic
draws FLVIBED over the button's rectangle, 52b6871 / 6200900). The subtitles preview is drawn by the page itself
(see the bullet). Verified in the release build across the display matrix (a83e06c: the 4:3 "narrower box" was the
game's own page-switch slide caught mid-way); native row controls (ToggleButtonControl / ScrollControl) stay replicas.
*Check-up 25 Oct 2026 (073a665), HUD 2:* the CONTROLLER tab gained a HUD section of 11 rows (`hud_style` CLASSIC / HUD 2
ON THE PAD / HUD 2 ALWAYS, `hud_fade`, `hud_opacity`, `hud_scale`, `hud_numbers`, `hud_threats`, `hud_context`,
`hud_alerts`, `hud_info`, `hud_wireframes` as player rows, `hud_transition` as an advanced row; `core\ui_meta.h`,
`docs\UI_HUD2.md` 5.3) - the tier rule of U11 holds. One gap grew: while HUD 2 shows, the subtitles' top panel draws in
Agency FB and the chatter goes to HUD 2's comm feed at HUD 2's sizes (`display.cpp DrawHud2Feed`), which the SUBTITLES
tab's replica preview does not show (`options_page.cpp DrawPreview` knows nothing of HUD 2) - more weight for the
preview through the display's own code (U12).
*Check-up 25 Oct 2026 (855745b):* the page grew with Extended HUD 2 - `ui_style` (Screens: CLASSIC / HUD 2) at the top of
the CONTROLLER tab's MENUS AND PROMPTS section (adf77f1) and `glyph_look` under INPUT (c5d0191 / a885411), both player
rows (U11's tiers hold); the page is opaque in both styles, standalone and hosted (17bc818: a bar character's name label
showed through), follows `text_size` 80-150 % (3abdd81) and its texts take the one fit rule with the legibility floor
(4e9e1f6); Extended HUD 2's strip hides under it (e8b1e15). The U12 gap stands: the preview is still the page's replica
(`options_page.cpp DrawPreview`), now further from the display, which gained the comm portrait's live placement, the
bottom-lane keep-out and the alert yield (ab00529, 014f39a, 1bcb0a3) and the game font resolved per build (1e81c4a; the
preview uses the same `gamefont` faces).
- **Put the mod's options where players look first: inside the game's own Options screen.** Freelancer's `NN_Preferences`
  has a column of page buttons with a **free slot in both contexts** (main menu: under CREDITS; in game: under AUDIO), and
  the right-hand page is a plain list of rows (label, slider / checkbox / picker) with a one-line rollover help at the top
  of the screen, DEFAULT and CANCEL at the bottom, no OK / Apply (changes apply at once). The mod's page copies exactly
  that grammar (IDEAS U10; design `docs\UI_PLAN.md`). The F1 pause menu needs no sixth button: F1 -> OPTIONS -> FLVIBED
  works in space and in bases. *[Built dd5bce6 / 6200900 (25 Oct 2026) - with a sixth pause-menu button after all (the
  owner's call; it opens the page standalone as an overlay panel), and the page caption drawn by the logic because the
  game never draws a TextControl added to NN_Preferences (`uibtn.cpp kNativeOptionsCaption`).]*
- **Presets first, then individual rows, "Custom" when a row deviates** (MSFS Assistance, Forza's driving-assists
  preset, TLOU2 / GoW presets). The autopilot gets a preset row at the top of its tab; a changed row flips it to CUSTOM.
  The MSFS lesson: a preset must never silently overwrite an individual change made after it (preset first, rows last).
  *[Built 25 Oct 2026 (`options_page.cpp`, `presets.*`): the autopilot `preset` and the controller `pad_preset` rows apply
  their table only when chosen and read CUSTOM when a member deviates - the MSFS rule holds by construction.]*
- **Every row has an explanation** (GoW: "explainers for each option", Outer Worlds' subtitle descriptions cited by XAG
  104; the Drexel study: universal-design menus test better). Freelancer already has the mechanism: the rollover help
  line. The registry's help strings are written in that voice; they become the rollover text.
- **Mark changed values with more than colour** (GoW's reviewer: "blue text is the only indicator"): a dot before the
  label, and the DEFAULT button per page (the game's own).
- **Subtitles need a live preview** (XAG 104: "a visual simulation of what subtitle and caption presentation options
  will look like", Forza Horizon 4/5 and Spider-Man do it): a sample line drawn in the page while the Subtitles tab is
  open, through the display's own code (`display::Show` / the `test` command) so it is exact.
  *[Built differently (25 Oct 2026, `options_page.cpp DrawPreview`): the SUBTITLES tab shows a PREVIEW band, but the page
  draws the sample itself from the bus values (the size table, the game font or Segoe UI, Juni's colour from
  `speaker_colors.tsv`) - a replica, not the display's own code; exactness is by keeping the two in step. The
  `speaker_colors` key exists (U12). Routing the preview through the display (XAG 104's "simulation") stays the exact
  form, a refinement item.]*
- **Channels toggle independently** (XAG 104: dialogue types on / off separately; Outer Worlds): story / comms / bar /
  Neural Net / chatter rows, as the module already has.
- **Assist options are wording, not shame** (Celeste: "we recommend playing without it first"; GAG "assist modes",
  "fine-tuning variables", "change difficulty mid-game"): the autopilot tab reads as assistance (hand-back, caution,
  consumables), reachable mid-game, never framed as cheating. GAG also asks for remappable controls, haptics on / off,
  sensitivity, separate volumes and saved settings: the controller tab's rows cover the first three.
- **Don't overwhelm** (GoW: 70 options "needed a break"; GAG: simpler alternative): the player tier only; the ~95 dev
  keys stay in the ini and behind "Show advanced" (U11). *[Built 25 Oct 2026: the spec's rows carry a tier
  (`core\ui_meta.h` from `UI_SPEC.json`, b097bab); `show_advanced` lists every bus row the spec does not name, raw
  (label = key, help from the registry).]*
- **Both menus, one screen, instant apply, countdown-revert only for risky display changes** (bugnet): FLvibed has no
  display-mode change; module switches need a restart and say so in the row ("after restart").

## Findings

### Freelancer's Options screen (measured 25 Oct 2026, HDE 0.7.1, 4K 16:9; facts in `FREELANCER_RE.md` "Options screen")
- Main menu: FREELANCER logo top left, a column of mesh buttons GENERAL / CONTROLS / PERFORMANCE / AUDIO / CREDITS
  (pitch 238 px at 4K) and MAIN MENU at the bottom; **one empty slot** between CREDITS and MAIN MENU. In game (F1 ->
  OPTIONS): GENERAL / CONTROLS / PERFORMANCE / AUDIO (pitch 216 px), RETURN TO GAME at the bottom, a close X top right;
  **two empty slots**. The selected page's caption is yellow, the rest white.
- The page: title at the top centre ("GENERAL OPTIONS"), section headers (DISPLAY / CONTROL / MISCELLANEOUS) with
  indented rows: a label on the left, the control on the right - a slider with a value caption to its left (0.30,
  Average, 120), a checkbox with ENABLED / DISABLED text, or a picker box with up / down arrows (Resolution). The
  Controls page has three sub-tab buttons across the top (SHIP CONTROLS / USER INTERFACE / MULTIPLAYER) over a
  PRIMARY / SECONDARY table. Rollover help = one line of small text at the top right of the screen ("Activates cruise
  engines."). Bottom: DEFAULT (this page) and CANCEL. No OK or Apply: values apply as they change; CANCEL reverts.
- The pause menu in space: RESUME GAME / LOAD GAME / OPTIONS / QUIT TO MAIN MENU (Save only when docked); QUIT asks
  YES / NO in a nested frame.

### Comparables
| Game / source | What it does well | Take |
|---|---|---|
| God of War Ragnarök (caniplaythat review) | 70+ settings; an explainer per option, preview images for comparisons; four presets (vision / hearing / motion / motor) each "some" or "full"; changed rows in pale blue | explainers + preview; presets then rows; **mark changes with a dot too** (colour alone was criticised); the preset screen without previews was criticised; "needed a break" after setting up: keep the player tier short |
| The Last of Us Part II (Naughty Dog) | 60+ settings; three presets (vision / hearing / motor) that set recommended values, tweakable afterwards; subtitle names, direction, size, background | presets as entry points; subtitle rows match ours (names, size, background) |
| Forza Horizon 5 (caniplaythat) | Difficulty section with a driving-assists preset above the individual assists (braking, steering, traction, shifting, line, damage, rewind); subtitle size and background opacity sliders; text size to 150 %; vibration scale; 16 default pad layouts | the autopilot tab = "assists": preset + rows; subtitle preview on a button (FH4, cited by XAG 104) |
| Marvel's Spider-Man Remastered (caniplaythat) | subtitle background and speaker colours assignable, sizes, "a preview to let you see what to expect in-game" | speaker colours as a setting (new key); live preview |
| Microsoft Flight Simulator Assistance (guides, forum) | presets Easy / Medium / Hard / True to Life over many switches; "Custom" when any switch differs; the piloting assists grouped apart from the aircraft's own autopilot | preset + Custom; a known gotcha: choosing a preset after a tweak overwrites it - apply presets first |
| Celeste Assist Mode | five plain switches (speed 50-100 %, stamina, dashes, dash assist, invincibility), an honest intro ("we recommend playing without it the first time"), default "no" | tone for the autopilot tab; one row per assist, no bundles that hide what they do |
| Game Accessibility Guidelines (full list) | remappable controls, sensitivity, haptics toggle, separate volumes, settings saved, profiles (advanced), assist modes, fine-tuning variables, difficulty changeable mid-game, subtitle customisation, subtitles on before any sound | the checklist the tabs are read against |
| XAG 104 (Microsoft) | subtitles on by default or settable before sound; speaker identified (colour never alone); channels toggle independently with plain descriptions; size scalable to 200 %; solid background with 0-100 % opacity; **a visual simulation of the current settings**, in a realistic context if possible | the Subtitles tab's rows and its preview line |
| bugnet.io (generic) | tabs Display / Graphics / Audio / Controls / Accessibility / Gameplay; apply instantly; test cue for audio sliders; countdown-revert for risky changes; say "restart required" before the change; reset per tab and globally; reachable from main and pause menu; controller-navigable; readable at couch distance | matches Freelancer's own DEFAULT / CANCEL / instant-apply grammar; "after restart" wording for module switches |
| gdevelop.io analysis | on / off states must be labelled, not inferred; a button that toggles must look different from one that opens a screen; proximity = meaning | Freelancer's ENABLED / DISABLED text beside the checkbox already does this; keep it |
| Drexel 2023 thesis (abstract) | menus built on the 7 principles of universal design tested better in A/B against a commercial game | explanations, consistent controls, low effort |

### What the Freelancer community did (from the 24 Oct checkup, unchanged)
Nobody added a page to `NN_Preferences`; HDE re-enables two hidden `optlist.ini` sliders by exe patch, FLSharp hooks
the class at ~20 sites for the resolution list, FLUF draws its own ImGui options window and once hooked the main menu's
Options button, adoxa adds single native controls by byte patch. The free page-button slot is unused by all of them.
