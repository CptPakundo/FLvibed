/* The replayer's tests (tools\hudbench\FORMAT.md, v0 provisional). Under Node, no browser: the list reader against the
   synthetic fixture at every preset size (it validates clean), the draw order (passes, then the kit's six layers), the
   colour and coordinate rules, every refusal the reader owes, the atlas mapping, the list comparison, the pixel
   difference, and a static check that the page makes no network request (no URL, fetch, XHR, socket, beacon or
   import in its code; its CSP blocks connections).
   --headless also replays the fixture in a headless Chromium browser (chromium() below; WebGL by SwiftShader, so the result does not
   depend on the GPU) and checks every probe pixel, the difference view's logic and the pass switch at 1280x800,
   1920x1080 and 3840x2160; the fixture is written to a temporary folder and removed after.

   --bench <folder> checks a folder the bench wrote (build\bench\flbench.exe card ...: card@<W>x<H>.fldl.json, its
   .probes.json, card_atlas.json / .png) instead: every list and the atlas validate clean, every texture a list names
   is in the atlas, and with --headless each list replays with every probe pixel as the bench computed it (the
   kit's own record of where it drew). Probes only: the fixture's own checks (its red square, its icon corner) do
   not apply to another list.

   node tools\hudbench\test_replayer.js [--headless] [--gpu] [--bench <folder>]      exit 0 when every check holds */
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');
const FLDL = require('./web/fldl.js');
const synth = require('./synth_fixture.js');

let checks = 0, fails = 0;
function ok(cond, what) { checks++; if (!cond) { fails++; console.log('FAIL ' + what); } }
function refuses(doc, what, needle) {
    const r = FLDL.validate(doc);
    ok(!r.list && r.errors.some((e) => e.includes(needle)), `refuses ${what} (wanted an error with '${needle}', got ${JSON.stringify(r.errors)})`);
}
const clone = (x) => JSON.parse(JSON.stringify(x));

// the fixture validates clean at every size, and through the text path too
for (const [w, h] of synth.SIZES) {
    const { list, probes } = synth.build(w, h);
    const r = FLDL.parse(JSON.stringify(list));
    ok(r.list && !r.errors.length && !r.warnings.length, `${w}x${h}: the fixture validates clean (${r.errors.concat(r.warnings).join('; ')})`);
    ok(probes.length >= 20, `${w}x${h}: probes written`);
    ok(probes.every((p) => p.x >= 0 && p.y >= 0 && p.x < w && p.y < h), `${w}x${h}: probes inside the target`);
}
const { list } = synth.build(1280, 800);

// every primitive kind the format has is in the fixture
const used = new Set();
for (const pn of FLDL.PASSES) for (const l of list.passes[pn].layers) for (const s of l.spans) used.add(list.textures[s[2]].split(':')[0]);
ok(['white', 'glyph', 'icon'].every((k) => used.has(k)), 'the fixture uses white, glyph and icon textures');
ok(FLDL.PASSES.every((pn) => list.passes[pn].layers.some((l) => l.v.length)), 'the fixture draws in both passes');
ok(list.passes.over.layers.every((l) => l.v.length), 'the fixture draws in all six layers of the over pass');
ok(FLDL.HOLE_KINDS.every((k) => list.holes.some((h) => h.kind === k)), 'the fixture names every hole kind');
ok(list.runs.some((r) => r.clip) && list.runs.some((r) => !r.clip), 'the fixture has a clipped and an unclipped run');
ok(list.marks.length === 2 && list.hot.length && list.items.length, 'the fixture has marks, hot-spots and items');

// the draw order: the under pass first, then the over pass; in each the kit's layer order; empty layers skipped
const order = FLDL.drawOrder(list).map((c) => c.pass + '.' + c.layer);
ok(JSON.stringify(order) === JSON.stringify(['under.shapes', 'over.shapes', 'over.iconBig', 'over.iconSmall', 'over.textSmall', 'over.textBig', 'over.caption']),
    'draw order ' + order.join(' '));
ok(FLDL.drawOrder(list).every((c) => c.op === FLDL.KIT_OPS[c.layer]), 'each layer drawn with the kit op');
ok(FLDL.drawOrder(list)[0].writeAlpha === false && FLDL.drawOrder(list)[1].writeAlpha === true, 'the under pass writes no alpha');
ok(FLDL.drawOrder(list, { passes: { under: false }, layers: { caption: false } }).map((c) => c.layer).join() === 'shapes,iconBig,iconSmall,textSmall,textBig',
    'switched-off passes and layers are not drawn');

// colours and coordinates
ok(JSON.stringify(FLDL.argb(0x80FF4020)) === '[255,64,32,128]', 'ARGB to RGBA bytes');
ok(JSON.stringify(FLDL.argb(0xFFFFFFFF)) === '[255,255,255,255]', 'ARGB white');
const c0 = FLDL.toClip(-0.5, -0.5, 1280, 800), c1 = FLDL.toClip(1279.5, 799.5, 1280, 800);
ok(c0[0] === -1 && c0[1] === 1 && c1[0] === 1 && c1[1] === -1, 'the kit -0.5 coordinates reach the target edges exactly');

// the atlas: the fixture's manifest is clean; UVs inside a glyph box map into its cell; a missing name is counted
const at = synth.atlas();
const pa = FLDL.parseAtlas(JSON.stringify(at.manifest));
ok(pa.atlas && !pa.errors.length, 'the synthetic atlas manifest validates');
const lb = FLDL.layerBuffers(list, 'over', 3, pa.atlas);
ok(lb.missing['glyph:synth:U+005A'] === 2 && lb.missingTris === 2, 'a name the atlas lacks is counted (2 triangles)');
ok(Math.abs(lb.uv[0] - 16 / 64) < 1e-6 && Math.abs(lb.uv[1] - 0) < 1e-6 && lb.slot[0] === 1, 'glyph box UV (0,0) -> its cell corner');
ok(Math.abs(lb.uv[2] - 32 / 64) < 1e-6, 'glyph box UV (1,0) -> the cell right edge');
const clipFirst = list.runs.find((r) => r.tag === 'synth.clipped').first;
ok(Math.abs(lb.uv[(clipFirst + 1) * 2] - 8 / 64) < 1e-6, 'the clipped glyph keeps half its cell');
ok(FLDL.layerBuffers(list, 'over', 0, pa.atlas).slot.every((s) => s === 0), 'white needs no atlas (slot 0)');
ok(!FLDL.parseAtlas(Object.assign(clone(at.manifest), { entries: { 'icon:x': [60, 0, 8, 8] } })).atlas, 'an atlas entry past the image is refused');
ok(!FLDL.parseAtlas(Object.assign(clone(at.manifest), { schema: 'x' })).atlas, 'an atlas of another schema is refused');

// refusals: the reader fails closed
refuses(Object.assign(clone(list), { schema: 'fldl/1' }), 'another schema', 'schema');
refuses(Object.assign(clone(list), { privacy: 'secret' }), 'an unknown privacy', 'privacy');
refuses(Object.assign(clone(list), { size: [0, 800] }), 'a zero size', 'size');
refuses(Object.assign(clone(list), { textures: ['white', 'white'] }), 'a texture twice', 'twice');
refuses(Object.assign(clone(list), { textures: ['white', 'glyph:x:A'] }), 'a bad texture name', 'textures[1]');
let d = clone(list); [d.passes.over.layers[0], d.passes.over.layers[1]] = [d.passes.over.layers[1], d.passes.over.layers[0]];
refuses(d, 'layers out of the kit order', "expected 'shapes'");
d = clone(list); d.passes.over.layers[0].v.push(1, 2, 3);
refuses(d, 'a vertex array that is not whole triangles', 'three vertices per triangle');
ok(list.passes.over.layers[3].spans.length === 3, 'the small-text layer holds three spans');
d = clone(list); d.passes.over.layers[3].spans[1][0] += 3;
refuses(d, 'a gap between spans', 'expected');
d = clone(list); d.passes.over.layers[3].spans[0][2] = 99;
refuses(d, 'a span naming no texture', 'no such entry');
d = clone(list); d.passes.over.layers[3].spans.pop();
refuses(d, 'spans that do not cover the layer', 'cover');
d = clone(list); d.passes.over.layers[0].v[2] = 2 ** 32;
refuses(d, 'a colour past 32 bits', 'colour');
d = clone(list); d.passes.over.layers[0].v[0] = NaN;
refuses(d, 'a position that is not a number', 'finite');
d = clone(list); d.passes.over.layers[2].op = 'add';
refuses(d, 'an unknown op', 'modulate or icon');
d = clone(list); delete d.passes.under;
refuses(d, 'a missing pass', 'passes.under missing');
d = clone(list); d.passes.under.writeAlpha = 'no';
refuses(d, 'writeAlpha not a boolean', 'writeAlpha');
d = clone(list); d.runs[0].count = 600;
refuses(d, 'a run past its layer', 'past the layer');
d = clone(list); d.marks = d.marks.slice().reverse();
refuses(d, 'marks out of order', 'previous mark');
d = clone(list); d.holes[0].rect = [1, 2, 3];
refuses(d, 'a hole without a rectangle', 'holes[0].rect');
ok(!FLDL.parse('{').list, 'not JSON is refused');
// every value the page shows has its type (a string put into the inspector unchecked could carry markup)
d = clone(list); d.holes[0].shown = '<meta http-equiv="refresh" content="0">';
refuses(d, 'a hole whose shown is not a boolean', 'holes[0].shown');
d = clone(list); d.holes[1].kind = 7;
refuses(d, 'a hole kind that is not a string', 'holes[1].kind');
d = clone(list); d.hot[0].name = { x: 1 };
refuses(d, 'a hot-spot name that is not a string', 'hot[0].name');
d = clone(list); d.runs[0].color = '#fff';
refuses(d, 'a run colour that is not ARGB', 'runs[0].color');
d = clone(list); d.state = ['x'];
refuses(d, 'a state that is not a string', 'state');
d = clone(list); d.holes[0].kind = 'view.new';
let r = FLDL.validate(d);
ok(r.list && r.warnings.some((w) => w.includes('view.new')), 'an unknown hole kind is a warning, the list still loads');
d = clone(list); d.passes.over.layers[1].op = 'modulate';
r = FLDL.validate(d);
ok(r.list && r.warnings.some((w) => w.includes("'icon' today")), 'an op other than the kit draws today is followed, with a warning');
d = clone(list); d.privacy = FLDL.MARKER;
ok(FLDL.validate(d).list && FLDL.summary(FLDL.validate(d).list).privacy === 'private', 'a private list loads and is shown as private');

// the list comparison
ok(FLDL.compareLists(list, clone(list)).length === 0, 'a list equals itself');
d = clone(list); d.passes.over.layers[0].v[0] += 0.005;
ok(FLDL.compareLists(list, d).length === 0, 'a move under 0.01 px is equal');
d.passes.over.layers[0].v[0] += 0.02;
ok(FLDL.compareLists(list, d).some((l) => l.includes('moved')), 'a move over 0.01 px is reported');
d = clone(list); d.passes.over.layers[1].v[2] = 0xFF123456;
ok(FLDL.compareLists(list, d).some((l) => l.includes('colours')), 'a colour change is reported');
d = clone(list); d.passes.over.layers[3].spans[0][2] = 1;
ok(FLDL.compareLists(list, d).some((l) => l.includes('another texture')), 'a texture change is reported');
ok(FLDL.compareLists(list, synth.build(1920, 1080).list).some((l) => l.startsWith('size')), 'a size change is reported');

// the pixel difference
const A = new Uint8Array(4 * 4 * 4).fill(10), B = A.slice();
B[(2 * 4 + 1) * 4] = 40;
const pd = FLDL.diffPixels(A, B, 4, 4, 8, new Uint8Array(64));
ok(pd.count === 1 && pd.max === 30 && JSON.stringify(pd.box) === '[1,2,1,1]', 'the pixel difference finds the one pixel');
ok(FLDL.diffPixels(A, B, 4, 4, 30).count === 0, 'the threshold holds');

// no network: nothing in the replayer page's code can reach one. The bench's window (bench.*, served by `flbench serve` on
// 127.0.0.1) talks to its own program only: every request it makes is to a path of its own origin, its CSP connect-src 'self'.
const web = path.join(__dirname, 'web');
for (const f of fs.readdirSync(web).filter((n) => /^bench\.(js|html|css)$/.test(n))) {
    const t = fs.readFileSync(path.join(web, f), 'utf8');
    const far = t.match(/https?:|\/\/[a-z]|XMLHttpRequest|WebSocket|EventSource|\bimport\s*\(|@import|(fetch|get|sendBeacon)\(\s*(?!['`]\/[a-z_]|url\))[^)]/gi); // (url: the helper get(url), called with paths)
    ok(!far, `${f}: requests only to its own program (${far})`);
}
for (const f of fs.readdirSync(web).filter((n) => /\.(js|html|css)$/.test(n) && !/^bench\./.test(n))) {
    const t = fs.readFileSync(path.join(web, f), 'utf8');
    const hits = t.match(/https?:|\bfetch\s*\(|XMLHttpRequest|WebSocket|EventSource|sendBeacon|\bimport\s*\(|<link\b[^>]*href\s*=\s*["'][^"']*(:|\/\/)|@import|src\s*=\s*["']\/\//g);
    ok(!hits, `${f}: no network reference (${hits})`);
}
for (const f of ['index.html', 'test.html'].filter((n) => fs.existsSync(path.join(web, n)))) {
    const t = fs.readFileSync(path.join(web, f), 'utf8');
    ok(/Content-Security-Policy[^>]*default-src 'none'[^>]*connect-src 'none'/.test(t), `${f}: its CSP blocks every connection`);
}
// the marker is never written literally into the public code
for (const f of fs.readdirSync(web).concat(['FORMAT.md', 'synth_fixture.js', 'test_replayer.js'].map((n) => '../' + n))) {
    const p = path.join(web, f);
    if (fs.statSync(p).isFile()) ok(!fs.readFileSync(p, 'utf8').toLowerCase().includes(FLDL.MARKER), `${f}: no literal private marker`);
}

// A Chromium browser for the headless run, found as tools\hudbench\app\browser.ps1 finds the bench's window (the owner,
// 13 Nov 2026: "Prefer default browser"): the user's default browser when it is a Chromium one, else the first installed
// of Chrome, Brave, Vivaldi, Edge. (Firefox has no --dump-dom, so it cannot run this check.)
const CHROMIUMS = /\\(chrome|brave|vivaldi|chromium|msedge)\.exe$/i;
function chromium() {
    try {
        const reg = (k, v) => execFileSync('reg', ['query', k].concat(v ? ['/v', v] : ['/ve']), { encoding: 'utf8', stdio: ['ignore', 'pipe', 'ignore'] });
        const prog = /ProgId\s+REG_SZ\s+(\S+)/.exec(reg('HKCU\\Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\https\\UserChoice', 'ProgId'));
        const cmd = prog && /REG_\w+\s+"?([^"]+?\.exe)"?/i.exec(reg(`HKCR\\${prog[1]}\\shell\\open\\command`));
        if (cmd && CHROMIUMS.test(cmd[1]) && fs.existsSync(cmd[1])) return cmd[1];
    } catch (e) { /* (no default found: the list below) */ }
    const local = process.env.LOCALAPPDATA || '', pf = process.env.ProgramFiles || 'C:\\Program Files', pf86 = process.env['ProgramFiles(x86)'] || 'C:\\Program Files (x86)';
    for (const p of [`${pf}\\Google\\Chrome\\Application\\chrome.exe`, `${pf86}\\Google\\Chrome\\Application\\chrome.exe`, `${local}\\Google\\Chrome\\Application\\chrome.exe`,
        `${pf}\\BraveSoftware\\Brave-Browser\\Application\\brave.exe`, `${local}\\BraveSoftware\\Brave-Browser\\Application\\brave.exe`,
        `${local}\\Vivaldi\\Application\\vivaldi.exe`, `${pf}\\Vivaldi\\Application\\vivaldi.exe`,
        `${pf86}\\Microsoft\\Edge\\Application\\msedge.exe`, `${pf}\\Microsoft\\Edge\\Application\\msedge.exe`])
        if (fs.existsSync(p)) return p;
    return null;
}
// The bench's folder as the page's bundle (window.SYNTH), after the checks Node can make; null when it cannot be read.
function benchBundle(dir) {
    const read = (f) => fs.readFileSync(path.join(dir, f), 'utf8');
    let atlas;
    try { atlas = JSON.parse(read('card_atlas.json')); } catch (e) { ok(false, `bench: ${dir}\\card_atlas.json: ${e.message}`); return null; }
    const at = FLDL.parseAtlas(atlas);
    ok(!!at.atlas, 'bench: the atlas validates (' + (at.errors || []).join('; ') + ')');
    const bundle = { atlas, atlasPng: fs.readFileSync(path.join(dir, atlas.image)).toString('base64'), lists: {}, probesOnly: true };
    const files = fs.readdirSync(dir).filter((n) => /^card@\d+x\d+\.fldl\.json$/.test(n));
    ok(files.length > 0, `bench: lists in ${dir}`);
    for (const f of files) {
        const r = FLDL.parse(read(f));
        ok(!!r.list && !r.errors.length, `bench: ${f} validates (${r.errors.join('; ')})`);
        if (r.warnings.length) console.log(`  ${f} warnings: ${r.warnings.join('; ')}`);
        if (!r.list) continue;
        ok(r.list.privacy === atlas.privacy, `bench: ${f} and the atlas have the same privacy`);
        const lacking = r.list.textures.filter((t) => t !== 'white' && !(t in atlas.entries));
        ok(!lacking.length, `bench: every texture of ${f} is in the atlas (${lacking.slice(0, 5).join(', ')})`);
        const probes = JSON.parse(read(f.replace('.fldl.json', '.probes.json')));
        ok(probes.length > 0, `bench: ${f} has probes`);
        bundle.lists[r.list.size.join('x')] = { list: r.list, probes };
    }
    return bundle;
}

function headless(gpu, bench) {
    const exe = chromium();
    if (!exe) { ok(false, 'headless: no Chromium browser found (Chrome, Brave, Vivaldi, Edge)'); return; }
    const tmp = fs.mkdtempSync(path.join(os.tmpdir(), 'flv_hudbench_'));
    try {
        if (bench) {
            fs.mkdirSync(path.join(tmp, 'fx'));
            fs.writeFileSync(path.join(tmp, 'fx', 'synthetic.js'), 'window.SYNTH = ' + JSON.stringify(bench) + ';\n');
        } else
            execFileSync(process.execPath, [path.join(__dirname, 'synth_fixture.js'), '--out', path.join(tmp, 'fx')], { stdio: 'pipe' });
        const url = (p) => 'file:///' + p.replace(/\\/g, '/').split('/').map(encodeURIComponent).join('/').replace(/^([A-Za-z])%3A/, '$1:');
        const page = url(path.join(web, 'test.html')) + '?fixture=' + encodeURIComponent(url(path.join(tmp, 'fx', 'synthetic.js')));
        const flags = ['--headless=new', '--no-first-run', '--disable-extensions', '--mute-audio', '--disable-background-networking',
            '--disable-component-update', '--disable-sync', '--no-default-browser-check', `--user-data-dir=${path.join(tmp, 'profile')}`,
            '--virtual-time-budget=20000', '--dump-dom'];
        if (!gpu) flags.splice(1, 0, '--use-angle=swiftshader', '--enable-unsafe-swiftshader');
        const t0 = Date.now();
        const dom = execFileSync(exe, flags.concat([page]), { encoding: 'utf8', timeout: 180000, stdio: ['ignore', 'pipe', 'ignore'], maxBuffer: 64 << 20 });
        const m = dom.match(/RESULT (\{.*\})/);
        ok(!!m, 'headless: the page reported (' + (m ? '' : dom.slice(0, 300)) + ')');
        if (!m) return;
        const rep = JSON.parse(m[1].replace(/&quot;/g, '"').replace(/&amp;/g, '&').replace(/&lt;/g, '<').replace(/&gt;/g, '>'));
        console.log(`headless ${path.basename(exe)} (${rep.renderer}), ${((Date.now() - t0) / 1000).toFixed(1)} s`);
        if (rep.error) ok(false, 'headless: ' + rep.error);
        ok(!rep.violations || !rep.violations.length, 'headless: no CSP violation ' + JSON.stringify(rep.violations));
        for (const [key, s] of Object.entries(rep.sizes || {})) {
            const bad = (s.probes || []).filter((p) => !p.ok);
            for (const p of bad) console.log(`  ${key} probe '${p.name}' at ${p.at}: want ${p.want} got ${p.got}`);
            if (s.diffError) console.log(`  ${key} difference: ${JSON.stringify(s.diff)}`);
            if (!s.diff) // (a bench list: probes only)
                console.log(`  ${key}: ${s.probes.length - bad.length}/${s.probes.length} probes, draw ${s.drawMs} ms, calls ${(s.calls || []).join(' ')}, ` +
                    `missing ${JSON.stringify(s.missing)}${s.errors && s.errors.length ? ', errors ' + s.errors.join('; ') : ''}`);
            else console.log(`  ${key}: ${s.probes.length - bad.length}/${s.probes.length} probes, draw ${s.drawMs} ms, calls ${s.calls.join(' ')}, ` +
                `difference same ${s.diff.same} / changed ${s.diff.changed} px in ${JSON.stringify(s.diff.box)}, missing ${JSON.stringify(s.missing)}, no-atlas corner ${s.noAtlasCorner}, over off ${JSON.stringify(s.overOff)}`);
            ok(s.fails === 0, `headless ${key}: ${s.fails} failure(s)`);
            checks += s.probes.length;
        }
        ok(Object.keys(rep.sizes || {}).length === (bench ? Object.keys(bench.lists).length : synth.SIZES.length), 'headless: every size replayed');
    } finally {
        fs.rmSync(tmp, { recursive: true, force: true });
    }
}
const benchAt = process.argv.indexOf('--bench');
const bench = benchAt > 0 ? benchBundle(path.resolve(process.argv[benchAt + 1] || '.')) : null;
if (process.argv.includes('--headless') && (benchAt < 0 || bench)) headless(process.argv.includes('--gpu'), bench);

console.log(`test_replayer: ${checks} checks, ${fails} failed`);
process.exitCode = fails ? 1 : 0;
