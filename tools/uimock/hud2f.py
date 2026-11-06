"""hud2f.py - the owner's rulings of 6 Nov 2026 on D4 and D6, drawn: the cue's final language (a still sheet and a moving one) and the radiation line.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2f.py [d4 d6]

Real frames under drawn marks. Written to research\\evidence\\hud2_gate\\decisions\\.
"""
import math
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2d as d  # noqa: E402
import hud2g as g  # noqa: E402
import hud2s as hs  # noqa: E402

K4, DECK = g.K4, g.DECK
INK, CHROME, ACTIVE, AMBER, HOSTILE = g.INK, g.CHROME, g.ACTIVE, g.AMBER, g.HOSTILE
HZ = 1.0            # the lock's breath: glow and bounce together; the flash rule's ceiling is 3 Hz
BOUNCE = 0.0028     # the tick's travel, in H: 6 px at 4K, 2.2 px at 800
REFIRE = 2.0        # the launchers' refire in s (`weapons`: 2.000)


def cell(s, u, x, y, cw, c, t, label, sel=False, reach=False, ok=True, lock=False, phase=0.0, refire=None):
    """As hud2d.cell, with the owner's lock: glow breathing at HZ, an inverted tick bouncing with it; the refire bar under M and T while they reload."""
    ch = cw * 0.24
    pts = [(x + ch, y), (x + cw, y), (x + cw, y + c - ch), (x + cw - ch, y + c), (x, y + c), (x, y + ch)]
    col = AMBER if not ok else ACTIVE if sel else d.DIMC
    reach = reach and ok
    wave = 0.5 + 0.5 * math.sin(2 * math.pi * phase)
    if lock:
        lay = Image.new('RGBA', (int(s.W), int(s.H)), (0, 0, 0, 0))
        ImageDraw.Draw(lay).rectangle((x - cw * 0.12, y - cw * 0.12, x + cw * 1.12, y + c + cw * 0.12), fill=INK + (int(120 + 110 * wave),))
        s.paste(lay.filter(ImageFilter.GaussianBlur(cw * 0.32)))
    s.plate((x - 1, y - 1, x + cw + 1, y + c + 1), chamfer=ch, alpha=225, rim_alpha=0, corners=(1, 0, 1, 0))

    def f(dr, P, q):
        if reach:
            dr.polygon([P(*p) for p in pts], fill=(ACTIVE if sel else CHROME) + (255,))
        elif sel:
            dr.line([P(*p) for p in pts + pts[:1]], fill=col + (255,), width=max(1, int(cw * 0.08 * q)))
    s.shape(f, box=(x - 4, y - 4, x + cw + 4, y + c + 4))
    s.text((x + cw / 2, y + c / 2), label, t, fill=d.NAVYD if reach else col, anchor='mm', shadow=not reach)
    if not ok:
        def sl(dr, P, q):
            dr.line([P(x + cw * 0.08, y + c * 0.92), P(x + cw * 0.92, y + c * 0.08)], fill=AMBER + (255,), width=max(1, int(cw * 0.10 * q)))
        s.shape(sl, box=(x - 2, y - 2, x + cw + 2, y + c + 2))
    if lock:
        lift = c * 0.10 + BOUNCE * s.H * u * wave

        def tk(dr, P, q):
            dr.polygon([P(x + cw * 0.14, y - lift - c * 0.26), P(x + cw * 0.86, y - lift - c * 0.26), P(x + cw * 0.5, y - lift)], fill=INK + (255,))
        s.shape(tk, box=(x - 2, y - c * 0.8, x + cw + 2, y))
    if refire is not None:
        bh = max(2.0, 0.0026 * s.H * u)
        yb = y + c + max(2.0, 0.002 * s.H * u)

        def rb(dr, P, q):
            dr.rectangle([P(x, yb), P(x + cw, yb + bh)], fill=(0, 0, 0, 200))
            dr.rectangle([P(x, yb), P(x + cw * refire, yb + bh)], fill=CHROME + (255,))
        s.shape(rb, box=(x - 2, yb - 2, x + cw + 2, yb + bh + 2))


def cue(s, k, cells, u=1.0):
    cx, cy = s.W / 2, s.H / 2
    R = s.h(0.10 * k) * u
    cw, c, gp, sep = s.h(0.020) * u, s.h(0.026) * u, s.h(0.003) * u, s.h(0.008) * u
    gaps = [(sep if cells[i][0].isdigit() and not cells[i + 1][0].isdigit() else gp) for i in range(len(cells) - 1)]
    x = cx - (len(cells) * cw + sum(gaps)) / 2
    y = cy - R * 0.93 - c / 2
    for i, (lb, kw) in enumerate(cells):
        cell(s, u, x, y, cw, c, s.h(0.024) * u, lb, **kw)
        x += cw + (gaps[i] if i < len(cells) - 1 else 0)


def crop_base(size, n, half=0.17):
    """A square round the ring, as its own file: the drawing is then cheap enough for every frame of a loop."""
    w, h = size
    c = int(h * half)
    p = os.path.join(d.TMP, f'fin_{n}_{w}.png')
    os.makedirs(d.TMP, exist_ok=True)
    Image.open(d.f2(size, n)).convert('RGBA').crop((w // 2 - c, h // 2 - c, w // 2 + c, h // 2 + c)).save(p)
    return p, h / (2 * c)


def scene(size, k, cells, deck, n='bracket_900_kbm'):
    p, u = crop_base(size, n)
    s = hs.Skin(p, deck)
    cue(s, k, cells, u)
    return s


def state(phase, t):
    """t in s of a loop: guns in reach, both launchers locked; a torpedo fired at 0, its bar refilling over REFIRE."""
    return [('1', dict(sel=True, reach=True)), ('2', dict(reach=True)), ('3', {}), ('M', dict(reach=True, lock=True, phase=phase)),
            ('T', dict(reach=True, lock=True, phase=phase, refire=min(1.0, t / REFIRE)))]


def d4():
    fps, loop = 20, 2.0
    frames = []
    for i in range(int(fps * loop)):
        t = i / fps
        ph = (t * HZ) % 1.0
        a = scene(K4, 1.0, state(ph, t), False)
        b = scene(DECK, 1.08, state(ph, t), True)
        ia = d.fit(Image.open(d.save(a, 'gif_a')).convert('RGB'), 640)
        ib = Image.open(d.save(b, 'gif_b')).convert('RGB')
        ib = ib.resize((ib.width * 2, ib.height * 2), Image.NEAREST)
        f = Image.new('RGB', (640 + 10 + ib.width, 640), (9, 11, 24))
        f.paste(ia, (0, 0))
        f.paste(ib, (650, (640 - ib.height) // 2))
        frames.append(f.quantize(colors=200, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE))
    frames[0].save(os.path.join(d.OUT, 'D4_final_motion.gif'), save_all=True, append_images=frames[1:], duration=int(1000 / fps), loop=0)
    # the still sheet
    os.makedirs(d.TMP, exist_ok=True)
    p = os.path.join(d.TMP, 'chartf.png')
    rows = [('1', {}, 'A GROUP', 'dim chrome digit on a dark cell'), ('1', dict(sel=True), 'SELECTED', 'yellow, and a hairline'), ('2', dict(reach=True), 'IN REACH', 'filled, the digit dark'),
            ('1', dict(sel=True, reach=True), 'SELECTED, IN REACH', 'yellow fill'), ('M', dict(reach=True, lock=True, phase=0.25), 'LOCKED', 'glow and tick, breathing together at 1 Hz'),
            ('T', dict(reach=True, refire=0.45), 'RELOADING', 'a thin bar under M and T, for the 2 s only'), ('T', dict(ok=False), 'NO ROUNDS', 'amber, a slash, never filled')]
    Image.new('RGBA', (1240, 164 * len(rows) + 30), (12, 16, 36, 255)).save(p)
    s = hs.Skin(p)
    for i, (lb, kw, a, b) in enumerate(rows):
        y = 44 + i * 164
        cell(s, 1.0, 40, y + 12, 76, 99, 92, lb, **kw)
        s.text((160, y + 42), a, 54, fill=INK)
        s.text((160, y + 94), b, 40, fill=CHROME)
    chart = Image.open(d.save(s, 'chartf_done'))

    def pair(ph, refire=None):
        st = [('1', dict(sel=True, reach=True)), ('2', dict(reach=True)), ('3', {}), ('M', dict(reach=True, lock=True, phase=ph)), ('T', dict(reach=True, lock=True, phase=ph, refire=refire))]
        a = Image.open(d.save(scene(K4, 1.0, st, False), 'p4')).convert('RGB')
        b = Image.open(d.save(scene(DECK, 1.08, st, True), 'pd')).convert('RGB')
        return [a, b.resize((a.width, a.height), Image.NEAREST)]
    d.sheet('D4_final', 'D4 as ruled: glow, tick, refire bar', 'The tick points down at its mark and bounces lightly with the glow. Left 4K, right 1280x800 enlarged, the pixels as they are. The moving form: D4_final_motion.gif.',
            [('The language', '', [chart], False),
             ('Top of the breath', 'Glow full, tick at its height.', pair(0.25), False),
             ('Bottom of the breath', 'Glow low, tick at rest; a torpedo just fired, its bar a third back.', pair(0.75, 0.33), False)],
            'Rules: 1 Hz against the 3 Hz ceiling. On `skin_motion` FADES or NONE the tick stops and the glow holds steady: the state stays. At 1280x800 the tick moves 2 px, near the smallest a move can be and not shimmer: read it in the gif. No real lock seen yet, 48 samples. Real: scene, bracket, ring. Drawn: the cue.')


def nav_bottom(p, size):
    """Where the nav block ends, read from the frame: the last bright text row of the top left."""
    w, h = size
    im = Image.open(p).convert('L').crop((int(w * 0.02), 0, int(w * 0.34), int(h * 0.34)))
    px = im.load()
    rows = [sum(1 for x in range(0, im.width, 2) if px[x, y] > 120) for y in range(im.height)]
    last, gap = 0, 0
    for y, v in enumerate(rows):
        if v > max(3, im.width // 200):
            last, gap = y, 0
        else:
            gap += 1
            if last and gap > h * 0.05:
                break
    return last / h


def icon(size):
    """The game's own radiation art, cut from a frame of its on phase."""
    w, h = size
    p = d.radiation_base(K4, False)
    im = Image.open(p).convert('RGBA')
    W, H = K4
    x0, y0, x1, y1 = int(W / 2 + H * 0.40), int(H * 0.04), int(W / 2 + H * 0.56), int(H * 0.19)
    a = im.crop((x0, y0, x1, y1))
    px = a.convert('RGB').load()
    xs, ys = [], []
    for y in range(a.height):
        for x in range(a.width):
            r, g_, b = px[x, y]
            if r > 200 and g_ > 180 and b < 90:
                xs.append(x)
                ys.append(y)
    m = 4
    b_ = (min(xs) - m, min(ys) - m, max(xs) + m, max(ys) + m)
    ic = a.crop(b_)
    mask = Image.new('L', (ic.width * 4, ic.height * 4), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, mask.width, mask.height), fill=255)
    ic.putalpha(mask.resize(ic.size, Image.LANCZOS))
    return ic


def line_on(src, size, deck, with_icon=True, ring=True, lag=False, n='x', place_row=False):
    """place_row: the frame's place line has faded, so its slot is kept: the amber line never takes it."""
    s = hs.Skin(src, deck)
    k = 1.08 if deck else 1.0
    yb = nav_bottom(src, size) + (0.046 * k if place_row else 0)
    x = 0.075 if not deck else 0.03
    z = 0.030 * k
    y = yb + 0.014 + z
    if with_icon:
        ic = icon(size)
        px = int(s.h(0.040 * k))
        s.paste(ic.resize((px, px), Image.LANCZOS), (int(s.h(x)), int(s.h(y) - px / 2)))
        x += 0.052 * k
    s.text((s.h(x), s.h(y)), 'RADIATION  ·  LEAVE THE ZONE', s.h(0.026 * k), fill=AMBER)
    if lag:
        s.text((s.h(0.075 if not deck else 0.03), s.h(y + 0.046 * k)), 'LAG 640 MS', s.h(0.024 * k), fill=CHROME)
    if ring:
        cx, cy, R = s.W / 2, s.H / 2, s.h(0.10 * k)
        r = R + s.h(0.034 * k)
        wd = s.h(0.007 * k)

        def f(dr, P, q):
            dr.arc([P(cx - r, cy - r), P(cx + r, cy + r)], 100, 124, fill=HOSTILE + (255,), width=int(wd * q))
        s.shape(f, box=(cx - r, cy - r, cx + r, cy + r + wd))
    return Image.open(d.save(s, 'l_' + n)).convert('RGB')


def d6():
    r4 = d.radiation_base(K4, True)
    rd = d.radiation_base(DECK, True)
    # a K&M frame of the icon's off phase, or the icon painted out of one (the deny rule's picture)
    W, H = K4
    box = (int(W / 2 + H * 0.40), int(H * 0.04), int(W / 2 + H * 0.56), int(H * 0.19))
    kb = Image.open(g.frame(K4, 'radiation__kbm')).convert('RGBA')
    yel = [(x, y) for y in range(box[1], box[3]) for x in range(box[0], box[2]) if (lambda p: p[0] > 200 and p[1] > 180 and p[2] < 90)(kb.getpixel((x, y)))]
    if yel:
        b_ = (min(p[0] for p in yel) - 14, min(p[1] for p in yel) - 14, max(p[0] for p in yel) + 14, max(p[1] for p in yel) + 14)
        wd = b_[2] - b_[0]
        kb.paste(kb.crop((b_[0] - wd, b_[1], b_[0], b_[3])), b_[:2])
    k4 = d.save(kb, 'd6_kbm_off')
    d.sheet('D6_final', 'D6 as ruled: amber line, ring mark, one small word', 'Cards stay for dangers with a cure. The game\'s own radiation art stands beside the line where there is room: it fits at both sizes.',
            [('4K, on the pad and on K&M', 'The ring mark is the tail, at its bearing.', [line_on(r4, K4, False, n='a', place_row=True)], False),
             ('', '', [line_on(k4, K4, False, n='b', place_row=True)], False),
             ('1280x800', '', [line_on(rd, DECK, True, n='c')], False),
             ('With a mission\'s objective above it, K&M at 4K and pad at 1280x800', 'The line takes the next row of the nav block, read from the frame: nothing overlaps it, and it pushes nothing.', [line_on(g.frame(K4, 'mission__kbm'), K4, False, ring=False, n='d'), line_on(g.frame(DECK, 'mission__pad'), DECK, True, ring=False, n='e')], False),
             ('Lag, multiplayer only', 'One small word in the same block.', [line_on(g.frame(K4, 'calm__kbm'), K4, False, with_icon=False, ring=False, lag=True, n='f').crop((0, 0, 1500, 500))], False)],
            'The art is the game\'s, cut from a frame here; in the build it is the native control moved, or its own texture, so nothing of the game ships. The native icon blinks at 0.7 Hz; the design holds it steady. The line wants a lint row. Real: scene, HUD. Drawn: line, mark, word.')


def speed_base(size, src, k):
    """The real ring with the speed, the power arc and the throttle arc taken out: the scene's own pixels through a mask of them."""
    im = Image.open(g.frame(size, src)).convert('RGBA')
    sc = Image.open(g.frame(size, src.split('__')[0] + '__scene')).convert('RGBA')
    w, h = size
    cx, cy = w / 2, h / 2
    mask = Image.new('L', size, 0)
    dr = ImageDraw.Draw(mask)
    R = 0.10 * k * h
    aw = 0.0065 * k * h
    dr.arc([cx - R - aw, cy - R - aw, cx + R + aw, cy + R + aw], 64, 116, fill=255, width=max(3, int(aw * 2.2)))
    rt = R + 0.022 * k * h
    dr.arc([cx - rt - aw, cy - rt - aw, cx + rt + aw, cy + rt + aw], 78, 102, fill=255, width=max(3, int(aw * 2.2)))
    dr.rectangle((cx - 0.05 * h * k, cy + 0.036 * h * k, cx + 0.05 * h * k, cy + 0.076 * h * k), fill=255)
    im.paste(sc, (0, 0), mask.filter(ImageFilter.GaussianBlur(1.5)))
    return d.save(im, f'sp_{src}_{w}')


def speed_form(src, size, k, deck, form, power, thr, speed):
    """form: 'A' meter, throttle, speed. 'B' throttle inside, meter, speed: the shortest stack."""
    s = hs.Skin(src, deck)
    h = s.H
    cx, cy, R = s.W / 2, s.H / 2, s.h(0.10 * k)
    g.power_arc(s, k, power)
    tw = 0.0065 * k * h * 0.75
    rt = R + 0.022 * k * h if form == 'A' else R - 0.020 * k * h
    s.arcbar(cx, cy, rt, 82, 98, tw, thr, hs.CHROME, n=5, gapdeg=2.2, reverse=True, empty=(40, 48, 80))
    y = (R + 0.0494 * k * h) if form == 'A' else (R + 0.0285 * k * h)
    s.text((cx, cy + y), str(speed), s.h(0.030 * k), fill=INK, anchor='mm')
    return s


def speed():
    d.PICK = 'recommended'
    rows = []
    for size, k, deck, word in ((K4, 1.0, False, '4K'), (DECK, 1.08, True, '1280x800')):
        base = speed_base(size, 'planet_full__pad', k)
        dark = speed_base(size, 'nebula_full__pad', k)
        rows.append((size, k, deck, word, base, dark))

    def crop(s_, size):
        return d.ring_crop(d.save(s_, 'spc'), size, 0.21, 0.56)
    opts = [('Today', 'Speed inside the ring; meter, throttle arc below.', [d.ring_crop(g.frame(K4, 'planet_full__pad'), K4, 0.21, 0.56), d.ring_crop(g.frame(DECK, 'planet_full__pad'), DECK, 0.21, 0.56)], False)]
    for form, title, note in (('B', 'B. Throttle inside, meter, speed under it', 'The ring\'s bottom grows by 0.019 H. The throttle takes the speed\'s old room; the cruise readout has the rest of the inside to itself.'),
                              ('A', 'A. Meter, throttle, speed', 'Nothing else moves; the ring\'s bottom grows by 0.040 H, further over the ship.')):
        ims = [crop(speed_form(r[4], r[0], r[1], r[2], form, 0.6, 0.6, 84), r[0]) for r in rows]
        opts.append((title, note + ' Left 4K, right 1280x800, over a bright planet.', ims, form == 'B'))
        ims = [crop(speed_form(r[5], r[0], r[1], r[2], form, 0.25, 1.0, 312), r[0]) for r in rows]
        opts.append(('', 'Over a nebula, low power, full throttle.', ims, form == 'B'))
    d.sheet('Speed_below_the_meter', 'The speed under the energy meter', 'A player\'s request the owner agrees with: the speed goes to the bottom of the reticle. Where does the throttle arc stand?', opts,
            'Real: the scene, the ring\'s arcs, the ship. Drawn: meter, throttle, speed (the old ones taken out with the scene\'s pixels). The lower-centre zone starts lower by the same amount: the speed sits over the ship\'s nose, and the zone\'s rule moves with the owner\'s word.')


def map2():
    """A new map over the game's: the map real, its labels replaced by HUD 2's, the route and the distances from FLvibed's own data."""
    size = K4
    w, h = size
    sq = g.mapsq(size, 'map_real_bare')
    px = sq.load()
    sqw = sq.width
    fill = sq.filter(ImageFilter.MedianFilter(21))
    for x0, y0, x1, y1 in ((540, 660, 800, 735), (825, 550, 990, 600)):   # the game's three labels here
        sq.paste(fill.crop((x0, y0, x1, y1)), (x0, y0))
    g.hs.LABELS[:] = []
    g.use_frame(size, 'map_real_bare', box=False)
    hs.clean_map = lambda: sq
    n = 'navmap_new_3840x2160_pad'
    hs.navmap(size, n, pad=True, ends=hs.navmap(size, n, pad=True))
    s = hs.Skin(os.path.join(g.OUT, n + '.png'))
    top, S, gap = 0.115, 0.785, 0.018
    pw = (s.WH - 2 * s.mx - S - 2 * gap) / 2
    mx0 = s.mx + pw + gap

    def P(fx, fy):
        return (s.h(mx0 + fx * S), s.h(top + fy * S))
    objs = [(0.376, 0.378, 'Planet Manhattan', 'BASE', g.FRIEND), (0.518, 0.310, 'Fort Bush', 'BASE', g.FRIEND), (0.70, 0.27, 'Pittsburgh', 'PLANET', hs.CHROME), (0.33, 0.45, 'Newark Station', 'BASE', g.FRIEND),
            (0.62, 0.56, 'Battleship Missouri', 'BASE', g.FRIEND), (0.27, 0.62, 'West Point', 'BASE', g.FRIEND), (0.74, 0.76, 'New York > Texas', 'GATE', hs.CHROME), (0.09, 0.68, 'New York > California', 'GATE', hs.CHROME),
            (0.56, 0.78, 'Buffalo Base', 'BASE', hs.HOSTILE), (0.36, 0.84, 'New York > Magellan', 'HOLE', hs.AMBER)]
    a, b = P(0.50, 0.50), P(0.376, 0.378)

    def route(dr, Pq, q):
        pts = [P(0.50, 0.50), P(0.518, 0.310), P(0.376, 0.378)]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            n_ = 22
            for i in range(0, n_, 2):
                t0, t1 = i / n_, (i + 1) / n_
                dr.line([Pq(x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0), Pq(x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1)], fill=ACTIVE + (255,), width=int(s.h(0.0026) * q))
    s.shape(route, box=(s.h(mx0), s.h(top), s.h(mx0 + S), s.h(top + S)))
    for fx, fy, name, kind, col in objs:
        x, y = P(fx, fy)
        r = s.h(0.0065)

        def dot(dr, Pq, q, x=x, y=y, r=r, col=col):
            dr.ellipse([Pq(x - r, y - r), Pq(x + r, y + r)], fill=(10, 14, 40, 230), outline=col + (255,), width=int(s.h(0.0018) * q))
        s.shape(dot, box=(x - r - 2, y - r - 2, x + r + 2, y + r + 2))
        s.text((x + s.h(0.014), y), name, s.h(0.024), fill=hs.INK)
    x, y = P(0.50, 0.50)
    s.text((x + s.h(0.016), y + s.h(0.002)), 'YOU', s.h(0.024), fill=ACTIVE)
    x, y = P(0.447, 0.344)
    s.text((x, y - s.h(0.022)), '14.8 K  ·  1:12', s.h(0.024), fill=ACTIVE, anchor='mm')
    s.finish(n)
    g.NOTES[n] = 'REAL: the map, its grid, lanes and art at the grown size. DRAWN: everything else; object places partly illustrative.'
    d.PICK = 'recommended'
    d.sheet('Map_new_and_improved', 'A new and improved nav map', '"It\'s as good a chance as any to make a new and improved navmap if the game lets us." The game draws the map; HUD 2 draws what a player reads on it.',
            [('Today', 'Real.', [Image.open(g.frame(K4, 'nn_navmap__kbm'))], False),
             ('Grown, as probed', 'The real map at 0.785 H with the game\'s own labels.', [Image.open(os.path.join(g.OUT, 'navmap_3840x2160_pad.png'))], False),
             ('New: HUD 2 draws the map\'s words and the route', 'Labels at HUD 2\'s size, one mark per object coloured by attitude, the route with its distance and time, you on the map. Pan, zoom, selection, zones and lanes stay the game\'s.', [Image.open(os.path.join(g.OUT, n + '.png'))], True)],
            'Real: the map\'s art, grid and lanes at the grown size (the game\'s three labels here painted out). Drawn: all else; object places partly illustrative, the time an example. Unproven: hiding the game\'s labels on every view, and the overlay staying on them through pan and zoom.')


if __name__ == '__main__':
    for n in (sys.argv[1:] or ['d4', 'd6', 'speed', 'map2']):
        {'d4': d4, 'd6': d6, 'speed': speed, 'map2': map2}[n]()
