# Adaptive UI research: keyboard/mouse and controller in one game (17 Oct 2026)

How modern games build a UI that serves keyboard/mouse **and** controller without losing function, intuitive and
fast with both, switching live with the last-used device; accessibility is the emphasis. Input for
`..\TASK_GLYPHS_HUD.md` (the controller UI task) and `..\AUTOPILOT_UX.md` (the autopilot goal UI).

**User premise (17 Oct 2026):** glyph swaps are not enough; Freelancer's UI is mouse-led, so good controller
support needs new or heavily modified UI/UX built for the pad. The research below supports this strongly
(section 2.1).

## 1. Method and limits
- **Text sources** (about 30): platform guidelines (Xbox Accessibility Guidelines, Steam Deck Verified), developer
  posts (Paradox, Larian via PlayStation Blog, Ubisoft Mainz, Blizzard), Epic's CommonUI docs, articles, forum
  threads (marked as anecdotal where used).
- **Video** (12, downloaded by the user, studied without playback: subtitles as text, frames extracted with
  ffmpeg): 7 talks (GDC: The Division UI (Ubisoft Massive), God of War Ragnarök UI (Santa Monica); The Last of Us
  Part II accessibility (Naughty Dog); SpecialEffect motor accessibility; Game Maker's Toolkit motor disabilities and
  controllers; Accessibility at Ubisoft) and 5 gameplay videos (Diablo II: Resurrected alpha controller UI, Baldur's
  Gate 3 controller UI, Crusader Kings III console, Anno 1800 console radial menus, ConsolePort for World of
  Warcraft). Every frame was change-scored at the native frame rate; UI events were then sampled at 12 fps up to
  every frame (tooling: session scratchpad scripts uiflow.ps1 / frames.ps1, not in the repo).
- **Limits:** subtitles are YouTube auto-captions (wording approximate); the D2R footage is a 2021 alpha; game UIs
  change after release. Not reachable: the Game UI Database (bot check), members-only GDC Vault talks (e.g. the
  Cyberpunk 2077 UI talk), Ubisoft's own UI write-ups for Assassin's Creed / Far Cry / The Division input switching
  (none found in text).

## 2. Findings

### 2.1 Controller mode is a different UI, not the same UI with glyphs
- **Diablo II: Resurrected** (a mouse-led classic, the closest precedent to FLvibed). Pressing a pad button mid-game
  swaps the whole HUD in **one frame** (measured: 38.00 → 38.08 s at 25 fps, no transition): the PC bar (2 mouse
  skills, a row of menu buttons, 4 unlabeled belt slots) becomes a controller bar with **6 skill slots, each labeled
  with its button** (X □ △ ○ R1 R2), an **L2 indicator** for the second skill set (12 skills in all), belt slots
  marked with **D-pad directions**, a run toggle with the stick glyph, the menu buttons removed and mana always
  labeled. A keyboard press swaps it back. The menus are redesigned too: one tabbed window (Quests, Character,
  Inventory, Skill Tree, Options) switched with the shoulder buttons, a second tab row inside (L2/R2), a ring cursor
  that snaps to slots, a tooltip on the focused item, a **context prompt bar that changes with the focused item**
  ("□ Bind Skill  ○ Close Menu"), inline combo glyphs under bound skills ("L2 + X"), a setting that spaces dropped
  items wider for the pad. Blizzard's design post: mouse play is an "eye in the sky" with pathfinding; on the pad the
  stick steers directly, pathfinding is off, targets are chosen by a cone scan ranked per class and only the primary
  target is highlighted (showing all options overloaded players). Reviewers name the inventory, which kept a cursor,
  as the weakest part of the pad UI.
- **Baldur's Gate 3:** controller mode removes the dense PC hotbar entirely. What remains: party portraits with an
  LT glyph, the minimap, **world-anchored prompts next to the object** ("A Open", "A Use"), an "Actions RB" corner
  hint; actions live in radial menus (a light trigger pull peeks, a full pull locks). Map and radial screens have a
  bottom prompt bar (Place Marker, Fast Travel, Legend, Zoom, Centre, Close); the radial shows the focused entry's
  description in its centre. Larian chose radials because the pad has "12 buttons", and offers an optional precision
  cursor mode. BG3 switches **by an Input Mode setting** (Controller only / Keyboard only / Automatic) plus a separate
  glyph style; the video's author found Automatic unreliable.
- **Crusader Kings III console:** Paradox rejected the virtual mouse outright. Permanent bottom prompt bar
  (Navigation, Select, "Close / (Hold) Close All", Hints), a top command bar of tabs on the bumpers, panels docked
  beside the map instead of covering it ("switching focus" between open panels), a **fixed reticle at screen centre
  with the map moving under it** (instead of a cursor moving over the map), "(Hold) Advanced Tooltips", a character
  radial and a map-mode radial.
- **Anno 1800 console (Ubisoft Mainz):** radials replaced lists ("slow and tiring" on the pad), categories as L1/R1
  tabs under the wheel, focused entry details in the centre, the same fixed world reticle, a wide cursor where one is
  needed, trade route setup split into step-by-step screens (which also improved PC).
- **ConsolePort (WoW add-on, a mod precedent):** replaces the action bars with four-button clusters per modifier
  layer (L1 / L2 held), a calibration wizard ("press the button for X"), a controller-diagram binding page, an
  interface cursor that snaps between clickable elements (D-pad), a "raid cursor" that snaps between unit frames,
  ring menus for utility items, a smart "interact" button. Its author's advice to players: think of the game as a
  console game (one menu button opens everything).
- **God of War Ragnarök:** consistent navigation grammar everywhere (one sidebar style; deeper levels slide right,
  back slides left), prompts repositioned with menu depth, menu tabs on L2/R2, character switch on L1/R1,
  "bring the information to where the player is" (quest markers on the realm gate instead of a trip to the pause
  menu; found only by watching playtest video: "they will just suffer in silence").

**So:** the strong pad UIs keep the game logic and data identical and replace the *interaction surface*: a
pad-specific HUD (what's bound where, shown where the thumbs are), tabbed and radial menus instead of free pointing,
fixed reticles instead of cursors, contextual prompts. A cursor survives only for precision tasks, and there it is the
weak spot.

### 2.2 Device switching
- **Last input wins, per device timestamps**, with noise floors (stick drift deadzone, minimum mouse travel) and game
  time rather than frame delta (Game Developer, "The 5 Golden Rules of Input"). Every device always works; no mode
  toggle needed. In pad mode the cursor is hidden **and hover is disabled** (a parked cursor must not highlight
  things). Disconnecting the active pad pauses gameplay (in menus just switch).
- **Glyphs of an inactive device never show** (Steam Deck Verified); text entry must work with the pad (on-screen
  keyboard).
- **Swap is instant** (D2R: one frame). D2R also briefly pauses the game at the first switch.
- **Flicker is the known failure:** Division 2, Cyberpunk 2077, Mass Effect LE, Overwatch 2, Clair Obscur users report
  prompts flipping without input (Steam Input / multiple HID interfaces, gyro or drift seen as mouse). The standard
  fix is a **manual lock** ("UI Button Prompt Display" in Overwatch 2; Elden Ring, Path of Exile 2, BG3 offer manual
  choice). FLvibed must not count its own injected mouse/keys as keyboard/mouse input.
- Focus and hover are separate states; a mouse click must not leave a stale pad focus (engine forum pitfalls).
- The Division: an action resolves its glyph automatically for console, PC, and PC with a controller.

### 2.3 Navigation and structure (pad)
- Focus-based navigation; the focused element always clearly visible (XAG 113), digital input alone must reach
  everything (XAG 112: no analog precision required).
- **Consistent grammar:** LB/RB tabs, LT/RT pages, A select, B back on every screen (XAG 112; Figma article by an Epic
  designer); a persistent way back; linear lists wrap, grids don't; the same input that moves focus in moves it out.
- **Start focus where most things are near** (not top-left), count presses per common task, global shortcuts that
  work regardless of focus (Figma).
- **Prompt bar** at the bottom with the actions valid now (CK3, BG3, D2R, GoW, Anno): it is how players learn a pad UI.
- More than one way to find content (tabs plus filter or search).
- **Radials** (BG3, Anno, CK3, Star Citizen, X4 7.0, Everspace 2): equal distance to every entry, muscle memory with
  fixed slots; risks: content that changes with context slows snap decisions, organising them is a chore (BG3 player
  complaints), too many entries. Patterns: **sticky selection** (the choice survives the stick returning to centre;
  Anno, players call it essential), an **entry limit** setting, details in the centre, category tabs on the bumpers.
  Measured opening (Anno): about **180 ms** from first frame to settled, prompts and tabs swap at the midpoint, the
  world reticle hides at the same time.
- **Dense screens are restructured:** step-by-step flows (Anno trade routes), two simultaneous lists when the user
  must see both sides (GoW enchantments: items left, slots right), panels beside the world instead of over it (CK3),
  items appearing near their category to cut travel (Disciples).
- Hold vs tap: hold-to-confirm only for destructive actions (No Man's Sky is criticised for using it everywhere);
  offer two taps instead of holds (Anno).

### 2.4 Accessibility (emphasis)
- **Input effort tier list** (SpecialEffect): single press / single stick = best; one hold or a double press = a big
  step down; two buttons at once = worse; two sticks = worse still; several holds plus sticks = worst. So **chords
  such as LB + button need an alternative** (a latched layer or a single-button route). XAG 107: every UI operable
  with single, non-simultaneous presses.
- **Full remapping** is the most requested accessibility feature (GoW: it forced actions-not-buttons through the
  whole game); remaps update every prompt and tutorial (XAG 107); remap the menu inputs too; always a reset to
  defaults that cannot be remapped away (SpecialEffect).
- **Menus with one stick and one button** (EA FC option: navigate everything with the left stick, no bumpers needed);
  "press any button" instead of "press start".
- **Holds → toggles, mashing → hold**, adjustable deadzones and sensitivity (tremor vs weak muscles), rumble
  adjustable and never the only channel, pause allowed.
- **Assist modes are accessibility:** Mario Kart auto-accelerate + smart steering, Red Dead auto-travel to the
  waypoint, Uncharted camera assist and lock-on, Sea of Thieves single-stick mode (camera recentres, prompts at eye
  level), The Last of Us II navigation assist (a button turns you toward the objective), a "what's my status?"
  readout, combat-accessibility tweaks (very popular). Uncharted 4's goal: playable with one hand. FLvibed's
  autopilot is exactly this kind of feature.
- **Settings UX:** each setting with a one-line explanation and ideally a clip (Ghost Recon, TLOU II), changed
  values highlighted (GoW: blue), the last-touched setting stays focused, a safe test space, settings duplicated into
  every category where players look for them (GoW), grouped by logical subcategory, **not by disability**
  (TLOU II: players did not trust the condition filter), **presets** as a one-button start, with fine tuning
  afterwards (GoW, TLOU II), a first-run choice "Quick start / Guided setup" (GoW).
- **Text:** a body-font minimum (GoW: never below 24 pt at 4K; Steam Deck: 9 px at 1280x800, 12 recommended),
  **text scaling** (GoW: about 60 % of players changed the size), which needs layouts that adapt to their content;
  text and icon size and HUD backgrounds as options (Ubisoft AC Valhalla); colour never the only cue; colour-blind
  test filters in the UI pipeline (The Division).
- Tutorials: always in the same place, one thing at a time, in-menu tutorials at most 7 steps (GoW).
- "Nothing about us without us": playtest with disabled players early (TLOU II, Ubisoft, SpecialEffect).

## 3. What this means for FLvibed (summary; details in TASK_GLYPHS_HUD.md)
1. Pad mode gets its **own HUD layer** (D2R / BG3 model): what is bound where, shown where the thumbs are; the
   mouse-only HUD parts hidden in pad mode. Keyboard/mouse keeps Freelancer's HUD untouched.
2. **Instant, per-device-timestamp switching** with noise floors, ignoring FLvibed's own injected input, plus a
   **manual lock** setting (Automatic / Controller / Keyboard-mouse).
3. **No cursor on the pad** except as an opt-in precision tool; maps and lists by focus, fixed reticles and pickers.
4. **One grammar:** A select, B back, LB/RB tabs, LT/RT pages, X/Y contextual, Start menu, a bottom prompt bar on every
   pad screen, focus always visible.
5. **Accessibility from the start:** latched LB layer and single-press alternatives for every chord, full pad
   remapping with prompts following it, holds ↔ toggles, deadzones, text scaling, presets, the autopilot as the
   assist mode.
6. **Restructure dense screens** (nav map, contact list, dealers, inventory) rather than port them.

## 4. Sources
Text:
- Game Developer, The 5 Golden Rules of Input: https://www.gamedeveloper.com/design/the-5-golden-rules-of-input
- Steamworks, Steam Deck compatibility review: https://partner.steamgames.com/doc/steamdeck/compat
- Xbox Accessibility Guidelines 107 / 112 / 113: https://devdocs.xbox.com/gaming/accessibility/xbox-accessibility-guidelines/107 ,
  https://devdocs.xbox.com/gaming/accessibility/xbox-accessibility-guidelines/112 ,
  https://dotnet.territoriali.olinfo.it/en-us/gaming/accessibility/xbox-accessibility-guidelines/113
- Paradox, CK3 console dev diary 3 (UI/UX and controls): https://www.paradoxinteractive.com/zh-CN/games/crusader-kings-iii/news/ck3-console-dev-diary-3-uiux-and-controls
- PlayStation Blog, Baldur's Gate 3 on DualSense: https://blog.playstation.com/2023/09/05/how-baldurs-gate-3-adapts-its-expansive-rpg-gameplay-for-your-dualsense-controller/
- PlayStation Blog, Disciples: Liberation UI: https://blog.playstation.com/?p=354901
- Ubisoft, Anno 1800 Console Edition accessibility spotlight: https://news.ubisoft.com/en-us/article/6xDLQxuay1FASfTGQyquJl/anno-1800-console-edition-accessibility-spotlight
- Ubisoft, AC Valhalla accessibility: https://news.ubisoft.com/en-us/article/5Rys9UeXZh9bAjnHBZN4pC/assassins-creed-valhalla-bringing-accessibility-to-a-viking-epic
- Diablo II: Resurrected, crafting the controller experience (Blizzard design director, reprint): http://www.gamebanshee.com/aywyw ;
  tech alpha controller UI: https://blizzplanet.substack.com/p/diablo-ii-resurrected-tech-alpha-controller-ui
- Figma blog, Press Start (controller navigation): https://www.figma.com/blog/press-start-video-game-navigation/
- Epic, CommonUI input fundamentals: https://dev.epicgames.com/documentation/unreal-engine/input-fundamentals-for-commonui-in-unreal-engine
- ConsolePort: https://github.com/seblindfors/ConsolePort , https://www.wowinterface.com/downloads/info23536-ConsolePort-GameControllerAddon.html
- IGDA GA-SIG guidelines: https://igda-gasig.org/get-involved/sig-initiatives/resources-for-game-developers/sig-guidelines/
- Game Accessibility Guidelines: https://gameaccessibilityguidelines.com/
- Prompt flicker reports (anecdotal): https://us.forums.blizzard.com/en/overwatch/t/simultaneous-mouse-controller-input-causing-lag/997558 ,
  https://steamcommunity.com/app/1903340/discussions/0/592896433808527526 , https://groups.google.com/g/quadstick/c/97Rf26l3_mQ ,
  https://steamcommunity.com/app/1091500/discussions/7/3875968032475488117
- Focus vs hover pitfalls (anecdotal): https://forum.godotengine.org/t/whats-the-deal-with-focusmode-and-mouse-clicks/62702 , https://itch.io/post/4753768
- No Man's Sky interface critique: https://www.escapistmagazine.com/no-mans-interface/
- Can I Play That, Flight Simulator on console (d-pad vs cursor menus): https://caniplaythat.com/2021/07/26/how-microsoft-flight-simulator-and-accessibility-feels-on-console/

Video (studied without playback; files in `FLvibed\research\videos\ui\`, git-ignored):
- GDC 2017, Lessons Learned Creating UI for The Division: https://www.youtube.com/watch?v=H1MLtML0np0
- GDC 2023, God of War Ragnarök: Building the UI for a AAA Sequel: https://www.youtube.com/watch?v=C5gfkeFXDq8
- The Accessibility in The Last of Us Part II: A 3 Year Journey: https://www.youtube.com/watch?v=5HDdino-umA
- SpecialEffect, Integrating Motor Accessibility Into Your Game: https://www.youtube.com/watch?v=tZs_-59LFg0
- Game Maker's Toolkit, Making Games Better for Players with Motor Disabilities: https://www.youtube.com/watch?v=Ufe0i26DGiA
- Game Maker's Toolkit, Controllers Control Everything: https://www.youtube.com/watch?v=VJGKDyrR8qc
- Accessibility at Ubisoft: Where We Are Today: https://www.youtube.com/watch?v=4KKLpN4R8uM
- Diablo 2 Resurrected Alpha: Controller (Teleport, Targeting, UI): https://www.youtube.com/watch?v=ouzh0yu5T2M
- Overview of User Interface & Controls for Xbox Controller, Baldur's Gate 3: https://www.youtube.com/watch?v=VXEg_RvDmqU
- Crusader Kings III Console quick beginner's menu and mechanic tutorial: https://www.youtube.com/watch?v=tEA8jg0hPaA
- Anno 1800 Console Edition, How To Make The Radial Menu Work With You: https://www.youtube.com/watch?v=5Pbw8JyZ10Y
- Setting Up ConsolePort, a World of Warcraft UI replacement: https://www.youtube.com/watch?v=a5iX1U6ZKbk
