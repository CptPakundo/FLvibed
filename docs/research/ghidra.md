# Ghidra + an MCP bridge: the game's binaries decompiled on demand
Status: current
Checked against: FLvibed 97e118fb (31 Oct 2026 ~15:25, short form, 97e118fb landed during the round (docs only: AUTOPILOT.md records fx2 - the back-off fires, the stall not fixed; PRESETS / WORK_QUEUE record the lint baseline as run), read and consistent; 1 watched-path commit: f8e1dd31 (FREELANCER_RE: the engine-kill-at-cruise coast, measured from a motion trace, not decompiled); `core/flexe.h` unchanged since 5eb7bcb; the benchmark claims stand)
Dirty paths at check: another session's uncommitted `controller/src/impl/autopilot_impl.h`, `controller/src/impl/autopilot_objective.cpp`, `controller/src/impl/autopilot_settings.cpp`, `controller/src/impl/lookahead.{cpp,h}`, `core/fluf.cpp`, `tests/fluf_probe_test.cpp`, `tools/setwatch.sh` (not judged) (31 Oct 2026 ~15:25)
Watch: core/flexe.h, core/fldll.h, docs/FREELANCER_RE.md, tools/ghidra
Evidence: research\ghidra\ (the install, the project, logs; git-ignored); the benchmark's transcript is the session's (29 Oct 2026 AFK)
Sources: https://github.com/bethington/ghidra-mcp (v7.0.0 release, README), https://github.com/NationalSecurityAgency/ghidra (12.1.4 release), Adoptium API (Temurin 21); LaurieWired/GhidraMCP was the fallback, not needed

## What it means for FLvibed

*31 Oct 2026 ~04:00 (fc379a3, short form):* another use of the method this note benchmarks: a function in Common.dll decompiled for a gameplay question (FREELANCER_RE 'Missiles in flight': a guided round stores the whole target object, so a burst cannot be aimed at a part). The note's status and the benchmark stay as they are; no new tool or limit was found (the decompile's reliability is the FREELANCER_RE row's own [M] mark, not rechecked here).

*30 Oct 2026 ~11:50 (16df8bd, short form):* FREELANCER_RE.md grew after the 05:18Z seed (4e1c8e7: space ids stable
within a loaded game; d488fbd: the load's own moment - Content.dll's loader notification comes before the new mission
runs, its server.dll imports already bound), so drift_check's `ghidra` line will call the seed due again; neither row
carries a new Freelancer.exe address (flexe.h unchanged, 217 symbols), so a re-seed adds no name. Neither finding used
Ghidra (both measured in game). No statement here changes.
*30 Oct 2026 ~07:25 (aafe41d, short form):* **the seed blocker is fixed** (8db7d81, H17): the lock was stale (Windows
gives no command line for the server `mcp_launch.js` starts detached, so `-Stop` never found it) - `ghidra.ps1` now
finds the server by its loopback port and moves a lock no process holds aside; the seed ran (217 symbols, 6 s) and the
next `gq.js` query restarted the server. Ghidra then answered a Q4 question: `[Munition] detonation_dist` read at
Munition +0xA4 (the proximity fuse, 217a972, not verified in game). FREELANCER_RE gained that row and the statics'
guns (29b6ae2) after the 02:09 seed; re-seeded 30 Oct 05:18Z (`ghidra.ps1 -Check`: today's flexe.h and FREELANCER_RE.md).
*30 Oct 2026 ~01:50 (4ca36df, short form):* the frame timer for T2 was read with Ghidra and then verified in game
(4ca36df: six symbols, `FREELANCER_RE.md` the 0x42D770 row) - the cross-check the 04b4c05 check-up asked for is done.
flexe.h now has 217 symbols and FREELANCER_RE.md gained the frame-timer row (4ca36df) and a modal-prompt line
(b376a21), so the seed is behind both; **`ghidra.ps1 -Seed` was not run** (the project was locked by the session's own
`ghidra` MCP: WORK_QUEUE H17) - drift_check's `ghidra` line keeps it due. A lesson from the same commit for every
lookup that ends in a symbol: a data symbol inside a function that gets a detour must be generated after the detour's
code symbol (sigtool HOOK-OVERLAP; skill `new-exe-symbol` step 3).
*29 Oct 2026 ~13:20 (bf5da3b):* the project also carries `docs\FREELANCER_RE.md`: its 634 Freelancer.exe addresses
as comments (a decompile shows the verified facts in its plate comment; re-applied by `ghidra.ps1 -Seed`), and
drift_check's `ghidra` line notices a seed older than flexe.h or the doc. K11 answered from the code (five dialogs take
USER_ENTER). K12 partly done.
*29 Oct 2026 (9249f71):* adopted (the owner's OK that day). The 18 binaries of the test install are in a headless
Ghidra project, Freelancer.exe named with all 211 flexe.h symbols; the `ghidra` MCP server and `tools\ghidra\gq.js`
answer "what does this do / who calls it / who writes this field" in seconds, where the static listing needed manual
reading. The `game-facts` agent uses it with its own standing ("from decompiled code", below "verified in game"); the
`new-exe-symbol` skill looks up candidates there. Three new reverse-engineering leads from the benchmark: K9 (lane
ride and cruise charge), K10 (the current room read directly), K11 (USER_ENTER per screen); K12 (more seed names).

## Set-up (tools\ghidra\README.md has the operating detail)
- Ghidra 12.1.4 + Temurin JDK 21.0.12.1 + GhidraMCP 7.0.0 (the extension declares `version=12.1.4`: an exact match is
  required). The release jar already contains `com.xebyte.headless.GhidraMCPHeadlessServer` (the README says to build
  it with Maven's headless profile; not needed). The bridge (Python, `bridge-mcp-ghidra`) installs from the release
  wheel with uv into a local venv. Loopback only, bearer token, file access limited to `research\ghidra\`, program
  selectors required (the bridge refuses a call without `program=`, so concurrent sessions cannot act on the wrong
  binary).
- Import + analysis: Freelancer.exe 64 s of analysis (79 s with the import), all 18 ~6 min, 8 cores.
- **Analysis misses vtable-only code**: the first benchmark function (Common.dll's LaneAction update, RVA 0x619D0) had
  no function. `FLvibedVtables.java` creates the functions behind every vftable symbol and every code-referenced run
  of code pointers in data: ~6,200 over the 18 (Freelancer.exe +1,200, Common.dll +464, Alchemy.dll +2,034).
- MCP handshake through Claude Code's own path (stdio, `node tools\ghidra\mcp_launch.js`): 3.8 s from a running
  server, 75 tools listed at the handshake (67 of the `listing,function,program` groups + 8 bridge tools; lazy loading), the flexe.h names and comments come through.

## Benchmark (29 Oct 2026; the `game-facts` agent, Sonnet, with gq.js; ~10 min, 86 tool calls)
Three questions earlier sessions had left open (FREELANCER_RE.md now holds the answers, each "from decompiled code,
not verified in game", with the probe that would verify it):
1. **The lane ride and a running cruise charge**: a negative result no listing grep gives - no LaneAction code (start,
   update, end) touches the CEEngine; the charge clock (`CEEngine::Update`) ignores the control exclusion; only
   `CEEngine::Activate(false)` calls EngageCruise in Common.dll. Left open: three end-of-ride calls into the exe.
2. **The current base room**: the location controller 0x668708's field map (+0x34 base, +0x38 room id, +0x3C, +0x40
   RoomData with the nickname at +0xC), its setter / clearer. Spot-checked by hand (the setter writes +0x38..+0x40;
   RoomInfo's signature reads +0x38).
3. **Accept / back**: the command ids (CANCEL 0x0F, ENTER 0x11, NO 0x12, YES 0x13), the per-screen handler
   registration (0x5760D0, ~50 constructors), the untaken-CANCEL fallback, the pause menu's and offer dialog's
   handlers. Conclusion: no single game-side accept / back; `confirm_button` stays a pad-side remap.
Verdict: worth it. The agent said itself which answers needed Ghidra (all three); it marked the unresolved parts
instead of guessing. Cost: the agent's ~150 k tokens; the static-listing route had not answered any of the three.
