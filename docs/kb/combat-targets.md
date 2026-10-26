# Combat targets

What can be fought, how hard it is, and what decides whether the autopilot should engage it. Code: the autopilot's
`Assess` (controller\src\impl\autopilot.cpp); tables `generated\ships.tsv`, `solararch.tsv`, `guns.tsv`, `shields.tsv`.

## What there is
| Kind | How to tell (game data) | Notes |
|---|---|---|
| Ships | CShip; `type` FIGHTER / FREIGHTER / GUNBOAT / CRUISER / CAPITAL... in shiparch `[gen]` ships.tsv | attitude from the faction (pub::Reputation); toughness = hit_pts (li_elite 1600) |
| Weapon platforms | solar `type = WEAPONS_PLATFORM`, hit_pts 1600 (M01b's `wplatform_m01b` 3000), radius 300, guns from the loadout `[gen]` solararch.tsv | targetable and destroyable with guns |
| Destroyable stations | solar `type = STATION` with finite hit_pts: 4 archetypes (o_osiris 100000, d_depot 400000, ...) `[gen]` | story targets; guns alone are too slow - torpedoes / the mission's own means |
| DESTROYABLE_DEPOT | 26 archetypes, all hit_pts 1e36 `[gen]` | `[assumed]` destroyed through parts (collision groups) or by script, not by hull damage - verify when a mission needs it |
| Indestructible solars | hit_pts 1e36: planets, docking rings, most stations, gates `[gen]` | never targets |
| Mission objects | spawned by the mission script (`Act_SpawnSolar` / ships), often marked (pub::Player::MarkObj) `[game]` | **the mission's targets rank first** whatever their kind (user, 19 Oct 2026) - not built yet |

## Rules the autopilot follows (19 Oct 2026)
- **Line of sight first:** a hostile hidden behind a solar or field rock is not a target (`space::SightBlocker`,
  PhySys::FindRayCollisions, real hulls); hidden by a station it is not even flown towards (the Rogue Base launches
  its fighters from inside its bays - M01b) `[game]`. Hidden hostiles within 3 km keep a fight from ending.
- **Destroyable hostile solars** are targets when our guns' measured damage rate takes their hull in <= 90 s
  (platforms yes, armoured bases no) `[game]`.
- **Missiles** only when the target's shield is down at impact (pub::SpaceObj::GetShieldHealth now, minus its measured
  drain over the flight): a missile meeting a shield is spent on it `[game]` (user saw a Javelin swallowed, 19 Oct).
  A shield fails at its generator's `offline_threshold` (15 %), not at 0 - predictions to 0 came true 1.0-1.5 s early
  every time; to the threshold the error is ~0.3 s - and stays down for exactly its `offline_rebuild_time` (12.00 s
  measured in every case, all 123 vanilla generators), coming back at the threshold `[game]` 21 Oct 2026.
- **The burst** (user, 21 Oct 2026): a failed shield is a reason to launch on its own; inside the 12 s window the
  launcher refires at its own rate (Javelin 2 s), launches start just before the predicted failure (a pair 0.4 s
  ahead landed 0.4 s after it), each missile only if it lands with 0.5 s of window to spare, and the overkill rule
  (guns over the flight + missiles already homing) ends it. Event `missile-launch why=burst window-left=`.
- **Our missiles hit** (measured 21 Oct 2026): the tracker counted a missile that killed its target as a miss (the hull
  read 0 in the kill frame) - "1 of 6" was that bug; recounted, the planner's launches hit ~84 % in the old logs and
  30 of 30 in the day's fight matrix. NPCs barely dodge. Old misses were the slow tail chase: < 150 m/s closing under
  200 m hit 76 %, fast closing ~93-97 %, flights under 0.6 s 100 %. The run-in now learns per target type (the flight
  limit tightens after a miss, relaxes with hits; event `missile-gate`) and holds a launch while the afterburner run
  still shortens the flight fast. Each outcome logs the launch geometry (target speed, turn rate, strafe, aspect,
  crossing, escape ratio). Javelin: pure pursuit, max_angular_velocity 2.90 rad/s, refire 2 s.

## Duel tactics (ship-adaptive, 21 Oct 2026)
Every ship's agility is readable (`space::ShipMotionOf`: Archetype::Ship steering torque / angular drag = the turn rate
it holds, strafe force / mass; debug `motion [id]`): our starter ship 1.67 rad/s, police light fighters 1.6, heavy
Rheinland 1.34, pirates 1.2, transports 0.28-0.5. Three ways to fly a duel against a fighter: close turn (the lead
pursuit at 40-110 m), wide turn (~180 m), slash (afterburner passes out to 700 m). A/B on the starter ship's fight
matrix, each forced (turn ratios 1.0-1.4), exchange = enemy hull taken / our points lost: **close 3.35, wide 2.52,
slash 1.38** (slash 2-4x slower, a single pirate not killed in 150 s). The NPC pilots punish passes. The autopilot
chooses by the turn-rate ratio (close first wherever we turn about as well), measures each option's exchange in the
fight and switches when another should be 25 % better, remembering per pair of ship types (event `duel-choice`,
`duel-switch`, `duel-option`; settings `adaptive`, `duel` to force one). Untested: a ship that clearly out-turns us.
Duel geometry per fight (`they-on` / `we-on`: noses within 15 deg inside 900 m): they-on 0.09-0.52, we-on 0.26-0.66.
- **Big ships' bow arc** (user, 21 Oct 2026, M04: a cruiser's main guns fire straight ahead): inside 35 deg of a
  gunboat / cruiser / capital ship's nose within 3 km the pursuit gives way to a run on the afterburner to a flank
  point beside it (>= 900 m off, perpendicular to its axis) until 50 deg off the axis, then the attack from the flank.
  The chase limit (8 s out of gun range -> given up for 30 s) must not count that run: it gave a Kusari gunboat up 8 s
  in and the ship idled ~30 s (fights +22-35 s); fixed 21 Oct 2026 (gunboat 25-40 s) `[game]`.
- **Fixed guns** (station or platform firing, no ship to fight): stay 500-900 m off, random strafe reversals every
  0.4-0.8 s, drift sideways, within 1.5 km of the fight; a straight retreat (still jinking) only below 30 % hull.
  Why (mathed, `[data]` guns.tsv): NPC gunners lead a constant-velocity target, so straight flight is hit at any
  speed; shots fly ~700 m/s (median of 512 guns), a 50 m/s strafe reversal puts the ship ~19 m off at 300 m (the
  Starflier is ~10 m in radius) and ~60 m off at 700 m; under ~250 m the shot lands before a reversal acts.

## Numbers worth knowing `[gen]`
- Javelin (`missile01_mark01`): 489 hull damage, blast 16 m, muzzle 30 m/s + ship speed, motor 22 m/s², seeker 1000 m,
  half-cone 35°, 25 credits. Mark II 747, Mark III 1212.
- Liberty light gun (`li_gun01_mark01`): 16.3 per shot, 750 m/s, range 600 m, refire 0.12 s = 136 hull dps if every
  shot hits.
- Starflier (`li_elite`): 1600 hull, 150 kg, strafe 20000 N = 133 m/s², 14 nanobots / batteries, hold 30.

## The target model (built 19 Oct 2026, M01b PASS: the platforms first)
1. **Mission-pointed hostiles first:** what the mission marks (pub::Player::MarkObj) or names as the objective's
   target objects (SetObjectives 'R'), host API v10 - the player's own information. Solars among them count when
   destroyable by guns within 10 minutes (finite hit points); battle reach applies.
2. Hostile ships by threat (attackers, fighters, damaged), as before.
3. Destroyable hostile solars opportunistically (<= 90 s of guns).
- **No progress = invulnerable for now:** 6 s on target without the hull moving (and no shield taking the hits) drops
  it for 20 s - mission scripts toggle invulnerability (`Act_Invulnerable`; M01b's platforms and base,
  `generated\msn_objects.tsv`).
- Not by guns at all: objects the script destroys itself (`Act_Destroy` / `LightFuse`: M01b's Rogue Base) or with
  indestructible hit points - the generated mission table shows which.
- Still open: other means (torpedoes, cruise disruptors, sub-targets / parts) for capital-class mission targets.
- Fight entry by cruise only when the target is > 3 km beyond gun range (it cruised at 20 m beyond once).

## Nomads and static targets (22 Oct 2026, M12 / M13)
- **Nomad fighter guns** (`nomad_gun01_mark02_ammo`): 1630 hull damage a bolt, refire 1 s, 750 m/s for 0.8 s = 600 m
  reach `[data]`. The "instant deaths at high hull" of the M12 / M13 legs were 3-4 such bolts at 40-115 m in < 0.3 s
  (`big-hit lost=1630`) `[game]`. A bolt from 600 m flies 0.8 s (dodgeable); from 100 m about 0.13 s.
- **Generators** (`dyson_generator` 50000 hp, the lair's shield generators, the city's `dyson_city_generator` radius 915
  m): no guns; their death fuse is effects only (no blast). The danger is the fighters around them.
- **The 2020 playthrough at generators** (catalogue c0006, c0011): attack passes - in fast, close along the hull
  firing (50-100 m), away low over the structure so the fighters lose the line, back again; batteries spent freely,
  nanobots kept (c0010). Inside the Dyson sphere: straight to the city through the fighters, the generators rushed
  (user: "bumrush the generators and ignore the fighters completely").
- **Own torpedoes**: a launch with a solid in the way or along the nose bursts beside the ship (M12, the lair lattice:
  dead at 93 % hull) - launches need a clear line through the real hulls.
