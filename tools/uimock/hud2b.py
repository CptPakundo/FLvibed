"""hud2b.py - both glows breathing (the owner, 6 Nov 2026): the cue's lock relatively fast, the waypoint diamond somewhat slower. A clip of a locked target that is also the waypoint.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2b.py

On a real frame, still: only the marks move. Nothing here is built. Written to research\\evidence\\hud2_gate\\decisions\\Target3_breathing.gif.
"""
import math
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2d as d  # noqa: E402
import hud2f as f  # noqa: E402
import hud2s as hs  # noqa: E402
import hud2t as tt  # noqa: E402
import hud2v as v  # noqa: E402
import hud2z as z  # noqa: E402

LOCK_HZ = f.HZ     # 1 Hz, the owner's D4
WAY_HZ = 0.5       # one breath for two of the lock's: kept by the owner, 6 Nov 2026
LOW, HIGH = 70, 200
LOOP, FPS = 2.0, 12.5


def frame(sc, t, box):
    s = hs.Skin(sc.clean)
    ph = (t * LOCK_HZ) % 1.0
    f.cue(s, sc.k, [('1', dict(sel=True, reach=True)), ('2', dict(reach=True)), ('3', {}), ('M', dict(reach=True, lock=True, phase=ph)), ('T', dict(reach=True, lock=True, phase=ph))])
    wave = 0.5 + 0.5 * math.sin(2 * math.pi * WAY_HZ * t)
    tg = sc.targets[0]
    z.diamond(s, tg['x'], tg['y'] - s.h(tg['half']) * 1.22, glow=int(LOW + (HIGH - LOW) * wave))
    v.card_worn(s, sc, tg, 1.0, True)
    return Image.open(d.save(s, 'b_' + sc.tag)).convert('RGB').crop(box)


def main():
    S = tt.build()
    a, b = S['ring']
    frames = []
    n = int(LOOP * FPS)
    for i in range(n):
        t = i / FPS
        ia = d.fit(frame(a, t, (1230, 760, 2340, 1320)), 640)
        ib = d.fit(frame(b, t, (385, 276, 800, 490)), 640)
        fr = Image.new('RGB', (1290, max(ia.height, ib.height)), d.BG)
        fr.paste(ia, (0, 0))
        fr.paste(ib, (650, 0))
        frames.append(fr.quantize(colors=256, method=Image.Quantize.FASTOCTREE, dither=Image.Dither.NONE))
    frames[0].save(os.path.join(d.OUT, 'Target3_breathing.gif'), save_all=True, append_images=frames[1:], duration=int(1000 / FPS), loop=0)
    print(n, 'frames')


if __name__ == '__main__':
    main()
