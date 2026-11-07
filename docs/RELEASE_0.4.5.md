# FLvibed 0.4.5: release notes

Released 7 Nov 2026 on the owner's word ("carry out this task of overhauling the remainders into fully releasing 0.4.5"), with no full sweep and no gate, just the crucial checks. The owner was away for the release; a bindings editor moved to a later release.

## What it is
HUD 2 game-wide: the NeuroNet's windows as full screens (`docs\NEURONET_PLAN.md`) and the base rooms' windows as pages (`docs\BASE_ROOMS_PLAN.md`): dealers, ship dealer and its final stage, repair, job board, news, best prices; room and NeuroNet word tabs; item images in every list; compare; empty hardpoints; the world paused under a NeuroNet window in single player; the star map's mouse flow. Full list: CHANGELOG.md.

## Checked
Unit tests, the module lint, `tools\setup\test_setup.ps1`, the export's doc check; in a background game with a virtual pad, dev and release builds, at 1280x800 and 4K: buying, selling, quantity, filters, repair, a ship bought through its final stage, a job taken, every page closed with B, no fault in the log.

## Refreshed the same day (on the owner's word, in place)
Shockwave's 1280x800 shots: STATUS's reputation list a thin strip, INVENTORY's middle numbers run into each other, INFO's faction names scrolling. One cause: the card window of each stood at a shift fixed in H, right for 16:9, short at 16:10. `windowtext` Spec `rightEdge` now puts that card's right edge at the page's (any aspect, `deckmode::SideW`). Checked in a background game at 1280x800 and 4K, in space and docked, every NeuroNet page, dealers, job board and news.
Also: a station deck's actions (Shockwave: bare icons) - `winplates::DockedPlates` took a room's actions only within 0.13 H of the current room's button, and the game centres a deck's under the bar; with none that close it now takes every shown one. The mission offer's giver name had a fixed 0.28 H: it now takes the room up to the title, and the title (`OfferDialogTitle`, its position property from a home read once per dialog) steps right for a long name, never into the X (the owner's ask).

Also (Shockwave, the owner): commands under the NeuroNet pause (`input.cpp`: a page's press or click and every PlotBestPath call give the server 5 frames of FrameUpdate at dt 0 with its 0xB9 gate open, the clock's gate and the space update shut; checked: a route from the destination list and a jettison went through, two nearby ships stood at the same distance before and after); the job board's DIFFICULTY per row (host v45 `OfferDifficulties`, read from the offer's text arguments at creation, rows lined up by reward, none shown when they do not line up) and its scroll bar; INFO / LOG lists "N OF M" when cut; HUD 2's swaps frame-exact (CHANGELOG). Then: dealer NEEDS LEVEL / REP from the market files (gamedata Need, mbases local_faction, pub::Player::GetRank; seen: Lavablade Mk III NEEDS LEVEL 2 at level 0), FOR MOUNTED ammo (gamedata Fires; seen after buying a Javelin launcher), one side prompt, TRADE ROUTES (the label is its own control TradeRouteText, blanked by its setter in UnderBegin; the button moved off), the briefing (the strip stands down under a full-screen page or map; NavMapContinueText / NavMapAbortMissionText blanked while the chips stand, restored after; seen on Save7e1c's launch at both sizes).

Refreshed again (the owner's word, Shockwave's reports): `hud_module` runs `windowtext::Frame` while `nnpage::On()` whatever `window_text` says (with it 0 the card stood at the game's place: reproduced at 1280x800, fixed, seen); `uistrip` stands down for `navnew::Grown()` only (with `nnpage::On()`, which includes `basepage::On()`, the dealers lost their credits); the docked full-screen map hides the NavBar's ButtonControl / RoomControl and the NeuroNet box's NNBackground / *ButtonRoot with its own art and `winplates::SetMapOver` keeps the docked plates off (seen); the story popup (`NeuroNetMissionMessage`) has a `windowtext` Spec with `fit`: its box grows to its text (boxFoot 0.80), `FitFoot` moves MMOkButton / MMOkText and grows the plate as much (seen at 1280x800: the whole text, no bar). Not reproduced: DECK always lit (a planet's rooms followed the pad's bumpers; no station fixture).

## Not run
No full sweep, no gate, no perf numbers. Never tried: a real pad, a real mouse's clicks and wheel on the pages, multiplayer on the pages, 4:3, FLUF beside the new word tabs.

## Known, left for later
- At 1280x800 the load and save lists run past their frame into the prompts (game_text_size 125): a fix needs the in-game names column's field.
- The item mesh does not scale up. The docked inventory has no 3D ship: ship numbers stand there.
- Options keeps its side tabs; K&M prompts are missing on Options, load and the main menu; the LAN / INTERNET question keeps the game's art; the jettison count cannot be typed.
- BEST PRICES was seen with one known base and docked only.
- CHAT and GROUP keep the game's windows on the page's ground.
- The bindings editor.
- The news list was not checked with more than five headlines (Manhattan has four, no scroll control found).
- The mission briefing's MISSION BRIEFING title (the game's) stands on the grid's letter row: not `AttentionTitle` (moving it moved nothing); the control is not found yet.
- At a station, the docked bar's DECK reads lit whatever room you are in (Shockwave): not reproduced on a planet, no station fixture yet.
