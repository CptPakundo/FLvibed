# In-game test: loot refused at the limit says why (GitHub issue 33)

**Run in part on instance 7, 6 Nov 2026** (`research\experience\runs\loot_full_p2b`, then runs p5, p5b, p5s, p7a, p7b): cases 1, 2 (both sizes), 5a, 7 and 9 passed; 3, 4, 5b, 6 and 8 not run, see "Fixture". The script form is `tests\loot_full.ps1`; this page stays its procedure. The design rests on decompiled code (`docs\FREELANCER_RE.md` "Tractor failure codes"); this test is what verifies it. Background instance, muted,
`tools\deploy.ps1 -Background`, HDE 0.7.1, the branch's dev build, then the release build before it counts. Harness: `tools\harness\`, `docs\HARNESS.md`.

## What changed, and what it can reach
Two listening hooks (`exe::TractorNotify`, `exe::QueueNN`), the HUD bus record, HUD 2's card, the subtitles' Neural Net line, the row `neural_net_reason`. Reach: the
tractor's refusals, every Neural Net line (they pass through the QueueNN hook), `hud_alerts`. No flight, no autopilot logic.

## Fixture
SaveC705 (Starflier, in space near Fort Bush; 3 batteries, 3 nanobots) or any free-roam save at a base with an equipment dealer.
- Limits from the ship: debug `ai self` (`nanobots=n/max batteries=n/max hold=free/size`), or `docs\kb\generated\ships.tsv`.
- Fill up at the dealer: `Invoke-FLBaseRestock` (nanobots, batteries, mounted ammo). It must end at n = max for both; check `ai self` after launch.
- Loot: the roll is per item the dying ship **carries** (`loadouts.ini` cargo), so a pirate fighter (missile and countermeasure ammo only) never drops a kit or battery:
  the first run (6 Nov, run p2z, 110 `destroytarget` kills) found none. Only `MSN13_Nomad_Fighter_Populator` (battery 7, kit 4) and `fauxplayer` (3 / 3) carry them;
  no NPC carries a commodity. The script finds the carrier from the game data and kills it with the autopilot (the roll uses the involved players' worth, which
  `destroytarget` may not credit: unconfirmed; `FREELANCER_RE.md` says both "drops no loot" and "6 items"). Read the drops with `Get-FLContacts -All`
  (`Type loot`, its name); the script prints a census after each kill. Batteries and nanobots drop 33 % per carried unit (`docs\kb\generated\loot.tsv`). Keep the ship inside the tractor's
  reach and out of the fight (godmode as in the other fixtures); spending a consumable breaks the case, so re-read `ai self` before each tractor.
- Tractor one item with the game's own command: target it, `USER_TRACTOR_BEAM` (FREELANCER_RE: target set before the command). Tractor all: `USER_COLLECT_LOOT`.
- Ammo at the limit (50: Common.dll `MAX_PLAYER_AMMO`, which the script reads from the install): buy the mounted launcher's ammo to the limit at the dealer
  (`Invoke-FLBaseRestock` fills every mounted ammo; `weapons` shows the stack). The drop must be that same ammo (missiles and countermeasure ammo drop at 5 %, `loot.tsv`,
  and only above the row's `min_worth`; `loot <ammo> <carried>` gives the game's own expected drops per kill). **Fixture gap, closed by repeating kills:** jettisoning cannot make
  this case (stack plus loot is conserved, so the stack ends below the limit). The script loops spawn + `destroytarget` + `Get-FLContacts -All` up to `-MaxKills` (60: at 5 % a
  kill, 95 % to see one) and stops as soon as that ammo lies there. If it never does, the case is reported NOT RUN with the kill count, never as a pass. The local server
  (`tools\mptest`) with FLHook's cargo command remains the fallback, and covers the multiplayer case below.

Three limits of the fixture, read in the run of 6 Nov 2026 (p7a, p7b; `godmode on` after a load does not protect, HARNESS.md):
- A nanobot is spent only on a dented hull: at hull 1 `usenano 1` answers `used 1` and the count stays. Case 5b waits for a dent and a kit in reach together; 8 kills did not bring both (NOT RUN).
- Cases 6 and 8 need a commodity lying in space. No NPC carries one and every drop has volume 0, so the hold never fills (6) and equipment goes ahead of the kit in the game's list (8): both NOT RUN by drops.
- The hull is not repaired between kills: a farm of 20-odd kills in one game ended in the ship's death, and spawn then says `player location unknown`, `ai self` `not flying` (case 10 shows it on purpose). Split long farms over games.

## Log lines and frames
- `Get-FLEventLog`: `tractor-fail reason=<3|4|5|6> msg=<hash>`; `msg` = CreateID("nnv_tractor_failure") (`Get-FLHash`).
- `nn` line played: subtitles `shown`; `Send-FLS 'shown'` gives the caption text.
- Frames at full resolution within 1 s of the event (a card lives 3 s), 4K and 1280x800 (the handheld size), of HUD 2 and the classic HUD. Judge frame AND log together.
- At start, `FLvibed.log` must not hold a refused hook for "why the tractor refused an item".

## Cases (HUD 2 with Alerts on; expected card: title TRACTOR FAILURE, the amber line below)
| # | Case | Set-up | Expected event | Expected amber line |
|---|---|---|---|---|
| 1 | Nanobots | n = max, one repair kit loot targeted | `reason=5` | STORAGE CAPACITY FOR NANOBOTS IS FULL |
| 2 | Shield batteries | n = max, one battery loot | `reason=4` | STORAGE CAPACITY FOR SHIELD BATTERIES IS FULL |
| 3 | Ammo, gun mounted | stack at 50, that ammo as loot | `reason=6` | STORAGE CAPACITY FOR THIS AMMO IS FULL |
| 4 | Ammo, gun not mounted | (a) stack below 50: loot of ammo for a gun the ship does not mount. (b) if a stack of it can be had at 50 | (a) none, the item arrives (b) `reason=6` | (a) none (b) as case 3 |
| 5 | Normal loot | room for it: a commodity, and a nanobot with n < max | none | none; the item arrives: hold or count goes up; the game's own voice as ever |
| 6 | Hold full | hold free < the item's volume | `reason=3` | YOU DON'T HAVE ENOUGH CARGO SPACE |
| 7 | Out of range | loot beyond the tractor's reach | none | none: the game's `target_out_of_range` only |
| 8 | Tractor all | nanobots at max beside a commodity with room | `reason=5` before the pull, as the game orders it | the commodity still arrives; card as case 1 |

Case 4 is expected from the code: the limit is the stack's, whatever is mounted. If the game refuses the item below 50, or the issue's "currently-equipped" distinction shows
in play, that reading is wrong: stop and go back to the disassembly.

Then the same cases 1 and 5 with each of:
- `hud_alerts` 0, HUD 2 on: no card; the Neural Net column reads TRACTOR FAILURE: STORAGE CAPACITY FOR NANOBOTS IS FULL, fully on screen.
- Classic HUD: the same caption in the top-right Neural Net corner.
- `neural_net_reason` 0 (`Send-FLS 'set neural_net_reason 0'`), classic HUD: TRACTOR FAILURE alone. With HUD 2's Alerts on: the card still shows, no caption doubled.
- `neural_net` 0, classic HUD: no text at all, the voice as ever.
- `[Modules] hud = 0` and `subtitles = 0` each: no fault, the other surface works, the voice as ever.

Layout without loot: `hud2 alert tractor 3|4|5|6` draws each card; the longest (case 2) must fit inside the card at 4K, 1280x800 and every HUD size preset, no clipping.

Multiplayer (`tools\mptest`, `tests\mp_modules.ps1`'s server): case 1 for the client's own ship shows as in single player (the client verifies first). A second client's
refusal shows nothing on the first.

## Pass
Every row's event and text as above, in frames and log together; no card or caption without the game's line; a hot reload (`deploy.ps1 -Background`) keeps it working;
every other Neural Net line unchanged. Nothing runs per frame beyond one compare in HUD 2's slow pass, so no frame-time measurement is owed. A case that misbehaves is a
failure of the design, not of the fixture, until shown otherwise. Close the game afterwards.
