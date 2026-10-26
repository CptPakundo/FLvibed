# Changelog

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
