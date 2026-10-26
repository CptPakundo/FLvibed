// ================================================================== the drawer (editor / notes)
function renderDrawer() {
  const d = $('#drawer');
  if (view === 'bindings') return renderBindingsDrawer(d);
  if (!editMode) {
    d.innerHTML = `<h2>Try it</h2><p class="note">Everything on the screen works: toggles, pickers, sliders, the key box ("press a key" checks the game's default bindings), DEFAULT and CANCEL (click FLVIBED first to mark the entry point), the Show-advanced row on GENERAL, the preset rows (any changed row makes them CUSTOM), the preview line on SUBTITLES.</p><p class="note">With a pad as the input the mouse arrow is gone, the focused row is highlighted and a prompt bar sits bottom left; arrow keys stand in for the D-pad, PageUp / PageDown for the bumpers.</p>${ro() ? '<p class="note">Choices made here are only a preview: they go back to the current settings when you leave.</p>' : '<p class="note">Turn on <b>Edit mode</b> to change the rows, tabs, sections, presets and the bindings.</p>'}<h3>${ro() ? 'The page' : 'Spec'}</h3><p class="small">${spec.tabs.length} tabs, ${allRows().length} rows (${allRows().filter(x => x.r.tier === 'advanced').length} advanced).</p>${ro() ? '' : `<h3>Notes for Claude</h3><textarea id="notesAll" style="min-height:110px" placeholder="Anything else: wording, rows to add or drop, how it should feel...">${esc(spec.notes || '')}</textarea>`}`;
    const na = $('#notesAll'); if (na) na.addEventListener('input', (e) => { spec.notes = e.target.value; touched(); });
    return;
  }
  const tab = tabOf(curTab); const h = [];
  h.push(`<h2>Edit: ${esc(tab.label)}</h2>`);
  h.push(`<div class="btnrow"><button data-do="addRow">+ Row</button><button data-do="addSection">+ Section</button><button data-do="addTab">+ Tab</button><button data-do="presets">Presets…</button></div>`);
  if (selected && selected.type === 'row') {
    const x = findRow(selected.id); if (!x) { selected = null; return renderDrawer(); } const r = x.r;
    h.push(`<h3>Row</h3><div class="field">
      <label>Label</label><input data-f="label" value="${esc(r.label)}">
      <label>Ini key</label><input data-f="key" value="${esc(r.key)}">
      <label>Kind</label><select data-f="kind"><option value="toggle">Toggle (checkbox)</option><option value="picker">Picker (list of choices)</option><option value="slider">Slider</option><option value="key">Key binding</option><option value="button">Button (action)</option></select>
      <label>Help line</label><textarea data-f="help">${esc(r.help || '')}</textarea>
      <label>Takes effect</label><select data-f="needs"><option value="live">Live</option><option value="load">At the next load</option><option value="restart">After restart</option></select>
      <label>Tier</label><select data-f="tier"><option value="player">Player</option><option value="advanced">Advanced (hidden unless Show advanced)</option></select>
      <label>Depends on</label><select data-f="parent"><option value="">(none)</option>${allRows().filter(y => y.r.kind === 'toggle' && y.r.id !== r.id).map(y => `<option value="${y.r.id}">${esc(y.t.label)} › ${esc(y.r.label)}</option>`).join('')}</select>
      </div>`);
    if (r.kind === 'picker') h.push(`<h3>Choices</h3><div class="list" id="optList">${(r.options || []).map((o, i) => `<div class="it"><input data-opt="${i}" value="${esc(o)}"><button data-optdel="${i}" title="Remove">×</button></div>`).join('')}</div><div class="btnrow"><button data-do="addOpt">+ Choice</button></div><div class="field"><label>Default</label><select data-f="def">${(r.options || []).map(o => `<option ${o === r.def ? 'selected' : ''}>${esc(o)}</option>`).join('')}</select><label>Preset row</label><input type="checkbox" data-f="preset" ${r.preset ? 'checked' : ''} title="This picker applies the tab's presets"></div>`);
    if (r.kind === 'slider') h.push(`<h3>Range</h3><div class="field"><label>Min</label><input type="number" data-f="min" value="${r.min}" step="any"><label>Max</label><input type="number" data-f="max" value="${r.max}" step="any"><label>Step</label><input type="number" data-f="step" value="${r.step}" step="any"><label>Default</label><input type="number" data-f="def" value="${r.def}" step="any"><label>Unit</label><input data-f="unit" value="${esc(r.unit || '')}" placeholder="%, m, s"><label>Words</label><textarea data-f="words" placeholder="One per line: the slider shows words instead of numbers (e.g. LOW / NORMAL / HIGH)">${esc((r.words || []).join('\n'))}</textarea></div><p class="small">To turn this slider into a limited list, set Kind to Picker: the words (or five evenly spaced values) become the choices.</p>`);
    if (r.kind === 'toggle') h.push(`<div class="field"><label>Default</label><select data-f="def"><option value="1" ${r.def ? 'selected' : ''}>Enabled</option><option value="0" ${!r.def ? 'selected' : ''}>Disabled</option></select></div>`);
    if (r.kind === 'key') h.push(`<div class="field"><label>Default key</label><input data-f="def" value="${esc(r.def || '')}"></div>`);
    if (r.kind === 'button') h.push(`<div class="field"><label>Button text</label><input data-f="btn" value="${esc(r.btn || '')}"></div>`);
    h.push(`<h3>Place</h3><div class="field"><label>Tab</label><select data-f="moveTab">${spec.tabs.map(t => `<option value="${t.id}" ${t.id === x.t.id ? 'selected' : ''}>${esc(t.label)}</option>`).join('')}</select><label>Section</label><select data-f="moveSec">${x.t.sections.map((s, i) => `<option value="${i}" ${s === x.s ? 'selected' : ''}>${esc(s.label || '(unnamed)')}</option>`).join('')}</select></div>
      <div class="btnrow"><button data-do="up">▲ Up</button><button data-do="down">▼ Down</button><button data-do="dup">Duplicate</button><button data-do="del" style="color:var(--err)">Delete row</button></div>`);
  } else if (selected && selected.type === 'tab') {
    const t = tabOf(selected.id);
    h.push(`<h3>Tab</h3><div class="field"><label>Label</label><input data-tf="label" value="${esc(t.label)}"><label>Preset row</label><input data-tf="presetKey" value="${esc(t.presetKey || '')}" placeholder="key of the preset picker, if any"></div><div class="btnrow"><button data-do="tabLeft">◀ Move left</button><button data-do="tabRight">Move right ▶</button><button data-do="tabDel" style="color:var(--err)">Delete tab</button></div>`);
  } else if (selected && selected.type === 'section') {
    const s = tabOf(selected.tab).sections[selected.index];
    if (s) h.push(`<h3>Section</h3><div class="field"><label>Header</label><input data-sf="label" value="${esc(s.label)}" placeholder="(empty = no header)"></div><div class="btnrow"><button data-do="secUp">▲ Up</button><button data-do="secDown">▼ Down</button><button data-do="secDel" style="color:var(--err)">Delete section (rows too)</button></div>`);
  } else if (selected && selected.type === 'presets') {
    const list = spec.presets[tab.id] || [];
    h.push(`<h3>Presets of ${esc(tab.label)}</h3><p class="small">A preset is a bundle of values for this tab's rows (by ini key). "Capture" stores the rows' current values under a name.</p>`);
    list.forEach((p, i) => h.push(`<div class="it list" style="flex-direction:row"><input data-pname="${i}" value="${esc(p.name)}"><button data-pcap="${i}" title="Overwrite with the current values">Capture</button><button data-papply="${i}">Apply</button><button data-pdel="${i}">×</button></div>`));
    h.push(`<div class="btnrow"><button data-do="pAdd">+ Preset from current values</button></div><textarea id="pJson" style="min-height:140px">${esc(JSON.stringify(list, null, 1))}</textarea><div class="btnrow"><button data-do="pJsonApply">Apply JSON</button></div>`);
  } else h.push('<p class="small">Click a row, a tab or a section header on the screen to edit it. Drag rows to reorder them, drag tabs to reorder the tabs.</p>');
  d.innerHTML = h.join('');
  const r = selected && selected.type === 'row' ? findRow(selected.id) : null;
  if (r) {
    $$('[data-f]', d).forEach(el => {
      const f = el.dataset.f; if (f === 'kind') el.value = r.r.kind; if (f === 'needs') el.value = r.r.needs || 'live'; if (f === 'tier') el.value = r.r.tier || 'player'; if (f === 'parent') el.value = r.r.parent || '';
      el.addEventListener(el.tagName === 'SELECT' || el.type === 'checkbox' ? 'change' : 'input', () => {
        if (f === 'kind') changeKind(r.r, el.value);
        else if (f === 'moveTab' || f === 'moveSec') { const tId = $('[data-f=moveTab]', d).value; const sI = +$('[data-f=moveSec]', d).value; const row = detachRow(r.r.id); const t2 = tabOf(tId); const s2 = t2.sections[Math.min(sI, t2.sections.length - 1)] || (t2.sections.push({ label: '', rows: [] }), t2.sections[0]); s2.rows.push(row); curTab = tId; }
        else if (f === 'preset') { allRows().forEach(y => { if (y.t.id === r.t.id) delete y.r.preset; }); if (el.checked) { r.r.preset = true; r.t.presetKey = r.r.key; } else delete r.t.presetKey; }
        else if (f === 'def') { r.r.def = r.r.kind === 'toggle' ? el.value === '1' : r.r.kind === 'slider' ? +el.value : el.value; if (r.r.kind !== 'slider' || !isNaN(r.r.def)) r.r.value = r.r.def; }
        else if (['min', 'max', 'step'].includes(f)) { r.r[f] = +el.value; r.r.value = snap(r.r, r.r.value); }
        else if (f === 'words') r.r.words = el.value.split('\n').map(s => s.trim()).filter(Boolean);
        else if (f === 'parent') r.r.parent = el.value || null;
        else r.r[f] = el.value;
        touched(); if (f === 'kind' || f === 'moveTab' || f === 'moveSec' || f === 'preset' || f === 'def') render(); else renderStageOnly();
      });
    });
    $$('[data-opt]', d).forEach(el => el.addEventListener('input', () => { const i = +el.dataset.opt; const old = r.r.options[i]; r.r.options[i] = el.value; if (r.r.value === old) r.r.value = el.value; if (r.r.def === old) r.r.def = el.value; touched(); renderStageOnly(); }));
    $$('[data-optdel]', d).forEach(el => el.addEventListener('click', () => { r.r.options.splice(+el.dataset.optdel, 1); if (!r.r.options.includes(r.r.value)) r.r.value = r.r.options[0] || ''; if (!r.r.options.includes(r.r.def)) r.r.def = r.r.options[0] || ''; touched(); render(); }));
  }
  if (selected && selected.type === 'tab') $$('[data-tf]', d).forEach(el => el.addEventListener('input', () => { const t = tabOf(selected.id); t[el.dataset.tf] = el.value; if (el.dataset.tf === 'presetKey' && !el.value) delete t.presetKey; touched(); renderStageOnly(); }));
  if (selected && selected.type === 'section') $$('[data-sf]', d).forEach(el => el.addEventListener('input', () => { tabOf(selected.tab).sections[selected.index].label = el.value; touched(); renderStageOnly(); }));
  $$('[data-pname]', d).forEach(el => el.addEventListener('input', () => { spec.presets[tab.id][+el.dataset.pname].name = el.value; syncPresetOptions(tab); touched(); }));
  $$('[data-pcap]', d).forEach(el => el.addEventListener('click', () => { spec.presets[tab.id][+el.dataset.pcap].values = captureValues(tab); touched(); render(); }));
  $$('[data-papply]', d).forEach(el => el.addEventListener('click', () => { applyPreset(tab, spec.presets[tab.id][+el.dataset.papply].name); syncPresetRow(tab); touched(); render(); }));
  $$('[data-pdel]', d).forEach(el => el.addEventListener('click', () => { spec.presets[tab.id].splice(+el.dataset.pdel, 1); syncPresetOptions(tab); touched(); render(); }));
  $$('[data-do]', d).forEach(el => el.addEventListener('click', () => doAction(el.dataset.do, tab)));
}
function captureValues(tab) { const v = {}; tab.sections.forEach(s => s.rows.forEach(r => { if (!r.preset && r.kind !== 'button' && r.kind !== 'key') v[r.key] = r.value; })); return v; }
function syncPresetOptions(tab) { const pr = tab.sections.flatMap(s => s.rows).find(r => r.preset); if (!pr) return; const names = (spec.presets[tab.id] || []).map(p => p.name); pr.options = names.concat(pr.options.includes('CUSTOM') ? ['CUSTOM'] : []); if (!pr.options.includes(pr.value)) pr.value = pr.options[0] || ''; }
function changeKind(r, kind) {
  const old = r.kind; r.kind = kind;
  if (kind === 'picker') {
    if (!r.options || !r.options.length) {
      if (old === 'slider' && r.words && r.words.length) r.options = r.words.slice();
      else if (old === 'slider') { const n = Math.min(5, Math.max(2, Math.round((r.max - r.min) / (r.step || 1)) + 1)); r.options = []; for (let i = 0; i < n; i++) { const v = +(r.min + (r.max - r.min) * i / (n - 1)).toFixed(2); r.options.push(String(Number.isInteger(r.step) ? Math.round(v) : v) + (r.unit || '')); } }
      else r.options = old === 'toggle' ? ['DISABLED', 'ENABLED'] : ['OPTION A', 'OPTION B'];
    }
    if (old === 'slider') { const w = sliderWord({ ...r, value: r.def }); r.def = r.options.includes(w) ? w : r.options.reduce((best, o) => Math.abs(parseFloat(o) - r.def) < Math.abs(parseFloat(best) - r.def) ? o : best, r.options[0]); }
    if (!r.options.includes(r.def)) r.def = r.options[0]; r.value = r.def;
  }
  if (kind === 'slider') { if (r.min === undefined) { r.min = 0; r.max = 100; r.step = 5; } if (old === 'picker') { r.words = r.options.slice(); r.min = 0; r.max = r.words.length - 1; r.step = 1; r.def = Math.max(0, r.options.indexOf(r.def)); } if (typeof r.def !== 'number') r.def = r.min; r.value = r.def; r.unit = r.unit || ''; }
  if (kind === 'toggle') { r.def = !!r.def && r.def !== 'DISABLED' && r.def !== 'OFF'; r.value = r.def; }
  if (kind === 'key') { r.def = typeof r.def === 'string' ? r.def : 'K'; r.value = r.def; }
  if (kind === 'button') { r.btn = r.btn || r.label.toUpperCase(); r.def = null; r.value = null; }
  delete r.preset;
}
function doAction(a, tab) {
  const sel = selected;
  if (a === 'addRow') { const s = (sel && sel.type === 'row' && findRow(sel.id).s) || tab.sections[tab.sections.length - 1] || (tab.sections.push({ label: '', rows: [] }), tab.sections[0]); const r = T(uid('new'), 'New setting', false, { help: 'What it does.' }); r.id = uid('row'); s.rows.push(r); selected = { type: 'row', id: r.id }; }
  else if (a === 'addSection') { tab.sections.push({ label: 'NEW SECTION', rows: [] }); selected = { type: 'section', tab: tab.id, index: tab.sections.length - 1 }; }
  else if (a === 'addTab') { const id = uid('tab'); spec.tabs.push({ id, label: 'NEW TAB', sections: [{ label: '', rows: [] }] }); curTab = id; selected = { type: 'tab', id }; }
  else if (a === 'presets') selected = { type: 'presets' };
  else if (a === 'addOpt' && sel && sel.type === 'row') findRow(sel.id).r.options.push('NEW CHOICE');
  else if (a === 'pAdd') { (spec.presets[tab.id] = spec.presets[tab.id] || []).push({ name: 'PRESET ' + ((spec.presets[tab.id] || []).length + 1), values: captureValues(tab) }); syncPresetOptions(tab); }
  else if (a === 'pJsonApply') { try { spec.presets[tab.id] = JSON.parse($('#pJson').value); syncPresetOptions(tab); } catch (e) { toast('Presets JSON: ' + e.message); return; } }
  else if (sel && sel.type === 'row') {
    const x = findRow(sel.id); const i = x.s.rows.indexOf(x.r);
    if (a === 'up' && i > 0) { x.s.rows.splice(i, 1); x.s.rows.splice(i - 1, 0, x.r); }
    else if (a === 'down' && i < x.s.rows.length - 1) { x.s.rows.splice(i, 1); x.s.rows.splice(i + 1, 0, x.r); }
    else if (a === 'dup') { const c = deepClone(x.r); c.id = uid('row'); c.key = x.r.key + '_copy'; delete c.preset; x.s.rows.splice(i + 1, 0, c); selected = { type: 'row', id: c.id }; }
    else if (a === 'del') { x.s.rows.splice(i, 1); selected = null; }
  } else if (sel && sel.type === 'tab') {
    const i = spec.tabs.findIndex(t => t.id === sel.id);
    if (a === 'tabLeft' && i > 0) { const t = spec.tabs.splice(i, 1)[0]; spec.tabs.splice(i - 1, 0, t); }
    else if (a === 'tabRight' && i < spec.tabs.length - 1) { const t = spec.tabs.splice(i, 1)[0]; spec.tabs.splice(i + 1, 0, t); }
    else if (a === 'tabDel' && spec.tabs.length > 1) { spec.tabs.splice(i, 1); curTab = spec.tabs[0].id; selected = null; }
  } else if (sel && sel.type === 'section') {
    const t = tabOf(sel.tab); const i = sel.index;
    if (a === 'secUp' && i > 0) { const s = t.sections.splice(i, 1)[0]; t.sections.splice(i - 1, 0, s); selected.index = i - 1; }
    else if (a === 'secDown' && i < t.sections.length - 1) { const s = t.sections.splice(i, 1)[0]; t.sections.splice(i + 1, 0, s); selected.index = i + 1; }
    else if (a === 'secDel') { t.sections.splice(i, 1); selected = null; }
  }
  touched(); render();
}
// ---- screens drawer: the list, the states, the notes; edit mode: prompts, notes, focus, hidden, extras
// ---- bindings drawer: context, preset, the selected input's tap / hold / double, conflicts
function renderBindingsDrawer(d) {
  const bd = bindingsFor(curCtx); const h = [];
  h.push(`<h2>Bindings</h2><div class="field"><label>Situation</label><select id="ctxSel">${CONTEXTS.map(c => `<option value="${c[0]}" ${c[0] === curCtx ? 'selected' : ''}>${esc(c[1])}</option>`).join('')}</select>`);
  if (curCtx === 'flight' && ro()) h.push(`<label>Preset</label><div class="seg">${Object.keys(spec.bindings.presets).map(p => `<button data-rop="${p}" class="${p === spec.bindings.preset ? 'on' : ''}">${esc(p)}</button>`).join('')}</div>`);
  else if (curCtx === 'flight') h.push(`<label>Preset</label><select id="presetSel">${Object.keys(spec.bindings.presets).map(p => `<option ${p === spec.bindings.preset ? 'selected' : ''}>${p}</option>`).join('')}</select>`);
  h.push(`</div>`);
  const used = {}; Object.entries(bd).forEach(([k, b]) => ['tap', 'hold', 'dbl'].forEach(v => { if (b[v] && b[v] !== 'none' && !['focus', 'map_snap', 'side', 'shift', 'map_scope', 'handback'].includes(b[v])) (used[b[v]] = used[b[v]] || []).push(k + (v === 'tap' ? '' : ' ' + v)); }));
  const conflicts = Object.entries(used).filter(([a, ks]) => ks.length > 1 && ks.filter(x => !x.includes(' ')).length > 1);
  if (conflicts.length) h.push(`<p class="conflict">Same tap action on two inputs: ${conflicts.map(([a, ks]) => `${esc(ACTIONS[a])} (${ks.join(', ')})`).join('; ')}</p>`);
  const sel = bindSel && bd[bindSel] ? bindSel : null;
  if (sel) {
    const b = bd[sel]; const opts = (cur) => Object.entries(ACTIONS).map(([k, v]) => `<option value="${k}" ${k === cur ? 'selected' : ''}>${esc(v)}</option>`).join('');
    if (ro()) h.push(`<h3>${esc(glyphName(sel, dev() === 'km' ? 'xbox' : dev()))} (${sel})</h3><div class="field"><label>Tap</label><span>${esc(b.tap === 'none' ? '-' : ACTIONS[b.tap] || b.tap)}</span><label>Hold</label><span>${esc(b.hold === 'none' ? '-' : ACTIONS[b.hold] || b.hold)}</span><label>Double tap</label><span>${esc(b.dbl === 'none' ? '-' : ACTIONS[b.dbl] || b.dbl)}</span></div>`);
    else h.push(`<h3>${esc(glyphName(sel, dev() === 'km' ? 'xbox' : dev()))} (${sel})</h3><div class="field"><label>Tap</label><select data-bv="tap">${opts(b.tap)}</select><label>Hold</label><select data-bv="hold">${opts(b.hold)}</select><label>Double tap</label><select data-bv="dbl">${opts(b.dbl)}</select></div>`);
    if (curCtx === 'flight' && !editMode && !ro()) h.push(`<p class="small">Changing a binding of a named preset makes it the CUSTOM preset on the Options page (the preset itself keeps its definition).</p>`);
  } else h.push(`<p class="small">Click a part of the pad or a label on the diagram to ${ro() ? 'see' : 'change'} what it does. Click a stick or the D-pad twice to reach the press / the next direction.</p>`);
  h.push(`<h3>Why this layout</h3><div class="note"><ul>
    <li><b>Fire on the right trigger, afterburner on the left:</b> the trigger is the most comfortable hold on a pad and afterburner is held 29 % of fight time (Chorus, Halo and Destiny boost on LT; players dislike stick-click holds).</li>
    <li><b>Targets on the right stick press and the left bumper:</b> R3 is the lock-on habit from Elden Ring, Armored Core and BotW: tap = what you aim at, else the nearest enemy (Squadrons' "target what is in front"), again = next, double tap = clear, hold = rear view; LB tap opens the picker, LB hold the action wheel (Squadrons' tap / hold pattern on one bumper). No "my attacker" bind: nearest enemy and firing-selects-the-target cover it.</li>
    <li><b>Consumables one tap each on the D-pad:</b> the throttle lever persists, so the left thumb can leave the stick for a nanobot without losing speed (Squadrons, Everspace 2, Armored Core VI put consumables there).</li>
    <li><b>A = context, B = cancel / brake:</b> the universal grammar; B held = reverse (Rebel Galaxy Outlaw's throttle-down on B).</li>
    <li><b>Rare and contextual actions in one wheel on LB hold, picked with the right stick</b> (dead-zoned; the D-pad works too) while the left stick keeps flying; X held = turret view; no chords in combat; the only chord in the whole layout is the optional L3 + R3 autopilot alternative.</li>
    <li><b>Autopilot on a half-second hold of View with a ring:</b> deliberate, one-handed, not a double-tap (the Star Citizen anti-pattern).</li></ul></div>`);
  h.push(`<h3>All inputs</h3><table class="bind"><tr><th>Input</th><th>Tap</th><th>Hold</th></tr>${INPUTS.map(k => { const b = bd[k] || Bd(); return `<tr data-row-b="${k}" style="cursor:pointer${k === sel ? ';color:var(--accent)' : ''}"><td>${esc(glyphName(k, dev() === 'km' ? 'xbox' : dev()))}${k === 'L3' || k === 'R3' ? ' press' : ''}</td><td>${esc(ACTIONS[b.tap] || '')}</td><td>${esc(b.hold === 'none' ? '' : ACTIONS[b.hold])}${b.dbl !== 'none' ? ' · ×2 ' + esc(ACTIONS[b.dbl]) : ''}</td></tr>`; }).join('')}</table>`);
  d.innerHTML = h.join('');
  $('#ctxSel').addEventListener('change', (e) => { curCtx = e.target.value; render(); });
  $$('[data-rop]', d).forEach(el => el.addEventListener('click', () => { spec.bindings.preset = el.dataset.rop; const pr = rowByKey('pad_preset'); if (pr) pr.r.value = el.dataset.rop; render(); }));
  const ps = $('#presetSel'); if (ps) ps.addEventListener('change', (e) => { spec.bindings.preset = e.target.value; const pr = rowByKey('pad_preset'); if (pr) pr.r.value = e.target.value; touched(); render(); });
  $$('[data-bv]', d).forEach(el => el.addEventListener('change', () => {
    if (curCtx === 'flight' && spec.bindings.preset !== 'CUSTOM') { spec.bindings.presets.CUSTOM = deepClone(spec.bindings.presets[spec.bindings.preset]); spec.bindings.preset = 'CUSTOM'; const pr = rowByKey('pad_preset'); if (pr) pr.r.value = 'CUSTOM'; }
    bindingsFor(curCtx)[sel][el.dataset.bv] = el.value; touched(); render();
  }));
  $$('[data-row-b]', d).forEach(el => el.addEventListener('click', () => { bindSel = el.dataset.rowB; render(); }));
}

// ================================================================== persistence: the artifact's store (db) + this browser
function touched() {
  const kr = rowByKey('autopilot_key'); if (kr && !kr.r.capture) spec.bar.key = kr.r.value; const mb = rowByKey('maneuver_button'); if (mb) spec.bar.enabled = !!mb.r.value;
  if (ro()) return; // read-only: the previews never write anywhere
  try { localStorage.setItem('flvibed-ui-mock-v2', JSON.stringify(spec)); } catch (e) { }
  clearTimeout(saveTimer); saveTimer = setTimeout(saveToStore, 900); setStatus('Saving…');
}
let saving = false, saveAgain = false;
async function saveToStore() {
  if (ro()) return;
  if (!db) { setStatus('Kept in this browser only (the artifact\'s store is not available here)', true); return; }
  if (saving) { saveAgain = true; return; } saving = true;
  try { await db.doc('ui/spec').set({ spec: deepClone(spec), savedAt: new Date().toISOString(), v: 2 }); setStatus('Saved into the artifact ' + new Date().toLocaleTimeString()); }
  catch (e) { setStatus('Could not save into the artifact (' + (e && e.code || 'error') + '); kept in this browser', true); }
  saving = false; if (saveAgain) { saveAgain = false; saveToStore(); }
}
function setStatus(s, err) { const el = $('#status'); el.textContent = s; el.classList.toggle('err', !!err); }
function applySpec(s) {
  if (!s || !s.tabs) return false;
  if (!s.v || s.v < 2) { // a v1 spec (the options page only): keep the answered decisions and the notes, take the rest from the v2 proposal
    const old = s; s = deepClone(DEFAULT_SPEC);
    (old.decisions || []).forEach(od => { const nd = s.decisions.find(x => x.id === od.id); if (nd && od.answer !== null && od.answer !== undefined) { nd.answer = od.answer; nd.note = od.note || ''; } });
    s.notes = old.notes || ''; if (old.bar) s.bar = old.bar;
  }
  spec = s; spec.decisions = spec.decisions || deepClone(DECISIONS); spec.presets = spec.presets || deepClone(PRESETS); spec.bar = spec.bar || deepClone(DEFAULT_SPEC.bar);
  spec.bindings = spec.bindings || deepClone(BINDINGS); spec.screens = spec.screens && spec.screens.length ? spec.screens : deepClone(SCREENS); spec.device = spec.device || 'xbox';
  DECISIONS.forEach(dd => { if (!spec.decisions.find(x => x.id === dd.id)) spec.decisions.push(deepClone(dd)); });
  if (!tabOf(curTab)) curTab = spec.tabs[0].id; return true;
}
// The runtime, decided once at load: the claude.ai viewer injects window.claude (capabilities `user` + `db`). A page
// without it (the static copy in docs\spec written by export.ps1, a local test server, any other host) has no runtime:
// read-only, nothing read from or written to a store, no browser storage written. The static export also sets
// window.FLV_STATIC_COPY (and carries the owner's ui/spec as window.FLV_SPEC_SNAPSHOT), which forces the same even if
// some host defined a window.claude.
const STATIC_COPY = !!window.FLV_STATIC_COPY;
const RUNTIME = !STATIC_COPY && !!(window.claude && typeof window.claude.use === 'function');
// The mode: editor tools only when the platform says this viewer is the owner or an editor and may write the shared
// db; no runtime (a copy outside claude.ai), no viewer (signed out), no user capability or any doubt = read-only.
async function resolveMode() {
  if (!RUNTIME) return false;
  let user = null; try { user = await window.claude.use('user'); } catch (e) { user = null; }
  if (!user) return false;
  try { const [own, edit, w] = await Promise.all([user.isOwner(), user.canEdit(), user.can('data.write')]); return !!(own || edit) && w !== false; }
  catch (e) { return false; }
}
async function initStore() {
  if (!RUNTIME) { // no claude.ai runtime: read-only on the owner's ui/spec snapshot (static copy) or the built-in data
    isEditor = false; db = null;
    const keep = devPicked ? spec.device : null;
    if (window.FLV_SPEC_SNAPSHOT) applySpec(deepClone(window.FLV_SPEC_SNAPSHOT));
    if (keep) spec.device = keep;
    canonSpec = deepClone(spec); render(); return;
  }
  const dbP = (async () => { try { return await window.claude.use('db'); } catch (e) { return null; } })();
  isEditor = await resolveMode();
  if (isEditor) {
    try { if (localStorage.getItem('flvibed-ui-edit-tools') === '0') editTools = false; } catch (e) { }
    $('#roToggle').classList.remove('hidden');
    try { const ls = localStorage.getItem('flvibed-ui-mock-v2'); if (ls) { applySpec(JSON.parse(ls)); setStatus('Loaded from this browser'); } } catch (e) { }
  }
  db = await dbP;
  const keepDev = devPicked ? spec.device : null;
  if (!db) { if (isEditor) setStatus('Store unavailable here: changes stay in this browser', true); }
  else try {
    const snap = await db.doc('ui/spec').get();
    if (snap.exists && snap.data().spec) { applySpec(deepClone(snap.data().spec)); setStatus('Loaded from the artifact (saved ' + (snap.data().savedAt || '?') + (snap.data().v < 2 ? ', upgraded to the controller round' : '') + ')'); if (isEditor && (!snap.data().v || snap.data().v < 2)) touched(); }
    else setStatus('Ready: the proposal (nothing saved yet)');
  } catch (e) { setStatus('Could not read the artifact\'s store (' + (e && e.code || 'error') + ')', true); }
  if (keepDev) spec.device = keepDev;
  canonSpec = deepClone(spec);
  render();
}

// ================================================================== toolbar
$$('#itoolViews button').forEach(b => b.addEventListener('click', () => { view = b.dataset.view; selected = null; render(); }));
$('#backDoc').addEventListener('click', () => { view = VIEW_PARENT[view] || 'overview'; selected = null; render(); });
try { const h = (location.hash || '').slice(1); if (isDocView(h) || VIEW_PARENT[h]) view = h; } catch (e) { }
$$('.toolbar .dev').forEach(b => b.addEventListener('click', () => { devPicked = true; spec.device = b.dataset.dev; const gs = rowByKey('glyph_style'); if (gs && b.dataset.dev !== 'km') { const m = { xbox: 'XBOX', ps: 'PLAYSTATION', switch: 'NINTENDO' }; if (gs.r.value !== 'AUTOMATIC') gs.r.value = m[b.dataset.dev]; } touched(); render(); }));
$('#roToggle').addEventListener('click', () => {
  if (!isEditor) return; const d = spec.device;
  if (editTools) canonSpec = deepClone(spec); else { spec = deepClone(canonSpec); spec.device = d; }
  editTools = !editTools; try { localStorage.setItem('flvibed-ui-edit-tools', editTools ? '1' : '0'); } catch (e) { }
  selected = null; render();
});
$('#editToggle').addEventListener('click', () => { if (ro()) return; editMode = !editMode; selected = null; render(); });
$('#advToggle').addEventListener('click', () => { const r = rowByKey('show_advanced'); if (r) { r.r.value = !r.r.value; touched(); } else showAdv = !showAdv; render(); });
$('#exportBtn').addEventListener('click', () => { if (ro()) return; // editor-only (hidden and inert in read-only)
  const d = $('#drawer'); d.innerHTML = `<h2>Export</h2><p class="small">The whole spec as JSON. Copy it, or just tell Claude to read it: it is saved in this artifact.</p><textarea id="exportTa" style="min-height:300px">${esc(JSON.stringify(spec, null, 1))}</textarea><div class="btnrow"><button id="copyBtn">Copy</button><button id="backBtn">Back</button></div>`;
  $('#copyBtn').addEventListener('click', async () => { const ta = $('#exportTa'); try { await navigator.clipboard.writeText(ta.value); toast('Copied'); } catch (e) { ta.select(); } });
  $('#backBtn').addEventListener('click', render);
});
$('#importBtn').addEventListener('click', () => { if (ro()) return;
  const d = $('#drawer'); d.innerHTML = `<h2>Import</h2><p class="small">Paste a spec JSON (from Export) and apply it. It replaces everything.</p><textarea id="importTa" style="min-height:300px"></textarea><div class="btnrow"><button id="applyImport">Apply</button><button id="backBtn">Back</button></div><p class="small" id="importMsg"></p>`;
  $('#applyImport').addEventListener('click', () => { if (ro()) return; try { if (!applySpec(JSON.parse($('#importTa').value))) throw new Error('not a spec'); touched(); render(); } catch (e) { $('#importMsg').textContent = 'Could not read it: ' + e.message; } });
  $('#backBtn').addEventListener('click', render);
});
$('#resetBtn').addEventListener('click', () => { if (ro()) return;
  const d = $('#drawer'); d.innerHTML = `<h2>Reset</h2><p>Throw away every change to the rows, tabs, presets and bindings and go back to the proposal? (The decision history in the spec is reset too.)</p><div class="btnrow"><button id="resetYes" style="color:var(--err)">Yes, reset</button><button id="backBtn">No</button></div>`;
  $('#resetYes').addEventListener('click', () => { if (ro()) return; spec = deepClone(DEFAULT_SPEC); curTab = 'controller'; selected = null; touched(); render(); });
  $('#backBtn').addEventListener('click', render);
});

render();
initStore();
