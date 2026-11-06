# Changelog

## 0.4.0 (6 Nov 2026)

- **Haptics, on every pad** (built, not yet felt on a pad): one engine fed by the game. A near-quiet bed from your engine's own numbers; hits, blasts and collisions
  from the side they came; guns by their rate and damage; missiles felt before they are seen; the cruise charge and its release, lanes ring by ring, jump gates
  and holes; radiation, nebulas and asteroids. Maximum strength is kept for the biggest moments. Everything else in the mix follows Rumble.
- **Adaptive Triggers on a DualSense:** gun power and the thruster gauge are pull weight that thins as they drain, vibration while you fire and on the afterburner,
  limp when empty, a wall when you cannot fire. A held trigger settles so it does not tire the finger. **Trigger effects:** FULL, LIGHT, OFF.
- **Impulse Triggers on Xbox One and Series pads** on USB or the dongle: the trigger motors pulse with your guns and from the side of a hit. Over Bluetooth, the two
  main motors only.
- **Waveform on a USB DualSense,** on by default, off under Show all settings (Motors): its grips driven as audio, the speaker set to four channels in the Sound panel. If it cannot start, the motors take over and the log says why.
- Show all settings has a strength for each of Ship and engine, Weapons, Hits and danger, Set pieces and Prompts. Steam Input is never needed for any of it.
- Autopilot: docking at a base beyond the game's dock range no longer stops at once. It flies into range, then docks (the original game and mods built on it allow 10 km, HD Edition 50 km).
- On the original game and on mods built on it, a pad strafe up or down no longer keeps sliding after the stick is let go.
- Beside a newer FLSharp, FLvibed no longer refuses two of its cursor addresses. A game with no startup movies keeps FLvibed's core lines in `FLController.log`.
- **HUD size:** one row, AUTOMATIC, SMALL, NORMAL, LARGE. Small is close to the game's own HUD, Large is what Steam Deck mode was, and Automatic reads the
  machine. A saved Steam Deck mode moves into it.
- **Windows fit what they hold:** the dealers', ship dealer's, job board's and offer's panels are now sized from the game's own controls as they stand: side-by-side panels end level, the infocard takes the room they leave, a short list shortens its panel and an edge travels instead of jumping. A text wider than its box steps down, the job lines no longer touch, and the job board, news and inventory stand centred. Every list row places its lines by its own frame, so a price no longer crosses it and a line alone stands in the middle. Text boxes end on a whole line, the log and info text start under their title's rule, and both windows use more of the screen. Dealers refuse in HUD 2's amber line, as the ship dealer does; the ship's refusal and trade-in stand in the centre panel and its panels start at their headings. The room bar's underline stands on the current room's button and no longer slides, no highlight flashes on the bar while a window opens, and a dealer's panels and prompts no longer open offset. The prompts keep to the column beside the infocard, with a stacked pill where one is too wide.
- The native HUD no longer shows for one frame when a load reaches space, and the NeuroNet box's art no longer stands under a window in flight.
- The flight HUD covers less: hold and credits leave it, the ship panel stands only in a fight or formation, the target card is narrower and shorter, the
  consumables are small dark cells with no plate, the escorts a slim plate. A fight shows two context prompts at most.
- **The bottom of the reticle** (GitHub issue 28): the energy meter drains to the middle, the throttle sits inside the ring and fills from the middle out, and
  the speed stands under the meter. Every ring arc has a thin dark edge, so it reads over a bright planet.
- The target card and the threat list read the same distance as the game's own label, to the surface.
- Keyboard and mouse: one lit chip with its key names the running maneuver, and the whole row reads in words while the mouse is on it. The target card's
  buttons read NEXT and CLEAR, and up to four hostiles are listed while no target is held.
- The Neural Net's lines no longer run off the screen's edge at 1280x800.
- **The targeting display:** HUD 2 replaces the game's bracket, label and bars. A new target opens as a small card beside the ship, then settles into marks
  the ship wears: the attitude by shape as well as colour, two bars, name and distance. Inside the ring they move to a seat beside the cue.
- **The weapon group cue** (GitHub issue 26): a row above the ring with your groups, then M and T. The selected group is yellow; a mark fills when your target
  is in the centre and in its reach; M or T glows under a tick while locked, shows a thin bar while reloading, and goes amber with a slash when it cannot fire.
  `group_cue` turns it off.
- **Failing bars:** a bar running low breathes in its own colour, faster as it falls, always under 3 Hz; a bar that breaks blooms. The HUD's motion has a row:
  `skin_motion`, full, fades or none.
- The flight HUD's label reads NEURAL NET, the game's own name for it.
- **One strip of words on every NeuroNet window:** LOG, INVENTORY, MAP, STATUS, INFO, with CHAT and GROUP in multiplayer, in place of the box's icons. The open
  one is yellow and underlined, and a click switches straight to a window, also during a slide. A CLOSE chip stands at the lane's right end on keyboard and
  mouse. The five windows share one scrim.
- **The nav map's frame comes off, and words replace its icons:** PHYSICAL, POLITICAL, PATROLS, MINING, BASES, LEGEND, MISSION, LABELS, SYSTEM, SIRIUS and what a
  selection offers, in panels beside the map, each calling the game's own button. A button the game disables shows disabled. The map keeps its size for now: the grown map (a test switch, `navnew`) is built but off, and the frame and words stand around the game's own map.
- **The pad on the map has two zones:** the map itself (left stick, triggers, A, X, Y) and its panels (the D-pad walks every word, A presses, the left stick or B
  returns), so a press never means two things. RS click is SYSTEM / SIRIUS, LS click back to the ship's system.
- **The pad reaches every NeuroNet window:** under HUD 2, LB and RB step the strip from any of them, wrapping. Only the map had a pad route until now. The log's
  MISSION and LOG move to the D-pad's left and right.
- Loot the tractor refuses says why, in the game's own words (GitHub issue 33): HUD 2's card, or the Neural Net line.
- An alert card no longer lies over another part of the HUD, at any HUD size, and its words stay on it (GitHub issue 35).
- Names with an apostrophe, a curly quote or an accented letter read right on the pad's screens, not with a question mark: Death's Hand in the weapon group window (GitHub issue 37).
- Names in capitals keep their accents as capitals: GRAFENWÖHR, GROSSGLOCKNER. Unproven: not seen in game yet (`tests\apostrophe_game.md`).
- Autopilot: a mission ship whose name has an accent or apostrophe is found by its name, Tekagi's Transport and the Rheinland cruisers among them. Unproven: not seen in game yet (`tests\apostrophe_game.md`).
- Radiation, a ship on your tail and multiplayer lag have homes in HUD 2 (GitHub issue 27): an amber line in the nav block with the game's own radiation art,
  a mark on the ring at the ship's bearing, one small word. The game's icons no longer draw over the target card, the map or the pause menu.
- **The options page has a simple mode:** five tabs by what they belong to (HUD, CONTROLLER, SUBTITLES, AUTOPILOT, MORE) and the settings most players
  need, with no scrolling on a Steam Deck at the Large size. Your settings stay exactly as they are. A hidden setting you changed comes back on its tab under
  YOU CHANGED, and **Show all settings** on MORE lists the rest. Each feature's switch is the first row of its tab, and off greys what stands under it; a
  switch that needs a restart says so on the row, in amber. The page's text now follows HUD size, and the description stands in the page, under the rows.
- **Page controls:** DEFAULT (X / D), CANCEL (Y / C) and CLOSE (B / Esc) carry their buttons, Q / E and LB / RB change the tab, and a held direction repeats
  after 0.4 s. Esc on the hosted page now goes back to the game's Options column instead of leaving the screen.
- **New rows:** Button hints and Hit effects, one word each over several settings, reading CUSTOM when you change a part of them; Window text size. Steering
  response is a word now (the number is under Show all settings), Stick dead zone reads in percent. A on Layout shows every button of that layout.
- **Disabling the Controller and Reset Everything now ask first,** with the safe answer focused. The rule that one of Subtitles and Controller stays on no longer
  refuses you: disable both and the subtitles simply stay hidden.
- **First start:** a fresh install meets one card over the main menu with HUD size, Subtitles, Hit effects and Held buttons; closing it is the skip. A player
  upgrading never sees it.
- HUD 2: the focused word on the pause menu, the job rows and the dealers' rows now reads in the bright text colour on its yellow block, so the row you are on is as legible as the rows around it.
- HUD 2: the quit, delete and exit questions and the game's error and please-wait boxes are one centred dialog that says what it does (the multiplayer prompt keeps the game's look): QUIT ANYWAY, KEEP PLAYING, DELETE, KEEP SAVE. B and Escape answer safely. An amber edge marks what can lose progress. Any other text keeps the game's own words.
- Pad: deleting a save is a press of X into the game's confirm, no longer a one-second hold.
- HUD 2: **CONTINUE** on the main menu: one press loads your latest save. The menu's buttons are floating chips, the front load screen's too.
- HUD 2: a new loading screen: a navy screen and, where the game gives it, where you are going, else LOADING; a bar only if the load runs past a second and a half.
- HUD 2: its own cursor, an arrow, a reticle and word bubbles for all 19 of the game's cursors; tooltips and popups in HUD 2's look.
- HUD 2: at 800 lines the names in the dealers' and inventory rows stand one size larger, the same size for a whole visit. Large keeps text by role in the windows. The log, status and info windows share one top line, left edge and dimmed backdrop, and INFO's plate ends at its text.
- Pad: a tap on the dealer's own BUY or SELL does the trade, as an alternative to the hold on the row; the prompt says which.
- Pad: a ship can be bought without a mouse (GitHub issue 34). The ship dealer's final stage opens on BUY SHIP: hold A, as for any purchase. On a row the hold is the game's TRANSFER, and LB / RB reach BUY SHIP from either list.
- Pad: a button still held from the screen before no longer counts on the new one: release it first.
- Pad: at the dealers a held A on a filter no longer trades the row selected behind it. The prompt reads SELECT there.
- HUD 2: the ship dealer's final stage sets its words on a plate, and the prompts keep off them.
- Subtitles: the fonts no longer rebuild every frame while a line and the Neural Net feed show together.
- HUD 2: **a long pilot name reads whole,** past the game's own 23 characters, where the game's data can finish it.
- HUD 2: the reticle goes with the rest of the HUD in a jump, a lane's entry and a dock's scene.
- HUD 2: close to the reticle, the target's bars and words stand centred on top of it, just over the ring when no weapon group row shows, over an alert that stands there. With the ring faded they stay with the target.
- HUD 2: the panels fit what they show, with even space around it: the target card, the ship panel, the loadout, the alert card and the contact rows. The consumables stand closer to the loadout.
- HUD 2: the prompts beside the bottom panels mirror each other on the centre where the room allows, and a row of pad prompts stands centred under the screen where its band is free.
- HUD 2: the reticle's bottom is now the boost meter, always there: yellow, emptying to the middle, breathing and blooming when it runs out as the hull does. The percentage stands beside BOOST.
- HUD 2: the cruise meter stands at the reticle's top, in cyan, only while cruise charges or runs. Its glow grows as it charges; running, it breathes at 1 Hz; turned off, it blooms out gently. Disrupted, it turns red, breathes fast and empties the moment cruise can charge again.
- HUD 2: the power meter moved inside the reticle's top.
- Dealers: the stats line up with their labels and lose their scroll bars; prices, classes and long names no longer run into each other or out of their rows; the FAIR / GOOD word gives way on a long name, the coloured mark stays. The quantity block rises into the slider's room when there is no slider.
- HUD 2: the prompts stand in a centred column balanced on the dealer's bottom panel, and bottom centre in rooms where the row has the room.
- The Neural Net log, help and status windows stand centred between the docked strip and the prompts.
- HUD 2: the on-target firing cursor turns again, as the game's does, at the rate of the game's data.
- Pad: HUD FADES / HUD ALWAYS on the action wheel, the options page's setting, live and saved.
- HUD 2: the target's bars and distance no longer vanish for good after a wide alert card has gone.
- Pad: a mapper's keys are dropped again on a PC that refuses a hook by DLL.
- HUD 2: a new alert card no longer lands on the one sliding out of its place. Under `skin_motion` Fades and None nothing on the HUD pulses or slides.
- Pad: a dealer's first row is no longer missed when the window is still sliding in.
- Pad: at the dealers the D-pad no longer stops on the room's bar, the NeuroNet's box behind the window or the description's scroll bar, and LB / RB bring the focus back from a filter when the other list is empty.
- Pad: at the dealers the right stick scrolls the list you are on, by the D-pad or the cursor, and no longer the other side's (GitHub issue 36).
- HUD 2: a window the skin cannot draw keeps the game's look until it closes, and one line says so. New rows: **Menu motion** (`skin_motion`) and `skin_native`, a list of windows that keep the game's look.

## 0.3.5rc (test build)

A test build of 0.3.5: every change below, not yet through the full regression sweep and gate. Short checks only: unit tests, the module matrix, one start each of flight, a dock, a subtitle and a movie.

- **PlayStation and Switch pads work natively, with no setting and no mapper:** FLvibed reads them itself through SDL3, with the right button
  prompts, rumble and hot-plug, whether or not Steam Input or a mapper is running. Their output for that pad is ignored, so each press acts once: the
  Xbox-pad copy they present and, while you use the pad, the keys and mouse they send. A real Xbox pad beside it works too: the pad in your hands is the
  pad, with its prompts and rumble. To hand the pad to Steam Input or your mapper, turn off **Read PlayStation and Switch pads natively** (Options,
  Controller, Input). A Steam Deck or Steam Controller is not touched: Steam Input is how those work.
  Known limits: what a mapper sends while the pad rests still reaches the game (Steam's gyro, macros). A mapper that outputs through a virtual device of
  its own, or as a DualShock, is not seen: turn the row off for those.
- A Bluetooth PlayStation pad's rumble switches the pad to a mode other apps may not read, as in other games that rumble it, as far as is known. If another app sees a dead pad
  after playing over Bluetooth, hold the PS button for about 10 seconds to turn the pad off.
- Windows 7: FLvibed is now built with a Microsoft compiler that still targets it, after a tester's Windows 7 needed VxKex to run 0.3.0.
  Expected to work without VxKex, not yet confirmed on a real Windows 7.
- Autopilot: stopping it during a coast gives you the engine back, and starting it again no longer sits there lit and doing nothing.
- Autopilot: loading a save, from the pause menu too, stops it. It used to fly on into the game you loaded.
- Autopilot: it keeps turning with the rear view on, and with the pause menu open in multiplayer, where the game has no pause.
- After a launch, the first press of the throttle key no longer drops the ship's speed under HUD 2 (from 80 m/s to about 30). The pad's throttle and HUD 2's throttle reading had the same stale value.
- Autopilot: **Skip docking cutscenes** now works on a trip to a station or base, not only to a planet (GitHub issue 32). In multiplayer the scenes play.
- Autopilot: a dock you made by hand no longer stops its next start for four seconds.
- Autopilot beside FLUF: FLUF's windows get the pad while it flies, and the stick there no longer takes the ship back.
- Multiplayer: a server's word changed during a flight counts at once, not at the next start.
- Restart stalled maneuvers leaves a queued dock alone: waiting for the bay is no stall.
- Pad: after using a menu with the pad, the mouse works again at once. A held A or X no longer stays pressed for the mouse.
- Pad: a worn stick resting off centre, or a wheel's axis, no longer takes the game from your mouse.
- Pad: the right stick scrolls the lists it says it does, at the dealers too (an overflowing description's own scroll bar is not reached yet).
- Pad: the ship dealer no longer sometimes opens on its final trade stage. A click from the pad could count as a double click.
- Pad: the job board's tooltip no longer flickers on a still screen. With the pad there is no cursor for it to follow, so in the docked windows it stays off (rooms keep the characters' name cards).
- Pad: the button prompts stay for as long as a window, menu or modal is open. They used to leave after 3 seconds and not come back on a later visit, as at the job board. In a base room they still fade; the Prompt bar row's Always keeps them there too.
- HUD 2: the face buttons in the prompts wear their platform's colours, in the shades of each family's latest coloured generation: the Xbox Series pad, the DualShock 4 (for the DualSense too) and the New Nintendo 3DS (for the Switch). Glyph look's HUD 2 style keeps them single-colour.
- Pad: vibration now defaults to 100 %. If your setting is the old default of 60 %, it moves to 100 % once, at the first start of this version; any other value stays. To set it back: Options, Controller, Rumble. A 60 % you set afterwards stays.
- Pad: with Weapon Groups On Triggers, both triggers fire their groups under the autopilot too, and HUD 2 shows both. LT used to light the afterburner there (GitHub issue 31).
- Pad: unplugging or alt-tabbing with the action wheel open no longer fires the highlighted action.
- Pad: the Keyboard and mouse input mode also silences Start. X in the flight inventory keeps the item's description.
- Saves: a Speedrun or Assisted save loaded from the pause menu now loads as what it is. A save folder that is out of reach for a moment no longer forgets every save's mode.
- HUD 2: an alert no longer comes back on the next flight after a dock.
- Speedrun: a best is kept when two splits land in the same frame. New Game's list also takes up, down, Enter and Escape; SPEEDRUN MODE is offered only with the module on.
- Options: Reset Everything also clears the rows of modules that are off, and the Borderless row says when your change acts. The Bindings button says plainly that the editor is not built yet.
- `EXE\FLvibed.ini`: `[Modules]` rows take `on`, `off`, `yes` and `no` as every other setting does (`hud = on` used to turn the HUD off).
- With the Autopilot off, the target picker and HUD 2 still know the mission's ships.
- Smaller logs: the stall recorder keeps to its cap, and 2,400 trace lines at every load are gone.
- Updating by hand from the zip keeps your settings. The zip's `FLvibed.ini` used to replace yours, which reset every FLvibed setting at
  the next start; the zip no longer ships that file (the game writes it at its first start).
- Uninstall: the autopilot's learned ranges (`EXE\FLvibed\ranges.tsv`) and the `*.before-0.2.0` backups go with your settings, so the
  `EXE\FLvibed` folder no longer stays behind; the dialog now names everything it removes, the speedrun bests included.
- FLvibed is now a core with eight modules: HUD, Controller, Subtitles, Movies, Autopilot, NPC, Display and Speedrun. Each has
  its row in `EXE\FLvibed.ini` `[Modules]`. A row you never set follows the module it came from (`hud`, `autopilot`, `speedrun`,
  `display` and `npc` follow `controller`, `movies` follows `subtitles`), so nothing changes on an existing install. Keyboard and
  mouse players can keep HUD 2, the autopilot or speedrun mode with the Controller off. With `subtitles = 0` and `controller = 0`
  nothing starts, and `safe = 1` turns everything off, for troubleshooting.
- Options, GENERAL: the FLVIBED page sets all eight modules, each row saying what it needs; no ini to edit. Turning the Controller
  off no longer takes the other modules with it, and one of Subtitles and Controller always stays on, because with both off
  FLvibed does not start, this page included. Reset Everything brings every module back alike.
- Options: the FLVIBED page works with the Controller module off. A setting that acts on the game stays live with what uses it:
  Text size, Steam Deck mode, FLUF's windows and Restart stalled maneuvers are core's; the window rows go with Display, the speedrun
  rows with Speedrun, the autopilot's with the Autopilot, the movie rows with Movies when Subtitles are off. A row whose module
  is off says so, shows the value in the file, and tells you where to turn the module on.
- New Game on keyboard and mouse: with the Controller off, NEW GAME offers NORMAL GAME, ASSISTED and SPEEDRUN MODE by mouse, so
  speedrun mode, its timer, splits and LiveSplit work with no pad. Saves carry their mode there too.
- Restart stalled maneuvers now protects every player, whichever modules are on.
- Multiplayer: a server's word on the autopilot reaches it with the Controller off, exactly as with it, and its tag line stays
  out of the chat whichever modules run.
- A crash in the HUD, Autopilot, Speedrun or NPC module stops that module alone, as if it were off: the pad, the options page and
  the rest play on, and the module's rows say MODULE STOPPED until you restart the game.
- Options, Autopilot tab: its rows work again. Every row was greyed, and Reset, Default and Save skipped them, in 0.2.0 to 0.3.0.
- Autopilot, goal panel: the SETTINGS tab's changes are saved.
- Autopilot: the guns are released when you stop it, whatever stopped it.
- A crash in FLvibed's own logic gives you back the game's HUD and stops the ship turning, instead of leaving the panels hidden.
- HUD 2: the keyboard's throttle keys work. With the game's maneuver bar hidden they did nothing, so the ship kept whatever speed it had
  (this fits a report of speed locked in flight while trade lanes still worked; the reported 10 m/s itself was not reproduced).
- HUD 2: your allies' health bars in the lower left have the same small pips as the rest of the HUD (GitHub issue 23).
- Pad, dealers: the QUANTITY line stands still; it moved sideways whenever its digits changed (a likely cause of GitHub issue 24, not confirmed yet).
- Pad, inventory: X steps its categories as at the Equipment Dealer, in flight and docked, and the prompt bar says FILTER (GitHub issue 30).
- HUD 2: the game's next and previous target keys work; with the contact list hidden they found nothing.
- Holds as toggles: a boost or rear view the game ended by itself no longer needs two presses to start again (GitHub issue 29).
- Autopilot: it keeps flying and fighting while the map, a NeuroNet window or the options page is open (GitHub issue 25); only the game's
  pause holds it. The window keeps your pad, keys and cursor: what you do there does not steer the ship or take it back.
- With the Controller module off: the map and inventory can be clicked while the autopilot flies, the goal panel closes at a mode
  change, turret view keeps the autopilot's gun aim, HUD 2 names your own `autopilot_key`, and the options page keeps Steam Deck
  mode and Text size.
- With `controller = 0` the game also gets the message-pump fix (`pump_no_block`) and the LAN list fix, which belong to the core
  now.
- Pad, equipment dealer: the D-pad no longer lands on a carried item's mount box, so A on your row cannot unmount it by accident (the comment on GitHub issue 10). LS click mounts and unmounts; the box stays on screen for the mouse.
- HUD 2, docked windows: text no longer changes size and weight as you use a window (GitHub issue 22). A list's names stay at the game's size, where a long name scrolling into view used to drop the whole list to a smaller, thinner font until the window closed, and a dealer's descriptions keep one size for all items instead of a size per item. A description too long for that size scrolls in its box.
- Uninstall removes `FLvibed_npc_stalls.log`.

## 0.3.0 - 5 Nov 2026

- HUD 2, danger alerts: Shield Failing now stands by the left meter, Missile Incoming and Hull Critical by the right, level with
  the reticle, so your eyes stay near the centre (GitHub issue 21). Two dangers show together instead of one erasing the other:
  on the same side the newest stands level and pushes the older one down, and subtitles push a side's cards down when they reach
  them. The cards arrive with a burst and then glow and pulse; the words and prompts stay steady, and the Shield hit cue option
  turns the glow off. Shield Failing no longer offers FLARE.
- Pad, weapon groups on both triggers: a held trigger fires full auto. It fired one shot a pull (GitHub issue 14).
- Pad, dealers: hold A to buy or sell, one trade a hold, as you accept a job. A tap never spends money, and holding again
  repeats the trade (the comment on GitHub issue 20).
- Pad, equipment dealer: your own equipment can be sold; A was only the mount box. LS click now mounts and unmounts it (GitHub
  issue 20).
- Pad, equipment dealer: LT / RT QUANTITY and Y MAX show whenever the game offers a quantity (GitHub issue 17).
- HUD 2, loadout panel: with a weapon group on each trigger, each group's row shows its own trigger (GitHub issue 16).
- HUD 2: the shield meter drains downward, as the hull meter does (GitHub issue 18).
- Pad, the Freelancer layout: **Y tap now flips to the next weapon group and Y held is cruise**, the other way round from
  before (GitHub issue 15). The new option "Swap Y tap and hold" (`y_swap`) gives the old order back. With Weapon groups on
  Simple or Two groups, Y stays cruise on a tap. The Weapon groups choice CYCLE ON Y HOLD is now CYCLE ON Y.
- New Button prompts choice "Steam" for the Steam Deck and Steam Controller: Xbox's face buttons and View / Menu with L1 / L2 /
  R1 / R2, picked automatically on a Deck (GitHub issue 19).
- HUD 2, docked: the game's tooltip no longer flashes over a resting cursor. FLvibed's own focus check set it off four times
  a second in every docked window, the job board and the dealers included (GitHub issue 10).
- HUD 2, docked: the button prompts no longer blink on a purchase, and jump around less while a window opens.
- HUD 2, job board: the window grows to fit a long mission description instead of the text running out of it (GitHub
  issue 13).
- Steam Deck mode: at 1280x800 HUD 2's text is larger (120 % unless you chose your own size) and the safe margins at the screen's
  edge are gone, which also gives the prompts more room. The new `deck_mode` setting forces it on or off.
- HUD 2, loadout panel: a long torpedo name moves left into the empty space beside it instead of being cut.
- HUD 2, ship panel: CREDITS no longer shows as a cut, scrolling word when the panel is a fraction too narrow.
- HUD 2, docked: over a dealer the top strip keeps CARGO; the price change gives way first, since the dealer shows the total.
- HUD 2, the game's windows: text now takes the room it has. Prices and the numbers that decide a deal grow most, labels less,
  headings and buttons not at all, and a text steps up only while it fits. Dealers, ship dealer, job board, news, the NeuroNet's
  windows, save / load and the rumor dialog. Infocards take more of their plates and stay free of scroll bars where the text fits.
  New `window_text` setting ("Larger window text"), on by default.
- Options: rows for Steam Deck mode and Larger window text.
- HUD 2, nav map: it stands a little lower, so the subtitles' panel no longer covers its toolbar.
- HUD 2, pause in space: the game's comm portrait and caption no longer stand bare around the pause menu.
- PlayStation prompts: L1 / R1 no longer read as U.

## 0.2.7a - 4 Nov 2026

- Pad, in bases: the bumpers work in a Bar with no news or job board (two-room bases such as Ku07_02). 0.2.7 took that Bar
  for the Deck, so LB / RB seemed to do nothing there.
- Smoother cutscenes: a story line and a chat card on screen together no longer rebuild the text every frame.

## 0.2.7 - 4 Nov 2026

- HUD 2: Neural Net lines (Objective Reached, Tractor Failure, the docking lines and the rest), voice and subtitle, play
  in flight. With HUD 2 on, only the first one played and the others waited until the NeuroNet opened (GitHub issue 8).
- Pad, in bases: LB / RB step through every room on stations too. The Trader and Equipment rooms dropped out of the
  bumpers there, which sent you back to the wrong room or never reached them (GitHub issue 9).
- HUD 2's dealer screens: an item you cannot buy (no free hardpoint, say) keeps the full centre panel and the infocard's
  backing, so its text fits and stays readable, and the prompts stay off it (GitHub issue 10). The comparison with your
  mounted gun uses the panel's whole width, and a long heading takes a second line instead of being cut.
- Pad, in bases: the prompts hold their place while a subtitle shows. This guards against the flicker reported with
  Neural Net subtitles at the dealers, which could not be reproduced (GitHub issue 10).
- HUD 2: the chat keeps its cards through in-flight cutscenes such as docking and launching. The game's plain chat window
  came back in them, with bare lines at the screen's edge (GitHub issue 12).
- HUD 2: the ship panel grows for a long credit count, so CREDITS stays whole and the number no longer clips. Only past
  the panel's widest does the word shorten to CR (GitHub issue 11).
- Pad: turret view follows Invert Y on the right stick; so does the aim cursor of the ASSISTED preset (GitHub issue 6).
- Pad: holding the missile button fires the torpedo in every preset and while the autopilot flies: RB in ASSISTED, B in
  ACE. Before, it only worked in FREELANCER with you flying (GitHub issue 7).
- Pad: the same hold fires a cruise disruptor when the ship has one and no torpedo, and HUD 2's row says HOLD for it
  (GitHub issue 7).
- HUD 2: with one item in reach and targeted, the prompts no longer offer it on both A and X.
- HUD 2: the tractor prompts fade like the others while you move; the changing range kept them on screen.
- HUD 2, keyboard: NEAREST ENEMY now shows the game's nearest-enemy key, not the next-enemy key.
- HUD 2: the enemies cue stays off during cinematic scenes (docking, lanes, jumps).
- Classic HUD, ASSISTED preset: no tractor prompt on A, where A starts the autopilot.
- Pad: if the game refuses the enemy the right stick picked, the next click takes another one. More than 32 enemies
  near no longer stops the clicks from moving on.

## 0.2.6 - 4 Nov 2026

- Pad: the NeuroNet box's buttons (mission log, inventory, player info, help) can be reached with the cursor in flight,
  and the nav map answers the cursor. A NeuroNet window now takes the cursor whoever flies, the pad, the autopilot or
  nobody; the HUD's cursor block for aiming stops while one is open (GitHub issue 2).
- Pad: the triggers zoom the nav map, in space and docked: RT zooms in on what lies under the cursor, LT zooms out, from
  the sector to the system to the universe. They only ever worked with something already selected (GitHub issue 2).
- Pad: weapon groups can be set up. EDIT GROUPS in the action wheel lists the ship's guns: A switches a gun on or off in
  the group, D-pad up / down picks the gun, LB / RB the group, B is done. Holding Y now skips groups without guns, so it no longer lands on a group that fires nothing (GitHub issue 4).
- HUD 2: torpedoes have their own row in the weapons list, with their count and how to fire them (HOLD on the pad, the
  key in the keyboard form). They were counted with the missiles under the missile's name. A ship with torpedoes shows
  two group rows instead of three, the active group always among them (GitHub issue 5).
- HUD 2: the hull meters are red, the game's own HUD colour, on the ring and the target card (GitHub issue 1).
- HUD 2: a low shield keeps its blue and flashes red at 25 % and below; a low hull flashes bright red with a soft bloom
  at 25 % and below. `shield_cue` and `hull_cue` switch the flashes off.
- New: when a hostile ship near you stops moving, FLvibed writes what the game says about it to
  `EXE\FLvibed_npc_stalls.log`, for reports of enemies sitting idle (GitHub issue 3). It only reads and changes nothing in
  the game; `npc_stall_log` 0 switches it off.
- Pad: the right stick's click targets the nearest enemy first. Clicking again within three seconds takes the next
  enemy, and so on through all of them; after a pause it starts again from the nearest. With no enemy near, it still
  targets what is in front of the reticle. A double click still clears the target.
- HUD 2, pad: with enemies near and nothing targeted, the top-right corner says how many enemies are near and shows the
  right stick's TARGET NEAREST.
- Pad: A tractors one item, the targeted loot or, with nothing targeted, the nearest loot. X tractors everything in
  reach. Before, both pulled everything. The prompts say which does what and name the item with its range
  (`A TRACTOR DRAIL · 730 m`, `X TRACTOR ALL 9`), in HUD 2 and the classic HUD. Loot your hold cannot take is no longer
  offered. Keyboard: HUD 2 now shows the key for one item and the key for all loot, instead of the one-item key for both.
- When a formation, goto or dock stalls, FLvibed now leaves the maneuver and starts it again, the way that got a
  player out of it. Starting the same formation again on a ship that was still in it did nothing. The cause of the stall
  is not found yet, and this has not yet met a real stall in testing.
- HUD 2: the game's NeuroNet box could stay on screen beside HUD 2 after loading a save. It now hides again. This could
  not be reproduced in 44 test loads; if it comes back, `FLController.log` names the window that kept it up.
- Autopilot: with HUD 2 on, it read the guns' fire slots from their hardpoint names, because HUD 2 hides the game's list.
  On ships where the two orders differ it could leave a gun idle or fire a launcher's slot. It now reads the game's list.

## 0.2.5 - 2 Nov 2026

- Note: the FLVIBED options page still needs the controller module (`controller = 1`, the default). This is
  temporary: HUD 2, the menus' look and the options page will become their own core module in a later version.
- The FLVIBED options page always shows its DEFAULT, CANCEL and CLOSE buttons, inside the Options screen too (CLOSE
  was missing there), spaced to fit the panel at any aspect ratio and text size.
- Opening the FLVIBED options page while the game's Options screen is up now shows it in the screen's panel, not as a
  second, centred panel on top of it.
- In flight the off-screen target arrows keep clear of HUD 2's comm feed cards at the left screen edge.
- Checked: with the classic HUD on 4:3 screens the flight subtitles use the name-tab layout; the display test now checks
  the classic HUD and HUD 2 separately.

- HUD 2's look now also covers the main menu and the mission / tutorial popups (same layout and controls; the classic look unchanged).
- The game's cursor now also hides in flight while you fly with the controller (after 3 s; any real mouse movement brings it back).
- HUD 2's ship panel and loadout are now only as tall as their rows (a short loadout no longer leaves an empty area).
- Contextual HUD: it now also fades in turret view, and pressing any fire button (even with nothing able to fire)
  brings back the whole HUD.
- HUD 2's NeuroNet / objectives readout and the target picker are only as large as what they show (up to their old
  sizes), easing gently; the loadout's launcher name no longer scrolls while its row has room to spare.

- Subtitles: the dialogue panel at the top of the screen in flight is a little smaller (10 %); rooms, story scenes and
  movies keep their size.

- Subtitles: the side chat is smaller by default (Normal); the old size is Large, under `chat_size`.
- HUD 2's target card, contact list, ship panel and loadout panel are only as wide as what they show (up to their old
  widths), easing gently when the content changes.
- The action wheel is more compact: two-word commands on two lines, a smaller ring whose plates clear the reticle and
  each other, and no repeated name under the reticle (both HUD looks).
- Contextual HUD: mouse steering and cursor movement in flight wake the reticle and speed, as the stick does.
- The mouse cursor also hides in flight when you leave the mouse alone, and comes back as soon as you move it.
- Formation, goto and dock no longer stay stuck: if your ship stalls during one, FLvibed restarts the maneuver and logs
  what happened (setting `maneuver_watchdog`, on by default).

- HUD 2's button prompts no longer scroll: a label too long for its place wraps onto two lines in a slightly smaller
  (still readable) font, so the whole text is always visible. Ship and weapon names keep their scrolling.
- HUD 2's context prompts now show when they change or right after you act (dock, cruise, a target, a maneuver), stay
  four seconds and fade out. The new setting `hud_prompts` (0 = always shown, as in 0.2.0) brings back the old way.
- When a mission locks your maneuvers (for example while you fly in Brandt's formation in the first mission), pressing
  free flight, formation, dock or goto now shows LOCKED BY THE MISSION at the top instead of silently doing nothing.
- **Contextual HUD (new default):** `hud_fade` now has two modes, FULL and CONTEXTUAL. Contextual fades HUD 2 out while
  you fly calmly and brings each part back for four seconds when it matters: the ring and speed when you steer or boost,
  the target card when you switch targets, the weapons when you fire, the objectives when they change. Alerts, low
  shield or hull, incoming missiles and fights always show. The old "calm dims" and "minimal" modes are gone (a saved 2
  reads as contextual).
- The action wheel matches the HUD: under HUD 2 each command sits on a HUD 2 plate over a see-through disc; under the
  classic HUD it uses the game's own panel look, lighter than before.
- Subtitles: when lines stack, the newest stays full size and only the older ones shrink, more gently.
- HUD 2 uses the room it has before shortening anything: the weapons panel grows to fit a weapon row on one line and
  only wraps it when the panel is already full width, a long missile name takes a free line above its row before it
  scrolls, and the target picker is always wide enough for its category tabs (they ran past its edges).
- The mouse cursor under the contextual HUD always comes back at once in the pause menu (and any other non-flight
  screen) and hides again only after three idle seconds back in flight.

## 0.2.0 - 1 Nov 2026 - second preview

- **HUD 2 is the default** with the controller and with keyboard and mouse (`hud_style = 2`), and HUD 2's look over the
  menus, bases, dealers and dialogs (Extended HUD 2 in 0.1.0) is now part of HUD 2 and on with it. `hud_style = 0`
  brings back the game's own HUD and screens everywhere; `ui_style = 0` keeps HUD 2 in flight with the game's own
  menus.
- **Every feature on by default:** the hit effects at the screen edges (`shield_cue`, `hull_cue`), the crosshair pulses
  (`reticle_cue`), the autopilot's prompts for players who have not used it yet (`ap_hints`) and the red marker before
  a hostile speaker's name in the subtitles (`hostile_marker`); each can be turned off on the options page.
- HUD 2 sits closer to the screen's edges: 3 % margins instead of 5 % (the TV safe area), more room for the view.
- **Settings reset:** the first start of 0.2.0 puts every FLvibed setting back to its default (so everyone gets the new
  defaults): `FLvibed.ini`, `FLController.ini` and `FLSubtitles.ini` are kept beside the new ones as
  `<name>.before-0.2.0`. The per-save game modes and your speedrun bests stay.
- Subtitles: every dialogue line reviewed by ear against the recordings (names, words, the bracketed sound cues).
- Subtitles: when several speakers' lines are on screen together they shrink smoothly (to 70 % at most, `stack_fit`) so
  the stack keeps about one line's height; at 4:3 and in story scenes they no longer cover large parts of the picture.
- Subtitles: loading a game clears every subtitle still on screen; a comm line from the game you left no longer stays
  stacked over the loaded game's first line.
- HUD 2: an experimental second layout, **Focus** (`hud_preset = 1`, among the options page's advanced rows): in calm
  flight no ring, two thin shield / hull arcs and the instruments at the screen's edges. The Speedrun and Guided layouts are planned; the setting no longer
  accepts their numbers (they drew as Standard).
- HUD 2 layout: nothing overlaps inside a panel or card any more; the ship and loadout panels share one height and lost
  their wasted space (the consumables sit on top of the loadout); a name too long for its room scrolls like a public
  display's marquee; prompts sit in two lanes at the bottom left and right, and the lower centre stays clear of the HUD
  so your ship is never covered (CRUISE ON and the cruise countdown now show inside the ring).
- Licence: FLvibed's source code is now licensed GPL-3.0, like the subtitle data (README "Credits and licences").
- Options page: a long help text above the page widens to the screen's left instead of losing its end (the Rumble
  row's last words were cut at 4:3).
- Autopilot: loading a save mid-session no longer leaves it with the previous mission's goal, title or mission markers.
- Autopilot: no longer writes its flight trace (`FLMotion.tsv`, several MB an hour) in the game folder while it flies;
  the debugging aid is now `motion_log = 1` in `FLController.ini` (the old `motion_trace` line is ignored).
- Pad: the controller now vibrates when your ship is hit (by the hit's weight: the hull heavier than the shield) and
  when your shield fails, and ticks twice when one of the game's boxes waits for your answer, besides the autopilot's
  hand-back pulse - all under **Rumble** (0 = off), only while you play with the controller and the game is in front,
  never in cutscenes or menus, and while paused only for those boxes. The hand-back pulse feels as before but no longer
  plays when you take the ship back from the keyboard or mouse.
- Pad: an experimental, opt-in SDL3 controller backend (`pad_backend = 1` in `FLController.ini`, or the options
  page's advanced rows): meant to read PlayStation and Switch pads as well, with their button prompts; not yet tested
  with those pads. The default stays XInput, unchanged. FLvibed.dll grows by about 1.2 MB (SDL3, zlib licence).
- Autopilot on the original Freelancer 1.1 (not HD Edition): it now knows each system's minefields, damaging and
  dense fields, the lethal cores of planets and suns and the planets' docking rings there too (it read only HD
  Edition's text data files and saw none of them in the original game's binary ones). HD Edition: unchanged.
  The rest of the game data FLvibed reads now comes from those binary files too: HUD 2's zone and lane names,
  prices, pilots, NPC ships, loot and tractor reach, shield thresholds, the bar characters.
- Pad: the options page's **Confirm with** now works: set to RIGHT BUTTON (NINTENDO), every menu, list, picker and
  popup takes the right face button as accept and the bottom one as back, and the prompts show it so; flight keeps
  its layout.
- The autopilot's own follow flight steers round what is in the way (a planet's ring, a station) and keeps out of a
  planet's danger zone, instead of stalling short of the leader.
- Pad: holding RB for a torpedo no longer also launches a missile. A tap of RB launches the missiles when the button
  is released (a hold of 0.4 s is the torpedo).
- Options page on the pad: the stick deadzone setting now applies there too.
- Subtitles with mods: a scene whose recording a mod replaced (a different file under the same sound name, as
  Crossfire ships) no longer shows the original scene's lines over it.
- Subtitles: the 19 scene mixes (room scenes recorded as one track: the landing at Manhattan, Lonnigan on the pad,
  Tekagi's garden, ...) are subtitled, with speaker names and word-timed pages; a skipped scene's lines leave with it,
  also without the letterbox bars. New data file `FLSubtitles\mixes.tsv`.
- Subtitles: the HD Edition intro now subtitles the launch radio exchange before "The war had been raging" (five
  lines that were missing, two completed).
- Turret view while the autopilot flies: hold X to look around from the turret camera while the autopilot keeps
  flying and fighting on its own; the right stick turns the camera and does not take the ship back.
- Dealers: B with the focus on the BUY / SELL button or the quantity slider goes back to the selected item instead
  of closing the dealer (B BACK on the prompt bar).
- The autopilot on the pad is always View held: the unused alternatives (both stick clicks, hold Menu) and their
  setting `autopilot_pad` are gone.
- The installer's uninstall also removes the motion trace and the speedrun bests file (kept with the settings on
  request); a hand-written `.\FLvibed.dll` line in dacom.ini is recognised.
- Windows 7: FLvibed.dll no longer imports a Windows 10 function (it kept the DLL from loading on Windows 7 without
  an extension such as VxKex); FLvibed.dll stays loaded until the game process ends (a reported crash on exit pointed
  at code of an unloaded DLL). Every release is now checked to import nothing newer than Windows 7 (the DLL, the
  installer and the server plugin), and the installer declares Windows 7 to 11. Not yet played on Windows 7.
- The chat never shows an unfilled name template ("%s0 %s1 %s2 %d0", reported from multiplayer).
- **Multiplayer:** FLvibed now works in multiplayer (tested on a local HD Edition server). Distances, attitudes, pilot names, factions, shields,
  trade lanes and credits come from the game's own client data there, so HUD 2, targeting, the subtitles' speaker
  names and the Advanced Autopilot work as in single player (missions, splits and speedrun mode stay single player).
- **The autopilot in multiplayer follows the server:** a server line `[flvibed autopilot=on]`, `=travel` (flying and
  docking, no shooting) or `=off` decides; on a server that says nothing the new setting `mp_autopilot` applies (default
  travel only).
- **Multiplayer with other players:** another player typing the server's tag in the chat can no longer change what
  the autopilot may do (the server's line must be the tag alone - server hosts: put `[flvibed autopilot=...]` on a line
  of its own); the autopilot sees a player who attacks you as a threat (it leaves a lost fight in time instead of at
  10 % hull), and on a server it leans to survival (a lost ship is lost there): new settings `mp_caution` (default
  2, leave sooner) and `mp_flee` (default: leave the fight below 40 % hull).
- Experimental, off by default: `pvp_duel`, a duel flight against other players (jinks while their guns are on you,
  trading fire while sliding out of their line, steady fire when you have the angle, breaking off before a collision).
- **The autopilot's Hold defends itself:** a Hold you started fights ships that attack it (where fighting is allowed),
  then holds again at its point (setting `hold_defend`, on by default).
- **For server hosts:** `flvibed_server.dll`, an FLHook plugin (HD Edition's FLHook) that tells FLvibed's players what
  the server allows the autopilot (`on`, `travel` or `off` in one config file); the line it sends stays out of the chat.
- The autopilot's plain start on a targeted base, jump gate or jump hole now travels there by the game's best path -
  trade lanes when they are faster - before docking or jumping, instead of flying a straight line past the lanes (or
  doing nothing when the gate was beyond the game's docking range).
- Multiplayer server and character lists on the pad: D-pad to pick, A to connect / load, Y for a new character.
- **Multiplayer missions:** objectives, mission marks and job-board missions now reach FLvibed on a server too (read
  where the game itself receives them, the same path in single player): HUD 2's objective line, the autopilot's
  objective mode (take a job, press the autopilot: it flies the job's way and fights its marked targets), dock replies
  on HUD 2, mission results.
- Multiplayer: the docked screens' strip shows the base's name, your ship, hold and prices from the game's own client
  data; your own comm lines carry your character's name.
- The pad on the main menu, the MULTIPLAYER button's LAN / INTERNET prompt (A picks, B goes back) and the multiplayer
  death dialog now presses the game's own buttons directly (no simulated mouse click).
- Pad: on the multiplayer LAN / INTERNET question the D-pad keeps the focus on its answers (it could walk onto
  the main menu's buttons behind it, where A did nothing).
- Pad in the background: while another window is in front, the controller is ignored in menus as well as in flight,
  and it does not vibrate (0.1.0 ignored it in menus always, in flight only with **Controller only while the game is
  in focus**). That setting (`focus_only`, on by default) now decides for both: off, the pad acts in the background in
  menus and flight alike. A button held when you come back to the game is ignored until you let go of it.
- The autopilot's fight odds now count the shield damage it deals (it counted only hull damage dealt, against hull and
  shield lost), so it no longer misjudges fights against shielded ships as lost.
- The autopilot no longer blinks the free-flight button while it waits behind a ship that has stopped (story scenes),
  nor while the game keeps ending a follow it asked for.
- **The autopilot uses all its guns on stations and generators:** every gun that can reach the target fires, also while
  the ship circles it with the nose elsewhere; a friendly ship behind a generator no longer stops the guns (the
  generator takes the misses); the ship holds far enough off that every gun can point at the target (settings
  `fire_covering`, `static_hold_guns`).
- **The autopilot fights for time on target:** it finishes the enemy it is on before turning to another attacker
  (`finish_target`); against a ship flying at it, it reverses, turns ahead of the pass, slides the pass wide and keeps
  firing - through incoming fire while the shield lasts (`headon_reverse`, `headon_shield`); it keeps a distance where a
  crossing target cannot outturn it, then learns the best distance for each pair of ships from its own fights
  (`standoff`, kept in `EXE\FLvibed\ranges.tsv`); missiles at moving ships aim where the target will be and leave out of
  an afterburner run (`missile_lead`); against stations, generators and capital ships every launcher fires as soon as it
  has reloaded.
- **The autopilot rolls:** toward the angle where more of its guns can reach the target (`roll_guns`), a barrel roll
  with a sideways dodge when that costs no gun (`roll_flair`), and it levels the ship smoothly by rolling instead of
  handing it to the game's Goto (`level_off`).
- **The autopilot steers true when the ship is rolled:** the game turns a ship relative to its view, not to the rolled
  ship, so a banked ship turned off its course (and banking into turns spun the ship round); the steering now accounts
  for it (`steer_level`). Banking into turns works now but stays off by default (`roll_bank`: it costs ~0.2 s a turn).
  A roll toward more guns is flown to its end instead of being reconsidered halfway (`roll_commit`).
- **Following without the flicker:** when the game keeps ending a follow near a slow leader (the free-flight button
  blinking), the autopilot keeps the follow position itself (`follow_own`; escorts too).
- **No more missiles bursting on yourself:** at targets where a launch exploded right at the ship, the autopilot no
  longer launches from that close (`self_burst`; the Dyson Sphere's last generator). A dock the game stops flying at
  a moving ship (Battleship Hood) is lined up again (`dock_regain`, now on), and the fast dock no longer rams a
  station's bay at cruise speed: it brakes to what the way in allows (`bay_arrive`; Benford Station).
- **Escapes are flown as escapes:** in an escape mission the autopilot pursues nothing until the first jump out -
  it fights only what shoots at it and waits by the mission's next place (`survival_calm`, `survival_until_jump`,
  `survival_stage`); in speedrun mode it already flies toward the next objective while the story still talks
  (`prestage_next`).
- **Huge static targets (the Dyson city's generators):** the autopilot cruises in, kills the engine while still in
  cruise and coasts in firing its missiles and torpedoes, stops at weapons range and fights close (`big_coast`); it aims
  at the hull in front of it, not the far centre. It never coasts at a moon, planet, sun, lane or gate, nor turns to a
  station while a fight on ships is on.
- A follow you start defends itself: under attack it escorts its leader (`follow_defend`).
- Fixed: the game could stop drawing for seconds to minutes (its message loop waited for a message that never came);
  FLvibed keeps it running (`pump_no_block` in FLController.ini).
- The autopilot holds fire for friendlies in the line less often: only when one is close to the line of fire (setting
  `lof_caution`, 0..1; 1 = as cautious as before).
- The autopilot levels the ship on long straight flights (the camera no longer stays tilted after a turn; setting
  `level_off`).
- Multiplayer with the original game (1.1): local servers in the LAN list are listed by their IPv4 address, so
  connecting to them works (the same fix HD Edition and FLUF apply; FLvibed leaves any existing version of it alone).
- Multiplayer: marked cargo pods no longer read as hostile ships (the autopilot fought nobody instead of shooting a
  job's pod).
- The pad and FLvibed keep working inside the game's own dialog loops (the MULTIPLAYER button's LAN / INTERNET prompt,
  connection errors), which used to freeze them.
- Robustness fixes: an error inside FLvibed's controller logic now also releases the pad's steering and held buttons;
  settings files with an unreadable value keep the previous value; subtitle data saved with a byte-order mark loads.

## 0.1.0 - 26 Oct 2026 - first preview

The first release of FLvibed: one `FLvibed.dll` for Freelancer: HD Edition 0.7.1 and the original Freelancer 1.1.
(It replaces the 0.1 files published earlier the same day, with the fixes from the first player tests below.)

**In this release**
- 🎮 Controller play everywhere (XInput pads): flight, bases, dealers, the nav map and every menu, with button prompts
  for Xbox, PlayStation and Nintendo pads, an on-screen keyboard and three flight layouts (FREELANCER, ACE, ASSISTED).
- 🎯 Gimbal aim: the right stick steers, the reticle locks onto the target's lead.
- 🛰️ HUD 2, FLvibed's own flight HUD (on by default with the pad), and glyph badges on the classic HUD.
- 🖼️ Extended HUD 2 (optional, experimental): HUD 2's look over the menus, bases, dealers and dialogs.
- 💬 Dialogue subtitles with speaker names, colours and portraits, timed on the spoken words; chatter in the chat window.
- 🎬 Movies fitted to the screen; subtitled intros (HD, original 2003, full Bonus DVD cut).
- 🤖 The Advanced Autopilot (work in progress): story missions, jobs, travel, docking and fights; the goal panel, the
  target picker and the action wheel; presets COMPETENT, SPEEDRUN and NO CONSUMABLES.
- 🏁 New Game modes: normal, assisted, or speedrun mode with an in-game timer, splits and LiveSplit.
- ⚙️ The FLVIBED page in the game's Options screen; 💥 optional hit effects; borderless windowed where the game has none.
- 📦 FLvibed-0.1.0-Setup.exe: a one-click installer that finds the game, installs or updates (keeping your settings) and
  uninstalls (also from Windows Settings > Apps); the zip stays for installing by hand.

**Fixed and changed since the first 0.1 files (player tests, 26 Oct 2026)**
- 🖱️ The mouse is yours again in flight: moving it hands control back from the pad at any frame rate (it was pulled
  back to the centre), and in a borderless window covering the monitor it stays in the game at the screen edges.
- 🎯 Target ahead (right-stick click) picks the nearest object inside HUD 2's reticle ring; the target picker's MISSION
  WAYPOINT works when your own waypoint list is empty and no longer points at the sun when a step has no marker.
- 🔭 Turret view on the pad: hold X to switch it on or off; the reticle stays centred (gimbal lock-on included) and the
  right stick turns the camera. It follows the game's own camera (windows, save loads, the H key), and the camera no
  longer keeps turning.
- 🛰️ HUD 2: the cruise engine's state always under the reticle with its button, the tractor prompt there while loot is
  in reach, the throttle and BOOST at the ring's bottom centre, LB TARGETS / WHEEL hints while no target is selected,
  and the clear-target double click in the target card's footer.
- 💾 Loading a save from inside a flight keeps HUD 2's objective, place line and protected-ship bar (M03's convoy).
- 🧰 The installer: the final folder path, a retry while the game still locks its files, the VirtualStore settings of a
  protected folder, and a clean self-delete; the intro plays its audio, the FLVIBED page button is clickable, the
  options page scrolls with a scroll bar and the wheel.

**Known limits:** see the README's "Known limits" - among them: the autopilot still fails some runs, menus and bases are
proven on a virtual pad (flight and combat on a real Elite pad), no bindings editor yet, exclusive fullscreen not
verified, XInput pads only.
