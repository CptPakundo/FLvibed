# `skip_dock_scene` in game: the test owed

The fix of 6 Nov 2026 (branch `skip-dock-cutscenes`, GitHub issue 32): a plain autopilot trip to a station or base skips its
scene as a trip to a planet does. Reach of the change: plain trips (objective mode off) in single player, plus which object
decides under the objective mode. Hence the plain trips below and one objective leg. Results: the end of this file.

**Setup.** Build this branch (`dev`), `tools\deploy.ps1 -Background -Instance <n> -Save SaveC702.fl`, muted. Free roam, New
York, no objective. In PowerShell: `. .\tools\harness\flctl.ps1`, `$env:FLVIBED_INSTANCE = '<n>'`. Read `FLController.log` of
the instance, and take frames at the camera change and at docked.

**Each dock, from a fresh `Load-FLSave SaveC702.fl`:** `Move-FLShipNear <id> -Clearance 5000`, `Set-FLTarget <id>`,
`Send-FL 'ap start smart'`, wait for `Get-FLMode` = docked (180 s cap).

| # | Dock | id | Setting | Expect |
|---|---|---|---|---|
| 1 | Planet: Manhattan Docking Ring | 2755957258 | default | skipped, as before |
| 2 | Station: Baltimore Shipyard | 3197945416 | default | skipped: **the bug** |
| 3 | Base: Fort Bush | 2661040712 | default | skipped: **the bug** |
| 4 | Fort Bush, the game's own dock | 2661040712 | `ap set fast_dock 0` | skipped |
| 5 | Fort Bush, the target changed to a nearby ship once the dock is granted | 2661040712 | default | skipped, `object=2661040712` |
| 6 | Fort Bush | 2661040712 | `ap set skip_dock_scene 0` | the scene plays out (~15 s), docks |
| 7 | Fort Bush by hand: `Set-FLTarget`, the dock maneuver, no autopilot | 2661040712 | default | the scene plays out, docks |
| 8 | Jump gate New York - Colorado (SaveC703) | by contact | default | no skip event, the jump completes |

Then one objective leg with a station dock and one with a ring (M05, M04: `flleg.ps1`), and M12's lair airlock leg: no
regression in time, the airlock never skipped.

**Multiplayer** (`tools\mptest\README.md`, session as there; `deploy.ps1 -Instance 21 -Background`): dock 1 and 3 by `ap start
smart`. Expect both scenes played, no skip event, docked on both (a ring's taps did nothing on 6 Nov: the results). To lift the gate later: send
`tap USER_CANCEL` by hand at a station's scene, watch the server's log and the client; it must dock, and leave the same state
as an unskipped dock (launch again from it).

**Pass (each of 1-5):** in the log, `control-exclusion 0 -> 4 camera=docking`, `mode space -> cutscene`, `autopilot
dock-cutscene skipped (try 1) object=<id>` within 0.3 s, `mode cutscene -> docked` within 1 s of the camera, then `autopilot
stop ... why=docked` or `docking` before it (ring). No `try 2`. Frames: the base's room, no black screen, the HUD right after
the next launch, and a launch that works.
**Fail:** no skip event with the option on; a `try 2` that opens the pause menu; docked never reached; control exclusion left
set; a skip event at a gate, hole or airlock.

## Results, 6 Nov 2026

Evidence: `research\experience\runs\dock_skip_2026-11-06\` (each row: `log.txt`, frames; `summary.tsv`). Camera to docked, in seconds. Frames read at the camera and docked.

| Row | main 1c917e6e (unfixed) | 4103fba9, dev | 5cc80c7d, release |
|---|---|---|---|
| 1 ring | skipped 0.39 | PASS 0.54, launch works | PASS 0.52 |
| 2 Baltimore | **not skipped, 13.0: the bug** | PASS 0.37, launch works | PASS 0.44, launch works |
| 3 Fort Bush | **not skipped, 14.3: the bug** | PASS 0.37 | PASS 0.42, launch works |
| 4 `fast_dock 0` | - | PASS 0.36 | - |
| 5 target changed after the grant | - | PASS 0.27, `object=2661040712`, launch works | - |
| 6 option off | - | PASS, played 14.3 | PASS, played 16.3 |
| 7 by hand | - | PASS, played 14.7 | PASS, played 15.9 |
| 8 gate | - | PASS: no skip event, `camera=other`, jumped | - |

Multiplayer, local server: on 4103fba9 the ring's two taps did nothing (1.14 s, `try 2` 38 ms before docked: a row 1 fail by the rule "no try 2"), Fort Bush played 14.6 s. Fixed in 5cc80c7d: no tap, ring 1.08 s, Fort Bush 15.1 s, docked on both. The gate above stays.

Legs on 4103fba9, all SUCCESS, every dock skipped by `try 1`, no `try 2`: M05 1150 s (30 Oct: 1130, 1145), M04 1079 s (976, 1036), M12 397 s (386 to 422), both airlock transits with no skip event. Three to five games and RPCS3 at once, 65-84 fps against 109-156: the times show no failure, they are no clean comparison. M04's 43 s over its slowest recent run is not explained.

The HUD after a launch: hidden in calm flight on `hud_fade 1`, with the skip on and off alike; a target brings it back (`branch_4103fba9_hudctl`). After a skip the base's grant line stays up in the room for its last 3-4 s, then fades.

**A player's own dock** (the keyboard's command by `tap USER_MANEUVER_DOCK`, the pad's A through the virtual pad; main's build, row 7 on both fixed builds): no skip on either. A ring plays 1.1 s anyway, a station 15.5 s. As the option says ("the autopilot's own dock"), and issue 32 is the autopilot's dock: not widened. A draft for it that was backed out, never built or run in game: `player_paths_not_applied.patch` there.
