# Freelancer.exe notes, part 2: the screens and the options page, multiplayer, missions, voices, the docked pages

Documents diet part 2 (15 Nov 2026): `FREELANCER_RE.md` was over its size limit. Its sections from "Options screen `NN_Preferences`" on
moved here word for word, in the same order; `FREELANCER_RE.md` keeps a list of their headings, so a reference by section name still leads here.
The introduction of `FREELANCER_RE.md` (image base, how addresses are read, the credits) applies to this file as well.

## Options screen `NN_Preferences` (verified in game 25 Oct 2026, HDE 0.7.1, 4K 16:9; screenshots `research\experience\ui_2026-10-25\`)
- One class for both entries (the main menu's Options and the pause menu's code 4). Page buttons are icon buttons
  (vt 0x5E15F4) named `interfaceButton` (GENERAL), `keymapButton` (CONTROLS), `VideoButton` (PERFORMANCE),
  `AudioButton` (AUDIO), `CreditsButton` (main menu only), `MainMenuButton` (MAIN MENU / in game RETURN TO GAME); the
  selected page's caption is yellow. **Main menu**: centres x 996, y 492 + 238 i (i = 0..4), MainMenuButton y 1920, half
  364 x 73: **one free slot at y 1682**. **In game**: x 1080, y 506 + 216 i (i = 0..3), MainMenuButton y 1800: **two free
  slots (1369, 1585)**; a close X `PreferencesCloseButton` (3124, 433). Page title centred at x ~2340 (y 346 / 432 in
  game). Rows: label x 1619, pitch 98 px; `opt_toggle N` (vt 0x5E1764 ToggleButtonControl, centre x 3082, half 42, with
  ENABLED / DISABLED text ending x ~3010), `opt_scroll N` (vt 0x5E55AC ScrollControl, centre x 2781, half 351 x 27, a
  value caption to its left: 0.30 / Average / 120), the resolution picker (a box x 2458..3140 with up / down arrows);
  `opt_set_defaults_btn` (1878, 1906) and `opt_cancel_btn` (2763, 1906) (MMOk text buttons vt 0x5E614C). The Controls
  page: three sub-tab buttons across the top (SHIP CONTROLS 1655..2029 x 503..586, USER INTERFACE, MULTIPLAYER) over a
  PRIMARY / SECONDARY key table. Rollover help: one small Arial line at the top right of the screen ("Activates cruise
  engines."). Semantics: no OK / Apply - values apply as they change; CANCEL reverts; DEFAULT resets the page.
- **The maneuver bar is built in code from `hud.ini [Maneuvers]`** (read 25 Oct 2026 in the disassembly, 0x4D7B50..):
  the frame = the static-control factory 0x5A0DB0 with name "ManeuverFrame" (0x5CB68C) and its mesh (0x5D81E0); then
  one icon button per record through the button factory **0x5A0C90**, named `ManeuverButton%d` (0x5D8430); the records
  are 0x148 bytes each at HUD_Maneuvers +0x380 (+0x138 the maneuver op: 2 Goto, 7 Dock, 10 Formation, 19 Free Flight;
  the lit / grey meshes), the button pointers at +0x36C + 4 i - **four slots, fixed**. The per-frame layout
  `exe::ManeuverLayout` 0x4D88F0 walks a constant table {0, 2, 1, 3}, sets each button's state from
  `IBehaviorManager::external_player_evaluate` and the current op (+0xC0), places the shown buttons at x = base -
  count / 2 x pitch + i x pitch (pitch = the float at 0x5D845C; vt[0xA8] msg 6 = set position, msg 0xC = lit, 0x33 =
  colour 0x679B94), then the tab / cruise button (0x4D8BF0). In space at 4K (bar moved top-left by the subtitles):
  `ManeuverButton0..3` centres x 189 + 224 i, y 85, half 78 at depth 0.5; `ManeuverCruiseButton` (189, 248) half 39.
  A fifth button is therefore not a data change: FLvibed creates its own control with the same factory, parents it to
  `HUD_Maneuvers`, places it in its `ManeuverLayout` observer after the game's pass, and routes its click.

### FLvibed's native buttons (verified in game 25 Oct 2026, HDE 0.7.1; code `core\host\uibtn.cpp`)
- **The icon button factory 0x5A0C90** (`exe::CreateIconButton`, `ret 3Ch`, `this` = the parent control; the argument
  list from the game's call sites 0x4D7C49 (maneuver buttons), 0x4A9C62 (page buttons) and FLUF's declaration):
  `FlControl* thiscall(parent, const char* name, const char* mesh, const char* mesh2, const char* clickSound, const
  char* action, const Vector* pos, const char* hardpoint, const Vector* mouseHalf, bool noMeshRender, bool actionButton,
  bool noColourChange, bool noZEnable, u32 tipIds, u32 tip2, u32 command)`. It is `CreateControl(name, "ButtonControl",
  parent)` (0x59DB20: the class registry [0x67DCEC], the name copied into +0x18, at most 31 chars) plus the button's
  Init 0x55AC30 with arguments 3..16: the mesh name is copied into +0xD0 and looked up in the mesh table [0x679A48]
  (0x24-byte entries, name at +4; loaded through vt[0x98] when missing), `clickSound` becomes the std::string at +0x360
  (its buffer pointer at +0x364) and the sound id at +0x374 (-1 = none; "ui_select_item" for the HUD and page buttons),
  `action` is parsed by 0x55A490 into the command +0x34C / hint +0xCC, `pos` -> +0xA8, `mouseHalf` -> +0xB4 (the hit
  half extents: the maneuver buttons 0.014 x 0.014 at depth 0.5 = 78 px at 2160p; the page buttons 0.13 x 0.013),
  `tipIds` -> +0xCC, `tip2` -> +0x32C, `command` -> +0x34C (the maneuver buttons: their maneuver's command; the cruise
  button 0x45), the bools into +0x330 / +0x331 bits (actionButton = +0x331 bit 0; the maneuver buttons pass 1, the page
  buttons 0). A null mesh is fine (no mesh at all: the control is hit-testable and invisible). The maneuver records are
  0x148 bytes: the mesh name string at +0x28, the grey mesh at +0xAC, the tip ids at +0x12C, the command at +0x134,
  the op at +0x138. The mesh name is relative to DATA, and the loader resolves `..`: **a mesh outside DATA loads**
  (`..\EXE\FLvibed\autopilot.3db`, verified with a copy of `mnvr_goto.3db` and with FLvibed's own file).
- **HUD_Maneuvers** is a HUD part object whose window (the FlControl named "HUD_Maneuvers", class vtable 0x5D8344 =
  `exe::VtManeuvers`) sits at part + 0x38; `ManeuverLayout` 0x4D88F0 takes the part, the window's own methods take the
  window: vt[9] = the builder 0x4D7B50 (InitElements), vt[0xA8] = `exe::ManeuversMessage` 0x4D8DA0. Window-relative:
  the four buttons +0x334 + 4 i, the records vector +0x348 / +0x34C, the frame-size cache +0x37C, the frame +0x380,
  the tab +0x384, the cruise button +0x38C (part-relative = + 0x38: +0x36C, +0x380, +0x3B4, +0x3B8, +0x3BC, +0x3C4).
  The message handler acts on 0x26 only: it finds the button among the four (+0x334) and runs its record's maneuver
  (0x4D9470); an id it does not know does nothing - so a fifth button's own action message is harmless, and FLvibed's
  detour on the handler takes 0x26 / 0x27 for its control first (the icon button sends 0x27 when its hover bit 0x80 is
  set, else runs vt[0xB4] -> 0x26: "Room buttons ignored" above).
- **The frame** (`ManeuverFrame`, a static image) is sized by `exe::ManeuverFrameSize` 0x4D95B0 `thiscall(part, int
  shown)` (-1 = count the applicable records itself), called by the layout every frame: it switches the mesh to
  `Interface\HUD\HUD_maneuverbox<n>.cmp` (n = 2..9, the table at 0x612654, 9 for more) when the count differs from the
  cache (+0x37C), through msg 4 (set mesh by name) and re-reads the mesh box into +0x3D0. The layout's x pattern for n
  shown buttons: `base(n) - n/2 x pitch + i x pitch` with base = the (HudShift-relocated) float of `ManeuverBarXRef`
  (0.02) for even n, 0.0 for odd, pitch = `exe::ManeuverPitch` 0.04 (224 px at 2160p: buttons and frame both at depth
  0.5); adding a button on the right therefore shifts a centred group left by half a pitch, so FLvibed instead keeps
  the game's four where they are, asks the sizer for n + 1 (a detour adds one when its button is shown) and moves the
  frame right by half a pitch (its position through msg 6; idempotent over HudShift's / the subtitles' rewrites).
  The tab (`ManeuverTab`, 0x4D8BF0) follows the current maneuver's button (its position read with msg 7, x doubled for
  depth 1) and the cruise button hangs under the tab (x - 0.032).
- **The control state byte +0x6C:** bit 1 (0x02) is what the UI pass tests - 0x59E300 draws every root with bit 1 set
  through its vt[0x34], and a window's draw skips children without it; bit 0 alone is the "listed / hit" bit (a text with
  bit 0 cleared and bit 1 set still draws: the Options page's labels stayed on screen until both were cleared). The
  game's own hide (`and 0xFC`) clears both; show = `or 3`. Bit 3 (0x08) = enabled (hit-testable).
- **NN_Preferences page switch:** vt[9] = `exe::PrefsInitElements` 0x4A9790 builds the screen; the page buttons sit at
  +0x34C keymapButton (page 0, CONTROLS), +0x350 interfaceButton (page 1, GENERAL), +0x354 VideoButton (2),
  +0x358 AudioButton (3), +0x35C CreditsButton (4, main menu only), +0x360 MainMenuButton, +0x348 PreferencesCloseButton
  (in game only); +0x32D = 1 in game (0 in the main menu); the title text ("Screen Title") +0x344; the captions are
  TextControls (`InterfaceText` +0x368, `keymapText`, `VideoText`, `AudioText`, `CreditsText`, `mainMenuText`); the
  current page +0x7F4, the page being switched to +0x7F8. The page buttons are made with mesh
  `Interface\NeuroNet\Inventory\FRONT_button.cmp`, x -0.30 (in game) / -0.33 (main menu), the first at y 0.205 / 0.21,
  pitch 0.077 / 0.085, mouse half 0.13 x 0.013, then 0x55BFC0(1) sets +0x335 = 1 (that is all it does). **+0x331 bit 1
  (0x02) set = the button takes the mouse:** its mouse handler 0x55B950 starts `test [ebx+331h],2 / je` (skips
  everything); the factory sets it (0x0E), the current page's button has it clear (the selected look, not clickable).
  Hover works without it (0x2C enter, 0x25 each hovered frame, 0x2D leave to the owner's vt[0xA8]); the click does not
  (verified 26 Oct 2026: FLvibed had it backwards and its FLVIBED page button took no mouse click on any install). The handler vt[0xA8] =
  `exe::PrefsMessage` 0x4AC0A0 (`u32 thiscall(msg, FlControl* id, int)`): in the normal state msg 0x26 compares `id`
  with the page buttons (0x4AD2A1..), sets +0x7F8, hides the old page through the per-page function (0x4AEAB0 /
  0x4AEBB0 / 0x4AECA0 / 0x4AED70; credits inline) and shows the new one through 0x4AEE30 (sounds 2 and 5, the per-page
  show 0x4AE4B0.. which also sets +0x7F4 and the title via 0x4347E0(ids) + msg 0x1D); msg 0x25 / 0x46 = hover enter /
  leave for the rollover help (0x4B2130), 0x29 / 0x3E the sliders and toggles, 0x4F the gamma. `exe::PrefsShowPage`
  0x4ABDF0 `thiscall(bool hide)` is the game's own hide (1) / show (0) of the current page's interactive controls (used
  around the key-capture overlay); it leaves the labels (`opt_text N`, `opt_titleText N`, `opt_toggleText N`,
  `opt_smallText N`) and `video_mode_*` alone, and the game re-shows the page's texts each frame - FLvibed's page
  switch therefore also clears bits 0|1 of every visible child of NN_Preferences except the furniture
  (`Options_BlackPlane`, `OptionsBox`, the page buttons and their captions, the close X) and re-clears them at the
  start of every UI pass; any other 0x26 (a page button, RETURN TO GAME / MAIN MENU, the X) restores first. The page
  content controls are named `opt_toggle N` / `opt_toggleText N`, `opt_scroll N` / `opt_smallText N`, `opt_text N`,
  `opt_titleText N`, `opt_set_defaults_btn` / `_txt`, `opt_cancel_btn` / `_txt`, `video_mode_cycle` / `_box` / `_box2`
  / `_text`. The free slot: after CREDITS in the main menu (y 1682 at 2160p), after AUDIO in game.
- **TextControl** (class vtable 0x5E4F24 `exe::VtTitleText`, the factory 0x5A0E30 `exe::CreateTextControl`, `ret 2Ch`:
  `thiscall(parent, name, fontNickname, const Vector* pos, int, float, float, float, u32 ids, int, int, int)` - the page
  captions pass ("ButtonText", pos, 0, 0.258, 0.015, 1.25, ids, 1, 0, 1), the pause captions (0, 0.27, 0.015, 1.5, 0, 1,
  0, 0)): vt[0xA8] = `exe::TextCtlSetProperty` 0x58B360, msg 0x1D = raw text (`const wchar_t*`), 0x1E = a string id,
  6 = the position; the caption colour at +0x348 (the globals 0x679B94 `UiColourSelected` = the selected page's yellow,
  0x679BB0 `UiColourNormal`). So an added button can carry a caption without a string id.
- **NN_Pause** (vtable 0x5D462C): vt[9] = `exe::PauseInitElements` 0x4A6BA0 builds `Pause_BlackPlane`, `PauseFrame`
  (`Interface\NeuroNet\Options\escapewindow2.cmp`), `PauseTitle`, then n boxes `PauseButtonBox%d` (the text / MMOk
  button class 0x5E614C through the factory 0x5A0D00 `exe::CreateWireControl`, `ret 24h`: `thiscall(parent, name,
  sound, const Vector* pos, const Vector* dims, hardpoint, int, u32 hint, int, int)` with no sound, dims (0.175, 0.014,
  0) -> +0xB4, the last box flagged +0x35D bit 0) at +0x338 + 4 i and captions `PauseButtonText%d` at +0x34C + 4 i
  (+0.005 above the box), action codes +0x360 + 4 i. The layout depends on the count: 4 boxes at y 0.1284 - 0.0963 i,
  5 (docked, with Save) at 0.149 - 0.0825 i - one more box moves the first up by 0.0206 and tightens the pitch by
  0.0138, and the same step once more (0.1696 - 0.0687 i) seats six inside the frame (a sixth simply appended fell
  below it). The handler vt[0xA8] = `exe::PauseMessage` 0x4A7130: 0x26 finds the box among the five (+0x338) and runs
  its code through 0x4A7210 (an unknown box does nothing); 0x42 = set position; 0x3006 = the Escape path (resume).
  Captions: msg 0x1E with ids 0x71D.. on the texts, msg 3 with 0x33A.. on the boxes (the rollover help).
- **The text / MMOk button class** (vtable 0x5E614C: `PauseButtonBox%d`, `opt_set_defaults_btn`, `opt_cancel_btn`):
  vt[0xA8] = `exe::TextButtonSetProperty` 0x5A4690 (id 0x2E its own, 6 = the position through ControlSetProperty).
- **Raw keys:** `exe::WinKey` 0x577850 `bool cdecl(UINT msg, WPARAM, LPARAM)` handles WM_KEYDOWN..WM_SYSKEYUP (a jump
  table on msg - 0x100): it stores the vk in 0x67C27C, the repeat flag 0x67C291 (bit 30 of lParam), the down flag
  0x67C290, the modifiers 0x67C280 from GetKeyState, then HandleKey(USER_KEY = 1 or USER_CHAR = 2, category) and, when
  not handled, the bound commands. FLvibed's detour tells the player's keys from its own injected ones (fl::SendKey calls
  the same entry from the logic DLL) by the return address: inside Freelancer.exe's image = the game's window procedure
  = a physical key. **Keyboard focus:** a window takes it with 0x577B40(&window + 0x32C) (NN_Pause at 0x4A701D); a text
  edit (class "UITextEdit", factory 0x5A1900) takes it with its handler at edit + 0x498 (0x57C750); HandleKey hands
  category-1 keys only to the handler that equals [0x67C3AC] when one holds the focus. So "a text field has the focus"
  = [0x67C3AC] - 0x498 is a live control.
- **Icon meshes** (`tools\mk3db.py`, verified 25 Oct 2026 with FLvibed's own `autopilot.3db` on the bar): a HUD icon
  `.3db` is a UTF with `VMeshPart\VMeshRef` (60 bytes: headerSize 0x3C, VMeshLibId, startVertex, vertexCount,
  startIndex, indexCount, startMesh, meshCount, bbox max/min xyz, centre, radius), `VMeshWire\VWireData`, `Material
  library\<name>\{Type "DcDtOcOt", Dt_name "<x>.tga", Dt_flags 0x40}` + `Material count`, `Texture library\<x>.tga\MIP0`
  (an uncompressed 32-bit TGA). The game's icons keep their vertices in the shared `DATA\INTERFACE\interface.generic.vms`
  (`VMeshLibrary\interface.generic-102.vms\VMeshData`: 479 meshes of 4 vertices, FVF 0x102); a self-contained 3db
  embeds its own `VMeshLibrary\<lib>.vms\VMeshData` (meshType 1, surfaceType 4, meshCount, indexCount, FVF 0x112 =
  XYZ|NORMAL|TEX1 32-byte vertices, vertexCount; per mesh {materialCrc, startVertex, endVertex, refVertexCount,
  0xCC}; the u16 indices; the vertices), and the VMeshRef's library id is the CRC of that node's name. **The CRC is
  Freelancer's own CRC-32** (DACOM GetCRC32: a table that is not the standard polynomial's, over the lower-cased name,
  init and final xor 0xFFFFFFFF; FLUF's `Utf::Crc32` reproduces it; `mk3db.py crc`): crc("interface.generic-102.vms")
  = 0x1351B6D4 (the goto icon's library id), crc("HUD_MNVR_goto") = 0x1B3E1E7C (its mesh's material id); with the
  standard CRC the mesh loads but nothing draws (no material match). A quad of half size 0.0155 matches the game's
  icons (their bbox +-0.016). The button draws it untinted (white texture = white) until msg 0x33 (the colour's address,
  flag 1 = grey) and msg 0xC (lit) are sent as the layout sends them.

### HUD 2 hotspots (verified in game 25 Oct 2026, HDE 0.7.1, 4K and 1280x720; code `core\host\uibtn.cpp` "HUD 2's hotspots")
- **A top-level control of our own.** The icon button factory 0x5A0C90 with `this` = 0 makes a root like the HUD's: it
  passes the parent to `CreateControl` 0x59DB20 (registry by type name at [0x67DCEC] +0xC, then the class's create with
  (parent, 0)), whose vt[0x24] (0x55A6B0 for the icon button) calls `SetParent` 0x59DD00 (null parent: +4 = 0, no child
  link) and adds the control to the flat list (0x59DBF0). Roots are updated each frame by the UI update 0x59E080 (every
  list entry with +4 == 0: vt[0x2C] when bit 0 is set, else vt[0x30]) and drawn by the UI pass when bit 1 is set.
- **The hit test 0x59DD80** takes a control only with bits 0, 1 and 3 of +0x6C set **and bit 1 on every ancestor**
  (the +4 chain); the winner is the highest vt[0x6C] priority, else the deepest (ancestor count), ties by the frame z
  +0x68. So the children of a hidden HUD root can never be hit, and a root with bit 3 clear and a zero box is never hit
  itself but lets its children be. The icon button's box test (vt[0x10] 0x5A0340 -> 0x5A1130) uses frame +0x60 + pos
  +0xA8 (+ +0xC0) and the half extents +0xB4 (all three zero = never hit); a hit sends msg 0x25 to the parent.
- **The click** (the icon button's mouse handler 0x55B950): a press (event {-1, 2}; {-1, 3} = a double click, sets
  +0x330 bit 0x80) sends 0x28 to the parent and captures; the release ({-1, 1}) re-runs the hit test with the cursor
  ray, plays the click sound (+0x374, or the std::string at +0x364), then sends **0x27** to the parent after a double
  click or runs vt[0xB4] (`exe::IconButtonActivate` 0x55A660: the command +0x34C through 0x5763B0, else **0x26** (id =
  the button, 0) to the parent's vt[0xA8]). With no parent both are dropped, so the hotspots sit in a box that owns
  them; the box's vt[0xA8] is ours through a private copy of the icon button's vtable (46 slots, 0x5E15F4..0x5E16A8,
  vt[-1] copied too; slot 0 the scalar deleting destructor, slot 42 = 0xA8 `IconButtonSetProperty` 0x55BC30, slot 45 =
  0xB4 Activate). A control under a modal window (0x59E6A0: some control's vt[8]() object has +0x2D0 set) takes clicks
  only when its parent's vt[8]() object has +0x2D0 set - the HUD's own rule, so the hotspots ignore clicks under a
  NeuroNet window as the HUD does. Click-vs-fire: while a control is hovered (0x67DCD8, 0x59DAA0) the steering (0x4C7D62)
  and the HUD's other mouse users (0x4EC6B9, 0x4ED91F) stand down and the mouse button goes to the control - the same
  for these buttons as for the game's (not verified with a physical click: a background instance reads no mouse).
- **A mesh-less icon button must have +0x338 cleared.** The Init 0x55AC30 sets +0x338 (the mesh table entry, 0x24-byte
  entries at [0x679A48]) only inside `if (mesh)` (0x55AC69 / 0x55ACEC); the ctor 0x55A560 leaves it, so a button made
  without a mesh keeps the allocation's garbage there, and its draw (0x55B3EF -> 0x559AA0 reads [+0x338]+0x20) faults
  on the first UI pass (seen: AV at 0x559ABB). 0 = no entry, which the draw handles. (The bar button always has a mesh.)
- `exe::MouseWheel` 0x616848: the wheel's accumulated position in notches (the poll adds each read's lZ / 120;
  0x41FC6A / 0x41FC9A); a frame's turn = the change since the last read (0x41F282 / 0x41F296 do the same).
- **The nav map lives under NN_MainButton** (the NeuroNet box root, `NavMap*` controls with root NN_MainButton in
  `uiitems`): USER_NAV_MAP opens it (mode space-ui) with the root shown; with the root hidden by HUD 2's switch the same
  command leaves the mode `space` and nothing visible (25 Oct 2026, instance 6).

## Direct3D 8
- Device reachable via RP8.dll IAT → `IDirect3D8` vt 15 `CreateDevice` → device vt 14 `Reset`.
- IDirect3DDevice8 vtable indices (from memory of d3d8.h — verify before relying on them):
  3 TestCooperativeLevel, 14 Reset, 15 Present, 20 CreateTexture(w,h,levels,usage,fmt,pool,out),
  34 BeginScene, 35 EndScene, 40 SetViewport, 41 GetViewport, 50 SetRenderState,
  52 BeginStateBlock, 53 EndStateBlock, 54 ApplyStateBlock(DWORD), 55 CaptureStateBlock(DWORD),
  56 DeleteStateBlock(DWORD), 57 CreateStateBlock(type, DWORD*), 61 SetTexture(stage, tex),
  63 SetTextureStageState, 72 DrawPrimitiveUP, 76 SetVertexShader(FVF), 88 SetPixelShader.
  IDirect3DTexture8: 16 LockRect, 17 UnlockRect. The Windows SDK has no d3d8.h.
- Reference overlay implementation: `repos\FLUF\FLUF\Source\Client\Ui\ImGui\ImGuiD3D8.cpp`.

## Missions, strings, saves
- **A "Destroy ..." objective need not end on its kill** (script data, 30 Oct 2026; `mission::StepAdvanceOf`, the
  install's sweep; verified in game for M13): M13 24420 "Destroy the Nomads" ends on `cap_timer` -> `trent_go_back`
  (Cnd_Timer 15; it also `Act_LockDock ... unlock`s Toledo's ring and arms `land_on_base`) -> `ether_one` (Orillion's
  0460) -> `go_home` 24430, ~20 s after 24420 shows, while `watch_cap2` (NomadCap2 destroyed) only hides the objective -
  set td1: 24420 -> 24440 took 27.4 s without a shot at Cap2. M01b 22080 "Destroy the Hostile Fighters" is the only
  other such step (the scouts flee on an 8 s Cnd_Timer).
- Mission scripts run in DLLS\bin\Content.dll and talk to the player through server.dll's pub:: API.
  IAT-hooking Content.dll's imports gives exact events: `pub::Player::SetMissionObjectives(const u32& client,
  const u32&, const MissionObjective*, u32 count, const FmtStr& title, u8, const FmtStr& text)`,
  `SetMissionObjectiveState`, `PopUpDialog(u32, const FmtStr& caption, const FmtStr& msg, u32 buttons)`,
  `DisplayMissionMessage(const u32&, const FmtStr&, MissionMessageType {0 Failure, 1 dialog, 2 objective
  text, 3 Success}, bool)`, `SendNNMessage`, `pub::SpaceObj::SendComm`. FmtStr +4 = string resource id.
  The mission results the host names SUCCESS / FAILURE in its `mission-message` event (type 1 and 3 or a
  "Mission Result" text, type 0 or a "... Failure" text) are counted as `MissionResult` (a sequence + the
  latest type 1 success / 2 failure): every leg's PASS log ends in `mission-message type=SUCCESS text=1231 "Mission
  Result: Success"`. A plain death (self-destruct outside a mission) sends no mission message, only the death dialog.
- `SendComm`'s line ids are CreateID of the dialogue line's nickname as the mission script writes it (lower-cased
  `dx_m10_0460b_botzler` = 0x899657CC, `dx_m10_0461_vonclaussen` = 0x9B942303 - the M10 log's comm events, verified 23
  Oct 2026): a script's `Cnd_CommComplete = <line>` can be matched to the comms seen (`CommHeardAt`).
- A script can set an objective and replace it in the same instant (M04 Mactan, 24 Oct 2026, every run of s12c / s12d:
  "Destroy the Rheinland Assault Wing" 22685 and "Awaiting Mission Objective" 21660 at the same millisecond, the wing
  marked by Act_MarkObj): the HUD shows the placeholder; the marks are what is left of the step. Watch the marks, not
  only the newest objective.
- The mission's own ships are told apart from encounter traffic only by where they spawn: Content.dll spawns both through
  one helper; a mission ship appears exactly at its [MsnShip] / [MsnFormation] position or its Act_SpawnShip /
  Act_SpawnFormation position (formation members as the next spawns within 1 km, in `ship =` order) - the harness's
  Get-FLMissionShips and the logic's mission::MapSpawns. Mission ships
  with `random_name = true` share no unique pilot name. **Unverified:** whether the game reuses ship space ids after a
  load (the logic's spawn list spans the process; it maps only spawns after the last menu - autopilot `NoteMenu`).
  **Verified (30 Oct 2026, A37 S1, 4 saves):** within a loaded game the space ids are stable - the contacts right after the
  load and 10 game seconds later had the same ids (49/49, 32/32, 48/48, 48/48; one fewer where a ship died). The
  controller host used to keep the objective's title and the mission marks across a load (a free-roam save loaded after a
  mission still listed that mission's marks and title); **fixed 30 Oct 2026**: they (and the comms heard) start over at
  each game load ("The client's copy of the objectives and save loads" below).
- String ids: dll index = ids >> 16 (0 resources.dll, then freelancer.ini [Resources] DLL order),
  LoadStringW(ids & 0xFFFF).
- Content.dll strings: story states `Mission_01a` ... `Mission_13`, `No_Mission` (free roam).
- Story missions have timers and nags that fail the mission (scripts: DATA\MISSIONS\Mxx\*.ini).
- Save files (HDE: plain text): `[StoryInfo] Mission`, `[MissionState]` clamps, `[TriggerSave]`/`[Msn*]`/
  `[NNObjective]` mission state, `[mPlayer] can_dock/can_tl` (missions revoke docking), `[Player] tstamp`
  (FILETIME hi,lo; load lists sort by it, AUTO SAVE always first; the list is read at game start),
  `description` = hex UTF-16. Saves/settings live in the game's user-data folder: Common.dll
  `?GetUserDataPath@@YA_NQAD@Z` (vanilla `<Documents>\My Games\Freelancer\`; HDE's flplusplus detours it:
  `save_folder_name` under My Games, or `save_in_directory` = `<install>\SAVE` - the test install uses the latter).
  The game's log goes to `%LOCALAPPDATA%\Freelancer\FLSpew.txt` regardless.
- **MissionMessageType, corrected (18 Oct 2026, in game):** 0 = story failure, 1 = success ("Mission Result:
  Success", 1231); a job board mission reports its failure with type 2 (13093 "...You ran from the battle") and its success with
  type 3 (327731 "You have made %F0v1 very pleased."; no pub::Player::AdjustCash call from Content.dll for the
  reward); types 8 and 10 come at a job's accept. The host names a message FAILURE / SUCCESS by its "Mission Result" text when the type is
  not 0 / 1.
- **Objective entries (18 Oct 2026, in game):** `MissionObjective` = 28 bytes: +0 type, +4 the argument chain, +8 the
  objective's text id. Chain node {+0 next, +4 value, +8 u16 kind} = a `FmtStr::Val` (Common.dll's `FmtStr::append_*`
  write the kinds: `S` append_system, `R` append_rep_instance, `O` append_spaceobj_id, ...; disassembly 19 Oct 2026):
  `S` system hash, `R` a **reputation instance** (pub::SpaceObj::GetRep of the object the HUD marks; small for ships and
  overlapping their small space ids - M03 Walker = 8; a job's lane ring and M03's dock ring arrived as space ids:
  resolve by rep, by id only for non-ships, `space::ObjectiveObject`), `N` a navmarker -> {+0 system, +4 ?, +8 x, +0xC y,
  +0x10 z} (the HUD's "Mission Waypoint"; a job sets no waypoint list, only these; an `N` at 0,0,0 is no marker - M01a's
  21875 "Dock with the Trade Lane" carries one beside its object TL146, read as a marker it pointed at the sun: FLvibed
  ignores it since 26 Oct 2026, `events.cpp` NavSet), `F` a number. The chain is freed
  after the call: read it inside the hook. Entry 0 is the current step, the others the plan ahead (a Kill Ships job:
  dock the marked lane -> exit ring -> "Kill all ships near the mission waypoint" at the last N). Host events
  `objective ... target= nav= system=` and `objective-step <i> ...`.
  **The entry's +4 is a FmtStr** ({+0 the Val chain, +4 the string id}: all exe::FmtStrFormat 0x41C970 reads,
  disassembly 25 Oct 2026): the game's formatter turns it into the shown text. Its codes (jump table 0x41D3B4 / index
  0x41D3E8): `%B %F %G %I %L %N %R %S %Z %d %s` (+ digits = the value's index, `v<digit>` a variant) take the chain value
  of that kind and index; any other letter, or a code without a value, is dropped. Vanilla 1.1's objective strings carry
  `%M` ("Destroy the Hostile Ships%M", 21830; HDE's strings do not): dropped, verified in game on both (host v24
  ObjectiveText, HUD 2's headline).
- **The client's copy of the objectives and save loads (25 Oct 2026, in game):** every save load reloads Content.dll
  (until 30 Oct 2026 the controller host's IAT hooks were re-applied ~0.5-1 s after each load, `events: Content.dll hooks
  installed`; now at the load itself, below), and
  the mission's objective is restored during the load, before the hooks are back: **a mission save loaded in a running
  game brings no objective event** (only the first load after the start is seen: Content.dll was hooked before it).
  The game keeps its own copy: Freelancer.exe 0x4C4FF0 `const void* cdecl(int index)` (`exe::ClientObjective`; count
  0x4C4FB0) returns entry `index` of the list at 0x672950 (first node +0x14 / +0x18 = a vector of 28-byte entries: +0 the
  FmtStr {+0 the Val chain, +4 the string id}, +0x18 the type, FLSharp: 0xA = a space objective), null past the end -
  what the NeuroNet's Current Information window (0x475A94 -> exe::FmtStrFormat) and the banner show. Read right after
  the hooks' re-install it holds the loaded save's objective (HDE: SaveC705 -> Save17f7 21830, -> Save1e7d 22425, ->
  M09_ck_arch 23635; a free-roam save: empty; vanilla 1.1 the same, "...Ships%M" formatted to "Destroy the Hostile
  Ships"). Host v25 ObjectiveReread takes it over then (event `objective-reread count= obj=`); its 'N' values are not
  read (unverified in the client copy).
  **The load's own moment (verified in game 30 Oct 2026, HDE 0.7.1, instance 2, SaveS208 -> S182, -> S010 -> S023 in one
  game, runs `research\experience\runs\loadreset_2026-10-30_0910` and the commit's re-check):** the first game after the
  start reuses the Content.dll the start loaded (no load notification for it); every later load maps a fresh copy, and the
  loader's DLL notification (LdrRegisterDllNotification, reason 1 "loaded") for `content.dll` comes **before** the new
  game's mission runs, with its server.dll imports **already bound** (3 of 3 loads) - before the load's marks (S182: 7
  marks 0.4 s later) and its SetMissionObjectives (S182: title 24200 "MISSION 12"). So the host forgets the
  title, the marks and the comms heard there, installs the Content.dll hooks at once (the load's objective, title, comms
  and spawns are seen as the first game's are) and counts the load (`GameLoads`; the logic then forgets
  mission.cpp's last script / text and shown objectives). Events `game-load <n>: Content.dll loaded - forgot title .. (imports
  bound 1, hooks 15)` and `game-load <n>: mission memory forgotten`. After the fix S182 read its own title 24200 and the
  by-title stand-in M12 (was M07's 23200), S023 no title and no marks (was M13's title and S010's marks).
- **Marks:** server.dll `pub::Player::MarkObj` forwards to the client interface (rpclocal.dll in single player,
  FLHookSDK IClient slot 75 Send_FLPACKET_SERVER_MARKOBJ); a job never calls it - its marks are the objectives' `R`
  arguments. Host event `mark id= state=` (vtable hook, found by the call's signature in Server.dll). A save load started
  inside a flight reloads rpclocal.dll (its vtable comes back unpatched; the client vtable sits at rpclocal.dll +0x33978
  in HDE 0.7.1) and the load's marks go out before the next frame: the host re-patches from a DLL load notification
  (events.cpp ClientModuleLoaded), so M03's convoy marks arrive after SaveC705 -> SaveS008 too (verified 26 Oct 2026).
- **Job board offers (18 Oct 2026):** Content.dll creates them with `pub::GF::MissionVendorOfferCreate(const
  MissionVendorOfferDescription&)` once per base (kept across save loads) and removes them with
  `MissionVendorOfferDestroy(ulong* key)` (accepted / renewed). Description: +0 base hash, +4 index, +0xC job type (1
  Kill Ships; 3 portrait icon; 6 cargo icon; 5 seen; others unconfirmed), +0x58 reward (credits). The board
  (`MissionVendorSummaryBox0..4`, `MissionVendorAcceptButton`, the Bar hotspot IDS_HOTSPOT_MISSIONVENDOR, behaviour 3)
  lists a base's live offers in key order. Difficulty is only in the text arguments: **verified in game 7 Nov 2026** (probe in
  OfferCreateHook): +0x10 / +0x28 / +0x40 are FmtStr-like blocks whose first dword heads a chain of 16-byte nodes {next, value,
  type} (type's low byte the kind: '!' 0x21 a mad-lib piece whose value is the piece's own chain, its first node's value the
  piece's IDS; 'd' 0x64 an integer; 's', 'S', 'F', 'Z', 'L' others); the +0x40 chain (IDS 0x50001 at +0x44) holds the piece
  IDS 0x50002 "Difficulty : %d0\nReward : %d1" with [d n] [d reward], the reward equal to +0x58. n is Content.dll's
  (0x06f9e9b0, decompiled): from the offer's 0..100 difficulty d, f = 6.3693 ln d + 13.668 for d >= 0.900376, else
  15.193 d^3 - 28.236 d^2 + 28.682 d - 1.0113; n = max(1, floor(f + 0.5)). FLvibed keeps n per offer (host v45 OfferDifficulties). Host events `offer key= base=
  type= reward=`, `offer-gone key=`; harness Get-FLJobOffers / Start-FLJob / Invoke-FLJob.
- **What a job pays (game data, 18 Oct 2026):** `DATA\MISSIONS\mbases.ini` [MBase] per base: [MVendor] `num_offers`,
  [BaseFaction] `faction`, `weight` (share of the board), `mission_type = DestroyMission, min difficulty, max
  difficulty, weight`; the reward is `DATA\RandomMissions\diff2money.ini` at the offer's difficulty (0 -> 1800,
  0.0821 -> 2200, 0.11238 -> 2655, 0.20513 -> 3219, 0.351 -> 3919, 0.48 -> 4792 ...; rankdiff.ini's FreeTime values
  are points on the same scale). New York: Liberty Navy at West Point (Li01_09, 70 % of offers) and Norfolk Shipyard
  (Li01_07) go to 0.21 (up to 3219); every other lawful board stops at 0.11 (2655); Buffalo / Rochester (Rogues,
  Outcasts, Junkers) go to 0.35-0.48 but are hostile to a lawful player. Observed boards matched (Manhattan
  2000-2550, Pittsburgh 2350-2550). **The story stage sets the window** (adoxa's RE in the Starport KnowledgeBase,
  research\repos\KnowledgeBase docs\guides\random-missions-editing-tutorial.md): single player, rank_diff =
  `rankdiff.ini` for the current free time (MissionNum), f = 1.3689^3; hi = the range's maxDiff, lo = max(minDiff,
  maxDiff / f) when rank_diff x f >= maxDiff, else max(minDiff, rank_diff / f); no offer when rank_diff x f <=
  minDiff; difficulty = hi - cos(rnd x pi/2) x (hi - lo). After M01b (0.112387): Manhattan 0.044-0.112 = 2013-2655
  credits (seen 2000-2550), Navy 0.08-0.205 = 2200-3219. **What a job sends:** one npcranktodiff.ini row per wave
  (NPC level), the ship count from a weighted draw (single player: 2 ships 3, 3 ships 9, 4 ships 6, 5-8 ships 2/1/1/1,
  never 1) whose column matches the difficulty. Harness: Get-FLJobBases ranks boards by this expected pay.
- **Story progression by net worth (Content.dll story object, 18 Oct 2026):** `[StoryInfo] MissionNum` counts story
  states (`mission_xx_loaded`, `_accepted`, `freetime_xx_yy`); at a mission's end the next free time sets
  `delta_worth` = net worth now + a per-free-time amount: FreeTime 01-02 **3500**, 02-03 5200, 03-04 7000, 04-05
  10000, 05-06 16000, 06-07 0, 07-08 36000, 08-09 30000, later 0 (floats written in the story object's constructor,
  Content.dll 0x06EA5E59..). Net worth = a float from one server.dll import (by its use the asset value: ship, equipment, cargo; import name not
  checked) + the player's cash (int). On the story
  object's events 0 and 0x100B, when net worth >= delta_worth (and no other mission runs), MissionNum moves on and the
  next mission loads (Mission_02 at MissionNum 7: objective 22210 "Meet Juni in the Manhattan Bar"). `delta_worth` -1
  = no free time. **Confirmed** by adoxa's StoryLvl plugin source (it patches the same table, v1.1 0x6fced28 at
  Content.dll base 0x6ea0000; same defaults); mods may change the amounts (docs\MOD_COMPATIBILITY.md).
  The imports (Content.dll IAT, 18 Oct 2026): net worth = `pub::Player::GetAssetValue(const u32& client, float&)` +
  `pub::Player::InspectCash(const u32& client, int&)`; the target goes to `pub::Player::SetMoneyNeededToNextRank(u32
  client, int target)` (host event `worth-target <n>`; debug `worth [client]`, single-player client 1).
- **Net worth = asset value + cash, with assets valued below their price (Server.dll GetAssetValue, read 18 Oct
  2026; verified: SaveC701 6750 = 0.8 x 6000 + 0.3 x 2500 + 1200):** the ship's hull good price x 0.8; each cargo /
  equipment entry: if its good's flag at +0x70 is set (cargo, consumables), count x price; otherwise (int)(item
  health x price) x count x 0.3. So **buying equipment lowers net worth by 70 % of its price** (a ship by 20 %, damage
  lowers it further): during free time, buy only after the target is met (the next target is set at the next
  mission's end).
- **Cutscene lengths:** every mission `.thn` scene's `duration`, read with adoxa's Dethorn: docs\missions\CUTSCENES.md.

## Voices and dialogue audio
- **Neural Net voice (verified in game 17 Oct 2026):** voice `NNVoice` (string 0x5D24A8) resolved once at 0x489DE0
  (id cached at 0x671F5C, Voice* at 0x671F70). Lines go through a queue object at 0x671F50 (list head +4, count +8,
  max 4, a msg already queued is skipped): enqueue `0x489E80 cdecl(msg id, 0)`; player 0x489A80 thiscall(entry)
  (entry +0 msg, +8 sound handle): gate 0x41A3E0 (float 0x6166B0 > [0x5C7474] blocks), loads the sound if needed
  (0x553660 / 0x553590), `Voice vt[5]` GetSound, starts it with 0x4285F0, which calls PlayArchetype 0x428680 from
  0x42862E. A line whose sound is not loaded yet can be dropped (first request). 57 call sites queue fixed ids from
  globals filled by CreateID at startup: launch checks (must_have_mounted_*), dealers (mounted,
  loaded_into_cargo_hold, insufficient_cargo_space, not_enough_money, ship_purchase_complete, repair_complete),
  cargo_jettisoned, cancelled, trade lane disrupted, scanning_cargo, incoming missile/torpedo, objective_reached,
  danger / warning / sun corona / atmosphere, equipment destroyed, shield states, repair droids,
  target_out_of_range, waiting_to_dock, nnv_dock_too_far, click_on_characters_and_buttons. Room changes queue none.
- **The Neural Net queue's step (exe::NNPump 0x489E00; verified in game 4 Nov 2026):** the head line ended = drop it and
  play the next. Its one caller is 0x45C8C0, which does not run in flight while FLvibed hides the NeuroNet box: under
  HUD 2 one line in four played, the classic HUD three in four; each later line waited until the box opened. A queued
  line on an empty queue starts at once, so only the second and later ones showed the fault. FLvibed steps the queue itself
  while it hides the box (hudvis.cpp), except in a cutscene, where 0x489A80 plays nothing and the line would be lost.
  There is no native text for these lines: FLvibed's subtitles are their only text.

Data facts verified by parsing the HDE files. The Freelancer.exe addresses are from static disassembly
only (not traced in game yet).
- **No dialogue text ships with the game**: the resource DLLs (string tables + RT_HTML infocards) hold
  news, rumors (infocards in MiscText), offers, objectives and logs, but no transcripts of voiced lines.
  Mission scripts and .thn files only reference line ids.
- `CreateID` (Common.dll, nickname hash): lowercase the string; `h = (h >> 8) ^ T[(u8)h ^ c]` with the
  CRC table of polynomial `0xA001 << 14`; then byte-swap h and return `(h >> 2) | 0x80000000`.
- **Voice banks**: `freelancer.ini [Data] voices = ...` → `DATA\AUDIO\voices_*.ini`: `[Voice] nickname`
  (or `extend` = add lines to an existing voice), then `[Sound] msg = <line id>`, `attenuation`, optional
  `duration`, `priority`. Audio file = `DATA\AUDIO\<voice nickname>.utf`. UTF root children are named
  `0x%08X` = CreateID(msg), and each leaf is a complete RIFF WAVE (format 0x55 = MP3, mono, 11025/22050 Hz).
  All 24,002 msg entries of HDE 0.7.1 resolve. 114 banks, 26,769 clips, ~758 min.
- UTF file: "UTF " magic, +8 node block offset, +24 string block offset, +36 data block offset. Node
  (44 bytes): +0 next sibling, +4 name offset, +8 flags (0x10 dir, 0x80 leaf), +16 first child (dir) or
  data offset (leaf), +24 data size.
- **Story sounds** (cutscenes, room scenes): `DATA\AUDIO\story_sounds.ini` `[Sound] nickname, type = voice,
  file = Audio\Dialogue\<speaker>\<nickname>.wav` (MP3 in WAV, ~52 min). A `.thn` SOUND entity has
  `template_name = <nickname>`, and an event `{ t, START_SOUND, { "<entity>" }, { duration = d } }`.
- Line ids name the speaker: `DX_M01_0100_KING` (mission comm), `dx_s006x_0002_King` (story scene s006x),
  `GCS_ML_*` (base speech fragments), `gcs_*` / `rms_*` (space chatter fragments; `+`/`-` suffix =
  intonation variant). Mission `[Dialog] Line = <from>, <to>, <msg>`; `Act_EtherComm = <voice>,
  <name ids>, <to>, <msg>, -1, <head>, <body>[, <comm costume>]`.
- Space and base chatter is built from fragments (faction, formation designation, number, base, system,
  commodity).
- Fragment ↔ game object: `msg_id_prefix = <prefix>` in system INIs (systems: `gcs_refer_system_Li01`),
  mbases.ini (bases: `gcs_refer_base_Li01_01_Base`, name = universe.ini [Base] strid_name of the same
  nickname), faction_prop.ini (`gcs_refer_faction_li_lsf`, names = initialworld.ini [Group]
  ids_name / ids_short_name of `affiliation`), goods.ini (`gcs_gen_commodity_boron`, name = the
  equipment entry of the same nickname) and shiparch.ini. Several goods reuse another good's prefix
  (commodity_vips and commodity_prisoners both use `gcs_gen_commodity_boron`). Msg ids append
  suffixes: `-`/`+`, `_base-`, `_short`.
- Formation designations ("Alpha 3-7"): `gcs_refer_formationdesig_N` speaks string 197808 + N − 1
  (Alpha, Beta, …, Omega, Red, …, Yellow, Matsu, …, Yanagi; 29 in all, `permutation_count =
  gcs_refer_formationdesig, 29` in voice_properties.ini). faction_prop.ini `formation_desig = first,
  last` string ids picks each faction's range. Ids sometimes zero-padded (`_07`).
- GCS variant counts: voice_properties.ini `permutation_count = <msg base>, N`.
- Bases name their "run by" fragment: universe.ini [Base] `BGCS_base_run_by = W02bF35` (base speech
  `GCS_ML_W02bF35` = "Run by …").
- Some lines exist twice: e.g. `DX_M01_1525_KING` is a bank entry (king.utf) and a story sound in
  DIALOGUE\MISSION13.
- Scene mixes: gf_sounds.ini `mix_s002xe` … `mix_s072d` (19 files, up to 3.4 min) carry whole room
  scenes as one track.

| Address (Freelancer.exe) | Meaning (static) |
|---|---|
| 0x430B00 | load a voices INI (called from the [Data] loader at 0x5B0DA3, key string "voices" 0x5E6A68) |
| 0x667E18 | voice registry (std::map voice id → Voice; head node ptr 0x667E1C, size 0x667E28) |
| 0x430AB0 | `Voice* __cdecl FindVoice(u32 voiceId)` (null if unknown). Callers: 0x457200, 0x489DF1, 0x4CAE5B, 0x4CDF56, 0x4CE015, 0x4CE20A, 0x4CE560, 0x553597, 0x554D64 |
| Voice vt[5] (+0x14) | `Sound* GetSound(u32 msgId)` (null if the voice has no such line); the returned object's vt[14] (+0x38) gives an int used as a maximum (priority?) |
| 0x4CDF40 | iterates a comm record: voice id +0x3C, msg ids from +0x44, count +0x84 (likely the client-side comm = fragment list) |
| 0x4571C0 | GCS variant pick: the msg name's trailing `_N` → random variant `rand()*N >> 15`, then CreateID + FindVoice + GetSound |

**Dialogue playback, verified in game (FLSubtitles, 17 Oct 2026, HDE 0.7.1):**

| Address (Freelancer.exe) | Meaning |
|---|---|
| 0x4CE000 | comm received by the client comm player: `thiscall(player, ?, CommRecord*)`, ret 8 (the record is the **2nd** argument). Record: +0 sender obj id, +4 receiver obj id, +0x3C voice id, +0x44 msg ids (u32[16]), +0x84 count. Called when the comm starts playing (the intro's Freeport 7 lines arrived 5–12 s apart, matching the clips); covers mission comms, ATC and NPC chatter (one call per assembled message). Prologue `83 EC 34 53 55` (5) |
| comm player state | (18 Oct 2026, verified in game) The comm player ("RadioReceiver", factory 0x4CD2E0: 0x5B8 bytes, ctor 0x4CD350, vtable 0x5D764C; update method 0x4CD9D0 = vt[11]) is `this` of 0x4CE000 (the host captures it). **+0x460 byte = a comm is playing** (set when one starts, cleared by the update when 0x4CBC40 reports the current one finished; stays 1 across back-to-back lines of a conversation); +0x45C current comm's priority; +0x454 the pending-comm queue (0x4CDE50 pops the next). Debug `commplaying`, `commstate` |
| 0x428680 | `u8 __cdecl PlayArchetype(SoundArchetype*, flags*, handle* out, int)`: every sound archetype started, at the moment it starts; nonzero = started. The archetype's nickname id (CreateID) is at **+4**. Callers: 0x4CB18D first piece of a comm, 0x4CC020 each following piece (real per-piece start times), 0x4575A5 THN SOUND entity start (cutscenes, room and bar scenes, scene mixes). Prologue `53 8B 5C 24 08` (5) |
| 0x42AE40 | `SoundArchetype* __cdecl FindArchetype(const ID_String* id)` (story_sounds.ini / sounds INIs, loader 0x42B030, archetype ctor 0x42B7B0 with base vtable 0x5CA604) |
| 0x4570A0 | THN SOUND entity: resolves `$BGCS_Base_Name` / `$BGCS_Base_Run_By` and `VoiceProfile_*` lines via FindVoice + Voice vt[5] GetSound (return 0x457216) just before starting them; 0x457081 = `CreateID(template_name)` + FindArchetype for story sounds and `mix_*` |
| Voice vt[5] callers | 0x5535B3 / 0x431546 preload a mission's lines when it loads; 0x554D7B releases them; 0x4CDF7D, 0x4CE231, 0x4CB020, 0x4CE58F = comm path; 0x457216 = room scene |
| 0x431012 | voices INI loader: CreateID of every msg at startup |
| 0x46A150 | `void __cdecl ChatPrint(int, const void* rdl, u32 size)`: adds Freelancer rich text (RDL nodes {u32 type, u32 len, payload}: 1 TRA {u32 attrs (0xBBGGRR00 colour, bits 1 bold 2 italic 4 underline), u32 mask}, 2 text (UTF-16, NUL), 6 style u16, e.g. 0x5C12 dialog) to the chat log, also in single player (technique from ChaosMod) |
- New game opening (Freeport 7) plays as `mode=cutscene` with camera 0x6792F8 and mission comms; the landing at
  Manhattan is the scene mix `mix_s002xe`; the "PERSONAL ENTRY" texts are DisplayMissionMessage, no voice.
- Room scenes (bar conversations) are letterboxed but run with camera 0 (base), not the cutscene camera.
- server.dll (in process in single player): `?GetRep@SpaceObj@pub@@YAHIAAH@Z`, `?GetAffiliation@Reputation@pub@@YAHABHAAI@Z`,
  `?GetShortGroupName@Reputation@pub@@YAHABIAAI@Z` (string id → "Police", "DSE", "Gateway"),
  `?GetAttitude@Reputation@pub@@YAHABH0AAM@Z` (attitude of a sender towards the player's rep) work from the client.
- Flight HUD geometry (HDE, Advanced Widescreen HUD, 3840x2160; scales with the height, anchored to the corners):
  bottom-left panels end 0.613 H from the left, bottom-right ones start 0.564 H from the right, both 0.30 H tall;
  throttle bar 0.044 H; maneuver bar at the top centre down to 0.155 H; the chat log grows upwards from 0.30 H on
  the left; system name / objective texts type at the top left (~0.21 H).
- Front end: Escape on the main menu opens the Exit dialog (`ExitButtonBox0` = Yes, `ExitButtonBox1` = No); Escape
  skips the startup movies, Space does not. The rumour/offer window in bars: `OfferDialogCloseButton` /
  `OfferDialogClose2Button`. Bar character hover cursors: `talk_story` (story star), `talk_rumor`, `talk_bribe`,
  `talk_blowoff`, `talk_mission`.
- Load lists sort by `[Player] tstamp` read when the game starts; changing a save's tstamp later does not reorder
  the list until a restart.

**Chatter grammar (Content.dll, HDE, static disassembly via dumpbin, not traced in game).** NPC radio chatter
is built server-side in Content.dll: about 70 builder functions (0x06ED6DD0–0x06EE6C30 at the HDE load
address 0x06EA0000) each append msg ids in a fixed order to one comm and send it. The full table, one
template per builder, is `docs\chatter_grammar.tsv` (header explains the columns). Helpers:

| Address (Content.dll) | Meaning (static) |
|---|---|
| 0x06EB5770 | append a msg id to the comm being built (ids come from `pub::GetNicknameId`, Server.dll import at [0x06FB3684]; Common.dll `CreateID` is [0x06FB3000]) |
| 0x06F937C0 | generic `vector<T*>` insert (also queues line objects into a conversation) |
| 0x06EEAF10 | variant: `"<base>_%02d<suffix>"`, random N within the voice's `permutation_count` |
| 0x06EEB210 | ship name part: 0 = `gcs_dhail_mbid_trader(s)/patrol(lers)` + `_leg`/`_ill`, 1 = `gcs_refer_formationdesig_%02d`, 2/3 = the numbers (`gcs_misc_number_%d`, `-` when it ends the sentence) |
| 0x06ED6800 | send the built comm |

Example, an NPC docking request (0x06ED6DD0): `gcs_misc_thisis+`, the speaker's faction `_short`
(`gcs_refer_faction_player_short` for the player), designation, number, `gcs_misc_dash`, number,
`gcs_misc_to`, the base's `msg_id_prefix` + `-`, `gcs_dockrequest_request_0N+`, then
`gcs_dockrequest_todock-`/`tomoor-`/`toland-` by the dock type ("This is Liberty Navy Alpha 1-2 to Planet
Manhattan. I'm requesting to dock."). `_p`/`_s` msg variants are the plural ("we") / singular ("I") forms.

**Chatter triggers (Content.dll, HDE, static disassembly, not traced in game).** Per-builder conditions are
in the `trigger` column of `chatter_grammar.tsv`; the machinery that applies to all of them:
- Each builder is the constructor of a 0x30-byte line object (vt[1] = "built OK"; +0x28 = string id
  19000 + number of pieces, the "Comm STRIDS" ordering table in the resources). A conversation object
  queues its lines; the chatter manager (global object 0x06FD1C08, controller id at 0x06FD1C74,
  heartbeat 1 s) keeps the running conversations.
- **Speaker gate** (every builder): the speaker's voice (`pub::SpaceObj::GetVoiceID`) must have the role
  bits in `voices_*.ini [mVoiceProp] Voice = ..., supports_roles = ...` (parser 0x06FA692C, test 0x06FA6E30 = all bits set):
  combat_lev_1 0x4000, combat_lev_2 0x4, combat_lev_3 0xC004 (= both + 0x8000), scan 0x8, hail_hailee 0x10,
  hail_dock 0x20, dockrequest_dock 0x40, dockrequest_docker 0x80, behavior 0x100, misc_chatter 0x200,
  fidget 0x400, combat_dock 0x800, random_mission_ship 0x1000, mission_commission 0x2000. HDE voices: 17
  full pilot voices (dockrequest_docker, scan, combat_lev_3, hail_hailee, misc_chatter, fidget,
  random_mission_ship), 12 combat_lev_2, 12 combat_lev_1, 3 base voices (hail_dock, dockrequest_dock,
  combat_dock). A missing role (or a missing line) drops the line and the rest of its conversation.
- Every builder also has `if (rand()/32768 != 0) skip` (e.g. 0x06ED6E95): always 0, so it never blocks.
  Real 50/50 choices use `(rand()*2) >> 15`.
- **Dispatcher** 0x06EE8D70 (`this` = 0x06FD1C08) takes an event whose vt[0] returns the kind 1-25 and
  creates the conversation: k1 0x06EE2000 dock hail, k2 0x06EE6E60 docking, k3 0x06EE8880 launch,
  k5 0x06EE4CD0 player hail, k6-16 0x06EE2C20 combat, k17-24 0x06EE55F0 scan, k25 0x06EE7B00 fidget.
- **Event gate** 0x06EEBD10(kind, mgr, A, B), record 0x06EEB920 (last 10 per kind): global switch
  0x06FCE368 = !clamp (mission `Act_GCSClamp = true` / mission ini `gcs_clamp` silences all chatter); an
  NPC that is not owned by the global controller 0x06FD0E64 (inferred: mission-scripted ships) only
  passes kinds 6, 7, 8, 10, 11, 13-16; kinds 1-4 need A (0x06F73A40) and pass at once when B is the
  player (so bases always answer the player); otherwise a same-kind
  event recorded within 10 km (distance² < 1e8) of A (or B) and younger than the kind's cooldown blocks
  it, then `rand()/32768 < probability`. Hardcoded config 0x06F5CDA0: cooldown k2 12 s, k25 30 s,
  k6 6 s, k8 6 s, k7 0 s, k9-11 and k13-16 3 s, all others 15 s (k1, k3, k12); probability k6 0.05,
  k7 0.4, others 1. The dispatcher gates k1-3; the manager's message handler gates k6-16 and the fidget
  timer k25; k5 and the scan kinds have no cooldown.
- Shared conditions: "NPC" = found in Content's per-system ship list (0x06F73990); the player's ship is
  never in it (and trent_voice has no supports_roles), so the player never speaks chatter. Formation: list from 0x06F66220 (formation id at
  ship+0x154), leader = its first member; "follower" = ship+0x14C set and +0x154 != -1. The leader of a
  formation does the full exchange only when its **ship id % 5 == 0** (deterministic per ship);
  otherwise each ship just says its own line. Job type = ship+0x178 → vt[0x28]: 2 trader, 3 and 4
  patrol (3 = waypoint patrol, has a waypoint count via vt[0x4C]); job vt[0x34] = target (^dest).

| Kind | Source (static) |
|---|---|
| k1 dock hail | manager message 0x2D (a ship comes near a base, inferred; sets a 30 s timer); ambient NPC, not a follower |
| k2 docking | docking request handler 0x06F6E110 (response r of 0x06F6D820, posted unless r = 5), queue re-check 0x06F6E9D0 (r = 4, "now cleared" flag), trade lane handler 0x06F6FCB0 (r = 4, then `pub::SpaceObj::LaneResponse`). Base answer by r: 0 responsehostile, 1 none, 2 denied/nofit, 3 standby/delayed, 4 granted (then `pub::SpaceObj::Dock`). Target type (`GetType`): 0x40 jump gate / 0x800 jump hole → convoy gate lines, 0x80 trade lane ring → tradelanestart, 0x400 airlock → none, else base docking |
| k3 launch | launch handlers 0x06F5D690 / 0x06F607E0 |
| k5 player hail | manager message 0x1F (0x06EE9900; inferred to be the player's hail) |
| k6 hit | engine message 0x0D: damage > 0, cause 2-5 or 7, victim hull (`GetRelativeHealth`) < 1.0 |
| k7 destroyed | engine messages 0x38 (destroyed ship) and 0x39 with field +0x10 = 1 (killer) |
| k8 in sights | `ContentCallback::ship_in_sights_event` → message 0x100E |
| k9 / k11 / k10 | `ContentCallback::combat_state_notification` (0x06F34090) → message 0x1001: type 0 → k9 engage, 1 (too far) / 2 (too tough) → k11, 3 / 4 → k10 combat over (3 = followers say rejoiningform) |
| k12 help | 0x06F70580: an NPC within 5 km joins a fight on another NPC's side (sets its attitude) → message 0x1003 |
| k13 drift | `ContentCallback::combat_drift_event` → message 0x1002 |
| k14 flee | `ContentCallback::request_flee_destination` (0x06F34CE0) → message 0x1004 with the FleeReason: 0 tough, 2 damaged, 3 noweapons, else other |
| k15 friendly fire | player-damage handler 0x06F6FF90 → message 0x1005 (first field 0) |
| k16 new leader | formation leader replaced (0x06F5E680) → message 0x1009 |
| k17-24 scan | scan state machine 0x06F0CBC0 / 0x06F0DA00: 17 announce and 18 nothing found only with faction_prop `scan_announce = true`; 19 order drop (scan_for_cargo good found); 20 NPC complied (comply + happy, player: happy); 21 NPC refused (`AI::get_scan_response` 1/2); 22 angry; 23 silent; 24 loot destroyed |
| k25 fidget | manager timer (message 0x20, 0x06EEA890): each 1 s heartbeat a 1/120 roll, an ambient formation leader (not in combat) within 2.5 km of a player; topic roll r: < 1/30 rumor, < 0.2 status (whatstatus + every wingman's saystatus, or the waypoint question), else simple / cargo talk |

Thresholds: taking_damage_worst when hull < 0.49, inflicting_damage_worst when the target's hull < 0.33;
saystatus damagelarge < 0.30, medium < 0.75, small < 1.0, else none; base hails are hostile at attitude
≤ -0.6; dock type 1 berth (todock / clear_berth), 3-5 moor (tomoor / clear_moor), 6 docking ring (toland /
clear_ring, no number), 2 none; docking numbers above 20 are read digit by digit (0x06EEB590).
Needs an in-game trace: what engine messages 0x2D, 0x38, 0x39, 0x0D and 0x1F are exactly, the owner of
controller 0x06FD0E64, the NPC predicates vt[1]/vt[2], job types 3 vs 4, how queued lines are timed.

**Base and bar speech (data files, HDE).** Room conversations are Lua 3.2 bytecode scenes in
`DATA\Scripts\GCS\GCS_{FM,ML}{Sit,Stand}_<code>_NN[_MP].thn` (637 files); each SOUND entity names
`VoiceProfile_<msg>`. A `$`-prefixed msg is a placeholder Freelancer.exe resolves per base
(error text "No BGCS_base_run_by specified for base %s, cannot resolve voice profile message %s"); the
voice banks also hold a default clip under the placeholder name. Scenes that join two pieces into one
sentence: W02aF `$BGCS_Base_Name` (= `GCS_ML_W02aF_<base>`, "Welcome to Harris") + `$BGCS_Base_Run_By`
(= `GCS_ML_<universe.ini BGCS_base_run_by>`, e.g. W02bF37 "run by LWB", W02bF35 "a Liberty Planet");
A03aG `$BGCS_Faction_Rebuff_ML/FM` (= `GCS_ML_A03aG_<faction>_grp`, "The Corsairs don't") then scene
A04aG (`GCS_ML_A04aGNN`, "waste any time hunting down people who cross them."); D06aG `GCS_ML_D06aG00`
("Oh, well, I work") + `$BGCS_My_Faction_ML/FM` (= `GCS_ML_D06aG_<faction>_grp`, "for the Corsairs").
Trent's bar replies are `$BGCS_Trent_<code>P0N_<variant>` (voice trent_voice).

## Movies (Movie.dll, verified in game 17 Oct 2026)
- Startup movies: `freelancer.ini` `[Startup] movie_file` lines (HDE comments out the MGS/DA logos by default).
  Freelancer.exe imports `PlayMovie(HWND, bool, const char* path)`, `UpdateMovie()` (polled in a loop while it
  plays; the game frame loop does not run), `StopMovie()` from Movie.dll.
- Threads (disassembly and thread ids in game, 5 Nov 2026): Movie.dll creates no thread and calls `Sleep` at two sites,
  `Sleep(300)` at the start of `UpdateMovie` (RVA 0x1735) and `Sleep(0)` in `PlayMovie`'s message pump (RVA 0x21A5); the
  game's thread runs both. Only the video itself plays on DirectShow's thread.
- Movie.dll (2003) plays through DirectShow: `CoCreateInstance(CLSID_FilterGraph)` (its own IAT), the old Video
  Renderer as a child window of class `VideoRenderer` inside the game window, stretched to the whole client area
  (aspect not kept). The picture placement can be changed without touching any window through the graph's
  `IBasicVideo` (IDispatch-based vtable): 28 SetSourcePosition(l,t,w,h) in native video pixels (crops), 31
  SetDestinationPosition(l,t,w,h) in renderer-window pixels, 32 GetDestinationPosition, 33
  SetDefaultDestinationPosition, 34 GetVideoSize; verified 17 Oct 2026 (S_OK, takes effect at once, Movie.dll does
  not reset it). Resizing the child window instead (SetWindowPos) needs SetDefaultDestinationPosition afterwards,
  leaves stale pixels around a smaller window, and disturbs capture tools.
- Intro files: HDE `fl_intro.wmv` = HD intro (1440x960, 104 MB, 223.6 s), `fl_intro_vanilla.wmv` = 2003 intro
  (720x480, 39 MB, 226.8 s); Bonus DVD full intro 720x480, ~451 s. Encoded bars (cropdetect): HD picture
  1438x594 at y 182, vanilla 718x296 at y 92; DA logo 1440x838 at y 60; MGS logo 1280x960 without bars.

## HD Edition
- Console plugin (console.dll): commands are typed in chat, e.g. `launch` (did nothing in story mission 1,
  blocked by the mission), `rep l H`. Enter must be a real window message (PostMessage WM_KEYDOWN/WM_CHAR/
  WM_KEYUP VK_RETURN); FL's own key handler path does not submit the chat line.
- HudShift.dll moves HUD elements (`repos\HudShift`).
- Tutorial popups reference keyboard keys ("N key", "F5 key", "press F1").

## The NeuroNet windows by their own calls (7 Nov 2026)
Read in Ghidra, used in game (instance 7, background); the research is `docs\research\neuronet-windows-facts.md`, the use `docs\NEURONET_PLAN.md`.
- **InvSelected 0x47E790** answers the EquipDesc of the row at +0x3A4 under the filter at +0x32C: asking row by row lists a whole filter (verified in game). **InvSelectRow 0x47E0E0**, **InvSetFilter 0x47F240**, **InvMount 0x47F720** (thiscall(EquipDesc*, mounted)). Filters: 0 weapons, 1 ammo, 2 external, 3 internal, 4 commodities (the hint words verified).
- **Mounting is a dock's** (GitHub issue 79, read from the window dumps and seen in game 11 Nov 2026, commit b2c75339): the game's own inventory window hides its mount toggles `EquipToggle1..5` in space (state 0x0A / 0x5A; docked 0x2B / 0x7B), and **InvMount 0x47F720 itself has no docked test**: a caller must make it (FLvibed's INVENTORY page: `nnpage.cpp` Mountable, the player docked). Before that, A unmounted a gun in flight. Details: `docs\research\neuronet-windows-facts.md` section 2.
- **The map's marks without a size of their own** (records of type 2 ship, 3, 4; 0x496600, read and driven 7 Nov 2026): their size is record +0x3C, divided by max(1, Z / 2) in view 2 and then scaled like the world, so a grown map grows them; other records take +0xF48 instead. **The status window's data** is the in-process server's answer to the draw's once-a-second vt+0x124: with `FrameUpdate`'s gates shut it never comes (seen in game: numbers blank, the pairs at +0x410 empty).
- **StatusFactionCard 0x4A8E80** thiscall(index) opens a faction's card; the status pairs sit at +0x410. The game's five standings come from 0x45A6E0 (thresholds -0.6, -0.2, 0.2, 0.6).
- **GamePaused**, by return address: 13 callers. Answering true to the world's update and the frame update only stops the world while the window stays open (the game's own pause closes it); verified in game, contacts freeze and resume.

## The docked pages (7 Nov 2026; docs\BASE_ROOMS_PLAN.md keeps the list, read in game unless marked)
- The save lists: visible rows = list +0x8A8 (20, written by setup 0x56B330 and read each frame; decompiled, and a write of 16 took in game). The pitch is no field: the list font's line height, so game_text_size 125 runs the 20 rows past the frame. Each screen has two column lists. Front: screen +0xD8 and +0xDC (VtFrontList; 16 written to both, read back). NN_LoadSave: +0x5E4 (VtGameList) is the date column, its scroll bar +0xEA0 (min +0x3C0, max +0x3C4 = rows - 20, position +0x3C8, page +0x3CC); the names column is not among the screen's fields up to +0xF00: not found.
- NN_Repair: `RepairCheckBoxN` toggle +0x3A0 is 0 ticked, 1 not; `RepairSmallIconsN` are image controls (PlaceAt moves them); `RepairPlayerCash`, `RepairTotalCost`; `RepairItemDamageN` holds no readable share. The client's equipment list node holds the health at +0x18.
- The final stage: `InvSelected` works on both windows (each its own +0x34C list; 0x47E790 with +0x35C and +0x978 == 2). ShipDealerShipPrice is net of the dealer's side; the charge is that less NN_Dealer +0x9FC. Same-named controls of the two lists stand in no fixed order: tell them by the +4 parent chain.
- NN_CommodityByBase vt 0x5D027C; rows `CommodityByBasePriceN`, `CommodityByBuySellPriceN`, `CommodityByBaseBaseN`, `CommodityByBaseSystemN`. It replaces the inventory while it stands.
- **ShipViewControl** (`exe::VtShipView` 0x5E4BCC, ctor 0x5836A0; NN_ShipTrader keeps it at +0x370; decompiled and seen in
  game 7 Nov 2026): the dealer's update 0x4B9260 drags it (0x584C80, the cursor in the rect +0x494..+0x4A0, a button down):
  yaw +0x520 / pitch +0x524 in degrees (a rect's width = 180), wrapped to +-360, copied to the live YPR +0x37C / +0x380, the
  matrix +0x358 (9 floats, Ry then Rx) rebuilt; no easing pair, so a write stands (basepage ShipTurn). The game has no zoom
  (wheel slot vt[0x48] returns 0): HDE's flplusplus puts its handler there and adds speed x notches to +0x3EC (negative =
  farther, clamped by flplusplus.ini; about -30 for a new ship) - basepage ShipZoom writes it. Yaw, pitch and zoom reset per ship.
- ShipView_Control (Inventory +0x330, vt 0x5E4BCC as ShipTrader3DShip): hidden by the builder 0x478100 (+0x6C bits 0 and 1 cleared), companions ShipView_Toggle / _DamageBar / _Name, Shipview_Frame fetched in 0x47B410 (decompiled; what sets its ship unread).
- The dealer's whole stock: write +0x3A4 = k, `exe::DealerSelectedGood`, restore; `exe::InvSelectRow` selects on either window, row counted from +0x3C4, and clears the other window's +0x3A4.
- A window moved off the screen goes on working: clicks by name, the slider, filters, buying and selling. An image control (vt 0x5E4D7C, a row's mesh) moves by position property 6 through vt[0xA8]; its parent's bar: +0x3D0.
- **The list rows' item models** (NN_Inventory, NN_Dealer and its partner; decompiled 0x47B410 / 0x47C740 / 0x47D2F0 / 0x589050 /
  0x5A13C0 / 0x59FBD0, verified in game 11 Nov 2026, instance 4, HDE 0.7.1, 1280x800 and 3840x2160, docked and in space; issue 58,
  `modules\hud\src\nnpage_kit.h` RowIcons): each window has five image controls `ItemMesh1..5` at +0x44C + 4k (0x47B410; vt 0x5E4D7C,
  `exe::ImageSetProperty`), and its list update 0x47D2F0 runs **every frame** from the window's update (a write watch on mesh +0x2EC:
  317 writes in 311 frames, eip 0x47D091, caller 0x47D7BB): the row fill 0x47C740 gives mesh k = (game row - +0x3C4) its row's icon,
  rows outside the window of five are skipped. The game's rows count the ship's **empty hardpoints** too (InvSelected answers 0 for
  them; their fill sends (0x17, -1) and the mesh drops its model). The fill's messages through vt[0xA8]: mesh +0x2EC = k; (4, GoodInfo
  +0x78, 0) when not 0; (0x19, 0x10000, 0x10000); (0xC, row == +0x3A4, 0); (0x17, -1, 0) for a row with no GoodInfo. GoodInfo +0x78 is
  the good's `item_icon` file name (a `const char*`: the mesh then holds "equipment\models\commodities\nn_icons\EQUIPICON_gun.3db";
  every gun shares that file, launchers have their own). Message 4 goes to ControlSetProperty, which calls vt[0x98] = 0x59FBD0: it
  returns at once when the control holds a model (+0x84 != -1) of the same name (stricmp with +0xD0), else copies the name to +0xD0,
  drops the old model and loads the new one. 0xC sets +0x347; 0x17 sets +0x84 = -1. Sent from outside, a model takes **only between
  the fill and the UI pass** (the pages' Under, `views` UnderBegin): sent after the UI pass (the overlay) it was undone by the next
  fill before it was ever drawn (a Javelin launcher's row stayed blank, gun rows looked right only because the game's own model was
  the same file). A load costs about 10 us at 1280x800 and up to about 360 us at 3840x2160 when two models alternate on one mesh
  every frame (the fill's and the page's), so the page moves the window (its scroll bar +0x3D0) to cover the most
  shown rows and lends a mesh only to the rest. The list update keeps +0x3C4 inside the list (start <= count - 5).
  A lent model can stay without a load a frame (verified in game 11 Nov 2026, instance 4, both sizes; `nn.RowModel` 49 / 22 us a frame at
  1280x800 / 3840x2160 -> 0.6 us, the game's frame rate 55 / 45 -> 60 under fpscap 60): with its name +0xD0 set to the model the fill sends
  that mesh, 0x59FBD0 finds that name with a model held and returns, and the lent model is drawn. A model's load or drop goes through
  0x59EEE0 -> 0x420F70, which destroys the instance (Engine vt[0x60]) before 0x59EC80 makes the new one (Engine vt[0x44] finds the archetype
  by name, vt[0x5C] makes an instance); the engine **reuses instance handles**, so a new model can come back under the old +0x84: tell a
  model by handle and name together. (0x17, h) puts handle h at +0x84 after destroying the old instance. A mesh whose own row the fill sends
  (0x17, -1) (an EMPTY hardpoint) keeps nothing: lent last (`nnpage_kit.h` LentModel).
- **A model drawn with no game control** (audit R3b, verified in game 12 Nov 2026, HDE 0.7.1, instance 5, docked at 3840x2160 with
  no window open; `modules\hud\src\modelprobe.cpp`, the `modelprobe` test verb; evidence `research\experience\runs\r3b_probe_20261112_095514`):
  the ship view's draw 0x584470 works with the mod's own objects. A 0x28-byte holder (0x420D40, +0x10 = -1, five 1.0 floats at +0x14)
  loaded by 0x59EC80(holder, file, 0, 0) (thiscall, ret 0Ch: the third argument keeps the archetype forever when non-zero) gives an
  instance handle at +4 (EQUIPICON_gun.3db: 0.84 ms for the first load); a camera from 0x40F150 (no destructor, nothing registered) with
  clip 0x40FB60(0.1, 10000); three lights as the ship view's ctor builds them (0x410CA0(-1), 0x4110C0(3, 0), 0x411470(negated unit
  direction), 0x4114F0(rgba), then the light's vt[1]); each frame: Engine vt[0x9C](h, float[12] 3x3 + position) and vt[0x6C](h, 0, 0) place
  the model in front of the camera (the camera stays at the origin looking down -Z), 0x4101E0 / 0x410240 / 0x4102C0 the lights,
  0x40FA10 the field of view then 0x40FB80 the rectangle (the ship view's order), 0x422610 the viewport, Pipeline vt[0x3C](2, the local
  rect) the square's depth, 0x422910 the projection, Pipeline vt[0xF0] (7, 1) (0x17, 4) (0xE, 1), Renderer vt[0x1C](camera, h, 1.0,
  0x300, 0, 0), 0x421ED0(0xB71); afterwards the Z states (7, 0) (0x17, 8) (0xE, 0), 0x421ED0(0xB71), RenderSetCamera(UiCamera),
  RenderSetViewport(saved), the lights cleared and the ambient white. The model is drawn lit, only inside its rectangle, over the room's
  3D scene with correct depth; the viewport reads back unchanged (RenderGetInt 0xBA2 and the device's); nothing stays drawn after it
  stops. The engine interfaces come from the import slots 0x5C6D90 (Engine), 0x5C6DE0 (Pipeline), 0x5C6DF0 (Renderer), each `**slot`.
  CPU time per frame: 1 model 0.011-0.021 ms, 12 models 0.031-0.033 ms (submission only). The nn_icons models are flat cards with a
  black backdrop drawn with back faces culled (invisible for half a turn). The same in flight (HUD 2 drawn over the squares) and at
  1280x800; two hot reloads of the logic with the probe drawing left the game and its lighting as before (the lights taken out of the
  Pipeline's list by their destructor vt[0](0) at shutdown). Engine vt[0x88](h, 0, A, B): B is a centre the model turns about well
  (the Starflier centred with it), A[0] is SMALLER than the model's reach (the Starflier reaches about 1.4 x A[0] from B: it clipped at a
  computed fill of 0.85 and fitted at 0.6); li_auto_cannon.cmp turns about its mount; br_railgun.cmp and ge_mine_dropper.3db load (a
  handle, no fault) but draw nothing: not diagnosed. Not verified: flags 0x300's meaning (no visible difference against 0), vanilla 1.1
  in game (its exe has every signature at the same address: `sigtool check`). Runs: `research\experience\runs\r3b_probe2_20261112_100403`,
  `r3b_probe3_20261112_102048`.
- **The model slot's facts** (audit 70b F7, `modules\hud\src\modelslot.*`; decompiled 13 Nov 2026, HDE 0.7.1, not verified in game
  unless marked): **CamSetFov 0x40FA10** stores its degrees as the VERTICAL half angle (+0x9C; the top and bottom planes' slope is
  their tan) and derives the horizontal one +0x98 = atan(tan x aspect +0xA0); **CamSetRect 0x40FB80** stores the inclusive
  rectangle (+0xAC..+0xB8), the aspect, half sizes and centre, KEEPS the horizontal angle and derives the vertical one from the new
  aspect. So with rectangles of different shapes on one camera the field of view drifts unless the rectangle is set first and
  the field of view after it (the slot's order; for squares both orders give the same camera). **Instances** (EngBase.dll, the
  Engine component): a handle is the instance object's pointer; vt[0x60] (0x6621830 -> 0x6621710) checks only 0 and -1, then
  destroys children, erases the instance from the engine's set (+0x10), releases its archetype references (+8, +0xC) and returns
  the object to a LIFO pool (0x661EB70), which is why handles come back reused. Nothing in the engine destroys instances
  wholesale or purges archetypes: only owners (`~CObject` Common 0x62AF440, the ship view's destructor 0x583C30, the image
  control 0x59FA50 / 0x59FB60) and the engine's own destructor 0x661F750 at `DALib::Shutdown`. Each instance holds its archetype
  by its own reference (vt[0x38] at create; MeshLoad drops its find reference when its fourth argument is 0). MeshFree 0x59EEE0
  frees the anim handle and the instance only when set and clears +4 / +0x10 itself, so a second free of one holder is
  harmless; a free of a handle the engine already destroyed is NOT (no generation check). How a device Reset touches instances,
  lights and cameras is unread (rp8.dll is not in the Ghidra project). **Verified in game 13 Nov 2026** (HDE 0.7.1, instance 9,
  1280x800 and 3840x2160, the slot through `modelprobe`; run `research\experience\runs\f7_slot_20261113`): a holder freed and
  loaded again gets the SAME handle back (the pool), and the slot's holders freed and reloaded over HUD 2's leave, a forced
  device Reset (`d3dreset`), a launch, two save loads and two hot reloads left the game running and the models drawn lit.
- **A dealer row's price exists only on the game's row of five** (decompiled 14 Nov 2026, 0x47D2F0 / 0x47C740; GitHub issue 93): the list update hands the row
  fill an int price (its 6th argument), which formats it into ItemPrice0N (control +0x410 + 4k, 0x4779A0) or writes "" when it is < 1 or the row's flag
  (10th argument) is set; rows outside the window (+0x3C4) are skipped. For the ship's items: `ftol(MarketGoodPrice(base market, good +0x48) * health
  (EquipDesc +0x10))`, 0 when the base's market has no entry of kind 0 or 1 (0x438100); with kind 0, GoodInfo +0x70 clear and the item's sub id found in
  the window's EquipDescList +0x3A8, `ftol(price * 0.3 + 0.49999)` (0x5D0E2C, 0x5D1910): the resale. Nothing else keeps a row's number, so HUD 2's dealer
  page (rows in hardpoint order, often outside the game's five) keeps the last number read per row and steers the game's window over a row not read yet
  (`core\game\screens\market_screen.cpp` Read / ReadFrame), reading only a window that has stood a frame (the game refills the texts in its next update).
  Before the fix, Save1148 at Buffalo, weapons filter, walked with the pad: Drone Mine and Adv. Countermeasure Dropper showed no price at 2 stops of 24;
  after, none of 14 rows at either size (`FLvibed-lane-dialogs\scratch\probe93_*`, 14 Nov 2026).
- ItemPrice0N holds a visible row's own number. NN_ShipTrader +0x414 holds indices, not ships. ShipTrader3DShip has no rectangle and follows its window by its own vt[0x14].
- The ship view's selected part (decompiled, 0x4E6EC0, 0x4E6CB0): `ViewPrepare` colours the watched ship's equipment and armour groups; the one whose id equals the short at owner +0x3B4 pulses (0x561F20). `FUN_00405a00(list, root, flag, argb)` sets a part's vertex colour.
- HD Edition's base infocards list ships and commodities.

## The INFO page's composers (R3)
The NeuroNet INFO window's text is composed by 0x474B90 (docked / in space) and 0x4754A0 (a target); `core\flexe.h` signs what HUD 2's INFO page
(`core\game\screens\nnhelp_screen.*` and `modules\hud\src\pages\neuronet\info_*`, since audit 70b F2a) calls. All cdecl unless marked; read in Ghidra 12 Nov 2026 (HDE 0.7.1; the same addresses in upstream HDE and vanilla 1.1
by `sigtool check`): **decompiled, not verified in game**. The text helpers share the scratch buffers 0x66FC60 / 0x66DC60: game thread only.
- **RdlAppendCard 0x57DA40** `bool (uint ids, RenderDisplayList*)`: XML infocard (resource type 0x17) or string-table text; false when the DLL / block is absent.
- **RdlAppendText 0x57E2C0** `void (const wchar_t* text, int count, RenderDisplayList*, int ptrMode)`: XML first, else the plain parser 0x57DB50.
- **InfoMarket 0x476080** `void (RenderDisplayList*, uint base, int commodities, int equipment, int ships)`: the market sections (low byte of each flag); nothing when all are 0.
- **ZoneAtPoint 0x4A1B50** `IZone* (ISystem*, const Vector* pos, int needName, int needInfo, int ignoreVisit)`; the page passes (sys, pos, 1, 0, 0).
- **SectorText 0x48DC10** `void (const float* pos, wchar_t* out, int count)`: the grid sector of pos (x [0], z [2]) by IDS 1357 (the
  window passes the count of its buffer as the third argument, 0x475221).
- **AttitudeBand 0x45A490** `int (IObjRW* player, void* target)`: -1 / 0 / 1 at -0.6 / 0.6 (target vt 0x68).
- **ShieldLevels 0x4D4B50** `void (CSimple*, float* max, float* cur, byte* anyFlag)`.
- **ObjectVisit 0x4D4D00** `byte (CSolar*)`: GetVisitValue | the visit map's byte; 1 for non-solars.
- **VisitLookup 0x4C4DA0** `bool (uint id, byte* out)`: `out` is not written when absent.
- **ObjectNameIds 0x5472A0** `uint (CSimple*)`. **PilotName 0x5472D0** `size_t (CShip*, struct { wchar_t* buf; uint cap; }*)`: buf malloc-ed, the caller frees it.
- **InfocardMapFind 0x458890** `bool (uint ids_info, Entry** out)`, Entry {+0 second card, +4 equipment, +8 commodities, +0xC ships}.
- **FindObject 0x5416C0** `void* (uint id, int kind)`: kind 0 searches every list; vt 0x150 gives the CSimple*.
- **WaypointShown 0x4F42A0** `bool __thiscall (manager, byte* isPlayer, int* index)`, `this` = the pointer in **WaypointManager 0x674BC8** (data).
- **BaseNameIds 0x43BBE0** `uint __thiscall (RoomInfoOwner 0x668708)`: the docked base's name IDS; a base id missing from its map is a null read, so docked only.
- **MissionSolarsLoad 0x476C40** `void ()`: loads `Universe\MissionCreatedSolars.ini` once into **MissionSolarTable 0x66DC0C** (data: the vector's begin, end at +4),
  records of 0x14 bytes {+0 base id, +4 pos, +0x10 ids_info}. 0x476FE0 `bool (uint base, Vector* pos, uint* ids_info)` is the game's own lookup in it (unsigned).
- Already signed: 0x54BAF0 is `exe::GetPlayerIObj`; 0x4C4830 returns `exe::PlayerSystem`, 0x4C4810 tests `exe::PlayerBase` & 0x3FFFFFFF, 0x4C4900 returns &`exe::PlayerBase`.
- **Verified in game 12 Nov 2026** (HDE 0.7.1, instance 5, SaveC706 / SaveC701, `nnpage info`; run `research\experience\runs\r3_probe\20261112_160819`):
  these called in the window's order (0x474B90 docked / space, 0x4754A0 a target) into a RenderDisplayList the mod owns (Common.dll
  `??0RenderDisplayList@@QAE@XZ`, 0xC bytes) and written out by `BinaryRDLWriter::write_buffer` give the window's own text paragraph
  for paragraph: no target in space, docked at Manhattan (57 paragraphs with the market), Fort Bush, Planet Manhattan, Baltimore
  Shipyard, a storage depot, a lane ring, the sun, two NPC ships, three unknown objects. The player's waypoint differed (the window:
  WAYPOINT, "Player Waypoint 1"; the composition: UNKNOWN OBJECT): open. A RichTextWin keeps its list embedded at +0x488 (12 bytes);
  write_buffer reads it without changing it. "Health: 100": MSVCRT's swprintf prints the card's "%d%\n" without the stray %.
  **The refresh** (0x4E4690): with the classic HUD a new target with the window open rebuilt it, a cleared target left the card; with
  HUD 2 the window was not rebuilt (its target panel is hidden and the world paused under the page: not separated).
  **No knowledge reveal:** opening and closing INFO left the visit bytes of Fort Bush, Baltimore, the depot (1) and Manhattan (30)
  unchanged; the builders reach neither 0x459950, 0x459210 nor 0x4C4E00 (decompiled, direct calls).

## Rich text: how the game draws an infocard's styles (audit 84_, 12-13 Nov 2026)
Decompiled [D] (Common.dll, 12 Nov 2026), not yet seen through a hook in game: the INFO window is expected to draw this way.
- **Fonts:** per rich-font slot eight GDI fonts, one per bold / italic / underline combination (the LOGFONT builder 0x063277F0,
  `ReinitializeRichTextFonts` 0x06326410, `DeviceTRC::static_set_font` 0x06327D50, the variant table 0x063ED608), drawn with
  ExtTextOutW; `DeviceTRC::execute_text` 0x06328240 picks `HFONT[attrs & 0xFF]` (slot x 8 + variant). Italic = `lfItalic`,
  underline = `lfUnderline` (GDI's rule in the text's colour, the shadow pass underlined too), bold = weight 700 against 400.
  ANTIALIASED_QUALITY, lfHeight = size x screen height / 768; every run gets a drop shadow 0x0A0A0A at 2 x H / 768 px first.
- **The TRA record** (BinaryRDLWriter 0x0632A970): bit 0 bold, 1 italic, 2 underline, 3-7 the font slot, 8-31 the colour as a
  COLORREF shifted up (0xBBGGRR00: #FF0000 arrives as 0x0000FF00). A card's `color="default"` loses its default flag in the writer
  and arrives as colour 0 under a set mask; `font="default"` as slot 0; a card's `font="n"` is 1-based (slot n - 1).
- **The styles:** id = CRC16 of the name; STYLE_NN 0x9CA4 = slot 0 (Arial Unicode MS 22) plain in 135, 195, 224, STYLE_NN_BOLD
  0xF64 the same in bold; both built into the exe (0x468C20), not in `rich_fonts.ini`.
- **The faces have no italic file** (HDE's DATA\FONTS: AGENCYB, AGENCYR, AGENCYR_CR, ARIALUNI), so the italic is GDI's synthesized
  oblique. Measured 13 Nov 2026 through GDI on AGENCYB.TTF / AGENCYR.TTF (700 / 400, 24 to 600 px): the upright glyphs sheared
  0.34 px across per px up about the baseline, every advance unchanged (GetTextExtentPoint32 equal upright and italic), no slope
  in the outline metrics; the underline at 1/8 em below the baseline, 1/16 em thick (otmsUnderscorePosition / Size). HUD 2 draws
  the same (`core\ui\hud2_kit.h` kStyleItalic / kStyleUnderline).
- **Bold on a face with no bold file** (slot 0, ARIALUNI.TTF, regular only) is GDI's synthesized emboldening. Measured 13 Nov 2026
  through GDI on the game's ARIALUNI.TTF, 400 against 700 (an 'l' stem at 0.4 of the cell; the script, git-ignored, in
  `research\experience\lanes\fixsmall_scratch\gdi_bold.ps1 -Font <ARIALUNI.TTF>`): the left edge kept, the ink widened to
  the right by 1 px from 16 to 48 px, 2 px at 96, 3 px at 200 (about em / 64, at least 1 px); every advance 1 px wider (24
  characters: +24 px at every size). HUD 2 draws its bold runs the same way over its already bold face, wider (0.025 em, at
  least 1 px) and with the advances unchanged (`hud2_kit.h` kStyleBold; the owner's "Synthetic emphasis", 13 Nov 2026).
- **In HDE 0.7.1's cards** (5,258, counted from `research\strings\resources.tsv`): one colour, #FF0000 (bases' CLASSIFIED, the
  zones' red bold warnings), plus `default`; italic in 261 cards, underline only in system cards (with bold), never on INFO.

## Screens: the server interface and the shared stores (audit 70b F-sym, 12 Nov 2026)
Read in the disassembly (Ghidra, HDE 0.7.1; the exports of Server.dll), **not yet run in game**; signed in `core\flexe.h`
"screens: shared / dealers / menus" (`sigtool check --scan`: found at v1.1 in HDE 0.7.1, the HDE repository's exe, the
decrypted vanilla 1.1 and Discovery's hotfix exe), the slots in `core\game\iserver.h`. Nothing calls them yet (the page
framework and the families' probes will; sources: the audit's notes 31 to 35 with their independent checks, outside the repository).
- **The server interface.** `ServerIface` [0x67ECD0] and `ChatServerIface` [0x67ECD4] are set by 0x5A8080, which loads
  Server.dll in single player and RemoteServer.dll in multiplayer (0x5A8129 / 0x5A8133); other writers clear or replace
  them (0x5A8260, 0x5A8770, 0x5A8840), so a caller tests for null. Every request is `(*ServerIface)->vt[slot](...,
  OwnClientId)`. `OwnClientId` [0x673344] is a dword (written by 0x5A881F, 0x5A8914, 0x5A82AF; 0x5A80DE zeroes it
  when the server goes); most sites read 32 bits, the chat and trade
  sites its low 16 (0x45F77B, 0x45E36F, 0x462C1F). Chat is `ChatServerIface->vt[0]` SubmitChat(CHAT_ID from, size,
  rdl, CHAT_ID to, -1) (0x45F77B-0x45F7AB).
- **The slots** were read from Server.dll's files, HDE 0.7.1 and vanilla 1.1 alike: the exported vtable
  `??_7IServerImpl@@6BIServer@@@` (188 slots) holds each method's export; `tests\iserver_test.cpp` checks all 27 of
  `iserver.h` against the test install's Server.dll on every unit-test run. Connect and ReqCargo share one body (slots
  0x3C and 0x74), placed by FLHook's SDK order. CreateNewCharacter is (info, client), StopTradeRequest takes one argument
  (the requester). A direct slot call skips the game's gates: trade's one-pending byte [0x674B00] (0x4E2CC5), save's
  single-player and docked tests, a PopUpDialog the server did not open (`34_`, `35_` checks).
- **The knowledge reveal.** `RevealText` 0x459950 cdecl(FmtStr*): when the knowledge map [0x66D334] exists, each value
  of the text (base 0x42, faction 0x46, system 0x53, zone 0x5A, string id 0x73; nested 0x21 recursed) goes to 0x459210,
  which reads the visit byte (`VisitLookup`), ORs the value's `KnowledgeMap.ini` flags and stores it with `VisitStore`
  (send = 1). The base value goes to Universe::get_base **with no null check** (the zone has one): never pass resolved log
  entries. Each reveal may send a SetVisitedState a server sees. `VisitStore` 0x4C4E00 cdecl(id, u8 flags, bool send)
  **stores** the byte (no OR) unless the stored byte already holds every flag, then sends SetVisitedState(client, {1, id,
  flags}, 9). (`31_` check 6: held; the INFO page never reveals, above.)
- **The base's records.** `BaseRecord` 0x43A8D0 and `MarketOf` 0x43A510 are thiscall on `RoomInfoOwner` (0x668708) with a
  base id, ret 4: the record P (`BaseRecord`; the job board's offers mirror at +0x10 -> +4 -> +4), P + 0x18 (0x43A630, the
  news) and P + 0x30 (`MarketOf`, the market; a good's MarketGoodInfo through 0x437D90(market, good), stock +0x1C). An
  unknown id logs "Could not find base id" and reads address 4: a crash. Call them only with the docked base. (`31_`
  check 4 held; `32_`'s "the map node" was wrong: 0x43A510 returns the market inside the record. MarketGoodInfo's +4,
  +0x14 and +0x18 were `32_`'s: verified in game since, "Screens: the dealers" below.)
- **The dealers' sends** (no window, no change to the client's cash): `SendGoodBuy` 0x438220 and `SendGoodSell` 0x438270
  stdcall(base, good, count) ret 0xC (GFGoodBuy {base, 0, good, count}, GFGoodSell {base, good, count});
  `SendAddCargo` 0x4C4230 cdecl(arch, count, health) = ReqAddItem(arch, the cargo bay, count, health, unmounted) and
  leaves the client's list to the server's answer; `SendRemoveItem` 0x4C4260 cdecl bool(u16 id, count) sends ReqRemoveItem
  and removes the item from the client's list (0x672960) at once (then [0x673384] + 1 and 0x446BE0 / 0x447850, unread); `SendCashAdd` 0x4C4D30 / `SendCashSub` 0x4C4D50
  cdecl(amount) = ReqChangeCash(+ / -amount). The window's own buy is 0x47EAB0, its sale 0x47EEA0. (Run in game since
  for a commodity and a battery bought and a commodity and a shield sold: "Screens: the dealers" below.)
- **The save list.** `CharSession` 0x67E7B8 is a static object (`mov ecx, 0x67E7B8`, never a pointer read), `this` of
  `CharListFetch` 0x5AA680 (clears the list +0x1B8, CharacterInfoReq(client, 0), blocks in 0x5A89B0 for the answer; 0
  when ServerIface is null) and `CharDestroy` 0x5AA8F0(&CHARACTER_ID) ret 4 (DestroyCharacter, then the same wait).
  Who constructs the object and whether it is live in space: not read.
- **The pause count.** `PauseCount` 0x667D54 (int), written only by 0x42D6F0(bool on) (read by 0x42D730 and GamePaused
  0x42D74A): the first +1 pauses single player
  (0x428550), the last -1 resumes (0x428580); a window's close (0x4A7690) passes 0.
- **Left unsigned:** 0x43A630 (the news record; no probe needs it yet). (0x47F040 and MarketGoodInfo's fields: the next
  section, verified in game.)

## Screens: the dealers, verified in game (audit 78_, 12 Nov 2026)
The commodity trader and the equipment dealer were reproduced with no window by the test verb `dealprobe`
(`core\game\dealers_probe.cpp`, behind the test switch) and compared with the game's window on the same saves, single
player, HDE 0.7.1 release build, Manhattan (run `research\experience\runs\dealprobe_20261112_1955\`; the reads:
the audit's notes 78a and 78b, its results 78, outside the repository). **Verified in game**
unless marked. Multiplayer: not run (every action stays on the game's button there).
- **Objects.** The window pair: NN_Dealer `W` (vt 0x5D192C; a control named `NN_Dealer`; +0x978 mode 1 commodity trader,
  3 equipment dealer, 2 the ship dealer's final stage; +0x3A4 the selected dealer row, -1 none; +0x988 the quantity;
  +0x32C its class filter; +0x990 the standing taken when it is built) and the player's NN_Inventory `P` = W+0x974
  (P+0x360 = W; +0x32C the filter 0 weapons, 1 ammo, 2 external, 3 cargo bay, 4 commodities; +0x330 its ship model, whose
  +0 is the ship viewer; +0x34C a sorted copy of the client list; +0x394 the hardpoint name chosen for a purchase; +0x3A8
  the visit's snapshot, below). The handler 0x4822A0's BuySellTradeButton case sells (0x47EEA0) when W+0x3A4 == -1, else
  buys (0x47EAB0), both thiscall(P, count = W+0x988) [dis, held by the runs].
- **The market.** `MarketOf` -> `MarketGood` 0x437D90 thiscall(market, good) -> MarketGoodInfo: +4 float price (already
  the base's: Water 15 = goods.ini 30 x the market line's 0.5), +0x14 float level required, +0x18 float standing required
  (-1 none), +0x1C int stock (0x7F000000 for every good at Manhattan: unlimited; the dealer's code lowers / raises it
  itself), +0x20 float factor (1.0 for all 7 goods read), +0x24 byte the row's availability, written by the window's row
  build (unset before any window: 25, 224, 160, 148, 15 were read). The market is an MSVCP60 map (market +4 its head; node +0xC good id, +0x10 the
  MarketGoodInfo); the rows walk it in ascending good id.
- **The price.** `MarketGoodPrice` 0x438060 = (float)(int)(MGI+4 x MGI+0x20), clamped to 9,999,999, below 0 -> 1.
  `DealerPrice` 0x47F040 thiscall(W or P, good) ret 4: `this` is either window (a byte +0x35C = 1 marks the dealer, which
  then uses its +0x974); it returns that price plus a launcher's free ammo (GoodInfo +0xB8 / +0xBC) at the ammo's price,
  capped by the ammo's stock, the hold room after the launcher (the window's hold copy) and MAX_PLAYER_AMMO (50) minus the
  held stack: Javelin launcher row 750 = 500 + 10 x 25. Reproduced from data to the credit on every row of both dealers.
  Standing and level never change a price.
- **The rows.** Market entries with stock > 0 and GoodInfo +0x4C = 0 (trader) / 1 (equipment dealer), the latter of the
  filter's class (`DealerClassOf` 0x47E460 cdecl(arch): 0 weapons, 1 ammo, 2 external, 3 cargo bay, 4 commodity, 5 not
  listed). Order (stable: ties keep the map order): the trader by name (`_wcsicmp`); the equipment dealer by availability
  (available first), the equipment's type (vt +0x10), a launcher / munition flag, then price. Availability = 0 when the
  standing or the single-player level falls short, when no hardpoint takes it (filters 0 / 2), or for ammunition (filter
  1) when no held launcher, mine dropper or CM dropper (types 0x11-0x13, mounted or not) fires it (0x482D80). An
  unavailable ammo row is still buyable (0x4837A0 does not test it). Equal row for row: trader 8, weapons 18, ammo 31,
  external 7, cargo bay 2.
- **Buying** (0x47EAB0, the cargo-bay routes run directly): count capped at the stock, the stock lowered, `SendGoodBuy`
  (base, good, count), `SendAddCargo`(arch, count, 1.0) (the equipment dealer then QueueNN("loaded_into_cargo_hold");
  a single cargo-bay item other than nanobots / batteries goes in mounted by 0x4C4200, a hardpoint item by 0x4C41D0 onto
  P+0x394's hardpoint), `SendCashSub`(DealerPrice x count); for a launcher a second GFGoodBuy and ReqAddItem of its ammo,
  whose count is capped by its own hold rule, which can differ from the price's [dis, not run]. The buy itself checks
  nothing else: the refusals are the window's, below. Water x1 and a shield battery x1 bought directly equalled the
  window's purchase in cash, list, stock and the window's rows afterwards.
- **Selling** (0x47EEA0): count capped at the stack, the stock raised, `SendCashAdd`(credit), `SendGoodSell`(base, good,
  count), `SendRemoveItem`(id, count) (a weapon also leaves the HUD's weapon groups first, 0x4F6380: not run directly).
  **The credit**: full (ftol(price x count)) for a commodity, a combinable good, or an item NOT in the window's snapshot
  P+0x3A8; reduced, ftol((price x 0.3 + 0.499999) x health x count), for an item IN it. The snapshot is a copy of the
  client list taken when the **window is built** (0x479D10 message 0x48, sent by 0x477AB0 at the opening), not at
  docking: so equipment owned when the window opened sells used, an item bought in the same opening sells back at full
  price (a used shield, health 0.6, held at the opening: +180 both ways = (0.3 x 1000 + 0.499999) x 0.6; a Lavablade
  bought and sold in one opening: 500 paid, 500 credited). `32_`'s first draft had this inverted. The player-side row's
  shown price uses another constant (0.49999, health before the 0.3) [dis]: compare credits, not the row text.
- **The refusals** (the window hides BuySellTradeButton, the slider and the quantity and shows ErrorHelp for every one:
  none is only informative). Buy, from `DealerBuyMost` 0x4837A0 thiscall(W, good, int* reason) ret 8 then 0x47FEF0:
  no ship 3 (IDS 1152, not run); credits / price < 1 -> 2 (1151); the hardpoint path (filters 0 / 2 with a free
  hardpoint: at most 1, the hold not asked); else room = ftol((hold - occupied cargo) / volume) (volume 0 -> the stock),
  at most 1 for filters 0 / 2 / 3 except nanobots / batteries, which stop at Ship +0x140 / +0x144 minus the held stack
  -> 9 / 10 (932 / 933), ammunition at MAX_PLAYER_AMMO minus the held stack -> 11 (975), a hold room < 1 -> 1 (1150);
  then, after 0 or 2 only, the level (single player, the level 0..998 and below MGI+0x14, equal passes) -> 5 (1563),
  else the standing W+0x990 below MGI+0x18 -> 6 (458782); then for filters 0 / 2 `DealerHardpointFor` 0x585F20
  thiscall(viewer, good, any) ret 8: no hardpoint takes it -> 7 (1591, overriding 2 / 5 / 6), none free (with a
  quantity) -> 8 (1599). Sale: mission cargo (EquipDesc +0x18) -> 1397, an engine -> 1324 (not reached: no engine is in
  any filter's list), nothing else (a mounted item sells: it is unmounted by the sale). Each was brought about in game
  and the data's answer equalled the window's (1151, 1150, 1563, 458782, 932, 933, 975, 1591, 1599, 1397's hidden
  button); volume-0 goods (ammo, nanobots, batteries in HDE) are never stopped by a full hold.
- **The level the window tests** is `ClientRank` [0x67338C], the client's copy, which the window resets to -1 and re-asks
  the server for when it is built (0x4C55F0); before any window it is stale (0 on a fresh start of a rank-2 save; after
  a window, a later rank-0 save kept 2). A route with no window takes the server's rank (pub::Player::GetRank). The
  standing is `BaseOwnerStanding` 0x483DF0 cdecl() -> float, the player's feeling toward the group of the docked base's
  system object (1.0 if none; 0.65 at Manhattan in SaveC701); lowering li_p_grp alone (Manhattan's own) to -0.3 brought the
  refusal (an independent check's run).
- **Not synchronous in single player.** When the direct calls return, only what the client edits itself has changed
  (the stock; a sale's item removal by `SendRemoveItem`); cash and a bought item arrive 1-2 frames later through the
  server's answers (the window's own purchase shows the same: stock at once, the rest within 15 frames). A page re-reads
  cash and the list on a later frame, never straight after its call.
- **Not run**: multi-unit direct buys, a launcher with its free ammo, hardpoint (mounted) direct buys, a weapon's direct
  sale (0x4F6380), the no-ship refusal, the engine refusal, multiplayer.

## Screens: the NeuroNet text, verified in game (audit 81_, 12 Nov 2026)
The mission log (NeuroNetStoryStar), the job board (NN_MissionVendor) and the news (NN_News) were reproduced with no window
by the test verb `nnprobe` (`core\game\neuronet_probe.cpp`, behind the test switch) and compared with the game's windows on
copies of SaveC701 (Manhattan), single player, HDE 0.7.1 release build, with HUD 2 and with the classic HUD (run
`research\experience\runs\nnprobe_20261112\`; the reads: the audit's notes 81a, 81b and
81c, its results 81, outside the repository). **Verified in game** unless marked [dis] (read in the disassembly).
Multiplayer: not run (every action stays on the game's button there).
- **The log's store and text.** `MissionLog` [0x672334] is a circular std::list's sentinel, the count at +4; node +0
  next (newest first), +8 the entry's FmtStr, whose values are string ids (resolved when added, 0x4BA5A0), so it is
  formatted with `LogFormat` 0x4BA860 cdecl(FmtStr*, out, max, allowM), never with FmtStrFormat. Text starting with '*'
  is a LOG-tab entry (shown without the '*'), the rest MISSION; an entry with no ':' gets IDS 1596 "PERSONAL ENTRY:\n"
  (LOG) or 1574 "OBJECTIVE:\n" (MISSION; 987 for death mode's first entry [dis]); "\n\n" between entries; 1039 only when
  the list is empty [dis]. Both tabs composed so equal the window's rich text (`StoryStarHistoryText`, its display list
  at control +0x488 through BinaryRDLWriter) word for word, the title (`StoryStarTitle`) is 1552 in single player. The
  window opens on LOG while `RouteList` is empty, not in death mode and in single player, else on MISSION (both seen). It
  rebuilds when [MissionLog + 4] differs from its +0x338 (seen: 17 -> 8 after an acceptance, the window open). The
  current objective's highlight (style 0x3D0A, `wcscmp` of the two texts formatted without %M) [dis]: not reached.
- **The job board's store.** RoomInfoOwner's map (+0x24 its head; node key +0xC, value +0x10, the record at value +4) is
  walked by key: `BaseRecord` on an id not in it reads address 4. The offers list is the record itself: {+4 head},
  nodes {+0 next, +8 MissionVendorProperties*}, null-terminated, complete when record +0x14 is set; the window copies it
  in this order (+0x394, stride 0x60, count +0x398, first visible +0x370, selected +0x378). Properties: +0 key, +8 a
  per-load serial and +0xC the vendor kind 2 (MissionResponse's two arguments), +0x10 type, FmtStrs +0x14 destination,
  +0x2C title, +0x44 full text (difficulty and reward inside), +0x5C reward; values are hashes: FmtStrFormat. The rows'
  and the detail's texts equal the window's controls (`MissionVendorSummaryDest|Title|RewardN`,
  `MissionVendorMission{Dest,Title,Reward,FullText}`); the reward is 1857 "TBD" for 0, else 1842 "$ %d0" with thousands
  commas. **The offers are drawn anew at every load of a save**, inside one game process too (11, 10, 9, 12 offers; new
  keys; the serial back to 1).
- **The news' store.** Record +0x18 is the news list (same node shape, complete +0x2C); the window orders it in four
  passes (byte +9 set, dword +0xC == 1, byte +8 set, the rest) [dis; on SaveC701 all fall in the last pass]: rows
  (`NewsHeadlineHeadlineN`, 5), the story's headline (`NewsStoryHeadline`) and text (`NewsStoryText`) equal FmtStrFormat
  of +0x28 / +0x40. The keys are renumbered at each load; the stories are the same.
- **The reveal (RevealText 0x459950).** The board reveals its row's +0x14, +0x2C, +0x44 and the news its story's +0x28,
  +0x40 **when the window opens (the preselected first row) and at each selection**; scrolling reveals nothing. The
  change to `VisitMap` is exactly the game's knowledge map's (`KnowledgeMap` [0x66D334], by ids number: the FmtStr's own
  ids, `s` values and the ids_name of `B` / `S` / `Z` / `F` values; each entry {id, flags} ORed in where a flag is
  missing): opening the board, selecting a row natively and calling RevealText directly on the same texts gave the
  predicted ids and flags every time (zones 33, factions 65), and a repeat changes nothing. Guards for a direct call:
  KnowledgeMap non-null (it is 0 in the menus and loading: the call then does nothing), every `B` / `S` value a known
  base / system and every `!` value non-null (Universe::get_base / get_system's result is used untested [dis]). HDE's
  news texts have no knowledge-map entry (news.ini's 768 ids, none among knowledgemap.ini's 2,283 keys), so a news reveal
  changes nothing with HDE's data.
- **The ACCEPT button** sends IServer MissionResponse(+8, +0xC, true, OwnClientId) and nothing else: its callback +0x33C
  is 0 at every opening; the window's only gates are a selected row and a board with offers (with no selection the
  button sends nothing: no answer comes). "Already have a mission" (IDS 1840) and the empty-board reasons are the
  server's. **The answer is not synchronous**: `MissionAcceptRecv` 0x4379B0 (stdcall(u32, packet, size), the receive
  table's entry; MissionVendorAcceptance +8 accepted, +0xC the reason) runs 2 frames after the request, for the direct
  call and the button alike. Accepted: the offer leaves the mirror, the log loses its MISSION entries and gains the job's
  text, the open board closes itself (0x48D440); refused: the board shows 1839 "Mission Rejected", the heads cleared, the
  reason as its text, the selection -1. With no board open the answer is dropped by the board's code (0x48D4A0) and the
  rest happens the same.
- **HUD 2's borrowed board** (basepage) picks row 0 once per opening (13 Nov 2026; until then whenever the selection was
  below 0, which hid the refusal's reason at once and revealed row 0 again): after a refusal the selection stays -1 and the
  reason stays shown, as on the classic HUD, until a row is picked (the D-pad from -1 starts at row 0).
- **Not run**: the bar's offers and rumours (the dialog setter 0x4A6630 [dis]), death mode, the objective highlight, the
  21660 entry swap, an empty board, multiplayer.

## Screens: the menus, verified in game (audit 82_, 12 Nov 2026)
The in-game load / save window `NN_LoadSave`, the pause count, the options and the load were reproduced with no window by
the test verb `menuprobe` (`core\game\menus_probe.cpp`, behind the test switch) and compared with the game's own window on
the same saves, single player, HDE 0.7.1 release build, Manhattan / free flight (runs
`research\experience\runs\menuprobe_20261112_2217\`, the independent check `menuprobe_check_20261112_2238\`; the reads:
the audit's notes 82a, 82b and 82c, its results 82, outside the repository).
**Verified in game** unless marked:
- **The save list needs no window.** `SaveList` (CharSession +0x1B8) holds every save the window lists, in the window's
  order (newest first, not by name): the window skips the Restart record (+0x200), shows at most 256 rows, column 1 the
  description (the IDS at +0x224 when set: the autosave's 1628 "AUTO SAVE", a mission save's; else the typed text at
  +0x218), column 2 the time = Common.dll filetime_to_wstring of +0x228 (the user's short date, a space, 24-hour HH:mm,
  no seconds, no UTC conversion: the server stores local time). **+0x234 is the money and +0x270 the rank** (34_ had them
  the other way round; checked against the files' [Player] lines), +0x260 the system, +0x230 the ship. 235 rows EQUAL
  line by line in both the load and the save mode.
- **Save, overwrite, delete with no window = the window's.** A new save is `IServer::SaveGame` (vt+0xE8, thiscall:
  `&CHARACTER_ID` all zero, the description, OwnClientId), then `CharListFetch`: the server names the file `Save%04x` of
  `rand()` (random, retried while the name exists) and the next fetch lists it at once (no restart needed for a file
  the game itself saved). An empty description becomes "save" (IDS 1629) + one more than the highest "save<N>" row
  (the window's numbering continued the probe's), cut at 30 characters. An overwrite is the same call with the row's own
  CHARACTER_ID and its own description (the window first asks NN_Confirm IDS 1798): same file, newer tstamp, no new file.
  A delete is `CharDestroy` (IServer DestroyCharacter) then the fetch. The window's row click is its own
  `vt[0x5C](0xF001, 0, row)` (p3 = the row, p2 unused; 0x487CB0 fills the panel and, in save mode, the edit field with the
  row's description). In SP the list fetch's wait returns at once (the server answers inside the call; read, 82a 5).
- **The gates.** The window is reached only through the pause menu (0x4A7210 codes 2 / 3), so its gates are the pause
  menu's: it opens only with PauseCount 0, UiBusy and WorldLoading clear (0x4A75C0); it offers Save only in single player,
  docked (`DockedBaseId` 0x66873C non-zero) and with no scene running (`ScriptsIdle` 0x4568A0); the window greys SAVE and
  DELETE on the autosave row. Seen: docked codes 1-5; in space 1,3,4,5; during the launch animation Save offered; during
  the docking cutscene not; with the game's Options open (UiBusy, a count) the pause menu does not open. The probe's
  refusals agreed in every state. **At the front end `DockedBaseId` keeps the last base's id**, so "docked" there must
  come from the game mode, not this byte. The server's SaveGame also needs a docked player (+0x3E4 = 0: read, not run).
- **The load.** `LoadGame` 0x5B2C70(record, record +0x201) **in game** leaves exactly what the window's load leaves (the
  landing room 1 character, the Bar 7, pause count 0, cursor shown, no window left), with nothing open, with a pause count
  taken and released after the call (as the window's 0x4880F0 does with LeaveModal) and from space. With a count still
  held the next room change waits until it is released, then the rooms fill normally. **From the front end (main menu)
  the same call brings every room up without characters** (pause count 0 before and after), and afterwards even loads
  through the window stay empty until the game restarts: the front end loads through its character select (0x56C360 ->
  0x5743A0 -> CharacterSelect), never LoadGame. This is the failure recorded on 17 Oct 2026 (12e55943).
- **The pause count.** A window's count = EnterModal 0x4A7670 (+1, cursor shown, returns the cursor's old state) /
  LeaveModal 0x4A7690(that state) (-1, no clamp): the same PauseCount, GamePaused and cursor as the pause menu open (1, 1, 1).
- **The options.** `OptionList` 0x67ED00 (+4 the head; option {vt, +4 name, +8 ids, +0x10 pending, +0x14 value}) holds the
  36 values of `SAVE\PerfOptions.ini` (EQUAL line by line). Setting one as the screen does (pending := v; vt[0](v); value
  := pending and vt[1](v) when it changed) and `PerfOptionsWrite` 0x5B3EC0("PerfOptions.ini") wrote a file byte-identical
  to the Options screen's own (toggle, close, YES on its keep / revert question); only that line changed. No restart rule
  was found in the code (read: every option applies at once; resolution with its 10 s revert countdown).
- **Not run / open:** multiplayer (no Save / Load there: read); the front end's own Load window (character select:
  delete and load stay the game's); Alt+F8 USER_SAVE_GAME (no handler found: read); the identity of the scene holder's
  flags; after a direct load from space one Bar frame still showed a space comm subtitle (the window's load from space
  did not, but nothing was being said then: unexplained); one unexplained game exit in free flight after read-only
  commands (run 1).
