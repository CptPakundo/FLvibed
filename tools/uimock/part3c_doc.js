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
    return `<tr${r.tier === 'advanced' ? ' class="adv"' : ''}><td><b>${esc(r.label)}</b>${r.key ? `<br><code>${esc(r.key)}</code>` : ''}${r.tier === 'advanced' ? ' <span class="tag">advanced</span>' : ''}</td><td>${rowOptions(r)}</td><td>${rowVal(r, r.def)}${ch ? (ro() ? `<br><span class="tag warn" title="The current setting differs from the default">current: ${rowVal(r, r.value)}</span>` : `<br><span class="tag warn" title="The value saved in this spec differs from the default">spec: ${rowVal(r, r.value)}</span>`) : ''}</td><td>${esc(r.help || '')}</td></tr>`;
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
  const blurb = { FREELANCER: 'The default: right stick steers with gimbal lock-on, left stick is the throttle lever and strafe, triggers fire and boost, the D-pad holds the consumables.', ACE: 'Arcade flight for Ace Combat hands: left stick steers, right stick strafes, the triggers are the throttle (afterburner at full pull, reverse at full brake), A guns, B missiles.', ASSISTED: 'The autopilot flies and fights; you aim (right stick), fire, pick targets and decide. Built on the turret view.' };
  const h = [`<h3>Flight layouts (controller)</h3><div class="seg" role="tablist">` + names.map(n => `<button data-preset="${n}" class="${n === cur ? 'on' : ''}" role="tab" aria-selected="${n === cur}">${esc(n)}${n === spec.bindings.preset ? (ro() ? ' (current)' : ' (spec)') : ''}</button>`).join('') + `</div><p class="small">${esc(blurb[cur] || '')}${dev() === 'km' ? ' Glyphs shown as Xbox; pick a pad with the Input buttons.' : ''}</p>`];
  h.push(bindTable(spec.bindings.presets[cur].flight));
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
    t.sections.forEach(s => { const rows = s.rows.filter(r => r.kind !== 'button'); if (!rows.length) return; if (s.label) h.push(`<h4>${esc(s.label)}</h4>`); h.push(settingsRows(rows)); });
  });
  return h.join('');
}
function cmpHtml(title, cl, h2, cap, i, ll, rl) {
  ll = ll || 'DEFAULT'; rl = rl || 'HUD 2';
  return `<figure class="cmp"><figcaption><b>${esc(title)}</b></figcaption><div class="cstage" style="--pos:50%"><img src="${h2}" alt="${esc(title)}: ${esc(rl)}" loading="lazy" width="1600" height="900"><div class="before"><img src="${cl}" alt="${esc(title)}: ${esc(ll)}" loading="lazy" width="1600" height="900"></div><span class="lab l">${esc(ll)}</span><span class="lab r">${esc(rl)}</span><div class="handle"></div><input type="range" min="0" max="100" value="50" id="cmp_${i}" aria-label="Drag to compare ${esc(ll)} and ${esc(rl)}: ${esc(title)}"></div>${cap ? `<p class="cap">${esc(cap)}</p>` : ''}</figure>`;
}
// The Input switch (toolbar): K&M or a pad family. Blocks may carry `only: 'pad' | 'km'` and a `km: {...}` variant (the
// same fields, used while K&M is chosen); gallery items `km: {img, title, cap}`. Every section says how the device changes
// it (`dv`: {same} or {pad, km}), so the switch always shows its effect.
function isKM() { return dev() === 'km'; }
function devLabel() { return { km: 'keyboard and mouse', xbox: 'an Xbox controller', ps: 'a PlayStation controller', switch: 'a Nintendo Switch controller' }[dev()] || dev(); }
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
    else if (b.t === 'table') h.push(`${b.title ? `<h3>${esc(b.title)}</h3>` : ''}<div class="tscroll"><table class="doc"><thead><tr>${b.head.map(x => `<th>${esc(x)}</th>`).join('')}</tr></thead><tbody>${b.rows.map(r => `<tr>${r.map(c => `<td>${c}</td>`).join('')}</tr>`).join('')}</tbody></table></div>`);
    else if (b.t === 'cmp') h.push(cmpHtml(b.title, b.cl, b.h2, b.cap, n++, b.ll, b.rl));
    else if (b.t === 'gal') h.push(`${b.title ? `<h3>${esc(b.title)}</h3>` : ''}<div class="gal">${b.items.filter(g0 => !g0.only || (g0.only === 'km') === isKM()).map(g0 => { const g = forDev(g0); return `<figure><img src="${g.img}" alt="${esc(g.title)}" loading="lazy"><figcaption><b>${esc(g.title)}</b>${g.cap ? ' ' + esc(g.cap) : ''}</figcaption></figure>`; }).join('')}</div>`);
    else if (b.t === 'family') h.push(`<section class="fam"><h3>${esc(b.title)}</h3><p>${esc(b.text)}</p><div class="famgrid">${b.items.map(([k, t, c]) => cmpHtml(t, CAP('x_' + k + '_cl'), CAP('x_' + k + '_h2'), c, n++, 'CLASSIC', 'HUD 2')).join('')}</div></section>`);
    else if (b.t === 'settings') { const rows = b.keys.map(k => rowByKey(k)).filter(Boolean); h.push(`<h3>${esc(b.title || 'Settings')}</h3>` + settingsRows(rows)); }
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
}
