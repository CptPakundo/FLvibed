# The design bench

A program that draws HUD 2's flight HUD with no game running, from frames the game recorded, and checks that it draws
exactly what the game drew. It is the first step of the bench planned in the audit's design 71b (section 7, package
B5a). Everything it reads and writes stays on this machine: the recordings hold the game's names and font, so nothing
here is ever published.

## Start it: double-click `HUD 2 Bench`

The shortcut **HUD 2 Bench** sits in the folder that holds FLvibed. Double-click it: one window opens with the bench in
it. Close the window and the bench is gone (nothing keeps running).

In the window:

- **Recorded state** (left, top): the recordings it found, the fullest and newest first; the states in one, the most
  crowded first. A click draws one.
- **Picture**: HUD size (80 / 100 / 130 %), the preset (Standard / Focus), pad or keyboard and mouse (where the
  recording has both), the screen size (1280 x 800 or 3840 x 2160: the recording of that size when there is one).
- **Settings**: every setting the recording carries, with a search box; a changed one is marked, with Reset beside it.
- **Layout numbers**: where each panel sits and how big it is, with a search box, - and + buttons and Reset.
- **Saved variants**: give the current changes a name and save them; load one again later.
- Above the picture: **Before** (the recording as it is), **After** (with your changes), **Side by side**,
  **Difference**, and **Flip** (or the F key). Every change redraws at once. One line says what the picture cannot show
  truly (the game's 3D views are hatched boxes; a redraw at another size than recorded is the first frame after a
  resize). Under the picture: whether the unchanged frame is exactly what the game drew, and what moved.

If the bench program is older than the HUD code it is rebuilt first (about half a minute, the window says "building";
a failed build shows its last lines there). If the shortcut is missing or the FLvibed folder moved, it is made again by
`tools\hudbench\app\make_shortcut.ps1` (for the lead or an installer; the owner never runs it).

## For tests and agents: the command line

In PowerShell, from the FLvibed folder:

```
tools\hudbench\show.ps1 kbm_cruise
```

That draws the recorded state `kbm_cruise` (keyboard and mouse, cruising) at 1280 x 800 and says whether the bench drew
exactly what the game drew. Other choices:

- `tools\hudbench\show.ps1 -List` names every recorded state (25 states, each in the pad form `pad_...` and the keyboard
  and mouse form `kbm_...`).
- `-Size 3840x2160` draws at that screen size: the 4K recording when there is one, else the 1280 x 800 recording drawn
  again at that size (`-Record 1280x800` picks the recording); `-Frame 1` the second recorded frame.

It writes into `build\bench\show\<folder>\` (the command prints the folder): the bench's drawing (`...bench.fldl.json`),
the game's drawing of the same frame (`game_....fldl.json`) and the picture sheet both draw with (`replay_atlas.png` and
`.json`).

**To look at the picture:** open `tools\hudbench\web\index.html` in a browser, press **Folder** and pick
`build\bench\show`. Every result is listed by its folder; press **A** beside one file and **B** beside another (each
draws with its own folder's picture sheet). **Flip A / B** (or the F key) switches between the two; "A | B" puts them
side by side; "Difference" shows where they differ (nothing, when the command said EQUAL). Dropping files on the page
still works as before.

## Change a setting

```
tools\hudbench\show.ps1 pad_combat -Set hud_scale=130
```

draws the recorded state twice, unchanged and with HUD size 130 %, into one folder (`...unchanged@1280x800...` and
`...hud_scale-130@1280x800...`), and says first that the unchanged frame is still EQUAL to the game's, then what changed
and where (per layer: how many corners of the drawing moved, and the box they lie in). Settings by the names a record
writes them (`modules\hud\src\hud2_state_io.h`): `hud_scale`, `hud_preset` (0 Standard, 1 Focus), `hud_fade`,
`hud_opacity`, `text_size`, `hud_threats`, `glyph_look`, `family=playstation`, `rows.shield.on=0`, ... A name sets every
copy that ends in it (`hud_scale` sets the frame's two); `hud_size=small|normal|large` resolves the size choice as the
game does at the height drawn. Several: `-Set hud_preset=1,hud_scale=80`. A value no player can choose in the options
page (`hud_preset=2`) is drawn and said ("OUTSIDE"). `hud_style` and `hud_transition` act before the recorded state
starts, so they change nothing here (said).

## Change a layout number

The layout presets' numbers (`core\ui_meta.h`, generated from `docs\UI_SPEC.json` "hud2_presets": where a panel sits,
how tall it is) can be changed for one run, with no rebuild, in a small file:

```json
{
  "_about": "a note for the reader (a name starting with _ is skipped)",
  "standard.combat.loadout.h": 0.12
}
```

Each name is `<preset>.<context>.<element>.<key>`; `build\bench\flbench.exe layout-names` lists all 341 with their
values. A name the tables do not hold is refused.

**Worked example, a crowded state.** The fight on the pad (`pad_combat`: target card, threat list, ship panel, loadout,
context prompts, an alert) with Standard's loadout panel lowered from 0.15 to 0.12 of the screen's height
(`tools\hudbench\variants\example.layout.json` holds exactly that):

```
tools\hudbench\show.ps1 pad_combat -Layout tools\hudbench\variants\example.layout.json
```

It prints that the unchanged frame is EQUAL to the game's, `layout standard.combat.loadout.h 0.15 -> 0.12`, and that
what changed lies in the bottom right only (x 937..1265, y 530..778 at 1280 x 800: the loadout panel, its icons and its
words; nothing elsewhere moved). Open the page, Folder, `build\bench\show`, A on the `.unchanged@` file, B on the
`.example@` file, and flip. Add `-Size 3840x2160` to see the same change at 4K.

To try another number: copy the example file, change or add a line, run the command with `-Layout <your file>`. If the
report says "changed: NOTHING", the number is not read in that state (or not at all, see below).

**Which numbers take effect.** Today's code reads these by name (the rest of the tables are the spec's and the lint's):
Standard: `combat.loadout.h`, `calm.context.y`. Focus: `calm.card.w`, `calm.card.h_on_screen` / `h_off_screen`,
`combat.card.h_on_screen` / `h_off_screen`, `calm.nav.w_max`, `calm.nav.w_max_4_3`, `calm.nav.h`, `calm.nav.text_h`,
`calm.hints.text_h`, `calm.hold.y`, `combat.ship.row_h`, `combat.loadout.h`, `combat.loadout.icon`, `calm.context.y`,
`calm.context.pills_max`, `combat.context.pills_max`, `reticle.reticle.*` (band_dy, the arcs and chevrons, the speed
read-out's timing) and Focus's reveal timings `bundle.new.*` (`modules\hud\src\hud2_layout.h` FocusCorners, ComputeWith,
RevealRulesOf; `hud2_frame.cpp` Draw; `hud2_parts.cpp` FocusNum). Speedrun and Guided draw as Standard (not built).
**Not reachable without a rebuild:** the numbers written in the code: `hud2_layout.h`'s `std_ov::` and `focus_ns::`
constants (the ship panel's and the loadout's widths, the gaps, the cross), its `k...` constants, the margins and
aspect factors in `core\ui_meta.h` (`kMarginSideW`, `kAspectK16x10`, ...), and every literal in `hud2_parts.cpp`. Change
those in the source and run the same command (it rebuilds the bench: a few seconds).

## Another screen size

`-Size WxH` with no recording of that size draws the 1280 x 800 recording at that size (`-Size 3840x2160 -Record
1280x800` does it at 4K too). The report names what cannot be true at another size: the text is measured anew, the
values the recording carries in its own pixels (the nav's last drawn edge, the notice and the alerts in travel, the
cursor, the target's mark and the targeting display's places) are scaled by the size ratio, so it is the first frame
after a resize; the panels' content eases snap to their targets; the hotspots are the bench's, not the game's; what the
game's model refresh decided from its screen (the list's row count) is the recording's; `hud_size` stays as the
recording resolved it unless `-Set hud_size=...` is given. The game's 3D views are holes, drawn hatched on the page.

## Two runs of your own

Each run goes into its own folder under `build\bench\show\` (named after the state, the size and the change); the page
lists them all, so a list from one run can go into A and one from another run into B.

What its first line means:

- `EQUAL ... (bit for bit)`: every number of every vertex is the game's.
- `EQUAL ...` without it: equal within the design's tolerance (0.01 px); the report names the largest move (about
  0.00003 px in the autopilot and dock states: the bench and the game round one float differently).
- `DIFFERS`, `FONT` or `FAILED`: the bench did not draw what the game drew, its font is not the game's, or the record
  could not be replayed; the lines under it say where.

## What the bench does (B5a)

A recorded frame (`tests\bench_record.ps1` makes them in a game; the format is `FORMAT.md` "Written by the game") holds
what HUD 2's two halves took in that frame: the inputs and the state for Step (the layout, the fades) and for Draw (the
drawing), and every answer the game gave them while they ran, in the order they asked (clocks, the typing state, the
hotspot under the cursor). `flbench replay` (`replay.cpp`):

1. runs Step from the recorded state with the recorded inputs, answering each question from the recording in order;
2. draws the under pass (the dark backing under the game's 3D views) when the game drew one;
3. runs Draw from the state the game's Draw took, into a pass opened at the game's size and text size;
4. writes the two passes as a display list with the game's own writer, and compares it with the game's list.

A recording whose answers run out, or that has answers left over, fails the replay: the code asked the host something
the game's code did not, or did not ask what it did. That catches a change of the code since the recording.

The comparison (level 1 of the design): per layer the vertex counts at each cut mark, each vertex's texture, colour and
place (within 0.01 px), each layer's fingerprint, each font's fingerprint, the hotspots Draw gave, and Step's results
(the 3D views' rectangles, and the state it left against the state the game's Draw took).

## What it does not draw (the holes)

The bench draws only HUD 2's own parts. Not drawn, never counted as a difference: the comm caption (the game's window
text over a talking contact, drawn by the game's side after the parts' mark `parts.end`; none of the recorded states
draws one), the game's 3D views of the target and the player's ship, the radiation art, the comm portrait, the game's
own target bracket, the pad's prompt bar (another module's pass) and the subtitles' panels.

## Other commands

- `tests\bench_level1.ps1` replays every frame of the fixture recordings (515 frames, no game; 12 of them with the under
  pass) and fails on any difference: the bench's test.
- `build\bench\flbench.exe replay --install <game folder> [--out <folder>] [--brief] <record or folder>...`: the replay
  itself. Exit codes: 0 equal; 1 an error (arguments, files); 2 the install refused (no game font); 3 a font differs
  from the game's; 4 a list, a hotspot or Step's state differs; 5 a replay failed.
- the same with `--set <name>=<value>` (repeatable), `--layout <file>`, `--size WxH`, `--name <word>`: a variant
  (`replay.h` ReplayVariant): per record the unchanged frame and the changed one, the report `variant.txt`. Exit 0 drawn;
  4 the unchanged frame differs from the game's (the recording is older than the code: record again); 5 an override
  cannot apply or a replay failed. With a change, the frame's clock questions are answered leniently (the changed code
  may ask more or fewer: the report counts them).
- `build\bench\flbench.exe diff <a.fldl.json> <b.fldl.json>`: two lists of one size compared as sets of corners (place,
  colour, picture): what is only in one, per layer, and where. Exit 0 the same picture, 4 not.
- `build\bench\flbench.exe layout-names`: every layout number a layout file can name.
- `build\bench\flbench.exe serve [--parent <pid>] [--portfile <file>] [--port <n>]`: the window's server (`serve.cpp`):
  127.0.0.1 only, a port the system picks, the page `web\bench.html` and its requests (`/api/records`, `/api/settings`,
  `/api/layout`, `/api/draw` with the lines `rec`, `size`, `set`, `layout`, `/api/variants`, `/api/save`, `/api/ping`, `/api/bye`)
  answered in memory, one draw in about 0.1-0.4 s. It ends by itself, whichever comes first, when the process `--parent`
  ends, the window's process (its pid in `<portfile>.window`) ends, the page said goodbye on closing (3 s, so a reload
  survives) or has not pinged for 15 s (it pings every 3 s). The launcher `app\bench_app.ps1` (run by the shortcut through
  `conhost --headless`, so no console shows) builds when stale, starts it and opens the window in the default browser
  (`app\browser.ps1`; none found: the first installed of Chrome, Brave, Vivaldi, Firefox, Edge): a Chromium browser in app
  mode with a profile of its own per start (`build\bench\app\browser_<pid>`), any other browser as a tab; when the window
  is gone it ends the server and every process started with that profile (by its command line, whatever the browser) and
  deletes the profile. Saved variants: `research\experience\bench\variants\<name>.variant.txt` (the same lines).
- `build\bench\flbench.exe card ...`: the kit's test card (B2; `bench.cpp`).

## How the layout numbers reach the frame

HUD 2's frame reads a preset's data through its host (`modules\hud\src\hud2_frame.h` FrameHost `preset`, `PresetOf`):
the game never sets it and reads the spec's tables as before; the bench gives copies with the run's numbers. That one
member is the bench's only change to the game DLL's sources.

## Not yet (the design's later packages)

Watching files and redrawing on a change (B5b), settling a resized frame over several frames (B5b), the docked pages (B9), pixel-level proof against the game's screen (B6b).

## Files

| File | What |
|---|---|
| `bench.cpp` | the program `flbench` (`build.cmd bench` -> `build\bench\flbench.exe`) |
| `replay.*` | a recorded frame replayed and compared (B5a) |
| `kitlist.*`, `fldl_write.*` | the draw kit's list back end; the list, atlas and fingerprint writers |
| `FORMAT.md` | the display list's format |
| `record_check.py` | checks a recording's files against each other (B4b) |
| `show.ps1` | the command line for tests and agents |
| `serve.cpp` | `flbench serve`: the window's server |
| `app\` | `bench_app.ps1` the launcher the shortcut runs; `make_shortcut.ps1` makes the shortcut `HUD 2 Bench.lnk` |
| `web\bench.html`, `bench.js`, `bench.css` | the bench's window (it draws with the replayer's `fldl.js` and `gl.js`) |
| `variants\` | layout files (`<name>.layout.json`); `example.layout.json` the worked example |
| `web\` | the replayer page; `synth_fixture.js`, `test_replayer.js` its tests |
