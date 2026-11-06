"""hud2y.py - a waypoint target by its type (the owner, 6 Nov 2026): a ship stays a ship, a solar a solar, and being the waypoint is an addition. A comparison for the owner.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2y.py

On the playthrough's frames (hud2w.py). Nothing here is built. Written to research\\evidence\\hud2_gate\\decisions\\Target3_waypoint.jpg.
"""
import os
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2d as d  # noqa: E402
import hud2s as hs  # noqa: E402
import hud2t as tt  # noqa: E402
import hud2u as u  # noqa: E402
import hud2w as w  # noqa: E402

INK, HOSTILE, T = tt.INK, tt.HOSTILE, tt.T
WAY = (198, 96, 236)     # the game's waypoint colour, read by eye from the diamond
GATE = dict(c=(949, 550), hw=121, hh=117, band=24, box=(560, 330, 1340, 770), half=0.05, name='New York > Alaska', dist='8.7 km', bars=False, col=w.NEUTRAL, form='hollow',
            label=[((878, 358, 1028, 426), 'white')], dist_box=((925, 462, 978, 490), 'white'), src=('m11_910', '.png'))
SHIP = dict(c=(528, 618), hw=150, hh=148, band=40, box=(140, 330, 920, 770 + 90), half=0.10, name='Nomad Battleship', dist='0 m', bars=True, sh=0.0, hu=0.66, col=HOSTILE, form='solid',
            label=[((478, 388, 575, 412), 'all'), ((478, 441, 575, 463), 'all'), ((425, 650, 472, 682), 'all'), ((472, 752, 518, 774), 'all'), ((542, 752, 594, 774), 'all'), ((518, 752, 542, 774), 'red')],
            dist_box=((498, 512, 558, 538), 'all'), src=('d_365', '.jpg'))


def no_diamond(im, o):
    a = np.asarray(im.convert('RGB'))
    yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]
    q = np.abs(xx - o['c'][0]) / o['hw'] + np.abs(yy - o['c'][1]) / o['hh']
    m = np.abs(q - 1.0 + 6.0 / o['hw']) < o['band'] / float(o['hw'])
    out = tt.heal(im, m, 7)
    if o['bars']:
        return out
    b = np.asarray(out.convert('RGB')).astype(int).copy()    # on dark sky the diamond's tint shows: take it off
    tint = (q < 1.3) & (b[..., 2] > b[..., 1] + 8) & (b[..., 0] > b[..., 1])
    b[..., 0][tint] = np.minimum(b[..., 0], b[..., 1] + 5)[tint]
    b[..., 2][tint] = np.minimum(b[..., 2], b[..., 1] + 10)[tint]
    return Image.fromarray(b.astype(np.uint8)).convert('RGBA')


def way_mark(s, x, y):
    q = s.h(0.0095)
    wd = max(2.0, s.h(0.0024))

    def fn(dr, P, k):
        p = [P(x, y - q), P(x + q, y), P(x, y + q), P(x - q, y)]
        dr.line(p + p[:1], fill=tt.NAVY + (255,), width=int((wd + 2) * k))
        dr.line(p + p[:1], fill=WAY + (255,), width=int(wd * k))
    s.shape(fn, box=(x - q - 4, y - q - 4, x + q + 4, y + q + 4))


def form(s, o, x, y, mark=True):
    """The candidate's settled form by type; the waypoint is a small diamond before the words."""
    hh = s.h(o['half'])
    z = s.h(T)
    line = o['name'] + '   ' + o['dist']
    wl = s.textlen(line, z)
    bw = max(hh * 2.7, s.h(0.095))
    y0 = y + hh * 1.12
    wy = y0 + (s.h(0.032) if o['bars'] else s.h(0.010))
    w.mark(s, x, y - hh * 1.22, o['col'], o['form'])
    if o['bars']:
        u.hbar(s, x - bw / 2, y0, bw, o['sh'], tt.SHIELD)
        u.hbar(s, x - bw / 2, y0 + s.h(0.0085), bw, o['hu'], tt.HULL)
    s.text((x, wy), line, z, fill=INK, anchor='mm')
    if mark:
        way_mark(s, x - wl / 2 - s.h(0.018), wy)


def last(s, o, x, y):
    """What the last sheet drew."""
    k = s.H / 1080
    z = s.h(T)
    if o is GATE:
        s.text((x + 136 * k, y), 'Mission Waypoint', z, fill=INK, anchor='lm')
        s.text((x + 136 * k, y + s.h(0.028)), 'New York > Alaska', z, fill=INK, anchor='lm')
    else:
        bw = s.h(0.14)
        y0 = y + 160 * k
        u.hbar(s, x - bw / 2, y0, bw, o['sh'], tt.SHIELD)
        u.hbar(s, x - bw / 2, y0 + s.h(0.0085), bw, o['hu'], tt.HULL)
        s.text((x, y0 + s.h(0.032)), o['name'], z, fill=INK, anchor='mm')


def main():
    out = {}
    for key, o in (('gate', GATE), ('ship', SHIP)):
        name, ext = o['src']
        raw = Image.open(os.path.join(w.FOOT, name + ext)).convert('RGB')
        base = w.paint(os.path.join(w.FOOT, name + ext), o['label'])
        nodist = w.paint(tt.save_im(base, key + '_nd') if False else tt.save_im(base, 'y_' + key), [o['dist_box']])
        gone = no_diamond(nodist, o)
        out[key] = (o, raw, base, nodist, gone)

    def pic(key, im, fn, small=False):
        o = out[key][0]
        b = o['box']
        if small:
            k = 1280 / 1728
            b8 = (int((b[0] - 96) * k), int(b[1] * 800 / 1080), int((b[2] - 96) * k), int(b[3] * 800 / 1080))
            r = w.on(w.to800(im), 'y8_' + key, lambda s: fn(s, o, *w.p8(*o['c'])))
            return r.crop(b8)
        return w.on(im, 'y_' + key + '_o', lambda s: fn(s, o, *o['c'])).crop(b)

    def none(s, o, x, y):
        pass

    def row(idx, fn, small=False):
        return [pic(k, out[k][idx], fn, small) for k in ('gate', 'ship')]

    rows = [('Today', 'Left a solar, a jump gate. Right a ship, the Nomad battleship. Both as the game draws a waypoint target: a large diamond, whatever the thing is.', row(1, none), False),
            ('The last sheet', 'Two shapes for two things: words beside the gate, bars under the battleship, and neither looks like its own kind of target.', row(2, last), False),
            ('A. By type, the game\'s diamond kept', 'The form for a ship or a solar, as for any target, with the large diamond still round it. The waypoint is plain to see, at the price of two shapes on one thing. The distance moves into the words.', row(3, lambda s, o, x, y: form(s, o, x, y, False)), False),
            ('B. By type, a small waypoint mark', 'The form for a ship or a solar, and a small diamond before the name. The large diamond goes, so a waypoint looks like its own kind of target. It needs the diamond\'s art hidden in the game: unprobed.', row(4, form), True),
            ('For comparison: the same two, not a waypoint', 'B without the mark: the only difference is the small diamond. A solar with nothing to damage has no bars; a ship always has them.', row(4, lambda s, o, x, y: form(s, o, x, y, False)), False),
            ('B at 1280x800', 'The same frames cut and scaled.', row(4, form, True), False)]
    d.PICK = "this sheet's pick"
    d.sheet('Target3_waypoint', 'A waypoint target by its type', 'A ship stays a ship and a solar a solar; being the waypoint is an addition. Left a solar, right a ship.', rows,
            'Real: the playthrough (vanilla, 1080p, classic HUD), the only frames holding these. Painted out: the game\'s labels and bars, and in A and B its distance; in B and the rows after it, the diamond too, which leaves a soft trace along its line. Drawn: the marks. Marks follow the object\'s size up to 0.10 H. The gate\'s attitude and both names are as the frames show them; the caret\'s neutral form is assumed for the gate.')


if __name__ == '__main__':
    main()
