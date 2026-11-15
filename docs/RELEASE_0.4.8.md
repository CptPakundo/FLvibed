# FLvibed 0.4.8: release notes (released 15 Nov 2026)

Status: released from `main` on 15 Nov 2026 (the owner's word). Public v0.4.7 was dev 8e1cae36 (public main 40227d0); everything since is 0.4.8.
The player-facing list is `CHANGELOG.md`, section 0.4.8. The replies to the issues are in `..\audit_2026-11-12\148_release_048_replies.md`.

## What it is
A small release: HUD 2's map shows who operates where, other ships' icons are drawn from their own models, and two HUD 2 fixes.

## What is in it for the player
- **Influence on the map's ALL (#61).** On the political layer, ALL shows hex cells coloured by who operates there. They come from an offline-built simulation of the game's own encounter spawning (`core\spawnsim*`, read from the executable and checked against it), run on a worker thread on the install's own data (`EXE\freelancer.ini` [Data]), so mods' zones and factions are read. It fails closed: no system, no player relations or a load error gives no cells, the game's fill. The old stripes are gone. HUD 2's own hooks are now three (`uihooks::InstallMapZones` joins the loading screen and the target mute), installed at HUD 2's first selection.
- **NPC ship icons and rank marks (#84, #85).** Icons are drawn from each ship's own model (cached per ship, freed in HUD 2's give-back list, dropped after a device change); rank icons with tier marks beside the rank word; the card's class-line icon.
- **The map's cursor in flight (#91).** HUD 2 keeps its arrow on the map when the game names its flight cursor.
- **Room scenes (#12, #92).** Bar conversations and room scenes show only the subtitles, as cutscenes did since 0.4.7.

## Verification
Each change was checked independently with its nets: hud_modules, classic_frames 16/16, nn_pages (1,012 / 0 for the map change; its map tab is the physical layer, so the cells never show there), and the unit tests (115, 0 failures). Stress for the cells: six map close/open during the New York compute, HUD 2 off and on, New York > Hudson > New York with the compute running (cancelled each time, recomputed in 4.26 s): game alive, no crash log, 0 faults, draw 0.51 ms. Not exercised: HUD 2 off WHILE a compute runs. The Legend was re-shot at 1280x800.
**No full gate was run for this release.** The `bench_level1` recordings are to be re-recorded for the new icons (the stored frames show the old ones).
Not seen: #91 with a real mouse and stick (the fix follows the game's own flight-cursor name, checked in a background game); the cells on any mod's data (read from the install's files, none tried).

## The release build and package
The released files:
- `FLvibed.dll`, from commit 2555f02c: SHA-256 a4592cd1c45b414aaf2b223d186a22dafaebcdd03e433c680ae797c2d901db1d, 6,681,600 bytes
- `FLvibed-0.4.8.zip`: SHA-256 fbe2550001f572cf5bc0204d4b5cf8285bbd1ece8fe016fe917b10fcec47a3da, 4,560,802 bytes (the DLL inside it is that file)
- `FLvibed-0.4.8-Setup.exe`: SHA-256 7f3a4dafdaadb515cbb1a0dab3326000d762e430d14adb6f8800d5de36d91571, 15,558,656 bytes; the setup test all passed

## Release steps
As for 0.4.7 (`RELEASE_0.4.7.md`): export the archive of main into the published clone, commit, tag v0.4.8, push, GitHub release with the zip and Setup, then the issue replies.