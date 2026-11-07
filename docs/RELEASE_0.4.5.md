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

Refreshed a third time (the owner's word; issues 41-46 and the owner's pad check): the ship dealer's pad sides (ShipViewControl turned by the stick, flplusplus's zoom +0x3EC by the triggers; the owner checked it on a DualSense), the rooms' D-pad walk (snaps, the hidden cursor parked on the focus) and CONTEXTUAL room prompts (4 s after the last input), the objective banner held back between NeuroNet tabs (`exe::ObjectiveBannerPump` detour; A/B: released build 6 of 6 steps, this none), INFO / LOG text over the prompts' lane, the dealer's compare tag (RS click, a mouse chip), the boards' list / text sides, the inventory ship view's square box. Not seen: a real mouse's chip click, an Anubis, the rooms on the owner's pad.

Refreshed a fourth time (the owner's word; Shockwave, issue 47): the bars' glow and bloom bigger (`hud2bars` kBloomReachH 0.08 etc., all in H) and a fresh Bloom inactive (it blocked the first 1.6 s); the shield-down state (the arc's pulse, SHIELDS / DOWN at 1.12 R left of the reticle, blue-red at 2.9 Hz; `hud2 sim shielddown`); the equipment dealer's own rows (the tag change had split an if/else: the class twice; MOUNTED green, TAGGED TO COMPARE in the class's place). Seen at 1280x800; not seen: a bloom in real combat.

Refreshed a fifth time (the owner's word; Shockwave: comparing in the inventory did nothing, an empty hold): `nnkit::Compare` gives a mounted item `gamedata::Against` too, kept only when it is the tag and not itself (VS ITS NAME (TAGGED)); inventory and the dealer's own rows alike. Seen at 1280x800 (SaveC701, a bought Lavablade against the tagged Justice).

Refreshed a sixth time (the owner's word; Shockwave: AMMU.): the dealer's filter tab takes the English short word where the game's long word starts as it does (AMMUNITION -> AMMO), other languages cut as before. Seen at 1280x800.

Refreshed a seventh time (the owner's word; Shockwave: the game's name card flickering over HUD 2's on the D-pad in rooms): `uiskin::TipUnder` hides the ToolTip again after the game's update while `uistrip::HoverTipNow` sees a hovered character; padbases re-hovers only a hover the game dropped and keeps the parked cursor still inside the box. Seen in a background game (the card on every sample); the flicker itself not reproduced there (no picking in a background window): the owner's or Shockwave's foreground game is the check.

Refreshed an eighth time (the owner's word; Shockwave: the cruise meter flashing while it works): the cruise arc's halo gone while charging / cruising (a steady 0.35 glow), a bloom only on a disruption (`hud2 sim disrupt`); every zeroed meter's bloom in one red (kHostile: shield, hull, cruise, the target's bars). Seen at 1280x800; the owner got a GIF of the ring's states.

## Not run
No full sweep, no gate, no perf numbers. Never tried: a real pad, a real mouse's clicks and wheel on the pages, multiplayer on the pages, 4:3, FLUF beside the new word tabs.

## Known, left for later
- At 1280x800 the load and save lists run past their frame into the prompts (game_text_size 125): a fix needs the in-game names column's field.
- The item mesh does not scale up. The docked inventory has no 3D ship: ship numbers stand there.
- Options keeps its side tabs; K&M prompts are missing on Options, load and the main menu; the LAN / INTERNET question keeps the game's art; the jettison count cannot be typed.
- BEST PRICES was seen with one known base and docked only.
- CHAT and GROUP keep the game's windows on the page's ground.
- The bindings editor.
- Refreshed again on the owner's word: DECK at a station's dealers, the briefing title, STATUS's X - fixed. The news window has five headline boxes and no scroll control (NN_News's children, 7 Nov 2026): nothing to mark. STATUS still shows the word STATUS cut at the card's top right: no text control (PlayerInfoTitle blanked showed it still; PlayerStatsTitle is the faction's name), likely the window's own art.
