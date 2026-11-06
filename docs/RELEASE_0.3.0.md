# 0.3.0: what the next release holds (the owner, 5 Nov 2026)

No 0.2.8 release: everything built under that name goes out in a bigger **0.3.0**, which also solves the issues reported since.
Nothing is published until the owner says so. `CHANGELOG.md` "Unreleased" is the player's list; `modules\hud\docs\UI_HUD2.md` 5.33-5.40 the detail.
"0.2.8" in commit messages, run folders and test budgets is this work's old name.

## Built, in main, not released
Hover help flash, steady prompt pills, job plate growth (5.33); Steam Deck mode (5.35); text by role in every game window, `window_text`,
on by default (the owner, 5 Nov 2026) (5.36-5.38); option rows for `deck_mode` and `window_text`; nav map lower; comm window hidden under a pause in space; L1 / R1 readable.
The owner on the dealer, 5 Nov 2026: "Dealer looks good, keep going." The other windows' frames were sent; no verdict on them recorded.

## Reported since: built 5 Nov 2026, replies on the owner's word (detail: `modules\hud\docs\UI_HUD2.md` 5.39)
| Issue | Kind | What was built |
|---|---|---|
| 14 | bug | groups on both triggers fire full auto |
| 16 | bug | each group row shows its own trigger |
| 18 | bug | the shield drains down; the hull already did (the owner's word) |
| 15 | request | Y tap next group, Y held cruise by default (the owner's word); `y_swap` the old order |
| 17 | request | QUANTITY and MAX at the equipment dealer whenever the game offers a slider |
| 19 | request | `glyph_style` 4 Steam, automatic on a Deck (not run on a Deck or a Steam Controller) |
| 20 | bug | A held buys or sells at both dealers, LS click mounts (the owner's word, on the reporter's comment) |
| 21 | request | danger alerts beside the ring in a stack per side, with a burst and a glow (the owner's design: `modules\hud\docs\UI_HUD2.md` 5.40) |

Also open on GitHub: 10 and 13 (built, replies on the owner's word), 3 (stalled NPCs, not reproduced).

## Open before a release
- **The blank UI** (`modules\hud\docs\UI_HUD2.md` 5.38): all game text and icons gone, seen once, cause not read. What to read if it returns is written there.
- Not run: the real pad on any of this; other bases than Buffalo and Manhattan; a three-row comm line over the nav map; the map docked; a real mouse on the map's objects.
- Living spec captures: retaken after the release (v71, `spec_030_2026-11-05`; flight, alerts, dealers, the settled windows). Still older: the mission offer, player info, pause docked and save windows, the 21:9 and other aspect frames, the classic HUD's combat and dock frames. `docs\spec` with them pushed to the published repository the same day, after the tag.
- The release path: `tools\package.ps1`, `core\version.h`, the version texts on the spec.

## Replies, drafted for the owner's word (posted from the owner's account: no "we")
Each reply carries a post-fix frame (the owner, 5 Nov 2026), for the reply alone: never committed. `build\package\issue-frames\issue-<n>.png` (git-ignored), attached through GitHub's comment box in the owner's browser, since `gh` cannot attach an image to a comment. 14 has nothing to see: its reply gives the measured rate.
- **14:** Fixed for 0.3.0. A held trigger now fires full auto with a weapon group on each trigger; the game only fired those groups once per press. Measured: 12 shots in 3 s from guns with a 0.25 s refire, 25 from one with 0.12 s.
- **15:** Done for 0.3.0, and it is the new default in the Freelancer layout: tap Y for the next weapon group, hold Y for cruise. "Swap Y tap and hold" under Weapon groups gives the old order back.
- **16:** Fixed for 0.3.0. With a group on each trigger, each group's row now shows its own trigger.
- **17:** Added for 0.3.0. The Equipment Dealer now shows LT / RT QUANTITY and Y MAX whenever an item has a quantity.
- **18:** Fixed for 0.3.0. The shield meter now drains downward. The hull meter already did, so the two now match.
- **19:** Added for 0.3.0: a Steam choice under Button prompts, with Xbox face buttons, View / Menu and L1 / L2 / R1 / R2. It should pick itself on a Deck, but neither a Deck nor a Steam Controller has been tested yet, so a report from one is welcome.
- **20:** Fixed for 0.3.0, and the comment's idea is in too. Hold A to buy or sell at both dealers, as on the Job Board; LS click mounts and unmounts.
- **21:** Done for 0.3.0. Shield Failing now stands by the left meter, Missile Incoming and Hull Critical by the right, level with the reticle. Two dangers show together, on the same side the newest pushes the older down, and they glow and pulse while the prompts stay steady.
- **10, 13:** built since their last reply (UI_HUD2.md 5.33): the replies still to write when the owner says so.

## State at the handover (5 Nov 2026)
`tools\drift_check.ps1` clean but for `research` (22 notes due, as before). Unit tests 64, 0 failures. `ui/spec` v143. No full sweep ran: one was started by
mistake and stopped on the owner's word after a minute (`pad-input` passed, nothing else), so the last full sweep predates all of this. Published 5 Nov 2026 on the owner's word: GitHub bc4b048, tag v0.3.0, the release with the zip and the setup (verified by download), living spec v70, then v71 with the captures. Replies posted the same day with their frames (the texts above are the drafts; the posted ones differ a little): 13-21 closed, 10 open until the reporter confirms the flicker on the Deck, 3 open.
