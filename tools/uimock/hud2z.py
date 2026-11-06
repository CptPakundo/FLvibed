"""hud2z.py - the waypoint mark as the owner asked (6 Nov 2026): the small diamond round the target's attitude caret, lightly glowing; the name line stays plain.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2z.py

Nothing here is built. Written to research\\evidence\\hud2_gate\\decisions\\Target3_waypoint_caret.jpg.
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2d as d  # noqa: E402
import hud2t as tt  # noqa: E402
import hud2u as u  # noqa: E402
import hud2w as w  # noqa: E402
import hud2y as y_  # noqa: E402

R = 0.027      # the diamond's half-diagonal in H: the caret's corners stand inside it
CARET = 0.011  # the caret's half-width in H (hud2w.mark)
WAY = y_.WAY


def diamond(s, x, y, glow=170):
    """Nested polygons, so all four corners are mitred alike. The caret's centre of mass (y - 0.433 q for half-width q, top y - q, tip y + 0.7 q) stands on the diamond's centre."""
    r = s.h(R)
    y = y - 0.433 * s.h(CARET)
    wd = max(2.0, s.h(0.0026)) * 1.414      # along the diagonal

    def dia(rad):
        return [(x, y - rad), (x + rad, y), (x, y + rad), (x - rad, y)]
    lay = Image.new('RGBA', (int(s.W), int(s.H)), (0, 0, 0, 0))
    dl = ImageDraw.Draw(lay)
    dl.polygon(dia(r + wd * 1.5), fill=WAY + (255,))
    dl.polygon(dia(r - wd * 1.5), fill=(0, 0, 0, 0))
    lay = lay.filter(ImageFilter.GaussianBlur(s.h(0.0065)))
    lay.putalpha(lay.getchannel('A').point(lambda v: min(255, v * glow // 100)))
    s.paste(lay)

    def fn(dr, P, k):
        for rad, col in ((r + wd / 2 + 2, tt.NAVY + (230,)), (r + wd / 2, (226, 176, 255, 255)), (r - wd / 2, tt.NAVY + (230,)), (r - wd / 2 - 2, (0, 0, 0, 0))):
            dr.polygon([P(*p) for p in dia(rad)], fill=col)
    s.shape(fn, box=(x - r - wd - 6, y - r - wd - 6, x + r + wd + 6, y + r + wd + 6))


def way(s, o, x, y):
    diamond(s, x, y - s.h(o['half']) * 1.22)
    y_.form(s, o, x, y, False)


def main():
    out = {}
    for key, o in (('gate', y_.GATE), ('ship', y_.SHIP)):
        name, ext = o['src']
        src = os.path.join(w.FOOT, name + ext)
        base = w.paint(src, o['label'])
        nd = w.paint(tt.save_im(base, 'z_' + key), [o['dist_box']])
        out[key] = (o, Image.open(src).convert('RGB'), y_.no_diamond(nd, o))

    def pic(key, fn, small=False, box=None):
        o, raw, gone = out[key]
        b = box or o['box']
        if small:
            k = 1280 / 1728
            b8 = (int((b[0] - 96) * k), int(b[1] * 800 / 1080), int((b[2] - 96) * k), int(b[3] * 800 / 1080))
            return w.on(w.to800(gone), 'z8_' + key, lambda s: fn(s, o, *w.p8(*o['c']))).crop(b8)
        return w.on(gone, 'z_' + key + '_o', lambda s: fn(s, o, *o['c'])).crop(b)

    def plain(s, o, x, y):
        y_.form(s, o, x, y, False)

    S = tt.build()
    tgt = S['one']

    def fighter(s, sc, on_way):
        t = sc.targets[0]
        if on_way:
            diamond(s, t['x'], t['y'] - s.h(t['half']) * 1.22)
        w.settled(s, t)

    boxes = ((1040, 860, 1680, 1320), (400, 330, 660, 516))
    zoom = {'gate': (889, 430, 1009, 530), 'ship': (468, 430, 588, 530)}
    rows = [('A solar and a ship as the waypoint', 'The diamond stands round the attitude caret, with a light glow; the name line is plain again. Left a solar, right a ship.', [pic('gate', way), pic('ship', way)], False),
            ('Close up', 'The caret keeps its own form and colour inside the diamond: hollow neutral on the left, solid hostile on the right.', [pic('gate', way, box=zoom['gate']).resize((600, 500), Image.LANCZOS), pic('ship', way, box=zoom['ship']).resize((600, 500), Image.LANCZOS)], False),
            ('The same two, not a waypoint', 'The only difference is the diamond.', [pic('gate', plain), pic('ship', plain)], False),
            ('At 1280x800', 'The same frames cut and scaled.', [pic('gate', way, True), pic('ship', way, True)], False),
            ('A fighter as the waypoint, on the game\'s own sky', 'An escort-mission kind of target. Left 4K, right 1280x800.', [u.draw(sc, lambda s, c: fighter(s, c, True), b) for sc, b in zip(tgt, boxes)], False),
            ('The same fighter, not a waypoint', 'For comparison.', [u.draw(sc, lambda s, c: fighter(s, c, False), b) for sc, b in zip(tgt, boxes)], False)]
    d.sheet('Target3_waypoint_caret', 'The waypoint diamond round the caret', 'B as refined: the small diamond holds the attitude caret, and glows lightly.', rows,
            'Real: the playthrough (vanilla, 1080p, classic HUD) for the solar and the ship, an HDE frame for the fighter. Painted out: the game\'s labels, bars, distance and large diamond, which leaves soft traces on the footage: they would not exist in the game. Drawn: the marks and, on the fighter, the cue. A still: the glow breathes at 0.5 Hz, slower than the lock\'s 1 Hz (Target3_breathing.gif). The gate\'s neutral caret is assumed, and the fighter is not a waypoint in its frame.')


if __name__ == '__main__':
    main()
