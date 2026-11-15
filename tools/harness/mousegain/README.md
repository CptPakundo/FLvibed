# Mouse gain recorder and analysis

**What it is for:** measuring the game's cursor against the real mouse's raw counts. It is read-only and runs from
outside the game: a Raw Input listener in its own process, plus ReadProcessMemory of the game's cursor (0x616840 /
0x616844) and the mouse flags (0x6167CC). A mouse fault then shows as cursor pixels per raw count, whatever the hand
speed.

**Rule:** a real mouse cannot be simulated; the owner's hand moves it. Record a baseline without the suspected cause
before any fix.

## Recorder: `mouse_gain_rec.ps1`

```
mouse_gain_rec.ps1 -PathLike '<pattern for the game exe path>' -WaitSeconds n -MaxSeconds n -Out file.csv
```

It waits up to `-WaitSeconds` for the game, records until the game exits or `-MaxSeconds` passes, and writes
`<file>.start.txt` with the wall clock of row 0. `-Out` defaults to `mouse_gain.csv` beside the script.

CSV columns: `t_us, rawX, rawY, rawN, injX, injY, cx, cy, wx, wy, flags, fg`
(time in microseconds; raw counts and their event count; program-sent moves; the game's cursor; Windows' pointer;
the mouse object's flags; foreground).

## Analysis scripts (run with `research\tools\ocr\.venv\Scripts\python.exe -I`)

- `gain_phase.py`: gain per raw event by the event's counts, per phase (`HH:MM:SS.fff=label` arguments).
- `gain_analyse.py`: 100 ms windows per phase, raw and cursor timing.
- `gain_flags.py`: the flags timeline and gain per flags value.
- `gain_pos.py`: gain by the cursor's place on screen.
- `gain_edge.py`: what the cursor does at the screen edges.
- `gain_inj.py`: gain per program-sent event.
- `raw_selfcheck.ps1`: program-sent moves with `mouse_raw` 0 and 1 in a front test game. **Never run it without the
  owner's word**: it needs a game in front. Output goes to `out\` beside it.

## Result of 14 Nov 2026

- The game's own DirectInput mouse: about 3 to 4 px per raw count when slow and 5.7 to 7 when fast, also without FLvibed.
- With raw mouse input: 3.0 to 3.2 px per count at every speed.
