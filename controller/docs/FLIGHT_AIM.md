# Flight, aiming and the test bot (reference)

Detail moved out of CLAUDE.md. Code: `src/impl/controller.cpp` (UpdateSpace, PadAim, UpdateBot,
kOptions), `src/impl/aim.cpp`. Measured with the virtual pad and the user's Xbox Elite Series 2.

## Steering
- Freelancer's mouse flight turns at `offset * 1.3 * max_rate` (max_rate = steering_torque /
  angular_drag), saturating at offset ~0.77. `steer_scale` 0.77 maps full stick to the ship's max turn
  rate; radial `steer_expo` 1.5.
- The host's steering override (0x4C7CB0 detour) feeds the stick offset into Freelancer's own steering
  for that call only and restores the engage timer/flag afterwards (else the HUD shows MOUSE FLIGHT).
  Releasing control sends a few "no turn" frames first: Freelancer keeps the last direction otherwise
  and the ship spins.
- Freelancer stops steering while any UI control is hovered (0x59DAA0 = hovered control). While the
  pad flies, the host hides HUD controls from the UI hit test (0x59DD80 detour, host API v4), so the
  reticle can cross the HUD; windows (nav map, pause, dialogs) stay clickable; off when the physical
  mouse takes over.

## Aiming (user-approved defaults)
- Freelancer's guns fire at the cursor (within their gimbal limits), not along the nose; the HUD's red
  "prediction cross" is the lead point (`CShip::get_tgt_lead_fire_pos`, common.dll).
- **aim_mode 1 "gimbal" (default, user's design)**: right stick only steers; the reticle rests at the
  screen center and locks onto the *selected hostile* target's lead cross while it is on screen
  (`aim_cone` 90 = whole screen), smoothed (`aim_smooth` 0.06 s). No target = reticle centered.
- aim_mode 0 "stick": reticle follows the stick (to the screen edges) + slowdown, magnetism and a
  capped rotational assist (references: CoD/Overwatch controller aim assist). Kept as an option.
- Targeting defaults (user): no automatic selection, a selected target is never changed, D-pad up =
  closest enemy. `fire_target` 1: with nothing targeted, firing while the reticle is over a
  hostile/neutral ship selects it (one left click = Freelancer's click selection; cursor shapes
  `hostile`/`fire`/`neutral`/`fire_neutral`). `auto_target` 1/2 = optional automatic selection.
- Hostile = target attitude towards the player <= -0.55 (IObjInspect vt[26]).

## Test bot (`Send-FL 'bot fight' | 'bot sloppy' | 'bot off' | 'bot'`)
- `fight`: frame-rate dogfighter on the same aim code (hostile targets only). Range keeping modelled on
  the user's clip `CombatTest.mp4` (HUD speed box + nearest contact read with ffmpeg at 1 fps):
  binary throttle (full forward ~80 / full reverse ~-15, switched about once a second), fighters fought
  at ~40-110 m, reverse in sharp turns and when the target is about to leave the screen, afterburner
  whenever beyond the band and roughly ahead; gives up fleeing targets (8 s out of range / > 2.5 km).
  Typical: 3-4 kills per minute vs Liberty police.
- `sloppy`: a gentle human (0.25 s delay, 20 % under-correction, hand-speed stick, noise) steering
  through the real pad pipeline, to compare assist settings. 45 s each: gimbal 25-28 s on target,
  stick+assists 6 s, no assist 1 s.

## Test infrastructure
- Free-roam saves: `tools/make-testsaves.ps1 -Template <save>` (HDE saves are plain-text INI; story
  `No_Mission`, mission blocks removed, `can_dock/can_tl = 1`, timestamp 2099 so they sort right below
  AUTO SAVE): `SaveC702.fl` space at the Fort Bush end of the Manhattan lane (traffic), `SaveC701.fl`
  docked on Manhattan. `Load-FLSave <file>` loads any save by name, natively, from any state (main menu,
  in game, death dialog). New save files need a game restart to appear in the list. Template: `Save5889.fl`.
- `Start-FLCombatTest [-Rep 'rep l H']`: godmode (console toggle, typed once per game process), loads the
  space save, makes Liberty police hostile. Planets still kill instantly.
- Event log (host API v3): mission objectives/popups/mission messages/comm lines hooked from Content.dll
  (text resolved from string ids), plus `mode a -> b`, `player-death`, `bot ...`, `fire-target ...`.
- Story missions: read the script / runbook first (`..\docs\missions\`); nag triggers fail missions;
  never reload code during timed phases.

## Open ideas
- Firm stick input could cancel autopilot (formation/goto keep weapons offline).
- "Nearest to screen center" target selection needs an object enumeration (not available yet).
- Pad cannot target objects outside the contact list filter (e.g. docking rings).
- Physical-mouse handback triggers on any mouse count (no threshold yet).
