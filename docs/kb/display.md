# Display modes, resolution and DPI

How HD Edition 0.7.1 chooses the window mode and resolution, what DPI scaling does to the game, and how FLvibed's test
instances switch them (sweep section `display-matrix`, `tools\fldata.ps1 Set-FLInstanceDisplay`). Started 21 Oct 2026.

## Where the settings live
- **Resolution:** `PerfOptions.ini` `[Display] size= W, H` (in the install's `SAVE\` with flplusplus
  `save_in_directory`; the game writes it back on exit) `[data]`. FL# (`FLSharp.ini better_resolutions = true`) builds
  the resolution list from the main monitor's modes.
- **Window mode is baked into `Freelancer.exe`** by HDE's installer (`Process_DisplayMode` in its `mod_options.iss`,
  github.com/FLHDE/freelancer-hd-edition-installer, read 21 Oct 2026) `[community, verified against the install]`:
  - file offset `0x1B16CC` (code `0x5B16C8`, `mov byte ptr [esp+79h], 0|1`): `01` fullscreen (the vanilla default),
    `00` windowed; the installer's comment says the `-f` command-line flag still forces fullscreen;
  - file offsets `0x2477A` and `0x2490D` (code `0x424777` / `0x42490A`, `push 02CA0000h` = WS_CAPTION | WS_SYSMENU |
    WS_MINIMIZEBOX | WS_MAXIMIZEBOX): the two high bytes `CA 02` give a bordered window, `00 00` the borderless one.
  - Nothing in `dxwrapper.ini` (its d3d9 wrapper is off in the DxWrapper + d3d8to9 setup; `d3d8.dll` = d3d8to9,
    `d3d9.dll` = ReShade), nothing in `freelancer.ini`, no registry key.
- **Game facts:** the fullscreen flag at `0x6679F0` (written at `0x424891`, read by the mouse capture at `0x424C33` /
  `0x424C60`); render size `0x610850` / `0x610854`; window size `0x679BC8` / `0x679BCC` (`..\FREELANCER_RE.md`).

## DPI
- HDE's exe carries a DPI-aware manifest (HDE changelog) `[community]`: at any Windows scaling the game renders at the
  physical size, so a 200 % desktop and a 100 % desktop give the same back buffer (3840x2160 here). FLvibed reads
  nothing from desktop metrics (no GetSystemMetrics / GetDpi* / MonitorFrom* calls, audited 21 Oct 2026): placement
  comes from the back buffer (`render::ScreenSize`) and the game's UI camera.
- **A DPI-unaware game process** (an exe without the manifest, or `__COMPAT_LAYER=DPIUNAWARE` in its environment, which
  is how the test runs it without changing any Windows setting) sees a logical desktop: at 200 % on 3840x2160 the
  game gets a 1920x1080 desktop, FL# clamps the resolution to it and the back buffer is 1920x1080, stretched by Windows
  to the screen; `GetDpiForWindow` 96, awareness 0 `[game, 21 Oct 2026]`. Both modules answered in that state. An
  aware process reading the unaware window's rects gets them scaled to physical pixels.

## Verified (display-matrix, 21 Oct 2026) `[game]`
| Configuration | Back buffer | Window | Result |
|---|---|---|---|
| borderless 3840x2160 | 3840x2160 | = client | HUD 28 items inside, pad flies, subtitle in place |
| windowed 1600x1200 (4:3) | 1600x1200 | 1606x1251 (border) | same |
| windowed 2560x1080 (21:9) | 2560x1080 | 2566x1131 (border) | same |
| borderless 1280x720 | 1280x720 | = client | same |
| DPI-unaware process | 1920x1080 | 3840x2160 as read by an aware process | same; a PrintWindow capture by an aware process shows the buffer in the top-left quarter |

Seen on the way: a bordered window asked to be 3840x2160 on a 3840x2160 desktop gets a 3840x2139 client (Windows
fits the frame on the screen) while the back buffer stays 3840x2160; a size line the game cannot parse falls back to
the desktop size with FL#'s `better_resolutions` on and to 1024x768 with it off; ReShade's "update available" banner
appears in the first seconds of a windowed start (ReShade's own, not FLvibed's).

## Subtitles across modes (`display-subtitles`, 21 Oct 2026) `[game]`
Real dialogue, judged by the module's own `layout` report (debug command: screen, font and row metrics, the placement
context and anchor, every block's top / height / widest row) and screenshots (`research\experience\runs\display_subs_*`):
Juni's room scene on Manhattan (letterboxed), the M01a opening cutscene in space (cutscene camera, letterboxed), King's
flight comms; at 1600x1200, 2560x1080, 1280x720 and 3840x2160.
- **Everything scales with the height, nothing with the width** (as designed): font 43 / 39 / 26 / 78 px = 3.6 % H;
  rows 0.98 of the font's line height, the name row 1.02 of the condensed name font, 0.35 of a row between speakers;
  rows at most 0.5 W wide in 4:3, 0.3 W in 21:9 (cap 0.8 W). Legible at 720p.
- **Flight since 21 Oct 2026: the top panel** (`space_position 2`) at 16:9 / 16:10 / 21:9 - the maneuver bar moved to
  the top left, one sliding HUD panel at the top centre between it and the NeuroNet box (its room: 0.80 H half at 4K
  16:9 less the gap, wider at 21:9); 4:3 / 5:4 have only ~0.18 H of room there (rows of one or two words): they use the
  name-tab layout (`space_narrow 5`, the user's pick): the name in a tab at the top, the rows in a body below the top
  HUD row under the bar's lower right (subtitles\docs\DISPLAY_PLAN.md).
- **Flight band (`space_position 1`):** hangs from 0.162 H (0.235 H under the objective banner), ends at 0.26-0.31 H, well above the 0.44 H
  limit; clear of the comm portrait (top left), the maneuver bar and the bottom panels in every aspect ratio.
- **Letterbox bars are 0.140 H each at every aspect ratio** (the game letterboxes by a fraction of the height, not to
  a cinema ratio; measured in the room-scene captures). Room scenes (base context, letterbox flag) and space cutscenes
  (cutscene camera) both place the stack with its bottom at 0.975 H:
  - a name row + one text row (0.875-0.975 H) sits inside the bar;
  - a name row + two text rows (0.828-0.975 H) reaches 0.03 H above the bar: the name and the top of the navy panel
    lie over the picture's bottom edge (visible in both the room scene and the space cutscene);
  - a second speaker still showing (room scenes: Juni and the diplomat answer each other) stacks above the first at
    0.71-0.86 H: entirely over the picture, with its navy backing across the actors. This is the flight design (two
    speakers, TLOU2 queue) carried into letterboxed scenes; DISPLAY_PLAN's intent was "inside the lower letterbox bar".
  **Built 21 Oct 2026 (906c8ef, verified in Juni's scene at 4K):** the bar is read from the game's own math each frame
  (`exe::LetterboxAmount` 0..1 and `exe::CinematicAspect` K: bar = floor((H - W / (A + (K - A) * amount)) / 2 + 0.5);
  the game computed 303 px at 2160 = the measured 0.140 H), so bars removed by a mod give 0 and the plain placement.
  JFLP writes K per resolution (1.853 / 2.471 / 3.295 at 4:3 / 16:9 / 21:9), which is why the bars are 0.14 H
  everywhere; the vanilla exe has 1.85. `letterbox_mode` 0 = inside the bar, inline name, no backing (one 2-row line
  spans 0.881-0.975 H); 1 = resting on the bar's top edge in the picture with the name row and backing (Outlaws);
  `cinematic_single_speaker` = one speaker at a time in letterboxed scenes. **Default: bar mode (user, 21 Oct 2026).**
  What other games do: `..\..\subtitles\docs\SUBTITLE_PRACTICES.md`.

## Test instances
- An instance mirror hard-links `EXE\Freelancer.exe` to the install's; `Set-FLInstanceDisplay` replaces it with a copy
  before patching (the install keeps the user's mode) and refuses an exe whose bytes at the three sites are not the
  0.7.1 build's. The main install (instance 0) is never changed: a serial sweep runs the matrix on instance 9.
- Exclusive fullscreen takes the screen: the matrix includes it only with `flsweep.ps1 -Attended`.

## Device Reset (26 Oct 2026)
- HD Edition's `d3d8.dll` (d3d8to9 inside the game folder): after one `CopyRects` from the back buffer to a system-memory
  surface, every later device Reset fails D3DERR_INVALIDCALL for good (the game's own recovery retries every second and
  never comes back: a mode change or Alt+Tab out of fullscreen hangs it). FLvibed reads the back buffer only on the
  system's own Direct3D 8 (`core\game\render.cpp` ReadbackSafe). Vanilla 1.1 does not retry a failed Reset at all (one
  D3DERR_DEVICELOST and the game hangs: the shape of the FLUF Alt+Enter freeze). Test-only host command `d3dreset`
  (`controller\docs\HARNESS.md`); evidence `research\experience\runs\pad_fluf_popup_2026-10-26\run.log`.

## Window mode path (26 Oct 2026, read from the code and verified in game) `[game]`
- The game creates its window at 0x5B32E5 (0x4246E0: `CreateWindowExA`, the style from the display settings' fullscreen
  byte: WS_POPUP or the windowed style; the HWND at `0x6679F4` and `exe::MainHwnd` 0x67ECA0) **before** 0x5B1300 loads the
  DACOM libraries (FLvibed, FLUF): a DLL cannot change the window's DPI awareness (fixed at creation); the exe's own
  awareness (HDE's manifest, the HIGHDPIAWARE compatibility flag, `__COMPAT_LAYER`) decides.
- **Every mode change goes through SetupScreen 0x4249C0** (`exe::SetupScreen`, `bool cdecl(HWND, request*, applied*)`):
  the start (0x424DD0, three fallback sizes), the Options screen and the restore path (0x424CE0 with the applied
  settings `exe::DisplaySettings` 0x679BC0), FLUF's Alt+Enter (a copy of 0x679BC0 with +0x25 flipped, applied = 0x679BC0).
  The settings: 0x28 bytes, +8 width, +0xC height, +0x25 fullscreen. Windowed: a request larger than the desktop is
  clamped (width x 3/4 width). The renderer's mode call (vt+0x30 of [0x5C6DE0]) writes what it applied into `applied`;
  then WindowStyle 0x424870 (`cdecl(HWND, bool fullscreen, w, h)`: render size 0x610850/54, fullscreen flag 0x6679F0;
  fullscreen = WS_POPUP + topmost at least the desktop's size, windowed = the style immediate at 0x42490A +
  AdjustWindowRectEx + `SetWindowPos(HWND_NOTOPMOST, size only)`). The applied settings feed the mouse capture (0x41E9EB,
  0x4201BF) and the window procedure's activation (0x5B2671): they must stay the real mode.
- HDE 0.7.1 writes the applied size into `SAVE\PerfOptions.ini` at the setup (vanilla at its exit).
- **FLvibed's borderless** (`borderless` 0 / 1 auto / 2 always; `controller\src\host\winmode.cpp`, decisions
  `core\winmode_rules.h`): auto stays out when the windowed style immediate has no caption (HDE: 0) or the d3d8.dll is
  not the system's (HDE's d3d8to9 + DxWrapper, dgVoodoo). Otherwise every request becomes windowed at the monitor's
  size and, after WindowStyle, the window becomes WS_POPUP at the monitor's rectangle (centred when smaller). A request
  whose fullscreen byte differs from the applied one is a toggle (FLUF's Alt+Enter): borderless <-> the game's bordered
  window. Verified (`research\experience\runs\borderless_2026-10-26\`, release, background):

| Configuration | Provider | Window / render |
|---|---|---|
| vanilla 1.1, DPI-unaware, PerfOptions 1280x960 | flvibed | 1920x1080 logical = the whole 4K monitor, style 0x96000000, render 1920x1080 |
| vanilla 1.1 + FLUF order A / B, DPI-aware (`__COMPAT_LAYER=HighDpiAware`) | flvibed | 3840x2160 = client = render; two Alt+Enter setups: bordered 3846x2190 (client 3840x2139) -> borderless, Resets D3D_OK; M01 in space, HUD 2, FLUF's Debug Window in place |
| vanilla 1.1 + FLUF order A, unaware | flvibed | 1920x1080 logical, the toggles the same |
| vanilla 1.1, `borderless 0` | off | the game's bordered 1286x989 (client 1280x960) as before |
| HDE 0.7.1 instance 1 (borderless exe) | game | untouched 3840x2160, style 0x14000000 |
| HDE 0.7.1 instance 9 (bordered exe, 1600x1200) | wrapper (d3d8.dll, DxWrapper) | untouched 1606x1251 |
| HDE instance 9, `borderless 2` | flvibed | 3840x2160 borderless; HDE stored 3840x2160 in PerfOptions (restored by hand) |