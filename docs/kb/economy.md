# Economy at a base: repairs, purchases, the ship's value

What the game charges and what it counts as the player's net worth, as measured through FLvibed's own reads (debug
`worth`: `cash` and `assets` = the ship with its equipment and cargo, as the server values them; `equipment`: the
server's equipment list). Started 21 Oct 2026 from the sweep's `base-repair` section; grow it with every measured fact.

## Repairs (the base's "Repair your ship" window, `IDS_NN_REPAIR_YOUR_SHIP`, native `RepairOkButton`)
- **The repair price is not the ship value it restores** `[game, 21 Oct 2026, Manhattan, SaveC705 Starflier]`: 207
  credits charged for a value rise of 56; 398 charged for a rise of 201; 172 charged for a rise of 3 (with a destroyed
  shield generator, see below). The repair price and the net-worth value of hull / part points are different formulas.
  A test must not expect cash -> value 1:1 (purchases are 1:1, below).
- **Damage does not always show in `assets` while in space** `[game]`: after the same deliberate fight the value read
  7908 (= full), 7852 and 7605 on different runs; docked, after the repair, it was always consistent. Read the value
  docked when it matters.
- **A destroyed item is gone for good** `[game, 21 Oct 2026]`: pirates destroyed the fitted shield generator (`self`
  event reads `shield=-1.00` from then on, as with no generator fitted); the repair window does not list it, the value
  stays short by its price (7608 of 7908) until one is bought at a dealer. The server's equipment list (`equipment`,
  `mounted=1` rows) before and after a fight shows what was destroyed.
- **The window closes into a busy NavBar** `[game, 21 Oct 2026]`: a second `goto REPAIR` right after the first window
  closed answers `busy: navbar+34C`; wait for the room to be ready (`Wait-FLRoomReady`) as for every room click. A
  second visit with nothing damaged shows the toggle alone: `Invoke-FLBaseRepair` returns `nothing to repair`.
- **The window's controls and behaviour** `[game, 25 Oct 2026, PADFIX, SaveC707 hull 50 %]`: `NN_Repair` is a root of
  its own over the room (the NavBar stays visible and navigable beside it); hit-testable are `RepairCheckBox<n>` per
  damaged item (ticked on opening), `RepairToggleChecks` (tick / untick all), `RepairOkButton` (only with something
  damaged) and `RepairCancelButton`. OK charges (`ReqChangeCash -181`, `ReqHullStatus 1.000` for the hull alone) and the
  window **stays open** (closed by Cancel). The room's repair hotspot clicked again while the window is open closes it
  (a toggle). The pad (`padbases` screen `repair`): its own D-pad ring over those four kinds of control (the generic
  focus left the window for the NavBar), A = the focused control's own click, X tick all, Y repair, B cancel.

## Purchases (equipment dealer, native `BuySellTradeButton`)
- A purchase moves cash into ship value 1:1 `[game, 20 Oct 2026]`: cash -2800, assets +2800 (restock of nanobots and
  batteries); net worth - the story's money gates - is unchanged by restocking, only liquidity changes.
- Mounted launchers' ammo, nanobots and batteries are what the player refills at stops (`consumables.md` "Restock at
  stops vs loot"); the ship's `nanobots` / `batteries` limits are in `generated\ships.tsv`.

## Net worth
- The story's level gates (+3500 / 5200 / 7000 / 10000 ...) are net worth = cash + assets (`worth total`); the restock
  budget keeps `-Reserve` cash for the next repair (`Complete-FLBaseStory`).
- **Assets are valued by the goods.ini price, not by what was paid** `[game, 22 Oct 2026, Manhattan, SaveC701]`
  (`..\FREELANCER_RE.md` "Net worth = asset value + cash" has the formula): 1 Water bought at the local 15 credits ->
  cash -15, assets +30 (goods.ini price 30): **net worth +15**. A li_gun01_mark01 bought for 500 -> assets +150 (0.3 x
  price): **net worth -350**. Consumables / ammo / cargo count at full price (the restock line above: 1:1 because their
  local price was the goods.ini price). Consequences for the money gates (speedrun plan, `..\LEGS.md`):
  - buying a commodity where it sells **below** its goods.ini price raises net worth at once by the difference x
    units (`tools\flmarket.js --sells`: e.g. Light Arms at li01_10 140 under price, Diamonds at Freital / Ronneburg 462,
    H-Fuel at gas miners up to 294); above it lowers it;
  - selling cargo above its goods.ini price raises it by the difference; a trade's whole profit = sell - buy, split
    between the buy (price - buy) and the sale (sell - price);
  - equipment and ships cost net worth (70 % / 20 % of their price) - buy them after the gate has fired (the next target
    is set at the next mission's end). Resale price of equipment not measured yet (StrategyWiki: 30 %, = its value).
