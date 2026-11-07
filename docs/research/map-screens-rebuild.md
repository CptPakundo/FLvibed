# Rebuilding the nav map: how modern maps look, control and work, and a proposal for FLvibed
Status: current
Checked against: FLvibed d95714ba (7 Nov 2026; HEAD had moved to 3f750496 by the end of the round, those commits not reviewed here); HDE 0.7.1 test install (data read, game not run); FLUF source 627ea2c in `research\repos\FLUF` (read, not run)
Dirty paths at check: core/flexe.h, docs/MAP_PLAN.md, docs/module_map.tsv, modules/controller/src/impl/controller_menus.cpp, padmap.cpp, padmap.h, tests/main.cpp, core/game/navview.cpp, navview.h, navview_rules.h, tests/navview_test.cpp (the zoom / pan work in flight)
Watch: docs/MAP_PLAN.md, modules/hud/src/navnew.*, modules/hud/src/navskin.*, modules/hud/src/navmap_layout.h, modules/hud/src/navmap_labels.h, modules/controller/src/impl/padmap.*, core/game/navview.*, modules/hud/docs/UI_HUD2_SKIN.md#6.1
Evidence: research\evidence\map_visual\ (20 reference frames, 2 silent frame strips and 2 clips; git-ignored)
Sources: [page] = fetched this round and read; [frame] = a screenshot or clip saved and looked at, sizes measured from it by eye on a 1920 x 1080 grid (about +/- 1 %); [snippet] = seen only in a search summary; [file] = this repository or install; (memory, unverified) = model knowledge, not checked this round. Controls already covered are in `map-screens-aaa.md`; this note adds to it and does not repeat it.

Not done in this round: `IDEAS.md` and `README.md` were not updated (the round's brief allowed one file). The working session should add the index row and merge section 9's items as ideas.

## What it means for FLvibed

1. **"Too small" is measurable.** Every modern map measured here gives the map 68 to 100 % of the screen, and most run it full bleed with panels floating over it. Today's 0.49 H square is 13.5 % of a 16:9 screen; the grown 0.785 H square is 34.7 % (section 1). The grown map is a step, not the goal: **the screen has to become the map.**
2. **"Archaic" is not the label size.** Today's 2 to 2.5 % H is at or above Starfield's 1.3 % H cap height. What reads old is everything else: a boxed viewport, a full-strength grid over a busy star field, thin condensed text with no backing, small 3D models where flat symbols belong, and no hierarchy (section 2). Starfield's star map replaces grid lines with 1 px cross ticks on a near-black ground, the cheapest modern signal seen.
3. **Most of the look works over the game's map.** `navnew` already holds labels, marks and route behind its switch. Flat marks, backed labels, a route line and a full-screen dark ground do not need a new map. Only three things do: removing the game's art, a non-square canvas, and a scaling universe (section 8).
4. **Self-explanatory prompts have a recipe in shipped frames:** a fixed bar whose unavailable verbs dim instead of vanishing (Horizon, Starfield), a second line for the hold (Starfield "HOLD TO EXIT"), and the verb written beside the object (BotW "A Travel", Starfield "TRAVEL TO KREET"). B must name what it does next (section 4).
5. **Freelancer's data covers nearly everything a modern map offers.** Route time, reputation, known prices and visit flags are read or readable; the game's own "seen" data keeps overlays honest (section 6). The one classic community want is trade: FLCompanion and DarkStat exist because the game never showed it, so **"where do I sell this" on the map is the feature no vanilla Freelancer player has had.**
6. **One earlier ruling deserves a second look, and it is the owner's call.** `UI_HUD2_SKIN.md` 6.1 rejected FLvibed's own map partly because zone, lane and patrol containers are undecoded. FLUF's map never decodes them: it parses the system inis and walks `Universe::`. Cost and mod risk still stand: FLUF's map is about 4,900 lines (section 8).

The proposal is section 7; the owner's decisions are section 10.

## Findings

### 1. Screen share (7 Nov 2026)

Today, computed for 16:9: 0.49 H square = 13.5 % of the screen; 0.785 H = 34.7 %; a full-height square = 56 %.

| Game | Form | Map share | Panels, legend, filters, card | Basis |
|---|---|---|---|---|
| Starfield, system | full bleed | about 79 % clear | one docked-looking but floating card left, 360 x 993 px (17 % of the screen); faction tag top right; prompts bottom right, no plate; the orbits pass under the card | [frame] |
| Starfield, galaxy | full bleed | about 85 % clear | travel card right, 360 x 730 px, only with a route; tip box top centre | [frame] |
| Elite Dangerous, galaxy (current look) | full bleed | about 89 % clear | icon rail left, 48 px; search top centre; route bar bottom, 1345 x 77 px; hint row under it | [frame] |
| Elite Dangerous, galaxy (older look) | panel + map | 77 %, popup 5.6 % more | opaque left panel 23 % of the width; a floating card beside the star | [frame] |
| Elite Dangerous, system | panel + map | about 77 % | same left panel; bodies as a schematic tree | [frame] |
| X4 | full bleed under a big panel | about 68 % clear | left panel 28 % of the width, bottom selected-object card, filters and search top right; panel width has a slider since 9.00 [snippet] | [frame] |
| Horizon Zero Dawn / Forbidden West | full bleed | about 100 %, 79 % with the filter list open | tab row and prompts float with no plate; filter list slides over a dark gradient, 21 % of the width | [frame] |
| Cyberpunk 2077 | full bleed | about 100 % | header line, filter top left, quest top right, prompts bottom; no plates | [frame] |
| Breath of the Wild | framed | about 83 % | carved border top and bottom, 85 % of the height | [frame] |
| Spider-Man (2018) | framed tab | about 85 % of the height (inferred from the legend page's bars) | bars 9 % top, 6 % bottom; the legend is a full page | [frame] |
| Elden Ring | full screen | 100 % | "fills the entire screen" [page 1] | [page] |
| EVE Online | window or full screen | ? | Legion: filter bar bottom centre, layout panel bottom left, quick actions bottom right, selection top centre; responsive to minimised, floating, fullscreen [page 2] | [page] |
| No Man's Sky, Star Citizen, Everspace 2, Mass Effect, Outer Wilds, Outlaws, Spider-Man 2, TotK | full screen, galaxy maps on a hologram or plain ground | ? | no frame obtained | (memory, unverified) |

**Pattern:** the map is the screen. Panels float over it, take 11 to 32 %, and appear only when they have something to say: Starfield's travel card exists only with a route, Horizon's filter list only on demand. X4 is the dense extreme and its readers complain (section 2). Docked panels with a boxed map survive only in Elite's older look, which Frontier left.

### 2. What reads modern, what reads archaic

Sizes at 1080p from the frames.

| Trait | Modern practice | Numbers and examples | Today's map |
|---|---|---|---|
| Ground | near-black or dark, low-contrast grain, no busy texture | Starfield charcoal with fine grain; Elite dim warm haze | saturated purple clouds and sharp white stars compete with the content |
| Grid | ticks or faint lines, or none | Starfield: 1 px crosses about 430 px apart, arms about 70 px, corner brackets. Elite: 1 px, about 10 % opacity, in perspective. BotW keeps a faint grid in undiscovered space only | full-strength cyan 8 x 8 lines |
| Line weights | 3 tiers | orbits or links 1 px at 20 to 30 %; route 2.5 to 7 px, full strength (Starfield 2.5, X4 4, Elite 5, Horizon 7) | lanes thin and dotted, the same strength as background |
| Route colour | one accent used for nothing else | Starfield cyan, Elite and Horizon orange, X4 blue | no distinct route |
| Icons | flat, two-tone, constant screen size | 30 to 50 px (2.8 to 4.6 % H): Cyberpunk 30, Starfield 35, Horizon 40 to 50; Starfield's plate is a white diamond with a dark glyph | small 3D models, scale with zoom |
| Shape carries class, colour carries state | | BotW towers and shrines: orange discovered, blue complete [page 3]; Starfield: white reachable, red out of range [snippet 4]; TotK: filled glowing arrow on this layer, open on another [page 5] | colour only, in the Political layer |
| Labels | plain sans, sentence or caps, backing on the selected one | Starfield cap height 14 px (1.3 % H), selected label on a light plate 27 px (2.5 % H); Elite 13 to 20 px, fading with depth; Cyberpunk and Horizon larger panels at about 2 % H | thin Agency FB at 2 to 2.5 % H, no backing |
| Hierarchy | few strong things, the rest receding | Elite's unselected names sit at about 50 % grey, the route at full colour | every label the same weight |
| Glow | on the selected thing and "you" only | Elite's cyan ring; BotW's selected shrine; Starfield nearly none | none |
| LOD by zoom | labels and icons appear with room | BotW: major regions first, minor on zoom-in, labels hidden until there is room [page 3]; Cyberpunk's Dynamic filter: missions when far, vendors and fast travel when near [page 6]; Stamen fault TotK geoglyphs for vanishing on zoom-in and want point at world scale, polygon near [page 5] | a radius test only (the game's 550 rule) |
| Fixed size text under zoom | fix the label, resolve collisions per zoom | a held size makes labels collide when zooming out; remove or fade them [snippet 7]; precompute per scale [snippet 7] | fixed size, overlap boxes |
| Depth | optional | Elite is true 3D; EVE added a flat abstract toggle in 2025 because the 3D map could not do the job alone [page 2]; players ask for 2D in Stellaris [snippet 8] | flat |

**Negative examples.** Cyberpunk before 1.5: a "tangle of disorder", teal, dark blue and muddy yellow on muted red [page 6]; red ground that "fails to communicate the variety between different zones" [page 9]; Interface In Game calls it "overloaded with icons". X4: station overlays hard to read because of the background transparency [snippet 10]. Elite Odyssey: icons all orange or yellow, hard to tell apart, more clicks than before [snippet 11]. Star Citizen commenters: legibility has to come before the coolness of blue holograms [snippet 12]. Starfield: a "crowded" map [page 13]; systems lack distinctive markers [snippet 14]. Tsushima, Horizon: `map-screens-aaa.md` section 2.

**So, in order of what the eye sees:** ground, grid, then hierarchy, then symbols. The labels' size is not the problem.

### 3. Space-game specifics

- **Star system.** Starfield: real-looking sun and planet renders at the centre of concentric orbit lines, tilted, with diamond POI plates [frame]. Elite older system map: not spatial at all, a schematic tree of bodies [frame]; the later orrery view is spatial (memory, unverified). Outer Wilds: a stylised planet list with a separate clue graph, "Rumor Mode", nodes are discoveries and edges how one clue leads to another [snippet 15]. X4: sectors as hexes with yellow edges, ownership as a hex-cell tint, flat station plates [frame].
- **Routes.** Elite: thick orange polyline through every intermediate system; blue arrow at the start, orange at each waypoint [page 16][frame]. Starfield: cyan line with one mid-arrow and an end hexagon; the card lists start, end, jumps, range, distance, fuel [frame]. Elite's bottom bar: current system, next jump, jump count, destination [frame]. Horizon: thick orange with a mark at the end, kept visible at every zoom [frame].
- **Zones.** X4 tints soft regions with coarse cells; Horizon's unexplored area is soft white cloud with a torn edge [frame]. Elden Ring pairs a cloud with faded detail below, praised as inviting exploration [page 1]; TotK shows water outlines in undiscovered parts [page 5]. Soft regions beat hard polygons.
- **Fit for Freelancer.** Systems are flat fixed layouts: planets do not orbit, so **orbit lines would be false** and range rings around "you" serve better. Lanes are the system's roads, so they should be the strongest geography, like Horizon's paths and Mario Kart World's order of tracks, roads, land [snippet 17]. Zones are soft tints by kind. Gates and holes sit at system edges, so edge-pointing marks for "leads to Colorado" add meaning that Elite's map cannot.

### 4. Control, beyond `map-screens-aaa.md`

**Gaps of its section 7, as far as filled:**

| Game | Now known | Basis |
|---|---|---|
| Elite Dangerous, pad | galaxy map hints read: pitch and yaw on RS, forward / back and left / right on LS, up and down on keys, zoom RT / LT, an Axis Toggle on X | [frame] |
| Horizon Zero Dawn, pad | Close O, Pan LS, Zoom RS, Travel R2, Filter Options, Center on Aloy Triangle, Place waypoint Cross | [frame] |
| Horizon Forbidden West, K&M | Close Esc, Zoom wheel, Filter T, Travel click / X, Center on Aloy MMB / F, Place Waypoint RMB / Space | [frame] |
| Cyberpunk 2077, K&M | Move, Zoom Out / In on mouse buttons, Track Waypoint Space, Jump to V Tab, Legend X, Free Camera M, Close | [frame] |
| Starfield, K&M | Missions L, Show Me V, Reset Camera O, Set Course X, Back to Galaxy Tab, Hold to Exit | [frame] |
| BotW, pad | Move LS, Sheikah Sensor Y, Back B, Select A, Zoom RS | [frame] |
| Mass Effect 1, Xbox 360 | four levels, galaxy, cluster, system, planet; select any location to travel; one button zooms out, one exits (glyphs lost in the PDF) [page 18]. Legendary Edition swapped them to match ME2, so a zoom-out no longer quits by accident [snippet 19] | [page] |
| RDR2 | Options hold opens the pause map, X sets a waypoint, L3 adds a marker [snippet 20] | [snippet] |
| No Man's Sky | action names only; buttons shown as images, still not known [page 21] | gap |
| Tsushima, GTA V map | still not documented in what was read | gap |

**Cursor numbers.** No shipped map publishes its values. What exists:

| Source | Value | Basis |
|---|---|---|
| Unreal CommonUI analog cursor | fields acceleration, max speed, dead zone (0 to 0.9), HoverSlowdownFactor (0 to 1), scroll dead zone and period; defaults not on the page [page 22]. Acceleration 1500, max speed 2200 px/s, dead zone 0.25, slowdown 0.4 | fields [page]; values (memory, unverified) |
| Unity VirtualMouseInput | cursorSpeed = pixels a second per axis at full tilt [snippet 23]; 400 | field [snippet]; value (memory, unverified) |
| Leaflet (mouse maps) | wheel 60 px per zoom level, 40 ms debounce, inertia deceleration 3400, pan easing 0.25 s, keyboard pan 80 px [page 24] | [page] |
| Skyrim | map zoom speed 2, pitch 15 to 75, yaw range 80 [snippet 25] | [snippet] |
| FLUF's map | zoom speed 3, wheel | [file] `NavMapWindow.hpp` |
| Horizon Zero Dawn | whole-map zoom out about 1.5 s, slowing as it goes (frame change 24 down to 9 per frame) | [frame], measured |
| FLvibed plan | zoom 1.2 s end to end, pan 0.9 viewport/s | [file] `MAP_PLAN.md` |

The plan's zoom agrees with Horizon's. Microsoft says plainly that XY focus is not practical for maps and drawing surfaces, and its sample turns pointer mode on for a map control [snippet 26].

**How they feel and combine** (the combinations are judgement from the above):
- Two speeds are the point: fast traverse, slow over targets. Friction makes a free cursor feel like snapping without taking control.
- Cursor and pan are separate jobs at Z above 1: RS moves the world, LS moves the pointer. A pointer held at the viewport's edge should also push the view (`map-screens-aaa.md` pattern 1); the plan does not have this yet.
- Zoom follows the cursor: Starsector made zooming centre on the mouse as a stated improvement [snippet 27]; the plan already does.
- Object stepping is the precision and accessibility route, not the main one: D-pad up / down nearest first; the list is the route to far targets.
- Elite shows the failure: six axes and a toggle on a pad, with players unable to find vertical movement [snippet 28]. Freelancer's flat map needs none of it.

**Prompts that teach.** XAG 114 asks that a player knows what to expect before activating, that select / back and tab buttons stay consistent, and that help is context-sensitive [page 29]; the Game Accessibility Guidelines prefer help in context over upfront teaching [page 30]; a bottom Action Bar is the named pattern [snippet 31]. The frames add the craft:

1. **One fixed bar, fixed order.** Horizon and Starfield keep the same verbs in the same places.
2. **Dim, never hide.** Horizon's Travel and Place Waypoint dim with nothing to act on; Starfield's Reset Camera dims while the camera is home [frame]. Vanishing verbs make a player hunt.
3. **Hold on a second line.** Starfield's "BACK TO GALAXY / HOLD TO EXIT" states two meanings of one button [frame]. Kotaku still found hold-to-exit confusing, with rapid zooming in and out [page 13], so the words alone do not save an overloaded button.
4. **The verb sits at the object too.** BotW writes "A Travel" in the selected shrine's card; Starfield "TRAVEL TO KREET" at the cursor; Elite a row of labelled icons in the card [frame].
5. **Name the effect, with the target.** "Back to Galaxy" beats "Back". XAG 114: the player should expect the result.
6. **Dangerous-sounding verbs last, close at the far right** (every frame seen).

### 5. Transitions

| Moment | Seen | Basis |
|---|---|---|
| Open, Horizon Zero Dawn | world dims and map icons fade in large, then scale down: about 0.27 s, a cut settles by 0.55 s | [frame], clip measured at 30 fps |
| Open, BotW | a hard one-frame cut to the map, then the Sheikah eye plays over it for about 0.35 s, no wait to use it | [frame], clip measured |
| Zoom, BotW | docked levels with a slight blur [page 3] | [page] |
| Tabs, BotW | a slide of about 0.4 s | [frame] |
| Level change, Starfield | B zooms planet to system to galaxy; "Show Me" can snap-cut to a planet with no explanation [page 13] | [page] |
| Level change, Elite | players asked for zooming between galaxy and system map [snippet 11] | [snippet] |
| Reveal | TotK Lightroots light the map radially from the centre [page 5]; BotW's tower download [page 3] | [page] |
| Selection, Elite, Starfield | ring and card at once, no animation seen in stills | [frame] |
| Game today | NeuroNet slide about 0.5 s, view change 1.5 s, 0.4 s on the select path | [file] `07_navmap_facts.md` |

**Rule of thumb:** usable at once, ceremony on top; opening under about 0.5 s; never a blocking animation. The game's 1.5 s fly-out to the universe is the slowest step here: shorten it only if the timer allows, as `MAP_PLAN.md` keeps it.

### 6. Functionality, mapped to Freelancer

Value and effort are judgement. "Over" = works over the game's map; "Own" = needs FLvibed's canvas.

| Function | Who does it well | Game data today | FLvibed adds | Fit | Value / effort |
|---|---|---|---|---|---|
| Search | Elite, X4 by name, type or id [page 32], Star Citizen 3.23 [snippet 12] | names, `universe_data` | search on `osk.*`, list jump | Over | high / low |
| Destination list by kind | Elden Ring, X4 Object List | `padmap` list, records | kinds, sort by distance | Over | high / low |
| Filters and layers | EVE bar with favourites [page 2], Horizon eye toggles, Cyberpunk 14 categories [page 6] | the game's Physical, Political, Patrol, Mineable, All / Friendly / Hostile | chips exist; per-kind toggles of FLvibed's marks | Over | medium / low |
| Legend | Spider-Man page with checkboxes, Horizon (legend = filter) | `NAV_legend.3db`, no route yet | own legend that doubles as toggles | Over | medium / low |
| Custom pins | BotW stamps, Elden Ring wax stamps, 10 symbols, 100 per map [page 1] | none | a per-save store | Over | medium / medium |
| Route, multi-stop | Everspace 2 Shift+RMB chain [snippet 33], Elite one route only [page 16] | waypoints to 99 (0x4C4780), best path 0x4C4EB0 | route editing in the panel | Over | high / low |
| Route preview | Starfield jumps, range, distance, fuel; Elite jumps | `route::Plan`, `RideTime`, `LaneBetween` | total time, jumps, lanes, hostile passes | Over | **high / low** |
| Autopilot from the map | X4 orders; Everspace 2 auto-heads to the next waypoint [snippet 33] | Advanced Autopilot, Y hold TRAVEL | a stated ETA on the prompt | Over | high / done |
| Fast travel | every open world | none | **does not fit**: the autopilot is the answer | no | n/a |
| Track objective | ME2 lists mission titles on the map [page 34]; Cyberpunk Track Waypoint | navmarker (differs from waypoints, `navigation-and-hazards.md`), `NavMapMissionObjective*` | objective mark and a "go to objective" verb | Over | high / low |
| Info card | Starfield facts grid, Elite popup, Elden Ring none | infocards, faction, type | short card by template: faction, attitude, distance, services, what it sells | Over | high / medium |
| Trade overlay | X4 trade offers, dims filtered wares [snippet 10]; FLCompanion ranks by profit and time; DarkStat cyclic routes [page 35][page 36] | `market_commodities.ini` (178 bases, 1,994 goods rows), `pub::Market::GetPrice`, **seen prices at 0x66DABC** | "sell here" tint for the held cargo; a best-run list by profit per second | Over | **high / medium** |
| Faction overlay | Stellaris map modes [snippet 8], X4 tint | `pub::Reputation`, Political layer; white = neutral | marks by live attitude; hostile-system flags in the universe | Over | high / low |
| Danger overlay | Starfield red stars, none in space sims this deep | hazards table, zone flags, patrol zones, encounters | zone tints, a warning on the route card | Over; soft tints Own | medium / medium |
| Fog and discovery | Elden Ring, Outlaws [snippet 37], TotK | visit flags (Known, Unknown, Unknowable, Zone, Solar), `ObjectUnknown`, record byte +0x48 | **never show what the game hides** (6.1's limit); a "?" mark for rumoured | Over | must / low |
| History | BotW Hero's Path, replayable [page 3] | none | breadcrumb per save; a "recent" list is cheaper | Over | low / medium |
| Minimap link | GTA route on the radar; Stamen on BotW's auto-zoom [page 3] | no minimap in the game | not needed; a route chip on HUD 2 instead | partly | low |
| In-world guidance | Horizon, Dead Space's line [snippet 38] | waypoint marks in space, the HUD | next leg, distance and ETA chip; a world line would be an extra | Over | medium / low |
| Accessibility | God of War icon and text sizes, plates; XAG | HUD 2 sizes, cursors | icon size, text floor, shape + colour, magnetism, hold or toggle, reduced motion | Over | must / low |
| Multiplayer | DarkStat's live positions | groupmates (FLUF shows them) | group marks; prices differ on servers | Over | low / medium |

**What does not fit:** fast travel, orbit lines, terrain, 3D tilt (the data is flat), fuel and jump range (no such mechanic). Trade data must be limited to what the player has seen, by default, in line with the autopilot's `exposed` scope. The data is plain ini in HDE 0.7.1: 53 systems, 198 bases; New York alone has 413 zones, 140 objects, 2 nebulae and 8 fields, so declutter is not optional [file].

### 7. The proposal

**Screen (16:9; the solver gives the others):**

```
+--------------------------------------------------------------+
| full-screen dark ground, game world hidden (subtitles float)  |
|  NEW YORK  Liberty          |            | PLANET MANHATTAN   |
|  Objective ...              |    MAP     | Liberty - friendly |
|  ROUTE  3 legs, 4:10        |  no frame  | 9.4 K, C3          |
|  1 Trade Lane Ring   2.1 K  |  ticks,    | Sells  Buys  Rep   |
|  2 Fort Bush        14.8 K  |  not grid  | (only when         |
|  (only with content)        |            |  selected)         |
|                                                              |
|  [Physical Political Patrol Mining | Trade Danger] scale bar  |
|  LOG MAP INVENTORY ...     LS Cursor RS Pan LT/RT Zoom  A .. B|
+--------------------------------------------------------------+
```

- Ground: one near-black navy over the whole screen; the flight scene no longer shows round the panels. At Z 1 the system stays inside the safe square (0.785 H, clear of the subtitles' panel, the owner's 5 Nov rule); at Z above 1 the content runs to the edge of the viewport, feathered about 3 % H.
- No frame, no box. Panels are glass over the ground, sized to content, right panel only with a selection, sliding in about 0.15 s.
- Grid letters and numbers stay (the game speaks sectors: "C3") but small and dim, inside the viewport's edge, with cross ticks at the intersections if the grid art can go.
- Layers as one chip row at the bottom, EVE's place, instead of a panel block, freeing the right panel for the card.
- Symbols: base (shape by class: station, planet base, outpost), gate, hole, lane ring only at sector zoom; colour by live attitude: friendly, neutral, hostile, plus shape, per HUD 2's cursor set.
- LOD: Z 1 planets, bases, gates, holes, lanes, mission; Z 2 to 4 names of zones and fields; above 4 rings, wrecks, patrol names; labels drop by rank, marks stay, positions stable (`navmap_labels.h` already does this).
- Route: one accent, 3 px at 1080p scaled, with leg numbers at waypoints and a dim arrow; "you" with heading and a soft glow, the only other glow.

**Control:** `MAP_PLAN.md`'s table stands (research 4, the owner's word). Additions from this round:

| Item | Proposal | Basis |
|---|---|---|
| Cursor | full tilt about 1 viewport a second, squared response, dead zone 0.2, ramp about 0.2 s | judgement, within CommonUI's fields |
| Friction | speed x 0.4 inside about 0.03 H of a mark, Low 0.6, High 0.3 | CommonUI's value (memory, unverified); `map-screens-aaa.md` said 0.35 |
| Edge push | at Z above 1, the cursor in the outer 6 % pans at up to half the RS speed | pattern 1 of `map-screens-aaa.md` |
| B | named: BACK TO SECTOR, BACK TO SYSTEM, BACK TO SIRIUS, CLOSE; never a hold | Mass Effect swap, Kotaku on Starfield |
| Closing | View closes from any level in one press, so B can stay a pure "back" | same |
| Zoom readout | a scale bar in K and the level's name | none of the frames, judgement |

**The prompt bar (pad; K&M shows its glyphs in the same slots):**

| State | Left: moving | Right: acting |
|---|---|---|
| Nothing under the cursor | LS Cursor, RS Pan (dim at Z 1), LT / RT Zoom, RS click Find me (dim when centred) | A Select (dim), X Waypoint (dim), Y Info (dim), B Close |
| Over an object | same | A Select, X Waypoint (dim until selected), B Close, and a small tag at the object: its name and "A Select" |
| Selected | same | A Route here, X Add waypoint, Y Info / hold Fly there, B Clear selection |
| Selected waypoint | same | A Route here, X Remove, hold X Clear all, B Clear selection |
| A route exists | + the route card in the left panel | Y hold Fly route with its ETA |
| Universe | LS Cursor, RT Enter system | A Select, B Back to New York |

Holds show a filling ring and "hold" on a second line. On the first opens the bar's left half is bright, then it dims once each input has been used (GAG's help in context); a setting pins it on.

**Features, ranked by player value per effort:**

| Rank | Item | Tier | Phase |
|---|---|---|---|
| 1 | Dark ground, frame off, viewport feather, grown map by default | (a) | 1 |
| 2 | HUD 2's labels and flat marks with LOD (`navnew`) | (a) | 1 |
| 3 | Route line and "you" with heading | (a) | 1 |
| 4 | The prompt bar, dim rule, object tag, named B | (a) | 1 |
| 5 | Route card: legs, distance, time, jumps | (a) | 2 |
| 6 | Own selection and hover card (needs the selection trace, P3) | (a) | 2 |
| 7 | Marks by live attitude, objective verb | (a) | 2 |
| 8 | List by kind plus search | (a) | 2 |
| 9 | Own legend that toggles marks; chip row | (a) | 2 |
| 10 | Friction on real objects, stepping, edge push | (a) | 2 |
| 11 | Trade: sell-here tint and best runs from seen prices | (a) | 3 |
| 12 | Danger: route warnings, then zone tints | (a) | 3 |
| 13 | Universe: names, attitude, jump count at HUD 2's size | (a) | 3 |
| 14 | Accessibility rows, custom pins, recents | (a) | 3 |
| 15 | Own canvas: art gone, ticks, soft zones, rectangular full bleed, scaling universe | (b) | 4, only if the owner reopens it |

**Phases.** Phase 0 is `MAP_PLAN.md` P1 and P2, in flight. Phase 1 alone answers "too small and archaic" as far as (a) can, and its result decides whether phase 4 is wanted at all.

### 8. Over the game's map, or FLvibed's own

**(a) Over the game's map.** Available: frame off (`+0x84`), side art off (bit 1), the box to any square, labels off by the game's flag, overlays from `+0x400`, Z and C driven (`navview`). In hand: `navnew`, `navmap_labels.h`, `route.*`. Limits: the game's background, zones, lanes, patrol lines and 3D objects stay under the overlay; the universe does not scale.

**Cheap probes that would widen (a), none done:**

| Probe | Question | Why |
|---|---|---|
| Handles -1 on `+0x354 / +0x37C` | do the backgrounds go, and does the grid go with them? | `07_navmap_facts.md`: untested. Where the grid lines come from is not established; if baked into `NAV_zoomedliberty`, the clouds go too and FLvibed's ground replaces them, the largest single visual win |
| Non-square box with `+0x4A8 / +0x4AC` in ratio | can the viewport be 16:9 at uniform scale? | axes are separate in the transform (`FREELANCER_RE.md`, labels and records), so it may work: full bleed without a new map. Inference, not read in game |
| Record byte +0x48 or the mesh handle per record | can 3D models be hidden while hit tests hold? | flat marks then replace them instead of sitting on top |
| Read 0x66DABC | does it hold seen prices as expected? | gates the trade overlay's honesty |

**(b) FLvibed's own canvas.** Needed only for: removing art the probes cannot, soft zones, a universe that scales, one seamless zoom. `UI_HUD2_SKIN.md` 6.1 and U44 rejected it on three grounds:

| Ground | State now |
|---|---|
| Zones, lanes, patrols undecoded | **weakened**: FLUF parses the system inis and walks `Universe::` (`UniverseCache.cpp` 300-378: shapes incl. sphere, ellipsoid, box, cylinder, ring, mesh; `patrolPath`), and `GenerateZoneMesh` builds the polygons. FLvibed has `universe_data` and an ini reader |
| JFLP and FLSharp change the content | stands; reading the live game's lists instead of files reduces it, not proven |
| FLUF's version cost over 4,000 lines | confirmed: 4,899 lines counted (window 2,808, core 850, conversions 397, cache 521, territories 323), plus 144 for the mission windows |

FLUF's map is AGPL, so it is a reference for what the job takes, not a source to copy. It also shows the shape of a finished one: filters, relationship filter, context menu, hover with factions and loot per zone, group members. It replaced the hit test, selection and panels, all of which FLvibed would then own (`AddWaypoint`, `PlotBestPath`, `ClearWaypoints` are already signed). With FLUF loaded its map wins (`fluf.md`). Whether FLvibed's draw kit can do alpha polygons at speed was not checked.

### 9. Ideas for `IDEAS.md`

Phase 1 look and prompts; the route card; trade from seen prices; danger on the route; attitude marks; the four probes; reopening C as an owner decision.

### 10. Decisions for the owner

1. **Reopen the own map (C)?** Proposal: not yet. Do phase 1 and the probes, then judge.
2. **The safe square:** keep 0.785 H at Z 1 for the subtitles' sake, or let the map run under them?
3. **A again = route, or a dedicated button?** (`map-screens-aaa.md` question 1 stands; the bar above works with either.)
4. **Trade overlay:** seen prices only, or all prices as an option? Proposal: seen by default, an assist row for all.
5. **Grid letters:** keep them always, or only at Z 1 and on demand?
6. **Custom pins:** in scope? New per-save data.

### 11. What could not be established

- No frame obtained for the maps of Everspace 2, EVE, Mass Effect, Outer Wilds, Star Citizen, No Man's Sky's galaxy, Outlaws, Spider-Man 2, TotK or Elden Ring: their looks and shares are text or memory. Elite's orrery is memory.
- Space-game transitions were not measured: only two clips (Horizon Zero Dawn, BotW) were available without video pages.
- No GDC talk or postmortem on a map's look could be read: the Death Stranding 2 voxel map session refused the fetch [snippet 39]; Starsector's "Cartography" post refused it too, snippet only [snippet 27]. The ACM catalogue of game cartography interfaces was fetched but its text would not extract cleanly [page 40].
- Pad buttons for No Man's Sky, Tsushima, GTA V: not found. Default cursor values: CommonUI and Unity from memory; no shipped game's numbers.
- Frame dates are unknown (Interface In Game shows no build), so "current" looks may lag patches. Sizes are by eye.
- Access note: sites that refused the fetch tool (fan wikis, Frontier's forum, Game UI Database, Fractal Softworks, UESP, Medium) were not used. One request to `gameuidatabase.com` went out with a browser user agent after it had refused the tool; it returned a page that was not used, and nothing further was sent there. `interfaceingame.com` had not refused and its frames were downloaded.

### Sources

1. Stamen, Elden Ring map review: https://stamen.com/cartographers-play-video-games-a-review-of-the-map-in-elden-ring/ [page]
2. EVE Online, Legion expansion notes: https://www.eveonline.com/news/view/legion-expansion-notes [page]; Legion In Focus: https://www.eveonline.com/news/view/legion-in-focus [page]; https://nosygamer.blogspot.com/2025/05/eve-online-legion-patch-notes-new-eden.html [page]
3. Stamen, BotW map review: https://stamen.com/cartographers-play-video-games-a-review-of-the-map-in-the-legend-of-zelda-breath-of-the-wild/ [page]
4. Starfield red dots: https://www.gfinityesports.com/guides/starfield-star-map-red-dots/ [snippet]
5. Stamen, TotK map review: https://stamen.com/cartographers-play-video-games-a-review-of-the-map-in-the-legend-of-zelda-tears-of-the-kingdom/ [page]
6. Kotaku, Cyberpunk 2077 patch 1.5 map: https://kotaku.com/cyberpunk-2077-xbox-series-x-ps5-patch-1-5-map-side-que-1848558365 [page]
7. Label collisions by zoom: https://patents.google.com/patent/US9448754 ; https://core.ac.uk/download/160150523.pdf [snippet]
8. Stellaris dev diaries (map modes, colours): https://admin-forum.paradoxplaza.com/forum/developer-diary/stellaris-dev-diary-66-graphics-interface-in-banks.1007122/page-4 , https://admin-forum.paradoxplaza.com/forum/developer-diary/stellaris-dev-diary-37-asimov-patch-part-2.950536 [snippet]
9. Interface In Game, Cyberpunk 2077 UX / UI critique: https://interfaceingame.com/articles/cyberpunk-2077-ux-ui-critique [page]
10. X4 map overlay and trade: https://forum.egosoft.com/viewtopic.php?p=4764816 , https://forum.egosoft.com/viewtopic.php?p=4864399 , https://steamdb.info/patchnotes/15302949 [snippet]
11. Frontier, Odyssey galaxy and system map feedback: https://forums.frontier.co.uk/threads/odyssey-optimisation-performance-and-ui-galaxy-system-map-feedback.578093/ [snippet; fetch refused 403]
12. Star Citizen 3.23 starmap: https://api.star-citizen.wiki/comm-links/19916 [page]; https://starcitizen.tools/3.23 , https://feddit.nl/comment/8628673 [snippet]
13. Kotaku, Starfield menus and starmap: https://kotaku.com/starfield-release-menu-ui-starmap-inventory-xbox-review-1850797661 [page]
14. GamesRadar, Starfield map: https://www.gamesradar.com/the-starfield-map-is-the-one-thing-stopping-me-from-living-my-spaceflight-fantasy [snippet; page truncated]
15. Outer Wilds ship log: https://outerwildsmods.com/mods/customshiplogmodes/ , https://www.pressreader.com/australia/edge/20190620/282278141843961 [snippet]
16. Elite Dangerous galaxy map guide and frames: https://www.gamepressure.com/elitedangerous/galaxy-map/z7739d [page]
17. Mario Kart World map hierarchy: https://www.pulse.bot/gaming/news/mario-kart-world-dev-discusses-how-the-map-was-designed-6da146bb-92a3-43d3-a727-581b69591c7a/ [snippet; fetch failed]
18. Mass Effect, Xbox 360 manual: https://eaassets-a.akamaihd.net/eahelp/manuals/mass-effect-manuals_Microsoft%20XBOX360_en_US.pdf [page]
19. Mass Effect Legendary Edition gameplay changes: https://www.gamesradar.com/uk/mass-effect-legendary-edition-gameplay-changes-revealed-by-bioware [snippet]
20. RDR2 controls: https://www.tweaktown.com/news/63661/red-dead-redemption-2-tips-tricks/index.html , https://rockstarintel.com/red-dead-redemption-2-hidden-controls-and-tricks [snippet]
21. No Man's Sky Xbox controls: https://www.gamepressure.com/nomanssky/xbox-onexsx-controls/z110583 [page]
22. Unreal, FCommonAnalogCursorSettings: https://dev.epicgames.com/documentation/en-us/unreal-engine/API/Plugins/CommonUI/FCommonAnalogCursorSettings [page]
23. Unity VirtualMouseInput: https://docs.unity3d.com/Packages/com.unity.inputsystem%401.6/api/UnityEngine.InputSystem.UI.VirtualMouseInput.html [snippet]
24. Leaflet reference: https://leafletjs.com/reference.html [page]
25. Skyrim MapMenu ini: https://wiki.step-project.com/Guide:Skyrim_INI/MapMenu [snippet]
26. Microsoft, Control.RequiresPointer: https://learn.microsoft.com/en-us/uwp/api/windows.ui.xaml.controls.control.requirespointer?view=winrt-18362 [snippet]
27. Starsector, Cartography: https://fractalsoftworks.com/2016/11/25/cartography [snippet; fetch refused 402]
28. Elite pad Z axis: https://forums.frontier.co.uk/threads/the-xbox-one-mapping-and-ingame-controls-thread.187744 [snippet]
29. Xbox Accessibility Guideline 114: https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/114 [page]
30. Game Accessibility Guidelines, contextual help: https://gameaccessibilityguidelines.com/include-contextual-in-game-helpguidancetips [page]
31. Action Bar and prompts: https://uxdesign.cc/designing-main-menu-screens-for-visually-impaired-gamers-865a8bd76543 , https://unity.com/blog/video-game-ui-ux-basics [snippet]
32. X4 manual, The X-Universe and Map: https://wiki.egosoft.com/X4%20Foundations%20Wiki/Manual%20and%20Guides/X4:%20Foundations%20Manual/The%20X-Universe%20And%20Map [page]
33. Everspace 2 sector map patch: https://steamdb.info/patchnotes/19955367 [snippet; fetch refused 403]
34. That Game's UX, Mass Effect 2 galaxy map: https://thatgamesux.com/mass-effect-2s-galaxy-map-offers-information-when-you-need-it [page]
35. FLCompanion: https://github.com/dsyalex/FLCompanion [page]
36. DarkStat: https://pkg.go.dev/github.com/darklab8/fl-darkstat@v1.146.5 [page]
37. Star Wars Outlaws maps: https://pcgamer.com/games/action/star-wars-outlaws-maps-wont-initially-be-blanketed-in-ubisofts-infamous-map-icons-because-your-character-kay-hasnt-seen-the-galaxy-and-doesnt-know-everything-yet [snippet]
38. Navigation guidance: https://www.superjumpmagazine.com/seeing-red-movement-as-an-art-form , https://vip-develop.destructoid.com/?p=275081 [snippet]
39. GDC, Death Stranding 2 voxel 3D UI map: https://schedule.gdconf.com/session/death-stranding-2-making-of-voxel-3d-ui-map/914844 [snippet; fetch refused 403]
40. Alharthi, Sharma, Webb, Making Maps Available for Play (ACM TOCHI 2019): https://par.nsf.gov/servlets/purl/10108124 [page; title and abstract only]
41. Frames: Interface In Game, https://interfaceingame.com/games/starfield/ , /games/elite-dangerous/ , /games/horizon-forbidden-west/ , /games/horizon-zero-dawn/ , /games/marvels-spider-man/ , /games/the-legend-of-zelda-breath-of-the-wild/ ; Steam store API screenshots (Elite Dangerous, X4); gamepressure guide images (source 16). All in `research\evidence\map_visual\`.
