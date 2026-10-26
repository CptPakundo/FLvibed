# Menu screens on the pad (saves, loads, text entry, speedrun mode)

What the game's menu screens are made of and how FLvibed drives them natively from a pad (D-MENUS, 25 Oct 2026;
engine facts in `..\FREELANCER_RE.md` "Main menu, load screens, pause menu"; code `controller\src\impl\padmenus.cpp`,
`newgame.cpp`, `osk.cpp`, `livesplit.cpp`, `runtimer.cpp`).

- **Load / save lists** `[game]`: a row is selected natively through the list's own fields (the hovered row +0x7C, the
  selected row +0x8AC / the in-game screen's +0xE88) and loaded through `menu::Load` (the game's load-by-index); the
  virtual mouse cannot click out of focus, so the pad's single A on a row is a native select + load. Delete = a one-second
  X hold, then the game's DELETE / `LoadSave_Delete` button, which asks in `NN_Confirm`.
- **Save description** `[game]`: the game's edit (`LoadSave_Edit2`, 30 characters) receives text through the game's own
  WM_CHAR path, so FLvibed pre-fills "<location> - <date>" (the base's name from universe.ini `strid_name`, "Planet " dropped
  when the limit cuts it) and the on-screen keyboard types into it; Y opens the keyboard, A clicks SAVE GAME.
- **Chat** `[game]`: `CTID` is the edit itself; the mode stays `space`; the keyboard's Menu sends Enter.
- **The confirmation `NN_Confirm`** `[game, 25 Oct 2026, PADFIX]`: a child of `NN_Pause` (the quit question) - the pause
  boxes stay visible behind it, so a screen detector keyed on the pause menu reads it as the pause menu; it is known by
  `Confirm_delBtnYes` / `Confirm_delBtnNo` (both hit-testable, Yes left, No right). The pad: focus on NO first, A = the
  focused answer's own button (natively: the free cursor's click does not land out of focus), B = `Confirm_delBtnNo`;
  YES on the quit question goes to the main menu (`mode docked -> menu`), NO back to the pause menu.
- **The game's arrow on the pad** `[FLvibed, 25 Oct 2026, PADFIX]`: on the main menu, load, save, pause, confirm and death
  screens as in bases, the arrow hides while the pad drives the focus (a D-pad / bumper press, or `pad_cursor 0`) and
  shows on a stick move (`pad_cursor` > 0), through the host's `SetCursorHidden` (HostApi v20); keyboard and mouse - or
  any frame the pad layer does not run - show it again (`padbases::Overlay`). `bases` reports `arrowHidden` / `menuScreen`.
- **The session's mode per save** (NORMAL / ASSISTED / SPEEDRUN) `[FLvibed]`: never in a game file - `EXE\FLvibed\saves.ini`
  maps save file names to the mode (+ the speedrun clock and checkpoint index); every save the game writes during a
  session is tagged (folder change notification), the loaded save's line sets the session; the load screens draw the tag.
- **Speedrun mode** `[FLvibed]`: the clock (game time, menus excluded), the split detector over `docs\checkpoints.tsv`,
  LiveSplit Server over TCP and the `FLvibedSplits` struct: `..\livesplit\README.md`.
