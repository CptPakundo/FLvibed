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

**Drawing, with and without the device.** What FLvibed draws is prepared apart from the Direct3D device that shows it, so a
program with no game (the design bench) runs the same preparation: `core\game\fontraster.*` rasterises and packs the fonts
with Windows' GDI and looks glyphs up, `render.*` uploads the atlas and draws; `core\game\flicons_decode.*` decodes the
game's flat icons, `flicons.*` loads them on a thread and uploads them; `gamefont::LoadFrom` resolves the game's faces from
a given DATA folder, `Load` from the running game's. HUD 2's draw kit builds its six vertex lists in `core\ui\hud2_kit.cpp`
with no device (and a seventh, `LShip`, drawn last and never recorded: the NPC ships' outlines from the source the HUD module
sets, `modules\hud\src\npcicons.*`, which bakes them from the ships' models on a worker thread; none on the bench); the fonts, faces, icons and the draw calls are behind `hud2_kit_backend.h`, which the game's
`hud2_kit_d3d8.cpp` implements (a program with no game links its own: the design bench, `build.cmd bench`, links
`tools\hudbench\kitlist.cpp`, which records the lists instead of drawing them; the unit tests compile the kit with it
too, so a game header that creeps into the kit fails the build). The two sides share one copy of each rule: the atlas
image and the font fingerprint (`fontraster`), the icons' cells (`flicons_decode.h`), the glyphs' colours and look
(`glyphs.h`, as functions of the setting's value). HUD 2's parts (`modules\hud\src\hud2_parts.cpp`, `hud2_target.cpp`,
`hud2_cue.cpp`, `hud2_click_draw.cpp`) read their clocks, the player's typing and the keyboard form's hotspots (the
rectangles they record, the one under the cursor) through a host bound to them (`hud2_frame.h`: `FrameHost`; the kit's
marquees run on its clock too, `hud2kit::SetClock`) and their settings from copies (`PartsIn`: `uiopt`, `hudrows`, the
pad family the glyphs draw in, handed to the kit as a `hud2kit::GlyphStyle`, and `hud_size`'s results, which the layout
core takes in its `Frame`), and keep what they carry from frame to frame in a `PartsState` their caller owns (the kit's
marquee table with it, `hud2kit::SetMarquees`). These files, the frame below and the headers they share are HUD 2's
game-free core: `tools\module_lint.py --enforce` refuses a game header in them, a mutable static (but the parts' binding)
and a read of what the frame hands in (`BENCH_CORE`), and the unit build links them with no game source
(`tests\hud2_core_test.cpp`). HUD 2's frame itself is a set of functions of what they are given
(`modules\hud\src\hud2_frame.*`): `StepStart` and `Step` (Update's state machine, fades, wakes and layout) and `Draw` /
`DrawUnder` (the pass and the backing under the game's 3D views) take the host's clocks, what was read for them
(`FrameIn`, `DrawIn`) and the state they carry from frame to frame (`FrameState`, the parts' state in it), bind the
parts to them, and return what they ask of the host (`StartOut`, `StepOut`). All they take in and carry is written and
read as text by one list per struct (`hud2_state_io.h`: `Fields`, a JSON state file; pins on each struct's size and
member count fail the build of any program that includes it - the unit build and, through the recorder, the DLL - when a
struct changes and its list does not). The test verb `hud2 bench record` (`modules\hud\src\hud2_record.*`, host code)
writes live frames in that form: per frame the state Step took and the one Draw took with their inputs, the host's
answers each used (a tape of the clocks, the typing state and the hovered hotspot), the kit's own state (its text sizes,
the characters it added, the icons' readiness), and the display list the kit submitted (`core\ui\hud2_kit_fldl.*`, the
same writer the design bench uses), tied by fingerprints: what a program with no game needs to step and draw that frame
again (`tests\bench_record.ps1` records HUD 2's 25 simulated states). The design bench does that (`flbench replay`,
`tools\hudbench\replay.*`, B5a): Step and Draw from the record, every host call answered from the tape in call order (a
tape that runs short or is left over fails), and its lists compared with the game's (`tests\bench_level1.ps1`, no game;
`tools\hudbench\README.md`). The game's side is `hud2.cpp`: it reads
the game for the frame, refreshes the model (`hud2_data.cpp`), runs the native HUD switch (`hudvis`), the HUD bus, the
hotspots (`hud2_click.cpp`: the set to the controller host, the clicks through the game's commands) and the give-back,
and opens and closes the kit's passes around `Draw`. What core's frame tells HUD 2 each
frame (`views::HudContext`) is in `core\ui\hud_context.h`, which includes no game header.

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

## The two interfaces
The player picks one of two interfaces (`hud_style`): **HUD 2**, FLvibed's own (the HUD module), or the **classic HUD**, the game's own
interface with the Controller's pad layer on it. The two are never mixed; the direction from here on (the owner, 13 Nov 2026, decision 77; which of
today's touches on the classic HUD stay: his answers below): the classic HUD is the game's own native HUD (or what a third-party mod makes of it), untouched beyond what other modules
need to function (the controller, the autopilot, ...), and HUD 2 stands on its own. A change to one must not change the other, so every source of `core\` and `modules\`
belongs to one zone, listed in `tools\hud_zones.tsv` (the row with the longest matching path wins). `core\game\gamedata.*` and
`equip_rules.h` moved from `hud2` to `shared` on 12 Nov 2026 (audit 78_): plain readers of the game's data, they serve the
screen models of both interfaces (the dealers' probe first).

**What FLvibed does on the classic HUD** (the owner's answers of 13 Nov 2026 to the cards of the audit's un-mixing inventory, `102_`; each
with the switch that turns it off):
- The room characters' name card: the pad moves the game's card beside the character, clear of its prompts; with keyboard and mouse it stays
  where the game puts it (`core\ui\winplates.cpp` ClassicPass, `core\ui\gamecard.*`).
- The game's hover help in flight: with the pad it shows only after a real mouse move (the pad's aim rests the cursor on the HUD's buttons);
  with keyboard and mouse the game's own (`core\ui\hud_door.cpp` UnderBegin, the extras' rule).
- The shield, hull and reticle cues: off by default, on with `classic_cues`, then each by its own switch; `game_text_size` AUTOMATIC is the
  game's own size there, a size the player chooses applies (`modules\hud\src\extras\extras.cpp`, `core\game\hudsize.h`).
  The options page's HUD 2 switch turns HUD 2 alone off (`hud_style` 0; the HUD module keeps running, so these stay reachable:
  `core\ui\options_rules.h` PlanHead, the owner's answer to card N11); the module has its own row.
- The pad's layer on the game's HUD, kept whole ("Keep all four"), each on by default with its own switch: the glyph badges on the HUD's
  buttons (`hud_glyphs`), the button legend at the right (`button_legend`), the status line under the maneuver bar (`ap_status_line`),
  the prompts by a target (`world_prompts`) (`modules\controller\src\impl\padhud.cpp`); the prompt bar, the cursor and the pad's screens
  are the controller's own and stay.
- What the subtitles move in flight, kept ("Keep the moves"), each by the switch that makes its room: the top panel (`space_position` 2,
  the default) moves the maneuver bar to the top left, the comm window below it and the objective banner below the panel; 1 and 0 move
  neither (`modules\subtitles\src\impl\display.cpp` PlaceManeuverBar, KeepBannerClear); the comm window's slot in the panel
  (`comm_portrait`; off: the game's place, below the bar when the bar moved); the off-screen target arrows kept clear (`hud_arrows`).
- The maneuver bar's fifth button, the autopilot's mouse entry, kept ("Keep it"): `maneuver_button` (on; off also narrows the bar's frame
  back to four buttons) (`core\host\uibtn.*`, `modules\autopilot\src\barbutton.*`); the FLVIBED button in the game's Options and the
  pause menu's sixth button (every module's settings), the goal panel and the idle cursor's hide (the owner's decisions 62, 76) stay too.

| Zone | What | For example |
|---|---|---|
| `hud2` | HUD 2's own code | `modules\hud\src\` except the files named below; the data HUD 2's target display reads (`core\game\indicators.h`) |
| `shared` | code that runs under both interfaces | core's base, game layer and UI kit; the pad's screens and prompt bar; the extras kept on both (the shield and reticle cues, `game_text_size`, the idle cursor's hide, the hover help's rule: `modules\hud\src\extras\`) and the HUD module's table (`hud_module.cpp`); the subtitles' display; the goal panel |
| `classic` | code that runs or draws only while the game's HUD is up | the pad layer on the game's HUD (`modules\controller\src\impl\padhud.cpp`) |
| `door` | the door between core and HUD 2 | `core\ui\hud_door.*` |

**The door.** Core reaches HUD 2 only through `core\ui\hud_door.h` (namespace `huddoor`): core's frame and passes
(`core\game\coreframe.cpp`), the options page and the window plates call its entries, never `views::hud` or `views::skin`
themselves. The door decides which interface is selected, once a frame at the top of core's frame (`huddoor::Frame`): HUD 2 when
the HUD module runs (`huddoor::Present()`, its slot, never its views) and `hud_style` is not 0 (its two states, 0 and 2, the same on
both devices: an older release's 1 reads as 2, `core\ui\hud_rows.h` StyleOf; a device change is not a switch). The module registers two entries with the door at its Attach
(`huddoor::Register`, `modules\hud\src\hud2_entry.cpp`): **Enter**, on the first frame HUD 2 is selected, publishes its views, the
loading screen's draw and the idle cursor's feed; **Leave**, from the first classic frame on, withdraws them and gives back
everything HUD 2 changed in the game through one list it shares with the module's Shutdown and Fault (`hud2entry::GiveBack`: the
game's panels, windows, text, cursor, click box, the HUD bus's layout and the comm window), over two frames when a base page
stands, whose window closes only after its text went back. While the classic HUD is selected no HUD 2 code runs: HUD 2's views
are empty, so every entry of the door and every view the other modules call answers as with the HUD module off, and the module's
raw key passes to the game. A load runs inside one game frame with no core frame in it, so the door decides each of its frames
itself (its load draw, registered with the host while the module runs): HUD 2's loading screen only while HUD 2 is selected, the
game's own from the frame `hud_style` turns 0 to the load's end (the owner's decision 63: no frame mixes the two). The door's classic under pass is the classic pass (`winplates::ClassicPass`) and the extras over it.
**HUD 2's hooks** (audit R4c-2, 13 Nov 2026): only three hooks are HUD 2's alone, the loading screen's draw (`exe::UiBatchClose` at
`exe::LoadBracketCall`) and the target label's mute (`exe::HudVertex`, `exe::HudIndicatorDraw` and the label text's draw slot,
`uihooks::InstallTargetMute`) and the map's zone build and palette (`exe::NavMapZoneBuild`, `exe::NavMapPalette`, `uihooks::InstallMapZones`). None is installed at the start: the door's first Enter in a session asks the host for them
(`hostcalls::g.InstallHud2Hooks` -> `router::InstallHud2Hooks`, from the logic's frame on the game thread, where neither target is
on the stack); once in they stay and pass through on the classic HUD (no load draw, no mute wanted). So a session that never selects
HUD 2 runs the game's loading screen and target indicator unhooked. Everything else stays installed at the start as the shared base,
because a second module or the classic HUD needs it: `uihooks::Install` (the key handler, the control registry of
`core\ui\winplates.*`, the lock answers), `uihooks::InstallHits` (the shield cue, the autopilot), `uibtn`'s click box and buttons
(the options page, the goal panel, the autopilot), the router's frame, comm, cursor, maneuver layout, character, D3D and UI-render
hooks (the classic pass among them), `input`'s hooks and the tractor hook.
The door counts every call into the HUD module made while the classic HUD is selected, from core and from the other modules
alike, the key and the idle cursor's feed included (`modslots::g_viewCalls`, less the extras' and the door's own Enter and
Leave): the debug verb `huddoor`, which `tests\hud_modules.ps1 -Row classic` requires to read 0. The other modules still reach
HUD 2 through `core\ui\views.h` themselves, counted below; their panels move behind the door later.

What leaving HUD 2 no longer carries over to the classic HUD (audit R4c-1, 13 Nov 2026): HUD 2's 120 ms fade-out in flight; the
HUD bus's layout and the comm window are given back at once, docked too; the reticle ring's radius and the page state the pad
reads give their classic answers (before, the ring radius stayed HUD 2's last after it had been used); a HUD 2 click, wake or map
focus queued at the switch is dropped; a base page's window closes and its rows get their names back; the test aids `uiskin hover`,
`modelprobe` and `hud2 on` do nothing on the classic HUD; the debug `layout sim` draws there in space (no layout from HUD 2 to
yield to); going back to HUD 2 in space arms its alerts 1.5 s later, as a fresh flight does; a CONTINUE that has not reached the
game's load screen is dropped (the game's own Load Game screen stays); an Enter key held across the switch gives its key-up to
the game.

**The extras.** The HUD module holds two entries side by side. The extras' (`modules\hud\src\extras\`: the shield and hull cue, the
reticle cue, `game_text_size`, the idle cursor's hide; zone `shared`) publish `views::extras` and include no file of HUD 2's. HUD 2's
(`modules\hud\src\hud2_entry.*`, zone `hud2`) publishes `views::hud` and `views::skin` at the door's Enter, and tells the idle
cursor's hide when HUD 2 lets the flight HUD rest (`extras::SetCursorFeed`, from Enter to Leave). `hud_module.cpp` is the module's table and calls both. With the module off,
neither runs (`[Modules] hud=0`). The door calls the extras through `views::extras` and does not count them as calls into HUD 2.
The door's under pass keeps the order of the passes as it was: first HUD 2's glass, then the shield cue. The overlay draws the cues
first and HUD 2's panels over them.

Since R4c-1 no HUD 2 code runs with the classic HUD selected (above). Three rules keep it so, all in `tools\module_lint.py --enforce`:

**Core reaches HUD 2 only through the door.** A use (below) in a `core\` file outside the door fails.

**The door's own uses may only fall** (`HUD2_DOOR_USES`, 21 at its first commit, 18 once the extras had their own view: the
cues, the game's text size and the idle cursor are not HUD 2's; 16 once it decided the interface itself: no `views::HudPresent()`,
and no device-lost entry, the draw kit's being core's since F2), counted like the uses from outside HUD 2 but apart from them.

**The uses of HUD 2 from outside it may only fall** (`HUD2_USES`; 99 before the door, 73 after: the 26 uses of core's
frame, the logic's entry, the options page and the window plates went behind it; 57 once the module's table stopped
reaching HUD 2 itself: its 17 uses moved into `hud2_entry.cpp`, in the `hud2` zone, and its one include of
`hud2_entry.h` is counted: 73 - 17 + 1). The lint counts every place outside the `hud2` zone and the door that reaches HUD 2: a call
through the HUD's or the skin's view (`views::hud.<entry>(`, `views::skin.<entry>(`, `views::Hud<name>(`, `views::Skin<name>(`), a call
of HUD 2's half of the HUD bus (`ReadLayout`, `PublishLayout`), an include of a `hud2` file, a name from a `hud2` file of core, and a read
of `hud_style` or `ui_style`. Comments, strings, a null test before a call and the views' data types do not count. The count is compared
with `HUD2_USES` in the script: a rise fails (`--list` names each use); a fall fails too until that number is lowered in the same commit,
so it cannot climb back. A `classic` file includes nothing of HUD 2, its draw kit `core\ui\hud2_kit.h` included. The counter reads
names line by line, so a reviewer still reads `--list` for what it cannot see: a namespace alias, a view held under another name, a
function pointer to an entry, a call split across lines, a new wrapper in `core\ui\views.h` not named `Hud*` or `Skin*`, and a change
that removes one use and adds another.

**A change to shared code runs the classic checks.** Each zone row names the checks a change there needs (`CHECKS` in the script: the
classic HUD's frame set, `tests\pixels.ps1` with and without `-Classic`, the window and dealer walks, the sweep's sections that see the
classic HUD). `module_lint.py --changed <files>`, `--staged` or `--range <a>..<b>` prints them for a change. A `hud2` file names
none since R4c-1 (`HUD2_STANDDOWN` empty): it no longer runs under the classic HUD. A new file takes the zone of the longest row that matches its path:
`core\`, its subfolders and every module folder have a row, so a new file there is `shared` (in most of them with no classic check)
until a row of its own says otherwise; give it one when it is HUD 2's, classic only, or draws. Only a source no row reaches, such as one
in a new module folder, fails `--enforce`.

**The page framework** (HUD 2's full-screen pages, being built from 12 Nov 2026). A page is files, never a row in a shared table:
its description and view in `modules\hud\src\pages\<family>\` (zone `hud2`; the families `dealers`, `neuronet`, `menus`, `map`,
`mp`, each a folder `build.cmd` compiles), registered from its own file (`PAGE_REGISTER`, `pages\pagelist.h`); its model, its pure
rules and its actions' table in `core\game\screens\` (zone `shared`: nothing there draws or includes a module's header; its headers
reach every module through `core_all.h`). The runtime (`pages\pagerun.*`) claims the game's window, parks it, refreshes the model and
runs every action through one runner (`core\game\screens\action.h`). The rest of HUD 2 (the skin, the game's text, the strip, the
HUD's roots) asks the runtime whether a page stands, which game window it is and whether its controls are off the screen, and draws
the pages through its hooks; the pages not on the runtime yet (`nnpage.cpp`, `basepage.cpp`) answer through entries they register
themselves (`pages\pagelist.h` LegacyPage), so no shared file names a page's file. The lint holds four rules of its own: a view
(`pages\...\<name>_view.cpp`) includes only the draw kits (`page_view.h`, `page_kit.h`, `core\ui\hud2_kit.h`) and a screen's model
(`<window>_model.h`), and `build.cmd test` compiles each view alone. The draw kit draws and records, never reads: a view gets its box and
the hovered box in `ViewIn` and gives its clickable boxes back as records (`ViewOut` hits) that whoever runs the page hands to the mouse;
what reads the game for the same pieces (the frame's place, a hole round the game's text, the compare's numbers) and the click box
of the pages not yet moved is `nnpage_kit.h`, which includes the kit. A file name is unique in its tree (the wrappers are named by a
file's stem); a page calls the game only through its screen's table (`exe::`, `hk::Call`, a click or scroll of a game control and a
screen message are refused in `pages\`, the runtime's park aside); and the calls that work the game's own window controls from the HUD
module and the screens (`PAGES_BORROWED`) may only fall. Each family's unit tests, `core\flexe.h` symbols and `tests\main.cpp` rows go
under its own `---- screens: <family>` / `---- pages: <family>` line, so families work side by side.
Until the pages have left `nnpage.cpp` and `basepage.cpp` (audit 70b F3 / F4) those two files are **shells**: each decides which of its
pages stands and calls the runtime for the pages already moved (`Keep` / `Frame` / `Draw` / `Prompts` / `Pad`); each shell's `Keep`
releases only the page it opened. A moved docked page (`pages\dealers\repair_*`, `prices_*`, `market_*`, `shiptrade_*`; audit 108_ "What a
moved base page looks like"; the ship dealer's 3D view stays the game's, brought back to the screen's middle by the shell after the park) keeps its window parked by its own setter and never given back (`Park::Away`: the window closes by its own close button
when HUD 2 leaves; the dealer's partner NN_Inventory the same way, `pagerun::Frame`'s `partner`); its model holds what the window and the
ship show plus the page's focus (reset on every open, as the old statics were), read each frame because the window's texts change with no
event, and what the view shows of the game read again right before it draws (`PageInput::Prepare`); its prompts and pad are its own
(`page::PageInput`, `pagerun.h`), because words that change, holds with their fill and the triggers and sticks do not fit the bindings yet
(the docked pages share one A hold, `pagerun::PurchaseHold`); its view's hit boxes, icon sets, game lists and holes are records the
runtime hands to the click box, moves the game's icons by (a game list's five item meshes: `ViewOut::lists`, their models sent in
`pagerun::IconsUnder`) and draws the ground in; the docked pages share one ground's holes (`pagerun::DrawStart`).
The job board and the news (`pages\neuronet\board_*`, one body in two modes, two descriptions `jobs` / `news`; their reads and actions
`core\game\screens\board_*`) are `Park::Hosted` docked pages: their window stays where it stands (its picture beside it), the shell
reflows its text into the panel's column and moves its other controls off the screen before the page reads it (`basepage.cpp`
BoardPlace) and runs the news's ticker (`boardscreen::Advance`).
A moved NeuroNet page over a window that stays on the screen (LOG and the death page, `pages\neuronet\mlog_*`, `death_*`; STATUS,
`pages\dealers\status_*`; audit 110_) is `Park::Hosted`: the game's journal (STATUS: the faction's card) shows through the page's hole,
which the shell cuts as before and hands in `ViewIn` (the HUD
module's windowtext places that text after the frame's update, so the hole is read at the draw); what the page read of the game while it
drew is read again right before its view (`PageInput::Prepare`), never a frame early in the update.
INVENTORY (`pages\dealers\inventory_*`, its reads and actions `core\game\screens\inventory_*`; audit 136_) is the same, with three
differences: its frame's reads keep their clocks in frame time (`PageInput::Tick`, in place of the wheel's hook); the game's controls
it draws round (the jettison panel's slider and buttons, the number field, BEST PRICES' hidden button and blank label) are still
placed by its shell, nnpage.cpp, before the view, since the placing is the HUD module's; and its game list is an `atSend` list
(`GameList::atSend`): nothing given or placed at draw time, the rows' meshes given, placed and sent under the next UI pass only, the
view cutting its own holes from what that pass gave (`ViewIn::sent`); its jettison panel, a state of the page, is a list of one fixed row.

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

Targets: `dev` (default), `impl` (the logic alone), `release`, `test` (the host-free unit tests in `tests\`, seconds, no game), `sigtool`, `setup`, `flhook` (the server plugin), `mptest` (the local test server's tools), `spawnsim` (the offline encounter spawn simulator `tools\spawnsim\`: the host-free core `core\game\spawnsim.*` and its ini loader `spawnsim_ini.*`, GitHub #61; the same core runs in the logic DLL for HUD 2 map's influence cells: `core\game\influence.*` the field, `modules\hud\src\mapcells.*` its worker thread and draw; `flspawnsim field` prints a system's field offline).

## Rules the code keeps
- **No raw addresses.** Every hooked, called, read or compared Freelancer.exe address is a symbol in `core\flexe.h` with its signature.
  The host and the logic each verify the table at startup. A mismatch fails closed: that hook, call or read is refused and logged, and
  only its feature stops. The verified facts behind the symbols are in [FREELANCER_RE.md](FREELANCER_RE.md).
- **Hooks chain.** Detours follow an existing jump, and vtable and import patches call the previous pointer. Other plugins may hook the
  same address: never take one exclusively, never change game logic.
- **Reload.** The host and the logic match exactly: the loader loads only a logic built with the host's interface number
  (`kLogicApiVersion` in `core\loader.h`), and refuses any other with the log line "logic: not loaded: ... built for interface N, this
  FLvibed.dll has M (restart the game to load it)", so its modules stay off until a restart. Bump that number with every change to what
  crosses between host and logic (a module's `HostApi` or `ImplApi`, `core\logic\core_api.h`, the buses, `core\game\hostcalls.h`, a
  shared block, any host type the logic reads). A new host function is then simply a new field, called with no version check; a hot
  reload works while the host is unchanged, and a host change means a restart. Logic-side patches, window classes and threads need a
  teardown, because a reload frees that code.
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
5. List its files, settings and verbs in `docs\module_map.tsv`, its folder in `FOLDER` of `tools\module_lint.py` and its zone in `tools\hud_zones.tsv`, add a test that includes its
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
| `A37`, `A38`, `T<n>`, `U<n>`, `D<n>` | an idea's id in [research/IDEAS.md](research/IDEAS.md) (T11 and T14-T35, on the Claude Code environment, are in a private process document); `A37` is the situation assessor and arbiter, planned in the A37 plan (a private development plan) |
| `P<n>` | a package of the split into modules, from the module split plan (a private development plan; this page is its public view) |
| the private handoff, work queue, harness notes, session hooks | files of the development repository that the public export leaves out |
