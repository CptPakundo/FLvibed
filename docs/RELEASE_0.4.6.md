# FLvibed 0.4.6: release notes

Released 11 Nov 2026. The release before it: [0.4.5.8](RELEASE_0.4.5.md). Every change, in the player's words: `CHANGELOG.md`, section 0.4.6.

## What it is
A sweep over the open GitHub reports: every open issue labelled bug, the two critical ones (80, 79) first, then the smaller requests that needed no new design, and two requests of the owner. A cause shared by several issues was fixed once as one mechanism, otherwise with the smallest change; what the player sees changed only where an issue asked for it. Every fix was seen in a game; layout fixes at 3840x2160 and 1280x800. Playtesting and reports: Shockwave (Shockwave508).

## GitHub issues fixed
| Issue | What the player saw | The cause, as found | The fix | Commits | Verified / not verified |
|---|---|---|---|---|---|
| 80 (critical) | The options page's Intro picker did nothing on a default HD Edition install; on a Steam Deck the 2003 choice left the game on a black screen at every start | HD Edition's installer skips the startup intros by default, so there was nothing to replace. The 2003 choice's black screen did not reproduce on Windows: the 2003 intro played, subtitled, and the game reached the main menu | The option is removed (the owner's decision on the issue): the intro is HD Edition's own choice again; an old `movie_intro` line is ignored; the intro subtitles stay | dcb62085 | The 2003 intro through the old option played on Windows. **Not verified:** Proton, where the black screen was reported; its cause there is unexplained (nothing reads the setting now) |
| 79 (critical) | The Neural Net inventory mounted and unmounted weapons in the middle of space | The page called the game's mount function with no docked test; the game's own inventory window hides its mount toggles in space | MOUNT / UNMOUNT only while docked: the action, its button and its prompt | b2c75339 | In space A changes nothing and no prompt shows; docked, unmount and mount work |
| 77 | A target's name and meters drew over the weapons panel | The loadout drew before the targeting display, and the name's keep-off rule tested the wanted place, not the place shown | One per-frame list of the flight HUD's reserved areas that every part places itself by, with a lint rule for one part drawn over another's area; the panels draw over the targeting display; the name stays faded until it has left a panel | 2cc7127a, b83d01ef, then bfdc964f (the name's keep-off looks ahead over its fade) | A test target swept through the loadout, then a real target in the release check: no part over another. Both sizes |
| 78 | A long TRACTOR prompt ran across the weapons after a fight | The fallback prompt lanes ignored the consumables cross | The same list of reserved areas: the prompts stop beside the cross and wrap | 2cc7127a, b83d01ef | At 1280x800 the long prompt wraps and ends beside the cross; 4K the same; the lint flags nothing |
| 72 | Zoomed in, the map's place names drew over the objective and route card | Names were kept clear only of the map's viewport, and text draws over plates | Keep-out areas for the map's name placing: a name that would cross a card, the title, the Neural Net tabs, CLOSE or the pad's prompts moves to its mark's other side, or hides while its mark stays | b3ba4048, then d3dd6411 (the tab strip and the mouse's CLOSE, which the first fix missed because the map places its names before the strip draws), then b2526028 (the pad's prompt chips) | Panned across every area at both sizes and all three HUD sizes, space and docked, mouse and pad: no name under a keep-out (before the last fix: a name under a pad chip in most steps). **Not verified:** a two-row prompt lane; a real objective card (a route card stood in for it) |
| 74 | At HUD size Normal on a 16:10 screen the map's system name sat further left than the other tabs' titles | The map's layout shrank its side margin below about 16:9 | The title at the other tabs' margin | 7f8833cf | At 1280x800, Normal (the only size that showed it) |
| 59 | The map Legend's icons were huge, the red box ran off the screen, the words stood a row low | The per-frame rewrite of the map's box and scale; a text margin that dropped the map window by 0.05 of the screen height; the game itself stretches the Legend's icon mesh | The Legend in the game's own box, scale and origin; each word beside its swatch; the icons' width set from the mesh's own bounds, so they are square at every aspect ratio (the owner's request) | 48421fc2, 88104ca7, 4990cf17 | Matches a game without the HUD module; swatches square at 16:9, 16:10, 4:3 and 21:9 |
| 75 | Hovering a place on the map showed the game's old box, away from the place on the bigger map | The map's hover is the game's tooltip, placed by the size of the game's own, smaller map | A HUD 2 name tag by the cursor, clear of the title, chips and cards; nearby names step aside from the tag, the hovered place's own name does not show twice | 86fc6882, ad05467d | A planet, a station, the player ship, the sun, empty space; mouse and pad; 4K and 1280x800. **Not verified:** a field (none discovered in the test games) |
| 71 | With a mouse or the stick's cursor, the hint boxes for the room bar and the Neural Net box sat under HUD 2's top strip | HUD 2's tooltip did not catch the room buttons' hints, and the game placed them under the strip | Hints in HUD 2's look, clear of the strip, nearest the game's place; each of HUD 2's words shows the hint of the control it stands for | 3245c0d7, then f1395a70 (the first fix showed the hints of the game's hidden buttons lying under the words, not of the controls HUD 2's words stand for), 9633073c (see below) | Every room word, action word and Neural Net word showed its own hint, both sizes, by a simulated hover with the mouse and the stick's cursor in the release build. **Not verified:** a real mouse |
| 62 | Stepping between Neural Net tabs flashed the room bar, the Neural Net box and the game's old window art for about a second | No page counts as open in the gap between two windows (about 1.2 s), so the cover dropped | The page's ground and title stand while a tab is on its way; the text area opens once the game's text stands in its place; a page's columns appear with its text; no mouse arrow at the screen's centre as a page opens in space on a pad | a8a23d70, then b6ea0177 (the window's title flashing in the text area), 8534d0fa (the columns at a wrong width), 0d43256b (the arrow), e85bdb8c (the gap into the MAP tab, which the first fix had left out) | Frames at 60 per second, docked and in space, both sizes: no flash; about 50 page openings without a jump; no arrow frame in 53 openings and switches of the release build; the cargo demand's box without an arrow. The death screen's log page is as in 0.4.5.8 (see "Known") |
| 57 | A yellow highlight flashed at the top right in the fade between two docked rooms | HUD 2's own focus mark on the Neural Net box, drawn while the box's plate was down | The mark shows only while the plate stands | 00911f9a | 3,600 frames over three room changes at both sizes: no mark |
| 63 | A piece of the game's window frame showed above the text on INFO and LOG | The text area was grown on its top edge too | No growth on the top edge | 00671e80 | INFO and LOG, docked and in space, scrolled and not, both sizes |
| 73 | STATUS's Total Time stood still while the page was open | The page stopped reading the stats once the faction card opened; the game keeps writing them under the card | The stats are read under the card | ac70203d | At 1280x800 docked: the time ticks on |
| 58 | Some of the ship's own items showed no icon on the inventory and at the equipment dealer until scrolled | 0.4.5's order by hardpoint put visible rows outside the game's window of five icon rows | A row's icon is set through the game's own call where it can be; otherwise the window covering the most visible rows | 42f0bdf6, then da34ee54 (the release check found icons lagging a scroll step behind their rows and a model reload every frame) | Never a wrong icon; the cost of the icons at rest fell from about 551 to 0.8 microseconds a frame in the worst state found. **Known:** see below |
| 76 | A pirate's or the police's cargo demand came in the game's old window | The game's mission message box in its drop-cargo form, only re-plated | HUD 2's standard dialog with the ship's name, DROP CARGO / CANCEL; on a pad A drops, B cancels | e6ae4efd, then e0110e3f (the flight HUD stays up under the box), 8a2a575d (the pad's answers in the flight HUD's prompt lanes, the box a reserved area, no arrow on it) | Opened through the game's own opener: CANCEL kept the cargo, A dropped all of it; mouse and pad, both sizes. **Not verified:** a real pirate or police demand |
| 61 | Political and Patrol ALL on the map show white zones and paths | Not FLvibed's: a game without the HUD module draws the same (white is neutral) | None in 0.4.6 | - | **Not verified:** the report's own system (Hamburg). See "Known" |

## Requests taken
| What | Issue | Commits | Verified / not verified |
|---|---|---|---|
| With Comms off and dialogue copied to the chat, mission lines stand in the chat only, as the setting's help says; the Comms help says how to get this | 83 | 04274249, e17cdcb0 | Four setting combinations at 1280x800, each line once; chat only also at 4K. **Not verified:** the classic HUD's chat window |
| Subtitles in HUD 2's font everywhere, the new default; settings files saved before 0.4.6 get it too unless they had picked Segoe UI (a settings revision mark and a table of carried defaults) | 82 | b6405038, b4a820a0, f6250c5e (the Font row's third choice) | Both sizes: a bar conversation, the intro movie, flight, also with the classic HUD and the HUD module off; an old settings file read at 1280x800 |
| A notice at the flight HUD's lower left for a mission payout in space and for loot the tractor took in, with the hold's room; it fades after four seconds, nothing stays on the HUD | 70, 69 | 22c92b3c, 4fdb66e3, then 550eb5dc (never over the grown chat stack or a fading ship panel), f9006a31 (a game load never shows as a payout), e296af3f (off Focus's escort rows) | Real loot and a stack of 15 in the release check; loads from space and docked and the death dialog's load show no notice. **Not verified:** New Game, LOAD AUTOSAVE, a jettison, mission cargo |
| Rear View off the action wheel (the right stick's click held instead); X tractors everything in reach in the Assisted layout and under the autopilot | 65 | c300bf13, 4eb45f93 | A virtual pad: every slice of the wheel at both sizes; X took the loot in reach. **Not verified:** a real pad, the classic HUD's wheel |
| Credits and cargo under the ship's name on INVENTORY; the cargo pips are gone, a full hold shows in the warning colour (not asked for) | 67 | b8b8da90 | In space and docked, both sizes |
| A locked item's card and a refused ship's deal show the player's own level or standing beside the need | 56 | 561f0d0b | The level line at both sizes, the reputation line at 4K. **Not verified:** a ship refused for standing; the reputation card at 1280x800 |
| The equipment dealer's durability meters share one width per list | 55 | 5611d446 | Both sizes |
| Coloured button outlines: the ring around A, B, X and Y in the button's own colour, every family; plain buttons unchanged (the owner asked for this) | - | abfa7fbc | Xbox and PlayStation in game; the release build's frames differ from 0.4.5.8 only by these rings. **Not verified:** Nintendo in game |
| The map Legend's icons keep their shape at every aspect ratio (the owner asked for this) | 59 | 4990cf17 | See issue 59 |

## Fixed on the way, no issue
- **The flight HUD under a cargo demand** (e0110e3f): it vanished for as long as the box stood, because the box's arrow cursor read as an open Neural Net window. It stays up and live now. Also: no flight HUD on the cover between two Neural Net tabs in space (eabde8d3).
- **The STATUS title over the card** (42c4306d): a cut piece of the window's own STATUS title showed at the top of the faction card.
- **Prompts and presses in the gap between two tabs** (72adf31f): while the next page loaded docked, the pad's lane showed the room's prompts and A could click the room behind. Only B CLOSE shows now and nothing reaches the room.
- **The comm feed over prompts** (49a8aa65): a chatter card at the lower left covered prompts and the notice. The feed's cards are a reserved area now.
- **A character's name card over the options page** (9633073c): since issue 71's first fix the game's name card of a room character drew over FLvibed's options page; it stays under it again, as in 0.4.5.8.

## How it was verified
- Every fix seen in a background game, with frames at 3840x2160 and 1280x800.
- The release build checked item by item in three passes, each after the fixes the one before had found.
- `tests\pixels.ps1` against 0.4.5.8: no difference on a UI element beyond the coloured rings.
- `tests\docked_text.ps1`: 4K 0 findings, 1280x800 1 finding, as in 0.4.5.8.
- The setup test (`tools\setup\test_setup.ps1`): all passed.

## The release build and package
The released files (11 Nov 2026):
- `FLvibed.dll`, from commit d634a02d: SHA-256 3CE9309C3C905499D732CC67BF6C32D91A693A7ED05D997801FB127C66EBF237
- `FLvibed-0.4.6.zip`: SHA-256 9FDC01896817A0CFDF04D288D50441B5097209DE0E148B0C40002485A1F69101 (the DLL inside it is that file)
- `FLvibed-0.4.6-Setup.exe`: SHA-256 8D5F61E80870D2E56DA39B4FD7674EA972F7372364728CBD0B7D5ED53A10EBCA; the setup test passed on it

## Not verified
A real pad; a real mouse over the hints and the pages; a real pirate or police demand; a field's name tag on the map; the Nintendo glyph family in game; New Game against the notice; the classic HUD's wheel and chat window; Proton for issue 80's black screen, which did not reproduce on Windows.

## Known, left for later
- **Issue 61:** Political / Patrol ALL is the game's own drawing. The owner decided (11 Nov 2026): the issue stays open as an idea for 0.5.0, HUD 2's map drawing the neutral zones more quietly so the lanes and bases under them stay readable.
- **Item icons:** while a list scrolls with a held D-pad, the row arriving at the top has no icon for one frame.
- **The cargo demand's dialog** stands over the lower part of the reticle.
- **Neural Net pages show their text about a sixth of a second later than in 0.4.5.8:** the text area opens once the text is in its place, which is what keeps the game window's title from flashing through it.
- **The same in 0.4.5.8, measured at 60 frames a second:** on the death screen's log page in a pad game the mouse arrow shows for about half a second and the game's own window for one frame; STATUS opens as a half-filled page for about half a second (labels without values); INVENTORY's stats column is squeezed for 4 frames.
- When a fading notice and a comm card meet in the Focus layout they can overlap for the fade's length.
- **Single frames around a tab switch:** the room's prompts for one frame as the map opens from a room, and the room's card over the map's first frame; in space the map's prompts and reticle stay 4 to 6 frames over LOG's ground, and B CLOSE shows only for the gap's first frames there.
- **The same in 0.4.5.8:** the docked map's ROUTE card without names; the destinations list offered in space only; the room bar's icons at 4:3.
- **The same in 0.4.5.8:** the target's name on the weapon-group pills at 4K and the clipped names in Focus's weapons panel. The centre of the screen is issue 86.
- The game's own green "CARGO JETTISONED" text stands over HUD 2's words.
- An alert card can stand under the open action wheel.
- The release DLL carries the folder path it was built in.

## Issues that wait, with the reason
- **60** (the Bases submenu): parts 1-3 are new columns and sorting; part 4 did not reproduce (the test games know one system).
- **40**: the stat card lacks three of the text's stats.
- **53** (difficulty on New Game), **84 / 85** (rank and ship icons: new art), **66 / 64 / 54** (new mechanisms or pages): larger than this sweep.
- **86**: the centre of the screen's redesign, 0.5.0.
- **51** (fonts): its route is not decided.
- **81** (other mods): not part of this sweep.
- **3** (stalled NPCs): not reproduced.

## The last checks before the package
- Release build 4FF42ED2 (commit 431e1d30), re-checked in a game at both sizes for what had changed since the full passes: the notice never at a load (space to space, with a notice standing, space to docked, docked to space, the death screen's two loads); no arrow in 53 opens and switches in a pad game; the map's names off the pad's prompt chips; the Focus layout's escort rows and the notice clean by the layout check. It found one fault against this release's own promise: docked, a switch into the MAP tab still showed the room for about a second.
- That was fixed in e85bdb8c (issue 62's last commit) and seen by its fixer in a release build: every direction of switch into and out of the map, docked, at both sizes, and a press of A in the gap.
- The packaged build 3CE9309C (commit d634a02d) differs from 4FF42ED2 by that commit and a changelog line. On it: the unit tests, the module lint, the setup test, and a smoke check in a game at both sizes: the logs without an error line; docked, every frame of the gaps LOG to MAP, MAP to LOG, INVENTORY to MAP and of the map's first open shows the ground, the tab strip and B CLOSE only, and A or X pressed in the gap does nothing in the room; in space the gap shows the ground and no flight HUD, which is back when the Neural Net closes; INVENTORY's rows with their icons; STATUS's Total Time advancing.
