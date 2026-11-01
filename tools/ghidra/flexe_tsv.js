// core\flexe.h -> a TSV of its symbols for the Ghidra seed script (FLvibedSeed.java): one row per X(...) entry,
// columns name, kind, va, site, off, sig, comment. Usage: node flexe_tsv.js [out.tsv] (default: stdout).
// Exit 1 when the count of parsed entries differs from the count of X( lines (a format the regex does not know).
'use strict';
const fs = require('fs');
const path = require('path');

const src = fs.readFileSync(path.join(__dirname, '..', '..', 'core', 'flexe.h'), 'utf8');
const body = src.slice(src.indexOf('#define FLV_EXE_SYMBOLS'), src.indexOf('namespace exe'));
const re = /X\(\s*(\w+)\s*,\s*(Code|Data)\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*(0x[0-9A-Fa-f]+|0)\s*,\s*(\d+)\s*,\s*\\?\s*((?:"[^"]*"\s*\\?\s*)+)\)\s*\\?\s*(?:\/\*([\s\S]*?)\*\/)?/g;
const rows = [];
for (let m; (m = re.exec(body));) {
    const sig = m[6].match(/"([^"]*)"/g).map((s) => s.slice(1, -1)).join(' ').replace(/\s+/g, ' ').trim();
    const comment = (m[7] || '').replace(/\\\s*\n/g, ' ').replace(/\s+/g, ' ').trim();
    rows.push([m[1], m[2], m[3], m[4], m[5], sig, comment.replace(/\t/g, ' ')]);
}
const expected = (body.match(/^\s*X\(/gm) || []).length;
const out = rows.map((r) => r.join('\t')).join('\n') + '\n';
if (process.argv[2]) fs.writeFileSync(process.argv[2], out);
else process.stdout.write(out);
console.error(`flexe_tsv: ${rows.length} symbols (X( lines: ${expected})`);
if (rows.length !== expected) process.exit(1);
