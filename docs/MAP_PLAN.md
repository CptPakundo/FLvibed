# The nav map, rebuilt like a modern game's (plan, 7 Nov 2026)

**The owner, 7 Nov 2026:** "send an agent to research how modern AAA games do their map screens, and I want you to replicate that behavior in FLvibed.
Much of it will require ground up work, so make it so." Then, on the research's scheme against the tap / double-tap rule: "Go for research best
practices." The research: `docs\research\map-screens-aaa.md` (19 sources read; section 4 is the scheme, section 5 maps every action to the game's own
mechanism, section 6 the open questions). Today's map: the game's own map with FLvibed's pad layer (`modules\controller\src\impl\padmap.*`), HUD 2's
frame and words round it (`modules\hud\src\navskin.*`) and the full-screen map (`navnew.*`, on since phase 1; what follows is the plan as it was written, the sections from "Phase 1" on are what stands).

## The scheme (research 4, the owner's word) and where it stands

| Input | Action | Phase |
|---|---|---|
| A | select the object at the cursor and show its card; A again on the selected object = best path to it | P1 |
| X | add a waypoint for the selection; on a selected waypoint: remove it | P1 |
| X hold | clear all waypoints | P1 (was already) |
| Y | the full info card of the selection | P1 (the game's card for now) |
| Y hold | TRAVEL: the autopilot flies there (was Y tap; FLvibed's own, not in the research: kept, moved to the hold) | P1 |
| LT / RT | zoom out / in toward the cursor, continuous; past the widest -> universe | done, not on the owner's pad yet |
| RS | pan; the cursor stays put on screen | done, not on the owner's pad yet |
| LS | cursor with a speed ramp, slowing over objects (magnetism Off / Low / High) | P2 (needs the objects' screen places: the map's list at +0x400) |
| LS click | destination list (View today) | P2 |
| RS click | recentre on the ship | P2 |
| D-pad left / right | filters Physical / Political / Patrol / Mineable | P2 |
| D-pad up / down | step through the objects on screen, nearest first | P2 |
| Menu | legend | P2 |
| LB / RB | the hub's tabs | P2 (system / universe today) |
| Touchpad click | View (done: b691de25) | done |
| Touchpad one-finger drag | pan, 1:1 with a short glide; never the only way; `map_touchpad` | done on a virtual DualSense, not on the owner's pad yet |
| Touchpad two-finger slide | zoom about the cursor; never the only way; `map_touchpad` | done on a virtual DualSense, not on the owner's pad yet |
| Mouse | hover card; LMB select, again = best path; RMB waypoint +/-; wheel zoom to cursor; drag pan; Space recentre; Tab list | P4 |

**P3:** HUD 2's own info card and hover card (the game's card is hidden since 7 Nov 2026: bug B6 fixed that way). Needs a traced selection read: the root's +0x90C held 0x45AD6CA6 for two different selections, no record, no object id. Since 7 Nov 2026 the pad's A
logic reads the selection from the selection's own buttons Set / Clear Waypoint (standing = "something"; NOT the info card's title, which once made names
what the cursor is over, and NOT Best Path, whose visible bit outlived a selection); the popup comes a frame or more after the click, so the outcome is read over 0.6 s. An object id
still needs the trace. The game's waypoint list is `NavMapMissionObjective1..n` (siblings in the map root, vt 0x5E6084, 278 x 130 px half-size at 4K): navskin
moves it under the VIEW panel by its position setter (+0xA8 property 6) in UI camera units (pixels per unit from `ui::Project` at its depth). OPEN: the move
held once (662 -> 777 px, the card not yet made) but not later: the game stacks the list under its (hidden) info card (853 px) and re-lays it out, so a
one-off move is undone (the owner's release game: still over VIEW). DONE the owner's way, "HUD 2's own list": the boxes, `NavMapMissionObjectiveText1..n`
(TextControls, "Player Waypoint 1\nNew York System\nSector D3") and `NavMapMissionObjectiveScroll` are hidden; navskin's ROUTE section under VIEW shows the
shown texts. Open: the game shows a page of three; more waypoints need HUD 2's own paging (the game's scroll bar is hidden). Open: the grid's
`NavMapGridNumber1..8` and the system name stand lower than the grid once waypoints exist. **P4:** the grown map
(`navnew`) by default, both scales in one view, keyboard and mouse parity, settings (magnetism, touch pan, hold-to-clear), the living spec.

## P2's design: zoom and pan (7 Nov 2026, before building)

**The finding that decides it.** The game's map has fixed zoom *steps* only in its buttons. Its transform is continuous: decompiled 0x496600 (places every record), 0x4963D0 (the
background meshes), 0x492410 (screen to world, the hit test) and the labels' 0x499C20 all read the zoom Z = +0x3F8 and the centre C = +0x46C / +0x470 in view 2, and the game's own
1.5 s zoom runs Z through every value from 1 to 8.6 (0x49F1A0: the target is +0x4A8 x 0.11627907, so Z = 8.6) from whatever Z it finds. View 2 also has a native mouse drag
(+0x7D4 / +0x7D5, the frame function 0x494920 at 0x4957E4). So the grown map is **not** needed for this: FLvibed drives the game's own Z and C.

**What renders what.** The game's map keeps everything: backgrounds, zones, lanes, patrols, records, labels, the selection and its popup, the hit test, waypoints, best path, the universe view.
FLvibed adds only `core\game\navview.*` (the writes, the game's placement calls) over `navview_rules.h` (pure, tested); `navnew` stays off and stays P4's. Core and not the HUD module, because the
classic map with `hud = 0` zooms too and P4's mouse will use the same calls.

**The zoom.** One value, Z from 1 (the whole system) to 8.6 (the game's sector level), exponential, so the same trigger pull feels the same at every depth: analogue on LT / RT, about 1.2 s
from end to end at full pull, eased, toward the cursor (the world point under the cursor stays under it). Z above 1 is view 2; back at Z = 1 the map returns to view 1 with C = 0, where the
game's own view-1 rules hold. Past the widest: LT held on at Z = 1 for a moment (a detent, so a zoom out never overshoots into the universe) presses the game's own universe button, its
animated fly-out. RT in the universe is what it was (select the system under the cursor, the game's zoom into it). **The universe itself does not scale**: its background mesh +0x3A4 has no
placement call and 0x492410 ignores Z in views 0 and 1, so the universe is the widest level, reached and left by the game's own animation.

**Pan.** RS, in screen terms (full tilt about 0.9 of the viewport a second, whatever the zoom), the cursor stays where it is on screen, so the world slides under it. C is clamped to
(box / 2)(1 - 1 / Z) per axis, tighter than the game's own drag clamp, so the map's edge never leaves the viewport. RS at Z = 1 has nothing to show: it does nothing.

**Touchpad** (any pad SDL reads with a touchpad; a new host read, HostApi v42). One finger down and moved past a small dead zone = pan, 1:1 (the pad's width = the viewport's width), with a
glide of about 0.25 s on release. Two fingers = zoom by their common vertical slide, up = in, about the cursor. A touch without movement does nothing; the click stays View. A finger that lands
while the pad is clicked is ignored until it lifts. `map_touchpad` (default on) switches every gesture off; each one has its stick route. Only an open map reads the fingers.

**Grid.** 0x49FBF0 places the grid's letters and numbers with a fixed 8.6 in view 2: FLvibed places them by the same formula with the live Z, through the same property call.

**Backgrounds (found in frames, 7 Nov 2026).** The sector's background is a blur of fine lines far below Z = 8.6, so view 2 keeps the system's background under it (+0x4A4 = 1) and fades the sector's in from Z = 2.5 to 6.

**Prompts (the owner, 7 Nov 2026: "prompts that are really self-explanatory"; removing a waypoint had none).** Each says what the press does here and now: A SELECT, or PLOT ROUTE where a press would plot one; X ADD WAYPOINT, REMOVE WAYPOINT on a selected waypoint, CLEAR ALL WAYPOINTS (hold) with nothing selected; Y FLY ROUTE only with a route; RS MOVE MAP only when zoomed; RS click names its destination; RT OPEN SYSTEM in the universe. What cannot happen now is not offered.

## Phase 1 of the rebuild (the owner, 7 Nov 2026: "phase 1 is go"; `docs\research\map-screens-rebuild.md` 7): built, frames on instance 7, not on the owner's pad yet

**The screen is the map.** The viewport is the whole screen, centred on the old square, so the system stands where the map stood and runs under everything when zoomed. No frame, no backgrounds (their alpha is 0), near-black ground (`skin_rules.h` kScrimFullMap), ticks and sector letters drawn by HUD 2 (`navnew.cpp` DrawSectors), HUD 2's labels and marks, one yellow route line. The game's objects, lanes, zones, selection and hit test stay. The universe keeps its art in the square.

**Read in frames, not to be undone:** the map's camera keeps its field of view, so a box unit is port W / the game's box W pixels on both axes, and +0x4A8 / +0x4AC must grow by W x W / (port0 x square) and W x H / (port0 x square) (`navnew.cpp` Grow). The objects stand 0.854 / 0.769 of the way to the box's edge at +0x4A8 (the game's labels' 0.2 and 0.18 over 0.2342). Lanes follow Z and C only in view 2, so the full-screen map stays in view 2 at Z = 1 (`navview::Settle`).

**The screen's furniture (the owner: "side panels only contextually appear as needed", "button prompts that start a flow"):** the system's name top left; the NeuroNet tabs top centre on every NeuroNet window, sliding in from the flight HUD's NEURAL NET over 0.3 s; the left card only with an objective or a route; the right card only with a selection; the layers' card only when asked for. The bottom is prompts: how to move about on the left, what to do at the right edge (a keep-out in the lane holds the bar there, as the strip did).

**Every button the map may use (the owner: all but the bumpers, Menu and View):**

| Input | Action |
|---|---|
| LS / RS / LT, RT | cursor / move the map / zoom |
| A | select; again = plot the route to it |
| X | a waypoint at the cursor, chained, selection or none |
| X hold | remove the waypoint the cursor points at; with none there, clear all |
| Y | fly the route (NO ROUTE YET without one) |
| B | close (in the layers' card: close the card) |
| D-pad left | the destinations' card, on the left |
| D-pad right | the layers' card, on the right |
| D-pad up / down | the next object, nearest the cursor first; the cursor goes to it |
| RS click | Sirius / back to the system |
| LS click | find me: the ship's system, the ship in the middle |
| View | closes the map (the NeuroNet's button); LB / RB the tabs |

**The mouse (the owner: "HUD 2 does support K&M as well"):** the wheel zooms toward the cursor (`navnew.cpp` MouseFrame), the game's own drag moves the map (`navview` rescales its step), LAYERS is a chip at the top right, a selection's actions stand on its card, and one line says what the buttons do at the lane's left. Right-click waypoints: not built.

## The owner's notes after the first pad test (7 Nov 2026): the work in hand

Measured first: the camera is right and any transform sum is wrong. `navview::Project` (the map camera +0xC80 vt[0x34], stdcall(this, float* x, y, z, const float* in), at the box place of 0x496600 with depth -0.55 + (C + D / 2) Z) gave the sun within 5 px at Z 4, where the sum was 100 px off. Replaces "Read in frames" above: scale +0x4A8 / +0x4AC = box x PixelsPerBox / (square x 112000 / 131072), one scale on both axes; `navview::Store` holds +0x474 at -D / 2, depth -0.55, the plane the hit test 0x492410 unprojects to.

| # | The owner's note | State |
|---|---|---|
| 1 | Click to find me does not work | **Fixed, reproduced (instance 7):** two causes. At Z = 1 C clamps to 0, so `CentreOn` comes in to Z 3 and centres by the camera, the cursor on the ship. And on the owner's pad it stayed dead: after Sirius and back `NavMapShipButton` stands over the ship's own system, and the press clicked it (the owner's log). `navview::ShowsShipSystem` decides now, from Sirius too. Not on the owner's pad |
| 2 | Stations and rings not robust: Newark will not select, Storage Depot will, though the tooltip names Newark | **Fixed, repro + frames:** Newark *was* selected. 0x49EA50 gives a click that hits an object's radius Info, Select Object and Best Path and NO Set Waypoint; one that misses (the small depots, the rings) gets Set Waypoint; FLvibed read "selected" off Set / Clear. Now off the zoom button every popup shows. A is one click: the map's own, A owned over the canvas. A again replaces a plotted route (Best Path is hidden while one stands). X on an object adds through `space::AddWaypoint`, on the object |
| 3 | LS and RS panning too slow | **Built:** `kPanRate` 0.9 to 1.5, the map's cursor x 1.5 (`padmap::kCursorScale`), the cursor pushed on in the outer 6 % moves the map. Numbers for the owner's pad to judge |
| 4 | Marks do not stick to objects, parallax | **Fixed, frames at Z 3:** marks on the models by `navview::Project`; the planet's within 30 px of its centre at 4K |
| 5 | D-pad prompts show when they do nothing; up / down should snap to known destinations at once; left stays the sidebar's toggle. Prompts show / are not grey only when they do something | **Fixed, frames:** `views::HudMapList` tells navskin; rows that do nothing are taken out and the rest close up (layers open: no D-pad rows; list open: no MOVE CURSOR and no D-pad rows; RS and the triggers go on moving the map beside it, the owner's pad, 7 Nov 2026). Up / down worked all along: a D-pad press hid the pad's arrow (padbases `g_cursorShown`), so the walk looked dead; the map now keeps the arrow. After the second pad test: up / down walk the found destinations, nearest the ship first, the map centred on each and the cursor on it; the list's rows do the same |
| 6 | Mouse scroll does nothing | **Fixed in code, a real wheel not tried:** the page drained `RealMouseWheel` before the map read it, the map divided whole notches by 120 again, and only K&M read it (a wheel alone does not make the device K&M). The drain leaves it while the map is movable; both routes read it |
| 7 | LMB select clears when the mouse moves: buttons unreachable, a loss against vanilla | **Built, the real mouse not tried:** `navnew::Frame` holds +0x92C at 0, so the popup stays until the next click replaces it. Newark's fix (2) puts the card up for the big objects too |
| 8 | K&M: limited ship control and weapons fire with the map open | **Fixed, key presses refused in game (W, Tab); the mouse button not tried:** `input::SetFlightLock` (HostApi v43), asked each frame the grown map stands: `fl::FlightCommand`s answer no key press (HandleKey) and no poll (IsCmdHeld: the left button that clicks the map also fired the guns). What FLvibed holds or fires itself goes on: the autopilot flies under the map. The second pad test: mouse flight still steered; `SteerHook` now reads the cursor as the screen's centre under the lock. **Written, not verified:** mouse flight needs K&M as the device, which a background test cannot set |

**OPEN, the owner, 7 Nov 2026: NeuroNet windows pause single player by default, an option in the HUD module to keep the game going. NOT BUILT.** Tried: the game's own pause, 0x42D6F0 (bool on: the count 0x667D54, then the pausables' list 0x667C70, vt 0x68 / 0x6C), the pause menu's call. It closes the NeuroNet window at once, whether called during the open or after it (`nnstrip open` falls to 0, instance 7), and the UI code reads `GamePaused` to stand down (0x4BB390, 0x4BB680, 0x4BC990; those pass only with a `NeuroNetMissionMessage` control up). The world's update 0x54B000 has its own `GamePaused` gates round PhySys::Update, so a pause of the world alone looks reachable there, by a hook: the next try. Check mission timers and the server side before calling it paused.

**OPEN, the owner, 7 Nov 2026: "We may still need to rebuild the map, because it's too small and looks archaic."** The owner's scope for the research: "the WHOLE deal: How maps look and how they control, and the functionality they offer". The size, the look and the feature list are the next decision; the research: `docs\research\map-screens-rebuild.md`.

## Rules
- The game's own calls do the work: selection, Set Waypoint / Clear Waypoint / Clear All / Best Path buttons, `space::PlotBestPath`, `exe::AddWaypoint`.
- Every action has a button route and a list route; nothing only on the touchpad (research pattern 10).
- Each phase: built on instance 7 by `deploy.ps1 -Impl`, proved by frames and the log, the release build last; the owner's real pad before a phase is called done.
- Open for the owner (research 6): Y's travel on the hold; hold-to-clear or a confirm; magnetism default; the hub's tabs on LB / RB.
