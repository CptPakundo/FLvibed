/* The replayer page's controls (index.html): files from the user's disk only, two list slots (A, B), an atlas, an
   optional background; the size, the pixel mapping and the zoom; A, B, A | B or the difference; passes, layers and
   overlays (holes as labelled hatched rectangles, run boxes and clip rooms, hot-spots, items); the inspector tables
   and the pointer readout. Drawing is gl.js, reading and checking fldl.js. */
(function () {
    'use strict';
    const $ = (id) => document.getElementById(id);
    const state = {
        lists: { A: null, B: null }, names: { A: '', B: '' }, warnings: { A: [], B: [] },
        atlas: null, atlasName: '', atlasImage: null, atlasImageName: '', pendingPngs: {},
        bgImage: null, bgName: '', hoverRun: -1, messages: [],
        slotAtlas: { A: '', B: '' }, slotPrivate: { A: false, B: false }, // (a list loaded from a folder: its folder's atlas)
    };
    const views = {};
    for (const k of ['A', 'B']) {
        const fig = $('view' + k);
        const gl = fig.querySelector('canvas.gl'), ov = fig.querySelector('canvas.ov');
        let renderer = null, error = '';
        try { renderer = new FLGL.Renderer(gl); } catch (e) { error = String(e.message || e); }
        views[k] = { fig, gl, ov, renderer, error, frame: fig.querySelector('.frame') };
    }
    const diffCanvas = $('viewD').querySelector('canvas.d2');
    const OVERLAYS = { holes: 'holes', runs: 'text runs', clips: 'clip rooms', hot: 'hot-spots', items: 'items' };
    const enabled = { passes: { under: true, over: true }, layers: {}, overlays: { holes: true, runs: false, clips: true, hot: false, items: false } };
    FLDL.LAYERS.forEach((l) => { enabled.layers[l] = true; });

    function checkboxes(container, label, obj, names) {
        const c = $(container);
        c.textContent = label + ' ';
        for (const [key, text] of Object.entries(names)) {
            const l = document.createElement('label');
            const i = document.createElement('input');
            i.type = 'checkbox'; i.checked = obj[key];
            i.addEventListener('change', () => { obj[key] = i.checked; render(); });
            l.append(i, ' ' + text);
            c.append(l);
        }
    }
    checkboxes('passes', 'Passes', enabled.passes, { under: 'under', over: 'over' });
    checkboxes('layers', 'Layers', enabled.layers, Object.fromEntries(FLDL.LAYERS.map((l) => [l, l])));
    checkboxes('overlays', 'Overlays', enabled.overlays, OVERLAYS);

    function radio(name) { return document.querySelector(`input[name=${name}]:checked`).value; }
    function say(kind, text) { state.messages.push({ kind, text }); }

    // ---------------------------------------------------------------- files (local only)
    async function loadFiles(files) {
        state.messages = [];
        const slot = radio('slot');
        const jsons = [], pngs = [];
        let listsSeen = 0;
        for (const f of files) (/\.png$/i.test(f.name) ? pngs : jsons).push(f);
        for (const f of jsons) {
            let doc;
            try { doc = JSON.parse(await f.text()); } catch (e) { say('err', `${f.name}: not JSON (${e.message})`); continue; }
            if (doc && doc.schema === FLDL.ATLAS_SCHEMA) {
                const r = FLDL.parseAtlas(doc);
                if (!r.atlas) { say('err', `${f.name}: atlas refused: ${r.errors.join('; ')}`); continue; }
                // a new manifest drops the old atlas at once: never draw one manifest's names from another's image
                state.atlas = r.atlas; state.atlasName = f.name; state.atlasImage = null; state.atlasImageName = '';
                state.slotAtlas = { A: '', B: '' }; // (a dropped atlas serves both slots again)
                for (const v of Object.values(views)) if (v.renderer) v.renderer.setAtlas(null, null);
                say('ok', `${f.name}: atlas manifest, ${Object.keys(r.atlas.entries).length} names; its image: ${r.atlas.image}`);
                continue;
            }
            const r = FLDL.validate(doc);
            // two lists dropped together with A chosen: the first into A, the second into B
            const into = slot === 'A' && listsSeen++ === 1 ? 'B' : slot;
            if (!r.list) {
                // the slot is emptied: the list that was there must not pass for the refused one
                state.lists[into] = null; state.names[into] = f.name + ' (refused)'; state.warnings[into] = [];
                say('err', `${f.name}: refused (${r.errors.length} error(s)), slot ${into} emptied: ${r.errors.slice(0, 8).join('; ')}`);
                continue;
            }
            state.lists[into] = r.list; state.names[into] = f.name; state.warnings[into] = r.warnings;
            say('ok', `${f.name}: list into ${into}, ${r.list.size.join('x')}, ${r.list.privacy === 'public' ? 'public' : 'PRIVATE'}`);
            for (const w of r.warnings) say('warn', `${f.name}: ${w}`);
        }
        for (const f of pngs) state.pendingPngs[f.name.toLowerCase()] = f;
        await attachAtlasImage();
        render();
    }
    async function attachAtlasImage() {
        if (!state.atlas) return;
        const want = state.atlas.image.toLowerCase().split(/[\\/]/).pop();
        let f = state.pendingPngs[want];
        const names = Object.keys(state.pendingPngs);
        if (!f && names.length === 1 && !state.atlasImage) f = state.pendingPngs[names[0]];
        if (!f) { if (!state.atlasImage) say('warn', `the atlas image ${state.atlas.image} is not loaded yet: drop it too (glyphs and icons draw as solid boxes until then)`); return; }
        try {
            const img = await createImageBitmap(f, { premultiplyAlpha: 'none', colorSpaceConversion: 'none' });
            for (const v of Object.values(views)) if (v.renderer) v.renderer.setAtlas(state.atlas, img);
            state.atlasImage = img; state.atlasImageName = f.name;
            delete state.pendingPngs[f.name.toLowerCase()];
            say('ok', `${f.name}: atlas image ${img.width}x${img.height}`);
        } catch (e) { say('err', `${f.name}: ${e.message || e}`); }
    }
    $('files').addEventListener('change', (e) => { loadFiles([...e.target.files]); e.target.value = ''; });
    const drop = $('drop');
    for (const ev of ['dragenter', 'dragover']) document.addEventListener(ev, (e) => { e.preventDefault(); drop.classList.add('over'); });
    for (const ev of ['dragleave', 'drop']) document.addEventListener(ev, (e) => { e.preventDefault(); drop.classList.remove('over'); });
    document.addEventListener('drop', (e) => { if (e.dataTransfer && e.dataTransfer.files.length) loadFiles([...e.dataTransfer.files]); });
    $('bgfile').addEventListener('change', async (e) => {
        const f = e.target.files[0];
        if (!f) return;
        try {
            state.bgImage = await createImageBitmap(f, { premultiplyAlpha: 'none', colorSpaceConversion: 'none' });
            state.bgName = f.name;
            for (const v of Object.values(views)) if (v.renderer) v.renderer.setBackground(state.bgImage);
        } catch (err) { say('err', `${f.name}: ${err.message || err}`); }
        e.target.value = '';
        render();
    });
    // ---------------------------------------------------------------- a folder of results (local only: the picker's files)
    // Every list in the folder picked, by sub-folder; A / B loads one with the atlas of its own folder (each bench run writes
    // one atlas for its lists: two runs' lists never share one), so a before / after pair can be flipped or put side by side.
    const folder = { dirs: {}, slotDir: { A: '', B: '' } };
    function dirOf(f) { const p = (f.webkitRelativePath || f.name).split('/'); p.pop(); return p.join('/'); }
    function listFolder() {
        const box = $('folderList');
        box.textContent = '';
        for (const d of Object.keys(folder.dirs).sort()) {
            const lists = folder.dirs[d].lists;
            if (!lists.length) continue;
            const h = document.createElement('div'); h.className = 'mono'; h.textContent = d + (folder.dirs[d].atlas ? '' : ' (no atlas)');
            box.append(h);
            for (const f of lists.slice().sort((a, b) => a.name.localeCompare(b.name))) {
                const row = document.createElement('div'); row.className = 'row';
                for (const k of ['A', 'B']) {
                    const b = document.createElement('button'); b.type = 'button'; b.textContent = k;
                    b.addEventListener('click', () => loadInto(k, d, f));
                    row.append(b);
                }
                const n = document.createElement('span'); n.className = 'mono'; n.textContent = ' ' + f.name;
                row.append(n);
                box.append(row);
            }
        }
        if (!box.childElementCount) box.textContent = 'no display list (.fldl.json) in this folder';
    }
    async function loadInto(k, d, f) {
        state.messages = [];
        let doc;
        try { doc = JSON.parse(await f.text()); } catch (e) { say('err', `${f.name}: not JSON (${e.message})`); render(); return; }
        const r = FLDL.validate(doc);
        if (!r.list) {
            state.lists[k] = null; state.names[k] = f.name + ' (refused)'; state.warnings[k] = [];
            say('err', `${f.name}: refused, slot ${k} emptied: ${r.errors.slice(0, 8).join('; ')}`);
            render(); return;
        }
        // the list's own folder's atlas into this slot's renderer (none: glyphs and icons as solid boxes, said)
        const dir = folder.dirs[d], v = views[k];
        let atlasName = '';
        if (dir.atlas && v.renderer) {
            try {
                if (!dir.decoded) { // (once per folder: both slots may draw from it)
                    const a = FLDL.parseAtlas(JSON.parse(await dir.atlas.text()));
                    if (!a.atlas) throw new Error(a.errors.join('; '));
                    const png = dir.pngs[a.atlas.image.toLowerCase().split(/[\\/]/).pop()];
                    if (!png) throw new Error(`its image ${a.atlas.image} is not in the folder`);
                    dir.decoded = { atlas: a.atlas, img: await createImageBitmap(png, { premultiplyAlpha: 'none', colorSpaceConversion: 'none' }) };
                }
                v.renderer.setAtlas(dir.decoded.atlas, dir.decoded.img);
                atlasName = d + '/' + dir.atlas.name;
                if (dir.decoded.atlas.privacy !== 'public') state.slotPrivate[k] = true;
            } catch (e) { v.renderer.setAtlas(null, null); say('err', `${d}: atlas not loaded (${e.message || e})`); }
        } else if (v.renderer) { v.renderer.setAtlas(null, null); say('warn', `${d}: no replay_atlas.json: glyphs and icons draw as solid boxes`); }
        state.lists[k] = r.list; state.names[k] = d + '/' + f.name; state.warnings[k] = r.warnings; state.slotAtlas[k] = atlasName;
        folder.slotDir[k] = d;
        say('ok', `${f.name}: list into ${k}, ${r.list.size.join('x')}${r.list.variant ? ', variant ' + r.list.variant : ''}${r.list.relayout ? ', drawn at another size than recorded' : ''}`);
        for (const w of r.warnings) say('warn', `${f.name}: ${w}`);
        render();
    }
    // entries: [{dir, file}] (the picker's: the file's folder from its relative path)
    function loadFolder(entries) {
        folder.dirs = {};
        for (const { dir: d, file: f } of entries) {
            const dir = folder.dirs[d] || (folder.dirs[d] = { lists: [], atlas: null, pngs: {} });
            if (/\.fldl\.json$/i.test(f.name)) dir.lists.push(f);
            else if (/_atlas\.json$/i.test(f.name)) dir.atlas = f;
            else if (/\.png$/i.test(f.name)) dir.pngs[f.name.toLowerCase()] = f;
        }
        listFolder();
    }
    $('folder').addEventListener('change', (e) => {
        loadFolder([...e.target.files].map((f) => ({ dir: dirOf(f), file: f })));
        e.target.value = '';
    });
    // flip: A shows B's list and back (from A | B or the difference: A first)
    function flip() {
        const v = radio('view');
        const to = v === 'A' ? 'B' : 'A';
        document.querySelector(`input[name=view][value=${to}]`).checked = true;
        render();
    }
    $('flip').addEventListener('click', flip);
    document.addEventListener('keydown', (e) => {
        if ((e.key === 'f' || e.key === 'F') && !e.ctrlKey && !e.altKey && !e.metaKey && !(e.target && /^(INPUT|SELECT|TEXTAREA)$/.test(e.target.tagName) && e.target.type !== 'radio' && e.target.type !== 'checkbox')) flip();
    });
    $('clearBg').addEventListener('click', () => { state.bgImage = null; state.bgName = ''; for (const v of Object.values(views)) if (v.renderer) v.renderer.setBackground(null); render(); });
    $('clearB').addEventListener('click', () => { state.lists.B = null; state.names.B = ''; render(); });
    for (const id of ['size', 'cw', 'ch', 'bgcolor', 'thresh']) $(id).addEventListener('change', render);
    for (const n of ['map', 'zoom', 'view']) document.querySelectorAll(`input[name=${n}]`).forEach((i) => i.addEventListener('change', render));
    window.addEventListener('resize', () => layout());

    // ---------------------------------------------------------------- drawing
    function target() {
        const v = $('size').value;
        const first = state.lists.A || state.lists.B;
        if (v === 'list') {
            const s = first ? first.size.slice() : [1280, 800];
            $('cw').value = s[0]; $('ch').value = s[1];
            return s;
        }
        if (v === 'custom') return [clampSide($('cw').value), clampSide($('ch').value)];
        const [w, h] = v.split('x').map(Number);
        $('cw').value = w; $('ch').value = h;
        return [w, h];
    }
    function clampSide(x) { return Math.max(16, Math.min(16384, Math.round(Number(x) || 0))); }
    function bgColour() { const c = $('bgcolor').value; return [1, 3, 5].map((i) => parseInt(c.slice(i, i + 2), 16)); }

    let last = { W: 0, H: 0 };
    function render() {
        const [W, H] = target();
        last = { W, H };
        const view = radio('view'), stretch = radio('map') === 'stretch';
        const show = { A: view === 'A' || view === 'AB', B: view === 'B' || view === 'AB', D: view === 'diff' };
        const opts = { background: bgColour(), stretch, enabled };
        const drawn = {};
        for (const k of ['A', 'B']) {
            const v = views[k];
            const need = show[k] || show.D;
            v.fig.hidden = !show[k];
            if (!v.renderer) { if (show[k]) say('err', `${k}: ${v.error}`); continue; }
            if (!need) continue;
            if (state.lists[k]) drawn[k] = v.renderer.draw(state.lists[k], W, H, opts);
            else v.renderer.draw({ size: [W, H], passes: { under: { writeAlpha: true, layers: [] }, over: { writeAlpha: true, layers: [] } } }, W, H, Object.assign({}, opts, { enabled: { passes: { under: false, over: false } } }));
            v.ov.width = W; v.ov.height = H;
            overlay(v.ov, state.lists[k], W, H, stretch);
        }
        $('viewD').hidden = !show.D;
        const msgs = [];
        if (show.D) {
            diffCanvas.width = W; diffCanvas.height = H;
            if (state.lists.A && state.lists.B && views.A.renderer && views.B.renderer) {
                const pa = views.A.renderer.pixels(), pb = views.B.renderer.pixels();
                const img = new ImageData(W, H);
                const d = FLDL.diffPixels(pa, pb, W, H, Number($('thresh').value) || 0, img.data);
                diffCanvas.getContext('2d').putImageData(img, 0, 0);
                msgs.push({ kind: d.count ? 'warn' : 'ok', text: `difference: ${d.count} of ${W * H} pixels over ${$('thresh').value} levels (max ${d.max})${d.box ? ', within ' + d.box.join(', ') + ' (x, y, w, h)' : ''}` });
                const lines = FLDL.compareLists(state.lists.A, state.lists.B);
                msgs.push({ kind: lines.length ? 'warn' : 'ok', text: 'lists: ' + (lines.length ? lines.join('; ') : 'equal (positions within 0.01 px)') });
            } else msgs.push({ kind: 'warn', text: 'the difference needs a list in A and one in B' });
        }
        for (const k of ['A', 'B']) {
            const l = state.lists[k];
            if (!l || !(show[k] || show.D)) continue;
            if ((l.size[0] !== W || l.size[1] !== H) && !stretch) msgs.push({ kind: 'warn', text: `${k} was written for ${l.size.join('x')}, drawn 1:1 on ${W}x${H}: another size is a re-layout by the bench, not a replay` });
            if (stretch) msgs.push({ kind: 'warn', text: `${k} stretched from ${l.size.join('x')} to ${W}x${H}: a preview, not the game's pixels` });
            const m = drawn[k] && drawn[k].missing;
            if (m && Object.keys(m).length) msgs.push({ kind: 'warn', text: `${k}: ${Object.keys(m).length} texture name(s) not in the atlas, drawn as solid boxes: ${Object.keys(m).slice(0, 12).join(', ')}` });
        }
        const priv = ['A', 'B'].some((k) => (state.lists[k] && state.lists[k].privacy !== 'public') || state.slotPrivate[k]) ||
            (state.atlas && state.atlas.privacy !== 'public');
        $('badge').hidden = !priv;
        $('badge').textContent = 'PRIVATE: never publish this page\'s content';
        const slotAtlas = ['A', 'B'].filter((k) => state.slotAtlas[k]).map((k) => `${k}'s atlas: ${state.slotAtlas[k]}`);
        $('loaded').textContent = [`A: ${state.names.A || '-'}`, `B: ${state.names.B || '-'}`, ...slotAtlas,
            `atlas: ${state.atlasName ? state.atlasName + ' + ' + (state.atlasImageName || 'no image yet') : '-'}`,
            `background: ${state.bgName || 'colour'}`].join(' | ');
        const box = $('messages');
        box.textContent = '';
        for (const m of state.messages.concat(msgs)) { const d = document.createElement('div'); d.className = m.kind; d.textContent = m.text; box.append(d); }
        inspect();
        layout();
    }

    /* the frames' CSS size: Fit to the column, or zoom x list pixels (devicePixelRatio taken out) */
    function layout() {
        const z = radio('zoom'), { W, H } = last;
        const dpr = window.devicePixelRatio || 1;
        const visible = ['A', 'B'].filter((k) => !views[k].fig.hidden).length + ($('viewD').hidden ? 0 : 1);
        const avail = Math.max(200, ($('views').clientWidth - 12 * visible) / Math.max(1, visible) - 16);
        const cssW = z === 'fit' ? Math.min(avail, W / dpr) : W * Number(z) / dpr;
        const cssH = cssW * H / W;
        for (const c of document.querySelectorAll('#views canvas')) { c.style.width = cssW + 'px'; c.style.height = cssH + 'px'; }
        for (const f of document.querySelectorAll('.frame')) f.classList.toggle('zoomed', z !== 'fit' && Number(z) >= 1);
    }

    function overlay(cv, list, W, H, stretch) {
        const g = cv.getContext('2d');
        g.clearRect(0, 0, W, H);
        if (!list) return;
        const sx = stretch ? W / list.size[0] : 1, sy = stretch ? H / list.size[1] : 1;
        const R = (r) => [r[0] * sx, r[1] * sy, r[2] * sx, r[3] * sy];
        const lw = Math.max(1, Math.round(H / 600));
        g.lineWidth = lw;
        g.font = `${Math.max(10, Math.round(H / 70))}px system-ui, sans-serif`;
        g.textBaseline = 'top';
        if (enabled.overlays.holes) for (const h of list.holes || []) {
            const [x, y, w, hh] = R(h.rect);
            g.save();
            g.beginPath(); g.rect(x, y, w, hh); g.clip();
            g.strokeStyle = h.shown ? 'rgba(255,200,0,0.55)' : 'rgba(160,160,160,0.4)';
            const step = Math.max(6, Math.round(H / 90));
            for (let t = -hh; t < w; t += step) { g.beginPath(); g.moveTo(x + t, y + hh); g.lineTo(x + t + hh, y); g.stroke(); }
            g.restore();
            g.strokeStyle = h.shown ? '#ffc800' : '#a0a0a0';
            g.setLineDash(h.shown ? [] : [4 * lw, 3 * lw]);
            g.strokeRect(x, y, w, hh);
            g.setLineDash([]);
            label(g, h.kind + (h.shown ? '' : ' (hidden)'), x + 3 * lw, y + 3 * lw, h.shown ? '#ffc800' : '#a0a0a0');
        }
        if (enabled.overlays.clips) for (const r of list.runs || []) if (r.clip) {
            const [x, y, w, h] = R(r.clip);
            g.strokeStyle = '#00e0ff'; g.setLineDash([3 * lw, 2 * lw]); g.strokeRect(x, y, w, h); g.setLineDash([]);
        }
        if (enabled.overlays.runs) (list.runs || []).forEach((r, i) => {
            const b = runBox(list, r);
            if (!b) return;
            const [x, y, w, h] = R(b);
            g.strokeStyle = i === state.hoverRun ? '#ff40ff' : 'rgba(255,64,255,0.6)';
            g.strokeRect(x, y, w, h);
        });
        if (enabled.overlays.hot) for (const h of list.hot || []) { const [x, y, w, hh] = R(h.rect); g.strokeStyle = '#40ff60'; g.strokeRect(x, y, w, hh); label(g, h.name, x + 2, y + hh + 2, '#40ff60'); }
        if (enabled.overlays.items) for (const it of list.items || []) { const [x, y, w, h] = R(it.rect); g.strokeStyle = 'rgba(120,170,255,0.8)'; g.strokeRect(x, y, w, h); }
    }
    function label(g, text, x, y, colour) {
        const m = g.measureText(text);
        const h = parseInt(g.font, 10) + 4;
        g.fillStyle = 'rgba(0,0,0,0.65)'; g.fillRect(x - 2, y - 2, m.width + 4, h);
        g.fillStyle = colour; g.fillText(text, x, y);
    }
    /* a run's box: the bounding box of its vertices */
    function runBox(list, r) {
        const l = list.passes[r.pass].layers[FLDL.LAYERS.indexOf(r.layer)];
        let x0 = Infinity, y0 = Infinity, x1 = -Infinity, y1 = -Infinity;
        for (let k = r.first; k < r.first + r.count; k++) {
            const x = l.v[k * 5] + 0.5, y = l.v[k * 5 + 1] + 0.5;
            x0 = Math.min(x0, x); y0 = Math.min(y0, y); x1 = Math.max(x1, x); y1 = Math.max(y1, y);
        }
        return Number.isFinite(x0) ? [x0, y0, x1 - x0, y1 - y0] : null;
    }

    // ---------------------------------------------------------------- inspector
    function hex(c) { return '#' + (c & 0xFFFFFF).toString(16).padStart(6, '0'); }
    function colourCell(c) { const [r, g, b, a] = FLDL.argb(c); return `<span class="swatch" style="background:rgb(${r},${g},${b})"></span>${hex(c)} a${a}`; }
    function esc(s) { return String(s).replace(/[&<>"]/g, (ch) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[ch])); }
    function f1(x) { return typeof x === 'number' ? (Math.round(x * 100) / 100).toString() : esc(x); }
    function table(head, rows) {
        return `<table><tr>${head.map((h) => `<th>${esc(h)}</th>`).join('')}</tr>${rows.join('')}</table>`;
    }
    function inspect() {
        const out = [];
        for (const k of ['A', 'B']) {
            const l = state.lists[k];
            if (!l) continue;
            const s = FLDL.summary(l);
            out.push(`<h3>${k}: ${esc(state.names[k])} (${s.size.join('x')}, ${s.privacy}, state ${esc(l.state || '-')}, variant ${esc(l.variant || '-')}${l.relayout ? ', a re-layout' : ''})</h3>`);
            out.push(table(['pass', ...FLDL.LAYERS], FLDL.PASSES.map((p) => `<tr><td>${p}${l.passes[p].writeAlpha ? '' : ' (rgb only)'}</td>${s.passes[p].map((n) => `<td>${n / 3} tri</td>`).join('')}</tr>`)));
            if ((l.marks || []).length) out.push(table(['mark', 'pass', ...FLDL.LAYERS], l.marks.map((m) => `<tr><td>${esc(m.name)}</td><td>${m.pass}</td>${m.counts.map((c) => `<td>${c / 3}</td>`).join('')}</tr>`)));
            if (k === 'A' || !state.lists.A) {
                out.push(`<h3>Text runs (${(l.runs || []).length})</h3>`);
                out.push(table(['#', 'text', 'tag', 'layer', 'set', 'x', 'y', 'px', 'align', 'clip', 'colour'], (l.runs || []).map((r, i) =>
                    `<tr data-run="${i}"${i === state.hoverRun ? ' class="hl"' : ''}><td>${i}</td><td>${esc(r.text)}</td><td>${esc(r.tag || '')}</td><td>${r.pass}.${r.layer}</td><td>${esc(r.set || '')}</td><td>${f1(r.x)}</td><td>${f1(r.y)}</td><td>${f1(r.px)}</td><td>${esc(r.align || '')}</td><td>${r.clip ? r.clip.map(f1).join(', ') : '-'}</td><td>${typeof r.color === 'number' ? colourCell(r.color) : '-'}</td></tr>`)));
                out.push(`<h3>Hot-spots (${(l.hot || []).length})</h3>`);
                out.push(table(['name', 'x, y, w, h'], (l.hot || []).map((h) => `<tr><td>${esc(h.name)}</td><td>${h.rect.map(f1).join(', ')}</td></tr>`)));
                out.push(`<h3>Holes (${(l.holes || []).length}): drawn by the game, not by the list</h3>`);
                out.push(table(['kind', 'shown', 'x, y, w, h'], (l.holes || []).map((h) => `<tr><td>${esc(h.kind)}</td><td>${esc(h.shown)}</td><td>${h.rect.map(f1).join(', ')}</td></tr>`)));
                out.push(`<h3>Items (${(l.items || []).length})</h3>`);
                out.push(table(['tag', 'x, y, w, h'], (l.items || []).map((h) => `<tr><td>${esc(h.tag)}</td><td>${h.rect.map(f1).join(', ')}</td></tr>`)));
            }
        }
        if (!out.length) out.push('<p class="small">No list loaded. The synthetic fixture: <code>node tools\\hudbench\\synth_fixture.js</code> writes lists and an atlas to <code>build\\bench\\synthetic\\</code>.</p>');
        $('inspect').innerHTML = out.join('');
    }

    // ---------------------------------------------------------------- pointer
    function onMove(k, e) {
        const v = views[k], l = state.lists[k];
        const rect = v.gl.getBoundingClientRect();
        const { W, H } = last;
        const tx = Math.floor((e.clientX - rect.left) / rect.width * W), ty = Math.floor((e.clientY - rect.top) / rect.height * H);
        if (tx < 0 || ty < 0 || tx >= W || ty >= H) return;
        const stretch = radio('map') === 'stretch';
        const lx = l && stretch ? tx * l.size[0] / W : tx, ly = l && stretch ? ty * l.size[1] / H : ty;
        const px = v.renderer ? v.renderer.pixelAt(tx, ty) : new Uint8Array(4);
        const inside = (r) => lx >= r[0] && ly >= r[1] && lx < r[0] + r[2] && ly < r[1] + r[3];
        const hits = [];
        let run = -1;
        if (l) {
            (l.runs || []).forEach((r, i) => { const b = runBox(l, r); if (b && inside(b)) { hits.push(`run ${i} '${r.text}' ${r.tag || ''}`); run = i; } });
            for (const h of l.hot || []) if (inside(h.rect)) hits.push('hot ' + h.name);
            for (const h of l.holes || []) if (inside(h.rect)) hits.push('hole ' + h.kind);
            for (const it of l.items || []) if (inside(it.rect)) hits.push('item ' + it.tag);
        }
        $('hover').textContent = `${k} pixel ${tx}, ${ty}${stretch && l ? ` (list ${lx.toFixed(1)}, ${ly.toFixed(1)})` : ''}  rgb ${px[0]} ${px[1]} ${px[2]}\n${hits.join('\n') || '-'}`;
        if (run !== state.hoverRun) {
            state.hoverRun = run;
            if (enabled.overlays.runs) overlay(v.ov, l, W, H, stretch);
            document.querySelectorAll('#inspect tr[data-run]').forEach((tr) => tr.classList.toggle('hl', Number(tr.dataset.run) === run));
        }
    }
    // (pointer events carry fractional CSS coordinates: at 100 % on a scaled display every list pixel can be pointed at)
    for (const k of ['A', 'B']) views[k].gl.addEventListener('pointermove', (e) => onMove(k, e));

    window.FLREPLAY = { loadFiles, loadFolder, loadInto, flip, folder, render, state };   // (for a test driving the page)
    render();
    document.documentElement.dataset.ready = views.A.renderer ? 'webgl' : 'no-webgl';
})();
