# The flight HUD's controls and the pad's commands (verified in game 25 Oct 2026, HDE 0.7.1, SaveC702)

What the pad HUD layer (`modules\controller\src\impl\padhud.*`) and the bindings presets (`presets.*`) rely on; the
addresses and structures are in `..\FREELANCER_RE.md` "UI (NeuroNet / FlControl family)".

## The HUD's named controls (`uiitems`; names as the game registers them)
| Control | What | Pad badge |
|---|---|---|
| `ManeuverButton0..3` | the maneuver bar, left to right **free flight, goto, dock, formation** (the keymap's IDR_SPACE order) | B, A (goto or dock: whichever applies to the target), A hold |
| `ManeuverCruiseButton` | the cruise box hanging under the bar's left end | Y |
| `NNNavmapButtonRoot`, `NNInventoryButtonRoot`, `NNMissionLogButtonRoot`, `NNPlayerInfoButtonRoot`, `NNHelpButtonRoot` | the NeuroNet box, top right | View on the nav map |
| `SwitchToContactList` | the contact list's expander (the target panel's right button) | LB |
| `ImportantFilterButton`, `ShipsFilterButton`, `SolarFilterButton`, `LootFilterButton`, `AllFilterButton` | the expanded list's filters | LB on the first |
| `NextEnemyButton`, `TargetCloseButton`, `StatusCloseButton` | the target panel's next-enemy and close buttons | RS click |
| `HudRepairButton`, `UseBatteryButton` | nanobots and shield batteries (the counts beside them) | D-pad up / down |
| `HudWeaponGroupButton` x3 (the same name three times, left to right groups 1 / 2 / 3) | the weapon groups; the active one lit yellow | Y above the active one |
| `HealthBar`, `ShieldBar`, `PowerBar`, `HUD_Core` | the gauges (images, not buttons) | - |
At 4K the HudShift layout puts the maneuver bar at x 111..939 (buttons 78 px half extents at depth 0.5), the NeuroNet
box at 3377..3776, the weapon group buttons at 3522..3712 (30 px half extents). Anchor on the controls by name, never
on positions: HudShift moves them.

## Commands (keymap.ini `nickname`; `fl.h` USER_*)
- A command need not be in the key map: the game takes it by id (the slide's handler at 0x4D80A4). `USER_MANEUVER_SLIDE_EVADE_UP` / `_DOWN` are in HDE's map, not vanilla's or a mod's on it; with no map row a hold still needs its key-up, or the slide stays set (0x4D812D re-reads the held commands only on a key-up). Flown on FrontierSpace (`tests\strafe_release.ps1`); vanilla's map read, not flown.
- `USER_AUTO_TURRET` (0xBF, the H key) = **turret view**, a toggle (keydown only). Leaving it, the game also sends
  `USER_LOOK_ROTATE_CAMERA_RESET` itself. The pad holds it: on at the hold, off at the release.
- `USER_WEAPON_GROUP1..3` (keydown) make a group the active one; the HUD's group button lights it (group 2 lit
  after the pad sent GROUP2). `USER_FIRE_WEAPON_GROUP1..3` fire one group each: keymap `state = keydown`, no repeat, and
  the handler (0x4DCC60) fires once per keydown, so the game does **not** poll them as held the way it polls fire-all
  (0x51D115). The pad sends a keydown every frame while the trigger is held (`HoldFireGroup`); the guns' own refire sets the
  rate (shot count on SaveC706, 5 Nov 2026: 12 in 3 s at refire 0.25, 24 in 3 s at 0.12; one shot a pull before; GitHub issue 14). `USER_ASSIGN_WEAPON_GROUPn` stores the current selection into a
  group. The pad sets groups with the action wheel's EDIT GROUPS (USER_TOGGLE_WEAPONn + USER_WEAPON_GROUPn;
  `..\FREELANCER_RE.md` "Weapon groups"). No key is bound to any of them by default (keymap.ini).
- `USER_MANEUVER_FREE_FLIGHT` **does not disengage the cruise engine** (cruise stayed on after it, 25 Oct 2026): the
  pad's cancel button sends `USER_CRUISE` as well when the engine is engaged (`flight::ReadModel` cruiseEngaged).
- `USER_REARVIEW_CAMERA_MODE` and `USER_AFTERBURN` / `USER_MANEUVER_BRAKE_REVERSE` are keydown + keyup commands: a
  "look / thrust while held"; the game's own handler turns an afterburner press into `USER_AFTERBURN_ON` / `_OFF`.
- `USER_CLEAR_TARGET` reports handled = 0 with nothing to clear (harmless).
- `USER_RADIO` (keymap P, keydown) is the game's hail: its key name is ids 1971 "RADIO", its help 3239 "Hails the
  targeted ship." (research\strings\resources.tsv). The action wheel's COMMS slot sends it (phase D, 25 Oct 2026); what
  it does to a single-player target is the game's own business (not measured yet).
- The phase D flight modals (picker / wheel / goal panel, `modules\controller\src\impl\picker.*`, `wheel.*`, `goals.*`) select
  targets through `space::SetTarget` (the contact list's own USER_SET_TARGET action, filter-independent) and the objective's
  navmarker through `space::SelectFirstWaypoint`; the wheel's CONTACT FILTER clicks the HUD's own filter buttons
  (`ImportantFilterButton` .. `AllFilterButton`, `SwitchToContactList` first while the list is collapsed) through `uiclick`.
- The flight cursor's name is `cross` over nothing; over an object it names the object's relation (`hostile`, `fire`,
  `neutral`, `fire_neutral`, ...): the pad's "target the object under the reticle" (RS click) clicks only when the name is
  neither `cross` nor `arrow`, through the game's own click selection (as `fire_target` does).
- The game refuses a dock request past its range (10 km in vanilla and mods on it, 50 km in HDE 0.7.1): the autopilot flies there by the game's Goto first (`tests\dock_far.ps1`).
- The game's dock range (`space::DockRange`, HDE 50 km) is the world prompt's "dock in range" test; the tractor's
  reach (`ai::Self::tractorRange`) the "loot in reach" test.

## Under HUD 2 (25 Oct 2026; `modules\hud\src\hudvis.*`, `..\UI_HUD2.md`)
HUD 2 hides the six panel roots above (`HUD_Maneuvers`, `NN_MainButton`, `HUD_Target`, `HUD_ContactList`, `HUD_Status`,
`HUD_Core`) and the cruise line (`HUD_CruiseProgress`) by their state byte, so **none of these controls is drawn or
hit-testable while HUD 2 shows**: commands still work (the keymap and the pad send them), clicks on the hidden buttons
do not. Exceptions: the NeuroNet box returns while one of its windows is open (the nav map, inventory, log, player
info, help live under `NN<name>Button`), and it takes the cursor then: the pad's HUD hit suppression stops while a
NeuroNet window is open, whoever flies (`..\FREELANCER_RE.md` "The pad's HUD hit suppression"); the cruise line stays while the engine is disrupted ("Cruise Disrupted - n").
The arrival / zone line (`NN_ArrivalText`), the comm window, the scanner window, the chat, the indicators and the damage
arcs stay native. The 3D target and ship views can be drawn without their panels (`hudvis::SetViewRect`). Facts:
`..\FREELANCER_RE.md` "UI" > "The flight HUD's roots and hiding them".
