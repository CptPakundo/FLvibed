// flmeta.js - the player's ships and equipment from the game's own data (no game run): what each ship and item does,
// costs, needs (level, reputation) and where it is sold; per ship the best gun / shield loadout a player of a given
// level can buy, with power-limited sustained damage. For docs\SHIP_EQUIPMENT_META.md and, later, the autopilot's
// purchase decisions.
//
//   node tools\flmeta.js [--json <out.json>] [--md]      (paths resolve from this file's location)
//
// Sources (DATA\...): SHIPS\shiparch.ini (ships: hull, mass, hold, hardpoints, steering), EQUIPMENT\*_equip.ini
// (Gun / Munition / ShieldGenerator / Power / Engine / Thruster / ...), EQUIPMENT\goods.ini + *_good.ini (prices;
// ship hull + package = default loadout), EQUIPMENT\market_ships.ini + market_misc.ini (MarketGood = good, level,
// reputation, ...: where sold), EQUIPMENT\weaponmoddb.ini (weapon type x shield type), UNIVERSE\universe.ini (bases,
// systems), research\strings\resources.tsv (names).
'use strict';
const fs = require('fs');
const path = require('path');

const root = path.resolve(__dirname, '..');
const args = process.argv.slice(2);
// --data <DATA folder>: another install or a decoded copy (e.g. vanilla 1.0 via adoxa's TINI) - for comparisons.
const data = args.includes('--data') ? path.resolve(args[args.indexOf('--data') + 1])
                                     : path.join(root, 'Install', 'Freelancer HD Edition', 'DATA');
const jsonOut = args.includes('--json') ? args[args.indexOf('--json') + 1] : null;

// ---------------------------------------------------------------- ini
function readIni(rel) {
  const file = path.join(data, rel);
  const out = [];
  let cur = null;
  for (let line of fs.readFileSync(file, 'latin1').split(/\r?\n/)) {
    line = line.replace(/;.*$/, '').trim();
    if (!line) continue;
    const s = line.match(/^\[(.+)\]$/);
    if (s) { cur = { type: s[1].toLowerCase(), kv: [] }; out.push(cur); continue; }
    const m = line.match(/^([^=]+?)\s*=\s*(.*)$/);
    if (m && cur) cur.kv.push([m[1].trim().toLowerCase(), m[2].trim()]);
  }
  return out;
}
const get = (sec, k) => { const e = sec.kv.find(x => x[0] === k); return e ? e[1] : undefined; };
const all = (sec, k) => sec.kv.filter(x => x[0] === k).map(x => x[1]);
const num = (sec, k, d = 0) => { const v = get(sec, k); return v === undefined ? d : parseFloat(v); };
const list = v => v.split(',').map(x => x.trim());

// ---------------------------------------------------------------- names
const strings = new Map();
for (const line of fs.readFileSync(path.join(root, 'research', 'strings', 'resources.tsv'), 'utf8').split(/\r?\n/)) {
  const p = line.split('\t');
  if (p.length >= 3) strings.set(parseInt(p[0], 10), p.slice(2).join('\t'));
}
const name = id => strings.get(parseInt(id, 10)) || null;

// ---------------------------------------------------------------- universe: bases, systems
const bases = new Map(), systems = new Map();
for (const s of readIni('UNIVERSE\\universe.ini')) {
  const nick = (get(s, 'nickname') || '').toLowerCase();
  if (s.type === 'base') bases.set(nick, { nick, system: (get(s, 'system') || '').toLowerCase(), name: name(get(s, 'strid_name')) });
  if (s.type === 'system') systems.set(nick, { nick, name: name(get(s, 'strid_name')) });
}

// ---------------------------------------------------------------- equipment
const equip = new Map();   // nickname -> {type, sec}
for (const f of fs.readdirSync(path.join(data, 'EQUIPMENT')).filter(f => /_equip\.ini$/i.test(f))) {
  for (const s of readIni('EQUIPMENT\\' + f)) {
    const n = (get(s, 'nickname') || '').toLowerCase();
    if (n) equip.set(n, { type: s.type, sec: s, file: f });
  }
}
const goods = new Map();   // nickname -> sec
for (const f of fs.readdirSync(path.join(data, 'EQUIPMENT')).filter(f => /^(goods|.*_good)\.ini$/i.test(f))) {
  for (const s of readIni('EQUIPMENT\\' + f)) {
    const n = (get(s, 'nickname') || '').toLowerCase();
    if (n && s.type === 'good') goods.set(n, s);
  }
}
const price = n => { const g = goods.get(n); return g ? num(g, 'price', NaN) : NaN; };

// Markets: every base selling a good, with the level and reputation it asks.
const markets = new Map();  // good -> [{base, level, rep}]
for (const f of ['market_misc.ini', 'market_ships.ini']) {
  for (const s of readIni('EQUIPMENT\\' + f)) {
    if (s.type !== 'basegood') continue;
    const base = (get(s, 'base') || '').toLowerCase();
    for (const mg of all(s, 'marketgood')) {
      const p = list(mg);
      const good = p[0].toLowerCase();
      // MarketGood = good, level, reputation, min, max, ?, price multiplier[, ...]
      if (!markets.has(good)) markets.set(good, []);
      markets.get(good).push({ base, level: parseFloat(p[1]), rep: parseFloat(p[2]) });
    }
  }
}
// Each base's own faction (MISSIONS\mbases.ini local_faction): fc_* = pirate / outlaw groups (no docking for a lawful
// player following the story), the rest = houses, corporations, guilds.
const baseFaction = new Map();
{ let b = null;
  for (const s of readIni('MISSIONS\\mbases.ini')) if (s.type === 'mbase') baseFaction.set((get(s, 'nickname') || '').toLowerCase(), (get(s, 'local_faction') || '').toLowerCase()); }
const lawfulBase = base => { const f = baseFaction.get(base); return !!f && !/^fc_/.test(f); };
// Each sale with what it asks of the player: the base's owner faction (docking needs the player's standing with it)
// and the MarketGood reputation (the player's reputation with that faction must reach it). The runtime filter is the
// player's own reputation at the autopilot's activation (user, 18 Oct 2026), not a fixed lawful / outlaw split.
const sold = good => (markets.get(good) || []).map(m => ({ ...m, baseName: bases.get(m.base)?.name, system: bases.get(m.base)?.system,
                                                            faction: baseFaction.get(m.base) || null }));
const minLevel = good => { const m = markets.get(good); return m && m.length ? Math.min(...m.map(x => x.level)) : null; };

// Weapon type x shield type.
const shieldMod = new Map();  // weapon type -> Map(shield type -> factor)
for (const s of readIni('EQUIPMENT\\weaponmoddb.ini')) {
  if (s.type !== 'weapontype') continue;
  const m = new Map();
  for (const v of all(s, 'shield_mod')) { const p = list(v); m.set(p[0].toLowerCase(), parseFloat(p[1])); }
  shieldMod.set((get(s, 'nickname') || '').toLowerCase(), m);
}

// ---------------------------------------------------------------- guns
const guns = [];
for (const [n, e] of equip) {
  if (e.type !== 'gun') continue;
  const g = e.sec;
  const ammo = equip.get((get(g, 'projectile_archetype') || '').toLowerCase());
  if (!ammo || ammo.type !== 'munition') continue;
  const m = ammo.sec;
  const hp = get(g, 'hp_gun_type') || '';
  const cls = parseInt((hp.match(/(\d+)$/) || [])[1] || '0', 10);
  const missile = !!get(m, 'motor') || !!get(m, 'seeker');
  const refire = num(g, 'refire_delay', 1);
  const hull = num(m, 'hull_damage'), energy = num(m, 'energy_damage');
  const speed = num(g, 'muzzle_velocity'), life = num(m, 'lifetime');
  guns.push({
    nick: n, name: name(get(g, 'ids_name')), hp, cls, turret: /turret/.test(hp), missile,
    ammo: get(m, 'requires_ammo') === 'true', weaponType: (get(m, 'weapon_type') || '').toLowerCase(),
    hull, energy, refire, power: num(g, 'power_usage'), speed, range: Math.round(speed * life),
    hullDps: hull / refire, energyPerSec: num(g, 'power_usage') / refire,
    price: price(n), level: minLevel(n), sold: sold(n),
  });
}

// ---------------------------------------------------------------- shields, power, engines, thrusters
const shields = [];
for (const [n, e] of equip) {
  if (e.type !== 'shieldgenerator') continue;
  const s = e.sec, hp = get(s, 'hp_type') || '';
  shields.push({
    nick: n, name: name(get(s, 'ids_name')), hp, cls: parseInt((hp.match(/(\d+)$/) || [])[1] || '0', 10),
    kind: (hp.match(/hp_(\w+?)_shield/) || [])[1] || '?', type: (get(s, 'shield_type') || '').toLowerCase(),
    capacity: num(s, 'max_capacity'), regen: num(s, 'regeneration_rate'), rebuild: num(s, 'offline_rebuild_time'),
    power: num(s, 'constant_power_draw'), price: price(n), level: minLevel(n), sold: sold(n),
  });
}
const thrusters = [];
for (const [n, e] of equip) {
  if (e.type !== 'thruster') continue;
  thrusters.push({ nick: n, name: name(get(e.sec, 'ids_name')), force: num(e.sec, 'max_force'), power: num(e.sec, 'power_usage'),
                   price: price(n), level: minLevel(n), sold: sold(n) });
}

// ---------------------------------------------------------------- ships
const ships = [];
const hulls = new Map();   // ship nick -> hull good
for (const [n, g] of goods) if (get(g, 'category') === 'shiphull') hulls.set((get(g, 'ship') || '').toLowerCase(), { nick: n, sec: g });
const packages = new Map(); // hull good -> package good
for (const [n, g] of goods) if (get(g, 'category') === 'ship') packages.set((get(g, 'hull') || '').toLowerCase(), { nick: n, sec: g });
for (const s of readIni('SHIPS\\shiparch.ini')) {
  if (s.type !== 'ship') continue;
  const nick = (get(s, 'nickname') || '').toLowerCase();
  const hull = hulls.get(nick);
  if (!hull) continue;   // not sold to players
  const pkg = packages.get(hull.nick);
  const addons = pkg ? all(pkg.sec, 'addon').map(a => list(a)[0].toLowerCase()) : [];
  const power = addons.map(a => equip.get(a)).find(x => x && x.type === 'power');
  const engine = addons.map(a => equip.get(a)).find(x => x && x.type === 'engine');
  const shield = addons.map(a => ({ n: a, e: equip.get(a) })).find(x => x.e && x.e.type === 'shieldgenerator');
  // Gun slots: each HpWeaponNN takes the highest class it is listed for.
  const slot = new Map(), shieldHp = [], powerHp = [];
  for (const v of all(s, 'hp_type')) {
    const p = list(v);
    let t = p[0].toLowerCase();
    // EngClass shorthand "TYPE MAX" (e.g. "elite_power 3") = hp_TYPE_special_MAX .. 1 (adoxa's EngClass readme)
    const short = t.match(/^(\w+_(?:power|engine)) (\d+)$/);
    if (short) t = `hp_${short[1]}_special_${short[2]}`;
    const c = parseInt((t.match(/(\d+)$/) || [])[1] || '0', 10);
    if (/^hp_gun_special_/.test(t)) for (const h of p.slice(1)) slot.set(h, Math.max(slot.get(h) || 0, c));
    if (/_shield_special_/.test(t)) shieldHp.push(t);
    if (/_power_special_/.test(t)) powerHp.push(t);   // a mountable power plant (EngClass-style mods; none in vanilla)
  }
  const torque = list(get(s, 'steering_torque') || '0,0,0').map(Number), drag = list(get(s, 'angular_drag') || '1,1,1').map(Number);
  const turn = Math.min(torque[0] / drag[0], torque[1] / drag[1]) * 180 / Math.PI;
  const shipDrag = num(s, 'linear_drag');
  const eng = engine ? { force: num(engine.sec, 'max_force'), drag: num(engine.sec, 'linear_drag') } : null;
  const shieldMax = shieldHp.reduce((a, t) => Math.max(a, parseInt((t.match(/(\d+)$/) || [])[1], 10)), 0);
  ships.push({
    nick, name: name(get(s, 'ids_name')), shipClass: num(s, 'ship_class'), type: get(s, 'type'),
    hull: num(s, 'hit_pts'), mass: num(s, 'mass'), hold: num(s, 'hold_size'),
    nanobots: num(s, 'nanobot_limit'), batteries: num(s, 'shield_battery_limit'),
    guns: [...slot.values()].sort((a, b) => b - a), shieldClass: shieldMax,
    shieldKind: (shieldHp[0] && (shieldHp[0].match(/hp_(\w+?)_shield/) || [])[1]) || '?',
    turnDeg: Math.round(turn), topSpeed: eng ? Math.round(eng.force / (eng.drag + shipDrag)) : null,
    powerCapacity: power ? num(power.sec, 'capacity') : null, powerCharge: power ? num(power.sec, 'charge_rate') : null,
    thrustCapacity: power ? num(power.sec, 'thrust_capacity') : null, thrustCharge: power ? num(power.sec, 'thrust_charge_rate') : null,
    stockShield: shield ? shield.n : null, stockPower: power ? (get(power.sec, 'nickname') || '').toLowerCase() : null,
    powerSlots: powerHp,
    hullPrice: num(hull.sec, 'price'), package: pkg ? pkg.nick : null,
    level: pkg ? minLevel(pkg.nick) : null, sold: pkg ? sold(pkg.nick) : [],
  });
}

// ---------------------------------------------------------------- best loadout per ship and level
// Guns: every slot takes the best hull-damage gun of its class or lower that the level allows (sold somewhere at
// that level; energy guns only - no ammo, no missiles). Sustained output is limited by the power plant: damage per
// second x min(1, charge rate / energy per second). Shield: the strongest the ship's shield hardpoint takes.
// Every ship is power-limited (the guns draw 2-3x the plant's charge rate): the capacitor carries a burst at full rate,
// then the output is the charge rate x the loadout's damage per energy. So the loadout maximises the damage over a
// fight of `fightSec`: min(dps * T, damage-per-energy * (capacity + charge * T)) - searched over the best candidates
// of each class (by damage per second and by damage per energy).
function fightDamage(pick, plant, T) {
  const dps = pick.reduce((a, g) => a + g.hullDps, 0), demand = pick.reduce((a, g) => a + g.energyPerSec, 0);
  if (!plant || !plant.charge || demand <= 0) return dps * T;
  return Math.min(dps * T, (dps / demand) * (plant.capacity + plant.charge * T));
}
// The power plants a ship can use: its own (fixed, as in vanilla / HD Edition), plus - when the data makes plants
// mountable (EngClass-style mods: a hp_*_power_special_N hardpoint on the ship and on the plant) - every plant sold at
// the level whose hardpoint type the ship takes.
function plantsFor(ship, ok) {
  const own = ship.powerCharge ? [{ nick: ship.stockPower, capacity: ship.powerCapacity, charge: ship.powerCharge, price: 0 }] : [];
  if (!ship.powerSlots.length) return own;
  const mountable = [...equip].filter(([n, e]) => e.type === 'power' && ship.powerSlots.includes((get(e.sec, 'hp_type') || '').toLowerCase()))
    .map(([n, e]) => ({ nick: n, capacity: num(e.sec, 'capacity'), charge: num(e.sec, 'charge_rate'), price: price(n), sold: sold(n) }))
    .filter(p => ok(p));
  return [...own, ...mountable];
}
function bestLoadout(ship, level, fightSec = 20, lawful = false) {
  const ok = g => lawful ? g.sold.some(m => m.level <= level && lawfulBase(m.base)) : g.sold.some(m => m.level <= level);
  const avail = guns.filter(g => !g.missile && !g.ammo && !g.turret && ok(g));
  const cands = ship.guns.map(c => {
    const k = avail.filter(g => g.cls <= c);
    const byDps = [...k].sort((a, b) => b.hullDps - a.hullDps).slice(0, 3);
    const byEff = [...k].sort((a, b) => b.hull / b.power - a.hull / a.power).slice(0, 3);
    return [...new Set([...byDps, ...byEff])];
  }).filter(c => c.length);
  let pick = [], best = -1, plant = null;
  for (const pl of plantsFor(ship, ok).length ? plantsFor(ship, ok) : [null]) {
    const walk = (i, cur) => {
      if (i === cands.length) { const d = fightDamage(cur, pl, fightSec); if (d > best) { best = d; pick = [...cur]; plant = pl; } return; }
      for (const g of cands[i]) { cur.push(g); walk(i + 1, cur); cur.pop(); }
    };
    walk(0, []);
  }
  const dps = pick.reduce((a, g) => a + g.hullDps, 0);
  const demand = pick.reduce((a, g) => a + g.energyPerSec, 0);
  const charge = plant ? plant.charge : 0, capacity = plant ? plant.capacity : 0;
  const sustained = charge ? dps * Math.min(1, charge / Math.max(demand, 1e-6)) : dps;
  const burstSec = charge && demand > charge ? capacity / (demand - charge) : Infinity;
  const sh = shields.filter(s => s.kind === ship.shieldKind && s.cls <= ship.shieldClass && ok(s))
                    .sort((a, b) => b.capacity - a.capacity)[0];
  const cost = pick.reduce((a, g) => a + (g.price || 0), 0) + (sh ? sh.price || 0 : 0) + (plant ? plant.price || 0 : 0);
  return { guns: pick.map(g => g.nick), dps: Math.round(dps), demand: Math.round(demand), sustained: Math.round(sustained),
           burstSec: Math.round(burstSec * 10) / 10, fight20: Math.round(best), plant: plant ? plant.nick : null,
           shield: sh ? sh.nick : null, shieldCapacity: sh ? Math.round(sh.capacity) : 0, cost };
}

// ---------------------------------------------------------------- the enemy side: NPC ships by level
// MISSIONS\npcships.ini [NPCShipArch]: level dN, ship_archetype, loadout (SHIPS\loadouts*.ini: equip = item, hp).
// Per NPC: hull, shield capacity, the guns' hull damage per second. Job difficulty = the sum of its enemies'
// RandomMissions\npcranktodiff.ini entries (rank, group size), so a job's difficulty predicts what it sends.
const shipArch = new Map();
for (const s of readIni('SHIPS\\shiparch.ini')) if (s.type === 'ship') shipArch.set((get(s, 'nickname') || '').toLowerCase(), s);
const loadouts = new Map();
for (const f of fs.readdirSync(path.join(data, 'SHIPS')).filter(f => /^loadouts.*\.ini$/i.test(f))) {
  for (const s of readIni('SHIPS\\' + f)) {
    if (s.type === 'loadout') loadouts.set((get(s, 'nickname') || '').toLowerCase(), all(s, 'equip').map(e => list(e)[0].toLowerCase()));
  }
}
const gunBy = new Map(guns.map(g => [g.nick, g]));
const npcs = [];
for (const s of readIni('MISSIONS\\npcships.ini')) {
  if (s.type !== 'npcshiparch') continue;
  const lvl = parseInt(((get(s, 'level') || '').match(/d(\d+)/) || [])[1] || '0', 10);
  const arch = shipArch.get((get(s, 'ship_archetype') || '').toLowerCase());
  const lo = loadouts.get((get(s, 'loadout') || '').toLowerCase()) || [];
  if (!lvl || !arch) continue;
  const gl = lo.map(x => gunBy.get(x)).filter(g => g && !g.missile && !g.ammo);
  const sh = lo.map(x => equip.get(x)).find(e => e && e.type === 'shieldgenerator');
  const pw = lo.map(x => equip.get(x)).find(e => e && e.type === 'power');
  const raw = gl.reduce((a, g) => a + g.hullDps, 0), demand = gl.reduce((a, g) => a + g.energyPerSec, 0);
  const sustained = pw ? raw * Math.min(1, num(pw.sec, 'charge_rate') / Math.max(demand, 1e-6)) : raw;
  npcs.push({ nick: (get(s, 'nickname') || '').toLowerCase(), level: lvl, ship: (get(s, 'ship_archetype') || '').toLowerCase(),
              type: get(arch, 'type'), hull: num(arch, 'hit_pts'), shield: sh ? num(sh.sec, 'max_capacity') : 0,
              shieldType: sh ? (get(sh.sec, 'shield_type') || '').toLowerCase() : null,
              dps: raw, sustained, guns: gl.length, gunNames: gl.map(g => g.nick) });
}
const median = a => { const b = [...a].sort((x, y) => x - y); return b.length ? b[Math.floor(b.length / 2)] : 0; };
const enemyByLevel = {};
for (let l = 1; l <= 19; l++) {
  const f = npcs.filter(n => n.level === l && n.type === 'FIGHTER');
  if (!f.length) continue;
  enemyByLevel[l] = { count: f.length, hull: median(f.map(n => n.hull)), shield: Math.round(median(f.map(n => n.shield))),
                      dps: Math.round(median(f.map(n => n.dps))), sustained: Math.round(median(f.map(n => n.sustained))),
                      guns: median(f.map(n => n.guns)), ships: [...new Set(f.map(n => n.ship))].slice(0, 6) };
}
const hullFactor = (() => { for (const s of readIni('constants.ini')) if (s.type === 'shieldequipconsts') return num(s, 'hull_damage_factor', 0.5); return 0.5; })();
// Power plants sold on their own (mods that separate the reactor from the ship; none in vanilla / HD Edition).
const powerSold = [...equip].filter(([n, e]) => e.type === 'power' && markets.has(n)).map(([n]) => n);

const result = { ships, guns, shields, thrusters, npcs, enemyByLevel, hullFactor, powerSold, loadouts: {}, lawful: {} };
for (const sh of ships) {
  result.loadouts[sh.nick] = {};
  result.lawful[sh.nick] = {};
  for (const lv of [0, 2, 4, 6, 8, 10, 13, 16, 19, 22, 25, 30, 38]) {
    result.loadouts[sh.nick][lv] = bestLoadout(sh, lv);
    result.lawful[sh.nick][lv] = bestLoadout(sh, lv, 20, true);
  }
}
for (const s of ships) s.lawfulSale = s.sold.some(m => lawfulBase(m.base));
if (jsonOut) fs.writeFileSync(path.resolve(jsonOut), JSON.stringify(result, null, 1));

// --ship <nickname> [--level N] [--lawful]: one ship's best loadout as JSON (for the harness / autopilot: the ship
// from `pilot arch=`, the level from the player).
if (args.includes('--ship')) {
  const want = args[args.indexOf('--ship') + 1].toLowerCase();
  const s = ships.find(x => x.nick === want);
  const lv = args.includes('--level') ? parseFloat(args[args.indexOf('--level') + 1]) : 38;
  console.log(JSON.stringify(s ? { ship: s.nick, name: s.name, level: lv, ...bestLoadout(s, lv, 20, args.includes('--lawful')) }
                               : { error: `no player ship ${want}` }));
}

// ---------------------------------------------------------------- generated tables (docs\SHIP_EQUIPMENT_TABLES.md)
const mdOut = args.includes('--md') ? args[args.indexOf('--md') + 1] : null;
if (mdOut) {
  const L = [];
  const P = s => L.push(s);
  const where = x => [...new Map(x.sold.map(m => [m.base, m])).values()].slice(0, 3)
    .map(m => `${m.baseName} (${(m.system || '').toUpperCase()}${lawfulBase(m.base) ? '' : ', outlaw'})`).join(', ') || 'not sold';
  const gn = n => (guns.find(g => g.nick === n) || {}).name;
  const sn = n => (shields.find(g => g.nick === n) || {}).name;
  const count = a => Object.entries(a.map(gn).reduce((o, n) => (o[n] = (o[n] || 0) + 1, o), {})).map(([n, c]) => `${c}x ${n}`).join(', ');
  const clsName = c => ['light fighter', 'heavy fighter', 'freighter', 'very heavy fighter'][c] || c;
  const byLevel = [...ships].filter(s => s.package && s.sold.length).sort((a, b) => a.level - b.level || a.hullPrice - b.hullPrice);
  P('# Ships and equipment - generated tables');
  P('');
  P('Generated by `node tools\\flmeta.js --md docs\\SHIP_EQUIPMENT_TABLES.md` from the installed game data (HD Edition');
  P('0.7.1). Do not edit by hand: change the tool and regenerate. Reading guide: docs\\SHIP_EQUIPMENT_META.md.');
  P('');
  P('## Ships sold to players');
  P('');
  P('| Ship | nickname | level | class | hull | gun slots (class) | shield | turn deg/s | power cap / charge | hold | nanobots / batteries | hull price | sold at |');
  P('|---|---|---|---|---|---|---|---|---|---|---|---|---|');
  for (const s of byLevel)
    P(`| ${s.name} | ${s.nick} | ${s.level} | ${clsName(s.shipClass)} | ${s.hull} | ${s.guns.join('/')} | ${s.shieldKind} ${s.shieldClass} | ${s.turnDeg} | ${s.powerCapacity} / ${s.powerCharge} | ${s.hold} | ${s.nanobots} / ${s.batteries} | ${s.hullPrice} | ${where(s)} |`);
  const st = ships.find(s => s.nick === 'ge_fighter');
  if (st) P(`| ${st.name} (the story's starter, not sold) | ge_fighter | - | ${clsName(st.shipClass)} | ${st.hull} | ${st.guns.join('/')} | ${st.shieldKind} ${st.shieldClass} | ${st.turnDeg} | ${st.powerCapacity} / ${st.powerCharge} | ${st.hold} | ${st.nanobots} / ${st.batteries} | ${st.hullPrice} | - |`);
  P('');
  P('## Gun families (energy guns that are sold; every mark of a family has the same damage per energy)');
  P('');
  P('| Family | damage per energy | refire s | shot speed m/s | range m | weapon type | classes | sold at (first mark) |');
  P('|---|---|---|---|---|---|---|---|');
  const fam = new Map();
  for (const g of guns.filter(g => !g.missile && !g.ammo && !g.turret && g.sold.length)) {
    const k = g.nick.replace(/_mark\d+$/, '');
    if (!fam.has(k)) fam.set(k, []);
    fam.get(k).push(g);
  }
  for (const [k, gs] of [...fam].map(([k, gs]) => [k, gs.sort((a, b) => a.cls - b.cls)]).sort((a, b) => b[1][0].hull / b[1][0].power - a[1][0].hull / a[1][0].power)) {
    const g = gs[0];
    P(`| ${[...new Set(gs.map(x => x.name))].join(', ')} (${k}) | ${(g.hull / g.power).toFixed(2)} | ${g.refire} | ${Math.round(g.speed)} | ${g.range} | ${g.weaponType.replace('w_', '')} | ${gs[0].cls}-${gs[gs.length - 1].cls} | ${where(g)} |`);
  }
  const table = (title, set) => {
    P('');
    P(title);
    P('');
    P('| Ship | at level | guns | burst dps | sustained dps | burst lasts s | damage in a 20 s fight | best shield | gear cost |');
    P('|---|---|---|---|---|---|---|---|---|');
    for (const s of [...(st ? [st] : []), ...byLevel]) {
      const lvs = Object.keys(set[s.nick]).map(Number);
      for (const lv of [...new Set([Math.max(0, s.level || 0), 38])]) {
        const k = Math.max(...lvs.filter(x => x <= lv));
        const lo = set[s.nick][k];
        if (!lo.guns.length) continue;
        P(`| ${s.name} | ${k} | ${count(lo.guns)} | ${lo.dps} | ${lo.sustained} | ${Number.isFinite(lo.burstSec) ? lo.burstSec : 'no limit'} | ${lo.fight20} | ${sn(lo.shield) || '-'} (${lo.shieldCapacity}) | ${lo.cost} |`);
      }
    }
  };
  table('## Best loadout from lawful bases (what a story player can buy), at the ship\'s level and at the top level', result.lawful);
  table('## Best loadout from any base (outlaw bases included: needs their reputation)', result.loadouts);
  P('');
  P('## Shields (best per hardpoint kind and class; all sold)');
  P('');
  P('| class | light fighter | heavy fighter | freighter | level | shield type |');
  P('|---|---|---|---|---|---|');
  for (let c = 1; c <= 10; c++) {
    const b = k => shields.filter(x => x.kind === k && x.cls === c && x.sold.length).sort((a, b) => b.capacity - a.capacity)[0];
    const f = b('fighter'), e = b('elite'), fr = b('freighter');
    if (!f && !e) continue;
    const cell = x => x ? `${x.name} ${Math.round(x.capacity)} (+${x.regen}/s) ${x.price}` : '-';
    P(`| ${c} | ${cell(f)} | ${cell(e)} | ${cell(fr)} | ${(f || e).level} | ${(f || e).type.replace('s_', '')} |`);
  }
  P('');
  P('## Enemy fighters by NPC level (MISSIONS\\\\npcships.ini; medians; their guns at full rate - NPC loadouts carry no power plant)');
  P('');
  P('| level | NPC fighters | hull | shield | gun dps (max) | guns | ships |');
  P('|---|---|---|---|---|---|---|');
  for (const [l, e] of Object.entries(enemyByLevel)) P(`| d${l} | ${e.count} | ${e.hull} | ${e.shield} | ${e.dps} | ${e.guns} | ${e.ships.join(', ')} |`);
  fs.writeFileSync(path.resolve(mdOut), L.join('\n') + '\n');
}

// ---------------------------------------------------------------- report
const pad = (v, n) => String(v ?? '-').padEnd(n);
if (!args.includes('--ship')) console.log(`ships sold to players: ${ships.length}, guns: ${guns.length}, shields: ${shields.length}`);
if (!args.includes('--ship')) console.log('\nSHIPS (by level)');
if (!args.includes('--ship')) for (const s of [...ships].sort((a, b) => (a.level ?? 99) - (b.level ?? 99) || a.hullPrice - b.hullPrice)) {
  const where = [...new Set(s.sold.map(m => m.baseName))].slice(0, 3).join(', ');
  console.log(`${pad(s.name, 26)} ${pad(s.nick, 14)} lvl ${pad(s.level, 3)} class ${pad(s.shipClass, 2)} hull ${pad(s.hull, 5)} guns ${pad(s.guns.join('/'), 12)} shield ${s.shieldKind}${s.shieldClass} ` +
              `turn ${pad(s.turnDeg, 3)} top ${pad(s.topSpeed, 3)} power ${pad(s.powerCapacity, 5)}/${pad(s.powerCharge, 4)} hold ${pad(s.hold, 4)} bots ${s.nanobots}/${s.batteries} ` +
              `hull price ${pad(s.hullPrice, 7)} @ ${where}`);
}
