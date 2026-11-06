"""options.py - the FLvibed options page, simple mode (docs\\UI_OPTIONS.md): mock frames for the owner to judge.

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\options.py [recipe ...]

Reads the game's own frames in research\\evidence\\hud2_gate\\frames\\, writes research\\evidence\\options_menu\\mock\\ and its index.html.
REAL in a frame: the scene behind, the logo, the place and size of the game's Options panel and page buttons (measured: panel 0.825 H wide, 0.125-0.965 H,
page buttons 0.11 H apart, the same fractions at 4K and 1280x800). DRAWN: everything on the plate. Mock, not game: nothing here ran in game.
1280x800 is drawn at the Large size (text x 1.2, as the Steam Deck will get it), 4K at Normal.
"""
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hud2 as h2  # noqa: E402
import hud2s as hs  # noqa: E402
import hudmock as hm  # noqa: E402

GATE = os.path.join(hm.ROOT, 'research', 'evidence', 'hud2_gate')
OUT = os.path.join(hm.ROOT, 'research', 'evidence', 'options_menu', 'mock')
hs.OUT = OUT
K4, DECK = (3840, 2160), (1280, 800)
INK, CHROME, DIM, ACTIVE = h2.INK, h2.CHROME, h2.DIM, h2.ACTIVE
AMBER = hs.AMBER
FRIEND = (96, 214, 96)
NOTES = {}

# The rows, as docs\UI_OPTIONS.md 4 lists them. Kinds: head (a feature's switch, at the head of its tab or section), pick (a word, left / right), tog (ENABLED / DISABLED),
# bar (a stepped value), key, act (a button). A row: (kind, label, value, help, flags); flags: 'changed' (the dot), 'off' (it cannot change now), 'sub' (under a head on MORE).
HUD = [
    ('head', 'HUD 2', 'ENABLED', 'The new flight display and menu look. On the pad: keyboard and mouse keep the game\'s HUD. Disabled: the game\'s own HUD.', ''),
    ('pick', 'HUD size', 'AUTOMATIC', 'Automatic: Large on a handheld, Normal elsewhere. Small is close to the game\'s own HUD. This page follows it.', ''),
    ('pick', 'Window text size', 'AUTOMATIC', 'The game\'s text in dealers, job boards and the Neural Net. Automatic follows HUD size. Applies at the main menu.', ''),
    ('pick', 'HUD in calm flight', 'FADES', 'Fades: parts of the HUD fade out in calm flight and return when they matter. Always: everything stays.', ''),
    ('pick', 'Numbers on the ring', 'WHEN LOW', 'Your shield and hull as numbers beside the ring round the crosshair: when low, always, or never.', ''),
    ('pick', 'Button hints', 'WHEN USEFUL', 'Which button does what. When useful: only when something new is possible. If unsure, leave this.', ''),
    ('pick', 'Hit effects', 'FULL', 'Flashes at the screen\'s edge when your ship is hit. Choose Soft or Off if flashes bother you.', ''),
]
CONTROLLER = [
    ('head', 'Controller', 'ENABLED', 'The whole game on a controller: prompts, screens, on-screen keyboard. Takes effect at the next start.', ''),
    ('pick', 'Layout', 'FREELANCER', 'Freelancer: the game\'s controls on a pad, the right stick steers. Press A to see every button.', ''),
    ('tog', 'Invert steering', 'DISABLED', 'Stick up turns the nose down, as in a flight simulator.', ''),
    ('pick', 'Aim', 'GIMBAL', 'Gimbal: the stick steers and the reticle locks onto your target. Reticle: the stick moves the reticle, with aim assist.', ''),
    ('pick', 'Steering response', 'SOFT', 'Softer is gentler near the centre, for fine aim. If unsure, leave it on Soft.', ''),
    ('bar', 'Stick dead zone', '24 %', 'How far a stick moves before the game reacts. Raise it if the ship drifts by itself.', ''),
    ('bar', 'Rumble', '60 %', 'Vibration strength: hits on your ship and warnings. Zero switches it off.', ''),
    ('pick', 'Button prompts', 'AUTOMATIC', 'Which button pictures to show. Automatic follows the connected controller.', ''),
    ('pick', 'Weapon groups', 'CYCLE ON Y', 'One button: one fire button. Cycle: Y steps through your groups. Two triggers: a group on each trigger.', ''),
    ('pick', 'Held buttons', 'HOLD', 'Toggle: boost, reverse and rear view go on with one press and off with the next. Nothing needs holding.', ''),
]
SUBTITLES = [
    ('head', 'Subtitles', 'ENABLED', 'Spoken lines as text, in flight, in bases and in cutscenes.', ''),
    ('pick', 'Text size', 'MEDIUM', 'The size of the subtitles only. The sample above shows it.', ''),
    ('bar', 'Background', '60 %', 'How solid the panel behind the text is. More is easier to read over bright scenes.', ''),
    ('pick', 'Speaker names', 'OWN ROW', 'Who is speaking: on a row of its own, in front of the line, or not at all.', ''),
    ('pick', 'Radio chatter', 'CHAT WINDOW', 'Other pilots\' talk outside the story: in the chat window, as subtitles, or off.', ''),
]
AUTOPILOT = [
    ('head', 'Autopilot', 'ENABLED', 'The Advanced Autopilot, its goal panel and its button. Takes effect at the next start.', ''),
    ('pick', 'Autopilot style', 'COMPETENT', 'Competent: flies well, and knows only what you have seen. It uses your missiles and repairs when needed.', ''),
    ('pick', 'When it starts', 'OBJECTIVE FIRST', 'Objective first: the mission, then your waypoints, then your target. Target only: just the target you picked.', ''),
    ('bar', 'Stick takes over', '35 %', 'How far to push the stick to take the ship back. Zero: only the autopilot button does.', ''),
    ('tog', 'Skip docking scenes', 'ENABLED', 'When the autopilot docks, skip the docking scene.', ''),
    ('key', 'Autopilot key', 'K', 'The keyboard key that starts and stops the autopilot. A key the game already uses is refused.', ''),
    ('tog', 'Autopilot hints', 'ENABLED', 'Show the autopilot\'s prompts before you first use it. Disabled: nothing about it shows until you start it once.', ''),
]
MORE = [
    ('head', 'Movie handling', 'ENABLED', 'Movies fitted to the screen, the choice of intro, skipping the startup movies. Takes effect at the next start.', ''),
    ('pick', 'Startup movies', 'PLAY', 'Skip goes straight to the main menu.', 'sub'),
    ('pick', 'Intro', 'AS INSTALLED', 'Which intro plays. Only the versions this install has are offered.', 'sub'),
    ('head', 'Window handling', 'ENABLED', 'Borderless window, fit to the monitor, running in the background. Takes effect at the next start.', ''),
    ('tog', 'Keep running in background', 'DISABLED', 'The game normally pauses when you switch to another window.', 'sub'),
    ('head', 'Speedrun mode', 'ENABLED', 'The New Game choice, the timer, the splits and LiveSplit. Takes effect at the next start.', ''),
    ('head', 'Bug recorder', 'ENABLED', 'Writes a log when a ship near you stalls, for bug reports. Changes nothing in the game. Takes effect at the next start.', ''),
    ('tog', 'Show all settings', 'DISABLED', 'Every tab also lists the rest of its settings, as they were before this page. Advanced mode replaces this later.', ''),
    ('act', 'Reset', 'RESET EVERYTHING', 'Every FLvibed setting back to its default, hidden ones too. It asks first.', ''),
    ('act', 'About', 'VERSION AND CREDITS', 'FLvibed 0.4.0, its credits and licences, and where the settings files are.', ''),
]
TABS = [('HUD', HUD), ('CONTROLLER', CONTROLLER), ('SUBTITLES', SUBTITLES), ('AUTOPILOT', AUTOPILOT), ('MORE', MORE)]
# The first-start card: one screen, skippable, four rows (docs\UI_OPTIONS.md 5a).
CARD = [
    ('pick', 'HUD size', 'AUTOMATIC', 'How large the HUD and FLvibed\'s text are. This card follows it.', ''),
    ('pick', 'Subtitles', 'MEDIUM', 'Spoken lines as text, and their size. The sample above shows it.', ''),
    ('pick', 'Hit effects', 'FULL', 'Flashes at the screen\'s edge when your ship is hit. Choose Soft or Off if flashes bother you.', ''),
    ('pick', 'Held buttons', 'HOLD', 'Toggle: boost, reverse and rear view go on with one press and off with the next. Nothing needs holding.', ''),
]


def tag(size):
    return f'{size[0]}x{size[1]}'


def menu_base(size):
    return os.path.join(GATE, 'frames', 'front', f'options_general_{tag(size)}.png')


def flight_base(size):
    return os.path.join(GATE, 'frames', tag(size), 'pause__pad.png')


class Page(hs.Skin):
    """The hosted page: one plate on the game's panel, chips on the game's page buttons."""

    def __init__(self, path, deck, pad=True):
        super().__init__(path, deck)
        self.pad = pad
        self.overflow = False

    def row_h(self):
        return 0.053 if self.deck else 0.046   # Large: 42 px at 800, pitch 46 px: what ten rows cost

    def column(self):
        """The game's page buttons, as chips on their places (positions measured from the frames)."""
        left = {1.6: 0.2025, 1.778: 0.292}[round(self.WH, 3)]
        names = ['GENERAL', 'CONTROLS', 'PERFORMANCE', 'AUDIO', 'CREDITS', 'FLVIBED', 'MAIN MENU']
        self.glass((left - 0.03, 0.15, left + 0.368, 0.965 - 0.02), content=False, alpha=0.86)
        for i, n in enumerate(names):
            y = 0.19 + i * 0.11
            self.chip((left, y, left + 0.338, y + 0.0757), n, current=n == 'FLVIBED')

    def plate_page(self, title=None, box=None):
        px0 = {1.6: 0.585, 1.778: 0.674}[round(self.WH, 3)]
        box = box or (px0, 0.125, px0 + 0.825, 0.965)
        self.glass(box, content=False, alpha=1.0)   # the page is opaque (17bc818)
        self.titled = bool(title)
        if title:
            self.t((box[0] + box[2]) / 2, box[1] + 0.036 * self.ts, title, 0.040, INK, 'mm')
        return box

    def tabstrip(self, box, current, names, marks=()):
        y0 = box[1] + (0.074 * self.ts if self.titled else 0.012)
        bh = 0.052 * self.ts
        gl = 0.040 * self.ts if self.pad else 0.0
        x = box[0] + 0.016 + gl
        room = box[2] - box[0] - 0.032 - 2 * gl
        size = self.h(0.030 * self.ts)
        ws = [self.textlen(n, size) / self.H + 0.020 for n in names]
        gap = (room - sum(ws)) / max(1, len(ws) - 1)
        gap = min(gap, 0.012)
        tot = sum(ws) + gap * (len(ws) - 1)
        x += (room - tot) / 2
        if self.pad:
            self.glyph('LB', self.h(box[0] + 0.016 + gl / 2), self.h(y0 + bh / 2), self.h(0.028 * self.ts))
            self.glyph('RB', self.h(box[2] - 0.016 - gl / 2), self.h(y0 + bh / 2), self.h(0.028 * self.ts))
        for n, w in zip(names, ws):
            self.chip((x, y0, x + w, y0 + bh), n, current=n == current)
            if n in marks:   # something on this tab waits for the next start
                self.dot2(x + w - 0.012, y0 + 0.012)
            x += w + gap
        self.overflow = tot > room
        return y0 + bh + 0.008

    def control(self, kind, value, x1, y, focus, dim):
        """The value, right-aligned: every kind keeps one column."""
        h = self.row_h()
        col = ACTIVE if focus else (DIM if dim else INK)
        w = 0.300 * (1.0 + (self.ts - 1.0) * 0.6)
        x0 = x1 - w
        mid = y + h / 2
        if kind == 'head':
            pend = value.startswith(('DISABLED AT', 'ENABLED AT'))
            if value in ('ENABLED', 'DISABLED') or pend:
                kind, value2 = 'tog', value
                on = value.startswith('ENABLED')
                bw = 0.028 * self.ts
                self.t(x1 - bw - 0.014, mid, value, 0.030 if not pend else 0.028, AMBER if pend else (col if on or focus else CHROME), 'rm')
                X = self.px((x1 - bw, mid - bw / 2, x1, mid + bw / 2))
                self.plate(X, chamfer=self.h(0.004), alpha=235, rim_alpha=210, corners=(1, 0, 1, 0))
                if value == 'ENABLED':
                    self.line([(X[0] + self.h(0.2 * bw), self.h(mid)), (X[0] + self.h(0.42 * bw), self.h(mid + 0.22 * bw)), (X[2] - self.h(0.16 * bw), self.h(mid - 0.26 * bw))],
                              col=FRIEND, w=self.h(0.0045 * self.ts))
                return
            self.t(x1, mid, value, 0.030, AMBER, 'rm')
            return
        if kind in ('pick', 'key'):
            a = 0.010 * self.ts
            if kind == 'pick' and not dim:   # a row that cannot change loses its arrows: a second signal beside the grey (XAG 103)
                self.arrowhead(x0 + a, mid, -1, CHROME)
                self.arrowhead(x1 - a, mid, 1, CHROME)
            self.t((x0 + x1) / 2, mid, value, 0.030, col, 'mm')
        elif kind == 'tog':
            on = value == 'ENABLED'
            bw = 0.028 * self.ts
            self.t(x1 - bw - 0.014, mid, value, 0.030, col if on or focus or dim else CHROME, 'rm')
            if dim:
                return
            bx = (x1 - bw, mid - bw / 2, x1, mid + bw / 2)
            X = self.px(bx)
            self.plate(X, chamfer=self.h(0.004), alpha=235, rim_alpha=210, corners=(1, 0, 1, 0))
            if on:
                self.line([(X[0] + self.h(0.2 * bw), self.h(mid)), (X[0] + self.h(0.42 * bw), self.h(mid + 0.22 * bw)), (X[2] - self.h(0.16 * bw), self.h(mid - 0.26 * bw))],
                          col=FRIEND, w=self.h(0.0045 * self.ts))
        elif kind == 'bar':
            if dim:
                self.t(x1, mid, value, 0.030, col, 'rm')
                return
            frac = float(value.split()[0]) / 100
            bw = w - 0.070 * self.ts
            self.segbar(self.h(x0 + 0.004), self.h(mid - 0.008), self.h(bw), self.h(0.016), frac, ACTIVE if focus else CHROME, n=10)
            self.t(x1, mid, value, 0.030, col, 'rm')
        elif kind == 'act':
            w2 = 0.46
            self.chip((x1 - w2, y + 0.004, x1, y + h - 0.004), value)

    def arrowhead(self, x, y, d, col):
        s = 0.009 * self.ts
        pts = [(self.h(x + d * s), self.h(y)), (self.h(x - d * s * 0.6), self.h(y - s)), (self.h(x - d * s * 0.6), self.h(y + s))]

        def f(dr, P, q):
            dr.polygon([P(*p) for p in pts], fill=col + (255,))
        self.shape(f, box=(self.h(x - s * 1.2), self.h(y - s * 1.2), self.h(x + s * 1.2), self.h(y + s * 1.2)))

    def rows(self, box, y, rows, focus=0, sections=None, dimmed=False):
        x0, x1 = box[0] + 0.024, box[2] - 0.024
        h = self.row_h()
        gap = 0.004
        for i, (kind, label, value, help_, flags) in enumerate(rows):
            if sections and i in sections:
                y = self.section(box[0], box[2], y, sections[i])
            f = i == focus
            dim = dimmed or 'off' in flags
            if f:
                self.focus((x0 - 0.004, y, x1 + 0.004, y + h))
            lc = ACTIVE if f else (DIM if dim else INK)
            tx = x0 + 0.010 + (0.026 if 'sub' in flags else 0)
            if 'changed' in flags:
                self.dot(x0 + 0.012, y + h / 2)
                tx += 0.022
            self.t(tx, y + h / 2, label, 0.032, lc)
            self.control(kind, value, x1 - 0.008, y, f, dim and kind != 'head')
            y += h + gap
            if kind == 'head' and i + 1 < len(rows) and rows[i + 1][0] != 'head' and 'sub' not in rows[i + 1][4]:
                self.line([(self.h(x0), self.h(y - gap / 2)), (self.h(x1), self.h(y - gap / 2))], col=CHROME, w=max(1, self.h(0.0008)), alpha=110)
        return y

    def dot2(self, x, y):
        r = 0.006 * self.ts

        def f(dr, P, q):
            dr.ellipse([P(self.h(x - r), self.h(y - r)), P(self.h(x + r), self.h(y + r))], fill=AMBER + (255,))
        self.shape(f, box=(self.h(x - r * 2), self.h(y - r * 2), self.h(x + r * 2), self.h(y + r * 2)))

    def dot(self, x, y):
        r = 0.006 * self.ts

        def f(dr, P, q):
            dr.ellipse([P(self.h(x - r), self.h(y - r)), P(self.h(x + r), self.h(y + r))], fill=ACTIVE + (255,))
        self.shape(f, box=(self.h(x - r * 2), self.h(y - r * 2), self.h(x + r * 2), self.h(y + r * 2)))

    def helpband(self, box, text, foot_top, lines=2):
        """The description: inside the plate, under the rows, the same place on every tab."""
        hh = 0.030 * self.ts * 1.28 * lines + 0.016
        y0 = foot_top - hh
        self.line([(self.h(box[0] + 0.018), self.h(y0)), (self.h(box[2] - 0.018), self.h(y0))], col=CHROME, w=max(2, self.h(0.0012)), alpha=130)
        end = self.wrap(box[0] + 0.026, y0 + 0.008 + 0.030 * self.ts * 0.64, text, 0.030, box[2] - box[0] - 0.052, CHROME)
        if end - (y0 + 0.008 + 0.030 * self.ts * 0.64) > 0.030 * self.ts * 1.28 * lines + 0.001:
            print('  HELP TOO LONG:', text)
        return y0

    def foot(self, box, items):
        """The bottom chips; on the pad each carries its button, so they need no second lane of prompts. K&M: its key."""
        bh = 0.048 * self.ts
        y0 = box[3] - bh - 0.012
        n = len(items)
        g = 0.014
        w = (box[2] - box[0] - 0.040 - g * (n - 1)) / n
        x = box[0] + 0.020
        for gl, key, word in items:
            self.chip((x, y0, x + w, y0 + bh), '      ' + word)
            size = self.textlen(word, self.h(0.030 * self.ts)) / self.H
            cx = x + w / 2 - size / 2 - 0.016 * self.ts
            if self.pad:
                self.glyph(gl, self.h(cx), self.h(y0 + bh / 2), self.h(0.032 * self.ts))
            else:
                kw = max(0.030, self.textlen(key, self.h(0.024 * self.ts)) / self.H + 0.014) * self.ts
                X = self.px((cx - kw / 2, y0 + bh / 2 - 0.016 * self.ts, cx + kw / 2, y0 + bh / 2 + 0.016 * self.ts))
                self.plate(X, chamfer=self.h(0.004), alpha=235, rim_alpha=200, corners=(1, 1, 1, 1))
                self.t(cx, y0 + bh / 2, key, 0.024, CHROME, 'mm')
            x += w + g
        return y0 - 0.006


FOOT = [('X', 'D', 'DEFAULT'), ('Y', 'C', 'CANCEL'), ('B', 'ESC', 'CLOSE')]


def page(size, name, tabname, rows, focus=0, note=None, pad=True, sections=None, preview=False, base=None, box=None, scrim=False, marks=(), title=None):
    deck = size == DECK
    s = Page(base or menu_base(size), deck, pad)
    if scrim:
        s.scrim(0.70)
    else:
        s.column()
    box = s.plate_page(title=title, box=box)
    y = s.tabstrip(box, tabname, [t[0] for t in TABS], marks)
    foot_top = s.foot(box, FOOT)
    help_top = s.helpband(box, rows[focus][3], foot_top)
    off = rows[0][0] == 'head' and rows[0][2].startswith('DISABLED') and tabname != 'MORE'
    head = rows[:1] if preview else []
    if preview:
        y = s.rows(box, y, head, focus if focus == 0 else -1)
        y = preview_band(s, box, y, off)
        rest = rows[1:]
        end = s.rows(box, y, rest, focus - 1 if focus else -1, sections, dimmed=off)
    else:
        end = s.rows(box, y, rows, focus, sections, dimmed=off and False)
    NOTES[name] = note or 'REAL: the logo, the scene, where the game\'s panel and page buttons stand. DRAWN: the rest.'
    print(name, 'rows end', round(end, 3), 'help top', round(help_top, 3), 'FIT' if end <= help_top + 0.002 else 'OVER', 'tabs overflow' if s.overflow else '')
    s.finish(name)


def preview_band(s, box, y, off=False, name=True):
    """The live sample: the subtitles as they will look (XAG 104; the owner's decision 7 of 25 Oct)."""
    bh = 0.092 * s.ts
    x0, x1 = box[0] + 0.024, box[2] - 0.024
    s.plate(s.px((x0, y, x1, y + bh)), chamfer=s.h(0.006), alpha=255, rim_alpha=90, corners=(1, 0, 1, 0))
    if not off:
        s.t((x0 + x1) / 2, y + 0.024 * s.ts, 'Juni', 0.030, FRIEND, 'mm')
        s.t((x0 + x1) / 2, y + 0.062 * s.ts, 'Trent, this is Juni. Meet me at the bar.', 0.030, (255, 255, 255), 'mm')
    else:
        s.t((x0 + x1) / 2, y + bh / 2, 'No subtitles', 0.030, DIM, 'mm')
    return y + bh + 0.008


def dialog(size, name, title, body, act, safe, note, tabname='MORE', rows=None, focus=8):
    deck = size == DECK
    s = Page(menu_base(size), deck, True)
    s.column()
    b0 = s.plate_page()
    y = s.tabstrip(b0, tabname, [t[0] for t in TABS])
    s.rows(b0, y, rows or MORE, focus)
    s.foot(b0, FOOT)
    s.scrim(0.70)
    w, hh = 1.02, 0.340 if deck else 0.300
    x0, y0 = (s.WH - w) / 2, 0.5 - hh / 2
    s.glass((x0, y0, x0 + w, y0 + hh), content=False, accent=('t', AMBER), alpha=0.92)
    s.t(s.WH / 2, y0 + 0.052 * s.ts, title, 0.046, INK, 'mm')
    s.t(s.WH / 2, y0 + 0.108 * s.ts, body, 0.030, CHROME, 'mm')
    bw, bh = 0.42, 0.062 * s.ts
    by = y0 + hh - bh - 0.032
    s.chip((x0 + 0.050, by, x0 + 0.050 + bw, by + bh), act)
    s.chip((x0 + w - 0.050 - bw, by, x0 + w - 0.050, by + bh), safe, focus=True)
    NOTES[name] = note
    s.finish(name)


LAYOUT = [('RT', 'Fire'), ('LT', 'Afterburner, held'), ('RB', 'Missile; hold: torpedo'), ('LB', 'Target list; hold: action wheel'), ('A', 'Dock, jump, go to; hold: formation'),
          ('B', 'Cancel, drop cruise; hold: reverse'), ('X', 'Tractor; hold: turret view'), ('Y', 'Next weapon group; hold: cruise'), ('RS', 'Steer; click: target ahead, next'),
          ('LS', 'Throttle and strafe; click: engine kill'), ('D', 'Nanobot, battery, countermeasure, mine'), ('VIEW', 'Neural Net; hold: autopilot')]


def layout_card(size, name):
    """What A on the Layout row opens: every button of the chosen layout, read-only (UI_PLAN.md 6.4 for the words)."""
    deck = size == DECK
    s = Page(menu_base(size), deck, True)
    s.column()
    s.plate_page()
    s.scrim(0.70)
    w, top, bot = 1.36, 0.035, 0.975
    x0 = (s.WH - w) / 2
    b = (x0, top, x0 + w, bot)
    s.glass(b, content=False, alpha=1.0)
    s.t((b[0] + b[2]) / 2, b[1] + 0.034 * s.ts, 'LAYOUT: FREELANCER', 0.040, INK, 'mm')
    s.line([(s.h(b[0] + 0.018), s.h(b[1] + 0.072 * s.ts)), (s.h(b[2] - 0.018), s.h(b[1] + 0.072 * s.ts))], col=CHROME, w=max(2, s.h(0.0012)), alpha=130)
    foot_top = s.foot(b, [('LB', 'Q', 'PREVIOUS LAYOUT'), ('RB', 'E', 'NEXT LAYOUT'), ('B', 'ESC', 'CLOSE')])
    y = b[1] + 0.086 * s.ts
    pitch = (foot_top - y) / 12
    for i, (g, what) in enumerate(LAYOUT):
        yy = y + (i + 0.5) * pitch
        s.glyph(g, s.h(x0 + 0.060), s.h(yy), s.h(0.034 * s.ts))
        s.t(x0 + 0.110, yy, what, 0.032, INK)
    NOTES[name] = 'REAL: the page behind the scrim. DRAWN: the card. Read-only: the Bindings editor stays unbuilt, so this replaces today\'s row that opens nothing.'
    s.finish(name)


def first_card(size, name, pad=True):
    """The first-start card: over the main menu, once, on a fresh install (UI_OPTIONS.md 5a)."""
    deck = size == DECK
    s = Page(os.path.join(GATE, 'mock', f'mainmenu_{tag(size)}_pad.png'), deck, pad)
    s.scrim(0.70)
    w = 1.10
    x0 = (s.WH - w) / 2
    top, bot = 0.12, 0.83
    b = (x0, top, x0 + w, bot)
    s.glass(b, content=False, alpha=1.0)
    s.t((b[0] + b[2]) / 2, b[1] + 0.036 * s.ts, 'BEFORE YOU FLY', 0.040, INK, 'mm')
    s.t((b[0] + b[2]) / 2, b[1] + 0.076 * s.ts, 'Nothing here is required. It is all in Options, FLVIBED, later too.', 0.030, CHROME, 'mm')
    y = b[1] + 0.104 * s.ts
    s.line([(s.h(b[0] + 0.018), s.h(y)), (s.h(b[2] - 0.018), s.h(y))], col=CHROME, w=max(2, s.h(0.0012)), alpha=130)
    foot_top = s.foot(b, [('X', 'D', 'DEFAULT'), ('B', 'ESC', 'CLOSE')])
    help_top = s.helpband(b, CARD[0][3], foot_top)
    y = preview_band(s, b, y + 0.010)
    end = s.rows(b, y, CARD, 0)
    NOTES[name] = 'REAL: the main menu behind the scrim (itself a mock of 0.4.0). DRAWN: the card.'
    print(name, 'rows end', round(end, 3), 'help top', round(help_top, 3), 'FIT' if end <= help_top + 0.002 else 'OVER')
    s.finish(name)


RECIPES = {}


def recipe(fn):
    RECIPES[fn.__name__] = fn
    return fn


@recipe
def tabs():
    for size in (DECK, K4):
        t = tag(size)
        page(size, f'1_hud_{t}_pad', 'HUD', HUD, 1)
        page(size, f'2_controller_{t}_pad', 'CONTROLLER', CONTROLLER, 1)
        page(size, f'3_subtitles_{t}_pad', 'SUBTITLES', SUBTITLES, 1, preview=True)
        page(size, f'4_autopilot_{t}_pad', 'AUTOPILOT', AUTOPILOT, 1)
        page(size, f'5_more_{t}_pad', 'MORE', MORE, 0)


@recipe
def kbm():
    page(K4, '6_hud_3840x2160_kbm', 'HUD', HUD, 3, pad=False, note='Keyboard and mouse: no bumper glyphs, keycaps on the chips, hover moves the focus.')
    page(DECK, '6_controller_1280x800_kbm', 'CONTROLLER', CONTROLLER, 7, pad=False, note='Keyboard and mouse at 1280x800: the tab stays, the rows work. Nothing is hidden by the device.')


@recipe
def flight():
    for size in (DECK, K4):
        w = 1.04
        x0 = (size[0] / size[1] - w) / 2
        page(size, f'7_flight_{tag(size)}_pad', 'CONTROLLER', CONTROLLER, 0, base=flight_base(size), box=(x0, 0.028, x0 + w, 0.972), scrim=True, title='FLVIBED OPTIONS',
             note='The same page from the pause menu\'s FLVIBED button, over the scene: here the title stays, since no column says where you are. REAL: the scene behind the scrim. DRAWN: the plate.')


def dimmed(rows, head, value='DISABLED'):
    out = [('head', rows[0][1], value, head, 'changed')]
    for r in rows[1:]:
        out.append((r[0], r[1], r[2], r[3], r[4] + ' off'))
    return out


@recipe
def states():
    changed = [HUD[0], ('pick', HUD[1][1], 'LARGE', HUD[1][3], 'changed')] + HUD[2:]
    stay = changed + [('pick', 'Opacity', '80 %', 'A setting you changed that this page does not normally show. It stays here until it is back at its default.', 'changed')]
    stay[7] = ('bar', 'Opacity', '80 %', stay[7][3], 'changed')
    for size in (DECK, K4):
        t = tag(size)
        page(size, f'8_changed_{t}_pad', 'HUD', stay, 7, sections={7: 'YOU CHANGED'}, note='The dot marks a changed row. A hidden row the player changed comes back under YOU CHANGED, so nothing is lost out of sight.')
        page(size, f'9_subtitles_off_{t}_pad', 'SUBTITLES', dimmed(SUBTITLES, 'No subtitles from now on. Your settings are kept.'), 0, preview=True,
             note='The switch at the head, off at once, no restart: the one row a player sees is the live switch and the module together. The rows dim and lose their arrows, and stay focusable.')
        page(size, f'10_autopilot_pending_{t}_pad', 'AUTOPILOT', dimmed(AUTOPILOT, 'It keeps working until you restart. Your settings are kept.', 'DISABLED AT NEXT START'), 0, marks=('AUTOPILOT',),
             note='A feature with no live switch: the row says when, in amber, and the tab carries a mark until the game restarts.')
        page(size, f'11_more_{t}_pad', 'MORE', [MORE[0], MORE[1], MORE[2], ('head', 'Window handling', 'ENABLED AT NEXT START', 'It was off when the game started, so it comes on at the next start. Its row unlocks then.', 'changed'),
                                                (MORE[4][0], MORE[4][1], MORE[4][2], MORE[4][3], 'sub off')] + MORE[5:], 3, marks=('MORE',),
             note='Features without a tab: each is a head on MORE, its rows under it. Turning on a feature that was off at the start is the one case every switch waits for a restart.')
        dialog(size, f'12_controller_off_{t}_pad', 'DISABLE THE CONTROLLER?', 'From the next start the game is keyboard and mouse only.', 'DISABLE IT', 'KEEP IT ON',
               'The one switch that asks: it can lock a pad-only player out of this page. REAL: the logo behind the scrim. DRAWN: the rest.', 'CONTROLLER', CONTROLLER, 0)
        dialog(size, f'13_reset_{t}_pad', 'RESET EVERYTHING?', 'Every FLvibed setting goes back to its default, hidden ones too.', 'RESET EVERYTHING', 'KEEP MY SETTINGS',
               'A dialog uses verbs (UI_HUD2_SKIN.md 6.2); the page around it uses the game\'s DEFAULT, CANCEL, CLOSE. A plain press confirms (XAG 115).')
        layout_card(size, f'14_layout_{t}_pad')
        first_card(size, f'15_first_start_{t}_pad')


@recipe
def helpcheck():
    """Every description must fit two lines at Large."""
    s = Page(menu_base(DECK), True)
    room = 0.825 - 0.052
    for name, rows in TABS + [('CARD', CARD)]:
        for r in rows:
            n, line = 1, ''
            for w in r[3].split(' '):
                c = (line + ' ' + w).strip()
                if s.textlen(c, s.h(0.030 * s.ts)) > s.h(room) and line:
                    n, line = n + 1, w
                else:
                    line = c
            if n > 2:
                print('  OVER TWO LINES:', name, r[1], n)


QUESTIONS = """<h2>1. Before you look at the page</h2>
<p>These five tabs are all you get to see for now. <b>Which tab would you open to...</b></p>
<img src="tabs.jpg" alt="The five tabs: HUD, CONTROLLER, SUBTITLES, AUTOPILOT, MORE">
<ol type="a"><li>make the text in the dealers larger?</li><li>stop the screen flashing when your ship is hit?</li><li>change how strong the vibration is?</li>
<li>get rid of the autopilot altogether?</li><li>skip the intro movies?</li><li>go back to the game's original HUD?</li></ol>
<h2>2. The page</h2>
<p>Look through the frames below. Each shows one tab, with one row highlighted and its description at the bottom. The arrows mean you can change a value.</p>
<ol start="1"><li>Which rows would you not know the meaning of without reading the description?</li><li>Which description left you unsure, even after reading it?</li>
<li>What would you look for in a page like this, and not find? Your game's own volume, resolution and keyboard keys are on the game's own pages, beside this one.</li>
<li>Is anything too small to read comfortably? If you play on a handheld, please look at the frames full size on it.</li></ol>
<h2>3. What happens next?</h2>
<ol start="5"><li><b>Frame "Subtitles off".</b> What has happened, and how would you undo it?</li><li><b>Frame "Autopilot off".</b> Is the autopilot still working right now? What would you do next?</li>
<li><b>The bottom buttons.</b> What do you expect DEFAULT, CANCEL and CLOSE to do? Would any of them lose your changes?</li><li><b>Frame "First start".</b> This appears once, the first time you start the game. Would you change something, or close it? Is any row out of place?</li></ol>
<h2>4. About you</h2>
<ol start="9"><li>Controller, or keyboard and mouse? On a monitor, a TV or a handheld?</li><li>Do you usually go through a game's settings before you play, or only when something bothers you?</li></ol>"""
FRAMES = [('1_hud_1280x800_pad', 'HUD'), ('2_controller_1280x800_pad', 'Controller'), ('3_subtitles_1280x800_pad', 'Subtitles'), ('4_autopilot_1280x800_pad', 'Autopilot'), ('5_more_1280x800_pad', 'More'),
          ('9_subtitles_off_1280x800_pad', 'Subtitles off'), ('10_autopilot_pending_1280x800_pad', 'Autopilot off'), ('15_first_start_1280x800_pad', 'First start'),
          ('6_hud_3840x2160_kbm', 'HUD on a 4K screen, keyboard and mouse')]


@recipe
def pack():
    """The check for two or three real players: frames and questions, nothing that needs the game (UI_OPTIONS.md 12a)."""
    import zipfile
    dst = os.path.join(os.path.dirname(OUT), 'player_check')
    os.makedirs(dst, exist_ok=True)
    im = Image.open(os.path.join(OUT, '1_hud_1280x800_pad.png')).convert('RGB')
    im.crop((468, 100, 1128, 170)).resize((990, 105), Image.LANCZOS).save(os.path.join(dst, 'tabs.jpg'), quality=90)
    html = ['<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>FLvibed: a new options page</title>',
            '<style>body{background:#0d1020;color:#e2f0fc;font:17px/1.5 Segoe UI,Arial,sans-serif;margin:0 auto;padding:16px;max-width:1000px}img{max-width:100%;display:block;margin:8px 0 20px}h1,h2{color:#82c9f3}li{margin:6px 0}</style>',
            '<h1>FLvibed: a new options page</h1>',
            '<p>FLvibed is a mod for Freelancer. Its options are being rebuilt so that a new player finds what matters quickly. These pictures are mock-ups: nothing here needs the game, and it takes about ten minutes.</p>',
            '<p><b>The page is on trial, not you.</b> If something confuses you, that is the useful answer. Please answer in order, and do part 1 before scrolling down. Short answers are fine.</p>',
            QUESTIONS, '<h2>The frames</h2>']
    names = ['tabs.jpg']
    for f, words in FRAMES:
        n = f + '.jpg'
        fr = Image.open(os.path.join(OUT, f + '.png')).convert('RGB')
        if fr.size[0] > 1600:
            fr = fr.resize((1600, 900), Image.LANCZOS)
        fr.save(os.path.join(dst, n), quality=88)
        names.append(n)
        html.append(f'<h3>{words}</h3><img src="{n}" alt="{words}" loading="lazy">')
    open(os.path.join(dst, 'index.html'), 'w', encoding='utf8').write('\n'.join(html))
    with zipfile.ZipFile(os.path.join(os.path.dirname(OUT), 'flvibed_options_check.zip'), 'w', zipfile.ZIP_DEFLATED) as z:
        for n in ['index.html'] + names:
            z.write(os.path.join(dst, n), n)
    print('pack', dst)


def index():
    names = sorted((n for n in os.listdir(OUT) if n.endswith('.png')), key=lambda n: (int(n.split('_')[0]), n))
    out = ['<!doctype html><meta charset="utf-8"><title>FLvibed options: simple mode mocks</title>',
           '<style>body{background:#0d1020;color:#dfe;font:16px Segoe UI;margin:24px}img{max-width:100%;margin:6px 0 24px}</style>',
           '<h1>Simple mode, mock frames</h1><p>Design: docs\\UI_OPTIONS.md. Nothing here ran in game. 1280x800 is drawn at the Large size.</p>']
    for n in names:
        out.append(f'<h3>{n}</h3><p>{NOTES.get(n[:-4], "")}</p><img src="{n}" loading="lazy">')
    open(os.path.join(OUT, 'index.html'), 'w', encoding='utf8').write('\n'.join(out))


if __name__ == '__main__':
    want = sys.argv[1:] or list(RECIPES)
    os.makedirs(OUT, exist_ok=True)
    for w in want:
        RECIPES[w]()
    index()
    print('wrote', OUT)
