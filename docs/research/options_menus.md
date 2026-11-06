# Options menus: how well-regarded games do it, and what Freelancer's own screen looks like
Status: current (extended 6 Nov 2026 by the simple mode round; the UI build of 25 Oct 2026 realised U10-U14; two departures marked in place: the pause menu got a sixth button after all, the subtitles preview is a replica, not the display's own code)
Checked against: FLvibed 3c9c1390 (6 Nov 2026, short form: 3 watched-path commits: ccf824e6 the spec lists only binds that exist, ddb71c86 [Modules] reads "on" like every row, c4652165 pad fixes; U57-U63 as 4cab3e0f (the options rulings, landed during the round) left them)
Dirty paths at check: another session's uncommitted the private work queue, `tools/uimock/options.py` (not judged); 950b25cc, 4a270d9e and 3c9c1390 landed during the round, read
Watch: docs/UI_PLAN.md, core/settings.h, core/game/menu.*, modules/controller/src/impl/controller*.cpp, modules/subtitles/src/impl/display.*, docs/SETTINGS.md
Evidence: `research\evidence\options_menu\` (the simple mode round: three readers, three verifiers, inventory, mock frames); `research\experience\ui_2026-10-25\` (the test install's Options screen, pause menu and flight HUD at 4K, 25 Oct 2026, and the three mock-ups built over them); the sources below (read 25 Oct 2026)
Sources: [page] gameaccessibilityguidelines.com full list; [page] learn.microsoft.com XAG 104 (subtitles and captions); [page] caniplaythat.com God of War Ragnarok accessibility review, Forza Horizon 5 menu deep dive, Marvel's Spider-Man Remastered deep dive; [page] naughtydog.com TLOU2 accessibility features; [page] bugnet.io "how to design a good options menu"; [page] flyawaysimulation.com / forums.flightsimulator.com on MSFS Assistance presets; [page] pixelpoppers.com / gameaccessibilityguidelines.com on Celeste's Assist Mode; [page] Drexel 2023 thesis abstract on settings-menu UX; [page] gdevelop.io "Borrowing without copying" (settings screens)

## What it means for FLvibed

**Simple mode round (6 Nov 2026, the owner's brief for 0.4.0): design `docs\UI_OPTIONS.md`, the owner's rulings in its section 14.** Five tabs by feature (HUD, CONTROLLER, SUBTITLES, AUTOPILOT, MORE), 30 settings for today's 114 rows,
no scrolling at Large on a Deck, each feature's switch at the head of its tab, off greying what is under it. A hidden row the player changed comes back under YOU CHANGED, so nothing is lost out of sight (U57), and the
description moves into the page at body size (U58). Bundle rows with CUSTOM extend presets to the hints and hit effects (U59). A first-start card covers four access choices once (U63). `pad_backend`, the one hidden default that could harm, is settled by
0.3.5's SDL3 fallback (U61). The evidence is thin where it matters most: all 30 GitHub issues have one author, every usage figure is from another genre, and no player has walked the page: the check pack is U62.

*Check-up 29 Oct 2026 ~07:15 (short form, HEAD 067bf32; 2 watched-path commits 7518008..067bf32):* no effect: no key
or row added; the page's own pad reader now applies `stick_deadzone` (6970588: a player with a larger deadzone got the
default on the page), and `UI_PLAN.md` section 2 names the View hold as the toggle the bar button mirrors (373634a).
*Check-up 29 Oct 2026 (whole set, HEAD 7518008; 31 watched-path commits 77d8655..7518008):* **the bus grew by about 45
keys, the page by one row.** `docs\SETTINGS.md` gained the 28-29 Oct autopilot switches (`roll_guns` .. `big_coast`,
`finish_target`, `headon_*`, `standoff`, `survival_*`, `hold_defend`, `follow_defend`; most on by default) and the
multiplayer keys (`mp_autopilot`, `mp_caution`, `mp_flee`, `pvp_duel` / `pvp_*`), and lost `autopilot_pad` (393bea9: the
CONTROLLER tab's LAYOUT section 5 -> 4 rows, the three controller presets one member less); the only new page row is
`start_smart` (AUTOPILOT tab). By U11's tier rule that is right for the tuning switches (advanced, raw), but
**`mp_autopilot` is a player decision** (what the autopilot may do on a server that says nothing; the owner set its
default on 28 Oct) and appears only under Show advanced, and MULTIPLAYER.md's decision 3 (the player sees the server's
word) has no surface yet - the `fluf_windows` / `borderless` pattern (a status note on the row) fits it (IDEAS U28). The
handoff's "presets.cpp tables unchecked vs ui_meta.h" is the guard of this note's MSFS rule (a preset never silently
writes a row it no longer names) - already on the working session's list. Nothing contradicted.
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
`modules\hud\docs\UI_HUD2.md` 5.3) - the tier rule of U11 holds. One gap grew: while HUD 2 shows, the subtitles' top panel draws in
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

### Simple mode round (6 Nov 2026)
Readers: `research\evidence\options_menu\reader_a_games.md` (about 40 pages), `reader_b_standards.md` (about 50), `reader_c_players.md` (over 40). Each was attacked by a verifier who re-fetched the claims: `verify_a_games.md`,
`verify_b_standards.md`, `verify_c_players.md`. Page inventory: `inventory.tsv`. What survived is table 2 of `docs\UI_OPTIONS.md`; what did not:

| Claim | Fate |
|---|---|
| No game has a global Simple / Advanced switch | **Wrong.** OpenTTD ships Basic as the default with Expert and changed-from-default filters; PCSX2 hides advanced by default; RetroArch has the switch, on by default. None found in a big commercial title |
| TLOU II dropped per-audience menus after feedback | The reason has a primary source (Gallant on NPR: players "didn't fully trust us to configure those options"); the menu history stays second-hand |
| FH5 and GoW put accessibility first | Overstated: the sources' section order, not the menu's; no tab order is cited |
| Subtitles: 60 % is true demand | Overstated: Origins' 60 % reached first-party, Division 2 runs 75 % ever to 50 % now, TLOU II's 63 % sits in a video |
| Sniper Elite 5 text size 35 % | Very likely subtitle size; menu text size is a separate row. Its aim assist reads 8 % and 97 % in two years, so its numbers are not leaned on |
| MWII: blur and FOV over half | A group of settings, no per-setting figure. Dead zone over 40 % stands, second-hand |
| Black Ops 6: accessibility menu nearly half | The sentence names no title; not BO6 data |
| Small text is the loudest complaint | About a dozen voices; Valve's rule carries it |
| Page 1996: 92 % customised | Stands, but like for like with Spool it is 86 % changing preferences |
| Mods commonly have an advanced mode | Only BepInEx builds it in; Fallout 4's MCM example is a sample of a generic switch; Sodium has no Advanced page today and warns by toast |
| Never say "module" | Untested. Foundry VTT and ElvUI use the word; plain names stay a judgement |
| Holds under 3 s are safe | Wrong: that line replaces pinch gestures; no safe hold is published |
| Disabled rows must stay focusable | A design choice: Microsoft's default says otherwise |
| XAG 111 navigation, XAG 28 px | The brief's errors: 111 is audio description, navigation 112, focus 113, context 114; 28 px is GAG's, XAG 101 says 18 / 36 px PC and 26 / 52 px console |

Open: no first-hand screens or frames of other games (text sources only); PCGamingWiki refused every fetch; Reddit unreachable; the Xbox handheld measuring method; Elite Dangerous' tab list and first run.
