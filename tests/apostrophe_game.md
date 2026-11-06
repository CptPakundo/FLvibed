# In-game test: the apostrophe in the weapon group window (GitHub issue 37)

**Run in part, 6 Nov 2026** (`research\experience\runs\apostrophe_p3`): rows 1, 3 and 4 at 1280x800 and 4K with a Death's Hand Mk II put into a copy of SaveC702 (`text_size` stops at 150). Not run: row 2, the capitals, a Latin Extended-A name, and the legs below. Reach: every atlas FLvibed draws from. Hence the one
window that failed, then a glance at the others that share the row kit.

**Setup.** Build this branch (`dev`), `tools\deploy.ps1 -Background -Instance <n>`, muted, HDE 0.7.1, 1280x800 as in the report. In PowerShell:
`. .\tools\harness\flctl.ps1`, `$env:FLVIBED_INSTANCE = '<n>'`.

**Fixture.** A free-roam save docked at `Li01_15_base`, `Li04_06_base` or `Iw02_03_base`, whose dealers sell Death's Hand Mk II (`fc_or_gun01_mark02`, ids 263221,
`market_misc.ini`; rank 16, so a late save with the credits). Buy one, mount it, launch. No such save at hand: Thor's Hammer or Gaia's Savior in a loadout does as well.

**Steps.**
1. `Send-FL 'wgroups open'`, then `Send-FL 'wgroups state'`: the gun's row is listed.
2. Take a frame, full resolution, and crop the panel at the right.

**Read from the frame.**
| # | Where | Expect |
|---|---|---|
| 1 | The row of the gun | DEATH'S HAND MK II with an apostrophe, no `?`, no empty box; the text not cut, the panel as wide as the name |
| 2 | Picker (`picker open`) on a ship or base whose name holds an apostrophe or accent | the same |
| 3 | `FLController.log` | no `render: U+2019 not in the atlas`; `uikit: fonts` once at the start, no rebuild each frame; no `fonts FAILED` |
| 4 | 4K at `text_size` 200, options page open | the fonts build: `render: font ... does not fit the atlas` absent |

Also the case a mod brings: a name with a Latin Extended-A letter (U+010D) on Agency FB shows the plain letter (c), not a box.

**Capitals (`str::Upper`).** Reach: every all-caps name in HUD 2, the strip, the pad screens, the tractor's reason, and subtitle sentence starts. Fly to Grafenwöhr
(Rheinland) or target a base with an umlaut: the target card and the strip read GRAFENWÖHR with a capital umlaut, Großglockner reads GROSSGLOCKNER, no box, no `?`,
the HUD's per-frame cost unchanged (ASCII names never reach the API). `hud2 lint` (hud2_lint_live) reports no new text mismatch.

**Mission ships by name (`str::SameText`).** Reach: only mission ships whose `individual_name` is not ASCII, found by the name text when their name id did not match. M08: `Transport1` and
`Transport2`, both "Tekagi's Transport" (216803): `Act_MarkObj = Transport1`, `Cnd_Destroyed`. Two ships, one name: the look takes the first match, the leash the last, so read which one it
takes. M07: the cruiser RNC Saarbruecken at the Tau-31 gate (216709): `GotoShip`, `Cnd_Destroyed`. M10: `new_berlin_rh_cruiser_3 / _8 / _10 / _p_2`, `frankfurt_rh_cruiser_7` (217070, 217075, 217077,
217067, 217085): named wing members; no leash or objective found on them by a text search of `m10.ini`. Run the legs M07, M08, M10 (`flleg.ps1`), compare with their best in `legs.tsv`: same route, no time lost, and in M08 the
autopilot heads to the marked transport. Before the fix these lookups found nothing and the fallback chose; the fallback's ship and the named one may now differ.

**Then** the release build, and lift "unproven" from the CHANGELOG line with the frame's folder.
