# Ships and equipment: the meta from the game's own data

What a player should fly and fit at each stage of the story to win fights fast, derived from the installed game data
(HD Edition 0.7.1) - for the autopilot's future purchase decisions (user, 18 Oct 2026: "know the meta-game of efficient
success"). The numbers come from `tools\flmeta.js` (reads the data, no game run); the full tables are generated into
`docs\SHIP_EQUIPMENT_TABLES.md` (`node tools\flmeta.js --md docs\SHIP_EQUIPMENT_TABLES.md --json
research\meta\meta.json`). The JSON has every ship, gun, shield, thruster, NPC and the computed loadouts for code to use.

## Sources (DATA\...)

| What | File / section | Fields used |
|---|---|---|
| Ships | `SHIPS\shiparch.ini` [Ship] | hit_pts, mass, hold_size, nanobot_limit, shield_battery_limit, hp_type (gun / shield classes per hardpoint), steering_torque / angular_drag (turn rate), linear_drag, ship_class |
| Ship prices, default loadout | `EQUIPMENT\goods.ini` [Good] category shiphull / ship | price; the package's `addon` = engine, power plant, shield, ... |
| Guns | `EQUIPMENT\weapon_equip.ini` [Gun] + its [Munition] | hp_gun_type (class), refire_delay, power_usage, muzzle_velocity; hull_damage, energy_damage, lifetime (range = speed x lifetime), weapon_type, motor / seeker (missiles) |
| Shields, power, engines, thrusters | `EQUIPMENT\st_equip.ini`, `misc_equip.ini`, `engine_equip.ini` | max_capacity, regeneration_rate, shield_type; capacity, charge_rate; max_force, linear_drag; thruster max_force, power_usage |
| Prices | `EQUIPMENT\*_good.ini` | price |
| Where sold, requirements | `EQUIPMENT\market_misc.ini`, `market_ships.ini` [BaseGood] | `MarketGood = good, level, reputation, ...` |
| Base owners | `MISSIONS\mbases.ini` [MBase] | local_faction (`fc_*` = outlaw groups) |
| Weapon vs shield | `EQUIPMENT\weaponmoddb.ini` | shield_mod per weapon type x shield type |
| Shield damage rule | `constants.ini` [ShieldEquipConsts] | HULL_DAMAGE_FACTOR = 0.5 |
| Enemies | `MISSIONS\npcships.ini` [NPCShipArch] + `SHIPS\loadouts*.ini` | level dN, ship_archetype, loadout |
| Job size | `RandomMissions\npcranktodiff.ini`, `diff2money.ini` | difficulty per NPC rank and group size; pay per difficulty |
| Names | `research\strings\resources.tsv` | ids_name / strid_name |

The rule for shields was checked against the modding community: a hit on a shield deals `energy_damage + hull_damage x
HULL_DAMAGE_FACTOR` and no hull damage while the shield is up ([The Starport wiki, weapon_equip.ini](https://the-starport.net/modules/mediawiki/index.php/MDB:weapon_equip.ini);
[Starport forum on HULL_DAMAGE_FACTOR](https://the-starport.net/freelancer/forum/viewtopic.php?topic_id=4611&start=0)); HD
Edition keeps the factor at 0.5 and every energy gun here has energy_damage 0, so **a gun hits shields for half its hull
damage** (x the weapon / shield type factor: lasers 0.8 vs Graviton shields, 1.2 vs Positron, ...). Stats pages online
are mostly vanilla 1.0 (the fan wiki refuses automated reads). **Vanilla vs HD Edition** (vanilla DATA decoded with
adoxa's TINI, `node tools\flmeta.js --data research\vanilla_text`): every player ship, gun, shield and price is the
same; HDE stopped selling the Starflier and changed 242 of 404 NPC loadouts' shields (docs\MOD_COMPATIBILITY.md).

## What decides a fight (the findings)

1. **Every ship is power-limited.** The best guns a ship can mount draw 2-3x what its power plant recharges (Defender:
   5 guns want 374 energy/s, the plant gives 157). The capacitor carries a full-rate burst of about 10-20 s, then the
   output falls to *charge rate x damage per energy*. So for anything longer than one burst, **damage per energy beats
   raw damage per second**, and a ship's power plant (fixed per hull in vanilla / HDE) is as important as its gun slots.
   `tools\flmeta.js` chooses loadouts by the damage dealt over a 20 s fight: `min(dps x T, efficiency x (capacity +
   charge x T))`.
2. **Damage per energy is a property of the gun family**, the same for every mark: Vulture 6.0; Angelito, Reaper,
   Protector, Scrambler 5.0; Lavablade / Magma Hammer, Pyros / Kraken, Flamecurse / Firekiss, Scorpion 4.8; Sunblast,
   Skyrail, Dragoon 4.0; Justice 3.2; pulse guns 0.4-0.5 (shield breakers, not killers). The mark only sets the class
   (and with it the raw damage). Choosing the family is the efficiency decision.
3. **Hit chance is not in these numbers.** Plasma guns (Lavablade, Flamecurse) fly at 500 m/s with a 0.5 s refire;
   lasers at 750 m/s with 0.12 s. Against agile fighters the faster, more frequent shots land more often; the autopilot's
   own hit statistics (FREELANCER_RE / controller CLAUDE.md) should weight the families per target class.
4. **Every ship has the same top speed (80 m/s; engines 48000 N / 600 drag) and the same cruise (300 m/s).** Ships differ
   in hull, gun slots, power, shield class, hold and **turn rate**: Starflier 95 deg/s, Drake 95, Patriot 92,
   Barracuda / Hawk / Falcon 77, Defender 72, most heavy fighters 60, freighters 52 and the Humpback 23. A high turn rate
   keeps the nose on target (time on target was the autopilot's main loss in the studies).
5. **Reputation gates the best guns.** Vulture (Kagoshima Depot), Pyros / Kraken (Outcast bases), Scorpion (Xenos) and
   the other `fc_*` families are sold only at outlaw bases a lawful story player cannot dock at. The tables give both
   "lawful bases" and "any base" loadouts; the story path uses the lawful one.
6. **Enemy toughness by level** (npcships.ini medians, fighters): d1-d3 ~1300-1900 hull and 620 shield; d4-d7
   1800-1900 hull and 840-1540 shield; d8-d11 3300 hull and 1700-2360 shield; d12-d14 6200 / 3100-3200; d15+ 9900-10800
   / 4400-8300. NPC loadouts carry no power plant (their guns are not power-limited in the data; their fire is limited by
   their pilots' accuracy and bursts instead).
7. **Jobs tell their size**: each wave is one `npcranktodiff.ini` row (an NPC level) at the column (ship count) matching
   the job's difficulty, the count drawn by weight (single player: 3 ships most often, never 1; adoxa's RE in the
   Starport KnowledgeBase), so a 0.11 job (the top of the lawful New York boards) is about 3 level-2 or 2 level-4
   fighters per wave. With the pay
   table (`diff2money.ini`) this gives pay per expected enemy - part of choosing jobs.

**Model check against a real fight:** the stock Starflier (2x Justice Mk I: 272 dps, not power-limited) needs 620 /
(272 x 0.5) + 1600 / 272 = ~10.5 s per d3 fighter at perfect accuracy; after the cruise fix the autopilot measured ~13 s
per kill (18 Oct 2026, job board, level-1 Rogues) - the model holds, at ~80 % effective accuracy.

## Availability follows the player, not a fixed side (user, 18 Oct 2026)

The player can switch the autopilot off and on at any time; at every activation it takes the player as they are.
What the player can buy is decided by **their reputation at that moment**: a base sells only to a player its owner
faction lets dock, and each `MarketGood` asks a reputation with that faction. The JSON carries both per sale
(`sold[]: base, faction, level, rep`), so the runtime filter is: player level >= level, player's reputation with
`faction` >= rep (and high enough to dock). The "lawful bases" tables below are the story path's example of that filter
(a player in good standing with the houses, hostile to the `fc_*` groups); a player allied with, say, the Outcasts gets
the Pyros / Kraken line instead, with the same optimizer.

## What to buy, stage by stage (lawful story path)

Levels per story stage from the campaign saves (docs\missions\SAVES.md): M01 0-1, M02 3, M03 5, M04 7, M05 9, M06 11,
M07 12, M08 14, M09 16, M10-M13 17, after the story 21+. Free time between missions asks for more net worth (Content.dll
story rule: +3500 after M01b, +5200, +7000, +10000, +16000, 0, +36000, +30000; docs\FREELANCER_RE.md).

| Stage (level) | Best value | Why (numbers from the tables) |
|---|---|---|
| M01 -> M02 (1-3) | **3x Lavablade Mk II on the Starflier** (860 each, Manhattan / Missouri / Norfolk / West Point / Fort Bush) | burst 272 -> 734 dps, sustained 272 -> 456 (+68 %); a d3 fighter in ~3.9 s instead of ~10.5 s (perfect hits); cheapest gain per credit |
| same, more money | **Defender** (li_elite, level 2, Battleship Missouri, hull 10400) with 5x Lavablade Mk III, Guardian H.F. shield | 5 slots (4/4/3/3/3), power 1500 / 157, hull 1600, elite shield class 4: sustained 754, 22 300 damage in 20 s (Starflier 13 900); turns 72 deg/s vs 95. The Patriot (Manhattan, level 1, 6600) is the cheap step: 4 slots, 605 sustained, keeps the 92 deg/s turn |
| M03-M04 (5-7) | Magma Hammer Mk I (class 4, level 6) in the Defender's class-4 slots; Adv. Guardian shield (level 6) | same efficiency (4.8), more damage per slot; shield 1225 -> 1678 |
| M05-M06 (9-11) | **Crusader** (br_elite, level 8, Bretonia battleships, 31 400) or **Cavalier** (br_fighter, level 6, 22 000, turns 69) | Crusader: 6 slots (5/5/5/5/4/4), 248 charge: sustained 1191 with Lavablade Mk III (lawful), 2600 hull |
| M06-M08 (11-14) | **Drake** (ku_fighter, level 10, Kusari planets, 44 500): turns 95 deg/s, 4 slots 6/6/5/5, charge 290; or **Dragon / Barracuda** (level 13, 63 200, 6 slots, charge 345; the Barracuda turns 77) | Drake keeps the Starflier's agility with 3x its power; Barracuda: sustained 1698 lawful, 3600 hull |
| M09-M13 (16-17) | Barracuda / Dragon with Sunblast A (Gas Miners, guild-owned) or Magma Hammer; Adv. Sentinel shield (level 16) | the Anubis (Order, level 16) is the strongest at 16 (6x class 6, charge 395, 4200 hull) |
| after the story (20+) | Falcon (level 24, 227 000, turns 77, charge 645), then Eagle / Sabre / Titan (level 30-32, 505 000-658 000) | Titan: charge 1190, 12 600 hull; class 10 shields 8280 |

Rules the autopilot can apply from this data:
- Fill every slot with the most damage-per-energy family the bases allow; take the highest mark that fits the slot.
  More guns of a lower class rarely help once the plant is the limit (the burst gets shorter, the sustained rate stays).
- Upgrade the ship when its *power charge* (not its slot count) is the limit: sustained damage scales with the charge.
- Prefer agile hulls (turn rate) at equal power; the autopilot's lead pursuit is limited by time on target.
- Shield class follows the ship; buy the highest the level allows (capacity grows ~37 % per class).
- Against a known shield type, pick a weapon type with a factor above 1 (weaponmoddb.ini).

## Open (not verified)

- ~~Whether buying equipment lowers net worth~~ **Settled (Server.dll GetAssetValue, verified):** equipment counts
  0.3 x price (x its health), the hull 0.8 x price, cargo and consumables full price. A purchase during free time
  delays the next mission by 70 % of its price in extra earnings: buy once the free time's target is met.
- Ship trade-in value (selling the old ship when buying a new one).
- Hit chance per gun family vs target class (needs the autopilot's in-game statistics).
- Missiles, torpedoes, mines, countermeasures: listed in the JSON (`guns` with `missile` / `ammo`), not ranked yet.
- Thrusters (afterburner force and its power use: the thrust capacity of the plant) - in the JSON, not ranked.
- Mods that sell power plants separately (the user's note): `powerSold` in the JSON lists them (none here); the
  optimizer reads the plant from the ship's package, so such mods need a plant choice added.
- Reputation needs per item: `MarketGood` reputation is in the JSON (`sold[].rep`); the tables only separate lawful and
  outlaw bases.
