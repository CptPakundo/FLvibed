// ================================================================== state
let spec = deepClone(DEFAULT_SPEC);
let view = 'overview', curTab = 'controller', editMode = false, showAdv = false, selected = null, focusRow = null;
let curCtx = 'flight', bindSel = null;
let snapshot = null; // values when the FLVIBED page was entered (CANCEL)
let db = null, saveTimer = null;
// Read-only is the default (sharing with players and third parties): only a viewer the platform names the owner or an editor
// (user capability, part3b_editor.js resolveMode) gets the editing tools, and they can switch them off ("Edit: off") to see
// the page as everyone else does. canonSpec = the current settings (ui/spec when readable, else the built-in data); in
// read-only the Options / Bindings previews play on a copy and the document always shows canonSpec.
let isEditor = false, editTools = true, canonSpec = deepClone(DEFAULT_SPEC), devPicked = false;
function ro() { return !(isEditor && editTools); }
const $ = (s, el) => (el || document).querySelector(s);
const $$ = (s, el) => Array.from((el || document).querySelectorAll(s));
function deepClone(o) { return JSON.parse(JSON.stringify(o)); }
function esc(s) { return String(s).replace(/[&<>"']/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]); }
function allRows() { const out = []; spec.tabs.forEach(t => t.sections.forEach(s => s.rows.forEach(r => out.push({ r, t, s })))); return out; }
function findRow(id) { return allRows().find(x => x.r.id === id) || null; }
function rowByKey(key) { return allRows().find(x => x.r.key === key); }
function tabOf(id) { return spec.tabs.find(t => t.id === id); }
function uid(prefix) { return prefix + '_' + Math.random().toString(36).slice(2, 7); }
function dev() { return spec.device || 'xbox'; }
function padMode() { return dev() !== 'km'; }
function gl(input) { return glyph(input, dev()); }
function bindingsFor(ctx) { if (ctx === 'flight') { const p = spec.bindings.presets[spec.bindings.preset] || spec.bindings.presets.FREELANCER; return p.flight; } return spec.bindings.common[ctx]; }

// ================================================================== values (the options page)
function sliderWord(r) {
  if (r.words && r.words.length) { const n = r.words.length; const f = (r.value - r.min) / ((r.max - r.min) || 1); return r.words[Math.min(n - 1, Math.round(f * (n - 1)))]; }
  const v = Math.round(r.value * 100) / 100; return (Number.isInteger(r.step) && Number.isInteger(r.min) ? Math.round(v) : v) + (r.unit || '');
}
function isChanged(r) { return r.kind !== 'button' && JSON.stringify(r.value) !== JSON.stringify(r.def); }
function parentOff(r) { if (!r.parent) return false; const p = findRow(r.parent); return !!(p && p.r.kind === 'toggle' && !p.r.value); }
function setValue(r, v, tab) {
  r.value = v;
  if (r.preset) applyPreset(tab, v); else if (tab && tab.presetKey) syncPresetRow(tab);
  if (r.key === 'show_advanced') showAdv = !!v;
  if (r.key === 'pad_preset' && spec.bindings.presets[v]) spec.bindings.preset = v;
  if (r.key === 'glyph_style') { const m = { XBOX: 'xbox', PLAYSTATION: 'ps', NINTENDO: 'switch' }; if (m[v] && padMode()) spec.device = m[v]; }
  touched();
}
function applyPreset(tab, name) {
  const ps = (spec.presets[tab.id] || []).find(p => p.name === name); if (!ps) return;
  tab.sections.forEach(s => s.rows.forEach(r => { if (r.key in ps.values) r.value = ps.values[r.key]; }));
  if (tab.id === 'controller' && spec.bindings.presets[name]) spec.bindings.preset = name;
}
function syncPresetRow(tab) {
  const pr = tab.sections.flatMap(s => s.rows).find(r => r.preset); if (!pr) return;
  const list = spec.presets[tab.id] || [];
  const match = list.find(ps => Object.keys(ps.values).every(k => { const x = rowByKey(k); return x && JSON.stringify(x.r.value) === JSON.stringify(ps.values[k]); }));
  pr.value = match ? match.name : (pr.options.includes('CUSTOM') ? 'CUSTOM' : pr.value);
}
function stepPicker(r, dir, tab) { const i = Math.max(0, r.options.indexOf(r.value)); setValue(r, r.options[(i + dir + r.options.length) % r.options.length], tab); }
function stepSlider(r, dir, tab) { const v = Math.min(r.max, Math.max(r.min, +(r.value + dir * r.step).toFixed(4))); setValue(r, v, tab); }
function nudge(r, dir, tab) { if (r.kind === 'toggle') setValue(r, !r.value, tab); else if (r.kind === 'picker') stepPicker(r, dir, tab); else if (r.kind === 'slider') stepSlider(r, dir, tab); }
function snap(r, v) { const st = r.step || 1; return Math.min(r.max, Math.max(r.min, +(Math.round(v / st) * st).toFixed(4))); }
function snapshotValues() { const m = {}; allRows().forEach(x => m[x.r.id] = x.r.value); return m; }
function restoreValues(m) { allRows().forEach(x => { if (x.r.id in m) x.r.value = m[x.r.id]; }); spec.tabs.forEach(t => t.presetKey && syncPresetRow(t)); }
function showHelp(r) { const el = $('#help'); if (!el) return; el.textContent = r ? (r.help || '') + (r.key ? `   [${r.key}]` : '') : ''; }
function toast(msg) { const el = $('#toast'); if (!el) return; el.textContent = msg; el.classList.add('show'); clearTimeout(toast.t); toast.t = setTimeout(() => el.classList.remove('show'), 2200); }
function promptBar(prompts, cls) {
  if (!padMode() || !prompts || !prompts.length) return '';
  return `<div class="pbar ${cls || ''}">` + prompts.map(p => `<span class="p">${p.keys.map(k => gl(k)).join('')}${p.v === 'hold' ? '<span class="hold">HOLD</span>' : p.v === 'dbl' ? '<span class="hold">×2</span>' : ''} ${esc(p.label)}</span>`).join('') + `</div>`;
}

// ================================================================== render: dispatcher
function render() {
  const docv = isDocView(view), navOn = docv ? view : VIEW_PARENT[view];
  document.body.classList.toggle('ed', !ro()); if (ro()) { editMode = false; if (docv) { const d = spec.device; spec = deepClone(canonSpec); spec.device = d; } }
  $('#roToggle').textContent = 'Edit: ' + (editTools ? 'on' : 'off'); $('#roToggle').classList.toggle('on', editTools);
  $('#viewBtns').innerHTML = DOC.map(s => `<button data-view="${s.id}" class="${s.id === navOn ? 'on' : ''}"${s.id === navOn ? ' aria-current="page"' : ''}>${esc(s.nav)}</button>`).join('');
  $$('#viewBtns button').forEach(b => b.addEventListener('click', () => { view = b.dataset.view; selected = null; render(); }));
  $$('.toolbar .dev').forEach(b => b.classList.toggle('on', b.dataset.dev === dev()));
  $('#doc').hidden = !docv; $('#interactive').hidden = docv;
  try { history.replaceState(null, '', '#' + view); } catch (e) { }
  if (docv) { renderDoc(); return; }
  $$('#itoolViews button').forEach(b => b.classList.toggle('on', b.dataset.view === view));
  $('#editToggle').textContent = 'Edit mode: ' + (editMode ? 'ON' : 'off'); $('#editToggle').classList.toggle('on', editMode);
  const advRow = rowByKey('show_advanced'); if (advRow) showAdv = !!advRow.r.value;
  $('#advToggle').textContent = 'Advanced rows: ' + (showAdv ? 'on' : 'off'); $('#advToggle').classList.toggle('on', showAdv);
  const stage = $('#stage');
  stage.className = 'stage' + (view === 'bindings' ? ' diagram' : '');
  stage.style.backgroundImage = 'none';
  if (view === 'main' || view === 'ingame') renderOptions(stage, CTX[view]);
  else renderBindings(stage);
  fitStage();
  renderDrawer();
}
function renderStageOnly() { if (isDocView(view)) return; const s = $('#stage'); if (view === 'main' || view === 'ingame') renderOptions(s, CTX[view]); else renderBindings(s); fitStage(); }

// ================================================================== render: the options screen
function renderOptions(stage, c) {
  const tab = tabOf(curTab) || spec.tabs[0]; curTab = tab.id;
  const h = [];
  stage.style.backgroundImage = `url(${c.bg})`;
  c.gameBtns.forEach(([lab, y]) => h.push(`<div class="abs hit gamebtn" data-lab="${esc(lab)}" style="left:${c.btnX - 218}px;top:${y - 59}px;width:436px;height:118px"></div>`));
  h.push(`<div class="abs pagebtn sel" id="flvBtn" style="left:${c.btnX - 218}px;top:${c.slotY - 59}px"><span>FLVIBED</span></div>`);
  h.push(`<div class="abs game title" style="top:${c.titleY}px">FLVIBED OPTIONS</div>`);
  const tabsW = spec.tabs.length * 187 + (spec.tabs.length - 1) * 8; const tabsX = c.panel.x + (c.panel.w - tabsW) / 2;
  h.push(`<div class="abs tabs" style="left:${tabsX}px;top:${c.tabsY}px">` + spec.tabs.map(t => `<div class="game tab${t.id === tab.id ? ' sel' : ''}${editMode && selected && selected.type === 'tab' && selected.id === t.id ? ' edit-sel' : ''}" data-tab="${t.id}" draggable="${editMode}">${esc(t.label)}</div>`).join('') + `</div>`);
  const rows = []; let firstVisible = null;
  tab.sections.forEach((s, si) => {
    const vis = s.rows.filter(r => showAdv || r.tier !== 'advanced');
    if (!vis.length && !editMode) return;
    if (s.label || editMode) rows.push(`<div class="game sec${editMode && selected && selected.type === 'section' && selected.tab === tab.id && selected.index === si ? ' edit-sel' : ''}" data-sec="${si}">${esc(s.label || (editMode ? '(unnamed section)' : ''))}</div>`);
    s.rows.forEach(r => {
      if (!showAdv && r.tier === 'advanced' && !editMode) return;
      if (!firstVisible && !parentOff(r)) firstVisible = r.id;
      const cls = ['row'];
      if (isChanged(r)) cls.push('changed'); if (parentOff(r)) cls.push('disabled'); if (r.tier === 'advanced') cls.push('adv');
      if (focusRow === r.id) cls.push('focus'); if (padMode() && (focusRow || firstVisible) === r.id) cls.push('padfocus');
      if (editMode && selected && selected.type === 'row' && selected.id === r.id) cls.push('edit-sel');
      let ctl = '';
      if (r.kind === 'toggle') ctl = `<div class="game state">${r.value ? 'ENABLED' : 'DISABLED'}</div><div class="box${r.value ? ' on' : ''}" data-act="toggle"></div>`;
      else if (r.kind === 'picker') ctl = `<div class="picker" data-act="picker"><div class="game val">${esc(r.value)}</div><div class="arr"></div></div>`;
      else if (r.kind === 'slider') { const f = (r.value - r.min) / ((r.max - r.min) || 1); ctl = `<div class="slider"><div class="cap">${esc(sliderWord(r))}</div><div class="l" data-act="dec"></div><div class="track" data-act="track"><div class="knob" style="left:${(23 + f * (285 - 46)).toFixed(1)}px"></div></div><div class="r" data-act="inc"></div></div>`; }
      else if (r.kind === 'key') { const bound = !r.capture && r.value && BOUND[r.value]; ctl = `${bound ? `<div class="game state conflict" style="width:auto;font-size:20px" title="${esc(bound)}">BOUND: ${esc(bound.toUpperCase())}</div>` : ''}<div class="game keybox${r.capture ? ' capture' : ''}" data-act="key">${r.capture ? 'PRESS A KEY' : esc(r.value || '—')}</div>`; }
      else if (r.kind === 'button') ctl = `<div class="game actbtn" data-act="button">${esc(r.btn || r.label)}</div>`;
      const suffix = r.needs === 'restart' ? ' <span style="font-size:20px;color:#b9c8e6">(AFTER RESTART)</span>' : r.needs === 'load' ? ' <span style="font-size:20px;color:#b9c8e6">(NEXT LOAD)</span>' : '';
      rows.push(`<div class="${cls.join(' ')}" data-row="${r.id}" draggable="${editMode}" tabindex="0"><div class="game lab">${esc(r.label)}${suffix}</div><div class="ctl">${ctl}</div></div>`);
    });
  });
  const rowsH = c.rowsBottom - c.rowsY - (tab.id === 'subtitles' ? 90 : 0);
  h.push(`<div class="abs rows" id="rows" style="left:${c.panel.x}px;top:${c.rowsY}px;width:${c.panel.w}px;height:${rowsH}px">${rows.join('')}</div>`);
  if (tab.id === 'subtitles') {
    const sz = rowByKey('size'), col = rowByKey('text_color'), nm = rowByKey('speaker_names'), op = rowByKey('background_opacity'), sc = rowByKey('speaker_colors'), en = rowByKey('enabled');
    const px = sz ? [19, 23, 27, 32, 38][Math.max(0, sz.r.options.indexOf(sz.r.value))] || 23 : 23;
    const colr = col && col.r.value === 'YELLOW' ? '#ffe74a' : '#fff'; const bgA = op ? op.r.value / 100 * 0.9 : .6;
    const inline = nm && nm.r.value === 'INLINE'; const noName = nm && nm.r.value === 'OFF';
    const nameCol = sc && sc.r.value ? '#ffc45a' : colr;
    h.push(`<div class="preview" style="top:${c.rowsY + rowsH + 8}px;background:rgba(13,11,26,${bgA.toFixed(2)});opacity:${en && !en.r.value ? .3 : 1}">${noName ? '' : inline ? `<span class="nm" style="color:${nameCol};font-size:${px}px">Trent: </span>` : `<div class="nm" style="color:${nameCol}">Trent</div>`}<span class="tx" style="font-size:${px}px;color:${colr}">Preview of the current settings: this is what a subtitle looks like.</span></div>`);
  }
  h.push(`<div class="abs hit" data-act="default" title="DEFAULT: this tab back to its defaults" style="left:${c.defX - 98}px;top:${c.btnY - 21}px;width:196px;height:42px"></div>`);
  h.push(`<div class="abs hit" data-act="cancel" title="CANCEL: every change since the page was entered" style="left:${c.canX - 98}px;top:${c.btnY - 21}px;width:196px;height:42px"></div>`);
  if (c.closeX) h.push(`<div class="abs hit" data-act="close" style="left:${c.closeX - 28}px;top:${c.closeY - 28}px;width:56px;height:56px"></div>`);
  if (padMode()) { h.push(`<div class="abs" style="left:0;top:0;width:60px;height:60px;background:url(${c.bg}) -12px -12px / 1920px 1080px"></div>`); } // the game's arrow cursor is hidden in pad mode
  h.push(promptBar([Pm(['DU', 'DD'], 'ROW'), Pm(['DL', 'DR'], 'CHANGE'), Pm('A', 'TOGGLE'), Pm(['LB', 'RB'], 'TAB'), Pm('X', 'DEFAULT'), Pm('B', 'BACK')], 'left'));
  h.push(`<div class="help" id="help"></div><div class="toast" id="toast"></div>`);
  stage.innerHTML = h.join('');
  wireOptions(stage, tab, c);
}
function wireOptions(stage, tab, c) {
  $$('.gamebtn', stage).forEach(el => el.addEventListener('click', () => toast(`${el.dataset.lab}: the game's own page (not mocked)`)));
  $('#flvBtn', stage).addEventListener('click', () => { snapshot = snapshotValues(); toast('FLVIBED page entered (CANCEL returns to these values)'); });
  $$('.tab', stage).forEach(el => {
    el.addEventListener('click', () => { if (editMode) { selected = { type: 'tab', id: el.dataset.tab }; } curTab = el.dataset.tab; focusRow = null; render(); });
    if (editMode) wireDrag(el, 'tab');
  });
  $$('.sec', stage).forEach(el => el.addEventListener('click', () => { if (editMode) { selected = { type: 'section', tab: tab.id, index: +el.dataset.sec }; render(); } }));
  $$('.row', stage).forEach(el => {
    const r = findRow(el.dataset.row).r;
    el.addEventListener('mouseenter', () => showHelp(r)); el.addEventListener('mouseleave', () => showHelp(null));
    el.addEventListener('focus', () => { focusRow = r.id; showHelp(r); el.classList.add('focus'); });
    el.addEventListener('click', (e) => {
      focusRow = r.id;
      if (editMode) { selected = { type: 'row', id: r.id }; render(); return; }
      const act = e.target.closest('[data-act]'); if (!act) { render(); return; } const a = act.dataset.act;
      if (a === 'toggle') setValue(r, !r.value, tab);
      else if (a === 'picker') stepPicker(r, 1, tab);
      else if (a === 'dec') stepSlider(r, -1, tab);
      else if (a === 'inc') stepSlider(r, 1, tab);
      else if (a === 'track') { const rect = act.getBoundingClientRect(); const f = Math.min(1, Math.max(0, (e.clientX - rect.left - 23 * scaleK) / ((285 - 46) * scaleK))); setValue(r, snap(r, r.min + f * (r.max - r.min)), tab); }
      else if (a === 'key') { allRows().forEach(x => delete x.r.capture); r.capture = true; render(); return; }
      else if (a === 'button') { if (r.key === 'edit_bindings') { view = 'bindings'; render(); return; } toast(r.key === 'reset_all' ? 'Every FLvibed setting would be reset now' : r.key === 'about' ? 'FLvibed 0.x · credits and licences' : (r.btn || r.label)); return; }
      render();
    });
    if (editMode) wireDrag(el, 'row');
    const knob = $('.knob', el);
    if (knob && !editMode) knob.addEventListener('pointerdown', (e) => {
      e.preventDefault(); const track = knob.parentElement; const rect = track.getBoundingClientRect(); knob.setPointerCapture(e.pointerId);
      const move = (ev) => { const f = Math.min(1, Math.max(0, (ev.clientX - rect.left - 23 * scaleK) / ((285 - 46) * scaleK))); r.value = snap(r, r.min + f * (r.max - r.min)); knob.style.left = (23 + (r.value - r.min) / ((r.max - r.min) || 1) * (285 - 46)) + 'px'; $('.cap', el).textContent = sliderWord(r); };
      const up = () => { knob.removeEventListener('pointermove', move); knob.removeEventListener('pointerup', up); setValue(r, r.value, tab); render(); };
      knob.addEventListener('pointermove', move); knob.addEventListener('pointerup', up);
    });
  });
  $$('[data-act=default]', stage).forEach(el => el.addEventListener('click', () => { tab.sections.forEach(s => s.rows.forEach(r => { if (r.kind !== 'button') r.value = r.def; })); if (tab.presetKey) syncPresetRow(tab); touched(); toast('DEFAULT: ' + tab.label + ' back to its defaults'); render(); }));
  $$('[data-act=cancel]', stage).forEach(el => el.addEventListener('click', () => { if (!snapshot) { toast('CANCEL: nothing to revert (click FLVIBED to enter the page first)'); return; } restoreValues(snapshot); touched(); toast('CANCEL: changes since entering the page reverted'); render(); }));
  $$('[data-act=close]', stage).forEach(el => el.addEventListener('click', () => toast('Closed: the changes are kept and written to the ini files')));
  if (focusRow) { const el = $(`.row[data-row="${focusRow}"]`, stage); if (el) { el.classList.add('focus'); showHelp(findRow(focusRow).r); } }
}

// keyboard on the stage (the pad's D-pad in the mock-up)
document.addEventListener('keydown', (e) => {
  const cap = allRows().find(x => x.r.capture);
  if (cap) { e.preventDefault(); const name = keyName(e); if (name === 'ESCAPE') { delete cap.r.capture; render(); return; } cap.r.value = name; delete cap.r.capture; touched(); render(); const b = BOUND[name]; toast(b ? `${name} is bound by the game (${b}): it would be refused` : `${name}: free`); return; }
  if (e.target.matches('input, textarea, select') || editMode) return;
  if (view !== 'main' && view !== 'ingame') return;
  const tab = tabOf(curTab); const vis = tab.sections.flatMap(s => s.rows).filter(r => (showAdv || r.tier !== 'advanced') && !parentOff(r));
  let i = vis.findIndex(r => r.id === focusRow);
  if (e.key === 'ArrowDown') { i = Math.min(vis.length - 1, i + 1); focusRow = vis[i].id; }
  else if (e.key === 'ArrowUp') { i = Math.max(0, i - 1); focusRow = vis[i].id; }
  else if (i >= 0 && e.key === 'ArrowRight') nudge(vis[i], 1, tab);
  else if (i >= 0 && e.key === 'ArrowLeft') nudge(vis[i], -1, tab);
  else if (i >= 0 && e.key === 'Enter') { if (vis[i].kind === 'key') { vis[i].capture = true; } else nudge(vis[i], 1, tab); }
  else if (e.key === 'PageDown' || e.key === 'PageUp') { const ti = spec.tabs.findIndex(t => t.id === curTab); curTab = spec.tabs[(ti + (e.key === 'PageDown' ? 1 : spec.tabs.length - 1)) % spec.tabs.length].id; focusRow = null; }
  else return;
  e.preventDefault(); render();
  const el = $(`.row[data-row="${focusRow}"]`); if (el) el.scrollIntoView({ block: 'nearest' });
});
function keyName(e) {
  let k = e.key.length === 1 ? e.key.toUpperCase() : e.key.toUpperCase();
  if (k === ' ') k = 'SPACE';
  if (e.code === 'NumpadAdd') k = 'NUMPAD+'; if (e.code === 'NumpadSubtract') k = 'NUMPAD-';
  const mods = []; if (e.ctrlKey && k !== 'CONTROL') mods.push('CTRL'); if (e.altKey && k !== 'ALT') mods.push('ALT'); if (e.shiftKey && k !== 'SHIFT') mods.push('SHIFT');
  if (['CONTROL', 'ALT', 'SHIFT'].includes(k)) return k;
  return mods.length ? mods.join('+') + '+' + k : k;
}
let dragInfo = null;
function wireDrag(el, type) {
  el.addEventListener('dragstart', (e) => { dragInfo = { type, id: el.dataset.row || el.dataset.tab }; e.dataTransfer.effectAllowed = 'move'; });
  el.addEventListener('dragover', (e) => { if (dragInfo && dragInfo.type === type) { e.preventDefault(); el.classList.add('dragover'); } });
  el.addEventListener('dragleave', () => el.classList.remove('dragover'));
  el.addEventListener('drop', (e) => { e.preventDefault(); el.classList.remove('dragover'); if (!dragInfo || dragInfo.type !== type) return; if (type === 'row') moveRowBefore(dragInfo.id, el.dataset.row); else moveTabBefore(dragInfo.id, el.dataset.tab); dragInfo = null; touched(); render(); });
}
function detachRow(id) { for (const t of spec.tabs) for (const s of t.sections) { const i = s.rows.findIndex(r => r.id === id); if (i >= 0) return s.rows.splice(i, 1)[0]; } return null; }
function moveRowBefore(id, beforeId) { if (id === beforeId) return; const r = detachRow(id); if (!r) return; for (const t of spec.tabs) for (const s of t.sections) { const i = s.rows.findIndex(x => x.id === beforeId); if (i >= 0) { s.rows.splice(i, 0, r); return; } } }
function moveTabBefore(id, beforeId) { if (id === beforeId) return; const i = spec.tabs.findIndex(t => t.id === id); const t = spec.tabs.splice(i, 1)[0]; const j = spec.tabs.findIndex(x => x.id === beforeId); spec.tabs.splice(j, 0, t); }

// ================================================================== render: the screens (every game screen with its pad layout)
// ================================================================== render: the maneuver bar
// ================================================================== render: the bindings diagram
const DIAG = { // label anchor positions on the 1920x900 diagram: [part x, part y, label x, label y, anchor side]
  LT: [690, 120, 300, 110, 'l'], LB: [690, 190, 300, 185, 'l'], VIEW: [860, 330, 300, 330, 'l'], LS: [690, 420, 300, 420, 'l'], L3: [690, 420, 300, 470, 'l'], DU: [800, 560, 300, 545, 'l'], DD: [800, 640, 300, 595, 'l'], DL: [760, 600, 300, 645, 'l'], DR: [840, 600, 300, 695, 'l'],
  RT: [1230, 120, 1620, 110, 'r'], RB: [1230, 190, 1620, 185, 'r'], MENU: [1060, 330, 1620, 290, 'r'], Y: [1230, 350, 1620, 365, 'r'], X: [1170, 410, 1620, 435, 'r'], B: [1290, 410, 1620, 505, 'r'], A: [1230, 470, 1620, 575, 'r'], RS: [1120, 600, 1620, 655, 'r'], R3: [1120, 600, 1620, 720, 'r']
};
function renderBindings(stage) {
  const ctx = curCtx; const bd = bindingsFor(ctx); const set = dev() === 'km' ? 'xbox' : dev();
  const lab = (a) => ACTIONS[a] || a;
  const svg = [];
  svg.push(`<svg width="1920" height="900" viewBox="0 0 1920 900">`);
  svg.push(`<path class="dbody" d="M640 260 Q620 140 760 130 L1160 130 Q1300 140 1280 260 L1330 520 Q1340 640 1250 640 L1180 640 Q1120 560 1040 560 L880 560 Q800 560 740 640 L670 640 Q580 640 590 520 Z"/>`);
  svg.push(`<rect class="dpart${bindSel === 'LT' ? ' sel' : ''}" data-part="LT" x="640" y="95" width="100" height="40" rx="12"/><rect class="dpart${bindSel === 'RT' ? ' sel' : ''}" data-part="RT" x="1180" y="95" width="100" height="40" rx="12"/>`);
  svg.push(`<rect class="dpart${bindSel === 'LB' ? ' sel' : ''}" data-part="LB" x="640" y="170" width="100" height="36" rx="8"/><rect class="dpart${bindSel === 'RB' ? ' sel' : ''}" data-part="RB" x="1180" y="170" width="100" height="36" rx="8"/>`);
  svg.push(`<circle class="dpart${bindSel === 'LS' || bindSel === 'L3' ? ' sel' : ''}" data-part="LS" cx="690" cy="420" r="58"/><circle class="dpart${bindSel === 'RS' || bindSel === 'R3' ? ' sel' : ''}" data-part="RS" cx="1120" cy="600" r="58"/>`);
  svg.push(`<rect class="dpart${bindSel === 'VIEW' ? ' sel' : ''}" data-part="VIEW" x="840" y="315" width="40" height="30" rx="6"/><rect class="dpart${bindSel === 'MENU' ? ' sel' : ''}" data-part="MENU" x="1040" y="315" width="40" height="30" rx="6"/>`);
  [['Y', 1230, 350], ['X', 1170, 410], ['B', 1290, 410], ['A', 1230, 470]].forEach(([k, x, y]) => svg.push(`<circle class="dpart${bindSel === k ? ' sel' : ''}" data-part="${k}" cx="${x}" cy="${y}" r="30" style="stroke:${GLYPHSETS[set][k][1]}"/><text x="${x}" y="${y + 9}" text-anchor="middle" class="dlab key" style="font-size:28px;fill:${GLYPHSETS[set][k][1]}">${GLYPHSETS[set][k][0]}</text>`));
  svg.push(`<path class="dpart${['DU', 'DD', 'DL', 'DR'].includes(bindSel) ? ' sel' : ''}" data-part="DU" d="M780 540 h40 v40 h40 v40 h-40 v40 h-40 v-40 h-40 v-40 h40 z"/>`);
  Object.entries(DIAG).forEach(([k, [px, py, lx, ly, side]]) => {
    const b = bd[k] || Bd(); const parts = [lab(b.tap)]; if (b.hold !== 'none') parts.push('hold: ' + lab(b.hold)); if (b.dbl !== 'none') parts.push('×2: ' + lab(b.dbl));
    const name = k === 'L3' ? 'press ' + glyphName('LS', set) : k === 'R3' ? 'press ' + glyphName('RS', set) : ['LS', 'RS'].includes(k) ? glyphName(k, set) + ' stick' : glyphName(k, set);
    const tx = side === 'l' ? lx : lx; const anchor = side === 'l' ? 'end' : 'start';
    svg.push(`<path class="dline" d="M${px} ${py} L${side === 'l' ? lx + 20 : lx - 20} ${ly}"/>`);
    svg.push(`<text x="${tx}" y="${ly - 4}" text-anchor="${anchor}" class="dlab key${bindSel === k ? ' sel' : ''}" data-part="${k}" style="cursor:pointer">${esc(name)}</text>`);
    svg.push(`<text x="${tx}" y="${ly + 18}" text-anchor="${anchor}" class="dlab${bindSel === k ? ' sel' : ''}" style="font-size:21px">${esc(parts[0])}</text>`);
    if (parts[1]) svg.push(`<text x="${tx}" y="${ly + 38}" text-anchor="${anchor}" class="dlab sub">${esc(parts[1])}${parts[2] ? ' · ' + esc(parts[2]) : ''}</text>`);
  });
  svg.push(`<text x="960" y="60" text-anchor="middle" class="dlab" style="font-size:34px">${esc(CONTEXTS.find(c => c[0] === ctx)[1])} · ${esc(ctx === 'flight' ? spec.bindings.preset + ' PRESET' : 'ALL PRESETS')} · ${esc(GLYPHSETS[set].name)} GLYPHS</text>`);
  svg.push(`<text x="960" y="880" text-anchor="middle" class="dlab sub">Click a part or a label to ${ro() ? 'see what it does' : 'edit it in the drawer'}. ${ctx === 'flight' ? 'The sticks keep flying while the wheel or the picker is open; any stick past the hand-back threshold stops the autopilot.' : ''}</text>`);
  svg.push(`</svg>`);
  stage.innerHTML = `<div class="abs" style="left:0;top:0;width:1920px;height:900px">${svg.join('')}</div><div class="toast" id="toast" style="top:820px"></div>`;
  $$('[data-part]', stage).forEach(el => el.addEventListener('click', () => { let k = el.dataset.part; if (k === 'LS' && bindSel === 'LS') k = 'L3'; else if (k === 'RS' && bindSel === 'RS') k = 'R3'; else if (k === 'DU' && bindSel === 'DU') k = 'DD'; else if (k === 'DD' && bindSel === 'DD') k = 'DL'; else if (k === 'DL' && bindSel === 'DL') k = 'DR'; bindSel = k; render(); }));
}

// ================================================================== render: the decisions stage
// ================================================================== stage scaling
let scaleK = 1;
function fitStage() { const wrap = $('#stagewrap'), st = $('#stage'); const w = wrap.clientWidth; scaleK = w / 1920; st.style.transform = `scale(${scaleK})`; wrap.style.height = (st.offsetHeight * scaleK) + 'px'; }
window.addEventListener('resize', fitStage);
