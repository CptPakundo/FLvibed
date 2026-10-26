# Footage scans: HUD lists read from the 2020 playlist `[footage, OCR]`

The user's 2020 playthrough (19 videos, `research\videos\playlist`, ~12.7 h) read row by row at 2 fps by
`tools\ocr\flocr.py` (CRNN + game-data snapping; held-out accuracy ~98 %, frame voting covers the rest), run by
`tools\vidmissiles.py [--panel=contacts] --all` (~67x real time at 2 fps: 4 processes x 6 streams, NVDEC hardware crop;
5 fps 26x, 10 fps 9.4x - keep 2 fps, denser only for short windows). Outputs in `research\videos\catalogue\tracks\`
(local, kept for queries):

| File | What |
|---|---|
| `<id>.weapons.tsv` | weapon panel, every row: t, row, text (snapped), valid, raw read, confidence |
| `<id>.ordnance.csv` / `.launches.tsv` / `.ammo_gains.tsv` | ammo counts per munition, launches, gains - results in [consumables.md](consumables.md) |
| `<id>.contacts.tsv` | contact list, every row (same columns): `<distance> <faction> - <name>`, objects, routes `<a>-><b>` |

Contact list (20 Oct 2026, first look; 8-14k valid rows per video, 2-4 per frame): who was around (factions over time:
fight composition, engagement distances), the routes taken (trade-lane / gate legs - the player's travel choices),
loot in space (Shield Battery, Nanobots), story companions (King, Juni, Order ships). Not captured: hostility (the
row colour; the OCR reads the brightest channel) - add a hue sample per row when a study needs it.
Queries: plain awk / pandas over the tsv files; valid = column 4 = 1.
