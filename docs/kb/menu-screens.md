# Menu screens on the pad (saves, loads, text entry, speedrun mode)

What the game's menu screens are made of and how FLvibed drives them natively from a pad (D-MENUS, 25 Oct 2026;
engine facts in `..\FREELANCER_RE.md` "Main menu, load screens, pause menu"; code `modules\controller\src\impl\padmenus.cpp`,
`newgame.cpp`, `osk.cpp`, `livesplit.cpp`, `runtimer.cpp`).

- **Load / save lists** `[game]`: a row is selected natively through the list's own fields (the hovered row +0x7C, the
  selected row +0x8AC / the in-game screen's +0xE88) and loaded through `menu::Load` (the game's load-by-index); the
  virtual mouse cannot click out of focus, so the pad's single A on a row is a native select + load. Delete = a press of X
  into the game's DELETE / `LoadSave_Delete` button, which asks in `NN_Confirm`. In game the row is selected by the window's
  own notify `vt[0x5C](0xF001, 0, row)` (load and save mode): it enables DELETE (+0x5D6); a write of +0xE88 alone left DELETE
  grey and X did nothing `[game, 15 Nov 2026, audit 139_ A]` (`..\FREELANCER_RE.md` "In-game load/save screen").
- **The save lists at HUD 2's text size** `[game, 15 Nov 2026, audit 139_ B]`: the game draws a fixed 20 rows (16 when saving)
  at the font's line height, so the larger text ran the last rows past the panel; HUD 2 shows the rows that fit, trims the
  hover to them and keeps the game's scroll bar reaching every save (`modules\hud\src\skinlists.cpp`; facts
  `..\FREELANCER_RE.md` "The save lists' rows"). The classic HUD keeps the game's 20.
- **Save description** `[game]`: the game's edit (`LoadSave_Edit2`, 30 characters) receives text through the game's own
  WM_CHAR path, so FLvibed pre-fills "<location> - <date>" (the base's name from universe.ini `strid_name`, "Planet " dropped
  when the limit cuts it) and the on-screen keyboard types into it; Y opens the keyboard, A clicks SAVE GAME.
- **Chat** `[game]`: `CTID` is the edit itself; the mode stays `space`; the keyboard's Menu sends Enter.
- **The confirmation `NN_Confirm`** `[game, 25 Oct 2026, PADFIX]`: a child of `NN_Pause` (the quit question) - the pause
  boxes stay visible behind it, so a screen detector keyed on the pause menu reads it as the pause menu; it is known by
  `Confirm_delBtnYes` / `Confirm_delBtnNo` (both hit-testable, Yes left, No right). The pad: focus on NO first, A = the
  focused answer's own button (natively: the free cursor's click does not land out of focus), B = `Confirm_delBtnNo`;
  YES on the quit question goes to the main menu (`mode docked -> menu`), NO back to the pause menu.
  Being a child, its answers share the pause menu's top-level window, so the D-pad's modal filter by root kept the pause boxes
  in reach: Up / Down from the question walked onto RESUME, SAVE ... (6 of 11 stops, `[game + FLvibed, 14 Nov 2026]`, instance 9,
  SaveC701, 1280x800, the virtual pad). `padmenus::OutsideModalBox` keeps the walk inside the question's own window (the
  answers' parent): 0 of 11 at 1280x800 and 3840x2160; Up / Down now stay on the focused answer.
- **The multiplayer LAN / INTERNET prompt** `[game, 30 Oct 2026, found by the focus crawler]`: `PromptYes` (LAN),
  `PromptNo` (INTERNET), `PromptExit` run as a modal loop but do NOT take the game's keyboard focus (`exe::KeyFocus`), so
  the D-pad's modal test (`ui::ModalRoot`) misses it and the main menu's buttons behind it stayed D-pad targets - the
  focus walked off the prompt (A then did nothing). `padmenus::ModalRoot` names the prompt's window as the modal one.
  The game's EXIT confirmation (`ExitButtonBox0` / `1`) opens on a B / Escape on the main menu.
- **The keyboard on the quit and confirm boxes** `[game + FLvibed, 14 Nov 2026]`: the game ignores the arrow keys on `NN_Exit` and `NN_Confirm`; Escape on
  the quit question leaves the pause menu too (`..\FREELANCER_RE.md` "Keys on NN_Exit and NN_Confirm"). Under HUD 2's skin the arrows move the focus
  (the cursor onto the answer, as the pad's D-pad) and Enter takes it (`modules\hud\src\skindialog.cpp`).
- **The Options screen's rows** `[game, 30 Oct 2026]`: a page's rows (`opt_toggle N`, `opt_scroll N`) come up ~1.5-3 s
  after the screen opens; the 54-px sliders are found by the hit-test probe only (`uiitems` depth 0) and can be missed by
  its grid - the game's hit test at the cursor (`hit x y`) finds them.
- **The game's arrow on the pad** `[FLvibed, 25 Oct 2026, PADFIX]`: on the main menu, load, save, pause, confirm and death
  screens as in bases, the arrow hides while the pad drives the focus (a D-pad / bumper press, or `pad_cursor 0`) and
  shows on a stick move (`pad_cursor` > 0), through the host's `SetCursorHidden` (HostApi v20); keyboard and mouse - or
  any frame the pad layer does not run - show it again (`padbases::Overlay`). `bases` reports `arrowHidden` / `menuScreen`.
- **The session's mode per save** (NORMAL / ASSISTED / SPEEDRUN) `[FLvibed]`: never in a game file - `EXE\FLvibed\saves.ini`
  maps save file names to the mode (+ the speedrun clock and checkpoint index); every save the game writes during a
  session is tagged (folder change notification), the loaded save's line sets the session; the load screens draw the tag.
- **Speedrun mode** `[FLvibed]`: the clock (game time, menus excluded), the split detector over `docs\checkpoints.tsv`,
  LiveSplit Server over TCP and the `FLvibedSplits` struct: `..\livesplit\README.md`.
