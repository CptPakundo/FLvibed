// docs\FREELANCER_RE.md -> a TSV of its Freelancer.exe addresses for the Ghidra project (FLvibedNotes.java): one row per
// address and doc line - va, line number, a function name when a table row gives one (`| 0x5B2890 | \`void __cdecl
// FrameUpdate(double dt)\``), the line's text. Addresses in the exe's image only (0x400000-0x6FFFFF, 6 hex digits).
// Usage: node re_notes.js [out.tsv]   (stdout without an argument)
'use strict';
const fs = require('fs');
const path = require('path');

const lines = fs.readFileSync(path.join(__dirname, '..', '..', 'docs', 'FREELANCER_RE.md'), 'utf8').split(/\r?\n/);
const rows = [];
const seen = new Set();
lines.forEach((line, i) => {
    const text = line.replace(/\s+/g, ' ').replace(/\t/g, ' ').trim();
    // a table row naming a function: | 0xVA | `ret conv Name(...)`
    const row = /^\|\s*0x([0-9A-Fa-f]{6})\s*\|\s*`[^`(]*?\b([A-Za-z_]\w*)\s*\(/.exec(line);
    for (const m of line.matchAll(/(?<![0-9A-Za-z_+])0x([4-6][0-9A-Fa-f]{5})(?![0-9A-Fa-f])/g)) {
        const va = parseInt(m[1], 16);
        const key = va + ':' + i;
        if (seen.has(key)) continue;
        seen.add(key);
        const name = row && parseInt(row[1], 16) === va ? row[2] : '';
        rows.push(['0x' + m[1].toUpperCase(), i + 1, name, text.slice(0, 300)].join('\t'));
    }
});
const out = rows.join('\n') + '\n';
if (process.argv[2]) fs.writeFileSync(process.argv[2], out);
else process.stdout.write(out);
console.error(`re_notes: ${rows.length} mentions of ${new Set(rows.map((r) => r.split('\t')[0])).size} addresses, ` +
    `${rows.filter((r) => r.split('\t')[2]).length} named`);
