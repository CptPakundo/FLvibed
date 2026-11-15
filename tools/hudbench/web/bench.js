/* The design bench's window (bench.html): asks the bench program (`flbench serve`, the same machine) for the recorded
   states, a record's settings and the layout numbers, and for a draw whenever a control changes: the recorded state
   unchanged (Before) and with the changes (After), drawn here with the replayer's renderer (gl.js) and the program's
   atlas. Saved variants are files of the program's (research\experience\bench\variants\). A ping every 3 s tells the
   program the window is still open; closing it says goodbye (the program ends then, or 15 s after the last ping). */
(function () {
    'use strict';
    const $ = (id) => document.getElementById(id);
    const S = {
        records: [], groups: {}, run: '', key: '', input: 'pad', screen: '1280x800',
        settings: [], base: {}, set: {},          // the record's settings, their recorded values, the changes (name -> value)
        layout: [], layBase: {}, lay: {},         // the layout numbers, their values, the changes
        seq: 0, shown: 0, atlasGen: -1, atlas: null, atlasImg: null, last: null,
    };
    const views = {};
    for (const k of ['A', 'B']) {
        const fig = $('view' + k);
        views[k] = { fig, gl: fig.querySelector('canvas.gl'), ov: fig.querySelector('canvas.ov'), r: null };
        try { views[k].r = new FLGL.Renderer(views[k].gl); } catch (e) { say(`This window cannot draw: ${e.message || e}`); }
    }
    const diffCanvas = $('viewD').querySelector('canvas.d2');
    const enabled = { passes: { under: true, over: true }, layers: {} };
    FLDL.LAYERS.forEach((l) => { enabled.layers[l] = true; });

    function say(text) { $('report').innerHTML = `<div class="err">${esc(text)}</div>`; }
    function esc(s) { return String(s).replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c])); }
    async function get(url) { const r = await fetch(url); if (!r.ok) throw new Error(`${url}: ${r.status}`); return r; }

    // ---------------------------------------------------------------- the recorded states, in plain words
    const WORDS = { ap: 'autopilot', apwait: 'autopilot waiting', alertlist: 'alert list', hud: 'HUD', kbm: 'keyboard and mouse',
        nav: 'nav', cruise: 'cruising', combat: 'combat', dock: 'docking', live: 'live', second: 'second', target: 'target' };
    function plain(name) {
        return name.split('_').map((w) => WORDS[w] || w).join(' ').replace(/^./, (c) => c.toUpperCase());
    }
    function splitInput(state) {
        const m = /^(pad|kbm)_(.*)$/.exec(state);
        if (m) return { input: m[1], base: m[2] };
        const p = /^(.*?)_?(pad|kbm)$/.exec(state);
        if (p && p[1]) return { input: p[2], base: p[1] };
        return { input: 'pad', base: state };
    }
    function when(t) { return t ? new Date(t * 1000).toLocaleString([], { day: 'numeric', month: 'short', hour: '2-digit', minute: '2-digit' }) : ''; }
    async function loadRecords() {
        S.records = await (await get('/api/records')).json();
        // runs newest first; in a run one entry per state and frame: its pad / keyboard forms at each size, how crowded
        const runs = {};
        for (const r of S.records) {
            const run = runs[r.run] || (runs[r.run] = { name: r.run, time: r.runTime, entries: {} });
            const { input, base } = splitInput(r.state);
            const key = base + (r.frame ? ` (frame ${r.frame + 1})` : '');
            const e = run.entries[key] || (run.entries[key] = { key, base, frame: r.frame, crowd: 0, forms: {} });
            (e.forms[input] || (e.forms[input] = {}))[r.size] = r.rel;
            e.crowd = Math.max(e.crowd, r.crowd);
        }
        S.groups = runs;
        // (full recordings, 10 states or more, before the small check runs; each newest first)
        const full = (r) => (Object.keys(r.entries).length >= 10 ? 1 : 0);
        const order = Object.values(runs).sort((a, b) => full(b) - full(a) || b.time - a.time || a.name.localeCompare(b.name));
        const sel = $('run');
        sel.textContent = '';
        for (const run of order) {
            const o = document.createElement('option');
            o.value = run.name;
            o.textContent = `${run.name.replace(/_/g, ' ')}${run.time ? ', ' + when(run.time) : ''} (${Object.keys(run.entries).length} states)`;
            sel.append(o);
        }
        if (!order.length) { say('No recorded states found under research\\experience\\bench\\record or tests\\bench.'); return; }
        S.run = order[0].name;
        sel.value = S.run;
        listStates();
    }
    function listStates() {
        const run = S.groups[S.run];
        const box = $('states');
        box.textContent = '';
        const entries = Object.values(run.entries).sort((a, b) => b.crowd - a.crowd || a.key.localeCompare(b.key));
        for (const e of entries) {
            const b = document.createElement('button');
            b.type = 'button';
            b.className = e.key === S.key ? 'on' : '';
            const forms = Object.keys(e.forms).map((f) => (f === 'pad' ? 'pad' : 'keyboard')).join(' + ');
            const sizes = [...new Set(Object.values(e.forms).flatMap((m) => Object.keys(m)))].join(', ');
            b.innerHTML = `${esc(plain(e.base))}${e.frame ? ' <span class="sub">frame ' + (e.frame + 1) + '</span>' : ''} <span class="sub">${esc(forms)}; ${esc(sizes)}</span>`;
            b.addEventListener('click', () => { S.key = e.key; listStates(); pickRecord(); });
            box.append(b);
        }
        if (!S.key || !run.entries[S.key]) { S.key = entries[0].key; listStates(); pickRecord(); }
    }
    $('run').addEventListener('change', () => { S.run = $('run').value; S.key = ''; listStates(); });

    // the record of the entry for the input and screen chosen (the recording of that size when there is one)
    function current() {
        const e = S.groups[S.run] && S.groups[S.run].entries[S.key];
        if (!e) return null;
        const input = e.forms[S.input] ? S.input : Object.keys(e.forms)[0];
        const sizes = e.forms[input];
        if (!S.screenChosen) S.screen = sizes['1280x800'] ? '1280x800' : Object.keys(sizes)[0]; // (the recorded size until one is picked)
        const rel = sizes[S.screen] || sizes['1280x800'] || Object.values(sizes)[0];
        const recorded = Object.keys(sizes).find((s) => sizes[s] === rel);
        return { e, input, rel, recorded };
    }
    async function pickRecord() {
        const c = current();
        if (!c) return;
        for (const i of document.querySelectorAll('input[name=input]')) { i.disabled = !c.e.forms[i.value]; i.checked = i.value === c.input; }
        S.input = c.input;
        try {
            S.settings = await (await get('/api/settings?rec=' + encodeURIComponent(c.rel))).json();
        } catch (e) { say(`The record's settings did not load: ${e.message}`); return; }
        S.base = {};
        for (const s of S.settings) S.base[s.name] = s.value;
        for (const n of Object.keys(S.set)) if (!(n in S.base)) delete S.set[n]; // (a change this record cannot take)
        listSettings();
        syncTop();
        draw();
    }

    // ---------------------------------------------------------------- the picture's own controls (settings by name)
    function syncTop() {
        const v = (n) => (n in S.set ? S.set[n] : S.base[n]);
        for (const i of document.querySelectorAll('input[name=hudsize]')) i.checked = String(v('hud_scale')) === i.value;
        for (const i of document.querySelectorAll('input[name=preset]')) i.checked = String(v('hud_preset')) === i.value;
        for (const i of document.querySelectorAll('input[name=screen]')) i.checked = i.value === S.screen;
    }
    function setSetting(name, value) {
        if (String(value) === String(S.base[name])) delete S.set[name]; else S.set[name] = String(value);
        listSettings(); syncTop(); draw();
    }
    document.querySelectorAll('input[name=hudsize]').forEach((i) => i.addEventListener('change', () => setSetting('hud_scale', i.value)));
    document.querySelectorAll('input[name=preset]').forEach((i) => i.addEventListener('change', () => setSetting('hud_preset', i.value)));
    document.querySelectorAll('input[name=input]').forEach((i) => i.addEventListener('change', () => { S.input = i.value; pickRecord(); }));
    document.querySelectorAll('input[name=screen]').forEach((i) => i.addEventListener('change', () => { S.screen = i.value; S.screenChosen = true; pickRecord(); }));

    // ---------------------------------------------------------------- the settings list (searchable)
    function numberControl(value, step, onSet, min, max) {
        const box = document.createElement('span'); box.className = 'ctl';
        const inp = document.createElement('input'); inp.type = 'number'; inp.value = value; inp.step = step || 'any';
        if (min !== undefined) inp.min = min;
        if (max !== undefined) inp.max = max;
        const bump = (d) => {
            const s = Number(step) || 1, x = Number(inp.value) || 0;
            const n = Math.round((x + d * s) / s) * s;
            inp.value = String(Number(n.toFixed(6)));
            onSet(inp.value);
        };
        const minus = document.createElement('button'); minus.type = 'button'; minus.textContent = '−'; minus.title = 'smaller';
        const plus = document.createElement('button'); plus.type = 'button'; plus.textContent = '+'; plus.title = 'larger';
        minus.addEventListener('click', () => bump(-1));
        plus.addEventListener('click', () => bump(1));
        inp.addEventListener('change', () => { if (inp.value !== '' && isFinite(Number(inp.value))) onSet(inp.value); });
        box.append(minus, inp, plus);
        return box;
    }
    function row(list, name, sub, changed, control, onReset) {
        const d = document.createElement('div'); d.className = 'num' + (changed ? ' changed' : '');
        const n = document.createElement('div'); n.className = 'name'; n.innerHTML = `${esc(name)}${sub ? '<small>' + esc(sub) + '</small>' : ''}`;
        const r = document.createElement('button'); r.type = 'button'; r.className = 'reset'; r.textContent = 'Reset'; r.title = 'back to the recorded value';
        r.addEventListener('click', onReset);
        d.append(n, control, r);
        list.append(d);
    }
    function listSettings() {
        const box = $('settings'), q = $('setFind').value.trim().toLowerCase();
        box.textContent = '';
        let shown = 0;
        const list = S.settings.slice().sort((a, b) => ((b.name in S.set) - (a.name in S.set)) || (!!b.label - !!a.label) || a.name.localeCompare(b.name));
        for (const s of list) {
            if (q && !(s.name.toLowerCase().includes(q) || (s.label || '').toLowerCase().includes(q))) continue;
            if (++shown > 150) break;
            const value = s.name in S.set ? S.set[s.name] : s.value;
            let ctl;
            if (s.kind === 'bool') {
                ctl = document.createElement('span'); ctl.className = 'ctl';
                const c = document.createElement('input'); c.type = 'checkbox'; c.checked = value === '1';
                c.addEventListener('change', () => setSetting(s.name, c.checked ? '1' : '0'));
                ctl.append(c, ' on');
            } else if (s.options) {
                ctl = document.createElement('span'); ctl.className = 'ctl';
                const sel = document.createElement('select');
                for (const [v, w] of s.options) { const o = document.createElement('option'); o.value = v; o.textContent = w; sel.append(o); }
                if (!s.options.some(([v]) => v === value)) { const o = document.createElement('option'); o.value = value; o.textContent = value; sel.append(o); }
                sel.value = value;
                sel.addEventListener('change', () => setSetting(s.name, sel.value));
                ctl.append(sel);
            } else if (s.kind === 'text') {
                ctl = document.createElement('span'); ctl.className = 'ctl';
                const t = document.createElement('input'); t.type = 'text'; t.value = value; t.size = 12;
                t.addEventListener('change', () => setSetting(s.name, t.value));
                ctl.append(t);
            } else ctl = numberControl(value, s.step || (s.kind === 'float' ? 0.01 : 1), (v) => setSetting(s.name, v), s.min, s.max);
            row(box, s.label || s.name, s.label ? s.name : (s.copies > 1 ? `${s.copies} copies` : ''), s.name in S.set, ctl, () => setSetting(s.name, s.value));
        }
        if (shown > 150) box.append(Object.assign(document.createElement('div'), { className: 'small', textContent: 'More: narrow the search.' }));
        const n = Object.keys(S.set).length;
        $('setCount').textContent = `${S.settings.length}${n ? ', ' + n + ' changed' : ''}`;
    }
    $('setFind').addEventListener('input', listSettings);
    $('setReset').addEventListener('click', () => { S.set = {}; listSettings(); syncTop(); draw(); });

    // ---------------------------------------------------------------- the layout numbers
    async function loadLayout() {
        S.layout = await (await get('/api/layout')).json();
        S.layBase = {};
        for (const [n, v] of S.layout) S.layBase[n] = v;
        listLayout();
    }
    function setLay(name, value) {
        if (Number(value) === Number(S.layBase[name])) delete S.lay[name]; else S.lay[name] = String(value);
        listLayout(); draw();
    }
    function listLayout() {
        const box = $('layout'), q = $('layFind').value.trim().toLowerCase();
        box.textContent = '';
        let shown = 0;
        const list = S.layout.slice().sort((a, b) => ((b[0] in S.lay) - (a[0] in S.lay)));
        for (const [n, v] of list) {
            if (q && !n.toLowerCase().includes(q)) continue;
            if (++shown > 150) break;
            const value = n in S.lay ? S.lay[n] : v;
            const step = Math.abs(Number(v)) >= 2 ? 1 : 0.005;
            const parts = n.split('.');
            row(box, `${parts[0]}: ${parts.slice(1).join(' ')}`, `was ${v}`, n in S.lay, numberControl(value, step, (x) => setLay(n, x)), () => setLay(n, v));
        }
        if (shown > 150) box.append(Object.assign(document.createElement('div'), { className: 'small', textContent: 'More: narrow the search.' }));
        const c = Object.keys(S.lay).length;
        $('layCount').textContent = `${S.layout.length}${c ? ', ' + c + ' changed' : ''}`;
    }
    $('layFind').addEventListener('input', listLayout);
    $('layReset').addEventListener('click', () => { S.lay = {}; listLayout(); draw(); });

    // ---------------------------------------------------------------- a draw (the program answers in a fraction of a second)
    function askText() {
        const c = current();
        if (!c) return '';
        const lines = [`rec ${c.rel}`];
        if (S.screen !== c.recorded) lines.push(`size ${S.screen}`);
        for (const [n, v] of Object.entries(S.set)) lines.push(`set ${n}=${v}`);
        for (const [n, v] of Object.entries(S.lay)) lines.push(`layout ${n}=${v}`);
        return lines.join('\n') + '\n';
    }
    let timer = 0;
    function draw() { clearTimeout(timer); timer = setTimeout(drawNow, 60); }
    async function drawNow() {
        const body = askText();
        if (!body) return;
        const seq = ++S.seq;
        $('status').textContent = 'drawing...';
        let res;
        try { res = await (await fetch('/api/draw', { method: 'POST', body })).json(); }
        catch (e) { if (seq === S.seq) { $('status').textContent = ''; say(`The bench program did not answer: ${e.message || e}`); } return; }
        if (seq !== S.seq) return; // (a newer change is on its way)
        if (!res.ok) { $('status').textContent = ''; say(`Not drawn: ${res.error}`); return; }
        if (res.atlas !== S.atlasGen) {
            const [m, png] = await Promise.all([get('/replay_atlas.json?g=' + res.atlas).then((r) => r.text()), get('/replay_atlas.png?g=' + res.atlas).then((r) => r.blob())]);
            const a = FLDL.parseAtlas(m);
            if (!a.atlas) { say('The picture sheet did not read: ' + a.errors.join('; ')); return; }
            const img = await createImageBitmap(png, { premultiplyAlpha: 'none', colorSpaceConversion: 'none' });
            for (const v of Object.values(views)) if (v.r) v.r.setAtlas(a.atlas, img);
            S.atlasGen = res.atlas;
        }
        const before = FLDL.validate(res.before).list, after = res.after ? FLDL.validate(res.after).list : before;
        S.last = { res, before, after };
        $('status').textContent = `drawn in ${res.ms} ms`;
        render();
        report();
    }

    function report() {
        const { res } = S.last;
        const c = current();
        const changes = Object.keys(S.set).length + Object.keys(S.lay).length;
        let h = `<div class="verdict">${esc(plain(c.e.base))}, ${c.input === 'pad' ? 'pad' : 'keyboard and mouse'}, ${res.w} x ${res.h}: the unchanged frame is ${esc(res.verdict)}.</div>`;
        if (changes) {
            h += '<div>Changed:</div><ul>' + res.lines.map((l) => `<li>${esc(l)}</li>`).join('') + '</ul>';
            h += '<div>What moved (before against after):</div><ul>' + res.diff.map((l) => `<li>${esc(l)}</li>`).join('') + '</ul>';
        } else if (res.lines.length) h += '<ul>' + res.lines.map((l) => `<li>${esc(l)}</li>`).join('') + '</ul>';
        else h += '<div class="small">Nothing changed yet: use the controls on the left. Before and After are the same picture until then.</div>';
        if (res.notes.length) h += '<details><summary>What cannot be true of this picture, in full</summary><ul>' + res.notes.map((l) => `<li>${esc(l)}</li>`).join('') + '</ul></details>';
        $('report').innerHTML = h;
        let t = 'Hatched boxes are the game’s 3D views (your ship, the target): the bench does not draw them.';
        if (res.relayout) t += ` This state was recorded at ${res.recorded} and is drawn at ${res.w} x ${res.h}: like the first frame after a resize, not what the game settles to.`;
        $('truth').textContent = t;
    }

    function radio(name) { return document.querySelector(`input[name=${name}]:checked`).value; }
    function render() {
        if (!S.last) return;
        const { res, before, after } = S.last;
        const W = res.w, H = res.h, view = radio('view');
        const show = { A: view === 'A' || view === 'AB', B: view === 'B' || view === 'AB', D: view === 'diff' };
        const lists = { A: before, B: after };
        for (const k of ['A', 'B']) {
            const v = views[k];
            v.fig.hidden = !show[k];
            if (!v.r || !(show[k] || show.D)) continue;
            try { v.r.draw(lists[k], W, H, { background: [0, 0, 0], stretch: false, enabled }); } catch (e) { say(e.message || String(e)); }
            v.ov.width = W; v.ov.height = H;
            holes(v.ov, lists[k], W, H);
        }
        $('viewD').hidden = !show.D;
        if (show.D && views.A.r && views.B.r) {
            diffCanvas.width = W; diffCanvas.height = H;
            const img = new ImageData(W, H);
            const d = FLDL.diffPixels(views.A.r.pixels(), views.B.r.pixels(), W, H, 8, img.data);
            diffCanvas.getContext('2d').putImageData(img, 0, 0);
            $('viewD').querySelector('figcaption').textContent = `Difference: ${d.count ? d.count + ' pixels differ' + (d.box ? ', within x ' + d.box[0] + ', y ' + d.box[1] + ', ' + d.box[2] + ' x ' + d.box[3] : '') : 'none: the same picture'}`;
        }
        layout();
    }
    function holes(cv, list, W, H) {
        const g = cv.getContext('2d');
        g.clearRect(0, 0, W, H);
        const lw = Math.max(1, Math.round(H / 600));
        g.lineWidth = lw;
        g.font = `${Math.max(10, Math.round(H / 60))}px system-ui, sans-serif`;
        g.textBaseline = 'top';
        for (const h of (list && list.holes) || []) {
            if (!h.shown) continue;
            const [x, y, w, hh] = h.rect;
            g.save(); g.beginPath(); g.rect(x, y, w, hh); g.clip();
            g.strokeStyle = 'rgba(160,160,160,0.45)';
            const step = Math.max(6, Math.round(H / 90));
            for (let t = -hh; t < w; t += step) { g.beginPath(); g.moveTo(x + t, y + hh); g.lineTo(x + t + hh, y); g.stroke(); }
            g.restore();
            g.strokeStyle = '#a0a0a0'; g.strokeRect(x, y, w, hh);
            g.fillStyle = '#c8c8c8';
            g.fillText(h.kind === 'view.ship' ? '3D view: your ship' : '3D view: the target', x + 4 * lw, y + 4 * lw);
        }
    }
    function layout() {
        if (!S.last) return;
        const { w: W, h: H } = S.last.res;
        const visible = [...document.querySelectorAll('#views .view')].filter((f) => !f.hidden).length;
        const avail = Math.max(200, ($('views').clientWidth - 12 * visible) / Math.max(1, visible) - 16);
        const cssW = avail, cssH = cssW * H / W;
        for (const c of document.querySelectorAll('#views canvas')) { c.style.width = cssW + 'px'; c.style.height = cssH + 'px'; }
    }
    window.addEventListener('resize', layout);
    document.querySelectorAll('input[name=view]').forEach((i) => i.addEventListener('change', render));
    function flip() {
        const to = radio('view') === 'A' ? 'B' : 'A';
        document.querySelector(`input[name=view][value=${to}]`).checked = true;
        render();
    }
    $('flip').addEventListener('click', flip);
    document.addEventListener('keydown', (e) => {
        const t = e.target;
        const typing = t && /^(INPUT|SELECT|TEXTAREA)$/.test(t.tagName) && t.type !== 'radio' && t.type !== 'checkbox';
        if ((e.key === 'f' || e.key === 'F') && !e.ctrlKey && !e.altKey && !e.metaKey && !typing) flip();
    });

    // ---------------------------------------------------------------- saved variants
    async function listVariants() {
        const list = await (await get('/api/variants')).json();
        const box = $('variants');
        box.textContent = '';
        if (!list.length) { box.innerHTML = '<div class="small">None yet.</div>'; return; }
        for (const v of list) {
            const d = document.createElement('div'); d.className = 'row';
            const b = document.createElement('button'); b.type = 'button'; b.textContent = 'Load';
            b.addEventListener('click', () => loadVariant(v.name));
            d.append(b, ' ' + v.name + ' ', Object.assign(document.createElement('span'), { className: 'when', textContent: when(v.time) }));
            box.append(d);
        }
    }
    $('varSave').addEventListener('click', async () => {
        const name = $('varName').value.trim();
        if (!/^[A-Za-z0-9_-][A-Za-z0-9 _-]{0,79}$/.test(name)) { say('A variant’s name: letters, digits, spaces, - and _.'); return; }
        const r = await (await fetch('/api/save?name=' + encodeURIComponent(name), { method: 'POST', body: askText() })).json();
        if (!r.ok) { say('Not saved: ' + r.error); return; }
        $('status').textContent = `saved as "${name}"`;
        listVariants();
    });
    async function loadVariant(name) {
        const text = await (await get('/api/variant?name=' + encodeURIComponent(name))).text();
        let rel = '', size = '';
        const set = {}, lay = {};
        for (const line of text.split(/\r?\n/)) {
            const m = /^(\w+) (.*)$/.exec(line);
            if (!m) continue;
            if (m[1] === 'rec') rel = m[2];
            else if (m[1] === 'size') size = m[2];
            else if (m[1] === 'set' || m[1] === 'layout') { const i = m[2].indexOf('='); (m[1] === 'set' ? set : lay)[m[2].slice(0, i)] = m[2].slice(i + 1); }
        }
        const rec = S.records.find((r) => r.rel === rel);
        if (!rec) { say(`The variant's recorded state is no longer there: ${rel}`); return; }
        const { input, base } = splitInput(rec.state);
        S.run = rec.run; $('run').value = rec.run;
        S.key = base + (rec.frame ? ` (frame ${rec.frame + 1})` : '');
        S.input = input; S.screen = size || rec.size; S.screenChosen = true;
        S.set = set; S.lay = lay;
        $('varName').value = name;
        listStates(); listLayout();
        await pickRecord();
    }

    // ---------------------------------------------------------------- the window is open
    async function ping() {
        try { await fetch('/api/ping', { cache: 'no-store' }); $('gone').hidden = true; }
        catch (e) { $('gone').hidden = false; }
    }
    setInterval(ping, 3000);
    ping();
    // closing (or reloading) the window: the program ends 3 s later unless a ping says it was a reload
    window.addEventListener('pagehide', () => { navigator.sendBeacon('/api/bye', ''); });

    (async () => {
        try { await Promise.all([loadLayout(), listVariants()]); await loadRecords(); }
        catch (e) { say(`The bench program did not answer: ${e.message || e}`); }
    })();
})();
