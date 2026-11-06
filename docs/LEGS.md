# Speedrun legs: the campaign run split into timed sections

The campaign-run benchmark (an unattended New Game -> story end, the long-term benchmark; the user's AFK speedrun plan of 22 Oct 2026, the private handoff)
is split into **legs**, as speedrunners split a run: each leg has fixed start / end conditions, its own best time and a
dev save at its start, so a change is tested on the legs it can reach (TESTING.md "Test what the change can affect")
instead of a four-hour run. The golden run (one instance, uncapped frame rate, New Game -> the final cutscene, no
save / load at all) is the benchmark; leg times are its splits.

Rules (user, 22 Oct 2026): pure non-stop gameplay - **no save / load abuse** (no reload as a skip or as a story
trigger); game-native skips only (dialogue, dealer, room transitions, launch / dock sequences; space cutscenes cannot be
skipped); methods chosen by speedrun research + online evidence, tests only as the fallback; frame rate never capped
(parallel instances while each stays >= 60 fps).

## Legs and their boundaries
Mission legs use the speedrun.com per-level boundaries (`research\speedrun.md` section 2) so they compare with any IL
time; FT legs cover the free time between (money gate, purchases, travel to the next offer, the accept).

| Leg | Start | End | Dev save at start | Reference |
|---|---|---|---|---|
| L00 | New Game click | M01a accepted (Juni, Manhattan) | - (New Game) | - |
| M01 | M01a accepted | "Game Saved" after the Rogue base (M01b end) | TODO | - |
| FT1 | M01b end | M02 accepted (+3500 gate) | TODO | - |
| M02 | M02 accepted | land on Missouri | TODO | - |
| FT2 | Missouri | M03 accepted (+5200) | TODO | - |
| M03 | M03 accepted | Juni's payment | BC46 Save1e7d (docked CA Minor, accepted) | IL WR 836 s (HotSliceOfJesus) |
| FT3 | M03 end | M04 accepted (+7000) | TODO | - |
| M04 | M04 accepted | land on Leeds | BC46 Save6380 (Manhattan, accepted) | user 2020: 1359 s scene-accounted |
| FT4 | Leeds | M05 accepted (+10000) | TODO | - |
| M05 | M05 accepted | land on Leeds | BC46 Save1d87 (Cambridge, accepted) | - |
| FT5 | Leeds | M06 accepted (+16000) | TODO | - |
| M06 | M06 accepted (Hood bar) | land on Leeds | BC46 Save33fa (Hood, before the offer) | - |
| FT6 | Leeds | M07 accepted (gate 0) | - (M07 is offered at Leeds) | - |
| M07 | M07 accepted (Leeds) | land on Kyushu | BC46 Save766f (Leeds, before the offer) | - |
| FT7 | Kyushu | M08 accepted (+36000) | TODO | - |
| M08 | M08 accepted (Shinagawa bar) | land on Kyoto | BC46 Save3a48 (Shinagawa) | - |
| FT8 | Kyoto | M09 accepted (+30000) | TODO | - |
| M09 | M09 accepted (Kyoto bar) | land on Kyoto | BC46 Save4686 (Kyoto) | - |
| FT9 | Kyoto | M10 accepted | - | - |
| M10 | M10 accepted (Kyoto bar) | land on the Osiris | BC46 Save7b39 (Kyoto) | - |
| FT10 | Osiris | M11 accepted | - | - |
| M11 | M11 accepted (Osiris Deck2) | land on the Osiris | BC46 Save08f9 (Li04 Osiris) | - |
| M12 | M12 accepted (Osiris bar, St02) | land on Toledo | BC46 Save5e67 | - |
| M13 | M13 accepted (Toledo) | the final cutscene after the last generator | BC46 Save3989 | - |
| **Total** | New Game | final cutscene | | **WR any% 4:02:48 = 14,568 s** (TM4S13, 18 Sep 2026) |

## Targets: every leg at or under the top speedrun (user, 22 Oct 2026)
**The table below is generated** - `node tools\legtable.js` rewrites it from the leg history (`research\experience\
legs\legs.tsv`, PASS rows; a full leg = a run from its start save) and the runners' numbers (`docs\legs_reference.tsv`);
`flleg.ps1` runs it after every leg, so it is always current. Do not edit it by hand; commit it with the leg's results.
The speedrun preset beats its benchmark only if it matches or goes under **each leg time** of the top speedrun, not
just the total. Leg times from the frame-by-frame study of the top-3 recordings (`research\speedrun.md` V2; evidence
`research\evidence\speedrun\`; boundaries as above). **WR** = TM4S13's 4:02:48; **best of 3** = the fastest of the three
runs; **scene-free** = best of 3 minus the room scenes the runners had to watch and our autopilot skips with Esc
(Benford, Sprague, the Tekagi arch, Holstein, the prison break) - the honest bar for our legs, which never contain
those scenes. Ours = the current best (table below). Seconds.

<!-- legtable:begin -->
| Leg | WR | Best of 3 | Scene-free | Ours (best full leg) | To go vs scene-free | Set | Commit |
|---|---|---|---|---|---|---|---|
| M01a | 417 | 409 | 409 | 479 | 70 | 2026-10-29 07:52 | 538761f |
| M01b | 457 | 457 | 457 | 634 | 177 | 2026-10-30 15:11 | b63c1ad |
| M02 | 876 | 876 | 876 | 1099 | 223 | 2026-10-30 16:28 | 4eef991 |
| M03 | 780 | 763 | 763 | 861 | 98 | 2026-10-30 15:10 | b63c1ad |
| M04 | 1036 | 1014 | 813 | 974 | 161 | 2026-10-24 23:38 | 34ba88f |
| M05 | 1204 | 1170 | 952 | 1084 | 132 | 2026-10-30 02:35 | ee25e00 |
| M06 | 544 | 489 | 489 | 681 | 192 | 2026-10-28 22:36 | c8b70fa+dirty |
| M07 | 1262 | 1250 | 1250 | 1648 | 398 | 2026-10-30 03:43 | 4c4fb6f |
| M08 | 601 | 582 | 582 | 655 | 73 | 2026-10-30 16:39 | 4eef991 |
| M09 | 796 | 796 | 530 | 724 | 194 | 2026-10-24 12:29 | 7ccb022+dirty |
| M10 | 980 | 980 | 915 | 1079 | 164 | 2026-10-30 15:56 | 4eef991 |
| M11 | 1158 | 1158 | 979 | 1169 | 190 | 2026-10-23 14:57 | b2fe700+dirty |
| M12 | 362 | 348 | 348 | 354 | 6 | 2026-10-24 06:08 | 3d102cd+dirty |
| M13 | 752 | 686 | 686 | 746 | 60 | 2026-10-31 05:13 | 136ef81e |
| **Mission legs** | **11225** | **10978** | **10049** | **12187** | **2138** (over the legs we have) | | |

Whole run (WR 4:02:48 = 14,568 s; the three runs' best legs sum to 14,002 s; the free-time legs FT1-FT12 are not
built yet - runners' best 2,876 s, ~2,200 s scene-free; L00 140 s): ours = mission legs only until the chain runs.

Checkpoint bests (partial legs from `research\saves\legs\`; compare with `ref.py <leg> <section>`):

| Checkpoint | Best (s) | Set | Commit |
|---|---|---|---|
| M06_ck_glorious | 330 | 2026-10-22 05:41 | 024cf8e+dirty |
| M07_ck_ku03 | 163 | 2026-10-22 06:10 | 2255208+dirty |
| M09_ck_arch | 597 | 2026-10-22 18:52 | 8df84fb+dirty |
| M10_ck_holstein | 806 | 2026-10-22 13:51 | 0bc887d+dirty |
| M11_ck_buffalo | 101 | 2026-10-22 06:28 | 770a3f5 |
| M12_ck_lair | 322 | 2026-10-22 18:15 | c772206+dirty |
| M12_ck_launch | 463 | 2026-10-23 05:39 | 75a2d8f+dirty |
| M13_ck_city | 126 | 2026-10-29 03:41 | 2097d8f+dirty |
| M13_ck_st03 | 422 | 2026-10-23 00:22 | c7f51cf+dirty |

Personal-best history (full legs; each new best, its gain and what changed since the previous best - commit subjects
between the two runs, newest first, at most 4; notes from `docs\legs_notes.tsv`):

| Leg | Date | Best (s) | Gain (s) | Commit | What changed since the previous best |
|---|---|---|---|---|---|
| M01a | 2026-10-22 08:03 | 517 | - | a38d30d+dirty | first full pass |
| M01a | 2026-10-23 15:27 | 515 | 2 | 7d3275a+dirty | e8e95aca Leash anchor by the pilot's name: mission::Leash::nameIds from [MsnShip] -> [NPC] individual_name, th; 57f855c2 Objective mode after the s8 leg gate: mission leashes (Cnd_DistShip outside Player -> FAIL; a script-; 85cad5ad M12 hops: planes only (RayPath planesFirst - the 3D lattice's chords stalled two of four hop runs; it; 5a9cb7ed M12 airlock: the airlock run plans to the door's outer point (its aim past the mount is unreachable f; (+12 more) |
| M01a | 2026-10-28 17:35 | 511 | 4 | 35f8632+dirty | 833cf42a Multiplayer, two clients: a player's relayed chat never sets the autopilot's permission; player ships; 16e84187 Multiplayer: lane links and ship shields from the client's objects; da018aca Multiplayer, stage 2: the server's word on the autopilot, client cash, speaker factions; b09fcab3 Multiplayer, stage 1: a local MP test setup, modal-loop frames, client-side space readers; (+83 more) |
| M01a | 2026-10-28 23:37 | 509 | 2 | 29c4d7c+dirty | 2ead6f50 Autopilot roll: a roll engine (PhySys angular velocity), roll toward more guns on target (roll_guns),; f72304f5 Autopilot: odds_shield switch (A/B of 9212cc3's shield-point rate); Follow holds off 2.5 s after the ; 0689c297 Level-off on straight travel runs; the LAN list's IPv6 fix shipped (vanilla bytes only); roll in the ; c69dad62 Multiplayer gaps round 2: one client interface for both modes, client-side docked readers, own comm n |
| M01a | 2026-10-29 07:52 | 479 | 30 | 538761f | 538761fd core ini reader decodes vanilla 1.1's binary (BINI) inis (they read as empty); 3c9933ea core\str.h: the one UTF-8 conversion and Lower (three copies each before); a6015348 Subtitles: the scene mixes are subtitled in game; ca2ef929 Signatures: padding is a wildcard; the 28 that held it re-cut; drift_check enforces it (research C1); (+1 more) |
| M01b | 2026-10-22 05:41 | 648 | - | 024cf8e+dirty | first full pass |
| M01b | 2026-10-30 15:11 | 634 | 14 | b63c1ad | c79fc352 Q6 S3: the arbiter's first rules as pure Rule_* beside ObjectiveDecide, in shadow behind situation_sh; 63291e3e Q6 S2: the live trigger table as the situation assessor's story source (A39) - behaviour-neutral, not; d488fbd0 Load reset: the objective title, marks, comms heard and mission.cpp's memory start over at each game ; 4e1c8e7d Q6 S1: the situation assessor (A37), the A41 objective-kind table, the A40 job-text parser and the sn; (+131 more) |
| M02 | 2026-10-22 08:54 | 1134 | - | 1133a16+dirty | first full pass |
| M02 | 2026-10-28 23:05 | 1128 | 6 | 29c4d7c+dirty | 2ead6f50 Autopilot roll: a roll engine (PhySys angular velocity), roll toward more guns on target (roll_guns),; f72304f5 Autopilot: odds_shield switch (A/B of 9212cc3's shield-point rate); Follow holds off 2.5 s after the ; 0689c297 Level-off on straight travel runs; the LAN list's IPv6 fix shipped (vanilla bytes only); roll in the ; c69dad62 Multiplayer gaps round 2: one client interface for both modes, client-side docked readers, own comm n; (+103 more) |
| M02 | 2026-10-29 13:25 | 1109 | 19 | 8c5c9a3 | 8c5c9a39 Universe tables on ini::Read (core\game\universe_data): the autopilot sees minefields, lethal cores a; 538761fd core ini reader decodes vanilla 1.1's binary (BINI) inis (they read as empty); 3c9933ea core\str.h: the one UTF-8 conversion and Lower (three copies each before); a6015348 Subtitles: the scene mixes are subtitled in game; (+2 more) |
| M02 | 2026-10-30 16:28 | 1099 | 10 | 4eef991 | c79fc352 Q6 S3: the arbiter's first rules as pure Rule_* beside ObjectiveDecide, in shadow behind situation_sh; 63291e3e Q6 S2: the live trigger table as the situation assessor's story source (A39) - behaviour-neutral, not; d488fbd0 Load reset: the objective title, marks, comms heard and mission.cpp's memory start over at each game ; 4e1c8e7d Q6 S1: the situation assessor (A37), the A41 objective-kind table, the A40 job-text parser and the sn; (+14 more) |
| M03 | 2026-10-22 08:20 | 919 | - | 4daf565+dirty | first full pass |
| M03 | 2026-10-23 08:37 | 901 | 18 | b88ff41+dirty | 85cad5ad M12 hops: planes only (RayPath planesFirst - the 3D lattice's chords stalled two of four hop runs; it; 5a9cb7ed M12 airlock: the airlock run plans to the door's outer point (its aim past the mount is unreachable f; 2459f3f4 Path follow: on to the next leg within 150 m of its end too (circled a leg end inside the turning cir; 9f8331ff M12: 3D lattice planner (avoid::RayPathGrid; every plane and the lattice per berth, the shortest kept; (+10 more) |
| M03 | 2026-10-24 10:34 | 896 | 5 | c7f0993+dirty | 67288869 Planner: RayPath narrowGain - after the wide berth's way the narrower berth too, kept when shorter by; 0a40dba2 Avoid: the wheel's crossing ring is the smallest at least half free; 70ceef59 Leashes disarmed by the script: a leash whose deactivating trigger waits on a comm line ends once tha; 2da9a045 Leashes: no TICK_AWAY condition and no range under 100 m (M01b's 'gmg_leader, 1, 25, TICK_AWAY' read ; (+2 more) |
| M03 | 2026-10-24 14:16 | 885 | 11 | a898dd1+dirty | 88ab4193 towards_nag (a fight objective after one with an Act_NagDistTowards is fought on the way - M09's patr |
| M03 | 2026-10-29 14:23 | 872 | 13 | 6bf605f | a08c2235 debug obstacles: the lethal zones counted on their own (a vanilla check could not tell them from the ; a71cede2 Autopilot: big_coast never coasts at a moon / planet / sun / lane / gate, nor at a static in place of; f44841af core\safemem.h: the guarded read in core; the release UI code no longer includes debug.h for it (SWEE; a7d55243 geom::Dist for the two local Dist copies (hudinfo, route; bit-identical arithmetic, route tests + dec; (+94 more) |
| M03 | 2026-10-29 15:38 | 869 | 3 | 6bf605f | - |
| M03 | 2026-10-30 15:10 | 861 | 8 | b63c1ad | c79fc352 Q6 S3: the arbiter's first rules as pure Rule_* beside ObjectiveDecide, in shadow behind situation_sh; 63291e3e Q6 S2: the live trigger table as the situation assessor's story source (A39) - behaviour-neutral, not; d488fbd0 Load reset: the objective title, marks, comms heard and mission.cpp's memory start over at each game ; 4e1c8e7d Q6 S1: the situation assessor (A37), the A41 objective-kind table, the A40 job-text parser and the sn; (+8 more) |
| M04 | 2026-10-22 08:42 | 1043 | - | 7221218+dirty | first full pass |
| M04 | 2026-10-23 16:10 | 993 | 50 | b6331d5+dirty | 2da9a045 Leashes: no TICK_AWAY condition and no range under 100 m (M01b's 'gmg_leader, 1, 25, TICK_AWAY' read ; e8e95aca Leash anchor by the pilot's name: mission::Leash::nameIds from [MsnShip] -> [NPC] individual_name, th; 57f855c2 Objective mode after the s8 leg gate: mission leashes (Cnd_DistShip outside Player -> FAIL; a script-; 85cad5ad M12 hops: planes only (RayPath planesFirst - the 3D lattice's chords stalled two of four hop runs; it; (+13 more) |
| M04 | 2026-10-24 23:38 | 974 | 19 | 34ba88f | e3bbd961 clang-format pass over every C++ source (tools\format.ps1, .clang-format = the existing style: Allman; b8b0f221 Core reports what the game says: the pre-aggro overlay moves into the autopilot; avoid / route / zone; de044713 Per-frame memo for space::Contacts / Obstacles / PlayerWeapons, a perf counter for the controller (`p; 825cff9a core\game\space.cpp split by concern (a file move, 124 functions before = 124 after, no body touched); (+15 more) |
| M05 | 2026-10-22 09:05 | 1346 | - | 9c00ca7+dirty | first full pass |
| M05 | 2026-10-23 09:09 | 1249 | 97 | 57f855c | 57f855c2 Objective mode after the s8 leg gate: mission leashes (Cnd_DistShip outside Player -> FAIL; a script-; 85cad5ad M12 hops: planes only (RayPath planesFirst - the 3D lattice's chords stalled two of four hop runs; it; 5a9cb7ed M12 airlock: the airlock run plans to the door's outer point (its aim past the mount is unreachable f; 2459f3f4 Path follow: on to the next leg within 150 m of its end too (circled a leg end inside the turning cir; (+11 more) |
| M05 | 2026-10-23 15:51 | 1236 | 13 | 2da9a04+dirty | 2da9a045 Leashes: no TICK_AWAY condition and no range under 100 m (M01b's 'gmg_leader, 1, 25, TICK_AWAY' read ; e8e95aca Leash anchor by the pilot's name: mission::Leash::nameIds from [MsnShip] -> [NPC] individual_name, th |
| M05 | 2026-10-24 10:18 | 1203 | 33 | c7f0993+dirty | 67288869 Planner: RayPath narrowGain - after the wide berth's way the narrower berth too, kept when shorter by; 0a40dba2 Avoid: the wheel's crossing ring is the smallest at least half free; 70ceef59 Leashes disarmed by the script: a leash whose deactivating trigger waits on a comm line ends once tha |
| M05 | 2026-10-29 13:22 | 1118 | 85 | 8c5c9a3 | 8c5c9a39 Universe tables on ini::Read (core\game\universe_data): the autopilot sees minefields, lethal cores a; 538761fd core ini reader decodes vanilla 1.1's binary (BINI) inis (they read as empty); 3c9933ea core\str.h: the one UTF-8 conversion and Lower (three copies each before); a6015348 Subtitles: the scene mixes are subtitled in game; (+89 more) |
| M05 | 2026-10-29 15:23 | 1112 | 6 | 6bf605f | a08c2235 debug obstacles: the lethal zones counted on their own (a vanilla check could not tell them from the ; a71cede2 Autopilot: big_coast never coasts at a moon / planet / sun / lane / gate, nor at a static in place of; f44841af core\safemem.h: the guarded read in core; the release UI code no longer includes debug.h for it (SWEE; a7d55243 geom::Dist for the two local Dist copies (hudinfo, route; bit-identical arithmetic, route tests + dec; (+2 more) |
| M05 | 2026-10-30 02:35 | 1084 | 28 | ee25e00 | ee25e009 Q3 slice 2 (WORK_QUEUE): FLvibed's decision timers on one game clock; the server clock follows fixed ; 4ca36df7 Q3 (WORK_QUEUE) slice 1: fixed dt as a test tool (IDEAS T2) - the frame timer detoured, probes 1 and ; 08a99a7b Q1 (WORK_QUEUE): one focus gate for the physical pad (U38) + the rumble help says what is sent (U30); 580b1bb0 A39: the story mission's live trigger table - mission::LiveTriggers + debug `mission live`, verified ; (+1 more) |
| M06 | 2026-10-22 08:03 | 1193 | - | 3c528af+dirty | first full pass |
| M06 | 2026-10-23 07:52 | 719 | 474 | b88ff41 | 85cad5ad M12 hops: planes only (RayPath planesFirst - the 3D lattice's chords stalled two of four hop runs; it; 5a9cb7ed M12 airlock: the airlock run plans to the door's outer point (its aim past the mount is unreachable f; 2459f3f4 Path follow: on to the next leg within 150 m of its end too (circled a leg end inside the turning cir; 9f8331ff M12: 3D lattice planner (avoid::RayPathGrid; every plane and the lattice per berth, the shortest kept; (+10 more) |
| M06 | 2026-10-23 14:37 | 714 | 5 | b2fe700 | e8e95aca Leash anchor by the pilot's name: mission::Leash::nameIds from [MsnShip] -> [NPC] individual_name, th; 57f855c2 Objective mode after the s8 leg gate: mission leashes (Cnd_DistShip outside Player -> FAIL; a script- |
| M06 | 2026-10-23 22:32 | 710 | 4 | e166ed7 | 0a40dba2 Avoid: the wheel's crossing ring is the smallest at least half free; 70ceef59 Leashes disarmed by the script: a leash whose deactivating trigger waits on a comm line ends once tha; 2da9a045 Leashes: no TICK_AWAY condition and no range under 100 m (M01b's 'gmg_leader, 1, 25, TICK_AWAY' read  |
| M06 | 2026-10-24 13:24 | 682 | 28 | 19482da | 88ab4193 towards_nag (a fight objective after one with an Act_NagDistTowards is fought on the way - M09's patr; 67288869 Planner: RayPath narrowGain - after the wide berth's way the narrower berth too, kept when shorter by |
| M06 | 2026-10-28 22:36 | 681 | 1 | c8b70fa+dirty | 2ead6f50 Autopilot roll: a roll engine (PhySys angular velocity), roll toward more guns on target (roll_guns),; f72304f5 Autopilot: odds_shield switch (A/B of 9212cc3's shield-point rate); Follow holds off 2.5 s after the ; 0689c297 Level-off on straight travel runs; the LAN list's IPv6 fix shipped (vanilla bytes only); roll in the ; c69dad62 Multiplayer gaps round 2: one client interface for both modes, client-side docked readers, own comm n; (+82 more) |
| M07 | 2026-10-22 06:41 | 1816 | - | d638602+dirty | first full pass |
| M07 | 2026-10-23 08:11 | 1721 | 95 | b88ff41 | 85cad5ad M12 hops: planes only (RayPath planesFirst - the 3D lattice's chords stalled two of four hop runs; it; 5a9cb7ed M12 airlock: the airlock run plans to the door's outer point (its aim past the mount is unreachable f; 2459f3f4 Path follow: on to the next leg within 150 m of its end too (circled a leg end inside the turning cir; 9f8331ff M12: 3D lattice planner (avoid::RayPathGrid; every plane and the lattice per berth, the shortest kept; (+13 more) |
| M07 | 2026-10-23 14:52 | 1669 | 52 | b2fe700 | e8e95aca Leash anchor by the pilot's name: mission::Leash::nameIds from [MsnShip] -> [NPC] individual_name, th; 57f855c2 Objective mode after the s8 leg gate: mission leashes (Cnd_DistShip outside Player -> FAIL; a script- |
| M07 | 2026-10-30 03:43 | 1648 | 21 | 4c4fb6f | ee25e009 Q3 slice 2 (WORK_QUEUE): FLvibed's decision timers on one game clock; the server clock follows fixed ; 4ca36df7 Q3 (WORK_QUEUE) slice 1: fixed dt as a test tool (IDEAS T2) - the frame timer detoured, probes 1 and ; 08a99a7b Q1 (WORK_QUEUE): one focus gate for the physical pad (U38) + the rumble help says what is sent (U30); 580b1bb0 A39: the story mission's live trigger table - mission::LiveTriggers + debug `mission live`, verified ; (+104 more) |
| M08 | 2026-10-22 04:52 | 691 | - | 024cf8e | first full pass |
| M08 | 2026-10-22 07:54 | 685 | 6 | a3059ca+dirty | a3059ca0 Selling at the commodity trader, natively: Invoke-FLTraderSell (the ship's cargo list NN_Inventory re; e8caa1fa Endgame fixes: a mission's idle park is no hold, the game's own docks are not overridden, capital-shi; e2c7809c Shoot mode (ap start pods: depot cargo pods, space::TargetHardpoint; not yet opening pods), handoff f; 3f063675 Speedrun legs M06-M09: race ring passes (cruise, racing line, look-ahead; M06 race 1 won), mission fo |
| M08 | 2026-10-22 14:03 | 676 | 9 | 3f5991e+dirty | 33254c06 Dangerous attackers outrank the marks; ray end-point fix; wall slide; 4beba192 Geometry guard, static-target orbit, airlock approach straight in; 714064f1 M13 letterbox fixed: airlocks are flown through, never docked |
| M08 | 2026-10-28 23:25 | 663 | 13 | 29c4d7c+dirty | 2ead6f50 Autopilot roll: a roll engine (PhySys angular velocity), roll toward more guns on target (roll_guns),; f72304f5 Autopilot: odds_shield switch (A/B of 9212cc3's shield-point rate); Follow holds off 2.5 s after the ; 0689c297 Level-off on straight travel runs; the LAN list's IPv6 fix shipped (vanilla bytes only); roll in the ; c69dad62 Multiplayer gaps round 2: one client interface for both modes, client-side docked readers, own comm n; (+100 more) |
| M08 | 2026-10-29 01:38 | 656 | 7 | 0332ec1+dirty | - |
| M08 | 2026-10-30 16:39 | 655 | 1 | 4eef991 | c79fc352 Q6 S3: the arbiter's first rules as pure Rule_* beside ObjectiveDecide, in shadow behind situation_sh; 63291e3e Q6 S2: the live trigger table as the situation assessor's story source (A39) - behaviour-neutral, not; d488fbd0 Load reset: the objective title, marks, comms heard and mission.cpp's memory start over at each game ; 4e1c8e7d Q6 S1: the situation assessor (A37), the A41 objective-kind table, the A40 job-text parser and the sn; (+20 more) |
| M09 | 2026-10-22 07:39 | 899 | - | 7d2e437+dirty | first full pass |
| M09 | 2026-10-22 13:47 | 852 | 47 | 5fa8cd6+dirty | 33254c06 Dangerous attackers outrank the marks; ray end-point fix; wall slide; 4beba192 Geometry guard, static-target orbit, airlock approach straight in; 714064f1 M13 letterbox fixed: airlocks are flown through, never docked; a3059ca0 Selling at the commodity trader, natively: Invoke-FLTraderSell (the ship's cargo list NN_Inventory re |
| M09 | 2026-10-22 18:38 | 834 | 18 | c772206+dirty | **hold-and-salvo at the arch generators (static_hold), launch rules at static targets; 4 generators 131 s**; 3fa5999e A mission's dock grant at an airlock is the way in: flown through |
| M09 | 2026-10-24 12:17 | 731 | 103 | 7ccb022 | 88ab4193 towards_nag (a fight objective after one with an Act_NagDistTowards is fought on the way - M09's patr; 67288869 Planner: RayPath narrowGain - after the wide berth's way the narrower berth too, kept when shorter by; 0a40dba2 Avoid: the wheel's crossing ring is the smallest at least half free; 70ceef59 Leashes disarmed by the script: a leash whose deactivating trigger waits on a comm line ends once tha; (+13 more) |
| M09 | 2026-10-24 12:29 | 724 | 7 | 7ccb022+dirty | (uncommitted changes on 7ccb022) |
| M10 | 2026-10-22 08:44 | 1498 | - | c608521+dirty | first full pass |
| M10 | 2026-10-23 13:24 | 1379 | 119 | 909a9fd | e8e95aca Leash anchor by the pilot's name: mission::Leash::nameIds from [MsnShip] -> [NPC] individual_name, th; 57f855c2 Objective mode after the s8 leg gate: mission leashes (Cnd_DistShip outside Player -> FAIL; a script-; 85cad5ad M12 hops: planes only (RayPath planesFirst - the 3D lattice's chords stalled two of four hop runs; it; 5a9cb7ed M12 airlock: the airlock run plans to the door's outer point (its aim past the mount is unreachable f; (+12 more) |
| M10 | 2026-10-23 17:07 | 1171 | 208 | b34853d+dirty | 70ceef59 Leashes disarmed by the script: a leash whose deactivating trigger waits on a comm line ends once tha; 2da9a045 Leashes: no TICK_AWAY condition and no range under 100 m (M01b's 'gmg_leader, 1, 25, TICK_AWAY' read  |
| M10 | 2026-10-24 16:39 | 1143 | 28 | b892e7c | 44b14c8a Review fixes (fresh-eyes pass): flleg catches deploy's own throw in the boot retry and checks the pre; faf29a9a dock_regain reachable from the travel's dock step (DockRegain, sampled each second); closed_lane_ahea; 88ab4193 towards_nag (a fight objective after one with an Act_NagDistTowards is fought on the way - M09's patr; 67288869 Planner: RayPath narrowGain - after the wide berth's way the narrower berth too, kept when shorter by; (+1 more) |
| M10 | 2026-10-24 16:58 | 1128 | 15 | b892e7c+dirty | (uncommitted changes on b892e7c) |
| M10 | 2026-10-24 16:59 | 1125 | 3 | 84310e2+dirty | - |
| M10 | 2026-10-29 12:53 | 1115 | 10 | 8c5c9a3 | 8c5c9a39 Universe tables on ini::Read (core\game\universe_data): the autopilot sees minefields, lethal cores a; 538761fd core ini reader decodes vanilla 1.1's binary (BINI) inis (they read as empty); 3c9933ea core\str.h: the one UTF-8 conversion and Lower (three copies each before); a6015348 Subtitles: the scene mixes are subtitled in game; (+86 more) |
| M10 | 2026-10-30 02:25 | 1095 | 20 | ee25e00 | ee25e009 Q3 slice 2 (WORK_QUEUE): FLvibed's decision timers on one game clock; the server clock follows fixed ; 4ca36df7 Q3 (WORK_QUEUE) slice 1: fixed dt as a test tool (IDEAS T2) - the frame timer detoured, probes 1 and ; 08a99a7b Q1 (WORK_QUEUE): one focus gate for the physical pad (U38) + the rumble help says what is sent (U30); 580b1bb0 A39: the story mission's live trigger table - mission::LiveTriggers + debug `mission live`, verified ; (+7 more) |
| M10 | 2026-10-30 11:46 | 1093 | 2 | d0715f7 | c79fc352 Q6 S3: the arbiter's first rules as pure Rule_* beside ObjectiveDecide, in shadow behind situation_sh; 63291e3e Q6 S2: the live trigger table as the situation assessor's story source (A39) - behaviour-neutral, not; d488fbd0 Load reset: the objective title, marks, comms heard and mission.cpp's memory start over at each game ; 4e1c8e7d Q6 S1: the situation assessor (A37), the A41 objective-kind table, the A40 job-text parser and the sn; (+3 more) |
| M10 | 2026-10-30 13:04 | 1090 | 3 | d8c498f | - |
| M10 | 2026-10-30 15:56 | 1079 | 11 | 4eef991 | - |
| M11 | 2026-10-22 07:43 | 1196 | - | 636f9ef+dirty | first full pass |
| M11 | 2026-10-23 14:57 | 1169 | 27 | b2fe700+dirty | e8e95aca Leash anchor by the pilot's name: mission::Leash::nameIds from [MsnShip] -> [NPC] individual_name, th; 57f855c2 Objective mode after the s8 leg gate: mission leashes (Cnd_DistShip outside Player -> FAIL; a script-; 85cad5ad M12 hops: planes only (RayPath planesFirst - the 3D lattice's chords stalled two of four hop runs; it; 5a9cb7ed M12 airlock: the airlock run plans to the door's outer point (its aim past the mount is unreachable f; (+13 more) |
| M12 | 2026-10-22 13:59 | 627 | - | 5a81268+dirty | first full pass |
| M12 | 2026-10-23 06:26 | 454 | 173 | 0cafa8f | 85cad5ad M12 hops: planes only (RayPath planesFirst - the 3D lattice's chords stalled two of four hop runs; it; 5a9cb7ed M12 airlock: the airlock run plans to the door's outer point (its aim past the mount is unreachable f; 2459f3f4 Path follow: on to the next leg within 150 m of its end too (circled a leg end inside the turning cir; 9f8331ff M12: 3D lattice planner (avoid::RayPathGrid; every plane and the lattice per berth, the shortest kept; (+7 more) |
| M12 | 2026-10-23 06:36 | 407 | 47 | a061c04+dirty | - |
| M12 | 2026-10-23 06:46 | 404 | 3 | 52e6d90 | - |
| M12 | 2026-10-23 07:07 | 394 | 10 | a144c01 | - |
| M12 | 2026-10-23 07:27 | 393 | 1 | 250c218 | - |
| M12 | 2026-10-23 13:32 | 392 | 1 | ff430f5+dirty | e8e95aca Leash anchor by the pilot's name: mission::Leash::nameIds from [MsnShip] -> [NPC] individual_name, th; 57f855c2 Objective mode after the s8 leg gate: mission leashes (Cnd_DistShip outside Player -> FAIL; a script- |
| M12 | 2026-10-23 14:23 | 389 | 3 | 30b8fb1+dirty | - |
| M12 | 2026-10-23 18:02 | 388 | 1 | ac62572+dirty | 70ceef59 Leashes disarmed by the script: a leash whose deactivating trigger waits on a comm line ends once tha; 2da9a045 Leashes: no TICK_AWAY condition and no range under 100 m (M01b's 'gmg_leader, 1, 25, TICK_AWAY' read  |
| M12 | 2026-10-23 18:12 | 375 | 13 | f7bfc52+dirty | - |
| M12 | 2026-10-23 19:58 | 372 | 3 | f116850 | - |
| M12 | 2026-10-23 21:07 | 366 | 6 | 60e4f90+dirty | - |
| M12 | 2026-10-23 22:16 | 364 | 2 | 13f8632 | 0a40dba2 Avoid: the wheel's crossing ring is the smallest at least half free |
| M12 | 2026-10-23 23:47 | 362 | 2 | 4cad6dd+dirty | - |
| M12 | 2026-10-24 02:00 | 359 | 3 | 29fabbc+dirty | - |
| M12 | 2026-10-24 02:00 | 355 | 4 | 29fabbc+dirty | (uncommitted changes on 29fabbc) |
| M12 | 2026-10-24 06:08 | 354 | 1 | 3d102cd+dirty | 67288869 Planner: RayPath narrowGain - after the wide berth's way the narrower berth too, kept when shorter by |
| M13 | 2026-10-22 11:35 | 1194 | - | ce50649+dirty | first full pass |
| M13 | 2026-10-28 19:24 | 909 | 285 | f72304f+dirty | f72304f5 Autopilot: odds_shield switch (A/B of 9212cc3's shield-point rate); Follow holds off 2.5 s after the ; 0689c297 Level-off on straight travel runs; the LAN list's IPv6 fix shipped (vanilla bytes only); roll in the ; c69dad62 Multiplayer gaps round 2: one client interface for both modes, client-side docked readers, own comm n; 833cf42a Multiplayer, two clients: a player's relayed chat never sets the autopilot's permission; player ships; (+102 more) |
| M13 | 2026-10-29 13:08 | 892 | 17 | 8c5c9a3 | 8c5c9a39 Universe tables on ini::Read (core\game\universe_data): the autopilot sees minefields, lethal cores a; 538761fd core ini reader decodes vanilla 1.1's binary (BINI) inis (they read as empty); 3c9933ea core\str.h: the one UTF-8 conversion and Lower (three copies each before); a6015348 Subtitles: the scene mixes are subtitled in game; (+3 more) |
| M13 | 2026-10-30 13:19 | 879 | 13 | d8c498f | c79fc352 Q6 S3: the arbiter's first rules as pure Rule_* beside ObjectiveDecide, in shadow behind situation_sh; 63291e3e Q6 S2: the live trigger table as the situation assessor's story source (A39) - behaviour-neutral, not; d488fbd0 Load reset: the objective title, marks, comms heard and mission.cpp's memory start over at each game ; 4e1c8e7d Q6 S1: the situation assessor (A37), the A41 objective-kind table, the A40 job-text parser and the sn; (+14 more) |
| M13 | 2026-10-30 17:32 | 864 | 15 | 51fd767 | 6de97e08 Living spec v54 published (the owner's standing OK): 85b0839's haptics / pad_backend rows + 7aca17a's |
| M13 | 2026-10-31 01:27 | 785 | 79 | 95a9a4a | bafeb642 M13-MIMIC bundles A + B (all default 0): battery_budget reworked, static_full_dps, bs_approach; 95b2527a M13-MIMIC fixes from sets mm4 / mm5: budget drops a fought group + a base-dock step ends its window; ; 192f8d06 M13-MIMIC items 4-6 switches (all default 0): standoff_hold (+ slide), kill_ahead's fight-step form, ; bb342162 M13-MIMIC item 3: kill_ahead + battery_budget (both default 0) - the barrier and the two-budget consu; (+4 more) |
| M13 | 2026-10-31 01:42 | 783 | 2 | f85b9d8 | - |
| M13 | 2026-10-31 02:12 | 756 | 27 | ae78f55 | - |
| M13 | 2026-10-31 05:13 | 746 | 10 | 136ef81e | 55d42263 Audit robustness fixes + two stage-1 log lines (bundle for set ab12; dev / test builds pass, decision |

(generated by tools\legtable.js from legs.tsv, 2026-11-06 17:29)
<!-- legtable:end -->
Where each leg's gap sits, section by section: `research\evidence\speedrun\topics\navigation\sections.tsv`
(combat ~2,300 s, objective logic ~600-700, docks ~350-400 over all mission legs).

BC46 saves sit before or after the accept (`missions\SAVES.md`); the leg driver accepts story offers itself, so a
mission leg run from a pre-offer save includes the offer (a few seconds). Dev saves of our own runs replace them once the
chain reaches each leg (TODO cells).

## Free-time legs: the money layer (design, 22 Oct 2026)
The gate is net worth >= the target the story sets at a mission's end (`worth-target`; `Get-FLWorthNeeded`), checked on
the story object's events (docking is one - Campaign.ps1). Measured: cargo counts at its goods.ini price
(`kb\economy.md`), so the sources of worth per second, cheapest first:
1. **Free cargo**: depot cargo pods (destroyable depots in most systems: Li01 has 5, e.g. `Li01_14_depot_Hfuel` near
   Fort Bush, 4 pods x 16 H-Fuel = 300 each) and mission loot (M03 convoy artifacts, M10 wrecks) - worth = price x units
   for nothing but the shots and the tractor; the hold is the limit (Starflier 20).
2. **Cheap buys**: a commodity a base sells below its goods.ini price adds (price - local) per unit at once
   (`tools\flmarket.js --sells`; Li01: Light Arms at Detroit Munitions 140 / unit, Trenton 126).
3. **Trades** along the story's own path (sell above price adds the rest); the job board last.
Then any dock fires the check and the next mission's offer appears; equipment / ships only after (they cost 70 % /
20 % of their price in net worth). Needs: shooting a depot's pods (sub-object aim), selling at a trader (UI not yet
found: the trader list shows the dealer's rows only), a planner over (1)-(3) by worth per second.
Measured 22 Oct 2026 (Shoot mode, FT1 save, Fort Bush H-Fuel depot): the pods do open (4 crates x 16 H-Fuel), but the
depot's attitude fell 0.65 -> -1.00 within 7 s of the first pod's hits, the police and pirates around attacked, the
Starflier died after the run - shooting a lawful depot without the runners' cover (an enemy within 2.4 km) is not
viable early. Rep-free FT1: Light Arms at **Trenton Outpost** (Li01_06, beside Manhattan) +126 / unit x hold 25 =
+3150 of the 3500; the rest from loot on the way (to find), or Detroit Munitions (+140 x 25 = 3500, but 60 km west).

## Tools
- `tools\harness\missions\Leg.ps1`: one leg by one set of rules - docked: `Complete-FLBaseStory -AcceptStory`
  (story talks, story offers accepted, scenes skipped, the objective's room, a room tour while launching is not
  allowed, repair + restock, launch); in space: the objective mode. Ends on the mission's result / -Until / time-out /
  -BudgetSec (OVER BUDGET stops a leg that is off pace at once). Phase table (base / space times) with -Out. Prints a
  SPLIT line at every checkpoint and fails the leg on a checkpoint limit (`docs\TESTING.md` "Speedrun splits"; `-Full`
  from the leg's start save; `-ReplayLog <leg folder>` replays an archived log through the same check).
- `tools\splits.py`: the checkpoints' splits - runners, par, ours, PB / gold, limits, `docs\SPLITS.md`, LiveSplit files.
- `tools\harness\flleg.ps1 -Instance n -Save s -Label l`: one leg on its own game instance from a save, evidence in
  `research\experience\legs\<time>_<label>_i<n>\` (transcript, phases.tsv), history `research\experience\legs\legs.tsv`.
  Several legs at once: one flleg per instance (build once first: `tools\deploy.ps1 -NoStart`), check-ins with
  `tools\flcheckin.ps1 -Instances 1,2,3`. A leg that does not pass (death, mission failure, stall / time-out) keeps the
  last 15 s of the sampler's 5 fps recording beside its logs: `end_tail.mkv` + `end_sheet.jpg` (every frame,
  time-stamped, with the HUD's speed strip; user, 22 Oct 2026: dense frames at every death / failure / stall). Deaths
  that take 0.2 s need more than 5 fps: an in-game ring buffer is the next step (HANDOFF).

## Status and bests
Per checkpoint and per leg: `docs\SPLITS.md` (generated after every leg: par, our PB, gold, sum of best, spread, the
fail limits); whole legs: "Targets" above. The per-leg status notes of 22-23 Oct moved to the private handoff history.
**M01b note (24 Oct 2026):** the runners' M01b ends at "GAME SAVED" after the Rogue base; our leg flies on to land on
Pittsburgh (~100 s more), so its whole-leg time above is not comparable - the checkpoint `M01b.end` (objective 22055,
shown with GAME SAVED) is: PB 572 vs par 457.
Race practice (`tools\harness\flrace.ps1`, fixture `research\saves\legs\SaveC707.fl`): best lap 74.1 s (= the
user's 2020 lap, `kb\missions\M06_race_footage.md`). Checkpoint saves of probes: `research\saves\legs\` (the game's
own mission saves copied; any name loads).
