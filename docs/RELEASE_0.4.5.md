# FLvibed 0.4.5: release notes

Released 7 Nov 2026 on the owner's word ("carry out this task of overhauling the remainders into fully releasing 0.4.5"), with no full sweep and no gate, just the crucial checks. The owner was away for the release; a bindings editor moved to a later release.

## What it is
HUD 2 game-wide: the NeuroNet's windows as full screens (`docs\NEURONET_PLAN.md`) and the base rooms' windows as pages (`docs\BASE_ROOMS_PLAN.md`): dealers, ship dealer and its final stage, repair, job board, news, best prices; room and NeuroNet word tabs; item images in every list; compare; empty hardpoints; the world paused under a NeuroNet window in single player; the star map's mouse flow. Full list: CHANGELOG.md.

## Checked
Unit tests, the module lint, `tools\setup\test_setup.ps1`, the export's doc check; in a background game with a virtual pad, dev and release builds, at 1280x800 and 4K: buying, selling, quantity, filters, repair, a ship bought through its final stage, a job taken, every page closed with B, no fault in the log.

## Not run
No full sweep, no gate, no perf numbers. Never tried: a real pad, a real mouse's clicks and wheel on the pages, multiplayer on the pages, 4:3, FLUF beside the new word tabs.

## Known, left for later
- At 1280x800 the load and save lists run past their frame into the prompts (game_text_size 125): a fix needs the in-game names column's field.
- The item mesh does not scale up. The docked inventory has no 3D ship: ship numbers stand there.
- Options keeps its side tabs; K&M prompts are missing on Options, load and the main menu; the LAN / INTERNET question keeps the game's art; the jettison count cannot be typed.
- BEST PRICES was seen with one known base and docked only.
- CHAT and GROUP keep the game's windows on the page's ground.
- The bindings editor.
