// ================================================================== render: the spec document (DOC, part2b_doc.js)
const DOC_IDS = DOC.map(s => s.id);
const VIEW_PARENT = { main: 'options', ingame: 'options', bindings: 'controls' };
function isDocView(v) { return DOC_IDS.includes(v); }
function rowOptions(r) {
  if (r.kind === 'toggle') return 'ON / OFF';
  if (r.kind === 'picker') return r.options.map(esc).join(' / ');
  if (r.kind === 'slider') return `${r.min} .. ${r.max}${r.unit ? ' ' + esc(r.unit) : ''}`;
  if (r.kind === 'key') return 'a key';
  return '';
}
function rowVal(r, v) { return r.kind === 'toggle' ? (v ? 'ON' : 'OFF') : v === null || v === undefined ? '' : esc(String(v)) + (r.kind === 'slider' && r.unit ? ' ' + esc(r.unit) : ''); }
function settingsRows(rows) {
  return `<div class="tscroll"><table class="doc"><thead><tr><th>Setting</th><th>Choices</th><th>Default</th><th>What it does</th></tr></thead><tbody>` + rows.map(x => {
    const r = x.r || x; const ch = isChanged(r);
    return `<tr id="row-${esc(r.key || r.id || '')}"${r.tier === 'advanced' ? ' class="adv"' : ''}><td><b>${esc(r.label)}</b>${r.key ? `<br><code>${esc(r.key)}</code>` : ''}${r.tier === 'advanced' ? ' <span class="tag">advanced</span>' : r.tier !== 'simple' ? ' <span class="tag" title="Shown on the options page once Show all settings (MORE) is on">all settings</span>' : ''}</td><td>${rowOptions(r)}</td><td>${rowVal(r, r.def)}${ch ? (ro() ? `<br><span class="tag warn" title="The current setting differs from the default">current: ${rowVal(r, r.value)}</span>` : `<br><span class="tag warn" title="The value saved in this spec differs from the default">spec: ${rowVal(r, r.value)}</span>`) : ''}</td><td>${esc(r.help || '')}</td></tr>`;
  }).join('') + `</tbody></table></div>`;
}
function bindTable(map) {
  const lab = a => ACTIONS[a] || a;
  return `<div class="tscroll"><table class="doc binds"><thead><tr><th>Button</th><th>Press</th><th>Hold</th><th>Double</th></tr></thead><tbody>` + INPUTS.map(k => {
    const b = map[k]; if (!b || (b.tap === 'none' && b.hold === 'none' && b.dbl === 'none')) return '';
    return `<tr><td class="gcell">${glyph(k, dev() === 'km' ? 'xbox' : dev())}</td><td>${b.tap === 'none' ? '' : esc(lab(b.tap))}</td><td>${b.hold === 'none' ? '' : esc(lab(b.hold))}</td><td>${b.dbl === 'none' ? '' : esc(lab(b.dbl))}</td></tr>`;
  }).join('') + `</tbody></table></div>`;
}
let docPreset = null;
function renderBinds() {
  const names = Object.keys(spec.bindings.presets); const cur = docPreset && names.includes(docPreset) ? docPreset : spec.bindings.preset;
  const blurb = { FREELANCER: 'The default: right stick steers with gimbal lock-on, left stick is the throttle lever and strafe, triggers fire and boost, the D-pad holds the consumables.', ACE: 'Arcade flight for Ace Combat hands: left stick steers, right stick strafes, the triggers are the throttle (afterburner at full pull, reverse at full brake), A guns, B missiles (B held: the torpedo).', ASSISTED: 'The autopilot flies and fights; you aim (right stick), fire, pick targets and decide; RB missiles (RB held: the torpedo). Built on the turret view.' };
  const h = [`<h3>Flight layouts (controller)</h3><div class="seg" role="tablist">` + names.map(n => `<button data-preset="${n}" class="${n === cur ? 'on' : ''}" role="tab" aria-selected="${n === cur}">${esc(n)}${n === spec.bindings.preset ? (ro() ? ' (current)' : ' (spec)') : ''}</button>`).join('') + `</div><p class="small">${esc(blurb[cur] || '')}${dev() === 'km' ? ' Glyphs shown as Xbox; pick a pad with the Input buttons.' : ''}</p>`];
  h.push(fold(`Every binding in the ${esc(cur)} layout`, bindTable(spec.bindings.presets[cur].flight)));
  h.push(`<details class="more"><summary>Every other situation (shared by all layouts)</summary>` + CONTEXTS.filter(c => c[0] !== 'flight').map(c => `<h4>${esc(c[1])}</h4>` + bindTable(spec.bindings.common[c[0]] || {})).join('') + `</details>`);
  h.push(`<p><button class="go" data-go="bindings">Open the interactive bindings diagram${ro() ? '' : ' and editor'}</button></p>`);
  return h.join('');
}
function renderKeys() {
  const pick = ['F2', 'F3', 'F4', 'SHIFT+W', 'TAB', 'Z', 'X', 'R', 'CTRL+R', 'T', 'CTRL+T', 'G', 'F', 'C', 'E', 'Q', 'SHIFT+B', 'H', 'F6', 'F7', 'F1'];
  const ak = (rowByKey('autopilot_key') || { r: { value: 'K' } }).r.value;
  return `<h3>Keyboard and mouse</h3><p>The game's own keys and mouse flight, unchanged. FLvibed adds the autopilot key and the maneuver bar's fifth button; on HUD 2 (ALWAYS) the HUD shows each action's key as a keycap read from your live key map, and its chips, list rows and gun groups click.</p><div class="tscroll"><table class="doc"><thead><tr><th>Key</th><th>Action (the game's defaults)</th></tr></thead><tbody><tr><td><span class="g km">${esc(ak)}</span></td><td><b>FLvibed autopilot on / off</b> (<code>autopilot_key</code>)</td></tr>` + pick.filter(k => BOUND[k]).map(k => `<tr><td><span class="g km">${esc(k)}</span></td><td>${esc(BOUND[k])}</td></tr>`).join('') + `</tbody></table></div>`;
}
function renderTabsDoc() {
  const h = [`<p class="btnrow"><button class="go" data-go="main">Try the page: main menu Options</button><button class="go" data-go="ingame">Try the page: in-game Options (F1)</button></p>`];
  spec.tabs.forEach(t => {
    h.push(`<h3>${esc(t.label)}${t.presetKey ? ' <span class="tag">has presets</span>' : ''}</h3>`);
    const inner = [], n = t.sections.reduce((a, s) => a + s.rows.filter(r => r.kind !== 'button').length, 0);
    t.sections.forEach(s => { const rows = s.rows.filter(r => r.kind !== 'button'); if (!rows.length) return; if (s.label) inner.push(`<h4>${esc(s.label)}</h4>`); inner.push(settingsRows(rows)); });
    h.push(fold(`All ${n} settings on the ${esc(t.label)} tab`, inner.join('')));
  });
  return h.join('');
}
// Lighter reading (the owner, 11 Nov 2026: "a little less crowded, a tad less dense"): reference (long tables, settings,
// bindings) folds behind one line; a caption shows its first sentence, the rest on a tap.
function fold(summary, inner, open) { return `<details class="more"${open ? ' open' : ''}><summary>${summary}</summary><div class="foldin">${inner}</div></details>`; }
function capHtml(cap) {
  if (!cap) return '';
  const m = /^(.{40,}?[.!?)])\s+(?=[A-Z(\[])/.exec(cap);
  if (!m || cap.length - m[1].length < 40) return esc(cap);
  return `${esc(m[1])} <details class="capmore"><summary>More</summary>${esc(cap.slice(m[0].length))}</details>`;
}
// A link to a settings row, #row-<key>: the section that lists it (its settings block, else the options page).
function sectionOfRow(key) {
  const s = DOC.find(x => x.blocks.some(b => b.t === 'settings' && b.keys.includes(key)));
  return s ? s.id : (rowByKey(key) ? 'options' : null);
}
let pendingRow = null; // set from the URL's #row-<key> at load and on hashchange (render() rewrites the hash to the view)
function takeRowHash() {
  const h = (location.hash || '').slice(1); if (!h.startsWith('row-')) return false;
  const v = sectionOfRow(h.slice(4)); if (!v) return false;
  pendingRow = h; pendingSec = v; view = v; return true;
}
let pendingSec = null;
window.addEventListener('hashchange', () => { if (takeRowHash()) render(); });
// (kept while its section shows: the page renders again once the store answers; leaving the section drops it)
function openDeepLink() {
  if (pendingRow && view !== pendingSec) pendingRow = null;
  const h = pendingRow; if (!h) return;
  const el = document.getElementById(h); if (!el) return;
  for (let d = el.closest('details'); d; d = d.parentElement && d.parentElement.closest('details')) d.open = true;
  el.classList.add('hit'); el.scrollIntoView({ block: 'center' });
}
function cmpHtml(title, cl, h2, cap, i, ll, rl) {
  ll = ll || 'DEFAULT'; rl = rl || 'HUD 2';
  return `<figure class="cmp"><figcaption><b>${esc(title)}</b></figcaption><div class="cstage" style="--pos:50%"><img src="${h2}" alt="${esc(title)}: ${esc(rl)}" loading="lazy" width="1600" height="900"><div class="before"><img src="${cl}" alt="${esc(title)}: ${esc(ll)}" loading="lazy" width="1600" height="900"></div><span class="lab l">${esc(ll)}</span><span class="lab r">${esc(rl)}</span><div class="handle"></div><input type="range" min="0" max="100" value="50" id="cmp_${i}" aria-label="Drag to compare ${esc(ll)} and ${esc(rl)}: ${esc(title)}"></div>${cap ? `<div class="cap">${capHtml(cap)}</div>` : ''}</figure>`;
}
// The Input switch (toolbar): K&M or a pad family. Blocks may carry `only: 'pad' | 'km'` and a `km: {...}` variant (the
// same fields, used while K&M is chosen); gallery items `km: {img, title, cap}`. Every section says how the device changes
// it (`dv`: {same} or {pad, km}), so the switch always shows its effect.
function isKM() { return dev() === 'km'; }
function devLabel() { return { km: 'keyboard and mouse', xbox: 'an Xbox controller', ps: 'a PlayStation controller', switch: 'a Nintendo Switch controller', steam: 'a Steam Deck or Steam Controller' }[dev()] || dev(); }
function forDev(b) { return isKM() && b.km ? Object.assign({}, b, b.km) : b; }
function devNote(s) {
  if (!s.dv) return '';
  const txt = s.dv.same ? s.dv.same : isKM() ? s.dv.km : s.dv.pad;
  return `<p class="devnote"><span class="tag">${s.dv.same ? 'same on every device' : 'shown for ' + esc(devLabel())}</span> ${txt}</p>`;
}
function renderDocSection(id) {
  const s = DOC.find(x => x.id === id) || DOC[0]; let n = 0;
  const h = [`<article class="docsec"><h2>${esc(s.title)}</h2><p class="lead">${s.lead}</p>${devNote(s)}`];
  s.blocks.forEach(b0 => {
    if (b0.only && (b0.only === 'km') !== isKM()) return;
    const b = forDev(b0);
    if (b.t === 'p') h.push(`<p>${b.html}</p>`);
    else if (b.t === 'table') {
      const tbl = `<div class="tscroll"><table class="doc"><thead><tr>${b.head.map(x => `<th>${esc(x)}</th>`).join('')}</tr></thead><tbody>${b.rows.map(r => `<tr>${r.map(c => `<td>${c}</td>`).join('')}</tr>`).join('')}</tbody></table></div>`;
      // short tables stay open; long ones fold behind their title (b.fold forces either way, b.summary words the line)
      const folded = b.fold !== undefined ? b.fold : b.rows.length > 5;
      h.push(folded ? fold(b.summary || `${esc(b.title || 'The table')} <span class="cnt">${b.rows.length} rows</span>`, tbl) : `${b.title ? `<h3>${esc(b.title)}</h3>` : ''}${tbl}`);
    }
    else if (b.t === 'fold') h.push(fold(b.title, `<p>${b.html}</p>`));
    else if (b.t === 'cmp') h.push(cmpHtml(b.title, b.cl, b.h2, b.cap, n++, b.ll, b.rl));
    else if (b.t === 'gal') {
      const items = b.items.filter(g0 => !g0.only || (g0.only === 'km') === isKM());
      const grid = `<div class="gal">${items.map(g0 => { const g = forDev(g0); return `<figure><img src="${g.img}" alt="${esc(g.title)}" loading="lazy"><figcaption><b>${esc(g.title)}</b>${g.cap ? ' ' + capHtml(g.cap) : ''}</figcaption></figure>`; }).join('')}</div>`;
      h.push(b.fold ? fold(`${esc(b.title || 'More pictures')} <span class="cnt">${items.length} pictures</span>`, grid) : `${b.title ? `<h3>${esc(b.title)}</h3>` : ''}${grid}`);
    }
    else if (b.t === 'family') {
      const grid = `<p>${esc(b.text)}</p><div class="famgrid">${b.items.map(([k, t, c]) => cmpHtml(t, CAP('x_' + k + '_cl'), CAP('x_' + k + '_h2'), c, n++, 'CLASSIC', 'HUD 2')).join('')}</div>`;
      h.push(b.fold ? fold(`${esc(b.title)} <span class="cnt">${b.items.length} comparisons</span>`, grid) : `<section class="fam"><h3>${esc(b.title)}</h3>${grid}</section>`);
    }
    else if (b.t === 'settings') { const rows = b.keys.map(k => rowByKey(k)).filter(Boolean); h.push(rows.length > 5 ? fold(`All ${rows.length} settings: ${esc(b.title || 'Settings')}`, settingsRows(rows)) : `<h3>${esc(b.title || 'Settings')}</h3>` + settingsRows(rows)); }
    else if (b.t === 'binds') h.push(isKM() ? `<details class="more"><summary>The controller layouts (shown with Xbox glyphs; pick a pad with Input)</summary>${renderBinds()}</details>` : renderBinds());
    else if (b.t === 'keys') h.push(isKM() ? renderKeys() : `<details class="more"><summary>Keyboard and mouse (choose K&amp;M under Input to put it first)</summary>${renderKeys()}</details>`);
    else if (b.t === 'tabs') h.push(renderTabsDoc());
    else if (b.t === 'dev') h.push(`<details class="devblk"><summary>For developers</summary>${b.html}</details>`);
  });
  const i = DOC_IDS.indexOf(s.id), nx = DOC[i + 1];
  if (nx) h.push(`<p class="next"><button class="go" data-go="${nx.id}">Next: ${esc(nx.nav)} &rarr;</button></p>`);
  h.push(`</article>`);
  return h.join('');
}
function renderDoc() {
  const el = $('#doc'); el.innerHTML = renderDocSection(view);
  $$('.cstage input', el).forEach(inp => inp.addEventListener('input', () => inp.parentElement.style.setProperty('--pos', inp.value + '%')));
  $$('[data-go]', el).forEach(b => b.addEventListener('click', () => { view = b.dataset.go; selected = null; render(); window.scrollTo(0, 0); }));
  $$('[data-preset]', el).forEach(b => b.addEventListener('click', () => { docPreset = b.dataset.preset; renderDoc(); }));
  openDeepLink();
}
