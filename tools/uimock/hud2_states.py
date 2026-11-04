"""hud2_states.py - HUD 2 (the ground-up pad HUD) rendered per flight state on HUD-free 4K frames. Iteration 2
(25 Oct 2026): the critique of iteration 1 applied (a smaller ring with shield / hull / power on separate arcs, no
duplicate gauges, every text >= 0.024 H, outlined free text, the D-pad cross inside the loadout panel, the comm feed
left, the context line under the ship, one place per state) plus the missing combat cues (lead pip with in-range
state, missile lock, incoming missile, damage bearing, off-screen hostiles, the protected ship, escorts).

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2_states.py [combat travel cruise dock lane]

Layout (fractions of H; anchored to the safe frame's corners: 5 % of W at the sides, 5 % of H top and bottom):
  top-left      NAV plate: maneuver, objective (+ the protected ship's hull), place, NeuroNet
  top-centre    one notification at a time (none when a comm line carries the same event)
  top-right     TARGET card (+ in combat the threat list: LB TARGETS, R3 NEXT, the filter)
  centre        the RETICLE RING, radius 0.10 H: shield arc left, hull arc right, power arc below; the speed inside the
                ring's bottom; numbers only when low or changing; alerts on the ring's outside (missile bearing, damage
                bearing); off-screen arrows clamped to it
  left-middle   the comm feed (the subtitles module's lines), above the ship panel
  bottom-left   SHIP panel: the game's wireframe of the player's ship with its damage, escorts, hold, credits
  bottom-centre the context line under the player's ship (at most three, holds as a broken ring)
  bottom-right  LOADOUT panel: the D-pad cross and the weapon groups (">" on the active), the launcher and its lock

Since 31 Oct 2026 it draws the BUILT Standard layout (docs\\UI_HUD2.md 5.19-5.23), not the proposal above where they
differ: both bottom panels 0.15 H tall (ship 0.31 wide, loadout base 0.40 with the consumables cross on its own 0.20
plate stacked 0.012 above the base's right end), tight glyphs in the loadout, a too-long name in its cut start form (the
marquee), the context prompts in two lanes beside the panels (movement / control left, fire / use / target right), the
cruise state inside the ring (seconds / CRUISE ON at +0.083), the throttle word beside its arc, and nothing under the
ring (the lower centre). Renders also feed the living spec page (tools\\uimock\\spec_images.py, img\\cap\\m_*.jpg).
"""
import glob
import math
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2  # noqa: E402
from hud2 import (INK, CHROME, DIM, CYAN, ACTIVE, HOSTILE, FRIEND, SHIELD, HULL, POWER)  # noqa: E402

PLATE = 'plate_subs.png'      # the subtitles' area from a capture without lines (the camera is static: same sky)
PLATE_XY = (0, 900)
T = 0.024                     # the text floor (XAG 101: 26 px at 1080p); captions never go below it
G = 0.036                     # glyph height in prompts and on clusters
# The built Standard layout (31 Oct 2026, docs\UI_HUD2.md 5.19-5.23; fractions of H at 16:9, k 1)
PANEL_H = 0.15                # both bottom panels (A5)
SHIP_W = 0.31                 # the ship panel (5.22): icon column 0.085, texts from 0.107, values 0.014 before the edge
BASE_W = 0.40                 # the loadout's base (5.20 decision 3: the weapons only; K&M 0.44)
CROSS_W = 0.20                # the consumables cross on its own plate, stacked on the base's right end, 0.012 above it
CROSS_GAP = 0.012
TIGHT = 0.0288                # the tight keycap / glyph (A1): 1.2 x the 0.024 name
LANE_ROWS = (0.915, 0.855)    # the context lanes' two rows (5.20 decision 4)


def fit(c, s, size, room, floor=T):
    """The kit's fit rule as the frame shows it: step down to the floor, then the marquee's cut start form (the text's
    start + "..."; in game it holds 1.5 s, then scrolls to the end - 5.20 decision 1)."""
    H = c.H
    sz = size
    while c.textlen(s, sz) > room and sz > floor * H:
        sz = max(floor * H, sz - 0.001 * H)
    if c.textlen(s, sz) <= room:
        return s, sz
    while s and c.textlen(s + '...', sz) > room:
        s = s[:-1]
    return s.rstrip() + '...', sz

SHIP = dict(hull=0.88, shield=0.22, power=0.64, speed=84, throttle=0.75, cruise='off', nano=3, batt=3, cm=12, mines=8,
            missiles=36, hold=(12, 40), credits=4250)
CALM = dict(SHIP, hull=1.0, shield=1.0, power=1.0, speed=80, throttle=1.0)
# group 1 is too long for the names' ~0.23 H room: it shows the marquee's cut start form (5.20 decision 1)
GROUPS = ['Azrael ×2  ·  Lavablade Mk II ×2', 'Azrael ×2', 'Lavablade Mk II ×2']
ESCORTS = [('Escort 1 · Charackli', 1.0), ('Escort 2 · Cazamias', 0.7), ('Escort 3 · Carlson', 0.35)]


class Layout:
    def __init__(self, c):
        H, W = c.H, c.W
        self.mx, self.my = 0.05 * W, 0.05 * H
        self.cx, self.cy = W / 2, H / 2
        self.R = 0.10 * H


# ------------------------------------------------------------------ the reticle ring
def reticle(c, L, s, combat=True, charge=None, missile=None, damage=None, word=None):
    """Shield left (138..222), hull right (-42..42), power below (68..112); the speed inside the ring above the power
    arc; a number at its arc's end only when below 40 %. Calm: 40 %. The throttle arc (82..98) just outside the ring's
    bottom with its word (BOOST / KILL / REV) RIGHT of it, not under it (5.23). Cruise reads in one place, inside the
    ring in the charge seconds' slot (+0.083): the seconds while the bottom arc fills cyan, CRUISE ON while it runs;
    the speed rises to +0.045 and the power arc yields meanwhile. Nothing HUD 2 draws lies under the ring (the lower
    centre, x 0.40-0.60 W from the throttle arc + 0.01 H down: 5.23). Alerts outside: the missile's bearing on the
    ring (pulsing), a hit's side as a transient amber flash at 0.13 H."""
    H = c.H
    cx, cy, R = L.cx, L.cy, L.R
    w = 0.0065 * H
    cruising = charge is not None or s['cruise'] == 'on'
    c.op = 1.0 if combat else 0.4
    low_shield = s['shield'] <= 0.25
    c.arcbar(cx, cy, R, 138, 222, w, s['shield'], HOSTILE if low_shield else SHIELD, n=14, reverse=True)
    c.arcbar(cx, cy, R, -42, 42, w, s['hull'], HULL, n=14, reverse=True)
    if not cruising:
        c.arcbar(cx, cy, R, 68, 112, w * 0.8, s['power'], POWER, n=8, gapdeg=2.2)
    c.arcbar(cx, cy, R + 0.022 * H, 82, 98, w * 0.75, s['throttle'], CHROME, n=5, gapdeg=1.2)   # throttle
    f = T * H
    if s['shield'] < 0.4:
        c.text((cx - R * 0.72, cy - R * 0.92), f"{int(s['shield'] * 100)}", f, fill=HOSTILE, anchor='rm')
    if s['hull'] < 0.4:
        c.text((cx + R * 0.72, cy - R * 0.92), f"{int(s['hull'] * 100)}", f, fill=HULL, anchor='lm')
    c.op = 1.0
    if word:   # beside the throttle arc: from cx + 0.068, its box's bottom on the arc's outer edge
        c.text((cx + 0.068 * H, cy + R + 0.022 * H + w * 0.375 - f / 2), word, f, fill=ACTIVE, anchor='lm', track=0.08)
    if charge is not None:
        c.arcbar(cx, cy, R, 68, 112, w, charge, CYAN, n=8, gapdeg=2.2, reverse=True)
        c.text((cx, cy + 0.083 * H), f"{(1 - charge) * 5:.1f} s", f, fill=CYAN, anchor='mm')
    elif s['cruise'] == 'on':
        c.text((cx, cy + 0.083 * H), 'CRUISE ON', f, fill=CYAN, anchor='mm', track=0.08)
    c.text((cx, cy + (0.045 if cruising else 0.058) * H), f"{s['speed']}", 0.030 * H, fill=INK, anchor='mm')
    r = 0.018 * H   # the game's own reticle (kept native; drawn here as it looks)
    c.shape(lambda d, P, S: (d.ellipse([P(cx - r, cy - r), P(cx + r, cy + r)], outline=CHROME + (230,),
                                       width=int(0.0016 * H * S)),
                             d.ellipse([P(cx - 3, cy - 3), P(cx + 3, cy + 3)], fill=CHROME + (255,))),
            box=(cx - r - 4, cy - r - 4, cx + r + 4, cy + r + 4))
    if damage is not None:
        rd = 0.13 * H
        c.shape(lambda d, P, S: d.arc([P(cx - rd, cy - rd), P(cx + rd, cy + rd)], damage - 16, damage + 16,
                                      fill=(255, 196, 120, 190), width=int(0.006 * H * S)),
                glow=0.006 * H, box=(cx - rd - 0.03 * H, cy - rd - 0.03 * H, cx + rd + 0.03 * H, cy + rd + 0.03 * H))
    if missile is not None:
        ang = math.radians(missile)
        ux, uy = math.cos(ang), math.sin(ang)
        vx, vy = -uy, ux
        for k, rr in enumerate((R + 0.022 * H, R + 0.044 * H)):
            px, py = cx + rr * ux, cy + rr * uy
            sz = 0.018 * H
            c.line([(px + vx * sz, py + vy * sz), (px - ux * sz * 0.8, py - uy * sz * 0.8), (px - vx * sz, py - vy * sz)],
                   col=HOSTILE, w=0.005 * H, alpha=255 - k * 80)


def direction(c, L, ang, col, dist):
    """Every off-screen object of interest (the target, hostiles) on ONE ellipse around the centre (0.62 x 0.40 H),
    one chevron shape (the game's arrow) in the stance colour, the distance under it."""
    H = c.H
    a = math.radians(ang)
    x, y = L.cx + 0.62 * H * math.cos(a), L.cy + 0.40 * H * math.sin(a)
    c.arrow(x, y, 0.040 * H, -ang, col=col)
    c.text((x, y + 0.036 * H), dist, T * H, fill=col, anchor='mm')

def context_lanes(c, L, left=(), right=()):
    """The context prompts (at most three) in two lanes tight to the bottom panels (5.20 decision 4, the corners'
    jobs): the ship's movement and control LEFT beside the ship panel (DOCK, CRUISE / CANCEL CRUISE, STOP, ABORT DOCK,
    TAKE CONTROL, MAP ...), what you fire / use / target RIGHT beside the loadout (TURRET, TRACTOR n, TARGET AHEAD,
    TARGETS); the first prompt hugs its panel, two rows a lane (the baseline 0.915 H and one 0.06 above). A prompt too
    wide for an empty row is squeezed (its label in the marquee's cut form). The lower centre stays empty."""
    H, W = c.H, c.W
    g = G * H
    lab = 0.026 * H
    pad, gap = 0.012 * H, 0.024 * H
    # the prompts' own extents (a pill reaches pad further): panel + 0.012 gap + 0.012 pad .. 0.40 W - pad; the right
    # lane mirrored (0.60 W + pad .. the loadout - 0.024)
    lanes = ((left, L.mx + SHIP_W * H + 0.024 * H, 0.40 * W - pad, 1),
             (right, 0.60 * W + pad, W - L.mx - BASE_W * H - 0.024 * H, -1))
    for items, x0, x1, way in lanes:
        row, used = 0, 0.0
        for k, t, hold in items:
            w = g * 1.3 + c.textlen(t, lab, track=0.06)
            if used and used + gap + w > x1 - x0:
                row, used = row + 1, 0.0
            if row >= len(LANE_ROWS):
                break
            if w > x1 - x0:   # squeezed to the lane: the label's cut start form
                t, _ = fit(c, t, lab, x1 - x0 - g * 1.3)
                w = g * 1.3 + c.textlen(t, lab, track=0.06)
            y = H * LANE_ROWS[row]
            x = x0 + used if way > 0 else x1 - used - w
            c.plate((x - pad, y - 0.026 * H, x + w + pad, y + 0.026 * H), alpha=150, rim_alpha=0,
                    corners=(1, 1, 1, 1), chamfer=0.026 * H)
            c.glyph(k, x + g / 2, y, g, colored=False, hold=hold)
            c.text((x + g * 1.3, y), t, lab, fill=INK, track=0.06)
            used += w + gap

def world_target(c, x, y, w, h, dist, col=HOSTILE, lead=None, in_range=True, lock=None):
    """The game's corner brackets in the attitude colour and the distance under them (the card has the rest); the
    lead pip = a small ring with a centre dot, filled when in gun range (diamonds are objectives only); the missile
    lock closing around the brackets."""
    H = c.H
    c.bracket(x, y, w, h, col=col, size=0.024 * H)
    r = max(w, h) * 0.75
    c.text((x, y + (r if lock is not None else h / 2) + 0.024 * H), dist, T * H, fill=col, anchor='mm')
    if lock is not None:
        c.arcbar(x, y, r, -90, 270, 0.0035 * H, lock, col, n=24, gapdeg=3, empty=(70, 30, 40))
    if lead:
        lx, ly = lead
        rr = 0.012 * H
        c.shape(lambda d, P, S: (d.ellipse([P(lx - rr, ly - rr), P(lx + rr, ly + rr)],
                                           fill=(ACTIVE + (110,)) if in_range else None, outline=ACTIVE + (255,),
                                           width=int(0.003 * H * S)),
                                 d.ellipse([P(lx - 3, ly - 3), P(lx + 3, ly + 3)], fill=ACTIVE + (255,))),
                box=(lx - rr - 4, ly - rr - 4, lx + rr + 4, ly + rr + 4))

def nav(c, L, objective, protect=None, mode=None, mode_col=CHROME, man='free', dest=None, ap=False, progress=None,
        eta=None, place=None):
    """No box: free text on a soft scrim that fades inward. The objective is the headline; the maneuver is a chip
    (shown when one runs, with the AUTOPILOT chip when FLvibed flies) with progress and ETA; the place shows for 5 s
    after a system change; NeuroNet on View."""
    H = c.H
    x, y = L.mx, L.my + 0.02 * H
    sc_w, sc_h = 0.70 * H, 0.24 * H
    c.shape(lambda d, P, S: [d.rectangle([P(x - 0.06 * H, y - 0.08 * H + k * sc_h / 24), P(x - 0.06 * H + sc_w * (1 - k / 30),
                                          y - 0.08 * H + (k + 1) * sc_h / 24)], fill=(4, 4, 16, int(80 * (1 - k / 24))))
                             for k in range(24)], box=(x - 0.06 * H, y - 0.08 * H, x - 0.06 * H + sc_w, y - 0.08 * H + sc_h))
    c.shape(lambda d, P, S: d.polygon([P(x + 0.01 * H, y - 0.01 * H), P(x + 0.02 * H, y), P(x + 0.01 * H, y + 0.01 * H),
                                       P(x, y)], outline=ACTIVE + (255,), width=int(0.002 * H * S)),
            box=(x - 2, y - 0.012 * H, x + 0.022 * H, y + 0.012 * H))
    tx = x + 0.032 * H
    ow = c.text((tx, y), objective, 0.032 * H, fill=INK)
    if protect is not None:
        c.segbar(tx + ow + 0.016 * H, y - 0.005 * H, 0.08 * H, 0.010 * H, protect, FRIEND, n=8, gap=2)
    yy = y + 0.05 * H
    if mode:
        chip_x = tx
        if ap:
            aw = c.textlen('AUTOPILOT', T * H, track=0.1)
            c.plate((chip_x, yy - 0.02 * H, chip_x + aw + 0.016 * H, yy + 0.02 * H), alpha=210, rim=CYAN, rim_alpha=255,
                    corners=(1, 1, 1, 1), chamfer=0.006 * H)
            c.text((chip_x + 0.008 * H, yy), 'AUTOPILOT', T * H, fill=CYAN, track=0.1, shadow=False)
            chip_x += aw + 0.03 * H
        c.ficon(man, chip_x + 0.016 * H, yy, 0.032 * H, col=mode_col)
        mw = c.text((chip_x + 0.038 * H, yy), mode, T * H * 1.1, fill=mode_col, track=0.06)
        if dest:
            c.text((chip_x + 0.038 * H + mw + 0.012 * H, yy), dest, T * H * 1.1, fill=INK)
        yy += 0.04 * H
        if progress is not None:
            c.segbar(tx, yy - 0.005 * H, 0.24 * H, 0.009 * H, progress, mode_col, n=24, gap=2)
            c.text((tx + 0.255 * H, yy), eta, T * H, fill=INK)
            yy += 0.04 * H
    if place:
        c.text((tx, yy), place, T * H, fill=DIM, track=0.08)
        yy += 0.04 * H
    g = 0.032 * H
    c.glyph('VIEW', tx + g / 2, yy, g, colored=False)
    nw = c.text((tx + g * 1.25, yy), 'NEURONET', T * H, fill=CHROME, track=0.1)
    c.dot(tx + g * 1.25 + nw + 0.012 * H, yy - 0.006 * H, 0.005 * H)

def target_card(c, L, src, name, faction, klass, dist, sh, hu, att='HOSTILE', col=HOSTILE, wire=None, icon='station',
                threats=None, level=None, scan=None, bars=True):
    H, W = c.H, c.W
    w, h = 0.44 * H, 0.18 * H
    x1, y0 = W - L.mx, L.my
    x0 = x1 - w
    c.plate((x0, y0, x1, y0 + h), alpha=170, accent=('l', col), corners=(0, 1, 0, 1))
    vw = 0.13 * H
    if wire:
        c.icon_from(src, wire, x0 + 0.012 * H + vw / 2, y0 + h / 2, vw * 0.74, floor=55)
    else:
        c.ficon(icon, x0 + 0.012 * H + vw / 2, y0 + h / 2, vw * 0.5, col=col)
    tx = x0 + vw + 0.026 * H
    yy = y0 + 0.030 * H
    cw = c.textlen(att, T * H * 0.9, track=0.1) + 0.016 * H
    c.plate((tx, yy - 0.016 * H, tx + cw, yy + 0.016 * H), alpha=230, rim=col, rim_alpha=255, corners=(1, 1, 1, 1),
            chamfer=0.005 * H)
    c.text((tx + 0.008 * H, yy), att, T * H * 0.9, fill=col, track=0.1, shadow=False)
    c.text((tx + cw + 0.012 * H, yy), klass + (f'  ·  {level}' if level else ''), T * H * 0.9, fill=DIM, track=0.06)
    yy += 0.040 * H
    c.text((tx, yy), name, 0.032 * H, fill=INK)
    yy += 0.036 * H
    c.text((tx, yy), faction, 0.026 * H, fill=col)
    c.text((x1 - 0.018 * H, yy), dist, 0.032 * H, fill=INK, anchor='rm')
    yy += 0.034 * H
    bw = x1 - 0.018 * H - tx - 0.052 * H
    if bars:   # only for what can be damaged (ships, bases that can be attacked); planets and lanes show none
        c.segbar(tx, yy - 0.005 * H, bw, 0.009 * H, sh, SHIELD, n=20)
        c.text((x1 - 0.018 * H, yy), f"{int(sh * 100)}", T * H, fill=SHIELD, anchor='rm')
        yy += 0.024 * H
        c.segbar(tx, yy - 0.005 * H, bw, 0.009 * H, hu, INK, n=20)
        c.text((x1 - 0.018 * H, yy), f"{int(hu * 100)}", T * H, fill=INK, anchor='rm')
    if scan:
        yy = y0 + h + 0.03 * H
        c.text((x0 + 0.016 * H, yy), scan, T * H, fill=DIM)
    if threats:
        yy = y0 + h + 0.034 * H
        g = G * H * 0.9
        c.glyph('LB', x0 + g * 0.7, yy, g, colored=False)
        tw = c.text((x0 + g * 1.5, yy), 'TARGETS', T * H, fill=CHROME, track=0.08)
        gx = x0 + g * 1.5 + tw + 0.03 * H
        c.glyph('R3', gx + g / 2, yy, g, colored=False)
        c.text((gx + g * 1.2, yy), 'NEXT', T * H, fill=CHROME, track=0.08)
        c.text((x1 - 0.018 * H, yy), 'HOSTILES', T * H, fill=DIM, anchor='rm', track=0.08)   # the list's filter
        yy += 0.044 * H
        for icon_, nm, d, tc in threats:
            c.plate((x0, yy - 0.019 * H, x1, yy + 0.019 * H), alpha=120, rim_alpha=0, corners=(0, 0, 0, 0))
            c.ficon(icon_, x0 + 0.024 * H, yy, 0.028 * H, col=tc)
            c.text((x0 + 0.05 * H, yy), nm, 0.026 * H, fill=INK)
            c.text((x1 - 0.018 * H, yy), d, 0.026 * H, fill=INK, anchor='rm')
            yy += 0.042 * H


# ------------------------------------------------------------------ bottom-left: the ship panel
def ship_panel(c, L, src, s, wire, escorts=None, combat=True):
    """The player's ship as the game draws it (wireframe, hardpoints; damage shows on it); the escorts by their comm
    call signs with their hull in a fight, else the hold and credits. No shield ring: the reticle ring has it."""
    H = c.H
    c.op = 1.0 if combat else 0.65
    w, h = SHIP_W * H, PANEL_H * H            # 0.31 x 0.15 (5.22; the loadout's height, A5)
    x0, y1 = L.mx, H - L.my
    y0 = y1 - h
    c.plate((x0, y0, x0 + w, y1), alpha=200, corners=(1, 0, 1, 0))
    vw = 0.085 * H                            # the icon column: the icon at half the panel's height
    c.icon_from(src, wire, x0 + vw / 2, (y0 + y1) / 2, 0.075 * H, floor=55)
    tx, vx = x0 + 0.107 * H, x0 + w - 0.014 * H
    room = vx - tx
    if escorts:   # up to 3 from 0.026 at a 0.042 pitch, bars 0.018 under their names; long names in the cut form
        for i, (nm, hp) in enumerate(escorts[:3]):
            yy = y0 + (0.026 + 0.042 * i) * H
            t, sz = fit(c, nm, T * H, room)
            c.text((tx, yy), t, sz, fill=INK)
            c.segbar(tx, yy + 0.018 * H - 0.0035 * H, room, 0.007 * H, hp, FRIEND, n=10, gap=2)
    else:         # name / class 0.030, HOLD 0.068 (its bar + 0.020), CREDITS 0.120
        nw = c.text((tx, y0 + 0.030 * H), 'Patriot', T * H, fill=INK)
        t, sz = fit(c, 'LIGHT FIGHTER', T * H, room - nw - 0.011 * H)   # ClearRoom: ends 0.011 H clear of the name
        c.text((vx, y0 + 0.030 * H), t, sz, fill=DIM, anchor='rm')
        used, cap = s['hold']
        yy = y0 + 0.068 * H
        val = f"{used} / {cap}"
        c.text((tx, yy), 'HOLD', T * H, fill=DIM, track=0.08)
        c.text((vx, yy), val, T * H, fill=INK, anchor='rm')
        c.segbar(tx, yy + 0.020 * H - 0.0035 * H, room, 0.007 * H, used / cap, CHROME, n=16, gap=2)
        yy = y0 + 0.120 * H
        c.text((tx, yy), 'CREDITS', T * H, fill=DIM, track=0.08)
        c.text((vx, yy), f"{s['credits']:,}".replace(',', ' '), T * H, fill=INK, anchor='rm')
    c.op = 1.0

def loadout(c, L, s, groups, active=0, combat=True, hot=None, lock=None):
    """One panel: the D-pad cross (the shape is the label: Everspace 2 / Elden Ring) left, the weapons right - RT the
    gun groups with ">" on the active one (Y held cycles), RB the launcher with its icon, ammo and lock."""
    H, W = c.H, c.W
    c.op = 1.0 if combat else 0.6
    x1, y1 = W - L.mx, H - L.my
    w, h = BASE_W * H, PANEL_H * H            # the base: the weapons only (5.20 decision 3)
    x0, y0 = x1 - w, y1 - h
    c.plate((x0, y0, x1, y1), alpha=190, corners=(0, 1, 0, 1))
    # the consumables cross: its own plate on the base's right end, right-aligned, 0.012 H above it
    k0, k1 = x1 - CROSS_W * H, y0 - CROSS_GAP * H
    c.plate((k0, k1 - h, x1, k1), alpha=190, corners=(0, 1, 0, 1))
    d = 0.050 * H
    sz = 0.046 * H
    cx, cy = (k0 + x1) / 2, k1 - h / 2
    c.glyph('D', cx, cy, TIGHT, colored=False, alpha=220)
    for ic, n, (dx, dy) in (('wrench', s['nano'], (0, -1)), ('battery', s['batt'], (0, 1)),
                            ('flare', s['cm'], (-1, 0)), ('mine', s['mines'], (1, 0))):
        px, py = cx + dx * d * 1.15, cy + dy * d * 0.98
        hotcol = SHIELD if hot and ic == 'battery' and 'battery' in hot else HOSTILE
        is_hot = hot and ic in hot
        c.plate((px - sz / 2, py - sz / 2, px + sz / 2, py + sz / 2), alpha=220 if is_hot else 150,
                corners=(1, 1, 1, 1), chamfer=0.008 * H, rim=hotcol if is_hot else CHROME,
                rim_alpha=255 if is_hot else 110, accent=('b', hotcol) if is_hot else None)
        if is_hot:   # a lit slot glows in its meaning's colour (the alert names it, the slot answers)
            c.shape(lambda dd, P, S, px=px, py=py: dd.rectangle([P(px - sz / 2, py - sz / 2), P(px + sz / 2, py + sz / 2)],
                                                               outline=hotcol + (160,), width=int(0.004 * H * S)),
                    glow=0.006 * H, box=(px - sz / 2 - 8, py - sz / 2 - 8, px + sz / 2 + 8, py + sz / 2 + 8))
        # the Slot helper (A1): the count at the text floor at the slot's right, the icon beside it (two digits' room)
        ix = px - sz / 2 + (sz - 0.0199 * H) / 2
        if ic in ('wrench', 'battery'):
            c.icon_from(c.native, (3192, 1512, 3255, 1600) if ic == 'wrench' else (3352, 1512, 3430, 1602),
                        ix, py, sz * 0.52)
        else:   # the game's flat icons as filled silhouettes (the starburst, the spiked sphere)
            c.ficon('star' if ic == 'flare' else 'mine', ix, py, sz * 0.54, col=CHROME)
        c.text((px + sz / 2 - 0.003 * H, py), f"{n}", T * H, fill=INK, anchor='rm')
    # the weapons: tight glyphs (0.0288), the pad's rows 0.028 at a 0.030 pitch, the launcher row 0.122; a name too long
    # for its room (~0.23 H) shows its cut start form (the marquee)
    g = TIGHT * H
    wx = x0 + 0.016 * H
    tx = wx + g + 0.014 * H
    nx = tx + 0.030 * H
    xr = x1 - 0.016 * H
    c.glyph('RT', wx + g / 2, y0 + (0.028 + 0.030 * (len(groups) - 1) / 2) * H, g, colored=False)
    for i, gname in enumerate(groups):
        lit = i == active
        yy = y0 + (0.028 + 0.030 * i) * H
        c.text((tx, yy), ('>' if lit else '') + f"{i + 1}", T * H, fill=ACTIVE if lit else DIM, anchor='lm')
        t, sz_ = fit(c, gname, T * H, min(0.23 * H, xr - nx))
        c.text((nx, yy), t, sz_, fill=INK if lit else DIM)
    yy = y0 + 0.122 * H
    c.glyph('RB', wx + g / 2, yy, g, colored=False)
    c.icon('missile', tx + 0.012 * H, yy, 0.026 * H, col=INK)
    x_end = xr
    if lock:
        lw = c.textlen(lock, T * H * 0.9, track=0.12) + 0.016 * H
        c.plate((xr - lw, yy - 0.016 * H, xr, yy + 0.016 * H), alpha=230, rim=HOSTILE, rim_alpha=255,
                corners=(1, 1, 1, 1), chamfer=0.005 * H)
        c.text((xr - lw + 0.008 * H, yy), lock, T * H * 0.9, fill=HOSTILE, track=0.12, shadow=False)
        x_end = xr - lw - 0.012 * H
    cnt = f"{s['missiles']}"
    c.text((x_end, yy), cnt, T * H, fill=INK, anchor='rm')
    x_end -= c.textlen(cnt, T * H) + 0.012 * H
    t, sz_ = fit(c, 'JAVELIN MISSILE', T * H, x_end - nx)
    c.text((nx, yy), t, sz_, fill=INK)
    c.op = 1.0


# ------------------------------------------------------------------ left-middle: comms
def comms(c, L, lines):
    """The comm feed where FLvibed's subtitles already sit (left, above the ship panel), newest last, each at most two
    lines wide (0.30 W), the speaker in the attitude colour."""
    H, W = c.H, c.W
    maxw = 0.30 * W
    y = H - L.my - PANEL_H * H - 0.05 * H
    blocks = []
    for speaker, faction, text, col in lines:
        words, rows, cur = text.split(), [], ''
        for wd in words:
            t = (cur + ' ' + wd).strip()
            if c.textlen(t, 0.03 * H) > maxw and cur:
                rows.append(cur)
                cur = wd
            else:
                cur = t
        rows.append(cur)
        blocks.append((speaker, faction, rows, col))
    total = sum(0.036 * H + len(r) * 0.04 * H + 0.016 * H for _, _, r, _ in blocks)
    y -= total
    for speaker, faction, rows, col in blocks:
        x = L.mx
        c.panel((x - 0.016 * H, y - 0.012 * H, x - 0.016 * H + 3, y + 0.024 * H + len(rows) * 0.04 * H),
                chamfer=0, alpha=255, rim=col, glow=False)
        wd = c.text((x, y), speaker, T * H, fill=col)
        c.text((x + wd + 0.012 * H, y), faction, T * H, fill=DIM)
        y += 0.040 * H
        for r in rows:
            c.text((x, y), r, 0.03 * H, fill=INK)
            y += 0.04 * H
        y += 0.016 * H


def notification(c, L, title, prompts=(), col=HOSTILE):
    """One transient card (3 s), 0.05 H tall: the event in its colour and the buttons that answer it as glyphs; the
    named slots pulse meanwhile and stay lit after the card is gone."""
    H = c.H
    g = 0.032 * H
    tw = c.textlen(title, 0.028 * H, track=0.1)
    pw = sum(g * 1.25 + c.textlen(t, T * H) + 0.03 * H for _, t in prompts)
    w = 0.07 * H + tw + 0.03 * H + pw
    x0, y0 = L.cx - w / 2, L.my
    c.plate((x0, y0, x0 + w, y0 + 0.05 * H), alpha=205, accent=('l', col), corners=(1, 1, 1, 1))
    yc = y0 + 0.025 * H
    c.ficon('important', x0 + 0.03 * H, yc, 0.03 * H, col=col)
    c.text((x0 + 0.056 * H, yc), title, 0.028 * H, fill=col, track=0.1, shadow=False)
    x = x0 + 0.056 * H + tw + 0.03 * H
    for k, t in prompts:
        c.glyph(k, x + g / 2, yc, g, colored=False)
        x += g * 1.25
        x += c.text((x, yc), t, T * H, fill=INK, shadow=False) + 0.03 * H

def native_frame():
    return Image.open(sorted(glob.glob(os.path.join(hud2.E, 'h_native_combat2-*.png')))[0]).convert('RGBA')


def frame(name):
    c = hud2.HUD(hud2.base(name))
    c.im.paste(Image.open(os.path.join(hud2.E, PLATE)).convert('RGBA'), PLATE_XY)
    c.src = c.native = native_frame()
    return c, Layout(c)


OWN_WIRE = (2695, 1790, 2995, 2080)
TGT_WIRE = (940, 1790, 1220, 1960)


def save(c, name):
    os.makedirs(hud2.OUT, exist_ok=True)
    p = os.path.join(hud2.OUT, f'hud2_{name}.png')
    c.save(p, crops={'tl': (0, 0, 1500, 600), 'tr': (2340, 0, 3840, 700), 'c': (1320, 620, 2520, 1500),
                     'bl': (0, 1200, 1500, 2160), 'br': (2340, 1500, 3840, 2160)})
    return p


def render_combat():
    c, L = frame('h_cc4')
    H = c.H
    s = dict(SHIP)
    world_target(c, 1080, 832, 0.085 * H, 0.06 * H, '427 m', lead=(985, 868), in_range=True, lock=1.0)
    direction(c, L, -8, HOSTILE, '1.2 km')
    reticle(c, L, s, combat=True, missile=28, damage=200, word='BOOST')
    nav(c, L, 'Protect the armored transport', protect=0.62)
    notification(c, L, 'SHIELD FAILING', [('DD', 'BATTERY'), ('DL', 'FLARE')], col=HOSTILE)
    target_card(c, L, c.native, 'Dustin Leinenbach', 'Rogues', 'FIGHTER', '427 m', 0.30, 0.74, wire=TGT_WIRE,
                level='LEVEL 3', threats=[('enemy', 'Charles Emerson', '460 m', HOSTILE),
                                          ('enemy', 'Bart Kessler', '1.2 km', HOSTILE)])
    comms(c, L, [('Escort 1 · Charackli', 'Cryer', "I've got a target.", FRIEND)])
    ship_panel(c, L, c.native, s, OWN_WIRE, escorts=ESCORTS)
    loadout(c, L, s, GROUPS, active=0, hot=('flare', 'battery'), lock='LOCKED')
    context_lanes(c, L, left=[('B', 'REVERSE', 1.0)], right=[('X', 'TURRET', 1.0)])
    return save(c, 'combat')


def render_travel():
    c, L = frame('h_clean_travel')
    s = dict(CALM)
    direction(c, L, 214, FRIEND, '3.4 km')
    reticle(c, L, s, combat=False)
    nav(c, L, 'Dock at Fort Bush', place='NEW YORK  ·  LIBERTY SPACE')
    target_card(c, L, c.native, 'Fort Bush', 'Liberty Police', 'POLICE BASE', '3.4 km', 1.0, 1.0, att='FRIENDLY',
                col=FRIEND, icon='station', bars=False)
    comms(c, L, [('Baltimore Shipyard', 'DSE', 'Mooring link disengaged. Good luck out there.', FRIEND)])
    ship_panel(c, L, c.native, s, OWN_WIRE, combat=False)
    loadout(c, L, s, GROUPS, active=0, combat=False)
    context_lanes(c, L, left=[('A', 'DOCK', None), ('Y', 'CRUISE', None)], right=[('X', 'TRACTOR 3', None)])
    return save(c, 'travel')


def render_cruise():
    c, L = frame('h_clean_travel')
    s = dict(CALM, speed=80)
    direction(c, L, 214, FRIEND, '3.4 km')
    reticle(c, L, s, combat=False, charge=0.58)
    nav(c, L, 'Dock at Fort Bush', mode='CRUISE CHARGING', mode_col=CYAN, man='cruise')
    target_card(c, L, c.native, 'Fort Bush', 'Liberty Police', 'POLICE BASE', '3.4 km', 1.0, 1.0, att='FRIENDLY',
                col=FRIEND, icon='station', bars=False)
    ship_panel(c, L, c.native, s, OWN_WIRE, combat=False)
    loadout(c, L, s, GROUPS, active=0, combat=False)
    context_lanes(c, L, left=[('Y', 'CANCEL CRUISE', None), ('A', 'DOCK', None)])
    return save(c, 'cruise')


def render_dock():
    c, L = frame('h_clean_travel')
    s = dict(CALM, cruise='on', speed=300)
    direction(c, L, 218, FRIEND, '3.4 km')
    reticle(c, L, s, combat=False)
    nav(c, L, 'Dock at Fort Bush', mode='DOCK', mode_col=ACTIVE, man='dock', dest='FORT BUSH', ap=True,
        progress=0.62, eta='0:12')
    target_card(c, L, c.native, 'Fort Bush', 'Liberty Police', 'POLICE BASE', '3.4 km', 1.0, 1.0, att='FRIENDLY',
                col=FRIEND, icon='station', bars=False)
    comms(c, L, [('Fort Bush', 'Police', 'Roger that. Your request to dock is granted. Proceed to dock 3.', FRIEND)])
    ship_panel(c, L, c.native, s, OWN_WIRE, combat=False)
    loadout(c, L, s, GROUPS, active=0, combat=False)
    context_lanes(c, L, left=[('B', 'ABORT DOCK', None), ('VIEW', 'TAKE CONTROL', 1.0)])
    return save(c, 'dock')


def render_lane():
    c, L = frame('h_clean_travel')
    s = dict(CALM, speed=2600)
    reticle(c, L, s, combat=False)
    nav(c, L, 'Dock at Manhattan', mode='TRADE LANE', mode_col=ACTIVE, man='lane', dest='MANHATTAN', progress=0.35,
        eta='9 rings  ·  0:48')
    c.op = 0.75
    target_card(c, L, c.native, 'Manhattan', 'Liberty', 'PLANET', '38 km', 1.0, 1.0, att='FRIENDLY', col=FRIEND,
                icon='planet', bars=False)
    c.op = 1.0
    ship_panel(c, L, c.native, s, OWN_WIRE, combat=False)
    loadout(c, L, s, GROUPS, active=0, combat=False)
    context_lanes(c, L, left=[('VIEW', 'MAP', None)])
    return save(c, 'lane')


STATES = {'lane': render_lane, 'combat': render_combat, 'travel': render_travel, 'cruise': render_cruise, 'dock': render_dock}

if __name__ == '__main__':
    for st in sys.argv[1:] or list(STATES):
        print(STATES[st]())
