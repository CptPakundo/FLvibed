// The look of every shield hit and weapon impact, from the game's own data (user, 19 Oct 2026: a table of hit types
// for the HUD shield vignette - the fitted shield's hit look, tinted by the incoming weapon's impact).
// Joins st_equip.ini (shield -> shield_hit_effects per damage tier), weapon_equip.ini (gun -> munition ->
// munition_hit_effect, weapon_type, damage), effects.ini ([Effect] -> vis_effect) and the [VisEffect] blocks under
// DATA\FX (-> .ale file + effect name) with the .ale contents (tools\aledump.js): textures, colours, alpha / size
// keys, particle lifespans. Writes docs\kb\hit_effects.md (generated: re-run after an install change).
//   node tools\hitfx.js [<install DATA dir>] [--out <file>]
'use strict';
const fs = require('fs');
const path = require('path');
const { parseAle, fmt } = require('./aledump');

const root = path.resolve(__dirname, '..');
const args = process.argv.slice(2);
const outIx = args.indexOf('--out');
const out = outIx >= 0 ? args[outIx + 1] : path.join(root, 'docs', 'kb', 'hit_effects.md');
const data = args.find((a, i) => !a.startsWith('--') && (outIx < 0 || i !== outIx + 1)) || path.join(root, 'Install', 'Freelancer HD Edition', 'DATA');

// ---- INI (sections in order, repeated keys kept)
function ini(file) {
  const secs = [];
  let cur = null;
  for (let line of fs.readFileSync(file, 'latin1').split(/\r?\n/)) {
    line = line.replace(/;.*$/, '').trim();
    if (!line) continue;
    const m = line.match(/^\[(.+)\]$/);
    if (m) { cur = { type: m[1].toLowerCase(), kv: [] }; secs.push(cur); continue; }
    const e = line.indexOf('=');
    if (cur && e > 0) cur.kv.push([line.slice(0, e).trim().toLowerCase(), line.slice(e + 1).trim()]);
  }
  return secs;
}
const get = (s, k) => (s.kv.find(([a]) => a === k) || [])[1];
const all = (s, k) => s.kv.filter(([a]) => a === k).map(([, v]) => v);
const walk = (dir) => fs.readdirSync(dir, { withFileTypes: true }).flatMap((d) => (d.isDirectory() ? walk(path.join(dir, d.name)) : [path.join(dir, d.name)]));
const ciPath = (rel) => { // DATA-relative path, case-insensitive
  let p = data;
  for (const part of rel.split(/[\\/]/)) {
    const hit = fs.readdirSync(p).find((n) => n.toLowerCase() === part.toLowerCase());
    if (!hit) return null;
    p = path.join(p, hit);
  }
  return p;
};

// ---- effects and visual effects
const effect = {}; // nickname -> { vis, snd, type }
for (const s of ini(ciPath('FX/effects.ini'))) if (s.type === 'effect') {
  const n = (get(s, 'nickname') || '').toLowerCase();
  effect[n] = { vis: (get(s, 'vis_effect') || '').toLowerCase(), snd: get(s, 'snd_effect'), type: get(s, 'effect_type') };
}
const vis = {}; // nickname -> { alchemy, crc, textures }
for (const f of walk(ciPath('FX')).filter((f) => /\.ini$/i.test(f)))
  for (const s of ini(f)) if (s.type === 'viseffect') {
    const n = (get(s, 'nickname') || '').toLowerCase();
    vis[n] = { alchemy: get(s, 'alchemy'), crc: (Number(get(s, 'effect_crc')) >>> 0), textures: all(s, 'textures') };
  }
const aleCache = {};
function look(visName) { // the visual effect's appearance nodes, summarised
  const v = vis[visName];
  if (!v || !v.alchemy) return null;
  const file = ciPath(v.alchemy);
  if (!file) return { error: 'missing ' + v.alchemy };
  const ale = (aleCache[file] = aleCache[file] || parseAle(file));
  const e = ale.effects.find((x) => x.crc === v.crc) || ale.effects.find((x) => x.name.toLowerCase() === visName);
  if (!e) return { error: `effect_crc ${v.crc} not in ${v.alchemy}` };
  const apps = [];
  for (const n of e.nodes) {
    const q = n.p;
    if (!q.BasicApp_TexName && !q.ParticleApp_LifeName) continue;
    const feeders = e.nodes.filter((m) => m.targets.includes(n.id));
    const firstVal = (c) => (c && c[0] ? (c[0].k.length ? Math.max(...c[0].k.map((k) => k[1])) : c[0].def) : 0);
    const life = Math.max(0, ...feeders.map((m) => firstVal(m.p.Emitter_InitLifeSpan)));
    const burst = feeders.reduce((a, m) => a + (m.p.Emitter_InitialParticles || 0), 0);
    const rate = Math.max(0, ...feeders.map((m) => firstVal(m.p.Emitter_Frequency)));
    const ends = (a) => (a && a[0] ? [a[0].v[0], a[0].v[a[0].v.length - 1]] : null);
    const col = ends(q.BasicApp_Color), alpha = q.BasicApp_Alpha && q.BasicApp_Alpha[0].v, size = ends(q.BasicApp_Size);
    const hex = (c) => '#' + c.map((x) => Math.round(Math.min(1, Math.max(0, x)) * 255).toString(16).padStart(2, '0')).join('');
    const peak = alpha ? alpha.reduce((a, b) => (b[1] > a[1] ? b : a)) : null;
    apps.push({
      name: q.Node_Name, tex: q.BasicApp_TexName || '(particle: ' + q.ParticleApp_LifeName + ')',
      color: col ? (hex(col[0][1]) === hex(col[1][1]) ? hex(col[0][1]) : `${hex(col[0][1])} -> ${hex(col[1][1])}`) : '',
      alpha: alpha ? (peak[0] > 0.01 ? `0 -> 1 at ${fmt(peak[0])} -> ${fmt(alpha[alpha.length - 1][1])}` : `${fmt(alpha[0][1])} -> ${fmt(alpha[alpha.length - 1][1])}`) : '',
      size: size ? (size[0][1] === size[1][1] ? fmt(size[0][1]) : `${fmt(size[0][1])} -> ${fmt(size[1][1])}`) : '',
      life, burst, rate,
    });
  }
  return { effect: e.name, file: v.alchemy, apps };
}
const lookRows = (l) => (!l ? ['| (no visual) | | | | | |'] : l.error ? [`| ${l.error} | | | | | |`] :
  l.apps.map((a) => `| ${a.tex} | ${a.color} | ${a.alpha} | ${a.size} | ${a.life ? fmt(a.life) + ' s' : ''} | ${a.burst ? a.burst + ' at once' : ''}${a.rate ? (a.burst ? ', ' : '') + fmt(a.rate) + '/s' : ''} |`));

// ---- shields
const md = [];
const stEquip = ini(ciPath('EQUIPMENT/st_equip.ini')).filter((s) => s.type === 'shieldgenerator');
const families = {}; // visual of the lowest tier -> { shields, tiers }
for (const s of stEquip) {
  const tiers = all(s, 'shield_hit_effects').map((v) => v.split(',').map((x) => x.trim()));
  if (!tiers.length) continue;
  const key = tiers.map(([, e]) => (effect[e.toLowerCase()] || {}).vis).join('|');
  (families[key] = families[key] || { tiers, shields: [], types: new Set(), threshold: new Set() }).shields.push(get(s, 'nickname'));
  families[key].types.add(get(s, 'shield_type'));
  families[key].threshold.add(get(s, 'offline_threshold'));
}
md.push('# Shield hits and weapon impacts: how they look (game data)', '',
  'Generated by `tools\\hitfx.js` from the install\'s DATA (st_equip.ini, weapon_equip.ini, effects.ini, DATA\\FX ' +
  '[VisEffect] + .ale files via `tools\\aledump.js`); do not edit by hand, re-run after an install change. Colours are the ' +
  'first -> last colour keys (additive blending unless noted: they light up, black = invisible); alpha / size keys are ' +
  'over the particle\'s life (0..1); life = the emitter\'s particle lifespan.', '',
  '**What the frames showed (19 Oct 2026, `[game]` captures at ~150-185 fps, research\\experience\\shield_hits):** a hit on ' +
  'a shield draws BOTH the shield\'s effect (its family, below) and the weapon\'s impact effect; on a bare hull the ' +
  'weapon\'s stood out (the data also gives every ship a hull hit effect, a faint white ring + flare: `hull_damage.md`, ' +
  'not singled out in those frames). The damage tier (0 / 100 / 500 shield damage) changes only the SOUND for every family except Pirate (its ' +
  'three tiers have their own visuals) - all other tiers share one .ale effect.', '');
md.push('## Shield families', '');
for (const [key, f] of Object.entries(families)) {
  const visNames = key.split('|');
  md.push(`### ${visNames[0] || '?'} (${f.shields.length} shields: ${f.shields.slice(0, 6).join(', ')}${f.shields.length > 6 ? ', ...' : ''})`, '',
    `shield_type ${[...f.types].join(', ')}; offline_threshold ${[...f.threshold].join(', ')}; tiers: ` +
    f.tiers.map(([dmg, e]) => `>= ${dmg}: ${e} (sound ${(effect[e.toLowerCase()] || {}).snd}, visual ${(effect[e.toLowerCase()] || {}).vis})`).join('; '), '');
  for (const v of [...new Set(visNames)]) {
    const l = look(v);
    md.push(`Visual \`${v}\` (${l && l.file ? l.file : '?'}):`, '', '| texture | colour | alpha | size | life | particles |', '|---|---|---|---|---|---|', ...lookRows(l), '');
  }
}

// ---- weapons
md.push('## Weapon impacts (guns, by impact effect)', '');
const wsecs = ini(ciPath('EQUIPMENT/weapon_equip.ini'));
const munition = {};
for (const s of wsecs) if (s.type === 'munition') munition[(get(s, 'nickname') || '').toLowerCase()] = s;
const byImpact = {};
for (const s of wsecs) if (s.type === 'gun') {
  const m = munition[(get(s, 'projectile_archetype') || '').toLowerCase()];
  if (!m) continue;
  const hit = (get(m, 'munition_hit_effect') || '').toLowerCase();
  if (!hit) continue;
  const g = (byImpact[hit] = byImpact[hit] || { guns: [], types: new Set(), dmg: [] });
  g.guns.push(get(s, 'nickname'));
  g.types.add(get(m, 'weapon_type'));
  g.dmg.push(Number(get(m, 'hull_damage') || 0));
}
for (const [hit, g] of Object.entries(byImpact).sort()) {
  const e = effect[hit];
  const l = e && look(e.vis);
  md.push(`### ${hit} (${[...g.types].join(', ')}; hull damage ${fmt(Math.min(...g.dmg))}-${fmt(Math.max(...g.dmg))}; ${g.guns.length} guns: ${g.guns.slice(0, 5).join(', ')}${g.guns.length > 5 ? ', ...' : ''})`, '',
    '| texture | colour | alpha | size | life | particles |', '|---|---|---|---|---|---|', ...lookRows(l), '');
}
fs.writeFileSync(out, md.join('\n'));
console.log(`${Object.keys(families).length} shield families, ${Object.keys(byImpact).length} weapon impacts -> ${out}`);
