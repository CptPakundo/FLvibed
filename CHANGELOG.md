# Changelog

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
