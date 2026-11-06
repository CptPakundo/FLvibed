# FLvibed 0.4.0: release notes

Released 6 Nov 2026 on the owner's word, fast-tracked: the basic checks below, with the full sweep and gate to follow after release, and what they find going in as silent in-place updates. "Checked for this build" says what a game showed and what it did not.

## What it is
- **HUD 2's makeover** (`modules\hud\docs\UI_HUD2_SKIN.md`, built step by step in `modules\hud\docs\UI_HUD2_BUILD_040.md`): every 2D screen in one language, the game's logic left in charge. With HUD 2 off the game stays exactly classic.
- **A new options menu** (`docs\UI_OPTIONS.md`): five tabs by feature, simple mode first, Show all settings for the rest.
- **Haptics on every pad** (`modules\controller\docs\HAPTICS.md`), **unproven on any real pad:** built on the branch `haptics-040`; its readers, gates, lease and `controller = 0` run in game on virtual pads (`tests\haptics_game.ps1`, sweep `pad-haptics`). The waveform stream and the Impulse route's start have never run; nothing has been felt. The DualSense leads (Adaptive Triggers, waveform opt-in behind its probe), then Xbox Impulse Triggers, then two motors. The owner's card: `tests\haptics_owner_card.md`.
- **Everything of 0.3.5rc** (`docs\RELEASE_0.3.5rc.md`): the core with eight modules, native PlayStation and Switch pads, the audit's fixes. There was no 0.3.5: its sweep was skipped on the owner's order and 0.4.0 takes its place.

## GitHub issues this answers
| Issue | What changed |
|---|---|
| 26 | The weapon group cue above the ring |
| 27 | The game's radiation symbol no longer draws over the card, map and pause: an amber nav line with the art beside it |
| 28 | The energy meter drains to the middle |
| 32 | Skip docking cutscenes works on a trip to a station or base, not only a planet |
| 33 | Loot the tractor refuses says why, in the game's own words. Shown in game for full batteries at both sizes and for full nanobots; the hold's and the ammo's cards only by the card test, no fixture ("Checked for this build") |
| 34 | A pad buys a ship: the final stage opens on BUY SHIP, A held buys it, LB / RB reach it from either list. Bought in game at both sizes; the charge matched the price less the trade-in to the credit |
| 35 | An alert card never lies over another part of the HUD, at any size or `hud_size`, and its words stay on it: a lint rule, `alert.overlap`. Shown on a pad at 1280x800 with the contact list open, a real missile and the FLARE row |
| 36 | The right stick scrolls only the list under the focus or cursor. Shown at the commodity and equipment dealers with both lists overflowing, at both sizes; no run showed both lists moving |
| 37 | Names with an apostrophe draw, on every screen: one glyph set for all. Shown: DEATH'S HAND MK II in the weapon group window on a pad at 1280x800, and in the loadout at both sizes |
| 38 | The boost meter: the reticle's bottom arc, always there, amber under a quarter, with the hull's breath and bloom as it runs out |

The tester's list: the classic nav map and top-right box under HUD 2, the load game page and the loading screen. Issues 3, 10 and 24 stay open: not reproduced or waiting on the reporter.

## For the owner to review
BUY SHIP on the pad is hold only, a departure from R16's tap beside a purchase: the focus opens on it by itself, so a tap could spend a ship (`modules\hud\docs\UI_HUD2_INPUTS.md` 5). A contact list the player opened on the pad may stand in zone S2 ("Checked for this build").

## Not run, at the owner's word (6 Nov 2026, the weekly limit)
No full sweep, no gate, no perf numbers. Leg M07 passed in its second run: 1708 s, every checkpoint, build e920ec57 with the 2x-par stop lifted (`flleg.ps1 -NoFailStates`, on the owner's word: its purpose was the Tau-31 name compare, not time). The first run, at ffa099eb with the stop on, ended at 606 s, 2 s before the next objective; the Sinclair segment took 202 s against its 182 s limit, so with the stop on the leg still fails. Its pace against 30 Oct is not known: those runs kept no segment times. A real jump was not flown for the reticle: the save's gate was locked, and a launch's letterbox and `hud2 sim letterbox` showed the fade. The spin's unit in the game was not traced, so the pace is a choice under 3 Hz. The game's windows were respaced from a survey of 19 faults, at 1280x800 with the skin on; the save list, options, news, repair and job board rows were not reached, and the Neural Net windows keep the game's size. The cruise meter's disruption was not seen in game: no ship disrupted the test's cruise. Issue 33: cases 3 and 4a (no ammo dropped in 40 kills), 4b (no way to the stack), 5b (no kit in reach with a dented hull in 8 kills), 6 and 8 (both need a commodity lying in space, and no NPC carries one): `tests\loot_full_game.md` "Fixture", no product fault. A frame of a block opening on a panel, a lock with the target behind the ship by frame, the wheel at the dealers, a foreground player's real pad. Passed on the final merge (b1a418b2, clean): `sizes` 90 lints 0 FAIL, `modules-matrix` 17 rows, the launch smokes to flight on `hde_upstream` and FLUF (no fault line), legs M08 (662 s) and M10 (1125 s) on build ffa099eb (the code since: HUD drawing, the wheel and the prompts' places, nothing a flight reads), `pad_fallback`, `pad-sdl`, `pad-device`, `docked_text`, `window_prompts`, `state_fixes`, `page_modules`, ship buy on both skins, the module lint, Windows 7's imports.

## Late HUD changes (the owner, 6 Nov 2026)
The reticle goes in the letterbox; the seat centred over it, under the rules of what shows; the panels sized to their content with even air (the live lint's `panel.air`: 277 findings to 26); the bottom prompts mirrored or centred; the fire cursor's spin; the wheel's HUD FADES / HUD ALWAYS, proved by a held LB at both sizes and saved; the boost meter at the reticle's bottom, the cruise meter at its top, the power arc inside it; the dealers' rows, stats and quantity block; the prompts' centred column; the NeuroNet windows centred. These came after the merge named above: `sizes` (90 lints, 0 FAIL) and `owedseat` reran on them, the unit tests and the module lint too; the rest of that list did not.

## Upgrading
Settings stay. A saved Steam Deck mode moves into HUD size once. A player upgrading never sees the first-start card.

## Known limits
The targeting display, read by a judge over five clips: no cut over 100 px at 4K in the last. Left as polish: near a panel the words can fade on and off a few times a second, under the 3 Hz limit; the block crosses the cue or a ring arc for its 0.20 s of travel; a ship can cross the block; in a hard turn the block waits at the seat while the ship swings; hit numbers can touch the cue chip in the seat; another ship's name can lie on a ring arc. One frame of the game's own bracket shows on the first on-screen target of a session. The mini card, a target change, FADES and NONE were not seen in that clip.
Whether a space comm's voice plays on into the base is unmeasured: the sound handles could not be read, and the comm player's flag stayed set for 8 s after the step, which reads as stale. A line stands to its clip's end at most.
A dealer's and the ship dealer's description and stats stay at 115 % at every size and text setting: the stats table is sized for it. At 1280x800 a dealer row's class text touches its price, in both skins: the game's own text at the game's own place. The status window's REPUTATION heading touches its box at that size. The small corner marks on ships and stations that are not the target stay, at every size: no HUD 2 code draws corners, so they read as the game's own, and such a mark can pass over a ring number.
Performance is unmeasured for this build so far: the fingerprint's perf launches need a quiet machine and did not get one; the proof pass takes them on its first quiet window. Its other rows are named in `modules\hud\docs\UI_HUD2_BUILD_040.md`.
Issue 37's shared uppercase (GRAFENWOHR's umlaut) and the autopilot's exact name compares were not seen in game; the name compares owe the legs M07, M08 and M10 in the proof gate. With the larger atlases flight ran at 200 fps at 1280x800 and 186 at 4K, one measure each, RPCS3 running, no build before them to compare.
`tests\formation_control.ps1` loses about 1 round in 20 on the 5 Nov build as on this one (3 of 60 each), always the wing beside Fort Bush after its leader's comm: not a change of this release; the game's reason unread. The game's small corner marks stay on ships that are not the target, beside HUD 2's caret and name: `UI_HUD2_SKIN.md` 6.10 speaks of the target only, for the owner to judge.
A machine can refuse every DLL's handle for a low-level hook (error 126, seen from a plain PowerShell with six system DLLs, cause in Windows unread): the mapper filter now falls back to the exe's handle, shown by `tests\pad_fallback.ps1`'s injected row. A foreground player's real pad was not exercised: background tests open none by rule.
The grown nav map is built but off behind `navnew`, its checks N7-N11 owed, so the frame and words stand around the game's own map; NN_Prompt and so the multiplayer prompt keep the game's look; the loading screen's place name may read LOADING on some load paths; the handheld detection has not run on real hardware; a second install and a Proton run are owed for the new nav map; Windows 7 is checked by imports, not on a real Windows 7.
The cue's lock state was seen with the target ahead (82 of 149 samples, one frame read); a lock with the target behind was not seen in a frame. The failing bars' flash passed by number, under 3 Hz; its stills at `skin_motion` 2 were not judged.

## Checked for this build
**A change to what the HUD lint accepts:** the failing bars' bloom counts as an overlay, as the hit flash does, so `centre.overlap` skips it. It lasts 600 ms (`kBloomSec`, held by `tests\hud2_bars_test.cpp`) at most once a second per bar, at 22 % alpha, so it may pass over a reading element for that long and no longer. The rest of the ring's items keep the rule.

**A second change to what the lint accepts:** the weapon group cue's cells record as one unit, so `panel.overlap` no longer weighs a cell against its neighbour. They stand 0.003 apart by design and the row cannot widen: at 4:3 the ring's low numbers start at 0.058 and a full row ends at 0.059.

**A third, the driver's decision for the owner to review:** a contact list the player opened may stand in zone S2 on the pad, as it always could on the keyboard, by the input-parity rule: S2 only, at most 10 rows; the pad's automatic combat list keeps the rule (`UI_HUD2_PRESETS.md` 1.3). Issue 35's scene is that list.

**A fourth:** the pad's consumable cross's centre mark is not weighed against its slots: it stands among them by design.

Not seen in a game frame: a long pilot name joined past the game's cut (`Escort 1 Rookie Kirkpatrick`: held by `tests\namecut_test.cpp`; the final captures' escorts have short names), and the target block opening faded on a panel (held by `tests\hud2_target_test.cpp`).

Pilot names, after the owed frames: e9ea0d62's join let the record's call sign through and left encounter pilots cut; b8e14ad1 and the surname completion replace it. A cut surname that two surnames fit stays cut.

Pilot names (e9ea0d62): the story missions' 412 `individual_name` references hold none of 24 characters or more, four at exactly 23, which the game's buffer holds whole, so no story objective's pilot was ever cut and no leg was flown for it. The autopilot compares `RepName` with `IdsText`, the game's uncut string, so the fix can only add matches. `speaker_colors.tsv` and `subtitles.tsv` hold two speakers of 23 characters or more, neither of them a cut name.

Issue 33's refusals: full batteries shown at both sizes, the item staying and the count unchanged; normal loot unaffected. Cases 1, 5a, 7 and 9 (every card, by the test verb) of `tests\loot_full_game.md` passed too; the rest stand under "Not run".

Without a game: the release build, the host-free unit tests, `tools\module_lint.py --enforce`, `tools\win7_check.ps1`, `sigtool check --scan`. In game: `tests\hud040\run.ps1`'s sections on instance 7, `tests\ship_buy_pad.ps1`, `tests\dealer_focus.ps1`, `tests\loot_full.ps1`; frames read as well as logs.

## The window layouts (the owner, 6 Nov 2026, last before release)
`modules\hud\docs\UI_HUD2_SKIN.md` 18. Run on the final DLL at 1280x800 and 3840x2160: `tests\docked_text.ps1` (0 text out of a frame or box, 0 ROLE / BACK / RICH), `tests\ship_buy_pad.ps1` hold, buy, stick, kbm with the game's hit test at the moved X, Log buttons and inventory row, `tests\hud040\run.ps1` sizes, owedseat, menus (90 lints, no FAIL). Not run: other aspect ratios, the classic look, the FLUF and upstream HDE smokes on this build, frame cost; the opening slide and the bar's flash are too short for frames and rest on the owner's eye. All fall to the post-release sweep.
