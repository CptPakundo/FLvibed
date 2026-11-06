# Hostile NPCs: how they decide, why they can stall, what a difficulty system could pull
Status: current
Checked against: FLvibed 3c9c1390 (6 Nov 2026, short form: 34 watched-path commits: the module split P0-P11e (moves, comments, map rows) (the NPC module, 68dc8f32), 07ac7595 the recorder; no real stall recorded yet, issue 3 open; D2-D4 still the owner's), HDE 0.7.1 (the test install), upstream HDE main c0c70a1924, FLSharp 0.9.x
Dirty paths at check: another session's uncommitted the private work queue, `tools/uimock/options.py` (not judged); 950b25cc, 4a270d9e and 3c9c1390 landed during the round, read
Watch: core/game/ai.cpp, core/game/spawn.cpp, modules/npc/src, modules/controller/src/host/main.cpp, docs/FREELANCER_RE.md
See also: `docs\research\player-ship-ai.md` (can the game's AI fly the player's ship)

Why: GitHub issue 3 (two players) and the owner (4 Nov 2026): enemies sometimes sit at 0 speed, seen at the start of M01's
Donau fight, not every run. The owner's scope: **declining a fight the NPC judges too tough is expected game behaviour; the
bug is a stall.** The owner also sees an avenue to dynamic difficulty, scope undefined. Raw notes, tables and sources:
`research\evidence\npc_ai_2026-11-04\` (`mechanics.md`, `community_and_dda.md`, `hde_upstream.md`, `footage_baseline.md`,
`idle_fights.tsv`, `toughness.tsv`, `stalls_fights.tsv`, `stalls_donau.tsv`). Standing of each claim: in those files.

## The stall: state of the hunt
**Not reproduced, cause not found.** A stall = a hostile ship moving under 2 m across 3 s or more, sampled at 2 Hz by debug
`track`. Positive control: pausing the game flagged all three test ships; running it flagged none.

| Test | Runs | Stalls |
|---|---|---|
| M01a from Save7e1c through the Order fight, autopilot flying, dev build f2cede02 | 7 | 0 |
| Six pirates d5, autopilot fighting, kills leaving loot | 6 | 0 |
| Three pirates, idle player, pad and keyboard (flat range only) | 2 + 12 | 0-6 % of samples, no freeze |

Seven clean Donau runs still allow a rate up to about a third (95 %). None of them is a human or an idle player.

**Ruled out for the Donau fight:** the loot freeze below: Order fighters use `story_job_fighter_a`, `loot_preference =
LT_NONE`. **Ruled out as a cause of the stall in general:** HDE data (0 of 301 population pilot sections differ from vanilla,
`state_graph.db` identical, no hull / weapon / shield stat changes, M01A's Order sections equal), upstream HDE and FLSharp
(all issues, PRs and commits since 0.7.1: no NPC stall, no AI change), and FLvibed writes to NPC state in default play (none
found: `hooks` are client side or pass-through observers; the Evade guard installs only for the autopilot's `dodge 2`).

**Open leads, best first:**
1. **Donau script order** (`hde_upstream.md`, not proven): `ol_order_donau_dead` has BreakFormation after StayInRange, the
   reverse of the other lists, and goes to `badboys` 2 s after the Donau's first hit, whose time depends on the AI: the one
   run-to-run variable found. A wingman broken off without a list would idle. Also `o_hostile4/5` exist only in the first of
   two `badboys` definitions. Settled by logging each fighter's directive.
2. **The loot freeze, for ordinary fights** (the GitHub report): stock Common.dll bug, an NPC trying to tractor loot it cannot
   take stands still until the loot goes. adoxa's Check Loot Volume patches it (Common.dll 0x62CE8E8, 0x62CE9D9; Crossfire
   and Discovery load it). HDE, upstream included, does not. 31 of 51 job blocks loot. Not reproduced here: the fights above
   kept every NPC busy with the player; the test wants a looting NPC free of targets beside oversize loot.
3. **Frame rate:** the project's own data shows step-dependent physics (`test-efficiency.md`); untested for stalls.

**The stall recorder is built** (`modules\controller\src\impl\stallrec.*`, rule `stallwatch.h`, setting `npc_stall_log`, on by default,
read only): a hostile within 6 km that stays within 2 m for 3 s of game time gets a record in `EXE\FLvibed_npc_stalls.log`: its
running behaviour, job, target, neighbours with their behaviours, loot within 2.5 km, objective, device, fps, then how it
ended. It tells the leads apart: `op=loot` is lead 2, a formation wingman with no running behaviour lead 1. Proven on a live
NPC by `stalls hold` (the sample held, not the ship: a brain overrides `ai` directives). Up to 200 records a session.

**Next:** a record from a real session; Donau runs with an idle player; the loot test.

## How NPCs decide
- **Scene test (verified in game 4 Nov 2026, `toughness.tsv`):** an NPC computes hostile / (own + hostile) from ranks and commits
  to no target above its job's `scene_toughness_threshold`: EASIEST 0.15, EASY 0.35, EQUAL 0.65, HARD 0.85, HARDEST 1.0. An
  NPC's rank is its template level; **the player's is wealth** through `PTough.ini` (rank 25 at about 433k, 40 at 3.13M). A
  lone HARD pirate: 1,000 credits, engages at every level; 500k, from level 5; 5M, at 8 (level 7 untested). It keeps flying
  when it declines. Story jobs are all HARDEST. So the game goes easier on the rich, a hidden difficulty running backwards.
- **Enemy clamp:** about two NPCs attack the player at once (`PlayerEnemyClamp` 2,2, global); story scripts set it per scene.
- **Flee:** hull under 10-20 % (fighters) to 75 % (traders), a fleeing leader, no weapons.
- **Skill is not level:** pilot tier is mixed at every level. Level picks the template, so hull, shield and guns, and its rank.
- **Weak by design:** gun cone median 5 degrees, fire duty 44-80 %, most missiles every 60 s, NPC shields regenerate 1/s
  against the player's 13-96/s, no countermeasures. `constants.ini` has no difficulty key.
- **Damage is not the weak spot:** idle players died in 7-21 s in 10 of 12 spawned fights (HARDEST job, one run per case,
  pad and keyboard alike). A moving player is another matter: not measured.

## Why fights can still feel easy with FLvibed (nothing here is a stall)
Pad only: `aim_mode` 1 snaps the reticle onto the selected hostile's lead anywhere on screen; a test bot was on target 25-28 s
of 45 with it, 6 with stick and assists, 1 with neither (`..\..\modules\controller\docs\FLIGHT_AIM.md`; no human data). Both devices:
HUD 2's cues and alerts. Footage (2020 owner, runner, autopilot runs; M01a, M02): levels, hull and shields read the same,
damage intake about equal, no NPC holding still in any era, but no pad play is recorded and no run compares mod on and off.

## Dynamic difficulty: what the research supports
Not designed: the owner has set no scope. Community: no plugin adjusts skill during a fight. Match Player, Post Story Diff and
Match Job act at spawn or on the job board, and none ships with HDE.
- **Levers, most native first:** encounter level and density (system ini, or adoxa's Content.dll hook), the enemy clamp (live),
  a live pilot override (`core\game\ai.cpp`), spawn level or rank, the job's threshold, template or loadout. Avoid damage
  multipliers: they are symmetric with the player.
- **Signal:** a short window of recent combat outcomes, never worth or level. Elite Dangerous made the same move to combat rank.
- **Rules from other games:** change at encounter boundaries, never mid-fight (God Hand); zone bands and caps, not matching
  the player (Skyrim, Everspace 2); pacing levers before stat changes; leave scripted story ships alone; off in timed
  categories, and log every change, because hidden rank becomes a route step (RE4, shmups) and would void par and splits;
  optional and disclosed, default vanilla, separate knobs (Celeste, Hades); higher pay for harder play; in multiplayer
  server-side, per group, at spawn or not at all.
- **Project conflict:** CLAUDE.md says hooks never change game logic; difficulty does, so it needs the owner's word and its own
  opt-in module.
