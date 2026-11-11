# How FLvibed is built

For anyone reading or changing the code. FLvibed is one DLL (`FLvibed.dll`) that the game loads through `dacom.ini`. It adds a native
controller, dialogue subtitles, a flight HUD, movie handling and an autopilot to Freelancer (2003) and Freelancer: HD Edition. It has
no prerequisite beyond Windows and the game: the C runtime is linked statically, and each feature is a module the player can turn off.

## The layers
```
Freelancer.exe and its DLLs
   |  detours and vtable / import patches, every address verified by signature (core\flexe.h)
FLvibed.dll
   HOST    core\flvibed.cpp   DllMain, the [Modules] switches
           core\host\         the router: every hook two modules share, once, in a fixed order;
                              input injection, the debug channels, game fixes, the UI hit points
           modules\*\src\host each module's own hooks, installed only when the module is on
           core\loader.*      the one owner of the logic's load, unload and reload
   LOGIC   core\game\         the shared game layer, one instance: rooms, missions, space objects, AI, routes, drawing
           core\ui\           the draw kit, glyphs, the row kit, the options page, the views
           modules\*\src      each module's logic, in its own namespace
```
**The host** never reloads. It owns everything that must outlive the logic: hooks, the buses, each module's fault flag and fail-safe.
**The logic** is where features live. In a release it is linked into `FLvibed.dll`. In a development build it is `FLvibedLogic.dll`,
reloaded in one act while the game runs. Both builds have the same shape on purpose: a development build shaped differently from a
release would hide exactly the state bugs a reload can cause.

## The modules
| Module | Folder | Owns | Alone it gives |
|---|---|---|---|
| Core | `core\` | DLL entry, router, input injection, buses, game layer, draw kit, options page, the multiplayer rule reader and the chat lines it reads, the play session and every save's mode, the maneuver watchdog, FLUF awareness, the focus guard | the options menu and the watchdog on any install |
| HUD | `modules\hud\` | HUD 2, the Extended HUD 2 skin, shield and reticle cues | HUD 2 on keyboard and mouse |
| Controller | `modules\controller\` | device, presets, aim, pad screens, prompt bar, on-screen keyboard, wheel, picker, groups, haptics | the full pad game on the classic HUD |
| Subtitles | `modules\subtitles\` | dialogue, chat, speakers, the display, the shipped lines | subtitles on the classic HUD |
| Movies | `modules\movies\` | fit to screen, skip | fitted movies |
| Autopilot | `modules\autopilot\` | the autopilot, situation assessment, goal queue, goal panel, bar button | the autopilot on keyboard and mouse |
| NPC | `modules\npc\` | the stall recorder and watch | the recorder |
| Display | `modules\display\` | borderless windowed, window fit, running out of focus | window mode without the pad |
| Speedrun | `modules\speedrun\` | clock, split detector, LiveSplit client, the New Game list on keyboard and mouse | timer and splits; with the Controller on, its prompt is the one |

A module is a namespace, a folder and a switch: a row of `[Modules]` in `EXE\FLvibed.ini`. A row that is absent inherits from the
module it was cut out of (`hud`, `autopilot`, `speedrun`, `display` and `npc` from `controller`, `movies` from `subtitles`). A module
that is off installs nothing. `safe = 1` turns everything off, for troubleshooting. With both `subtitles = 0` and `controller = 0` nothing
starts, whatever the other rows say: "alone" above means with one of those two on, because the debug channels and the logs hang on them.
The options page sets every row and keeps one of those two on. Its save writes what the next start would otherwise change: an absent row
stays absent while it would inherit the value anyway, and a parent turned off writes the children that stay on (`modules::ToWrite`).

`server\` holds the optional server plugin. `subtitles\review\` is where the subtitle text is authored: speech recognition, alignment
and a review tool. It is no part of the DLL and publishes its products into `modules\subtitles\data\`.

## How the parts talk
**Dependencies point one way: a module uses core, never another module.** `tools\module_lint.py` fails on any include that crosses, and
the namespaces make such a call fail to link. Module to module runs through core:

| A module needs | It uses |
|---|---|
| a place in the frame, a draw pass, init, shutdown, a debug verb, a game event | a slot in `core\game\modslots.h`, called at a fixed place, so an off module is an empty slot and the order cannot drift |
| to read a module that may be off | a view in `core\ui\views.h`, with a helper that behaves as without that module when the view is null |
| a host service | `core\game\hostcalls.h` |
| settings | a registry over `core\settings.h`, shared through `core\settings_bus.*`; the options page reads every registry through the bus. A changed default reaches files saved before it through the section's `settings_rev` and its `Carry` rows (`core\settings.h`) |
| the screen layout of another part | `core\hud_bus.*`, a published read-only view; a missing publisher means stale, absent, classic behaviour |
| held input | `core\game\flightio.*` over `core\host\input.*`: one slot per command, tagged with the owner that last claimed it (`core\game\input_rules.h` says who may); a release is for everyone, the frame claims again in the same pass |

**The autopilot keeps working if the game isn't paused.** No window, menu or page stops it: under a NeuroNet window or the options page it
flies on (`inputrules::FliesUnderWindow`, the frame row `Window`), and the window keeps the pad, the keys and the cursor. `flightio::UnderWindow`
tells the flight so: it reads no pad, moves no cursor, and gives the guns a world point. Only the pause holds it.

**One frame order.** `core\game\coreframe.*` runs the logic's frame and draw passes in one sequence for every combination of modules.
The router's table keeps the hooks in a fixed order.

**A fault stops one module, and gives the game back.** The Autopilot, HUD, Speedrun and NPC modules run inside the Controller's entry (or, with
the controller off, core's page host), and every way into them is guarded: a slot through `modslots::Frame` and its neighbours, a view
through `modslots::Guard`, so it makes no difference who called. A fault empties that module's slot and view at once, so the rest of the
frame behaves as with it off, and logs which module it was. At the top of the next frame its `Fault` slot gives the game back what it
changed, each step under its own guard. A fault in the entry itself stops that entry: the Controller's (or core's page host), the
Subtitles', the Movies', each with its flag and fail-safe in the host; the other entries play on. That fail-safe at once releases held
keys, steering and the cursor, and the Controller's entry or core's page host then gives back through `OnFault` what its modules changed; the
Subtitles' entry has none, so the maneuver bar and comm window it moved stay moved until a restart. `tests\fault_giveback.ps1` breaks both on purpose.

## How it builds
`build.cmd` is the only build; run it through `tools\buildlock.ps1 <target>`. It compiles every module source through a generated
wrapper: `core\pre.h` first, then the source inside `namespace flv_<module>_<host|impl>`. That has three consequences:

- Modules link into one DLL without name clashes.
- **Every system header a module uses must be in `core\pre.h`**: it is included before the namespace.
- **Never forward-declare a core type in a module header; include it.** `core\game` is one instance at global scope, and a logic wrapper
  includes every core header first, so a declaration inside a namespace would be a second, different type. `tools\core_once.py` proves
  the one instance from the link map.

**The toolset is pinned:** MSVC 14.44 for every target, because later toolsets no longer target Windows 7. `build.cmd` stops if it is missing, or if the toolset's
`yvals_core.h` has lost its Windows 7 level; SDL3's cached library is stamped with the toolset; `tools\win7_check.ps1` refuses a binary with a newer part in it.

Targets: `dev` (default), `impl` (the logic alone), `release`, `test` (the host-free unit tests in `tests\`, seconds, no game), `sigtool`, `setup`, `flhook` (the server plugin), `mptest` (the local test server's tools).

## Rules the code keeps
- **No raw addresses.** Every hooked, called, read or compared Freelancer.exe address is a symbol in `core\flexe.h` with its signature.
  The host and the logic each verify the table at startup. A mismatch fails closed: that hook, call or read is refused and logged, and
  only its feature stops. The verified facts behind the symbols are in [FREELANCER_RE.md](FREELANCER_RE.md).
- **Hooks chain.** Detours follow an existing jump, and vtable and import patches call the previous pointer. Other plugins may hook the
  same address: never take one exclusively, never change game logic.
- **Reload.** A new host function goes into the module's `HostApi` with a version bump and is called only behind the version check: the
  logic must cope with an older host. Logic-side patches, window classes and threads need a teardown, because a reload frees that code.
- **Nothing player-facing moves.** Ini files, sections, keys, defaults, log names and folders stay as they are, whichever module owns
  them. So `[Controller]` holds rows of the HUD, Speedrun and Display modules, and one section may have several owners.
- **Comments** follow [COMMENTS.md](COMMENTS.md); `tools\comment_lint.py` reports what breaks them.

## Adding a module
1. Make `modules\<name>\src\` and its `CLAUDE.md` map, and add its namespace to `build.cmd` (`:wrapmodule` for logic, `:wrappart` for a host
   part) and to `MODULES` in `tools\core_once.py`. A host part also needs its `Start` in `core\flvibed.cpp`, under its row.
2. Add its slot to the enum and its name to `kNames` in `core\game\modslots.h`, a `<name>_module.cpp` with the slot table and `extern "C" flv_<name>_module()`, and
   fill the slot in `core\logic\modules.cpp`. `Frame` and `Draw` are called per slot: add the call in `core\game\coreframe.cpp` and in
   `core\logic\core_page.cpp`, or the module never runs. Views go in `Attach` and `Detach`; what it changes in the game goes back in
   `Shutdown` and `Fault`. A module that logs starts its `Init` with `LogJoin(hostcalls::g.LogWrite)`, or its lines vanish: it writes to
   its entry's log. Every view entry goes through `modslots::Guard<slot, fn>::Call`, or a fault there stops the whole entry; its `Fault`
   gives back what a stopped module no longer hears (the HUD's state blocks, since `DeviceLost` no longer reaches it).
3. Add its `[Modules]` row in `core\modules_rules.h` (`Row`, `kRowNames`, `kInherits`, and `tests\modules_rules_test.cpp`), with a default
   that changes nothing for existing players, in `kModuleDefs` and `kModuleDefaults` of `core\flvibed.cpp`, as a `mod_<name>` row of the
   Feature Tour with its entry in `ALIASES` of `tools\ui_meta.py`, and in `$rows` of `tools\deploy.ps1`.
4. Reach the rest only through the doors above. A hook only it needs goes in its own host part; a hook a second module needs moves to
   `core\host\`. A setting whose row stays in another module's registry is a macro in `core\ui\shared_rows.h`, listed in both tables
   (`core\logic\alone.cpp` with the module that reads it), with its owner in `RowOwner` (`core\ui\options_page.cpp`) and the Feature Tour.
5. List its files, settings and verbs in `docs\module_map.tsv`, its folder in `FOLDER` of `tools\module_lint.py`, add a test that includes its
   pure headers, and run `tools\module_lint.py --enforce`. A file it writes at run time goes in `tools\setup\setup.cpp` and its test. Note
   that `.gitignore` is a whitelist: check that new files are really tracked.

## Testing
Pure rules have unit tests in `tests\` (`build.cmd test`). Everything else is tested in the running game by the harness in
`tools\harness\` ([HARNESS.md](HARNESS.md)): background instances, driven by game state and the debug channels, judged on frames and
logs. Rules: [TESTING.md](TESTING.md). Each file's owner: `docs\module_map.tsv`.

## Words you will meet in these docs
| Word | Meaning |
|---|---|
| the owner, the user | the project's maintainer, whose decisions the docs record |
| leg | one story mission's stretch, flown by the autopilot as a timed test |
| sweep, section | the regression run `tools\harness\flsweep.ps1`, and one of its parts |
| gate | every mission leg once, compared with the best so far (`tools\flgate.ps1`) |
| set | several runs of one leg, to compare two settings |
| AFK run | a long unattended work session |
| `Q<n>`, `H<n>`, `J<n>` | a package of the project's private work queue, in its tooling or its attended checks; a slice, step or probe is a part of one |
| `A37`, `A38`, `T<n>`, `U<n>`, `D<n>` | an idea's id in [research/IDEAS.md](research/IDEAS.md); `A37` is the situation assessor and arbiter, [A37_PLAN.md](../modules/autopilot/docs/A37_PLAN.md) |
| `P<n>` | a package of the split into modules, [MODULES_PLAN.md](MODULES_PLAN.md) |
| the private handoff, work queue, harness notes, session hooks | files of the development repository that the public export leaves out |
