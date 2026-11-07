# Map screens in AAA games: controller-first maps, and a scheme for FLvibed's nav map
Status: current
Checked against: FLvibed e7fd4b63 (7 Nov 2026); HDE 0.7.1 test install (not run: desk research only)
Dirty paths at check: not listed (research-only round; this note is the only file written)
Watch: modules/controller/src/impl/padmap.*, modules/controller/src/impl/padmap_zones.h, modules/controller/docs/BINDINGS.md, docs/UI_PLAN.md#nav map
Evidence: none saved (text sources only, no frames; no video pages opened)
Sources: [page] = fetched this round and read; [snippet] = seen only in a search-result summary, page not fetched or refused;
(memory, unverified) = from model knowledge, not checked against a source this round. Earlier map-screen work:
`hud-skin.md` "Map screens" (its reader / verifier notes in `research\evidence\hud2_skin\02_*`).

Not done in this round: `IDEAS.md` and `README.md` were not updated (the round's brief allowed one file only). The working
session should add the index row and merge section 6's items as ideas.

## What it means for FLvibed

1. **The modern pad map is a free cursor on the left stick with friction, not snap.** Every documented map of the
   last decade (Elden Ring, Starfield via its D-pad pan plus cursor, Spider-Man 2, Breath of the Wild / Tears of the
   Kingdom, No Man's Sky) puts a free cursor on the left stick; precision comes from slowing the cursor over an object
   ("stickiness" / magnetism), not from hard jumps between objects, which players call jarring (section 3). FLvibed's
   `padmap` already drives the game's cursor with the left stick; its magnetism sees only FlControls (the map's buttons),
   not the map's objects (`modules\controller\src\impl\padmap.h` header comment). **The rebuild's first job is an
   object list for the canvas (FLvibed-drawn overlay or read from the nav map's private list) so the cursor can feel
   its targets.**
2. **Triggers zoom; the right stick (or D-pad) pans; a stick click recentres.** Starfield (RT / LT zoom, RS click reset
   camera), Elden Ring (RS up / down zoom, LS click centre on the player), Everspace 2 (RT / LT zoom [snippet]). Zooming
   out of a system into the universe and back in should be one continuous zoom (Star Wars Outlaws and Starfield step
   through their map levels by zooming out / backing out), with B as "up one level".
3. **One press to act, a hold only for what is destructive or a commitment.** Elden Ring: A places a beacon, X opens
   the marker menu; Starfield: X sets the route; Spider-Man 2 makes fast travel a hold, on purpose, "for player
   usability" (Insomniac, section 2). For Freelancer: **A select (and card), A again = best path** to it, X add / remove a
   waypoint, X hold clear all waypoints.
4. **A destination list is the non-cursor route to every target.** Elden Ring's Sites of Grace list (Y), X4's object
   list and search, No Man's Sky's "Quick Navigation", Elite's search; FLvibed's `padmap` already has the list (View).
   Keep it, add types (bases, gates, holes, lanes, mission targets, waypoints) and a search on the on-screen keyboard.
5. **The touchpad is a bonus surface, never the only path.** The PlayStation games use a touchpad *press* to open the
   map (Spider-Man 2, God of War Ragnarok, Elden Ring closes it with Options / Touchpad) and swipes for shortcuts;
   panning by swipe is documented for Elite's PS4 map and Assassin's Creed IV [snippet / page]. Forced touchpad use is
   the worst motor barrier in Ghost of Yotei's accessibility review. So: touchpad click = View (the owner's mapping),
   touchpad drag = pan (an extra), two-finger slide = zoom (an extra), all switchable off, every one duplicated on sticks
   and buttons.
6. **Freelancer's map verbs map one-to-one** onto these patterns; nothing the game offers needs to be dropped
   (section 5): select, set waypoint (0x4C4780, up to 99 entries), clear one (0x4C46E0) / all (0x4C46C0), best path
   (0x4C4EB0, replaces the list, crosses systems, respects locked gates), zoom, system / universe, the five filters, a
   base list, the info card (`docs\FREELANCER_RE.md` "Waypoints", "Best path").

The recommended scheme is section 4; open questions for the owner are section 6.

## Findings

### 1. Per game (7 Nov 2026)

Legend: LS / RS = left / right stick, LT / RT triggers, LB / RB bumpers, View / Menu (Xbox), Cross / Circle / Square /
Triangle (PlayStation). "?" = not documented in what was read.

| Game | Open | Cursor | Pan | Zoom | Waypoint / marker | Touchpad | Notable |
|---|---|---|---|---|---|---|---|
| **Elden Ring** | ? (closes with B, or Menu / View; Options / Touchpad on PS) | LS free cursor [page 1] | cursor pushes the view (memory, unverified) | RS up / down; mouse wheel [page 1] | A places a beacon (the light pillar); X opens the Markers window (stamp icons), "Remove All Markers" there; one marker removed by putting the cursor on it [page 1][snippet 2] | Touchpad closes the map (PS) [page 1] | LS click = centre on current location; RS click = show underground; RB = switch overworld / Shadow Realm map; Y = Sites of Grace list with L1 / R1 region tabs and an R3 mark toggle; LB = map menu [page 1] |
| **Starfield** | Quick menu / Menu; Star Map from the scanner (memory, unverified) | LS cursor (memory, unverified) | D-pad pans (W / A / S / D on PC) [page 3] | RT in / LT out; mouse wheel [page 3] | X = Set Route Destination (Square on PS) [page 3]; hold to travel [snippet 4] | Touchpad = Open Mission Menu (PS) [page 3] | RB = Surface Map, LB = Resource View, Y = "Show Me", RS click = reset camera; B steps out system -> galaxy [page 3][snippet 4] |
| **Elite Dangerous** | dedicated bind / left-panel tab [page 5] | pad: cursor on a stick, a modifier to move the view [snippet 6] | ? | ? | Plot Route button on the selected star's card; economical / fastest; one route at a time [page 5] | PS4: the galaxy map "swiped" with the touchpad (Frontier launch text) [snippet 7] | Search by partial name centres the map; "Map" vs "Realistic" view with filters (allegiance, economy, trade lines); hover shows a System-view button [page 5]. Gamepad map binds are a known weak spot: a third-party guide calls the pad "a rather poor choice" [page 8], forum users cannot find Z-axis movement [snippet 6] |
| **No Man's Sky** | Quick menu (D-pad down / X) -> galaxy [page 9] | camera, not a cursor; target follows view centre (memory, unverified) | free camera action [page 10] | "Zoom" action [page 10] | "Add Waypoint" action; hold toward the next system to advance along a path; hold to warp [page 9][page 10] | ? | Navigation modes cycle (Next / Previous Navigation Mode: free explore, path to centre, goals), Next / Previous Filter, Current Location, Quick Navigation, Display System Info [page 10][page 9] |
| **Star Citizen** | F2 (starmap toggle) [page 11] | mouse | RMB drag [snippet 12] | wheel [snippet 12] | double-click destination, "Set Route" [snippet 12] | none | No documented pad map binds [page 11] |
| **Everspace 2** | ? | ? | ? | RT in / LT out; A OK, B cancel [snippet 13] | PC: RMB creates a waypoint, Shift+RMB chains several; the ship heads for the next as soon as one is placed [snippet 13] | ? | Sector map redesigned for "clarity and planning" (patch summary) [snippet 13] |
| **X4: Foundations** | M, or the ship menu [page 14] | mouse | LMB drag / arrow keys [page 14] | wheel / + - [page 14] | Set guidance (Shift+G) on a selected object [page 14] | ? | 3D rotate RMB drag / numpad, R resets camera; Filters tab = layers, Legend tab; Object List of what is visible at this zoom; search by name, type or id [page 14]. Pad maps rely on community Steam Input layouts (right stick as mouse while held) [snippet 15] |
| **Mass Effect (trilogy)** | the Normandy's galaxy map console [snippet 16] | the Normandy model is the cursor, LS (memory, unverified) | ? | levels galaxy -> cluster -> system, B backs out (memory, unverified) | ? | ? | ME3: moving inside a system costs no fuel; scanning there risks Reapers [snippet 16] |
| **Star Wars Outlaws** | menu | ? | ? | zoom out steps City / POI -> Planet -> Orbit -> Galaxy [page 17] | ? | ? | No minimap; a compass instead; maps reveal as you explore [page 17] |
| **Breath of the Wild / Tears of the Kingdom** | Minus, then L / R to the map tab [snippet 18] | LS cursor (memory, unverified) | sticks [snippet 18] | RS zoom [snippet 18]; zoom snaps to fixed levels with a blur transition [page 19] | A places a pin on the cursor; A on a pin edits / converts it to a stamp; the scope places pins in the world [snippet 18][page 19] | n/a | Labels by zoom level; pins are one-shot (gone on arrival), stamps persistent (6 pins / 300 stamps in TotK); D-pad switches Sky / Surface / Depths; pins on another layer are drawn differently but do not say above or below (a criticism) [page 19][page 20] |
| **Ghost of Tsushima** | touchpad press (memory, unverified) | LS (memory, unverified) | ? | ? | Cross on the map sets a marker; D-pad right picks what the Guiding Wind tracks [snippet 21] | swipe up = Guiding Wind to the tracked marker [snippet 21] | Map icons white on light grey: poor contrast [page 22] |
| **Horizon Forbidden West** | Touchpad opens the menu [snippet 23] | ? | ? | ? | ? | Touchpad = menu | Marker / route setting "oddly implemented"; no mini-map, so the map had to be reopened often; manual markers not on the compass; thin text, small icons [page 24]. Accessibility: hold / toggle per action incl. "Menu Hold Actions", Guided / Explorer pathfinding [page 25] |
| **Marvel's Spider-Man 2** | touchpad press [page 26] | LS [snippet 27] | ? | ? | ? | press = map; swipe = FNSM app / camera; a later toggle disables touchpad menus in combat [page 26] | Fast travel to any point; **hold Triangle to confirm**, a design choice, not a loading mask [page 28] |
| **Red Dead Redemption 2 / GTA V** | RDR2: hold Menu / Options = world map [snippet 29]; GTA V: Options = pause + map [snippet 30] | LS (memory, unverified) | ? | ? | GTA V: Cross / A sets the custom waypoint, drawn purple vs mission yellow [snippet 30] | ? | RDR2's waypoint display size Regular / Expanded [snippet 29] |
| **Assassin's Creed** | ? | ? | ? | ? | Mirage: custom markers placed from the Enkidu (eagle) view [snippet 31] | AC IV: the touchpad "is keyed to the map, and will let you look around, zoom in, and set waypoints" [page 32] | Map filters; a legend button (Unity) [snippet 31] |
| **God of War Ragnarok** | touchpad press: left half map, right half weapons; options Left / Right entry, Press / Hold entry, Map Only, Screen Only [page 33] | ? | ? | ? | ? | four swipe directions as optional shortcuts that work "in addition to their main input" [snippet 34][page 33] | Icon size Small to X-Large incl. map icons; UI text size to XX-Large; high-contrast HUD backing; colour presets [page 33] |

### 2. What the sources say beyond the table

- **Hold to commit, and to be cancellable late.** Insomniac kept Spider-Man 2's hold-to-confirm because "having a
  confirmation window was important for player usability" and invites players to see how late they can cancel [page 28].
  Starfield's set-course is a press, the jump a hold [snippet 4]; No Man's Sky warps on a hold [page 9].
- **Fixed zoom steps read well.** Stamen (cartographers) praise BotW's zoom snapping to fixed levels with a short blur,
  and labels that appear by zoom level; they fault duplicate labels at some levels [page 19]. In TotK they fault
  geoglyph icons that vanish when zooming in ("backward") and pins that do not encode their layer [page 20].
- **Several ways to the same target.** BotW's map, scope and mini-map all place pins; the review calls the flexibility
  a strength [page 19]. Elden Ring offers both the cursor and a list (Sites of Grace) per region [page 1].
- **Contrast and size decide legibility more than design.** Ghost of Tsushima: white icons on light grey [page 22];
  Horizon Forbidden West: thin text, small icons, no backing [page 24]; God of War Ragnarok's answer is a backing plate
  option and an icon-size option that covers map icons [page 33].
- **Touchpad.** Ghost of Yotei's review calls forced touchpad use "by far the worst" fine-motor barrier, and players
  with an Access controller without a DualSense cannot progress where a swipe is required [page 35]. God of War
  Ragnarok's swipes are shortcuts on top of their main input [page 33], the safe model.
- **Pad cursor magnetism.** Search summaries describe GNOG's cursor "stickiness" / "magnetism" (credited to FRACT OSC:
  the cursor slows over targets) and a UX Collective case that a map justifies a cursor in a focus-driven game; both
  pages refused the fetch [snippet 36]. Path of Exile players call node-to-node snapping jarring and ask for a toggle
  [snippet 36]. `hud-skin.md` reached the same "friction over snap" result through Destiny and Unreal CommonUI's
  `HoverSlowdownFactor`.
- **Accessibility guidelines** (Game Accessibility Guidelines, full list) [page 37]: adjustable control sensitivity;
  all UI reachable with the gameplay input method; remappable controls; alternatives to holds; no required simultaneous
  actions (click-drag, swipe) unless as an extra; stationary targets for cursor-precise elements; more than one input
  device; contrast, readable default font size, no information by colour alone, choice of cursor colour / design,
  resizable interface, adjustable font size; a reminder of controls during play.

### 3. Patterns, distilled (what feels standard in 2024-2026)

1. **LS = free cursor**, analogue speed with a short ramp (acceleration) and a slowdown over objects; hard snap only as
   an option. The view pans when the cursor reaches the edge.
2. **RT / LT = zoom in / out** (Starfield, Everspace 2); right stick vertical is the Elden Ring alternative. Zoom toward
   the cursor, or toward the centre with the cursor centred (memory, unverified: both exist; toward-cursor is the
   mouse-map norm).
3. **A stick click recentres** on the player (Elden Ring LS, Starfield RS reset camera).
4. **A = act on what is under the cursor** (select / beacon / pin). The primary travel action is one more press on the
   same object or a dedicated face button (Starfield X). Destructive or committing actions are holds.
5. **Hovering shows a short card** (name, type, distance, faction); a full info card on one press.
6. **A list beside the canvas** (destinations / sites / object list) with tabs or filters and a search: the non-cursor
   path to every target, and the fastest one on a pad.
7. **B steps out**: card -> selection -> one map level up -> close. Zooming out past the widest level changes scale
   (system -> galaxy) and A on a galaxy entry zooms into it.
8. **Bumpers switch views or tabs** (Elden Ring RB other realm, Starfield RB surface / LB resources; `hud-skin.md`:
   "bumpers change view or tab"), **D-pad steps layers or lists** (TotK layers, Elden Ring lists).
9. **Waypoints of two kinds**: a one-shot destination that clears on arrival (BotW pin, Freelancer's waypoints) and,
   where the game has them, persistent marks; remove one by acting on it again; "remove all" in a menu or a hold.
10. **Touchpad on PlayStation: press opens the map / menu**; swipes are optional shortcuts; touch panning exists (Elite,
    AC IV) but is never the only way.
11. **Legibility options**: icon and text size, a backing plate, colour plus shape, contrast.
12. **Avoid**: hidden map routes that force reopening the map (Horizon); markers that do not say which layer / system
    they are on (TotK); white-on-light icons (Tsushima); touchpad-only actions (Yotei); pad maps that need a modifier
    to reach basic axes (Elite's Z axis); snapping that yanks the cursor; holds without a toggle option.

### 4. Recommended control scheme for FLvibed's nav map

Principles: one map for both scales; the cursor works on objects FLvibed knows (bases, planets, gates, holes, lanes,
zones, waypoints, mission marks); every action has a button route and a list route; the game's own calls do the work
(waypoints, best path, filters).

**Xbox layout (and Switch Pro / generic by position)**

| Input | Action |
|---|---|
| View (tap) | open the NeuroNet hub on the map (the owner's 18 Oct decision, `TASK_GLYPHS_HUD.md`); on the map: close |
| LS | cursor; speed ramp over ~0.25 s; slows to ~35 % over an object (setting: magnetism Off / Low / High); hard snap as an option |
| RS | pan the map (in both scales); the cursor stays put on screen |
| LS click | open / focus the destination list (bases, gates, holes, lanes, mission targets, waypoints; by distance; search on Y in the list via the own keyboard) |
| RS click | recentre on the ship (in the universe: on the current system) |
| RT / LT | zoom in / out, analogue, toward the cursor; past the widest system zoom -> universe view; RT on a system in the universe -> into it |
| A | on an object: select it (the game's selection) and open its card; A again on the selected object = **best path** to it (replaces the waypoint list, crosses systems) |
| X | add a waypoint at the cursor / object (append); on an existing waypoint: remove that one |
| X hold (0.6 s, toggle-able) | clear all waypoints |
| Y | full info card of the hovered / selected object (the game's info text); in the list: search |
| B | back: card -> selection -> universe -> system -> close |
| LB / RB | the hub's tabs (Map / Journal / Inventory / Reputation / Info: the 18 Oct decision) |
| D-pad left / right | filter set: Physical / Political / Patrol / Mineable (the game's four), plus "All" |
| D-pad up / down | step through the objects on screen nearest-first from the cursor (the precise route), or walk the side panels (`padmap_zones.h`) when focus is there |
| Menu | legend (the game's legend toggle) |

**DualSense / DualShock 4**: the same by position (Cross = A, Circle = B, Square = X, Triangle = Y, L1 / R1, L2 / R2,
L3 / R3). Touchpad:

| Touchpad | Action |
|---|---|
| click (whole pad) | = View: open the hub on the map; on the map: close (the owner's equivalence) |
| one-finger drag | pan the map, 1:1 with a short glide on release (setting: off / on) |
| two-finger vertical slide (or pinch) | zoom (setting: off / on) (memory, unverified: the pad reports two touches; pinch on a 2-inch pad is cramped, so the slide is the default) |
| tap without click | nothing (no accidental actions) |
| left / right half click | optional, God of War style: left = map, right = inventory (default off) |

Nothing on the touchpad is required: every touch action has its stick / trigger twin (Yotei finding).

**Mouse and keyboard (parity)**

| Input | Action |
|---|---|
| the game's nav map key | open / close (keep the game's bind) |
| hover | short card (name, type, distance, faction) |
| LMB | select + card; LMB again on the selected object or double-click = best path; Enter = best path |
| RMB | add a waypoint; RMB on a waypoint = remove it; Shift+RMB = append without selecting (Everspace 2) |
| Ctrl+Del (or a confirm) | clear all waypoints |
| wheel | zoom toward the cursor (past the widest -> universe) |
| LMB drag / MMB drag, W A S D / arrows | pan (X4, Starfield) |
| Space or Home | recentre on the ship (Elden Ring T, X4 R) |
| Tab | destination list; typing in it searches (X4, Elite) |
| U / Backspace / Esc | universe view / back / close |
| 1-4, L | filters, legend |
| I | full info card |

### 5. Freelancer specifics, and how each maps

| The game's map action | Game mechanism (`docs\FREELANCER_RE.md`) | Pad | Mouse |
|---|---|---|---|
| select object | the nav map's selection (hover names, click selects) | A | LMB |
| set waypoint | 0x4C4780 append (the map's button 0x493A7C), up to 99 entries | X | RMB |
| clear waypoint | 0x4C46E0 remove one | X on a waypoint | RMB on a waypoint |
| clear all | 0x4C46C0 | X hold | Ctrl+Del |
| best path | 0x4C4EB0: replaces the list; lanes, gates, the end; respects locked gates (an empty answer = say so on screen) | A on the selected | LMB again / Enter |
| zoom in / out | NavMapInZoom / NavMapOutZoom (the game's two steps) or FLvibed's own scale | RT / LT | wheel |
| system / universe | NavMapUniverseButton | zoom past the end / B | U |
| filters | physical / political / patrol / mineable, legend | D-pad left / right, Menu | 1-4, L |
| base list | FLvibed's destination list (exists in `padmap`) | LS click | Tab |
| info card | the game's info text | Y | I |

Notes: Freelancer's waypoints are one-shot (removed on arrival, `FREELANCER_RE.md` "Waypoints"), like BotW's pins, so the
pattern of "a route = the waypoint list" fits; persistent marks (stamps) do not exist in the game and would be an
FLvibed addition. Best path replacing the list means "add a waypoint after a best path" appends to the route: a feature
(manual detours), worth a prompt line.

### 6. Open questions for the owner

1. **A twice for best path, or a dedicated button** (Starfield's X = Set Route) and A only selects?
2. **Hold or press for best path?** It replaces the whole waypoint list (Spider-Man 2 holds for travel; Freelancer's
   best path is cheap to redo, so a press is proposed).
3. **LB / RB**: keep them for the hub tabs (18 Oct) or give them to the map (filters / scale) while the canvas has focus?
4. **D-pad up / down**: object stepping on the canvas, or panel focus only?
5. **Zoom: continuous FLvibed zoom with a seamless system -> universe change, or the game's discrete steps**
   (NavMapInZoom / OutZoom)? The first needs the canvas drawn by FLvibed (the FLUF route, `hud-skin.md` U44).
6. **Magnetism default** (Low proposed) and whether a hard snap option is wanted at all.
7. **Touchpad**: drag-to-pan and two-finger zoom on by default? Half-click split (left map / right inventory) wanted?
8. **Persistent custom marks** (stamps), beyond the game's waypoints: in scope?
9. **Universe view targets**: A on a system plots a best path to what (its nearest entry gate, its main base, or the
   system's centre)? Proposed: zoom into it; the path needs a target inside.
10. **The autopilot**: should "best path" on the map also offer "fly it" (the Advanced Autopilot's travel, as the
    destination list does today in space)?

### 7. Gaps

- No source gave the full pad bindings for Elite Dangerous's galaxy / system maps, No Man's Sky's buttons (only the
  action names), Horizon Forbidden West's, Ghost of Tsushima's, RDR2's / GTA V's map screens, Assassin's Creed
  Mirage / Valhalla's map, Mass Effect's galaxy map or Star Wars Squadrons (no galaxy map; not researched further). The
  rows marked (memory, unverified) need an in-game check or a better page.
- No numbers for cursor speed, acceleration or slowdown in any shipped map; the values in section 4 are judgement.
- Frames were not studied (no video pages by rule); map proportions and transitions are not covered here.

### Sources

1. Elden Ring wiki (wiki.gg), Map - controls tables: https://eldenring.wiki.gg/wiki/Map [page]
2. Elden Ring beacons and markers removal: https://www.destructoid.com/elden-ring-how-to-remove-clear-out-missing-blue-beacon-map-markers/ [snippet]
3. Starfield menu controls (Starmap, System Inspect, Surface Map): https://eip.gg/starfield/db/menu-controls/ [page]
4. Starfield fast travel / set course: https://gamesradar.com/starfield-fast-travel-starmap and https://www.gameskinny.com/tips/all-starfield-controls-for-pc-and-xbox/ [snippet]
5. Elite Dangerous galaxy map guide: https://www.gamepressure.com/elitedangerous/galaxy-map/z7739d [page]
6. Elite Dangerous pad map, forum and guide summaries: https://www.ludo.guide/guide/elite-dangerous/using-the-map (fetched: not about Elite's map, so not used) and https://forums.frontier.co.uk/threads/the-xbox-one-mapping-and-ingame-controls-thread.187744 [snippet]
7. Frontier, Elite Dangerous launches for PS4 (touchpad swipe on the map): https://frontier.co.uk/node/759 [snippet; fetch refused 403]
8. Elite Dangerous gamepad section: https://gamepressure.com/elitedangerous/gamepad/zb7389 [page]
9. No Man's Sky NEXT, the galaxy map and warping: https://www.magicgameworld.com/no-mans-sky-next-the-galaxy-map-warping/ [page]
10. No Man's Sky Xbox controls (galactic map action names): https://www.gamepressure.com/nomanssky/xbox-onexsx-controls/z110583 [page]
11. Star Citizen wiki, controls: https://starcitizen.tools/Guide:Controls [page]
12. Star Citizen starmap mouse use: https://star-netrunners.gitbook.io/star-citizen-beginners-guide/part-2/section-2-2 [snippet]
13. Everspace 2 controls wiki and patch notes: https://everspace.fandom.com/wiki/Controls_(ES2) [snippet; fetch refused 402], https://steamdb.info/patchnotes/19955367 [snippet]
14. X4: Foundations manual, The X-Universe and Map: https://wiki.egosoft.com/X4%20Foundations%20Wiki/Manual%20and%20Guides/X4:%20Foundations%20Manual/The%20X-Universe%20And%20Map [page]
15. X4 Steam Input layouts: https://www.steaminputdb.com/app/392160 [snippet]
16. Mass Effect 3 galaxy guide: https://gamerguides.com/mass-effect-3-legendary-edition/guide/exploration [snippet]; https://www.gamepressure.com/masseffect3/galaxy/z13a67 [page, index only]
17. Star Wars Outlaws maps (Game8): https://game8.co/games/Star-Wars-Outlaws/archives/469136 [page]
18. TotK map controls summaries: https://www.wepc.com/gaming/how-to-expand-the-map-in-zelda-tears-of-the-kingdom/ , https://zeldawiki.wiki/wiki/Pin [snippet; fetch refused 403]
19. Stamen, cartographers review BotW's map: https://stamen.com/cartographers-play-video-games-a-review-of-the-map-in-the-legend-of-zelda-breath-of-the-wild/ [page]
20. Stamen, cartographers review TotK's map: https://stamen.com/cartographers-play-video-games-a-review-of-the-map-in-the-legend-of-zelda-tears-of-the-kingdom/ [page]
21. Ghost of Tsushima guiding wind / markers: https://www.gamerevolution.com/guides/653064-how-to-use-the-guiding-wind-in-ghost-of-tsushima-ps4 [snippet; fetch refused 403]
22. Can I Play That, Ghost of Tsushima accessibility review: https://caniplaythat.com/2020/07/29/ghost-of-tsushima-accessibility-review/ [page]
23. Horizon Forbidden West controls: https://www.gamenguides.com/horizon-forbidden-west-controls-guide [snippet]
24. Can I Play That, Horizon Forbidden West accessibility review: https://caniplaythat.com/2022/02/14/horizon-forbidden-west-accessibility-review-can-i-play-that/ [page]
25. Can I Play That, Horizon Forbidden West menu deep dive: https://caniplaythat.com/2022/02/14/horizon-forbidden-west-accessibility-menu-deep-dive/ [page]
26. Spider-Man 2 controls guide (PS5): https://outsidergaming.com/spider-man-2-complete-controls-guide-for-ps5/ [page]
27. Spider-Man 2 fast travel: https://www.sportskeeda.com/esports/marvel-s-spider-man-2-fast-travel-guide [snippet]
28. PSU, Spider-Man 2 hold-to-confirm: https://www.psu.com/news/marvels-spider-man-2-fast-travel-hold-to-confirm-prompt-isnt-masking-any-background-loading/ [page]
29. RDR2 easy map access / waypoint size: https://www.gamerevolution.com/?p=453047 , https://www.tweaktown.com/news/63661/red-dead-redemption-2-tips-tricks/index.html [snippet]
30. GTA V map and waypoint: https://wiki.gameproshop.net/content/wikihow_en_maxi_2023-03/Play-Grand-Theft-Auto-5-(Story-Mode) , https://gta.fandom.com/wiki/GPS [snippet]
31. Assassin's Creed Mirage eagle vision / markers: https://sportskeeda.com/esports/assassin-s-creed-mirage-eagle-vision-guide-how-locate-objectives-scout [snippet]
32. Kotaku, how Assassin's Creed IV uses the PS4 touchpad: https://kotaku.com/how-assassins-creed-iv-uses-the-ps4-touchpad-1428875609 [page]
33. PlayStation, God of War Ragnarok accessibility options: https://www.playstation.com/en-us/games/god-of-war-ragnarok/accessibility/ [page]
34. God of War Ragnarok touchpad defaults: https://www.shacknews.com/article/133014/controls-buttons-god-of-war-ragnarok [snippet]
35. Can I Play That, Ghost of Yotei accessibility review: https://caniplaythat.com/?p=16160 [page]
36. Pad cursor magnetism: https://medium.com/ko-op-words/prodding-gnoggins-f481bea963ab , https://uxdesign.cc/game-ux-the-cursor-that-wasnt-supposed-to-be-there-f9369dafc451 , https://pathofexile.com/forum/view-thread/3642737 [snippet; the first two refused 403]
37. Game Accessibility Guidelines, full list: https://gameaccessibilityguidelines.com/full-list/ [page]
