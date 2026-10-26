// The speedrun comparison table in docs\LEGS.md, regenerated from the leg history (user, 22 Oct 2026: an ever-updating
// table of leg and whole-run times, speedruns vs the autopilot's current best, organized for the training sessions).
// Sources: docs\legs_reference.tsv (the top-3 runs' leg times, speedrun.md V2; scene-free = minus the room scenes the
// runners watched and our autopilot skips) and research\experience\legs\legs.tsv (every leg run; PASS rows count). A
// full leg is a PASS from the leg's start save; a run from a checkpoint (M13_ck_st03.fl ...) is listed apart.
// Writes the block between "<!-- legtable:begin -->" and "<!-- legtable:end -->" in docs\LEGS.md. flleg.ps1 runs it
// after every leg. Usage: node tools\legtable.js
const fs = require('fs');
const path = require('path');
const root = path.join(__dirname, '..');
const refPath = path.join(root, 'docs', 'legs_reference.tsv');
const histPath = path.join(root, 'research', 'experience', 'legs', 'legs.tsv');
const docPath = path.join(root, 'docs', 'LEGS.md');
// Written whole by a rename (session 8, 23 Oct 2026: three legs ending together ran this at once - one read LEGS.md
// half-written and stopped on 'no legtable markers')
const writeWhole = (file, text) => {
  const tmp = `${file}.${process.pid}.tmp`;
  fs.writeFileSync(tmp, text);
  fs.renameSync(tmp, file);
};

const rows = (file) => {
  const lines = fs.readFileSync(file, 'utf8').split(/\r?\n/).filter((l) => l.trim());
  const head = lines[0].split('\t');
  return lines.slice(1).map((l) => Object.fromEntries(l.split('\t').map((v, i) => [head[i], v])));
};
const ref = rows(refPath);
const hist = fs.existsSync(histPath) ? rows(histPath).filter((r) => r.outcome === 'PASS' && +r.secs > 0) : [];
const bestBy = (pred) => hist.filter(pred).reduce((b, r) => (!b || +r.secs < +b.secs ? r : b), null);

const out = [];
out.push('| Leg | WR | Best of 3 | Scene-free | Ours (best full leg) | To go vs scene-free | Set | Commit |');
out.push('|---|---|---|---|---|---|---|---|');
let sWr = 0, sB3 = 0, sSf = 0, sOurs = 0, sSfCovered = 0, missing = [];
for (const r of ref) {
  const b = bestBy((h) => h.save.toLowerCase() === r.start_save.toLowerCase());
  sWr += +r.wr; sB3 += +r.best3; sSf += +r.scenefree;
  let ours = '-', togo = '-', set = '-', commit = '-';
  if (b) {
    ours = b.secs; togo = String(+b.secs - +r.scenefree); set = b.date; commit = b.commit;
    sOurs += +b.secs; sSfCovered += +r.scenefree;
  } else missing.push(r.leg);
  out.push(`| ${r.leg} | ${r.wr} | ${r.best3} | ${r.scenefree} | ${ours} | ${togo} | ${set} | ${commit} |`);
}
out.push(`| **Mission legs** | **${sWr}** | **${sB3}** | **${sSf}** | **${sOurs}**${missing.length ? ` (missing: ${missing.join(', ')})` : ''} | **${sOurs - sSfCovered}** (over the legs we have) | | |`);
out.push('');
out.push('Whole run (WR 4:02:48 = 14,568 s; the three runs\' best legs sum to 14,002 s; the free-time legs FT1-FT12 are not');
out.push('built yet - runners\' best 2,876 s, ~2,200 s scene-free; L00 140 s): ours = mission legs only until the chain runs.');
out.push('');
out.push('Checkpoint bests (partial legs from `research\\saves\\legs\\`; compare with `ref.py <leg> <section>`):');
out.push('');
out.push('| Checkpoint | Best (s) | Set | Commit |');
out.push('|---|---|---|---|');
const cks = [...new Set(hist.map((h) => h.save).filter((s) => /_ck_/i.test(s)))].sort();
for (const s of cks) {
  const b = bestBy((h) => h.save === s);
  out.push(`| ${s.replace(/\.fl$/i, '')} | ${b.secs} | ${b.date} | ${b.commit} |`);
}
// Personal-best history per leg (user, 22 Oct 2026: "the autopilot's personal best should include the variables /
// details / changes that enabled an improvement over the previous best"): every new best of a full leg with its gain,
// its commit and what changed since the previous best - the commit subjects between the two (git log), plus a note
// from docs\legs_notes.tsv when one is written (leg, date as in legs.tsv, note: the setting / behaviour behind it).
// "+dirty" = run on uncommitted changes (those are in the next commit's subject).
const { execFileSync } = require('child_process');
const notesPath = path.join(root, 'docs', 'legs_notes.tsv');
const notes = fs.existsSync(notesPath) ? rows(notesPath) : [];
const subjects = (from, to) => {
  const a = from.replace(/\+dirty$/, ''), b = to.replace(/\+dirty$/, '');
  if (!a || !b || a === b) return b && to.endsWith('+dirty') ? ['(uncommitted changes on ' + b + ')'] : [];
  try {
    // (code commits only: the autopilot, the shared game code, the leg tools - docs commits are noise here)
    const s = execFileSync('git', ['-C', root, 'log', '--format=%h %s', `${a}..${b}`, '--', 'controller/src', 'core',
                                   'controller/tools'], { encoding: 'utf8' })
      .trim()
      .split(/\r?\n/);
    return s.filter(Boolean).map((l) => l.slice(0, 110));
  } catch (e) {
    return [];
  }
};
out.push('');
out.push('Personal-best history (full legs; each new best, its gain and what changed since the previous best - commit subjects');
out.push('between the two runs, newest first, at most 4; notes from `docs\\legs_notes.tsv`):');
out.push('');
out.push('| Leg | Date | Best (s) | Gain (s) | Commit | What changed since the previous best |');
out.push('|---|---|---|---|---|---|');
for (const r of ref) {
  const runs = hist.filter((h) => h.save.toLowerCase() === r.start_save.toLowerCase()).sort((x, y) => (x.date < y.date ? -1 : 1));
  let best = null;
  for (const h of runs) {
    if (best && +h.secs >= +best.secs) continue;
    const note = notes.find((n) => n.leg === r.leg && n.date === h.date);
    let what = best ? subjects(best.commit, h.commit) : ['first full pass'];
    if (what.length > 4) what = what.slice(0, 4).concat([`(+${what.length - 4} more)`]);
    if (note) what.unshift(`**${note.note}**`);
    out.push(`| ${r.leg} | ${h.date} | ${h.secs} | ${best ? +best.secs - +h.secs : '-'} | ${h.commit} | ${what.join('; ') || '-'} |`);
    best = h;
  }
}
out.push('');
const now = new Date(Date.now() - new Date().getTimezoneOffset() * 60000); // local time
out.push(`(generated by tools\\legtable.js from legs.tsv, ${now.toISOString().slice(0, 16).replace('T', ' ')})`);

// The leg dossier (docs\LEG_DOSSIER.md, user, 22 Oct 2026: a data- and context-rich comparison per leg): each leg's
// "<!-- legtimes:<leg>:begin -->" ... "end" block gets the current times - the three runs and our best full leg with
// its gain history - so the hand-written context around it never carries stale numbers.
const dossierPath = path.join(root, 'docs', 'LEG_DOSSIER.md');
if (fs.existsSync(dossierPath)) {
  let d = fs.readFileSync(dossierPath, 'utf8');
  const dnl = d.includes('\r\n') ? '\r\n' : '\n';
  const runLegs = {}; // leg -> [run1, run2, run3] from docs\legs_reference.tsv's optional run columns
  for (const r of ref) {
    const b = bestBy((h) => h.save.toLowerCase() === r.start_save.toLowerCase());
    const pbs = hist
      .filter((h) => h.save.toLowerCase() === r.start_save.toLowerCase())
      .sort((x, y) => (x.date < y.date ? -1 : 1))
      .reduce((acc, h) => (acc.length && +h.secs >= +acc[acc.length - 1].secs ? acc : acc.concat([h])), [])
      .map((h) => `${h.secs} (${h.date.slice(5)})`)
      .join(' -> ');
    const block = [
      `| Run 1 (WR) | Run 2 | Run 3 | Best of 3 | Scene-free | Ours (best) | To go |`,
      `|---|---|---|---|---|---|---|`,
      `| ${r.run1 || '-'} | ${r.run2 || '-'} | ${r.run3 || '-'} | ${r.best3} | ${r.scenefree} | ${b ? b.secs : '-'} | ${b ? +b.secs - +r.scenefree : '-'} |`,
      '',
      `Our personal bests (full leg): ${pbs || 'none yet'} - what changed: LEGS.md "Personal-best history".`,
    ];
    const re = new RegExp(`(<!-- legtimes:${r.leg}:begin -->)[\\s\\S]*?(<!-- legtimes:${r.leg}:end -->)`);
    d = d.replace(re, `$1${dnl}${block.join(dnl)}${dnl}$2`);
  }
  writeWhole(dossierPath, d);
}

const doc = fs.readFileSync(docPath, 'utf8');
const begin = '<!-- legtable:begin -->', end = '<!-- legtable:end -->';
const i = doc.indexOf(begin), j = doc.indexOf(end);
if (i < 0 || j < i) {
  console.error('LEGS.md has no legtable markers');
  process.exit(1);
}
const nl = doc.includes('\r\n') ? '\r\n' : '\n';
writeWhole(docPath, doc.slice(0, i + begin.length) + nl + out.join(nl) + nl + doc.slice(j));
console.log(`legtable: ${ref.length - missing.length}/${ref.length} legs with a best, mission legs ours ${sOurs} vs scene-free ${sSfCovered}`);
