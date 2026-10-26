# HD Edition after 0.7.1 (upstream changes and what they mean for FLvibed)

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

## For FLvibed
- Runbooks stay on 0.7.1 (what tests run); the upstream notes say where newer HDE differs.
- Moving the test install to a newer HDE is the user's decision (installer options, `tools\install-hde.ps1`); the
  hook addresses are unaffected, the mouse layer needs a retest, docking from far away gets easier (no distance
  limit), M06 no longer allows autopilot in the race.
