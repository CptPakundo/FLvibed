# FLvibed 0.4.7: release notes (released 15 Nov 2026)

Status: released from `main` on 15 Nov 2026 (the owner's word). Public v0.4.6 was the dev history up to `RELEASE_0.4.6.md`; everything since is 0.4.7.
The tracker is `..\audit_2026-11-12\117_release_0.4.7.md`. The player-facing list is `CHANGELOG.md`, section 0.4.7.

## What it is
Bug fixes since 0.4.6, raw mouse input on by default, a calmer classic HUD (its cues start off, its tooltips and name card
behave as the game's own), the loot notice that says FULL (issue 87), HUD 2's own INFO page, and, underneath, a page
framework for HUD 2's docked pages.

## What is in it for the player
- **Raw mouse input, on by default** (`mouse_raw`, 497d794e; its options row 2788b127). Measured in the owner's own game on
  14 Nov 2026: before, the game's own DirectInput mouse moved the cursor about 3.6 px per mouse count at slow speed and 5.6 px
  fast (also with FLvibed not loaded: `docs\FREELANCER_RE.md`); with raw input, 3.0-3.2 px per count at every speed. The owner
  after trying it: "Raw mouse input is excellent". It is core's, so it works with any module on or off. To turn it off: the
  options page, MORE > SYSTEM > Raw mouse input (with Show all settings), or `mouse_raw = 0` in `FLController.ini`
  [Controller]. It falls back to the game's own mouse by itself: on an absolute device (a tablet, remote desktop, a virtual
  machine's mouse), when its listener stays silent while DirectInput moves (250 ms over 3 reads), and when another part of
  the process already holds the mouse's raw input (it then never registers, and gives up for the run if its registration is
  taken later).
- **The device rule** (eb8865e5): only a new press or move takes the device, never a held one. Measured in the owner's game
  log on 14 Nov 2026: a held controller trigger and a held left mouse button swapped the device about every 50 ms, each swap
  resetting the mouse and restarting the grips' haptic stream, and the game slowed to a crawl.
- **`pad_mapper_mouse`** (hidden, default 0; 6cc9b986): while FLvibed reads a PlayStation or Switch pad itself, a mapper's
  keys for that pad are still dropped, but the low-level mouse hook that dropped its mouse input too is installed only with
  `pad_mapper_mouse = 1`: nothing needs it unless a mapper also sends mouse input for the pad (one hook less in the mouse
  chain). It is not a mouse fix: the speed-dependent gain was the game's own mouse (4a6cbeb0 corrected the first wording).
- Flight HUD: the loot and payout card waits for room instead of climbing to mid-screen; a faded-out prompt holds no room;
  the first prompt takes the corner, the second stands beside the ship panel; the loot notice says FULL (issue 87).
- Pad: View closes a Neural Net window pressed right away; every prompt of a HUD 2 page shows (B CLOSE no longer cut); the
  pad keeps steering after a window with the HUD module off.
- INFO is HUD 2's own page (scrolls, aligned market lists, bold runs with a synthetic emphasis).
- Classic HUD: goal panel buttons in the panel's foot; cues off unless `classic_cues`; the game's name card and tooltips;
  the idle cursor hides; a switch during a load is classic at once.
- HUD 2 is on or off on every device (ON THE PAD is gone); turning it off leaves the HUD module running; three settings that
  put the game's screens in HUD 2's skin are gone.
- HUD 2's quit and confirm boxes (QUIT TO THE MAIN MENU?, EXIT FREELANCER? and the other two-answer boxes): the arrow keys move
  the highlight between the two answers and Enter takes it; Escape cancels as before (43bbc409). On the pad the D-pad stays
  on the two answers (quit to the main menu, delete a save) instead of walking onto the screen behind (cd4f897d).
- Phase 1 fixes (bundles A and B, in main 572ed199): in a cutscene (a launch, a docking, a mission's scene) HUD 2 shows only
  the subtitles, no chat log beside them (issues 12 and 92); no radio line stays over the death page; the equipment dealer's
  rows on your side keep their sell price while you scroll (issue 93); the game's hover help (the FRIENDLY plate under STATUS'
  faction rows) stays hidden while a HUD 2 page stands; the idle cursor has its own row (HUD > CUES AND TEXT > Hide the idle
  cursor); the goal panel's foot lists its keyboard and mouse keys (TAB, DO NOW, and under CLOSE SHIFT plus the autopilot key
  and SHIFT+CLICK); the maneuver row's first key is named by the default keyboard layout, the same at every start.
- Round 2 fixes: on the pad, X enables DELETE on the load / save list (the game's own "DELETE THIS SAVE?" box still asks); HUD 2's save lists show only the rows that fit their panel (15 at 1280x800, 19 at 4K; scrolling reaches every save; classic keeps 20).
- Also in 0.4.7: a comm's animated head no longer shows through INVENTORY's icons or other pages in space (issue 89); the pad's X deletes a save in the load and save list.
- Options: six settings get a row, and Raw mouse input a seventh; Show all no longer lists the raw keys. A crash leaves `FLvibed_crash.log`. The test tools'
  channel opens only with `FLVIBED_TEST=1`.

## Under the hood
The HUD 2 page framework: repair, BEST PRICES, the goods and equipment dealers, the ship dealer, LOG, the death page and STATUS
(INFO before them), and now the job board, the news and INVENTORY with its jettison panel, are pages of one runtime (no page
body is left in basepage.cpp or nnpage.cpp; INFO's left column stays in the shell, a phase 2 unit if wanted), with the same
screens and no intended change in behaviour. The retired fallbacks' last fields are gone: older design-bench state recordings
(flstate/0) are no longer read; the DLL never reads them and no player file is touched. The small differences
the commits name:
- Read clocks are wall time (GetTickCount) where a sum of the frames' time stood: the ship dealer's ships (0.4 s, 50 ms after a
  pick), STATUS's list (0.5 s) and its card.
- The ship dealer's right-stick repeat is its own, reset on each open (a stick held while it and the job board or news change
  steps once more); its card text starts empty on each open (before, the last visit's stayed until the first draw).
- STATUS's card clocks are its own (INVENTORY's walk shared them within 0.25 s of a tab switch).
- The ship dealer reads its window before the park in the frame, not after.

## Changes nobody asked for
- Goal panel foot: also applies to "HUD style classic with the HUD module on"; QUEUE and DO NEXT are drawn dimmed to 40 %; the
  action rows' room is smaller by the foot's height (still 6 rows at both sizes); the bold emphasis is wider than the game's own.
- Lower band: the lint's right-rail rule exempts every prompt; a notice card that loses its place while fading keeps its fade
  level; when a prompt returns, the notice in the corner disappears at once (no fade-out) and waits; other ships' target marks now
  draw where a faded-out prompt stands.

- The game's own hover help is off under every HUD 2 page (the fix for the FRIENDLY plate hides it for all of them, not for
  the one row); HUD 2's own help stays.
- A line playing at the moment of death now ends at the death camera (the radio line over the death page).
- The goal panel's foot with the keyboard and mouse starts with TAB and wraps to two lines; ESC is not listed under CLOSE.
- HUD 2's HUD in calm flight row no longer controls the classic HUD's idle-cursor hide; its own row (Hide the idle cursor)
  does, with HUD 2 disabled as well.
- The job board and news: the shared holes after the board are bands, not the panel's rectangle, for the first frame only if
  another docked page replaces it with no frame between (not seen by any check).
- In SAVE GAME, X replaces a typed description with the selected row's (the same as a mouse click on a row).

## Known / not yet seen in the game
- Issue 91 (the map's cursor vanishes at the edge columns and rows) was not reproduced: in a background game the arrow cursor
  showed at every grid point at 1280x800, 1920x1080 and 4K with the virtual mouse and pad. It needs a real mouse and stick.
- Not opened in a game: the Alt+F4 box (a posted key is no key message), the delete-a-save box with the keyboard, the
  leave-server box (needs a server); the mouse wheel on the job board's text; INVENTORY's single-item jettison (Y twice,
  "AGAIN TO JETTISON").
- Raw mouse input: the numbers above are the owner's game with a real mouse; the test games run in the background and never
  read the mouse (no GetDeviceState call), so only the listener's start, the switch and the options row were seen there
  (instance 21, release build, also with `[Modules] controller = 0`). The fallbacks are proven by unit tests (`rawmouse`)
  only; no tablet, remote desktop or virtual machine was tried.
- The device rule is proven by its unit test only: the harness's virtual mouse cannot hold a real mouse button.
- The goal panel foot's two-line wrap was never drawn in a frame; "HUD style classic with the module on" was not run; at extreme
  text sizes the three-row floor can still overflow.
- The waiting loot clock and the faded-prompt case are proven by unit tests only; two of the 24 recorded lower-band states
  (controller, size 80, loot) have no in-game lint.
- To try in the owner's pass: tractor loot during radio chatter in calm flight (default prompt setting, wait for the prompts to
  fade); hover help and the name card with keyboard and mouse; switching HUD 2 back on; real controller and mouse over the moved
  pages (repair, best prices, dealers, log, death page, STATUS, INFO).
- Left for later (phase 2, on the owner's word): INFO's left column as a page; the issues still open
  on GitHub. Done since the first candidate notes: the job board, news and inventory on the framework,
  radio subtitles over the main menu after the death page, the FRIENDLY plate, the maneuver row's first key name.

## Verification
The candidate's automated checks ran together on the version-bumped release build (13 to 14 Nov 2026, one test
instance, background): the HUD modules, the classic HUD's 16 pinned frames (none changed), the Neural Net pages (1,012
checks), the INFO comparison (20 pass, 1 known), the pad's page moves, the dealer's focus, ship buying on the pad (no
failure; three steps could not run for lack of a row with an overflowing description), the base pages field by field
(6,898 equal; one read of the game's own transfer price came back empty and passed on its re-run), and the design
bench's replay of 100 newly recorded frames (equal). The final gate ran on fe22e9ec: 13 of 14 nets equal; pad_page_moves failed once at its jettison row, judged a flake (5 re-runs of that row and the net passed, the code ruled out B jettisoning; its test now names a changed hold's items and keeps the game log). Later commits are documents and that test. Phase 1 went through two gates: gate A on 96d46da4 (the bench, HUD modules,
Neural Net pages 1,012 / 0, INFO comparison 20 pass and 1 known, the pad's page moves, the dealer's focus all pass; the classic
HUD's frames differed only in the new idle-cursor row and the options list's scrollbar thumb, and were re-pinned for those; the
base-pages read of the game's price text, then flaky, was the test's, fixed in 90ab550a, six runs 4,282 / 0; the state-fixes
right-stick row tested a window HUD 2 no longer drives and was rewritten). gate B: all nets equal on 572ed199; bench_level1 equal after 2ee6d369 (audit 138_). The arrow keys of the quit and confirm boxes (43bbc409) were checked
in a background game only, and by the owner's own try on 14 Nov ("I checked the changes, they work"); not with a real keyboard
in a front game by another tester, not at 4K, and the delete-a-save, leave-server and Alt+F4 boxes (same code path) were not
opened. The owner's pass: he tried the 43bbc409 build in his own game on 14 Nov 2026 ("they work"; raw mouse input "excellent", the device rule "works great"). Phase 1 and round 2 were not tried by him with real devices; his list of what needs his hands is kept apart (real mouse, stick and pad over the cutscenes, the quit and delete boxes, the FRIENDLY plate, the job board's wheel, the jettison, the goal panel's foot). The item-by-item pass is not done.

## The release build and package
The released files (15 Nov 2026):
- `FLvibed.dll`, from commit 7b27dfbc: SHA-256 98ebb93c5741cb42e0aec97cb02aa86bc263f7bf4d5bc439a63993c88a6f3bab, 6,526,464 bytes
- `FLvibed-0.4.7.zip`: SHA-256 83ab452b45cbcc53bda60bf2770b6a07b8e657933ac6414b32ab299f4ce786b2, 4,479,187 bytes (the DLL inside it is that file)
- `FLvibed-0.4.7-Setup.exe`: SHA-256 f0ec180e926f0c0d058e5beb5e34aca1a3e05b2ba65d51d21774e58d2b72afd9, 15,558,656 bytes; the setup test passed (install, reinstall, uninstall, a hand-written line, a broken dacom.ini)

## Release steps
As done for 0.4.6 (`RELEASE_0.4.6.md`): export the archive of main into the published clone, commit, tag v0.4.7, push, GitHub release with the zip and Setup, then the issue replies; the driver fills the file lines above after packaging.
