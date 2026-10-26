// Pools the per-video totals of vidstudy.js: seconds-weighted shares across videos (M13 i3vAiGmtO84 excluded:
// uniquely scripted). Usage: node vidpool.js <dir with <videoid>.study.jsonl>
// Producing the inputs, per video: node vidhud.js <video.mp4> --fps 1 > <id>.hud1.csv
//   node vidstudy.js <video.mp4> <id>.hud1.csv --min 8 > <id>.study.jsonl   (run several videos in parallel)
const fs = require('fs'), path = require('path');
const dir = process.argv[2] || process.cwd();
const rows = [];
for (const f of fs.readdirSync(dir).filter((f) => f.endsWith('.study.jsonl') && !f.startsWith('i3vAiGmtO84') && !f.startsWith('CombatTest'))) {
    const lines = fs.readFileSync(path.join(dir, f), 'utf8').trim().split('\n').filter(Boolean).map((l) => JSON.parse(l));
    const tot = lines.find((l) => l.total);
    if (tot) rows.push(tot);
}
const w = (sel, key) => {
    let s = 0, n = 0;
    for (const r of rows) { const p = sel(r); if (p && p[key] !== null && p[key] !== undefined && p.secs) { s += p[key] * p.secs; n += p.secs; } }
    return n ? Math.round(s / n) : null;
};
console.log('video                fights secs  fwd rev  ab cruise | HOT secs onScr ctr  ab rev fwd | COOL secs onScr ctr  ab rev fwd | eps hotDur reeng ext%');
for (const r of rows) {
    const h = r.hot || {}, c = r.cool || {};
    console.log(`${r.total.padEnd(20)} ${String(r.fights).padStart(6)} ${String(r.secs).padStart(4)} ${String(r.fwd).padStart(4)} ${String(r.rev).padStart(3)} ${String(r.ab).padStart(3)} ${String(r.cruise).padStart(6)} | ` +
        `${String(h.secs).padStart(8)} ${String(h.onScreen).padStart(5)} ${String(h.centred).padStart(3)} ${String(h.ab).padStart(3)} ${String(h.rev).padStart(3)} ${String(h.fwd).padStart(3)} | ` +
        `${String(c.secs).padStart(9)} ${String(c.onScreen).padStart(5)} ${String(c.centred).padStart(3)} ${String(c.ab).padStart(3)} ${String(c.rev).padStart(3)} ${String(c.fwd).padStart(3)} | ` +
        `${String(r.hotEpisodes).padStart(3)} ${String(r.hotDur).padStart(6)} ${String(r.reengage).padStart(5)} ${String(r.extendedPct).padStart(4)}`);
}
const all = (k) => w((r) => ({ ...r, secs: r.secs }), k);
const ph = (p, k) => w((r) => r[p], k);
const eps = rows.reduce((a, r) => a + (r.hotEpisodes || 0), 0);
console.log(`\nPOOLED (${rows.length} videos, ${rows.reduce((a, r) => a + r.secs, 0)} s of fights, ${eps} hot episodes):`);
console.log(`  overall: fwd ${all('fwd')} rev ${all('rev')} ab ${all('ab')} cruise ${all('cruise')} low ${all('low')} | on screen ${all('onScreen')} centred ${all('centred')}`);
for (const p of ['hot', 'cool'])
    console.log(`  ${p}: secs ${rows.reduce((a, r) => a + ((r[p] || {}).secs || 0), 0)} on screen ${ph(p, 'onScreen')} centred ${ph(p, 'centred')} | ab ${ph(p, 'ab')} rev ${ph(p, 'rev')} fwd ${ph(p, 'fwd')} low ${ph(p, 'low')}`);
const med = (a) => { a = a.filter((x) => x !== null && x !== undefined).sort((x, y) => x - y); return a.length ? a[Math.floor(a.length / 2)] : null; };
console.log(`  hot episodes: median length ${med(rows.map((r) => r.hotDur))} s, re-engage ${med(rows.map((r) => r.reengage))} s after the hits stop, extended (target off screen) ${med(rows.map((r) => r.extendedPct))} %  (medians of per-video medians)`);
