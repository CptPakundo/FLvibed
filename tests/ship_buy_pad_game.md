# In-game test: buying a ship on the pad, and the right stick at the dealers (GitHub issues 34 and 36)

**Run on instance 7, 6 Nov 2026** (`research\experience\runs\ship_buy_p2d` to `p2i`; results in `docs\RELEASE_0.4.0.md`). The script form is `tests\ship_buy_pad.ps1`; this page stays its procedure. Shown in game: steps 1-18 and 20, the hold across screens (H1-H3) and a PlayStation test pad, at 3840x2160 and 1280x800 on the HUD 2 skin; the classic skin ran once before the last fixes (`p2d`), its steps 8-9 and 13 at 1280x800 only. Not run: step 19 at the equipment dealer at 4K (no description overflows there) and the keyboard and mouse section. Background instance, muted, `tools\deploy.ps1 -Background`, HDE 0.7.1, the branch's dev build, then the release build before it counts. Harness: `tools\harness\`, `docs\HARNESS.md`. A stuck step: stop and diagnose, never repeat a press.

## What changed, and what it can reach
`padbases.cpp`: ShipFinal, ListBar, the hold's rules (`dealer_rules.h`), the side keys, the entry focus, the prompts. Reach: the commodity and equipment dealers (the same code), the job board and first ship stage (the same hold, with `Tick` and `NewScreen`), every pad screen's A carried across a screen change. Not flight, not keyboard and mouse.

## Fixture
- **Save:** SaveC701: free roam at Manhattan, 500k credits, a Starflier with two Justice Mk I (`docs\TESTING.md`). `Goto-FLRoom 'ShipDealer'`, `Wait-FLRoomReady`, `Send-FL 'goto IDS_HOTSPOT_SHIPDEALER'`, wait for `ShipTraderShipName0` in `uiitems`.
- **An unrefused row:** `uistrip data` (its line `ship sel= refusal= level= price= budget= credits=`) must read `refusal=0` on the row chosen. +0x3FC read 1 1 1 or 0 0 0 across openings of one game, cause not read (`FREELANCER_RE`): close and reopen; if it stays 1, say so and use a save with a higher level. Never report a refusal as a pass.
- **The pad:** at the device layer: `Enable-FLDevPads`, `Set-FLDevPad -Slot 0 -Buttons A`, `Press-FLDevPad`, for each of the kinds `-Vendor xbox` and a PlayStation / Nintendo pad through the SDL test pads (`Press-FLSdlPad`); `Disable-FLDevPads` at the end. The hold has no pad-kind branch, so the second kind needs step 2 only.
- **Too few credits:** the client's copy of the cash, `exe::PlayerCash` 0x673364, by `poke` while the window is closed, to under the ship's price less the hull's trade-in and over the first stage's budget. It is the client's copy: only for the refused steps, and reload the save after them, since a purchase on it would disagree with the server.
- **One item short (step 12):** at the final stage, the same address poked to the price less `NN_Dealer` +0x9FC less 100: step 10's credits lie under the sum by all the equipment's worth, so a transfer or two brings BUY SHIP.
- **Sizes and skins:** 3840x2160 and 1280x800; `ui_style` HUD 2 skin on, then off (classic). The strip must not stand at the dealers, or LB / RB would be its tabs: `views::HudNNStrip` false in both.

## Steps
Each step: `bases` (`focused=`, `screen=`, the prompt bar's status), `Get-FLEventLog`, a full-resolution frame.

| # | Do | Pass |
|---|---|---|
| 1 | First stage: D-pad to the row, hold A 0.6 s and **keep it down 2 s more** | `pad ship buy: clicked ShipTraderBuyButton`; the final stage opens; **no** `pad ship trade` line, the ship unchanged, the A pill does not fill. The danger this guards: a hold running on into a purchase |
| 2 | Release. Read the focus and the lane | `focused=ContinueButton`; pills SIDE, FILTER, A BUY SHIP with the hold mark, CLOSE; none over BUY SHIP or a panel, nothing cut, at both sizes |
| 3 | Cancel: A down 0.3 s, release | no click line, the credits and ship unchanged, the pill emptied, the window open |
| 4 | A tap, then a second tap | as step 3: BUY SHIP has no tap |
| 5 | RB, then LB | `pad dealer: side -> yours` / `side -> buy ship`; on a row the pill reads SIDE / BUY SHIP and A TRANSFER |
| 6 | The trade-in: on a mounted row, A held | `pad transfer: clicked BuySellTradeButton`; the item stands on the dealer's side (the dealer's rows rise: the player's list shows five rows of more, so its count cannot show it), SHIP PRICE falls (by frame); no purchase; the focus keeps its row |
| 7 | RB from the dealer's rows | three stops: BUY SHIP one press from each list (`NextStop`) |
| 8 | Back: focus on `BackToShipDealer` by the D-pad or the stick, tap A | the ship dealer again; the dealer's rows back to what they were before the transfer, `equipment` unchanged |
| 9 | Again to the final stage, on BUY SHIP, a fresh hold of A | `pad ship trade: clicked ContinueButton`; `uistrip data` names the new ship; credits = before + trade-in - price, read with `worth` (the trade-in is `NN_Dealer` +0x9FC, an integer; +0x9F8 is no offer price); the transferred item sold, the rest moved; the room back, the pad's prompts back; no fault line |
| 10 | Reload. Too few credits (fixture); open the final stage | no BUY SHIP (`visible ContinueButton` empty), the game's "You need $n more credits to purchase the ship."; `focused=ItemView01`, A TRANSFER, the pill SIDE alone |
| 11 | A held, focus off every row (a filter) | no click line, nothing bought |
| 12 | Transfer rows until the text goes | BUY SHIP appears; the pill reads SIDE / BUY SHIP; LB or RB reaches it; a hold buys, on a fresh save copy only |
| 13 | Close (B) from the final stage | the room, the old ship, the equipment as before |

## The right stick (issue 36)
Commodity dealer and equipment dealer, each with both lists overflowing: at Manhattan buy enough kinds of goods, or use a filter with more than five rows on both sides. Read each bar with `scroll Inventory_Scroll 0` and `scroll Inventory_Scroll#2 0` (`#n` = the n-th visible, so a hidden bar shifts the numbers: read `uiitems` first and map them to sides).

| # | Do | Pass |
|---|---|---|
| 14 | Focus on a row of the dealer's list, `Set-FLPad -RY -1` 300 ms | that list's bar moves, the other's stands still |
| 15 | LB / RB to the player's rows, the same | the other way round; the rows re-selected are the focused list's (`pad ...: selected`) |
| 16 | The left stick's cursor over the other list's rows, no D-pad press, the same | the list under the cursor moves |
| 17 | Focus on a filter or on BUY / SELL | the list last focused moves |
| 18 | A focused list that fits (its bar hidden), the other overflowing | neither moves |
| 19 | An overflowing description | the text first, then the focused list, as before |
| 20 | Single lists, unchanged: the inventory alone, the job board, the log, help, status (`tests\state_fixes.ps1 -Row rs` stays green) | RS moves them as in 0.3.5rc |

## Keyboard and mouse
At the final stage, by mouse: rows, TRANSFER and BUY SHIP as in the game; no pad prompt with the mouse last used. The wheel unchanged.

## Regression by reach
`tests\dealer_focus.ps1` (its walk runs a long list to the end; the filters are X's, not the D-pad's; Up from the first row stops on no mount box and no A follows, as a tap on the game's SELL trades; the prompt is read from `bases items=`), `tests\window_prompts.ps1` (nothing bought by walking), `tests\docked_text.ps1`, `tests\state_fixes.ps1 -Row rs` (its `BarPos` reads the first shown bar: with the focus on the other list it must read that list). The job board's hold once, with A held in from the room: it must wait for a release.

## Log lines and frames
`pad screen shipdealer -> ...`, `pad ship buy:`, `pad ship trade:`, `pad transfer:`, `pad dealer: side ->`. Keep frames of steps 2, 5, 9, 10, 14, 15 at both sizes and both skins. Judge frame AND log together; a pass by log alone does not count. Evidence: `research\experience\runs\ship_buy_<Label>\`, the run of record `research\experience\runs\ship_buy_p2d`.

## Pass
All of 1-20 at both sizes, skin on and off; a real purchase changes the ship and the credits by the game's own sum; no purchase without a full fresh hold on BUY SHIP. Any purchase in steps 1, 3, 4, 6 or 11 fails the test outright.
