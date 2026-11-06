# HD Edition after 0.7.1 (upstream changes and what they mean for FLvibed)

The facts of 17 Oct 2026; newer verdicts: `docs\research\hde-upstream.md`, `docs\research\npc-ai.md`.

Checked 17 Oct 2026 against the HDE repository (`research\repos\freelancer-hd-edition`, git-ignored clone of
github.com/FLHDE/freelancer-hd-edition, head `7c6186e` of 25 Aug 2026). The test install is the 0.7.1 release
(tag `45145d9`, 15 Jul 2025); its mission scripts are byte-identical to the tag. 153 commits since then, no new
release and no changelog section for them yet.

## Mission scripts (details at the end of each runbook in `missions\`)
| Mission | Change | Effect |
|---|---|---|
| M06 | goto disabled during the Hovis race, re-enabled after the finish | 0.7.1 lets the player autopilot the race |
| M08 | `gangster2` no longer activated (its block is commented out) | removes an orphan; no gameplay change |
| M10 | reserve escorts get random names | names only |
| M11 | missing `prototype_elite4` NPC added | 0.7.1 may spawn one prototype fighter fewer |
| M12 | commented-out open-single-player warning | none |
| M13 | generator name, airlock shield position, a pilot costume | visual / names |
| mbases | rumour and knowledge fixes (Kabukicho, Ainu robot) | text |

Open PR #270 "Mission 08 fixes" (not merged since June 2025): transport formation speed fix and a player "full stop"
via `Act_PobjIdle` (the clearest hint of what `PobjIdle` does). Other open PRs (#334 solar textures, #322 comm window
stays open with the nav map, #291 Blood Dragon pilot bodies, #235/#236 lane and gate infocards) do not touch
missions. Forks: of 21, only three have commits HDE lacks: CI workflows (StellarwhimsRepo), Beta sound effects
(shtaraen, 2023), one cutscene file (BrianxTu, 2023). Nothing for the missions.

## Engine, binaries, plugins (matters if the test install moves past 0.7.1)
- **Freelancer.exe** is now patched automatically from `FLDOCS\BinaryEdits.patch` (FLStaticPatch). Compared with
  our 0.7.1 file: ~25 small code edits, a data table near VA 0x666B8C-0x667068, the PE header (4 GB flag) and 4 KB
  more at the end. **None of the changed code bytes is at or near an address FLvibed hooks or calls** (frame update,
  keys, UI hit test, steering, room characters, NavBar, load / pause, target action 0x54BCB0, ...). New patches
  touching areas we work in: target view sub-target buttons for ships and no typing effect (glyph/HUD work), the
  dealer-menu location frame flicker fix (0x43E732), countermeasures no longer fire on right-click, faster contact
  list refresh (1.5 s), player ships in the contact list's ship filter.
- **Common.dll**: maximum docking initiation distance **removed** (0.7.1 multiplies it by 5); planets keep spinning;
  first-time machine speed test skipped. **Content.dll**: NPC call sign fix in dock requests. **server.dll**: small
  edits (empty_cube_frequency).
- **Windowed Mode Mouse Fix (`dinput8.dll`) removed** (Aug 2026); FL# 0.9.2 (`cursor_fix`, `mouse_warp_fix`) takes
  over. FLvibed's controller injects DirectInput mouse input and handles the window edge: **retest the mouse layer**
  (pad cursor, fire-select, handback) before moving to a newer HDE.
- FL# 0.9.x (already in the 0.7.1 install, older): `flight_controls_fix` (auto level / rotation lock reset on
  launch), `better_resolutions`, `dealer_menu_open_fix`, `copy_paste_feature`, waypoint fixes.
- New plugins: TargetScreen.dll (dynamic target view, top-down mode), VersionText.dll, LightingBugFix.dll (off),
  Story Factions update (mission NPC affiliation no longer shown in comm text - affects the comm text our subtitles
  sit beside), more console commands, engbase.dll animation crash fix.

## The second install (`Install\instances\hde_upstream`, made 6 Nov 2026; IDEAS M7)
A named instance for compatibility passes, never the baseline: 0.7.1 stays primary. Built from HDE `7c6186e` (0.7.1-153, the clone as it stands) with the
installer source at `55121f8` (31 Oct 2026, a new clone in `research\repos\freelancer-hd-edition-installer`), over `research\Vanilla\Freelancer`.
Not started yet: the launch check is owed (below).

**Why the installer and not an overlay:** about 35 `Process_*` routines (`mod_options.iss`) rename files, edit inis and patch bytes; the repository alone does not
run (its `*_tsr.dll` resources and `d3d8_*.dll` are renamed by them). So HDE's own option code ran, compiled from a copy.

**Inno Setup:** 6.7.4, from the jrsoftware/issrc release that jrsoftware.org links (10.6 MB, SHA-256 `A5499293...D8EB30E8C`, signed by Pyrsys B.V.),
`Install\downloads\innosetup\app`, with `/CURRENTUSER /PORTABLE=1 /NOICONS /DIR=...`: no admin, no uninstall entry, no file association, no shortcut, no PATH
change (compared before and after). 6.5.1, which built the 0.7.1 setup, cannot compile this source (`DialogFontBaseScaleHeight`). The only trace was an empty
`HKCU\Software\Jordan Russell\Inno Setup` key, removed afterwards; a new compile makes it again.

**The copy's edits** (`Install\downloads\hde_upstream_build\make.ps1`, git-ignored; the source as HDE ships it needs admin):
`PrivilegesRequired=lowest`, `Uninstallable=no`, `UsePreviousAppDir=no`; no icons, fonts (all four are in `C:\Windows\Fonts`) or VC++ redistributable; no 7z:
the mod's files come by the installer's own `dircpy` from `/ModSrc=<a robocopy of the clone without .git>` (the stage is deleted again; the repository's
working tree is CRLF, as the installer needs); `{userdocs}` never touched (`My Games` goes to `{app}\_mygames`, deleted after, the `Restart.fl` deletes dropped).
Option code untouched. Setup: `/SP- /SILENT /SUPPRESSMSGBOXES /NORESTART /DIR=... /LOG=... /VanillaFlDir=... /ModSrc=... /NoMsvcRedist` and the primary's options
`/DisplayMode=2 /GraphicsApi=1 /JumpTunnel=0 /SkipIntros=0 /NewSaveFolder=0`. 16 s, 5.2 GB, log `Install\instances\hde_upstream\hde-install.log`.

**After it:** `flplusplus.ini` `save_in_directory` / `screenshots_in_directory` true, an empty `SAVE\Accts\SinglePlayer`, `Set-FLInstanceDisplay hde_upstream borderless
'3840, 2160'`. `deploy.ps1 -Instance hde_upstream -Background -NoBuild -NoStart` reached it (dacom.ini, dev build 1c917e6e copied in: stale for any test, deploy again).
`Assert-FLNoFullscreen` and `Assert-FLDataSeparate` pass; `sigtool check` 228 / 228 at their 1.1 addresses on this exe.

**Compared with 0.7.1, as installed:** FLSharp 0.9.2 (0.8.0), exe 2,523,648 B (2,519,552), DxWrapper 1.7.8467.25 (1.3.7700.25), no `dinput8.dll`, TargetScreen.dll and
VersionText.dll in dacom.ini, HDEResources.dll for Controls.dll in freelancer.ini. The setup's 16 changed exe bytes equal 0.7.1's at every offset except 0x0E15D3
(`26000700`, the IDS of the DynamicTargetScreen button, upstream only). `PerfOptions.ini` is the installer's own: `depth_bpp= 32` (the main install 24).

**Owed once a game is free:** a background start, the main menu reached, the log clean, FLvibed loaded beside FLSharp 0.9.2; deploy first (a fresh build). No fixtures yet: mission
saves of 0.7.1 are not assumed to load (M08 / M11 scripts changed). The test scripts take a named instance only with 31fe28ad; a worktree reaches the instance by its `Install` junction.

## For FLvibed
- Runbooks stay on 0.7.1 (what tests run); the upstream notes say where newer HDE differs.
- Moving the test install to a newer HDE is the user's decision (installer options, `tools\install-hde.ps1`); the
  hook addresses are unaffected, the mouse layer needs a retest, docking from far away gets easier (no distance
  limit), M06 no longer allows autopilot in the race.
