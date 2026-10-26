// Freelancer Alchemy effects (.ale, UTF): each effect's node tree with every node's type, texture, blending, lifespans
// and animated colour / alpha / size keys - to compare the game's VFX assets with captured frames (tools\hitfx.js
// builds the shield-hit / weapon-impact table on it).
// Format: research\repos\KnowledgeBase\docs\file-structures\utf\ale.md + ale-nodes.md (property names; its
// 'Emitter_InitialLifeSpan' is really Emitter_InitLifeSpan - checked with the game's hash).
//   node tools\aledump.js <file.ale> [effectName...]        (CLI)
//   require('./aledump').parseAle(file) -> { effects: [{ name, nodes: [{ id, parent, type, p, targets }] }] }
'use strict';
const fs = require('fs');
const path = require('path');

const NAMES = ['Node_Name', 'Node_LifeSpan', 'Node_Transform', 'Node_ClassName', 'Emitter_LODCurve', 'Emitter_InitialParticles',
  'Emitter_Frequency', 'Emitter_MaxParticles', 'Emitter_EmitCount', 'Emitter_InitLifeSpan', 'Emitter_Pressure',
  'Emitter_VelocityApproach', 'CubeEmitter_Width', 'CubeEmitter_Height', 'CubeEmitter_Depth', 'CubeEmitter_MinSpread',
  'CubeEmitter_MaxSpread', 'SphereEmitter_MinRadius', 'SphereEmitter_MaxRadius', 'ConeEmitter_MinRadius',
  'ConeEmitter_MaxRadius', 'ConeEmitter_MinSpread', 'ConeEmitter_MaxSpread', 'Appearance_LODCurve', 'BasicApp_TriTexture',
  'BasicApp_QuadTexture', 'BasicApp_MotionBlur', 'BasicApp_Color', 'BasicApp_Alpha', 'BasicApp_Size', 'BasicApp_HtoVAspect',
  'BasicApp_Rotate', 'BasicApp_TexName', 'BasicApp_BlendInfo', 'BasicApp_UseCommonTexFrame', 'BasicApp_TexFrame',
  'BasicApp_CommonTexFrame', 'BasicApp_FlipTexU', 'BasicApp_FlipTexV', 'OrientedApp_Width', 'OrientedApp_Height',
  'RectApp_CenterOnPos', 'RectApp_ViewingAngleFade', 'RectApp_Scale', 'RectApp_Length', 'RectApp_Width',
  'BeamApp_DisablePlaceHolder', 'BeamApp_DupeFirstParticle', 'BeamApp_LineAppearance', 'ParticleApp_LifeName',
  'ParticleApp_DeathName', 'ParticleApp_UseDynamicRotation', 'ParticleApp_SmoothRotation', 'MeshApp_MeshId',
  'MeshApp_MeshName', 'MeshApp_UseParticleTransform', 'MeshApp_ParticleTransform', 'RadialField_Approach',
  'RadialField_Attenuation', 'RadialField_Magnitude', 'RadialField_Radius', 'CollideField_Height', 'CollideField_Reflectivity',
  'CollideField_Width', 'AirField_Approach', 'AirField_Magnitude', 'GravityField_Gravity', 'TurbulenceField_Approach',
  'TurbulenceField_Magnitude', 'TurbulenceField_Radius', 'SparkApp_MotionBlur'];

// FLCRC32 (verified 19 Oct 2026 against Node_Name and shields_ale.ini effect_crc values): CRC-32 with init / final xor
// 0xFFFFFFFF over alchemy.dll's OWN table (t[1] = 0x09073096, not the standard 0x77073096 - so no standard CRC-32
// matches). The table is read from the install's EXE\alchemy.dll, found by the CRC-table property t[i^j] = t[i]^t[j].
const tables = {};
function alchemyTable(fromFile) {
  let dir = path.dirname(path.resolve(fromFile));
  for (let i = 0; i < 6; i++, dir = path.dirname(dir)) {
    const dll = path.join(dir, 'EXE', 'alchemy.dll');
    if (tables[dll]) return tables[dll];
    if (!fs.existsSync(dll)) continue;
    const d = fs.readFileSync(dll);
    for (let o = 0; o + 1024 <= d.length; o += 4) {
      if (d.readUInt32LE(o) !== 0 || d.readUInt32LE(o + 512) !== 0xEDB88320) continue;
      let ok = true;
      for (let a = 1; a < 16 && ok; a++) for (let c = 1; c < 16; c++) if ((d.readUInt32LE(o + 4 * (a ^ c)) >>> 0) !== ((d.readUInt32LE(o + 4 * a) ^ d.readUInt32LE(o + 4 * c)) >>> 0)) { ok = false; break; }
      if (ok) { const t = new Uint32Array(256); for (let k = 0; k < 256; k++) t[k] = d.readUInt32LE(o + 4 * k); return (tables[dll] = t); }
    }
  }
  throw new Error('alchemy.dll CRC table not found above ' + fromFile);
}
const flcrc = (t, s) => { let c = 0xFFFFFFFF; for (const ch of Buffer.from(s, 'ascii')) c = (t[(c ^ ch) & 0xFF] ^ (c >>> 8)) >>> 0; return (c ^ 0xFFFFFFFF) >>> 0; };

function utfLeaves(b) {
  if (b.toString('ascii', 0, 4) !== 'UTF ') throw new Error('not a UTF file');
  const nodeOff = b.readUInt32LE(8), strOff = b.readUInt32LE(24), dataOff = b.readUInt32LE(36);
  const cstr = (o) => { let e = o; while (b[e]) e++; return b.toString('ascii', o, e); };
  const leaves = {};
  (function walk(rel, prefix) {
    for (;;) {
      const n = nodeOff + rel;
      const name = cstr(strOff + b.readUInt32LE(n + 4)), flags = b.readUInt32LE(n + 8);
      const p = prefix ? prefix + '\\' + name : name;
      if (flags & 0x10) walk(b.readUInt32LE(n + 16), p);
      else { const d = dataOff + b.readUInt32LE(n + 16); leaves[p.replace(/^\\+/, '').toLowerCase()] = b.subarray(d, d + b.readUInt32LE(n + 24)); }
      const peer = b.readUInt32LE(n);
      if (!peer) break;
      rel = peer;
    }
  })(0, '');
  return leaves;
}

function parseAle(file) {
  const t = alchemyTable(file);
  const byId = {};
  for (const n of NAMES) byId[flcrc(t, n)] = n;
  const leaves = utfLeaves(fs.readFileSync(file));
  const leaf = (s) => leaves[Object.keys(leaves).find((k) => k.endsWith(s.toLowerCase()))];
  const nl = leaf('alchemynodelibrary'), el = leaf('aleffectlib');
  if (!nl || !el) throw new Error('no AlchemyNodeLibrary / ALEffectLib in ' + file);

  const u8 = (r) => r.buf.readUInt8(r.p++), u16 = (r) => { const v = r.buf.readUInt16LE(r.p); r.p += 2; return v; };
  const u32 = (r) => { const v = r.buf.readUInt32LE(r.p); r.p += 4; return v; }, i32 = (r) => { const v = r.buf.readInt32LE(r.p); r.p += 4; return v; };
  const f32 = (r) => { const v = r.buf.readFloatLE(r.p); r.p += 4; return v; };
  // count includes the NUL; the bytes are padded to an even count
  const astr = (r) => { const n = u16(r); const s = r.buf.toString('ascii', r.p, r.p + n).replace(/\0+$/, ''); r.p += n + (n & 1); return s; };
  const curve = (r) => { const key = f32(r), def = f32(r); u16(r); const n = u16(r); const k = []; for (let i = 0; i < n; i++) k.push([f32(r), f32(r), f32(r), f32(r)]); return { key, def, k }; };
  const anim = (r, color) => { u8(r); const n = u8(r); const out = []; for (let i = 0; i < n; i++) { const key = f32(r); u8(r); const m = u8(r); const v = []; for (let j = 0; j < m; j++) v.push([f32(r), color ? [f32(r), f32(r), f32(r)] : f32(r)]); out.push({ key, v }); } return out; };
  const animCurve = (r) => { u8(r); const n = u8(r); const out = []; for (let i = 0; i < n; i++) out.push(curve(r)); return out; };

  const R = { buf: nl, p: 0 };
  f32(R);
  const count = u32(R), nodes = {};
  for (let i = 0; i < count; i++) {
    const type = astr(R), p = {};
    for (;;) {
      const tp = u16(R);
      if (tp === 0) break;
      const id = u32(R), base = tp & 0x7FFF;
      let v;
      if (base === 0x001) v = !!(tp & 0x8000);
      else if (base === 0x002) v = i32(R);
      else if (base === 0x003) v = f32(R);
      else if (base === 0x103) v = astr(R);
      else if (base === 0x104) v = [u32(R), u32(R)];
      else if (base === 0x105) { const fl = u32(R); v = fl >>> 24 ? [0, 1, 2, 3, 4, 5, 6, 7, 8].map(() => animCurve(R)) : null; }
      else if (base === 0x200) v = anim(R, false);
      else if (base === 0x201) v = anim(R, true);
      else if (base === 0x202) v = animCurve(R);
      else throw new Error(`unknown property type 0x${tp.toString(16)} in ${type} (${file})`);
      p[byId[id] || '0x' + id.toString(16)] = v;
    }
    if (p.Node_Name) nodes[flcrc(t, p.Node_Name)] = { type, p };
  }

  const E = { buf: el, p: 0 };
  const ver = f32(E), ecount = u32(E), effects = [];
  for (let e = 0; e < ecount; e++) {
    const name = astr(E);
    if (ver > 1.05) for (let k = 0; k < 4; k++) f32(E);
    const ni = u32(E), inst = [];
    for (let i = 0; i < ni; i++) inst.push({ flags: u32(E), name: u32(E), parent: i32(E), id: i32(E) });
    const np = u32(E), pairs = [];
    for (let i = 0; i < np; i++) pairs.push([i32(E), i32(E)]);
    effects.push({
      name, crc: flcrc(t, name),
      nodes: inst.map((it) => ({ id: it.id, parent: it.parent === 32768 ? null : it.parent, attachment: !!it.flags,
        ...(nodes[it.name] || { type: '?', p: {} }), targets: pairs.filter(([s]) => s === it.id).map(([, x]) => x) })),
    });
  }
  return { effects };
}

// ---- CLI
const fmt = (x) => (Math.round(x * 1000) / 1000).toString();
const blendName = { 1: 'ZERO', 2: 'ONE', 3: 'SRCCOLOR', 4: 'INVSRCCOLOR', 5: 'SRCALPHA', 6: 'INVSRCALPHA', 7: 'DESTALPHA', 8: 'INVDESTALPHA', 9: 'DESTCOLOR', 10: 'INVDESTCOLOR' };
const showAnim = (a, col) => a.map((s) => (a.length > 1 ? `@${fmt(s.key)}:` : '') + s.v.map(([k, v]) => `${fmt(k)}=${col ? '(' + v.map(fmt).join(',') + ')' : fmt(v)}`).join(' ')).join(' | ');
const showCurve = (a) => a.map((c) => (c.k.length ? c.k.map((k) => `${fmt(k[0])}s=${fmt(k[1])}`).join(' ') : fmt(c.def))).join(' | ');
function describe(n) {
  const q = n.p, out = [];
  if (q.BasicApp_TexName) out.push(`tex=${q.BasicApp_TexName}`);
  if (q.BasicApp_BlendInfo) out.push(`blend=${blendName[q.BasicApp_BlendInfo[0]]}/${blendName[q.BasicApp_BlendInfo[1]]}`);
  if (q.BasicApp_Color) out.push(`color ${showAnim(q.BasicApp_Color, true)}`);
  if (q.BasicApp_Alpha) out.push(`alpha ${showAnim(q.BasicApp_Alpha)}`);
  if (q.BasicApp_Size) out.push(`size ${showAnim(q.BasicApp_Size)}`);
  if (q.Emitter_InitialParticles !== undefined) out.push(`initial=${q.Emitter_InitialParticles}`);
  for (const k of ['Emitter_Frequency', 'Emitter_InitLifeSpan', 'Emitter_Pressure', 'SphereEmitter_MaxRadius', 'ConeEmitter_MaxSpread'])
    if (q[k]) out.push(`${k.replace(/^\w*Emitter_/, '')} ${showCurve(q[k])}`);
  if (q.ParticleApp_LifeName) out.push(`life-effect=${q.ParticleApp_LifeName}`);
  if (q.ParticleApp_DeathName) out.push(`death-effect=${q.ParticleApp_DeathName}`);
  return out;
}
if (require.main === module) {
  const file = process.argv[2], only = process.argv.slice(3).map((s) => s.toLowerCase());
  if (!file) { console.error('usage: node aledump.js <file.ale> [effectName...]'); process.exit(1); }
  for (const e of parseAle(file).effects) {
    if (only.length && !only.includes(e.name.toLowerCase())) continue;
    console.log(`\n== effect ${e.name} (${e.nodes.length} nodes)`);
    for (const n of e.nodes) {
      if (n.attachment) { console.log(`  #${n.id} attachment`); continue; }
      console.log(`  #${n.id} ${n.type} "${n.p.Node_Name}" life=${fmt(n.p.Node_LifeSpan ?? 0)}` + (n.parent ? ` parent=#${n.parent}` : '') + (n.targets.length ? ` -> ${n.targets.map((x) => '#' + x).join(',')}` : ''));
      for (const x of describe(n)) console.log('      ' + x);
    }
  }
}
module.exports = { parseAle, describe, fmt };
