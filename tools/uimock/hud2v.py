"""hud2v.py - the targeting display, the owner's candidate (6 Nov 2026): a mini card that settles into the form worn by the ship, bars at the bottom; with the picks of round 2.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2v.py

Real frames under drawn marks, as hud2t.py. A candidate: nothing here is built. Written to research\\evidence\\hud2_gate\\decisions\\Target3_candidate.*.
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2d as d  # noqa: E402
import hud2t as tt  # noqa: E402
import hud2u as u  # noqa: E402

K4, DECK = tt.K4, tt.DECK
INK, CHROME, HOSTILE, T = tt.INK, tt.CHROME, tt.HOSTILE, tt.T
HOLD, EASE, SHOW = u.HOLD, u.EASE, u.SHOW
ONE = ((700, 700, 2300, 1420), (250, 262, 860, 545))


def card_worn(s, sc, t, p, seat=False):
    """p = 0 the mini card, 1 the settled form. The card's rows stand in the settled form's order, bars over words, so everything travels and nothing crosses."""
    e = p * p * (3 - 2 * p)
    hh = s.h(t['half'])
    z = s.h(T)
    sp = s.textlen('   ', z)
    wn, wd = s.textlen(t['name'], z), s.textlen(t['dist'], z)
    u.caret(s, t['x'], t['y'] - hh * 1.22)
    w, h = s.h(0.235), s.h(0.098)
    if seat:
        x, y, sd = u.seats(s, sc, t, 'shoulder')
        x0 = x - w if sd < 0 else x
        y0 = y - h / 2
        bw = s.h(0.13)
        fb = x - bw if sd < 0 else x                 # settled bars
        fby = y - s.h(0.016)
        fy = y + s.h(0.014)                          # settled words
        ln = wn + sp + wd
        fn = x - ln if sd < 0 else x
    else:
        sd = tt.side_of(sc, t)
        gap = hh + s.h(0.016)
        x0 = t['x'] - gap - w if sd < 0 else t['x'] + gap
        y0 = t['y'] - h / 2
        bw = max(hh * 2.7, s.h(0.095))
        fb = t['x'] - bw / 2
        fby = t['y'] + hh * 1.12
        fy = fby + s.h(0.016) + s.h(0.016)
        fn = t['x'] - (wn + sp + wd) / 2
    fd = fn + wn + sp
    tx, r = x0 + s.h(0.012), x0 + w - s.h(0.012)
    tight = (min(fb, fn) - s.h(0.006), fby - s.h(0.006), max(fb + bw, fd + wd) + s.h(0.006), fy + s.h(0.016))
    box = tuple(u.lerp(a, b, e) for a, b in zip((x0, y0, x0 + w, y0 + h), tight))
    if e < 0.995:
        s.plate(box, chamfer=s.h(0.010), alpha=int(205 * (1 - e)), rim_alpha=int(120 * (1 - e)))
        ax = box[2] - s.h(0.003) if sd < 0 else box[0]
        al = int(255 * (1 - e))
        s.shape(lambda dr, P, q: dr.rectangle([P(ax, box[1]), P(ax + s.h(0.003), box[3])], fill=HOSTILE + (al,)), box=(ax - 2, box[1] - 2, ax + s.h(0.003) + 2, box[3] + 2))
    fade = max(0.0, 1 - e * 3.5)
    if fade > 0:
        s.text((box[0] + s.h(0.012), box[1] + s.h(0.019)), 'HOSTILE', s.h(0.021), fill=HOSTILE, shadow=False, alpha=int(255 * fade))
        s.text((box[2] - s.h(0.012), box[1] + s.h(0.019)), t['cls'], s.h(0.021), fill=CHROME, anchor='rm', shadow=False, alpha=int(255 * fade))
    by = u.lerp(y0 + s.h(0.036), fby, e)
    bx = u.lerp(tx, fb, e)
    bwid = u.lerp(w - s.h(0.024), bw, e)
    u.hbar(s, bx, by, bwid, t['sh'], tt.SHIELD)
    u.hbar(s, bx, by + s.h(0.0085), bwid, t['hu'], tt.HULL)
    wy = u.lerp(y0 + s.h(0.074), fy, e)
    s.text((u.lerp(tx, fn, e), wy), t['name'], z, fill=INK)
    s.text((u.lerp(r - wd, fd, e), wy), t['dist'], z, fill=INK)


def close(s, sc, form):
    t0, l, r = sc.targets
    u.c1(s, t0, 'h', 'c')
    u.nontarget(s, l, -1, form, 1)
    u.nontarget(s, r, 1, form, 1)
    u.nontarget(s, u.fourth(sc, s), 1, 'min', 1)


def main():
    S = tt.build()
    S['ring'][0].crop, S['ring'][1].crop = (1100, 640, 2740, 1440), (330, 232, 950, 526)
    t0 = u.tgt0
    rows = [('Today', 'The game\'s bracket, label and bars.', [tt.crop(tt.render(sc, 'today')[0], b) for sc, b in zip(S['one'], ONE)], False)]
    for p, lab, note in ((0.0, f'New target, 0 to {HOLD:.1f} s: the mini card', 'Everything at once: attitude, class, bars, name, distance, with the caret already on the ship.'),
                         (0.3, f'{HOLD + 0.1:.1f} s', 'The class line goes first, the plate thins.'),
                         (0.6, f'{HOLD + 0.2:.1f} s', 'Bars and words travel under the ship in the same order, so nothing crosses.'),
                         (1.0, f'From {HOLD + EASE:.2f} s: settled', 'Caret over the ship, two bars and one line of words under it. It stays until the target changes.')):
        rows.append((lab, note, u.pair(S, 'one', lambda s, sc, p=p: card_worn(s, sc, t0(sc), p), ONE), False))
    rows.append(('A hit on the shield', f'The remaining shield in numbers for {SHOW:.0f} s; white marks what was just lost.', u.pair(S, 'one', lambda s, sc: u.c1(s, dict(t0(sc), sh=0.62), 'h', 'c', ('s', 0.80)), ONE), False))
    rows.append(('A hit on the hull', 'The same for the hull, once the shield is down.', u.pair(S, 'one', lambda s, sc: u.c1(s, dict(t0(sc), sh=0.0, hu=0.74), 'h', 'c', ('h', 0.86)), ONE), False))
    rows.append(('Inside the ring, a new target', 'The card opens at the shoulder seat, in line with the cue; the caret stays on the ship.', u.pair(S, 'ring', lambda s, sc: card_worn(s, sc, t0(sc), 0.0, True)), False))
    rows.append(('Inside the ring, settled', 'Bars and words at the seat, in the same order as under the ship. The ring stays the player\'s.', u.pair(S, 'ring', lambda s, sc: card_worn(s, sc, t0(sc), 1.0, True)), False))
    rows.append(('Close ships', 'The target settled and in full; the others caret and name, each falling a step when it would touch something: the ship tucked against the target has only its caret.', u.pair(S, 'three', lambda s, sc: close(s, sc, 'name')), False))
    rows.append(('Close ships, no room at all', 'A bare caret on every other ship.', u.pair(S, 'three', lambda s, sc: close(s, sc, 'min')), False))
    d.sheet('Target3_candidate', 'The candidate: card first, then worn by the ship', f'The card for {HOLD:.1f} s, a {EASE:.2f} s change, then the settled form, bars at the bottom. With the picks of round 2.', rows,
            f'Left 4K, right 1280x800. Real: scene, ships, ring, meter, speed. Painted out: the game\'s bracket, label and bars. Drawn: the marks and the cue. The times are proposals. Close ships: the player\'s own group stands in for hostiles, its white marks and the off-screen arrow painted out, Ken Norris placed on the transport, the cue left out. Moving: Target3_candidate.gif, on a still frame: the ship does not move in it.')
    frames, durs = [], []
    a, b = S['one']
    for p, ms in [(0.0, 1500)] + [(i / 8, 44) for i in range(1, 8)] + [(1.0, 1700)]:
        ia = u.draw(a, lambda s, sc: card_worn(s, sc, t0(sc), p), (760, 800, 2120, 1400))
        ib = u.draw(b, lambda s, sc: card_worn(s, sc, t0(sc), p), (240, 300, 760, 530))
        ia = d.fit(ia, 640)
        ib = ib.resize((640, int(ib.height * 640 / ib.width)), Image.LANCZOS)
        fr = Image.new('RGB', (1290, max(ia.height, ib.height)), d.BG)
        fr.paste(ia, (0, 0))
        fr.paste(ib, (650, 0))
        frames.append(fr.quantize(colors=256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE))
        durs.append(ms)
    frames[0].save(os.path.join(d.OUT, 'Target3_candidate.gif'), save_all=True, append_images=frames[1:], duration=durs, loop=0)


if __name__ == '__main__':
    main()
