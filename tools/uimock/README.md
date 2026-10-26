# The living spec of FLvibed (standing order, the owner, 25 Oct 2026)

The page (claude.ai artifact `4v4dXbB5GTVt1URowFtSvN`) is **the living spec of FLvibed as a whole**: every feature
FLvibed adds, how a player installs and uses it, and what is not implemented yet. It is also **the source of the UI
spec**: every UI change (a row, a binding, a screen's prompts or badges, a glyph, a state) is made here first or at the
latest in the same commit, and `docs\UI_SPEC.json` is regenerated from it.

**Standing rules (the owner, 25 Oct 2026):**
- **Every user-visible change updates the page in the same work package** (a feature, a setting, a default, a fix that
  changes what the player sees or can do, a status change): its section's text, captures and tables, and the "Not
  implemented yet" list (an item built or closed leaves it in the same change). Never let the page drift from what is
  built; no text may describe a superseded state.
- **The install / requirements section is updated whenever a requirement or compatibility fact changes** (supported
  exe / HDE version, plugin dependencies, shipped files, install steps, a compatibility test), each line marked
  verified or not with its source.
- **Always functional and fully representative**: every control on the page works (the nav, the Input switch, the
  comparison sliders, the layout tabs, the Try-the-page / Bindings / Edit / Advanced / Export / Import / Reset buttons,
  the expanders) and every image loads. Before every publish, test every interactive control once in a local browser
  (serve the build folder, e.g. `python -m http.server`, and script the clicks; see the commit of 25 Oct 2026 evening).
  The **Input switch** must change what the page shows: per section a device note (`dv`), the captures where both
  exist (`km` variants), glyphs vs keycaps in the tables; a section the device does not change says so.

**Read-only by default (the owner, 25 Oct 2026: shareable with players and third parties; since v19):** the page
decides the mode from the platform at load (`resolveMode` in `part3b_editor.js`): the `user` capability's
`isOwner()` / `canEdit()` true and `can("data.write")` not false = the editor view (today's full editing, plus a small
"Edit: on / off" toolbar toggle for the owner to preview what others see, remembered per browser); anything else - a
viewer, signed out, no `window.claude` (the static copy in `docs\spec\`, a copy hosted elsewhere, the local test server; the `RUNTIME` check), any doubt - is
read-only, which is also what renders before the answer arrives. Read-only: no Edit mode, Export / Import / Reset, status
line, notes box or binding selects (plain text instead; the layouts as buttons); values from `ui/spec` when readable
(else the built-in data), shown as "current" beside the default; the Options / Bindings previews work but write nothing
(no db, no browser storage) and revert on returning to the document. Owner- or database-only text carries
`class="edonly"` (`roonly` for the reverse); keep every new mention of editing, saving or the store inside one. Test both
paths before publishing: the plain build = read-only; a copy with a stub `window.claude` (`user` true / false + an
in-memory `db`) = the owner and a db-reading viewer. Never leave such a stub copy in the build folder (it is the
publish root): on 26 Oct 2026 they were written there as `index_owner.html` / `index_viewer.html`, served by the
environment's `.claude\launch.json` entry `spec-local` (the project venv's `http.server` on 127.0.0.1:8790; images are
lazy-loaded, so check them by fetching every referenced path) and deleted after the test.

**Structure (since 25 Oct 2026, late evening; "With FLUF" added 26 Oct 2026):** fourteen sections in a sticky nav -
Overview (FLvibed at a glance), Install (requirements and compatibility), With FLUF (what FLvibed does beside FLUF, the
recorded conflicts), Flight & aim, Flight HUD 2, Classic HUD, Hit effects (the HUD cues), Menus and
bases (Extended HUD 2), Controls, Subtitles & movies, Autopilot (WIP), Options page, Speedrun & New Game, Not
implemented yet - each with a plain-language lead, a device note, real release-build captures (drag sliders with their
own labels, galleries), live tables of the spec's settings rows and pad bindings, and an expandable "For developers"
block (rules, sizes in fractions of H, settings keys, files, docs, sources). The interactive Options page (main menu / F1) and the Bindings diagram stay
as the editable views ("Try the page", "Open the interactive bindings diagram"). The drawn Screens, Maneuver bar and
Decisions views are gone from the page; `screens`, `bar` and `decisions` stay in the spec data (`ui/spec`,
`docs\UI_SPEC.json`) as the history and are saved back untouched. Sections link as `#flight`, `#screens`, ...

| Piece | Where |
|---|---|
| Page source | `part1_head.html` (CSS, header, section nav, the interactive views' toolbar), `part2_data.js` (the spec: tabs, rows, presets, glyph families, bindings, screens, decisions), `part2b_doc.js` (the document: sections, leads, device notes, comparisons, galleries, status tables, developer blocks; static, reviewed against the docs and the code), `part3a_render.js` (dispatcher, options screen, bindings diagram), `part3c_doc.js` (the document renderer: sliders, galleries, the Input switch's variants, settings / bindings tables from the live spec), `part3b_editor.js` (editor drawer, notes, persistence, toolbar) |
| Build | `build.ps1` concatenates the parts into `index.html` beside the images and syntax-checks the script |
| Images | `research\evidence\controller_ui\mockup\img\` (git-ignored): the Options backgrounds and control sprites (`assets.py`, from `research\experience\ui_2026-10-25\`) and `img\cap\` = the real captures as 1600-px JPEGs written by `spec_images.py` (flight: `runs\hud2_fix2_2026-10-25\final_*.png` + classic counterparts; screens: `runs\hud2x_final_2026-10-25\` before_ / final_ pairs + aspect frames; cues and aim: `runs\spec_flvibed_2026-10-25\`; subtitles, autopilot, timer, glyph look: earlier runs, listed in the script). New captures: rerun it, keep the page names |
| Publish | the Artifact tool with `url` = the artifact, `root` = the build folder, `files` = the changed `img/**` files (files left out are kept); capabilities `db` + `user` are stored on the artifact and carry forward |
| The owner's edits | the artifact's database document `ui/spec` (ArtifactData get, collection `ui`, doc `spec`; version 122 after `glyph_look`); patch it with a pinned `set` (`if_version`) after reading, never overwrite blindly (`specpatch.py`: `add_rows` first / end / `after` a row key); `docs\UI_SPEC.json` = that document's `spec` with an `_about` line; then `tools\ui_meta.py` for `core\ui_meta.h` |
| Evidence copy | `research\evidence\controller_ui\mockup_v4_index.html` (the last built page: FLvibed's living spec; `mockup_v3_index.html` = the HUD 2 spec, `mockup_v2_index.html` = the mock-up before the restructure) |
| Static copy (repository) | `docs\spec\` = `index.html` + `img\` (WebP captures, at most 1280 px wide; ~7.5 MB), written by `export.ps1` (PowerShell tool) from the same parts + `docs\UI_SPEC.json` (the owner's values, carried as a snapshot): **always read-only** (`window.FLV_STATIC_COPY` and no `window.claude` runtime -> `RUNTIME` false in `part3b_editor.js`: no editor, no store, no browser storage), no network (the Google Fonts link dropped), a banner naming it a static copy. For readers of the repository (GitHub), who cannot open the artifact. `export.ps1 -Check` = `drift_check`'s `spec-copy` line |

Rules for a change: edit the part files here (a built behaviour: the `part2b_doc.js` text and, if a setting or binding
changed, `part2_data.js` + `ui/spec`), build, test every control locally (above) (the `db` store and the `user`
capability exist only inside the claude.ai viewer; locally the page is read-only), publish to the same artifact, patch
`ui/spec` the same way the data changed (the owner's values and decisions must survive), copy the page to the evidence
folder, regenerate `docs\UI_SPEC.json` and `core\ui_meta.h`, commit the parts and the spec together.

**Sync rule for the static copy (25 Oct 2026):** every spec build also refreshes `docs\spec\` - run `export.ps1` after
`build.ps1` and after `docs\UI_SPEC.json` is regenerated (it builds the parts itself and converts only changed images) -
and `docs\spec\` is committed together with the parts, the spec and the artifact publish, so the repository's copy is
never older than the live page. `tools\drift_check.ps1` flags a `docs\spec\index.html` that differs from a fresh export
of the parts (or images that differ from / are newer than the source images).

## Pixel mock-ups on game frames (`hudmock.py`, `hudmock_variants.py`; 25 Oct 2026)
For HUD proposals judged at full resolution against the game: `hudmock.py` draws in the HUD's own language (the
panel glass and rim, the cyan foot bar, Agency FB, the glyph families, the HUD's icons lifted off a frame) onto 4K
captures made with the pad layer off (debug `padhud off`); `hudmock_variants.py` renders the pad HUD round 2's
directions A / B / C in two situations plus a glyph style sheet into `research\evidence\controller_ui\hud_redesign\`
(the proposal page: `page\`, artifact `Mh1UW7KC8QtSEGFNKa23h6`). Run with the Python venv (`-I`). A proposal the owner
accepts goes into the living mock-up and the spec as any UI change does.
`hud2.py` + `hud2_states.py` (HUD 2, `docs\UI_HUD2.md`): the ground-up pad HUD's component kit (segmented bars and
arcs, the game's flat icons / brackets / arrow / lead art from `hud_redesign\fl_assets\`, plates, outlined text) and its
states (combat, travel, cruise, dock, lane) on frames captured with the whole HUD hidden; renders in
`hud_redesign\hud2\`, the page in `hud_redesign\hud2_page\` (artifact `D6XnaBvWLKHARfdvTQuj3Y`).
