# FLvibed game knowledge base

What the game *is* and how it *plays*, for decisions on features and fixes (user-approved, 19 Oct 2026). Consulted
before every feature or bug fix (the checklist in `..\..\CLAUDE.md`), updated by every fix that teaches something.
Look things up here first; read the addresses and structures in `..\FREELANCER_RE.md` (engine facts) and the mission
runbooks in `..\missions\` (step by step) as before - this base links them by topic.

## Rules
- **Provenance on every fact:** `[game]` measured or seen in game (date), `[data]` read from the install's files,
  `[gen]` generated table (below), `[community]` wiki / forum / other projects (name it), `[footage]` the user's 2020
  playlist (catalogue id), `[assumed]` not verified - say so, verify before relying on it.
- **Matches the test install.** Generated tables carry the install's fingerprint (`generated\SOURCE.txt`); check with
  `tools\kbgen.ps1 -Check` and regenerate after the user updates or changes the install. Hand-written pages name the
  build they were verified on when it matters (HDE 0.7.1 unless stated).
- **Short and findable.** One topic per page, a table where a table fits, links instead of copies.
- **Write back.** A fix that taught something about the game adds it to its topic page in the same commit.

## Generated tables (`tools\kbgen.ps1`, ~4 s; git-ignored - they are the game's own data, regenerate locally)
| Table | What |
|---|---|
| `generated\hazards.tsv` | Every damage zone (lethal = damage >= 1000/s: all 119 are SPHEREs - planet and sun cores) and every danger field (mines, gas, density, DAMAGING) of every system |
| `generated\solars.tsv` | Every system object: nickname, archetype, position, base, dock link, reputation, name id |
| `generated\solararch.tsv` | Solar archetypes: type (WEAPONS_PLATFORM, STATION, PLANET...), hit points (inf = indestructible), radius, loadout, dock kinds |
| `generated\guns.tsv` | All 513 guns / launchers with their munition: kind (energy / ammo / homing), hull + energy damage incl. explosion, blast, muzzle speed, range, refire, hull dps, power, seeker, motor, ammo + prices |
| `generated\shields.tsv`, `engines.tsv` | capacity / regen / offline threshold; force, drag, top speed |
| `generated\ships.tsv` | All 69 ships: type, class, mass, hit points, strafe force and acceleration, drag, torque, nanobots / batteries, hold |
| `generated\loot.tsv` | lootprops drop rules (chance, worth, counts) with prices |
| `generated\msn_objects.tsv` | every mission-spawned ship / solar (13 missions, 1060): archetype, faction, labels, position, name, and what the script does to it: marked, invulnerable at times, destroyed by script, its kill trigger |
| `generated\msn_objectives.tsv`, `msn_triggers.tsv` | NNObjectives (ids / navmarkers / targets); every trigger's conditions + the pilot-relevant actions (marks, objectives, fails, invulnerability, destroys, spawns, nags) |

To add: factions and attitudes, loadouts.

## Topics
| Page | Covers |
|---|---|
| `navigation-and-hazards.md` | planets and death zones, fields, waypoints vs navmarkers, the native goto's behaviour, docking locks |
| `consumables.md` | nanobots / shield batteries: one press fills the bar, failed-shield reboot, the user's play from the 2020 footage (18 presses), spec match; ordnance use across the playlist (missiles M01-M04, torpedoes M11-M13) |
| `footage_scans.md` | the 2020 playlist's HUD lists read by OCR (weapon panel, contact list) at 2 fps: data files, speeds, what the contact list holds |
| `hit_effects.md` | generated (`tools\hitfx.js`): how every shield family's hit and every gun's impact looks (textures, colours, timing from the .ale files); a shield hit = shield effect + weapon impact, verified in game |
| `hud-controls.md` | the flight HUD's named controls (maneuver bar order, NeuroNet box, contact list, repair / battery, weapon groups) and the pad's commands (turret view, weapon groups, free flight vs cruise, the flight cursor's names), 25 Oct 2026 |
| `menu-screens.md` | the menu screens on the pad: lists selected natively (hovered / selected row fields), the save description edit and its WM_CHAR path, the chat field, the mode per save (saves.ini sidecar), speedrun mode (clock, splits, LiveSplit), 25 Oct 2026 |
| `hull_damage.md` | hull hit look (one visual for all stock ships; tiers differ in sound) and the damage fuses that light sparks / fire / smoke below per-ship hull thresholds, verified in game |
| `display.md` | window mode (bytes in Freelancer.exe set by HDE's installer), resolution (PerfOptions), DPI awareness and the DPI-unaware case, how the test instances switch them |
| `economy.md` | repairs at a base (the price is not the value restored; destroyed items stay gone; the busy NavBar after the window), purchases (cash -> value 1:1), net worth and the story's money gates |
| `combat-targets.md` | what can be fought (ships, platforms, destroyable stations, mission targets), shields vs missiles (offline threshold / 12 s window, the burst), our missiles' real hit rate and the run-in, duel tactics (agility, A/B of close / wide / slash), line of sight, fixed guns (mathed), key numbers |
| `missions\M01a.md`, `M01b.md`, `M02.md`, `M03.md`, `M04.md`, `M05.md` | set pieces of each mission for the autopilot and harness; `missions\M06_race_footage.md` the M06 ring race read from the footage |
| `story\README.md` | the story catalogue: arc, characters, per-mission beats and intent (survival / duty / raid), subtitle gaps |
| `world\README.md` | the world lore compendium: news, rumours, bar KNOW offers ranked by source (game data + story > story news > story rumours > generic); game-data corrections (zones, planets, wrecks, weapon-vs-shield table); 12 regional pages |

## Missions
`missions\<Mxx>.md`: what each mission *does* that a pilot must understand (set pieces, special spawns, what is marked,
what fails it) - distilled from the script (`DATA\MISSIONS`), the runbook and the footage. Template: M01b.
