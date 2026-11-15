# The HUD bench's display list, format v0

**v0, written by the bench since B2.** The writer is `fldl_write.cpp` (the kit's list back end `kitlist.cpp`, the program
`bench.cpp`: `build.cmd bench` -> `build\bench\flbench.exe card ...`); the replayer (`web\`) reads it. B2 settled points
1-6 and 11 of the list below and the replayer has replayed B2's test card at 1280x800 and 3840x2160 (synthetic and
with the game's font). It is not frozen as v1 yet: the text runs (point 7) change the list's shape and wait for B4b's
recording; v1 then. The design behind it is the bench design (audit 71b, section 4); the kit it mirrors is
`core\ui\hud2_kit.cpp` (`LayerId`, `Push`, `Glyphs`, `End`, `Mark`).

## Files
| File | What | Git |
|---|---|---|
| `<state>@<W>x<H>[.<variant>].fldl.json` | one display list (one frame of one state at one size) | never, except unit fixtures in the export-ignored private folder |
| `<name>.json` + its PNG | an atlas: a manifest (`flatlas/0`) naming rectangles in one RGBA PNG | never (the bench's cache under the git-ignored build folder) |

The replayer refuses a list or atlas that breaks any rule below (fail closed: one error refuses the file, it is never
drawn in part). Unknown top-level keys and unknown hole kinds are warnings.

## Decisions (all P)
- **Text, not bytes: UTF-8 JSON, one object per file.** The writer needs no library (numbers and strings by `printf`),
  the browser parses it natively, a diff tool reads it. Size: about 40 bytes per vertex; a busy HUD frame of ~5,000
  vertices is ~200 KB. A packed binary form waits for a measured need.
- **Numbers:** positions and texture coordinates as decimal floats written to round-trip a 32-bit float (`%.9g`);
  colours as unsigned 32-bit integers in D3DCOLOR order `0xAARRGGBB` (the kit's `Vertex::color`), written in decimal.
- **Coordinates:** back-buffer pixels of the list's `size`, origin top-left, y down: exactly the kit's pre-transformed
  vertices (`XYZRHW`), including the kit's `-0.5` (`Push`). The replayer adds 0.5 (GL's pixel centres sit on halves,
  D3D's on integers), so a quad the kit placed on pixel edges lands on the same pixel edges. It draws into an offscreen
  target whose row 0 is the list's top row, so an edge through a pixel centre falls as D3D's top-left rule has it (left
  column and top row in, right column and bottom row out; drawing y-flipped put the row tie the wrong way), then copies
  the target 1:1 onto the canvas. Measured: axis-aligned ties exact on ANGLE D3D11 and SwiftShader; SwiftShader still
  differs from a D3D reference by a few pixels along diagonal edges (the independent check, 12 Nov 2026).
- **Alpha:** the replay target keeps an alpha channel, cleared to 0; the `under` pass writes none (as the kit's
  COLORWRITEENABLE 0x7), every other draw blends alpha with the same factors as colour (D3D8 has no separate alpha
  blend). The canvas shows colour only; the headless check reads the target's alpha.
- **Triangles:** triangle lists, three vertices per triangle, as the kit's `DrawPrimitiveUP(TRIANGLELIST)`.
- **Textures by name:** a list-level `textures` table of names; each layer's vertices are cut into `spans` that name one
  table entry each. Names: `white` (the shape texel, exactly 1.0), `glyph:<set>:U+XXXX` (a character of a font set, the
  code point in hex), `icon:<name>` (an icon cell). A glyph's or icon's u, v are **relative to its own box** (0..1 over
  the entry's rectangle), so the marquee clip's cut u (`Glyphs`) survives any atlas packing. The replayer maps them
  into the atlas it is given; a name the atlas lacks draws as a solid box in the vertex colour and is reported.
- **No scissor primitive:** the kit clips on the CPU (D3D8 has no scissor rect; the marquee room cuts the quad and its
  u). A run records its room as `clip` for inspection only.

## The list
```
{ "schema": "fldl/0",
  "privacy": "public" | <the private marker>,       // the marker: tools\private_check.py's MARKER; never in a public file
  "build": {"game": "<build id>", "bench": "<build id>"},
  "size": [W, H],
  "state": "<state name>", "variant": "<name or empty>", "relayout": false,   // true: drawn at another size than recorded
  "sets": [{"name": "label", "px": 26, ...}],        // the font sets used (fingerprint fields: B1a / B2)
  "textures": ["white", "glyph:label:U+0041", "icon:shield", ...],
  "passes": {
    "under": {"writeAlpha": false, "layers": [ six layers ]},   // the view backings; RGB writes only (COLORWRITEENABLE 0x7)
    "over":  {"writeAlpha": true,  "layers": [ six layers ]} },
  "marks": [{"name": "parts.end", "pass": "over", "counts": [six vertex counts]}, ...],
  "runs":  [{"text", "set", "x", "y", "px", "align", "track", "outline", "clip": [x,y,w,h] | null, "color", "tag",
             "pass", "layer", "first", "count"}],   // first / count: the run's vertex range in its layer
  "items": [{"tag", "rect": [x,y,w,h], ...}],       // the kit's recorder (hud2_kit.h Drawn)
  "hot":   [{"name", "rect": [x,y,w,h]}],           // K&M hot-spots
  "holes": [{"kind", "rect": [x,y,w,h], "shown": true}] }
```
A layer: `{"name", "op", "v": [x, y, argb, u, v, ...], "spans": [[first, count, textureIndex], ...]}`. The six layers in
this order, always all six (empty ones with `"v": []`): `shapes`, `iconBig`, `iconSmall`, `textSmall`, `textBig`,
`caption`. Spans cover the layer's vertices in order without a gap, each a positive multiple of three.

**Ops:** `modulate` = texture x vertex colour (rgb and alpha); `icon` = the vertex colour's rgb, alpha = vertex alpha x
texture alpha (the kit's COLOROP SELECTARG2 for the two icon layers). Every layer blends `SRCALPHA, INVSRCALPHA`, add;
no depth, no culling; textures filtered linearly, clamped, no mip maps (`End`). Draw order: `under` then `over`; in each
pass the six layers in order, one draw call each; empty layers drawn not at all.

**Marks** are cut points (`hud2kit::Mark`, B1b): the vertex count of each layer at the mark, in order within a pass, so
ranges say which vertices are HUD 2's parts, the comm caption, or anything later added to the same pass.

**Holes** are what the list does not draw (the game draws it): `view.target`, `view.ship`, `radiation.art`,
`comm.portrait`, `game.bracket`, `promptbar`, `subtitles.dialogue`, `subtitles.chat`, `model.slot`. The replayer shows
them as labelled hatched rectangles over the replay, never inside it.

## The atlas
```
{ "schema": "flatlas/0", "privacy": "public" | <marker>, "image": "<file>.png", "size": [w, h],
  "entries": {"glyph:label:U+0041": [x, y, w, h], "icon:shield": [x, y, w, h], ...} }
```
The PNG is RGBA, straight (not premultiplied) alpha; the page decodes it with no colour conversion. `white` is never
an entry (the replayer has it).

## Settled by B2 (12 Nov 2026)
1. **Box-relative UVs, named by the box sampled.** The kit's vertices carry its atlases' u, v; the writer names each
   triangle's texture by the box its texture coordinates' centre falls in (a glyph's inked box, an icon cell, a font's
   4 x 4 white block = `white`) and rewrites u, v relative to that box. A triangle that samples no box, or has a corner
   outside its box, fails the whole list (nothing is written in part). So the kit needs no side list of names and the
   game pays nothing for the bench.
2. **The boxes and the padding.** A glyph entry is its inked box exactly as `core\game\fontraster` packs it (`w x h`
   texels, the box `RasterGlyph` gives); an icon entry is its whole cell (128 or 64 texels; the kit's UVs sit half a
   texel in, `flicons::CellUv`). The bench's atlas (`card_atlas.png`) copies every box with its own texels and leaves
   two empty texels round each, as the game's atlases do between boxes, so linear filtering at a box's edge reads what
   it reads in the game. One atlas holds every set and both icon sizes of a run.
3. **Sets.** A font set is named `<face>-<px>-<weight>` (the face's letters and digits: `AgencyFB-21-700`), `.2`, `.3`,
   ... for a later build of the same three (the kit rebuilds a set when a character is added), so a name always means
   one texture; `glyph:` names need nothing more. `sets` carries `name`, `face`, `source` (1 the install's font files,
   2 Windows' installed face, 0 synthetic), `px`, `weight`, `ascent`, `line`, `glyphs` and `print`: the FNV-1a 64 of
   the set's fingerprint block. The blocks themselves go to `card_fontprint.txt` in `hud2 fontdump`'s format (one
   writer, `render::FontPrint`), so `flbench --prints <a game's fontdump>` compares the bench's raster with the game's
   line by line (exit 3 on a difference; the bench refuses, exit 2, an install whose label face would be the fallback).
4. **Icon names:** the `flicons` enum names (`flicons::kIconNames`) and the cell size: `icon:Cruise.128`,
   `icon:Cruise.64` (the kit picks the atlas by the draw's size, so one list can hold both).
5. **The under pass's alpha:** `writeAlpha` is the pass's own flag (`SetKeepDestAlpha`: colour writes only). In game
   two passes set it: HUD 2's under pass (`modules\hud\src\hud2.cpp`) and Extended HUD 2's glass (`uiskin.cpp`).
6. **Numbers:** positions and box-relative UVs `%.9g`, colours decimal (as v0).
11. **Marks:** `hud2kit::Mark(name)` (B2) stores every layer's count; a pass's Begin clears them; the writer writes the
   marks of both passes with their pass's name. Nothing marks the under pass today.
- **Items** are the kit recorder's (`hud2kit::Record`): `tag`, `rect`, and the bench adds `kind` (the recorder's letter)
  and `text`; the replayer ignores the extra fields. `hot` and `holes` are empty in the card and in a replay's list; a
  variant's list (`flbench replay --set / --layout / --size`, B5a) carries its hotspots (`slot <n>`), the game's 3D views
  as holes (`view.ship`, `view.target`, `shown`), its `variant` name and `relayout` true when drawn at another size.
- **Probes** (`card@<W>x<H>.probes.json`): pixels the replay must show, computed by the bench at the centre of a recorded
  primitive (`test_replayer.js --bench <folder>` replays them in a headless Chromium browser: the default one, else Chrome, Brave, Vivaldi or Edge).

## Written by the game (B4b, 13 Nov 2026)
The test verb `hud2 bench record <n> <folder> [<name>]` (`modules\hud\src\hud2_record.h`; `tests\bench_record.ps1`
records HUD 2's 25 simulated states) writes per frame `<name>_<W>x<H>_f<k>.fldl.json` (no `@`: a name with one reads as
an e-mail address to the commit hook, and a recorded fixture is committed), HUD 2's under and over passes as
the kit submitted them, with the same writer as the bench (`core\ui\hud2_kit_fldl.cpp`), and beside it the frame's
record `<name>_<W>x<H>_f<k>.flstate.json` (`modules\hud\src\hud2_state_io.h` StateDoc: the inputs and states Step and
Draw took, the host's answers on a tape, the kit's text sizes and added characters). What differs from the bench's lists:
- **`record`** (top level, the game's only): `{"file": <the record's file name>, "inputs": <FNV-1a 64 of the record's
  draw_in and render, as the record's writer writes them alone in one object>}`; the record names its list back
  (`list.file`, `list.hash` = the FNV-1a 64 of the list file's bytes, `list.inputs`, each set's print, each layer's
  vertex count and `hud2 kitdump` hash). The project's FNV-1a 64 (`render::Fnv`) starts from 0x14650FB0739D0383 (the
  decimal 1469598103934665603, one digit short of the standard basis): every fingerprint since B1 uses it.
- **Set names** carry no `.n`: one pass holds one build of a face, px and weight, so `AgencyFB-22-700` is unambiguous in
  a game list; its `print` hash says which build. The bench's lists may say `AgencyFB-22-700.2` for the same raster: a
  comparison (B6a) matches sets by face, px, weight and print, not by name.
- **`items` and `hot` are empty**: the kit's recorder belongs to the layout lint in game, and the hotspots are in the
  record (`render.hot`). The comm caption (drawn after the mark `parts.end`, before `comm.end`) is in the list; the bench
  does not draw it (the game's window text: a hole for level 1).

## Replayed by the bench (B5a, 13 Nov 2026)
`flbench replay` (`replay.cpp`, `README.md`) writes `<record>.bench.fldl.json` beside its report: the same writer and
fields as the game's list, `record` naming the record it replayed, `build.bench` its own build. Its passes are copied
before End as the game's recorder copies them (an empty pass is a list with empty layers and its two marks). Level 1
matches font sets by face, px and weight and compares their prints apart (the bench's `.n` names never count).

## Still open
7. **Runs:** nested text the recorder skips (`hud2_kit.cpp` around `:741`, `:847`) and the outline's four copies: one run
   or five; whether `x, y` is the pen origin or the box. The card writes no `runs` (the kit's TextDraw does not report
   its vertex ranges yet): B4b, then v1.
8. **Sequences:** one file per frame (v0) or a clip of frames with shared textures (B5b).
9. **Size:** whether ~200 KB a frame is fine for watch mode, or a binary form is wanted (measure in B5b).
10. **The background:** whether the list names its capture (`under.png` of level 2) or the page keeps choosing it.
11. **Marks in the under pass:** none planned; confirm.
12. **The back buffer's alpha in game** before the HUD draws (the replayer starts from 0): matters only if level 2
    ever compares alpha; PrintWindow captures carry none.
13. **Which rasteriser level 2 replays on:** SwiftShader is GPU-independent but a few pixels off D3D along diagonal
    edges; ANGLE D3D11 matched the reference exactly in the check. Decide with B6b's first comparison.

## Testing it
`tools\hudbench\synth_fixture.js` writes a made-up list at each preset size (every pass, layer, op, blend, a triangle,
a per-vertex gradient, the clip, a name the atlas lacks, marks, runs, hot-spots, items and every hole kind; block shapes
for glyphs, no game text, art or metrics) with the pixels each element must produce. `tools\hudbench\test_replayer.js`
checks the reader under Node; with `--headless` it replays the fixture in a headless Chromium browser (SwiftShader WebGL)
at 1280x800, 1920x1080 and 3840x2160 and checks every probe pixel.
