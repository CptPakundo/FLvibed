// Reads Freelancer HUD state from a gameplay video, frame by frame, without playing it (combat study: how skilled
// players fight). ffmpeg decodes to raw RGB; this script measures, per frame:
//   bracket: the selected target's yellow corner bracket (centre offset from the screen centre in half-screens,
//            -1..1, x right / y up; size as a fraction of the screen height) or, when the target is off screen,
//            the yellow edge arrow (offscreen = 1, direction in cx / cy);
//   speed:   the HUD speed readout box (bottom right of the power bar) as a digit string, when a digit template set
//            is given (-Digits), else only its ink fingerprint for clustering.
// Usage: node vidhud.js <video> [--fps 10] [--from s] [--to s] [--width 960] > out.csv
//   red:     hostile-red ink in the playfield and in the contact list (combat detection: hostiles near).
//   bars:    power / shield / hull fill (0..1) from the bottom bar's cell rows (incoming damage over time).
// Requires ffmpeg on PATH (or FFMPEG env). Output CSV: t,found,offscreen,cx,cy,w,h,ink,redField,redList,redListTop,
// power,shield,hull
'use strict';
const { spawn } = require('child_process');

const args = process.argv.slice(2);
const opt = (name, def) => {
    const i = args.indexOf('--' + name);
    return i >= 0 ? args[i + 1] : def;
};
const video = args[0];
if (!video) {
    console.error('usage: node vidhud.js <video> [--fps 10] [--from s] [--to s] [--width 960]');
    process.exit(2);
}
const fps = Number(opt('fps', 10));
const W = Number(opt('width', 960));
const H = Math.round(W * 9 / 16);
const from = opt('from'), to = opt('to');

const ffArgs = ['-v', 'error'];
if (from) ffArgs.push('-ss', from);
if (to) ffArgs.push('-to', to);
ffArgs.push('-i', video, '-vf', `fps=${fps},scale=${W}:${H}`, '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-');
const ff = spawn(process.env.FFMPEG || 'ffmpeg', ffArgs, { stdio: ['ignore', 'pipe', 'inherit'] });

// HUD panels (fractions of the screen) where yellow is HUD art, not the bracket: top bar, bottom bar, the corner
// panels (contact list, weapons).
const masked = (x, y) => {
    const fx = x / W, fy = y / H;
    if (fy < 0.10) return true;                       // top bar (maneuver icons, NN buttons)
    if (fy > 0.93) return true;                       // bottom bar (power, speed)
    if (fy > 0.68 && (fx < 0.21 || fx > 0.79)) return true; // contact list / equipment panels
    return false;
};
const yellow = (r, g, b) => r > 150 && g > 140 && b < 120 && Math.abs(r - g) < 45 && r - b > 90;

const frameBytes = W * H * 3;
let buf = Buffer.alloc(0), n = 0;
console.log('t,found,offscreen,cx,cy,w,h,ink,redField,redList,redListTop,power,shield,hull');
ff.stdout.on('data', (chunk) => {
    buf = Buffer.concat([buf, chunk]);
    while (buf.length >= frameBytes) {
        const f = buf.subarray(0, frameBytes);
        buf = buf.subarray(frameBytes);
        analyse(f, n++);
    }
});
ff.on('close', () => process.exit(0));

// Hostile red (contact list names / distances, edge arrows, the hostile reticle and bracket).
const red = (r, g, b) => r > 160 && g < 80 && b < 80;

// Red ink: in the playfield (edge arrows / brackets of hostiles) and in the contact list (x < 20 %, y 77-99 %:
// hostile entries; the list is sorted by distance, so its top rows say how close they are).
function redInk(f) {
    let field = 0, list = 0, listTop = 0;
    for (let y = 0; y < H; y += 2) {
        for (let x = 0; x < W; x += 2) {
            const o = (y * W + x) * 3;
            if (!red(f[o], f[o + 1], f[o + 2])) continue;
            const fx = x / W, fy = y / H;
            if (fx < 0.20 && fy > 0.77) {
                list++;
                if (fy < 0.86) listTop++;
            } else if (!masked(x, y)) field++;
        }
    }
    return `${field},${list},${listTop}`;
}

// The bottom bar's three cell rows (HDE HUD; 1080p: x 848..1086, rows y 1038 power / 1052 shield / 1066 hull):
// fill = the rightmost lit cell's end as a fraction of the row (bars empty from the right).
const BAR = { x0: 848 / 1920, x1: 1086 / 1920, rows: { power: 1038 / 1080, shield: 1052 / 1080, hull: 1066 / 1080 } };
const lit = {
    power: (r, g, b) => r > 115 && g > 115 && b < 115 && r - b > 40,
    shield: (r, g, b) => b > 140 && b - r > 70,
    hull: (r, g, b) => r > 95 && r - g > 50 && r - b > 35,
};
function bars(f) {
    const x0 = Math.round(BAR.x0 * W), x1 = Math.round(BAR.x1 * W);
    const out = [];
    for (const k of ['power', 'shield', 'hull']) {
        let right = -1, hits = 0;
        for (const dy of [-1, 0, 1]) {
            const y = Math.round(BAR.rows[k] * H) + dy;
            for (let x = x0; x <= x1; x++) {
                const o = (y * W + x) * 3;
                if (lit[k](f[o], f[o + 1], f[o + 2])) { hits++; if (x > right) right = x; }
            }
        }
        out.push(hits < 3 ? 0 : Math.min(1, (right - x0 + 1) / (x1 - x0 + 1)).toFixed(3));
    }
    return out.join(',');
}

function analyse(f, i) {
    const t = (Number(from || 0) + i / fps).toFixed(2);
    const reds = redInk(f) + ',' + bars(f);
    // Yellow pixels outside the HUD panels; a row/column histogram finds the bracket's box (four L corners).
    let count = 0, minX = W, maxX = -1, minY = H, maxY = -1;
    const xs = [], ys = [];
    for (let y = 0; y < H; y += 1) {
        for (let x = 0; x < W; x += 1) {
            const o = (y * W + x) * 3;
            if (!yellow(f[o], f[o + 1], f[o + 2]) || masked(x, y)) continue;
            count++;
            xs.push(x);
            ys.push(y);
        }
    }
    if (count < 12) {
        console.log(`${t},0,0,,,,,${count},${reds}`);
        return;
    }
    // Robust box: 2nd..98th percentile (ignores stray yellow: muzzle flashes, suns)
    xs.sort((a, b) => a - b);
    ys.sort((a, b) => a - b);
    const q = (a, p) => a[Math.min(a.length - 1, Math.floor(a.length * p))];
    minX = q(xs, 0.02); maxX = q(xs, 0.98); minY = q(ys, 0.02); maxY = q(ys, 0.98);
    const w = (maxX - minX) / H, h = (maxY - minY) / H;
    const cx = ((minX + maxX) / 2 - W / 2) / (W / 2), cy = -((minY + maxY) / 2 - H / 2) / (H / 2);
    // The off-screen arrow is a small blob at the screen edge; the bracket is a box with an empty middle.
    const edge = Math.abs(cx) > 0.9 || Math.abs(cy) > 0.85;
    const offscreen = edge && w < 0.06 && h < 0.06 ? 1 : 0;
    console.log(`${t},1,${offscreen},${cx.toFixed(3)},${cy.toFixed(3)},${w.toFixed(3)},${h.toFixed(3)},${count},${reds}`);
}
