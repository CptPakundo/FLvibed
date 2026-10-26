# The upstream HD Edition as a test install: switch now or later?
Status: current
Checked against: FLvibed 01fcfa9 (26 Oct 2026, the step 3 short form re-confirmed: the only watched commits since, 9327f86 / 01fcfa9, are doc and handoff edits recording what this note already says); before: FLvibed e5c02cc (26 Oct 2026, step 3 short form: 202 / 202 on four exes; borderless and Reset fixes - first "What it means" line); before: FLvibed 855745b (25 Oct 2026 ~23:05, whole-set check-up: 15 watched-path commits 073a665..855745b - the vanilla 1.1 support and the signature re-cut (4042c7d..0d0bcb7, 68755f1), Extended HUD 2 / HUD 2's 36 new symbols, the FLUF co-residency; 195 / 195 static on the upstream exe - point 1); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: 5 watched-path commits abf84ac..073a665 in `core/flexe.h` (+8 symbols, 159) and `docs/TESTING.md` (e3edc6a: the display-matrix sweep checks HUD 2); the decision stands; point 1 re-measured statically - all parseable symbols match on the upstream exe); before: abf84ac (25 Oct 2026 ~07:00, short form after the UI build: 2 watched-path commits 3cfa88c..abf84ac in `core/flexe.h` (+24 UI symbols, dd5bce6 / 28dd009); the decision stands, point 1's symbol count is stale - marked in place); before: 3cfa88c (25 Oct 2026 ~03:30: the 24 Oct refactors and the UI design docs; nothing touched)
Dirty paths at check: none at the start (25 Oct 2026 ~23:05; during the round another session's uncommitted `controller/src/impl/padbases.cpp`, `controller/src/impl/promptbar.cpp`, `docs/UI_HUD2_EXTENDED.md` appeared - not judged); none (25 Oct 2026 ~12:15); none (25 Oct 2026 ~07:00); before: none at the start (24 Oct 2026, 1defa3f); at the end `controller/src/impl/autopilot_frame.cpp`, `controller/src/impl/autopilot_objective.cpp` (another session's uncommitted edits, appeared during the check; not judged here); before: tools/ocr/__pycache__/ (untracked; 22 Oct 2026 check-up)
Watch: docs/HDE_UPSTREAM.md, docs/MOD_COMPATIBILITY.md, tools/install-hde.ps1, docs/TESTING.md, core/flexe.h, docs/kb/display.md
Evidence: the two installs' `EXE` folders compared file by file; `build\sigtool\sigtool.exe check` on both exes; the clone's git history since the 0.7.1 tag; GitHub milestones and releases of FLHDE/freelancer-hd-edition and -installer
Sources: [file] docs\HDE_UPSTREAM.md (the project's own list of what changed, 17 Oct 2026); [file] the clone and the install; [api] GitHub; `flsharp.md`

## What it means for FLvibed

*Check-up 26 Oct 2026 (step 3 short form, HEAD e5c02cc; 14 watched-path commits 855745b..e5c02cc):* **202 / 202
statically** on the upstream exe, 0.7.1's, Discovery's and vanilla 1.1's (`research\evidence\discovery\sigcheck_e5c02cc.txt`).
FLvibed's borderless window (90a8b7f, `docs\kb\display.md` "Window mode path") stays out under HDE's DxWrapper /
d3d8to9 (`auto`), so the decision below is unchanged; the Reset fix e145662 (the comm portrait's back-buffer readback
only on the system's own D3D8) applies to both HDE installs alike (the same wrapper). Nothing contradicted.

**Decision (user, 21 Oct 2026): HDE 0.7.1 stays the primary test install and the benchmark baseline; the upstream
combination becomes a second, separate install for compatibility sweeps; the primary switches when HDE publishes
its next release (v0.8), which is when players move.** The reasons:

1. **The hooks are not the issue.** `sigtool check` resolves all 114 `flexe.h` symbols on the upstream exe at their
   1.1 addresses, and all 114 again in search mode. The upstream exe's ~25 byte edits (FLStaticPatch, plus the
   large-address-aware flag) sit nowhere near FLvibed's hook sites (`HDE_UPSTREAM.md`). Nothing about the exe forces
   a move.
   *Check-up 25 Oct 2026 (abf84ac):* the count is stale - `flexe.h` has **151 symbols** now, 24 of them from the UI
   build (the Options screen NN_Preferences, the pause menu, the maneuver bar, the control factories and classes, the
   cursor-shown flag; dd5bce6, 28dd009). `sigtool check` on the upstream exe must be re-run for them before this point is
   cited again, and the new NN_Preferences hooks live in the screen FLSharp 0.9.2's resolution patches live in
   (`flsharp.md`, U4) - both are items for the second install's sweep (M7). The conclusion (no exe reason to move) is
   unchanged in kind: entry detours and factory calls at 1.1 addresses FLStaticPatch does not touch, to be confirmed.
   *Check-up 25 Oct 2026 (073a665):* **confirmed statically.** `flexe.h` has 159 symbols (HUD 2 added 8); the 151 the
   evidence script parses (`research\evidence\discovery\sigcheck.py` over a joined copy, `flexe_join.py`; output
   `sigcheck_073a665.txt`) all match their bytes on the upstream exe and on 0.7.1's - the 24 UI-build and the 8 HUD 2
   symbols included. Still open for the second install: the in-process `sigtool check` with FLSharp 0.9.2 loaded (it
   patches call sites at run time) and the 8 entries the script cannot parse (IDEAS C6). HUD 2's other neighbours are
   unchanged between the targets: Territory.dll, Zone.dll and HudShift.dll are byte-identical in both `EXE` folders (its
   place line reads Territory.ini's formats and Zone.dll's zone edges); HUDless.dll differs (same 4,608 bytes, another
   hash) - its toggle beside HUD 2's root hiding is a sweep item (`flsharp.md`, IDEAS U4).
   *Check-up 25 Oct 2026 (855745b):* **195 / 195 statically** on the upstream exe, 0.7.1's and Discovery's
   (`sigcheck_855745b.txt`; every entry parses now), and by `sigtool check --scan --shift 16 --hooked` in da1363f on
   0.7.1, the upstream exe and vanilla 1.1. The project now has a third exe: vanilla 1.1 with a decrypted exe runs FLvibed
   (0d0bcb7; `MOD_COMPATIBILITY.md`), so "1.1 addresses" is tested on an unpatched build too. HUD 2 no longer leans on
   HDE's HudStatus.dll for the own-ship view (65cd6dd: the game's own panel view routines, six new symbols), one
   neighbour less that could differ between the targets. Still open: the in-process check with FLSharp 0.9.2 loaded (C6).
2. **What the switch would cost is the test baseline, not the code.** The autopilot's numbers, runbooks and sweep
   fixtures are measured on 0.7.1: the dock planner's timings (upstream removes Common.dll's maximum docking
   initiation distance, 0.7.1 multiplies it by 5), the M06 runbook (upstream disables goto during the Hovis race),
   M08 / M11 trigger changes (fixtures loaded into changed scripts), the window-mode byte sites the display matrix
   instances patch (upstream patches the exe differently), the mouse layer (upstream drops the Windowed Mode Mouse
   Fix `dinput8.dll` and lets FLSharp 0.9.2's cursor and Acquire hooks take over), the comm caption the subtitle
   speaker names sit beside (story NPC affiliation removed). Every one of these is a re-measure, and the campaign
   benchmark would restart from a different baseline mid-way.
   *Check-up 22 Oct 2026 (309f618):* the M06 item is weaker now - the race is flown by ring passes in cruise without
   goto (3f06367, `flrace.ps1`; IDEAS A8), so upstream's goto disable no longer touches our race flight (the rest of
   the M06 leg still uses goto / travel). The dock item is stronger: the autopilot now hands docks over to the game's
   own dock maneuver (636f9ef, ee2579a, GameDocksUs), the code whose initiation distance differs between the targets.
   *Check-up 24 Oct 2026 (1defa3f):* stronger again. The 0.7.1 baseline is now the speedrun benchmark itself - story
   checkpoints, par and fail limits per checkpoint (`docs\checkpoints.tsv`, `docs\SPLITS.md`), checkpoint saves for
   segment practice, ~200 judged M12 legs - and the speedrun preset's `ring_request` (a planet ring requested at p2 at
   full speed, landing 0.03 s after the request) is a dock timing that upstream's removed maximum docking initiation
   distance could change. A mid-way switch would invalidate the splits' history; the v0.8 rule is right.
3. **What the switch would gain now is available more cheaply as a second install.** The real gains are compatibility
   facts: FLSharp 0.9.2 (`flsharp.md`), TargetScreen's dynamic target view next to the HUD work, the split resource
   DLLs, the LAA exe. All of them are answered by a sweep against a second install (the signature check, the mouse
   layer, the subtitles' caption comparison, the display matrix), without touching the baseline. CLAUDE.md already
   names both the release and the repository as compatibility targets.
4. **Players are on 0.7.1 and will move on a release, not on a commit.** HDE's cadence: 0.6 (Aug 2022), 0.7 (Jun
   2025), 0.7.1 (Jul 2025); 153 commits since, no changelog section for them, the v0.8 milestone at 8 closed / 5 open
   (the open ones: reputation softlocks, nebula and station textures, pilot voices, a licence), the repository quiet
   since 30 Aug 2026. A v0.8 within months is plausible, not scheduled. Switching the primary at that release aligns
   the benchmark baseline with what players run and costs the re-measure once.
5. **An upstream install is not a download away.** No installer exists for the repository state; the installer repo
   documents the build (clone, 7-Zip the mod files under 2.05 GB, compile `setup.iss` with Inno Setup, which is not on
   this machine). `tools\install-hde.ps1` then drives that installer unattended with the project's options. The test
   tooling knows one install root plus hard-linked mirrors (`Install\instances\<n>`), so a second root needs a
   parameter or a second checkout of the tooling's root (IDEAS M7).

## Findings (21 Oct 2026)

### What differs between the 0.7.1 install and the upstream clone
| Area | 0.7.1 install | Upstream (7c6186e) |
|---|---|---|
| `Freelancer.exe` | 2,519,552 B, installer-patched (window mode bytes) | 2,523,648 B, FLStaticPatch `FLDOCS\BinaryEdits.patch`: ~25 code edits, a data table at 0x666B8C-0x667068, LAA flag, 4 KB appended; sigtool 114/114 both ways |
| `Common.dll`, `Server.dll`, `engbase.dll`, `FLServer.exe` | 0.7.1 bytes | same sizes, patched (dock initiation distance removed, planets spin, speed test skipped, call sign fix, empty_cube_frequency, animation crash fix) |
| FLSharp | 0.8.0 (17 features) | 0.9.2 + Aug 2026 rebuild (43 features) |
| flplusplus | 359,936 B | 377,856 B, new ini keys |
| console | 89,088 B | 103,424 B (more commands) |
| Plugins added | - | TargetScreen.dll, VersionText.dll, LightingBugFix.dll (off), cyrillic_chat.dll, ReShade32.dll, d3d8_dxwrapper.dll, `*_tsr.dll` split resource DLLs, hderesources.dll |
| Plugins removed | `dinput8.dll` (Windowed Mode Mouse Fix) present | removed Aug 2026 (FLSharp cursor_fix / mouse_warp_fix instead) |
| `dacom.ini` | installer adds libcurl, gls_workaround, HudFacility / HudWeaponGroups / HudTarget / HudStatus (an installer option), FLvibed.dll (ours) | TargetScreen.dll, VersionText.dll; the Hud*.dll files still ship for the installer option |
| DATA | tag 0.7.1 | 110 files, +210 / -71 lines: UNIVERSE 37, SOLAR 35, SHIPS 11, SCRIPTS 7, MISSIONS 7 (M06, M08, M10, M11, M12, M13 as in `HDE_UPSTREAM.md`), INTERFACE 6, EQUIPMENT 4 |
| ReShade / dxwrapper | 0.7.1 versions | updated |
| Commits | - | 153 (Jul 2025-Aug 2026: 29 in Aug 2025, 25 in Apr 2026, 19 in Jul 2026, 18 in Aug 2026) |

### Release outlook
- Releases: 0.4.1, 0.5 (Jan 2022), 0.6 (Aug 2022), 0.7 (14 Jun 2025), 0.7.1 (15 Jul 2025); installer v0.7.1-1 on
  25 Aug 2025. Milestones: v0.8 open (8 closed, 5 open), v1.0 open (6 closed, 12 open), Backlog (26 closed, 62 open).
  97 open issues and PRs. Last push 30 Aug 2026 (repository), 22 Aug 2026 (installer: an M13 OpenSP warning, "general
  fixes"). No announced date.

### Plan for the second install (adopted 21 Oct 2026; for the working session, in order)
1. **Build the upstream installer** (one-off, repeatable): install Inno Setup 6 (not on this machine; a download,
   the user's OK), `git pull` the clone only when the user asks, 7-Zip `DATA`, `EXE` and the other mod files from the
   clone into `freelancer-hd-edition.7z` under 2.05 GB (LZMA2), compile `setup.iss` from a clone of
   FLHDE/freelancer-hd-edition-installer. Record the clone commit in the installer's name.
2. **Install it** with `tools\install-hde.ps1` (the same unattended options as the primary) into a new root, e.g.
   `Install\Freelancer HD Edition (upstream)\`; the tool's side-effect clean-up applies unchanged.
3. **Teach the tooling a second root**: one parameter (or environment variable) read by `deploy.ps1`, `flctl.ps1`,
   `flsweep.ps1` and the instance mirrors, defaulting to the primary; `fldata.ps1` paths follow it.
4. **Its own data**: `tools\kbgen.ps1` tables fingerprinted for that install (kept apart from the primary's);
   fixtures copied only where the mission scripts are unchanged (not M08, M11; M06 only outside the race).
5. **The sweeps that matter**, as sections that name the root: signatures (`release-build` section), the mouse layer
   without `dinput8.dll` (pad cursor, fire-select, handback), the subtitle caption comparison (story NPC affiliation
   removed; FLSharp pilot-name fix), the display matrix (window-mode bytes under FLStaticPatch: `kb\display.md`),
   docking from far away (no distance limit), the M06 race without goto, TargetScreen beside the HUD work.
6. **Write back**: `docs\HDE_UPSTREAM.md` gets the FLSharp 0.8 -> 0.9.2 entry and each sweep's result;
   `MOD_COMPATIBILITY.md` the plugin differences; IDEAS C6, U4, U5, S6, M1 close from these sweeps.
7. **At HDE v0.8**: re-run `install-hde.ps1` with the release installer into the primary root, regenerate the kb
   tables, re-measure the runbooks and the campaign benchmark once, retire the upstream root or point it at the next
   clone state.

### What a second install needs
- Build the installer from the clone (7-Zip is on this machine, Inno Setup is not) or overlay the clone's DATA and
  EXE over a vanilla install by hand (the wiki's "simple installation"); `tools\install-hde.ps1` already knows the
  unattended options.
- A second install root in the tooling (deploy, flctl, flsweep assume `Install\Freelancer HD Edition` plus mirrors),
  or a second checkout of FLvibed pointing at it.
- Regenerate the knowledge base tables for that install (`tools\kbgen.ps1`, fingerprinted), and keep its fixtures
  apart (M08 / M11 scripts changed).
- Then the sweeps that matter: the signature section, the mouse layer (pad cursor, fire-select, handback) without
  the dinput8 fix, the subtitle caption comparison (story NPC affiliation), the display matrix (the exe's window-mode
  bytes under FLStaticPatch), docking from far away (no distance limit), the M06 race without goto.
