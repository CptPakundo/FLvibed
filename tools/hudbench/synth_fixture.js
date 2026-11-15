/* The synthetic display list (tools\hudbench\FORMAT.md v0, provisional until B2): a made-up list that exercises every
   primitive, pass, layer, texture op, blend, the clip, the marks, runs, hot-spots, items and every named hole, with a
   made-up atlas (block shapes, not a font: no game text, art or metrics), and the pixels each drawn element must
   produce ("probes", computed from the kit's blend rule), so a replay can be checked without a game or a real list.
   Its output is public by construction (privacy "public").

   node tools\hudbench\synth_fixture.js [--out <folder>] [--sizes 1280x800,1920x1080,3840x2160]
     writes synthetic_<W>x<H>.fldl.json and .probes.json, synthetic_atlas.png / .json, and synthetic.js (all of it as
     one script for the headless test page) into <folder>, default build\bench\synthetic\ (git-ignored).
   As a module: build(W, H) -> {list, probes}; atlas() -> {manifest, png}. */
'use strict';
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const SIZES = [[1280, 800], [1920, 1080], [3840, 2160]];
const ATLAS_W = 64, ATLAS_H = 32;
// the atlas' named cells (x, y, w, h): two block "glyphs" and one icon, none of them a real character's shape
const CELLS = {
    'glyph:synth:U+0041': [0, 0, 16, 16],   // a full grey block (rgb 128, alpha 255): proves modulate multiplies rgb
    'glyph:synth:U+0042': [16, 0, 16, 16],  // white, opaque left half, clear right half: proves the UV orientation
    'icon:synth.ring': [32, 0, 32, 32],     // red texels, an opaque disc of radius 12: proves the icon op ignores rgb
    'glyph:synth:U+0043': [16, 16, 16, 16], // white, alpha rising 17 a column: proves linear filtering and straight alpha
    'glyph:synth:U+0044': [0, 16, 16, 16],  // white, opaque top half, clear bottom half: proves v is not flipped
};
const RAMP = (col) => (col - 16) * 17; // the C cell's alpha by atlas column (16..31)

function atlasPixels() {
    const px = new Uint8Array(ATLAS_W * ATLAS_H * 4);
    const set = (x, y, r, g, b, a) => px.set([r, g, b, a], (y * ATLAS_W + x) * 4);
    for (let y = 0; y < 16; y++) for (let x = 0; x < 16; x++) set(x, y, 128, 128, 128, 255);
    for (let y = 0; y < 16; y++) for (let x = 16; x < 32; x++) set(x, y, 255, 255, 255, x < 24 ? 255 : 0);
    for (let y = 16; y < 32; y++) for (let x = 16; x < 32; x++) set(x, y, 255, 255, 255, RAMP(x));
    for (let y = 16; y < 32; y++) for (let x = 0; x < 16; x++) set(x, y, 255, 255, 255, y < 24 ? 255 : 0);
    for (let y = 0; y < 32; y++) for (let x = 32; x < 64; x++) {
        const dx = x + 0.5 - 48, dy = y + 0.5 - 16;
        set(x, y, 255, 0, 0, dx * dx + dy * dy <= 144 ? 255 : 0);
    }
    return px;
}

let CRC_TABLE = null;
function crc32(buf) {
    if (zlib.crc32) return zlib.crc32(buf) >>> 0;
    if (!CRC_TABLE) {
        CRC_TABLE = new Uint32Array(256);
        for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; CRC_TABLE[n] = c >>> 0; }
    }
    let c = 0xFFFFFFFF;
    for (const b of buf) c = CRC_TABLE[(c ^ b) & 255] ^ (c >>> 8);
    return (c ^ 0xFFFFFFFF) >>> 0;
}
function png(w, h, rgba) {
    const chunk = (type, body) => {
        const len = Buffer.alloc(4); len.writeUInt32BE(body.length);
        const tb = Buffer.concat([Buffer.from(type, 'ascii'), body]);
        const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(tb));
        return Buffer.concat([len, tb, crc]);
    };
    const ihdr = Buffer.alloc(13);
    ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4);
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    const raw = Buffer.alloc((w * 4 + 1) * h);
    for (let y = 0; y < h; y++) Buffer.from(rgba.buffer, rgba.byteOffset + y * w * 4, w * 4).copy(raw, y * (w * 4 + 1) + 1);
    return Buffer.concat([Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]), chunk('IHDR', ihdr),
        chunk('IDAT', zlib.deflateSync(raw)), chunk('IEND', Buffer.alloc(0))]);
}
function atlas() {
    const manifest = { schema: 'flatlas/0', privacy: 'public', image: 'synthetic_atlas.png', size: [ATLAS_W, ATLAS_H], entries: CELLS };
    return { manifest, pixels: atlasPixels(), png: png(ATLAS_W, ATLAS_H, atlasPixels()) };
}

/* The list at W x H. Positions in layout units of H / 100 px, so every size draws the same picture. */
function build(W, H) {
    const s = H / 100;
    const LAYERS = ['shapes', 'iconBig', 'iconSmall', 'textSmall', 'textBig', 'caption'];
    const OPS = { shapes: 'modulate', iconBig: 'icon', iconSmall: 'icon', textSmall: 'modulate', textBig: 'modulate', caption: 'modulate' };
    const textures = ['white', 'glyph:synth:U+0041', 'glyph:synth:U+0042', 'icon:synth.ring', 'glyph:synth:U+005A', 'glyph:synth:U+0043', 'glyph:synth:U+0044'];
    const T = Object.fromEntries(textures.map((t, i) => [t, i]));
    const mk = () => LAYERS.map((name) => ({ name, op: OPS[name], v: [], spans: [] }));
    const passes = { under: { writeAlpha: false, layers: mk() }, over: { writeAlpha: true, layers: mk() } };
    const runs = [], probes = [];
    const L = (pass, name) => passes[pass].layers[LAYERS.indexOf(name)];
    const count = (l) => l.v.length / 5;
    // one span per texture run; the kit's -0.5 (hud2_kit.cpp Push) applied as the kit does
    function push(l, tex, verts) {
        const first = count(l);
        for (const [x, y, c, u, v] of verts) l.v.push(x - 0.5, y - 0.5, c >>> 0, u, v);
        const last = l.spans[l.spans.length - 1];
        if (last && last[2] === T[tex] && last[0] + last[1] === first) last[1] += verts.length;
        else l.spans.push([first, verts.length, T[tex]]);
        return first;
    }
    // a quad as the kit writes one: two triangles, (x0 y0)(x1 y0)(x1 y1) and (x0 y0)(x1 y1)(x0 y1); c = one colour or four
    function quad(l, tex, x0, y0, x1, y1, c, uv) {
        const cs = Array.isArray(c) ? c : [c, c, c, c]; // top-left, top-right, bottom-right, bottom-left
        const [u0, v0, u1, v1] = uv || [0, 0, 1, 1];
        const a = [x0 * s, y0 * s, cs[0], u0, v0], b = [x1 * s, y0 * s, cs[1], u1, v0], d = [x1 * s, y1 * s, cs[2], u1, v1], e = [x0 * s, y1 * s, cs[3], u0, v1];
        return push(l, tex, [a, b, d, a, d, e]);
    }
    const probe = (name, ux, uy, rgb, tol, a) => probes.push({ name, x: Math.floor(ux * s), y: Math.floor(uy * s), rgb, tol: tol || 3, a });
    const BG = [0, 0, 0];

    // under pass (the view backings: colour writes only)
    quad(L('under', 'shapes'), 'white', 2, 2, 12, 12, 0xFF000080);
    probe('under pass: backing', 5, 5, [0, 0, 128]);
    // the target's alpha starts at 0 and the under pass writes none of it; an opaque over-pass shape writes 255
    probe('alpha: the under pass writes none', 4, 4, [0, 0, 128], 3, 0);

    // over pass, shapes: opaque, translucent over opaque and over the background, a triangle, a per-vertex gradient
    const shapes = L('over', 'shapes');
    // the pass order: a translucent over-pass shape across the under pass' backing blends over it, not under it
    quad(shapes, 'white', 8, 8, 14, 14, 0x8000FF00);
    const ag = 128 / 255;
    probe('pass order: over blends on the under backing', 10, 10, [0, Math.round(255 * ag), Math.round(128 * (1 - ag))]);
    quad(shapes, 'white', 15, 2, 25, 12, 0xFFFF0000);
    probe('shape: opaque', 17, 4, [255, 0, 0]);
    // its edges to the pixel: covered are the pixels whose centre (i + 0.5) lies in [X0, X1) once the replayer has
    // undone the kit's -0.5; a replayer that forgets the +0.5 moves every edge by half a pixel and fails one of these
    const first = (X) => Math.ceil(X - 0.5), red = [255, 0, 0];
    const px = (name, x, y, rgb) => probes.push({ name, x, y, rgb, tol: 3 });
    const X0 = 15 * s, X1 = 25 * s, Y0 = 2 * s, Y1 = 12 * s, my = Math.floor(4 * s), mx = Math.floor(17 * s);
    px('edge: first column inside', first(X0), my, red);
    px('edge: column before it outside', first(X0) - 1, my, BG);
    px('edge: last column inside', first(X1) - 1, my, red);
    px('edge: column after it outside', first(X1), my, BG);
    px('edge: first row inside', mx, first(Y0), red);
    px('edge: row above it outside', mx, first(Y0) - 1, BG);
    px('edge: last row inside', mx, first(Y1) - 1, red);
    px('edge: row below it outside', mx, first(Y1), BG);
    probes.push({ name: 'alpha: an opaque over-pass shape writes it', x: mx, y: my, rgb: red, tol: 3, a: 255 });
    // a rectangle whose four edges pass exactly through pixel centres (layout positions on halves): D3D's top-left
    // rule keeps the left column and the top row, drops the right column and the bottom row
    {
        const x0 = Math.floor(62 * s) + 0.5, x1 = Math.floor(72 * s) + 0.5, y0 = Math.floor(2 * s) + 0.5, y1 = Math.floor(12 * s) + 0.5;
        const c = 0xFFFF00FF, mag = [255, 0, 255];
        push(shapes, 'white', [[x0, y0, c, 0, 0], [x1, y0, c, 1, 0], [x1, y1, c, 1, 1], [x0, y0, c, 0, 0], [x1, y1, c, 1, 1], [x0, y1, c, 0, 1]]);
        const cx = Math.floor(67 * s), cy = Math.floor(7 * s);
        px('tie: the left column through pixel centres is in', x0 - 0.5, cy, mag);
        px('tie: the column before it is out', x0 - 1.5, cy, BG);
        px('tie: the right column through pixel centres is out', x1 - 0.5, cy, BG);
        px('tie: the column before it is in', x1 - 1.5, cy, mag);
        px('tie: the top row through pixel centres is in', cx, y0 - 0.5, mag);
        px('tie: the row above it is out', cx, y0 - 1.5, BG);
        px('tie: the bottom row through pixel centres is out', cx, y1 - 0.5, BG);
        px('tie: the row above it is in', cx, y1 - 1.5, mag);
    }
    // a rectangle with edges 0.4 past whole layout pixels: a replayer off by a quarter pixel moves its first column
    {
        const x0 = Math.floor(75 * s) + 0.4, x1 = Math.floor(85 * s) + 0.4, y0 = Math.floor(2 * s) + 0.4, y1 = Math.floor(12 * s) + 0.4;
        const c = 0xFF00FFFF, cyan = [0, 255, 255];
        push(shapes, 'white', [[x0, y0, c, 0, 0], [x1, y0, c, 1, 0], [x1, y1, c, 1, 1], [x0, y0, c, 0, 0], [x1, y1, c, 1, 1], [x0, y1, c, 0, 1]]);
        const cx = Math.floor(80 * s), cy = Math.floor(7 * s);
        px('fraction: first column in', first(x0), cy, cyan);
        px('fraction: column before out', first(x0) - 1, cy, BG);
        px('fraction: last column in', first(x1) - 1, cy, cyan);
        px('fraction: column after out', first(x1), cy, BG);
        px('fraction: first row in', cx, first(y0), cyan);
        px('fraction: row above out', cx, first(y0) - 1, BG);
        px('fraction: last row in', cx, first(y1) - 1, cyan);
        px('fraction: row below out', cx, first(y1), BG);
    }
    quad(shapes, 'white', 20, 7, 30, 17, 0x8000FF00);
    const a = 128 / 255;
    probe('blend: srcalpha over the red', 22, 9, [Math.round(255 * (1 - a)), Math.round(255 * a), 0]);
    probe('blend: srcalpha over the background', 28, 15, [0, Math.round(255 * a), 0]);
    push(shapes, 'white', [[35 * s, 2 * s, 0xFFFFFF00, 0, 0], [45 * s, 2 * s, 0xFFFFFF00, 1, 0], [35 * s, 12 * s, 0xFFFFFF00, 0, 1]]);
    probe('triangle: inside', 37, 4, [255, 255, 0]);
    probe('triangle: outside (past its hypotenuse)', 43, 10, BG);
    quad(shapes, 'white', 50, 2, 60, 12, [0xFF000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFF000000]);
    probe('gradient: left edge dark', 50.5, 7, [Math.round(255 * 0.05), Math.round(255 * 0.05), Math.round(255 * 0.05)], 14);
    probe('gradient: middle grey', 55, 7, [128, 128, 128], 8);
    // a shape under text and caption: the layer order puts the text on top whatever the vertex order
    quad(shapes, 'white', 2, 20, 12, 30, 0xFF0000FF);
    const partsShapes = count(shapes);

    // icons: the vertex colour alone, the texture's alpha (its red rgb must not show)
    quad(L('over', 'iconBig'), 'icon:synth.ring', 20, 20, 32, 32, 0xFF00FF00);
    probe('icon op: tint, not the texture red', 26, 26, [0, 255, 0]);
    probe('icon op: the texture alpha cuts the corner', 20.4, 20.4, BG);
    quad(L('over', 'iconSmall'), 'icon:synth.ring', 35, 20, 41, 26, 0x80FFFFFF);
    probe('icon small: half alpha white', 38, 23, [128, 128, 128]);

    // text: modulate, UV orientation, the marquee clip (the kit cuts the quad and its u at the room's edge)
    const tsmall = L('over', 'textSmall');
    let f = quad(tsmall, 'glyph:synth:U+0042', 45, 20, 55, 30, 0xFFFFFFFF);
    runs.push({ text: 'B', set: 'synth', x: 45 * s, y: 20 * s, px: 10 * s, align: 'left', track: 0, outline: false, clip: null, color: 0xFFFFFFFF, tag: 'synth.bar', pass: 'over', layer: 'textSmall', first: f, count: 6 });
    probe('glyph uv: the opaque left half', 47, 25, [255, 255, 255]);
    probe('glyph uv: the clear right half', 53, 25, BG);
    f = quad(tsmall, 'glyph:synth:U+0041', 60, 20, 65, 30, 0xFFFFFFFF, [0, 0, 0.5, 1]);
    runs.push({ text: 'A', set: 'synth', x: 60 * s, y: 20 * s, px: 10 * s, align: 'left', track: 0, outline: false, clip: [58 * s, 18 * s, 7 * s, 14 * s], color: 0xFFFFFFFF, tag: 'synth.clipped', pass: 'over', layer: 'textSmall', first: f, count: 6 });
    probe('clip: inside the room', 63, 25, [128, 128, 128]);
    probe('clip: past the room', 67, 25, BG);
    f = quad(tsmall, 'glyph:synth:U+005A', 75, 20, 80, 30, 0xFFFF8000);
    runs.push({ text: 'Z', set: 'synth', x: 75 * s, y: 20 * s, px: 10 * s, align: 'left', track: 0, outline: false, clip: null, color: 0xFFFF8000, tag: 'synth.missing', pass: 'over', layer: 'textSmall', first: f, count: 6 });
    probe('a name the atlas lacks: a solid box in its colour', 77, 25, [255, 128, 0]);
    const partsTextSmall = count(tsmall);

    // textBig over the blue shape (layer order), caption over textBig (layer order)
    f = quad(L('over', 'textBig'), 'glyph:synth:U+0041', 2, 20, 12, 30, 0xFFFFFFFF);
    runs.push({ text: 'A', set: 'synth', x: 2 * s, y: 20 * s, px: 10 * s, align: 'left', track: 0, outline: false, clip: null, color: 0xFFFFFFFF, tag: 'synth.big', pass: 'over', layer: 'textBig', first: f, count: 6 });
    probe('layer order: text over the shape drawn before it', 4, 22, [128, 128, 128]);
    // the ramp glyph: what a pixel shows is the linear filter between two texels of straight (not premultiplied)
    // alpha, white over black = the filtered alpha; nearest filtering or premultiplied texels give other values
    const tbig = L('over', 'textBig');
    const cell = CELLS['glyph:synth:U+0043'];
    const rampAt = (X0, X1, u0, u1, pxl) => {          // the expected level at pixel column pxl of a ramp quad
        const u = u0 + (u1 - u0) * (pxl + 0.5 - X0) / (X1 - X0);
        const t = cell[0] + u * cell[2] - 0.5, i = Math.floor(t), f = t - i;
        const A = (c) => RAMP(Math.max(cell[0], Math.min(cell[0] + cell[2] - 1, c)));
        return Math.round(A(i) * (1 - f) + A(i + 1) * f);
    };
    quad(tbig, 'glyph:synth:U+0043', 82, 20, 98, 30, 0xFFFFFFFF);
    for (const k of [20, 23, 27]) {        // pixels nearest the points half-way between two texel centres
        const X0 = 82 * s, X1 = 98 * s, pxl = Math.floor(X0 + (k + 1 - cell[0]) / cell[2] * (X1 - X0) - 0.5);
        const lv = rampAt(X0, X1, 0, 1, pxl);
        px(`filter: linear between texels (column ${k})`, pxl, Math.floor(25 * s), [lv, lv, lv]);
    }
    // the same ramp clipped at half its width: the kit cuts u to 0.5, so the right edge shows the middle of the ramp
    quad(tbig, 'glyph:synth:U+0043', 100, 20, 108, 30, 0xFFFFFFFF, [0, 0, 0.5, 1]);
    {
        const X0 = 100 * s, X1 = 108 * s, pxl = Math.ceil(X1 - 0.5) - 2;
        const lv = rampAt(X0, X1, 0, 0.5, pxl);
        px('clip: the cut u holds at the room edge', pxl, Math.floor(25 * s), [lv, lv, lv]);
    }
    // the top-half glyph: its top quarter is drawn, its bottom quarter is not
    quad(tbig, 'glyph:synth:U+0044', 82, 34, 92, 44, 0xFFFFFFFF);
    probe('glyph v: the opaque top half', 87, 36, [255, 255, 255]);
    probe('glyph v: the clear bottom half', 87, 42, BG);
    f = quad(L('over', 'caption'), 'glyph:synth:U+0041', 6, 24, 16, 34, 0xFF00FFFF);
    runs.push({ text: 'A', set: 'synth', x: 6 * s, y: 24 * s, px: 10 * s, align: 'left', track: 0, outline: false, clip: null, color: 0xFF00FFFF, tag: 'synth.caption', pass: 'over', layer: 'caption', first: f, count: 6 });
    probe('layer order: caption over text', 9, 27, [0, 128, 128]);
    probe('modulate: cyan x grey texel, alone', 14, 32, [0, 128, 128]);

    const counts = (pass) => passes[pass].layers.map(count);
    const marks = [
        { name: 'parts.end', pass: 'over', counts: [partsShapes, count(L('over', 'iconBig')), count(L('over', 'iconSmall')), partsTextSmall, count(L('over', 'textBig')), 0] },
        { name: 'comm.end', pass: 'over', counts: counts('over') },
    ];
    const kinds = ['view.target', 'view.ship', 'radiation.art', 'comm.portrait', 'game.bracket', 'promptbar', 'subtitles.dialogue', 'subtitles.chat', 'model.slot'];
    const holes = kinds.map((kind, i) => ({ kind, rect: [(2 + i * 10) * s, 70 * s, 9 * s, 9 * s], shown: kind !== 'game.bracket' }));
    for (const h of holes) probe(`hole ${h.kind}: nothing drawn (a placeholder only)`, h.rect[0] / s + 4.5, 74.5, BG);
    const hot = [{ name: 'synth.button', rect: [15 * s, 2 * s, 10 * s, 10 * s] }, { name: 'synth.icon', rect: [20 * s, 20 * s, 12 * s, 12 * s] }];
    const items = [{ tag: 'synth.shape', rect: [15 * s, 2 * s, 10 * s, 10 * s] }, { tag: 'synth.icon', rect: [20 * s, 20 * s, 12 * s, 12 * s] }, { tag: 'synth.text', rect: [45 * s, 20 * s, 10 * s, 10 * s] }];
    const list = {
        schema: 'fldl/0', privacy: 'public', build: { game: 'synthetic', bench: 'synth_fixture.js' }, size: [W, H],
        state: 'synthetic', variant: '', relayout: false, sets: [{ name: 'synth', px: Math.round(10 * s) }],
        textures, passes, marks, runs, items, hot, holes,
    };
    return { list, probes };
}

function main(argv) {
    let out = path.join(__dirname, '..', '..', 'build', 'bench', 'synthetic');
    let sizes = SIZES;
    for (let i = 0; i < argv.length; i++) {
        if (argv[i] === '--out') out = path.resolve(argv[++i]);
        else if (argv[i] === '--sizes') sizes = argv[++i].split(',').map((t) => t.split('x').map(Number));
        else { console.error('usage: node synth_fixture.js [--out <folder>] [--sizes WxH,...]'); return 2; }
    }
    fs.mkdirSync(out, { recursive: true });
    const at = atlas();
    fs.writeFileSync(path.join(out, 'synthetic_atlas.png'), at.png);
    fs.writeFileSync(path.join(out, 'synthetic_atlas.json'), JSON.stringify(at.manifest, null, 1));
    const bundle = { atlas: at.manifest, atlasPng: at.png.toString('base64'), lists: {} };
    for (const [w, h] of sizes) {
        const { list, probes } = build(w, h);
        fs.writeFileSync(path.join(out, `synthetic_${w}x${h}.fldl.json`), JSON.stringify(list));
        fs.writeFileSync(path.join(out, `synthetic_${w}x${h}.probes.json`), JSON.stringify(probes, null, 1));
        bundle.lists[`${w}x${h}`] = { list, probes };
    }
    fs.writeFileSync(path.join(out, 'synthetic.js'), 'window.SYNTH = ' + JSON.stringify(bundle) + ';\n');
    console.log(`synthetic fixture: ${sizes.map((s) => s.join('x')).join(', ')} -> ${out}`);
    return 0;
}

module.exports = { build, atlas, png, SIZES };
if (require.main === module) process.exitCode = main(process.argv.slice(2));
