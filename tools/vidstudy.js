// Combat study of a Freelancer gameplay video (HDE HUD): finds the fights and measures how the pilot flies them.
//   node vidstudy.js <video> <hud1.csv> [--fps 10] [--min 15]
// hud1.csv: `node vidhud.js <video> --fps 1` (fights = hostile red in the contact list). Per fight it runs vidspeed.js
// (speed readout) and vidhud.js (target bracket) at --fps and prints one JSON line per fight plus a total:
//   throttle shares: fwd (60..100 = full forward), rev (< -3), ab (100..260 = afterburner), cruise (> 260),
//   low (-3..60: coasting / engine kill / speeding up), switches/min fwd<->rev, median reverse / afterburner bursts (s);
//   aim: share with the bracket on screen, centred (|offset| < 0.15 half-screens), median offset;
//   reverse vs turn: share of reverse while the target is far off the nose (> 0.35) vs near it (< 0.15);
//   damage phases (shield + hull bars): hot (hp fell > 2 % in 2 s) vs cool (no loss for 3 s) with the same
//   throttle / aim shares, hot episodes, their median length, seconds to re-centre the target after the losses stop
//   (re-engage) and how often the target went off screen meanwhile (extended away).
'use strict';
const { execFileSync } = require('child_process');
const fs = require('fs');
const path = require('path');

const [video, hudCsv, ...rest] = process.argv.slice(2);
const opt = (n, d) => { const i = rest.indexOf('--' + n); return i >= 0 ? rest[i + 1] : d; };
const fps = Number(opt('fps', 10)), minLen = Number(opt('min', 15));
const rows = (file) => fs.readFileSync(file, 'utf8').trim().split('\n').slice(1).map((r) => r.split(','));

// Fights: 10 s windows with hostile red in the contact list on >= 4 of 10 samples, merged across gaps < 15 s.
const hud = rows(hudCsv).map((r) => ({ t: +r[0], list: +r[9] || 0 }));
const wins = [];
for (let i = 0; i < hud.length; i++) {
    let c = 0;
    for (let j = i; j < Math.min(hud.length, i + 10); j++) if (hud[j].list >= 2) c++;
    if (c < 4) continue;
    const a = hud[i].t, b = hud[Math.min(hud.length - 1, i + 9)].t;
    const last = wins[wins.length - 1];
    if (last && a - last[1] < 15) last[1] = Math.max(last[1], b);
    else wins.push([a, b]);
}
const fights = wins.filter((w) => w[1] - w[0] >= minLen);

const node = process.execPath;
const run = (script, from, to) => rows0(execFileSync(node, [path.join(__dirname, script), ...(script === 'vidspeed.js' ? ['read'] : []),
    video, '--fps', String(fps), '--from', String(from), '--to', String(to)], { maxBuffer: 1 << 28 }).toString());
function rows0(s) { return s.trim().split('\n').slice(1).map((r) => r.split(',')); }
const median = (a) => { if (!a.length) return null; const s = [...a].sort((x, y) => x - y); return s[Math.floor(s.length / 2)]; };

const total = { secs: 0, n: {}, switches: 0, rev: [], ab: [], on: 0, ctr: 0, offs: [], revFar: [0, 0], revNear: [0, 0] };
const cls = (v) => v < -3 ? 'rev' : v < 60 ? 'low' : v <= 100 ? 'fwd' : v <= 260 ? 'ab' : 'cruise';
for (const [a, b] of fights) {
    const sp = run('vidspeed.js', a, b), bk = run('vidhud.js', a, b);
    // Speed: fill unreadable frames from the previous reading (max 0.5 s), then a 5-frame median.
    const raw = sp.map((r) => (r[1] === '' ? null : +r[1]));
    let lastV = null, age = 99;
    const filled = raw.map((v) => { if (v !== null) { lastV = v; age = 0; return v; } age++; return age <= fps / 2 ? lastV : null; });
    const spd = filled.map((_, i) => median(filled.slice(Math.max(0, i - 2), i + 3).filter((x) => x !== null)));
    const f = { from: a, to: b, n: {}, switches: 0, rev: [], ab: [], frames: 0 };
    let prevMode = null, runCls = null, runLen = 0;
    const flush = () => { if (runCls === 'rev') f.rev.push(runLen / fps); if (runCls === 'ab') f.ab.push(runLen / fps); };
    let on = 0, ctr = 0, bkN = 0;
    const offs = [];
    const revFar = [0, 0], revNear = [0, 0];
    for (let i = 0; i < spd.length; i++) {
        const v = spd[i];
        if (v === null) continue;
        const c = cls(v);
        f.n[c] = (f.n[c] || 0) + 1;
        f.frames++;
        if (c === runCls) runLen++; else { flush(); runCls = c; runLen = 1; }
        if (c === 'fwd' || c === 'rev') { if (prevMode && prevMode !== c) f.switches++; prevMode = c; }
        const r = bk[i];
        if (r) {
            bkN++;
            if (r[1] === '1' && r[2] === '0') {
                on++;
                const o = Math.hypot(+r[3], +r[4]);
                offs.push(o);
                if (o < 0.15) ctr++;
                if (o > 0.35) { revFar[1]++; if (c === 'rev') revFar[0]++; }
                if (o < 0.15) { revNear[1]++; if (c === 'rev') revNear[0]++; }
            }
        }
    }
    flush();
    // Incoming damage: hp = shield + hull fill (bars), 1.5 s median against bar noise; hot = hp fell by > 2 % in the
    // last 2 s, cool = no fall for 3 s. Per phase: throttle and aim shares; after each hot episode: seconds until the
    // target is centred again (re-engaged) and whether the player turned it off screen (extended) meanwhile.
    const hpRaw = bk.map((r) => (r && r[13] !== undefined ? (+r[12] + +r[13]) / 0.983 : null));
    const hp = hpRaw.map((_, i) => median(hpRaw.slice(Math.max(0, i - Math.round(fps * 0.75)), i + Math.round(fps * 0.75) + 1).filter((x) => x !== null)));
    const ph = { hot: {}, cool: {} }, hotEp = [];
    let hotStart = -1, lastLoss = -1e9, epAway = false;
    for (let i = 0; i < spd.length; i++) {
        if (hp[i] === null || hp[i] === undefined) continue;
        const back = Math.max(0, i - 2 * fps);
        const top = Math.max(...hp.slice(back, i + 1).filter((x) => x !== null && x !== undefined));
        const losing = top - hp[i] > 0.02;
        if (losing) lastLoss = i;
        const phase = losing ? 'hot' : i - lastLoss > 3 * fps ? 'cool' : null;
        const r = bk[i], v = spd[i];
        const onS = r && r[1] === '1' && r[2] === '0';
        const off = onS ? Math.hypot(+r[3], +r[4]) : 9;
        if (phase) {
            const p = ph[phase];
            p.n = (p.n || 0) + 1;
            if (onS) p.on = (p.on || 0) + 1;
            if (off < 0.15) p.ctr = (p.ctr || 0) + 1;
            if (v !== null) { const c = cls(v); p[c] = (p[c] || 0) + 1; p.sp = (p.sp || 0) + 1; }
        }
        if (losing && hotStart < 0) { hotStart = i; epAway = false; }
        if (hotStart >= 0) {
            if (!onS) epAway = true;
            // The episode ends when the target is centred again after the losses stopped (or the fight ends).
            if (!losing && i - lastLoss > fps && off < 0.15) {
                hotEp.push({ dur: (lastLoss - hotStart) / fps, reengage: (i - lastLoss) / fps, away: epAway, drop: 0 });
                hotStart = -1;
            }
        }
    }
    const share = (p, k, d) => (p[d] ? Math.round(100 * (p[k] || 0) / p[d]) : null);
    const phaseOut = {};
    for (const k of ['hot', 'cool']) {
        const p = ph[k];
        phaseOut[k] = { secs: Math.round((p.n || 0) / fps), onScreen: share(p, 'on', 'n'), centred: share(p, 'ctr', 'n'),
            ab: share(p, 'ab', 'sp'), rev: share(p, 'rev', 'sp'), fwd: share(p, 'fwd', 'sp'), low: share(p, 'low', 'sp') };
    }
    const secs = f.frames / fps;
    const pct = (k) => Math.round(100 * (f.n[k] || 0) / Math.max(1, f.frames));
    const out = {
        fight: `${a}-${b}`, secs: Math.round(secs), read: Math.round(100 * f.frames / Math.max(1, spd.length)),
        fwd: pct('fwd'), rev: pct('rev'), ab: pct('ab'), cruise: pct('cruise'), low: pct('low'),
        switchesPerMin: +(60 * f.switches / Math.max(1, secs)).toFixed(1),
        revBurst: median(f.rev), abBurst: median(f.ab),
        onScreen: Math.round(100 * on / Math.max(1, bkN)), centred: Math.round(100 * ctr / Math.max(1, on)),
        medOffset: offs.length ? +median(offs).toFixed(2) : null,
        revWhenFar: revFar[1] ? Math.round(100 * revFar[0] / revFar[1]) : null,
        revWhenNear: revNear[1] ? Math.round(100 * revNear[0] / revNear[1]) : null,
        hot: phaseOut.hot, cool: phaseOut.cool, hotEpisodes: hotEp.length,
        hotDur: median(hotEp.map((e) => e.dur)), reengage: median(hotEp.map((e) => e.reengage)),
        extendedPct: hotEp.length ? Math.round(100 * hotEp.filter((e) => e.away).length / hotEp.length) : null,
    };
    console.log(JSON.stringify(out));
    total.phases = total.phases || { hot: {}, cool: {} };
    for (const k of ['hot', 'cool']) for (const q in ph[k]) total.phases[k][q] = (total.phases[k][q] || 0) + ph[k][q];
    total.hotEp = (total.hotEp || []).concat(hotEp);
    total.secs += secs;
    for (const k in f.n) total.n[k] = (total.n[k] || 0) + f.n[k];
    total.switches += f.switches; total.rev.push(...f.rev); total.ab.push(...f.ab);
    total.on += on; total.ctr += ctr; total.offs.push(...offs);
    total.revFar[0] += revFar[0]; total.revFar[1] += revFar[1]; total.revNear[0] += revNear[0]; total.revNear[1] += revNear[1];
    total.bk = (total.bk || 0) + bkN;
}
const fr = Object.values(total.n).reduce((x, y) => x + y, 0) || 1;
const tp = (k) => Math.round(100 * (total.n[k] || 0) / fr);
console.log(JSON.stringify({
    total: path.basename(video), fights: fights.length, secs: Math.round(total.secs), fwd: tp('fwd'), rev: tp('rev'), ab: tp('ab'),
    cruise: tp('cruise'), low: tp('low'), switchesPerMin: +(60 * total.switches / Math.max(1, total.secs)).toFixed(1),
    revBurst: median(total.rev), abBurst: median(total.ab), onScreen: Math.round(100 * total.on / Math.max(1, total.bk || 1)),
    centred: Math.round(100 * total.ctr / Math.max(1, total.on)), medOffset: total.offs.length ? +median(total.offs).toFixed(2) : null,
    revWhenFar: total.revFar[1] ? Math.round(100 * total.revFar[0] / total.revFar[1]) : null,
    revWhenNear: total.revNear[1] ? Math.round(100 * total.revNear[0] / total.revNear[1]) : null,
    ...(() => {
        const P = total.phases || { hot: {}, cool: {} }, E = total.hotEp || [];
        const sh = (p, k, d) => (p[d] ? Math.round(100 * (p[k] || 0) / p[d]) : null);
        const po = (p) => ({ secs: Math.round((p.n || 0) / fps), onScreen: sh(p, 'on', 'n'), centred: sh(p, 'ctr', 'n'),
            ab: sh(p, 'ab', 'sp'), rev: sh(p, 'rev', 'sp'), fwd: sh(p, 'fwd', 'sp'), low: sh(p, 'low', 'sp') });
        return { hot: po(P.hot), cool: po(P.cool), hotEpisodes: E.length, hotDur: median(E.map((e) => e.dur)),
            reengage: median(E.map((e) => e.reengage)),
            extendedPct: E.length ? Math.round(100 * E.filter((e) => e.away).length / E.length) : null };
    })(),
}));
