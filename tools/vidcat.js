// Footage catalogue (user, 18 Oct 2026): the reference videos (the user's 2020 HDE playthrough, PvP clips) are studied
// throughout the project - player behaviour, bug hunts, features. Everything extracted is kept and indexed once, so
// later work finds and reuses it instead of decoding the videos again. No playback (no audio, ever).
//
// Layout (research\videos\catalogue, git-ignored):
//   videos.tsv               one row per video: id, source, mission, duration, path
//   thumbs\<id>\NNNNN.jpg    1 frame per second, 480 px wide (frame N = second N-1): browse, sheets without decoding
//   tracks\<id>.hud1.csv     vidhud.js at 1 fps (target bracket, hostile red, power / shield / hull bars)
//   tracks\<id>.speed1.csv   vidspeed.js at 1 fps (speed readout: throttle, reverse, afterburner, cruise)
//   segments\<id>.tsv        per-second state runs: offhud (base, cutscene, menu), space-quiet, space-fight
//   clips\<id>\<from>-<to>@<fps>\NNNNN.jpg   dense frames (1280 px) + sheet-*.png, made on demand
//   index.tsv                every clip and every observation (note): id, video, mission, from, to, fps, kind, tags,
//                            path, notes, added - the thing to search first
//
// Usage: node tools\vidcat.js <command> ...
//   init                                   videos.tsv from the known videos (playlist ids, research\videos\pvp)
//   base [<id>|all] [--jobs 3]             thumbs + tracks + segments (skips what exists)
//   segments [<id>|all]                    recompute segments from the tracks
//   clip <id> <from> <to> [--fps 5] [--tags a,b] [--notes "..."]   dense frames + sheets, indexed (reused if present)
//   sheet <id> <from> <to> [--every 1] [--cols 6] [--out file]     contact sheet from the thumbs (no decoding)
//   note <id> <from> <to> --tags a,b --notes "..."                  an observation without frames
//   find [text]                            index rows (and videos) matching text (mission, tag, note, id)
//   where <id> [--kind space-fight|space-quiet|offhud] [--min s]  the video's segments
// Times: seconds or h:mm:ss / m:ss. Requires ffmpeg on PATH (or FFMPEG env).
'use strict';
const fs = require('fs');
const path = require('path');
const { spawnSync, spawn } = require('child_process');

const ROOT = path.join(__dirname, '..', 'research', 'videos');
const CAT = path.join(ROOT, 'catalogue');
const FFMPEG = process.env.FFMPEG || 'ffmpeg';
const FONT = "fontfile='C\\:/Windows/Fonts/arial.ttf'";

// The user's playlist "Freelancer: A 2020 playthrough in 4K" (an older HDE: the HUD differs from 0.7.1).
const PLAYLIST = {
    MWSz9wgxg8c: 'intro', mUW3yIGlYMI: 'prologue', '5l6c083ld7g': 'M01', zFNdsRwecJ4: 'M01.5', vPQhYExpj_Q: 'M02',
    TkjJn6Eacgw: 'M02.5', Hra0yD6Enxg: 'M03', UywxaoRzvtk: 'M03.5', LYJbE04wl00: 'M04', pcBxVU1SrUg: 'M04.5',
    '9SPp6bNddzw': 'M05', ZgUBwtd_GUs: 'M05.5', Fdgs5drVmlc: 'M06-07', 'M42p3-_wiRQ': 'M07.5', LLwpoyPpe58: 'M08',
    '6h2Qm6NJvgY': 'M08.5', EbZ1sdlTCEQ: 'M09', FhzBnSP8zII: 'M10', 'qr-Bai_uxmA': 'M11', WwzFWVz7nfQ: 'M12',
    i3vAiGmtO84: 'M13', 'UhFW5H_E-_0': 'epilogue',
};

function mkdirp(d) { fs.mkdirSync(d, { recursive: true }); }
function secs(v) {
    if (v === undefined) return NaN;
    const p = String(v).split(':').map(Number);
    return p.reduce((a, x) => a * 60 + x, 0);
}
function hms(s) {
    s = Math.round(s);
    const h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, x = s % 60;
    return (h ? h + ':' + String(m).padStart(2, '0') : String(m)) + ':' + String(x).padStart(2, '0');
}
function args(list) {
    const pos = [], opt = {};
    for (let i = 0; i < list.length; i++) {
        if (list[i].startsWith('--')) opt[list[i].slice(2)] = list[i + 1] !== undefined && !list[i + 1].startsWith('--') ? list[++i] : '1';
        else pos.push(list[i]);
    }
    return { pos, opt };
}
function readTsv(file) {
    if (!fs.existsSync(file)) return [];
    const lines = fs.readFileSync(file, 'utf8').split(/\r?\n/).filter(l => l && !l.startsWith('#'));
    const head = lines.shift().split('\t');
    return lines.map(l => { const c = l.split('\t'); const o = {}; head.forEach((h, i) => (o[h] = c[i] ?? '')); return o; });
}
function writeTsv(file, head, rows) {
    fs.writeFileSync(file, [head.join('\t'), ...rows.map(r => head.map(h => String(r[h] ?? '').replace(/[\t\r\n]/g, ' ')).join('\t'))].join('\n') + '\n');
}
const IDX_HEAD = ['id', 'video', 'mission', 'from', 'to', 'fps', 'kind', 'tags', 'path', 'notes', 'added'];
function index() { return readTsv(path.join(CAT, 'index.tsv')); }
function addIndex(row) {
    const rows = index();
    row.id = 'c' + String(rows.reduce((m, r) => Math.max(m, parseInt(r.id.slice(1)) || 0), 0) + 1).padStart(4, '0');
    row.added = new Date().toISOString().slice(0, 10);
    rows.push(row);
    writeTsv(path.join(CAT, 'index.tsv'), IDX_HEAD, rows);
    return row;
}
function videos() { return readTsv(path.join(CAT, 'videos.tsv')); }
function video(id) {
    const v = videos().find(r => r.id === id || r.mission.toLowerCase() === String(id).toLowerCase());
    if (!v) throw new Error(`unknown video ${id} (vidcat init; vidcat find)`);
    return v;
}
function duration(file) {
    const r = spawnSync('ffprobe', ['-v', 'error', '-show_entries', 'format=duration', '-of', 'csv=p=0', file], { encoding: 'utf8' });
    return parseFloat(r.stdout) || 0;
}
function run(cmd, a, outFile) {
    return new Promise((res, rej) => {
        const out = outFile ? fs.openSync(outFile + '.part', 'w') : 'ignore';
        const p = spawn(cmd, a, { stdio: ['ignore', out, 'pipe'] });
        let err = '';
        p.stderr.on('data', d => (err += d));
        p.on('close', code => {
            if (outFile) { fs.closeSync(out); if (code === 0) fs.renameSync(outFile + '.part', outFile); else fs.rmSync(outFile + '.part', { force: true }); }
            code === 0 ? res() : rej(new Error(`${path.basename(cmd)} ${code}: ${err.slice(-400)}`));
        });
    });
}

function init() {
    mkdirp(CAT);
    const rows = videos();
    const have = new Set(rows.map(r => r.id));
    const add = (id, source, mission, file) => {
        if (have.has(id) || !fs.existsSync(file)) return;
        rows.push({ id, source, mission, duration: duration(file).toFixed(1), path: path.relative(ROOT, file) });
        have.add(id);
    };
    for (const [id, m] of Object.entries(PLAYLIST)) add(id, 'playlist-2020', m, path.join(ROOT, 'playlist', id + '.mp4'));
    const pvp = path.join(ROOT, 'pvp');
    if (fs.existsSync(pvp))
        for (const f of fs.readdirSync(pvp).filter(f => f.endsWith('.mp4'))) add(path.basename(f, '.mp4'), 'pvp', 'pvp', path.join(pvp, f));
    writeTsv(path.join(CAT, 'videos.tsv'), ['id', 'source', 'mission', 'duration', 'path'], rows);
    console.log(`${rows.length} videos in ${path.join(CAT, 'videos.tsv')}`);
}

// Per-second state from the tracks: offhud (no power / hull bars: base, cutscene, menu), space-fight (hostile red in
// the contact list), space-quiet. Runs shorter than 3 s merge into their neighbour (flicker).
function segments(id) {
    const hud = readCsv(path.join(CAT, 'tracks', id + '.hud1.csv'));
    if (!hud.length) return 0;
    const spd = new Map(readCsv(path.join(CAT, 'tracks', id + '.speed1.csv')).map(r => [Math.round(+r.t), r.speed]));
    const state = hud.map(r => (+r.power > 0.3 && +r.hull > 0.05 ? (+r.redList > 0 ? 'space-fight' : 'space-quiet') : 'offhud'));
    for (let pass = 0; pass < 2; pass++) // smooth: a state lasting < 3 s takes the previous one
        for (let i = 1; i < state.length; i++) {
            let j = i;
            while (j < state.length && state[j] === state[i]) j++;
            if (state[i] !== state[i - 1] && j - i < 3) for (let k = i; k < j; k++) state[k] = state[i - 1];
            i = j - 1;
        }
    const rows = [];
    for (let i = 0; i < state.length;) {
        let j = i;
        while (j < state.length && state[j] === state[i]) j++;
        const sp = [];
        for (let k = i; k < j; k++) { const v = spd.get(Math.round(+hud[k].t)); if (v !== undefined && v !== '') sp.push(+v); }
        sp.sort((a, b) => a - b);
        const hull0 = +hud[i].hull, hull1 = +hud[j - 1].hull;
        rows.push({ from: (+hud[i].t).toFixed(0), to: (+hud[j - 1].t + 1).toFixed(0), kind: state[i], secs: j - i,
                    speed_median: sp.length ? sp[Math.floor(sp.length / 2)] : '', still: sp.length ? (sp.filter(v => Math.abs(v) < 5).length / sp.length).toFixed(2) : '',
                    hull_change: state[i] === 'offhud' ? '' : (hull1 - hull0).toFixed(2) });
        i = j;
    }
    mkdirp(path.join(CAT, 'segments'));
    writeTsv(path.join(CAT, 'segments', id + '.tsv'), ['from', 'to', 'kind', 'secs', 'speed_median', 'still', 'hull_change'], rows);
    return rows.length;
}
function readCsv(file) {
    if (!fs.existsSync(file)) return [];
    const lines = fs.readFileSync(file, 'utf8').split(/\r?\n/).filter(Boolean);
    const head = lines.shift().split(',');
    return lines.map(l => { const c = l.split(','); const o = {}; head.forEach((h, i) => (o[h] = c[i])); return o; });
}

async function base(which, jobs) {
    const list = which && which !== 'all' ? [video(which)] : videos();
    const tasks = [];
    for (const v of list) {
        const file = path.join(ROOT, v.path);
        const th = path.join(CAT, 'thumbs', v.id);
        const need = Math.floor(+v.duration) - 2;
        const haveThumbs = fs.existsSync(th) ? fs.readdirSync(th).length : 0;
        mkdirp(th);
        mkdirp(path.join(CAT, 'tracks'));
        if (haveThumbs < need)
            tasks.push(() => run(FFMPEG, ['-v', 'error', '-y', '-i', file, '-vf', 'fps=1,scale=480:-2', '-q:v', '5', path.join(th, '%05d.jpg')]));
        const hud = path.join(CAT, 'tracks', v.id + '.hud1.csv');
        const spd = path.join(CAT, 'tracks', v.id + '.speed1.csv');
        if (!fs.existsSync(hud)) tasks.push(() => run('node', [path.join(__dirname, 'vidhud.js'), file, '--fps', '1'], hud));
        if (!fs.existsSync(spd)) tasks.push(() => run('node', [path.join(__dirname, 'vidspeed.js'), 'read', file, '--fps', '1'], spd));
    }
    let next = 0, done = 0;
    const worker = async () => {
        while (next < tasks.length) {
            const t = tasks[next++];
            try { await t(); } catch (e) { console.error(e.message); }
            console.error(`  ${++done}/${tasks.length}`);
        }
    };
    await Promise.all(Array.from({ length: Math.max(1, jobs) }, worker));
    for (const v of list) console.log(`${v.id} ${v.mission}: ${segments(v.id)} segments`);
}

function thumb(id, t) { return path.join(CAT, 'thumbs', id, String(Math.floor(t) + 1).padStart(5, '0') + '.jpg'); }

// Contact sheet: frames listed in a concat file (each 1 frame), stamped with the video time, tiled.
function sheetFrom(frames, times, cols, out, width) {
    const rows = Math.ceil(frames.length / cols);
    const list = out + '.txt';
    fs.writeFileSync(list, frames.map(f => `file '${f.replace(/\\/g, '/').replace(/'/g, "'\\''")}'\nduration 1`).join('\n') + '\n');
    const labels = times.map(hms);
    // Concat input has no video time to stamp: tile first, then label each tile at its position.
    const tile = `scale=${width}:-2,tile=${cols}x${rows}:padding=4`;
    const r = spawnSync(FFMPEG, ['-v', 'error', '-y', '-f', 'concat', '-safe', '0', '-i', list, '-vf', tile, '-frames:v', '1', out], { encoding: 'utf8' });
    fs.rmSync(list, { force: true });
    if (r.status !== 0) throw new Error(r.stderr);
    // Labels: a second pass drawing each tile's time at its position.
    const probe = spawnSync('ffprobe', ['-v', 'error', '-select_streams', 'v', '-show_entries', 'stream=width,height', '-of', 'csv=p=0', frames[0]], { encoding: 'utf8' });
    const [fw, fh] = probe.stdout.trim().split(',').map(Number);
    const th = Math.round(width * fh / fw);
    const draws = labels.map((l, i) => `drawtext=${FONT}:text='${l.replace(/:/g, '\\:')}':x=${(i % cols) * (width + 4) + 10}:y=${Math.floor(i / cols) * (th + 4) + 8}:fontsize=${Math.max(14, Math.round(width / 22))}:fontcolor=white:box=1:boxcolor=black@0.6`);
    const tmp = out.replace(/\.png$/, '.tmp.png');
    fs.renameSync(out, tmp);
    const r2 = spawnSync(FFMPEG, ['-v', 'error', '-y', '-i', tmp, '-vf', draws.join(','), out], { encoding: 'utf8' });
    if (r2.status !== 0) { fs.renameSync(tmp, out); throw new Error(r2.stderr); }
    fs.rmSync(tmp, { force: true });
    return out;
}

function sheet(id, from, to, opt) {
    const v = video(id);
    const every = +(opt.every || 1), cols = +(opt.cols || 6);
    const times = [];
    for (let t = from; t <= to; t += every) if (fs.existsSync(thumb(v.id, t))) times.push(t);
    if (!times.length) throw new Error('no thumbs in range (vidcat base ' + v.id + ')');
    const out = opt.out || path.join(CAT, 'sheets', `${v.id}_${Math.floor(from)}-${Math.floor(to)}_e${every}.png`);
    mkdirp(path.dirname(out));
    return sheetFrom(times.map(t => thumb(v.id, t)), times, cols, out, 480);
}

function clip(id, from, to, opt) {
    const v = video(id);
    const fps = +(opt.fps || 5);
    const dir = path.join(CAT, 'clips', v.id, `${Math.floor(from)}-${Math.ceil(to)}@${fps}`);
    const rel = path.relative(CAT, dir);
    const known = index().find(r => r.kind === 'clip' && r.path === rel);
    if (!fs.existsSync(dir) || fs.readdirSync(dir).filter(f => f.endsWith('.jpg')).length === 0) {
        mkdirp(dir);
        const r = spawnSync(FFMPEG, ['-v', 'error', '-y', '-ss', String(from), '-t', String(to - from), '-i', path.join(ROOT, v.path),
                                     '-vf', `fps=${fps},scale=1280:-2`, '-q:v', '4', path.join(dir, '%05d.jpg')], { encoding: 'utf8' });
        if (r.status !== 0) throw new Error(r.stderr);
        // Sheets of 24 frames each (6 x 4), stamped with the video time.
        const frames = fs.readdirSync(dir).filter(f => f.endsWith('.jpg')).sort();
        for (let i = 0, s = 1; i < frames.length; i += 24, s++)
            sheetFrom(frames.slice(i, i + 24).map(f => path.join(dir, f)), frames.slice(i, i + 24).map((f, k) => from + (i + k) / fps), 6,
                      path.join(dir, `sheet-${String(s).padStart(2, '0')}.png`), 640);
    }
    if (known) {
        if (opt.tags || opt.notes) { // more on a known clip: an observation row
            addIndex({ video: v.id, mission: v.mission, from: hms(from), to: hms(to), fps, kind: 'note', tags: opt.tags || '', path: rel, notes: opt.notes || '' });
        }
        return known;
    }
    return addIndex({ video: v.id, mission: v.mission, from: hms(from), to: hms(to), fps, kind: 'clip', tags: opt.tags || '', path: rel, notes: opt.notes || '' });
}

function find(text) {
    const q = (text || '').toLowerCase();
    const hit = r => !q || Object.values(r).join(' ').toLowerCase().includes(q);
    for (const v of videos().filter(hit)) console.log(`video ${v.id} ${v.mission} ${hms(+v.duration)} ${v.path}`);
    for (const r of index().filter(hit)) console.log(`${r.id} ${r.kind} ${r.mission} ${r.video} ${r.from}-${r.to}${r.fps ? ' @' + r.fps : ''} [${r.tags}] ${r.notes} -> ${r.path}`);
}

function where(id, opt) {
    const v = video(id);
    const min = +(opt.min || 0);
    for (const s of readTsv(path.join(CAT, 'segments', v.id + '.tsv')))
        if ((!opt.kind || s.kind === opt.kind) && +s.secs >= min)
            console.log(`${hms(+s.from)}-${hms(+s.to)} ${s.kind} ${s.secs}s speed~${s.speed_median} still=${s.still} hull${s.hull_change}`);
}

(async () => {
    const [cmd, ...rest] = process.argv.slice(2);
    const { pos, opt } = args(rest);
    try {
        if (cmd === 'init') init();
        else if (cmd === 'base') await base(pos[0] || 'all', +(opt.jobs || 3));
        else if (cmd === 'segments') for (const v of pos[0] && pos[0] !== 'all' ? [video(pos[0])] : videos()) console.log(v.id, segments(v.id));
        else if (cmd === 'clip') { const r = clip(pos[0], secs(pos[1]), secs(pos[2]), opt); console.log(`${r.id} ${path.join(CAT, r.path)}`); }
        else if (cmd === 'sheet') console.log(sheet(pos[0], secs(pos[1]), secs(pos[2]), opt));
        else if (cmd === 'note') { const v = video(pos[0]); const r = addIndex({ video: v.id, mission: v.mission, from: hms(secs(pos[1])), to: hms(secs(pos[2])), fps: '', kind: 'note', tags: opt.tags || '', path: '', notes: opt.notes || '' }); console.log(r.id); }
        else if (cmd === 'find') find(pos.join(' '));
        else if (cmd === 'where') where(pos[0], opt);
        else console.log(fs.readFileSync(__filename, 'utf8').split('\n').filter(l => l.startsWith('//')).join('\n'));
    } catch (e) {
        console.error('vidcat: ' + e.message);
        process.exit(1);
    }
})();
