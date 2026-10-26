"""make_logo.py - FLvibed's logo (concept A, the owner's pick of 26 Oct 2026): HUD 2's reticle - the blue shield arc
and the white hull arc around the yellow centre - and the wordmark FL VIBED drawn as strokes in the game's condensed,
chamfered style (no font file needed). One geometry, written as:

  docs/images/logo/logo-dark.svg      mark + wordmark for dark backgrounds   (README in dark mode, the spec)
  docs/images/logo/logo-light.svg     mark + wordmark for light backgrounds  (README in light mode)
  docs/images/logo/mark-dark.svg      the mark alone, dark backgrounds
  docs/images/logo/mark-light.svg     the mark alone, light backgrounds
  docs/images/logo/icon.svg           the app icon: the mark on the dark tile, heavier strokes for small sizes
  docs/images/logo/social-preview.*   GitHub's social preview, 1280 x 640 (the owner uploads the PNG: repository
                                      Settings > General > Social preview; GitHub has no API for it)
  docs/images/logo/icon-256.png, logo-dark.png, logo-light.png   rendered by headless Edge (--png)
  tools/setup/flvibed.ico             16-256 px from icon.svg (--png), for the installer

  research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\logo\\make_logo.py [--png]
"""
import math
import os
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, 'docs', 'images', 'logo')

# the spec's palette (tools/uimock/part1_head.html): accent yellow, cyan, HUD 2's shield blue, the text white, the page
DARK = {'shield': '#5aa8ff', 'hull': '#e8ecfa', 'core': '#ffe846', 'fl': '#e8ecfa', 'vibed': '#8fd0ff', 'tile': '#0b0c14'}
LIGHT = {'shield': '#1f6fd1', 'hull': '#262b45', 'core': '#c98f00', 'fl': '#262b45', 'vibed': '#1f6fd1', 'tile': '#0b0c14'}


def arc(cx, cy, r, a0, a1):
    """SVG arc path from angle a0 to a1 (degrees, 0 = right, clockwise on screen)."""
    p0 = (cx + r * math.cos(math.radians(a0)), cy + r * math.sin(math.radians(a0)))
    p1 = (cx + r * math.cos(math.radians(a1)), cy + r * math.sin(math.radians(a1)))
    large = 1 if (a1 - a0) % 360 > 180 else 0
    return f'M{p0[0]:.2f} {p0[1]:.2f} A{r} {r} 0 {large} 1 {p1[0]:.2f} {p1[1]:.2f}'


def mark(c, x=0, y=0, size=120, heavy=False):
    """The reticle in a size x size box: two arcs (shield left, hull right) leaving gaps top and bottom, and the core."""
    s = size / 120
    cx, cy, r = x + 60 * s, y + 60 * s, 41 * s
    w = (12 if heavy else 8) * s
    gap = 26 if heavy else 24  # half-gap in degrees around top and bottom
    parts = [
        f'<path d="{arc(cx, cy, r, 90 + gap, 270 - gap)}" fill="none" stroke="{c["shield"]}" stroke-width="{w:.2f}" stroke-linecap="round"/>',
        f'<path d="{arc(cx, cy, r, -90 + gap, 90 - gap)}" fill="none" stroke="{c["hull"]}" stroke-width="{w:.2f}" stroke-linecap="round"/>',
    ]
    if heavy:
        parts.append(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{11 * s:.2f}" fill="{c["core"]}"/>')
    else:
        parts.append(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{9 * s:.2f}" fill="none" stroke="{c["core"]}" stroke-width="{3.5 * s:.2f}"/>')
        parts.append(f'<circle cx="{cx:.2f}" cy="{cy:.2f}" r="{2.6 * s:.2f}" fill="{c["core"]}"/>')
    return '\n  '.join(parts)


# ---- the wordmark: letters as stroke centrelines on a 100-high cap, stroke 13, chamfers 11 (Agency FB's cut corners)
H, S, C = 100.0, 13.0, 11.0
I = S / 2


def letter(ch):
    """(width, [polyline point lists]) of one letter, centrelines inset by half a stroke."""
    if ch == 'F':
        w = 40
        return w, [[(w, I), (I, I), (I, H)], [(I, H * 0.5), (w * 0.82, H * 0.5)]]
    if ch == 'L':
        w = 38
        return w, [[(I, 0), (I, H - I), (w, H - I)]]
    if ch == 'V':  # a filled shape (flat tops and foot), marked by a leading None
        w = 46
        return w, [[None, (0, 0), (S * 1.05, 0), (w / 2, H - S * 2.1), (w - S * 1.05, 0), (w, 0),
                    (w / 2 + S * 0.62, H), (w / 2 - S * 0.62, H)]]
    if ch == 'I':
        return S, [[(I, 0), (I, H)]]
    if ch == 'B':
        w = 44
        w1, w2 = w - I - 3, w - I
        return w, [[(I, I), (w1 - C, I), (w1, I + C), (w1, H / 2 - C * 0.6), (w1 - C * 0.6, H / 2), (I, H / 2)],
                   [(I, H / 2), (w2 - C, H / 2), (w2, H / 2 + C), (w2, H - I - C), (w2 - C, H - I), (I, H - I), (I, I - I)]]
    if ch == 'E':
        w = 38
        return w, [[(w, I), (I, I), (I, H - I), (w, H - I)], [(I, H / 2), (w * 0.84, H / 2)]]
    if ch == 'D':
        w = 46
        k = C * 1.4
        return w, [[(I, I), (w - I - k, I), (w - I, I + k), (w - I, H - I - k), (w - I - k, H - I), (I, H - I), (I, I - I)]]
    raise ValueError(ch)


def wordmark(c, x, y, cap):
    """'FL' and 'VIBED' at (x, y) top-left, cap height cap; returns (svg, width)."""
    sc = cap / H
    track, word_gap = 15.0, 15.0
    out, pen = [], 0.0
    for k, ch in enumerate('FLVIBED'):
        if k == 2:
            pen += word_gap - track
        w, lines = letter(ch)
        colour = c['fl'] if k < 2 else c['vibed']
        for pts in lines:
            filled = pts[0] is None
            pts = pts[1:] if filled else pts
            d = 'M' + ' L'.join(f'{x + (pen + px) * sc:.2f} {y + py * sc:.2f}' for px, py in pts)
            if filled:
                out.append(f'<path d="{d} Z" fill="{colour}"/>')
            else:
                out.append(f'<path d="{d}" fill="none" stroke="{colour}" stroke-width="{S * sc:.2f}" '
                           f'stroke-linejoin="miter" stroke-miterlimit="2" stroke-linecap="butt"/>')
        pen += w + track
    return '\n  '.join(out), (pen - track) * sc


def svg(w, h, body, title):
    return (f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w:.0f} {h:.0f}" width="{w:.0f}" height="{h:.0f}" '
            f'role="img" aria-label="{title}">\n  <title>{title}</title>\n  {body}\n</svg>\n')


def logo(c):
    """Mark left, wordmark right (cap = 46 % of the mark), vertically centred."""
    m = 120
    cap = 54
    words, ww = wordmark(c, m + 26, (m - cap) / 2, cap)
    return svg(m + 26 + ww + 4, m, mark(c) + '\n  ' + words, 'FLvibed')


def social():
    """GitHub's social preview (1280 x 640, what Discord and others show for the repository's link): the logo on the
    spec's page colour, a tagline in Bahnschrift (Windows' own condensed face; the PNG is rendered here, so viewers need
    no font), GitHub keeps 40 px clear at the edges."""
    m = 210
    cap = round(m * 0.45)
    words, ww = wordmark(DARK, 0, 0, cap)
    total = m + 46 + ww
    x0, y0 = (1280 - total) / 2, 170
    body = [f'<rect width="1280" height="640" fill="{DARK["tile"]}"/>',
            f'<rect x="0" y="606" width="1280" height="6" fill="{DARK["core"]}" opacity="0.9"/>',
            mark(DARK, x0, y0, m),
            f'<g transform="translate({x0 + m + 46:.1f} {y0 + (m - cap) / 2:.1f})">' + wordmark(DARK, 0, 0, cap)[0] + '</g>',
            '<text x="640" y="488" text-anchor="middle" font-family="Bahnschrift, \'Bahnschrift SemiCondensed\', '
            '\'Arial Narrow\', sans-serif" font-stretch="semi-condensed" font-size="44" letter-spacing="2" fill="#9aa3c7">'
            'Controller play, subtitles and better movies for Freelancer</text>',
            '<text x="640" y="548" text-anchor="middle" font-family="Bahnschrift, \'Arial Narrow\', sans-serif" '
            'font-size="30" letter-spacing="3" fill="#5f6a94">FREELANCER (2003) · FREELANCER: HD EDITION</text>']
    return svg(1280, 640, '\n  '.join(body), 'FLvibed')


def icon():
    t = DARK['tile']
    body = f'<rect x="4" y="4" width="112" height="112" rx="22" fill="{t}"/>\n  ' + mark(DARK, 10, 10, 100, heavy=True)
    return svg(120, 120, body, 'FLvibed')


def write(name, text):
    with open(os.path.join(OUT, name), 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    print('wrote', os.path.relpath(os.path.join(OUT, name), ROOT))


def edge():
    for p in (r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe', r'C:\Program Files\Microsoft\Edge\Application\msedge.exe'):
        if os.path.exists(p):
            return p
    raise SystemExit('Microsoft Edge not found (needed to render the PNGs)')


def render(svg_name, png_path, width, height, tmp):
    """The SVG at width x height on a transparent page, by headless Edge."""
    page = os.path.join(tmp, 'page.html')
    with open(os.path.join(OUT, svg_name), encoding='utf-8') as f:
        body = f.read()
    with open(page, 'w', encoding='utf-8') as f:
        f.write('<!doctype html><meta charset="utf-8"><style>html,body{margin:0;background:transparent}'
                f'svg{{display:block;width:{width}px;height:{height}px}}</style>' + body)
    subprocess.run([edge(), '--headless=new', '--disable-gpu', '--hide-scrollbars', '--mute-audio',
                    f'--user-data-dir={os.path.join(tmp, "prof")}', '--force-device-scale-factor=1',
                    '--default-background-color=00000000', f'--window-size={width},{height}',
                    f'--screenshot={png_path}', 'file:///' + page.replace('\\', '/')],
                   check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=90)


def main():
    os.makedirs(OUT, exist_ok=True)
    write('logo-dark.svg', logo(DARK))
    write('logo-light.svg', logo(LIGHT))
    write('mark-dark.svg', svg(120, 120, mark(DARK), 'FLvibed'))
    write('mark-light.svg', svg(120, 120, mark(LIGHT), 'FLvibed'))
    write('icon.svg', icon())
    write('social-preview.svg', social())
    if '--png' not in sys.argv:
        return
    from PIL import Image
    tmp = tempfile.mkdtemp(prefix='flvlogo_')
    try:
        render('icon.svg', os.path.join(OUT, 'icon-256.png'), 256, 256, tmp)
        print('wrote docs/images/logo/icon-256.png')
        render('social-preview.svg', os.path.join(OUT, 'social-preview.png'), 1280, 640, tmp)
        print('wrote docs/images/logo/social-preview.png')
        for v in ('dark', 'light'):
            with open(os.path.join(OUT, f'logo-{v}.svg'), encoding='utf-8') as f:
                vb = f.read().split('viewBox="0 0 ')[1].split('"')[0].split()
            w, h = float(vb[0]), float(vb[1])
            render(f'logo-{v}.svg', os.path.join(OUT, f'logo-{v}.png'), round(w * 4), round(h * 4), tmp)
            print(f'wrote docs/images/logo/logo-{v}.png')
        big = os.path.join(tmp, 'icon-1024.png')
        render('icon.svg', big, 1024, 1024, tmp)
        img = Image.open(big).convert('RGBA')
        ico = os.path.join(ROOT, 'tools', 'setup', 'flvibed.ico')
        img.save(ico, sizes=[(16, 16), (20, 20), (24, 24), (32, 32), (40, 40), (48, 48), (64, 64), (128, 128), (256, 256)])
        print('wrote tools/setup/flvibed.ico')
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == '__main__':
    main()
