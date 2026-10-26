// Builds a free-roam test fixture save: a copy of a plain fixture save whose ship block (ship_archetype .. the last
// `wg =` weapon group line: the ship, its equipment and cargo) is taken from another save (a campaign save: FLS1
// encoded or plain), with a new description. The place, story state and reputation stay the fixture's (free roam).
// Usage: node tools\mkfixture.js <fixture.fl> <donor.fl> <out.fl> "<description>"
// Example (SaveC706 = SaveC705 with the M09 ship, bh_elite):
//   node tools\mkfixture.js <SAVE>\SaveC705.fl research\saves\bc46-all-missions\Save4686.fl <SAVE>\SaveC706.fl
//        "CTL free roam - space Manhattan, M09 bh_elite loadout"
const fs = require('fs');
const gene = Buffer.from('Gene');
function decode(buf) { // the same as tools\flsave.js
  if (buf.slice(0, 4).toString('latin1') !== 'FLS1') return buf.toString('latin1');
  const d = buf.slice(4), out = Buffer.alloc(d.length);
  for (let i = 0; i < d.length; i++) out[i] = d[i] ^ (((gene[i % 4] + i) & 0xff) | 0x80);
  return out.toString('latin1');
}
const SHIP_KEYS = /^(equip|cargo|wg|base_equip|base_cargo|hull_status|base_hull_status|collision_group|base_collision_group|ship_archetype)\s*=/;
// [first, last] line of the ship block: from ship_archetype through the last weapon group line, across blank lines
function block(lines) {
  const s = lines.findIndex((l) => /^ship_archetype\s*=/.test(l));
  let e = -1;
  for (let i = s; s >= 0 && i < lines.length; i++) {
    if (/^\s*$/.test(lines[i]) || SHIP_KEYS.test(lines[i])) { if (/^wg\s*=/.test(lines[i])) e = i; }
    else if (e > s) break;
  }
  return [s, e];
}
const [fixture, donor, out, desc] = process.argv.slice(2);
if (!out || !desc) { console.error('usage: node mkfixture.js <fixture.fl> <donor.fl> <out.fl> "<description>"'); process.exit(2); }
const a = decode(fs.readFileSync(fixture)).split(/\r?\n/), b = decode(fs.readFileSync(donor)).split(/\r?\n/);
const [as, ae] = block(a), [bs, be] = block(b);
if (as < 0 || bs < 0 || ae <= as || be <= bs) { console.error(`ship block not found: ${as}-${ae} / ${bs}-${be}`); process.exit(1); }
const hex = [...desc].map((c) => c.charCodeAt(0).toString(16).padStart(4, '0')).join('');
const res = [...a.slice(0, as), ...b.slice(bs, be + 1), ...a.slice(ae + 1)]
  .map((l) => (/^description\s*=/.test(l) ? `description = ${hex}` : l));
fs.writeFileSync(out, res.join('\r\n'), 'latin1');
console.log(`fixture ${out}: lines ${as + 1}-${ae + 1} replaced by the donor's ${bs + 1}-${be + 1}`);
