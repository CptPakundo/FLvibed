# Contributing to FLvibed

Thanks for looking. FLvibed is one DLL for Freelancer: HD Edition 0.7.1 and the original Freelancer 1.1: controller
support, subtitles, HUD 2, movies and the autopilot, each a module that works alone and together. Pull requests are
welcome; the owner reviews each one.

## 1. Reports
Bug reports and ideas are the most useful contribution: open an issue (the bug form asks for what helps) with what you
did, what you saw and what you expected, your game (HD Edition 0.7.1 or Freelancer 1.1), your device (pad model,
keyboard and mouse, Steam Deck), your resolution and window mode, and `EXE/FLController.log` from the game folder.

## 2. Building
- Windows, Visual Studio with the C++ desktop tools, and the MSVC v143 **14.44** toolset (an individual component in
  the Visual Studio Installer). Newer toolsets no longer target Windows 7, which FLvibed still supports, so
  `build.cmd` pins 14.44 and refuses to build without it.
- On a fresh clone, once: `tools\sdl3\fetch_sdl3.ps1` (PowerShell) fetches the pinned SDL3.
- `build.cmd release` builds `build\release\FLvibed.dll`, what players get. `build.cmd test` builds and runs the
  host-free unit tests in seconds, without the game.
- To try it: copy the DLL into your game's `EXE\` and add `FLvibed.dll` to `EXE\dacom.ini` `[Libraries]` (README,
  "Install by hand").

## 3. Where things are
- `docs/ARCHITECTURE.md`: the layers, the modules, how they talk, and how to add a module. Read it before a change
  that crosses a module boundary; `tools/module_lint.py --enforce` (any Python 3) holds the boundaries.
- `core/flexe.h`: every Freelancer.exe address, each checked by a signature at start-up (never a raw address in code).
- `docs/FREELANCER_RE.md` and `docs/kb/`: verified facts about how the game works.
- `docs/TESTING.md` and `docs/HARNESS.md`: the testing rules and the in-game test harness.
- `docs/COMMENTS.md`: how the code is commented.

## 4. A pull request
- One change per pull request; a restructure separate from a behaviour change.
- `build.cmd test` passes and `build.cmd release` builds without new warnings.
- Say how you tested it in game (HD Edition or 1.1, device, resolution).
- Never commit game files, audio, extracted game data beyond what the code needs, or disassembly. Subtitle text comes
  only from `subtitles/review/` (its tools publish the data files through a pre-commit hook, installed once per clone
  by `subtitles/review/tools/install-hooks.ps1`); the copies under `modules/subtitles/data/` are never edited by hand.
- Contributions are licensed GPL-3.0, like the project.

## 5. How this project is made
Made with Claude (Anthropic) under the owner's direction; contributions are reviewed by the owner.
