// Reads the HUD speed readout (the box right of the power bar) from Freelancer gameplay video, frame by frame,
// without playing it (combat study: throttle, reverse, engine kill, afterburner over time). HDE HUD, 16:9 frames.
// The digits are cut into glyphs (columns of ink) and matched by correlation against grayscale digit templates
// learnt from readings done once by eye (speedglyphs.json next to this script):
//   node vidspeed.js train <video> --fps f --from s --to s --labels "79,-15,..."  (one reading per sampled frame,
//        in order; '?' skips a frame). Grids to read: tools\vidsheet.ps1 style tiles of the box, see the combat study.
//   node vidspeed.js read <video> [--fps 10] [--from s] [--to s]   -> CSV t,speed,score (speed empty: unreadable)
// Box geometry as fractions of the frame (HDE HUD at 1920x1080: x 1117-1195, y 1044-1068). Windows OCR cannot
// read this thin, compressed font (tested 17 Oct 2026).
'use strict';
const { spawn } = require('child_process');
const fs = require('fs');
const path = require('path');

// Other HUD numbers in the same font (19 Oct 2026): --box x,y,w,h at 1920x1080 (e.g. the nanobot counter
// 1640,764,40,28 / the battery counter 1724,764,40,28 above the weapon list) and --db <templates.json> (default the
// speed set).
const W = 1920, H = 1080;
const [cmd, video, ...rest] = process.argv.slice(2);
const opt = (name, def) => {
    const i = rest.indexOf('--' + name);
    return i >= 0 ? rest[i + 1] : def;
};
const [cx0, cy0, CW, CH] = String(opt('box', '1117,1044,78,24')).split(',').map(Number);
const GW = 10, GH = 16;
const DB = opt('db') ? path.resolve(opt('db')) : path.join(__dirname, 'speedglyphs.json');
if (!['train', 'read'].includes(cmd) || !video) {
    console.error('usage: node vidspeed.js train|read <video> [--fps n] [--from s] [--to s] [--labels a,b,...]');
    process.exit(2);
}
const fps = Number(opt('fps', 10));
const from = opt('from'), to = opt('to');
const ffArgs = ['-v', 'error'];
if (from) ffArgs.push('-ss', from);
if (to) ffArgs.push('-to', to);
ffArgs.push('-i', video, '-vf', `fps=${fps},scale=${W}:${H},crop=${CW}:${CH}:${cx0}:${cy0}`, '-f', 'rawvideo',
    '-pix_fmt', 'rgb24', '-');

// Glyphs: background-subtracted grayscale; ink columns above a threshold relative to the brightest pixel.
function glyphs(f) {
    const px = (x, y) => f[y * CW + x];
    let max = 0, sum = 0;
    for (const v of f) { max = Math.max(max, v); sum += v; }
    const bg = sum / f.length;
    if (max - bg < 50) return null;
    // Low: compression leaves the 1-px strokes (the 7's bar, the minus) at a quarter of the brightest ink.
    const thr = bg + (max - bg) * 0.22;
    // Ignore the box's outline: 4 px margins left / right, rows 1..CH-2.
    const cols = [];
    for (let x = 0; x < CW; x++) {
        let c = 0;
        if (x >= 4 && x < CW - 4) for (let y = 1; y < CH - 1; y++) if (px(x, y) >= thr) c++;
        cols.push(c);
    }
    const out = [];
    for (let x = 0; x < CW;) {
        if (!cols[x]) { x++; continue; }
        let x1 = x;
        while (x1 < CW && cols[x1]) x1++;
        // Touching digits ("77", "11"): a run wider than a digit (~7 px at 1080p) ends at its first gap-like
        // column (least ink) between 5 and 9 px in.
        if (x1 - x >= 11) {
            let cut = x + 5;
            for (let k = x + 5; k <= Math.min(x + 9, x1 - 3); k++) if (cols[k] < cols[cut]) cut = k;
            x1 = cut;
        }
        let y0 = CH, y1 = -1, ink = 0;
        for (let y = 1; y < CH - 1; y++) for (let xx = x; xx < x1; xx++) if (px(xx, y) >= thr) { y0 = Math.min(y0, y); y1 = Math.max(y1, y); ink++; }
        const w = x1 - x, h = y1 - y0 + 1;
        if (ink >= 3) {
            if (h <= 5 && w >= 3) out.push({ minus: true, x, x1, ym: (y0 + y1) / 2 });
            else if (h >= 9) {
                // Fixed cell: the glyph's columns, full digit height (rows of the tallest glyph found later).
                out.push({ x, x1, y0, y1, w, h });
            }
        }
        x = x1;
    }
    if (!out.length) return null;
    // Digits share one baseline / height: use the tallest glyph's rows for all.
    const ds = out.filter((g) => !g.minus);
    if (!ds.length) return null;
    const Y0 = Math.min(...ds.map((g) => g.y0)), Y1 = Math.max(...ds.map((g) => g.y1));
    // A real minus sits right before the first digit, around mid height (light streaks elsewhere are not one).
    const first = ds[0];
    for (let k = out.length - 1; k >= 0; k--) {
        const g = out[k];
        if (!g.minus) continue;
        const mid = (Y0 + Y1) / 2, hgt = Y1 - Y0 + 1;
        if (g.x1 > first.x || first.x - g.x1 > 5 || Math.abs(g.ym - mid) > hgt * 0.25) out.splice(k, 1);
    }
    for (const g of ds) {
        // Narrow glyphs ("1") stay narrow: sample a cell 0.62 x height wide, centred on the ink.
        const cellW = Math.max(g.w, Math.round((Y1 - Y0 + 1) * 0.62));
        const cx = (g.x + g.x1) / 2;
        const v = new Array(GW * GH);
        let m = 0;
        for (let gy = 0; gy < GH; gy++) for (let gx = 0; gx < GW; gx++) {
            const sx = Math.round(cx - cellW / 2 + (gx + 0.5) * cellW / GW - 0.5);
            const sy = Math.round(Y0 + (gy + 0.5) * (Y1 - Y0 + 1) / GH - 0.5);
            const val = sx >= 0 && sx < CW ? Math.max(0, px(sx, sy) - bg) : 0;
            v[gy * GW + gx] = val;
            m += val;
        }
        // Normalise (zero mean, unit length) for correlation.
        m /= v.length;
        let n = 0;
        for (let i = 0; i < v.length; i++) { v[i] -= m; n += v[i] * v[i]; }
        n = Math.sqrt(n) || 1;
        for (let i = 0; i < v.length; i++) v[i] /= n;
        g.v = v;
    }
    return out;
}

const corr = (a, b) => {
    let s = 0;
    for (let i = 0; i < a.length; i++) s += a[i] * b[i];
    return s;
};

const labels = cmd === 'train' ? String(opt('labels', '')).split(',') : null;
const db = fs.existsSync(DB) ? JSON.parse(fs.readFileSync(DB, 'utf8')) : { sums: {}, counts: {} };
let templates = null;
if (cmd === 'read') {
    templates = Object.keys(db.sums).map((c) => {
        const v = db.sums[c].map((x) => x / db.counts[c]);
        let n = Math.sqrt(v.reduce((a, x) => a + x * x, 0)) || 1;
        return { c, v: v.map((x) => x / n) };
    });
    console.log('t,speed,score');
}

let buf = Buffer.alloc(0), n = 0, used = 0, mismatched = 0;
const ff = spawn(process.env.FFMPEG || 'ffmpeg', ffArgs, { stdio: ['ignore', 'pipe', 'inherit'] });
ff.stdout.on('data', (chunk) => {
    buf = Buffer.concat([buf, chunk]);
    const fb = CW * CH * 3;
    while (buf.length >= fb) {
        // Whiteness = min(R, G, B): the digits are white, the box behind them can be tinted (teal in M13).
        const rgb = buf.subarray(0, fb);
        const f = new Uint8Array(CW * CH);
        for (let p = 0; p < f.length; p++) f[p] = Math.min(rgb[p * 3], rgb[p * 3 + 1], rgb[p * 3 + 2]);
        buf = buf.subarray(fb);
        const i = n++;
        const t = (Number(from || 0) + i / fps).toFixed(2);
        const gs = glyphs(f);
        if (cmd === 'train') {
            const lab = labels[i];
            if (!lab || lab === '?' || !gs) continue;
            const digits = lab.replace('-', '').split('');
            const ds = gs.filter((g) => !g.minus);
            if (ds.length !== digits.length || lab.startsWith('-') !== gs.some((g) => g.minus)) { mismatched++; continue; }
            ds.forEach((g, k) => {
                const c = digits[k];
                db.sums[c] = db.sums[c] ? db.sums[c].map((x, j) => x + g.v[j]) : g.v.slice();
                db.counts[c] = (db.counts[c] || 0) + 1;
            });
            used++;
        } else {
            if (!gs) { console.log(`${t},,0`); continue; }
            let s = '', score = 1;
            for (const g of gs) {
                if (g.minus) { s += '-'; continue; }
                let best = null, bc = -1;
                for (const tp of templates) {
                    const c = corr(g.v, tp.v);
                    if (c > bc) { bc = c; best = tp.c; }
                }
                s += best;
                score = Math.min(score, bc);
            }
            const ok = /^-?\d{1,4}$/.test(s) && score > 0.6;
            console.log(`${t},${ok ? Number(s) : ''},${score.toFixed(2)}`);
        }
    }
});
ff.on('close', () => {
    if (cmd !== 'train') return;
    fs.writeFileSync(DB, JSON.stringify(db));
    console.log(`trained on ${used} frames (${mismatched} segmentation mismatches); templates: ` +
        Object.keys(db.counts).sort().map((c) => `${c}:${db.counts[c]}`).join(' '));
});
