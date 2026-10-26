"""hudmock_variants.py - the pad-HUD proposals of the refinement round (25 Oct 2026), rendered by hudmock.py.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hudmock_variants.py

Bases (pad layer off, SaveC702, Fort Bush targeted, instance 8): base_travel-*.png (calm), base_contacts2-*.png (two
Rogues inbound). Outputs: <variant>_<context>.png (4K) + _1080.jpg + crops, in research\\evidence\\controller_ui\\
hud_redesign\\. The loadout drawn in the clusters is illustrative (the SaveC702 ship carries no launcher, flares or
mines; the M02 ship of SaveC705 does).
"""
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hudmock as hm  # noqa: E402
from hudmock import CHROME, ACTIVE, DIM, CYAN, RIM  # noqa: E402

E = hm.EVID


def base(name):
    return sorted(glob.glob(os.path.join(E, name + '-*.png')))[-1]


# The game's panels at 4K (HudShift layout, measured on the captures)
STRIP = (3160, 1490, 3765, 1620)       # the consumables + weapon-group header over the weapons list
TGT_HEAD = (85, 1500, 680, 1612)       # the contact list's header (expander, next enemy)
MBAR = (60, 0, 1215, 190)              # the maneuver bar with the autopilot button
CRUISE = (110, 182, 265, 330)          # the cruise box under the bar's left end
NN = (3085, 0, 3840, 128)               # the NeuroNet box

CTX = {
    'travel': dict(base='base_travel', title=None,
                   legend=[('A', 'DOCK AT FORT BUSH', None), ('Y', 'CRUISE', None), ('LB', 'TARGETS', None),
                           ('R3', 'TARGET AHEAD', None), ('VIEW', 'MAP', None)]),
    'combat': dict(base='base_contacts2', title='COMBAT',
                   legend=[('R3', 'NEXT ENEMY', None), ('LB', 'TARGETS', None), ('B', 'REVERSE', 1.0),
                           ('X', 'TURRET VIEW', 1.0)]),
}

# Glyph size: 0.04 H in the legend and the cluster (the research round's floor for a primary prompt at TV distance
# is ~0.035-0.045 H; the built badges were 0.022-0.036 H, the built legend 0.029 H).
G = 0.040


def legend(c, items, title, right=0.985, top=None, gsize=G):
    """The contextual legend, restyled: glyph column, label, holds as a ring around the glyph (no HOLD word)."""
    H = c.H
    g = c.h(gsize)
    row = g * 1.42
    lab = c.h(gsize * 0.78)
    widths = [c.textlen(t, lab) for _, t, _ in items]
    w = g * 1.5 + max(widths)
    x0 = c.W * right - w
    n = len(items) + (1 if title else 0)
    y = top if top is not None else H * 0.47 - n * row / 2
    y = min(y, H * 0.665 - (n - 0.45) * row)   # keep clear of the bottom panels' headers (as the built legend does)
    # the edge line, the HUD's frame colour
    c.panel((x0 - g * 0.55, y - row * 0.55, x0 - g * 0.55 + max(2, H * 0.0012), y + (n - 0.45) * row),
            chamfer=0, alpha=255, rim=RIM, glow=False)
    if title:
        c.text((x0, y), title, lab * 0.82, fill=DIM)
        y += row
    for (k, t, hold) in items:
        c.glyph(k, x0 + g * 0.5, y, g, hold=hold)
        c.text((x0 + g * 1.5, y), t, lab)
        y += row


def cluster(c, combat=True):
    """The pad loadout cluster replacing the consumables / weapon-group header: the D-pad cross with its four
    consumables (spatial mapping, the Elden Ring / Halo / Horizon pattern), the trigger rows (RT the active gun group
    with the game's 1 2 3 digits, RB the launcher with its count)."""
    src = c.src
    c.erase(STRIP)
    # 1. the weapons strip, in the native header's footprint (a little taller): RT the active gun group with the
    #    game's own 1 2 3 digits (lit yellow), RB the launcher and its count
    x0, x1, y1 = 3165, 3765, 1622
    y0 = y1 - c.h(0.072)
    c.panel((x0, y0, x1, y1), foot=True)
    cy = (y0 + y1) / 2 - c.h(0.004)
    g = c.h(0.038)
    x = x0 + c.h(0.032)
    c.glyph('RT', x, cy, g)
    w, _ = c.icon_from(src, (3525, 1518, 3718, 1602), x + g * 0.62 + 75, cy, c.h(0.030))
    x += g * 0.62 + w + c.h(0.016)
    c.panel((x, cy - c.h(0.020), x + 2, cy + c.h(0.020)), chamfer=0, alpha=255, glow=False)   # divider
    x += c.h(0.016)
    c.glyph('RB', x + g * 0.62, cy, g)
    x += g * 1.3
    c.icon('missile', x + c.h(0.018), cy, c.h(0.030))
    c.text((x + c.h(0.040), cy), '36', c.h(0.030))
    # 2. the D-pad cross over the own-ship panel: four slots in the pad's own geometry (spatial mapping: the layout
    #    is the glyph), each the consumable's icon and count; the pressed slot flashes (not drawn)
    cx, cy = 2848, 1468
    d = c.h(0.064)
    s = c.h(0.058)
    c.glyph('D', cx, cy, c.h(0.034), alpha=235)
    slots = [('wrench', '3', (0, -1)), ('battery', '3', (0, 1)), ('flare', '12', (-1, 0)), ('mine', '8', (1, 0))]
    for ic, n, (dx, dy) in slots:
        px, py = cx + dx * d * 1.12, cy + dy * d
        c.panel((px - s / 2, py - s / 2, px + s / 2, py + s / 2), chamfer=c.h(0.010), alpha=150, glow=False)
        ix, iy = px - s * 0.10, py - s * 0.08
        if ic == 'wrench':
            c.icon_from(src, (3192, 1512, 3255, 1600), ix, iy, s * 0.54)
        elif ic == 'battery':
            c.icon_from(src, (3352, 1512, 3430, 1602), ix, iy, s * 0.54)
        else:
            c.icon(ic, ix, iy, s * 0.50)
        c.text((px + s * 0.40, py + s * 0.25), n, c.h(0.020), anchor='rm')


def neuronet_tab(c):
    """The NeuroNet box's five mouse buttons collapsed into one tab: View opens the hub (map first); a dot when the
    log or the map has news (the game's own flash)."""
    c.erase(NN)
    g = c.h(0.034)
    x1, y0 = 3765, 14
    label = 'NEURONET'
    w = g * 1.6 + c.textlen(label, c.h(0.027)) + c.h(0.03)
    c.panel((x1 - w, y0, x1, y0 + c.h(0.050)), chamfer=c.h(0.012))
    cy = y0 + c.h(0.025)
    c.glyph('VIEW', x1 - w + c.h(0.012) + g * 0.5, cy, g)
    c.text((x1 - w + c.h(0.018) + g * 1.1, cy), label, c.h(0.027))
    c.dot(x1 - c.h(0.014), cy - c.h(0.010), c.h(0.0055))


def world_prompt(c, x, y, items, arrow=True):
    """The action where the eye already is: beside the target's bracket or off-screen arrow (the Fenyx / Squadrons
    pattern), glyph = line height, label outlined, at most two."""
    g = c.h(0.040)
    for k, t, hold in items:
        c.glyph(k, x + g * 0.5, y, g, hold=hold)
        c.text((x + g * 1.2, y), t, c.h(0.031))
        x += g * 1.5 + c.textlen(t, c.h(0.031)) + c.h(0.02)


def render(variant, ctx):
    spec = CTX[ctx]
    c = hm.Canvas(base(spec['base']))
    c.src = c.im.copy()
    # A keeps the built style's family colours; B and C use the HUD tint the space games use (Everspace 2,
    # Starfield, Squadrons: monochrome glyphs in the HUD's colour)
    hm.COLORED = variant == 'A'
    if variant == 'A':      # clean overlay: no badges on the game's HUD; the legend carries every action
        items = list(spec['legend'])
        if ctx == 'combat':
            items = [('RT', 'FIRE', None), ('RB', 'MISSILE  36', None), ('DU', 'NANOBOT  3', None),
                     ('DD', 'BATTERY  3', None), ('DL', 'COUNTERMEASURE  12', None), ('DR', 'MINE  8', None)] + items
        legend(c, items, spec['title'])
    elif variant == 'B':    # the pad cluster replaces the strip; the legend keeps only what the cluster does not show
        cluster(c, ctx == 'combat')
        legend(c, spec['legend'], spec['title'], top=None)
    elif variant == 'C':    # minimal pad cockpit: B's cluster, the action at the target, NeuroNet as one tab,
        cluster(c, ctx == 'combat')   # the legend only while learning (first contexts, idle) - not drawn here
        neuronet_tab(c)
        if ctx == 'travel':
            world_prompt(c, 1300, 1352, [('A', 'DOCK', None), ('Y', 'CRUISE', None)])
        else:
            world_prompt(c, 2930, 760, [('R3', 'NEXT ENEMY', None)], arrow=False)
    out = os.path.join(E, f'{variant}_{ctx}.png')
    c.save(out, crops={'br': (2560, 1080, 3840, 2160), 'tl': (0, 0, 1400, 500)})
    return out


def render_styles():
    """The glyph styles side by side on the same patch of space and HUD: the family colours on the letter (the
    built style), neutral HUD tint (Steam Input's NeutralColorABXY idea), and the built badge size for scale."""
    c = hm.Canvas(base('base_contacts2'))
    H = c.H
    keys = ['A', 'B', 'X', 'Y', 'LB', 'RT', 'R3', 'DU', 'VIEW']
    rows = [('FAMILY COLOURS (letters)', True, 0.040), ('HUD TINT (neutral)', False, 0.040),
            ('BUILT BADGE SIZE (0.022 H)', True, 0.022)]
    y = 1180
    for title, colored, size in rows:
        c.text((2050, y - c.h(0.035)), title, c.h(0.020), fill=DIM)
        x = 2080
        for k in keys:
            c.glyph(k, x, y, c.h(size), colored=colored)
            x += c.h(max(size, 0.03) * 1.35)
        y += c.h(0.085)
    c.im.convert('RGB').crop((2000, 1040, 3840, 1700)).save(os.path.join(E, 'styles_crop.png'))


if __name__ == '__main__':
    if sys.argv[1:] == ['styles']:
        render_styles()
        sys.exit()
    for v in sys.argv[1:] or ['A', 'B']:
        for ctx in CTX:
            print(render(v, ctx))
