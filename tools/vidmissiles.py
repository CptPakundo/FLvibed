"""Offensive consumable launches in the reference footage (user, 20 Oct 2026: how the player made missiles hit -
"engage afterburners to close the distance and aim the nose to guarantee the shortest flight time and the least
tracking" - over the whole 2020 playlist). Replaces vidmissiles.ps1 (Windows OCR at 1 fps, which lost the later
missions' longer weapon lists).

The weapon panel is read by tools\\ocr\\flocr.py (scan: every row at 2 fps, snapped to the game's names); a row
'<slot> <weapon> <count>' of equipment that uses ammo (launchers, mine / countermeasure droppers: weapon_equip.ini,
misc_equip.ini) gives that slot's count over time; the count is median-smoothed over 5 samples (2.5 s: a one-frame
misread cannot make or hide a launch) and a launch is a drop of 1-3 that holds for the next 2 samples.

Output per video (research\\videos\\catalogue\\tracks):
  <id>.weapons.tsv   every row read: t, row, text, valid, raw, confidence (also the icon bar: nanobot / battery counts)
  <id>.ordnance.csv  t, weapon, smoothed count (keyed by weapon: ammo is shared per munition type)
  <id>.launches.tsv  t (s, +-0.5), weapon, count before, after
  <id>.ammo_gains.tsv rises (tractored loot, purchases): t, weapon, before, after
Usage (the OCR environment): research\\tools\\ocr\\.venv\\Scripts\\python.exe tools\\vidmissiles.py [--all | id ...]
       [--force] [--procs=4]   (the whole 2020 playlist, 12.7 h of video: ~10 min)
"""
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, 'tools', 'ocr'))
import flocr  # noqa: E402

CAT = os.path.join(ROOT, 'research', 'videos', 'catalogue')
ROW = re.compile(r'^(\d{1,2}) (.+?)(?: (\d{1,3}))?$')


def series(weapons_tsv):
    """weapon -> [(t, count)] from the valid rows of a scan. Keyed by weapon, not slot: ammo is shared per munition
    type (two launchers of one missile show one count), and slot misreads fragmented the series (M10: 8 keys for 3
    weapons, 20 Oct 2026). One sample per weapon per time (the majority if rows disagree)."""
    from collections import Counter
    counted = flocr.game_lexicon()['counted']
    per = {}
    with open(weapons_tsv, encoding='utf-8') as f:
        for line in f:
            t, row, text, valid = line.rstrip('\n').split('\t')[:4]
            if valid != '1':
                continue
            m = ROW.match(text)
            if m and m.group(3) and m.group(2) in counted:
                per.setdefault(m.group(2), {}).setdefault(float(t), []).append(int(m.group(3)))
    return {w: [(t, Counter(cs).most_common(1)[0][0]) for t, cs in sorted(d.items())] for w, d in per.items()}


def smooth(pts, k=5):
    """Median over k samples (the sample at the centre keeps its time)."""
    res = []
    for i in range(len(pts)):
        win = sorted(c for _, c in pts[max(0, i - k // 2):i + k // 2 + 1])
        res.append((pts[i][0], win[len(win) // 2]))
    return res


def launches(pts, hold=2):
    """Count changes that hold for `hold` more samples -> (drops, rises). A drop of 1-3 = a launch; a rise = ammo
    gained (tractored loot - M05's Drone Mines went 10 -> 25 one by one - or a purchase / restock at a base)."""
    drops, rises, stable = [], [], None
    for i, (t, c) in enumerate(pts):
        nxt = [cc for _, cc in pts[i + 1:i + 1 + hold]]
        if len(nxt) < hold or any(cc != c for cc in nxt):
            continue
        if stable is None:
            stable = c
        elif 1 <= stable - c <= 3:
            drops.append((t, stable, c))
            stable = c
        elif c != stable:
            if c > stable:
                rises.append((t, stable, c))
            stable = c
    return drops, rises


def run(vid, path, force=False):
    tracks = os.path.join(CAT, 'tracks')
    wt = os.path.join(tracks, f'{vid}.weapons.tsv')
    if force or not os.path.exists(wt):
        with open(wt + '.part', 'w', encoding='utf-8') as f:
            flocr.scan_video(path, 'weapons', 2.0, out=f)
        os.replace(wt + '.part', wt)
    ser = series(wt)
    n = 0
    with open(os.path.join(tracks, f'{vid}.ordnance.csv'), 'w', encoding='utf-8') as fo, \
            open(os.path.join(tracks, f'{vid}.launches.tsv'), 'w', encoding='utf-8') as fl, \
            open(os.path.join(tracks, f'{vid}.ammo_gains.tsv'), 'w', encoding='utf-8') as fg:
        fo.write('t,weapon,count\n')
        fl.write('t\tweapon\tbefore\tafter\n')
        fg.write('t\tweapon\tbefore\tafter\n')
        for w, pts in sorted(ser.items()):
            sm = smooth(pts)
            for t, c in sm:
                fo.write(f'{t:.1f},{w},{c}\n')
            drops, rises = launches(sm)
            for t, b, a in drops:
                fl.write(f'{t:.1f}\t{w}\t{b}\t{a}\n')
                n += b - a
            for t, b, a in rises:
                fg.write(f'{t:.1f}\t{w}\t{b}\t{a}\n')
    return n, len(ser)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    force = '--force' in sys.argv
    try:   # idle priority so the scan never competes with the user (ffmpeg children inherit it)
        import ctypes
        k32 = ctypes.windll.kernel32
        k32.SetPriorityClass(k32.GetCurrentProcess(), 0x40)   # IDLE_PRIORITY_CLASS
    except Exception:
        pass
    with open(os.path.join(CAT, 'videos.tsv'), encoding='utf-8') as f:
        vids = [r for r in csv.DictReader(f, delimiter='\t')
                if ('--all' in sys.argv and r['source'] == 'playlist-2020') or r['id'] in args]
    # Fastest layout (measured 20 Oct 2026, RTX 5090 + 32 cores, VP9 1080p60): a few processes, each reading several
    # videos at once with one model (flocr.scan_many); about 2 of 3 streams decoded by NVDEC (hardware crop), the rest
    # by the CPU. One process per video: ~39x real time (GPU contexts time-sliced); one process for all: ~54x (Python
    # lock); 4 processes x 6 streams: ~73x.
    procs = next((int(a.split('=', 1)[1]) for a in sys.argv if a.startswith('--procs=')), 4)
    group = next((a.split('=', 1)[1] for a in sys.argv if a.startswith('--group=')), None)
    tracks = os.path.join(CAT, 'tracks')
    if group is None and procs > 1 and len(vids) > 1:
        import subprocess
        ps = [subprocess.Popen([sys.executable, os.path.abspath(__file__), f'--group={g}/{procs}']
                               + [r['id'] for r in vids] + (['--force'] if force else [])
                               + [a for a in sys.argv if a.startswith('--panel=')])
              for g in range(min(procs, len(vids)))]
        for p in ps:
            p.wait()
        return
    if group:
        g, n = (int(x) for x in group.split('/'))
        vids = vids[g::n]
    # --panel=contacts: the contact list instead (bottom left: distance, faction - name / objects), scan only
    panel = next((a.split('=', 1)[1] for a in sys.argv if a.startswith('--panel=')), 'weapons')
    todo = [r for r in vids if force or not os.path.exists(os.path.join(tracks, f"{r['id']}.{panel}.tsv"))]
    if todo:
        jobs = [(os.path.join(ROOT, 'research', 'videos', r['path']), os.path.join(tracks, f"{r['id']}.{panel}.tsv.part"),
                 'cpu' if k % 3 == 2 else 'gpu') for k, r in enumerate(todo)]
        flocr.scan_many(jobs, panel=panel)
        for r in todo:
            p = os.path.join(tracks, f"{r['id']}.{panel}.tsv")
            os.replace(p + '.part', p)
    if panel != 'weapons':
        return
    for r in vids:
        n, k = run(r['id'], os.path.join(ROOT, 'research', 'videos', r['path']), False)
        print(f"{r['id']} {r['mission']}: {n} launches from {k} ammo slots", flush=True)


if __name__ == '__main__':
    main()
