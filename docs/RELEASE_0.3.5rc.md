# FLvibed 0.3.5rc: release notes

**This is a test build.** It holds everything meant for 0.3.5, but it has not been through the full regression sweep and gate yet. Those run on this code next, and 0.3.5 follows them. Expect bugs, and please report what you find.

## What it is
- **FLvibed rebuilt as a core with eight modules** you can turn on and off: HUD, Controller, Subtitles, Movies, Autopilot, NPC, Display and Speedrun. Nothing changes on an existing install.
- **PlayStation and Switch pads work natively,** with no setting and no mapper. Steam Input's or a mapper's output for that pad is ignored, so each press acts once. To hand the pad to them, turn off **Read PlayStation and Switch pads natively** (Options, Controller, Input). A Steam Deck or Steam Controller is not touched: Steam Input is how those work.
- **Every bug fix from a complete audit of the project,** and the reports since 0.3.0. The ship dealer no longer sometimes opens on its final trade stage. The job board's tooltip no longer flickers with the pad. Button prompts stay for as long as a window or menu is open, and HUD 2's face buttons wear their platform's colours. Vibration defaults to 100 %: a stored 60 %, the old default, moves to 100 % once at the first start, and any other value stays; to set it back, Options, Controller, Rumble. The full list is in the [changelog](../CHANGELOG.md).

## Windows 7
A tester's Windows 7 needed VxKex to run 0.3.0. This build uses a Microsoft compiler that still targets Windows 7, so it is **expected to work without VxKex, but this is not confirmed on a real Windows 7**. If you try it, please say what happens, and attach `EXE\FLController.log` if it starts.

## Known limits
- **Real pads:** played on a real DualSense Edge on a cable, with Steam open: flight, guns, the pause menu and the job board, each press acting once. Other PlayStation pads, a Switch pad, Bluetooth, and a real Xbox pad beside a PlayStation pad have only been tested with virtual pads or not at all.
- **Steam Input:** a run with Steam's mapping active and proven to be dropped has not been seen: nothing doubled in that session, but nothing was caught either.
- **Steam Input and mappers:** what they send while the pad rests still reaches the game (Steam's gyro, macros). A mapper that outputs through a virtual device of its own, or as a DualShock, is not seen: turn the row above off for those.
- **Bluetooth PlayStation pads:** rumble switches the pad to a mode other apps may not read, as in other games that rumble it, as far as is known. If another app sees a dead pad after playing over Bluetooth, hold the PS button for about 10 seconds to turn the pad off.
- **FLUF:** not re-checked after these fixes.
- **Speed:** performance has not been re-measured since the split into modules.

## Checked for this build
Unit tests; the module matrix, every module off and alone; one start each of flight, a dock, a subtitle and a movie; the pad's own tests; the Windows 7 import check. The full sweep and gate are still to come.
