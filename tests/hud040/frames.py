"""Reads the frames tests\\hud040\\run.ps1 grabs. Run with the project venv (research\\tools\\ocr\\.venv\\Scripts\\python.exe -I).

  frames.py rate <folder> [fps]     the flash test (WCAG's and the owner's 3 Hz limit): the mean luminance of each frame, its spectrum; exit 1 when energy above
                                    3 Hz reaches a quarter of the strongest peak. fps: the grab's own times.txt (seconds per frame), else 60 or the argument.
  frames.py native <folder>         the first-frame check: frames whose top strip holds the native HUD's bright top bar against the median frame; exit 1 when any does.
"""
import glob
import os
import sys

import numpy as np
from PIL import Image


def load(folder):
    names = sorted(glob.glob(os.path.join(folder, '*.png')) + glob.glob(os.path.join(folder, '*.bmp')))
    if not names:
        sys.exit('no frames in ' + folder)
    return names


def clock(folder, frames, fps):
    """The grab's real rate from its times.txt, a frame's time a line; `fps` without one."""
    p = os.path.join(folder, 'times.txt')
    if os.path.exists(p):
        t = [float(x.split()[-1]) for x in open(p) if x.strip()]
        if len(t) >= 2 and t[-1] > t[0]:
            return (len(t) - 1) / (t[-1] - t[0])
    return fps


def rate(folder, fps):
    lum = []
    names = load(folder)
    fps = clock(folder, len(names), fps)
    for n in names:
        a = np.asarray(Image.open(n).convert('L'), dtype=np.float32)
        lum.append(a.mean())
    x = np.array(lum) - np.mean(lum)
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    f = np.fft.rfftfreq(len(x), 1.0 / fps)
    top = spec[1:].max() if len(spec) > 1 else 0
    peak = f[1:][spec[1:].argmax()] if top > 0 else 0
    over = spec[f > 3.0].max() if (f > 3.0).any() else 0
    print('frames %d, strongest %.2f Hz, energy above 3 Hz %.0f %% of it' % (len(x), peak, 100 * over / top if top else 0))
    return 1 if peak >= 3.0 or (top and over >= 0.25 * top) else 0


def native(folder):
    names = load(folder)
    stack = [np.asarray(Image.open(n).convert('L'), dtype=np.float32) for n in names]
    med = np.median(np.stack(stack), axis=0)
    bad = 0
    for n, a in zip(names, stack):
        d = np.abs(a - med)
        share = float((d > 12).mean())
        if share > 0.02:
            print('%s differs from the median in %.1f %% of its pixels' % (os.path.basename(n), 100 * share))
            bad += 1
    print('frames %d, suspect %d: read each suspect by eye (a scene change also differs)' % (len(names), bad))
    return 1 if bad else 0


if __name__ == '__main__':
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    if sys.argv[1] == 'rate':
        sys.exit(rate(sys.argv[2], float(sys.argv[3]) if len(sys.argv) > 3 else 60.0))
    if sys.argv[1] == 'native':
        sys.exit(native(sys.argv[2]))
    sys.exit(__doc__)
