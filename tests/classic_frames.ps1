# The classic HUD's frames against a pinned set (hud_style 0: the game's own HUD and screens, with the Controller module's pad layer
# - modules\controller\src\impl\padhud.cpp, promptbar, padbases - and FLvibed's options page hosted in the game's Options screen).
# The net of the HUD 2 rebuild's restructure packages: a change that alters what the classic HUD draws shows as changed cells.
#   flight  SaveC706sky (SaveC706 25 km above its spot: a still, empty sky, Get-SkySave): flight_pad (badges, legend;
#           input_mode 1), flight_kbm (input_mode 2: the game's HUD alone) - both NO-MASK frames: every cell compared;
#           options_hud / options_controller (pause menu -> the game's Options -> FLvibed's page hosted there, tabs 0 and 1)
#   docked  SaveC701 at Manhattan: room_bar (the pad's hover on the bartender: his card, the prompt bar), the game's equipment dealer window -
#           dealer_open, dealer_scrolled (D-pad down past the visible rows), dealer_player (the player's side); B closes it, checked
#           by the verbs only (the equipment room's own 3D scene differs between fresh starts in a quarter of the picture)
#   Left out of the comparison, by the game's own control rectangles (uiitems, written to <frame>.exclude in every pass; only
#   the pin passes' go into the mask, so a control that moved since is compared where it is now): the Options screen's
#   page buttons (their shading band sits elsewhere in each fresh start) and, in the options frames, everything outside the
#   Options screen (the paused flight HUD behind it, its glow frozen at a random phase). The cells whose mean moved inside a
#   frame's own window by more than a limit (<frame>.moving: room_bar 6, the bar's characters; the flight frames 20, named
#   only, while the HUD panels' glow moves a cell by up to ~15) go into the mask only at the pin and never for a no-mask
#   frame; a run compares every cell that was stable at the pin, its own moving cells too (12 Nov 2026: masking a run's own
#   motion left 289 cells out of a 1280x800 flight frame, the weapon rows and the ship-status panel among them). A frame
#   whose changed cells all moved inside its own window is INCONCLUSIVE (exit 3), not masked. The flight frames are no-mask
#   frames (12 Nov 2026, F1b: near Fort Bush the traffic behind the partly transparent panels had masked weapon rows, the
#   ship-status panel's edge, pad badges, the AUTOPILOT legend row and the maneuver icons): the pin refuses one whose passes
#   differ in any cell, and a run FAILs one whose pinned mask is not empty
# Each frame is taken only once the state is reached and confirmed by the mod's verbs (state, device, padhud, hud2 state, ui state,
# bases, hotspots), and the same verbs must still hold after the capture. The game keeps moving behind the HUD (the panels' glow
# pulses about every 4 s, the active maneuver icon spins, the 3D views turn, traffic, bar characters), so a frame is a COMPOSITE:
# -Captures (24; the flight frames -FlightCaptures, 48, at the same percentile) spread over a measurement window (-WindowSec,
# 6 s), per pixel the -Rank-th (6th) smallest value - a low
# percentile: the additive glow (bright under ~75 % of the time) is waited out, and so is a dark object passing (under ~25 %); a
# window whose captures' grey level ranges over 2 (something big passed) is taken again. Random text is kept off the frames: the
# NPCs' radio chatter off (subtitles `chatter 2`),
# a window starts once the picture has settled (8 captures in a row within 1 grey level: a load's fade-in, a room's entry) and
# 12 s after the last subtitle / chat line (the chat window's own fade) and is taken again when a line shows during
# it; in the rooms `prompt_bar 1` (Always: the default fades the room's prompts 4 s after the last action). What still differs
# between fresh games is MASKED: -Pin runs the whole set -PinPasses times (3) from fresh starts and masks the cells where a later
# pass disagrees with the first (dilated by -Dilate cells); the first pass is the reference. Comparison = tests\pixels_compare.py's:
# the frame cut into 64 x 36 cells, a cell counts when its mean absolute difference (grey) passes -Limit (6); a frame PASSes when
# no more than -Allow (0) unmasked cells count. A frame whose mask covers over 40 % of it fails the pin.
# Frames, diff maps and the verbs' replies: research\experience\runs\classic_frames\<time>\ (git-ignored); the pinned set:
# research\experience\runs\classic_frames\pinned\ (<size>_<frame>.png, masks.json, pinned.txt). Never re-pinned by itself.
# Usage (PowerShell tool; the release build, never builds):
#   tests\classic_frames.ps1 -Instance <n> [-Sizes '3840, 2160','1280, 800']        compare with the pinned set; exit 1 on a FAIL,
#                                                                                    3 INCONCLUSIVE (only moving cells changed), 2 error
#   tests\classic_frames.ps1 -Pin -Instance <n>                                      (re)pin: -PinPasses fresh passes -> pinned\
#   tests\classic_frames.ps1 -Pin -PinFrom '<run>_pin1','<run>_pin2','<run>_pin3'     pin from the passes of an earlier -Pin (no game,
#                                                                                    no instance touched, -Instance not needed)
#   ... -Pin [-PinFrom ...] -PinFrames '<regex>'   re-pin only the frames whose file name matches (e.g. 'flight_'): every
#                                                  other frame keeps the current pinned set's image and mask
# Run time on this machine: a comparison ~4 min (248-261 s, two sizes, four game starts), a pin ~12 min (666-745 s), 12 Nov 2026.
param([string]$Instance = '', [string[]]$Sizes = @('3840, 2160', '1280, 800'), [switch]$Pin, [double]$Limit = 6, [int]$Allow = 0,
      [int]$Dilate = 1, [double]$WindowSec = 6, [int]$Captures = 24, [int]$Rank = 6, [int]$PinPasses = 3,
      [string[]]$PinFrom = @(), [string]$PinFrames = '', [int]$FlightCaptures = 48)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
# A lane (tools\lane.ps1: a linked git worktree) reaches the pinned set through its research\experience\runs junction: the
# set is main's, shared by every lane, and is never re-pinned from a lane (docs\TESTING.md "Parallel lanes")
if ($Pin) {
    $gd = "$(git -C $root rev-parse --path-format=absolute --git-dir 2>$null)"; $cd = "$(git -C $root rev-parse --path-format=absolute --git-common-dir 2>$null)"
    if ($gd -and $cd -and [IO.Path]::GetFullPath($gd) -ine [IO.Path]::GetFullPath($cd)) { throw "-Pin from a lane ($root): the pinned set is shared; re-pin from the main checkout only" }
}
# A pin from earlier passes (-Pin -PinFrom) starts no game, so it touches no instance: no game stopped, no display mode or size
# written, no sampler stopped (13 Nov 2026: its clean-up wrote the default instance 6's exe while another checkout's game ran
# there, and threw in Set-FLInstanceDisplay). Everything else plays on the named instance: no default (docs\TESTING.md
# "Parallel lanes": a default instance belongs to no checkout).
$noGame = [bool]($Pin -and $PinFrom.Count)
if (-not $noGame -and -not $Instance) { throw 'name the game instance: -Instance <n> (no default: a default instance belongs to no checkout - docs\TESTING.md "Parallel lanes")' }
if ($Instance) { $env:FLVIBED_INSTANCE = "$Instance" }
. (Join-Path $root 'tools\harness\flctl.ps1')
. (Join-Path $root 'tools\harness\flsub.ps1')
. (Join-Path $root 'tools\fldata.ps1')
$python = Join-Path $root 'research\tools\ocr\.venv\Scripts\python.exe'
$base = Join-Path $root 'research\experience\runs\classic_frames'
$pinned = Join-Path $base 'pinned'
$deploy = Join-Path $root 'tools\deploy.ps1'
$started = Get-Date
$stamp = '{0:yyyy-MM-dd_HHmmss}' -f $started
$buildInfo = (Get-Content (Join-Path $root 'build\release\build_info.txt') -ErrorAction SilentlyContinue | Select-Object -First 1)
if (-not $Pin -and -not (Test-Path -LiteralPath (Join-Path $pinned 'masks.json'))) { throw "no pinned set in ${pinned}: run with -Pin first" }

# ---- the composite: per-pixel minimum over the window's captures (PrintWindow of the game window as Save-FLShot takes it)
if (-not ('FLRankShot' -as [type])) {
    Add-Type -AssemblyName System.Drawing
    $null = [System.Drawing.Bitmap]   # loads System.Drawing.Common and what it rests on (the GDI+ interop assembly in newer runtimes)
    $refs = @([AppDomain]::CurrentDomain.GetAssemblies() | Where-Object { -not $_.IsDynamic -and $_.Location -and $_.GetName().Name -match '^System\.(Drawing|Private\.Windows|Runtime\.InteropServices$|Runtime$|Primitives)' } | ForEach-Object Location | Sort-Object -Unique)
    Add-Type -ReferencedAssemblies $refs -TypeDefinition @'
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public static class FLRankShot {
  [DllImport("user32.dll")] static extern bool SetProcessDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] static extern IntPtr SetThreadDpiAwarenessContext(IntPtr v);
  [DllImport("user32.dll")] static extern IntPtr GetWindowDpiAwarenessContext(IntPtr h);
  [DllImport("user32.dll")] static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  struct RECT { public int L, T, R, B; }
  // per byte the k smallest values seen so far, ascending (s[k-1] = the k-th smallest: the composite)
  static byte[][] s; static byte[] tmp; static int w, h, k = 1;
  public static int Count, Width, Height;
  public static void Reset(int rank) { s = null; Count = 0; k = Math.Max(1, rank); cmin = null; cmax = null; }
  // per cell of the comparison's grid (64 x 36) the lowest and highest mean grey level over the window's captures
  const int GX = 64, GY = 36;
  static double[] cmin, cmax;
  public static string Moving(double limit) {
    var sb = new System.Text.StringBuilder();
    if (cmin != null) for (int c = 0; c < GX * GY; c++) if (cmax[c] - cmin[c] > limit) sb.Append(c).Append(' ');
    return sb.ToString().Trim();
  }
  static void Cells(byte[] px, int stride, int bw, int bh) {
    if (cmin == null) { cmin = new double[GX * GY]; cmax = new double[GX * GY]; for (int c = 0; c < GX * GY; c++) { cmin[c] = 1e9; cmax[c] = -1; } }
    for (int cy = 0; cy < GY; cy++) for (int cx = 0; cx < GX; cx++) {
      int x0 = cx * bw / GX, x1 = (cx + 1) * bw / GX, y0 = cy * bh / GY, y1 = (cy + 1) * bh / GY;
      long sum = 0; int n = 0;
      for (int y = y0; y < y1; y += 2) for (int x = x0; x < x1; x += 2) { int i = y * stride + x * 4; sum += px[i] + px[i + 1] + px[i + 2]; n += 3; }
      double m = n > 0 ? (double)sum / n : 0; int c = cy * GX + cx;
      if (m < cmin[c]) cmin[c] = m; if (m > cmax[c]) cmax[c] = m;
    }
  }
  // one capture folded into the k smallest; returns its mean grey level of a sample (0 = a black capture)
  public static double Add(IntPtr hwnd) { return Take(hwnd, true); }
  // a capture's mean grey level only (the settle check before a window)
  public static double Probe(IntPtr hwnd) { return Take(hwnd, false); }
  static double Take(IntPtr hwnd, bool fold) {
    SetProcessDpiAwarenessContext(new IntPtr(-4));
    IntPtr old = SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(hwnd));
    RECT r; Bitmap bmp;
    try {
      if (!GetClientRect(hwnd, out r) || r.R <= r.L || r.B <= r.T) throw new Exception("game window has no client area");
      bmp = new Bitmap(r.R - r.L, r.B - r.T, PixelFormat.Format32bppRgb);
      using (Graphics g = Graphics.FromImage(bmp)) {
        IntPtr hdc = g.GetHdc();
        bool ok = PrintWindow(hwnd, hdc, 3);
        g.ReleaseHdc(hdc);
        if (!ok) { bmp.Dispose(); throw new Exception("game window capture failed (PrintWindow)"); }
      }
    } finally { if (old != IntPtr.Zero) SetThreadDpiAwarenessContext(old); }
    using (bmp) {
      if (fold && s != null && (bmp.Width != w || bmp.Height != h)) throw new Exception("the window size changed inside the window");
      if (fold) { w = bmp.Width; h = bmp.Height; Width = w; Height = h; }
      int bw = bmp.Width, bh = bmp.Height;
      BitmapData d = bmp.LockBits(new Rectangle(0, 0, bw, bh), ImageLockMode.ReadOnly, PixelFormat.Format32bppRgb);
      int n = d.Stride * bh;
      if (tmp == null || tmp.Length != n) tmp = new byte[n];
      Marshal.Copy(d.Scan0, tmp, 0, n);
      bmp.UnlockBits(d);
      long sum = 0; int cnt = 0;
      for (int i = 0; i < n; i += 4 * 97) { sum += tmp[i] + tmp[i + 1] + tmp[i + 2]; cnt += 3; }
      if (fold) {
        if (s == null) { s = new byte[k][]; for (int j = 0; j < k; j++) { s[j] = new byte[n]; for (int i = 0; i < n; i++) s[j][i] = 255; } }
        byte[] last = s[k - 1];
        for (int i = 0; i < n; i++) {
          byte v = tmp[i];
          if (v >= last[i]) continue;
          int j = k - 1;
          while (j > 0 && s[j - 1][i] > v) { s[j][i] = s[j - 1][i]; j--; }
          s[j][i] = v;
        }
        Cells(tmp, d.Stride, bw, bh);
        Count++;
      }
      return cnt > 0 ? (double)sum / cnt : 0;
    }
  }
  public static void Save(string path) {
    using (Bitmap bmp = new Bitmap(w, h, PixelFormat.Format32bppRgb)) {
      BitmapData d = bmp.LockBits(new Rectangle(0, 0, w, h), ImageLockMode.WriteOnly, PixelFormat.Format32bppRgb);
      if (Count < k) throw new Exception("fewer captures than the rank");
      byte[] res = s[k - 1];
      for (int i = 3; i < res.Length; i += 4) res[i] = 255;
      Marshal.Copy(res, 0, d.Scan0, Math.Min(res.Length, d.Stride * h));
      bmp.UnlockBits(d);
      bmp.Save(path, ImageFormat.Png);
    }
  }
}
'@
}

# ---- the comparison (pixels_compare.py's cells), the masks; Python on stdin (the OCR venv, -I)
$cmpPy = @'
import json, os, sys
from PIL import Image, ImageChops, ImageStat, ImageDraw
GX, GY = 64, 36
def cells(a, b, limit):
    d = ImageChops.difference(a, b).convert("L")
    w, h = d.size
    out = {}
    for y in range(GY):
        for x in range(GX):
            box = (x * w // GX, y * h // GY, (x + 1) * w // GX, (y + 1) * h // GY)
            out[y * GX + x] = (ImageStat.Stat(d.crop(box)).mean[0], box)
    return d, out
def moving(folder, n):
    # the cells that moved inside this capture window (-SelfMask frames: <frame>.moving)
    mv = os.path.join(folder, n[:-4] + ".moving")
    return {int(c) for c in open(mv).read().split()} if os.path.exists(mv) else set()
def excluded(folder, n, size, with_moving=True):
    # the cells under the controls left out (<frame>.exclude: "<control> x0 y0 x1 y1" per line, pixels); with_moving: and
    # the cells that moved inside this capture window (the pin passes' motion goes into the pin's mask; a run's own does not)
    p = os.path.join(folder, n[:-4] + ".exclude")
    out = moving(folder, n) if with_moving else set()
    if not os.path.exists(p):
        return out
    w, h = size
    for line in open(p):
        parts = line.split()
        if len(parts) != 5:
            continue
        x0, y0, x1, y1 = map(int, parts[1:])
        keep = parts[0] == "KEEP"   # only this box is compared: every cell wholly outside it is left out
        for y in range(GY):
            for x in range(GX):
                bx0, by0, bx1, by1 = x * w // GX, y * h // GY, (x + 1) * w // GX, (y + 1) * h // GY
                if (bx0 < x1 and x0 < bx1 and by0 < y1 and y0 < by1) != keep:
                    out.add(y * GX + x)
    return out
def frames(folder):
    return sorted(n for n in os.listdir(folder) if n.endswith(".png") and not n.endswith("_diff.png") and not n.endswith("_mask.png"))
def nomask(folder, n):
    # a -NoMask frame (<frame>.nomask): every cell compared, no mask allowed
    return os.path.exists(os.path.join(folder, n[:-4] + ".nomask"))
def cellnames(cs):
    return " ".join(f"({i % GX},{i // GX})" for i in sorted(cs))
mode = sys.argv[1]
if mode == "mask":   # mask <limit> <dilate> <out masks.json> <passA> <passB> [<passC> ...]: A is pinned, the others mask
    limit, dil, out, a, others = float(sys.argv[2]), int(sys.argv[3]), sys.argv[4], sys.argv[5], sys.argv[6:]
    b = others[-1]
    masks, bad, strict = {}, 0, []
    names = frames(a)
    for n in sorted(set(names).union(*[set(frames(o)) for o in others])):
        if n not in names or not all(os.path.exists(os.path.join(o, n)) for o in others):
            print(f"PIN-FAIL {n}: not in every pass"); bad += 1; continue
        ia = Image.open(os.path.join(a, n)).convert("RGB")
        hot, per = set(), []
        for o in others:
            ib = Image.open(os.path.join(o, n)).convert("RGB")
            if ia.size != ib.size:
                break
            d, cs = cells(ia, ib, limit)
            h = {i for i, (m, _) in cs.items() if m > limit}
            per.append(len(h)); hot |= h
        else:
            ib = None
        if ib is not None:
            print(f"PIN-FAIL {n}: sizes differ between passes"); bad += 1; continue
        grown = set()
        for i in hot:
            x, y = i % GX, i // GX
            for dy in range(-dil, dil + 1):
                for dx in range(-dil, dil + 1):
                    if 0 <= x + dx < GX and 0 <= y + dy < GY:
                        grown.add((y + dy) * GX + x + dx)
        if nomask(a, n):
            # a no-mask frame: its moving cells stay compared, and any cell the passes disagree on (or a control left out)
            # refuses the pin, by name - the frame's scene was not the same picture in every start
            strict.append(n)
            left_out = set().union(*[excluded(f, n, ia.size, with_moving=False) for f in [a] + others])
            moved = set().union(*[moving(f, n) for f in [a] + others])
            ok = not hot and not left_out
            bad += 0 if ok else 1
            masks[n] = [] if ok else sorted(grown | left_out)
            print(f"{'PINNED  ' if ok else 'PIN-FAIL'} {n:34} no mask allowed: the passes differ from the first in {'/'.join(map(str, per))} cells{(' ' + cellnames(hot)) if hot else ''}; controls left out {len(left_out)}; moved inside a pass's window (compared) {len(moved)}")
            grown = set(masks[n])
        else:
            left_out = set().union(*[excluded(f, n, ia.size) for f in [a] + others])
            grown |= left_out
            masks[n] = sorted(grown)
            # the noise's share of what is compared (the controls left out are no noise)
            share = len(grown - left_out) / max(1, GX * GY - len(left_out))
            ok = share <= 0.40
            bad += 0 if ok else 1
            print(f"{'PINNED  ' if ok else 'PIN-FAIL'} {n:34} the passes differ from the first in {'/'.join(map(str, per))} cells; left out {len(left_out)}, masked as noise {len(grown - left_out)} of the {GX * GY - len(left_out)} compared ({share:.0%})")
        # the mask drawn over pass A, for the reader
        m = ia.copy(); dr = ImageDraw.Draw(m, "RGBA")
        for i in grown:
            dr.rectangle(cs[i][1], fill=(255, 0, 255, 90))
        m.save(os.path.join(b, n[:-4] + "_mask.png"))
    json.dump({"limit": limit, "dilate": dil, "grid": [GX, GY], "nomask": strict, "masks": masks}, open(out, "w"), indent=0)
    sys.exit(1 if bad else 0)
if mode == "compare":   # compare <pinned> <run> <limit> <allow>
    pin, run, limit, allow = sys.argv[2], sys.argv[3], float(sys.argv[4]), int(sys.argv[5])
    pj = json.load(open(os.path.join(pin, "masks.json")))
    masks, strict = pj["masks"], set(pj.get("nomask", []))
    fails = unsure = total = 0
    for n in sorted(set(frames(pin)) | set(frames(run))):
        total += 1
        if not os.path.exists(os.path.join(run, n)):
            print(f"FAIL {n:34} not captured"); fails += 1; continue
        if not os.path.exists(os.path.join(pin, n)) or n not in masks:
            print(f"FAIL {n:34} not in the pinned set"); fails += 1; continue
        # a no-mask frame is compared in every cell: a pinned set that masks one of its cells, or that was pinned before
        # the frame became a no-mask frame, is refused (re-pin), never used to leave cells out
        if nomask(run, n) and (n not in strict or masks[n]):
            print(f"FAIL {n:34} a no-mask frame, but the pinned set {'masks ' + str(len(masks[n])) + ' of its cells' if n in strict else 'was pinned before the rule'}: re-pin"); fails += 1; continue
        ia, ib = Image.open(os.path.join(pin, n)).convert("RGB"), Image.open(os.path.join(run, n)).convert("RGB")
        if ia.size != ib.size:
            print(f"FAIL {n:34} size {ib.size}, pinned {ia.size}"); fails += 1; continue
        d, cs = cells(ia, ib, limit)
        # every cell stable at the pin is compared: the pin's mask alone (it holds every pin pass's control rectangles and
        # moving cells; a run's own rectangles would mask wherever a misplaced control lands now). The cells that moved
        # inside this run's own window stay compared (the composite waits motion out); when every changed cell is one of
        # them the frame is INCONCLUSIVE (motion the pin never saw, or a fault under it: run again), never masked
        mask = set(masks[n])
        moved = moving(run, n) - mask
        changed =sorted(((m, i) for i, (m, _) in cs.items() if m > limit and i not in mask), reverse=True)
        noisy = sum(1 for i, (m, _) in cs.items() if m > limit and i in mask)
        worst = f", worst {changed[0][0]:.1f} at cell ({changed[0][1] % GX},{changed[0][1] // GX}) px {cs[changed[0][1]][1][:2]}" if changed else ""
        if changed:   # every changed cell by name (the first 40)
            worst += "; cells " + cellnames([i for _, i in changed][:40]) + (" ..." if len(changed) > 40 else "")
        ok = len(changed) <= allow
        unclear = not ok and all(i in moved for _, i in changed)
        fails += 0 if ok or unclear else 1
        unsure += 1 if unclear else 0
        word = "PASS" if ok else "INCONCLUSIVE" if unclear else "FAIL"
        print(f"{word} {n:34} changed cells {len(changed)} (limit {limit:g}, allow {allow}), mean {ImageStat.Stat(d).mean[0]:.2f}, masked {len(mask)} (over limit there {noisy}), moved in this window and compared {len(moved)}{worst}")
        # the map: the difference x8, masked cells dimmed blue, changed cells outlined red
        m = d.point(lambda v: min(255, v * 8)).convert("RGB"); dr = ImageDraw.Draw(m, "RGBA")
        for i in mask:
            dr.rectangle(cs[i][1], fill=(0, 60, 160, 110))
        for _, i in changed:
            dr.rectangle(cs[i][1], outline=(255, 40, 40, 255), width=max(2, ia.size[1] // 540))
        m.save(os.path.join(run, n[:-4] + "_diff.png"))
    print(f"VERDICT {'FAIL' if fails else 'INCONCLUSIVE' if unsure else 'PASS'} {total - fails - unsure}/{total} frames")
    sys.exit(1 if fails else 3 if unsure else 0)
'@

# ---- the instance's display as found (restored at the end; none with no game)
$instDir = Join-Path $root "Install\instances\$Instance\Freelancer HD Edition"
$homeSize = if (-not $noGame) { Get-FLDisplaySize -InstallDir $instDir }
$homeMode = if (-not $noGame) { & {
    $fs = [IO.File]::OpenRead((Join-Path $instDir 'EXE\Freelancer.exe'))
    try {
        $fs.Position = 0x1B16CC; $full = $fs.ReadByte(); $fs.Position = 0x2477A; $st = $fs.ReadByte()
        if ($full -eq 1) { 'fullscreen' } elseif ($st -eq 0xCA) { 'windowed' } else { 'borderless' }
    } finally { $fs.Dispose() }
} }

# The flight frames' fixture (12 Nov 2026): SaveC706 with the ship 25 km straight above its saved spot (pos y + $skyRise,
# the same heading): the ship stands still there (throttle 0) and no lane, zone, station or traffic is in view, so the 3D
# scene behind the HUD's partly transparent panels is the same picture in every start and the flight frames need no mask
# (near Fort Bush, the traffic passing behind the panels masked weapon rows, badges and the legend: 219 cells of the 4K
# pad frame). Written next to the pinned set from the save folder's SaveC706.fl (only its pos line changed, the same bytes
# every time); deploy.ps1 -Save copies it into the save folder.
$skyRise = 25000
$skyClear = 10000   # no ship nearer than this while a flight frame is taken (contacts): checked, never assumed
function Get-SkySave {
    $src = Join-Path (Get-FLSaveDir) 'SaveC706.fl'
    $text = [IO.File]::ReadAllText($src, [Text.Encoding]::Latin1)
    $m = [regex]::Match($text, '(?m)^pos = ([^\r\n]*)')
    $p = @($m.Groups[1].Value -split ',' | ForEach-Object { [double]$_.Trim() })
    if (-not $m.Success -or $p.Count -ne 3) { throw "${src}: no 'pos = x, y, z' line" }
    $text = $text.Substring(0, $m.Index) + ('pos = {0:0}, {1:0}, {2:0}' -f $p[0], ($p[1] + $skyRise), $p[2]) + $text.Substring($m.Index + $m.Length)
    $out = Join-Path $base 'SaveC706sky.fl'
    if (-not (Test-Path -LiteralPath $out) -or [IO.File]::ReadAllText($out, [Text.Encoding]::Latin1) -cne $text) {
        New-Item -ItemType Directory -Force $base | Out-Null
        [IO.File]::WriteAllText($out, $text, [Text.Encoding]::Latin1)
    }
    $script:skyY = $p[1] + $skyRise
    $out
}
# The nearest ship's distance (contacts; [int]::MaxValue with none listed)
function Get-NearestShip {
    $d = @([regex]::Matches("$(Send-FL 'contacts')", '(?m)^\d+ \S+ dist=(\d+) ship ') | ForEach-Object { [int]$_.Groups[1].Value })
    if ($d.Count) { ($d | Measure-Object -Minimum).Minimum } else { [int]::MaxValue }
}
# The still sky: the ship at the fixture's spot, throttle 0, no ship within $skyClear, and the game's system arrival line
# ("New York System, Liberty Space.", typed on top left for a few seconds after the load: NN_ArrivalText, `hud2 state`'s
# arrival now=, 0 when none stands) gone; a summary when it holds
function Sky-Check {
    $p = Get-FLPilot; $near = Get-NearestShip
    $arrival = [regex]::Match("$(Send-FL 'hud2 state')", '(?m)^\s*arrival\s+want=\d now=([0-9A-F]{8})[^\r\n]*').Groups
    if ($p -and [Math]::Abs($p.Pos[1] - $script:skyY) -lt 50 -and (Send-FL 'state') -match 'throttle=0\b' -and $near -ge $skyClear -and
        $arrival[1].Value -eq '00000000' -and $arrival[0].Value -notmatch 'unresolved') {
        "still sky at y=$([int]$p.Pos[1]), throttle 0, nearest ship $(if ($near -eq [int]::MaxValue) { 'none' } else { "$near m" }), no arrival line"
    }
}

function Start-Game([string]$Save) {
    Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
    $r = & $deploy -Release -NoBuild -Background -Instance $Instance -Save $Save 2>&1 | Select-Object -Last 1
    "  start ${Save}: $r"
    $null = Send-FL 'set hud_style 0'   # live only: the instance's FLController.ini keeps its own
    $null = Send-FL 'set input_mode 1'
    $null = Send-FL 'set glyph_style 1'   # Xbox glyphs, as pinned: a real pad on the machine (the owner's DualSense) would switch the family
    $null = Send-FLS 'set chatter 2'      # the NPCs' radio chatter (random lines into the game's chat window) off; mission comms stay
}

# The subtitles' feed of shown lines (subtitles and chat window lines): its count and the last line's tick (GetTickCount ms).
function Get-Feed {
    $n = [int](((Send-FLS 'shown 999999999') -split "`n")[0] -replace '^last ', '')
    $tick = 0L
    if ($n -gt 0) {
        $l = @((Send-FLS "shown $($n - 1)") -split "`n" | Where-Object { $_ -match '^\d+ \d+ ' }) | Select-Object -Last 1
        if ($l) { $tick = [long](($l -split ' ')[1]) }
    }
    @{ n = $n; tick = $tick }
}
# ms since the feed's last line (the game's chat window drops a line ~10 s after it printed it: measured 12 Nov 2026, 1280x800)
function Get-FeedAge($f) { if (-not $f.tick) { return [long]::MaxValue }; ([long][Environment]::TickCount64 % 4294967296L - $f.tick + 4294967296L) % 4294967296L }

# One frame: wait for the state ($Check returns a summary when it holds), the composite over the window, the state again after it.
function Take([string]$Dir, [string]$Tag, [string]$Name, [scriptblock]$Check, [string]$What, [string[]]$Exclude = @(), [string[]]$Keep = @(),
              [double]$SelfMask = 0, [switch]$NoMask, [int]$FrameCaptures = 0) {
    # -FrameCaptures: this frame's own number of captures in the same window, its rank scaled to the same percentile (-Rank of -Captures)
    $useCaptures = if ($FrameCaptures -gt 0) { $FrameCaptures } else { $Captures }
    $useRank = [Math]::Max(1, [int][Math]::Round($Rank * $useCaptures / $Captures))
    $hwnd = (Get-FLGameProcess).MainWindowHandle
    for ($try = 1; ; $try++) {
        $null = Wait-FLUntil $Check $What 30
        # no subtitle or chat line on screen: 12 s past the feed's last line (the chat window's own fade), none during the window
        $null = Wait-FLUntil { (Get-FeedAge (Get-Feed)) -ge 12000 } 'no subtitle or chat line for 12 s' 90
        $feed = Get-Feed
        # a minimised window (the owner's show-desktop) is restored without being activated (flsub.ps1 Restore-FLWindow), written to window.log;
        # a restore during the capture window spoils that window (the picture comes back through the restore): it is taken again
        $wlog = Join-Path $Dir 'window.log'
        $restored = Restore-FLWindow $hwnd $wlog
        # the picture settled: 8 captures in a row within 1 grey level (a load's fade-in, a room's entry, a window's slide)
        $probes = [Collections.Generic.List[double]]::new()
        $null = Wait-FLUntil {
            if (Restore-FLWindow $hwnd $wlog) { $probes.Clear() }
            $probes.Add([FLRankShot]::Probe($hwnd)); $null = Wait-FLFramesPassed 2
            if ($probes.Count -gt 8) { $probes.RemoveAt(0) }
            $probes.Count -eq 8 -and (($probes | Measure-Object -Maximum).Maximum - ($probes | Measure-Object -Minimum).Minimum) -le 1.0
        } 'the picture settled (8 captures within 1 grey level)' 30
        # -Captures spread over the window (each 2+ game frames after the last), folded into their -Rank-th smallest value per pixel
        [FLRankShot]::Reset($useRank)
        $lums = [Collections.Generic.List[double]]::new()
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $restored = $false
        for ($i = 0; $i -lt $useCaptures; $i++) {
            $null = Wait-FLFramesPassed 2
            while ($sw.Elapsed.TotalSeconds -lt $i * $WindowSec / $useCaptures) { $null = Step-FLPopups; $null = Wait-FLFramesPassed 1 }
            if (Restore-FLWindow $hwnd $wlog) { $restored = $true; break }
            $lum = [FLRankShot]::Add($hwnd)
            if ($lum -lt 1) { throw "${Name}: a black capture (the window is not drawn)" }
            $lums.Add($lum)
        }
        $after = & $Check
        if (-not $after) { throw "${Name}: '$What' no longer held after the capture window" }
        $lines = (Get-Feed).n - $feed.n
        $range = ($lums | Measure-Object -Maximum).Maximum - ($lums | Measure-Object -Minimum).Minimum
        if (-not $restored -and $lines -eq 0 -and $range -le 2.0) { break }
        if ($try -ge 3) { throw "${Name}: 3 capture windows each saw a subtitle / chat line, an unsteady picture or a minimised window (last: $lines lines, range $([Math]::Round($range, 2)), restored $restored)" }
        if ($restored) { "  ${Name}: the window was minimised during the capture window and restored (window.log): again"; continue }
        "  ${Name}: $lines line(s) during the window, the picture's grey level ranged $([Math]::Round($range, 2)) (more than 2: something passed): again"
    }
    $file = Join-Path $Dir "${Tag}_$Name.png"
    [FLRankShot]::Save($file)
    # -SelfMask <grey levels>: the cells whose mean moved by more than that inside this very window (3D motion), written down:
    # masked at the pin, compared (and named when they alone changed) in a run
    $moved = ''
    if ($SelfMask -gt 0) { $moved = [FLRankShot]::Moving($SelfMask); Set-Content -LiteralPath ($file -replace '\.png$', '.moving') $moved }
    # -NoMask: a frame every cell of which is compared (the mask and compare modes hold it to that)
    if ($NoMask) { Set-Content -LiteralPath ($file -replace '\.png$', '.nomask') 'every cell compared: no mask allowed' }
    # controls left out of the comparison: their rectangles as the game reports them now (uiitems: center and half, pixels)
    # ($Keep: only the box around those controls is compared)
    if ($Exclude.Count -or $Keep.Count) {
        $items = "$(Send-FL 'uiitems')" -split "`n"
        $rect = {
            param($c)
            $l = $items | Where-Object { $_ -match "^$c\s" } | Select-Object -First 1
            if (-not $l -or $l -notmatch 'center=\((-?\d+),(-?\d+)\) half=\((\d+),(\d+)\)') { throw "${Name}: the control $c is not listed (uiitems)" }
            , @(([int]$Matches[1] - [int]$Matches[3]), ([int]$Matches[2] - [int]$Matches[4]), ([int]$Matches[1] + [int]$Matches[3]), ([int]$Matches[2] + [int]$Matches[4]))
        }
        $rects = @(foreach ($c in $Exclude) { "$c $((& $rect $c) -join ' ')" })
        if ($Keep.Count) {
            $all = @(foreach ($c in $Keep) { , (& $rect $c) })
            $rects += 'KEEP {0} {1} {2} {3}' -f ($all | ForEach-Object { $_[0] } | Measure-Object -Minimum).Minimum, ($all | ForEach-Object { $_[1] } | Measure-Object -Minimum).Minimum,
                ($all | ForEach-Object { $_[2] } | Measure-Object -Maximum).Maximum, ($all | ForEach-Object { $_[3] } | Measure-Object -Maximum).Maximum
        }
        Set-Content -LiteralPath ($file -replace '\.png$', '.exclude') $rects
    }
    $line = "{0}_{1}: {2} captures over {3:n1} s, {4}x{5}{7} | {6}" -f $Tag, $Name, [FLRankShot]::Count, $sw.Elapsed.TotalSeconds, [FLRankShot]::Width, [FLRankShot]::Height, $after, $(if ($useCaptures -ne $Captures) { ", rank $useRank" })
    if ($SelfMask -gt 0) { $line += " | cells that moved over $SelfMask in the window: $(@($moved -split ' ' | Where-Object { $_ }).Count)" }
    Add-Content -LiteralPath (Join-Path $Dir 'states.txt') $line
    "  frame ${Tag}_${Name}: $([FLRankShot]::Count) captures"
}

# ---- the verbs' checks
function Get-Facts {
    @{ state = "$(Send-FL 'state')"; device = "$(Send-FL 'device')"; padhud = "$(Send-FL 'padhud state')"; hud2 = "$(Send-FL 'hud2 state')" }
}
$flightPad = {
    $f = Get-Facts
    $d = [regex]::Match($f.padhud, 'drawn badges=(\d+) legendRows=(\d+)')
    if ($f.state -match 'mode=space' -and $f.state -match 'paused=0' -and $f.device -match 'device=pad' -and $f.hud2 -match '^hud2 active=0 visible=0' -and
        $f.padhud -match 'active=1 flying=1 modal=0' -and $f.padhud -match 'target=0 ' -and [int]$d.Groups[1].Value -gt 0 -and [int]$d.Groups[2].Value -gt 0 -and
        ($sky = Sky-Check)) {
        "device=pad hud2 off, padhud badges=$($d.Groups[1].Value) legendRows=$($d.Groups[2].Value) $([regex]::Match($f.padhud, '(?m)^legend.*$').Value.Trim()); $sky"
    }
}
$flightKbm = {
    $f = Get-Facts
    $d = [regex]::Match($f.padhud, 'drawn badges=(\d+) legendRows=(\d+)')
    # the idle cursor already hidden (hud_fade 1: 3 s with the mouse still, the owner's decision 76), so it cannot go DURING the window: the
    # composite's low rank kept the cursor's dark outline where it showed in some of the captures (13 Nov 2026: a ghost of the ring in the
    # 1280x800 frame, its window 6 s from the switch to the keyboard; the 4K window, 11 s, had none)
    # (on the classic HUD the idle_cursor row decides, not hud_fade: 14 Nov 2026)
    $ic = "$(Send-FL 'idlecursor')"
    $hides = $ic -match 'idle_cursor=1' -and ($ic -match 'rule=classic' -or $ic -match 'hud_fade=1')
    $idle = if ($hides) { $ic -match 'eligible=1 hidden=1' } else { $true }
    if ($f.state -match 'mode=space' -and $f.state -match 'paused=0' -and $f.device -match 'device=kbm' -and $f.hud2 -match '^hud2 active=0 visible=0' -and
        $f.padhud -match 'target=0 ' -and $d.Success -and $d.Groups[1].Value -eq '0' -and $d.Groups[2].Value -eq '0' -and $idle -and ($sky = Sky-Check)) {
        "device=kbm hud2 off, padhud draws nothing (badges=0 legendRows=0); $(if ($hides) { 'the idle cursor hidden; ' })$sky"
    }
}
function Options-Check([int]$Tab, [int]$Focus = -1) {
    $u = "$(Send-FL 'ui state')"; $s = "$(Send-FL 'state')"
    if ($u -match "^open=1 host=options tab=$Tab focus=$(if ($Focus -ge 0) { "$Focus " } else { '\d+ ' })" -and $u -match 'pageState=2' -and $u -match 'settled=1' -and $s -match 'paused=1' -and (Send-FL 'hud2 state') -match '^hud2 active=0') {
        "hosted in the game's Options: " + [regex]::Match($u, 'open=\S+ host=\S+ tab=\S+ focus=\S+').Value + ' ' + [regex]::Match($u, 'panel=\S+').Value
    }
}
function Room-Check([string]$Want) {
    $b = "$(Send-FL 'bases')"; $h = "$(Send-FL 'hotspots')"
    if ($b -match 'screen=room ' -and $b -match 'pad=1' -and $h -match 'hotspots \d+ ready' -and $h -match "(?m)^IDS_HOTSPOT_$Want\s" -and
        $b -match 'promptbar screen=room count=(\d+) pending=0 visible=1 drawn=(\d+)' -and [int]$Matches[2] -gt 0 -and (Send-FL 'hud2 state') -match '^hud2 active=0') {
        "room with IDS_HOTSPOT_$Want, " + [regex]::Match($b, "promptbar screen=room count=\d+ .*?items='[^']*'").Value -replace ' area=\S+| widest=\S+| column=\S+', ''
    }
}
function Dealer-Check([int]$Side, [string]$Focus) {
    $b = "$(Send-FL 'bases')"; $h = "$(Send-FL 'hotspots')"
    if ($b -match 'screen=equipment ' -and $b -match "side=$Side " -and $b -match "focused=$Focus " -and $h -notmatch 'busy' -and
        (Send-FL 'uiitems') -match '(?m)^\s*ItemButton1\s' -and (Send-FL 'hud2 state') -match '^hud2 active=0') {
        "the game's dealer window, side=$Side focused=$Focus, " + [regex]::Match($b, "promptbar screen=equipment .*?items='[^']*'").Value -replace ' area=\S+| widest=\S+| column=\S+', ''
    }
}
function Focused { [regex]::Match("$(Send-FL 'bases')", 'focused=(\S*)').Groups[1].Value }
function ListPos { $b = "$(Send-FL 'bases')"; '{0} {1}' -f [regex]::Match($b, 'focused=(\S*)').Groups[1].Value, [regex]::Match($b, 'stickBar=(-?[\d.]+)\(').Groups[1].Value }

# ---- one pass: every frame at every size into $Dir
function Invoke-Pass([string]$Dir) {
    New-Item -ItemType Directory -Force $Dir | Out-Null
    Set-Content -LiteralPath (Join-Path $Dir 'states.txt') "classic_frames pass $(Get-Date -Format s), build $buildInfo, instance $Instance, window $WindowSec s"
    foreach ($size in $Sizes) {
        $tag = $size -replace '[ ,]+', 'x'
        "--- $tag"
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
        $null = Set-FLInstanceDisplay $Instance 'borderless' $size
        # flight, in the still sky (Get-SkySave)
        Start-Game (Get-SkySave)
        # -NoMask: no cell of these frames may be masked (the pin refuses one that would be, a run fails on a pinned set that
        # masks one). -SelfMask 20 only names the cells that moved inside the window (the HUD's panels pulse by up to ~15
        # per cell, measured 12 Nov 2026 over a glow cycle at 4K): compared all the same, INCONCLUSIVE when they alone changed
        # -FlightCaptures (48, the 12th smallest: the same low percentile as 6 of 24): the player ship's engine glow below the
        # centre changes its size from capture to capture, so the percentile of 24 captures is a noisy estimate there: the
        # glow's cells of 1280x800_flight_pad read 187.5 to 195.3 (mean grey) over the runs of 12-13 Nov, up to 8.0 against
        # the limit 6 in three of them. Twice the captures halve that noise's variance (13 Nov 2026, three runs each: 48
        # captures 187.6-187.9; 24 captures 187.8-189.1; the minimum, rank 1, 179.3-183.4: the glow dips deeper at times,
        # so the minimum is the noisiest - audit 101_ batch 2 D).
        Take $Dir $tag 'flight_pad' $flightPad 'flight, the pad form of the classic HUD' -SelfMask 20 -NoMask -FrameCaptures $FlightCaptures
        $null = Send-FL 'set input_mode 2'
        Take $Dir $tag 'flight_kbm' $flightKbm 'flight, the keyboard-and-mouse form of the classic HUD' -SelfMask 20 -NoMask -FrameCaptures $FlightCaptures
        $null = Send-FL 'set input_mode 1'
        $null = Wait-FLUntil { & $flightPad } 'the pad form again' 10
        # the options page hosted in the game's Options screen (pause menu, code 4 = Options: docs\FREELANCER_RE.md)
        $null = Send-FL 'pause'
        $box = Wait-FLUntil { [regex]::Match("$(Send-FL 'pausecodes')", '(?m)^(PauseButtonBox\d+) ctl \S+ code 4\s*$').Groups[1].Value } 'the pause menu''s Options' 10
        $r = Send-FL "click $box"; if ($r -notmatch '^clicked') { throw "pause menu Options: $r" }
        $null = Wait-FLUntil { (Send-FL 'ui state') -match 'pageState=1' } 'the game''s Options screen' 10
        $r = Send-FL 'ui options open'; if ($r -notmatch '^(hosting|open)') { throw "ui options open: $r" }
        $null = Wait-FLUntil { Options-Check 0 -1 } 'the FLvibed page hosted and settled' 15
        # The page's focus and hover follow the game's cursor until the pad drives (options_page.cpp: hover = focus once the
        # pointer moved since the pad drove), and where the game's cursor stands at the open differs from start to start: the
        # focus onto the first row, then a neutral input (`ui press` with no button word: the pad drove, nothing pressed)
        $r = Send-FL 'ui focus hud_style'; if ($r -notmatch '^focus (\d+)') { throw "ui focus hud_style: $r" }
        $f0 = [int]$Matches[1]
        $null = Send-FL 'ui press none'
        # the Options screen's page buttons (the game's button class, FLvibed's page button too) are left out: their shading band
        # stands still within a game but sits elsewhere in each fresh start (measured 12 Nov 2026: 30 s still, moved between starts)
        # Only the Options screen is compared (the box of its controls: the page buttons to the close button): the flight HUD
        # behind it is the flight frames' subject, and the pause freezes its glow pulse at whatever phase it had (the rank
        # composite cannot wait out a frozen animation; 12 Nov 2026 a run failed one cell of the maneuver bar so)
        $pageButtons = 'interfaceButton', 'keymapButton', 'VideoButton', 'AudioButton', 'FLvibedOptionsButton', 'MainMenuButton'
        $screen = $pageButtons + 'PreferencesCloseButton'
        Take $Dir $tag 'options_hud' { Options-Check 0 $f0 } "the FLvibed page hosted, tab HUD, focus $f0 (HUD 2), the pad driving" $pageButtons $screen
        $r = Send-FL 'ui tab 1'; if ($r -notmatch '^tab ') { throw "ui tab 1: $r" }
        $null = Send-FL 'ui press none'
        Take $Dir $tag 'options_controller' { Options-Check 1 0 } 'the FLvibed page hosted, tab CONTROLLER, focus 0, the pad driving' $pageButtons $screen
        $null = Send-FL 'ui options cancel'
        # docked: rooms and the game's dealer window
        Start-Game 'SaveC701.fl'
        # prompt_bar 1 (Always): in a room the default (Contextual) fades the prompts 4 s after the last action, a timer no
        # frame can hold still; the windows show the same either way (ui_meta.h prompt_bar: "on every window ... as long as it is open")
        $null = Send-FL 'set prompt_bar 1'
        $null = Goto-FLRoom 'Bar'
        # (-SelfMask: the bar's characters idle and walk; their cells move inside the window and are left out, the HUD's do not)
        # The pad's hover starts on the third character from the left, and the patrons in the bar's seats differ from start to
        # start (their kinds and faces): the D-pad (the pad's CHARACTER) onto the bartender, the rightmost character (his
        # place behind the counter: x 2931 of 3840 in every start seen), the same man in every start. His offer kind is not:
        # talk_bribe or talk_rumor, and his card's text and width follow it, so the pins mask the card (12 Nov 2026)
        $bartender = {
            $c = "$(Send-FL 'bases chars')"
            $rows = @([regex]::Matches($c, '(?m)^(\*?)\s*(\d+) \S+ (\S+) x=(\d+)'))
            if ($rows.Count -lt 1) { throw "the bar lists no characters: $c" }
            $b = $rows | Sort-Object { [int]$_.Groups[4].Value } | Select-Object -Last 1
            $h = $rows | Where-Object { $_.Groups[1].Value -eq '*' } | Select-Object -First 1
            @{ at = [int]$b.Groups[2].Value; kind = $b.Groups[3].Value; hovered = $(if ($h) { [int]$h.Groups[2].Value } else { -1 }) }
        }
        for ($i = 0; $i -lt 8; $i++) {
            $p = & $bartender
            if ($p.hovered -eq $p.at) { break }
            Press-FLPad $(if ($p.hovered -lt $p.at) { 'Right' } else { 'Left' })
            $null = Wait-FLUntil { (& $bartender).hovered -ne $p.hovered } 'the hover moving to the next character' 5
        }
        $barCheck = { $p = & $bartender; if ($p.hovered -eq $p.at) { $r = Room-Check 'MISSIONVENDOR'; if ($r) { "the bartender ($($p.kind), character $($p.at)) hovered, $r" } } }
        Take $Dir $tag 'room_bar' $barCheck 'the bar (its job board hotspot), the bartender hovered, the prompt bar' -SelfMask $Limit
        # the equipment room itself is no frame: its light shafts and the walking dealer differ between fresh starts in a
        # quarter of the picture (21-28 % masked over three passes, and a fourth start still changed 20-30 cells beside them)
        $null = Goto-FLRoom 'Equipment'
        $null = Wait-FLUntil { Room-Check 'EQUIPMENTDEALER' } 'the equipment room, the prompt bar' 30
        $r = Send-FL 'goto IDS_HOTSPOT_EQUIPMENTDEALER'; if ($r -notmatch '^clicked') { throw "the dealer: $r" }
        Take $Dir $tag 'dealer_open' { Dealer-Check 1 'ItemView01' } 'the dealer window open, the dealer''s list'
        # down to the fifth (last visible) row, then the list scrolls by two (the game's Inventory_Scroll): driven by the list's
        # state, not a press count (12 Nov 2026: two runs, 4K and 1280x800, ended scrolled by 4 after six presses); a list
        # scrolled past 2 is walked back up (Up moves the focus to the first row, then scrolls)
        for ($i = 1; ($was = ListPos) -ne 'ItemView05 2.0'; $i++) {
            if ($i -gt 16 -or $was -notmatch '^ItemView(\d+) (-?[\d.]+)$') { throw "the dealer's list: not at row 5 scrolled by 2 after $($i - 1) steps (at '$was')" }
            $way = if ([double]$Matches[2] -gt 2) { 'Up' } else { 'Down' }   # (not $dir: PowerShell names are case-blind, $Dir is the pass folder)
            Press-FLPad $way
            $null = Wait-FLUntil { (ListPos) -ne $was } "the list's step $i ($way from $was)" 5
            if ($i -gt 6) { Add-Content -LiteralPath (Join-Path $Dir 'states.txt') "${tag}_dealer list step ${i}: $way from '$was' (more steps than the six a clean walk takes)" }
        }
        Take $Dir $tag 'dealer_scrolled' { if ((ListPos) -eq 'ItemView05 2.0') { Dealer-Check 1 'ItemView05' } } 'the dealer''s list moved down past its visible rows (focus row 5, scrolled by 2)'
        Press-FLPad Left
        $null = Wait-FLUntil { "$(Send-FL 'bases')" -match 'side=2 ' } 'the player''s side' 5
        $pf = Focused
        Take $Dir $tag 'dealer_player' { Dealer-Check 2 $pf } "the player's side ($pf)"
        Press-FLPad B
        $closed = Wait-FLUntil { if ((Send-FL 'uiitems') -notmatch '(?m)^\s*ItemButton1\s') { Room-Check 'EQUIPMENTDEALER' } } 'B closed the dealer: the equipment room again' 30
        Add-Content -LiteralPath (Join-Path $Dir 'states.txt') "${tag}_dealer_closed (a state, no frame): $closed"
        "  ${tag}: the dealer closed by B (checked, no frame)"
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
    }
}

# A game of this run that crashed left a block in the crash recorder's log (Get-FLCrashBlocks, tools\fldata.ps1): a FAIL quoting it
function Crash-Lines { foreach ($c in @(Get-FLCrashBlocks $started)) { "FAIL a game crashed during the run (EXE\FLvibed_crash.log):"; $c } }
$code = 1
try {
    if ($Pin) {
        if ($PinFrom.Count) {   # the passes of an earlier -Pin (a refused one, say): no game, the mask and the swap only
            if ($PinFrom.Count -lt 2) { throw '-PinFrom: two pass folders at least' }
            $passes = @($PinFrom | ForEach-Object { (Resolve-Path -LiteralPath $_).Path })
            "=== pin from $($passes.Count) earlier passes: $($passes -join ' + ')"
        } else {
            $passes = @(1..([Math]::Max(2, $PinPasses)) | ForEach-Object { Join-Path $base "${stamp}_pin$_" })
            foreach ($p in $passes) { "=== pin pass $($passes.IndexOf($p) + 1) of $($passes.Count) (each a fresh start; what differs from the first is masked) -> $p"; Invoke-Pass $p }
        }
        $a = $passes[0]
        $new = Join-Path $base "${stamp}_pinned_new"
        New-Item -ItemType Directory -Force $new | Out-Null
        Get-ChildItem -LiteralPath $a -Filter '*.png' | Copy-Item -Destination $new
        Copy-Item (Join-Path $a 'states.txt') (Join-Path $new 'states.txt')
        $out = $cmpPy | & $python -I - mask "$Limit" "$Dilate" (Join-Path $new 'masks.json') @passes
        $code = $LASTEXITCODE
        if ($PinFrames) { $out = @($out | ForEach-Object { if ($_ -match '^(PINNED  |PIN-FAIL) (\S+)' -and $Matches[2] -notmatch $PinFrames) { "KEPT     $($Matches[2]) (the set before's image and mask; these passes: $(($_ -split '\s+', 3)[2]))" } else { $_ } }) }
        $out
        # the build the passes ran: their own header (states.txt line 1) when they are earlier passes (-PinFrom: run from another checkout, a lane
        # say, whose build this checkout's build_info.txt does not know - the 13 Nov 2026 re-pin named main's old 4a7088da for a lane's 04b0d0fe)
        $passBuilds = @($passes | ForEach-Object { [regex]::Match("$(Get-Content -LiteralPath (Join-Path $_ 'states.txt') -TotalCount 1 -ErrorAction SilentlyContinue)", 'build (.+?), instance').Groups[1].Value } | Where-Object { $_ } | Sort-Object -Unique)
        $from = "from $($passes -join ' + '); build $(if ($passBuilds.Count) { $passBuilds -join ' / ' } else { "$buildInfo (this checkout's: the passes name none)" })"
        if ($PinFrames) {
            # -PinFrames: only the matching frames come from these passes (their lines above decide the pin); every other
            # frame keeps the current pinned set's image and mask, so a re-pin of one group changes nothing else
            $code = 0
            foreach ($l in $out) { if ($l -match '^PIN-FAIL (\S+)' -and $Matches[1] -match $PinFrames) { $code = 1 } }
            $oldSet = Get-Content -LiteralPath (Join-Path $pinned 'masks.json') -Raw | ConvertFrom-Json -AsHashtable
            $newSet = Get-Content -LiteralPath (Join-Path $new 'masks.json') -Raw | ConvertFrom-Json -AsHashtable
            $kept = @($oldSet.masks.Keys | Where-Object { $_ -notmatch $PinFrames } | Sort-Object)
            if (-not @($newSet.masks.Keys | Where-Object { $_ -match $PinFrames }).Count) { throw "-PinFrames '$PinFrames' matches no frame" }
            # a kept no-mask frame keeps the old pin only when that pin was a no-mask one too (in its list, no cell masked): then it stays
            # in the list (13 Nov 2026: every kept no-mask frame was refused, so one flight frame could not be re-pinned alone)
            $oldStrict = @($oldSet.nomask)
            foreach ($n in $kept) {
                if ($newSet.nomask -contains $n -and ($oldStrict -notcontains $n -or @($oldSet.masks[$n]).Count)) { throw "-PinFrames: $n is a no-mask frame now and cannot keep an older pin's mask" }
                Copy-Item -LiteralPath (Join-Path $pinned $n) (Join-Path $new $n) -Force
                $newSet.masks[$n] = $oldSet.masks[$n]
            }
            $newSet.nomask = @($newSet.nomask | Where-Object { $_ -match $PinFrames -or $oldStrict -contains $_ })
            # states.txt: the matching frames' lines from these passes, every other frame's from the set before
            $isNew = { param($l) $l -notmatch '^(\d+x\d+_[a-z_]+?)[: ]' -or "$($Matches[1]).png" -match $PinFrames }
            $st = @(Get-Content -LiteralPath (Join-Path $new 'states.txt') | Where-Object { & $isNew $_ }) +
                  "(the frames below: kept from the set before, $pinned)" +
                  @(Get-Content -LiteralPath (Join-Path $pinned 'states.txt') -ErrorAction SilentlyContinue | Where-Object { -not (& $isNew $_) })
            Set-Content -LiteralPath (Join-Path $new 'states.txt') $st
            $newSet | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $new 'masks.json')
            $was = (Get-Content -LiteralPath (Join-Path $pinned 'pinned.txt') -ErrorAction SilentlyContinue) -join ' '
            $from = "frames matching '$PinFrames' $from; the other $($kept.Count) frames kept from the set before ($was)"
            "  -PinFrames '$PinFrames': $($kept.Count) frames kept from $pinned"
        }
        if (-not $noGame) { $crashed = @(Crash-Lines); if ($crashed.Count) { $out = @($out) + $crashed; $crashed; $code = 1 } }
        Set-Content -LiteralPath (Join-Path $passes[-1] 'pin_result.txt') $out
        if ($code -ne 0) { "PIN REFUSED: see the PIN-FAIL lines; the pinned set is unchanged ($pinned)" }
        else {
            Set-Content -LiteralPath (Join-Path $new 'pinned.txt') "pinned $(Get-Date -Format s) $from; limit $Limit dilate $Dilate window $WindowSec s; sizes $($Sizes -join ' / ')"
            if (Test-Path -LiteralPath $pinned) { [IO.Directory]::Move($pinned, (Join-Path $base "${stamp}_pinned_old")) }
            [IO.Directory]::Move($new, $pinned)
            "PINNED: $pinned (the previous set, if any: ${stamp}_pinned_old)"
        }
    } else {
        $run = Join-Path $base $stamp
        "=== run -> $run (pinned: $((Get-Content (Join-Path $pinned 'pinned.txt') -ErrorAction SilentlyContinue) -join ' '))"
        Invoke-Pass $run
        $out = $cmpPy | & $python -I - compare $pinned $run "$Limit" "$Allow"
        $code = $LASTEXITCODE
        $crashed = @(Crash-Lines); if ($crashed.Count) { $out = @($out) + $crashed; $code = 1 }
        $out
        Set-Content -LiteralPath (Join-Path $run 'result.txt') ($out + "build $buildInfo" + ('{0:n0} s' -f ((Get-Date) - $started).TotalSeconds))
    }
} catch {
    "ERROR: $($_.Exception.Message)"
    try { "state: $(Get-FLStoryState)" } catch { }
    $code = 2
} finally {
    if (-not $noGame) {   # (a pin from earlier passes started nothing here: see $noGame)
        Get-FLGameProcess | Stop-Process -Force -ErrorAction SilentlyContinue
        Get-FLGameProcess | Wait-Process -Timeout 15 -ErrorAction SilentlyContinue
        $null = Set-FLInstanceDisplay $Instance $homeMode $homeSize
        # the screenshot samplers this script's starts began (deploy -Background): their stop file, then any still running
        $stop = Join-Path $root "research\experience\shots\sampler$Instance.stop"
        Set-Content -LiteralPath $stop 'classic_frames'
        $mine = { @(Get-CimInstance Win32_Process -Filter "Name='pwsh.exe'" | Where-Object { $_.CommandLine -match 'flshots\.ps1' -and $_.CommandLine -match "-Instance $Instance(\s|$)" -and $_.CreationDate -ge $started }) }
        $sw = [Diagnostics.Stopwatch]::StartNew()
        while ((& $mine).Count -and $sw.Elapsed.TotalSeconds -lt 20) { Start-Sleep -Milliseconds 250 }
        & $mine | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    }
}
'classic_frames {0} in {1:n0} s (exit {2})' -f $(if ($code -eq 0) { 'PASS' } elseif ($Pin) { 'PIN FAILED' } elseif ($code -eq 3) { 'INCONCLUSIVE' } elseif ($code -eq 2) { 'ERROR' } else { 'FAIL' }), ((Get-Date) - $started).TotalSeconds, $code
exit $code
