"""Two builds' frames from tests\\pixels.ps1, compared per scene (OCR venv python, -I).

The game goes on behind the UI (stars, the ship), so a frame never matches exactly: a difference is read by where it is. Each frame
is cut into 64 x 36 cells; a cell counts when its mean difference passes the limit. Reported: how many cells, the worst one, and a
map <scene>_diff.png in the second run's folder (bright = moved). Exit 1 when a scene is missing.

Usage: pixels_compare.py <labelA> <labelB> [--limit 6]
"""
import os
import sys

from PIL import Image, ImageChops, ImageStat

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GRID_X, GRID_Y = 64, 36


def folder(label):
    return os.path.join(ROOT, "research", "experience", "runs", "pixels_" + label)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    limit = float(sys.argv[sys.argv.index("--limit") + 1]) if "--limit" in sys.argv else 6.0
    args = [a for a in args if a != str(limit) and a != sys.argv[-1] or "--limit" not in sys.argv]
    a, b = folder(args[0]), folder(args[1])
    failed = False
    for name in sorted(n for n in os.listdir(a) if n.endswith(".png") and not n.endswith("_diff.png")):
        other = os.path.join(b, name)
        if not os.path.exists(other):
            print(f"ABSENT {name}")
            failed = True
            continue
        ia, ib = Image.open(os.path.join(a, name)).convert("RGB"), Image.open(other).convert("RGB")
        if ia.size != ib.size:
            print(f"SIZE   {name}: {ia.size} {ib.size}")
            failed = True
            continue
        d = ImageChops.difference(ia, ib).convert("L")
        w, h = d.size
        cells = []
        for y in range(GRID_Y):
            for x in range(GRID_X):
                box = (x * w // GRID_X, y * h // GRID_Y, (x + 1) * w // GRID_X, (y + 1) * h // GRID_Y)
                m = ImageStat.Stat(d.crop(box)).mean[0]
                if m > limit:
                    cells.append((m, box))
        cells.sort(reverse=True)
        worst = f", worst {cells[0][0]:.1f} at {cells[0][1][:2]}" if cells else ""
        print(f"{name:36} mean {ImageStat.Stat(d).mean[0]:5.2f}, cells over {limit:g}: {len(cells)} of {GRID_X * GRID_Y}{worst}")
        d.point(lambda v: min(255, v * 8)).save(os.path.join(b, name[:-4] + "_diff.png"))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
