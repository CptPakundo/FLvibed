# NeuroNet screens: online research (7 Nov 2026)

How modern AAA and good space games build Log, Inventory, Status, Info, Chat/Group and the tabbed hub, for FLvibed's in-flight screens. The owner's rules are taken as given (screen = window, full-screen, no frames; prompts show only what a press does now; contextual side panels; tabs on top on LB/RB; near-black ground, chamfered glass plates; K&M equal).

## How to read the claims

Every claim carries a tag and a source number. R and S are two separate numbered lists at the end, so [R 31] and [S 31] are different sources.

- **[R n]** read: the page was opened. Microsoft pages came back as full text. Other pages came back through the fetch tool's summariser, so a quote there is a summary's wording, not checked against the page.
- **[S n]** search result: a search summary or snippet only, the page itself was not opened (often because it refused the fetch).
- **[M]** memory: from my own knowledge, not checked in this session. Treat as a lead. Button names in [M] claims are the weakest.
- **[Rec]** my recommendation or inference for FLvibed, not a finding.

**What failed, plainly**

- **Game UI Database gave no data at all.** It answered 403 to the fetch tool, and in the browser pane it stopped at a bot-verification page, which was left alone. No screen in this file was seen in the database. An owner visit in a normal browser is the way to get those references.
- **No screenshots were viewed.** Every layout statement below is from text.
- **No developer talk was read.** Candland's GDC 2016 Destiny talk is known only from search summaries (the Vault page and the video were not opened).
- Refused or empty: Frontier forums, Medium (Destiny 2 character essay, BG3 console essay, Andromeda UX essay), GameSkinny, Paradox forum, Bungie help, Sea of Thieves site, RPGamer, Family Gaming Database, Neowin, Twinfinite, Factorio forum (403 / 410 / 500); GamesRadar, PC Gamer, TechRadar, Tom's Guide and Apple HIG returned only a headline.
- **Thin evidence:** Horizon's quest and Notebook screens, Red Dead 2 stats, Everspace 2's quest and pilot pages, Civilopedia on a pad, Elite's right-hand panel, X4's faction screen layout, and Warframe console chat. Their entries are mostly [M] or a single [S].
- **No source at all found for:** remembering the last tab or row, transition timings, hold duration in milliseconds. Those lines are [M] or [Rec].

Game facts below (mission structure, what can be mounted in flight, reputation thresholds) are not verified against the game; each is marked "verify".

---

## Findings that should change the plan

1. **Hold-to-confirm as the only path fails Xbox guideline 115.** The text: "Don't require button holds for confirmation of destructive actions. Provide alternate options to button holds in these scenarios." [R 5] A hold is fine as the default only with an option that replaces it (The Long Dark converts all hold actions to presses [R 3]).
2. **Right-stick scrolling cannot be the only way to scroll.** The UI must be fully navigable by digital input, and games should not require two analog sticks for a mechanic [R 3][R 4]. Text panes need a digital path too (LT/RT paging, or D-pad after engaging the pane).
3. **Tooltips are the wrong tool on a pad.** Microsoft says to avoid them on TV: they appear after a moment, stay briefly and vanish [R 9]. The contextual side panel, which follows focus and stays, is the correct form.
4. **Tabs must not shift.** Cyberpunk's top tabs move depending on which is active, which breaks spatial memory; the critique cites spatially stable interfaces against it [R 13].
5. **For a 48-row reputation list, one way of finding things is not enough.** "There should be more than one way to locate content" in complex sets such as quest logs and large inventories [R 4].
6. **Freelancer is not on Steam [M], so Steam's text-input API is likely unavailable.** Verified Deck play needs text entry that works with a controller only [R 8]; a built-in pad keyboard is the likely route.

---

## 1. LOG / journal

### What the best do

- **Split by kind, with a separate completed tab.** Cyberpunk 2.0: Main/Side Jobs and Gigs in tabs, a fourth tab of all completed missions marked with a red checkmark [R 15]. Starfield also splits Missions across tabs [S 31].
- **Show the cost of going there.** Cyberpunk 2.0 shows distance to each quest in the list [R 15].
- **Let the player stop tracking.** Cyberpunk 2.0 added untrack in the Journal, which also removes the automatic Main Job tracker [R 15].
- **Connect the list and the map.** Halo Infinite: picking a mission from a text list moves the map to it [R 4]. Starfield offers Set Course or Show on Map on non-local quests [S 31]. The Witcher 3 Remastered (2026): untracked quests have a distinct map marker and can be tracked from there [R 14].
- **Bundle notifications.** Witcher 3 Remastered combines quest updates into a single HUD notification [R 14].
- **Witcher 3:** left list grouped by type with suggested level, right pane with journal prose, objectives under it [M].

### What reviewers criticise

- **Everything kept, little to filter.** Cyberpunk at launch: the log stores all you find with limited filtering [S 30]; "tedious" [S 30].
- **Hidden state.** Kotaku on Starfield: the current mission is not labelled as a mission; a screen of visual noise overwhelms [R 12]. The Show All Targets prompt reportedly shows the opposite of the current state [S 31].
- **Depth.** Andromeda: missions are "in a menu within a menu" [S 32]. Mass Effect 3's journal: called crowded, and PC Gamer's headline ranks it worse than the ending [S 33].
- **Too many categories.** Horizon Forbidden West: a long category list clutters the menu; one opinion piece asks to merge near-identical lists (weak source) [S 34]. Another review praises the split as easier to glance over [S 34]. The difference between the two is whether the groups mean anything to the player [Rec].
- **Fixed small windows.** Elite: Transactions reportedly shows only seven missions at once, so counting free slots needs scrolling, and missions cannot be matched between Transactions and the board [S 35]. Odyssey removed information players relied on, such as jurisdiction on illegal missions [S 35].

### Patterns worth copying [Rec]

- A **"Now" plate at the top**, labelled in words: mission name, the one current objective, distance, target system. It answers the question that made Starfield fail [R 12].
- **History as a timeline** grouped by mission or chapter, newest first, on the same screen below it or in a second sub-tab. Freelancer's story is linear with few active jobs (verify), so a multi-quest tracker would be weight without use; do not copy Witcher's categories for their own sake.
- **A on an objective opens it on the star map**, returning to the same row on B, using the just-rebuilt map as the Halo Infinite pattern [R 4].
- **Reading pane on the context panel**, only when the row has prose. Rows carry status in word plus glyph, never colour alone [R 2].
- Filters only if the history is long enough to need them: All / Story / Jobs, one press to cycle.

### Pad schemes seen

| Game | Scheme | Tag |
|---|---|---|
| Cyberpunk 2.0 | Tabs by job type; D-pad navigation improved in 2.0 after the launch cursor drew complaints | [R 15][S 30] |
| Starfield | Tabs; press to set active; a separate key toggles all targets | [S 31] |
| Witcher 3 | Stick list, A track, one button to show on map | [M] |
| Elite | Bumpers move tabs in a panel; A selects | [S 36] |

---

## 2. INVENTORY

### What the best do

- **Slot first.** Everspace 2: pick a slot, get every compatible part. Slots: four weapons, core, shield, plating, sensor, thrusters, cargo unit, four consumables [R 16]. This maps directly onto hardpoints.
- **Compare on focus, no equipping needed.** Cyberpunk 2.0: most important stats as bars; focusing another weapon adjusts the bars and shows differences [R 15]. Everspace 2: red and green numbers, and a shop marker on items better than the current one [R 16]. Starfield's opposite drew complaints: having to equip weapons to compare them [R 17].
- **Character slots around the model, flyout per slot.** Destiny 2: weapons one side, armour the other, focus a slot to open a small grid of alternatives, stat changes shown in the detail card [M]. Nine stored per slot in a 3x3 grid [M]. The critique of the screen praises hints that a cluster holds more [S 37].
- **Preview before commit.** Cyberpunk 2.0 previews clothes and weapons in the inventory, not only at vendors [R 15].
- **Density and columns.** What StarUI added to Starfield is the best public list of what a dense list needs: compact rows, configurable columns, sort by column, category icon before each name, combined mass per category, Page Up/Down/Home/End, mass transfer of whole categories, a larger scrollbar, adjustable font size, one key straight back to the game [R 18]. Bethesda later added tabs to container menus [S 38].
- **Radial for rare or quick access.** BG3 on pad: a large radial for map, journal, spellbook, character sheet and so on; a light trigger pull peeks, a firm press locks it [R 19]. A reviewer rated this the best gamepad inventory approach so far, though still less smooth than mouse [S 39].
- **Elite:** cargo panel with jettison per item or all; outfitting list by slot with stat deltas [M].

### What reviewers criticise

- **Cannot tell what is equipped.** Starfield: the equipped item looks no different from the category headers, labels run together; about nine data points per item; unexplained abbreviations; ammo not matched to guns [R 12].
- **Everything through one list, one tab at a time.** Starfield: categories do not fit without scrolling; each tab has to be opened [S 38].
- **Confusing names.** Cyberpunk: "Inventory" showed the character while the item list sat a level down in "Backpack", which caused misclicks [R 13].
- **Equipped items reshuffling the grid** [R 13]. Stable order matters; a row must not jump away from the focus after an action [Rec].
- **No bulk.** Cyberpunk: no keep/sell/dismantle marks, long hold per dismantle, stack needs a slider instead of Sell Stack, slider should default to max [R 13].
- **Sort not context-aware.** "Sort by DPS" offered on clothing [R 13].
- **No favourites.** Andromeda: loadouts cumbersome [S 32]. Polygon: "still a disaster" with tiers and mods [S 32].
- **Shuffling.** Everspace 2: constant loot juggling [S 40].

### Patterns worth copying [Rec]

- **One screen, two lenses: Loadout and Cargo**, stepped by a sub-tab or a button, sharing the detail panel. Loadout lists hardpoints by type with the fitted item and an empty state that reads as empty.
- **Hardpoints shown on a ship outline only if each slot stays reachable in six or fewer steps** edge to edge [R 9]; a list of slots must exist either way (Halo's list-next-to-map logic [R 4]).
- **A on a slot opens the compatible list** (Everspace), with signed deltas per stat. Delta is arrow plus sign plus number, not colour alone [R 2]. Only show the stats that change, keep the full card behind the panel (Kotaku's nine data points [R 12]).
- **Equipped state differs in shape, not only tint**: a glyph, and a section of its own at the top.
- **Group headers show counts and totals** (StarUI's category mass [R 18]): cargo hold used/total, value per group.
- **Sort and filter cycle with one press each, options per group**, so no meaningless sort appears [R 13].
- **Row actions in a small action sheet**, not nested focus targets in the row: nested UI suits XY navigation badly, and a flyout is Microsoft's fix [R 10].
- **Jettison flow:** X on a row, quantity stepper (LS or D-pad, plus All), a line stating what happens ("Drops 12 Boron into space"), then the confirm. A warning when the item is mission cargo (verify whether that exists). The owner's rule of showing only what a press does applies: the confirm prompt says "Jettison 12", not "Confirm".
- **Mounting rules come from the game.** What can be mounted in flight or only at a base must be read from the game, not designed here (verify); unavailable rows stay visible with the reason, at 3:1 contrast or better [R 2].

### Pad schemes seen

| Game | Scheme | Tag |
|---|---|---|
| Destiny 2 | Free cursor on LS, LB/RB between screens, hold to dismantle or transfer | [S 41][M] |
| Cyberpunk | Tabs, long hold to dismantle; cursor complaints at launch | [R 13][S 30] |
| BG3 | Trigger radial plus cursor mode | [R 19] |
| Everspace 2 | Slot select then list; controls not rebindable on PS5 | [R 16] |
| Starfield | Tab per category, list | [S 38] |

Note on the cursor: reporting on Candland's talk says Bungie chose a free cursor against console custom, and GamesRadar called it a "genius move" [S 41]; a piece against mouse-style cursors exists [S 42], and Cyberpunk's reviewers wanted it dropped [S 30]. The memory note says the owner already has a pad cursor option. Microsoft's advice: XY focus by default, mouse mode only where a surface needs it, such as a map [R 10].

---

## 3. STATUS

### What the best do

- **Tell the player what a standing means.** X4 is the strongest model for a long faction system: internal -1.0 to 1.0, shown as -30 to +30 through a log formula, with named thresholds and effects at 30, 27, 20, 10, 0, -9, -19, -24, -30 (for example 20 = ally, capital licences, 9-12% discount; 10 = friend, military licences; -19 and above = not attacked) [R 20]. It carries the same internal range as Freelancer's attitude. Version 8.00 added Factions and Relations menus for overview [S 43].
- **Named tiers beat percentages.** Elite ladders use nine named ranks (Harmless to Elite; Penniless to Elite) [S 44]. New Vegas titles come from two separate scales, fame and infamy [S 45]; a 4x4 grid of names such as Idolized or Vilified [M].
- **Explain the consequence.** The Outer Worlds: separate positive and negative percentages in four tiers, each pairing gives a merchant effect from a 25% discount to a 50% markup with attack on sight [R 21].
- **Progress to next.** Elite shows progress toward the next rank [M]; rank and progress are logged as separate values in the journal [S 44].
- **Stats as a page you browse, not a dashboard.** Red Dead 2's Player and Compendium pages list long stats [M]; Cyberpunk's Character page holds attributes [R 13].
- **Theme helps the top-level page.** Cyberpunk's character screen, with ribbon cables and illustrated icons, was judged its strongest menu [R 13].

### What reviewers criticise

- Starfield's abbreviations and dense ship data read as a stress response [R 12]. Starfield's skills: five clear sections "mucked up by dozens of tiny icons" [R 12].
- Odyssey's statistics reportedly do not show prestige rank progress [S 44]. A stat that stops being kept current reads as broken [Rec].
- Decorative "FUI" elements in the same red as functional data bury the data [R 13]. Decoration needs a colour and weight of its own [R 13].
- Starfield: no faction standing screen was found in this search; that is a gap in results, not proof [S 31].

### Long reputation list: patterns [Rec]

Basis: 48 rows, TV viewing, XY focus, one direction of scroll, more than one way to find [R 4].

- **Default sort by what matters now**: factions in the current system first, then those hostile to the player, then the rest by group. Sorted alphabetically it is a phone book.
- **Group headers**: House, corporation, guild, criminal (use the game's own affiliation data). Sticky header while scrolling. Each header shows a one-line summary (e.g. "9 factions, 2 hostile").
- **Row**: faction name, diverging bar centred on 0, signed value, band word. The band word and bar position carry meaning without colour [R 2]. The band edges come from the game's own hostility rules (I recall about -0.6 and +0.6 [M], verify). Do not invent X4's many tiers unless the game has effects at them.
- **Trend marker**: an arrow on rows changed since the last view, which doubles as the sticky "changed" marker.
- **Filters on one button**: All / Hostile / Neutral / Friendly. Sort on another: Relevance / Standing / Name. Group jump on LT/RT (see the caveat on triggers below) so 48 rows never need 48 presses; list paging is the Microsoft default [R 10].
- **Context panel on focus**: what this standing does in play (attack, docking, trade), where the faction operates, who are its allies and enemies, recent causes if the game records them. Faction text from the game's own infocards.
- **Hidden rows**: factions the player has not met could be listed as unknown or hidden (a decision for the owner; X4 and Elite only list what you meet [M]).
- **Top of the page**: name, rank, money, net worth, ship, kills: a plate of few large numbers, the rest in a browsable list below. Net worth trend only if the game keeps it (verify).

### Pad schemes seen

| Game | Scheme | Tag |
|---|---|---|
| Elite | Panel tabs on bumpers | [S 36] |
| Outer Worlds | Faction page in the character menu | [R 21] |
| New Vegas | Pip-Boy sub-tabs; faction list then detail with title | [M] |
| Red Dead 2 | Pause page, list to detail | [M] |

---

## 4. INFO / help

### What the best do

- **Concepts as linked text.** Stellaris 3.8: terms in tooltips, shown in a fixed teal, open further terms; the aim is short tooltips with depth for those who want it [S 46]. Concepts can contain Concepts [R 22].
- **A search over the same store.** Stellaris 4.0's Databank: search returns all matches with the most relevant first, categories, a link out to the wiki, and the help button opens it [R 22][S 46]. Before it, players complained there was no in-game encyclopedia [S 46]. A Factorio forum post describes the same stacked-tooltip pattern, citing Age of Wonders 4 and Stellaris [S 46].
- **Primary and secondary entries.** Mass Effect's codex groups by category, entries marked Codex or Secondary [S 33]; primary entries narrated in ME1 [M]; the journal shows what you have unlocked [S 33].
- **Glossary inside the notebook.** Horizon's Notebook holds tutorials and a glossary with collected datapoints [M].
- **Help about the screen you are on.** Elden Ring has "Menu Explanation", a help line about the focused item [S 47][M]. That is the exact idea of a Current Information page.
- **Distinct forms.** Deus Ex: Mankind Divided gives lore types a look of their own; Cyberpunk's shards use one icon and style for everything, so nobody knows what is inside before opening it [R 13].

### What reviewers criticise

- One look for all lore [R 13].
- Help that rots: Stellaris' in-game help blamed for lagging its reworks [S 46]. FLvibed's generated help must follow the build [Rec].
- Text styled as a button that is not one (Cyberpunk's "Crafting Spec") [R 13]. Links have to look like links, and non-links must not [Rec].
- Civilization VI on Switch: reviews praise the port of a text-heavy UI to a pad, one calling it "fully translated" [S 48], another "bumpy, cumbersome" [S 48]. Nothing on the Civilopedia itself was found.

### Patterns worth copying [Rec]

- **Open on where the player is.** Current system, base or ship first, as the game's page does; a short list of siblings at the side (This System / This Base / This Ship / Game Help).
- **Reading column at a fixed measure**: at most 80 characters per line, line spacing 1.5, left aligned, sentence case [R 1]. Never full width at 4K.
- **Text pane without focusable items** is the known trap: Microsoft lists it as a problem and offers engagement or a tab stop [R 10]. Right stick scrolls the pane while focus stays in the list; digital alternative as above.
- **Keywords as focusable links only if the text has them**: LS/D-pad steps links, A follows, B returns along a history stack. Colour plus underline, one meaning (teal in Stellaris [S 46]).
- **Search**: Y is the convention for search on Xbox [R 9][R 10], opening with the keyboard already up [R 9]. Worth it only for a full encyclopedia, not one page.
- **Help page**: glyphs from the active device, in the current binding; after a remap all hints must show the new binding [R 3].

### Pad schemes seen

| Game | Scheme | Tag |
|---|---|---|
| Stellaris | Mouse-driven on PC; hover and click on concepts | [M] |
| Mass Effect LE | Stick list, A open, stick scroll | [M] |
| Horizon | Tabs on bumpers, RS scroll in text | [M] |
| Elden Ring | One button toggles item explanation | [M] |

---

## 5. CHAT and GROUP

### What the best do

- **Text entry wherever chat exists**, and everything readable there [R 6]. Sea of Thieves: box at top left, focus opens the on-screen keyboard; Gears 5: RS click opens it [R 6].
- **Quick chat first on a pad.** Microsoft: typing on a pad during play is cumbersome, so offer short predefined replies. Their worked scheme: D-pad direction opens a category, the same directions pick the reply, two seconds of inactivity closes it, bumpers change categories, Y opens the keyboard [R 7]. Default to a category that fits the moment; keep phrases short, concrete, varied [R 7]. Sea of Thieves has a chat radial of contextual messages [S 49]. The category bumpers conflict with nothing in flight, but on the hub they are reserved [Rec].
- **Actions start from a person.** Elite's Contacts tab: pick a contact, get comms and wing options; invites arrive under Inbox with accept and decline; both must be in the same instance [S 36]. Destiny's Roster: pick a name, Invite to Fireteam; tabs for platform and all players; per-player privacy toggles for invites [S 41]. Warframe: messages open from a name in any UI menu [R 23].
- **Channels as tabs.** Warframe: Squad, Relay, Clan, Region, and so on, with include and exclude filters, chat commands, whispers and a local Notes tab [R 23]. Elite: Tab cycles direct, local and wing [S 36]. FFXIV pads: View cycles HUD windows including the log; D-pad up or down enters the party list [R 24].

### Numbers for the display (Microsoft PlayFab) [R 11]

- Minimum **28 px at 1080p** for chat, larger than the 24 pt minimum for other 10-foot UI, because it is on screen for a limited time.
- Example window **525 x 395 px at 1080p**.
- Newest message at the **bottom**, auto-scroll, no manual scrollbar during active play.
- Close after inactivity: example **15 s**, the reading time of a 280-character message.
- Keep the window open while paused.
- Distinguish speakers. Style it apart from game UI so chat cannot be mistaken for it.
- Position on a nine-cell grid, not pixel coordinates.

Duration must be adjustable up to at least ten times the default, or dismissable by input [R 26]; Halo Infinite offers 5 to 120 s, Gears 5 up to 60 s [R 26].

### What reviewers criticise

- Warframe: controller chat ignoring keyboard or mouse, chat disappearing once a squad forms, on PS4 still a keyboard step to send (old forum reports) [S 50].
- Elite: players cannot find the Contacts tab on a pad and are told to check menu navigation settings [S 36]. A player bought a wireless keyboard to type on Xbox [S 36]; Sea of Thieves players type through the phone app [S 49]. Pad typing is the universal weak spot.
- Roblox's console guidance simply says to disable the chat window [R 25]. Removing it is one accepted answer, though not for FLvibed's scope [Rec].

### Patterns worth copying [Rec]

- **One screen, people on one side, conversation on the other**, stepped LS left/right. Focused person gets a contextual panel with actions (Message, Invite to group, Leave, Kick) as the game allows them.
- **Group plate on top**, listing members with ship, system, health if available. Empty state tells how to invite.
- **In the hub, reply with a list of quick phrases plus Keyboard**; the Microsoft pattern, adapted.
- **Text entry paths:** (1) a hardware keyboard always works directly, (2) built-in pad keyboard with the same phrase row on top (Microsoft allows game-made keyboards with phrases [R 7]), (3) on Steam or a Deck the player's own overlay keyboard. Steam's API call needs Steamworks [R 27], so it is likely unavailable (see finding 6).
- **Auto-scroll stops when the player scrolls up**, with a "new messages" marker: XAG 117 requires a way to pause auto-updating content [R 28].
- **Multiplayer only:** the tab is present but states "Single player: no chat" or hides, by the owner's choice of consistency. XAG asks for repeated components in the same relative order [R 4], which favours keeping tab positions stable and greying the tab.

### Pad schemes seen

| Game | Scheme | Tag |
|---|---|---|
| Destiny 2 | Roster tab in Director; LS cursor; A on a name opens actions | [S 41] |
| Elite | Comms panel, bumpers change tabs | [S 36] |
| FFXIV | View cycles HUD windows, RB UI tabs, D-pad party list, direct chat by default | [R 24][S 51] |
| Sea of Thieves | Chat radial; text box on the screen | [R 6][S 49] |
| Warframe | Tabs; a button for the virtual keyboard | [S 50] |

---

## 6. The hub as a whole: cross-cutting rules

### Tabs and structure

- **Fixed tab positions, in a stable order, the active one marked in place** [R 13]. Same top position on all screens [R 4].
- **Consistent interaction**: A select, B back, LB/RB tabs, LT/RT pages, as in Sea of Thieves [R 4]. Microsoft's table: triggers page up/down, bumpers page left/right, Y search, View nav pane, Menu context [R 10]. The owner's reservation of View and Menu overrides the last two.
- **Triggers:** the brief reserves "shoulders". If that means LB/RB only, LT/RT are free and should page lists as above. If it covers triggers too, paging needs other buttons. To be decided by the caller.
- **B always backs out one level, and from a tab root closes the hub**; a persistent way back on all sub-screens [R 4]. Show no on-screen back button when B does it [R 10].
- **Tab wrap**: linear lists loop first to last [R 4]. Grids do not [R 4]. Offer a setting to turn looping off [R 4]. Whether LB/RB wrap at the ends follows the same rule.
- **Short path**: at most six steps edge to edge [R 9]; common tasks least presses [R 10]; one level of depth where possible (Andromeda [S 32]).
- **Put actions against the scroll direction**, not below a long list, or a list of 50 costs 51 presses [R 10].
- **Clear names**: name by what the player wants (Cyberpunk's Inventory / Backpack mix-up [R 13]). Keeping the game's "Inventory" and "Log" words is fine as they are accurate.

### Pause and world-keeps-running

- Spectrum: Elden Ring never pauses from menus, the character stays vulnerable, and pause sits in a hidden help screen [S 47]. Warframe pauses in Solo [S 50]. Most single-player AAA pauses full-screen menus [M]. BG3 and Red Dead slow or peek for quick radial use: BG3's light pull peeks [R 19], Red Dead's wheel slows time [M].
- The owner memory says the autopilot stops only for the game's own pause and no window does. So the hub runs with the world live by default [Rec, from that memory].
- That makes these necessary [Rec]:
  - A **live state strip** on every tab: shield/hull, threat or "under attack" cue, autopilot state. Elden Ring players are told to prepare before opening menus because nothing warns them [S 47]. A full-screen hub over a live world has the same risk.
  - **A word "PAUSED" or "LIVE" in the hub's fixed header**, so the state is never hidden. The game's pause should be reachable from the hub.
  - A note that time limits outside core play need warnings [R 26], which is unaffected as mission timers are core.
  - Moving background behind text: XAG 117 asks for a way to disable it [R 28]. Near-black ground already does this.

### Focus and scroll

- **Exactly one focus visual on screen at all times**, and it must be unmistakable from a distance [R 10]. No text-only pop-up with nothing focusable [R 10].
- **Initial focus at top left** on a page or on the first likely action [R 10].
- **Keep focus away from the edges of a scrolling list** so the next row is visible; Microsoft centres the focused item in one example [R 9], and a Roblox thread shows scrolling to the exact edge hides what comes next [S 52]. Row at about the centre or with one row of lead [Rec].
- **Lists may run off the screen bottom** while focus stays in the safe area, showing that more follows [R 9].
- **Sliders and steppers need engagement**, else focus gets trapped [R 10]. Single presses only, no holding A while adjusting [R 3].
- **Modal sheets dim the rest** with a smoke layer [R 10]; warnings distinct from other text [R 5].
- **Remember the last tab and row per tab** across open and close, within a session at least. No written source was found; CommonUI's activatable widgets restore focus this way [M]; the CommonUI pages read cover only the Nav-bar and Back action (below). Reset to the top when the list's contents changed so that the old row no longer exists [Rec].
- **Reflow**: if scale changes the layout, focus order must follow; never two-axis scrolling [R 4][R 1].
- **Sound and haptics on navigation** help pads: selection, end of list, disabled button [R 25]. Check against the owner's rules on sound during tests.

### Prompts

- **Same place, same relative order**, even when the number of prompts changes [R 4]. The owner's rule (only what works now) stays compatible: A slot first, then X, Y, B last, skipping absent ones [Rec]. CommonUI's Nav-bar sorts actions left to right by a priority value [R 29].
- **Labels to the right of glyphs**, always shown [R 9].
- **Glyphs**: match the active device only, never K&M glyphs when a pad is active [R 8]. Text inside a glyph meets the minimum text size, and the line grows to fit the glyph, not the reverse [R 1]. Must follow remaps [R 3]. Build glyph swapping in early [R 33].
- **Do not invert meaning**: a toggle prompt names the action it will do, plainly (Starfield's Show All Targets [S 31]).

### Destructive actions

- **Offer review, confirm and undo** for selling, overwriting, and the like; Fallout 76's two-step, each with cancel, is Microsoft's example [R 5].
- **Button holds cannot be required** [R 5]. Holds of 2-3 seconds count as prolonged [R 3]; a long press below 3 seconds is an accepted alternative form for gestures [R 3].
- **Default scheme [Rec]**: hold with a visible fill about 0.6-1.0 s (no source for this number), or a two-step A then A dialog when the "no holds" option is on. Never hold for frequent actions: Cyberpunk's per-item dismantle hold drew fire [R 13]. Focus defaults to the safe choice in a dialog [Rec].
- **Mouse**: activate on release, moving away cancels [R 3].
- GAG lists a 0.5 s cool-down between inputs as a guideline title [R 26] (not opened). It could protect a two-step confirm from a double press [Rec].

### Sticky "new" markers

- Evidence is mostly from failures, in Battlefield 6 forum reports [S 53]:
  - viewing an item clears it but the parent dot stays;
  - dots on parent and child disagree;
  - state resets after a restart;
  - players ask for Mark All As Viewed.
- Rules [Rec]: a marker rolls up from row to group to tab; clearing the last child clears parents in the same frame; the state is saved; one action clears a page. Everspace 2 has a mark-all action [R 31]. Destiny clears a marker on hover [M].
- Flash cues stay brief and under 3 Hz by the owner's HUD rule, and blinking needs an off switch [R 28].

### Legibility numbers

| Measure | Value | Source |
|---|---|---|
| Console body height (ascender to descender) | 26 px at 1080p, 52 px at 4K | [R 1] |
| PC body height | 18 px at 1080p, 36 px at 4K | [R 1] |
| Scaling | to 200% of minimum, no loss; glyphs too | [R 1] |
| Steam Deck | floor 9 px character height at 1280x800, aim 12 px, readable at 30 cm | [R 8] |
| UWP 10-foot font size | 15 epx main, 12 epx supplemental, 200% scale at 1080p (30 / 24 px) | [R 9] |
| Interactive element | at least 32 epx tall (64 px at 1080p) | [R 9] |
| Safe area | essentials outside 5% of edges (96 px sides, 54 px top and bottom at 1080p) | [R 9] |
| Chat | at least 28 px at 1080p | [R 11] |
| Contrast | 4.5:1 standard, 3:1 large or inactive, 7:1 in a high-contrast mode; measure on the lightest part of the background | [R 2] |
| Line width | at most 80 characters | [R 1] |
| Line spacing | 1.5; paragraph 2x | [R 1] |
| Letter / word spacing | 0.12 / 0.16 of font size | [R 1] |
| Case and alignment | sentence case for lines of text; left aligned | [R 1] |
| Density | what a phone shows, not a desktop | [R 9] |

Notes:

- XAG's px is a measured height, while UWP's is a font size; they are not the same scale.
- Scaling the console figure gives about 19 px at 800 p, and the PC figure about 13 px [Rec arithmetic]. Valve's 12 px is well under what XAG would want at a TV, though a Deck is at 30 cm. The memory note says 1280x800 means the Deck, so 12 px is the floor and larger the aim.
- Text size "anecdotally" trips many Deck titles and "make all text bigger" late is messy [R 33]. Thronefall went from Playable for small text [R 30]. Everspace 2's 2026 update enlarged many UI fonts for the Deck before Verified [R 31][S 40].
- Glass plates: measure contrast over the brightest area that can show through [R 2]. An opaque-plate option helps [R 28].
- High-contrast mode at 7:1, and text colour choice, are basic to intermediate items in GAG and XAG [R 2][R 32].

### A pad scheme for the hub [Rec]

Works for all six tabs, built from the above, within the owner's reservations.

| Input | Action |
|---|---|
| LB / RB | Previous / next tab (fixed) |
| LS or D-pad | Move focus |
| RS | Scroll the detail or reading pane; focus stays |
| A | Open or do the row's main action (on release with mouse) |
| B | Back one level; at root, close the hub |
| X | Row's secondary action or action sheet |
| Y | Filter or search on the page |
| LT / RT | Page list or pane, or jump between groups (if triggers are free) |
| RS click | Sort (stick clicks are hard to hold [R 32], so press only; remappable [R 3]) |

K&M equal: mouse hover sets focus and click activates on release; wheel scrolls under the pointer; Page Up/Down/Home/End on lists [R 18]; Tab/Shift+Tab or Q/E for tabs; Esc back; column header click sorts; typed text in search; one key straight back to the game [R 18].

---

## Sources

**Read (opened; see the note at the top)**

1. XAG 101 Text display: https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/101
2. XAG 102 Contrast: https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/102
3. XAG 107 Input: https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/107
4. XAG 112 UI navigation: https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/112
5. XAG 115 Error prevention: https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/115
6. XAG 119 Communication: https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/119
7. PlayFab Party text-to-speech and text input UX: https://learn.microsoft.com/en-us/gaming/playfab/features/multiplayer/networking/party-text-to-speech-ux-guidelines
8. Steam Deck compatibility review: https://partner.steamgames.com/doc/steamdeck/compat
9. Designing for Xbox and TV (archived UWP doc, 2020): https://learn.microsoft.com/en-us/windows/apps/design/devices/designing-for-tv
10. Gamepad and remote control interactions: https://learn.microsoft.com/en-us/windows/apps/design/input/gamepad-and-remote-interactions
11. PlayFab Party speech-to-text and text display UX: https://learn.microsoft.com/en-us/gaming/playfab/features/multiplayer/networking/party-speech-to-text-ux-guidelines
12. Kotaku, Starfield menus: https://kotaku.com/starfield-release-menu-ui-starmap-inventory-xbox-review-1850797661
13. Interface In Game, Cyberpunk 2077 UX/UI critique: https://interfaceingame.com/articles/cyberpunk-2077-ux-ui-critique
14. RPG Site, The Witcher 3 Remastered patch notes: https://www.rpgsite.net/news/21482-the-witcher-3-remastered-launch-day-patch-notes-ps5-pro-xbox-play-anywhere-steam-pc-nintendo-switch-2
15. Game8, Cyberpunk 2.0 changes: https://game8.co/games/Cyberpunk-2077/archives/427117
16. Gert Lush Gaming, Everspace 2 PS5 review: https://gertlushgaming.co.uk/everspace-2-review-playstation-5/
17. CharlieIntel, Starfield UI mods: https://charlieintel.com/starfield/starfield-players-resort-to-mods-to-fix-trash-ui-272576
18. MP1st, StarUI feature list: https://mp1st.com/news/starfield-ui-mod-provides-much-needed-improvements-to-inventory-system
19. PlayStation Blog, BG3 on DualSense: https://blog.playstation.com/2023/09/05/how-baldurs-gate-3-adapts-its-expansive-rpg-gameplay-for-your-dualsense-controller/
20. Egosoft wiki, X4 reputation effects: https://wiki.egosoft.com/X4%20Foundations%20Wiki/Manual%20and%20Guides/Objects%20in%20the%20Game%20Universe/Factions/%28Player%29/Reputation%20Effects/
21. Gamepressure, Outer Worlds reputation: https://www.gamepressure.com/the-outer-worlds/reputation/z5cc78
22. Stellaris Dev Diary 367 mirror: https://devtrackers.gg/stellaris/p/8456ca90-stellaris-dev-diary-367-4-0-changes-part-1
23. Warframe wiki, Chat: https://wiki.warframe.com/w/Chat?oldid=2789384
24. Console Games Wiki, FFXIV Controls: https://ffxiv.consolegameswiki.com/wiki/Controls
25. Roblox console guidelines: https://create.roblox.com/docs/production/publishing/console-guidelines
26. XAG 116 Time limits: https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/116
27. Steamworks ISteamUtils: https://partner.steamgames.com/doc/api/ISteamUtils
28. XAG 117 Visual distractions and motion: https://learn.microsoft.com/en-us/gaming/accessibility/xbox-accessibility-guidelines/117
29. Unreal CommonUI quickstart: https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-quickstart-guide-for-unreal-engine (overview also read: https://dev.epicgames.com/documentation/en-us/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine; neither covers tab lists or focus restore)
30. SteamDeckHQ, Thronefall text size: https://steamdeckhq.com/news/thronefall-increases-text-size-on-steam-deck/
33. Blendo Games, planning for Deck verification: https://blendogames.com/news/post/2026-02-19-steamdeckverification/
31. Everspace 2 final update notes: https://everspace-game.com/?p=5475
32. Game Accessibility Guidelines: https://gameaccessibilityguidelines.com/full-list/ and https://gameaccessibilityguidelines.com/avoid-provide-alternatives-to-requiring-buttons-to-be-held-down/

Also opened, little used: FFXIV user interface (https://ffxiv.consolegameswiki.com/wiki/User_interface), Gamepressure Elite comms panel (https://www.gamepressure.com/elitedangerous/communication-panel/z67393), Roguey Elite UI (https://roguey.co.uk/elite-dangerous/help/ui), Ludo Destiny 2 controls (https://www.ludo.guide/guide/destiny-2/getting-started/controls-ui).

**Search results only (page not opened)**

- 30. Cyberpunk launch reviews and 2.0: https://criticalhit.net/gaming/cyberpunk-2077-review-blade-gunner ; https://gamingbolt.com/cyberpunk-2077-15-gameplay-features-it-needs ; https://cogconnected.com/feature/top-5-things-about-cyberpunk-2077-mad/
- 31. Starfield missions: https://starfield.wiki.fextralife.com/Missions ; https://twinfinite.net/starfield/a-starfield-option-can-help-with-quest-management-but-fans-still-want-more/ ; https://stealthoptional.com/article/how-to-set-the-active-mission-in-starfield
- 32. Andromeda: https://www.techradar.com/news/mass-effect-andromeda-review ; https://www.gameskinny.com/opinion/why-mass-effect-andromedas-ui-is-bad/ ; https://medium.com/games-ux/thoughts-on-mass-effect-andromedas-user-experience-7e8d2a684928 ; https://www.gamesradar.com/mass-effect-andromeda-critical-round-up-what-does-the-world-think
- 33. Mass Effect: https://pcgamer.com/mass-effect-3s-ending-isnt-as-bad-as-navigating-its-quest-log ; https://www.gamepressure.com/masseffect3/codex-entries/z03a66
- 34. Horizon: https://palegoldenrodsweet.pages.dev/posts/horizon-forbidden-west-s-sequel-should-consolidate-its-side-quest-categories (a mirror, weak); https://gamermatters.com/horizon-forbidden-west-ps4-review-open-world-buffet/
- 35. Elite: https://forums.frontier.co.uk/threads/qol-pass-for-the-mathematically-challenged.345677 ; https://forums.frontier.co.uk/threads/summary-thread-removed-and-missing-information-in-elite-odyssey.576454/post-9187793
- 36. Elite comms: https://elite-dangerous.fandom.com/wiki/Communications ; https://forums.frontier.co.uk/threads/first-day-and-cant-get-to-contacts-tab.292599
- 37. Destiny 2 character screen essay: https://medium.com/@saoliveri95/ui-of-destiny-2-the-character-screen-3463f8fc0e3a
- 38. Starfield inventory: https://steamcommunity.com/app/1716740/discussions/0/4035854109134673443 ; https://Gamespot.Com/articles/starfields-biggest-update-since-launch-arrives-on-may-15/1100-6523100/
- 39. RPGamer BG3 review: https://rpgamer.com/review/baldurs-gate-iii-review/
- 40. Everspace 2: https://xxlgamer.com/reviews/exit/178 ; https://steamdeckhq.com/news/everspace-2-gets-steam-deck-improvements/ ; https://www.gamingonlinux.com/2026/09/everspace-2-gets-a-final-tech-update-making-it-steam-deck-and-steam-machine-verified/
- 41. Destiny: https://gamesradar.com/destiny-2-menu ; https://gdcvault.com/play/1023460/contactUs ; https://www.trueachievements.com/game/Destiny-2/walkthrough/2 ; https://twinfinite.net/ps4/destiny-2-crossplay-how-invite-play-friends-xbox-playstation-pc/
- 42. GamingBible on cursors: https://www.gamingbible.com/features/games-console-games-please-stop-using-a-mouse-style-cursor-for-menus-20201203
- 43. X4: https://wiki.egosoft.com/X4%20Foundations%20Wiki/Manual%20and%20Guides/Objects%20in%20the%20Game%20Universe/Factions/ ; https://www.pcgamer.com/games/sim/x4-foundations-massive-diplomacy-update-adds-an-actual-embassy-room-to-your-hq-where-you-can-entreat-with-aliens-from-across-the-galaxy-and-establish-yourself-as-space-machiavelli/
- 44. Elite ranks: https://elite-journal.readthedocs.io/en/latest/Appendix.html ; https://hosting.zaonce.net/community/journal/v30/Journal_Manual_v30.pdf ; https://www.wikiwand.com/en/articles/Elite_Dangerous
- 45. New Vegas: https://fallout-archive.fandom.com/wiki/Fallout:_New_Vegas_reputations
- 46. Stellaris: https://admin-forum.paradoxplaza.com/forum/developer-diary/stellaris-dev-diary-292-3-7-4-released-and-concepts.1576269 ; https://www.gamingonlinux.com/2025/05/stellaris-biogenesis-and-free-4-0-phoenix-update-out-now/comment_id=278477 ; https://steamcommunity.com/app/281990/discussions/0/4299320351876822733 ; https://forums.factorio.com/viewtopic.php?p=615275
- 47. Elden Ring: https://www.gamespot.com/articles/elden-ring-can-you-pause/1100-6500990 ; https://www.tomsguide.com/news/elden-ring-has-a-hidden-pause-button-heres-how-to-access-it
- 48. Civ VI Switch: https://nintendoworldreport.com/review/49010/civilization-vi-switch-review ; https://www.theaureview.com/?p=102997
- 49. Sea of Thieves: https://www.seaofthieves.com/accessibility ; https://www.seaofthieves.com/community/forums/post/1366134
- 50. Warframe: https://forums.warframe.com/topic/657914-console-pc-controller-gamepad-suggestions/?page=23 ; https://support.warframe.com/hc/en-us/articles/38801911653517-Mission-Interface
- 51. FFXIV chat: https://consolegameswiki.com/wiki/Controls
- 52. Roblox scroll thread: https://devforum.roblox.com/t/gamepad-ui-highlight-improvements/1582185/9
- 53. Battlefield 6 dots: https://forums.ea.com/idea/battlefield-6-bug-reports-en/orange-dot-notifications-will-not-clear/13196010/comments/12966236

**Blocked, worth a visit by the owner:** Game UI Database, https://www.gameuidatabase.com/ (bot verification).
