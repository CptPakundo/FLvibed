# Scene saves (test start points; every mission)

Re-tests start seconds before their scene instead of replaying a mission (user, 18 Oct 2026). **Catalogue:**
`docs\missions\scene_saves.tsv` (versioned); the files in `research\saves\scenes\SaveSnnn.fl` (git-ignored; `S` is
never in the game's own hex save names).

- **A native save restores a story mission exactly where it was made** (verified 18 Oct 2026, M02: saved after lane
  146 -> reloaded at the same spot, objective "Dock with the Trade Lane" on ring 16 = step 4, King respawned beside the
  ship), so scene saves can sit anywhere; the mission's own checkpoints (`Act_Save`) are archived too.
- **Make:** a mission script run with `-SceneSaves` saves at its scene points (`Save-FLScene <mission> <step>
  <label>`: debug `save` = server.dll `pub::Save`, the function `Act_Save` calls) and archives the checkpoints the
  mission wrote (`Save-FLCheckpoints`, from the `mission-save ids=` events). Re-making a scene replaces its file.
- **Use:** `Start-FLScene <mission> <step> [label]` restarts the game on that save and checks it: the objective after
  loading against the recorded one, the game build (Freelancer.exe + Content.dll hashes) against the recorded build
  - a mismatch is reported STALE (re-make it with a fresh `-SceneSaves` run). Then run the mission script `-From <step>`.
- **Adding a mission:** in its script, `Scene <step> '<label>'` right where a re-test should begin (after the step's
  objective is shown, before acting), and `-From <step>` entry for that step.
- **Audits / health checks:** run each mission script with `-SceneSaves` (refreshes every scene and checkpoint for
  the current build), then any scene can be re-tested in isolation.
- **Note:** the game writes `Act_Save` checkpoints, `AutoSave.fl` and `Restart.fl` into its save folder during every
  run - the test install's own (`SAVE\`), never a personal install's (FLvibed\CLAUDE.md, Testing).

| Mission | Step | Label | Why |
|---|---|---|---|
| M02 | 8 | scan-stop | the waypoint wait + scan (hold behaviour, scan) |
| M02 | 17 | jailbreak-fight | the fight where the stray missile failed the mission |
| M02 | 18 | missouri-leg | lane 178 + the Missouri dock (dialogue buffer before docking) |

# Campaign saves (every mission)

48 single-player saves through the whole story, shared by BC46 on a Freelancer Discord server (late 2023; files
dated 22 Aug 2021), given to us by the user on 18 Oct 2026. Local only: `research\saves\bc46-all-missions\`
(git-ignored), decoded copies in `decoded\` (`node tools\flsave.js <dir> [<out>]`: catalogue + decoding).

**Use:** `tools\deploy.ps1 -Save <path>` copies one save into the test install's save folder
(`Install\Freelancer HD Edition\SAVE\Accts\SinglePlayer`, `Get-FLSaveDir`; never a personal install's) and loads it.
Copy only the save a test needs. Never use this set's `AutoSave.fl` / `Restart.fl` (the game's own slots: copying
one in replaces the install's). Saves in space mid-mission can sit inside a timed phase: read the mission's runbook first
(`docs\missions\Mxx.md`), and check the objective with `Get-FLStoryState` after loading.

Ships (archetype by `tools\flhash.ps1`): ge_fighter (starter), li_elite, br_elite, bh_elite, or_elite.

| Save | MissionNum | Mission | Where | Rank | Credits | Ship |
|---|---|---|---|---|---|---|
| Restart.fl (do not copy) | 1 | Mission_01a | space FP7_system | 0 | 500 | ge_fighter |
| Save7e1c.fl | 2 | Mission_01a | Li01_01_Base (Manhattan) | 0 | 10000 | ge_fighter |
| Save08f1.fl | 2 | Mission_01a | space Li01 | 0 | 8145 | ge_fighter |
| Save7757.fl | 2 | Mission_01a | space Li01 | 0 | 8145 | ge_fighter |
| Save5f61.fl | 4 | Mission_01b | Li01_02_Base | 1 | 10895 | ge_fighter |
| Save6bc5.fl | 4 | Mission_01b | space Li01 | 1 | 11492 | ge_fighter |
| Save71d8.fl | 4 | Mission_01b | space Li01 | 1 | 11492 | ge_fighter |
| Save75f0.fl | 4 | Mission_01b | space Li01 | 1 | 11492 | ge_fighter |
| Save05c7.fl | 8 | Mission_02 | Li01_01_Base (Manhattan) | 3 | 30000 | ge_fighter |
| Save4cac.fl | 8 | Mission_02 | space Li01 | 3 | 30000 | ge_fighter |
| Save29e0.fl | 8 | Mission_02 | space Li03 | 3 | 30000 | ge_fighter |
| Save2d91.fl | 8 | Mission_02 | space Li03 | 3 | 30000 | ge_fighter |
| Save415f.fl | 8 | Mission_02 | space Li03 | 3 | 30000 | ge_fighter |
| Save1e7d.fl | 11 | Mission_03 | Li02_02_Base | 5 | 39291 | li_elite |
| Save2de9.fl | 11 | Mission_03 | space Li02 | 5 | 39291 | li_elite |
| Save302a.fl | 11 | Mission_03 | space Li02 | 5 | 38500 | li_elite |
| Save6380.fl | 14 | Mission_04 | Li01_01_Base (Manhattan) | 7 | 69000 | li_elite |
| Save44a6.fl | 14 | Mission_04 | space Li01 | 7 | 64836 | li_elite |
| Save1d87.fl | 17 | Mission_05 | Br03_01_Base | 9 | 90513 | br_elite |
| Save2b5b.fl | 17 | Mission_05 | space Br03 | 9 | 90754 | br_elite |
| Save6d0d.fl | 17 | Mission_05 | space Bw01 | 9 | 90513 | br_elite |
| Save7cb1.fl | 17 | Mission_05 | space Bw01 | 9 | 90513 | br_elite |
| Save33fa.fl | 20 | Mission_06 | Br05_01_Base | 11 | 161110 | br_elite |
| Save245a.fl | 20 | Mission_06 | Br05_02_Base | 11 | 161110 | br_elite |
| Save2c85.fl | 20 | Mission_06 | space Br05 | 11 | 161110 | br_elite |
| Save766f.fl | 23 | Mission_07 | Br04_01_Base | 12 | 150591 | br_elite |
| Save5057.fl | 23 | Mission_07 | space Br04 | 12 | 144976 | br_elite |
| Save7d5b.fl | 23 | Mission_07 | space Br04 | 12 | 144976 | br_elite |
| Save0099.fl | 23 | Mission_07 | space Bw09 | 12 | 144976 | br_elite |
| Save4180.fl | 23 | Mission_07 | space Bw09 | 12 | 116010 | br_elite |
| Save051f.fl | 23 | Mission_07 | space Bw10 | 12 | 144976 | br_elite |
| Save3a48.fl | 26 | Mission_08 | Ku01_05_Base | 14 | 114297 | bh_elite |
| Save4686.fl | 29 | Mission_09 | Ku06_01_Base | 16 | 250000 | bh_elite |
| Save2d01.fl | 29 | Mission_09 | space Ku07 | 16 | 250000 | bh_elite |
| Save3b18.fl | 29 | Mission_09 | space Ku07 | 16 | 250000 | bh_elite |
| Save7b39.fl | 32 | Mission_10 | Ku06_01_Base | 17 | 222736 | bh_elite |
| Save475c.fl | 32 | Mission_10 | space Rh02 | 17 | 211317 | bh_elite |
| Save1c70.fl | 32 | Mission_10 | space Rh04 | 17 | 224090 | bh_elite |
| Save08f9.fl | 35 | Mission_11 | Li04_06_Base | 17 | 221317 | bh_elite |
| Save1148.fl | 35 | Mission_11 | Li01_12_Base | 17 | 257169 | or_elite |
| Save4554.fl | 35 | Mission_11 | space Li01 | 17 | 238567 | or_elite |
| Save02c1.fl | 35 | Mission_11 | space Li05 | 17 | 238567 | or_elite |
| Save5e67.fl | 38 | Mission_12 | St02_01_Base | 17 | 248567 | or_elite |
| Save1605.fl | 38 | Mission_12 | space St02c | 17 | 243806 | or_elite |
| Save3989.fl | 41 | Mission_13 | St01_01_Base | 17 | 253806 | or_elite |
| Save3177.fl | 41 | Mission_13 | space St01 | 17 | 228123 | or_elite |
| Save0e2b.fl | 42 | No_Mission (free roam after the story) | Li01_01_Base (Manhattan) | 21 | 227290 | or_elite |
| AutoSave.fl (do not copy) | 42 | No_Mission | Li01_01_Base | 21 | 227290 | or_elite |

Not verified yet: each save loads in HDE 0.7.1 with FLvibed, and where in its mission each one stands (objective,
open popups). The save descriptions are empty; MissionNum is the game's story counter.
