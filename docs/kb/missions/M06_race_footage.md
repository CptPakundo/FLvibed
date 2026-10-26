# M06 race — how the user raced Dexter Hovis (2020 playthrough footage)

Source: playlist video `Fdgs5drVmlc` (M06-07, 1920x1080 @ 60 fps; times below are video seconds). Read silently from
frames (ffmpeg, no audio); frames in `research\videos\catalogue\clips\m06race\` (git-ignored). Script facts:
runbook `..\..\missions\M06.md` (steps 3-8). Study done 22 Oct 2026 for the autopilot's race function.

## Summary
- **One race, won at the first try** (no reset, no race 2). GO **339.1 s**, finish (ring 2, finish camera cut)
  **≈413.2 s**: **lap ≈ 74.1 s** from standstill — the same as our autopilot's ~74 s. Hovis's race-1 lap is ~80-82 s.
- **Start**: throttle already at 100 % while frozen (speed 0, popup "Prepare for the Race" 331-338.9). The popup's
  close is GO: at +0.15 s the speed rises and **cruise is engaged at once** (charge 1 % at 339.25). No afterburner
  (speed held at 79 during the whole charge, which is thrust max). Charge 5.0 s (≈20 %/s), cruise from **344.2**,
  ramp 79 -> 299 in **4.5 s** (≈49-60 m/s per s, linear), 299 at 348.75.
- **Whole lap in cruise**: from 348.75 to the finish the speed never left cruise (readout 281-301, median 298). The
  only dips (281-290, each < 1 s) sit next to gate passages (383, 394.5, 397.5, 401-402): turning losses, not a
  cruise drop. No braking, no collision, no missed gate (the waypoint chain advances monotonically; gate gaps 2.1-5 s).
- **Hovis was behind the whole lap**: 41 m at GO, 32-57 m through ring 19, 87-142 m mid-course, ~200 m at ring 15,
  448 m at 402, ~1 km at 404.5; 516 m when the finish camera ended (419). The user looked back twice (turret view
  341.5-346.3 during the cruise charge, rear view at 393).
- **The cheat triggered**: the user reached ring 15 first (≈394.5); Hovis speaks at 395 (the `0230` line), two wingmen
  "D's Men - Gilbert Andrews" / "D's Men - Hugh Littlebury" are in the list (red) from 404.5, the weapon platforms
  are red from ≥407.5 (first moment they rank high enough in the contact list), a red incoming-fire ring and big
  platform plasma bolts 405-413. **The user ignored it**: no countermeasure (dropper 5 before and after), no evasion,
  no return fire, stayed in cruise (292-301) — and the catalogue segment 405-413 shows no hull change. At 300 m/s
  the platforms' shots and the late-spawned wingmen never mattered.
- **After the finish**: finish camera 413.4-418.5 (HUD off), `PobjIdle` (speed 0) to ~431 with Hovis's lines,
  "GAME SAVED" (the `winner` save) ~426, "FLY TO BATTLESHIP HOOD" ~430, platforms turn back green. Cruise to the Hood
  ~432-455 (readout **359 m/s** 442-448, see below), "LAND ON BATTLESHIP HOOD" ~454, docking 468-472. **Finish -> docked
  ≈ 59 s** (≈18 s scripted wait + ≈41 s flight).

## Evidence: start (4 fps HUD strips, `clips\m06race\go2\`)
| t (s) | speed | cruise charge | other |
|---|---|---|---|
| 331-338.9 | 0 | - | popup "Prepare for the Race" (OK); throttle 100 %; contact list empty |
| 339.0 | 0 | - | |
| 339.25 | 21 | 1 % | GO = popup close (`tr_theyreoff`) |
| 339.5 / 339.75 | 58 / 72 | 6 / 11 % | |
| 340.0-340.5 | 76-79 | 16-26 % | Hovis 41 m |
| 341.0 | 79 | 36 % | objective "BEAT HOVIS TO THE FINAL GATE"; waypoint 24 m (start ring 2) |
| 341.5 | 79 | 46 % | turret view on (looks back at Hovis) |
| 341.75 | 79 | 51 % | waypoint 1371 m: **ring 2 (gate 1) passed ≈341.4** |
| 344.0 | 79 | 96 % | |
| 344.25 | 80 | engaged | |
| 344.5-346.0 | 88, 100, 115, 129, 144, 159, 174 | | per 0.25 s |
| 346.25-348.0 | 189, 204, 219, 234, 249, 264, 279, 292 | | turret view off 346.4 |
| 348.75 | 299 | | cruise speed reached |

## Evidence: gates (waypoint distance in the contact list; it refreshes only every ~2.5-3 s, so ±0.5 s)
| # | ring | passed ≈ (s) | split (s) | notes |
|---|---|---|---|---|
| 1 | 2 (start) | 341.4 | - | during the charge, at 79 m/s |
| 2 | 4 | 349.8 | 8.4 | 1371 m from ring 2 |
| 3 | 19 | 354.6 | 4.8 | |
| 4 | 5 | 359.1 | 4.5 | |
| 5 | 6 | 362.8 | 3.7 | announcer at 364 (1st-quarter lead line) |
| 6 | 7 | 365.5 | 2.7 | ±1 s |
| 7 | 8 | 368.8 | 3.3 | |
| 8 | 9 | 371.6 | 2.8 | |
| 9 | 10 | 375.7 | 4.1 | |
| 10 | 11 | 378.4 | 2.7 | |
| 11 | 20 | 380.9 | 2.5 | dip to 287 at 383 |
| 12 | 12 | 384.0 | 3.1 | |
| 13 | 13 | 387.5 | 3.5 | |
| 14 | 14 | 389.9 | 2.4 | |
| 15 | 21 | 392.4 | 2.5 | rear view 393 |
| 16 | **15** | 394.5 | 2.1 | **cheat**: Hovis talks 395-397; dip 284 |
| 17 | 22 | 397.2 | 2.7 | announcer 399-401 (3rd-quarter line); dip 289 |
| 18 | 16 | 399.4 | 2.2 | |
| 19 | 23 | 401.7 | 2.3 | dip 281-284 |
| 20 | 17 | 404.3 | 2.6 | wingmen in the list, red ring (fire warning) from 405 |
| 21 | 18 | ≈408.2 | 3.9 | "FINAL GATE" text 409-411; platform bolts 409-413 |
| final | 2 | ≈413.2 | 5.0 | finish camera cut 413.4 |

Lap distance from the speeds: ≈0.36 km before cruise + ≈0.87 km ramp + ≈19.1 km in cruise ≈ **20.3 km**.

## Lines through the gates
Mouse flight throughout, chase camera. The gates are large octagonal truss rings with green corner lights; the next
two or three rings are in view as a chain, so the user flew the chain, not ring by ring. Frames at 393, 401-403 and
413 show the ship crossing **off-centre, near the inner edge** on the turn side (cutting the corner), never on the
waypoint marker itself. No ring was missed and no braking was seen. Exact lateral offsets cannot be measured from
the footage.

## Notes for automating the race
- GO is the popup close in race 1: closing the popup and engaging cruise in the same instant is what the user did.
  Throttle 100 % before GO is free (the ship is frozen). Crossing ring 2 before GO is "jump the gun" (countdown
  restart, second time FAIL), so any pre-charge idea must not move the ship. Whether cruise can be charged while
  frozen before the popup is **untested**; it would save up to ~5 s.
- Ring 2 is passed during the charge (the player starts ~a few hundred m before it); ring 4 comes at ~1.4 km.
- Leading at ring 15 always triggers the cheat in race 1; at cruise speed it costs nothing: the wingmen spawn too
  far back and the platforms' fire did no visible damage. Do not shoot back (platform/wingman kills after the finish
  fail the mission; hits on Hovis count toward the 3-hit warning).
- Post-finish: ~18 s of scripted idle (camera, dialog, autosave `winner`) before "Fly to Battleship Hood"; then
  ≈41 s to dock. The Hood dock opens only when Hovis is within 1.2 km of it ("Land on Battleship Hood" ~454).
- **359 m/s on the way back (441-448, HUD crop `f444_hud.jpg`, throttle 100 %)**: faster than the 300 cruise. Not
  explained by the footage (energy bar full); possibly thrusters in cruise or formation with Hovis. Worth checking in
  game: if it is a player technique it would shorten the race too. Not seen during the race (max 301).

## Could not read
- Exact gate passage times (list refresh granularity), hull/shield values during the platform fire (only the
  catalogue's hull-change flag), the countdown before the popup (offhud camera scenes 199-331), and where Hovis was
  laterally (he was off-screen behind; only distances).
