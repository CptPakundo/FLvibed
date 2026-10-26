// Commodity market table from the install's own data (22 Oct 2026, the speedrun plan's money layer): every
// (base, commodity) of DATA\EQUIPMENT\market_commodities.ini with its local price (goods.ini price x the base's
// multiplier), whether the base sells it, and the net-worth effect of buying it there: Server.dll GetAssetValue values
// cargo at the goods.ini price (docs\FREELANCER_RE.md "Net worth = asset value + cash"), so buying below that price raises
// net worth at once by (price - local) per unit, buying above lowers it.
// Usage: node tools\flmarket.js [install dir] [--sells] [--good <nick>] [--base <prefix>] [--routes <hold>]
//   --routes <hold>: best one-hop trades (buy where sold, sell where bought) by profit per full hold.
// Output: TSV on stdout. The universe's base names are not resolved (nicknames; prefix = system).
const fs = require('fs');
const path = require('path');
const args = process.argv.slice(2);
const opt = (k) => { const i = args.indexOf(k); return i >= 0 ? args[i + 1] : null; };
const dir = args[0] && !args[0].startsWith('--') ? args[0]
    : path.join(__dirname, '..', 'Install', 'Freelancer HD Edition');
const eq = path.join(dir, 'DATA', 'EQUIPMENT');
const read = (f) => fs.readFileSync(path.join(eq, f), 'latin1');

const price = {}; const volume = {};
{
    let cur = null;
    for (const l of read('goods.ini').split(/\r?\n/)) {
        if (/^\s*\[/.test(l)) cur = null;
        let m;
        if ((m = l.match(/^\s*nickname\s*=\s*(\S+)/i))) cur = m[1].toLowerCase();
        else if (cur && (m = l.match(/^\s*price\s*=\s*([\d.]+)/i))) price[cur] = +m[1];
    }
}
// cargo volume (select_equip.ini [Commodity] volume) for per-hold numbers
try {
    let cur = null;
    for (const l of read('select_equip.ini').split(/\r?\n/)) {
        if (/^\s*\[/.test(l)) cur = null;
        let m;
        if ((m = l.match(/^\s*nickname\s*=\s*(\S+)/i))) cur = m[1].toLowerCase();
        else if (cur && (m = l.match(/^\s*volume\s*=\s*([\d.]+)/i))) volume[cur] = +m[1];
    }
} catch (e) { }
const rows = [];
{
    let base = null;
    for (const l of read('market_commodities.ini').split(/\r?\n/)) {
        let m;
        if ((m = l.match(/^\s*base\s*=\s*(\S+)/i))) base = m[1].toLowerCase();
        else if (base && (m = l.match(/^\s*MarketGood\s*=\s*([^,]+),\s*[^,]+,\s*[^,]+,\s*[^,]+,\s*[^,]+,\s*(\d+),\s*([\d.]+)/i))) {
            const g = m[1].trim().toLowerCase(); const p = price[g] || 0; const mult = +m[3];
            rows.push({ base, good: g, price: p, mult, local: Math.round(p * mult), sells: m[2] === '0',
                        gain: Math.round(p - p * mult), vol: volume[g] || 1 });
        }
    }
}
let sel = rows;
if (args.includes('--sells')) sel = sel.filter(r => r.sells);
if (opt('--good')) sel = sel.filter(r => r.good.includes(opt('--good').toLowerCase()));
if (opt('--base')) sel = sel.filter(r => r.base.startsWith(opt('--base').toLowerCase()));
if (opt('--routes')) {
    const hold = +opt('--routes');
    const out = [];
    for (const b of sel.filter(r => r.sells)) for (const s of rows.filter(r => r.good === b.good && r.base !== b.base)) {
        const units = Math.floor(hold / b.vol);
        out.push({ good: b.good, from: b.base, buy: b.local, to: s.base, sell: s.local, unit: s.local - b.local,
                   hold: (s.local - b.local) * units, worthAtBuy: b.gain * units });
    }
    out.sort((a, c) => c.hold - a.hold);
    console.log('good\tfrom\tbuy\tto\tsell\tprofit/unit\tprofit/hold\tworth gain at buy (hold)');
    for (const r of out.slice(0, +(opt('--top') || 60))) console.log([r.good, r.from, r.buy, r.to, r.sell, r.unit, r.hold, r.worthAtBuy].join('\t'));
} else {
    console.log('base\tgood\tprice\tmult\tlocal\tsells\tworth gain/unit if bought');
    for (const r of sel) console.log([r.base, r.good, r.price, r.mult, r.local, r.sells ? 'SELLS' : 'buys', r.gain].join('\t'));
}
