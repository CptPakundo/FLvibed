"""hud2x_page.py - the owner's before / after page for Extended HUD 2 (docs\\UI_HUD2_EXTENDED.md 11).

    research\\tools\\ocr\\.venv\\Scripts\\python.exe -I tools\\uimock\\hud2x_page.py [frames dir]

Reads the release build's pairs `before_<screen>.png` (ui_style 0) / `final_<screen>.png` (ui_style 1) from the verify
run, writes 1600-px JPEGs and index.html into research\\evidence\\controller_ui\\hud_redesign\\hud2x_page\\ (git-ignored);
published as an Artifact with `files` = img/*.
"""
import glob
import html
import os
import sys

from PIL import Image

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SRC = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'research', 'experience', 'runs', 'hud2x_final_2026-10-25')
OUT = os.path.join(ROOT, 'research', 'evidence', 'controller_ui', 'hud_redesign', 'hud2x_page')

# (screen, title, what changed) by family; the order is the page's
FAMILIES = [
    ('Base rooms', 'Every docked screen shares the same frame: the room bar on one slim glass bar with the current room '
     'underlined, the NeuroNet box on glass, and the context strip top left.', [
        ('room_trader', 'Commodity trader room', 'The context strip names the base, the room (from its own button) and the '
         'system, with credits, cargo and ship. The pad\'s character card replaces the grey tooltip: faction over the '
         'name, an accent in the attitude colour.'),
        ('room_bar', 'Bar', 'The same frame; the bar\'s characters get the card on hover, mouse or pad.'),
        ('room_launchpad', 'Launch pad', 'The strip at 16:9 keeps its two lines left of the room bar; at 4:3 it wraps '
         'and drops the least important values first.'),
    ]),
    ('Base windows', 'The game\'s windows keep every control, text and position. Their purple art is hidden and HUD 2 '
     'glass is drawn underneath, so the game\'s own buttons still click by name.', [
        ('jobboard', 'Job board', 'One glass plate with a header rule; the focused job gets the one focus style (a 28 % '
         'yellow fill and a bar outside its left edge); the job type from the icon\'s own hint beside the reward.'),
        ('news', 'News', 'The same plate language; the ticker inside the plate.'),
        ('offer', 'Mission offer', 'Glass with an accent; the giver (faction and name) top right of the body.'),
    ]),
    ('Dealers', 'Four plates per dealer; the credits and cargo move to the strip, where a purchase the game allows '
     'shows its effect as yellow deltas.', [
        ('commodity', 'Commodity dealer', 'GOOD / FAIR / HIGH beside the game\'s coloured square (the game\'s own '
         'verdict), the strip\'s deltas match the Total line.'),
        ('equipment', 'Equipment dealer', 'A compare column in the transfer panel: the selected item against the one '
         'mounted in that slot.'),
        ('shipdealer', 'Ship dealer', 'The game\'s refusal first ("NEEDS LEVEL n", amber), then the trade-in and the '
         'net price; no delta for a purchase the game refuses.'),
        ('repair', 'Repair', 'The repair window joins the family (and is a pad screen of its own now).'),
    ]),
    ('NeuroNet windows', 'Inventory, player status, log and help on the same glass; the inventory\'s own credits and '
     'cargo bar give way to the strip.', [
        ('nn_inventory_docked', 'Inventory', ''),
        ('nn_playerinfo_docked', 'Player status', ''),
        ('nn_log_docked', 'Neural net log', ''),
        ('nn_help_docked', 'Help', ''),
    ]),
    ('Menus and dialogs', 'Modal menus get a 70 % navy scrim, one plate per panel, HUD 2 tab plates and the bottom '
     'prompt lane; in space the whole flight HUD steps away while the menu is open.', [
        ('pause_docked', 'Pause (docked)', ''),
        ('pause_space', 'Pause (in space)', 'The flight HUD hidden under the menu, back on resume.'),
        ('save_dialog', 'Save', 'The side buttons on HUD 2 plates, the name field visible.'),
        ('osk_save', 'Save with the on-screen keyboard', ''),
        ('load_ingame', 'Load (in game)', ''),
        ('confirm_docked', 'Quit confirmation', 'Focus starts on NO; B answers No (it used to read as RESUME).'),
        ('options_ingame_video', 'Options (in game)', 'The game\'s page tabs on HUD 2 tab plates.'),
        ('options_front_video', 'Options (main menu)', ''),
    ]),
    ('Left as the game draws them', 'Decided against a restyle (reasons in the design document, section 10-11).', [
        ('mainmenu', 'Main menu', 'Rebuilt by the game on every appearance with its slide-in: risk without gain.'),
        ('load_front', 'Load (main menu)', ''),
        ('newgame_prompt', 'New Game prompt', 'FLvibed\'s own prompt, unchanged.'),
        ('navmap_docked', 'Nav map (docked)', 'The map\'s frame draws outside the path the skin can trust.'),
        ('navmap_space', 'Nav map (in space)', ''),
        ('death', 'Mission failed', ''),
        ('chat_space', 'Chat with the on-screen keyboard', ''),
    ]),
]


def convert():
    os.makedirs(os.path.join(OUT, 'img'), exist_ok=True)
    have = set()
    for f in glob.glob(os.path.join(SRC, 'final_*.png')) + glob.glob(os.path.join(SRC, 'before_*.png')):
        n = os.path.basename(f).replace('.png', '.jpg')
        dst = os.path.join(OUT, 'img', n)
        if not os.path.exists(dst) or os.path.getmtime(dst) < os.path.getmtime(f):
            im = Image.open(f).convert('RGB')
            im.resize((1600, int(1600 * im.height / im.width)), Image.LANCZOS).save(dst, quality=82, optimize=True)
        have.add(n)
    return have


def page(have):
    e = html.escape
    sections = []
    nav = []
    for fam, intro, screens in FAMILIES:
        fid = fam.lower().replace(' ', '-')
        nav.append(f'<a href="#{fid}">{e(fam)}</a>')
        cards = []
        for key, title, note in screens:
            if f'final_{key}.jpg' not in have or f'before_{key}.jpg' not in have:
                continue
            cards.append(f'''<figure class="cmp" id="s-{key}">
  <div class="stage" style="--pos:50%">
    <img src="img/final_{key}.jpg" alt="{e(title)} with Extended HUD 2" loading="lazy" width="1600" height="900">
    <div class="before"><img src="img/before_{key}.jpg" alt="{e(title)} as the game draws it" loading="lazy" width="1600" height="900"></div>
    <div class="handle" aria-hidden="true"></div>
    <span class="tag tl">CLASSIC</span><span class="tag tr">HUD 2</span>
    <input type="range" min="0" max="100" value="50" aria-label="Compare {e(title)}: classic to HUD 2" id="r-{key}">
  </div>
  <figcaption><b>{e(title)}</b>{(' ' + e(note)) if note else ''}</figcaption>
</figure>''')
        if cards:
            sections.append(f'<section id="{fid}"><h2>{e(fam)}</h2><p class="intro">{e(intro)}</p>{"".join(cards)}</section>')
    return TEMPLATE.replace('{{NAV}}', ''.join(nav)).replace('{{SECTIONS}}', '\n'.join(sections))


TEMPLATE = r'''<title>Extended HUD 2</title>
<meta name="claude:viewport-fit-sides" content="cover">
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Saira+Condensed:wght@500;600;700&family=IBM+Plex+Sans:wght@400;500&display=swap">
<style>
/* Layout: one reading column of before / after comparisons, grouped by screen family, in HUD 2's own palette
   (a single dark look on purpose: the game's navy glass and light-blue chrome). */
:root {
  --bg: #0b0d1e; --glass: #141735; --rim: #3d5c86; --chrome: #82c9f3; --ink: #e2f0fc; --dim: #8fa6c0; --active: #fcf344;
  --display: "Saira Condensed", "Agency FB", "Arial Narrow", sans-serif;
  --body: "IBM Plex Sans", "Segoe UI", system-ui, sans-serif;
  color-scheme: dark;
}
html { background: var(--bg); }
body { background: var(--bg); color: var(--ink); font: 15px/1.6 var(--body); margin: 0;
  padding-left: max(16px, env(safe-area-inset-left, 0px)); padding-right: max(16px, env(safe-area-inset-right, 0px)); }
.wrap { max-width: 1180px; margin: 0 auto; padding-block: 40px 64px; display: grid; gap: 40px; }
h1, h2 { font-family: var(--display); font-weight: 600; letter-spacing: .04em; text-transform: uppercase; text-wrap: balance; margin: 0; }
h1 { font-size: clamp(34px, 6vw, 56px); line-height: 1.05; }
h1 span { color: var(--chrome); }
h2 { font-size: 26px; color: var(--chrome); border-bottom: 1px solid var(--rim); padding-bottom: 6px; }
p { margin: 0; max-width: 68ch; }
.lead { font-size: 17px; color: var(--ink); }
.dim { color: var(--dim); }
header { display: grid; gap: 16px; }
.switch { display: grid; gap: 10px; background: var(--glass); border: 1px solid var(--rim); padding: 18px 20px;
  clip-path: polygon(14px 0, 100% 0, 100% calc(100% - 14px), calc(100% - 14px) 100%, 0 100%, 0 14px); }
.switch h3 { font-family: var(--display); font-weight: 600; text-transform: uppercase; letter-spacing: .06em; margin: 0;
  color: var(--active); font-size: 18px; }
.switch ul { margin: 0; padding-left: 20px; display: grid; gap: 4px; }
code { font-family: ui-monospace, Consolas, monospace; font-size: .92em; color: var(--chrome); }
nav { display: flex; flex-wrap: wrap; gap: 8px 16px; font-family: var(--display); text-transform: uppercase; letter-spacing: .06em; }
nav a { color: var(--chrome); text-decoration: none; border-bottom: 1px solid transparent; }
nav a:hover, nav a:focus-visible { border-bottom-color: var(--active); color: var(--ink); outline: none; }
section { display: grid; gap: 18px; scroll-margin-top: 16px; }
.intro { color: var(--dim); }
.cmp { margin: 0; display: grid; gap: 8px; }
.stage { position: relative; aspect-ratio: 16 / 9; max-width: 100%; overflow: hidden; border: 1px solid var(--rim); background: #000; }
.stage img { display: block; width: 100%; height: 100%; object-fit: cover; }
.before { position: absolute; inset: 0; clip-path: inset(0 calc(100% - var(--pos)) 0 0); }
.handle { position: absolute; top: 0; bottom: 0; left: var(--pos); width: 2px; margin-left: -1px; background: var(--active);
  box-shadow: 0 0 0 1px rgba(0,0,0,.5); pointer-events: none; }
.stage input { position: absolute; inset: 0; width: 100%; height: 100%; opacity: 0; cursor: ew-resize; margin: 0; }
.stage:focus-within { outline: 2px solid var(--active); outline-offset: 2px; }
.tag { position: absolute; top: 8px; font: 600 12px/1 var(--display); letter-spacing: .12em; padding: 4px 8px;
  background: rgba(11,13,30,.8); border: 1px solid var(--rim); pointer-events: none; }
.tl { left: 8px; color: var(--dim); } .tr { right: 8px; color: var(--active); }
figcaption { color: var(--dim); }
figcaption b { color: var(--ink); font-weight: 500; margin-right: 6px; }
.facts { display: grid; grid-template-columns: repeat(auto-fit, minmax(220px, 1fr)); gap: 12px; }
.fact { border-left: 3px solid var(--chrome); padding: 4px 12px; }
.fact b { display: block; font: 600 20px/1.2 var(--display); letter-spacing: .04em; font-variant-numeric: tabular-nums; }
.fact span { color: var(--dim); font-size: 13px; }
footer { color: var(--dim); font-size: 13px; display: grid; gap: 6px; }
@media (prefers-reduced-motion: reduce) { * { scroll-behavior: auto !important; } }
</style>
<div class="wrap">
<header>
  <p class="dim">FLvibed &middot; built while you were away, 25 Oct 2026 &middot; yours to keep or discard</p>
  <h1>Extended HUD 2: <span>the rooms, dealers and menus</span></h1>
  <p class="lead">HUD 2's look carried over to the rest of the game. The game's windows keep all their controls and
  positions; their purple art is hidden and HUD 2 glass is drawn underneath. A strip names where you are and what you
  have, and the dealers show the game's own verdicts as words and numbers. Drag across each picture to compare.</p>
  <div class="switch">
    <h3>One setting, off by default</h3>
    <ul>
      <li>Options &rsaquo; FLVIBED &rsaquo; CONTROLLER &rsaquo; MENUS AND PROMPTS &rsaquo; <b>Screens</b>: CLASSIC or HUD 2.</li>
      <li>Or in the console: <code>set ui_style 1</code> (HUD 2), <code>set ui_style 0</code> (classic); <code>config save</code> keeps it.</li>
      <li>Classic is exactly today's screens. Switching back restores every byte the skin touched, at once.</li>
    </ul>
  </div>
  <div class="facts">
    <div class="fact"><b>67&ndash;84 &micro;s</b><span>per frame at 4K, release build (budget 100 &micro;s)</span></div>
    <div class="fact"><b>4 displays &times; 2 inputs</b><span>4K, 1280&times;720, 21:9, 4:3; pad and keyboard-mouse</span></div>
    <div class="fact"><b>Harness: all pass</b><span>rooms, buying, repair, jobs, save / load, launch, a mission step</span></div>
    <div class="fact"><b>3 critique rounds</b><span>two on the mock-ups, one on the built screens</span></div>
  </div>
  <nav aria-label="Screen families">{{NAV}}</nav>
</header>
{{SECTIONS}}
<footer>
  <p>How it was made: three research agents (modern and remastered menus, every screen's controls, what can be restyled
  in game), mock-ups on layered 4K captures with two independent critiques and three iterations, six build tracks on
  separate files, then a verification of the release build across the display matrix. The full record:
  <code>docs\UI_HUD2_EXTENDED.md</code>.</p>
  <p>Fixed on the way for every player: the repair window, the docked nav map and the quit confirmation are pad screens
  of their own; the room hover and the arrow come back when you switch to keyboard and mouse; the subtitles step aside
  under the menus; and the pad's menu navigation no longer costs 2.5-7 ms per frame (now under 0.1 ms on every screen).</p>
  <p>Still native or open: the main menu, the nav map's frame, the scroll bars, the confirm dialog's ornate frame, the job
  board's tooltips, the faction colours on Player Status. With the setting on CLASSIC none of it matters: the screens are
  exactly as before.</p>
</footer>
</div>
<script>
document.querySelectorAll('.stage input').forEach(function (r) {
  var stage = r.closest('.stage');
  function set() { stage.style.setProperty('--pos', r.value + '%'); }
  r.addEventListener('input', set); set();
});
</script>
'''


if __name__ == '__main__':
    have = convert()
    open(os.path.join(OUT, 'index.html'), 'w', encoding='utf-8', newline='\n').write(page(have))
    print(len(have), 'images;', os.path.join(OUT, 'index.html'))
