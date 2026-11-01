# Changelog

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
