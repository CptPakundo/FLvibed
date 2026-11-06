# The FLvibed options menu: simple mode (0.4.0 design, 6 Nov 2026)

**The owner's brief, 6 Nov 2026:** "I want to default to HUD 2, and I also want the FLvibed options menu to be overhauled so that it fits the new module structure, and front loads the most important options. I want a default simple mode that shows basic and important options, with an optional advanced mode toggle that will allow the user more control, but frankly? I prefer to leave this advanced mode for later, as I prefer we first ensure the default simple mode covers the important options, makes sense, is readable, legible and intuitive."

**Status:** the owner ruled on section 14 on 6 Nov 2026, and this text carries those rulings. Two or three real players check the mock before the row list freezes (section 12a). Nothing is built and no game ran. Research: `docs\research\options_menus.md`, section "Simple mode round". Evidence and mock frames: `research\evidence\options_menu\`. Open `mock\index.html` there first.

**Standing of a claim:** *web* = a page fetched and checked by a verifier; *measured* = read from a real frame or computed from the code's sizes; *code* / *doc* = read here; *owner* = the owner's words; *judgement* = this design's call.

## 1. Verdict

**Five tabs, 39 rows, no scrolling, on a Steam Deck at the Large size.** Today a player has 114 rows in 5 tabs, and 61 lines in the one tab that holds the pad, the HUD and the window.

| Tab | Module | Rows | Its head |
|---|---|---|---|
| **HUD** | HUD | 7 | HUD 2: ENABLED / ON THE PAD / DISABLED |
| **CONTROLLER** | Controller | 10, the full tab | Controller |
| **SUBTITLES** | Subtitles | 5, around a live sample | Subtitles |
| **AUTOPILOT** | Autopilot | 7 | Autopilot |
| **MORE** | Movies, Display, Speedrun, NPC, then the page's own rows | 10, the full tab | one head per feature |

That is 30 settings, 6 further feature switches, and 3 rows of the page's own. Four rules carry the rest:

1. **Each feature's switch is the first row of its tab,** and off greys everything under it (the owner, O4). Features without a tab are heads on MORE, their rows under them.
2. **One switch per feature in a player's eyes.** Where a live switch and a module both exist, as for Subtitles and HUD 2, the head row is both: off acts at once, and a restart only ever shows when a feature that was off at the start comes back on (section 6.3).
3. **A setting the player changed never hides.** A hidden row that differs from its default comes back on its tab under YOU CHANGED, until it is at its default again (section 6.5). Rows use plain feature names, not the word "module" (*judgement*: nobody has tested the word, and Foundry VTT uses it).
4. **The description sits in the page,** under the rows, in the same place, at body size. Today it is a small line in the screen's top corner.

A first-start card, one screen with four rows, covers the access choices once on a fresh install (section 5a). The page keeps the game's words: DEFAULT, CANCEL, CLOSE, ENABLED, DISABLED.

**"Default to HUD 2" is already true for new installs.** `hud_style` 2 and `ui_style` 1 have been the defaults since 0.2.0 (ded9aec2, *code*). What 0.4.0 adds is the HUD tab first, with the way back as its first row. It should not raise `settings_version`: that moves every player's settings aside (section 10).

**What the frames still lack:** a player. All 30 GitHub issues come from one reporter on a Steam Deck, and every usage figure comes from another genre. Section 12a is that check.

## 2. What the research says

Three readers, three adversarial verifiers. What survived:

| Finding | Strength | What this design does |
|---|---|---|
| **Presets over rows that stay editable** is the settled pattern: TLOU II, GoW Ragnarok, NMS, MSFS, Doom's HUD presets. An edit reads CUSTOM (*web*) | Strong | Layout and Autopilot style are presets; two new bundle rows work the same way |
| **A global Simple / Advanced switch exists, but not in big commercial games.** OpenTTD ships it with Basic as the default and a changed-from-default filter; PCSX2 hides advanced by default; Fallout 4's MCM and BepInEx have one; RetroArch's is on by default (*web*). The first reader's "no precedent" fell to its verifier | Medium | Simple mode is a sound choice for a mod; its closest model is OpenTTD |
| **Hiding needs a road back.** TLOU II's lead said players "didn't fully trust us to configure those options" (NPR, *web*): they wanted fine control. The story of its dropped per-audience menus stays second-hand | Medium | YOU CHANGED; Show all settings |
| **Defaults decide more than the page.** Where subtitles are on by default nearly everyone keeps them; where off, about 60 % turned them on in Origins, a first-party figure. Others vary: Division 2 went from 75 % ever to 50 % now | Strong in direction, weak on the numbers | Subtitles stay on. Every hidden default was audited (section 5.3) |
| **Measured demand** in other genres: stick dead zone over 40 % (Call of Duty, second-hand), a text size row 35 % of players (Sniper Elite 5, very likely the subtitles' size: its menu text size is a separate row). Blur, FOV and aim assist figures fell | Medium | Stick dead zone and both text rows are on the page |
| **Small text is the best-supported complaint:** about a dozen voices name TV or Deck, and Valve sets a bar of 9 px, 12 px recommended, at 1280x800 (*web*). The bar carries more weight than the voices | Medium, strong for the bar | Sizes in section 8 |
| **Pad rules are published:** bumpers for tabs, single presses, lists that loop, no required hold for a destructive confirm, prompts for the live device (XAG 107, 112, 113, 115; *web*) | Strong | Section 7 |
| **Disabled needs a second signal beyond grey** (XAG 103). Keeping disabled rows focusable is a design choice, not a rule: Microsoft's own default says otherwise (*web*) | Medium | Rows lose their arrows; they stay focusable so the help line can say why |
| **Restart notes on the row** before the change: no well-regarded game was confirmed doing it. It is what players ask for, and Sodium and RetroArch do it | Weak as practice, firm as guidance | Said before the change in the description, and after it on the head row |
| **Search** suits hundreds of settings (MWII, 500+). None found in the six games named. OpenTTD names the filter when a search hits a hidden row | Medium | None on 39 rows; a note for advanced mode |
| **Tab counts** run 5 to 10, mostly 5 to 7, and no source shows fewer is better. No sourced row budget exists | Weak | The budget is measured here: what fits at Large with no scrolling |

Three things the brief expected did not hold: XAG 111 is audio description, not navigation (navigation is 112, focus 113, context 114); the 28 px figure is GAG's, not XAG's (XAG 101: 18 px PC, 26 px console at 1080p); RetroArch is not simple by default.

## 3. Today's page

*Code and frames:* `docs\UI_SPEC.json`, `core\ui\options_page.cpp`, `research\evidence\options_menu\inventory.tsv` (every row, its line, its owner, its verdict; made by `inventory.py` beside it).

| Tab | Sections | Rows (player + advanced) | Lines with headers | Screens at 1280x800 (about 11 lines) |
|---|---|---|---|---|
| GENERAL | MODULES, SETTINGS | 11 + 0 | 13 | 2 |
| CONTROLLER | INPUT, LAYOUT, AIM, STICKS, TARGETING, MENUS AND PROMPTS, HUD, SYSTEM | 53 + 6 | 61 | 6 |
| AUTOPILOT | (none), SPEEDRUN MODE | 20 + 3 | 22 | 2 |
| SUBTITLES | TEXT, BACKGROUND, SPEAKERS, CHANNELS, PLACEMENT, MOTION | 26 + 2 | 32 | 3 |
| MOVIES | (none) | 4 + 0 | 5 | 1 |
| **Total** | | **114 + 11** | **133** | |

Underneath: **291 registry keys** (76 Controller, 165 Autopilot, 42 Subtitles, 8 Modules), of which 170 have no row: they show raw under Show advanced, or only in the ini.

**What a player meets:**
- **The tabs predate the split.** The HUD's 20 rows, the text size and the window rows all sit in CONTROLLER, so a keyboard player looks for HUD size under a tab named for a device they do not own. Speedrun sits in AUTOPILOT.
- **The depth.** From the top of CONTROLLER, by lines: Invert Y 16, Rumble 22, Text size 33, HUD 38, HUD size 41, Borderless 61.
- **GENERAL leads with eight module switches** and a raw Show advanced row: the first thing seen is the mod's plumbing.
- **The help line is small and far.** Arial at 0.0155 H: 14 px at 800, in the screen's top corner, away from the row. At 4K its 33 px is under XAG's PC 36 px (*measured*).
- **Every restart row always says AFTER RESTART,** whether or not anything waits.
- **`game_text_size` has no row,** though the owner's goal of 4 Nov turns on it. Neither do `mp_autopilot` or `hull_cue_pattern`.
- **`edit_bindings` opens nothing:** a toast says it is not built.
- **Reset All acts at once with no question** (`ResetEverything`), against XAG 115.

## 4. The simple page, tab by tab

Wording is proposed in full, because the wording is the design. Every description fits two lines at Large (`options.py helpcheck`). A toggle reads ENABLED or DISABLED, the game's own words, with a box. Values read in capitals, labels and descriptions in sentence case (XAG 101). **Head** marks the feature's switch; section 6.3 says what it writes.

### HUD (the HUD module, with the text sizes)
| Row | Values (default first) | Key | Says |
|---|---|---|---|
| **HUD 2** (head) | ENABLED / ON THE PAD / DISABLED | `hud_style` + `[Modules] hud` | The new flight display and menu look. On the pad: keyboard and mouse keep the game's HUD. Disabled: the game's own HUD. |
| HUD size | AUTOMATIC / SMALL / NORMAL / LARGE | `hud_size`, new (`UI_HUD2_INPUTS.md` 7) | Automatic: Large on a handheld, Normal elsewhere. Small is close to the game's own HUD. This page follows it. |
| Window text size | AUTOMATIC / NORMAL / LARGE / EXTRA LARGE | `game_text_size` (100 / 125 / 150; Automatic new) | The game's text in dealers, job boards and the Neural Net. Automatic follows HUD size. Applies at the main menu. |
| HUD in calm flight | FADES / ALWAYS | `hud_fade` | Fades: parts of the HUD fade out in calm flight and return when they matter. Always: everything stays. |
| Numbers on the ring | WHEN LOW / ALWAYS / NEVER | `hud_numbers` | Your shield and hull as numbers beside the ring round the crosshair: when low, always, or never. |
| Button hints | WHEN USEFUL / ALWAYS / OFF | bundle, 6.2 | Which button does what. When useful: only when something new is possible. If unsure, leave this. |
| Hit effects | FULL / SOFT / OFF | bundle, 6.2 | Flashes at the screen's edge when your ship is hit. Choose Soft or Off if flashes bother you. |

### CONTROLLER (the Controller module)
| Row | Values | Key | Says |
|---|---|---|---|
| **Controller** (head) | ENABLED / DISABLED | `[Modules] controller` | The whole game on a controller: prompts, screens, on-screen keyboard. Takes effect at the next start. |
| Layout | FREELANCER / ACE / ASSISTED / CUSTOM | `pad_preset` | Per value, for example: Freelancer: the game's controls on a pad, the right stick steers. Press A to see every button. |
| Invert steering | DISABLED / ENABLED | `invert_y` | Stick up turns the nose down, as in a flight simulator. |
| Aim | GIMBAL / RETICLE | `aim_mode` | Gimbal: the stick steers and the reticle locks onto your target. Reticle: the stick moves the reticle, with aim assist. |
| Steering response | SOFT / LINEAR / SOFTER / SOFTEST | `steer_expo` | Softer is gentler near the centre, for fine aim. If unsure, leave it on Soft. |
| Stick dead zone | 0-90 %, steps of 2 | `stick_deadzone` | How far a stick moves before the game reacts. Raise it if the ship drifts by itself. |
| Rumble | 0-100 %, steps of 10 | `rumble` | Vibration strength: hits on your ship and warnings. Zero switches it off. |
| Button prompts | AUTOMATIC / XBOX / PLAYSTATION / NINTENDO / STEAM | `glyph_style` | Which button pictures to show. Automatic follows the connected controller. |
| Weapon groups | CYCLE ON Y / ONE BUTTON / TWO TRIGGERS | `weapon_groups` | One button: one fire button. Cycle: Y steps through your groups. Two triggers: a group on each trigger. |
| Held buttons | HOLD / TOGGLE | `holds_as_toggles` | Toggle: boost, reverse and rear view go on with one press and off with the next. Nothing needs holding. |

A on Layout opens the **layout card**, read-only: every button of that layout, LB / RB through the layouts (frame 14). It replaces the row that opens nothing. Button names in any row follow the active family (Y reads as the triangle's name on PlayStation).

### SUBTITLES (the Subtitles module)
The sample stands under the head, always, and changes with the rows (XAG 104; the owner's decision 7 of 25 Oct).

| Row | Values | Key | Says |
|---|---|---|---|
| **Subtitles** (head) | ENABLED / DISABLED | `enabled` + `[Modules] subtitles` | Spoken lines as text, in flight, in bases and in cutscenes. |
| Text size | MEDIUM / SMALL / LARGE / EXTRA LARGE / HUGE | `size` | The size of the subtitles only. The sample above shows it. |
| Background | 0-100 %, steps of 10 | `background_opacity` | How solid the panel behind the text is. More is easier to read over bright scenes. |
| Speaker names | OWN ROW / INLINE / OFF | `speaker_names` | Who is speaking: on a row of its own, in front of the line, or not at all. |
| Radio chatter | CHAT WINDOW / SUBTITLES / OFF | `chatter` | Other pilots' talk outside the story: in the chat window, as subtitles, or off. |

### AUTOPILOT (the Autopilot module)
| Row | Values | Key | Says |
|---|---|---|---|
| **Autopilot** (head) | ENABLED / DISABLED | `[Modules] autopilot` | The Advanced Autopilot, its goal panel and its button. Takes effect at the next start. |
| Autopilot style | COMPETENT / SPEEDRUN / NO CONSUMABLES / CUSTOM | `preset` | Per value, for example: Competent: flies well, and knows only what you have seen. It uses your missiles and repairs when needed. |
| When it starts | OBJECTIVE FIRST / TARGET ONLY | `start_smart` | Objective first: the mission, then your waypoints, then your target. Target only: just the target you picked. |
| Stick takes over | 0-100 %, steps of 5 | `autopilot_cancel` | How far to push the stick to take the ship back. Zero: only the autopilot button does. |
| Skip docking scenes | ENABLED / DISABLED | `skip_dock_scene` | When the autopilot docks, skip the docking scene. |
| Autopilot key | a key | `autopilot_key` | The keyboard key that starts and stops the autopilot. A key the game already uses is refused. |
| Autopilot hints | ENABLED / DISABLED | `ap_hints` | Show the autopilot's prompts before you first use it. Disabled: nothing about it shows until you start it once. |

### MORE (the features without a tab of their own, then the page's rows)
Each feature is a head; its rows stand under it, indented. Off greys those rows alone.

| Row | Values | Key | Says |
|---|---|---|---|
| **Movie handling** (head) | ENABLED / DISABLED | `[Modules] movies` | Movies fitted to the screen, the choice of intro, skipping the startup movies. Takes effect at the next start. |
| Startup movies | PLAY / SKIP | `movie_skip` | Skip goes straight to the main menu. |
| Intro | AS INSTALLED / HD / ORIGINAL 2003 / FULL (BONUS DVD) | `movie_intro` | Which intro plays. Only the versions this install has are offered. |
| **Window handling** (head) | ENABLED / DISABLED | `[Modules] display` | Borderless window, fit to the monitor, running in the background. Takes effect at the next start. |
| Keep running in background | DISABLED / ENABLED | `run_in_background` | The game normally pauses when you switch to another window. |
| **Speedrun mode** (head) | ENABLED / DISABLED | `[Modules] speedrun` | The New Game choice, the timer, the splits and LiveSplit. Takes effect at the next start. |
| **Bug recorder** (head) | ENABLED / DISABLED | `[Modules] npc` | Writes a log when a ship near you stalls, for bug reports. Changes nothing in the game. Takes effect at the next start. |
| Show all settings | DISABLED / ENABLED | `show_advanced` | Every tab also lists the rest of its settings, as they were before this page. Advanced mode replaces this later. |
| Reset | RESET EVERYTHING | | Every FLvibed setting back to its default, hidden ones too. It asks first. |
| About | VERSION AND CREDITS | | FLvibed 0.4.0, its credits and licences, and where the settings files are. |

**Why this grouping.** Modules a player recognises get a tab; the small ones share MORE. Why not one tab per module: eight tabs do not fit, and NPC has nothing to set. Six tabs do not fit either: at Large the five already fill the strip, 0.77 H of it (*measured*, frame 1). HUD first, by the owner's O1: it holds the legibility rows, and they matter on any device.

## 5. Which settings a player truly needs

### 5.1 How rows were ranked
By what the player gains, in order: (1) it stops them playing, reading or holding the pad at all; (2) measured demand elsewhere; (3) a report in this project; (4) an owner rule; (5) taste. Grade A = a real loss without it, B = a real gain for some, C = taste or a rare need, D = tuning. A row earns the page at A, or at B when nothing better wants its place. The full list: `inventory.tsv`.

| Row | Grade | Evidence |
|---|---|---|
| HUD | A | Owner: HUD 2 is the default, so the way back comes first. D8 of the critique: the likeliest resentment |
| HUD size | A | Owner, 5 Nov; issue 10; Valve's bar |
| Window text size | A | Owner, 4 Nov: "very legible and readable in all windows"; Valve's bar |
| Subtitles, Text size, Background, Speaker names | A | XAG 104; the defaults data; a size row used by 35 % in Sniper Elite 5 |
| Layout | A | Decides every button (`UI_PLAN.md` 6.4) |
| Invert steering | A | Issue 6. It blocks some players outright. No telemetry; self-selected polls say 45-48 %, likely high |
| Steering response, Stick dead zone | A | Over 40 % changed the dead zone in Call of Duty (second-hand, a shooter); every pad game offers sensitivity |
| Rumble | A | GAG basic, XAG 110 |
| Aim | A | The owner, O2: gimbal is the mod's signature (`aim_design_preference`) |
| Held buttons | A | XAG 107: motor access. Issue 29 shows it in use |
| Weapon groups | A | Issues 4, 14, 16, 29: the most reported setting |
| Button prompts | A | Issue 19; Steam's virtual pad reads as Xbox, so Automatic can be wrong |
| Hit effects | A | Owner: flash cues have an off switch (`hud_design_principles`); XAG 118 |
| HUD in calm flight | A | The one HUD 2 behaviour a new player can take for a fault |
| Startup movies | A | Judgement: the first thing a returning player wants. No figure found |
| Autopilot style | A | Presets first; owner's product shape (`autopilot_ui_later`) |
| Numbers on the ring | B | The owner, O2 |
| Button hints, Radio chatter, Stick takes over, When it starts, Autopilot key, Skip docking scenes, Autopilot hints | B | Owner: hints must not nag; the game's Controls page cannot list the key |
| Keep running in the background, Intro | B | The only choices in their modules |

**The weight of it.** Every issue here has one author, on a Steam Deck, keen enough to file 30 reports. Mod users likely change more than the figures suggest (Page 1996: 86 % of 101 WordPerfect users changed preferences, heavy users most; Spool's "under 5 %" has no count behind it and is not leaned on here).

### 5.2 What stays out, and why
**Out at the margin, first in line for advanced:** Auto-target and Swap Y (the owner, O2), HUD opacity, In-flight subtitle position, Side chat size, Subtitles in movies, Speedrun timer, Input device, Cursor speed.

**Folded into a row:** Autopilot style holds 11 rows; Button hints 6; Hit effects 3; HUD holds `ui_style`; HUD size takes `text_size`, `hud_scale`, `deck_mode` (`UI_HUD2_INPUTS.md` 7).

**Not a player's:** the 170 keys with no row and the tuning rows. The autopilot's experiments are all off by default.

**Outside this page:** volume, gamma, mouse sensitivity, difficulty and the keyboard map are the game's own pages, one button away in the same column.

### 5.3 Every hidden setting at its default: the audit
Each hidden row was read for one question: can a player who never sees it be harmed? All are safe at their defaults (`inventory.tsv`), with one exception, now settled, and one trap.

- **The exception was `pad_backend`.** XInput is the default, so a DualSense or Switch pad without Steam Input or a mapper was not read, with the fix behind a hidden row. The owner made the SDL3 fallback a must-have of 0.3.5 (O5), so this page adds no row for it.
- **The trap: a changed value out of sight.** `input_mode` locked to the keyboard, say, leaves a pad dead with no visible reason. Rule 2 is the answer.
- **`mp_autopilot`** stays travel-only by default and a server's word wins, so hidden is safe; `docs\research\options_menus.md` still holds it a player's decision (U28).
- **`hud_alerts`, `hud_threats`, `focus_only`, `pause_on_disconnect`, `maneuver_watchdog`, `fire_target`:** on, and safe on.
- **`npc_stall_log`** writes a local log and changes nothing; its row on MORE says so.

## 5a. The first-start card (the owner, O7)

One screen, once, over the main menu, on a fresh install (frame 15). It covers what can stop someone reading, holding the pad or looking at the screen, and nothing else.

**BEFORE YOU FLY** / *Nothing here is required. It is all in Options, FLVIBED, later too.*

| Row | Values | Writes |
|---|---|---|
| HUD size | AUTOMATIC / SMALL / NORMAL / LARGE | `hud_size`; the card follows it |
| Subtitles | MEDIUM / OFF / SMALL / LARGE / EXTRA LARGE / HUGE | the head and `size`; the live sample stands above |
| Hit effects | FULL / SOFT / OFF | the bundle |
| Held buttons | HOLD / TOGGLE | `holds_as_toggles` |

- **Skippable by being closable:** two chips, DEFAULT and CLOSE, B or Esc. Every row applies at once, so closing is the skip and no answer is needed. The defaults already pass every bar.
- **When:** a fresh install only, once. A stamp `first_card = 1` in `[FLvibed]` of `FLvibed.ini` records it. A player upgrading (`FLController.ini` already there) gets the stamp silently and never sees the card.
- **Where:** after the startup movies, at the first main menu. Subtitles are on by default, so the movies before it need no prompt.
- **Which four:** text size (Valve's bar), subtitles (XAG 104), flashes (XAG 118), holds (XAG 107). Volumes are the game's own page. Invert steering stays off the card: it is a preference, and it is on the page.

**Collisions, flagged:**
1. **With the ruling against a HUD 2 / Classic page: none.** The card has no HUD look row, it is not in the setup, and HUD 2 stays the default. The way back stays the first row of the HUD tab.
2. **With `UI_HUD2_PRESETS.md`, yes.** Its Guided preset names a "first-run guided setup choice". When presets ship, that belongs on this card as a fifth row, or goes. There must be no second first-run screen.
3. **With the harness.** A card over the main menu stalls any test that starts fresh. `tools\deploy.ps1` must write the stamp with `settings_version`, and a test of the card clears it.

## 6. How the page behaves

### 6.1 Rows and focus
One list per tab, looping, the head first. Left / right changes the value at once; nothing waits for Apply. On return to a tab the focus is where it was. A dot marks a value away from its default (colour alone was criticised in GoW). The page's own text follows HUD size, so the page is its own preview of that row, and the subtitles' sample is the other.

### 6.2 Presets and bundles
A preset or bundle row is one word over several keys. It applies its table only when chosen; a member that differs reads **CUSTOM**, which stays in the row's cycle, so a preset is always one step away (BG3's one-way Custom is the shape to avoid; the MSFS rule of `options_menus.md` holds). The existing `presets.*` mechanism does this.

| Bundle | Value | `prompt_bar` | `world_prompts` | `hud_context` | `hud_prompts` | `button_legend` | `hud_glyphs` |
|---|---|---|---|---|---|---|---|
| Button hints | WHEN USEFUL (all defaults) | 0 | 0 | 0 | 1 | 0 | 1 |
| | ALWAYS | 1 | 0 | 0 | 0 | 0 | 1 |
| | OFF | 2 | 1 | 1 | 1 | 1 | 0 |

| Bundle | Value | `shield_cue` | `shield_cue_intensity` | `hull_cue` |
|---|---|---|---|---|
| Hit effects | FULL (defaults) | 1 | 1.0 | 1 |
| | SOFT | 1 | 0.5 | 1 |
| | OFF | 0 | 1.0 | 0 |

### 6.3 The switch at the head (the owner, O4: "At each tab's head. Off means everything inside is greyed out, which is intuitive.")

**One switch in a player's eyes.** Two features have both a live switch and a module: Subtitles (`enabled`, `[Modules] subtitles`) and the HUD (`hud_style`, `[Modules] hud`). The head row is both, so the double master goes:

| Head | Off | On, feature running | On, feature off since the start |
|---|---|---|---|
| **Subtitles** | At once: `enabled` 0, and the module row written for the next start | At once: both back; nothing waits | `AT NEXT START`, rows grey until then |
| **HUD 2** | At once: `hud_style` 0, the game's HUD returns and the page goes classic. The module row written; Hit effects and Window text size end at the next start | At once | `AT NEXT START` |
| **Controller, Autopilot, Movie handling, Window handling, Speedrun mode, Bug recorder** | Rows grey at once; the feature runs until the next start. The row says so | Clears a waiting off | `AT NEXT START` |

So a Subtitles or HUD 2 player meets a restart in one case only: turning back on, in a later session, a feature they turned off. **The head reads ENABLED only when both keys are on.** A saved mix reads DISABLED, and the page writes nothing until the player acts; a Classic player of today, with the module on, keeps their hit effects running that way. Keys and ini files stay as they are.

**"Needs a restart" reads on the head row, and nowhere else** (frames 10, 11). Before a change, the description says *Takes effect at the next start*. After it, the value itself says so in amber: **DISABLED AT NEXT START**, **ENABLED AT NEXT START**. The tab's chip carries an amber mark until the game restarts, so it shows from any tab. Going back cancels it, with nothing waiting. Today's permanent AFTER RESTART goes, and the full-width strip of the first draft went with it: on a full tab it cost a row. The page compares the row with what is running (`hostcalls ModuleOn`). There is no restart button: the page cannot start the game again.

**The rule "one of Subtitles and Controller stays on" leaves the player's sight.** FLvibed starts only with one of those two modules on. If a player disables both, the page keeps the Subtitles module loaded with `enabled` 0: nothing shows, every gate in `display.cpp` reads `enabled` (*code*), and the head still reads DISABLED. No refusal, no explanation.

**Controller asks first** (frame 12), the only switch that does. It can lock a pad-only player out of this very page, on a Deck above all: **DISABLE THE CONTROLLER?** / *From the next start the game is keyboard and mouse only.* / DISABLE IT, KEEP IT ON, the safe answer focused.

### 6.4 What off looks like
| State | The head | The rows under it |
|---|---|---|
| Off | DISABLED, box empty | Values dimmed, arrows and boxes gone (the second signal, XAG 103), still focusable. Help: *Subtitles are disabled, so this has no effect. Your setting is kept.* A or Enter moves the focus to the head |
| Going off, waiting | DISABLED AT NEXT START | Dimmed; help: *It keeps working until you restart.* |
| Coming on, waiting | ENABLED AT NEXT START | Dimmed: their owner is not running, so they cannot change (`e.def` null) |
| Stopped by a fault | STOPPED, no box. Help: *It stopped after a fault. Restart the game.* | Dimmed |

On SUBTITLES the sample reads *No subtitles*. On MORE a head greys its own rows alone. A tab never disappears, and no row hides by hardware (Valve advises against it; a pad can arrive later).

### 6.5 YOU CHANGED: nothing lost out of sight
A hidden row away from its default appears at the end of its tab, under the section label YOU CHANGED, with its dot, editable, until it is back at its default (frame 8). Its home follows its owner: the tab its module has. So a player from 0.3.x sees only the rows they changed, and a row they set still does what they set. Hidden values stay in force throughout.

The keys with no row cannot be listed this way. MORE counts them, as one line above Reset, only when the count is not zero: **Other changed settings: 3 ... RESET THEM**.

*Precedent:* OpenTTD's changed-from-default filter. *Cost:* at Large such a tab may scroll, and only for that player. *Caveat:* a default changed in a later release makes the old saved value read as changed, since the ini holds every key. It is honest, if noisy.

### 6.6 DEFAULT, CANCEL, CLOSE (the owner, O8: the game's words, and the game's meaning)
| Chip | Pad | Keys | Does |
|---|---|---|---|
| DEFAULT | X | D | This tab's rows, head and bundles and YOU CHANGED rows included, back to default, at once |
| CANCEL | Y | C | Every change since the page opened, on every tab, then closes (today's, `RevertAll` then `CloseAs`) |
| CLOSE | B | Esc | Closes, keeps, writes the ini |
| RESET EVERYTHING (MORE) | A | Enter | Asks once (frame 13): **RESET EVERYTHING?** / *Every FLvibed setting goes back to its default, hidden ones too.* / RESET EVERYTHING, KEEP MY SETTINGS. A plain press, the safe answer focused (XAG 115). Feature switches return to on. CANCEL still reverses it until the page closes |

DEFAULT has no dialog: the game's has none, and CANCEL takes it back. The description says what each does while its chip has the pointer.

**Where this meets the skin's verbs.** The owner ruled verbs in dialogs (`UI_HUD2_SKIN.md` 13.1) and the game's words on this page. They agree, and the line between them is deliberate: **a page keeps the game's nouns, because GENERAL, CONTROLS and the rest sit beside it and use them; a dialog that asks a question answers in verbs.** So DEFAULT, CANCEL, CLOSE on the plate; RESET EVERYTHING / KEEP MY SETTINGS and DISABLE IT / KEEP IT ON in its two dialogs. The same applies to ENABLED and DISABLED, which this page now uses for every toggle, as the game's rows do: an extension of O8 to its stated aim.

## 7. Pad and keyboard and mouse

| Action | Pad | Keyboard | Mouse |
|---|---|---|---|
| Next / previous row | D-pad or left stick, loops | Up / Down, loops | Hover moves the focus |
| Change the value | Left / right; held repeats | Left / Right | Click an arrow or the value |
| Toggle, open | A | Enter | Click |
| Tab | LB / RB, loops | Q / E | Click |
| DEFAULT, CANCEL, CLOSE | X, Y, B | D, C, Esc | Click the chip |

- **Single presses do everything** (XAG 107): sliders step on a press, no analogue input is needed, no hold is required.
- **Held direction:** 0.4 s, then 8 steps a second, tuned in game. No published number exists.
- **The chips are the prompts.** Each carries its button, and they are also what a mouse clicks, so the page needs no second lane. Glyphs show for the live device only.
- **The pad's list stays on the rows:** X, Y and B reach the chips, so nobody walks past a page of rows to a button.
- The game's Options column stays where it is; CLOSE returns to it.
- From the pause menu the same page opens over the scene (frame 7). The autopilot flies on under it, as under every window.

## 8. Legibility

Sizes are `UI_HUD2_SKIN.md` 4's, in fractions of H, multiplied by HUD size. 1280x800 is drawn at Large (text x 1.2), which Automatic picks on a Deck; 4K at Normal. Metrics: Agency FB Bold, from the font (*measured*). Verifier's warning: code px is em, the guidelines' px is body height, and Valve gives no method.

| Text | Size | 1280x800 Large: em / x-height / caps | 4K Normal: em / x-height / caps |
|---|---|---|---|
| Row label | 0.032 H | 30 / 15 / 23 | 69 / 34 / 53 |
| Value, description | 0.030 H | 28 / 14 / 21 | 64 / 32 / 49 |
| Section label (capitals) | 0.024 H | 23 / - / 18 | 51 / - / 39 |

- **Valve's 9 px floor and 12 px aim:** passed on the smallest character by any reading. XAG PC 36 px at 4K and console 26 / 52: passed by the rows and descriptions. Section labels sit under console's 52 at 4K: acceptable for a PC title's secondary text (*judgement*).
- **A forced Normal at 800** (the player's choice): description x-height 12.
- **Rows** 0.046 H, pitch 0.050 H at Normal; 0.053 H, pitch 0.057 H = 46 px at Large.
- **The budget (measured):** at Large, ten rows fit between the tabs and a two-line description. CONTROLLER and MORE are full. SUBTITLES holds 5 around its sample.
- **What ten rows cost** (O2's two rows and O4's heads took the count from 8 to 10): the title FLVIBED OPTIONS leaves the hosted page, where the lit FLVIBED button in the game's column names it, and stays on the pause menu's version; row pitch at Large falls from 48 to 46 px, under Microsoft's 47 px, a television figure and no rule; the bottom chips lose 0.005 H. Text sizes do not change. Any further row on CONTROLLER or MORE costs another, and YOU CHANGED or Show all settings scroll there.
- **Contrast** against the plate, computed: INK 15.8:1, CHROME (descriptions) 10.1:1, unavailable DIM 4.4:1, AMBER 11.7:1, active yellow on the focus fill 6.9:1. XAG: 4.5:1 text, 3:1 inactive. The plate is opaque, so these hold over any room. The fill itself is 2.3:1 from the plate, so the yellow word and the bar carry the focus (`UI_HUD2_CRITIQUE.md` R9).
- **4:3, 21:9:** the panel is the game's. Not mocked; the page must pass the display matrix before it counts.
- **Width at Large:** labels stay under about 24 characters beside a picker. An amber DISABLED AT NEXT START fits the head at 0.028 H.

## 9. Show all settings, and the place kept for advanced mode

Advanced mode is not designed here. The owner leaves it for later, and keeps **Show all settings** until then (O3): a row of MORE, DISABLED by default. Enabled, each tab also lists the rest of its rows, as they are today, under the tab of their owner: the 54 player rows with no row or bundle of their own, and the raw keys as Show advanced lists them now. The list scrolls. It is today's page re-homed, with no new design.

What simple mode already gives advanced mode: the tier in the metadata, every row on a tab by its owner, YOU CHANGED, the description band. Notes for that round: search belongs there (with OpenTTD's hint when a result is hidden); a sub-level needs a default pad binding.

## 10. HUD 2, the skin, and old settings

**The skin.** Page item 10 of `UI_HUD2_SKIN.md` 5 (Options, plates). This design uses section 4's primitives as they stand: plate, header 0.040 H, tab chips with the yellow underline, row, section label, chip, one focus style, scrim, dialog with the amber edge. "Tooltip, pad: a fixed help line" becomes the description band for every device. Dialogs take the skin's verbs by 6.2; the page keeps the game's words (section 6.6). The FLVIBED page button and column follow the skin's chips.

**With HUD 2 disabled** the same page, same tabs and rows, draws in the game's look at once (`views::SkinOn()` false), as today. The rule "off means classic" holds.

**`hud_size` comes first.** It is P11f's (critique section 9 step 5). This page's HUD tab and its own scaling wait for it, and `UI_HUD2_INPUTS.md` 7's migration of `deck_mode` applies.

**Old settings stay.** No `settings_version` raise: settings lost after a patch or a restart is a common complaint, more so than restarts themselves. Keys, sections and defaults stay where they are (ARCHITECTURE "Nothing player-facing moves"); only the page's grouping changes.

**The default.** Nothing to change in code. A player who set Classic keeps it, and reads DISABLED at the head.

## 11. What the build takes

No build order is fixed here; it follows the owner's decisions and P11f's `hud_size`.

1. `UI_SPEC.json`: new tabs, a `simple` tier, per-value help, the words above; `tools\ui_meta.py`; the living spec in the same package.
2. `options_page.cpp`: the band, the chips with X / Y, tabs by owner, head rows and their two-key logic, the amber value and the tab mark, YOU CHANGED, the indented rows of MORE, the layout card (from the spec's binding tables), the two dialogs, the first-start card and its stamp, Q / E and D / C. `tools\deploy.ps1` writes the stamp.
3. `presets.*`: two bundles. `RowOwner` for the new rows.
4. Tests: `tests\` for bundles and the changed-row rule (pure); in game at 1280x800 and 4K, pad and K&M, hosted and from the pause menu; each feature off, waiting, coming on, stopped; both Subtitles and Controller disabled, and that the game still starts; that a feature switch still holds after the restart (the common complaint is a value lost there); `hud2 lint`; the display matrix. Fails closed: with the spec's rows missing, the page shows them greyed, as today.

## 12. Mock frames

`research\evidence\options_menu\mock\index.html`; `tools\uimock\options.py` draws them. **Real:** the scene, the logo, where the game's panel and buttons stand. **Drawn:** the plate and all on it. 1280x800 at Large, 4K at Normal.

| Frame | Shows |
|---|---|
| 1-5 | The five tabs, both sizes |
| 6 | Keyboard and mouse |
| 7 | From the pause menu, over the scene, with the title |
| 8 | Changed rows, YOU CHANGED |
| 9 | Subtitles disabled: off at once, the tab grey |
| 10 | Autopilot disabled, waiting for the next start |
| 11 | MORE, a feature coming on at the next start |
| 12 | DISABLE THE CONTROLLER? |
| 13 | RESET EVERYTHING? |
| 14 | The layout card |
| 15 | The first-start card |

**Limits:** the frames differ from a build in the fonts' hinting and the glyphs (Xbox only). No 4:3 or 21:9. The classic look is not drawn.

## 12a. The player check (the owner, O9)

Two or three real players look at the mock before the row list freezes. **The pack:** `research\evidence\options_menu\flvibed_options_check.zip`, 1.2 MB, one page and nine frames, about ten minutes, nothing that needs the game (`options.py pack`). It opens with six "which tab would you open" tasks on the tab names alone, then asks what rows and descriptions confused them, what is missing, what the greyed and waiting states mean, what DEFAULT, CANCEL and CLOSE do, and who they are.

**What would change the design:** a task answered wrongly by two of three moves a row; a description misread by anyone is rewritten; a state read wrongly goes to its wording first. The owner picks the players. The issues' reporter is one, and a player new to FLvibed is worth more than a second keen one.

## 13. How far the evidence goes

- **No player has used this.** Reader C's advice stands: a few first-time players walking the page would test it better than any of this.
- **Figures** are from other genres and years, and several are second-hand; the verifier's table is `verify_c_players.md`. Sniper Elite 5's aim assist reads 8 % one year and 97 % the next, so its numbers are not leaned on.
- **All first-hand screens are missing:** menus were read through reviews, wikis and guides, mostly from launch. No frames of other games were studied (the owner's standing wish): Reader A's sources are text.
- **Doom Eternal and Elite Dangerous are thin.** PCGamingWiki refused every fetch. Elite's tab list and first run stay open.
- **Reddit could not be reached,** so complaints come from Steam, ResetEra and articles, a few dozen voices in all. Restart without warning is one voice; a warned restart that then loses the value is the commoner report. An accidental reset-all: nothing found.
- **Unread:** the Xbox handheld measuring method, the CVAA and EAA primary texts (neither seems to reach a mod's page; not legal advice).
- **Handheld detection** is unverified on real hardware (`UI_HUD2_INPUTS.md` 7).

## 14. The owner's decisions (6 Nov 2026)

| # | Ruling | Where |
|---|---|---|
| O1 | Five tabs, HUD first | 4 |
| O2 | The row list, with **Aim and Numbers on the ring back**; Auto-target and Swap Y out. Fit it without scrolling at Large and say the cost | 4, 8 |
| O3 | Keep Show all settings | 9 |
| O4 | **"At each tab's head. Off means everything inside is greyed out, which is intuitive."** The sub-page is gone | 6.3, 6.4 |
| O5 | The SDL3 fallback is a must-have of 0.3.5, done there, not on this page | 5.3 |
| O6 | Yes, the two bundles | 6.2 |
| O7 | A short first-start card, one screen, skippable; HUD 2 stays the default | 5a |
| O8 | The game's words: DEFAULT, CANCEL, CLOSE | 6.6 |
| O9 | Two or three real players check the mock before the row list freezes | 12a |

**Left to confirm:** O8 was asked together with sentence-case labels, and the ruling names only the words. Labels stay in sentence case here (XAG 101); the game's rows are in capitals. ENABLED / DISABLED were applied to every toggle on the same ruling. The Guided row on the card waits for the presets (5a).

