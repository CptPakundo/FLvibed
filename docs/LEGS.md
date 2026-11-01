# Speedrun legs: the campaign run split into timed sections

The campaign-run benchmark (an unattended New Game -> story end, the long-term benchmark; the user's AFK speedrun plan of 22 Oct 2026, `..\HANDOFF.txt`)
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
| M01a | 2026-10-23 15:27 | 515 | 2 | 7d3275a+dirty | 7d3275a2 Travel: the no-wait rule for short trips only for a point, not an object (M05 gate_s8v: the dock with; 0d2bfff4 Objective: the last dock under a placeholder never with a ship leash in the system, and dropped when ; 2b94663a Objective: a leash trip's end is no arrival of the step's trip (M10 gate_s8v: it set 'arrived', the c; 30b8fb1d Way planning: the planner before an 'out first' elbow, and a failed plan tried again once the ship is; (+87 more) |
| M01a | 2026-10-28 17:35 | 511 | 4 | 35f8632+dirty | 35f86323 mprules.h: the silent-server default is decided (travel only); 21771de8 Two clients and autopilot PvP: players as threats, Hold defends itself, multiplayer survival defaults; 833cf42a Multiplayer, two clients: a player's relayed chat never sets the autopilot's permission; player ships; 29bbadae Multiplayer gaps, round 1: pad on the MP lists, gates by travel, fight / jump / vanilla server verifi; (+303 more) |
| M01a | 2026-10-28 23:37 | 509 | 2 | 29c4d7c+dirty | c8b70fa5 Head-on pass and its hold only in a race we win (not while the race is being lost); 8da620ea No roll in a big turn (level-off waits for the nose within 10 deg, fight rolls for the lead within 25; ac96f7cb No missile run out of a big target's blind spot: its salvo goes from the spot; 66050055 Bank into turns behind roll_bank (off): unstable against the screen-space steering; the roll frame ve; (+23 more) |
| M01a | 2026-10-29 07:52 | 479 | 30 | 538761f | 538761fd core ini reader decodes vanilla 1.1's binary (BINI) inis (they read as empty); f81a22aa Pad: RB hold launches the torpedo alone (a missile went with every torpedo); 6f50d4bb tools\flbench.ps1 -> tools\flbenchhist.ps1: no two harness scripts share a name; 3c9933ea core\str.h: the one UTF-8 conversion and Lower (three copies each before); (+14 more) |
| M01b | 2026-10-22 05:41 | 648 | - | 024cf8e+dirty | first full pass |
| M01b | 2026-10-30 15:11 | 634 | 14 | b63c1ad | b63c1ad9 Q6 S7 fix: the arbiter's queue carries the old body's lastReal after its decision (Advance wrote it a; 408c85e3 Harness: an obj checkpoint the script skipped is soft (Leg.ps1 live + splits.py our_splits / judge, t; e4e4d1a6 Q6 S5 gate s5shadow (situation_shadow 1, d0715f7 src:clean): 487 agree / 0 differ over 23 rules and 1; 08346a9f Q6 S7: the executor and the switch situation_arbiter (default 0 = today's decisions) - ObjectiveDecid; (+513 more) |
| M02 | 2026-10-22 08:54 | 1134 | - | 1133a16+dirty | first full pass |
| M02 | 2026-10-28 23:05 | 1128 | 6 | 29c4d7c+dirty | c8b70fa5 Head-on pass and its hold only in a race we win (not while the race is being lost); 8da620ea No roll in a big turn (level-off waits for the nose within 10 deg, fight rolls for the lead within 25; ac96f7cb No missile run out of a big target's blind spot: its salvo goes from the spot; 66050055 Bank into turns behind roll_bank (off): unstable against the screen-space steering; the roll frame ve; (+414 more) |
| M02 | 2026-10-29 13:25 | 1109 | 19 | 8c5c9a3 | 8c5c9a39 Universe tables on ini::Read (core\game\universe_data): the autopilot sees minefields, lethal cores a; 6ae73f5d Controller: the camera's projection read once under a guard (fl::ReadCamera), offsets named; 230be464 fllegrun: pass only parameters with a value (an empty -ApSet killed every leg at its boot); M01a set ; e3e6e952 Harness: every install path through Get-FLInstallDir (the instance's), none hard-coded to the main in; (+18 more) |
| M02 | 2026-10-30 16:28 | 1099 | 10 | 4eef991 | 4eef9917 Q6 S7: the destroy conditions' seen lists are story memory (goal queue record / text v2) - kept in th; dfaa5efe Q6 S7 round 2: gate s7arb2 judged (M11 death = the static-hold hazard at the satellite, not S7; M13 =; 86bb9da2 Gate s7arb rows (situation_arbiter 1 on d8c498f; NOT kept - its parity bug fixed in b63c1ad): 12 OK, ; b63c1ad9 Q6 S7 fix: the arbiter's queue carries the old body's lastReal after its decision (Advance wrote it a; (+52 more) |
| M03 | 2026-10-22 08:20 | 919 | - | 4daf565+dirty | first full pass |
| M03 | 2026-10-23 08:37 | 901 | 18 | b88ff41+dirty | b88ff41f Fast dock: no queue-shortcut rush under fire, a running rush ends when fire comes (M12 Omicron Minor ; b06d0f78 Ways: the elbows alone each second while no way is known (the planner keeps its 6 s pace); airlock li; 250c218e Ways: the out-first leg also level (the ray probe: from the M12 gen 3 kill point a level slide to the; 278acc9f Ways: out first, then the elbow (8 headings x 2 climbs, 250 / 450 m, at most 1.6x straight) when no d; (+61 more) |
| M03 | 2026-10-24 10:34 | 896 | 5 | c7f0993+dirty | 69f574e2 ref_line: the rotation ray-tested and stepped back toward the reference's azimuth when blocked (s11s:; 9ff80b3c ref_line: past the line's end on along the door's axis (never back to p2), the line extended on the a; e5f79136 ref_line: the user's try-3 line through the M12 core (pure pursuit, rotated to this run's turbine cro; a5f1d74b passage_axis reads the last airlock door line kept across trips (s11h: never applied - each trip rese; (+112 more) |
| M03 | 2026-10-24 14:16 | 885 | 11 | a898dd1+dirty | 19482da4 Speedrun preset holds the kept switches (ring_request 10, airlock_charge_at 0.5, rejoin_nearest 1, re; 3e733950 stale_destroy_wait: never a goal that ends the mission (M13's city generators: unmarked at the start ; a7840743 flleg: a boot whose game never answers is retried once (twice in a day the set / gate queue died ther; 7ccb0229 stale_destroy_wait: an unmarked destroy goal does not replace a step's own trip either (M09 'Fly to t; (+2 more) |
| M03 | 2026-10-29 14:23 | 872 | 13 | 6bf605f | 6bf605f8 Autopilot: the own follow rounds what is in the way (ApplyAvoid, as every other flight of ours) and k; 941da84c Gate mpb23 (8c5c9a3 + 4c67fee): 12 PASS, new bests M02 1109, M05 1118, M10 1115, M13 892; M01b FAIL (; a08c2235 debug obstacles: the lethal zones counted on their own (a vanilla check could not tell them from the ; 68d7842a deploy / flleg / flshots: -Instance also takes a prepared folder's name (vanilla11c: a leg on vanilla; (+266 more) |
| M03 | 2026-10-29 15:38 | 869 | 3 | 6bf605f | - |
| M03 | 2026-10-30 15:10 | 861 | 8 | b63c1ad | b63c1ad9 Q6 S7 fix: the arbiter's queue carries the old body's lastReal after its decision (Advance wrote it a; 408c85e3 Harness: an obj checkpoint the script skipped is soft (Leg.ps1 live + splits.py our_splits / judge, t; e4e4d1a6 Q6 S5 gate s5shadow (situation_shadow 1, d0715f7 src:clean): 487 agree / 0 differ over 23 rules and 1; 08346a9f Q6 S7: the executor and the switch situation_arbiter (default 0 = today's decisions) - ObjectiveDecid; (+38 more) |
| M04 | 2026-10-22 08:42 | 1043 | - | 7221218+dirty | first full pass |
| M04 | 2026-10-23 16:10 | 993 | 50 | b6331d5+dirty | b6331d5c Leash: follow back only beyond 80 % (M10 gate_s8w: trips back at ~4.5 km of 6 km broke the exit waypo; 6c925a98 Objective: the last dock kept under a placeholder only for a base or planet, never a lane / gate / ho; 2da9a045 Leashes: no TICK_AWAY condition and no range under 100 m (M01b's 'gmg_leader, 1, 25, TICK_AWAY' read ; 7d3275a2 Travel: the no-wait rule for short trips only for a point, not an object (M05 gate_s8v: the dock with; (+84 more) |
| M04 | 2026-10-24 23:38 | 974 | 19 | 34ba88f | 34ba88fa Objective mode: a placeholder step's marked hostiles while an escort is active are the escort's threa; 946d2741 Set s13a: M04 x2 on 3 instances at the refactored build with the session-12 end code - 6/6 PASS, the ; f5d8d76d Practice saves get a fitness check: the ship's state recorded at -SaveAt, judged on the first run fro; e3bbd961 clang-format pass over every C++ source (tools\format.ps1, .clang-format = the existing style: Allman; (+123 more) |
| M05 | 2026-10-22 09:05 | 1346 | - | 9c00ca7+dirty | first full pass |
| M05 | 2026-10-23 09:09 | 1249 | 97 | 57f855c | 57f855c2 Objective mode after the s8 leg gate: mission leashes (Cnd_DistShip outside Player -> FAIL; a script-; b88ff41f Fast dock: no queue-shortcut rush under fire, a running rush ends when fire comes (M12 Omicron Minor ; b06d0f78 Ways: the elbows alone each second while no way is known (the planner keeps its 6 s pace); airlock li; 250c218e Ways: the out-first leg also level (the ray probe: from the M12 gen 3 kill point a level slide to the; (+55 more) |
| M05 | 2026-10-23 15:51 | 1236 | 13 | 2da9a04+dirty | 2da9a045 Leashes: no TICK_AWAY condition and no range under 100 m (M01b's 'gmg_leader, 1, 25, TICK_AWAY' read ; 7d3275a2 Travel: the no-wait rule for short trips only for a point, not an object (M05 gate_s8v: the dock with; 0d2bfff4 Objective: the last dock under a placeholder never with a ship leash in the system, and dropped when ; 2b94663a Objective: a leash trip's end is no arrival of the step's trip (M10 gate_s8v: it set 'arrived', the c; (+21 more) |
| M05 | 2026-10-24 10:18 | 1203 | 33 | c7f0993+dirty | 69f574e2 ref_line: the rotation ray-tested and stepped back toward the reference's azimuth when blocked (s11s:; 9ff80b3c ref_line: past the line's end on along the door's axis (never back to p2), the line extended on the a; e5f79136 ref_line: the user's try-3 line through the M12 core (pure pursuit, rotated to this run's turbine cro; a5f1d74b passage_axis reads the last airlock door line kept across trips (s11h: never applied - each trip rese; (+86 more) |
| M05 | 2026-10-29 13:22 | 1118 | 85 | 8c5c9a3 | 8c5c9a39 Universe tables on ini::Read (core\game\universe_data): the autopilot sees minefields, lethal cores a; 6ae73f5d Controller: the camera's projection read once under a guard (fl::ReadCamera), offsets named; 230be464 fllegrun: pass only parameters with a value (an empty -ApSet killed every leg at its boot); M01a set ; e3e6e952 Harness: every install path through Get-FLInstallDir (the instance's), none hard-coded to the main in; (+261 more) |
| M05 | 2026-10-29 15:23 | 1112 | 6 | 6bf605f | 6bf605f8 Autopilot: the own follow rounds what is in the way (ApplyAvoid, as every other flight of ours) and k; 941da84c Gate mpb23 (8c5c9a3 + 4c67fee): 12 PASS, new bests M02 1109, M05 1118, M10 1115, M13 892; M01b FAIL (; a08c2235 debug obstacles: the lethal zones counted on their own (a vanilla check could not tell them from the ; 68d7842a deploy / flleg / flshots: -Instance also takes a prepared folder's name (vanilla11c: a leg on vanilla; (+7 more) |
| M05 | 2026-10-30 02:35 | 1084 | 28 | ee25e00 | ee25e009 Q3 slice 2 (WORK_QUEUE): FLvibed's decision timers on one game clock; the server clock follows fixed ; 4ca36df7 Q3 (WORK_QUEUE) slice 1: fixed dt as a test tool (IDEAS T2) - the frame timer detoured, probes 1 and ; b376a215 Q2b (WORK_QUEUE) slice 1: the focus-graph crawler (IDEAS T7) + the LAN / INTERNET prompt's focus fix ; e38ca61e Q2 (WORK_QUEUE): device-layer test pads - XInput slots served from shared memory (IDEAS T36 slice 1); (+14 more) |
| M06 | 2026-10-22 08:03 | 1193 | - | 3c528af+dirty | first full pass |
| M06 | 2026-10-23 07:52 | 719 | 474 | b88ff41 | b88ff41f Fast dock: no queue-shortcut rush under fire, a running rush ends when fire comes (M12 Omicron Minor ; b06d0f78 Ways: the elbows alone each second while no way is known (the planner keeps its 6 s pace); airlock li; 250c218e Ways: the out-first leg also level (the ray probe: from the M12 gen 3 kill point a level slide to the; 278acc9f Ways: out first, then the elbow (8 headings x 2 climbs, 250 / 450 m, at most 1.6x straight) when no d; (+65 more) |
| M06 | 2026-10-23 14:37 | 714 | 5 | b2fe700 | 30b8fb1d Way planning: the planner before an 'out first' elbow, and a failed plan tried again once the ship is; 73e87325 Objective: a Dock objective overwritten by a nag goes on while only a placeholder text follows it (M1; dd64b644 Travel: no waypoint-list wait for a short trip in this system in the objective mode (under 6 km) (bui; a5caabbb Travel: in the objective mode the waypoint list is waited for only in the step's first 1.5 s (M12: ev; (+18 more) |
| M06 | 2026-10-23 22:32 | 710 | 4 | e166ed7 | 70ef3aaf Autopilot: drop the "far spokes" code; the bump event skips the pursuit's own braking; bf7dc5de Autopilot: under fire, a lined-up fast run keeps its speed to the normal request point; 0a40dba2 Avoid: the wheel's crossing ring is the smallest at least half free; 79ead461 Autopilot: the passage point first, the parallel line through the plate; (+51 more) |
| M06 | 2026-10-24 13:24 | 682 | 28 | 19482da | 19482da4 Speedrun preset holds the kept switches (ring_request 10, airlock_charge_at 0.5, rejoin_nearest 1, re; 3e733950 stale_destroy_wait: never a goal that ends the mission (M13's city generators: unmarked at the start ; a7840743 flleg: a boot whose game never answers is retried once (twice in a day the set / gate queue died ther; 7ccb0229 stale_destroy_wait: an unmarked destroy goal does not replace a step's own trip either (M09 'Fly to t; (+41 more) |
| M06 | 2026-10-28 22:36 | 681 | 1 | c8b70fa+dirty | c8b70fa5 Head-on pass and its hold only in a race we win (not while the race is being lost); 8da620ea No roll in a big turn (level-off waits for the nose within 10 deg, fight rolls for the lead within 25; ac96f7cb No missile run out of a big target's blind spot: its salvo goes from the spot; 66050055 Bank into turns behind roll_bank (off): unstable against the screen-space steering; the roll frame ve; (+233 more) |
| M07 | 2026-10-22 06:41 | 1816 | - | d638602+dirty | first full pass |
| M07 | 2026-10-23 08:11 | 1721 | 95 | b88ff41 | b88ff41f Fast dock: no queue-shortcut rush under fire, a running rush ends when fire comes (M12 Omicron Minor ; b06d0f78 Ways: the elbows alone each second while no way is known (the planner keeps its 6 s pace); airlock li; 250c218e Ways: the out-first leg also level (the ray probe: from the M12 gen 3 kill point a level slide to the; 278acc9f Ways: out first, then the elbow (8 headings x 2 climbs, 250 / 450 m, at most 1.6x straight) when no d; (+75 more) |
| M07 | 2026-10-23 14:52 | 1669 | 52 | b2fe700 | 30b8fb1d Way planning: the planner before an 'out first' elbow, and a failed plan tried again once the ship is; 73e87325 Objective: a Dock objective overwritten by a nag goes on while only a placeholder text follows it (M1; dd64b644 Travel: no waypoint-list wait for a short trip in this system in the objective mode (under 6 km) (bui; a5caabbb Travel: in the objective mode the waypoint list is waited for only in the step's first 1.5 s (M12: ev; (+18 more) |
| M07 | 2026-10-30 03:43 | 1648 | 21 | 4c4fb6f | 4c4fb6f8 Autopilot objective: a decision the loop guard swallows no longer loses its step (gate q3s2 M07, run-; ee25e009 Q3 slice 2 (WORK_QUEUE): FLvibed's decision timers on one game clock; the server clock follows fixed ; 4ca36df7 Q3 (WORK_QUEUE) slice 1: fixed dt as a test tool (IDEAS T2) - the frame timer detoured, probes 1 and ; b376a215 Q2b (WORK_QUEUE) slice 1: the focus-graph crawler (IDEAS T7) + the LAN / INTERNET prompt's focus fix ; (+385 more) |
| M08 | 2026-10-22 04:52 | 691 | - | 024cf8e | first full pass |
| M08 | 2026-10-22 07:54 | 685 | 6 | a3059ca+dirty | a3059ca0 Selling at the commodity trader, natively: Invoke-FLTraderSell (the ship's cargo list NN_Inventory re; ab062b69 Objective escort: name match by words ("Sinclair and Quintaine"); a follow standing in for a refused ; 64c13c9e Combat: a wedged fight (no 30 m moved, nothing dealt in 6 s) breaks out away from the nearest solid (; 7d2e4378 Objective escort prefers the marked ship the objective names (M13 Sinclair, not Juni who follows us);; (+9 more) |
| M08 | 2026-10-22 14:03 | 676 | 9 | 3f5991e+dirty | 5a812688 A 'Fly to' a hostile place with a navmarker is a trip from the start; 665fb13d No blind spot on a moving target (> 30 m/s); 23855e09 Pressed only when the race is lost: mark kill time vs our time to die; 0bc887d4 Blind spots reachable through free space; the guard walls the target's hull on station / steer runs; (+36 more) |
| M08 | 2026-10-28 23:25 | 663 | 13 | 29c4d7c+dirty | c8b70fa5 Head-on pass and its hold only in a race we win (not while the race is being lost); 8da620ea No roll in a big turn (level-off waits for the nose within 10 deg, fight rolls for the lead within 25; ac96f7cb No missile run out of a big target's blind spot: its salvo goes from the spot; 66050055 Bank into turns behind roll_bank (off): unstable against the screen-space steering; the roll frame ve; (+385 more) |
| M08 | 2026-10-29 01:38 | 656 | 7 | 0332ec1+dirty | 0332ec1d Autopilot: steer in the game's turn frame (steer_level), roll_guns to a committed angle (roll_commit); 79cf4272 Gate mpb21 verdict: 12/14 PASS, 10 of 12 legs faster, PBs M01a / M02 / M06 / M08; fight matrix PASS 3 |
| M08 | 2026-10-30 16:39 | 655 | 1 | 4eef991 | 4eef9917 Q6 S7: the destroy conditions' seen lists are story memory (goal queue record / text v2) - kept in th; dfaa5efe Q6 S7 round 2: gate s7arb2 judged (M11 death = the static-hold hazard at the satellite, not S7; M13 =; 86bb9da2 Gate s7arb rows (situation_arbiter 1 on d8c498f; NOT kept - its parity bug fixed in b63c1ad): 12 OK, ; b63c1ad9 Q6 S7 fix: the arbiter's queue carries the old body's lastReal after its decision (Advance wrote it a; (+72 more) |
| M09 | 2026-10-22 07:39 | 899 | - | 7d2e437+dirty | first full pass |
| M09 | 2026-10-22 13:47 | 852 | 47 | 5fa8cd6+dirty | 5fa8cd60 Pressed targeting by the shield with hysteresis; db8aa510 Blind spots that shelter: blind_shelter scores the other hostiles' real-hull lines to each spot; 70850bcb Airlock: straight in only from in front of the door; past-mount test near its axis; staging beyond th; 33254c06 Dangerous attackers outrank the marks; ray end-point fix; wall slide; (+35 more) |
| M09 | 2026-10-22 18:38 | 834 | 18 | c772206+dirty | **hold-and-salvo at the arch generators (static_hold), launch rules at static targets; 4 generators 131 s**; 3fa5999e A mission's dock grant at an airlock is the way in: flown through; f0011979 Attack passes at static targets (static_pass, default; the orbit is the fallback); 5a812688 A 'Fly to' a hostile place with a navmarker is a trip from the start; 665fb13d No blind spot on a moving target (> 30 m/s); (+2 more) |
| M09 | 2026-10-24 12:17 | 731 | 103 | 7ccb022 | 7ccb0229 stale_destroy_wait: an unmarked destroy goal does not replace a step's own trip either (M09 'Fly to t; 5de66f6e stale_destroy_wait (an unmarked destroy goal only the last objective stands in for is a later step's:; 88ab4193 towards_nag (a fight objective after one with an Act_NagDistTowards is fought on the way - M09's patr; 69f574e2 ref_line: the rotation ray-tested and stepped back toward the reference's azimuth when blocked (s11s:; (+143 more) |
| M09 | 2026-10-24 12:29 | 724 | 7 | 7ccb022+dirty | (uncommitted changes on 7ccb022) |
| M10 | 2026-10-22 08:44 | 1498 | - | c608521+dirty | first full pass |
| M10 | 2026-10-23 13:24 | 1379 | 119 | 909a9fd | c6ad02e4 Leash follow-back: our trip toward the ship (sphere avoidance), not the game's follow (M10 s8n: the f; 84d297ad Leash: also off when the step's navmarker lies within 2 km of a jump hole / gate (M10's Hamburg step ; 5c3b388a Leash: off when the step's target is a jump hole / gate (M10's Hamburg hole step has a blank text - t; 1be483e6 Leash anchor: also by the pilot's name text (RepName vs IdsText of individual_name) - Contact::nameId; (+72 more) |
| M10 | 2026-10-23 17:07 | 1171 | 208 | b34853d+dirty | b34853dd Objective: no script goals from a stand-in (last known) objective when the step has its own navmarker; ff00ef55 Airlock run: the travel's unstick (forward thrust, no forward speed 1.5 s -> back off and slide 2 s) ; fd81832f Static hold on the side the ship leaves to: the objective mode's leave points (the destroy goal's oth; 70ceef59 Leashes disarmed by the script: a leash whose deactivating trigger waits on a comm line ends once tha; (+13 more) |
| M10 | 2026-10-24 16:39 | 1143 | 28 | b892e7c | 694fd913 blast_flank (on, untested): no missile / torpedo launch with another solid beside the ship within the; b22ab14a M10 stalls and the exit fail from the logs: a dock trip to the step's base that finds no way (script-; 766faaec Set M12-s11v (ref_line sanity on the review-fixed build): 6/6 PASS, core 13.3-18.2 s; 44b14c8a Review fixes (fresh-eyes pass): flleg catches deploy's own throw in the boot retry and checks the pre; (+93 more) |
| M10 | 2026-10-24 16:58 | 1128 | 15 | b892e7c+dirty | (uncommitted changes on b892e7c) |
| M10 | 2026-10-24 16:59 | 1125 | 3 | 84310e2+dirty | - |
| M10 | 2026-10-29 12:53 | 1115 | 10 | 8c5c9a3 | 8c5c9a39 Universe tables on ini::Read (core\game\universe_data): the autopilot sees minefields, lethal cores a; 6ae73f5d Controller: the camera's projection read once under a guard (fl::ReadCamera), offsets named; 230be464 fllegrun: pass only parameters with a value (an empty -ApSet killed every leg at its boot); M01a set ; e3e6e952 Harness: every install path through Get-FLInstallDir (the instance's), none hard-coded to the main in; (+248 more) |
| M10 | 2026-10-30 02:25 | 1095 | 20 | ee25e00 | ee25e009 Q3 slice 2 (WORK_QUEUE): FLvibed's decision timers on one game clock; the server clock follows fixed ; 4ca36df7 Q3 (WORK_QUEUE) slice 1: fixed dt as a test tool (IDEAS T2) - the frame timer detoured, probes 1 and ; b376a215 Q2b (WORK_QUEUE) slice 1: the focus-graph crawler (IDEAS T7) + the LAN / INTERNET prompt's focus fix ; e38ca61e Q2 (WORK_QUEUE): device-layer test pads - XInput slots served from shared memory (IDEAS T36 slice 1); (+25 more) |
| M10 | 2026-10-30 11:46 | 1093 | 2 | d0715f7 | d0715f73 Q6 S5: the rest of ObjectiveDecide (lane / pass / route / arrive / navmarker / rest) as pure Rule_* i; 16a4b638 Q6 S4: ObjectiveDecide from the pre-stage to a ship target as pure Rule_* in shadow (situation_shadow; c79fc352 Q6 S3: the arbiter's first rules as pure Rule_* beside ObjectiveDecide, in shadow behind situation_sh; 63291e3e Q6 S2: the live trigger table as the situation assessor's story source (A39) - behaviour-neutral, not; (+15 more) |
| M10 | 2026-10-30 13:04 | 1090 | 3 | d8c498f | e4e4d1a6 Q6 S5 gate s5shadow (situation_shadow 1, d0715f7 src:clean): 487 agree / 0 differ over 23 rules and 1; 08346a9f Q6 S7: the executor and the switch situation_arbiter (default 0 = today's decisions) - ObjectiveDecid; ae5ba160 Q6 S6: the goal queue (pure, behaviour-neutral) - A38 GoalRecord + condition language + validator, re |
| M10 | 2026-10-30 15:56 | 1079 | 11 | 4eef991 | 4eef9917 Q6 S7: the destroy conditions' seen lists are story memory (goal queue record / text v2) - kept in th; dfaa5efe Q6 S7 round 2: gate s7arb2 judged (M11 death = the static-hold hazard at the satellite, not S7; M13 =; 86bb9da2 Gate s7arb rows (situation_arbiter 1 on d8c498f; NOT kept - its parity bug fixed in b63c1ad): 12 OK, ; b63c1ad9 Q6 S7 fix: the arbiter's queue carries the old body's lastReal after its decision (Advance wrote it a; (+1 more) |
| M11 | 2026-10-22 07:43 | 1196 | - | 636f9ef+dirty | first full pass |
| M11 | 2026-10-23 14:57 | 1169 | 27 | b2fe700+dirty | 30b8fb1d Way planning: the planner before an 'out first' elbow, and a failed plan tried again once the ship is; 73e87325 Objective: a Dock objective overwritten by a nag goes on while only a placeholder text follows it (M1; dd64b644 Travel: no waypoint-list wait for a short trip in this system in the objective mode (under 6 km) (bui; a5caabbb Travel: in the objective mode the waypoint list is waited for only in the step's first 1.5 s (M12: ev; (+92 more) |
| M12 | 2026-10-22 13:59 | 627 | - | 5a81268+dirty | first full pass |
| M12 | 2026-10-23 06:26 | 454 | 173 | 0cafa8f | 84bd3186 Path follow: plan again when more than 250 m off the leg flown or 8 s without 50 m of the way gained ; 75a2d8f0 M12 airlock: the planned way only beyond 400 m of p2, judged by the line to p2 (the run orbited p2 at; f718e4ab M12 airlock: revert the beside-the-door straight-in rule and its turn-speed companion (3 of 3 runs ci; eb340fd7 M12: no dodges while hopping between static targets (untested); dossier + handoff: since the 480 s PA; (+18 more) |
| M12 | 2026-10-23 06:36 | 407 | 47 | a061c04+dirty | a061c043 M12 session 8 fixes (untested): 8 km trigger chained through to the 3 km ring trigger it arms (cruise |
| M12 | 2026-10-23 06:46 | 404 | 3 | 52e6d90 | 1f9a39e0 Objective trips: a new trip to within 150 m of the running one's destination keeps it (M12 gen 3 -> a |
| M12 | 2026-10-23 07:07 | 394 | 10 | a144c01 | a144c01a Ways through structure: an elbow first (one via point over the straight line at the destination's hei; 504d7b09 Objective trips: a trip to the object the running one goes to is kept (M12 Omicron Minor hole: the re |
| M12 | 2026-10-23 07:27 | 393 | 1 | 250c218 | 250c218e Ways: the out-first leg also level (the ray probe: from the M12 gen 3 kill point a level slide to the; 278acc9f Ways: out first, then the elbow (8 headings x 2 climbs, 250 / 450 m, at most 1.6x straight) when no d |
| M12 | 2026-10-23 13:32 | 392 | 1 | ff430f5+dirty | ff430f51 Way following: skip a corner within 3 s of speed when the next one is in plain sight for the ship's w; c6ad02e4 Leash follow-back: our trip toward the ship (sphere avoidance), not the game's follow (M10 s8n: the f; 84d297ad Leash: also off when the step's navmarker lies within 2 km of a jump hole / gate (M10's Hamburg step ; 5c3b388a Leash: off when the step's target is a jump hole / gate (M10's Hamburg hole step has a blank text - t; (+14 more) |
| M12 | 2026-10-23 14:23 | 389 | 3 | 30b8fb1+dirty | 30b8fb1d Way planning: the planner before an 'out first' elbow, and a failed plan tried again once the ship is; 73e87325 Objective: a Dock objective overwritten by a nag goes on while only a placeholder text follows it (M1; dd64b644 Travel: no waypoint-list wait for a short trip in this system in the objective mode (under 6 km) (bui; a5caabbb Travel: in the objective mode the waypoint list is waited for only in the step's first 1.5 s (M12: ev; (+2 more) |
| M12 | 2026-10-23 18:02 | 388 | 1 | ac62572+dirty | ac625721 M12 user feedback 23 Oct: hops - a disrupted charge near the generator gives way to the afterburner (; 9ff04d58 Revert "Fast dock: a jump gate / hole is requested ~1.5 km out while cruising, so the game's dock aut; 5aad202a Revert "Fast dock: planet docking rings also requested ~1.5 km out while cruising (speedrun runs 1 / ; 21686e7f Arc planner: depths 0-46 % and 60 m lifts above / below mid-way; a log of which chord blocked when no; (+18 more) |
| M12 | 2026-10-23 18:12 | 375 | 13 | f7bfc52+dirty | f7bfc522 Arc planner: log the structure's radius when an end is too near its middle; 8961c9a0 Arc planner: the first / last chords untested (the generator's own hull at the hold points blocked ev; 227cbfda Core line: under the cruise lock a fly-through's rejoin of its line of approach is the way, untested  |
| M12 | 2026-10-23 19:58 | 372 | 3 | f116850 | f116850a Autopilot: the wheel scan also probes along the motion; a6314d75 Autopilot: the airlock run times a rotating part on its way too; 05f62194 Autopilot: the pursuit brakes for a big target by the flight model's stop; 756ea21b Autopilot: at a close static hull the nose decides a launch, not the seeker cone; (+12 more) |
| M12 | 2026-10-23 21:07 | 366 | 6 | 60e4f90+dirty | 60e4f90d Autopilot: a trip into a script's sphere beside an airlock stages on the door's line; 0e62ab10 Revert "Autopilot: a fly-through sphere over an airlock is aimed at its centre"; 671a773e Autopilot: a fly-through sphere over an airlock is aimed at its centre |
| M12 | 2026-10-23 22:16 | 364 | 2 | 13f8632 | 0a40dba2 Avoid: the wheel's crossing ring is the smallest at least half free; 79ead461 Autopilot: the passage point first, the parallel line through the plate; 210746d4 Revert "Autopilot: the cruise-lock rejoin's farthest point needs a speed margin"; 4e6e6ef7 Autopilot: a cruise charge reset under fire burns away at once; (+5 more) |
| M12 | 2026-10-23 23:47 | 362 | 2 | 4cad6dd+dirty | 4cad6dd6 Legs: hull damage from a minefield fails the leg (the user, Zone 21); 70ef3aaf Autopilot: drop the "far spokes" code; the bump event skips the pursuit's own braking; bf7dc5de Autopilot: under fire, a lined-up fast run keeps its speed to the normal request point |
| M12 | 2026-10-24 02:00 | 359 | 3 | 29fabbc+dirty | 29fabbcd Autopilot: the user's M12 lair notes as switches (all off): airlock_charge_at (the share of the charg; 799eb511 Autopilot: ring_request (planet ring requested at p2 at full speed) and airlock_charge_lead switches,; fac712ad Splits: runner matches bounded by the other runs' segment (an unread banner no longer matches one min; 6d4f5b0d Testing overhaul: story checkpoints, speedrun splits, par, fail limits (the user, 24 Oct 2026); (+3 more) |
| M12 | 2026-10-24 02:00 | 355 | 4 | 29fabbc+dirty | (uncommitted changes on 29fabbc) |
| M12 | 2026-10-24 06:08 | 354 | 1 | 3d102cd+dirty | 3d102cdb jumpin_burn: after a jump-in only (the trip-start window would also hold re-planned charges, e.g. bef; c6fcfe0a Omicron Minor disruptors: charge_cm (CMs while charging near a learned disruptor launcher), jumpin_bu; 57c27013 salvo-check logs the hold's gap, the torpedo need and the distance to the hold point (probe: gen 3 ho; ba961302 Generator holds: leave_on_kill v2 (the finishing torpedo under it; DoomedWatch logs the left target's; (+22 more) |
| M13 | 2026-10-22 11:35 | 1194 | - | ce50649+dirty | first full pass |
| M13 | 2026-10-28 19:24 | 909 | 285 | f72304f+dirty | f72304f5 Autopilot: odds_shield switch (A/B of 9212cc3's shield-point rate); Follow holds off 2.5 s after the ; a372482e Gate mpb0 results (baseline at 35f8632; M01a PB 511 s); MP death dialog on the pad verified; handoff ; 0689c297 Level-off on straight travel runs; the LAN list's IPv6 fix shipped (vanilla bytes only); roll in the ; 9212cc3a Autopilot: the exchange estimates count shield damage dealt; Follow rests behind a stopped leader; (+374 more) |
| M13 | 2026-10-29 13:08 | 892 | 17 | 8c5c9a3 | 8c5c9a39 Universe tables on ini::Read (core\game\universe_data): the autopilot sees minefields, lethal cores a; 6ae73f5d Controller: the camera's projection read once under a guard (fl::ReadCamera), offsets named; 230be464 fllegrun: pass only parameters with a value (an empty -ApSet killed every leg at its boot); M01a set ; e3e6e952 Harness: every install path through Get-FLInstallDir (the instance's), none hard-coded to the main in; (+40 more) |
| M13 | 2026-10-30 13:19 | 879 | 13 | d8c498f | e4e4d1a6 Q6 S5 gate s5shadow (situation_shadow 1, d0715f7 src:clean): 487 agree / 0 differ over 23 rules and 1; 08346a9f Q6 S7: the executor and the switch situation_arbiter (default 0 = today's decisions) - ObjectiveDecid; ae5ba160 Q6 S6: the goal queue (pure, behaviour-neutral) - A38 GoalRecord + condition language + validator, re; d0715f73 Q6 S5: the rest of ObjectiveDecide (lane / pass / route / arrive / navmarker / rest) as pure Rule_* i; (+47 more) |
| M13 | 2026-10-30 17:32 | 864 | 15 | 51fd767 | 51fd7675 shortcut_rush switch (default 1 = unchanged): 0 = no queue-shortcut rush and no queued take-over at g; c6f601d8 options help: a help text that needs more than three lines above the page widens to the screen's left; 4f6900d6 Gate s7arb3 (4eef991, situation_arbiter 1): 13 PASS, M11 death = the Zone 21 vertical sag (pre-existi; 6de97e08 Living spec v54 published (the owner's standing OK): 85b0839's haptics / pad_backend rows + 7aca17a's; (+6 more) |
| M13 | 2026-10-31 01:27 | 785 | 79 | 95a9a4a | 95a9a4a0 kill_ahead fight-step form: the destroy goals ahead of a fight step fall back to the armed rows' (mis; 131f8813 kill_ahead fight-step form: a 'defend' step with no object to escort that we fight in counts as a fig; bafeb642 M13-MIMIC bundles A + B (all default 0): battery_budget reworked, static_full_dps, bs_approach; adcea3f0 M13-MIMIC item 3b: hold_shelter (default 0) - the static hold's points scored by open sky (26 rays to; (+19 more) |
| M13 | 2026-10-31 01:42 | 783 | 2 | f85b9d8 | f85b9d8d big_cruise_in (default 0): the cruise in to a huge static target goes on under fire (R1 cruises 13 s ; 8d075739 bay_request_lined (default 0): a fast dock at a bay requests only within 40 m of the bay's axis, flyi; fcfa83bb bs_approach: the launch-site check at a blind-spot target without launch_site_check everywhere (the r; cd9fcd9d battery_budget: a failing shield under fire at a big target is always refilled, whatever the hull - t; (+1 more) |
| M13 | 2026-10-31 02:12 | 756 | 27 | ae78f55 | ae78f55f battery_budget: the floor past a share is 4 a kill still ahead (the city enters with 8; run-judge on ; 10f5961c battery_budget (not built yet - the gate m13afk1 runs on c1418a6): no big fight ahead keeps nothing (; 32410f66 bay_request_lined: off a bay's axis the brake keys on the way to p2, not the axis distance - ab6-b_i4; 62c07305 battery_budget: past a big fight's share a failing shield gets 3 units (back online), not a full bar  |
| M13 | 2026-10-31 05:13 | 746 | 10 | 136ef81e | 6ccb0e00 Audit A1-4 (part, the find): a goal 'wins' by leading to a landing only through a '_' token starting ; 23503988 Audit A1-3 (speedrun knowledge leaks): RefLineAim aims only with Knows(Source::Meta) (knowledge all);; aa79f7dd city_hold_side transit: the station output moved into the off-point branch, after the hop - probe ch2; 2ff77132 M13 probe ch2 (city_hold_side transit + clearance, 3 legs): all PASS 780.8 / 788.0 / 776.0 s; M13.end; (+20 more) |

(generated by tools\legtable.js from legs.tsv, 2026-10-31 17:50)
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
- `controller\tools\missions\Leg.ps1`: one leg by one set of rules - docked: `Complete-FLBaseStory -AcceptStory`
  (story talks, story offers accepted, scenes skipped, the objective's room, a room tour while launching is not
  allowed, repair + restock, launch); in space: the objective mode. Ends on the mission's result / -Until / time-out /
  -BudgetSec (OVER BUDGET stops a leg that is off pace at once). Phase table (base / space times) with -Out. Prints a
  SPLIT line at every checkpoint and fails the leg on a checkpoint limit (`docs\TESTING.md` "Speedrun splits"; `-Full`
  from the leg's start save; `-ReplayLog <leg folder>` replays an archived log through the same check).
- `tools\splits.py`: the checkpoints' splits - runners, par, ours, PB / gold, limits, `docs\SPLITS.md`, LiveSplit files.
- `controller\tools\flleg.ps1 -Instance n -Save s -Label l`: one leg on its own game instance from a save, evidence in
  `research\experience\legs\<time>_<label>_i<n>\` (transcript, phases.tsv), history `research\experience\legs\legs.tsv`.
  Several legs at once: one flleg per instance (build once first: `tools\deploy.ps1 -NoStart`), check-ins with
  `tools\flcheckin.ps1 -Instances 1,2,3`. A leg that does not pass (death, mission failure, stall / time-out) keeps the
  last 15 s of the sampler's 5 fps recording beside its logs: `end_tail.mkv` + `end_sheet.jpg` (every frame,
  time-stamped, with the HUD's speed strip; user, 22 Oct 2026: dense frames at every death / failure / stall). Deaths
  that take 0.2 s need more than 5 fps: an in-game ring buffer is the next step (HANDOFF).

## Status and bests
Per checkpoint and per leg: `docs\SPLITS.md` (generated after every leg: par, our PB, gold, sum of best, spread, the
fail limits); whole legs: "Targets" above. The per-leg status notes of 22-23 Oct moved to `..\HANDOFF_history.txt`.
**M01b note (24 Oct 2026):** the runners' M01b ends at "GAME SAVED" after the Rogue base; our leg flies on to land on
Pittsburgh (~100 s more), so its whole-leg time above is not comparable - the checkpoint `M01b.end` (objective 22055,
shown with GAME SAVED) is: PB 572 vs par 457.
Race practice (`controller\tools\flrace.ps1`, fixture `research\saves\legs\SaveC707.fl`): best lap 74.1 s (= the
user's 2020 lap, `kb\missions\M06_race_footage.md`). Checkpoint saves of probes: `research\saves\legs\` (the game's
own mission saves copied; any name loads).
