# Controller input backends and haptics: SDL3, Windows.Gaming.Input / GameInput, DualSense, rumble design
Status: current
Checked against: FLvibed 3c9c1390 (6 Nov 2026, short form: 14 watched-path commits: the module split P0-P11e (moves, comments, map rows), the controller.cpp split; c4652165 device-switch fixes; `pad_backend` ships as a player row (README: not tested with real PlayStation or Switch pads); U30-U32 unchanged)
Dirty paths at check: another session's uncommitted the private work queue, `tools/uimock/options.py` (not judged); 950b25cc, 4a270d9e and 3c9c1390 landed during the round, read
Watch: modules/controller/src/impl/gamepad.*, core/ui/glyphs.*, modules/controller/src/impl/controller*.cpp, core/ui/uikit.cpp, modules/controller/docs/BINDINGS.md, core/pre.h
Evidence: none saved (web pages read 29 Oct 2026)
Sources: a research agent's report (web + repo greps); the coordinating session checked SDL3's Windows support on
https://wiki.libsdl.org/SDL3/README-windows and the repo facts. Tags: [page], [file], [assumed]

## What it means for FLvibed

*6 Nov 2026 (3c9c1390, short form):* no backend or haptics rule changed. `pad_backend` left the developer keys with 0.2.0: an options row, `docs\SETTINGS.md` and the README, marked experimental and not tested on real PlayStation or Switch pads (U61, adopted for 0.3.5 by 4cab3e0f: SDL3 falls back when no XInput pad is seen; since 9c2bd746 native first: SDL reads such a pad whether or not XInput answers). 0.3.0 and the fixes since were not run on a real pad (HANDOFF), so J8 stands.

*30 Oct 2026 ~11:50 (16df8bd, short form):* the Q5 slice 3 line below checked against the code (1893428,
`haptics_rules.h`: kEffects, 4 voices, the maximum per motor, the fatigue limiter over a ~3 s busy share above 0.5 sparing
priority 5+, changes-only renderer at most every 30 ms) and its re-check on the clean build (473ab12: pad-device /
pad-sdl / pad-haptics PASS) - it stands. Not verified: the real popup source in game (only the debug injection), the
feel on a real pad (J8). U29 slice 2+, U30's per-category sliders and the rest of section 4's table, U31 / U32 open.
c79fc35 / d488fbd touched watched files (`core\pre.h`, `controller.cpp`) for A37 and the load reset - no input-backend
or haptics statement.
*30 Oct 2026 (WORK_QUEUE Q5 slice 3, dated line; not a check-up round):* **U30's skeleton built** - the effect table,
the mixer (max per motor, ducking, rate limits, 4 voices, a fatigue limiter at about half the time busy), the silence
rules (the one focus gate, the pad as the device in use, flight only for the world, the game's own pausing boxes as the
one exception to "silent while paused"), a renderer of the two motors over both backends; first effects hits by weight,
shield failure, the hand-back (unchanged), a confirm tick (`modules\hud\docs\HUD_CUES.md` "Haptics"). Section 4's table
beyond those, the trigger / DualSense / waveform renderers and the per-category sliders stay open.
*30 Oct 2026 (aafe41d, short form):* **U29 slice 1 built** (baae629, WORK_QUEUE Q5): SDL3 3.4.18 MinSizeRel static
in the controller host on its own thread, opt-in `pad_backend` 1 (default 0 = XInput, unchanged), the hints this note
asked for (background events on behind FLvibed's own gate, message loop off, enhanced reports auto, the queue
drained, no `SDL_PumpEvents`) plus DirectInput / RawInput / WGI / GameInput off - **SDL_Init 3-5 ms** (the ~115 ms
measured on 29 Oct was DirectInput's enumeration), +1.21 MB. Verified in game (00b97aa: 180 vs 181 fps, clean stop)
and with `FLVIBED_SIM_WIN7` (4a506a4). With WGI off, Xbox trigger rumble (WGI in SDL) is presumably unavailable [inference] - to
weigh when U30 renders trigger effects. **T36 slice 2** (8e19482): SDL virtual pads in the game record rumble, trigger
rumble and effects, the command stream the tests above ask for. U30's engine and U31 are not built; the game sends no
trigger rumble / effects yet. Open from U29: a real pad on `pad_backend` 1 (J8), Steam Input / DS4Windows detection.
*29 Oct 2026 (04b4c05, new; the owner: actually support every specified controller, not only XInput, with AAA-grade
contextual rumble, trigger rumble and DualSense waveform haptics):* today input is **XInput only**
(`modules\controller\src\impl\gamepad.cpp`, one pad slot - since 30 Oct 2026 its scan in `pad_slots.h` and testable through the device-layer test pads, IDEAS T36 / WORK_QUEUE Q2); RawInput lists devices only to pick glyph families (`glyphs.cpp`
40-85); a DualSense, DualShock 4 or Switch Pro pad works only through Steam Input / DS4Windows. Rumble is one two-motor
pulse with **one caller**, the autopilot's hand-back (`controller.cpp` 1348); the `rumble` setting's help promises
"hits" and "a confirmation request" (`controller.cpp` 2765) that nothing sends - the help overclaims (U30; fixed 30 Oct 2026, WORK_QUEUE Q1: the help names the hand-back pulse only). *Also 30 Oct 2026 (08a99a7):* the pad never vibrates with the
game in the background (a running pulse stops) - U30's "silent on focus loss" holds for today's one pulse; the
mixer and the rest of U30 are not built. The test pads record rumble as slot / motor speeds / heartbeat (e38ca61), the
command stream T36 asks for - U30's effects can be asserted without a device.
- **Backend (U29): SDL3, linked statically**, as the first layer: zlib licence; "All desktop Windows versions, back to
  Windows XP, are still supported" (README-windows, checked) - so Windows 7 stays in, untested by us; DualSense / DS4
  over USB and Bluetooth, Switch Pro, rumble, Xbox trigger rumble (through its WGI driver), DualSense trigger effects,
  gyro, no SDL window needed. The current XInput code stays as the fallback when SDL fails to start. Probes first: the
  size a joystick-only static SDL adds to the DLL; SDL inside Freelancer's process (no `SDL_INIT_VIDEO`, never
  `SDL_PumpEvents` from the game thread, `SDL_UpdateJoysticks()`, `SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP=0`,
  `SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS=1` with FLvibed's own foreground gate; no clash with the game's
  DirectInput); impulse triggers on the owner's Elite by connection (USB, dongle, Bluetooth).
- **A haptics engine (U30)**, data-driven: game events -> an effect library authored once (Apple AHAP-like: transient /
  continuous, intensity and sharpness curves, pan, an optional trigger block, an optional waveform) -> a mixer (~250 Hz,
  priorities, ducking, per-category and master sliders, a fatigue limiter, silence on focus loss / pause / cutscenes)
  -> a renderer per capability (two motors; + impulse triggers; + DualSense trigger effects; + waveform). Never the only
  cue (XAG 110; the owner's HUD rules). Effects keyed on game data (gun class, explosion radius) - Generalization.
- **DualSense extras (U31)**: trigger effects through `SDL_SendGamepadEffect`; **waveform haptics are not in SDL** -
  over USB the pad is a 4-channel audio device whose rear pair drives the voice coils (WASAPI on the endpoint sharing the
  HID device's ContainerId); over Bluetooth only reverse-engineered (experimental). Sony's own PC ports need USB for
  full haptics.
- Tests: log every backend command with a timestamp; unit-test the mixer on scripted event streams (host-free
  `tests\`); golden files per renderer; feel, fatigue and panning stay human (attended cards, `test-automation.md`).

## Findings

### 1. Backends
- **SDL3** [page]: supported platforms "Windows XP and later" (https://wiki.libsdl.org/SDL3/SupportedPlatforms); WinRT /
  UWP / Windows Phone removed. Static: `SDL_STATIC=ON`, `SDL_SHARED=OFF`, `SDL_STATIC_VCRT` for /MT
  (https://discourse.libsdl.org/t/sdl-build-sdl-with-the-static-c-runtime-on-visual-studio/48550). Size not verified
  (one reduced DLL 6.85 MB: https://discourse.libsdl.org/t/2x-larger-sdl2-dll-file/41842; a joystick-only static link
  should be far smaller - measure). `SDL_RumbleGamepadTriggers` Xbox One-class pads only
  (https://wiki.libsdl.org/SDL3/SDL_RumbleGamepadTriggers); `SDL_SendGamepadEffect` takes the device's own packet
  (https://wiki.libsdl.org/SDL3/SDL_SendGamepadEffect); for the PS5 driver `DS5EffectsState_t`, 47 bytes, right trigger
  at offset 10, left at 21, 11 bytes each (https://github.com/libsdl-org/SDL/blob/main/src/joystick/hidapi/SDL_hidapi_ps5.c).
  Bluetooth PlayStation rumble / effects need "enhanced reports" - one-way per power cycle and breaking DirectInput for
  other apps; use "auto" (https://wiki.libsdl.org/SDL3/SDL_HINT_JOYSTICK_ENHANCED_REPORTS). SDL's emulated-rumble mode
  disables the voice coils. `SDL_INIT_GAMEPAD` pulls joystick + events only; detection runs on SDL's own thread with a
  hidden window (`SDL_HINT_JOYSTICK_THREAD`). Known pitfall: RawInput-to-XInput correlation can delay rumble until the
  first press (https://github.com/flyinghead/flycast/issues/802) - keep `SDL_HINT_JOYSTICK_RAWINPUT` switchable.
  Feature detection: `SDL_GetGamepadProperties` (`SDL_PROP_GAMEPAD_CAP_RUMBLE_BOOLEAN`, `TRIGGER_RUMBLE`), vendor / product.
- **Windows.Gaming.Input** [page]: `Gamepad.Vibration` drives four motors, the only documented way to the Xbox impulse
  triggers (XInput cannot); Windows 10+; on Win32 the list starts empty and pads arrive as "added" events
  (https://discourse.libsdl.org/t/sdl-get-the-current-wgi-controller-list-at-startup/33732). SDL wraps it. Whether it
  needs focus from an injected DLL: unverified (probe).
- **GameInput** [page]: Windows 10 19H1+ (https://learn.microsoft.com/en-us/gaming/gdk/docs/features/common/input/overviews/input-faq);
  newest features need `GameInputRedist.msi` - against "only Windows and the game", so optional, used only when present;
  foreground-only by default; v2.2 exposes audio-haptics endpoints
  (https://developer.microsoft.com/en-us/games/articles/2025/09/gameinput-pc-update-adds-haptics-motion-and-more/); v3.5
  reportedly adds a USB-only `DualSenseHelper.h`, header and static library MIT (secondary source).
- **Own HID code**: feasible (`hid.dll`, `WriteFile`) but duplicates SDL's drivers; only for the waveform path.
- **Steam Input** hides the real device (and has its own `SetDualSenseTriggerEffect`, irrelevant in an injected DLL):
  detect it and tell the player; double input from both paths is a risk.

### 2. DualSense specifics (Linux hid-playstation: https://github.com/torvalds/linux/blob/master/drivers/hid/hid-playstation.c; SDL)
- USB output report 0x02, 63 bytes (the 47-byte common block + padding); Bluetooth 0x31, 78 bytes (sequence / tag byte
  0x10 with the sequence in the high nibble, the common block, a little-endian CRC32 seeded 0xA2). DualShock 4: USB 0x05
  (32 bytes), Bluetooth 0x11 (78).
- Flags: `valid_flag0` bit0 compatible vibration, bit1 haptics select; `valid_flag2` bit2 improved rumble emulation
  (firmware 2.24+); 0x04 / 0x08 enable the right / left trigger effects [assumed: medium confidence].
- Trigger effect modes (Nielk1's MIT factory: https://gist.github.com/Nielk1/6d54cc2c00d2201ccb8c2720ad7538db), 11-byte
  blocks: Off 0x05, Feedback 0x21, Weapon 0x25, Vibration 0x26 (official); Bow 0x22, Galloping 0x23, Machine 0x27
  (unofficial); 10 zones, strength 0-8, a frequency byte; never the calibration codes 0xFC-0xFE.
- Waveform over USB: the 4-channel audio device, rear pair = voice coils (community:
  https://gitlab.freedesktop.org/pipewire/pipewire/-/issues/2486; the 48 kHz rate unverified); Sony's PC ports require
  wired USB for full haptics (https://insomniac.games/game/ratchet-clank-rift-apart-pc).
- Waveform over Bluetooth: SAxense (MPL-2.0, July 2025 proof of concept: report 0x32, 141 bytes, 3 kHz 8-bit stereo,
  64 samples per report, CRC seed 0xA2: https://github.com/egormanga/SAxense); the commercial DSX 3.2 beta added it in
  2026 (https://tweaktown.com/news/112219/). Experimental.
- Safety for a free mod: the community-documented protocols are common practice (SDL, Linux, Steam); stay away from
  Sony's NDA SDK; do not copy GPL kernel code into the DLL; MIT / zlib sources with attribution. Not legal advice.

### 3. Design references
- Apple Core Haptics / AHAP: Transient and Continuous events with Intensity and Sharpness (0-1), an
  attack / decay / release envelope, parameter curves - the best "author once" model.
- Housemarque, Returnal (https://blog.playstation.com/2021/05/13/how-housemarque-created-returnals-immersive-dualsense-controller-effects/):
  haptics designed like audio; a moderate baseline, the maximum kept for the strongest weapons and enemies; a signature
  per signal; left / right weighting by the physical source; a two-stage trigger (half = aim, full = alt fire); always
  with audio and visuals.
- Forza's impulse triggers: limit signals (grip lost, wheel lock) at the fingertips (https://news.xbox.com/en-us/?p=11803).
- Everspace 2: trigger resistance for low weapon energy and low boost; cues for consumables and incoming missiles
  (https://blog.playstation.com/?p=383180).
- XAG 110: an on / off switch, intensity from off to max, never the only cue
  (https://devdocs.xbox.com/build/game-principles/accessibility/xag-deep-dives/xag-110-haptic-feedback).
- Not found / unverified: published haptics of Squadrons, Elite Dangerous, MSFS; Team Asobi's talk; Nintendo HD Rumble's
  encoding (low / high band frequency + amplitude per side, after dekuNukem's notes).
- Principles: the low-frequency motor carries mass, the high-frequency one texture; transients attack < 10 ms then decay;
  continuous effects low and ducked under events; priority mixing with a cap; range scaling; per-category + master
  sliders and an off switch; limit constant vibration (numbness); triggers express state (energy, limits), not
  decoration; big motors spin up in tens of ms - pre-roll big events.

### 4. A first effect table (to be authored and felt; priorities 1 low - 7 high)
| Event | Motors (low / high) | Triggers / DualSense | Waveform | Prio |
|---|---|---|---|---|
| Gun fire by class | short high transient; low for heavy guns | impulse kick on the fire trigger; Weapon 0x25 / Machine 0x27 | shot texture per class | 3 |
| Shield hit | high, sharp, short | - | crackle | 4 |
| Hull hit | low, heavy, panned by side | brief trigger jolt | thud | 5 |
| Explosion | low, inverse-square by distance (capped), 100-300 ms decay | - | rumble tail | 4-6 |
| Engine / throttle | very low continuous, ducked | Feedback resistance rising with throttle | hum | 1 |
| Afterburner | low continuous ramp | Vibration 0x26 on its trigger; resistance at low fuel | roar | 2 |
| Cruise charge | rising ramp, release pulse | rising trigger vibration frequency | spool-up | 3 |
| Trade lane in / out | swell, then a jolt | - | whoosh | 3 |
| Docking clamps | two sharp low clunks | - | clank | 3 |
| Enemy missile lock | high pulses, rate rising with the lock | - | beeps | 6 |
| Missile incoming | urgent alternating left / right pulses | - | - | 7 |
| Low hull | heartbeat every 1.5 s, rate-limited | - | - | 5 |
| Energy depleted | - | the trigger goes slack or stiff (Everspace) | - | 4 |
| Autopilot hand-back | the existing pulse | a short tick on both triggers | chime | 6 |

### Open questions (probes)
1. A joystick-only static SDL3's added size. 2. SDL in Freelancer's process: messages, focus, DirectInput. 3. WGI
trigger rumble from an injected DLL per connection type. 4. SDL3 on Windows 7 today (the release check `win7_check`
would catch new imports). 5. The DualSense USB audio endpoint: identification, channel order, latency. 6. How the owner
connects the Elite Series 2 (USB, dongle, Bluetooth).
