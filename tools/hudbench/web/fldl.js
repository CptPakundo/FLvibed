/* The display list's reader (tools\hudbench\FORMAT.md, v0: PROVISIONAL until B2 writes real lists): parse and validate a
   list (fail closed: any error refuses the whole list), the draw order (passes, then the kit's six layers), colours,
   named-texture resolution against an atlas manifest, a summary, a structural comparison of two lists and a pixel
   difference. No drawing here (gl.js draws) and no I/O: the page hands it text, Node's unit test does the same.
   A plain script: window.FLDL in the page, module.exports under Node. */
(function (root, factory) {
    const api = factory();
    if (typeof module === 'object' && module.exports) module.exports = api;
    else root.FLDL = api;
})(typeof self !== 'undefined' ? self : this, function () {
    'use strict';
    const SCHEMA = 'fldl/0';
    const ATLAS_SCHEMA = 'flatlas/0';
    // the private marker, never written literally in a public file (tools\private_check.py refuses files holding it)
    const MARKER = 'flbench' + '-private';
    const PASSES = ['under', 'over'];
    // the kit's six layers in draw order and their texture op today (core\ui\hud2_kit.cpp LayerId, End)
    const LAYERS = ['shapes', 'iconBig', 'iconSmall', 'textSmall', 'textBig', 'caption'];
    const KIT_OPS = { shapes: 'modulate', iconBig: 'icon', iconSmall: 'icon', textSmall: 'modulate', textBig: 'modulate', caption: 'modulate' };
    const OPS = ['modulate', 'icon'];
    const HOLE_KINDS = ['view.target', 'view.ship', 'radiation.art', 'comm.portrait', 'game.bracket', 'promptbar',
        'subtitles.dialogue', 'subtitles.chat', 'model.slot'];
    const PER_VERTEX = 5; // x y argb u v
    const TEX_NAME = /^(white|glyph:[A-Za-z0-9_.-]+:U\+[0-9A-F]{4,6}|icon:[A-Za-z0-9_.-]+)$/;
    const MAX_SIDE = 16384;

    function isNum(x) { return typeof x === 'number' && Number.isFinite(x); }
    function isInt(x) { return Number.isInteger(x); }
    function isRect(r) { return Array.isArray(r) && r.length === 4 && r.every(isNum) && r[2] >= 0 && r[3] >= 0; }

    /* D3DCOLOR (0xAARRGGBB) -> [r, g, b, a] bytes */
    function argb(c) { return [(c >>> 16) & 255, (c >>> 8) & 255, c & 255, (c >>> 24) & 255]; }

    /* Parse and validate a list. Returns {list, errors, warnings}; list is null when there is any error. */
    function parse(text) {
        let doc;
        try { doc = typeof text === 'string' ? JSON.parse(text) : text; }
        catch (e) { return { list: null, errors: ['not JSON: ' + e.message], warnings: [] }; }
        return validate(doc);
    }

    function validate(doc) {
        const errors = [], warnings = [];
        const err = (s) => errors.push(s), warn = (s) => warnings.push(s);
        if (!doc || typeof doc !== 'object' || Array.isArray(doc)) return { list: null, errors: ['the file is not a JSON object'], warnings };
        if (doc.schema !== SCHEMA) err(`schema '${doc.schema}': this replayer reads '${SCHEMA}' only`);
        if (doc.privacy !== 'public' && doc.privacy !== MARKER) err(`privacy '${doc.privacy}': 'public' or the private marker`);
        const size = doc.size;
        if (!Array.isArray(size) || size.length !== 2 || !size.every((n) => isInt(n) && n > 0 && n <= MAX_SIDE)) err('size: [width, height] in pixels, 1..' + MAX_SIDE);
        const textures = doc.textures;
        if (!Array.isArray(textures) || !textures.length) err('textures: a non-empty array of names');
        else {
            const seen = new Set();
            textures.forEach((t, i) => {
                if (typeof t !== 'string' || !TEX_NAME.test(t)) err(`textures[${i}] '${t}': white, glyph:<set>:U+XXXX or icon:<name>`);
                else if (seen.has(t)) err(`textures[${i}] '${t}' twice`);
                seen.add(t);
            });
        }
        const nTex = Array.isArray(textures) ? textures.length : 0;
        const vcount = {}; // pass -> [count per layer]
        if (!doc.passes || typeof doc.passes !== 'object') err('passes: {under, over}');
        else {
            for (const k of Object.keys(doc.passes)) if (!PASSES.includes(k)) err(`passes.${k}: unknown pass (under, over)`);
            for (const pn of PASSES) {
                const p = doc.passes[pn];
                if (!p) { err(`passes.${pn} missing`); continue; }
                if (typeof p.writeAlpha !== 'boolean') err(`passes.${pn}.writeAlpha: true or false`);
                if (!Array.isArray(p.layers) || p.layers.length !== LAYERS.length) { err(`passes.${pn}.layers: the six layers ${LAYERS.join(', ')}`); continue; }
                vcount[pn] = [];
                p.layers.forEach((l, li) => {
                    const at = `passes.${pn}.layers[${li}]`;
                    if (!l || l.name !== LAYERS[li]) err(`${at}.name '${l && l.name}': expected '${LAYERS[li]}' (the kit's order)`);
                    if (!OPS.includes(l && l.op)) err(`${at}.op '${l && l.op}': modulate or icon`);
                    else if (l.op !== KIT_OPS[LAYERS[li]]) warn(`${at}.op '${l.op}': the kit draws ${LAYERS[li]} with '${KIT_OPS[LAYERS[li]]}' today`);
                    const v = l && l.v;
                    if (!Array.isArray(v) || v.length % (PER_VERTEX * 3)) { err(`${at}.v: x y argb u v per vertex, three vertices per triangle (${v && v.length} numbers)`); vcount[pn].push(0); return; }
                    const n = v.length / PER_VERTEX;
                    vcount[pn].push(n);
                    for (let i = 0; i < v.length; i += PER_VERTEX) {
                        if (!isNum(v[i]) || !isNum(v[i + 1]) || !isNum(v[i + 3]) || !isNum(v[i + 4])) { err(`${at}.v: vertex ${i / PER_VERTEX} has a value that is not a finite number`); break; }
                        const c = v[i + 2];
                        if (!isInt(c) || c < 0 || c > 0xFFFFFFFF) { err(`${at}.v: vertex ${i / PER_VERTEX} colour ${c}: 0..0xFFFFFFFF (ARGB)`); break; }
                    }
                    const spans = l.spans;
                    if (!Array.isArray(spans)) { err(`${at}.spans: [[first, count, texture], ...]`); return; }
                    let next = 0;
                    for (let si = 0; si < spans.length; si++) {
                        const s = spans[si];
                        if (!Array.isArray(s) || s.length !== 3 || !s.every(isInt)) { err(`${at}.spans[${si}]: [first, count, texture]`); break; }
                        if (s[0] !== next) { err(`${at}.spans[${si}] starts at ${s[0]}, expected ${next} (spans cover the layer in order)`); break; }
                        if (s[1] <= 0 || s[1] % 3) { err(`${at}.spans[${si}] count ${s[1]}: a positive multiple of 3`); break; }
                        if (s[2] < 0 || s[2] >= nTex) { err(`${at}.spans[${si}] texture ${s[2]}: no such entry in textures`); break; }
                        next += s[1];
                    }
                    if (next !== n && errors.length === 0) err(`${at}.spans cover ${next} of ${n} vertices`);
                });
            }
        }
        const sizeOk = Array.isArray(size) && size.length === 2;
        const rectList = (key, need) => {
            const a = doc[key];
            if (a === undefined) return;
            if (!Array.isArray(a)) { err(`${key}: an array`); return; }
            a.forEach((r, i) => {
                if (!r || typeof r !== 'object') { err(`${key}[${i}]: an object`); return; }
                if (!isRect(r.rect)) err(`${key}[${i}].rect: [x, y, w, h]`);
                for (const f of need) if (r[f] === undefined) err(`${key}[${i}].${f} missing`);
            });
        };
        rectList('holes', ['kind', 'shown']);
        rectList('hot', ['name']);
        rectList('items', ['tag']);
        // every value the page shows must have its type: strings for names, booleans for flags (a page shows them)
        const str = (key, field) => (Array.isArray(doc[key]) ? doc[key] : []).forEach((r, i) => { if (r && r[field] !== undefined && typeof r[field] !== 'string') err(`${key}[${i}].${field}: a string`); });
        str('holes', 'kind'); str('hot', 'name'); str('items', 'tag');
        (Array.isArray(doc.holes) ? doc.holes : []).forEach((h, i) => {
            if (h && h.shown !== undefined && typeof h.shown !== 'boolean') err(`holes[${i}].shown: true or false`);
            if (h && typeof h.kind === 'string' && !HOLE_KINDS.includes(h.kind)) warn(`holes[${i}].kind '${h.kind}': not one of the known holes (${HOLE_KINDS.join(', ')})`);
        });
        for (const k of ['state', 'variant']) if (doc[k] !== undefined && typeof doc[k] !== 'string') err(`${k}: a string`);
        if (doc.relayout !== undefined && typeof doc.relayout !== 'boolean') err('relayout: true or false');
        if (doc.runs !== undefined) {
            if (!Array.isArray(doc.runs)) err('runs: an array');
            else doc.runs.forEach((r, i) => {
                const at = `runs[${i}]`;
                if (!r || typeof r.text !== 'string') { err(`${at}.text: a string`); return; }
                if (!PASSES.includes(r.pass)) err(`${at}.pass '${r.pass}'`);
                const li = LAYERS.indexOf(r.layer);
                if (li < 0) err(`${at}.layer '${r.layer}'`);
                if (!isInt(r.first) || !isInt(r.count) || r.first < 0 || r.count < 0 || r.first % 3 || r.count % 3) err(`${at}: first / count, whole triangles`);
                else if (vcount[r.pass] && li >= 0 && r.first + r.count > vcount[r.pass][li]) err(`${at}: vertices ${r.first}..${r.first + r.count} past the layer's ${vcount[r.pass][li]}`);
                if (!isNum(r.x) || !isNum(r.y) || !isNum(r.px)) err(`${at}: x, y, px numbers`);
                if (r.clip !== null && r.clip !== undefined && !isRect(r.clip)) err(`${at}.clip: null or [x, y, w, h]`);
                if (r.color !== undefined && (!isInt(r.color) || r.color < 0 || r.color > 0xFFFFFFFF)) err(`${at}.color: 0..0xFFFFFFFF (ARGB)`);
                for (const f of ['set', 'tag', 'align']) if (r[f] !== undefined && typeof r[f] !== 'string') err(`${at}.${f}: a string`);
            });
        }
        if (doc.marks !== undefined) {
            if (!Array.isArray(doc.marks)) err('marks: an array');
            else {
                const last = {};
                doc.marks.forEach((m, i) => {
                    const at = `marks[${i}]`;
                    if (!m || typeof m.name !== 'string' || !PASSES.includes(m.pass)) { err(`${at}: {name, pass, counts}`); return; }
                    if (!Array.isArray(m.counts) || m.counts.length !== LAYERS.length || !m.counts.every((c) => isInt(c) && c >= 0 && c % 3 === 0)) { err(`${at}.counts: six vertex counts, whole triangles`); return; }
                    const vc = vcount[m.pass];
                    if (vc && m.counts.some((c, li) => c > vc[li])) err(`${at}.counts past the layers' vertex counts`);
                    const prev = last[m.pass];
                    if (prev && m.counts.some((c, li) => c < prev[li])) err(`${at}.counts fall below the previous mark's (marks are in order)`);
                    last[m.pass] = m.counts;
                });
            }
        }
        const known = ['schema', 'privacy', 'build', 'size', 'state', 'variant', 'relayout', 'record', 'sets', 'textures', 'passes', 'marks', 'runs', 'items', 'hot', 'holes'];
        for (const k of Object.keys(doc)) if (!known.includes(k)) warn(`unknown key '${k}' (ignored)`);
        if (errors.length) return { list: null, errors, warnings };
        if (!sizeOk) return { list: null, errors: ['size'], warnings };
        return { list: doc, errors, warnings };
    }

    /* The draw calls in order: under then over, each pass's layers in the kit's order; empty layers are skipped, as the
       kit skips them. One call per layer (the kit draws a layer in one DrawPrimitiveUP). */
    function drawOrder(list, enabled) {
        const out = [];
        for (const pn of PASSES) {
            if (enabled && enabled.passes && enabled.passes[pn] === false) continue;
            const p = list.passes[pn];
            p.layers.forEach((l, li) => {
                if (enabled && enabled.layers && enabled.layers[l.name] === false) return;
                const n = l.v.length / PER_VERTEX;
                if (n) out.push({ pass: pn, layer: l.name, index: li, op: l.op, writeAlpha: p.writeAlpha, vertices: n });
            });
        }
        return out;
    }

    /* Validate an atlas manifest: {schema, privacy, image, size:[w,h], entries:{name:[x,y,w,h]}} */
    function parseAtlas(text) {
        let doc;
        try { doc = typeof text === 'string' ? JSON.parse(text) : text; }
        catch (e) { return { atlas: null, errors: ['not JSON: ' + e.message] }; }
        const errors = [];
        if (!doc || doc.schema !== ATLAS_SCHEMA) errors.push(`schema '${doc && doc.schema}': this replayer reads '${ATLAS_SCHEMA}'`);
        else {
            if (doc.privacy !== 'public' && doc.privacy !== MARKER) errors.push(`privacy '${doc.privacy}'`);
            if (!Array.isArray(doc.size) || doc.size.length !== 2 || !doc.size.every((n) => isInt(n) && n > 0)) errors.push('size: [w, h]');
            if (typeof doc.image !== 'string') errors.push('image: the PNG file name');
            if (!doc.entries || typeof doc.entries !== 'object') errors.push('entries: {name: [x, y, w, h]}');
            else for (const [k, r] of Object.entries(doc.entries)) {
                if (!TEX_NAME.test(k) || k === 'white') errors.push(`entries '${k}': glyph:<set>:U+XXXX or icon:<name>`);
                else if (!isRect(r) || (doc.size && (r[0] < 0 || r[1] < 0 || r[0] + r[2] > doc.size[0] || r[1] + r[3] > doc.size[1]))) errors.push(`entries '${k}': [x, y, w, h] inside the image`);
            }
        }
        return { atlas: errors.length ? null : doc, errors };
    }

    /* Per-vertex GPU data for one layer: positions (list pixels), colours (RGBA bytes), atlas UVs and the texture slot
       (0 = white, exactly 1.0; 1 = the atlas). A name the atlas lacks is drawn white (a solid box) and counted. */
    function layerBuffers(list, pass, li, atlas) {
        const l = list.passes[pass].layers[li];
        const v = l.v, n = v.length / PER_VERTEX;
        const pos = new Float32Array(n * 2), col = new Uint8Array(n * 4), uv = new Float32Array(n * 2), slot = new Float32Array(n);
        const missing = {};
        let mi = 0;
        for (const [first, count, ti] of l.spans) {
            const name = list.textures[ti];
            let rect = null;
            if (name !== 'white') {
                rect = atlas && atlas.entries[name];
                if (!rect) { missing[name] = (missing[name] || 0) + count / 3; mi += count / 3; }
            }
            for (let k = first; k < first + count; k++) {
                const o = k * PER_VERTEX;
                pos[k * 2] = v[o]; pos[k * 2 + 1] = v[o + 1];
                const c = argb(v[o + 2]);
                col.set(c, k * 4);
                if (rect) {
                    uv[k * 2] = (rect[0] + v[o + 3] * rect[2]) / atlas.size[0];
                    uv[k * 2 + 1] = (rect[1] + v[o + 4] * rect[3]) / atlas.size[1];
                    slot[k] = 1;
                } else slot[k] = 0;
            }
        }
        return { pos, col, uv, slot, vertices: n, missing, missingTris: mi };
    }

    /* Counts for the summary and the comparison */
    function summary(list) {
        const s = { size: list.size.slice(), privacy: list.privacy === MARKER ? 'private' : 'public', passes: {}, textures: list.textures.length,
            runs: (list.runs || []).length, hot: (list.hot || []).length, items: (list.items || []).length, holes: (list.holes || []).length, marks: (list.marks || []).length };
        for (const pn of PASSES) s.passes[pn] = list.passes[pn].layers.map((l) => l.v.length / PER_VERTEX);
        return s;
    }

    /* Structural differences between two lists (level 1's questions, loosely): vertex counts per layer, texture names
       per triangle, colours, positions over a tolerance. Returns lines; empty = equal within tol. */
    function compareLists(a, b, tol) {
        tol = tol === undefined ? 0.01 : tol;
        const out = [];
        if (a.size[0] !== b.size[0] || a.size[1] !== b.size[1]) out.push(`size ${a.size.join('x')} vs ${b.size.join('x')}`);
        for (const pn of PASSES) {
            LAYERS.forEach((name, li) => {
                const la = a.passes[pn].layers[li], lb = b.passes[pn].layers[li];
                const na = la.v.length / PER_VERTEX, nb = lb.v.length / PER_VERTEX;
                if (na !== nb) { out.push(`${pn}.${name}: ${na} vs ${nb} vertices`); return; }
                const ta = texPerVertex(a, la), tb = texPerVertex(b, lb);
                let pos = 0, col = 0, tex = 0, uv = 0, maxd = 0;
                for (let k = 0; k < na; k++) {
                    const o = k * PER_VERTEX;
                    const d = Math.max(Math.abs(la.v[o] - lb.v[o]), Math.abs(la.v[o + 1] - lb.v[o + 1]));
                    if (d > tol) { pos++; maxd = Math.max(maxd, d); }
                    if (la.v[o + 2] !== lb.v[o + 2]) col++;
                    if (Math.abs(la.v[o + 3] - lb.v[o + 3]) > 1e-6 || Math.abs(la.v[o + 4] - lb.v[o + 4]) > 1e-6) uv++;
                    if (ta[k] !== tb[k]) tex++;
                }
                if (pos) out.push(`${pn}.${name}: ${pos} vertices moved over ${tol} px (max ${maxd.toFixed(3)})`);
                if (col) out.push(`${pn}.${name}: ${col} vertex colours differ`);
                if (uv) out.push(`${pn}.${name}: ${uv} texture coordinates differ`);
                if (tex) out.push(`${pn}.${name}: ${tex} vertices name another texture`);
            });
        }
        const ra = (a.runs || []).map((r) => r.text).join('\u0001'), rb = (b.runs || []).map((r) => r.text).join('\u0001');
        if (ra !== rb) out.push('text runs differ');
        return out;
    }
    function texPerVertex(list, l) {
        const t = new Array(l.v.length / PER_VERTEX);
        for (const [first, count, ti] of l.spans) for (let k = first; k < first + count; k++) t[k] = list.textures[ti];
        return t;
    }

    /* Pixel difference of two RGBA buffers of the same size: count over threshold (any channel), max delta, bounding box,
       and (when out is given, RGBA) a view: differing pixels magenta, others A's pixel dimmed to a third. */
    function diffPixels(pa, pb, w, h, threshold, out) {
        threshold = threshold || 0;
        let count = 0, max = 0, x0 = w, y0 = h, x1 = -1, y1 = -1;
        for (let y = 0; y < h; y++) {
            for (let x = 0; x < w; x++) {
                const i = (y * w + x) * 4;
                const d = Math.max(Math.abs(pa[i] - pb[i]), Math.abs(pa[i + 1] - pb[i + 1]), Math.abs(pa[i + 2] - pb[i + 2]));
                if (d > max) max = d;
                const hit = d > threshold;
                if (hit) { count++; if (x < x0) x0 = x; if (y < y0) y0 = y; if (x > x1) x1 = x; if (y > y1) y1 = y; }
                if (out) {
                    if (hit) { out[i] = 255; out[i + 1] = 0; out[i + 2] = 255; }
                    else { out[i] = pa[i] / 3; out[i + 1] = pa[i + 1] / 3; out[i + 2] = pa[i + 2] / 3; }
                    out[i + 3] = 255;
                }
            }
        }
        return { count, max, box: count ? [x0, y0, x1 - x0 + 1, y1 - y0 + 1] : null };
    }

    /* list pixel -> clip space for a target of tw x th: D3D's pixel centres sit on integers, GL's on half-integers, so
       +0.5 (the kit already subtracted 0.5, hud2_kit.cpp Push); y down in the list, up in GL */
    function toClip(x, y, tw, th) { return [((x + 0.5) / tw) * 2 - 1, 1 - ((y + 0.5) / th) * 2]; }

    return { SCHEMA, ATLAS_SCHEMA, MARKER, PASSES, LAYERS, KIT_OPS, OPS, HOLE_KINDS, PER_VERTEX, TEX_NAME,
        argb, parse, validate, drawOrder, parseAtlas, layerBuffers, summary, compareLists, diffPixels, toClip };
});
