"""hud2w.py - the owner's targeting candidate in the cases held back until a concept was chosen: bright scenes, capital ships, off screen, a mission waypoint target; and carets by attitude.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2w.py

Real frames under drawn marks. Capitals and waypoints stand on the owner's playthrough (vanilla, 1080p, the classic HUD: research\\evidence\\hud2_gate\\foot\\, silent frames cut from the
video); no HDE frame holds them. Nothing here is built. Written to research\\evidence\\hud2_gate\\decisions\\Target3_cases.jpg.
"""
import os
import sys

import cv2
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2d as d  # noqa: E402
import hud2f as f  # noqa: E402
import hud2g as g  # noqa: E402
import hud2s as hs  # noqa: E402
import hud2t as tt  # noqa: E402
import hud2u as u  # noqa: E402

K4, DECK = tt.K4, tt.DECK
INK, HOSTILE, T = tt.INK, tt.HOSTILE, tt.T
NEUTRAL, FRIEND = (250, 208, 84), (104, 214, 104)
FOOT = os.path.join(g.GATE, 'foot')


def inset(pts, dist):
    """A triangle grown or shrunk by `dist` on every side, about its incentre: a stroke of even width with mitred corners."""
    (ax, ay), (bx, by), (cx, cy) = pts
    a_, b_, c_ = np.hypot(bx - cx, by - cy), np.hypot(ax - cx, ay - cy), np.hypot(ax - bx, ay - by)
    per = a_ + b_ + c_
    ix, iy = (a_ * ax + b_ * bx + c_ * cx) / per, (a_ * ay + b_ * by + c_ * cy) / per
    inr = abs((bx - ax) * (cy - ay) - (cx - ax) * (by - ay)) / per
    k = (inr - dist) / inr
    return [(ix + (px - ix) * k, iy + (py - iy) * k) for px, py in pts]


def mark(s, x, y, col, form='solid', size=0.011):
    q = s.h(size)
    w = max(2.0, s.h(0.0024))
    tri = [(x - q, y - q), (x + q, y - q), (x, y + q * 0.7)]

    def fn(dr, P, k):
        if form == 'solid':
            dr.polygon([P(*p) for p in tri], fill=col + (255,), outline=tt.NAVY + (255,))
        elif form == 'hollow':
            e = max(1.0, s.h(0.0006))     # the stroke stands inside the solid caret's outline, so all forms are one size
            for dist, c in ((-e, tt.NAVY + (255,)), (0, col + (255,)), (w, tt.NAVY + (255,)), (w + e, (0, 0, 0, 0))):
                dr.polygon([P(*p) for p in inset(tri, dist)], fill=c)
        else:
            pts = [P(*tri[0]), P(*tri[2]), P(*tri[1])]
            dr.line(pts, fill=tt.NAVY + (255,), width=int((w + 2) * k), joint='curve')
            dr.line(pts, fill=col + (255,), width=int(w * k), joint='curve')
    s.shape(fn, box=(x - q - 8, y - q - 8, x + q + 8, y + q + 8))


def halo(s, box):
    """A soft dark bed: a wider outline, not a plate."""
    lay = Image.new('RGBA', (int(s.W), int(s.H)), (0, 0, 0, 0))
    ImageDraw.Draw(lay).rectangle(box, fill=(4, 6, 18, 170))
    s.paste(lay.filter(ImageFilter.GaussianBlur(s.h(0.009))))


def settled(s, t, col=HOSTILE, form='solid', soft=False, bars=True, cap=True):
    """The candidate's settled form; the words line is one piece."""
    hh = s.h(min(t['half'], 0.10) if cap else t['half'])
    x, y = t['x'], t['y']
    z = s.h(T)
    line = t['name'] + '   ' + t['dist']
    wl = s.textlen(line, z)
    w = max(hh * 2.7, s.h(0.095))
    y0 = y + hh * 1.12
    wy = y0 + s.h(0.032)
    if soft:
        halo(s, (x - max(w, wl) / 2 - s.h(0.006), y0 - s.h(0.006), x + max(w, wl) / 2 + s.h(0.006), wy + s.h(0.016)))
    mark(s, x, y - hh * 1.22, col, form)
    if bars:
        u.hbar(s, x - w / 2, y0, w, t['sh'], tt.SHIELD)
        u.hbar(s, x - w / 2, y0 + s.h(0.0085), w, t['hu'], tt.HULL)
    s.text((x, wy), line, z, fill=INK, anchor='mm')


# ---- the footage: the classic bracket painted out, 1080p, and the same frame cut and scaled to 16:10
def paint(path, rects):
    im = Image.open(path).convert('RGB')
    a = np.asarray(im).astype(int)
    r, gg, b = a[..., 0], a[..., 1], a[..., 2]
    tests = {'white': (r > 150) & (gg > 150) & (b > 150), 'warm': (r > 0.7 * gg) & (r > 55), 'red': (r > gg + 40) & (r > b + 40), 'all': np.ones(a.shape[:2], bool)}
    m = np.zeros(a.shape[:2], np.uint8)
    for (x0, y0, x1, y1), kind in rects:
        m[y0:y1, x0:x1] |= tests[kind][y0:y1, x0:x1].astype(np.uint8)
    m = cv2.dilate(m, np.ones((5, 5), np.uint8))
    return tt.heal(im, m, 4)


def to800(im):
    return im.convert('RGB').crop((96, 0, 1824, 1080)).resize(DECK, Image.LANCZOS)


def p8(x, y):
    return (x - 96) * 1280 / 1728, y * 800 / 1080


def foot(name, ext, rects):
    src = os.path.join(FOOT, name + ext)
    clean = paint(src, rects)
    return Image.open(src).convert('RGB'), clean


def on(im, n, fn):
    p = tt.save_im(im, n)
    s = hs.Skin(p)
    fn(s)
    return Image.open(d.save(s, n + '_o')).convert('RGB')


def both(name, raw, clean, c1080, box, fn):
    """fn(s, x, y) on the 1080p frame and on its 1280x800 cut. Returns (today pair, candidate pair)."""
    k = 1280 / 1728
    b8 = (int((box[0] - 96) * k), int(box[1] * 800 / 1080), int((box[2] - 96) * k), int(box[3] * 800 / 1080))
    t = [raw.crop(box), to800(raw).crop(b8)]
    a = on(clean, name + '_a', lambda s: fn(s, *c1080))
    b = on(to800(clean), name + '_b', lambda s: fn(s, *p8(*c1080)))
    return t, [a.crop(box), b.crop(b8)]


def main():
    S = tt.build()
    fr = g.frame
    rows = []
    # bright
    dust = dict(name='Dustin Leinenbach', dist='427 m', sh=0.30, hu=0.74, cls='FIGHTER  LV 3', half=0.030)
    spots = {'sun': ((2380, 930), (815, 347)), 'planet': ((2230, 368), (750, 147))}
    crops = {'sun': ((1500, 560, 2900, 1300), (520, 220, 1000, 520)), 'planet': ((1700, 180, 2800, 760), (560, 70, 960, 300))}
    for key, lab, base in (('sun', 'Over the sun', 'sun_full__pad'), ('planet', 'Over bright cloud', 'planet_full__pad')):
        srcs = [fr(K4, base), fr(DECK, base)]

        def pic(soft):
            out = []
            for i, src in enumerate(srcs):
                s = hs.Skin(src)
                settled(s, dict(dust, x=spots[key][i][0], y=spots[key][i][1]), soft=soft)
                out.append(Image.open(d.save(s, 'w_b')).convert('RGB').crop(crops[key][i]))
            return out
        rows.append((f'{lab}, outline only', 'The text\'s own outline. No ship stands at this spot in the frame: the marks alone, placed on the brightest sky.', pic(False), False))
        rows.append((f'{lab}, with a soft dark bed', 'The research\'s fallback: a wider outline, not a plate.', pic(True), False))
    scs = [tt.Scene('4K', K4, fr(K4, 'sun_full__pad'), fr(K4, 'sun_full__pad'), 1.0, tt.FAR, [dict(dust, x=1900, y=1090)], (1100, 620, 2740, 1420), None),
           tt.Scene('1280x800', DECK, fr(DECK, 'sun_full__pad'), fr(DECK, 'sun_full__pad'), 1.08, tt.FAR, [dict(dust, x=634, y=404)], (330, 232, 950, 526), None)]

    def seat(s, sc):
        t = sc.targets[0]
        x, y, sd = u.seats(s, sc, t, 'shoulder')
        mark(s, t['x'], t['y'] - s.h(t['half']) * 1.22, HOSTILE)
        z = s.h(T)
        bw = s.h(0.13)
        ln = t['name'] + '   ' + t['dist']
        u.hbar(s, x - bw if sd < 0 else x, y - s.h(0.016), bw, t['sh'], tt.SHIELD)
        u.hbar(s, x - bw if sd < 0 else x, y - s.h(0.0075), bw, t['hu'], tt.HULL)
        s.text((x, y + s.h(0.014)), ln, z, fill=INK, anchor='rm' if sd < 0 else 'lm')
    rows.append(('The sun inside the ring, shoulder seat', 'A target in the sun: the seat stands clear of the worst of it, and outside the ring\'s low-shield number. The caret over the glare holds by its navy edge.', [u.draw(sc, seat) for sc in scs], False))

    # capitals
    raw, clean = foot('m11_1416', '.png', [((845, 445, 905, 500), 'warm'), ((1065, 445, 1125, 500), 'warm'), ((845, 580, 905, 635), 'warm'), ((1065, 580, 1125, 635), 'warm'),
                                           ((840, 508, 878, 572), 'warm'), ((1092, 508, 1130, 572), 'warm'), ((888, 430, 1088, 484), 'white'), ((1058, 466, 1080, 492), 'white'), ((892, 596, 942, 620), 'white'), ((936, 598, 1032, 618), 'all')])
    cru = dict(half=0.030, name='Order - Cruiser LNS Phoenix', dist='2.1 km', sh=1.0, hu=1.0)
    td, cd = both('cf', raw, clean, (984, 541), (640, 330, 1340, 760), lambda s, x, y: settled(s, dict(cru, x=x, y=y), NEUTRAL, 'hollow'))
    rows.append(('A capital far off: today', 'Real footage: the bracket far larger than the ship.', td, False))
    rows.append(('A capital far off: the candidate', 'Marks at the floor size, on the ship; a long name makes a long line.', cd, False))
    raw, clean = foot('m11_1434', '.png', [((975, 448, 1164, 497), 'white'), ((1152, 482, 1182, 514), 'white'), ((988, 610, 1044, 642), 'white'), ((1010, 640, 1124, 662), 'all'),
                                           ((922, 468, 990, 532), 'red'), ((922, 606, 990, 672), 'red'), ((1118, 468, 1178, 532), 'red'), ((1118, 606, 1178, 672), 'red')])
    gun = dict(half=0.10, name='Military - Gunboat RNC Ulm', dist='120 m', sh=0.0, hu=0.46)
    td, cd = both('cc', raw, clean, (1062, 568), (640, 330, 1500, 860), lambda s, x, y: settled(s, dict(gun, x=x, y=y)))
    rows.append(('A capital close: today', 'Real footage. The bracket stays the same size whatever the ship.', td, False))
    rows.append(('A capital close: the candidate', 'Marks follow the ship\'s size up to 0.10 H and stop there, so the block never leaves the screen.', cd, False))

    # waypoints
    raw, clean = foot('m11_910', '.png', [((878, 358, 1028, 426), 'white')])
    wp = (949, 550)

    def way(s, x, y):
        k = s.H / 1080
        s.text((x + 136 * k, y), 'Mission Waypoint', s.h(T), fill=INK, anchor='lm')
        s.text((x + 136 * k, y + s.h(0.028)), 'New York > Alaska', s.h(T), fill=INK, anchor='lm')
    td, cd = both('wa', raw, clean, wp, (560, 320, 1500, 780), way)
    rows.append(('A waypoint target: today', 'Real footage: the game\'s diamond, its distance inside, two lines over it.', td, False))
    rows.append(('A waypoint target: the candidate', 'The diamond and its distance stay the game\'s. The words move beside it at HUD 2\'s size. Nothing to damage, so no bars and no caret.', cd, False))
    raw, clean = foot('d_365', '.jpg', [((478, 388, 575, 412), 'all'), ((478, 441, 575, 463), 'all'), ((425, 650, 472, 682), 'all'), ((472, 752, 518, 774), 'all'), ((542, 752, 594, 774), 'all'), ((518, 752, 542, 774), 'red')])
    bs = dict(name='Nomad Battleship', dist='0 m', sh=0.0, hu=0.66, half=0.02)

    def ship(s, x, y):
        k = s.H / 1080
        z = s.h(T)
        w = s.h(0.14)
        y0 = y + 160 * k
        u.hbar(s, x - w / 2, y0, w, bs['sh'], tt.SHIELD)
        u.hbar(s, x - w / 2, y0 + s.h(0.0085), w, bs['hu'], tt.HULL)
        s.text((x, y0 + s.h(0.032)), bs['name'], z, fill=INK, anchor='mm')
    td, cd = both('wb', raw, clean, (528, 618), (150, 340, 1050, 860), ship)
    rows.append(('A target that is both, filling the screen: today', 'Real footage: the Nomad battleship as the mission\'s waypoint.', td, False))
    rows.append(('A target that is both: the candidate', 'The diamond and its distance stay; bars and name under them. The game\'s 0-M stands inside the diamond, so no second distance.', cd, False))

    # off screen
    a, b = fr(K4, 'fight__pad'), fr(DECK, 'fight__size_k100_pad')
    boxes = ((2300, 0, 3840, 1300), (680, 0, 1280, 440))
    rows.append(('Off screen', 'As today, real: the game\'s arrow; name and distance on the card, HUD 2\'s home for them.', [rgb_crop(a, boxes[0]), rgb_crop(b, boxes[1])], False))

    def leaving(src, x, y, box):
        s = hs.Skin(src)
        s.text((x, y), 'Ken Norris   225 m' if src == a else 'Ken Norris   137 m', s.h(T), fill=INK, anchor='rm', alpha=140)
        return Image.open(d.save(s, 'w_l')).convert('RGB').crop(box)
    rows.append(('Just leaving the screen', 'For about 0.5 s the words wait by the arrow and fade, so a target at the edge does not flicker. Only the words: the arrow keeps its own bars.', [leaving(a, 3490, 1040, boxes[0]), leaving(b, 845, 335, boxes[1])], False))

    # carets by attitude
    forms = ((HOSTILE, 'solid', 'Ken Norris'), (NEUTRAL, 'hollow', 'Ken Norris'), (FRIEND, 'open', 'Ken Norris'))
    for i, tag in ((0, '4K'), (1, '1280x800')):
        sc = S['one'][i]
        ims = []
        for col, form, _ in forms:
            ims.append(u.draw(sc, lambda s, sc2: settled(s, tt.tgt(sc2.targets[0]['x'], sc2.targets[0]['y'], 0.030, 'Ken Norris', sc2.targets[0]['dist']), col, form), ((1100, 900, 1640, 1320) if i == 0 else (426, 356, 626, 516))))
        rows.append((f'Attitude by shape as well as colour, {tag}', 'Hostile a solid caret, neutral a hollow one, friendly an open one. The same ship, to compare.', ims, False))
    d.sheet('Target3_cases', 'The candidate in the cases that waited', 'Bright scenes, capital ships, a waypoint target, off screen, and attitude by shape.', rows,
            'Left 4K, right 1280x800, but for capitals and waypoints: no HDE frame holds them, so those stand on the playthrough (vanilla, classic HUD), left 1080p, right the same frame cut and scaled to 1280x800. Painted out there: the game\'s bracket, label and bars. Bright scenes: the card\'s target is a simulated one and no ship stands under the marks. Drawn: the marks and, on the ring rows, the cue.')


def rgb_crop(p, box):
    return Image.open(p).convert('RGB').crop(box)


if __name__ == '__main__':
    main()
