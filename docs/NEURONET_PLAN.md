# The NeuroNet windows as full screens (plan and record, 7 Nov 2026)

**The owner, 7 Nov 2026:** "overhaul all other neural net screens with the same standard" as the nav map; "list out the Neural Net tabs, research how to improve each one to the level of the NavMap, and then implement." Research: `docs\research\neuronet-screens.md` (37 pages, online), `docs\research\neuronet-windows-facts.md` (what each window is in the game). The map: `docs\MAP_PLAN.md`.

## The tabs

| Tab | The game's window | What it became |
|---|---|---|
| LOG | NeuroNetStoryStar | Full screen, title, MISSION / JOURNAL as tabs, a RIGHT NOW card with the live objective (only with one), the game's text in a reading column; Y shows the map |
| MAP | NeuroNetNavMap | Done: `MAP_PLAN.md` |
| INVENTORY | NN_Inventory | HUD 2's own list of every row of a filter (five rows show at a time, the list scrolls), the game's five filters as tabs in the game's words, the game's card on the right, cargo and credits under the list; A mounts, Y jettisons |
| STATUS | NN_PlayerInfo | Rank, worth and the next level large, the game's other ten numbers as rows; every faction on one list with a bar from the middle, the game's marks at -0.6 and 0.6, its five standings in words; X filter, Y sort, A the faction's card |
| INFO | NN_Help | Full screen, title, the game's text in a column. The window is the target's or the system's card, not help |
| CHAT, GROUP | ChatControl, NN_GroupControl (multiplayer) | The same ground and title round the game's window. **Not seen:** no multiplayer run was made; GROUP is an offer dialog, no member list exists in the game |

## How it works

**One mechanism (`modules\hud\src\nnpage.*`, rules `nnpage_rules.h`).** The game's window stays open and does the work. HUD 2 draws an opaque page over it, with a hole where the game's text is wanted (`Cover`); nothing of the game is hidden or moved by writes, so nothing needs restoring. Where the window has to stand elsewhere, `windowtext.cpp`'s table moves it (`shift`, `grow`).

**The game's calls (`core\flexe.h`, read in Ghidra, used in game):** the list of a filter comes from the window's own answer, row by row (`InvSelected`: 0x47E790 at +0x3A4 = k), so the filter and order are the game's; the page's focus is the game's selection (`InvSelectRow`, so its card and JETTISON follow); `InvMount`, `InvSetFilter`, `StatusFactionCard`. STATUS reads `PlayerInfoHeading / Stat n` and the pairs at +0x410.

**Pad:** `padbases.cpp` `UpdatePage`: while `views::HudPageOn` the presses go to `nnpage::Pad`, the lane shows `nnpage::Prompts`. RS scrolls the game's text, the D-pad too (XAG 112: never only an analogue stick). LB / RB stay the strip's, B closes or backs out. The jettison panel stays the game's and the old pad path.

**Keyboard and mouse:** rows and tabs click, a second click on a faction opens its card, the wheel scrolls lists, the actions are chips in the lane, one line says what the mouse does.

**Rules from the research taken:** a single item's jettison asks once more by a press, never a hold (XAG 115); tabs never shift; a row of lead in lists; the band in words and place, not colour alone; more than one way through a long list (filter, sort, page by D-pad left / right).

**Steps aside:** the log after a death or a failed mission (its load buttons stand: `LoadAutosaveButton`); a dealer's or a job board's frame (`nnstrip::Wanted`); `ui_style` classic.

## The refinement pass (the owner, 7 Nov 2026: "avoid having tons of blank space", "no single screen feels worse or less useful than its vanilla FL counterpart")

Set against the game's own windows first: the first pass had lost the commodity row's best prices and every row's slot line. What stands now:

| Page | Added |
|---|---|
| INVENTORY | Three columns. Rows carry what a gun does (hull and shield a second, range; `baseinfo::GunOf`); the middle is the ship's own 3D view, large (`hudvis::DrawShipAt`, drawn after the pass's last draw because the view's usual pass lies under the page's ground; in space only: docked there is no status panel), and the loadout: guns, hull and shield damage a second, energy draw, cargo, credits. X BEST PRICES on a commodity (the game's `TradeRouteButton`). The jettison panel says how many of what. Added 7 Nov (`BASE_ROOMS_PLAN.md`): five rows show at a time and the list scrolls; empty hardpoints are rows (A mounts the first hold item that fits); the selected item is compared with what it would replace (guns, shields, thrusters, mines, countermeasures; green better, red worse); the game's icons stand in the rows; in space the selected mounted item pulses on the ship view |
| STATUS | Three columns: the numbers, the factions, the focused faction's card, always up: moving the focus shows it, no press to open or close (the game took two). Under the list its standing and how far hostile is. The card's close button is hidden. The rows grow to the room |
| LOG | The text from under the tabs to the prompts; left column RIGHT NOW (says so when there is none), time left when a mission counts, system, zone or base, ship, then the places near |
| INFO | Left: system, zone or base, what the player has found in this system by distance (the target menu's test). Under a short text: the ships near, by faction, in the colour of their standing. Added 7 Nov: docked, the base, what it offers, who keeps an office (standing colours, weight) and the job board size; in space, who flies in the system with standing and share, found places with where gates lead and what bases offer |

**The pause and the server:** the status window's numbers and factions are the in-process server's answer, and the pause stops that server: found in a frame, every number blank. The world runs on until the answer is in (about a second on that tab), then stops.

**The map, the same pass (the owner: "as quick as the original", Shockwave: "big McLargeHuge"):** on the mouse the selection's card opens beside the click, as the game's popup did, so SET WAYPOINT and BEST PATH are a short move away; the right button sets a waypoint in one click. The ship, waypoints and mission marks (record types 2, 3, 4) are held near the game's size at every zoom (`navnew.cpp` HoldMarks). Under the NeuroNet's tabs the prompts take two rows at the bottom sooner than a column (`promptfit::g_rowsFirst`).

## Pause (7 Nov 2026)

`core\host\input.cpp` `GamePausedHook` detours `GamePaused` and answers true to the world update and `FrameUpdate` only (by return address), so the world stops while the NeuroNet window stays open; the game's own pause would close it. Setting `nn_pause` (default 1; 0 keeps the game going). Conditions: single player, in space, not under the autopilot (it keeps flying). PAUSED shows. Verified in game: contacts freeze and resume. Not tried: multiplayer.

## Verified, and not

Background game, instance 7, virtual pad and `nnpage press`, frames at 4K: all four pages in space and docked, in the pad and the keyboard-and-mouse forms; filters; mount and unmount with the selection following; a single item's jettison; the faction card and back; B closing. Also at 1280x800 (the instance's PerfOptions, restored after): the map and the pages. **Not tried:** a real mouse wheel, real clicks on rows and the real right button (a background game has no real mouse: the map's card was driven by `navskin click`); the real pad; CHAT and GROUP; non-16:9 but 16:10.
