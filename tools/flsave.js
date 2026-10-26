// Freelancer saves: decodes FLS1 files (byte i XOR ((("Gene"[i % 4] + i) & 0xFF) | 0x80), after the 4-byte magic)
// and prints a catalogue line per save: file, MissionNum, Mission, base (or space + system), rank, money, ship
// archetype id (tools\flhash.ps1 Get-FLHash <nickname> resolves it), description. Read only; plain saves pass through.
// Usage: node tools\flsave.js <dir> [<decoded-out-dir>]   (decoded copies as <file>.ini)
const fs = require('fs');
const path = require('path');
const [dir, outDir] = process.argv.slice(2);
const gene = Buffer.from('Gene');
function decode(buf) {
  if (buf.slice(0, 4).toString('latin1') !== 'FLS1') return buf.toString('latin1');
  const d = buf.slice(4), out = Buffer.alloc(d.length);
  for (let i = 0; i < d.length; i++) out[i] = d[i] ^ (((gene[i % 4] + i) & 0xff) | 0x80);
  return out.toString('latin1');
}
function val(text, section, key) {
  const re = new RegExp('\\[' + section + '\\][^\\[]*?^\\s*' + key + '\\s*=\\s*(.*)$', 'mi');
  const m = text.match(re);
  return m ? m[1].trim() : '';
}
if (outDir) fs.mkdirSync(outDir, { recursive: true });
const rows = [];
for (const f of fs.readdirSync(dir).filter((n) => n.toLowerCase().endsWith('.fl'))) {
  const t = decode(fs.readFileSync(path.join(dir, f)));
  if (outDir) fs.writeFileSync(path.join(outDir, f + '.ini'), t, 'latin1');
  // description is UTF-16 hex in [Player] description = <hex>
  let desc = val(t, 'Player', 'description');
  if (/^[0-9a-f]+$/i.test(desc) && desc.length % 4 === 0) {
    let s = '';
    for (let i = 0; i < desc.length; i += 4) s += String.fromCharCode(parseInt(desc.substr(i, 4), 16));
    desc = s;
  }
  rows.push({
    file: f,
    time: (fs.statSync(path.join(dir, f)).mtime.toISOString()),
    desc,
    mission: val(t, 'StoryInfo', 'Mission'),
    missionNum: val(t, 'StoryInfo', 'MissionNum'),
    base: val(t, 'Player', 'base'),
    system: val(t, 'Player', 'system'),
    pos: val(t, 'Player', 'pos'),
    rank: val(t, 'Player', 'rank'),
    money: val(t, 'Player', 'money'),
    ship: val(t, 'Player', 'ship_archetype'),
  });
}
for (const r of rows) console.log([r.file, r.missionNum, r.mission, r.base || ('space ' + r.system), r.rank, r.money, r.ship, r.desc].join('\t'));
