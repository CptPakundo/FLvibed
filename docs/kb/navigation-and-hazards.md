# Navigation and hazards

## Planets, suns, death zones
- Every planet and sun sits inside a **death zone**: a `[zone]` with `damage` (2000000/s for planets, 10000/s for the
  Li01 sun) - contact kills at once. All 119 lethal zones of the install are SPHEREs `[gen]` hazards.tsv. The visible
  planet is smaller than its zone (Manhattan: zone r 4100) `[data]`.
- The **native goto** flies straight in, brakes hard ~600 m from the zone's surface and slides along it at 600 m
  until it can turn to the target (auto avoidance toggle irrelevant) `[game]` 19 Oct 2026, flight recorder.
- FLvibed (since 20 Oct 2026, user): lethal spheres are avoidance obstacles only (never "strict" fields), with a berth
  from the ship's own flight model - max(2 ship radii, 0.5 s of travel + half the turning radius) ~244 m for the
  Starflier at cruise (was the game goto's 600 m: stations sit closer to the atmosphere, e.g. Trenton) - planned ahead
  `[game]` M01b passes the Pittsburgh leg since 19 Oct (it died twice before on a straight leg through the zone).
- Radiation zones (damage 40-50) are crossable but "strict" (slow, careful) `[data]`.
- A planet is not docked with: its dock is a separate ring (Manhattan Docking Ring) `[game]`. Arrivals at a
  non-dockable object stop outside its radius and costly zones + 300 m `[game]`.

## Fields
- Danger fields by their file's `[Properties] flag`: mine_danger_objects, gas_danger_objects, object_density_high,
  DAMAGING; exclusion zones carve lanes and gates out of them `[data]` hazards.tsv.
- Field rocks (class 0x1003) are solid; dynamic asteroids (0x4001, debris < 40 m) break harmlessly and drop loot -
  not avoided (user, 19 Oct 2026) `[game]`.

## Waypoints vs navmarkers
- The player's waypoint list (HUD "Player Waypoint n") and the mission's **navmarker** ("Mission Waypoint") are
  different: an objective's navmarker comes with the objective (SetObjectives 'N' entry) and may never enter the
  list (M01b "Fly to the Waypoint") `[game]`. The 2020 footage shows only the Mission Waypoint `[footage]` c0003.
- Never add a player waypoint on top of the mission's (no duplicates); FLvibed plans with the game's best path and
  restores the player's list at once `[game]`.

- An objective's `target=` ('R' entry) is a **reputation instance**: Common.dll `FmtStr::append_rep_instance` writes
  kind 'R' (`append_spaceobj_id` writes 'O') `[game]` disassembly 19 Oct 2026. For mission ships it is small (M03 "Fly
  to Walker" target=8) and **overlaps the small space ids of ships** in single player (S010: reps 2-20 = ships 13-31);
  M03 "Fly to Planet California Minor" sent the docking ring's space id (a nickname hash). Resolve by the contacts'
  `pub::SpaceObj::GetRep`, by id only for non-ships: `space::ObjectiveObject`, debug `objobj <ref>`, harness
  `Resolve-FLObjectiveTarget`; the HUD mark + the objective's name (`Get-FLMarkedFriendly`) stays the cross-check.
  Host API v11 hands marks (space ids) and objective refs over apart `[game]` 19 Oct 2026.
- A mission navmarker can sit **just outside** a lethal zone (M01 "Meet King": 129 m off Manhattan's 4100 m death
  zone): the avoidance's berth (shrunk to the aim's clearance) left a sliver and the ship swerved 22 s. An arrival at a
  point inside or within 300 m of a lethal zone ends at the zone's surface + 300 m, out from the zone's centre
  (route.cpp `StandOff`) `[game]` 21 Oct 2026.

## Docking and locks
- **The docking / landing cutscene is skipped by the autopilot itself** (user, 21 Oct 2026: "the planet landing
  animation could be skipped just the same as the launch animation"): USER_CANCEL on the docking camera
  (exe::CamDocking) right after the autopilot's own dock (or under the objective mode), once, again after 1 s if it
  still runs; story cutscenes in space are never sent it. Before, only some harness calls skipped it: the objective
  mode's docks played 5-6 s, M03's Willard 14 s. Now 0.4 s cutscene to docked (Manhattan ring). Setting
  `skip_dock_scene`; event `dock-cutscene skipped` `[game]`.
- During the game's own docking sequence the ship is locked (mission-hold flags) a frame before the docking cutscene:
  after our own dock request that is the dock going through, not a mission takeover `[game]` 19 Oct 2026.
- Hostile bases refuse docking (attitude <= -0.6) `[game]`.
- **Planet docking rings (dock type 6): straight in, request early** (user, 20 Oct 2026, M04 Leeds: "swerve sideways
  and up before finally landing"; the 2020 footage: straight at the ring at ~300 m/s, 299 -> 35 -> 4 m/s in the last
  2 s, the game's dock taking over). A ring has 2 dock points, parallel 112 m lines that enter at 106 deg (Manhattan)
  to 160 deg (Leeds, coming from the Leeds hole) to a straight approach: lining up on one was the swerve, and slow
  (requested at p2: 16-28 s more to docked). The fast dock aims straight at p2 and requests 1.4 s out, at least
  400 m (a 200 m floor deadlocked: slowed beside the Leeds ring, 300 m from p2, at rest 3 min); requested that way
  the game docked us 6-7 s later. Any lined-up approach at rest 3 s without a request requests (failsafe).
  **Measured, not copied** (user: fast and normal docks stay distinct, the fast one is whatever is fastest - the 2020
  footage mixes both): A/B at the Manhattan ring from 5 km (`research\tools\ringdock_ab.ps1`, 3 games x 3 rounds,
  9/9 docked each): straight in **31.2 s** median (31.1-31.5), line-up on the dock point (`ap set ring_lineup 1`)
  36.4 s, the game's own dock (`ap set fast_dock 0`) 37.9 s `[game]` 20 Oct 2026. The line-up switch was removed on
  24 Oct 2026 (244a4b7, the slower variant); straight in and the game's own dock stay selectable.
- **A ring requested at p2 at full speed lands at once** `[game]` 24 Oct 2026: the archive's 80+ ring landings
  requested at ~400 m took 6.6-7.0 s from the request to the landing scene whatever the speed (194-300 m/s) or the angle
  (way-in 6-162 deg) - the game's dock drops the cruise at once (300 -> 83 m/s) and flies the rest itself. Requested
  10-11 m before p2 at 293 m/s, straight in from way-in 120 deg (no line-up), the landing scene started 0.03 s later:
  the dock from 5 km 25.0 s vs 30.8-30.9 s (Manhattan ring, `research\tools\ringdock_p2.ps1`). The top-3 any% runs land
  so at Toledo (M12: 299 m/s into the ring, HUD up, the scene at the ring - REF run 3 v13840-13847). Setting
  `ring_request` (m before p2; a pass beyond p2 requests at once).
- **A ring behind its planet** (21 Oct 2026, M04): Planet Leeds' ring (13215, 0, 35862) sits ~150 m outside the
  planet's 3100 m death zone (centre 16369, 0, 36660) on the far side from the Iw03 hole (33043, 0, 50098): the
  straight line hole -> ring passes ~1.2 km from the planet's centre, inside the zone. The avoidance flies round the
  zone's berth (~1.5 km more at cruise), so every M04 Leeds dock was 4.7-4.9 s over the straight-line plan (28.3 vs
  23.5 s, 6 of 6 runs; the request then at the normal 400 m). The flown path (recorded): straight until the zone's
  clearance is inside the steering's ~5 s look-ahead, then hugging the zone at ~305 m off its surface into p2. Plans
  now measure that way (`avoid::PathLength`: straight to that bend point, then tangent - arc - tangent round each lethal
  zone / planet / big station with the steering's berth; a planet inside its own zone counts once - counted twice it
  said 2046 m for ~1470 flown), in the fast dock's plan and the travel planner's free-flight legs. From 20 km off
  (4 km past the hole): ideal tangent alone 900 m extra, with the late bend 1236 m, flown ~1220; plan 76.4 s ->
  request at 77.4 s `[game]`.
- One ship per bay: a request while another ship is docking puts us in the game's queue ("dock in use, entering
  queue"); mission companions dock at the same moment as the player (M03 Willard: Juni first, our fast dock planned
  7.6 s, docked after ~35 s) `[game]` 19 Oct 2026. Harmless; a companion could be let in first.
  The hold-short wait for a single bay flickered at Planet Leeds' ring (M05, 21 Oct 2026: ships 46 / 47 read busy and
  free by turns - 12 wait starts, the 10 s cap never ran, the approach burned between waits: "tapped the afterburner
  several times"). The wait gained nothing: the request at 400 m was granted at once (reply 4) and the dock took
  31.7 s vs 19.4 planned; the 2020 playthrough (9SPp6bNddzw 29:57-30:15) cruised straight to the ring and docked in
  ~18 s `[game]` `[footage]`. **Now (user decision, 21 Oct 2026): no hold short at bases** - the request goes in at
  the normal point and the server's answer decides (3 = queued: the game's own dock waits for its turn; logged as
  "fast-dock queued at a base"). Gates keep the queue shortcut. Not yet seen queued at a base since.
- **Queue shortcut (user's trick, 20 Oct 2026; footage not checked yet):** at a jump gate's or trade lane's entrance
  while a non-hostile NPC is in its docking animation, "Enter Formation" on it starts the player's jump / ride at once -
  no queue. User: "valid for jump gates and trade lanes for sure, jump holes maybe too. Definitely not docking or
  landing" (3 tries at the Manhattan docking ring: no effect). `[game]` 20 Oct 2026, navigation sweep x15 (Li01->Li02
  gate): 2 of ~6 gate tries took us in within 4 s (jump sequence, NPC chatter "not the only ones headed to the California
  system"); the misses targeted ships arriving out of the gate, the docking one already gone, or a patrol idling at a
  lane ring. Lane tries so far: none on a real entering ship.
- **"Busy" = going in:** a ship near the entrance blocks it (and is a shortcut candidate) only when it is slow (not a
  lane rider at ~2500 m/s) and closing on the entrance or inside it (< 60 m) - velocity from positions between frames.
  Gate arrivals spawn at the gate (Li01->Li02: -76629, 255, 42487) and fly out: not busy. `[game]` 20 Oct 2026.
- The formation is itself a priority directive: `ai::MissionHolds` reads true while it runs - never take it as "the
  game took us in"; the real signs are a dock port (`ai::DockInProgress`), a jump or a lane ride. A formation left
  running flies the player wherever its leader goes (a police leader took us into its fight with Rogues: death)
  `[game]` 20 Oct 2026. A miss sends the dock request at once (it replaces the formation).
- **The user's technique (20 Oct 2026):** "idle my ship right at the physical entrance to the jump gate, maybe even
  a little bit further in (as the gate opens), and then as the NPC is locked in the docking animation (their docking
  lights flash red)" hit formation - the jump starts at once; lanes alike from very close to the ring, like a fast
  dock. **Only a shortcut when the queue is not empty** - else the normal fast dock is optimal (user).
  User: **a race, never a wait** - "It's either a fast-dock or quickly ascertaining an NPC will hit the dock queue
  first, and then the player rushes in to the perfect spot to then toggle formation and instantly jump. The focus is
  to get the jump going as fast as possible."
- **Autopilot (`fast-dock shortcut-rush` events), jump gates only:** on the fast approach, an NPC going in whose ETA
  to the entrance (distance / closing speed, scanned within 2 km; locked in = 0) beats ours starts the rush - full
  speed to the entrance (p2 + 10 % of the approach line, max 30 m), no request, braking as late as the flight model
  allows; formation the moment a ship is **locked in** = on the docking animation's path (within 40 m of the bay line
  p2 -> mount, moving in or past p2: the client's NPC copies hold no dock port - probe 20 Oct 2026) and we are within
  80 m (the lock is tested on every dock point's path: the Li01->Li02 gate has one, others may have large berths).
  Called off when no ship is ahead any more (the fast dock goes on); at the entrance it stays while the ship ahead is
  still coming (up to 10 s: its queue would cost the same, leaving gives the moment away - 20 Oct 2026, the lock came
  3.4 s after our arrival, past the old 2 s cap that had sent 2 rushes into the queue), 2 s with none; 30 s in all.
  Queued while still at the entrance: the lock is watched there too.
  The server's "queued" (docs\FREELANCER_RE.md "Dock replies", host v14) starts the same rush once, if a ship going in
  is in sight. The Li01->Li02 gate's approach line p2 -> mount is 463 m. `[game]` 20 Oct 2026, 2 rounds x 3 sweeps:
  rushes with a lock 4 of 4 taken in (1.7-4.9 s; gate legs 113-118 s vs plan 112 - no queue cost), no lock 2 (queued
  20-24 s), called off 2 (no cost). Earlier tries from 100-635 m on ships merely closing (or a convoy's escort: the
  formation charged the cruise engine) 0 of ~9.
- **Lane ambushes (Rogue lane hackers):** Li01's lanes from Manhattan toward the California gate are disrupted often
  (about every other navigation run, 20 Oct 2026; rings 2256551873, 2256027521, 2793212811, 2793235339, ... at 12-35 km):
  "trade lane disrupted", Rogue Bloodhounds beside the ring - the game's encounter, not a fault; 0-3 hits, no deaths.
  The ring where the ride stopped is closed for the trip and the lane is **split** there: the re-plan re-enters past
  the ambush (was: the whole lane dropped, ~27 km cruise to the gate, +72 s); legs then match the re-plan (83.6 vs 84.1 s).
  `[game]` route.cpp Make. The split segments only served legs to a fixed point (gate, base); when the **next item
  is another lane** the best path's lane was still dropped whole and the ship cruised beside its rest (user saw it
  twice; 21 Oct 2026 Li01 147 -> 154 cut at 150: ~130 s cruise). Now the best path's lane keeps the part beyond its
  last closed ring as its item: `route 2793737163` from beside ring 150 = enter 151, ride to 154, next lane, gate:
  128 s (the live re-plan: 176 s). Debug `route [ring ...]` plans with those rings closed `[game]` 21 Oct 2026.
- **A hold begun in a jump** (21 Oct 2026, M05 at the hole to Leeds: the objective mode swapped the travel for a hold
  on the tunnel's objective " "): a "restart" stop keeps the cruise engine for the next mode and the game's Idle does
  not end it - the ship cruised 28 s at 299 m/s away from Juni; repro (research\tools\hold_after_jump.ps1, Li01 -> Li02
  gate): the start point came from the old system's coordinates and the hold flew at it 30 s, 1.2 km. Now a hold never
  cruises, has no point while `jumping`, and starts over in a new system: 0 m/s, 0 m drift at the exit. The 2020
  playthrough (9SPp6bNddzw 28:00-28:22): speed 0 at the hole's exit, turret view on the Rheinland ships `[game]`
  `[footage]`.
- **The lock test** wants a docker moving in: arrivals spawn in the gate's mouth and fly out along the same path
  (3 false locks before, 20 Oct 2026).
- **Lanes: no shortcut.** Every lane request today was granted at once (40+ replies kind 2 response 4, never queued;
  the user does not remember lane queues either) - a lane rush only cost time (+4 s, never a lock); lanes keep the
  plain fast entry and no hold-short. Holes: untested.
- **Airlocks: fly in, never dock** (22 Oct 2026; solar type AIRLOCK_GATE: the Dyson sphere door St03 -> St03b, the
  Nomad lair entry / core enter / exit in St02 / St02c; dock point type 8). Crossing the docking sphere
  (`docking_sphere = airlock, HpDockMountA, 233` outside, 100 inside) under the mission's dock grant switches the system
  at once and `CShip::airlock_in` places the ship inside at full throttle. A Dock maneuver there locks the player out
  for good: its start sets control exclusion 4 (letterbox + no steering), and an airlock's switch - unlike a gate's or
  hole's jump - has nothing that clears it (`..\FREELANCER_RE.md` "Control exclusion") `[game]` + `[decomp]`. The 2020
  playthrough flies straight into the opening door (M13 19:41-19:43, M12 5:12 / 5:52, HUD up) `[footage]`. A later jump
  (M12's jump hole) clears a lockout left earlier; M13 has none, so its whole sphere was flown locked out.

## Line of sight
- `PhySys::FindRayCollisions` (Common.dll) tests the real hulls in the physics world; FLvibed's `space::SightBlocker`
  wraps it (debug `sight <id>`) `[game]` + `[community]` FLUF uses it for explosion line of sight.
  `space::RayHitDistance` returns the nearest solid's distance (hits in a ray's last 3 % are its own end point, not a
  surface); the autopilot's geometry guard, launch checks, blind-spot reachability and pass exits all use it.
