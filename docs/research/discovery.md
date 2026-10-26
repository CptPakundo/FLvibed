# Discovery Freelancer 5.3: a patched 1.1 client under FLUF and a closed hook
Status: current
Checked against: FLvibed 77d8655 (26 Oct 2026, coordinator short form before the export: the watched commits since - d89f88c / 79e2bcc smart start + K&M goal panel, 7632ced job objective text, 77d8655 objective-named loot shot, 9ff7fe7 handoff - are autopilot / UI internals; no statement of this note changes; not re-read in depth); before: FLvibed 01fcfa9 (26 Oct 2026, the step 3 short form re-confirmed: the only watched commits since, 9327f86 / 01fcfa9, are doc and handoff edits recording what this note already says); before: FLvibed e5c02cc (26 Oct 2026, step 3 short form: 202 / 202 on Discovery's exe, the FLUF layer's unknown-build rule - first "What it means" line); before: FLvibed 855745b (25 Oct 2026 ~23:05, whole-set check-up: 16 watched-path commits 073a665..855745b - Extended HUD 2's host hooks and 36 new `flexe.h` symbols (195), the vanilla 1.1 support 4042c7d..0d0bcb7 with the 13 padding signatures re-cut (68755f1), the FLUF co-residency da1363f; the signature regression re-measured on Discovery's exe: 195 / 195 - second bullet); before: 073a665 (25 Oct 2026 ~12:15, short form after the HUD 2 build: 5 watched-path commits abf84ac..073a665 in `core/flexe.h` (+8 symbols, 159), `controller/src/host` (the HUD 2 hotspot box, ToolTip / NeuroNet window handling) and the HUD bus 2ae3bbd; the signature regression re-measured - second bullet); before: abf84ac (25 Oct 2026 ~07:00, short form after the UI build: 6 watched-path commits 3cfa88c..abf84ac in `core/flexe.h` (+24 symbols), `controller/src/host` (the UI hooks) and `core/game/menu.*` (New Game / load-screen reads); nothing contradicted - the hooks keep the chainable form, second bullet); before: 3cfa88c (25 Oct 2026 ~03:30: the 24 Oct refactors and the UI design docs; nothing touched)
Dirty paths at check: none (25 Oct 2026 ~23:05); none (25 Oct 2026 ~12:15); none (25 Oct 2026 ~07:00); before: none at the start (24 Oct 2026, 1defa3f); at the end `controller/src/impl/autopilot_frame.cpp`, `controller/src/impl/autopilot_objective.cpp` (another session's uncommitted edits, appeared during the check; not judged here)
Watch: core/flexe.h, core/hook.*, build.cmd, docs/MOD_COMPATIBILITY.md, controller/src/host, core/game/menu.*
Evidence: research\evidence\discovery\ (two Freelancer.exe builds for signature regressions, sigcheck.py + sigcheck.txt, patchlist.xml, notes.yml, module strings); the five patch archives (318 MB) and the cloned repos stayed in the session scratchpad (IDEAS X3)
Sources: [file] official patch server archives, MD5-verified; [file] DiscoveryGC repositories; [page] the-starport FLUF docs, flserver.de; discoverygc.com and ModDB were unreadable (Cloudflare / 403)

## What it means for FLvibed

*Check-up 26 Oct 2026 (step 3 short form, HEAD e5c02cc; watched-path commits 855745b..e5c02cc):* **202 of 202
signatures match on Discovery's 22 Aug 2026 hotfix exe** (7 new: the NeuroNet window manager / close / transition reads of
the dealer close f52815b, and `SetupScreen`, `WindowStyle`, `WindowedStyle`, `DisplaySettings` of the borderless window
90a8b7f; `research\evidence\discovery\sigcheck_e5c02cc.txt`, also HDE 0.7.1, upstream HDE and vanilla 1.1). The FLUF
co-residency is now the FLUF-awareness layer, verified in game on FLUF nightly-2026-10-16 in both orders (`fluf.md`);
Discovery ships an older FLUF build, which the layer treats as **co-residency only** (an unknown build = no keep-outs, no
input rules, one log line) - fail closed, untested on Discovery itself.
- **Signature regression, not a test target.** 115 of 123 `flexe.h` symbols resolve on Discovery's exe; the 8
  misses are signatures that include inter-function padding, which Discovery overwrites (IDEAS C1).
  *[855745b: 195 of 195 match - the failing ones were re-cut (68755f1); see the next bullet.]* No story, saves
  wiped by the launcher, closed anticheat hook: use its exes for the regression only (IDEAS M5).
- **Two hook facts worth acting on:** the 2012 client hook takes `HandleKey` 0x576410 with a jump form FLvibed cannot
  chain (IDEAS C2); FLUF, the Starport framework Discovery runs on and FLHook's declared successor, detours five of
  FLvibed's symbols with plain E9 jumps and swaps vtable pointers, both chainable (IDEAS M3).
  *Correction 25 Oct 2026 (35b71fd, `fluf.md`):* chainable only when FLvibed hooks first - FLSDK's `FunctionDetour`
  removes and re-writes its jump on every call, which erases a jump chained after it (IDEAS C7); FLUF also detours
  `HandleKey` and `FrameUpdate`.
  *Check-up 25 Oct 2026 (abf84ac):* the UI build's hooks keep the chainable form - entry detours (`hk::Detour`; the
  `ManeuverLayout` one chained after the subtitles host's own) plus the game's own control factories, no byte patches -
  so co-residency with FLUF / DSAce is unchanged in kind. The HUD colour bytes U1 names as data Discovery rewrites are now
  read live (`UiColourSelected` 0x679B94, `UiColourNormal` 0x679BB0 in `flexe.h`, dd5bce6), the right way round. The
  signature regression (C1) now has 151 symbols; the 24 new ones are unmeasured on Discovery's exe.
  *Check-up 25 Oct 2026 (073a665), measured:* `flexe.h` has 159 symbols (HUD 2 +8). `sigcheck.py` no longer parses
  multi-line entries, so it ran over a joined copy (`research\evidence\discovery\flexe_join.py`; 151 of 159 parsed;
  output `sigcheck_073a665.txt`): on the 22 Aug 2026 hotfix exe **139 match, 12 miss, all 12 only by inter-function
  padding in the window** - the 8 known ones plus four from the UI build (PrefsShowPage, TextCtlSetProperty, CursorShown,
  ToggleButtonActivate). HUD 2's 8 all match, though three again include padding (TargetViewDraw, ObjectUnknown,
  WeaponGroupCurrent): C1's rule is the one fix that would clear the whole list. HUD 2's hooks keep the chainable form
  (no new detour: per-root state-byte writes, the game's own icon-button factory for the hotspot box, a private vtable
  copy on FLvibed's own control only), so FLUF / DSAce co-residency is unchanged in kind. HUD 2 does not use the HUD
  visibility bytes U1 names (it hides root controls), and it reads the chat input `CTID` that U3 names (the chat window
  stays while the input is open).
  *Check-up 25 Oct 2026 (855745b), measured again:* **195 of 195 match on Discovery's 22 Aug 2026 hotfix exe** (and on
  HDE 0.7.1's and the upstream exe; `research\evidence\discovery\sigcheck_855745b.txt`; `flexe_join.py` now merges a
  signature split over several string literals, so every entry parses). The 12 misses are gone: 68755f1 re-cut the 13
  signatures that failed on the decrypted vanilla 1.1 exe for the same reason (the cracked vanilla exe and Discovery's
  both carry leftover call bytes in the padding), and `FREELANCER_RE.md` "Signatures" now states the rule "never span
  the alignment padding between functions" (C1). The rule is written, not yet applied to the whole table: 22
  signatures still span padding and match only because the builds tested agree there (`padspan_855745b.txt`: e.g.
  TargetViewDraw, ObjectUnknown, WeaponGroupCurrent, UiControlAdd, WindowBaseDraw, CurrentCamera). The hook forms:
  da1363f teaches `hk::Detour` to follow a jump into a restore-style hook's handler and move whole instructions (a
  small x86-32 length decoder), but it still chains only an `E9` at the entry - Discovery's `mov ecx, imm32; jmp ecx` on
  `HandleKey` stays unchained (C2). New since 073a665: vanilla 1.1 runs FLvibed (0d0bcb7, a decrypted exe; the CD's exe is
  SafeDisc-encrypted: 0 / 187 statically, 4042c7d) - a Discovery-like "patched 1.1 exe" target is now the third exe the
  table is checked on; still no FileVersion line or "unsupported exe" message at start-up (C3).
- **Data, never assumptions:** key-command table, HUD colour and visibility bytes, save path, docking types, cargo
  volume rules, max dock range, render wrapper (d3d8to9) (IDEAS U1, C4, C5, M4). A catalogue of ~40 client patch
  sites from the public DSAce half and TargetScreen can be recorded as unverified facts (IDEAS K6).
- **Ideas to borrow later:** FLUF's module lifecycle, key-slot reuse for new commands, the QoL-patcher constant
  catalogue, a patch-notes feed and MD5 manifest (IDEAS U6, U8, P2, P3).

## Findings (agent report of 21 Oct 2026)

### 1. Summary
1. Discovery is a multiplayer-only total conversion (campaign removed since 4.85 [page]); current line **5.3 "Singularity"** (22 Aug 2025) with cumulative patches up to **5.3.5 hotfix 5 (22 Aug 2026)** [file: patch list + notes.yml]; one official server, ~29 players monthly average, rank 1 of all Freelancer servers [page: flserver.de].
2. Its client is **vanilla v1.1 Freelancer.exe with in-place byte patches** (no new sections, no LAA, `.rdata` renamed), plus **FLUF** (The Starport's framework, ImGui UI, d3d8to9 wrapper from ReShade) and **adoxa plugins + the closed client hook `dsace.dll`** in `EXE\dacom.ini [Libraries]` [file].
3. FLvibed's signature table resolves **115 of 123 symbols** on Discovery's Freelancer.exe; the 8 failures are signatures that include NOP padding, which Discovery overwrites with `call` stubs (514 sites to 0x401379, a second family to 0x402309) [file: `extracted\sigcheck.txt`]. Fix: exclude inter-function padding from signatures.
4. Real hook collisions: FLUF detours **UiRender 0x59E300, CursorUpdate 0x41F150, WinKey 0x577850, PlayArch 0x428680, MainMenuButtonClick 0x5664A0, control creation 0x59DB20** (plain `E9` jumps, chainable) and the legacy DSAce hooked **HandleKey 0x576410** with `mov ecx,imm32; jmp ecx` (7 bytes, not a JMP rel32 FLvibed can chain) [file]. Everything else FLvibed touches is untouched or only read by Discovery.
5. Single player under Discovery: `[Initial SP DLLs]` still loads `StoryLauncher`, voice/mission ini lists are vanilla, but the launcher deletes `Accts\SinglePlayer\*.fl` on every launch and the data has no story missions: FLvibed's campaign-driven subtitles/autopilot have nothing to drive there; only generic parts (comms, HUD, controller input, free flight) apply.
6. Open-source map: `DiscoveryGC/FLHook` (server, DBAD licence; contains the **public half of the 2012 DSAce client hook** with ~40 Freelancer.exe/Common.dll patch addresses), `TargetScreen`, `SkyboxParallax`, `EquipMass`/`shipclass` forks, `DSLauncher` (GPL-3, archived), `DiscoveryApi` (GPL-3). None are in `FLvibed\research\repos`.
7. Borrow: FLUF's module/lifecycle design and widescreen code, the QoL-patcher's catalogue of hard-coded constants, DSAce's key-command renaming trick, the patch-notes feed, the launcher's MD5 patch manifest.
8. Not verifiable: `dsace.dll` itself, the full 5.3 installer (discoverygc.com behind a Cloudflare human check; ModDB 403), forum/wiki text, current anti-cheat behaviour.

### 2. What Discovery is and its history
**What.** The long-running multiplayer role-play mod (Discovery Gaming Community, discoverygc.com): new systems, factions, ships (capital ships, battlecruisers), economy, player-owned bases, cloaks, jump drives, driven by an FLHook-based server and a client hook [file: FLHook plugin folders `base_plugin`, `cloak_plugin`, `mobiledocking_plugin`, `dronebays`, `minecontrol_plugin`, `pvecontroller`, `npc_control`, `dynamic_missions`; page: the Discovery wiki says the single-player campaign is removed and replaced by an online player-driven narrative - https://discoverygc.com/wiki/Discovery_Mod, via search snippet].

**Versions and dates** (older ones [page: ModDB via search], 2015+ [file: `patchlist.xml`, 230 entries]):

| Version | Date | Source |
|---|---|---|
| 4.84 | Dec 2007 (Igiss) - last version with the campaign | [page] |
| 4.85 Reunion | Jun 2009 - campaign disabled from here | [page] |
| 4.86 Exiles | Apr 2012 | [page] |
| 4.87 Conquest | Sep 2013 | [page] |
| 4.88 | Feb 2015; 4.88.1 patches Jul 2015-2016 | [page]+[file] |
| 4.89.1 | 14 Apr 2017 ("Initial"); 4.89 Final Nov 2017 | [file]+[page] |
| 4.90 | 21 Jan 2018 | [file] |
| 4.91.0 | 23 Nov 2018 | [file] |
| 4.92.0 | 21 Dec 2019 | [file] |
| 4.93 Horizons | Patch 1 on 2 May 2020 | [file] |
| 4.94 | Patch 1 on 8 Sep 2020 | [file] |
| 4.95 | Hotfix 1 on 23 Dec 2020 ... Patch 9 hotfix 2 on 15 Feb 2023 | [file] |
| 5.0 Fire and Fortune | 6 Oct 2023 (patch); installer on ModDB 20 Nov 2023, 590 MB | [file]/[page] |
| 5.1 | 1 Jun 2024 | [file] |
| 5.2 | 5 Oct 2024; patch 5 on 1 Mar 2025 | [file] |
| **5.3 Singularity** | **22 Aug 2025** (216 MB patch) | [file] patch list + notes.yml |
| 5.3.1 POB update | 20 Dec 2025 | [file] |
| 5.3.2 | 15 Apr 2026 | [file] |
| 5.3.3 | 21 May 2026 | [file] |
| 5.3.4 | 27 Jun 2026 | [file] |
| 5.3.5 | 14 Jul 2026; hotfix 5 on 22 Aug 2026 = newest | [file] |

The patch list's `<Settings>` still says `gameversion 4.88.1`, `mainserver -s88.198.43.188:2302` [file]; the launcher passes that `-s` to Freelancer.exe [file: DSLauncher `Forms\Primary.cs`].

**Team.** "Discovery Dev Team" [page]. In evidence: Igiss (founder, 4.84/4.85), Cannon (DSAce author 2010-2012) [file: dsace_readme.txt]; Laz/Lazrius (DSLauncher V2, 2018) [file]; dsyalex, zibbizor (FLHook repo 2017) [page: issue #13]; jammi (patch notes 2023-2026) [page]; @Aingar and @Krmloo thanked in 5.3.3 notes for engine/ini work [file: notes.yml]. Aingar and lnvoker keep FLHook forks [page].

**Servers and players.** "Discovery Freelancer RP 24/7" 88.198.43.188:2302; flserver.de 21 Oct 2026: rank 1, **29.32 players monthly average**, next server 10.05 [page: https://flserver.de/topservers.php?timeframe=month]; snapshots via search: 46/255 (Sep 2026), 30.5 avg (Jun 2026); the fan wiki's "100-196" is historical [page]. Two other "Discovery" servers at 0 average [page].

**Licensing.** `DiscoveryGC/FLHook`: "DON'T BE A DICK PUBLIC LICENSE" [file]; `DiscoveryApi` GPL-3.0 [page]; `DSLauncher` GPL-3.0 [file]; `TargetScreen`, `SkyboxParallax`, `EquipMass`, `shipclass`: no licence file, the latter two carry adoxa's headers [file]. Mod data/binaries: no licence; DSAce and the server anti-cheat withheld ("won't be public", 2017) [page: https://github.com/DiscoveryGC/FLHook/issues/13]. The patch server serves every patch openly with MD5s [file].

**Status (2026).** Active: patches roughly monthly through Aug 2026, in-game patch-notes feed updated 22 Aug 2026 [file]; FLUF adopted in 5.3 and updated each patch [file]. TheStarport's FLHook archived 16 Jul 2026 ("development has ceased"; FLUF "intended to be a replacement to the unfinished FLHook 5") [page: https://github.com/TheStarport/FLHook]. Discovery's own FLHook repo last commit 5 Dec 2023 [file]; its current server code is not public [assumed].

### 3. Technical findings with evidence

#### 3.1 Installer, launcher, patch server
- **Installer**: players use "discovery_5.3.2.exe" [page: Lutris]; 5.0 installer 590.15 MB on ModDB [page]; 5.3 size not readable (ModDB 403, forum Cloudflare). Install guide: base game first, separate folder, **"HD Edition is not compatible with Discovery, so it will need to be installed separately"** (FAQ updated Jan 2026) [page: forum tid=200225 snippet]. Antivirus false positives on the anti-cheat are expected; the launcher has a "DSAce toggle" that restores `DSAce.dll` [page: snippets].
- **Launcher** (`DSLauncherV2.exe`, C#/.NET 4.0, MetroFramework; source archived May 2024) [file: `repos\DSLauncher`]: reads `launcherconfig.xml`, downloads `patchlist.xml` from `http://patch.discoverygc.net/` (fallback "Kitty" mirror), downloads each `<patch url md5hash>` zip in order, MD5-verifies, extracts over the install (SharpCompress), records hashes; refuses to launch without `EXE\DSAce.dll` (error L03); **deletes every `*.fl` in `Documents\My Games\Freelancer\Accts\SinglePlayer\` if `Restart.fl` exists** before each launch; launches `EXE\Freelancer.exe` with `-s<ip:port>` plus `-windowed`, `-dx` or `-d<W>-x<H>`, `-logchat -logappend -logtime -localtime`, `-noflighttext`, `-dptplayer`, `-newplayer`, `-lag`, `-numdmg`, `-nochat` (consumed by DSAce, 3.5); accounts in `My Games\<ModName>\launcheraccounts.xml` [file: `Forms\Primary.cs` 1655-1770, `Settings\LauncherSettings.cs`, `Self Patch\Patch.cs`].
- **Patch server**: `http://patch.discoverygc.net/patchlist.xml` (230 patches 2015-2026), same on patch.discoverygc.com; no installer there [file]. Downloaded + MD5-verified: `20250822-patch.zip` (5.3 base, 216,348,678 B), `20260415-patch.zip` (5.3.2, 59.5 MB), `20260627-patch.zip` (5.3.4, 30.9 MB), `20260714-patch.zip` (5.3.5, 11.6 MB), `20260822-hotfix.zip` (13.8 MB) [file]. Patches are delta sets, so absence of a file in them is not evidence.
- **In-game patch notes**: `EXE\modules\config\patch_notes.yml` -> `https://discoverygc.com/files/notes.yml` (125 KB YAML, readable without the Cloudflare check) [file].

#### 3.2 EXE folder in the 5.3 base patch [file: `extracted\base53\EXE`]
`Common.dll` (1,774,080 B), `Discovery.dll` (13.0 MB), `DsyAddition.dll` (1.86 MB), `EquipMass.dll`, `EquipResources.dll`, `FLServer.exe`, `FLUF.dll` + `FLUF.yml`, `Freelancer.exe` (3,005,956 B), `InfoCards.dll`, `MiscText.dll`, `MiscTextInfo2.dll`, `NameResources.dll`, `OfferBribeResources.dll`, `SkylineParallax.dll`, `TargetScreen.dll`, `d3d8.dll`, `dacom.ini`, `directplay8.dll`, `freelancer.ini`, `modules\{FLUF.CrashWalker.dll, FLUF.UI.dll, freetype.dll, libcurl.dll, patch_notes.dll, qol_patcher.dll, ship_dealer_enhancements.dll, config\FLUF.UI.yml, config\patch_notes.yml}`, `mpnewcharacter.fl`, `neutralcharacter.fl`, `resources.dll`. Later: `FreelancerSDK.dll` (5.3.2), modules `expanded_hardpoints.dll`, `screen_control.dll` (5.3.2), `full_thruster.dll` (5.3.4); 5.3.4 splits `Discovery.dll` into `DiscoveryCommodities/Equipment/Lore/Misc/News/RuntimeWorld/Ships/Statblocks/UI/Universe/Weapons.dll`; the 22 Aug 2026 hotfix ships `DLLS\BIN\content.dll` (1,311,232 B, vanilla timestamp 0x3ECBB142 = byte-patched vanilla Content.dll) [file]. `Discovery.dll`/`DsyAddition.dll` are **pure resource DLLs** (one `.rsrc` section, VC6, 2003 timestamp; `[Resources]` "Discovery infocards") - not the client hook [file: peinfo].

#### 3.3 dacom.ini [Libraries] [file]
Vanilla list to `flmaterials.dll`, then: `hudshift.dll, directplay8.dll, multiCruise.dll, dsace.dll, ShieldOffline.dll, mp3codec.dll, mprep.dll, moors.dll, chklootvol.dll, console.dll, shipfilter.dll, shipclass.dll, healing.dll, engclass.dll, Zone.dll, MultiUniverse.dll, Territory.dll, numeric.dll, NoNavMap.dll, WheelScroll.dll, MuzzleCone.dll, FixedMounts.dll, EquipMass.dll, storyfactions.dll, scroller.dll, TargetScreen.dll, SkylineParallax.dll`. **FLUF.dll is not in dacom.ini**; `directplay8.dll` is a custom build ("new directplay8 build to reduce crashes", 5.3.1 hotfix 1 [file: notes.yml]) and presumably bootstraps FLUF ("Changed: FLUF's loading mechanism", 5.3.4 hotfix 4) [assumed; the FLUF repo has `FLUFBoot` and `DacomFix` loaders]. **No jflp.dll**: widescreen is FLUF's (`WidescreenSupport.cpp` rewrites 0x5C9020 and HUD constants; `ScaleFontsForWidescreen`, `ScaleCursor`) [file: FLUF repo on hand].
Authors: adoxa (verified on http://adoxa.altervista.org/freelancer/plugins.html [page]): Check Loot Volume, Console, EngClass, Equip Mass, Fixed Mounts, Healing, HudShift, Moors, MP Rep, MP3 Codec, NoNavMap, Numeric Damage, Scroller, Shield Offline, Ship Class, Ship Filter, Story Factions, Territory, Wheel Scroll, Zone. Not his: `multiCruise.dll`, `MultiUniverse.dll`, `MuzzleCone.dll` (author unknown), `dsace.dll` (Cannon/Discovery), `TargetScreen.dll`/`SkylineParallax.dll` (DiscoveryGC). Overlap with HDE (`docs\MOD_COMPATIBILITY.md`): hudshift, console, mp3codec, mprep, numeric, storyfactions, Territory, WheelScroll, Zone, moors, engclass, EquipMass, healing - families FLvibed already copes with.

#### 3.4 Freelancer.exe: a patched vanilla 1.1 [file: `scripts\sigcheck.py`, `scripts\peinfo.py`]
- 3,005,956 bytes, VC6 linker 6.24, timestamp 0x00534D69 (zeroed/fake), entry 0x5B808E, base 0x400000, **not LAA**, sections `.text` (raw = rva, file offset = VA - 0x400000), **`iMS\0ta`** (renamed `.rdata`, va 0x5C6000), `.data`, `.rsrc`, and a 785,924-byte **overlay** starting with an `NB10` CodeView record (`\\aus-usp-02\builds\freelancer\symbols\020403_release_1336_Iretail_ver1223\Freelancer.pdb`) + import strings, entropy 7.77 (probably the retail exe's own debug data) [assumed]. Imports vanilla (Common, DALib, Movie, DACOM, winmm, advapi32, user32, kernel32, gdi32, ole32, ws2_32, msvcp60/msvcrt, dinput8, ImeUI) - **no direct import of FLUF, dsace or d3d9** [file].
- **Signature check** (`extracted\sigcheck.txt`): 115/123 OK on the 5.3 base and the 22 Aug 2026 exe. The 8 mismatches - `UiRender`, `DmgIndContact`, `ClearWaypoints`, `LetterboxAmount`, `CursorName`, `MouseFlightTimer`, `PlayerThrottle`, `RoomInfoOwner` - fail only in bytes that are **inter-function NOP padding** in vanilla: Discovery writes `E8 rel32` calls there. 514 such calls target 0x401379 (itself padding before 0x401380, holding a self-call `E8 FB FF FF FF`), a second family targets 0x402309 [file]. Unreachable in normal flow (after `ret`); purpose undocumented - likely anchors for the closed client hook or the anti-cheat integrity check [assumed]. Function bodies and every Data operand FLvibed uses are intact [file].
- **Real edits** (5.3 base vs 22 Aug 2026 exe differ in 33 bytes): a 20-byte logic patch at **0x52C8A1-0x52C8B4** (region FLUF's `WeaponAnimationFixes.cpp` names `GET_ROOT_INDEX_CALL_ADDR 0x52C8AF`), float **0x5D6D38** 0.5 -> 0.2 (unknown), PE checksum, version strings [file]. Constants FLvibed cares about are vanilla on disk: `CinematicAspect` 0x5C9020 = 1.85 (FLUF rewrites at run time), `HullCriticalFrac` 0x5DFB40 = 0.2, far plane 0x610534 = 250000 (DSAce doubles at run time) [file].
- Consequence: tools keyed on section names or padding bytes break on Discovery; FLvibed's `sigtool` signatures that start before a function entry (`off > 0`) are exactly the ones that broke.

#### 3.5 The client hook `dsace.dll` and its public half [file: `repos\FLHook\LegacyContent\dsace_client_hook\`]
`DSAce.dll` (Cannon, 2010-2012): "limited client side anticheat, infocard and economy updates under server control, turret zoom, cloaking and jumpdrive support"; receives server commands embedded in hidden chat messages or via the `dsacesrv` FLHook plugin; required by POB (`base.dll`), mobile docking, cloak and jump plugins [file: dsace_readme.txt]. Only `public_funcs.cpp`/`equip_funcs.cpp` are public (2012); the 5.3 build is "clienthook" in the notes ("clienthook update: properly reload zones for serverside updates" 5.3.4; "Clienthook crashfix" 5.3.3) [file: notes.yml] and **is never shipped through the patch server** (first entry: "v2 without dsace.dll to prevent failure") [file]. The 2012 code patches (raw `WriteProcMem`, no chaining):

| Site | What | FLvibed relevance |
|---|---|---|
| **0x576410 `HandleKeyCmd`**: `B9 imm32 FF E1` (7 bytes) -> `HkCb_HandleKeyCmd`; unhandled commands re-execute `sub esp,80h; push ebx; push 0x576417; ret` | adds `/jump* /lights* /cloak* /shields* /selfdestruct* /marktarget* /unmarktarget* /jettisonall /showinfo` keys, HUD toggle, HUD colour toggle, Ctrl+wheel turret zoom; detects open chat via `GetObjByStr("CTID")` byte +108 & 3 | **same address as FLvibed's `HandleKey` detour**; not E9, so `hk::Detour` cannot chain; signature fails closed -> controller key hook refused under Discovery (if the current DSAce still does this) [file + assumed] |
| 0x614FC4..0x614FFC key-name table | renames unused `USER_MANEUVER_CRUISE/TRADE_LANE/DRASTIC_EVADE/STRAFE/TRAIL_*/CORKSCREW/SLIDE_EVADE/ACTIVATE_MANEUVER` to `USER_ACTIVATE_JUMPDRIVE, USER_HUD, USER_TOGGLE_HUD_COLOR, USER_LIGHTS, USER_PLAYER_INFO, USER_SELF_DESTRUCT, USER_SHIELDS, USER_GROUP_MARK_TARGET, USER_GROUP_UNMARK_TARGET, USER_JETTISON_ALL` (ids 0x7B-0x89) | the table FLvibed reads as `KeyCmdNames` (0x614DD8); Discovery's `KeyMap.ini` defines these plus `USER_CLOAK`, `USER_CLOAKDISRUPTOR`, `USER_JDDISRUPTOR`, `USER_THROTTLE_70/60`, `FLUF_OPEN_EXTENDED_OPTIONS_MENU` [file: KeyMap.ini] - the controller must treat the command table as data |
| 0x4347E0 `GetIDSString`, 0x57DA40 `GetInfocard` (entry overwritten) | string/infocard overrides from `infocards.cfg` and the server; `FormatXML` 0x57DB50 | FLvibed `text.*` gets overridden text if it calls the game's getter; FLUF detours the same two today |
| 0x4C7940 (+0x4C7F14): adoxa's roll in mouse flight | mouse-flight input | adjacent to FLvibed `MouseSteer` 0x4C7CB0 |
| 0x54AEA5, 0x54A0D3, exe+0x14A65B/0x11D89C/0x11D8BB/0x1C5AB0/0x1C5AD0, exe+0xC7903/0xDBB12.. | turret camera independent of ship; keyboard moves ship in turret view | FLvibed only reads camera objects |
| 0x5B264C: `mov edx,1; nop` when `-w` | **game runs without focus in windowed mode** | same goal as FLvibed's background-run work; now FLUF's `screen_control.dll` |
| 0x46AAF8/0x46AF1E, 0x4DCA20 (`ret`), 0x5D9368, 0x5A6250 (`ret 10h`), 0x4691D1/0x46A440 (127 chat lines/chars) | player join/leave messages, flight-mode text, lag indicator, chat suppression/limits | FLvibed `ChatFlush` 0x46A150 unaffected |
| 0x610534 far plane x2, Common+0x13DFDC asteroid range x2, exe+0x11DB5C hull shake 0.005, 0x5D7964 contact-list refresh 1 s, Common+0x7637F formation catch-up 1.02 | render/HUD constants | contact-list refresh changes the cadence of distances FLvibed reads |
| 0x679C0C/10/20/40 HUD visibility bytes; 0x679BA8/B84/BAC/B88 hostile/neutral/friendly/group colours | HUD toggle/colours | HUD cues must read these, not assume vanilla |
| exe+0xDE314/0xDE366 (bots/bats count), Common+0x63645/0x63650 (F3 docking for FREIGHTER), Common+0xE698E (planet spin), exe+0x838AF (volume counted when buying external equipment), Common+0x139B74 (cloak on CM slot), Common+0x960C, Common+0x53048/0x5330E/0xA9BA3/0xAA904 (external equipment in cargo size) | gameplay rules | **docking-type and cargo-volume rules differ under Discovery** for the autopilot's dock test and loadout optimizer |
| `SetDir()` via `IBehaviorManager::update_current_behavior_direction` from mouse 0x616840/44 vs screen 0x610850/54, flag 0x673554 | turret-mode steering experiments (commented out) | confirms FLvibed's `MouseX/Y`, `ScreenW/H`, `MouseFlightEngaged` and a behaviour-manager steering path the autopilot could use |

#### 3.6 FLHook and plugins (server) [file: `repos\FLHook`]
"FLHook Discovery 3.1.0", VS2017/VC141, starport SVN rev #267 lineage; ~40 public plugins (`alley, autobuy, balancemagic, base_plugin, bountyscan, cloak_plugin, commoditylimit, conn_plugin, dronebays, dynamic_missions, event, getrep, help_expanded, hookext_plugin, kill_tracker, minecontrol_plugin, mobiledocking_plugin, munitioncntl, npc_control, playercntl_plugin, pvecontroller, tempban, JSONBuddy, MarketFucker...`) plus configs for private ones (`dsacesrv.cfg`, `techcompat.cfg`, `shipcompat.cfg`, `jump.cfg`, `cloak.cfg`). Hooks are the classic `HkIServerImpl::*` vtable hooks of Server.dll (`Hook.h`: `ADDR_SRV_*`, `ADDR_COMMON_VFTABLE_*` equipment vtables, `ADDR_CRCANTICHEAT 0x6FAF0` in Server.dll, `ADDR_DISABLENPCSPAWNS*` in Content.dll) - same as TheStarport's FLHookSDK on hand; nothing client-side except `LegacyContent` (DSAce public half, `acctpath_patch` = Common.dll+0xA2FA0 `GetUserDataPath` jump, `condata`, `old_event_plugin`). `plugin_documentation.txt` lists every hookable server call (`ActivateCruise, GoTradelane, StopTradelane, SetManeuver, SetTarget, RequestBestPath, JumpInComplete, LaunchComplete, SPObjUpdate, SPMunitionCollision, Hail, SubmitChat...`) [file]. Server anti-cheat withheld; launcher-level checks are MD5 of patch archives only [file].

#### 3.7 FLUF as Discovery ships it [file: extracted FLUF.dll/modules; FLUF + FLSDK repos on hand]
- `FLUF.yml`: `modules: [FLUF.UI.dll, FLUF.CrashWalker.dll, ship_dealer_enhancements.dll, qol_patcher.dll, patch_notes.dll]`, `setSaveDirectoryRelativeToExecutable: false`, `writeSpewToLogSinks: true`; `FLUF.UI.yml`: `uiMode: ImGui`, `dpi: 1`, font Saira Condensed (`DATA\FONTS`) [file]. FLUF.dll (5.3.4 build, VS2022 14.43) imports `common.dll` (INI_Reader, `PhySys::FindRayCollisions`), **`FreelancerSDK.dll`** (FLSDK as a DLL since 5.3.2), exports 231; `FLUF.UI.dll` exports 1,736 (ImGui, `HudManager::RegisterHud`, `AbstractVanillaInterface`, `mainFreelancerWindow`), imports `GetUserDataPath`, `RenderDisplayList` from Common.dll, FreeType [file].
- Discovery modules (version resources) [file]: **qol_patcher** "Change various hardcoded settings via a nice friendly UI"; **screen_control** "Enables the game continuing to run in the background, as well as toggling between fullscreen and windowed modes"; **expanded_hardpoints** "unlimited equipment subclasses"; **full_thruster** "Thrusting activates full throttle" (`GetBehaviorManager`, `CShip::any_thrusters_on`, `EngineEquipConsts::MAX_ENGINE_FX_THROTTLE`); **ship_dealer_enhancements** (paginated dealer); **patch_notes** (libcurl -> main-menu box). QoL-patcher switches [page: the-starport docs]: borderless window (default on), render distances, LOD ranges, contact-window refresh, FPS cap, HUD bar colours/width, "Disable Targeting Brackets", damage-indicator fade, background audio, hull/shield screen shake, "Disable Auto-level by Default".
- FLUF client hooks **coinciding with FLvibed symbols** [file: FLUF sources]: `FunctionDetour` on 0x59E300 (`UiRender`), 0x41F150 (`CursorUpdate`), 0x577850 (`WinKey`, twice: UiManager + KeyManager), 0x428680 (`PlayArch`), 0x5664A0 (`MainMenuButtonClick`), 0x59DB20 (control creation), 0x4347E0/0x57DA40 (IDS/infocard). **`FunctionDetour` writes a plain 5-byte `E9 rel32` and saves the original 5 bytes** (`FLSDK\include\Utils\Detour.hpp` lines 150-156); `VTableHook` swaps vtable pointers via memcpy - both chainable by FLvibed's rules [file]. FLUF **calls** `GamePaused 0x42D740`, `GetPlayerIObj 0x54BAF0`, `FindControl 0x59DA10`, `PlayUiSound 0x5645C0`, `FmtStrFormat 0x41C970`, `ClearWaypoints 0x4C46C0`, `AddWaypoint 0x4C4780`, `PlotBestPath 0x4C4EB0`, control factories 0x5A0DB0/0x5A1870/0x5A1900; **writes** 0x5C9020 (CinematicAspect) and widescreen/HUD constants, patches the call at exe+0xF4141 (`WaypointCheckDetour`, waypoints 0x672978, count 0x673374, system 0x673354 = FLvibed's `Waypoints/WaypointCount/PlayerSystem`), plus `ClientPatches.cpp`: chat limits 0x46A440/0x4691D1, cruise display limit, speed limit, ESRB notice, nav-map group members, RP8+0x4467, Common+0xDC34C/0x33487/0x995B6/0xE698E, Common+0x13F48C max dock range 20,000 [file].
- Module API: `FlufClientModule` `OnClientStart/OnClientUpdate/OnClientFixedUpdate(60 Hz, gamePaused)/OnMultiplayerStateChange/OnUiRender`; shared `OnCommonCShipInit/Update/Destructor`; packet-level `OnEnterTradeLane(XGoTradelane)`, `OnActivateCruise`, `OnLand(FLPACKET_LAND)`, `OnSetReputation`, `BeforeRequestEvent`, `OnPhysicsUpdate`, `OnCargoObjectUpdated`, `OnPayloadReceived`; priorities, `SuppressEventCall()`, per-module IDS offset ranges, `KeyManager` custom commands [page: module-system doc; file: import lists].

#### 3.8 Rendering, window, HUD, widescreen
- `EXE\d3d8.dll` = **ReShade's d3d8to9 1.12** (exports only `Direct3DCreate8`, imports d3d9) [file]; "FLUF can now work with either DX8, DX9, DX12 and Vulkan renderers; Discovery Launcher now contains a toggle to choose your renderer" (5.3 hotfix 8) [file: notes.yml]. HDE ships dgVoodoo/DxWrapper. FLvibed reaches the device via RP8's IAT -> `IDirect3D8::CreateDevice` [file: FREELANCER_RE.md], fine with any wrapper; only vtable identity assumptions about real d3d8 would break.
- Borderless/windowed: FLUF settings + `screen_control.dll`; notes: borderless checkbox removed as "main suspect in the 'Black Screen' era" (5.3 hotfix 4), "Fixed: Borderless windowed mode" (5.3.4), fonts on high-res monitors (5.3.1 hotfix 2), "Missing text at 2160p (4K)" (5.3 hotfix 8) [file] - the same display-matrix problems FLvibed lists as unverified musts.
- HUD: `hudshift.dll`; DiscoveryGC `TargetScreen.dll` replaces the target window with `Interface\hud\hud_targed_advanced.cmp`/`hud_shipinfo_advanced.cmp`, hooks 0x4E0F7C (`StealHudTarget`), 0x4E0CD2 (destroy), 0x4E2BC7/0x4E2C25 (click), 0x4E3E96/0x4E3EBE/0x4E7359/0x4E736D/0x4E732E (target model rotation/wobble), 0x4E7C00/0x4E7CA2/0x4E7C0C (equipment traversal), 0x062935E9/0x062A0380 (in a DLL), control factory 0x5A0DB0 (args: style 0x5D8CCC, cmp path, vector, matrix, 10 flags), destroy 0x59FA50 [file: `repos\TargetScreen\dllmain.cpp`]; `SkylineParallax.dll` hooks 0x066FE5F4, reads camera 0x667C5C and system 0x673354, config `DATA\universe\backgrounds.ini` [file]. None in `flexe.h`, but the HUD_Target family sits next to FLvibed's `VtHudIndicators` work - expect a non-vanilla target window under Discovery.

#### 3.9 Single player, story, saves, dialogue, input
- `freelancer.ini` [file]: "Modified by Igiss for Discovery Mod: no intro movies, server reporting functions enabled, discovery.dll added to dll list"; `[Startup]` movies commented out; `[Initial SP DLLs]` keeps `StoryLauncher`; `[Initial MP DLLs]` adds `Content-0C458F.dll, CrashPatch, LOWEST`; `[Data]` adds `solararch_playerbase.ini`, event/upgrade/power/cosmetic equipment+goods, `loadouts_special/utility`, many fuses, `rtcslider.ini`, `market_capital.ini`; `voices_mission01..13.ini` vanilla. The base patch's DATA has only encounter definitions, no `MISSIONS\M01..M13`; the wiki says the campaign is removed since 4.85 because moved gates/holes break the scripts [page]. "Campaign objects" appear in notes only as infocard holders [file].
- Saves: vanilla `GetUserDataPath` -> `My Games\Freelancer`; FLUF can redirect (`setSaveDirectoryRelativeToExecutable`, `saveDirectoryName`) and `FLUF.UI.dll` imports `GetUserDataPath` [file]; a "new Discovery save path" is announced for 5.3 [page: snippet] but the shipped `FLUF.yml` has the redirect off and the launcher still wipes `My Games\Freelancer\Accts\SinglePlayer\*.fl` [file] - exact path unverified. `mpnewcharacter.fl` (118 KB), `neutralcharacter.fl` define the MP start [file].
- Dialogue/comms/subtitles: no subtitle system; vanilla voice banks + NPC chatter; strings overridden by resource DLLs and at run time by DSAce/FLUF `GetIDSString`/`GetInfocard` hooks [file]. Movies: `Movie.dll` imports unchanged; intros disabled by ini [file].
- Input: `KeyMap.ini`/`keylist.ini` add the commands in 3.5; a 5.3.1 "keybind to swap strafe and turn keys" [file]; **no controller/XInput/joystick strings in any Discovery or FLUF binary** [file: grep of FLUF.dll, FLUF.UI.dll, modules, d3d8.dll].
- Autopilot/AI/docking/lanes: nothing client-side; server plugins touch `GoTradelane`, `StopTradelane`, `SetManeuver`, `RequestBestPath`, docking (`mobiledocking`, "disabledock", `HkCb_Dock_Call`), NPC spawning (`pvecontroller`, `npc_control`), `dynamic_missions` [file]. `constants.ini [FlufConstants]` `CAMERA_ACCELERATION_MODIFIER`, `USE_HITRAY_EXPLOSION_LOGIC`; "Ships now get pushed away from each other when colliding", "Cruise animations now synchronize between players", "Autopilot crashing ships into Brunoy" fixed by moving rocks [file/page].
- HDE: explicitly incompatible installs [page]; both are vanilla 1.1 + dacom plugins + a d3d8 wrapper, but Discovery replaces jflp/flplusplus/FLSharp with FLUF+DSAce and patches Freelancer.exe/Content.dll on disk.

### 4. Open-source map (NOT in `FLvibed\research\repos` unless stated)
| Repository | Clone | Contents | Offsets/hooks/structures for FLvibed | On hand? |
|---|---|---|---|---|
| `DiscoveryGC/FLHook` (5 Dec 2023, 462 commits, DBAD) | `repos\FLHook` | FLHook 3.1.0 + ~40 plugins + configs + LegacyContent | **`LegacyContent\dsace_client_hook\public_funcs.cpp`**: HandleKeyCmd 0x576410, key-name table 0x614FC4.., GetIDSString 0x4347E0, GetInfocard 0x57DA40, FormatXML 0x57DB50, GetObjByStr 0x59DA10, HUD bytes 0x679C0C.., colours 0x679B84.., mouse 0x616840/44/4C, screen 0x610850/54, mouse-flight 0x67354C/0x673554, background-run 0x5B264C, chat/camera patches, Common.dll cargo/docking/cloak patches; `acctpath_patch` Common+0xA2FA0; `Hook.h` server/common/content addresses; `plugin_documentation.txt` | No |
| `DiscoveryGC/TargetScreen` (Sep 2025) | `repos\TargetScreen` | one dllmain.cpp | HUD_Target hooks (3.8), control factory 0x5A0DB0 convention, destroy 0x59FA50 | No |
| `DiscoveryGC/SkyboxParallax` (Jul 2025) | `repos\SkyboxParallax` | main.cpp + FLCore headers | 0x066FE5F4, camera 0x667C5C, system 0x673354 | No |
| `DiscoveryGC/EquipMass`, `shipclass` (2024) | cloned | adoxa forks (/MT builds) | Common 0x639C098 `CShip::get_mass`; ship-class tables 0x4B79CA.. | adoxa originals on hand |
| `Lazrius/DSLauncher` (archived May 2024, GPL-3) | `repos\DSLauncher` | C# launcher + self-patcher | manifest format, launch args, save wipe, accounts | No |
| `DiscoveryGC/DiscoveryApi` (GPL-3) | `repos\DiscoveryApi` | ASP.NET Core API (online players, factions, stats) | none client-side | No |
| `DiscoveryGC/Tools`, `DSCore`, `Navmap`, `DiscoApi` | Tools only | data tools, web map | none | No |
| `Aingar/FLHook`, `lnvoker/FLHook` | not cloned | forks (Aingar 1,318 commits) [page] | same client content | No |
| TheStarport `FLUF`, `FLSDK`, `FLHookSDK`, `KnowledgeBase` | - | the framework Discovery runs on | all FLUF client addresses (3.7) | **Yes** |

Closed: current `dsace.dll` source/binary, server anti-cheat, `dsacesrv`, "ducktales", "disabledock", post-2023 FLHook tree, installer scripts.

### 5. Relevance to FLvibed
### Actionable (ordered)
1. **Signatures: never include inter-function padding.** 8/123 fail on Discovery solely because the window starts before the entry (`off > 0`) or runs past `ret`. Make `sigtool gen` stop at function boundaries / wildcard padding; re-run `scripts\sigcheck.py core\flexe.h <Freelancer.exe>` against the three Discovery exes in `extracted\` as a cheap regression (robustness/generalization for any mod that hot-patches padding).
2. **`HandleKey` 0x576410 is taken by DSAce** (2012 form `B9 imm32 FF E1`). `hk::Detour` chains only `E9`; add chaining for `mov ecx,imm32; jmp ecx` and `push imm32; ret`, or hook one level up (`WinKey` 0x577850, FLUF-chainable) / use `IsCmdHeld`.
3. **FLUF co-residency** on `UiRender`, `CursorUpdate`, `WinKey`, `PlayArch`, `MainMenuButtonClick`: FLUF's detour is a plain E9 (chainable) and its vtable hooks are pointer swaps (FLvibed calls the previous pointer) - run one scratch-install test with the FLUF release package already on hand; it de-risks every FLUF-based mod.
4. **Treat key-command table, HUD colours and HUD visibility bytes as data**: DSAce renames `USER_MANEUVER_*` slots and rewrites 0x679B84.. / 0x679C0C..; `KeyMap.ini` is the truth per install (keep `KeyCmdNames`/`KeyCommands` pointing at the live table; never ship a vanilla id list).
5. **Save path**: keep asking the game (`GetUserDataPath`); FLUF may redirect, the launcher wipes `Accts\SinglePlayer\*.fl` - FLvibed fixtures must never live there on a Discovery install.
6. **Docking/cargo/physics rules differ** (FREIGHTER docking type Common+0x63645, max dock range 20,000 Common+0x13F48C (FLUF), external equipment in cargo volume, EquipMass, full_thruster max throttle, formation catch-up 1.02, far plane x2): add to the "read at run time, never assume" list; keep `PhySys::GetMass`-style reads.
7. **Render wrapper**: d3d8to9 (Discovery) vs dgVoodoo/DxWrapper (HDE); device via RP8 IAT works for both; never compare D3D vtable pointers to real d3d8.dll.
8. **Mine `public_funcs.cpp` and TargetScreen into `docs\FREELANCER_RE.md`** (marked "from Discovery source, not verified in game"): HUD visibility/colour bytes, chat-limit bytes, background-run 0x5B264C, chat-open flag (`GetObjByStr("CTID")` +108 & 3 - controller must not steal keys while chat is open in MP), `FormatXML` 0x57DB50, HUD_Target creation/destroy sites and control-factory args, turret camera sites.
9. **Borrow the QoL-patcher catalogue** as a checklist of constants to expose as settings (contact refresh, damage-indicator fade, brackets, auto-level default, FPS cap, shake).
10. **Discovery is a low-value test target for the campaign goal** (no story, saves wiped, closed hook, Cloudflare site): use its exes for the signature regression only; do not install it for testing until `dsace.dll` can be inspected [judgement].

### Theoretical / ideas
- FLUF's module lifecycle + server payloads (`OnClientFixedUpdate(60 Hz, gamePaused)`, `OnMultiplayerStateChange`, `OnPayloadReceived`, priorities, `SuppressEventCall`) as a model for a future FLvibed plugin API.
- Key-slot reuse: new `USER_*` ids by renaming unused `USER_MANEUVER_*` entries, handled in `HandleKeyCmd`, visible in the vanilla options screen - a path for pad chords in the controller-first UI.
- FLUF Objective Tracker (ImGui objective panel) and Story Enhancements (custom `Act_*`/`Cnd_*` + save-file flags) - the campaign surface FLvibed's autopilot reads and could extend for runbook-free scripting.
- YAML patch-notes feed + MD5 patch manifest as a trivial update channel for subtitle data.
- Resource-DLL splitting (5.3.4) for large string sets; an IDS fallback would let vanilla string functions (and DSAce/FLUF overrides) see subtitle text.
- Server-controlled live zone reloads: under MP the world data FLvibed caches can change at run time - invalidate on `JumpInComplete`/base exit rather than caching once.

### 6. Sources
- Patch server: http://patch.discoverygc.net/ , http://patch.discoverygc.net/patchlist.xml , `http://patch.discoverygc.net/<name>.zip`; https://raw.githubusercontent.com/LazDisco/DSLauncher/patch/patchlist.xml ; notes feed https://discoverygc.com/files/notes.yml
- GitHub: https://github.com/DiscoveryGC , /FLHook , /TargetScreen , /SkyboxParallax , /EquipMass , /shipclass , /Tools , /DiscoveryApi ; https://github.com/DiscoveryGC/FLHook/issues/13 ; https://github.com/Lazrius/DSLauncher ; https://github.com/Aingar/FLHook ; https://github.com/TheStarport/FLHook ; https://github.com/topics/discovery-freelancer
- The Starport: https://the-starport.com/forums/docs/fluf , .../fluf/user/configuration , .../fluf/user/modules/qol-patcher , .../story-enhancements , .../objective-tracker , .../ui-overhaul , .../fluf/developer/concepts/module-system ; https://codeberg.org/TheStarport
- https://flserver.de/topservers.php?timeframe=month ; http://adoxa.altervista.org/freelancer/plugins.html
- Blocked, seen via search snippets only: discoverygc.com forum tid=200225 (install), 125278 (patch notes), 167176 (launcher changelog), 166570 (Launcher 2.0), 198946/198486 (5.0), 206823 (5.3 dev diary), https://discoverygc.com/wiki/Discovery_Mod ; ModDB mod/downloads pages; https://lutris.net/games/discovery-freelancer/ ; gamepressure 5.0 listing
- FLvibed (read only): `CLAUDE.md`, `docs\MOD_COMPATIBILITY.md`, `docs\FREELANCER_RE.md`, `core\flexe.h`, `core\hook.cpp`; on-hand repos `research\repos\FLUF` (ClientPatches.cpp, Fixes\*, UiManager.hpp, HudManager.hpp, InfocardManager.cpp, SoundApi.hpp, KeyManager.cpp, WidescreenSupport.cpp), `research\repos\FLSDK\include\Utils\Detour.hpp`, `KnowledgeBase\docs\community-info.md`

### 7. What could not be verified and why
- **discoverygc.com (forums, wiki, download page)**: Cloudflare "Verify you are human" checkbox in the built-in browser and 403 for curl/WebFetch; not bypassed (prohibited); web.archive.org unreachable from WebFetch. Install thread, forum changelogs, wiki version history, team list, 5.3 installer link/size unread; dates come from the patch manifest and notes.yml.
- **ModDB**: 403 to WebFetch/curl and the browser pane; 5.0 installer facts from search snippets only.
- **Full 5.3 installer**: no official readable URL found; not downloaded. The five patch archives (332 MB) contain every EXE-level file changed since 5.2 except the client hook.
- **`dsace.dll` (current)**: never in patches, withheld from GitHub; only the 2012 public half analysed. The 0x576410 collision and its byte form are from 2012 code.
- **Meaning of the 514 `call 0x401379` padding stubs, the `.rdata` -> `iMS ta` rename, float 0x5D6D38 (0.5 -> 0.2), the 20-byte patch at 0x52C8A1**: observed, unexplained.
- **Whether the retail vanilla exe carries the same 786 KB overlay**: no vanilla exe read (FLvibed `Install\`/`research\` off limits per brief); the NB10/pdb header suggests it does.
- **Adoxa authorship** of `multiCruise.dll`, `MultiUniverse.dll`, `MuzzleCone.dll`: not on his page.
- **Player counts**: only flserver.de aggregates and snippets; api.discoverygc.com exposes no unauthenticated online-count endpoint at the paths tried.
