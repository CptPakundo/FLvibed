# Test plan: flight aim cursor, aim assist, combat bot

Written for an executing session (Sonnet) that runs it and reports numbers. Everything goes
through the harness; never use screen coordinates. Read `CLAUDE.md` (workflow) first.

## Rules
- PowerShell tool only for `flctl.ps1` (never dot-source it from Bash).
- Every PowerShell call is a new process: start each with
  `. '<FLvibed>\controller\tools\flctl.ps1'` (full path of the FLvibed folder).
- Free-roam test saves only (no story missions, no timers). Planets kill instantly even with godmode:
  if the ship gets near one, `bot off` and reload.
- After any failure, collect `Get-FLLog 30` and `Get-FLEvents -After <n>` and report; do not change code.
- Wait on game state, never on fixed delays: `Wait-FLUntil { <condition from state / ship / events> } '<what>'
  <safety seconds>` (flctl.ps1). Fixed durations below are only measurement windows.
- The game window takes focus during UI steps (`Focus-FLGame`); warn the user before starting.

## Setup
1. If the game is not running: `& '<FLvibed>\tools\deploy.ps1' -Restart`.
2. `Start-FLCombatTest` -> expect `space`. (Godmode on, save `SaveC702.fl` next to Fort Bush,
   Liberty police hostile; hostiles arrive with the lane traffic.)
3. Defaults (from `FLController.ini`): `aim_mode 1` (gimbal), `auto_target 0`, `fire_target 1`. All `set`
   changes below are live only; restore them at the end (or `config reload`). Never `config save`.

## G. Gimbal mode (default) - run first
1. No target (`Send-FL 'tap USER_CLEAR_TARGET'`), any stick input: `mouse` stays at the screen center.
2. `Press-FLPad Up` (closest enemy): while the target's lead is on screen (`ship` w2s `lead=` inside the
   screen), `mouse` follows it within ~20 px (60 ms smoothing); off screen -> back to center.
3. Nothing selects a target by itself for 10 s after clearing (auto_target 0).
4. Select by firing: clear the target, run `bot fight` until its target's lead is on screen near the centre
   (`Wait-FLUntil` on `ship` `lead=`), `bot off`, clear the target again, hold `Set-FLPad -RT 1 -LX 0.3`
   until event `fire-target select` (`Wait-FLEvent`): expect a hostile target.

## A. Aim cursor follows the stick (aim_mode 0; `set aim_mode 0` first, no target needed)
For each pad state hold it until `mouse=(x,y)` from `Send-FL state` reads the same twice in a row (the 60 ms
smoothing has settled), then read it and `steer=(sx,sy)` from `Send-FL ship`. Expected (W,H = screen size from `state`):
the cursor points in the steering direction, full deflection on the screen edge (diagonals in the
corners, within 3 px). In every case `mode` must stay `space`, `cursor` must be `cross`, `uiitems`
must report `hovered= (0)` and `ship` must show a non-zero turn rate (HUD transparency).

| Pad | Expected steer |
|---|---|
| neutral | (0, 0), cursor at center |
| `-RX 1` | (0.77, 0), cursor at the right edge |
| `-RY 1` | (0, 0.77), cursor at the top edge (over the maneuver bar) |
| `-RX -0.707 -RY -0.707` | about (-0.54, -0.54), cursor in the bottom-left corner (over the HUD panels) |
Pass: every cursor within 3 px of expected.

## B. Aim assist (aim_mode 0, hostile only)
1. `Send-FL 'bot fight'` until its target's lead is within 0.12H of the screen centre (`Wait-FLUntil` on `ship`
   `lead=`), then `Send-FL 'bot off'`.
2. Immediately `Set-FLPad -LY 0.01` (pad active, sticks neutral) and sample 4x every 250 ms:
   `mouse` from state, `lead=` (w2s line) and `attitude=` from ship.
3. Repeat step 2 with `set aim_assist 0` (3 samples), then restore `0.75`.
Pass: with assist, whenever the lead is within 0.12H of the screen center the cursor is between the
center and the lead (pulled at least 50% when the lead is within 0.06H). Without assist the cursor
stays at the center. With a friendly target (`attitude` > 0, select with D-pad right until one is
found) the cursor never moves off center.

## C. Combat bot
1. `$s = [int]([regex]::Match((Send-FL 'events 999999999'),'^last (\d+)').Groups[1].Value)`
2. `Send-FL 'bot fight'`, then `Wait-FLEvent 'bot target-lost \(2\)' 180` (the bot's second target lost; 180 s is
   a safety limit), `Send-FL 'bot'`, `Send-FL 'bot off'`, `Get-FLEvents -After $s`.
Pass: `targets-lost >= 2`, `firing / time >= 0.5`, no `player-death`, every
`target-acquired` line has `attitude <= -0.55`. Report `afterburn`, `reverse`, `avg-dist` and `given-up`.
For fighters (radius ~10) the bot should spend most engaged time within ~65-165 m.
3. Chase limit: if a target flees (cruise), expect `bot target-given-up` within ~8 s of it leaving gun
   range, and no target farther than 2500 m being chased.

## D. Regression: menus with the pad
1. `Load-FLSave 'SaveC701.fl'` -> expect `docked` (native load by file name, from any state).
2. Launch: `Start-FLLaunch` -> `space` (goes to the launch room if needed, clicks Launch natively, skips the
   base's launch sequence as Escape does: 3.5 s; `-Watch` keeps it: 14 s). The Launch button
   (`IDS_HOTSPOT_LAUNCH`) exists only while the story allows launching (absent in M01 before Juni's job is
   accepted): Start-FLLaunch then throws. Room buttons are numbered per room: never click `RoomControlN` /
   `ButtonControlN` by number.
3. `Press-FLPad Back` (nav map) -> `space-ui`; `Press-FLPad Back` -> `space`.
Pass: each step's mode/event appears within 30 s.

## Report format
Per section: PASS/FAIL, the measured numbers, and for FAIL the last 20 events + 30 log lines.
