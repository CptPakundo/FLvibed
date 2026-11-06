# 0.4.0, phase 2: what a running game must show

**Start only on the driver's word that the games are free.** Everything here runs one background game on one instance, muted, never the owner's screen. `run.ps1` is the driver of it; `frames.py` reads its frames. Phase 2 ran on instance 7 on 6 Nov 2026: what it showed is `docs\RELEASE_0.4.0.md`, "Checked for this build" and "Known limits".

**First, once:** the worktree has no `Install\`. Link it as `fs-10ms`'s worktree does (a junction `FLvibed-040\Install` to `..\FLvibed\Install`, `research\saves` likewise; instance 7 unless the driver names another, never `v11nightly`). `tools\buildlock.ps1 release` builds; `tools\deploy.ps1 -NoBuild -Release -Instance 7` carries the DLL into that instance alone. `tools\deploy.ps1` needs the `first_card` stamp it now writes, or a fresh start stalls on the card.

Run: `tests\hud040\run.ps1 -Label p2a -Only sizes` (PowerShell tool, in the background, one section at a time; read each before the next). Frames: `research\experience\runs\hud040_<Label>\`. `# budget: hud040 n/N` goes on every launch.

| # | Section | What runs | Pass | Games, minutes |
|---|---|---|---|---|
| 1 | `sizes` | `hud_size` 1-3 at 4K and 1280x800, 13 sims through `hud2 lint` | 0 FAIL; steady under budget; Small at 800 equal to Normal; the log's `hud_size:` line right on this machine | 2, 25 |
| 2 | `first` | A load into space, three times, 240 frames of the top strip | `frames.py native` names no frame with the native HUD | 1, 5 |
| 3 | `alerts` | `SaveG004` in the radiation zone, map, inventory, pause | A1: `radiation=1` in 70 of 80 samples through the blink. A2: the art steady at the line, gone in every window. Issue 27 gone. `hud2 tree roots`: each unknown name gets a row | 2, 10 |
| 4 | `form` | T1, T2, T4, T5: form against bracket, records against projection, a load | T1: corners gone, target, click, RS pick, sound, lead, arrows as before. T2: label and bars gone, the bracket back after `hud2 form off` across a load. T4: within 2 px, also in turret and rear view (by hand). T5: a capital, a planet: frames read | 2, 15 |
| 5 | `motion` | The clip on real motion: consecutive frames at every 4th pixel, about 190 under the grab's cap, T3, T8 from the log over 60 s | run-judge: no flicker at the seat's edge, the mark on the ship under 1 px or a measured lag; sets `kSeatEnter`, `kSeatLeave`, `kSideDead`, `kEdgeWaitSec` | 1, 10 |
| 6 | `flash` | `hud2 sim critical`, 480 frames, `frames.py rate` | strongest under 3 Hz, energy above 3 Hz under a quarter of it; `skin_motion 2` steady; `hull_cue 0`, `shield_cue 0` no breath | 1, 5 |
| 7 | `cue` | All states on the row, a real lock sought | lint 0 FAIL; the tick 2 px at 800 reads, no shimmer; W6: a lock seen, or the row stays unproven and says so | 2, 10 |
| 8 | `menus` | Options' five tabs, `uiskin seams` per window | seam list empty or each line owned; no word cut at 800 | 2, 15 |
| 8a | `dock` | A skipped dock at Baltimore, a frame a second for 8 s | The green line (`NN_ArrivalText`, a type-on) reaches its whole text and goes: if it stands cut, fix it. A space comm does not stand on in the base past its voice: if it does, end it at the dock | 1, 5 |
| 9 | `captures` | 4K and 1280x800 for the spec and the owner | read at full resolution before anyone calls them good | 2, 5 |
| 10 | `fingerprint` | `tools\fingerprint.ps1 -Label p40 -Live`, `-Compare p40 -Against p11e2` | every difference named in `UI_HUD2_BUILD_040.md`; perf only from a quiet machine, flight within 5 %, skinned screens under 150 us, the map under 300 us (`UI_HUD2_SKIN.md` 10) | 1, 10 |
| 11 | sections | `tools\harness\flsweep.ps1 -Only` the sections that read HUD 2: on the driver's word; a full sweep is the owner's | as each section | n, 30-60 |
| 12 | Module matrix | `tests\page_modules.ps1`, `tests\page_no_controller.ps1`, `tests\window_modules.ps1`, `[Modules]` hud 0 | off states valid, HUD 2 off frames equal classic (`tests\pixels.ps1 -Classic`, `tests\pixels_compare.py` against main's build) | 6, 20 |

**Budget:** sections 1-10, 16 games, about 110 minutes of background time; 11 and 12 on the driver's word, up to 80 more. A section that fails stops there: read the evidence, fix, rerun that section alone (one retry each, then report). Moments of `UI_HUD2_SKIN.md` 3.1 not reached by these (Alt-Tab, device Reset, mission failure, a story cutscene) are a section of their own once 1-9 stand.

**Not in the scripts, by hand or by another session:** T6 (other plugins on 0x4EF030, 0x421CC0: a second install pass, on the owner's word), T7 (longest names at 0.024 H), a Proton run, 4:3 and 21:9 frames (add them to `-Sizes`), a real pad on the owner's prompt. The other workers' checks are listed in `modules\hud\docs\UI_HUD2_BUILD_040.md`.
