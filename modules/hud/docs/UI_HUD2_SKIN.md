# HUD 2 as a complete skin of the game (P11f design, 5 Nov 2026)

**Status:** design for the owner's reading; the build's record is `UI_HUD2_BUILD_040.md`: code on `build-040`, none of it seen in game. Game facts are from the docs, the install's data and decompiled code unless marked; section 12 lists the probes that must pass before the build leans on them. Research: `docs\research\hud-skin.md`, evidence `research\evidence\hud2_skin\` (readers, verifiers, facts, frames).

**The owner, 5 Nov 2026:** "HUD 2 should actually be a complete skin replace for the game when enabled, so that should extend to things like the navmap, quit UI etc." This overrides the "Left native (decided)" list of `UI_HUD2_EXTENDED.md` (9.10, 10): the main menu, the loading screen, the nav map's drawing, the welcome texts. With HUD 2 off the game stays exactly classic. The 3D scene and rooms are no skin matter; every 2D screen, window, dialog, overlay and transition is. The tester's "not fully integrated" (the game's HUD showing briefly at every screen load) belongs here: nothing native may flash, at any load, room change, menu or mission moment.

**6 Nov 2026:** this design now carries the critique's five changes (`UI_HUD2_CRITIQUE.md` 1) and the owner's rulings on D1-D9 (its section 7). It is release **0.4.0**: "total, complete and global, a real makeover possibly". 0.3.5 comes first. Nothing is built: the build waits for the owner's go, after 0.3.5. The owner's rulings on the sheets (R4-R17, the reticle's bottom; the critique's section 7, third table) are written in below: 4, 6.2-6.5, 6.9, 15, 16.

## 1. Verdict
- **The five changes:** effort follows the player, so flight comes first and the base screens' text gets a step of its own (section 15). The weapon cue sits above the ring on dark cells, its channels split (`UI_HUD2_GROUP_CUE.md` 0). HUD 2's own look reaches every surface and the cursor, while the game keeps its logo and its word, NEURAL NET. The pad map's inputs close by two zones, and the grown map is high risk, so frame, words and strip ship before it (6.1). The sheets now match what would be built.
- **It can be done, screen by screen, over the game's own logic.** Of the 62 screens and overlays of section 5, 17 are finished today, 41 get work (some are skinned in part; 8 of them were not found or traced, and the seam audit or a probe must reach them first), and 4 stay native on purpose (section 11). The logo counts with the 40 by the owner's rule, if a route exists.
- **The nav map is the big change.** The code says its frame comes off, and that the map itself can grow from 0.49 H to about 0.78 H by writing two fields, with clicks following (unproven until probe 2). Its labels keep their size while the map grows, so it declutters and reads better at both sizes. Frames: `navmap_4k_pad`, `navmap_1280x800_pad`.
- **Nothing is re-implemented.** No Freelancer mod has restyled a native screen in place. The community swaps art, patches coordinates or draws on top; FLUF's map, over 4,000 lines of ImGui, removes the native draw and replaces the window. The skin stays what it is: live code strips native art while the game's logic stays in charge, the closest model being ElvUI, itself a hybrid.
- **The flash is caught** (12a row 10): the whole native HUD for one frame, the first of space, on a load: an ordering fix on the first UI pass. And the native NeuroNet box under every window in space. The rule is a registry, one row per root (3.1).
- **Legibility decides, not looks.** In the best-documented broad re-skin, EVE's Photon, density, transparency and readability drew far more complaints than mixed styling (rough keyword count by author: 66, 35, 34 against 11, of 352). So glass stays near opaque and measured, and the skin adds no padding that costs a row (section 4). Primitives still come first, for coverage: a generic rule for any window of a known class, then each screen; a window the skin does not know is logged, never silently left (section 3.3).

## 2. What the research says (verified findings; corrections in `docs\research\hud-skin.md`)
| Finding | What it means here |
|---|---|
| EVE's Photon: density, transparency, readability and moved controls outweighed seams from the start (rough counts, opinions); CCP's answer to the seams was a re-skin of base components | Measured contrast, no lost rows; primitives first (section 4); a control moves only for legibility or room |
| New glass under existing controls: NN/g on Apple's Liquid Glass found translucent controls over busy images lower contrast | Plates at 85 % or more; 4.5:1 against the worst background, read from frames |
| "Complete" never meant everything | Section 11 says plainly what stays |
| ElvUI: shared handlers, a master switch, per-frame toggles, each skin under its own guard | One master (`ui_style`), one troubleshooting list (`skin_native`), each family under a guard |
| Fallback: mods mostly warn; returning to native is this project's stricter rule | Seam audit (3.3), window-level fallback (8) |
| An instant toggle depends on reversibility: remember every write, restore on off, restore only your own value | `uiskin.cpp` does all three; every window stays alive and built |
| Maps: free cursor on the canvas, focus in the panels; a non-cursor route to every target | Section 6.1 |
| Triggers zoom, bumpers change layer or tab | Section 6.1 |
| Declutter by priority: the icon survives, the label drops, positions stay stable | The game's own declutter stays; the larger map does the rest |
| Risky confirms focus the safe answer, routine ones proceed; verbs with a speed bump; B is the safe answer; XAG 115: no button hold to confirm a destructive action | Section 6.2 |
| No looped animation under about 1 s (NN/g), a delay before a bar shows (Android) | Section 6.4 |
| Motion: 150-200 ms in, 100-150 ms out, input never blocked (the numbers are judgement inside published ranges); reduced motion means fades for Apple, removal for Windows and NN/g | Section 6.7 |
| Text floors: Valve 9 / 12 px at 1280x800; XAG console 26 px at 1080p (0.024 H) | HUD 2's floor of 0.024 H stands; Deck text at 120 |

## 3. The engine
Four layers in `modules\hud\src`, on `core\ui\winplates.*`. Every write remembered, every restore only while the value is still FLvibed's, as `uiskin.cpp` does today.

### 3.1 The gate: one registry row per root (rewritten 6 Nov 2026 from 12a; evidence `research\evidence\hud2_gate\probes\results_front.md`)
Caught in game, native art reaches the screen by:
1. **The first UI pass of space** runs before the first hide: the whole native HUD for one frame, on a load into space. The tester's flash. An ordering fix: the hide must run on that very pass.
2. **A keep rule shows a root with its art:** in space and under the death dialog the NeuroNet box draws the whole time a window is open, and ~600 ms into the close. `nnbox` applies docked only. A kept root must still lose its art.
3. **The letterbox restores on purpose** (`uiskin.cpp:747`): not excluded; the probes could not read under scenes but at launch.

The 2 Hz search and the 3-frame debounce were not what the tester saw; a load shows no 3-frame or 500 ms stand.

**The rule: every root has a row: hide / keep / keep the root, hide its art / its home.** A true deny would blank what is wanted (`UI_HUD2_INPUTS.md` 3: brackets, arrows, the banner, waypoints, `NN_Scanner`, group control, briefing, the cruise countdown). **An unknown root stays visible and is logged.** Hides run at the UI pass's entry, from the raw signal, on the very first pass. Windows clear bit 1 only: bit 0 stops a window's update (`UI_HUD2_INPUTS.md` 4). The alerts go only after each has its home: `TrailAlert` is the only tail warning until the ring mark stands.

**Fall to native does not exist today** (one `__except`, the whole module): build it per window, retry at the next open, since windows rebuild anyway; keep the strip independent of families; say so in one lane line, or an ornate window reads as a new bug.

Moments to prove, every frame grabbed (`grab`) with the audit of 3.3 at zero: launch, dock, a load from each of the three load paths, jump, room change, conversation start and end, cutscene start and end, pause open and close, each NeuroNet window open and close, death, mission failure, a mission popup, a device Reset, Alt-Tab, classic to skin and back.

### 3.2 Primitives
Section 4. New in `core\ui\hud2_kit` only what two modules draw; the rest in `modules\hud\src\skinkit.*`.

### 3.3 Families and the seam audit
`kFamilies` stays the table. Three additions:
- **Class rules for what has no family.** The front end is data (`.page` files, parser 0x592230): a `UIWinObject` art child hides, a front text button loses its art child and takes a chip, whatever page it stands on. So a page a mod added or a screen not listed still gets the language. Key on class and name, never on geometry: mods change `.page`, `.cmp`, fonts (`05_frontend_facts.md`).
- **Tier per window:**
  - **Art** (today's): art off, glass under, native content on top.
  - **Layout** (`windowtext`'s): native controls moved and sized through signed setters, plates follow.
  - **Shadow** (new, only where the two above cannot reach): the skin draws the surface from the game's own state; the native control stays built and hit-testable with its drawn part off (bit 1), and clicks stay the game's. Used for dialog captions, tooltips, the map's side panels.
- **The seam audit:** `uiskin seams` walks the control list and names every shown control whose art is neither hidden nor claimed. In game, the first sight of an unknown window logs one line (`skin: native window <name> vt 0x...`). Tests fail on a non-empty list. This is what makes "complete" a checked claim and not an impression. **It cannot rest on `uigen`:** key capture and jettison do not move it (12a row 12). Class writes differ: a static image takes bit 1 or `+0x84`, a `.page` UIWinObject bit 1, a front text button its own `+0x84` (bit 1 kills its caption and `hit`).

### 3.4 Ownership
All of it is the HUD module. `module_lint --enforce` holds. Hooks: `uiskin` runs on the existing under-UI pass; the loading screen has no UI pass (0x578F70 skips 0x59E300), so it needs a draw point of its own (section 6.4): one new signed symbol, a hook in the HUD module's first host part if nothing else needs it. The pad's screens (`padmap`, `padmenus`) stay the controller's: they read the skin's published rectangles through a view, as the prompt lane does. Off module = classic.

## 4. One language
HUD 2's as built: navy glass, light-blue hairline, chamfers top-left and bottom-right, Agency FB, active yellow, sizes in fractions of H.

| Primitive | Rule |
|---|---|
| Plate | `GLASS` 85 % over rooms, 92 % for a dialog, rim `CHROME` 70 %; sized to its content, never empty |
| Header | title 0.040 H INK, optional small-caps tag `CHROME` 0.024 H beside it, 0.0012 H rule |
| Section label | small caps `CHROME` 0.024 H |
| Body | 0.030-0.032 H INK; a long text wraps, then scrolls, never cut |
| Row | one line 0.046 H min; name left, value right; one fixed size per opening (issue 22: a name never steps down by itself); a text too long for FLvibed's own rows takes the marquee; native rows have none |
| Chip (button) | 0.046-0.066 H tall, word 0.030 H; a word, not an icon, wherever room allows (no ambiguous symbols) |
| Tab | a chip, the current one yellow with an underline; pad: bumpers |
| Focus | today's: active yellow fill at 0.32 on the drawn element, 0.005 H bar outside its left edge; one per screen. **The focused word in INK** (the owner, R9): `+0x348` reaches `PauseButtonText`, `ItemName` and the job rows, and the chips the skin draws are free; box buttons and rich text keep today's. Only the word changes: an icon on the row keeps its colour |
| Scroll bar | 0.005 H track, thumb from the native range (the control stays for clicks) |
| Scrim | 70 % navy for a modal, 78 % for the full map: above the published 30-60 %, set by the owner's frames over bright rooms (`UI_HUD2_EXTENDED.md` 10) |
| Dialog | a centred plate (the owner, R10), amber top edge when it can lose progress; title, cost in one line, two chips |
| Tooltip | plate, body at 0.026 H, beside its control, inside the safe area; pad: a fixed help line |
| Lane | as built, `UI_HUD2_EXTENDED.md` 13 |

**The grammar (the owner, 6 Nov 2026, R12): what carries HUD 2's flavour onto a new screen.** A screen nobody has drawn yet follows these before it follows anything else.

| Rule | Means | Where it already stands | Carried to |
|---|---|---|---|
| **Pips for any quantity** | a count or a share is a row of segments, never a bare bar | ring, target bars, escorts | menus: reputation, cargo, the route |
| **The accent edge faces the eye** | a plate's coloured edge is on the side nearest the reticle or the reading start, and says what kind of thing it is | target card, alert cards | the selection card; a dialog keeps its centred plate's top edge (R10) |
| **Event, cost, answer** | what happened, what it costs, what to do, in that order | alert cards | every dialog, every notice |
| **Verbs** | a button says what it does: QUIT ANYWAY, not YES | the dialogs of 6.2 | every button the skin words |
| **Mustard focus** | one focus per screen: the block, the bar outside its left edge, the word in INK | every list and menu | every chip |
| **The HOLD ring** | a held action shows its hold as a ring filling round the glyph | prompts | any hold, with a tap beside it where the action is a purchase or a loss (R16, 6.9) |

**Floors and sizes (the owner, 6 Nov 2026, D7).** One row, `hud_size`: AUTOMATIC, SMALL, NORMAL, LARGE, on the HUD / Display tab (`UI_HUD2_INPUTS.md` 7). **Floor 0.024 H on Normal, 0.021 H only on Small, with Small keeping Normal's text at 1280x800.** Large is today's Deck mode: text 120, margins 0.0125 W. Sizes in this document are Normal's. `deck_mode` retires into the row, a saved value migrating. Valve's 9 / 12 px counts the smallest character, so lower case in a thin font is measured from frames, not from this number: the lint moves to rendered ink. Glyph letters count as text: a 0.032 H glyph's letter is about 0.023 H, so the skin's lane uses 0.034 H where the row allows.

**Density and contrast.** The skin adds no padding that costs a row: a native list keeps its pitch and its row count. Text over glass meets 4.5:1 against the worst background behind it, read from frames over the brightest rooms; 3:1 for large text and inactive items.

**In flight, size is earned** (the owner, 5 Nov 2026: "take care not to occlude screen space too much in the flight UI ... only as big as they need to be, and only as big as their importance"). This language's chips and plates are for screens the player stops to read. Nothing here adds to the flight HUD's size: anything new in flight starts at the floor, ranks itself against what stands near it, and says what it covers.

**Rules kept:** nothing in a panel overlaps (`panel.overlap`); panels side by side share a top line, each ends at its content; room first, shortening last; game text by role through `windowtext`.

## 5. Every screen
Tier: **done** = skinned today; **A** art, **L** layout, **S** shadow. Risk: L low, M medium, H high. Standing of each row's facts: `05_frontend_facts.md`, `07_navmap_facts.md`, `research\evidence\controller_ui\hud2x_inventory.md`.

| # | Screen | Built how | Today | Becomes | Risk |
|---|---|---|---|---|---|
| **Front end** | | | | | |
| 1 | Startup splash (startupscreen) | 0x5B1300, before the menu | native | stays, section 11 | - |
| 2 | Intro movies | Movie.dll | Movies module | content, stays | - |
| 3 | First load bar | 0x579180 / 0x579290 | native | S, 6.4 | M |
| 4 | Main menu | static 0x67B780, buttons vt 0x5E2844 | plates | A + chips on the game's places, CONTINUE on top, 6.3 | L |
| 5 | Logo | outside the control tree, drawn before 0x59E300 | native | **stays the game's** (the owner, D1) | - |
| 6 | Version text | TextControl | native | recolour, keep place | L |
| 7 | New Game prompt | FLvibed's | HUD 2 | done | - |
| 8 | Load game (front) | page, vt 0x5E2A84 | plates, native buttons | A: buttons to chips | L |
| 9 | Delete confirm | NN_Confirm | focus only | dialog, 6.2 | L |
| 10 | Options, 5 pages + FLVIBED | NN_Preferences | plates | done; A for its scroll bars where safe | L |
| 11 | Key capture overlay (Controls) | not traced | native | probe, then dialog | M |
| 12 | Credits | PAGE_Credits, in-game page | native | A | L |
| 13 | Exit dialog | modal 0x471810 | native ornate | dialog, 6.2 | L |
| 14 | MP prompt LAN / Internet | modal 0x4740F0 | native | dialog | L |
| 15 | Account select | PAGE_AccountSelect | native | page rule | L |
| 16 | Server list | PAGE_ServerSelect, table vt 0x5E511C | native | page rule, 6.5 | M |
| 17 | Set Filter dialog | not found | native | seam audit finds it | M |
| 18 | Character select | PAGE_CharacterSelect | native | page rule | L |
| 19 | Character create | PAGE_CharacterCreate, UIWinDeform | native | page rule, the 3D stays | M |
| 20 | Ship create, Tutorial pages | pages, maybe unused | native | page rule covers them | L |
| 21 | NN_Error, NN_Prompt, PromptFrame | modal loops | native | dialog family | L |
| 22 | Network operation, blocking dialog | 0x473AD0.. | native | dialog family | L |
| 23 | "Freelancer Encountered an Error" | 0x4725B0 | native | stays: a failing game shows the plainest box | - |
| **Loading** | | | | | |
| 24 | Front-end load | 0x578F70, no UI pass | native | S, 6.4 | M |
| 25 | In-game load | not traced whether same | native | same, delay rule | M |
| 26 | MP connect | not traced | native | same or dialog | M |
| **Base** | | | | | |
| 27 | Room bar, NeuroNet box | NavBar, NN_MainButton | plates | done | - |
| 28 | Character card | ToolTip | HUD 2 | done | - |
| 29 | Offer, job board, news | named art | plates | **skinned, text open** (the critique's R1: stem 1 px at 800) | M |
| 30 | Commodity, equipment dealers | NN_Dealer + NN_Inventory, the transfer panel inline | plates, text by role | **skinned, text open** | M |
| 31 | Ship dealer | NN_ShipTrader | plates | **skinned, text open**; rows at pitch 0.065 do not take a rung | M |
| 32 | Ship purchase confirm | not captured | native | dialog | L |
| 33 | Repair | NN_Repair | plate; art stays per 11 | A on the art, probe | L |
| 34 | Rumour / talk text | offer family | plates | done | - |
| **NeuroNet** | | | | | |
| 35 | Nav map, system | NN_NavMap | focus only | frame, words, strip first; grown after, 6.1 | **H** |
| 36 | Nav map, universe | same | focus only | same | **H** |
| 37 | Map legend, base list | modes 4, 7 | native | right panel | M |
| 38 | Mission briefing | mode 6 | native | same frame, objectives left, Continue a chip | M |
| 39 | Abort mission confirm | 0x49F2E0 | native | dialog | L |
| 40 | Mission log | NeuroNetStoryStar | plate | done | - |
| 41 | Player info, Info (the game's Help) | | plates | **skinned, text open**; the plate to its content | M |
| 42 | Inventory | | plate | **skinned, text open** | M |
| 43 | Jettison | an inline panel of NN_Inventory, not a dialog (12a row 12) | native | A | L |
| 43a | The log's inner tabs, `NN_Scanner` | | native | **no row before the critique:** art tier, or keep | M |
| **Modal** | | | | | |
| 44 | Pause docked, in space | NN_Pause | plates | done | - |
| 45 | Save, Load in game | NN_LoadSave | plates | done | - |
| 46 | OSK | FLvibed's | HUD 2 | done | - |
| 47 | Quit confirm | NN_Confirm | focus only, ornate box | dialog, 6.2 | L |
| 48 | Death, mission failed | NeuroNetStoryStar | plate | done; focus on retry, 6.6 | L |
| 49 | MP death | `RespawnButtonButton`.. | native | dialog | L |
| 50 | Mission popup | vt 0x5D24F4 | plate, native title and OK frame | A: chip, title | L |
| 51 | Group invite | NN_GroupControl | native | dialog | L |
| 52 | Player trade (MP) | strings only | native | seam audit | M |
| 53 | Disconnect (`DCCancelButton`) | 0x48A4C0 | native | dialog | L |
| **Overlays** | | | | | |
| 54 | Chat field, chat window | CTID, MsgWin | HUD 2 feed; native field | A on the field | L |
| 55 | ToolTip | vt 0x5D5F7C | native Arial, but the card | S, 6.6 | L |
| 56 | Rollover | vt 0x5D5454 | native Arial | S | L |
| 57 | Arrival text | NN_ArrivalText | HUD 2 place line | done | - |
| 58 | Map popups | NavMap buttons | native | right panel | M |
| 59 | Cursor | software mesh | native | HUD 2's set of 19, 6.6 | M |
| 60 | Letterbox | engine | native | stays: a scene | - |
| 61 | Notices | "GAME SAVED" is a save's description, no notice; Mission Accepted not reached | unknown | seam audit | L |
| 62 | Flight HUD roots | hudvis | HUD 2 | done, gate | - |
| 63 | `RadiationAlert`, `TrailAlert`, `LagAlert` | children of `Targetable_Objects` | native, never hidden | homes, then hidden: 6.8 | M |
| 64 | Objective banner, waypoints | native | native | keep: registry rows | - |

## 6. What each screen becomes
### 6.1 The nav map
Frames: `research\evidence\hud2_gate\mock\navmap_*`, on the real grown map. Native today: `research\experience\runs\hud28_windows_2026-11-05\m4_navmap`.

**A new and improved map, in 0.4.0 (the owner, 6 Nov 2026: "it's as good a chance as any to make a new and improved navmap if the game lets us"; frames `decisions\Map_new_and_improved.jpg`).** Aim higher than growing it, as far as the game allows. The game keeps what only it can do: its art, lanes, zones, patrols, pan, zoom, selection. HUD 2 draws what a player reads, over the map's viewport, from data already in hand:
- **Labels:** the game's off through its own flag (byte 0x611C5C, the label button's), HUD 2's drawn at the floor from the object list at `+0x400` (position, radius, name ids), so the map's words are finally HUD 2's size, and hook 0x4177B0 is no longer needed. Decluttered by rank: bases, gates, then the rest; the game's 550 rule as a start.
- **One mark per object**, coloured by attitude with a form per kind, as the cursor set does; the game's own art stays under it.
- **The route** on the map: the plotted path as a line, each leg's distance and an estimate (`core\game\route.*`, `universe_data.cpp`), and the same list in the left panel.
- **You, on the map,** heading included; the objective's mark.
- **A destination list** by kind, on both devices: what the pad's View list does today, a non-cursor route to every target; a search on K&M.
- **The selection card** as the target card; services of a base where the game's data gives them.
- **Universe view:** systems named at HUD 2's size from the connections list `+0x810`, the jump count on the route.

**The limits, from the probes:** zones, lanes and patrols stay drawn by the game (their containers are undecoded: C stays rejected). Unproven: that the label flag holds on every view, that the overlay tracks through pan, zoom and the slide (the transform is read each frame from `+0x424..+0x434` and the view fields), objects the player has not discovered (the "known" flag is unread: the overlay must not reveal what the game hides), mods that change the map's content. Each falls back to the game's own labels.

**Changed 6 Nov 2026 (the critique's R7; this wins over the text below):**
- **In steps, risk H, all in 0.4.0.** First the frame off, the words, the strip, around the native map: ship-worthy alone. Then the growth and the new map, after a sweep of the hit boxes under the hidden controls and a second-install pass. The growth adds, from the probes: `+0x4A8 / +0x4AC` written every frame the map shows, a new-window hook (the map is rebuilt at each open), 16 grid texts, titles and a base list by the Layout tier, the legend's meshes with no route yet, a reader unwritten.
- **The pad's inputs close by two zones.** Canvas: left stick, triggers, A / X / Y, no chip focused. Panel: the D-pad enters and walks every chip, A presses, the left stick returns: the owner's Destiny rule. One named press for SYSTEM / SIRIUS, and one back to the ship's system. The design's X WAYPOINT is X REMOVE as built: keep the build's.
- **One solver:** the map is `min(0.785 H, W - panels - margins)`; at 16:10 that is 0.764 H, not four aspect rows.
- **Labels:** the game's are thin at 800, so growth alone only declutters: HUD 2's labels above are the fix.
- **Right panel:** the selection card takes the target card's form (attitude, class, distance, faction edge); layers as tiles on K&M, one line on the pad.
- **Words:** the strip reads **LOG, MAP, INVENTORY, STATUS, INFO** (the game's own top-right order since 7 Nov 2026, Shockwave; was INVENTORY before MAP); the flight label reads **NEURAL NET** (the owner, D2). Below, read INFO for HELP.

**Why:** 0.49 H of map inside 1.0 H of chrome, its toolbar icons ambiguous ("?", crossed tools) and over the map's top row, the name plate over row 8, the panels mostly empty.

**The tier.** A + L + S, not a new map:
- **The frame goes:** `+0x84 = -1`; the two side windows by bit 1 of `+0xBB8` / `+0xBBC`. The note in `winplates.cpp` that the draw never reaches the base draw misses 0x5A06A0, the art half. Nobody has shown this on this window, and what `GameOption(0x15)` gates there is unknown (probe 1). The draw 0x4926D0 itself is never replaced: it draws the side controls, briefing, objectives and mode-dependent children by hand from a private list, so replacing it takes them all.
- **The map grows:** `+0x40C / +0x418 / +0x41C` written after creation; the viewport, the camera and `NavMapMouse` follow (decompiled). Square of 0.785 H at 16:9, from 0.115 H down to 0.900 H, centred between the panels. The top stays clear of the subtitles' panel: the map is what moved, as the owner chose on 5 Nov ("Why not move the map, instead?"). It replaces `windowtext`'s 0.05 H drop.
- **The labels stay native** and keep their size, so they stop colliding: 0.025 H at 4K, 0.0225 H at 1280 wide (`NavMap*` fonts, chosen by screen width). At the Deck the font hook 0x4177B0 may take a rung up (probe 4).
- **The side windows become panels (Shadow):** the native texts stay, hidden; the panels read them. Left: place, objective, route, view. Right: the selection card with its actions, then the layers. Each ends at its content.
- **Words replace icons:** PHYSICAL, POLITICAL, PATROLS, MINING, BASES, LEGEND, each a chip calling the native button by its own action (`uiclick::Click`, vt[0xB4]: no new symbol). Filters the game disables show disabled.
- **The legend and base list** take the right panel's lower part; the briefing keeps this frame with the mission text left and Continue as a chip.

**C, FLvibed's own map, is rejected:** zones, lanes and patrols are drawn by code whose data is not decoded, JFLP and FLSharp change the map's content, and FLUF's version cost over 4,000 lines. If probe 2 fails, A alone still gives the new frame, the panels and the words around a map of native size.

**Every NeuroNet window stays one step away** (a player's question, 5 Nov 2026: "How will you access your inventory and stats from this larger starmap?"; evidence `10_neuronet_routes.md`).
- **The native routes:** the NeuroNet box's five buttons (`NN...ButtonRoot`, children of `NN_MainButton`, top right) and the keys F5 log, F6 map, F7 inventory, F8 player status, F9 help. A click and a key are one path: `HandleKey` to the handler 0x45CAE0. Another window's command switches directly; the same command closes; a press during a slide is lost; no window links to another by a button.
- **Taking the frame off cuts none of it:** the Roots are siblings of the windows, not children of the map or its art (0x45B8F0), and `+0x84` and the side windows' bits do not touch them. Decompiled; probe 13.
- **The pad has no route today but View to the map.** The action wheel holds none; `SetHudHitSuppression` takes the Roots from the pad's cursor. So log, inventory, status and help are out of the pad's reach from flight now, map or no map. This fixes that.
- **The strip:** one row of words on every NeuroNet window, the same place, the same order: **LOG, MAP, INVENTORY, STATUS, HELP** (CHAT and GROUP join in multiplayer, as the game's box does), the open one yellow and underlined. It replaces the box's icons, by the owner's rule: HUD 2's, not the game's, and words for symbols.
- **Where:** the bottom lane's left half, beside the prompts, at the 0.05 W margin (0.0125 W on the Deck). **It takes nothing from the map's 0.785 H.** The top band was tried first in the mock and rejected: the comm panel holds the top centre, and against its place in `m4_navmap` the strip ran about 0.19 H into it at 4K and 0.24 H at 1280x800. The bottom lane's rule 2 takes the strip as a keep-out; at 1280x800 strip and pills leave about 0.2 H between them (`navmap_1280x800_pad`). 4:3 unread: the pills' column applies first.
- **The switch:** every press is `Tap(cmd)`, the game's own gates, toggle and switch. A press during a slide is held and sent the first frame after, as the pad's B already is (`room::NNWindowBusy`); the last press wins, so LB LB lands on the second tab, one slide later. The strip follows the open window from the manager (`+0x7C`), so a window the game opens itself (the log on new mission information) is right without a press.
- **Pad:** LB / RB step the tabs from any NeuroNet window, wrapping. View opens the map, as today. With the map in the middle, **INVENTORY and STATUS are one press from it**, LOG and HELP two: the tab order is the player's question answered, not the box's order. That moves two bindings: the log's MISSION / LOG to the D-pad's left and right, and `UI_PLAN.md` 6.4's map LB / RB system / universe, which LT at the system's edge already reaches (`padmap.cpp:85`). Both are the controller module's.
- **K&M:** one click on a word, the keys as ever, and a CLOSE chip at the lane's right end, where the pad has B.
- **Docked:** the same strip while a NeuroNet window is open; with none open the room bar and box stand as today. `FREELANCER_RE` says the inventory opens nothing docked, the 25 Oct inventory says it did: probe 15. If the game refuses, the word dims.
- **The new-information flash** on the log's button was never located (`FREELANCER_RE` 1122). The strip must not lose it: probe 16. HUD 2 already hides the box in flight, so it is lost there today.

**Everything else on the map keeps a route.** Native control to the design:
| Native | Becomes |
|---|---|
| Physical, Political, Patrol, mineable, Legend, Base list filters | PHYSICAL, POLITICAL, PATROLS, MINING, LEGEND, BASES chips |
| Mission filter (off without objectives) | MISSION chip, disabled alike |
| `NavMapLabelButton` | LABELS chip |
| `NavMapAllFilter`, `FriendlyFilter`, `HostileFilter` | chips under the layers where the game shows them (probe 14) |
| Universe, Ship buttons | SYSTEM, SIRIUS |
| Zoom, Set / Clear / Clear All waypoints, Best Path | selection card chips |
| `NavMapSelectObjectButton`, `NavMapJumpGateButton` | selection card chips, when the game shows them |
| Mission objectives, `NavMapAbortMissionButton` + Yes / No | left panel; abort a chip, then the dialog (6.2) |
| Briefing's Continue | a chip |
| Close | B, the lane's CLOSE |
| Info card, `NavMapCloseInfocardButton` | selection card; any link it holds: probe 14 |

**Pad.** The left stick moves the free cursor on the canvas, with friction over objects: the object list at `+0x400` gives position and radius, which ends `padmap.h`'s "no snapping to objects" (it could only see FlControls). The D-pad reaches objects and panel rows, so every target has a non-cursor route; View keeps the destination list while the map is open. Triggers zoom, the D-pad's left and right change layer, up and down step objects, A select, again = waypoint, X waypoint, Y travel, B close: as built where it exists. Selecting through `NavMapMouse` stays; a selection by record needs probe 5.

**K&M.** The native mouse on the map; chips are hotspots (`hud2_click`); no lane. Same frame.

**Sizes.** 16:9: two panels of 0.40 H. 16:10 and the Deck: 0.38 H, text 120. 21:9: the panels gain, the map keeps its height. 4:3: the map shrinks to 0.70 H and one column of tabs replaces the left panel; **unread until frames exist.**

**Universe view.** The art stretches 1.6x (`navmap_universe_4k_pad`: acceptable in the mock, a texture of unknown size). Region names are baked into it and stay.

**Open and view change.** The native slide runs; the zoom's own timer stays (0.4 s, 1.5 s).

### 6.2 Quit and every confirm
Frames: `quit_confirm_4k_pad`, `quit_confirm_1280x800_pad`, `exit_game_4k_pad`.

**The cargo demand (0.4.6, GitHub issue 76):** the mission message box's drop-cargo form (told from the other forms by `DropCargoButton` and `DCCancelButton`)
is this dialog too: the demanding ship's name as the title, the game's own words (formatted from the box's FmtStr, its rich text has no reader), DROP CARGO
and CANCEL as the game words them, the X (the same as CANCEL) gone. No scrim and the flight HUD not put under it: the game does not pause for it and closes it
by itself after 20 s. The pad: A drops, B cancels (`padmenus` Popup). Under the flight HUD (0.4.6's verification): its plate is a reserved area of HUD 2's
(`ABox`, `UI_HUD2.md` 5.45), its answers stand in HUD 2's prompt lanes (not the prompt bar, whose fallback put them on the weapons panel), and the game's arrow,
which the box switches to wherever the pad's aim stands, is hidden while the pad is the device.

One dialog primitive for NN_Confirm, the Exit dialog, NN_Prompt, NN_Error, the MP prompt, abort mission, ship purchase. The ornate box goes (`Confirm_deleteBox`, `ExitFrame`, `ExitTextFrame`: art children). `UI_HUD2_EXTENDED.md` kept the confirm's box because it dims the pause menu; the scrim takes over that job and the boxes behind go dark under it.

**The dialog stays a centred box (the owner, R10, 6 Nov 2026; the critique's alert card overruled):** HUD 2's own plate, centred, title and cost centred, the two chips side by side, as `decisions\R10_dialog.jpg` A. What goes is the game's ornate art, not the centred form. The focused chip's word is in INK (R9; the sheet still draws it blue): the verbs are chips the skin draws. Where a dialog keeps the game's own words on a native box button, R9's limit holds and the word keeps today's colour.

| Dialog | Title | Cost line | Action, safe | Focus |
|---|---|---|---|---|
| Quit from a game | QUIT TO THE MAIN MENU? | Progress since your last save will be lost. | QUIT ANYWAY, KEEP PLAYING | safe |
| Exit from the main menu | EXIT FREELANCER? | Back to the desktop. | EXIT GAME, STAY | action: nothing is at risk |
| Alt-F4 in a game | as quit, EXIT | same cost | EXIT ANYWAY, KEEP PLAYING | safe |
| Delete a save | DELETE THIS SAVE? | the save's name | DELETE, KEEP SAVE | safe |
| Abort mission | ABORT THE MISSION? | the game's own text | ABORT, KEEP GOING | safe |

- **Words (decided, no setting):** the captions are the game's controls; the skin hides them and draws verbs on the same hit boxes (Shadow). If the dialog's text is not the one the row expects (a mod, another language), the game's own words stay in the same primitive.
- **Order:** the game's: action left, safe right. Windows does the same.
- **B / Escape:** the safe answer everywhere. No hold: a dialog is the friction, and XAG 115 says not to require holds for destructive actions. FLvibed's own one-second X hold to delete a save breaks that: a press of X now opens the game's confirm (section 13). Amber edge where progress can be lost; red stays with hostility, one meaning per colour.
- **Overwrite:** the game's save-overwrite question was not located. If the seam audit finds one, XAG 115 asks that the player sees what is overwritten.
- **Stale claim guarded:** "your last save" stays true because it says nothing of when.

### 6.3 Main menu, load
Frames: `research\evidence\hud2_gate\mock\mainmenu_*`, `decisions\D9_continue.jpg`, `decisions\R17_menu_and_loading.jpg` (Menu B).
- The planet scene stays: a 3D base. **The game's logo stays** (the owner, D1, 6 Nov 2026). Chips on the game's own button places, focus as everywhere. The buttons keep their slide-in. **Floating chips, no column plate** (the owner, R17): each chip carries its own glass.
- **CONTINUE on top** (the owner, D9, reversing 13.3): one press loads the latest save; six entries at a smaller pitch. Failure cases are the feature's: no save, a save of another mode.
- Version line in the corner, one line, DIM (`Version`; FLvibed's version joins it).
- Load (front): LOAD GAME, DELETE, MAIN MENU lose their native frames and take chips (the gap of `UI_HUD2_EXTENDED.md` 16).
- The logo is drawn before the UI pass, so at 1280x800 it lies under any plate that meets it: it ends at 0.20 H, the first button starts at 0.276 H.

### 6.4 Loading
**Built in 0.4.0, form C (the owner, R17, 6 Nov 2026; the critique's deferral overruled): the place name at once, a bar only past 1.5 s.** A load takes 1.08-1.22 s, so most loads end before any bar shows and a slow machine still sees one. This replaces the 500 ms delay of the first design, which left a bar for about 0.65 s.
Frame: `decisions\R17_menu_and_loading.jpg` (Loading C). 0x578F70 skips the UI pass, so the skin needs its own draw point. Probe 9 picked: the bracket, 0x59E5A0 filtered on caller 0x5790F2; 0x578F70 only to fill the black frames between. **The game never waits for the skin.**
- Navy fill and, from the first frame, where to: the base and the system when the game's data gives them, else LOADING. Which data gives them on each load path is a probe (section 15, step 1).
- Past 1.5 s: a segmented bar from the native progress under the name, no looped animation.
- No tips (the load is short). Fail closed: flag unreadable, the native screen.
- A Deck's load is unmeasured: the 1.5 s is there so that a longer load still shows progress.

### 6.5 Multiplayer
Frame: `servers_4k_pad`; rows illustrative, places from `PAGE_ServerSelect.page`. Page rule: plates, chips, header. **Table rows** are drawn by the table control in Arial `ListText`, so their text stays the game's until probe 7 says the font can change or the rows can be read. That is the weakest screen for legibility, and it stays so: **multiplayer's tables stay with the page rule, no Shadow table** (the owner, R17). FLUF's "Select Character" overlay draws above the character list; the skin under it is harmless.

### 6.6 Small things
- **Tooltip, Rollover:** Shadow: the text read, the game's box hidden, HUD 2's drawn beside its control. On the pad the fixed help line. No autopop; the game's delay stays.
  Built in 0.4.6 (GitHub issue 71: the Shadow read had tested a click flag and an empty field, so it never read; the game's boxes stood under the strip):
  both read from +0x7C on skin screens (docked, a window, a menu; plain flight keeps the game's), the game's hidden for one UI pass and given back after
  it, HUD 2's at the game's place unless the strip, the room words, the NeuroNet box, a NeuroNet window's tabs, the map's panels or the prompt lane
  stand there - then the free place nearest to it (`skinrules::TipPlace`); the box first, the line clear of it. The full-screen map (GitHub issue 75): the
  map's hover is the ToolTip, which the game places by its own map's size, so there the name tag stands below right of the cursor (mouse and the pad's
  map cursor alike), clear of the top row (title, tabs, LAYERS), the chips and the cards; nothing hovered, no tag. The map's place names keep out of
  the tag as they keep out of the cards (the other side of their mark, else they drop), and the hovered place's own name stands down while the tag says it.
  **HUD 2's words** (issue 71, second round): the room bar's words, the actions under it, the NeuroNet box's and the strip's tabs stand elsewhere than the
  game's buttons they stand for, which stay hidden at their own places - the game's hover there named whatever button lay under the word. With a cursor on
  screen (mouse, the pad's stick cursor) and the cursor on one of their plates, the game's help is hidden and not drawn; on a word, HUD 2 shows the help of
  the control that word stands for (the one its click acts for: `nnstrip::WordAt`), read as the game reads it (`skinover::WordHelp`: the hint IDS with the
  command's key, the Rollover's long hint by its map, each only while the game's option for it is on), after the game's quarter-second rest, under the word
  by `TipPlace`. The short tip shows only where it says more than the word (`skinrules::WordHelpText`: "Neural Net Log (F5)" on LOG, "Commodity Trader" on
  TRADER; not "Bar" on BAR). The D-pad's focus (no cursor) keeps the path above.
  FLvibed's options page over the screen: the help stays the game's, under the page (read and drawn last of the overlay, a hovered character's name card
  had stood over the page).
- **Popups:** the title in INK, OK a chip. They stay until dismissed, as the game's do.
- **Death:** native buttons on the plate; focus on LOAD AUTOSAVE, one press to retry; input taken at once. The ~2 s before they appear is the game's.
- **Cursor (decided: HUD 2's own, the set of `decisions\D1b_cursor.jpg`, as drawn; the owner, D1):** all 19 names of `DATA\mouse.ini`, one meaning each. An arrow for menus and `stretch`; a four-part reticle for flight, colour by attitude and a form per state (since 7 Nov 2026, the owner: "include inwards-pointing triangles ... a bit closer to vanilla FL": four arcs on the axes that read as the game's ring, its dot, and over a ship four darts in the diagonal gaps pointing in, solid / hollow / open by attitude as the target's carets; firing pushes them in and bolds the ring, as the game's hostile_fire; no-fire a bar; free aim the ring and dot alone; debug `uiskin cursorsheet 1` draws the set); a word in a bubble for the `talk_*` names: JOB, INFO, RUMOR, STORY, BRIBE, "...", GEAR, GOODS, SHIPS. At the game's own cursor position, with the game's mesh hidden through `SetCursorHidden`. The shape comes from the game's current cursor name; a name the table does not know (a mod's) stays the game's and is logged. The game's cursor is a software mesh that scales with screen width, so HUD 2's keeps that rule, at the size of the frames. Position, clicks and hover stay the game's: a change of look, no emulation. It costs what the game's software cursor already costs.

### 6.6a Routes on every other screen
Removing art must cut no route. Checked screen by screen:
| Screen | Finding |
|---|---|
| Rooms, room bar, NeuroNet box docked | art only; `ButtonControl`, `RoomControl`, the Roots stay hit-testable (verified since 25 Oct) |
| Dealers, ship dealer, job board, news, offer, repair | built, harness-tested on every button |
| Pause, save, load, options | built; hit boxes are separate controls |
| Confirm, Exit, prompts | the dialog hides art and captions, the buttons' hit boxes stay: no route changes; rows with verbs need the captions' own controls left clickable (they are wire buttons) |
| Main menu, front load | art off; buttons, captions, clicks stay |
| Front pages | page rule keeps every `UITextMsgButton`; a control of an unknown class stays visible |
| Tooltip, Rollover | text only; a hidden help box hides no action |
| Popups | close X and OK stay |
| Cursor | look only |
| Multiplayer windows | CHAT and GROUP Roots live in the strip |
Only the map's side windows and the box's icons change where a route is drawn; both are covered above.

### 6.8 The native alerts (the owner, D6, 6 Nov 2026; frames `decisions\D6_final.jpg`)
"Cards stay for dangers with a cure."
- **Radiation:** one standing amber line in the nav block, RADIATION · LEAVE THE ZONE; it takes the next free row and keeps the place line's slot, so nothing overlaps it; a lint row holds that. **The game's radiation art stands beside it**, steady, as there is room at both sizes: the native control moved, or its own texture, so nothing of the game ships. A card once on entry.
- **Tail:** an unlabelled ring mark at its bearing, as the hit arc; no stack slot. Hide `TrailAlert` only after this stands.
- **Lag:** one small word in the same block, multiplayer only.
- FLvibed's own Neural Net column cuts "DETECTE" at the screen's edge at 800: a live bug of its own, not touched by hiding the native alert.

### 6.7 Transitions
| Moment | Native | Skin |
|---|---|---|
| Any screen to any screen | art visible until replaced | the registry (3.1) |
| NeuroNet window | two slides of 0.30 s, 0.61 s measured, plates follow | stays; shortening goes with one frame for all five windows (R8, 6.9) and needs no hook on 0x55E120: the 0.30 is one float in .rdata, `exe::NNSlideTime`, not an immediate (`FREELANCER_RE` "The NeuroNet's window manager"); a close during it is held, as built |
| Front-end screens | 0.83 s out, 1.54 s in, 2.37 s, about 1 s empty | stays until decoded |
| Modal open, close | instant | scrim eases 150 ms in, 100 ms out; input never waits |
| Base to space | no fade | none added |
| Reduced motion | - | `skin_motion`: FULL, FADES, NONE; default FULL |

The skin adds no wait anywhere and no motion on a path a speedrun measures.

### 6.9 Flight, windows, the dealer (the owner's rulings on the sheets, 6 Nov 2026; frames `decisions\R*.jpg`)
- **K&M in flight (R4, all of B):** one lit chip with its key in place of the maneuver row's pictograms; the whole row in words on peek; NEXT and CLEAR in words on the target buttons; two to four threat rows at floor size while no target is held. Read them with `hud_fade` on first. The keys stand inside the K&M card's click row (`UI_HUD2_SIZE_AUDIT.md`), not over its lower edge as the sheet draws them.
- **The ring (R6):** a navy keyline of 1-2 px on every arc, no area added; in the same pass the place line goes to `kChrome`. The sheet's sun rows show the intent; its ragged outline over the planet is the mock's.
- **The bottom of the reticle:** form C, `UI_HUD2_INPUTS.md` 7b.
- **One frame for the five windows (R8):** one top line, one left edge, one scrim from MAP to INVENTORY. Windows grow by pitch and font, not rows, by the Layout tier, Deck first: probe 18 proves `NN_Inventory`, whose hit boxes are unproven. The hook on 0x55E120 goes with the strip's work. INFO's plate ends at its content. Still to state in one line at step 6: the docked bumper hand-over (room bar, SIDE, strip).
- **One distance (R13):** the target card and the threat rows read the game's surface value, so card, bracket and cue agree on one number. The card shows it beside the bracket: an exception to "one value, one place", written with the cue's in `UI_HUD2_PRESETS.md` 1.7. The name stands on both too, as a holding position: the bracket and its label are open (section 16).
- **Buying and selling (R16):** the hold on the row stays. One press up from the row reaches the game's BUY or SELL, and a tap on it does the trade. The lane says which: HOLD BUY or HOLD SELL on the row, BUY or SELL on the button. On a filter A reads SELECT and a hold does nothing, whatever row stands selected behind the focus (`dealerrules::HoldAct`). One focus: the row gives up its block when the button takes it. The controller module's (`padbases.cpp`): `PanelFocus` only reads focus, the tap is blocked on purpose today, and the re-select after a trade needs care.

### 6.10 The targeting display (the owner, 6 Nov 2026; frames `decisions\Target3_*`; research `docs\research\target-info-display.md`)
It replaces the native bracket corners, label and bars on screen. No anchor at the target card, and no line to it: it makes the player look too far away. **The circle is the player's:** nothing here draws one.

| Part | Rule |
|---|---|
| A new target | **The mini card:** beside the ship, on the side away from the ring: attitude and class, two bars, name and distance. The rows stand in the settled form's order, bars over words, so nothing crosses. Stays 2.0 s |
| The change | 0.35 s: the class line fades first, the plate draws in, bars and words travel under the ship. `skin_motion` FADES fades in place; NONE cuts. A new target starts it again |
| Settled | **Worn by the ship:** four attitude carets on its diagonals, each pointing at it (the owner, 7 Nov 2026: one caret over the ship did not keep the eye on a target inside the ring; like the game's brackets, but triangles pointing in: `CaretQuad`, `kQuadGap` / `kQuadLen`, the block and the card clear them by `QuadReach`), two straight pip bars under it, shield over hull, one line of name and distance under them, until the target changes |
| The soft bed | A soft dark bed behind bars and words, low strength, always on (the owner): the text's outline alone runs thin over the sun's flare |
| Attitude | By shape as well as colour: hostile solid carets, neutral hollow ones, friendly open ones (the base left out) |
| Size | Marks follow the object's on-screen size from the game's radius, floor 0.030 H, cap 0.10 H, so the block stays on screen on a capital |
| After a hit | The remaining shield or hull in numbers beside its bar for 3 s, faded over 0.4 s; another hit starts it again; a white trail marks what was just lost: held 0.35 s after the last hit, then drained to the left towards the value (about 0.6 s for a fifth of the bar; gone at once under skin_motion NONE), not held for the numbers' 3 s (Shockwave via the owner, 7 Nov 2026) |
| Inside the ring | **The seat (the owner, 6 Nov 2026; was the shoulder):** the four carets stay on the ship; bars and words stand centred on top of the reticle: over the cue's row while it shows, just over the ring when it does not, over an alert card that stands there. With the ring faded there is no seat and they stay with the target. A new target's card opens there. Enter and leave thresholds by probe T8 (the research's R + 0.03, R + 0.06 to start from) |
| Close ships | The target in full. Each other ship takes the richest form that touches nothing, falling a step when it would: caret and name, caret and distance, caret and one row of hull pips, bare caret; no mark at all where the caret would stand on a panel, a prompt or the subtitles (GitHub issue 77). Where no room exists for the card, the target goes straight to the settled form (this design's pick; not drawn) |
| Waypoint target | **By its type:** a ship stays a ship, a solar a solar; a solar with nothing to damage has no bars. Being the waypoint adds a small diamond round the attitude caret, no more. The game's large diamond and its distance go, the distance into the words line. If the diamond's art cannot be hidden (+0x7B4, unprobed), it stays round the same form |
| Breathing | The lock's glow on the cue at 1 Hz, the waypoint diamond's at 0.5 Hz (the owner); both hold steady under `skin_motion` FADES and NONE |
| Off screen | The game's arrow, name and distance on the card. On leaving, the words wait by the arrow about 0.5 s and fade, so a target at the edge does not flicker |
| The card | Keeps name and distance on screen too (the owner): R13's exception to "one value, one place" stands (`UI_HUD2_PRESETS.md` 1.7) |
| Moving (phase 2, three clips judged) | A jump of the block travels 0.20 s, never cuts; `skin_motion` FADES and NONE cut. The side changes past the middle by 0.06 k and holds 0.8 s; the seat holds 0.6 s, enter R + 0.03, leave R + 0.09, and both wait for the change to start, so the block goes to the seat straight. The mini card is beside the ship, on its other side where the screen ends; where it would touch the ring, a panel, a prompt, an alert or the subtitles it does not stand: the settled form at its opening, the change later (the "no room" rule above). Off the seat the block stands outside the ring beside it, above the ship over it, held 0.5 s; an alert card has the place, and with no room the words fade and the top card still names the target. Off screen the block stays where it stood and fades; a target never on screen has none. The caret stands over the ring and the plate, its size stops at 0.012 k: a fighter's radius reaches the marks' cap at about 300 m. Position is projected at the draw |

**Words:** all at the floor, `hud_size`'s. Pad and K&M alike: draw only, no hotspot; clicks stay the game's records. The lint needs a row: the card is a plate in S1 for 2.0 s, and the settled block stands in the lower centre when the target does (the 31 Oct rule's diegetic exception, the owner's word).

**How (decompiled, unprobed; the note's 3 and 5):** corners by nulling indicators +0x7A0, no hook; label and bars by the re-cut stash on 0x4EF030, keeping a waypoint target's route in mind; `Targetable_Objects` stays a kept root, with three part rows under it (corners, label, bars) in the registry of 3.1. All or nothing: a refused symbol or the module off leaves the native bracket and no form. Position by projection, `space::TargetBody` for size; the other ships need the core reader of the indicator records (U66).

**Still owed:** the form on a moving ship (a clip on real motion, then the side-change and edge rules), probes T1-T8 and the diamond's art; a raised caret on small waypoint targets, whose diamond comes near the ship; a solar with bars.

### 6.11 Failing bars (the owner, 6 Nov 2026; frames `decisions\Target3_bars.*`; research `docs\research\bar-critical-feedback.md`)
Every shield and hull bar: the ring's arcs, the target's card and settled form, the escorts'. It replaces the fixed 1.5 Hz low pulse, and the alert cards' glow follows the new beat (the owner). **A bar never changes colour:** both effects stay in its own, blue or red, with no white.

| Part | Rule |
|---|---|
| Breathing | From 25 % down, stopping above 30 % so a recharging bar does not flicker. A glow that swells and sinks; depth fixed, only the rate changes |
| The rate | `f = 1.25 * (2.9 / 1.25) ^ (1 - v / 0.25)` Hz: 1.25 at 25 %, 1.75 at 15 %, 2.45 at 5 %, 2.9 at 0. The phase is integrated, never `sin(2 pi f t)`, and f low-passed over about 200 ms: a rate change must not jump the glow. 2.9 Hz stands just under the 3 Hz limit |
| Its shape | Tight and symmetrically centred on what is left: full over the lit pips, the same short roll-off at both ends, 10 % of the bar's length, so the real value stays plain |
| Its reach | The ring: full. A target's bars: held inside the row. Escorts: pips only, 50 % depth (not drawn) |
| Quiet state | No further loss on that bar for 6 s: depth falls to 40 % over 1 s, rate kept; any new loss restores it at once (the research's pick; not drawn) |
| Bloom | At zero, in 600 ms from the frame of the break: catch 0-50 ms, the whole bar lights in its own colour; burst 50-130, the halo reaches 0.05 H away from the reticle, at most 0.02 H inward, a target's stays near its row; spread 130-450, it fades as it creeps on by 20 %; settle 450-600, empty pips come up under it. Words and numbers draw above it |
| Limits | A target's bloom waits 60 ms behind the player's; at most 2 full blooms at once; the same bar at most once a second; escorts: pips only |
| A shield | Triggers on the `up` flag falling, not on the value: a failed shield's value climbs while it is offline. No breathing while offline |
| A hull | At zero a ship is dead, so the bar needs a ghost kept for the 600 ms. The death path's effect on the HUD is unchecked |
| `skin_motion` | FADES: a steady glow rising as the value falls, 0.10 at 25 % to 0.28 at 0; a break fades in place, 100 ms up, 300 ms down. NONE: steady, a break cuts. Pips, numbers and the alert card carry the state in all three |

**Works beside:** the screen-edge flash of `shield_cue` on a failed shield (`HUD_CUES.md`): the bloom must not double it. **To settle at the build:** a flash test: the hull's saturated red at 2.9 Hz is the strict case (the note's limits); the game's own hull-critical line is 20 %, the ring's 25 %: 25 % stays unless a frame says otherwise.

## 7. Pad and K&M
Both forms, the same frames; only lanes and focus differ. A = accept, B = back or the safe answer; prompts only for the active device; glyph family as set. Every chip the skin draws is reachable by D-pad (`padmenus` reads the skin's rectangles) and by mouse (`hud2_click`), and calls the native control's action. No input is emulated. Mouse and pad both live at all times, as built.

## 8. Fail closed
- **One window fails, one window goes native.** A read that fails, a rect not known, a vtable or signature that does not match: that window's writes restore at once, one log line, native art for the session. A plate never stands without its art hidden, nor art hidden without its plate.
- **Guards:** each family's pass under its own SEH guard; a fault stops the family, then the module (`OnFault` gives back).
- **FLUF:** its map replaces the native window, so the skin's map never opens: nothing to stand down. Its windows stay on top (the owner, 26 Oct). Its MP overlay lies over the skin. FLUF also has OpenGL (FLAR) and D3D11 back ends; what they do to the game's 2D screens is unverified. The rule holds whatever they do: no D3D8 device, no skin, everything native.
- **HDE:** art variants (Dark, flat icons) are hidden alike; nothing depends on them. The installer can rename the font family and rewrite `fixed_height`: read the face and sizes from the game's tables, never a literal.
- **JFLP, FLSharp:** change the map's content and waypoint selection; the skin calls the native paths, so both carry through. Check in a second-install pass.
- **Discovery, Crossfire:** their own exe patches and art; signatures decide per symbol.
- **`skin_native`** (hidden key, comma list of family labels): a window stays native by name, for troubleshooting and reports.
- **Classic:** `hud_style 0` or `ui_style 0`: no write, no draw, as today. Tested by comparing frames, not assumed.

## 9. Mock frames
`tools\uimock\hud2s.py`, written to `research\evidence\hud2_skin\mock\`. Mock, not game: the map's pixels are the game's 1050 px scaled up (the game draws it sharp), its labels redrawn at true size; distances, routes and server rows are illustrative; at 1280x800 the mock crops the 16:9 scene. The scene layer keeps a ghost of the classic gauges under the scrim, a capture's leftover. Text wrap and fit are the mock's. No critique pass has run: the first in-game frames get one.

## 10. Cost
Flight: unchanged by design; the gate's work is the compares of today, the 2 Hz search replaced by the generation counter. Non-flight today 67-84 us at 4K. Budget: 150 us per frame on a skinned screen, 300 us on the map (about 40 text draws, of which panels cached and redrawn on change). Measured at the first quiet window; noisy numbers are not stored. No per-frame walk of the control list; audit only on request.

## 11. What cannot or should not be done
| What | Why |
|---|---|
| Startup splash | before FLvibed's draw is up; unproven either way, low value |
| Intro movies, cutscenes, letterbox | content, not skin |
| "Freelancer Encountered an Error", Windows message boxes (no 3D device) | a failing game should show the plainest box; OS windows are not the game's to draw |
| Region names in the universe view | baked into the art |
| The 3D in windows (ship, item meshes, rooms) | no skin matter |
| Table rows' font in MP lists | until probe 7 |
| Screens a mod replaces (FLUF's map) | its owner's |
| One press to every NeuroNet window on the pad | LB / RB reach neighbours: two windows of five are two presses from the map. Nothing is lost: today the pad has no route to four of them |

## 12. Probes (first step of the build lane; one background instance, each a debug verb, no code kept; the weapon cue adds W1-W5, `UI_HUD2_GROUP_CUE.md` 10)
| # | Probe | Decides |
|---|---|---|
| 1 | `+0x84 = -1` on the map; bit 1 on `+0xBB8 / +0xBBC`; GameOption(0x15) | tier A |
| 2 | `+0x418 / +0x41C` to 0.73, a click against a drawn object, labels, zoom, pan, universe, docked | tier B; else A |
| 3 | Move the side windows' texts or read them hidden | Shadow panels |
| 4 | `NavMap*` font by hook 0x4177B0 at 1280x800 | Deck label size |
| 5 | Selection by record (0x49EA50, `+0x90C`) | D-pad cycling without the cursor |
| 6 | Bit 1 and `<none>` on page art children; frame child of a front text button | page rule |
| 7 | Table rows: font, readable cells | MP legibility |
| 8 | Duration of the window slide; the front-end transition's field | motion |
| 9 | 0x578F70's callers in a load from each path; which draw point of 6.4 sees every load | 6.4 |
| 10 | A `grab` of each moment in 3.1 before the gate | which path the tester saw |
| 11 | The logo's draw route: can it be hidden; its place at 1280x800 and 4:3 | 6.3 |
| 12 | Set Filter, jettison, key capture, notices: find by the seam audit | rows 11, 17, 43, 52, 61 |
| 13 | `hit` on a Root with the map open; `Tap(99)` and the others under HUD 2: mode and visibility (RE 2072) | the strip's route |
| 14 | The zone filters and any link in the map's info card | the map's route table |
| 15 | Inventory docked | the strip docked |
| 16 | The new-information flash on the log's button | LOG tab's mark |
| 17 | `hud2 lint` per context (calm, travel, Focus, 21:9, 4:3): `boxed=` and prompts | `UI_HUD2_SIZE_AUDIT.md` 6 |

### 12a. Results (the P11f gate, 5 Nov 2026; a probe build in a throwaway worktree, no code kept; scripts, logs and frames `research\evidence\hud2_gate\probes\`)
| # | Result | What was seen | What it changes |
|---|---|---|---|
| 1 | **PASS** | `+0x84 = -1` takes the frame off; clearing bit 1 of the side windows takes their art off. The map, toolbar and texts draw on, at 4K and docked. GameOption(0x15) not touched | nothing |
| 2 | **PASS, with four changes** | Box 0.73 and 0.757: viewport, grid, objects, zones, patrol lines, the selection popup and clicks all follow. A native click selects at the new place, nothing at the old. Zoom in and out, universe and back, every filter, docked: all work. Read back: 0.757 gives (1083, 124) 1696 px at 4K: origin writes are not screen-centred, so the 0.115-0.900 place needs its own numbers | (a) **Labels do not follow by themselves:** 0x499C20 places them with constants, 0.2 (0x5D3EEC, shared by two other functions) and 0.18 (0x5D3EE8) over `+0x4A8 / +0x4AC`. Writing those two fields x 0.468 / box makes them follow exactly, no patch. (b) The game **rewrites `+0x4A8` on a return from the universe**, so write it every frame the map shows. (c) **The window is rebuilt at each open:** a new object each time, so the writes go on the registry's new-window path. (d) **Everything else stays where it was:** the 16 grid letters and numbers, PHYSICAL MAP, the system name, the toolbar, the legend's art, the base list: the grid and titles are `TextControl`s for the Layout tier, the legend's meshes (`NAV_legend.3db`) are not: 6.1's "legend takes the right panel" needs a look at their draw |
| 3 | **PARTIAL** | With the side windows' art off, their texts stay in the tree and report a layout (`NavMapInfoTitle`, ext 8 x 50 empty). Reading the strings hidden: not done | Shadow panels still rest on a reader to write |
| 4 | **NOT ANSWERED** | `game_text_size` 130, live and from a restart, does not move the map's labels (nor did `gametext` apply: "applied=100"). Hook 0x4177B0 not tried | The labels stay 0.0225 H at 1280x800 until a hook is tried; readable in the frames |
| 5 | **PASS by another route** | No write of the record: the game's cursor warped to an object's place plus the native click (`uiclick::MapClick`) selects it. List head `+0x400` holds 66 objects in New York | D-pad cycling = warp and click. Selection by record is not needed |
| 13 | **PASS** | `hit` finds all five Roots with the map open. `tap` switches window to window directly, once the slide ended; a tap during the slide closes and drops the second press: the design's held press is needed | the bindings stand |
| 14 | **PARTIAL** | `NavMapAllFilter`, `HostileFilter`, `FriendlyFilter` appear under Political and Patrol, top right of the map. The map's info card links: not tried | chips as 6.1's table; add "POLITICAL, PATROLS only" |
| 15 | **PASS** | The inventory opens docked, skin family `inventory`. `FREELANCER_RE`'s line is wrong | the word stays lit docked |
| 16 | **NOT RUN** | The flash stays unlocated | the LOG tab's mark stays open |
| 17 | **DONE** | `UI_HUD2_SIZE_AUDIT.md` 6. Travel contexts all over 10 %; a real fight reads 18 %, not 15.4 %. Only Standard read; 21:9 and 4:3 not | the audit's totals re-read |
| W1 | **PASS** | Fresh ship: one real group. Three assigns: three groups, stored rows carry the mask | section 3's list stands |
| W2 | **PARTIAL** | Reach: guns muzzle x life (600 x 1.00 = 600; 600 x 0.92 = 550), LOCK seekers their seeker range (1000, 1500). `CanFire`: 0 ok, angle, refire seen. 3, 4, 6, 7 and a destroyed gun not reached: cruise read "angle" | the reach rule stands |
| W3 | **PASS for the ring** | Turret view: ring and row's place stay (`state=turret`). The turret's aim point not read | section 7's row stands; aim point open |
| W4 | **NOT RUN** | needs a mission weapon lock | cell state open |
| W5 | **NOT RUN** | the test ship has no turret | open |
| W6 | **CHANGES THE DESIGN'S PICTURE** | 16 samples, target closing in the guns' arc from 1300 m to under 500 m: M and T "ok" inside reach, `locked=0` in every sample | **A lit M or T does not mean locked.** As 4 says, but a player may read it so. Whether a lock ever forms with this ship is not shown: the autopilot flew, no missile fired. The owner's call: `UI_HUD2_INPUTS.md` 9 |

**The map at the owner's size:** the design's frames now hold the real map: `sheet_05.jpg`.

**The front end and the moments** (`probes\results_front.md`, frames `probes\frames\grab\`):
| # | Result | What was seen | What it changes |
|---|---|---|---|
| 6 | **PASS, two lines in the page rule** | Static image vt 0x5E4D7C: bit 1 or `+0x84 = -1` both hide it. `.page` UIWinObject vt 0x5E5D34: bit 1 only. Front text button vt 0x5E5AF4: no art child, its frame is its own `+0x84`: -1 keeps caption, `hit` and click; bit 1 kills the caption and `hit` | 3.3's class rule: per class, not one write. Pages rebuild on entry, so the hide re-runs |
| 7 | **PARTIAL** (a real LAN server) | The table has no child texts. Cells at `+0x564` (9 x 8, 0x64 bytes), getter 0x58D960. Row text not read, font not proven: the constructor does not name `ListText` | 6.5's "Arial `ListText`" is unconfirmed; a Shadow table is still possible, not shown |
| 8 | **PASS** | NeuroNet: two slides of 0.30 s in turn, 0.61 s measured, not ~0.5 s; the value is an immediate at 0x45CCF0 / 0x45D210, passed to 0x55E120, so a poke of `+0x1C` is overwritten. Front end: 0.83 s out, 1.54 s in, 2.37 s, about 1 s of it an empty screen | 6.7's numbers. Shortening needs a code patch or a hook on 0x55E120; not tried |
| 9 | **PASS, two cells not run** | 0x578F70 on a load, New Game, MP connect, launch and dock. A jump hits neither it nor the bracket; the UI pass runs every frame there | 6.4's draw point: the bracket, 0x59E5A0 filtered on caller 0x5790F2; 0x578F70 only to fill the black frames between. **6.4's "under a second, no bar flashes" is wrong: loads take 1.08-1.22 s, so with the 500 ms delay a bar shows about 0.65 s.** Its delay needs a new look |
| 10 | **PASS: the tester's flash caught** | **The whole native HUD for ONE frame, the first of space: 3 of 3 loads from the menu, 1 of 1 in-game load from a base.** About 20 ms, at 3-6 % contrast inside the fade from black. None on space-to-space load, load into a base, launch, jump, room change, pause, popup. Log: mode change and `hud2: on` in the same ms | **3.1's named paths are not it:** not the 2 Hz search, not a 3-frame debounce. The first UI pass of space runs before the first hide. So "hide at the UI pass's entry from the raw signal" stands, and it must run on that very pass. **A fifth path:** in space and under the death dialog the native NeuroNet box draws the whole time a window is open and ~600 ms into the close: the keep rule shows the root, and family `nnbox` is applied docked only. A keep rule must keep the root and still hide its art. Limits: the letterbox hides the top strip, so scenes are read only at launch; Alt-Tab, device Reset, mission failure not run |
| 11 | **PASS** | Object 0x67A7A8, vt 0x5E2718, draw 0x565A10, from the Intro frame 0x573EB0 before UiRender. Scales with width, ends at 0.20 H, the first button at 0.276 H; on every front page | a chained hook on 0x565A10 is the clean route to HUD 2's wordmark |
| 12 | **PASS, two not reached** | Set Filter: `ServerFilterDialog` vt 0x5E20EC. Key capture: `SetKey*` inside `NN_Preferences`. Jettison: an inline panel in `NN_Inventory`, not a dialog. "GAME SAVED" is a save's description: no notice exists | Rows 11, 17, 43, 61 are answered. **Key capture and jettison do not move `uigen`: the seam audit of 3.3 cannot rest on the generation counter.** Mission Accepted, player trade: not reached |

**A caution for the build:** a full-frame `grab` during a load killed the logic in that game (`FATAL: exception 0xE06D7363 in present`, likely out of memory): a test tool's limit, region grabs were fine.

## 13. The owner's decisions (5 Nov 2026, on reading this design)
1. **Dialog verbs:** yes, QUIT ANYWAY / KEEP PLAYING and the rest of 6.2, with no setting.
2. **Exit focus:** on EXIT GAME at the main menu.
3. **CONTINUE on the main menu:** out of this skin, for later.
4. **The full map's scrim:** start at 78 %, judged from real frames.
5. **Menu arrow:** "HUD 2 is HUD 2 - all new": HUD 2's own. **The rule for any such choice in the skin: HUD 2's, not the game's.**
6. **Deleting a save on the pad:** X into the game's confirm, replacing the one-second hold. The controller module's change (`padmenus`), in step 6 of the build; `docs\kb\menu-screens.md` follows.

**On 6 Nov 2026 the owner ruled D1-D9 (`UI_HUD2_CRITIQUE.md` 7).** Item 3 is reversed: CONTINUE is in. Item 5's rule now reads: HUD 2's for every surface and the cursor; the logo stays the game's, and the word is NEURAL NET. Item 1 stands, with **Dialog, Cursor, Alerts and Sizes as 6.2, 6.6, 6.8 and 4 give them.** HUD 2 is the default; no first-run page.

## 14. Fixes that land first, what this makes moot
**Built on, not redone:** issue 22 (docked fonts changing size and weight, P11c): this design keeps `windowtext` and adds rows to it, so that fix's rules bind every new row. Issue 23 (the squadron display's pips, P11e): flight HUD, not touched; if the fix adds a primitive, section 4 takes it.

**No fix should be spent on these; the overhaul replaces them:**
- The HUD flash of P11e (already moved here).
- `windowtext` row `NeuroNetNavMap`'s 0.05 H drop (`UI_HUD2.md` 5.38): replaced by 6.1. Its open reads (a three-row comm line over the map, a real mouse on the map's objects) carry to 6.1's tests.
- `UI_BUILD.md` gap "nav map objects not controls" and `padmap.h`'s "no D-pad snapping": answered by the object list.
- `UI_HUD2_EXTENDED.md` 12's left items: the confirm's ornate frame, the job board's native tooltips, the death dialog's focus; 16's gaps: the front load buttons, the popup's title.
- `UI_HUD2.md` 5.34's "subtitles' comm plate covers the toolbar".

**Not moot:** the blank UI seen once (5.38); issue 10's D-pad on the mount box; the target label over HUD 2's card.

## 15. Build order (rewritten 6 Nov 2026: by exposure, the critique's section 9, with the owner's rulings)
For release 0.4.0, after 0.3.5, on the owner's go. Each step proved at 4K and 1280x800, pad and K&M, with HUD 2 off unchanged, and committed alone.
1. **Probes** still owed: dialog classes, **a real lock**, hit boxes under hidden map controls, probe 18 (`NN_Inventory`'s Layout), the new map's three (16), 0x55E120, Alt-Tab, device Reset, mission failure, a story cutscene, the art under a letterbox, the load's place name on each path with probe 9's two cells not run (6.4).
2. **The registry, then each alert's home, then the hide** (3.1, 6.8): the first-frame flash, the box under windows; the seam audit.
3. **Flight:** the lint's `boxed` split into steady and awake peak, `hud_size` with the floors of D7, the size audit's cuts, the ring's keyline with the place line in `kChrome`, the reticle's bottom in form C (issue 28; `UI_HUD2_INPUTS.md` 7b), the alerts, the card's and the threat rows' distance, K&M's chip, words and threat rows (6.9); the Neural Net column at 800.
3a. **The targeting display** (6.10): probes T1-T8 and the diamond's art first (the note's 3.4), the core reader, then the form; a clip on real motion before the side-change and edge rules are fixed.
3b. **Failing bars** (6.11), after the targeting display's bars stand; the alert cards' beat with them; a flash test.
4. **Primitives** (4) with a grammar table, the focus word in INK.
5. **The strip, NEURAL NET and INFO, the map's frame and words around the native map; the pad's two zones; the slides.**
6. **Base text** (rows 29-31, 41, 42: one rung by resolution, 800 first), the five windows on one frame, the tap on BUY and SELL with its lane (6.9).
7. **Dialogs** (6.2), centred, one probe per class.
8. **The cue** (`UI_HUD2_GROUP_CUE.md` 0).
9. **Main menu with CONTINUE on floating chips, front load, the loading screen, tooltips, popups, the cursor set** (6.3, 6.4, 6.6).
10. **The new map: grown, HUD 2's labels, route and marks, Shadow panels; the page rule** (6.1, 3.3), after the hit-box sweep and a second install; a Proton run.
11. Optional after: the front end's transition.
12. Close: tests, the Feature Tour, `CHANGELOG.md`, the retro row, `docs-auditor`, `tools\backup.ps1`, the release's own audit and sweep; `UI_HUD2_EXTENDED.md`'s "Left native" marked superseded.

## 16. Still open before the build
**The owner's:** the go, after 0.3.5.

**Decided (the owner, 6 Nov 2026): the targeting display, 6.10.** It replaces the native bracket, label and bars on screen, and closes what they reopened:
- 3.1 and `UI_HUD2_INPUTS.md` 3: `Targetable_Objects` stays a kept root; below it three part rows, as 6.10.
- The cue's dark cells stay, the owner's choice; the label they answered is gone but for a kept diamond (fallback of 6.10).
- R13's exception stands: the card keeps name and distance (the owner).
- Section 14's target label over the card is moot: the native label goes.

**Decided (the owner, 6 Nov 2026):** R12, the grammar, in section 4. The travel budget: fix the metric, step 3 below `hud_size`: `hud2 lint` reports **steady** (after `hud_fade`) and **awake peak** apart, and the budgets are read against each. R4, R6, R8, R9, R10, R13, R16, R17 and the reticle's bottom: `UI_HUD2_CRITIQUE.md` 7, third table; written in at 4, 6.2-6.5, 6.9 and `UI_HUD2_INPUTS.md` 7b.

**The game's, by probe (step 1):** whether a lock can be had at all (48 samples, none); whether hidden map controls still take clicks; the dialogs' caption routes; `NN_Inventory`'s growth; the load's place name on each path; for the new map: the label flag on every view, the overlay's transform through pan and zoom, the objects' "known" flag.

**Stale here on purpose:** the design's rows of `UI_HUD2_EXTENDED.md` stand until step 12.

## 17. Build state of steps 4, 6, 7, 9 (6 Nov 2026; branch `build-040-skin`; written before the games: what they showed is `docs\RELEASE_0.4.0.md`)
Built to the unit tests and the release build only. Files: `skin_rules.h` (tested), `skinkit.*`, `skindialog.*`, `skinmenu.*`, `skinload.*`, `skinover.*`; `uiskin`, `windowtext`, `winplates`, `padmenus`, `padbases`, `router`.

**What the code found against this design:**
- **The dialogs draw over the UI pass, not under it** (`uiskin::DrawModal`, before the lane): anything under the UI pass lies under the buttons of the screen behind, which the ornate box used to cover. So every word on a dialog is read and drawn by the skin. A dialog whose words cannot be read stays whole native: **NN_Prompt, so the MP prompt, stays native** until a reader for its rich text exists.
- **No native progress exists** (6.4's "from the native progress"): the bar's fill only wraps. The bar shows that fill held at its highest, so never a loop; on a long load it fills early and stands.
- **Abort mission is no dialog** but the map's controls (`FREELANCER_RE` "Dialogs"): its row stands in `skin_rules.h`, undrawn; it belongs with step 5's frame. **No ship purchase confirm was found.**
- **CONTINUE runs through the load screen** (the quick load 0x5B2C70 brings rooms up without characters), so it costs that screen's slide, about 2.4 s.
- **Alt-F4 and the menu's EXIT are one object:** told by the main menu behind it.
- **Large:** `windowtext` ran only at game text 100 because its close put the rich table back to 100. It now returns to the size applied, and runs at any size.
- **The bumpers docked, in one line:** a window that binds them keeps them (dealers SIDE, log, map); the strip of step 5 takes them on NeuroNet windows; else the room bar.

**Owed in game, pass criteria in brackets:**
| Check | Pass |
|---|---|
| Dialog probe, one per class: Quit, Delete (front and in game), Exit (menu, Alt-F4), NN_Error, please-wait, multiplayer's leave question | `uiskin windows` lists the family with art hidden; verbs drawn on the boxes; click, A, B, Esc each give the game's answer; `fell=0`; `uiskin restore` brings the ornate box back |
| Text read: `TextOf` on `ExitText` against vt[0xA8] message 0x1F | same string |
| Dialog at 1280x800, 4K, 21:9 | plate centred on the chips, inside the screen, title and cost uncut |
| NN_Prompt: find `PromptText`'s string; name the captions of `PromptYes / No` | then its row can be written |
| CONTINUE: no save; a save; a speedrun save; the five buttons at rest and in the slide | no chip and five native places; one press lands in the save with characters in the rooms; the mode shows beside the chip; `uiskin state` placed=1, captions and hits follow the buttons, leaving the menu puts them back |
| The save list 0x67E970 against `saves` | same names, newest first by file time |
| Loading, each path: menu load, in-game load, New Game, MP connect, launch, dock | navy from the first frame, no native bar flash, black frames between (probe 9) none or fill by 0x578F70; bar only past 1.5 s; load time unchanged within noise |
| `uiskin loadplace 1` on each path | says which paths read the right place; only those go live |
| Cursor: each of the 19 names, K&M, pad's cursor, FLUF beside, options page, modal loop | shape at the game's hot spot, the mesh gone, a mod's name logged and native, no cursor in a movie |
| ToolTip: read `+0x7C` while hovering (`uiskin hover x y` in a background game) | same text as the frame; else it stays native by itself. Done 11 Nov 2026 |
| Rollover | its text at +0x7C too (FREELANCER_RE "The hover help's fields"); read and drawn since 0.4.6 |
| Popups, front load | MMOkButton's and the load buttons' frames gone, hit and click kept |
| Page rule on every `.page` screen | art with a rectangle has a plate, no unknown class hidden, `seams` lists what is left |
| `uiskin seams` at every moment of 3.1 | zero, or each line owned by a row |
| Base text at 800: `gametext window` `over=` on all goods | names at the fixed rung; each overflow read from a frame; issue 22's test holds |
| Large at 125 from a start | text by role, rich table back at 125 after a window closes |
| The frame: log, status, info | top line and left edge equal within 2 px, clicks land, no visible jump too large to keep; else hook 0x55E120 first |
| Probe 18: `gametext frame 1`, then NN_Inventory | every row, filter and scroll hit box follows; its growth by pitch and font is still to build after this |
| R16: focus on BUY and SELL, tap | one trade, focus back on its row and selected, lane BUY on the button, HOLD on the row, one focus block; the D-pad reaches the button from the top row |
| Delete by X, front and in game | one press opens the confirm, focus on KEEP SAVE |
| HUD 2 off, `[Modules] hud = 0`, `skin_native` each label | frames equal classic; every write restored |

**Decided in the owner's absence (6 Nov 2026, the AFK order), each for the owner to overturn:**
1. Alt-F4 in a game: title EXIT FREELANCER?, the quit's cost, EXIT ANYWAY / KEEP PLAYING, as built: the table's "as quit, EXIT".
2. **Enter follows the focus** (`skindialog::EnterKey`): a word in focus that Enter ignored would lose progress on a risky dialog; the safe answer is the rule of 6.2. Owed: the raw key reaches the chain inside the dialog's modal loop.
3. A latest save of another mode: CONTINUE loads it and says the mode beside the chip, as built: D9 says "the latest save".
4. Past 1.5 s the held fill stays: R17's form has a bar, a slow machine still sees something move once, and it never loops.
5. The rung at 900 lines and under only: the frames at 4K decide in phase 2; nothing grows without a reason read from a frame.
6. A name too long for the rung wraps: panels grow first, abbreviation last, and native rows have no marquee.
7. **One scrim, 78 %, for all five windows:** R8's "one scrim", at the number the owner set for the map.
8. The fall line stays THIS WINDOW KEEPS THE GAME'S LOOK: event, plainly, and it is a rare line.

**Settings** (`[Controller]`, ini names unchanged): `ui_style` stays the master. New: `skin_motion` (0 full, 1 fades, 2 none), `skin_native` (hidden, empty). The map has no switch of its own: `skin_native = navmap` gives the old one.

## 18. Panels fitted to what they hold (6 Nov 2026, the owner: no overflow, no spill, no waste, no needless asymmetry, one mechanism)
`core\ui\winfit.h` is the pure rule, `tests\winfit_test.cpp` its states; `winplates.cpp` feeds it the window's real controls every 6 frames once the window stands still, and only under the skin's own plates. Debug: `gametext fit` lists each fitted panel and the air round its content; `tests\docked_text.ps1` writes it to `fit.log` at every step.
- What is measured: text by its top and its laid-out height (the game draws line under line from the control's point, 0x4143C0), rich text by what it fills, buttons by their frames, the cargo bar. No other art, no scroll bar. A text's left depends on its alignment, so it counts on the vertical axis only.
- `Hug` ends under its content plus 0.018 H; panels side by side within 0.05 H end level; `Level` is a neighbour whose content cannot be read (the ship's 3D view); `Under` takes the room the row leaves, its foot stays, and its text boxes go with it past 0.004 H (`FitBoxShift`). Edges travel at 2 H a second; the first fit of an opening does not travel.
- Fitted: dealer, ship dealer, job board, offer. Not fitted, because their content is not all controls: Options' pages, the menus, repair's rows, the news ticker, the root-framed NeuroNet windows. These keep the table's numbers.
- A text with no role that is wider than its box steps down at most three rungs, never under `kRowFloor`, and stays down for the opening (`Loose`). That test is the same for every alignment: a text no wider than its box cannot leave it. Window headings are left alone.
- The infocards carry no rule: a long heading wraps and its second line stood on it.
- A column of prompts checks the place it moves to, not only the one it left: it touched the transfer panel.
- Rows in frames (`kFramed` in `windowtext.cpp`): every list row in the dealers, the ship dealer, the inventory, the job board and the status window, and the transfer row, is placed by the frame as it stands, by measure each third frame: the first line under the frame's top, the others on its foot, one line alone in the middle. The game's offsets are for its own fonts: a larger price crossed the frame. A row's width room is its frame's.
- Whole lines: a rich box ends on a whole line (`kRichLine`), never through one. The job's story grows to its text as far as the lane; the log and the info text start under the title's rule.
- The log and the info window stand 0.15 H taller than the game's (`kTextWindowGrow`), the window higher by half of it so it stays centred; the log's MISSION and LOG buttons go to the new foot by where they stand, so a window the game keeps is never moved twice. So do the death / mission-failed screen's buttons with their captions (7 Nov 2026, the owner's frame: 0.4.0 left them at the game's height, over the taller window's text; `StretchFoot`, each caption keeping its place on its button; verified at 4K by frame and a native click of LOAD AUTOSAVE). NN_Inventory stands centred too.
- `gametext fit` also lists every text out of its frame or box (`over frame`, `over box`).
- Skin words count as content: uistrip reports each word it draws (`AddSkinItem`), cleared by the frame that fills the list, never between that and the measure. A blank line counts too: the text object's +0x24 leaves it out, so a text's height is line pitch x lines (`TextHeightPx`). A state change (another set of plates) starts from no offset and fits that frame; panels are measured every frame.
- The ship dealer's side panels start at their headings: the X stands on the headings' line (`StandShipHead`), the table's numbers are from there, the centre panel `Match`es the tallest list, the infocard rises. One refusal: HUD 2's line, the game's `ErrorHelp` hidden while it stands. Buttons move by the position property, checked to stay on their column (`MoveButton`): vt[0x14] threw the X 1000 px left.
- The room bar's underline stands on the current room's button, found as the game finds it (`CurrentRoomButton`, hotspot label = room nickname); the sub-row was a stand-in that slid. A sub-row button far from it is the room before's. No focus on the bar for 0.3 s after it moves (`BarBusy`); the focus hit test runs every frame. `gametext bar` reads the bar's steps frame by frame.
- Prompts: a column's room runs to the screen's edge where HUD 2 centres it in its gap; a pill still too wide stacks its glyphs over its word in two slots (`LanePlaced::stack`); the hold never keeps pills on a panel (`Blocked`); a row stays at the right edge, the strip's mirror, when something stands at the band's left. Until a dealer's X reaches its column its plates are not painted, and the prompts keep clear of where they will stand.
- Refusals: the dealers show HUD 2's amber line as the ship dealer does (`DealError`): NEEDS LEVEL n, else the game's words, at the largest size that fits between the panel's top and the item. The white line is parked off the screen (`windowtext::Park`). The ship's refusal and trade-in stand in the centre panel at one size, the largest the longest line leaves inside it. A panel with a rule ends no higher than the rule (`least`); `Match` counts what it can read (SELECT SHIP).
- The pad's stick scrolls a rich text by whole lines, so no line stands cut at the box's edge. Status and Repair are not fitted.
