"""hud2c.py - critical bars (the owner, 6 Nov 2026): shield and hull bars breathe when failing, faster towards zero, and bloom out at zero. Numbers: docs\\research\\bar-critical-feedback.md.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2c.py [sheet] [clip]

On a real frame, still: only the marks move; the ring is redrawn over the frame. Nothing here is built. Written to research\\evidence\\hud2_gate\\decisions\\Target3_bars.*.
"""
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2d as d  # noqa: E402
import hud2f as f  # noqa: E402
import hud2s as hs  # noqa: E402
import hud2t as tt  # noqa: E402
import hud2u as u  # noqa: E402
import hud2w as w  # noqa: E402

SHIELD, HULL, HOSTILE, INK = tt.SHIELD, tt.HULL, tt.HOSTILE, tt.INK
OWN = {'s': SHIELD, 'h': HULL}     # breathing and bloom stay in the bar's own colour (the owner): no hue change, no white


def rate(v):
    return 1.25 * (2.9 / 1.25) ** (1 - min(max(v, 0.0), 0.25) / 0.25)    # raised at the owner's word; the ceiling stays under the 3 Hz limit


def mix(a, b, e):
    return tuple(int(x + (y - x) * e) for x, y in zip(a, b))


def no_ring(sc):
    p = os.path.join(d.TMP, 'c_noring_' + sc.tag + '.png')
    im = Image.open(sc.clean).convert('RGB')
    al = np.asarray(u.reticle_layer(sc).getchannel('A')) > 0
    Hh, Ww = al.shape
    yy, xx = np.mgrid[0:Hh, 0:Ww]
    m = (np.abs(np.hypot(xx - Ww / 2, yy - Hh / 2) - sc.R) < 0.0095 * Hh) & (yy < Hh / 2 + sc.R * 0.8)
    a = np.asarray(im).astype(int)
    sat = ((a[..., 0] > 120) & (a[..., 1] < 120)) | ((a[..., 2] > 140) & (a[..., 0] < 140))
    tt.heal(im, m & sat | (m & al), 5).convert('RGBA').save(p)
    return p


def band(s, sc, a0, a1, rin, rout, col, alpha, blur):
    """A blurred arc band as a small layer."""
    pad = int(s.h(0.10))
    r = int(sc.R) + pad
    n = 2 * r
    lay = Image.new('RGBA', (n, n), (0, 0, 0, 0))
    ImageDraw.Draw(lay).arc((r - rout, r - rout, r + rout, r + rout), a0, a1, fill=col + (255,), width=max(1, int(rout - rin)))
    if blur > 0:
        lay = lay.filter(ImageFilter.GaussianBlur(blur))
    lay.putalpha(lay.getchannel('A').point(lambda v: int(v * alpha)))
    s.paste(lay, (int(sc.cx) - r, int(sc.cy) - r))


TAIL = BACK = 0.10         # the glow's roll-off past the value and past the bar's origin, as shares of the bar's length: equal, so it stays centred on what is left (the owner)


def profile(t, v):
    """Along the bar, t = 0 at its origin, 1 at its far end: full over the lit part, a raised cosine down to nothing past the value and, shorter, past the origin."""
    out = np.zeros_like(t)
    out[(t >= 0) & (t <= v)] = 1.0
    a = (t > v) & (t < v + TAIL)
    out[a] = 0.5 + 0.5 * np.cos(np.pi * (t[a] - v) / TAIL)
    b = (t < 0) & (t > -BACK)
    out[b] = 0.5 + 0.5 * np.cos(np.pi * (-t[b]) / BACK)
    return out


def arc_glow(s, sc, key, v, rin, rout, col, alpha, blur):
    pad = int(s.h(0.10))
    r = int(sc.R) + pad
    n = 2 * r
    lay = Image.new('RGBA', (n, n), (0, 0, 0, 0))
    ImageDraw.Draw(lay).ellipse((r - rout, r - rout, r + rout, r + rout), outline=col + (255,), width=max(1, int(rout - rin)))
    lay = lay.filter(ImageFilter.GaussianBlur(blur))
    yy, xx = np.mgrid[0:n, 0:n]
    ang = np.degrees(np.arctan2(yy - r, xx - r))
    t = ((ang % 360) - 138) / 84.0 if key == 's' else (42 - ang) / 84.0
    a = np.asarray(lay.getchannel('A')).astype(float) * profile(t, v) * alpha
    lay.putalpha(Image.fromarray(a.astype(np.uint8)))
    s.paste(lay, (int(sc.cx) - r, int(sc.cy) - r))


def bar_glow(s, x, y, bw, h, v, g, col, alpha):
    pad = int(g * 3 + bw * BACK + 4)
    W, H_ = int(bw) + 2 * pad, int(h + 2 * g) + 2 * int(g * 3 + 4)
    py = int(g * 3 + 4)
    lay = Image.new('RGBA', (W, H_), (0, 0, 0, 0))
    ImageDraw.Draw(lay).rectangle((0, py, W, py + h + 2 * g), fill=col + (255,))
    lay = lay.filter(ImageFilter.GaussianBlur(g * 0.7))
    t = (np.arange(W) - pad) / float(bw)
    a = np.asarray(lay.getchannel('A')).astype(float) * profile(t, v)[None, :] * alpha
    lay.putalpha(Image.fromarray(a.astype(np.uint8)))
    s.paste(lay, (int(x) - pad, int(y - g) - py))


def stage(ms):
    """Reach out and in (in H), halo alpha, white core alpha: the note's four stages."""
    if ms < 50:
        return 0.006, 0.003, 0.60, 1.0
    if ms < 130:
        e = 1 - (1 - (ms - 50) / 80.0) ** 3
        return 0.006 + 0.044 * e, 0.003 + 0.017 * e, 0.60 + 0.30 * e, 1.0
    if ms < 450:
        e = 1 - (1 - (ms - 130) / 320.0) ** 2
        return 0.05 * (1 + 0.2 * e), 0.02 * (1 + 0.2 * e), 0.90 * (1 - e) ** 1.4 + 0.06, max(0.0, 1 - (ms - 130) / 120.0)
    e = (ms - 450) / 150.0
    return 0.06, 0.024, 0.06 * (1 - e), 0.0


def arc_of(key):
    return (138, 222) if key == 's' else (-42, 42)


def own(s, sc, key, v, wave, ms=None, quiet=False):
    """One of the player's arcs. wave 0..1 is the breath, None when not breathing; ms the bloom's clock."""
    width = 0.0065 * sc.k * s.H
    base = OWN[key]
    a0, a1 = arc_of(key)
    if wave is not None:      # the breath is the glow alone: it swells and sinks, the arc keeps its colour
        depth = 0.4 if quiet else 1.0
        reach = s.h(0.004 + 0.007 * wave)
        arc_glow(s, sc, key, v, sc.R - reach * 0.5, sc.R + s.h(0.0065) + reach, base, (0.18 + 0.52 * wave) * depth, reach * 0.5)
    s.arcbar(sc.cx, sc.cy, sc.R + width / 2, a0, a1, width, v, base, n=14, gapdeg=1.6, reverse=key == 'h')
    if ms is not None and ms < 600:
        o, i, halo, core = stage(ms)
        band(s, sc, a0, a1, sc.R - s.h(i), sc.R + s.h(o), base, halo, s.h(o) * 0.33)
        if core > 0:
            band(s, sc, a0, a1, sc.R - s.h(0.002), sc.R + s.h(0.0085), base, core, s.h(0.0015))


def rect_glow(s, box, col, alpha, blur):
    x0, y0, x1, y1 = box
    pad = int(blur * 3 + 4)
    lay = Image.new('RGBA', (int(x1 - x0) + 2 * pad, int(y1 - y0) + 2 * pad), (0, 0, 0, 0))
    ImageDraw.Draw(lay).rectangle((pad, pad, pad + x1 - x0, pad + y1 - y0), fill=col + (255,))
    if blur > 0:
        lay = lay.filter(ImageFilter.GaussianBlur(blur))
    lay.putalpha(lay.getchannel('A').point(lambda v: int(v * alpha)))
    s.paste(lay, (int(x0) - pad, int(y0) - pad))


def target(s, sc, sh, hu, ws=None, wh=None, bs=None, bh=None):
    """The settled form with breathing and blooming bars."""
    t = sc.targets[0]
    hh = s.h(t['half'])
    x, y = t['x'], t['y']
    z = s.h(tt.T)
    line = t['name'] + '   ' + t['dist']
    bw = max(hh * 2.7, s.h(0.095))
    y0 = y + hh * 1.12
    wy = y0 + s.h(0.032)
    wl = s.textlen(line, z)
    w.halo(s, (x - max(bw, wl) / 2 - s.h(0.006), y0 - s.h(0.006), x + max(bw, wl) / 2 + s.h(0.006), wy + s.h(0.016)))
    w.mark(s, x, y - hh * 1.22, HOSTILE)
    for key, v, wave, ms, yy in (('s', sh, ws, bs, y0), ('h', hu, wh, bh, y0 + s.h(0.0085))):
        base = OWN[key]
        if wave is not None:
            bar_glow(s, x - bw / 2, yy, bw, s.h(0.005), v, s.h(0.0015 + 0.0035 * wave), base, 0.18 + 0.52 * wave)
        u.hbar(s, x - bw / 2, yy, bw, v, base)
        if ms is not None and ms < 600:
            o, i, halo, core = stage(ms)
            k = 0.4      # a target's bar blooms inside its row
            rect_glow(s, (x - bw / 2 - s.h(o) * k, yy - s.h(o) * k, x + bw / 2 + s.h(o) * k, yy + s.h(0.005) + s.h(o) * k), base, halo, s.h(o) * k * 0.4)
            if core > 0:
                rect_glow(s, (x - bw / 2, yy, x + bw / 2, yy + s.h(0.005)), base, core, 0)
    s.text((x, wy), line, z, fill=INK, anchor='mm')


def scene(sc, base, st, box):
    s = hs.Skin(base)
    f.cue(s, sc.k, sc.cells)
    own(s, sc, 's', st['os'], st.get('ows'), st.get('obs'), st.get('quiet', False))
    own(s, sc, 'h', st['oh'], st.get('owh'), st.get('obh'), st.get('quiet', False))
    target(s, sc, st['ts'], st['th'], st.get('tws'), st.get('twh'), st.get('tbs'), st.get('tbh'))
    return Image.open(d.save(s, 'c_' + sc.tag)).convert('RGB').crop(box)


BOX = ((960, 690, 2420, 1400), (340, 250, 860, 556))


def sheet(S, bases):
    rows = []

    def pair(st):
        return [scene(sc, b, st, x) for sc, b, x in zip(S['one'], bases, BOX)]
    for v, lab in ((0.25, '25 %: breathing starts, 1.25 Hz'), (0.15, '15 %: 1.75 Hz'), (0.05, '5 %: 2.45 Hz')):
        rows.append((lab, 'At the top of a breath: a tight glow in the bar\'s own colour, centred on what is left, with the same short roll-off at both ends. Stills cannot show the rate: the clip does.', pair(dict(os=v, oh=1.0, ts=v, th=1.0, ows=1.0, tws=1.0)), False))
    for ms, lab, note in ((20, 'Bloom, 20 ms: catch', 'The whole bar lights in its own colour.'), (130, 'Bloom, 130 ms: the peak', 'The halo reaches out, away from the reticle; a target\'s stays near its row.'),
                          (250, 'Bloom, 250 ms: spread', 'The solid core is gone, the glow lingers.'), (420, 'Bloom, 420 ms', 'Nearly away.'), (620, 'After 600 ms', 'The depleted state: empty pips.')):
        rows.append((lab, note, pair(dict(os=0.0, oh=1.0, ts=0.0, th=1.0, obs=ms, tbs=ms)), False))
    rows.append(('A hull blooms', 'The same in the hull\'s colours. On the player\'s ship a hull at zero is the end, so the bar needs a ghost kept for the 600 ms, or it would never be seen.', pair(dict(os=0.0, oh=0.0, ts=0.0, th=0.0, obh=130, tbh=130)), False))
    d.sheet('Target3_bars', 'Failing bars: breathing, then the bloom', 'All shield and hull bars breathe from 25 %, faster towards zero, and bloom out at zero, in 600 ms.', rows,
            'Left 4K, right 1280x800. Real: scene, ship, meter, speed. Drawn: the ring, the target\'s form, the cue. Values are set by hand, as is the target at that value. Escorts\' small bars and the alert cards\' beat are not drawn. Numbers: docs\\research\\bar-critical-feedback.md; design picks, none seen in game.')


def clip(S, bases):
    t_os, t_ts, t_th = 4.0, 4.5, 7.2
    grid = sorted(set([round(i * 0.06, 3) for i in range(134)] +[round(b + j * 0.04, 3) for b in (t_os, t_ts, t_th) for j in range(16)]))
    frames, durs = [], []
    ph = {'os': 0.0, 'ts': 0.0, 'oh': 0.0, 'th': 0.0}
    last = 0.0
    a, b = S['one']

    def ramp(t, t0, t1, v0, v1):
        return v0 if t <= t0 else v1 if t >= t1 else v0 + (v1 - v0) * (t - t0) / (t1 - t0)
    for i, t in enumerate(grid):
        dt = t - last
        last = t
        st = dict(os=ramp(t, 0, t_os, 0.32, 0.0), ts=ramp(t, 1.0, t_ts, 0.32, 0.0), oh=ramp(t, 4.8, 7.8, 0.32, 0.05), th=ramp(t, 5.0, t_th, 0.32, 0.0))
        for key, wk in (('os', 'ows'), ('ts', 'tws'), ('oh', 'owh'), ('th', 'twh')):
            v = st[key]
            if 0 < v <= 0.25:
                ph[key] += 2 * math.pi * rate(v) * dt
                st[wk] = 0.5 - 0.5 * math.cos(ph[key])
        for bk, tb in (('obs', t_os), ('tbs', t_ts), ('tbh', t_th)):
            if t >= tb:
                st[bk] = (t - tb) * 1000
        ia = d.fit(scene(a, bases[0], st, BOX[0]), 640)
        ib = d.fit(scene(b, bases[1], st, BOX[1]), 640)
        fr = Image.new('RGB', (1290, max(ia.height, ib.height)), d.BG)
        fr.paste(ia, (0, 0))
        fr.paste(ib, (650, 0))
        frames.append(fr.quantize(colors=256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE))
        durs.append(int(round(((grid[i + 1] - t) if i + 1 < len(grid) else 1.2) * 1000)))
    frames[0].save(os.path.join(d.OUT, 'Target3_bars.gif'), save_all=True, append_images=frames[1:], duration=durs, loop=0)
    print(len(frames), 'frames')


def main():
    S = tt.build()
    bases = [no_ring(sc) for sc in S['one']]
    what = sys.argv[1:] or ['sheet', 'clip']
    if 'sheet' in what:
        sheet(S, bases)
    if 'clip' in what:
        clip(S, bases)


if __name__ == '__main__':
    main()
